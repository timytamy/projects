/*
 * Dmxdummy.c
 * a driver that does nothing but registering a dummy driver.
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


#include <linux/module.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#include <dmx/dmxdev.h>
#include <dmx/dmxconfig.h>

#define DEBUG 0

#ifdef DEBUG
#define DO_DEBUG(x...) (x)
#else
#define DO_DEBUG(x...)
#endif

#define UNIVERSE_SIZE 512

#define PROC_DIRECTORY "dmxdummy"

#ifdef proc_mkdir
#undef proc_mkdir
#endif
#define proc_mkdir(name,root) create_proc_entry(name,S_IFDIR|S_IRUGO|S_IXUGO,root)


static int in_universes=1;    /* create 1 input universe by default */
module_param(in_universes, int, S_IRUGO);
MODULE_PARM_DESC(in_universes, "Number of input universes to be created on module-startup");

static int out_universes=1;    /* create 1 output universe by default */
module_param(out_universes, int, S_IRUGO);
MODULE_PARM_DESC(out_universes, "Number of output universes to be created on module-startup");

static int loopback[MAX_UNIVERSES];
module_param_array(loopback, int, NULL, 0/*S_IRUGO|S_IWUGO*/);
MODULE_PARM_DESC(loopback, "Set to 1 to loop back output slots on the input universes");

MODULE_AUTHOR ("(c) 1999-2001 Michael Stickel");
MODULE_DESCRIPTION ("Dummy interface used for test purpose if no hardware is available or needed. version " DMXVERSION);
MODULE_LICENSE("GPL");


typedef struct
{
  char                   proc_entry_name[10];
  struct proc_dir_entry *proc_entry;
  char                   cooked_proc_entry_name[10];
  struct proc_dir_entry *cocked_proc_entry;
  unsigned char          buffer[UNIVERSE_SIZE];
  DMXUniverse           *universe;
  char                   data_avail;
} DummyUniverse;


#define GetDummyUniverse(u) (((u)->conn_id>=0 && (u)->conn_id<MAX_UNIVERSES)?((u)->kind?(input_universe[(u)->conn_id]):(output_universe[(u)->conn_id])):NULL)



static struct proc_dir_entry   *outdir_entry = NULL;
static DMXFamily               *dummy_family = NULL;
static DummyUniverse           *input_universe[MAX_UNIVERSES],
                               *output_universe[MAX_UNIVERSES];



/*
 *-----------------[ dmxdev access functions ]-------------------
 */

/*
 * This method is called by the dmxdev module
 * if slots have been changed by the userspace
 * for e.g. by a write to /dev/dmx.
 */
static int  write_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  int id;

  if (!u || !buff || size<=0) return -EINVAL;

  if(offs+size > UNIVERSE_SIZE)
    size=UNIVERSE_SIZE-size;

  id=u->conn_id;

  if(u->kind)
    {
      DO_DEBUG(printk("write input %d %d\n", (int)offs, size));
      memcpy (&input_universe[id]->buffer[offs], buff, size);
      input_universe[id]->data_avail=1;
      input_universe[id]->universe->signal_changed (input_universe[id]->universe, offs, size);
    }
  else
    {
      DO_DEBUG(printk("write output %d %d\n", (int)offs, size));
      memcpy (&output_universe[id]->buffer[offs], buff, size);
      output_universe[id]->data_avail=1;
      output_universe[id]->universe->signal_changed (output_universe[id]->universe, offs, size);
      if(loopback[id])
	memcpy (&input_universe[id]->buffer[offs], &output_universe[id]->buffer[offs], size);
    }

  return size;
}


/*
 * read slots from the universe.
 * This function can be used for input and the output universe.
 */
static int read_universe (DMXUniverse *u, off_t start, DMXSlotType *buff, size_t size)
{
  if (u && start >= 0 && start < UNIVERSE_SIZE)
    {
      int id = u->conn_id;
      if (id>=0 && id<MAX_UNIVERSES)
	{
	  if (start+size>UNIVERSE_SIZE)
	    size = UNIVERSE_SIZE-start;
	  if (u->kind)
	    {
	      memcpy (buff, &input_universe[id]->buffer[start], size);
	      input_universe[u->conn_id]->data_avail = 0;
	    }
	  else
	    {
	      memcpy (buff, &output_universe[id]->buffer[start], size);
	      output_universe[u->conn_id]->data_avail = 0;
	    }
	  return size;
	}
    }
  return -EINVAL;
}

/*
 * returns < 0 for error, > 0 if new data is available and 0
 * if no new data is available for that universe.
 */
static int  dummy_data_available (DMXUniverse *u, uint start, uint size)
{
  if (!u || u->conn_id<0 || u->conn_id>=MAX_UNIVERSES) return 0;
  if (u->kind)
    return input_universe[u->conn_id]->data_avail;
  /* output is by definition at any time ready to read */
  return 1;
}


/*
 *------------[ proc-fs access functions ]---------------------
 */

/*
 * This method is called if someone writes to
 * the file /proc/dmxdummy/in<connector>
 */
static int proc_write_universe (struct file *file, const char *buf, unsigned long count, void *data)
{
  DMXUniverse *u = (DMXUniverse *)data;

  if (u)
    {
      int start=file->f_pos;
      DummyUniverse *du = GetDummyUniverse(u);
      if (!du)
	return -EBUSY;

      if (count > UNIVERSE_SIZE)
	count = UNIVERSE_SIZE;
      if(start+count > UNIVERSE_SIZE)
	  count=UNIVERSE_SIZE-start;

      memcpy (&(du->buffer[start]), buf, count);
      du->data_avail=1;
      u->signal_changed (u, start, count); /* a signal for the channel that has changed */
      return count;
    }
  return -EINVAL;
}

/*
 * This method is called if someone reads from
 * the file /proc/dmxdummy/out<connector>
 */
static int proc_read_universe (char *buf, char **start, off_t offset, int length, int *eof, void *data)
{
  DMXUniverse *u = (DMXUniverse *)data;
  if (u && u->conn_id>=0 && u->conn_id<MAX_UNIVERSES)
    {
      DummyUniverse *du = GetDummyUniverse(u);

      if (!du)
	return -EINVAL;

      if (length+offset > UNIVERSE_SIZE)
        length = UNIVERSE_SIZE - offset;

      if (length > 0)
        memcpy (buf, &(du->buffer[offset]), length);

      return length;
    }
  return -EINVAL;
}

/*
 * This method is called if someone reads from
 * the file /proc/dmxdummy/outc<connector>
 */
static int proc_read_universe_cocked (char *buf, char **start, off_t offset, int length, int *eof, void *data)
{
  DMXUniverse *u = (DMXUniverse *)data;
  if (u && u->conn_id>=0 && u->conn_id<MAX_UNIVERSES)
    {
      DummyUniverse *du = GetDummyUniverse(u);
      char *p = buf;
      int i;

      if (!du)
	return -EINVAL;

      for (i=0; i<UNIVERSE_SIZE; i++)
        p += sprintf (p, "%02x ", du->buffer[i]);

      *p++='\n';
      return p-buf;
    }
  return -EINVAL;
}


/*
 *---------------[ setter / getter functions ]------------------
 */


static int loopback_get_long (DMXProperty *p, long *val)
{
  if (p && val && p->type == (DMXPTYPE_LONG|DMXPTYPE_USER))
    {
      DMXUniverse *u = (DMXUniverse *)p->data;
      if (u && u->conn_id>=0 && u->conn_id<MAX_UNIVERSES)
        {
          *val = loopback[u->conn_id];
          return 0;
        }
    }
  return -1;
}

static int loopback_set_long (DMXProperty *p, long val)
{
  if (p && val && p->type == (DMXPTYPE_LONG|DMXPTYPE_USER))
    {
      DMXUniverse *u = (DMXUniverse *)p->data;
      if (u && u->conn_id>=0 && u->conn_id<MAX_UNIVERSES)
        {
          loopback[u->conn_id] = val?1:0;
          return 0;
        }
    }
  return -1;
}



/*
 *---------------[ universe creation / deletion ]------------------
 */

/*
 * This method is called before the universe will be deleted
 */
static int delete_universe (DMXUniverse *u)
{
  if (u && outdir_entry && u->conn_id>=0 && u->conn_id<MAX_UNIVERSES)
    {
      DummyUniverse *du = GetDummyUniverse(u);
      remove_proc_entry (du->proc_entry_name, outdir_entry);
      remove_proc_entry (du->cooked_proc_entry_name, outdir_entry);

      printk (KERN_INFO "dummy-universe deleted\n");

      if (u->kind)
	{
	  DMX_FREE(input_universe[u->conn_id]);
	  input_universe[u->conn_id] = NULL;
	}
      else
	{
	  DMX_FREE(output_universe[u->conn_id]);
	  output_universe[u->conn_id] = NULL;
	}
   }
  return 0;
}


/*
 * return the id of the interface.
 *
 */
static int misc_getUniverseID (DMXUniverse *u, char *id, size_t size)
{
  if (u && id && size >= MAX_UNIVERSES)
    {
      char *p = id;
      p += sprintf(id, "%d", u->conn_id);
      return p-id;
    }
  return -1;
}




static int create_universe (DMXUniverse *u, DMXPropList *pl)
{
  long iobase = 0L;
  if (pl)
    {
	DMXProperty *p = pl->find(pl, "iobase");
        if (p) p->get_long(p, &iobase);
    }

  printk ("dmxdummy:create_universe_%s(iobase=%ld) called\n",
	  (u->kind==0)?"out":"in",
	  iobase);
  if (u)
    {
      int maxid = 0;
      DMXUniverse *tu = NULL;
      DummyUniverse *du = NULL;

      if ((u->kind==0 && output_universe[u->conn_id]) || (u->kind==1 && input_universe[u->conn_id]))
	return -EBUSY;

      u->user_delete = delete_universe;

      /*
       *
       */
      for (tu = u->interface ? u->interface->universes : NULL; tu; tu = tu->next)
        {
          if (tu->conn_id >= maxid && tu->kind==u->kind)
            maxid = tu->conn_id + 1;
        }

      strcpy (u->connector, "none"); /* it's an interface that has definitly no connector */
      u->conn_id = maxid;

      if (u->conn_id >=0 && u->conn_id<MAX_UNIVERSES)
	{
	  int  i;

	  if (u->kind)
	    du = input_universe[u->conn_id] = DMX_ALLOC(DummyUniverse);
	  else
	    du = output_universe[u->conn_id] = DMX_ALLOC(DummyUniverse);
	  if (!du)
	    {
	      printk (KERN_INFO "unable to create internal-dmxdummy structures\n");
	      return -EBUSY;
	    }

	  u->data_available = dummy_data_available;

	  sprintf (du->proc_entry_name, u->kind?"in%d":"out%d", u->conn_id);
	  du->proc_entry = create_proc_entry (du->proc_entry_name, S_IFREG|S_IRUGO|S_IWUGO, outdir_entry);
	  du->proc_entry->write_proc = proc_write_universe;
	  du->proc_entry->read_proc  = proc_read_universe;
	  du->proc_entry->data       = (void *)u;

	  sprintf (du->cooked_proc_entry_name, u->kind?"in%dc":"out%dc", u->conn_id);
	  du->cocked_proc_entry = create_proc_entry (du->cooked_proc_entry_name, S_IFREG | S_IRUGO, outdir_entry);
	  du->cocked_proc_entry->read_proc  = proc_read_universe_cocked;
	  du->cocked_proc_entry->data       = (void *)u;

	  du->universe = u;

	  for (i=0; i<UNIVERSE_SIZE; i++)
	    du->buffer[i]=0;
	  du->data_avail = 0;

	  u->read_slots   = read_universe;
	  u->write_slots =  write_universe;

          if (pl)
            {
              DMXProperty *p = pl->find(pl, "loopback");
              if (p)
                {
                  dmxprop_user_long (p, loopback_get_long, loopback_set_long, (void *)u);
                  p->data = (void *)u;
                }
            }

	}
    }
  return 0;
}



/*
 *----------------[ module initialition / cleanup ]------------------
 */

/*
 * Called by the kernel-module-loader after the module is loaded.
 */
static int __init dummy_init(void)
{
  int  i;

  printk ("dmxdummy:init_module called\n");

  if (in_universes>MAX_UNIVERSES || out_universes>MAX_UNIVERSES)
    {
      printk ("in_universes or out_universes is greater than %d\n", MAX_UNIVERSES);
      return -EINVAL;
    }

  for (i=0; i<MAX_UNIVERSES; i++)
    input_universe[i] = output_universe[i] = NULL;

  outdir_entry = proc_mkdir (PROC_DIRECTORY, 0);

  dummy_family = dmx_create_family ("dummy");
  if (dummy_family)
    {
      DMXDriver *drv = dummy_family->create_driver (dummy_family, "dummy", create_universe, NULL);

      printk (KERN_INFO "dmx_find_driver(dummy_family,\"dummy\") => %p\n", dmx_find_driver(dummy_family, "dummy") );

      if (drv)
        {
          DMXInterface *dif;

	  drv->getUniverseID = misc_getUniverseID;

	  dif = drv->create_interface (drv, NULL);
          if (dif)
            {
              int  j;
              for (j=0; j<out_universes; j++)
		  dif->create_universe (dif, 0, dmxproplist_vacreate("loopback=0"));

              for (j=0; j<in_universes; j++)
		  dif->create_universe (dif, 1, dmxproplist_vacreate("test=1,aaaa=0"));
              return 0;
            }
        }
      dummy_family->delete(dummy_family, 0);
    }
  remove_proc_entry (PROC_DIRECTORY, 0);
  return -1;
}



/*
 * Called by the kernel-module-loader before the module is unloaded.
 */
static void __exit dummy_exit(void)
{
  printk ("dmxdummy:cleanup_module called\n");
  if (dummy_family)
    dummy_family->delete(dummy_family, 0);
  remove_proc_entry (PROC_DIRECTORY, 0);
}

module_init(dummy_init);
module_exit(dummy_exit);
