# dmx/drivers/devices/isa/Makefile

-include /tmp/dmxconfig.mk

obj-m	:= dmxenlight.o

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
	$(MAKE) -C soundlight $@

distclean: clean uninstall

uninstall:
	cd $(MODULEINSTALLPATH); rm -f $(obj-ko)
	$(MAKE) -C soundlight $@

endif
