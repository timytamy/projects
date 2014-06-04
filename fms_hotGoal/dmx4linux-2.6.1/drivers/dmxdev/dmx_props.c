/*
 * dmx_props.c
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
 * These functions are for handling of properties.
 * Those properties are used variable handling of
 * settings for interfaces and universes.
 */

#define __NO_VERSION__
#include <linux/module.h>

#include <dmxdev/dmxdevP.h>
#include <dmx/dmxproperty.h>

#include <linux/slab.h>

static int  dmxprop_get_long   (DMXProperty *p, long *pval);
static int  dmxprop_set_long   (DMXProperty *p, long val);
static int  dmxprop_get_string (DMXProperty *p, char *str, size_t maxlen);
static int  dmxprop_set_string (DMXProperty *p, char *str);
static void dmxprop_attach (DMXProperty *p);


static struct DMXPropNode *dmxpropnode_next  (struct DMXPropNode *pn);
static struct DMXPropNode *dmxproplist_first (DMXPropList *pl);
static struct DMXPropNode *dmxproplist_last  (DMXPropList *pl);

static int dmxproplist_size  (DMXPropList *list);
static int dmxproplist_names (DMXPropList *pl, char *buffer, size_t bufsize, size_t *used);

/* #define DO_DEBUG(x...) x */
#define DO_DEBUG(x...)


static char *dmx_strdup(const char *str)
{
  char *newstr = kmalloc (strlen(str)+1, GFP_KERNEL);
  if (newstr)
    strcpy (newstr, str);
  return newstr;
}


static void strfree (char *s)
{
  if (s)
    kfree(s);
}


static int dmxprop_get_long (DMXProperty *p, long *pval)
{
  if (p && pval)
    {
      if (p->type & DMXPTYPE_USER && p->get_string != dmxprop_get_string)
	{
	  char str[50];
	  int stat = p->get_string (p, str, sizeof(str));
	  if (stat < 0) return stat;
	  *pval = simple_strtoul(str, NULL, 0);
	  return 0;
	}
      else
	{
	  switch (p->type)
	    {
	    case DMXPTYPE_LONG:   *pval = p->val.Long;         return 0;
	    case DMXPTYPE_STRING: *pval = simple_strtoul(p->val.String, NULL, 0); return 0;
	    default: break;
	    }
	}
    }
  return -1;
}


static int dmxprop_set_long (DMXProperty *p, long val)
{
  if (p)
    {
      if (p->type & DMXPTYPE_USER && p->set_string != dmxprop_set_string)
	{
	  char str[50];
	  str[0]=0;
	  sprintf(str, "%ld", val);
	  return dmxprop_set_string (p, str);
	}
      else
	{
	  switch (p->type)
	    {
	    case DMXPTYPE_LONG:   p->val.Long=val; return 0;
	    case DMXPTYPE_STRING:
	      {
		char str[50];
		char *pstr = p->val.String;
		sprintf(str, "%ld", val);
		p->val.String = dmx_strdup(str);
		strfree(pstr);
		return 0;
	      }
	    default: break;
	    }
	}
    }
  return -1;
}


static int dmxprop_get_string (DMXProperty *p, char *str, size_t maxlen)
{
  if (p && str && maxlen>0)
    {
      if (p->type & DMXPTYPE_USER && p->get_long != dmxprop_get_long && p->get_long)
	{
	  long tval=0L;
	  int stat = p->get_long (p, &tval);
	  if (stat >= 0)
            sprintf (str, "%ld", tval);
	  return stat;
	}
      else
	{
	  switch (p->type)
	    {
	    case DMXPTYPE_LONG:   sprintf (str, "%ld", p->val.Long);         return 0;
	    case DMXPTYPE_STRING: if (strlen(p->val.String)<maxlen) strcpy(str,p->val.String); return 0;
	    default: *str=0; break;
	    }
	}
    }
  return -1;
}


static int dmxprop_set_string (DMXProperty *p, char *str)
{
  if (p && str)
    {
      if (p->type & DMXPTYPE_USER && p->set_long != dmxprop_set_long && p->set_long)
	{
	  return p->set_long(p, simple_strtoul(str,NULL,0));
	}
      else
	{
	  switch (p->type & ~DMXPTYPE_USER)
	    {
	    case DMXPTYPE_LONG:   p->val.Long=simple_strtoul(str,NULL,0); return 0;
	    case DMXPTYPE_STRING:
	      {
		char *pstr = p->val.String;
		p->val.String = dmx_strdup(str);
		strfree(pstr);
		return 0;
	      }
	    default: break;
	    }
	}
    }
  return -1;
}






DMXProperty *dmxprop_create (char *name, unsigned char type, void *data, size_t size)
{
  DMXProperty *p = DMX_ALLOC(DMXProperty);
  if (p)
    {
      p->name = dmx_strdup(name);
      p->type = type;
      if (type & DMXPTYPE_USER)
	{
	  p->val.Long = 0L;
	}
      else
	{
	  switch (type)
	    {
	    case DMXPTYPE_LONG:
	      p->val.Long = *((long *)data);
	      break;

	    case DMXPTYPE_STRING:
	      p->val.String = dmx_strdup((char *)data);
	      break;

	    default:
	      DMX_FREE(p);
	      return NULL;
	    }
	}
      p->get_long   = dmxprop_get_long;
      p->set_long   = dmxprop_set_long;
      p->get_string = dmxprop_get_string;
      p->set_string = dmxprop_set_string;
      p->delete = dmxprop_delete;
      p->attach = dmxprop_attach;
      p->refcount = 0;
    }
  return p;
}


int dmxprop_user_long (DMXProperty *p, int (*get_long) (DMXProperty *, long *), int (*set_long) (DMXProperty *, long), void *data)
{
  if (p && p->type == DMXPTYPE_LONG && get_long)
    {
      p->type |= DMXPTYPE_USER;
      p->get_long = get_long;
      p->set_long = set_long;
      p->data = data;
      return 0;
    }
  return -1;
}


int dmxprop_user_string (DMXProperty *p, int (*get_string) (DMXProperty *, char *, size_t size), int (*set_string) (DMXProperty *, char *), void *data)
{
  if (p && p->type == DMXPTYPE_STRING && get_string)
    {
      p->type |= DMXPTYPE_USER;
      p->get_string = get_string;
      p->set_string = set_string;
      p->data = data;
      return 0;
    }
  return -1;
}



static void dmxprop_attach (DMXProperty *p)
{
  if (p)
    p->refcount++;
}

void dmxprop_delete (DMXProperty *p)
{
  if (p)
    {
      p->refcount--;  /* TODO: enclose p->refcount into critical-section */
      if (p->refcount <= 0)
	{
	  DO_DEBUG(printk (KERN_INFO "dmxprop_delete: refcount for %s = %d => deleting instance\n", p->name, p->refcount));
	  if (p->type == DMXPTYPE_STRING)
	    strfree (p->val.String);
	  DMX_FREE(p);
	}
      else
        {
	  DO_DEBUG(printk (KERN_INFO "dmxprop_delete: refcount for %s = %d => NOT deleting instance\n", p->name, p->refcount));
        }
    }
  else
    printk (KERN_INFO "dmxprop_delete: property pointer is NULL\n");
}



DMXProperty *dmxprop_create_long (char *name, long value)
{
  return dmxprop_create (name, DMXPTYPE_LONG, (void *)&value, sizeof(value));
}

DMXProperty *dmxprop_create_string (char *name, char *str)
{
  return dmxprop_create (name, DMXPTYPE_STRING, (void *)str, strlen(str)+1);
}




DMXProperty *dmxprop_copy (DMXProperty *p)
{
  if (p)
    {
      DMXProperty *np = DMX_ALLOC(DMXProperty);
      if (np)
	{
	  np->get_long   = p->get_long;
	  np->set_long   = p->set_long;
	  np->get_string = p->get_string;
	  np->set_string = p->set_string;
	  np->delete = dmxprop_delete;
	  np->attach = dmxprop_attach;
	  np->refcount = 1;

	  np->name = dmx_strdup(p->name);
	  np->type = p->type;
	  np->val.Long = 0L;
	  if (p->type & DMXPTYPE_USER)
	    {
	      char tstr[100];
	      if (p->get_string (p, tstr, sizeof(tstr)) < 0 || np->set_string (np, tstr) < 0)
		{
		  DMX_FREE(np);
		  return NULL;
		}
	    }
	  else
	    {
	      switch (p->type)
		{
		case DMXPTYPE_LONG:
		  np->val.Long = p->val.Long;
		  break;

		case DMXPTYPE_STRING:
		  p->val.String = dmx_strdup(p->val.String);
		  break;

		default:
		  DMX_FREE(np);
		  return NULL;
		}
	    }
	}
      return np;
    }
  return NULL;
}




/*
 *------------ PropLists ------------------
 */


/*
 * i->create_universe (i, dmxprop_vacreate ("membase=0xa000, irq=9"), 2),
 * pl = dmxproplist_vacreate ("iobase=%s,membase=%l", "0x300", 0xA000L)
 * pl = dmxproplist_vacreate ("iobase=0x300,membase=%l", 0xA000L)
 * pl = dmxproplist_vacreate ("iobase=0x300,membase=0xA000")
 */


int   dmxproplist_add    (DMXPropList *pl, DMXProperty *p)
{
  if (pl && p)
    {
      struct DMXPropNode *pn = DMX_ALLOC(struct DMXPropNode);
      if (pn)
	{
	  INIT_LIST_HEAD(&pn->head);
	  pn->prop = p;
	  pn->next = dmxpropnode_next;
	  list_add(&pn->head, &(pl->list));

	  if (p->attach) p->attach(p);
	  return 1;
	}
    }
  return 0;
}



int   dmxproplist_remove (DMXPropList *pl, DMXProperty *p)
{
  DO_DEBUG(printk (KERN_INFO "dmxproplist_remove (%p, %p)\n", pl, p));
  if (pl && p /* && pl->exists(pl, p)*/)
    {
      struct list_head *pn;

      for (pn = pl->list.next; pn != &(pl->list); pn = pn->next)
	{
	  /*	  struct DMXPropNode *pn; */
	  if (((struct DMXPropNode *)pn)->prop == p)
	    {
	      DO_DEBUG(printk (KERN_INFO "dmxproplist_remove: remove and try delete property %s\n", p->name?p->name:"NULL"));

	      list_del(pn);
	      /* TODO: what happens to the memory that has been allocated for the DMXPropNode?
	       * Has to be freed.
	       */

	      p->delete(p); /* loescht nur wenn refference count < 0 */
	      return 1;
	    }
	}
      return 0;
    }
  return -1;
}


int   dmxproplist_remove_byname (DMXPropList *pl, char *name)
{
  return (pl && name) ? dmxproplist_remove (pl, pl->find (pl, name)) : 0;
}


DMXProperty *dmxproplist_find  (DMXPropList *pl, char *name)
{
  if (pl)
    {
      struct list_head *pn;
      for (pn = pl->list.next; pn != &(pl->list); pn = pn->next)
	{
	  struct DMXPropNode *node = (struct DMXPropNode *)pn;

	  if (node->prop && !strcmp(node->prop->name, name))
	    return node->prop;
	}
    }
  return NULL;
}


#define PropListEmpty(pl) list_empty(&pl->list)

void  dmxproplist_delete (DMXPropList *pl)
{
  if (pl)
    {
      DO_DEBUG(printk (KERN_INFO "dmxproplist_delete (%p)\n", pl));
      while ( ! PropListEmpty(pl) )
	{
	  struct DMXPropNode *pn = (struct DMXPropNode *)(pl->list.next);
	  if (pn)
	    {
	      DO_DEBUG(printk (KERN_INFO "dmxproplist_delete try remove property %s\n", pn->prop?pn->prop->name:"<unknown>"));
	      pl->remove(pl, pn->prop);
	    }
	}
      DMX_FREE(pl);
    }
  else
    printk (KERN_INFO "calling dmxproplist_delete with NULL pointer\n");
}

static struct DMXPropNode *dmxpropnode_next  (struct DMXPropNode *pn)
{
  return (struct DMXPropNode *)(pn?pn->head.next:NULL);
}

static struct DMXPropNode *dmxproplist_first (DMXPropList *pl)
{
  return (struct DMXPropNode *)(pl?pl->list.next:NULL);
}

static struct DMXPropNode *dmxproplist_last  (DMXPropList *pl)
{
  return (struct DMXPropNode *)(pl?pl->list.prev:NULL);
}


#undef DLIST_INIT
#define DLIST_INIT(head,listnam) (head)->listnam.dl_prev = (head)->listnam.dl_next = (head)

DMXPropList *dmxproplist_create ()
{
  DMXPropList *pl = DMX_ALLOC(DMXPropList);
  if (pl)
    {
      INIT_LIST_HEAD(&pl->list);

      pl->first = dmxproplist_first;
      pl->last  = dmxproplist_last;

      pl->add           = dmxproplist_add;
      pl->remove        = dmxproplist_remove;

      pl->find          = dmxproplist_find;

      pl->delete        = dmxproplist_delete;

      pl->size          = dmxproplist_size;
      pl->names         = dmxproplist_names;
    }
  return pl;
}



DMXPropList *dmxproplist_vacreate (char *form, ...)
{
  DMXPropList *pl = NULL;
  va_list ap;
  char *fp = form;
  int  state=0;
  int  ni=0, vi=0;
  int  c=0;
  char name[100];
  char value[100];

  va_start(ap, form);

  if (!*fp)
    return NULL;

  do
    {
      c = *(fp++);

      switch (state)
	{
	case 0: /* scanning name */
	  if (!c) break;
	  if (c=='=')      { name[ni] = 0; state++; vi=0;}
	  else if (c==',') ni=0;
	  else             name[ni++] = c;
	  break;


	case 1: /* scan for type */
	  if (!c) break;
	  if (c=='%')  state=2;
	  else {state=4; value[vi++]=c;}
	  break;


	case 2:
	  if (!c) break;
	  switch (c)
	    {
	    case 's': /* get char * */
	      {
		char *s = va_arg(ap, char *);
		if (s)
		  {
                    if (!pl) pl = dmxproplist_create ();
		    if (pl && pl->add)
		      pl->add(pl, dmxprop_create_string(name, s));
		  }
	      }
	      break;

	    case 'd': /* decimal */
	    case 'l': /* long */
	      {
		long l = va_arg(ap, long);
                if (!pl) pl = dmxproplist_create ();
		if (pl && pl->add)
		  pl->add(pl, dmxprop_create_long(name, l));
	      }
	      break;
	    }
	  state=3;
	  break;


	case 3:
	  if (!c) break;
	  if (c==',')
	    {
	      state=0;
	      ni=0;
	    }
	  break;


	case 4: /* scanning value */
	  if (c==',' || c==0)
	    {
	      value[vi]=0;
              if (!pl) pl = dmxproplist_create ();
	      if (pl && pl->add)
		pl->add(pl, dmxprop_create_string(name, value));
	      state=0;
	      ni=0;
	      vi=0;
	    }
	  else
	    value[vi++]=c;
	  break;

	}
    }
  while (c);

  va_end(ap);

  return pl;
}




/*---------------------------------------
 * Return the number of items in the proplist.
 *-------------------------------------*/
static int dmxproplist_size (DMXPropList *pl)
{
  int size = 0;
  if (pl)
    {
      struct list_head *pn;
      for (pn = pl->list.next; pn != &(pl->list); pn = pn->next)
	size++;
    }
  return size;
}

/*---------------------------------------
 *
 *-------------------------------------*/
static int dmxproplist_names (DMXPropList *pl, char *buffer, size_t bufsize, size_t *used)
{
  int     count = 0; /* number of names */
  size_t  sizeremain = bufsize; /* remaining size of buffer */

  if (pl)
    {
      struct list_head *pn;
      for (pn = pl->list.next; pn != &(pl->list); pn = pn->next)
	{
	  struct DMXPropNode *node = (struct DMXPropNode *)pn;

	  if (node->prop && node->prop->name)
	    {
	      int namelen = strlen (node->prop->name)+1;
	      if (namelen < sizeremain)
		{
		  strcpy(buffer, node->prop->name);
		  buffer += namelen;
		  sizeremain -= namelen;
		  count++;
		}
	    }
	}
    }
  if (used)
    *used = bufsize - sizeremain; /* used space in buffer */
  return count;
}

