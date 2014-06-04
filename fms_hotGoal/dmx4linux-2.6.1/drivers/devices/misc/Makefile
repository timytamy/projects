-include /tmp/dmxconfig.mk

obj-m	:= dmxdummy.o

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

distclean: clean uninstall
	rm -f Module.symvers

uninstall:
	cd $(MODULEINSTALLPATH); rm -f $(obj-ko)

endif
