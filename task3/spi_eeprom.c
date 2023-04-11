#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spi/spi.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kdev_t.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/ioctl.h>

/*Custom Return Values*/
#define SUCCESS 0
#define FAILURE -1

/*Macros to get WIP & WEL from Status Register*/
#define GET_WIP 0 // Write in Progress bit status
#define GET_WEL 1 // Write enable latch  bit status

/*SPI Meta Info*/
#define SPI_BUS 0
#define SPI_DEV_NAME "spi_eeprom_slave"
#define KILO_BYTE 1024
#define MEM_SIZE (128 * KILO_BYTE)
#define SECTOR_SIZE (32 * KILO_BYTE)
#define PG_SIZE 256

/*INSTRUCTION SET*/
#define WREN (0x6)          // WRITE ENABLE LATCH
#define WR (0x2)            // WRITE INSTRUCTION
#define RD (0x3)            // READ INSTRUCTION
#define RDSR (0x5)          // READ STATUS REGISTER
#define PAGE_ERASE (0x42)   // PAGE ERASE
#define SECTOR_ERASE (0xD8) // SECTOR ERASE
#define CHIP_ERASE (0xC7)   // CHIP ERASE
#define WRDI (0x4)          // Reset write enable latch
#define WRSR (0x1)          // WRITE STATUS REGISTER
#define RDID (0xAB)         // Release from Deep power-down and read electronic signature
#define DPD (0xB9)          // Deep Power-Down mode

#define SPI_SPEED_HZ 1000000 // SPI Max Clock Speed

/*Command Macros For ioctl*/
#define PE _IOW('a', 'a', int) // for page erase
#define CE _IOW('a', 'b', int) // for chip erase
#define SE _IOW('a', 'c', int) // for sector erase

/*Driver Buffer*/
static char *eeprom_buffer;
static char transmit_buffer[5];    // INSTRUCTION + ADDRESS
static char instruction_buffer[5]; // Instruction Buffer

/*spi structure*/
static struct spi_master *master;
static struct spi_device *spi_device;

/*Message and Transfer Structures*/
static struct spi_message message;
static struct spi_transfer transfer;

/*char driver initialization*/
static dev_t spi_dev;
static struct class *spi_class;
static struct cdev spi_cdev;

static unsigned int location = 0;

/*for reading status register*/
static int read_status_register(int val)
{
  int ret = 0;
  int status;
  instruction_buffer[0] = RDSR;
  ret = spi_write_then_read(spi_device, instruction_buffer, 1, &status, 1);

  if (ret < 0)
  {
    pr_err("Failed to read status register\n");
    return FAILURE;
  }
  return ((status >> val) & 1);
}

/*open function*/
static int eeprom_open(struct inode *inode, struct file *file)
{
  eeprom_buffer = kzalloc(MEM_SIZE, GFP_KERNEL);

  if (eeprom_buffer == NULL)
  {
    pr_err("Failed at kzalloc()\n");
    return FAILURE;
  }
  printk("SPI Opened...\n");
  return SUCCESS;
}

/*release function*/
static int eeprom_close(struct inode *inode, struct file *file)
{
  kfree(eeprom_buffer);
  printk("SPI Closed..\n");
  return SUCCESS;
}

/*write function*/
static ssize_t eeprom_write(struct file *file, const char __user *buf, size_t len, loff_t *off)
{
  int ret = 0;
  char page_buffer[PG_SIZE + 4] = {0};
  int rem_bytes = len;
  int wr_len = 0;
  int buf_pos = 0;

  if (copy_from_user(eeprom_buffer, buf, len) > 0)
  {
    pr_err("couldn't write all bytes from user\n");
    return FAILURE;
  }

  printk("Writing to location: %d\n", location);

  while (rem_bytes != 0)
  {
    // enabling write enable latch (WREN)
    instruction_buffer[0] = WREN;
    ret = spi_write(spi_device, instruction_buffer, 1);
    if (ret < 0)
    {
      pr_err("failed to write to spi eeprom\n");
      return FAILURE;
    }
    ret = read_status_register(GET_WEL);
    printk("Write WEL: %d\n", ret);
    if (location % PG_SIZE != 0)
    {
      wr_len = ((location / PG_SIZE) + 1) * PG_SIZE - location;
      if (wr_len > len)
      {
        wr_len = len;
      }
    }
    else if (rem_bytes < PG_SIZE)
    {
      wr_len = rem_bytes;
    }
    else
    {
      wr_len = PG_SIZE;
    }
    page_buffer[0] = WR;
    page_buffer[1] = (location >> 16) & 0xff;
    page_buffer[2] = (location >> 8) & 0xff;
    page_buffer[3] = (location & 0xff);
    printk("page_buffer[1] = %02x\n", page_buffer[1]);
    printk("page_buffer[2] = %02x\n", page_buffer[2]);
    printk("page_buffer[3] = %02x\n", page_buffer[3]);
    memcpy(page_buffer + 4, eeprom_buffer + buf_pos, wr_len);
    printk("Writing: %s\n", page_buffer + 4);
    transfer.tx_buf = page_buffer;
    transfer.len = wr_len + 4;
    transfer.bits_per_word = 8;
    transfer.speed_hz = SPI_SPEED_HZ;
    spi_message_init(&message);
    spi_message_add_tail(&transfer, &message);
    ret = spi_sync(spi_device, &message);
    if (ret < 0)
    {
      pr_err("failed to write data to spi eeprom\n");
      return FAILURE;
    }
    location = location + wr_len;
    buf_pos = buf_pos + wr_len;
    rem_bytes = rem_bytes - wr_len;
    memset(page_buffer, 0, PG_SIZE + 4);
    printk("Bytes written: %d\n", wr_len);
    printk("Remaining Bytes: %d\n", rem_bytes);
    do
    {
      ret = read_status_register(GET_WIP);
      printk("status register WIP after writing data = %d\n", ret);
      mdelay(6);
    } while (ret != 0);
  }
  memset(instruction_buffer, 0, 5);
  return len;
}

/*read function*/
static ssize_t eeprom_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
  int ret = 0;
  struct spi_transfer read_transfer = {0};

  memset(eeprom_buffer, 0, MEM_SIZE);

  printk("Reading from location: %u\n", location);
  transmit_buffer[0] = RD;
  transmit_buffer[1] = (location >> 16) & 0xff;
  transmit_buffer[2] = (location >> 8) & 0xff;
  transmit_buffer[3] = (location & 0xff);

  printk("tansmit_buffer[0]: %02x\n", transmit_buffer[0]);
  printk("tansmit_buffer[1]: %02x\n", transmit_buffer[1]);
  printk("tansmit_buffer[2]: %02x\n", transmit_buffer[2]);
  printk("tansmit_buffer[3]: %02x\n", transmit_buffer[3]);

  // transmitting intruction + address
  read_transfer.tx_buf = transmit_buffer;
  read_transfer.len = 4;
  read_transfer.bits_per_word = 8;
  read_transfer.speed_hz = SPI_SPEED_HZ;

  // data to receive
  transfer.rx_buf = eeprom_buffer,
  transfer.len = len,
  transfer.bits_per_word = 8;
  transfer.speed_hz = SPI_SPEED_HZ,

  spi_message_init(&message);
  spi_message_add_tail(&read_transfer, &message);
  spi_message_add_tail(&transfer, &message);

  ret = spi_sync(spi_device, &message);

  if (ret < 0)
  {
    pr_err("failed to read\n");
    return FAILURE;
  }
  printk("Read return = %d\n", ret);
  printk("eeprom_buffer: ");
  /*for (i = 0; i < len; i++)
  {
    printk("%c", eeprom_buffer[i]);
  }*/
  printk("%s\n", eeprom_buffer);

  if (copy_to_user(buf, eeprom_buffer, len) > 0)
  {
    pr_err("couldn't read all the bytes to user\n");
    return FAILURE;
  }

  return len;
}

loff_t eeprom_llseek(struct file *file, loff_t off, int whence)
{
  location = (unsigned int)off;
  printk("User given location = %d\n", location);
  return SUCCESS;
}

static long eeprom_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
  int ret;
  int address = (int)arg;

  printk("Performing IOCTL Operation...\n");
  printk("User given location: %d\n", address);
  switch (cmd)
  {
  case PE:
  {
    printk("Performing Page Erase Operation...\n");
    instruction_buffer[0] = WREN;
    ret = spi_write(spi_device, instruction_buffer, 1);
    if (ret < 0)
    {
      pr_err("failed to enable write latch\n");
      return FAILURE;
    }
    ret = read_status_register(GET_WEL);
    printk("Status Register for WEL: %d\n", ret);

    transmit_buffer[0] = PAGE_ERASE;
    transmit_buffer[1] = (address >> 16) & 0xff;
    transmit_buffer[2] = (address >> 8) & 0xff;
    transmit_buffer[3] = (address & 0xff);

    ret = spi_write(spi_device, transmit_buffer, 4);
    if (ret < 0)
    {
      pr_err("failed to erase page\n");
      return FAILURE;
    }
    do
    {
      ret = read_status_register(GET_WIP);
      printk("Status Register for Page Erase WIP: %d\n", ret);
      mdelay(6);
    } while (ret != 0);
    memset(transmit_buffer, 0, 5);
    break;
  }
  case SE:
  {
    printk("Performing Sector Erase Operation...\n");
    instruction_buffer[0] = WREN;
    ret = spi_write(spi_device, instruction_buffer, 1);
    if (ret < 0)
    {
      pr_err("failed to enable write latch\n");
      return FAILURE;
    }
    ret = read_status_register(GET_WEL);
    printk("Status Register for WEL: %d\n", ret);
    transmit_buffer[1] = (address >> 16) & 0xff;
    transmit_buffer[2] = (address >> 8) & 0xff;
    transmit_buffer[3] = (address & 0xff);

    ret = spi_write(spi_device, transmit_buffer, 4);
    if (ret < 0)
    {
      pr_err("failed to erase sector\n");
      return FAILURE;
    }
    do
    {
      ret = read_status_register(GET_WIP);
      printk("Status Register for Sector Erase WIP: %d\n", ret);
      mdelay(6);
    } while (ret != 0);
    memset(transmit_buffer, 0, 5);
    break;
  }
  case CE:
  {
    printk("Performing Chip Erase Operation...\n");
    instruction_buffer[0] = WREN;
    ret = spi_write(spi_device, instruction_buffer, 1);
    if (ret < 0)
    {
      pr_err("failed to enable write latch\n");
      return -1;
    }
    ret = read_status_register(GET_WEL);
    printk("Status Register for WEL: %d\n", ret);
    instruction_buffer[0] = CHIP_ERASE;
    ret = spi_write(spi_device, instruction_buffer, 1);
    if (ret < 0)
    {
      pr_err("failed to erase chip\n");
      return FAILURE;
    }
    do
    {
      ret = read_status_register(GET_WIP);
      printk("Status Register for Chip Erase WIP: %d\n", ret);
      mdelay(6);
    } while (ret != 0);
    break;
  }
  default:
  {
    printk("Default\n");
    break;
  }
  }
  printk("IOCTL Successful\n");
  return SUCCESS;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = eeprom_open,
    .release = eeprom_close,
    .read = eeprom_read,
    .write = eeprom_write,
    .llseek = eeprom_llseek,
    .unlocked_ioctl = eeprom_ioctl,
};

static int spi_probe(struct spi_device *spi_device)
{
  int ret_val;

  ret_val = alloc_chrdev_region(&spi_dev, 0, 1, SPI_DEV_NAME);
  if (ret_val < 0)
  {
    pr_err("Failed to allocated chrdev region\n");
    return FAILURE;
  }

  cdev_init(&spi_cdev, &fops);
  if (cdev_add(&spi_cdev, spi_dev, 1) < 0)
  {
    pr_err("failed to add cdev\n");
    unregister_chrdev_region(spi_dev, 1);
    return FAILURE;
  }

  spi_class = class_create(THIS_MODULE, SPI_DEV_NAME);
  if (spi_class == NULL)
  {
    cdev_del(&spi_cdev);
    unregister_chrdev_region(spi_dev, 1);
    pr_err("Failed to create class\n");
    return FAILURE;
  }

  if (device_create(spi_class, NULL, spi_dev, NULL, SPI_DEV_NAME) == NULL)
  {
    class_destroy(spi_class);
    cdev_del(&spi_cdev);
    unregister_chrdev_region(spi_dev, 1);
    pr_err("Failed to create device file\n");
    return FAILURE;
  }
  printk("SPI Device Probed!\n");
  return SUCCESS;
}

static int spi_remove(struct spi_device *spi_device)
{
  device_destroy(spi_class, spi_dev);
  class_destroy(spi_class);
  cdev_del(&spi_cdev);
  unregister_chrdev_region(spi_dev, 1);
  printk("SPI Device Removed\n");
  return SUCCESS;
}

#if 0
static const struct of_device_id spi_ids[] = {
    {.compatible = SPI_DEV_NAME},
    {},
};
#endif

static const struct spi_device_id spi_ids[] = {
    {SPI_DEV_NAME, 0},
    {}};

MODULE_DEVICE_TABLE(spi, spi_ids);

static struct spi_driver spi_slave_driver = {
    .driver = {
        .name = SPI_DEV_NAME,
        .owner = THIS_MODULE,
    },
    .probe = spi_probe,
    .remove = spi_remove,
    .id_table = spi_ids,
};

static struct spi_board_info spi_chip = {
    .modalias = SPI_DEV_NAME,
    .max_speed_hz = SPI_SPEED_HZ,
    .bus_num = SPI_BUS,
    .chip_select = 0,
    .mode = SPI_MODE_0};

static int __init spi_init(void)
{
  int ret_val;

  master = spi_busnum_to_master(SPI_BUS);

  if (master == NULL)
  {
    pr_err("Bus doesn't exit\n");
    return FAILURE;
  }

  spi_device = spi_new_device(master, &spi_chip);

  if (spi_device == NULL)
  {
    pr_err("Failed to create new spi device\n");
    return FAILURE;
  }

  spi_device->bits_per_word = 8; // can b 0

  ret_val = spi_setup(spi_device);

  if (ret_val < 0)
  {
    pr_err("failed to setup spi\n");
    return FAILURE;
  }

  ret_val = spi_register_driver(&spi_slave_driver);

  if (ret_val < 0)
  {
    spi_unregister_device(spi_device);
    printk("failed to register spi driver\n");
    return FAILURE;
  }

  printk("SPI Slave Driver Loaded\n");

  return SUCCESS;
}

static void __exit spi_cleanup(void)
{
  spi_unregister_driver(&spi_slave_driver);
  spi_unregister_device(spi_device);
  printk("SPI Slave Driver Unloaded\n");
}

module_init(spi_init);
module_exit(spi_cleanup);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A Simple SPI EEPROM Slave Device Driver");
MODULE_AUTHOR("Ram");
