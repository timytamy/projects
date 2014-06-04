/*
 * slh.h
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

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
#include <linux/config.h>
#endif

#if !defined(CONFIG_ISA)
#error Linux Kernel needs ISA Bus support for the Soundlight card family
#endif

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <dmx/dmxdev.h>

#include "dmx/dmxtools.h"

#define ONDBG(x...)   x
#define ONINFO(x...)  x


/* The Firmware for the card */
typedef struct
{
    char           *name;
    size_t          size;
    unsigned char  *bin;
} DMXBinary;


struct struct_SLHUniverse;
typedef struct struct_SLHUniverse SLHUniverse;

/* created for each card (not universe, because one card can have multible universes) */
typedef struct s_SLHCard SLHCard;
struct s_SLHCard
{
  char           *name;
  unsigned short  type;
  unsigned int    iobase;

  /*
   * Interrupt stuff
   */
  int             irq;      /* the irq that is currently used */
  char            has_isr;  /* does the card have an I. status register */
  unsigned long   irqmask;  /* what intterupt can the card raise?
			     * Bit0=IRQ0,Bit1=IRQ1,...
			     */
  int            updates_per_second; /* default is 32 */

  DMXBinary      *os;

  struct wait_queue  *wait_worker;

  struct
    {
      char                running;
      int                 pid;
      struct wait_queue  *queue;
    } thread;


  struct timer_list timer;


  SLHUniverse     *universe[4];

  /* must have some kind of semaphore, because more than one process can access the same card */

  /* low level methods */
  void (*enable)  (SLHCard *);
  void (*disable) (SLHCard *);
  int  (*write)  (SLHCard *, int addr, unsigned char value);
  int  (*read)   (SLHCard *, int addr);
  int  (*write_buffer) (SLHCard *, int addr, unsigned char *buffer, size_t len);
  int  (*read_buffer)  (SLHCard *, int addr, unsigned char *buffer, size_t len);
  void (*write_os)     (SLHCard *card, DMXBinary *os);

  /* high level methods */
  int  (*read_isr) (SLHCard *);

  int (*set_break) (SLHCard *card, int usBreak);
  int (*set_idt)   (SLHCard *card, int idt);
  int (*set_dbf)   (SLHCard *card, int delay_uS);
  int (*set_slots) (SLHCard *card, int slots);
  int (*set_startbyte) (SLHCard *card, unsigned char sb);

  void (*restart_transfer) (SLHCard *card);
};



struct struct_SLHUniverse
{
  SLHCard      *card;      /* the card */
  int           universe;  /* the universe of the card 0,1,... */
  DMXUniverse  *univ;
  char          direction; /* 0=out or 1=in */

  char     data_avail;
  long     upd_start;
  long     upd_end;
  unsigned char buffer[512]; /* temp buffer */
};



#define DEFAULT_IOBASE(id)  (((id)>=0&&(id)<4)?defaultio[(id)]:-1)
#define NUM_SLHUNIVERSES  16
#define NUM_SLHCARDS      4



#define CARDRAM_OS     (0x000) /*..0x3EF */
#define CARDRAM_PARAM  (0x3F0) /*..0x3FF */
#define CARDRAM_DMX    (0x400) /*..0x5FF */
#define CARDRAM_DMX2   (0x600) /*..0x7FF */
#define CARDRAM_DMXIN  (0x800) /*..0x9FF */
#define CARDRAM_DMXIN2 (0xC00) /*..0xDFF */




#define SL1512A_BREAK              (0x3F0)
#define SL1512A_STARTBYTE          (0x3F1)
#define SL1512A_DELAYBETWEENFRAMES (0x3F2)
#define SL1512A_SLOTS_LOW          (0x3F3)
#define SL1512A_SLOTS_HIGH         (0x3F4)
#define SL1512A_CONFIG             (0x3F5)
# define SL1512_LOOP               (0x80) /* loop forever */
# define SL1512A_SINGLE_STOP  (0x00) /* transmit once, then stop */
# define SL1512C_SINGLE_STOP  (0x40) /* transmit once, then stop */
# define SL1512C_SINGLE_REST  (0x60) /* restart, transmit once */
#define SL1512A_IDT                (0x3F6)
#define SL1512_MODE                (0x3F8)
# define MODE_WRITE           (0)
# define MODE_READ            (0xff)
#define SL1512A_ID                 (0x3F9)


#define CARDTYPE_1512A      (1) /* or B/LC or Lightline */
#define CARDTYPE_1512B      (2)
#define CARDTYPE_1512C      (3)

#define CARDTYPE2NAME(t)  (((t)==CARDTYPE_1512A)?"slh1512A,slh1512B/LC":((t)==CARDTYPE_1512B)?"slh1512B":((t)==CARDTYPE_1512C)?"slh1512C":"unknown")

#define PC1512C_ParameterValid(card,valid) set_breaksize_us(card,valid)

int soundlight_init_card (SLHCard *card);


/*
 * name: read_isr
 * func: reads the status of the isr, if it exists and returns its contents.
 *       it returns -1 if the isr does not exist.
 */
int read_isr (SLHCard *card);

#define SET_ADDR(card,addr)  outb_p((addr)&0xff, (card)->iobase),outb_p(((addr)>>8)&0xff, (card)->iobase+1)
#define SET_VALUE(card,data) outb_p((data), (card)->iobase+2)
#define GET_VALUE(card)      ((unsigned char)inb_p ((card)->iobase+2))





void slh_zero            (SLHCard *card, int base, int size);
void slh_init_cardaccess (SLHCard *card);
void slh_card_init       (SLHCard *card);
int get_card_capabilities (SLHCard *card);


int slh1512a_card_init (SLHCard *card);
int slh1512b_card_init (SLHCard *card);
int slh1512c_card_init (SLHCard *card);

int  slh1512a_read_isr         (SLHCard *card);
void slh1512a_enable_card      (SLHCard *card);
void slh1512a_disable_card     (SLHCard *card);
int  slh1512a_start            (SLHCard *card);
int  slh1512a_set_breaksize_us         (SLHCard *card, int usBreak);
int  slh1512a_set_delay_between_frames (SLHCard *card, int delay_uS);
int  slh1512a_set_interdigit_time      (SLHCard *card, int idt);


int  slh1512c_read_isr         (SLHCard *card);
void slh1512c_enable_card      (SLHCard *card);
void slh1512c_disable_card     (SLHCard *card);
int  slh1512c_start            (SLHCard *card);
int  slh1512c_set_breaksize_us         (SLHCard *card, int usBreak);
int  slh1512c_set_delay_between_frames (SLHCard *card, int delay_uS);
int  slh1512c_set_interdigit_time      (SLHCard *card, int idt);


int count_universes (DMXUniverse *u, int kind);

int slh1512a_create_universe (DMXUniverse *u, DMXPropList *p);
int slh1512b_create_universe (DMXUniverse *u, DMXPropList *p);
int slh1512c_create_universe (DMXUniverse *u, DMXPropList *p);

int slh1512a_create_interface (DMXInterface *i, DMXPropList *pl);
int slh1512b_create_interface (DMXInterface *i, DMXPropList *pl);
int slh1512c_create_interface (DMXInterface *i, DMXPropList *pl);

int  slh1512_starttimer(SLHCard *card);
void slh1512_stoptimer (SLHCard *card);

void slh_fill (SLHCard *card, int base, int size, unsigned char value);
void slh_zero (SLHCard *card, int base, int size);



int  slh1512_write_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size);
int  slh1512_read_universe  (DMXUniverse *u, off_t start, DMXSlotType *buff, size_t size);
int  slh1512_data_available (DMXUniverse *u, uint start, uint size);
