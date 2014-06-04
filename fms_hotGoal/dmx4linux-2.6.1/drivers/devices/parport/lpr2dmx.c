/*
 * lpr2dmx.c
 * A driver for the dmx-interface made by Jan Menzel.
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
 * TODO: double-buffering: 1 for prepare, 1 as active.
 * The interrupt sends one and the thread prepares
 * the other buffer. After transfer the buffer usage is swapped.
 */

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
#include <linux/config.h>
#endif

#if (!defined(CONFIG_PARPORT)) && (!defined(CONFIG_PARPORT_MODULE))
#error Linux Kernel needs Parport support for the lpr2dmx interface
#endif

#include <linux/init.h>
#include <linux/module.h>
#include <linux/parport.h>
#include <linux/delay.h>

#include <dmx/dmxdev.h>

#define LPR2DMX_USE_THREADS (1)

//#define DEBUG
#undef  DEBUG

#ifdef DEBUG
#define ONDEBUG(x...) (x)
#else
#define ONDEBUG(x...)
#endif

MODULE_AUTHOR("(c) 2001 Michael Stickel <michael@cubic.org> http://llg.cubic.org");
MODULE_DESCRIPTION("Driver for the lpr2dmx interfaces from Lighting Solutions (http://www.lightingsolutions.de) version " DMXVERSION);
MODULE_LICENSE("GPL");

typedef struct
{
  DMXInterface      *interface;

  struct pardevice  *pdev;
  int                parport;
  char               mabsize;
  char               saverdmx;
  int                slots;

  char               type;

  int                num_universes;

  unsigned char      startcode;

  long               frames;

  unsigned char     *xmit_buffer;
  unsigned int       xmit_size;
  int                xmit_error;

  char               watchdog_triggered;  /* set if the watchdog has been triggered */
  char               transfer_finished;   /* set by the interrupt after the transfer is finished */

  wait_queue_head_t  lpr2dmxd_wait;      /* wake(...) will wake up the worker thread for that interface */
  int                lpr2dmxd_pid;       /* pid of the worker thread */
  int                lpr2dmxd_running;   /* is the worker thread running ? */
} LPR2DMX_Interface;


typedef struct
{
  DMXUniverse       *universe;
  LPR2DMX_Interface *card;

  unsigned char buffer[512];  /* this is the dmx-buffer */

} LPR2DMX_Universe;


#define LPR2DMX1  (0x1)
#define LPR2DMX2  (0x4)
#define LPR2DMX3  (0xD)



/*
 *----------------- Autoidentification ----------------------------
 */

static void lpr2dmx_rb_strobe (struct pardevice *dev)
{
  parport_frob_control (dev->port, 1, 1);
  udelay (10);
  parport_frob_control (dev->port, 1, 0);
}



static unsigned char lpr2dmx_rb_getbyte (struct pardevice *dev)
{
  int            i = 7;
  unsigned char  c = 0;

  while (i-- >= 0)
    {
      udelay(2);
      lpr2dmx_rb_strobe (dev);
      udelay(8);
      c >>= 1;
      c |=  parport_read_status(dev->port) & 0x80;
    }

  return ~c;
}


/*
 *  reset the interface.
 */
static int lpr2dmx_rb_reset (struct pardevice *dev)
{
  if (dev)
    {
      parport_frob_control (dev->port, PARPORT_CONTROL_INIT, 0);
      udelay(10);
      parport_frob_control (dev->port, PARPORT_CONTROL_INIT, PARPORT_CONTROL_INIT);
      udelay(10);
      return 0;
    }
  return -1;
}



static void lpr2dmx_rb_init (struct pardevice *dev)
{
  lpr2dmx_rb_strobe (dev);
  lpr2dmx_rb_reset (dev);
  parport_write_data (dev->port, 0xff);
  lpr2dmx_rb_strobe (dev);
}

static unsigned short lpr2dmx_getid (struct pardevice *dev)
{
  unsigned short id;
  lpr2dmx_rb_init (dev);
  id = lpr2dmx_rb_getbyte (dev) << 8;
  id |= lpr2dmx_rb_getbyte (dev);
  lpr2dmx_rb_reset (dev);
  return id;
}

static int lpr2dmx_getinfo (struct pardevice *dev, char *str, size_t len)
{
  char c;
  unsigned short id;

  if (!str) return -1;

  lpr2dmx_rb_init (dev);
  id = lpr2dmx_rb_getbyte(dev) << 8;
  id |= lpr2dmx_rb_getbyte(dev);

  if (id!=0 && id!=0xffff)
    {
      while (--len>0 && (c = lpr2dmx_rb_getbyte(dev)))
        {
          if (c=='\r' || c=='\n')
            {
              *(str++) = ' ';
              *(str++) = '|';
              *(str++) = ' ';
            }
          else
            *(str++) = c;
        }
      *(str++) = 0;
    }
  else
    *str=0;

  return id;
}
/*-- End Of Autoident ------------------------------------*/


/*
 *  prepares the buffer.
 */
int lpr2dmx_prepare_buffer (LPR2DMX_Interface *card, unsigned char *buffer, size_t maxsize)
{
  if (card && card->interface)
    {
      if (card->interface->universes && buffer && maxsize >= 512+3)
      {
        DMXUniverse  *dmxu1 = card->interface->universes;
        DMXUniverse  *dmxu2 = dmxu1?dmxu1->next:NULL;

        unsigned char *buffer1 = (dmxu1 && dmxu1->user_data) ? ((LPR2DMX_Universe *)dmxu1->user_data)->buffer : NULL;
        unsigned char *buffer2 = (dmxu2 && dmxu2->user_data) ? ((LPR2DMX_Universe *)dmxu2->user_data)->buffer : NULL;

        int            slots     = card->slots;
        int            mabsize   = card->mabsize;
        char           saverdmx  = 0; /* card->saverdmx */
        unsigned char  startcode = card->startcode;


        if (!buffer1)
    return -1;

        /* build the buffer */
        buffer[0] = ((saverdmx>0)?0x80:0)
                  | ((mabsize <= 4)?0x40:0)
                  | (((slots-1) >> 8) & 1)
                  ;
        buffer[1] = (slots-1) & 0xff;

        card->xmit_buffer = buffer;
        card->xmit_size   = slots + 3;

        if (card && card->num_universes <= 0)
    return -1;

        if (!buffer1)
    return -1;

        if (card && card->num_universes > 1 && maxsize >= 2*512+3)
          {
      int i;
      if (!buffer2)
        return -1;

            buffer[0] |= 3<<4; /* Dual Headed Mode */
            buffer[2] = startcode; /* startcode 1. universe */
            buffer[3] = startcode; /* startcode 2. universes */

      for (i=0; i<slots; i++)
        {
          buffer[4 + i*2] = buffer1[i];
          buffer[5 + i*2] = buffer2[i];
        }
      card->xmit_size += slots+1;
      return card->xmit_size;
          }
        else
          {
            if (dmxu1->conn_id != 0)
              buffer[0] |= 1<<4; /* Use the second universe only */
#if 0
      if (card->type == LPR2DMX2)
              buffer[0] |= 2<<4; /* identical data on both outputs with one universe */
#endif

            buffer[2] = startcode;
            memcpy(&buffer[3], buffer1, slots);
            return card->xmit_size;
          }
        }
      return 0;
    }
  return -1;
}



/*---- DMX Transfer  ------------------------------*/

/*
 * This function is responsable for sending the next byte in SPP mode.
 */
static void lpr2dmx_send_next_byte (LPR2DMX_Interface *card, const unsigned short reset)
{
  if (card && card->num_universes > 0 && card->xmit_buffer)
    {
      card->xmit_error=0;
      if (card->xmit_size > 0)
	{
	  /* write out the next byte to the interface */
	  /* write data in SPP-mode */
	  if (card->pdev && card->pdev->port)
	    {
	      struct parport *pport = card->pdev->port;

	      if (reset==4711)
		lpr2dmx_rb_reset (card->pdev);

	      if(*(card->xmit_buffer) && card->xmit_size<0x400)
		{
		  ONDEBUG(printk("lpr2dmx_send_next_byte() %02X %02x\n", card->xmit_size, *(card->xmit_buffer)));
		}

	      parport_write_data (pport, *(card->xmit_buffer++));

	      parport_frob_control (pport, PARPORT_CONTROL_STROBE | PARPORT_CONTROL_INIT, PARPORT_CONTROL_STROBE | PARPORT_CONTROL_INIT);
	      udelay(1);
	      parport_frob_control (pport, PARPORT_CONTROL_STROBE | PARPORT_CONTROL_INIT, PARPORT_CONTROL_INIT);
	    }
	  else
	    { /* wake up because of error */
	      card->xmit_error = 1;
	      wake_up_interruptible (&card->lpr2dmxd_wait);
	    }
	  card->xmit_size--;
	}
      else
	{
	  card->frames++;
	  wake_up_interruptible (&card->lpr2dmxd_wait);
	}
    }
  else
    {
      printk (KERN_INFO "lpr2dmx_interrupt: FATAL: invalid LPR2DMX_Interface card=%p, num_universes=%i, xmit_buffer=%p\n", card, card?card->num_universes:-1, card?card->xmit_buffer:NULL);
    }
}


/*
 * This interrupt will be called in SPP mode to send the next byte.
 */
static void dmx_interrupt (int x, void *user)
{
  LPR2DMX_Interface *card = (LPR2DMX_Interface *)user;
  if (card && card->lpr2dmxd_running > 0)
    lpr2dmx_send_next_byte ( (LPR2DMX_Interface *)user, 0);
  else
    if (!card)
      printk (KERN_INFO "%s: FATAL: user pointer is null => watchdog may catch it\n", "lpr2dmx");
}




/*
 *  sends the buffer.
 */
int send_buffer (LPR2DMX_Interface *card)
{
  if (card)
    {
#if 0
      if (card->have_ecp)
  {
    if (card->have_dma)
      parport_fifo_write_block_dma (card->port, buffer, size);
    else
      parport_fifo_write_block_pio (card->port, buffer, size);
    return 0;
  }
#endif

  lpr2dmx_send_next_byte (card, 0);
    }
  return 0;
}



#ifdef LPR2DMX_USE_THREADS

static int dmx_lpr2dmx_thread(void *userdata)
{
  LPR2DMX_Interface *card = (LPR2DMX_Interface *)userdata;
  char buffer[2*512+4];


  printk (KERN_INFO "call: dmx_lpr2dmx_thread(%p => %p)\n", userdata, card);

  if (!card)
    {
      card->lpr2dmxd_running = 0;
      return 0;
    }

  card->lpr2dmxd_running = 1;
  ONDEBUG(printk ("lpr2dmxd running\n"));

  /* Setup a nice name */
  strcpy(current->comm, "lpr2dmxd");

#if 0
  lock_kernel();
#endif

#if 0
  daemonize();
#endif


  parport_write_control (card->pdev->port, 0);
  udelay(1);
  parport_enable_irq(card->pdev->port);
  lpr2dmx_rb_reset (card->pdev);

  ONDEBUG(printk (KERN_INFO "lpr2dmx-thread: resetting interface\n"));
  lpr2dmx_rb_reset (card->pdev);

  /* Send me a signal to get me die (for debugging) */
  do
    {
      int stat = lpr2dmx_prepare_buffer (card, buffer, 512*2+4);
      if (stat > 0)
    send_buffer (card);
      else if (stat < 0)
  printk (KERN_INFO "%s: FATAL: error filling txbuffer\n", "lpr2dmx");

      if (!interruptible_sleep_on_timeout(&card->lpr2dmxd_wait, 250))
  {
    unsigned short  lpr2dmxid;

    /* timed out => reset */
    printk (KERN_INFO "lpr2dmx: timeout => resetting interface\n");
    lpr2dmx_rb_reset (card->pdev);
    interruptible_sleep_on_timeout(&card->lpr2dmxd_wait, 1);
    lpr2dmxid = lpr2dmx_getid (card->pdev);
    lpr2dmx_rb_reset (card->pdev);
    printk (KERN_INFO "lpr2dmx: reset OK interface with id 0x%04X\n", lpr2dmxid);
  }
      else
  {
  }
    } while (!signal_pending(current));

  if (card && card->pdev && card->pdev->port)
    {
      struct parport *pport = card->pdev->port;
      if (pport->ops && pport->ops->disable_irq)
  pport->ops->disable_irq(pport);
    }
  else
    printk (KERN_INFO "lpr2dmx-irq: failed to disable interupt");

  card->xmit_buffer = NULL;
  card->xmit_size   = 0;


  ONDEBUG(printk (KERN_INFO "lpr2dmx-thread exiting"));
  card->lpr2dmxd_running = 0;

  return 0;
}




/*
 * called to start the thread for that interface
 */
int lpr2dmxthread_init(LPR2DMX_Interface *card)
{
  int pid = kernel_thread(dmx_lpr2dmx_thread, (void *)card, CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
  if (pid >= 0)
    {
      card->lpr2dmxd_pid = pid;
      printk (KERN_INFO "pid for lpr2dmxd is %d\n", pid);
      return 0;
    }
  return -1;
}


/*
 * called to stop the thread for that interface
 */
void lpr2dmxthread_cleanup(LPR2DMX_Interface *card)
{
  if (card && card->lpr2dmxd_pid>2)
    {
      int ret;

      ONDEBUG(printk (KERN_INFO "attempting to kill process with id %d\n", card->lpr2dmxd_pid));

      ret = kill_proc(card->lpr2dmxd_pid, SIGTERM, 0);
      if (!ret)
  {
    /* Wait 10 seconds */
    int count = 10 * 100;

    while (card->lpr2dmxd_running && --count)
      {
        current->state = TASK_INTERRUPTIBLE;
        schedule_timeout(1);
      }
    if (!count)
      printk (KERN_INFO "giving up on killing lpr2dmx-thread");
  }
    }
}
#endif

/*
 * ----------- Universe Access ----------------------------
 */


/*
 * This method is called by the dmxdev module
 * if slots have been changed by the userspace
 * for e.g. by a write to /dev/dmx.
 */
static int  write_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  ONDEBUG(printk("lpr2dmx:write_universe(): offs=%i size=%i\n", (int)offs, (int)size));
  if (u && u->user_data && buff && size > 0 && offs+size < 512)
    {
      LPR2DMX_Universe *lu = (LPR2DMX_Universe *)u->user_data;

      memcpy ((lu->buffer)+offs, (void *)buff, size);
      u->signal_changed (u, offs, size);
      return size;
    }
  return -EINVAL;
}


/*
 * read slots from the output universe.
 * This function can be used for both, the input and the output universe.
 */
static int read_universe (DMXUniverse *u, off_t start, DMXSlotType *buff, size_t size)
{
  if (u && u->user_data && buff && size > 0 && start+size < 512)
    {
      LPR2DMX_Universe *lu = (LPR2DMX_Universe *)u->user_data;

      memcpy ((void *)buff, (lu->buffer)+start, size);
      return size;
    }
  return -EINVAL;
}



int  dummy_data_available (DMXUniverse *u, uint start, uint size)
{
  return 1;
}



/*=======[ insertion / deletion of module ]==========*/

/*
 * ----------------------- Properties --------------------------------
 */



#define LPR2DMX_INTERFACE(mif) \
  LPR2DMX_Interface *mif = NULL; \
  if (!p || !(mif = (LPR2DMX_Interface *)p->data)) \
    return -1;


/*
 * parport setter/getter methods
 */
static int parport_get_long (DMXProperty *p, long *val)
{ /* interface */
  LPR2DMX_INTERFACE(mif);
  ONDEBUG(printk (KERN_INFO "parport_get_long called\n"));
  if (val)
    {
      *val = (long)mif->parport;
      return -1;
    }
  return 0;
}

static int parport_set_long (DMXProperty *p, long val)
{ /* interface */
  LPR2DMX_INTERFACE(mif);
  ONDEBUG(printk (KERN_INFO "parport_set_long called\n"));
  mif->parport = (int)val;
  return 0;
}

/* ----------------------------
 * slots setter/getter methods
 */
static int slots_get_long (DMXProperty *p, long *val)
{ /* interface */
  LPR2DMX_INTERFACE(mif);
  ONDEBUG(printk (KERN_INFO "slots_get_long called\n"));
  if (val)
    {
      *val = (long)mif->slots;
      return -1;
    }
  return 0;
}

static int slots_set_long (DMXProperty *p, long val)
{ /* interface */
  LPR2DMX_INTERFACE(mif);
  ONDEBUG(printk (KERN_INFO "slots_set_long called\n"));
  mif->slots = (int)val;
  return 0;
}


/* ----------------------------
 * mabsize setter/getter methods
 */
static int mabsize_get_long (DMXProperty *p, long *val)
{ /* interface */
  LPR2DMX_INTERFACE(mif);
  ONDEBUG(printk (KERN_INFO "mabsize_get_long called\n"));
  if (val)
    {
      *val = (long)mif->mabsize;
      return -1;
    }
  return 0;
}

static int mabsize_set_long (DMXProperty *p, long val)
{ /* interface */
  LPR2DMX_INTERFACE(mif);
  ONDEBUG(printk (KERN_INFO "mabsize_set_long called\n"));
  mif->mabsize = (char)val;
  return 0;
}

/* ----------------------------
 * startcode setter/getter methods
 */
static int startcode_get_long (DMXProperty *p, long *val)
{ /* universe */
  LPR2DMX_INTERFACE(mif);
  ONDEBUG(printk (KERN_INFO "startcode_get_long called\n"));
  if (val)
    {
      *val = (long)mif->startcode;
      return -1;
    }
  return 0;
}

static int startcode_set_long (DMXProperty *p, long val)
{ /* universe */
  LPR2DMX_INTERFACE(mif);
  ONDEBUG(printk (KERN_INFO "startcode_set_long called\n"));
  mif->startcode = (unsigned char)val;
  return 0;
}

/* ----------------------------
 * startcode setter/getter methods
 */
static int frames_get_long (DMXProperty *p, long *val)
{ /* universe */
  LPR2DMX_INTERFACE(mif);

  ONDEBUG(printk (KERN_INFO "frames_get_long called\n"));
  if (val)
    {
      *val = mif->frames;
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



/*
 * This method is called before the universe will be deleted
 */
static int lpr2dmx_delete_universe (DMXUniverse *u)
{
  ONDEBUG(printk (KERN_INFO "lpr2dmx_delete_universe (%p)\n", u));

  if (u && u->user_data)
    {
      LPR2DMX_Universe *dmxu = (LPR2DMX_Universe *)u->user_data;
      if (dmxu)
  {
    if (dmxu->card)
      dmxu->card->num_universes--;
    ONDEBUG(printk (KERN_INFO "lpr2dmx_delete_universe: try free LPR2DMX_Universe\n"));
    DMX_FREE(dmxu);
    u->user_data = NULL;
    ONDEBUG(printk (KERN_INFO "lpr2dmx_delete_universe: LPR2DMX_Universe freed\n"));
  }
    }
  return 0;
}



/*
 * This method is called after the universe is created
 * but before it will be inserted.
 */
static int lpr2dmx_create_universe (DMXUniverse *u, DMXPropList *pl)
{
  ONDEBUG(printk ("lpr2dmx_create_universe (%p, %p)\n", u, pl));

  if (u && u->interface && u->interface->user_data)
    {
      LPR2DMX_Interface *card = (LPR2DMX_Interface *)(u->interface->user_data);
      LPR2DMX_Universe  *univ = DMX_ALLOC(LPR2DMX_Universe);

      ONDEBUG(printk ("lpr2dmx_create_universe: card=%p, univ=%p\n", card, univ));

      if (univ)
  {
    int  i;
    u->user_data = univ;
    univ->universe = u;

    univ->card = card;
    for (i=0; i<512; i++)
      univ->buffer[i] = 0;

    u->read_slots = read_universe;
    u->write_slots = write_universe;

          ONDEBUG(printk ("before switch\n"));

    switch (card->type)
      {
      case LPR2DMX1:
        if (u->interface->universes)
                {
                  printk (KERN_INFO "lpr2dmx_create_interface: LPR2DMX1\n");
      return -1;
                }
        strcpy (u->connector, "first");
        u->conn_id = 0;
        break;


      case LPR2DMX2:
        if (u->interface->universes  &&  u->interface->universes->next)
    return -1;

        if (u->interface->universes && u->interface->universes->conn_id == 0)
    {
      strcpy (u->connector, "second");
      u->conn_id = 1;
    }
        else
    {
      strcpy (u->connector, "first");
      u->conn_id = 0;
    }
              printk (KERN_INFO "lpr2dmx_create_interface: LPR2DMX2 universe %d/%s\n", u->conn_id, u->connector);
        break;


      case LPR2DMX3:
        if (u->interface->universes)
                {
                  printk (KERN_INFO "lpr2dmx_create_interface: LPR2DMX3\n");
      return -1;
                }
        strcpy (u->connector, "first");
        u->conn_id = 0;
        break;


      default:
        printk (KERN_INFO "unknown interface type %d\n", card->type);
        return -1;
      }

    u->user_delete = lpr2dmx_delete_universe;
    card->num_universes++;
    wake_up_interruptible (&card->lpr2dmxd_wait);

          ONDEBUG(printk ("lpr2dmx_create_universe - OK\n"));

    return 0;
  }
    }
  return -1;
}



static int lpr2dmx1_create_universe (DMXUniverse *u, DMXPropList *pl)
{
  return lpr2dmx_create_universe (u, pl);
}

static int lpr2dmx2_create_universe (DMXUniverse *u, DMXPropList *pl)
{
  return lpr2dmx_create_universe (u, pl);
}

#if 0
static int lpr2dmx3_create_universe (DMXUniverse *u, DMXPropList *pl)
{
  int state = lpr2dmx_create_universe (u, pl);
  if (state >= 0)
    {
#if 0
      /*
       * If we are requested to use buffered mode we only have 64 channels.
       */
      if(p = pl->find(pl, "slots"))
        p->set_long(p, 64);
#endif
    }
  return state;
}
#endif



static int lpr2dmx_delete_interface (DMXInterface *i)
{
  printk (KERN_INFO "lpr2dmx_delete_interface (%p)\n", i);
  if (i && i->user_data)
    {
      LPR2DMX_Interface *dmxif = (LPR2DMX_Interface *)i->user_data;

#ifdef LPR2DMX_USE_THREADS
      lpr2dmxthread_cleanup (dmxif);
#endif

      if (dmxif->pdev)
  {
    parport_release (dmxif->pdev);
    parport_unregister_device(dmxif->pdev);
  }
      ONDEBUG(printk (KERN_INFO "lpr2dmx_delete_interface: try free LPR2DMX_Interface\n"));
      DMX_FREE(dmxif);
      i->user_data = NULL;
      ONDEBUG(printk (KERN_INFO "lpr2dmx_delete_interface: LPR2DMX_Interface freed\n"));
      return 0;
    }
  return -1;
}




void lpr2dmxinterface_initialize (LPR2DMX_Interface *intr)
{
  intr->interface = NULL;
  intr->pdev      = NULL;
  intr->parport   = -1;
  intr->mabsize   = 8;
  intr->saverdmx  = 0;
  intr->slots     = 512;
  intr->type      = -1;
  intr->num_universes = 0;
  intr->startcode   = 0;
  intr->frames      = 0L;
  intr->xmit_buffer = NULL;
  intr->xmit_size   = 0;
  intr->xmit_error  = 0;
  intr->watchdog_triggered = 0;
  intr->transfer_finished  = 0;
  init_waitqueue_head (&intr->lpr2dmxd_wait);
  intr->lpr2dmxd_pid = 0;
  intr->lpr2dmxd_running = 0;
}


static int lpr2dmx_create_interface (DMXInterface *i, DMXPropList *pl)
{
  ONDEBUG(printk (KERN_INFO "lpr2dmx_create_interface (%p, %p)\n", i, pl));
  if (i)
    {
      LPR2DMX_Interface *intr = DMX_ALLOC(LPR2DMX_Interface);
      if (intr)
  {
    long parport = -1;
    struct parport *pport = NULL;

    lpr2dmxinterface_initialize (intr);

    i->user_data = (void *)intr;

    if (pl && pl->find)
      {
        DMXProperty *p = pl->find(pl, "parport");
        if (!p) pl->add(pl, p=dmxprop_create_long("parport", 0L));
        if (p)
    {
      p->get_long (p, &parport);
      dmxprop_user_long (p, parport_get_long, parport_set_long, (void *)i);
    }
        else
    printk (KERN_INFO "failed to create property parport\n");

        if (parport < 0)
    return -1;

        ONDEBUG(printk (KERN_INFO "lpr2dmx_create_interface: looking for parport%ld\n", parport));

        pport = parport_find_number(parport);
        if (pport /*  && pport->irq != -1 */ )
    {
      struct pardevice *newpdev;

      ONDEBUG(printk (KERN_INFO "lpr2dmx_create_interface: found parport%ld\n", parport));


      newpdev = parport_register_device (pport, "lpr2dmx",
                 NULL,
                                                     NULL,
                                                     dmx_interrupt,
                                                     PARPORT_DEV_EXCL,
                                                     (void *)intr);
      if (!newpdev)
        {
          printk (KERN_INFO "failed to get access to parport\n");
          return -1;
        }

      printk (KERN_INFO "got parport%ld for dmx-device\n", parport);

      intr->pdev = newpdev;

      if (parport_claim (newpdev)==0)
        {
          char            str[200];
          unsigned short  lpr2dmxid = lpr2dmx_getid (newpdev);
          int             t = lpr2dmxid>>8;
          unsigned int    tid=0;

          ONDEBUG(printk (KERN_INFO "claim parport%ld succeded, interface-id = 0x%04X\n", parport, lpr2dmxid));

          if ((tid=lpr2dmx_getinfo (newpdev, str, sizeof(str))) != lpr2dmxid)
      printk (KERN_INFO "failure while rereading device-info %04X,%04X\n", tid, lpr2dmxid);
          else
      printk (KERN_INFO "%s\n", str);

          switch ((intr->type = t))
      {
      case LPR2DMX1:
      case LPR2DMX2:
      case LPR2DMX3:
        break;

      default:
        intr->type = -1;
        break;
      }


          if (intr->type >= 0)
      {
        if (pport->irq == -1)
          {
            printk (KERN_ERR "I found an lpr2dmx interface at parport%ld, but it has no IRQ assigned.\n", parport);
            printk (KERN_ERR "Take a look at the documentation on how to assign an interrupt to the parport.\n");
          }
        else
          {
            if ((p = pl->find(pl, "slots")) == NULL)
        pl->add(pl, p=dmxprop_create_long("slots", 512L));
            if (p)
        dmxprop_user_long (p, slots_get_long, slots_set_long, (void *)intr);

            if ((p = pl->find(pl, "mabsize")) == NULL)
        pl->add(pl, p=dmxprop_create_long("mabsize", 4L));
            if (p)
        dmxprop_user_long (p, mabsize_get_long, mabsize_set_long, (void *)intr);

            if ((p = pl->find(pl, "frames")) == NULL)
        pl->add(pl, p=dmxprop_create_long("frames", 0L));
            if (p)
        dmxprop_user_long (p, frames_get_long, frames_set_long, (void *)intr);

            if ((p = pl->find(pl, "startcode")) == NULL)
        pl->add(pl, p=dmxprop_create_long("startcode", 0L));
            if (p)
        dmxprop_user_long (p, startcode_get_long, startcode_set_long, (void *)intr);

            /* create thread */

            printk(KERN_INFO "after property creation\n");

            intr->interface = i;

            intr->pdev = newpdev;
            intr->parport = parport;
            intr->mabsize = 8;
            intr->saverdmx = 0;
            intr->slots = 512;

            intr->startcode = 0;
            intr->frames = 0L;

            intr->xmit_buffer = NULL;
            intr->xmit_size   = 0;
            intr->xmit_error  = 0;

            intr->watchdog_triggered=0;
            intr->transfer_finished=0;

            init_waitqueue_head (&intr->lpr2dmxd_wait);
            intr->lpr2dmxd_pid = 0;
            intr->lpr2dmxd_running = 0;

            ONDEBUG(printk(KERN_INFO "after default settings\n"));
            printk(KERN_INFO "interface=%p\n", intr);

#ifdef LPR2DMX_USE_THREADS
            if (lpr2dmxthread_init(intr) >= 0)
        {
          i->user_delete = lpr2dmx_delete_interface;
          return 0;
        }
            printk (KERN_INFO "error createing thread\n");
#else
            lpr2dmx_send_next_byte (dev, 4711);
            return 0;
#endif
          }
      }
          else
      printk (KERN_INFO "no interface found at parport%ld.\n", parport);
        }
      else
        printk (KERN_INFO "unable to claim parport%ld. may be locked by another driver?\n", parport);
      parport_unregister_device(newpdev);
    }

      }
    DMX_FREE(intr);
  }
    }
  return -1;
}


/*
 * Autoprobe for a specific parport
 */
static DMXDriver *lpr2dmx_autoprobe_driver (DMXFamily *f, DMXPropList *pl)
{
  DMXDriver *drv = NULL;

  if (f && pl && pl->find)
    {
      long parport = 0L;
      struct parport *pport;

      DMXProperty *p = pl->find(pl, "parport");
      if (!p)
        return NULL;

      p->get_long(p, &parport);

      pport = parport_find_number(parport);
      if (pport)
        {
          struct pardevice *newpdev = parport_register_device (pport, "lpr2dmx", NULL,NULL, NULL, 0, NULL);
          if (newpdev)
      {
        if (parport_claim (newpdev)==0)
                {
            switch (lpr2dmx_getid (newpdev) >> 8)
                  {
                    case LPR2DMX1: drv = dmx_find_driver (f, "lpr2dmx1"); break;
                    case LPR2DMX2: drv = dmx_find_driver (f, "lpr2dmx2"); break;
                    case LPR2DMX3: drv = dmx_find_driver (f, "lpr2dmx3"); break;
                  }
            parport_release (newpdev);
                }
              parport_unregister_device(newpdev);
      }
        }
    }
  return drv;
}




/*
 * Autoprobe for DMX-Interfaces that act as Jan Menzels
 * and return on what parports to find them returns the
 * number of the last parport that has been found + 1;
 */
static int autoprobe_for_lpr2dmx (int *porttype, int maxports)
{
  int count = -1;
  struct parport *pport;
  char *drivername = "lpr2dmx";
  int i;

  for (i=0; i<maxports; i++)
    porttype[i] = -1;

  for (i=0; (pport=parport_find_number(i)); ++i)
    {
      int pnum = pport->number;
      struct pardevice *newpdev = parport_register_device (pport, "lpr2dmx",
                 NULL,NULL,
                 NULL, /*dmx_interrupt*/
                 0,
                 (void *)NULL /*dev*/
                 );
      if (newpdev)
  {
    if (parport_claim (newpdev)==0)
      {
        char            str[200];
        unsigned short  lpr2dmxid = lpr2dmx_getid (newpdev);
              unsigned char   lpr2dmxminor = lpr2dmxid & 0xff;
        lpr2dmxid >>= 8;
        if (lpr2dmxid==LPR2DMX1 || lpr2dmxid==LPR2DMX2 || lpr2dmxid==LPR2DMX3)
    {
      int num_universes = 0;

      printk (KERN_INFO "%s: device with major id 0x%02X (minor=0x%02X) at parport%d found\n", drivername, lpr2dmxid, lpr2dmxminor, pnum);

      if (lpr2dmx_getinfo (newpdev, str, sizeof(str))>>8 != lpr2dmxid)
        printk (KERN_INFO "%s: failure while rereading device-info at parport%d\n", drivername, pnum);
      else
        printk (KERN_INFO "%s: %s with id 0x%02X at parport%d\n", drivername, str, lpr2dmxid, pnum);
      switch (lpr2dmxid)
        {
        case LPR2DMX1:
          if (pnum < maxports)
      {
        num_universes = 1;
        porttype[pnum] = 0x1;
        if (count < pnum) count = pnum;
      }
          break;  /* one universe out */

        case LPR2DMX2:
          if (pnum < maxports)
      {
        num_universes = 2;
        porttype[pnum] = 0x4;
        if (count < pnum) count = pnum;
      }
          break;  /* two universes out */

                    case LPR2DMX3:
          if (pnum < maxports)
      {
        num_universes = 1;
        porttype[pnum] = 0x0d;
        if (count < pnum) count = pnum;
      }
          break;  /* one universe out */

        }
      if (num_universes > 0)
        printk (KERN_INFO "Interface of type %d has %d universes\n", porttype[pnum], num_universes);
    }
        else if (lpr2dmxid==0 || lpr2dmxid==0xffff)
    printk (KERN_INFO "%s: no interface found at parport%d\n", drivername, pnum);
        else
    printk (KERN_INFO "%s: unknown interface with id 0x%04X at parport%d\n", drivername, lpr2dmxid, pnum);

        parport_release (newpdev);
      }
    parport_unregister_device(newpdev);
  }
    }
  return count+1;
}


/*
 * return the id of the interface.
 * It is the parport number.
 */
int lpr2dmx_getUniverseID (DMXUniverse *u, char *id, size_t size)
{
  if (u && u->user_data && id && size >= 5)
    {
      LPR2DMX_Universe *lu = (LPR2DMX_Universe *)u->user_data;
      if (lu->card)
        {
          char *p = id;
          p += sprintf(id, "%d", lu->card->parport);
          return p-id;
        }
    }
  return -1;
}



/*
 * --------- Module creation / deletion ---------------
 */

/* TODO: use module_param_array() */
static int parport[4];
static int pp0=-1, pp1=-1, pp2=-1, pp3=-1;
module_param(pp0, int, S_IRUGO);
module_param(pp1, int, S_IRUGO);
module_param(pp2, int, S_IRUGO);
module_param(pp3, int, S_IRUGO);
MODULE_PARM_DESC(pp0,"parport numbers (0=parport0)");
MODULE_PARM_DESC(pp1,"parport numbers (0=parport0)");
MODULE_PARM_DESC(pp2,"parport numbers (0=parport0)");
MODULE_PARM_DESC(pp3,"parport numbers (0=parport0)");



static DMXFamily *family = NULL;


static int __init lpr2dmx_init(void)
{
  parport[0] = pp0;
  parport[1] = pp1;
  parport[2] = pp2;
  parport[3] = pp3;

  family = dmx_create_family ("PAR");
  if (family)
    {
      DMXDriver *drv1 = family->create_driver (family, "lpr2dmx1", lpr2dmx1_create_universe, NULL);
      DMXDriver *drv2 = family->create_driver (family, "lpr2dmx2", lpr2dmx2_create_universe, NULL);
      DMXDriver *drv3 = family->create_driver (family, "lpr2dmx3", lpr2dmx1_create_universe, NULL);

      if (drv1 && drv2 && drv3)
  {
    int num = 0;

    drv1->user_create_interface = lpr2dmx_create_interface;
    drv2->user_create_interface = lpr2dmx_create_interface;
    drv3->user_create_interface = lpr2dmx_create_interface;
          drv1->getUniverseID = lpr2dmx_getUniverseID;
          drv2->getUniverseID = lpr2dmx_getUniverseID;
          drv3->getUniverseID = lpr2dmx_getUniverseID;

          if (parport[0] == -1)
            {
        int  i;
        int  tmp[10];
        int  count;
        count = autoprobe_for_lpr2dmx (tmp, 10);
        printk (KERN_INFO "autoprobing found %d interfaces\n", count);

        for (i=0; i<count; i++)
          {
            if (tmp[i] >= 0)
        {
          DMXInterface *dmxif = NULL;

          ONDEBUG(printk (KERN_INFO "try createing interface for parport%d of type %d\n", i, tmp[i]));
          if (tmp[i]==LPR2DMX1)
            dmxif = drv1->create_interface (drv1, dmxproplist_vacreate ("parport=%l", (long)i));

          else if (tmp[i]==LPR2DMX2)
            dmxif = drv2->create_interface (drv2, dmxproplist_vacreate ("parport=%l", (long)i));

          else if (tmp[i]==LPR2DMX3)
            dmxif = drv3->create_interface (drv3, dmxproplist_vacreate ("parport=%l", (long)i));

          if (dmxif)
                        {
                          if (dmxif->create_universe (dmxif, 0, NULL))
                num++;
              if (tmp[i]==LPR2DMX2 && dmxif->create_universe (dmxif, 0, NULL))
                num++;
                        }
        }
          }
            }
          else
            {
              int i;
              for (i=0; parport[i]!=-1 && i < sizeof(parport)/sizeof(parport[0]); i++)
                {
      DMXPropList *pl = dmxproplist_vacreate ("parport=%l", (long)parport[i]);
                  DMXDriver *drv = lpr2dmx_autoprobe_driver (family, pl);
                  if (drv)
                    {
          DMXInterface *dmxif = drv->create_interface (drv, pl);
                if (dmxif && dmxif->create_universe (dmxif, 0, NULL))
                num++;
                    }
                }
            }
    printk (KERN_INFO "created %d universes\n", num);
  }
    }

  return 0;
}


static void __exit lpr2dmx_exit(void)
{
  if (family)
    family->delete (family, 0);
}

module_init(lpr2dmx_init);
module_exit(lpr2dmx_exit);
