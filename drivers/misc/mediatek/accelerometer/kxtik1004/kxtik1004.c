/* KXTIK1004 motion sensor driver
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
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

//#include <mach/mt_devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#define POWER_NONE_MACRO MT65XX_POWER_NONE

#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include "kxtik1004.h"
#include <linux/hwmsen_helper.h>
/*----------------------------------------------------------------------------*/
#define I2C_DRIVERID_KXTIK1004 150
/*----------------------------------------------------------------------------*/
#define DEBUG 1
/*----------------------------------------------------------------------------*/
//#define CONFIG_KXTIK1004_LOWPASS   /*apply low pass filter on output*/       
#define SW_CALIBRATION

/*----------------------------------------------------------------------------*/
#define KXTIK1004_AXIS_X          0
#define KXTIK1004_AXIS_Y          1
#define KXTIK1004_AXIS_Z          2
#define KXTIK1004_AXES_NUM        3
#define KXTIK1004_DATA_LEN        6
#define KXTIK1004_DEV_NAME        "KXTIK1004"
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id kxtik1004_i2c_id[] = {{KXTIK1004_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_kxtik1004={ I2C_BOARD_INFO(KXTIK1004_DEV_NAME, (KXTIK1004_I2C_SLAVE_ADDR>>1))};
/*the adapter id will be available in customization*/
//static unsigned short kxtik1004_force[] = {0x00, KXTIK1004_I2C_SLAVE_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const kxtik1004_forces[] = { kxtik1004_force, NULL };
//static struct i2c_client_address_data kxtik1004_addr_data = { .forces = kxtik1004_forces,};

/*----------------------------------------------------------------------------*/
static int kxtik1004_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int kxtik1004_i2c_remove(struct i2c_client *client);
static int kxtik1004_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
#ifndef USE_EARLY_SUSPEND
static int kxtik1004_suspend(struct i2c_client *client, pm_message_t msg);
static int kxtik1004_resume(struct i2c_client *client);
#endif
/*----------------------------------------------------------------------------*/
typedef enum {
    ADX_TRC_FILTER  = 0x01,
    ADX_TRC_RAWDATA = 0x02,
    ADX_TRC_IOCTL   = 0x04,
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
    s16 raw[C_MAX_FIR_LENGTH][KXTIK1004_AXES_NUM];
    int sum[KXTIK1004_AXES_NUM];
    int num;
    int idx;
};
/*----------------------------------------------------------------------------*/
struct kxtik1004_i2c_data {
    struct i2c_client *client;
    struct acc_hw *hw;
    struct hwmsen_convert   cvt;
    
    /*misc*/
    struct data_resolution *reso;
    atomic_t                trace;
    atomic_t                suspend;
    atomic_t                selftest;
	atomic_t				filter;
    s16                     cali_sw[KXTIK1004_AXES_NUM+1];

    /*data*/
    s8                      offset[KXTIK1004_AXES_NUM+1];  /*+1: for 4-byte alignment*/
    s16                     data[KXTIK1004_AXES_NUM+1];

#if defined(CONFIG_KXTIK1004_LOWPASS)
    atomic_t                firlen;
    atomic_t                fir_en;
    struct data_filter      fir;
#endif 
    /*early suspend*/
#ifdef USE_EARLY_SUSPEND
    struct early_suspend    early_drv;
#endif     
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver kxtik1004_i2c_driver = {
    .driver = {
//        .owner          = THIS_MODULE,
        .name           = KXTIK1004_DEV_NAME,
    },
	.probe      		= kxtik1004_i2c_probe,
	.remove    			= kxtik1004_i2c_remove,
	.detect				= kxtik1004_i2c_detect,
#if !defined(USE_EARLY_SUSPEND)    
    .suspend            = kxtik1004_suspend,
    .resume             = kxtik1004_resume,
#endif
	.id_table = kxtik1004_i2c_id,
//	.address_data = &kxtik1004_addr_data,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *kxtik1004_i2c_client = NULL;
static struct platform_driver kxtik1004_gsensor_driver;
static struct kxtik1004_i2c_data *obj_i2c_data = NULL;
static bool sensor_power = true;
static int sensor_suspend = 0;
static GSENSOR_VECTOR3D gsensor_gain;
static char selftestRes[8]= {0}; 
static DEFINE_MUTEX(kxtik1004_i2c_mutex);
static DEFINE_MUTEX(kxtik1004_op_mutex);

static bool enable_status = false;

/*----------------------------------------------------------------------------*/
#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN(f)               printk(KERN_INFO GSE_TAG"%s\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)    printk(KERN_ERR GSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    printk(KERN_INFO GSE_TAG fmt, ##args)
/*----------------------------------------------------------------------------*/
static struct data_resolution kxtik1004_data_resolution[1] = {
 /* combination by {FULL_RES,RANGE}*/
    {{ 0, 9}, 1024}, // dataformat +/-2g  in 12-bit resolution;  { 3, 9} = 3.9 = (2*2*1000)/(2^12);  256 = (2^12)/(2*2)          
};
/*----------------------------------------------------------------------------*/
static struct data_resolution kxtik1004_offset_resolution = {{15, 6}, 64};
/*----------------------------------------------------------------------------*/
static int kxt_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
    u8 beg = addr;
	int err;
	struct i2c_msg msgs[2]={{0},{0}};
	
	mutex_lock(&kxtik1004_i2c_mutex);
	
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len =1;
	msgs[0].buf = &beg;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len =len;
	msgs[1].buf = data;
	
	if (!client)
	{
	    mutex_unlock(&kxtik1004_i2c_mutex);
		return -EINVAL;
	}
	else if (len > C_I2C_FIFO_SIZE) 
	{
		GSE_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&kxtik1004_i2c_mutex);
		return -EINVAL;
	}
	err = i2c_transfer(client->adapter, msgs, sizeof(msgs)/sizeof(msgs[0]));
	if (err != 2) 
	{
		GSE_ERR("i2c_transfer error: (%d %p %d) %d\n",addr, data, len, err);
		err = -EIO;
	} 
	else 
	{
		err = 0;
	}
	mutex_unlock(&kxtik1004_i2c_mutex);
	return err;

}

static int kxt_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{   /*because address also occupies one byte, the maximum length for write is 7 bytes*/
    int err, idx, num;
    char buf[C_I2C_FIFO_SIZE];
    err =0;
	mutex_lock(&kxtik1004_i2c_mutex);
    if (!client)
    {
        mutex_unlock(&kxtik1004_i2c_mutex);
        return -EINVAL;
    }
    else if (len >= C_I2C_FIFO_SIZE) 
	{        
        GSE_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&kxtik1004_i2c_mutex);
        return -EINVAL;
    }    

    num = 0;
    buf[num++] = addr;
    for (idx = 0; idx < len; idx++)
    {
        buf[num++] = data[idx];
    }

    err = i2c_master_send(client, buf, num);
    if (err < 0)
	{
        GSE_ERR("send command error!!\n");
		mutex_unlock(&kxtik1004_i2c_mutex);
        return -EFAULT;
    } 
	mutex_unlock(&kxtik1004_i2c_mutex);
    return err;
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static int KXTIK1004_SetPowerMode(struct i2c_client *client, bool enable);
/*--------------------KXTIK1004 power control function----------------------------------*/
static void KXTIK1004_power(struct acc_hw *hw, unsigned int on) 
{
	static unsigned int power_on = 0;

	if(hw->power_id != POWER_NONE_MACRO)		// have externel LDO
	{        
		GSE_LOG("power %s\n", on ? "on" : "off");
		if(power_on == on)	// power status not change
		{
			GSE_LOG("ignore power control: %d\n", on);
		}
		else if(on)	// power on
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "KXTIK1004"))
			{
				GSE_ERR("power on fails!!\n");
			}
		}
		else	// power off
		{
			if (!hwPowerDown(hw->power_id, "KXTIK1004"))
			{
				GSE_ERR("power off fail!!\n");
			}			  
		}
	}
	power_on = on;    
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int KXTIK1004_SetDataResolution(struct kxtik1004_i2c_data *obj)
{
	int err;
	u8  databuf[2];

	KXTIK1004_SetPowerMode(obj->client, false);

	if(kxt_i2c_read_block(obj->client, KXTIK1004_REG_DATA_RESOLUTION, databuf, 0x01))
	{
		printk("kxtik1004 read Dataformat failt \n");
		return KXTIK1004_ERR_I2C;
	}

	databuf[0] &= ~KXTIK1004_RANGE_DATA_RESOLUTION_MASK;
	databuf[0] |= KXTIK1004_RANGE_DATA_RESOLUTION_MASK;//12bit

	err = kxt_i2c_write_block(obj->client, KXTIK1004_REG_DATA_RESOLUTION, databuf, 0x1);

	if(err < 0)
	{
		return KXTIK1004_ERR_I2C;
	}

	KXTIK1004_SetPowerMode(obj->client, true);

	//kxtik1004_data_resolution[0] has been set when initialize: +/-2g  in 8-bit resolution:  15.6 mg/LSB*/   
	obj->reso = &kxtik1004_data_resolution[0];

	return 0;
}
/*----------------------------------------------------------------------------*/
static int KXTIK1004_ReadData(struct i2c_client *client, s16 data[KXTIK1004_AXES_NUM])
{
	struct kxtik1004_i2c_data *priv = i2c_get_clientdata(client);        
	u8 addr = KXTIK1004_REG_DATAX0;
	u8 buf[KXTIK1004_DATA_LEN] = {0};
	int err = 0;
	int i;

	if(NULL == client)
	{
		err = -EINVAL;
	}
	else if((err = kxt_i2c_read_block(client, addr, buf, 0x06)))
	{
		GSE_ERR("error: %d\n", err);
	}
	else
	{
		data[KXTIK1004_AXIS_X] = (s16)((buf[KXTIK1004_AXIS_X*2] >> 4) |
		         (buf[KXTIK1004_AXIS_X*2+1] << 4));
		data[KXTIK1004_AXIS_Y] = (s16)((buf[KXTIK1004_AXIS_Y*2] >> 4) |
		         (buf[KXTIK1004_AXIS_Y*2+1] << 4));
		data[KXTIK1004_AXIS_Z] = (s16)((buf[KXTIK1004_AXIS_Z*2] >> 4) |
		         (buf[KXTIK1004_AXIS_Z*2+1] << 4));

		for(i=0;i<3;i++)				
		{								//because the data is store in binary complement number formation in computer system
			if ( data[i] == 0x0800 )	//so we want to calculate actual number here
				data[i]= -2048;			//10bit resolution, 512= 2^(12-1)
			else if ( data[i] & 0x0800 )//transfor format
			{							//printk("data 0 step %x \n",data[i]);
				data[i] -= 0x1; 		//printk("data 1 step %x \n",data[i]);
				data[i] = ~data[i]; 	//printk("data 2 step %x \n",data[i]);
				data[i] &= 0x07ff;		//printk("data 3 step %x \n\n",data[i]);
				data[i] = -data[i]; 	
			}
		}	


		if(atomic_read(&priv->trace) & ADX_TRC_RAWDATA)
		{
			GSE_LOG("[%08X %08X %08X] => [%5d %5d %5d]\n", data[KXTIK1004_AXIS_X], data[KXTIK1004_AXIS_Y], data[KXTIK1004_AXIS_Z],
		                               data[KXTIK1004_AXIS_X], data[KXTIK1004_AXIS_Y], data[KXTIK1004_AXIS_Z]);
		}
#ifdef CONFIG_KXTIK1004_LOWPASS
		if(atomic_read(&priv->filter))
		{
			if(atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend))
			{
				int idx, firlen = atomic_read(&priv->firlen);   
				if(priv->fir.num < firlen)
				{                
					priv->fir.raw[priv->fir.num][KXTIK1004_AXIS_X] = data[KXTIK1004_AXIS_X];
					priv->fir.raw[priv->fir.num][KXTIK1004_AXIS_Y] = data[KXTIK1004_AXIS_Y];
					priv->fir.raw[priv->fir.num][KXTIK1004_AXIS_Z] = data[KXTIK1004_AXIS_Z];
					priv->fir.sum[KXTIK1004_AXIS_X] += data[KXTIK1004_AXIS_X];
					priv->fir.sum[KXTIK1004_AXIS_Y] += data[KXTIK1004IK_AXIS_Y];
					priv->fir.sum[KXTIK1004_AXIS_Z] += data[KXTIK1004_AXIS_Z];
					if(atomic_read(&priv->trace) & ADX_TRC_FILTER)
					{
						GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d]\n", priv->fir.num,
							priv->fir.raw[priv->fir.num][KXTIK1004_AXIS_X], priv->fir.raw[priv->fir.num][KXTIK1004_AXIS_Y], priv->fir.raw[priv->fir.num][KXTIK1004_AXIS_Z],
							priv->fir.sum[KXTIK1004_AXIS_X], priv->fir.sum[KXTIK1004_AXIS_Y], priv->fir.sum[KXTIK1004_AXIS_Z]);
					}
					priv->fir.num++;
					priv->fir.idx++;
				}
				else
				{
					idx = priv->fir.idx % firlen;
					priv->fir.sum[KXTIK1004_AXIS_X] -= priv->fir.raw[idx][KXTIK1004_AXIS_X];
					priv->fir.sum[KXTIK1004_AXIS_Y] -= priv->fir.raw[idx][KXTIK1004_AXIS_Y];
					priv->fir.sum[KXTIK1004_AXIS_Z] -= priv->fir.raw[idx][KXTIK1004_AXIS_Z];
					priv->fir.raw[idx][KXTIK1004_AXIS_X] = data[KXTIK1004_AXIS_X];
					priv->fir.raw[idx][KXTIK1004_AXIS_Y] = data[KXTIK1004_AXIS_Y];
					priv->fir.raw[idx][KXTIK1004_AXIS_Z] = data[KXTIK1004_AXIS_Z];
					priv->fir.sum[KXTIK1004_AXIS_X] += data[KXTIK1004_AXIS_X];
					priv->fir.sum[KXTIK1004_AXIS_Y] += data[KXTIK1004_AXIS_Y];
					priv->fir.sum[KXTIK1004_AXIS_Z] += data[KXTIK1004_AXIS_Z];
					priv->fir.idx++;
					data[KXTIK1004_AXIS_X] = priv->fir.sum[KXTIK1004_AXIS_X]/firlen;
					data[KXTIK1004_AXIS_Y] = priv->fir.sum[KXTIK1004_AXIS_Y]/firlen;
					data[KXTIK1004_AXIS_Z] = priv->fir.sum[KXTIK1004_AXIS_Z]/firlen;
					if(atomic_read(&priv->trace) & ADX_TRC_FILTER)
					{
						GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d] : [%5d %5d %5d]\n", idx,
						priv->fir.raw[idx][KXTIK1004_AXIS_X], priv->fir.raw[idx][KXTIK1004_AXIS_Y], priv->fir.raw[idx][KXTIK1004_AXIS_Z],
						priv->fir.sum[KXTIK1004_AXIS_X], priv->fir.sum[KXTIK1004_AXIS_Y], priv->fir.sum[KXTIK1004_AXIS_Z],
						data[KXTIK1004_AXIS_X], data[KXTIK1004_AXIS_Y], data[KXTIK1004_AXIS_Z]);
					}
				}
			}
		}	
#endif         
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int KXTIK1004_ReadOffset(struct i2c_client *client, s8 ofs[KXTIK1004_AXES_NUM])
{    
	int err = 0;

	ofs[1]=ofs[2]=ofs[0]=0x00;

	printk("offesx=%x, y=%x, z=%x",ofs[0],ofs[1],ofs[2]);
	
	return err;    
}
/*----------------------------------------------------------------------------*/
static int KXTIK1004_ResetCalibration(struct i2c_client *client)
{
	struct kxtik1004_i2c_data *obj = i2c_get_clientdata(client);
	//u8 ofs[4]={0,0,0,0};
	int err = 0;

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	memset(obj->offset, 0x00, sizeof(obj->offset));
	return err;    
}
/*----------------------------------------------------------------------------*/
static int KXTIK1004_ReadCalibration(struct i2c_client *client, int dat[KXTIK1004_AXES_NUM])
{
    struct kxtik1004_i2c_data *obj = i2c_get_clientdata(client);
    int  err = 0;
    int mul;

	#ifdef SW_CALIBRATION
		mul = 0;//only SW Calibration, disable HW Calibration
	#else
	    if ((err = KXTIK1004_ReadOffset(client, obj->offset))) {
        GSE_ERR("read offset fail, %d\n", err);
        return err;
    	}    
    	mul = obj->reso->sensitivity/kxtik1004_offset_resolution.sensitivity;
	#endif

    dat[obj->cvt.map[KXTIK1004_AXIS_X]] = obj->cvt.sign[KXTIK1004_AXIS_X]*(obj->offset[KXTIK1004_AXIS_X]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[KXTIK1004_AXIS_X]);
    dat[obj->cvt.map[KXTIK1004_AXIS_Y]] = obj->cvt.sign[KXTIK1004_AXIS_Y]*(obj->offset[KXTIK1004_AXIS_Y]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[KXTIK1004_AXIS_Y]);
    dat[obj->cvt.map[KXTIK1004_AXIS_Z]] = obj->cvt.sign[KXTIK1004_AXIS_Z]*(obj->offset[KXTIK1004_AXIS_Z]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[KXTIK1004_AXIS_Z]);                        
                                       
    return err;
}
/*----------------------------------------------------------------------------*/
static int KXTIK1004_ReadCalibrationEx(struct i2c_client *client, int act[KXTIK1004_AXES_NUM], int raw[KXTIK1004_AXES_NUM])
{  
	/*raw: the raw calibration data; act: the actual calibration data*/
	struct kxtik1004_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	int mul;
	err = 0;
 

	#ifdef SW_CALIBRATION
		mul = 0;//only SW Calibration, disable HW Calibration
	#else
		if((err = KXTIK1004_ReadOffset(client, obj->offset)))
		{
			GSE_ERR("read offset fail, %d\n", err);
			return err;
		}   
		mul = obj->reso->sensitivity/kxtik1004_offset_resolution.sensitivity;
	#endif
	
	raw[KXTIK1004_AXIS_X] = obj->offset[KXTIK1004_AXIS_X]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[KXTIK1004_AXIS_X];
	raw[KXTIK1004_AXIS_Y] = obj->offset[KXTIK1004_AXIS_Y]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[KXTIK1004_AXIS_Y];
	raw[KXTIK1004_AXIS_Z] = obj->offset[KXTIK1004_AXIS_Z]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[KXTIK1004_AXIS_Z];

	act[obj->cvt.map[KXTIK1004_AXIS_X]] = obj->cvt.sign[KXTIK1004_AXIS_X]*raw[KXTIK1004_AXIS_X];
	act[obj->cvt.map[KXTIK1004_AXIS_Y]] = obj->cvt.sign[KXTIK1004_AXIS_Y]*raw[KXTIK1004_AXIS_Y];
	act[obj->cvt.map[KXTIK1004_AXIS_Z]] = obj->cvt.sign[KXTIK1004_AXIS_Z]*raw[KXTIK1004_AXIS_Z];                        
	                       
	return 0;
}
/*----------------------------------------------------------------------------*/
static int KXTIK1004_WriteCalibration(struct i2c_client *client, int dat[KXTIK1004_AXES_NUM])
{
	struct kxtik1004_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int cali[KXTIK1004_AXES_NUM], raw[KXTIK1004_AXES_NUM];
	//int lsb = kxtik1004_offset_resolution.sensitivity;
	//int divisor = obj->reso->sensitivity/lsb;

	if((err = KXTIK1004_ReadCalibrationEx(client, cali, raw)))	/*offset will be updated in obj->offset*/
	{ 
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}

	GSE_LOG("OLDOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n", 
		raw[KXTIK1004_AXIS_X], raw[KXTIK1004_AXIS_Y], raw[KXTIK1004_AXIS_Z],
		obj->offset[KXTIK1004_AXIS_X], obj->offset[KXTIK1004_AXIS_Y], obj->offset[KXTIK1004_AXIS_Z],
		obj->cali_sw[KXTIK1004_AXIS_X], obj->cali_sw[KXTIK1004_AXIS_Y], obj->cali_sw[KXTIK1004_AXIS_Z]);

	/*calculate the real offset expected by caller*/
	cali[KXTIK1004_AXIS_X] += dat[KXTIK1004_AXIS_X];
	cali[KXTIK1004_AXIS_Y] += dat[KXTIK1004_AXIS_Y];
	cali[KXTIK1004_AXIS_Z] += dat[KXTIK1004_AXIS_Z];

	GSE_LOG("UPDATE: (%+3d %+3d %+3d)\n", 
		dat[KXTIK1004_AXIS_X], dat[KXTIK1004_AXIS_Y], dat[KXTIK1004_AXIS_Z]);

#ifdef SW_CALIBRATION
	obj->cali_sw[KXTIK1004_AXIS_X] = obj->cvt.sign[KXTIK1004_AXIS_X]*(cali[obj->cvt.map[KXTIK1004_AXIS_X]]);
	obj->cali_sw[KXTIK1004_AXIS_Y] = obj->cvt.sign[KXTIK1004_AXIS_Y]*(cali[obj->cvt.map[KXTIK1004_AXIS_Y]]);
	obj->cali_sw[KXTIK1004_AXIS_Z] = obj->cvt.sign[KXTIK1004_AXIS_Z]*(cali[obj->cvt.map[KXTIK1004_AXIS_Z]]);	
#else
	obj->offset[KXTIK1004_AXIS_X] = (s8)(obj->cvt.sign[KXTIK1004_AXIS_X]*(cali[obj->cvt.map[KXTIK1004_AXIS_X]]) * obj->reso->sensitivity / GRAVITY_EARTH_1000/(divisor));
	obj->offset[KXTIK1004_AXIS_Y] = (s8)(obj->cvt.sign[KXTIK1004_AXIS_Y]*(cali[obj->cvt.map[KXTIK1004_AXIS_Y]]) * obj->reso->sensitivity / GRAVITY_EARTH_1000/(divisor));
	obj->offset[KXTIK1004_AXIS_Z] = (s8)(obj->cvt.sign[KXTIK1004_AXIS_Z]*(cali[obj->cvt.map[KXTIK1004_AXIS_Z]]) * obj->reso->sensitivity / GRAVITY_EARTH_1000/(divisor));

	/*convert software calibration using standard calibration*/
	obj->cali_sw[KXTIK1004_AXIS_X] = obj->cvt.sign[KXTIK1004_AXIS_X]*(cali[obj->cvt.map[KXTIK1004_AXIS_X]])%(divisor);
	obj->cali_sw[KXTIK1004_AXIS_Y] = obj->cvt.sign[KXTIK1004_AXIS_Y]*(cali[obj->cvt.map[KXTIK1004_AXIS_Y]])%(divisor);
	obj->cali_sw[KXTIK1004_AXIS_Z] = obj->cvt.sign[KXTIK1004_AXIS_Z]*(cali[obj->cvt.map[KXTIK1004_AXIS_Z]])%(divisor);

	GSE_LOG("NEWOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n", 
		obj->offset[KXTIK1004_AXIS_X]*divisor + obj->cali_sw[KXTIK1004_AXIS_X], 
		obj->offset[KXTIK1004_AXIS_Y]*divisor + obj->cali_sw[KXTIK1004_AXIS_Y], 
		obj->offset[KXTIK1004_AXIS_Z]*divisor + obj->cali_sw[KXTIK1004_AXIS_Z], 
		obj->offset[KXTIK1004_AXIS_X], obj->offset[KXTIK1004_AXIS_Y], obj->offset[KXTIK1004_AXIS_Z],
		obj->cali_sw[KXTIK1004_AXIS_X], obj->cali_sw[KXTIK1004_AXIS_Y], obj->cali_sw[KXTIK1004_AXIS_Z]);

	if((err = kxt_i2c_write_block(obj->client, KXTIK1004_REG_OFSX, obj->offset, KXTIK1004_AXES_NUM))<0)
	{
		GSE_ERR("write offset fail: %d\n", err);
		return err;
	}
#endif

	return err;
}
/*----------------------------------------------------------------------------*/
static int KXTIK1004_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);    

	res = kxt_i2c_read_block(client, KXTIK1004_REG_DEVID, databuf, 0x1);
	if(res < 0)
	{
		goto exit_KXTIK1004_CheckDeviceID;
	}

	if(databuf[0] == KXTIK1004_DEVICE_ID)
	{
		GSE_LOG("KXTIK1004_CheckDeviceID 0x%x pass!\n ", databuf[0]);
	}
	else if(databuf[0] == KXCJK1013_DEVICE_ID)
	{
		GSE_LOG("KXCJK1013_CheckDeviceID 0x%x pass!\n ", databuf[0]);/*because KXCJK-1013 use same driver with KXTIK1004*/
	}
	else
	{
		GSE_LOG("KXTIK1004_CheckDeviceID 0x%x failt!\n ", databuf[0]);
		return KXTIK1004_ERR_IDENTIFICATION;
	}

	exit_KXTIK1004_CheckDeviceID:
	if (res < 0)
	{
		return KXTIK1004_ERR_I2C;
	}
	
	return KXTIK1004_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int KXTIK1004_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2];    
	int res = 0;
	u8 addr = KXTIK1004_REG_POWER_CTL;
	//struct kxtik1004_i2c_data *obj = i2c_get_clientdata(client);
	
	
	if(enable == sensor_power)
	{
		GSE_LOG("Sensor power status is newest!\n");
		return KXTIK1004_SUCCESS;
	}

	if(kxt_i2c_read_block(client, addr, databuf, 0x01))
	{
		GSE_ERR("read power ctl register err!\n");
		return KXTIK1004_ERR_I2C;
	}

	
	if(enable == TRUE)
	{
		databuf[0] |= KXTIK1004_MEASURE_MODE;
	}
	else
	{
		databuf[0] &= ~KXTIK1004_MEASURE_MODE;
	}

	res = kxt_i2c_write_block(client, KXTIK1004_REG_POWER_CTL, databuf, 0x1);
	if(res < 0)
	{
		return KXTIK1004_ERR_I2C;
	}


	GSE_LOG("KXTIK1004_SetPowerMode %d!\n ",enable);


	sensor_power = enable;

	mdelay(50);
	
	return KXTIK1004_SUCCESS;    
}
/*----------------------------------------------------------------------------*/
static int KXTIK1004_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	struct kxtik1004_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);  

	KXTIK1004_SetPowerMode(client, false);

	if(kxt_i2c_read_block(client, KXTIK1004_REG_DATA_FORMAT, databuf, 0x01))
	{
		GSE_LOG("kxtik1004 read Dataformat failt \n");
		return KXTIK1004_ERR_I2C;
	}

	databuf[0] &= ~KXTIK1004_RANGE_MASK;
	databuf[0] |= dataformat;

	res = kxt_i2c_write_block(client, KXTIK1004_REG_DATA_FORMAT, databuf, 0x1);
	if(res < 0)
	{
		return KXTIK1004_ERR_I2C;
	}

	KXTIK1004_SetPowerMode(client, true);
	
	GSE_LOG("KXTIK1004_SetDataFormat OK! \n");
	

	return KXTIK1004_SetDataResolution(obj);    
}
/*----------------------------------------------------------------------------*/
static int KXTIK1004_SetBWRate(struct i2c_client *client, u8 bwrate)
{
	u8 databuf[10]={0};    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);    

	if(kxt_i2c_read_block(client, KXTIK1004_REG_BW_RATE, databuf, 0x01))
	{
		GSE_LOG("kxtik1004 read rate failt \n");
		return KXTIK1004_ERR_I2C;
	}

	databuf[0] &= 0xf8;
	databuf[0] |= bwrate;

	res = kxt_i2c_write_block(client, KXTIK1004_REG_BW_RATE, databuf, 0x1);
	if(res < 0)
	{
		return KXTIK1004_ERR_I2C;
	}
	
	GSE_LOG("KXTIK1004_SetBWRate OK! \n");
	
	return KXTIK1004_SUCCESS;    
}
/*----------------------------------------------------------------------------*/
static int KXTIK1004_SetIntEnable(struct i2c_client *client, u8 intenable)
{
	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);    
	databuf[0] =  0x00;

	res = kxt_i2c_write_block(client, KXTIK1004_REG_INT_ENABLE, databuf, 0x1);

	if(res < 0)
	{
		return KXTIK1004_ERR_I2C;
	}
	
	return KXTIK1004_SUCCESS;    
}
/*----------------------------------------------------------------------------*/
static int kxtik1004_init_client(struct i2c_client *client, int reset_cali)
{
	struct kxtik1004_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	res = KXTIK1004_CheckDeviceID(client); 
	if(res != KXTIK1004_SUCCESS)
	{
		return res;
	}	

	res = KXTIK1004_SetPowerMode(client, enable_status);
	if(res != KXTIK1004_SUCCESS)
	{
		return res;
	}
	

	res = KXTIK1004_SetBWRate(client, KXTIK1004_BW_100HZ);
	if(res != KXTIK1004_SUCCESS ) //0x2C->BW=100Hz
	{
		return res;
	}

	res = KXTIK1004_SetDataFormat(client, KXTIK1004_RANGE_2G);
	if(res != KXTIK1004_SUCCESS) //0x2C->BW=100Hz
	{
		return res;
	}

	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = obj->reso->sensitivity;


	res = KXTIK1004_SetIntEnable(client, 0x00);        
	if(res != KXTIK1004_SUCCESS)//0x2E->0x80
	{
		return res;
	}

	if(0 != reset_cali)
	{ 
		/*reset calibration only in power on*/
		res = KXTIK1004_ResetCalibration(client);
		if(res != KXTIK1004_SUCCESS)
		{
			return res;
		}
	}
	GSE_LOG("kxtik1004_init_client OK!\n");
#ifdef CONFIG_KXTIK1004_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));  
#endif

	return KXTIK1004_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int KXTIK1004_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
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

	sprintf(buf, "KXTIK1004 Chip");
	return 0;
}
/*----------------------------------------------------------------------------*/
static int KXTIK1004_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
	struct kxtik1004_i2c_data *obj = (struct kxtik1004_i2c_data*)i2c_get_clientdata(client);
	u8 databuf[20];
	int acc[KXTIK1004_AXES_NUM];
	int res = 0;
	memset(databuf, 0, sizeof(u8)*10);

	if(NULL == buf)
	{
		return -1;
	}
	if(NULL == client)
	{
		*buf = 0;
		return -2;
	}
		
	if(sensor_suspend == 1)
	{
		//GSE_LOG("sensor in suspend read not data!\n");
		return 0;
	}

	if((res = KXTIK1004_ReadData(client, obj->data)))
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return -3;
	}
	else
	{
	#if 1
		obj->data[KXTIK1004_AXIS_X] = obj->data[KXTIK1004_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		obj->data[KXTIK1004_AXIS_Y] = obj->data[KXTIK1004_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		obj->data[KXTIK1004_AXIS_Z] = obj->data[KXTIK1004_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;		
	#endif	
		//printk("raw data x=%d, y=%d, z=%d \n",obj->data[KXTIK1004_AXIS_X],obj->data[KXTIK1004_AXIS_Y],obj->data[KXTIK1004_AXIS_Z]);
		obj->data[KXTIK1004_AXIS_X] += obj->cali_sw[KXTIK1004_AXIS_X];
		obj->data[KXTIK1004_AXIS_Y] += obj->cali_sw[KXTIK1004_AXIS_Y];
		obj->data[KXTIK1004_AXIS_Z] += obj->cali_sw[KXTIK1004_AXIS_Z];
		
		//printk("cali_sw x=%d, y=%d, z=%d \n",obj->cali_sw[KXTIK1004_AXIS_X],obj->cali_sw[KXTIK1004_AXIS_Y],obj->cali_sw[KXTIK1004_AXIS_Z]);
		
		/*remap coordinate*/
		acc[obj->cvt.map[KXTIK1004_AXIS_X]] = obj->cvt.sign[KXTIK1004_AXIS_X]*obj->data[KXTIK1004_AXIS_X];
		acc[obj->cvt.map[KXTIK1004_AXIS_Y]] = obj->cvt.sign[KXTIK1004_AXIS_Y]*obj->data[KXTIK1004_AXIS_Y];
		acc[obj->cvt.map[KXTIK1004_AXIS_Z]] = obj->cvt.sign[KXTIK1004_AXIS_Z]*obj->data[KXTIK1004_AXIS_Z];
		//printk("cvt x=%d, y=%d, z=%d \n",obj->cvt.sign[KXTIK1004_AXIS_X],obj->cvt.sign[KXTIK1004_AXIS_Y],obj->cvt.sign[KXTIK1004_AXIS_Z]);


		//GSE_LOG("Mapped gsensor data: %d, %d, %d!\n", acc[KXTIK1004_AXIS_X], acc[KXTIK1004_AXIS_Y], acc[KXTIK1004_AXIS_Z]);

		//Out put the mg
		//printk("mg acc=%d, GRAVITY=%d, sensityvity=%d \n",acc[KXTIK1004_AXIS_X],GRAVITY_EARTH_1000,obj->reso->sensitivity);
#if 0
		acc[KXTIK1004_AXIS_X] = acc[KXTIK1004_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		acc[KXTIK1004_AXIS_Y] = acc[KXTIK1004_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		acc[KXTIK1004_AXIS_Z] = acc[KXTIK1004_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;		
	#endif	
	

		sprintf(buf, "%04x %04x %04x", acc[KXTIK1004_AXIS_X], acc[KXTIK1004_AXIS_Y], acc[KXTIK1004_AXIS_Z]);
		if(atomic_read(&obj->trace) & ADX_TRC_IOCTL)
		{
			GSE_LOG("gsensor data: %s!\n", buf);
		}
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int KXTIK1004_ReadRawData(struct i2c_client *client, char *buf)
{
	struct kxtik1004_i2c_data *obj = (struct kxtik1004_i2c_data*)i2c_get_clientdata(client);
	int res = 0;

	if (!buf || !client)
	{
		return EINVAL;
	}
	
	if((res = KXTIK1004_ReadData(client, obj->data)))
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return EIO;
	}
	else
	{
		sprintf(buf, "KXTIK1004_ReadRawData %04x %04x %04x", obj->data[KXTIK1004_AXIS_X], 
			obj->data[KXTIK1004_AXIS_Y], obj->data[KXTIK1004_AXIS_Z]);
	
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int KXTIK1004_InitSelfTest(struct i2c_client *client)
{
	int res = 0;
	u8  data,result;
	
	res = kxt_i2c_read_block(client, KXTIK1004_REG_CTL_REG3, &data, 0x1);
	if(res < 0)
	{
		return res;
	}
//enable selftest bit
	data |= KXTIK1004_SELF_TEST;
	res = kxt_i2c_write_block(client, KXTIK1004_REG_CTL_REG3,  &data, 0x1);
	if(res < 0) //0x2C->BW=100Hz
	{
		return res;
	}
//step 1
	res = kxt_i2c_read_block(client, KXTIK1004_DCST_RESP, &result, 0x1);
	if(res < 0)
	{
		return res;
	}
	GSE_LOG("step1: result = %x",result);
	if(result != 0xaa)
		return -EINVAL;

//step 2
	data |= KXTIK1004_SELF_TEST;
	res = kxt_i2c_write_block(client, KXTIK1004_REG_CTL_REG3,  &data, 0x1);
	if(res < 0) //0x2C->BW=100Hz
	{
		return res;
	}
//step 3
	res = kxt_i2c_read_block(client, KXTIK1004_DCST_RESP, &result, 0x1);
	if(res < 0)
	{
		return res;
	}
	GSE_LOG("step3: result = %x",result);
	if(result != 0xAA)
		return -EINVAL;
		
//step 4
	res = kxt_i2c_read_block(client, KXTIK1004_DCST_RESP, &result, 0x1);
	if(res < 0)
	{
		return res;
	}
	GSE_LOG("step4: result = %x",result);
	if(result != 0x55)
		return -EINVAL;
	else
		return KXTIK1004_SUCCESS;
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = kxtik1004_i2c_client;
	char strbuf[KXTIK1004_BUFSIZE];
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	
	KXTIK1004_ReadChipInfo(client, strbuf, KXTIK1004_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);        
}

/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = kxtik1004_i2c_client;
	char strbuf[KXTIK1004_BUFSIZE];
	
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	KXTIK1004_ReadSensorData(client, strbuf, KXTIK1004_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = kxtik1004_i2c_client;
	struct kxtik1004_i2c_data *obj;
	int err, len = 0, mul;
	int tmp[KXTIK1004_AXES_NUM];

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);



	if((err = KXTIK1004_ReadOffset(client, obj->offset)))
	{
		return -EINVAL;
	}
	else if((err = KXTIK1004_ReadCalibration(client, tmp)))
	{
		return -EINVAL;
	}
	else
	{    
		mul = obj->reso->sensitivity/kxtik1004_offset_resolution.sensitivity;
		len += snprintf(buf+len, PAGE_SIZE-len, "[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n", mul,                        
			obj->offset[KXTIK1004_AXIS_X], obj->offset[KXTIK1004_AXIS_Y], obj->offset[KXTIK1004_AXIS_Z],
			obj->offset[KXTIK1004_AXIS_X], obj->offset[KXTIK1004_AXIS_Y], obj->offset[KXTIK1004_AXIS_Z]);
		len += snprintf(buf+len, PAGE_SIZE-len, "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1, 
			obj->cali_sw[KXTIK1004_AXIS_X], obj->cali_sw[KXTIK1004_AXIS_Y], obj->cali_sw[KXTIK1004_AXIS_Z]);

		len += snprintf(buf+len, PAGE_SIZE-len, "[ALL]    (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n", 
			obj->offset[KXTIK1004_AXIS_X]*mul + obj->cali_sw[KXTIK1004_AXIS_X],
			obj->offset[KXTIK1004_AXIS_Y]*mul + obj->cali_sw[KXTIK1004_AXIS_Y],
			obj->offset[KXTIK1004_AXIS_Z]*mul + obj->cali_sw[KXTIK1004_AXIS_Z],
			tmp[KXTIK1004_AXIS_X], tmp[KXTIK1004_AXIS_Y], tmp[KXTIK1004_AXIS_Z]);
		
		return len;
    }
}
/*----------------------------------------------------------------------------*/
static ssize_t store_cali_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct i2c_client *client = kxtik1004_i2c_client;  
	int err, x, y, z;
	int dat[KXTIK1004_AXES_NUM];

	if(!strncmp(buf, "rst", 3))
	{
		if((err = KXTIK1004_ResetCalibration(client)))
		{
			GSE_ERR("reset offset err = %d\n", err);
		}	
	}
	else if(3 == sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z))
	{
		dat[KXTIK1004_AXIS_X] = x;
		dat[KXTIK1004_AXIS_Y] = y;
		dat[KXTIK1004_AXIS_Z] = z;
		if((err = KXTIK1004_WriteCalibration(client, dat)))
		{
			GSE_ERR("write calibration err = %d\n", err);
		}		
	}
	else
	{
		GSE_ERR("invalid format\n");
	}
	
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_self_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = kxtik1004_i2c_client;

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

    return snprintf(buf, 8, "%s\n", selftestRes);
}
/*----------------------------------------------------------------------------*/
static ssize_t store_self_value(struct device_driver *ddri, const char *buf, size_t count)
{   /*write anything to this register will trigger the process*/
	struct item{
	s16 raw[KXTIK1004_AXES_NUM];
	};
	
	struct i2c_client *client = kxtik1004_i2c_client;  
	int res, num;
	struct item *prv = NULL, *nxt = NULL;
	//s32 avg_prv[KXTIK1004_AXES_NUM] = {0, 0, 0};
	//s32 avg_nxt[KXTIK1004_AXES_NUM] = {0, 0, 0};
	u8 data;


	if(1 != sscanf(buf, "%d", &num))
	{
		GSE_ERR("parse number fail\n");
		return count;
	}
	else if(num == 0)
	{
		GSE_ERR("invalid data count\n");
		return count;
	}

	prv = kzalloc(sizeof(*prv) * num, GFP_KERNEL);
	nxt = kzalloc(sizeof(*nxt) * num, GFP_KERNEL);
	if (!prv || !nxt)
	{
		goto exit;
	}


	GSE_LOG("NORMAL:\n");
	KXTIK1004_SetPowerMode(client,true); 

	/*initial setting for self test*/
	if(!KXTIK1004_InitSelfTest(client))
	{
		GSE_LOG("SELFTEST : PASS\n");
		strcpy(selftestRes,"y");
	}	
	else
	{
		GSE_LOG("SELFTEST : FAIL\n");		
		strcpy(selftestRes,"n");
	}

	res = kxt_i2c_read_block(client, KXTIK1004_REG_CTL_REG3, &data, 0x1);
	if(res < 0)
	{
		return res;
	}

	data &= ~KXTIK1004_SELF_TEST;
	res = kxt_i2c_write_block(client, KXTIK1004_REG_CTL_REG3,  &data, 0x1);
	if(res < 0) //0x2C->BW=100Hz
	{
		return res;
	}
	
	exit:
	/*restore the setting*/    
	kxtik1004_init_client(client, 0);
	kfree(prv);
	kfree(nxt);
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_selftest_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = kxtik1004_i2c_client;
	struct kxtik1004_i2c_data *obj;

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->selftest));
}
/*----------------------------------------------------------------------------*/
static ssize_t store_selftest_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct kxtik1004_i2c_data *obj = obj_i2c_data;
	int tmp;

	if(NULL == obj)
	{
		GSE_ERR("i2c data obj is null!!\n");
		return 0;
	}
	
	
	if(1 == sscanf(buf, "%d", &tmp))
	{        
		if(atomic_read(&obj->selftest) && !tmp)
		{
			/*enable -> disable*/
			kxtik1004_init_client(obj->client, 0);
		}
		else if(!atomic_read(&obj->selftest) && tmp)
		{
			/*disable -> enable*/
			KXTIK1004_InitSelfTest(obj->client);            
		}
		
		GSE_LOG("selftest: %d => %d\n", atomic_read(&obj->selftest), tmp);
		atomic_set(&obj->selftest, tmp); 
	}
	else
	{ 
		GSE_ERR("invalid content: '%s', length = %d\n", buf, count);   
	}
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_firlen_value(struct device_driver *ddri, char *buf)
{
#ifdef CONFIG_KXTIK1004_LOWPASS
	struct i2c_client *client = kxtik1004_i2c_client;
	struct kxtik1004_i2c_data *obj = i2c_get_clientdata(client);
	if(atomic_read(&obj->firlen))
	{
		int idx, len = atomic_read(&obj->firlen);
		GSE_LOG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);

		for(idx = 0; idx < len; idx++)
		{
			GSE_LOG("[%5d %5d %5d]\n", obj->fir.raw[idx][KXTIK1004_AXIS_X], obj->fir.raw[idx][KXTIK1004_AXIS_Y], obj->fir.raw[idx][KXTIK1004_AXIS_Z]);
		}
		
		GSE_LOG("sum = [%5d %5d %5d]\n", obj->fir.sum[KXTIK1004_AXIS_X], obj->fir.sum[KXTIK1004_AXIS_Y], obj->fir.sum[KXTIK1004_AXIS_Z]);
		GSE_LOG("avg = [%5d %5d %5d]\n", obj->fir.sum[KXTIK1004_AXIS_X]/len, obj->fir.sum[KXTIK1004_AXIS_Y]/len, obj->fir.sum[KXTIK1004_AXIS_Z]/len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->firlen));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}
/*----------------------------------------------------------------------------*/
static ssize_t store_firlen_value(struct device_driver *ddri, const char *buf, size_t count)
{
#ifdef CONFIG_KXTIK1004_LOWPASS
	struct i2c_client *client = kxtik1004_i2c_client;  
	struct kxtik1004_i2c_data *obj = i2c_get_clientdata(client);
	int firlen;

	if(1 != sscanf(buf, "%d", &firlen))
	{
		GSE_ERR("invallid format\n");
	}
	else if(firlen > C_MAX_FIR_LENGTH)
	{
		GSE_ERR("exceeds maximum filter length\n");
	}
	else
	{ 
		atomic_set(&obj->firlen, firlen);
		if(0 == firlen)
		{
			atomic_set(&obj->fir_en, 0);
		}
		else
		{
			memset(&obj->fir, 0x00, sizeof(obj->fir));
			atomic_set(&obj->fir_en, 1);
		}
	}
#endif    
	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct kxtik1004_i2c_data *obj = obj_i2c_data;
	if (obj == NULL)
	{
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}
	
	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct kxtik1004_i2c_data *obj = obj_i2c_data;
	int trace;
	if (obj == NULL)
	{
		GSE_ERR("i2c_data obj is null!!\n");
		return 0;
	}
	
	if(1 == sscanf(buf, "0x%x", &trace))
	{
		atomic_set(&obj->trace, trace);
	}	
	else
	{
		GSE_ERR("invalid content: '%s', length = %d\n", buf, count);
	}
	
	return count;    
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;    
	struct kxtik1004_i2c_data *obj = obj_i2c_data;
	if (obj == NULL)
	{
		GSE_ERR("i2c_data obj is null!!\n");
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
/*----------------------------------------------------------------------------*/
static ssize_t show_power_status_value(struct device_driver *ddri, char *buf)
{
	if(sensor_power)
		GSE_LOG("G sensor is in work mode, sensor_power = %d\n", sensor_power);
	else
		GSE_LOG("G sensor is in standby mode, sensor_power = %d\n", sensor_power);

	return 0;
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo,   S_IWUSR | S_IRUGO, show_chipinfo_value,      NULL);
static DRIVER_ATTR(sensordata, S_IWUSR | S_IRUGO, show_sensordata_value,    NULL);
static DRIVER_ATTR(cali,       S_IWUSR | S_IRUGO, show_cali_value,          store_cali_value);
static DRIVER_ATTR(selftest, S_IWUSR | S_IRUGO, show_self_value,  store_self_value);
static DRIVER_ATTR(self,   S_IWUSR | S_IRUGO, show_selftest_value,      store_selftest_value);
static DRIVER_ATTR(firlen,     S_IWUSR | S_IRUGO, show_firlen_value,        store_firlen_value);
static DRIVER_ATTR(trace,      S_IWUSR | S_IRUGO, show_trace_value,         store_trace_value);
static DRIVER_ATTR(status,               S_IRUGO, show_status_value,        NULL);
static DRIVER_ATTR(powerstatus,               S_IRUGO, show_power_status_value,        NULL);


/*----------------------------------------------------------------------------*/
static u8 i2c_dev_reg =0 ;

static ssize_t show_register(struct device_driver *pdri, char *buf)
{
	//int input_value;
		
	GSE_LOG("i2c_dev_reg is 0x%2x \n", i2c_dev_reg);

	return 0;
}

static ssize_t store_register(struct device_driver *ddri, const char *buf, size_t count)
{
	//unsigned long input_value;

	i2c_dev_reg = simple_strtoul(buf, NULL, 16);
	GSE_LOG("set i2c_dev_reg = 0x%2x \n", i2c_dev_reg);

	return 0;
}
static ssize_t store_register_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct kxtik1004_i2c_data *obj = obj_i2c_data;
	u8 databuf[2];  
	unsigned long input_value;
	int res;
	
	memset(databuf, 0, sizeof(u8)*2);    

	input_value = simple_strtoul(buf, NULL, 16);
	GSE_LOG("input_value = 0x%2lx \n", input_value);

	if(NULL == obj)
	{
		GSE_ERR("i2c data obj is null!!\n");
		return 0;
	}

	databuf[0] = input_value;
	GSE_LOG("databuf[0]=0x%2x  databuf[1]=0x%2x \n", databuf[0],databuf[1]);

	res = kxt_i2c_write_block(obj->client, i2c_dev_reg, databuf, 0x1);

	if(res < 0)
	{
		return KXTIK1004_ERR_I2C;
	}
	return 0;
	
}

static ssize_t show_register_value(struct device_driver *ddri, char *buf)
{
		struct kxtik1004_i2c_data *obj = obj_i2c_data;
		u8 databuf[1];	
		
		memset(databuf, 0, sizeof(u8)*1);	 
	
		if(NULL == obj)
		{
			GSE_ERR("i2c data obj is null!!\n");
			return 0;
		}
		
		if(kxt_i2c_read_block(obj->client, i2c_dev_reg, databuf, 0x01))
		{
			GSE_ERR("read power ctl register err!\n");
			return KXTIK1004_ERR_I2C;
		}

		GSE_LOG("i2c_dev_reg=0x%2x  data=0x%2x \n", i2c_dev_reg,databuf[0]);
	
		return 0;
		
}


static DRIVER_ATTR(i2c,      S_IWUSR | S_IRUGO, show_register_value,         store_register_value);
static DRIVER_ATTR(register,      S_IWUSR | S_IRUGO, show_register,         store_register);


/*----------------------------------------------------------------------------*/
static struct driver_attribute *kxtik1004_attr_list[] = {
	&driver_attr_chipinfo,     /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/
	&driver_attr_cali,         /*show calibration data*/
	&driver_attr_self,         /*self test demo*/
	&driver_attr_selftest,     /*self control: 0: disable, 1: enable*/
	&driver_attr_firlen,       /*filter length: 0: disable, others: enable*/
	&driver_attr_trace,        /*trace log*/
	&driver_attr_status,
	&driver_attr_powerstatus,
	&driver_attr_register,
	&driver_attr_i2c,
};
/*----------------------------------------------------------------------------*/
static int kxtik1004_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(kxtik1004_attr_list)/sizeof(kxtik1004_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, kxtik1004_attr_list[idx])))
		{            
			GSE_ERR("driver_create_file (%s) = %d\n", kxtik1004_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int kxtik1004_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(kxtik1004_attr_list)/sizeof(kxtik1004_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, kxtik1004_attr_list[idx]);
	}
	

	return err;
}

/*----------------------------------------------------------------------------*/
int gsensor_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value, sample_delay;	
	struct kxtik1004_i2c_data *priv = (struct kxtik1004_i2c_data*)self;
	hwm_sensor_data* gsensor_data;
	char buff[KXTIK1004_BUFSIZE];
	
	//GSE_FUN(f);
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				GSE_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value <= 5)
				{
					sample_delay = KXTIK1004_BW_200HZ;
				}
				else if(value <= 10)
				{
					sample_delay = KXTIK1004_BW_100HZ;
				}
				else
				{
					sample_delay = KXTIK1004_BW_50HZ;
				}
				mutex_lock(&kxtik1004_op_mutex);
				err = KXTIK1004_SetBWRate(priv->client, sample_delay);
				if(err != KXTIK1004_SUCCESS ) //0x2C->BW=100Hz
				{
					GSE_ERR("Set delay parameter error!\n");
				}
				mutex_unlock(&kxtik1004_op_mutex);
				if(value >= 50)
				{
					atomic_set(&priv->filter, 0);
				}
				else
				{	
				#if defined(CONFIG_KXTIK1004_LOWPASS)
					priv->fir.num = 0;
					priv->fir.idx = 0;
					priv->fir.sum[KXTIK1004_AXIS_X] = 0;
					priv->fir.sum[KXTIK1004_AXIS_Y] = 0;
					priv->fir.sum[KXTIK1004_AXIS_Z] = 0;
					atomic_set(&priv->filter, 1);
				#endif
				}
			}
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				GSE_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				mutex_lock(&kxtik1004_op_mutex);
				GSE_LOG("Gsensor device enable function enable = %d, sensor_power = %d!\n",value,sensor_power);
				if(((value == 0) && (sensor_power == false)) ||((value == 1) && (sensor_power == true)))
				{
					enable_status = sensor_power;
					GSE_LOG("Gsensor device have updated!\n");
				}
				else
				{
					enable_status = !sensor_power;
					err = KXTIK1004_SetPowerMode( priv->client, !sensor_power);
					GSE_LOG("Gsensor not in suspend BMA222_SetPowerMode!, enable_status = %d\n",enable_status);
				}
				mutex_unlock(&kxtik1004_op_mutex);
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				GSE_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				mutex_lock(&kxtik1004_op_mutex);
				gsensor_data = (hwm_sensor_data *)buff_out;
				KXTIK1004_ReadSensorData(priv->client, buff, KXTIK1004_BUFSIZE);
				sscanf(buff, "%x %x %x", &gsensor_data->values[0], 
					&gsensor_data->values[1], &gsensor_data->values[2]);				
				gsensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;				
				gsensor_data->value_divide = 1000;
				mutex_unlock(&kxtik1004_op_mutex); 
			}
			break;
		default:
			GSE_ERR("gsensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	
	return err;
}

/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int kxtik1004_open(struct inode *inode, struct file *file)
{
	file->private_data = kxtik1004_i2c_client;

	if(file->private_data == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int kxtik1004_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
static long kxtik1004_unlocked_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct kxtik1004_i2c_data *obj = (struct kxtik1004_i2c_data*)i2c_get_clientdata(client);	
	char strbuf[KXTIK1004_BUFSIZE];
	void __user *data;
	SENSOR_DATA sensor_data;
	long err = 0;
	int cali[3];

	//GSE_FUN(f);
	if(_IOC_DIR(cmd) & _IOC_READ)
	{
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	}
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
	{
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	if(err)
	{
		GSE_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch(cmd)
	{
		case GSENSOR_IOCTL_INIT:
			kxtik1004_init_client(client, 0);			
			break;

		case GSENSOR_IOCTL_READ_CHIPINFO:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			
			KXTIK1004_ReadChipInfo(client, strbuf, KXTIK1004_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				err = -EFAULT;
				break;
			}				 
			break;	  

		case GSENSOR_IOCTL_READ_SENSORDATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			KXTIK1004_SetPowerMode(client,true);	
			KXTIK1004_ReadSensorData(client, strbuf, KXTIK1004_BUFSIZE);
			if(copy_to_user(data, strbuf, strlen(strbuf)+1))
			{
				err = -EFAULT;
				break;	  
			}				 
			break;

		case GSENSOR_IOCTL_READ_GAIN:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}			
			
			if(copy_to_user(data, &gsensor_gain, sizeof(GSENSOR_VECTOR3D)))
			{
				err = -EFAULT;
				break;
			}				 
			break;

		case GSENSOR_IOCTL_READ_RAW_DATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			KXTIK1004_ReadRawData(client, strbuf);
			if(copy_to_user(data, &strbuf, strlen(strbuf)+1))
			{
				err = -EFAULT;
				break;	  
			}
			break;	  

		case GSENSOR_IOCTL_SET_CALI:
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			if(copy_from_user(&sensor_data, data, sizeof(sensor_data)))
			{
				err = -EFAULT;
				break;	  
			}
			if(atomic_read(&obj->suspend))
			{
				GSE_ERR("Perform calibration in suspend state!!\n");
				err = -EINVAL;
			}
			else
			{
			#if 0
				cali[KXTIK1004_AXIS_X] = sensor_data.x * obj->reso->sensitivity / GRAVITY_EARTH_1000;
				cali[KXTIK1004_AXIS_Y] = sensor_data.y * obj->reso->sensitivity / GRAVITY_EARTH_1000;
				cali[KXTIK1004_AXIS_Z] = sensor_data.z * obj->reso->sensitivity / GRAVITY_EARTH_1000;			  
			#else
			cali[KXTIK1004_AXIS_X] = sensor_data.x;
			cali[KXTIK1004_AXIS_Y] = sensor_data.y;
			cali[KXTIK1004_AXIS_Z] = sensor_data.z;			  
					

			#endif
				err = KXTIK1004_WriteCalibration(client, cali);			 
			}
			break;

		case GSENSOR_IOCTL_CLR_CALI:
			err = KXTIK1004_ResetCalibration(client);
			break;

		case GSENSOR_IOCTL_GET_CALI:
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			if((err = KXTIK1004_ReadCalibration(client, cali)))
			{
				break;
			}
			#if 0
			sensor_data.x = cali[KXTIK1004_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			sensor_data.y = cali[KXTIK1004_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			sensor_data.z = cali[KXTIK1004_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			#else
			sensor_data.x = cali[KXTIK1004_AXIS_X];
			sensor_data.y = cali[KXTIK1004_AXIS_Y];
			sensor_data.z = cali[KXTIK1004_AXIS_Z];
			
			#endif
			if(copy_to_user(data, &sensor_data, sizeof(sensor_data)))
			{
				err = -EFAULT;
				break;
			}		
			break;
		

		default:
			GSE_ERR("unknown IOCTL: 0x%08x\n", cmd);
			err = -ENOIOCTLCMD;
			break;
			
	}

	return err;
}


/*----------------------------------------------------------------------------*/
static struct file_operations kxtik1004_fops = {
	.owner = THIS_MODULE,
	.open = kxtik1004_open,
	.release = kxtik1004_release,
	.unlocked_ioctl = kxtik1004_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice kxtik1004_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &kxtik1004_fops,
};
/*----------------------------------------------------------------------------*/
#ifndef USE_EARLY_SUSPEND
/*----------------------------------------------------------------------------*/
static int kxtik1004_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct kxtik1004_i2c_data *obj = i2c_get_clientdata(client);    
	int err = 0;
	GSE_FUN();    
    mutex_lock(&kxtik1004_op_mutex);
	if(msg.event == PM_EVENT_SUSPEND)
	{   
		if(obj == NULL)
		{
			GSE_ERR("null pointer!!\n");
			mutex_unlock(&kxtik1004_op_mutex);
			return -EINVAL;
		}
		atomic_set(&obj->suspend, 1);
		if((err = KXTIK1004_SetPowerMode(obj->client, false)))
		{
			GSE_ERR("write power control fail!!\n");
			mutex_unlock(&kxtik1004_op_mutex);
			return err;
		}

		KXTIK1004_power(obj->hw, 0);
	}
	sensor_suspend = 1;
	mutex_unlock(&kxtik1004_op_mutex);
	return err;
}
/*----------------------------------------------------------------------------*/
static int kxtik1004_resume(struct i2c_client *client)
{
	struct kxtik1004_i2c_data *obj = i2c_get_clientdata(client);        
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	mutex_lock(&kxtik1004_op_mutex);

	KXTIK1004_power(obj->hw, 1);
	if((err = kxtik1004_init_client(client, 0)))
	{
		GSE_ERR("initialize client fail!!\n");
		mutex_unlock(&kxtik1004_op_mutex);
		return err;        
	}
	sensor_suspend = 0;
	atomic_set(&obj->suspend, 0);
    mutex_unlock(&kxtik1004_op_mutex);
	return 0;
}
/*----------------------------------------------------------------------------*/
#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void kxtik1004_early_suspend(struct early_suspend *h) 
{
	struct kxtik1004_i2c_data *obj = container_of(h, struct kxtik1004_i2c_data, early_drv);   
	int err;
	GSE_FUN();    

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1); 
	mutex_lock(&kxtik1004_op_mutex);
	if((err = KXTIK1004_SetPowerMode(obj->client, false)))
	{
		GSE_ERR("write power control fail!!\n");
		mutex_unlock(&kxtik1004_op_mutex);
		return;
	}

	
	sensor_suspend = 1;
	mutex_unlock(&kxtik1004_op_mutex);
	KXTIK1004_power(obj->hw, 0);
}
/*----------------------------------------------------------------------------*/
static void kxtik1004_late_resume(struct early_suspend *h)
{
	struct kxtik1004_i2c_data *obj = container_of(h, struct kxtik1004_i2c_data, early_drv);         
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}

	KXTIK1004_power(obj->hw, 1);
	mutex_lock(&kxtik1004_op_mutex);
	if((err = kxtik1004_init_client(obj->client, 0)))
	{
		GSE_ERR("initialize client fail!!\n");
		mutex_unlock(&kxtik1004_op_mutex);
		return;        
	}
	sensor_suspend = 0;
	mutex_unlock(&kxtik1004_op_mutex);
	atomic_set(&obj->suspend, 0);    
}
/*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/
static int kxtik1004_i2c_detect(struct i2c_client *client, struct i2c_board_info *info) 
{    
	strcpy(info->type, KXTIK1004_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int kxtik1004_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct kxtik1004_i2c_data *obj;
	struct hwmsen_object sobj;
	int err = 0;
	int retry = 0;
	GSE_FUN();

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	
	memset(obj, 0, sizeof(struct kxtik1004_i2c_data));

	obj->hw = get_cust_acc_hw();
	
	if((err = hwmsen_get_convert(obj->hw->direction, &obj->cvt)))
	{
		GSE_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit;
	}

	obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client,obj);
	
	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	
#ifdef CONFIG_KXTIK1004_LOWPASS
	if(obj->hw->firlen > C_MAX_FIR_LENGTH)
	{
		atomic_set(&obj->firlen, C_MAX_FIR_LENGTH);
	}	
	else
	{
		atomic_set(&obj->firlen, obj->hw->firlen);
	}
	
	if(atomic_read(&obj->firlen) > 0)
	{
		atomic_set(&obj->fir_en, 1);
	}
	
#endif

	kxtik1004_i2c_client = new_client;	

	for(retry = 0; retry < 3; retry++){
	if((err = kxtik1004_init_client(new_client, 1)))
	{
			GSE_ERR("kxtik1004_device init cilent fail time: %d\n", retry);
			continue;
	}
	}
	if(err != 0)
		goto exit_init_failed;
	

	if((err = misc_register(&kxtik1004_device)))
	{
		GSE_ERR("kxtik1004_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	if((err = kxtik1004_create_attr(&kxtik1004_gsensor_driver.driver)))
	{
		GSE_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	sobj.self = obj;
    sobj.polling = 1;
    sobj.sensor_operate = gsensor_operate;
	if((err = hwmsen_attach(ID_ACCELEROMETER, &sobj)))
	{
		GSE_ERR("attach fail = %d\n", err);
		goto exit_kfree;
	}

#ifdef USE_EARLY_SUSPEND
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	obj->early_drv.suspend  = kxtik1004_early_suspend,
	obj->early_drv.resume   = kxtik1004_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif 

	GSE_LOG("%s: OK\n", __func__);    
	return 0;

	exit_create_attr_failed:
	misc_deregister(&kxtik1004_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	//i2c_detach_client(new_client);
	exit_kfree:
	kfree(obj);
	exit:
	GSE_ERR("%s: err = %d\n", __func__, err);        
	return err;
}

/*----------------------------------------------------------------------------*/
static int kxtik1004_i2c_remove(struct i2c_client *client)
{
	int err = 0;	
	
	if((err = kxtik1004_delete_attr(&kxtik1004_gsensor_driver.driver)))
	{
		GSE_ERR("kxtik1004_delete_attr fail: %d\n", err);
	}
	
	if((err = misc_deregister(&kxtik1004_device)))
	{
		GSE_ERR("misc_deregister fail: %d\n", err);
	}

	if((err = hwmsen_detach(ID_ACCELEROMETER)))
	    

	kxtik1004_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int kxtik1004_probe(struct platform_device *pdev) 
{
	struct acc_hw *hw = get_cust_acc_hw();
	GSE_FUN();

	KXTIK1004_power(hw, 1);
	if(i2c_add_driver(&kxtik1004_i2c_driver))
	{
		GSE_ERR("add driver error\n");
		return -1;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int kxtik1004_remove(struct platform_device *pdev)
{
    struct acc_hw *hw = get_cust_acc_hw();

    GSE_FUN();    
    KXTIK1004_power(hw, 0);    
    i2c_del_driver(&kxtik1004_i2c_driver);
    return 0;
}
/*----------------------------------------------------------------------------*/
#if 1
#ifdef CONFIG_OF
static const struct of_device_id gsensor_of_match[] = {
	{ .compatible = "mediatek,gsensor", },
	{},
};
#endif

static struct platform_driver kxtik1004_gsensor_driver = {
	.probe      = kxtik1004_probe,
	.remove     = kxtik1004_remove,    
	.driver     = 
	{
		.name  = "gsensor",
		.owner  = THIS_MODULE,
        #ifdef CONFIG_OF
		.of_match_table = gsensor_of_match,
		#endif
	}
};
#else

static struct platform_driver kxtik1004_gsensor_driver = {
	.probe      = kxtik1004_probe,
	.remove     = kxtik1004_remove,    
	.driver     = {
		.name  = "gsensor",
		.owner = THIS_MODULE,
	}
};
#endif
/*----------------------------------------------------------------------------*/
static int __init kxtik1004_init(void)
{
	struct acc_hw *hw = get_cust_acc_hw();
	GSE_FUN();	
	GSE_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num);
	i2c_register_board_info(hw->i2c_num, &i2c_kxtik1004, 1);
	if(platform_driver_register(&kxtik1004_gsensor_driver))
	{
		GSE_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit kxtik1004_exit(void)
{
	GSE_FUN();
	platform_driver_unregister(&kxtik1004_gsensor_driver);
}
/*----------------------------------------------------------------------------*/
module_init(kxtik1004_init);
module_exit(kxtik1004_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("KXTIK1004 I2C driver");
MODULE_AUTHOR("Dexiang.Liu@mediatek.com");
