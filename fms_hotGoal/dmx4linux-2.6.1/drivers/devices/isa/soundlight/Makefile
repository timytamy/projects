-include /tmp/dmxconfig.mk

obj-m	:= dmxsoundlight.o

ifneq ($(KERNELRELEASE),)
dmxsoundlight-objs=soundlight.o autoprobe.o slh_general.o slh1512a.o slh1512b.o slh1512c.o card_access.o
EXTRA_CFLAGS:=-DDMXVERSION=\"${DMXVERSION}\"

$(src)/%.h : $(src)/%.asm
	$(AS31)/as31 -Fbin -s $< | sh $(src)/bin2hex.sh $< > $@

else

PWD	:= $(shell pwd)
KDIR	?= /lib/modules/$(shell uname -r)/build
KVER	?= $(shell uname -r)
obj-ko	=  $(obj-m:%.o=%.ko)

all:	modules

clean modules modules_install:
	$(MAKE) -C $(AS31) $@
	$(MAKE) -C $(KDIR) M=$(PWD) $@
	$(RM) slhdmx12a.h slhdmx16.h slhdmx17.h slhcheckirq.h slhcheckisr.h slhcheck.h

distclean: clean uninstall

uninstall:
	cd $(MODULEINSTALLPATH); rm -f $(obj-ko)

endif

$(obj)/autoprobe.o:	$(src)/slhcheckirq.h $(src)/slhcheckisr.h $(src)/slhcheck.h
$(obj)/slh1512a.o:	$(src)/slhdmx12a.h
$(obj)/slh1512b.o:	$(src)/slhdmx16.h
$(obj)/slh1512c.o:	$(src)/slhdmx17.h
