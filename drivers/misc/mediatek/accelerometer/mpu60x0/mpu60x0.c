/* MPU6050C_ACC motion sensor driver
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

#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include "mpu60x0.h"
#include "mpu6050c.h"
#include <linux/hwmsen_helper.h>
#include <linux/kernel.h>

//#include <mach/mt_devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#include <mach/mt_boot.h>

#define POWER_NONE_MACRO MT65XX_POWER_NONE

/*----------------------------------------------------------------------------*/
#define I2C_DRIVERID_MPU6050C_ACC	3000
/*----------------------------------------------------------------------------*/
#define MPU6050C_DEFAULT_FS		MPU6050C_FS_1000
#define MPU6050C_DEFAULT_LSB		MPU6050C_FS_1000_LSB
/*---------------------------------------------------------------------------*/
#define DEBUG 0
/*----------------------------------------------------------------------------*/
#define CONFIG_MPU6050C_ACC_LOWPASS   /*apply low pass filter on output*/       
/*----------------------------------------------------------------------------*/
#define MPU6050C_ACC_AXIS_X          0
#define MPU6050C_ACC_AXIS_Y          1
#define MPU6050C_ACC_AXIS_Z          2
#define MPU6050C_ACC_AXES_NUM        3
#define MPU6050C_ACC_DATA_LEN        6   
#define MPU6050C_ACC_DEV_NAME        "MPU6050C_ACC"
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id mpu6050c_acc_i2c_id[] = {{MPU6050C_ACC_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_mpu6050c_acc={ I2C_BOARD_INFO("MPU6050C_ACC", MPU6050C_ACC_I2C_SLAVE_ADDR>>1)};
/*the adapter id will be available in customization*/
//static unsigned short mpu6050c_acc_force[] = {0x00, MPU6050C_ACC_I2C_SLAVE_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const mpu6050c_acc_forces[] = { mpu6050c_acc_force, NULL };
//static struct i2c_client_address_data mpu6050c_acc_addr_data = { .forces = mpu6050c_acc_forces,};

int packet_thresh = 75; // 600 ms / 8ms/sample

/*----------------------------------------------------------------------------*/
static int mpu6050c_acc_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int mpu6050c_acc_i2c_remove(struct i2c_client *client);
//static int mpu6050c_acc_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
//static int mpu6050c_acc_suspend(struct i2c_client *client, pm_message_t msg) ;
//static int mpu6050c_acc_resume(struct i2c_client *client);
/*----------------------------------------------------------------------------*/
typedef enum {
    ACC_TRC_FILTER  = 0x01,
    ACC_TRC_RAWDATA = 0x02,
    ACC_TRC_IOCTL   = 0x04,
    ACC_TRC_CALI	= 0X08,
    ACC_TRC_INFO	= 0X10,
    ACC_TRC_DATA	= 0X20,
} ACC_TRC;
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
    s16 raw[C_MAX_FIR_LENGTH][MPU6050C_ACC_AXES_NUM];
    int sum[MPU6050C_ACC_AXES_NUM];
    int num;
    int idx;
};

typedef enum {
    CMC_BIT_ACC    = 1,
    CMC_BIT_GYRO     = 2,
} CMC_BIT;

/*----------------------------------------------------------------------------*/
struct mpu6050c_acc_i2c_data {
    struct i2c_client *client;
    struct acc_hw *hw;
    struct hwmsen_convert   cvt;
    
    /*misc*/
    struct data_resolution *reso;
    atomic_t                trace;
    atomic_t                suspend;
    atomic_t                selftest;
    atomic_t				filter;
    s16                     acc_cali_sw[MPU6050C_ACC_AXES_NUM+1];
    s16                     gyro_cali_sw[MPU6050C_ACC_AXES_NUM+1];

    /*data*/
    s8                      offset[MPU6050C_ACC_AXES_NUM+1];  /*+1: for 4-byte alignment*/
    s16                     data[MPU6050C_ACC_AXES_NUM+1];

#if defined(CONFIG_MPU6050C_ACC_LOWPASS)
    atomic_t                firlen;
    atomic_t                fir_en;
    struct data_filter      fir;
#endif 
    /*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_drv;
#endif     
	ulong		enable; 		/*enable mask*/

};
/*----------------------------------------------------------------------------*/
static struct i2c_driver mpu6050c_acc_i2c_driver = {
    .driver = {
        .name           = MPU6050C_ACC_DEV_NAME,
    },
	.probe      		= mpu6050c_acc_i2c_probe,
	.remove    			= mpu6050c_acc_i2c_remove,
#if !defined(CONFIG_HAS_EARLYSUSPEND)    
    .suspend            = mpu6050c_acc_suspend,
    .resume             = mpu6050c_acc_resume,
#endif
	.id_table = mpu6050c_acc_i2c_id,
};

/*----------------------------------------------------------------------------*/
struct i2c_client *mpu6050c_acc_i2c_client = NULL;
static struct platform_driver mpu6050c_acc_driver;
struct mpu6050c_acc_i2c_data *mpu6050c_obj_i2c_data = NULL;



/*----------------------------------------------------------------------------*/
#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN(f)               printk(KERN_INFO GSE_TAG"%s\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)    printk(KERN_ERR GSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    printk(KERN_INFO GSE_TAG fmt, ##args)
/*----------------------------------------------------------------------------*/
/*

//----------------------------------------------------------------------------//
static struct data_resolution mpu6050c_acc_offset_resolution = {{15, 6}, 64};
*/
/*--------------------accscopy power control function----------------------------------*/
static struct data_resolution mpu6050c_data_resolution[] = {
 /* combination by {FULL_RES,RANGE}*/
	 {{ 1, 0}, 16384},   // dataformat +/-2g	in 16-bit resolution;    16384 = (2^16)/(2*2)			
};

/*----------------------------------------------------------------------------*/


struct mpu6050c_acc_i2c_data* MPU6050C_Acc_GetI2CData(void)
{
	return mpu6050c_obj_i2c_data;
}
EXPORT_SYMBOL_GPL(MPU6050C_Acc_GetI2CData);


static void MPU6050C_ACC_power(struct acc_hw *hw, unsigned int on) 
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
			if(!hwPowerOn(hw->power_id, hw->power_vol, "MPU6050C_ACC"))
			{
				GSE_ERR("power on fails!!\n");
			}
		}
		else	// power off
		{
			if (!hwPowerDown(hw->power_id, "MPU6050C_ACC"))
			{
				GSE_ERR("power off fail!!\n");
			}			  
		}
	}
	power_on = on;    
}
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
static int MPU6050C_ACC_write_rel_calibration(struct mpu6050c_acc_i2c_data *obj, int dat[MPU6050C_ACC_AXES_NUM])
{
    obj->acc_cali_sw[MPU6050C_ACC_AXIS_X] = obj->cvt.sign[MPU6050C_ACC_AXIS_X]*dat[obj->cvt.map[MPU6050C_ACC_AXIS_X]];
    obj->acc_cali_sw[MPU6050C_ACC_AXIS_Y] = obj->cvt.sign[MPU6050C_ACC_AXIS_Y]*dat[obj->cvt.map[MPU6050C_ACC_AXIS_Y]];
    obj->acc_cali_sw[MPU6050C_ACC_AXIS_Z] = obj->cvt.sign[MPU6050C_ACC_AXIS_Z]*dat[obj->cvt.map[MPU6050C_ACC_AXIS_Z]];
#if DEBUG		
		if(atomic_read(&obj->trace) & ACC_TRC_CALI)
		{
			GSE_LOG("test  (%5d, %5d, %5d) ->(%5d, %5d, %5d)->(%5d, %5d, %5d))\n", 
				obj->cvt.sign[MPU6050C_ACC_AXIS_X],obj->cvt.sign[MPU6050C_ACC_AXIS_Y],obj->cvt.sign[MPU6050C_ACC_AXIS_Z],
				dat[MPU6050C_ACC_AXIS_X], dat[MPU6050C_ACC_AXIS_Y], dat[MPU6050C_ACC_AXIS_Z],
				obj->cvt.map[MPU6050C_ACC_AXIS_X],obj->cvt.map[MPU6050C_ACC_AXIS_Y],obj->cvt.map[MPU6050C_ACC_AXIS_Z]);
			GSE_LOG("write acc calibration data  (%5d, %5d, %5d)\n", 
				obj->acc_cali_sw[MPU6050C_ACC_AXIS_X],obj->acc_cali_sw[MPU6050C_ACC_AXIS_Y],obj->acc_cali_sw[MPU6050C_ACC_AXIS_Z]);
		}
#endif
    return 0;
}
static int MPU6050C_ACC_write_gyro_rel_calibration(struct mpu6050c_acc_i2c_data *obj, int dat[MPU6050C_ACC_AXES_NUM])
{
    obj->gyro_cali_sw[MPU6050C_ACC_AXIS_X] = obj->cvt.sign[MPU6050C_ACC_AXIS_X]*dat[obj->cvt.map[MPU6050C_ACC_AXIS_X]];
    obj->gyro_cali_sw[MPU6050C_ACC_AXIS_Y] = obj->cvt.sign[MPU6050C_ACC_AXIS_Y]*dat[obj->cvt.map[MPU6050C_ACC_AXIS_Y]];
    obj->gyro_cali_sw[MPU6050C_ACC_AXIS_Z] = obj->cvt.sign[MPU6050C_ACC_AXIS_Z]*dat[obj->cvt.map[MPU6050C_ACC_AXIS_Z]];
#if DEBUG		
		if(atomic_read(&obj->trace) & ACC_TRC_CALI)
		{
			GSE_LOG("test  (%5d, %5d, %5d) ->(%5d, %5d, %5d)->(%5d, %5d, %5d))\n", 
				obj->cvt.sign[MPU6050C_ACC_AXIS_X],obj->cvt.sign[MPU6050C_ACC_AXIS_Y],obj->cvt.sign[MPU6050C_ACC_AXIS_Z],
				dat[MPU6050C_ACC_AXIS_X], dat[MPU6050C_ACC_AXIS_Y], dat[MPU6050C_ACC_AXIS_Z],
				obj->cvt.map[MPU6050C_ACC_AXIS_X],obj->cvt.map[MPU6050C_ACC_AXIS_Y],obj->cvt.map[MPU6050C_ACC_AXIS_Z]);
			GSE_LOG("write acc calibration data  (%5d, %5d, %5d)\n", 
				obj->gyro_cali_sw[MPU6050C_ACC_AXIS_X],obj->gyro_cali_sw[MPU6050C_ACC_AXIS_Y],obj->gyro_cali_sw[MPU6050C_ACC_AXIS_Z]);
		}
#endif
    return 0;
}



/*----------------------------------------------------------------------------*/
static int MPU6050C_ACC_ResetCalibration(struct i2c_client *client)
{
	struct mpu6050c_acc_i2c_data *obj = i2c_get_clientdata(client);	

	memset(obj->acc_cali_sw, 0x00, sizeof(obj->acc_cali_sw));
	return 0;    
}

/*----------------------------------------------------------------------------*/
static int MPU6050C_ACC_ResetGyroCalibration(struct i2c_client *client)
{
	struct mpu6050c_acc_i2c_data *obj = i2c_get_clientdata(client);	

	memset(obj->gyro_cali_sw, 0x00, sizeof(obj->gyro_cali_sw));
	return 0;    
}

/*----------------------------------------------------------------------------*/
static int MPU6050C_ACC_ReadCalibration(struct i2c_client *client, int dat[MPU6050C_ACC_AXES_NUM])
{
    struct mpu6050c_acc_i2c_data *obj = i2c_get_clientdata(client);

    dat[obj->cvt.map[MPU6050C_ACC_AXIS_X]] = obj->cvt.sign[MPU6050C_ACC_AXIS_X]*obj->acc_cali_sw[MPU6050C_ACC_AXIS_X];
    dat[obj->cvt.map[MPU6050C_ACC_AXIS_Y]] = obj->cvt.sign[MPU6050C_ACC_AXIS_Y]*obj->acc_cali_sw[MPU6050C_ACC_AXIS_Y];
    dat[obj->cvt.map[MPU6050C_ACC_AXIS_Z]] = obj->cvt.sign[MPU6050C_ACC_AXIS_Z]*obj->acc_cali_sw[MPU6050C_ACC_AXIS_Z];

#if DEBUG		
		if(atomic_read(&obj->trace) & ACC_TRC_CALI)
		{
			GSE_LOG("Read acc calibration data  (%5d, %5d, %5d)\n", 
				dat[MPU6050C_ACC_AXIS_X],dat[MPU6050C_ACC_AXIS_Y],dat[MPU6050C_ACC_AXIS_Z]);
		}
#endif
                                       
    return 0;
}
/*----------------------------------------------------------------------------*/
static int MPU6050C_ACC_ReadGyroCalibration(struct i2c_client *client, int dat[MPU6050C_ACC_AXES_NUM])
{
    struct mpu6050c_acc_i2c_data *obj = i2c_get_clientdata(client);

    dat[obj->cvt.map[MPU6050C_ACC_AXIS_X]] = obj->cvt.sign[MPU6050C_ACC_AXIS_X]*obj->gyro_cali_sw[MPU6050C_ACC_AXIS_X];
    dat[obj->cvt.map[MPU6050C_ACC_AXIS_Y]] = obj->cvt.sign[MPU6050C_ACC_AXIS_Y]*obj->gyro_cali_sw[MPU6050C_ACC_AXIS_Y];
    dat[obj->cvt.map[MPU6050C_ACC_AXIS_Z]] = obj->cvt.sign[MPU6050C_ACC_AXIS_Z]*obj->gyro_cali_sw[MPU6050C_ACC_AXIS_Z];

#if DEBUG		
		if(atomic_read(&obj->trace) & ACC_TRC_CALI)
		{
			GSE_LOG("Read acc calibration data  (%5d, %5d, %5d)\n", 
				dat[MPU6050C_ACC_AXIS_X],dat[MPU6050C_ACC_AXIS_Y],dat[MPU6050C_ACC_AXIS_Z]);
		}
#endif
                                       
    return 0;
}

/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static int MPU6050C_ACC_WriteCalibration(struct i2c_client *client, int dat[MPU6050C_ACC_AXES_NUM])
{
	struct mpu6050c_acc_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int cali[MPU6050C_ACC_AXES_NUM];


	GSE_FUN();
	if(!obj || ! dat)
	{
		GSE_ERR("null ptr!!\n");
		return -EINVAL;
	}
	else
	{        		
		cali[obj->cvt.map[MPU6050C_ACC_AXIS_X]] = obj->cvt.sign[MPU6050C_ACC_AXIS_X]*obj->acc_cali_sw[MPU6050C_ACC_AXIS_X];
		cali[obj->cvt.map[MPU6050C_ACC_AXIS_Y]] = obj->cvt.sign[MPU6050C_ACC_AXIS_Y]*obj->acc_cali_sw[MPU6050C_ACC_AXIS_Y];
		cali[obj->cvt.map[MPU6050C_ACC_AXIS_Z]] = obj->cvt.sign[MPU6050C_ACC_AXIS_Z]*obj->acc_cali_sw[MPU6050C_ACC_AXIS_Z]; 
		cali[MPU6050C_ACC_AXIS_X] += dat[MPU6050C_ACC_AXIS_X];
		cali[MPU6050C_ACC_AXIS_Y] += dat[MPU6050C_ACC_AXIS_Y];
		cali[MPU6050C_ACC_AXIS_Z] += dat[MPU6050C_ACC_AXIS_Z];
#if DEBUG		
		if(atomic_read(&obj->trace) & ACC_TRC_CALI)
		{
			GSE_LOG("write acc calibration data  (%5d, %5d, %5d)-->(%5d, %5d, %5d)\n", 
				dat[MPU6050C_ACC_AXIS_X], dat[MPU6050C_ACC_AXIS_Y], dat[MPU6050C_ACC_AXIS_Z],
				cali[MPU6050C_ACC_AXIS_X],cali[MPU6050C_ACC_AXIS_Y],cali[MPU6050C_ACC_AXIS_Z]);
		}
#endif
		return MPU6050C_ACC_write_rel_calibration(obj, cali);
	} 

	return err;
}
static int MPU6050C_ACC_WriteGyroCalibration(struct i2c_client *client, int dat[MPU6050C_ACC_AXES_NUM])
{
	struct mpu6050c_acc_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int cali[MPU6050C_ACC_AXES_NUM];


	GSE_FUN();
	if(!obj || ! dat)
	{
		GSE_ERR("null ptr!!\n");
		return -EINVAL;
	}
	else
	{        		
		cali[obj->cvt.map[MPU6050C_ACC_AXIS_X]] = obj->cvt.sign[MPU6050C_ACC_AXIS_X]*obj->gyro_cali_sw[MPU6050C_ACC_AXIS_X];
		cali[obj->cvt.map[MPU6050C_ACC_AXIS_Y]] = obj->cvt.sign[MPU6050C_ACC_AXIS_Y]*obj->gyro_cali_sw[MPU6050C_ACC_AXIS_Y];
		cali[obj->cvt.map[MPU6050C_ACC_AXIS_Z]] = obj->cvt.sign[MPU6050C_ACC_AXIS_Z]*obj->gyro_cali_sw[MPU6050C_ACC_AXIS_Z]; 
		cali[MPU6050C_ACC_AXIS_X] += dat[MPU6050C_ACC_AXIS_X];
		cali[MPU6050C_ACC_AXIS_Y] += dat[MPU6050C_ACC_AXIS_Y];
		cali[MPU6050C_ACC_AXIS_Z] += dat[MPU6050C_ACC_AXIS_Z];
#if DEBUG		
		if(atomic_read(&obj->trace) & ACC_TRC_CALI)
		{
			GSE_LOG("write acc calibration data  (%5d, %5d, %5d)-->(%5d, %5d, %5d)\n", 
				dat[MPU6050C_ACC_AXIS_X], dat[MPU6050C_ACC_AXIS_Y], dat[MPU6050C_ACC_AXIS_Z],
				cali[MPU6050C_ACC_AXIS_X],cali[MPU6050C_ACC_AXIS_Y],cali[MPU6050C_ACC_AXIS_Z]);
		}
#endif
		return MPU6050C_ACC_write_gyro_rel_calibration(obj, cali);
	} 

	return err;
}

/*----------------------------------------------------------------------------*/

static int MPU6050C_ACC_Reset(struct i2c_client *client)
{
	int res = 0;
	GSE_LOG("Reset MPU6050 B1\n");
	res = hwmsen_write_byte(client,MPU6050C_ACC_REG_PWR_MGMT_1,BIT_HW_RESET);
	if (res) 
	{
		GSE_ERR(" Reset MPU6050 B1 error,res: %d!\n", res);
		return res;
	}
	msleep(40);
	return MPU6050C_ACC_SUCCESS;
}


static int MPU6050C_ACC_SetPWR_MGMT_12(struct i2c_client *client)
{
	u8 databuf1[2] = {0};
	u8 databuf2[2] = {0};
	int res = 0;
	struct mpu6050c_acc_i2c_data *obj = i2c_get_clientdata(client);
	//GSE_FUN();

	if((!test_bit(CMC_BIT_GYRO, &obj->enable))
		&&(test_bit(CMC_BIT_ACC, &obj->enable)))
	{ 
		//means it works only for acc,so set gyro as standby mode
		GSE_LOG("MPU6050C_ACC_SetPWR_MGMT_12 !CMC_BIT_GYRO&&CMC_BIT_ACC \n");
		//databuf1[0] = 0x24;// CYCLE =1 ;SLEEP =0; TEMP_DIS =1
		databuf1[0] = 0x28;// CYCLE =1 ;SLEEP =0; TEMP_DIS =1; internal 8M clock
		databuf2[0] = 0xc7;//STBY_XG =1,STBY_YG =1,STBY_ZG =1,LP_WAKE_CTRL =11

	}
	else if((test_bit(CMC_BIT_GYRO, &obj->enable))
		&&(test_bit(CMC_BIT_ACC, &obj->enable)))
	{
		//mean  work both
		GSE_LOG("MPU6050C_ACC_SetPWR_MGMT_12 CMC_BIT_GYR&&CMC_BIT_ACC \n");
		databuf1[0] = 0x01;//SLEEP=0;CYCLE =0 MPU6050C_ACC_CLKSEL_PLL_X;TEMP_DIS = 0
		databuf2[0] = 0x00;//STBY_XG =0,STBY_YG =0,STBY_ZG =0;STBY_XA =0,STBY_YA =0,STBY_ZA =0
	}
	else if((test_bit(CMC_BIT_GYRO, &obj->enable))
		&&(!test_bit(CMC_BIT_ACC, &obj->enable)))
	{
		//mean	gyro work only,acc sleep
		GSE_LOG("MPU6050C_ACC_SetPWR_MGMT_12 CMC_BIT_GYR&&CMC_BIT_ACC \n");
		databuf1[0] = 0x01;//SLEEP=0;CYCLE =0;MPU6050C_ACC_CLKSEL_PLL_X;TEMP_DIS = 0		
		databuf2[0] = 0x38;//0x34;//STBY_XA =1,STBY_YA =1,STBY_ZA =1
	}
	else if((!test_bit(CMC_BIT_GYRO, &obj->enable))
		&&(!test_bit(CMC_BIT_ACC, &obj->enable)))
	{
		//mean both sleep
		GSE_LOG("MPU6050C_ACC_SetPWR_MGMT_12 !CMC_BIT_GYR&&!CMC_BIT_ACC \n");
		databuf1[0] |= MPU6050C_ACC_SLEEP;
		databuf1[0] |= MPU6050C_ACC_CLKSEL_PLL_X;

		databuf2[0] = 0x00;
		
	}
	
	databuf1[1] = databuf1[0];
	databuf1[0] = MPU6050C_ACC_REG_PWR_MGMT_1;  
	res = i2c_master_send(client, databuf1, 0x2);
	if(res <= 0)
	{
		GSE_LOG("set power mode failed!\n");
		return MPU6050C_ACC_ERR_I2C;
	}

	databuf2[1] = databuf2[0];
	databuf2[0] = MPU6050C_ACC_REG_PWR_MGMT_2;
	res = i2c_master_send(client, databuf2, 0x2);
	if(res <= 0)
	{
		GSE_LOG("set power mode failed!\n");
		return MPU6050C_ACC_ERR_I2C;
	}
	return MPU6050C_ACC_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int MPU6050C_ACC_SetAccDataRange(struct i2c_client *client, u8 range)
{
	
	u8 databuf[2];	  
	int res = 0;

	memset(databuf, 0, sizeof(u8)*2);	 
	databuf[0] = MPU6050C_ACC_REG_ACC_CONFIG;    
	databuf[1] = range;
	databuf[1] = databuf[1]<<3;//refer to datasheet
	databuf[1] &= 0x18;
	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		return MPU6050C_ACC_ERR_I2C;
	}

	mpu6050c_obj_i2c_data->reso = &mpu6050c_data_resolution[0];
	return 0;

}
static int MPU6050C_ACC_SetGyroDataRange(struct i2c_client *client, u8 range)
{
	
	u8 databuf[2];	  
	int res = 0;

	memset(databuf, 0, sizeof(u8)*2);	 
	databuf[0] = MPU6050C_ACC_REG_GYRO_CONFIG;    
	databuf[1] = range;
	databuf[1] = databuf[1]<<3;//refer to datasheet
	databuf[1] &= 0x18;


	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		return MPU6050C_ACC_ERR_I2C;
	}

	return 0;

}

static int MPU6050C_ACC_SetDataBandWidth(struct i2c_client *client, u8 BandWidth)
{
	
	u8 databuf[2];	  
	int res = 0;

	memset(databuf, 0, sizeof(u8)*2);	 
	databuf[0] = MPU6050C_ACC_REG_DATA_CONFIG;    
	databuf[1] = BandWidth&0x07;

	res = i2c_master_send(client, databuf, 0x2);

	if(res <= 0)
	{
		return MPU6050C_ACC_ERR_I2C;
	}

	return 0;

}

// set the sample rate
static int MPU6050C_ACC_SetSampleRate(struct i2c_client *client, int sample_rate)
{
	u8 databuf[2] = {0}; 
	int smplrt_div = 0;
	int res = 0;
	GSE_FUN();    

	if(hwmsen_read_byte(client, MPU6050C_ACC_REG_DATA_CONFIG, databuf))
	{
		GSE_ERR("read acc data format register err!\n");
		return MPU6050C_ACC_ERR_I2C;
	}
	else
	{
		GSE_LOG("read  acc data format register: 0x%x\n", databuf[0]);
	}

	if((databuf[0] & 0x07) == 0)	//Analog sample rate is 8KHz
	{
		smplrt_div = 8 * 1000 / sample_rate - 1;
	}
	else	// 1kHz
	{
		smplrt_div = 1000 / sample_rate - 1;
	}

	if(smplrt_div > 255)	// rate_div: 0 to 255;
	{
		smplrt_div = 255;
	}
	else if(smplrt_div < 0)
	{
		smplrt_div = 0;
	}
	
	databuf[0] = MPU6050C_ACC_REG_DATA_SMPRT_DIV;
	databuf[1] = smplrt_div;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GSE_ERR("write sample rate register err!\n");
		return MPU6050C_ACC_ERR_I2C;
	}

	//read sample div after written for test	
	udelay(500);
	if(hwmsen_read_byte(client, MPU6050C_ACC_REG_DATA_SMPRT_DIV, databuf))
	{
		GSE_ERR("read acc sample rate register err!\n");
		return MPU6050C_ACC_ERR_I2C;
	}
	else
	{
		GSE_LOG("read  acc sample rate: 0x%x\n", databuf[0]);
	}
	
	return MPU6050C_ACC_SUCCESS;    
}

/*----------------------------------------------------------------------------*/
static int MPU6050C_ACC_ReadAccData(struct i2c_client *client, char *buf, int bufsize)
{
	char databuf[6];	
	int data[3];
	struct mpu6050c_acc_i2c_data *obj = i2c_get_clientdata(client);  

	if(!test_bit(CMC_BIT_ACC, &obj->enable))
	{
		set_bit(CMC_BIT_ACC, &obj->enable);
		MPU6050C_ACC_SetPWR_MGMT_12(client);
		msleep(50);
	}

	if(hwmsen_read_block(client, MPU6050C_ACC_REG_ACCEL_XOUT_H, databuf, 6))
	{
		GSE_ERR("MPU6050C_ACC read accscope data	error\n");
		return -2;
	}
	else
	{
		obj->data[MPU6050C_ACC_AXIS_X] = ((s16)((databuf[MPU6050C_ACC_AXIS_X*2+1]) | (databuf[MPU6050C_ACC_AXIS_X*2] << 8)));
		obj->data[MPU6050C_ACC_AXIS_Y] = ((s16)((databuf[MPU6050C_ACC_AXIS_Y*2+1]) | (databuf[MPU6050C_ACC_AXIS_Y*2] << 8)));
		obj->data[MPU6050C_ACC_AXIS_Z] = ((s16)((databuf[MPU6050C_ACC_AXIS_Z*2+1]) | (databuf[MPU6050C_ACC_AXIS_Z*2] << 8)));
#if DEBUG		
		if(atomic_read(&obj->trace) & ACC_TRC_RAWDATA)
		{
			GSE_LOG("read acc register: %d, %d, %d, %d, %d, %d",
				databuf[0], databuf[1], databuf[2], databuf[3], databuf[4], databuf[5]);
			GSE_LOG("get acc raw data (0x%08X, 0x%08X, 0x%08X) -> (%5d, %5d, %5d)\n", 
				obj->data[MPU6050C_ACC_AXIS_X],obj->data[MPU6050C_ACC_AXIS_Y],obj->data[MPU6050C_ACC_AXIS_Z],
				obj->data[MPU6050C_ACC_AXIS_X],obj->data[MPU6050C_ACC_AXIS_Y],obj->data[MPU6050C_ACC_AXIS_Z]);
			GSE_LOG("acc_cali_sw[0] =%d,acc_cali_sw[1] =%d,acc_cali_sw[2] =%d,\n", 
				obj->acc_cali_sw[0], obj->acc_cali_sw[1], obj->acc_cali_sw[2]);
		}
#endif		
		#if 1
				obj->data[MPU6050C_ACC_AXIS_X] = obj->data[MPU6050C_ACC_AXIS_X] * GRAVITY_EARTH_1000 / (obj->reso->sensitivity);
				obj->data[MPU6050C_ACC_AXIS_Y] = obj->data[MPU6050C_ACC_AXIS_Y] * GRAVITY_EARTH_1000 / (obj->reso->sensitivity);
				obj->data[MPU6050C_ACC_AXIS_Z] = obj->data[MPU6050C_ACC_AXIS_Z] * GRAVITY_EARTH_1000 / (obj->reso->sensitivity);
		#endif

		obj->data[MPU6050C_ACC_AXIS_X] = obj->data[MPU6050C_ACC_AXIS_X] + obj->acc_cali_sw[MPU6050C_ACC_AXIS_X];
		obj->data[MPU6050C_ACC_AXIS_Y] = obj->data[MPU6050C_ACC_AXIS_Y] + obj->acc_cali_sw[MPU6050C_ACC_AXIS_Y];
		obj->data[MPU6050C_ACC_AXIS_Z] = obj->data[MPU6050C_ACC_AXIS_Z] + obj->acc_cali_sw[MPU6050C_ACC_AXIS_Z];
		/*remap coordinate*/
		data[obj->cvt.map[MPU6050C_ACC_AXIS_X]] = obj->cvt.sign[MPU6050C_ACC_AXIS_X]*obj->data[MPU6050C_ACC_AXIS_X];
		data[obj->cvt.map[MPU6050C_ACC_AXIS_Y]] = obj->cvt.sign[MPU6050C_ACC_AXIS_Y]*obj->data[MPU6050C_ACC_AXIS_Y];
		data[obj->cvt.map[MPU6050C_ACC_AXIS_Z]] = obj->cvt.sign[MPU6050C_ACC_AXIS_Z]*obj->data[MPU6050C_ACC_AXIS_Z];

		////Out put the mg
		#if 0
		data[MPU6050C_ACC_AXIS_X] = data[MPU6050C_ACC_AXIS_X] * GRAVITY_EARTH_1000 / 16384;
		data[MPU6050C_ACC_AXIS_Y] = data[MPU6050C_ACC_AXIS_Y] * GRAVITY_EARTH_1000 / 16384;
		data[MPU6050C_ACC_AXIS_Z] = data[MPU6050C_ACC_AXIS_Z] * GRAVITY_EARTH_1000 / 16384;
		#endif
	}
	
	sprintf(buf, "%04x %04x %04x", data[MPU6050C_ACC_AXIS_X],data[MPU6050C_ACC_AXIS_Y],data[MPU6050C_ACC_AXIS_Z]);

#if DEBUG		
	if(atomic_read(&obj->trace) & ACC_TRC_DATA)
	{
		GSE_LOG("get acc data packet:[%d %d %d]\n", data[0], data[1], data[2]);
	}
#endif
	
	return 0;
	
}

/*----------------------------------------------------------------------------*/
static int MPU6050C_ACC_ReadGyroData(struct i2c_client *client, char *buf, int bufsize)
{
	char databuf[6];	
	int data[3];
	struct mpu6050c_acc_i2c_data *obj = i2c_get_clientdata(client);  
	
	if(!test_bit(CMC_BIT_GYRO, &obj->enable))
	{
		set_bit(CMC_BIT_GYRO, &obj->enable);
		MPU6050C_ACC_SetPWR_MGMT_12(client);
		msleep(50);
	}

	if(hwmsen_read_block(client, MPU6050C_ACC_REG_GYRO_XOUT_H, databuf, 6))
	{
		GSE_ERR("MPU6050C_ACC read gyroscope data  error\n");
		return -2;
	}
	else
	{
		obj->data[MPU6050C_ACC_AXIS_X] = ((s16)((databuf[MPU6050C_ACC_AXIS_X*2+1]) | (databuf[MPU6050C_ACC_AXIS_X*2] << 8)));
		obj->data[MPU6050C_ACC_AXIS_Y] = ((s16)((databuf[MPU6050C_ACC_AXIS_Y*2+1]) | (databuf[MPU6050C_ACC_AXIS_Y*2] << 8)));
		obj->data[MPU6050C_ACC_AXIS_Z] = ((s16)((databuf[MPU6050C_ACC_AXIS_Z*2+1]) | (databuf[MPU6050C_ACC_AXIS_Z*2] << 8)));
#if DEBUG		
		if(atomic_read(&obj->trace) & ACC_TRC_RAWDATA)
		{
			GSE_LOG("read gyro register: %d, %d, %d, %d, %d, %d",
				databuf[0], databuf[1], databuf[2], databuf[3], databuf[4], databuf[5]);
			GSE_LOG("get gyro raw data (0x%08X, 0x%08X, 0x%08X) -> (%5d, %5d, %5d)\n", 
				obj->data[MPU6050C_ACC_AXIS_X],obj->data[MPU6050C_ACC_AXIS_Y],obj->data[MPU6050C_ACC_AXIS_Z],
				obj->data[MPU6050C_ACC_AXIS_X],obj->data[MPU6050C_ACC_AXIS_Y],obj->data[MPU6050C_ACC_AXIS_Z]);
			GSE_LOG("gyro_cali_sw[0] =%d,gyro_cali_sw[1] =%d,gyro_cali_sw[2] =%d,\n",
				obj->gyro_cali_sw[0], obj->gyro_cali_sw[1], obj->gyro_cali_sw[2]);

		}
#endif		
		obj->data[MPU6050C_ACC_AXIS_X] = obj->data[MPU6050C_ACC_AXIS_X] + obj->gyro_cali_sw[MPU6050C_ACC_AXIS_X];
		obj->data[MPU6050C_ACC_AXIS_Y] = obj->data[MPU6050C_ACC_AXIS_Y] + obj->gyro_cali_sw[MPU6050C_ACC_AXIS_Y];
		obj->data[MPU6050C_ACC_AXIS_Z] = obj->data[MPU6050C_ACC_AXIS_Z] + obj->gyro_cali_sw[MPU6050C_ACC_AXIS_Z];

		/*remap coordinate*/
		data[obj->cvt.map[MPU6050C_ACC_AXIS_X]] = obj->cvt.sign[MPU6050C_ACC_AXIS_X]*obj->data[MPU6050C_ACC_AXIS_X];
		data[obj->cvt.map[MPU6050C_ACC_AXIS_Y]] = obj->cvt.sign[MPU6050C_ACC_AXIS_Y]*obj->data[MPU6050C_ACC_AXIS_Y];
		data[obj->cvt.map[MPU6050C_ACC_AXIS_Z]] = obj->cvt.sign[MPU6050C_ACC_AXIS_Z]*obj->data[MPU6050C_ACC_AXIS_Z];

		//Out put the degree/second(o/s)
		data[MPU6050C_ACC_AXIS_X] = data[MPU6050C_ACC_AXIS_X] * MPU6050C_FS_MAX_LSB / MPU6050C_DEFAULT_LSB;
		data[MPU6050C_ACC_AXIS_Y] = data[MPU6050C_ACC_AXIS_Y] * MPU6050C_FS_MAX_LSB / MPU6050C_DEFAULT_LSB;
		data[MPU6050C_ACC_AXIS_Z] = data[MPU6050C_ACC_AXIS_Z] * MPU6050C_FS_MAX_LSB / MPU6050C_DEFAULT_LSB;

	
	}

	sprintf(buf, "%04x %04x %04x", data[MPU6050C_ACC_AXIS_X],data[MPU6050C_ACC_AXIS_Y],data[MPU6050C_ACC_AXIS_Z]);

#if DEBUG		
	if(atomic_read(&obj->trace) & ACC_TRC_DATA)
	{
		GSE_LOG("get gyro data packet:[%d %d %d]\n", data[0], data[1], data[2]);
	}
#endif
	
	return 0;
	
}

//for factory mode

/*----------------------------------------------------------------------------*/
static int MPU6050C_ACC_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
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

	sprintf(buf, "MPU6050C_ACC Chip");
	return 0;
}


/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mpu6050c_acc_i2c_client;
	char strbuf[MPU6050C_ACC_BUFSIZE];
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	
	MPU6050C_ACC_ReadChipInfo(client, strbuf, MPU6050C_ACC_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);        
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mpu6050c_acc_i2c_client;
	char strbuf[MPU6050C_ACC_BUFSIZE];
	
	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}
	
	MPU6050C_ACC_ReadAccData(client, strbuf, MPU6050C_ACC_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);;            
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct mpu6050c_acc_i2c_data *obj = mpu6050c_obj_i2c_data;
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
	struct mpu6050c_acc_i2c_data *obj = mpu6050c_obj_i2c_data;
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
		GSE_ERR("invalid content: '%s', length = %lld\n", buf, (long long)count);
	}
	
	return count;    
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;    
	struct mpu6050c_acc_i2c_data *obj = mpu6050c_obj_i2c_data;
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
static DRIVER_ATTR(chipinfo,             S_IRUGO, show_chipinfo_value,      NULL);
static DRIVER_ATTR(sensordata,           S_IRUGO, show_sensordata_value,    NULL);
static DRIVER_ATTR(trace,      S_IWUSR | S_IRUGO, show_trace_value,         store_trace_value);
static DRIVER_ATTR(status,               S_IRUGO, show_status_value,        NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *MPU6050C_ACC_attr_list[] = {
	&driver_attr_chipinfo,     /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/	
	&driver_attr_trace,        /*trace log*/
	&driver_attr_status,        
};
/*----------------------------------------------------------------------------*/
static int mpu6050c_acc_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(MPU6050C_ACC_attr_list)/sizeof(MPU6050C_ACC_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if(0 != (err = driver_create_file(driver, MPU6050C_ACC_attr_list[idx])))
		{            
			GSE_ERR("driver_create_file (%s) = %d\n", MPU6050C_ACC_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int mpu6050c_acc_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(MPU6050C_ACC_attr_list)/sizeof(MPU6050C_ACC_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, MPU6050C_ACC_attr_list[idx]);
	}
	

	return err;
}
#if 0 // for build error
/*----------------------------------------------------------------------------*/
static int mpu6050c_acc_gpio_config(void)
{
    //because we donot use EINT ,to support low power
    // config to GPIO input mode + PD    
    //set   GPIO_MSE_EINT_PIN
#if 0
    mt_set_gpio_mode(GPIO_GYRO_EINT_PIN, GPIO_GYRO_EINT_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_GYRO_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_GYRO_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_GYRO_EINT_PIN, GPIO_PULL_DOWN);
#endif
	return 0;
}
#endif
static int mpu6050c_acc_init_client(struct i2c_client *client, bool enable)
{
	struct mpu6050c_acc_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;
	GSE_FUN();	
	//mpu6050c_acc_gpio_config();
	res = MPU6050C_ACC_Reset(client);
	if(res != MPU6050C_ACC_SUCCESS)
	{
		GSE_ERR("MPU6050C_ACC_Reset failed!\n");
		return res;
	}
	res = MPU6050C_ACC_SetPWR_MGMT_12(client);
	if(res != MPU6050C_ACC_SUCCESS)
	{
		return res;
	}
	
	
	res = MPU6050C_ACC_SetAccDataRange(client,MPU6050C_ACC_ACC_RNG_2G);
	if(res != MPU6050C_ACC_SUCCESS) 
	{
		GSE_ERR("MPU6050C_ACC_SetAccDataRange failed!\n");
		return res;
	}
	res = MPU6050C_ACC_SetGyroDataRange(client,MPU6050C_ACC_GYRO_RNG_1000);
	if(res != MPU6050C_ACC_SUCCESS) 
	{
		GSE_ERR("MPU6050C_ACC_SetGyroDataRange failed!\n");
		return res;
	}
	res = MPU6050C_ACC_SetDataBandWidth(client,MPU6050C_ACC_RATE_1K_LPFB_98HZ);
	if(res != MPU6050C_ACC_SUCCESS) 
	{
		GSE_ERR("MPU6050C_ACC_SetDataBandWidth failed!\n");
		return res;
	}

	// Set 125HZ sample rate
	res = MPU6050C_ACC_SetSampleRate(client, 125);
	if(res != MPU6050C_ACC_SUCCESS ) 
	{
		GSE_ERR("MPU6050C_ACC_SetSampleRate failed!\n");
		return res;
	}
	GSE_LOG("mpu6050c_acc_init_client OK!\n");

#ifdef CONFIG_MPU6050C_ACC_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));  
#endif

	return MPU6050C_ACC_SUCCESS;
}

/*----------------------------------------------------------------------------*/
int mpu6050c_acc_acc_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	struct mpu6050c_acc_i2c_data *priv = (struct mpu6050c_acc_i2c_data*)self;
	hwm_sensor_data* acc_data;
	char buff[MPU6050C_ACC_BUFSIZE];	

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
			
			}
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				GSE_ERR("Enable accscope parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				//GSE_LOG("mpu6050c_acc_acc_operate SENSOR_ENABLE =%d",value);
				if(((value == 0) && (!test_bit(CMC_BIT_ACC, &mpu6050c_obj_i2c_data->enable))) 
					||((value == 1) && (test_bit(CMC_BIT_ACC, &mpu6050c_obj_i2c_data->enable))))
				{
					GSE_LOG("accscope device have updated!\n");
				}
				else
				{
					if(value == true)
					{
						set_bit(CMC_BIT_ACC, &mpu6050c_obj_i2c_data->enable);
					}
					else
					{
						clear_bit(CMC_BIT_ACC, &mpu6050c_obj_i2c_data->enable);
					}
					err = MPU6050C_ACC_SetPWR_MGMT_12(priv->client);
				}
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				GSE_ERR("get accscope data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				acc_data = (hwm_sensor_data *)buff_out;
				MPU6050C_ACC_ReadAccData(priv->client, buff, MPU6050C_ACC_BUFSIZE);
				sscanf(buff, "%x %x %x", &acc_data->values[0], 
					&acc_data->values[1], &acc_data->values[2]);				
				acc_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;				
				acc_data->value_divide = 1000;
				//GSE_ERR("X :%d,Y: %d, Z: %d\n",gsensor_data->values[0],gsensor_data->values[1],gsensor_data->values[2]);
			}
			break;
		default:
			GSE_ERR("accscope operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}

	return err;
}

int mpu6050c_acc_gyro_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;	
	struct mpu6050c_acc_i2c_data *priv = (struct mpu6050c_acc_i2c_data*)self;
	hwm_sensor_data* gyro_data;
	char buff[MPU6050C_ACC_BUFSIZE];	

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
			
			}
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				GSE_ERR("Enable gyroscope parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				//GSE_LOG("mpu6050c_acc_gyro_operate SENSOR_ENABLE =%d",value);
				if(((value == 0) && (!test_bit(CMC_BIT_GYRO, &mpu6050c_obj_i2c_data->enable))) 
					||((value == 1) && (test_bit(CMC_BIT_GYRO, &mpu6050c_obj_i2c_data->enable))))
				{
					GSE_LOG("accscope device have updated!\n");
				}
				else
				{
					if(value == true)
					{
						set_bit(CMC_BIT_GYRO, &mpu6050c_obj_i2c_data->enable);
					}
					else
					{
						clear_bit(CMC_BIT_GYRO, &mpu6050c_obj_i2c_data->enable);
					}
					err = MPU6050C_ACC_SetPWR_MGMT_12(priv->client);
				}
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				GSE_ERR("get gyroscope data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				gyro_data = (hwm_sensor_data *)buff_out;
				MPU6050C_ACC_ReadGyroData(priv->client, buff, MPU6050C_ACC_BUFSIZE);
				sscanf(buff, "%x %x %x", &gyro_data->values[0], 
									&gyro_data->values[1], &gyro_data->values[2]);				
				gyro_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;				
				gyro_data->value_divide = DEGREE_TO_RAD;
			}
			break;
		default:
			GSE_ERR("gyroscope operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}

	return err;
}

/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int mpu6050c_acc_open(struct inode *inode, struct file *file)
{
	file->private_data = mpu6050c_acc_i2c_client;

	if(file->private_data == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int mpu6050c_acc_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*----------------------------------------------------------------------------*/
//static int mpu6050c_acc_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
//       unsigned long arg)
static long mpu6050c_acc_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	struct mpu6050c_acc_i2c_data *obj = (struct mpu6050c_acc_i2c_data*)i2c_get_clientdata(client);	
	char strbuf[MPU6050C_ACC_BUFSIZE] = {0};
	//s16 *SMTdata;
	void __user *data;
	long err = 0;
	int copy_cnt = 0;
	SENSOR_DATA sensor_data;
	int cali[3];
	int smtRes=0;
	//GSE_FUN();
	
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
		case GYROSCOPE_IOCTL_INIT:
			mpu6050c_acc_init_client(client, false);			
			break;
		case GYROSCOPE_IOCTL_SMT_DATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			copy_cnt = copy_to_user(data, &smtRes,  sizeof(smtRes));
			
			if(copy_cnt)
			{
				err = -EFAULT;
				GSE_LOG("copy gyro data to user failed!\n");
			}	
			GSE_LOG("copy gyro data to user OK: %d!\n", copy_cnt);

			break;
		case GYROSCOPE_IOCTL_READ_SENSORDATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			MPU6050C_ACC_ReadGyroData(client, strbuf, MPU6050C_ACC_BUFSIZE);
			if(copy_to_user(data, strbuf, sizeof(strbuf)))
			{
				err = -EFAULT;
				break;	  
			}				 
			break;

		case GYROSCOPE_IOCTL_SET_CALI:
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
			
			else
			{
				GSE_LOG("fwq going to set cali\n");
				cali[MPU6050C_ACC_AXIS_X] = sensor_data.x * MPU6050C_DEFAULT_LSB / MPU6050C_FS_MAX_LSB;
				cali[MPU6050C_ACC_AXIS_Y] = sensor_data.y * MPU6050C_DEFAULT_LSB / MPU6050C_FS_MAX_LSB;
				cali[MPU6050C_ACC_AXIS_Z] = sensor_data.z * MPU6050C_DEFAULT_LSB / MPU6050C_FS_MAX_LSB;			  
				err = MPU6050C_ACC_WriteGyroCalibration(client, cali);			 
				GSE_LOG("fwq GSENSOR_IOCTL_SET_CALI!!sensor_data .x =%d,sensor_data .z =%d,sensor_data .z =%d \n",sensor_data.x,sensor_data.y,sensor_data.z);
			}
			break;

		case GYROSCOPE_IOCTL_CLR_CALI:
			err = MPU6050C_ACC_ResetGyroCalibration(client);
			break;

		case GYROSCOPE_IOCTL_GET_CALI:
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			err = MPU6050C_ACC_ReadGyroCalibration(client, cali);
			if(err)
			{
				break;
			}
			
			sensor_data.x = cali[MPU6050C_ACC_AXIS_X] * MPU6050C_FS_MAX_LSB / MPU6050C_DEFAULT_LSB;
			sensor_data.y = cali[MPU6050C_ACC_AXIS_Y] * MPU6050C_FS_MAX_LSB / MPU6050C_DEFAULT_LSB;
			sensor_data.z = cali[MPU6050C_ACC_AXIS_Z] * MPU6050C_FS_MAX_LSB / MPU6050C_DEFAULT_LSB;
			if(copy_to_user(data, &sensor_data, sizeof(sensor_data)))
			{
				err = -EFAULT;
				break;
			}		
			break;
			case GSENSOR_IOCTL_INIT:
				mpu6050c_acc_init_client(client, false);
			break;
			case GSENSOR_IOCTL_READ_CHIPINFO:
				GSE_LOG("fwq GSENSOR_IOCTL_READ_CHIPINFO\n");
				data = (void __user *) arg;
				if(data == NULL)
				{
					err = -EINVAL;
					break;	  
				}
				
				MPU6050C_ACC_ReadChipInfo(client, strbuf, MPU6050C_ACC_BUFSIZE);
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
				MPU6050C_ACC_ReadAccData(client, strbuf, MPU6050C_ACC_BUFSIZE);
				if(copy_to_user(data, strbuf, strlen(strbuf)+1))
				{
					err = -EFAULT;
					break;	  
				}				 
			break;
			case GSENSOR_IOCTL_READ_OFFSET:
				
			break;
			case GSENSOR_IOCTL_READ_RAW_DATA:
			break;
			case GSENSOR_IOCTL_SET_CALI:
			GSE_LOG("fwq GSENSOR_IOCTL_SET_CALI!!\n");
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
				GSE_LOG("fwq going to set cali\n");
			#if 0
				cali[MPU6050C_ACC_AXIS_X] = sensor_data.x * obj->reso->sensitivity / GRAVITY_EARTH_1000;
				cali[MPU6050C_ACC_AXIS_Y] = sensor_data.y * obj->reso->sensitivity / GRAVITY_EARTH_1000;
				cali[MPU6050C_ACC_AXIS_Z] = sensor_data.z * obj->reso->sensitivity / GRAVITY_EARTH_1000;
			#else
				cali[MPU6050C_ACC_AXIS_X] = sensor_data.x;
				cali[MPU6050C_ACC_AXIS_Y] = sensor_data.y;
				cali[MPU6050C_ACC_AXIS_Z] = sensor_data.z;
			
			#endif
				err = MPU6050C_ACC_WriteCalibration(client, cali);
				GSE_LOG("fwq GSENSOR_IOCTL_SET_CALI!!sensor_data .x =%d,sensor_data .z =%d,sensor_data .z =%d \n",sensor_data.x,sensor_data.y,sensor_data.z);
			}

			break;			
			case GSENSOR_IOCTL_CLR_CALI:
				GSE_LOG("fwq GSENSOR_IOCTL_CLR_CALI!!\n");
				err = MPU6050C_ACC_ResetCalibration(client);
			break;
			case GSENSOR_IOCTL_GET_CALI:
				GSE_LOG("fwq GSENSOR_IOCTL_GET_CALI\n");
				data = (void __user*)arg;
				if(data == NULL)
				{
					err = -EINVAL;
					break;	  
				}
                err = MPU6050C_ACC_ReadCalibration(client, cali);
				if(err)
				{
					break;
				}
				#if 0
				sensor_data.x = cali[MPU6050C_ACC_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
				sensor_data.y = cali[MPU6050C_ACC_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
				sensor_data.z = cali[MPU6050C_ACC_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
				#else
				sensor_data.x = cali[MPU6050C_ACC_AXIS_X];
				sensor_data.y = cali[MPU6050C_ACC_AXIS_Y];
				sensor_data.z = cali[MPU6050C_ACC_AXIS_Z];
				
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
static struct file_operations mpu6050c_acc_fops = {
//	.owner = THIS_MODULE,//modified
	.open = mpu6050c_acc_open,
	.release = mpu6050c_acc_release,
	.unlocked_ioctl = mpu6050c_acc_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice mpu6050c_acc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &mpu6050c_acc_fops,
};
#if 1
/*----------------------------------------------------------------------------*/

static struct miscdevice mpu6050c_acc_gyro_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gyroscope",
	.fops = &mpu6050c_acc_fops,
};
#endif
/*----------------------------------------------------------------------------*/
#ifndef CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int mpu6050c_acc_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct mpu6050c_acc_i2c_data *obj = i2c_get_clientdata(client);    
	int err; 
	GSE_FUN();   

	if(msg.event == PM_EVENT_SUSPEND)
	{   
		if(obj == NULL)
		{
			GSE_ERR("null pointer!!\n");
			return -EINVAL;
		}
		atomic_set(&obj->suspend, 1);		

		err = MPU6050C_ACC_SetPWR_MGMT_12(client);
		if(err <= 0)
		{
			return err;
		}
	}
	return 0;//modified
}
/*----------------------------------------------------------------------------*/
static int mpu6050c_acc_resume(struct i2c_client *client)
{
	struct mpu6050c_acc_i2c_data *obj = i2c_get_clientdata(client);        
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return -EINVAL;
	}

	MPU6050C_ACC_power(obj->hw, 1);
	err = mpu6050c_acc_init_client(client, false);
	if(err)
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
static void mpu6050c_acc_early_suspend(struct early_suspend *h) 
{
	struct mpu6050c_acc_i2c_data *obj = container_of(h, struct mpu6050c_acc_i2c_data, early_drv);   
	int err;
	u8 databuf1[2]={0};
	u8 databuf2[2]={0};
	GSE_FUN();    

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1);


	databuf1[0] |= MPU6050C_ACC_SLEEP;
	databuf1[0] |= MPU6050C_ACC_CLKSEL_PLL_X;
	

	databuf1[1] = databuf1[0];
	databuf1[0] = MPU6050C_ACC_REG_PWR_MGMT_1;  
	err = i2c_master_send(obj->client, databuf1, 0x2);
	if(err <= 0)
	{
		GSE_LOG("set power mode failed!\n");
		return ;
	}
	
	databuf2[0] = 0x00;
	databuf2[1] = databuf2[0];
	databuf2[0] = MPU6050C_ACC_REG_PWR_MGMT_2;
	err = i2c_master_send(obj->client, databuf2, 0x2);
	if(err <= 0)
	{
		GSE_LOG("set power mode failed!\n");
		return ;
	}

	MPU6050C_ACC_power(obj->hw, 0);
}
/*----------------------------------------------------------------------------*/
static void mpu6050c_acc_late_resume(struct early_suspend *h)
{
	struct mpu6050c_acc_i2c_data *obj = container_of(h, struct mpu6050c_acc_i2c_data, early_drv);         
	int err;
	GSE_FUN();

	if(obj == NULL)
	{
		GSE_ERR("null pointer!!\n");
		return;
	}

	MPU6050C_ACC_power(obj->hw, 1);
	err = mpu6050c_acc_init_client(obj->client, false);
	if(err)
	{
		GSE_ERR("initialize client fail! err code %d!\n", err);
		return;        
	}
	atomic_set(&obj->suspend, 0);    
}
/*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/
/*static int mpu6050c_acc_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{    
	strcpy(info->type, MPU6050C_ACC_DEV_NAME);
	return 0;
}
*/
/*----------------------------------------------------------------------------*/
static int mpu6050c_acc_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct mpu6050c_acc_i2c_data *obj;
	//struct hwmsen_object sobj;
	struct hwmsen_object sobj_acc, sobj_gyro;

	int err = 0;
	GSE_FUN();

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	
	memset(obj, 0, sizeof(struct mpu6050c_acc_i2c_data));

	obj->hw = get_cust_acc_hw();
	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if(err)
	{
		GSE_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit;
	}

	
	obj->enable = 0;
	mpu6050c_obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client,obj);
	
	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	
	set_bit(CMC_BIT_ACC, &obj->enable);
	set_bit(CMC_BIT_GYRO, &obj->enable);


	mpu6050c_acc_i2c_client = new_client;
	err = mpu6050c_acc_init_client(new_client, false);
	if(err)
	{
		goto exit_init_failed;
	}
	

	err = misc_register(&mpu6050c_acc_device);
	if(err)
	{
		GSE_ERR("mpu6050c_acc_device misc register failed!\n");
		goto exit_misc_device_register_failed;
	}
#if 1
	err = misc_register(&mpu6050c_acc_gyro_device);
	if(err)
	{
		GSE_ERR("mpu6050c_acc_gyro_device misc register failed!\n");
		goto exit_misc_device_register_failed;
	}
#endif
	err = mpu6050c_acc_create_attr(&mpu6050c_acc_driver.driver);
	if(err)
	{
		GSE_ERR("mpu6050c_acc create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	
	sobj_acc.self = obj;
    sobj_acc.polling = 1;
    sobj_acc.sensor_operate = mpu6050c_acc_acc_operate;
	err = hwmsen_attach(ID_ACCELEROMETER, &sobj_acc);
	if(err)
	{
		GSE_ERR("hwmsen_attach fail = %d\n", err);
		goto exit_kfree;
	}
	sobj_gyro.self = obj;
    sobj_gyro.polling = 1;
    sobj_gyro.sensor_operate = mpu6050c_acc_gyro_operate;
	err = hwmsen_attach(ID_GYROSCOPE, &sobj_gyro);
	if(err)
	{
		GSE_ERR("hwmsen_attach fail = %d\n", err);
		goto exit_kfree;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	obj->early_drv.suspend  = mpu6050c_acc_early_suspend,
	obj->early_drv.resume   = mpu6050c_acc_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif 

	GSE_LOG("%s: OK\n", __func__);    
	return 0;

	exit_create_attr_failed:
	misc_deregister(&mpu6050c_acc_device);
	
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
static int mpu6050c_acc_i2c_remove(struct i2c_client *client)
{
	int err = 0;	
	
	err = mpu6050c_acc_delete_attr(&mpu6050c_acc_driver.driver);
	if(err)
	{
		GSE_ERR("mpu6050c_acc_delete_attr fail: %d\n", err);
	}
	
	err = misc_deregister(&mpu6050c_acc_device);
	if(err)
	{
		GSE_ERR("misc_deregister fail: %d\n", err);
	}
#if 1
	err = misc_deregister(&mpu6050c_acc_gyro_device);
	if(err)
	{
		GSE_ERR("misc_deregister fail: %d\n", err);
	}
#endif
	err = hwmsen_detach(ID_ACCELEROMETER);
	if(err)
	{
		GSE_ERR("hwmsen_detach fail: %d\n", err);
	}
	
	err = hwmsen_detach(ID_GYROSCOPE);
	if(err)
	{
		GSE_ERR("hwmsen_detach fail: %d\n", err);
	}

	mpu6050c_acc_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
//extern struct mpu6050c_gyro_i2c_data* MPU6050C_Gyro_GetI2CData(void);
extern void* MPU6050C_Gyro_GetI2CData(void);
/*----------------------------------------------------------------------------*/
static int mpu6050c_acc_probe(struct platform_device *pdev) 
{
	struct acc_hw *hw = get_cust_acc_hw();
	GSE_FUN();

	MPU6050C_ACC_power(hw, 1);


	if(MPU6050C_Gyro_GetI2CData()!= NULL)
	{
		return 0;
	}
	else
	{
		if(i2c_add_driver(&mpu6050c_acc_i2c_driver))
		{
			GSE_ERR("add driver error\n");
			return -1;
		}
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
static int mpu6050c_acc_remove(struct platform_device *pdev)
{
    struct acc_hw *hw = get_cust_acc_hw();

    GSE_FUN();    
    MPU6050C_ACC_power(hw, 0);
    i2c_del_driver(&mpu6050c_acc_i2c_driver);
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
	
	static struct platform_driver mpu6050c_acc_driver = {
		.probe		= mpu6050c_acc_probe,
		.remove 	= mpu6050c_acc_remove,	 
		.driver 	= 
		{
			.name  = "gsensor",
			.owner  = THIS_MODULE,
        #ifdef CONFIG_OF
			.of_match_table = gsensor_of_match,
		#endif
		}
	};
#else

static struct platform_driver mpu6050c_acc_driver = {
	.probe      = mpu6050c_acc_probe,
	.remove     = mpu6050c_acc_remove,    
	.driver     = {
		.name  = "gsensor",
		.owner = THIS_MODULE,//modified
	}
};
#endif
/*----------------------------------------------------------------------------*/
static int __init mpu6050c_acc_init(void)
{
	//GSE_FUN();
	struct acc_hw *hw = get_cust_acc_hw();
	GSE_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	i2c_register_board_info(hw->i2c_num, &i2c_mpu6050c_acc, 1);
	if(platform_driver_register(&mpu6050c_acc_driver))
	{
		GSE_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit mpu6050c_acc_exit(void)
{
	GSE_FUN();
	platform_driver_unregister(&mpu6050c_acc_driver);
}
/*----------------------------------------------------------------------------*/
module_init(mpu6050c_acc_init);
module_exit(mpu6050c_acc_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MPU6050C_ACC  driver");
MODULE_AUTHOR("Chunlei.Wang@mediatek.com");
