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

/*
 * name: dmxtools.h
 * func: some supporting functions that may be used by several interfaces
 */


/*
 * name: dmxslots2jiffies
 * func: calculates the timer increment in jiffies
 */
unsigned long dmxslots2jiffies (const int channels);


/*
 * name: hexstr2ulong
 * func: tries to convert a hex string to binary long.
 */
unsigned long hexstr2ulong (const char *s);

/*
 * name: str2ulong
 * func: tries to convert a string to a binary long.
 */
unsigned long str2ulong (const char *s);


