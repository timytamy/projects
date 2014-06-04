/* This example shows a simple blocking read. */

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <dmx/dmx.h>

int main (int argc, const char **argv)
{
  const char *dmxname = DMXINdev(&argc, argv);
  int fd = open (dmxname, O_RDONLY);
  if (fd!=-1)
    {
      int i, n;
      dmx_t buffer[10];

      lseek (fd, 0, SEEK_SET);  /* set to the first channel */
      n=read (fd, buffer, sizeof(buffer)); /* read up to 10 channels */

      if (n>0)
        for (i=0; i<n; i++)
          printf ("channel %d has value %d\n", i+1, buffer[i]);
      else
        printf ("Error %d while reading %s\n", n, dmxname);
      close (fd);
    }
  return 0;
}
