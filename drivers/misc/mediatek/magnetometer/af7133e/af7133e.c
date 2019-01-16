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
 * but WITHOUT ANY WRRANTY; without even the implied warranty of
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
#include "af7133e.h"
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

//#define AF7133_NEW_ARCH

#ifdef AF7133_NEW_ARCH
#include "mag.h"
/* Add for auto detect feature */
//extern struct mag_hw* af7133_get_cust_mag_hw(void); 
static int  af7133e_af8133i_local_init(void);
static int vtc_remove(void);
static int af7133e_af8133i_m_open_report_data(int en);
static int af7133e_af8133i_m_set_delay(u64 delay);
static int af7133e_af8133i_m_enable(int en);
static int af7133e_af8133i_o_open_report_data(int en);
static int af7133e_af8133i_o_set_delay(u64 delay);
static int af7133e_af8133i_o_enable(int en);
static int af7133e_af8133i_get_data_m(int *x,int *y, int *z,int *status);
static int af7133e_af8133i_get_data_o(int *x,int *y, int *z,int *status);

static int af7133e_af8133i_init_flag = -1;  //0:ok,,-1:fail

static struct mag_init_info af7133e_af8133i_init_info = {
        .name   = "af7133e_af8133i",	
        .init   = af7133e_af8133i_local_init,
        .uninit = vtc_remove,	
};

#endif


/*----------------------------------------------------------------------------*/
#define DEBUG 1
#define AF7133E_AF8133I_DEV_NAME         "af7133e_af8133i"
#define DRIVER_VERSION          "1.0.5"
#define DRIVER_RELEASE          "20141014"
/*----------------------------------------------------------------------------*/
#define AF7133E_AF8133I_DEFAULT_DELAY     40
#define AF7133E_AF8133I_DELAY_MIN     		2
#define AF7133E_AF8133I_DELAY_MAX     		100
/*----------------------------------------------------------------------------*/
#define MSE_TAG                  "MSENSOR"
#define MSE_FUN(f)               printk(MSE_TAG" %s\r\n", __FUNCTION__)
#define MSE_ERR(fmt, args...)    printk(KERN_ERR MSE_TAG" %s %d : \r\n"fmt, __FUNCTION__, __LINE__, ##args)
#define MSE_LOG(fmt, args...)    printk(MSE_TAG fmt, ##args)
#define MSE_VER(fmt, args...)   ((void)0)
static DECLARE_WAIT_QUEUE_HEAD(data_ready_wq);
static DECLARE_WAIT_QUEUE_HEAD(open_wq);

static atomic_t open_flag = ATOMIC_INIT(0);
static atomic_t m_flag = ATOMIC_INIT(0);
static atomic_t o_flag = ATOMIC_INIT(0);
/*----------------------------------------------------------------------------*/
static struct i2c_client *af7133e_af8133i_i2c_client = NULL;
/*----------------------------------------------------------------------------*/
#define FIND_SW_OFFSET_LOOP    5
#define FIND_SW_OFFSET_INDEX   2

static int mag_pos[3][FIND_SW_OFFSET_LOOP];
static int mag_neg[3][FIND_SW_OFFSET_LOOP];
static int mag_offset[3];
static int mag_cnt=0;

static unsigned char ADC_log;
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id af7133e_af8133i_i2c_id[] = {{AF7133E_AF8133I_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_af7133e_af8133i={ I2C_BOARD_INFO("af7133e_af8133i", AF7133E_AF8133I_I2C_ADDRESS)};  //7-bit address
/*the adapter id will be available in customization*/
//static unsigned short af7133_force[] = {0x00, AF7133_I2C_ADDRESS, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const af7133_forces[] = { af7133_force, NULL };
//static struct i2c_client_address_data af7133_addr_data = { .forces = af7133_forces,};

/*----------------------------------------------------------------------------*/
static int af7133e_af8133i_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int af7133e_af8133i_i2c_remove(struct i2c_client *client);

static int af7133e_af8133i_suspend(struct i2c_client *client, pm_message_t msg) ;
static int af7133e_af8133i_resume(struct i2c_client *client);

static struct platform_driver vtc_sensor_driver;

/*----------------------------------------------------------------------------*/
typedef enum {
    VTC_TRC_DEBUG  = 0x01,
} AMI_TRC;
/*----------------------------------------------------------------------------*/
struct _af7133e_af8133i_data {
    rwlock_t lock;
    int mode;
    int rate;
    volatile int updated;
} af7133e_af8133i_data;
/*----------------------------------------------------------------------------*/
struct _af7133e_af8133i_mid_data {
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
} af7133e_af8133i_mid_data;
/*----------------------------------------------------------------------------*/
struct af7133e_af8133i_i2c_data {
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
static struct i2c_driver af7133e_af8133i_i2c_driver = {
    .driver = {
//#ifdef AF7133_NEW_ARCH
        .owner = THIS_MODULE, 
//#endif
        .name  = AF7133E_AF8133I_DEV_NAME,
    },
	.probe      = af7133e_af8133i_i2c_probe,
	.remove     = af7133e_af8133i_i2c_remove,
//	.detect     = af7133_i2c_detect,
//#if !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend    = af7133e_af8133i_suspend,
	.resume     = af7133e_af8133i_resume,
//#endif 
	.id_table = af7133e_af8133i_i2c_id,
	//.address_list = af7133_forces,//address_data->address_list
};

static DEFINE_MUTEX(af7133e_af8133i_mutex);
#define I2C_FLAG_WRITE	0
#define I2C_FLAG_READ	1


/*----------------------------------------------------------------------------*/
static atomic_t dev_open_count;
/*----------------------------------------------------------------------------*/


/*------------------------i2c function for 89-------------------------------------*/
int af7133e_af8133i_i2c_master_operate(struct i2c_client *client, const char *buf, int count, int i2c_flag)
{
	int res = 0;
	mutex_lock(&af7133e_af8133i_mutex);
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
	MSE_LOG("af7133e_af8133i_i2c_master_operate i2c_flag command not support!\n");
	break;
	}
	if(res <= 0)
	{
		goto EXIT_ERR;
	}
	mutex_unlock(&af7133e_af8133i_mutex);
	return res;
	EXIT_ERR:
	mutex_unlock(&af7133e_af8133i_mutex);
	MSE_ERR("af7133e_af8133i_i2c_transfer fail\n");
	return res;
}



static void af7133e_af8133i_power(struct mag_hw *hw, unsigned int on) 
{
#ifdef __USE_LINUX_REGULATOR_FRAMEWORK__
#else
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
			if(!hwPowerOn(hw->power_id, hw->power_vol, "AF7133E_AF8133I")) 
			{
				MSE_ERR("power on fails!!\n");
			}
		}
		else
		{
			if(!hwPowerDown(hw->power_id, "AF7133E_AF8133I")) 
			{
				MSE_ERR("power off fail!!\n");
			}
		}
	}
	power_on = on;
#endif
}
/*----------------------------------------------------------------------------*/

static int af7133e_af8133i_GetOpenStatus(void)
{
	wait_event_interruptible(open_wq, (atomic_read(&open_flag) != 0));
	return atomic_read(&open_flag);
}

/*----------------------------------------------------------------------------*/
static int af7133e_af8133i_Chipset_Init(int mode)
{

	u8 databuf[2];
	u8 vaule;
	int ret;

	//af7133e_af8133i_i2c_client->addr = af7133e_af8133i_i2c_client->addr & I2C_MASK_FLAG;
  
	databuf[0] = AF7133E_AF8133I_REG_PCODE;
	af7133e_af8133i_i2c_master_operate(af7133e_af8133i_i2c_client, databuf, 0x101, I2C_FLAG_READ);
  	
	if(databuf[0] != AF7133E_AF8133I_PCODE_VALUE)
	{
		MSE_ERR("af7133e_af8133i PCODE is incorrect: %d\n", databuf[0]);
		return -3;
	} 
	else
	{
		printk("%s chip id:%#x\n",__func__,databuf[0]);
	}
    
	databuf[0] = 0x10;
	databuf[1] = 0x55; 
  af7133e_af8133i_i2c_master_operate(af7133e_af8133i_i2c_client, databuf, 0x02, I2C_FLAG_WRITE);  
  
	databuf[0] = 0x14;
	databuf[1] = 0x34;
  af7133e_af8133i_i2c_master_operate(af7133e_af8133i_i2c_client, databuf, 0x02, I2C_FLAG_WRITE);  

  databuf[0] = 0x33;
  //databuf[1] = 0x16;
  databuf[1] = ADC_log;
  af7133e_af8133i_i2c_master_operate(af7133e_af8133i_i2c_client, databuf, 0x02, I2C_FLAG_WRITE);  

  databuf[0] = 0x0B;
  databuf[1] = 0x3C;
  af7133e_af8133i_i2c_master_operate(af7133e_af8133i_i2c_client, databuf, 0x02, I2C_FLAG_WRITE);
  
  databuf[0] = 0x13;
  databuf[1] = 0x00;
  af7133e_af8133i_i2c_master_operate(af7133e_af8133i_i2c_client, databuf, 0x02, I2C_FLAG_WRITE);

  databuf[0] = 0x0A;
  databuf[1] = 0x01;
  af7133e_af8133i_i2c_master_operate(af7133e_af8133i_i2c_client, databuf, 0x02, I2C_FLAG_WRITE); 

	

	write_lock(&af7133e_af8133i_data.lock);
	af7133e_af8133i_data.mode = mode;
	write_unlock(&af7133e_af8133i_data.lock);

	return 0;
}
/*----------------------------------------------------------------------------*/
static int af7133e_af8133i_SetMode(int newmode)
{
	int mode = 0;

	read_lock(&af7133e_af8133i_data.lock);
	mode = af7133e_af8133i_data.mode;
	read_unlock(&af7133e_af8133i_data.lock);        

	if(mode == newmode)
	{
		return 0;    
	}

	return af7133e_af8133i_Chipset_Init(newmode);
}
/*----------------------------------------------------------------------------*/
static int af7133e_af8133i_SetADC(void)
{
	unsigned char databuf[2];
	int err;
	
	if(NULL == af7133e_af8133i_i2c_client)
	{
		return -2;
	}
	
	struct af7133e_af8133i_i2c_data *data = i2c_get_clientdata(af7133e_af8133i_i2c_client);
	 
	databuf[0] = 0x1F;  
	err = af7133e_af8133i_i2c_master_operate(af7133e_af8133i_i2c_client, databuf, 0x101, I2C_FLAG_READ);  

	if(err < 0)
		return err;
	
	if(databuf[0] == 0x00)
	{
		databuf[0] = 0x33;
		databuf[1] = 0x16;
		ADC_log = 0x16;
	}
	else if(databuf[0] == 0x01)
	{
		databuf[0] = 0x33;
		databuf[1] = 0x15;
		ADC_log = 0x15;
	}
	else if(databuf[0] == 0x03)
	{
		databuf[0] = 0x33;
		databuf[1] = 0x17;
		ADC_log = 0x17;
	}
	else if(databuf[0] == 0x07)
	{
		databuf[0] = 0x33;
		databuf[1] = 0x14;
		ADC_log = 0x14;
	}

	err = af7133e_af8133i_i2c_master_operate(af7133e_af8133i_i2c_client, databuf, 0x02, I2C_FLAG_WRITE);

	if(err<0)
		return err;
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int af7133e_af8133i_Read_Regiser(unsigned char reg, char* value)
{  
	struct af7133e_af8133i_i2c_data *data = i2c_get_clientdata(af7133e_af8133i_i2c_client);    
	unsigned char databuf[10];  
	databuf[0] = reg;  
	af7133e_af8133i_i2c_master_operate(af7133e_af8133i_i2c_client, databuf, 0x101, I2C_FLAG_READ);  
	*value = databuf[0];  
	return;
}
/*----------------------------------------------------------------------------*/
static int af7133e_af8133i_ReadSensorData(int *buf, int bufsize)
{
	if(NULL == af7133e_af8133i_i2c_client)
	{
		*buf = 0;
		return -2;
	}

	struct af7133e_af8133i_i2c_data *data = i2c_get_clientdata(af7133e_af8133i_i2c_client);  
	unsigned char databuf[10];
	int output[3];
        int i,j,k;
	
	if(NULL == af7133e_af8133i_i2c_client)
	{
		*buf = 0;
		return -2;
	}    
   
	// We can read all measured data in once
	databuf[0] = AF7133E_AF8133I_REG_DATA;
	//i2c_master_send(af7133_i2c_client, databuf, 1);    
	//i2c_master_recv(af7133_i2c_client, databuf, 6);
	af7133e_af8133i_i2c_master_operate(af7133e_af8133i_i2c_client, databuf, 0x601, I2C_FLAG_READ);

	output[0] = ((int) databuf[1]) << 8 | ((int) databuf[0]);
	output[1] = ((int) databuf[3]) << 8 | ((int) databuf[2]);
	output[2] = ((int) databuf[5]) << 8 | ((int) databuf[4]);

	//buf[0] = output[0] > 32767 ? output[0] - 65536 : output[0];
	//buf[1] = output[1] > 32767 ? output[1] - 65536 : output[1];
	//buf[2] = output[2] > 32767 ? output[2] - 65536 : output[2];

  for(i=0;i<3;i++) output[i] = (output[i] > 32767) ? (output[i] - 65536) : output[i];

  if(mag_cnt >= 0 && mag_cnt <= (FIND_SW_OFFSET_LOOP*2))
  {
    if(mag_cnt >= 1 && mag_cnt <= FIND_SW_OFFSET_LOOP)                        
      for(i=0;i<3;i++) mag_neg[i][mag_cnt-1] = output[i]; 
    else if(mag_cnt > FIND_SW_OFFSET_LOOP && mag_cnt <= (FIND_SW_OFFSET_LOOP*2))
      for(i=0;i<3;i++) mag_pos[i][mag_cnt-FIND_SW_OFFSET_LOOP-1] = output[i];
            
    mag_cnt++; 

    if(mag_cnt >= 1 && mag_cnt <= FIND_SW_OFFSET_LOOP) 
    {                       
      databuf[0] = 0x14;
	    databuf[1] = 0x34;
	    af7133e_af8133i_i2c_master_operate(af7133e_af8133i_i2c_client, databuf, 0x02, I2C_FLAG_WRITE);
    }
    else if(mag_cnt > FIND_SW_OFFSET_LOOP && mag_cnt <= (FIND_SW_OFFSET_LOOP*2))
    {
      databuf[0] = 0x14;
	    databuf[1] = 0x38;
	    af7133e_af8133i_i2c_master_operate(af7133e_af8133i_i2c_client, databuf, 0x02, I2C_FLAG_WRITE);
    }
    
    if(mag_cnt > (FIND_SW_OFFSET_LOOP*2))
    {
      for(i=0;i<3;i++)
      {    
        for(j=0;j<(FIND_SW_OFFSET_LOOP-1);j++)
        {
          for(k=0;k<(FIND_SW_OFFSET_LOOP-1);k++)
          {
            if(mag_neg[i][k] < mag_neg[i][k+1])
            {
              int tmp = mag_neg[i][k];
              mag_neg[i][k] = mag_neg[i][k+1];
              mag_neg[i][k+1] = tmp;
            }
            if(mag_pos[i][k] < mag_pos[i][k+1])
            {
              int tmp = mag_pos[i][k];
              mag_pos[i][k] = mag_pos[i][k+1];
              mag_pos[i][k+1] = tmp;
            } 
          }
        }
              
        mag_offset[i] = (mag_pos[i][(FIND_SW_OFFSET_INDEX)] + mag_neg[i][FIND_SW_OFFSET_INDEX]) / 2; 
      }
    }
  }       

  for(i=0;i<3;i++) buf[i] = output[i] - mag_offset[i];

  //Next data
	databuf[0] = 0x0A;
	databuf[1] = 0x01;
	af7133e_af8133i_i2c_master_operate(af7133e_af8133i_i2c_client, databuf, 0x02, I2C_FLAG_WRITE);

	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_daemon_name(struct device_driver *ddri, char *buf)
{
	char strbuf[AF7133E_AF8133I_BUFSIZE];
	sprintf(strbuf, "af7133e");
	return sprintf(buf, "%s", strbuf);
}

static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[AF7133E_AF8133I_BUFSIZE];
	//AF7133_ReadChipInfo(strbuf, AF7133E_AF8133I_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);        
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	int strbuf[AF7133E_AF8133I_BUFSIZE];
	af7133e_af8133i_ReadSensorData(strbuf, AF7133E_AF8133I_BUFSIZE);
	return sprintf(buf, "%d %d %d\n", strbuf[0],strbuf[1],strbuf[2]);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_posturedata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[AF7133E_AF8133I_BUFSIZE];
	//AF7133_ReadPostureData(strbuf, AF7133E_AF8133I_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_calidata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[AF7133E_AF8133I_BUFSIZE];
	//AF7133_ReadCaliData(strbuf, AF7133E_AF8133I_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_midcontrol_value(struct device_driver *ddri, char *buf)
{
	char strbuf[AF7133E_AF8133I_BUFSIZE];
	//AF7133_ReadMiddleControl(strbuf, AF7133E_AF8133I_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_midcontrol_value(struct device_driver *ddri, const char *buf, size_t count)
{   
	int p[10];
	if(10 == sscanf(buf, "%d %d %d %d %d %d %d %d %d %d",&p[0], &p[1], &p[2], &p[3], &p[4], 
		&p[5], &p[6], &p[7], &p[8], &p[9]))
	{
		write_lock(&af7133e_af8133i_mid_data.ctrllock);
		memcpy(&af7133e_af8133i_mid_data.controldata[0], &p, sizeof(int)*10);    
		write_unlock(&af7133e_af8133i_mid_data.ctrllock);        
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
	read_lock(&af7133e_af8133i_mid_data.ctrllock);
	len = sprintf(buf, "0x%08X\n", af7133e_af8133i_mid_data.debug);
	read_unlock(&af7133e_af8133i_mid_data.ctrllock);

	return len;            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_middebug_value(struct device_driver *ddri, const char *buf, size_t count)
{   
	int debug;
	if(1 == sscanf(buf, "0x%x", &debug))
	{
		write_lock(&af7133e_af8133i_mid_data.ctrllock);
		af7133e_af8133i_mid_data.debug = debug;
		write_unlock(&af7133e_af8133i_mid_data.ctrllock);        
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
	read_lock(&af7133e_af8133i_data.lock);
	mode = af7133e_af8133i_data.mode;
	read_unlock(&af7133e_af8133i_data.lock);        
	return sprintf(buf, "%d\n", mode);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_mode_value(struct device_driver *ddri, const char *buf, size_t count)
{
	int mode = 0;
	sscanf(buf, "%d", &mode);    
	af7133e_af8133i_SetMode(mode);
	return count;            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	if(NULL == af7133e_af8133i_i2c_client)
	{
		MSE_ERR("af7133e_af8133i_i2c_client IS NULL !\n");
		return -1;
	}
	struct af7133e_af8133i_i2c_data *data = i2c_get_clientdata(af7133e_af8133i_i2c_client);

	if(NULL == data)
	{
		MSE_ERR("data IS NULL !\n");
		return -1;
	}


	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		data->hw->direction,atomic_read(&data->layout),	data->cvt.sign[0], data->cvt.sign[1],
		data->cvt.sign[2],data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri, const char *buf, size_t count)
{
	if(NULL == af7133e_af8133i_i2c_client)
	{
		MSE_ERR("af7133e_af8133i_i2c_client IS NULL !\n");
		return -1;
	}
	struct af7133e_af8133i_i2c_data *data = i2c_get_clientdata(af7133e_af8133i_i2c_client);

	if(NULL == data)
	{
		MSE_ERR("data IS NULL !\n");
		return -1;
	}

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
	if(NULL == af7133e_af8133i_i2c_client)
	{
		MSE_ERR("af7133_i2c_client IS NULL !\n");
		return -1;
	}
	struct af7133e_af8133i_i2c_data *data = i2c_get_clientdata(af7133e_af8133i_i2c_client);

	if(NULL == data)
	{
		MSE_ERR("data IS NULL !\n");
		return -1;
	}

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
	if(NULL == af7133e_af8133i_i2c_client)
	{
		MSE_ERR("af7133e_af8133i_i2c_client IS NULL !\n");
		return -1;
	}
	struct af7133e_af8133i_i2c_data *data = i2c_get_clientdata(af7133e_af8133i_i2c_client);

	if(NULL == data)
	{
		MSE_ERR("data IS NULL !\n");
		return -1;
	}

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
	if(NULL == af7133e_af8133i_i2c_client)
	{
		MSE_ERR("af7133e_af8133i_i2c_client IS NULL !\n");
		return -1;
	}
	struct af7133e_af8133i_i2c_data *data = i2c_get_clientdata(af7133e_af8133i_i2c_client);

	if(NULL == data)
	{
		MSE_ERR("data IS NULL !\n");
		return -1;
	}	

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&data->trace));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	if(NULL == af7133e_af8133i_i2c_client)
	{
		MSE_ERR("af7133e_af8133i_i2c_client IS NULL !\n");
		return -1;
	}
	struct af7133e_af8133i_i2c_data *data = i2c_get_clientdata(af7133e_af8133i_i2c_client);

	if(NULL == data)
	{
		MSE_ERR("data IS NULL !\n");
		return -1;
	}
	int trace = 0;


	if(1 == sscanf(buf, "0x%x", &trace))
	{
		atomic_set(&data->trace, trace);
	}
	else 
	{
		MSE_ERR("invalid content: '%s', length = %zu\n", buf, count);
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
static struct driver_attribute *af7133e_af8133i_attr_list[] = {
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
static int af7133e_af8133i_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(af7133e_af8133i_attr_list)/sizeof(af7133e_af8133i_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, af7133e_af8133i_attr_list[idx])))
		{            
			MSE_ERR("driver_create_file (%s) = %d\n", af7133e_af8133i_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int af7133e_af8133i_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(af7133e_af8133i_attr_list)/sizeof(af7133e_af8133i_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}


	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, af7133e_af8133i_attr_list[idx]);
	}


	return err;
}


/*----------------------------------------------------------------------------*/
static int af7133e_af8133i_open(struct inode *inode, struct file *file)
{    
	struct af7133e_af8133i_i2c_data *obj = i2c_get_clientdata(af7133e_af8133i_i2c_client);    
	int ret = -1;
	atomic_inc(&dev_open_count);

	if(atomic_read(&obj->trace) & VTC_TRC_DEBUG)
	{
		MSE_LOG("Open device node:af7133e_af8133i\n");
	}
	ret = nonseekable_open(inode, file);

	return ret;
}
/*----------------------------------------------------------------------------*/
static int af7133e_af8133i_release(struct inode *inode, struct file *file)
{
	struct af7133e_af8133i_i2c_data *obj = i2c_get_clientdata(af7133e_af8133i_i2c_client);
	atomic_dec(&dev_open_count);
	if(atomic_read(&obj->trace) & VTC_TRC_DEBUG)
	{
		MSE_LOG("Release device node:af7133e_af8133i\n");
	}	
	return 0;
}
/*----------------------------------------------------------------------------*/
//static int af7133_ioctl(struct inode *inode, struct file *file, unsigned int cmd,unsigned long arg)//modified here
static long af7133e_af8133i_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
    void __user *argp = (void __user *)arg;
	int valuebuf[10] = {0};
	int magbuf[3] = {0};
	char strbuf[AF7133E_AF8133I_BUFSIZE] = {0};
	void __user *data;
	long retval=0;
	int mode=0;
	int status; 				/* for OPEN/CLOSE_STATUS */
	short sensor_status;		/* for Orientation and Msensor status */
	char version[10];
	unsigned char reg;

    struct mag_hw *hw = get_cust_mag_hw();
	
#if DEBUG
	if(NULL == af7133e_af8133i_i2c_client)
	{
		MSE_ERR("af7133e_af8133i_i2c_client IS NULL !\n");
		return -1;
	}
	struct af7133e_af8133i_i2c_data *obj = i2c_get_clientdata(af7133e_af8133i_i2c_client);

	if(NULL == obj)
	{
		MSE_ERR("obj IS NULL !\n");
		return -1;
	}
#endif

	int mag_layout;

	switch (cmd)
	{
		case MSENSOR_IOCTL_INIT:      
			//AF7133_Chipset_Init(AF7133_MODE_IDLE);
			af7133e_af8133i_Chipset_Init(AF7133E_AF8133I_MODE_SINGLE);
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
#if DEBUG
				if (atomic_read(&obj->trace) & VTC_TRC_DEBUG) {
						MSE_ERR("af7133e_af8133i driver: Osensor %d %d %d\n", valuebuf[0], valuebuf[1], valuebuf[2]);
				}
#endif

      
			write_lock(&af7133e_af8133i_mid_data.datalock);
			af7133e_af8133i_mid_data.yaw   = valuebuf[0];
			af7133e_af8133i_mid_data.pitch = valuebuf[1];
			af7133e_af8133i_mid_data.roll  = valuebuf[2];
			af7133e_af8133i_mid_data.nmx = valuebuf[3];
			af7133e_af8133i_mid_data.nmy = valuebuf[4];
			af7133e_af8133i_mid_data.nmz = valuebuf[5];
			af7133e_af8133i_mid_data.nax = valuebuf[6];
			af7133e_af8133i_mid_data.nay = valuebuf[7];
			af7133e_af8133i_mid_data.naz = valuebuf[8];
			af7133e_af8133i_mid_data.mag_status = valuebuf[9];
			write_unlock(&af7133e_af8133i_mid_data.datalock);    
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
			status = af7133e_af8133i_GetOpenStatus();			
			if(copy_to_user(argp, &status, sizeof(status)))
			{
				MSE_LOG("copy_to_user failed.");
				return -EFAULT;
			}
			break;

		case ECOMPASS_IOC_GET_DELAY: 			
			status = af7133e_af8133i_mid_data.controldata[0];						
			if(copy_to_user(argp, &status, sizeof(status)))			
			{				
				MSE_LOG("copy_to_user failed.");				
				return -EFAULT;			
			}			
			break;

		case MSENSOR_IOCTL_SET_CALIDATA:
			break;                                

		case MSENSOR_IOCTL_READ_CHIPINFO:
			data = (void __user *) arg;			
			if(data == NULL)			
			{				
				MSE_ERR("IO parameter pointer is NULL!\r\n");				
			break;    			
			}			
			sprintf(version, "%s-%s", (char*)DRIVER_VERSION, (char*)DRIVER_RELEASE);			
			if(copy_to_user(data, version, strlen(version)))			
			{				
				retval = -EFAULT;				
				goto err_out;			
			}          			
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
			af7133e_af8133i_ReadSensorData(magbuf, AF7133E_AF8133I_BUFSIZE);
#if DEBUG
				if (atomic_read(&obj->trace) & VTC_TRC_DEBUG) {
						MSE_ERR("af7133e_af8133i driver: Msensor %d %d %d\n", magbuf[0], magbuf[1], magbuf[2]);
				}
#endif			
			
			
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
			data = (void __user *) arg;			
			if(data == NULL)			
			{			  
				MSE_ERR("IO parameter pointer is NULL!\r\n");			  
				break;    			
			}			
			if(copy_from_user(strbuf, data, sizeof(strbuf[0])))			
			{				
				retval = -EFAULT;				
				goto err_out;			
			}                         
			reg = (unsigned char)strbuf[0];			
			af7133e_af8133i_Read_Regiser(reg, strbuf);						
			if(copy_to_user(data, strbuf, sizeof(strbuf[0])))			
				{				
				retval = -EFAULT;				
				goto err_out;			
				}
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
			if(af7133e_af8133i_SetMode(mode))
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

#ifdef CONFIG_COMPAT
static long af7133_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;

	void __user *arg32 = compat_ptr(arg);
	
	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;
	
    //printk("af7133_compat_ioctl arg: 0x%lx, arg32: 0x%p\n",arg, arg32);
	
	switch (cmd) {
		
		 case COMPAT_MSENSOR_IOCTL_INIT:
		 	 //printk("af7133_compat_ioctl COMPAT_MSENSOR_IOCTL_INIT\n");
			 if(arg32 == NULL)
			 {
				 MSE_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_INIT,
							(unsigned long)arg32);
			 if (ret){
			 	MSE_ERR("MSENSOR_IOCTL_INIT unlocked_ioctl failed.");
				return ret;
			 }			 

			 break;
		 case COMPAT_MSENSOR_IOCTL_SET_POSTURE:
		 	 //printk("af7133_compat_ioctl COMPAT_MSENSOR_IOCTL_SET_POSTURE\n");
			 ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_SET_POSTURE,
							(unsigned long)arg32);
			 if (ret){
			 	MSE_ERR("MSENSOR_IOCTL_SET_POSTURE unlocked_ioctl failed.");
				return ret;
			 }
		     break;		 
		 case COMPAT_ECOMPASS_IOC_GET_OFLAG:
		 	 //printk("af7311_compat_ioctl ECOMPASS_IOC_GET_OFLAG\n");
			 if(arg32 == NULL)
			 {
				 MSE_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, ECOMPASS_IOC_GET_OFLAG,
							(unsigned long)arg32);
			 if (ret){
			 	MSE_ERR("ECOMPASS_IOC_GET_OFLAG unlocked_ioctl failed.");
				return ret;
			 }
			 
			 break;

		case COMPAT_ECOMPASS_IOC_GET_MFLAG:
		 	 //printk("af7311_compat_ioctl COMPAT_ECOMPASS_IOC_GET_MFLAG\n");
			 ret = file->f_op->unlocked_ioctl(file, ECOMPASS_IOC_GET_MFLAG,
							(unsigned long)(arg32));
			 if (ret){
			 	MSE_ERR("ECOMPASS_IOC_GET_MFLAG unlocked_ioctl failed.");
				return ret;
			 }

			 break;
			 
		 case COMPAT_ECOMPASS_IOC_GET_OPEN_STATUS:
		 	 //printk("af7311_compat_ioctl COMPAT_ECOMPASS_IOC_GET_OPEN_STATUS\n");
			 ret = file->f_op->unlocked_ioctl(file, ECOMPASS_IOC_GET_OPEN_STATUS,
							(unsigned long)(arg32));
			 if (ret){
			 	MSE_ERR("ECOMPASS_IOC_GET_OPEN_STATUS unlocked_ioctl failed.");
				return ret;
			 }

			 break;
			 	 
		 case COMPAT_MSENSOR_IOCTL_SET_CALIDATA:
		 	 //printk("af7311_compat_ioctl COMPAT_MSENSOR_IOCTL_SET_CALIDATA\n");
			 if(arg32 == NULL)
			 {
				 MSE_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_SET_CALIDATA,
							(unsigned long)(arg32));
			 if (ret){
			 	MSE_ERR("MSENSOR_IOCTL_SET_CALIDATA unlocked_ioctl failed.");
				return ret;
			 }
			 break;
		
		 case COMPAT_MSENSOR_IOCTL_READ_CHIPINFO:
		 	 //printk("af7311_compat_ioctl COMPAT_MSENSOR_IOCTL_READ_CHIPINFO\n");
			 ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_CHIPINFO,
							(unsigned long)(arg32));
			 if (ret){
			 	MSE_ERR("MSENSOR_IOCTL_READ_CHIPINFO unlocked_ioctl failed.");
				return ret;
			 }

			 break;
			 
		 case COMPAT_MSENSOR_IOCTL_SENSOR_ENABLE:
		 	 //printk("af7311_compat_ioctl COMPAT_MSENSOR_IOCTL_SENSOR_ENABLE\n");
			 if(arg32 == NULL)
			 {
				 MSE_ERR("invalid argument.");
				 return -EINVAL;
			 }		
			 
			 ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_SENSOR_ENABLE,
							(unsigned long)(arg32));
			 if (ret){
			 	MSE_ERR("MSENSOR_IOCTL_SENSOR_ENABLE unlocked_ioctl failed.");
				return ret;
			 }
			 break;
		
		 case COMPAT_MSENSOR_IOCTL_READ_SENSORDATA:
		 	 //printk("af7311_compat_ioctl COMPAT_MSENSOR_IOCTL_READ_SENSORDATA\n");
			 ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_SENSORDATA,
							(unsigned long)(arg32));
			 if (ret){
			 	MSE_ERR("MSENSOR_IOCTL_READ_SENSORDATA unlocked_ioctl failed.");
				return ret;
			 }

			 break;
			 
		 case COMPAT_MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
		 	 //printk("af7311_compat_ioctl COMPAT_MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:\n");
			 ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_FACTORY_SENSORDATA,
							(unsigned long)(arg32));
			 if (ret){
			 	MSE_ERR("MSENSOR_IOCTL_READ_FACTORY_SENSORDATA unlocked_ioctl failed.");
				return ret;
			 }

			 break;
			 
		 case COMPAT_MSENSOR_IOCTL_READ_POSTUREDATA:
		 	 //printk("af7311_compat_ioctl COMPAT_MSENSOR_IOCTL_READ_POSTUREDATA\n");
			 ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_POSTUREDATA,
							(unsigned long)(arg32));
			 if (ret){
			 	MSE_ERR("MSENSOR_IOCTL_READ_POSTUREDATA unlocked_ioctl failed.");
				return ret;
			 }

			 break;
			 
		 case COMPAT_MSENSOR_IOCTL_READ_CALIDATA:
		 	 //printk("af7311_compat_ioctl COMPAT_MSENSOR_IOCTL_READ_CALIDATA\n");
			 ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_CALIDATA,
							(unsigned long)(arg32));
			 if (ret){
			 	MSE_ERR("MSENSOR_IOCTL_READ_CALIDATA unlocked_ioctl failed.");
				return ret;
			 }
			 
			 break;
		
		 case COMPAT_MSENSOR_IOCTL_READ_CONTROL:
			 //printk("af7311 COMPAT_MSENSOR_IOCTL_READ_CONTROL\n");
			 ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_CONTROL,
							(unsigned long)arg32);
			 if (ret){
			 	MSE_ERR("MSENSOR_IOCTL_READ_CONTROL unlocked_ioctl failed.");
				return ret;
			 }
			 
			 break;
		
		 case COMPAT_MSENSOR_IOCTL_SET_CONTROL:
		 	 //printk("af7311_compat_ioctl COMPAT_SENSOR_IOCTL_SET_CONTROL\n");
			 ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_SET_CONTROL,
							(unsigned long)arg32);
			 if (ret){
			 	MSE_ERR("SENSOR_IOCTL_SET_CONTROL unlocked_ioctl failed.");
				return ret;
			 }
			 
			 break;
		
		 case COMPAT_MSENSOR_IOCTL_SET_MODE:	
		 	 //printk("af7311_compat_ioctl COMPAT_MSENSOR_IOCTL_SET_MODE\n");
			 ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_SET_MODE,
							(unsigned long)arg32);
			 if (ret){
			 	MSE_ERR("MSENSOR_IOCTL_SET_MODE unlocked_ioctl failed.");
				return ret;
			 }

			 break;
			 
		 case COMPAT_ECOMPASS_IOC_GET_LAYOUT:
		 	 //printk("af7311_compat_ioctl COMPAT_ECOMPASS_IOC_GET_LAYOUT\n");
			 if(arg32 == NULL)
			 {
				 MSE_ERR("invalid argument.");
				 return -EINVAL;
			 }

			 ret = file->f_op->unlocked_ioctl(file, ECOMPASS_IOC_GET_LAYOUT,
							(unsigned long)(arg32));
			 if (ret){
			 	MSE_ERR("ECOMPASS_IOC_GET_LAYOUT unlocked_ioctl failed.");
				return ret;
			 }
			 
			 break;
			 
		 default:
			 //printk(KERN_ERR "%s not supported = 0x%04x", __FUNCTION__, cmd);
			 return -ENOIOCTLCMD;
			 break;
	}
	return 0;
}
#endif

/*----------------------------------------------------------------------------*/
static struct file_operations af7133e_af8133i_fops = {
//	.owner = THIS_MODULE,
	.open = af7133e_af8133i_open,
	.release = af7133e_af8133i_release,
	.unlocked_ioctl = af7133e_af8133i_unlocked_ioctl,//modified
	#ifdef CONFIG_COMPAT
	.compat_ioctl = af7133_compat_ioctl,
	#endif
};
/*----------------------------------------------------------------------------*/
static struct miscdevice af7133e_af8133i_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "msensor",
    .fops = &af7133e_af8133i_fops,
};
/*----------------------------------------------------------------------------*/
#ifdef AF7133_NEW_ARCH
static int af7133e_af8133i_m_open_report_data(int en)
{
#if 0
	if(NULL == af7133_i2c_client)
	{
		MSE_ERR("af7133_i2c_client IS NULL !\n");
		return -1;
	}
	struct af7133_i2c_data *data = i2c_get_clientdata(af7133_i2c_client);

	if(NULL == data)
	{
		MSE_ERR("data IS NULL !\n");
		return -1;
	}
#endif
	return 0;
}
static int af7133e_af8133i_m_set_delay(u64 delay)
{
	int value = 0;
	int sample_delay = af7133e_af8133i_mid_data.controldata[0];
	
	if(NULL == af7133e_af8133i_i2c_client)
	{
		MSE_ERR("af7133e_af8133i_i2c_client IS NULL !\n");
		return -1;
	}
	struct af7133e_af8133i_i2c_data *data = i2c_get_clientdata(af7133e_af8133i_i2c_client);

	if(NULL == data)
	{
		MSE_ERR("data IS NULL !\n");
		return -1;
	}

	value = (int)delay;
	if(value < AF7133E_AF8133I_DELAY_MIN)
	{
		sample_delay = AF7133E_AF8133I_DELAY_MIN;
	}
	else if(value > AF7133E_AF8133I_DELAY_MAX)
	{
		sample_delay = AF7133E_AF8133I_DELAY_MAX;
	}
	else
	{
		sample_delay = value;
	}
	
	af7133e_af8133i_mid_data.controldata[0] = sample_delay;  // Loop Delay


	return 0;
}
static int af7133e_af8133i_m_enable(int en)
{
	int value = 0;
	if(NULL == af7133e_af8133i_i2c_client)
	{
		MSE_ERR("af7133e_af8133i_i2c_client IS NULL !\n");
		return -1;
	}
	struct af7133e_af8133i_i2c_data *data = i2c_get_clientdata(af7133e_af8133i_i2c_client);

	if(NULL == data)
	{
		MSE_ERR("data IS NULL !\n");
		return -1;
	}

	value = en;
	read_lock(&af7133e_af8133i_mid_data.ctrllock);
	if(value == 1)
	{
		af7133e_af8133i_mid_data.controldata[7] |= SENSOR_MAGNETIC;
		atomic_set(&m_flag, 1);
		atomic_set(&open_flag, 1);
	}
	else
	{
		af7133e_af8133i_mid_data.controldata[7] &= ~SENSOR_MAGNETIC;
		atomic_set(&m_flag, 0);
		if(atomic_read(&o_flag) == 0)
		{
			atomic_set(&open_flag, 0);
		}	
	}
	wake_up(&open_wq);
	read_unlock(&af7133e_af8133i_mid_data.ctrllock);
	// TODO: turn device into standby or normal mode


	return 0;
}
static int af7133e_af8133i_o_open_report_data(int en)
{
#if 0
		if(NULL == af7133_i2c_client)
		{
			MSE_ERR("af7133_i2c_client IS NULL !\n");
			return -1;
		}
		struct af7133_i2c_data *data = i2c_get_clientdata(af7133_i2c_client);
		
		if(NULL == data)
		{
			MSE_ERR("data IS NULL !\n");
			return -1;
		}

#endif
	return 0;

}
static int af7133e_af8133i_o_set_delay(u64 delay)
{
	return af7133e_af8133i_m_set_delay(delay);//the same is msensor set_delay
}
static int af7133e_af8133i_o_enable(int en)
{
	int value = 0;
	if(NULL == af7133e_af8133i_i2c_client)
	{
		MSE_ERR("af7133e_af8133i_i2c_client IS NULL !\n");
		return -1;
	}
	struct af7133e_af8133i_i2c_data *data = i2c_get_clientdata(af7133e_af8133i_i2c_client);

	if(NULL == data)
	{
		MSE_ERR("data IS NULL !\n");
		return -1;
	}
	
	value = en;
	read_lock(&af7133e_af8133i_mid_data.ctrllock);
	if(value == 1)
	{
		af7133e_af8133i_mid_data.controldata[7] |= SENSOR_ORIENTATION;
		atomic_set(&o_flag, 1);
		atomic_set(&open_flag, 1);
	}
	else
	{
		af7133e_af8133i_mid_data.controldata[7] &= ~SENSOR_ORIENTATION;
		atomic_set(&o_flag, 0);
		if(atomic_read(&m_flag) == 0)
		{
			atomic_set(&open_flag, 0);
		}
	}
	wake_up(&open_wq);
	read_unlock(&af7133e_af8133i_mid_data.ctrllock);


	return 0;
}
static int af7133e_af8133i_get_data_m(int *x,int *y, int *z,int *status)
{
	int x_tmp = 0;
	int y_tmp = 0;
	int z_tmp = 0;
	
	if(NULL == af7133e_af8133i_i2c_client)
	{
		MSE_ERR("af7133e_af8133i_i2c_client IS NULL !\n");
		return -1;
	}
	struct af7133e_af8133i_i2c_data *data = i2c_get_clientdata(af7133e_af8133i_i2c_client);

	if(NULL == data)
	{
		MSE_ERR("data IS NULL !\n");
		return -1;
	}

	read_lock(&af7133e_af8133i_mid_data.datalock);
	x_tmp = af7133e_af8133i_mid_data.nmx;
	y_tmp = af7133e_af8133i_mid_data.nmy;
	z_tmp = af7133e_af8133i_mid_data.nmz;
	*status = af7133e_af8133i_mid_data.mag_status;
	read_unlock(&af7133e_af8133i_mid_data.datalock); 
	
	*x = x_tmp * CONVERT_M;
	*y = y_tmp * CONVERT_M;
	*z = z_tmp * CONVERT_M;


#if DEBUG
	if (atomic_read(&data->trace) & VTC_TRC_DEBUG) {
			MSE_LOG("%s get data: %d, %d, %d. divide %d, status %d!", __func__,
			*x, *y, *z, CONVERT_M_DIV, *status);
	}
#endif

	return 0;
}
static int af7133e_af8133i_get_data_o(int *x,int *y, int *z,int *status)
{
	if(NULL == af7133e_af8133i_i2c_client)
	{
		MSE_ERR("af7133e_af8133i_i2c_client IS NULL !\n");
		return -1;
	}
	struct af7133e_af8133i_i2c_data *data = i2c_get_clientdata(af7133e_af8133i_i2c_client);

	if(NULL == data)
	{
		MSE_ERR("data IS NULL !\n");
		return -1;
	}


	read_lock(&af7133e_af8133i_mid_data.datalock);
	*x = af7133e_af8133i_mid_data.yaw;
	*y = af7133e_af8133i_mid_data.pitch;
	*z = af7133e_af8133i_mid_data.roll;
	*status = af7133e_af8133i_mid_data.mag_status;	
	read_unlock(&af7133e_af8133i_mid_data.datalock); 

#if DEBUG
	if (atomic_read(&data->trace) & VTC_TRC_DEBUG) {
			MSE_ERR("%s get data: %d, %d, %d. divide %d, status %d!", __func__,
			*x, *y, *z,	CONVERT_O_DIV, *status);
	}
#endif
	return 0;
}

#else

int af7133e_af8133i_operate(void* self, uint32_t command, void* buff_in, int size_in,
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
				if(value < AF7133E_AF8133I_DELAY_MIN)
				{
					sample_delay = AF7133E_AF8133I_DELAY_MIN;
				}
				else if(value > AF7133E_AF8133I_DELAY_MAX)
				{
					sample_delay = AF7133E_AF8133I_DELAY_MAX;
				}
				else
				{
					sample_delay = value;
				}

				af7133e_af8133i_mid_data.controldata[0] = sample_delay;  // Loop Delay
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
				read_lock(&af7133e_af8133i_mid_data.ctrllock);
				if(value == 1)
				{
					af7133e_af8133i_mid_data.controldata[7] |= SENSOR_MAGNETIC;
					atomic_set(&m_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					af7133e_af8133i_mid_data.controldata[7] &= ~SENSOR_MAGNETIC;
					atomic_set(&m_flag, 0);
					if(atomic_read(&o_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}	
				}
				wake_up(&open_wq);
				read_unlock(&af7133e_af8133i_mid_data.ctrllock);
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
				read_lock(&af7133e_af8133i_mid_data.datalock);
				msensor_data->values[0] = af7133e_af8133i_mid_data.nmx;
				msensor_data->values[1] = af7133e_af8133i_mid_data.nmy;
				msensor_data->values[2] = af7133e_af8133i_mid_data.nmz;
				msensor_data->status = af7133e_af8133i_mid_data.mag_status;
				read_unlock(&af7133e_af8133i_mid_data.datalock); 

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
int af7133e_af8133i_orientation_operate(void* self, uint32_t command, void* buff_in, int size_in,
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
				if(value < AF7133E_AF8133I_DELAY_MIN)
				{
					sample_delay = AF7133E_AF8133I_DELAY_MIN;
				}
				else if(value > AF7133E_AF8133I_DELAY_MAX)
				{
					sample_delay = AF7133E_AF8133I_DELAY_MAX;
				}
				else
				{
					sample_delay = value;
				}

				af7133e_af8133i_mid_data.controldata[0] = sample_delay;  // Loop Delay
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
				read_lock(&af7133e_af8133i_mid_data.ctrllock);
				if(value == 1)
				{
					af7133e_af8133i_mid_data.controldata[7] |= SENSOR_ORIENTATION;
					atomic_set(&o_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					af7133e_af8133i_mid_data.controldata[7] &= ~SENSOR_ORIENTATION;
					atomic_set(&o_flag, 0);
					if(atomic_read(&m_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}
				}
				wake_up(&open_wq);
				read_unlock(&af7133e_af8133i_mid_data.ctrllock);
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
				read_lock(&af7133e_af8133i_mid_data.datalock);
				osensor_data->values[0] = af7133e_af8133i_mid_data.yaw;
				osensor_data->values[1] = af7133e_af8133i_mid_data.pitch;
				osensor_data->values[2] = af7133e_af8133i_mid_data.roll;
				osensor_data->status = af7133e_af8133i_mid_data.mag_status;	
				read_unlock(&af7133e_af8133i_mid_data.datalock); 

				osensor_data->value_divide = CONVERT_O_DIV;
			}
			break;
			
		default:
            MSE_ERR("osensor operate function no this parameter %d!\n", command); 
			err = -1;
			break;
	}

	return err;
}
#endif
/*----------------------------------------------------------------------------*/
//#ifndef	CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int af7133e_af8133i_suspend(struct i2c_client *client, pm_message_t msg) 
{
	//int err;
	struct af7133e_af8133i_i2c_data *obj = i2c_get_clientdata(client);
	MSE_FUN();    
#if 1
	if(msg.event == PM_EVENT_SUSPEND)
	{   
	  af7133e_af8133i_power(obj->hw, 0);
	}
#endif
	return 0;
}
/*----------------------------------------------------------------------------*/
static int af7133e_af8133i_resume(struct i2c_client *client)
{
	int err;
	struct af7133e_af8133i_i2c_data *obj = i2c_get_clientdata(client);
	MSE_FUN();
#if 1
	af7133e_af8133i_power(obj->hw, 1);

	if((err = af7133e_af8133i_Chipset_Init(AF7133E_AF8133I_MODE_SINGLE))!=0)
	{
		MSE_ERR("initialize client fail!!\n");
		return err;        
	}
#endif
	return 0;
}
/*----------------------------------------------------------------------------*/
//#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void af7133_early_suspend(struct early_suspend *h) 
{
	struct af7133e_af8133i_i2c_data *obj = container_of(h, struct af7133e_af8133i_i2c_data, early_drv);   
	//int err;
	MSE_FUN();    

	if(NULL == obj)
	{
		MSE_ERR("null pointer!!\n");
		return;
	}
	af7133e_af8133i_power(obj->hw, 0);
}
/*----------------------------------------------------------------------------*/
static void af7133_late_resume(struct early_suspend *h)
{
	struct af7133e_af8133i_i2c_data *obj = container_of(h, struct af7133e_af8133i_i2c_data, early_drv);         
	int err;
	MSE_FUN();

	if(NULL == obj)
	{
		MSE_ERR("null pointer!!\n");
		return;
	}

	af7133e_af8133i_power(obj->hw, 1);

	if((err = af7133e_af8133i_Chipset_Init(AF7133E_AF8133I_MODE_SINGLE))!=0)
	{
		MSE_ERR("initialize client fail!!\n");
		return ;        
	}

}
/*----------------------------------------------------------------------------*/
//#endif /*CONFIG_HAS_EARLYSUSPEND*/

/*----------------------------------------------------------------------------*/
static int af7133e_af8133i_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct af7133e_af8133i_i2c_data *data;
	int err = 0;
#ifdef AF7133_NEW_ARCH
			struct mag_control_path ctl_path ={0};
			struct mag_data_path dat_path = {0};
#else

	struct hwmsen_object sobj_m, sobj_o;
#endif    
    printk("%s start\n",__func__);
	if (!(data = kmalloc(sizeof(struct af7133e_af8133i_i2c_data), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	memset(data, 0, sizeof(struct af7133e_af8133i_i2c_data));
	

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

	af7133e_af8133i_i2c_client = new_client;	

	if((err = af7133e_af8133i_Chipset_Init(AF7133E_AF8133I_MODE_IDLE)))
	{
		goto exit_init_failed;
	}

	/* Register sysfs attribute */
#ifdef AF7133_NEW_ARCH
	if(err = af7133e_af8133i_create_attr(&af7133_init_info.platform_diver_addr->driver))
#else
	if((err = af7133e_af8133i_create_attr(&vtc_sensor_driver.driver)))
#endif
	{
		MSE_ERR("create attribute err = %d\n", err);
		goto exit_sysfs_create_group_failed;
	}

	if((err = misc_register(&af7133e_af8133i_device)))
	{
		MSE_ERR("af7133e_af8133i_device register failed\n");
		goto exit_misc_device_register_failed;	}    

#ifdef AF7133_NEW_ARCH
		ctl_path.m_open_report_data = af7133e_af8133i_m_open_report_data;
		ctl_path.m_enable 			= af7133e_af8133i_m_enable;
		ctl_path.m_set_delay 		= af7133e_af8133i_m_set_delay;
		
		ctl_path.o_open_report_data = af7133e_af8133i_o_open_report_data;
		ctl_path.o_enable 			= af7133e_af8133i_o_enable;
		ctl_path.o_set_delay 		= af7133e_af8133i_o_set_delay;

		ctl_path.is_report_input_direct = false;
		ctl_path.is_support_batch 		= data->hw->is_batch_supported;
		
		err = mag_register_control_path(&ctl_path);

		if(err < 0)
		{
			MSE_ERR("mag_register_control_path failed!\n");
			goto exit_misc_device_register_failed;
		}

		dat_path.div_m = CONVERT_M_DIV;
		dat_path.div_o = CONVERT_O_DIV;

		dat_path.get_data_m = af7133e_af8133i_get_data_m;
		dat_path.get_data_o = af7133e_af8133i_get_data_o;

		err = mag_register_data_path(&dat_path);
		if(err < 0)
		{
			MSE_ERR("mag_register_control_path failed!\n");
			goto exit_misc_device_register_failed;
		}
#else

	  sobj_m.self = data;
    sobj_m.polling = 1;
    sobj_m.sensor_operate = af7133e_af8133i_operate;
	if((err = hwmsen_attach(ID_MAGNETIC, &sobj_m)))
	{
		MSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}

	sobj_o.self = data;
    sobj_o.polling = 1;
    sobj_o.sensor_operate = af7133e_af8133i_orientation_operate;
	if((err = hwmsen_attach(ID_ORIENTATION, &sobj_o)))
	{
		MSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}
#endif
#if CONFIG_HAS_EARLYSUSPEND
	data->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	data->early_drv.suspend  = af7133_early_suspend,
	data->early_drv.resume   = af7133_late_resume,    
	register_early_suspend(&data->early_drv);
#endif

	MSE_LOG("%s: OK\n", __func__);
#ifdef AF7133_NEW_ARCH
	af7133e_af8133i_init_flag=0;
#endif
	return 0;

	exit_sysfs_create_group_failed:   
	exit_init_failed:
	//i2c_detach_client(new_client);
	exit_misc_device_register_failed:
	exit_kfree:
	kfree(data);
	exit:
	MSE_ERR("%s: err = %d\n", __func__, err);
#ifdef AF7133_NEW_ARCH
	af7133e_af8133i_init_flag=-1;
#endif
	return err;
}
/*----------------------------------------------------------------------------*/
static int af7133e_af8133i_i2c_remove(struct i2c_client *client)
{
	int err;	
#ifdef AF7133_NEW_ARCH
	if(err = af7133e_af8133i_delete_attr(&af7133_init_info.platform_diver_addr->driver))
#else
	if((err = af7133e_af8133i_delete_attr(&vtc_sensor_driver.driver)))
#endif
	{
		MSE_ERR("af7133e_af8133i_delete_attr fail: %d\n", err);
	}

	af7133e_af8133i_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));	
	misc_deregister(&af7133e_af8133i_device);    
	return 0;
}
/*----------------------------------------------------------------------------*/
#ifdef AF7133_NEW_ARCH
/*----------------------------------------------------------------------------*/
static int af7133e_af8133i_local_init(void)
{
	struct mag_hw *hw = get_cust_mag_hw();
	printk("af7133e_af8133i_local_init");

	af7133e_af8133i_power(hw, 1);    
	rwlock_init(&af7133e_af8133i_mid_data.ctrllock);
	rwlock_init(&af7133e_af8133i_mid_data.datalock);
	rwlock_init(&af7133e_af8133i_data.lock);
	memset(&af7133e_af8133i_mid_data.controldata[0], 0, sizeof(int)*10);    
	af7133e_af8133i_mid_data.controldata[0] =    AF7133E_AF8133I_DEFAULT_DELAY;  // Loop Delay
	af7133e_af8133i_mid_data.controldata[1] =     0;  // Run   
	af7133e_af8133i_mid_data.controldata[2] =     0;  // Disable Start-AccCali
	af7133e_af8133i_mid_data.controldata[3] =     1;  // Enable Start-Cali
	af7133e_af8133i_mid_data.controldata[4] =   350;  // MW-Timout
	af7133e_af8133i_mid_data.controldata[5] =    10;  // MW-IIRStrength_M
	af7133e_af8133i_mid_data.controldata[6] =    10;  // MW-IIRStrength_G   
	af7133e_af8133i_mid_data.controldata[7] =     0;  // Active Sensors
	af7133e_af8133i_mid_data.controldata[8] =     0;  // Wait for define
	af7133e_af8133i_mid_data.controldata[9] =     0;  // Wait for define   
	atomic_set(&dev_open_count, 0);

	if(i2c_add_driver(&af7133e_af8133i_i2c_driver))
	{
		MSE_ERR("add driver error\n");
		return -1;
	} 
	
	if(-1 == af7133e_af8133i_init_flag)	
	{	   
		return -1;	
	}

	mag_cnt = 0;
  	mag_offset[0] = 0;
  	mag_offset[1] = 0;
  	mag_offset[2] = 0;
	
    printk("%s done\n",__func__);
	return 0;
#if 0
	struct mag_hw *hw = af7133_get_cust_mag_hw();
	printk("af7133_local_init");
	af7133e_af8133i_power(hw, 1);
	//af7133_force[0] = hw->i2c_num;
#if 1
	if(i2c_add_driver(&af7133_i2c_driver))
	{
		printk("add driver error\n");
		return -1;
	}
#endif
	if(-1 == af7133e_af8133i_init_flag)	
	{	   
		return -1;	
	}
	return 0;
#endif
}

static int vtc_remove(void)
{
	struct mag_hw *hw = get_cust_mag_hw();

	MSE_FUN();    
	af7133e_af8133i_power(hw, 0);    
	atomic_set(&dev_open_count, 0);  
	i2c_del_driver(&af7133e_af8133i_i2c_driver);
	af7133e_af8133i_init_flag = -1;
	return 0;
#if 0
    	struct mag_hw *hw = af7133_get_cust_mag_hw();

	MSE_FUN();    
	af7133e_af8133i_power(hw, 0);    
	atomic_set(&dev_open_count, 0);  
	i2c_del_driver(&af7133e_af8133i_i2c_driver);
	return 0;
#endif
}
/*----------------------------------------------------------------------------*/

#else

static int vtc_probe(struct platform_device *pdev) 
{
	struct mag_hw *hw = get_cust_mag_hw();
	int err=0;

	af7133e_af8133i_power(hw, 1);    
	rwlock_init(&af7133e_af8133i_mid_data.ctrllock);
	rwlock_init(&af7133e_af8133i_mid_data.datalock);
	rwlock_init(&af7133e_af8133i_data.lock);
	memset(&af7133e_af8133i_mid_data.controldata[0], 0, sizeof(int)*10);    
	af7133e_af8133i_mid_data.controldata[0] =    AF7133E_AF8133I_DEFAULT_DELAY;  // Loop Delay
	af7133e_af8133i_mid_data.controldata[1] =     0;  // Run   
	af7133e_af8133i_mid_data.controldata[2] =     0;  // Disable Start-AccCali
	af7133e_af8133i_mid_data.controldata[3] =     1;  // Enable Start-Cali
	af7133e_af8133i_mid_data.controldata[4] =   350;  // MW-Timout
	af7133e_af8133i_mid_data.controldata[5] =    10;  // MW-IIRStrength_M
	af7133e_af8133i_mid_data.controldata[6] =    10;  // MW-IIRStrength_G   
	af7133e_af8133i_mid_data.controldata[7] =     0;  // Active Sensors
	af7133e_af8133i_mid_data.controldata[8] =     0;  // Wait for define
	af7133e_af8133i_mid_data.controldata[9] =     0;  // Wait for define   
	atomic_set(&dev_open_count, 0);

	if(i2c_add_driver(&af7133e_af8133i_i2c_driver))
	{
		MSE_ERR("add af7133e_af8133i i2c driver error\n");
		return -1;
	}

	err = af7133e_af8133i_SetADC();

	if(err<0)
	{
		return err;
	}
	
	mag_cnt = 0;
  	mag_offset[0] = 0;
  	mag_offset[1] = 0;
  	mag_offset[2] = 0;
  
    printk("%s done\n",__func__);
	return 0;
}
/*----------------------------------------------------------------------------*/
static int vtc_remove(struct platform_device *pdev)
{
	struct mag_hw *hw = get_cust_mag_hw();

	MSE_FUN();    
	af7133e_af8133i_power(hw, 0);    
	atomic_set(&dev_open_count, 0);  
	i2c_del_driver(&af7133e_af8133i_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
#endif


#ifndef AF7133_NEW_ARCH
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
//		.owner = THIS_MODULE,
	#ifdef CONFIG_OF
		.of_match_table = msensor_of_match,
	#endif
	}
};
#endif
/*----------------------------------------------------------------------------*/
static int __init af7133e_af8133i_init(void)
{
	MSE_FUN();
#if 0
#ifdef AF7133_NEW_ARCH
    	struct mag_hw *hw = af7133_get_cust_mag_hw();
#else
	struct mag_hw *hw = get_cust_mag_hw();
#endif
#endif
	struct mag_hw *hw = get_cust_mag_hw();

	i2c_register_board_info(hw->i2c_num, &i2c_af7133e_af8133i, 1);
	
#ifdef AF7133_NEW_ARCH
	//hwmsen_msensor_add(&af7133_init_info);
/*	if(i2c_add_driver(&af7133_i2c_driver))
	{
		printk("senodia_init  -1.5 add i2c  driver error\n");
		return -1;
	}*/
	if(mag_driver_add(&af7133e_af8133i_init_info) < 0)
	{
		MSE_ERR("mag_driver_add failed!\n");
	}
#else
	if(platform_driver_register(&vtc_sensor_driver))
	{
		MSE_ERR("af7133e_af8133i: failed to register driver");
		return -ENODEV;
	}
	else
	{
		MSE_ERR("af7133e_af8133i: passed to register driver");
	}
#endif
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit af7133e_af8133i_exit(void)
{
	MSE_FUN();
#ifndef AF7133_NEW_ARCH
	platform_driver_unregister(&vtc_sensor_driver);
#endif
}
/*----------------------------------------------------------------------------*/
module_init(af7133e_af8133i_init);
module_exit(af7133e_af8133i_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Gary Huang");
MODULE_DESCRIPTION("AF7133E_AF8133I m-Sensor driver");
MODULE_LICENSE("VTC");
MODULE_VERSION(DRIVER_VERSION);

