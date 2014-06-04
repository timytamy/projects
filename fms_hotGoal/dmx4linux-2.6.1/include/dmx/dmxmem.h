/*
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

#ifndef __DMX4L_MEM_H__
#define __DMX4L_MEM_H__

#include <dmx/dmxconfig.h>

#define MALLOC(size)     kmalloc((size),GFP_KERNEL)
#define FREE(obj)        kfree(obj)
#define DMX_ALLOC(Type)  (Type *)MALLOC(sizeof(Type))
#define DMX_FREE(obj)    if(obj)FREE(obj)

#endif
