# dmx/drivers/devices/pcmcia/Makefile

-include /tmp/dmxconfig.mk

obj-m += digimedia_cs.o

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
	$(MAKE) -C configs $@

distclean: clean uninstall
	rm -f Module.symvers

uninstall:
	cd $(MODULEINSTALLPATH); rm -f $(obj-ko)
	$(MAKE) -C configs $@

endif
