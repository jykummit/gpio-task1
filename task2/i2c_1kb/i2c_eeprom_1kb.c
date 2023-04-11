/*I2C EEPROM Slave Device Driver*/
#include<linux/module.h>
#include<linux/init.h>
#include<linux/fs.h>
#include<linux/i2c.h>
#include<linux/kdev_t.h>
#include<linux/cdev.h>
#include<linux/kernel.h>
#include<linux/string.h>
#include<linux/delay.h>


#define I2C_BUS (1)  //i2c bus number 
#define PG_SIZE 16	//Page size to read and write into eeprom
#define BASE_SLAVE_ADDRESS (0x50)	//eeprom i2c address
#define SLAVE_DEVICE_NAME "eeprom_slave" //slave device name
#define BLK_SIZE 256	//block size
#define MEM_SIZE 1024  //eeprom buffer size


static dev_t eeprom_dev;  //device number
static struct cdev eeprom_cdev;	 //cdev structure
static struct class* eeprom_class;  //class structure
static struct i2c_adapter* i2c_adapter;  // i2c adapter structure
static struct i2c_client* eeprom_client;  //i2c client structure
static char* kernel_buffer;  //kernel buffer	  
static int ebuf_loc = 0;  //eeprom loction
static int addresses[] = {0x50, 0x51, 0x52, 0x53};

/*eeprom open function*/
static int eeprom_open(struct inode* inode, struct file* file){
	printk("I2C EEPROM Slave Device Opened...\n");
	kernel_buffer = kzalloc(MEM_SIZE, GFP_KERNEL);
	if(kernel_buffer == NULL){
		pr_err("Failed to allocate memory");
		return -1;
	}

	return 0;
}

/*eeprom close function*/
static int eeprom_release(struct inode* inode, struct file* file){
	kfree(kernel_buffer);

	printk("I2C EEPROM Slave Device Closed\n");
	return 0;
}

/*eeprom read function*/
static ssize_t eeprom_read(struct file* file,char* __user buf,size_t len,loff_t* off){
	int ret = 0;
	int kbuf_pos = 0;
	int read_len = 0;
	int rem_bytes = len;
	int block_num = 0;
	int block_loc = 0;
	char page_buffer[PG_SIZE] ={0};

	memset(kernel_buffer,0x00,MEM_SIZE);

	block_num = ebuf_loc/BLK_SIZE;

	if (block_num > 3){
		pr_err("Block number exceeded EEPROM size\n");
		return -1;
	}

	printk("Block Number = %d\n", block_num);

	printk("Initial i2c_addr = 0x%x\n", eeprom_client->addr);
	eeprom_client->addr = addresses[block_num];
	printk("After change i2c_addr = 0x%x\n", eeprom_client->addr);

	//block_loc = ebuf_loc - (block_num * BLK_SIZE);
	block_loc = ebuf_loc % BLK_SIZE;
	page_buffer[0] = block_loc;

	ret = i2c_master_send(eeprom_client,page_buffer,1);
	if(ret < 0){
		printk("Failed to write in Read\n");
		return -1;
	}
	printk("Byte Write from read: %d\n",ret);
	mdelay(6);

	// Reading Page Wise
	if(block_loc%PG_SIZE != 0){
		memset(page_buffer,0x00,PG_SIZE);

		read_len = ((block_loc/PG_SIZE) +1)* PG_SIZE - block_loc;

		ret = i2c_master_recv(eeprom_client,page_buffer,read_len);
		if(ret < 0){
			pr_err("failed to write first chunk\n");
			return -1;
		}
		mdelay(6);
		printk("ret = %d\n",ret);

		memcpy(kernel_buffer+kbuf_pos,page_buffer,read_len);
		printk("page_buffer: %s\n",page_buffer);

		kbuf_pos = kbuf_pos + read_len;
		rem_bytes = rem_bytes - read_len;
		block_loc = block_loc + read_len;
		if (block_loc >= BLK_SIZE){
			eeprom_client->addr = addresses[++block_num];
			block_loc = 0;
		}
	}
	while(rem_bytes >= PG_SIZE){
		memset(page_buffer,0x00,PG_SIZE);
		read_len = PG_SIZE;
		ret = i2c_master_recv(eeprom_client,page_buffer,read_len);
		if(ret < 0){
			pr_err("failed to write in while loop\n");
			return -1;
		}
		mdelay(6);
		printk("ret = %d\n",ret);

		memcpy(kernel_buffer+kbuf_pos,page_buffer,read_len);
		printk("page_buffer: %s\n",page_buffer);

		kbuf_pos = kbuf_pos + read_len;
		rem_bytes = rem_bytes - read_len;
		block_loc = block_loc + read_len;
		if (block_loc >= BLK_SIZE){
			eeprom_client->addr = addresses[++block_num];
			block_loc = 0;
		}
	}

	if(rem_bytes > 0 && rem_bytes < PG_SIZE){
		memset(page_buffer,0x00,PG_SIZE);
		read_len = rem_bytes;
		ret = i2c_master_recv(eeprom_client,page_buffer,read_len);
		if(ret < 0){
			pr_err("failed to write the remaining bytes\n");
			return -1;
		}
		mdelay(5);
		printk("ret = %d\n",ret);

		memcpy(kernel_buffer+kbuf_pos,page_buffer,read_len);
		printk("page_buffer: %s\n",page_buffer);

		kbuf_pos = kbuf_pos + read_len;
		rem_bytes = rem_bytes - read_len;
		block_loc = block_loc + read_len;
	}

	/*Reading Continuously
	  ret = i2c_master_recv(eeprom_client,page_buffer,read_len);
	  if(ret < 0){
	  pr_err("failed to write from eeprom\n");
	  return -1;
	  }
	  mdelay(5);
	  printk("ret = %d\n",ret);
	  */

	printk("Reading: %s\n",kernel_buffer);
	if(copy_to_user(buf,kernel_buffer,len) > 0){
		pr_err("Couldn't read all the bytes to user\n");
		return -1;
	}  
	return len;
}

/*eeprom write function*/
static ssize_t eeprom_write(struct file* file,const char* __user buf,size_t len,loff_t* off){
	int ret=0;
	int kbuf_pos=0;
	int rem_bytes = len;
	int write_len = 0;
	int block_num = 0;
	int block_loc = 0;
	char page_buffer[PG_SIZE+1] = {0};

	memset(kernel_buffer,0x00,MEM_SIZE);

	printk("Writing to EEPROM Slave..\n");

	if(copy_from_user(kernel_buffer,buf,len) > 0){
		pr_err("failed to write\n");
		return -1;
	}

	printk("Writing: %s\n",kernel_buffer);


	block_num = ebuf_loc/BLK_SIZE;

	if (block_num > 3){
		pr_err("Block number exceeded EEPROM size\n");
		return -1;
	}

	printk("Block Number = %d\n", block_num);
	printk("Initial i2c_addr = 0x%x\n", eeprom_client->addr);
	eeprom_client->addr = addresses[block_num];
	//eeprom_client->addr = BASE_SLAVE_ADDRESS + block_num;
	printk("After change of i2c_addr = 0x%x\n", eeprom_client->addr);
	block_loc = ebuf_loc % BLK_SIZE;
	//block_loc = ebuf_loc - (block_num * BLK_SIZE);


	if(block_loc%PG_SIZE != 0){
		memset(page_buffer,0x00,PG_SIZE+1);

		write_len = ((block_loc/PG_SIZE) +1)* PG_SIZE - block_loc;
		page_buffer[0] = block_loc;

		if(len < write_len){
			write_len = len;
		}

		memcpy(page_buffer + 1,kernel_buffer+kbuf_pos,write_len);
		printk("page_buffer: %s\n",page_buffer);
		ret = i2c_master_send(eeprom_client,page_buffer,write_len+1);
		if(ret < 0){
			pr_err("failed to write first chunk\n");
			return -1;
		}
		mdelay(6);
		printk("ret = %d\n",ret);

		kbuf_pos = kbuf_pos + write_len;
		rem_bytes = rem_bytes - write_len;
		block_loc = block_loc + write_len;

		if (block_loc >= BLK_SIZE){
			eeprom_client->addr = addresses[++block_num];
			block_loc = 0;
		}
	}

	while(rem_bytes >= PG_SIZE){
		memset(page_buffer,0x00,PG_SIZE+1);
		write_len = PG_SIZE;
		page_buffer[0] = block_loc;

		memcpy(page_buffer+1,kernel_buffer+kbuf_pos,write_len);
		printk("page_buffer: %s\n",page_buffer);
		ret = i2c_master_send(eeprom_client,page_buffer,write_len+1);
		if(ret < 0){
			pr_err("failed to write in while loop\n");
			return -1;
		}
		mdelay(6);
		printk("ret = %d\n",ret);

		kbuf_pos = kbuf_pos + write_len;
		rem_bytes = rem_bytes - write_len;
		block_loc = block_loc + write_len;

		if (block_loc >= BLK_SIZE){
			eeprom_client->addr = addresses[++block_num];
			block_loc = 0;
		}
	}

	if(rem_bytes > 0 && rem_bytes < PG_SIZE){
		memset(page_buffer,0x00,PG_SIZE+1);
		write_len = rem_bytes;
		page_buffer[0] = block_loc;

		memcpy(page_buffer+1,kernel_buffer+kbuf_pos,write_len);
		printk("page_buffer: %s\n",page_buffer);
		ret = i2c_master_send(eeprom_client,page_buffer,write_len+1);
		if(ret < 0){
			pr_err("failed to write in last chunk\n");
			return -1;
		}
		mdelay(6);
		printk("ret = %d\n",ret);

		kbuf_pos = kbuf_pos + write_len;
		rem_bytes = rem_bytes - write_len;
		block_loc = block_loc + write_len;
	}
	return len;
}

loff_t eeprom_llseek(struct file *file, loff_t off, int whence){
	ebuf_loc = (int)off;
	printk("Given Starting location = %d\n", ebuf_loc);
	return off;
}

/*eeprom file operations structure*/
static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = eeprom_open,
	.read = eeprom_read,
	.write = eeprom_write,
	.llseek = eeprom_llseek,
	.release = eeprom_release,
};


/*eeprom driver probe function*/
static int eeprom_probe(struct i2c_client* eeprom_client, const struct i2c_device_id* eeprom_ids){
	int ret = alloc_chrdev_region(&eeprom_dev, 0, 1, SLAVE_DEVICE_NAME);

	if(ret < 0){
		pr_err("Failed to register i2c eeprom slave device\n");
		return -1;
	}

	cdev_init(&eeprom_cdev, &fops);

	if(cdev_add(&eeprom_cdev, eeprom_dev, 1) < 0){
		pr_err("Failed i2c eeprom slave cdev_add\n");
		unregister_chrdev_region(eeprom_dev, 1);
		return -1;
	}

	eeprom_class = class_create(THIS_MODULE,"eeprom_slave_class");
	if(eeprom_class == NULL){
		pr_err("Failed to create i2c eeprom slave class\n");
		cdev_del(&eeprom_cdev);
		unregister_chrdev_region(eeprom_dev, 1);
		return -1;
	}


	if(device_create(eeprom_class, NULL, eeprom_dev, NULL, SLAVE_DEVICE_NAME) == NULL){
		pr_err("Failed to create i2c eeprom slave device file\n");
		class_destroy(eeprom_class);
		cdev_del(&eeprom_cdev);
		unregister_chrdev_region(eeprom_dev, 1);
		return -1;
	}

	printk("I2C EEPROM Salve Driver Probed\n");
	return 0;
}

/*eeprom driver remove function*/
static int eeprom_remove(struct i2c_client* eeprom_client){
	device_destroy(eeprom_class,eeprom_dev);
	class_destroy(eeprom_class);
	cdev_del(&eeprom_cdev);
	unregister_chrdev_region(eeprom_dev, 1);
	printk("I2C EEPROM Salve Driver Removed\n");
	return 0;
}

/*i2c slave(eeprom) device id structure*/
static const struct i2c_device_id eeprom_ids[] = {
	{SLAVE_DEVICE_NAME, 0},
	{} 
};

MODULE_DEVICE_TABLE(i2c, eeprom_ids);


/*i2c driver structure*/
static struct i2c_driver eeprom_driver  = {
	.driver = {
		.name = SLAVE_DEVICE_NAME,
		.owner = THIS_MODULE,
	},
	.probe = eeprom_probe,
	.remove = eeprom_remove,
	.id_table = eeprom_ids,
};

/*EEPROM I2C information structure*/
static const struct i2c_board_info eeprom_ic_info = {
	I2C_BOARD_INFO(SLAVE_DEVICE_NAME, BASE_SLAVE_ADDRESS)  
};


/*module init function*/
static int __init eeprom_init(void){
	i2c_adapter = i2c_get_adapter(I2C_BUS);

	if(i2c_adapter == NULL){
		pr_err("I2C adapter failed to allocate\n");
		return -1;
	}

	eeprom_client = i2c_new_client_device(i2c_adapter, &eeprom_ic_info);

	if(eeprom_client == NULL){
		pr_err("Failed to create new client device for i2c eeprom slave\n");
		return -1;
	} 

	i2c_add_driver(&eeprom_driver);

	printk("I2C EEPROM Salve Driver loaded successsfully\n");
	return 0;
}

/*module exit function*/
static void __exit eeprom_cleanup(void){
	i2c_unregister_device(eeprom_client);
	i2c_del_driver(&eeprom_driver);
	printk("I2C EEPROM Salve Driver unloaded successfully\n");
}



module_init(eeprom_init);
module_exit(eeprom_cleanup);


/*Meta information*/
MODULE_AUTHOR("Anusha");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C EEPROM Slave Device Driver");
MODULE_VERSION("1.0");
