/*
 * usb2dmx.c
 * Driver for the Lighting Solutions USB DMX-Interface
 * http://www.lighting-solutions.de
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

#define __KERNEL_SYSCALLS__

// #include <asm/errno.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/usb.h>
// #include <asm/unistd.h>

#include <dmx/dmxdev.h>
#include "usb2dmx.h"

MODULE_AUTHOR("(c) 2001 Michael Stickel <michael@cubic.org> http://llg.cubic.org");
MODULE_DESCRIPTION("Driver for the USB2DMX interface (http://www.lightingsolutions.de) version " DMXVERSION);
MODULE_LICENSE("GPL");

static int inputrate=30;
module_param(inputrate, int, S_IRUGO);
MODULE_PARM_DESC(inputrate,"input universe is read <inputrate> times per second (default 30)");

/* handle up_and_exit confusion */
#include <linux/completion.h>
typedef struct completion           THREAD_SEM;
#define THREAD_SEM_EXIT(c,l)        complete_and_exit(c,l)
#define THREAD_SEM_DECLARE(c)       DECLARE_COMPLETION(c)
#define THREAD_SEM_INIT(c)          init_completion(c)
#define THREAD_SEM_WAIT_COMPLETE(c) wait_for_completion(c)

#define DEBUGLEVEL(name,level) (0)

#define USB2DMX_TIMEOUT 200

#define FUNCFRAME_(name)   name,sizeof(name)
#define _FUNCFRAME_        "= [0x%p, %u]"

static DEFINE_SPINLOCK(usb2dmx_thread_lock);

struct usb2dmx_interface;
struct usb2dmx_universe
{
  struct usb2dmx_interface *dmx_if;
  unsigned char             universe_id; /* Universe id on that interface */
  DMXUniverse              *universe;
  volatile char             data_avail;  /* for userspace: 1 = data available on universe. 0 = no new values available on universe. */
  long                      framecount;  /* currently only on input universe */
  unsigned long             buffer_offset; /* offset for the tx/rx buffer for this universe into the interface buffer */
  unsigned char             buffer[512];
};

#define USB2DMX_UNIVERSE_IN   (1)
#define USB2DMX_UNIVERSE_OUT  (0)

struct usb2dmx_interface
{
  struct usb_device        *dmx_dev;      /* init: probe_dmx      */
  DMXInterface             *interface;
  struct usb2dmx_universe  *universes[4];

  int                rx_frames;
  int                do_read;
  wait_queue_head_t  read_waitqueue;   /* wake(...) will wake up the worker thread for that interface */
  wait_queue_head_t  waitqueue;   /* wake(...) will wake up the worker thread for that interface */
  int                thread_pid;  /* pid of the worker thread */

  THREAD_SEM         thr_exited;  /* semaphore that signals that the thread is exiting */

  int                running;     /* is the worker thread running ? */
  char               data_pending;
  volatile int       id_led;
};


DMXFamily *usb2dmx_family = NULL;

/*
 * dmx_write_universe
 *
 * Used to write a couple of slot-values to the universe.
 */
static int dmx_write_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
#ifdef DEBUG
  printk(KERN_INFO "dmx_write_universe called\n");
#endif
  if (u && size>0 && offs+size <= 512)
    {
      struct usb2dmx_universe *u2d_u = (struct usb2dmx_universe *)u->user_data;
      if (u2d_u && u2d_u->dmx_if && u2d_u->universe_id==USB2DMX_UNIVERSE_OUT)
	{
	  /* copy the data and tell the thread something has changed */

	  memcpy ((u2d_u->buffer)+offs, (void *)buff, size);

	  u2d_u->dmx_if->data_pending = 1;
	  wake_up (&u2d_u->dmx_if->waitqueue);

	  u2d_u->data_avail=1;
	  u->signal_changed (u, offs, size);

	  return 0;
	}
    }
  return -EINVAL;
}




/*
 * usb2dmx_data_available
 */
int  usb2dmx_data_available (DMXUniverse *u, uint start, uint size)
{
  if (u && u->user_data)
    {
      struct usb2dmx_universe *u2d_u = (struct usb2dmx_universe *)u->user_data;
      if (u2d_u)
	return u2d_u->data_avail;
    }
  return 0;
}


/*
 * dmx_read_universe
 *
 * Used to write a couple of slot-values to the universe.
 */
static int dmx_read_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
#if DEBUGLEVEL(USB2DMX,1)
  printk(KERN_INFO "dmx_read_universe called\n");
#endif
  if (u && size>0 && offs+size <= 512)
    {
      struct usb2dmx_universe *u2d_u = (struct usb2dmx_universe *)u->user_data;

#if DEBUGLEVEL(USB2DMX,2)
      printk(KERN_INFO "dmx_read_universei: call is valid\n");
#endif
      if (u2d_u && u2d_u->dmx_if)
	{
          unsigned char *inbuffer = u2d_u->buffer;

#if DEBUGLEVEL(USB2DMX,2)
          printk(KERN_INFO "dmx_read_universe: usb2dmx universe data valid\n");
#endif

          if (u2d_u->data_avail)
            {
	      u2d_u->data_avail=0;

	      if (offs+size>512)
	        size = 512-offs;

	      memcpy (buff, inbuffer+offs, size);

	      return size;
            }
#if DEBUGLEVEL(USB2DMX,2)
          else
            printk(KERN_INFO "dmx_read_universe: no change at universe\n");
#endif
          return 0;
	}
#if DEBUGLEVEL(USB2DMX,2)
      else
        printk(KERN_INFO "dmx_read_universei: usb2dmx universe data INVALID\n");
#endif
    }
#if DEBUGLEVEL(USB2DMX,2)
  else
    printk(KERN_INFO "dmx_read_universei: call is INVALID: offs=%lu, size=%d\n", offs, size);
#endif
  return -EINVAL;
}


/*
 * usb2dmx_delete_universe
 *
 */
static int usb2dmx_delete_universe (DMXUniverse *u)
{
  printk ("usb2dmx_delete_universe (DMXUniverse *u=0x%p) " _FUNCFRAME_ "\n", u, FUNCFRAME_(usb2dmx_delete_universe));
  if (u && u->user_data)
    {
      struct usb2dmx_universe *u2d_u = (struct usb2dmx_universe *)u->user_data;
      /* lock the universe */

      if (u2d_u->universe_id==USB2DMX_UNIVERSE_IN || u2d_u->universe_id==USB2DMX_UNIVERSE_OUT)
	{
	  u2d_u->dmx_if->universes[u2d_u->universe_id] = NULL;
	  u2d_u->dmx_if = NULL;
          u2d_u->universe = NULL;
	}

      printk ("freeing universe->user_data = 0x%p\n", u->user_data);
      FREE(u->user_data);
      u->user_data = NULL;
      printk ("freeing universe->user_data done\n");
    }
  printk ("after usb2dmx_delete_universe (DMXUniverse *u=0x%p)\n", u);
  return 0;
}



/*
 * usb2dmx_create_universe
 *
 */
static int usb2dmx_create_universe (DMXUniverse *u, DMXPropList *pl)
{
  printk ("usb2dmx_create_universe (DMXUniverse *u=0x%p, DMXPropList *pl=0x%p)" _FUNCFRAME_ "\n", u, pl, FUNCFRAME_(usb2dmx_create_universe));
  if (u && u->interface)
    {
      struct usb2dmx_universe *u2d_u = NULL;
      unsigned long connector_id = 0;

      if (pl && pl->find)
        {
          DMXProperty *p = pl->find(pl, "conid");
          if (p)
            p->get_long (p, &connector_id);
        }

      u2d_u = DMX_ALLOC(struct usb2dmx_universe);
      if (u2d_u)
	{
	  memset (u2d_u->buffer, 0, 512);

	  u->user_data  = (void *)u2d_u;

	  u->user_delete = usb2dmx_delete_universe;
	  u->read_slots  = dmx_read_universe;
	  u->data_available = usb2dmx_data_available;
	  if (u->kind == 0)
	    {
	      /* output universe */
	      u->write_slots = dmx_write_universe;
	      strcpy (u->connector, "OUT");
	      u->conn_id = USB2DMX_UNIVERSE_OUT;
	      u2d_u->universe_id = USB2DMX_UNIVERSE_OUT;
	    }
	  else
	    {
	      /* input universe */
	      strcpy (u->connector, "IN");
	      u->conn_id = USB2DMX_UNIVERSE_IN;
	      u2d_u->universe_id = USB2DMX_UNIVERSE_IN;
	    }
          u2d_u->buffer_offset = connector_id*512;
	  u2d_u->dmx_if = (struct usb2dmx_interface *)u->interface->user_data;
	  u2d_u->universe    = u;
	  u2d_u->dmx_if->universes[u2d_u->universe_id] = u2d_u;
	}
      return 0;
    }
  return -1;
}


/*
 * usb2dmx_delete_interface
 *
 */
static int usb2dmx_delete_interface (DMXInterface *dif)
{
  printk ("usb2dmx_delete_interface (DMXInterface *dif=%p)" _FUNCFRAME_ "\n", dif, FUNCFRAME_(usb2dmx_delete_interface));

  if (dif && dif->user_data)
    {
      // int waitpid_result = 0;
      int ret = 0;
      struct usb2dmx_interface *u2d_if = (struct usb2dmx_interface *)dif->user_data;

      /* lock the interface */


      if (u2d_if->thread_pid > 0) /* we don't want to kill init */
	{
	  printk ("attempting to kill usb2dmx-thread pid=%d\n", u2d_if->thread_pid);
	  ret = kill_proc(u2d_if->thread_pid, SIGTERM, 1);
	  if (ret)
	    {
	      printk(KERN_ERR "usb2dmx: unable to signal thread\n");
	      return -1;
	    }
          THREAD_SEM_WAIT_COMPLETE(&u2d_if->thr_exited);
	    /* down(&u2d_if->thr_exited); */
	  // waitpid_result = waitpid (u2d_if->thread_pid, NULL, __WCLONE|WNOHANG);
	  printk("usb2dmx thread has been stopped\n");
	}

      printk("usb2dmx thread has been stopped\n");

      /* be paranoyd and wait for a while */
      schedule_timeout(HZ/5);


      if (u2d_if->universes[USB2DMX_UNIVERSE_IN])
	u2d_if->universes[USB2DMX_UNIVERSE_IN]->dmx_if = NULL;
      if (u2d_if->universes[USB2DMX_UNIVERSE_OUT])
	u2d_if->universes[USB2DMX_UNIVERSE_OUT]->dmx_if = NULL;
      u2d_if->universes[USB2DMX_UNIVERSE_IN] = NULL;
      u2d_if->universes[USB2DMX_UNIVERSE_IN] = NULL;

      u2d_if->interface = NULL;

      printk ("freeing interface->user_data = 0x%p\n", dif->user_data);
      FREE(dif->user_data);
      dif->user_data = NULL;
      printk ("freeing interface->user_data done\n");
    }
  return 0;
}


/*
 * usb2dmx_getlong_command
 *
 * This isues a command that returns a long value from the interface.
 */
static long usb2dmx_getlong_command (struct usb2dmx_interface *u2d_if, int cmd)
{
  long d = 0;
  int result = usb_control_msg(u2d_if->dmx_dev, usb_rcvctrlpipe(u2d_if->dmx_dev, 0),
			       cmd,
			       USB_DIR_IN | USB_TYPE_VENDOR, 0, 0, &d, sizeof(d), USB2DMX_TIMEOUT);
  if (result < 0)
    return result;
  return d;
}

#define usb2dmx_get_receive_framecount(u2d_if) usb2dmx_getlong_command(u2d_if,DMX_RX_FRAMES)
#define usb2dmx_get_startcode(u2d_if) usb2dmx_getlong_command(u2d_if,DMX_RX_STARTCODE)
#define usb2dmx_get_slots(u2d_if) usb2dmx_getlong_command(u2d_if,DMX_RX_SLOTS)

/*
 * usb2dmx_event
 *
 * compute an event from the userspace.
 * This function must only be called from the
 * usb2dmx_interface_thread.
 */
static void usb2dmx_rx_event(struct usb2dmx_interface *u2d_if)
{
  if (u2d_if)
    {
      struct usb2dmx_universe *u2d_u = u2d_if->universes[USB2DMX_UNIVERSE_IN];
      if (u2d_u  && u2d_u->universe)
	{
	  long rcframecount = usb2dmx_get_receive_framecount(u2d_if);
	  if (u2d_u->framecount != rcframecount)
	    {
	      unsigned char inbuffer[512];
	      int result;

	      u2d_u->framecount = rcframecount;

	      result = usb_control_msg(u2d_if->dmx_dev,
				       usb_rcvctrlpipe(u2d_if->dmx_dev, 0),
				       DMX_RX_MEM,
				       USB_DIR_IN | USB_TYPE_VENDOR, 0,
				       0, inbuffer, 512, USB2DMX_TIMEOUT);
	      if (result >= 24 && result <= 512)
		{
		  if (memcmp(u2d_u->buffer, inbuffer, 512))
		    {
		      memcpy(u2d_u->buffer, inbuffer, result);
		      /* u2d_u->framecount = rcframecount; */
		      u2d_u->data_avail = 1;
		      u2d_u->universe->signal_changed (u2d_u->universe, 0, 512);
		    }
		}
	    }
	}
    }
}

static int usb2dmx_setlong_command (struct usb2dmx_interface *u2d_if, int cmd, long d)
{
  return usb_control_msg(u2d_if->dmx_dev, /* struct usb_device *dev */
			 usb_sndctrlpipe(u2d_if->dmx_dev, 0), /* unsigned int pipe */
			 cmd, /* u8 request */
			 USB_DIR_OUT | USB_TYPE_VENDOR, /* u8 requesttype */
			 d, 0, /* u16 value, u16 index */
			 0, 0, /* void *data, u16 size */
			 USB2DMX_TIMEOUT);
}

#define usb2dmx_set_slots(u2d_if, s) usb2dmx_setlong_command(u2d_if,DMX_TX_SLOTS, s)
#define usb2dmx_set_startcode(u2d_if, s) usb2dmx_setlong_command(u2d_if,DMX_TX_STARTCODE, s)
#define usb2dmx_set_led(u2d_if, l) usb2dmx_setlong_command(u2d_if,DMX_ID_LED, l)

static void usb2dmx_tx_event(struct usb2dmx_interface *u2d_if)
{
  if (u2d_if->data_pending)
    {
      struct usb2dmx_universe *u2d_u = u2d_if->universes[USB2DMX_UNIVERSE_OUT];
      if (u2d_u)
	{
	  unsigned char *buff = u2d_u->buffer;
	  int result = 0;

#ifdef DEBUG
	  printk (KERN_INFO "data-pending - sending it\n");
#endif

	  u2d_if->data_pending = 0;
	  result = usb_control_msg(u2d_if->dmx_dev,
				   usb_sndctrlpipe(u2d_if->dmx_dev, 0),
				   DMX_TX_MEM,
				   USB_DIR_OUT | USB_TYPE_VENDOR, 0,
				   0, buff, 512, USB2DMX_TIMEOUT);
	  if (result!=512)
	    {
	      u2d_if->data_pending = 1;
	      printk (KERN_ERR "failed to send dmx-data\n");
	    }
#ifdef DEBUG
	  else
	    printk(KERN_INFO "wrote %d bytes to usb2dmx\n", size);
#endif
	  u2d_u->data_avail = 1;
	  if (u2d_u->universe && u2d_u->universe->signal_changed)
	    u2d_u->universe->signal_changed (u2d_u->universe, 0, 512);
	}
    }
}

/* DMXProperty functions*/

static int usb2dmx_set_led_prop(DMXProperty *prop, long val)
{
  DMXInterface *dif;
  struct usb2dmx_interface *u2d_if;

  if(!prop) return -1;
  if(!prop->data) return -1;
  if(val<0 || val>255) return -1;

  dif=(DMXInterface*) prop->data;
  if(!dif->user_data) return -1;

  u2d_if=(struct usb2dmx_interface*)dif->user_data;
  u2d_if->id_led=val;

  return val;
}

static int usb2dmx_get_led_prop(DMXProperty *prop, long *val)
{
  DMXInterface *dif;
  struct usb2dmx_interface *u2d_if;

  if(!prop) return -1;
  if(!prop->data) return -1;

  dif=(DMXInterface*) prop->data;
  if(!dif->user_data) return -1;

  u2d_if=(struct usb2dmx_interface*)dif->user_data;
  *val=-u2d_if->id_led;

  return 0;
}

/*
 * u2d_interface_thread
 *
 */
static int u2d_interface_thread (void *user_data)
{
  int timeout = HZ / 30;
  int last_jiffies = jiffies;
  struct usb2dmx_interface *u2d_if = (struct usb2dmx_interface *)user_data;

  printk ("u2d_interface_thread (void *user_data=0x%p)" _FUNCFRAME_ "\n" , user_data, FUNCFRAME_(u2d_interface_thread));

  if(inputrate<1)
    inputrate=1;

  timeout=HZ/inputrate;

  if (!user_data)
    {
      printk ("u2d_interface_thread: user_data = NULL -> exiting\n");
      THREAD_SEM_EXIT (&u2d_if->thr_exited, 1);
    }


  lock_kernel();

  daemonize ("usb2dmxd");
  spin_lock_irq(&usb2dmx_thread_lock);
  sigemptyset(&current->blocked);
  recalc_sigpending();
  spin_unlock_irq(&usb2dmx_thread_lock);

  strncpy (current->comm, "usb2dmxd", sizeof(current->comm) - 1);
  current->comm[sizeof(current->comm) - 1] = '\0';

  u2d_if->running = 1;

  do
    {
      int stat = 1;

      /*
       * Check for pending data before going to sleep and wait for a signal that
       * data is pending is nessesary here, because the interruptible_sleep_on_timeout
       * only returns for signals arived while it is executed. If we don't check data_pending
       * here, we may loose some updates.
       */
      if (u2d_if->data_pending==0)
	stat = interruptible_sleep_on_timeout(&u2d_if->waitqueue, timeout);

      /*
       * data is pending -> update it to the interface
       */
      if (stat)
	usb2dmx_tx_event(u2d_if);

      /*
       * We wan't to check for input data from time to time.
       */
      if (jiffies > (HZ/inputrate)+last_jiffies)
	{
	  last_jiffies = jiffies;
	  /* timout, do some sensefull things here, e.g. poll for input */
	  if (u2d_if->universes && u2d_if->universes[USB2DMX_UNIVERSE_IN])
	    usb2dmx_rx_event(u2d_if);
	}

      if(u2d_if->id_led>0 && u2d_if->id_led<256)
	{
	  usb2dmx_set_led(u2d_if, u2d_if->id_led);
	  u2d_if->id_led=-u2d_if->id_led;
	}

    } while (!signal_pending(current));

  printk ("usb2dmx thread is exiting\n");
  u2d_if->running = 0;

  THREAD_SEM_EXIT (&u2d_if->thr_exited, 0);

  return 0;
}


/*
 *
 */
static int usb2dmx_create_interface (DMXInterface *dif, DMXPropList *pl)
{
  printk ("usb2dmx_create_interface (DMXInterface *dif=%p, DMXPropList *pl=%p)" _FUNCFRAME_ "\n", dif, pl, FUNCFRAME_(usb2dmx_create_interface));
  if (dif)
    {
      struct usb2dmx_interface  *u2d_if=NULL;
      struct usb_device         *usbdev = NULL;
#if 0
      unsigned int               ifnum = 0;
#endif

      if (pl && pl->find)
	{
	  DMXProperty *p = pl->find(pl, "usbdev");
	  if (p)
	    p->get_long (p, (unsigned long *)&usbdev);

	  p=dmxprop_create_long("led", 0xFF);
	  if(p)
	    {
	      if(dmxprop_user_long(p, usb2dmx_get_led_prop, usb2dmx_set_led_prop, dif) < 0)
		p->delete(p);
	      else
		pl->add(pl,p);
	    }
	}

      if (!usbdev) /* fail to create, if no usbdevice has been given */
	{
	  printk("usb2dmx: failed to evaluate usbdev parameter\n");
	  return -1;
	}

      if (usbdev->actconfig->desc.iConfiguration)
	{
	  char str[128];
	  if(usb_string(usbdev, usbdev->actconfig->desc.iConfiguration, str, sizeof(str)) >= 0 )
	    info ("current configuration is \"%s\"", str);
	}

      u2d_if = DMX_ALLOC(struct usb2dmx_interface);
      if (u2d_if)
	{
	  int i;

	  dif->user_data = (void *)u2d_if;
	  dif->user_delete = usb2dmx_delete_interface;

	  for (i=0; i<sizeof(u2d_if->universes)/sizeof(u2d_if->universes[0]); i++)
	    u2d_if->universes[i] = NULL;
	  u2d_if->interface = dif;
	  u2d_if->dmx_dev   = usbdev; /* usb-device handle */
	  init_waitqueue_head (&u2d_if->waitqueue);
	  THREAD_SEM_INIT (&u2d_if->thr_exited);

          u2d_if->rx_frames = 0;
	  u2d_if->do_read = 0;
	  init_waitqueue_head (&u2d_if->read_waitqueue);
	  u2d_if->running = 0; /* set by the thread */
	  u2d_if->data_pending = 0; /* no data pending at the beginning */
	  u2d_if->id_led = 0xFF;

	  printk ("starting usb2dmx thread\n");
	  u2d_if->thread_pid = kernel_thread(u2d_interface_thread, (void *)u2d_if, CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
	  if (u2d_if->thread_pid >= 0)
	    {
	      printk ("usb2dmx thread successfully started\n");
	      return 0;
	    }
	  FREE(u2d_if);
	}
    }
  return -1;
}







/*
 * Some data structures for USB2DMX declaration.
 *
 */
static int  usb2dmx_probe (struct usb_interface *intf, const struct usb_device_id *id);
static void usb2dmx_disconnect(struct usb_interface *intf);


/*
Name			PID	default configuration

USBDMX X-Switch		0x01	1x Input + 1x Output
Rodin1		        0x02	1x Output
Rodin2		        0x03	1x Input
USBDMX21	        0x06	1x Input + 2xOutput
RodinT		        0x08	1x Input + 1xOutput
*/
static struct usb_device_id usb2dmx_id_table [] =
{
  { USB_DEVICE(VENDOR_ID_LIGHTINGSOLUTIONS, 0x1), driver_info: 0 },  /* USBDMX X-Switch */
  { USB_DEVICE(VENDOR_ID_LIGHTINGSOLUTIONS, 0x2), driver_info: 0 },  /* Rodin1 */
  { USB_DEVICE(VENDOR_ID_LIGHTINGSOLUTIONS, 0x3), driver_info: 0 },  /* Rodin2 */
  { USB_DEVICE(VENDOR_ID_LIGHTINGSOLUTIONS, 0x6), driver_info: 0 },  /* USBDMX21 */
  { USB_DEVICE(VENDOR_ID_LIGHTINGSOLUTIONS, 0x8), driver_info: 0 },  /* RodinT */
  { }                                         /* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, usb2dmx_id_table);




static struct usb_driver dmx_driver =
{
  .name =		"USB2DMX",
  .probe =		usb2dmx_probe,
  .disconnect =		usb2dmx_disconnect,
//  .driver_list =	LIST_HEAD_INIT(dmx_driver.driver_list),
  .id_table =		usb2dmx_id_table,
/*
  struct semaphore serialize;
  void (*suspend)(struct usb_device *dev);
  void (*resume)(struct usb_device *dev);
*/
};


struct usb2dmxConfiguration_t
{
  int   pid;
  char *name;
  int   default_conf;
  int   num_outputs;
  int   num_inputs;
};


/*
Name			PID	default configuration
USBDMX X-Switch		0x01	0x02			1x Input + 1x Output
MicroUSBDMX TX		0x02	0x01			1x Output
MicroUSBDMX RX		0x03	0x02			1x Input
*/
struct usb2dmxConfiguration_t usb2dmx_info[] =
{ /* PID    Name             Default  Num  Num */
  /*                         Config   Out  In  */
  { 0x01, "USBDMX X-Switch", 0x02,     1,   1  }, /* 1x Input + 1x Output */
  { 0x02, "Rodin1",          0x01,     1,   0  }, /* 1x Output */
  { 0x03, "Rodin2",          0x01,     0,   1  }, /* 1x Input */
  { 0x06, "USBDMX21",        0x01,     2,   1  }, /* 1x Input + 2x Output */
  { 0x08, "RodinT",          0x01,     1,   1  }, /* 1x Input + 1x Output */
};
#define NUM_USBDMX_INFO_ENTRIES  (sizeof(usb2dmx_info)/sizeof(usb2dmx_info[0]))


/*
 * Find a config entry for a given pid.
 *
 */
static struct usb2dmxConfiguration_t * findUSB2DMXInfo (int pid)
{
  int i;
  for (i=0; i<NUM_USBDMX_INFO_ENTRIES; i++)
    if (pid == usb2dmx_info[i].pid)
      return &usb2dmx_info[i];
  return NULL;
}




/* usb2dmx_probe
 *
 * Is called after a USB device has been added
 * to the bus and check wether it is a USB2DMX.
 * If so, it initializes it and creates as much
 * DMX universes as the interface provides.
 */
static int usb2dmx_probe (struct usb_interface *intf, const struct usb_device_id *id)
{
  struct usb_device *dev = interface_to_usbdev (intf);
  struct usb2dmxConfiguration_t * usbdmx_info = NULL;
  DMXDriver *drv = NULL;
  //char str[128];
  //int res;

  /*
   * Try to find the description for that interface.
   * and return unsuccessfull if none has been found.
   */
  usbdmx_info = findUSB2DMXInfo (id->idProduct);
  if (usbdmx_info == NULL)
  {
    warn(KERN_INFO "USB2DMX model with product-id 0x%04X not supported.", id->idProduct);
    return -1;
  }
#if 0
  info("USB2DMX Model \"%s\" with %d input and %d output universes found at address %d",
       usbdmx_info->name,
       usbdmx_info->num_inputs,
       usbdmx_info->num_outputs,
       dev->devnum);
#endif

  /*
   * The Linux USB implementation has some bugs if the confugration is changed withing the probe.
   * the file  drivers/usb/usb.c  has to be patched.
   */
  /*
   * Initialize it with default values.
   */
#if 0 // TODO: it seems that changing configuration has changed in 2.6
  if(usb_set_configuration(dev, usbdmx_info->default_conf)) /* transmitter */
    err ("[%s@%d]: setting configuration failed", usbdmx_info->name, dev->devnum);
  else
    info("[%s@%d]: configuration set to %d",
	 usbdmx_info->name,
	 dev->devnum,
	 usbdmx_info->default_conf);

  if (dev->actconfig->desc.iConfiguration)
  {
    if((res = usb_string(dev, dev->actconfig->desc.iConfiguration, str, sizeof(str))) < 0 )
      err ("reading string for current config failed (error=%i)", res);
    else
      info ("configuration changed to \"%s\"", str);
  }
#endif


  /*
   * Create a USB2DMX specific record and a DMX universe.
   */
  drv = dmx_find_driver (usb2dmx_family, "usb2dmx");
  if (drv)
    {
      DMXInterface *dmx_if = drv->create_interface (drv, dmxproplist_vacreate ("usbdev=%l", (long)dev));
      if (dmx_if && dmx_if->user_data)
	{
	  int error = 0;
	  int i;

          usb_set_intfdata(intf, dmx_if);

	  for (i=0; error==0 && i<usbdmx_info->num_outputs; i++)
	    {
	      DMXUniverse  *uout = dmx_if->create_universe (dmx_if, 0, dmxproplist_vacreate ("conid=%l", (long)i)); /* output */
	      if (!uout)
		error++;
	    }

	  for (i=0; error==0 && i<usbdmx_info->num_inputs; i++)
	    {
	      DMXUniverse  *uin  = dmx_if->create_universe (dmx_if, 1, dmxproplist_vacreate ("conid=%l", (long)i)); /* input */
	      if (!uin)
		error++;
	    }

	  if (error == 0)
	    {
	      printk("[%s@%d]: input/output-universes created, usb.private=%08lX\n",
		     usbdmx_info->name, dev->devnum,
		     (unsigned long)dmx_if);
	      return 0;
	    }
	  printk("[%s@%d]: Error (%d) in creation of input/output-universes\n",
		 usbdmx_info->name, dev->devnum, error);

	  dmx_if->delete(dmx_if);
	}
    }
  else
    printk (KERN_INFO "unable to find driver for dmx-family usb2dmx.usb2dmx\n");

  printk("returning NULL as usb.private\n");
  /*
   * Something failed.
   */
  return -1;
}



/*
 *  disconnect method for usb2dmx.
 *
 * It is called after a USB2DMX device has been
 * removed from the USB bus.
 */
static void usb2dmx_disconnect(struct usb_interface *intf)
{
  DMXInterface *dmx = (DMXInterface *)usb_get_intfdata(intf);
  struct usb_device *dev = interface_to_usbdev (intf);

  printk ("usb2dmx_disconnect(struct usb_device *dev=0x%p, void *interface=0x%p" _FUNCFRAME_ "\n", dev, intf, FUNCFRAME_(usb2dmx_disconnect));
  if (dmx)
    {
#if 0
      wait(dmx->sem); /* wait for the interface to be unused and block it (forever :-) */
#endif
      printk (KERN_INFO "delete usb2dmx interface\n");
      if (dmx->delete)
	dmx->delete (dmx);
      else
        printk ("usb2dmx_disconnect: no delete function\n");
      /* does a cascaded delete on the universes of that interface */
    }
  else
    printk ("usb2dmx_disconnect: dmx == NULL\n");

  printk ("after usb2dmx_disconnect\n");
}


/*
 * usb2dmx_init
 *
 */
static int __init usb2dmx_init(void)
{
  DMXDriver *drv = NULL;

  usb2dmx_family = dmx_create_family("USB");
  if (!usb2dmx_family)
    {
      printk (KERN_INFO "unable to register dmx-family USB2DMX\n");
      return -1;
    }

  drv = usb2dmx_family->create_driver (usb2dmx_family, "usb2dmx", usb2dmx_create_universe, NULL);
  if (!drv)
    {
      usb2dmx_family->delete(usb2dmx_family, 0);
      info("failed to create u2b2dmx driver\n");
      return -1;
    }
  drv->num_out_universes = 1;
  drv->num_in_universes = 1;
  drv->user_create_interface = usb2dmx_create_interface;

  if (usb_register(&dmx_driver) < 0)
    {
      usb2dmx_family->delete(usb2dmx_family, 0);
      info("failed to register USB2DMX\n");
      return -1;
    }

  info("USB2DMX registered.");
  return 0;
}


/*
 * usb2dmx_cleanup
 *
 */
static void __exit usb2dmx_cleanup(void)
{
  usb_deregister(&dmx_driver);
  mdelay(5);
  if (usb2dmx_family)
    usb2dmx_family->delete(usb2dmx_family, 0);
}

module_init(usb2dmx_init);
module_exit(usb2dmx_cleanup);
