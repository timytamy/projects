/*
 * EnLight.c
 * Device-driver for the EnLight card (ICCC Hamburg)
 * http://www.enlight.de/
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

#include <linux/version.h>
#include <linux/module.h>

#if !defined(CONFIG_ISA)
#error Linux Kernel needs ISA Bus support for the Enlight card
#endif

#include <linux/init.h>
#include <linux/slab.h>
#include <asm/io.h>

#include <dmx/dmxdev.h>

#define MODULENAME  "EnLight"

#define DEBUG 1

/*
 *  card specific defines
 */


#if 0
#define COLDSTART                       1
#define WARMSTART                       2

#define RESTART_DELAY                   10
#endif


/*
 * No struct is used he. We use the mmap read/write functions.
 * So porting to other Architectures should be easier.
 *
 * The address-map of the card looks like:
 * Offset       Function
 * 0x000-0x1ff  512 DMX values
 * 0x200-0x20f  card signature. Usualy 'EnLight!' or 'PIC_DMX'
 * 0x210-0x22f  application/driver signature. 32 byte RAM used by the application/driver.
 * 0x230-0x231  slot count. Number of slots per frame.
 * 0x232-0x233  mainloop-counter. Number of frames transfered since reset.
 * 0x234-0x3fe  reserved for extensions.
 * 0x3ff-0x3ff  reset-flag
 */



typedef struct
{
  char           type;
  unsigned long  membase;
  void          *cardptr;
} EnLightCard;


/*
 *  Hardware specific functions
 */

static int get_channels (EnLightCard *card)
{
  return *(volatile unsigned short *)((card->cardptr)+0x230);
}

static void set_channels (EnLightCard *card, unsigned int n)
{
  *(volatile unsigned short *)((card->cardptr)+0x230)=n;
}

static int get_mailloop_counter(EnLightCard *card)
{
  return *(volatile unsigned short *)((card->cardptr)+0x232);
}


/*
 * Make a check if a card is present at the <membase>
 * and if so of what kind it is.
 */
static int check_interface_type (unsigned long membase)
{
  int   cardtype = -1;
  char  cardsign[17];

  if (membase>=0xA000 && membase<0xF000)
    {
      void *ptr = ioremap/*_nocache*/ (membase<<4, 1024);
      if (ptr)
        {
          memcpy_fromio (cardsign, (ptr)+512, 16);
          cardsign[16]=0;
#ifdef DEBUG
	  printk(KERN_INFO MODULENAME ": sign at 0x%05lX: %s\n", (membase<<4)+512, cardsign);
#endif
          if (!strncmp (cardsign, "EnLight!", 8))
            cardtype = 'E';
          else if (!strncmp (cardsign, "PIC_DMX", 7))
            cardtype = 'P';

          if (cardtype > 0)
            printk (KERN_INFO MODULENAME ": found EnlightCard '%s' at segment 0x%04X\n", cardsign, (unsigned int)membase);
        }
      iounmap (ptr);
    }
  return cardtype;
}

/*
 * Try to create an interface for an EnLight-card at the
 * given <membase> and return the card specific structure.
 */
static EnLightCard *check_create_interface (unsigned long membase)
{
  char  cardtype = cardtype = check_interface_type (membase);
  if (cardtype > 0)
    {
      void *ptr = ioremap/*_nocache*/ (membase<<4, 1024);
      if (ptr)
        {
          EnLightCard *card = DMX_ALLOC(EnLightCard);
          if (card)
            {
              card->cardptr = ptr;
              card->membase = membase;
              card->type    = cardtype;
              return card;
            }
          iounmap (ptr);
        }
    }
  return NULL;
}

/*
 * Delete the card specific structure.
 */
static void free_interface (EnLightCard *card)
{
  if (card)
    {
      iounmap (card->cardptr);
      kfree (card);
    }
}




/*
 * This method is called by the dmxdev module
 * if slots have been changed by the userspace
 * for e.g. by a write to /dev/dmx.
 */
static int  write_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  if (u && buff && size > 0 && offs+size < 512)
    {
      EnLightCard *card = (EnLightCard *)u->user_data;
      if (card)
        {
          memcpy_toio ((card->cardptr)+offs, (void *)buff, size);
          u->signal_changed (u, offs, size);
          return size;
        }
    }
  return -EINVAL;
}




/*
 * read slots from the output universe.
 * This function can be used for both, the input and the output universe.
 */
static int read_universe (DMXUniverse *u, off_t start, DMXSlotType *buff, size_t size)
{
  if (u && buff && size > 0 && start+size < 512)
    {
      EnLightCard *card = (EnLightCard *)u->user_data;
      if (card)
        {
          memcpy_fromio ((void *)buff, (card->cardptr)+start, size);
          return size;
        }
    }
  return -EINVAL;
}



int  dummy_data_available (DMXUniverse *u, uint start, uint size)
{
  return 1;
}




/*
 *---------------[ setter / getter functions ]------------------
 */


static int membase_get_long (DMXProperty *p, long *val)
{
  if (p && val && p->type == (DMXPTYPE_LONG|DMXPTYPE_USER))
    {
      EnLightCard *card = p->data ? (EnLightCard *)((DMXUniverse *)(p->data))->user_data : NULL;

      if (card)
        {
          *val = card->membase;
          return 0;
        }
    }
  return -1;
}

static int membase_set_long (DMXProperty *p, long val)
{
  if (p && val && p->type == (DMXPTYPE_LONG|DMXPTYPE_USER))
    {
      /* read only */
      return 0;
    }
  return -1;
}


static int slots_get_long (DMXProperty *p, long *val)
{
  if (p && val && p->type == (DMXPTYPE_LONG|DMXPTYPE_USER))
    {
      EnLightCard *card = p->data ? (EnLightCard *)((DMXUniverse *)(p->data))->user_data : NULL;

      if (card)
        {
          *val = get_channels (card);
          printk (KERN_INFO MODULENAME ":slots_get_long: actualy got %ld from card\n", *val);
          return 0;
        }
    }
  return -1;
}
static int slots_set_long (DMXProperty *p, long val)
{
  if (p && val && p->type == (DMXPTYPE_LONG|DMXPTYPE_USER))
    {
      EnLightCard *card = p->data ? (EnLightCard *)((DMXUniverse *)(p->data))->user_data : NULL;

      if (card)
        {
          set_channels (card, val);
          printk (KERN_INFO MODULENAME ":slots_set_long: actualy setting slots to %ld => %ld\n", val, (long)get_channels (card));
          return 0;
        }
    }
  return -1;
}


static int frames_get_long (DMXProperty *p, long *val)
{
  if (p && val && p->type == (DMXPTYPE_LONG|DMXPTYPE_USER))
    {
      EnLightCard *card = p->data ? (EnLightCard *)((DMXUniverse *)(p->data))->user_data : NULL;

      if (card)
        {
          *val = get_mailloop_counter(card);
          return 0;
        }
    }
  return -1;
}
static int frames_set_long (DMXProperty *p, long val)
{
  /* read only */
  return 0;
}


/*
 * returns the id-string
 *
 */
int unique_id_get_string (DMXProperty *prop, char *str, size_t size)
{
  int membase = (int)prop->data;
  if (membase)
    {
      sprintf(str, "enlight/enlight/out/0/0x%04X", membase);
      return 0;
    }
  return -1;
}

int unique_id_set_string (DMXProperty *prop, char *str)
{
  /* read-only */
  return 0;
}


/*
 *---------------[ Universe creation / deletion ]------------------
 */


/*
 * Delete an EnLight card universe.
 */
static int EnLight_delete_universe (DMXUniverse *u)
{
  if (u)
    {
      if (u->user_data)
        free_interface ((EnLightCard *)u->user_data);
    }
  return 0;
}


/*
 * Create an EnLight card Universe.
 */
static int EnLight_create_universe (DMXUniverse *u, DMXPropList *pl)
{
  long membase = 0L;


  if (u && u->interface && !u->interface->universes)
    {
      EnLightCard *card = NULL;

      DMXPropList *pl = u->props;
      if (pl && pl->find)
        {
          DMXProperty *p = pl->find(pl, "membase");
          if (p)
            {
              p->get_long(p, &membase);
              dmxprop_user_long (p, membase_get_long, membase_set_long, (void *)u);
            }

          if ((p=pl->find(pl, "slots"))==NULL)
            pl->add(pl, p=dmxprop_create_long ("slots", 512L));
          if (p)
	    dmxprop_user_long (p, slots_get_long, slots_set_long, (void *)u);

          if ((p=pl->find(pl, "frames"))==NULL)
            pl->add(pl, p=dmxprop_create_long ("frames", 512L));
          if (p)
	    dmxprop_user_long (p, frames_get_long, frames_set_long, (void *)u);

#if 0
	  if ((p=pl->find(pl, "unique_id"))==NULL)
	    pl->add(pl, p=dmxprop_create_string("unique_id", (void *)u));
	  if (p)
	    dmxprop_user_string(p, unique_id_get_string, unique_id_set_string, (void *)membase);
#endif
          if (membase>=0xA000 && membase <= 0xfd00)
            {
              card = check_create_interface (membase);
              if (card)
		{
                  u->user_delete = EnLight_delete_universe;

                  if (u->interface && u->interface->universes)
                    return -1;

                  u->write_slots = write_universe;
                  u->read_slots  = read_universe;

                  u->user_data = (void *)card;
                  strcpy (u->connector, "one");
                  u->conn_id = 0;


		  return 0;
		}
            }
        }
    }
  return -1;
}


MODULE_AUTHOR("Michael Stickel <michael@cubic.org> http://llg.cubic.org");
MODULE_DESCRIPTION("This is a driver for the EnLight DMX-interface card (http://www.enlight.de) from ICCC (http://www.iccc.de) version " DMXVERSION);
MODULE_LICENSE("GPL");

/* TODO: use array directly with module_param_array() */
int membase[8];
static int mb0=-1, mb1=-1, mb2=-1, mb3=-1, mb4=-1, mb5=-1, mb6=-1, mb7=-1;
module_param(mb0, int, S_IRUGO);
module_param(mb1, int, S_IRUGO);
module_param(mb2, int, S_IRUGO);
module_param(mb3, int, S_IRUGO);
module_param(mb4, int, S_IRUGO);
module_param(mb5, int, S_IRUGO);
module_param(mb6, int, S_IRUGO);
module_param(mb7, int, S_IRUGO);


static DMXFamily *el_fam = NULL;


static int __init EnLight_init(void)
{
  DMXInterface *i=0;

  membase[0] = mb0;
  membase[1] = mb1;
  membase[2] = mb2;
  membase[3] = mb3;
  membase[4] = mb4;
  membase[5] = mb5;
  membase[6] = mb6;
  membase[7] = mb7;

  if (el_fam)
    return -EBUSY;

  el_fam = dmx_create_family ("ISA");
  if (!el_fam)
    {
      printk (KERN_INFO MODULENAME "unable to register family EnLight\n");
      return -EBUSY;
    }
  else
    {
      DMXDriver *el_drv = el_fam->create_driver (el_fam, "EnLight", EnLight_create_universe, NULL);

      if (membase[0]==-1)
        {
          unsigned long addr = 0L;

          printk (KERN_INFO MODULENAME ":Autoprobing for EnLight interfaces\n");
          for (addr=0xC800; addr<0xEC00; addr+=0x100)
            {
	      if (check_interface_type (addr) > 0)
		{
		  i = el_drv->create_interface (el_drv, NULL);
		  printk (KERN_INFO MODULENAME ":found EnLight card at segment 0x%04lX\n", addr);
		  if (i)
		    i->create_universe (i, 0, dmxproplist_vacreate ("membase=%l", (long)addr));
                }
#if 0
	      else
		printk(KERN_INFO MODULENAME ": no interface found at segment 0x%04lX", addr);
#endif
	    }
        }
      else
        {
          int  j;
          for (j=0; j<8; j++)
	    if (membase[j] != -1)
	      {
		long addr = membase[j];
		if (check_interface_type (addr) > 0)
		  {
		    i = el_drv->create_interface (el_drv, NULL);
		    printk (KERN_INFO MODULENAME ":found EnLight card at segment 0x%04lX\n", addr);
		    if (i)
		      i->create_universe (i, 0, dmxproplist_vacreate ("membase=%l", addr));
		  }
		else
		  printk (KERN_INFO MODULENAME ":no EnLight interface found at segment 0x%04lX\n", addr);
	      }
        }

    }

  if(i==0)
    {
      if (el_fam)
	el_fam->delete (el_fam, 0), el_fam=0;
      printk(KERN_ERR MODULENAME ": no EnLight found\n");
      return -1;
    }

  return 0;
}


static void __exit EnLight_exit(void)
{
  if (el_fam)
    el_fam->delete (el_fam, 0), el_fam=0;
}

module_init(EnLight_init);
module_exit(EnLight_exit);
