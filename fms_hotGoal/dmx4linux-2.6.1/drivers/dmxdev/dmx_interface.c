/*
 * dmx_interface.c
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

#define __NO_VERSION__
#include <linux/module.h>

#include <dmxdev/dmxdevP.h>
#include <dmxdev/llist.h>

#include <linux/slab.h>

DMXInterface *dmx_alloc_interface (void)
{
  DMXInterface *i = DMX_ALLOC(DMXInterface);
  return i;
}

void dmx_free_interface (DMXInterface *i)
{
  if (i) kfree (i);
}


static DMXProperty *dmx_interface_find_property (DMXInterface *dif, char *name)
{
  DMXProperty *p = NULL;
  if (dif && name)
    {
      DMXPropList *pl = dif->props;
      if (pl && pl->find)
        p = pl->find (pl, name);
    }
  return p;
}



/*
 *
 */
DMXInterface *dmx_family_create_interface (DMXFamily *f, DMXPropList *pl)
{
  if (f && f->drivers && f->autoprobe_driver)
    {
      DMXDriver *drv = f->autoprobe_driver (f, pl);

      if (drv)
        {
          DMXInterface *dmxif = drv->create_interface (drv, pl);
          if (dmxif)
              return dmxif;
        }
    }

  if (pl)
    pl->delete(pl);

  return NULL;
}



DMXInterface *dmx_create_interface (DMXDriver *drv, DMXPropList *props)
{
  DMXInterface *i = dmx_alloc_interface ();
  if (i)
    {
      i->next = NULL;

      i->driver = drv;
      i->props = NULL;

      i->num_universes = 0;
      i->universes = NULL;

      i->create_universe = dmx_create_universe;

      i->delete = dmx_delete_interface;
      i->user_delete = NULL;

      i->current_universes = 0;
      i->max_universes = 0;

      i->props = props?props:dmxproplist_create();
      i->findprop = dmx_interface_find_property;

      /*
       *  give the user-driver a chance to make some updates..
       */
      if (drv->user_create_interface && drv->user_create_interface (i, props) < 0)
        {
          dmx_free_interface (i);
          printk (KERN_INFO "interface creation failed\n");
          return NULL;
        }

      LLIST_INSERT(drv->interfaces, i);

      printk (KERN_INFO "interface of driver %s created\n", (i->driver)?(i->driver->name):"drv=NULL");
    }
  return i;
}


static int dmx_remove_interface (DMXInterface *i)
{
  if (i && i->driver && i->driver->interfaces)
    {
      LLIST_REMOVE(i->driver->interfaces, i);
    }
  return 0;
}


/*
 * removes an interface
 */
int dmx_delete_interface (DMXInterface *i)
{
  if (i)
    {
      DMXUniverse *u = NULL;
      /* if (wait(i->sem) < 0) return -1;
         wait(..) returns an error if semaphore is deleted
       */
      dmx_remove_interface (i);

      while ((u=i->universes))
        {
          if (u->delete)
            u->delete(u);
          else
            printk (KERN_INFO "FATAL: delete method for universe is NULL\n");
        }

      /*
       * notify user-driver that the universe has been deleted
       */
      if (i->user_delete)
        i->user_delete (i);

      if (i->props && i->props->delete)
        i->props->delete(i->props);

      dmx_free_interface(i);

      printk (KERN_INFO "interface deleted\n");
    }
  return 0;
}
