/* sunlite.h
 *
 * Copyright (C) Michael Stickel <michael@cubic.org>
 *               Bastien Andres  <bastos@balelec.ch>
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

#ifndef __DMXSUNLITE_H__
#define __DMXSUNLITE_H__

#define __KERNEL_SYSCALLS__

#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/usb.h>
#include <asm/unistd.h>
#include <asm/atomic.h>

#include <dmx/dmxdev.h>


#define USB_VENDOR_DIGITALARTSYSTEM     0x0962
#define USB_PRODUCT_EZ_USB              0x1000
#define USB_PRODUCT_EZ_USB2             0x2000
#define USB_PRODUCT_EZ_USB2a            0x2001
#define USB_PRODUCT_EZ_USB3             0x3000
#define USB_PRODUCT_EZ_USB3a            0x3001

#define SUNLITE_DMX_SLOTS          512
#define SUNLITE_PACKET_SIZE        0x340
#define SUNLITE_FRAMES_PER_SECOND  40

#define SUNLITE_IN_BULK_BUF_SIZE 0x203
#define SUNLITE_SEND_FIRMWARE_PACKET_REQUEST 0xa0

#define DRIVERNAME "dmxsunlite"

#define SUNLITE_TIMEOUT (HZ) /* 1 second maximum */

struct sunlite_firmware_packet
{
  int wValue;
  int wLength;
  u_char data[16];
};

struct sunlite_hex_record {
  __u16 address;
  __u8  data_size;
  __u8  data[16];
};

struct sunlite_interface_s;

struct sunlite_universe_s
{
  struct sunlite_interface_s *sunlite_if;
  DMXUniverse                *dmx_universe;
  volatile char               data_avail;  /* for userspace: 1 = data available on universe. 0 = no new values available on universe. */
  unsigned char               buffer[512];
};

struct sunlite_interface_s
{
  struct usb_device         *usb_dev;      /* init: probe_dmx      */
  struct usb_interface      *usb_if;
  DMXInterface              *dmx_interface;
  struct sunlite_universe_s *sunlite_universe;

  struct workqueue_struct   *wkq;
  struct work_struct        *wk;
  int                        wkrun;    /* 0:stop 1:run */
  int                        wkstatus; /* 0:stop 1:run */
  unsigned long              delay;

  wait_queue_head_t  waitqueue;   /* wake(...) will wake up the worker thread for that interface */
  wait_queue_head_t  stop_waitqueue;   /* wake(...) will wake up the worker thread for that interface */
  char               data_pending;
  atomic_t           urb_submit_pending;  /* value 1 during urb submit, reset in complete function*/

  /* unsigned char bulkout_buffer[PACKET_SIZE]; */

  char               bulkout_buffer[1024];
#if 0
  unsigned char      inbuffer[1024];
#endif
};

struct sunlite_device_info_s
{
  unsigned short  vendor;
  unsigned short  device;
  char  *         name;
  short           universes_out;
  short           universes_in;
  const struct sunlite_hex_record *firmware;
};

typedef struct sunlite_universe_s    sunlite_universe_t;
typedef struct sunlite_interface_s   sunlite_interface_t;
typedef struct sunlite_device_info_s sunlite_device_info_t;

/* fn protos */

static char *usberror2string (int e);
/*
int  sunlite_receive_bulk (sunlite_interface_t *sunlite_if, int pipe_id, char *buffer, int size);
*/
static int  sunlite_send_bulk    (sunlite_interface_t *sunlite_if, int pipe_id, char *buffer, int size);
/*
int  sunlite_read_ports   (sunlite_interface_t *sunlite_if);
int  sunlite_init_0         (sunlite_interface_t *sunlite_if);
int  sunlite_init_1       (sunlite_interface_t *sunlite_if);
int  sunlite_init_2       (sunlite_interface_t *sunlite_if);
int  sunlite_init_3       (sunlite_interface_t *sunlite_if);
*/
static int  sunlite_in_init      (sunlite_interface_t *sunlite_if);
static int  sunlite_startup      (struct usb_device *dev, __u16 idProduct, const struct sunlite_hex_record *firmware);

static void sunlite_init_packet  (unsigned char packet[]);
static int  sunlite_do_write     (sunlite_interface_t *sunlite_if, int offs, int size, unsigned char *dmx_values);
static int  sunlite_update       (sunlite_interface_t *sunlite_if);

static void sunlite_bulk_urb_complete (struct urb *purb, struct pt_regs *pptregs);

#endif
