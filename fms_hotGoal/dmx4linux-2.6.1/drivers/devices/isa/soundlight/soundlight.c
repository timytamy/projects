/*
 * soundlight.c
 * Driver for the ISA based dmx cards from Soundlight.
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
/*
 *  Should also work with the LightLine DMX-Interface presented at the
 *  german electronic magazine ELRAD, that should be detected as SLH1512A.
 */
/* #define delete(x) (((x)&&(x)->delete)?((x)->delete(x),0):0) */

#include "slh.h"

#include <linux/module.h>
#include <linux/init.h>

#define SLH_IO_NUM 4
static const int defaultio[SLH_IO_NUM] = {0x100, 0x120, 0x140, 0x160};

MODULE_AUTHOR("Michael Stickel <michael@cubic.org> http://llg.cubic.org");
MODULE_DESCRIPTION("This is the dmx driver module for the soundlight cards PC1512A, PC1512B-LC, PC1512B, PC1512C and ELRAD's Lightline version " DMXVERSION);
MODULE_LICENSE("GPL");

int  slh_autoprobe_card    (SLHCard *card);
int  get_card_capabilities (SLHCard *card);



DMXDriver *slh_autoprobe_driver (DMXFamily *f, DMXPropList *pl)
{
  printk (KERN_INFO "--------[ slh_autoprobe_driver (%p,%p) ]----------\n", f, pl);
  if (pl && pl->find)
    {
      long     iobase = 0L;
      SLHCard  testcard;

      DMXProperty *p = pl->find(pl, "iobase");

      if (!p || !p->get_long)
        {
          printk (KERN_INFO "can not find property iobase\n");
	  return NULL;
        }

      p->get_long(p, &iobase);

      printk (KERN_INFO "slh_autoprobe_driver at iobase=0x%03lX\n", iobase);

      testcard.iobase = iobase; /* the only thing that is needed for autoprobing */
      if (slh_autoprobe_card (&testcard) >= 0)
        {
          DMXDriver *drv = NULL;

	  printk (KERN_INFO "Soundlight card at io=0x%03lX\n", iobase);
          printk (KERN_INFO "slh_autoprobe_driver: type %s\n", CARDTYPE2NAME(testcard.type));
          switch (testcard.type)
	    {
            case CARDTYPE_1512A: drv = dmx_find_driver (f, "slh1512a"); break;
            case CARDTYPE_1512B: drv = dmx_find_driver (f, "slh1512b"); break;
            case CARDTYPE_1512C: drv = dmx_find_driver (f, "slh1512c"); break;

            default:
              printk (KERN_INFO "slh_autoprobe_driver: illegal %d\n", testcard.type);
              break;
	    }

          if (drv)
            {
              printk (KERN_INFO "driver of name %s found for card at io=%lX\n", drv->name, iobase);
              return drv;
            }

          printk (KERN_INFO "no driver found\n");
	}
      else
	printk (KERN_INFO "NO Soundlight card found at io=0x%03lX\n", iobase);
    }
  printk (KERN_INFO "---------[ slh_autoprobe_driver: not found ]-------------\n");
  return NULL;
}

static DMXFamily *family = NULL;

static int __init slh_init(void)
{
  int i;
  DMXInterface *dmxif_a[SLH_IO_NUM] = { 0 };
  family = dmx_create_family ("ISA");
  if (family)
    {
      DMXDriver *drv_a=NULL, *drv_b=NULL, *drv_c=NULL;

      family->autoprobe_driver = slh_autoprobe_driver;

      drv_a = family->create_driver (family, "slh1512a", slh1512a_create_universe, NULL);
      if (drv_a)
        {
          drv_a->num_out_universes = 1;
          drv_a->user_create_interface = slh1512a_create_interface;
        }

      drv_b = family->create_driver (family, "slh1512b", slh1512b_create_universe, NULL);
      if (drv_b)
        {
          drv_b->num_out_universes = 1;
          drv_b->user_create_interface = slh1512b_create_interface;
        }

      drv_c = family->create_driver (family, "slh1512c", slh1512c_create_universe, NULL);
      if (drv_c)
        {
          drv_c->num_out_universes = drv_c->num_in_universes = 2;
          drv_c->user_create_interface = slh1512c_create_interface;
        }

      for (i=0; i<SLH_IO_NUM; i++)
        {
	  DMXInterface *dmxif;
          printk (KERN_INFO "try create soundlight-interface with iobase=0x%03X\n", defaultio[i]);
          dmxif = family->create_interface (family, dmxproplist_vacreate("iobase=%l", (long)defaultio[i]));
          printk (KERN_INFO "create soundlight-interface %s\n", dmxif?"successfull":"failed");

          if (dmxif && dmxif->driver)
            {
              int j;

              printk (KERN_INFO "create %d output and %d input universes\n", dmxif->driver->num_out_universes, dmxif->driver->num_in_universes);

              for (j=0; j<dmxif->driver->num_out_universes; j++)
		dmxif->create_universe (dmxif, 0, NULL);

              for (j=0; j<dmxif->driver->num_in_universes; j++)
		dmxif->create_universe (dmxif, 1, NULL);
            }

	  dmxif_a[i]=dmxif;
        }
    }

  /* check if an interface was found */
  for (i=0; i<SLH_IO_NUM; i++)
    if(dmxif_a[i])
      return 0;

  printk (KERN_INFO "No Soundlight ISA DMX-Interface found\n");
  return -1;
}


/*
 * tries to clean up anything.
 */
static void __exit slh_exit(void)
{
  if (family)
    family->delete (family, 0);
}


module_init(slh_init);
module_exit(slh_exit);
