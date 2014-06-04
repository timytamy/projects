/*
 * dmxpcp.c
 * driver for soundlight DMXPCP
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

/*
 * set sceduling policy to fifo. (Realtime sceduling)
 */

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
#include <linux/config.h>
#endif

#if (!defined(CONFIG_PARPORT)) && (!defined(CONFIG_PARPORT_MODULE))
#error Linux Kernel needs Parport support for the dmxpcp interface
#endif

#include <linux/init.h>
#include <linux/module.h>
#include <linux/parport.h>
#include <linux/delay.h>

#include <dmx/dmxdev.h>

#define DEBUG
//#undef  DEBUG

#define ONDEBUG(arg...)

MODULE_AUTHOR("(c) 2001 Michael Stickel <michael@cubic.org> http://llg.cubic.org");
MODULE_DESCRIPTION("Generic Parport Driver version " DMXVERSION);
MODULE_LICENSE("GPL");

typedef struct struct_PCPInterface PCPInterface;
typedef struct struct_PCPUniverse PCPUniverse;


struct struct_PCPInterface
{
  DMXInterface      *interface;
  PCPUniverse   *ppuniverse;

  struct pardevice  *pdev;
  int                parport;

  void              *user_data;

  long               timeout; /* in jiffies */

  struct {
    unsigned char values:1;
    unsigned char slots:1;
    unsigned char p_update:1;
    unsigned char num_slots:1;
    unsigned char startcode:1;
    unsigned char breaktime:1;
    unsigned char mabtime:1;
    unsigned char error:1;

    unsigned int start;
    unsigned int end;
  } modified;

  int error;

  struct
    {
      wait_queue_head_t  waitqueue; /* wake(...) will wake up the worker thread for that interface */
      int                pid;       /* pid of the worker thread */
      int                running;   /* is the worker thread running ? */
    } thread;

  struct
    {
      char     *buffer;
      size_t    index;
      size_t    size;
    } active_buffer;


  void    (*reset_interface) (PCPInterface *);
  size_t  (*prepare_buffer)  (PCPInterface *, unsigned char *buffer, size_t maxsize);
  void    (*timed_out)       (PCPInterface *, long timeout);


  /* called by the transfer thread to initiate the transfer of the buffer */
  int     (*send_buffer)     (PCPInterface *, unsigned char *buffer, size_t size);

  /* called by the buffer transfer routine to signal the thread that the buffer is no longer
   * used and can be freed or can otherwise be used
   */
  int     (*buffer_send)     (PCPInterface *, unsigned char *buffer, size_t size);

  /* called by the parport interrupt */
  void    (*interrupt)       (PCPInterface *);  /* used if another than the standard transfer scheme is used */

};





struct struct_PCPUniverse
{
/* --- Parameters (public members) ---- */
/* These member pointers points to the
 * Variables that holds the real values,
 * so they can be either universe or
 * interface dependen. Some interfaces
 * have independent values for different universes
 * on the same interface and some values
 * are for all universes on the same interface.
 * These pointers are filled with NULL and have
 * to be set by the creation function of the
 * parallel port driver.
 */
  int                *breaktime; /* in uS */
  int                *mabtime;   /* in uS */
  int                *numslots;
  unsigned char      *startcode;
  long               *frames;

  unsigned char     buffer[512];  /* this is the dmx-buffer */

  DMXUniverse       *universe;
  PCPInterface   *card;
};


/*---- DMX Transfer  ------------------------------*/

/*
 * This function is responsable for sending the next byte in SPP mode.
 */
static void dmxpcp_send_next_byte (PCPInterface *pif)
{
  if (pif && pif->active_buffer.buffer)
    {
      if (pif->active_buffer.index < pif->active_buffer.size)
	{
	  /* write out the next byte to the interface */
	  /* write data in SPP-mode */
	  if (pif->pdev && pif->pdev->port)
	    {
	      struct parport *pport = pif->pdev->port;

	      parport_write_data (pport, pif->active_buffer.buffer[pif->active_buffer.index++]);
	      udelay(8);
	      parport_write_control (pport, PARPORT_CONTROL_STROBE | PARPORT_CONTROL_INIT);
	      udelay(4);
	      parport_write_control (pport, PARPORT_CONTROL_INIT);
	    }
	  else
	    { /* wake up because of error */
              pif->modified.error=1;
              pif->error = 1;
	      wake_up_interruptible (&pif->thread.waitqueue);
	    }
	}
      else
	{
#if 0
          pif->buffer_send (pif, pif->active_buffer.buffer, pif->active_buffer.size);
#endif
	  pif->modified.error=1;
	  pif->error = 0; /* no error => finished */
	  wake_up_interruptible (&pif->thread.waitqueue);
	}
    }
  else
    { /* wake up because of error */
      printk (KERN_INFO "dmxpcp_send_next_byte: FATAL: invalid PCPInterface\n");
      pif->modified.error=1;
      pif->error = 2;
      wake_up_interruptible (&pif->thread.waitqueue);
    }
}



static int dmxpcp_thread(void *userdata)
{
  PCPInterface *pif = (PCPInterface *)userdata;
  char buffer[2048];


  printk (KERN_INFO "dmxpcp started\n");

  if (!pif)
    {
      printk (KERN_INFO "dmxpcp exiting (pif==NULL)\n");
      return 0;
    }

  pif->thread.running = 1;
  printk ("dmxpcp running\n");


/*  lock_kernel(); */
/*  daemonize(); */

  parport_enable_irq(pif->pdev->port);


  /* Setup a nice name */
  strcpy(current->comm, "dmxpcpd");

  ONDEBUG(printk (KERN_INFO "dmxpcp resetting interface\n"));

  pif->reset_interface (pif);

  /* set 512 slots */
  buffer[0] = 27;
  buffer[1] = 67;
  buffer[2] = 2;
  buffer[3] = 0;
  pif->send_buffer (pif, buffer, 4);

  /* Send me a signal to get me die (for debugging) */
  do
    {
      if (pif->active_buffer.buffer==NULL)
        {
          ONDEBUG(printk (KERN_INFO "modified={%s%s%s%s%s%s%s%s}, start=%d, end=%d\n",
                  pif->modified.values?"values ":"",
                  pif->modified.slots?"slots ":"",
                  pif->modified.p_update?"p_update ":"",
                  pif->modified.num_slots?"num_slots ":"",
                  pif->modified.startcode?"startcode ":"",
                  pif->modified.breaktime?"breaktime ":"",
                  pif->modified.mabtime?"mabtime ":"",
                  pif->modified.error?"error ":"",
                  pif->modified.start,
                  pif->modified.end));

          if (pif->modified.values || pif->modified.slots)
            {
	      size_t size = pif->prepare_buffer (pif, buffer, sizeof(buffer));
              if (size > 0)
                {
                  ONDEBUG(printk (KERN_INFO "dmxpcpd: sending buffer, size=%d\n", size));

                  pif->send_buffer (pif, buffer, size);
                }
	      else
		printk (KERN_INFO "ups! buffer-size = %d\n", size);

	      pif->modified.start  = 0;
	      pif->modified.end    = 0;
              pif->modified.slots  = 0;
	      pif->modified.values = 0;
            }
        }


      if (pif->active_buffer.buffer || (!pif->modified.values && !pif->modified.slots))
        {
          /*int status = 0;*/

          ONDEBUG(printk (KERN_INFO "dmxpcpd: going to sleep\n"));

          if (!interruptible_sleep_on_timeout(&pif->thread.waitqueue, HZ/50))
            {
              ONDEBUG(printk (KERN_INFO "dmxpcpd: awaken (timeout)\n"));

	      if (pif->active_buffer.buffer && pif->modified.values)
		{
		  printk (KERN_INFO "request while transfer - initiate send_next_byte\n");
		  dmxpcp_send_next_byte (pif);
		}
            }
	  else
	    {
	      if (pif->modified.error)
		{
		  if (!pif->error)
		    printk (KERN_INFO "parportd: transfer is finished\n");
		  else
		    printk (KERN_INFO "parportd: awaken in cause of error %d\n", pif->error);
		}
	    }
        }
    } while (!signal_pending(current));

  if (pif->pdev && pif->pdev->port)
    {
      struct parport *pport = pif->pdev->port;
      if (pport->ops && pport->ops->disable_irq)
	pport->ops->disable_irq(pport);
    }
  else
    printk (KERN_INFO "dmxpcp: failed to disable interupt");

  pif->buffer_send(pif, NULL, 0);

  printk (KERN_INFO "dmxpcp-thread exiting");
  pif->thread.running = 0;

  return 0;
}




/*
 * called to start the thread for that interface
 */
int dmxpcp_startthread (PCPInterface *pif)
{
  int pid = kernel_thread(dmxpcp_thread, (void *)pif, CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
  if (pid >= 0)
    {
      pif->thread.pid = pid;
      printk (KERN_INFO "pid for dmxpcp is %d\n", pid);
      return 0;
    }
  return -1;
}

/*
 * called to stop the thread for that interface
 */
void dmxpcp_stopthread (PCPInterface *pif)
{
  if (pif && pif->thread.pid>2)
    {
      int ret;

      printk (KERN_INFO "attempting to kill dmxpcp-thread with id %d\n", pif->thread.pid);

      ret = kill_proc(pif->thread.pid, SIGTERM, 0);
      if (!ret)
	{
	  /* Wait 10 seconds */
	  int count = 10 * 100;

	  while (pif->thread.running && --count)
	    {
	      current->state = TASK_INTERRUPTIBLE;
	      schedule_timeout(1);
	    }
	  if (!count)
	    printk (KERN_INFO "giving up on killing dmxpcp-thread");
	}
    }
}




/*
 * This method is called before the universe will be deleted
 */
static int dmxpcp_delete_universe (DMXUniverse *u)
{
  printk (KERN_INFO "dmxpcp_delete_universe (%p)\n", u);

  if (u && u->user_data)
    {
      PCPUniverse *dmxu = (PCPUniverse *)u->user_data;
      if (dmxu)
	{
	  printk (KERN_INFO "dmxpcp_delete_universe: try free PCPUniverse\n");
	  DMX_FREE(dmxu);
	  u->user_data = NULL;
	  printk (KERN_INFO "dmxpcp_delete_universe: PCPUniverse freed\n");
	}
    }
  return 0;
}





static int dmxpcp_delete_interface (DMXInterface *i)
{
  printk (KERN_INFO "dmxpcp_delete_interface (%p)\n", i);
  if (i && i->user_data)
    {
      PCPInterface *dmxif = (PCPInterface *)i->user_data;

      dmxpcp_stopthread (dmxif);

      if (dmxif->pdev)
	{
	  parport_release (dmxif->pdev);
	  parport_unregister_device(dmxif->pdev);
	}
      printk (KERN_INFO "dmxpcp_delete_interface: try free PCPInterface\n");
      DMX_FREE(dmxif);
      i->user_data = NULL;
      printk (KERN_INFO "dmxpcp_delete_interface: PCPInterface freed\n");
      return 0;
    }
  return -1;
}



/*
 * ----------- Universe Access ----------------------------
 */


/*
 * This method is called by the dmxdev module
 * if slots have been changed by the userspace
 * for e.g. by a write to /dev/dmx.
 */
static int  dmxpcp_write_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  ONDEBUG(printk (KERN_INFO "dmxpcp_write_universe\n"));

  if (u && u->user_data && buff && offs >= 0  && size > 0 && offs+size <= 512)
    {
      PCPUniverse  *lu = (PCPUniverse *)u->user_data;
      PCPInterface *pif = lu->card;

      if(pif)
        {
          ONDEBUG(printk (KERN_INFO "dmxpcp_write_universe: universe and interface OK\n"));

          memcpy ((lu->buffer)+offs, (void *)buff, size);
          if (pif->modified.values)
            {
              if (offs<pif->modified.start)
                pif->modified.start = offs;

              if (offs+size>pif->modified.end)
                pif->modified.end = offs+size;
            }
          else
            {
              pif->modified.values = 1; /* channel values have been modified */
              pif->modified.start = offs;
              pif->modified.end = offs+size;
              ONDEBUG(printk (KERN_INFO "dmxpcp_write_universe: wrote values\n"));
            }
          ONDEBUG(printk (KERN_INFO "dmxpcp_write_universe: wake up worker\n"));
          wake_up_interruptible(&pif->thread.waitqueue); /* data has been modified */
        }
#if 0
      printk (KERN_INFO "dmxpcp_write_universe: signal_changed\n");
      u->signal_changed (u, offs, size);
#endif
      ONDEBUG(printk (KERN_INFO "dmxpcp_write_universe: successfull\n"));
      return size;
    }

  printk (KERN_INFO "dmxpcp_write_universe: illegal parameters\n");

  return -EINVAL;
}






/*----------------------------------------------------------
 * Default functions for non-continous transfers to interface
 */

static int  default_sendbuffer (PCPInterface *pif, unsigned char *buffer, size_t size)
{
  if (pif && buffer && size>0 && !pif->active_buffer.buffer)
    {
      pif->active_buffer.buffer = buffer;
      pif->active_buffer.index  = 0;
      pif->active_buffer.size   = size;
      dmxpcp_send_next_byte (pif);
      return 0;
    }
  return -1;
}
static int default_buffer_send (PCPInterface *pif, unsigned char *buffer, size_t size)
{
  if (pif && buffer && size>0)
    {
      pif->active_buffer.buffer = NULL;
      return 0;
    }
  return -1;
}



/*
 * This method is called after the universe is created
 * but before it will be inserted.
 */
static int dmxpcp_create_universe (DMXUniverse *u, DMXPropList *pl)
{
  printk ("dmxpcp_create_universe (%p, %p)\n", u, pl);

  if (u && u->interface && u->interface->user_data)
    {
      PCPInterface *pif = (PCPInterface *)(u->interface->user_data);
      if (pif)
	{
	  PCPUniverse  *ppu = DMX_ALLOC(PCPUniverse);

	  if (ppu)
	    {
	      int  i;
	      for (i=0; i<512; i++)
		ppu->buffer[i] = 0;

	      u->user_data  = ppu;
	      ppu->universe = u;
	      ppu->card     = pif;
	      pif->ppuniverse = ppu;


	      u->write_slots = dmxpcp_write_universe;

	      strcpy (u->connector, "first");
	      u->conn_id = 0;

	      u->user_delete = dmxpcp_delete_universe;
	      return 0;
	    }
	  else
	    printk (KERN_INFO "dmxpcp_create_universe:ppu=NULL");
	}
      else
	printk (KERN_INFO "dmxpcp_create_universe: interface->user_data == NULL - no parport interface\n");
    }
  else
    printk (KERN_INFO "dmxpcp_create_universe:u=%p, u->interface=%p, u->interface->user_data=%p\n", u, u?u->interface:NULL, (u && u->interface)?u->interface->user_data:NULL);
  return -1;
}


/*
 *  reset the interface.
 */
static void default_reset_interface (PCPInterface *pif)
{
  if (pif && pif->pdev)
    {
      struct parport *port = pif->pdev->port;

      parport_write_control (port, /* parport_read_control (port) & ~4 */  0);
      udelay(10);
      parport_write_control (port, /* parport_read_control (port) | 4*/ PARPORT_CONTROL_INIT);
    }
}

/*------------------------------------------------------
 * Soundlight SLH2512A specific functions
 */


static size_t slh2512a_prepare_buffer (PCPInterface *pif, unsigned char *buffer, size_t maxsize)
{
  size_t index = 0;

  ONDEBUG(printk("slh2512a_prepare_buffer called\n"));

  if (pif && pif->ppuniverse)
    {
      PCPUniverse *ppu = pif->ppuniverse;

       if (pif->modified.values)
	{
	  size_t start = pif->modified.start;
	  size_t slots = pif->modified.end-start /* +1 */; /* there is a +1 bug in write_universe */
	  size_t offset = 0;

          ONDEBUG(printk ("slh2512a_prepare_buffer: write_slots(start=%d, size=%d)\n", start, slots));

	  for (offset=start; slots>0; offset+=256)
	    {
	      unsigned char *ubuffer = ppu->buffer;
	      int len  = (slots > 256)?256:slots;
	      int i;

	      buffer[index++] = 27;
	      buffer[index++] = 'D';
	      buffer[index++] = (offset>>8)&0xff;
	      buffer[index++] = offset&0xff;
	      buffer[index++] = (len & 0xff); /* 1..255, 0=256 */
	      for (i=0; i<len; i++)
		buffer[index++] = ubuffer[offset+i];

	      ONDEBUG(printk("prepared %d slots starting at %d\n", len, offset));
	      slots -= len;
	    }
	    buffer[index++] = 27;
	    buffer[index++] = 'G';

          pif->modified.values = 0;
          pif->modified.start = 0;
          pif->modified.end = 0;
        }

      if (pif->modified.slots && 4 < maxsize)
        {
          size_t slots = ppu->numslots?(*ppu->numslots):512;
          ONDEBUG(printk ("slh2512a_prepare_buffer: write_slotscount\n"));
          buffer[index++] = 27;
          buffer[index++] = 'C';
          buffer[index++] = (slots>>8)&0xff;
          buffer[index++] = slots&0xff;
          pif->modified.slots = 0;
        }
    }
  else
    printk (KERN_INFO "pif=%p, pif->ppuniverse=%p\n", pif, pif?pif->ppuniverse:NULL);
  return index;
}


static void slh2512a_timed_out   (PCPInterface *pif, long timeout)
{
  if (pif)
    {
      unsigned char *buff = pif->active_buffer.buffer;
      size_t         size = pif->active_buffer.size;

      printk("slh2512a: transfer timed out\n");

      pif->reset_interface(pif);
      pif->buffer_send (pif, buff, size);
      pif->send_buffer (pif, buff, size);
    }
}


static int slh2512a_create_universe (DMXUniverse *u, DMXPropList *pl)
{
  return dmxpcp_create_universe (u, pl);
#if 0
  if (u)
    {
      PCPUniverse *ppu = (PCPUniverse *)u->user_data;
      if (ppu && ppu->card)
	{
	  ppu->card->timed_out       = slh2512a_timed_out;
	  ppu->card->prepare_buffer  = slh2512a_prepare_buffer;
	  ppu->card->delete_universe = slh2512a_delete_universe;
	  if (dmxpcp_create_universe (u, pl) >= 0)
	    {
	      return 0;
	    }
	}
    }
  return -1;
#endif
}

static void default_interrupt_handler (PCPInterface *pif)
{
  /*static int irqcnt=0;*/

  if (pif && pif->active_buffer.buffer && pif->active_buffer.index < pif->active_buffer.size)
    dmxpcp_send_next_byte (pif);
  else
    pif->buffer_send(pif, pif->active_buffer.buffer, pif->active_buffer.size);
}



/* dmxpcp_interface_new
 * creates a new PCPInterface instance and fills
 * it with deault values
 */
static PCPInterface *dmxpcp_interface_new (void)
{
  PCPInterface *pif = DMX_ALLOC(PCPInterface);
  if (pif)
    {
      pif->interface  = NULL;
      pif->ppuniverse = NULL;
      pif->pdev       = NULL;
      pif->parport    = 0;
      pif->user_data  = NULL;
      pif->timeout    = HZ; /* 1sec in jiffies */

      pif->error = 0;

      pif->modified.values    = 0;
      pif->modified.slots     = 0;
      pif->modified.p_update  = 0;
      pif->modified.num_slots = 0;
      pif->modified.startcode = 0;
      pif->modified.breaktime = 0;
      pif->modified.mabtime   = 0;
      pif->modified.error     = 0;

      pif->modified.start = -1;
      pif->modified.end   = -1;


      init_waitqueue_head(&pif->thread.waitqueue);
      pif->thread.pid     = -1;  /* pid of the worker thread */
      pif->thread.running = 0;   /* is the worker thread running ? */

      pif->active_buffer.buffer = NULL;
      pif->active_buffer.index  = 0;
      pif->active_buffer.size   = 0;


      pif->reset_interface = default_reset_interface;
      pif->prepare_buffer  = slh2512a_prepare_buffer;
      pif->timed_out       = slh2512a_timed_out;

       /* called by the transfer thread to initiate the transfer of the buffer */
      pif->send_buffer = default_sendbuffer;

       /* called by the buffer transfer routine to signal the thread that the buffer is no longer
        * used and can be freed or can otherwise be used
        */
      pif->buffer_send = default_buffer_send;

       /* called by the parport interrupt */
      pif->interrupt = default_interrupt_handler;
    }
  return pif;
}


/*
 * dmxpcp interrupt function
 */
static void dmxpcp_interrupt (int irq, void *user)
{
  if (user)
    {
      PCPInterface *pi = (PCPInterface *)user;
      if (pi->interrupt)
        pi->interrupt (pi);
    }
  else
    printk (KERN_INFO "FATAL: user pointer is null in interrupt\n");
}










/*=======[ insertion / deletion of module ]==========*/

/*
 * ----------------------- Properties --------------------------------
 */



/*
 * parport setter/getter methods
 */
static int parport_get_long (DMXProperty *p, long *val)
{ /* interface */
#if 0
  PCPUniverse *ppu = (PCPUniverse *)p->data;
  if (ppu && val && ppu->parport)
    {
      *val = (long)*ppu->parport;
      return -1;
    }
#endif
  return 0;
}

static int parport_set_long (DMXProperty *p, long val)
{ /* interface */
#if 0
  PCPUniverse *ppu = (PCPUniverse *)p->data;
  if (ppu && ppu->parport)
    *ppu->parport = (int)val;
#endif
  return 0;
}


/* ----------------------------
 * slots setter/getter methods
 */
static int slots_get_long (DMXProperty *p, long *val)
{ /* interface */
  PCPUniverse *ppu = (PCPUniverse *)p->data;
  if (ppu && val && ppu->numslots)
    {
      *val = (long)*ppu->numslots;
      return -1;
    }
  return 0;
}

static int slots_set_long (DMXProperty *p, long val)
{ /* interface */
  PCPUniverse *ppu = (PCPUniverse *)p->data;
  if (ppu && ppu->numslots)
    {
      *ppu->numslots = (int)val;
      /* tell it the thread */
    }
  return 0;
}


/* ----------------------------
 * mabsize setter/getter methods
 */
static int mabsize_get_long (DMXProperty *p, long *val)
{ /* interface */
  PCPUniverse *ppu = (PCPUniverse *)p->data;
  if (ppu && val && ppu->mabtime)
    {
      *val = (long)*ppu->mabtime;
      return -1;
    }
  return 0;
}

static int mabsize_set_long (DMXProperty *p, long val)
{ /* interface */
  PCPUniverse *ppu = (PCPUniverse *)p->data;
  if (ppu && ppu->mabtime)
    {
      *ppu->mabtime = (char)val;
      /* tell it the thread */
    }
  return 0;
}

/* ----------------------------
 * startcode setter/getter methods
 */
static int startcode_get_long (DMXProperty *p, long *val)
{ /* universe */
  PCPUniverse *ppu = (PCPUniverse *)p->data;
  if (ppu && val && ppu->startcode)
    {
      *val = (long)*ppu->startcode;
      return -1;
    }
  return 0;
}

static int startcode_set_long (DMXProperty *p, long val)
{ /* universe */
  PCPUniverse *ppu = (PCPUniverse *)p->data;
  if (ppu && ppu->startcode)
    *ppu->startcode = (unsigned char)val;
  return 0;
}

/* ----------------------------
 * startcode setter/getter methods
 */
static int frames_get_long (DMXProperty *p, long *val)
{ /* universe */
  PCPUniverse *ppu = (PCPUniverse *)p->data;
  if (ppu && val && ppu->frames)
    {
      *val = *ppu->frames;
      return -1;
    }
  return 0;
}

static int frames_set_long (DMXProperty *p, long val)
{
  /* read only */
  return 0;
}



/*
 * ------------------- instance creating / deletion ---------------------------
 */


static int dmxpcp_create_interface (DMXInterface *i, DMXPropList *pl)
{
  printk (KERN_INFO "dmxpcp_create_interface (%p, %p)\n", i, pl);
  if (i)
    {
      PCPInterface *intr = dmxpcp_interface_new ();
      if (intr)
	{
	  long parport = 0L;
	  struct parport *pport = NULL;

	  i->user_data = (void *)intr;

	  if (pl && pl->find)
	    {
	      DMXProperty *p = pl->find(pl, "parport");
	      if (!p) pl->add(pl, dmxprop_create_long("parport", 0L));
	      if (p)
		{
		  p->get_long (p, &parport);
		  dmxprop_user_long (p, parport_get_long, parport_set_long, (void *)i);
		}
	      else
		printk (KERN_INFO "failed to create property parport\n");

	      if (parport < 0)
		return -1;

	      printk (KERN_INFO "dmxpcp_create_interface: looking for parport%ld\n", parport);

	      pport = parport_find_number(parport);
	      if (pport)
		{
		  if (pport->irq!=-1)
		    {
		      struct pardevice *newpdev;

		      printk (KERN_INFO "dmxpcp_create_interface: found parport%ld\n", parport);

		      newpdev = parport_register_device (pport, "dmxpcp",
							 NULL,NULL,dmxpcp_interrupt,0,(void *)intr);
		      if (!newpdev)
			{
			  printk (KERN_INFO "failed to get access to parport\n");
			  return -1;
			}

		      printk (KERN_INFO "got parport%ld for dmx-device\n", parport);

		      intr->pdev = newpdev;

		      if (parport_claim (newpdev)==0)
			{
			  if ((p = pl->find(pl, "slots")) == NULL)
			    pl->add(pl, dmxprop_create_long("slots", 0L));
			  if (p)
			    dmxprop_user_long (p, slots_get_long, slots_set_long, (void *)i);

			  if ((p = pl->find(pl, "mabsize")) == NULL)
			    pl->add(pl, dmxprop_create_long("mabsize", 0L));
			  if (p)
			    dmxprop_user_long (p, mabsize_get_long, mabsize_set_long, (void *)i);

			  if ((p = pl->find(pl, "frames")) == NULL)
			    pl->add(pl, dmxprop_create_long("frames", 0L));
			  if (p)
			    dmxprop_user_long (p, frames_get_long, frames_set_long, (void *)i);

			  if ((p = pl->find(pl, "startcode")) == NULL)
			    pl->add(pl, dmxprop_create_long("startcode", 0L));
			  if (p)
			    dmxprop_user_long (p, startcode_get_long, startcode_set_long, (void *)i);

			  /* create thread */


			  intr->interface = i;

			  intr->pdev = newpdev;
			  intr->parport = parport;

			  if (dmxpcp_startthread (intr) >= 0)
			    {
			      i->user_delete = dmxpcp_delete_interface;
			      return 0;
			    }
			  printk (KERN_INFO "error createing thread\n");
			}
		      else
			printk (KERN_INFO "unable to claim parport%ld. may be locked by another driver?\n", parport);
		      parport_unregister_device(newpdev);
		    }
		  else
		    {
		      printk (KERN_ERR "There is no IRQ assigned to parport%ld.\n", parport);
		      printk (KERN_ERR "Take a look at the documentation on how to assign an interrupt to the parport.\n");
		    }

		}
	    }
	  DMX_FREE(intr);
	}
    }
  return -1;
}








/*
 * --------- Module creation / deletion ---------------
 */

static int parport=0;
module_param(parport, int, S_IRUGO);
MODULE_PARM_DESC(parport,"parport number (0=parport0)");

static DMXFamily *family = NULL;

static int __init dmxpcp_init(void)
{
  DMXDriver *pcpdriver = NULL;

  family = dmx_create_family ("PAR");
  if (family)
    {
      pcpdriver=family->create_driver (family, "dmxpcp", slh2512a_create_universe, NULL);
      pcpdriver->user_create_interface = dmxpcp_create_interface;
      pcpdriver->num_out_universes = 1;

      if (pcpdriver)
        {
	  DMXInterface *dmxif = pcpdriver->create_interface (pcpdriver, dmxproplist_vacreate ("parport=%l", parport));
	  if (dmxif)
	    {
	      if(dmxif->create_universe (dmxif, 0, NULL))
		return 0;
	    }
        }
    }

  if (family)
    family->delete (family, 0);
  return -1;
}


static void __exit dmxpcp_exit(void)
{
  if (family)
    family->delete (family, 0);
}

module_init(dmxpcp_init);
module_exit(dmxpcp_exit);
