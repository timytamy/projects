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

#include <stdio.h>
#include <stdlib.h>
#include <sys/timeb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/** sleep for usec. The "select" system call is used, this routine should be portable accross most unix flavours. */
void DMXsleep(int usec)
{
  struct timeval tv;
  tv.tv_sec = usec/1000000;
  tv.tv_usec = usec%1000000;
  if(select(1, NULL, NULL, NULL, &tv) < 0)
    perror("could not select");
}

/** sleep for used. The "nanosleep" system call is used */
void DMXusleep(int usec)
{
  struct timespec time;
  time.tv_sec = usec/1000000;
  time.tv_nsec = 1000 * (usec%1000000);
  nanosleep(&time, &time);
}

/** @return the system time in milliseconds */
unsigned long timeGetTime()
{
  struct timeb t;
  ftime(&t);
  return (unsigned long)t.time*1000UL+(unsigned long)t.millitm;
}
