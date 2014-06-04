CC=gcc
CFLAGS=-Wall -g -c -std=c99 -fprofile-arcs -ftest-coverage
LDFLAGS=-fprofile-arcs -ftest-coverage
MAKEFLAGS += -r
#CFLAGS=-Wall  -fmudflap  -g -c -std=c99
#LDFLAGS=-lmudflap -rdynamic


SOURCES=usb_pro_example.cpp PRO_EXAMPLE.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLES=PRO_EXAMPLE usb_pro_example

#this makes sure that typing 'make' will execute 'make all'
.default: all

%.o: %.c
	$(CC) $(CFLAGS)  $< 
	
usb_pro_example: usb_pro_example.o
	$(CC) $(LDFLAGS)  -o $@ $+

PRO_EXAMPLE: PRO_EXAMPLE.o
	$(CC) $(LDFLAGS)  -o $@ $+

all: $(OBJECTS) $(EXECUTABLES)

clean:
	rm *.o $(EXECUTABLES)
