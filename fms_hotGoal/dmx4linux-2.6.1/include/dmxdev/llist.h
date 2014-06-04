/*
 * llist.h
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
 * These are templates for Linked-List operations in C.
 * The List elements needs to have a next-pointer.
 * If unsure add a Line like LIST_HEAD(MyType) to the struct
 * with the type-name MyType.
 * examples are at the end of this file.
 */
#ifndef __DMX4L_LLIST_H__
#define __DMX4L_LLIST_H__

#define LLIST_HEAD(Type)  Type *next

#define LLIST_INSERT(root,elem) \
      ((elem)->next = (root), (root) = (elem))


#define LLIST_REMOVE(root,elem) \
  { \
    typeof(root) ii = root; \
    if ((elem) == ii) \
      root = (elem)->next; \
    else \
      { \
        while (ii->next) \
          { \
            if (ii->next == (elem)) \
              { \
                ii->next = (elem)->next; \
                (elem)->next = NULL; \
              } \
            else \
              ii = ii->next; \
          } \
      } \
  }



/* Example:
  struct MyElem
  {
    MyElem *next;   or   LIST_HEAD(MyElem);
    ....
  };


  static int dmx_remove_interface (DMXInterface *i)
  {
    if (i && i->driver && i->driver->interfaces)
        LIST_REMOVE(i->driver->interface, i);
    return 0;
  }
*/

#endif
