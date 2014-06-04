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
#include "slhdmx17.h"


/*---------[ 1512C dependend functions ]---------------*/

/*
 * name: read_isr
 * func: reads the isr of the card
 *       It is a patch to the SLH1512A, B, B/LC and the ELRAD Lightline interface.
 *       The SLH1512C and SLH1512D does not need that patch, because the interrupt
 *       status is read from the DPRAM.
 */
int slh1512c_read_isr (SLHCard *card)
{
  if (card)
    return card->read (card, 0x3f7);
  return -1;
}


/*
 * name: set_interdigit_time
 * func: 
 */
int slh1512c_set_interdigit_time (SLHCard *card, int idt)
{
  int n = (idt * 16) / 24;
  card->write (card, SL1512A_IDT, (n<0)?0:(n>255)?255:n);
  return 0;
}

/*
 * name: set_delay_between_frames
 * func: sets the delay that will be inserted between
 *       two dmx-frames (packets). Minimum is 40uS
 */
int slh1512c_set_delay_between_frames (SLHCard *card, int delay_uS)
{
  int n = (delay_uS - 30) / 2;
  card->write (card, 0x3F2, (n<0)?0:(n>255)?255:n);
  return 0;
}



/*
 * name: enable_card
 * func: starts processing of the dmx-card cpu
 */
void slh1512c_enable_card (SLHCard *card)
{
  if (card)
    outb_p (0, card->iobase + 3); /* starts the cpu (1512C manual) */
}

/*
 * name: disable_card
 * func: stops the dmx-card cpu
 */
void slh1512c_disable_card (SLHCard *card)
{
  if (card)
    inb_p (card->iobase); /* this resets the card (1512C manual) */
}


int slh1512c_start (SLHCard *card)
{
  slh1512c_disable_card(card);
  udelay(5);
  slh1512c_enable_card(card);
  return 0;
}


/*
 * name: set_breaksize_us
 * func: sets the breaksize in microseconds
 */
int slh1512c_set_breaksize_us (SLHCard *card, int usBreak)
{
  int n = usBreak?1:0;
  card->write (card, SL1512A_BREAK, (n<0)?0:(n>255)?255:n);
  return 0;
}

static void slh1512c_setslots (SLHCard *card, int slots)
{
  if (card && slots>0 && slots<=512)
    {
      card->write (card, 0x3f3, slots&0xff);
      card->write (card, 0x3f4, ((slots-1)>>8)&0xff);
    }
}


/*
 * Interrupt function for a SLH1512C card.
 */
void slh1512c_interrupt (int irq, void *private, struct pt_regs *regs)
{
  SLHCard *card = (SLHCard *)private;
  if (card)
    {
      int isr = slh1512c_read_isr(card);
      if (isr > 0) /* Has this card been triggered ? */
	{
	  int i;
	  /* The SLG1512C has four interrupt sources
	   * Bit 0 = Frame on universe 0 has been send
	   * Bit 1 = Break on universe 0 has been received
	   * Bit 2 = Frame on universe 1 has been send
	   * Bit 3 = Break on universe 1 has been received
	   */
	  for (i=0; i<2; i++)
	    {
	      if (card->universe[i] && card->universe[i]->univ)
		{
#if 0
		  if (isr & (1<<(0+i*2)))
		    card->universe[i]->univ->signal_changed(card->universe[i], 0, 512); /* signal sender */

		  if (isr & (1<<(1+i*2)))
		    card->universe[i]->univ->signal_changed(card->universe[i], 0, 512); /* signal receiver */
#endif
		}
	    }
	}
    }
}


/*
 * This function is called 100 times per second to
 * do some updateing.
 */
static void slh1512c_checkready (SLHCard *card)
{
  if (card)
    {
      int k;

      for (k=0; k<2; k++)
        {
          int abase = (k>0)?0xC00:0x800;
          SLHUniverse *slhu = card->universe[2+k];
          if (slhu)
            {
              int i;
              int changed_from = -1;
              int changed_to = -1;

              for (i=0; i<512; i++)
                {
                  int c = card->read(card, abase+i);
                  if (slhu->buffer[i] != c)
                    {
                      slhu->buffer[i] = c;
                      if (changed_from==-1)
                        changed_from=i;
                      else
                        changed_to=i;
                    }
                }

              if (changed_to==-1)
                changed_to=changed_from;

              if (changed_from!=-1)
                {
                  slhu->data_avail = 1; 
#if 0
                  slhu->univ->signal_changed(slhu->univ, changed_from, changed_to-changed_from+1);
#else
                  slhu->univ->signal_changed(slhu->univ, 0, 512);
#endif
                }
            }
        }

      /* 
       * Here we will update the data to the card, that has been written using the write
       *     call to the universe.
       */

      /*
       * Transfer the data from the two output universes to the card.
       */
      for (k=0; k<2; k++)
        {
          SLHUniverse *slhu = card->universe[k];
          if (slhu)
            {
              /* This has to be done in a critical section */
              if (slhu->upd_start>=0 && slhu->upd_end>=0)
                {
                  size_t start = slhu->upd_start;
                  size_t end   = slhu->upd_end;
                  slhu->upd_start = slhu->upd_end = -1;
                  if (start<512 && start<=end)
                    {
/*
                      printk (KERN_INFO "syncing buffer to card from %d size %d @ %d\n", start, end-start+1, CARDRAM_DMX+(k*(CARDRAM_DMX2-CARDRAM_DMX))+start);
*/
                      card->write_buffer (card, CARDRAM_DMX+(k*(CARDRAM_DMX2-CARDRAM_DMX))+start, &slhu->buffer[start], end-start+1);
                    }
                }
              /* end of the critical section */
            }
        }
    }
}

#if 0 /* USE_THREAD */
static int slh1512c_thread (void *user_data)
{
  SLHCard *card = (SLHCard *)user_data;
  if (!card)
    return -1;

  printk (KERN_INFO "slh1512cd running\n");

  /* Setup a nice name */
  strcpy(current->comm, "slh1512c");

#if 0
  lock_kernel();
#endif

  /*
   * This thread doesn't need any user-level access,
   * so get rid of all our resources
   */
  exit_files(current);  /* daemonize doesn't do exit_files */

#if 0
  daemonize();
#endif

  card->thread.running = 1;
  do
    {
      slh1512c_checkready (card);

      interruptible_sleep_on_timeout (&card->thread.queue, 100);

    } while (!signal_pending(current));

  printk (KERN_INFO "slh1512c-thread exiting");
  card->thread.running = 0;
  return 0;
}

int slh1512c_startworking (SLHCard *card)
{
  if (card)
    {
      int pid;

      card->thread.pid = -1;
      card->thread.running=0;
#if 0
      init_queue(&card->thread.queue);
#endif

      pid = kernel_thread(slh1512c_thread, (void *)card, CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
      if (pid >= 0)
        {
          card->thread.pid = pid;
          printk (KERN_INFO "pid for slh1512c-thread is %d\n", pid);
          return 0;
        }
    }
  return -1;
}

void slh1512c_stopworking (SLHCard *card)
{
  if (card && card->thread.pid>2)
    {
      int ret;

      printk (KERN_INFO "attempting to kill process with id %d\n", card->thread.pid);

      ret = kill_proc(card->thread.pid, SIGTERM, 0);
      if (!ret)
        {
          /* Wait 10 seconds */
          int count = 10 * 100;
          
          while (card->thread.running && --count)
            {
              current->state = TASK_INTERRUPTIBLE;
              schedule_timeout(1);
            }
          if (!count)
            printk (KERN_INFO "giving up on killing slh1512c-thread");
        }
    }
}

#else
void slh1512c_timer (unsigned long data)
{
  /*static unsigned long cnt = 0L;*/

  unsigned long newtime  = jiffies;
  if (data)
    {
      SLHCard *card = (SLHCard *)data;
#if 1
      if (HZ/20 > 0)
        newtime += HZ/20;
      else
        newtime++;
#else
      printk (KERN_INFO "slh1512c_checkready(%ld)\n", cnt++);
      newtime += HZ;
#endif

      slh1512c_checkready (card);

#if 0
      card->write (card, 0x3f0, 1);
      card->write (card, 0x3f1, 0);
#endif

      mod_timer (&card->timer, newtime);
    }
}

int slh1512c_startworking(SLHCard *card)
{
  init_timer (&card->timer);
  card->timer.function = slh1512c_timer;
  card->timer.data     = (unsigned long)card;
  card->timer.expires  = jiffies + HZ/10;
  add_timer (&card->timer);
  return 0;
}

void slh1512c_stopworking(SLHCard *card)
{
  del_timer(&card->timer);
}
#endif




int slh1512c_card_init (SLHCard *card)
{
  if (card)
    {
      DMXBinary *os = &firmware_slhdmx17;
      int i;

      slh_card_init (card);

      card->read_isr = slh1512c_read_isr;
      card->disable  = slh1512c_disable_card;
      card->enable   = slh1512c_enable_card;

      card->set_break = slh1512c_set_breaksize_us;
      card->set_dbf   = slh1512c_set_delay_between_frames;
      card->set_idt   = slh1512c_set_interdigit_time;

      slh1512c_disable_card (card);
      card->write_buffer (card, 0, os->bin, os->size);

      for (i=0; i<512*2; i++)
        card->write (card, 0x400+i, 0);

      card->write (card, 0x3f0, 44);
      card->write (card, 0x3f1, 0);

      slh1512c_setslots (card, 510);
#if 1
      card->write (card, 0x3f5, 0x80);
#else
      card->write (card, 0x3f5, 0);
#endif
      card->write (card, 0x3f6, 0);

      slh1512c_start (card);
    }
  return -1;
}


static int slh1512c_delete_universe (DMXUniverse *u)
{
  if (u && u->interface && u->interface->user_data && u->user_data)
    {
      SLHUniverse *slhu = (SLHUniverse *)u->user_data;
      SLHCard     *card = (SLHCard *)u->interface->user_data;

      /* csec-begin */

      if (u->kind>=0 && u->kind<2 && u->conn_id>=0 && u->conn_id<2)
        card->universe[u->kind*2+u->conn_id] = NULL;

      DMX_FREE(slhu);

      /* csec-end */

      return 0;
    }
  return -1;
}


int slh1512c_create_universe (DMXUniverse *u, DMXPropList *pl)
{
  if (u && u->interface && u->interface->user_data && u->kind>=0 && u->kind<2)
    {
      SLHCard *card = (SLHCard *)u->interface->user_data;

      u->conn_id = count_universes (u->interface->universes, u->kind);
      if (u->conn_id >=0 && u->conn_id<2)
        {
          SLHUniverse *slhu = DMX_ALLOC(SLHUniverse);
          if (slhu)
            {
              int i;
              for (i=0; i<512; i++)
                slhu->buffer[i]=0;

              slhu->data_avail = 0;
              slhu->upd_start = -1;
              slhu->upd_end = -1;
              slhu->direction = u->kind;
              slhu->univ = u;
              slhu->universe = u->conn_id;
              slhu->card = card;

              u->user_data = slhu;

              card->universe[2*u->kind + u->conn_id] = slhu;

              strcpy (u->connector, u->conn_id?"second":"first");

              if (!u->kind)
                u->write_slots    = slh1512_write_universe;
              u->read_slots     = slh1512_read_universe;
              u->data_available = slh1512_data_available;

              u->user_delete = slh1512c_delete_universe;

              return 0;
            }
        }
    }
  return -1;
}


static int slh1512c_delete_interface (DMXInterface *i)
{
  if (i && i->user_data)
    {
      SLHCard *card = (SLHCard *)i->user_data;

      slh1512c_stopworking(card);

      slh1512c_disable_card(card);

      i->user_data = NULL;
      DMX_FREE(card);
      return 0;
    }
  return -1;
}

int slh1512c_create_interface (DMXInterface *i, DMXPropList *pl)
{
  long iobase = 0L;
  
  ONDBG(printk(KERN_INFO "slh1512c_create_interface (DMXInterface *=%p, DMXPropList *=%p)\n", i, pl));

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
          int j;

          i->user_data = (void *)card;

          card->iobase = iobase;
          card->type   = CARDTYPE_1512C;
	  card->restart_transfer = NULL;

          slh1512c_card_init (card);
          slh1512c_disable_card (card);

#if 0
          card->name    = "test";
          card->irq     = -1;
          card->has_isr = 0;
          card->irqmask = 0L;
          card->os      = NULL;
#endif

          for (j=0; j<4; j++)
              card->universe[j] = NULL;

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

          slh1512c_disable_card (card);    /* reset card */
          card->write (card, 0x3f0, 1);
          card->write_buffer(card, 0, firmware_slhdmx17.bin, firmware_slhdmx17.size);
          slh1512c_disable_card (card);    /* reset card */
          slh1512c_enable_card (card);    /* reset card */


          i->user_delete = slh1512c_delete_interface;


          card->write (card, 0x3f0, 1);
          card->write (card, 0x3f1, 0);

          slh1512c_setslots (card, 510);

          card->write (card, 0x3f5, 0x80);
          slh1512c_start (card);

          slh1512c_startworking(card);

          return 0;
        }
    }
  return -1;
}


