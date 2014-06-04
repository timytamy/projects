/*
 * ftdi2dmx.h
 * Driver for DMX output devices based on the FTDI FT232BM chip, including
 *	 http://www.enttec.com/dmxusb.php
 * and
 *	http://www.circellar.com, issue 170 (september 2004), page 72
 *	(also ftp://ftp.circuitcellar.com/pub/Circuit_Cellar/2004/170/kalbermatter_170.zip)
 * The ftdi chip and info is available from http://www.ftdichip.com/
 *
 * Copyright (C) 2004  Steve Tell
 * borrows heavily from usb2dmx, Copyright (C) 2001  Michael Stickel
 * version tell-26Nov2004 - output works, input not yet attempted.
 *
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/usb.h>

#include <dmx/dmxdev.h>
#include "ftdi2dmx.h"

MODULE_AUTHOR("(c) 2004 Steve Tell <tell@telltronics.org>");
MODULE_DESCRIPTION("Driver for the DMX-512 interfaces using FTDI FT232BM serial interface chip " DMXVERSION);
MODULE_LICENSE("GPL");

/* Use our own dbg macros */
#undef dbg
#define dbg(format, arg...) do { if (debug) printk(KERN_DEBUG __FILE__ ": " format "\n" , ## arg); } while (0)
#define dbg2(format, arg...) do { if (debug>1) printk(KERN_DEBUG __FILE__ ": " format "\n" , ## arg); } while (0)

static int inputrate=30;
module_param(inputrate, int, S_IRUGO);
MODULE_PARM_DESC(inputrate,"input universe is read <inputrate> times per second (default 30)");
static int debug=1;
module_param(debug, int, S_IRUGO);
MODULE_PARM_DESC(debug, "Debug enable and verbosity (0,1,2)");

/* handle up_and_exit confusion */
#include <linux/completion.h>
typedef struct completion           THREAD_SEM;
#define THREAD_SEM_EXIT(c,l)        complete_and_exit(c,l)
#define THREAD_SEM_DECLARE(c)       DECLARE_COMPLETION(c)
#define THREAD_SEM_INIT(c)          init_completion(c)
#define THREAD_SEM_WAIT_COMPLETE(c) wait_for_completion(c)


/* timeout for usb control messages */
#define FTDI2DMX_TIMEOUT 200

#define FUNCFRAME_(name)   name,sizeof(name)
#define _FUNCFRAME_        "= [0x%p, %u]"

static DEFINE_SPINLOCK(ftdi2dmx_thread_lock);

struct usb_ftdi2dmx;
struct ftdi2dmx_universe
{
  struct usb_ftdi2dmx      *dmx_if;
  unsigned char             universe_id; /* Universe id on that interface */
  DMXUniverse              *universe;
  volatile char             data_avail;  /* for userspace: 1 = data available on universe. 0 = no new values available on universe. */
  long                      framecount;  /* currently only on input universe */
  unsigned char             buffer[513]; /* includes room for the start byte */
};

#define FTDI2DMX_UNIVERSE_IN   (1)
#define FTDI2DMX_UNIVERSE_OUT  (0)

struct usb_ftdi2dmx
{
  struct usb_device        *udev;      /* init: probe_dmx      */
  struct usb_interface     *interface;
  struct kref              kref;

  DMXInterface             *dmxinterface;
  struct ftdi2dmx_universe  *universes[2];

  int                rx_frames;
  int                do_read;
  wait_queue_head_t  read_waitqueue;   /* wake(...) will wake up the worker thread for that interface */
  wait_queue_head_t  waitqueue;   /* wake(...) will wake up the worker thread for that interface */
  int                thread_pid;  /* pid of the worker thread */

  THREAD_SEM         thr_exited;  /* semaphore that signals that the thread is exiting */

  int                running;     /* is the worker thread running ? */
  char               data_pending;

  __u8               bulk_in_endpointAddr;	/* the address of the bulk in endpoint */
  struct urb *	     write_urb;		/* the urb used to send data */
  __u8		     bulk_out_endpointAddr;	/* the address of the bulk out endpoint */
  atomic_t	     write_busy;	/* true iff write urb is busy */
  struct completion  write_finished; 	/* wait for the write to finish */
};


DMXFamily *ftdi2dmx_family = NULL;


static int ftdi_usb_setup(struct usb_ftdi2dmx* dev);
static void ftdi_usb_set_break(struct usb_ftdi2dmx* dev, int break_state);
static __u16 ftdi_usb_get_status(struct usb_ftdi2dmx* dev);


#define to_ftdi2dmx_dev(d) container_of(d, struct usb_ftdi2dmx, kref)

static void ftdi2dmx_delete(struct kref *kref)
{
        struct usb_ftdi2dmx *dev = to_ftdi2dmx_dev(kref);

        usb_put_dev(dev->udev);
        kfree (dev);
}


/*
 * dmx_write_universe
 *
 * Used to write a couple of slot-values to the universe.
 */
static int dmx_write_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  dbg2("dmx_write_universe called");
  if (u && size>0 && offs+size <= 512)
    {
      struct ftdi2dmx_universe *u2d_u = (struct ftdi2dmx_universe *)u->user_data;
      if (u2d_u && u2d_u->dmx_if && u2d_u->universe_id==FTDI2DMX_UNIVERSE_OUT)
	{
	  /* copy the data and tell the thread something has changed */

	  memcpy ((u2d_u->buffer)+offs+1, (void *)buff, size);

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
 * ftdi2dmx_data_available
 */
int  ftdi2dmx_data_available (DMXUniverse *u, uint start, uint size)
{
  if (u && u->user_data)
    {
      struct ftdi2dmx_universe *u2d_u = (struct ftdi2dmx_universe *)u->user_data;
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
  dbg("dmx_read_universe called");
  if (u && size>0 && offs+size <= 512)
    {
      struct ftdi2dmx_universe *u2d_u = (struct ftdi2dmx_universe *)u->user_data;

      dbg2("dmx_read_universei: call is valid");
      if (u2d_u && u2d_u->dmx_if)
	{
          unsigned char *inbuffer = u2d_u->buffer;

	  dbg2("dmx_read_universe: ftdi2dmx universe data valid");

          if (u2d_u->data_avail)
            {
	      u2d_u->data_avail=0;

	      if (offs+size>512)
	        size = 512-offs;

	      memcpy (buff, inbuffer+offs, size);

	      return size;
            }
          else
	     dbg2("dmx_read_universe: no change at universe");
          return 0;
	}
      else
        dbg2("dmx_read_universei: ftdi2dmx universe data INVALID");
    }
  else
    dbg2("dmx_read_universe: call is INVALID: offs=%lu, size=%d", offs, size);
  return -EINVAL;
}


/*
 * ftdi2dmx_delete_universe
 *
 */
static int ftdi2dmx_delete_universe (DMXUniverse *u)
{
  dbg("ftdi2dmx_delete_universe (DMXUniverse *u=0x%p) " _FUNCFRAME_, u, FUNCFRAME_(ftdi2dmx_delete_universe));
  if (u && u->user_data)
    {
      struct ftdi2dmx_universe *u2d_u = (struct ftdi2dmx_universe *)u->user_data;
      /* lock the universe */

      if (u2d_u->universe_id==FTDI2DMX_UNIVERSE_IN || u2d_u->universe_id==FTDI2DMX_UNIVERSE_OUT)
	{
	  u2d_u->dmx_if->universes[u2d_u->universe_id] = NULL;
	  u2d_u->dmx_if = NULL;
          u2d_u->universe = NULL;
	}

      dbg("freeing universe->user_data = 0x%p", u->user_data);
      FREE(u->user_data);
      u->user_data = NULL;
      dbg("freeing universe->user_data done");
    }
  dbg("after ftdi2dmx_delete_universe (DMXUniverse *u=0x%p)", u);
  return 0;
}



/*
 * ftdi2dmx_create_universe
 *
 */
static int ftdi2dmx_create_universe (DMXUniverse *u, DMXPropList *pl)
{
  dbg("ftdi2dmx_create_universe (DMXUniverse *u=0x%p, DMXPropList *pl=0x%p)" _FUNCFRAME_, u, pl, FUNCFRAME_(ftdi2dmx_create_universe));
  if (u && u->interface)
    {
      struct ftdi2dmx_universe *u2d_u = NULL;

      u2d_u = DMX_ALLOC(struct ftdi2dmx_universe);
      if (u2d_u)
	{
	  memset (u2d_u->buffer, 0, 513);

	  u->user_data  = (void *)u2d_u;

	  u->user_delete = ftdi2dmx_delete_universe;
	  u->read_slots  = dmx_read_universe;
	  u->data_available = ftdi2dmx_data_available;
	  if (u->kind == 0)
	    {
	      /* output universe */
	      u->write_slots = dmx_write_universe;
	      strcpy (u->connector, "OUT");
	      u->conn_id = FTDI2DMX_UNIVERSE_OUT;
	      u2d_u->universe_id = FTDI2DMX_UNIVERSE_OUT;
	    }
	  else
	    {
	      /* input universe */
	      strcpy (u->connector, "IN");
	      u->conn_id = FTDI2DMX_UNIVERSE_IN;
	      u2d_u->universe_id = FTDI2DMX_UNIVERSE_IN;
	    }
	  u2d_u->dmx_if = (struct usb_ftdi2dmx *)u->interface->user_data;
	  u2d_u->universe    = u;
	  u2d_u->dmx_if->universes[u2d_u->universe_id] = u2d_u;
	}
      return 0;
    }
  return -1;
}


void
ftdi2dmx_thread_cleanup(struct usb_ftdi2dmx *u2d_if)
{
      //int waitpid_result = 0;
      int ret = 0;

      if (u2d_if->thread_pid > 0) /* we don't want to kill init */
	{
	  info("attempting to kill ftdi2dmxd thread pid=%d", u2d_if->thread_pid);
	  ret = kill_proc(u2d_if->thread_pid, SIGTERM, 1);
	  if (ret)
	    {
	      err("ftdi2dmx_thread_cleanup() unable to signal thread");
	      /*return -1;*/
	    }
          THREAD_SEM_WAIT_COMPLETE(&u2d_if->thr_exited);
	    /* down(&u2d_if->thr_exited); */
	  //waitpid_result = waitpid (u2d_if->thread_pid, NULL, __WCLONE|WNOHANG);
	  info("ftdi2dmxd thread has been stopped");
	} else {
		dbg("ftdi2dmx_thread_cleanup: wasn't running.");
	}

      /* be paranoyd and wait for a while */
      schedule_timeout(HZ/5);
}

/*
 * ftdi2dmx_delete_interface.  assumes thred is already stopped.
 */
static int ftdi2dmx_delete_interface (DMXInterface *dif)
{
  dbg("ftdi2dmx_delete_interface (DMXInterface *dif=%p)" _FUNCFRAME_, dif, FUNCFRAME_(ftdi2dmx_delete_interface));

  if (dif && dif->user_data)
    {
      struct usb_ftdi2dmx *u2d_if = (struct usb_ftdi2dmx *)dif->user_data;

      /* ftdi2dmx_thread_cleneanup(u2d_if); */

      if (u2d_if->universes[FTDI2DMX_UNIVERSE_IN])
	u2d_if->universes[FTDI2DMX_UNIVERSE_IN]->dmx_if = NULL;
      if (u2d_if->universes[FTDI2DMX_UNIVERSE_OUT])
	u2d_if->universes[FTDI2DMX_UNIVERSE_OUT]->dmx_if = NULL;
      u2d_if->universes[FTDI2DMX_UNIVERSE_IN] = NULL;
      u2d_if->universes[FTDI2DMX_UNIVERSE_IN] = NULL;

      if(u2d_if->write_urb)
	      usb_free_urb(u2d_if->write_urb);

      u2d_if->interface = NULL;

      dbg("freeing interface->user_data = 0x%p", dif->user_data);
      FREE(dif->user_data);
      dif->user_data = NULL;
      dbg("freeing interface->user_data done");
    }
  return 0;
}


static __u16 ftdi_usb_get_status(struct usb_ftdi2dmx* dev)
{
	int retval = 0;
	size_t count = 0;
	__u16 buf;

	retval = usb_bulk_msg (dev->udev,
				usb_rcvbulkpipe (dev->udev, dev->bulk_in_endpointAddr),
				&buf, 2, &count, HZ*10);

	if (retval)
		return 0;

	return buf;
}


/*#define ftdi2dmx_get_receive_framecount(u2d_if) ftdi2dmx_getlong_command(u2d_if,DMX_RC_FRAMES)*/


/*
 * ftdi2dmx_event
 *
 * compute an event from the userspace.
 * This function must only be called from the
 * ftdi2dmx_interface_thread.
 */
#if 0
static void ftdi2dmx_rx_event(struct ftdi2dmx_interface *u2d_if)
{
  if (u2d_if)
    {
      struct ftdi2dmx_universe *u2d_u = u2d_if->universes[FTDI2DMX_UNIVERSE_IN];
      if (u2d_u  && u2d_u->universe)
	{
#if 0
	  /* TODO receive for ftdi */

	  long rcframecount = ftdi2dmx_get_receive_framecount(u2d_if);
	  if (u2d_u->framecount != rcframecount)
	    {
	      unsigned char inbuffer[512];
	      int result;

	      u2d_u->framecount = rcframecount;


	      result = usb_control_msg(u2d_if->udev,
				       usb_rcvctrlpipe(u2d_if->udev, 0),
				       DMX_RC_MEM,
				       USB_DIR_IN | USB_TYPE_VENDOR, 0,
				       0, inbuffer, 512, FTDI2DMX_TIMEOUT);
	      if (result >= 24 && result <= 512)
		{
		  if (memcmp(u2d_u->buffer, inbuffer, 512))
		    {
		      /* printk("received frame %lu, ret=%d, signaling  {$%02X,$%02X,$%02X,$%02X}\n", rcframecount, result, inbuffer[0],inbuffer[1],inbuffer[2],inbuffer[3]); */

		      memcpy(u2d_u->buffer, inbuffer, result);
		      /* u2d_u->framecount = rcframecount; */
		      u2d_u->data_avail = 1;
		      u2d_u->universe->signal_changed (u2d_u->universe, 0, 512);
		    }
		}
	    }
#endif
	}
    }
}
#endif

static void ftdi_write_bulk_callback(struct urb *urb
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
				     , struct pt_regs * regs
#endif
				     )
{
	struct usb_ftdi2dmx *u2d_if = (struct usb_ftdi2dmx *)urb->context;

	dbg2("%s", __FUNCTION__);
	/* sync/async unlink faults aren't errors */
	if (urb->status && !(urb->status == -ENOENT ||
				urb->status == -ECONNRESET)) {
		err("%s - nonzero write bulk status received: %d",
		    __FUNCTION__, urb->status);
	}

        /* free up our allocated buffer */
        usb_buffer_free(urb->dev, urb->transfer_buffer_length,
                        urb->transfer_buffer, urb->transfer_dma);

	/* notify anyone waiting that the write has finished */
	atomic_set (&u2d_if->write_busy, 0);
	complete (&u2d_if->write_finished);
}


static void ftdi2dmx_tx_event(struct usb_ftdi2dmx *u2d_if)
{
  if (u2d_if->data_pending)
    {
      struct ftdi2dmx_universe *u2d_u = u2d_if->universes[FTDI2DMX_UNIVERSE_OUT];
      unsigned char *buff = u2d_u->buffer;
      int result = 0;
      __u16 stat;

      dbg2("ftdi2dmx_tx_event: sending");

#ifdef FTDI2DMX_SINGLE_FRAME
      u2d_if->data_pending = 0;
#endif

      /* wait for previous send of data to finish */
      if (atomic_read (&u2d_if->write_busy))
	      wait_for_completion (&u2d_if->write_finished);

      /* poll ftdi-uart to see if transmit buffer is empty */
      do {
	      stat = ftdi_usb_get_status(u2d_if);
	      if (stat == 0) {
		      err("ftdi_usb_get_status error");
		      return;
	      }
      } while ( (stat & ((FTDI_RS_TEMT) << 8) ) == 0 ) ;

      /* now we can set break */
      ftdi_usb_set_break(u2d_if, 1);
      ftdi_usb_set_break(u2d_if, 0);

      init_completion (&u2d_if->write_finished);
      atomic_set (&u2d_if->write_busy, 1);
      usb_fill_bulk_urb(u2d_if->write_urb,
			u2d_if->udev,
			usb_sndbulkpipe(u2d_if->udev, u2d_if->bulk_out_endpointAddr),
			buff, 513,
			ftdi_write_bulk_callback,
			u2d_if);


      result = usb_submit_urb(u2d_if->write_urb, GFP_ATOMIC);
      if (result) {
	      atomic_set (&u2d_if->write_busy, 0);
	      err("%s - failed submitting write urb, error %d",
		  __FUNCTION__, result);
      }

      if (u2d_u)
	{
	  u2d_u->data_avail = 1;
	  if (u2d_u->universe && u2d_u->universe->signal_changed)
	    u2d_u->universe->signal_changed (u2d_u->universe, 0, 512);
	}
    }
}



/*
 * u2d_interface_thread
 *
 */
static int u2d_interface_thread (void *user_data)
{
  int timeout = HZ / 30;
  struct usb_ftdi2dmx *u2d_if = (struct usb_ftdi2dmx *)user_data;

  dbg("u2d_interface_thread (void *user_data=0x%p)" _FUNCFRAME_, user_data, FUNCFRAME_(u2d_interface_thread));

  if(inputrate<1)
    inputrate=1;

  timeout=HZ/inputrate;

  if (!user_data)
    {
      err("u2d_interface_thread: user_data = NULL -> exiting");
      THREAD_SEM_EXIT (&u2d_if->thr_exited, 1);
    }


  lock_kernel();

  daemonize ("usb_ftdi2dmx");
  allow_signal(SIGTERM);
  spin_lock_irq(&ftdi2dmx_thread_lock);
  sigemptyset(&current->blocked);
  recalc_sigpending();
  spin_unlock_irq(&ftdi2dmx_thread_lock);

  strncpy (current->comm, "ftdi2dmxd", sizeof(current->comm) - 1);
  current->comm[sizeof(current->comm) - 1] = '\0';

  u2d_if->running = 1;

  do
    {
      int stat = 1;
      yield();

      /*
       * Check for pending data before going to sleep and wait for a signal that
       * data is pending is nessesary here, because the interruptible_sleep_on_timeout
       * only returns for signals arived while it is executed. If we don't check data_pending
       * here, we may lose some updates.
       *   what about signal between the test and the sleep? - sgt
       */
      if (u2d_if->data_pending==0)
	stat = interruptible_sleep_on_timeout(&u2d_if->waitqueue, timeout);

      /*
       * data is pending -> update it to the interface
       */
      if (stat)
	     ftdi2dmx_tx_event(u2d_if);

      /*
       * We want to check for input data from time to time.
       */
#if 0
      /* rx postponed for now - sgt */
      if (jiffies > (HZ/inputrate)+last_jiffies)
	{
	  last_jiffies = jiffies;
	  if (u2d_if->universes && u2d_if->universes[FTDI2DMX_UNIVERSE_IN])
	    ftdi2dmx_rx_event(u2d_if);
	}
#endif
    } while (!signal_pending(current));

  if (atomic_read (&u2d_if->write_busy))
	  wait_for_completion (&u2d_if->write_finished);

  info("thread is exiting");
  u2d_if->running = 0;

  THREAD_SEM_EXIT (&u2d_if->thr_exited, 0);

  return 0;
}



/*
 *
 */
static int ftdi2dmx_create_interface (DMXInterface *dif, DMXPropList *pl)
{
  dbg("ftdi2dmx_create_interface (DMXInterface *dif=%p, DMXPropList *pl=%p)" _FUNCFRAME_, dif, pl, FUNCFRAME_(ftdi2dmx_create_interface));
  if (dif)
    {
      struct usb_ftdi2dmx *u2d_if=NULL;
      struct usb_device *usbdev = NULL;
      struct usb_interface *interface;
      struct usb_host_interface *iface_desc;

      /* ifnum should come from the call to probe(), but we know that
	 the FT232BM only has one interface.*/
      unsigned int               ifnum = 0;

      if (pl && pl->find)
	{
	  DMXProperty *p = pl->find(pl, "usbdev");
	  if (p)
	    p->get_long (p, (unsigned long *)&usbdev);
	}
      if (!usbdev) /* fail to create, if no usbdevice has been given */
	{
	  err("ftdi2dmx_create_interface: failed to evaluate usbdev parameter");
	  return -1;
	}

      interface = usbdev->actconfig->interface[ifnum];

      u2d_if = DMX_ALLOC(struct usb_ftdi2dmx);
      if (u2d_if)
	{
	  int i;

	  dif->user_data = (void *)u2d_if;
	  dif->user_delete = ftdi2dmx_delete_interface;

	  for (i=0; i<sizeof(u2d_if->universes)/sizeof(u2d_if->universes[0]); i++)
	    u2d_if->universes[i] = NULL;
	  u2d_if->dmxinterface = dif;
	  u2d_if->udev = usbdev; /* usb-device handle */

	  /* initialize the ftdi hardware */
	  ftdi_usb_setup(u2d_if);

	  /* allocate write_urb */

	  iface_desc = &interface->altsetting[0];
	  for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		struct usb_endpoint_descriptor *endpoint;
		endpoint = &iface_desc->endpoint[i].desc;

                if ((endpoint->bEndpointAddress & 0x80) &&
                    ((endpoint->bmAttributes & 3) == 0x02)) {
                        /* we found a bulk in endpoint */
                        u2d_if->bulk_in_endpointAddr = endpoint->bEndpointAddress;
                }

                if (((endpoint->bEndpointAddress & 0x80) == 0x00) &&
                    ((endpoint->bmAttributes & 3) == 0x02)) {
                        /* we found a bulk out endpoint */
                        u2d_if->write_urb = usb_alloc_urb(0, GFP_ATOMIC);
                        if (!u2d_if->write_urb) {
                                err("No free urbs available");
				goto error;
                        }
                        u2d_if->bulk_out_endpointAddr = endpoint->bEndpointAddress;
                }
	  }

	  if(!u2d_if->bulk_in_endpointAddr || !u2d_if->bulk_out_endpointAddr) {
		  err("bulk in and out endpoints not found");
		  goto error;
	  }
	  dbg("ftdi2dmx endpoints: in=%x out=%x",
		u2d_if->bulk_in_endpointAddr, u2d_if->bulk_out_endpointAddr);

	  /* initialize of write_finished needed? */
	  atomic_set (&u2d_if->write_busy, 0);

	  init_waitqueue_head (&u2d_if->waitqueue);
	  THREAD_SEM_INIT (&u2d_if->thr_exited);

          u2d_if->rx_frames = 0;
	  u2d_if->do_read = 0;
	  init_waitqueue_head (&u2d_if->read_waitqueue);
	  u2d_if->running = 0; /* set by the thread */
	  u2d_if->data_pending = 0; /* no data pending at the beginning */

	  info("starting ftdi2dmxd thread");
	  u2d_if->thread_pid = kernel_thread(u2d_interface_thread, (void *)u2d_if, CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
	  if (u2d_if->thread_pid >= 0)
	    {
	      info("ftdi2dmxd thread successfully started pid=%d", u2d_if->thread_pid);
	      return 0;
	    }
	}
 error:
      if(u2d_if)
	 FREE(u2d_if);
    }

  return -1;
}


static __u32 ftdi_usb_baud_to_divisor(int baud)
{
	static const unsigned char divfrac[8] = { 0, 3, 2, 4, 1, 5, 6, 7 };
	__u32 divisor;
	int divisor3 = 48000000 / 2 / baud; /* divisor shifted 3 bits to the left */
	divisor = divisor3 >> 3;
	divisor |= (__u32)divfrac[divisor3 & 0x7] << 14;
	/* Deal with special cases for highest baud rates. */
	if (divisor == 1) divisor = 0; else     /* 1.0 */
	if (divisor == 0x4001) divisor = 1;     /* 1.5 */
	return divisor;
}

static int ftdi_usb_set_speed(struct usb_ftdi2dmx* dev)
{
	char *buf;
	__u16 urb_value;
	__u16 urb_index;
	__u32 urb_index_value;
	int rv;

	buf = kmalloc(1, GFP_NOIO);
	if (!buf)
		return -ENOMEM;

	urb_index_value = ftdi_usb_baud_to_divisor(250000);
	urb_value = (__u16)urb_index_value;
	urb_index = (__u16)(urb_index_value >> 16);

	rv = usb_control_msg(dev->udev,
				usb_sndctrlpipe(dev->udev, 0),
				FTDI_SIO_SET_BAUDRATE_REQUEST,
				FTDI_SIO_SET_BAUDRATE_REQUEST_TYPE,
				urb_value, urb_index,
				buf, 0, HZ*10);

	kfree(buf);
	return rv;
}

static int ftdi_usb_setup(struct usb_ftdi2dmx* dev)
{
	__u16 urb_value;
	char buf[1];

	urb_value = FTDI_SIO_SET_DATA_STOP_BITS_2 | FTDI_SIO_SET_DATA_PARITY_NONE;
	urb_value |= 8; /* number of data bits */

	if (usb_control_msg(dev->udev, usb_sndctrlpipe(dev->udev, 0),
				FTDI_SIO_SET_DATA_REQUEST,
				FTDI_SIO_SET_DATA_REQUEST_TYPE,
				urb_value , 0,
				buf, 0, HZ*10) < 0) {
		err("%s FAILED to set databits/stopbits/parity", __FUNCTION__);
	}

	if (usb_control_msg(dev->udev, usb_sndctrlpipe(dev->udev, 0),
				FTDI_SIO_SET_FLOW_CTRL_REQUEST,
				FTDI_SIO_SET_FLOW_CTRL_REQUEST_TYPE,
				0, 0,
				buf, 0, HZ*10) < 0) {
		err("%s error from disable flowcontrol urb", __FUNCTION__);
	}

	ftdi_usb_set_speed(dev);

	return 0;
}

static void ftdi_usb_set_break(struct usb_ftdi2dmx* dev, int break_state)
{
	__u16 urb_value = FTDI_SIO_SET_DATA_STOP_BITS_2 | FTDI_SIO_SET_DATA_PARITY_NONE | 8;

	char buf[2];

	if (break_state) {
		urb_value |= FTDI_SIO_SET_BREAK;
	}

	if (usb_control_msg(dev->udev, usb_sndctrlpipe(dev->udev, 0),
				FTDI_SIO_SET_DATA_REQUEST,
				FTDI_SIO_SET_DATA_REQUEST_TYPE,
				urb_value , 0,
				buf, 2, HZ*10) < 0) {
		err("%s FAILED to enable/disable break state (state was %d)", __FUNCTION__,break_state);
	}
}




/*
 * Some data structures for FTDI2DMX declaration.
 *
 */
//static void *ftdi2dmx_probe (struct usb_device *dev, unsigned int ifnum, const struct usb_device_id *id /* since 2.4.2 */);
//static void ftdi2dmx_disconnect(struct usb_device *dev, void *ptr);
static int ftdi2dmx_probe(struct usb_interface *interface, const struct usb_device_id *id);
static void ftdi2dmx_disconnect(struct usb_interface *interface);

static struct usb_device_id ftdi2dmx_id_table [] =
{
  { USB_DEVICE(FTDI_VID, FTDI_DMX512_PID),       driver_info: 0 },  /* FTDI2DMX-1 */
  { USB_DEVICE(FTDI_VID, ENTTEC_OPENDMXUSB_PID), driver_info: 0 },  /* OpenUSB-DMX */
  { }                                         /* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, ftdi2dmx_id_table);


static struct usb_driver dmx_driver =
{
  .name =           "FTDI2DMX",
  .probe =          ftdi2dmx_probe,
  .disconnect =     ftdi2dmx_disconnect,
  .id_table =       ftdi2dmx_id_table,
/*
  struct semaphore serialize;
  void (*suspend)(struct usb_device *dev);
  void (*resume)(struct usb_device *dev);
*/
};





/* ftdi2dmx_probe
 *
 * Is called after a USB device has been added
 * to the bus and check wether it is a FTDI2DMX.
 * If so, it initializes it and creates as much
 * DMX universes as the interface provides.
 */
//static void *ftdi2dmx_probe (struct usb_device *dev, unsigned int ifnum,
//			    const struct usb_device_id *id /* since 2.4.2 */
//)
static int ftdi2dmx_probe(struct usb_interface *interface, const struct usb_device_id *id)

{
  DMXDriver *drv = NULL;
  struct usb_ftdi2dmx *dev = NULL;
  struct usb_host_interface *iface_desc;
  struct usb_endpoint_descriptor *endpoint;
  //size_t buffer_size;
  int i;
  int retval = -ENOMEM;

  //dbg("ftdi2dmx_probe (struct usb_device *dev=0x%p, unsigned int ifnum=%u, const struct usb_device_id *id=0x%p" _FUNCFRAME_, dev, ifnum, id, FUNCFRAME_(ftdi2dmx_probe));

  /* allocate memory for our device state and initialize it */
  dev = kmalloc(sizeof(*dev), GFP_KERNEL);
  if (dev == NULL) {
    err("Out of memory");
    goto error;
  }
  memset(dev, 0x00, sizeof(*dev));
  kref_init(&dev->kref);

  dev->udev = usb_get_dev(interface_to_usbdev(interface));
  dev->interface = interface;

  /* set up the endpoint information */
  /* use only the first bulk-in and bulk-out endpoints */
  iface_desc = interface->cur_altsetting;
  for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
    endpoint = &iface_desc->endpoint[i].desc;

    if (!dev->bulk_in_endpointAddr &&
      (endpoint->bEndpointAddress & USB_DIR_IN) &&
      ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
                          == USB_ENDPOINT_XFER_BULK)) {
        /* we found a bulk in endpoint */
        //buffer_size = le16_to_cpu(endpoint->wMaxPacketSize);
        //dev->bulk_in_size = buffer_size;
        dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
        //dev->bulk_in_buffer = kmalloc(buffer_size, GFP_KERNEL);
        //if (!dev->bulk_in_buffer) {
        //  err("Could not allocate bulk_in_buffer");
        //  goto error;
        //}
    }

    if (!dev->bulk_out_endpointAddr &&
      !(endpoint->bEndpointAddress & USB_DIR_IN) &&
      ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
                          == USB_ENDPOINT_XFER_BULK)) {
        /* we found a bulk out endpoint */
        dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
    }
  }
  if (!(dev->bulk_in_endpointAddr && dev->bulk_out_endpointAddr)) {
    err("Could not find both bulk-in and bulk-out endpoints");
    goto error;
  }

  /*
   * Check if it is a known FTDI2DMX release.
   */
  if (dev->udev->descriptor.idVendor == FTDI_VID)
    {
      if (dev->udev->descriptor.idProduct != FTDI_DMX512_PID
	  && dev->udev->descriptor.idProduct != ENTTEC_OPENDMXUSB_PID)
	{
	  warn("FTDI product=%x not supported(vendor=%x)", dev->udev->descriptor.idProduct, dev->udev->descriptor.idVendor);
	  retval =  -ENODEV;
          goto error;
	}
    }
  else
    {
      retval = -ENODEV;
      goto error;
    }

  info("FTDI2DMX found at address %d", dev->udev->devnum);

  /*
   * Create a FTDI2DMX specific record and a DMX universe.
   */
  drv = dmx_find_driver (ftdi2dmx_family, "ftdi2dmx");
  if (drv)
    {
      DMXInterface *dmx_if = drv->create_interface (drv, dmxproplist_vacreate ("usbdev=%l", (long)dev));
      if (dmx_if && dmx_if->user_data)
	{
	  DMXUniverse  *uout = dmx_if->create_universe (dmx_if, 0, NULL); /* output */
	  DMXUniverse  *uin  = dmx_if->create_universe (dmx_if, 1, NULL); /* input */
	  if (uout && uin)
	    {
	      dbg("input/output-universes created, usb.private=%08lX", (unsigned long)dmx_if);
              /* save our data pointer in this interface device */
              usb_set_intfdata(interface, dev);

              /* let the user know what node this device is now attached to */
              info("USB Skeleton device now attached to USBSkel-%d", interface->minor);
	      return 0;
	    }
	  dmx_if->delete(dmx_if);
	}
    }
  else
    err("unable to find driver for dmx-family ftdi2dmx.ftdi2dmx");

  /*
   * Something failed.
   */
error:
  if (dev)
    kref_put(&dev->kref, ftdi2dmx_delete);
  return retval;
}



/*
 *  disconnect method for ftdi2dmx.
 *
 * It is called after a FTDI2DMX device has been
 * removed from the USB bus.
 */
//static void ftdi2dmx_disconnect(struct usb_device *dev, void *ptr)
static void ftdi2dmx_disconnect(struct usb_interface *interface)
{
  struct usb_ftdi2dmx *dev;
  int minor = interface->minor;
  //DMXInterface *dmx;

  /* prevent skel_open() from racing skel_disconnect() */
  lock_kernel();

  dev = usb_get_intfdata(interface);

  //dmx = &dev.dmxinterface;
      //dbg("ftdi2dmx_disconnect(struct usb_device *dev=0x%p, void *ptr=0x%p" _FUNCFRAME_, dev, ptr, FUNCFRAME_(ftdi2dmx_disconnect));
      if (dev->dmxinterface)
	{
	  struct usb_ftdi2dmx *u2d_if = (struct usb_ftdi2dmx *)dev->dmxinterface->user_data;

#if 0
	  wait(dmx->sem); /* wait for the interface to be unused and block it (forever :-) */
#endif
	  info("deleting ftdi2dmx interface");

	  ftdi2dmx_thread_cleanup(u2d_if);

	  if (dev->dmxinterface && dev->dmxinterface->delete)
	    dev->dmxinterface->delete (dev->dmxinterface);
          /* does a cascaded delete on the universes of that interface */
	}

  usb_set_intfdata(interface, NULL);

  unlock_kernel();

  /* decrement our usage count */
  kref_put(&dev->kref, ftdi2dmx_delete);

  info("USB Skeleton #%d now disconnected", minor);

  dbg("after ftdi2dmx_disconnect");
}

/*
 * ftdi2dmx_init
 *
 */
static int __init ftdi2dmx_init(void)
{
  DMXDriver *drv = NULL;
  int result;

  ftdi2dmx_family = dmx_create_family("USB");
  if (!ftdi2dmx_family)
    {
      err("unable to register dmx-family FTDI2DMX");
      return -1;
    }

  drv = ftdi2dmx_family->create_driver (ftdi2dmx_family, "ftdi2dmx", ftdi2dmx_create_universe, NULL);
  if (!drv)
    {
      ftdi2dmx_family->delete(ftdi2dmx_family, 0);
      err("failed to create ftdi2dmx driver");
      return -1;
    }
  drv->num_out_universes = 1;
  drv->num_in_universes = 1;
  drv->user_create_interface = ftdi2dmx_create_interface;

  result = usb_register(&dmx_driver);
  if ( result )
    {
      ftdi2dmx_family->delete(ftdi2dmx_family, 0);
      err("failed to usb_register FTDI2DMX. Error : %d", result);
      return result;
    }

  info("FTDI2DMX registered.");
  return 0;
}


/*
 * ftdi2dmx_cleanup
 *
 */
static void __exit ftdi2dmx_cleanup(void)
{
  usb_deregister(&dmx_driver);
  mdelay(5);
  if (ftdi2dmx_family)
    ftdi2dmx_family->delete(ftdi2dmx_family, 0);
}

module_init(ftdi2dmx_init);
module_exit(ftdi2dmx_cleanup);

