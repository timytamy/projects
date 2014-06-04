/*
 * autoprobe.c
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
/*
 * Check if there is a card present that:
 *  1. acts like the soundlight cards.
 *  2. What type they are
 *  3. If they use internal or external program memory (MODE on the B/LC and INT/AXT on the C)
 *  4. If it is an A B or B/LC card:
 *  4.1. If it can raise interrupts and when, what interrupts.
 *  4.2. If it has the ISR-Hardware-Patch (alowing shared interrupts)
 *  4.3. It is an A variant if P1.2 and TXD/P3.1 are not connected.
 *  4.4. It is a B/LC variant if it runs at 12 MHz speed, that must be timerd out.
 *  4.5. If the card runs at 16MHz speed it must be a B variant.
 *  4.6. Or if P3.2 Is high if RXD is driven from outside and can not be driven from the 8031.
 *       If RXD is low (or drivern to low) and P3.2 produses an interrupt after 64uS it is a
 *       B variant (with receiver).
 *  5.   It must be a C variant if the cpu goes on running even if we access the RAM (DPRAM).
 *
 * That all would lead to N classes of different drivers:
 * 1. A driver for the A, B or B/LC without interrupts that uses
 *    a timer for repeated transmisions.
 * 2. A driver for A, B or B/LC that uses non sharable interrupts.
 * 3. A driver for a patched A, B or B/LC card that can use shared interrupts because of the ISR.
 * 4. A driver for the C card that supports two universes out and two universes in.
 * 5. A driver for the C card as above but with use of shared interrupts.
 */

#include "slh.h"

#include "slhcheckirq.h"
#include "slhcheckisr.h"  /* writes the location 0x3fc from RAM to the ISR */
#include "slhcheck.h"


/* slh_probe_interrupt
 * This is the interrupt routine that is used during probe for interrupts
 * and detemines what interrupts can be raised by the card.
 */
static unsigned long  all_irqs = 0L;
static unsigned int   irq_counter[20];
static unsigned int   irq_isr_counter[20];
static irqreturn_t slh_probe_interrupt (int irq, void *private)
{
  SLHCard *card = (SLHCard *)private;
  if (irq>=0 && irq<20 && card)
    {
      int i;
      for (i=0; i<4; i++)
        if (slh1512a_read_isr(card) == 0x23)
          {
            /* there has an interrupt for the card at the addr */
            irq_isr_counter[irq]++;
          }
        irq_counter[irq]++;
    }
  all_irqs++;

  return IRQ_HANDLED;
}



/*
 * name: check_for_card
 * func: tries to determine if a soundlight dmx card 1512B-LC,
 *       1512B or 1512C can be found at io-address <base>.
 * returns 0 if no card has been found and nozero if a card
 * could be there. Use soundlight_init_card to initialize it
 * and to get the type of card.
 */
/*
 * before calling this function we must have called autoprobe_card
 * to set determine the type or must be sure that we know the type
 * and card->type is set to it.
 */
int get_card_capabilities (SLHCard *card)
{
  int  i;
/*  int  ramsize = 0; */
  unsigned long irqmask = 0L;
  char has_isr  = 0;
  char card_b_sw3 = -1;
  int  firmware_code=0;
  int  trials[] = {3,4,5,7,9,-1};
  int  tried[]  = {0,0,0,0,0,-1};
  int  found_irq = -1;


  if (!card || (card->type != CARDTYPE_1512A && card->type != CARDTYPE_1512B && card->type != CARDTYPE_1512C))
    return -1;

  /*
   * check for the existance of the ISR-Hack
   */
  has_isr = 1;
  card->write_os (card, &firmware_slhcheckisr);

  for (i=0; i<=0xff && has_isr; i++)
  {
    card->write (card, 0x3fc, i);
    card->enable(card);
    udelay(1000);
    if (inb_p (card->iobase+1) != i)
      has_isr = 0;
  }
  card->has_isr = has_isr;

  ONDBG(if (has_isr) printk (KERN_INFO "card has ISR-Hack, so it supports interrupt sharing\n"));


  /*
   * initialize for interrupt testing
   */
  for (i=0; i<20; i++)
    {
      irq_counter[i] = 0;
      irq_isr_counter[i] = 0;
    }
  all_irqs = 0;


  /*
   * load the firmware that tries to raise some interrupts
   */
  card->disable(card);
  card->write_os (card, &firmware_slhcheckirq);

  ONINFO(printk (KERN_INFO "IRQ for check fail(-) OK(+):"));

  for (i=0; trials[i]!=-1 && i<sizeof(trials); i++)
    {
      tried[i] = request_irq (trials[i], slh_probe_interrupt, SA_INTERRUPT, "soundlight probe", card);
      ONINFO(printk (" %c%d", tried[i]?'-':'+', trials[i]));
    }
  ONINFO(printk ("\n"));

  /*
   * start raising interrupts
   */
  card->enable(card);

  /*
   * wait for one interrupt to be risen 16 times.
   */
  for (i=0; i<100; i++)
    {
      int  j;
      for (j=0; j<20 && found_irq==-1; j++)
        if (irq_counter[j] == 16)
          found_irq = j;
      udelay(1000);
    }

  card->disable(card);

  ONDBG(printk (KERN_INFO "IOBASE[0]=$%02X\n", inb_p (card->iobase)));

  firmware_code = card->read (card, 0x3ff);

  ONDBG(printk (KERN_INFO "firmware_code = 0x%02X\n", firmware_code));

  switch (firmware_code)
  {
    case 0xEE: printk (KERN_INFO "Interrupt Firmware loaded successfully. Firmware upload possible\n");
               card_b_sw3 = (card->read (card, 0x3fd)&0x20)?1:0;
               printk (KERN_INFO "SW3 is in Position %s\n", card_b_sw3?"OFF":"ON");
               printk (KERN_INFO "P1=0x%02X, P3=0x%02X\n", card->read (card, 0x03fd), card->read (card, 0x03fe));
               break;

    case 0xAA:
               printk (KERN_INFO "Using Internal Firmware. Firmware upload and interrupt autoprobe not possible\n");
               printk (KERN_INFO "Set config switch 4 (Mode) to upper position to use custom firmware upload\n");
               break;

    default:   printk (KERN_INFO "Unexpected firmware ID 0x%02X\n", firmware_code);
               break;
  }

  /*
   *  Check for interrupts that has been triggered.
   */
  ONDBG(printk (KERN_INFO "%ld interrupts counted\n", all_irqs));

  for (i=0; trials[i]!=-1 && i<sizeof(trials); i++)
    {
      if (!tried[i])
        {
	  /*
	   * check what interrupts the card can raise and then decide what to use.
	   * The interrupt mask will also be remebered, so the driver can change
	   * the interrupt on the fly.
	   */
	  if (irq_counter[trials[i]] > 2  &&  trials[i] < 32)
	    {
	      irqmask |= 1<<trials[i];
	    }

	  ONDBG(if (irq_counter[trials[i]] > 0 || irq_isr_counter[trials[i]] > 0) \
                  printk (KERN_INFO "IRQ%d: raised %d times - ISR raised %d times\n", trials[i], \
                          irq_counter[trials[i]], irq_isr_counter[trials[i]]));

          free_irq(trials[i], card);
        }
    }
  card->irqmask = irqmask;  /* remeber the interrupts the card is able to raise. */

  ONDBG(for (i=0; i<20; i++) \
          if (irq_isr_counter[i]>0 || irq_counter[i]>0) \
            printk (KERN_INFO "IRQ=%d  ISR-trigger=%d  IRQtrigger=%d\n", i, irq_isr_counter[i], irq_counter[i]));

  return 1;
}



/* slh_autoprobe_card
 *
 * Autoprobes for a card at a specific io-address that has to
 * be written into card->iobase by the caller of the function.
 * The other fields can be uninitialized.
 */
int slh_autoprobe_card (SLHCard *card)
{
  ONDBG(printk (KERN_INFO "-------[ slh_autoprobe_card (%p) at io=0x%03X ]-------\n", card, card?card->iobase:0));

  if (card)
    {
      int type = 0;

      slh_card_init(card);

      card->write (card, 0x3ff, 0xAA);
      if (card->read (card, 0x3ff) == 0xAA)
        {
          ONDBG(printk (KERN_INFO "possibly found a Soundlight DMX-interface\n"));

          slh1512c_disable_card (card);    /* reset card */

          card->write (card, 0x3f9, 0xff); /* default = Error */

          slh1512c_card_init (card);

          slh1512c_disable_card (card);    /* reset card */
          card->write (card, 0x3f0, 0);
#if 1
          card->write_buffer(card, 0, firmware_slhcheck.bin, firmware_slhcheck.size);
#else
          card->write_buffer(card, 0, firmware_slhdmx17.bin, firmware_slhdmx17.size);
#endif
          slh1512c_disable_card (card);    /* reset card */
          slh1512c_enable_card (card);    /* reset card */


          udelay(4000);


          slh1512c_disable_card (card);    /* reset card */
          type = card->read (card, 0x3f9);

          if (type == 0xff)
            {
              printk (KERN_INFO "card did not start. check for A, B/LC or B\n");
              slh1512a_card_init (card);
              udelay(4000);
              slh1512a_disable_card (card);    /* reset card */
              type = card->read (card, 0x3f9);
            }

          ONDBG(printk (KERN_INFO "card reports type = 0x%02X\n", type));

#if 0
          switch(type)
          {
          case 0x80:
              card->write (card, 0x3f9, 0);
              slh1512c_card_init (card);
              slh1512c_start (card);
              udelay(4000);
              type = card->read (card, 0x3f9);
              ONDBG(printk (KERN_INFO "type is 0x80 retry. reported type is 0x%02X\n", type));
              break;

          case 0:
              card->write (card, 0x3f9, 0);
              slh1512b_card_init (card);
              slh1512a_start (card);
              udelay(4000);
              type = card->read (card, 0x3f9);
              ONDBG(printk (KERN_INFO "type is 0x00 retry. reported type is 0x%02X\n", type));
              break;

          case 0xff:
              printk (KERN_INFO "card seems to be no functioning Soundlight card\n");
              break;
          }
#endif

          switch (type)
          {
          case 0x80: /* SLH1512A, B/LC */
                  card->type = CARDTYPE_1512A;
                  break;
                                       /* have to check wether pin 3.1 and 1.2 are connected or not */
          case 1: /* SLH1512B */
                  card->type = CARDTYPE_1512B;
                  break;
          case 4:
          case 5: /* SLH1512C */
                  card->type = CARDTYPE_1512C;
                  break;

          default:
                  return -1;
          }

          printk (KERN_INFO "slh_autoprobe_card: type %s\n", CARDTYPE2NAME(card->type));

          return card->type;
        }
    }
  return -1;
}


/*
 * slh_autoprobe
 *
 * This function probes for Soundlight cards and sets the iobase and the type member
 * in the cards structures acording to what it found out about the cards. In returns
 * the number of cards found.
 */
int slh_autoprobe (SLHCard *cards, int maxcards)
{
  if (cards && maxcards>0)
    {
      int cnt=0;
      int i;

      for (i=0; cnt<maxcards && i<4; i++)
        {
          SLHCard *card = &cards[cnt];
          card->iobase = 0x100 + i*0x20;
          if (slh_autoprobe_card (card) >= 0)
            cnt++;
        }
      return cnt;
    }
  return -1;
}


/*
 * A simple test for the autoprobe funnction, but not nessesary for the driver.
 */
int slh1512_test (void)
{
  SLHCard cards[4];

  int cnt = slh_autoprobe (cards, 4);
  if (cnt > 0)
    {
      int i;

      ONINFO(printk (KERN_INFO "slh1512c_test: autoprobing found %d cards\n", cnt));
      for (i=0; i<cnt; i++)
        {
          int j;
          SLHCard *card = &cards[i];

          switch (card->type)
          {
          case CARDTYPE_1512C:
              slh1512c_card_init (card);
              for (j=0; j<10; j++)
                card->write (card, 0x400+j, 0x80);
              card->write (card, 0x3f0, 1);
              card->write (card, 0x3f1, 0);
              card->write (card, 0x3f3, 60);
              card->write (card, 0x3f4, 0);
              card->write (card, 0x3f5, 0x80);
              slh1512c_start (card);
              ONINFO(printk (KERN_INFO "slh1512c_test: SLH1512C card at 0x%03X\n", card->iobase));
              break;

          case CARDTYPE_1512B:
              slh1512b_card_init (card);
              for (j=0; j<10; j++)
                card->write (card, 0x400+j, 0x80);
              card->write (card, 0x3f0, 59);
              card->write (card, 0x3f1, 0);
              card->write (card, 0x3f2, 0);
              card->write (card, 0x3f3, 60);
              card->write (card, 0x3f4, 0);
              card->write (card, 0x3f5, 0x80);
              card->write (card, 0x3f6, 0);
              card->write (card, 0x3f8, 0);
              slh1512a_start (card);
              ONINFO(printk (KERN_INFO "slh1512c_test: cardtype SLH1512B  card at 0x%03X\n", card->iobase));
              break;

          case CARDTYPE_1512A:
              slh1512a_card_init (card);
              for (j=0; j<10; j++)
                card->write (card, 0x400+j, 0x80);
              card->write (card, 0x3f0, 44);
              card->write (card, 0x3f1, 0);
              card->write (card, 0x3f2, 0);
              card->write (card, 0x3f3, 60);
              card->write (card, 0x3f4, 0);
              card->write (card, 0x3f5, 0x80);
              card->write (card, 0x3f6, 0);
              slh1512a_start (card);
              ONINFO(printk (KERN_INFO "slh1512c_test: cardtype SLH1512A or SLH1512B/LC card at 0x%03X\n", card->iobase));
              break;

          default:
              printk (KERN_INFO "unknown card found (type=%d)\n", card->type);
              break;
          }
        }
    }
  else
    {
      ONINFO(printk (KERN_INFO "slh1512c_test: no cards found\n"));
    }
  return 0;
}

