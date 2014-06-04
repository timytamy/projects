/*======================================================================

    A driver for PCMCIA digimedia dmx512 devices

    digimedia_cs.h

    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is David A. Hinds
    <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
    are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

    Alternatively, the contents of this file may be used under the
    terms of the GNU General Public License version 2 (the "GPL"), in which
    case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.
    
======================================================================*/

#ifndef __DIGIMEDIA_H__
#define __DIGIMEDIA_H__

#define  digimedia_memory_offset (0x800)


/*
 * # Link0 / Link1
 * 0: Idle / Idle
 * 1: Out  / Idle
 * 2: Idle / In
 * 3: Out  / In
 * 4: Out  / Out
 * 5: In   / In
 */
typedef enum {
  dgmcpumode_invalid=-1,
  dgmcpumode_idle_idle=0,
  dgmcpumode_out_idle,
  dgmcpumode_idle_in,
  dgmcpumode_out_in,
  dgmcpumode_out_out,
  dgmcpumode_in_in,
  dgmcpumode_inval,
  dgmcpumode_in_out /* this mode is a fake-mode and is reverted to dgmcpumode_out_in.
		     * It is nessesary to know that the universes has to be swapped
		     */
} dgm_cpumode_t;


typedef unsigned char  byte;
typedef unsigned short word;


/*
 * This is as the memory looks beginning at location 0x800.
 *
 */
typedef struct
{
  byte    ready_flag; /* 0xF0 after reset */
  byte    signature[10]; /* = "DMXOEMx " */
  byte    cpu_version_high;
  byte    cpu_version_low;
  byte    cpu_mode;

  byte    out_startcode[2];
  word    out_channel_cnt;
  word    out_break_time;
  word    out_break_count;
  word    out_mbb_time; /* mark before break time */

  byte  in_startcode[2];   /* be aware - use ntohs,htons for access for all 16-bit values */
  word  in_channel_cnt[2]; /* because they are in network byte order (MSB,LSB) */
  word in_break_cnt[2];

  unsigned char    reserved[0x9ff-0x822+1];

  unsigned char    dmxbuffer[2][512];
} __attribute__ ((packed)) dgm_memory_t;



#define DMU_IS_INUSE(in_use,uid)  (((in_use)>>(uid))&1)
#define DMU_DIR(in_use,uid)       (((in_use)>>(2+(uid)))&1)
#define DMU_IS_INPUT(in_use,uid)  (DMU_IS_INUSE(in_use,uid)&&DMU_DIR(in_use,uid))
#define DMU_IS_OUTPUT(in_use,uid) (DMU_IS_INUSE(in_use,uid)&&(!DMU_DIR(in_use,uid)))

#define DMU_UNUSE(in_use,uid)      ((in_use)&=(~(5<<(uid))))
#define DMU_ENABLE(in_use,uid,dir) ((in_use)|=(1|((dir)?4:0))<<(uid))


#define DMU_SETUSE(in_use,uid) (in_use&=(~(5<<(uid))))

typedef struct
{
  /* in_use:
   *  bit 0 = first  universe is in use
   *  bit 1 = second universe is in use
   *
   * -- the following two bits are currently not supported ---
   *  bit 2 = first  universe is O=output, 1=input
   *  bit 3 = second universe is O=output, 1=input
   */
  char                 in_use;
  char                 swap_universes;

  dgm_memory_t       * mem;       /* memory area of the interface */

  struct timer_list   recv_timer; /* checks periodicly for input */
  char                timer_use;
  DMXUniverse       * universes[2];
} digimedia_interface_t;



typedef struct
{
  unsigned char  *startcode;
  unsigned short *channels;
  unsigned short *framecount;
  unsigned short *breaksize;
  unsigned short *mbb_size;
  unsigned char  *data_pointer;

  unsigned short last_framecount;

  digimedia_interface_t  *dgm_interface;
  char                   data_avail;

  DMXUniverse           *universe;

  unsigned char  local_buffer[512];
} digimedia_universe_t;


#endif
