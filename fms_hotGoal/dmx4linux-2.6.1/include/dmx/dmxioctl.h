/*
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

#ifndef __DMX_IOCTL_H__
#define __DMX_IOCTL_H__

#ifndef __KERNEL__
#include <sys/ioctl.h>
#endif

struct dmx_conf
{
    /* The following two are the primary key for pointing to a unique universe */
    int  universe;     /* index to the universe  */
    char direction;    /* DMX_IN, DMX_OUT or may be DMX_THRU */

    unsigned int   channels;  /* number of channels to output (1..512) */
    unsigned char  startcode; /* startcode to send */

    unsigned char  saverdmx:1; /* send checksum after last slot ? */

    unsigned long  framecount;
} __attribute__ ((packed));

struct dmx_parameter
{
    /* The following two are the primary key for pointing to a unique universe */
    char direction;    /* DMX_IN, DMX_OUT or may be DMX_THRU */
    int  universe;     /* index to the universe  */

    char name[40];
    char value[40];
} __attribute__ ((packed));

struct dmx_capabilities
{
    /* The following two are the primary key for pointing to a unique universe */
    int  universe;     /* index to the universe  */
    char direction;    /* DMX_IN, DMX_OUT or may be DMX_THRU */

    char family[20];   /* name of the driver that accesses the card */
    char driver[20];   /* name of the card that is used (detected)  */

    char connector[20]; /* name of the connector that the universe uses */
    int  conn_id;       /* interface wide id of the connector the universe uses */

    int  breaksize;    /* in uS */
    int  mabsize;      /* in uS */
    int  maxSlots;     /* in number of slots (channels) */
} __attribute__ ((packed));

struct dmx_parm_names
{
  int   universe;
  char  direction;
  int   offset;       /* index of first name to return. */
  int   size;         /* the number of properties for that universe. */
  int   num_names;    /* set by dmxdev to the number of strings returned in this call. */
  char  names[200];   /* the strings,  */
} __attribute__ ((packed));

struct dmx_info
{
  int  version_major;
  int  version_minor;
  int  max_out_universes;
  int  max_in_universes;
  int  used_in_universes;
  int  used_out_universes;

  int num_families; /* number of families available */
  int num_entries;  /* number of families copied into this structure */
  char family_names[200];
} __attribute__ ((packed));


struct dmx_family_info
{
  int  num_drivers; /* number of drivers available */
  int  num_entries; /* number of drivers copied into this structure */
  int  offset;      /* number of driver-names to ignore before copying into structure */
  char familyname[50];
  char driver_names[200];
} __attribute__ ((packed));


struct dmx_createinterface_parm
{
  char name[50]; /* combined name: family/driver */
  char properties[150]; /* the properties that are given to the interface */
  int id; /* return id of interface. */
} __attribute__ ((packed));

struct dmx_createuniverse_parm
{
  int interface_id;
  int preferred_id;
  int direction;
  char properties[200]; /* the properties that are given to the universe */
} __attribute__ ((packed));

struct dmx_deleteuniverse_parm
{
  int  id;
  int  direction;
} __attribute__ ((packed));


#define DMX_DIRECTION_OUTPUT   (0)
#define DMX_DIRECTION_INPUT    (1)

#define DMX_UNDEFINED  (-1)

#define DMX_IOCTL_TYPE   ('D')

#define DMX_IOCTL_GET_CAP        _IOR(DMX_IOCTL_TYPE,3,struct dmx_capabilities)
#define DMX_IOCTL_GET_PARM	 _IOR(DMX_IOCTL_TYPE,4,struct dmx_parameter)
#define DMX_IOCTL_SET_PARM	 _IOW(DMX_IOCTL_TYPE,5,struct dmx_parameter)
#define DMX_IOCTL_GET_PARM_NAMES _IOW(DMX_IOCTL_TYPE,6,struct dmx_parm_names)

#define DMX_IOCTL_GET_INFO       _IOR(DMX_IOCTL_TYPE,8,struct dmx_info)
#define DMX_IOCTL_GET_DRIVERS    _IOR(DMX_IOCTL_TYPE,9,struct dmx_family_info)

#endif
