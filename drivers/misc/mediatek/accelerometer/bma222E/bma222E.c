/* BMA150 motion sensor driver
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

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include "bma222E.h"
#include <linux/hwmsen_helper.h>


#define POWER_NONE_MACRO MT65XX_POWER_NONE

/*----------------------------------------------------------------------------*/
#define I2C_DRIVERID_BMA222 222
/*----------------------------------------------------------------------------*/
#define DEBUG 1
/*----------------------------------------------------------------------------*/
//#define CONFIG_BMA150_LOWPASS   /*apply low pass filter on output*/       
#define SW_CALIBRATION

/*----------------------------------------------------------------------------*/
#define BMA222_AXIS_X          0
#define BMA222_AXIS_Y          1
#define BMA222_AXIS_Z          2
#define BMA222_AXES_NUM        3
#define BMA222_DATA_LEN        6
#define BMA222_DEV_NAME        "BMA222"
/*----------------------------------------------------------------------------*/

/*********/
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id bma222_i2c_id[] = {{BMA222_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_BMA222={ I2C_BOARD_INFO("BMA222", 0x18)};
/*the adapter id will be available in customization*/
//static unsigned short bma222_force[] = {0x00, BMA222_I2C_SLAVE_WRITE_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const bma222_forces[] = { bma222_force, NULL };
//static struct i2c_client_address_data bma222_addr_data = { .forces = bma222_forces,};

/*----------------------------------------------------------------------------*/
static int bma222_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int bma222_i2c_remove(struct i2c_client *client);
static int bma222_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
#ifndef USE_EARLY_SUSPEND
static int bma222_suspend(struct i2c_client *client, pm_message_t msg);
static int bma222_resume(struct i2c_client *client);
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
    s16 raw[C_MAX_FIR_LENGTH][BMA222_AXES_NUM];
    int sum[BMA222_AXES_NUM];
    int num;
    int idx;
};
/*----------------------------------------------------------------------------*/
struct bma222_i2c_data {
    struct i2c_client *client;
    struct acc_hw *hw;
    struct hwmsen_convert   cvt;
    
    /*misc*/
    struct data_resolution *reso;
    atomic_t                trace;
    atomic_t                suspend;
    atomic_t                selftest;
	atomic_t				filter;
    s16                     cali_sw[BMA222_AXES_NUM+1];

    /*data*/
    s8                      offset[BMA222_AXES_NUM+1];  /*+1: for 4-byte alignment*/
    s16                     data[BMA222_AXES_NUM+1];

#if defined(CONFIG_BMA222_LOWPASS)
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
static struct i2c_driver bma222_i2c_driver = {
    .driver = {
        .name           = BMA222_DEV_NAME,
    },
	.probe      		= bma222_i2c_probe,
	.remove    			= bma222_i2c_remove,
#if !defined(USE_EARLY_SUSPEND)    
    .suspend            = bma222_suspend,
    .resume             = bma222_resume,
#endif
	.id_table = bma222_i2c_id,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *bma222_i2c_client = NULL;
static struct platform_driver bma222_gsensor_driver;
static struct bma222_i2c_data *obj_i2c_data = NULL;
static bool sensor_power = true;
static int sensor_suspend = 0;
static GSENSOR_VECTOR3D gsensor_gain;
//static char selftestRes[8]= {0}; 
static DEFINE_MUTEX(bma222_i2c_mutex);
static DEFINE_MUTEX(bma222_op_mutex);

static bool enable_status = false;


/*----------------------------------------------------------------------------*/
#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN(f)               printk(KERN_INFO GSE_TAG"%s\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)    printk(KERN_ERR GSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    printk(KERN_INFO GSE_TAG fmt, ##args)
/*----------------------------------------------------------------------------*/
static struct data_resolution bma222_data_resolution[1] = {
 /* combination by {FULL_RES,RANGE}*/
    {{ 15, 6}, 64},   // dataformat +/-2g  in 8-bit resolution;  { 15, 6} = 15.6= (2*2*1000)/(2^8);  64 = (2^8)/(2*2)          
};
/*----------------------------------------------------------------------------*/
static struct data_resolution bma222_offset_resolution = {{15, 6}, 64};

/*----------------------------------------------------------------------------*/
static int bma_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
    u8 beg = addr;
	int err;
	struct i2c_msg msgs[2]={{0},{0}};
	
	mutex_lock(&bma222_i2c_mutex);
	
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
	    mutex_unlock(&bma222_i2c_mutex);
		return -EINVAL;
	}
	else if (len > C_I2C_FIFO_SIZE) 
	{
		GSE_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&bma222_i2c_mutex);
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
	mutex_unlock(&bma222_i2c_mutex);
	return err;

}

static int bma_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{   /*because address also occupies one byte, the maximum length for write is 7 bytes*/
    int err, idx, num;
    char buf[C_I2C_FIFO_SIZE];
    err =0;
	mutex_lock(&bma222_i2c_mutex);
    if (!client)
    {
        mutex_unlock(&bma222_i2c_mutex);
        return -EINVAL;
    }
    else if (len >= C_I2C_FIFO_SIZE) 
	{        
        GSE_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&bma222_i2c_mutex);
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
		mutex_unlock(&bma222_i2c_mutex);
        return -EFAULT;
    } 
	mutex_unlock(&bma222_i2c_mutex);
    return err;
}


/*----------------------------------------------------------------------------*/
/*--------------------BMA222 power control function----------------------------------*/
static void BMA222_power(struct acc_hw *hw, unsigned int on) 
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
			if(!hwPowerOn(hw->power_id, hw->power_vol, "BMA222"))
			{
				GSE_ERR("power on fails!!\n");
			}
		}
		else	// power off
		{
			if (!hwPowerDown(hw->power_id, "BMA222"))
			{
				GSE_ERR("power off fail!!\n");
			}			  
		}
	}
	power_on = on;    
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int BMA222_SetDataResolution(struct bma222_i2c_data *obj)
{

/*set g sensor dataresolution here*/

/*BMA222 only can set to 10-bit dataresolution, so do nothing in bma222 driver here*/

/*end of set dataresolution*/


 
 /*we set measure range from -2g to +2g in BMA150_SetDataFormat(client, BMA150_RANGE_2G), 
                                                    and set 10-bit dataresolution BMA150_SetDataResolution()*/
                                                    
 /*so bma222_data_resolution[0] set value as {{ 3, 9}, 256} when declaration, and assign the value to obj->reso here*/  

 	obj->reso = &bma222_data_resolution[0];
	return 0;
	
/*if you changed the measure range, for example call: BMA222_SetDataFormat(client, BMA150_RANGE_4G), 
you must set the right value to bma222_data_resolution*/

}
/*----------------------------------------------------------------------------*/
static int BMA222_ReadData(struct i2c_client *client, s16 data[BMA222_AXES_NUM])
{
	struct bma222_i2c_data *priv = i2c_get_clientdata(client);        
	u8 addr = BMA222_REG_DATAXLOW;
	u8 buf[BMA222_DATA_LEN] = {0};
	int err = 0;

	if(NULL == client)
	{
		err = -EINVAL;
	}
	else if((err = bma_i2c_read_block(client, addr, buf, 0x05))!=0)
	{
		GSE_ERR("error: %d\n", err);
	}
	else
	{
		data[BMA222_AXIS_X] = (s16)buf[BMA222_AXIS_X*2] ;
		data[BMA222_AXIS_Y] = (s16)buf[BMA222_AXIS_Y*2];
		data[BMA222_AXIS_Z] = (s16)buf[BMA222_AXIS_Z*2] ;
		if(atomic_read(&priv->trace) & ADX_TRC_RAWDATA)
		{
			GSE_LOG("[%08X %08X %08X] => [%5d %5d %5d] before\n", data[BMA222_AXIS_X], data[BMA222_AXIS_Y], data[BMA222_AXIS_Z],
		                               data[BMA222_AXIS_X], data[BMA222_AXIS_Y], data[BMA222_AXIS_Z]);
		}

		if(data[BMA222_AXIS_X]&0x80)
		{
				data[BMA222_AXIS_X] = ~data[BMA222_AXIS_X];
				data[BMA222_AXIS_X] &= 0xff;
				data[BMA222_AXIS_X]+=1;
				data[BMA222_AXIS_X] = -data[BMA222_AXIS_X];
		}
		if(data[BMA222_AXIS_Y]&0x80)
		{
				data[BMA222_AXIS_Y] = ~data[BMA222_AXIS_Y];
				data[BMA222_AXIS_Y] &= 0xff;
				data[BMA222_AXIS_Y]+=1;
				data[BMA222_AXIS_Y] = -data[BMA222_AXIS_Y];
		}
		if(data[BMA222_AXIS_Z]&0x80)
		{
				data[BMA222_AXIS_Z] = ~data[BMA222_AXIS_Z];
				data[BMA222_AXIS_Z] &= 0xff;
				data[BMA222_AXIS_Z]+=1;
				data[BMA222_AXIS_Z] = -data[BMA222_AXIS_Z];
		}

		if(atomic_read(&priv->trace) & ADX_TRC_RAWDATA)
		{
			GSE_LOG("[%08X %08X %08X] => [%5d %5d %5d] after\n", data[BMA222_AXIS_X], data[BMA222_AXIS_Y], data[BMA222_AXIS_Z],
		                               data[BMA222_AXIS_X], data[BMA222_AXIS_Y], data[BMA222_AXIS_Z]);
		}
#ifdef CONFIG_BMA222_LOWPASS
		if(atomic_read(&priv->filter))
		{
			if(atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend))
			{
				int idx, firlen = atomic_read(&priv->firlen);   
				if(priv->fir.num < firlen)
				{                
					priv->fir.raw[priv->fir.num][BMA222_AXIS_X] = data[BMA222_AXIS_X];
					priv->fir.raw[priv->fir.num][BMA222_AXIS_Y] = data[BMA222_AXIS_Y];
					priv->fir.raw[priv->fir.num][BMA222_AXIS_Z] = data[BMA222_AXIS_Z];
					priv->fir.sum[BMA222_AXIS_X] += data[BMA222_AXIS_X];
					priv->fir.sum[BMA222_AXIS_Y] += data[BMA222_AXIS_Y];
					priv->fir.sum[BMA222_AXIS_Z] += data[BMA222_AXIS_Z];
					if(atomic_read(&priv->trace) & ADX_TRC_FILTER)
					{
						GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d]\n", priv->fir.num,
							priv->fir.raw[priv->fir.num][BMA222_AXIS_X], priv->fir.raw[priv->fir.num][BMA222_AXIS_Y], priv->fir.raw[priv->fir.num][BMA222_AXIS_Z],
							priv->fir.sum[BMA222_AXIS_X], priv->fir.sum[BMA222_AXIS_Y], priv->fir.sum[BMA222_AXIS_Z]);
					}
					priv->fir.num++;
					priv->fir.idx++;
				}
				else
				{
					idx = priv->fir.idx % firlen;
					priv->fir.sum[BMA222_AXIS_X] -= priv->fir.raw[idx][BMA222_AXIS_X];
					priv->fir.sum[BMA222_AXIS_Y] -= priv->fir.raw[idx][BMA222_AXIS_Y];
					priv->fir.sum[BMA222_AXIS_Z] -= priv->fir.raw[idx][BMA222_AXIS_Z];
					priv->fir.raw[idx][BMA222_AXIS_X] = data[BMA222_AXIS_X];
					priv->fir.raw[idx][BMA222_AXIS_Y] = data[BMA222_AXIS_Y];
					priv->fir.raw[idx][BMA222_AXIS_Z] = data[BMA222_AXIS_Z];
					priv->fir.sum[BMA222_AXIS_X] += data[BMA222_AXIS_X];
					priv->fir.sum[BMA222_AXIS_Y] += data[BMA222_AXIS_Y];
					priv->fir.sum[BMA222_AXIS_Z] += data[BMA222_AXIS_Z];
					priv->fir.idx++;
					data[BMA222_AXIS_X] = priv->fir.sum[BMA222_AXIS_X]/firlen;
					data[BMA222_AXIS_Y] = priv->fir.sum[BMA222_AXIS_Y]/firlen;
					data[BMA222_AXIS_Z] = priv->fir.sum[BMA222_AXIS_Z]/firlen;
					if(atomic_read(&priv->trace) & ADX_TRC_FILTER)
					{
						GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d] : [%5d %5d %5d]\n", idx,
						priv->fir.raw[idx][BMA222_AXIS_X], priv->fir.raw[idx][BMA222_AXIS_Y], priv->fir.raw[idx][BMA222_AXIS_Z],
						priv->fir.sum[BMA222_AXIS_X], priv->fir.sum[BMA222_AXIS_Y], priv->fir.sum[BMA222_AXIS_Z],
						data[BMA222_AXIS_X], data[BMA222_AXIS_Y], data[BMA222_AXIS_Z]);
					}
				}
			}
		}	
#endif         
	}
	return err;
}
/*----------------------------------------------------------------------------*/

static int BMA222_ReadOffset(struct i2c_client *client, s8 ofs[BMA222_AXES_NUM])
{    
	int err;
	err = 0;
#ifdef SW_CALIBRATION
	ofs[0]=ofs[1]=ofs[2]=0x0;
#else
	if((err = bma_i2c_read_block(client, BMA222_REG_OFSX, ofs, BMA222_AXES_NUM)))
	{
		GSE_ERR("error: %d\n", err);
	}
#endif
	//printk("offesx=%x, y=%x, z=%x",ofs[0],ofs[1],ofs[2]);
	
	return err;    
}

/*----------------------------------------------------------------------------*/
static int BMA222_ResetCalibration(struct i2c_client *client)
{
	struct bma222_i2c_data *obj = i2c_get_clientdata(client);
	//u8 ofs[4]={0,0,0,0};
	int err;
	err = 0;
	
	#ifdef SW_CALIBRATION
		
	#else
		if((err = hwmsen_write_block(client, BMA222_REG_OFSX, ofs, 4)))
		{
			GSE_ERR("error: %d\n", err);
		}
	#endif

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	memset(obj->offset, 0x00, sizeof(obj->offset));
	return err;    
}
/*----------------------------------------------------------------------------*/
static int BMA222_ReadCalibration(struct i2c_client *client, int dat[BMA222_AXES_NUM])
{
    struct bma222_i2c_data *obj = i2c_get_clientdata(client);
    int  err = 0;
    int mul;

	#ifdef SW_CALIBRATION
		mul = 0;//only SW Calibration, disable HW Calibration
	#else
	    if ((err = BMA222_ReadOffset(client, obj->offset))) {
        GSE_ERR("read offset fail, %d\n", err);
        return err;
    	}    
    	mul = obj->reso->sensitivity/bma222_offset_resolution.sensitivity;
	#endif

    dat[obj->cvt.map[BMA222_AXIS_X]] = obj->cvt.sign[BMA222_AXIS_X]*(obj->offset[BMA222_AXIS_X]*mul * GRAVITY_EARTH_1000/obj->reso->sensitivity + obj->cali_sw[BMA222_AXIS_X]);
    dat[obj->cvt.map[BMA222_AXIS_Y]] = obj->cvt.sign[BMA222_AXIS_Y]*(obj->offset[BMA222_AXIS_Y]*mul * GRAVITY_EARTH_1000/obj->reso->sensitivity + obj->cali_sw[BMA222_AXIS_Y]);
    dat[obj->cvt.map[BMA222_AXIS_Z]] = obj->cvt.sign[BMA222_AXIS_Z]*(obj->offset[BMA222_AXIS_Z]*mul * GRAVITY_EARTH_1000/obj->reso->sensitivity + obj->cali_sw[BMA222_AXIS_Z]);                        
                                       
    return err;
}
/*----------------------------------------------------------------------------*/
static int BMA222_ReadCalibrationEx(struct i2c_client *client, int act[BMA222_AXES_NUM], int raw[BMA222_AXES_NUM])
{  
	/*raw: the raw calibration data; act: the actual calibration data*/
	struct bma222_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	int mul;
	err = 0;
 

	#ifdef SW_CALIBRATION
		mul = 0;//only SW Calibration, disable HW Calibration
	#else
		if((err = BMA222_ReadOffset(client, obj->offset)))
		{
			GSE_ERR("read offset fail, %d\n", err);
			return err;
		}   
		mul = obj->reso->sensitivity/bma222_offset_resolution.sensitivity;
	#endif
	
	raw[BMA222_AXIS_X] = obj->offset[BMA222_AXIS_X]*mul * GRAVITY_EARTH_1000/obj->reso->sensitivity + obj->cali_sw[BMA222_AXIS_X];
	raw[BMA222_AXIS_Y] = obj->offset[BMA222_AXIS_Y]*mul * GRAVITY_EARTH_1000/obj->reso->sensitivity + obj->cali_sw[BMA222_AXIS_Y];
	raw[BMA222_AXIS_Z] = obj->offset[BMA222_AXIS_Z]*mul * GRAVITY_EARTH_1000/obj->reso->sensitivity + obj->cali_sw[BMA222_AXIS_Z];

	act[obj->cvt.map[BMA222_AXIS_X]] = obj->cvt.sign[BMA222_AXIS_X]*raw[BMA222_AXIS_X];
	act[obj->cvt.map[BMA222_AXIS_Y]] = obj->cvt.sign[BMA222_AXIS_Y]*raw[BMA222_AXIS_Y];
	act[obj->cvt.map[BMA222_AXIS_Z]] = obj->cvt.sign[BMA222_AXIS_Z]*raw[BMA222_AXIS_Z];                        
	                       
	return 0;
}
/*----------------------------------------------------------------------------*/
static int BMA222_WriteCalibration(struct i2c_client *client, int dat[BMA222_AXES_NUM])
{
	struct bma222_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int cali[BMA222_AXES_NUM], raw[BMA222_AXES_NUM];
	int lsb; 
	lsb = bma222_offset_resolution.sensitivity;
	//int divisor = obj->reso->sensitivity/lsb;

	if((err = BMA222_ReadCalibrationEx(client, cali, raw)))	/*offset will be updated in obj->offset*/
	{ 
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}

	GSE_LOG("OLDOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n", 
		raw[BMA222_AXIS_X], raw[BMA222_AXIS_Y], raw[BMA222_AXIS_Z],
		obj->offset[BMA222_AXIS_X], obj->offset[BMA222_AXIS_Y], obj->offset[BMA222_AXIS_Z],
		obj->cali_sw[BMA222_AXIS_X], obj->cali_sw[BMA222_AXIS_Y], obj->cali_sw[BMA222_AXIS_Z]);

	/*calculate the real offset expected by caller*/
	cali[BMA222_AXIS_X] += dat[BMA222_AXIS_X];
	cali[BMA222_AXIS_Y] += dat[BMA222_AXIS_Y];
	cali[BMA222_AXIS_Z] += dat[BMA222_AXIS_Z];

	GSE_LOG("UPDATE: (%+3d %+3d %+3d)\n", 
		dat[BMA222_AXIS_X], dat[BMA222_AXIS_Y], dat[BMA222_AXIS_Z]);

#ifdef SW_CALIBRATION
	obj->cali_sw[BMA222_AXIS_X] = obj->cvt.sign[BMA222_AXIS_X]*(cali[obj->cvt.map[BMA222_AXIS_X]]);
	obj->cali_sw[BMA222_AXIS_Y] = obj->cvt.sign[BMA222_AXIS_Y]*(cali[obj->cvt.map[BMA222_AXIS_Y]]);
	obj->cali_sw[BMA222_AXIS_Z] = obj->cvt.sign[BMA222_AXIS_Z]*(cali[obj->cvt.map[BMA222_AXIS_Z]]);	
#else
	int divisor = obj->reso->sensitivity/lsb;//modified
	obj->offset[BMA222_AXIS_X] = (s8)(obj->cvt.sign[BMA222_AXIS_X]*(cali[obj->cvt.map[BMA222_AXIS_X]]) * obj->reso->sensitivity / GRAVITY_EARTH_1000/(divisor));
	obj->offset[BMA222_AXIS_Y] = (s8)(obj->cvt.sign[BMA222_AXIS_Y]*(cali[obj->cvt.map[BMA222_AXIS_Y]]) * obj->reso->sensitivity / GRAVITY_EARTH_1000/(divisor));
	obj->offset[BMA222_AXIS_Z] = (s8)(obj->cvt.sign[BMA222_AXIS_Z]*(cali[obj->cvt.map[BMA222_AXIS_Z]]) * obj->reso->sensitivity / GRAVITY_EARTH_1000/(divisor));

	/*convert software calibration using standard calibration*/
	obj->cali_sw[BMA222_AXIS_X] = obj->cvt.sign[BMA222_AXIS_X]*(cali[obj->cvt.map[BMA222_AXIS_X]])%(divisor);
	obj->cali_sw[BMA222_AXIS_Y] = obj->cvt.sign[BMA222_AXIS_Y]*(cali[obj->cvt.map[BMA222_AXIS_Y]])%(divisor);
	obj->cali_sw[BMA222_AXIS_Z] = obj->cvt.sign[BMA222_AXIS_Z]*(cali[obj->cvt.map[BMA222_AXIS_Z]])%(divisor);

	GSE_LOG("NEWOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n", 
		obj->offset[BMA222_AXIS_X]*divisor + obj->cali_sw[BMA222_AXIS_X], 
		obj->offset[BMA222_AXIS_Y]*divisor + obj->cali_sw[BMA222_AXIS_Y], 
		obj->offset[BMA222_AXIS_Z]*divisor + obj->cali_sw[BMA222_AXIS_Z], 
		obj->offset[BMA222_AXIS_X], obj->offset[BMA222_AXIS_Y], obj->offset[BMA222_AXIS_Z],
		obj->cali_sw[BMA222_AXIS_X], obj->cali_sw[BMA222_AXIS_Y], obj->cali_sw[BMA222_AXIS_Z]);

	if((err = hwmsen_write_block(obj->client, BMA222_REG_OFSX, obj->offset, BMA222_AXES_NUM)))
	{
		GSE_ERR("write offset fail: %d\n", err);
		return err;
	}
#endif
	mdelay(1);
	return err;
}
/*----------------------------------------------------------------------------*/
static int BMA222_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf[2]={0};    
	int res = 0;
	
	res = bma_i2c_read_block(client,BMA222_REG_DEVID,databuf,0x1);
	if(res < 0)
	{
		goto exit_BMA222_CheckDeviceID;
	}
	

	GSE_LOG("BMA222_CheckDeviceID %d done!\n ", databuf[0]);

	#if 0
	if(databuf[0]!=BMA222_FIXED_DEVID)
	{
		GSE_LOG("BMA222_CheckDeviceID %d failt!\n ", databuf[0]);
		return BMA222_ERR_IDENTIFICATION;
	}
	else
	{
		GSE_LOG("BMA222_CheckDeviceID %d pass!\n ", databuf[0]);
	}
	#endif

	exit_BMA222_CheckDeviceID:
	if (res < 0)
	{
		GSE_ERR("BMA222_CheckDeviceID %d failt!\n ", BMA222_ERR_I2C);
		return BMA222_ERR_I2C;
	}
	mdelay(1);
	return BMA222_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int BMA222_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2];    
	int res = 0;
	u8 addr = BMA222_REG_POWER_CTL;
	struct bma222_i2c_data *obj = i2c_get_clientdata(client);
	
	//GSE_LOG("enter Sensor power status is sensor_power = %d\n",sensor_power);

	if(enable == sensor_power )
	{
		GSE_LOG("Sensor power status is newest!\n");
		return BMA222_SUCCESS;
	}

	if(bma_i2c_read_block(client, addr, databuf, 0x01))
	{
		GSE_ERR("read power ctl register err!\n");
		return BMA222_ERR_I2C;
	}
	GSE_LOG("set power mode value = 0x%x!\n",databuf[0]);
	mdelay(1);
	if(enable == TRUE)
	{
		databuf[0] &= ~BMA222_MEASURE_MODE;
	}
	else
	{
		databuf[0] |= BMA222_MEASURE_MODE;
	}
	
	res = bma_i2c_write_block(client,BMA222_REG_POWER_CTL,databuf,0x1);
	if(res < 0)
	{
		GSE_LOG("set power mode failed!\n");
		return BMA222_ERR_I2C;
	}
	else if(atomic_read(&obj->trace) & ADX_TRC_INFO)
	{
		GSE_LOG("set power mode ok %d!\n", databuf[1]);
	}
	
	sensor_power = enable;
	mdelay(1);
	//GSE_LOG("leave Sensor power status is sensor_power = %d\n",sensor_power);
	return BMA222_SUCCESS;    
}
/*----------------------------------------------------------------------------*/
static int BMA222_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	struct bma222_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[10]={0};    
	int res = 0;
   
	if(bma_i2c_read_block(client, BMA222_REG_DATA_FORMAT, databuf, 0x01))
	{
		printk("bma222 read Dataformat failt \n");
		return BMA222_ERR_I2C;
	}
	mdelay(1);
	databuf[0] &= ~BMA222_RANGE_MASK;
	databuf[0] |= dataformat;
	
	res = bma_i2c_write_block(client,BMA222_REG_DATA_FORMAT,databuf,0x1);
	if(res < 0)
	{
		return BMA222_ERR_I2C;
	}
	
	//printk("BMA222_SetDataFormat OK! \n");
	mdelay(1);
	return BMA222_SetDataResolution(obj);    
}
/*----------------------------------------------------------------------------*/
static int BMA222_SetBWRate(struct i2c_client *client, u8 bwrate)
{
	u8 databuf[10]={0};    
	int res = 0;

	if(bma_i2c_read_block(client, BMA222_REG_BW_RATE, databuf, 0x01))
	{
		printk("bma222 read rate failt \n");
		return BMA222_ERR_I2C;
	}
	mdelay(1);
	databuf[0] &= ~BMA222_BW_MASK;
	databuf[0] |= bwrate;
	

    res = bma_i2c_write_block(client,BMA222_REG_BW_RATE,databuf,0x1);
	if(res < 0)
	{
		return BMA222_ERR_I2C;
	}
	mdelay(1);
	//printk("BMA222_SetBWRate OK! \n");
	
	return BMA222_SUCCESS;    
}
/*----------------------------------------------------------------------------*/
static int BMA222_SetIntEnable(struct i2c_client *client, u8 intenable)
{
			//u8 databuf[10];    
			int res = 0;
		
			res = hwmsen_write_byte(client, BMA222_INT_REG_1, 0x00);
			if(res != BMA222_SUCCESS) 
			{
				return res;
			}
			mdelay(1);
			res = hwmsen_write_byte(client, BMA222_INT_REG_2, 0x00);
			if(res != BMA222_SUCCESS) 
			{
				return res;
			}
			//printk("BMA222 disable interrupt ...\n");
		
			/*for disable interrupt function*/
			mdelay(1);
			return BMA222_SUCCESS;	  
}

/*----------------------------------------------------------------------------*/
static int bma222_init_client(struct i2c_client *client, int reset_cali)
{
	struct bma222_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;
	printk("bma222_init_client \n");

	res = BMA222_CheckDeviceID(client); 
	if(res != BMA222_SUCCESS)
	{
		return res;
	}	
	//printk("BMA222_CheckDeviceID ok \n");
	
	res = BMA222_SetBWRate(client, BMA222_BW_25HZ);
	if(res != BMA222_SUCCESS ) 
	{
		return res;
	}
	//printk("BMA222_SetBWRate OK!\n");
	
	res = BMA222_SetDataFormat(client, BMA222_RANGE_2G);
	if(res != BMA222_SUCCESS) 
	{
		return res;
	}
	//printk("BMA222_SetDataFormat OK!\n");

	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = obj->reso->sensitivity;


	res = BMA222_SetIntEnable(client, 0x00);        
	if(res != BMA222_SUCCESS)
	{
		return res;
	}
	//printk("BMA222 disable interrupt function!\n");
	
	res = BMA222_SetPowerMode(client, enable_status);//false);//
		if(res != BMA222_SUCCESS)
		{
			return res;
		}
	//printk("BMA222_SetPowerMode OK!\n");


	if(0 != reset_cali)
	{ 
		/*reset calibration only in power on*/
		res = BMA222_ResetCalibration(client);
		if(res != BMA222_SUCCESS)
		{
			return res;
		}
	}
	GSE_LOG("bma222_init_client OK!\n");
#ifdef CONFIG_BMA222_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));  
#endif
	return BMA222_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int BMA222_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
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

	sprintf(buf, "BMA222 Chip");
	return 0;
}
/*----------------------------------------------------------------------------*/
static int BMA222_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
	struct bma222_i2c_data *obj = (struct bma222_i2c_data*)i2c_get_clientdata(client);
	u8 databuf[20];
	int acc[BMA222_AXES_NUM];
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
	#if 0 //wrong operation marked
	mutex_lock(&bma222_mutex);
	if(sensor_power == FALSE)
	{
		GSE_ERR("BMA222_ReadSensorData bad operation sensor_power = %d!\n", sensor_power);
		res = BMA222_SetPowerMode(client, true);
		if(res)
		{
			GSE_ERR("Power on bma222 error %d!\n", res);
		}
	}
	mutex_unlock(&bma222_mutex);	
	#endif
	if((res = BMA222_ReadData(client, obj->data))!=0)
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return -3;
	}
	else
	{
	#if 1
		obj->data[BMA222_AXIS_X] = obj->data[BMA222_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		obj->data[BMA222_AXIS_Y] = obj->data[BMA222_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		obj->data[BMA222_AXIS_Z] = obj->data[BMA222_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;		
	#endif	
		//GSE_LOG("raw data x=%d, y=%d, z=%d \n",obj->data[BMA222_AXIS_X],obj->data[BMA222_AXIS_Y],obj->data[BMA222_AXIS_Z]);
		obj->data[BMA222_AXIS_X] += obj->cali_sw[BMA222_AXIS_X];
		obj->data[BMA222_AXIS_Y] += obj->cali_sw[BMA222_AXIS_Y];
		obj->data[BMA222_AXIS_Z] += obj->cali_sw[BMA222_AXIS_Z];
		
		//printk("cali_sw x=%d, y=%d, z=%d \n",obj->cali_sw[BMA150_AXIS_X],obj->cali_sw[BMA150_AXIS_Y],obj->cali_sw[BMA150_AXIS_Z]);
		
		/*remap coordinate*/
		acc[obj->cvt.map[BMA222_AXIS_X]] = obj->cvt.sign[BMA222_AXIS_X]*obj->data[BMA222_AXIS_X];
		acc[obj->cvt.map[BMA222_AXIS_Y]] = obj->cvt.sign[BMA222_AXIS_Y]*obj->data[BMA222_AXIS_Y];
		acc[obj->cvt.map[BMA222_AXIS_Z]] = obj->cvt.sign[BMA222_AXIS_Z]*obj->data[BMA222_AXIS_Z];
		//printk("cvt x=%d, y=%d, z=%d \n",obj->cvt.sign[BMA150_AXIS_X],obj->cvt.sign[BMA150_AXIS_Y],obj->cvt.sign[BMA150_AXIS_Z]);


		//GSE_LOG("Mapped gsensor data: %d, %d, %d!\n", acc[BMA150_AXIS_X], acc[BMA150_AXIS_Y], acc[BMA150_AXIS_Z]);

		//Out put the mg
		//printk("mg acc=%d, GRAVITY=%d, sensityvity=%d \n",acc[BMA150_AXIS_X],GRAVITY_EARTH_1000,obj->reso->sensitivity);
#if 0
		acc[BMA222_AXIS_X] = acc[BMA222_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		acc[BMA222_AXIS_Y] = acc[BMA222_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		acc[BMA222_AXIS_Z] = acc[BMA222_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;		
	#endif	
	

		sprintf(buf, "%04x %04x %04x", acc[BMA222_AXIS_X], acc[BMA222_AXIS_Y], acc[BMA222_AXIS_Z]);
		if(atomic_read(&obj->trace) & ADX_TRC_IOCTL)
		{
			GSE_LOG("gsensor data: %s!\n", buf);
		}
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int BMA222_ReadRawData(struct i2c_client *client, char *buf)
{
	struct bma222_i2c_data *obj = (struct bma222_i2c_data*)i2c_get_clientdata(client);
	int res = 0;

	if (!buf || !client)
	{
		return EINVAL;
	}
	
	if((res = BMA222_ReadData(client, obj->data)))
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return EIO;
	}
	else
	{
		sprintf(buf, "BMA222_ReadRawData %04x %04x %04x", obj->data[BMA222_AXIS_X], 
			obj->data[BMA222_AXIS_Y], obj->data[BMA222_AXIS_Z]);
	
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma222_i2c_client;
	char strbuf[BMA222_BUFSIZE];
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	
	BMA222_ReadChipInfo(client, strbuf, BMA222_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);        
}

#if 0
static ssize_t gsensor_init(struct device_driver *ddri, char *buf, size_t count)
	{
		struct i2c_client *client = bma222_i2c_client;
		char strbuf[BMA222_BUFSIZE];
		
		if(NULL == client)
		{
			GSE_ERR("i2c client is null!!\n");
			return 0;
		}
		bma222_init_client(client, 1);
		return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);			
	}
#endif


/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma222_i2c_client;
	char strbuf[BMA222_BUFSIZE];
	
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	BMA222_ReadSensorData(client, strbuf, BMA222_BUFSIZE);
	//BMA150_ReadRawData(client, strbuf);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);            
}

#if 0
static ssize_t show_sensorrawdata_value(struct device_driver *ddri, char *buf, size_t count)
	{
		struct i2c_client *client = bma222_i2c_client;
		char strbuf[BMA222_BUFSIZE];
		
		if(NULL == client)
		{
			GSE_ERR("i2c client is null!!\n");
			return 0;
		}
		//BMA150_ReadSensorData(client, strbuf, BMA150_BUFSIZE);
		BMA222_ReadRawData(client, strbuf);
		return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);			
	}
#endif

/*----------------------------------------------------------------------------*/
#if 1
static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma222_i2c_client;
	struct bma222_i2c_data *obj;
	int err, len = 0, mul;
	int tmp[BMA222_AXES_NUM];

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);



	if((err = BMA222_ReadOffset(client, obj->offset)))
	{
		return -EINVAL;
	}
	else if((err = BMA222_ReadCalibration(client, tmp)))
	{
		return -EINVAL;
	}
	else
	{    
		mul = obj->reso->sensitivity/bma222_offset_resolution.sensitivity;
		len += snprintf(buf+len, PAGE_SIZE-len, "[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n", mul,                        
			obj->offset[BMA222_AXIS_X], obj->offset[BMA222_AXIS_Y], obj->offset[BMA222_AXIS_Z],
			obj->offset[BMA222_AXIS_X], obj->offset[BMA222_AXIS_Y], obj->offset[BMA222_AXIS_Z]);
		len += snprintf(buf+len, PAGE_SIZE-len, "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1, 
			obj->cali_sw[BMA222_AXIS_X], obj->cali_sw[BMA222_AXIS_Y], obj->cali_sw[BMA222_AXIS_Z]);

		len += snprintf(buf+len, PAGE_SIZE-len, "[ALL]    (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n", 
			obj->offset[BMA222_AXIS_X]*mul + obj->cali_sw[BMA222_AXIS_X],
			obj->offset[BMA222_AXIS_Y]*mul + obj->cali_sw[BMA222_AXIS_Y],
			obj->offset[BMA222_AXIS_Z]*mul + obj->cali_sw[BMA222_AXIS_Z],
			tmp[BMA222_AXIS_X], tmp[BMA222_AXIS_Y], tmp[BMA222_AXIS_Z]);
		
		return len;
    }
}
/*----------------------------------------------------------------------------*/
static ssize_t store_cali_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct i2c_client *client = bma222_i2c_client;  
	int err, x, y, z;
	int dat[BMA222_AXES_NUM];

	if(!strncmp(buf, "rst", 3))
	{
		if((err = BMA222_ResetCalibration(client)))
		{
			GSE_ERR("reset offset err = %d\n", err);
		}	
	}
	else if(3 == sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z))
	{
		dat[BMA222_AXIS_X] = x;
		dat[BMA222_AXIS_Y] = y;
		dat[BMA222_AXIS_Z] = z;
		if((err = BMA222_WriteCalibration(client, dat)))
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
#endif

/*----------------------------------------------------------------------------*/
static ssize_t show_firlen_value(struct device_driver *ddri, char *buf)
{
#ifdef CONFIG_BMA222_LOWPASS
	struct i2c_client *client = bma222_i2c_client;
	struct bma222_i2c_data *obj = i2c_get_clientdata(client);
	if(atomic_read(&obj->firlen))
	{
		int idx, len = atomic_read(&obj->firlen);
		GSE_LOG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);

		for(idx = 0; idx < len; idx++)
		{
			GSE_LOG("[%5d %5d %5d]\n", obj->fir.raw[idx][BMA222_AXIS_X], obj->fir.raw[idx][BMA222_AXIS_Y], obj->fir.raw[idx][BMA222_AXIS_Z]);
		}
		
		GSE_LOG("sum = [%5d %5d %5d]\n", obj->fir.sum[BMA222_AXIS_X], obj->fir.sum[BMA222_AXIS_Y], obj->fir.sum[BMA222_AXIS_Z]);
		GSE_LOG("avg = [%5d %5d %5d]\n", obj->fir.sum[BMA222_AXIS_X]/len, obj->fir.sum[BMA222_AXIS_Y]/len, obj->fir.sum[BMA222_AXIS_Z]/len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->firlen));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}
/*----------------------------------------------------------------------------*/
static ssize_t store_firlen_value(struct device_driver *ddri, const char *buf, size_t count)
{
#ifdef CONFIG_BMA222_LOWPASS
	struct i2c_client *client = bma222_i2c_client;  
	struct bma222_i2c_data *obj = i2c_get_clientdata(client);
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
		if(NULL == firlen)
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
	struct bma222_i2c_data *obj = obj_i2c_data;
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
	struct bma222_i2c_data *obj = obj_i2c_data;
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
		GSE_ERR("invalid content: '%s', length = %d\n", buf, (int)count);
	}
	
	return count;    
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;    
	struct bma222_i2c_data *obj = obj_i2c_data;
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
	
	u8 databuf[2];    
	//int res = 0;
	u8 addr = BMA222_REG_POWER_CTL;
	struct bma222_i2c_data *obj = obj_i2c_data;
	if(bma_i2c_read_block(obj->client, addr, databuf, 0x01))
	{
		GSE_ERR("read power ctl register err!\n");
		return 1;
	}
	
	if(sensor_power)
		GSE_LOG("G sensor is in work mode, sensor_power = %d\n", sensor_power);
	else
		GSE_LOG("G sensor is in standby mode, sensor_power = %d\n", sensor_power);

	return snprintf(buf, PAGE_SIZE, "%x\n", databuf[0]);
}
/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo,   S_IWUSR | S_IRUGO, show_chipinfo_value,      NULL);
static DRIVER_ATTR(sensordata, S_IWUSR | S_IRUGO, show_sensordata_value,    NULL);
static DRIVER_ATTR(cali,       S_IWUSR | S_IRUGO, show_cali_value,          store_cali_value);
static DRIVER_ATTR(firlen,     S_IWUSR | S_IRUGO, show_firlen_value,        store_firlen_value);
static DRIVER_ATTR(trace,      S_IWUSR | S_IRUGO, show_trace_value,         store_trace_value);
static DRIVER_ATTR(status,     S_IRUGO, show_status_value,        NULL);
static DRIVER_ATTR(powerstatus,               S_IRUGO, show_power_status_value,        NULL);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *bma222_attr_list[] = {
	&driver_attr_chipinfo,     /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/
	&driver_attr_cali,         /*show calibration data*/
	&driver_attr_firlen,       /*filter length: 0: disable, others: enable*/
	&driver_attr_trace,        /*trace log*/
	&driver_attr_status,
	&driver_attr_powerstatus,
};
/*----------------------------------------------------------------------------*/
static int bma222_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(bma222_attr_list)/sizeof(bma222_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, bma222_attr_list[idx])))
		{            
			GSE_ERR("driver_create_file (%s) = %d\n", bma222_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int bma222_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(bma222_attr_list)/sizeof(bma222_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, bma222_attr_list[idx]);
	}
	

	return err;
}

/*----------------------------------------------------------------------------*/
int gsensor_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value, sample_delay;	
	struct bma222_i2c_data *priv = (struct bma222_i2c_data*)self;
	hwm_sensor_data* gsensor_data;
	char buff[BMA222_BUFSIZE];
	
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
					sample_delay = BMA222_BW_200HZ;
				}
				else if(value <= 10)
				{
					sample_delay = BMA222_BW_100HZ;
				}
				else
				{
					sample_delay = BMA222_BW_50HZ;
				}
				mutex_lock(&bma222_op_mutex);
				err = BMA222_SetBWRate(priv->client, sample_delay);
				if(err != BMA222_SUCCESS ) //0x2C->BW=100Hz
				{
					GSE_ERR("Set delay parameter error!\n");
				}
				mutex_unlock(&bma222_op_mutex);
				if(value >= 50)
				{
					atomic_set(&priv->filter, 0);
				}
				else
				{	
				#if defined(CONFIG_BMA222_LOWPASS)
					priv->fir.num = 0;
					priv->fir.idx = 0;
					priv->fir.sum[BMA222_AXIS_X] = 0;
					priv->fir.sum[BMA222_AXIS_Y] = 0;
					priv->fir.sum[BMA222_AXIS_Z] = 0;
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
				mutex_lock(&bma222_op_mutex);
				GSE_LOG("Gsensor device enable function enable = %d, sensor_power = %d!\n",value,sensor_power);
				if(((value == 0) && (sensor_power == false)) ||((value == 1) && (sensor_power == true)))
				{
					enable_status = sensor_power;
					GSE_LOG("Gsensor device have updated !\n");
				}
				else
				{
					enable_status = !sensor_power;
					err = BMA222_SetPowerMode( priv->client, !sensor_power);
					GSE_LOG("Gsensor not in suspend BMA222_SetPowerMode!, enable_status = %d\n",enable_status);
					
				}
				mutex_unlock(&bma222_op_mutex);
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
				gsensor_data = (hwm_sensor_data *)buff_out;
				BMA222_ReadSensorData(priv->client, buff, BMA222_BUFSIZE);
				sscanf(buff, "%x %x %x", &gsensor_data->values[0], 
					&gsensor_data->values[1], &gsensor_data->values[2]);				
				gsensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;				
				gsensor_data->value_divide = 1000;
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
static int bma222_open(struct inode *inode, struct file *file)
{
	file->private_data = bma222_i2c_client;

	if(file->private_data == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int bma222_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
//static int bma222_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
//       unsigned long arg)
static long bma222_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)

{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct bma222_i2c_data *obj = (struct bma222_i2c_data*)i2c_get_clientdata(client);	
	char strbuf[BMA222_BUFSIZE];
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
			bma222_init_client(client, 0);	
			break;

		case GSENSOR_IOCTL_READ_CHIPINFO:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			
			BMA222_ReadChipInfo(client, strbuf, BMA222_BUFSIZE);
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
			BMA222_SetPowerMode(client,true);	
			BMA222_ReadSensorData(client, strbuf, BMA222_BUFSIZE);
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
			BMA222_ReadRawData(client, strbuf);
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
				cali[BMA222_AXIS_X] = sensor_data.x * obj->reso->sensitivity / GRAVITY_EARTH_1000;
				cali[BMA222_AXIS_Y] = sensor_data.y * obj->reso->sensitivity / GRAVITY_EARTH_1000;
				cali[BMA222_AXIS_Z] = sensor_data.z * obj->reso->sensitivity / GRAVITY_EARTH_1000;			  
			#else
			cali[BMA222_AXIS_X] = sensor_data.x;
			cali[BMA222_AXIS_Y] = sensor_data.y;
			cali[BMA222_AXIS_Z] = sensor_data.z;			  
					

			#endif
				err = BMA222_WriteCalibration(client, cali);			 
			}
			break;

		case GSENSOR_IOCTL_CLR_CALI:
			err = BMA222_ResetCalibration(client);
			break;

		case GSENSOR_IOCTL_GET_CALI:
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			if((err = BMA222_ReadCalibration(client, cali)))
			{
				break;
			}
			#if 0
			sensor_data.x = cali[BMA222_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			sensor_data.y = cali[BMA222_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			sensor_data.z = cali[BMA222_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			#else
			sensor_data.x = cali[BMA222_AXIS_X];
			sensor_data.y = cali[BMA222_AXIS_Y];
			sensor_data.z = cali[BMA222_AXIS_Z];
						

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
static struct file_operations bma222_fops = {
	.owner = THIS_MODULE,
	.open = bma222_open,
	.release = bma222_release,
	.unlocked_ioctl = bma222_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice bma222_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &bma222_fops,
};
/*----------------------------------------------------------------------------*/
#ifndef USE_EARLY_SUSPEND
/*----------------------------------------------------------------------------*/
static int bma222_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct bma222_i2c_data *obj = i2c_get_clientdata(client);    
	int err = 0;
	GSE_FUN();    
    mutex_lock(&bma222_op_mutex);
	if(msg.event == PM_EVENT_SUSPEND)
	{   
		if(obj == NULL)
		{
			GSE_ERR("null pointer!!\n");
			mutex_unlock(&bma222_op_mutex);
			return -EINVAL;
		}
		atomic_set(&obj->suspend, 1);
		if((err = BMA222_SetPowerMode(obj->client, false)))
		{
			GSE_ERR("write power control fail!!\n");
			mutex_unlock(&bma222_op_mutex);
			return -EINVAL;
		}       
		BMA222_power(obj->hw, 0);
	}
	mutex_unlock(&bma222_op_mutex);
	return err;
}
/*----------------------------------------------------------------------------*/
static int bma222_resume(struct i2c_client *client)
{
	struct bma222_i2c_data *obj = i2c_get_clientdata(client);        
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	mutex_lock(&bma222_op_mutex);
	BMA222_power(obj->hw, 1);

	if((err = bma222_init_client(client, 0)))
	{
		GSE_ERR("initialize client fail!!\n");
		mutex_unlock(&bma222_op_mutex);
		return err;        
	}
	atomic_set(&obj->suspend, 0);
    mutex_unlock(&bma222_op_mutex);
	return 0;
}
/*----------------------------------------------------------------------------*/
#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void bma222_early_suspend(struct early_suspend *h) 
{
	struct bma222_i2c_data *obj = container_of(h, struct bma222_i2c_data, early_drv);   
	int err;
		
	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1); 
	mutex_lock(&bma222_op_mutex);
	GSE_FUN();  
	u8 databuf[2]; //for debug read power control register to see the value is OK
	if(bma_i2c_read_block(obj->client, BMA222_REG_POWER_CTL, databuf, 0x01))
	{
		GSE_ERR("read power ctl register err!\n");
		mutex_unlock(&bma222_op_mutex);
		return BMA222_ERR_I2C;
	}
	if(databuf[0]==0xff)//if the value is ff the gsensor will not work anymore, any i2c operations won't be vaild
		GSE_LOG("before BMA222_SetPowerMode in suspend databuf = 0x%x\n",databuf[0]);
	if((err = BMA222_SetPowerMode(obj->client, false)))
	{
		GSE_ERR("write power control fail!!\n");
		mutex_unlock(&bma222_op_mutex);
		return;
	}
	if(bma_i2c_read_block(obj->client, BMA222_REG_POWER_CTL, databuf, 0x01)) //for debug read power control register to see the value is OK
	{
		GSE_ERR("read power ctl register err!\n");
		mutex_unlock(&bma222_op_mutex);
		return BMA222_ERR_I2C;
	}
	if(databuf[0]==0xff)//if the value is ff the gsensor will not work anymore, any i2c operations won't be vaild
		GSE_LOG("after BMA222_SetPowerMode suspend err databuf = 0x%x\n",databuf[0]);
	sensor_suspend = 1;
	mutex_unlock(&bma222_op_mutex);
	BMA222_power(obj->hw, 0);
}
/*----------------------------------------------------------------------------*/
static void bma222_late_resume(struct early_suspend *h)
{
	struct bma222_i2c_data *obj = container_of(h, struct bma222_i2c_data, early_drv);         
	int err;
	
	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}

	BMA222_power(obj->hw, 1);
	mutex_lock(&bma222_op_mutex);
	GSE_FUN();
	u8 databuf[2];//for debug read power control register to see the value is OK
	if(bma_i2c_read_block(obj->client, BMA222_REG_POWER_CTL, databuf, 0x01))
	{
		GSE_ERR("read power ctl register err!\n");
		mutex_unlock(&bma222_op_mutex);
		return BMA222_ERR_I2C;
	}
	if(databuf[0]==0xff)//if the value is ff the gsensor will not work anymore, any i2c operations won't be vaild
		GSE_LOG("before bma222_init_client databuf = 0x%x\n",databuf[0]);
	if((err = bma222_init_client(obj->client, 0)))
	{
		GSE_ERR("initialize client fail!!\n");
		mutex_unlock(&bma222_op_mutex);
		return;        
	}
	
	if(bma_i2c_read_block(obj->client, BMA222_REG_POWER_CTL, databuf, 0x01)) //for debug read power control register to see the value is OK
	{
		GSE_ERR("read power ctl register err!\n");
		mutex_unlock(&bma222_op_mutex);
		return BMA222_ERR_I2C;
	}
	if(databuf[0]==0xff)//if the value is ff the gsensor will not work anymore, any i2c operations won't be vaild
		GSE_LOG("after bma222_init_client databuf = 0x%x\n",databuf[0]);
	sensor_suspend = 0;
	mutex_unlock(&bma222_op_mutex);
	atomic_set(&obj->suspend, 0);    
}
/*----------------------------------------------------------------------------*/
#endif /*USE_EARLY_SUSPEND*/
/*----------------------------------------------------------------------------*/
static int bma222_i2c_detect(struct i2c_client *client, struct i2c_board_info *info) 
{    
	strcpy(info->type, BMA222_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int bma222_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct bma222_i2c_data *obj;
	struct hwmsen_object sobj;
	int err = 0;
	int retry = 0;
	GSE_FUN();

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	
	memset(obj, 0, sizeof(struct bma222_i2c_data));

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
	
#ifdef CONFIG_BMA222_LOWPASS
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

	bma222_i2c_client = new_client;	

	for(retry = 0; retry < 3; retry++){
		if((err = bma222_init_client(new_client, 1)))
		{
			GSE_ERR("bma222_device init cilent fail time: %d\n", retry);
			continue;
		}
	}
	if(err != 0)
		goto exit_init_failed;
	

	if((err = misc_register(&bma222_device)))
	{
		GSE_ERR("bma222_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	if((err = bma222_create_attr(&bma222_gsensor_driver.driver)))
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
	obj->early_drv.suspend  = bma222_early_suspend,
	obj->early_drv.resume   = bma222_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif 

	GSE_LOG("%s: OK\n", __func__);    
	return 0;

	exit_create_attr_failed:
	misc_deregister(&bma222_device);
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
static int bma222_i2c_remove(struct i2c_client *client)
{
	int err = 0;	
	
	if((err = bma222_delete_attr(&bma222_gsensor_driver.driver)))
	{
		GSE_ERR("bma150_delete_attr fail: %d\n", err);
	}
	
	if((err = misc_deregister(&bma222_device)))
	{
		GSE_ERR("misc_deregister fail: %d\n", err);
	}

	if((err = hwmsen_detach(ID_ACCELEROMETER)))
	    

	bma222_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int bma222_probe(struct platform_device *pdev) 
{
	struct acc_hw *hw = get_cust_acc_hw();
	GSE_FUN();

	BMA222_power(hw, 1);
	//bma222_force[0] = hw->i2c_num;
	if(i2c_add_driver(&bma222_i2c_driver))
	{
		GSE_ERR("add driver error\n");
		return -1;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int bma222_remove(struct platform_device *pdev)
{
    struct acc_hw *hw = get_cust_acc_hw();

    GSE_FUN();    
    BMA222_power(hw, 0);    
    i2c_del_driver(&bma222_i2c_driver);
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

static struct platform_driver bma222_gsensor_driver = {
	.probe      = bma222_probe,
	.remove     = bma222_remove,    
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

static struct platform_driver bma222_gsensor_driver = {
	.probe      = bma222_probe,
	.remove     = bma222_remove,    
	.driver     = {
		.name  = "gsensor",
		.owner = THIS_MODULE,
	}
};
#endif
/*----------------------------------------------------------------------------*/
static int __init bma222_init(void)
{
	//GSE_FUN();
	struct acc_hw *hw = get_cust_acc_hw();
	GSE_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num);
	i2c_register_board_info(hw->i2c_num, &i2c_BMA222, 1);
	if(platform_driver_register(&bma222_gsensor_driver))
	{
		GSE_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit bma222_exit(void)
{
	GSE_FUN();
	platform_driver_unregister(&bma222_gsensor_driver);
}
/*----------------------------------------------------------------------------*/
module_init(bma222_init);
module_exit(bma222_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BMA222 I2C driver");
MODULE_AUTHOR("Xiaoli.li@mediatek.com");
