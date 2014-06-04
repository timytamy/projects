/* this example shows a a non-blocking read trigered by a select. */

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <dmx/dmx.h>

int dmxin=-1,
    dmxout=-1;

void exit_daemon (int sig)
{
  exit(0);
}

void close_files(void)
{
  if (dmxin)
    close(dmxin);
  if (dmxout)
    close(dmxout);
}

int main(int argc, const char **argv)
{
  dmx_t dmxbuffer[512];

  dmxout = open (DMXdev(&argc, argv), O_WRONLY);
  dmxin  = open (DMXINdev(&argc, argv), O_RDONLY | O_NONBLOCK);

  if (dmxout==-1 || dmxin==-1)
    exit(0);

  signal (SIGKILL, exit_daemon);
  atexit (close_files);

  while (1)
    {
      int n;
      fd_set readset;

      FD_ZERO(&readset);
      FD_SET(dmxin, &readset);
      FD_SET(0, &readset);

      n = select (dmxin+1, &readset, NULL, NULL, NULL);
      if (n>0)
        {
          if (FD_ISSET(dmxin, &readset))
            {
              int i;

              lseek (dmxin, 0, SEEK_SET);
              n=read (dmxin, dmxbuffer, sizeof(dmxbuffer));
              for (i=0; i<512; i+=2)
                {
                  int c = dmxbuffer[i];
                  dmxbuffer[i] = dmxbuffer[i+1];
                  dmxbuffer[i+1] = c;
                }
              lseek (dmxout, 0, SEEK_SET);
              write (dmxout, dmxbuffer, sizeof(dmxbuffer));
            }

          /* exit after the user presses return */
          if (FD_ISSET(0, &readset))
          {
            exit(0);
          }
        }
    }
}
