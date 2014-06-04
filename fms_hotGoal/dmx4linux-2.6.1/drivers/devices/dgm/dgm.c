/*
 * dgm.c : driver for Digimedia - Soundlight 1514 & 2514 & 2512 DMX PCI & PCMCIA boards
 *
 * Copyright (C) 2004 Bastien Andres <bastos@balelec.ch>
 *
 * Based on code from :
 *
 * Generic PCI Driver :
 *
 * Julien Gaulmin <julien.gaulmin@fr.alcove.com>, Alc?ve
 * Pierre Ficheux (pierre@ficheux.com)
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

#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

/*
 * Includes
 */

#include <linux/kernel.h>           /* printk() */
#include <linux/module.h>           /* modules */
#include <linux/config.h>
#include <linux/init.h>             /* module_{init,exit}() */
#include <linux/slab.h>             /* kmalloc()/kfree() */
#include <linux/list.h>             /* list_*() */
#include <linux/types.h>            /* size_t */
#include <asm/page.h>
#include <asm/semaphore.h>

#include <dmx/dmxdev.h>

#include "dgm.h"

/*
 * Arguments
 */

static int debug_flags=7;   /* debug messages trigger */

MODULE_DESCRIPTION("DMX4Linux driver for DGM - SLH 2512-1514-2514 boards (dgm)");
MODULE_AUTHOR("Bastien Andres");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("Digimedia MLS 2512 1514 2514");
MODULE_PARM_DESC(debug_flags,    "Set trigger for debugging messages (see dgm.h)");
module_param(debug_flags, int, S_IRUGO);

/*
 * Global variables
 */

static dgm_driver_t dgm_drv_data;

/* ------------------------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------ */
/* Functions to handle memory access and properties */
/* ------------------------------------------------------------------------ */
DGM_STATIC inline unsigned short shortwritetocard (unsigned short *_x, unsigned short v)
{
  unsigned char *x = (unsigned char *)_x;
  x[0]=(v>>8)&0xff;
  x[1]=v&0xff;
  return v;
}

DGM_STATIC inline unsigned short shortreadfromcard (unsigned short *_x)
{
  unsigned char *x = (unsigned char *)_x;
  return x[0]<<8 | x[1];
}

DGM_STATIC int byteptr_get_long (DMXProperty *p, long *val)
{
  if (p && p->data && val)
  {
    *val = *((unsigned char *)p->data);
    return 0;
  }
  return -1;
}

DGM_STATIC int byteptr_set_long (DMXProperty *p, long val)
{
  if (p && p->data)
  {
    *((unsigned char *)p->data) = val;
    return 0;
  }
  return -1;
}

DGM_STATIC int shortptr_get_long (DMXProperty *p, long *val)
{
  if (p && p->data && val)
  {
    *val = shortreadfromcard (p->data);
    return 0;
  }
  return -1;
}

DGM_STATIC int shortptr_set_long (DMXProperty *p, long val)
{
  if ((uint)p && p->data)
  {
    shortwritetocard (p->data, val);
    return 0;
  }
  return -1;
}

DGM_STATIC int native_shortptr_get_long (DMXProperty *p, long *val)
{
  short *shortp;
  if (p && p->data && val)
  {
    shortp = (short*)p->data;
    *val = *shortp;
    return 0;
  }
  return -1;
}

DGM_STATIC int fake_shortptr_set_long (DMXProperty *p, long val)
{
  return -1;
}

/* our own memcpy to be sure to access the board byte per byte */
#if 1
DGM_STATIC void* dgm_memcpy(void* dest, void* src, size_t size)
{
  int i;
  unsigned char *ucp_dest = (unsigned char*)dest;
  unsigned char *ucp_src = (unsigned char*)src;

  for (i = 0; i < size; i++)
  {
    ucp_dest[i] = ucp_src[i];
  }
  return dest;
}
#else
#define dgm_memcpy memcpy
#endif

/*--------------------------------------------------------
 *-- dmx4linux calls it to write to an universe
 *-------------------------------------------------------*/
/* error if input universe */
DGM_STATIC int dgm_write_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  /* If it is an output universe,
   * then data has changed if a user
   * writes new data.
   */
  dgm_universe_t *dgm_u = NULL;

  DO_DEBUG(DGM_DEBUG_FUNCTION, printk(DGM_DEBUG "dgm : int dgm_write_universe (DMXUniverse *u=%p)\n", u));

  if (u && !u->kind && u->user_data && offs >= 0 && offs < 512)  /* output universe & offset check */
  {
    dgm_u = (dgm_universe_t *)(u->user_data);
    if (offs+size>512)
      size = 512-offs;
    if (!dgm_u->data_pointer) return -1;
    /* copy data to local buffer */
    memcpy (&dgm_u->local_buffer[offs], buff, size);
    /* copy data to card buffer if virtual universe connected to card universe */
    if (dgm_u->mode == 2)
      dgm_memcpy (&dgm_u->data_pointer[offs], buff, size);
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
/* do we copy to buffer ? */
DGM_STATIC int dgm_read_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  dgm_universe_t  *dgm_u = NULL;

  DO_DEBUG(DGM_DEBUG_FUNCTION, printk(DGM_DEBUG "dgm : int dgm_read_universe (DMXUniverse *u=%p)\n", u));

  if (u && u->user_data && offs >= 0 && offs < 512) /* struct & offset check */
  {
    dgm_u = (dgm_universe_t *)(u->user_data);
    if (offs+size>512)
      size = 512-offs;
    if (!dgm_u->data_pointer) return -1;
    /* copy data from card if virtual universe connected to card universe */
    if (dgm_u->mode == 3)
      dgm_memcpy (&dgm_u->local_buffer[offs], &dgm_u->data_pointer[offs], size);
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
DGM_STATIC int dgm_data_available (DMXUniverse *u, uint start, uint size)
{
  dgm_universe_t  *dgm_u = NULL;

  DO_DEBUG(DGM_DEBUG_FUNCTION,printk(DGM_DEBUG "dgm : int dgm_data_available (DMXUniverse *u=%p, uint start=%d, uint size=%d)\n", u, start, size))
  if (u && u->user_data)
  {
    dgm_u = (dgm_universe_t *)(u->user_data);
    return dgm_u->data_avail;
  }
  return -1;
}

/*--------------------------------------------------------
 *-- this check one input universe for new values
 *-------------------------------------------------------*/
DGM_STATIC int dgm_check_universe (dgm_universe_t * dgm_u, unsigned short fc)
{
  int             j;
  int             first = -1;
  int             last  = 0;
  DMXUniverse    *dmx_u;
  unsigned char   val;

  DO_DEBUG(DGM_DEBUG_TIMER, printk(DGM_DEBUG "dgm : int dgm_check_universe (dgm_universe_t *dgm_u=%p)\n", dgm_u));

  if (!dgm_u)
  {
    DO_DEBUG(DGM_DEBUG_WARN_1, printk(DGM_WARN "dgm : dgm_check_universe received a NULL universe struct\n"));
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
  DO_DEBUG(DGM_DEBUG_TIMER, printk(DGM_DEBUG "dgm : universe <%d> checked interval=<%d,%d>\n", dgm_u->index,first,last));
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
DGM_STATIC int dgm_check_receiver (dgm_board_t *brd)
{
  int i;
  int                 mode;
  unsigned short      fc=0;
  dgm_interface_t *dgm_if = NULL;
  dgm_universe_t  *dgm_u  = NULL;

  DO_DEBUG(DGM_DEBUG_TIMER, printk(DGM_DEBUG "dgm : int dgm_check_receiver (dgm_board_t *brd=%p)\n", brd));

  if (!brd)
  {
    DO_DEBUG(DGM_DEBUG_WARN_1, printk(DGM_WARN "dgm : dgm_check_receiver received a NULL board struct\n"));
    return -1;
  }

  for (i = 0 ; i < 2 ; i++)
  {
    if((dgm_if = brd->dgm_if[i]))
    {
      mode = dgm_if->mode;
      /* DO_DEBUG(DGM_DEBUG_TIMER, printk(DGM_DEBUG "dgm : check_receiver interface=%d mode=%d\n", i,mode)); */
      switch (mode)
      {
        case 0: /* Idle-Idle */
          break;
        case 1: /* Out-Idle */
          break;
        case 2: /* Idle-In */
          /* fall thru to case 3 which is identical to treat */
        case 3: /* Out-In */
          dgm_u = dgm_if->dgm_u[3];
          fc = shortreadfromcard(dgm_u->framecount);
          if (fc != dgm_u->last_framecount)
            dgm_check_universe(dgm_u, fc);
          break;
        case 4: /* Out-Out */
          break;
        case 5: /* In-In */
          /* in mode 5 framecount is not updated so we always check - the user will need to choose timer delay ;-)*/
          fc = shortreadfromcard(dgm_if->dgm_u[3]->framecount);
          dgm_check_universe(dgm_if->dgm_u[2], fc);
          dgm_check_universe(dgm_if->dgm_u[3], fc);
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
DGM_STATIC void dgm_update_mode (dgm_interface_t *dgm_if, unsigned char mode)
{
  unsigned int   i;
  unsigned short m[4];
  unsigned short cc1,cc2;

  DO_DEBUG(DGM_DEBUG_FUNCTION, printk(DGM_DEBUG "dgm : void dgm_update_mode (dgm_interface_t *dgm_if=%p, mode=%d )\n", dgm_if,mode));

  m[0] = 0;
  m[1] = 0;
  m[2] = 1;
  m[3] = 1;
  switch (mode)
  {
    case 0:
      break;
    case 1:
      m[0] = 2;
      shortwritetocard(dgm_if->dgm_u[0]->channels,dgm_if->dgm_u[0]->channels_count);
      break;
    case 2:
      m[3] = 3;
      break;
    case 3:
      m[0] = 2;
      m[3] = 3;
      shortwritetocard(dgm_if->dgm_u[0]->channels,dgm_if->dgm_u[0]->channels_count);
      break;
    case 4:
      m[0] = 2;
      m[1] = 2;
      cc1 = dgm_if->dgm_u[0]->channels_count;
      cc2 = dgm_if->dgm_u[1]->channels_count;
      shortwritetocard(dgm_if->dgm_u[0]->channels,(cc1>cc2)?cc1:cc2);
      break;
    case 5:
      m[2] = 3;
      m[3] = 3;
      break;
  }

  DO_DEBUG(DGM_DEBUG_TEMP_1, printk(DGM_DEBUG "dgm : ... will set mode ...\n"));
  dgm_if->mode = mode;
  dgm_if->mem->cpu_mode = mode;
  for (i = 0; i<4; i++) dgm_if->dgm_u[i]->mode = m[i];
  DO_DEBUG(DGM_DEBUG_TEMP_1, printk(DGM_DEBUG "dgm : ... end of update_mode\n"));
}

/*--------------------------------------------------------
 * Timer handler
 *-------------------------------------------------------*/
DGM_STATIC void dgm_timer_function(unsigned long param)
{
  struct list_head *cur = NULL;
  dgm_board_t      *brd = NULL;
  dgm_driver_t     *drv = (dgm_driver_t *)param;

  drv->timer_count++;
  drv->timer_tl.expires = jiffies + drv->timer_delay;
#if 0
  drv->timer_tl.expires = jiffies + 500; /* when it comes to test with slow debug messages stream ;-) */
#endif

  list_for_each(cur, &(dgm_drv_data.link))
  {
#if 0
    brd = (dgm_board_t*)cur; /* as link is first field mabe we could simlpify */
#endif
    brd = list_entry(cur, dgm_board_t, link);
    if ((int)brd->minor != 0)
      dgm_check_receiver(brd);
  }

  /* if the timer is to be killed stop_async will be set to 3 so we
     don't register again and ack by setting value 4 */

  if (drv->stop_async == 3)
  {
    wake_up_interruptible(&(drv->stop_wq));
    drv->stop_async = 4;
  }
  else
  {
    add_timer(&drv->timer_tl);
  }
}

/*--------------------------------------------------------
 *-- property callback to get cpu_mode
 *-------------------------------------------------------*/
DGM_STATIC int dgm_get_cpumode (DMXProperty *prop, long *value)
{
  dgm_interface_t *dgm_if = NULL;

  DO_DEBUG(DGM_DEBUG_TEMP_1, printk (DGM_DEBUG "dgm : int dgm_get_cpumode() !\n"));

  if (prop == NULL) return -1;
  if (prop->data == NULL) return -3;
  if (value == NULL) return -4;
  dgm_if = (dgm_interface_t*)prop->data;
  *value = dgm_if->mode;
  return 0;
}

/*--------------------------------------------------------
 *-- property callback to set cpu_mode
 *-------------------------------------------------------*/
DGM_STATIC int dgm_set_cpumode (DMXProperty *prop, long value)
{
  dgm_interface_t *dgm_if = NULL;

  DO_DEBUG(DGM_DEBUG_TEMP_1, printk (DGM_DEBUG "dgm : int dgm_set_cpumode() !\n"));

  if (prop == NULL) return -1;
  if (prop->data == NULL) return -3;
  dgm_if = (dgm_interface_t*)prop->data;
  if (value < 0 || value > 5) return -6;
  dgm_update_mode (dgm_if, (unsigned char)value);
  return 0;
}

/*--------------------------------------------------------
 *-- property callback to get timer_freq
 *-------------------------------------------------------*/
DGM_STATIC int dgm_get_timer_freq (DMXProperty *prop, long *value)
{
  dgm_driver_t * drv = NULL;

  DO_DEBUG(DGM_DEBUG_TEMP_1, printk (DGM_DEBUG "dgm : int dgm_get_timer_freq() !\n"));

  if (prop == NULL) return -1;
  if (prop->data == NULL) return -3;
  if (value == NULL) return -4;
  drv = (dgm_driver_t*)prop->data;
  *value = drv->timer_freq;
  return 0;
}

/*--------------------------------------------------------
 *-- property callback to set timer_freq
 *-------------------------------------------------------*/
DGM_STATIC int dgm_set_timer_freq (DMXProperty *prop, long value)
{
  dgm_driver_t * drv = NULL;

  DO_DEBUG(DGM_DEBUG_TEMP_1, printk (DGM_DEBUG "dgm : int dgm_set_timer_freq() !\n"));

  if (prop == NULL) return -1;
  if (prop->name == NULL) return -2;
  drv = (dgm_driver_t*)prop->data;
  drv->timer_freq = value;
  drv->timer_delay = HZ / drv->timer_freq;
  if (drv->timer_delay < 1) drv->timer_delay = 1;

  return 0;
}

/*--------------------------------------------------------
 *-- property callback to get timer_state
 *-------------------------------------------------------*/
DGM_STATIC int dgm_get_timer_state (DMXProperty *prop, long *value)
{
  dgm_driver_t * drv = NULL;

  DO_DEBUG(DGM_DEBUG_TEMP_1, printk (DGM_DEBUG "dgm : int dgm_get_timer_freq() !\n"));

  if (prop == NULL) return -1;
  if (prop->data == NULL) return -3;
  if (value == NULL) return -4;
  drv = (dgm_driver_t*)prop->data;
  *value = drv->timer_state;
  return 0;
}

/*--------------------------------------------------------
 *-- start timer
 *-------------------------------------------------------*/
DGM_STATIC int dgm_start_timer (dgm_driver_t *drv)
{
  if (drv == NULL) return -1;
  if (drv->timer_state == 0 && drv->stop_async == 0)
  {
    DO_DEBUG(DGM_DEBUG_TIMER, printk(DGM_DEBUG "dgm : SET TIMER > init & fill struct !\n"));

    drv->timer_delay = HZ / drv->timer_freq;
    if (drv->timer_delay < 1) drv->timer_delay = 1;
    init_timer(&drv->timer_tl);
    drv->timer_tl.expires  = jiffies + drv->timer_delay * 10;
    drv->timer_tl.data     = (unsigned long)drv;
    drv->timer_tl.function = dgm_timer_function;

    DO_DEBUG(DGM_DEBUG_TIMER, printk(DGM_DEBUG "dgm : SET TIMER > add_timer !\n"));
    add_timer(&drv->timer_tl);
    DO_DEBUG(DGM_DEBUG_TIMER, printk(DGM_DEBUG "dgm : SET TIMER > end !\n"));

    drv->timer_state = 1;
    drv->timer_count = 0;
    return 0;
  }
  return -1;
}

/*--------------------------------------------------------
 *-- stop timer
 *-------------------------------------------------------*/
DGM_STATIC int dgm_stop_timer (dgm_driver_t *drv)
{
  if (drv == NULL) return -1;
  if (drv->timer_state == 1 && drv->stop_async == 0)
  {
    drv->stop_async = 3;
    while (drv->stop_async == 3)
      wait_event_interruptible(drv->stop_wq,drv->stop_async == 4);
    del_timer(&drv->timer_tl);
    drv->stop_async = 0;
    drv->timer_state = 0;
    return 0;
  }
  return -1;
}

/*--------------------------------------------------------
 *-- property callback to start & stop timer
 *-------------------------------------------------------*/
DGM_STATIC int dgm_set_timer_state (DMXProperty *prop, long value)
{
  int ret = 0;
  dgm_driver_t * drv = NULL;

  DO_DEBUG(DGM_DEBUG_TEMP_1, printk (DGM_DEBUG "dgm : int dgm_set_timer_state() !\n"));

  if (prop == NULL) return -1;
  if (prop->data == NULL) return -3;
  drv = (dgm_driver_t*)prop->data;
  if (value != 0) value = 1;
  switch (drv->timer_state<<1 | value)
  {
    case 0x00: /* off -> off */
      /* nothing to do */
      break;
    case 0x01: /* off -> on  */
      ret = dgm_start_timer(drv);
      break;
    case 0x02: /* on  -> off */
      ret = dgm_stop_timer(drv);
      break;
    case 0x03: /* on  -> on  */
      /* nothing to do */
      break;
    default: /* eroneous state -> do nothing */
      ret = -2;
      break;
  }
  return ret;
}

/*--------------------------------------------------------
 *-- setup initial values for dmx parameters
 *-------------------------------------------------------*/
DGM_STATIC int dgm_init_interface (dgm_memory_t *dgm)
{
  DO_DEBUG(DGM_DEBUG_FUNCTION, printk(DGM_DEBUG "sgm : int dgm_init_interface (dgm_memory_t *dgm=%p)\n", dgm));

  if (dgm)
  {
    int i;
    dgm->out_startcode[0] = 0;
    dgm->out_startcode[1] = 0;
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
    DO_DEBUG(DGM_DEBUG_WARN_2, printk (DGM_WARN "dgm : dgm_init_interface received NULL card memory struct\n"));
    return -1;
  }
}

/*--------------------------------------------------------
 *-- called by dmx4linux just before releasing DMXUniverse
 *-- release ressource of dgm_universe_t
 *-------------------------------------------------------*/
DGM_STATIC int dgm_delete_universe (DMXUniverse *u)
{
  dgm_interface_t *dgm_if = NULL;
  dgm_universe_t  *dgm_u  = NULL;

  DO_DEBUG(DGM_DEBUG_FUNCTION, printk (DGM_DEBUG "dgm : int dgm_delete_universe (DMXUniverse *u=%p)\n", u));

  if (u && u->interface && u->interface->user_data && u->user_data)
  {
    dgm_if = (dgm_interface_t *)(u->interface->user_data);
    dgm_u  = (dgm_universe_t *)(u->user_data);

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
DGM_STATIC int dgm_create_universe (DMXUniverse *dmx_u, DMXPropList *pl)
{
  unsigned long index = 666;
  dgm_interface_t *dgm_if = NULL;
  dgm_universe_t *dgm_u   = NULL;
  DMXProperty *p             = NULL;

  DO_DEBUG(DGM_DEBUG_FUNCTION, printk (DGM_DEBUG "dgm : int dgm_create_universe (DMXUniverse *u=%p, DMXPropList *pl=%p)\n", dmx_u, pl));

  if (dmx_u && dmx_u->interface && dmx_u->interface->user_data && dmx_u->kind>=0 && dmx_u->kind<2)
  {
    dgm_if = (dgm_interface_t *)dmx_u->interface->user_data;
    if (pl && pl->find)
    {
      p = pl->find(pl, "u_id");
      if (p)
      {
        p->get_long (p, &index);
      }
    }
    if (index>4)
    {
      DO_DEBUG(DGM_DEBUG_WARN_1, printk(DGM_WARN "dgm : create_universe cannot find index in proplist <%lu>\n",index));
      return -1;
    }

    dgm_u = DMX_ALLOC(dgm_universe_t);
    if (!dgm_u)
    {
      DO_DEBUG(DGM_DEBUG_WARN_1, printk(DGM_WARN "dgm : create_universe cannot allocate universe struct\n"));
      return -1;
    }

    /* set dmx_u fields */

    dmx_u->conn_id = index;
    sprintf (dmx_u->connector, "DGM BRD %d IF %u U %lu %s ", dgm_if->dgm_brd->minor, dgm_if->index,index,dmx_u->kind?"IN":"OUT");

    if (!dmx_u->kind)
      dmx_u->write_slots  = dgm_write_universe;
    dmx_u->read_slots     = dgm_read_universe;
    dmx_u->data_available = dgm_data_available;
    dmx_u->user_delete    = dgm_delete_universe;

    /* set dgm_u fields */

    dgm_u->index = (unsigned short) index;
    dgm_u->mode  = (unsigned short) index>>1; /* will be re set by update mode after */
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

    dgm_u->dgm_if    = dgm_if;
    dgm_u->dmx_u     = dmx_u;

   /*
    * Create a property for the startcode, slots and framecount.
    */

    if (!dmx_u->props)
    {
      DO_DEBUG(DGM_DEBUG_WARN_1, printk(DGM_WARN "dgm : create_universe cannot find dmx_u's proplist\n"));
      return -1;
    }

    pl = dmx_u->props;
    p  = NULL;

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
    p = dmxprop_create_long ("u_mode", 0);
    if (p)
    {
      if (dmxprop_user_long(p, native_shortptr_get_long, fake_shortptr_set_long, &dgm_u->mode) < 0)
        p->delete(p);
      else
        pl->add(pl, p);
    }
    p = pl->find(pl,"slots");
    if(p)
      dmxprop_user_long(p, shortptr_get_long, shortptr_set_long, dgm_u->channels);

    dmx_u->user_data = (void *)dgm_u;
    return 0;
  }
  return -1;
}

/*--------------------------------------------------------
 *-- dgm_delete_interface
 *--
 *-- This is the function called by the dmx4linux layer
 *-- after all universes for that interface are successfully
 *-- deleted and before the interface itself is to be deleted.
 *-- It cleans up anything that is not removed by dmx4linux.
 *-------------------------------------------------------*/
DGM_STATIC int dgm_delete_interface (DMXInterface *i)
{
  dgm_interface_t *dgm_if = NULL;

  DO_DEBUG(DGM_DEBUG_FUNCTION, printk(DGM_DEBUG "dgm : int dgm_delete_interface (DMXInterface *i=%p)\n", i));
  if (i && i->user_data)
  {
    dgm_if = (dgm_interface_t *)i->user_data;
    i->user_data = NULL;
    dgm_if->dgm_brd->dgm_if[dgm_if->index] = NULL;
    DMX_FREE(dgm_if);
    return 0;
  }
  return -1;
}

/*--------------------------------------------------------
 *-- dgm_create_interface
 *--
 *-- This function is called after the internal data
 *-- structures are created and before the interface
 *-- is added.
 *-------------------------------------------------------*/
DGM_STATIC int dgm_create_interface (DMXInterface *dmx_if, DMXPropList *pl)
{
  int i;
  struct list_head *cur;
  dgm_board_t *brd_temp      = NULL;
  dgm_board_t *brd           = NULL;
  unsigned long minor        = 0L;
  unsigned long interface    = 666L;
  DMXProperty *p             = NULL;
  dgm_interface_t *dgm_if = NULL;
  DMXUniverse *dmx_u[4];

  DO_DEBUG(DGM_DEBUG_FUNCTION, printk(DGM_DEBUG "dgm : int dgm_create_interface (DMXInterface *i=%p, DMXPropList *pl=%p)\n", dmx_if, pl));

  if (pl && pl->find)
  {
    p = pl->find(pl, "brd_id");
    if (p)
    {
      p->get_long (p, &minor);
    }
  }
  if (!minor)
  {
    DO_DEBUG(DGM_DEBUG_WARN_1, printk(DGM_WARN "dgm : create_interface cannot find minor in proplist\n"));
    return -1;
  }
  if (pl && pl->find)
  {
    p = pl->find(pl, "if_id");
    if (p)
    {
      p->get_long (p, &interface);
    }
  }
  if (interface > 1)
  {
    DO_DEBUG(DGM_DEBUG_WARN_1, printk(DGM_WARN "dgm : create_interface cannot find interface id in proplist\n"));
    return -1;
  }

  list_for_each(cur, &(dgm_drv_data.link))
  {
    brd_temp = list_entry(cur, dgm_board_t, link);
    if ((int)brd_temp->minor == minor)
      brd = brd_temp;
  }

  if (!brd)
  {
    DO_DEBUG(DGM_DEBUG_WARN_1, printk(DGM_WARN "dgm : create_interface cannot find board with given minor %lu\n",minor));
    return -1;
  }

  DO_DEBUG(DGM_DEBUG_TEMP_1, printk (DGM_DEBUG "dgm : create_interface found board struct with given minor %lu\n", minor));

  dgm_if = DMX_ALLOC(dgm_interface_t);
  if (!dgm_if)
  {
    DO_DEBUG(DGM_DEBUG_WARN_2, printk(DGM_WARN "dgm : failed to allocate interface\n"));
    return -1;
  }

  dgm_if->index = interface;
  dgm_if->mode = DGM_INIT_CPU_MODE;
  if (interface)
    dgm_if->mem = (dgm_memory_t *)(brd->mem_addr[1]);
  else
    dgm_if->mem = (dgm_memory_t *)(brd->mem_addr[0]);

  dgm_if->dgm_brd = brd;
  dgm_if->dmx_if = dmx_if;

  dmx_if->user_data   = dgm_if;
  dmx_if->user_delete = dgm_delete_interface;

  brd->dgm_if[interface]=dgm_if;

  /* create property for cpu_mode get and set */

  pl = dmx_if->props;
  if (pl)
  {
    p = dmxprop_create_long ("cpumode", 0);
    if (p)
    {
      if (dmxprop_user_long(p, dgm_get_cpumode, dgm_set_cpumode, dgm_if) < 0)
        p->delete(p);
      else
      pl->add(pl, p);
    }
    p = dmxprop_create_long ("timer_freq", 0);
    if (p)
    {
      if (dmxprop_user_long(p, dgm_get_timer_freq, dgm_set_timer_freq, &dgm_drv_data) < 0)
        p->delete(p);
      else
      pl->add(pl, p);
    }
    p = dmxprop_create_long ("timer_state", 0);
    if (p)
    {
      if (dmxprop_user_long(p, dgm_get_timer_state, dgm_set_timer_state, &dgm_drv_data) < 0)
        p->delete(p);
      else
      pl->add(pl, p);
    }
    p = dmxprop_create_long ("timer_count", 0);
    if (p)
    {
      if (dmxprop_user_long(p, native_shortptr_get_long, fake_shortptr_set_long, &dgm_drv_data.timer_count) < 0)
        p->delete(p);
      else
      pl->add(pl, p);
    }
  }
  else
    DO_DEBUG(DGM_DEBUG_TEMP_1, printk (DGM_DEBUG "dgm : create_interface can't find dmx_if proplist !\n"));

  /* create universes & link structs */

  for (i=0; i<4 ; i++)
  {
    if ((dmx_u[i] = dmx_if->create_universe(dmx_if,i>>1,dmxproplist_vacreate("u_id=%l",i)))
    &&  (dgm_if->dgm_u[i] = (dgm_universe_t*)dmx_u[i]->user_data))
    {
      DO_DEBUG(DGM_DEBUG_INFO_2, printk (DGM_INFO "dgm : created universe %i\n",i));
    }
    else
    {
      DO_DEBUG(DGM_DEBUG_WARN_2, printk(DGM_WARN "dgm : failed to initialize universe %i\n",i));
      return -1;
    }
  }

  /* update_mode */

  dgm_update_mode(dgm_if,DGM_INIT_CPU_MODE);

  /* init card */

  if (dgm_init_interface (dgm_if->mem) < 0)
  {
    DO_DEBUG(DGM_DEBUG_WARN_2, printk(DGM_WARN "dgm : failed to initialize interface\n"));
    DMX_FREE(dgm_if);
    return -1;
  }
  DO_DEBUG(DGM_DEBUG_INFO_2, printk (DGM_INFO "dgm : successfully initialized dmx pci interface\n"));
  return 0;
}

/* -------------------------------------------------------------------------------------------
 * functions called by hw driver to create and delete boards
 * ------------------------------------------------------------------------------------------- */

int dgm_create_board (dgm_board_t *brd)
{
  int i;
  DMXDriver    *dmx_drv = NULL;
  DMXInterface *dmx_if  = NULL;

  /* *** get mutex to access dgm_drv_data */
  down(&dgm_drv_data.sem);

  /* update board count and get the id for the board */
  ++dgm_drv_data.boards;
  brd->minor = ++dgm_drv_data.id;

  /* Link the new dgm_board_t structure with others */
  list_add_tail(&brd->link, &(dgm_drv_data.link));

  /* *** release mutex */
  up(&dgm_drv_data.sem);

  /* link struct */

  brd->dgm_drv = &dgm_drv_data;
  brd->dgm_if[0] = NULL;
  brd->dgm_if[1] = NULL;

  /* build dmx4linux interface(s) */

  dmx_drv = dgm_drv_data.dmx_drv;
  if (!dmx_drv)
  {
    DO_DEBUG(DGM_DEBUG_WARN_1, printk(DGM_WARN "dgm : driver is NULL while creating board !\n"));
    return -1;
  }
  for (i = 0; i < brd->if_count ; i++)
  {
    dmx_if = dmx_drv->create_interface(dmx_drv, dmxproplist_vacreate("brd_id=%l,if_id=%l",brd->minor,i));
    if (!dmx_if)
    {
      DO_DEBUG(DGM_DEBUG_WARN_1, printk(DGM_WARN "dgm : unable to create DMXInterface while creating board !\n"));
      return -2;
    }
  }
  return 0;
}

int dgm_delete_board (dgm_board_t *brd)
{
  int i;
  dgm_interface_t *dgm_if;

  for (i = 0; i < brd->if_count ; i++)
  {
    if((dgm_if = brd->dgm_if[i]) && dgm_if->dmx_if)
      dgm_if->dmx_if->delete(dgm_if->dmx_if);
  }

  /* *** get mutex to access dgm_drv_data */
  down(&dgm_drv_data.sem);

  /* update board count and unlink the struct */
  list_del(&brd->link);
  --dgm_drv_data.boards;

  /* *** release mutex */
  up(&dgm_drv_data.sem);
  return 0;
}

/* ------------------------------------------------------------------------------------------- *
 * Init and Exit
 * ------------------------------------------------------------------------------------------- */

DGM_STATIC int __init dgm_init(void)
{
  dgm_driver_t *drv = &dgm_drv_data;

  /* Init private struct */

  /* this should be like kernel's INIT_LIST_HEAD(&(dgm_drv_data.link)) */
  dgm_drv_data.link.next = &dgm_drv_data.link;
  dgm_drv_data.link.prev = &dgm_drv_data.link;

  dgm_drv_data.minor     = 0; /* flag for the driver list item */
  dgm_drv_data.boards    = 0; /* init boards count */
  dgm_drv_data.id        = 0; /* init the auto id generator */

  sema_init(&dgm_drv_data.sem, 1);

  drv->timer_state = 0;
  drv->timer_freq  = DGM_INIT_TIMER_FREQ;
  drv->timer_delay = 0;
  drv->timer_count = 0;
  drv->stop_async  = 0;

  init_waitqueue_head(&(drv->stop_wq));

  /* Create dmx4l family & driver */

  DO_DEBUG(DGM_DEBUG_INFO_1, printk(DGM_INFO "dgm : module insertion !\n"));
  dgm_drv_data.dmx_f = dmx_create_family ("dgm_family");
  if (dgm_drv_data.dmx_f)
  {
    dgm_drv_data.dmx_drv = dgm_drv_data.dmx_f->create_driver (dgm_drv_data.dmx_f, "dgm_driver", dgm_create_universe, NULL);
    if (!dgm_drv_data.dmx_drv)
    {
      dgm_drv_data.dmx_f->delete(dgm_drv_data.dmx_f, 0);
      DO_DEBUG(DGM_DEBUG_WARN_2, printk(DGM_WARN "dgm : cannot create dmx_driver !\n"));
      return -1;
    }
    dgm_drv_data.dmx_drv->user_create_interface = dgm_create_interface;
  }
  else
  {
    DO_DEBUG(DGM_DEBUG_WARN_2, printk(DGM_WARN "dgm : cannot create dmx_family !\n"));
    return -1;
  }

  /* Timer init */

  if (DGM_INIT_TIMER_STATE) dgm_start_timer(drv);

  return 0;
}

DGM_STATIC void __exit dgm_exit(void)
{
  dgm_driver_t *drv = &dgm_drv_data;
  DO_DEBUG(DGM_DEBUG_INFO_1, printk(DGM_INFO "dgm : module remove !\n"));
  if (dgm_drv_data.dmx_f)
    dgm_drv_data.dmx_f->delete(dgm_drv_data.dmx_f, 0);
  dgm_stop_timer(drv);
}

module_init(dgm_init);
module_exit(dgm_exit);

EXPORT_SYMBOL(dgm_create_board);
EXPORT_SYMBOL(dgm_delete_board);
