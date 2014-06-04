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


#include <dmxdev/dmxdevP.h>
#include <dmx/dmxioctl.h>

#include <linux/module.h>

#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/fs.h>

#ifndef UTS_RELEASE
#include <linux/utsrelease.h>
#endif

MODULE_AUTHOR("Michael Stickel, michael@cubic.org, http://llg.cubic.org/");
MODULE_DESCRIPTION("DMX4Linux " DMXVERSION " management module. Abstraction layer to filesystem. (C) Michael Stickel");
MODULE_LICENSE("GPL");

#undef DEBUG

#ifdef DEBUG
#define DO_DEBUG(x...)  (x)
#else
#define DO_DEBUG(x...)
#endif

static unsigned char dmxbuffer[MAX_UNIVERSES*512];


/*
 * dmx_data_available
 *
 * returns 1 if data has changed since the last read access.
 */
static int  dmx_data_available (struct _DMXFileInfo *fi, uint start, uint size)
{
  if (size && start > sizeof(dmxbuffer)) /* eigentlich hier nicht notwendig */
	return -EINVAL;		/* out of range */
  if (fi->changed>0) return 1;
  return 0;
}

/*
 * dmxout_to_user
 *
 * copies the data from a dmx output universe to the userspace.
 */
static size_t dmxout_to_user (struct _DMXFileInfo *fi, unsigned char *buff, off_t offs, size_t size)
{
  if (offs > sizeof(dmxbuffer))
    return -EINVAL;

  if (offs+size > sizeof(dmxbuffer))
    size = sizeof(dmxbuffer) - offs;

  if(copy_to_user (buff, &dmxbuffer[offs], size))
    return 0;

  return size;
}


/*
 * dmxout_to_user
 *
 * copies the data from a dmx input universe to the userspace.
 */
static size_t dmxin_to_user (struct _DMXFileInfo *fi, unsigned char *buff, off_t offs, size_t size)
{
  int consumed=0;
  while(consumed<size)
    {
      DMXUniverse *inu;
      const int universum=offs/512;
      const int universum_start=offs%512;
      int len=512-universum_start;
      if(consumed+len > size)
	len=size-consumed;

      inu = dmx_universe_by_index(1, universum);
      if (inu && inu->read_slots)
	{
	  int readlen;
	  unsigned char tbuff[512];
          memset(tbuff, 0, sizeof(tbuff));

	  readlen = inu->read_slots (inu, universum_start, tbuff, len);
          if (readlen > 0 && readlen<=len)
	    if(copy_to_user(buff, tbuff, readlen))
	      return 0;
	}
      buff+=len;
      consumed+=len;
      offs+=len;
    }
  return size;
}

/*
 *  Write to an output universe
 */
static void dmx_write_to_universe (loff_t start, size_t size)
{
  unsigned  char *buf=dmxbuffer+start;
  const size_t end = start + size;
  while(start<end)
    {
      const int universe=start/512;
      const int s=start%512;
      const int len=512-s;
      DMXUniverse *uni = dmx_universe_by_index (0, universe);
      if(uni && uni->write_slots)
	uni->write_slots (uni, s, buf, len);
      start+=len;
      buf+=len;
    }
}

/*-----------------------------------------------------------------
 * name    : DMXDev_read
 * function: This function is called by the read-system-call
 */
static ssize_t DMXDev_read (struct file *file, char *buf, size_t size, loff_t *pos)
{
  int minor = FILE_MINOR (file);

  if (!file)   return -EINVAL;
  if (!buf)    return -EINVAL;
  if (size<=0) return -EINVAL;

  if (IS_MINOR_DMXOUT(minor))
    {
      DMXFileInfo *info = (DMXFileInfo *)file->private_data;
      if (file->f_pos+size > sizeof(dmxbuffer))
	size = sizeof(dmxbuffer) - file->f_pos;
      if (info)
	{
          int n;

          if (info->block)
            {
	      int stat = info->data_available(info, file->f_pos, size);
	      if (stat<0)
                {
                  printk (KERN_INFO "dmxdev.read:/dev/dmx/:error while data_available\n");
                  return stat;
                }
	      if (!stat)
	        {
	          info->flags |= DMXFI_F_REQUEST;
	          info->request.start = file->f_pos;
	          info->request.size = size;
	          interruptible_sleep_on (&info->read_wait_queue);
	          info->flags &= ~DMXFI_F_REQUEST;
	        }
            }
	  info->changed = 0;
	  n = copy_to_user (buf, &dmxbuffer[file->f_pos], size);
          if (n>0)
            file->f_pos += size;
   	  return size;
	}
    }

  /* read dmx-in is the same as read dmx-mod with exception
   * of the data that is read. dmx-in comes from input universes
   * but dmx-mon comes from userspace (data written to outputs.
   * data_available and copy_to_user are different for dmx-in and dmx-mon.
   */
  else if (IS_MINOR_DMXIN(minor))
    {
      DMXFileInfo *info = (DMXFileInfo *)file->private_data;
      if (info)
	{
          int n=0;
          if (info->block)
            {
	      int stat = info->data_available(info, file->f_pos, size);
	      if (stat<0)
                {
                  printk (KERN_INFO "dmxdev.read:/dev/dmxmon,/dev/dmxout: error while data_available\n");
                  return stat;
                }
	      if (!stat)
	        {
	          info->flags |= DMXFI_F_REQUEST;
	          info->request.start = file->f_pos;
	          info->request.size = size;
	          interruptible_sleep_on (&info->read_wait_queue);
	          info->flags &= ~DMXFI_F_REQUEST;
	        }
            }
	  info->changed = 0;
	  n = info->to_user (info, buf, file->f_pos, size);
          if (IS_MINOR_DMXIN(minor) && n>0)
            file->f_pos += n;
          return n;
	}
    }
  return -EINVAL;
}


/*
 * Signals a process that data is available.
 * All process that have requested a select and
 * all processes that have requested a read within
 * the area that has changed are signaled.
 *
 * <u> is a refference to the universe that has changed.
 * if size is 0 all processes that have a pending read
 * will be signalled.
 * !!!!! HAS TO BE MOVED ELSEWHERE !!!!!
 */
int  dmx_universe_signal_changed (DMXUniverse *u, uint start, uint size)
{
  int i;
  DMXFileInfo *info = NULL;

  DO_DEBUG(printk (KERN_INFO "dmx_universe_signal_changed(%p, %u, %u)\n", u, start, size));

  for (i=0; (info = dmx_fileinfo_by_index(i)); i++)
    {
      if (info->target==DMXFI_DMXIN)
	{
	  if (info->flags & DMXFI_F_POLL)
	    {
	      DO_DEBUG(printk (KERN_INFO "signal_changed: DMX_FI_POLL:info %p signaled\n", info));
	      info->flags &= ~DMXFI_F_POLL;
	      wake_up (&info->read_wait_queue);
	    }

	  if (info->flags & DMXFI_F_REQUEST)
	    {
	      if (!size || (start <= info->request.start+info->request.size &&
			    start+size >= info->request.start))
		{
		  DO_DEBUG(printk (KERN_INFO "signal_changed: DMXFI_F_REQUEST:info %p signaled\n", info));
		  wake_up (&info->read_wait_queue);
		}
	    }
	  info->changed = 1;
	}
      else if (info->target==DMXFI_DMXOUT)
        {
	  info->changed = 1;

          if (info->async_queue)
	    {
	      struct fasync_struct *p = (struct fasync_struct *)info->async_queue;
	      KILL_FASYNC(p, SIGIO, POLL_IN);
	    }
        }
    }
  return 0;
}




/*--------------------------------------------------------------
 * name    : DMXDev_write
 * function: This function is called by the write-system-call
 */
static ssize_t DMXDev_write (struct file *file, const char *buf, size_t size, loff_t *pos)
{
  int minor = FILE_MINOR (file);

  DO_DEBUG(printk (KERN_INFO "DMXDev_write()\n"));

  if (IS_MINOR_DMXOUT(minor))
    {
      int i;
      DMXFileInfo *info = NULL;

      if (file->f_pos+size > sizeof(dmxbuffer))
	size = sizeof(dmxbuffer) - file->f_pos;

      if(copy_from_user (&dmxbuffer[file->f_pos], buf, size))
	return 0;

          DO_DEBUG(printk (KERN_INFO "DMXDev_write: about to call dmx_write_to_universe\n"));
	  dmx_write_to_universe (file->f_pos, size);

	  /*
	   * wake up all universes which are waiting for
	   * data to be written to the output.
	   */
	  for (i=0; (info = dmx_fileinfo_by_index(i)); i++)
	    {
	      if (info->target==DMXFI_DMXOUT)
		{
		  if (info->flags & DMXFI_F_POLL)
		    {
		      info->flags &= ~DMXFI_F_POLL;
		      wake_up (&info->read_wait_queue);
		    }

		  if (info->flags & DMXFI_F_REQUEST)
		    {
		      if (file->f_pos <= info->request.start+info->request.size &&
			  file->f_pos+size >= info->request.start)
			wake_up (&info->read_wait_queue);
		    }
		  info->changed = 1;
		}
	    }
          file->f_pos += size;
   	  return size;
    }

  else if (IS_MINOR_DMXIN(minor))
    {
      DO_DEBUG(printk (KERN_INFO "I am not able to write to DMX-IN\n"));
      file->f_pos += size;
      return size;
    }

  else
    {
      DO_DEBUG(printk (KERN_INFO "dmxdev::write request for unused minor %d\n", minor));
      file->f_pos += size;
      return size;
    }
  return -EINVAL;
}




/*--------------------------------------------------------------
 * name    : DMXDev_poll
 * function:
 */
static unsigned int DMXDev_poll (struct file *file, poll_table * wait)
{
  DMXFileInfo *info = (DMXFileInfo *)file->private_data;
  unsigned int mask = 0;
  int          stat=0;

  DO_DEBUG(printk (KERN_INFO "DMXDev_poll(%p,%p)\n", file, wait));

  if (!info)
    return POLLERR;

  DO_DEBUG(printk (KERN_INFO "select on file %p with info %p\n", file, info));

  poll_wait (file, &info->read_wait_queue, wait);

  stat = info->data_available(info, 0, 0);
  if (stat<0)
    return POLLERR;

  if (stat)
    mask |= POLLIN | POLLRDNORM;
  else
    info->flags |= DMXFI_F_POLL;  /* tell writer that someone polls for data */

  /*  mask |= POLLOUT | POLLWRNORM; */
  return mask;
}



/*---------------------------------------------------------
 * name    : DMXDev_lseek
 * function: This function is calles by the lseek-system-call
*/
static long long DMXDev_lseek (struct file *file, long long offset, int orig)
{
  switch (orig)
    {
    case 0:
      file->f_pos = offset;
      return file->f_pos;
    case 1:
      file->f_pos += offset;
      return file->f_pos;
    default:
      return -EINVAL;
    }
}



/*----------------------------------------------------------
 * name    : DMXDev_ioctl
 * function: This function will later be called by the ioctl-system-call
*/
static int DMXDev_ioctl (struct inode *node, struct file *file, unsigned int cmd, unsigned long arg)
{
  /*int minor = FILE_MINOR (file);*/

  if(!arg)
    return -EINVAL;

  switch (cmd)
    {
      /*
       * Get some global information from dmx4linux as:
       * => dmx4linux version.
       * => number of max universes supported
       * => number of output-universes used.
       * => number of input-universes used.
       * => number of drivers currently loaded.
       * => names of the drivers currently loaded.
       *
       */
    case DMX_IOCTL_GET_INFO:
	{
	  struct dmx_info  info;
	  size_t           remain_size = sizeof(info.family_names);
	  char           * buffer = info.family_names;
	  DMXFamily      *f = NULL;

	  info.version_major = VERSIONMAJOR;
	  info.version_minor = VERSIONMINOR;
	  info.max_out_universes  = MAX_UNIVERSES;
	  info.max_in_universes   = MAX_UNIVERSES;
	  info.used_in_universes  = number_input_universes();
	  info.used_out_universes = number_output_universes();
	  info.num_entries = 0;
	  info.num_families = 0;

	  /*
	   * Collect all possible driver names.
	   */
	  f = dmx_get_root_family ();
	  while (f)
	    {
	      size_t namelen = f->name ? (strlen(f->name)+1) : 0;
	      if (f->name && namelen < remain_size)
		{
		  strcpy (buffer, f->name);
		  buffer += namelen;
		  remain_size -= namelen;
		  info.num_entries++;
		}
	      info.num_families++;
	      f = f->next;
	    }

	  if(copy_to_user ((void *)arg, &info, sizeof(info)))
	    return -EMSGSIZE;

	  return 0;
	}
      break;

    case DMX_IOCTL_GET_DRIVERS:
	{
	  DMXFamily *f = NULL;
	  struct dmx_family_info info;
	  size_t                 remain_size = sizeof(info.driver_names);
	  char                 * buffer      = info.driver_names;

	  if(copy_from_user (&info, (void *)arg, sizeof(info)))
	    return -EMSGSIZE;

	  info.num_entries = 0;
	  info.num_drivers = 0;

	  f = dmx_find_family (info.familyname);
	  if (f)
	    {
	      int offset = info.offset;
	      DMXDriver *d = f->drivers;

	      /*
	       * skip unwhanted names.
	       */
	      while (d && offset > 0)
		{
		  info.num_drivers++;
		  offset--;
		  d = d->next;
		}

	      while (d)
		{
		  size_t namelen = d->name ? (strlen(d->name)+1) : 0;
		  if (d->name && namelen < remain_size)
		    {
		      strcpy (buffer, d->name);
		      buffer += namelen;
		      remain_size -= namelen;
		      info.num_entries++;
		    }
		  info.num_drivers++;
		  d = d->next;
		}
	    }
	  else
	    printk("failed to find family %s\n", info.familyname);
	  if(copy_to_user ((void *)arg, &info, sizeof(info)))
	    return -EMSGSIZE;

	  return 0;
	}
      break;

      /*
       *
       *
       */
    case DMX_IOCTL_GET_CAP:
	{
	  struct dmx_capabilities  cap;
	  DMXUniverse  *uni = NULL;
	  DMXProperty  *p = NULL;

	  if(copy_from_user (&cap, (void *)arg, sizeof(cap)))
	    return -EMSGSIZE;

	  uni = dmx_universe_by_index(cap.direction, cap.universe);
	  if (uni)
	    {
	      strcpy (cap.family,    uni->interface->driver->family->name);
	      strcpy (cap.driver,    uni->interface->driver->name);
	      strcpy (cap.connector, uni->connector);
	      cap.conn_id = uni->conn_id;

	      if ((p = uni->findprop (uni, "slots")))
		{
		  long slots = 512L;
		  p->get_long (p, &slots);
		  cap.maxSlots = slots;
		}
	      else
		cap.maxSlots = 512;

	      cap.mabsize   = DMX_UNDEFINED; /* multible of 1/10 uS */
	      cap.breaksize = DMX_UNDEFINED;
	      if(copy_to_user ((void *)arg, &cap, sizeof(cap)))
		return -EMSGSIZE;
	      return 0;
	    }
	  else
	    return -EAGAIN;
	}
      break;


      /*
       * Get and set parameters.
       */
    case DMX_IOCTL_GET_PARM:
    case DMX_IOCTL_SET_PARM:
      {
	struct dmx_parameter val;
	DMXUniverse *u   = NULL;

	if(copy_from_user (&val, (void *)arg, sizeof(val)))
	  return -EMSGSIZE;

	u = dmx_universe_by_index(val.direction, val.universe);
	if (u->props)
	  {
	    DMXProperty *prop = u->props->find (u->props, val.name);
	    if (!prop && u->interface && u->interface->props)
	      prop = u->interface->props->find (u->interface->props, val.name);
	    if (prop)
	      {
		int stat=0;
		if (cmd==DMX_IOCTL_SET_PARM)
		  stat=prop->set_string(prop, val.value);
		else
		  {
		    stat=prop->get_string(prop, val.value, sizeof(val.value));
		    if(copy_to_user ((void *)arg, &val, sizeof(val)))
		      return -EMSGSIZE;
		  }
		if (stat >= 0)
		  return 0;
	      }
	  }
	return -EAGAIN;
      }
      break;


    case DMX_IOCTL_GET_PARM_NAMES:
      {
	DMXUniverse *u = NULL;
	struct dmx_parm_names  names;

	if(copy_from_user (&names, (void *)arg, sizeof(names)))
	  return -EMSGSIZE;

	names.size = 0;
	names.num_names = 0;

	u = dmx_universe_by_index(names.direction, names.universe);
	if (u)
	  {
	    size_t sizeused = 0;
	    printk("try to get names\n");

	    if (u->props && u->props->size)
	      names.size += u->props->size (u->props);

	    if (u->interface->props && u->interface->props->size)
	      names.size += u->interface->props->size (u->props);

	    names.num_names = u->props->names (u->props, names.names, sizeof(names.names), &sizeused);
	    if (names.num_names>=0)
	      names.num_names += u->interface->props->names (u->interface->props, names.names+sizeused, sizeof(names.names)-sizeused, NULL);

	    if(copy_to_user ((void *)arg, &names, sizeof(names)))
	      return -EMSGSIZE;
	    printk("copied to userspace\n");
	    return 0;
	  }
	return -EINVAL;
      }
      break;

    default:
      DO_DEBUG(printk (KERN_INFO "DMXout.ioctl currently not implemented\n"));
    }

  return -EINVAL;
}


/*-------------------------------------------------------
 * name    : DMXDev_open
 * function: This function is called by the open-system-call
*/
static int DMXDev_open (struct inode *inode, struct file *file)
{
/*  int  ret = 0; */
  int minor = FILE_MINOR (file);

  DO_DEBUG(printk (KERN_INFO "%s: open (minor=%d, inodeptr=%p,fileptr=%p)\n", MODULE_NAME, minor, inode, file));

  file->private_data = NULL;

  if (IS_MINOR_DMXOUT(minor))
    {
      DMXFileInfo *fi = dmx_fileinfo_create (file);
      if (!fi)
        {
          printk (KERN_INFO "dmxdev: Error while open: creation of fileinfo failed");
	  return -EBUSY;
        }
      printk (KERN_INFO "dmxdev: opening as DMXFI_DMXOUT\n");
      fi->target = DMXFI_DMXOUT;
      fi->data_available = dmx_data_available;
      fi->to_user = dmxout_to_user;
      fi->block = !(file->f_flags & O_NONBLOCK);
      return 0;
    }
  else if (IS_MINOR_DMXIN(minor))
    {
      DMXFileInfo *fi = dmx_fileinfo_create (file);
      if (!fi)
        {
          printk (KERN_INFO "dmxdev: Error while open: creation of fileinfo failed");
	  return -EBUSY;
        }
      printk (KERN_INFO "dmxdev: opening as DMXFI_DMXIN\n");
      fi->target = DMXFI_DMXIN;
      fi->data_available = dmx_data_available;
      fi->to_user = dmxin_to_user;
      fi->block = !(file->f_flags & O_NONBLOCK);
      return 0;
    }

  return -ENODEV;
}


/*------------------------------------------------------
 * name    : DMXDev_close
 * function: This function is called by the close-system-call.
*/
static int DMXDev_close (struct inode *inode, struct file *file)
{
  int minor = FILE_MINOR (file);

  DO_DEBUG(printk (KERN_INFO "%s: close (minor=%d)\n", MODULE_NAME, minor));

  if (IS_MINOR_DMXOUT(minor) || IS_MINOR_DMXIN(minor))
    {
      if (file->private_data)
	dmx_fileinfo_delete ((DMXFileInfo *)file->private_data);
      return 0;
    }

  return -EBUSY;
}

/*------------------------------------------------------
 * name		: DMXDev_fasync
*/
int DMXDev_fasync (int fd, struct file *filp, int mode)
{
  DMXFileInfo *info = (DMXFileInfo *)filp->private_data;
  return info?fasync_helper (fd, filp, mode, &info->async_queue):-EINVAL;
}

/* these are the file operations for the DMX-devices */
static struct file_operations dmxdev_fops =
{
	llseek:	DMXDev_lseek,
	read:	DMXDev_read,
	write:	DMXDev_write,
	poll:	DMXDev_poll,    /* no select, will be used to signal received frame */
	ioctl:	DMXDev_ioctl,	/* no ioctl, will follow for configuration */
	open:	DMXDev_open,
	release:DMXDev_close,

	fasync:	DMXDev_fasync,
};


static struct miscdevice dmxout_misc =
{
  minor:	DMXOUTMINOR,
  name:		"dmx",
  fops:		&dmxdev_fops, /* should be something else */
};

static struct miscdevice dmxin_misc =
{
  minor:	DMXINMINOR,
  name:		"dmxin",
  fops:		&dmxdev_fops, /* should be something else */
};

/*
 *  And now the modules code and kernel interface.
 */


/* This func. is called when the module is loaded */
static int __init dmx_dev_init(void)
{
  printk (KERN_INFO MODULE_NAME " " DMXVERSION " compiled for Linux " UTS_RELEASE " Copyright (c) 1998-2005 by Michael Stickel\n");

  dmxdev_fops.owner = THIS_MODULE;

  dmx_fileinfo_init ();

  printk (KERN_INFO "%s: %d universes for input / %d universes for output\n", MODULE_NAME, MAX_UNIVERSES, MAX_UNIVERSES);

  if (!misc_register (&dmxout_misc))
    {
#ifdef CONFIG_DEVFS_FS
      if(devfs_set_flags (dmxout_misc.devfs_handle, DEVFS_FL_AUTO_OWNER) < 0)
	printk(KERN_ERR MODULE_NAME ": could not set flags on %s\n", dmxout_misc.name);
#endif
      printk (KERN_INFO "%s: registration of %s succeded\n", MODULE_NAME, dmxout_misc.name);
      if (!misc_register (&dmxin_misc))
        {
#ifdef CONFIG_DEVFS_FS
	  if(devfs_set_flags (dmxin_misc.devfs_handle, DEVFS_FL_AUTO_OWNER) < 0)
	    printk(KERN_ERR MODULE_NAME ": could not set flags on %s\n", dmxout_misc.name);
#endif
          printk (KERN_INFO "%s: registration of %s succeded\n", MODULE_NAME, dmxin_misc.name);

          if (DMXProcInit () >= 0)
            return 0;
          else
            printk (KERN_INFO "%s: unable to create proc entries\n", MODULE_NAME);

          misc_deregister(&dmxin_misc);
        }
      else
        printk (KERN_INFO "%s: misc_register(%s) failed\n", MODULE_NAME, dmxin_misc.name);

      misc_deregister(&dmxout_misc);
    }
  else
    printk (KERN_INFO "%s: misc_register(%s) failed\n", MODULE_NAME, dmxout_misc.name);

  return -EBUSY;
}


/* This is called when the module should be unloaded */
static void __exit dmx_dev_exit(void)
{
  if (dmx_get_root_family ())
    printk (KERN_INFO "dmxdev: cleanup_module: There are some families\n");

  printk (KERN_INFO "%s: cleanup_module called\n", MODULE_NAME);

  DMXProcCleanup ();
  printk (KERN_INFO "%s: proc entries removed\n", MODULE_NAME);

  if (misc_deregister(&dmxin_misc))
    printk (KERN_INFO "%s: deregistration of %s failed\n", MODULE_NAME, dmxin_misc.name);
  else
    printk (KERN_INFO "%s: deregistration of %s succeded\n", MODULE_NAME, dmxin_misc.name);

  if (misc_deregister(&dmxout_misc))
    printk (KERN_INFO "%s: deregistration of %s failed\n", MODULE_NAME, dmxout_misc.name);
  else
    printk (KERN_INFO "%s: deregistration of %s succeded\n", MODULE_NAME, dmxout_misc.name);
}

module_init(dmx_dev_init);
module_exit(dmx_dev_exit);

EXPORT_SYMBOL(dmx_create_family);
EXPORT_SYMBOL(dmx_find_driver);
EXPORT_SYMBOL(dmxprop_create_long);
EXPORT_SYMBOL(dmxprop_user_long);
EXPORT_SYMBOL(dmxproplist_create);
EXPORT_SYMBOL(dmxproplist_vacreate);
