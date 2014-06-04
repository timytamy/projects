/*
 * dmx_family.c
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

/*
 * This is the root of the family chain.
 */
static DMXFamily         *root_family = NULL;

/*static struct semaphore   family_modify_sem = MUTEX;*/


/*
 * makes the root family accessible to other parts of dmx4linux
 */
DMXFamily *dmx_get_root_family (void)
{
  return root_family;
}

/*
 * name: dmx_alloc_family
 * func: allocates a structure of type DMXFamily.
 */
static DMXFamily* dmx_alloc_family (void)
{
  DMXFamily *f = (DMXFamily *)kmalloc(sizeof(DMXFamily), GFP_KERNEL);
  return f;
}


/*
 * name: dmx_free_family
 * func: frees a previously allocated DMXFamily structure.
 */
static void dmx_free_family (DMXFamily *f)
{
  if (f)
    kfree (f);
}


/*
 * This is the only function that has to be exported.
 * With this function you can create a new driver family
 * and create new drivers in it.
 */
DMXFamily *dmx_create_family (char *name)
{
  DMXFamily *f = dmx_alloc_family ();
  if (f)
    {
/*      f->name    = strdup (name);*/
      strcpy (f->name, name);
      f->name[31] = 0; /* for assurance */


      f->drivers = NULL;
      f->num_drivers = 0;

      f->create  = NULL;
      f->delete  = dmx_delete_family;

      f->create_driver = dmx_create_driver;

      f->user_data   = NULL;

      f->autoprobe_driver = NULL;
      f->create_interface = dmx_family_create_interface;

      /*
       * now insert it into the chain.
       * This should be the only place to do this.
       */
      LLIST_INSERT(root_family, f);
      printk (KERN_INFO "family %s created\n", name);
    }
  return f;
}


/*
 * remove the family from the cain of families.
 */
static void remove_from_chain (DMXFamily *f)
{
  if (!f) return;
  LLIST_REMOVE(root_family, f);
}


/*
 *  this is called to delete the family. If <constraint> is != 0
 *  all drivers and their universes are also deleted else an error
 *  is returned if there are undeleted drivers.
 */
int dmx_delete_family (DMXFamily *f, int constraint)
{
  printk (KERN_INFO "dmx_delete_family called\n");
  if (f)
    {
      char name[64];
      strcpy (name,"<NULL>");
      if (f->name)
	strcpy (name, f->name);

      /*
       * First to do here is to remove it from the chain.
       * This is the only place to do so.
       */
      remove_from_chain (f);

      /*
       * delete all drivers for that family
       */
      while (f->drivers)
        {
          DMXDriver *d = f->drivers;
          f->drivers = f->drivers->next;
          if (d->delete) d->delete(d);
        }
      dmx_free_family(f);

      printk (KERN_INFO "family %s deleted\n", name);
    }
  return -1;
}


DMXFamily *dmx_find_family (const char *name)
{
  DMXFamily *f = NULL;

  for (f = root_family; f && strcmp(f->name, name); f=f->next);

  return f;
}
