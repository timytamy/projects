-include /tmp/dmxconfig.mk

all clean distclean install uninstall modules_install:
	$(MAKE) -C misc $@
ifneq	($(CONFIG_USB),)
	$(MAKE) -C usb $@
endif
ifneq	($(CONFIG_ISA),)
	$(MAKE) -C isa $@
endif
ifneq	($(CONFIG_PCI),)
	$(MAKE) -C pci $@
endif
ifneq	($(CONFIG_PARPORT),)
	$(MAKE) -C parport $@
endif

# PCMCIA dmx drivers are currently not supported on 2.6 kernels
#ifneq	($(CONFIG_PCMCIA),)
#	$(MAKE) -C pcmcia $@
#endif
#ifeq	"$(CONFIG_DMX4LINUX_DGM)" "y"
#	$(MAKE) -C dgm $@
#endif
