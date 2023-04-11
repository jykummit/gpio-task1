#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/*
#define ADDR_BIT_SIZE 24

unsigned int extracted(unsigned int n)
{
    return (((1 << ADDR_BIT_SIZE) - 1) & (n >> 0));
}

int main()
{
    unsigned int x = 171;
    unsigned int result = extracted(x);
    printf("%ld\n", sizeof(result));
    printf("%d\n", result);
    return 0;
}
*/

int main()
{
    uint8_t buf[10];
    unsigned int location = 100;
    buf[1] = (location >> 16) & 0xff;
    buf[2] = (location >> 8) & 0xff;
    buf[3] = (location & 0xff);
    printf("%ld\n", sizeof(buf[0]));
    // printf("%08x\n", buf[0]);
    printf("%08x\n", buf[1]);
    printf("%08x\n", buf[2]);
    printf("%08x\n", buf[3]);
    return 0;
}

/*
char loc[20];
int main()
{
    int location = 66051;
    eeprom_buffer[0] = 2;
    eeprom_buffer[3] = (location & 0x000000ff);
    eeprom_buffer[2] = (location & 0x0000ff00) >> 8;
    eeprom_buffer[1] = (location & 0x00ff0000) >> 16;
char eeprom_buffer[20] = {
    2,
    (location & 0x000000ff),
    (location & 0x0000ff00) >> 8,
    (location & 0x00ff0000) >> 16};

printf("%ld\n", strlen(eeprom_buffer));
for (int i = 0; i < 4; i++)
{
    printf("%c ", eeprom_buffer[i]);
}
// puts(loc);
return 0;
}


/*to get a particular bit

int main()
{
    int y = 185;
    int x = y - 0;
    printf("%d", (x >> 7) & 1);
    printf("%d", (x >> 6) & 1);
    printf("%d", (x >> 5) & 1);
    printf("%d", (x >> 4) & 1);
    printf("%d", (x >> 3) & 1);
    printf("%d", (x >> 2) & 1);
    printf("%d", (x >> 1) & 1);
    printf("%d\n", (x >> 0) & 1);
    printf("%02x\n", x);
}
*/