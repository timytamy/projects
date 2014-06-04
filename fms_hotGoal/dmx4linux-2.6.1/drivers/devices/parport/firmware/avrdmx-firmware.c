// AVR DMX-512 for PARALLEL PORT (ECP) Ver 4.0
// COPYRIGHTS Kari Viitala,  ALL RIGHTS RESERVED
// Non commercial and personal use only 
//
// patched to Version 4.0 by Dirk Jagdmann <doj@cubic.org>
// http://llg.cubic.org

// TODO: 
//       - EPPreadI und EPPwriteI setzen
//       - ieee1284 device info
//       - break neu austimen
//       - mit gcc3.3 und der dann aktuellen libc kann man auf die cbi() und sbi() makros verzichten
//       - InBuf Size auf 128 oder mehr erhöhen, wenn gcc das endlich mal macht

#include <avr/eeprom.h>
#include <string.h>

#include "avrdmx-monitor.h"

#if defined(__GNUC__)
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/signal.h>

#define out(port, val) outp(val, port)
#define in(port) inp(port)

#define BIT(a) BV(a)

#define UART_TRANSMIT_ON()  sbi(UCR, 3);
#define UART_TRANSMIT_OFF() cbi(UCR, 3);

#define CLI() cli()
#define SEI() sei()

#define EEPROMwrite(a, d) do { while(!eeprom_is_ready()); eeprom_wb(a, d); } while(0)
#define EEPROMread(a) eeprom_rb(a)

#elif defined(__ICC__)
#include <io8515.h>
#include <macros.h>

#define sbi(port, bit) port |=  BIT(bit)
#define cbi(port, bit) port &= ~BIT(bit)

#define in(port) port
#define out(port, val) port = val

#define bit_is_set(port, bit) (in(port) & BIT(bit))
#define bit_is_clear(port, bit) (!bit_is_set(port,bit))

#define loop_until_bit_is_set(port, bit) while(bit_is_clear(port,bit))
#define loop_until_bit_is_clear(port, bit) while(bit_is_set(port,bit))

#define eeprom_read_block(buf, start, size) do { int i; for(i=start; i<start+size; i++) buf[i]=EEPROMread(i); } while(0)

#define SIGNAL(func) void func(void)

#else

#error Compiler not yet supported. Please contact authors if you have ported this source.

#endif

#define DATASTROBE      2 //(14. AUTOFEED) data strobe signal input, PORTD2
#define ADDRSTROBE	3 //(17. SELECT IN) address strobe signal input, PORTD3
#define WRITE		5 //(1. STROBE) read/~write select strobe signal input, PORTD5
#define WAIT		0 //(11. BUSY) wait signal output, PORTA0

#define BUSYON		sbi(PORTA, WAIT)
#define BUSYOFF		cbi(PORTA, WAIT)

#define LED1OFF		sbi(PORTB, 0)
#define LED1ON		cbi(PORTB, 0)

#define LED2OFF		sbi(PORTB, 1)
#define LED2ON		cbi(PORTB, 1)

#define LED3OFF		sbi(PORTB, 2)
#define LED3ON		cbi(PORTB, 2)

#define LED4OFF		sbi(PORTB, 3)
#define LED4ON		cbi(PORTB, 3)

#define TXDPIN 		1

#define PORTCIN		out(PORTC, 0); out(DDRC, 0x00)
#define PORTCOUT	out(DDRC, 0xff)

#if defined(DEBUG)
volatile unsigned char InBuf[16];
#else
volatile unsigned char InBuf[127];
#endif

volatile unsigned char OutBuf[256];

#define ReceiveEnable	0x01
#define MergeEnable	0x02
volatile unsigned char STATUS=ReceiveEnable | MergeEnable;

volatile unsigned char EPPwriteI;
volatile unsigned char EPPreadI;

int main(void)
{    	
  //////////////////// Initialize Hardware ///////////////////////////////////

  /////// Ports
  out(DDRA, 0xFF);		// PORTA bit 0-7 OUTPUT
  BUSYON;
  out(DDRB, 0xBF);		// PORTB 0-5,7 output, D6 input
  PORTCIN;
  out(DDRD, 0xC2); // PORTD D0 INPUT, D1 OUT, D2-D5 INPUT, D6-7 OUTPUT
  
  //CLEAR LEDS
  LED1OFF;
  LED2OFF;
  LED3OFF;
  LED4OFF;

  //init EXT_INT0 & EXT_INT1 as edgetriggered interrupt mode
  out(MCUCR, 0x0A);
  // out(GIMSK, 0xc0);

  //////// UART
  out(UBRR, 1);			// baudrate 250Kbps
  //init UART:
  // - 9 bit mode to emulate a second stop bit
  // - TX enable
  // - RX enable
  // - RX interrupt
  out(UCR, /* BIT(RXCIE) | */ BIT(RXEN) | BIT(TXEN) | BIT(CHR9) | 1);

  //load default scene to DMX buffer
  eeprom_read_block(OutBuf, 0, 255);
  OutBuf[255]=0;
  STATUS=EEPROMread(255);

  // clear input buffer
  memset(InBuf, 0, sizeof(InBuf));
  
  out(GIFR, 0xff);		//CLEAR INTERRUPT FLAGS
  SEI();
  BUSYOFF;

  // TX loop
  while (1)
    {
      loop_until_bit_is_set(USR, UDRE);

      // generate break
      {
	int o;
#define delay(i) for(o=0;o<i;o++) o+=10, o-=10;

	sbi(PORTD, TXDPIN);
	//delay(254);		//do not generate break too early
	UART_TRANSMIT_OFF();
	cbi(PORTD, TXDPIN);
	delay(110);		// 88us for break
	sbi(PORTD, TXDPIN);
	delay(11);		// 8us mark after break
	UART_TRANSMIT_ON();
#undef delay
      }

      // startcode
      out(UDR, 0);

      // transmit slots
      unsigned char OutBufI=0;

      if(!(STATUS & MergeEnable))
	goto dontmerge;

      // transmit output buffer merged with input buffer
      do 
	{
	  LED2ON;

	  if(OutBufI>=sizeof(InBuf))
	    goto dontmerge;

	  unsigned char o=OutBuf[OutBufI];
	  const unsigned char i=InBuf[OutBufI];

	  loop_until_bit_is_set(USR, UDRE);
	  LED2OFF;
	  out(UDR, (o>i)?o:i);
	}
      while(++OutBufI!=0);
      continue;

      // transmit output buffer
    dontmerge:
      do 
	{
	  LED2ON;
	  loop_until_bit_is_set(USR, UDRE);
	  LED2OFF;
	  out(UDR, OutBuf[OutBufI]);
	}
      while(++OutBufI!=0);

    }

  return 0;
}

// EPP data cycle
#if defined(__ICC__)
#pragma interrupt_handler SIG_INTERRUPT0:2
#endif

SIGNAL(SIG_INTERRUPT0)
{
  BUSYON;
  LED1ON;

  //if write mode
  if(bit_is_set(PIND, WRITE))
    {
      unsigned int timeout=0xffff;
      PORTCOUT;
      out(PORTC, InBuf[EPPwriteI]);
      if(EPPwriteI<sizeof(InBuf))
        EPPwriteI++;
      while(bit_is_clear(PORTD, DATASTROBE) && --timeout);
      PORTCIN;
    }
  //if read mode
  else
    {
      OutBuf[EPPreadI++]=in(PINC);
      OutBuf[0]=EPPreadI;
    }
	
  LED1OFF;
  //sbi(GIFR, 6);	//clear interrupt
  BUSYOFF;
}

// EPP addr cycle
#if defined(__ICC__)
#pragma interrupt_handler SIG_INTERRUPT1:3
#endif

SIGNAL(SIG_INTERRUPT1)
{
  BUSYON;
  LED1ON;

  // write addr
  if(bit_is_set(PIND, WRITE))
    {
      unsigned int timeout=0xffff;
      PORTCOUT;			//set porta to out
      out(PORTC, STATUS&0x80);	//write data to port
      while(bit_is_clear(PORTD, ADDRSTROBE) && --timeout);
      /* reset PORTC */
      out(PORTC, 0);
      PORTCIN;

      OutBuf[3]=27;
    }
  // read addr
  else
    {
      unsigned char temp=in(PINC);

      // enable receiver
      if((temp & 0x01) && bit_is_clear(UCR, RXEN))
	{
	  STATUS|=ReceiveEnable;
	  sbi(UCR, RXEN);
	}
      // disable receiver
      if(!(temp & 0x01))
	{
	  STATUS&=~ReceiveEnable;
	  cbi(UCR, RXEN);
	}

      // set merge
      if(temp & 0x02)
        STATUS|=MergeEnable;
      else
        STATUS&=~MergeEnable;

      // clear write counter
      if(temp & 0x10)
        EPPwriteI=0;
	  
      // clear read counter
      if(temp & 0x20)
        EPPreadI=0;
	  
      //write default scene	  
      if(temp & 0x40)	
	{
	  int i;
	  cbi(UCR, RXEN);	//disable receiver
          for(i=0; i<255; i++)
	    EEPROMwrite(i, OutBuf[i]);
	  EEPROMwrite(255, STATUS);
	  sbi(UCR, RXEN);	//enable receiver
	}

      //clear counters
      if(temp & 0x80)
        EPPwriteI=EPPreadI=0;	
    }
		
  LED1OFF;
  //sbi(GIFR, 7);			//clear interrupt
  BUSYOFF;
}

// UART RX
#if defined(__ICC__)
#pragma interrupt_handler SIG_UART_RECV:10
#endif

SIGNAL(SIG_UART_RECV)
{
  static signed int InBufI;
  unsigned char temp=in(UDR);

  // overrun
  if(bit_is_set(USR, 3))
    {
      //cbi(USR, 3);		//CLEAR OR
      LED3OFF;
      InBufI=-2;
    }

  //IF framing error and ninth data bit is low then BREAK was detected	
  else if(bit_is_set(USR,FE) && bit_is_clear(UCR,RXB8))
    {
      LED3OFF;
      InBufI=-1;
    }
    
  //DETECT valid start code
  else if(InBufI==-1)
    {
      if(temp == 0)
	{
	  LED3ON; 
	  InBufI=0;
	}
      else
	{
	  LED3OFF;
	  InBufI=-2;
	}
    }

  //RECEIVE DATA
  else if(InBufI>=0 && InBufI<sizeof(InBuf))
    InBuf[InBufI++]=temp;

  // receive buffer is filled
  else
    LED3OFF;
}
