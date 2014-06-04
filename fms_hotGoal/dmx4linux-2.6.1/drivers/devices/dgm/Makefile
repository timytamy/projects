# dmx/drivers/devices/dgm/Makefile

-include /tmp/dmxconfig.mk

obj-m := dgm.o

ifneq ($(CONFIG_PCI),)
	ifneq ($(CONFIG_DMX4LINUX_DGMPCI),)
		obj-m += dgmpci.o
	endif
endif

ifneq ($(CONFIG_PCMCIA),)
	ifneq ($(CONFIG_DMX4LINUX_DGM_CS),)
		obj-m += dgm_cs.o
	endif
endif

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
ifneq ($(CONFIG_PCMCIA),)
ifneq ($(CONFIG_DMX4LINUX_DGM_CS),)
	$(MAKE) -C configs $@
endif
endif

distclean: clean uninstall

uninstall:
	cd $(MODULEINSTALLPATH); rm -f $(obj-ko)
	$(MAKE) -C $(KDIR) M=$(PWD) $@
ifneq ($(CONFIG_PCMCIA),)
ifneq ($(CONFIG_DMX4LINUX_DGM_CS),)
	$(MAKE) -C configs $@
endif
endif

endif
