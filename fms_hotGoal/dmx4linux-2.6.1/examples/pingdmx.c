/*
 * dmxping.c
 * generates some test-fades to dmx.
 *
 * Copyright (C) Michael Stickel <michael@cubic.org>
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <dmx/dmx.h>

int direct = 0;
static dmx_t dmx[512] = {0};

void usage ()
{
  printf ("pingdmx [options]\n");
  printf ("--dmx <DMX-devicename>  specify DMX-device\n");
  printf ("  -c  <channel-count>   number of channels to output\n");
  printf ("  -t  <delay (ms)>      delay between two channel updates\n");
  printf ("  -l  <lower-value>     value to begin (low)\n");
  printf ("  -u  <upper-value>     value to stop (upper)\n");
  printf ("  -v                    output debug information\n");
  printf ("  -s                    silence (don't do any output)\n");
}

int main (int argc, const char **argv)
{
  const char *dmxname = DMXdev(&argc, argv);
  int   chanz   = 100;          /* channel count        -c <ch-count> */
  int   tdelay  = 10;		/* delay betw. update (1s) -t <delay> */
  int   lval    = 0;		/* lower value         -l <value>  */
  int   uval    = 255;          /* upper value         -u <value>  */
  char  vflag   = 0;
  char  sflag   = 0;
  int   c=0;
  int   fd;

  while(c!=EOF)
    {
      c=getopt(argc, (char**)argv, "c:l:hst:u:v");
      switch (c)
	{
	case 'v': vflag=1; break;
	case 's': sflag=1; break; /* silence : don't output anything */
	case 'c': chanz = atoi(optarg); break;
	case 't': tdelay = atoi(optarg); break;
	case 'l': lval = atoi(optarg); break;
	case 'u': uval = atoi(optarg); break;
	case '?':
	case 'h': usage (); return 1;
	}
    }

  if (lval>uval)
    {
      int t=lval; lval=uval; uval=t;
      if (!sflag)
        printf (" lower-value > upper-value :: swapping them\n");
    }
  if (lval<0)   lval=0;
  if (uval>255) uval=255;

  if (chanz > 512)
    {
      if (!sflag)
	printf (" channel-count > 512 :: setting it to 512\n");
      chanz = 512;
    }

  if (vflag && !sflag)
    {
      printf (" dmx-dev...: %s\n", dmxname);
      printf (" ch-anz....: %d\n", chanz);
      printf (" delay.....: %d (ms)\n", tdelay);
      printf (" lower-val.: %d\n", lval);
      printf (" upper-val.: %d\n", uval);
    }

  fd = open (dmxname, O_WRONLY);
  if (fd!=-1)
    {
      int val;
      for (val=lval; val<=uval; val++)
	{
	  int i;
	  for (i=0; i<chanz && i<512; i++)
	    dmx[i]=val;
	  lseek (fd, 0, SEEK_SET);
	  write (fd, dmx, (chanz<=512)?chanz:512);
	  DMXsleep(tdelay*1000);
	}
      close (fd);
    }
  return 0;
}
