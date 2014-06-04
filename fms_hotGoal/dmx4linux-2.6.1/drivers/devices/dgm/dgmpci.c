/*
 * dgmpci.c : driver for Digimedia - Soundlight 1514 & 2514 DMX PCI boards
 *
 * Copyright (C) 2004 Bastien Andres <bastos@balelec.ch>
 *
 * Based on code from :
 *
 * Generic PCI Driver :
 *
 * Julien Gaulmin <julien.gaulmin@fr.alcove.com>, Alcôve
 * Pierre Ficheux (pierre@ficheux.com)
 *
 * DMX4Linux part adapted from digimedia_cs driver from :
 *
 * (c) 2001 Michael Stickel <michael@cubic.org> http://llg.cubic.org
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA or look at http://www.gnu.org/copyleft/gpl.html
 */

#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

/*
 * Includes
 */

#include <linux/kernel.h>           /* printk() */
#include <linux/module.h>           /* modules */
#include <linux/config.h>
#include <linux/init.h>             /* module_{init,exit}() */
/*#include <linux/devfs_fs_kernel.h> */ /* devfs support */
#include <linux/slab.h>             /* kmalloc()/kfree() */
#include <linux/pci.h>              /* pci_module_init() */
#include <linux/list.h>             /* list_*() */
#include <linux/interrupt.h>
#include <asm/uaccess.h>            /* copy_{from,to}_user() */
#include <linux/fs.h>               /* everything... */
#include <linux/errno.h>            /* error codes */
#include <linux/types.h>            /* size_t */
#include <asm/page.h>

//#include <linux/tqueue.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>

#include "dgmpci.h"

MODULE_DESCRIPTION("DMX4Linux driver for DGM - SLH 2512-1514 boards (dgmpci)");
MODULE_AUTHOR("Bastien Andres");
MODULE_LICENSE("GPL");

static int debug_flags=7;   /* debug messages trigger */
MODULE_PARM_DESC(debug_flags,    "Set trigger for debugging messages (see dgm.h)");
module_param(debug_flags, int, S_IRUGO);

/*
 * Supported devices
 */

static struct {char *name;} dgmpci_board_info[] __devinitdata =
{
  {DGMPCI_BOARD_INFO_0},
  {DGMPCI_BOARD_INFO_1},
  {0,}
};

static struct pci_device_id dgmpci_id_table[] __devinitdata =
{
  {DGMPCI_VENDOR_ID_0, DGMPCI_DEVICE_ID_0, PCI_ANY_ID, PCI_ANY_ID, 0, 0, DGMPCI_BOARD_TYPE_0},
  {DGMPCI_VENDOR_ID_1, DGMPCI_DEVICE_ID_1, PCI_ANY_ID, PCI_ANY_ID, 0, 0, DGMPCI_BOARD_TYPE_1},
  {0,} /* 0 terminated list */
};

MODULE_DEVICE_TABLE(pci, dgmpci_id_table);

/*
 * PCI handling
 */

/* PROBE */

DGM_STATIC int __devinit dgmpci_probe(struct pci_dev *dev, const struct pci_device_id *devid)
{
  int i,j;
  int ret = 0;

  int bar_if[2];
  u32 bar_offset[2];
  int bar_count;
  u32 bar_start;
  u32 bar_stop;
  u32 bar_len;
  u32 bar_flags;

  dgm_board_t *brd    = NULL;
  unsigned char *base          = NULL;

  DO_DEBUG(DGM_DEBUG_INFO_1, printk(DGM_INFO "dgmpci: found %s\n", dgmpci_board_info[devid->driver_data].name));

  /* Allocate a private structure and reference it as driver's data */
  brd = (dgm_board_t *)kmalloc(sizeof(dgm_board_t), GFP_KERNEL);
  if (brd == NULL)
  {
    DO_DEBUG(DGM_DEBUG_WARN_2, printk(DGM_WARN "dgmpci: unable to allocate board structure\n"));

    ret = -ENOMEM;
    goto cleanup_kmalloc;
  }

  pci_set_drvdata(dev, brd);

  /* Init private field */

  brd->dev         = (void*)dev;
  brd->type        = devid->driver_data;
  brd->if_count    = (brd->type == 1)?2:1;

  /* Initialize device before it's used by the driver */
  ret = pci_enable_device(dev);
  if (ret < 0)
  {
    DO_DEBUG(DGM_DEBUG_WARN_2, printk(DGM_WARN "dgmpci: unable to initialize PCI device\n"));
    goto cleanup_pci_enable;
  }

  /* Reserve PCI I/O and memory resources */
  ret = pci_request_regions(dev, "dgmpci");
  if (ret < 0)
  {
    DO_DEBUG(DGM_DEBUG_WARN_2, printk(DGM_WARN "dgmpci: unable to reserve PCI resources\n"));
    goto cleanup_regions;
  }

  /* Check number of bar to get */
  switch(brd->type)
  {
    case 0: /* 1514PCI */
      bar_count     = 1;
      bar_if[0]     = DGMPCI_MEMORY_REGION_0;
      bar_offset[0] = DGMPCI_MEMORY_OFFSET_0;
      break;
    case 1: /* 2514PCI */
      bar_count     = (DGMPCI_MEMORY_REGION_10 != DGMPCI_MEMORY_REGION_11) ? 2 : 1;
      bar_if[0]     = DGMPCI_MEMORY_REGION_10;
      bar_if[1]     = DGMPCI_MEMORY_REGION_11;
      bar_offset[0] = DGMPCI_MEMORY_OFFSET_10;
      bar_offset[1] = DGMPCI_MEMORY_OFFSET_11;
      break;
    default:
      bar_count = 1;
  }

  brd->bar_count = bar_count;

  /* Inspect PCI BARs and remap I/O memory */
  for (i=0; i < bar_count; i++)
  {
    bar_start = (u32) pci_resource_start(dev, bar_if[i]);
    bar_stop  = (u32) pci_resource_end(dev, bar_if[i]);
    bar_len   = (u32) pci_resource_len(dev, bar_if[i]);
    bar_flags = (u32) pci_resource_flags(dev, bar_if[i]);

    if (pci_resource_start(dev, i) != 0)
    {
      DO_DEBUG(DGM_DEBUG_INFO_2, printk(DGM_INFO "dgmpci: BAR %d (%#08x-%#08x), len=%d, flags=%#08x\n",
             bar_if[i],
             bar_start,
             bar_stop,
             bar_len,
             bar_flags));
    }
    if (bar_flags & IORESOURCE_MEM)
    {
      /* MEM > remap */
      brd->bar_addr[i] = ioremap(bar_start,bar_len);
      if (brd->bar_addr[i] == NULL)
      {
        DO_DEBUG(DGM_DEBUG_WARN_2, printk(DGM_WARN "dgmpci: unable to remap I/O memory\n"));
        ret = -ENOMEM;
        goto cleanup_ioremap;
      }
      DO_DEBUG(DGM_DEBUG_INFO_2,printk(DGM_INFO "dgmpci: BAR %d I/O memory has been remaped at %#08x\n",bar_if[i], (u32)brd->bar_addr[i]));
    }
    else
    {
      DO_DEBUG(DGM_DEBUG_WARN_2,printk(DGM_WARN "dgmpci: BAR %d is I/O ports\n",bar_if[i]));
      goto cleanup_ioremap;
    }
  }

  /* be sure that i is ok for evt. cleanup */
  i = bar_count;
  /* duplicate address if two interfaces on same bar */
  if (bar_count == 1) brd->bar_addr[1] = brd->bar_addr[0];

  /* check for signature and fill pointer fields */
  for (j = 0; j < brd->if_count; j++)
  {
    base = brd->bar_addr[j]+bar_offset[j];
    if (memcmp("DMXOEM",base+0x01,6)==0)
    {
      DO_DEBUG(DGM_DEBUG_INFO_1,printk(DGM_INFO "dgmpci:  region %d offset %#08x is signed 'DMXOEM'\n",bar_if[j],bar_offset[j]));
      brd->mem_addr[j] = base;
    }
    else
    {
      DO_DEBUG(DGM_DEBUG_WARN_2,printk(DGM_WARN "dgmpci: BAR %d offset %#08x is NOT signed 'DMXOEM'\n",bar_if[j],bar_offset[j]));
      goto cleanup_ioremap;
    }
  }

  /* call dgm_create_board to do the dgm generic job */

  ret = dgm_create_board(brd);
  if (ret)
  {
    DO_DEBUG(DGM_DEBUG_WARN_2, printk(DGM_WARN "dgmpci: error at board creation by 'dgm'\n"));
    goto cleanup_ioremap;
  }
  return 0;

 cleanup_ioremap:
  for (i--; i >= 0; i--)
  {
    if (brd->bar_addr[i] != NULL)
      iounmap(brd->bar_addr[i]);
  }
  pci_release_regions(dev);
 cleanup_regions:
  pci_disable_device(dev);
 cleanup_pci_enable:
  kfree(brd);
 cleanup_kmalloc:
  return ret;
}

/* REMOVE */

DGM_STATIC void __devexit dgmpci_remove(struct pci_dev *dev)
{
  int i;
  dgm_board_t *brd = pci_get_drvdata(dev);

  DO_DEBUG(DGM_DEBUG_INFO_2, printk(DGM_INFO "dgmpci: remove board with minor %d\n",brd->minor));

  dgm_delete_board(brd);

  for (i=0; i < brd->bar_count; i++)
    if (brd->bar_addr[i] != NULL)
      iounmap(brd->bar_addr[i]);

  pci_release_regions(dev);

  pci_disable_device(dev);

  kfree(brd);
}

/* PCI_DRIVER */

static struct pci_driver dgmpci_pci_driver = {
  name:     "dgmpci",
  id_table:  dgmpci_id_table,
  probe:     dgmpci_probe,    /* Init one device */
  remove:    dgmpci_remove,   /* Remove one device */
};

/*
 * Init and Exit
 */

DGM_STATIC int __init dgmpci_init(void)
{
  int ret;

  /* Register PCI driver */

  ret = pci_module_init(&dgmpci_pci_driver);
  if (ret < 0)
  {
    DO_DEBUG(DGM_DEBUG_WARN_2, printk(DGM_WARN "dgmpci: unable to register PCI driver\n"));
    return ret;
  }

  return 0;
}

DGM_STATIC void __exit dgmpci_exit(void)
{
  DO_DEBUG(DGM_DEBUG_INFO_1, printk(DGM_INFO "dgmpci: module remove !\n"));
  pci_unregister_driver(&dgmpci_pci_driver);
}

module_init(dgmpci_init);
module_exit(dgmpci_exit);
