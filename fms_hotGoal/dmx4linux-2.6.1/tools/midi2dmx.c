/*
 * Copyright (C) Dirk Jagdmann <doj@cubic.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/* TODO: - midi channel beachten */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dmx/dmx.h>

#define true 1
#define false 0
typedef int bool;

char* noteStr[] = {
  "C-0","C#0","D-0","D#0","E-0","F-0","F#0","G-0","G#0","A-0","A#0","B-0",
  "C-1","C#1","D-1","D#1","E-1","F-1","F#1","G-1","G#1","A-1","A#1","B-1",
  "C-2","C#2","D-2","D#2","E-2","F-2","F#2","G-2","G#2","A-2","A#2","B-2",
  "C-3","C#3","D-3","D#3","E-3","F-3","F#3","G-3","G#3","A-3","A#3","B-3",
  "C-4","C#4","D-4","D#4","E-4","F-4","F#4","G-4","G#4","A-4","A#4","B-4",
  "C-5","C#5","D-5","D#5","E-5","F-5","F#5","G-5","G#5","A-5","A#5","B-5",
  "C-6","C#6","D-6","D#6","E-6","F-6","F#6","G-6","G#6","A-6","A#6","B-6",
  "C-7","C#7","D-7","D#7","E-7","F-7","F#7","G-7","G#7","A-7","A#7","B-7",
  "C-8","C#8","D-8","D#8","E-8","F-8","F#8","G-8","G#8","A-8","A#8","B-8",
  "C-9","C#9","D-9","D#9","E-9","F-9","F#9","G-9","G#9","A-9","A#9","B-9",
  "C-A","C#A","D-A","D#A","E-A","F-A","F#A","G-A","G#A","A-A","A#A","B-A",
};

char* controllerStr[] = {
  "Bank Select MSB",
  "Modulation Wheel MSB",
  "Breath Control MSB",
  "Undefined 003",
  "Foot Controller MSB",
  "Portamento Time MSB",
  "Data Entry MSB",
  "Channel Volume MSB",

  "Ballance MSB",
  "Undefined 009",
  "Pan MSB",
  "Expression Controller MSB",
  "Effect Control 1 MSB",
  "Effect Control 2 MSB",
  "Undefined 014",
  "Undefined 015",

  "General Purpose Controller #1 MSB",
  "General Purpose Controller #2 MSB",
  "General Purpose Controller #3 MSB",
  "General Purpose Controller #4 MSB",
  "Undefined 020",
  "Undefined 021",
  "Undefined 022",
  "Undefined 023",
  "Undefined 024",
  "Undefined 025",
  "Undefined 026",
  "Undefined 027",
  "Undefined 028",
  "Undefined 029",
  "Undefined 030",
  "Undefined 031",

  "Bank Select LSB",
  "Modulation Wheel LSB",
  "Breath Control LSB",
  "Undefined 035",
  "Foot Controller LSB",
  "Portamento Time LSB",
  "Data Entry LSB",
  "Channel Volume LSB",

  "Ballance LSB",
  "Undefined 041",
  "Pan LSB",
  "Expression Controller LSB",
  "Effect Control 1 LSB",
  "Effect Control 2 LSB",
  "Undefined 046",
  "Undefined 047",

  "General Purpose Controller #1 LSB",
  "General Purpose Controller #2 LSB",
  "General Purpose Controller #3 LSB",
  "General Purpose Controller #4 LSB",
  "Undefined 052",
  "Undefined 053",
  "Undefined 054",
  "Undefined 055",
  "Undefined 056",
  "Undefined 057",
  "Undefined 058",
  "Undefined 059",
  "Undefined 060",
  "Undefined 061",
  "Undefined 062",
  "Undefined 063",

  "Sustain On/Off",
  "Portamento On/Off",
  "Sustenuto On/Off",
  "Soft Pedal On/Off",
  "Legato Footswitch",
  "Hold 2",

  "Sound Controller 1 (Sound Variation) LSB",
  "Sound Controller 2 (Timbre) LSB",
  "Sound Controller 3 (Release Time) LSB",
  "Sound Controller 4 (Attack Time) LSB",
  "Sound Controller 5 (Brightness) LSB",
  "Sound Controller 6 LSB",
  "Sound Controller 7 LSB",
  "Sound Controller 8 LSB",
  "Sound Controller 9 LSB",
  "Sound Controller 10 LSB",
  "General Purpose Controller #5 LSB",
  "General Purpose Controller #6 LSB",
  "General Purpose Controller #7 LSB",
  "General Purpose Controller #8 LSB",
  "Portamento Control Source No",
  "Undefined 085",
  "Undefined 086",
  "Undefined 087",
  "Undefined 088",
  "Undefined 089",
  "Undefined 090",
  "Effects 1 Depth LSB",
  "Effects 2 Depth LSB",
  "Effects 3 Depth LSB",
  "Effects 4 Depth LSB",
  "Effects 5 Depth LSB",

  "Data Entry +1",
  "Data Entry -1",
  "Non-Registred Parameter Number LSB",
  "Non-Registred Parameter Number MSB",
  "Undefined 102",
  "Undefined 103",
  "Undefined 104",
  "Undefined 105",
  "Undefined 106",
  "Undefined 107",
  "Undefined 108",
  "Undefined 109",
  "Undefined 110",
  "Undefined 111",
  "Undefined 112",
  "Undefined 113",
  "Undefined 114",
  "Undefined 115",
  "Undefined 116",
  "Undefined 117",
  "Undefined 118",
  "Undefined 119",

  "All Sound Off",
  "Reset All Controllers",
  "Local Control On/Off",

  "All Notes Off",
  "Omni Mode Off",
  "Omni Mode On",
  "Poly Mode On/Off",
  "Poly Mode On",
};

int fadeval=0;

int slot_len=0;
dmx_t *slot=0, *note=0, *controller=0;

int noteoffset=0, controlleroffset=0;

int midifh=-1, dmxfh=-1;

void cleanup()
{
  if(midifh>=0)
    close(midifh), midifh=-1;
  if(dmxfh>=0)
    close(dmxfh), dmxfh=-1;
  if(slot)
    free(slot), slot=0;
  if(note)
    free(note), note=0;
  if(controller)
    free(controller), controller=0;
}

unsigned char MIDIget()
{
  unsigned char u;
  size_t r;
  assert(midifh>=0);
  r=read(midifh, &u, 1);
  if(r==0)
    {
      fprintf(stderr, "could not read a byte\n");
      exit(1);
    }
  return u;
}

void DMXput(int o)
{
  assert(dmxfh>=0);

  if(o==-1)
    {
      int i;
      for(i=0; i<slot_len; i++)
	{
	  int v=note[i];
	  if(controller[i]>v)
	    v=controller[i];
	  v+=fadeval;
	  if(v<0)
	    v=0;
	  else if(v>255)
	    v=255;
	  slot[i]=v;
	}
      lseek(dmxfh, 0, SEEK_SET);
      write(dmxfh, slot, slot_len);
    }
  else if(o>=0 && o<slot_len)
    {
      dmx_t v=note[o];
      if(controller[o]>v)
	v=controller[o];
      lseek(dmxfh, o, SEEK_SET);
      write(dmxfh, &v, 1);
    }
}

void note_put(int o, int v)
{
  if(v<0)
    v=0;
  else if(v>255)
    v=255;

  o+=noteoffset;
  if(o>=0 && o<slot_len)
    {
      note[o]=v;
      DMXput(o);
    }
}

void controller_put(int o, int v)
{
  if(v<0)
    v=0;
  else if(v>255)
    v=255;

  o+=controlleroffset;
  if(o>=0 && o<slot_len)
    {
      controller[o]=v;
      DMXput(o);
    }
}

void reset()
{
  assert(dmxfh>=0);
  assert(slot_len>0);

  memset(slot, 0, slot_len);
  memset(note, 0, slot_len);
  memset(controller, 0, slot_len);
  lseek(dmxfh, 0, SEEK_SET);
  write(dmxfh, slot, slot_len);
  fadeval=0;
}

void help()
{
  printf("midi2dmx: convert midi messages to dmx slots\n");
  printf("params: --dmx <device>   use device for dmx output. default /dev/dmx, /dev/misc/dmx\n");
  printf("        --midi <device>   use device for midi input. default /dev/midi\n");
  printf("        --noteoffset <int>   add int to every note\n");
  printf("        --controlleroffset <int>   add int to every controller\n");
  exit(0);
}

int main(int argc, const char **argv)
{
  const char *dmxdevice=0, *mididevice="/dev/midi", *arg=0;
  bool flag=true, sysex=false;
  int sysexcount=0;

  atexit(cleanup);

  /* help  */
  if(DMXgetarg(&argc, argv, "--help", 0, 0)==0
     || DMXgetarg(&argc, argv, "-h", 0, 0)==0)
    help();

  /* open DMX */
  dmxdevice=DMXdev(&argc, argv);
  if(dmxdevice==0)
    {
      fprintf(stderr, "could not determine dmx device\n");
      return 1;
    }
  dmxfh=open(dmxdevice, O_WRONLY);
  if(dmxfh<0)
    {
      fprintf(stderr, "could not open %s:%s\n", dmxdevice, strerror(errno));
      return 1;
    }

  /* open MIDI */
  DMXgetarg(&argc, argv, "--midi", 1, &mididevice);
  if(mididevice==0)
    {
      fprintf(stderr, "could not determine midi device\n");
      return 1;
    }
  midifh=open(mididevice, O_RDONLY);
  if(midifh<0)
    {
      fprintf(stderr, "could not open %s:%s\n", mididevice, strerror(errno));
      return 1;
    }

  /* get noteoffset */
  arg=0;
  DMXgetarg(&argc, argv, "--noteoffset", 1, &arg);
  if(arg)
    noteoffset=atoi(arg);

  /* get controlleroffset */
  arg=0;
  DMXgetarg(&argc, argv, "--controlleroffset", 1, &arg);
  if(arg)
    controlleroffset=atoi(arg);

  /* alloc */
  slot_len=((controlleroffset>noteoffset)?controlleroffset:noteoffset)+128;
  if(slot_len<1)
    {
      fprintf(stderr, "noteoffset and controlleroffset are too negative\n");
      return 1;
    }
  slot=malloc(slot_len);
  if(slot==0)
    {
      fprintf(stderr, "could not alloc memory for slot\n");
      return 1;
    }
  note=malloc(slot_len);
  if(note==0)
    {
      fprintf(stderr, "could not alloc memory for note\n");
      return 1;
    }
  controller=malloc(slot_len);
  if(controller==0)
    {
      fprintf(stderr, "could not alloc memory for controller\n");
      return 1;
    }

  reset();

  /* start processing */
  while(flag)
    {
      static int lastswitchval=0;
      int switchval;
      int channel;
      unsigned char key=0, value=0;
      unsigned char uc=MIDIget();

      if(uc==0xf7)
	{
	sysexend:
	  sysex=false;
	  printf("Received %i sysex bytes\n", sysexcount);
	  continue;
	}

    startofcommand: ;

      channel=uc&0xF;
      key=0;
      value=0;
      switchval=uc>>4;
      if(switchval<0x8)
	switchval=lastswitchval;
      lastswitchval=switchval;
      switch(switchval)
	{
	default: break;

	  /* Note Off event */
	case 0x8:
	  key=MIDIget();
	  if(key&0x80)
	    {
	      uc=key;
	      goto startofcommand;
	    }

	  value=MIDIget();
	  if(value&0x80)
	    {
	      uc=value;
	      goto startofcommand;
	    }

	  printf("Note-Off ch%02i i%s %i\n", channel, noteStr[key], value);
	  note_put(key, 0);
	  break;

	  /* Note On event */
	case 0x9:
	  key=MIDIget();
	  if(key&0x80)
	    {
	      uc=key;
	      goto startofcommand;
	    }

	  value=MIDIget();
	  if(value&0x80)
	    {
	      uc=value;
	      goto startofcommand;
	    }

	  if(value==0)
	    {
	      printf("Note-Off ch%02i %s\n", channel, noteStr[key]);
	      note_put(key, 0);
	    }
	  else
	    {
	      printf("Note-On  ch%02i %s %i\n", channel, noteStr[key], value);
	      note_put(key, value*2);
	    }
	  break;

	  /* Polyphonic key pressure / after touch */
	case 0xA:
	  key=MIDIget();
	  if(key&0x80)
	    {
	      uc=key;
	      goto startofcommand;
	    }

	  value=MIDIget();
	  if(value&0x80)
	    {
	      uc=value;
	      goto startofcommand;
	    }

	  printf("Key-AfterTouch ch%02i %s %i\n", channel, noteStr[key], value);
	  note_put(key, value*2);
	  break;

	  /* Control Change */
	case 0xB:
	  key=MIDIget();
	  if(key&0x80)
	    {
	      uc=key;
	      goto startofcommand;
	    }

	  value=MIDIget();
	  if(value&0x80)
	    {
	      uc=value;
	      goto startofcommand;
	    }

	  switch(key)
	    {
	    default:
	      printf("Control-Change ch%02i %s=%i\n", channel, controllerStr[key], value);
	      break;
#if 0
	    case 122:
	      printf("Local Control ch%02i %s\n", channel, value?"On":"Off");
	      break;

	    case 123:
	      printf("All Notes Off ch%02i\n", channel);
	      break;

	    case 124:
	      printf("Omni Mode Off ch%02i\n", channel);
	      break;

	    case 125:
	      printf("Omni Mode On ch%02i\n", channel);
	      break;

	    case 126:
	      printf("Mono Mode On with %i voices ch%02i\n", value, channel);
	      break;

	    case 127:
	      printf("Poly Mode Off ch%02i\n", channel);
	      if(value==0)
		printf("All Notes Off ch%02i\n", channel);
	      break;
#endif
	    }

	  controller_put(key, value*2);
	  break;

	  /* Program Change */
	case 0xC:
	  key=MIDIget();
	  if(key&0x80)
	    {
	      uc=key;
	      goto startofcommand;
	    }

	  printf("Program-Change ch%02i %i\n", channel, value);
	  break;

	  /* Channel pressure / after touch */
	case 0xD:
	  key=MIDIget();
	  if(key&0x80)
	    {
	      uc=key;
	      goto startofcommand;
	    }

	  printf("Channel-AfterTouch ch%02i %i\n", channel, key);
	  break;

	  /* Pitch bend messages */
	case 0xE:
	  key=MIDIget();
	  if(key&0x80)
	    {
	      uc=key;
	      goto startofcommand;
	    }

	  value=MIDIget();
	  if(value&0x80)
	    {
	      uc=value;
	      goto startofcommand;
	    }

	  {
	    int lsb=key, msb=value;
	    msb<<=7;
	    lsb|=msb;
	    printf("Bitch-Bend ch%02i %i\n", channel, lsb);

	    fadeval=lsb/32-256;
	    DMXput(-1);
	  }
	  break;

	  /* System Exclusive */
	case 0xF:

	  switch(channel)
	    {
	      /* Start of SysEx Transmission */
	    case 0:
	      sysex=true;
	      sysexcount=0;
	      break;

	      /* Song Position Pointer */
	    case 2:
	      key=MIDIget();
	      if(key&0x80)
		{
		  uc=key;
		  goto startofcommand;
		}

	      value=MIDIget();
	      if(value&0x80)
		{
		  uc=value;
		  goto startofcommand;
		}

	      {
		int lsb=key, msb=value;
		msb<<=7;
		lsb|=msb;
		printf("Song Position Pointer %i\n", lsb);
	      }
	      break;

	    case 3:
	      key=MIDIget();
	      if(key&0x80)
		{
		  uc=key;
		  goto startofcommand;
		}

	      printf("Song Select %i\n", key);
	      break;

	    case 6:
	      printf("Tune Request\n");
	      break;

	    case 7:
	      goto sysexend;

	    case 1:
	    case 4:
	    case 5:
	    case 9:
	    case 0xD:
		printf("Undefined Command\n");
	      break;

	    case 0x8:
	      /*printf("Timing Clock\n");*/
	      break;

	    case 0xA:
	      printf("Start\n");
	      break;

	    case 0xB:
	      printf("Continue\n");
	      break;

	    case 0xC:
	      printf("Stop\n");
	      break;

	    case 0xE:
	      /*printf("Active Sensing\n");*/
	      break;

	    case 0xF:
	      printf("System Reset\n");
	      reset();
	      break;
	    }

	  break;
	}
    }

  return 0;
}
