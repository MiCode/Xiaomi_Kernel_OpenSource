/* L3G4200D motion sensor driver
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
#include "l3g4200d.h"
#include <linux/hwmsen_helper.h>
#include <linux/kernel.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#include <mach/mt_boot.h>

/*-------------------------MT6516&MT6573 define-------------------------------*/

#define POWER_NONE_MACRO MT65XX_POWER_NONE
/*----------------------------------------------------------------------------*/
#define I2C_DRIVERID_L3G4200D	3000
/*----------------------------------------------------------------------------*/
//#define L3G4200D_DEFAULT_FS		L3G4200D_FS_1000
//#define L3G4200D_DEFAULT_LSB		L3G4200D_FS_250_LSB
/*---------------------------------------------------------------------------*/
#define DEBUG 1
/*----------------------------------------------------------------------------*/
#define CONFIG_L3G4200D_LOWPASS   /*apply low pass filter on output*/       
/*----------------------------------------------------------------------------*/
#define L3G4200D_AXIS_X          0
#define L3G4200D_AXIS_Y          1
#define L3G4200D_AXIS_Z          2
#define L3G4200D_AXES_NUM        3
#define L3G4200D_DATA_LEN        6   
#define L3G4200D_DEV_NAME        "L3G4200D"
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id l3g4200d_i2c_id[] = {{L3G4200D_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_l3g4200d={ I2C_BOARD_INFO(L3G4200D_DEV_NAME, (0xD0>>1))};
/*the adapter id will be available in customization*/
//static unsigned short l3g4200d_force[] = {0x00, L3G4200D_I2C_SLAVE_ADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const l3g4200d_forces[] = { l3g4200d_force, NULL };
//static struct i2c_client_address_data l3g4200d_addr_data = { .forces = l3g4200d_forces,};

int packet_thresh = 75; // 600 ms / 8ms/sample

/*----------------------------------------------------------------------------*/
static int l3g4200d_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int l3g4200d_i2c_remove(struct i2c_client *client);
static int l3g4200d_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int l3g4200d_init_client(struct i2c_client *client, bool enable);

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
    s16 raw[C_MAX_FIR_LENGTH][L3G4200D_AXES_NUM];
    int sum[L3G4200D_AXES_NUM];
    int num;
    int idx;
};
/*----------------------------------------------------------------------------*/
struct l3g4200d_i2c_data {
    struct i2c_client *client;
    struct gyro_hw *hw;
    struct hwmsen_convert   cvt;
    
    /*misc*/
    struct data_resolution *reso;
    atomic_t                trace;
    atomic_t                suspend;
    atomic_t                selftest;
    atomic_t				filter;
    s16                     cali_sw[L3G4200D_AXES_NUM+1];

    /*data*/
    s8                      offset[L3G4200D_AXES_NUM+1];  /*+1: for 4-byte alignment*/
    s16                     data[L3G4200D_AXES_NUM+1];

#if defined(CONFIG_L3G4200D_LOWPASS)
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
static struct i2c_driver l3g4200d_i2c_driver = {
    .driver = {
//      .owner          = THIS_MODULE,
        .name           = L3G4200D_DEV_NAME,
    },
	.probe      		= l3g4200d_i2c_probe,
	.remove    			= l3g4200d_i2c_remove,
	.detect				= l3g4200d_i2c_detect,
#if !defined(CONFIG_HAS_EARLYSUSPEND)    
    .suspend            = l3g4200d_suspend,
    .resume             = l3g4200d_resume,
#endif
	.id_table = l3g4200d_i2c_id,
//	.address_data = &l3g4200d_addr_data,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *l3g4200d_i2c_client = NULL;
static struct platform_driver l3g4200d_gyro_driver;
static struct l3g4200d_i2c_data *obj_i2c_data = NULL;
static bool sensor_power = false;



/*----------------------------------------------------------------------------*/
#define GYRO_TAG                  "[Gyroscope] "
#define GYRO_FUN(f)               printk(KERN_INFO GYRO_TAG"%s\n", __FUNCTION__)
#define GYRO_ERR(fmt, args...)    printk(KERN_ERR GYRO_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GYRO_LOG(fmt, args...)    printk(KERN_INFO GYRO_TAG fmt, ##args)
/*----------------------------------------------------------------------------*/

static void L3G4200D_dumpReg(struct i2c_client *client)
{
  int i=0;
  u8 addr = 0x20;
  u8 regdata=0;
  for(i=0; i<25 ; i++)
  {
    //dump all
    hwmsen_read_byte(client,addr,&regdata);
	HWM_LOG("Reg addr=%x regdata=%x\n",addr,regdata);
	addr++;
	
  }

}


/*--------------------gyroscopy power control function----------------------------------*/
static void L3G4200D_power(struct gyro_hw *hw, unsigned int on) 
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
			if(!hwPowerOn(hw->power_id, hw->power_vol, "L3G4200D"))
			{
				GYRO_ERR("power on fails!!\n");
			}
		}
		else	// power off
		{
			if (!hwPowerDown(hw->power_id, "L3G4200D"))
			{
				GYRO_ERR("power off fail!!\n");
			}			  
		}
	}
	power_on = on;    
}
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
static int L3G4200D_write_rel_calibration(struct l3g4200d_i2c_data *obj, int dat[L3G4200D_AXES_NUM])
{
    obj->cali_sw[L3G4200D_AXIS_X] = obj->cvt.sign[L3G4200D_AXIS_X]*dat[obj->cvt.map[L3G4200D_AXIS_X]];
    obj->cali_sw[L3G4200D_AXIS_Y] = obj->cvt.sign[L3G4200D_AXIS_Y]*dat[obj->cvt.map[L3G4200D_AXIS_Y]];
    obj->cali_sw[L3G4200D_AXIS_Z] = obj->cvt.sign[L3G4200D_AXIS_Z]*dat[obj->cvt.map[L3G4200D_AXIS_Z]];
#if DEBUG		
		if(atomic_read(&obj->trace) & GYRO_TRC_CALI)
		{
			GYRO_LOG("test  (%5d, %5d, %5d) ->(%5d, %5d, %5d)->(%5d, %5d, %5d))\n", 
				obj->cvt.sign[L3G4200D_AXIS_X],obj->cvt.sign[L3G4200D_AXIS_Y],obj->cvt.sign[L3G4200D_AXIS_Z],
				dat[L3G4200D_AXIS_X], dat[L3G4200D_AXIS_Y], dat[L3G4200D_AXIS_Z],
				obj->cvt.map[L3G4200D_AXIS_X],obj->cvt.map[L3G4200D_AXIS_Y],obj->cvt.map[L3G4200D_AXIS_Z]);
			GYRO_LOG("write gyro calibration data  (%5d, %5d, %5d)\n", 
				obj->cali_sw[L3G4200D_AXIS_X],obj->cali_sw[L3G4200D_AXIS_Y],obj->cali_sw[L3G4200D_AXIS_Z]);
		}
#endif
    return 0;
}


/*----------------------------------------------------------------------------*/
static int L3G4200D_ResetCalibration(struct i2c_client *client)
{
	struct l3g4200d_i2c_data *obj = i2c_get_clientdata(client);	

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	return 0;    
}
/*----------------------------------------------------------------------------*/
static int L3G4200D_ReadCalibration(struct i2c_client *client, int dat[L3G4200D_AXES_NUM])
{
    struct l3g4200d_i2c_data *obj = i2c_get_clientdata(client);

    dat[obj->cvt.map[L3G4200D_AXIS_X]] = obj->cvt.sign[L3G4200D_AXIS_X]*obj->cali_sw[L3G4200D_AXIS_X];
    dat[obj->cvt.map[L3G4200D_AXIS_Y]] = obj->cvt.sign[L3G4200D_AXIS_Y]*obj->cali_sw[L3G4200D_AXIS_Y];
    dat[obj->cvt.map[L3G4200D_AXIS_Z]] = obj->cvt.sign[L3G4200D_AXIS_Z]*obj->cali_sw[L3G4200D_AXIS_Z];

#if DEBUG		
		if(atomic_read(&obj->trace) & GYRO_TRC_CALI)
		{
			GYRO_LOG("Read gyro calibration data  (%5d, %5d, %5d)\n", 
				dat[L3G4200D_AXIS_X],dat[L3G4200D_AXIS_Y],dat[L3G4200D_AXIS_Z]);
		}
#endif
                                       
    return 0;
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static int L3G4200D_WriteCalibration(struct i2c_client *client, int dat[L3G4200D_AXES_NUM])
{
	struct l3g4200d_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;
	int cali[L3G4200D_AXES_NUM];


	GYRO_FUN();
	if(!obj || ! dat)
	{
		GYRO_ERR("null ptr!!\n");
		return -EINVAL;
	}
	else
	{        		
		cali[obj->cvt.map[L3G4200D_AXIS_X]] = obj->cvt.sign[L3G4200D_AXIS_X]*obj->cali_sw[L3G4200D_AXIS_X];
		cali[obj->cvt.map[L3G4200D_AXIS_Y]] = obj->cvt.sign[L3G4200D_AXIS_Y]*obj->cali_sw[L3G4200D_AXIS_Y];
		cali[obj->cvt.map[L3G4200D_AXIS_Z]] = obj->cvt.sign[L3G4200D_AXIS_Z]*obj->cali_sw[L3G4200D_AXIS_Z]; 
		cali[L3G4200D_AXIS_X] += dat[L3G4200D_AXIS_X];
		cali[L3G4200D_AXIS_Y] += dat[L3G4200D_AXIS_Y];
		cali[L3G4200D_AXIS_Z] += dat[L3G4200D_AXIS_Z];
#if DEBUG		
		if(atomic_read(&obj->trace) & GYRO_TRC_CALI)
		{
			GYRO_LOG("write gyro calibration data  (%5d, %5d, %5d)-->(%5d, %5d, %5d)\n", 
				dat[L3G4200D_AXIS_X], dat[L3G4200D_AXIS_Y], dat[L3G4200D_AXIS_Z],
				cali[L3G4200D_AXIS_X],cali[L3G4200D_AXIS_Y],cali[L3G4200D_AXIS_Z]);
		}
#endif
		return L3G4200D_write_rel_calibration(obj, cali);
	} 

	return err;
}
/*----------------------------------------------------------------------------*/
static int L3G4200D_CheckDeviceID(struct i2c_client *client)
{
	u8 databuf[10];    
	int res = 0;

	memset(databuf, 0, sizeof(u8)*10);    
	databuf[0] = L3G4200D_FIXED_DEVID;    

	res = hwmsen_read_byte(client,L3G4200D_REG_DEVID,databuf);
    GYRO_LOG(" L3G4200D  id %x!\n",databuf[0]);
	if(databuf[0]!=L3G4200D_FIXED_DEVID)
	{
		return L3G4200D_ERR_IDENTIFICATION;
	}

	//exit_MMA8453Q_CheckDeviceID:
	if (res < 0)
	{
		return L3G4200D_ERR_I2C;
	}
	
	return L3G4200D_SUCCESS;
}


//----------------------------------------------------------------------------//
static int L3G4200D_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2] = {0};    
	int res = 0;

	if(enable == sensor_power)
	{
		GYRO_LOG("Sensor power status is newest!\n");
		return L3G4200D_SUCCESS;
	}

	if(hwmsen_read_byte(client, L3G4200D_CTL_REG1, databuf))
	{
		GYRO_ERR("read power ctl register err!\n");
		return L3G4200D_ERR_I2C;
	}

	databuf[0] &= ~L3G4200D_POWER_ON;//clear power on bit
	if(true == enable )
	{
		databuf[0] |= L3G4200D_POWER_ON;
	}
	else
	{
		// do nothing
	}
	databuf[1] = databuf[0];
	databuf[0] = L3G4200D_CTL_REG1;    
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_LOG("set power mode failed!\n");
		return L3G4200D_ERR_I2C;
	}	
	else
	{
		GYRO_LOG("set power mode ok %d!\n", enable);
	}

	sensor_power = enable;
	
	return L3G4200D_SUCCESS;    
}

/*----------------------------------------------------------------------------*/


static int L3G4200D_SetDataResolution(struct i2c_client *client, u8 dataResolution)
{
	u8 databuf[2] = {0};    
	int res = 0;
	GYRO_FUN();     
	
	if(hwmsen_read_byte(client, L3G4200D_CTL_REG4, databuf))
	{
		GYRO_ERR("read L3G4200D_CTL_REG4 err!\n");
		return L3G4200D_ERR_I2C;
	}
	else
	{
		GYRO_LOG("read  L3G4200D_CTL_REG4 register: 0x%x\n", databuf[0]);
	}

	databuf[0] &= 0xcf;//clear 
	databuf[0] |= dataResolution;
	
	databuf[1] = databuf[0];
	databuf[0] = L3G4200D_CTL_REG4; 
	
	
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write SetDataResolution register err!\n");
		return L3G4200D_ERR_I2C;
	}
	return L3G4200D_SUCCESS;    
}

// set the sample rate
static int L3G4200D_SetSampleRate(struct i2c_client *client, u8 sample_rate)
{
	u8 databuf[2] = {0}; 
	int res = 0;
	GYRO_FUN();    

	if(hwmsen_read_byte(client, L3G4200D_CTL_REG1, databuf))
	{
		GYRO_ERR("read gyro data format register err!\n");
		return L3G4200D_ERR_I2C;
	}
	else
	{
		GYRO_LOG("read  gyro data format register: 0x%x\n", databuf[0]);
	}

	databuf[0] &= 0x3f;//clear 
	databuf[0] |= sample_rate;
	
	databuf[1] = databuf[0];
	databuf[0] = L3G4200D_CTL_REG1; 
	
	
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write sample rate register err!\n");
		return L3G4200D_ERR_I2C;
	}
	
	return L3G4200D_SUCCESS;    
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int L3G4200D_ReadGyroData(struct i2c_client *client, char *buf, int bufsize)
{
	char databuf[6];	
	int data[3];
	struct l3g4200d_i2c_data *obj = i2c_get_clientdata(client);  
	
	if(sensor_power == false)
	{
		L3G4200D_SetPowerMode(client, true);
	}

	if(hwmsen_read_block(client, AUTO_INCREMENT |L3G4200D_REG_GYRO_XL, databuf, 6))
	{
		GYRO_ERR("L3G4200D read gyroscope data  error\n");
		return -2;
	}
	else
	{
		obj->data[L3G4200D_AXIS_X] = (s16)((databuf[L3G4200D_AXIS_X*2+1] << 8) | (databuf[L3G4200D_AXIS_X*2]));
		obj->data[L3G4200D_AXIS_Y] = (s16)((databuf[L3G4200D_AXIS_Y*2+1] << 8) | (databuf[L3G4200D_AXIS_Y*2]));
		obj->data[L3G4200D_AXIS_Z] = (s16)((databuf[L3G4200D_AXIS_Z*2+1] << 8) | (databuf[L3G4200D_AXIS_Z*2]));
		
#if DEBUG		
		if(atomic_read(&obj->trace) & GYRO_TRC_RAWDATA)
		{
			GYRO_LOG("read gyro register: %x, %x, %x, %x, %x, %x",
				databuf[0], databuf[1], databuf[2], databuf[3], databuf[4], databuf[5]);
			GYRO_LOG("get gyro raw data (0x%08X, 0x%08X, 0x%08X) -> (%5d, %5d, %5d)\n", 
				obj->data[L3G4200D_AXIS_X],obj->data[L3G4200D_AXIS_Y],obj->data[L3G4200D_AXIS_Z],
				obj->data[L3G4200D_AXIS_X],obj->data[L3G4200D_AXIS_Y],obj->data[L3G4200D_AXIS_Z]);
		}
#endif		
		obj->data[L3G4200D_AXIS_X] = obj->data[L3G4200D_AXIS_X] + obj->cali_sw[L3G4200D_AXIS_X];
		obj->data[L3G4200D_AXIS_Y] = obj->data[L3G4200D_AXIS_Y] + obj->cali_sw[L3G4200D_AXIS_Y];
		obj->data[L3G4200D_AXIS_Z] = obj->data[L3G4200D_AXIS_Z] + obj->cali_sw[L3G4200D_AXIS_Z];
	
		/*remap coordinate*/
		data[obj->cvt.map[L3G4200D_AXIS_X]] = obj->cvt.sign[L3G4200D_AXIS_X]*obj->data[L3G4200D_AXIS_X];
		data[obj->cvt.map[L3G4200D_AXIS_Y]] = obj->cvt.sign[L3G4200D_AXIS_Y]*obj->data[L3G4200D_AXIS_Y];
		data[obj->cvt.map[L3G4200D_AXIS_Z]] = obj->cvt.sign[L3G4200D_AXIS_Z]*obj->data[L3G4200D_AXIS_Z];

	
		//Out put the degree/second(o/s)
		data[L3G4200D_AXIS_X] = data[L3G4200D_AXIS_X] * L3G4200D_OUT_MAGNIFY / L3G4200D_FS_2000_LSB;
		data[L3G4200D_AXIS_Y] = data[L3G4200D_AXIS_Y] * L3G4200D_OUT_MAGNIFY / L3G4200D_FS_2000_LSB;
		data[L3G4200D_AXIS_Z] = data[L3G4200D_AXIS_Z] * L3G4200D_OUT_MAGNIFY / L3G4200D_FS_2000_LSB;
	 
	}

	sprintf(buf, "%04x %04x %04x", data[L3G4200D_AXIS_X],data[L3G4200D_AXIS_Y],data[L3G4200D_AXIS_Z]);

#if DEBUG		
	if(atomic_read(&obj->trace) & GYRO_TRC_DATA)
	{
		GYRO_LOG("get gyro data packet:[%d %d %d]\n", data[0], data[1], data[2]);
	}
#endif
	
	return 0;
	
}

// if we use internel fifo then compile the function L3G4200D_SET_FIFO_MODE
#if 0

/*----------------------------------------------------------------------------*/
static int L3G4200D_SET_FIFO_MODE(struct i2c_client *client,u8 config)
{
    u8 databuf[2] = {0};    
	int res = 0;
	GYRO_FUN();     
	
	if(hwmsen_read_byte(client, L3G4200D_FIFO_CTL, databuf))
	{
		GYRO_ERR("read L3G4200D_CTL_REG4 err!\n");
		return L3G4200D_ERR_I2C;
	}
	else
	{
		GYRO_LOG("read  L3G4200D_CTL_REG4 register: 0x%x\n", databuf[0]);
	}

	databuf[0] &= 0x1f;//clear 
	databuf[0] |= config;
	
	databuf[1] = databuf[0];
	databuf[0] = L3G4200D_FIFO_CTL; 
	
	
	res = i2c_master_send(client, databuf, 0x2);
	if(res <= 0)
	{
		GYRO_ERR("write L3G4200D_SET_FIFO_MODE register err!\n");
		return L3G4200D_ERR_I2C;
	}
	return L3G4200D_SUCCESS;   
}
#endif

static int L3G4200D_SelfTest(struct i2c_client *client)
{
    int err =0;
	u8 data=0;
	char strbuf[L3G4200D_BUFSIZE] = {0};
	int avgx_NOST,avgy_NOST,avgz_NOST;
	int sumx,sumy,sumz;
	int avgx_ST,avgy_ST,avgz_ST;
	int nost_x,nost_y,nost_z=0;
	int st_x,st_y,st_z=0;

	int resx,resy,resz=-1;
	int i=0;
	int testRes=0;
	int sampleNum =5;

	sumx=sumy=sumz=0;
	// 1 init 
    err = l3g4200d_init_client(client, true);
	if(err)
	{
		GYRO_ERR("initialize client fail! err code %d!\n", err);
		return - 2;        
	}
	L3G4200D_dumpReg(client);
	// 2 check ZYXDA bit
	hwmsen_read_byte(client,L3G4200D_STATUS_REG,&data);
	GYRO_LOG("L3G4200D_STATUS_REG=%d\n",data );
	while(0x04 != (data&0x04))
	{
	  msleep(10);
	}
	msleep(1000); //wait for stable
	// 3 read raw data no self test data
	for(i=0; i<sampleNum; i++)
	{
	  L3G4200D_ReadGyroData(client, strbuf, L3G4200D_BUFSIZE);
	  sscanf(strbuf, "%x %x %x", &nost_x, &nost_y, &nost_z);
	  GYRO_LOG("NOst %d %d %d!\n", nost_x,nost_y,nost_z);
	  sumx += nost_x;
	  sumy += nost_y;
	  sumz += nost_z;
	  msleep(10);
	}
	//calculate avg x y z
	avgx_NOST = sumx/sampleNum;
	avgy_NOST = sumy/sampleNum;
	avgz_NOST = sumz/sampleNum;
	GYRO_LOG("avg NOST %d %d %d!\n", avgx_NOST,avgy_NOST,avgz_NOST);

	// 4 enalbe selftest
	hwmsen_read_byte(client,L3G4200D_CTL_REG4,&data);
	data = data | 0x02;
	hwmsen_write_byte(client,L3G4200D_CTL_REG4,data);

	msleep(1000);//wait for stable

	L3G4200D_dumpReg(client);
	// 5 check  ZYXDA bit
	
	//6 read raw data   self test data
	sumx=0;
	sumy=0;
	sumz=0;
	for(i=0; i<sampleNum; i++)
	{
	  L3G4200D_ReadGyroData(client, strbuf, L3G4200D_BUFSIZE);
	  sscanf(strbuf, "%x %x %x", &st_x, &st_y, &st_z);
	  GYRO_LOG("st %d %d %d!\n", st_x,st_y,st_z);
	  
	  sumx += st_x;
	  sumy += st_y;
	  sumz += st_z;
	
	  msleep(10);
	}
	// 7 calc calculate avg x y z ST
	avgx_ST = sumx/sampleNum;
	avgy_ST = sumy/sampleNum;
	avgz_ST = sumz/sampleNum;
	//GYRO_LOG("avg ST %d %d %d!\n", avgx_ST,avgy_ST,avgz_ST);
	//GYRO_LOG("abs(avgx_ST-avgx_NOST): %ld \n", abs(avgx_ST-avgx_NOST));
	//GYRO_LOG("abs(avgy_ST-avgy_NOST): %ld \n", abs(avgy_ST-avgy_NOST));
	//GYRO_LOG("abs(avgz_ST-avgz_NOST): %ld \n", abs(avgz_ST-avgz_NOST));

	if((abs(avgx_ST-avgx_NOST)>=175*131) && (abs(avgx_ST-avgx_NOST)<=875*131))
	{
	  resx =0; //x axis pass
	  GYRO_LOG(" x axis pass\n" );
	}
	if((abs(avgy_ST-avgy_NOST)>=175*131) && (abs(avgy_ST-avgy_NOST)<=875*131))
	{
	  resy =0; //y axis pass
	  GYRO_LOG(" y axis pass\n" );
	}
	if((abs(avgz_ST-avgz_NOST)>=175*131) && (abs(avgz_ST-avgz_NOST)<=875*131))
	{
	  resz =0; //z axis pass
	  GYRO_LOG(" z axis pass\n" );
	}

	if(0==resx && 0==resy && 0==resz)
	{
	  testRes = 0;
	}
	else
	{
	  testRes = -1;
	}

    hwmsen_write_byte(client,L3G4200D_CTL_REG4,0x00);
	err = l3g4200d_init_client(client, false);
	if(err)
	{
		GYRO_ERR("initialize client fail! err code %d!\n", err);
		return -2;        
	}
    GYRO_LOG("testRes %d!\n", testRes);
	return testRes;

}


//self test for factory 
static int L3G4200D_SMTReadSensorData(struct i2c_client *client)
{
	//S16 gyro[L3G4200D_AXES_NUM*L3G4200D_FIFOSIZE];
	int res = 0;

	GYRO_FUN();
	res = L3G4200D_SelfTest(client);

	GYRO_LOG(" L3G4200D_SMTReadSensorData %d", res ); 
	
	return res;
}

/*----------------------------------------------------------------------------*/
static int L3G4200D_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
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

	sprintf(buf, "L3G4200D Chip");
	return 0;
}


/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = l3g4200d_i2c_client;
	char strbuf[L3G4200D_BUFSIZE];
	if(NULL == client)
	{
		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}
	
	L3G4200D_ReadChipInfo(client, strbuf, L3G4200D_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);        
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = l3g4200d_i2c_client;
	char strbuf[L3G4200D_BUFSIZE];
	
	if(NULL == client)
	{
		GYRO_ERR("i2c client is null!!\n");
		return 0;
	}
	
	L3G4200D_ReadGyroData(client, strbuf, L3G4200D_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);;            
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct l3g4200d_i2c_data *obj = obj_i2c_data;
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
	struct l3g4200d_i2c_data *obj = obj_i2c_data;
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
	struct l3g4200d_i2c_data *obj = obj_i2c_data;
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
static DRIVER_ATTR(trace,      S_IWUGO | S_IRUGO, show_trace_value,         store_trace_value);
static DRIVER_ATTR(status,               S_IRUGO, show_status_value,        NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *L3G4200D_attr_list[] = {
	&driver_attr_chipinfo,     /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/	
	&driver_attr_trace,        /*trace log*/
	&driver_attr_status,        
};
/*----------------------------------------------------------------------------*/
static int l3g4200d_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(L3G4200D_attr_list)/sizeof(L3G4200D_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if(0 != (err = driver_create_file(driver, L3G4200D_attr_list[idx])))
		{            
			GYRO_ERR("driver_create_file (%s) = %d\n", L3G4200D_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}
/*----------------------------------------------------------------------------*/
static int l3g4200d_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(L3G4200D_attr_list)/sizeof(L3G4200D_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, L3G4200D_attr_list[idx]);
	}
	

	return err;
}

/*----------------------------------------------------------------------------*/
static int l3g4200d_init_client(struct i2c_client *client, bool enable)
{
	struct l3g4200d_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;
	GYRO_FUN();	
    	GYRO_LOG(" fwq l3g4200d addr %x!\n",client->addr);
	res = L3G4200D_CheckDeviceID(client);
	if(res != L3G4200D_SUCCESS)
	{
		return res;
	}

	res = L3G4200D_SetPowerMode(client, enable);
	if(res != L3G4200D_SUCCESS)
	{
		return res;
	}
	
	// The range should at least be 17.45 rad/s (ie: ~1000 deg/s).
	res = L3G4200D_SetDataResolution(client,L3G4200D_RANGE_2000);//we have only this choice
	if(res != L3G4200D_SUCCESS) 
	{
		return res;
	}

	// 
	res = L3G4200D_SetSampleRate(client, L3G4200D_100HZ);
	if(res != L3G4200D_SUCCESS ) 
	{
		return res;
	}

	GYRO_LOG("l3g4200d_init_client OK!\n");

#ifdef CONFIG_L3G4200D_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));  
#endif

	return L3G4200D_SUCCESS;
}

/*----------------------------------------------------------------------------*/
int l3g4200d_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;	
	struct l3g4200d_i2c_data *priv = (struct l3g4200d_i2c_data*)self;
	hwm_sensor_data* gyro_data;
	char buff[L3G4200D_BUFSIZE];	

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
					err = L3G4200D_SetPowerMode(priv->client, !sensor_power);
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
				L3G4200D_ReadGyroData(priv->client, buff, L3G4200D_BUFSIZE);
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
static int l3g4200d_open(struct inode *inode, struct file *file)
{
	file->private_data = l3g4200d_i2c_client;

	if(file->private_data == NULL)
	{
		GYRO_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int l3g4200d_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
//static int l3g4200d_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
//       unsigned long arg)
static long l3g4200d_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client*)file->private_data;
	//struct l3g4200d_i2c_data *obj = (struct l3g4200d_i2c_data*)i2c_get_clientdata(client);	
	char strbuf[L3G4200D_BUFSIZE] = {0};
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
			l3g4200d_init_client(client, false);			
			break;

		case GYROSCOPE_IOCTL_SMT_DATA:
			data = (void __user *) arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}

			smtRes = L3G4200D_SMTReadSensorData(client);
			GYRO_LOG("IOCTL smtRes: %d!\n", smtRes);
			copy_cnt = copy_to_user(data, &smtRes,  sizeof(smtRes));
			
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
			
			L3G4200D_ReadGyroData(client, strbuf, L3G4200D_BUFSIZE);
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
				cali[L3G4200D_AXIS_X] = sensor_data.x * L3G4200D_FS_2000_LSB / L3G4200D_OUT_MAGNIFY;
				cali[L3G4200D_AXIS_Y] = sensor_data.y * L3G4200D_FS_2000_LSB / L3G4200D_OUT_MAGNIFY;
				cali[L3G4200D_AXIS_Z] = sensor_data.z * L3G4200D_FS_2000_LSB / L3G4200D_OUT_MAGNIFY;			  
				err = L3G4200D_WriteCalibration(client, cali);
			}
			break;

		case GYROSCOPE_IOCTL_CLR_CALI:
			err = L3G4200D_ResetCalibration(client);
			break;

		case GYROSCOPE_IOCTL_GET_CALI:
			data = (void __user*)arg;
			if(data == NULL)
			{
				err = -EINVAL;
				break;	  
			}
			err = L3G4200D_ReadCalibration(client, cali);
			if(err)
			{
				break;
			}
			
			sensor_data.x = cali[L3G4200D_AXIS_X] * L3G4200D_OUT_MAGNIFY / L3G4200D_FS_2000_LSB;
			sensor_data.y = cali[L3G4200D_AXIS_Y] * L3G4200D_OUT_MAGNIFY / L3G4200D_FS_2000_LSB;
			sensor_data.z = cali[L3G4200D_AXIS_Z] * L3G4200D_OUT_MAGNIFY / L3G4200D_FS_2000_LSB;
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
static struct file_operations l3g4200d_fops = {
//	.owner = THIS_MODULE,
	.open = l3g4200d_open,
	.release = l3g4200d_release,
	.unlocked_ioctl = l3g4200d_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice l3g4200d_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gyroscope",
	.fops = &l3g4200d_fops,
};
/*----------------------------------------------------------------------------*/
#ifndef CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int l3g4200d_suspend(struct i2c_client *client, pm_message_t msg) 
{
	struct l3g4200d_i2c_data *obj = i2c_get_clientdata(client);    
	GYRO_FUN();    

	if(msg.event == PM_EVENT_SUSPEND)
	{   
		if(obj == NULL)
		{
			GYRO_ERR("null pointer!!\n");
			return -EINVAL;
		}
		atomic_set(&obj->suspend, 1);		
		
		err = L3G4200D_SetPowerMode(client, false);
		if(err <= 0)
		{
			return err;
		}
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int l3g4200d_resume(struct i2c_client *client)
{
	struct l3g4200d_i2c_data *obj = i2c_get_clientdata(client);        
	int err;
	GYRO_FUN();

	if(obj == NULL)
	{
		GYRO_ERR("null pointer!!\n");
		return -EINVAL;
	}

	L3G4200D_power(obj->hw, 1);
	err = l3g4200d_init_client(client, false);
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
static void l3g4200d_early_suspend(struct early_suspend *h) 
{
	struct l3g4200d_i2c_data *obj = container_of(h, struct l3g4200d_i2c_data, early_drv);   
	int err;
	GYRO_FUN();    

	if(obj == NULL)
	{
		GYRO_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&obj->suspend, 1);
	err = L3G4200D_SetPowerMode(obj->client, false);
	if(err)
	{
		GYRO_ERR("write power control fail!!\n");
		return;
	}
	if(err <= 0)
	{
		return;
	}

	sensor_power = false;
	
	L3G4200D_power(obj->hw, 0);
}
/*----------------------------------------------------------------------------*/
static void l3g4200d_late_resume(struct early_suspend *h)
{
	struct l3g4200d_i2c_data *obj = container_of(h, struct l3g4200d_i2c_data, early_drv);         
	int err;
	GYRO_FUN();

	if(obj == NULL)
	{
		GYRO_ERR("null pointer!!\n");
		return;
	}

	L3G4200D_power(obj->hw, 1);
	err = l3g4200d_init_client(obj->client, false);
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
static int l3g4200d_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{    
	strcpy(info->type, L3G4200D_DEV_NAME);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int l3g4200d_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct l3g4200d_i2c_data *obj;
	struct hwmsen_object sobj;
	int err = 0;
	GYRO_FUN();

	if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	
	memset(obj, 0, sizeof(struct l3g4200d_i2c_data));

	obj->hw = get_cust_gyro_hw();
	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if(err)
	{
		GYRO_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit;
	}

	obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client,obj);
	
	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	


	l3g4200d_i2c_client = new_client;	
	err = l3g4200d_init_client(new_client, false);
	if(err)
	{
		goto exit_init_failed;
	}
	

	err = misc_register(&l3g4200d_device);
	if(err)
	{
		GYRO_ERR("l3g4200d_device misc register failed!\n");
		goto exit_misc_device_register_failed;
	}

	err = l3g4200d_create_attr(&l3g4200d_gyro_driver.driver);
	if(err)
	{
		GYRO_ERR("l3g4200d create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}
	

	sobj.self = obj;
    sobj.polling = 1;
    sobj.sensor_operate = l3g4200d_operate;
	err = hwmsen_attach(ID_GYROSCOPE, &sobj);
	if(err)
	{
		GYRO_ERR("hwmsen_attach fail = %d\n", err);
		goto exit_kfree;
	}
	

#ifdef CONFIG_HAS_EARLYSUSPEND
	obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
	obj->early_drv.suspend  = l3g4200d_early_suspend,
	obj->early_drv.resume   = l3g4200d_late_resume,    
	register_early_suspend(&obj->early_drv);
#endif 

	GYRO_LOG("%s: OK\n", __func__);    
	return 0;

	exit_create_attr_failed:
	misc_deregister(&l3g4200d_device);
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
static int l3g4200d_i2c_remove(struct i2c_client *client)
{
	int err = 0;	
	
	err = l3g4200d_delete_attr(&l3g4200d_gyro_driver.driver);
	if(err)
	{
		GYRO_ERR("l3g4200d_delete_attr fail: %d\n", err);
	}
	
	err = misc_deregister(&l3g4200d_device);
	if(err)
	{
		GYRO_ERR("misc_deregister fail: %d\n", err);
	}

	err = hwmsen_detach(ID_ACCELEROMETER);
	if(err)
	{
		GYRO_ERR("hwmsen_detach fail: %d\n", err);
	}

	l3g4200d_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int l3g4200d_probe(struct platform_device *pdev) 
{
	struct gyro_hw *hw = get_cust_gyro_hw();
	GYRO_FUN();

	L3G4200D_power(hw, 1);
//	l3g4200d_force[0] = hw->i2c_num;
	if(i2c_add_driver(&l3g4200d_i2c_driver))
	{
		GYRO_ERR("add driver error\n");
		return -1;
	}
	return 0;
}
/*----------------------------------------------------------------------------*/
static int l3g4200d_remove(struct platform_device *pdev)
{
    struct gyro_hw *hw = get_cust_gyro_hw();

    GYRO_FUN();    
    L3G4200D_power(hw, 0);    
    i2c_del_driver(&l3g4200d_i2c_driver);
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

static struct platform_driver l3g4200d_gyro_driver = {
	.probe      = l3g4200d_probe,
	.remove     = l3g4200d_remove,    
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

static struct platform_driver l3g4200d_gyro_driver = {
	.probe      = l3g4200d_probe,
	.remove     = l3g4200d_remove,    
	.driver     = {
		.name  = "gyroscope",
//		.owner = THIS_MODULE,
	}
};
#endif
/*----------------------------------------------------------------------------*/
static int __init l3g4200d_init(void)
{
	struct gyro_hw *hw = get_cust_gyro_hw();
	GYRO_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	i2c_register_board_info(hw->i2c_num, &i2c_l3g4200d, 1);
	if(platform_driver_register(&l3g4200d_gyro_driver))
	{
		GYRO_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit l3g4200d_exit(void)
{
	GYRO_FUN();
	platform_driver_unregister(&l3g4200d_gyro_driver);
}
/*----------------------------------------------------------------------------*/
module_init(l3g4200d_init);
module_exit(l3g4200d_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("L3G4200D gyroscope driver");
MODULE_AUTHOR("Chunlei.Wang@mediatek.com");
