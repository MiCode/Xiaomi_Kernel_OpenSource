/* drivers/i2c/chips/hscdtd006.c - HSCDTD006 compass driver
 *
 * Copyright (C) 2009 ALPS Technology Inc. 
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
#include "hscdtd006.h"
#include <linux/hwmsen_helper.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>




/*-------------------------MT6516&MT6573 define-------------------------------*/

#define POWER_NONE_MACRO MT65XX_POWER_NONE

/*----------------------------------------------------------------------------*/
#define I2C_DRIVERID_HSCDTD006 304
#define DEBUG 1
#define HSCDTD006_DEV_NAME         "hscdtd006"
#define DRIVER_VERSION          "1.0.6.11"
/*----------------------------------------------------------------------------*/
#define HSCDTD006_AXIS_X            0
#define HSCDTD006_AXIS_Y            1
#define HSCDTD006_AXIS_Z            2
#define HSCDTD006_AXES_NUM          3
/*------------------------Msensor vendor struct------------------------------------*/
static struct TAIFD_HW_DATA sns_hw_data;
static struct TAIFD_SW_DATA sns_sw_data;
static int flgActivate = ACTIVE_SS_NUL;

/*----------------------------------------------------------------------------*/
#if 1
#define MSENSOR						   0x83
#define ALPSIO_AD_GET_ACTIVATE     _IOR(MSENSOR, 0x30, int)
#define ALPSIO_AD_GET_DELAY        _IOR(MSENSOR, 0x31, int)
#define ALPSIO_AD_GET_DATA         _IOR(MSENSOR, 0x32, int[11])
#define ALPSIO_AD_SET_DATA         _IOW(MSENSOR, 0x33, int[15])
#define ALPSIO_AD_EXE_SELF_TEST_A  _IOR(MSENSOR, 0x34, int)
#define ALPSIO_AD_EXE_SELF_TEST_B  _IOR(MSENSOR, 0x35, int)
#endif

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
static struct i2c_client *hscdtd006_i2c_client = NULL;
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id hscdtd006_i2c_id[] = {{HSCDTD006_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_hscdtd006={ I2C_BOARD_INFO("hscdtd006", (0x18>>1))};
/*the adapter id will be available in customization*/
//static unsigned short hscdtd006_force[] = {0x00, HSCDTD006_I2C_ADDRESS, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const hscdtd006_forces[] = { hscdtd006_force, NULL };
//static struct i2c_client_address_data hscdtd006_addr_data = { .forces = hscdtd006_forces,};

/*----------------------------------------------------------------------------*/
static int hscdtd006_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int hscdtd006_i2c_remove(struct i2c_client *client);
//static int hscdtd006_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int hscdtd006_suspend(struct i2c_client *client, pm_message_t msg) ;
static int hscdtd006_resume(struct i2c_client *client);

static struct platform_driver alps_sensor_driver;

/*----------------------------------------------------------------------------*/
typedef enum {
    ALPS_TRC_DEBUG  = 0x01,
	ALPS_TRC_DAEMON = 0x02,
} ALPS_TRC;
/*----------------------------------------------------------------------------*/
struct _alps302_data {
    rwlock_t lock;
    int mode;
    int rate;
    volatile int updated;
} hscdtd006_data;
/*----------------------------------------------------------------------------*/
struct _hscdtd006mid_data {
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
	int ori_status;
} hscdtd006mid_data;
/*----------------------------------------------------------------------------*/
struct hscdtd006_i2c_data {
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
static struct i2c_driver hscdtd006_i2c_driver = {
    .driver = {
//        .owner = THIS_MODULE, 
        .name  = HSCDTD006_DEV_NAME,
    },
	.probe      = hscdtd006_i2c_probe,
	.remove     = hscdtd006_i2c_remove,
//	.detect     = hscdtd006_i2c_detect,
//#if !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend    = hscdtd006_suspend,
	.resume     = hscdtd006_resume,
//#endif 
	.id_table = hscdtd006_i2c_id,
	//.address_list = hscdtd006_forces,//address_data->address_list
};
/*----------------------------------------------------------------------------*/
static atomic_t dev_open_count;
/*----------------------------------------------------------------------------*/
static void hscdtd006_power(struct mag_hw *hw, unsigned int on) 
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
			if(!hwPowerOn(hw->power_id, hw->power_vol, "HSCDTD006")) 
			{
				MSE_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "HSCDTD006")) 
			{
				MSE_ERR("power off fail!!\n");
			}
		}
	}
	power_on = on;
}
/*----------------------------------------------------------------------------*/

static int hscdtd006_GetOpenStatus(void)
{
	wait_event_interruptible(open_wq, (atomic_read(&open_flag) != 0));
	return atomic_read(&open_flag);
}


static int hscdtd006_gpio_config(void)
{
    //because we donot use EINT ,to support low power
    // config to GPIO input mode + PD    
    //set   GPIO_MSE_EINT_PIN
    mt_set_gpio_mode(73, 0);
    mt_set_gpio_dir(73, GPIO_DIR_OUT);
	mt_set_gpio_out(73,GPIO_OUT_ZERO);
	
	mdelay(5);
	
	mt_set_gpio_out(73,GPIO_OUT_ONE);
	
	mdelay(20);
	
	mt_set_gpio_out(73,GPIO_OUT_ZERO);
	
	mdelay(20);
	return 0;
}

static int HSCDTD006_Chipset_Init(int mode)
{
	u8 databuf[10];
	u8 ctrl1, ctrl2, ctrl3;
	int err;

	hscdtd006_gpio_config();
	hscdtd006_i2c_client->addr = (hscdtd006_i2c_client->addr & I2C_MASK_FLAG )|(I2C_ENEXT_FLAG);

	if((err = hwmsen_read_byte(hscdtd006_i2c_client, HSCDTD006_REG_CTRL1, &ctrl1)))
	{
		MSE_ERR("read CTRL1 fail: %d\n", err);
		return err;
	}
	
	if((err = hwmsen_read_byte(hscdtd006_i2c_client, HSCDTD006_REG_CTRL2, &ctrl2)))
	{
		MSE_ERR("read CTRL2 fail: %d\n", err);
		return err;        
	}
	
	if((err = hwmsen_read_byte(hscdtd006_i2c_client, HSCDTD006_REG_CTRL3, &ctrl3)))
	{
		MSE_ERR("read CTRL3 fail: %d\n", err);
		return err;
	}     
	//SW reset
	databuf[0] = HSCDTD006_REG_CTRL3;
	databuf[1] = HSCDTD006_CTRL3_SRST;
	i2c_master_send(hscdtd006_i2c_client, databuf, 2);
	mdelay(5);

	databuf[0] = HSCDTD006_REG_CTRL1;
	
	if(mode==HSCDTD006_FORCE_MODE)
	{
		databuf[1] = ctrl1 | HSCDTD006_CTRL1_PC1 | HSCDTD006_CTRL1_FS1_FORCE |HSCDTD006_CTRL1_RSX_ODR1;
		write_lock(&hscdtd006_data.lock);
		hscdtd006_data.mode = HSCDTD006_FORCE_MODE;
		write_unlock(&hscdtd006_data.lock);
	}
	else    
	{
		databuf[1] = ctrl1 | HSCDTD006_CTRL1_PC1 | HSCDTD006_CTRL1_FS1_NORMAL | HSCDTD006_CTRL1_RSX_ODR1;
		write_lock(&hscdtd006_data.lock);
		hscdtd006_data.mode = HSCDTD006_NORMAL_MODE;
		write_unlock(&hscdtd006_data.lock);
	}
	
	i2c_master_send(hscdtd006_i2c_client, databuf, 2);        

	//start measure in Force state
	databuf[0] = HSCDTD006_REG_CTRL3;
	databuf[1] = HSCDTD006_CTRL3_FORCE_BIT;
	i2c_master_send(hscdtd006_i2c_client, databuf, 2);


	return 0;
}
/*----------------------------------------------------------------------------*/
static int HSCDTD006_SetMode(int newmode)
{
	int mode = 0;

	read_lock(&hscdtd006_data.lock);
	mode = hscdtd006_data.mode;
	read_unlock(&hscdtd006_data.lock);        

	if(mode == newmode)
	{
		return 0;    
	}
	
	return HSCDTD006_Chipset_Init(newmode);
}
/*----------------------------------------------------------------------------*/
static int HSCDTD006_ReadChipInfo(char *buf, int bufsize)
{
	if((!buf)||(bufsize<=30))
	{
		return -1;
	}
	if(!hscdtd006_i2c_client)
	{
		*buf = 0;
		return -2;
	}

	sprintf(buf, "HSCDTD006 Chip");
	return 0;
}
/*----------------------------------------------------------------------------*/
static int HSCDTD006_ReadSensorData(char *buf, int bufsize)
{
	struct hscdtd006_i2c_data *data = i2c_get_clientdata(hscdtd006_i2c_client);
	char cmd;
	int mode = 0;    
	unsigned char databuf[10]={'0'};
	int output[3];
	int mag[HSCDTD006_AXES_NUM];

	if((!buf)||(bufsize<=80))
	{
		return -1;
	}	
	if(NULL == hscdtd006_i2c_client)
	{
		*buf = 0;
		return -2;
	}

	read_lock(&hscdtd006_data.lock);    
	mode = hscdtd006_data.mode;
	read_unlock(&hscdtd006_data.lock);        

	databuf[0] = HSCDTD006_REG_CTRL3;
	databuf[1] = HSCDTD006_CTRL3_FORCE_BIT;
	i2c_master_send(hscdtd006_i2c_client, databuf, 2);    
	// We can read all measured data in once
	cmd = HSCDTD006_REG_DATAXL;
	i2c_master_send(hscdtd006_i2c_client, &cmd, 1);    
	i2c_master_recv(hscdtd006_i2c_client, &(databuf[0]), 6);
	
	//this calculation is different from 75. same code but different resule
	//output[0] = ((int) databuf[1]) << 8 | ((int) databuf[0]);
	//output[1] = ((int) databuf[3]) << 8 | ((int) databuf[2]);
	//output[2] = ((int) databuf[5]) << 8 | ((int) databuf[4]);

	//get Raw data for Daemon
	sns_hw_data.mag[0] = 1;
	output[0] = sns_hw_data.mag[1]= (int)((short)(databuf[1]<<8 |databuf[0]));
	output[1] = sns_hw_data.mag[2]= (int)((short)(databuf[3]<<8 |databuf[2]));
	output[2] = sns_hw_data.mag[3]= (int)((short)(databuf[5]<<8 |databuf[4]));
	//memcpy(&sns_hw_data.mag[1], output, sizeof(output));

	
	mag[data->cvt.map[HSCDTD006_AXIS_X]] = data->cvt.sign[HSCDTD006_AXIS_X]*output[HSCDTD006_AXIS_X];
	mag[data->cvt.map[HSCDTD006_AXIS_Y]] = data->cvt.sign[HSCDTD006_AXIS_Y]*output[HSCDTD006_AXIS_Y];
	mag[data->cvt.map[HSCDTD006_AXIS_Z]] = data->cvt.sign[HSCDTD006_AXIS_Z]*output[HSCDTD006_AXIS_Z];

	sprintf(buf, "%04x %04x %04x", mag[HSCDTD006_AXIS_X], mag[HSCDTD006_AXIS_Y], mag[HSCDTD006_AXIS_Z]);

	return 0;
}
/*----------------------------------------------------------------------------*/
static int HSCDTD006_ReadPostureData(char *buf, int bufsize)
{
	if((!buf)||(bufsize<=80))
	{
		return -1;
	}
	
	read_lock(&hscdtd006mid_data.datalock);
	sprintf(buf, "%d %d %d %d", hscdtd006mid_data.yaw, hscdtd006mid_data.pitch,
		hscdtd006mid_data.roll, hscdtd006mid_data.ori_status);
	read_unlock(&hscdtd006mid_data.datalock);
	return 0;
}
/*----------------------------------------------------------------------------*/
static int HSCDTD006_ReadCaliData(char *buf, int bufsize)
{
	if((!buf)||(bufsize<=80))
	{
		return -1;
	}
	
	read_lock(&hscdtd006mid_data.datalock);
	sprintf(buf, "%d %d %d %d %d %d %d", hscdtd006mid_data.nmx, hscdtd006mid_data.nmy, 
		hscdtd006mid_data.nmz,hscdtd006mid_data.nax,hscdtd006mid_data.nay,hscdtd006mid_data.naz,hscdtd006mid_data.mag_status);
	read_unlock(&hscdtd006mid_data.datalock);
	return 0;
}
/*----------------------------------------------------------------------------*/
static int HSCDTD006_ReadMiddleControl(char *buf, int bufsize)
{
	if ((!buf)||(bufsize<=80))
	{
		return -1;
	}
	
	read_lock(&hscdtd006mid_data.ctrllock);
	sprintf(buf, "%d %d %d %d %d %d %d %d %d %d",hscdtd006mid_data.controldata[0],	hscdtd006mid_data.controldata[1], 
		hscdtd006mid_data.controldata[2],hscdtd006mid_data.controldata[3],hscdtd006mid_data.controldata[4],
		hscdtd006mid_data.controldata[5], hscdtd006mid_data.controldata[6], hscdtd006mid_data.controldata[7],
		hscdtd006mid_data.controldata[8], hscdtd006mid_data.controldata[9]);
	read_unlock(&hscdtd006mid_data.ctrllock);
	return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_daemon_name(struct device_driver *ddri, char *buf)
{
	char strbuf[HSCDTD006_BUFSIZE];
	sprintf(strbuf, "AcdApiDaemon");
	return sprintf(buf, "%s", strbuf);		
}

static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[HSCDTD006_BUFSIZE];
	HSCDTD006_ReadChipInfo(strbuf, HSCDTD006_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);        
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[HSCDTD006_BUFSIZE];
	HSCDTD006_ReadSensorData(strbuf, HSCDTD006_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_posturedata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[HSCDTD006_BUFSIZE];
	HSCDTD006_ReadPostureData(strbuf, HSCDTD006_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_calidata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[HSCDTD006_BUFSIZE];
	HSCDTD006_ReadCaliData(strbuf, HSCDTD006_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_midcontrol_value(struct device_driver *ddri, char *buf)
{
	char strbuf[HSCDTD006_BUFSIZE];
	HSCDTD006_ReadMiddleControl(strbuf, HSCDTD006_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_midcontrol_value(struct device_driver *ddri, const char *buf, size_t count)
{   
	int p[10];
	if(10 == sscanf(buf, "%d %d %d %d %d %d %d %d %d %d",&p[0], &p[1], &p[2], &p[3], &p[4], 
		&p[5], &p[6], &p[7], &p[8], &p[9]))
	{
		write_lock(&hscdtd006mid_data.ctrllock);
		memcpy(&hscdtd006mid_data.controldata[0], &p, sizeof(int)*10);    
		write_unlock(&hscdtd006mid_data.ctrllock);        
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
	read_lock(&hscdtd006mid_data.ctrllock);
	len = sprintf(buf, "0x%08X\n", hscdtd006mid_data.debug);
	read_unlock(&hscdtd006mid_data.ctrllock);

	return len;            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_middebug_value(struct device_driver *ddri, const char *buf, size_t count)
{   
	int debug;
	if(1 == sscanf(buf, "0x%x", &debug))
	{
		write_lock(&hscdtd006mid_data.ctrllock);
		hscdtd006mid_data.debug = debug;
		write_unlock(&hscdtd006mid_data.ctrllock);        
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
	read_lock(&hscdtd006_data.lock);
	mode = hscdtd006_data.mode;
	read_unlock(&hscdtd006_data.lock);        
	return sprintf(buf, "%d\n", mode);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_mode_value(struct device_driver *ddri, const char *buf, size_t count)
{
	int mode = 0;
	sscanf(buf, "%d", &mode);    
	HSCDTD006_SetMode(mode);
	return count;            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = hscdtd006_i2c_client;  
	struct hscdtd006_i2c_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		data->hw->direction,atomic_read(&data->layout),	data->cvt.sign[0], data->cvt.sign[1],
		data->cvt.sign[2],data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct i2c_client *client = hscdtd006_i2c_client;  
	struct hscdtd006_i2c_data *data = i2c_get_clientdata(client);
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
	struct i2c_client *client = hscdtd006_i2c_client;  
	struct hscdtd006_i2c_data *data = i2c_get_clientdata(client);
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
/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct hscdtd006_i2c_data *obj = i2c_get_clientdata(hscdtd006_i2c_client);
	if(NULL == obj)
	{
		MSE_ERR("hscdtd006_i2c_data is null!!\n");
		return 0;
	}	
	
	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));     
	return res;    
}

/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct hscdtd006_i2c_data *obj = i2c_get_clientdata(hscdtd006_i2c_client);
	int trace;
	if(NULL == obj)
	{
		MSE_ERR("hscdtd006_i2c_data is null!!\n");
		return 0;
	}
	
	if(1 == sscanf(buf, "0x%x", &trace))
	{
		atomic_set(&obj->trace, trace);
		hscdtd006mid_data.controldata[8] = trace;
	}
	else 
	{
		MSE_ERR("invalid content: '%s', length = %d\n", buf, count);
	}
	
	return count;    
}


/*----------------------------------------------------------------------------*/
static int hwmsen_read_byte_sr(struct i2c_client *client, u8 addr, u8 *data)
{
   u8 buf;
    int ret = 0;
	
   // client->addr = (client->addr & I2C_MASK_FLAG) | I2C_WR_FLAG |I2C_RS_FLAG;
    client->addr = (client->addr & I2C_MASK_FLAG);
    buf = addr;
	ret = i2c_master_send(client, (const char*)&buf,  1);
    //ret = i2c_master_send(client, (const char*)&buf, 1);
    if (ret !=1) {
        MSE_ERR("send command error!!\n");
        return -EFAULT;
    }
	
	ret = i2c_master_recv(client, (char*)&buf, 1);
	if (ret != 1) {
		MSE_ERR("read command error!!\n");
		return -1;
	}

    *data = buf;
	//client->addr = client->addr& I2C_MASK_FLAG;
    return 0;
}



static void hscdtd006_dumpReg(struct i2c_client *client)
{
  int i=0;
  u8 addr = 0;
  u8 regdata=0;
  for(addr=0x0B; i<0x2B ; i++)
  {
    //dump all
    hwmsen_read_byte_sr(client,addr,&regdata);
	MSE_LOG("Reg addr=%x regdata=%x\n",addr,regdata);
	//snprintf(buf,1,"%c",regdata);
	addr++;
	
  }
}
/*----------------------------------------------------------------------------*/
static ssize_t hscdtd006_show_reg(struct device_driver *ddri, char *buf)
{
	
	/*read*/
	hscdtd006_dumpReg(hscdtd006_i2c_client);
	
	return 0;
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
static DRIVER_ATTR(status,      S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(reg,       S_IRUGO | S_IWUSR, hscdtd006_show_reg, NULL );
static DRIVER_ATTR(trace,       S_IRUGO | S_IWUSR, show_trace_value, store_trace_value );
/*----------------------------------------------------------------------------*/
static struct driver_attribute *hscdtd006_attr_list[] = {
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
	&driver_attr_reg,
	&driver_attr_trace,
};
/*----------------------------------------------------------------------------*/
static int hscdtd006_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(hscdtd006_attr_list)/sizeof(hscdtd006_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, hscdtd006_attr_list[idx])))
		{            
			MSE_ERR("driver_create_file (%s) = %d\n", hscdtd006_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int hscdtd006_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(hscdtd006_attr_list)/sizeof(hscdtd006_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, hscdtd006_attr_list[idx]);
	}
	

	return err;
}


/*----------------------------------------------------------------------------*/
static int hscdtd006_open(struct inode *inode, struct file *file)
{    
	struct hscdtd006_i2c_data *obj = i2c_get_clientdata(hscdtd006_i2c_client);    
	int ret = -1;
	atomic_inc(&dev_open_count);
	
	if(atomic_read(&obj->trace) & ALPS_TRC_DEBUG)
	{
		MSE_LOG("Open device node:hscdtd006\n");
	}
	ret = nonseekable_open(inode, file);
	
	return ret;
}
/*----------------------------------------------------------------------------*/
static int hscdtd006_release(struct inode *inode, struct file *file)
{
	struct hscdtd006_i2c_data *obj = i2c_get_clientdata(hscdtd006_i2c_client);
	atomic_dec(&dev_open_count);
	if(atomic_read(&obj->trace) & ALPS_TRC_DEBUG)
	{
		MSE_LOG("Release device node:hscdtd006\n");
	}	
	return 0;
}
/*----------------------------------------------------------------------------*/
//static int hscdtd006_ioctl(struct inode *inode, struct file *file, unsigned int cmd,unsigned long arg)//modified here
static long hscdtd006_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
    void __user *argp = (void __user *)arg;
	//int valuebuf[4];
	int calidata[7];
	int controlbuf[10];
	char strbuf[HSCDTD006_BUFSIZE];
	void __user *data;
	long retval=0;
	int mode=0;
	hwm_sensor_data* osensor_data;
	uint32_t enable;
	char buff[512];	
	int status; 				/* for OPEN/CLOSE_STATUS */
	short sensor_status;		/* for Orientation and Msensor status */
	struct hscdtd006_i2c_data *obj = i2c_get_clientdata(hscdtd006_i2c_client);    

	struct TAIFD_HW_DATA tmpval_hw;
	 struct TAIFD_SW_DATA tmpval_sw;
//	MSE_FUN(f);

	switch (cmd)
	{
		case MSENSOR_IOCTL_INIT:
			read_lock(&hscdtd006_data.lock);
			mode = hscdtd006_data.mode;
			read_unlock(&hscdtd006_data.lock);
			HSCDTD006_Chipset_Init(mode);         
			break;

		
		//zhijie modify here for daemon used
		case MSENSOR_IOCTL_READ_SENSORDATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;    
			}
			HSCDTD006_ReadSensorData(strbuf, HSCDTD006_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				retval = -EFAULT;
				goto err_out;
			}                
			break;
			
		case ALPSIO_AD_GET_DATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;    
			}
			memset(&tmpval_hw, 0, sizeof tmpval_hw);
			
			HSCDTD006_ReadSensorData(strbuf, HSCDTD006_BUFSIZE);
			memcpy(&tmpval_hw, &sns_hw_data, sizeof tmpval_hw);
			
			sns_hw_data.activate = flgActivate;

			if(copy_to_user(data, &tmpval_hw, sizeof tmpval_hw))
			{
				retval = -EFAULT;
				goto err_out;
			}                
			break;
		//zhijie get data from daemon
		case ALPSIO_AD_SET_DATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			   
			if(copy_from_user(&tmpval_sw, data, sizeof(tmpval_sw)))
			{
				retval = -EFAULT;
				goto err_out;
			}

			write_lock(&hscdtd006mid_data.datalock);
			
            memcpy(&sns_sw_data, &tmpval_sw, sizeof(sns_sw_data));
			hscdtd006mid_data.yaw   = sns_sw_data.ori[1];
			hscdtd006mid_data.pitch = sns_sw_data.ori[2];
			hscdtd006mid_data.roll  = sns_sw_data.ori[3];
			hscdtd006mid_data.ori_status = sns_sw_data.ori[4];

			hscdtd006mid_data.nmx = sns_sw_data.mag[1];
			hscdtd006mid_data.nmy = sns_sw_data.mag[2];
			hscdtd006mid_data.nmz = sns_sw_data.mag[3];
			hscdtd006mid_data.mag_status = sns_sw_data.mag[4];
			
			write_unlock(&hscdtd006mid_data.datalock);    
			break;

		case ALPSIO_AD_GET_ACTIVATE:
			read_lock(&hscdtd006_data.lock);
			mode = flgActivate;
			read_unlock(&hscdtd006_data.lock);
			
			if(copy_to_user(argp, &mode, sizeof(mode)))
			{
				MSE_ERR("copy_to_user failed.");
				return -EFAULT;
			}
			break;
			
		case ALPSIO_AD_GET_DELAY:
			read_lock(&hscdtd006_data.lock);
			mode = sns_hw_data.delay  ;
			read_unlock(&hscdtd006_data.lock);
			
			if(copy_to_user(argp, &mode, sizeof(mode)))
			{
				MSE_ERR("copy_to_user failed.");
				return -EFAULT;
			}
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
			status = hscdtd006_GetOpenStatus();			
			if(copy_to_user(argp, &status, sizeof(status)))
			{
				MSE_LOG("copy_to_user failed.");
				return -EFAULT;
			}
			break;        

		case MSENSOR_IOCTL_SET_CALIDATA:
			data = (void __user *) arg;
			if (data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			if(copy_from_user(&calidata, data, sizeof(calidata)))
			{
				retval = -EFAULT;
				goto err_out;
			}
			//zhijie nothing to do here
		/*	write_lock(&hscdtd006mid_data.datalock);            
			hscdtd006mid_data.nmx = calidata[0];
			hscdtd006mid_data.nmy = calidata[1];
			hscdtd006mid_data.nmz = calidata[2];
			hscdtd006mid_data.nax = calidata[3];
			hscdtd006mid_data.nay = calidata[4];
			hscdtd006mid_data.naz = calidata[5];
			hscdtd006mid_data.mag_status = calidata[6];
			write_unlock(&hscdtd006mid_data.datalock);    */
			break;                                

		case MSENSOR_IOCTL_READ_CHIPINFO:
			data = (void __user *) arg;
			if(data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			
			HSCDTD006_ReadChipInfo(strbuf, HSCDTD006_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				retval = -EFAULT;
				goto err_out;
			}                
			break;

		case MSENSOR_IOCTL_SENSOR_ENABLE:
			
			data = (void __user *) arg;
			if (data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			if(copy_from_user(&enable, data, sizeof(enable)))
			{
				MSE_ERR("copy_from_user failed.");
				return -EFAULT;
			}
			else
			{
				read_lock(&hscdtd006mid_data.ctrllock);
				if(enable == 1)
				{
					hscdtd006mid_data.controldata[7] |= SENSOR_ORIENTATION;
					atomic_set(&o_flag, 1);
					atomic_set(&m_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					hscdtd006mid_data.controldata[7] &= ~SENSOR_ORIENTATION;
					atomic_set(&o_flag, 0);
					atomic_set(&m_flag, 0);
					if(atomic_read(&m_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}		
				}
				
				wake_up(&open_wq);
				read_unlock(&hscdtd006mid_data.ctrllock);
				
			}
			
			break;


		case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:  //this ioctl is used in FTM
			
			data = (void __user *) arg;
			if (data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			
			osensor_data = (hwm_sensor_data *)buff;

			read_lock(&hscdtd006mid_data.datalock);
			osensor_data->values[0] = hscdtd006mid_data.yaw;
			osensor_data->values[1] = hscdtd006mid_data.pitch;
			osensor_data->values[2] = hscdtd006mid_data.roll;
			osensor_data->status = hscdtd006mid_data.ori_status;
			read_unlock(&hscdtd006mid_data.datalock); 
						
			osensor_data->value_divide = ORIENTATION_ACCURACY_RATE;	
			/*
			switch (hscdtd006mid_data.ori_status)
		    {
		            case 1: 
		                osensor_data->status = SENSOR_STATUS_ACCURACY_LOW;
		                break;
		            case 2:
		                osensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
		                break;
		            case 3:
		                osensor_data->status = SENSOR_STATUS_ACCURACY_HIGH;
		                break;
		            default:        
		                osensor_data->status = SENSOR_STATUS_UNRELIABLE;
		                break;    
		    }
     		*/
			
            sprintf(buff, "%x %x %x %x %x", osensor_data->values[0], osensor_data->values[1],
				osensor_data->values[2],osensor_data->status,osensor_data->value_divide);
			if(copy_to_user(data, buff, strlen(buff)+1))
			{
				return -EFAULT;
			} 
			
			break;                

		case MSENSOR_IOCTL_READ_POSTUREDATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			
			HSCDTD006_ReadPostureData(strbuf, HSCDTD006_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				retval = -EFAULT;
				goto err_out;
			}                
			break;            

		case MSENSOR_IOCTL_READ_CALIDATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				break;    
			}
			//zhijie nothing to do here
			//HSCDTD006_ReadCaliData(strbuf, HSCDTD006_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				retval = -EFAULT;
				goto err_out;
			}                
			break;

		case MSENSOR_IOCTL_READ_CONTROL:
			read_lock(&hscdtd006mid_data.ctrllock);
			memcpy(controlbuf, &hscdtd006mid_data.controldata[0], sizeof(controlbuf));
			read_unlock(&hscdtd006mid_data.ctrllock);            
			data = (void __user *) arg;
			if(data == NULL)
			{
				break;
			}
			if(copy_to_user(data, controlbuf, sizeof(controlbuf)))
			{
				retval = -EFAULT;
				goto err_out;
			}                                
			break;

		case MSENSOR_IOCTL_SET_CONTROL:
			data = (void __user *) arg;
			if(data == NULL)
			{
				break;
			}
			if(copy_from_user(controlbuf, data, sizeof(controlbuf)))
			{
				retval = -EFAULT;
				goto err_out;
			}    
			write_lock(&hscdtd006mid_data.ctrllock);
			memcpy(&hscdtd006mid_data.controldata[0], controlbuf, sizeof(controlbuf));
			write_unlock(&hscdtd006mid_data.ctrllock);        
			break;

		case ECOMPASS_IOC_GET_LAYOUT:		//used by daemon ?
			status = atomic_read(&obj->layout);
			if(copy_to_user(argp, &status, sizeof(status)))
			{
				MSE_ERR("copy_to_user failed.");
				return -EFAULT;
			}
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
			
			HSCDTD006_SetMode(mode);                
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
static struct file_operations hscdtd006_fops = {
//	.owner = THIS_MODULE,
	.open = hscdtd006_open,
	.release = hscdtd006_release,
	.unlocked_ioctl = hscdtd006_unlocked_ioctl,//modified
};
/*----------------------------------------------------------------------------*/
static struct miscdevice hscdtd006_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "msensor",
    .fops = &hscdtd006_fops,
};
/*----------------------------------------------------------------------------*/
int hscdtd006_operate(void* self, uint32_t command, void* buff_in, int size_in,
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
				if(value <= 20)
				{
					sample_delay = 20;
				}
				
				
				hscdtd006mid_data.controldata[0] = sample_delay;  // Loop Delay
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
				
				read_lock(&hscdtd006mid_data.ctrllock);
				if(value == 1)
				{
					hscdtd006mid_data.controldata[7] |= SENSOR_MAGNETIC;
					atomic_set(&m_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					hscdtd006mid_data.controldata[7] &= ~SENSOR_MAGNETIC;
					atomic_set(&m_flag, 0);
					if(atomic_read(&o_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}	
				}
				
				if(hscdtd006mid_data.controldata[7] & SENSOR_MAGNETIC)
					flgActivate |= ACTIVE_SS_MAG;
				
				wake_up(&open_wq);
				read_unlock(&hscdtd006mid_data.ctrllock);
				
				// TODO: turn device into standby or normal mode
			}
			break;

		case SENSOR_GET_DATA:
			//MSE_LOG("++++++++++++++++++++++++MSENSOR_GET_DATA");
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				MSE_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				msensor_data = (hwm_sensor_data *)buff_out;
				read_lock(&hscdtd006mid_data.datalock);
				msensor_data->values[0] = hscdtd006mid_data.nmx;
				msensor_data->values[1] = hscdtd006mid_data.nmy;
				msensor_data->values[2] = hscdtd006mid_data.nmz;
				msensor_data->status = hscdtd006mid_data.mag_status;
				read_unlock(&hscdtd006mid_data.datalock); 
				
				msensor_data->values[0] = msensor_data->values[0] * CONVERT_M;
				msensor_data->values[1] = msensor_data->values[1] * CONVERT_M;
				msensor_data->values[2] = msensor_data->values[2] * CONVERT_M;
				msensor_data->value_divide = 1;//1000
				/*
				switch (status)
		        {
		            case 1: case 2:
		                msensor_data->status = SENSOR_STATUS_ACCURACY_HIGH;
		                break;
		            case 3:
		                msensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
		                break;
		            case 4:
		                msensor_data->status = SENSOR_STATUS_ACCURACY_LOW;
		                break;
		            default:        
		                msensor_data->status = SENSOR_STATUS_UNRELIABLE;
		                break;    
		        }
				*/
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
int hscdtd006_orientation_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value, sample_delay, status=0;
	hwm_sensor_data* osensor_data=NULL;
	
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
				if(value <= 20)
				{
					sample_delay = 20;
				}
				
				
				hscdtd006mid_data.controldata[0] = sample_delay;  // Loop Delay
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
				
				read_lock(&hscdtd006mid_data.ctrllock);
				if(value == 1)
				{
					hscdtd006mid_data.controldata[7] |= SENSOR_ORIENTATION;
					atomic_set(&o_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					hscdtd006mid_data.controldata[7] &= ~SENSOR_ORIENTATION;
					atomic_set(&o_flag, 0);
					if(atomic_read(&m_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}
				}
				
				if(hscdtd006mid_data.controldata[7] & SENSOR_ORIENTATION)
					flgActivate |= ACTIVE_SS_ORI;
				
				wake_up(&open_wq);
				read_unlock(&hscdtd006mid_data.ctrllock);
				// Do nothing
			}
			break;

		case SENSOR_GET_DATA:
			//MSE_LOG("+++++++++++MSENSOR_GET_ORIENTATION_DATA");
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				MSE_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				osensor_data = (hwm_sensor_data *)buff_out;
				read_lock(&hscdtd006mid_data.datalock);
				osensor_data->values[0] = hscdtd006mid_data.yaw;
				osensor_data->values[1] = hscdtd006mid_data.pitch;
				osensor_data->values[2] = hscdtd006mid_data.roll;
				osensor_data->status = hscdtd006mid_data.ori_status;
				read_unlock(&hscdtd006mid_data.datalock); 
				
				
				osensor_data->value_divide = ORIENTATION_ACCURACY_RATE;				
			}
			/*
			switch (status)
	        {
	            case 1: case 2:
	                osensor_data->status = SENSOR_STATUS_ACCURACY_HIGH;
	                break;
	            case 3:
	                osensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
	                break;
	            case 4:
	                osensor_data->status = SENSOR_STATUS_ACCURACY_LOW;
	                break;
	            default:        
	                osensor_data->status = SENSOR_STATUS_UNRELIABLE;
	                break;    
	        }
			*/
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
static int hscdtd006_suspend(struct i2c_client *client, pm_message_t msg) 
{
	int err;
	struct hscdtd006_i2c_data *obj = i2c_get_clientdata(client);
	//MSE_FUN();    

	if(msg.event == PM_EVENT_SUSPEND)
	{   
		if((err = hwmsen_write_byte(client, HSCDTD006_REG_CTRL1, 0x00))!=0)
		{
			MSE_ERR("write power control fail!!\n");
			return err;
		}
		
		hscdtd006_power(obj->hw, 0);
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int hscdtd006_resume(struct i2c_client *client)
{
	int err;
	struct hscdtd006_i2c_data *obj = i2c_get_clientdata(client);
	MSE_FUN();

	hscdtd006_power(obj->hw, 1);
	if((err = HSCDTD006_Chipset_Init(HSCDTD006_FORCE_MODE))!=0)
	{
		MSE_ERR("initialize client fail!!\n");
		return err;        
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
//#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void hscdtd006_early_suspend(struct early_suspend *h) 
{
	struct hscdtd006_i2c_data *obj = container_of(h, struct hscdtd006_i2c_data, early_drv);   
	int err;
	MSE_FUN();    

	if(NULL == obj)
	{
		MSE_ERR("null pointer!!\n");
		return;
	}

	if((err = hwmsen_write_byte(obj->client, HSCDTD006_REG_CTRL1, 0x00)))
	{
		MSE_ERR("write power control fail!!\n");
		return;
	}        
}
/*----------------------------------------------------------------------------*/
static void hscdtd006_late_resume(struct early_suspend *h)
{
	struct hscdtd006_i2c_data *obj = container_of(h, struct hscdtd006_i2c_data, early_drv);         
	int err;
	MSE_FUN();

	if(NULL == obj)
	{
		MSE_ERR("null pointer!!\n");
		return;
	}

	hscdtd006_power(obj->hw, 1);
	if((err = HSCDTD006_Chipset_Init(HSCDTD006_FORCE_MODE)))
	{
		MSE_ERR("initialize client fail!!\n");
		return;        
	}    
}
/*----------------------------------------------------------------------------*/
//#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/
/*
static int hscdtd006_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{    
	strcpy(info->type, HSCDTD006_DEV_NAME);
	return 0;
}
*/

/*----------------------------------------------------------------------------*/
static int hscdtd006_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct hscdtd006_i2c_data *data;
	int err = 0;
	struct hwmsen_object sobj_m, sobj_o;
	MSE_FUN();

	if (!(data = kmalloc(sizeof(struct hscdtd006_i2c_data), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	memset(data, 0, sizeof(struct hscdtd006_i2c_data));

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
	
	hscdtd006_i2c_client = new_client;	

	if((err = HSCDTD006_Chipset_Init(HSCDTD006_FORCE_MODE)))
	{
		goto exit_init_failed;
	}

	/* Register sysfs attribute */
	if((err = hscdtd006_create_attr(&alps_sensor_driver.driver)))
	{
		MSE_ERR("create attribute err = %d\n", err);
		goto exit_sysfs_create_group_failed;
	}

	
	if((err = misc_register(&hscdtd006_device)))
	{
		MSE_ERR("hscdtd006_device register failed\n");
		goto exit_misc_device_register_failed;	}    

	sobj_m.self = data;
    sobj_m.polling = 1;
    sobj_m.sensor_operate = hscdtd006_operate;
	if((err = hwmsen_attach(ID_MAGNETIC, &sobj_m)))
	{
		MSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}
	
	sobj_o.self = data;
    sobj_o.polling = 1;
    sobj_o.sensor_operate = hscdtd006_orientation_operate;
	if((err = hwmsen_attach(ID_ORIENTATION, &sobj_o)))
	{
		MSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}
	
#if CONFIG_HAS_EARLYSUSPEND
	data->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	data->early_drv.suspend  = hscdtd006_early_suspend,
	data->early_drv.resume   = hscdtd006_late_resume,    
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
static int hscdtd006_i2c_remove(struct i2c_client *client)
{
	int err;	
	
	if((err = hscdtd006_delete_attr(&alps_sensor_driver.driver)))
	{
		MSE_ERR("hscdtd006_delete_attr fail: %d\n", err);
	}
	
	hscdtd006_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));	
	misc_deregister(&hscdtd006_device);    
	return 0;
}
/*----------------------------------------------------------------------------*/
static int alps_probe(struct platform_device *pdev) 
{
	struct mag_hw *hw = get_cust_mag_hw();

	hscdtd006_power(hw, 1);    
	rwlock_init(&hscdtd006mid_data.ctrllock);
	rwlock_init(&hscdtd006mid_data.datalock);
	rwlock_init(&hscdtd006_data.lock);
	memset(&hscdtd006mid_data.controldata[0], 0, sizeof(int)*10);    
	hscdtd006mid_data.controldata[0] =    20;  // Loop Delay
	hscdtd006mid_data.controldata[1] =     0;  // Run   
	hscdtd006mid_data.controldata[2] =     0;  // Disable Start-AccCali
	hscdtd006mid_data.controldata[3] =     1;  // Enable Start-Cali
	hscdtd006mid_data.controldata[4] =   350;  // MW-Timout
	hscdtd006mid_data.controldata[5] =    10;  // MW-IIRStrength_M
	hscdtd006mid_data.controldata[6] =    10;  // MW-IIRStrength_G   
	hscdtd006mid_data.controldata[7] =     0;  // Active Sensors
	hscdtd006mid_data.controldata[8] =     0;  // Daemon log switch on/off
	hscdtd006mid_data.controldata[9] =     0;  // Wait for define   
	atomic_set(&dev_open_count, 0);
	//hscdtd006_force[0] = hw->i2c_num;

	memset(&sns_hw_data, 0, sizeof sns_hw_data);
	memset(&sns_sw_data, 0, sizeof sns_sw_data);
	sns_hw_data.delay = 50;  //set delay time for Daemon

	if(i2c_add_driver(&hscdtd006_i2c_driver))
	{
		MSE_ERR("add driver error\n");
		return -1;
	} 
	return 0;
}
/*----------------------------------------------------------------------------*/
static int alps_remove(struct platform_device *pdev)
{
	struct mag_hw *hw = get_cust_mag_hw();

	MSE_FUN();    
	hscdtd006_power(hw, 0);    
	atomic_set(&dev_open_count, 0);  
	i2c_del_driver(&hscdtd006_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
#if 0
static struct platform_driver alps_sensor_driver = {
	.probe      = alps_probe,
	.remove     = alps_remove,    
	.driver     = {
		.name  = "msensor",
//		.owner = THIS_MODULE,
	}
};
#endif

#ifdef CONFIG_OF
static const struct of_device_id hs_of_match[] = {
	{ .compatible = "mediatek,msensor", },
	{},
};
#endif

static struct platform_driver alps_sensor_driver =
{
	.probe      = alps_probe,
	.remove     = alps_remove,    
	.driver     = 
	{
		.name = "msensor",
        #ifdef CONFIG_OF
		.of_match_table = hs_of_match,
		#endif
	}
};

/*----------------------------------------------------------------------------*/
static int __init hscdtd006_init(void)
{
	MSE_FUN();
	struct mag_hw *hw = get_cust_mag_hw();
	MSE_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	i2c_register_board_info(hw->i2c_num, &i2c_hscdtd006, 1);
	if(platform_driver_register(&alps_sensor_driver))
	{
		MSE_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit hscdtd006_exit(void)
{
	MSE_FUN();
	platform_driver_unregister(&alps_sensor_driver);
}
/*----------------------------------------------------------------------------*/
module_init(hscdtd006_init);
module_exit(hscdtd006_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("zhijie.yuan@mediatek.com");
MODULE_DESCRIPTION("HSCDTD006 MI-Sensor driver without DRDY");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
