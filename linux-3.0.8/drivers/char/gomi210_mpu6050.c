#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>

#define MPU6050_NAME "mpu6050"

#define MPU6050_SMPLRT_DIV          0x19        //陀螺仪采样率，典型值：0x07(125Hz)
#define MPU6050_CONFIG              0x1A        //低通滤波频率，典型值：0x06(5Hz)
#define MPU6050_GYRO_CONFIG         0x1B        //陀螺仪自检及测量范围，典型值：0x18(不自检，2000deg/s)
#define MPU6050_ACCEL_CONFIG        0x1C        //加速计自检、测量范围及高通滤波频率，典型值：0x01(不自检，2G，5Hz)
#define MPU6050_ACCEL_XOUT_H        0x3B
#define MPU6050_ACCEL_XOUT_L        0x3C
#define MPU6050_ACCEL_YOUT_H        0x3D
#define MPU6050_ACCEL_YOUT_L        0x3E
#define MPU6050_ACCEL_ZOUT_H        0x3F
#define MPU6050_ACCEL_ZOUT_L        0x40
#define MPU6050_TEMP_OUT_H          0x41
#define MPU6050_TEMP_OUT_L          0x42
#define MPU6050_GYRO_XOUT_H         0x43
#define MPU6050_GYRO_XOUT_L         0x44       
#define MPU6050_GYRO_YOUT_H         0x45
#define MPU6050_GYRO_YOUT_L         0x46
#define MPU6050_GYRO_ZOUT_H         0x47
#define MPU6050_GYRO_ZOUT_L         0x48
#define MPU6050_PWR_MGMT_1          0x6B        //电源管理，典型值：0x00(正常启用)
#define MPU6050_WHO_AM_I            0x75        //IIC地址寄存器(默认数值0x68，只读)
#define MPU6050_SlaveAddress        0xD0        //IIC写入时的地址字节数据，+1为读取#define

static struct i2c_client *mpu6050_client;

static volatile char mpu6050_data[] = {
  '0','0',
  '0','0',  
  '0','0',
  
  '0','0',
  '0','0',
  '0','0',
};

static int mpu6050_read_register(struct i2c_client *client,unsigned char reg) {
  unsigned char reg_data_h,reg_data_l;
  
  reg_data_h = i2c_smbus_read_byte_data(client, reg);
  reg_data_l = i2c_smbus_read_byte_data(client, reg+1);

  return (((reg_data_h << 8) + reg_data_l) & 0xffff);
}

static void mpu6050_read_data(struct i2c_client *client) {

  int accel_x,accel_y,accel_z;
  int gyro_x,gyro_y,gyro_z;
  
  accel_x = mpu6050_read_register(client,MPU6050_ACCEL_XOUT_H);
  accel_y =  mpu6050_read_register(client,MPU6050_ACCEL_YOUT_H);
  accel_z =  mpu6050_read_register(client,MPU6050_ACCEL_ZOUT_H);
  gyro_x =  mpu6050_read_register(client,MPU6050_GYRO_XOUT_H);
  gyro_y =  mpu6050_read_register(client,MPU6050_GYRO_YOUT_H);
  gyro_z =  mpu6050_read_register(client,MPU6050_GYRO_ZOUT_H);

  mpu6050_data[0] = ((accel_x >> 8) & 0xff);
  mpu6050_data[1] = (accel_x & 0xff);

  mpu6050_data[2] = ((accel_y >> 8) & 0xff);
  mpu6050_data[3] = (accel_y & 0xff);

  mpu6050_data[4] = ((accel_z >> 8) & 0xff);
  mpu6050_data[5] = (accel_z & 0xff);

  mpu6050_data[6] = ((gyro_x >> 8) & 0xff);
  mpu6050_data[7] = (gyro_x & 0xff);

  mpu6050_data[8] = ((gyro_y >> 8) & 0xff);
  mpu6050_data[9] = (gyro_y & 0xff);

  mpu6050_data[10] = ((gyro_z >> 8) & 0xff);
  mpu6050_data[11] = (gyro_z & 0xff);
  
   printk("accel x:04%x y:%04x z:%04x\n gyro x:%04x y:%04x z:%04x\n",
  	 accel_x,accel_y,accel_z,
  	 gyro_x,gyro_y,gyro_z);
  
}


static int mpu6050_open(struct inode *inode, struct file *file)
{
  return 0;
}

static int mpu6050_close(struct inode *inode, struct file *file)
{
  return 0;
}

static int mpu6050_read(struct file *filp, char __user *buff,
			size_t count, loff_t *offp)
{
  int err;
  
  if(mpu6050_client != NULL) {
    mpu6050_read_data(mpu6050_client);

    err = copy_to_user((void *)buff,
		       (const void *)(&mpu6050_data),
		       min(sizeof(mpu6050_data),count));
  
    return err ? -EFAULT : min(sizeof(mpu6050_data),count);
  } else {
    err = copy_to_user((void *)buff, "0", 1);
  
    return err ? -EFAULT : 1;
  }
}


static struct file_operations mpu6050_dev_fops = {
  .owner	= THIS_MODULE,
  .open		= mpu6050_open,
  .release	= mpu6050_close, 
  .read		= mpu6050_read,
};

static struct miscdevice mpu6050_misc = {
  .minor	= MISC_DYNAMIC_MINOR,
  .name		= MPU6050_NAME,
  .fops		= &mpu6050_dev_fops,
};

static int mpu6050_initialize(struct i2c_client *client)
{

  int val;
  
  val = i2c_smbus_read_byte_data(client, MPU6050_WHO_AM_I);
  if(val != 0x68) {
    dev_err(&client->dev, "no device\n");
    return -ENODEV;
  }

  i2c_smbus_write_byte_data(client, MPU6050_PWR_MGMT_1, 0x00);
  mdelay(10);
  i2c_smbus_write_byte_data(client, MPU6050_SMPLRT_DIV, 0x07);
  mdelay(10);
  i2c_smbus_write_byte_data(client, MPU6050_CONFIG, 0x06);
  mdelay(10);
  i2c_smbus_write_byte_data(client, MPU6050_GYRO_CONFIG, 0x18);
  mdelay(10);
  i2c_smbus_write_byte_data(client, MPU6050_ACCEL_CONFIG, 0x01);
  mdelay(10);

  mpu6050_client = client;
  
  return 0;
}

/*-----------------------------------------------------------------------------
 * I2C client driver interfaces
 */

static int __devinit mpu6050_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
  struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
  int ret;

  ret = i2c_check_functionality(adapter,
				I2C_FUNC_SMBUS_BYTE | I2C_FUNC_SMBUS_BYTE_DATA);
  if (!ret) {
    dev_err(&client->dev, "I2C check functionality failed\n");
    return -ENXIO;
  }

  if (mpu6050_initialize(client) < 0) {
    goto error_init_client;
  }
  /*
    ret = misc_register(&mpu6050_misc);
    if (ret) {
    dev_err(&client->dev, "create misc failed!\n");
    goto error_init_client;
    }
  */
	
  printk("MPU6050 device is probed successfully.\n");
  dev_info(&client->dev, "MPU6050 device is probed successfully.\n");

  return 0;

 error_init_client:
  mpu6050_client = NULL;

  return 0;
}

static int __devexit mpu6050_remove(struct i2c_client *client)
{

  //misc_deregister(&mpu6050_misc);
  mpu6050_client = NULL;

  printk("mpu6050 remove\n");

  return 0;
}

static int mpu6050_suspend(struct i2c_client *client, pm_message_t state)
{
  int ret;

  //store register state

  /*
    oper_mode = i2c_smbus_read_byte_data(client, MPU6050_MODE);

    ret = i2c_smbus_write_byte_data(client, MMA7660_MODE, 0);
    if (ret) {
    printk("%s: set mode (0) for suspend failed, ret = %d\n",
    MPU6050_NAME, ret);
    }
  */
	
  return 0;
}

static int mpu6050_resume(struct i2c_client *client)
{
  int ret;

  //restore register state

  /*
    ret = i2c_smbus_write_byte_data(client, MMA7660_MODE, oper_mode);
    if (ret) {
    printk("%s: set mode (%d) for resume failed, ret = %d\n",
    MPU6050_NAME, oper_mode, ret);
    }
  */
	
  return 0;
}

static const struct i2c_device_id mpu6050_ids[] = {
  { "mpu6050", 0 },
  { },
};

MODULE_DEVICE_TABLE(i2c, mpu6050_ids);

static struct i2c_driver i2c_mpu6050_driver = {
  .driver = {
    .name	        = MPU6050_NAME,
  },

  .probe		= mpu6050_probe,
  .remove		= __devexit_p(mpu6050_remove),
  .suspend	        = mpu6050_suspend,
  .resume		= mpu6050_resume,
  .id_table	        = mpu6050_ids,
};


static int __init init_mpu6050(void)
{
  int ret;

  ret = i2c_add_driver(&i2c_mpu6050_driver);

  ret = misc_register(&mpu6050_misc);
  printk(KERN_INFO "MPU6050 sensor driver registered.\n");

  return ret;
}

static void __exit exit_mpu6050(void)
{

  misc_deregister(&mpu6050_misc);
  
  i2c_del_driver(&i2c_mpu6050_driver);
  
  printk(KERN_INFO "MPU6050 sensor driver removed.\n");
}


module_init(init_mpu6050);
module_exit(exit_mpu6050);

MODULE_AUTHOR("GoMi");
MODULE_DESCRIPTION("MPU6050 sensor driver");
MODULE_LICENSE("GPL");

