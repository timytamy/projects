/*
 * usb2dmx.h
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

#define VENDOR_ID_LIGHTINGSOLUTIONS  (0x0CE1)    /* Vendor ID for http://www.lightingsolutions.de */


#define _DMX_MRG (0<<2)  /* merging commands     */
#define _DMX_RX  (1<<2)  /* receiver commands    */
#define _DMX_TX  (2<<2)  /* transmitter commands */

#define _DMX_MEM       0
#define _DMX_STARTCODE 1
#define _DMX_SLOTS     2
#define _DMX_FRAMES    3

#define USB2DMX_CONFIGURATION_OUT   (1)
#define USB2DMX_CONFIGURATION_IO    (2)

/* 
 * USB2DMX-VendorRequests
 */
#define DMX_ID_LED       0x02
#define DMX_MRG_MEM      (_DMX_MRG | _DMX_MEM)
#define DMX_TX_MEM       (_DMX_TX | _DMX_MEM)
#define DMX_TX_SLOTS     (_DMX_TX | _DMX_SLOTS)
#define DMX_TX_STARTCODE (_DMX_TX | _DMX_STARTCODE)
#define DMX_TX_FRAMES    (_DMX_TX | _DMX_FRAMES)
#define DMX_RX_MEM       (_DMX_RX | _DMX_MEM)
#define DMX_RX_SLOTS     (_DMX_RX | _DMX_SLOTS)
#define DMX_RX_STARTCODE (_DMX_RX | _DMX_STARTCODE)
#define DMX_RX_FRAMES    (_DMX_RX | _DMX_FRAMES)

#define DMX_ERROR	 2

/* END */
