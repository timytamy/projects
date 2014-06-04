/*======================================================================

    A driver for PCMCIA digimedia dmx512 devices

    digimedia_cs.c

    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is David A. Hinds
    <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
    are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

    Alternatively, the contents of this file may be used under the
    terms of the GNU General Public License version 2 (the "GPL"), in which
    case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.

======================================================================*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <asm/io.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ciscode.h>
#include <pcmcia/cisreg.h>
#include <pcmcia/ds.h>

#include <dmx/dmxdev.h>

#include "dgm_cs.h"

#define PCMCIA_DEBUG (10)

MODULE_AUTHOR("(c) Michael Stickel <michael@cubic.org> http://llg.cubic.org");
MODULE_DESCRIPTION("Driver for the Digimedia PCMCIA interface version " DMXVERSION);
MODULE_LICENSE("Dual MPL/GPL");

/*
#ifdef PCMCIA_DEBUG
static int pc_debug=PCMCIA_DEBUG;
module_param(pc_debug, int, S_IRUGO);
#define DO_DEBUG(n, args...) { if (pc_debug>(n)) args; }
#else
#define DO_DEBUG(n, args...)
#endif
*/

static int debug_flags=7;   /* debug messages trigger */
MODULE_PARM_DESC(debug_flags,    "Set trigger for debugging messages (see dgm.h)");
module_param(debug_flags, int, S_IRUGO);

/*====================================================================*/

static dev_link_t *dev_list = NULL;

static dev_info_t dev_info = "digimedia_cs";

static void digimedia_config(dev_link_t *link);
static void digimedia_release(dev_link_t *link);
static int  digimedia_event(event_t event, int priority, event_callback_args_t *args);

static dev_link_t *digimedia_attach(void);
static void        digimedia_detach(dev_link_t *);

static struct pcmcia_device_id dgm_cs_id_table[] __devinitdata =
{
  PCMCIA_DEVICE_PROD_ID1234(DGM_CS_PRODID_1, DGM_CS_PRODID_2, DGM_CS_PRODID_3, DGM_CS_PRODID_4, 0x2b7b10fa,0x7dead2eb,0x622ed193,0x28e6efc6),
  PCMCIA_DEVICE_NULL, /* 0 terminated list */
};

/*====================================================================*/

/*--------------------------------------------------------
 *-- digimedia_attach() creates an "instance" of the driver,
 *-- allocating local data structures for one device.
 *-- The device is registered with Card Services.
 *-------------------------------------------------------*/
static dev_link_t *digimedia_attach(void)
{
  dev_link_t   *link;
  client_reg_t  client_reg;
  int           ret;

  DO_DEBUG(DGM_DEBUG_PCMCIA, printk(DGM_DEBUG "dgm_cs : digimedia_attach() {\n"));

  /* Create new digimedia device */
  link = (dev_link_t *)kmalloc(sizeof(dev_link_t), GFP_KERNEL);
  if (link == NULL) return NULL;

  memset(link,0,sizeof(dev_link_t));

  link->conf.Attributes = 0;
  link->conf.Vcc        = 50;
  link->conf.IntType    = INT_MEMORY_AND_IO;
  link->priv            = NULL;

  link->next = dev_list;
  dev_list = link;

  /* Register with Card Services */
  client_reg.dev_info   = &dev_info;
  client_reg.Attributes = INFO_IO_CLIENT | INFO_CARD_SHARE; /* UNSUED as of kernel 2.6.14.3 */
  client_reg.EventMask  = CS_EVENT_REGISTRATION_COMPLETE |
    CS_EVENT_CARD_INSERTION | CS_EVENT_CARD_REMOVAL |
    CS_EVENT_RESET_PHYSICAL | CS_EVENT_CARD_RESET |
    CS_EVENT_PM_SUSPEND | CS_EVENT_PM_RESUME;
  client_reg.event_handler = &digimedia_event;
  client_reg.event_callback_args.client_data = link;
  client_reg.Version = 0x0210;

  ret = pcmcia_register_client(&link->handle, &client_reg);
  if (ret != CS_SUCCESS)
  {
    cs_error(link->handle, RegisterClient, ret);
    digimedia_detach(link);
    return NULL;
  }
  return link;
} /* digimedia_attach */

/*--------------------------------------------------------
 *-- This deletes a driver "instance".  The device is de-registered
 *-- with Card Services.  If it has been released, all local data
 *-- structures are freed.  Otherwise, the structures will be freed
 *-- when the device is released.
 *-------------------------------------------------------*/
static void digimedia_detach(dev_link_t *link)
{
  dev_link_t **linkp;
  int ret;

  DO_DEBUG(DGM_DEBUG_PCMCIA, printk(DGM_DEBUG "dgm_cs : digimedia_detach(0x%p)\n", link));
  for (linkp = &dev_list; *linkp; linkp = &(*linkp)->next)
    if (*linkp == link) break;
  if (*linkp == NULL)
    return;

  if (link->state & DEV_CONFIG)
    digimedia_release(link);

  if (link->handle)
  {
    ret = pcmcia_deregister_client(link->handle);
    if (ret != CS_SUCCESS)
      cs_error(link->handle, DeregisterClient, ret);
  }

  /* Unlink device structure, free bits */
  *linkp = link->next;
  kfree(link);
} /* digimedia_detach */

/*================[ Digimedia specific stuff ]========================*/

/*--------------------------------------------------------
 *--
 *-- This get things ready to call dgm_create_board
 *--
 *-------------------------------------------------------*/
static int digimedia_create_board(dev_link_t *link, unsigned long base, size_t size)
{
  int ret;
  dgm_board_t *brd = NULL;
  dgm_memory_t *dgm_mem = NULL;

  if (!link)
  {
    printk(__FILE__ ":unable to find dmnx-driver digimedia/digimedia\n");
    return -1;
  }

  /* get the board structure */
  brd = (dgm_board_t *)kmalloc(sizeof(dgm_board_t), GFP_KERNEL);
  if (brd == NULL)
  {
    DO_DEBUG(DGM_DEBUG_WARN_2, printk(DGM_WARN "dgm_cs : unable to allocate board structure\n"));
    return -ENOMEM;
  }

  /* fill some fields */
  brd->dev         = (void*)link;
  brd->type        = 2512;
  brd->if_count    = 1;

  /* remap memory */
  if (!(base && (size>=1024) && (size<=4096)))
  {
    DO_DEBUG(DGM_DEBUG_WARN_2, printk(DGM_WARN "dgm_cs : memory parameter NOT OK\n"));
    kfree(brd);
    return -1;
  }
  dgm_mem = (dgm_memory_t *)ioremap(base+DGM_CS_MEMORY_OFFSET, size);
  if (!dgm_mem)
  {
    DO_DEBUG(DGM_DEBUG_WARN_2, printk(DGM_WARN "dgm_cs : unable to remap I/O Memory\n"));
    kfree(brd);
    return -ENOMEM;
  }
  if (memcmp("DMXOEM",dgm_mem+0x01,6)!=0)
  {
    DO_DEBUG(DGM_DEBUG_WARN_2, printk(DGM_WARN "dgm_cs : remaped I/O Memory NOT signed\n"));
    iounmap(dgm_mem);
    kfree(brd);
    return -1;
  }
  DO_DEBUG(DGM_DEBUG_INFO_1,printk(DGM_INFO "dgm_cs : successfully remaped signed memory\n"));
  brd->mem_addr[0] = dgm_mem;

  ret = dgm_create_board(brd);
  if (ret)
  {
    DO_DEBUG(DGM_DEBUG_WARN_2, printk(DGM_WARN "dgm_cs : error at board creation by 'dgm'\n"));
    iounmap(dgm_mem);
    kfree(brd);
    return -1;
  }

  /* link board to link */
  link->priv = (void *)brd;

  return 0; /* anything succeded */
}

#define CS_CHECK(fn, ret) \
do { last_fn = (fn); if ((last_ret = (ret)) != 0) goto cs_failed; } while (0)

/*--------------------------------------------------------
 *-- digimedia_config() is scheduled to run after a
 *-- CARD_INSERTION event is received, to configure
 *-- the PCMCIA socket, and to make the dmx interface
 *-- available to the system.
 *-------------------------------------------------------*/
void digimedia_config(dev_link_t *link)
{
  client_handle_t  handle = link->handle;

  unsigned long    iomem_base=0;
  size_t           iomem_size=0;

  tuple_t          tuple;
  cisparse_t       parse;
  u_char           buf[255];
  config_info_t    conf;
  int              last_fn, last_ret;

  win_req_t        req;
  memreq_t         mem;

  DO_DEBUG(DGM_DEBUG_PCMCIA, printk(DGM_DEBUG "dgm_cs : void digimedia_config(dev_link_t *link=%p)\n", link));

  if (!link) return;

  /*
    This reads the card's CONFIG tuple to find its configuration registers.
  */

  tuple.Attributes   = 0;
  tuple.DesiredTuple = CISTPL_CONFIG;
  tuple.TupleData    = buf;
  tuple.TupleDataMax = sizeof(buf);
  tuple.TupleOffset  = 0;
  CS_CHECK(GetFirstTuple, pcmcia_get_first_tuple(handle, &tuple));
  CS_CHECK(GetTupleData,  pcmcia_get_tuple_data(handle, &tuple));
  CS_CHECK(ParseTuple,    pcmcia_parse_tuple(handle, &tuple, &parse));
  link->conf.ConfigBase = parse.config.base;
  link->conf.Present    = parse.config.rmask[0];

  /* Configure card */
  link->state |= DEV_CONFIG;

  /* Look up the current Vcc */
  CS_CHECK(GetConfigurationInfo, pcmcia_get_configuration_info(handle, &conf));
  link->conf.Vcc = conf.Vcc;
  printk("Vcc=%d.%dV\n", conf.Vcc/10, conf.Vcc%10);

  CS_CHECK(RequestConfiguration, pcmcia_request_configuration(handle, &link->conf));

  /* allocate a memory window */
  req.Attributes = WIN_DATA_WIDTH_8 | WIN_MEMORY_TYPE_CM | WIN_ENABLE;
  req.Base = req.Size = 0;
  req.AccessSpeed = 0;
  if (pcmcia_request_window(&link->handle, &req, &link->win) == CS_SUCCESS)
  {
    mem.CardOffset = 0;
    mem.Page = 0;
    mem.CardOffset = link->conf.ConfigBase;
    DO_DEBUG(DGM_DEBUG_PCMCIA, printk (DGM_DEBUG "dgm_cs : req.Base=%lX  req.Size=%d  mem.CardOffset=%X\n", req.Base, req.Size, mem.CardOffset));
    if (pcmcia_map_mem_page(link->win, &mem) == CS_SUCCESS)
    {
      link->conf.ConfigIndex = 0;
      link->state &= ~DEV_CONFIG_PENDING;
      iomem_base = req.Base;
      iomem_size = req.Size;

      DO_DEBUG(DGM_DEBUG_PCMCIA, printk (DGM_DEBUG "dgm_cs : after MapMemPage: req.Base=%lX  req.Size=%d  mem.CardOffset=%X\n", req.Base, req.Size, mem.CardOffset));
    }
  }

  if (iomem_size)
  {
    if (!digimedia_create_board(link, iomem_base, iomem_size))
      return;
  }
  if (link->state & DEV_CONFIG)
    digimedia_release(link);
  return;

 cs_failed:
  cs_error(link->handle, last_fn, last_ret);
  if (link->state & DEV_CONFIG)
    digimedia_release(link);
}

/*--------------------------------------------------------
 *-- After a card is removed, digimedia_release()
 *-- will unregister the device, and release
 *-- the PCMCIA configuration.
 *-------------------------------------------------------*/
void digimedia_release(dev_link_t *link)
{
  dgm_board_t *brd  = NULL;
  int ret;

  DO_DEBUG(DGM_DEBUG_PCMCIA, printk (DGM_DEBUG "dgm_cs : digimedia_release(%p)\n", link));

  if (link == NULL) return;

  brd = (dgm_board_t *)link->priv;

  DO_DEBUG(DGM_DEBUG_INFO_2, printk(DGM_INFO "dgm_cs : remove board with minor %d\n",brd->minor));

  dgm_delete_board(brd);

  /* release memory mapping */
  if (brd->mem_addr[0])
    iounmap(brd->mem_addr[0]);

  /* unlink and free */
  link->priv = NULL;
  kfree(brd);
//  link->dev = NULL;

  DO_DEBUG(DGM_DEBUG_PCMCIA, printk (DGM_DEBUG "dgm_cs : link->dev = NULL;\n"));

  ret = pcmcia_release_window(link->win);
  if (ret != CS_SUCCESS)
    cs_error(link->handle, ReleaseWindow, ret);
  pcmcia_release_configuration(link->handle);

  link->state &= ~DEV_CONFIG;
  DO_DEBUG(DGM_DEBUG_PCMCIA, printk (DGM_DEBUG "dgm_cs : link->state &= ~DEV_CONFIG;\n"));
} /* digimedia_release */

/*--------------------------------------------------------
 *--
 *-- The card status event handler.  Mostly, this schedules other
 *-- stuff to run after an event is received.  A CARD_REMOVAL event
 *-- also sets some flags to discourage the dmx interface from
 *-- talking to the ports.
 *--
 *-------------------------------------------------------*/
static int digimedia_event(event_t event, int priority, event_callback_args_t *args)
{
  dev_link_t *link = args->client_data;

  DO_DEBUG(DGM_DEBUG_PCMCIA, printk(DGM_DEBUG "dgm_cs : digimedia_event : digimedia_event(0x%06x)\n", event));

  switch (event)
  {
    case CS_EVENT_CARD_REMOVAL:
      printk (KERN_INFO ":" __FILE__": card has been REMOVED\n");
      digimedia_release(link);
      break;

    case CS_EVENT_CARD_INSERTION:
      printk (KERN_INFO ":" __FILE__": card has been INSERTED\n");
      link->state |= DEV_PRESENT | DEV_CONFIG_PENDING;
      digimedia_config(link);
      printk (KERN_INFO ":" __FILE__ ": card insertion finished\n");
      break;

    case CS_EVENT_PM_SUSPEND:
      printk (KERN_INFO ":" __FILE__": card should SUSPEND\n");
      link->state |= DEV_SUSPEND;
      /* Fall through... */

    case CS_EVENT_RESET_PHYSICAL:
      printk (KERN_INFO ":" __FILE__": card should do a HARD-RESET\n");
      if (link->state & DEV_CONFIG)
      {
        // TODO check that dgm will survive
        pcmcia_release_configuration(link->handle);
      }
      printk (KERN_INFO ":" __FILE__": HARD-RESET finished\n");
      break;

    case CS_EVENT_PM_RESUME:
      printk (KERN_INFO ":" __FILE__": card should RESUME\n");
      link->state &= ~DEV_SUSPEND;
      /* Fall through... */

    case CS_EVENT_CARD_RESET:
      printk (KERN_INFO ":" __FILE__": card should do a SOFT-RESET\n");
      if (DEV_OK(link))
        pcmcia_request_configuration(link->handle, &link->conf);
      printk (KERN_INFO ":" __FILE__": SOFT-RESET finished\n");
      break;

    case CS_EVENT_REGISTRATION_COMPLETE:
      printk (KERN_INFO ":" __FILE__": REGISTRATION_COMPLETE\n");
      break;

    default:
      printk (KERN_INFO ":" __FILE__": UNKNOWN EVENT %d\n", event);
      break;
  }
  DO_DEBUG(DGM_DEBUG_PCMCIA, printk(DGM_DEBUG "dgm_cs : digimedia_event : digimedia_event(0x%06x): AT-END\n", event));
  return 0;
} /* digimedia_event */

MODULE_DEVICE_TABLE(pcmcia,dgm_cs_id_table);

static struct pcmcia_driver dgm_cs_pcmcia_driver = {
  attach:   digimedia_attach,
  event:    digimedia_event,
  detach:   digimedia_detach,
  owner:    THIS_MODULE,
  id_table: dgm_cs_id_table,
  drv:      {NULL,},
};

/*--------------------------------------------------------
 *-- init_digimedia_cs
 *--
 *-- This function is called after the module has loaded.
 *--
 *-------------------------------------------------------*/
static int __init init_digimedia_cs(void)
{
  DO_DEBUG(DGM_DEBUG_PCMCIA, printk(DGM_DEBUG "dgm_cs : static int __init init_digimedia_cs(void)\n"));
  return pcmcia_register_driver(&dgm_cs_pcmcia_driver);
}

/*--------------------------------------------------------
 *--
 *-- exit_digimedia_cs
 *--
 *-------------------------------------------------------*/
static void __exit exit_digimedia_cs(void)
{
  DO_DEBUG(DGM_DEBUG_PCMCIA, printk(DGM_DEBUG "dgm_cs : static void __exit exit_digimedia_cs(void)\n"));
  pcmcia_unregister_driver(&dgm_cs_pcmcia_driver);
  BUG_ON(dev_list!=NULL);
}

#ifdef MODULE
module_init(init_digimedia_cs);
module_exit(exit_digimedia_cs);
#endif
