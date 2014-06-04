-include /tmp/dmxconfig.mk

LIBINSTALLPATH=$(DMXPREFIX)/lib
HEADERINSTALLPATH=$(DMXPREFIX)/include/dmx

OBJ=	open.o sleep.o
LOBJ=	$(OBJ:%.o=%.lo)

STATIC=	libdmx4l.a

STEMNAME=libdmx4l.so
SONAME=$(STEMNAME).2
LIBNAME=$(SONAME).5

all:	$(STATIC) $(LIBNAME)

$(STATIC):	$(OBJ)
	ar rcs $@ $^

$(LIBNAME):	$(LOBJ)
	$(CC) -shared -Wl,-soname,$(SONAME) -o $@ $^ -lc
	ln -sf $@ $(SONAME)
	ln -sf $(SONAME) $(STEMNAME)

install:	all
	install -m 644 $(STATIC) $(LIBINSTALLPATH)
	install -m 755 $(LIBNAME) $(LIBINSTALLPATH)
	ldconfig
	if [ ! -d $(HEADERINSTALLPATH) ]; then mkdir -p $(HEADERINSTALLPATH); fi
	install -m 644 ../include/dmx/dmx.h $(HEADERINSTALLPATH)
	install -m 644 ../include/dmx/dmxioctl.h $(HEADERINSTALLPATH)

clean:	
	rm -f $(OBJ) $(LOBJ) $(STATIC) $(STEMNAME) $(SONAME) $(LIBNAME) *~

distclean:	clean uninstall

uninstall:
	rm -f $(LIBINSTALLPATH)/$(STATIC)
	rm -f $(LIBINSTALLPATH)/$(LIBNAME)
	rm -rf $(HEADERINSTALLPATH)

doxy:	
	doxygen Doxyfile

%.lo:	%.c
	$(CC) $(CFLAGS) -fPIC -c -o $@ $<
