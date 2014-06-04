# dmx/drivers/dmxdev/Makefile

-include /tmp/dmxconfig.mk

obj-m	:= dmxdev.o

ifneq ($(KERNELRELEASE),)
dmxdev-objs:=dmx_dev.o dmx_proc.o dmx_props.o dmx_family.o dmx_driver.o dmx_interface.o dmx_universe.o dmx_fileinfo.o
EXTRA_CFLAGS:=-DDMXVERSION=\"${DMXVERSION}\" \
	-DDMXOUTMINOR=$(DMXOUTMINOR) -DDMXINMINOR=$(DMXINMINOR) \
	-DVERSIONMAJOR=$(VERSIONMAJOR) -DVERSIONMINOR=$(VERSIONMINOR)

else

PWD	:= $(shell pwd)
KDIR	?= /lib/modules/$(shell uname -r)/build
KVER	?= $(shell uname -r)
obj-ko	=  $(obj-m:%.o=%.ko)

all:	modules

clean modules modules_install:
	$(MAKE) -C $(KDIR) M=$(PWD) $@

distclean: clean uninstall
	rm -f Module.symvers

uninstall:
	cd $(MODULEINSTALLPATH); rm -f $(obj-ko)

endif
