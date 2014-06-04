/*
 * fileinfo.c
 * Device-driver and registration module for dmx-devices
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
#ifndef __DMXDEV_FILEINFO_H__
#define __DMXDEV_FILEINFO_H__

#include <dmx/dmxdev.h>

#include <linux/sched.h>
#include <linux/fs.h>


/*
 * This structure manages the informations
 * that are specific for each session opened
 * by the user (using open).
 */
struct _DMXFileInfo
{
  uint                flags;
#define DMXFI_F_REQUEST (1<<0)  /* The values in the structure request are valid. */
#define DMXFI_F_POLL    (1<<1)  /* Someone polls for data. */
  /* causes a wake to read_wait_queue if values are changed in that range */

  int                 target;
#define DMXFI_DMXUNDEF  (0)
#define DMXFI_DMXOUT    (1)
#define DMXFI_DMXIN     (2)
#define DMXFI_DMXMON    (3)

  char                block; /* 0 for nonblocking, 1 for blocking read operation */

  struct _DMXFI_Request
  {
    uint              start; /* first slot requested for this file */
    uint              size;   /* last slot ... */
  } request;
  char                changed; /* specified input has changed */


  int                 (*data_available) (struct _DMXFileInfo *fi, uint start, uint size);
  size_t              (*to_user) (struct _DMXFileInfo *fi, unsigned char *buff, off_t offs, size_t size);

  wait_queue_head_t   read_wait_queue; /* wait for data available for this file */

#if 0 /* some things change in 2.4.0 */
  struct wait_queue  *read_wait_queue; /* wait for data available for this file*/
#endif

  struct fasync_struct *async_queue;
};
typedef struct _DMXFileInfo DMXFileInfo;


void         dmx_fileinfo_init   (void);
DMXFileInfo *dmx_fileinfo_create (struct file *file);
void         dmx_fileinfo_delete (DMXFileInfo *fi);
DMXFileInfo *dmx_fileinfo_by_index (int idx);

#endif
