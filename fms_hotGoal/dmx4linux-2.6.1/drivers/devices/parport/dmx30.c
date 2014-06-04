/*
 * dmx30.c
 * Device-driver for my own DMX-Interface (release 3.0).
 *
 * Copyright (C) Michael Stickel <michael@cubic.org>
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
#error Linux Kernel needs Parport support for the dmx30 interface
#endif

#include <linux/module.h>
#include <linux/init.h>
#include <linux/parport.h>
#include <linux/delay.h>

#include <dmx/dmxdev.h>

MODULE_AUTHOR("(c) 2001 Michael Stickel <michael@cubic.org> http://llg.cubic.org");
MODULE_DESCRIPTION("Driver for the dmx30 interface (http://llg.cubic.org/dmx30) version " DMXVERSION);
MODULE_LICENSE("GPL");

static struct pardevice  *pdev   = NULL;


static wait_queue_head_t  dmxthread_wait;      /* wake(...) will wake up the worker thread for that interface */
static int                dmxthread_pid = -1;       /* pid of the worker thread */
static int                dmxthread_running = 0;   /* is the worker thread running ? */

static unsigned char      buffer[120];
static int                limit_channel = -1;



static int  dmx30_write_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  if(offs+size>sizeof(buffer))
    size = sizeof(buffer)-offs;

  if(size>0)
    {
      memcpy(buffer+offs, buff, size);
      wake_up_interruptible (&dmxthread_wait);

      if (size > limit_channel)
	limit_channel = size;

      return size;
    }

  printk (KERN_ERR "dmx30_write_universe: illegal parameters\n");
  return -EINVAL;
}

static int setPropertyInt (DMXUniverse *u, char *name,  int val)
{
  DMXProperty *p = u ? u->findprop (u, name) : NULL;
  return p?p->set_long (p, val):-1;
}




static int dmx30_create_universe (DMXUniverse *u, DMXPropList *pl)
{
  if (!u) return -1;

  u->write_slots = dmx30_write_universe;

  u->user_data = NULL;
  strcpy (u->connector, "one");
  u->conn_id = 0;

  setPropertyInt(u, "slots", sizeof(buffer));

  memset(buffer, 0, sizeof(buffer));

  dmx30_write_universe(u, 0, buffer, sizeof(buffer));

  return 0;
}





static int dmx30_worker_thread (void *userdata)
{
  long work_counter = 0L;


  printk (KERN_INFO "call: dmx30 thread\n");

  dmxthread_running = 1;
  printk ("dmx30: dmxthread running\n");

  /* Setup a nice name */
  strcpy(current->comm, "dmx30");

#if 0
  lock_kernel();
#endif

#if 0
  daemonize();
#endif


  do
    {
      interruptible_sleep_on_timeout(&dmxthread_wait, 250);
      {
	struct parport *port = pdev ? pdev->port : NULL;
	int i;

	if (port)
	  {
	    int size = sizeof(buffer);
	    if (limit_channel > 0)
	      size = limit_channel;
	    limit_channel = -1;

	    /* kann man hier den startslot setzen ?? dazu nochmal den neuen sourcecode testen. */
	    port->ops->write_data(port, 0);
	    port->ops->write_control (port, PARPORT_CONTROL_AUTOFD);
	    udelay(8);
	    port->ops->write_control (port, 0);
	    udelay(10);

	    for (i=0; i<size; i++)
	      {
		port->ops->write_data (port, buffer[i]);
		port->ops->write_control (port, PARPORT_CONTROL_STROBE);
		udelay(5);
		port->ops->write_control (port, 0);
		udelay(8);
	      }

	    work_counter++;
	  }
      }
    } while (!signal_pending(current));

  printk (KERN_INFO "dmx30-thread exiting\n");
  dmxthread_running = 0;
  dmxthread_pid = -1;

  return 0;
}




/*
 * --------- Module creation / deletion ---------------
 */

static int parport=0;
module_param(parport, int, S_IRUGO);
MODULE_PARM_DESC(parport,"parport number (0=parport0)");

static DMXFamily *dmx30_fam = NULL;


static int __init dmx30_init(void)
{
  struct parport *pport = parport_find_number(parport);

  if (pport)
    {
      struct pardevice *newpdev;

      init_waitqueue_head (&dmxthread_wait);

      newpdev = parport_register_device (pport, "dmx30",
					 NULL,
					 NULL,
					 NULL,
					 PARPORT_DEV_EXCL,
					 (void *)NULL);
      if (!newpdev)
	{
	  printk (KERN_ERR "dmx30: failed to get access to parport\n");
	  return -1;
	}

      printk (KERN_INFO "dmx30: got parport%d\n", parport);

      pdev = newpdev;

      if (parport_claim (newpdev)==0)
	{
	  printk(KERN_INFO "dmx30: successfully  claimed parport\n");
#if 0
	  pentry = create_proc_entry("dmx30", S_IFREG | S_IRUGO, NULL);
	  if (pentry)
	    {
	      pentry->read_proc  = dmx30_proc_read;
	      pentry->write_proc = dmx30_proc_write;
	      pentry->data = 0;
	      printk(KERN_INFO "dmx30: proc entry created\n");
	    }
#endif
          dmx30_fam = dmx_create_family ("PAR");
          if (!dmx30_fam)
            {
              printk (KERN_ERR "dmx30: unable to register PAR family\n");
              return -EBUSY;
            }
          else
            {
              DMXInterface *dmxif = NULL;
              DMXDriver *dmx30_drv = dmx30_fam->create_driver (dmx30_fam, "dmx30", dmx30_create_universe, NULL);

              dmxif = dmx30_drv->create_interface (dmx30_drv, NULL);

              if (dmxif)
                {
                  if (dmxif->create_universe (dmxif, 0, NULL))
                    {
		      int pid = kernel_thread(dmx30_worker_thread, (void *)pdev, CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
		      if (pid >= 0)
			{
			  dmxthread_pid = pid;
			  printk (KERN_INFO "pid for dmx30-thread is %d\n", pid);
			  printk(KERN_INFO "dmx30: driver successfully started\n");
			  return 0;
			}
                    }
                }
	      dmx30_fam->delete (dmx30_fam, 0);
            }
          parport_release(pdev);
	}

      parport_unregister_device(newpdev);
    }
  return -1;
}


static void __exit dmx30_exit(void)
{
  if (dmxthread_pid != -1)
    {
      int ret = kill_proc(dmxthread_pid, SIGTERM, 0);
      if (!ret)
	{
	  /* Wait 10 seconds */
	  int count = 10 * 100;

	  while (dmxthread_running && --count)
	    {
	      current->state = TASK_INTERRUPTIBLE;
	      schedule_timeout(1);
	    }
	  if (!count)
	    printk (KERN_INFO "giving up on killing dmx30-thread");
	}
    }


  if (pdev)
    {
      struct pardevice  *d = pdev;
      pdev = NULL;
      parport_release (d);
      parport_unregister_device(d);
    }

  if (dmx30_fam)
    dmx30_fam->delete (dmx30_fam, 0);
}

module_init(dmx30_init);
module_exit(dmx30_exit);
