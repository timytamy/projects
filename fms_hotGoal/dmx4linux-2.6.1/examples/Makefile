# Makefile for the DMX-Device created by Michael Stickel
# (C)1997-2006 by Michael Stickel
#
# author: Michael Stickel
# email:  michael@cubic.org

-include /tmp/dmxconfig.mk

TARGETS=	pingdmx setdmx dmxinfo dmxdump

all:	$(TARGETS)
	$(MAKE) -C htmlexamples $@

pingdmx:	pingdmx.o
	$(CC) $(LDFLAGS) -o $@ $^ -ldmx4l

setdmx:	setdmx.o
	$(CC) $(LDFLAGS) -o $@ $^ -ldmx4l

dmxinfo : dmxinfo.o
	$(CC) $(LDFLAGS) -o $@ $^ -ldmx4l

dmxdump : dmxdump.o
	$(CC) $(LDFLAGS) -o $@ $^ -ldmx4l

clean :
	rm -f $(TARGETS) *.o
	$(MAKE) -C htmlexamples $@

distclean: clean uninstall

uninstall:

install:
