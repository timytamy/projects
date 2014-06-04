/*
 * dmxproperty.h
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

#ifndef _DMX4L_PROPERTY_H_
#define _DMX4L_PROPERTY_H_

#include <dmx/dmxconfig.h>
#include <dmx/dmxmem.h>

#include <linux/version.h>

#include <linux/list.h>

#include <linux/types.h>

typedef struct struct_DMXProperty DMXProperty;

struct struct_DMXProperty
{
  char *name;  /* name of the property */

  unsigned char type;   /* internal type of the property value */

  union
  {
    char  *String;  /* if type = DMXPTYPE_STRING */
    long   Long;    /* if type = DMXPTYPE_LONG */
  } val;

  int refcount; /* refference counter */

  int  (*get_long) (DMXProperty *, long *);  /* if !NULL values are read using this function */
  int  (*set_long) (DMXProperty *, long);    /* function for writing. =NULL for read-only */

  int  (*get_string) (DMXProperty *, char *, size_t size);  /* if !NULL values are read using this function */
  int  (*set_string) (DMXProperty *, char *);          /* function for writing. =NULL for read-only */

  void (*attach) (DMXProperty *);  /* increses refference counter */
  void (*delete) (DMXProperty *);  /* called for deletion */

  void *data;
};

#define dmxproplist_end(pl) ((struct DMXPropNode *)(&((pl)->list)))



struct DMXPropNode
{
  struct list_head head;

  DMXProperty *prop;

  struct DMXPropNode *(*next) (struct DMXPropNode *pn);
};


typedef struct struct_DMXPropList DMXPropList;
struct struct_DMXPropList
{
  struct list_head    list;

  struct DMXPropNode *(*first) (DMXPropList *pl);
  struct DMXPropNode *(*last)  (DMXPropList *pl);


  int           (*add)    (DMXPropList *pl, DMXProperty *p);
  int           (*remove) (DMXPropList *pl, DMXProperty *p);
  DMXProperty * (*find)   (DMXPropList *pl, char *name);
  int           (*exists) (DMXPropList *pl, DMXProperty *p);

  void          (*delete) (DMXPropList *pl);


  int           (*size)  (DMXPropList *pl);
  int           (*names) (DMXPropList *pl, char *buffer, size_t bufsize, size_t *used);
};




DMXProperty *dmxprop_create        (char *name, unsigned char type, void *data, size_t size);
void         dmxprop_delete        (DMXProperty *p);
DMXProperty *dmxprop_create_long   (char *name, long value);
DMXProperty *dmxprop_create_string (char *name, char *str);

DMXProperty *dmxprop_copy          (DMXProperty *p);

DMXPropList *dmxproplist_vacreate (char *form, ...);
DMXPropList *dmxproplist_create (void);


int dmxprop_user_long (DMXProperty *p, int (*get_long) (DMXProperty *, long *), int (*set_long) (DMXProperty *, long), void *data);
int dmxprop_user_string (DMXProperty *p, int (*get_string) (DMXProperty *, char *, size_t size), int (*set_string) (DMXProperty *, char *), void *);

#define DMXPTYPE_LONG    (1) /* range: 0 .. MAX_LONGINT */
#define DMXPTYPE_STRING  (2) /* range: n.a. */
#define DMXPTYPE_BOOL    (3) /* range: 0 .. 1 */
#define DMXPTYPE_ENUM    (4) /* range: 0 .. <number of items>-1 */
#define DMXPTYPE_USER    (1<<7)

#endif
