#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include<linux/version.h>
#include<linux/uaccess.h>
#include<linux/gpio.h>

/*Max number minor devices*/
#define MAX 3

/*GPIO pins*/

#define RED_LED (21)
#define GREEN_LED (20)
#define YELLOW_LED (16)

/*Gpio pins struct array*/
static struct gpio leds_gpio[] = {
    {RED_LED,GPIOF_OUT_INIT_LOW,"Red LED-1"},
    {GREEN_LED,GPIOF_OUT_INIT_LOW,"GreenLED-2"},
    {YELLOW_LED,GPIOF_OUT_INIT_LOW,"Yellow LED-3"}
};

/*device number*/
static dev_t major_number; 
/*class structure*/
static struct class* led_class;
/*cdev structure*/
static struct cdev led_cdev[MAX];



/*Open function*/
static int led_open(struct inode *pinode,struct file *pfile){
    int minor = iminor(file_inode(pfile));
    printk("Multiled Device file opened...\n");
    if(minor >= MAX){
        printk("Minor number has exceed MAX number of devices\n");
        return -1;
    }
    if(gpio_is_valid(leds_gpio[minor].gpio) == false){
        printk("GPIO - %d is not valid\n",leds_gpio[minor].gpio);
        return -1;
    }
    if(gpio_request(leds_gpio[minor].gpio,"RED_LED") < 0){
        printk("GPIO - %d failed to request\n",leds_gpio[minor].gpio);
        return -1;
    }
    if(gpio_direction_output(leds_gpio[minor].gpio,0) < 0){
        printk("GPIO -%d failed to set direction\n",leds_gpio[minor].gpio);
        gpio_free(leds_gpio[minor].gpio);
        return -1;
    }
    return 0;
}

/*release function*/
static int led_close(struct inode *pinode,struct file *pfile){
    int minor = iminor(file_inode(pfile));
    if(minor >= MAX){
        printk("Minor number has exceed MAX number of devices\n");
        return -1;
    }
    gpio_free(leds_gpio[minor].gpio);
    printk("Device file closed!\n");
    return 0;
}

/*read function*/
static ssize_t led_read(struct file* pfile,char* __user buf,size_t len,loff_t *loff){
    printk("Led Device file opened to read\n",len);
    return len;
}

/*write function*/
static ssize_t led_write(struct file* pfile,const char* __user buf,size_t len,loff_t *loff){
    uint8_t dev_buf[10] = {0};
    printk("Changing led state...\n");

    int minor = iminor(file_inode(pfile));
    
    
    if(copy_from_user(dev_buf,buf,len) > 0){
        printk("Couldn't write all the given bytes\n");
        return -1;
    }
    if(minor >= MAX){
        printk("Minor number has exceed MAX number of devices\n");
        return -1;
    }

    if(dev_buf[0] == '0'){
        gpio_set_value(leds_gpio[minor].gpio,0);
    }else if(dev_buf[0] == '1'){
        gpio_set_value(leds_gpio[minor].gpio,1);
    }else{
        printk("Please write either 1 or 0 to on/off Led\n");
        return -1;
    }
    return len;
}


/*file operations structure*/
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_close,
};

/*module init function*/
static int __init led_init(void){
    int err = alloc_chrdev_region(&major_number,0,MAX,"led_devices");
    dev_t dev_num;
    int major,i;
    if(err < 0){
        printk("failed to allocate major number\n");
        return -1;
    }
    major = MAJOR(major_number);
    printk("Device register with <MAJOR> = <%d>\n",MAJOR(major_number));
    led_class = class_create(THIS_MODULE,"led_class");
    
    if(IS_ERR(led_class)){
        printk("failed to create led device class\n");
        unregister_chrdev_region(major_number,1);
        return -2;
    }
    for(i=0;i<MAX;i++){
        dev_num = MKDEV(major,i);
        printk("minor number = %d\n",MINOR(dev_num));
        cdev_init(&led_cdev[i],&fops);
        if(cdev_add(&led_cdev[i],dev_num,1) < 0){
            printk("Failed at cdev_add\n");
            class_destroy(led_class);
            unregister_chrdev_region(major_number,1);
            return -3;
        }
        if(IS_ERR(device_create(led_class,NULL,dev_num,NULL,"led_device-%d",i))){
            printk("Failed to create device file in /dev directory\n");
            cdev_del(&led_cdev[i]);
            class_destroy(led_class);
            unregister_chrdev_region(major_number,1);
            return -4;
        }
    }
    printk("Multiple Leds Device Driver loaded succesfully\n");
    return 0;
}


/*module cleanup function*/
static void __exit led_cleanup(void){
    dev_t dev_num;
    int i,major;
    major = MAJOR(major_number);
    for(i=0;i<MAX;i++){
        dev_num = MKDEV(major ,i);
        cdev_del(&led_cdev[i]);
        device_destroy(led_class,dev_num);
    }
    class_destroy(led_class);
    unregister_chrdev_region(major_number,MAX);
    printk("Multiple Leds Device driver has been removed succefully\n");
}


module_init(led_init);
module_exit(led_cleanup);

/*Meta Information*/
MODULE_AUTHOR("Vasavi");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Multiple Leds Device Driver");
MODULE_VERSION("1.0");
