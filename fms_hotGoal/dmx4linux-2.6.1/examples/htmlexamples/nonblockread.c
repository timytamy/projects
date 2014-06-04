/* This example shows a non-blocking read from the dmx-output. */

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <dmx/dmx.h>

int main (int argc, const char **argv)
{
  const char *dmxname = DMXdev(&argc, argv);
  int fd = open (dmxname, O_RDONLY | O_NONBLOCK);
  if (fd!=-1)
    {
      int  i=0;

      lseek (fd, 0, SEEK_SET);  /* set to the first channel */

      while (i<200)
        {
          int j;
          dmx_t buffer[20];
          int n=read (fd, buffer, sizeof(buffer));
          if (n<=0)
            {
              printf ("error %d while reading %s\n", n, dmxname);
              close (fd);
	      return 1;
            }

          printf ("%d..%d:", i, i+n);
          for (j=0; j<n; j++)
            printf (" %03d", buffer[i+j]);
          printf ("\n");

          i += n;
        }
      close (fd);
    }
  else
    printf ("Error while opening %s\n", dmxname);
  return 0;
}
