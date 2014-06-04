/*
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

#include <ctype.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <dmx/dmx.h>
#include <dmx/dmxioctl.h>

#define BUFFERSIZE (512*1)
dmx_t buffer[BUFFERSIZE];
dmx_t buffer2[BUFFERSIZE];

const char *dmxfilename=0;
const char *dmxinfilename=0;

int Reader()
{
  fd_set rfds;
  int retval;
  bool run=true;
  int fh=open(dmxinfilename, O_RDONLY);
  if(fh<0)
    {
      perror(dmxinfilename);
      return -1;
    }

  while(run)
    {
      printf("\rReceiving: "); fflush(stdout);
      FD_ZERO(&rfds);
      FD_SET(0, &rfds);
      FD_SET(fh, &rfds);

      retval=select(fh+1, &rfds, NULL, NULL, NULL);
      if(retval)
	{
	  if(FD_ISSET(0, &rfds))
	    {
	      char buf[80];
	      fgets(buf, sizeof(buf), stdin);
	      switch(tolower(buf[0]))
		{
		case 'q': run=false; break;
		default:
		  printf("%s: unknown command\n", buf);
		}
	    }
	  if(FD_ISSET(fh, &rfds))
	    {
	      int r,i;
	      if(lseek(fh, 0, SEEK_SET)!=0)
		{
		  perror(dmxinfilename);
		  goto ReaderExit;
		}
	      r=read(fh, buffer2, BUFFERSIZE);
	      for(i=0; i<r; i++)
		if(buffer[i]!=buffer2[i])
		  break;
	      printf("read %04i slots, %04i slots are equal", r, i); fflush(stdout);
	    }
	}
      else if(retval==0)
	{
	  printf("timeout?\n");
	  run=false;
	}
      else
	{
	  perror("Reader: select error");
	  run=false;
	}
    }

 ReaderExit:
  if(close(fh) < 0)
    perror(dmxinfilename);

  return 0;
}

int Writer()
{
  fd_set rfds;
  struct timeval tv;
  int retval, mode=1, lastmode=!mode;
  bool run=true;
  int fh=open(dmxfilename, O_WRONLY);
  if(fh<0)
    {
      perror(dmxfilename);
      return -1;
    }

  while(run)
    {
      if(lastmode!=mode)
	{
	  memset (buffer, 0, sizeof(buffer));
	  if (write(fh, buffer, BUFFERSIZE) != BUFFERSIZE)
	    perror("failed to blackout channels");
	  lastmode=mode;
	}

      printf("\rSending in Mode %i ", mode); fflush(stdout);

      FD_ZERO(&rfds);
      FD_SET(0, &rfds);
      tv.tv_sec = 0;
      tv.tv_usec = 10000;

      retval=select(1, &rfds, NULL, NULL, &tv);
      if(retval)
	{
	  if(FD_ISSET(0, &rfds))
	    {
	      char buf[80];
	      fgets(buf, sizeof(buf), stdin);
	      switch(tolower(buf[0]))
		{
		case 'q': run=false; break;
		case '1':
		case '2':
		case '3':
		  mode=buf[0]-'0';
		  break;
		default:
		  printf("%s: unknown command\n", buf);
		}
	    }

	}
      else if(retval==0)
	{
	  /* timeout or signal */
	  switch(mode)
	    {
	    case 1:
	      printf("512 slots flush ");
	      if(lseek(fh, 0, SEEK_SET)!=0)
		{
		  perror(dmxfilename);
		  goto WriterExit;
		}
	      if(write(fh, buffer, BUFFERSIZE) != BUFFERSIZE)
		printf("could not send %i slots ", BUFFERSIZE);
	      break;
	    case 2:
	      {
		static int lastslot=0;
		printf("1 slot increasing ");
		if(lseek(fh, lastslot, SEEK_SET)!=lastslot)
		  {
		    perror(dmxfilename);
		    goto WriterExit;
		  }
		if(write(fh, buffer+lastslot, 1)!=1)
		  printf("could not send 1 slot at position %i ", lastslot);
		lastslot++;
		lastslot%=BUFFERSIZE;
	      }
	      break;
	    case 3:
	      {
		const int size=13;
		static int lastslot=0;
		printf("%i slots increasing ", size);
		if(lseek(fh, lastslot, SEEK_SET)!=lastslot)
		  {
		    perror(dmxfilename);
		    goto WriterExit;
		  }
		if(write(fh, buffer+lastslot, size)!=size)
		  printf("could not send %i slots at position %i ", size, lastslot);
		lastslot+=size;
		if(lastslot>=BUFFERSIZE)
		  lastslot=0;
	      }
	      break;
	    default:
	      printf("\nWriter: unknown command.\n");
	    }
	  fflush(stdout);
	}
      else
	{
	  perror("Writer: select error");
	  run=false;
	}
    }

 WriterExit:
  if(close(fh) < 0)
    perror(dmxfilename);

  return 0;
}

void help()
{
  printf("usage: dmxtest [-r] [-s] [-f <filename>]\n");
  printf("param: -r  run dmxtest in receiver mode\n");
  printf("       -s  run dmxtest in sender mode\n");
  printf("       -f  use contents of <filename> for transmission\n");
  printf("dmxtest is normally operating in receiver mode\n");
  exit(1);
}

int main(int argc, const char **argv)
{
  const char *filename="/bin/bash";
  const char *string=0;
  bool reader=true;
  FILE *f=0;

  if(DMXgetarg(&argc, argv, "-h", 0, 0)==0
     || DMXgetarg(&argc, argv, "-?", 0, 0)==0
     || DMXgetarg(&argc, argv, "--help", 0, 0)==0
     )
    help();

  if(DMXgetarg(&argc, argv, "-r", 0, 0)==0)
    reader=true;

  if((DMXgetarg(&argc, argv, "-s", 0, 0)==0) || (DMXgetarg(&argc, argv, "-w", 0, 0)==0))
    reader=false;

  if(DMXgetarg(&argc, argv, "-f", 1, &string) > 0)
    filename=string;

  dmxfilename=DMXdev(&argc, argv);
  dmxinfilename=DMXINdev(&argc, argv);

  f=fopen(filename, "rb");
  if(f==0)
    {
      perror(filename);
      return 2;
    }

  if(fread(buffer, BUFFERSIZE, 1, f) != 1)
    {
      perror(filename);
      return 2;
    }

  if(fclose(f) < 0)
    perror(filename);

  if(reader)
    return -Reader();
  else
    return -Writer();

  return 0;
}
