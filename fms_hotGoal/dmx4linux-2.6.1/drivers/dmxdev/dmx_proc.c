/*
 * dmx_proc.c
 * proc filesystem handleing for the dmx-device-drivers
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

#define __NO_VERSION__
#include <linux/module.h>

#include <dmxdev/dmxdevP.h>

#include <linux/proc_fs.h>

#ifdef proc_mkdir
#undef proc_mkdir
#endif
#define proc_mkdir(name,root) create_proc_entry(name,S_IFDIR|S_IRUGO|S_IXUGO,root)


/*------[ exported Module-Members ]-----------------*/
static int debug = 0;   /* debug-level */
/*--------------------------------------------------*/



/* name    : dmx_get_universe_info
 * function: This function creates the output from the  /proc/dmx/universes  entry
 */
static int dmx_get_universe_info (char *buf, char **start, off_t offset, int length, int *eof, void *data)
{
  char *p = buf;
  int i, j;

  p += sprintf (p, "#nr\t from-to\tfamily/driver\tconnector/id\n");
  for (j=0; j<2; j++)
    {
      for (i=0; i<MAX_UNIVERSES; i++)
  {
    DMXUniverse *u = dmx_universe_by_index (j, i);
    if (u)
      {
        DMXDriver *d = u->interface?(u->interface->driver):NULL;
        DMXFamily *f = d?d->family:NULL;
        p += sprintf (p, "%s%-2d\t%5d-%d\t%s/%s\t\"%s\"/%d\n",
          (j==0)?"OUT":"IN",
          i,
          i*512,
          (i+1)*512 - 1,
          f?f->name:"NULL",
          d?d->name:"NULL",
          u->connector,
          u->conn_id);
      }
  }
    }
  return p- buf;
}





/*
 * dmx_write_universe_info
 * This is called whenever /proc/dmx/universes/universelist is written to.
 */
static int dmx_write_universe_info (struct file *file, const char *buf,
            unsigned long count, void *data)
{
  const char *Identifier = "abcdefghijklmnopqrstuvwxyz-";
  const char *Number     = "0123456789";

  size_t  pos,pos1;
  char    buffer[200];
  char    cmdname[30];
  int     direction = 0;

  if (count < 200)
    {
      memcpy (buffer,buf,count);
      buffer[count]=0;
      printk (KERN_INFO "universelist-write(%s)\n", buffer);
    }
  /* comands like:
   * create-universe([input|output], <family>/<driver> [,parameter=value]*)
   * delete-universe([input|output], <index>)
   * swap-universe([input|output], <index-a>, <index-b>)
   */

  pos = strspn (buf, Identifier);
  if (pos >= count)
    return -EINVAL;
  strncpy (cmdname, buf, pos);

  while (pos < count && buf[pos]==' ')
    pos++;

  pos1 = strspn (&buf[pos], Identifier);
  if (pos+pos1 >= count)
    return -EINVAL;
  if (strncmp (&buf[pos], "input", pos1)==0)
    direction=1;
  else if (strncmp (&buf[pos], "input", pos1))
    return -EINVAL;
  pos += pos1;

  pos1 = strspn (buf, " ,");
  if (pos+pos1 >= count)
    return -EINVAL;
  pos += pos1;


  if (strncmp (buf, "create-universe",pos)==0)
    {
      DMXDriver *drv = dmx_find_familydriver ((char*)&buf[pos]);
      if (drv)
        {
          DMXInterface *dmx_if = drv->interfaces;
          if (dmx_if)
            dmx_if->create_universe(dmx_if, direction, NULL);
        }
    }

  else if (strncmp (buf, "delete-universe",pos)==0)
    {
      int universe = 0;
      DMXUniverse *u = NULL;
      char *ptr;

      ptr = strpbrk (&buf[pos], Number);
      if (!ptr)
        return -EINVAL;

      universe=simple_strtol (ptr, &ptr,0);
      u = dmx_universe_by_index(direction, universe);
      if (u)
        u->delete(u);

      printk (KERN_INFO "delete-universe (%s, %d)\n", direction?"input":"output", universe);

    }
  return -EINVAL;
}



/* name    : dmx_get_proc_stat
 * function: This function creates the output from the  /proc/dmx  entry
*/
static int dmx_get_proc_stat (char *buf, char **start, off_t offset,
                              int length, int *eof, void *data)
{
  strcpy(buf, "DMX4Linux v" DMXVERSION "\n");
  return strlen(buf);
}


/* name    : dmx_write_proc_stat
 * function: This function creates the output from the  /proc/dmx  entry
*/
static int dmx_write_proc_stat (struct file *file, const char *buf,
                                unsigned long count, void *data)
{
  int  state = 0;
  int  i;

  if (!buf || count<0) return -EINVAL;

  for (i=0; i<count; i++)
    {
      switch (state)
  {
  case 0:
    if (buf[i]=='d' && ((count-i) > 5) && strncmp (&buf[i],"debug",5)==0)
      {
        state++;
        i+=4;
      }
    break;

  case 1:
    if (buf[i]=='=') state++;
    else if (buf[i]!=' ' && buf[i]!='\t') state=0;
    break;

  case 2:
    if (buf[i]=='0' || buf[i]=='1')
      {
        debug = buf[i]-'0';
        printk (KERN_INFO "dmxdev: debug switched %s\n", (debug)?"ON":"OFF");
      }
    return i+1;
    break;
  }
    }
  return -EINVAL;
}


/* name    : dmx_proc_get_structure
 * function: This function returns the internal driver structure (debug-purpose)
*/
static int dmx_proc_get_structure (char *buf, char **start, off_t offset,
           int length, int *eof, void *data)
{
  DMXFamily *f=NULL;
  char *p = buf;

  printk (KERN_INFO "dmx-family=%p\n", f);

  for (f=dmx_get_root_family (); f; f=f->next)
    {
      DMXDriver *d = NULL;

      p += sprintf (p, "%s (this=%p, next=%p, drv=%p)\n", f->name, f, f->next, f->drivers);

      for (d = f->drivers; d; d=d->next)
        {
          DMXInterface *i = NULL;

          p += sprintf (p, "  %s (this=%p, next=%p)\n", d->name, d, d->next);

          for (i=d->interfaces; i; i=i->next)
            {
              DMXUniverse *u = NULL;

              p += sprintf (p, "    (this=%p, next=%p)\n", i, i->next);

              for (u = i->universes; u; u=u->next)
                p += sprintf (p, "      direction=%s,index=%d,conn=%s(%d) (this=%p, next=%p)\n", u->kind?"input":"output", u->index, u->connector, u->conn_id, u, u->next);
            }
        }
    }

  return p-buf;
}



/*
 * name: dmx_write_driver_info
 * you are able to load a driver on demand with the control of the application
 */
static int dmx_write_driverlist_info (struct file *file, const char *buf,
              unsigned long count, void *data)
{
  if (!buf || count<0) return -EINVAL;
  return count;
}




/* name    : dmx_read_driver_info
 * function: This function creates the output from the  /proc/dmx  entry
*/
static int dmx_read_driverlist_info (char *buf, char **start, off_t offset,
             int length, int *eof, void *data)
{
  int i=0;
  char *p = buf;
  DMXFamily *f = dmx_get_root_family ();
  while (f)
    {
      DMXDriver *d = f->drivers;
      while (d)
        {
          p += sprintf (p, "%d %s/%s\n", i, f->name?f->name:"NULL", d->name?d->name:"NULL");
          d = d->next;
          i++;
        }

      f = f->next;
    }
  return p- buf;
}






/* this is the entry  /proc/dmx  that informs about the existing devices      */
static struct proc_dir_entry   *dmx_proc_dir;        /* /proc/dmx/             */
static struct proc_dir_entry   *dmx_proc_list;       /* /proc/dmx/universelist */
static struct proc_dir_entry   *dmx_proc_stat;       /* /proc/dmx/drivers      */

static struct proc_dir_entry   *dmx_proc_driverlist; /* /proc/dmx/families     */
static struct proc_dir_entry   *dmx_proc_drivers;    /* /proc/dmx/families     */

static struct proc_dir_entry   *dmx_proc_families;   /* /proc/dmx/dmx          */

static struct proc_dir_entry   *dmx_proc_universes;  /* /proc/dmx/universes    */
static struct proc_dir_entry   *dmx_proc_univ_out;   /* /proc/dmx/universes/output */
static struct proc_dir_entry   *dmx_proc_univ_in;    /* /proc/dmx/universes/input  */

/* /proc/dmx/universes/output/<index> */
static struct proc_dir_entry   *dmx_proc_universe[MAX_UNIVERSES];

/* /proc/dmx/universes/input/<index> */
static struct proc_dir_entry   *dmx_proc_input_universe[MAX_UNIVERSES];




/* name    :
 * function:
 */
int DMXProcInit ()
{
  int i;
  for (i=0; i<DMX_MAX_DEVS; i++)
    {
      dmx_proc_universe[i] = NULL;
      dmx_proc_input_universe[i] = NULL;
    }

  dmx_proc_dir = proc_mkdir ("dmx", 0);
  if (dmx_proc_dir)
    {
      dmx_proc_universes = proc_mkdir ("universes", dmx_proc_dir);
      if (dmx_proc_universes)
        {
          dmx_proc_univ_out  = proc_mkdir ("output", dmx_proc_universes);
          dmx_proc_univ_in   = proc_mkdir ("input", dmx_proc_universes);
          dmx_proc_list = create_proc_entry ("universelist", S_IFREG | S_IRUGO | S_IWUGO, dmx_proc_universes);
          if (dmx_proc_list)
            {
              dmx_proc_list->read_proc  = dmx_get_universe_info;
              dmx_proc_list->write_proc = dmx_write_universe_info;
            }
        }

      dmx_proc_drivers = proc_mkdir ("drivers", dmx_proc_dir);
      if (dmx_proc_drivers)
        {
          dmx_proc_driverlist = create_proc_entry ("driverlist", S_IFREG | S_IRUGO | S_IWUGO, dmx_proc_drivers);
          if (dmx_proc_driverlist)
            {
              dmx_proc_driverlist->read_proc  = dmx_read_driverlist_info;
              dmx_proc_driverlist->write_proc = dmx_write_driverlist_info;
            }
        }

      dmx_proc_stat = create_proc_entry ("dmx", S_IFREG | S_IRUGO | S_IWUGO, dmx_proc_dir);
      if (dmx_proc_stat)
        {
          dmx_proc_stat->read_proc  = dmx_get_proc_stat;
          dmx_proc_stat->write_proc = dmx_write_proc_stat;
        }

      dmx_proc_families = create_proc_entry ("structure", S_IFREG | S_IRUGO, dmx_proc_dir);
      if (dmx_proc_families)
        dmx_proc_families->read_proc  = dmx_proc_get_structure;
    }

  printk (KERN_INFO "%s: proc entries created\n", MODULE_NAME);

  return 0;
}



/* name    :
 * function:
 */
void DMXProcCleanup ()
{
  remove_proc_entry ("universelist", dmx_proc_dir);
  remove_proc_entry ("input",        dmx_proc_universes);
  remove_proc_entry ("output",       dmx_proc_universes);
  remove_proc_entry ("universes",    dmx_proc_dir);

  remove_proc_entry ("driverlist",   dmx_proc_drivers);
  remove_proc_entry ("drivers",      dmx_proc_dir);

  remove_proc_entry ("dmx",          dmx_proc_dir);
  remove_proc_entry ("structure",    dmx_proc_dir);
  remove_proc_entry ("dmx",          0);
  printk (KERN_INFO "%s: proc entries removed\n", MODULE_NAME);
}






static int dmx_prop_write_universe (struct file *file, const char *buf,
                                    unsigned long count, void *data)
{
  DMXUniverse *u = (DMXUniverse *)data;
  int   i = 0;
  int   state = 0;
  char  name[100];
  char  value[100];
  char  *pname  = name;
  char  *pvalue = value;

  if (!buf || count<0) return -EINVAL;
  while (1)
    {
      int c = buf[i];
      if (state)
  {
    if (c==';' || c==0 || c=='\n' || c=='\r' || i>=count)
      {
              DMXProperty *p = NULL;
        state=0;
        *(pvalue++)=0;

              if (u->props && u->props->find)
                {
                  p = u->props->find (u->props, name);
                  if (!p)
                    {
                      DMXInterface *dmxif = u->interface;
                      if (dmxif->props && dmxif->props->find)
      p = dmxif->props->find (dmxif->props, name);
                    }
                }

              if (p)
                {
                  p->set_string (p, value);
            printk (KERN_INFO "set(%s,%s)\n", name, value);
                }
              else
          printk (KERN_INFO "unknown property %s\n", name);

        pname  = name;
        pvalue = value;
      }
    else
      *(pvalue++)=c;
  }
      else
  {
    if (c=='=')
      {
        state=1;
        *(pname++)=0;
      }
    else
      *(pname++)=c;
  }
      if (i >= count)
  return count;
      i++;
    }
  return count;
}


/*
 *
 */
static int dmx_prop_read_universe (char *buf, char **start, off_t offset,
                                   int length, int *eof, void *data)
{
  char *p = buf;
  DMXUniverse *u = (DMXUniverse *)data;
  if (u)
    {
      DMXInterface *i = u->interface;
      if (i)
        {
          DMXDriver *d = i->driver;
          if (d)
            {
              char id[100] = "";

              DMXFamily *f = d->family;
              if (f)
                {
                  p += sprintf (p, "family=%s\n", f->name);

                  if (d->getUniverseID && (d->getUniverseID (u, id, sizeof(id)) > 0))
                    p += sprintf (p, "id=%s/%s/%s/%d\n", f->name, d->name, id, /*u->conn_id*/ u->index);
                  else
                    p += sprintf (p, "id=unknown\n");
                }

              p += sprintf (p, "driver=%s\n", d->name);
            }

          if (u->props)
            {
              DMXPropList *pl = u->props;
              struct DMXPropNode *pn;

              for (pn = pl->first(pl); &(pn->head) != &(pl->list); pn = pn->next(pn) )
                {
                  DMXProperty *prop = pn->prop;

                  if (prop)
                    {
                      char str[100];
                      if (prop->get_string (prop, str, sizeof(str)) >= 0)
                        p += sprintf (p, "u:%s=%s\n", prop->name, str);
                    }
                }
            }

          if (i->props)
            {
              DMXPropList *pl = i->props;
              struct DMXPropNode *pn;

              for (pn = pl->first(pl); &(pn->head) != &(pl->list); pn = pn->next(pn) )
                {
                  DMXProperty *prop = pn->prop;

                  if (prop)
                    {
                      char str[100];
                      if (prop->get_string (prop, str, sizeof(str)) >= 0)
                        p += sprintf (p, "i:%s=%s\n", prop->name, str);
                    }
                }
            }
        }


      /* This is a fake and should be made cleaner. Create a property list for it. */
      p += sprintf (p, "index=%d\n",     u->index);
      p += sprintf (p, "connector=%s\n", u->connector);
      p += sprintf (p, "conn_id=%d\n",   u->conn_id);
      p += sprintf (p, "direction=%s\n", u->kind?"input":"output");
    }
  return p-buf;
}


/* dmx_universe_create_proc_entry
 * Create a proc entry for that universe.
 */
void dmx_universe_create_proc_entry (DMXUniverse *u)
{
  printk (KERN_INFO "dmx_universe_create_proc_entry(...)\n");
  if (u)
    {
      struct proc_dir_entry **pu = u->kind?dmx_proc_input_universe:dmx_proc_universe;
      int nr = u->index;
      if (nr>=0 && nr<MAX_UNIVERSES && !pu[nr])
        {
          char name[10];

    sprintf (name, "%d", nr);
    pu[nr] = create_proc_entry (name, S_IFREG | S_IRUGO | S_IWUGO, u->kind?dmx_proc_univ_in:dmx_proc_univ_out);
    pu[nr]->read_proc  = dmx_prop_read_universe;
    pu[nr]->write_proc = dmx_prop_write_universe;
    pu[nr]->data       = (void *)u;
        }
    }
}


void dmx_universe_delete_proc_entry (DMXUniverse *u)
{
  printk (KERN_INFO "dmx_universe_delete_proc_entry(...)\n");
  if (u)
    {
      struct proc_dir_entry **pu = u->kind?dmx_proc_input_universe:dmx_proc_universe;
      int nr = u->index;
      if (nr>=0 && nr<MAX_UNIVERSES && pu[nr])
        {
          char name[10];
          sprintf (name, "%d", nr);
          remove_proc_entry (name, u->kind?dmx_proc_univ_in:dmx_proc_univ_out);
          pu[nr] = NULL;
        }
    }
}
