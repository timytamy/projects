/*
 * setdmx.c
 * Sets one or a set of channels (slots).
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
#include <unistd.h>

#include <dmx/dmx.h>

int dmxfd=-1;

void set(int ch, dmx_t val)
{
  lseek(dmxfd, ch, SEEK_SET);
  write(dmxfd, &val, 1);
}

int main (int argc, const char **argv)
{
  const char * dmxname = DMXdev(&argc, argv);
  dmxfd=open(dmxname, O_WRONLY);
  if(dmxfd<0)
    {
      fprintf(stderr, "could not open %s : %s\n", dmxname, strerror(errno));
      return 1;
    }

  if(argc<2)
    {
      while(!feof(stdin))
	{
	  int ch, val;
	  int r=fscanf(stdin, "%i %i", &ch, &val);
	  if(r==2)
	    set(ch, val);
	}
    }
  else
    {
      int i=0;
      while(i<argc)
	set(atoi(argv[i]), atoi(argv[i+1])), i+=2;
    }

  close(dmxfd);
  return 0;
}
