/*
 * dev.c
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

#define __NO_VERSION__
#include <linux/module.h>

#include <dmxdev/dmxdevP.h>
#include <dmx/dmxioctl.h>

#include <linux/slab.h>


static DMXFileInfo  *opendmx_info[50];


DMXFileInfo *dmx_fileinfo_by_index (int idx)
{
  return (idx>=0 && idx<50) ? opendmx_info[idx] : NULL;
}


static int    default_data_available (struct _DMXFileInfo *fi, uint start, uint size)
{
  return 1; /* by default data is available */
}
static size_t default_to_user        (struct _DMXFileInfo *fi, unsigned char *buff, off_t offs, size_t size)
{
  return -EBUSY;
}


static DMXFileInfo *dmx_fileinfo_alloc (void)
{
  DMXFileInfo *fi = DMX_ALLOC(DMXFileInfo);
  if (fi)
    {
      fi->target = DMXFI_DMXUNDEF;
      fi->flags   = 0;
      fi->changed = 1;		/* so we can initially read the universe */
      fi->request.start = 0;
      fi->request.size  = 0;
      fi->to_user        = default_to_user;
      fi->data_available = default_data_available;
      init_waitqueue_head (&fi->read_wait_queue);
      fi->async_queue = NULL;
    }
  return fi;
}

static void dmx_fileinfo_free (DMXFileInfo *fi)
{
  DMX_FREE (fi);
}

void dmx_fileinfo_init ()
{
  int  i;
  for (i=0; i<50; i++)
    opendmx_info[i] = NULL;
}


DMXFileInfo *dmx_fileinfo_create (struct file *file)
{
  int i;

  for (i=0; i<50; i++)
  {
    if (!opendmx_info[i])
      {
	DMXFileInfo *fi = opendmx_info[i] = dmx_fileinfo_alloc ();
	if (fi)
	  {
	    file->private_data = (void *)fi;
	    return fi;
	  }
      }
  }
  return NULL;
}

void dmx_fileinfo_delete (DMXFileInfo *fi)
{
  int i;

  if (fi)
    for (i=0; i<50; i++)
      if (opendmx_info[i]==fi)
	{
	  /*	  wake_up (&read_wait_queue);*/
	  opendmx_info[i] = NULL;
	  dmx_fileinfo_free (fi);
	}
}
