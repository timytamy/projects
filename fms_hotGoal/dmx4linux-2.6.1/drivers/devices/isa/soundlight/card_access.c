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


int slh_write (SLHCard *card, int addr, unsigned char data)
{
  if (card)
    {
      SET_ADDR(card,addr);
      SET_VALUE(card,data);
      return 1;
    }
  return -1;
}


int slh_read (SLHCard *card, int addr)
{
  if (card)
    {
      SET_ADDR(card,addr);
      return  GET_VALUE(card);
    }
  return -1;
}


int slh_write_buffer (SLHCard *card, int addr, unsigned char *buff, size_t size)
{
  if (card)
    {
      int i;
      for (i=0; i<size; i++)
        {
          SET_ADDR(card,addr+i);
          SET_VALUE(card,buff[i]);
        }
      return size;
    }
  return -1;
}

int slh_read_buffer (SLHCard *card, int addr, unsigned char *buff, size_t size)
{
  if (card)
    {
      int i;
      for (i=0; i<size; i++)
        {
          SET_ADDR(card,addr+i);
          buff[i] = GET_VALUE(card);
        }
      return size;
    }
  return -1;
}


/*
 * name: write_os
 * func: writes the operating system given by <os> to the
 *       dmx-card at the io-base io
 *
 * cards: A,B,B-LC,C
 */
void slh_write_os (SLHCard *card, DMXBinary *os )
{
  if (card && os && os->size>0 && os->bin)
    {
      int i;
      
      printk (KERN_INFO "writing %d bytes DMX-OS %s to card at 0x%3X\n", os->size, os->name, card->iobase);
      for (i=0; i < os->size; i++)
	card->write (card, CARDRAM_OS+i, os->bin[i]);
    }
  else
    printk (KERN_INFO "soundlight:write_os(int io=0x%03X, DMXBinary *os=%p) illegal parameter\n", card->iobase, os);
}


/*
 * name: cardram_zero
 * func: clears <size> bytes begining at <from> in the
 *       ram of the dmx-card with base <io>
 *
 * cards: A,B,B-LC,C
 */
void slh_fill (SLHCard *card, int base, int size, unsigned char value)
{
  int i;

  if (card && base>=0 && size>0)
    for (i=0; i<size; i++)
      card->write (card, base+i, value);
}

void slh_zero (SLHCard *card, int base, int size)
{
  slh_fill (card, base, size, 0);
}


void slh_init_cardaccess (SLHCard *card)
{
  card->write        = slh_write;
  card->read         = slh_read;
  card->write_buffer = slh_write_buffer;
  card->read_buffer  = slh_read_buffer;
/*  card->fill         = slh_fill; */
  card->read_buffer  = slh_read_buffer;
  card->write_os     = slh_write_os;
}

