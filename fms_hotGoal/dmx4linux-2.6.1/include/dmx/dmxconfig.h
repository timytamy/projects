/*
 * dmxconfig.h
 * some definitions for dmx4linux
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

#ifndef __DMX__DMXCONFIG_H__
#define __DMX__DMXCONFIG_H__

#define  DMXDEV_RELEASE   (DMXVERSION*10)
#define  DMX4LINUX_VERSION   DMXDEV_RELEASE

#define dprintk  if(debug>0)printk

#define MODULE_NAME     "dmxdev"

#define DMX_CLASSNUMS   10
#define	DMX_MAX_DEVS    16            /* number of supported devices */
#define DMX_DEVNUMS     (DMX_MAX_DEVS)
#ifndef DMX_MAXBUFSIZE
#define DMX_MAXBUFSIZE 512
#endif

#define MAX_UNIVERSES 20    /* maximum number of universes */

//#include <linux/config.h>
#include <linux/version.h>

#define KILL_FASYNC(a,b,c)	kill_fasync(&(a),(b),(c))

/* do we really need those old kernels ??? */
#include <linux/version.h>
#include <linux/spinlock.h>
#define BASE_ADDRESS(x,i)       ((x)->resource[i].start)
#define SETUP_PARAM             char *str
#define SETUP_PARSE(x)          int ints[x]; get_options(str, x, ints)

#if defined(CONFIG_RTHAL) && CONFIG_RTHAL>0
#define CONFIG_KERNEL_HAS_RTAI CONFIG_RTHAL
#else
#undef CONFIG_KERNEL_HAS_RTAI
#endif


#endif
