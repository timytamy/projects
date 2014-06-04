/*
 * okddmx.c
 * Device-driver for O'ksi'D DMX-Interface (release 1.0).
 *
 * Copyright (c) 2003  O'ksi'D
 * Copyright (C) 1999,2000  Michael Stickel
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

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
#include <linux/config.h>
#endif

#if (!defined(CONFIG_PARPORT)) && (!defined(CONFIG_PARPORT_MODULE))
#error Linux Kernel needs Parport support for the okddmx interface
#endif

#define  __KERNEL_SYSCALLS__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/parport.h>
#include <linux/delay.h>

#include <dmx/dmxdev.h>

#define D0 0x1
#define D1 0x2
#define D2 0x4
#define D3 0x8
#define D4 0x10
#define D5 0x20
#define D6 0x40
#define D7 0x80

#define IN3 0x8
#define IN4 0x10
#define IN5 0x20
#define IN6 0x40
#define IN7 0x80

#define CBLK D2
#define RBLK D1
#define WBLK D0
#define RWBLK D1|D0
#define STAT D2|D0
#define BUSY 0x0F

#define NBRETRY 1000

MODULE_AUTHOR("(c) 2001 Michael Stickel <michael@cubic.org> http://llg.cubic.org"
"\n(c) 2003 O'ksi'D http://www.oksid.ch");
MODULE_DESCRIPTION("Driver for the okddmx interface (http://www.oksid.ch) version " DMXVERSION);
MODULE_LICENSE("GPL");

struct okddevice {
	struct pardevice 	*pdev;
	unsigned char buffer[512];
	char chip;
	DMXUniverse *input;
	int pid;
};

static struct pardevice *newpdev = NULL;
static char chip = 0;
static DMXUniverse *input_universe;

static int read_status(struct parport *port)
{
	return (port->ops->read_status(port) ^ 0x80) & 0xf8;
}

static int okddmx_thread(void *user_data)
{
	DMXUniverse *u = (DMXUniverse*) user_data;
	wait_queue_head_t queue;

	init_waitqueue_head(&queue);

	interruptible_sleep_on_timeout(&queue, HZ*5);
	while (!signal_pending(current)) {
		interruptible_sleep_on_timeout(&queue, HZ/44);
		u->signal_changed (u, 0, 512);
	}
	return 0;
}

static int okddmx_reset_chips(struct parport *port)
{
	int i;
	port->ops->write_data(port, D6|D5);
	for (i = NBRETRY; i > 0; i--) {
		if (!(read_status(port) & IN3)) break;
	}
	if (!i) {
  		printk (KERN_ERR "okddmx_reset_chips: "
			"device does not respond\n");
		return -1;
	}
  	printk (KERN_ERR "okddmx_reset_chips: pre-reset (%d)\n", i);
	port->ops->write_data(port, D7|D4|D3);
	for (i = NBRETRY; i > 0; i--) {
		if (read_status(port) & IN3) break;
	}
	if (!i) {
  		printk (KERN_ERR "okddmx_reset_chips: "
			"device does not respond to command (%d)\n", i);
		return -1;
	}
  	printk (KERN_ERR "okddmx_reset_chips: successful reset (%d)\n", i);
  	return 0;
}

static int okddmx_command(struct parport *port, int chip, char *in,
	char *out, int len)
{
	int i, n;
	int cmd = 0;
	int busy = 1;

  	chip = (chip & 0x3) << 5;

	if (in) cmd = 0x2;
	if (out) cmd |= 0x1;
	if (!cmd) return 0;
	if (len == 2) cmd = STAT;

	if (!(read_status(port) & IN3)) okddmx_reset_chips(port);
	if (!(read_status(port) & IN3)) return -1;

	while (busy < 200) {
  		/*port->ops->write_data(port, D4|D7|chip|cmd);*/
  		port->ops->write_data(port, D7|chip|cmd);
		for (i = NBRETRY; i > 0; i--) {
			if (!(read_status(port) & IN3)) break;
		}
		if (!i) {
  			printk (KERN_ERR "okddmx_command: "
				"device does not respond to command\n");
			return -1;
		} else {
			break;
		}

  		port->ops->write_data(port, D4);
		for (i = NBRETRY; i > 0; i--) {
			if ((read_status(port) & IN3)) break;
		}
		if (!i) {
  			printk (KERN_ERR "okddmx_command: "
				"device does not respond to busy\n");
			return -1;
		}
		busy++;
		if (busy >= 200) {
  			printk (KERN_ERR "okddmx_command: "
				"device is too busy\n");
			return -1;
		}
		udelay(10);
	}

	for (n = 0; n < len; n++) {
		unsigned int d = 0;
		unsigned int id = 0;
		if (out) d = (unsigned char)out[n];
  		/*port->ops->write_data(port, ((d >> 4) & 0x0f));*/
  		port->ops->write_data(port, D4|((d >> 4) & 0x0f));
		for (i = NBRETRY; i > 0; i--) {
			id = read_status(port);
			if (id & IN3) break;
		}
		if (!i) {
  			printk (KERN_ERR "okddmx_command: "
				"device does not respond to data 0\n");
			return -1;
		}
		if (!in && (((id >> 4) & 0x0F) != (d >> 4))) {
 			printk (KERN_ERR "okddmx_command: "
                        	"(%d-%d) data 0 mismatch %x %x\n", chip,
				n, d >> 4,
				((id >> 4) & 0x0F));
		}
		if (in) in[n] = id & 0xF0;
  		/*port->ops->write_data(port, D4|(d & 0x0f));*/
  		port->ops->write_data(port, (d & 0x0f));
		for (i = NBRETRY; i > 0; i--) {
			id = read_status(port);
			if (!(id & IN3)) break;
		}
		if (!i) {
  			printk (KERN_ERR "okddmx_command: "
				"device does not respond to data 1\n");
			return -1;
		}
		if (!in && (((id >> 4) & 0x0F) != (d & 0x0f))) {
 			printk (KERN_ERR "okddmx_command: "
                        	"(%d-%d) data 1 mismatch %x %x\n", chip,
				n, d & 0x0f,
				((id >> 4) & 0x0F));
		}
		if (in) {
			in[n] |= (id >> 4) & 0x0F;
		}

	}

	/*port->ops->write_data(port, 0);*/
	port->ops->write_data(port, D4);
	for (i = NBRETRY; i > 0; i--) {
		if (read_status(port) & IN3) break;
	}
	if (!i) {
  		printk (KERN_ERR "okddmx_command: "
			"device does not respond to EOC (%d)\n", i);
		return -1;
	}
  	return 0;
}

static int okddmx_rw_block(struct parport *port, int chip, char *in, char *out)
{
	return okddmx_command(port, chip, in, out, 32);
}

static int okddmx_get_status(struct parport *port, int chip, char *status)
{
	return okddmx_command(port, chip, status, NULL, 2);
}

static int okddmx_set_current_block(struct parport *port, int chip, int block)
{
	int i, in = 0;
  	chip = (chip & 0x3) << 5;
  	block &= 0xf;

	if (!(read_status(port) & IN3)) okddmx_reset_chips(port);
	if (!(read_status(port) & IN3)) return -1;

  	/*port->ops->write_data(port, D4|D7|chip|CBLK);*/
  	port->ops->write_data(port, D7|chip|CBLK);
	for (i = NBRETRY; i > 0; i--) {
		if (!(read_status(port) & IN3)) break;
	}
	if (!i) {
  		printk (KERN_ERR "okddmx_set_current_block: "
			"device does not respond to command\n");
		return -1;
	}

  	/*port->ops->write_data(port, block);*/
  	port->ops->write_data(port, D4|block);
	for (i = NBRETRY; i > 0; i--) {
		in = read_status(port);
		if (in & IN3) break;
	}
	if (!i) {
  		printk (KERN_ERR "okddmx_set_current_block: "
			"device does not respond to data\n");
		return -1;
	}
	if (((in >> 4) & 0x0F) != block) {
 		printk (KERN_ERR "okddmx_set_current_block: "
                        "data mismatch %x %x\n", block, ((in >> 4) & 0x0F));
	}
  	return 0;
}

static int okddmx_clean_device(struct parport *port)
{
	int i;
	char buf[32];
	int chip;
	okddmx_reset_chips(port);
	for (i = 0; i < 32; i++) buf[i] = 0xff;
	for (chip = 0; chip < 3; chip++) {
		for (i = 0; i <= 0x0F; i++) {
			if (okddmx_set_current_block(port, chip, i) < 0)
				return -1;
			if (okddmx_rw_block(port, chip, NULL, buf) < 0)
				return -1;
		}
	}
	for (i = 0; i < 32; i++) buf[i] = 0x00;
	for (chip = 0; chip < 3; chip++) {
		for (i = 0; i <= 0x0F; i++) {
			if (okddmx_set_current_block(port, chip, i) < 0)
				return -1;
			if (okddmx_rw_block(port, chip, NULL, buf) < 0)
				return -1;
		}
	}
	return 0;
}

static int okddmx_data_available (DMXUniverse *u, uint offs, uint size)
{
	/* int change = 0;
	int i;
	unsigned char stat[2];
	*/
	struct okddevice *okd;
  	struct parport *port;

	if (!u) return -1;
	okd = u->user_data;
	port=okd->pdev->port;

	if (size < 0 || (offs + size >= 512)) {
  		printk (KERN_ERR "okddmx_data_available: illegal parameters\n");
  		return -EINVAL;
	}

	return 1;
#if 0
	/* reading the status will clean it ! don't do it, we need it when
	   reading data */
	if (okddmx_get_status(port, okd->chip, (char*) stat) < 0) {
		return -1;
	}
	change = (stat[1] << 8) | stat[0];
	for (i = 0; i < 16; i++) {
		int n = i * 32;
		if (change & (0x1 << i) && n >= offs && n < (offs + size))
			return 1;
	}
	return 0;
#endif
}

static int okddmx_read_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
	int change = 0;
	int i;
	unsigned char stat[2];
	struct okddevice *okd = u->user_data;
  	struct parport *port=okd->pdev->port;

	if (size < 0 || (offs + size > 512)) {
  		printk (KERN_ERR "okddmx_read_universe: "
			"illegal parameters %ld %d\n", offs, size);
  		return -EINVAL;
	}

	if (okddmx_get_status(port, okd->chip, (char*) stat) < 0) {
		return -1;
	}
	change = (stat[1] << 8) | stat[0];

	for (i = 0; i < 16; i++) {
		if (change & (0x1 << i)) {
			int r;
			okddmx_set_current_block(port, okd->chip, i);
			r = okddmx_rw_block(port, okd->chip,
				okd->buffer + 32 * i, NULL);
			if (r < 0) return r;
		}
	}
	for (i = 0; i < size; i++) {
		buff[i] = okd->buffer[offs + i];
	}

	return size;
}

static int  okddmx_write_universe (DMXUniverse *u, off_t offs, DMXSlotType *buff, size_t size)
{
	int change = 0;
	int i;
	struct okddevice *okd = u->user_data;
  	struct parport *port=okd->pdev->port;

	if (size < 0 || (offs + size > 512)) {
  		printk (KERN_ERR "dmx43_write_universe: illegal parameters\n");
  		return -EINVAL;
	}
	for (i = 0; i < 512; i++) {
		if (i >= offs && i < (offs + size)) {
			int n = i - offs;
			if (buff[n] != okd->buffer[i]) {
				okd->buffer[i] = buff[n];
				n = i / 32;
				change |= 0x1 << n;
			}
		}
	}

	if (!change) return size;
	for (i = 0; i < 16; i++) {
		if (change & (0x1 << i)) {
			int r;
			okddmx_set_current_block(port, okd->chip, i);
			r = okddmx_rw_block(port, okd->chip, NULL,
				okd->buffer + 32 * i);
			if (r < 0) return r;
		}
	}

	return size;
}

static int setPropertyInt (DMXUniverse *u, char *name,  int val)
{
  DMXProperty *p = u ? u->findprop (u, name) : NULL;
  return p?p->set_long (p, val):-1;
}

static int okddmx_delete_universe (DMXUniverse *u)
{
  	struct okddevice *o;
  	if (!u) return -1;
 	o = u->user_data;
	if (o && o->pid > 0) {
		if (!kill_proc(o->pid, SIGKILL, 1)) {
		  /*waitpid(o->pid, NULL, __WCLONE|WNOHANG);*/
		}
	}
  	kfree(o);
  	return 0;
}


static int okddmx_create_universe (DMXUniverse *u, DMXPropList *pl)
{
  struct okddevice *o;
  if (!u) return -1;
  o = kmalloc(sizeof(struct okddevice), GFP_KERNEL);
  u->user_data = o;
  u->user_delete = okddmx_delete_universe;
  o->pdev = newpdev;
  strcpy (u->connector, "one");
  u->conn_id = 0;
  setPropertyInt(u, "slots", 512);
  memset(o->buffer, 0, sizeof(o->buffer));

  o->chip = chip;
  o->pid = 0;

  if (u->kind) {
  	u->read_slots = okddmx_read_universe;
	u->data_available = okddmx_data_available;
	o->pid = kernel_thread(okddmx_thread, (void*)u,
			CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
  } else {
	o->input = input_universe;
  	u->write_slots = okddmx_write_universe;
  }
  return 0;
}

int okddmx_delete_interface(DMXInterface *i)
{
	struct pardevice *pdev;
	if (!i) return -1;
  	pdev = i->user_data;
  	if (pdev) {
      		parport_release (pdev);
      		parport_unregister_device(pdev);
    	}
	return 0;
}

/*
 * --------- Module creation / deletion ---------------
 */

static int parport=0;
module_param(parport, int, S_IRUGO);
MODULE_PARM_DESC(parport,"parport number (0=parport0)");

static DMXFamily *okddmx_fam = NULL;


static int __init okddmx_init(void)
{
  struct parport *pport = parport_find_number(parport);
  if (pport)
    {

      newpdev = parport_register_device (pport, "okddmx",
					 NULL,
					 NULL,
					 NULL,
					 PARPORT_DEV_EXCL,
					 (void *)NULL);
      if (!newpdev)
	{
	  printk (KERN_ERR "okddmx: failed to get access to parport\n");
	  return -1;
	}

      printk (KERN_INFO "okddmx: got parport%d\n", parport);


      if (parport_claim (newpdev)==0)
	{
	  printk(KERN_INFO "okddmx: successfully  claimed parport\n");
          okddmx_fam = dmx_create_family ("PAR");
          if (!okddmx_fam)
            {
              printk (KERN_ERR "okddmx: unable to register PAR family\n");
              return -EBUSY;
            }
          else
            {
              DMXInterface *dmxif = NULL;
              DMXDriver *okddmx_drv = okddmx_fam->create_driver (okddmx_fam,
			"okddmx", okddmx_create_universe, NULL);

              dmxif = okddmx_drv->create_interface (okddmx_drv, NULL);

              if (dmxif)
                {
			DMXUniverse *u;
			dmxif->user_data = newpdev;
			chip = 0;
			input_universe = dmxif->create_universe (dmxif, 1, NULL);
			u = dmxif->create_universe (dmxif, 0, NULL);
			chip = 1;
			u = dmxif->create_universe (dmxif, 0, NULL);
			chip = 2;
			u = dmxif->create_universe (dmxif, 0, NULL);
                  if (u)
                    {
                      /*if (okddmx_clean_device(newpdev->port) >= 0) {*/
		      okddmx_clean_device(newpdev->port);
		      {
			dmxif->user_delete = okddmx_delete_interface;
			u = dmxif->create_universe (dmxif, 0, NULL);
                        printk(KERN_INFO
				"okddmx: driver successfully started\n");
                        return 0;
                      }
                    }
                }
    		okddmx_fam->delete (okddmx_fam, 0);
		okddmx_fam = NULL;
            }
          parport_release(newpdev);
	}

      parport_unregister_device(newpdev);
    }
  return -1;
}


static void __exit okddmx_exit(void)
{

  if (okddmx_fam)
    okddmx_fam->delete (okddmx_fam, 0);
  printk(KERN_INFO "okddmx: driver stopped\n");
}

module_init(okddmx_init);
module_exit(okddmx_exit);
