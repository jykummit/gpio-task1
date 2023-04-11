#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<fcntl.h>



int main(){
	int fd;
	fd = open("/dev/dht11",O_RDWR);

	if(fd < 0){
		printf("Failed to open device file\n");
		return -1;
	}

	char buf[4];
	if(read(fd, buf, sizeof(buf)) < 0){ 
		printf("Failed to read from dev file\n");
		return -1; 
	}

	for(int i=0; i<4; i++){
		printf("Buffer:%d \n",buf[i]);
	}

	//usleep(500000);
	close(fd);
	return 0;
}
