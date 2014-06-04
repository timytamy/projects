/*
 * slh1514pci.h : header for the dmx pci card driver
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

#ifndef SLH1514PCI__H
#define SLH1514PCI__H

#include <linux/version.h>

#define DMXPCI_RW_LOOP         1 /* will use a loop to copy_from/to_user() to be sure to access bytewise */

//#include <dmx/dmxdev.h>

#define DMXPCI_VENDOR_ID_0   0x10b5
#define DMXPCI_DEVICE_ID_0   0x2544
#define DMXPCI_BOARD_TYPE_0  0
#define DMXPCI_BOARD_INFO_0 "Digimedia - Soundlight DMX card 1514PCI"

#define DMXPCI_VENDOR_ID_1   0x10b5
#define DMXPCI_DEVICE_ID_1   0x2545
#define DMXPCI_BOARD_TYPE_1  1
#define DMXPCI_BOARD_INFO_1 "Digimedia - Soundlight DMX card 2514PCI"

#define DMXPCI_TIMER_PARAM  1          /* 1 for check receiver */

#define DMXPCI_CMD_TCB      0x08000000 /* fn to wait till next timer */
#define DMXPCI_CMD_SET_MODE 0x04000000 /* will try to set mode from parameter */
#define DMXPCI_CMD_GET_MODE 0x02000000 /* will return mode */

#define DMXPCI_IOCTL_SIGN   0x80000000 /* not used so unsigned equal signed */
#define DMXPCI_IOCTL_OPER   0x40000000 /* 0 = other  / 1 = read_write ops */
#define DMXPCI_IOCTL_WRITE  0x20000000 /* 0 = read   / 1 = write */
#define DMXPCI_IOCTL_WORD   0x10000000 /* 0 = 8 bits / 1 = 32 bits */
#define DMXPCI_IOCTL_IOMEM  0x08000000 /* 0 = memory / 1 = I/O */
#define DMXPCI_IOCTL_BARNO  0x07000000 /* BAR ID */
#define DMXPCI_IOCTL_ADDR   0x00FFFFFF /* address */

#define DMXPCI_BAR_FLAG_VALID  0x80000000
#define DMXPCI_BAR_FLAG_IO     0x40000000
#define DMXPCI_BAR_FLAG_MEM    0x20000000
#define DMXPCI_BAR_FLAG_SPARE  0x10000000
#define DMXPCI_BAR_FLAG_MASK   0xF0000000
#define DMXPCI_BAR_FLAG_DMXOEM 0x00000001


#define DMXPCI_DEBUG KERN_DEBUG
#define DMXPCI_WARN KERN_WARNING
#define DMXPCI_INFO KERN_INFO

/*
#define DMXPCI_DEBUG KERN_ALERT
#define DMXPCI_WARN KERN_ALERT
#define DMXPCI_INFO KERN_ALERT
*/

#define DMXPCI_DEBUG_WARN_1     1
#define DMXPCI_DEBUG_WARN_2     2
#define DMXPCI_DEBUG_INFO_1     4
#define DMXPCI_DEBUG_INFO_2     8
#define DMXPCI_DEBUG_FUNCTION  16
#define DMXPCI_DEBUG_COPY      32
#define DMXPCI_DEBUG_TIMER     64
#define DMXPCI_DEBUG_TEMP_1   128
#define DMXPCI_DEBUG_TEMP_2   256

#define DO_DEBUG(flag,cmd...) { if (debug_flags & (flag)) cmd; }
#define DMXPCI_STATIC static

/*
 * Const and simple types definitions
 */

#define DMXPCI_MEMORY_OFFSET_0 (0x0000)
#define DMXPCI_MEMORY_OFFSET_1 (0x0800)

#define DMXPCI_MEMORY_REGION_0 2
#define DMXPCI_MEMORY_REGION_1 2

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

typedef struct _dmxpci_driver_t    dmxpci_driver_t;
typedef struct _dmxpci_board_t     dmxpci_board_t;
typedef struct _dmxpci_interface_t dmxpci_interface_t;
typedef struct _dmxpci_universe_t  dmxpci_universe_t;

struct _dmxpci_driver_t {
  struct list_head  link;    /* MUST be first field  - Double linked list */
  int               minor;   /* MUST be second field - Minor number of the driver */
  int               boards;

#ifdef DMXPCI_DMX4LINUX
  DMXFamily        *dmx_f;
  DMXDriver        *dmx_drv;
#endif
};

struct _dmxpci_board_t {
  struct list_head    link;                            /* MUST be first filed  - Double linked list */
  int                 minor;                           /* MUST be second field - Minor number of the driver */
  struct pci_dev     *dev;                             /* PCI device */
  int                 type;                            /* Board type */
  void               *bar_addr[DEVICE_COUNT_RESOURCE]; /* Remaped I/O memory / IO ports address */
  u32                 bar_len[DEVICE_COUNT_RESOURCE];  /* Size of remaped I/O memory / ports range */
  u32                 bar_flags[DEVICE_COUNT_RESOURCE];/* Flags for the bars (valid/io) */
#ifdef DOJ_USE_TIMER_WQ
  wait_queue_head_t   timer_wq;                        /* wait queue to wake up callback */
  struct tq_struct    timer_task;                      /* task to be executed on timer */
#endif
  wait_queue_head_t   stop_wq;                         /* event wait for async rmmod */
  struct timer_list   timer_tl;                        /* timer struct */
  int                 timer_delay;                     /* jiffies count delay */
  u32                 timer_count;                     /* count of timer func executed */
  int                 timer_param;                     /* parameter for timer action ??? */
  int                 stop_async;                      /* wait sequencer */

  dmxpci_driver_t    *dgm_drv;
  dmxpci_interface_t *dgm_if[2];
};

struct _dmxpci_interface_t {
  int                 index;
  int                 cpu_mode;
  dgm_memory_t       *mem;       /* memory area of the interface */

  dmxpci_board_t     *dgm_brd;
  dmxpci_universe_t  *dgm_u[2]; /* out in */
#ifdef DMXPCI_DMX4LINUX
  DMXInterface       *dmx_if;
#endif
};

#define IO_MODE_IDLE 0
#define IO_MODE_OUT 2
#define IO_MODE_IN 3

struct _dmxpci_universe_t {
  unsigned short      index;
  unsigned short      io_mode;

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

  dmxpci_interface_t *dgm_if;
#ifdef DMXPCI_DMX4LINUX
  DMXUniverse        *dmx_u;
#endif
};

#endif /* _dmxpci_h */
