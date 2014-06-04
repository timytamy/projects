/* This example shows a simple write to the dmx-outputs. */

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <dmx/dmx.h>

int main(int argc, const char **argv)
{
  int fd = open (DMXdev(&argc, argv), O_WRONLY);
  if (fd!=-1)
    {
      dmx_t buffer[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

      lseek (fd, 0, SEEK_SET);  /* set to the first channel */
      write (fd, buffer, sizeof(buffer)); /* write the 10 channels */
      /* now it points to the 11th channel */
      close (fd);
    }
  return 0;
}
