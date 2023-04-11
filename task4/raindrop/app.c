#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<fcntl.h>



int main(){
	int fd;
	fd = open("/dev/rain_lm393",O_RDWR);

	if(fd < 0){
		printf("Failed to open device file\n");
		return -1;
	}

	int buf;
	while(1){
		if(read(fd, &buf, sizeof(int)) < 0){ 
			printf("Failed to read from dev file\n");
			return -1; 
		}
		if(buf == 0)	printf("Output:%d Raindrop detected\n",buf);
		else	printf("Output:%d Raindrop NOT detected\n", buf);
	}
	close(fd);
	return 0;
}
