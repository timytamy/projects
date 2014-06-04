-include /tmp/dmxconfig.mk

.phony:	all install postinstall clean distclean preclean devinodes

all install uninstall clean distclean:
	@if [ ! -f /tmp/dmxconfig.mk ]; then echo "did not find /tmp/dmxconfig.mk. Please run ./configure"; false; fi
	$(MAKE) -C libs $@
	$(MAKE) -C tools $@
	$(MAKE) -C examples $@
	$(MAKE) -C drivers $@

clean:	preclean

preclean:
	find . -name "*~" -exec rm -f {} ';'
	find . -name TAGS -exec rm -f {} ';'
	find . -name "Module.symvers" -exec rm -f {} ';'
	find . -name "*.cmd" -exec rm -f {} ';'
	find . -name "*.mod.c" -exec rm -f {} ';'
	-find . -name "*.ko" -exec rm -f {} ';'
	-find . -name .tmp_versions -exec rm -rf {} ';'

distclean: clean

config:
	sh configure

devinodes:	
	cd drivers ; $(MAKE) $@

cvsclean:	
	find . -name "CVS" -exec rm -rf {} ';'
