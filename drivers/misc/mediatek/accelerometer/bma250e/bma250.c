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


#define POWER_NONE_MACRO MT65XX_POWER_NONE



#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include "bma250.h"
#include <linux/hwmsen_helper.h>

#include <accel.h>
#include <linux/batch.h>
#ifdef CUSTOM_KERNEL_SENSORHUB
#include <SCP_sensorHub.h>
#endif//#ifdef CUSTOM_KERNEL_SENSORHUB

/*----------------------------------------------------------------------------*/
#define I2C_DRIVERID_BMA250 250
/*----------------------------------------------------------------------------*/
#define DEBUG 1
/*----------------------------------------------------------------------------*/
//#define CONFIG_BMA150_LOWPASS   /*apply low pass filter on output*/       
#define SW_CALIBRATION

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id bma250_i2c_id[] = {{BMA250_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_bma250={ I2C_BOARD_INFO("BMA250", (0x30>>1))};


/*the adapter id will be available in customization*/
//static unsigned short bma250_force[] = {0x00, BMA250_I2C_SLAVE_WRITE_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const bma250_forces[] = { bma250_force, NULL };
//static struct i2c_client_address_data bma250_addr_data = { .forces = bma250_forces,};

/*----------------------------------------------------------------------------*/
static int bma250_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int bma250_i2c_remove(struct i2c_client *client);
#ifdef CUSTOM_KERNEL_SENSORHUB
static int gsensor_setup_irq(void);
#endif//#ifdef CUSTOM_KERNEL_SENSORHUB

static int gsensor_local_init(void);
static int gsensor_remove(void);
static int gsensor_set_delay(u64 ns);

static DEFINE_MUTEX(gsensor_mutex);
static DEFINE_MUTEX(gsensor_scp_en_mutex);


static bool enable_status = false;

static int gsensor_init_flag =-1; // 0<==>OK -1 <==> fail


//static int bma250_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);

static struct acc_init_info bma250_init_info = {
    .name = "bma250",
    .init = gsensor_local_init,
    .uninit = gsensor_remove,
};

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
    s16 raw[C_MAX_FIR_LENGTH][BMA250_AXES_NUM];
    int sum[BMA250_AXES_NUM];
    int num;
    int idx;
};
/*----------------------------------------------------------------------------*/
struct bma250_i2c_data {
    struct i2c_client *client;
    struct acc_hw *hw;
    struct hwmsen_convert   cvt;

#ifdef CUSTOM_KERNEL_SENSORHUB
    struct work_struct	irq_work;
#endif//#ifdef CUSTOM_KERNEL_SENSORHUB
	
    /*misc*/
    struct data_resolution *reso;
    atomic_t                trace;
    atomic_t                suspend;
    atomic_t                selftest;
	atomic_t				filter;
    s16                     cali_sw[BMA250_AXES_NUM+1];

    /*data*/
    s8                      offset[BMA250_AXES_NUM+1];  /*+1: for 4-byte alignment*/
    s16                     data[BMA250_AXES_NUM+1];

#ifdef CUSTOM_KERNEL_SENSORHUB
    int                     SCP_init_done;
#endif//#ifdef CUSTOM_KERNEL_SENSORHUB

#if defined(CONFIG_BMA250_LOWPASS)
    atomic_t                firlen;
    atomic_t                fir_en;
    struct data_filter      fir;
#endif 
    /*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_drv;
#endif     
};
enum bma250_rev {
	BMA250,
	BMA250E,
};

static int bma250_revesion = BMA250;
/*----------------------------------------------------------------------------*/
static struct i2c_driver bma250_i2c_driver = {
    .driver = {
 //       .owner          = THIS_MODULE,
        .name           = BMA250_DEV_NAME,
    },
	.probe      		= bma250_i2c_probe,
	.remove    			= bma250_i2c_remove,
//	.detect				= bma250_i2c_detect,
#if !defined(CONFIG_HAS_EARLYSUSPEND)    
    .suspend            = bma250_suspend,
    .resume             = bma250_resume,
#endif
	.id_table = bma250_i2c_id,
//	.address_data = &bma250_addr_data,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *bma250_i2c_client = NULL;
static struct bma250_i2c_data *obj_i2c_data = NULL;
static bool sensor_power = true;
static GSENSOR_VECTOR3D gsensor_gain;
/* static char selftestRes[8]= {0}; */

/*----------------------------------------------------------------------------*/
#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN(f)               printk(KERN_INFO GSE_TAG"%s\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)    printk(KERN_ERR GSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    printk(KERN_INFO GSE_TAG fmt, ##args)
/*----------------------------------------------------------------------------*/
static struct data_resolution bma250_data_resolution[1] = {
 /* combination by {FULL_RES,RANGE}*/
    {{ 3, 9}, 256},   // dataformat +/-2g  in 8-bit resolution;  { 15, 6} = 15.6= (2*2*1000)/(2^8);  64 = (2^8)/(2*2)          
};
/*----------------------------------------------------------------------------*/
static struct data_resolution bma250_offset_resolution = {{3, 9}, 256};

/*--------------------BMA250 power control function----------------------------------*/
static void BMA250_power(struct acc_hw *hw, unsigned int on) 
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
			if(!hwPowerOn(hw->power_id, hw->power_vol, "BMA250"))
			{
				GSE_ERR("power on fails!!\n");
			}
		}
		else	// power off
		{
			if (!hwPowerDown(hw->power_id, "BMA250"))
			{
				GSE_ERR("power off fail!!\n");
			}			  
		}
	}
	power_on = on;    
}
/*----------------------------------------------------------------------------*/

#ifdef CUSTOM_KERNEL_SENSORHUB
int BMA250_SCP_SetPowerMode(bool enable, int sensorType)
{
    static bool gsensor_scp_en_status = false;
    static unsigned int gsensor_scp_en_map = 0;
    SCP_SENSOR_HUB_DATA req;
    int len;
    int err = 0;

    mutex_lock(&gsensor_scp_en_mutex);

    if (sensorType >= 32)
    {
        GSE_ERR("Out of index!\n");
        return -1;
    }

    if (true == enable)
    {
        gsensor_scp_en_map |= (1<<sensorType);
    }
    else
    {
        gsensor_scp_en_map &= ~(1<<sensorType);
    }

    if (0 == gsensor_scp_en_map)
        enable = false;
    else
        enable = true;

    if (gsensor_scp_en_status != enable)
    {
        gsensor_scp_en_status = enable;
        
        req.activate_req.sensorType = ID_ACCELEROMETER;
        req.activate_req.action = SENSOR_HUB_ACTIVATE;
        req.activate_req.enable = enable;
        len = sizeof(req.activate_req);
        err = SCP_sensorHub_req_send(&req, &len, 1);
        if (err)
        {
            GSE_ERR("SCP_sensorHub_req_send fail!\n");
        }
    }

    mutex_unlock(&gsensor_scp_en_mutex);

    return err;
}
EXPORT_SYMBOL(BMA250_SCP_SetPowerMode);
#endif //#ifdef CUSTOM_KERNEL_SENSORHUB

/*----------------------------------------------------------------------------*/
static int BMA250_SetDataResolution(struct bma250_i2c_data *obj)
{

/*set g sensor dataresolution here*/

/*BMA250 only can set to 10-bit dataresolution, so do nothing in bma250 driver here*/

/*end of set dataresolution*/


 
 /*we set measure range from -2g to +2g in BMA150_SetDataFormat(client, BMA150_RANGE_2G), 
                                                    and set 10-bit dataresolution BMA150_SetDataResolution()*/
                                                    
 /*so bma250_data_resolution[0] set value as {{ 3, 9}, 256} when declaration, and assign the value to obj->reso here*/  

 	obj->reso = &bma250_data_resolution[0];
	return 0;
	
/*if you changed the measure range, for example call: BMA250_SetDataFormat(client, BMA150_RANGE_4G), 
you must set the right value to bma250_data_resolution*/

}
/*----------------------------------------------------------------------------*/
static int BMA250_ReadData(struct i2c_client *client, s16 data[BMA250_AXES_NUM])
{
	struct bma250_i2c_data *priv = i2c_get_clientdata(client);        
	u8 addr = BMA250_REG_DATAXLOW;
	u8 buf[BMA250_DATA_LEN] = {0};
	int err = 0;
	int i;

	if(NULL == client)
	{
		err = -EINVAL;
		return err;
	}
	
	err = hwmsen_read_block(client, addr, buf, 0x06);
	if(err)
	{
		GSE_ERR("error: %d\n", err);
	}
	else
	{
		data[BMA250_AXIS_X] = (s16)((buf[BMA250_AXIS_X*2] >> 6) |
		         (buf[BMA250_AXIS_X*2+1] << 2));
		data[BMA250_AXIS_Y] = (s16)((buf[BMA250_AXIS_Y*2] >> 6) |
		         (buf[BMA250_AXIS_Y*2+1] << 2));
		data[BMA250_AXIS_Z] = (s16)((buf[BMA250_AXIS_Z*2] >> 6) |
		         (buf[BMA250_AXIS_Z*2+1] << 2));

		for(i=0;i<3;i++)				
		{								//because the data is store in binary complement number formation in computer system
			if ( data[i] == 0x0200 )	//so we want to calculate actual number here
				data[i]= -512;			//10bit resolution, 512= 2^(10-1)
			else if ( data[i] & 0x0200 )//transfor format
			{							//printk("data 0 step %x \n",data[i]);
				data[i] -= 0x1;			//printk("data 1 step %x \n",data[i]);
				data[i] = ~data[i];		//printk("data 2 step %x \n",data[i]);
				data[i] &= 0x01ff;		//printk("data 3 step %x \n\n",data[i]);
				data[i] = -data[i];		
			}
		}	

		if(atomic_read(&priv->trace) & ADX_TRC_RAWDATA)
		{
			GSE_LOG("[%08X %08X %08X] => [%5d %5d %5d] after\n", data[BMA250_AXIS_X], data[BMA250_AXIS_Y], data[BMA250_AXIS_Z],
		                               data[BMA250_AXIS_X], data[BMA250_AXIS_Y], data[BMA250_AXIS_Z]);
		}
#ifdef CONFIG_BMA250_LOWPASS
		if(atomic_read(&priv->filter))
		{
			if(atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend))
			{
				int idx, firlen = atomic_read(&priv->firlen);   
				if(priv->fir.num < firlen)
				{                
					priv->fir.raw[priv->fir.num][BMA250_AXIS_X] = data[BMA250_AXIS_X];
					priv->fir.raw[priv->fir.num][BMA250_AXIS_Y] = data[BMA250_AXIS_Y];
					priv->fir.raw[priv->fir.num][BMA250_AXIS_Z] = data[BMA250_AXIS_Z];
					priv->fir.sum[BMA250_AXIS_X] += data[BMA250_AXIS_X];
					priv->fir.sum[BMA250_AXIS_Y] += data[BMA250_AXIS_Y];
					priv->fir.sum[BMA250_AXIS_Z] += data[BMA250_AXIS_Z];
					if(atomic_read(&priv->trace) & ADX_TRC_FILTER)
					{
						GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d]\n", priv->fir.num,
							priv->fir.raw[priv->fir.num][BMA250_AXIS_X], priv->fir.raw[priv->fir.num][BMA250_AXIS_Y], priv->fir.raw[priv->fir.num][BMA250_AXIS_Z],
							priv->fir.sum[BMA250_AXIS_X], priv->fir.sum[BMA250_AXIS_Y], priv->fir.sum[BMA250_AXIS_Z]);
					}
					priv->fir.num++;
					priv->fir.idx++;
				}
				else
				{
					idx = priv->fir.idx % firlen;
					priv->fir.sum[BMA250_AXIS_X] -= priv->fir.raw[idx][BMA250_AXIS_X];
					priv->fir.sum[BMA250_AXIS_Y] -= priv->fir.raw[idx][BMA250_AXIS_Y];
					priv->fir.sum[BMA250_AXIS_Z] -= priv->fir.raw[idx][BMA250_AXIS_Z];
					priv->fir.raw[idx][BMA250_AXIS_X] = data[BMA250_AXIS_X];
					priv->fir.raw[idx][BMA250_AXIS_Y] = data[BMA250_AXIS_Y];
					priv->fir.raw[idx][BMA250_AXIS_Z] = data[BMA250_AXIS_Z];
					priv->fir.sum[BMA250_AXIS_X] += data[BMA250_AXIS_X];
					priv->fir.sum[BMA250_AXIS_Y] += data[BMA250_AXIS_Y];
					priv->fir.sum[BMA250_AXIS_Z] += data[BMA250_AXIS_Z];
					priv->fir.idx++;
					data[BMA250_AXIS_X] = priv->fir.sum[BMA250_AXIS_X]/firlen;
					data[BMA250_AXIS_Y] = priv->fir.sum[BMA250_AXIS_Y]/firlen;
					data[BMA250_AXIS_Z] = priv->fir.sum[BMA250_AXIS_Z]/firlen;
					if(atomic_read(&priv->trace) & ADX_TRC_FILTER)
					{
						GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d] : [%5d %5d %5d]\n", idx,
						priv->fir.raw[idx][BMA250_AXIS_X], priv->fir.raw[idx][BMA250_AXIS_Y], priv->fir.raw[idx][BMA250_AXIS_Z],
						priv->fir.sum[BMA250_AXIS_X], priv->fir.sum[BMA250_AXIS_Y], priv->fir.sum[BMA250_AXIS_Z],
						data[BMA250_AXIS_X], data[BMA250_AXIS_Y], data[BMA250_AXIS_Z]);
					}
				}
			}
		}	
#endif         
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int BMA250_ReadOffset(struct i2c_client *client, s8 ofs[BMA250_AXES_NUM])
{    
	int err = 0;
#ifdef SW_CALIBRATION
	ofs[0]=ofs[1]=ofs[2]=0x0;
#else
	if(err = hwmsen_read_block(client, BMA250_REG_OFSX, ofs, BMA250_AXES_NUM))
	{
		GSE_ERR("error: %d\n", err);
	}
#endif
	//printk("offesx=%x, y=%x, z=%x",ofs[0],ofs[1],ofs[2]);
	
	return err;    
}
/*----------------------------------------------------------------------------*/
static int BMA250_ResetCalibration(struct i2c_client *client)
{
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
	#ifndef SW_CALIBRATION
	u8 ofs[4]={0,0,0,0};
	#endif
	int err = 0;

#ifdef CUSTOM_KERNEL_SENSORHUB
    SCP_SENSOR_HUB_DATA data;
    BMA250_CUST_DATA *pCustData;
    unsigned int len;
	
	if (0 != obj->SCP_init_done)
	{
		pCustData = (BMA250_CUST_DATA *)&data.set_cust_req.custData;
		data.set_cust_req.sensorType = ID_ACCELEROMETER;
		data.set_cust_req.action = SENSOR_HUB_SET_CUST;
		pCustData->resetCali.action = BMA250_CUST_ACTION_RESET_CALI;
		len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->resetCali);
		SCP_sensorHub_req_send(&data, &len, 1);
	}
#endif

	#ifdef SW_CALIBRATION
		
	#else
		if(err = hwmsen_write_block(client, BMA250_REG_OFSX, ofs, 4))
		{
			GSE_ERR("error: %d\n", err);
		}
	#endif

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	memset(obj->offset, 0x00, sizeof(obj->offset));
	return err;    
}
/*----------------------------------------------------------------------------*/
static int BMA250_ReadCalibration(struct i2c_client *client, int dat[BMA250_AXES_NUM])
{
    struct bma250_i2c_data *obj = i2c_get_clientdata(client);
    #ifndef SW_CALIBRATION
    int err;
    #endif
    int mul;

	#ifdef SW_CALIBRATION
		mul = 0;//only SW Calibration, disable HW Calibration
	#else
	    if ((err = BMA250_ReadOffset(client, obj->offset))) {
        GSE_ERR("read offset fail, %d\n", err);
        return err;
    	}    
    	mul = obj->reso->sensitivity/bma250_offset_resolution.sensitivity;
	#endif

    dat[obj->cvt.map[BMA250_AXIS_X]] = obj->cvt.sign[BMA250_AXIS_X]*(obj->offset[BMA250_AXIS_X]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[BMA250_AXIS_X]);
    dat[obj->cvt.map[BMA250_AXIS_Y]] = obj->cvt.sign[BMA250_AXIS_Y]*(obj->offset[BMA250_AXIS_Y]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[BMA250_AXIS_Y]);
    dat[obj->cvt.map[BMA250_AXIS_Z]] = obj->cvt.sign[BMA250_AXIS_Z]*(obj->offset[BMA250_AXIS_Z]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[BMA250_AXIS_Z]);                        
                                       
    return 0;
}
/*----------------------------------------------------------------------------*/
static int BMA250_ReadCalibrationEx(struct i2c_client *client, int act[BMA250_AXES_NUM], int raw[BMA250_AXES_NUM])
{  
	/*raw: the raw calibration data; act: the actual calibration data*/
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
	#ifndef SW_CALIBRATION
	int err;
	#endif
	int mul;

 

	#ifdef SW_CALIBRATION
		mul = 0;//only SW Calibration, disable HW Calibration
	#else
		if(err = BMA250_ReadOffset(client, obj->offset))
		{
			GSE_ERR("read offset fail, %d\n", err);
			return err;
		}   
		mul = obj->reso->sensitivity/bma250_offset_resolution.sensitivity;
	#endif
	
	raw[BMA250_AXIS_X] = obj->offset[BMA250_AXIS_X]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[BMA250_AXIS_X];
	raw[BMA250_AXIS_Y] = obj->offset[BMA250_AXIS_Y]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[BMA250_AXIS_Y];
	raw[BMA250_AXIS_Z] = obj->offset[BMA250_AXIS_Z]*mul * GRAVITY_EARTH_1000 / obj->reso->sensitivity + obj->cali_sw[BMA250_AXIS_Z];

	act[obj->cvt.map[BMA250_AXIS_X]] = obj->cvt.sign[BMA250_AXIS_X]*raw[BMA250_AXIS_X];
	act[obj->cvt.map[BMA250_AXIS_Y]] = obj->cvt.sign[BMA250_AXIS_Y]*raw[BMA250_AXIS_Y];
	act[obj->cvt.map[BMA250_AXIS_Z]] = obj->cvt.sign[BMA250_AXIS_Z]*raw[BMA250_AXIS_Z];                        
	                       
	return 0;
}
/*----------------------------------------------------------------------------*/
static int BMA250_WriteCalibration(struct i2c_client *client, int dat[BMA250_AXES_NUM])
{
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
	int err;
	int cali[BMA250_AXES_NUM], raw[BMA250_AXES_NUM];
	#ifndef SW_CALIBRATION
	int lsb = bma250_offset_resolution.sensitivity;
	int divisor = obj->reso->sensitivity/lsb;
	#endif

#ifdef CUSTOM_KERNEL_SENSORHUB
	SCP_SENSOR_HUB_DATA data;
	BMA250_CUST_DATA *pCustData;
	unsigned int len;
#endif

	err = BMA250_ReadCalibrationEx(client, cali, raw);
	if(err)	/*offset will be updated in obj->offset*/
	{ 
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}

	GSE_LOG("OLDOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n", 
		raw[BMA250_AXIS_X], raw[BMA250_AXIS_Y], raw[BMA250_AXIS_Z],
		obj->offset[BMA250_AXIS_X], obj->offset[BMA250_AXIS_Y], obj->offset[BMA250_AXIS_Z],
		obj->cali_sw[BMA250_AXIS_X], obj->cali_sw[BMA250_AXIS_Y], obj->cali_sw[BMA250_AXIS_Z]);

#ifdef CUSTOM_KERNEL_SENSORHUB
	pCustData = (BMA250_CUST_DATA *)data.set_cust_req.custData;
	data.set_cust_req.sensorType = ID_ACCELEROMETER;
	data.set_cust_req.action = SENSOR_HUB_SET_CUST;
	pCustData->setCali.action = BMA250_CUST_ACTION_SET_CALI;
	pCustData->setCali.data[BMA250_AXIS_X] = dat[BMA250_AXIS_X];
	pCustData->setCali.data[BMA250_AXIS_Y] = dat[BMA250_AXIS_Y];
	pCustData->setCali.data[BMA250_AXIS_Z] = dat[BMA250_AXIS_Z];
	len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(pCustData->setCali);
	SCP_sensorHub_req_send(&data, &len, 1);
#endif

	/*calculate the real offset expected by caller*/
	cali[BMA250_AXIS_X] += dat[BMA250_AXIS_X];
	cali[BMA250_AXIS_Y] += dat[BMA250_AXIS_Y];
	cali[BMA250_AXIS_Z] += dat[BMA250_AXIS_Z];

	GSE_LOG("UPDATE: (%+3d %+3d %+3d)\n", 
		dat[BMA250_AXIS_X], dat[BMA250_AXIS_Y], dat[BMA250_AXIS_Z]);

#ifdef SW_CALIBRATION
	obj->cali_sw[BMA250_AXIS_X] = obj->cvt.sign[BMA250_AXIS_X]*(cali[obj->cvt.map[BMA250_AXIS_X]]);
	obj->cali_sw[BMA250_AXIS_Y] = obj->cvt.sign[BMA250_AXIS_Y]*(cali[obj->cvt.map[BMA250_AXIS_Y]]);
	obj->cali_sw[BMA250_AXIS_Z] = obj->cvt.sign[BMA250_AXIS_Z]*(cali[obj->cvt.map[BMA250_AXIS_Z]]);	
#else
	obj->offset[BMA250_AXIS_X] = (s8)(obj->cvt.sign[BMA250_AXIS_X]*(cali[obj->cvt.map[BMA250_AXIS_X]]) * obj->reso->sensitivity / GRAVITY_EARTH_1000/(divisor));
	obj->offset[BMA250_AXIS_Y] = (s8)(obj->cvt.sign[BMA250_AXIS_Y]*(cali[obj->cvt.map[BMA250_AXIS_Y]]) * obj->reso->sensitivity / GRAVITY_EARTH_1000/(divisor));
	obj->offset[BMA250_AXIS_Z] = (s8)(obj->cvt.sign[BMA250_AXIS_Z]*(cali[obj->cvt.map[BMA250_AXIS_Z]]) * obj->reso->sensitivity / GRAVITY_EARTH_1000/(divisor));

	/*convert software calibration using standard calibration*/
	obj->cali_sw[BMA250_AXIS_X] = obj->cvt.sign[BMA250_AXIS_X]*(cali[obj->cvt.map[BMA250_AXIS_X]])%(divisor);
	obj->cali_sw[BMA250_AXIS_Y] = obj->cvt.sign[BMA250_AXIS_Y]*(cali[obj->cvt.map[BMA250_AXIS_Y]])%(divisor);
	obj->cali_sw[BMA250_AXIS_Z] = obj->cvt.sign[BMA250_AXIS_Z]*(cali[obj->cvt.map[BMA250_AXIS_Z]])%(divisor);

	GSE_LOG("NEWOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n", 
		obj->offset[BMA250_AXIS_X]*divisor + obj->cali_sw[BMA250_AXIS_X], 
		obj->offset[BMA250_AXIS_Y]*divisor + obj->cali_sw[BMA250_AXIS_Y], 
		obj->offset[BMA250_AXIS_Z]*divisor + obj->cali_sw[BMA250_AXIS_Z], 
		obj->offset[BMA250_AXIS_X], obj->offset[BMA250_AXIS_Y], obj->offset[BMA250_AXIS_Z],
		obj->cali_sw[BMA250_AXIS_X], obj->cali_sw[BMA250_AXIS_Y], obj->cali_sw[BMA250_AXIS_Z]);

	if(err = hwmsen_write_block(obj->client, BMA250_REG_OFSX, obj->offset, BMA250_AXES_NUM))
	{
		GSE_ERR("write offset fail: %d\n", err);
		return err;
	}
#endif

	return err;
}
/*----------------------------------------------------------------------------*/
static int BMA250_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf[2];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*2);    
	databuf[0] = BMA250_REG_DEVID;    
	i2c_master_send(client, databuf, 0x1);
	mdelay(40);

	databuf[0] = 0x0;        
	res = i2c_master_recv(client, databuf, 0x01);
	if(res <= 0)
	{
		i2c_master_send(client, databuf, 0x1);

		mdelay(40);

		databuf[0] = 0x0;        
		res = i2c_master_recv(client, databuf, 0x01);
		if(res <= 0)
		{
			goto exit_BMA250_CheckDeviceID;
		}
	}


	if(databuf[0]==BMA250_FIXED_DEVID)
	{
		printk("BMA250_CheckDeviceID %4xh pass!\n ", databuf[0]);
		bma250_revesion = BMA250;
	}
	else if (databuf[0]==BMA250E_FIXED_DEVID)
	{
		printk("BMA250E_CheckDeviceID %4xh pass!\n ", databuf[0]);
		bma250_revesion = BMA250E;
	}
	else
	{
		printk("BMA250_CheckDeviceID %d fail!\n ", databuf[0]);
	}

	exit_BMA250_CheckDeviceID:
	if (res <= 0)
	{
		return BMA250_ERR_I2C;
	}
	
	return BMA250_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int BMA250_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2];    
	int res = 0;
	u8 addr = BMA250_REG_POWER_CTL;
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
	
	
	if(enable == sensor_power )
	{
		GSE_LOG("Sensor power status is newest!\n");
		return BMA250_SUCCESS;
	}

	if(hwmsen_read_block(client, addr, databuf, 0x01))
	{
		GSE_ERR("read power ctl register err!\n");
		return BMA250_ERR_I2C;
	}

	
	if(enable == TRUE)
	{
		databuf[0] &= ~BMA250_MEASURE_MODE;
	}
	else
	{
		databuf[0] |= BMA250_MEASURE_MODE;
	}
	databuf[1] = databuf[0];
	databuf[0] = BMA250_REG_POWER_CTL;
	

	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		GSE_LOG("set power mode failed!\n");
		return BMA250_ERR_I2C;
	}
	else if(atomic_read(&obj->trace) & ADX_TRC_INFO)
	{
		GSE_LOG("set power mode ok %d!\n", databuf[1]);
	}

	//GSE_LOG("BMA250_SetPowerMode ok!\n");


	sensor_power = enable;

	mdelay(20);
	
	return BMA250_SUCCESS;    
}
/*----------------------------------------------------------------------------*/
static int BMA250_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);    

	if(hwmsen_read_block(client, BMA250_REG_DATA_FORMAT, databuf, 0x01))
	{
		printk("bma250 read Dataformat failt \n");
		return BMA250_ERR_I2C;
	}

	databuf[0] &= ~BMA250_RANGE_MASK;
	databuf[0] |= dataformat;
	databuf[1] = databuf[0];
	databuf[0] = BMA250_REG_DATA_FORMAT;


	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		return BMA250_ERR_I2C;
	}
	
	//printk("BMA250_SetDataFormat OK! \n");
	

	return BMA250_SetDataResolution(obj);    
}
/*----------------------------------------------------------------------------*/
static int BMA250_SetBWRate(struct i2c_client *client, u8 bwrate)
{
	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);    

	if(hwmsen_read_block(client, BMA250_REG_BW_RATE, databuf, 0x01))
	{
		printk("bma250 read rate failt \n");
		return BMA250_ERR_I2C;
	}

	databuf[0] &= ~BMA250_BW_MASK;
	databuf[0] |= bwrate;
	databuf[1] = databuf[0];
	databuf[0] = BMA250_REG_BW_RATE;


	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		return BMA250_ERR_I2C;
	}
	
	//printk("BMA250_SetBWRate OK! \n");
	
	return BMA250_SUCCESS;    
}
/*----------------------------------------------------------------------------*/
static int BMA250_SetIntEnable(struct i2c_client *client, u8 intenable)
{
			int res = 0;
		
			res = hwmsen_write_byte(client, BMA250_INT_REG_1, 0x00);
			if(res != BMA250_SUCCESS) 
			{
				return res;
			}
			res = hwmsen_write_byte(client, BMA250_INT_REG_2, 0x00);
			if(res != BMA250_SUCCESS) 
			{
				return res;
			}
			printk("BMA250 disable interrupt ...\n");
		
			/*for disable interrupt function*/
			
			return BMA250_SUCCESS;	  
}


/*----------------------------------------------------------------------------*/
static int bma250_init_client(struct i2c_client *client, int reset_cali)
{
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;
	printk("bma250_init_client \n");

	res = BMA250_CheckDeviceID(client); 
	if(res != BMA250_SUCCESS)
	{
		return res;
	}	
	printk("BMA250_CheckDeviceID ok \n");
	
	res = BMA250_SetBWRate(client, BMA250_BW_100HZ);
	if(res != BMA250_SUCCESS ) 
	{
		return res;
	}
	printk("BMA250_SetBWRate OK!\n");
	
	res = BMA250_SetDataFormat(client, BMA250_RANGE_2G);
	if(res != BMA250_SUCCESS) 
	{
		return res;
	}
	printk("BMA250_SetDataFormat OK!\n");

	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = obj->reso->sensitivity;


#ifdef CUSTOM_KERNEL_SENSORHUB
	res = gsensor_setup_irq();
	if(res != BMA250_SUCCESS)
	{
		return res;
	}
#endif//#ifdef CUSTOM_KERNEL_SENSORHUB

	res = BMA250_SetIntEnable(client, 0x00);        
	if(res != BMA250_SUCCESS)
	{
		return res;
	}
	printk("BMA250 disable interrupt function!\n");

	res = BMA250_SetPowerMode(client, false);
	if(res != BMA250_SUCCESS)
	{
		return res;
	}
	printk("BMA250_SetPowerMode OK!\n");


	if(0 != reset_cali)
	{ 
		/*reset calibration only in power on*/
		res = BMA250_ResetCalibration(client);
		if(res != BMA250_SUCCESS)
		{
			return res;
		}
	}
	printk("bma250_init_client OK!\n");
#ifdef CONFIG_BMA250_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));  
#endif

	mdelay(20);

	return BMA250_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int BMA250_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
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

	sprintf(buf, "BMA250 Chip");
	return 0;
}
/*----------------------------------------------------------------------------*/
static int BMA250_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
	struct bma250_i2c_data *obj = (struct bma250_i2c_data*)i2c_get_clientdata(client);
	u8 databuf[20];
	int acc[BMA250_AXES_NUM];
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

	if(sensor_power == FALSE)
	{
		res = BMA250_SetPowerMode(client, true);
		if(res)
		{
			GSE_ERR("Power on bma250 error %d!\n", res);
		}
	}

	res = BMA250_ReadData(client, obj->data);
	if(res)
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return -3;
	}
	else
	{
	#if 1
		obj->data[BMA250_AXIS_X] = obj->data[BMA250_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		obj->data[BMA250_AXIS_Y] = obj->data[BMA250_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		obj->data[BMA250_AXIS_Z] = obj->data[BMA250_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;		
	#endif	
		//printk("raw data x=%d, y=%d, z=%d \n",obj->data[BMA150_AXIS_X],obj->data[BMA150_AXIS_Y],obj->data[BMA150_AXIS_Z]);
		obj->data[BMA250_AXIS_X] += obj->cali_sw[BMA250_AXIS_X];
		obj->data[BMA250_AXIS_Y] += obj->cali_sw[BMA250_AXIS_Y];
		obj->data[BMA250_AXIS_Z] += obj->cali_sw[BMA250_AXIS_Z];
		
		//printk("cali_sw x=%d, y=%d, z=%d \n",obj->cali_sw[BMA150_AXIS_X],obj->cali_sw[BMA150_AXIS_Y],obj->cali_sw[BMA150_AXIS_Z]);
		
		/*remap coordinate*/
		acc[obj->cvt.map[BMA250_AXIS_X]] = obj->cvt.sign[BMA250_AXIS_X]*obj->data[BMA250_AXIS_X];
		acc[obj->cvt.map[BMA250_AXIS_Y]] = obj->cvt.sign[BMA250_AXIS_Y]*obj->data[BMA250_AXIS_Y];
		acc[obj->cvt.map[BMA250_AXIS_Z]] = obj->cvt.sign[BMA250_AXIS_Z]*obj->data[BMA250_AXIS_Z];
		//printk("cvt x=%d, y=%d, z=%d \n",obj->cvt.sign[BMA150_AXIS_X],obj->cvt.sign[BMA150_AXIS_Y],obj->cvt.sign[BMA150_AXIS_Z]);


		//GSE_LOG("Mapped gsensor data: %d, %d, %d!\n", acc[BMA150_AXIS_X], acc[BMA150_AXIS_Y], acc[BMA150_AXIS_Z]);

		//Out put the mg
		//printk("mg acc=%d, GRAVITY=%d, sensityvity=%d \n",acc[BMA150_AXIS_X],GRAVITY_EARTH_1000,obj->reso->sensitivity);
#if 0
		acc[BMA250_AXIS_X] = acc[BMA250_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		acc[BMA250_AXIS_Y] = acc[BMA250_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		acc[BMA250_AXIS_Z] = acc[BMA250_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;		
	#endif	
	

		sprintf(buf, "%04x %04x %04x", acc[BMA250_AXIS_X], acc[BMA250_AXIS_Y], acc[BMA250_AXIS_Z]);
		if(atomic_read(&obj->trace) & ADX_TRC_IOCTL)
		{
			GSE_LOG("gsensor data: %s!\n", buf);
		}
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int BMA250_ReadRawData(struct i2c_client *client, char *buf)
{
	struct bma250_i2c_data *obj = (struct bma250_i2c_data*)i2c_get_clientdata(client);
	int res = 0;

	if (!buf || !client)
	{
		return EINVAL;
	}
	
	res = BMA250_ReadData(client, obj->data);
	if(res)
	{        
		GSE_ERR("I2C error: ret value=%d", res);
		return EIO;
	}
	else
	{
		sprintf(buf, "BMA250_ReadRawData %04x %04x %04x", obj->data[BMA250_AXIS_X], 
			obj->data[BMA250_AXIS_Y], obj->data[BMA250_AXIS_Z]);
	
	}
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma250_i2c_client;
	char strbuf[BMA250_BUFSIZE];
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	
	BMA250_ReadChipInfo(client, strbuf, BMA250_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);        
}


/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma250_i2c_client;
	char strbuf[BMA250_BUFSIZE];
	
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	BMA250_ReadSensorData(client, strbuf, BMA250_BUFSIZE);
	//BMA150_ReadRawData(client, strbuf);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);            
}


/*----------------------------------------------------------------------------*/
static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = bma250_i2c_client;
	struct bma250_i2c_data *obj;
	int err, len = 0, mul;
	int tmp[BMA250_AXES_NUM];

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	obj = i2c_get_clientdata(client);

	err = BMA250_ReadOffset(client, obj->offset);
	if(err)
	{
		return -EINVAL;
	}
	err = BMA250_ReadCalibration(client, tmp);
	if(err)
	{
		return -EINVAL;
	}
	else
	{    
		mul = obj->reso->sensitivity/bma250_offset_resolution.sensitivity;
		len += snprintf(buf+len, PAGE_SIZE-len, "[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n", mul,                        
			obj->offset[BMA250_AXIS_X], obj->offset[BMA250_AXIS_Y], obj->offset[BMA250_AXIS_Z],
			obj->offset[BMA250_AXIS_X], obj->offset[BMA250_AXIS_Y], obj->offset[BMA250_AXIS_Z]);
		len += snprintf(buf+len, PAGE_SIZE-len, "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1, 
			obj->cali_sw[BMA250_AXIS_X], obj->cali_sw[BMA250_AXIS_Y], obj->cali_sw[BMA250_AXIS_Z]);

		len += snprintf(buf+len, PAGE_SIZE-len, "[ALL]    (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n", 
			obj->offset[BMA250_AXIS_X]*mul + obj->cali_sw[BMA250_AXIS_X],
			obj->offset[BMA250_AXIS_Y]*mul + obj->cali_sw[BMA250_AXIS_Y],
			obj->offset[BMA250_AXIS_Z]*mul + obj->cali_sw[BMA250_AXIS_Z],
			tmp[BMA250_AXIS_X], tmp[BMA250_AXIS_Y], tmp[BMA250_AXIS_Z]);
		
		return len;
    }
}
/*----------------------------------------------------------------------------*/
static ssize_t store_cali_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct i2c_client *client = bma250_i2c_client;  
	int err, x, y, z;
	int dat[BMA250_AXES_NUM];

	if(!strncmp(buf, "rst", 3))
	{
		err = BMA250_ResetCalibration(client);
		if(err)
		{
			GSE_ERR("reset offset err = %d\n", err);
		}	
	}
	else if(3 == sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z))
	{
		dat[BMA250_AXIS_X] = x;
		dat[BMA250_AXIS_Y] = y;
		dat[BMA250_AXIS_Z] = z;
		err = BMA250_WriteCalibration(client, dat);
		if(err)
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
static ssize_t show_firlen_value(struct device_driver *ddri, char *buf)
{
#ifdef CONFIG_BMA250_LOWPASS
	struct i2c_client *client = bma250_i2c_client;
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
	if(atomic_read(&obj->firlen))
	{
		int idx, len = atomic_read(&obj->firlen);
		GSE_LOG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);

		for(idx = 0; idx < len; idx++)
		{
			GSE_LOG("[%5d %5d %5d]\n", obj->fir.raw[idx][BMA250_AXIS_X], obj->fir.raw[idx][BMA250_AXIS_Y], obj->fir.raw[idx][BMA250_AXIS_Z]);
		}
		
		GSE_LOG("sum = [%5d %5d %5d]\n", obj->fir.sum[BMA250_AXIS_X], obj->fir.sum[BMA250_AXIS_Y], obj->fir.sum[BMA250_AXIS_Z]);
		GSE_LOG("avg = [%5d %5d %5d]\n", obj->fir.sum[BMA250_AXIS_X]/len, obj->fir.sum[BMA250_AXIS_Y]/len, obj->fir.sum[BMA250_AXIS_Z]/len);
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->firlen));
#else
	return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}
/*----------------------------------------------------------------------------*/
static ssize_t store_firlen_value(struct device_driver *ddri, const char *buf, size_t count)
{
#ifdef CONFIG_BMA250_LOWPASS
	struct i2c_client *client = bma250_i2c_client;  
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);
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
	struct bma250_i2c_data *obj = obj_i2c_data;
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
	struct bma250_i2c_data *obj = obj_i2c_data;
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
	struct bma250_i2c_data *obj = obj_i2c_data;
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
		printk("G sensor is in work mode, sensor_power = %d\n", sensor_power);
	else
		printk("G sensor is in standby mode, sensor_power = %d\n", sensor_power);

	return 0;
}
/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo,   S_IWUSR | S_IRUGO, show_chipinfo_value,      NULL);
static DRIVER_ATTR(sensordata, S_IWUSR | S_IRUGO, show_sensordata_value,    NULL);
static DRIVER_ATTR(cali, S_IWUSR | S_IRUGO, show_cali_value, store_cali_value);
static DRIVER_ATTR(firlen,     S_IWUSR | S_IRUGO, show_firlen_value, store_firlen_value);
static DRIVER_ATTR(trace,      S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);
static DRIVER_ATTR(status,               S_IRUGO, show_status_value,        NULL);
static DRIVER_ATTR(powerstatus,               S_IRUGO, show_power_status_value,        NULL);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *bma250_attr_list[] = {
	&driver_attr_chipinfo,     /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/
	&driver_attr_cali,         /*show calibration data*/
	&driver_attr_firlen,       /*filter length: 0: disable, others: enable*/
	&driver_attr_trace,        /*trace log*/
	&driver_attr_status,
	&driver_attr_powerstatus,
};
/*----------------------------------------------------------------------------*/
static int bma250_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(bma250_attr_list)/sizeof(bma250_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		err = driver_create_file(driver, bma250_attr_list[idx]);
		if(err)
		{            
			GSE_ERR("driver_create_file (%s) = %d\n", bma250_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int bma250_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(bma250_attr_list)/sizeof(bma250_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, bma250_attr_list[idx]);
	}
	

	return err;
}

/*----------------------------------------------------------------------------*/
#ifdef CUSTOM_KERNEL_SENSORHUB
static void gsensor_irq_work(struct work_struct *work)
{
    struct bma250_i2c_data *obj = obj_i2c_data;
    struct scp_acc_hw scp_hw;
    BMA250_CUST_DATA *p_cust_data;
    SCP_SENSOR_HUB_DATA data;
    int max_cust_data_size_per_packet;
    int i;
    uint sizeOfCustData;
    uint len;
    char *p = (char *)&scp_hw;

    GSE_FUN();

    scp_hw.i2c_num = obj->hw->i2c_num;
    scp_hw.direction = obj->hw->direction;
    scp_hw.power_id = obj->hw->power_id;
    scp_hw.power_vol = obj->hw->power_vol;
    scp_hw.firlen = obj->hw->firlen;
    memcpy(scp_hw.i2c_addr, obj->hw->i2c_addr, sizeof(obj->hw->i2c_addr));
    scp_hw.power_vio_id = obj->hw->power_vio_id;
    scp_hw.power_vio_vol = obj->hw->power_vio_vol;
    scp_hw.is_batch_supported = obj->hw->is_batch_supported;

    p_cust_data = (BMA250_CUST_DATA *)data.set_cust_req.custData;
    sizeOfCustData = sizeof(scp_hw);
    max_cust_data_size_per_packet = sizeof(data.set_cust_req.custData) - offsetof(BMA250_SET_CUST, data);
    
    for (i=0;sizeOfCustData>0;i++)
    {
        data.set_cust_req.sensorType = ID_ACCELEROMETER;
        data.set_cust_req.action = SENSOR_HUB_SET_CUST;
        p_cust_data->setCust.action = BMA250_CUST_ACTION_SET_CUST;
        p_cust_data->setCust.part = i;
        if (sizeOfCustData > max_cust_data_size_per_packet)
        {
            len = max_cust_data_size_per_packet;
        }
        else
        {
            len = sizeOfCustData;
        }

        memcpy(p_cust_data->setCust.data, p, len);
        sizeOfCustData -= len;
        p += len;
        
        len += offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + offsetof(BMA250_SET_CUST, data);
        SCP_sensorHub_req_send(&data, &len, 1);
    }

    p_cust_data = (BMA250_CUST_DATA *)&data.set_cust_req.custData;

    data.set_cust_req.sensorType = ID_ACCELEROMETER;
    data.set_cust_req.action = SENSOR_HUB_SET_CUST;
    p_cust_data->resetCali.action = BMA250_CUST_ACTION_RESET_CALI;
    len = offsetof(SCP_SENSOR_HUB_SET_CUST_REQ, custData) + sizeof(p_cust_data->resetCali);
    SCP_sensorHub_req_send(&data, &len, 1);

    obj->SCP_init_done = 1;
}
/*----------------------------------------------------------------------------*/
static int gsensor_irq_handler(void* data, uint len)
{
	struct bma250_i2c_data *obj = obj_i2c_data;
    SCP_SENSOR_HUB_DATA_P rsp = (SCP_SENSOR_HUB_DATA_P)data;

    GSE_FUN();
    GSE_ERR("len = %d, type = %d, action = %d, errCode = %d\n", len, rsp->rsp.sensorType, rsp->rsp.action, rsp->rsp.errCode);
    
	if(!obj)
	{
		return -1;
	}

    switch(rsp->rsp.action)
    {
        case SENSOR_HUB_NOTIFY:
            switch(rsp->notify_rsp.event)
            {
                case SCP_INIT_DONE:
                    schedule_work(&obj->irq_work);
                    //schedule_delayed_work(&obj->irq_work, HZ);
                    break;
                default:
                    GSE_ERR("Error sensor hub notify");
                    break;
            }
            break;
        default:
            GSE_ERR("Error sensor hub action");
            break;
    }

    return 0;
}

static int gsensor_setup_irq()
{
    int err = 0;
    
#ifdef GSENSOR_UT
    GSE_FUN();
#endif

    err = SCP_sensorHub_rsp_registration(ID_ACCELEROMETER, gsensor_irq_handler);
    
	return err;
}
#endif//#ifdef CUSTOM_KERNEL_SENSORHUB


/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int bma250_open(struct inode *inode, struct file *file)
{
	file->private_data = bma250_i2c_client;

	if(file->private_data == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int bma250_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
//static int bma250_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
//       unsigned long arg)
static long bma250_unlocked_ioctl(struct file *file, unsigned int cmd,unsigned long arg)       
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct bma250_i2c_data *obj = (struct bma250_i2c_data*)i2c_get_clientdata(client);	
	char strbuf[BMA250_BUFSIZE];
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
			bma250_init_client(client, 0);			
			break;

		case GSENSOR_IOCTL_READ_CHIPINFO:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			
			BMA250_ReadChipInfo(client, strbuf, BMA250_BUFSIZE);
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
			
			BMA250_ReadSensorData(client, strbuf, BMA250_BUFSIZE);
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
			BMA250_ReadRawData(client, strbuf);
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
				cali[BMA250_AXIS_X] = sensor_data.x * obj->reso->sensitivity / GRAVITY_EARTH_1000;
				cali[BMA250_AXIS_Y] = sensor_data.y * obj->reso->sensitivity / GRAVITY_EARTH_1000;
				cali[BMA250_AXIS_Z] = sensor_data.z * obj->reso->sensitivity / GRAVITY_EARTH_1000;			  
			#else
			cali[BMA250_AXIS_X] = sensor_data.x;
			cali[BMA250_AXIS_Y] = sensor_data.y;
			cali[BMA250_AXIS_Z] = sensor_data.z;			  
						

			#endif
				err = BMA250_WriteCalibration(client, cali);			 
			}
			break;

		case GSENSOR_IOCTL_CLR_CALI:
			err = BMA250_ResetCalibration(client);
			break;

		case GSENSOR_IOCTL_GET_CALI:
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			err = BMA250_ReadCalibration(client, cali);
			if(err)
			{
				break;
			}
			#if 0
			sensor_data.x = cali[BMA250_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			sensor_data.y = cali[BMA250_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			sensor_data.z = cali[BMA250_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
			#else
			sensor_data.x = cali[BMA250_AXIS_X];
			sensor_data.y = cali[BMA250_AXIS_Y];
			sensor_data.z = cali[BMA250_AXIS_Z];
						

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
static struct file_operations bma250_fops = {
	//.owner = THIS_MODULE,
	.open = bma250_open,
	.release = bma250_release,
	.unlocked_ioctl = bma250_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice bma250_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &bma250_fops,
};
/*----------------------------------------------------------------------------*/
#ifndef CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int bma250_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);    
	int err = 0;
	GSE_FUN();    

	if(msg.event == PM_EVENT_SUSPEND)
	{   
		if(obj == NULL)
		{
			GSE_ERR("null pointer!!\n");
			return -EINVAL;
		}
		atomic_set(&obj->suspend, 1);
		
#ifndef CUSTOM_KERNEL_SENSORHUB
		if(err = BMA250_SetPowerMode(obj->client, false))
#else //#ifndef CUSTOM_KERNEL_SENSORHUB
		if ((err = BMA250_SCP_SetPowerMode(false, ID_ACCELEROMETER)))
#endif //#ifndef CUSTOM_KERNEL_SENSORHUB
		{
			GSE_ERR("write power control fail!!\n");
			return;
		}
#ifndef CUSTOM_KERNEL_SENSORHUB		
		BMA250_power(obj->hw, 0);
#endif
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int bma250_resume(struct i2c_client *client)
{
	struct bma250_i2c_data *obj = i2c_get_clientdata(client);        
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
#ifndef CUSTOM_KERNEL_SENSORHUB
	BMA250_power(obj->hw, 1);
#endif

#ifndef CUSTOM_KERNEL_SENSORHUB
	if(err = bma250_init_client(client, 0))
#else
	if ((err = BMA250_SCP_SetPowerMode(enable_status, ID_ACCELEROMETER)))
#endif
	{
		GSE_ERR("initialize client fail!!\n");
		return err;        
	}

	atomic_set(&obj->suspend, 0);

	return 0;
}
/*----------------------------------------------------------------------------*/
#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void bma250_early_suspend(struct early_suspend *h) 
{
	struct bma250_i2c_data *obj = container_of(h, struct bma250_i2c_data, early_drv);   
	int err;
	GSE_FUN();    

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1);
#ifndef CUSTOM_KERNEL_SENSORHUB
	err = BMA250_SetPowerMode(obj->client, false); 
#else
	if ((err = BMA250_SCP_SetPowerMode(false, ID_ACCELEROMETER)))
#endif
	if(err)
	{
		GSE_ERR("write power control fail!!\n");
		return;
	}

	sensor_power = false;
#ifndef CUSTOM_KERNEL_SENSORHUB	
	BMA250_power(obj->hw, 0);
#endif
}
/*----------------------------------------------------------------------------*/
static void bma250_late_resume(struct early_suspend *h)
{
	struct bma250_i2c_data *obj = container_of(h, struct bma250_i2c_data, early_drv);         
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}

#ifndef CUSTOM_KERNEL_SENSORHUB
	BMA250_power(obj->hw, 1);
#endif

#ifndef CUSTOM_KERNEL_SENSORHUB
	if (bma250_revesion == BMA250E)
	{
		u8 buf = 0xE6;
		u8 addr = 0x14;
		err = hwmsen_write_block(obj->client, addr, &buf, 1);
		if(err)
		{
			GSE_ERR("error: %d\n", err);
		}
		mdelay(1);
	}
	err = BMA250_SetPowerMode(obj->client, true);
#else
	if ((err = BMA250_SCP_SetPowerMode(enable_status, ID_ACCELEROMETER)))
#endif
	if(err)
	{
		GSE_ERR("write power control fail!!\n");
		return;
	}
	atomic_set(&obj->suspend, 0);    
}
/*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/


/*----------------------------------------------------------------------------*/
// if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats, div) to HAL
static int gsensor_open_report_data(int open)
{
	//should queuq work to report event if  is_report_input_direct=true
	return 0;
}
/*----------------------------------------------------------------------------*/
// if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL
static int gsensor_enable_nodata(int en)
{
    int err = 0;

#ifdef GSENSOR_UT
    GSE_FUN();
#endif

    mutex_lock(&gsensor_mutex);
	if(((en == 0) && (sensor_power == false)) ||((en == 1) && (sensor_power == true)))
	{
		enable_status = sensor_power;
		GSE_LOG("Gsensor device have updated!\n");
	}
	else
	{
		enable_status = !sensor_power;
		if (atomic_read(&obj_i2c_data->suspend) == 0)
		{
#ifdef CUSTOM_KERNEL_SENSORHUB
            err = BMA250_SCP_SetPowerMode(enable_status, ID_ACCELEROMETER);
            if (0 == err)
            {
                sensor_power = enable_status;
            }
#else//#ifdef CUSTOM_KERNEL_SENSORHUB
			err = BMA250_SetPowerMode(obj_i2c_data->client, enable_status);
#endif
			GSE_LOG("Gsensor not in suspend gsensor_SetPowerMode!, enable_status = %d\n",enable_status);
		}
		else
		{
			GSE_LOG("Gsensor in suspend and can not enable or disable!enable_status = %d\n",enable_status);
		}
	}
	mutex_unlock(&gsensor_mutex);

    if(err != BMA250_SUCCESS)
	{
		printk("gsensor_enable_nodata fail!\n");
		return -1;
	}

    printk("gsensor_enable_nodata OK!!!\n");
	return 0;
}
/*----------------------------------------------------------------------------*/
static int gsensor_set_delay(u64 ns)
{
    int err = 0;
    int value;
#ifdef CUSTOM_KERNEL_SENSORHUB
    SCP_SENSOR_HUB_DATA req;
    int len;
#else//#ifdef CUSTOM_KERNEL_SENSORHUB
	int sample_delay;
#endif//#ifdef CUSTOM_KERNEL_SENSORHUB

#ifdef GSENSOR_UT
    GSE_FUN();
#endif

    value = (int)ns/1000/1000;

#ifdef CUSTOM_KERNEL_SENSORHUB
    req.set_delay_req.sensorType = ID_ACCELEROMETER;
    req.set_delay_req.action = SENSOR_HUB_SET_DELAY;
    req.set_delay_req.delay = value;
    len = sizeof(req.activate_req);
    err = SCP_sensorHub_req_send(&req, &len, 1);
    if (err)
    {
        GSE_ERR("SCP_sensorHub_req_send!\n");
        return err;
    }
#else//#ifdef CUSTOM_KERNEL_SENSORHUB    
	if(value <= 5)
	{
		sample_delay = BMA250_BW_200HZ;
	}
	else if(value <= 10)
	{
		sample_delay = BMA250_BW_100HZ;
	}
	else
	{
		sample_delay = BMA250_BW_50HZ;
	}

	mutex_lock(&gsensor_mutex);
	err = BMA250_SetBWRate(obj_i2c_data->client, sample_delay);
	mutex_unlock(&gsensor_mutex);
	if(err != BMA250_SUCCESS ) //0x2C->BW=100Hz
	{
		GSE_ERR("Set delay parameter error!\n");
        return -1;
	}

	if(value >= 50)
	{
		atomic_set(&obj_i2c_data->filter, 0);
	}
	else
	{	
	#if defined(CONFIG_BMA250_LOWPASS)
		obj_i2c_data->fir.num = 0;
		obj_i2c_data->fir.idx = 0;
		obj_i2c_data->fir.sum[BMA250_AXIS_X] = 0;
		obj_i2c_data->fir.sum[BMA250_AXIS_Y] = 0;
		obj_i2c_data->fir.sum[BMA250_AXIS_Z] = 0;
		atomic_set(&obj_i2c_data->filter, 1);
	#endif
	}
#endif//#ifdef CUSTOM_KERNEL_SENSORHUB
    
    GSE_LOG("gsensor_set_delay (%d)\n",value);

	return 0;
}
/*----------------------------------------------------------------------------*/
static int gsensor_get_data(int* x ,int* y,int* z, int* status)
{
#ifdef CUSTOM_KERNEL_SENSORHUB
    SCP_SENSOR_HUB_DATA req;
    int len;
    int err = 0;
#else
    char buff[BMA250_BUFSIZE];
#endif //#ifdef CUSTOM_KERNEL_SENSORHUB

    //GSE_FUN();

#ifdef CUSTOM_KERNEL_SENSORHUB
    req.get_data_req.sensorType = ID_ACCELEROMETER;
    req.get_data_req.action = SENSOR_HUB_GET_DATA;
    len = sizeof(req.get_data_req);
    err = SCP_sensorHub_req_send(&req, &len, 1);
    if (err)
    {
        GSE_ERR("SCP_sensorHub_req_send!\n");
        return err;
    }

    if (ID_ACCELEROMETER != req.get_data_rsp.sensorType ||
        SENSOR_HUB_GET_DATA != req.get_data_rsp.action ||
        0 != req.get_data_rsp.errCode)
    {
        GSE_ERR("error : %d\n", req.get_data_rsp.errCode);
        return req.get_data_rsp.errCode;
    }

    //sscanf(buff, "%x %x %x", req.get_data_rsp.int16_Data[0], req.get_data_rsp.int16_Data[1], req.get_data_rsp.int16_Data[2]);
    *x = req.get_data_rsp.int16_Data[0];
    *y = req.get_data_rsp.int16_Data[1];
    *z = req.get_data_rsp.int16_Data[2];
    //GSE_ERR("x = %d, y = %d, z = %d\n", *x, *y, *z);
    *status = SENSOR_STATUS_ACCURACY_MEDIUM;

#else//#ifdef CUSTOM_KERNEL_SENSORHUB
    mutex_lock(&gsensor_mutex);
	BMA250_ReadSensorData(obj_i2c_data->client, buff, BMA250_BUFSIZE);
	mutex_unlock(&gsensor_mutex);
	sscanf(buff, "%x %x %x", x, y, z);
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;
#endif

	return 0;
}

/*----------------------------------------------------------------------------*/
//static int bma250_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
//{    
//	strcpy(info->type, BMA250_DEV_NAME);
//	return 0;
//}

/*----------------------------------------------------------------------------*/
static int bma250_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct bma250_i2c_data *obj;
	struct hwmsen_object sobj;
    struct acc_control_path ctl={0};
    struct acc_data_path data={0};
	int err = 0;
	GSE_FUN();

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	
	memset(obj, 0, sizeof(struct bma250_i2c_data));

	obj->hw = get_cust_acc_hw();
	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if(err)
	{
		GSE_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit;
	}

#ifdef CUSTOM_KERNEL_SENSORHUB
	INIT_WORK(&obj->irq_work, gsensor_irq_work);
#endif//#ifdef CUSTOM_KERNEL_SENSORHUB

	obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client,obj);
	
	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);

#ifdef CUSTOM_KERNEL_SENSORHUB
    obj->SCP_init_done = 0;
#endif//#ifdef CUSTOM_KERNEL_SENSORHUB

#ifdef CONFIG_BMA250_LOWPASS
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

	bma250_i2c_client = new_client;	

	err = bma250_init_client(new_client, 1);
	if(err)
	{
		goto exit_init_failed;
	}
	
	err = misc_register(&bma250_device);
	if(err)
	{
		GSE_ERR("bma250_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	err = bma250_create_attr(&bma250_init_info.platform_diver_addr->driver);
	if(err)
	{
		GSE_ERR("create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data= gsensor_open_report_data;
	ctl.enable_nodata = gsensor_enable_nodata;
	ctl.set_delay  = gsensor_set_delay;
	ctl.is_report_input_direct = false;
	
#ifdef CUSTOM_KERNEL_SENSORHUB
	ctl.is_support_batch = obj->hw->is_batch_supported;
#else
	ctl.is_support_batch = false;
#endif
	
	err = acc_register_control_path(&ctl);
	if(err)
	{
		GSE_ERR("register acc control path err\n");
		printk("register acc control path err\n");
		goto exit_create_attr_failed;
	}

	printk("acc_register_control_path sucess\n");

	data.get_data = gsensor_get_data;
	data.vender_div = 1000;
	err = acc_register_data_path(&data);
	if(err)
	{
		GSE_ERR("register acc data path err\n");
		printk("register acc data path err\n");
		goto exit_create_attr_failed;
	}

	printk("acc_register_data_path sucess\n");
	
	err = batch_register_support_info(ID_ACCELEROMETER,ctl.is_support_batch, 1000, 0);
	if(err)
	{
		GSE_ERR("register gsensor batch support err = %d\n", err);
		printk("register gsensor batch support err\n");
		goto exit_create_attr_failed;
	}

	printk("batch_register_support_info sucess\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	obj->early_drv.suspend  = bma250_early_suspend,
	obj->early_drv.resume   = bma250_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif 

	gsensor_init_flag = 0;
	GSE_LOG("%s: OK\n", __func__); 
	printk("bma250_i2c_probe sucess\n");

	return 0;

	exit_create_attr_failed:
	misc_deregister(&bma250_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	//i2c_detach_client(new_client);
	exit_kfree:
	kfree(obj);
	exit:
	GSE_ERR("%s: err = %d\n", __func__, err);
	gsensor_init_flag =-1;
	return err;
}

/*----------------------------------------------------------------------------*/
static int bma250_i2c_remove(struct i2c_client *client)
{
	int err = 0;	
	
	err = bma250_delete_attr(&bma250_init_info.platform_diver_addr->driver);
	if(err)
	{
		GSE_ERR("bma150_delete_attr fail: %d\n", err);
	}
	
	err = misc_deregister(&bma250_device);
	if(err)
	{
		GSE_ERR("misc_deregister fail: %d\n", err);
	}

	bma250_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	
	return 0;
}
/*----------------------------------------------------------------------------*/
static int bma250_probe(struct platform_device *pdev) 
{
	struct acc_hw *hw = get_cust_acc_hw();
	GSE_FUN();

	BMA250_power(hw, 1);
	//bma250_force[0] = hw->i2c_num;
	if(i2c_add_driver(&bma250_i2c_driver))
	{
		GSE_ERR("add driver error\n");
		return -1;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int bma250_remove(struct platform_device *pdev)
{
    struct acc_hw *hw = get_cust_acc_hw();

    GSE_FUN();    
    BMA250_power(hw, 0);    
    i2c_del_driver(&bma250_i2c_driver);
    return 0;
}

/*----------------------------------------------------------------------------*/
static int gsensor_local_init(void)
{
    struct acc_hw *hw = get_cust_acc_hw();

    GSE_FUN();

	BMA250_power(hw, 1);
	if(i2c_add_driver(&bma250_i2c_driver))
	{
		GSE_ERR("add driver error\n");
		return -1;
	}
	if(-1 == gsensor_init_flag)
	{
	   return -1;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int gsensor_remove()
{
    struct acc_hw *hw = get_cust_acc_hw();

    GSE_FUN();
    BMA250_power(hw, 0);    
    i2c_del_driver(&bma250_i2c_driver);
    return 0;
}

/*----------------------------------------------------------------------------*/
static int __init bma250_init(void)
{
	struct acc_hw *hw = get_cust_acc_hw();
	GSE_FUN();
	GSE_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	i2c_register_board_info(hw->i2c_num, &i2c_bma250, 1);
	acc_driver_add(&bma250_init_info);
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit bma250_exit(void)
{
	GSE_FUN();
}
/*----------------------------------------------------------------------------*/
module_init(bma250_init);
module_exit(bma250_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BMA250 I2C driver");
MODULE_AUTHOR("Xiaoli.li@mediatek.com");
