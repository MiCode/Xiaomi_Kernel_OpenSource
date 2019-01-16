/* MPU3000 motion sensor driver
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

#include <cust_gyro.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include "mpu3000.h"
#include <linux/hwmsen_helper.h>
#include <linux/kernel.h>

//#include <mach/mt_devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#include <mach/mt_boot.h>

#define POWER_NONE_MACRO MT65XX_POWER_NONE

/*----------------------------------------------------------------------------*/
#define I2C_DRIVERID_MPU3000	3000
/*----------------------------------------------------------------------------*/
#define MPU3000_DEFAULT_FS		MPU3000_FS_1000
#define MPU3000_DEFAULT_LSB		MPU3000_FS_1000_LSB
/*---------------------------------------------------------------------------*/
#define DEBUG 0
/*----------------------------------------------------------------------------*/
#define CONFIG_MPU3000_LOWPASS   /*apply low pass filter on output*/       
/*----------------------------------------------------------------------------*/
#define MPU3000_AXIS_X          0
#define MPU3000_AXIS_Y          1
#define MPU3000_AXIS_Z          2
#define MPU3000_AXES_NUM        3
#define MPU3000_DATA_LEN        6   
#define MPU3000_DEV_NAME        "MPU3000"
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id mpu3000_i2c_id[] = {{MPU3000_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_mpu3000={ I2C_BOARD_INFO("MPU3000", (0xD0>>1))};
/*the adapter id will be available in customization*/
//static unsigned short mpu3000_force[] = {0x00, MPU3000_I2C_SLAVE_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const mpu3000_forces[] = { mpu3000_force, NULL };
//static struct i2c_client_address_data mpu3000_addr_data = { .forces = mpu3000_forces,};

int packet_thresh = 75; // 600 ms / 8ms/sample

/*----------------------------------------------------------------------------*/
static int mpu3000_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int mpu3000_i2c_remove(struct i2c_client *client);
//static int mpu3000_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
//static int mpu3000_suspend(struct i2c_client *client, pm_message_t msg) ;
//static int mpu3000_resume(struct i2c_client *client);
/*----------------------------------------------------------------------------*/
typedef enum {
    GYRO_TRC_FILTER  = 0x01,
    GYRO_TRC_RAWDATA = 0x02,
    GYRO_TRC_IOCTL   = 0x04,
    GYRO_TRC_CALI	= 0X08,
    GYRO_TRC_INFO	= 0X10,
    GYRO_TRC_DATA	= 0X20,
} GYRO_TRC;
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
    s16 raw[C_MAX_FIR_LENGTH][MPU3000_AXES_NUM];
    int sum[MPU3000_AXES_NUM];
    int num;
    int idx;
};
/*----------------------------------------------------------------------------*/
struct mpu3000_i2c_data {
    struct i2c_client *client;
    struct gyro_hw *hw;
    struct hwmsen_convert   cvt;
    
    /*misc*/
    struct data_resolution *reso;
    atomic_t                trace;
    atomic_t                suspend;
    atomic_t                selftest;
    atomic_t				filter;
    s16                     cali_sw[MPU3000_AXES_NUM+1];

    /*data*/
    s8                      offset[MPU3000_AXES_NUM+1];  /*+1: for 4-byte alignment*/
    s16                     data[MPU3000_AXES_NUM+1];

#if defined(CONFIG_MPU3000_LOWPASS)
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
static struct i2c_driver mpu3000_i2c_driver = {
    .driver = {
//        .owner          = THIS_MODULE,//modified
        .name           = MPU3000_DEV_NAME,
    },
	.probe      		= mpu3000_i2c_probe,
	.remove    			= mpu3000_i2c_remove,
//	.detect				= mpu3000_i2c_detect,
#if !defined(CONFIG_HAS_EARLYSUSPEND)    
    .suspend            = mpu3000_suspend,
    .resume             = mpu3000_resume,
#endif
	.id_table = mpu3000_i2c_id,
	//.address_list = mpu3000_forces,//modified
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *mpu3000_i2c_client = NULL;
static struct platform_driver mpu3000_gyro_driver;
static struct mpu3000_i2c_data *obj_i2c_data = NULL;
static bool sensor_power = false;



/*----------------------------------------------------------------------------*/
#define GYRO_TAG                  "[Gyroscope] "
#define GYRO_FUN(f)               printk(KERN_INFO GYRO_TAG"%s\n", __FUNCTION__)
#define GYRO_ERR(fmt, args...)    printk(KERN_ERR GYRO_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GYRO_LOG(fmt, args...)    printk(KERN_ERR GYRO_TAG fmt, ##args)
/*----------------------------------------------------------------------------*/
/*

//----------------------------------------------------------------------------//
static struct data_resolution mpu3000_offset_resolution = {{15, 6}, 64};
*/
/*--------------------gyroscopy power control function----------------------------------*/
static void MPU3000_power(struct gyro_hw *hw, unsigned int on) 
{
	static unsigned int power_on = 0;

	if(hw->power_id != POWER_NONE_MACRO)		// have externel LDO
	{        
		GYRO_LOG("power %s\n", on ? "on" : "off");
		if(power_on == on)	// power status not change
		{
			GYRO_LOG("ignore power control: %d\n", on);
		}
		else if(on)	// power on
		{
			if(!hwPowerOn(hw->power_id, hw->power_vol, "MPU3000"))
			{
				GYRO_ERR("power on fails!!\n");
			}
		}
		else	// power off
		{
			if (!hwPowerDown(hw->power_id, "MPU3000"))
			{
				GYRO_ERR("power off fail!!\n");
			}			  
		}
	}
	power_on = on;    
}
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
static int MPU3000_write_rel_calibration(struct mpu3000_i2c_data *obj, int dat[MPU3000_AXES_NUM])
{
    obj->cali_sw[MPU3000_AXIS_X] = obj->cvt.sign[MPU3000_AXIS_X]*dat[obj->cvt.map[MPU3000_AXIS_X]];
    obj->cali_sw[MPU3000_AXIS_Y] = obj->cvt.sign[MPU3000_AXIS_Y]*dat[obj->cvt.map[MPU3000_AXIS_Y]];
    obj->cali_sw[MPU3000_AXIS_Z] = obj->cvt.sign[MPU3000_AXIS_Z]*dat[obj->cvt.map[MPU3000_AXIS_Z]];
#if DEBUG		
		if(atomic_read(&obj->trace) & GYRO_TRC_CALI)
		{
			GYRO_LOG("test  (%5d, %5d, %5d) ->(%5d, %5d, %5d)->(%5d, %5d, %5d))\n", 
				obj->cvt.sign[MPU3000_AXIS_X],obj->cvt.sign[MPU3000_AXIS_Y],obj->cvt.sign[MPU3000_AXIS_Z],
				dat[MPU3000_AXIS_X], dat[MPU3000_AXIS_Y], dat[MPU3000_AXIS_Z],
				obj->cvt.map[MPU3000_AXIS_X],obj->cvt.map[MPU3000_AXIS_Y],obj->cvt.map[MPU3000_AXIS_Z]);
			GYRO_LOG("write gyro calibration data  (%5d, %5d, %5d)\n", 
				obj->cali_sw[MPU3000_AXIS_X],obj->cali_sw[MPU3000_AXIS_Y],obj->cali_sw[MPU3000_AXIS_Z]);
		}
#endif
    return 0;
}


/*----------------------------------------------------------------------------*/
static int MPU3000_ResetCalibration(struct i2c_client *client)
{
	struct mpu3000_i2c_data *obj = i2c_get_clientdata(client);	

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	return 0;    
}
/*----------------------------------------------------------------------------*/
static int MPU3000_ReadCalibration(struct i2c_client *client, int dat[MPU3000_AXES_NUM])
{
    struct mpu3000_i2c_data *obj = i2c_get_clientdata(client);

    dat[obj->cvt.map[MPU3000_AXIS_X]] = obj->cvt.sign[MPU3000_AXIS_X]*obj->cali_sw[MPU3000_AXIS_X];
    dat[obj->cvt.map[MPU3000_AXIS_Y]] = obj->cvt.sign[MPU3000_AXIS_Y]*obj->cali_sw[MPU3000_AXIS_Y];
    dat[obj->cvt.map[MPU3000_AXIS_Z]] = obj->cvt.sign[MPU3000_AXIS_Z]*obj->cali_sw[MPU3000_AXIS_Z];

#if DEBUG		
		if(atomic_read(&obj->trace) & GYRO_TRC_CALI)
		{
			GYRO_LOG("Read gyro calibration data  (%5d, %5d, %5d)\n", 
				dat[MPU3000_AXIS_X],dat[MPU3000_AXIS_Y],dat[MPU3000_AXIS_Z]);
		}
#endif
                                       
    return 0;
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static int MPU3000_WriteCalibration(struct i2c_client *client, int dat[MPU3000_AXES_NUM])
{
	struct mpu3000_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int cali[MPU3000_AXES_NUM];


	GYRO_FUN();
	if(!obj || ! dat)
	{
		GYRO_ERR("null ptr!!\n");
		return -EINVAL;
	}
	else
	{        		
		cali[obj->cvt.map[MPU3000_AXIS_X]] = obj->cvt.sign[MPU3000_AXIS_X]*obj->cali_sw[MPU3000_AXIS_X];
		cali[obj->cvt.map[MPU3000_AXIS_Y]] = obj->cvt.sign[MPU3000_AXIS_Y]*obj->cali_sw[MPU3000_AXIS_Y];
		cali[obj->cvt.map[MPU3000_AXIS_Z]] = obj->cvt.sign[MPU3000_AXIS_Z]*obj->cali_sw[MPU3000_AXIS_Z]; 
		cali[MPU3000_AXIS_X] += dat[MPU3000_AXIS_X];
		cali[MPU3000_AXIS_Y] += dat[MPU3000_AXIS_Y];
		cali[MPU3000_AXIS_Z] += dat[MPU3000_AXIS_Z];
#if DEBUG		
		if(atomic_read(&obj->trace) & GYRO_TRC_CALI)
		{
			GYRO_LOG("write gyro calibration data  (%5d, %5d, %5d)-->(%5d, %5d, %5d)\n", 
				dat[MPU3000_AXIS_X], dat[MPU3000_AXIS_Y], dat[MPU3000_AXIS_Z],
				cali[MPU3000_AXIS_X],cali[MPU3000_AXIS_Y],cali[MPU3000_AXIS_Z]);
		}
#endif
		return MPU3000_write_rel_calibration(obj, cali);
	} 

	return err;
}
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
static int MPU3000_ReadStart(struct i2c_client *client, bool enable)
{
	u8 databuf[2] = {0};    
	int res = 0;
	GYRO_FUN();    
	  
	databuf[0] = MPU3000_REG_FIFO_EN;  
	
	if(enable)
	{
		//enable xyz gyro in FIFO
		databuf[1] = (MPU3000_FIFO_GYROX_EN|MPU3000_FIFO_GYROY_EN|MPU3000_FIFO_GYROZ_EN);
	}
	else
	{
		//disable xyz gyro in FIFO
		databuf[1] = 0;
	}
	
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR(" enable xyz gyro in FIFO error,enable: 0x%x!\n", databuf[1]);
		return MPU3000_ERR_I2C;
	}
	GYRO_LOG("MPU3000_ReadStart: enable xyz gyro in FIFO: 0x%x\n", databuf[1]);
	return MPU3000_SUCCESS;
}



//----------------------------------------------------------------------------//
static int MPU3000_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2] = {0};    
	int res = 0;

	if(enable == sensor_power)
	{
		GYRO_LOG("Sensor power status is newest!\n");
		return MPU3000_SUCCESS;
	}

	if(hwmsen_read_byte(client, MPU3000_REG_PWR_CTL, databuf))
	{
		GYRO_ERR("read power ctl register err!\n");
		return MPU3000_ERR_I2C;
	}

	databuf[0] &= ~MPU3000_SLEEP;	
	if(enable == FALSE)
	{
		databuf[0] |= MPU3000_SLEEP;
	}
	else
	{
		// do nothing
	}

	
	databuf[1] = databuf[0];
	databuf[0] = MPU3000_REG_PWR_CTL;    
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_LOG("set power mode failed!\n");
		return MPU3000_ERR_I2C;
	}	
	else
	{
		GYRO_LOG("set power mode ok %d!\n", enable);
	}

	sensor_power = enable;
	
	return MPU3000_SUCCESS;    
}

/*----------------------------------------------------------------------------*/
static int MPU3000_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	u8 databuf[2] = {0};    
	int res = 0;
	GYRO_FUN();     
	
	databuf[0] = MPU3000_REG_DATA_FMT;    
	databuf[1] = dataformat;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		return MPU3000_ERR_I2C;
	}

	//read sample rate after written for test
	udelay(500);
	if(hwmsen_read_byte(client, MPU3000_REG_DATA_FMT, databuf))
	{
		GYRO_ERR("read data format register err!\n");
		return MPU3000_ERR_I2C;
	}
	else
	{
		GYRO_LOG("read  data format: 0x%x\n", databuf[0]);
	}	
	
	//return MPU3000_SetDataResolution(obj);  
	return MPU3000_SUCCESS;    
}

// set the sample rate
static int MPU3000_SetSampleRate(struct i2c_client *client, int sample_rate)
{
	u8 databuf[2] = {0}; 
	int rate_div = 0;
	int res = 0;
	GYRO_FUN();    

	if(hwmsen_read_byte(client, MPU3000_REG_DATA_FMT, databuf))
	{
		GYRO_ERR("read gyro data format register err!\n");
		return MPU3000_ERR_I2C;
	}
	else
	{
		GYRO_LOG("read  gyro data format register: 0x%x\n", databuf[0]);
	}

	if((databuf[0] & 0x07) == 0)	//Analog sample rate is 8KHz
	{
		rate_div = 8 * 1024 / sample_rate - 1;
	}
	else	// 1kHz
	{
		rate_div = 1024 / sample_rate - 1;
	}

	if(rate_div > 255)	// rate_div: 0 to 255;
	{
		rate_div = 255;
	}
	else if(rate_div < 0)
	{
		rate_div = 0;
	}
	
	databuf[0] = MPU3000_REG_SAMRT_DIV;    
	databuf[1] = rate_div;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write sample rate register err!\n");
		return MPU3000_ERR_I2C;
	}

	//read sample div after written for test	
	udelay(500);
	if(hwmsen_read_byte(client, MPU3000_REG_SAMRT_DIV, databuf))
	{
		GYRO_ERR("read gyro sample rate register err!\n");
		return MPU3000_ERR_I2C;
	}
	else
	{
		GYRO_LOG("read  gyro sample rate: 0x%x\n", databuf[0]);
	}
	
	return MPU3000_SUCCESS;    
}
/*----------------------------------------------------------------------------*/
/*
static int MPU3000_SetIntEnable(struct i2c_client *client, u8 intenable)
{
	u8 databuf[2] = {0};    
	int res = 0;
	GYRO_FUN();    

	databuf[0] = MPU3000_REG_INT_EN;    
	databuf[1] = intenable;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		return MPU3000_ERR_I2C;
	}
	
	return MPU3000_SUCCESS;    
}
*/

#if 0
/*----------------------------------------------------------------------------*/
static int MPU3000_Reset(struct i2c_client *client, u8 reset)
{
	u8 databuf[2] = {0};    
	int res = 0;
	GYRO_FUN();    
	
	//read FIFO CTL register
	if(hwmsen_read_byte(client, MPU3000_REG_FIFO_CTL, databuf))
	{
		GYRO_ERR("read gyro FIFO CTRL register err!\n");
		return MPU3000_ERR_I2C;
	}
	else
	{
		GYRO_LOG("read  gyro FIFO CTRL: 0x%x\n", databuf[0]);
	}	

	//write the reset flag of this register
	databuf[1] = databuf[0] |reset;
	databuf[0] = MPU3000_REG_FIFO_CTL;    
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write FIFO CTRL register err!\n");
		return MPU3000_ERR_I2C;
	}
	
	GYRO_LOG("MPU3000_Reset OK!\n");
	
	return MPU3000_SUCCESS;    
}
#endif

/*----------------------------------------------------------------------------*/
static int MPU3000_FIFOConfig(struct i2c_client *client, u8 clk)
{
	u8 databuf[2] = {0};    
	int res = 0;
	GYRO_FUN();    

	//use gyro X, Y or Z for clocking
	databuf[0] = MPU3000_REG_PWR_CTL;    
	databuf[1] = clk;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write Power CTRL register err!\n");
		return MPU3000_ERR_I2C;
	}
	GYRO_LOG("MPU3000 use gyro X for clocking OK!\n");

	mdelay(50);
	
	//enable xyz gyro in FIFO
	databuf[0] = MPU3000_REG_FIFO_EN;    
	databuf[1] = (MPU3000_FIFO_GYROX_EN|MPU3000_FIFO_GYROY_EN|MPU3000_FIFO_GYROZ_EN);
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write Power CTRL register err!\n");
		return MPU3000_ERR_I2C;
	}
	GYRO_LOG("MPU3000 enable xyz gyro in FIFO OK!\n");

	//disable AUX_VDDIO
	databuf[0] = MPU3000_REG_AUX_VDD;    
	databuf[1] = MPU3000_AUX_VDDIO_DIS;
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write AUX_VDD register err!\n");
		return MPU3000_ERR_I2C;
	}
	GYRO_LOG("MPU3000 disable AUX_VDDIO OK!\n");
	
	//enable FIFO and reset FIFO
	databuf[0] = MPU3000_REG_FIFO_CTL;    
	databuf[1] = (MPU3000_FIFO_EN | MPU3000_FIFO_RST);
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write FIFO CTRL register err!\n");
		return MPU3000_ERR_I2C;
	}
	
	GYRO_LOG("MPU3000_FIFOConfig OK!\n");
	return MPU3000_SUCCESS;    
}

/*----------------------------------------------------------------------------*/
static int MPU3000_ReadFifoData(struct i2c_client *client, s16 *data, int* datalen)
{
	struct mpu3000_i2c_data *obj = i2c_get_clientdata(client);        
	u8 buf[MPU3000_DATA_LEN] = {0};
	s16 tmp1[MPU3000_AXES_NUM] = {0}; 
	s16 tmp2[MPU3000_AXES_NUM] = {0}; 
	int err = 0;
	u8 tmp = 0;
	int packet_cnt = 0;
	int i;
	GYRO_FUN();
	
	if(NULL == client)
	{
		return -EINVAL;
	}

	//stop putting data in FIFO
	MPU3000_ReadStart(client, FALSE);

	//read data number of bytes in FIFO
	err = hwmsen_read_byte(client, MPU3000_REG_FIFO_CNTH, &tmp);
	if(err)
	{
		GYRO_ERR("read data high number of bytes error: %d\n", err);
		return -1;
	}
	packet_cnt = tmp<< 8;

	err = hwmsen_read_byte(client, MPU3000_REG_FIFO_CNTL, &tmp);
	if(err)
	{
		GYRO_ERR("read data low number of bytes error: %d\n", err);
		return -1;
	}
	packet_cnt = (packet_cnt + tmp) /MPU3000_DATA_LEN;
	
	GYRO_LOG("MPU3000 Read Data packet number OK: %d\n", packet_cnt);

	*datalen = packet_cnt;
	
	//Within +-3% range: timing_tolerance * packet_thresh=0.03*75
	if(packet_cnt && (abs(packet_thresh -packet_cnt) < 4))
	{
		//read data in FIFO
		for(i = 0; i < packet_cnt; i++)
		{
			if(hwmsen_read_block(client, MPU3000_REG_FIFO_DATA, buf, MPU3000_DATA_LEN))
			{
				GYRO_ERR("MPU3000 read data from FIFO error: %d\n", err);
				return -2;
			}
			else
			{
				GYRO_LOG("MPU3000 read Data of diff address from FIFO OK !\n");
			}
				
			tmp1[MPU3000_AXIS_X] = (s16)((buf[MPU3000_AXIS_X*2+1]) | (buf[MPU3000_AXIS_X*2] << 8));
			tmp1[MPU3000_AXIS_Y] = (s16)((buf[MPU3000_AXIS_Y*2+1]) | (buf[MPU3000_AXIS_Y*2] << 8));
			tmp1[MPU3000_AXIS_Z] = (s16)((buf[MPU3000_AXIS_Z*2+1]) | (buf[MPU3000_AXIS_Z*2] << 8));
				
			//remap coordinate//
			tmp2[obj->cvt.map[MPU3000_AXIS_X]] = obj->cvt.sign[MPU3000_AXIS_X]*tmp1[MPU3000_AXIS_X];
			tmp2[obj->cvt.map[MPU3000_AXIS_Y]] = obj->cvt.sign[MPU3000_AXIS_Y]*tmp1[MPU3000_AXIS_Y];
			tmp2[obj->cvt.map[MPU3000_AXIS_Z]] = obj->cvt.sign[MPU3000_AXIS_Z]*tmp1[MPU3000_AXIS_Z];

			data[3* i +MPU3000_AXIS_X] = tmp2[MPU3000_AXIS_X];
			data[3* i +MPU3000_AXIS_Y] = tmp2[MPU3000_AXIS_Y];
			data[3* i +MPU3000_AXIS_Z] = tmp2[MPU3000_AXIS_Z];
			
			GYRO_LOG("gyro FIFO packet[%d]:[%04X %04X %04X] => [%5d %5d %5d]\n", i, 
			data[3*i +MPU3000_AXIS_X], data[3*i +MPU3000_AXIS_Y], data[3*i +MPU3000_AXIS_Z], 
			data[3*i +MPU3000_AXIS_X], data[3*i +MPU3000_AXIS_Y], data[3*i +MPU3000_AXIS_Z]);
		}

	}
	else
	{
		GYRO_ERR("MPU3000 Incorrect packet count: %d\n", packet_cnt);
		return -3;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU3000_ReadGyroData(struct i2c_client *client, char *buf, int bufsize)
{
	char databuf[6];	
	int data[3];
	struct mpu3000_i2c_data *obj = i2c_get_clientdata(client);  
	
	if(sensor_power == false)
	{
		MPU3000_SetPowerMode(client, true);
		msleep(50);
	}

	if(hwmsen_read_block(client, MPU3000_REG_GYRO_XH, databuf, 6))
	{
		GYRO_ERR("MPU3000 read gyroscope data  error\n");
		return -2;
	}
	else
	{
		obj->data[MPU3000_AXIS_X] = ((s16)((databuf[MPU3000_AXIS_X*2+1]) | (databuf[MPU3000_AXIS_X*2] << 8)));
		obj->data[MPU3000_AXIS_Y] = ((s16)((databuf[MPU3000_AXIS_Y*2+1]) | (databuf[MPU3000_AXIS_Y*2] << 8)));
		obj->data[MPU3000_AXIS_Z] = ((s16)((databuf[MPU3000_AXIS_Z*2+1]) | (databuf[MPU3000_AXIS_Z*2] << 8)));
#if DEBUG		
		if(atomic_read(&obj->trace) & GYRO_TRC_RAWDATA)
		{
			GYRO_LOG("read gyro register: %d, %d, %d, %d, %d, %d",
				databuf[0], databuf[1], databuf[2], databuf[3], databuf[4], databuf[5]);
			GYRO_LOG("get gyro raw data (0x%08X, 0x%08X, 0x%08X) -> (%5d, %5d, %5d)\n", 
				obj->data[MPU3000_AXIS_X],obj->data[MPU3000_AXIS_Y],obj->data[MPU3000_AXIS_Z],
				obj->data[MPU3000_AXIS_X],obj->data[MPU3000_AXIS_Y],obj->data[MPU3000_AXIS_Z]);
		}
#endif		

		//Out put the degree/second(o/s)
		obj->data[MPU3000_AXIS_X] = obj->data[MPU3000_AXIS_X] * MPU3000_FS_MAX_LSB / MPU3000_DEFAULT_LSB + obj->cali_sw[MPU3000_AXIS_X];
		obj->data[MPU3000_AXIS_Y] = obj->data[MPU3000_AXIS_Y] * MPU3000_FS_MAX_LSB / MPU3000_DEFAULT_LSB + obj->cali_sw[MPU3000_AXIS_Y];
		obj->data[MPU3000_AXIS_Z] = obj->data[MPU3000_AXIS_Z] * MPU3000_FS_MAX_LSB / MPU3000_DEFAULT_LSB + obj->cali_sw[MPU3000_AXIS_Z];


		/*remap coordinate*/
		data[obj->cvt.map[MPU3000_AXIS_X]] = obj->cvt.sign[MPU3000_AXIS_X]*obj->data[MPU3000_AXIS_X];
		data[obj->cvt.map[MPU3000_AXIS_Y]] = obj->cvt.sign[MPU3000_AXIS_Y]*obj->data[MPU3000_AXIS_Y];
		data[obj->cvt.map[MPU3000_AXIS_Z]] = obj->cvt.sign[MPU3000_AXIS_Z]*obj->data[MPU3000_AXIS_Z];
	
	}

	sprintf(buf, "%04x %04x %04x", data[MPU3000_AXIS_X],data[MPU3000_AXIS_Y],data[MPU3000_AXIS_Z]);

#if DEBUG		
	if(atomic_read(&obj->trace) & GYRO_TRC_DATA)
	{
		GYRO_LOG("get gyro data packet:[%d %d %d]\n", data[0], data[1], data[2]);
	}
#endif
	
	return 0;
	
}

//for factory mode
static int MPU3000_PROCESS_SMT_DATA(struct i2c_client *client, short *data)
{
    int total_num = 0;
	int retval =0;
    long xSum = 0;
    long ySum = 0;
    long zSum = 0;
    long xAvg, yAvg, zAvg;
    long xRMS, yRMS, zRMS;
	int i=0;

	int bias_thresh = 5242; // 40 dps * 131.072 LSB/dps
    //float RMS_thresh = 687.19f; // (.2 dps * 131.072) ^ 2
    long RMS_thresh = 68719; // (.2 dps * 131.072) ^ 2

	total_num = data[0];
	retval = data[1];
	GYRO_LOG("MPU3000 read gyro data OK, total number: %d \n", total_num); 
	for(i = 0; i < total_num; i++)
	{
		xSum =xSum + data[MPU3000_AXES_NUM*i + MPU3000_AXIS_X +2]; 
		ySum =ySum + data[MPU3000_AXES_NUM*i + MPU3000_AXIS_Y +2]; 
		zSum =zSum + data[MPU3000_AXES_NUM*i + MPU3000_AXIS_Z +2]; 
		
		/*
		FLPLOGD("read gyro data OK: packet_num:%d, [X:%5d, Y:%5d, Z:%5d]\n", i, data[MPU3000_AXES_NUM*i + MPU3000_AXIS_X +2], 
			data[MPU3000_AXES_NUM*i + MPU3000_AXIS_Y +2], data[MPU3000_AXES_NUM*i + MPU3000_AXIS_Z +2]);
		FLPLOGD("MPU3000 xSum: %5d,  ySum: %5d, zSum: %5d \n", xSum, ySum, zSum); 
		*/
	}
	GYRO_LOG("MPU3000 xSum: %5ld,  ySum: %5ld, zSum: %5ld \n", xSum, ySum, zSum); 
	
	if (total_num != 0)
	{
		xAvg = (xSum / total_num); 
		yAvg = (ySum / total_num); 
		zAvg = (zSum / total_num);
	}
	else
	{
		xAvg = xSum;
		yAvg = ySum;
		zAvg = zSum;
	}
	
	GYRO_LOG("MPU3000 xAvg: %ld,  yAvg: %ld,  zAvg: %ld \n", xAvg, yAvg, zAvg); 

	if ( abs(xAvg) >bias_thresh)
	{
		GYRO_LOG("X-Gyro bias exceeded threshold \n");
		retval |= 1 << 3;
	}
	if ( abs(yAvg) >  bias_thresh)
	{
		GYRO_LOG("Y-Gyro bias exceeded threshold \n");
		retval |= 1 << 4;
	}
	if ( abs(zAvg ) > bias_thresh)
	{
		GYRO_LOG("Z-Gyro bias exceeded threshold \n");
		retval |= 1 << 5;
	}

	xRMS = 0; 
	yRMS = 0; 
	zRMS = 0;

	//Finally, check RMS
	for ( i = 0; i < total_num ; i++)
	{
		xRMS += (data[MPU3000_AXES_NUM*i + MPU3000_AXIS_X+2]-xAvg)*(data[MPU3000_AXES_NUM*i + MPU3000_AXIS_X+2]-xAvg);
		yRMS += (data[MPU3000_AXES_NUM*i + MPU3000_AXIS_Y+2]-yAvg)*(data[MPU3000_AXES_NUM*i + MPU3000_AXIS_Y+2]-yAvg);
		zRMS += (data[MPU3000_AXES_NUM*i + MPU3000_AXIS_Z+2]-zAvg)*(data[MPU3000_AXES_NUM*i + MPU3000_AXIS_Z+2]-zAvg);        
	}

	GYRO_LOG("MPU3000 xRMS: %ld,  yRMS: %ld,  zRMS: %ld \n", xRMS, yRMS, zRMS); 
	xRMS = 100*xRMS;
	yRMS = 100*yRMS;
	zRMS = 100*zRMS;
	
	if (FACTORY_BOOT == get_boot_mode())
  		return retval;
	if ( xRMS > RMS_thresh * total_num)
	{
		GYRO_LOG("X-Gyro RMS exceeded threshold, RMS_thresh: %ld \n", RMS_thresh * total_num);
		retval |= 1 << 6;
	}
	if ( yRMS > RMS_thresh * total_num )
	{	
		GYRO_LOG("Y-Gyro RMS exceeded threshold, RMS_thresh: %ld \n", RMS_thresh * total_num);
		retval |= 1 << 7;
	}
	if ( zRMS > RMS_thresh * total_num )
	{
		GYRO_LOG("Z-Gyro RMS exceeded threshold, RMS_thresh: %ld \n", RMS_thresh * total_num);
		retval |= 1 << 8;
	}
	if ( xRMS == 0 || yRMS == 0 || zRMS == 0)
		//If any of the RMS noise value returns zero, then we might have dead gyro or FIFO/register failure
		retval |= 1 << 9;

	return retval;
	
}



/*----------------------------------------------------------------------------*/
static int MPU3000_SMTReadSensorData(struct i2c_client *client, s16 *buf, int bufsize)
{
	//S16 gyro[MPU3000_AXES_NUM*MPU3000_FIFOSIZE];
	int res = 0;
	int i;
	int datalen, total_num= 0;

	GYRO_FUN();

	if(sensor_power == false)
	{
		MPU3000_SetPowerMode(client, true);
	}

	if(NULL == buf)
	{
		return -1;
	}
	if(NULL == client)
	{
		*buf = 0;
		return -2;
	}
	
	for(i = 0; i < MPU3000_AXES_NUM; i++)
	{
		res = MPU3000_FIFOConfig(client, (i+1));
		if(res)
		{
			GYRO_ERR("MPU3000_FIFOConfig error:%d!\n", res);
			return -3;
		}
		
		//putting data in FIFO during the delayed 600ms
		mdelay(600);

		res = MPU3000_ReadFifoData(client, &(buf[total_num+2]), &datalen);
		if(res)
		{  
			if(res == (-3))
			{
				buf[1] = (1<< i);
			}
			else
			{
				GYRO_ERR("MPU3000_ReadData error:%d!\n", res);
				return -3;
			}
		}
		else
		{
			buf[0] = datalen;
			total_num+=datalen*MPU3000_AXES_NUM;
		}
	}

	GYRO_LOG("gyroscope read data OK, total packet: %d", buf[0] ); 
	
	return 0;
}

/*----------------------------------------------------------------------------*/
static int MPU3000_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
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

	sprintf(buf, "MPU3000 Chip");
	return 0;
}


/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mpu3000_i2c_client;
	char strbuf[MPU3000_BUFSIZE];
	if(NULL == client)
	{
		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}
	
	MPU3000_ReadChipInfo(client, strbuf, MPU3000_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);        
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = mpu3000_i2c_client;
	char strbuf[MPU3000_BUFSIZE];
	
	if(NULL == client)
	{
		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}
	
	MPU3000_ReadGyroData(client, strbuf, MPU3000_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);;            
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct mpu3000_i2c_data *obj = obj_i2c_data;
	if (obj == NULL)
	{
		GYRO_ERR("i2c_data obj is null!!\n");
		return 0;
	}
	
	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));     
	return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct mpu3000_i2c_data *obj = obj_i2c_data;
	int trace;
	if (obj == NULL)
	{
		GYRO_ERR("i2c_data obj is null!!\n");
		return 0;
	}
	
	if(1 == sscanf(buf, "0x%x", &trace))
	{
		atomic_set(&obj->trace, trace);
	}	
	else
	{
		GYRO_ERR("invalid content: '%s', length = %d\n", buf, count);
	}
	
	return count;    
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;    
	struct mpu3000_i2c_data *obj = obj_i2c_data;
	if (obj == NULL)
	{
		GYRO_ERR("i2c_data obj is null!!\n");
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
static struct driver_attribute *MPU3000_attr_list[] = {
	&driver_attr_chipinfo,     /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/	
	&driver_attr_trace,        /*trace log*/
	&driver_attr_status,        
};
/*----------------------------------------------------------------------------*/
static int mpu3000_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(MPU3000_attr_list)/sizeof(MPU3000_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if(0 != (err = driver_create_file(driver, MPU3000_attr_list[idx])))
		{            
			GYRO_ERR("driver_create_file (%s) = %d\n", MPU3000_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int mpu3000_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(MPU3000_attr_list)/sizeof(MPU3000_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, MPU3000_attr_list[idx]);
	}
	

	return err;
}

/*----------------------------------------------------------------------------*/
static int mpu3000_gpio_config(void)
{
    //because we donot use EINT ,to support low power
    // config to GPIO input mode + PD    
    //set   GPIO_MSE_EINT_PIN
    mt_set_gpio_mode(GPIO_GYRO_EINT_PIN, GPIO_GYRO_EINT_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_GYRO_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_GYRO_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_GYRO_EINT_PIN, GPIO_PULL_DOWN);
	return 0;
}
static int mpu3000_init_client(struct i2c_client *client, bool enable)
{
	struct mpu3000_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;
	GYRO_FUN();	
	mpu3000_gpio_config();

	res = MPU3000_SetPowerMode(client, enable);
	if(res != MPU3000_SUCCESS)
	{
		return res;
	}
	
	
	
	// The range should at least be 17.45 rad/s (ie: ~1000 deg/s).
	res = MPU3000_SetDataFormat(client, (MPU3000_SYNC_GYROX << MPU3000_EXT_SYNC)|
										(MPU3000_DEFAULT_FS << MPU3000_FS_RANGE)|
										MPU3000_RATE_1K_LPFB_188HZ);
	if(res != MPU3000_SUCCESS) 
	{
		return res;
	}

	// Set 125HZ sample rate
	res = MPU3000_SetSampleRate(client, 125);
	if(res != MPU3000_SUCCESS ) 
	{
		return res;
	}

	GYRO_LOG("mpu3000_init_client OK!\n");

#ifdef CONFIG_MPU3000_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));  
#endif

	return MPU3000_SUCCESS;
}

/*----------------------------------------------------------------------------*/
int mpu3000_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;	
	struct mpu3000_i2c_data *priv = (struct mpu3000_i2c_data*)self;
	hwm_sensor_data* gyro_data;
	char buff[MPU3000_BUFSIZE];	

	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				GYRO_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			else
			{
			
			}
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				GYRO_ERR("Enable gyroscope parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(((value == 0) && (sensor_power == false)) ||((value == 1) && (sensor_power == true)))
				{
					GYRO_LOG("gyroscope device have updated!\n");
				}
				else
				{
					err = MPU3000_SetPowerMode(priv->client, !sensor_power);
				}
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				GYRO_ERR("get gyroscope data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				gyro_data = (hwm_sensor_data *)buff_out;
				MPU3000_ReadGyroData(priv->client, buff, MPU3000_BUFSIZE);
				sscanf(buff, "%x %x %x", &gyro_data->values[0], 
									&gyro_data->values[1], &gyro_data->values[2]);				
				gyro_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;				
				gyro_data->value_divide = DEGREE_TO_RAD;
			}
			break;
		default:
			GYRO_ERR("gyroscope operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}

	return err;
}

/****************************************************************************** 
 * Function Configuration
******************************************************************************/
static int mpu3000_open(struct inode *inode, struct file *file)
{
	file->private_data = mpu3000_i2c_client;

	if(file->private_data == NULL)
	{
		GYRO_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int mpu3000_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
//static int mpu3000_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
//       unsigned long arg)
static long mpu3000_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	//struct mpu3000_i2c_data *obj = (struct mpu3000_i2c_data*)i2c_get_clientdata(client);	
	char strbuf[MPU3000_BUFSIZE] = {0};
	s16 *SMTdata;
	void __user *data;
	long err = 0;
	int copy_cnt = 0;
	SENSOR_DATA sensor_data;
	int cali[3];
	int smtRes=0;
	//GYRO_FUN();
	
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
		GYRO_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch(cmd)
	{
		case GYROSCOPE_IOCTL_INIT:
			mpu3000_init_client(client, false);			
			break;

		case GYROSCOPE_IOCTL_SMT_DATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}

			SMTdata = kzalloc(sizeof(*SMTdata) * 800, GFP_KERNEL);
			if(SMTdata == NULL)
			{
				err = -ENOMEM;
				break;
			}
			memset(SMTdata, 0, sizeof(*SMTdata) * 800);
			MPU3000_SMTReadSensorData(client, SMTdata, 800);
			//GYRO_LOG("gyroscope read data from kernel OK: sizeof:%d, strlen:%d, packet:%d!\n", 
				//sizeof(SMTdata), strlen(SMTdata), SMTdata[0]);
			GYRO_LOG("gyroscope read data from kernel OK: SMTdata[0]:%d, copied packet:%d!\n", SMTdata[0],
			((SMTdata[0]*MPU3000_AXES_NUM+2)*sizeof(s16)+1));
			
			smtRes = MPU3000_PROCESS_SMT_DATA(client,SMTdata);
			copy_cnt = copy_to_user(data, &smtRes,  sizeof(smtRes));
			kfree(SMTdata);
			if(copy_cnt)
			{
				err = -EFAULT;
				GYRO_ERR("copy gyro data to user failed!\n");
			}	
			GYRO_LOG("copy gyro data to user OK: %d!\n", copy_cnt);
			break;

		case GYROSCOPE_IOCTL_READ_SENSORDATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			
			MPU3000_ReadGyroData(client, strbuf, MPU3000_BUFSIZE);
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
				cali[MPU3000_AXIS_X] = sensor_data.x ;
				cali[MPU3000_AXIS_Y] = sensor_data.y ;
				cali[MPU3000_AXIS_Z] = sensor_data.z ;
				err = MPU3000_WriteCalibration(client, cali);
			}
			break;

		case GYROSCOPE_IOCTL_CLR_CALI:
			err = MPU3000_ResetCalibration(client);
			break;

		case GYROSCOPE_IOCTL_GET_CALI:
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			err = MPU3000_ReadCalibration(client, cali);
			if(err)
			{
				break;
			}
			sensor_data.x = cali[MPU3000_AXIS_X] ;
			sensor_data.y = cali[MPU3000_AXIS_Y] ;
			sensor_data.z = cali[MPU3000_AXIS_Z] ;
			if(copy_to_user(data, &sensor_data, sizeof(sensor_data)))
			{
				err = -EFAULT;
				break;
			}		
			break;

		default:
			GYRO_ERR("unknown IOCTL: 0x%08x\n", cmd);
			err = -ENOIOCTLCMD;
			break;			
	}
	return err;
}


/*----------------------------------------------------------------------------*/
static struct file_operations mpu3000_fops = {
//	.owner = THIS_MODULE,//modified
	.open = mpu3000_open,
	.release = mpu3000_release,
	.unlocked_ioctl = mpu3000_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice mpu3000_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gyroscope",
	.fops = &mpu3000_fops,
};
/*----------------------------------------------------------------------------*/
#ifndef CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int mpu3000_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct mpu3000_i2c_data *obj = i2c_get_clientdata(client);    
	int err; 
	GYRO_FUN();   

	if(msg.event == PM_EVENT_SUSPEND)
	{   
		if(obj == NULL)
		{
			GYRO_ERR("null pointer!!\n");
			return -EINVAL;
		}
		atomic_set(&obj->suspend, 1);		
		
		err = MPU3000_SetPowerMode(client, false);
		if(err <= 0)
		{
			return err;
		}
	}
	return 0;//modified
}
/*----------------------------------------------------------------------------*/
static int mpu3000_resume(struct i2c_client *client)
{
	struct mpu3000_i2c_data *obj = i2c_get_clientdata(client);        
	int err;
	GYRO_FUN();

	if(obj == NULL)
	{
		GYRO_ERR("null pointer!!\n");
		return -EINVAL;
	}

	MPU3000_power(obj->hw, 1);
	err = mpu3000_init_client(client, false);
	if(err)
	{
		GYRO_ERR("initialize client fail!!\n");
		return err;        
	}
	atomic_set(&obj->suspend, 0);

	return 0;
}
/*----------------------------------------------------------------------------*/
#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void mpu3000_early_suspend(struct early_suspend *h) 
{
	struct mpu3000_i2c_data *obj = container_of(h, struct mpu3000_i2c_data, early_drv);   
	int err;
	u8 databuf[2];
	GYRO_FUN();    

	if(obj == NULL)
	{
		GYRO_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1);
	err = MPU3000_SetPowerMode(obj->client, false);
	if(err)
	{
		GYRO_ERR("write power control fail!!\n");
		return;
	}

	databuf[0] = MPU3000_REG_PWR_CTL;    
	databuf[1] = MPU3000_SLEEP;
	err = i2c_master_send(obj->client, databuf, 0x2);
	if(err <= 0)
	{
		return;
	}

	sensor_power = false;
	
	MPU3000_power(obj->hw, 0);
}
/*----------------------------------------------------------------------------*/
static void mpu3000_late_resume(struct early_suspend *h)
{
	struct mpu3000_i2c_data *obj = container_of(h, struct mpu3000_i2c_data, early_drv);         
	int err;
	GYRO_FUN();

	if(obj == NULL)
	{
		GYRO_ERR("null pointer!!\n");
		return;
	}

	MPU3000_power(obj->hw, 1);
	err = mpu3000_init_client(obj->client, false);
	if(err)
	{
		GYRO_ERR("initialize client fail! err code %d!\n", err);
		return;        
	}
	atomic_set(&obj->suspend, 0);    
}
/*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/
/*static int mpu3000_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{    
	strcpy(info->type, MPU3000_DEV_NAME);
	return 0;
}
*/
/*----------------------------------------------------------------------------*/
static int mpu3000_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct mpu3000_i2c_data *obj;
	struct hwmsen_object sobj;
	int err = 0;
	GYRO_FUN();

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	
	memset(obj, 0, sizeof(struct mpu3000_i2c_data));

	obj->hw = get_cust_gyro_hw();
	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if(err)
	{
		GYRO_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit;
	}

	
    GYRO_LOG("gyro_default_i2c_addr: %x\n", client->addr);
	GYRO_LOG("gyro_custom_i2c_addr: %x\n", obj->hw->addr);
	if(0!=obj->hw->addr)
	{
	  client->addr = obj->hw->addr >> 1;
	  GYRO_LOG("gyro_use_i2c_addr: %x\n", client->addr);
	}
	
	obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client,obj);
	
	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	


	mpu3000_i2c_client = new_client;	
	err = mpu3000_init_client(new_client, false);
	if(err)
	{
		goto exit_init_failed;
	}
	

	err = misc_register(&mpu3000_device);
	if(err)
	{
		GYRO_ERR("mpu3000_device misc register failed!\n");
		goto exit_misc_device_register_failed;
	}

	err = mpu3000_create_attr(&mpu3000_gyro_driver.driver);
	if(err)
	{
		GYRO_ERR("mpu3000 create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	

	sobj.self = obj;
    sobj.polling = 1;
    sobj.sensor_operate = mpu3000_operate;
	err = hwmsen_attach(ID_GYROSCOPE, &sobj);
	if(err)
	{
		GYRO_ERR("hwmsen_attach fail = %d\n", err);
		goto exit_kfree;
	}
	

#ifdef CONFIG_HAS_EARLYSUSPEND
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	obj->early_drv.suspend  = mpu3000_early_suspend,
	obj->early_drv.resume   = mpu3000_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif 

	GYRO_LOG("%s: OK\n", __func__);    
	return 0;

	exit_create_attr_failed:
	misc_deregister(&mpu3000_device);
	exit_misc_device_register_failed:
	exit_init_failed:
	//i2c_detach_client(new_client);
	exit_kfree:
	kfree(obj);
	exit:
	GYRO_ERR("%s: err = %d\n", __func__, err);        
	return err;
}

/*----------------------------------------------------------------------------*/
static int mpu3000_i2c_remove(struct i2c_client *client)
{
	int err = 0;	
	
	err = mpu3000_delete_attr(&mpu3000_gyro_driver.driver);
	if(err)
	{
		GYRO_ERR("mpu3000_delete_attr fail: %d\n", err);
	}
	
	err = misc_deregister(&mpu3000_device);
	if(err)
	{
		GYRO_ERR("misc_deregister fail: %d\n", err);
	}

	err = hwmsen_detach(ID_ACCELEROMETER);
	if(err)
	{
		GYRO_ERR("hwmsen_detach fail: %d\n", err);
	}

	mpu3000_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int mpu3000_probe(struct platform_device *pdev) 
{
	struct gyro_hw *hw = get_cust_gyro_hw();
	GYRO_FUN();

	MPU3000_power(hw, 1);
	//mpu3000_force[0] = hw->i2c_num;
	if(i2c_add_driver(&mpu3000_i2c_driver))
	{
		GYRO_ERR("add driver error\n");
		return -1;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int mpu3000_remove(struct platform_device *pdev)
{
    struct gyro_hw *hw = get_cust_gyro_hw();

    GYRO_FUN();    
    MPU3000_power(hw, 0);    
    i2c_del_driver(&mpu3000_i2c_driver);
    return 0;
}
/*----------------------------------------------------------------------------*/
#if 1
#ifdef CONFIG_OF
static const struct of_device_id gyroscope_of_match[] = {
	{ .compatible = "mediatek,gyroscope", },
	{},
};
#endif

static struct platform_driver mpu3000_gyro_driver = {
	.probe      = mpu3000_probe,
	.remove     = mpu3000_remove,    
	.driver     = 
	{
		.name  = "gyroscope",
	//	.owner  = THIS_MODULE,
        #ifdef CONFIG_OF
		.of_match_table = gyroscope_of_match,
		#endif
	}
};
#else

static struct platform_driver mpu3000_gyro_driver = {
	.probe      = mpu3000_probe,
	.remove     = mpu3000_remove,    
	.driver     = {
		.name  = "gyroscope",
//		.owner = THIS_MODULE,//modified
	}
};
#endif
/*----------------------------------------------------------------------------*/
static int __init mpu3000_init(void)
{
	//GYRO_FUN();
	struct gyro_hw *hw = get_cust_gyro_hw();
	GYRO_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	i2c_register_board_info(hw->i2c_num, &i2c_mpu3000, 1);
	if(platform_driver_register(&mpu3000_gyro_driver))
	{
		GYRO_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit mpu3000_exit(void)
{
	GYRO_FUN();
	platform_driver_unregister(&mpu3000_gyro_driver);
}
/*----------------------------------------------------------------------------*/
module_init(mpu3000_init);
module_exit(mpu3000_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MPU3000 gyroscope driver");
MODULE_AUTHOR("Chunlei.Wang@mediatek.com");
