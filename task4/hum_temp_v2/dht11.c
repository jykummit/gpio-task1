/*Device Driver for DHT11 Humidity and Temperature sensor*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/time.h>

#define DHT11_DRIVER_NAME "dht11"
#define GPIO_4 (4)
#define TIMEOUT 100
#define LOW 0
#define HIGH 1
static dev_t dht11_device;
static struct class *dht11_class;
static struct cdev dht11_cdev;

//static int irq;
uint8_t gpio_state;
uint8_t data[5];
int level = LOW;
int lastresult = LOW;
int force = LOW;

/*static irqreturn_t irq_handler(unsigned int irq, void *dev_id){
  gpio_state = gpio_get_value(GPIO_4);
  printk("IRQ handler called =%d\n",gpio_state);
  return IRQ_HANDLED;
  }*/

//Open Function
static int open_dht11(struct inode *inode, struct file *file){
	printk("Device file for dht11 is opened...\n");
	if(gpio_is_valid(GPIO_4) == false){
		printk("GPIO - %d is not valid\n",GPIO_4);
		return -1;
	}
	if(gpio_request(GPIO_4,"GPIO_4") < 0){
		printk("GPIO - %d failed to request\n",GPIO_4);
		return -1;
	}
	if(gpio_direction_input(GPIO_4) < 0){
		printk("GPIO - %d failed to set direction\n",GPIO_4);
		gpio_free(GPIO_4);
		return -1;
	}

	/*	irq = gpio_to_irq(GPIO_4);
		printk("DHT11 GPIO: %d, IRQ: %d\n", GPIO_4, irq);
		if(request_irq(irq, (void *) irq_handler,IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "dht11", NULL)){
		printk("DHT11 sensor cannot register IRQ\n");
		gpio_free(GPIO_4);
		return -1;
		}
		*/

	return 0;
}

//Release Function
static int release_dth11(struct inode *inode, struct file *file){
	//	free_irq(GPIO_4,NULL);
	gpio_free(GPIO_4);
	printk("Device file for dht11 is closed...\n");
	return 0;
}

// Sending Request to DHT11 sensor
int send_request(void){
	gpio_direction_output(GPIO_4, 0);
	mdelay(18);

	return 0;
}


int expectPulse(int level) {
	uint32_t count = 0;
	while (gpio_get_value(GPIO_4) == level) {
		if (count++ >= TIMEOUT) {
			break; // Exceeded timeout, fail.
		}
		//udelay(1);
	}
	return count;
}


//Reading Response from DHT11 sensor
int read_response(void)
{
	int ret1, ret2;
	gpio_direction_input(GPIO_4);
	udelay(54);
	
	ret1 =expectPulse(LOW);
	printk("Expect Pulse low: %d\n", ret1);

	if (ret1 == TIMEOUT) {
		printk("DHT timeout waiting for start signal low pulse.\n");
		return -1;
	}

	ret2 =expectPulse(HIGH); 
	printk("Expect Pulse high: %d\n", ret2);

	if (ret2 == TIMEOUT) {
		printk("DHT timeout waiting for start signal high pulse.\n");
		return -1;
	}

	return 0;
}




//Read Function
static ssize_t read_dht11(struct file *filp, char __user *buf, size_t len, loff_t *off){
	uint32_t cycles[80];

	// Reset 40 bits of received data to zero.
	data[0] = data[1] = data[2] = data[3] = data[4] = 0;


	// Sending Request to DHT11 sensor
	if(send_request()){
		printk("Failed to send request\n");
		return -1;
	}

	//Reading Response from DHT11 sensor
	if(read_response()){
		printk("Failed to read response\n");
		return -1;
	}

	for (int i = 0; i < 80; i += 2) {
		cycles[i] = expectPulse(LOW);
		cycles[i + 1] = expectPulse(HIGH);
	}
	for (int i = 0; i < 40; ++i) {
		uint32_t lowCycles = cycles[2 * i];
		uint32_t highCycles = cycles[2 * i + 1];
		if ((lowCycles == TIMEOUT) || (highCycles == TIMEOUT)) {
			printk("DHT timeout waiting for pulse.\n");
			return -1;
		}

		data[i / 8] <<= 1;
		// Now compare the low and high cycle times to see if the bit is a 0 or 1.
		if (highCycles > lowCycles) {
			// High cycles are greater than 50us low cycle count, must be a 1.
			data[i / 8] |= 1;
		}
	}


	printk("Humidity:%d.%d Temperature:%d.%d, checksum: %d", data[0], data[1], data[2], data[3], data[4]);
	// Check we read 40 bits and that the checksum matches.
	if (data[4] != ((data[0] + data[1] + data[2] + data[3]))) {
		printk("DHT checksum failure!");
		return -1;
	}


	// Return the sensor data
	if(copy_to_user(buf, data, 4) != 0 ){
		printk("Couldn't copy to user\n");
		return -1;
	}

	return 4;
}


//Operations to be performed on device
static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = open_dht11,
	.read = read_dht11,
	.release = release_dth11,
};

//Init Function
static int dht11_init(void){
	int ret = alloc_chrdev_region(&dht11_device, 0, 1, DHT11_DRIVER_NAME);

	if(ret < 0){
		printk("Failed to allocate region\n");
		return -1;
	}

	cdev_init(&dht11_cdev, &fops);

	if(cdev_add(&dht11_cdev, dht11_device, 1) < 0){ 
		printk("Failed at cdev_add\n");
		unregister_chrdev_region(dht11_device, 1); 
		return -1; 
	}
	dht11_class = class_create(THIS_MODULE, DHT11_DRIVER_NAME);

	if(IS_ERR(dht11_class)){
		printk("Failed to create class\n");
		cdev_del(&dht11_cdev);
		unregister_chrdev_region(dht11_device, 1); 
		return -1; 
	}

	if(IS_ERR(device_create(dht11_class,NULL,dht11_device,NULL,DHT11_DRIVER_NAME))){
		printk("Failed to create device file in /dev directory\n");
		class_destroy(dht11_class);
		cdev_del(&dht11_cdev);
		unregister_chrdev_region(dht11_device, 1); 
		return -1; 
	}

	printk("DHT11 device driver loaded successfully\n");
	return 0;
}

//Exit Function
static void dht11_exit(void){
	device_destroy(dht11_class, dht11_device);
	class_destroy(dht11_class);
	cdev_del(&dht11_cdev);
	unregister_chrdev_region(dht11_device, 1);
	printk("DHT11 driver was removed sucessfully...\n");
	return;
}

module_init(dht11_init);
module_exit(dht11_exit);

//Meta Information
MODULE_DESCRIPTION("DHT11 temperature & humidity sensor driver");
MODULE_AUTHOR("Anusha");
MODULE_LICENSE("GPL");

