/*application for ioctl*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdint.h>

/*Command Macros For ioctl*/
#define PE _IOW('a', 'a', int) // for page erase
#define CE _IOW('a', 'b', int) // for chip erase
#define SE _IOW('a', 'c', int) // for sector erase

int main(int argc, char **argv)
{
    int fd;
    int arg = atoi(argv[1]); // location
    int cmd = atoi(argv[2]); // erase type
    if (cmd > 2)
    {
        printf("Error: please enter:\n%s <location> <(0-Page ERASE,1-Sector Erase,2-Chip Erase)>\n", argv[0]);
        return -1;
    }
    printf("Choosen loc: %d\n", arg);
    fd = open("/dev/spi_eeprom_slave", O_RDWR);

    if (fd < 0)
    {
        perror("Failed Open");
        return -1;
    }
    switch (cmd)
    {
    case 0:
    {
        if (ioctl(fd, PE, (int)arg) == -1)
        {
            perror("Failed ioctl");
            return -1;
        }
        printf("page erased succesfully\n");
        break;
    }
    case 1:
    {
        if (ioctl(fd, SE, (int)arg) == -1)
        {
            perror("Failed ioctl");
            return -1;
        }
        printf("Sector erased succesfully\n");
        break;
    }
    case 2:
    {
        if (ioctl(fd, CE, (int)arg) == -1)
        {
            perror("Failed ioctl");
            return -1;
        }
        printf("Chip erased succesfully\n");
        break;
    }
    default:
    {
        printf("Error: please enter (0-Page Erase,1-Sector Erase, 2-Chip Erase)\n");
        break;
    }
    }
    close(fd);
    return 0;
}