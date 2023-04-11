#include<stdio.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<stdlib.h>


int main(){
    int fd;
    while(1){
        char x;
        int c;
        printf("Enter choice:\n1:Red Light.\n2:Green Light.\n3:Yellow Light\n4:Exit\n");
        scanf("%d",&c);
            switch(c){
                case 1:{
                    fd = open("/dev/led_device-0",O_RDWR);
                    if(fd == -1){
                        printf("Failed to open led_device-0\n");
                        return -1;
                    }
                    x = '1';
                    if(write(fd,&x,sizeof(char)) < 0){
                        printf("failed to write");
                        return -2;
                    }
                    sleep(2);
                    x = '0';
                    if(write(fd,&x,sizeof(char)) < 0){
                        printf("failed to write");
                    return -2;
                    }
                    close(fd);
                    break;
                }
                case 2:{
                    fd = open("/dev/led_device-1",O_RDWR);
                    if(fd == -1){
                        printf("Failed to open led_device-1\n");
                        return -1;
                    }
                    x = '1';
                    if(write(fd,&x,sizeof(char)) < 0){
                        printf("failed to write");
                        return -2;
                    }
                    sleep(2);
                    x = '0';
                    if(write(fd,&x,sizeof(char)) < 0){
                        printf("failed to write");
                        return -2;
                    }
                    close(fd);
                    break;
                }
                case 3:{
                    fd = open("/dev/led_device-2",O_RDWR);
                    if(fd == -1){
                        printf("Failed to open led_device-2\n");
                        return -1;
                    }
                    x = '1';
                    if(write(fd,&x,sizeof(char)) < 0){
                        printf("failed to write");
                        return -2;
                    }
                    sleep(2);
                    x = '0';
                    if(write(fd,&x,sizeof(char)) < 0){
                        printf("failed to write");
                        return -2;
                    }
                    close(fd);
                    break;
                }
                case 4:{
                    printf("Exiting.....\n");
                    exit(0);
                }
                default:{
                    printf("Invalid choice\n");
                    break;
                }
            }
        }
    return 0;
}
