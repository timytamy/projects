#
# Toplevel Makefile for the DMX-Devices
#
# make           or
# make all       to compile the sources
# make install   to install them into the kernel-module-path
# make clean     to cleanup all subdirectories derived from here
# make devinodes to create inodes in /dev

-include /tmp/dmxconfig.mk

all clean distclean uninstall:
	$(MAKE) -C dmxdev $@
	$(MAKE) -C devices $@

install:
	if [ ! -d $(MODULEINSTALLPATH) ]; then mkdir -p $(MODULEINSTALLPATH); fi
	$(MAKE) -C dmxdev modules_install
	$(MAKE) -C devices modules_install
	depmod -ae
	if [ ! -f /etc/udev/rules.d/dmx4linux.rules ]; then install -m 644 dmx4linux.rules /etc/udev/rules.d/dmx4linux.rules; fi

distclean:	postdistclean

postdistclean:	
	-./setup_devs distclean

devinodes:
	./setup_devs install
