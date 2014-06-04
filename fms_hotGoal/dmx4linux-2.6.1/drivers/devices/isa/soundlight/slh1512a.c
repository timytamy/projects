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
#include "slhdmx12a.h"   /* used for interupt check */


/*------------------[ 1512A dependend functions ]---------------------------*/

/*
 * name: read_isr
 * func: reads the isr of the card
 *       It is a patch to the SLH1512A, B, B/LC and the ELRAD Lightline interface.
 *       The SLH1512C and SLH1512D does not need that patch, because the interrupt
 *       status is read from the DPRAM.
 */
int slh1512a_read_isr (SLHCard *card)
{
  if (card)
    return (card->has_isr)?inb_p (card->iobase+1):0x23;
  return -1;
}


/*
 * name: set_interdigit_time
 * func: 
 */
int slh1512a_set_interdigit_time (SLHCard *card, int idt)
{
  int n = idt / 2;
  card->write (card, SL1512A_IDT, (n<0)?0:(n>255)?255:n);
  return 0;
}


/*
 * name: set_delay_between_frames
 * func: sets the delay that will be inserted between
 *       two dmx-frames (packets). Minimum is 40uS
 */
int slh1512a_set_delay_between_frames (SLHCard *card, int delay_uS)
{
  int n = (delay_uS - 40) / 2;
  card->write (card, SL1512A_DELAYBETWEENFRAMES, (n<0)?0:(n>255)?255:n);
  return 0;
}


/*
 * name: enable_card
 * func: starts processing of the dmx-card cpu
 */
void slh1512a_enable_card (SLHCard *card)
{
  if (card)
    inb_p (card->iobase + 3); /* starts the cpu (1512B-LC or B or A) */
}

/*
 * name: disable_card
 * func: stops the dmx-card cpu
 */
void slh1512a_disable_card (SLHCard *card)
{
  if (card)
    outb_p (0,card->iobase); /* stops the cpu (1512B-LC or B or A) */
}


/*
 * name: set_breaksize_us
 * func: sets the breaksize in microseconds
 */
int slh1512a_set_breaksize_us (SLHCard *card, int usBreak)
{
  int n = ((usBreak<88)?88:usBreak) / 2;
  card->write (card, SL1512A_BREAK, (n<0)?0:(n>255)?255:n);
  return 0;
}

int slh1512a_start (SLHCard *card)
{
  slh1512a_disable_card(card);
  slh1512a_enable_card(card);
  return 0;
}


int slh1512a_card_init (SLHCard *card)
{
  if (card)
    {
      DMXBinary *os = &firmware_slhdmx12a;
      int i;

      slh_card_init (card);

      card->read_isr = slh1512a_read_isr;
      card->disable  = slh1512a_disable_card;
      card->enable   = slh1512a_enable_card;

      card->set_break = slh1512a_set_breaksize_us;
      card->set_dbf   = slh1512a_set_delay_between_frames;
      card->set_idt   = slh1512a_set_interdigit_time;

      slh1512a_disable_card (card);

      card->write_buffer (card, 0, os->bin, os->size);

      for (i=0; i<512; i++)
        card->write (card, 0x400+i, 0);

      card->set_break (card, 88);
      card->write (card, SL1512A_STARTBYTE, 0);
      card->write (card, SL1512A_DELAYBETWEENFRAMES, 0);

      card->set_slots (card, 512);

      card->write (card, SL1512A_CONFIG, (card->irq==3 || card->irq==4)?card->irq:0); /* send once */

      card->write (card, SL1512A_IDT, 0); /* inter digit time */
      slh1512a_start (card);
    }
  return -1;
}


static void slh1512a_restart_transfer (SLHCard *card)
{
  /* The SLH1512A based cards have only one universe */
  SLHUniverse *slhu = card->universe[0];
  if (slhu)
    {
      card->disable(card);

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


      card->set_break (card, 88);
      card->set_slots (card, 512);

      card->write (card, 0x3f1, 0);
      card->write (card, 0x3f2, 0);

      card->write (card, 0x3f5, (card->irq==3 || card->irq==4)?card->irq:0); /* send once */

      card->write (card, 0x3f6, 0); /* inter digit time */
 
      card->enable(card);
    }
}



/*----[ This is the interrupt based part ]--------*/
/*
 * Interrupt function for the SLH1512B,SLH1512B/LC and a patched SLH1512A or Lightine card.
 */
static irqreturn_t slh1512a_interrupt (int irq, void *private)
{
  SLHCard *card = (SLHCard *)private;
  if (card)
    {
      int isr = card->read_isr(card);
      if (isr==0x23)
        slh1512a_restart_transfer (card);
    }
  return IRQ_HANDLED;
}

static int slh1512a_startinterrupt (SLHCard *card)
{
  if (card && !request_irq (card->irq, slh1512a_interrupt, SA_INTERRUPT, "dmx/slh1512a", card))
    {
      return 0;
    }
  return -1;
}

static int slh1512a_stopinterrupt  (SLHCard *card)
{
  if (card)
    {
      free_irq(card->irq, card);
      return 0;
    }
  return -1;
}






static int slh1512a_delete_universe (DMXUniverse *u)
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


int slh1512a_create_universe (DMXUniverse *u, DMXPropList *p)
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

          u->user_delete = slh1512a_delete_universe;

          return 0;
        }
    }
  return -1;
}



static int slh1512a_delete_interface (DMXInterface *i)
{
  if (i && i->user_data)
    {
      SLHCard *card = (SLHCard *)i->user_data;

      if (card->irqmask)
        slh1512a_stopinterrupt(card);
      else
        slh1512_stoptimer(card);

      i->user_data = NULL;
#if 0
      release_region (card->iobase, 4);
#endif
      DMX_FREE(card);
      return 0;
    }
  return -1;
}

int slh1512a_create_interface (DMXInterface *i, DMXPropList *pl)
{
  long iobase = 0L;

  ONDBG(printk(KERN_INFO "slh1512a_create_interface (DMXInterface *=%p, DMXPropList *=%p)\n", i, pl));

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
          card->type   = CARDTYPE_1512A;
#if 0
          if (request_region (card->iobase, 4, "slh1512a"))
            {
            }
#endif

          slh1512a_card_init (card);

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

          i->user_delete = slh1512a_delete_interface;

          if (card->irqmask && (card->irqmask&(3<<3)))
            {
              if (card->irqmask&(1<<3))
                card->irq = 3;
              else
                if (card->irqmask&(1<<4))
                  card->irq = 4;

              printk (KERN_INFO "slh1512a: use interrupt driven retransmit with%s shared interrupts\n", (card->has_isr)?"":"out");

              slh1512a_startinterrupt(card);
            }
          else
            {
              card->irq = -1;
              card->irqmask=0L;
              printk (KERN_INFO "slh1512a: use timer driven retransmit\n");
	      card->restart_transfer = slh1512a_restart_transfer;
              slh1512_starttimer(card);
            }
          slh1512a_card_init (card);

          return 0;
        }
    }
  return -1;
}


