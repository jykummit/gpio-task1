#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/err.h>

#include <linux/interrupt.h>

#define RAIN_DRIVER_NAME "rain_lm393"
#define GPIO_21 (21)

static dev_t rain_lm393_device;
static struct class *rain_lm393_class;
static struct cdev rain_lm393_cdev;

static int raindrop_irq;
uint8_t gpio_state;

static irqreturn_t raindrop_irq_handler(unsigned int irq, void *dev_id){
	gpio_state = gpio_get_value(GPIO_21);
	printk("Sensing Raindrop sensor...\n");
	return IRQ_HANDLED;
}

static int open_rain_lm393(struct inode *inode, struct file *file){
	printk("Device file for rain_lm393 opened...\n");
	if(gpio_is_valid(GPIO_21) == false){
                printk("GPIO - %d is not valid\n",GPIO_21);
                return -1; 
        }
        if(gpio_request(GPIO_21,"GPIO_21") < 0){ 
                printk("GPIO - %d failed to request\n",GPIO_21);
                return -1; 
        }
        if(gpio_direction_input(GPIO_21) < 0){ 
                printk("GPIO - %d failed to set direction\n",GPIO_21);
                gpio_free(GPIO_21);
                return -1; 
        }

	raindrop_irq = gpio_to_irq(GPIO_21);
	printk("Raindrop GPIO: %d, IRQ: %d\n", GPIO_21, raindrop_irq);
	if(request_irq(raindrop_irq, (void *) raindrop_irq_handler, IRQF_TRIGGER_RISING,"rain_lm393_device" , NULL)){
                printk("Raindrop sensor cannot register IRQ\n");
                gpio_free(GPIO_21);
                return -1;
	}

	return 0;
};

static int release_rain_lm393(struct inode *inode, struct file *file){
	free_irq(raindrop_irq,NULL);
	gpio_free(GPIO_21);
	
	printk("Device file for rain_lm393 is closed...\n");
	return 0;
}

static ssize_t read_rain_lm393(struct file *filp, char __user *buf, size_t len, loff_t *off){
        printk("Read function has been called\n");
        len = 1;
        if(copy_to_user(buf,&gpio_state,len) > 0){
                printk("Couldn't write all the bytes to user\n");
        }

        printk("From GPIO - %d =  %d\n",GPIO_21,gpio_state);

	if(gpio_state == 0)	printk("Rain drops are detected\n");
	else printk("Rain drops are not detected\n");


	return len;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = open_rain_lm393,
	.read = read_rain_lm393,
	.release = release_rain_lm393,
};

static int rain_lm393_init(void){
	int ret = alloc_chrdev_region(&rain_lm393_device, 0, 1, RAIN_DRIVER_NAME);

	if(ret < 0){
		printk("Failed to allocate region\n");
		return -1;
	}

	cdev_init(&rain_lm393_cdev, &fops);

        if(cdev_add(&rain_lm393_cdev, rain_lm393_device, 1) < 0){ 
                printk("Failed at cdev_add\n");
                unregister_chrdev_region(rain_lm393_device, 1); 
                return -1; 
        }
        rain_lm393_class = class_create(THIS_MODULE, RAIN_DRIVER_NAME);

        if(IS_ERR(rain_lm393_class)){
                printk("Failed to create class\n");
                cdev_del(&rain_lm393_cdev);
                unregister_chrdev_region(rain_lm393_device, 1); 
                return -1; 
        }

        if(IS_ERR(device_create(rain_lm393_class,NULL,rain_lm393_device,NULL,RAIN_DRIVER_NAME))){
                printk("Failed to create device file in /dev directory\n");
                class_destroy(rain_lm393_class);
                cdev_del(&rain_lm393_cdev);
                unregister_chrdev_region(rain_lm393_device, 1); 
                return -1; 
        }
	printk("Raindrop LM393 sensor driver loaded successfully\n");
	return 0;
}

static void rain_lm393_exit(void){
	device_destroy(rain_lm393_class, rain_lm393_device);
	class_destroy(rain_lm393_class);
	cdev_del(&rain_lm393_cdev);
	unregister_chrdev_region(rain_lm393_device, 1);
	printk("Raindrop LM393 sensor driver was removed successfully\n");
	return;
}

module_init(rain_lm393_init);
module_exit(rain_lm393_exit);

MODULE_DESCRIPTION("DHT11 temperature & humidity sensor driver");
MODULE_AUTHOR("Anusha");
MODULE_LICENSE("GPL");
