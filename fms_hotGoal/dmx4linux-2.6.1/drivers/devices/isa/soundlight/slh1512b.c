/*
 * slh_utils.c
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
 * request_region (iobase, 4, "soundlight");
 * release_region (iobase, 4);
 */


#include "slh.h"
#include "slhdmx16.h"


int  slh1512a_read_isr         (SLHCard *card);
void slh1512a_enable_card      (SLHCard *card);
void slh1512a_disable_card     (SLHCard *card);
int  slh1512a_start            (SLHCard *card);

int  slh1512a_set_breaksize_us         (SLHCard *card, int usBreak);
int  slh1512a_set_delay_between_frames (SLHCard *card, int delay_uS);
int  slh1512a_set_interdigit_time      (SLHCard *card, int idt);


static int slh1512b_card_restart (SLHCard *card);

/* TODO: merge all that redundant stuff into slh_common.c */
static void slh1512b_restart_transfer (SLHCard *card)
{
  SLHUniverse *slhu = card->universe[0];
  if (slhu)
    {
      card->disable(card);

      if (card->read (card, 0x3fa) == 'O')
	{
	  unsigned char sign = card->read (card, 0x3fa);
	  unsigned char code = card->read (card, 0x3fb);
	  if (code != 'K')
	    {
	      printk("card started but did not end [%c%c]\n", sign, code);
	      if (card->updates_per_second > 10)
		{
		  card->updates_per_second--;
		  printk("reduced update rate to %dHz\n", card->updates_per_second);
		}
	      else
		printk("warning: update rate has is at %dHz\n", card->updates_per_second);
	    }
	}
      else
	printk("card failed to start\n");
      card->write (card, 0x3fa, ' ');
      card->write (card, 0x3fb, ' ');

      /* This has to be done in a critical section */
      if (slhu->upd_start>=0 && slhu->upd_end>=0)
        {
          size_t start = slhu->upd_start;
          size_t end   = slhu->upd_end;
          slhu->upd_start = slhu->upd_end = -1;
          if (start<512 && start<=end)
            card->write_buffer (card, 0x400+start, &slhu->buffer[start], end-start+1);
        }
      /* end of the critical section */

#if 0
      card->set_break (card, 88);
      card->set_slots (card, 512);

      card->write (card, 0x3f1, 0);
      card->write (card, 0x3f2, 0);
      card->write (card, 0x3f5, 0); /* send once */
      card->write (card, 0x3f6, 0); /* inter digit time */
#endif
      card->write (card, 0x3f8, 0); /* mode is output */
      card->enable(card);
    }
  else
    printk(KERN_INFO "slh1512b-restart: slhu=NULL\n");
}


/*---------[ 1512B dependend functions ]---------------*/

int slh1512b_card_init (SLHCard *card)
{
  if (card)
    {
      printk(KERN_INFO "slh1512b_card_init()\n");
      slh_card_init (card);

      card->os = &firmware_slhdmx16;

      card->read_isr = slh1512a_read_isr;
      card->disable  = slh1512a_disable_card;
      card->enable   = slh1512a_enable_card;

      card->set_break = slh1512a_set_breaksize_us;
      card->set_dbf   = slh1512a_set_delay_between_frames;
      card->set_idt   = slh1512a_set_interdigit_time;

      card->updates_per_second = 40;

      slh1512b_card_restart (card);
    }
  return -1;
}


static int slh1512b_card_restart (SLHCard *card)
{
  if (card)
    {
      int i;

      slh1512a_disable_card(card);
      /*      slh1512a_enable_card (card); */
      if (card->os)
	card->write_buffer (card, 0, card->os->bin, card->os->size);

      for (i=0; i<512; i++)
        card->write (card, CARDRAM_DMX+i, 0);

      card->set_break (card, 88);
      card->write (card, SL1512A_STARTBYTE, 0);
      card->write (card, SL1512A_DELAYBETWEENFRAMES, 0);

      card->set_slots (card, 60);

      card->write (card, SL1512A_CONFIG, 0);
      card->write (card, SL1512A_IDT,  0); /* inter digit time */
      card->write (card, SL1512_MODE,  MODE_WRITE);

      card->write (card, 0x3fa, ' ');
      card->write (card, 0x3fb, ' ');

      slh1512a_start (card);
      printk(KERN_INFO "card restarted\n");
    }
  return 0;
}

static int slh1512b_delete_universe (DMXUniverse *u)
{
  if (u && u->interface && u->interface->user_data && u->user_data)
    {
      SLHUniverse *slhu = (SLHUniverse *)u->user_data;
      SLHCard     *card = (SLHCard *)u->interface->user_data;

      /* csec-begin */

      card->universe[0] = NULL;
      DMX_FREE(slhu);

      /* csec-end */

      return 0;
    }
  return -1;
}


int slh1512b_create_universe (DMXUniverse *u, DMXPropList *p)
{
  if (u && u->interface && u->interface->user_data && !u->interface->universes && !u->kind)
    {
      SLHCard *card = u->interface->user_data;
      SLHUniverse *slhu = DMX_ALLOC(SLHUniverse);
      if (slhu)
         {
          int i;
          for (i=0; i<512; i++)
            slhu->buffer[i]=0;

          strcpy (u->connector, "first");
          u->conn_id = 0;

          slhu->data_avail = 0;
          slhu->upd_start = -1;
          slhu->upd_end = -1;
          slhu->direction = 0;
          slhu->univ = u;
          slhu->universe = 0;
          slhu->card = card;

          u->user_data = slhu;

          card->universe[0] = slhu;

          u->write_slots    = slh1512_write_universe;
          u->read_slots     = slh1512_read_universe;
          u->data_available = slh1512_data_available;

          u->user_delete = slh1512b_delete_universe;

          return 0;
        }
    }
  return -1;
}




static int slh1512b_delete_interface (DMXInterface *i)
{
  if (i && i->user_data)
    {
      SLHCard *card = (SLHCard *)i->user_data;
      i->user_data = NULL;
      if (!card->irqmask)
        slh1512_stoptimer(card);
      DMX_FREE(card);
      return 0;
    }
  return -1;
}


int slh1512b_create_interface (DMXInterface *i, DMXPropList *pl)
{
  long iobase = 0L;
  
  ONDBG(printk(KERN_INFO "slh1512b_create_interface (DMXInterface *=%p, DMXPropList *=%p)\n", i, pl));

  if (pl && pl->find)
    {
      DMXProperty *p = pl->find(pl, "iobase");
      if (p)
        p->get_long (p, &iobase);
    }

  if (i && iobase)
    {
      SLHCard *card = DMX_ALLOC(SLHCard);
      if (card)
        {
          int  j;

          i->user_data = (void *)card;

          card->iobase = iobase;
          card->type   = CARDTYPE_1512B;

          slh1512b_card_init (card);

          get_card_capabilities (card);

          printk (KERN_INFO "  =>     type = %d\n", card->type);
          printk (KERN_INFO "  =>  irqmask =");
          for (j=0; j<32; j++)
            {
              if (card->irqmask & (1<<j))
                printk (" %d", j);
            }
          printk ("\n");
          printk (KERN_INFO "  =>  has_isr = %d\n",   card->has_isr);
          printk (KERN_INFO "  =>   iobase = 0x%x\n", card->iobase);
          printk (KERN_INFO "  =>       os = %p\n",   card->os);

          slh1512a_disable_card (card);    /* reset card */

          i->user_delete = slh1512b_delete_interface;

	  card->irq = -1;
	  card->irqmask=0L;
	  printk (KERN_INFO "slh1512b: use timer driven retransmit\n");
	  card->restart_transfer = slh1512b_restart_transfer;
          slh1512b_card_init (card);

	  slh1512_starttimer(card);
          return 0;
        }
    }
  return -1;
}

