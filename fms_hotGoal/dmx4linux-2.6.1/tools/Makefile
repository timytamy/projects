-include /tmp/dmxconfig.mk

BININSTALLPATH=$(DMXPREFIX)/bin
MANINSTALLPATH=$(DMXPREFIX)/man/man1

DMXCONSOLELIBS+= -Lpointer pointer/pointer.a
CFLAGS+= -Ipointer

ifdef CONFIG_HAVE_LIRC
CFLAGS+=	-DHAVE_LIRC=1
DMXCONSOLELIBS+=-llirc_client
endif

ifdef CONFIG_HAVE_GPM
DMXCONSOLELIBS+= -lgpm
endif

TARGETS=	dmxtest midi2dmx uart_dmxd dmxconsole
MANPAGES=	dmxtest.1 dmxconsole.1 midi2dmx.1

all:	$(TARGETS)

dmxconsole : dmxconsole.o
	$(MAKE) -C pointer all
	$(CC) $(LDFLAGS) -o $@ $< -lncurses -ldmx4l $(DMXCONSOLELIBS)

uart_dmxd: uart_dmxd.o
	$(CC) $(LDFLAGS) -o $@ $< -ldmx4l

dmxtest: dmxtest.o
	$(CC) $(LDFLAGS) -o $@ $< -ldmx4l

midi2dmx: midi2dmx.o
	$(CC) $(LDFLAGS) -o $@ $< -ldmx4l

clean:
	$(RM) $(TARGETS) *.o *.chn
	$(MAKE) -C as31-unix $@
	$(MAKE) -C pointer $@

distclean: uninstall clean

uninstall:
	cd $(BININSTALLPATH) && $(RM) $(TARGETS)
	cd $(MANINSTALLPATH) && $(RM) $(MANPAGES)
	$(RM) /etc/bash_completion.d/dmx4linux

install:	$(TARGETS) $(MANPAGES)
	mkdir -p $(BININSTALLPATH)
	install -m 755 $(TARGETS) $(BININSTALLPATH)
	mkdir -p $(MANINSTALLPATH)
	install -m 644 $(MANPAGES) $(MANINSTALLPATH)
	if [ -d /etc/bash_completion.d ]; then install -m 755 bash_completion.sh /etc/bash_completion.d/dmx4linux; fi
