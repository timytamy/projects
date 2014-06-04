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

/* TODO: this driver is buggy and many pointers in the struct may be not initialized, thus leading to a null pointer exception. */

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

#include "digimedia.h"

#define PCMCIA_DEBUG (10)

MODULE_AUTHOR("(c) Michael Stickel <michael@cubic.org> http://llg.cubic.org");
MODULE_DESCRIPTION("Driver for the Digimedia PCMCIA interface version " DMXVERSION);
MODULE_LICENSE("Dual MPL/GPL");

#ifdef PCMCIA_DEBUG
static int pc_debug = PCMCIA_DEBUG;
MODULE_PARM(pc_debug, "i");
#define DO_DEBUG(n, args...) do { if (pc_debug>(n)) args; } while(0)
#else
#define DO_DEBUG(n, args...)
#endif


static char *directions[8];
MODULE_PARM(directions, "1-8i");
MODULE_PARM_DESC(directions, "directions=\"o,i,...\" o=output,i=input");


/*====================================================================*/

static dev_info_t dev_info = "digimedia_cs";

/*====================================================================*/



/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
static void dgm_cs_error (client_handle_t handle, int func, int ret)
{
    error_info_t err = { func, ret };
    CardServices(ReportError, handle, &err);
}



/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
inline unsigned short shortwritetocard (unsigned short *_x, unsigned short v)
{
  unsigned char *x = (unsigned char *)_x;
  x[0]=(v>>8)&0xff;
  x[1]=v&0xff;
  return v;
}



/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
inline unsigned short shortreadfromcard (unsigned short *_x)
{
  unsigned char *x = (unsigned char *)_x;
  return x[0]<<8 | x[1];
}

/*====================================================================*/


/* add this to the driver-structure */
static dev_link_t *dev_list = NULL;


/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
int pcmcia_add_instance (dev_link_t *link)
{
  DO_DEBUG(1, printk("int pcmcia_add_instance (dev_link_t *link=%p)\n",link));
  if (link)
    {
      link->next = dev_list;
      dev_list = link;
      return CS_SUCCESS;
    }
  return -1;
}

/*--------------------------------------------------------
 *-- After a card is removed, digimedia_release()
 *-- will unregister the device, and release
 *-- the PCMCIA configuration.
 *-------------------------------------------------------*/
void digimedia_release(u_long arg)
{
  dev_link_t *link = (dev_link_t *)arg;
  DO_DEBUG(1, printk ("digimedia_release(%p)\n", link));
  if (link)
    {
      DMXInterface *dmx_if = (DMXInterface *)link->priv;
      if (dmx_if)
	dmx_if->delete(dmx_if);

      link->dev = NULL;
      DO_DEBUG(0, printk ("link->dev = NULL;\n"));

      CardServices(ReleaseWindow, link->win);
      CardServices(ReleaseConfiguration, link->handle);

      link->state &= ~DEV_CONFIG;
      DO_DEBUG(0, printk ("link->state &= ~DEV_CONFIG;\n"));
  }
} /* digimedia_release */



/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
int pcmcia_delete_instance (dev_link_t *link)
{
  static int in_function = 0;
  printk("int pcmcia_delete_instance (dev_link_t *link=%p, depth=%d)\n", link, in_function);

  if (in_function) return -1;
  in_function++;
  if (link)
    {
      int ret;

      /* find the link */
      dev_link_t **l;
      for (l = &dev_list; *l; l = &((*l)->next))
	if (*l == link)
	  break;
      if (*l == NULL)
	{
	  printk ("no matching instance found\n");
	  in_function--;
	  return -1;
	}

      if (link->handle)
	{
	  del_timer(&link->release); /* not delaying release of instance. Deleting it now */
	  printk(" I call digimedia_release(0x%08lX)\n", (u_long)link);
	  if (link->state & DEV_CONFIG)
	    digimedia_release((u_long)link);

	  ret = CardServices(DeregisterClient, link->handle);
	  if (ret != CS_SUCCESS)
	    {
	      dgm_cs_error(link->handle, DeregisterClient, ret);
	      {
		in_function--;
		return -1;
	      }
	    }
	  else
	    {
	      /* remove the link */
	      if (l)
		*l = link->next;

	      if (link->dev)
		kfree(link->dev);
	      kfree(link);

	      in_function--;
	      return CS_SUCCESS;
	    }
	}
    }
  in_function--;
  return -1;
}



/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
static dev_link_t *pcmcia_create_link(void)
{
  dev_link_t *link = (dev_link_t *)kmalloc(sizeof(dev_link_t), GFP_KERNEL);
  if (link)
    memset(link, 0, sizeof(dev_link_t));
  return link;
}


/*======================[ Some CIS-Tuple Access Stuff ]================================*/

static DMXFamily    *family = NULL;

/*--------------------------------------------------------
 *--
 *-- This does a simple configuration of the card
 *--
 *-------------------------------------------------------*/
static int simple_config(dev_link_t *link, unsigned long base, size_t size)
{
  DMXDriver *driver = dmx_find_driver (family, "digimedia");
  if (link && driver)
    {
      DMXInterface *dmx_if = driver->create_interface(driver, dmxproplist_vacreate("base=%l,size=%l", base, size));
      if (dmx_if)
	{
          int i;
          for (i=0; i<2; i++)
            {
              int dir = -1;
              if (directions[i])
                {
                  if (strcmp(directions[i],"input")==0 ||
		      strcmp(directions[i],"i")==0)
		    dir=1;
                  else if (strcmp(directions[i],"output")==0 ||
			   strcmp(directions[i],"o")==0)
		    dir=0;
                  else dir=1;
                }
              else
                dir=0;

	      if (dmx_if->create_universe (dmx_if, dir, NULL))
	        printk(KERN_INFO ":" __FILE__ ": created universe %d as %s\n", i, dir?"input":"output");
              else
	        printk(KERN_INFO ":" __FILE__ ": failed to create universe %d as %s\n", i, dir?"input":"output");

            }
	  link->priv = (void *)dmx_if;
	  return 0; /* anything succeded */
	}
      else
	printk(__FILE__ ": unable to create digimedia-dmx-interface\n");
    }
  else
    printk(__FILE__ ":unable to find dmnx-driver digimedia/digimedia\n");
  return -1;
}


/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
static int get_tuple(int fn, client_handle_t handle, tuple_t *tuple,
		     cisparse_t *parse)
{
    int i;

    DO_DEBUG(1, printk ("static int get_tuple(int fn=%d, client_handle_t handle, tuple_t *tuple=%p, cisparse_t *parse=%p)\n", fn, tuple, parse));

    i = CardServices(fn, handle, tuple);
    if (i != CS_SUCCESS) return CS_NO_MORE_ITEMS;
    i = CardServices(GetTupleData, handle, tuple);
    if (i != CS_SUCCESS) return i;
    return CardServices(ParseTuple, handle, tuple, parse);
}

#define next_tuple(a, b, c) get_tuple(GetNextTuple, a, b, c)

#define first_tuple(a, b, c) get_tuple(GetFirstTuple, a, b, c)

/*--------------------------------------------------------
 *-- digimedia_config() is scheduled to run after a
 *-- CARD_INSERTION event is received, to configure
 *-- the PCMCIA socket, and to make the dmx interface
 *-- available to the system.
 *-------------------------------------------------------*/

#define CS_CHECK(fn, args...) \
  while ((last_ret=CardServices(last_fn=(fn),args))!=0) goto cs_failed

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

  int numchains = -1; /* no CIS present by default. */

  DO_DEBUG(1, printk("void digimedia_config(dev_link_t *link=%p)\n", link));

  if (!link) return;

  {
    cisinfo_t        cisinfo;
    int              ret;
    if ((ret=CardServices(ValidateCIS, handle, &cisinfo)) == CS_SUCCESS)
      {
	numchains = cisinfo.Chains;
	printk (KERN_INFO ":" __FILE__": CIS structure is valid and has %d entries\n", numchains);
      }
    else
      {
	char *estr = "unknown";
	if (ret==CS_BAD_HANDLE) estr="client handle is invalid";
	if (ret==CS_OUT_OF_RESOURCE) estr="unable to map CIS memory";
	printk("failed to validate CIS structure: %s\n", estr);
      }
  }


  /*
    This reads the card's CONFIG tuple to find its configuration registers.
  */
  tuple.DesiredTuple = CISTPL_CONFIG;
  tuple.Attributes   = 0;
  tuple.TupleData    = buf;
  tuple.TupleDataMax = sizeof(buf);
  tuple.TupleOffset  = 0;
  CS_CHECK(GetFirstTuple, handle, &tuple);
  CS_CHECK(GetTupleData,  handle, &tuple);
  CS_CHECK(ParseTuple,    handle, &tuple, &parse);
  link->conf.ConfigBase = parse.config.base;
  link->conf.Present    = parse.config.rmask[0];

  /* Configure card */
  link->state |= DEV_CONFIG;

  /* Look up the current Vcc */
  CS_CHECK(GetConfigurationInfo, handle, &conf);
  link->conf.Vcc = conf.Vcc;
  printk("Vcc=%d.%dV\n", conf.Vcc/10, conf.Vcc%10);


  if (0)
  {
    int j;
    int i;

    tuple.Attributes = TUPLE_RETURN_COMMON;
    tuple.TupleOffset = 0;
    tuple.TupleData = buf;
    tuple.TupleDataMax = sizeof(buf);
    tuple.DesiredTuple = RETURN_FIRST_TUPLE;

    i= first_tuple(handle, &tuple, &parse);
    for (j=0; i==CS_SUCCESS && j<10; j++)
      {
	DO_DEBUG(0, printk("    tuple%d: code=0x%x  desired=0x%x\n", j, tuple.TupleCode, tuple.DesiredTuple));
	switch(tuple.TupleCode)
	  {
	  case CISTPL_VERS_1:
	    {
	      int k;
	      cistpl_vers_1_t *version = &parse.version_1;
	      printk(__FILE__": Version: %d.%d:", version->major, version->minor);
	      for (k=0; k<version->ns; k++)
		printk(" %s", &version->str[version->ofs[k]]);
	      printk("\n");
	    }
	    break;

	  case CISTPL_DEVICE_A:
	    {
	      int k;
	      cistpl_device_t *device = &parse.device;
	      printk(__FILE__ ": Device-Memory with %d device\n", device->ndev);
	      for (k=0; k<device->ndev; k++)
		{
		  int t = device->dev[k].type;
		  char *typename =
		    (t==CISTPL_DTYPE_NULL)?"no-device":
		    (t==CISTPL_DTYPE_ROM)?"ROM":
		    (t==CISTPL_DTYPE_OTPROM)?"OTMROM":
		    (t==CISTPL_DTYPE_EPROM)?"EPROM":
		    (t==CISTPL_DTYPE_EEPROM)?"EEPROM":
		    (t==CISTPL_DTYPE_FLASH)?"FLASH":
		    (t==CISTPL_DTYPE_SRAM)?"SRAM":
		    (t==CISTPL_DTYPE_DRAM)?"DRAM":
		    (t==CISTPL_DTYPE_FUNCSPEC)?"FUNCSPEC":
		    (t==CISTPL_DTYPE_EXTEND)?"EXTENDED":"undefined";
		  printk (__FILE__ ":  device# %d of type %s access %s speed=%uns  size=%u bytes\n",
			  k,
			  typename,
			  device->dev[k].wp?"RO":"RW",
			  device->dev[k].speed,
			  device->dev[k].size);
		}
	    }
	    break;

	  case CISTPL_CONFIG:
	    break;

	  case CISTPL_CFTABLE_ENTRY:
	    {
	      cistpl_cftable_entry_t *cftable = &parse.cftable_entry;
	      int k;
	      for (k=0; k<cftable->mem.nwin && k<CISTPL_MEM_MAX_WIN; k++)
		printk(__FILE__ ": Found a memory region of size %d bytes located at card base 0x%08X to be mapped at host address 0x%08X\n",
		       cftable->mem.win[k].len,
		       cftable->mem.win[k].card_addr,
		       cftable->mem.win[k].host_addr
		       );
	    }
	    break;

	  default:
	    printk(__FILE__ ": unexpected tuple-code 0x%x. please mail this line to michael@cubic.org\n", tuple.TupleCode);
	    break;
	  }
	i = next_tuple(handle, &tuple, &parse);
      }
  }

  req.Attributes = WIN_DATA_WIDTH_8 | WIN_MEMORY_TYPE_CM | WIN_ENABLE;
  req.Base = req.Size = 0;
  req.AccessSpeed = 0;
  link->win = (window_handle_t)link->handle;
  if (CardServices(RequestWindow, &link->win, &req) == CS_SUCCESS)
    {
      mem.CardOffset = mem.Page = 0;
      mem.CardOffset = link->conf.ConfigBase;
      DO_DEBUG(0, printk (__FILE__ "req.Base=%lX  req.Size=%d  mem.CardOffset=%X\n", req.Base, req.Size, mem.CardOffset));
      if (CardServices(MapMemPage, link->win, &mem) == CS_SUCCESS)
	{
	  link->conf.ConfigIndex = 0;
	  CS_CHECK(RequestConfiguration, link->handle, &link->conf);

	  /*
	    typedef struct dev_node_t {
	    char                dev_name[DEV_NAME_LEN];
	    u_short             major, minor;
	    struct dev_node_t   *next;
	    } dev_node_t;
	  */

	  link->dev = DMX_ALLOC(dev_node_t);
	  if (link->dev)
	    {
	      /* Oh what a hack - this is some kind of impossible */
	      strcpy(link->dev->dev_name, "dmx");
	      link->dev->major = 10;     /* misc */
	      link->dev->minor = 210; /* /dev/dmx */
	      link->dev->next = NULL;
	    }
	  link->state &= ~DEV_CONFIG_PENDING;

	  iomem_base = req.Base;
	  iomem_size = req.Size;

	  DO_DEBUG(0, printk (__FILE__ "after MapMemPage: req.Base=%lX  req.Size=%d  mem.CardOffset=%X\n", req.Base, req.Size, mem.CardOffset));
	}
    }

  if (iomem_size)
    {
      if (!simple_config(link, iomem_base, iomem_size))
	return;
    }
  if (link->state & DEV_CONFIG)
    digimedia_release((u_long)link);
  return;

 cs_failed:
  dgm_cs_error(link->handle, last_fn, last_ret);
  if (link->state & DEV_CONFIG)
    digimedia_release((u_long)link);
}

#undef CS_CHECK
#undef first_tuple
#undef next_tuple

/*--------------------------------------------------------
 *--
 *-- The card status event handler.  Mostly, this schedules other
 *-- stuff to run after an event is received.  A CARD_REMOVAL event
 *-- also sets some flags to discourage the dmx interface from
 *-- talking to the ports.
 *--
 *-------------------------------------------------------*/
static int digimedia_event(event_t event, int priority,
			  event_callback_args_t *args)
{
    dev_link_t *link = args->client_data;

    DO_DEBUG(1, printk(__FILE__"digimedia_event:digimedia_event(0x%06x)\n", event));

    switch (event)
      {
      case CS_EVENT_CARD_REMOVAL:
	printk (KERN_INFO ":" __FILE__": card has been REMOVED\n");
	link->state &= ~DEV_PRESENT;
	if (link->state & DEV_CONFIG)
	  mod_timer(&link->release, jiffies + HZ/20);
	printk (KERN_INFO ":" __FILE__": card remove finished\n");
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
	if ((link->state & DEV_CONFIG))
	  CardServices(ReleaseConfiguration, link->handle);
	printk (KERN_INFO ":" __FILE__": HARD-RESET finished\n");
	break;

      case CS_EVENT_PM_RESUME:
	printk (KERN_INFO ":" __FILE__": card should RESUME\n");
	link->state &= ~DEV_SUSPEND;
	/* Fall through... */

      case CS_EVENT_CARD_RESET:
	printk (KERN_INFO ":" __FILE__": card should do a SOFT-RESET\n");
	if (DEV_OK(link))
	  CardServices(RequestConfiguration, link->handle, &link->conf);
	printk (KERN_INFO ":" __FILE__": SOFT-RESET finished\n");
	break;

      case CS_EVENT_REGISTRATION_COMPLETE:
	printk (KERN_INFO ":" __FILE__": REGISTRATION_COMPLETE\n");
	break;


      default:
	printk (KERN_INFO ":" __FILE__": UNKNOWN EVENT %d\n", event);
	break;
      }
    DO_DEBUG(1, printk(__FILE__":digimedia_event: digimedia_event(0x%06x): AT-END\n", event));
    return 0;
} /* digimedia_event */



/*====================================================================*/




/*--------------------------------------------------------
 *-- This deletes a driver "instance".  The device is de-registered
 *-- with Card Services.  If it has been released, all local data
 *-- structures are freed.  Otherwise, the structures will be freed
 *-- when the device is released.
 *-------------------------------------------------------*/
static void digimedia_detach(dev_link_t *link)
{
  DO_DEBUG(1, printk("digimedia_detach(0x%p)\n", link));
  if (pcmcia_delete_instance (link) != CS_SUCCESS)
    printk ("pcmcia_delete_instance failed\n");

} /* digimedia_detach */





/*--------------------------------------------------------
 *-- digimedia_attach() creates an "instance" of the driver,
 *-- allocating local data structures for one device.
 *-- The device is registered with Card Services.
 *-------------------------------------------------------*/
static dev_link_t *digimedia_attach(void)
{
    dev_link_t *link;

    DO_DEBUG(1, printk("digimedia_attach() {\n"));

    /* Create new digimedia device */
    link = pcmcia_create_link();
    if (link)
      {
        client_reg_t client_reg;
        int ret=0;

        link->release.function = &digimedia_release;
        link->release.data = (u_long)link;

        link->conf.Attributes = 0;
        link->conf.Vcc = 50;
        link->conf.IntType = INT_MEMORY_AND_IO;
	link->priv = NULL;

        /* Register with Card Services */
	if (pcmcia_add_instance (link) != CS_SUCCESS)
	  {
	    printk(__FILE__ ": failed to add instance\n");
	    dgm_cs_error(link->handle, RegisterClient, ret);
	    digimedia_detach(link);
	    return NULL;
	  }

	client_reg.dev_info   = &dev_info;
	client_reg.Attributes = INFO_IO_CLIENT | INFO_CARD_SHARE;
        client_reg.EventMask  = CS_EVENT_REGISTRATION_COMPLETE |
	  CS_EVENT_CARD_INSERTION | CS_EVENT_CARD_REMOVAL |
	  CS_EVENT_RESET_PHYSICAL | CS_EVENT_CARD_RESET |
	  CS_EVENT_PM_SUSPEND | CS_EVENT_PM_RESUME;
        client_reg.event_handler = &digimedia_event;
        client_reg.Version = 0x0210;
        client_reg.event_callback_args.client_data = link;

        ret = CardServices(RegisterClient, &link->handle, &client_reg);
        if (ret != CS_SUCCESS)
          {
	    dgm_cs_error(link->handle, RegisterClient, ret);
	    digimedia_detach(link);
	    return NULL;
          }
        return link;
      }
    return NULL;
} /* digimedia_attach */






/*================[ Digimedia specific stuff ]========================*/

/*--------------------------------------------------------
 *-- returns < 0 for error, > 0 if new data is available and 0
 *-- if no new data is available for that universe.
 *-------------------------------------------------------*/
static int  digimedia_data_available (DMXUniverse *u, uint start, uint size)
{
  printk("int  digimedia_data_available (DMXUniverse *u=%p, uint start=%d, uint size=%d)\n", u, start, size);
  if (u && u->user_data)
    {
      digimedia_universe_t  *dgm_u = (digimedia_universe_t *)(u->user_data);
      return dgm_u->data_avail;
    }
  return 0;
}


/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
static int  digimedia_write_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  /* If it is an output universe,
   * then data has changed if a user
   * writes new data.
   */
  if (u && !u->kind && u->user_data)  /* output universe */
    {
      digimedia_universe_t  *dgm_u = (digimedia_universe_t *)(u->user_data);
      if (dgm_u->dgm_interface)
	{
	  if (offs+size>512)
	    size = 512-offs;
	  memcpy (&dgm_u->data_pointer[offs], buff, size);
	  dgm_u->data_avail = 1;
	  u->signal_changed (u, offs, size);
	}
      return size;
    }
  return -1;
}



/*--------------------------------------------------------
 *-- read slots from the output universe.
 *-- This function can be used for both,
 *-- the input and the output universe.
 *-------------------------------------------------------*/
static int digimedia_read_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
  if (u && u->user_data && offs >= 0 && offs < 512)
    {
      digimedia_universe_t  *dgm_u = (digimedia_universe_t *)(u->user_data);
      if (offs+size>512)
        size = 512-offs;
      memcpy (buff, &dgm_u->data_pointer[offs], size);
      dgm_u->data_avail = 0;
      /* a bit buggy, since it depends on the process that has opened dmx */
      return size;
    }
  return -EINVAL;
}



/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
static void digimedia_check_reveiver (DMXUniverse *dmxu)
{
  if (dmxu && dmxu->kind==1) /* is it an input ? */
    {
      unsigned short fc=0;
      digimedia_universe_t *dgmu = (digimedia_universe_t *)dmxu->user_data;
      if (dgmu->framecount && (fc=*dgmu->framecount) != dgmu->last_framecount)
	{
	  int i;
	  char changed = 0;
	  int first=-1, last=0;

	  dgmu->last_framecount = *dgmu->framecount;

	  for (i=0; i<512; i++)
	    {
	      unsigned char val = dgmu->data_pointer[i];
	      if (val != dgmu->local_buffer[i])
		{
		  dgmu->local_buffer[i] = val;
		  changed=1;
		  if (first==-1)
		    first=i;
		  if (i>last)
		    last=i;
		}
	    }
	  if (changed)
	    {
	      dgmu->data_avail=1;
	      dmxu->signal_changed(dmxu, first, last-first+1);
	    }
	}
    }
}


/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
static void digimedia_timer_function (unsigned long data)
{
  digimedia_interface_t *dgm_if = (digimedia_interface_t *)data;
  unsigned long newtime  = jiffies + ((HZ/40 > 1)?(HZ/40):1);
  if (dgm_if)
    {
      if (DMU_IS_INPUT(dgm_if->in_use,0))
        digimedia_check_reveiver(dgm_if->universes[0]);

      if (DMU_IS_INPUT(dgm_if->in_use,1))
        digimedia_check_reveiver(dgm_if->universes[1]);

      mod_timer (&dgm_if->recv_timer, newtime);
    }
}


/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
static int digimedia_start_timer (DMXUniverse *u)
{
  if (u && u->user_data)
    {
      digimedia_universe_t  *dmxu = (digimedia_universe_t  *)u->user_data;

      if (!dmxu->dgm_interface->timer_use)
        {
          struct timer_list *t = &dmxu->dgm_interface->recv_timer;
          dmxu->dgm_interface->timer_use++;

          init_timer (t);
          t->function = digimedia_timer_function;
          t->data     = (unsigned long)(dmxu->dgm_interface);
          t->expires  = jiffies + (HZ/20 > 0)?(HZ/20):1;;
          add_timer (t);
        }
      return 0;
    }
  return -1;
}


/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
static int digimedia_stop_timer (DMXUniverse *u)
{
  if (u && u->user_data)
    {
      digimedia_universe_t  *dmxu = (digimedia_universe_t  *)u->user_data;
      if (dmxu->dgm_interface->timer_use>0)
        {
          dmxu->dgm_interface->timer_use--;
          if (dmxu->dgm_interface->timer_use <= 0)
            {
              dmxu->dgm_interface->timer_use=0;
              del_timer(&dmxu->dgm_interface->recv_timer);
            }
        }
      return 0;
    }
  return -1;
}



/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
static int digimedia_delete_universe (DMXUniverse *u)
{
  DO_DEBUG(1, printk ("static int digimedia_delete_universe (DMXUniverse *u=%p)\n", u));

  if (u && u->interface && u->interface->user_data && u->user_data)
    {
      digimedia_interface_t *dgm_if = (digimedia_interface_t *)(u->interface->user_data);
      digimedia_universe_t  *dgm_u = (digimedia_universe_t *)(u->user_data);

      /*
       * first of all stop the timer.
       */
      if (u->kind==1)
	digimedia_stop_timer (u);


      dgm_u->data_pointer = NULL;
      dgm_u->startcode    = NULL;
      dgm_u->channels     = NULL;
      dgm_u->framecount   = NULL;
      dgm_u->breaksize    = NULL;
      dgm_u->mbb_size     = NULL;

      DMU_UNUSE(dgm_if->in_use, u->conn_id); /* clear direction and in_use bit */

      DMX_FREE(dgm_u);

      return 0;
    }
  return -1;
}


/*--------------------------------------------------------
 *-- Setup or modify the digimedia specific information
 *-- on a universe for a digimedia PCMCIA card.
 *-- dir : 1=Input / 0=Output
 *-------------------------------------------------------*/
static void digimedia_setup_universe (digimedia_universe_t *dgm_u, int dir, int connector)
{
  DMXUniverse *u;
  digimedia_interface_t *dgm_if;

  DO_DEBUG(1, printk("digimedia_setup_universe(digimedia_universe_t *dgm_u=%p, int dir=%i, int connector=%i)\n", dgm_u, dir, connector));

  if(!dgm_u)
    {
      DO_DEBUG(2, printk("error: dgm_u is null\n"));
      return;
    }

  u = dgm_u->universe;
  if(!u)
    {
      DO_DEBUG(2, printk("error: u is null\n"));
      return;
    }

  dgm_if = dgm_u->dgm_interface;
  if(!dgm_if)
    {
      DO_DEBUG(2, printk("error: dgm_if is null\n"));
      return;
    }

  dgm_u->data_avail = 0;

  u->conn_id = connector-1;
  sprintf (u->connector, "DMX Link %d %s", u->conn_id?2:1, dir?"IN":"OUT");

  u->write_slots    = u->kind ? NULL : digimedia_write_universe;
  u->read_slots     = digimedia_read_universe;
  u->data_available = digimedia_data_available;
  u->user_delete    = digimedia_delete_universe;

  dgm_u->data_pointer = dgm_if->mem->dmxbuffer[u->conn_id];
  if (u->kind) /* Input */
    {
      dgm_u->startcode  = &(dgm_if->mem->in_startcode[u->conn_id]);
      dgm_u->channels   = &(dgm_if->mem->in_channel_cnt[u->conn_id]);
      dgm_u->framecount = &(dgm_if->mem->in_break_cnt[u->conn_id]);
      dgm_u->breaksize  = NULL;
      dgm_u->mbb_size   = NULL;
    }
  else /* Output */
    {
      dgm_u->startcode  = &(dgm_if->mem->out_startcode[u->conn_id]);
      dgm_u->channels   = &(dgm_if->mem->out_channel_cnt);
      dgm_u->framecount = &(dgm_if->mem->out_break_count);
      dgm_u->breaksize  = &(dgm_if->mem->out_break_time);
      dgm_u->mbb_size   = &(dgm_if->mem->out_mbb_time);
    }

  DO_DEBUG(2, printk("digimedia_setup_universe() finish\n"));
}


/*
 * # Link0 / Link1
 * 0: Idle / Idle
 * 1: Out  / Idle
 * 2: Idle / In
 * 3: Out  / In
 * 4: Out  / Out
 * 5: In   / In
 *
 * This is the mode assignment for a specific Idle,In,Out scenario.
 *
 * modes_table[first_universe][second_universe] first_universe,second_universe ={ 0=idle, 1=input, 2=output }
 */
static unsigned char modes_table[3][3] =
  {
    /*         y:-   y:I  y:O */
    /* -:x */ { 0  ,  2 , 1|8 },
    /* I:x */ {2|8 ,  5 , 3|8 },
    /* O:x */ { 1  ,  3 ,  4  }
  };

/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
static int digimedia_update_mode (digimedia_interface_t *dgm_if)
{
  DO_DEBUG(1, printk("static void digimedia_update_mode (digimedia_interface_t *dgm_if=%p)\n", dgm_if));
  if (dgm_if)
    {
      int mode2;

      if(!dgm_if->universes)
	{
	  DO_DEBUG(2, printk("dgm_if->universes is null"));
	  return -1;
	}

      DO_DEBUG(2, printk("1\n"));

      mode2 = modes_table[DMU_IS_INUSE(dgm_if->in_use,0)?(DMU_IS_OUTPUT(dgm_if->in_use,0)?2:1):0]
	                 [DMU_IS_INUSE(dgm_if->in_use,1)?(DMU_IS_OUTPUT(dgm_if->in_use,1)?2:1):0];

      DO_DEBUG(2, printk("2\n"));
      /*
       * This is an indication that some connectors may not match.
       */
      dgm_if->swap_universes = (mode2&8)?1:0;
      DO_DEBUG(2, printk("3\n"));

      if (mode2==5)
	{
	  DO_DEBUG(2, printk("4\n"));
	  if(!(dgm_if->universes[0]) || !(dgm_if->universes[1]))
	      DO_DEBUG(2, printk("error: dgm_if->universes is null (2)\n"));
	  else
	    {
	      /* We need to check if the two universes are in correct order */
	      digimedia_universe_t  *dgm_u1 = (digimedia_universe_t *)(dgm_if->universes[0]->user_data);
	      digimedia_universe_t  *dgm_u2 = (digimedia_universe_t *)(dgm_if->universes[1]->user_data);
	      digimedia_setup_universe (dgm_u1, 1, 1);
	      digimedia_setup_universe (dgm_u2, 1, 2);
	    }
	}
      else if (mode2==4)
	{
	  DO_DEBUG(2, printk("5\n"));

	  if(!(dgm_if->universes[0]) || !(dgm_if->universes[1]))
	    DO_DEBUG(2, printk("error: dgm_if->universes is null (3)\n"));
	  else
	    {
	      /* We need to check if the two universes are in correct order */
	      digimedia_universe_t  *dgm_u1 = (digimedia_universe_t *)(dgm_if->universes[0]->user_data);
	      digimedia_universe_t  *dgm_u2 = (digimedia_universe_t *)(dgm_if->universes[1]->user_data);
	      digimedia_setup_universe (dgm_u1, 0, 1);
	      digimedia_setup_universe (dgm_u2, 0, 2);
	    }
	}

      if(!dgm_if->mem)
	DO_DEBUG(2, printk("error: dgm_if->mem is null\n"));
      else
	{
	  DO_DEBUG(2, printk("6\n"));
	  dgm_if->mem->cpu_mode = mode2 & 7;
	}

      DO_DEBUG(0, printk ("digimedia_cs: setting cpu_mode to %d\n", dgm_if->mem->cpu_mode));
      return 0;
    }
  return -1;
}


/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
static int byteptr_get_long (DMXProperty *p, long *val)
{
  if (p && p->data && val)
    {
      *val = *((unsigned char *)p->data);
      return 0;
    }
  return -1;
}


/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
static int byteptr_set_long (DMXProperty *p, long val)
{
  if (p && p->data)
    {
      *((unsigned char *)p->data) = val;
      return 0;
    }
  return -1;
}


/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
static int shortptr_get_long (DMXProperty *p, long *val)
{
  if (p && p->data && val)
    {
      *val = shortreadfromcard (p->data);
      return 0;
    }
  return -1;
}


/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
static int shortptr_set_long (DMXProperty *p, long val)
{
  if (p && p->data)
    {
      shortwritetocard (p->data, val);
      return 0;
    }
  return -1;
}


/*--------------------------------------------------------
 *--
 *--
 *-------------------------------------------------------*/
int digimedia_create_universe (DMXUniverse *u, DMXPropList *pl)
{
  DO_DEBUG(1, printk ("int digimedia_create_universe (DMXUniverse *u=%p, DMXPropList *pl=%p)\n", u, pl));

  if (u && u->interface && u->interface->user_data && u->kind>=0 && u->kind<2)
    {
      digimedia_interface_t *dgm_if = (digimedia_interface_t *)u->interface->user_data;
      int connector = -1;

      /* not both universes active */
      if (!(DMU_IS_INUSE(dgm_if->in_use,0) && DMU_IS_INUSE(dgm_if->in_use,1)))
	{
	  int u1_kind=0, u2_kind=0;

	  DO_DEBUG(0, printk(__FILE__ ": in_use = %d\n", dgm_if->in_use));


	  /*--
	   *-- Here we check the number and kinnd of universes allready in use
	   *-- and decide how to map the universes to the outputs.
	   *--
	   *-- 3 states := {Idle=-,In=I,Out=O}
	   *--
	   *-- # Link0 / Link1
	   *-- 0: Idle / Idle
	   *-- 1: Out  / Idle
	   *-- 2: Idle / In
	   *-- 3: Out  / In
	   *-- 4: Out  / Out
	   *-- 5: In   / In
	   *--
	   *--
	   *-- Idle/Idle  => 0
	   *-- Idle/In    => 2
	   *-- Idle/Out   => invalid Out/Idle (1) instead.
	   *-- In/Idle    => invalid In/In (5) instead with first input used.
	   *-- In/In      => 5
	   *-- In/Out     => invalid Out/In (3) instead.
	   *-- Out/Idle   => 1
	   *-- Out/In     => 3
	   *-- Out/Out    => 4
	   *--
	   *--
	   *--
	   *--      New->  -/- | -/I | -/O | I/- | I/I | I/O | O/- | O/I | O/O
	   *--  V-Current+-----+-----+-----+-----+-----+-----+-----+-----+-------
	   *--       -/- |  -  |     |     |     |     |     |     |     |       
	   *--      -----+-----+-----+-----+-----+-----+-----+-----+-----+-------
	   *--       -/I |     |  -  |     |     |     |     |     |     |       
	   *--      -----+-----+-----+-----+-----+-----+-----+-----+-----+-------
	   *--       -/O |     |     |  -  |     |     |     |     |     |       
	   *--      -----+-----+-----+-----+-----+-----+-----+-----+-----+-------
	   *--       I/- |     |     |     |  -  |     |     |     |     |       
	   *--      -----+-----+-----+-----+-----+-----+-----+-----+-----+-------
	   *--       I/I |     |     |     |     |  -  |     |     |     |       
	   *--      -----+-----+-----+-----+-----+-----+-----+-----+-----+-------
	   *--       I/O |     |     |     |     |     |  -  |     |     |       
	   *--      -----+-----+-----+-----+-----+-----+-----+-----+-----+-------
	   *--       O/- |     |     |     |     |     |     |  -  |     |       
	   *--      -----+-----+-----+-----+-----+-----+-----+-----+-----+-------
	   *--       O/I |     |     |     |     |     |     |     |  -  |       
	   *--      -----+-----+-----+-----+-----+-----+-----+-----+-----+-------
	   *--       O/O |     |     |     |     |     |     |     |     |  -    
	   *--      -----+-----+-----+-----+-----+-----+-----+-----+-----+-------
	   *--
	   */

	  switch (DMU_IS_INUSE(dgm_if->in_use,0) + (DMU_IS_INUSE(dgm_if->in_use,1)<<1))
	    {
	    case 0: /* none is in use */
	      connector = (u->kind)?2:1; /* Input : Output */
	      DO_DEBUG(0, printk ("digimedia: input/output configuration\n"));
	      break;

	    case 1: /* first is in use */
	      u1_kind = DMU_DIR(dgm_if->in_use,0);
	      u2_kind = u->kind&1;
	      if ((u1_kind==1) && (u2_kind==0))
		{
		  printk ("digimedia: illegal configuration: link1=IN, link2=OUT\n");
		  return -1;
		}
	      connector = 2;
	      break;

	    case 2: /* second is in use */
	      u1_kind = u->kind&1;
	      u2_kind = DMU_DIR(dgm_if->in_use,1);
	      if ((u1_kind==1) && (u2_kind==0))
		{
		  printk ("digimedia: illegal configuration: link1=IN, link2=OUT\n");
		  return -1;
		}
	      connector = 1;
	      break;

	    case 3: /* both are in use. This can never happen, because of the prev. if */
	      printk (KERN_INFO "no more dmx-link available for use\n");
	      return -1;
	    }
	}


      if ((connector>=1) && (connector<=2))
        {
          digimedia_universe_t *dgm_u = DMX_ALLOC(digimedia_universe_t);
          if (dgm_u)
            {
	      DMXProperty *p = NULL;

	      memset(dgm_u->local_buffer, 0, sizeof(dgm_u->local_buffer));
	      dgm_u->universe = u;
	      dgm_u->last_framecount = 0;
	      dgm_u->dgm_interface = dgm_if;

	      /*
	       * slhu->direction = u->kind;
	       * slhu->universe = u->conn_id;
	       */

	      DO_DEBUG(2, printk("before digimedia_setup_universe ()\n"));
	      digimedia_setup_universe (dgm_u, u->kind, connector);

	      /*
	       * Create a property for the startcode, slots and framecount.
	       */
	      if (u->props)
		{
		  DMXPropList *pl = u->props;
		  p = dmxprop_create_long ("startcode", 0);
		  if (p)
		    {
		      if (dmxprop_user_long(p, byteptr_get_long, byteptr_set_long, dgm_u->startcode) < 0)
			p->delete(p);
		      else
			pl->add(pl, p);
		    }
		  p = dmxprop_create_long ("frames", 0);
		  if (p)
		    {
		      if (dmxprop_user_long(p, shortptr_get_long, shortptr_set_long, dgm_u->framecount) < 0)
			p->delete(p);
		      else
			pl->add(pl, p);
		    }
		  p = pl->find(pl,"slots");
		  if(p)
		    dmxprop_user_long(p, shortptr_get_long, shortptr_set_long, dgm_u->channels);
		}

              u->user_data = (void *)dgm_u;

	      DMU_ENABLE(dgm_if->in_use, u->conn_id, u->kind);


	      if (digimedia_update_mode(dgm_if) >= 0)
		{
                  dgm_if->universes[connector-1] = u;

		  /*
		   * now after anything else is set up start the timer
		   * that checks for new input, if it is an input universe.
		   */
		  if (u->kind==1)
		    digimedia_start_timer (u);

		  return 0;
		}

	      /*
	       * failed to create the interface, because the mode
	       * can not be set. Bailing out.
	       */
	      DMU_UNUSE(dgm_if->in_use, u->conn_id);

	      DMX_FREE(dgm_u);
            }
        }
    }
  return -1;
}



/*--------------------------------------------------------
 *--
 *-- Initializes the digimedia card
 *--
 *-------------------------------------------------------*/
int dgm_initialize (dgm_memory_t *dgm)
{
  DO_DEBUG(1, printk("int dgm_initialize (dgm_memory_t *dgm=%p)\n", dgm));

  if (dgm && (dgm->ready_flag == 0xF0) && !memcmp("DMXOEM", dgm->signature, 6))
    {
      int i;
      printk (KERN_INFO "Digimedia PCMCIA dmx512 interface version %d.%d found\n", dgm->cpu_version_high, dgm->cpu_version_low);

      dgm->cpu_mode = dgmcpumode_out_out;
      dgm->out_startcode[0] = 0;
      dgm->out_startcode[1] = 0;
      shortwritetocard(&dgm->out_channel_cnt, 512);
      shortwritetocard(&dgm->out_break_time, 2*100); /* 100us */
      shortwritetocard(&dgm->out_break_count, 0);    /* outgoing packets */
      shortwritetocard(&dgm->out_mbb_time, (2*0)<<8);    /* 0us */

      for (i=0; i<512; i++)
	{
	  dgm->dmxbuffer[0][i] = 0;
	  dgm->dmxbuffer[1][i] = 0;
	}
      return 0;
    }
  else
    {
      printk (__FILE__ ": cause for failure is ");
      if (!dgm)
	printk ("dgm==NULL\n");
      else if (dgm->ready_flag != 0xF0)
	printk ("ready_flag != 0xF0 (0x%0x)\n", dgm->ready_flag);
      else if (memcmp("DMXOEM", dgm->signature, 6))
	{
	  char s[11];
	  memcpy(s, dgm->signature, 10);
	  s[10] = 0;
	  printk ("signature is incorrect \"%s\"\n", s);
	}
    }
  return -1;
}



/*--------------------------------------------------------
 *-- digimedia_delete_interface
 *--
 *-- This is the function called by the dmx4linux layer
 *-- after all universes for that interface are successfully
 *-- deleted and before the interface itself is to be deleted.
 *-- It cleans up anything that is not removed by dmx4linux.
 *-------------------------------------------------------*/
static int digimedia_delete_interface (DMXInterface *i)
{
  DO_DEBUG(1, printk("static int digimedia_delete_interface (DMXInterface *i=%p)\n", i));
  if (i && i->user_data)
    {
      digimedia_interface_t *dgm_if = (digimedia_interface_t *)i->user_data;

      if (!(DMU_IS_INUSE(dgm_if->in_use,0)||DMU_IS_INUSE(dgm_if->in_use,1)))
	{
	  i->user_data = NULL;
	  if (dgm_if->mem)
	    vfree(dgm_if->mem); /* this fails, why I don't know */
	  DMX_FREE(dgm_if);
	  return 0;
	}
      printk ("FATAL: failed to remove interface - there are universes to be deleted\n");
    }
  return -1;
}



/*--------------------------------------------------------
 *-- digimedia_create_interface
 *--
 *-- This function is called after the internal data
 *-- structures are created and before the interface
 *-- is added.
 *-------------------------------------------------------*/
int  digimedia_create_interface (DMXInterface *i, DMXPropList *pl)
{
  unsigned long mem_base = 0L;
  unsigned long mem_size = 0L;

  DO_DEBUG(1, printk("int digimedia_create_interface (DMXInterface *i=%p, DMXPropList *pl=%p)\n", i, pl));

  if (pl && pl->find)
    {
      DMXProperty *p = pl->find(pl, "base");
      if (p)
	{
	  p->get_long (p, &mem_base);
	  p = pl->find(pl, "size");
	  if (p) p->get_long (p, &mem_size);
	}
    }

  printk (KERN_INFO ":" __FILE__ ": found interface-memory at 0x%08lX with size %ld\n", mem_base, mem_size);

  if (mem_base && (mem_size>=1024) && (mem_size<=4096))
    {
      digimedia_interface_t *dgm_if = DMX_ALLOC(digimedia_interface_t);
      if (dgm_if)
	{
	  dgm_memory_t  *dgm_mem = (dgm_memory_t *)ioremap(mem_base+digimedia_memory_offset, mem_size);
	  if (dgm_mem)
	    {
              dgm_if->timer_use=0;
	      i->user_data = dgm_if;
	      dgm_if->in_use = 0;
	      dgm_if->swap_universes = 0;
	      dgm_if->mem = dgm_mem;
	      i->user_delete = digimedia_delete_interface;
	      if (dgm_initialize (dgm_mem) >= 0)
		{

		  printk (KERN_INFO  ":" __FILE__ ": successfully initialized SLH2512 dmx interface\n");
		  return 0;
		}
	      vfree(dgm_mem);
	    }
	  printk(__FILE__ ": failed to initialize interface\n");
	  DMX_FREE(dgm_if);
	}
    }
  return -1;
}







/*--------------------------------------------------------
 *-- init_digimedia_cs
 *--
 *-- This function is called after the module has loaded.
 *--
 *-------------------------------------------------------*/
static int __init init_digimedia_cs(void)
{
  int ret=0;
  servinfo_t serv;

  DO_DEBUG(1, printk("static int __init init_digimedia_cs(void)\n"));

  family = dmx_create_family ("PCMCIA");
  if (family)
    {
      DMXDriver *driver = family->create_driver (family, "digimedia", digimedia_create_universe, NULL);
      if (!driver)
        {
          family->delete(family, 0);
	  return -1;
        }
      driver->user_create_interface = digimedia_create_interface;
    }
  else
    return -1;

  CardServices(GetCardServicesInfo, &serv);
  if (serv.Revision != CS_RELEASE_CODE)
    {
      printk(KERN_INFO ":" __FILE__ ": Card Services release does not match!\n");
      printk(KERN_INFO "Card Services Release = %X  driver version = %X\n", serv.Revision, CS_RELEASE_CODE);
      return -1;
  }

  ret=register_pccard_driver(&dev_info, &digimedia_attach, &digimedia_detach);
  printk ("register_pccard_driver() =%d\n", ret);
  if (ret!=CS_SUCCESS)
    {
      family->delete(family, 0);
      return -1;
    }
  return 0;
}



/*--------------------------------------------------------
 *--
 *-- exit_digimedia_cs
 *--
 *-------------------------------------------------------*/
static void __exit exit_digimedia_cs(void)
{
  int i;

  DO_DEBUG(1, printk("static void __exit exit_digimedia_cs(void)\n"));

  for (i=0; i<100 && dev_list != NULL; i++)
    digimedia_detach(dev_list);

  if (family)
    family->delete(family, 0);
  unregister_pccard_driver(&dev_info);
}


module_init(init_digimedia_cs);
module_exit(exit_digimedia_cs);
