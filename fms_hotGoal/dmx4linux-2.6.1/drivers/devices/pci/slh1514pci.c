/*
 * slh1514pci.c : driver for Digimedia - Soundlight 1514 & 2514 DMX PCI boards
 *
 * Copyright (C) 2004 Bastien Andres <bastos@balelec.ch>
 *
 * Based on code from :
 *
 * Generic PCI Driver :
 *
 * Julien Gaulmin <julien.gaulmin@fr.alcove.com>, Alcôve
 * Pierre Ficheux (pierre@ficheux.com)
 *
 * MMAP implementation : Linux Device Drivers Book from
 *
 * Alessandro Rubini and Jonathan Corbet
 * O'Reilly & Associates
 *
 * DMX4Linux part adapted from digimedia_cs driver from :
 *
 * (c) 2001 Michael Stickel <michael@cubic.org> http://llg.cubic.org
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA or look at http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/pci.h>
#include <linux/proc_fs.h>

#include <dmx/dmxdev.h>
#include "slh1514pci.h"

MODULE_DESCRIPTION("DMX PCI driver for DGM - SLH 1514-2514 boards");
MODULE_AUTHOR("Bastien Andres");
MODULE_LICENSE("GPL");

/*
 * Arguments
 */

#ifndef DMXPCI_DMX4LINUX
static int major         = 0;   /* Major number */
MODULE_PARM_DESC(major,         "Static major number (none = dynamic)");
module_param(major, int, S_IRUGO);
#endif

#ifdef DOJ_USE_TIMER_WQ
static int set_callback  = 1;   /* Use Timer callback ioctl */
MODULE_PARM_DESC(set_callback,  "Set timer callback ioctl or not (default = 1)");
module_param(set_callback, int, S_IRUGO);
#endif

static int set_timer     = 1;   /* Use Timer queue */
MODULE_PARM_DESC(set_timer,     "Set timer function or not (default = 1)");
module_param(set_timer, int, S_IRUGO);

static int timer_freq    = 100; /* set frequency of timer function call */
MODULE_PARM_DESC(timer_freq,    "Set frequency of timer function call (default = 100)");
module_param(timer_freq, int, S_IRUGO);

static int set_proc      = 1;   /* Use proc_fs */
MODULE_PARM_DESC(set_proc,      "Set proc_fs entry or not (default = 1)");
module_param(set_proc, int, S_IRUGO);

static int set_bar       = 2;   /* Use BAR # for Read, Write and mmap */
MODULE_PARM_DESC(set_bar,       "Set which BAR to read and write (default = 2)");
module_param(set_bar, int, S_IRUGO);

static int debug_flags   = 7;   /* debug messages trigger */
MODULE_PARM_DESC(debug_flags,    "Set trigger for debugging messages (see slh1514pci.h)");
module_param(debug_flags, int, S_IRUGO);

static int cpu_mode      = 4;   /* dmx card mode */
MODULE_PARM_DESC(cpu_mode,    "Set card mode at init 0:Idle/Idle 1:Out/Idle 2:Idle/In 3:Out/In 4:Out/Out 5:In/In (default 4)");
module_param(cpu_mode, int, S_IRUGO);

/*
 * Supported devices
 */

static struct {char *name;} dmxpci_board_info[] __devinitdata =
{
  {DMXPCI_BOARD_INFO_0},
  {DMXPCI_BOARD_INFO_1}
};

static struct pci_device_id dmxpci_id_table[] __devinitdata =
{
  {DMXPCI_VENDOR_ID_0, DMXPCI_DEVICE_ID_0, PCI_ANY_ID, PCI_ANY_ID, 0, 0, DMXPCI_BOARD_TYPE_0},
  {DMXPCI_VENDOR_ID_1, DMXPCI_DEVICE_ID_1, PCI_ANY_ID, PCI_ANY_ID, 0, 0, DMXPCI_BOARD_TYPE_1},
  {0,} /* 0 terminated list */
};

MODULE_DEVICE_TABLE(pci, dmxpci_id_table);

/* ------------------------------------------------------------------------------------------- */

/*
 * Global variables
 */

static dmxpci_driver_t dmxpci_drv_data;

/* ------------------------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------------------------- */

/*
 *  DMX4LINUX dependant methods & functions
 */

#ifdef DMXPCI_DMX4LINUX

/* ------------------------------------------------------------------------ */
/* Functions to handle memory access and properties */
/* ------------------------------------------------------------------------ */
DMXPCI_STATIC inline unsigned short shortwritetocard (unsigned short *_x, unsigned short v)
{
  unsigned char *x = (unsigned char *)_x;
  x[0]=(v>>8)&0xff;
  x[1]=v&0xff;
  return v;
}

DMXPCI_STATIC inline unsigned short shortreadfromcard (unsigned short *_x)
{
  unsigned char *x = (unsigned char *)_x;
  return x[0]<<8 | x[1];
}

DMXPCI_STATIC int byteptr_get_long (DMXProperty *p, long *val)
{
  if (p && p->data && val)
  {
    *val = *((unsigned char *)p->data);
    return 0;
  }
  return -1;
}

DMXPCI_STATIC int byteptr_set_long (DMXProperty *p, long val)
{
  if (p && p->data)
  {
    *((unsigned char *)p->data) = val;
    return 0;
  }
  return -1;
}

DMXPCI_STATIC int shortptr_get_long (DMXProperty *p, long *val)
{
  if (p && p->data && val)
  {
    *val = shortreadfromcard (p->data);
    return 0;
  }
  return -1;
}

DMXPCI_STATIC int shortptr_set_long (DMXProperty *p, long val)
{
  if (p && p->data)
  {
    shortwritetocard (p->data, val);
    return 0;
  }
  return -1;
}

/*--------------------------------------------------------
 *-- dmx4linux calls it to write to an universe
 *-------------------------------------------------------*/
/* error if input universe */
/* use memcpy to access board ?! byte access ??? */
DMXPCI_STATIC int dmxpci_write_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  /* If it is an output universe,
   * then data has changed if a user
   * writes new data.
   */
  DO_DEBUG(DMXPCI_DEBUG_FUNCTION, printk(DMXPCI_DEBUG "static int dmxpci_write_universe (DMXUniverse *u=%p)\n", u));

  if (u && !u->kind && u->user_data && offs >= 0 && offs < 512)  /* output universe & offset check */
  {
    dmxpci_universe_t *dgm_u = (dmxpci_universe_t *)(u->user_data);
    if (offs+size>512)
      size = 512-offs;
    if (!dgm_u->data_pointer) return -1;
    /* copy data to local buffer */
    memcpy (&dgm_u->local_buffer[offs], buff, size);
    /* copy data to card buffer if virtual universe connected to card universe */
    if (dgm_u->io_mode == IO_MODE_OUT)
      memcpy (&dgm_u->data_pointer[offs], buff, size);
    dgm_u->data_avail = 1;
    u->signal_changed (u, offs, size);
    return size;
  }
  return -1;
}

/*--------------------------------------------------------
 *-- dmx4linux calls it to read from an universe
 *--
 *-- read slots from the output universe.
 *-- This function can be used for both,
 *-- the input and the output universe.
 *-------------------------------------------------------*/
/* can read both in and out universes */
/* set data_available to 0 */
/* use memcpy() to access board */
/* do we copy to buffer ? */
DMXPCI_STATIC int dmxpci_read_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  DO_DEBUG(DMXPCI_DEBUG_FUNCTION, printk(DMXPCI_DEBUG "static int dmxpci_read_universe (DMXUniverse *u=%p)\n", u));

  if (u && u->user_data && offs >= 0 && offs < 512) /* struct & offset check */
  {
    dmxpci_universe_t  *dgm_u = (dmxpci_universe_t *)(u->user_data);
    if (offs+size>512)
      size = 512-offs;
    if (!dgm_u->data_pointer) return -1;
    /* copy data from card if virtual universe connected to card universe */
    if (dgm_u->io_mode == IO_MODE_IN)
      memcpy (&dgm_u->local_buffer[offs], &dgm_u->data_pointer[offs], size);
    /* copy data from local buffer to dmx4linux driver buffer */
    memcpy (buff, &dgm_u->local_buffer[offs], size);
    dgm_u->data_avail = 0;
    /* a bit buggy, since it depends on the process that has opened dmx */
    return size;
  }
  return -EINVAL;
}

/*--------------------------------------------------------
 *-- returns < 0 for error, > 0 if new data is available and 0
 *-- if no new data is available for that universe.
 *-------------------------------------------------------*/
DMXPCI_STATIC int dmxpci_data_available (DMXUniverse *u, uint start, uint size)
{
  DO_DEBUG(DMXPCI_DEBUG_FUNCTION,printk(DMXPCI_DEBUG "static int dmxpci_pci_data_available (DMXUniverse *u=%p, uint start=%d, uint size=%d)\n", u, start, size))
  if (u && u->user_data)
  {
    dmxpci_universe_t  *dgm_u = (dmxpci_universe_t *)(u->user_data);
    return dgm_u->data_avail;
  }
  return -1;
}

/*--------------------------------------------------------
 *-- this check one input universe for new values
 *-------------------------------------------------------*/
DMXPCI_STATIC int dmxpci_check_universe (dmxpci_universe_t * dgm_u, unsigned short fc)
{
  int             j;
  int             first = -1;
  int             last  = 0;
  DMXUniverse    *dmx_u;
  unsigned char   val;

  DO_DEBUG(DMXPCI_DEBUG_TIMER, printk(DMXPCI_DEBUG "static int dmxpci_check_universe (dmxpci_universe_t *dgm_u=%p)\n", dgm_u));

  if (!dgm_u)
  {
    DO_DEBUG(DMXPCI_DEBUG_WARN_1, printk(DMXPCI_WARN "dmxpci_check_universe received a NULL universe struct\n"));
    return -1;
  }

  dgm_u->last_framecount = fc;

  for (j=0; j<512; j++)
  {
    val = dgm_u->data_pointer[j];
    if (val != dgm_u->local_buffer[j])
    {
      dgm_u->local_buffer[j] = val;
      if (first==-1)
        first=j;
      last=j;
    }
  }
  DO_DEBUG(DMXPCI_DEBUG_TIMER, printk(DMXPCI_DEBUG "universe <%d> checked interval=<%d,%d>\n", dgm_u->index,first,last));
  if (first != -1)
  {
    dgm_u->data_avail=1;
    dmx_u = dgm_u->dmx_u;
    dmx_u->signal_changed(dmx_u, first, last-first+1);
  }
  return first;
}

/*--------------------------------------------------------
 *-- this board centric function handles checks for new values
 *-- (both) interface(s) cpu_mode are checked to find which
 *-- universes to check with previous function
 *-------------------------------------------------------*/
/* access board with short fn & uchars -> ok */
DMXPCI_STATIC int dmxpci_check_receiver (dmxpci_board_t *brd)
{
  int i;
  unsigned short      fc=0;
  dmxpci_interface_t *dgm_if;
  dmxpci_universe_t  *dgm_u = NULL;

  DO_DEBUG(DMXPCI_DEBUG_TIMER, printk(DMXPCI_DEBUG "static int dmxpci_check_receiver (dmxpci_board_t *brd=%p)\n", brd));

  if (!brd)
  {
    DO_DEBUG(DMXPCI_DEBUG_WARN_1, printk(DMXPCI_WARN "dmxpci_check_receiver received a NULL board struct\n"));
    return -1;
  }

  for (i = 0 ; i < 2 ; i++)
  {
    if((dgm_if = brd->dgm_if[i]))
    {
      const int mode = dgm_if->cpu_mode;
      /* DO_DEBUG(DMXPCI_DEBUG_TIMER, printk(DMXPCI_DEBUG "check_receiver interface=%d mode=%d\n", i,mode)); */
      switch (mode)
      {
        case 0: /* Idle-Idle */
          break;
        case 1: /* Out-Idle */
          break;
        case 2: /* Idle-In */
          /* fall thru to case 3 which is identical to treat */
        case 3: /* Out-In */
          if((dgm_u = dgm_if->dgm_u[1]))
	    {
	      fc = shortreadfromcard(dgm_u->framecount);
	      if (fc != dgm_u->last_framecount)
		dmxpci_check_universe(dgm_u, fc);
	    }
          break;
        case 4: /* Out-Out */
          break;
        case 5: /* In-In */
          /* in mode 5 framecount is not updated so we always check - the user will need to choose timer delay ;-)*/
	  if(dgm_if->dgm_u[0])
	    {
	      fc = shortreadfromcard(dgm_if->dgm_u[0]->framecount);
	      dmxpci_check_universe(dgm_if->dgm_u[0], fc);
	    }
	  if(dgm_if->dgm_u[1])
	    {
	      fc = shortreadfromcard(dgm_if->dgm_u[1]->framecount);
	      dmxpci_check_universe(dgm_if->dgm_u[1], fc);
	    }
          break;
        default: /* this should not happen we would do nothing */
          break;
      }
    }
  }
  return 0;
}

/*--------------------------------------------------------
 *-- handles cpu-mode changes to update universe patch and card reg.
 *-------------------------------------------------------*/
/* need to update something else than mode fields ? */
DMXPCI_STATIC void dmxpci_update_mode (dmxpci_interface_t *dgm_if, unsigned char cpu_mode_)
{
  unsigned int   i;
  unsigned short m[2];

  DO_DEBUG(DMXPCI_DEBUG_FUNCTION, printk(DMXPCI_DEBUG "static void dmxpci_update_mode (dmxpci_interface_t *dgm_if=%p, cpu_mode_=%d )\n", dgm_if,cpu_mode_));

  m[0] = m[1] = IO_MODE_IDLE;

  switch (cpu_mode_)
  {
    case 0:
      break;
    case 1:
      m[0] = IO_MODE_OUT;
      if(dgm_if->dgm_u[0])
	shortwritetocard(dgm_if->dgm_u[0]->channels,dgm_if->dgm_u[0]->channels_count);
      break;
    case 2:
      m[1] = IO_MODE_IN;
      break;
    case 3:
      m[0] = IO_MODE_OUT;
      m[1] = IO_MODE_IN;
      if(dgm_if->dgm_u[0])
	shortwritetocard(dgm_if->dgm_u[0]->channels,dgm_if->dgm_u[0]->channels_count);
      break;
    case 4:
      m[0] = IO_MODE_OUT;
      m[1] = IO_MODE_OUT;
      if(dgm_if->dgm_u[0])
	{
	  int c = dgm_if->dgm_u[0]->channels_count;
	  if(dgm_if->dgm_u[1])
	    {
	      if(dgm_if->dgm_u[1]->channels_count > c)
		c=dgm_if->dgm_u[1]->channels_count;
	    }
	  shortwritetocard(dgm_if->dgm_u[0]->channels,c);
	}
      break;
    case 5:
      m[0] = IO_MODE_IN;
      m[1] = IO_MODE_IN;
      break;
  }

  DO_DEBUG(DMXPCI_DEBUG_TEMP_1, printk(DMXPCI_DEBUG "... will set mode ...\n"));
  dgm_if->cpu_mode = cpu_mode_;
  dgm_if->mem->cpu_mode = cpu_mode_;
  for (i = 0; i<2; i++)
    if(dgm_if->dgm_u[i])
      dgm_if->dgm_u[i]->io_mode = m[i];
  DO_DEBUG(DMXPCI_DEBUG_TEMP_1, printk(DMXPCI_DEBUG "... end of update_mode\n"));
}

DMXPCI_STATIC int dmxpci_init_interface (dgm_memory_t *dgm)
{
  DO_DEBUG(DMXPCI_DEBUG_FUNCTION, printk(DMXPCI_DEBUG "int dmxpci_init_interface (dgm_memory_t *dgm=%p)\n", dgm));

  if (dgm)
  {
    int i;
    dgm->out_startcode[0] = 0;
    dgm->out_startcode[1] = 0;
#if 0
    shortwritetocard(&dgm->out_channel_cnt, 512); /* written by update mode */
#endif
    shortwritetocard(&dgm->out_break_time, 2*100); /* 100us */
    shortwritetocard(&dgm->out_break_count, 0);    /* outgoing packets */
    shortwritetocard(&dgm->out_mbb_time, (2*0)<<8);    /* 0us */

    for (i=0; i<512; i++)
    {
      dgm->dmxbuffer[0][i] = 0;
      dgm->dmxbuffer[1][i] = 0;
    }
    return 0;
  }
  else
  {
    DO_DEBUG(DMXPCI_DEBUG_WARN_2, printk (DMXPCI_WARN "dmxpci_init_interface received NULL card memory struct\n"));
    return -1;
  }
}


/*--------------------------------------------------------
 *-- called by dmx4linux just before releasing DMXUniverse
 *-- release ressource of dmxpci_universe_t
 *-------------------------------------------------------*/
DMXPCI_STATIC int dmxpci_delete_universe (DMXUniverse *u)
{
  DO_DEBUG(DMXPCI_DEBUG_FUNCTION, printk (DMXPCI_DEBUG "static int dmxpci_delete_universe (DMXUniverse *u=%p)\n", u));

  if (u && u->interface && u->interface->user_data && u->user_data)
  {
    dmxpci_interface_t *dgm_if = (dmxpci_interface_t *)(u->interface->user_data);
    dmxpci_universe_t  *dgm_u  = (dmxpci_universe_t *)(u->user_data);

    dgm_if->dgm_u[dgm_u->index] = NULL;
    DMX_FREE(dgm_u);

    return 0;
  }
  return -1;
}

/*--------------------------------------------------------
 *-- called by dmx4linux to allow driver specific
 *-- universe creation
 *-------------------------------------------------------*/
DMXPCI_STATIC int dmxpci_create_universe (DMXUniverse *dmx_u, DMXPropList *pl)
{
  dmxpci_universe_t *dgm_u;
  unsigned long index = 666;

  DO_DEBUG(DMXPCI_DEBUG_FUNCTION, printk (DMXPCI_DEBUG "int dmxpci_create_universe (DMXUniverse *u=%p, DMXPropList *pl=%p)\n", dmx_u, pl));

  if (dmx_u && dmx_u->interface && dmx_u->interface->user_data && dmx_u->kind>=0 && dmx_u->kind<2)
  {
    dmxpci_interface_t *dgm_if = (dmxpci_interface_t *)dmx_u->interface->user_data;
    if (pl && pl->find)
    {
      DMXProperty *p = pl->find(pl, "index");
      if (p)
      {
        p->get_long (p, &index);
      }
    }
    if (index>2)
    {
      DO_DEBUG(DMXPCI_DEBUG_WARN_1, printk(DMXPCI_WARN "create_universe cannot find index in proplist <%lu>\n",index));
      return -1;
    }

    dgm_u = DMX_ALLOC(dmxpci_universe_t);
    if (!dgm_u)
    {
      DO_DEBUG(DMXPCI_DEBUG_WARN_1, printk(DMXPCI_WARN "create_universe cannot allocate universe struct\n"));
      return -1;
    }

    /* set dmx_u fields */

    dmx_u->conn_id = index;
    sprintf (dmx_u->connector, "BRD %d IF %u U %lu %s ", dgm_if->dgm_brd->minor, dgm_if->index,index,dmx_u->kind?"IN":"OUT");

    if (!dmx_u->kind)
      dmx_u->write_slots  = dmxpci_write_universe;
    dmx_u->read_slots     = dmxpci_read_universe;
    dmx_u->data_available = dmxpci_data_available;
    dmx_u->user_delete    = dmxpci_delete_universe;

    /* set dgm_u fields */

    dgm_u->index = (unsigned short) index;
    dgm_u->io_mode  = IO_MODE_IDLE; /* will be re set by update mode after */
    dgm_u->data_avail = 0;
    dgm_u->last_framecount = 0;
    memset(dgm_u->local_buffer, 0, sizeof(dgm_u->local_buffer));
    dgm_u->data_pointer = dgm_if->mem->dmxbuffer[index&1];
    if (dmx_u->kind) /* Input */
    {
      dgm_u->startcode  = &(dgm_if->mem->in_startcode[index&1]);
      dgm_u->channels   = &(dgm_if->mem->in_channel_cnt[index&1]);
      dgm_u->framecount = &(dgm_if->mem->in_break_cnt[index&1]);
      dgm_u->breaksize  = NULL;
      dgm_u->mbb_size   = NULL;
    }
    else /* Output */
    {
      dgm_u->startcode  = &(dgm_if->mem->out_startcode[index&1]);
      dgm_u->channels   = &(dgm_if->mem->out_channel_cnt);
      dgm_u->framecount = &(dgm_if->mem->out_break_count);
      dgm_u->breaksize  = &(dgm_if->mem->out_break_time);
      dgm_u->mbb_size   = &(dgm_if->mem->out_mbb_time);
      dgm_u->channels_count  = 512;
   }

   /*
    * Create a property for the startcode, slots and framecount.
    */

    if (!dmx_u->props)
    {
      DO_DEBUG(DMXPCI_DEBUG_WARN_1, printk(DMXPCI_WARN "create_universe cannot find dmx_u's proplist\n"));
      return -1;
    }

    {
    DMXPropList *pl = dmx_u->props;
    DMXProperty *p  = NULL;

    p = dmxprop_create_long ("startcode", 0);
    if (p)
    {
      if (dmxprop_user_long(p, byteptr_get_long, byteptr_set_long, dgm_u->startcode) < 0)
        p->delete(p);
      else
      pl->add(pl, p);
    }
    p = dmxprop_create_long ("frames", 0);
    if (p)
    {
      if (dmxprop_user_long(p, shortptr_get_long, shortptr_set_long, dgm_u->framecount) < 0)
        p->delete(p);
      else
        pl->add(pl, p);
    }
    p = pl->find(pl,"slots");
    if(p)
      dmxprop_user_long(p, shortptr_get_long, shortptr_set_long, dgm_u->channels);
    }

    dgm_u->dgm_if    = dgm_if;
    dgm_u->dmx_u     = dmx_u;

    dmx_u->user_data = (void *)dgm_u;
    return 0;
  }
  return -1;
}

/*--------------------------------------------------------
 *-- dmxpci_delete_interface
 *--
 *-- This is the function called by the dmx4linux layer
 *-- after all universes for that interface are successfully
 *-- deleted and before the interface itself is to be deleted.
 *-- It cleans up anything that is not removed by dmx4linux.
 *-------------------------------------------------------*/
DMXPCI_STATIC int dmxpci_delete_interface (DMXInterface *i)
{
  DO_DEBUG(DMXPCI_DEBUG_FUNCTION, printk(DMXPCI_DEBUG "static int dmxpci_delete_interface (DMXInterface *i=%p)\n", i));
  if (i && i->user_data)
  {
    dmxpci_interface_t *dgm_if = (dmxpci_interface_t *)i->user_data;
    i->user_data = NULL;
    dgm_if->dgm_brd->dgm_if[dgm_if->index] = NULL;
    DMX_FREE(dgm_if);
    return 0;
  }
  return -1;
}

/*--------------------------------------------------------
 *-- dmxpci_create_interface
 *--
 *-- This function is called after the internal data
 *-- structures are created and before the interface
 *-- is added.
 *-------------------------------------------------------*/
DMXPCI_STATIC int dmxpci_create_interface (DMXInterface *dmx_if, DMXPropList *pl)
{
  int i;
  struct list_head *cur;
  dmxpci_board_t *brd_temp = NULL;
  dmxpci_board_t *brd      = NULL;
  unsigned long minor      = 0L;
  unsigned long interface  = 666L;
    dmxpci_interface_t *dgm_if;
  DMXUniverse *dmx_u[4];

  DO_DEBUG(DMXPCI_DEBUG_FUNCTION, printk(DMXPCI_DEBUG "int dmxpci_create_interface (DMXInterface *i=%p, DMXPropList *pl=%p)\n", dmx_if, pl));

  if (pl && pl->find)
  {
    DMXProperty *p = pl->find(pl, "minor");
    if (p)
    {
      p->get_long (p, &minor);
    }
  }
  if (!minor)
  {
    DO_DEBUG(DMXPCI_DEBUG_WARN_1, printk(DMXPCI_WARN "create_interface cannot find minor in proplist\n"));
    return -1;
  }
  if (pl && pl->find)
  {
    DMXProperty *p = pl->find(pl, "interface");
    if (p)
    {
      p->get_long (p, &interface);
    }
  }
  if (interface > 1)
  {
    DO_DEBUG(DMXPCI_DEBUG_WARN_1, printk(DMXPCI_WARN "create_interface cannot find interface id in proplist\n"));
    return -1;
  }

  list_for_each(cur, &(dmxpci_drv_data.link))
  {
    brd_temp = list_entry(cur, dmxpci_board_t, link);
    if ((int)brd_temp->minor == minor)
      brd = brd_temp;
  }

  if (!brd)
  {
    DO_DEBUG(DMXPCI_DEBUG_WARN_1, printk(DMXPCI_WARN "create_interface cannot find board with given minor %lu\n",minor));
    return -1;
  }

  DO_DEBUG(DMXPCI_DEBUG_TEMP_1, printk (DMXPCI_DEBUG "create_interface found board struct with given minor %lu\n", minor));

    dgm_if = DMX_ALLOC(dmxpci_interface_t);
    if (!dgm_if)
    {
      DO_DEBUG(DMXPCI_DEBUG_WARN_2, printk(DMXPCI_WARN "dmxpci : failed to allocate interface\n"));
      return -1;
  }

  dgm_if->index = interface;
  dgm_if->cpu_mode = cpu_mode;
  if (interface)
    dgm_if->mem = (dgm_memory_t *)(brd->bar_addr[DMXPCI_MEMORY_REGION_1]+DMXPCI_MEMORY_OFFSET_1);
  else
    dgm_if->mem = (dgm_memory_t *)(brd->bar_addr[DMXPCI_MEMORY_REGION_0]+DMXPCI_MEMORY_OFFSET_0);

  dgm_if->dgm_brd = brd;
  dgm_if->dmx_if = dmx_if;

  dmx_if->user_data   = dgm_if;
  dmx_if->user_delete = dmxpci_delete_interface;

  brd->dgm_if[interface]=dgm_if;

  /* create universes & link structs */


  for (i=0; i<2; ++i)
  {
    int kind=0;
    dmx_u[i] = 0;
    dgm_if->dgm_u[i] = 0;

    switch(cpu_mode)
      {
      default:
      case 0: goto next_iteration;
      case 1: if(i>0) goto next_iteration; break;
      case 2: if(i!=1) goto next_iteration; kind=1; break;
      case 3: if(i==1) kind=1; break;
      case 4: break;
      case 5: kind=1; break;
      }

    if ((dmx_u[i] = dmx_if->create_universe(dmx_if,kind,dmxproplist_vacreate("index=%l",i)))
    &&  (dgm_if->dgm_u[i] = (dmxpci_universe_t*)dmx_u[i]->user_data))
    {
      DO_DEBUG(DMXPCI_DEBUG_INFO_2, printk (DMXPCI_INFO "created universe %i\n",i));
    }
    else
    {
      DO_DEBUG(DMXPCI_DEBUG_WARN_2, printk(DMXPCI_WARN "dmxpci : failed to initialize universe %i\n",i));
#if 0
      /* will the universes be deleted by a side effect of the calling func ??? */
      for (i-- ; i>=0 ; i--)
      {
	/* how do we delete freshly created universes ??? */
        dmx_delete_universe(dmx_u[i]);
      }
      DMX_FREE(dgm_if);
#endif
      return -1;
    }

  next_iteration: ;
  }

  /* update_mode */

  dmxpci_update_mode(dgm_if,cpu_mode);

  /* init card */

  if (dmxpci_init_interface (dgm_if->mem) < 0)
  {
    DO_DEBUG(DMXPCI_DEBUG_WARN_2, printk(DMXPCI_WARN "dmxpci : failed to initialize interface\n"));
    DMX_FREE(dgm_if);
    return -1;
  }
  DO_DEBUG(DMXPCI_DEBUG_INFO_2, printk (DMXPCI_INFO "dmxpci : successfully initialized dmx pci interface\n"));
  return 0;
}

#endif /* DMXPCI_DMX4LINUX */

/* ------------------------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------------------------- */

/*
 * Timer handler
 */

DMXPCI_STATIC void dmxpci_timer_function(unsigned long param)
{
  dmxpci_board_t *brd = (dmxpci_board_t *)param;
  brd->timer_count++;
  brd->timer_tl.expires = jiffies + brd->timer_delay;
#if 0
  brd->timer_tl.expires = jiffies + 500; /* when it comes to test with slow debug messages stream ;-) */
#endif

#ifdef DOJ_USE_TIMER_WQ
  /* wake up ioctl call back */
  if (set_callback)
    wake_up_interruptible(&(brd->timer_wq));
#endif

#ifdef DMXPCI_DMX4LINUX
  if(brd->timer_param & 1)
    dmxpci_check_receiver(brd);
#endif

  /* place to DO SOMETHING more */

  /* if the timer is to be killed stop_async will be set to 3 so we don't register again and ack by setting value 4 */

  if (brd->stop_async == 3)
  {
    wake_up_interruptible(&(brd->stop_wq));
    brd->stop_async = 4;
  }
  else
  {
    add_timer(&brd->timer_tl);
  }
}

/* ------------------------------------------------------------------------------------------- */

/*
 * Proc entry read
 */

DMXPCI_STATIC int dmxpci_proc_read(char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
  dmxpci_board_t *brd = (dmxpci_board_t *)data;
  unsigned int i;
  unsigned char *base;

  u32 len = 0;

  /* data is NULL for the driver proc entry otherwise it points to the board struct */

  if (data == NULL)
  {
    len+= sprintf(buf+len, "\n");
    len = sprintf(buf, "DMXPCI DRIVER\n");
    len+= sprintf(buf+len, "\n");
    len+= sprintf(buf+len, "driver minor  = %d\n",dmxpci_drv_data.minor);
    len+= sprintf(buf+len, "boards count  = %d\n",dmxpci_drv_data.boards);
    len+= sprintf(buf+len, "set_timer     = %d\n",set_timer);
#ifdef DOJ_USE_TIMER_WQ
    len+= sprintf(buf+len, "set_callback  = %d\n",set_callback);
#endif
    len+= sprintf(buf+len, "set_proc      = %d\n",set_proc);
    len+= sprintf(buf+len, "set_bar       = %d\n",set_bar);
    len+= sprintf(buf+len, "debug_flags   = %d\n",debug_flags);
    len+= sprintf(buf+len, "init_cpu_mode = %d\n",cpu_mode);
#ifndef DMXPCI_DMX4LINUX
    len+= sprintf(buf+len, "major         = %d\n",major);
#endif
    len+= sprintf(buf+len, "timer_freq    = %d\n",timer_freq);
    len+= sprintf(buf+len, "HZ            = %u\n",HZ);
    len+= sprintf(buf+len, "\n");
  }
  else
  {
    len+= sprintf(buf+len, "\n");
    len = sprintf(buf, "DMXPCI BOARD / DEVICE    ID %d\n",brd->minor);
    len+= sprintf(buf+len, "\n");
    len+= sprintf(buf+len, "minor       = %u\n",brd->minor);
    len+= sprintf(buf+len, "type        = %u\n",brd->type);
    len+= sprintf(buf+len, "timer_count = %u\n",brd->timer_count);
    len+= sprintf(buf+len, "timer_param = %u\n",brd->timer_param);
    len+= sprintf(buf+len, "timer_delay = %u\n",(unsigned int)(brd->timer_delay));
    len+= sprintf(buf+len, "stop_async  = %u\n",brd->stop_async);
    len+= sprintf(buf+len, "\n");
    for (i=0;i<=brd->type;i++)
    {
      if (i)
        base = (unsigned char *)brd->bar_addr[DMXPCI_MEMORY_REGION_1]+DMXPCI_MEMORY_OFFSET_1;
      else
        base = (unsigned char *)brd->bar_addr[DMXPCI_MEMORY_REGION_0]+DMXPCI_MEMORY_OFFSET_0;

      len+= sprintf(buf+len, "Interface %d\n",i);
      len+= sprintf(buf+len, "\n");
      len+= sprintf(buf+len, "Ready Flag             = 0x%02X\n",(unsigned char)*(base+0x00));
      len+= sprintf(buf+len, "Signature              = <");
      len+= snprintf(buf+len, 10+1, "%s",(base+0x01));
      len+= sprintf(buf+len, ">\n");
      len+= sprintf(buf+len, "CPU Version High       = 0x%02X\n",(unsigned char)*(base+0x0B));
      len+= sprintf(buf+len, "CPU Version Low        = 0x%02X\n",(unsigned char)*(base+0x0C));
      len+= sprintf(buf+len, "CPU Mode               = 0x%02X\n",(unsigned char)*(base+0x0D));
      len+= sprintf(buf+len, "\n");
      len+= sprintf(buf+len, "Out Start Code      0  = 0x%02X\n",(unsigned char)*(base+0x0E));
      len+= sprintf(buf+len, "Out Start Code      1  = 0x%02X\n",(unsigned char)*(base+0x0F));
      len+= sprintf(buf+len, "Out Channel Count   01 = %05u\n",(unsigned char)*(base+0x11) + 256 * (unsigned char)*(base+0x10));
      len+= sprintf(buf+len, "Out Break Time      01 = %05u\n",(unsigned char)*(base+0x13) + 256 * (unsigned char)*(base+0x12));
      len+= sprintf(buf+len, "Out Break Count     01 = %05u\n",(unsigned char)*(base+0x15) + 256 * (unsigned char)*(base+0x14));
      len+= sprintf(buf+len, "Out Idle  Time      01 = %05u\n",(unsigned char)*(base+0x17) + 256 * (unsigned char)*(base+0x16));
      len+= sprintf(buf+len, "\n");
      len+= sprintf(buf+len, "In  Start Code      0* = 0x%02X\n",(unsigned char)*(base+0x18));
      len+= sprintf(buf+len, "In  Channels Count  0  = %05u\n",(unsigned char)*(base+0x1B) + 256 * (unsigned char)*(base+0x1A));
      len+= sprintf(buf+len, "In  Break Count     0* = %05u\n",(unsigned char)*(base+0x1F) + 256 * (unsigned char)*(base+0x1E));
      len+= sprintf(buf+len, "\n");
      len+= sprintf(buf+len, "In  Start Code      1* = 0x%02X\n",(unsigned char)*(base+0x19));
      len+= sprintf(buf+len, "In  Channels Count  1  = %05u\n",(unsigned char)*(base+0x1D) + 256 * (unsigned char)*(base+0x1C));
      len+= sprintf(buf+len, "In  Break Count     1* = %05u\n",(unsigned char)*(base+0x21) + 256 * (unsigned char)*(base+0x20));
      len+= sprintf(buf+len, "\n");
    }
  }

  *start = buf + offset;
  if (len <= offset+count) *eof = 1;
  len -= offset;
  if (len > count) len = count;
  if (len < 0) len = 0;
  return len;
}

/* ------------------------------------------------------------------------------------------- */

#ifndef DMXPCI_DMX4LINUX
/*
 * File operations
 */

/* LSEEK */
/* allow to seek in the specified region (set_bar) */

DMXPCI_STATIC long long dmxpci_lseek (struct file *file, long long offset, int whence)
{
  dmxpci_board_t *brd = (dmxpci_board_t *)file->private_data;
  long long temp;

  DO_DEBUG(DMXPCI_DEBUG_FUNCTION, printk(DMXPCI_DEBUG "dmxpci_lseek: offset=%lli whence=%d\n", offset,whence));

  /* Check for overflow */
  if (brd == NULL) return -EINVAL;
  if (brd->minor == 0) return -EINVAL;
  if (set_bar < 0 || set_bar >= DEVICE_COUNT_RESOURCE) return -EINVAL;
  if (!(brd->bar_flags[set_bar] & DMXPCI_BAR_FLAG_VALID)) return -EINVAL;
  switch (whence)
  {
    case 0: /* SEEK_SET */
      if (offset <= brd->bar_len[set_bar])
        file->f_pos = offset;
      return file->f_pos;

    case 1: /* SEEK_CUR */
      if ((temp = file->f_pos + offset) <= brd->bar_len[set_bar] && temp >= 0)
        file->f_pos = temp;
      return file->f_pos;

    case 2: /* SEEK_END */
      if (offset <= brd->bar_len[set_bar])
        file->f_pos = brd->bar_len[set_bar] - offset;
      return file->f_pos;

    default:
      return -EINVAL;
  }
}

/* READ */
/* allow to read the specified region (set_bar) */

DMXPCI_STATIC ssize_t dmxpci_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
  u32 real, ret, loop;
  long remain;

  dmxpci_board_t *brd = (dmxpci_board_t *)file->private_data;

  DO_DEBUG(DMXPCI_DEBUG_FUNCTION, printk(DMXPCI_DEBUG "dmxpci_read: asked %d bytes at %d \n", count,(int)*ppos));

  /* Check for overflow */
  if (brd == NULL) return -EINVAL;
  if (brd->minor == 0) return -EINVAL;
  if (set_bar < 0 || set_bar >= DEVICE_COUNT_RESOURCE) return -EINVAL;
  if (!(brd->bar_flags[set_bar] & DMXPCI_BAR_FLAG_VALID)) return -EINVAL;
  remain = brd->bar_len[set_bar] - *ppos;
  real = (count <= remain) ? count : remain;

  if (real<=0) return 0;

  /* copy_to_user seems to be optimized and will no more access the card byte wise with big chunks */
#ifdef DMXPCI_RW_LOOP
  ret = 0;
  for (loop = 0; loop < real; loop++)
    ret += copy_to_user(buf+loop, (char*)(brd->bar_addr[set_bar]) + (u32)(*ppos) + loop, 1);
#else
  ret = copy_to_user(buf, (char*)(brd->bar_addr[set_bar]) + (u32)(*ppos), real);
#if 0
  ret = copy_to_user(buf, (char*)pci_resource_start(brd->dev,set_bar)+(u32)*ppos,real);
#endif
#endif
  real -= ret;

  DO_DEBUG(DMXPCI_DEBUG_COPY, printk(DMXPCI_DEBUG "dmxpci_read: copy_to_user returns %d > real count is %d \n", ret,real));

  *ppos += real;
  return real;
}

/* WRITE */
/* allow to write the specified region (set_bar) */

DMXPCI_STATIC ssize_t dmxpci_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
  u32 real, ret, loop;
  long remain;

  dmxpci_board_t *brd = (dmxpci_board_t *)file->private_data;

  DO_DEBUG(DMXPCI_DEBUG_FUNCTION, printk(DMXPCI_DEBUG "dmxpci_write: %d bytes\n", count));

  /* Check for overflow */
  if (brd == NULL) return -EINVAL;
  if (brd->minor == 0) return -EINVAL;
  if (set_bar < 0 || set_bar >= DEVICE_COUNT_RESOURCE) return -EINVAL;
  if (!(brd->bar_flags[set_bar] & DMXPCI_BAR_FLAG_VALID)) return -EINVAL;
  remain = brd->bar_len[set_bar] - *ppos;
  real = (count <= remain) ? count : remain;

  if (real<=0) return 0;

  /* copy_from_user seems to be optimized and will no more access the card byte wise with big chunks */
#ifdef DMXPCI_RW_LOOP
  ret = 0;
  for (loop = 0; loop < real; loop++)
    ret += copy_from_user((char*)(brd->bar_addr[set_bar]) + (u32)(*ppos) + loop, buf+loop, 1);
#else
  ret = copy_from_user((char*)(brd->bar_addr[set_bar]) + (u32)(*ppos), buf, real);
#endif
  real -= ret;

  *ppos += real;
  return real;
}

/* OPEN */

DMXPCI_STATIC int dmxpci_open(struct inode *inode, struct file *file)
{
  int minor = MINOR(inode->i_rdev);
  struct list_head *cur;
  dmxpci_board_t *brd;

  DO_DEBUG(DMXPCI_DEBUG_FUNCTION ,printk(DMXPCI_DEBUG "dmxpci_open()\n"));

  if (!minor)
  {
    file->private_data = NULL;
    return 0;
  }

  list_for_each(cur, &(dmxpci_drv_data.link))
  {
    brd = list_entry(cur, dmxpci_board_t, link);

    if (brd->minor == minor)
    {
      file->private_data = brd;
      file->f_pos = 0;
      return 0;
    }
  }
  DO_DEBUG(DMXPCI_DEBUG_WARN_2, printk(DMXPCI_WARN "dmxpci: minor %d not found\n", minor));
  return -ENODEV;
}

/* RELEASE */

DMXPCI_STATIC int dmxpci_release(struct inode *inode, struct file *file)
{
  DO_DEBUG(DMXPCI_DEBUG_FUNCTION ,printk(DMXPCI_DEBUG "dmxpci_release()\n"));
  file->private_data = NULL;
  return 0;
}

/* IOCTL */
/* allow to :
     write byte from any region
     read  byte to   any region
     write word from any region (do not use this with BAR2)
     read  word to   any region (do not use this with BAR2)
     wait till next timer and return
     set cpu_mode for (both) interface(s)
     get cpu_mode for (both) interface(s)
     see dmxpci.h dmxpci_ioctl.c and this function code to learn command building */

DMXPCI_STATIC int dmxpci_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
  dmxpci_board_t *brd = (dmxpci_board_t *)file->private_data;
  int bar;
  int mem;
  u32 port   = 0;
  void* addr = NULL;
  u32 uoffset;
#ifdef DMXPCI_DMX4LINUX
  dmxpci_interface_t *dgm_if;
  unsigned char c = 0;
#endif

  /* check the board strucutre */
  if (brd == NULL) return -EINVAL;
  if (brd->minor == 0) return -EINVAL;

  if (cmd & DMXPCI_IOCTL_SIGN) return -EINVAL;
  if (cmd & DMXPCI_IOCTL_OPER)
  {
    bar     = (cmd & DMXPCI_IOCTL_BARNO) >> 24;
    uoffset = (cmd & DMXPCI_IOCTL_ADDR);
    if (bar < 0 || bar > DEVICE_COUNT_RESOURCE) return -EINVAL;
    if (! (brd->bar_flags[bar] & DMXPCI_BAR_FLAG_VALID)) return -EINVAL;
    mem = (brd->bar_flags[bar] & DMXPCI_BAR_FLAG_MEM);
    if(mem)
    {
      addr = brd->bar_addr[bar];
      if (addr == NULL) return -EINVAL;
      if (uoffset+((cmd & DMXPCI_IOCTL_WORD) ? 4 : 1) > brd->bar_len[bar]) return -EINVAL;
      addr += uoffset;
    }
    else
    {
      port = (int)brd->bar_addr[bar];
      if (port == 0) return -EINVAL;
      if (uoffset+((cmd & DMXPCI_IOCTL_WORD) ? 4 : 1) > brd->bar_len[bar]) return -EINVAL;
      port += uoffset;
    }

    if (cmd & DMXPCI_IOCTL_WRITE)
    {
      /* write */
      if (cmd & DMXPCI_IOCTL_WORD)
      {
        if(mem)
        {
          writel((u32)arg,addr);
          return readl(addr);
        }
        else
        {
          outl((u32)arg,port);
          return inl(port);
        }
      }
      else
      {
        if(mem)
        {
          writeb((u8)arg,addr);
          return readb(addr);
        }
        else
        {
          outb((u8)arg,port);
          return inb(port);
        }
      }
    }
    else
    {
      /* read */
      if (cmd & DMXPCI_IOCTL_WORD)
      {
        if (mem)
        {
          return readl(addr);
        }
        else
        {
          return inl(port);
        }
      }
      else
      {
        if(mem)
        {
          return readb(addr);
        }
        else
        {
          return inb(port);
        }
      }
    }
  }
  else switch (cmd)
  {
    case DMXPCI_CMD_TCB:
      if (set_timer && set_callback && brd->stop_async == 0)
      {
#if 0
	/* doj: fix */
        interruptible_sleep_on(&(brd->timer_wq));
#endif
        return brd->timer_count;
      }
      else return -EINVAL;
#ifdef DMXPCI_DMX4LINUX
    case DMXPCI_CMD_SET_MODE:
      DO_DEBUG(DMXPCI_DEBUG_TEMP_2, printk(DMXPCI_DEBUG "dmxpci: ioctl command SET MODE %lu\n", arg));
      if ((dgm_if = brd->dgm_if[0]))
      {
        c = (unsigned char)(arg & 7);
        if (c>5) return -EINVAL;
        DO_DEBUG(DMXPCI_DEBUG_TEMP_2, printk(DMXPCI_DEBUG "dmxpci: ioctl set mode to interface 0 mode %i\n", c));
        dmxpci_update_mode(dgm_if, c);
      }
      if ((dgm_if = brd->dgm_if[1]))
      {
        c = (unsigned char)((arg>>4) & 7);
        if (c>5) return -EINVAL;
        DO_DEBUG(DMXPCI_DEBUG_TEMP_2, printk(DMXPCI_DEBUG "dmxpci: ioctl set mode to interface 1 mode %i\n", c));
        dmxpci_update_mode(dgm_if, c);
      }
      if ((dgm_if = brd->dgm_if[0]))
        c = (unsigned char)(dgm_if->mode);
      if ((dgm_if = brd->dgm_if[1]))
        c += (unsigned char)(dgm_if->mode<<4);
      return c;
    case DMXPCI_CMD_GET_MODE:
      if ((dgm_if = brd->dgm_if[0]))
        c = (unsigned char)(dgm_if->mode);
      if ((dgm_if = brd->dgm_if[1]))
        c += (unsigned char)(dgm_if->mode<<4);
      DO_DEBUG(DMXPCI_DEBUG_TEMP_2, printk(DMXPCI_DEBUG "dmxpci: ioctl command GET MODE %i\n",c));
      return c;
#endif
    default:
      DO_DEBUG(DMXPCI_DEBUG_WARN_1, printk(DMXPCI_WARN "dmxpci: 0x%x unsupported ioctl command\n", cmd));
      return -EINVAL;
  }
  return 0;
}

/* MMAP */

DMXPCI_STATIC void dmxpci_vma_open(struct vm_area_struct *vma)
{;/* MOD_INC_USE_COUNT;*/ }

DMXPCI_STATIC void dmxpci_vma_close(struct vm_area_struct *vma)
{;/* MOD_DEC_USE_COUNT;*/ }

DMXPCI_STATIC struct vm_operations_struct dmxpci_vma_ops = {
    open:  dmxpci_vma_open,
    close: dmxpci_vma_close,
};

/* allow to mmap the specified region (set_bar) - do not allow other length than effecive region length */
DMXPCI_STATIC int dmxpci_mmap(struct file *file, struct vm_area_struct *vma)
{
  dmxpci_board_t *brd = (dmxpci_board_t *)file->private_data;

  /* check the board structure */
  if (brd == NULL) return -EINVAL;
  if (brd->minor == 0) return -EINVAL;

  {
#if 0
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
#endif
  u32 len;
  struct pci_dev* dev = brd->dev;
  u32 bar_start = (u32) pci_resource_start(dev, set_bar);
  u32 bar_len   = (u32) pci_resource_len(dev, set_bar);

  DO_DEBUG(DMXPCI_DEBUG_FUNCTION, printk(DMXPCI_DEBUG "dmxpci_mmap() bar = %d addr = %x\n",set_bar,(u32)brd->bar_addr[set_bar]));

  vma->vm_flags |= VM_IO;
  vma->vm_flags |= VM_RESERVED;
  len = vma->vm_end-vma->vm_start;
#if 0
  if (len != brd->bar_len[set_bar]) return -EINVAL;
#endif
  if (len != bar_len) return -EINVAL;
#if 0
  if (remap_page_range(vma->vm_start, (u32)(brd->bar_addr[set_bar]), len ,vma->vm_page_prot)) return -EAGAIN;
#endif
  if(remap_page_range(vma->vm_start, bar_start, bar_len, vma->vm_page_prot)) return -EAGAIN;

  vma->vm_ops = &dmxpci_vma_ops;
  dmxpci_vma_open(vma);
  }
  return 0;
}

/* FOPS */
static struct file_operations dmxpci_fops = {
  owner:    THIS_MODULE,
  llseek:   dmxpci_lseek,
  read:     dmxpci_read,
  write:    dmxpci_write,
  open:     dmxpci_open,
  ioctl:    dmxpci_ioctl,
  release:  dmxpci_release,
#if 0
  mmap:     dmxpci_mmap,
#endif
};
#endif // DMXPCI_DMX4LINUX

/* ------------------------------------------------------------------------------------------- */

/*
 * PCI handling
 */

/* PROBE */

DMXPCI_STATIC int __devinit dmxpci_probe(struct pci_dev *dev, const struct pci_device_id *devid)
{
#ifdef DMXPCI_DMX4LINUX
  DMXDriver *dmx_drv;
#endif
  int i;
  int ret = 0;
  u32 bar_start;
  u32 bar_stop;
  u32 bar_len;
  u32 bar_flags;
  char devfs_name[20];
  dmxpci_board_t *brd;

  DO_DEBUG(DMXPCI_DEBUG_INFO_1, printk(DMXPCI_INFO "dmxpci: found %s\n", dmxpci_board_info[devid->driver_data].name));

  /* Allocate a private structure and reference it as driver's data */
  brd = (dmxpci_board_t *)kmalloc(sizeof(dmxpci_board_t), GFP_KERNEL);
  if (brd == NULL)
  {
    DO_DEBUG(DMXPCI_DEBUG_WARN_2, printk(DMXPCI_WARN "dmxpci: unable to allocate private structure\n"));

    ret = -ENOMEM;
    goto cleanup_kmalloc;
  }

  pci_set_drvdata(dev, brd);

  /* Init private field */

  brd->timer_delay = 0;
  brd->timer_count = 0;
  brd->timer_param = DMXPCI_TIMER_PARAM;
  brd->stop_async  = 0;
  brd->dev         = dev;
  brd->type        = devid->driver_data;
  brd->minor       = dmxpci_drv_data.boards + 1;

  init_waitqueue_head(&(brd->stop_wq));
#ifdef DOJ_USE_TIMER_WQ
  init_waitqueue_head(&(brd->timer_wq));
#endif

#ifndef DMXPCI_DMX4LINUX
  DO_DEBUG(DMXPCI_DEBUG_INFO_1, printk(DMXPCI_INFO "dmxpci:  using major %d and minor %d for this device\n", major, brd->minor));
#endif

  /* Initialize device before it's used by the driver */
  ret = pci_enable_device(dev);
  if (ret < 0)
  {
    DO_DEBUG(DMXPCI_DEBUG_WARN_2, printk(DMXPCI_WARN "dmxpci: unable to initialize PCI device\n"));
    goto cleanup_pci_enable;
  }

  /* Reserve PCI I/O and memory resources */
  ret = pci_request_regions(dev, "dmxpci");
  if (ret < 0)
  {
    DO_DEBUG(DMXPCI_DEBUG_WARN_2, printk(DMXPCI_WARN "dmxpci: unable to reserve PCI resources\n"));
    goto cleanup_regions;
  }

  /* Inspect PCI BARs and remap I/O memory */
  for (i=0; i < DEVICE_COUNT_RESOURCE; i++)
  {
    bar_start = (u32) pci_resource_start(dev, i);
    bar_stop  = (u32) pci_resource_end(dev, i);
    bar_len   = (u32) pci_resource_len(dev, i);
    bar_flags = (u32) pci_resource_flags(dev, i);

    brd->bar_addr[i]  = (void*)bar_start;
    brd->bar_len[i]   = bar_len;
    brd->bar_flags[i] = 0;

    if (pci_resource_start(dev, i) != 0)
    {
      brd->bar_flags[i] |= DMXPCI_BAR_FLAG_VALID;
      DO_DEBUG(DMXPCI_DEBUG_INFO_2, printk(DMXPCI_INFO "dmxpci: BAR %d (%#08x-%#08x), len=%d, flags=%#08x\n",
             i,
             bar_start,
             bar_stop,
             bar_len,
             bar_flags));
    }
    if (bar_flags & IORESOURCE_MEM)
    {
      char *base;
      /* MEM > remap */
      brd->bar_addr[i] = ioremap(bar_start,bar_len);
      if (brd->bar_addr[i] == NULL)
      {
        DO_DEBUG(DMXPCI_DEBUG_WARN_2, printk(DMXPCI_WARN "dmxpci: unable to remap I/O memory\n"));
        ret = -ENOMEM;
        goto cleanup_ioremap;
      }
      brd->bar_flags[i] |= DMXPCI_BAR_FLAG_MEM;
      DO_DEBUG(DMXPCI_DEBUG_INFO_2,printk(DMXPCI_INFO "dmxpci: BAR %d I/O memory has been remaped at %#08x\n",i, (u32)brd->bar_addr[i]));
      base = (unsigned char *)(brd->bar_addr[i]);
      if (brd->bar_len[i] == 8192 && memcmp("DMXOEM",base+0x01,6)==0)
      {
        brd->bar_flags[i] |= DMXPCI_BAR_FLAG_DMXOEM;
        DO_DEBUG(DMXPCI_DEBUG_INFO_1,printk(DMXPCI_INFO "dmxpci:  region %d is 8192 bytes long and is signed 'DMXOEM'\n",i));
        set_bar = i;
      }
    }
    else
    {
      DO_DEBUG(DMXPCI_DEBUG_INFO_2,printk(DMXPCI_INFO "dmxpci: BAR %d is I/O ports\n",i));
      brd->bar_flags[i] |= DMXPCI_BAR_FLAG_IO;
    }
  }

  /* Timer init */

  if (set_timer)
  {
    DO_DEBUG(DMXPCI_DEBUG_TIMER, printk(DMXPCI_DEBUG "dmxpci: SET TIMER > init & fill struct !\n"));

    brd->timer_delay = HZ / timer_freq;
    if (brd->timer_delay < 1) brd->timer_delay = 1;
    init_timer(&brd->timer_tl);
    brd->timer_tl.expires  = jiffies + brd->timer_delay * 100;
    brd->timer_tl.data     = (unsigned long)brd;
    brd->timer_tl.function = dmxpci_timer_function;

    DO_DEBUG(DMXPCI_DEBUG_TIMER, printk(DMXPCI_DEBUG "dmxpci: SET TIMER > add_timer !\n"));

    add_timer(&brd->timer_tl);

    DO_DEBUG(DMXPCI_DEBUG_TIMER, printk(DMXPCI_DEBUG "dmxpci: SET TIMER > end !\n"));
  }

  /* Proc init */

  if (set_proc)
  {
    sprintf(devfs_name, "driver/dmxpci_%d", brd->minor);
    create_proc_read_entry(devfs_name, 0, NULL, dmxpci_proc_read, brd);

    DO_DEBUG(DMXPCI_DEBUG_INFO_2,printk(DMXPCI_INFO "dmxpci: SET PROC <%s> !\n",devfs_name));
  }

  /* Link the new dmxpci_board_t structure with others */
  list_add_tail(&brd->link, &(dmxpci_drv_data.link));

  /* update board count */
  dmxpci_drv_data.boards++;

  /* link struct */

  brd->dgm_drv = &dmxpci_drv_data;
  brd->dgm_if[0] = NULL;
  brd->dgm_if[1] = NULL;

#ifdef DMXPCI_DMX4LINUX

  /* build dmx4linux interface(s) */

  dmx_drv = dmxpci_drv_data.dmx_drv;
  if (!dmx_drv)
  {
    DO_DEBUG(DMXPCI_DEBUG_WARN_1, printk(DMXPCI_WARN "driver is NULL while probing board !\n"));
    ret = -1;
    goto cleanup_ioremap;
  }
  for (i = 0; i <= brd->type ; i++)
  {
    DMXInterface *dmx_if = dmx_drv->create_interface(dmx_drv, dmxproplist_vacreate("minor=%l,interface=%l",brd->minor,i));
    if (!dmx_if)
    {
      DO_DEBUG(DMXPCI_DEBUG_WARN_1, printk(DMXPCI_WARN "unable to create DMXInterface while probing board !\n"));
      ret = -2;
      goto cleanup_ioremap;
    }
  }

#endif

  return 0;

 cleanup_ioremap:
  for (i--; i >= 0; i--)
  {
    if (brd->bar_addr[i] != NULL)
      iounmap(brd->bar_addr[i]);
  }
  pci_release_regions(dev);
 cleanup_regions:
  pci_disable_device(dev);
 cleanup_pci_enable:
  kfree(brd);
 cleanup_kmalloc:
  return ret;
}

/* REMOVE */

DMXPCI_STATIC void __devexit dmxpci_remove(struct pci_dev *dev)
{
  int i;
  char proc_name[20];
  dmxpci_board_t *brd = pci_get_drvdata(dev);
#ifdef DMXPCI_DMX4LINUX
  dmxpci_interface_t *dgm_if;
#endif

  DO_DEBUG(DMXPCI_DEBUG_INFO_2, printk(DMXPCI_INFO "dmxpci: remove board with minor %d\n",brd->minor));
#if 0
  schedule();
#endif

  if (set_timer && brd->timer_count)
  {
    brd->stop_async = 3;
    while (brd->stop_async == 3)
      wait_event_interruptible(brd->stop_wq,brd->stop_async == 4);
    del_timer(&brd->timer_tl);
  }
  if (set_proc)
  {
    sprintf(proc_name, "driver/dmxpci_%d", brd->minor);
    remove_proc_entry(proc_name, NULL);
  }

#ifdef DMXPCI_DMX4LINUX
  for (i = 0; i <= brd->type ; i++)
  {
    if((dgm_if = brd->dgm_if[i]) && dgm_if->dmx_if)
      dgm_if->dmx_if->delete(dgm_if->dmx_if);
  }
#endif

  for (i=0; i < DEVICE_COUNT_RESOURCE; i++)
    if (brd->bar_flags[i] != DMXPCI_BAR_FLAG_MEM)
      iounmap(brd->bar_addr[i]);

  pci_release_regions(dev);

  pci_disable_device(dev);

  list_del(&brd->link);

  dmxpci_drv_data.boards--;

  kfree(brd);
}

/* PCI_DRIVER */

static struct pci_driver dmxpci_pci_driver = {
  name:     "slh1514pci",
  id_table:  dmxpci_id_table,
  probe:     dmxpci_probe,    /* Init one device */
  remove:    dmxpci_remove,   /* Remove one device */
};

/* ------------------------------------------------------------------------------------------- */

/*
 * Init and Exit
 */

/* INIT */

DMXPCI_STATIC int __init dmxpci_init(void)
{
  int ret;

  /* Init private struct */

  /* this should be like kernel's INIT_LIST_HEAD(&(dmxpci_drv_data.link)) */
  dmxpci_drv_data.link.next = &dmxpci_drv_data.link;
  dmxpci_drv_data.link.prev = &dmxpci_drv_data.link;

  dmxpci_drv_data.minor     = 0; /* flag for the driver list item */
  dmxpci_drv_data.boards    = 0; /* init boards count */

  /* Create dmx4l family & driver */

#ifdef DMXPCI_DMX4LINUX
    DO_DEBUG(DMXPCI_DEBUG_INFO_1, printk(DMXPCI_INFO "dmxpci: module insertion with DMX4LINUX support !\n"));
  dmxpci_drv_data.dmx_f = dmx_create_family ("PCI");
  if (dmxpci_drv_data.dmx_f)
  {
    dmxpci_drv_data.dmx_drv = dmxpci_drv_data.dmx_f->create_driver (dmxpci_drv_data.dmx_f, "slh1514", dmxpci_create_universe, NULL);
    if (!dmxpci_drv_data.dmx_drv)
    {
      dmxpci_drv_data.dmx_f->delete(dmxpci_drv_data.dmx_f, 0);
      DO_DEBUG(DMXPCI_DEBUG_WARN_2, printk(DMXPCI_WARN "dmxpci: cannot create dmx_driver !\n"));
  	  return -1;
    }
    dmxpci_drv_data.dmx_drv->user_create_interface = dmxpci_create_interface;
  }
  else
  {
    DO_DEBUG(DMXPCI_DEBUG_WARN_2, printk(DMXPCI_WARN "dmxpci: cannot create dmx_family !\n"));
    return -1;
  }
#else
    DO_DEBUG(DMXPCI_DEBUG_INFO_1, printk(DMXPCI_INFO "dmxpci: module insertion without dmx4linux support !\n"));
#endif

  /* Register PCI driver */

  ret = pci_module_init(&dmxpci_pci_driver);
  if (ret < 0)
  {
    DO_DEBUG(DMXPCI_DEBUG_WARN_2, printk(DMXPCI_WARN "dmxpci: unable to register PCI driver\n"));
    dmxpci_drv_data.dmx_drv->delete(dmxpci_drv_data.dmx_drv);
    return ret;
  }

  /* Set up proc entry */
  if (set_proc)
  {
    create_proc_read_entry("driver/dmxpci", 0, NULL, dmxpci_proc_read, NULL);
    DO_DEBUG(DMXPCI_DEBUG_INFO_2, printk(DMXPCI_INFO "dmxpci: SET PROC for driver !\n"));
  }
  return 0;
}

/* EXIT */

DMXPCI_STATIC void __exit dmxpci_exit(void)
{
  DO_DEBUG(DMXPCI_DEBUG_INFO_1, printk(DMXPCI_INFO "dmxpci: module remove !\n"));
  pci_unregister_driver(&dmxpci_pci_driver);
  if (set_proc)
    remove_proc_entry("driver/dmxpci", NULL);
#ifdef DMXPCI_DMX4LINUX
  if (dmxpci_drv_data.dmx_f)
    dmxpci_drv_data.dmx_f->delete(dmxpci_drv_data.dmx_f, 0);
#else
  devfs_unregister_chrdev(major, "dmxpci");
#endif
}

/*
 * Module entry points
 */

module_init(dmxpci_init);
module_exit(dmxpci_exit);
