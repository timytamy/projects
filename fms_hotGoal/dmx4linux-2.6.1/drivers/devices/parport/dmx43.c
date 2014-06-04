/*
 * dmx43.c
 * A driver for an ECP based interface.
 *
 * Copyright (C) Dirk Jagdmann <doj@cubic.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
#include <linux/config.h>
#endif

#if (!defined(CONFIG_PARPORT)) && (!defined(CONFIG_PARPORT_MODULE))
#error Linux Kernel needs Parport support for the dmx43 interface
#endif

#include <linux/init.h>
#include <linux/module.h>
#include <linux/parport.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/smp_lock.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/string.h>

#include <dmx/dmxdev.h>
#include <dmx/dmxmem.h>

MODULE_AUTHOR("(c) 2001 Michael Stickel <michael@cubic.org> http://llg.cubic.org");
MODULE_DESCRIPTION("dmx43 driver version " DMXVERSION);
MODULE_LICENSE("GPL");

#define ECPDEBUG 0

static struct pardevice      *pdev   = NULL;
static struct proc_dir_entry *pentry = NULL;

static unsigned char universe0sc=0;
static unsigned char universe1sc=0;
static unsigned char universe0break=90;
static unsigned char universe1break=90;
static unsigned char universe0mark=10;
static unsigned char universe1mark=10;
static unsigned short universe0slots=512;
static unsigned short universe1slots=512;
static unsigned short dmxconfig=0;

static int dmx43_sendcommand(int num, unsigned short arg, void *priv)
{
  unsigned char command[2];

  if(num<1 || num>127)
    return -EINVAL;

#if ECPDEBUG
  printk(KERN_INFO "dmx43: command: %02X arg: %04X\n", num+0x80, arg);
#endif

  command[0]=0x80+num;
  if(pdev->port->ops->ecp_write_addr (pdev->port, command, 1, 0) < 1)
    {
      printk(KERN_ERR "dmx43: sendcommand(): could not write addr\n");
      return -1;
    }
  command[0]=(arg>>8)&0xff;
  command[1]=arg&0xff;
  if(pdev->port->ops->ecp_write_data (pdev->port, command, 2, 0) < 2)
    {
      printk(KERN_ERR "dmx43: sendcommand(): could not write arg\n");
      return -1;
    }

  return 0;
}

static int dmx43_universe0slots(unsigned short slots, void *priv)
{
  if(slots<24 || slots>512)
    return -1;
  if(dmx43_sendcommand(1, slots, priv) < 0)
    return -1;
  universe0slots=slots;
  return 0;
}

static int dmx43_universe1slots(unsigned short slots, void *priv)
{
  if(slots<24 || slots>512)
    return -1;
  if(dmx43_sendcommand(2, slots, priv) < 0)
    return -1;
  universe1slots=slots;
  return 0;
}

static int dmx43_universe0sc(unsigned char sc, void *priv)
{
  unsigned short arg=(universe1sc<<8)|sc;
  if(dmx43_sendcommand(3, arg, priv) < 0)
    return -1;
  universe0sc=sc;
  return 0;
}

static int dmx43_universe1sc(unsigned char sc, void *priv)
{
  unsigned short arg=(sc<<8)|universe0sc;
  if(dmx43_sendcommand(3, arg, priv) < 0)
    return -1;
  universe1sc=sc;
  return 0;
}

static int dmx43_universe0break(unsigned char br, void *priv)
{
  unsigned short arg=(universe1break<<8)|br;
  if(dmx43_sendcommand(4, arg, priv) < 0)
    return -1;
  universe0break=br;
  return 0;
}

static int dmx43_universe1break(unsigned char br, void *priv)
{
  unsigned short arg=(br<<8)|universe0break;
  if(dmx43_sendcommand(4, arg, priv) < 0)
    return -1;
  universe1break=br;
  return 0;
}

static int dmx43_universe0mark(unsigned char mark, void *priv)
{
  unsigned short arg=(universe1mark<<8)|mark;
  if(dmx43_sendcommand(5, arg, priv) < 0)
    return -1;
  universe0mark=mark;
  return 0;
}

static int dmx43_universe1mark(unsigned char mark, void *priv)
{
  unsigned short arg=(mark<<8)|universe0mark;
  if(dmx43_sendcommand(5, arg, priv) < 0)
    return -1;
  universe1mark=mark;
  return 0;
}

static int dmx43_dmxconfig(unsigned short cnf, void *priv)
{
  if(dmx43_sendcommand(6, cnf, priv) < 0)
    return -1;
  dmxconfig=cnf;
  return 0;
}

#if 0
static char* dmx43_getversion(void *priv)
{
  char *s=0;
  char c=1;
  int len=1;
  int slen;
  int i=0;

#if ECPDEBUG
  printk(KERN_INFO "dmx43_getversion() start\n");
#endif
  if(dmx43_sendcommand(7, 1, priv) < 0)
    return s;

  if(pdev->port->ops->ecp_read_data (pdev->port, &c, len, 0) < len)
    return 0;
#if ECPDEBUG
  printk(KERN_INFO "dmx43: len1 %c %02X\n", c, c);
#endif
  slen=c<<8;
  if(pdev->port->ops->ecp_read_data (pdev->port, &c, len, 0) < len)
    return 0;
#if ECPDEBUG
  printk(KERN_INFO "dmx43: len2 %c %02X\n", c, c);
#endif
  slen+=c;

  s=MALLOC(slen+1);
  if(s==0)
    return s;

  c=1;
  while(c!=0 && i<slen)
    {
      if(pdev->port->ops->ecp_read_data (pdev->port, &c, len, 0) < len)
	return 0;
#if ECPDEBUG
      printk(KERN_INFO "dmx43: read %c %02X\n", c, c);
#endif
      s[i++]=c;
    }
  s[i]=0;

#if ECPDEBUG
  printk(KERN_INFO "dmx43_getversion() stop\n");
#endif
  return s;
}
#endif

static int dmx43_proc_read (char *buf, char **start, off_t offset, int length, int *eof, void *priv)
{
  char *p=buf;
#if 0
  char *s=0;
#endif
  p+=sprintf(p, "DMX43 Interface Configuration\n");
  p+=sprintf(p, "Universe0 - Slots: %i Startcode: 0x%02X Break: %ius Mark: %ius\n",
	     universe0slots, universe0sc, universe0break, universe0mark);
  p+=sprintf(p, "Universe1 - Slots: %i Startcode: 0x%02X Break: %ius Mark: %ius\n",
	     universe1slots, universe1sc, universe1break, universe1mark);

#if 0
  s=dmx43_getversion(priv);
  if(s)
    {
      p+=sprintf(p, "Version: %s\n", s);
      FREE(s);
    }
#endif

  return p-buf;
}

static int dmx43_proc_write (struct file *file, const char *buf, unsigned long count, void *priv)
{
  char command[128];
  int arg, commandlen;
  int i;

  if (!buf || count<0) return -EINVAL;


  for (i=0; buf[i] && buf[i]!=' '; i++)
    command[i] = buf[i];
  command[i] = 0;

  if (!command[0])
    return -EINVAL;

  arg = simple_strtoul (&buf[i], NULL, 10);

  commandlen=strlen(command);
  if(commandlen<1)
    return -EINVAL;

  if(!strnicmp(command, "universe0slots", commandlen)
     || !strnicmp(command, "u0slots", commandlen)
     || !strnicmp(command, "slots0", commandlen)
     || !strnicmp(command, "universe0channels", commandlen)
     || !strnicmp(command, "u0channels", commandlen)
     || !strnicmp(command, "channels0", commandlen)
     || !strnicmp(command, "universe0size", commandlen)
     || !strnicmp(command, "u0size", commandlen)
     || !strnicmp(command, "size0", commandlen)
     || !strnicmp(command, "universe0length", commandlen)
     || !strnicmp(command, "u0length", commandlen)
     || !strnicmp(command, "length0", commandlen)
     )
    {
      dmx43_universe0slots(arg, priv);
      goto raus;
    }

  if(!strnicmp(command, "universe1slots", commandlen)
     || !strnicmp(command, "u1slots", commandlen)
     || !strnicmp(command, "slots1", commandlen)
     || !strnicmp(command, "universe1channels", commandlen)
     || !strnicmp(command, "u1channels", commandlen)
     || !strnicmp(command, "channels1", commandlen)
     || !strnicmp(command, "universe1size", commandlen)
     || !strnicmp(command, "u1size", commandlen)
     || !strnicmp(command, "size1", commandlen)
     || !strnicmp(command, "universe1length", commandlen)
     || !strnicmp(command, "u1length", commandlen)
     || !strnicmp(command, "length1", commandlen)
     )
    {
      dmx43_universe1slots(arg, priv);
      goto raus;
    }

  if(   !strnicmp(command, "universe0break", commandlen)
     || !strnicmp(command, "u0break", commandlen)
     || !strnicmp(command, "break0", commandlen))
    {
      dmx43_universe0break(arg, priv);
      goto raus;
    }

  if(   !strnicmp(command, "universe1break", commandlen)
     || !strnicmp(command, "u1break", commandlen)
     || !strnicmp(command, "break1", commandlen))
    {
      dmx43_universe1break(arg, priv);
      goto raus;
    }

  if(   !strnicmp(command, "universe0sc", commandlen)
     || !strnicmp(command, "u0sc", commandlen)
     || !strnicmp(command, "sc0", commandlen))
    {
      dmx43_universe0sc(arg, priv);
      goto raus;
    }

  if(   !strnicmp(command, "universe1sc", commandlen)
     || !strnicmp(command, "u1sc", commandlen)
     || !strnicmp(command, "sc1", commandlen))
    {
      dmx43_universe1sc(arg, priv);
      goto raus;
    }

  if(   !strnicmp(command, "universe0mark", commandlen)
     || !strnicmp(command, "u0mark", commandlen)
     || !strnicmp(command, "mark0", commandlen))
    {
      dmx43_universe0mark(arg, priv);
      goto raus;
    }

  if(   !strnicmp(command, "universe1mark", commandlen)
     || !strnicmp(command, "u1mark", commandlen)
     || !strnicmp(command, "mark1", commandlen))
    {
      dmx43_universe1mark(arg, priv);
      goto raus;
    }

  if(   !strnicmp(command, "dmxconfig", commandlen)
     || !strnicmp(command, "config", commandlen))
    {
      dmx43_dmxconfig(arg, priv);
      goto raus;
    }

 raus:
  return count;
}


static int  dmx43_write_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  if (u && buff && size > 0 && offs+size <= 512)
    {
      unsigned char commands[2];

      size_t wlen = 0;

      /* write data */

      commands[0] = 0x80;
      wlen = pdev->port->ops->ecp_write_addr (pdev->port, commands, 1, 0);
      if(wlen < 1)
	{
	  printk(KERN_INFO "dmx43: write(): could not write addr\n");
	  return -1;
	}

      commands[0]=(offs>>8)&0xff;
      commands[1]=offs&0xff;
      wlen = pdev->port->ops->ecp_write_data (pdev->port, commands, 2, 0);
      if(wlen < 2)
	{
	  printk(KERN_INFO "dmx43: write(): could not write command\n");
	  return -1;
	}

      return pdev->port->ops->ecp_write_data (pdev->port, buff, size, 0);
    }

  printk (KERN_ERR "dmx43_write_universe: illegal parameters\n");
  return -EINVAL;
}

static int dmx43_create_universe (DMXUniverse *u, DMXPropList *pl)
{
  if (!u) return -1;

  u->write_slots = dmx43_write_universe;

  u->user_data = NULL;
  strcpy (u->connector, "one");
  u->conn_id = 0;

  return 0;
}


/*
 * --------- Module creation / deletion ---------------
 */

static int parport=0;
module_param(parport, int, S_IRUGO);
MODULE_PARM_DESC(parport,"parport number (0=parport0)");

static DMXFamily *dmx43_fam = NULL;

static int __init dmx43_init(void)
{
  struct parport *pport = parport_find_number(parport);
  if (pport)
    {
      struct pardevice *newpdev;

      newpdev = parport_register_device (pport, "dmx43",
					 NULL,
					 NULL,
					 NULL,
					 PARPORT_DEV_EXCL,
					 (void *)NULL);
      if (!newpdev)
	{
	  printk (KERN_ERR "dmx43: failed to get access to parport\n");
	  return -1;
	}

      printk (KERN_INFO "dmx43: got parport%d\n", parport);

      pdev = newpdev;

      if (parport_claim (newpdev)==0)
	{
	  printk(KERN_INFO "dmx43: successfully  claimed parport\n");
	  pentry = create_proc_entry("dmx43", S_IFREG | S_IRUGO, NULL);
	  if (pentry)
	    {
	      pentry->read_proc  = dmx43_proc_read;
	      pentry->write_proc = dmx43_proc_write;
	      pentry->data = 0;
	      printk(KERN_INFO "dmx43: proc entry created\n");
	    }

          dmx43_fam = dmx_create_family ("PAR");
          if (!dmx43_fam)
            {
              printk (KERN_ERR "dmx43: unable to register PAR family\n");
              return -EBUSY;
            }
          else
            {
              DMXInterface *dmxif = NULL;
              DMXDriver *dmx43_drv = dmx43_fam->create_driver (dmx43_fam, "dmx43", dmx43_create_universe, NULL);

              dmxif = dmx43_drv->create_interface (dmx43_drv, NULL);

              if (dmxif)
                {
                  if (dmxif->create_universe (dmxif, 0, NULL))
                    {
                      printk(KERN_INFO "dmx43: driver successfully started\n");
                      return 0;
                    }
                }
            }
          parport_release(pdev);
	}

      parport_unregister_device(newpdev);
    }
  return -1;
}


static void __exit dmx43_exit(void)
{
  if (pdev)
    {
      parport_release (pdev);
      parport_unregister_device(pdev);
    }
  remove_proc_entry("dmx43", NULL);

  if (dmx43_fam)
    dmx43_fam->delete (dmx43_fam, 0);
}

module_init(dmx43_init);
module_exit(dmx43_exit);
