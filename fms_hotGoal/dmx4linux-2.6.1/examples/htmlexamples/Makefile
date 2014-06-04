-include /tmp/dmxconfig.mk

TARGETS=	nonblockread selectread simpleread simplewrite

CFLAGS+=	-Wall

all:	$(TARGETS)

nonblockread:	nonblockread.o
	$(CC) $(LDFLAGS) -o $@ $^ -ldmx4l

selectread:	selectread.o
	$(CC) $(LDFLAGS) -o $@ $^ -ldmx4l

simpleread:	simpleread.o
	$(CC) $(LDFLAGS) -o $@ $^ -ldmx4l

simplewrite:	simplewrite.o
	$(CC) $(LDFLAGS) -o $@ $^ -ldmx4l

clean :
	rm -f $(TARGETS) *.o

distclean: clean uninstall

uninstall:

install:
