#ifndef MONITOR__H
#define MONITOR__H 1

#if defined(__GNUC__) && defined(DEBUG)
#include <avr/pgmspace.h>

#define TRAP() asm volatile ("rcall gdb_break")

void SendByte(uint8_t data);
void sendprgstr(prog_char *s);

#define DTRAP() TRAP()
#define DPRINT(str) sendprgstr(PSTR(str))

#else
#define DTRAP()
#define DPRINT(str)
#endif

#endif
