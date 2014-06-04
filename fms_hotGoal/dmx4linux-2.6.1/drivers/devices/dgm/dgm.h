/*
 * dgm.h : header for the digimedia mls dmx pci and pcmcia card driver
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

#ifndef _dgm_h
#define _dgm_h

#include <dmx/dmxdev.h>

#define DGM_DEBUG KERN_DEBUG
#define DGM_WARN KERN_WARNING
#define DGM_INFO KERN_INFO

/*
#define DGM_DEBUG KERN_ALERT
#define DGM_WARN KERN_ALERT
#define DGM_INFO KERN_ALERT
*/

#define DGM_DEBUG_WARN_1     1
#define DGM_DEBUG_WARN_2     2
#define DGM_DEBUG_INFO_1     4
#define DGM_DEBUG_INFO_2     8
#define DGM_DEBUG_FUNCTION  16
#define DGM_DEBUG_COPY      32
#define DGM_DEBUG_TIMER     64
#define DGM_DEBUG_TEMP_1   128
#define DGM_DEBUG_TEMP_2   256
#define DGM_DEBUG_PCMCIA   512

#define DO_DEBUG(flag,cmd...) { if (debug_flags & (flag)) cmd; }

#define DGM_STATIC static

#define DGM_INIT_CPU_MODE    0
#define DGM_INIT_TIMER_STATE 1
#define DGM_INIT_TIMER_FREQ  100

/*
 * Const and simple types definitions
 */

/* CPU MODES
 *
 * # Link0 / Link1
 * 0: Idle / Idle
 * 1: Out  / Idle
 * 2: Idle / In
 * 3: Out  / In
 * 4: Out  / Out
 * 5: In   / In
 *
 */

typedef unsigned char  byte;
typedef unsigned short word;

/*
 * Structs definitions
 */

typedef struct
{
  byte    ready_flag; /* 0xF0 after reset */
  byte    signature[10]; /* = "DMXOEMx " */
  byte    cpu_version_high;
  byte    cpu_version_low;
  byte    cpu_mode;

  byte    out_startcode[2];
  word    out_channel_cnt;
  word    out_break_time;
  word    out_break_count;
  word    out_mbb_time; /* mark before break time */

  byte    in_startcode[2];   /* be aware - use ntohs,htons for access for all 16-bit values */
  word    in_channel_cnt[2]; /* because they are in network byte order (MSB,LSB) */
  word    in_break_cnt[2];

  unsigned char reserved[0x9ff-0x822+1];

  unsigned char dmxbuffer[2][512];
} __attribute__ ((packed)) dgm_memory_t;

typedef struct _dgm_driver_t    dgm_driver_t;
typedef struct _dgm_board_t     dgm_board_t;
typedef struct _dgm_interface_t dgm_interface_t;
typedef struct _dgm_universe_t  dgm_universe_t;

struct _dgm_driver_t {
  struct list_head    link;    /* MUST be first field  - Double linked list */
  int                 minor;   /* MUST be second field - Minor number of the driver */
  int                 boards;
  int                 id;

  struct semaphore    sem;                             /* semaphore to access from both hw driver */

  u32                 timer_state;                     /* 0 = timer off 1 = timer on */
  u32                 timer_freq;                      /* frequency of the timer */

  struct timer_list   timer_tl;                        /* timer struct */
  int                 timer_delay;                     /* jiffies count delay */
  u32                 timer_count;                     /* count of timer func executed */
  int                 stop_async;                      /* wait sequencer */
  wait_queue_head_t   stop_wq;                         /* event wait for async rmmod */

  DMXFamily          *dmx_f;
  DMXDriver          *dmx_drv;
};

struct _dgm_board_t {
  struct list_head    link;                            /* MUST be first filed  - Double linked list */
  int                 minor;                           /* MUST be second field - Minor number of the driver */

  int                 type;                            /* Board type */
  int                 if_count;                        /* number of interfaces of this board */
  void               *mem_addr[2];                     /* Address of remapped dgm_memory_t */
  int                 bar_count;
  void               *bar_addr[2];                     /* Remaped I/O memory */
  void               *dev;                             /* link to the device (pci or pcmcia) */

  dgm_driver_t       *dgm_drv;
  dgm_interface_t    *dgm_if[2];
};

struct _dgm_interface_t {
  int                 index;
  int                 mode;
  dgm_memory_t       *mem;       /* memory area of the interface */

  dgm_board_t        *dgm_brd;
  dgm_universe_t     *dgm_u[4]; /* out0 out1 in0 in1 */
  DMXInterface       *dmx_if;
};

struct _dgm_universe_t {
  unsigned short      index;
  unsigned short      mode; /* 0=output_idle 1=input_idle 2=output 3=input */

  unsigned short      last_framecount;
  unsigned short      data_avail;

  unsigned int        channels_count;

  unsigned char      *startcode;
  unsigned short     *channels;
  unsigned short     *framecount;
  unsigned short     *breaksize;
  unsigned short     *mbb_size;
  unsigned char      *data_pointer;
  unsigned char       local_buffer[512];

  dgm_interface_t    *dgm_if;
  DMXUniverse        *dmx_u;
};

int dgm_create_board (dgm_board_t *brd);
int dgm_delete_board (dgm_board_t *brd);

#endif /* _dgm_h */
