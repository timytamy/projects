/*
 * dmx_universe.c
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

static DMXUniverse *out_universes[MAX_UNIVERSES] = {NULL,};
static DMXUniverse *in_universes[MAX_UNIVERSES] = {NULL,};


static int count_universes (DMXUniverse *u[], int count)
{
  int i;
  int num = 0;
  for (i=0; i < count; i++)
    if (u[i])
      num++;
  return num;
}

int number_input_universes ()
{
  return count_universes(in_universes, sizeof(in_universes)/sizeof(in_universes[0]));
}

int number_output_universes ()
{
  return count_universes(out_universes, sizeof(out_universes)/sizeof(out_universes[0]));
}


/*
 * kind is 0 for output universes and 1 for inputs.
 */
static int get_free_universe (DMXUniverse *u, int kind)
{
  if (kind>=0 && kind<=2)
    {
      DMXUniverse **universes = (kind==0)?out_universes:in_universes;
      int i;
      for (i=0; i<MAX_UNIVERSES; i++)
	if (!universes[i])
	  {
	    universes[i] = u;
	    return i;
	  }
    }
  return -1;
}



static int release_universe (DMXUniverse *u, int idx)
{
  if (idx>=0 && idx<MAX_UNIVERSES)
    {
      if (out_universes[idx] == u)
	out_universes[idx] = NULL;
      else if (in_universes[idx] == u)
	in_universes[idx] = NULL;
      else
	return -1;
      return 0;
    }
  return -1;
}


/*
 * kind is 0 for output universes and 1 for inputs.
 */
DMXUniverse *dmx_universe_by_index (int kind, int idx)
{
  if (idx>=0 && idx<MAX_UNIVERSES)
   return (kind==0)?out_universes[idx]:(kind==1)?in_universes[idx]:NULL;
  return NULL;
}




int dmx_delete_universe (DMXUniverse *u);


DMXUniverse* dmx_alloc_universe (void)
{
  DMXUniverse *u = DMX_ALLOC(DMXUniverse);
  if (u)
    {
      u->next  = NULL;
      u->index = -1;
      u->interface = NULL;
      strcpy(u->connector,"");
      u->conn_id = -1;
      u->props = NULL;
      u->current_universes=0;
      u->max_universes=0;
      u->create  = NULL;
      u->delete  = dmx_delete_universe;
      u->user_delete = NULL;
      u->write_slots = NULL;
      u->read_slots  = NULL;
      u->enable = NULL; /* enables/disables this universe */
      u->signal_changed = NULL; /* called by the receiver whenever input is available (or has changed) */
    }
  return u;
}

void dmx_free_universe (DMXUniverse *u)
{
  DMX_FREE(u);
}


/*
 * find a property for a universe.
 * This includes looking for the properties of the interface of the universe.
 */
static DMXProperty *dmx_universe_find_property (DMXUniverse *u, char *name)
{
  DMXProperty *p = NULL;

  if (u && name)
    {
      DMXPropList *pl = u->props;

      if (pl && pl->find)
        p = pl->find (pl, name);

      if (!p && u->interface)
        p = u->interface->findprop (u->interface, name);
    }
  return p;
}


/*
 * Create a new universe
 */
DMXUniverse *dmx_create_universe (DMXInterface *i, int kind, DMXPropList *props)
{
  DMXUniverse *u = NULL;

  if (kind<0 || kind>1)
    return NULL;

  /*
   * ensure that the parameters are correct
   */
  if (i && i->driver && i->driver->driver_create_universe)
    u = dmx_alloc_universe ();

  if (u)
    {
      int idx = -1;

      u->interface = i;
      u->kind = kind;
      u->props = NULL;
      u->findprop = dmx_universe_find_property;


      /*
       * unspecified connector, will be defined by the driver
       */
      strcpy(u->connector,"unknown");
      u->conn_id = -1;

      /*
       *  set the default read/write methods for that universe.
       */
      u->write_slots = i->driver->write_slots;
      u->read_slots  = i->driver->read_slots;


      /*
       * This method can ber overwritten by the
       * drivers create_function and will be called
       * before deletion.
       */
      u->user_delete = NULL;

      /*
       * call the driver specific init-function.
       */

      u->props = props?props:dmxproplist_create ();

      {
      /*
       * prepare properties
       */
        DMXProperty *p = NULL;

          if ((p = dmxprop_create_long("slots", 512L)))
            u->props->add(u->props, p);
        }


      if (i->driver->driver_create_universe (u, props))
        {
          if (u && u->props && u->props->delete)
            u->props->delete(u->props);
          dmx_free_universe (u);
          return NULL;
        }

      /* called by the receiver whenever input
       * is available (or has changed)
       */
      u->signal_changed = dmx_universe_signal_changed;

      idx = get_free_universe (u, kind);
      if (idx == -1)
        {
          if (u && u->props && u->props->delete)
            u->props->delete(u->props);
          dmx_free_universe(u);
          return NULL;
        }
      u->index = idx;

      LLIST_INSERT(u->interface->universes, u);

      dmx_universe_create_proc_entry (u);

      printk (KERN_INFO "universe %d created\n", u->index);
    }
  return u;
}



static int remove_universe (DMXUniverse *u)
{
  if (release_universe (u, u->index) < 0)
    printk (KERN_INFO "remove_universe: universe %p with id=%dnot found in universe-pool\n", u, u->index);


  if (u && u->interface && u->interface->universes)
    {
      LLIST_REMOVE(u->interface->universes, u);
    }
  return 0;
}


/*
 *  delete a universe (from-pool and interface)
 */
int dmx_delete_universe (DMXUniverse *u)
{
  if (u)
    {
      int idx = u->index;

      if (u->user_delete)
	u->user_delete (u);

      dmx_universe_delete_proc_entry (u);

      remove_universe (u);

      if (u && u->props && u->props->delete)
        u->props->delete(u->props);

      dmx_free_universe (u);

      printk (KERN_INFO "universe %d deleted\n", idx);
      return 0;
    }
  return -1;
}





