obj-m := led.o
obj-m += switch.o  
 
host:
	make -C /lib/modules/$(shell uname -r)/build  M=$(shell pwd) modules
 


clean:
	 make -C /lib/modules/$(shell uname -r)/build  M=$(shell pwd) clean
