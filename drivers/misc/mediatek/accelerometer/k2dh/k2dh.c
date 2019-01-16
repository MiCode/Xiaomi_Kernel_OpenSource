/* K2DH motion sensor driver
 *
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2010 STMicroelectronics
 * All Rights Reserved
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
#include <linux/module.h>

#define MT6582

#ifdef MT6516
#include <mach/mt6516_devs.h>
#include <mach/mt6516_typedefs.h>
#include <mach/mt6516_gpio.h>
#include <mach/mt6516_pll.h>
#endif

#ifdef MT6573
#include <mach/mt6573_devs.h>
#include <mach/mt6573_typedefs.h>
#include <mach/mt6573_gpio.h>
#include <mach/mt6573_pll.h>
#endif

#ifdef MT6575
#include <mach/mt_devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#endif

#ifdef MT6577
#include <mach/mt6577_devs.h>
#include <mach/mt6577_typedefs.h>
#include <mach/mt6577_gpio.h>
#include <mach/mt6577_pm_ldo.h>
#endif

#ifdef MT6572
#include <mach/devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#endif

#ifdef MT6582
#include <mach/devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#endif

#ifdef MT6516
#define POWER_NONE_MACRO MT6516_POWER_NONE
#endif

#ifdef MT6573
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#endif

#ifdef MT6575
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#endif

#ifdef MT6577
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#endif

#ifdef MT6572
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#endif

#ifdef MT6582
#define POWER_NONE_MACRO MT65XX_POWER_NONE
#endif


#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include "k2dh.h"
#include <linux/hwmsen_helper.h>

/*----------------------------------------------------------------------------*/
#define DEBUG 1
/*----------------------------------------------------------------------------*/
//#define CONFIG_K2DH_LOWPASS   /*apply low pass filter on output*/

#define K2DH_AXIS_X             0
#define K2DH_AXIS_Y             1
#define K2DH_AXIS_Z             2
#define K2DH_AXES_NUM      3
#define K2DH_DATA_LEN       6

#define K2DH_MODE_LOWPOWER                  0
#define K2DH_MODE_NORMAL                        1
#define K2DH_MODE_HIGH_RESOLUTION     2

#define CALIBRATION_DATA_AMOUNT         10

#define K2DH_ACCEL_CALIBRATION        // Accelerometer Sensor Calibration Enable Feature
/*----------------------------------------------------------------------------*/
#define HIGH_RESOLUTION                 0x08

#define AXISDATA_REG                        0x28
#define WHOAMI_K2DH_ACC               0x33    /*  Expctd content for WAI    */

/*  CONTROL REGISTERS    */
#define WHO_AM_I                                0x0F    /*    WhoAmI register        */
#define TEMP_CFG_REG                      0x1F    /*    temper sens control reg    */
/* ctrl 1: ODR3 ODR2 ODR ODR0 LPen Zenable Yenable Zenable */
#define CTRL_REG1                              0x20    /*    control reg 1        */
#define CTRL_REG2                              0x21    /*    control reg 2        */
#define CTRL_REG3                              0x22    /*    control reg 3        */
#define CTRL_REG4                              0x23    /*    control reg 4        */
#define CTRL_REG5                              0x24    /*    control reg 5        */
#define CTRL_REG6                              0x25    /*    control reg 6        */

#define FIFO_CTRL_REG                      0x2E    /*    FiFo control reg    */

#define INT_CFG1                                 0x30    /*    interrupt 1 config    */
#define INT_SRC1                                 0x31    /*    interrupt 1 source    */
#define INT_THS1                                  0x32    /*    interrupt 1 threshold    */
#define INT_DUR1                                 0x33    /*    interrupt 1 duration    */

#define INT_CFG2                                 0x34    /*    interrupt 2 config    */
#define INT_SRC2                                 0x35    /*    interrupt 2 source    */
#define INT_THS2                                 0x36    /*    interrupt 2 threshold    */
#define INT_DUR2                                 0x37    /*    interrupt 2 duration    */

#define TT_CFG                                    0x38    /*    tap config        */
#define TT_SRC                                    0x39    /*    tap source        */
#define TT_THS                                    0x3A    /*    tap threshold        */
#define TT_LIM                                     0x3B    /*    tap time limit        */
#define TT_TLAT                                   0x3C    /*    tap time latency    */
#define TT_TW                                      0x3D    /*    tap time window        */
/*    end CONTROL REGISTRES    */

#define K2DH_ACC_ENABLE_ALL_AXES              0x07
#define K2DH_ACC_ENABLE_ALL_AXES_MSK    0x07

#define ODR_POWERDOWN_MODE                0x00  /* Power Down Mode */
#define ODR1                0x10  /* 1Hz output data rate */
#define ODR10              0x20  /* 10Hz output data rate */
#define ODR25              0x30  /* 25Hz output data rate */
#define ODR50              0x40  /* 50Hz output data rate */
#define ODR100            0x50  /* 100Hz output data rate */
#define ODR200            0x60  /* 200Hz output data rate */  // default speed
#define ODR400            0x70  /* 400Hz output data rate */
#define ODR1344          0x90  /* 1344Hz output data rate */

#define I2C_RETRY_DELAY            5
#define I2C_RETRIES                     5
#define I2C_AUTO_INCREMENT    0x80

#define K2DH_LP_EN_MSK      0x08
#define K2DH_LP_EN_POS      3
#define K2DH_LP_EN_REG      CTRL_REG1

#define K2DH_HR_MSK             0x08
#define K2DH_HR_POS             3
#define K2DH_HR_REG             CTRL_REG4

#define K2DH_FS_MSK             0x30
#define K2DH_FS_POS             4
#define K2DH_FS_REG             CTRL_REG4

#define K2DH_ODR_MSK          0xF0
#define K2DH_ODR_POS          4
#define K2DH_ODR_REG          CTRL_REG1

#ifdef K2DH_ACCEL_CALIBRATION
#define K2DH_SHAKING_DETECT_THRESHOLD    (200)     //clubsh cal2 50 -> 200

struct K2DHacc{
    s16    x,
        y,
        z;
} ;
#endif

#define SELF_TEST_2G_MAX_LSB        (360)
#define SELF_TEST_2G_MIN_LSB        (17)
#define TESTLIMIT_XY                            (175)
#define TESTLIMIT_Z_USL_LSB             (1270)
#define TESTLIMIT_Z_LSL_LSB             (778)

#define READ_DATA_CHECK_CNT         (20)

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id k2dh_i2c_id[] = {{K2DH_ACC_DEV_NAME,0},{}};
static struct i2c_board_info __initdata k2dh_i2c_info ={ I2C_BOARD_INFO(K2DH_ACC_DEV_NAME, K2DH_ACC_I2C_SAD_L)};

/*----------------------------------------------------------------------------*/
static int k2dh_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int k2dh_i2c_remove(struct i2c_client *client);

/*----------------------------------------------------------------------------*/
typedef enum {
    K2DH_TRC_FILTER  = 0x01,
    K2DH_TRC_RAWDATA = 0x02,
    K2DH_TRC_IOCTL   = 0x04,
    K2DH_TRC_CALI    = 0X08,
    K2DH_TRC_INFO    = 0X10,
} K2DH_TRC;
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
#ifdef CONFIG_K2DH_LOWPASS
#define C_MAX_FIR_LENGTH (32)

struct data_filter {
    s16 raw[C_MAX_FIR_LENGTH][K2DH_AXES_NUM];
    int sum[K2DH_AXES_NUM];
    int num;
    int idx;
};
#endif
/*----------------------------------------------------------------------------*/
struct k2dh_i2c_data {
    struct i2c_client *client;
    struct acc_hw *hw;
    struct hwmsen_convert   cvt;

    /*misc*/
    struct data_resolution *reso;
    atomic_t                trace;
    atomic_t                suspend;
    atomic_t                selftest_rslt;
    atomic_t                filter;
    s16                     cali_sw[K2DH_AXES_NUM+1];

    /*data*/
    //s8                      offset[K2DH_AXES_NUM+1];  /*+1: for 4-byte alignment*/
    s16                     data[K2DH_AXES_NUM+1];

#if defined(CONFIG_K2DH_LOWPASS)
    atomic_t                firlen;
    atomic_t                fir_en;
    struct data_filter      fir;
#endif
    /*early suspend*/
#if defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend    early_drv;
#endif

#ifdef K2DH_ACCEL_CALIBRATION
    atomic_t fast_calib_x_rslt;
    atomic_t fast_calib_y_rslt;
    atomic_t fast_calib_z_rslt;
    atomic_t fast_calib_rslt;
#endif
};
/*----------------------------------------------------------------------------*/
static struct i2c_driver k2dh_i2c_driver = {
    .driver = {
        .name           = K2DH_ACC_DEV_NAME,
    },
    .probe              = k2dh_i2c_probe,
    .remove                = k2dh_i2c_remove,
#if !defined(CONFIG_HAS_EARLYSUSPEND)
    .suspend            = k2dh_suspend,
    .resume             = k2dh_resume,
#endif
    .id_table = k2dh_i2c_id,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *k2dh_i2c_client = NULL;
static struct platform_driver k2dh_gsensor_driver;
static struct k2dh_i2c_data *obj_i2c_data = NULL;
static bool sensor_power = true;
static int test_status = 0;
static UINT32 data_read_count = 0;
static GSENSOR_VECTOR3D gsensor_gain;

/*----------------------------------------------------------------------------*/
#define GSE_TAG                  "[Gsensor] "
#define GSE_FUN(f)               printk(KERN_ERR GSE_TAG"%s()\n", __FUNCTION__)
#define GSE_ERR(fmt, args...)    printk(KERN_ERR GSE_TAG"[ERROR] %s() line=%d : "fmt, __FUNCTION__, __LINE__, ##args)
#define GSE_LOG(fmt, args...)    printk(KERN_ERR GSE_TAG fmt, ##args)
/*----------------------------------------------------------------------------*/
static struct data_resolution K2DH_data_resolution[1] = {
 /* combination by {FULL_RES,RANGE}*/
    {{ 1, 0}, 1024},   // dataformat +/-2g  in 12-bit resolution;  { 1, 0} = 1.0= (2*2*1000)/(2^12);  1024 = (2^12)/(2*2)
};
/*--------------------K2DH power control function----------------------------------*/
static void k2dh_power(struct acc_hw *hw, unsigned int on)
{
    static unsigned int power_on = 0;

    //if(hw->power_id != POWER_NONE_MACRO)        // have externel LDO
    {
        GSE_LOG("power %s\n", on ? "on" : "off");
        if(power_on == on)    // power status not change
        {
            GSE_LOG("ignore power control: %d\n", on);
        }
        else if(on)    // power on
        {
            if(!(hw->power(hw, true, "K2DH")))
            {
                GSE_ERR("power on fails!!\n");
            }
        }
        else    // power off
        {
            if(!(hw->power(hw, false, "K2DH")))
            {
                GSE_ERR("power off fail!!\n");
            }
        }
    }
    power_on = on;
}

/*----------------------------------------------------------------------------*/
static int k2dh_SetDataResolution(struct k2dh_i2c_data *obj)
{
/*set g sensor dataresolution here*/
/*end of set dataresolution*/

    obj->reso = &K2DH_data_resolution[0];
    return 0;

/*if you changed the measure range, for example call: K2DH_SetDataFormat(client, K2DH_ACC_G_4G),
you must set the right value to K2DH_data_resolution*/
}

/*----------------------------------------------------------------------------*/
static int k2dh_ResetCalibration(struct i2c_client *client)
{
    struct k2dh_i2c_data *obj = i2c_get_clientdata(client);
    u8 ofs[4]={0,0,0,0};
    int err = 0;

    memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
        //memset(obj->offset, 0x00, sizeof(obj->offset));

    return err;
}

static int k2dh_acc_i2c_read(struct i2c_client *client, u8 * buf, int len)
{
    int err = -1;
    int tries = 0;

    struct i2c_msg    msgs[] = {
        {
            .addr = client->addr,
            .flags = client->flags & I2C_M_TEN,
            .len = 1,
            .buf = buf, },
        {
            .addr = client->addr,
            .flags = (client->flags & I2C_M_TEN) | I2C_M_RD,
            .len = len,
            .buf = buf, },
    };

#if 1 // SENSOR_I2C_FIX
	client->adapter->retries = I2C_RETRIES;
	err = i2c_transfer(client->adapter, msgs, 2);
#else
    do {
        err = i2c_transfer(client->adapter, msgs, 2);
        if (err != 2)
            msleep_interruptible(I2C_RETRY_DELAY);
    } while ((err != 2) && (++tries < I2C_RETRIES));
#endif

    if (err != 2) {
        GSE_ERR("read transfer error: %d\n", err);
        err = -EIO;
    } else {
        err = 0;
    }

    return err;
}

static int k2dh_acc_i2c_write(struct i2c_client *client, u8 * buf, int len)
{
    int err = -1;
    int tries = 0;

    struct i2c_msg msgs[] = { { .addr = client->addr,
            .flags = client->flags & I2C_M_TEN,
            .len = len + 1, .buf = buf, }, };

#if 1 // SENSOR_I2C_FIX
	client->adapter->retries = I2C_RETRIES;
	err = i2c_transfer(client->adapter, msgs, 1);
#else
    do {
        err = i2c_transfer(client->adapter, msgs, 1);
        if (err != 1)
            msleep_interruptible(I2C_RETRY_DELAY);
    } while ((err != 1) && (++tries < I2C_RETRIES));
#endif

    if (err != 1) {
        GSE_ERR("write transfer error: %d\n", err);
        err = -EIO;
    } else {
        err = 0;
    }

    return err;
}

static int k2dh_acc_register_write(struct i2c_client *client, u8 *buf, u8 reg_address, u8 new_value)
{
    int err = -1;

    buf[0] = reg_address;
    buf[1] = new_value;

    err = k2dh_acc_i2c_write(client, buf, 1);
    if(err != K2DH_SUCCESS)
    {
        return err;
    }
    return err;
}

static int k2dh_acc_register_read(struct i2c_client *client, u8 *buf, u8 reg_address)
{
    int err = -1;
    buf[0] = (reg_address);
    err = k2dh_acc_i2c_read(client, buf, 1);
    return err;
}

static int k2dh_acc_register_update(struct i2c_client *client, u8 *buf, u8 reg_address, u8 mask, u8 new_bit_values)
{
    int err = -1;
    u8 init_val;
    u8 updated_val;

    err = k2dh_acc_register_read(client, buf, reg_address);
    if(err == K2DH_SUCCESS)
    {
        init_val = buf[0];
        updated_val = ((mask & new_bit_values) | ((~mask) & init_val));
        err = k2dh_acc_register_write(client, buf, reg_address, updated_val);
    }
    return err;
}

static int k2dh_CheckDeviceID(struct i2c_client *client)
{
    u8 databuf[2] = {0,0};
    int res = -1;

    GSE_FUN();

    memset(databuf, 0, sizeof(u8)*2);
    databuf[0] = WHO_AM_I;

    res = k2dh_acc_i2c_read(client, databuf, 1);
    if(res != K2DH_SUCCESS)
    {
        res = k2dh_acc_i2c_read(client, databuf, 1);
        if(res != K2DH_SUCCESS)
        {
            goto exit_k2dh_CheckDeviceID;
        }
    }

    //udelay(500);

    if(databuf[0] != WHOAMI_K2DH_ACC)
    {
        GSE_ERR("device unknown. Expected: 0x%x, Replies: 0x%x\n", WHOAMI_K2DH_ACC, databuf[0]);
        return K2DH_ERR_IDENTIFICATION;
    }
    else
    {
        GSE_LOG("k2dh_CheckDeviceID %d pass! \n", databuf[0]);
    }

    exit_k2dh_CheckDeviceID:
    if(res != K2DH_SUCCESS)
    {
        return K2DH_ERR_I2C;
    }

    return K2DH_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int k2dh_SetBWRate(struct i2c_client *client, u8 bwrate)
{  // bwrate : ODRxxx
    int res = -1;
    u8 databuf[2] = {0,0};

    u8 new_val = bwrate | K2DH_ACC_ENABLE_ALL_AXES;
    u8 mask = K2DH_ODR_MSK | K2DH_ACC_ENABLE_ALL_AXES_MSK;

    res = k2dh_acc_register_update(client, databuf, CTRL_REG1, mask, new_val);
    if(res != K2DH_SUCCESS)
    {
        GSE_ERR("update odr failed, 0x%x, 0x%x : %d\n", databuf[0], databuf[1], res);
        return K2DH_ERR_I2C;
    }
    else
    {
        GSE_LOG("Set BWrate : 0x%02x\n", bwrate);
    }

    return K2DH_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int k2dh_SetPowerMode(struct i2c_client *client, bool enable)
{  // power mode controled by operating Bandwidth(ODR-Hz)
    GSE_FUN();

    int res = -1;
    struct k2dh_i2c_data *obj = i2c_get_clientdata(client);

    if(enable == sensor_power )
    {
        GSE_LOG("Sensor power status is newest!\n");
        return K2DH_SUCCESS;
    }

    if(enable == TRUE)
    {
        res = k2dh_SetBWRate(client, ODR200);  // operating mode as 200HZ
    }
    else
    {
        res = k2dh_SetBWRate(client, ODR_POWERDOWN_MODE);  // power down mode
    }

    if(res != K2DH_SUCCESS)
    {
        GSE_LOG("set power mode failed!\n");
        return K2DH_ERR_I2C;
    }
    else
    {
        GSE_LOG("set power mode ok: ");
    }

    sensor_power = enable;
    test_status = sensor_power;

    //mdelay(20);

    return K2DH_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int k2dh_SetDataFormat(struct i2c_client *client, u8 dataformat)
{  // dataformat : K2DH_ACC_G_2G . K2DH_ACC_G_4G. K2DH_ACC_G_8G. K2DH_ACC_G_16G

    int res = -1;

    u8 buf[2] = {0,0};
    u8 updated_val = 0;
    u8 init_val = 0;
    u8 new_val = 0;
    u8 mask = K2DH_ACC_FS_MASK | HIGH_RESOLUTION;

    struct k2dh_i2c_data *obj = i2c_get_clientdata(client);


    buf[0] = CTRL_REG4;
    res = k2dh_acc_i2c_read(client, buf, 1);
    if(res != K2DH_SUCCESS)
    {
        goto error;
    }
    else
    {
        init_val = buf[0];
        new_val = dataformat | HIGH_RESOLUTION;
        updated_val = ((mask & new_val) | ((~mask) & init_val));
        buf[1] = updated_val;
        buf[0] = CTRL_REG4;
        res = k2dh_acc_i2c_write(client, buf, 1);
        if(res != K2DH_SUCCESS)
        {
            goto error;
        }
        else
        {
            GSE_LOG("Set DataFormat: 0x%02x\n", dataformat);
        }
    }

    return k2dh_SetDataResolution(obj);

error:
    GSE_ERR("update g range failed 0x%x, 0x%x: %d\n", buf[0], buf[1], res);

    return res;
}

/*----------------------------------------------------------------------------*/
static int k2dh_SetIntEnable(struct i2c_client *client, u8 intenable)
{
    int res = -1;
    u8 databuf[2] = {0, 0};

    databuf[1] = 0x00;
    databuf[0] = INT_CFG1;

    res = k2dh_acc_i2c_read(client, databuf, 1);
    if(res != K2DH_SUCCESS)
    {
        GSE_ERR("Interrupt configuration failed, 0x%x, 0x%x : %d\n", databuf[0], databuf[1], res);
        return K2DH_ERR_I2C;
    }

    databuf[0] = INT_CFG2;
    res = k2dh_acc_i2c_read(client, databuf, 1);
    if(res != K2DH_SUCCESS)
    {
        GSE_ERR("Interrupt configuration failed, 0x%x, 0x%x : %d\n", databuf[0], databuf[1], res);
        return K2DH_ERR_I2C;
    }
    GSE_LOG("K2DH Interrupt was configured\n");

    return K2DH_SUCCESS;
}


static int k2dh_init_client(struct i2c_client *client)
{
    int err = -1;
    u8 buf[7] = {0};

    buf[0] = CTRL_REG1;
    buf[1] = K2DH_ACC_ENABLE_ALL_AXES;
    err = k2dh_acc_i2c_write(client, buf, 1);
    if (err < 0)
        goto error;

    buf[0] = TEMP_CFG_REG;
    buf[1] = 0x00;
    err = k2dh_acc_i2c_write(client, buf, 1);
    if (err < 0)
        goto error;

    buf[0] = FIFO_CTRL_REG;
    buf[1] = 0x00;
    err = k2dh_acc_i2c_write(client, buf, 1);
    if (err < 0)
        goto error;

    buf[0] = (I2C_AUTO_INCREMENT | TT_THS);
    buf[1] = 0x00;
    buf[2] = 0x00;
    buf[3] = 0x00;
    buf[4] = 0x00;
    err = k2dh_acc_i2c_write(client, buf, 4);
    if (err < 0)
        goto error;
    buf[0] = TT_CFG;
    buf[1] = 0x00;
    err = k2dh_acc_i2c_write(client, buf, 1);
    if (err < 0)
        goto error;

    buf[0] = (I2C_AUTO_INCREMENT | INT_THS1);
    buf[1] = 0x00;
    buf[2] = 0x00;
    err = k2dh_acc_i2c_write(client, buf, 2);
    if (err < 0)
        goto error;
    buf[0] = INT_CFG1;
    buf[1] = 0x00;
    err = k2dh_acc_i2c_write(client, buf, 1);
    if (err < 0)
        goto error;

    buf[0] = (I2C_AUTO_INCREMENT | INT_THS2);
    buf[1] = 0x00;
    buf[2] = 0x00;
    err = k2dh_acc_i2c_write(client, buf, 2);
    if (err < 0)
        goto error;
    buf[0] = INT_CFG2;
    buf[1] = 0x00;
    err = k2dh_acc_i2c_write(client, buf, 1);
    if (err < 0)
        goto error;

    buf[0] = (I2C_AUTO_INCREMENT | CTRL_REG2);
    buf[1] = 0x00;
    buf[2] = 0x00;
    buf[3] = 0x00;
    buf[4] = 0x00;
    buf[5] = 0x00;
    err = k2dh_acc_i2c_write(client, buf, 5);
    if (err < 0)
        goto error;

        GSE_LOG("k2dh_init_client : hw init done\n");
        return K2DH_SUCCESS;

error:
    GSE_ERR("hw init error 0x%x, 0x%x: %d\n", buf[0], buf[1], err);

    return err;
}

static int k2dh_acc_hw_init(struct i2c_client *client, int reset_cali)
{
    int res = -1;
    struct k2dh_i2c_data *obj = i2c_get_clientdata(client);

    GSE_FUN();

    res = k2dh_CheckDeviceID(client);
    if(res != K2DH_SUCCESS)
    {
            return res;
    }

    res = k2dh_init_client(client);
    if(res != K2DH_SUCCESS)
    {
        return res;
    }

    res = k2dh_SetBWRate(client, ODR200);
    if(res != K2DH_SUCCESS )
    {
        return res;
    }

    res = k2dh_SetDataFormat(client, K2DH_ACC_G_2G);
    if(res != K2DH_SUCCESS)
    {
        return res;
    }

    gsensor_gain.x = gsensor_gain.y = gsensor_gain.z = obj->reso->sensitivity;

    res = k2dh_SetIntEnable(client, 0x00);  // interrupt disable
    if(res != K2DH_SUCCESS)
    {
        return res;
    }

    res = k2dh_SetPowerMode(client, false);
    if(res != K2DH_SUCCESS)
    {
        return res;
    }

    if(0 != reset_cali)
    {
        /*reset calibration only in power on*/
        res = k2dh_ResetCalibration(client);
        if(res != K2DH_SUCCESS)
        {
            return res;
        }
    }

#ifdef CONFIG_K2DH_LOWPASS
    memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif

    //mdelay(20);

    return K2DH_SUCCESS;
}
/*----------------------------------------------------------------------------*/
static int k2dh_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
{
    u8 databuf[10] = {0};

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

    sprintf(buf, "K2DH Chip");
    return 0;
}
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
#if 1
static s16 prev_data[K2DH_AXES_NUM] = {0,0,0};
static s16 check_cnt = 0;
#endif

static int k2dh_ReadData(struct i2c_client *client, s16 data[K2DH_AXES_NUM])
{
    struct k2dh_i2c_data *priv = i2c_get_clientdata(client);
    u8 acc_data[K2DH_DATA_LEN] = {0};
    int err = -1;

    acc_data[0] = (I2C_AUTO_INCREMENT | AXISDATA_REG);

    if(NULL == client)
    {
        err = -EINVAL;
    }
    else if((err = k2dh_acc_i2c_read(client, acc_data, K2DH_DATA_LEN)) != K2DH_SUCCESS)
    {
        GSE_ERR("I2C read error: %d\n", err);
    }
    else
    {
        /* Convert sensor raw data to 16-bit integer */
        data[K2DH_AXIS_X] = (((s16) ((acc_data[1] << 8) | acc_data[0])) >> 4);
        data[K2DH_AXIS_Y] = (((s16) ((acc_data[3] << 8) | acc_data[2])) >> 4);
        data[K2DH_AXIS_Z] = (((s16) ((acc_data[5] << 8) | acc_data[4])) >> 4);

#if 1
        if((prev_data[K2DH_AXIS_X] == data[K2DH_AXIS_X]) && (prev_data[K2DH_AXIS_Y] == data[K2DH_AXIS_Y]) && (prev_data[K2DH_AXIS_Z] == data[K2DH_AXIS_Z]))
        {
            check_cnt++;
            if(check_cnt >= READ_DATA_CHECK_CNT)  // read data same case
            {
                GSE_ERR("READ DATA SAME CASE : (%d %d %d)\n", data[K2DH_AXIS_X], data[K2DH_AXIS_Y],data[K2DH_AXIS_Z]);

                // Try recovery the malfunction by change Power Down mode and ODR200
                k2dh_SetBWRate(client, ODR_POWERDOWN_MODE);  // power down mode
                msleep(100);                
                k2dh_SetBWRate(client, ODR200);  // operating mode as 200HZ                
                check_cnt = 0;
            }
        }
        else
        {
            prev_data[K2DH_AXIS_X] =data[K2DH_AXIS_X];
            prev_data[K2DH_AXIS_Y] =data[K2DH_AXIS_Y];
            prev_data[K2DH_AXIS_Z] =data[K2DH_AXIS_Z];
            check_cnt = 0;            
        }
#endif

        if(atomic_read(&priv->trace) & K2DH_TRC_RAWDATA)
        {
            GSE_LOG("gsensor raw data: x=%d, y=%d, z=%d!\n", data[K2DH_AXIS_X], data[K2DH_AXIS_Y], data[K2DH_AXIS_Z]);
        }
#ifdef CONFIG_K2DH_LOWPASS
        if(atomic_read(&priv->filter))
        {
            if(atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend))
            {
                int idx, firlen = atomic_read(&priv->firlen);
                if(priv->fir.num < firlen)
                {
                    priv->fir.raw[priv->fir.num][K2DH_AXIS_X] = data[K2DH_AXIS_X];
                    priv->fir.raw[priv->fir.num][K2DH_AXIS_Y] = data[K2DH_AXIS_Y];
                    priv->fir.raw[priv->fir.num][K2DH_AXIS_Z] = data[K2DH_AXIS_Z];
                    priv->fir.sum[K2DH_AXIS_X] += data[K2DH_AXIS_X];
                    priv->fir.sum[K2DH_AXIS_Y] += data[K2DH_AXIS_Y];
                    priv->fir.sum[K2DH_AXIS_Z] += data[K2DH_AXIS_Z];
                    if(atomic_read(&priv->trace) & K2DH_TRC_FILTER)
                    {
                        GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d]\n", priv->fir.num,
                            priv->fir.raw[priv->fir.num][K2DH_AXIS_X], priv->fir.raw[priv->fir.num][K2DH_AXIS_Y], priv->fir.raw[priv->fir.num][K2DH_AXIS_Z],
                            priv->fir.sum[K2DH_AXIS_X], priv->fir.sum[K2DH_AXIS_Y], priv->fir.sum[K2DH_AXIS_Z]);
                    }
                    priv->fir.num++;
                    priv->fir.idx++;
                }
                else
                {
                    idx = priv->fir.idx % firlen;
                    priv->fir.sum[K2DH_AXIS_X] -= priv->fir.raw[idx][K2DH_AXIS_X];
                    priv->fir.sum[K2DH_AXIS_Y] -= priv->fir.raw[idx][K2DH_AXIS_Y];
                    priv->fir.sum[K2DH_AXIS_Z] -= priv->fir.raw[idx][K2DH_AXIS_Z];
                    priv->fir.raw[idx][K2DH_AXIS_X] = data[K2DH_AXIS_X];
                    priv->fir.raw[idx][K2DH_AXIS_Y] = data[K2DH_AXIS_Y];
                    priv->fir.raw[idx][K2DH_AXIS_Z] = data[K2DH_AXIS_Z];
                    priv->fir.sum[K2DH_AXIS_X] += data[K2DH_AXIS_X];
                    priv->fir.sum[K2DH_AXIS_Y] += data[K2DH_AXIS_Y];
                    priv->fir.sum[K2DH_AXIS_Z] += data[K2DH_AXIS_Z];
                    priv->fir.idx++;
                    data[K2DH_AXIS_X] = priv->fir.sum[K2DH_AXIS_X]/firlen;
                    data[K2DH_AXIS_Y] = priv->fir.sum[K2DH_AXIS_Y]/firlen;
                    data[K2DH_AXIS_Z] = priv->fir.sum[K2DH_AXIS_Z]/firlen;
                    if(atomic_read(&priv->trace) & K2DH_TRC_FILTER)
                    {
                        GSE_LOG("add [%2d] [%5d %5d %5d] => [%5d %5d %5d] : [%5d %5d %5d]\n", idx,
                        priv->fir.raw[idx][K2DH_AXIS_X], priv->fir.raw[idx][K2DH_AXIS_Y], priv->fir.raw[idx][K2DH_AXIS_Z],
                        priv->fir.sum[K2DH_AXIS_X], priv->fir.sum[K2DH_AXIS_Y], priv->fir.sum[K2DH_AXIS_Z],
                        data[K2DH_AXIS_X], data[K2DH_AXIS_Y], data[K2DH_AXIS_Z]);
                    }
                }
            }
        }
#endif
    }
    return err;
}
 /*----------------------------------------------------------------------------*/
static int k2dh_ReadSensorData(struct i2c_client *client, char *buf, int bufsize)
{
    struct k2dh_i2c_data *obj = (struct k2dh_i2c_data*)i2c_get_clientdata(client);
    u8 databuf[20] = {0};
    int acc[K2DH_AXES_NUM] = {0};
    int res = -1;
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
        res = k2dh_SetPowerMode(client, true);
        if(res)
        {
            GSE_ERR("Power on K2DH error %d!\n", res);
        }
    }

    if(res = k2dh_ReadData(client, obj->data))
    {
        GSE_ERR("I2C error: ret value=%d", res);
        return -3;
    }
    else
    {
        data_read_count++;
        if(data_read_count >= 2147483647)
        {
            data_read_count =0;
        }
        
        obj->data[K2DH_AXIS_X] -= obj->cali_sw[K2DH_AXIS_X];
        obj->data[K2DH_AXIS_Y] -= obj->cali_sw[K2DH_AXIS_Y];
        obj->data[K2DH_AXIS_Z] -= obj->cali_sw[K2DH_AXIS_Z];

        //printk("cali_sw x=%d, y=%d, z=%d \n",obj->cali_sw[K2DH_AXIS_X],obj->cali_sw[K2DH_AXIS_Y],obj->cali_sw[K2DH_AXIS_Z]);

        /*remap coordinate*/
        acc[obj->cvt.map[K2DH_AXIS_X]] = obj->cvt.sign[K2DH_AXIS_X]*obj->data[K2DH_AXIS_X];
        acc[obj->cvt.map[K2DH_AXIS_Y]] = obj->cvt.sign[K2DH_AXIS_Y]*obj->data[K2DH_AXIS_Y];
        acc[obj->cvt.map[K2DH_AXIS_Z]] = obj->cvt.sign[K2DH_AXIS_Z]*obj->data[K2DH_AXIS_Z];
        //printk("cvt x=%d, y=%d, z=%d \n",obj->cvt.sign[K2DH_AXIS_X],obj->cvt.sign[K2DH_AXIS_Y],obj->cvt.sign[K2DH_AXIS_Z]);

        if(atomic_read(&obj->trace) & K2DH_TRC_RAWDATA)
        {
            //GSE_LOG("Mapped gsensor data: x=%d, y=%d, z=%d!\n", acc[K2DH_AXIS_X], acc[K2DH_AXIS_Y], acc[K2DH_AXIS_Z]);
        }

        //Out put the mg
        //printk("mg acc=%d, GRAVITY=%d, sensityvity=%d \n",acc[K2DH_AXIS_X],GRAVITY_EARTH_1000,obj->reso->sensitivity);
        acc[K2DH_AXIS_X] = acc[K2DH_AXIS_X] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
        acc[K2DH_AXIS_Y] = acc[K2DH_AXIS_Y] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;
        acc[K2DH_AXIS_Z] = acc[K2DH_AXIS_Z] * GRAVITY_EARTH_1000 / obj->reso->sensitivity;

        sprintf(buf, "%04x %04x %04x %x", acc[K2DH_AXIS_X], acc[K2DH_AXIS_Y], acc[K2DH_AXIS_Z], data_read_count);
        if(atomic_read(&obj->trace) & K2DH_TRC_IOCTL)
        {
            GSE_LOG("gsensor data: x=%d, y=%d, z=%d!\n", acc[K2DH_AXIS_X], acc[K2DH_AXIS_Y], acc[K2DH_AXIS_Z]);
        }
    }

    return 0;
}

static int k2dh_read_accel_raw_data(struct k2dh_acc *acc)
{
        int err;
        unsigned char acc_data[6] = {0};
        s16 temp;

        acc_data[0] = (I2C_AUTO_INCREMENT | AXISDATA_REG);
}

static int k2dh_do_calibration(void)
{
        struct K2DHacc acc_cal = {0, };
        struct K2DHacc acc_cal_pre = {0, };
        int sum[3] = {0, };
        int err = 0;
        int i;

        struct i2c_client *client = k2dh_i2c_client;
        struct k2dh_i2c_data *obj = i2c_get_clientdata(client);

        err = k2dh_ReadData(client, &acc_cal_pre);
        if(err < 0)
        {
            GSE_ERR("%s : k2dh_ReadData() failed\n", __func__);
            return err;
        }

        for(i = 0; i < CALIBRATION_DATA_AMOUNT; i++)
        {
            mdelay(20);
            err = k2dh_ReadData(client, &acc_cal);
            if(err < 0)
            {
                GSE_ERR("%s : k2dh_ReadData() failed in the %dth loop\n", __func__, i);
                return err;
            }

            GSE_LOG(KERN_INFO "===============moved x=============== timeout = %d\n", i);
            GSE_LOG(KERN_INFO "(%d, %d, %d) (%d, %d, %d)\n", acc_cal_pre.x, acc_cal_pre.y,  acc_cal_pre.z, acc_cal.x,acc_cal.y,acc_cal.z );

            if((abs(acc_cal.x - acc_cal_pre.x) > K2DH_SHAKING_DETECT_THRESHOLD)
                || (abs((acc_cal.y - acc_cal_pre.y)) > K2DH_SHAKING_DETECT_THRESHOLD)
                || (abs((acc_cal.z - acc_cal_pre.z)) > K2DH_SHAKING_DETECT_THRESHOLD))
            {
                atomic_set(&obj->fast_calib_rslt, 0);
                GSE_LOG(KERN_INFO "===============shaking x===============\n");
                return -EINVAL;                
            }
            else
            {
                sum[K2DH_AXIS_X] += acc_cal.x;
                sum[K2DH_AXIS_Y] += acc_cal.y;
                sum[K2DH_AXIS_Z] += acc_cal.z;

                acc_cal_pre.x = acc_cal.x;
                acc_cal_pre.y = acc_cal.y;
                acc_cal_pre.z = acc_cal.z;
            }

            GSE_LOG("calibration sum data (%d, %d, %d)\n", sum[K2DH_AXIS_X], sum[K2DH_AXIS_Y], sum[K2DH_AXIS_Z]);            
            GSE_LOG(KERN_INFO "===============timeout_shaking: %d=============== \n",i);            
        }

        GSE_LOG(KERN_INFO "===============complete shaking x check===============\n");        

#if 1
        // check zero-g offset
        if((abs(sum[K2DH_AXIS_X]/CALIBRATION_DATA_AMOUNT) >TESTLIMIT_XY) ||
            (abs(sum[K2DH_AXIS_Y]/CALIBRATION_DATA_AMOUNT) >TESTLIMIT_XY) ||
            ((abs(sum[K2DH_AXIS_Z]/CALIBRATION_DATA_AMOUNT) > TESTLIMIT_Z_USL_LSB) || (abs(sum[K2DH_AXIS_Z]/CALIBRATION_DATA_AMOUNT) < TESTLIMIT_Z_LSL_LSB)))
        {
            GSE_LOG("Calibration zero-g offset check failed (%d, %d, %d)\n", sum[K2DH_AXIS_X]/CALIBRATION_DATA_AMOUNT, 
                                                                    sum[K2DH_AXIS_Y]/CALIBRATION_DATA_AMOUNT, sum[K2DH_AXIS_Z]/CALIBRATION_DATA_AMOUNT);
             atomic_set(&obj->fast_calib_rslt, 0);
             return -EINVAL;                            
        }        
#endif

        obj->cali_sw[K2DH_AXIS_X] = sum[K2DH_AXIS_X] / CALIBRATION_DATA_AMOUNT;  //K2DH(12bit) 0+-154
        obj->cali_sw[K2DH_AXIS_Y] = sum[K2DH_AXIS_Y] / CALIBRATION_DATA_AMOUNT;  //K2DH(12bit) 0+-154

        if(sum[K2DH_AXIS_Z] < 0)
        {
            obj->cali_sw[K2DH_AXIS_Z] = (sum[K2DH_AXIS_Z] / CALIBRATION_DATA_AMOUNT) + 1024;        // K2DH(12bit) 1024 +- 226
        }
        else
        {
            obj->cali_sw[K2DH_AXIS_Z] = (sum[K2DH_AXIS_Z] / CALIBRATION_DATA_AMOUNT) - 1024;        // K2DH(12bit) 1024 +- 226
        }

        GSE_LOG("%s : calibration data (%d, %d, %d)\n", __func__, obj->cali_sw[K2DH_AXIS_X], obj->cali_sw[K2DH_AXIS_Y], obj->cali_sw[K2DH_AXIS_Z]);

        return err;
}

#if 1
static int k2dh_get_selftest(struct i2c_client *client)
{
    int val, i;
    int en_state = 0;

    unsigned char x[8];
    int NO_ST[3] = {0, 0, 0};
    int NO_ST_ZOFF[3] = {0, 0, 0};
    int ST[3] = {0, 0, 0};
    s16 tmp = 0;

    struct k2dh_i2c_data *obj = i2c_get_clientdata(client); 

    // ODR setting
    x[0] = CTRL_REG1;
    x[1] = 0x67;  // 0x47
    k2dh_acc_i2c_write(client, x, 1);

    x[0] = (I2C_AUTO_INCREMENT | CTRL_REG4);
    x[1] = 0x88;
    x[2] = 0x00;
    x[3] = 0x00;
    k2dh_acc_i2c_write(client, x, 3);

    mdelay(80);

    x[0] = (I2C_AUTO_INCREMENT | AXISDATA_REG);
    k2dh_acc_i2c_read(client, x, 6);

    for(i = 0; i < 5; i++)
    {
        while(1)
        {
            x[0] = 0x27;  // status_reg
            val = k2dh_acc_i2c_read(client, x, 1);
            if(val < 0)
            {
                GSE_ERR("[SELFTEST] I2C fail point1\n");
                goto ST_EXIT;
            }
            if(x[0] & 0x08)  // x,y,z asix new data Available
            {
                break;
            }
        }

        x[0] = (I2C_AUTO_INCREMENT | AXISDATA_REG);
        k2dh_acc_i2c_read(client, x, 6);

        NO_ST_ZOFF[0] += (((s16) ((x[1] << 8) | x[0])) >> 4); // 12 bit resolution 1LSB= 0.997mg
        NO_ST_ZOFF[1] += (((s16) ((x[3] << 8) | x[2])) >> 4);
        NO_ST_ZOFF[2] += (((s16) ((x[5] << 8) | x[4])) >> 4);
    
        NO_ST[0] += (((s16) ((x[1] << 8) | x[0])) >> 6); //10 bit resolution 1LSB=4mg
        NO_ST[1] += (((s16) ((x[3] << 8) | x[2])) >> 6);
        NO_ST[2] += (((s16) ((x[5] << 8) | x[4])) >> 6);

        if(atomic_read(&obj->trace) & K2DH_TRC_CALI)
        {
            GSE_LOG("[SELFTEST] NO_ST(%d) : %d, %d, %d\n", i, NO_ST[0], NO_ST[1], NO_ST[2]);
        }
    }

    NO_ST_ZOFF[0] /= 5;
    NO_ST_ZOFF[1] /= 5;
    NO_ST_ZOFF[2] /= 5;

    NO_ST[0] /= 5;
    NO_ST[1] /= 5;
    NO_ST[2] /= 5;

    //if(atomic_read(&obj->trace) & K2DH_TRC_CALI)
    {
        GSE_LOG("[SELFTEST] AVE_NO_ST : %d, %d, %d\n", NO_ST[0], NO_ST[1], NO_ST[2]);
    }

    x[0] = CTRL_REG4;
    x[1] = 0x8A; // ST enable
    k2dh_acc_i2c_write(client, x, 1);

    mdelay(80);

    x[0] = (I2C_AUTO_INCREMENT | AXISDATA_REG);
    k2dh_acc_i2c_read(client, x, 6);

    for(i = 0; i < 5; i++)
    {
        while(1)
        {
            x[0] = 0x27;
            val = k2dh_acc_i2c_read(client, x, 1);

            if(val < 0)
            {
                GSE_ERR("[SELFTEST] I2C fail point2\n");
                goto ST_EXIT;
            }

            if(x[0] & 0x08)
            {
                break;
            }
        }

        x[0] = (I2C_AUTO_INCREMENT | AXISDATA_REG);
        k2dh_acc_i2c_read(client, x, 6);

        ST[0] += (((s16) ((x[1] << 8) | x[0])) >> 6);
        ST[1] += (((s16) ((x[3] << 8) | x[2])) >> 6);
        ST[2] += (((s16) ((x[5] << 8) | x[4])) >> 6);

        if(atomic_read(&obj->trace) & K2DH_TRC_CALI)
        {
            GSE_LOG("[SELFTEST] ST(%d) : %d, %d, %d\n", i, ST[0], ST[1], ST[2]);
        }        
    }
    ST[0] /= 5;
    ST[1] /= 5;
    ST[2] /= 5;    
    //if(atomic_read(&obj->trace) & K2DH_TRC_CALI)
    {
        GSE_LOG("[SELFTEST] AVE_ST : %d, %d, %d\n", ST[0], ST[1], ST[2]);
    }

    for(val = 0, i = 0; i < 3; i++)
    {
        // calculate differece between SelfTest value and zoro g offset in 10bit resolution
        ST[i] -= NO_ST[i];
        ST[i] = abs(ST[i]);

        // range compare of the self test
        if((SELF_TEST_2G_MIN_LSB > ST[i]) || (ST[i] > SELF_TEST_2G_MAX_LSB))
        {
            GSE_ERR("[SELFTEST] ST[%d] : Out of range!! (%d)\n", i, ST[i]);
            val = -1;
        }
    }

    // check zero-g offset
    if(val >= 0)
    {
        for(val = 1, i = 0; i < 3; i++)
        {
            if(i < 2)
            {
                if(abs(NO_ST_ZOFF[i]) > TESTLIMIT_XY)  // X, Y axis flat check
                {
                    GSE_ERR("[SELFTEST] NO_ST[%d] : Out of ZOffset!! (%d)\n", i, NO_ST_ZOFF[i]);
                    val = -1;
                }
            }
            else
            {  // Z axis flat check
                if((abs(NO_ST_ZOFF[i]) > TESTLIMIT_Z_USL_LSB) || (abs(NO_ST_ZOFF[i]) < TESTLIMIT_Z_LSL_LSB))
                {
                    GSE_ERR("[SELFTEST] NO_ST[%d] : Out of ZOffset!! (%d)\n", i, NO_ST[i]);
                    val = -1;
                }
            }
        }
    }

    //if(atomic_read(&obj->trace) & K2DH_TRC_CALI)
    {
        if(val >= 0)
        {
            GSE_LOG("[SELFTEST] OK!! val : %d, (%d, %d, %d) ||| (%d, %d, %d)\n", val, ST[0], ST[1], ST[2], NO_ST_ZOFF[0], NO_ST_ZOFF[1], NO_ST_ZOFF[2]);
        }
        else
        {
            GSE_LOG("[SELFTEST] NG!! val : %d, (%d, %d, %d) ||| (%d, %d, %d)\n", val, ST[0], ST[1], ST[2], NO_ST_ZOFF[0], NO_ST_ZOFF[1], NO_ST_ZOFF[2]);
        }   
    }            

ST_EXIT:
#if 0  // do not control ODR rate
    x[0] = CTRL_REG1;
    x[1] = 0x00;
    k2dh_acc_i2c_write(client, x, 1);
#endif

    x[0] = CTRL_REG4;
    x[1] = 0x08; // ST disable
    k2dh_acc_i2c_write(client, x, 1);


    x[0] = CTRL_REG5;
    x[1] = 0x88;
    k2dh_acc_i2c_write(client, x, 1);

    return val;    
}

#endif


static int k2dh_set_mode(struct i2c_client *client, unsigned char mode)
{
    int comres = -1;
    unsigned char data[2] ={0, 0};
    unsigned char LPen = 0;
    unsigned char HR = 0;

    if((client == NULL) || (mode >= 3))
    {
        return -1;
    }

    switch(mode)
    {
        case K2DH_MODE_LOWPOWER :
            LPen = 1;
            HR = 0;
            break;
        case K2DH_MODE_NORMAL:
            LPen = 0;
            HR =  0;
            break;
        case K2DH_MODE_HIGH_RESOLUTION :
            LPen = 0;
            HR = 1;
            break;
        default :
            break;
    }

    comres = k2dh_acc_register_update(client, (u8 *) data, K2DH_LP_EN_REG, K2DH_LP_EN_MSK, LPen << K2DH_LP_EN_POS);
    if(comres != K2DH_SUCCESS)
    {
        return comres;
    }
    else
    {
        comres = k2dh_acc_register_update(client, (u8 *) data, K2DH_HR_REG, K2DH_HR_MSK, HR << K2DH_HR_POS);
        if(comres != K2DH_SUCCESS)
        {
            return comres;
        }
    }
    GSE_LOG("k2dh_set_mode, mode : %d, LPen : %d, HR : %d\n", mode, LPen, HR);
    return K2DH_SUCCESS;
}

static int k2dh_get_mode(struct i2c_client *client, unsigned char *mode)
{
    int comres = -1;
    unsigned char data[2] ={0, 0};
    unsigned char LPen = 0;
    unsigned char HR = 0;

    comres = k2dh_acc_register_read(client, data, K2DH_LP_EN_REG);
    if(comres != K2DH_SUCCESS)
    {
        return comres;
    }
    else
    {
        LPen = (data[0]& K2DH_LP_EN_MSK)?1:0;
        comres = k2dh_acc_register_read(client, data, K2DH_HR_REG);
        if(comres != K2DH_SUCCESS)
        {
            return comres;
        }
        else
        {
            HR = (data[0]& K2DH_HR_MSK)?1:0;
        }

        if(LPen == 1 && HR == 0)
        {
            *mode = K2DH_MODE_LOWPOWER;
        }
        else if(LPen == 0 && HR == 0)
        {
            *mode = K2DH_MODE_NORMAL;
        }
        else if(LPen == 0 && HR == 1)
        {
            *mode = K2DH_MODE_HIGH_RESOLUTION;
        }
        else
        {
            *mode = -1;
        }
    }
    GSE_LOG("k2dh_get_mode, LPen : %d, HR : %d\n", LPen, HR);
    return comres;
}

static int k2dh_set_range(struct i2c_client *client, unsigned char range)
{
    int comres = -1;
    unsigned char data[2] ={0, 0};

    if((client == NULL) || (range >= 4))
    {
        return -1;
    }

    comres = k2dh_acc_register_update(client, (u8 *) data, K2DH_FS_REG, K2DH_FS_MSK, range << K2DH_FS_POS);
    if(comres != K2DH_SUCCESS)
    {
        return comres;
    }

    GSE_LOG("k2dh_set_range, range : %d\n", range);
    return K2DH_SUCCESS;

}
static int k2dh_get_range(struct i2c_client *client, unsigned char *range)
{
    int comres = -1;
    unsigned char data[2] ={0, 0};

    comres = k2dh_acc_register_read(client, data, K2DH_FS_REG);
    if(comres != K2DH_SUCCESS)
    {
        return comres;
    }
    data[0] = (data[0] & K2DH_FS_MSK) >> K2DH_FS_POS;
    *range = data[0];
    GSE_LOG("k2dh_get_range, range : %d\n", data[0]);
    return comres;
}
static int k2dh_set_bandwidth(struct i2c_client *client, unsigned char bandwidth)
{
    int comres = -1;
    unsigned char data[2] ={0, 0};

    if((client == NULL) || (bandwidth >= 10))
    {
        return -1;
    }

    comres = k2dh_acc_register_update(client, (u8 *) data, K2DH_ODR_REG, K2DH_ODR_MSK, bandwidth << K2DH_ODR_POS);
    if(comres != K2DH_SUCCESS)
    {
        return comres;
    }

    GSE_LOG("k2dh_set_bandwidth, bandwidth : %d\n", bandwidth);
    return K2DH_SUCCESS;
}
static int k2dh_get_bandwidth(struct i2c_client *client, unsigned char *bandwidth)
{
    int comres = -1;
    unsigned char data[2] ={0, 0};

    comres = k2dh_acc_register_read(client, data, K2DH_ODR_REG);
    if(comres != K2DH_SUCCESS)
    {
        return comres;
    }
    data[0] = (data[0] & K2DH_ODR_MSK) >> K2DH_ODR_POS;
    *bandwidth = data[0];
    GSE_LOG("k2dh_get_bandwidth, bandwidth : %d\n", data[0]);
    return comres;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
    struct i2c_client *client = k2dh_i2c_client;
    char strbuf[K2DH_BUFSIZE] = {0};
    if(NULL == client)
    {
        GSE_ERR("i2c client is null!!\n");
        return 0;
    }

    k2dh_ReadChipInfo(client, strbuf, K2DH_BUFSIZE);
    return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t gsensor_init(struct device_driver *ddri, char *buf, size_t count)
{
    struct i2c_client *client = k2dh_i2c_client;
    char strbuf[K2DH_BUFSIZE] = {0};

    if(NULL == client)
    {
        GSE_ERR("i2c client is null!!\n");
        return 0;
    }
    k2dh_acc_hw_init(client, 1);
    return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t show_opmode_value(struct device_driver *ddri, char *buf)
{
    unsigned char data = 0;

    if (k2dh_get_mode(k2dh_i2c_client, &data) < 0)
    {
        return sprintf(buf, "Read error\n");
    }
    else
    {
        if(data == 0)
            return sprintf(buf, "0:LP MODE\n");
        else if(data == 1)
            return sprintf(buf, "1:Normal MODE\n");
        else if(data == 2)
            return sprintf(buf, "2:HR MODE\n");
        else
            return sprintf(buf, "Error\n");
    }
}

static ssize_t store_opmode_value(struct device_driver *ddri, char *buf, size_t count)
{
    unsigned long data = 0;
    int error = -1;

    if (error = strict_strtoul(buf, 10, &data))
    {
        return error;
    }

    if (k2dh_set_mode(k2dh_i2c_client, (unsigned char) data) < 0)
    {
        GSE_ERR("invalid content: '%s', length = %d\n", buf, count);
    }

    return count;
}

static ssize_t show_range_value(struct device_driver *ddri, char *buf)
{
    unsigned char data = 0;

    if (k2dh_get_range(k2dh_i2c_client, &data) < 0)
    {
        return sprintf(buf, "Read error\n");
    }
    else
    {
        return sprintf(buf, "%d\n", data);
    }
}

static ssize_t store_range_value(struct device_driver *ddri, char *buf, size_t count)
{
    unsigned long data = 0;
    int error = -1;

    if (error = strict_strtoul(buf, 10, &data))
    {
        return error;
    }
    if (k2dh_set_range(k2dh_i2c_client, (unsigned char) data) < 0)
    {
        GSE_ERR("invalid content: '%s', length = %d\n", buf, count);
    }

    return count;
}

static ssize_t show_bandwidth_value(struct device_driver *ddri, char *buf)
{
    unsigned char data = 0;

    if (k2dh_get_bandwidth(k2dh_i2c_client, &data) < 0)
    {
        return sprintf(buf, "Read error\n");
    }
    else
    {
        return sprintf(buf, "%d\n", data);
    }
}

static ssize_t store_bandwidth_value(struct device_driver *ddri, char *buf, size_t count)
{
    unsigned long data = 0;
    int error = -1;

    if (error = strict_strtoul(buf, 10, &data))
    {
        return error;
    }
    if (k2dh_set_bandwidth(k2dh_i2c_client, (unsigned char) data) < 0)
    {
        GSE_ERR("invalid content: '%s', length = %d\n", buf, count);
    }

    return count;
}

static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
    struct i2c_client *client = k2dh_i2c_client;
    char strbuf[K2DH_BUFSIZE] = {0};;

    if(NULL == client)
    {
        GSE_ERR("i2c client is null!!\n");
        return 0;
    }
    k2dh_ReadSensorData(client, strbuf, K2DH_BUFSIZE);
    return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t show_sensorrawdata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = k2dh_i2c_client;
	s16 data[K2DH_AXES_NUM];

	if(NULL == client)
	{
		GSE_ERR("i2c client is null!!\n");
		return 0;
	}

	k2dh_ReadData(client, data);
	return snprintf(buf, PAGE_SIZE, "Read RawData : x=%04x, y=%04x, z=%04x\n", data[0], data[1], data[2]);
}



/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
    ssize_t res = -1;
    struct k2dh_i2c_data *obj = obj_i2c_data;
    if (obj == NULL)
    {
        GSE_ERR("i2c_data obj is null!!\n");
        return 0;
    }

    res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
    return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, char *buf, size_t count)
{
    struct k2dh_i2c_data *obj = obj_i2c_data;
    unsigned long data = 0;
    int error = -1;    

    if (obj == NULL)
    {
        GSE_ERR("i2c_data obj is null!!\n");
        return 0;
    }

    if (error = strict_strtoul(buf, 10, &data))
    {
        return error;
    }

    atomic_set(&obj->trace, data);

    //GSE_LOG("data : %d saved_data : %d , buf : %s, count : %d\n", data, atomic_read(&obj->trace), buf, count);

    return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
    ssize_t len = 0;
    struct k2dh_i2c_data *obj = obj_i2c_data;
    if (obj == NULL)
    {
        GSE_ERR("i2c_data obj is null!!\n");
        return 0;
    }

    if(obj->hw)
    {
        len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d %d (%d %d)\n", obj->hw->i2c_num, obj->hw->direction, obj->hw->power_id, obj->hw->power_vol);
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

    return sprintf(buf, "%d\n", sensor_power);
}

static ssize_t show_teststatus_value(struct device_driver *ddri, char *buf)
{
    struct i2c_client *client = k2dh_i2c_client;
    if(NULL == client)
    {
        GSE_ERR("i2c client is null!!\n");
        return 0;
    }
    
    return snprintf(buf, PAGE_SIZE, "%d\n", test_status);
}


#ifdef K2DH_ACCEL_CALIBRATION
static int k2dh_read_accel_xyz(struct i2c_client *client, struct K2DHacc *acc)
{
    int comres = 0;
    unsigned char data[6];

    return comres;
}

static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
    GSE_FUN();
    struct i2c_client *client = k2dh_i2c_client;
    struct k2dh_i2c_data *obj = i2c_get_clientdata(client);
    int offset_x,offset_y,offset_z;

    offset_x = obj->cali_sw[K2DH_AXIS_X];
    offset_y = obj->cali_sw[K2DH_AXIS_Y];
    offset_z = obj->cali_sw[K2DH_AXIS_Z];

    GSE_LOG("offset_x: %d, offset_y: %d, offset_z: %d\n",offset_x,offset_y,offset_z);

    return snprintf(buf, PAGE_SIZE, "%d %d %d \n", offset_x, offset_y, offset_z);
}

static ssize_t store_cali_value(struct device_driver *ddri, char *buf, size_t count)
{
    struct i2c_client *client = k2dh_i2c_client;
    struct k2dh_i2c_data *obj = i2c_get_clientdata(client);
    int err;
    int offset_x,offset_y,offset_z;
    int dat[K2DH_AXES_NUM];

    if(!strncmp(buf, "rst", 3))
    {
        GSE_FUN();
        if(err = k2dh_ResetCalibration(client))
        {
            GSE_ERR("reset offset err = %d\n", err);
        }
    }
    else if(3 == sscanf(buf, "%d %d %d", &offset_x, &offset_y, &offset_z))
    {
        GSE_LOG("store_cali_value: x=%d, y=%d, z=%d\n", offset_x, offset_y, offset_z);

        obj->cali_sw[K2DH_AXIS_X] = offset_x;
        obj->cali_sw[K2DH_AXIS_Y] = offset_y;
        obj->cali_sw[K2DH_AXIS_Z] = offset_z;
    }
    else
    {
        GSE_ERR("invalid format\n");
    }

    return count;
}

static ssize_t k2dh_fast_calibration_x_show(struct device_driver *ddri, char *buf)
{
    struct i2c_client *client = k2dh_i2c_client;
    struct k2dh_i2c_data *k2dh = i2c_get_clientdata(client);

    return sprintf(buf, "%d\n", atomic_read(&k2dh->fast_calib_x_rslt));
}

static ssize_t k2dh_fast_calibration_x_store(struct device_driver *ddri, char *buf, size_t count)
{
    unsigned long data;
    signed char tmp;
    unsigned char timeout = 0;
    unsigned int timeout_shaking = 0;
    int error;
    struct i2c_client *client = k2dh_i2c_client;
    struct k2dh_i2c_data *k2dh = i2c_get_clientdata(client);
    struct K2DHacc acc_cal;
    struct K2DHacc acc_cal_pre;

    test_status = 4;  // calibration status

    if (error = strict_strtoul(buf, 10, &data))
        return error;

    if(k2dh_do_calibration() != K2DH_SUCCESS)
        {
            atomic_set(&k2dh->fast_calib_x_rslt, 0);
            return -EINVAL;
        }

    atomic_set(&k2dh->fast_calib_x_rslt, 1);
    GSE_LOG(KERN_INFO "x axis fast calibration finished\n");

    return count;
}

static ssize_t k2dh_fast_calibration_y_show(struct device_driver *ddri, char *buf)
{

    struct i2c_client *client = k2dh_i2c_client;
    struct k2dh_i2c_data *k2dh = i2c_get_clientdata(client);

    return sprintf(buf, "%d\n", atomic_read(&k2dh->fast_calib_y_rslt));
}

static ssize_t k2dh_fast_calibration_y_store(struct device_driver *ddri, char *buf, size_t count)
{
    unsigned long data;
    int error;
    struct i2c_client *client = k2dh_i2c_client;
    struct k2dh_i2c_data *k2dh = i2c_get_clientdata(client);

    if (error = strict_strtoul(buf, 10, &data))
        return error;

    atomic_set(&k2dh->fast_calib_y_rslt, 1);
    GSE_LOG(KERN_INFO "y axis fast calibration finished\n");

    return count;
}

static ssize_t k2dh_fast_calibration_z_show(struct device_driver *ddri, char *buf)
{
    struct i2c_client *client = k2dh_i2c_client;
    struct k2dh_i2c_data *k2dh = i2c_get_clientdata(client);

    return sprintf(buf, "%d\n", atomic_read(&k2dh->fast_calib_z_rslt));
}

static ssize_t k2dh_fast_calibration_z_store(struct device_driver *ddri, char *buf, size_t count)
{
    unsigned long data;
    int error;
    struct i2c_client *client = k2dh_i2c_client;
    struct k2dh_i2c_data *k2dh = i2c_get_clientdata(client);

    if (error = strict_strtoul(buf, 10, &data))
        return error;

    atomic_set(&k2dh->fast_calib_z_rslt, 1);
    GSE_LOG(KERN_INFO "z axis fast calibration finished\n");

    test_status = sensor_power;

    return count;
}

#if 1
static int k2dh_runCalibration(void)
{
    if(k2dh_do_calibration() != K2DH_SUCCESS)
    {
        return -EINVAL;
    }
    GSE_LOG(KERN_INFO "self calibration Done\n");

    return K2DH_SUCCESS;
}
#endif

static ssize_t k2dh_eeprom_writing_show(struct device_driver *ddri, char *buf)
{
    struct i2c_client *client = k2dh_i2c_client;
    struct k2dh_i2c_data *k2dh = i2c_get_clientdata(client);

    return sprintf(buf, "%d\n", atomic_read(&k2dh->fast_calib_rslt));
}

static ssize_t k2dh_eeprom_writing_store(struct device_driver *ddri, char *buf, size_t count)
{
    unsigned char offset_x,offset_y,offset_z;
    struct i2c_client *client = k2dh_i2c_client;
    struct k2dh_i2c_data *k2dh = i2c_get_clientdata(client);

    atomic_set(&k2dh->fast_calib_rslt, 1);

    return count;
}

static int k2dh_soft_reset(struct i2c_client *client)
{
    int comres = 0;
    unsigned char data;

    GSE_FUN();
    // no soft_reset need

    return comres;
}
static ssize_t k2dh_softreset_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct i2c_client *client = k2dh_i2c_client;
    struct k2dh_i2c_data *k2dh = i2c_get_clientdata(client);

    if (k2dh_soft_reset(k2dh->client) < 0)
        return -EINVAL;

    return count;
}

#if 0
static ssize_t k2dh_runCalibration_store(struct device_driver *ddri, char *buf, size_t count)
{        
        unsigned long data = 0;
        int error = -1;
        int cali[3];

        
        struct i2c_client *client = k2dh_i2c_client;
        struct k2dh_i2c_data *obj = i2c_get_clientdata(client);
        
        if (error = strict_strtoul(buf, 10, &data))
        {
            return error;
        }
        GSE_LOG("Calibration CMD value : %d\n", (int)data);

        if(data == 1 || data == 2)  // calibration start command
{
        if(k2dh_runCalibration() == K2DH_SUCCESS)
        {
                cali[0] = obj->cali_sw[0];
                cali[1] = obj->cali_sw[1];
                cali[2] = obj->cali_sw[2];                
                if(LGE_FacWriteAccelerometerCalibration((unsigned int*)cali) == TRUE)
                {
                    atomic_set(&obj->fast_calib_rslt, 1);
				}
			}
			else // wrong input
			{
				GSE_LOG("Calibration FAIL\n");
				return -EINVAL;
			}
        }
	return count;
}

static ssize_t k2dh_runCalibration_show(struct device_driver *ddri, char *buf)
{
    int selfCalibration = 1;  // fail

    struct i2c_client *client = k2dh_i2c_client;
    struct k2dh_i2c_data *obj = i2c_get_clientdata(client);

    if(atomic_read(&obj->fast_calib_rslt) == 1)  // calibration success
    {
        selfCalibration = 0;  // success
    }
    else
    {
        selfCalibration = 1;  // fail
    }
	return sprintf(buf, "%d\n", selfCalibration);
}

#endif

#endif  // END OF K2DH_ACCEL_CALIBRATION

#if 1
static ssize_t k2dh_SelfTest_store(struct device_driver *ddri, char *buf, size_t count)
{        
        unsigned long data = 0;
        int error = -1;
        
        struct i2c_client *client = k2dh_i2c_client;
        struct k2dh_i2c_data *obj = i2c_get_clientdata(client);
        
        if (error = strict_strtoul(buf, 10, &data))
        {
            return error;
        }
        GSE_LOG("Self test CMD value : %d\n", (int)data);

        if(data == 1)  // self test start command
        {
            if(k2dh_get_selftest(client) >= 0)
            {            
                atomic_set(&obj->selftest_rslt, 1);
			}
			else
			{
                atomic_set(&obj->selftest_rslt, 0);
			}
        }
		else // wrong input
		{
            GSE_LOG("SelfTest FAIL\n");
            return -EINVAL;
		}
	return count;
}

static ssize_t k2dh_SelfTest_show(struct device_driver *ddri, char *buf)
{
    int selftest = 1;  // fail

    struct i2c_client *client = k2dh_i2c_client;
    struct k2dh_i2c_data *obj = i2c_get_clientdata(client);

    if(atomic_read(&obj->selftest_rslt) == 1)  // selftest success
    {
        selftest = 0;  // success
    }
    else
    {
        selftest = 1;  // fail
    }
	return sprintf(buf, "%d\n", selftest);
}
#endif

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo, S_IWUSR|S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(opmode, S_IWUSR|S_IRUGO|S_IWGRP, show_opmode_value, store_opmode_value);
static DRIVER_ATTR(range, S_IWUSR|S_IRUGO|S_IWGRP, show_range_value, store_range_value);
static DRIVER_ATTR(bandwidth, S_IWUSR|S_IRUGO|S_IWGRP, show_bandwidth_value, store_bandwidth_value);
static DRIVER_ATTR(sensordata, S_IWUSR|S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(sensorrawdata, S_IWUSR|S_IRUGO, show_sensorrawdata_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR|S_IRUGO|S_IWGRP, show_trace_value, store_trace_value);
static DRIVER_ATTR(status, S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(powerstatus, S_IRUGO, show_power_status_value, NULL);
static DRIVER_ATTR(teststatus, S_IWUSR|S_IRUGO, show_teststatus_value, NULL);
#ifdef K2DH_ACCEL_CALIBRATION
static DRIVER_ATTR(softreset, S_IWUSR|S_IWGRP, NULL, k2dh_softreset_store);
static DRIVER_ATTR(cali, S_IWUSR|S_IRUGO|S_IWGRP, show_cali_value, store_cali_value);
static DRIVER_ATTR(fast_calibration_x, S_IRUGO|S_IWUSR|S_IWGRP, k2dh_fast_calibration_x_show, k2dh_fast_calibration_x_store);
static DRIVER_ATTR(fast_calibration_y, S_IRUGO|S_IWUSR|S_IWGRP, k2dh_fast_calibration_y_show, k2dh_fast_calibration_y_store);
static DRIVER_ATTR(fast_calibration_z, S_IRUGO|S_IWUSR|S_IWGRP, k2dh_fast_calibration_z_show, k2dh_fast_calibration_z_store);
static DRIVER_ATTR(eeprom_writing, S_IRUGO|S_IWUSR|S_IWGRP, k2dh_eeprom_writing_show, k2dh_eeprom_writing_store);

#if 0
static DRIVER_ATTR(run_fast_calibration, S_IRUGO|S_IWUSR|S_IWGRP, k2dh_runCalibration_show, k2dh_runCalibration_store);
#endif
#endif
#ifdef CONFIG_K2DH_LOWPASS
static DRIVER_ATTR(firlen, S_IWUSR|S_IRUGO|S_IWGRP, show_firlen_value, store_firlen_value);
#endif
#if 1
static DRIVER_ATTR(selftest, S_IRUGO|S_IWUSR|S_IWGRP, k2dh_SelfTest_show, k2dh_SelfTest_store);
#endif


/*----------------------------------------------------------------------------*/
static struct driver_attribute *k2dh_attr_list[] = {
    &driver_attr_chipinfo,     /*chip information*/
    &driver_attr_opmode,
    &driver_attr_range,
    &driver_attr_bandwidth,
    &driver_attr_sensordata,   /*dump sensor data*/
    &driver_attr_sensorrawdata,
    &driver_attr_trace,        /*trace log*/
    &driver_attr_status,
    &driver_attr_powerstatus,
    &driver_attr_teststatus,
#ifdef K2DH_ACCEL_CALIBRATION
    &driver_attr_softreset,
    &driver_attr_cali,         /*show calibration data*/
    &driver_attr_fast_calibration_x,
    &driver_attr_fast_calibration_y,
    &driver_attr_fast_calibration_z,
    &driver_attr_eeprom_writing,
#if 0
&driver_attr_run_fast_calibration,
#endif
#endif
#ifdef CONFIG_K2DH_LOWPASS
    &driver_attr_firlen,       /*filter length: 0: disable, others: enable*/
#endif
#if 1
    &driver_attr_selftest,
#endif
};
/*----------------------------------------------------------------------------*/
static int k2dh_create_attr(struct device_driver *driver)
{
    int idx = 0, err = 0;
    int num = (int)(sizeof(k2dh_attr_list)/sizeof(k2dh_attr_list[0]));
    if (driver == NULL)
    {
        return -EINVAL;
    }

    for(idx = 0; idx < num; idx++)
    {
        if(err = driver_create_file(driver, k2dh_attr_list[idx]))
        {
            GSE_ERR("driver_create_file (%s) = %d\n", k2dh_attr_list[idx]->attr.name, err);
            break;
        }
    }
    return err;
}
/*----------------------------------------------------------------------------*/
static int k2dh_delete_attr(struct device_driver *driver)
{
    int idx = 0 ,err = 0;
    int num = (int)(sizeof(k2dh_attr_list)/sizeof(k2dh_attr_list[0]));

    if(driver == NULL)
    {
        return -EINVAL;
    }

    for(idx = 0; idx < num; idx++)
    {
        driver_remove_file(driver, k2dh_attr_list[idx]);
    }

    return err;
}

/*----------------------------------------------------------------------------*/
int gsensor_operate(void* self, uint32_t command, void* buff_in, int size_in, void* buff_out, int size_out, int* actualout)
{
    int err = 0;
    int value = 0, sample_delay = 0;
    struct k2dh_i2c_data *priv = (struct k2dh_i2c_data*)self;
    hwm_sensor_data* gsensor_data;
    char buff[K2DH_BUFSIZE] = {0};

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

#if 1
                sample_delay = ODR200;  // 5ms
#else
                if(value >= 100)
                {
                    sample_delay = ODR10;  // 100ms
                }
                else if(value >= 40)
                {
                    sample_delay = ODR25;  // 40ms
                }
                else if(value >= 20)
                {
                    sample_delay = ODR50;  // 20ms
                }
                else if(value >= 10)
                {
                    sample_delay = ODR100;  // 10ms
                }
                else
                {
                    sample_delay = ODR200;  // 5ms
                }
#endif
                err = k2dh_SetBWRate(priv->client, sample_delay);
                if(err != K2DH_SUCCESS )
                {
                    GSE_ERR("Set delay parameter error!\n");
                }

                if(value >= 50)
                {
                    atomic_set(&priv->filter, 0);
                }
                else
                {
                #if defined(CONFIG_K2DH_LOWPASS)
                    priv->fir.num = 0;
                    priv->fir.idx = 0;
                    priv->fir.sum[K2DH_AXIS_X] = 0;
                    priv->fir.sum[K2DH_AXIS_Y] = 0;
                    priv->fir.sum[K2DH_AXIS_Z] = 0;
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
                if(((value == 0) && (sensor_power == false)) || ((value == 1) && (sensor_power == true)))
                {
                    GSE_LOG("Gsensor device have updated!, power: %d\n", sensor_power);
                }
                else
                {
                    err = k2dh_SetPowerMode( priv->client, !sensor_power);
                }
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
                k2dh_ReadSensorData(priv->client, buff, K2DH_BUFSIZE);
                sscanf(buff, "%x %x %x", &gsensor_data->values[0], &gsensor_data->values[1], &gsensor_data->values[2]);
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
static int k2dh_open(struct inode *inode, struct file *file)
{
    file->private_data = k2dh_i2c_client;

    if(file->private_data == NULL)
    {
        GSE_ERR("null pointer!!\n");
        return -EINVAL;
    }
    return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int k2dh_release(struct inode *inode, struct file *file)
{
    file->private_data = NULL;
    return 0;
}
/*----------------------------------------------------------------------------*/
static long k2dh_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct i2c_client *client = (struct i2c_client*)file->private_data;
    struct k2dh_i2c_data *obj = (struct k2dh_i2c_data*)i2c_get_clientdata(client);
    char strbuf[K2DH_BUFSIZE] = {0};
    void __user *data;
    SENSOR_DATA sensor_data;
    long err = 0;
    int cali[3] = {0};
    uint32_t enable = 0;

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
#if 1
#if 0
            case GSENSOR_IOCTL_SET_ENABLE:
                GSE_LOG("GSENSOR_IOCTL_SET_ENABLE\n");
                data = (void __user *) arg;
                if(data == NULL)
                {
                    err = -EINVAL;
                    break;
                }
                if(copy_from_user(&enable, data, sizeof(enable)))
                {
                    return -EFAULT;
                }
                else
                {
                    if(enable == 1)
                    {
                        k2dh_SetPowerMode( obj_i2c_data->client, 1);
                    }
                    else if(enable == 0)
                    {
                        k2dh_SetPowerMode( obj_i2c_data->client, 0);
						GSE_LOG("coolpisoo, test 1: %d\n", data2[0]);
						msleep(10);
						k2dh_acc_register_read(client, data2, CTRL_REG2);
						GSE_LOG("coolpisoo, test 2: %d\n", data2[0]);
						msleep(10);
						k2dh_acc_register_read(client, data2, CTRL_REG3);
						GSE_LOG("coolpisoo, test 3: %d\n", data2[0]);
						msleep(10);
						k2dh_acc_register_read(client, data2, CTRL_REG4);
						GSE_LOG("coolpisoo, test 4: %d\n", data2[0]);
						msleep(10);
						k2dh_acc_register_read(client, data2, CTRL_REG5);
						GSE_LOG("coolpisoo, test 5: %d\n", data2[0]);
						msleep(10);
						k2dh_acc_register_read(client, data2, CTRL_REG6);
						GSE_LOG("coolpisoo, test 6: %d\n", data2[0]);
						msleep(10);
						k2dh_acc_register_read(client, data2, FIFO_CTRL_REG);
						GSE_LOG("coolpisoo, test 7: %d\n", data2[0]);
						msleep(10);
						k2dh_acc_register_read(client, data2, TEMP_CFG_REG);
						GSE_LOG("coolpisoo, test 8: %d\n", data2[0]);
						msleep(10);
                    }
                }
                break;
            case GSENSOR_IOCTL_GET_STATUS:
                GSE_LOG("GSENSOR_IOCTL_GET_STATUS\n");
                data = (void __user *) arg;
                if(data == NULL)
                {
                    err = -EINVAL;
                    break;
                }
                if(copy_to_user(data, &sensor_power, sizeof(sensor_power)))
                {
                    return -EFAULT;
                }
                break;    
#endif /* 0 */
        case GSENSOR_IOCTL_INIT:
            k2dh_acc_hw_init(client, 0);
            break;

        case GSENSOR_IOCTL_READ_CHIPINFO:
            data = (void __user *) arg;
            if(data == NULL)
            {
                err = -EINVAL;
                break;
            }

            k2dh_ReadChipInfo(client, strbuf, K2DH_BUFSIZE);
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

            k2dh_ReadSensorData(client, strbuf, K2DH_BUFSIZE);
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
            //k2dh_ReadRawData(client, strbuf);
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

            GSE_LOG("GSENSOR_IOCTL_SET_CALI, Cal data : 0x%x, 0x%x, 0x%x\n", sensor_data.x, sensor_data.y, sensor_data.z);
            if((sensor_data.x == 0) && (sensor_data.y == 0) && (sensor_data.z == 0))
            {  // temp defensive code for nvram_daemon ioctl
                break;
            }
            
             obj->cali_sw[K2DH_AXIS_X] = sensor_data.x;
             obj->cali_sw[K2DH_AXIS_Y] = sensor_data.y;
             obj->cali_sw[K2DH_AXIS_Z] = sensor_data.z; 
			if(obj->cali_sw[K2DH_AXIS_X]>1000|| obj->cali_sw[K2DH_AXIS_X]<-1000||
			obj->cali_sw[K2DH_AXIS_Y]>1000|| obj->cali_sw[K2DH_AXIS_Y]<-1000||
			obj->cali_sw[K2DH_AXIS_Z]>1000|| obj->cali_sw[K2DH_AXIS_Z]<-1000){
				GSE_LOG("Unnormal Cal Data");
				obj->cali_sw[K2DH_AXIS_X] = 0;
				obj->cali_sw[K2DH_AXIS_Y] = 0;
				obj->cali_sw[K2DH_AXIS_Z] = 0;
			}
            break;

        case GSENSOR_IOCTL_CLR_CALI:
            err = k2dh_ResetCalibration(client);
            break;

        case GSENSOR_IOCTL_GET_CALI:
            GSE_LOG("GSENSOR_IOCTL_GET_CALI\n");
            break;
#endif /* 0 */

        default:
            GSE_ERR("unknown IOCTL: 0x%08x\n", cmd);
            err = -ENOIOCTLCMD;
            break;
    }

    return err;
}

/*----------------------------------------------------------------------------*/
static struct file_operations k2dh_fops = {
    //.owner = THIS_MODULE,
    .open = k2dh_open,
    .release = k2dh_release,
    .unlocked_ioctl = k2dh_unlocked_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice k2dh_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "gsensor",
    .fops = &k2dh_fops,
};
/*----------------------------------------------------------------------------*/
#ifndef CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int k2dh_suspend(struct i2c_client *client, pm_message_t msg)
{
    GSE_FUN();

    return 0;
}
/*----------------------------------------------------------------------------*/
static int k2dh_resume(struct i2c_client *client)
{
    GSE_FUN();

    return 0;
}
/*----------------------------------------------------------------------------*/
#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void k2dh_early_suspend(struct early_suspend *h)
{
    GSE_FUN();
}
/*----------------------------------------------------------------------------*/
static void k2dh_late_resume(struct early_suspend *h)
{
    GSE_FUN();
}

/*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/
static int k2dh_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct i2c_client *new_client;
    struct k2dh_i2c_data *obj;
    struct hwmsen_object sobj;
    int err = 0;
    GSE_FUN();

    if(!(obj = kzalloc(sizeof(*obj), GFP_KERNEL)))
    {
        err = -ENOMEM;
        goto exit;
    }

    memset(obj, 0, sizeof(struct k2dh_i2c_data));

    obj->hw = get_cust_acc_hw();
    if(err = hwmsen_get_convert(obj->hw->direction, &obj->cvt))
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

#ifdef CONFIG_K2DH_LOWPASS
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

    k2dh_i2c_client = new_client;

    if(err = k2dh_acc_hw_init(new_client, 1))
    {
        GSE_ERR ( "failed to init K2DH ( err = %d )\n", err );
        goto exit_init_failed;
    }

    if(err = misc_register(&k2dh_device))
    {
        GSE_ERR("k2dh_device register failed\n");
        goto exit_misc_device_register_failed;
    }

    if(err = k2dh_create_attr(&k2dh_gsensor_driver.driver))
    {
        GSE_ERR("create attribute err = %d\n", err);
        goto exit_create_attr_failed;
    }

    sobj.self = obj;
    sobj.polling = 1;
    sobj.sensor_operate = gsensor_operate;
    if(err = hwmsen_attach(ID_ACCELEROMETER, &sobj))
    {
        GSE_ERR("attach fail = %d\n", err);
        goto exit_kfree;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
    obj->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
    obj->early_drv.suspend  = k2dh_early_suspend,
    obj->early_drv.resume   = k2dh_late_resume,
    register_early_suspend(&obj->early_drv);
#endif

    GSE_LOG("%s: OK\n", __func__);
    return 0;

    exit_create_attr_failed:
    misc_deregister(&k2dh_device);
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
static int k2dh_i2c_remove(struct i2c_client *client)
{
    int err = 0;

    if(err = k2dh_delete_attr(&k2dh_gsensor_driver.driver))
    {
        GSE_ERR("k2dh_delete_attr fail: %d\n", err);
    }

    if(err = misc_deregister(&k2dh_device))
    {
        GSE_ERR("misc_deregister fail: %d\n", err);
    }

    if(err = hwmsen_detach(ID_ACCELEROMETER))


    k2dh_i2c_client = NULL;
    i2c_unregister_device(client);
    kfree(i2c_get_clientdata(client));
    return 0;
}
/*----------------------------------------------------------------------------*/
static int k2dh_probe(struct platform_device *pdev)
{
    struct acc_hw *hw = get_cust_acc_hw();

    GSE_FUN();

    if(i2c_add_driver(&k2dh_i2c_driver))
    {
        GSE_ERR("add driver error\n");
        return -1;
    }
    return 0;
}
/*----------------------------------------------------------------------------*/
static int k2dh_remove(struct platform_device *pdev)
{
    struct acc_hw *hw = get_cust_acc_hw();

    GSE_FUN();
    k2dh_power(hw, 0);
    i2c_del_driver(&k2dh_i2c_driver);
    return 0;
}
/*----------------------------------------------------------------------------*/
static struct platform_driver k2dh_gsensor_driver = {
    .probe      = k2dh_probe,
    .remove     = k2dh_remove,
    .driver     = {
        .name  = "gsensor",
    }
};

/*----------------------------------------------------------------------------*/
static int __init k2dh_init(void)
{
    struct acc_hw *hw = get_cust_acc_hw();

    GSE_FUN();
    i2c_register_board_info(hw->i2c_num, &k2dh_i2c_info, 1);
    if(platform_driver_register(&k2dh_gsensor_driver))
    {
        GSE_ERR("failed to register driver");
        return -ENODEV;
    }
    return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit k2dh_exit(void)
{
    GSE_FUN();
    platform_driver_unregister(&k2dh_gsensor_driver);
}
/*----------------------------------------------------------------------------*/
module_init(k2dh_init);
module_exit(k2dh_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("K2DH Accelerometer misc driver");
MODULE_AUTHOR("STMcroelectronics");
