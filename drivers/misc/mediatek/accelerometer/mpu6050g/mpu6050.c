/* MPU6050 motion sensor driver
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

#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include "mpu6050.h"
#include "mpu6xxx_hwselftest.h"
#include <linux/hwmsen_helper.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#define POWER_NONE_MACRO MT65XX_POWER_NONE

/*----------------------------------------------------------------------------*/
#define DEBUG 1
/*----------------------------------------------------------------------------*/
#define CONFIG_MPU6050_LOWPASS   /*apply low pass filter on output*/       
#define SW_CALIBRATION
/*----------------------------------------------------------------------------*/
#define MPU6050_AXIS_X          0
#define MPU6050_AXIS_Y          1
#define MPU6050_AXIS_Z          2
#define MPU6050_AXES_NUM        3
#define MPU6050_DATA_LEN        6
#define MPU6050_DEV_NAME        "MPU6050G"   /* name must different with gyro mpu6050 */
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id mpu6050_i2c_id[] = {{MPU6050_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_mpu6050={ I2C_BOARD_INFO(MPU6050_DEV_NAME, (MPU6050_I2C_SLAVE_ADDR>>1))};

/*----------------------------------------------------------------------------*/
static int mpu6050_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int mpu6050_i2c_remove(struct i2c_client *client);
static int mpu6050_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
#ifndef CONFIG_HAS_EARLYSUSPEND
static int mpu6050_suspend(struct i2c_client *client, pm_message_t msg) ;
static int mpu6050_resume(struct i2c_client *client);
#endif
/*----------------------------------------------------------------------------*/
typedef enum
{
    MPU6050_TRC_FILTER  = 0x01,
    MPU6050_TRC_RAWDATA = 0x02,
    MPU6050_TRC_IOCTL   = 0x04,
    MPU6050_TRC_CALI    = 0X08,
    MPU6050_TRC_INFO    = 0X10,
} MPU6050_TRC;
/*----------------------------------------------------------------------------*/
struct scale_factor
{
    u8  whole;
    u8  fraction;
};
/*----------------------------------------------------------------------------*/
struct data_resolution
{
    struct scale_factor scalefactor;
    int                 sensitivity;
};
/*----------------------------------------------------------------------------*/
#define C_MAX_FIR_LENGTH (32)
/*----------------------------------------------------------------------------*/
struct data_filter
{
    s16 raw[C_MAX_FIR_LENGTH][MPU6050_AXES_NUM];
    int sum[MPU6050_AXES_NUM];
    int num;
    int idx;
};
/*----------------------------------------------------------------------------*/
struct mpu6050_i2c_data
{
    struct i2c_client *client;
    struct acc_hw *hw;
    struct hwmsen_convert   cvt;

    /*misc*/
    struct data_resolution *reso;
    atomic_t                trace;
    atomic_t                suspend;
    atomic_t                selftest;
    atomic_t                filter;
    s16                     cali_sw[MPU6050_AXES_NUM+1];

    /*data*/
    s8                      offset[MPU6050_AXES_NUM+1];  /*+1: for 4-byte alignment*/
    s16                     data[MPU6050_AXES_NUM+1];

#if defined(CONFIG_MPU6050_LOWPASS)
    atomic_t                firlen;
    atomic_t                fir_en;
    struct data_filter      fir;
#endif 
    /*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_drv;
#endif     
    u8                      bandwidth;
#if MPU6XXX_HWSELFTEST
    struct inv_selftest_device mpu_selftest_device;
#endif
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver mpu6050_i2c_driver = {
    .driver = {
        .name           = MPU6050_DEV_NAME,
    },
    .probe              = mpu6050_i2c_probe,
    .remove             = mpu6050_i2c_remove,
    .detect             = mpu6050_i2c_detect,
#if !defined(CONFIG_HAS_EARLYSUSPEND)    
    .suspend            = mpu6050_suspend,
    .resume             = mpu6050_resume,
#endif
    .id_table = mpu6050_i2c_id,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *mpu6050_i2c_client = NULL;
static struct platform_driver mpu6050_gsensor_driver;
static struct mpu6050_i2c_data *obj_i2c_data = NULL;
static bool sensor_power = false;
static GSENSOR_VECTOR3D gsensor_gain;
static char selftestRes[8]= {0}; 


/*----------------------------------------------------------------------------*/
#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN(f)               printk(GSE_TAG"%s\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)    printk(GSE_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    printk(GSE_TAG fmt, ##args)
/*----------------------------------------------------------------------------*/
static struct data_resolution mpu6050_data_resolution[] = {
    /*8 combination by {FULL_RES,RANGE}*/
    {{ 0,  6}, 16384},  /*+/-2g  in 16-bit resolution:  0.06 mg/LSB*/
    {{ 0, 12}, 8192},   /*+/-4g  in 16-bit resolution:  0.12 mg/LSB*/
    {{ 0, 24}, 4096},   /*+/-8g  in 16-bit resolution:  0.24 mg/LSB*/
    {{ 0, 5}, 2048},   	/*+/-16g in 16-bit resolution:  0.49 mg/LSB*/
};
/*----------------------------------------------------------------------------*/
static struct data_resolution mpu6050_offset_resolution = {{ 0, 5}, 2048};

static unsigned int power_on = 0;

extern int MPU6050_gyro_power(void);
extern int MPU6050_gyro_mode(void);


int MPU6050_gse_power( void)
{
    return(power_on);
}
EXPORT_SYMBOL(MPU6050_gse_power);

int MPU6050_gse_mode(void)
{
    return sensor_power;
}
EXPORT_SYMBOL(MPU6050_gse_mode);


int MPU6050_i2c_master_send(u8 *buf, u8 len)
{
    int res = 0;
    if (NULL == mpu6050_i2c_client)
    {
        GSE_ERR("MPU6050_i2c_master_send null ptr!!\n");
    }
    else
    {
        res = i2c_master_send(mpu6050_i2c_client, buf, len);
    }

    return res;
}
EXPORT_SYMBOL(MPU6050_i2c_master_send);

int MPU6050_i2c_master_recv(u8 *buf, u8 len)
{
    int res = 0;
    if (NULL == mpu6050_i2c_client)
    {
        GSE_ERR("MPU6050_i2c_master_recv null ptr!!\n");
    }
    else
    {
        res = i2c_master_recv(mpu6050_i2c_client, buf, len);
    }

    return res;
}
EXPORT_SYMBOL(MPU6050_i2c_master_recv);
/*----------------------------------------------------------------------------*/
static int mpu_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len){
        u8 beg = addr;
	struct i2c_msg msgs[2] = {
		{
			.addr = client->addr,	.flags = 0,
			.len = 1,	.buf = &beg
		},
		{
			.addr = client->addr,	.flags = I2C_M_RD,
			.len = len,	.buf = data,
		}
	};
	int err;

	if (!client)
		return -EINVAL;
	else if (len > C_I2C_FIFO_SIZE) {
		GSE_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	err = i2c_transfer(client->adapter, msgs, sizeof(msgs)/sizeof(msgs[0]));
	if (err != 2) {
		GSE_ERR("i2c_transfer error: (%d %p %d) %d\n",
			addr, data, len, err);
		err = -EIO;
	} else {
		err = 0;
	}
	return err;

}
int MPU6050_hwmsen_read_block(u8 addr, u8 *buf, u8 len)
{
    if (NULL == mpu6050_i2c_client)
    {
        GSE_ERR("MPU6050_hwmsen_read_block null ptr!!\n");
        return MPU6050_ERR_I2C;
    }
    return mpu_i2c_read_block(mpu6050_i2c_client, addr, buf, len);
}
EXPORT_SYMBOL(MPU6050_hwmsen_read_block);


int MPU6050_hwmsen_read_byte(u8 addr, u8 *buf)
{
    if (NULL == mpu6050_i2c_client)
    {
        GSE_ERR("MPU6050_hwmsen_read_byte null ptr!!\n");
        return MPU6050_ERR_I2C;
    }
    return mpu_i2c_read_block(mpu6050_i2c_client, addr, buf, 1);
}
EXPORT_SYMBOL(MPU6050_hwmsen_read_byte);
/*--------------------mpu6050 power control function----------------------------------*/
static void MPU6050_power(struct acc_hw *hw, unsigned int on) 
{


    if (hw->power_id != POWER_NONE_MACRO)        // have externel LDO
    {
        GSE_LOG("power %s\n", on ? "on" : "off");
        if (power_on == on)  // power status not change
        {
            GSE_LOG("ignore power control: %d\n", on);
        }
        else if (on) // power on
        {
            if (!hwPowerOn(hw->power_id, hw->power_vol, "MPU6050G"))
            {
                GSE_ERR("power on fails!!\n");
            }
        }
        else    // power off
        {
            if (MPU6050_gyro_power() == false)
            {
                if (!hwPowerDown(hw->power_id, "MPU6050G"))
                {
                    GSE_ERR("power off fail!!\n");
                }
            }
        }
    }
    power_on = on;    
}

/*----------------------------------------------------------------------------*/
static int MPU6050_SetPowerMode(struct i2c_client *client, bool enable)
{
    u8 databuf[2];    
    int res = 0;
    //u8 addr = MPU6050_REG_POWER_CTL;
    struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);


    if (enable == sensor_power)
    {
        GSE_LOG("Sensor power status is newest!\n");
        return MPU6050_SUCCESS;
    }

    databuf[0] = MPU6050_REG_POWER_CTL;   
    res = i2c_master_send(client, databuf, 0x1);
    if (res <= 0)
    {
        return MPU6050_ERR_I2C;
    }

    udelay(500);

    databuf[0] = 0x0;        
    /*
    res = i2c_master_recv(client, databuf, 1);
    if (res <= 0)
    {
        return MPU6050_ERR_I2C;
    }
    */

    databuf[0] &= ~MPU6050_SLEEP;

    if (enable == FALSE)
    {
        if (MPU6050_gyro_mode() == false)
        {
            databuf[0] |= MPU6050_SLEEP;
        }
    }
    else
    {
        // do nothing
    }
    databuf[1] = databuf[0];
    databuf[0] = MPU6050_REG_POWER_CTL;

    res = i2c_master_send(client, databuf, 0x2);

    if (res <= 0)
    {
        GSE_LOG("set power mode failed!\n");
        return MPU6050_ERR_I2C;
    }
    else if (atomic_read(&obj->trace) & MPU6050_TRC_INFO)
    {
        GSE_LOG("set power mode ok %d!\n", databuf[1]);
    }

    if (enable == true)
    {
        msleep(50);
    }

    sensor_power = enable;
    return MPU6050_SUCCESS;    
}
/*----------------------------------------------------------------------------*/
static int MPU6050_SetDataResolution(struct mpu6050_i2c_data *obj)
{
    int err;
    u8  dat, reso;

    if ((err = mpu_i2c_read_block(obj->client, MPU6050_REG_DATA_FORMAT, &dat, 1)))
    {
        GSE_ERR("write data format fail!!\n");
        return err;
    }

    /*the data_reso is combined by 3 bits: {FULL_RES, DATA_RANGE}*/
    reso = 0x00;
    reso = (dat & MPU6050_RANGE_16G) >> 3; 

    if (reso < sizeof(mpu6050_data_resolution)/sizeof(mpu6050_data_resolution[0]))
    {
        obj->reso = &mpu6050_data_resolution[reso];
        return 0;
    }
    else
    {
        return -EINVAL;
    }
}
/*----------------------------------------------------------------------------*/
static int MPU6050_ReadData(struct i2c_client *client, s16 data[MPU6050_AXES_NUM])
{
    struct mpu6050_i2c_data *priv = i2c_get_clientdata(client);        
    u8 buf[MPU6050_DATA_LEN] = {0};
    int err = 0;


    if (NULL == client)
    {
        return -EINVAL;
    }

    {
        /* write then burst read */
        mpu_i2c_read_block(client, MPU6050_REG_DATAX0, buf, MPU6050_DATA_LEN);

        data[MPU6050_AXIS_X] = (s16)((buf[MPU6050_AXIS_X*2] << 8) |
                                     (buf[MPU6050_AXIS_X*2+1] ));
        data[MPU6050_AXIS_Y] = (s16)((buf[MPU6050_AXIS_Y*2] << 8) |
                                     (buf[MPU6050_AXIS_Y*2+1] ));
        data[MPU6050_AXIS_Z] = (s16)((buf[MPU6050_AXIS_Z*2] << 8) |
                                     (buf[MPU6050_AXIS_Z*2+1] ));

        if (atomic_read(&priv->trace) & MPU6050_TRC_RAWDATA)
        {
            GSE_LOG("[%08X %08X %08X] => [%5d %5d %5d]\n", data[MPU6050_AXIS_X], data[MPU6050_AXIS_Y], data[MPU6050_AXIS_Z],
                    data[MPU6050_AXIS_X], data[MPU6050_AXIS_Y], data[MPU6050_AXIS_Z]);
        }
#ifdef CONFIG_MPU6050_LOWPASS
        if (atomic_read(&priv->filter))
        {
            if (atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend))
            {
                int idx, firlen = atomic_read(&priv->firlen);   
                if (priv->fir.num < firlen)
                {
                    priv->fir.raw[priv->fir.num][MPU6050_AXIS_X] = data[MPU6050_AXIS_X];
                    priv->fir.raw[priv->fir.num][MPU6050_AXIS_Y] = data[MPU6050_AXIS_Y];
                    priv->fir.raw[priv->fir.num][MPU6050_AXIS_Z] = data[MPU6050_AXIS_Z];
                    priv->fir.sum[MPU6050_AXIS_X] += data[MPU6050_AXIS_X];
                    priv->fir.sum[MPU6050_AXIS_Y] += data[MPU6050_AXIS_Y];
                    priv->fir.sum[MPU6050_AXIS_Z] += data[MPU6050_AXIS_Z];
                    if (atomic_read(&priv->trace) & MPU6050_TRC_FILTER)
                    {
                        GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d]\n", priv->fir.num,
                                priv->fir.raw[priv->fir.num][MPU6050_AXIS_X], priv->fir.raw[priv->fir.num][MPU6050_AXIS_Y], priv->fir.raw[priv->fir.num][MPU6050_AXIS_Z],
                                priv->fir.sum[MPU6050_AXIS_X], priv->fir.sum[MPU6050_AXIS_Y], priv->fir.sum[MPU6050_AXIS_Z]);
                    }
                    priv->fir.num++;
                    priv->fir.idx++;
                }
                else
                {
                    idx = priv->fir.idx % firlen;
                    priv->fir.sum[MPU6050_AXIS_X] -= priv->fir.raw[idx][MPU6050_AXIS_X];
                    priv->fir.sum[MPU6050_AXIS_Y] -= priv->fir.raw[idx][MPU6050_AXIS_Y];
                    priv->fir.sum[MPU6050_AXIS_Z] -= priv->fir.raw[idx][MPU6050_AXIS_Z];
                    priv->fir.raw[idx][MPU6050_AXIS_X] = data[MPU6050_AXIS_X];
                    priv->fir.raw[idx][MPU6050_AXIS_Y] = data[MPU6050_AXIS_Y];
                    priv->fir.raw[idx][MPU6050_AXIS_Z] = data[MPU6050_AXIS_Z];
                    priv->fir.sum[MPU6050_AXIS_X] += data[MPU6050_AXIS_X];
                    priv->fir.sum[MPU6050_AXIS_Y] += data[MPU6050_AXIS_Y];
                    priv->fir.sum[MPU6050_AXIS_Z] += data[MPU6050_AXIS_Z];
                    priv->fir.idx++;
                    data[MPU6050_AXIS_X] = priv->fir.sum[MPU6050_AXIS_X]/firlen;
                    data[MPU6050_AXIS_Y] = priv->fir.sum[MPU6050_AXIS_Y]/firlen;
                    data[MPU6050_AXIS_Z] = priv->fir.sum[MPU6050_AXIS_Z]/firlen;
                    if (atomic_read(&priv->trace) & MPU6050_TRC_FILTER)
                    {
                        GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d] : [%5d %5d %5d]\n", idx,
                                priv->fir.raw[idx][MPU6050_AXIS_X], priv->fir.raw[idx][MPU6050_AXIS_Y], priv->fir.raw[idx][MPU6050_AXIS_Z],
                                priv->fir.sum[MPU6050_AXIS_X], priv->fir.sum[MPU6050_AXIS_Y], priv->fir.sum[MPU6050_AXIS_Z],
                                data[MPU6050_AXIS_X], data[MPU6050_AXIS_Y], data[MPU6050_AXIS_Z]);
                    }
                }
            }
        }
#endif         
    }
    return err;
}
/*----------------------------------------------------------------------------*/
static int MPU6050_ReadOffset(struct i2c_client *client, s8 ofs[MPU6050_AXES_NUM])
{    
    int err = 0;
#ifdef SW_CALIBRATION
    ofs[0]=ofs[1]=ofs[2]=0x0;
#else
    if ((err = mpu_i2c_read_block(client, MPU6050_REG_OFSX, ofs, MPU6050_AXES_NUM)))
    {
        GSE_ERR("error: %d\n", err);
    }
#endif
    //GSE_LOG("offesx=%x, y=%x, z=%x",ofs[0],ofs[1],ofs[2]);

    return err;    
}
/*----------------------------------------------------------------------------*/
static int MPU6050_ResetCalibration(struct i2c_client *client)
{
    struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
#ifndef SW_CALIBRATION
    s8 ofs[MPU6050_AXES_NUM] = {0x00, 0x00, 0x00};
#endif
    int err = 0;
#ifdef SW_CALIBRATION
    /* do not thing */
#else

    if ((err = hwmsen_write_block(client, MPU6050_REG_OFSX, ofs, MPU6050_AXES_NUM)))
    {
        GSE_ERR("error: %d\n", err);
    }
#endif

    memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
    memset(obj->offset, 0x00, sizeof(obj->offset));

    return err;    
}
/*----------------------------------------------------------------------------*/
static int MPU6050_ReadCalibration(struct i2c_client *client, int dat[MPU6050_AXES_NUM])
{
    struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
#ifdef SW_CALIBRATION
    int mul;
#else
    int err;
#endif
#ifdef SW_CALIBRATION
    mul = 0;//only SW Calibration, disable HW Calibration
#else

    if ((err = MPU6050_ReadOffset(client, obj->offset)))
    {
        GSE_ERR("read offset fail, %d\n", err);
        return err;
    }
    mul = obj->reso->sensitivity/mpu6050_offset_resolution.sensitivity;
#endif

    dat[obj->cvt.map[MPU6050_AXIS_X]] = obj->cvt.sign[MPU6050_AXIS_X]*(obj->offset[MPU6050_AXIS_X]*mul*GRAVITY_EARTH_1000/(obj->reso->sensitivity) + obj->cali_sw[MPU6050_AXIS_X]);
    dat[obj->cvt.map[MPU6050_AXIS_Y]] = obj->cvt.sign[MPU6050_AXIS_Y]*(obj->offset[MPU6050_AXIS_Y]*mul*GRAVITY_EARTH_1000/(obj->reso->sensitivity) + obj->cali_sw[MPU6050_AXIS_Y]);
    dat[obj->cvt.map[MPU6050_AXIS_Z]] = obj->cvt.sign[MPU6050_AXIS_Z]*(obj->offset[MPU6050_AXIS_Z]*mul*GRAVITY_EARTH_1000/(obj->reso->sensitivity) + obj->cali_sw[MPU6050_AXIS_Z]);                        

    return 0;
}
/*----------------------------------------------------------------------------*/
static int MPU6050_ReadCalibrationEx(struct i2c_client *client, int act[MPU6050_AXES_NUM], int raw[MPU6050_AXES_NUM])
{  
    /*raw: the raw calibration data; act: the actual calibration data*/
    struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
#ifdef SW_CALIBRATION
    int mul;
#else
    int err;
#endif
#ifdef SW_CALIBRATION
    mul = 0;//only SW Calibration, disable HW Calibration
#else

    if ((err = MPU6050_ReadOffset(client, obj->offset)))
    {
        GSE_ERR("read offset fail, %d\n", err);
        return err;
    }
    mul = obj->reso->sensitivity/mpu6050_offset_resolution.sensitivity;
#endif

    raw[MPU6050_AXIS_X] = obj->offset[MPU6050_AXIS_X]*mul*GRAVITY_EARTH_1000/(obj->reso->sensitivity) + obj->cali_sw[MPU6050_AXIS_X];
    raw[MPU6050_AXIS_Y] = obj->offset[MPU6050_AXIS_Y]*mul*GRAVITY_EARTH_1000/(obj->reso->sensitivity) + obj->cali_sw[MPU6050_AXIS_Y];
    raw[MPU6050_AXIS_Z] = obj->offset[MPU6050_AXIS_Z]*mul*GRAVITY_EARTH_1000/(obj->reso->sensitivity) + obj->cali_sw[MPU6050_AXIS_Z];

    act[obj->cvt.map[MPU6050_AXIS_X]] = obj->cvt.sign[MPU6050_AXIS_X]*raw[MPU6050_AXIS_X];
    act[obj->cvt.map[MPU6050_AXIS_Y]] = obj->cvt.sign[MPU6050_AXIS_Y]*raw[MPU6050_AXIS_Y];
    act[obj->cvt.map[MPU6050_AXIS_Z]] = obj->cvt.sign[MPU6050_AXIS_Z]*raw[MPU6050_AXIS_Z];                        

    return 0;
}
/*----------------------------------------------------------------------------*/
static int MPU6050_WriteCalibration(struct i2c_client *client, int dat[MPU6050_AXES_NUM])
{
    struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
    int err;
    int cali[MPU6050_AXES_NUM], raw[MPU6050_AXES_NUM];
#ifndef SW_CALIBRATION
    int lsb = mpu6050_offset_resolution.sensitivity;
    int divisor = obj->reso->sensitivity/lsb;
#endif
    if ((err = MPU6050_ReadCalibrationEx(client, cali, raw)))    /*offset will be updated in obj->offset*/
    {
        GSE_ERR("read offset fail, %d\n", err);
        return err;
    }

    GSE_LOG("OLDOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n", 
            raw[MPU6050_AXIS_X], raw[MPU6050_AXIS_Y], raw[MPU6050_AXIS_Z],
            obj->offset[MPU6050_AXIS_X], obj->offset[MPU6050_AXIS_Y], obj->offset[MPU6050_AXIS_Z],
            obj->cali_sw[MPU6050_AXIS_X], obj->cali_sw[MPU6050_AXIS_Y], obj->cali_sw[MPU6050_AXIS_Z]);

    /*calculate the real offset expected by caller*/
	#if 0
	cali[MPU6050_AXIS_X] = cali[MPU6050_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
	cali[MPU6050_AXIS_Y] = cali[MPU6050_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
	cali[MPU6050_AXIS_Z] = cali[MPU6050_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;	

	#endif
    cali[MPU6050_AXIS_X] += dat[MPU6050_AXIS_X];
    cali[MPU6050_AXIS_Y] += dat[MPU6050_AXIS_Y];
    cali[MPU6050_AXIS_Z] += dat[MPU6050_AXIS_Z];

    GSE_LOG("UPDATE: (%+3d %+3d %+3d)\n", 
            dat[MPU6050_AXIS_X], dat[MPU6050_AXIS_Y], dat[MPU6050_AXIS_Z]);
#ifdef SW_CALIBRATION
    obj->cali_sw[MPU6050_AXIS_X] = obj->cvt.sign[MPU6050_AXIS_X]*(cali[obj->cvt.map[MPU6050_AXIS_X]]);
    obj->cali_sw[MPU6050_AXIS_Y] = obj->cvt.sign[MPU6050_AXIS_Y]*(cali[obj->cvt.map[MPU6050_AXIS_Y]]);
    obj->cali_sw[MPU6050_AXIS_Z] = obj->cvt.sign[MPU6050_AXIS_Z]*(cali[obj->cvt.map[MPU6050_AXIS_Z]]);  
#else

    obj->offset[MPU6050_AXIS_X] = (s8)(obj->cvt.sign[MPU6050_AXIS_X]*(cali[obj->cvt.map[MPU6050_AXIS_X]])*(obj->reso->sensitivity)/GRAVITY_EARTH_1000/(divisor));
    obj->offset[MPU6050_AXIS_Y] = (s8)(obj->cvt.sign[MPU6050_AXIS_Y]*(cali[obj->cvt.map[MPU6050_AXIS_Y]])*(obj->reso->sensitivity)/GRAVITY_EARTH_1000/(divisor));
    obj->offset[MPU6050_AXIS_Z] = (s8)(obj->cvt.sign[MPU6050_AXIS_Z]*(cali[obj->cvt.map[MPU6050_AXIS_Z]])*(obj->reso->sensitivity)/GRAVITY_EARTH_1000/(divisor));

    /*convert software calibration using standard calibration*/
    obj->cali_sw[MPU6050_AXIS_X] = obj->cvt.sign[MPU6050_AXIS_X]*(cali[obj->cvt.map[MPU6050_AXIS_X]])%(divisor);
    obj->cali_sw[MPU6050_AXIS_Y] = obj->cvt.sign[MPU6050_AXIS_Y]*(cali[obj->cvt.map[MPU6050_AXIS_Y]])%(divisor);
    obj->cali_sw[MPU6050_AXIS_Z] = obj->cvt.sign[MPU6050_AXIS_Z]*(cali[obj->cvt.map[MPU6050_AXIS_Z]])%(divisor);

    GSE_LOG("NEWOFF: (%+3d %+3d %+3d): (%+3d %+3d %+3d) / (%+3d %+3d %+3d)\n", 
            obj->offset[MPU6050_AXIS_X]*divisor + obj->cali_sw[MPU6050_AXIS_X], 
            obj->offset[MPU6050_AXIS_Y]*divisor + obj->cali_sw[MPU6050_AXIS_Y], 
            obj->offset[MPU6050_AXIS_Z]*divisor + obj->cali_sw[MPU6050_AXIS_Z], 
            obj->offset[MPU6050_AXIS_X], obj->offset[MPU6050_AXIS_Y], obj->offset[MPU6050_AXIS_Z],
            obj->cali_sw[MPU6050_AXIS_X], obj->cali_sw[MPU6050_AXIS_Y], obj->cali_sw[MPU6050_AXIS_Z]);

    if ((err = hwmsen_write_block(obj->client, MPU6050_REG_OFSX, obj->offset, MPU6050_AXES_NUM)))
    {
        GSE_ERR("write offset fail: %d\n", err);
        return err;
    }
#endif

    return err;
}
/*----------------------------------------------------------------------------*/
static int MPU6050_CheckDeviceID(struct i2c_client *client)
{
    u8 databuf[10];    
    int res = 0;

    memset(databuf, 0, sizeof(u8)*10);    
    databuf[0] = MPU6050_REG_DEVID;    

    res = i2c_master_send(client, databuf, 0x1);
    if (res <= 0)
    {
        goto exit_MPU6050_CheckDeviceID;
    }

    udelay(500);

    databuf[0] = 0x0;        
    res = i2c_master_recv(client, databuf, 0x01);
    if (res <= 0)
    {
        goto exit_MPU6050_CheckDeviceID;
    }

    GSE_LOG("MPU6050_CheckDeviceID 0x%x\n", databuf[0]);

    exit_MPU6050_CheckDeviceID:
    if (res <= 0)
    {
        return MPU6050_ERR_I2C;
    }
    return MPU6050_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int MPU6050_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
    struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
    u8 databuf[2];    
    int res = 0;

    memset(databuf, 0, sizeof(u8)*2);    
    databuf[0] = MPU6050_REG_DATA_FORMAT;   
    res = i2c_master_send(client, databuf, 0x1);
    if (res <= 0)
    {
        return MPU6050_ERR_I2C;
    }

    udelay(500);

    databuf[0] = 0x0;        
    res = i2c_master_recv(client, databuf, 0x01);
    if (res <= 0)
    {
        return MPU6050_ERR_I2C;
    }

    /* write */    
    databuf[1] = databuf[0] | dataformat;
    databuf[0] = MPU6050_REG_DATA_FORMAT;   
    res = i2c_master_send(client, databuf, 0x2);

    if (res <= 0)
    {
        return MPU6050_ERR_I2C;
    }
    return MPU6050_SetDataResolution(obj);    
}
/*----------------------------------------------------------------------------*/
static int MPU6050_SetBWRate(struct i2c_client *client, u8 bwrate)
{
    struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
    u8 databuf[10];    
    int res = 0;

    if( (obj->bandwidth != bwrate) || (atomic_read(&obj->suspend)) )
    {    
        memset(databuf, 0, sizeof(u8)*10);    
    
        /* read */
        databuf[0] = MPU6050_REG_BW_RATE;    
        res = i2c_master_send(client, databuf, 0x1);
        if (res <= 0)
        {
            return MPU6050_ERR_I2C;
        }
    
        udelay(500);
    
        databuf[0] = 0x0;        
        res = i2c_master_recv(client, databuf, 0x01);
        if (res <= 0)
        {
            return MPU6050_ERR_I2C;
        }
    
    
        /* write */
        databuf[1] = databuf[0] | bwrate;
        databuf[0] = MPU6050_REG_BW_RATE;    
    
        res = i2c_master_send(client, databuf, 0x2);
    
        if (res <= 0)
        {
            return MPU6050_ERR_I2C;
        }

    obj->bandwidth = bwrate;
    }

    return MPU6050_SUCCESS;    
}

/*----------------------------------------------------------------------------*/
static int MPU6050_Dev_Reset(struct i2c_client *client)
{
    u8 databuf[10];    
    int res = 0;

    memset(databuf, 0, sizeof(u8)*10);    

    /* read */
    databuf[0] = MPU6050_REG_POWER_CTL;    
    res = i2c_master_send(client, databuf, 0x1);
    if (res <= 0)
    {
        return MPU6050_ERR_I2C;
    }

    udelay(500);

    databuf[0] = 0x0;        
    res = i2c_master_recv(client, databuf, 0x01);
    if (res <= 0)
    {
        return MPU6050_ERR_I2C;
    }


    /* write */
    databuf[1] = databuf[0] | MPU6050_DEV_RESET;
    databuf[0] = MPU6050_REG_POWER_CTL;    

    res = i2c_master_send(client, databuf, 0x2);

    if (res <= 0)
    {
        return MPU6050_ERR_I2C;
    }

    do
    {
        databuf[0] = MPU6050_REG_POWER_CTL;    
        res = i2c_master_send(client, databuf, 0x1);
        
        udelay(500);
        
        databuf[0] = 0x0;        
        res = i2c_master_recv(client, databuf, 0x01);

        printk("[Gsensor] check reset bit");

    }while((databuf[0]&MPU6050_DEV_RESET) != 0);

    msleep(50);
    return MPU6050_SUCCESS;    
}


/*----------------------------------------------------------------------------*/
static int MPU6050_Reset(struct i2c_client *client)
{
    u8 databuf[10];    
    int res = 0;

    /* write */
    databuf[1] = 0x7; /* reset gyro, g-sensor, temperature */
    databuf[0] = MPU6050_REG_RESET;    

    res = i2c_master_send(client, databuf, 0x2);

    if (res <= 0)
    {
        return MPU6050_ERR_I2C;
    }

    msleep(20);
    return MPU6050_SUCCESS;    
}


/*----------------------------------------------------------------------------*/
static int MPU6050_SetIntEnable(struct i2c_client *client, u8 intenable)
{
    u8 databuf[2];    
    int res = 0;

    memset(databuf, 0, sizeof(u8)*2);    
    databuf[0] = MPU6050_REG_INT_ENABLE;    
    databuf[1] = intenable;

    res = i2c_master_send(client, databuf, 0x2);

    if (res <= 0)
    {
        return MPU6050_ERR_I2C;
    }

    return MPU6050_SUCCESS;    
}
/*----------------------------------------------------------------------------*/
static int mpu6050_gpio_config(void)
{
//because we donot use EINT to support low power
// config to GPIO input mode + PD 

//set to GPIO_GSE_1_EINT_PIN
	/*
	mt_set_gpio_mode(GPIO_GSE_1_EINT_PIN, GPIO_GSE_1_EINT_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_GSE_1_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_GSE_1_EINT_PIN, GPIO_PULL_ENABLE);
	mt_set_gpio_pull_select(GPIO_GSE_1_EINT_PIN, GPIO_PULL_DOWN);
	 */
//set to GPIO_GSE_2_EINT_PIN
	/*
	mt_set_gpio_mode(GPIO_GSE_2_EINT_PIN, GPIO_GSE_2_EINT_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_GSE_2_EINT_PIN, GPIO_DIR_IN);
	mt_set_gpio_pull_enable(GPIO_GSE_2_EINT_PIN, GPIO_PULL_ENABLE);
	 mt_set_gpio_pull_select(GPIO_GSE_2_EINT_PIN, GPIO_PULL_DOWN);
	 */
    return 0;
}

static int mpu6050_init_client(struct i2c_client *client, int reset_cali)
{
    struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
    int res = 0;
    bool sensor_power_org;

    mpu6050_gpio_config();

    sensor_power_org = sensor_power;
    res = MPU6050_SetPowerMode(client, true);
    if (res != MPU6050_SUCCESS)
    {
        GSE_ERR("set power error\n");
        return res;
    }
    res = MPU6050_CheckDeviceID(client); 
    if (res != MPU6050_SUCCESS)
    {
        GSE_ERR("Check ID error\n");
        return res;
    }

    res = MPU6050_SetBWRate(client, MPU6050_BW_184HZ);
    if (res != MPU6050_SUCCESS ) //0x2C->BW=100Hz
    {
        GSE_ERR("set power error\n");
        return res;
    }

    res = MPU6050_SetDataFormat(client, MPU6050_RANGE_16G);
    if (res != MPU6050_SUCCESS) //0x2C->BW=100Hz
    {
        GSE_ERR("set data format error\n");
        return res;
    }

    gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = obj->reso->sensitivity;

    res = MPU6050_SetIntEnable(client, 0x00);//disable INT
    if (res != MPU6050_SUCCESS)
    {
        GSE_ERR("mpu6050_SetIntEnable error\n");
        return res;
    }

    if (0 != reset_cali)
    {
        /*reset calibration only in power on*/
        res = MPU6050_ResetCalibration(client);
        if (res != MPU6050_SUCCESS)
        {
            return res;
        }
    }

    res = MPU6050_SetPowerMode(client, sensor_power_org);
    if (res != MPU6050_SUCCESS)
    {
        GSE_ERR("set power error\n");
        return res;
    }

#ifdef CONFIG_MPU6050_LOWPASS
    memset(&obj->fir, 0x00, sizeof(obj->fir));  
#endif

    return MPU6050_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int MPU6050_ReadAllReg(struct i2c_client *client, char *buf, int bufsize)
{   
    u8 total_len= 0x5C; //(0x75-0x19);

    u8 addr = 0x19;
    u8 buff[total_len+1];
    int err = 0;
    int i;


    if (sensor_power == FALSE)
    {
        err = MPU6050_SetPowerMode(client, true);
        if (err)
        {
            GSE_ERR("Power on mpu6050 error %d!\n", err);
        }
    }

    mpu_i2c_read_block(client, addr, buff, total_len);

    for ( i=0; i<=total_len; i++)
    {
        GSE_LOG("MPU6050 reg=0x%x, data=0x%x \n",(addr+i), buff[i]);
    }

    return 0;
}
/*----------------------------------------------------------------------------*/
static int MPU6050_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
{
    u8 databuf[10];    

    memset(databuf, 0, sizeof(u8)*10);

    if ((NULL == buf)||(bufsize<=30))
    {
        return -1;
    }

    if (NULL == client)
    {
        *buf = 0;
        return -2;
    }

    sprintf(buf, "MPU6050 Chip");
    return 0;
}
/*----------------------------------------------------------------------------*/
static int MPU6050_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
    struct mpu6050_i2c_data *obj = obj_i2c_data; //(struct mpu6050_i2c_data*)i2c_get_clientdata(client);
    int acc[MPU6050_AXES_NUM];
    int res = 0;
    client = obj->client;

    if (atomic_read(&obj->suspend))
    {
        return -3;
    }

    if (NULL == buf)
    {
        return -1;
    }
    if (NULL == client)
    {
        *buf = 0;
        return -2;
    }

    if (sensor_power == FALSE)
    {
        res = MPU6050_SetPowerMode(client, true);
        if (res)
        {
            GSE_ERR("Power on mpu6050 error %d!\n", res);
        }
    }

    if ((res = MPU6050_ReadData(client, obj->data)))
    {
        GSE_ERR("I2C error: ret value=%d", res);
        return -3;
    }
    else
    {

	#if 1
        obj->data[MPU6050_AXIS_X] = obj->data[MPU6050_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
        obj->data[MPU6050_AXIS_Y] = obj->data[MPU6050_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
        obj->data[MPU6050_AXIS_Z] = obj->data[MPU6050_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;        
	#endif
        obj->data[MPU6050_AXIS_X] += obj->cali_sw[MPU6050_AXIS_X];
        obj->data[MPU6050_AXIS_Y] += obj->cali_sw[MPU6050_AXIS_Y];
        obj->data[MPU6050_AXIS_Z] += obj->cali_sw[MPU6050_AXIS_Z];

        /*remap coordinate*/
        acc[obj->cvt.map[MPU6050_AXIS_X]] = obj->cvt.sign[MPU6050_AXIS_X]*obj->data[MPU6050_AXIS_X];
        acc[obj->cvt.map[MPU6050_AXIS_Y]] = obj->cvt.sign[MPU6050_AXIS_Y]*obj->data[MPU6050_AXIS_Y];
        acc[obj->cvt.map[MPU6050_AXIS_Z]] = obj->cvt.sign[MPU6050_AXIS_Z]*obj->data[MPU6050_AXIS_Z];
		
        //Out put the mg
        #if 0
        acc[MPU6050_AXIS_X] = acc[MPU6050_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
        acc[MPU6050_AXIS_Y] = acc[MPU6050_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
        acc[MPU6050_AXIS_Z] = acc[MPU6050_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;        
		#endif
        sprintf(buf, "%04x %04x %04x", acc[MPU6050_AXIS_X], acc[MPU6050_AXIS_Y], acc[MPU6050_AXIS_Z]);
        if (atomic_read(&obj->trace) & MPU6050_TRC_IOCTL)
        {
            GSE_LOG("gsensor data: %s!\n", buf);
        }
    }

    return 0;
}
/*----------------------------------------------------------------------------*/
static int MPU6050_ReadRawData(struct i2c_client *client, char *buf)
{
    struct mpu6050_i2c_data *obj = (struct mpu6050_i2c_data*)i2c_get_clientdata(client);
    int res = 0;

    if (!buf || !client)
    {
        return EINVAL;
    }


    if (atomic_read(&obj->suspend))
    {
        return EIO;
    }

    if ((res = MPU6050_ReadData(client, obj->data)))
    {
        GSE_ERR("I2C error: ret value=%d", res);
        return EIO;
    }
    else
    {
        sprintf(buf, "%04x %04x %04x", obj->data[MPU6050_AXIS_X], 
                obj->data[MPU6050_AXIS_Y], obj->data[MPU6050_AXIS_Z]);

    }

    return 0;
}
/*----------------------------------------------------------------------------*/
static int MPU6050_InitSelfTest(struct i2c_client *client)
{
    int res = 0;
    u8  data;

    res = MPU6050_SetPowerMode(client, true);
    if (res != MPU6050_SUCCESS)
    {
        GSE_ERR("set power error\n");
        return res;
    }
    
    res = MPU6050_SetBWRate(client, MPU6050_BW_184HZ);
    if (res != MPU6050_SUCCESS ) //0x2C->BW=100Hz
    {
        return res;
    }

    res = mpu_i2c_read_block(client, MPU6050_REG_DATA_FORMAT, &data, 1);

    if (res != MPU6050_SUCCESS)
    {
        return res;
    }

    return MPU6050_SUCCESS;
}
/*----------------------------------------------------------------------------*/
#if !MPU6XXX_HWSELFTEST
static int MPU6050_JudgeTestResult(struct i2c_client *client, s32 prv[MPU6050_AXES_NUM], s32 nxt[MPU6050_AXES_NUM])
{
    struct criteria
    {
        int min;
        int max;
    };

    struct criteria self[4][3] = {
        {{ 0, 540}, { 0, 540}, { 0, 875}},
        {{ 0, 270}, { 0, 270}, { 0, 438}},
        {{ 0, 135}, { 0, 135}, { 0, 219}},            
        {{ 0,  67}, { 0,  67}, { 0, 110}},            
    };
    struct criteria (*ptr)[3] = NULL;
    u8 format;
    int res;
    if ((res = mpu_i2c_read_block(client, MPU6050_REG_DATA_FORMAT, &format, 1)))
        return res;

    format = format & MPU6050_RANGE_16G;

    switch (format)
    {
        case MPU6050_RANGE_2G:
            GSE_LOG("format use self[0]\n");
            ptr = &self[0];       
        break;

        case MPU6050_RANGE_4G:
            GSE_LOG("format use self[1]\n");
            ptr = &self[1];       
        break;
    
        case MPU6050_RANGE_8G:
            GSE_LOG("format use self[2]\n");
            ptr = &self[2];       
        break;
    
        case MPU6050_RANGE_16G:
            GSE_LOG("format use self[3]\n");
            ptr = &self[3];       
        break;
    
        default:
            GSE_LOG("format unknow use \n");
        break;
    }

    if (!ptr)
    {
        GSE_ERR("null pointer\n");
        return -EINVAL;
    }
    GSE_LOG("format=0x%x\n",format);

    GSE_LOG("X diff is %ld\n",abs(nxt[MPU6050_AXIS_X] - prv[MPU6050_AXIS_X]));
    GSE_LOG("Y diff is %ld\n",abs(nxt[MPU6050_AXIS_Y] - prv[MPU6050_AXIS_Y]));
    GSE_LOG("Z diff is %ld\n",abs(nxt[MPU6050_AXIS_Z] - prv[MPU6050_AXIS_Z]));


    if ((abs(nxt[MPU6050_AXIS_X] - prv[MPU6050_AXIS_X]) > (*ptr)[MPU6050_AXIS_X].max) ||
        (abs(nxt[MPU6050_AXIS_X] - prv[MPU6050_AXIS_X]) < (*ptr)[MPU6050_AXIS_X].min))
    {
        GSE_ERR("X is over range\n");
        res = -EINVAL;
    }
    if ((abs(nxt[MPU6050_AXIS_Y] - prv[MPU6050_AXIS_Y]) > (*ptr)[MPU6050_AXIS_Y].max) ||
        (abs(nxt[MPU6050_AXIS_Y] - prv[MPU6050_AXIS_Y]) < (*ptr)[MPU6050_AXIS_Y].min))
    {
        GSE_ERR("Y is over range\n");
        res = -EINVAL;
    }
    if ((abs(nxt[MPU6050_AXIS_Z] - prv[MPU6050_AXIS_Z]) > (*ptr)[MPU6050_AXIS_Z].max) ||
        (abs(nxt[MPU6050_AXIS_Z] - prv[MPU6050_AXIS_Z]) < (*ptr)[MPU6050_AXIS_Z].min))
    {
        GSE_ERR("Z is over range\n");
        res = -EINVAL;
    }
    return res;
}
#endif
/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
    struct i2c_client *client = mpu6050_i2c_client;
    char strbuf[MPU6050_BUFSIZE];
    if (NULL == client)
    {
        GSE_ERR("i2c client is null!!\n");
        return 0;
    }

    if (sensor_power == false)
    {
        MPU6050_SetPowerMode(client, true);
    }
	
    MPU6050_ReadAllReg(client, strbuf, MPU6050_BUFSIZE);

    MPU6050_ReadChipInfo(client, strbuf, MPU6050_BUFSIZE);
    return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);        
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
    struct i2c_client *client = mpu6050_i2c_client;
    char strbuf[MPU6050_BUFSIZE];

    if (NULL == client)
    {
        GSE_ERR("i2c client is null!!\n");
        return 0;
    }
    MPU6050_ReadSensorData(client, strbuf, MPU6050_BUFSIZE);
    return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
    struct i2c_client *client = mpu6050_i2c_client;
    struct mpu6050_i2c_data *obj;
    int err, len = 0, mul;
    int tmp[MPU6050_AXES_NUM];

    if (NULL == client)
    {
        GSE_ERR("i2c client is null!!\n");
        return 0;
    }

    obj = i2c_get_clientdata(client);


    if ((err = MPU6050_ReadOffset(client, obj->offset)))
    {
        return -EINVAL;
    }
    else if ((err = MPU6050_ReadCalibration(client, tmp)))
    {
        return -EINVAL;
    }
    else
    {
        mul = obj->reso->sensitivity/mpu6050_offset_resolution.sensitivity;
        len += snprintf(buf+len, PAGE_SIZE-len, "[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n", mul,                        
                        obj->offset[MPU6050_AXIS_X], obj->offset[MPU6050_AXIS_Y], obj->offset[MPU6050_AXIS_Z],
                        obj->offset[MPU6050_AXIS_X], obj->offset[MPU6050_AXIS_Y], obj->offset[MPU6050_AXIS_Z]);
        len += snprintf(buf+len, PAGE_SIZE-len, "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1, 
                        obj->cali_sw[MPU6050_AXIS_X], obj->cali_sw[MPU6050_AXIS_Y], obj->cali_sw[MPU6050_AXIS_Z]);

        len += snprintf(buf+len, PAGE_SIZE-len, "[ALL]    (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n", 
                        obj->offset[MPU6050_AXIS_X]*mul + obj->cali_sw[MPU6050_AXIS_X],
                        obj->offset[MPU6050_AXIS_Y]*mul + obj->cali_sw[MPU6050_AXIS_Y],
                        obj->offset[MPU6050_AXIS_Z]*mul + obj->cali_sw[MPU6050_AXIS_Z],
                        tmp[MPU6050_AXIS_X], tmp[MPU6050_AXIS_Y], tmp[MPU6050_AXIS_Z]);

        return len;
    }
}
/*----------------------------------------------------------------------------*/
static ssize_t store_cali_value(struct device_driver *ddri, const char *buf, size_t count)
{
    struct i2c_client *client = mpu6050_i2c_client;  
    int err, x, y, z;
    int dat[MPU6050_AXES_NUM];

    if (!strncmp(buf, "rst", 3))
    {
        if ((err = MPU6050_ResetCalibration(client)))
        {
            GSE_ERR("reset offset err = %d\n", err);
        }
    }
    else if (3 == sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z))
    {
        dat[MPU6050_AXIS_X] = x;
        dat[MPU6050_AXIS_Y] = y;
        dat[MPU6050_AXIS_Z] = z;
        if ((err = MPU6050_WriteCalibration(client, dat)))
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
#if MPU6XXX_HWSELFTEST
extern int inv_hw_self_test(struct inv_selftest_device *st);
int selftestret=0;

static ssize_t show_self_value(struct device_driver *ddri, char *buf)
{
    struct i2c_client *client = mpu6050_i2c_client;
    struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
    int len = 0;

    if (NULL == client)
    {
        GSE_ERR("i2c client is null!!\n");
        return 0;
    }

    if(selftestret & 0x1)
        len += snprintf(buf+len, PAGE_SIZE-len, "Self-Test result=%d- Gyro = PASS \t",selftestret);
    else
        len += snprintf(buf+len, PAGE_SIZE-len, "Self-Test result=%d- Gyro = FAIL \t",selftestret);
    if(selftestret & 0x2)
        len += snprintf(buf+len, PAGE_SIZE-len, "Accel = PASS\n");
    else
        len += snprintf(buf+len, PAGE_SIZE-len, "Accel = FAIL\n");

    len += snprintf(buf+len, PAGE_SIZE-len, "[gyro_bias](%+3d, %+3d, %+3d)\n",obj->mpu_selftest_device.gyro_bias[MPU6050_AXIS_X], obj->mpu_selftest_device.gyro_bias[MPU6050_AXIS_Y], obj->mpu_selftest_device.gyro_bias[MPU6050_AXIS_Z]);
    len += snprintf(buf+len, PAGE_SIZE-len, "[accel_bias](%+3d, %+3d, %+3d)\n",obj->mpu_selftest_device.accel_bias[MPU6050_AXIS_X], obj->mpu_selftest_device.accel_bias[MPU6050_AXIS_Y], obj->mpu_selftest_device.accel_bias[MPU6050_AXIS_Z]);
    len += snprintf(buf+len, PAGE_SIZE-len, "[gyro_bias_st](%+3d, %+3d, %+3d)\n",obj->mpu_selftest_device.gyro_bias_st[MPU6050_AXIS_X], obj->mpu_selftest_device.gyro_bias_st[MPU6050_AXIS_Y], obj->mpu_selftest_device.gyro_bias_st[MPU6050_AXIS_Z]);
    len += snprintf(buf+len, PAGE_SIZE-len, "[accel_bias_st](%+3d, %+3d, %+3d)\n",obj->mpu_selftest_device.accel_bias_st[MPU6050_AXIS_X], obj->mpu_selftest_device.accel_bias_st[MPU6050_AXIS_Y], obj->mpu_selftest_device.accel_bias_st[MPU6050_AXIS_Z]);
    return len;
}
/*----------------------------------------------------------------------------*/

static ssize_t store_self_value(struct device_driver *ddri, const char *buf, size_t count)
{
    struct i2c_client *client = mpu6050_i2c_client;  
    struct mpu6050_i2c_data *priv = i2c_get_clientdata(client);        
    int ret;
    //u8 databuf[2] = {0};    
    //u8 data;
#if 1    
    MPU6050_SetPowerMode(client,true);

    ret = inv_hw_self_test(&(priv->mpu_selftest_device));
    selftestret = ret;
    if (ret == 3)
    {
        GSE_LOG("SELFTEST : PASS\n");
        strcpy(selftestRes,"y");
    }
    else
    {
        GSE_LOG("SELFTEST : FAIL\n");
        strcpy(selftestRes,"n");
    }
#endif
    return count;	
}
#else
/*----------------------------------------------------------------------------*/
static ssize_t show_self_value(struct device_driver *ddri, char *buf)
{
    struct i2c_client *client = mpu6050_i2c_client;

    if (NULL == client)
    {
        GSE_ERR("i2c client is null!!\n");
        return 0;
    }

    return snprintf(buf, 8, "%s\n", selftestRes);
}
/*----------------------------------------------------------------------------*/
static ssize_t store_self_value(struct device_driver *ddri, const char *buf, size_t count)
{   /*write anything to this register will trigger the process*/
    struct item
    {
        s16 raw[MPU6050_AXES_NUM];
    };

    struct i2c_client *client = mpu6050_i2c_client;  
    int idx, res, num;
    struct item *prv = NULL, *nxt = NULL;
    s32 avg_prv[MPU6050_AXES_NUM] = {0, 0, 0};
    s32 avg_nxt[MPU6050_AXES_NUM] = {0, 0, 0};


    if (1 != sscanf(buf, "%d", &num))
    {
        GSE_ERR("parse number fail\n");
        return count;
    }
    else if (num == 0)
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
    MPU6050_SetPowerMode(client,true);

    for (idx = 0; idx < num; idx++)
    {
        if ((res = MPU6050_ReadData(client, prv[idx].raw)))
        {
            GSE_ERR("read data fail: %d\n", res);
            goto exit;
        }

        avg_prv[MPU6050_AXIS_X] += prv[idx].raw[MPU6050_AXIS_X];
        avg_prv[MPU6050_AXIS_Y] += prv[idx].raw[MPU6050_AXIS_Y];
        avg_prv[MPU6050_AXIS_Z] += prv[idx].raw[MPU6050_AXIS_Z];        
        GSE_LOG("[%5d %5d %5d]\n", prv[idx].raw[MPU6050_AXIS_X], prv[idx].raw[MPU6050_AXIS_Y], prv[idx].raw[MPU6050_AXIS_Z]);
    }

    avg_prv[MPU6050_AXIS_X] /= num;
    avg_prv[MPU6050_AXIS_Y] /= num;
    avg_prv[MPU6050_AXIS_Z] /= num;    

    /*initial setting for self test*/
    GSE_LOG("SELFTEST:\n");    
    for (idx = 0; idx < num; idx++)
    {
        if ((res = MPU6050_ReadData(client, nxt[idx].raw)))
        {
            GSE_ERR("read data fail: %d\n", res);
            goto exit;
        }
        avg_nxt[MPU6050_AXIS_X] += nxt[idx].raw[MPU6050_AXIS_X];
        avg_nxt[MPU6050_AXIS_Y] += nxt[idx].raw[MPU6050_AXIS_Y];
        avg_nxt[MPU6050_AXIS_Z] += nxt[idx].raw[MPU6050_AXIS_Z];        
        GSE_LOG("[%5d %5d %5d]\n", nxt[idx].raw[MPU6050_AXIS_X], nxt[idx].raw[MPU6050_AXIS_Y], nxt[idx].raw[MPU6050_AXIS_Z]);
    }

    avg_nxt[MPU6050_AXIS_X] /= num;
    avg_nxt[MPU6050_AXIS_Y] /= num;
    avg_nxt[MPU6050_AXIS_Z] /= num;    

    GSE_LOG("X: %5d - %5d = %5d \n", avg_nxt[MPU6050_AXIS_X], avg_prv[MPU6050_AXIS_X], avg_nxt[MPU6050_AXIS_X] - avg_prv[MPU6050_AXIS_X]);
    GSE_LOG("Y: %5d - %5d = %5d \n", avg_nxt[MPU6050_AXIS_Y], avg_prv[MPU6050_AXIS_Y], avg_nxt[MPU6050_AXIS_Y] - avg_prv[MPU6050_AXIS_Y]);
    GSE_LOG("Z: %5d - %5d = %5d \n", avg_nxt[MPU6050_AXIS_Z], avg_prv[MPU6050_AXIS_Z], avg_nxt[MPU6050_AXIS_Z] - avg_prv[MPU6050_AXIS_Z]); 

    if (!MPU6050_JudgeTestResult(client, avg_prv, avg_nxt))
    {
        GSE_LOG("SELFTEST : PASS\n");
        strcpy(selftestRes,"y");
    }
    else
    {
        GSE_LOG("SELFTEST : FAIL\n");
        strcpy(selftestRes,"n");
    }

    exit:
    /*restore the setting*/    
    mpu6050_init_client(client, 0);
    kfree(prv);
    kfree(nxt);
    return count;
}
#endif
/*----------------------------------------------------------------------------*/
static ssize_t show_selftest_value(struct device_driver *ddri, char *buf)
{
    struct i2c_client *client = mpu6050_i2c_client;
    struct mpu6050_i2c_data *obj;

    if (NULL == client)
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
    struct mpu6050_i2c_data *obj = obj_i2c_data;
    int tmp;

    if (NULL == obj)
    {
        GSE_ERR("i2c data obj is null!!\n");
        return 0;
    }


    if (1 == sscanf(buf, "%d", &tmp))
    {
        if (atomic_read(&obj->selftest) && !tmp)
        {
            /*enable -> disable*/
            mpu6050_init_client(obj->client, 0);
        }
        else if (!atomic_read(&obj->selftest) && tmp)
        {
            /*disable -> enable*/
            MPU6050_InitSelfTest(obj->client);            
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
#ifdef CONFIG_MPU6050_LOWPASS
    struct i2c_client *client = mpu6050_i2c_client;
    struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
    if (atomic_read(&obj->firlen))
    {
        int idx, len = atomic_read(&obj->firlen);
        GSE_LOG("len = %2d, idx = %2d\n", obj->fir.num, obj->fir.idx);

        for (idx = 0; idx < len; idx++)
        {
            GSE_LOG("[%5d %5d %5d]\n", obj->fir.raw[idx][MPU6050_AXIS_X], obj->fir.raw[idx][MPU6050_AXIS_Y], obj->fir.raw[idx][MPU6050_AXIS_Z]);
        }

        GSE_LOG("sum = [%5d %5d %5d]\n", obj->fir.sum[MPU6050_AXIS_X], obj->fir.sum[MPU6050_AXIS_Y], obj->fir.sum[MPU6050_AXIS_Z]);
        GSE_LOG("avg = [%5d %5d %5d]\n", obj->fir.sum[MPU6050_AXIS_X]/len, obj->fir.sum[MPU6050_AXIS_Y]/len, obj->fir.sum[MPU6050_AXIS_Z]/len);
    }
    return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&obj->firlen));
#else
    return snprintf(buf, PAGE_SIZE, "not support\n");
#endif
}
/*----------------------------------------------------------------------------*/
static ssize_t store_firlen_value(struct device_driver *ddri, const char *buf, size_t count)
{
#ifdef CONFIG_MPU6050_LOWPASS
    struct i2c_client *client = mpu6050_i2c_client;  
    struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);
    int firlen;

    if (1 != sscanf(buf, "%d", &firlen))
    {
        GSE_ERR("invallid format\n");
    }
    else if (firlen > C_MAX_FIR_LENGTH)
    {
        GSE_ERR("exceeds maximum filter length\n");
    }
    else
    {
        atomic_set(&obj->firlen, firlen);
        if (0 == firlen)
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
    struct mpu6050_i2c_data *obj = obj_i2c_data;
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
    struct mpu6050_i2c_data *obj = obj_i2c_data;
    int trace;
    if (obj == NULL)
    {
        GSE_ERR("i2c_data obj is null!!\n");
        return 0;
    }

    if (1 == sscanf(buf, "0x%x", &trace))
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
    struct mpu6050_i2c_data *obj = obj_i2c_data;
    if (obj == NULL)
    {
        GSE_ERR("i2c_data obj is null!!\n");
        return 0;
    }

    if (obj->hw)
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
static DRIVER_ATTR(cali,       S_IWUSR | S_IRUGO, show_cali_value,          store_cali_value);
static DRIVER_ATTR(self,       S_IWUSR | S_IRUGO, show_selftest_value,          store_selftest_value);
static DRIVER_ATTR(selftest,   S_IWUSR | S_IRUGO, show_self_value ,      store_self_value );
static DRIVER_ATTR(firlen,     S_IWUSR | S_IRUGO, show_firlen_value,        store_firlen_value);
static DRIVER_ATTR(trace,      S_IWUSR | S_IRUGO, show_trace_value,         store_trace_value);
static DRIVER_ATTR(status,               S_IRUGO, show_status_value,        NULL);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *mpu6050_attr_list[] = {
    &driver_attr_chipinfo,     /*chip information*/
    &driver_attr_sensordata,   /*dump sensor data*/
    &driver_attr_cali,         /*show calibration data*/
    &driver_attr_self,         /*self test demo*/
    &driver_attr_selftest,     /*self control: 0: disable, 1: enable*/
    &driver_attr_firlen,       /*filter length: 0: disable, others: enable*/
    &driver_attr_trace,        /*trace log*/
    &driver_attr_status,        
};
/*----------------------------------------------------------------------------*/
static int mpu6050_create_attr(struct device_driver *driver) 
{
    int idx, err = 0;
    int num = (int)(sizeof(mpu6050_attr_list)/sizeof(mpu6050_attr_list[0]));
    if (driver == NULL)
    {
        return -EINVAL;
    }

    for (idx = 0; idx < num; idx++)
    {
        if (0 != (err = driver_create_file(driver, mpu6050_attr_list[idx])))
        {
            GSE_ERR("driver_create_file (%s) = %d\n", mpu6050_attr_list[idx]->attr.name, err);
            break;
        }
    }    
    return err;
}
/*----------------------------------------------------------------------------*/
static int mpu6050_delete_attr(struct device_driver *driver)
{
    int idx ,err = 0;
    int num = (int)(sizeof(mpu6050_attr_list)/sizeof(mpu6050_attr_list[0]));

    if (driver == NULL)
    {
        return -EINVAL;
    }

    for (idx = 0; idx < num; idx++)
    {
        driver_remove_file(driver, mpu6050_attr_list[idx]);
    }

    return err;
}

/*----------------------------------------------------------------------------*/
int gsensor_operate(void* self, uint32_t command, void* buff_in, int size_in,
                    void* buff_out, int size_out, int* actualout)
{
    int err = 0;
    int value, sample_delay;    
    struct mpu6050_i2c_data *priv = (struct mpu6050_i2c_data*)self;
    hwm_sensor_data* gsensor_data;
    char buff[MPU6050_BUFSIZE];


    switch (command)
    {
    case SENSOR_DELAY:
        if ((buff_in == NULL) || (size_in < sizeof(int)))
        {
            GSE_ERR("Set delay parameter error!\n");
            err = -EINVAL;
        }
        else
        {
            value = *(int *)buff_in;
            
            if(value <= 5)
            {
            	sample_delay = MPU6050_BW_184HZ;
            }
            else if(value <= 10)
            {
            	sample_delay = MPU6050_BW_94HZ;
            }
            else
            {
            	sample_delay = MPU6050_BW_44HZ;
            }
            GSE_LOG("Set delay parameter value:%d \n", value);
        

            err = MPU6050_SetBWRate(priv->client, sample_delay);
            if (err != MPU6050_SUCCESS ) //0x2C->BW=100Hz
            {
                GSE_ERR("Set delay parameter error!\n");
            }

            if (value >= 50)
            {
                atomic_set(&priv->filter, 0);
            }
            else
            {
#if defined(CONFIG_MPU6050_LOWPASS)				
                priv->fir.num = 0;
                priv->fir.idx = 0;
                priv->fir.sum[MPU6050_AXIS_X] = 0;
                priv->fir.sum[MPU6050_AXIS_Y] = 0;
                priv->fir.sum[MPU6050_AXIS_Z] = 0;
#endif
                atomic_set(&priv->filter, 1);
            }
        }
        break;

    case SENSOR_ENABLE:
        if ((buff_in == NULL) || (size_in < sizeof(int)))
        {
            GSE_ERR("Enable sensor parameter error!\n");
            err = -EINVAL;
        }
        else
        {
            value = *(int *)buff_in;
            if (((value == 0) && (sensor_power == false)) ||((value == 1) && (sensor_power == true)))
            {
                GSE_LOG("Gsensor device have updated!\n");
            }
            else
            {
                err = MPU6050_SetPowerMode( priv->client, !sensor_power);
            }
        }
        break;

    case SENSOR_GET_DATA:
        if ((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
        {
            GSE_ERR("get sensor data parameter error!\n");
            err = -EINVAL;
        }
        else
        {
            gsensor_data = (hwm_sensor_data *)buff_out;
            err = MPU6050_ReadSensorData(priv->client, buff, MPU6050_BUFSIZE);
            if (!err)
            {
                sscanf(buff, "%x %x %x", &gsensor_data->values[0], 
                       &gsensor_data->values[1], &gsensor_data->values[2]);             
                gsensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;                
                gsensor_data->value_divide = 1000;
            }
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
static int mpu6050_open(struct inode *inode, struct file *file)
{
    file->private_data = mpu6050_i2c_client;

    if (file->private_data == NULL)
    {
        GSE_ERR("null pointer!!\n");
        return -EINVAL;
    }
    return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int mpu6050_release(struct inode *inode, struct file *file)
{
    file->private_data = NULL;
    return 0;
}
/*----------------------------------------------------------------------------*/
static long mpu6050_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
    struct i2c_client *client = (struct i2c_client*)file->private_data;
    struct mpu6050_i2c_data *obj = (struct mpu6050_i2c_data*)i2c_get_clientdata(client);    
    char strbuf[MPU6050_BUFSIZE];
    void __user *data;
    SENSOR_DATA sensor_data;
    long err = 0;
    int cali[3];

    if (_IOC_DIR(cmd) & _IOC_READ)
    {
        err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    }
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
    {
        err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
    }

    if (err)
    {
        GSE_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
        return -EFAULT;
    }

    switch (cmd)
    {
    case GSENSOR_IOCTL_INIT:
        mpu6050_init_client(client, 0);         
        break;

    case GSENSOR_IOCTL_READ_CHIPINFO:
        data = (void __user *) arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }

        MPU6050_ReadChipInfo(client, strbuf, MPU6050_BUFSIZE);
        if (copy_to_user(data, strbuf, strlen(strbuf)+1))
        {
            err = -EFAULT;
            break;
        }
        break;    

    case GSENSOR_IOCTL_READ_SENSORDATA:
        data = (void __user *) arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }

        MPU6050_ReadSensorData(client, strbuf, MPU6050_BUFSIZE);
        if (copy_to_user(data, strbuf, strlen(strbuf)+1))
        {
            err = -EFAULT;
            break;    
        }
        break;

    case GSENSOR_IOCTL_READ_GAIN:
        data = (void __user *) arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }

        if (copy_to_user(data, &gsensor_gain, sizeof(GSENSOR_VECTOR3D)))
        {
            err = -EFAULT;
            break;
        }
        break;

    case GSENSOR_IOCTL_READ_RAW_DATA:
        data = (void __user *) arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }

        if (atomic_read(&obj->suspend))
        {
            err = -EINVAL;
        }
        else
        {
        MPU6050_ReadRawData(client, strbuf);
        if (copy_to_user(data, strbuf, strlen(strbuf)+1))
        {
            err = -EFAULT;
            break;    
        }
        }
        break;    

    case GSENSOR_IOCTL_SET_CALI:
        data = (void __user*)arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }
        if (copy_from_user(&sensor_data, data, sizeof(sensor_data)))
        {
            err = -EFAULT;
            break;    
        }
        if (atomic_read(&obj->suspend))
        {
            GSE_ERR("Perform calibration in suspend state!!\n");
            err = -EINVAL;
        }
        else
        {
        #if 0
            cali[MPU6050_AXIS_X] = sensor_data.x * obj->reso->sensitivity / GRAVITY_EARTH_1000;
            cali[MPU6050_AXIS_Y] = sensor_data.y * obj->reso->sensitivity / GRAVITY_EARTH_1000;
            cali[MPU6050_AXIS_Z] = sensor_data.z * obj->reso->sensitivity / GRAVITY_EARTH_1000;           
		#else
			cali[MPU6050_AXIS_X] = sensor_data.x;
            cali[MPU6050_AXIS_Y] = sensor_data.y;
            cali[MPU6050_AXIS_Z] = sensor_data.z;           
		
		#endif
			err = MPU6050_WriteCalibration(client, cali);            
        }
        break;

    case GSENSOR_IOCTL_CLR_CALI:
        err = MPU6050_ResetCalibration(client);
        break;

    case GSENSOR_IOCTL_GET_CALI:
        data = (void __user*)arg;
        if (data == NULL)
        {
            err = -EINVAL;
            break;    
        }
        if ((err = MPU6050_ReadCalibration(client, cali)))
        {
            break;
        }
		#if 0
        sensor_data.x = cali[MPU6050_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
        sensor_data.y = cali[MPU6050_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
        sensor_data.z = cali[MPU6050_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
		#else
		sensor_data.x = cali[MPU6050_AXIS_X];
        sensor_data.y = cali[MPU6050_AXIS_Y];
        sensor_data.z = cali[MPU6050_AXIS_Z];
		
		#endif
		if (copy_to_user(data, &sensor_data, sizeof(sensor_data)))
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
static struct file_operations mpu6050_fops = {
    .open = mpu6050_open,
    .release = mpu6050_release,
    .unlocked_ioctl = mpu6050_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice mpu6050_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "gsensor",
    .fops = &mpu6050_fops,
};
/*----------------------------------------------------------------------------*/
#ifndef CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int mpu6050_suspend(struct i2c_client *client, pm_message_t msg) 
{
    struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);    
    int err = 0;
    GSE_FUN();    

    if (msg.event == PM_EVENT_SUSPEND)
    {
        if (obj == NULL)
        {
            GSE_ERR("null pointer!!\n");
            return -EINVAL;
        }
        atomic_set(&obj->suspend, 1);

        if ((err = MPU6050_SetPowerMode(obj->client, false)))
        {
            GSE_ERR("write power control fail!!\n");
            return err;
        }
        MPU6050_power(obj->hw, 0);
        GSE_LOG("mpu6050_suspend ok\n");
    }
    return err;
}
/*----------------------------------------------------------------------------*/
static int mpu6050_resume(struct i2c_client *client)
{
    struct mpu6050_i2c_data *obj = i2c_get_clientdata(client);        
    int err;
    GSE_FUN();

    if (obj == NULL)
    {
        GSE_ERR("null pointer!!\n");
        return -EINVAL;
    }

    MPU6050_power(obj->hw, 1);

    if ((err = mpu6050_init_client(client, 0)))
    {
        GSE_ERR("initialize client fail!!\n");
        return err;        
    }
    atomic_set(&obj->suspend, 0);
    GSE_LOG("mpu6050_resume ok\n");

    return 0;
}
/*----------------------------------------------------------------------------*/
#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void mpu6050_early_suspend(struct early_suspend *h) 
{
    struct mpu6050_i2c_data *obj = container_of(h, struct mpu6050_i2c_data, early_drv);   
    int err;
    GSE_FUN();    

    if (obj == NULL)
    {
        GSE_ERR("null pointer!!\n");
        return;
    }
    atomic_set(&obj->suspend, 1); 

    if ((err = MPU6050_SetPowerMode(obj->client, false)))
    {
        GSE_ERR("write power control fail!!\n");
        return;
    }

    /*
    if (MPU6050_gyro_mode() == false)
    {
        MPU6050_Dev_Reset(obj->client);
        MPU6050_Reset(obj->client);
    }
    */

    obj->bandwidth = 0;

    sensor_power = false;

    MPU6050_power(obj->hw, 0);
}
/*----------------------------------------------------------------------------*/
static void mpu6050_late_resume(struct early_suspend *h)
{
    struct mpu6050_i2c_data *obj = container_of(h, struct mpu6050_i2c_data, early_drv);         
    int err;
    GSE_FUN();

    if (obj == NULL)
    {
        GSE_ERR("null pointer!!\n");
        return;
    }

    MPU6050_power(obj->hw, 1);

    if ((err = mpu6050_init_client(obj->client, 0)))
    {
        GSE_ERR("initialize client fail!!\n");
        return;        
    }
    atomic_set(&obj->suspend, 0);    
}
/*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/
static int mpu6050_i2c_detect(struct i2c_client *client, struct i2c_board_info *info) 
{    
    strcpy(info->type, MPU6050_DEV_NAME);
    return 0;
}
/*----------------------------------------------------------------------------*/
static int mpu6050_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct i2c_client *new_client;
    struct mpu6050_i2c_data *obj;
    struct hwmsen_object sobj;
    int err = 0;
    GSE_FUN();

    if (!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
    {
        err = -ENOMEM;
        goto exit;
    }

    memset(obj, 0, sizeof(struct mpu6050_i2c_data));

    obj->hw = get_cust_acc_hw();

    if ((err = hwmsen_get_convert(obj->hw->direction, &obj->cvt)))
    {
        GSE_ERR("invalid direction: %d\n", obj->hw->direction);
        goto exit;
    }

    obj_i2c_data = obj;
    obj->client = client;
    obj->client->timing = 400;

    new_client = obj->client;
    i2c_set_clientdata(new_client,obj);

    atomic_set(&obj->trace, 0);
    atomic_set(&obj->suspend, 0);

#ifdef CONFIG_MPU6050_LOWPASS
    if (obj->hw->firlen > C_MAX_FIR_LENGTH)
    {
        atomic_set(&obj->firlen, C_MAX_FIR_LENGTH);
    }
    else
    {
        atomic_set(&obj->firlen, obj->hw->firlen);
    }

    if (atomic_read(&obj->firlen) > 0)
    {
        atomic_set(&obj->fir_en, 1);
    }

#endif

    mpu6050_i2c_client = new_client;    
    MPU6050_Dev_Reset(new_client);
    MPU6050_Reset(new_client);

    if ((err = mpu6050_init_client(new_client, 1)))
    {
        goto exit_init_failed;
    }


    if ((err = misc_register(&mpu6050_device)))
    {
        GSE_ERR("mpu6050_device register failed\n");
        goto exit_misc_device_register_failed;
    }


    if ((err = mpu6050_create_attr(&mpu6050_gsensor_driver.driver)))
    {
        GSE_ERR("create attribute err = %d\n", err);
        goto exit_create_attr_failed;
    }

    sobj.self = obj;
    sobj.polling = 1;
    sobj.sensor_operate = gsensor_operate;
    if ((err = hwmsen_attach(ID_ACCELEROMETER, &sobj)))
    {
        GSE_ERR("attach fail = %d\n", err);
        goto exit_kfree;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    obj->early_drv.level    = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2,
    obj->early_drv.suspend  = mpu6050_early_suspend,
    obj->early_drv.resume   = mpu6050_late_resume,    
    register_early_suspend(&obj->early_drv);
#endif 

#if MPU6XXX_HWSELFTEST
    obj->mpu_selftest_device.name = "MPU6XXX";
    obj->mpu_selftest_device.chip_type = INV_MPU6500;//MPU6515 don't need to modify this chip_type
    obj->mpu_selftest_device.samples = INIT_ST_SAMPLES;
#endif

    GSE_LOG("%s: OK\n", __func__);    
    return 0;

    exit_create_attr_failed:
    misc_deregister(&mpu6050_device);
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
static int mpu6050_i2c_remove(struct i2c_client *client)
{
    int err = 0;    

    if ((err =  mpu6050_delete_attr(&mpu6050_gsensor_driver.driver)))
    {
        GSE_ERR("mpu6050_delete_attr fail: %d\n", err);
    }

    if ((err = misc_deregister(&mpu6050_device)))
    {
        GSE_ERR("misc_deregister fail: %d\n", err);
    }

    if ((err = hwmsen_detach(ID_ACCELEROMETER)))
    {
        GSE_ERR("hwmsen_detach fail: %d\n", err);
    }

    mpu6050_i2c_client = NULL;
    i2c_unregister_device(client);
    kfree(i2c_get_clientdata(client));
    return 0;
}
/*----------------------------------------------------------------------------*/
static int mpu6050_probe(struct platform_device *pdev) 
{
    struct acc_hw *hw = get_cust_acc_hw();
    GSE_FUN();

    MPU6050_power(hw, 1);
    if (i2c_add_driver(&mpu6050_i2c_driver))
    {
        GSE_ERR("add driver error\n");
        return -1;
    }
    return 0;
}
/*----------------------------------------------------------------------------*/
static int mpu6050_remove(struct platform_device *pdev)
{
    struct acc_hw *hw = get_cust_acc_hw();

    GSE_FUN();    
    MPU6050_power(hw, 0);    
    i2c_del_driver(&mpu6050_i2c_driver);
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
	
	static struct platform_driver mpu6050_gsensor_driver = {
		.probe		= mpu6050_probe,
		.remove 	= mpu6050_remove,	 
		.driver 	= 
		{
			.name  = "gsensor",
		//	.owner  = THIS_MODULE,
        #ifdef CONFIG_OF
			.of_match_table = gsensor_of_match,
		#endif
		}
	};
#else

static struct platform_driver mpu6050_gsensor_driver = {
    .probe      = mpu6050_probe,
    .remove     = mpu6050_remove,    
    .driver     = {
        .name  = "gsensor",
    }
};
#endif
/*----------------------------------------------------------------------------*/
static int __init mpu6050gse_init(void)
{
    struct acc_hw *hw = get_cust_acc_hw();
	GSE_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
    i2c_register_board_info(hw->i2c_num, &i2c_mpu6050, 1);
    if (platform_driver_register(&mpu6050_gsensor_driver))
    {
        GSE_ERR("failed to register driver");
        return -ENODEV;
    }
    return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit mpu6050gse_exit(void)
{
    GSE_FUN();
    platform_driver_unregister(&mpu6050_gsensor_driver);
}
/*----------------------------------------------------------------------------*/
module_init(mpu6050gse_init);
module_exit(mpu6050gse_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MPU6050 gse driver");
MODULE_AUTHOR("Yucong.Xiong@mediatek.com");
