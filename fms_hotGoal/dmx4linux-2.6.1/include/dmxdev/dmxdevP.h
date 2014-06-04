/*
 * dmxdevP.h
 * private header, only for dmxdev.
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
#ifndef __DMXDEVP_H__
#define __DMXDEVP_H__

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
#include <linux/config.h>
#endif

#ifndef CONFIG_MODULES
#error Linux Kernel needs Modules support for DMX4Linux
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#error DMX4Linux requires Linux 2.6.0 or greater
#endif

#if defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS)
#  define MODVERSIONS
#endif

#ifdef MODVERSIONS
#include <config/modversions.h>
#endif

#include <dmx/dmxdev.h>
#include <dmxdev/fileinfo.h>

#define MINOR_NUM(min,node)      int min=MINOR(node->i_rdev)
#define FILE_MINOR_NUM(min,file) int min=MINOR(file->f_dentry->d_inode->i_rdev)

#define FILE_MINOR(file)         MINOR((file)->f_dentry->d_inode->i_rdev)

#define MINVAL(min)		 ((min)>=0 && (min)<DMX_DEVNUMS)
#define DEV_BUF(buf,n)           unsigned char *buf=DMX_Buffer[(n)]

#define IS_MINOR_DMXOUT(x)  ((x)==DMXOUTMINOR)  /* output device. memory mapping possible */
#define IS_MINOR_DMXIN(x)   ((x)==DMXINMINOR)  /* works as dmx-out for inputs */

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif



int  DMXProcInit    (void);
void DMXProcCleanup (void);


int number_input_universes (void);
int number_output_universes (void);


DMXInterface *dmx_family_create_interface (DMXFamily *f, DMXPropList *pl);

/*
 * for internal use only
 */
int
dmx_universe_signal_changed (DMXUniverse *u, uint start, uint size);

#endif
