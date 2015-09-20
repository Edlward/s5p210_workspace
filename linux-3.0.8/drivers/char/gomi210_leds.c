#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <mach/gpio.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#define DEVICE_NAME "leds"

static int led_gpios[] = {
  S5PV210_GPJ2(0),
  S5PV210_GPJ2(1),
  S5PV210_GPJ2(2),
  S5PV210_GPJ2(3),
};

#define LED_NUM ARRAY_SIZE(led_gpios)

static long gomi210_leds_ioctl(struct file *filp, unsigned int cmd,
			       unsigned long arg)
{
  switch(cmd) {
  case 0:
  case 1:
    if(arg > LED_NUM) {
      return -EINVAL;
    }
    gpio_set_value(led_gpios[arg],!cmd);
    printk(DEVICE_NAME": %d %d\n", arg, cmd);
    break;
  default:
    return -EINVAL;
  }
}

void set_leds(int data) {

  int i;
  int tmp;

  for(i = 0; i < LED_NUM; i++) {
    tmp = data & 0x01;
    printk("tmp[%d]:%d\n",i,tmp);
    gpio_set_value(led_gpios[i],!tmp);
    data = data >> 1;
  }
  
}

static ssize_t gomi210_leds_write (struct file *filp, const char __user *data_string,
				   size_t data_size, loff_t *off_set)
{
  int data = 0;
  
  //printk("string:%s\n",data_string);
  printk("size:%d\n",data_size);

  if(data_size > 1) {
    //printk("char:%c\n",data_string[0]);
    
    data = data_string[0] - 97;
    printk("data:%d\n",data);

    if(data > 15) {
      printk("out of range\n");
    } else {
      set_leds(data);
    }
    
  }

  
  return data_size;
}

static struct file_operations gomi210_led_dev_fops = {
  .owner	        = THIS_MODULE,
  .unlocked_ioctl	= gomi210_leds_ioctl,
  .write                = gomi210_leds_write,
};

static struct miscdevice gomi210_led_dev = {
  .minor		= MISC_DYNAMIC_MINOR,
  .name			= DEVICE_NAME,
  .fops			= &gomi210_led_dev_fops,
};

static int gomi_led_major = 0;

struct cdev gomi_led_cdev;

static void gomi210_led_setup_cdev() {

  int err,devno = MKDEV(gomi_led_major,0);

  cdev_init(&gomi_led_cdev,&gomi210_led_dev_fops);
  gomi_led_cdev.owner = THIS_MODULE;

  err = cdev_add(&gomi_led_cdev, devno, 1);
  if(err) {
    printk(KERN_NOTICE "Error %d adding led dev\n",err);
  }
}

static int __init gomi210_led_dev_init(void) {
  int ret;
  int i;

  printk("Gomi210 led dev init\n");
  printk("Led num:%d\n",LED_NUM);

  gomi_led_major = 230;

  dev_t devno = MKDEV(gomi_led_major, 0);

  if(gomi_led_major) {
    ret = register_chrdev_region(devno, 1, DEVICE_NAME);
  } else {
    ret = alloc_chrdev_region(&devno, 0, 1, DEVICE_NAME);
    gomi_led_major = MAJOR(devno);
  }

  printk("led major is %d\n",gomi_led_major);

  if(ret < 0) {
    return ret;
  }

  for(i = 0;i < LED_NUM; i++) {
    
    ret = gpio_request(led_gpios[i], "LED");

    printk("gpio_request return:%d\n",ret);
    
    if(ret) {
      printk("%s: request GPIO %d for LED failed, ret = %d\n",
	     DEVICE_NAME,led_gpios[i], ret);
      goto fail_init;
    }
    
    s3c_gpio_cfgpin(led_gpios[i], S3C_GPIO_OUTPUT);
    gpio_set_value(led_gpios[i], 1);

  }

  gomi210_led_setup_cdev();  
  
  //ret = misc_register(&gomi210_led_dev);
  printk("Gomi210 led dev init finish\n");
  
  return ret;

 fail_init:
  unregister_chrdev_region(devno, 1);
  return ret;
}

static void __exit gomi210_led_dev_cleanup(void) {
  int i;
  printk("Gomi210 led dev cleanup\n");
  
  for (i = 0; i < LED_NUM; i++) {
    gpio_free(led_gpios[i]);
  }

  cdev_del(&gomi_led_cdev);
  unregister_chrdev_region(MKDEV(gomi_led_major, 0), 1);

  //misc_deregister(&gomi210_led_dev);
  
  printk("Gomi210 led dev cleanup finish\n");
  
}

module_init(gomi210_led_dev_init);
module_exit(gomi210_led_dev_cleanup);

MODULE_LICENSE("GPL");

