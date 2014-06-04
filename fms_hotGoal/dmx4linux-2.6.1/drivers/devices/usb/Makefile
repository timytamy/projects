# dmx/drivers/devices/usb/Makefile

-include /tmp/dmxconfig.mk

obj-m	:= usb2dmx.o ftdi2dmx.o sunlite.o

ifneq ($(KERNELRELEASE),)
EXTRA_CFLAGS:=-DDMXVERSION=\"${DMXVERSION}\"
else

PWD	:= $(shell pwd)
KDIR	?= /lib/modules/$(shell uname -r)/build
KVER	?= $(shell uname -r)
obj-ko	=  $(obj-m:%.o=%.ko)

all:	modules

clean modules modules_install:
	$(MAKE) -C $(KDIR) M=$(PWD) $@
	sh setup_hotplug.sh $@

distclean: clean uninstall
	rm -f Module.symvers

uninstall:
	cd $(MODULEINSTALLPATH); rm -f $(obj-ko)
	sh setup_hotplug.sh $@

endif
