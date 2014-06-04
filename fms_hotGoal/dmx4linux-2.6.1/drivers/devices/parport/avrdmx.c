/*
 * AVR DMX-512 Dongle
 * http://www.ele.tut.fi/~viikari/
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
#error Linux Kernel needs Parport support for the avrdmx interface
#endif

#include <linux/delay.h>
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

MODULE_AUTHOR("(c) 2002 Dirk Jagdmann <doj@cubic.org> http://llg.cubic.org");
MODULE_DESCRIPTION("avrdmx driver version " DMXVERSION);
MODULE_LICENSE("GPL");

static struct pardevice             *pdev   = NULL;
static struct proc_dir_entry *pentry = NULL;

static int EPP=0;
static int CanRead=0;
static int Version=3;

static unsigned char ReceiveEnable=1;
static unsigned char MergeEnable=1;

static unsigned char OutputBuffer[256];
static int OutputIndex=0;
static unsigned char InputBuffer[240];
static int InputIndex=0;

static int writebyte(unsigned char b, int mode_select)
{
  struct parport *port=pdev->port;

  if(EPP && Version>3)
    {
      if(mode_select)
	return port->ops->epp_write_addr(port, &b, 1, 0);
      else
	return port->ops->epp_write_data(port, &b, 1, 0);
    }
  else
    {
      unsigned char a=(mode_select?PARPORT_CONTROL_INIT:0) | PARPORT_CONTROL_STROBE;
      int i=100;		/* timeout value */

      parport_write_control(port, a);

      /* data */
      parport_write_data(port, b);

      /* strobe */
      parport_write_control(port, a|(mode_select?PARPORT_CONTROL_SELECT:PARPORT_CONTROL_AUTOFD));

      /* wait for WAIT = 1 */
      do {
	udelay(100);
	a=parport_read_status(port);
      } while ((a&PARPORT_STATUS_BUSY) && --i);

      /* reset strobe */
      parport_write_control(port, 0);

      return i?1:-1;
    }
}

static int readbyte(int mode_select)
{
  unsigned char b;
  struct parport *port=pdev->port;

  if(!CanRead)
    return -2;

  if(EPP && Version>3)
    {
      if(mode_select)
	{
	  if(port->ops->epp_read_addr(port, &b, 1, 0) < 0)
	    return -3;
	}
      else
	if(port->ops->epp_read_data(port, &b, 1, 0) < 0)
	  return -3;
    }
  else
    {
      int i=100;		/* timeout value */
      unsigned char a=(mode_select?PARPORT_CONTROL_INIT:0);

      parport_write_data(port, 0);

      /* strobe */
      parport_write_control(port, a);
      udelay(1);
      parport_write_control(port, a|(mode_select?PARPORT_CONTROL_SELECT:PARPORT_CONTROL_AUTOFD));

      udelay(10);

      /* read data */
      parport_data_reverse(port);
      b=parport_read_data(port);
      parport_data_forward(port);

      /* deassert strobe */
      parport_write_control(port, a);

      /* wait for WAIT = 1 */
      do {
	udelay(100);
	a=parport_read_status(port);
      } while ((a&PARPORT_STATUS_BUSY) && --i);

      /* timeout? */
      if(i==0)
	{
	  printk(KERN_ERR "avrdmx: timeout in readbyte()\n");
	  b=-1;
	}

      /* reset auxstrobe */
      parport_write_control(port, 0);
    }

  return b;
}


static int set_status(void)
{
  return writebyte((MergeEnable?2:0) | (ReceiveEnable?1:0), 1);
}


static int write_universe(DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  if(!u || !buff)
    return -EINVAL;

  if(offs>sizeof(OutputBuffer))
    return -EINVAL;

  if(offs+size>sizeof(OutputBuffer))
    size=sizeof(OutputBuffer)-offs;

  /* copy new values to our buffer */
  memcpy(OutputBuffer+offs, buff, size);

  /* check if we need to reset OutputIndex */
  if(offs<OutputIndex)
    {
      if(Version>3)
	{
	  /* todo */
	}
      else
	{
	  /* reset Counters */
	  if(writebyte(128 | (MergeEnable?2:0) | (ReceiveEnable?1:0), 1) < 1)
	    {
	      printk(KERN_INFO "avrdmx: could not reset counters\n");
	      return -1;
	    }
	  OutputIndex=InputIndex=0;
	}
    }

  if(EPP)
    {
      int i=pdev->port->ops->epp_write_data(pdev->port, &OutputBuffer[OutputIndex], size, 0);
      OutputIndex+=i;
      return i;
    }
  else
    for(; OutputIndex<offs+size; OutputIndex++)
      if(writebyte(OutputBuffer[OutputIndex], 0) < 1)
	{
	  printk(KERN_INFO "avrdmx: could not write slot %i\n", OutputIndex);
	  return -1;
	}

  return size;
}

static int read_universe(DMXUniverse *u, off_t start, DMXSlotType *buff, size_t size)
{
  if(start>sizeof(InputBuffer))
    return -EINVAL;

  if(start+size>sizeof(InputBuffer))
    size=sizeof(InputBuffer)-start;

  if(start>InputIndex)
    {
      /* reset Counters */
      if(writebyte(128 | (MergeEnable?2:0) | (ReceiveEnable?1:0), 1) < 1)
	{
	  printk(KERN_INFO "avrdmx: could not reset counters\n");
 	  return -1;
	}
      OutputIndex=InputIndex=0;
    }

  if(EPP)
    {
      int i=pdev->port->ops->epp_read_data(pdev->port, &InputBuffer[InputIndex], size, 0);
      InputIndex+=i;
    }
  else
    {
      int i;
      for(; InputIndex<start+size; InputIndex++)
	if((i=readbyte(0)) < 0)
	  {
	    printk(KERN_INFO "avrdmx: could not read slot %i\n", InputIndex);
	    return -1;
	  }
	else
	  InputBuffer[InputIndex]=i;
    }

  memcpy(buff, &InputBuffer[start], size);

  printk(KERN_INFO "read\n");
  return size;
}

static int data_available (DMXUniverse *u, uint start, uint size)
{
  return 1;
}


static int setPropertyInt (DMXUniverse *u, char *name,  int val)
{
  DMXProperty *p = u ? u->findprop (u, name) : NULL;
  return p?p->set_long (p, val):-1;
}


static int get_merge_status (DMXProperty *p, long *value)
{
  if (value)
    {
      *value = MergeEnable;
      return 0;
    }
  return -1;
}

static int set_merge_status (DMXProperty *p, long value)
{
  MergeEnable = value;
  set_status ();
  return 0;
}

static int get_receive_status (DMXProperty *p, long *value)
{
  if (value)
    {
      *value = ReceiveEnable;
      return 0;
    }
  return -1;
}

static int set_receive_status (DMXProperty *p, long value)
{
  ReceiveEnable = value;
  set_status ();
  return 0;
}


static int create_universe (DMXUniverse *u, DMXPropList *pl)
{
  DMXProperty *p = NULL;

  if (!u) return -1;

  u->read_slots = read_universe;
  u->write_slots = write_universe;
  u->data_available = data_available;

  setPropertyInt(u, "slots", sizeof(OutputBuffer));

  p=dmxprop_create_long ("merge", 1);
  if (p)
    {
      dmxprop_user_long (p, get_merge_status, set_merge_status, NULL);
      u->props->add (u->props, p);
    }

  p=dmxprop_create_long ("receive", 1);
  if (p)
    {
      dmxprop_user_long (p, get_receive_status, set_receive_status, NULL);
      u->props->add (u->props, p);
    }

  u->user_data = NULL;
  strcpy (u->connector, "one");
  u->conn_id = 0;

  memset(OutputBuffer, 0, sizeof(OutputBuffer));
  OutputIndex=1000; /* make invalid, so we have to reset upon first use of the interface */
  /* reset interface */
  write_universe(u, 0, OutputBuffer, sizeof(OutputBuffer));

  return 0;
}

static int proc_read (char *buf, char **start, off_t offset, int length, int *eof, void *priv)
{
  char *p=buf;
  p+=sprintf(p, "AVR DMX-512 Dongle Version %i\n", Version);
  p+=sprintf(p, "DMX Receiver: %s\n", ReceiveEnable?"enabled":"disabled");
  p+=sprintf(p, "DMX Merge: %s\n", MergeEnable?"enabled":"disabled");
  p+=sprintf(p, "AVR Status: %02X\n", readbyte(1));
  p+=sprintf(p, "\nwrite the string \"PROGRAM\" into this procfile to program the avr.\n");
  return p-buf;
}

static int proc_write (struct file *file, const char *buf, unsigned long count, void *priv)
{
  if (!buf || count<0) return -EINVAL;

  if(!strnicmp(buf, "PROGRAM", (count<7)?count:7))
    {
      printk(KERN_INFO "avrdmx: programming avr\n");
      writebyte(64 | (MergeEnable?2:0) | (ReceiveEnable?1:0), 1);
    }

  return count;
}


/*
 * --------- Module creation / deletion ---------------
 */

static int parport=0;
module_param(parport, int, S_IRUGO);
MODULE_PARM_DESC(parport,"parport number (0=parport0)");

static DMXFamily *avrdmx_fam = NULL;

static int __init avrdmx_init(void)
{
  int i;

  struct parport *pport = parport_find_number(parport);
  if (pport)
    {
      struct pardevice *newpdev;

      printk (KERN_INFO "avrdmx: found parport %d\n", parport);

      if(pport->modes&PARPORT_MODE_TRISTATE)
	{
	  printk(KERN_INFO "avrdmx: parport supports tristate\n");
	  CanRead=1;
	}
      if(pport->modes&PARPORT_MODE_EPP)
	{
	  EPP=1;
	  printk(KERN_INFO "avrdmx: parport supports EPP\n");
	  CanRead=1;
	}

      if(pport->ieee1284.mode & IEEE1284_MODE_EPP)
	printk(KERN_INFO "avrdmx: parport has ieee1284 epp 1.9\n");
      if(pport->ieee1284.mode &  IEEE1284_MODE_EPPSL)
	printk(KERN_INFO "avrdmx: parport has ieee1284 epp 1.7\n");
      if(pport->ieee1284.mode &  IEEE1284_MODE_EPPSWE)
	printk(KERN_INFO "avrdmx: parport has ieee1284 epp software emulation\n");

      newpdev = parport_register_device (pport, "avrdmx",
					 NULL,
					 NULL,
					 NULL,
					 PARPORT_DEV_EXCL,
					 (void *)NULL);
      if (!newpdev)
	{
	  printk (KERN_ERR "avrdmx: failed to get access to parport\n");
	  return -1;
	}

      printk (KERN_INFO "avrdmx: got parport%d\n", parport);

      pdev = newpdev;

      if (parport_claim (newpdev)==0)
	{
	  printk("avrdmx: successfully claimed parport\n");

	  pentry = create_proc_entry("avrdmx", S_IFREG | S_IRUGO, NULL);
	  if (pentry)
	    {
	      pentry->read_proc  = proc_read;
	      pentry->write_proc = proc_write;
	      pentry->data = 0;
	      printk(KERN_INFO "avrdmx proc entry created\n");
	    }

          avrdmx_fam = dmx_create_family ("PAR");
          if (!avrdmx_fam)
            {
              printk (KERN_ERR "avrdmx: unable to register PAR family\n");
              return -EBUSY;
            }
          else
            {
              DMXInterface *dmxif = NULL;
              DMXDriver *avrdmx_drv = avrdmx_fam->create_driver (avrdmx_fam, "avrdmx", create_universe, NULL);

              dmxif = avrdmx_drv->create_interface (avrdmx_drv, NULL);

              if (dmxif)
                {
                  if (dmxif->create_universe (dmxif, 0, NULL))
                    {
		      i=readbyte(1);
		      if(i<0)
			printk(KERN_INFO "avrdmx: could not read status\n");
		      else
			{
			  printk(KERN_INFO "avrdmx: read status %02X, ~%02X\n", i, (~i)&0xff);
			  if(i>=0)
			    {
			      if(i==0xff)
				{
				  Version=3;
				  ReceiveEnable=1;
				  MergeEnable=1;
				}
			      else if(i==0x03)
				{
				  Version=4;
				  ReceiveEnable=i&1;
				  MergeEnable=i&2;
				}
			    }
			}

		      printk(KERN_INFO "avrdmx: using dongle version %i\n", Version);
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


static void __exit avrdmx_exit(void)
{
  if (pdev)
    {
      parport_release (pdev);
      parport_unregister_device(pdev);
    }
  remove_proc_entry("avrdmx", NULL);

  if (avrdmx_fam)
    avrdmx_fam->delete (avrdmx_fam, 0);

}

module_init(avrdmx_init);
module_exit(avrdmx_exit);
