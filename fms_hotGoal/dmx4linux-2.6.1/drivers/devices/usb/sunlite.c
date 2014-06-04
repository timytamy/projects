/* sunlite.c
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

#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

/* includes */

extern int errno;
#include "sunlite.h"
#include "dmxsunlite_firmware.h"

/* TODO: this code only works with older kernels */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)

/* module stuff */

MODULE_AUTHOR("(c) 2001 Michael Stickel <michael@cubic.org> http://llg.cubic.org - 2006 Bastien Andres <bastos@balelec.ch>");
MODULE_DESCRIPTION("Driver for the Sunlite-DMX512 interface version " DMXVERSION);
MODULE_LICENSE("GPL");

static int inputrate=30;
module_param(inputrate, int, S_IRUGO);
MODULE_PARM_DESC(inputrate,"input universe is read <inputrate> times per second (default 30)");

/* static structs */

static DMXFamily        *sunlite_family = NULL;

sunlite_device_info_t sunlite_device_info[] =
  {
    { USB_VENDOR_DIGITALARTSYSTEM, USB_PRODUCT_EZ_USB,   "Sunlite USBDMX1",            1, 0, NULL},
    { USB_VENDOR_DIGITALARTSYSTEM, USB_PRODUCT_EZ_USB2,  "Sunlite USBDMX2-firmware",   0, 0, sunlite_firmware},
    { USB_VENDOR_DIGITALARTSYSTEM, USB_PRODUCT_EZ_USB2a, "Sunlite USBDMX2",            1, 0, NULL},
    { USB_VENDOR_DIGITALARTSYSTEM, USB_PRODUCT_EZ_USB3,  "Sunlite USBDMX-IN-firmware", 0, 0, sunlite_firmware_in},
    { USB_VENDOR_DIGITALARTSYSTEM, USB_PRODUCT_EZ_USB3a, "Sunlite USBDMX-IN",          0, 1, NULL},
#if 0
    { 0xb334,                      0x1,                  "Twilight",                   0, 0, sunlite_firmware},
#endif
    { 0, 0, NULL, 0, 0, NULL}
  };

static struct usb_device_id sunlite_id_table [] =
  {
#if 0
    /* sunlite1 is currently not supported. */
    { USB_DEVICE(USB_VENDOR_DIGITALARTSYSTEM, USB_PRODUCT_EZ_USB),   driver_info: 0 },  /* USBDMX1 */
#endif
    { USB_DEVICE(USB_VENDOR_DIGITALARTSYSTEM, USB_PRODUCT_EZ_USB2),  driver_info: 1 },  /* USBDMX2 */
    { USB_DEVICE(USB_VENDOR_DIGITALARTSYSTEM, USB_PRODUCT_EZ_USB2a), driver_info: 2 },  /* USBDMX2 firmware */
    { USB_DEVICE(USB_VENDOR_DIGITALARTSYSTEM, USB_PRODUCT_EZ_USB3),  driver_info: 3 },  /* USBDMX-IN firmware */
    { USB_DEVICE(USB_VENDOR_DIGITALARTSYSTEM, USB_PRODUCT_EZ_USB3a), driver_info: 4 },  /* USBDMX-IN */
#if 0
    { USB_DEVICE(0xb334, 0x1), driver_info: 5 },  /* Twilight */
#endif
    { }                                           /* Terminating entry */
  };

MODULE_DEVICE_TABLE(usb, sunlite_id_table);

static int  sunlite_probe (struct usb_interface *usb_if, const struct usb_device_id *id);
static void sunlite_disconnect(struct usb_interface *usb_if);

static struct usb_driver sunlite_driver =
  {
    //  owner:          THIS_MODULE,
    name:           "SUNLITE",
    probe:          sunlite_probe,
    disconnect:     sunlite_disconnect,
    id_table:       sunlite_id_table,
  };

/* usb utility fn */

/*-----------------------------------------------------------
 *-- usberror2string
 *-- Returns the usb error in a human readable form.
 *---------------------------------------------------------*/
/* can't find the header for this (bastos) */
static char *usberror2string (int e)
{
  static char error_number[16];
  snprintf(error_number,12,"0x%08X",e);
  return error_number;
}
#if 0
static char *usberror2string (int e)
{
  switch (e)
    {
    case USB_ST_NOERROR:           return "USB_ST_NOERROR";
    case USB_ST_CRC:               return "USB_ST_CRC";
#if 0
    case USB_ST_BITSTUFF:          return "USB_ST_BITSTUFF";     /* == USB_ST_INTERNALERROR */
    case USB_ST_NORESPONSE:        return "USB_ST_NORESPONSE";   /* == USB_ST_TIMEOUT */
    case USB_ST_DATAUNDERRUN:      return "USB_ST_DATAUNDERRUN"; /* == USB_ST_SHORT_PACKET */
#endif
    case USB_ST_DATAOVERRUN:       return "USB_ST_DATAOVERRUN";
    case USB_ST_BUFFEROVERRUN:     return "USB_ST_BUFFEROVERRUN";
    case USB_ST_BUFFERUNDERRUN:    return "USB_ST_BUFFERUNDERRUN";
    case USB_ST_INTERNALERROR:     return "USB_ST_INTERNALERROR: unknown error";
    case USB_ST_SHORT_PACKET:      return "USB_ST_SHORT_PACKET";
    case USB_ST_PARTIAL_ERROR:     return "USB_ST_PARTIAL_ERROR: ISO transfer only partially completed";
    case USB_ST_URB_KILLED:        return "USB_ST_URB_KILLED: URB canceled by user";
    case USB_ST_URB_PENDING:       return "USB_ST_URB_PENDING";
    case USB_ST_REMOVED:           return "USB_ST_REMOVED: device not existing or removed";
    case USB_ST_TIMEOUT:           return "USB_ST_TIMEOUT: communication timed out, also in urb->status";
    case USB_ST_NOTSUPPORTED:      return "USB_ST_NOTSUPPORTED";
    case USB_ST_BANDWIDTH_ERROR:   return "USB_ST_BANDWIDTH_ERROR: too much bandwidth used";
    case USB_ST_URB_INVALID_ERROR: return "USB_ST_URB_INVALID_ERROR: invalid value/transfer type";
    case USB_ST_URB_REQUEST_ERROR: return "USB_ST_URB_REQUEST_ERROR: invalid endpoint";
    case USB_ST_STALL:             return "USB_ST_STALL: pipe stalled, also in urb->status";
    }
  return "unknown error";
}
#endif

/* usb oriented fns */

/*-----------------------------------------------------------
 *-- sunlite_receive_bulk
 *--
 *-- Requests data using a bulk message.
 *---------------------------------------------------------*/
#if 0
static int sunlite_receive_bulk (sunlite_interface_t *sunlite_if, int pipe_id, char *buffer, int size)
{
  int actual_length = 0;
  int status = usb_bulk_msg (sunlite_if->usb_dev,usb_rcvbulkpipe(sunlite_if->usb_dev, pipe_id), buffer, size, &actual_length, SUNLITE_TIMEOUT);
  if (status < 0)
    {
      printk ("sunlite_receive_bulk(pipe=%d) USB-Error: %s\n", pipe_id, usberror2string (status));
      return -1;
    }
  return actual_length;
}
#endif
/*-----------------------------------------------------------
 *-- sunlite_send_bulk
 *--
 *-- Sends data to the interface using a bulk message.
 *---------------------------------------------------------*/
static int sunlite_send_bulk (sunlite_interface_t *sunlite_if, int pipe_id, char *buffer, int size)
{
  int actual_length = 0;
  int status = usb_bulk_msg (sunlite_if->usb_dev, usb_sndbulkpipe(sunlite_if->usb_dev, 1), buffer, size, &actual_length, SUNLITE_TIMEOUT);
  if (status < 0)
    {
      printk ("sunlite_send_bulk(pipe=%d) USB-Error: %s\n", pipe_id, usberror2string (status));
      return -1;
    }
  return actual_length;
}

/*-----------------------------------------------------------
 *-- sunlite_read_ports
 *--
 *-- Reads the 10 digital input-ports from the USBDMX1 interface.
 *---------------------------------------------------------*/
#if 0
static int sunlite_read_ports (sunlite_interface_t *sunlite_if)
{
  unsigned char buf[0x20];
  int status;

  printk ("sunlite_read_ports ()\n");

  status = sunlite_receive_bulk (sunlite_if, 0, buf, sizeof(buf));
  if (status > 0)
    {
      printk("sunlite_read_ports: got %d bytes\n", status);
    }
  else if (status < 0)
    {
      /* some error */
      return status;
    }
  return 0;
}

/*-----------------------------------------------------------
 *-- sunlite_init_1
 *--
 *-- Sends the first init sequence to the USBDMX1 interface.
 *---------------------------------------------------------*/
static int sunlite_init_1 (sunlite_interface_t *sunlite_if)
{
  static unsigned char init1[] = {
    0x90, 0x51, 0x02, 0x00, 0x00, 0x4b, 0xe7, 0x77
  };
  static unsigned char init2[] = {
    0xa0, 0x51, 0x01, 0x00, 0x00, 0x4b, 0xe7, 0x77
  };
  int status;

  printk("sunlite_init_1: 1\n");

  status = sunlite_send_bulk (sunlite_if, 0, init1, sizeof(init1));
  if (status > 0)
    printk ("sunlite_init_1: wrote %d bytes from first packet\n", status);
  else if (status < 0)
    return status;

  printk("sunlite_init_1: 2\n");


  status = sunlite_send_bulk (sunlite_if, 0, init2, sizeof(init2));
  if (status > 0)
    printk ("sunlite_init_1: wrote %d bytes from second packet\n", status);
  else if (status < 0)
    return status;

  printk("sunlite_init_1: 3\n");

  return 0;
}

/*-----------------------------------------------------------
 *-- sunlite_init_2
 *--
 *-- Sends the second init sequence to the USBDMX1 interface.
 *---------------------------------------------------------*/
static int sunlite_init_2(sunlite_interface_t *sunlite_if)
{
  static unsigned char init1[] = {
    0x90, 0x51, 0x02, 0x00, 0x06, 0x9a, 0xee, 0x33
  };
  static unsigned char init2[] = {
    0xa0, 0x51, 0x01, 0x00, 0x06, 0x9a, 0xee, 0x33
  };
  int status;

  printk("sunlite_init_2: 1\n");

  status = sunlite_send_bulk (sunlite_if, 0, init1, sizeof(init1));
  if (status > 0)
    printk ("sunlite_init_2: wrote %d bytes from first packet\n", status);
  else if (status < 0)
    return status;

  printk("sunlite_init_2: 2\n");

  status = sunlite_send_bulk (sunlite_if, 0, init2, sizeof(init2));
  if (status > 0)
    printk ("sunlite_init_2: wrote %d bytes from second packet\n", status);
  else if (status < 0)
    return status;

  printk("sunlite_init_2: 3\n");

  return 0;
}

/*-----------------------------------------------------------
 *-- sunlite_init_3
 *--
 *-- Sends the third init sequence to the USBDMX1 interface.
 *---------------------------------------------------------*/
static int sunlite_init_3(sunlite_interface_t *sunlite_if)
{
  int status = 0;

  static unsigned char init[] = {
    0xb0, 0x00, 0x00, 0xce, 0x9c, 0x0a, 0xb2, 0x10,
    0x6a, 0x2a, 0xe1, 0x71, 0x69, 0xbf, 0x3b, 0x5b,
    0xce, 0x15, 0x00, 0x10, 0x06, 0x00, 0x00, 0x00,
    0x64, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00
  };

  printk("sunlite_init_3: 1\n");

  status = sunlite_send_bulk (sunlite_if, 0, init, sizeof(init));
  if (status > 0)
    printk ("sunlite_init_3: wrote %d bytes\n", status);
  else if (status < 0)
    return status;

  printk("sunlite_init_3: 2\n");

  return 0;
}

/*-----------------------------------------------------------
 *-- sunlite_init_0
 *--
 *-- Initializes the USBDMX1 interface.
 *---------------------------------------------------------*/
static int sunlite_init_0 (sunlite_interface_t *sunlite_if)
{
  int error;

  printk("sunlite_init: 1\n");

  if ((error = sunlite_init_1(sunlite_if)))
    return error;

  printk("sunlite_init: 2\n");

  if ((error = sunlite_read_ports(sunlite_if)))
    return error;

  printk("sunlite_init: 3\n");

  if ((error = sunlite_init_2(sunlite_if)))
    return error;

  printk("sunlite_init: 4\n");

  if ((error = sunlite_read_ports(sunlite_if)))
    return error;

  printk("sunlite_init: 5\n");

  if ((error = sunlite_init_3(sunlite_if)))
    return error;

  printk("sunlite_init: 6\n");

  return 0;
}
#endif
/*-----------------------------------------------------------
 *-- sunlite_in_init
 *--
 *-- Initializes the USBDMX-IN interface.
 *---------------------------------------------------------*/
static int sunlite_in_init (sunlite_interface_t *sunlite_if)
{
  int rc;
  unsigned char inbuffer[512];

  /*
    rc = usb_get_device_descriptor(sunlite_if->dmx_dev);
    printk("bf: usb_get_device_descriptor %d\n", rc);
  */

  rc = usb_control_msg(sunlite_if->usb_dev, usb_rcvctrlpipe(sunlite_if->usb_dev, 0), 0x10, USB_DIR_IN | USB_TYPE_VENDOR, 0, 0, inbuffer, 1, SUNLITE_TIMEOUT);

  printk("sunlitew_in_init: usb_control_msg (request 0x10) sent: rc=%d\n", rc);

  return 0;
}

/*-----------------------------------------------------------
 *-- sunlite_init_packet
 *--
 *-- Initializes the USBDMX bulkout buffer.
 *---------------------------------------------------------*/
static void sunlite_init_packet (unsigned char packet[])
{
  int p, a, s, i;

  for (i = 0; i < SUNLITE_PACKET_SIZE; ++i)
    packet[i] = 0;

  for (p = 0; p < 26; ++p) {
    a = p * 32;
    s = p * 20;

    packet[a] = 0x80;
    packet[a+1] = s / 2;
    packet[a+2] = 0x84;
    packet[a+7] = s / 2 + 2;
    packet[a+8] = 0x84;
    packet[a+13] = s / 2 + 4;
    if (p < 25) {
      packet[a+14] = 0x84;
      packet[a+19] = s / 2 + 6;
      packet[a+20] = 0x84;
      packet[a+25] = s / 2 + 8;
      packet[a+26] = 0x04;
      packet[a+31] = 0x00;
    } else /* the last packet ist short */
      packet[a+14] = 0x04;
  }
}

/*-----------------------------------------------------------
 *-- SUNLITE_SET_SLOT
 *--
 *-- Sets one channel in the bulkout buffer.
 *---------------------------------------------------------*/
#define SUNLITE_SET_SLOT(packet, slot, value) \
  (packet)[((slot) / 20) * 32   \
     + (((slot) / 4) % 5) * 6 \
     + 3        \
     + ((slot) % 4)] = (value);

/*-----------------------------------------------------------
 *-- sunlite_do_write
 *--
 *-- Writes <size> channels with offset <offs> from <dmx_values>
 *-- to the bulkout-buffer of <sunlite_if>.
 *---------------------------------------------------------*/
static int sunlite_do_write (sunlite_interface_t *sunlite_if, int offs, int size, unsigned char *dmx_values)
{
  int i;

  for (i = 0; i < size; ++i)
    SUNLITE_SET_SLOT((char *) sunlite_if->bulkout_buffer, offs+i, dmx_values[i]);
  return 0;
}

/*-----------------------------------------------------------
 *-- sunlite_update
 *--
 *-- Sends the current dmx-values to
 *-- the interface.
 *---------------------------------------------------------*/
static int sunlite_update (sunlite_interface_t *sunlite_if)
{
  int status = sunlite_send_bulk (sunlite_if, 0, sunlite_if->bulkout_buffer, SUNLITE_PACKET_SIZE);
  if (status > 0)
    ; /* printk ("sunlite_update: wrote %d bytes\n", status); */
  else if (status < 0)
    return status;

  return 0;
}

/*-----------------------------------------------------------
 *-- sunlite_writememory
 *--
 *-- Writes <length> bytes from the buffer <data> at <address>
 *-- into the memory of the EzUSB device.
 *---------------------------------------------------------*/
/* EZ-USB Control and Status Register.  Bit 0 controls 8051 reset */
#define CPUCS_REG    0x7F92

static int sunlite_writememory (struct usb_device *dev, int address, unsigned char *data, int length, __u8 bRequest)
{
  int result;
  unsigned char *transfer_buffer = kmalloc (length, GFP_KERNEL);

  if (!transfer_buffer)
    {
      printk( "sunlite_writememory: kmalloc(%d) failed\n", length);
      return -ENOMEM;
    }
  memcpy (transfer_buffer, data, length);
  result = usb_control_msg (dev, usb_sndctrlpipe(dev, 0), bRequest, 0x40, address, 0, transfer_buffer, length, 300);
  kfree (transfer_buffer);
  return result;
}

/*-----------------------------------------------------------
 *-- sunlite_startup
 *--
 *-- Uploads the firmware to the USBDMX2 interface.
 *---------------------------------------------------------*/
static int sunlite_startup (struct usb_device *dev, __u16 idProduct, const struct sunlite_hex_record *firmware)
{
  int response;
  const struct sunlite_hex_record *record = firmware;
  /*
    dbg("command_port_read_callback");
  */
  printk ("sunlite_startup()\n");

  if (!record)
    {
      printk ("sunlite_startup: no firmware for device available\n");
      return 0;
    }

  while (record->address != 0xffff)
    {
      response = sunlite_writememory (dev, record->address, (unsigned char *)record->data, record->data_size, 0xa0);
      if (response < 0)
	{
	  printk("sunlite_startup: sunlite_writememory failed for loader (%d %04X %p %d)\n",
		 response, record->address, record->data, record->data_size);
	  return -1;
	}
      ++record;
    }

  printk ("sunlite_startup: device is ready\n");
  /* we want this device to fail to have a driver assigned to it. */
  return 0;
}

/*-----------------------------------------------------------
 *-- sunlite_bulk_urb_complete
 *--
 *-- Called by the usb core when receiving requested input data
 *---------------------------------------------------------*/
static void sunlite_bulk_urb_complete (struct urb *purb, struct pt_regs *pptregs) {

  unsigned char       *ptmp_buffer;
  sunlite_universe_t  *sunlite_u;
  sunlite_interface_t *sunlite_if = (sunlite_interface_t *)purb->context;

  if (purb->status)
    printk("sunlite_bulk_urb_complete: purb->status: %d\n", purb->status);
  else if (purb->actual_length != SUNLITE_IN_BULK_BUF_SIZE)
    ; /*printk("purb->actual_length 0x%x != 0x%x\n", purb->actual_length, SUNLITE_IN_BULK_BUF_SIZE);*/
  else if (!purb->context)
    printk("sunlite_bulk_urb_complete: no purb->context\n");
  else {
    ptmp_buffer = (unsigned char *)purb->transfer_buffer + 3; /* offset 3 */
    sunlite_u = sunlite_if->sunlite_universe;
    if (sunlite_u  && sunlite_u->dmx_universe)
      {
	if (memcmp(sunlite_u->buffer, ptmp_buffer, SUNLITE_IN_BULK_BUF_SIZE-3))
	  {
	    /*printk("received {$%02X,$%02X,$%02X,$%02X,$%02X}\n", ptmp_buffer[0],ptmp_buffer[1],ptmp_buffer[2],ptmp_buffer[3], ptmp_buffer[4]);*/
	    memcpy(sunlite_u->buffer, ptmp_buffer, SUNLITE_IN_BULK_BUF_SIZE-3);
	    sunlite_u->data_avail=1;
	    sunlite_u->dmx_universe->signal_changed (sunlite_u->dmx_universe, 0, SUNLITE_IN_BULK_BUF_SIZE-3);
	  }
      }
    else
      printk("sunlite_bulk_urb_complete: no universe\n");
  }
  atomic_set(&sunlite_if->urb_submit_pending, 0);
}

/* dmx4linux related fns */

/*-----------------------------------------------------------
 *-- sunlite_write_universe
 *--
 *-- Used to write a couple of slot-values to the universe.
 *---------------------------------------------------------*/
static int sunlite_write_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  if (u && size>0 && offs+size <= 512)
    {
      sunlite_universe_t *sunlite_u = (sunlite_universe_t *)u->user_data;
      if (sunlite_u && sunlite_u->sunlite_if)
	{
	  /* copy the data and tell the thread something has changed */

	  memcpy ((sunlite_u->buffer)+offs, (void *)buff, size);
	  sunlite_do_write (sunlite_u->sunlite_if, offs, size, buff);
	  sunlite_u->sunlite_if->data_pending = 1;
	  wake_up (&sunlite_u->sunlite_if->waitqueue);
	  sunlite_u->data_avail=1;
	  u->signal_changed (u, offs, size);
	  return 0;
	}
    }
  return -EINVAL;
}

/*-----------------------------------------------------------
 *-- sunlite_read_universe
 *--
 *-- Used to write a couple of slot-values to the universe.
 *---------------------------------------------------------*/
static int sunlite_read_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  if (u && size>0 && offs+size <= 512)
    {
      sunlite_universe_t *sunlite_u = (sunlite_universe_t *)u->user_data;
      if (sunlite_u && sunlite_u->sunlite_if)
	{
	  unsigned char *inbuffer = sunlite_u->buffer;
	  if (sunlite_u->data_avail)
	    {
	      sunlite_u->data_avail=0;
	      if (offs+size>512)
		size = 512-offs;
	      memcpy (buff, inbuffer+offs, size);
	      return size;
	    }
	  return 0;
	}
    }
  return -EINVAL;
}

/*-----------------------------------------------------------
 *-- sunlite_data_available
 *--
 *--
 *---------------------------------------------------------*/
static int  sunlite_data_available (DMXUniverse *u, uint start, uint size)
{
  if (u && u->user_data)
    {
      sunlite_universe_t *sunlite_u = (sunlite_universe_t *)u->user_data;
      if (sunlite_u)
	return sunlite_u->data_avail;
    }
  return 0;
}

/*-----------------------------------------------------------
 *-- sunlite_delete_universe
 *--
 *-- Delete the universe of a USBDMX2 interface.
 *---------------------------------------------------------*/
static int sunlite_delete_universe (DMXUniverse *u)
{
  int ret;

  printk ("sunlite_delete_universe (u=0x%p)\n", u);
  if (u && u->user_data)
    {
      sunlite_universe_t *sunlite_u = (sunlite_universe_t *)u->user_data;

      sunlite_u->sunlite_if->wkrun = 0;
      printk ("sunlite_delete_universe : wait for work to stop ... wkstatus = %d\n",sunlite_u->sunlite_if->wkstatus);
      while (sunlite_u->sunlite_if->wkstatus)
	{
	  ret = wait_event_interruptible_timeout(sunlite_u->sunlite_if->stop_waitqueue, sunlite_u->sunlite_if->wkstatus,100);
	}
      printk("sunlite_delete_universe : ... ok work is stopped\n");

      sunlite_u->sunlite_if->sunlite_universe = NULL;
      sunlite_u->sunlite_if                   = NULL;
      sunlite_u->dmx_universe                 = NULL;

      FREE(u->user_data);
      u->user_data = NULL;
    }
  printk ("sunlite_delete_universe: exiting\n");
  return 0;
}

/*-----------------------------------------------------------
 *-- sunlite_create_universe
 *--
 *-- Create the universe for the USBDMX2 interface.
 *---------------------------------------------------------*/
static int sunlite_create_universe (DMXUniverse *u, DMXPropList *pl)
{
  printk ("sunlite_create_universe (u=0x%p, pl=0x%p)\n", u, pl);
  if (u && u->interface)
    {
      sunlite_universe_t *sunlite_u = NULL;
      sunlite_u = DMX_ALLOC(sunlite_universe_t);
      if (sunlite_u)
	{
	  memset (sunlite_u->buffer, 0, 512);
	  u->user_data      = (void *)sunlite_u;
	  u->user_delete    = sunlite_delete_universe;
	  u->read_slots     = sunlite_read_universe;
	  u->data_available = sunlite_data_available;
	  if (u->kind == 0)
	    {
	      /* output universe */
	      u->write_slots = sunlite_write_universe;
	      strcpy (u->connector, "OUT");
	      u->conn_id = 0;
	    }
	  else if (u->kind == 1)
	    {
	      /* input universe */
	      strcpy (u->connector, "IN");
	      u->conn_id = 1;
	    }
	  else
	    return -1;

	  sunlite_u->sunlite_if               = (sunlite_interface_t *)u->interface->user_data;
	  sunlite_u->dmx_universe             = u;
	  sunlite_u->sunlite_if->sunlite_universe = sunlite_u;
	  sunlite_u->sunlite_if->wkrun = 1;
	  wake_up_interruptible(&sunlite_u->sunlite_if->waitqueue);
	}
      return 0;
    }
  return -1;
}

/*-----------------------------------------------------------
 *-- sunlite_event
 *--
 *-- compute an event from the userspace.
 *-- This function must only be called from the
 *-- sunlite_interface_thread.
 *---------------------------------------------------------*/
static void sunlite_tx_event(sunlite_interface_t *sunlite_if)
{
  if (sunlite_if->data_pending)
    {
      /*    sunlite_universe_t *sunlite_u = sunlite_if->sunlite_universe; */

#ifdef DEBUG
      printk ("sunlite_tx_event: data-pending - sending it\n");
#endif

      sunlite_if->data_pending = 0;
      sunlite_update (sunlite_if);

      /* already done in write !!!
	 if (sunlite_u)
	 {
	 sunlite_u->data_avail = 1;
	 if (sunlite_u->dmx_universe && sunlite_u->dmx_universe->signal_changed)
	 sunlite_u->dmx_universe->signal_changed (sunlite_u->dmx_universe, 0, 512);
	 }
      */
    }
}

/*-----------------------------------------------------------
 *-- sunlite_work
 *--
 *-- The thread that sends the data to the USBDMX2 interface.
 *---------------------------------------------------------*/
static void sunlite_work (void *user_data)
{
  sunlite_interface_t *sunlite_if = (sunlite_interface_t *)user_data;
  int status;
  struct urb *purb;

  printk ("sunlite_work (void *user_data=0x%p)\n", user_data);

  if (!user_data)
    {
      printk ("sunlite_work: user_data = NULL -> exiting\n");
      return;
    }

  while (sunlite_if->wkrun == 0)
    {
      wait_event_interruptible(sunlite_if->waitqueue, (sunlite_if->wkrun == 1));
    }

  sunlite_if->wkstatus = 1;

  /* USBDMX-IN */
  if (sunlite_if->usb_dev->descriptor.idProduct == USB_PRODUCT_EZ_USB3a)
    {
      atomic_set(&sunlite_if->urb_submit_pending, 1);

      purb = usb_alloc_urb(0,0);
      if (purb == NULL)
	{
	  printk ("sunlite_work: cannot allocate urb\n");
	  sunlite_if->wkstatus = 0;
	  return;
	}
      /*
	usb_fill_bulk_urb (purb, sunlite_if->usb_dev, usb_rcvbulkpipe(sunlite_if->usb_dev, 2), sunlite_if->inbuffer, sizeof(sunlite_if->inbuffer), sunlite_bulk_urb_complete, (void*)sunlite_if);
      */
      usb_fill_bulk_urb (purb, sunlite_if->usb_dev, usb_rcvbulkpipe(sunlite_if->usb_dev, 2), sunlite_if->bulkout_buffer, sizeof(sunlite_if->bulkout_buffer), sunlite_bulk_urb_complete, (void*)sunlite_if);
      status=usb_submit_urb(purb,0);
      if (status)
	printk("sunlite_work: urb submit error: %d\n", status);
      do
	{
	  status = interruptible_sleep_on_timeout(&sunlite_if->waitqueue, sunlite_if->delay);
	  if (purb && !atomic_read(&sunlite_if->urb_submit_pending))
	    {
	      /*
		usb_fill_bulk_urb (purb, sunlite_if->usb_dev, usb_rcvbulkpipe(sunlite_if->usb_dev, 2), sunlite_if->inbuffer, sizeof(sunlite_if->inbuffer), sunlite_bulk_urb_complete, sunlite_if);
	      */
	      usb_fill_bulk_urb (purb, sunlite_if->usb_dev, usb_rcvbulkpipe(sunlite_if->usb_dev, 2), sunlite_if->bulkout_buffer, sizeof(sunlite_if->bulkout_buffer), sunlite_bulk_urb_complete, sunlite_if);
	      status=usb_submit_urb(purb,0);
	      if (status)
		printk("sunlite_work: urb submit error: %d\n", status);
	      atomic_set(&sunlite_if->urb_submit_pending, 1);
	    }
	}
      while (sunlite_if->wkrun);

      if (atomic_read(&sunlite_if->urb_submit_pending))
	printk("sunlite_work: waiting for urb submit completion\n");
      while (purb && atomic_read(&sunlite_if->urb_submit_pending))
	mdelay(5);
      if (purb)
	usb_free_urb(purb);
    }

  /* USBDMX2 */
  else if (sunlite_if->usb_dev->descriptor.idProduct == USB_PRODUCT_EZ_USB2a)
    {
      do
	{
	  /*
	   * Check for pending data before going to sleep and wait for a signal that
	   * data is pending is nessesary here, because the interruptible_sleep_on_timeout
	   * only returns for signals arived while it is executed. If we don't check data_pending
	   * here, we may loose some updates.
	   */
	  while (sunlite_if->data_pending != 0) sunlite_tx_event(sunlite_if);
	  /*
	   * if data is pending -> update it to the interface
	   */
	  wait_event_interruptible_timeout(sunlite_if->waitqueue, (sunlite_if->data_pending != 0), HZ);
	}
      while (sunlite_if->wkrun);
    }
  else
    {
      printk("sunlite_work: interface type not recognized! idProduct=0x%x\n", sunlite_if->usb_dev->descriptor.idProduct);
    }

  sunlite_if->wkstatus = 0;
  wake_up_interruptible(&sunlite_if->stop_waitqueue);
  printk ("sunlite_work: exiting wkstatus = %d\n",sunlite_if->wkstatus);
  return;
}

/*-----------------------------------------------------------
 *-- sunlite_delete_interface
 *--
 *--
 *---------------------------------------------------------*/
static int sunlite_delete_interface (DMXInterface *dif)
{
  sunlite_interface_t *sunlite_if = (sunlite_interface_t *)dif->user_data;
  int ret = 0;

  printk ("sunlite_delete_interface (DMXInterface *dif=%p)\n", dif);

  if (dif && dif->user_data)
    {
      sunlite_if->wkrun = 0;
      while (sunlite_if->wkstatus)
	{
	  ret = wait_event_interruptible_timeout(sunlite_if->stop_waitqueue, sunlite_if->wkstatus,100);
	}
      ret = cancel_delayed_work(sunlite_if->wk);

      flush_workqueue(sunlite_if->wkq);
      destroy_workqueue(sunlite_if->wkq);

      if (sunlite_if->sunlite_universe)
	sunlite_if->sunlite_universe->sunlite_if = NULL;
      sunlite_if->sunlite_universe = NULL;
      sunlite_if->dmx_interface = NULL;

      FREE(dif->user_data);
      dif->user_data = NULL;
    }
  printk ("sunlite_delete_interface: end \n");
  return 0;
}

/*-----------------------------------------------------------
 *-- sunlite_create_interface
 *--
 *-- Creates a dmx-interface for USBDMX2 hardware.
 *---------------------------------------------------------*/
static int sunlite_create_interface (DMXInterface *dif, DMXPropList *pl)
{
  int ret;

  printk ("sunlite_create_interface (dif=%p, pl=%p)\n", dif, pl);
  if (dif)
    {
      sunlite_interface_t *sunlite_if = NULL;
      struct usb_device   *usbdev     = NULL;
#if 0
      unsigned int               ifnum = 0;
#endif

      if (pl && pl->find)
	{
	  DMXProperty *p = pl->find(pl, "usbdev");
	  if (p)
	    p->get_long (p, (unsigned long *)&usbdev);
	}

      if (!usbdev) /* fail to create, if no usbdevice has been given */
	{
	  printk("sunlite_create_interface: failed to evaluate usbdev parameter\n");
	  return -1;
	}

      if (usbdev->actconfig && usbdev->actconfig->string)
	{
	  printk("sunlite_create_interface: current configuration is \"%s\"\n", usbdev->actconfig->string);
	}

      sunlite_if = DMX_ALLOC(sunlite_interface_t);
      if (sunlite_if)
	{
	  dif->user_data = (void *)sunlite_if;
	  dif->user_delete = sunlite_delete_interface;

	  sunlite_if->sunlite_universe = NULL;
	  sunlite_if->dmx_interface    = dif;
	  sunlite_if->usb_dev          = usbdev; /* usb-device handle */
	  init_waitqueue_head (&sunlite_if->waitqueue);
	  init_waitqueue_head (&sunlite_if->stop_waitqueue);
	  sunlite_if->wkq = create_singlethread_workqueue("sunlite_wq");
	  if (sunlite_if->wkq == NULL)
	    {
	      printk("sunlite_create_interface : cannot create work queue\n");
	      FREE(sunlite_if);
	      return -1;
	    }
	  sunlite_if->wk = DMX_ALLOC(struct work_struct);
	  if (sunlite_if->wk == NULL)
	    {
	      printk("sunlite_create_interface : cannot alloc work struct\n");
	      FREE(sunlite_if);
	    }
	  INIT_WORK(sunlite_if->wk, sunlite_work, (void*)sunlite_if);

	  sunlite_if->data_pending = 0; /* no data pending at the beginning */
	  /*
	    if(usbdev->descriptor.idProduct == USB_PRODUCT_EZ_USB)
	    {
	    printk ("initialize usbdmx1 hardware\n");
	    sunlite_init_0 (sunlite_if);
	    }
	  */
	  if(usbdev->descriptor.idProduct == USB_PRODUCT_EZ_USB2a)
	    {
	      printk ("sunlite_create_interface: initialize bulkout-buffer.\n");
	      sunlite_init_packet (sunlite_if->bulkout_buffer);
	    }

	  if(usbdev->descriptor.idProduct == USB_PRODUCT_EZ_USB3a)
	    {
	      printk ("sunlite_create_interface: initialize USBDMX-IN hardware\n");
	      sunlite_in_init (sunlite_if);
	    }

	  /* queue work */

	  if(inputrate<1)
	    inputrate=1;
	  printk ("sunlite_create_interface: work input rate = %d\n", inputrate);
	  sunlite_if->delay=HZ/inputrate;

	  sunlite_if->wkrun = 0;
	  ret = queue_work(sunlite_if->wkq, sunlite_if->wk);
	  if (ret < 0)
	    {
	      printk ("sunlite_create_interface: work cannot be queued\n");
	      FREE(sunlite_if->wk);
	      FREE(sunlite_if);
	      return -1;
	    }
	  return 0;
	}
    }
  return -1;
}

/* usb probe & disconnect */
/*-----------------------------------------------------------
 *-- sunlite_probe
 *--
 *-- Is called after a USB device has been added
 *-- to the bus and check wether it is a USBDMX2.
 *-- If so, it initializes it and creates as much
 *-- DMX universes as the interface provides.
 *---------------------------------------------------------*/
static int sunlite_probe (struct usb_interface *usb_if, const struct usb_device_id *id)
{
  const int              sunlite_info_size = sizeof(sunlite_device_info) / sizeof(sunlite_device_info[0]);
  int                    device_info_idx   = -1;
  sunlite_device_info_t *device_info       = NULL;
  DMXDriver             *drv               = NULL;
  struct usb_device     *dev               = NULL;

  if (usb_if == NULL)
    {
      printk ("sunlite_probe: FATAL: usb_if is NULL\n");
      return -ENODEV;
    }
  if (id == NULL)
    {
      printk ("sunlite_probe: FATAL: id is NULL\n");
      return -ENODEV;
    }

  device_info_idx = id->driver_info;
  device_info     = (device_info_idx>=0 && device_info_idx < sunlite_info_size) ? &sunlite_device_info[device_info_idx] : NULL;
  dev             = interface_to_usbdev(usb_if);

  if (device_info == NULL)
    {
      printk ("sunlite_probe: FATAL: device_info is NULL\n");
      return -ENODEV;
    }
  if (dev == NULL)
    {
      printk ("sunlite_probe: FATAL: dev is NULL\n");
      return -ENODEV;
    }

  printk ("sunlite_probe: VENDOR: 0x%X  PRODUCT: 0x%X\n", dev->descriptor.idVendor, dev->descriptor.idProduct);

  /*
   * Check if it is a known USBDMX2 release.
   */

  if (dev->descriptor.idVendor == USB_VENDOR_DIGITALARTSYSTEM)
    {
      if (dev->descriptor.idProduct == USB_PRODUCT_EZ_USB)
	printk("sunlite_probe: Sunlite USBDMX1 found at address %d\n", dev->devnum);
      else if(dev->descriptor.idProduct == USB_PRODUCT_EZ_USB2 )
	printk("sunlite_probe: Sunlite USBDMX2-firmware device found at address %d\n", dev->devnum);
      else if(dev->descriptor.idProduct == USB_PRODUCT_EZ_USB2a )
	printk("sunlite_probe: Sunlite USBDMX2 device found at address %d\n", dev->devnum);
      else if(dev->descriptor.idProduct == USB_PRODUCT_EZ_USB3 )
	printk("sunlite_probe: Sunlite USBDMX-IN-firmware device found at address %d\n", dev->devnum);
      else if(dev->descriptor.idProduct == USB_PRODUCT_EZ_USB3a )
	printk("sunlite_probe: Sunlite USBDMX-IN device found at address %d\n", dev->devnum);
      else
	{
	  printk("sunlite_probe: Sunlite USBDMX model not supported/tested. (id=%d)\n", dev->descriptor.idProduct);
	  return -ENODEV;
	}
    }
  else if (dev->descriptor.idVendor == 0xb334 && dev->descriptor.idProduct == 0x01)
    {
      printk("sunlite_probe: Twilight device found at address %d\n", dev->devnum);
    }
  else
    {
      printk("sunlite_probe: Unknown device : vendor=%d product=%d devnum=%d\n", dev->descriptor.idVendor,dev->descriptor.idProduct, dev->devnum);
      return -ENODEV;
    }

  /*------------------------------------------
  //-- Upload the firmware if nessesary.
  //------------------------------------------
  // device_info->firmware is not NULL, then upload the firmware */

  if ( (dev->descriptor.idVendor == USB_VENDOR_DIGITALARTSYSTEM) &&
       (dev->descriptor.idProduct == USB_PRODUCT_EZ_USB2 || dev->descriptor.idProduct == USB_PRODUCT_EZ_USB3 ) )
    {
      int err;
      printk("sunlite_probe: loading sunlite firmware...\n");
      err = sunlite_startup (dev, dev->descriptor.idProduct, device_info->firmware);
      if (err)
	{
	  printk("sunlite_probe: ...failed\n");
	  usb_set_intfdata(usb_if,(void *)0xDEADBEEF);
	  return err;
	}
      else
	{
	  printk("sunlite_probe: ...succeded\n");
	  usb_set_intfdata(usb_if,(void *)0xFA57FEED);
	  return err;
	}
    }
  else if (dev->descriptor.idVendor == 0xb334 && dev->descriptor.idProduct == 0x01)
    {
      int err;
      printk("sunlite_probe: loading twilight firmware...\n");
      err = sunlite_startup (dev, 0x1, device_info->firmware);
      if (err)
	{
	  printk("sunlite_probe: ...failed\n");
	  usb_set_intfdata(usb_if,(void *)0xDEADBEEF);
	  return err;
	}
      else
	{
	  printk("sunlite_probe: ...succeded\n");
	  usb_set_intfdata(usb_if,(void *)0xFA57FEED);
	  return err;
	}
    }

  /* if device_info->universes_out or device_info->universes_in is not
     0 then we have an input or an output or both and then we
     initialize the device and create one or more universes. */
#if 0
  if (dev->actconfig)
    {
      if (dev->actconfig->string)
	printk("sunlite_probe: configuration changed to \"%s\"\n", dev->actconfig->string);
      else
	printk("sunlite_probe: dev->actconfig->string = NULL\n");
    }
  else
    printk("sunlite_probe: dev->actconfig = NULL\n");
#endif
  /*
   * Create a Sunlite specific record and a DMX universe.
   */

  drv = dmx_find_driver (sunlite_family, "sunlite_driver");
  if (drv)
    {
      DMXUniverse  *dmx_u=NULL;
      DMXInterface *dmx_if = drv->create_interface (drv, dmxproplist_vacreate ("usbdev=%l", (long)dev));
      if (dmx_if ==  NULL)
	{
	  printk("sunlite_probe: create interface returns NULL interface\n");
	  return -ENODEV;
	}
      if (dmx_if->user_data ==  NULL)
	{
	  printk("sunlite_probe: create interface returns NULL interface's user data\n");
	  dmx_if->delete(dmx_if);
	  return -ENODEV;
	}

      if (dev->descriptor.idProduct == USB_PRODUCT_EZ_USB3a)
	dmx_u = dmx_if->create_universe (dmx_if, 1, NULL); /* input */
      if (dev->descriptor.idProduct == USB_PRODUCT_EZ_USB2a || dev->descriptor.idProduct == USB_PRODUCT_EZ_USB)
	dmx_u = dmx_if->create_universe (dmx_if, 0, NULL); /* output */

      if (dmx_u)
	{
	  printk("sunlite_probe: universe created, dmx_if=0x%08lX\n", (unsigned long)dmx_if);
	  usb_set_intfdata(usb_if, dmx_if);
	  return 0;
	}
      else
	{
	  dmx_if->delete(dmx_if);
	  return -ENODEV;
	}
    }
  else
    printk("sunlite_probe: unable to find driver for dmx-family sunlite.sunlite\n");

  printk("sunlite_probe: returning -ENODEV\n");

  /*
   * Something failed.
   */

  return -ENODEV;
}

/*-----------------------------------------------------------
 *-- sunlite_disconnect
 *--
 *-- disconnect method for sunlite.
 *-- It is called after a USBDMX2 device has been
 *-- removed from the USB bus.
 *---------------------------------------------------------*/
static void sunlite_disconnect (struct usb_interface *usb_if)
{
  void* ptr = usb_get_intfdata(usb_if);
  if (ptr == NULL)
    printk("sunlite_disconnect : NULL tag\n");
  else if (ptr == (void *)0xFA57FEED)
    printk("sunlite_disconnect : fake firmware loader\n");
  else if (ptr == (void *)0xDEADBEEF)
    printk("sunlite_disconnect : 0xDEADBEEF tag\n");
  else
    {
      DMXInterface *dmx_if = (DMXInterface *)ptr;
      printk (KERN_INFO "sunlite_disconnect : will delete sunlite interface\n");
      if (dmx_if->delete)
	dmx_if->delete(dmx_if);  /* does a cascaded delete on the universes of that interface */
    }
  printk ("sunlite_disconnect : exiting\n");
}

/*-----------------------------------------------------------
 *-- sunlite_init
 *--
 *-- Called after the module has been loaded.
 *---------------------------------------------------------*/
static int __init sunlite_init(void)
{
  DMXDriver *drv = NULL;
  int ret;

  sunlite_family = dmx_create_family("sunlite_family");
  if (!sunlite_family)
    {
      printk (KERN_INFO "sunlite_init: unable to register dmx-family Sunlite\n");
      return -1;
    }

  drv = sunlite_family->create_driver (sunlite_family, "sunlite_driver", sunlite_create_universe, NULL);
  if (!drv)
    {
      sunlite_family->delete(sunlite_family, 0);
      printk("sunlite_init: failed to create Sunlite driver\n");
      return -1;
    }

  drv->num_out_universes = 1;
  drv->num_in_universes = 1;
  drv->user_create_interface = sunlite_create_interface;

  ret = usb_register(&sunlite_driver);
  if (ret < 0)
    {
      sunlite_family->delete(sunlite_family, 0);
      printk("sunlite_init: failed to register Sunlite\n");
    }
  else
    printk("sunlite_init: Sunlite registered\n");

  return ret;
}

/*-----------------------------------------------------------
 *-- sunlite_cleanup
 *--
 *-- Called to do the cleanup.
 *---------------------------------------------------------*/
static void __exit sunlite_cleanup(void)
{
  usb_deregister(&sunlite_driver);
  mdelay(5);
  if (sunlite_family)
    sunlite_family->delete(sunlite_family, 0);
}

module_init(sunlite_init);
module_exit(sunlite_cleanup);
/*
  EXPORT_NO_SYMBOLS;
*/

#endif /* VERSION macro */
