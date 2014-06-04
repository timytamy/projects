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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/**
   retrieve a command line argument. arguments should start with "-"
   or "--" or "+" but this is not requiered. The command line is
   permuted, with all found parameters moved to the end and argc
   lowered. A parameter can have an optional argument, this is
   controlled with the paramhasarg variable. Found arguments are
   returned with paramarg.

   @param argc address of argc from main. The variable is decremented by the strings found.
   @param argv argv array from main. The array is permuted, found string moved to end
   @param param parameter string which should be found
   @param paramhasarg look for an additional argument after param
   @param paramarg string of additional argument. Returns the argument string, NULL otherwise.

   @return -1 if param was not found. 0 if param was found. 1 if param
   was found and additional argument was found. -2 if paramters are errorneous
   (NULL strings etc.)
*/
int DMXgetarg(int *argc, const char **argv, const char *param, const int paramhasarg, const char **paramarg)
{
  int skip=0;
  int i=1, j;
  int ret=-1;
  const char *parambak=NULL;

  /* check input */
  if(argc==NULL || argv==NULL || param==NULL)
    return -2;

  if(*argc<2)
    return -2;

  if(strlen(param)<1)
    return -2;

  if(paramhasarg && !paramarg)
    return -2;

  /* search for param */
  while(i<*argc)
    {
      if(!strcmp(argv[i], param))
	{
	  skip=1;
	  ret=0;
	  parambak=argv[i];
	  break;
	}
      i++;
    }

  /* no param found */
  if(ret<0 && skip==0)
    return ret;

  if(paramhasarg && i+1<*argc)
    {
      *paramarg=argv[i+1];
      ret=1;
      skip=2;
    }

  /* move processed strings to end of argv */
  for(j=i; j<*argc-skip; j++)
    argv[j]=argv[j+skip];
  if(j<*argc)
    argv[j++]=parambak;
  if(j<*argc && paramhasarg)
    argv[j]=*paramarg;

  *argc-=skip;

  return ret;
}

static int checkdevice(const char *filename)
{
  struct stat buf;
  if(stat(filename, &buf) < 0)
    return 0;
  if(S_ISCHR(buf.st_mode))
    return 1;

  return 0;
}

/**
   Search for a DMX output device on command line and environment. Use
   defaults if nothing found. Filenames are checked for beeing
   devices. Devices are searched in the following order:
   <ol>
   <li>The command line is searched for "--dmx <device>".
   <li>The environment is searched for "DMX".
   <li>Filesystem is searched for "/dev/dmx"
   <li>Filesystem is searched for "/dev/misc/dmx"
   </ol>
   If a valid device is found its string is returned. If nothing is found NULL is returned.

   @param argc address of argc from main routine
   @param argv argv from main routine

   @return device filename or NULL if nothing found
*/
const char* DMXdev(int *argc, const char **argv)
{
  int i;
  char *s=NULL;

  const char* FILE[] = {
    "/dev/dmx",
    "/dev/misc/dmx",
    0
  };

  if (argc && argv && *argc>1)
    {
      const char *arg=NULL;
      if(DMXgetarg(argc, argv, "--dmx", 1, &arg) > 0)
	return arg;
    }

  /* then environment */
  s=getenv("DMX");
  if(s)
    return s;

  /* now standard devices */
  i=0;
  while(FILE[i])
    {
      if(checkdevice(FILE[i]))
	return FILE[i];
      i++;
    }

  return NULL;
}

/**
   Search for a DMX input device on command line and environment. Use
   defaults if nothing found. Filenames are checked for beeing
   devices. Devices are searched in the following order:
   <ol>
   <li>The command line is searched for "--dmxin <device>".
   <li>The environment is searched for "DMXIN".
   <li>Filesystem is searched for "/dev/dmxin"
   <li>Filesystem is searched for "/dev/misc/dmxin"
   </ol>
   If a valid device is found its string is returned. If nothing is found NULL is returned.

   @param argc address of argc from main routine
   @param argv argv from main routine

   @return device filename or NULL if nothing found
*/
const char* DMXINdev(int *argc, const char **argv)
{
  int i;
  char *s=NULL;

  const char* FILE[] = {
    "/dev/dmxin",
    "/dev/misc/dmxin",
    0
  };

  if (argc && argv && *argc>1)
    {
      const char *arg=NULL;
      if(DMXgetarg(argc, argv, "--dmxin", 1, &arg) > 0)
	return arg;
    }

  /* then environment */
  s=getenv("DMXIN");
  if(s)
    return s;

  /* now standard devices */
  i=0;
  while(FILE[i])
    {
      if(checkdevice(FILE[i]))
	return FILE[i];
      i++;
    }

  return NULL;
}
