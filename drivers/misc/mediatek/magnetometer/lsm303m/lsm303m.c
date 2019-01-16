
/* LSM303M motion sensor driver
 *
 *
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
#include "lsm303m.h"
#include <linux/hwmsen_helper.h>




#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>



/*-------------------------MT6516&MT6573 define-------------------------------*/

#define POWER_NONE_MACRO MT65XX_POWER_NONE




#define SW_CALIBRATION

/*----------------------------------------------------------------------------*/
#define I2C_DRIVERID_LSM303M 345
/*----------------------------------------------------------------------------*/
#define DEBUG 1
/*----------------------------------------------------------------------------*/
#define CONFIG_LSM303M_LOWPASS   /*apply low pass filter on output*/       
/*----------------------------------------------------------------------------*/
#define LSM303M_AXIS_X          0
#define LSM303M_AXIS_Y          1
#define LSM303M_AXIS_Z          2
#define LSM303M_AXES_NUM        3
#define LSM303M_DATA_LEN        6
#define LSM303M_DEV_NAME        "lsm303m"


static DECLARE_WAIT_QUEUE_HEAD(data_ready_wq);
static DECLARE_WAIT_QUEUE_HEAD(open_wq);

static atomic_t open_flag = ATOMIC_INIT(0);
static atomic_t m_flag = ATOMIC_INIT(0);
static atomic_t o_flag = ATOMIC_INIT(0);
/*----------------------------------------------------------------------------*/
static atomic_t dev_open_count;

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id lsm303m_i2c_id[] = {{LSM303M_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_lsm303m={ I2C_BOARD_INFO("lsm303m", 0x3B>>1)};
/*the adapter id will be available in customization*/
//static unsigned short lsm303m_force[] = {0x00, LSM303M_I2C_SLAVE_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const lsm303m_forces[] = { lsm303m_force, NULL };
//static struct i2c_client_address_data lsm303m_addr_data = { .forces = lsm303m_forces,};

/*----------------------------------------------------------------------------*/
static int lsm303m_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int lsm303m_i2c_remove(struct i2c_client *client);
//static int lsm303m_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int lsm303m_suspend(struct i2c_client *client, pm_message_t msg) ;
static int lsm303m_resume(struct i2c_client *client);

/*----------------------------------------------------------------------------*/
typedef enum {
    ADX_TRC_FILTER  =     0x01,
    ADX_TRC_RAWDATA =     0x02,
    ADX_TRC_IOCTL   =     0x04,
    ADX_TRC_CALI	= 0X08,
    ADX_TRC_INFO	= 0X10,
    
} ADX_TRC;
/*----------------------------------------------------------------------------*/
struct scale_factor{
    u8  whole;
    u8  fraction;
};
/*----------------------------------------------------------------------------*/
struct data_resolution {
    struct scale_factor scalefactor;
    int                 sensitivity;
};
/*----------------------------------------------------------------------------*/
#define C_MAX_FIR_LENGTH (32)
/*----------------------------------------------------------------------------*/
struct data_filter {
    s16 raw[C_MAX_FIR_LENGTH][LSM303M_AXES_NUM];
    int sum[LSM303M_AXES_NUM];
    int num;
    int idx;
};
/*----------------------------------------------------------------------------*/
struct _lsm303m_data {
    rwlock_t lock;
    int mode;
    int rate;
    volatile int updated;
} lsm303m_data;

/*----------------------------------------------------------------------------*/
struct lsm303m_data {
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
}lsm303mmid_data;

/*----------------------------------------------------------------------------*/
struct lsm303m_i2c_data {
    struct i2c_client *client;
    struct mag_hw *hw;
    struct hwmsen_convert   cvt;
    
    /*misc*/
    struct data_resolution  reso;
    atomic_t                trace;
	atomic_t 				layout;   
    atomic_t                suspend;
    atomic_t                selftest;
	atomic_t				filter;
    s16                     cali_sw[LSM303M_AXES_NUM+1];

    /*data*/
    s8                      offset[LSM303M_AXES_NUM+1];  /*+1: for 4-byte alignment*/
    s32                     data[LSM303M_AXES_NUM+1];

#if defined(CONFIG_LSM303M_LOWPASS)
    atomic_t                firlen;
    atomic_t                fir_en;
    struct data_filter      fir;
#endif 
    /*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_drv;
#endif     
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver lsm303m_i2c_driver = {
    .driver = {
//        .owner          = THIS_MODULE,
        .name           = LSM303M_DEV_NAME,
    },
	.probe      		= lsm303m_i2c_probe,
	.remove    			= lsm303m_i2c_remove,
//	.detect				= lsm303m_i2c_detect,
//#if !defined(CONFIG_HAS_EARLYSUSPEND)    
    .suspend            = lsm303m_suspend,
    .resume             = lsm303m_resume,
//#endif
	.id_table = lsm303m_i2c_id,
	//.address_list= lsm303m_forces,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *lsm303m_i2c_client = NULL;
static struct platform_driver lsm303m_msensor_driver;
static struct lsm303m_i2c_data *obj_i2c_data = NULL;
static bool sensor_power = false;
//static MSENSOR_VECTOR3D msensor_gain;
static char selftestRes[8]= {0}; 

/*----------------------------------------------------------------------------*/
#define MSE_TAG                  "MSENSOR"
#define MSE_FUN(f)               printk(MSE_TAG" %s\r\n", __FUNCTION__)
#define MSE_ERR(fmt, args...)    printk(KERN_ERR MSE_TAG" %s %d : \r\n" fmt, __FUNCTION__, __LINE__, ##args)
#define MSE_LOG(fmt, args...)    printk(MSE_TAG fmt, ##args)
#define MSE_VER(fmt, args...)   ((void)0)


/*----------------------------------------------------------------------------*/
static struct data_resolution lsm303m_data_resolution[] = {
 /*8 combination by {FULL_RES,RANGE}*/
    {{ 3, 9}, 256},   /*+/-2g  in 10-bit resolution:  3.9 mg/LSB*/
    {{ 7, 8}, 128},   /*+/-4g  in 10-bit resolution:  7.8 mg/LSB*/
    {{15, 6},  64},   /*+/-8g  in 10-bit resolution: 15.6 mg/LSB*/
    {{31, 2},  32},   /*+/-16g in 10-bit resolution: 31.2 mg/LSB*/
    {{ 3, 9}, 256},   /*+/-2g  in 10-bit resolution:  3.9 mg/LSB (full-resolution)*/
    {{ 3, 9}, 256},   /*+/-4g  in 11-bit resolution:  3.9 mg/LSB (full-resolution)*/
    {{ 3, 9}, 256},   /*+/-8g  in 12-bit resolution:  3.9 mg/LSB (full-resolution)*/
    {{ 3, 9}, 256},   /*+/-16g in 13-bit resolution:  3.9 mg/LSB (full-resolution)*/            
};
/*----------------------------------------------------------------------------*/
static struct data_resolution lsm303m_offset_resolution = {{15, 6}, 64};

/*--------------------ADXL power control function----------------------------------*/
static void lsm303m_power(struct mag_hw *hw, unsigned int on) 
{
	static unsigned int power_on = 0;

	if(hw->power_id != POWER_NONE_MACRO)		// have externel LDO
	{        
		MSE_LOG("power %s\n", on ? "on" : "off");
		if(power_on == on)	// power status not change
		{
			MSE_LOG("ignore power control: %d\n", on);
		}
		else if(on)	// power on
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "LSM303M"))
			{
				MSE_ERR("power on fails!!\n");
			}
		}
		else	// power off
		{
			if (!hwPowerDown(hw->power_id, "LSM303M"))
			{
				MSE_ERR("power off fail!!\n");
			}			  
		}
	}
	power_on = on;    
}



/*----------------------------------------- OK -----------------------------------*/
static int lsm303m_CheckDeviceID(struct i2c_client *client)
{
	//MSE_LOG("++++++++++++++++++++++LSM303M_CheckDeviceID!");

	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);    
	databuf[0] = LSM303M_REG_DEVID;    

	res = i2c_master_send(client, databuf, 0x1);
	if(res <= 0)
	{
		goto exit_LSM303M_CheckDeviceID;
	}
	
	udelay(500);

	databuf[0] = 0x0;        
	res = i2c_master_recv(client, databuf, 0x01);
	if(res <= 0)
	{
		goto exit_LSM303M_CheckDeviceID;
	}
	

	if(databuf[0]!=LSM303M_FIXED_DEVID)
	{
		return LSM303M_ERR_IDENTIFICATION;
	}

	exit_LSM303M_CheckDeviceID:
	if (res <= 0)
	{
		return LSM303M_ERR_I2C;
	}
	
	return LSM303M_SUCCESS;
}



/*---------------------------------------  YES  -------------------------------------*/
static int lsm303m_SetPowerMode(struct i2c_client *client, bool enable)
{	
	u8 databuf[2];    
	int res = 0;
	u8 addr = LSM303M_REG_POWER_CTL;
	struct lsm303m_i2c_data *obj = i2c_get_clientdata(client);
	
	MSE_FUN();
	if(enable == sensor_power)
	{
		MSE_LOG("Sensor power status is newest!\n");
		return LSM303M_SUCCESS;
	}

	
	if(enable == TRUE)
	{
		databuf[0] = LSM303M_MAG_POWER_ON;
	}
	else
	{
		databuf[0] = LSM303M_MAG_POWER_OFF;
	}
	databuf[1] = databuf[0];
	databuf[0] = LSM303M_REG_POWER_CTL;
	

	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		MSE_LOG("set power mode failed!\n");
		return LSM303M_ERR_I2C;
	}
	else if(atomic_read(&obj->trace) & ADX_TRC_INFO)
	{
		MSE_LOG("set power mode ok %d!\n", databuf[1]);
	}

	sensor_power = enable;
	return 0;   
}


/*-----------------------------------   ok  -----------------------------------------*/
static int lsm303m_SetBWRate(struct i2c_client *client, u8 bwrate)
{
	u8 databuf[10];    
	int res = 0;

	MSE_FUN();

	memset(databuf, 0, sizeof(u8)*10); 

	bwrate = (ODR_MAG_MASK & bwrate);

	
	databuf[0] = LSM303M_REG_BW_RATE;    
	databuf[1] = bwrate;

	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		return LSM303M_ERR_I2C;
	}
	
	return LSM303M_SUCCESS;    
}


/*----------------------------------  done  ------------------------------------------*/
static int lsm303m_SetDataResolution(struct lsm303m_i2c_data *obj, u8 new_fs_range)
{
	MSE_FUN();

	int err;
	u8  dat, reso;

	switch (new_fs_range) {
	case LSM303M_MAG_FS_2G:
		obj->reso.sensitivity = SENSITIVITY_MAG_2G;
		break;
	case LSM303M_MAG_FS_4G:
		obj->reso.sensitivity = SENSITIVITY_MAG_4G;
		break;
	case LSM303M_MAG_FS_8G:
		obj->reso.sensitivity = SENSITIVITY_MAG_8G;
		break;
	case LSM303M_MAG_FS_12G:
		obj->reso.sensitivity = SENSITIVITY_MAG_12G;
		break;
	default:
		obj->reso.sensitivity = SENSITIVITY_MAG_4G;
		MSE_LOG("invalid magnetometer fs range requested: %u\n", new_fs_range);
		return -1;
	}

	return 0;

	
}


/*---------------------------------  ok -------------------------------------------*/
static int lsm303m_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	struct lsm303m_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[10];    
	int res = 0;

	MSE_FUN();

	memset(databuf, 0, sizeof(u8)*10); 
	
	dataformat = ((LSM303M_MAG_FS_MASK & dataformat));

	   
	databuf[0] = LSM303M_REG_DATA_FORMAT;    
	databuf[1] = dataformat;

	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		return LSM303M_ERR_I2C;
	}
	
	return lsm303m_SetDataResolution(obj, dataformat);  

}





/*----------------------------------------------------------------------------*/
static int lsm303m_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
{
	u8 databuf[10];    

	memset(databuf, 0, sizeof(u8)*10);

	if((NULL == buf)||(bufsize<=30))
	{
		return -1;
	}
	
	if(NULL == client)
	{
		*buf = 0;
		return -2;
	}

	sprintf(buf, "LSM303M Chip");
	return 0;
}


static int lsm303m_init_client(struct i2c_client *client, int reset_cali)
{
	struct lsm303m_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	MSE_FUN();

	// 1 check ID  ok
	res = lsm303m_CheckDeviceID(client); 
	if(res != LSM303M_SUCCESS)
	{
	    MSE_ERR("Check ID error\n");
		return res;
	}	

	// 2 POWER MODE  no
	res = lsm303m_SetPowerMode(client, false);
	if(res != LSM303M_SUCCESS)
	{
	    MSE_ERR("set power error\n");
		return res;
	}
	
	// 3 RATE  YES
	res = lsm303m_SetBWRate(client, LSM303M_MAG_ODR50);
	if(res != LSM303M_SUCCESS ) //0x2C->BW=100Hz
	{
	    MSE_ERR("set power error\n");
		return res;
	}

	// 4 RANGE  ok
	res = lsm303m_SetDataFormat(client, LSM303M_MAG_FS_4G);
	if(res != LSM303M_SUCCESS) //0x2C->BW=100Hz
	{
	    MSE_ERR("set data format error\n");
		return res;
	}

	

#ifdef CONFIG_LSM303M_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));  
#endif

	return LSM303M_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int lsm303m_ReadData(struct i2c_client *client, s32 data[LSM303M_AXES_NUM])
{
	struct lsm303m_i2c_data *priv = i2c_get_clientdata(client);        
	u8 addr = LSM303M_REG_DATAX0 | I2C_AUTO_INCREMENT;
	u8 buf[LSM303M_DATA_LEN] = {0};
	int err = 0;

	if(NULL == client)
	{
		err = -EINVAL;
	}
	else if((err = hwmsen_read_block(client, addr, buf, 0x06)))
	{
		MSE_ERR("error: %d\n", err);
	}
	else
	{		
		data[LSM303M_AXIS_X] = ((s32)( (s16)((buf[LSM303M_AXIS_X*2+1] << 8) | (buf[LSM303M_AXIS_X*2]))));
		data[LSM303M_AXIS_Y] = ((s32)( (s16)((buf[LSM303M_AXIS_Y*2+1] << 8) | (buf[LSM303M_AXIS_Y*2]))));
		data[LSM303M_AXIS_Z] = ((s32)( (s16)((buf[LSM303M_AXIS_Z*2+1] << 8) | (buf[LSM303M_AXIS_Z*2]))));

		//MSE_LOG("lsm303m_ReadData [%08X %08X %08X] => [%5d %5d %5d]\n", data[LSM303M_AXIS_X], data[LSM303M_AXIS_Y], data[LSM303M_AXIS_Z],
		//                               data[LSM303M_AXIS_X], data[LSM303M_AXIS_Y], data[LSM303M_AXIS_Z]);
		if(atomic_read(&priv->trace) & ADX_TRC_RAWDATA)
		{
			MSE_LOG("[%08X %08X %08X] => [%5d %5d %5d]\n", data[LSM303M_AXIS_X], data[LSM303M_AXIS_Y], data[LSM303M_AXIS_Z],
		                               data[LSM303M_AXIS_X], data[LSM303M_AXIS_Y], data[LSM303M_AXIS_Z]);
		}

	}
	return err;
}


/*----------------------------------------------------------------------------*/
static int lsm303m_ReadRawData(struct i2c_client *client, char *buf)
{
	struct lsm303m_i2c_data *obj = (struct lsm303m_i2c_data*)i2c_get_clientdata(client);
	int res = 0;

	if (!buf || !client)
	{
		return EINVAL;
	}
	
	if((res = lsm303m_ReadData(client, obj->data)))
	{        
		MSE_ERR("I2C error: ret value=%d", res);
		return EIO;
	}
	else
	{
		sprintf(buf, "%04x %04x %04x", obj->data[LSM303M_AXIS_X], 
			obj->data[LSM303M_AXIS_Y], obj->data[LSM303M_AXIS_Z]);
	
	}
	
	return 0;
}


/*----------------------------------------------------------------------------*/
static int lsm303m_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
	int mag[LSM303M_DATA_LEN];
	int res = 0;	
	struct lsm303m_i2c_data *obj = obj_i2c_data; //(struct lsm303m_i2c_data*)i2c_get_clientdata(client);
	client = obj->client;
	//u8 databuf[20];
	
	//memset(databuf, 0, sizeof(u8)*10);

	if(NULL == buf)
	{
		return -1;
	}
	if(NULL == client)
	{
		*buf = 0;
		return -2;
	}

	if(sensor_power == FALSE)
	{
		res = lsm303m_SetPowerMode(client, true);
		if(res)
		{
			MSE_ERR("Power on lsm303m error %d!\n", res);
		}
		msleep(20);
	}

	if((res = lsm303m_ReadData(client, obj->data)))
	{        
		MSE_ERR("I2C error: ret value=%d", res);
		return -3;
	}
	else
	{
		
		/*remap coordinate*/
		
		mag[obj->cvt.map[LSM303M_AXIS_X]] = obj->cvt.sign[LSM303M_AXIS_X]*obj->data[LSM303M_AXIS_X] * obj->reso.sensitivity / 1000;
		mag[obj->cvt.map[LSM303M_AXIS_Y]] = obj->cvt.sign[LSM303M_AXIS_Y]*obj->data[LSM303M_AXIS_Y] * obj->reso.sensitivity / 1000;
		mag[obj->cvt.map[LSM303M_AXIS_Z]] = obj->cvt.sign[LSM303M_AXIS_Z]*obj->data[LSM303M_AXIS_Z] * obj->reso.sensitivity / 1000;
		
		/**
		mag[obj->cvt.map[LSM303M_AXIS_X]] = obj->cvt.sign[LSM303M_AXIS_X]*obj->data[LSM303M_AXIS_X];
		mag[obj->cvt.map[LSM303M_AXIS_Y]] = obj->cvt.sign[LSM303M_AXIS_Y]*obj->data[LSM303M_AXIS_Y];
		mag[obj->cvt.map[LSM303M_AXIS_Z]] = obj->cvt.sign[LSM303M_AXIS_Z]*obj->data[LSM303M_AXIS_Z];
		**/

		//MSE_LOG("Mapped msensor data: %d, %d, %d!\n", mag[LSM303M_AXIS_X], mag[LSM303M_AXIS_Y], mag[LSM303M_AXIS_Z]);

		sprintf(buf, "%04x %04x %04x", mag[LSM303M_AXIS_X], mag[LSM303M_AXIS_Y], mag[LSM303M_AXIS_Z]);
		if(atomic_read(&obj->trace) & ADX_TRC_IOCTL)
		{
			MSE_LOG("msensor data: %s!\n", buf);
		}
	}

	
	//MSE_LOG("io read  msensor data: %d, %d, %d!\n", mag[LSM303M_AXIS_X],mag[LSM303M_AXIS_Y],mag[LSM303M_AXIS_Z]);
	return 0;
}



/*----------------------------------------------------------------------------*/
static int lsm303m_InitSelfTest(struct i2c_client *client)
{
	
	
	return LSM303M_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int lsm303m_JudgeTestResult(struct i2c_client *client, s32 prv[LSM303M_AXES_NUM], s32 nxt[LSM303M_AXES_NUM])
{

    int res = 0;
    return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm303m_i2c_client;
	char strbuf[LSM303M_BUFSIZE];
	if(NULL == client)
	{
		MSE_ERR("i2c client is null!!\n");
		return 0;
	}
	
	lsm303m_ReadChipInfo(client, strbuf, LSM303M_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);        
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm303m_i2c_client;
	char strbuf[LSM303M_BUFSIZE];
	
	if(NULL == client)
	{
		MSE_ERR("i2c client is null!!\n");
		return 0;
	}
	lsm303m_ReadSensorData(client, strbuf, LSM303M_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);            
}


/*---------------------------------  NO -------------------------------------------*/
static ssize_t show_reginfo_value(struct device_driver *ddri, char *buffer)
{

	struct i2c_client *client = lsm303m_i2c_client;
		
			struct lsm303m_i2c_data *priv = i2c_get_clientdata(client); 	   
			u8 addr = LSM303M_REG_CTL0 | I2C_AUTO_INCREMENT;
			u8 buf[8] = {0};
			int err = 0;
			ssize_t len = 0;
			
		
			if(NULL == client)
			{
				err = -EINVAL;
			}
			else if((err = hwmsen_read_block(client, addr, buf, 0x08)))
			{
				MSE_ERR("error: %d\n", err);
			}
		
			len += snprintf(buffer+len, PAGE_SIZE, "0x%04X , \t 0x%04X , \t 0x%04X, \t0x%04X ,	 \n  0x%04X , \t  0x%04X, \t0x%04X,  \t 0x%04X ,  \t  \n ", 
							buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]); 
		
	
			return len;
}
/*--------------------------------  NO --------------------------------------------*/
static ssize_t store_selftest_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct lsm303m_i2c_data *obj = obj_i2c_data;
	int tmp;

	if(NULL == obj)
	{
		MSE_ERR("i2c data obj is null!!\n");
		return 0;
	}
	
	
	if(1 == sscanf(buf, "%d", &tmp))
	{        
		if(atomic_read(&obj->selftest) && !tmp)
		{
			/*enable -> disable*/
			lsm303m_init_client(obj->client, 0);
		}
		else if(!atomic_read(&obj->selftest) && tmp)
		{
			/*disable -> enable*/
			lsm303m_InitSelfTest(obj->client);            
		}
		
		MSE_LOG("selftest: %d => %d\n", atomic_read(&obj->selftest), tmp);
		atomic_set(&obj->selftest, tmp); 
	}
	else
	{ 
		MSE_ERR("invalid content: '%s', length = %d\n", buf, count);   
	}
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct lsm303m_i2c_data *obj = obj_i2c_data;
	if (obj == NULL)
	{
		MSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}
	
	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct lsm303m_i2c_data *obj = obj_i2c_data;
	int trace;
	if (obj == NULL)
	{
		MSE_ERR("i2c_data obj is null!!\n");
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
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;    
	struct lsm303m_i2c_data *obj = obj_i2c_data;
	if (obj == NULL)
	{
		MSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}	
	
	if(obj->hw)
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d %d (%d %d)\n", 
	            obj->hw->i2c_num, obj->hw->direction, obj->hw->power_id, obj->hw->power_vol);   
	}
	else
	{
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");
	}
	return len;    
}

static ssize_t show_power_status_value(struct device_driver *ddri, char *buf)
{
	int relv = 0;
	if(sensor_power)
		relv = snprintf(buf, PAGE_SIZE, "1\n"); 
	else
		relv = snprintf(buf, PAGE_SIZE, "0\n"); 

	return relv;
}


static ssize_t store_power_status_value(struct device_driver *ddri, const char *buf, size_t count)
{
	int mode = 0;
	int res = 0;
	sscanf(buf, "%d", &mode);    
	res = lsm303m_SetPowerMode(lsm303m_i2c_client, mode);
	return count;   
}
  


/*----------------------------------------------------------------------------*/
static ssize_t show_daemon_name(struct device_driver *ddri, char *buf)
{
	char strbuf[LSM303M_BUFSIZE];
	sprintf(strbuf, "lsm303md");
	return sprintf(buf, "%s", strbuf);		
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static ssize_t show_debug_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct lsm303m_i2c_data *obj = i2c_get_clientdata(lsm303m_i2c_client);
	if(NULL == obj)
	{
		MSE_ERR("lsm303m_i2c_data is null!!\n");
		return 0;
	}	
	
	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", lsm303mmid_data.controldata[8]);     
	return res;    
}

static ssize_t store_debug_value(struct device_driver *ddri, const char *buf, size_t count)

{
	struct lsm303m_i2c_data *obj = i2c_get_clientdata(lsm303m_i2c_client);
	int trace;
	if(NULL == obj)
	{
		MSE_ERR("lsm303m_i2c_data is null!!\n");
		return 0;
	}
	
	if(1 == sscanf(buf, "0x%x", &trace))
	{
		//atomic_set(&obj->trace, trace);
		lsm303mmid_data.controldata[8] = trace;
	}
	else 
	{
		MSE_ERR("invalid content: '%s', length = %d\n", buf, count);
	}
	
	return count;	
}

/*----------------------------------------------------------------------------*/
static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = lsm303m_i2c_client;  
	struct lsm303m_i2c_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		data->hw->direction,atomic_read(&data->layout),	data->cvt.sign[0], data->cvt.sign[1],
		data->cvt.sign[2],data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);            
}

/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct i2c_client *client = lsm303m_i2c_client;  
	struct lsm303m_i2c_data *data = i2c_get_clientdata(client);
	int layout = 0;

	if(1 == sscanf(buf, "%d", &layout))
	{
		atomic_set(&data->layout, layout);
		if(!hwmsen_get_convert(layout, &data->cvt))
		{
			MSE_ERR("HWMSEN_GET_CONVERT function ok!\r\n");
			data->hw->direction = layout;
		}
		else if(!hwmsen_get_convert(data->hw->direction, &data->cvt))
		{
			MSE_ERR("invalid layout: %d, restore to %d, no changed \n", layout, data->hw->direction);
			
		}
		else
		{
			MSE_ERR("invalid layout: (%d, %d)\n", layout, data->hw->direction);
			hwmsen_get_convert(0, &data->cvt);
			data->hw->direction = 0;
		}
	}
	else
	{
		MSE_ERR("invalid format = '%s'\n", buf);
	}
	
	return count;            
}



/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(daemon,      		  S_IRUGO, show_daemon_name, 		 NULL);
static DRIVER_ATTR(chipinfo,             S_IRUGO, show_chipinfo_value,      NULL);
static DRIVER_ATTR(sensordata,           S_IRUGO, show_sensordata_value,    NULL);
static DRIVER_ATTR(reginfo,              S_IRUGO, show_reginfo_value,       NULL);
static DRIVER_ATTR(debugon,              S_IWUSR | S_IRUGO, show_debug_value,       store_debug_value);
static DRIVER_ATTR(trace,      S_IWUSR | S_IRUGO, show_trace_value,         store_trace_value);
static DRIVER_ATTR(status,               S_IRUGO, show_status_value,        NULL);
static DRIVER_ATTR(powerstatus,    S_IWUSR | S_IRUGO, show_power_status_value,        store_power_status_value);
static DRIVER_ATTR(layout,      S_IRUGO | S_IWUSR,     show_layout_value, store_layout_value );


/*----------------------------------------------------------------------------*/
static struct driver_attribute *lsm303m_attr_list[] = {
	&driver_attr_daemon,
	&driver_attr_chipinfo,     /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/
	&driver_attr_reginfo,         /*self test demo*/
	&driver_attr_debugon,		  /*self test demo*/
	&driver_attr_trace,        /*trace log*/
	&driver_attr_status,
	&driver_attr_powerstatus, 
	&driver_attr_layout,  
};


/*----------------------------------------------------------------------------*/
static int lsm303m_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(lsm303m_attr_list)/sizeof(lsm303m_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, lsm303m_attr_list[idx])))
		{            
			MSE_ERR("driver_create_file (%s) = %d\n", lsm303m_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int lsm303m_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(lsm303m_attr_list)/sizeof(lsm303m_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, lsm303m_attr_list[idx]);
	}
	

	return err;
}


/*----------------------------------------------------------------------------*/
static int lsm303m_ReadCaliData(char *buf, int bufsize)
{
	if((!buf)||(bufsize<=80))
	{
		return -1;
	}

	MSE_FUN(f);
	
	read_lock(&lsm303mmid_data.datalock);
	sprintf(buf, "%d %d %d %d %d %d %d", lsm303mmid_data.nmx, lsm303mmid_data.nmy, 
		lsm303mmid_data.nmz,lsm303mmid_data.nax,lsm303mmid_data.nay,lsm303mmid_data.naz,lsm303mmid_data.mag_status);
	read_unlock(&lsm303mmid_data.datalock);
	return 0;
}


/*----------------------------------------------------------------------------*/
static int lsm303m_ReadPostureData(char *buf, int bufsize)
{
	MSE_FUN(f);

	if((!buf)||(bufsize<=80))
	{
		return -1;
	}
	
	read_lock(&lsm303mmid_data.datalock);
	sprintf(buf, "%d %d %d %d", lsm303mmid_data.yaw, lsm303mmid_data.pitch,
		lsm303mmid_data.roll, lsm303mmid_data.mag_status);
	read_unlock(&lsm303mmid_data.datalock);
	return 0;
}



/*----------------------------------------------------------------------------*/
int lsm303m_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value, sample_delay, status, delay_time;
	hwm_sensor_data* msensor_data;
	struct lsm303m_i2c_data *priv = (struct lsm303m_i2c_data*)self;
	char buff[LSM303M_BUFSIZE];
	

	//MSE_FUN(f);
	
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
				MSE_LOG("Set delay parameter %d!\n", value);
				if(value <= 5)
				{
					sample_delay = LSM303M_MAG_ODR50;
				}
				else if(value <= 10)
				{
					sample_delay = LSM303M_MAG_ODR50;
				}
				else
				{
					sample_delay = LSM303M_MAG_ODR50;
				}
				
				err = lsm303m_SetBWRate(priv->client, sample_delay);
				if(err != LSM303M_SUCCESS ) //0x2C->BW=100Hz
				{
					MSE_ERR("Set delay parameter error!\n");
				}
			
				value = *(int *)buff_in;
				if(value <= 20)
				{
					value = 20;
				}
				
				lsm303mmid_data.controldata[0] = value;  // Loop Delay
			}	
			break;

		case SENSOR_ENABLE:
			//MSE_LOG("***lsm303m SENSOR_ENABLE****");
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				MSE_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				read_lock(&lsm303mmid_data.ctrllock);
				if(value == 1)
				{
					MSE_LOG("lsm303m SENSOR_ENABLE");
					lsm303m_SetPowerMode( priv->client, 1);
					lsm303mmid_data.controldata[7] |= SENSOR_MAGNETIC;
					atomic_set(&m_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					lsm303m_SetPowerMode( priv->client, 0);
					lsm303mmid_data.controldata[7] &= ~SENSOR_MAGNETIC;
					atomic_set(&m_flag, 0);
					if(atomic_read(&o_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}	
				}
				wake_up(&open_wq);
				read_unlock(&lsm303mmid_data.ctrllock);
				// TODO: turn device into standby or normal mode
			}
			break;

		case SENSOR_GET_DATA:
			//MSE_LOG("++++++++++++++++++++++++MSENSOR_GET_DATA");
			//MSE_LOG("***lsm303m SENSOR_GET_DATA****");
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				MSE_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				
				msensor_data = (hwm_sensor_data *)buff_out;
				read_lock(&lsm303mmid_data.datalock);
				msensor_data->values[0] = lsm303mmid_data.nmx;
				msensor_data->values[1] = lsm303mmid_data.nmy;
				msensor_data->values[2] = lsm303mmid_data.nmz;
				msensor_data->status = lsm303mmid_data.mag_status;
			
				read_unlock(&lsm303mmid_data.datalock); 


				msensor_data->values[0] = msensor_data->values[0] / 10;
				msensor_data->values[1] = msensor_data->values[1] / 10;
				msensor_data->values[2] = msensor_data->values[2] / 10;
				msensor_data->value_divide = 1;
	
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

				msensor_data->status = SENSOR_STATUS_ACCURACY_HIGH;

				//MSE_LOG("get msensor data: %d, %d, %d, %d!\n", msensor_data->values[0],msensor_data->values[1], msensor_data->values[2], msensor_data->status);
				
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
int lsm303m_orientation_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value, sample_delay, status=0;
	hwm_sensor_data* osensor_data=NULL;
	struct lsm303m_i2c_data *priv = (struct lsm303m_i2c_data*)self;

		
	MSE_FUN(f);
	switch (command)
	{
		case SENSOR_DELAY:

			MSE_LOG("***orientation SENSOR_DELAY****");
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
				
				
				lsm303mmid_data.controldata[0] = sample_delay;  // Loop Delay
			}	
			break;

		case SENSOR_ENABLE:
			MSE_LOG("***orientation SENSOR_ENABLE****");
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				MSE_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				read_lock(&lsm303mmid_data.ctrllock);
				if(value == 1)
				{
					lsm303m_SetPowerMode( priv->client, 1);
					lsm303mmid_data.controldata[7] |= SENSOR_ORIENTATION;
					atomic_set(&o_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					lsm303m_SetPowerMode( priv->client, 0);
					lsm303mmid_data.controldata[7] &= ~SENSOR_ORIENTATION;
					atomic_set(&o_flag, 0);
					if(atomic_read(&m_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}
				}
				wake_up(&open_wq);
				read_unlock(&lsm303mmid_data.ctrllock);
				// Do nothing
			}
			break;

		case SENSOR_GET_DATA:
			//MSE_LOG("+++++++++++MSENSOR_GET_ORIENTATION_DATA");
			MSE_LOG("************OSENSOR_GET_DATA***********\r\n");
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				MSE_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				osensor_data = (hwm_sensor_data *)buff_out;
				read_lock(&lsm303mmid_data.datalock);
				osensor_data->values[0] = lsm303mmid_data.yaw;
				osensor_data->values[1] = lsm303mmid_data.pitch;
				osensor_data->values[2] = lsm303mmid_data.roll;
				status = lsm303mmid_data.mag_status;
				read_unlock(&lsm303mmid_data.datalock); 
				
				osensor_data->value_divide = 1;		

				
				//MSE_LOG(" get osensor data: %d, %d, %d, %d!\n", osensor_data->values[0],osensor_data->values[1], osensor_data->values[2], osensor_data->status);
			}

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
			break;
		default:
			MSE_ERR("msensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}



/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int lsm303m_open(struct inode *inode, struct file *file)
{
	file->private_data = lsm303m_i2c_client;
	atomic_inc(&dev_open_count);

	if(file->private_data == NULL)
	{
		MSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}


/*----------------------------------------------------------------------------*/
static int lsm303m_release(struct inode *inode, struct file *file)
{
	struct lsm303m_i2c_data *obj = i2c_get_clientdata(lsm303m_i2c_client);
	atomic_dec(&dev_open_count);
	file->private_data = NULL;
	if(atomic_read(&obj->trace) & ADX_TRC_INFO)
	{
		MSE_LOG("Release device node:ami304\n");
	}	
	return 0;
}


static int lsm303m_GetOpenStatus(void)
{
	wait_event_interruptible(open_wq, (atomic_read(&open_flag) != 0));
	return atomic_read(&open_flag);
}



/*----------------------------------------------------------------------------*/
//static int  lsm303m_ioctl(struct inode *inode, struct file *file, unsigned int cmd,unsigned long arg)//modified here
static long  lsm303m_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
    void __user *argp = (void __user *)arg;
	int valuebuf[4];
	int YPRdata[4];
	int controlbuf[10];
	char strbuf[LSM303M_BUFSIZE];
	void __user *data;
	long retval=0;
	int mode=0;
	hwm_sensor_data* osensor_data;
	uint32_t enable;
	char buff[512];	
	int status; 				/* for OPEN/CLOSE_STATUS */
	short sensor_status;		/* for Orientation and Msensor status */
	struct lsm303m_i2c_data *priv = obj_i2c_data;
	
//	MSE_FUN(f);

//	MSE_FUN(f);


	switch (cmd)
	{
		case MSENSOR_IOCTL_INIT:
			MSE_LOG("===========IOCTL_INIT=======\r\n");
			read_lock(&lsm303m_data.lock);
			mode =  lsm303m_data.mode;
			read_unlock(&lsm303m_data.lock);
			lsm303m_init_client(lsm303m_i2c_client, 0);         
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
			MSE_LOG("===========GET__OPEN_STATU=======\r\n");
			status =  lsm303m_GetOpenStatus();			
			if(copy_to_user(argp, &status, sizeof(status)))
			{
				MSE_LOG("copy_to_user failed.");
				return -EFAULT;
			}
			MSE_LOG("===========GET__OPEN_STATU  DONE=======\r\n");
			break; 

		case MSENSOR_IOCTL_SET_POSTURE:
			//MSE_LOG("===========SET_POSTURE=======\r\n");
			data = (void __user *) arg;
			if(data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			   
			if(copy_from_user(&valuebuf, data, sizeof(valuebuf)))
			{
				retval = -EFAULT;
				goto err_out;
			}
			
			write_lock(&lsm303mmid_data.datalock);
			 lsm303mmid_data.yaw   = valuebuf[0];
			 lsm303mmid_data.pitch = valuebuf[1];
			 lsm303mmid_data.roll  = valuebuf[2];
			 lsm303mmid_data.mag_status = valuebuf[3];
			write_unlock(&lsm303mmid_data.datalock);   

			
			//MSE_LOG("SET_POSTURE osensor data: %d, %d, %d!\n", lsm303mmid_data.yaw ,lsm303mmid_data.pitch ,lsm303mmid_data.pitch);
			break;
			

		case  MSENSOR_IOCTL_SET_CALIDATA:
			//MSE_LOG("===========IOCTL_SET_YPRdata=======\r\n");
			data = (void __user *) arg;
			if (data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			if(copy_from_user(&YPRdata, data, sizeof(YPRdata)))
			{
				retval = -EFAULT;
				goto err_out;
			}

			write_lock(&lsm303mmid_data.datalock);
			 lsm303mmid_data.nmx   = YPRdata[0];
			 lsm303mmid_data.nmy = YPRdata[1];
			 lsm303mmid_data.nmz  = YPRdata[2];
			 lsm303mmid_data.mag_status = YPRdata[3];
			write_unlock(&lsm303mmid_data.datalock);    
			//MSE_LOG("IOCTL_SET_YPRdata msensor data: %d, %d, %d!\n", lsm303mmid_data.nmx ,lsm303mmid_data.nmy ,lsm303mmid_data.nmz);
			break;                                

		case MSENSOR_IOCTL_READ_CHIPINFO:
			data = (void __user *) arg;
			if(data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			
			 lsm303m_ReadChipInfo(lsm303m_i2c_client, strbuf, LSM303M_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				retval = -EFAULT;
				goto err_out;
			}                
			break;

		case MSENSOR_IOCTL_SENSOR_ENABLE:
			//MSE_LOG("===========IOCTL_SENSOR_ENABLE=======\r\n");
			
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
			    printk( "MSENSOR_IOCTL_SENSOR_ENABLE enable=%d!\r\n",enable);
				read_lock(&lsm303mmid_data.ctrllock);
				if(enable == 1)
				{
					lsm303m_SetPowerMode( priv->client, 1);
					lsm303mmid_data.controldata[7] |= SENSOR_ORIENTATION;
					atomic_set(&o_flag, 1);
					atomic_set(&open_flag, 1);
				}
				else
				{
					lsm303m_SetPowerMode( priv->client, 0);
					lsm303mmid_data.controldata[7] &= ~SENSOR_ORIENTATION;
					atomic_set(&o_flag, 0);
					if(atomic_read(&m_flag) == 0)
					{
						atomic_set(&open_flag, 0);
					}		
				}
				wake_up(&open_wq);
				read_unlock(&lsm303mmid_data.ctrllock);
				
			}
			
			break;
			
		case MSENSOR_IOCTL_READ_SENSORDATA:
			//MSE_LOG("===========IOCTL_READ_SENSORDATA=======\r\n");
			data = (void __user *) arg;
			if(data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;    
			}
			 lsm303m_ReadSensorData(lsm303m_i2c_client, strbuf, LSM303M_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				retval = -EFAULT;
				goto err_out;
			} 

			break;

		case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
			
			//MSE_LOG("===========IOCTL_READ_FACTORY_SENSORDATA=======\r\n");
			data = (void __user *) arg;
			if (data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			
			osensor_data = (hwm_sensor_data *)buff;

			read_lock(&lsm303mmid_data.datalock);
			osensor_data->values[0] =  lsm303mmid_data.yaw;
			osensor_data->values[1] =  lsm303mmid_data.pitch;
			osensor_data->values[2] =  lsm303mmid_data.roll;
			//status =  lsm303mmid_data.mag_status;
			read_unlock(&lsm303mmid_data.datalock); 
						
			osensor_data->value_divide = 1;	

			switch (lsm303mmid_data.mag_status)
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
     
			
            sprintf(buff, "%x %x %x %x %x", osensor_data->values[0], osensor_data->values[1],
				osensor_data->values[2],osensor_data->status,osensor_data->value_divide);
			if(copy_to_user(data, buff, strlen(buff)+1))
			{
				return -EFAULT;
			} 
			
			break;                

		case MSENSOR_IOCTL_READ_POSTUREDATA:
			//MSE_LOG("===========IOCTL_READ_READ_POSTUREDATA=======\r\n");
			data = (void __user *) arg;
			if(data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}
			
			lsm303m_ReadPostureData(strbuf, LSM303M_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				retval = -EFAULT;
				goto err_out;
			}                
			break;            

		case MSENSOR_IOCTL_READ_CALIDATA:
			//MSE_LOG("===========IOCTL_READ_READ_CALIDATA=======\r\n");
			data = (void __user *) arg;
			if(data == NULL)
			{
				break;    
			}
			 lsm303m_ReadCaliData(strbuf, LSM303M_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				retval = -EFAULT;
				goto err_out;
			}                
			break;

		case MSENSOR_IOCTL_READ_CONTROL:
			read_lock(&lsm303mmid_data.ctrllock);
			memcpy(controlbuf, &lsm303mmid_data.controldata[0], sizeof(controlbuf));
			read_unlock(&lsm303mmid_data.ctrllock);            
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
			//MSE_LOG("===========IOCTL_SET_CONTROL=======\r\n");
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
			write_lock(&lsm303mmid_data.ctrllock);
			memcpy(&lsm303mmid_data.controldata[0], controlbuf, sizeof(controlbuf));
			write_unlock(&lsm303mmid_data.ctrllock);        
			break;

		case MSENSOR_IOCTL_SET_MODE:
		              
			break;
		    
		default:
			MSE_ERR("%s not supported = 0x%04x", __FUNCTION__, cmd);
			retval = -ENOIOCTLCMD;
			break;
		}

	err_out:
		//MSE_LOG("===========liangruo lsm303m ioctl done err = %d, =======\r\n", retval);
	return retval;    
}



/*----------------------------------------------------------------------------*/
static struct file_operations lsm303m_fops = {
//	.owner = THIS_MODULE,
	.open = lsm303m_open,
	.release = lsm303m_release,
	.unlocked_ioctl = lsm303m_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice lsm303m_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msensor",
	.fops = &lsm303m_fops,
};


/*----------------------------------------------------------------------------*/
//#ifndef CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int lsm303m_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct lsm303m_i2c_data *obj = i2c_get_clientdata(client);    
	int err = 0;
	MSE_FUN();    

	if(msg.event == PM_EVENT_SUSPEND)
	{   
		if(obj == NULL)
		{
			MSE_ERR("null pointer!!\n");
			return -EINVAL;
		}
		atomic_set(&obj->suspend, 1);
		/**
		if((err = lsm303m_SetPowerMode(obj->client, false)))
		{
			MSE_ERR("write power control fail!!\n");
			return err;
		}        
		***/
		lsm303m_power(obj->hw, 0);
		MSE_LOG("lsm303m_suspend ok\n");
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int lsm303m_resume(struct i2c_client *client)
{
	struct lsm303m_i2c_data *obj = i2c_get_clientdata(client);        
	int err;
	MSE_FUN();

	if(obj == NULL)
	{
		MSE_ERR("null pointer!!\n");
		return -EINVAL;
	}

	lsm303m_power(obj->hw, 1);
/***
	if((err = lsm303m_SetPowerMode(obj->client, true)))
	{
		MSE_ERR("write power control fail!!\n");
		return err;
	} 

	***/
	atomic_set(&obj->suspend, 0);
	MSE_LOG("lsm303m_resume ok\n");

	return 0;
}
/*----------------------------------------------------------------------------*/
//#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void lsm303m_early_suspend(struct early_suspend *h) 
{
	struct lsm303m_i2c_data *obj = container_of(h, struct lsm303m_i2c_data, early_drv);   
	int err;
	MSE_FUN();    

	if(obj == NULL)
	{
		MSE_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1); 
	/***
	if((err = lsm303m_SetPowerMode(obj->client, false)))
	{
		MSE_ERR("write power control fail!!\n");
		return;
	}
	**/

	sensor_power = false;
	
	lsm303m_power(obj->hw, 0);
}
/*----------------------------------------------------------------------------*/
static void lsm303m_late_resume(struct early_suspend *h)
{
	struct lsm303m_i2c_data *obj = container_of(h, struct lsm303m_i2c_data, early_drv);         
	int err;
	MSE_FUN();

	if(obj == NULL)
	{
		MSE_ERR("null pointer!!\n");
		return;
	}

	lsm303m_power(obj->hw, 1);
	/***

	if((err = lsm303m_SetPowerMode(obj->client, true)))
	{
		MSE_ERR("write power control fail!!\n");
		return err;
	} 
	***/
	atomic_set(&obj->suspend, 0);    
}



/*----------------------------------------------------------------------------*/
static int lsm303m_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
		MSE_FUN();

		struct i2c_client *new_client;
		struct lsm303m_i2c_data *data;
		int err = 0;
		struct hwmsen_object sobj_m, sobj_o;
	
		MSE_FUN(f);

		client->addr = 0x3C>>1;
	
		if (!(data = kmalloc(sizeof(struct lsm303m_i2c_data), GFP_KERNEL)))
		{
			err = -ENOMEM;
			goto exit;
		}
		memset(data, 0, sizeof(struct lsm303m_i2c_data));
	
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
		
		lsm303m_i2c_client = new_client; 
		obj_i2c_data = data;

		if((err = lsm303m_init_client(new_client, 1)))
		{
			goto exit_init_failed;
		}
	
		/* Register sysfs attribute */
		if((err = lsm303m_create_attr(&lsm303m_msensor_driver.driver)))
		{
			MSE_ERR("create attribute err = %d\n", err);
			goto exit_sysfs_create_group_failed;
		}
	
		
		if((err = misc_register(&lsm303m_device)))
		{
			MSE_ERR("lsm303m_device register failed\n");
			goto exit_misc_device_register_failed;	
		}	 
	
		sobj_m.self = data;
		sobj_m.polling = 1;
		sobj_m.sensor_operate = lsm303m_operate;
		if((err = hwmsen_attach(ID_MAGNETIC, &sobj_m)))
		{
			MSE_ERR("attach fail = %d\n", err);
			goto exit_kfree;
		}
		
		sobj_o.self = data;
		sobj_o.polling = 1;
		sobj_o.sensor_operate = lsm303m_orientation_operate;
		if((err = hwmsen_attach(ID_ORIENTATION, &sobj_o)))
		{
			MSE_ERR("attach fail = %d\n", err);
			goto exit_kfree;
		}
		
#if CONFIG_HAS_EARLYSUSPEND
		data->early_drv.level	 = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
		data->early_drv.suspend  = lsm303m_early_suspend,
		data->early_drv.resume	 = lsm303m_late_resume,	  
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
static int lsm303m_i2c_remove(struct i2c_client *client)
{
	int err = 0;	
	
	if((err = lsm303m_delete_attr(&lsm303m_msensor_driver.driver)))
	{
		MSE_ERR("lsm303m_delete_attr fail: %d\n", err);
	}
	
	if((err = misc_deregister(&lsm303m_device)))
	{
		MSE_ERR("misc_deregister fail: %d\n", err);
	}

	if((err = hwmsen_detach(ID_MAGNETIC)))
	{
		MSE_ERR("hwmsen_detach fail: %d\n", err);
	}
	if((err = hwmsen_detach(ID_ORIENTATION)))
	{
		MSE_ERR("hwmsen_detach fail: %d\n", err);
	}
	    

	lsm303m_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int lsm303m_probe(struct platform_device *pdev) 
{
	struct mag_hw *hw = get_cust_mag_hw();
	
	MSE_FUN();

	lsm303m_power(hw, 1);    
	rwlock_init(&lsm303mmid_data.ctrllock);
	rwlock_init(&lsm303mmid_data.datalock);
	rwlock_init(&lsm303m_data.lock);
	memset(&lsm303mmid_data.controldata[0], 0, sizeof(int)*10);

	atomic_set(&dev_open_count, 0);

	MSE_ERR("i2c add driver \n");

	//lsm303m_force[0] = hw->i2c_num;//modified
	if(i2c_add_driver(&lsm303m_i2c_driver))
	{
		MSE_ERR("add driver error\n");
		return -1;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int lsm303m_remove(struct platform_device *pdev)
{
    struct mag_hw *hw = get_cust_mag_hw();

	MSE_FUN();    
	lsm303m_power(hw, 0);    
	atomic_set(&dev_open_count, 0);  
	i2c_del_driver(&lsm303m_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
#if 0
static struct platform_driver lsm303m_msensor_driver = {
	.probe      = lsm303m_probe,
	.remove     = lsm303m_remove,    
	.driver     = {
		.name  = "msensor",
//		.owner = THIS_MODULE,
	}
};
#endif

#ifdef CONFIG_OF
static const struct of_device_id lsm303m_of_match[] = {
	{ .compatible = "mediatek,msensor", },
	{},
};
#endif

static struct platform_driver lsm303m_msensor_driver =
{
	.probe      = lsm303m_probe,
	.remove     = lsm303m_remove,    
	.driver     = 
	{
		.name = "msensor",
        #ifdef CONFIG_OF
		.of_match_table = lsm303m_of_match,
		#endif
	}
};

/*----------------------------------------------------------------------------*/
static int __init lsm303m_init(void)
{
	MSE_FUN();
	
	struct mag_hw *hw = get_cust_mag_hw();
	MSE_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	
	i2c_register_board_info(hw->i2c_num, &i2c_lsm303m, 1);
	if(platform_driver_register(&lsm303m_msensor_driver))
	{
		MSE_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit lsm303m_exit(void)
{
	MSE_FUN();
	platform_driver_unregister(&lsm303m_msensor_driver);
}

/*----------------------------------------------------------------------------*/
module_init(lsm303m_init);
module_exit(lsm303m_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Ruo Liang");
MODULE_DESCRIPTION("LSM303M MI-Sensor driver without DRDY");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.1");


