#CFLAGS=	-DDEBUG
CFLAGS+=-O6

#LPT=	/dev/parports/2
LPT=	/dev/parport0
PROG=	stk200

.phony:	clean flash reset erase

all:	avrdmx-firmware.hex

clean:
	rm -f *._c *.cof *.dis *.dbg *.dp2 *.li[st] *.lk *.ma?[kp] *.mp *.o *.s *.hex

avrdmx-monitor.o:	avrdmx-monitor.c
	avr-gcc -g -mmcu=at90s8515 $(CFLAGS) -c $<

avrdmx-firmware.o:	avrdmx-firmware.c avrdmx-monitor.o
	avr-gcc -g -mmcu=at90s8515 $(CFLAGS) -o $@ $^

avrdmx-firmware.hex:	avrdmx-firmware.o
	avr-objcopy -j .text -O ihex $< $@

avrdmx-firmware.dis:	avrdmx-firmware.o
	avr-objdump -dS $< > $@

flash:	avrdmx-firmware.hex
	uisp -dprog=$(PROG) -dlpt=$(LPT) --erase -dno-poll --upload if=$<

reset:
	uisp -dprog=$(PROG) -dlpt=$(LPT)

erase:
	uisp -dprog=$(PROG) -dlpt=$(LPT) --erase

dis:	avrdmx-firmware.dis
