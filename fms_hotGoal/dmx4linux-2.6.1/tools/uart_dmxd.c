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
/* compile: cc -o serialgate serialgate.c -ldmx */
/*
 * Credits to Steve Tell <tell@telltronics.org> for idea and testing.
 */

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>


#include <dmx/dmx.h>



#define DEFAULT_BREAK_TIME_US   100 /* 100us */
#define MALF_TIME_US            100 /* 100us */




#ifndef B230400
#error We need a nominal baudrate of 230400Baud to support DMX512.With a modified clock, this leads to a baudrate of 250Kbaud.
#endif


void my_usleep(int usec)
{
#ifdef HAVE_NANOSLEEP
        struct timespec req;

        req.tv_sec = usec / 1000000;
        usec -= req.tv_sec * 1000000;
        req.tv_nsec = usec * 1000;

        nanosleep(&req, NULL);
#else
	usleep(usec);
#endif
}



static int setupLineForDMX (int fh)
{

  struct termios my_tios; /* read about all that stuff */
                          /* with 'man cfsetospeed'    */

  if (fh==-1) return -1;

  tcgetattr   (fh, &my_tios);
  cfmakeraw   (&my_tios);

  /* 250KBaud 8N2, no RTSCTS */
#if 0
  cfsetispeed (&my_tios, B230400);
  cfsetospeed (&my_tios, B230400);
#endif
  /* My clock has been a 22.1184MHz crystal that I have exchanged by a 24MHz crystal.
   * If it does not work, we may use a clock with half of the speed (12MHz) and use B460800.
   */

  my_tios.c_cflag &= ~CSIZE;
  my_tios.c_cflag |= CS8;
  my_tios.c_cflag &= ~(PARENB|PARODD);
  my_tios.c_cflag |=CSTOPB;
  my_tios.c_cflag &= ~CRTSCTS;

  return tcsetattr   (fh, TCSANOW, &my_tios);
}


static void usage ()
{
  fprintf (stderr, "\nserialgate [options] <port>\n");
  fprintf (stderr, "  <port>                Set serial port to use for output.\n");
  fprintf (stderr, "  --dmx <device>        Set dmx device to listen to.\n");
  fprintf (stderr, "  --channels <number>   Set the number of channels to output.[1..512]\n");
  fprintf (stderr, "  --dmxbase <number>    Set the base channel to start listening [1..N].\n");
  fprintf (stderr, "  --break-time <number> Break time in microseconds. [88..N] (default = %dus).\n",
	   DEFAULT_BREAK_TIME_US);
  fprintf (stderr, "  --malf-time <number>  Mark after last channel time in microseconds. [8..N] (default = %dus).\n",
	   MALF_TIME_US);
  fprintf (stderr, "  --verbose             Increase verbosity (default verbosity = 1).\n");
  fprintf (stderr, "  --quiet               Be quiet (verbosity = 0).\n");
  fprintf (stderr, "\n");
#ifdef HAVE_NANOSLEEP
  fprintf (stderr, "Have nanosleep\n");
#else
  fprintf (stderr, "Do not have nanosleep. use usleep instead.\n");
#endif
}



static int dmxfd = -1, g_serfd = -1;

void exit_handler(void)
{
  if (dmxfd != -1)
      close (dmxfd);

  if (g_serfd != -1)
    close(g_serfd);
}

void signal_handler (int sig)
{
  exit(1);
}


int main (int argc, const char **argv)
{
  unsigned long framecounter = 0L;
  const char *dmxname = DMXdev(&argc, argv);
  const char *sername = NULL;
  unsigned char dmxdata[513];
  int  n=0;
  int  i;
  int  dmxchannels = 24;
  int  dmxbase = 0;
  int  verbose = 1;
  int  error_count=0;
  int  break_time_us = DEFAULT_BREAK_TIME_US;
  int  malf_time_us = MALF_TIME_US; /* mark after last channel (Recomended Practice for DMX512, page 55) */


  /*---------------------------------------
  //-- checking for command line arguments
  //---------------------------------------*/

  for (i=1; i<argc; i++)
    {
      if (strcmp(argv[i], "--channels")==0)
        {
	  i++;
          if (i >= argc)
	    {
	      usage();
	      exit(1);
	    }
	  dmxchannels = strtol(argv[i], NULL, 10);
        }

      else if (strcmp(argv[i], "--dmxbase")==0)
        {
	  i++;
          if (i >= argc)
	    {
	      usage();
	      exit(1);
	    }
	  dmxbase = strtol(argv[i], NULL, 10) - 1;
        }

      else if (strcmp(argv[i], "--dmx")==0)
        {
	  i++;
          if (i >= argc)
	    {
	      usage();
	      exit(1);
	    }
        }


      else if (strcmp(argv[i], "--break-time")==0)
        {
	  i++;
          if (i >= argc)
	    {
	      usage();
	      exit(1);
	    }
	  break_time_us = strtol(argv[i], NULL, 10) - 1;
        }

      else if (strcmp(argv[i], "--malf-time")==0)
        {
	  i++;
          if (i >= argc)
	    {
	      usage();
	      exit(1);
	    }
	  malf_time_us = strtol(argv[i], NULL, 10) - 1;
        }

      else if (strcmp(argv[i], "--verbose")==0)
	{
	  /* Only increase verbose if not quiet */
	  if(verbose > 0)
	    verbose++;
	}

      else if (strcmp(argv[i], "--quiet")==0)
	verbose=0;

      else if (*argv[i]!='-')
	sername = argv[i];

      else
	{
	  usage();
	  exit(1);
	}

    }



  /*--------------------------------------------
  //-- checking for correct values for settings.
  //--------------------------------------------*/

  if (!sername)
    {
      fprintf (stderr, "serial port name is missing\n");
      usage();
      exit(1);
    }

  if (dmxchannels < 1 || dmxchannels > 512)
    {
      fprintf (stderr, "number of dmx-channels to output are out of range\n");
      usage();
      exit(1);
    }

  if (dmxbase < 0)
    {
      fprintf (stderr, "dmx channel base is out of range\n");
      usage();
      exit(1);
    }


  if (break_time_us < 88)
    {
      fprintf (stderr, "Break time needs to be at least 88 microseconds\n");
      usage();
      exit(1);
    }

  if (malf_time_us < 8)
    {
      fprintf (stderr, "Mark after last break needs to be at least 8 microseconds.\n");
      usage();
      exit(1);
    }

  if (verbose > 1)
    printf ("using \"%s\" as serial port, base channel = %d, channel count = %d\n",
	    sername, dmxbase, dmxchannels);




  /*---------------------------------------
  //-- opening dmx-device and serial port
  //---------------------------------------*/
  dmxfd   = open (dmxname, O_RDONLY | O_NONBLOCK);
  if (dmxfd == -1)
    {
      fprintf (stderr, "failed to open dmx output for reading\n");
      exit(1);
    }

  g_serfd = open (sername, O_WRONLY);
  if (g_serfd == -1)
    {
      close(dmxfd);
      fprintf (stderr, "failed to open serial port for output\n");
      exit(1);
    }

  if (setupLineForDMX (g_serfd) != 0)
    {
      close(dmxfd);
      close(g_serfd);
      fprintf (stderr, "failed to set up serial line for dmx-output\n");
      exit(1);
    }

  atexit(exit_handler);
  signal (SIGTERM, signal_handler);
  signal (SIGSEGV, signal_handler);

#if 0
  /*---------------------------------------
  //-- Setting up scheduler
  //---------------------------------------*/

  if (geteuid() == 0)
    {
      struct sched_param sparam;
      sparam.sched_priority = sched_get_priority_max(SCHED_RR);
      sched_setscheduler(0, SCHED_RR, &sparam);
      setuid(getuid());

      if (verbose > 1)
	printf ("Using Round-Robin scheduler for dmx-output.");
    }
  else
    {
      if (verbose > 0)
	printf ("Not using Round-Robin scheduler for dmx-output. You must be root to use RR-Scheduling.");
    }
#endif

  memset (dmxdata, 0, sizeof(dmxdata));

  dmxdata[0] = 0; /* startcode of 0 */

  if (verbose > 2)
    printf ("\n");


  /*--------------
  //-- Main loop.
  //--------------*/

  while (1)
    {
      lseek (dmxfd, dmxbase, SEEK_SET);  /* set to the first channel */
      n = read (dmxfd, dmxdata+1, dmxchannels);
      n=512;
      if (n > 0)
        {
          ioctl(g_serfd, TIOCSBRK, 0);
	  my_usleep(break_time_us); /* should sleep for 100 microseconds. */
          ioctl(g_serfd, TIOCCBRK, 0);
	  my_usleep(10); /* mark after break. */
          if (write(g_serfd, dmxdata, dmxchannels+1) <= 0)
	    {
	      /* This is an error, and because we use Sched-RR we need
		 to be sure, that we do not end up in an infinite loop
		 that eats up all cpu-cycles. */
	      if (error_count > 10)
		{
		  fprintf (stderr, "failed to do a write on the serial port for %i times. giving up.\n", error_count);
		  exit(1);
		}
	      error_count++;
	    }
	  else
            {
              unsigned int result = 0;
	      error_count = 0;
              /* After the write returned without an error, there may
		 be bytes left in the outgoing fifo or the tx-buffer
		 may not be empty. We need to wait until anything is
		 send out.  We may assume that the TX buffer is half
		 full. We can sleep for so long and then do some
		 sleeps until the TX-Buffer is realy empty.  It is not
		 worth going to sleep for a shorter period.  If this
		 period is to high, we may */
              usleep (8*44);
              while (ioctl (g_serfd, TIOCSERGETLSR, &result)==0 && result != TIOCSER_TEMT)
                usleep(44);

              if (malf_time_us > 0)
                usleep(malf_time_us);

	      if (verbose > 2)
	        {
	          printf ("\r%8lu\r", framecounter++);
	          fflush(stdout);
	        }
            }
        }
    }

  /* should never end up here. */
  return 0;
}

