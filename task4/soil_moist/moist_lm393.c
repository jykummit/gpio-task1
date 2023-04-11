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

#define SOIL_MOIST_DRIVER_NAME "soil_moist_lm393"
#define GPIO_21 (21)

static dev_t soil_moist_lm393_device;
static struct class *soil_moist_lm393_class;
static struct cdev soil_moist_lm393_cdev;

static int soil_moist_irq;
uint8_t gpio_state;

static irqreturn_t soil_moist_irq_handler(unsigned int irq, void *dev_id){
	gpio_state = gpio_get_value(GPIO_21);
	printk("Sensing Soil Moisture Sensor...\n");
	return IRQ_HANDLED;
}

static int open_soil_moist_lm393(struct inode *inode, struct file *file){
	printk("Device file for soil_moist_lm393 opened...\n");
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

	soil_moist_irq = gpio_to_irq(GPIO_21);
	printk("Soil Moisture GPIO: %d, IRQ: %d\n", GPIO_21, soil_moist_irq);
	if(request_irq(soil_moist_irq, (void *)soil_moist_irq_handler, IRQF_TRIGGER_RISING,"soil_moist_device" , NULL)){
		printk("Soil Moisture cannot register IRQ\n");
		gpio_free(GPIO_21);
		return -1;
	}

	return 0;
};

static int release_soil_moist_lm393(struct inode *inode, struct file *file){
	free_irq(soil_moist_irq, NULL);
	gpio_free(GPIO_21);
	printk("Device file for soil_moist_lm393 is closed...\n");
	return 0;
}

static ssize_t read_soil_moist_lm393(struct file *filp, char __user *buf, size_t len, loff_t *off){
        printk("Read function has been called\n");
        len = 1;
        if(copy_to_user(buf,&gpio_state,len) > 0){
                printk("Couldn't write all the bytes to user\n");
        }

        printk("From GPIO - %d =  %d\n",GPIO_21,gpio_state);

	if(gpio_state == 0)	printk("Moisture is detected\n");
	else printk("Moisture is not detected\n");


	return len;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = open_soil_moist_lm393,
	.read = read_soil_moist_lm393,
	.release = release_soil_moist_lm393,
};

static int soil_moist_lm393_init(void){
	int ret = alloc_chrdev_region(&soil_moist_lm393_device, 0, 1, SOIL_MOIST_DRIVER_NAME);

	if(ret < 0){
		printk("Failed to allocate region\n");
		return -1;
	}

	cdev_init(&soil_moist_lm393_cdev, &fops);

        if(cdev_add(&soil_moist_lm393_cdev, soil_moist_lm393_device, 1) < 0){ 
                printk("Failed at cdev_add\n");
                unregister_chrdev_region(soil_moist_lm393_device, 1); 
                return -1; 
        }
        soil_moist_lm393_class = class_create(THIS_MODULE, SOIL_MOIST_DRIVER_NAME);

        if(IS_ERR(soil_moist_lm393_class)){
                printk("Failed to create class\n");
                cdev_del(&soil_moist_lm393_cdev);
                unregister_chrdev_region(soil_moist_lm393_device, 1); 
                return -1; 
        }

        if(IS_ERR(device_create(soil_moist_lm393_class,NULL,soil_moist_lm393_device,NULL,SOIL_MOIST_DRIVER_NAME))){
                printk("Failed to create device file in /dev directory\n");
                class_destroy(soil_moist_lm393_class);
                cdev_del(&soil_moist_lm393_cdev);
                unregister_chrdev_region(soil_moist_lm393_device, 1); 
                return -1; 
        }
	printk("Soil Moisture LM393 driver is loaded sucessfully\n");
	return 0;
}

static void soil_moist_lm393_exit(void){
	device_destroy(soil_moist_lm393_class, soil_moist_lm393_device);
	class_destroy(soil_moist_lm393_class);
	cdev_del(&soil_moist_lm393_cdev);
	unregister_chrdev_region(soil_moist_lm393_device, 1);
	printk("Soil Moisture LM393 driver was removed sucessfully...\n");
	return;
}

module_init(soil_moist_lm393_init);
module_exit(soil_moist_lm393_exit);

MODULE_DESCRIPTION("Soil Moisture LM393 sensor driver");
MODULE_AUTHOR("Anusha");
MODULE_LICENSE("GPL");
