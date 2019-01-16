/* drivers/i2c/chips/af7133.c - AF7133 compass driver
 *
 * Copyright (C) 2013 VTC Technology Inc.
 * Author: Gary Huang <gary.huang@voltafield.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>


#include <cust_mag.h>
#include "af7133.h"
#include <linux/hwmsen_helper.h>


#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>


#ifdef MEDIATEK_CODE
#include <linux/miscdevice.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <cust_mag.h>
#include <linux/hwmsen_helper.h>
#endif
#include <mach/mt_pm_ldo.h>


#define POWER_NONE_MACRO MT65XX_POWER_NONE

/*----------------------------------------------------------------------------*/
#define DEBUG 1
#define AF7133_DEV_NAME         "af7133"
#define DRIVER_VERSION          "1.0.1"
/*----------------------------------------------------------------------------*/
#define AF7133_DEFAULT_DELAY     33
/*----------------------------------------------------------------------------*/
#define MSE_TAG                  "MSENSOR"
#define MSE_FUN(f)               printk(KERN_INFO MSE_TAG" %s\r\n", __FUNCTION__)
#define MSE_ERR(fmt, args...)    printk(KERN_ERR MSE_TAG" %s %d : \r\n"fmt, __FUNCTION__, __LINE__, ##args)
#define MSE_LOG(fmt, args...)    printk(KERN_INFO MSE_TAG fmt, ##args)
#define MSE_VER(fmt, args...)   ((void)0)
static DECLARE_WAIT_QUEUE_HEAD(data_ready_wq);
static DECLARE_WAIT_QUEUE_HEAD(open_wq);

static atomic_t open_flag = ATOMIC_INIT(0);
static atomic_t m_flag = ATOMIC_INIT(0);
static atomic_t o_flag = ATOMIC_INIT(0);
/*----------------------------------------------------------------------------*/
static struct i2c_client *af7133_i2c_client = NULL;
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id af7133_i2c_id[] = {{AF7133_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_af7133={ I2C_BOARD_INFO("af7133", AF7133_I2C_ADDRESS)};  //7-bit address
/*the adapter id will be available in customization*/
//static unsigned short af7133_force[] = {0x00, AF7133_I2C_ADDRESS, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const af7133_forces[] = { af7133_force, NULL };
//static struct i2c_client_address_data af7133_addr_data = { .forces = af7133_forces,};

/*----------------------------------------------------------------------------*/
static int af7133_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int af7133_i2c_remove(struct i2c_client *client);

static int af7133_suspend(struct i2c_client *client, pm_message_t msg) ;
static int af7133_resume(struct i2c_client *client);

static struct platform_driver vtc_sensor_driver;

/*----------------------------------------------------------------------------*/
typedef enum {
    VTC_TRC_DEBUG  = 0x01,
} AMI_TRC;
/*----------------------------------------------------------------------------*/
struct _af7133_data {
    rwlock_t lock;
    int mode;
    int rate;
    volatile int updated;
} af7133_data;
/*----------------------------------------------------------------------------*/
struct _af7133mid_data {
    rwlock_t datalock;
    rwlock_t ctrllock;    
    int controldata[10];
    unsigned int debug;
    int yaw;
    int roll;
    int pitch;
    int nmx;
    int nmy;
    int nmz;
    int nax;
    int nay;
    int naz;
    int mag_status;
} af7133mid_data;
/*----------------------------------------------------------------------------*/
struct af7133_i2c_data {
    struct i2c_client *client;
    struct mag_hw *hw;
    struct hwmsen_convert   cvt;
    atomic_t layout;   
    atomic_t trace;
#if defined(CONFIG_HAS_EARLYSUSPEND)    
    struct early_suspend    early_drv;
#endif 
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver af7133_i2c_driver = {
    .driver = {
//        .owner = THIS_MODULE, 
        .name  = AF7133_DEV_NAME,
    },
	.probe      = af7133_i2c_probe,
	.remove     = af7133_i2c_remove,
//	.detect     = af7133_i2c_detect,
//#if !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend    = af7133_suspend,
	.resume     = af7133_resume,
//#endif 
	.id_table = af7133_i2c_id,
	//.address_list = af7133_forces,//address_data->address_list
};

static DEFINE_MUTEX(af7133_mutex);
#define I2C_FLAG_WRITE	0
#define I2C_FLAG_READ	1


/*----------------------------------------------------------------------------*/
static atomic_t dev_open_count;
/*----------------------------------------------------------------------------*/


/*------------------------i2c function for 89-------------------------------------*/
int af7133_i2c_master_operate(struct i2c_client *client, const char *buf, int count, int i2c_flag)
{
	int res = 0;
	mutex_lock(&af7133_mutex);
	switch(i2c_flag){	
	case I2C_FLAG_WRITE:
	client->addr &=I2C_MASK_FLAG;
	res = i2c_master_send(client, buf, count);
	client->addr &=I2C_MASK_FLAG;
	break;
	
	case I2C_FLAG_READ:
	client->addr &=I2C_MASK_FLAG;
	client->addr |=I2C_WR_FLAG;
	client->addr |=I2C_RS_FLAG;
	res = i2c_master_send(client, buf, count);
	client->addr &=I2C_MASK_FLAG;
	break;
	default:
	MSE_LOG("af7133_i2c_master_operate i2c_flag command not support!\n");
	break;
	}
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	mutex_unlock(&af7133_mutex);
	return res;
	EXIT_ERR:
	mutex_unlock(&af7133_mutex);
	MSE_ERR("af7133_i2c_transfer fail\n");
	return res;
}



static void af7133_power(struct mag_hw *hw, unsigned int on) 
{
	static unsigned int power_on = 0;

	if(hw->power_id != POWER_NONE_MACRO)
	{        
		MSE_LOG("power %s\n", on ? "on" : "off");
		if(power_on == on)
		{
			MSE_LOG("ignore power control: %d\n", on);
		}
		else if(on)
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "AF7133")) 
			{
				MSE_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "AF7133")) 
			{
				MSE_ERR("power off fail!!\n");
			}
		}
	}
	power_on = on;
}
/*----------------------------------------------------------------------------*/

static int af7133_GetOpenStatus(void)
{
	wait_event_interruptible(open_wq, (atomic_read(&open_flag) != 0));
	return atomic_read(&open_flag);
}

/*----------------------------------------------------------------------------*/
static int AF7133_Chipset_Init(int mode)
{

	u8 databuf[2];
	u8 vaule;
	int ret;

	af7133_i2c_client->addr = af7133_i2c_client->addr & I2C_MASK_FLAG;

	
	databuf[0] = AF7133_REG_PCODE;
/*** 
  if(ret = i2c_master_send(af7133_i2c_client, databuf, 1) != 1)
  {
		MSE_ERR("send AF7133_REG_PCODE cmd fail: %d\n", ret);
		return -1;  	
  }
  if(ret = i2c_master_recv(af7133_i2c_client, databuf, 1) != 1)
  {
		MSE_ERR("recv AF7133_REG_PCODE data fail: %d\n", ret);
		return -2;  	
  }
  ***/
  

  af7133_i2c_master_operate(af7133_i2c_client, databuf, 0x101, I2C_FLAG_READ);
  	
	if(databuf[0] != AF7133_PCODE_VALUE)
	{
		MSE_ERR("AF7133_REG_PCODE is incorrect: %d\n", databuf[0]);
		return -3;
	} 
    else{
        printk("%s chip id:%#x\n",__func__,databuf[0]);
    }

  if(mode == AF7133_MODE_SINGLE)
  {
	  databuf[0] = AF7133_REG_MODE;
	  databuf[1] = AF7133_MODE_SINGLE;
	  //i2c_master_send(af7133_i2c_client, databuf, 2);  
	  af7133_i2c_master_operate(af7133_i2c_client, databuf, 0x02, I2C_FLAG_WRITE);
  }
	else if(mode == AF7133_MODE_WAKE)
	{
	  databuf[0] = AF7133_REG_RATE;
	  databuf[1] = AF7133_RATE_25HZ;
	  //i2c_master_send(af7133_i2c_client, databuf, 2); 
	  af7133_i2c_master_operate(af7133_i2c_client, databuf, 0x02, I2C_FLAG_WRITE);

	  databuf[0] = AF7133_REG_MODE;
	  databuf[1] = AF7133_MODE_WAKE;
	  //i2c_master_send(af7133_i2c_client, databuf, 2);   
	  af7133_i2c_master_operate(af7133_i2c_client, databuf, 0x02, I2C_FLAG_WRITE);
  }	
  else
	{
	  databuf[0] = AF7133_REG_MODE;
	  databuf[1] = AF7133_MODE_IDLE;
	  //i2c_master_send(af7133_i2c_client, databuf, 2);      
	  af7133_i2c_master_operate(af7133_i2c_client, databuf, 0x02, I2C_FLAG_WRITE);
  }
	
	write_lock(&af7133_data.lock);
	af7133_data.mode = mode;
	write_unlock(&af7133_data.lock);

	return 0;
}
/*----------------------------------------------------------------------------*/
static int AF7133_SetMode(int newmode)
{
	int mode = 0;

	read_lock(&af7133_data.lock);
	mode = af7133_data.mode;
	read_unlock(&af7133_data.lock);        

	if(mode == newmode)
	{
		return 0;    
	}

	return AF7133_Chipset_Init(newmode);
}
/*----------------------------------------------------------------------------*/
static int AF7133_ReadSensorData(int *buf, int bufsize)
{
	struct af7133_i2c_data *data = i2c_get_clientdata(af7133_i2c_client);  
	unsigned char databuf[10];
	int output[3];
	
	if(NULL == af7133_i2c_client)
	{
		*buf = 0;
		return -2;
	}    
   
	// We can read all measured data in once
	databuf[0] = AF7133_REG_DATA;
	//i2c_master_send(af7133_i2c_client, databuf, 1);    
	//i2c_master_recv(af7133_i2c_client, databuf, 6);
	af7133_i2c_master_operate(af7133_i2c_client, databuf, 0x601, I2C_FLAG_READ);

	output[0] = ((int) databuf[1]) << 8 | ((int) databuf[0]);
	output[1] = ((int) databuf[3]) << 8 | ((int) databuf[2]);
	output[2] = ((int) databuf[5]) << 8 | ((int) databuf[4]);

	buf[0] = output[0] > 32767 ? output[0] - 65536 : output[0];
	buf[1] = output[1] > 32767 ? output[1] - 65536 : output[1];
	buf[2] = output[2] > 32767 ? output[2] - 65536 : output[2];

	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_daemon_name(struct device_driver *ddri, char *buf)
{
	char strbuf[AF7133_BUFSIZE];
	sprintf(strbuf, "af7133d");
	return sprintf(buf, "%s", strbuf);		
}

static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[AF7133_BUFSIZE];
	//AF7133_ReadChipInfo(strbuf, AF7133_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);        
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	int strbuf[AF7133_BUFSIZE];
	AF7133_ReadSensorData(strbuf, AF7133_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_posturedata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[AF7133_BUFSIZE];
	//AF7133_ReadPostureData(strbuf, AF7133_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_calidata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[AF7133_BUFSIZE];
	//AF7133_ReadCaliData(strbuf, AF7133_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_midcontrol_value(struct device_driver *ddri, char *buf)
{
	char strbuf[AF7133_BUFSIZE];
	//AF7133_ReadMiddleControl(strbuf, AF7133_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_midcontrol_value(struct device_driver *ddri, const char *buf, size_t count)
{   
	int p[10];
	if(10 == sscanf(buf, "%d %d %d %d %d %d %d %d %d %d",&p[0], &p[1], &p[2], &p[3], &p[4], 
		&p[5], &p[6], &p[7], &p[8], &p[9]))
	{
		write_lock(&af7133mid_data.ctrllock);
		memcpy(&af7133mid_data.controldata[0], &p, sizeof(int)*10);    
		write_unlock(&af7133mid_data.ctrllock);        
	}
	else
	{
		MSE_ERR("invalid format\n");     
	}
	return sizeof(int)*10;            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_middebug_value(struct device_driver *ddri, char *buf)
{
	ssize_t len;
	read_lock(&af7133mid_data.ctrllock);
	len = sprintf(buf, "0x%08X\n", af7133mid_data.debug);
	read_unlock(&af7133mid_data.ctrllock);

	return len;            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_middebug_value(struct device_driver *ddri, const char *buf, size_t count)
{   
	int debug;
	if(1 == sscanf(buf, "0x%x", &debug))
	{
		write_lock(&af7133mid_data.ctrllock);
		af7133mid_data.debug = debug;
		write_unlock(&af7133mid_data.ctrllock);        
	}
	else
	{
		MSE_ERR("invalid format\n");     
	}
	return count;            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_mode_value(struct device_driver *ddri, char *buf)
{
	int mode=0;
	read_lock(&af7133_data.lock);
	mode = af7133_data.mode;
	read_unlock(&af7133_data.lock);        
	return sprintf(buf, "%d\n", mode);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_mode_value(struct device_driver *ddri, const char *buf, size_t count)
{
	int mode = 0;
	sscanf(buf, "%d", &mode);    
	AF7133_SetMode(mode);
	return count;            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = af7133_i2c_client;  
	struct af7133_i2c_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		data->hw->direction,atomic_read(&data->layout),	data->cvt.sign[0], data->cvt.sign[1],
		data->cvt.sign[2],data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct i2c_client *client = af7133_i2c_client;  
	struct af7133_i2c_data *data = i2c_get_clientdata(client);
	int layout = 0;

	if(1 == sscanf(buf, "%d", &layout))
	{
		atomic_set(&data->layout, layout);
		if(!hwmsen_get_convert(layout, &data->cvt))
		{
			MSE_ERR("HWMSEN_GET_CONVERT function error!\r\n");
		}
		else if(!hwmsen_get_convert(data->hw->direction, &data->cvt))
		{
			MSE_ERR("invalid layout: %d, restore to %d\n", layout, data->hw->direction);
		}
		else
		{
			MSE_ERR("invalid layout: (%d, %d)\n", layout, data->hw->direction);
			hwmsen_get_convert(0, &data->cvt);
		}
	}
	else
	{
		MSE_ERR("invalid format = '%s'\n", buf);
	}

	return count;            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = af7133_i2c_client;  
	struct af7133_i2c_data *data = i2c_get_clientdata(client);
	ssize_t len = 0;

	if(data->hw)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d %d (%d %d)\n", 
			data->hw->i2c_num, data->hw->direction, data->hw->power_id, data->hw->power_vol);
	}
	else
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}

	len += snprintf(buf+len, PAGE_SIZE-len, "OPEN: %d\n", atomic_read(&dev_open_count));
	return len;
}
static ssize_t store_status_value(struct device_driver *ddri, const char *buf, size_t count)

{
    
	struct i2c_client *client = af7133_i2c_client;  
	struct af7133_i2c_data *data = i2c_get_clientdata(client);
    int value = simple_strtol(buf, NULL, 10);
    
    data->hw->direction = value;
	if(hwmsen_get_convert(value, &data->cvt)<0)
	{
		MSE_ERR("invalid direction: %d\n", value);
	}

	atomic_set(&data->layout, value);
	return count;    

}
/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct af7133_i2c_data *obj = i2c_get_clientdata(af7133_i2c_client);
	if(NULL == obj)
	{
		MSE_ERR("af7133_i2c_data is null!!\n");
		return 0;
	}	

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct af7133_i2c_data *obj = i2c_get_clientdata(af7133_i2c_client);
	int trace;
	if(NULL == obj)
	{
		MSE_ERR("af7133_i2c_data is null!!\n");
		return 0;
	}

	if(1 == sscanf(buf, "0x%x", &trace))
	{
		atomic_set(&obj->trace, trace);
	}
	else 
	{
		MSE_ERR("invalid content: '%s', length = %d\n", buf, count);
	}

	return count;    
}


/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(daemon,      S_IRUGO, show_daemon_name, NULL);
static DRIVER_ATTR(chipinfo,    S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata,  S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(posturedata, S_IRUGO, show_posturedata_value, NULL);
static DRIVER_ATTR(calidata,    S_IRUGO, show_calidata_value, NULL);
static DRIVER_ATTR(midcontrol,  S_IRUGO | S_IWUSR, show_midcontrol_value, store_midcontrol_value );
static DRIVER_ATTR(middebug,    S_IRUGO | S_IWUSR, show_middebug_value, store_middebug_value );
static DRIVER_ATTR(mode,        S_IRUGO | S_IWUSR, show_mode_value, store_mode_value );
static DRIVER_ATTR(layout,      S_IRUGO | S_IWUSR, show_layout_value, store_layout_value );
static DRIVER_ATTR(status,      S_IRUGO | S_IWUSR, show_status_value, store_status_value);
static DRIVER_ATTR(trace,       S_IRUGO | S_IWUSR, show_trace_value, store_trace_value );
/*----------------------------------------------------------------------------*/
static struct driver_attribute *af7133_attr_list[] = {
    &driver_attr_daemon,
	&driver_attr_chipinfo,
	&driver_attr_sensordata,
	&driver_attr_posturedata,
	&driver_attr_calidata,
	&driver_attr_midcontrol,
	&driver_attr_middebug,
	&driver_attr_mode,
	&driver_attr_layout,
	&driver_attr_status,
	&driver_attr_trace,
};
/*----------------------------------------------------------------------------*/
static int af7133_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(af7133_attr_list)/sizeof(af7133_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, af7133_attr_list[idx])))
		{            
			MSE_ERR("driver_create_file (%s) = %d\n", af7133_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int af7133_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(af7133_attr_list)/sizeof(af7133_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}


	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, af7133_attr_list[idx]);
	}


	return err;
}


/*----------------------------------------------------------------------------*/
static int af7133_open(struct inode *inode, struct file *file)
{    
	struct af7133_i2c_data *obj = i2c_get_clientdata(af7133_i2c_client);    
	int ret = -1;
	atomic_inc(&dev_open_count);

	if(atomic_read(&obj->trace) & VTC_TRC_DEBUG)
	{
		MSE_LOG("Open device node:af7133\n");
	}
	ret = nonseekable_open(inode, file);

	return ret;
}
/*----------------------------------------------------------------------------*/
static int af7133_release(struct inode *inode, struct file *file)
{
	struct af7133_i2c_data *obj = i2c_get_clientdata(af7133_i2c_client);
	atomic_dec(&dev_open_count);
	if(atomic_read(&obj->trace) & VTC_TRC_DEBUG)
	{
		MSE_LOG("Release device node:af7133\n");
	}	
	return 0;
}
/*----------------------------------------------------------------------------*/
//static int af7133_ioctl(struct inode *inode, struct file *file, unsigned int cmd,unsigned long arg)//modified here
static long af7133_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
    void __user *argp = (void __user *)arg;
	int valuebuf[10];
	int magbuf[3];
	char strbuf[AF7133_BUFSIZE];
	void __user *data;
	long retval=0;
	int mode=0;
	int status; 				/* for OPEN/CLOSE_STATUS */
	short sensor_status;		/* for Orientation and Msensor status */
    struct mag_hw *hw = get_cust_mag_hw();
	int mag_layout;

	switch (cmd)
	{
		case MSENSOR_IOCTL_INIT:      
			break;

		case MSENSOR_IOCTL_SET_POSTURE:
			data = (void __user *) arg;
			if(data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}

			if(copy_from_user(valuebuf, data, sizeof(valuebuf)))
			{
				retval = -EFAULT;
				goto err_out;
			}

      MSE_ERR("af7133 driver: Osensor %d %d %d\n", valuebuf[0], valuebuf[1], valuebuf[2]);
			write_lock(&af7133mid_data.datalock);
			af7133mid_data.yaw   = valuebuf[0];
			af7133mid_data.pitch = valuebuf[1];
			af7133mid_data.roll  = valuebuf[2];
			af7133mid_data.nmx = valuebuf[3];
			af7133mid_data.nmy = valuebuf[4];
			af7133mid_data.nmz = valuebuf[5];
			af7133mid_data.nax = valuebuf[6];
			af7133mid_data.nay = valuebuf[7];
			af7133mid_data.naz = valuebuf[8];
			af7133mid_data.mag_status = valuebuf[9];
			write_unlock(&af7133mid_data.datalock);    
			break;

		case ECOMPASS_IOC_GET_OFLAG:
			sensor_status = atomic_read(&o_flag);
			if(copy_to_user(argp, &sensor_status, sizeof(sensor_status)))
			{
				MSE_ERR("copy_to_user failed.");
				return -EFAULT;
			}
			break;

		case ECOMPASS_IOC_GET_MFLAG:
			sensor_status = atomic_read(&m_flag);
			if(copy_to_user(argp, &sensor_status, sizeof(sensor_status)))
			{
				MSE_ERR("copy_to_user failed.");
				return -EFAULT;
			}
			break;

		case ECOMPASS_IOC_GET_OPEN_STATUS:
			status = af7133_GetOpenStatus();			
			if(copy_to_user(argp, &status, sizeof(status)))
			{
				MSE_LOG("copy_to_user failed.");
				return -EFAULT;
			}
			break;        

		case MSENSOR_IOCTL_SET_CALIDATA:
			break;                                

		case MSENSOR_IOCTL_READ_CHIPINFO:          
			break;

		case MSENSOR_IOCTL_SENSOR_ENABLE:
			break;

		case MSENSOR_IOCTL_READ_SENSORDATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;    
			}
			AF7133_ReadSensorData(magbuf, AF7133_BUFSIZE);
			
			MSE_ERR("af7133 driver: Msensor %d %d %d\n", magbuf[0], magbuf[1], magbuf[2]);
			
			if(copy_to_user(data, magbuf, sizeof(magbuf)))
			{
				retval = -EFAULT;
				goto err_out;
			}                
			break;

		case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
			break;                

		case MSENSOR_IOCTL_READ_POSTUREDATA:             
			break;            

		case MSENSOR_IOCTL_READ_CALIDATA:           
			break;

		case MSENSOR_IOCTL_READ_CONTROL:                         
			break;

		case MSENSOR_IOCTL_SET_CONTROL:
			break;

		case MSENSOR_IOCTL_SET_MODE:
			data = (void __user *) arg;
			if(data == NULL)
			{
				break;
			}
			if(copy_from_user(&mode, data, sizeof(mode)))
			{
				retval = -EFAULT;
				goto err_out;
			}             
			if(AF7133_SetMode(mode))
			{
				retval = -EFAULT;
				goto err_out;				
			}
			break;

        case ECOMPASS_IOC_GET_LAYOUT:
			mag_layout = hw->direction;
			if(copy_to_user(argp, &mag_layout, sizeof(mag_layout)))
			{
				retval = -EFAULT;
				goto err_out;
			}  
		    break;

		default:
			MSE_ERR("%s not supported = 0x%04x", __FUNCTION__, cmd);
			retval = -ENOIOCTLCMD;
			break;
		}

	err_out:
	return retval;    
}
/*----------------------------------------------------------------------------*/
static struct file_operations af7133_fops = {
//	.owner = THIS_MODULE,
	.open = af7133_open,
	.release = af7133_release,
	.unlocked_ioctl = af7133_unlocked_ioctl,//modified
};
/*----------------------------------------------------------------------------*/
static struct miscdevice af7133_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "msensor",
    .fops = &af7133_fops,
};
/*----------------------------------------------------------------------------*/
int af7133_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value, sample_delay, status;
	hwm_sensor_data* msensor_data;

	//MSE_FUN(f);
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				MSE_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value <= AF7133_DEFAULT_DELAY)
				{
					sample_delay = AF7133_DEFAULT_DELAY;
				}

				af7133mid_data.controldata[0] = sample_delay;  // Loop Delay
			}	
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				MSE_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				read_lock(&af7133mid_data.ctrllock);
				if(value == 1)
				{
					af7133mid_data.controldata[7] |= SENSOR_MAGNETIC;
					atomic_set(&m_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					af7133mid_data.controldata[7] &= ~SENSOR_MAGNETIC;
					atomic_set(&m_flag, 0);
					if(atomic_read(&o_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}	
				}
				wake_up(&open_wq);
				read_unlock(&af7133mid_data.ctrllock);
				// TODO: turn device into standby or normal mode
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				MSE_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				msensor_data = (hwm_sensor_data *)buff_out;
				read_lock(&af7133mid_data.datalock);
				msensor_data->values[0] = af7133mid_data.nmx;
				msensor_data->values[1] = af7133mid_data.nmy;
				msensor_data->values[2] = af7133mid_data.nmz;
				msensor_data->status = af7133mid_data.mag_status;
				read_unlock(&af7133mid_data.datalock); 

				msensor_data->values[0] = msensor_data->values[0] * CONVERT_M;
				msensor_data->values[1] = msensor_data->values[1] * CONVERT_M;
				msensor_data->values[2] = msensor_data->values[2] * CONVERT_M;
				msensor_data->value_divide = CONVERT_M_DIV;
			}
			break;
		default:
			MSE_ERR("msensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}

	return err;
}

/*----------------------------------------------------------------------------*/
int af7133_orientation_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value, sample_delay, status=0;
	hwm_sensor_data* osensor_data=NULL;

	MSE_FUN(f);
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				MSE_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value <= AF7133_DEFAULT_DELAY)
				{
					sample_delay = AF7133_DEFAULT_DELAY;
				}

				af7133mid_data.controldata[0] = sample_delay;  // Loop Delay
			}	
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				MSE_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				read_lock(&af7133mid_data.ctrllock);
				if(value == 1)
				{
					af7133mid_data.controldata[7] |= SENSOR_ORIENTATION;
					atomic_set(&o_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					af7133mid_data.controldata[7] &= ~SENSOR_ORIENTATION;
					atomic_set(&o_flag, 0);
					if(atomic_read(&m_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}
				}
				wake_up(&open_wq);
				read_unlock(&af7133mid_data.ctrllock);
				// Do nothing
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				MSE_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				osensor_data = (hwm_sensor_data *)buff_out;
				read_lock(&af7133mid_data.datalock);
				osensor_data->values[0] = af7133mid_data.yaw;
				osensor_data->values[1] = af7133mid_data.pitch;
				osensor_data->values[2] = af7133mid_data.roll;
				osensor_data->status = af7133mid_data.mag_status;	
				read_unlock(&af7133mid_data.datalock); 

				osensor_data->value_divide = CONVERT_O_DIV;
			}
			break;
			
		default:
			MSE_ERR("gsensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}

	return err;
}

/*----------------------------------------------------------------------------*/
//#ifndef	CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int af7133_suspend(struct i2c_client *client, pm_message_t msg) 
{
	int err;
	struct af7133_i2c_data *obj = i2c_get_clientdata(client);
	MSE_FUN();    

	if(msg.event == PM_EVENT_SUSPEND)
	{   
	  af7133_power(obj->hw, 0);
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int af7133_resume(struct i2c_client *client)
{
	int err;
	struct af7133_i2c_data *obj = i2c_get_clientdata(client);
	MSE_FUN();

	af7133_power(obj->hw, 1);

	if((err = AF7133_Chipset_Init(AF7133_MODE_IDLE))!=0)
	{
		MSE_ERR("initialize client fail!!\n");
		return err;        
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
//#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void af7133_early_suspend(struct early_suspend *h) 
{
	struct af7133_i2c_data *obj = container_of(h, struct af7133_i2c_data, early_drv);   
	int err;
	MSE_FUN();    

	if(NULL == obj)
	{
		MSE_ERR("null pointer!!\n");
		return;
	}
}
/*----------------------------------------------------------------------------*/
static void af7133_late_resume(struct early_suspend *h)
{
	struct af7133_i2c_data *obj = container_of(h, struct af7133_i2c_data, early_drv);         
	int err;
	MSE_FUN();

	if(NULL == obj)
	{
		MSE_ERR("null pointer!!\n");
		return;
	}

	af7133_power(obj->hw, 1);
}
/*----------------------------------------------------------------------------*/
//#endif /*CONFIG_HAS_EARLYSUSPEND*/

/*----------------------------------------------------------------------------*/
static int af7133_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct af7133_i2c_data *data;
	int err = 0;
	struct hwmsen_object sobj_m, sobj_o;
    
    printk("%s start\n",__func__);
	if (!(data = kmalloc(sizeof(struct af7133_i2c_data), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	memset(data, 0, sizeof(struct af7133_i2c_data));

	data->hw = get_cust_mag_hw();
	if((err = hwmsen_get_convert(data->hw->direction, &data->cvt)))
	{
		MSE_ERR("invalid direction: %d\n", data->hw->direction);
		goto exit;
	}

	atomic_set(&data->layout, data->hw->direction);
	atomic_set(&data->trace, 0);
	init_waitqueue_head(&data_ready_wq);
	init_waitqueue_head(&open_wq);

	data->client = client;
	new_client = data->client;
	i2c_set_clientdata(new_client, data);

	af7133_i2c_client = new_client;	

	if((err = AF7133_Chipset_Init(AF7133_MODE_IDLE)))
	{
		goto exit_init_failed;
	}

	/* Register sysfs attribute */
	if((err = af7133_create_attr(&vtc_sensor_driver.driver)))
	{
		MSE_ERR("create attribute err = %d\n", err);
		goto exit_sysfs_create_group_failed;
	}


	if((err = misc_register(&af7133_device)))
	{
		MSE_ERR("af7133_device register failed\n");
		goto exit_misc_device_register_failed;	}    

	sobj_m.self = data;
    sobj_m.polling = 1;
    sobj_m.sensor_operate = af7133_operate;
	if((err = hwmsen_attach(ID_MAGNETIC, &sobj_m)))
	{
		MSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}

	sobj_o.self = data;
    sobj_o.polling = 1;
    sobj_o.sensor_operate = af7133_orientation_operate;
	if((err = hwmsen_attach(ID_ORIENTATION, &sobj_o)))
	{
		MSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}

#if CONFIG_HAS_EARLYSUSPEND
	data->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	data->early_drv.suspend  = af7133_early_suspend,
	data->early_drv.resume   = af7133_late_resume,    
	register_early_suspend(&data->early_drv);
#endif

	MSE_LOG("%s: OK\n", __func__);
	return 0;

	exit_sysfs_create_group_failed:   
	exit_init_failed:
	//i2c_detach_client(new_client);
	exit_misc_device_register_failed:
	exit_kfree:
	kfree(data);
	exit:
	MSE_ERR("%s: err = %d\n", __func__, err);
	return err;
}
/*----------------------------------------------------------------------------*/
static int af7133_i2c_remove(struct i2c_client *client)
{
	int err;	

	if((err = af7133_delete_attr(&vtc_sensor_driver.driver)))
	{
		MSE_ERR("af7133_delete_attr fail: %d\n", err);
	}

	af7133_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));	
	misc_deregister(&af7133_device);    
	return 0;
}
/*----------------------------------------------------------------------------*/
static int vtc_probe(struct platform_device *pdev) 
{
	struct mag_hw *hw = get_cust_mag_hw();

	af7133_power(hw, 1);    
	rwlock_init(&af7133mid_data.ctrllock);
	rwlock_init(&af7133mid_data.datalock);
	rwlock_init(&af7133_data.lock);
	memset(&af7133mid_data.controldata[0], 0, sizeof(int)*10);    
	af7133mid_data.controldata[0] =    AF7133_DEFAULT_DELAY;  // Loop Delay
	af7133mid_data.controldata[1] =     0;  // Run   
	af7133mid_data.controldata[2] =     0;  // Disable Start-AccCali
	af7133mid_data.controldata[3] =     1;  // Enable Start-Cali
	af7133mid_data.controldata[4] =   350;  // MW-Timout
	af7133mid_data.controldata[5] =    10;  // MW-IIRStrength_M
	af7133mid_data.controldata[6] =    10;  // MW-IIRStrength_G   
	af7133mid_data.controldata[7] =     0;  // Active Sensors
	af7133mid_data.controldata[8] =     0;  // Wait for define
	af7133mid_data.controldata[9] =     0;  // Wait for define   
	atomic_set(&dev_open_count, 0);

	if(i2c_add_driver(&af7133_i2c_driver))
	{
		MSE_ERR("add driver error\n");
		return -1;
	} 
    printk("%s done\n",__func__);
	return 0;
}
/*----------------------------------------------------------------------------*/
static int vtc_remove(struct platform_device *pdev)
{
	struct mag_hw *hw = get_cust_mag_hw();

	MSE_FUN();    
	af7133_power(hw, 0);    
	atomic_set(&dev_open_count, 0);  
	i2c_del_driver(&af7133_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id msensor_of_match[] = {
	{ .compatible = "mediatek,msensor", },
	{},
};
#endif
static struct platform_driver vtc_sensor_driver = {
	.probe      = vtc_probe,
	.remove     = vtc_remove,    
	.driver     = {
		.name  = "msensor",
        #ifdef CONFIG_OF
		.of_match_table = msensor_of_match,
		#endif
	}
};

/*----------------------------------------------------------------------------*/
static int __init af7133_init(void)
{
	MSE_FUN();
    
	struct mag_hw *hw = get_cust_mag_hw();
	i2c_register_board_info(hw->i2c_num, &i2c_af7133, 1);
	if(platform_driver_register(&vtc_sensor_driver))
	{
		MSE_ERR("af7133: failed to register driver");
		return -ENODEV;
	}
	else
	{
		MSE_ERR("af7133: passed to register driver");
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit af7133_exit(void)
{
	MSE_FUN();
	platform_driver_unregister(&vtc_sensor_driver);
}
/*----------------------------------------------------------------------------*/
module_init(af7133_init);
module_exit(af7133_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Gary Huang");
MODULE_DESCRIPTION("AF7133 m-Sensor driver");
MODULE_LICENSE("VTC");
MODULE_VERSION(DRIVER_VERSION);

