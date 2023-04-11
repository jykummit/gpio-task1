#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<fcntl.h>



int main(){
    int fd;
    fd = open("/dev/led_device",O_RDWR);

    if(fd < 0){
        printf("Failed to open led device file\n");
        return -1;
    }

    while(1){
        int choice;
        char x;
        printf("Enter choice:\n1.On\n2.Off\n3.Exit\n");
        scanf("%d",&choice);
        switch(choice){
            case 1:{
                x = '1';
                if(write(fd,&x,sizeof(char)) <0){
                    printf("Failed to write on to device file\n");
                    return -1;
                }
                break;
            }
            case 2:{
                x = '0';
                if(write(fd,&x,sizeof(char)) <0){
                    printf("Failed to write on to device file\n");
                    return -1;
                }
                break;
            }
            case 3:{
                printf("Exiting...\n");
                close(fd);
                exit(0);
            }
            default:{
                pritnf("Invalid choice\n");
                break;
            }
        }
    }
    return 0;
}
