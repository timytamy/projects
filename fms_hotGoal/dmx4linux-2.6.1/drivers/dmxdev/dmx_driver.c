/*
 * dmx_driver.c
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

#include <dmxdev/dmxdevP.h>
#include <dmxdev/llist.h>

#define __NO_VERSION__
#include <linux/module.h>
#include <linux/slab.h>

#define  FAMILY_DRV_SEPERATOR ('/')


DMXDriver * dmx_alloc_driver (void)
{
  DMXDriver *d = (DMXDriver *)kmalloc (sizeof(DMXDriver), GFP_KERNEL);
  if (d)
    {
      d->next       = NULL;

      strcpy(d->name,"noname");

      d->family     = NULL;
      d->interfaces = NULL;

      d->create = NULL;
      d->delete = NULL;
      d->write_slots = NULL;
      d->read_slots  = NULL;
      d->create_interface = NULL;
      d->user_create_interface  = NULL;
      d->driver_create_universe = NULL;
      d->getUniverseID = NULL;
    }
  return d;
}

void dmx_free_driver (DMXDriver *d)
{
  if (d)
    kfree (d);
}



DMXDriver *dmx_create_driver (
                              DMXFamily *f,
                              char *name,
                              int (*create) (DMXUniverse *, DMXPropList *),
                              DMXPropList *values
                             )
{
  DMXDriver *d = NULL;
  /* <family>,<name> and <create> must be valid and <values> must fit to <num_values> */
  if (f && name)
    {
      d = dmx_alloc_driver ();
      if (d)
        {
       /* d->name   = strdup (name); */
          strcpy(d->name, name);

          d->create = NULL;
          d->delete = dmx_delete_driver;
          d->create_interface = dmx_create_interface;

          d->interfaces = NULL;

          d->driver_create_universe = create;

          /*
           * default methods for a newly created universe
           */
          d->write_slots = NULL;
          d->read_slots  = NULL;

          d->user_create_interface = NULL;

          d->num_in_universes = 0;
          d->num_out_universes = 0;

          d->family = f;

          LLIST_INSERT(f->drivers, d);

          printk (KERN_INFO "dmx-driver %s created\n", name);
        }
    }
  else
    printk (KERN_INFO "dmx_create_driver(): invalid parameter");
  return d;
}


static void dmx_remove_driver (DMXDriver *d)
{
  if (d && d->family && d->family->drivers)
    {
      LLIST_REMOVE(d->family->drivers, d);
    }
}

int dmx_delete_driver (DMXDriver *d)
{
  if (d)
    {
      DMXInterface *i = NULL;

      char name[64];
      strcpy (name, "<NULL>");
      if (d->name)
	strcpy (name, d->name);

      d->create_interface = NULL;
      dmx_remove_driver (d);

      while ((i=d->interfaces))
	if (i->delete)
          i->delete(i);

      dmx_free_driver (d);


      printk (KERN_INFO "dmx-driver %s deleted\n", name);
    }
  return 0;
}

/*
 *
 */
DMXDriver *dmx_find_driver (DMXFamily *f, char *name)
{
  DMXDriver *d = NULL;
  for (d = f->drivers; d; d=d->next)
    {
      if (!strcmp(d->name, name))
	return d;
    }
  return NULL;
}

DMXDriver *dmx_find_familydriver (char *name)
{
  DMXDriver *drv = NULL;
  char tname[64];
  char *dname = NULL;
  int i;

  strcpy(tname,name);

  for (i=0; !dname && tname[i]; i++)
    if (tname[i]==FAMILY_DRV_SEPERATOR)
      {
	dname = &tname[i+1];
	tname[i]=0;
      }

  if (dname)
    drv = dmx_find_driver(dmx_find_family (tname), dname);

  return drv;
}
