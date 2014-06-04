/*
 * dmxinfo.c 
 * Gets the configuration of the dmx-universes thru the * ioctl
 * interface and prints it to stdout.
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
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <dmx/dmx.h>
#include <dmx/dmxioctl.h>

void print_capabilities (int dmx, int universe, int direction)
{
  struct dmx_capabilities  cap;

  cap.direction = direction; /* 0 = output, 1 = input */
  cap.universe  = universe;  /* 0 = first universe, 1 = second universe, ... */
  if (ioctl (dmx, DMX_IOCTL_GET_CAP, &cap)>=0)
    {
      if (cap.maxSlots > 0)
        {
          printf ("%-3d ", universe+1);
          printf ("%-20s %-20s ",  cap.family, cap.driver);
          printf ("%3d ",       cap.maxSlots);
          printf ("%-20s %5d ", cap.connector, cap.conn_id);
          printf ("%5d ",       cap.breaksize);
          printf (" %2d\n",     cap.mabsize);
        }
    }
}

void print_universe_settings (int dmx, int universe, int direction)
{
  struct dmx_parm_names names;
  struct dmx_parameter  parm;

  names.universe = universe;
  names.direction = direction;
  names.offset = 0;
  if (ioctl(dmx, DMX_IOCTL_GET_PARM_NAMES, &names) >= 0)
    {
      char *name = names.names;
      int i;

      printf ("num parameters = %d\n", names.size);

      for (i=0; i<names.num_names; i++)
        {
          parm.universe = universe;
          parm.direction = direction;
          strcpy (parm.name, name);
          if (ioctl(dmx, DMX_IOCTL_GET_PARM, &parm) >= 0)
            {
              printf ("%s = %s\n", parm.name, parm.value);
            }
          name += strlen(name)+1;
        }
    }
}

static void show_dmx4linux_info (int dmx)
{
  struct dmx_info  info;

  if (ioctl (dmx, DMX_IOCTL_GET_INFO, &info) >= 0)
    {
      printf ("DMX4Linux - Version %d.%d\n", info.version_major, info.version_minor);
      printf ("  available output universes: %d\n", info.max_out_universes);
      printf ("  available input  universes: %d\n", info.max_in_universes);
      printf ("  used      output universes: %d\n", info.used_out_universes);
      printf ("  used      input  universes: %d\n", info.used_in_universes);

      printf ("  driver families registered: %d\n", info.num_families);
      printf ("  families in this entry    : %d\n", info.num_entries);

      if (info.num_entries)
	{
          char *family_name = info.family_names;
	  int i;

          for (i=0; i<info.num_entries; i++, family_name += (strlen(family_name)+1))
            {
              struct dmx_family_info dinfo;
              
              strcpy (dinfo.familyname, family_name);
              dinfo.offset = 0; /* start at offset 0 */
              
              printf ("  family  : %s\n", family_name);
	      
              if (ioctl(dmx, DMX_IOCTL_GET_DRIVERS, &dinfo) >= 0 && dinfo.num_entries > 0)
                {
                  char *driver_name = dinfo.driver_names;
		  int j;

                  printf ("    drivers registered   : %d\n", dinfo.num_drivers);
                  printf ("    drivers in this entry: %d\n", dinfo.num_entries);
		  printf ("    driver names: ");
                  
                  for (j=0; j<dinfo.num_entries; j++, driver_name += (strlen(driver_name)+1))
		    printf (" %s", driver_name);
		  printf ("\n\n");
                }
              else
                printf("ioctl DMX_IOCTL_GET_DRIVERS failed\n");
            }
        }
    }
}

int main (int argc, const char **argv)
{
  int  i;
  int  dir;
  const char *s=DMXdev(&argc, argv);
  int  dmx = open (s, O_RDONLY);
  if (dmx<0)
    {
      fprintf (stderr, "unable to open dmx-device\n");
      return 1;
    }

  show_dmx4linux_info (dmx);

  for (dir=0; dir<2; dir++)
    {
      printf ("%s universes:\n", dir?"input":"output");
      printf ("U#  family               driver            #slots connector               id break  mark-after-break\n");
      for (i=0; i<20; i++)
        {
          print_capabilities (dmx, i, dir);
        }
    }

  printf ("-------------\n");
  print_universe_settings (dmx, 0, DMX_DIRECTION_OUTPUT);

  close (dmx);
  return 0;
}
