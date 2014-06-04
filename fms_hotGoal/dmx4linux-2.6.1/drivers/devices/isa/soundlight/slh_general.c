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

/*------------------[ card independend functions ]---------------------------*/


/*
 * This is a candidate for a global dmxdev function.
 */
int count_universes (DMXUniverse *u, int kind)
{
  int cnt=0;
  while (u)
    {
      if (u->kind==kind) cnt++;
      u = u->next;
    }
  return cnt;
}



/*
 * name: set_slot_count
 * func: set the number of slots(channels) to transmit.
 */
int slh_set_slot_count (SLHCard *card, int slots)
{
  if (card)
    {
      if (slots > 512) slots = 512; /* maximum of 512 slots */
      if (slots < 24)  slots = 24;  /* and minimum of 24 slots */

      /* see the soundlight manual for getting more */
      /* information on encoding of slot-count      */
      card->write (card, SL1512A_SLOTS_LOW,  slots&0xff);
      card->write (card, SL1512A_SLOTS_HIGH, ((slots-1)>>8)&0xff);
      return 0;
    }
  return -1;
}


/*
 * name: set_startbyte
 * func: sets the startbyte (first byte transmitted after break
 */
int  slh_set_startbyte (SLHCard *card, unsigned char startbyte)
{
  if (card)
    {
      card->write (card, SL1512A_STARTBYTE, startbyte);
      return 0;
    }
  return -1;
}


void slh_card_init (SLHCard *card)
{
  slh_init_cardaccess (card);

  card->set_startbyte = slh_set_startbyte;
  card->set_slots     = slh_set_slot_count;

  card->updates_per_second = 32;
}


/*-------[ This is the timer based part ]--------*/
static void slh1512_timer (unsigned long data)
{
  unsigned long newtime  = jiffies;
  if (data)
    {
      SLHCard *card = (SLHCard *)data;
       if (HZ/card->updates_per_second > 0)
         newtime += HZ/card->updates_per_second;
       else
         newtime++;

       if(card->restart_transfer)
	 card->restart_transfer (card);

      mod_timer (&card->timer, newtime);
    }
}

int slh1512_starttimer(SLHCard *card)
{
  if (!card) return -1;
  init_timer (&card->timer);
  card->timer.function = slh1512_timer;
  card->timer.data     = (unsigned long)card;
  card->timer.expires  = jiffies + HZ/2;
  add_timer (&card->timer);
  printk("slh1512-timer started\n");
  return 0;
}

void slh1512_stoptimer(SLHCard *card)
{
  del_timer(&card->timer);
}


/*------[ data access stuff ]--------*/


/*
 * This method is called by the dmxdev module
 * if slots have been changed by the userspace
 * for e.g. by a write to /dev/dmx.
 */
int  slh1512_write_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  /* if it is an output universe, than data has changed if a user writes new data */
  if (u && !u->kind && u->user_data)  /* output universe */
    {
      SLHUniverse *slhu = (SLHUniverse *)u->user_data;

      memcpy (&slhu->buffer[offs], buff, size);

      /*
       * The data must be updated to the card, this must be done by the timer-function.
       * Here we will give it the range that has to be updated
       */
      if (slhu->upd_start<0 || offs < slhu->upd_start)
        slhu->upd_start = offs;
      if (slhu->upd_end<0 || size+offs > slhu->upd_end)
        slhu->upd_end = size+offs;

      slhu->data_avail = 1;
      u->signal_changed (u, offs, size);
      return size;
    }
  return -1;
}

/*
 * read slots from the output universe.
 * This function can be used for both, the input and the output universe.
 */
int slh1512_read_universe (DMXUniverse *u, off_t start, DMXSlotType *buff, size_t size)
{
  if (u && u->user_data && start >= 0 && start < 512)
    {
      SLHUniverse *slhu = (SLHUniverse *)u->user_data;

      if (start+size>512)
        size = 512-start;

      memcpy (buff, &slhu->buffer[start], size);
      slhu->data_avail = 0;

      return size;
    }
  return -EINVAL;
}

/*
 * returns < 0 for error, > 0 if new data is available and 0
 * if no new data is available for that universe.
 */
int  slh1512_data_available (DMXUniverse *u, uint start, uint size)
{
  if (u && u->user_data)
    {
      SLHUniverse *slhu = (SLHUniverse *)u->user_data;
      return slhu->data_avail;
    }
  return 0;
}

