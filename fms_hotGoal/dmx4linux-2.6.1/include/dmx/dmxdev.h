/*
 * dmxdev.h
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

#ifndef _DMXDEV_H_
#define _DMXDEV_H_

#include <dmx/dmxconfig.h>
#include <dmx/dmxmem.h>
#include <dmx/dmxproperty.h>

typedef unsigned char DMXSlotType;


/*================[ New DMX4Linux Device structures ]===============*/

typedef struct struct_DMXDriver    DMXDriver;
typedef struct struct_DMXFamily    DMXFamily;
typedef struct struct_DMXUniverse  DMXUniverse;
typedef struct struct_DMXInterface DMXInterface;



/*
 *  A DMXUniverse
 */
struct struct_DMXUniverse
{
  DMXUniverse   *next;

  int            index;   /* universe number */

  DMXInterface  *interface;

  char connector[64];  /* free form string, filled by the driver */
  int  conn_id;        /* sequential number for the connector, is unique for each interface */


  DMXPropList   *props;
  DMXProperty   *(*findprop) (DMXUniverse *, char *);


  int kind; /* what kind is it of: 0=output, 1=input, 3=thru. Others are possible */

  unsigned int  current_universes;
  unsigned int  max_universes;

  DMXUniverse *(*create) (DMXUniverse *);
  int          (*delete) (DMXUniverse *);

  int          (*user_delete) (DMXUniverse *);

  int  (*write_slots) (DMXUniverse *, off_t offs, DMXSlotType *slots, size_t numslots);
  int  (*read_slots)  (DMXUniverse *, off_t offs, DMXSlotType *slots, size_t numslots);

  /* enables/disables this universe */
  int  (*enable) (DMXUniverse *, int on); /* on=1 enables, on=0 disables */

  /* called by the receiver whenever input is available (or has changed)
   * within a specified area or if <size> is 0 unconditional.
   */
  int  (*signal_changed) (DMXUniverse *, uint start, uint size);

  /*
   * returns < 0 for error, > 0 if new data is available and 0
   * if no new data is available for that universe.
   */
  int  (*data_available) (DMXUniverse *, uint start, uint size);

  void *user_data;
};


/*----------------------------------------------------*/


DMXDriver *dmx_create_driver (
                              DMXFamily *f,
                              char *name,
                              int (*create) (DMXUniverse *, DMXPropList *),
                              DMXPropList *values
                             );


struct struct_DMXFamily
{
  DMXFamily     *next;        /* next in chain */

#if 0
  char          *name;        /* familyname */
#else
  char           name[32];
#endif

  DMXDriver     *drivers;     /* drivers in family */
  unsigned int   num_drivers; /* number of drivers */


  DMXUniverse *(*create) (DMXFamily *);
  int          (*delete) (DMXFamily *, int constraint);

  DMXDriver   *(*create_driver) (DMXFamily *, char *name,
                                 int (*create) (DMXUniverse *, DMXPropList *),
                                 DMXPropList *values
                                );

  DMXDriver    *(*autoprobe_driver) (DMXFamily *, DMXPropList *);
  DMXInterface *(*create_interface) (DMXFamily *, DMXPropList *);

  /* create a driver of that family */

  void        *user_data;  /* pointer to user data */

};


struct struct_DMXInterface
{
  DMXInterface *next;

  DMXDriver    *driver;

  int           num_universes;
  DMXUniverse  *universes;

  long          current_universes;
  long          max_universes;


  DMXPropList   *props;
  DMXProperty   *(*findprop) (DMXInterface *, char *);


  DMXUniverse *(*create_universe) (DMXInterface *i, int kind, DMXPropList *values);

  int (*delete) (DMXInterface *);  /* delete method */
  int (*user_delete) (DMXInterface *);  /* delete method */

  void *user_data; /* user data */
};



struct struct_DMXDriver
{
  DMXDriver     *next;
#if 0
  char          *name;
#else
  char           name[32];
#endif
  DMXFamily     *family;

  DMXInterface *interfaces;

  int   num_in_universes;   /* number of available input universes */
  int   num_out_universes;  /* number of available output universes */

  DMXUniverse *(*create) (DMXDriver *);
  int          (*delete) (DMXDriver *);

  int  (*write_slots) (DMXUniverse *, off_t offs, DMXSlotType *slots, size_t numslots);
  int  (*read_slots)  (DMXUniverse *, off_t offs, DMXSlotType *slots, size_t numslots);

  DMXInterface *(*create_interface) (DMXDriver *, DMXPropList *values);

  int (*user_create_interface) (DMXInterface *, DMXPropList *values);

  int (*driver_create_universe) (DMXUniverse *, DMXPropList *); /* not used */

  int (*getUniverseID) (DMXUniverse *, char *id, size_t size);
};


DMXFamily   *dmx4l_create_family (char *name);
DMXDriver   *dmx4l_create_driver (DMXFamily *fam, char *name, DMXUniverse *(create_instance) (DMXDriver *) );

DMXFamily *dmx_get_root_family (void);
DMXFamily *dmx_create_family (char *name);
int        dmx_delete_family (DMXFamily *f, int constraint);
DMXFamily *dmx_find_family   (const char *name);


DMXDriver *dmx_create_driver (
                              DMXFamily *f,
                              char *name,
                              int (*create) (DMXUniverse *, DMXPropList *),
                              DMXPropList *values
                             );
int        dmx_delete_driver     (DMXDriver *d);
DMXDriver *dmx_find_driver       (DMXFamily *f, char *name);
DMXDriver *dmx_find_familydriver (char *name);


DMXInterface *dmx_create_interface (DMXDriver *drv, DMXPropList *values);
int dmx_delete_interface (DMXInterface *i);



/*
 * kind is 0 for dmx-output
 * and 1 for dmx-input
 */
DMXUniverse *dmx_universe_by_index (int kind, int idx);


DMXUniverse *dmx_create_universe (DMXInterface *i, int kind, DMXPropList *values);
int dmx_delete_universe (DMXUniverse *u);

void dmx_universe_create_proc_entry (DMXUniverse *u);
void dmx_universe_delete_proc_entry (DMXUniverse *u);

#endif
