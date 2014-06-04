/*
 * dgmpci.h : header for the digimedia dmx pci card driver
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

#ifndef _dgmpci_h
#define _dgmpci_h

#include "dgm.h"

#define DGMPCI_VENDOR_ID_0      0x10b5
#define DGMPCI_DEVICE_ID_0      0x2544
#define DGMPCI_BOARD_TYPE_0     0
#define DGMPCI_BOARD_INFO_0     "Digimedia - Soundlight DMX card 1514PCI"
#define DGMPCI_MEMORY_OFFSET_0  (0x0000)
#define DGMPCI_MEMORY_REGION_0  2

#define DGMPCI_VENDOR_ID_1      0x10b5
#define DGMPCI_DEVICE_ID_1      0x2545
#define DGMPCI_BOARD_TYPE_1     1
#define DGMPCI_BOARD_INFO_1     "Digimedia - Soundlight DMX card 2514PCI"
#define DGMPCI_MEMORY_OFFSET_10 (0x0000)
#define DGMPCI_MEMORY_OFFSET_11 (0x0800)
#define DGMPCI_MEMORY_REGION_10 2
#define DGMPCI_MEMORY_REGION_11 2

#endif /* _dgmpci_h */
