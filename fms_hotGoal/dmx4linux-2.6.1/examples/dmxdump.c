/*
 * dumpdmx
 * dumps all input slots
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>

#include <dmx/dmx.h>
#include <dmx/dmxioctl.h>

int main (int argc, const char **argv)
{
  int MAX=-1;
  unsigned char *buf;
  struct dmx_info info;
  const char * dmxname = DMXINdev(&argc, argv);
  int dmxfd=open(dmxname, O_RDONLY);
  if(dmxfd<0)
    {
      fprintf(stderr, "could not open %s : %s\n", dmxname, strerror(errno));
      return 1;
    }

  /* get number of universes */
  if(ioctl(dmxfd, DMX_IOCTL_GET_INFO, &info) >= 0)
    {
      MAX=info.used_out_universes;
      if(info.used_in_universes > MAX)
	MAX=info.used_in_universes;
    }

  if(MAX<1)
    {
      fprintf(stderr, "no universes\n");
      return 1;
    }

  MAX*=512;
  buf=(unsigned char*)calloc(MAX,0);
  if(!buf)
    {
      perror("could not alloc");
      return 1;
    }

  printf ("numslots=%d\n", MAX);
  while(1)
    {
      int i, lastval, lasti=0;

      fd_set rfds;
      int retval;
      FD_ZERO(&rfds);
      FD_SET(dmxfd, &rfds);
      retval = select(dmxfd+1, &rfds, NULL, NULL, NULL);
      if (retval == -1)
	{
	  perror("select()");
	  return 1;
	}
      if(retval == 0)
	{
	  fprintf(stderr, "select() timeout. this should not happen\n");
	  return 1;
	}

      lseek(dmxfd, 0, SEEK_SET);
      if(read(dmxfd, buf, MAX) < 0)
	return 1;

      lastval=buf[0];
      for (i=1; i<MAX; i++)
        {
	  if(buf[i]!=lastval)
	    {
	      if(lasti==i-1)
                printf ("[%d]=%02X ", lasti, lastval);
	      else
                printf ("[%d..%d]=%02X ", lasti, i-1, lastval);
	      lastval=buf[i];
	      lasti=i;
	    }
        }
      if(lasti==MAX-1)
	printf ("[%d]=%02X\n", lasti, lastval);
      else
	printf ("[%d..%d]=%02X\n", lasti, i-1, lastval);
    }

  return 0;
}
