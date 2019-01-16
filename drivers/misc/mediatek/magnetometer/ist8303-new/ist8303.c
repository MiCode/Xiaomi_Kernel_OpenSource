/* drivers/i2c/chips/ist8303.c - IST8303 compass driver
 *
 * Copyright (C) 2009 Technology Inc.
 * Author: Minghung.chou
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
#include "ist8303.h"
#include <linux/hwmsen_helper.h>

#include <mach/devs.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

/*----------------------------------------------------------------------------*/
#define IST8303_M_NEW_ARCH   //susport kk new msensor arch

#ifdef IST8303_M_NEW_ARCH
#include "mag.h"
#endif

/*-------------------------MT6516&MT6573 define-------------------------------*/


#define POWER_NONE_MACRO MT65XX_POWER_NONE
//#define SOFT_GYRO
//#define REPLACE_ANDROID_VIRTUAL_SENSOR

/*----------------------------------------------------------------------------*/
#define I2C_DRIVERID_IST8303 304
#define DEBUG 1
#define IST8303_DEV_NAME          "ist8303"
#define DRIVER_VERSION            "1.0.0.0"
#define ODR_DELAY_TIME_MS         20  // ODR delayms for each loop
/*----------------------------------------------------------------------------*/
#define IST8303_AXIS_X            0
#define IST8303_AXIS_Y            1
#define IST8303_AXIS_Z            2
#define IST8303_AXES_NUM          3
/*----------------------------------------------------------------------------*/
// 0: augmented reality, stable but reponse slow
// 1: normal, medium stable and response medium
// 2: game, non-stable but response fast
// default is 0
#define ROTATION_VECTOR_RESPONSE_MODE   0

/*----------------------------------------------------------------------------*/
#define MSE_TAG                  "MSENSOR"
#define MSE_FUN(f)               printk(MSE_TAG" %s\r\n", __FUNCTION__)
#define MSE_ERR(fmt, args...)    printk(KERN_ERR MSE_TAG" %s %d : \r\n"fmt, __FUNCTION__, __LINE__, ##args)
#define MSE_LOG(fmt, args...)    printk(MSE_TAG fmt, ##args)
#define MSE_VER(fmt, args...)   ((void)0)
static DECLARE_WAIT_QUEUE_HEAD(data_ready_wq);
static DECLARE_WAIT_QUEUE_HEAD(open_wq);
static DECLARE_RWSEM(ist830x_rawdata_lock);

static atomic_t open_flag = ATOMIC_INIT(0);
/*----------------------------------------------------------------------------*/
static struct i2c_client *ist8303_i2c_client = NULL;
unsigned char ist830x_msensor_raw_data[7];
struct delayed_work ist_get_raw_data_work;
atomic_t ist830x_data_ready;
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id ist8303_i2c_id[] = {{IST8303_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_ist8303={ I2C_BOARD_INFO("ist8303", 0x0C)};
/*the adapter id will be available in customization*/
//static unsigned short ist8303_force[] = {0x00, IST8303_I2C_ADDRESS, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const ist8303_forces[] = { ist8303_force, NULL };
//static struct i2c_client_address_data ist8303_addr_data = { .forces = ist8303_forces,};

/*----------------------------------------------------------------------------*/
static int ist8303_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int ist8303_i2c_remove(struct i2c_client *client);
//static int ist8303_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
static int ist8303_suspend(struct i2c_client *client, pm_message_t msg) ;
static int ist8303_resume(struct i2c_client *client);

/*----------------------------------------------------------------------------*/
#ifdef IST8303_M_NEW_ARCH
static int ist8303_local_init(void);
static int ist8303_remove(void);
#else
static struct platform_driver ist_sensor_driver;
#endif

/*----------------------------------------------------------------------------*/
typedef enum {
    IST_TRC_DEBUG  = 0x01,
    IST_TRC_M_DATA  = 0x02,
    IST_TRC_O_DATA  = 0x04,
    IST_TRC_GYRO_DATA  = 0x08,
    IST_TRC_LINEAR_ACC_DATA  = 0x10,
    IST_TRC_GRAVITY_DATA  = 0x20,
    IST_TRC_ROTATION_VEC_DATA  = 0x40,
} IST_TRC;
/*----------------------------------------------------------------------------*/
struct _ist302_data {
    struct rw_semaphore lock;
    int mode;
    int rate;
    volatile int updated;
} ist830x_data;
/*----------------------------------------------------------------------------*/
struct _ist8303mid_data {
    struct rw_semaphore datalock;
    struct rw_semaphore ctrllock;    
    int controldata[10];
    unsigned int debug;
    int nmx;
    int nmy;
    int nmz;
    int mag_status;
    int nmx_uncali;
    int nmy_uncali;
    int nmz_uncali;
    int mag_status_uncali;
    int yaw;
    int roll;
    int pitch;
    int ori_status;
#ifdef SOFT_GYRO
    int ngx;
    int ngy;
    int ngz;
    int gyro_status;
    int rv[4];
    int rv_status;
#ifdef REPLACE_ANDROID_VIRTUAL_SENSOR
    int ngrx;
    int ngry;
    int ngrz;
    int gra_status;
    int nlax;
    int nlay;
    int nlaz;
    int la_status;
#endif
#endif
} ist8303mid_data;
/*----------------------------------------------------------------------------*/
struct ist8303_i2c_data {
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
static struct i2c_driver ist8303_i2c_driver = {
    .driver = {
//        .owner = THIS_MODULE, 
        .name  = IST8303_DEV_NAME,
    },
    .probe      = ist8303_i2c_probe,
    .remove     = ist8303_i2c_remove,
//  .detect     = ist8303_i2c_detect,
//#if !defined(CONFIG_HAS_EARLYSUSPEND)
    .suspend    = ist8303_suspend,
    .resume     = ist8303_resume,
//#endif 
    .id_table = ist8303_i2c_id,
    //.address_list = ist8303_forces,//address_data->address_list
};
/*----------------------------------------------------------------------------*/
#ifdef IST8303_M_NEW_ARCH
static int ist8303_init_flag =-1; // 0<==>OK -1 <==> fail
static struct mag_init_info ist8303_init_info = {
        .name = "ist8303",
        .init = ist8303_local_init,
        .uninit = ist8303_remove,   
};
#endif
/*----------------------------------------------------------------------------*/
static atomic_t dev_open_count;
/*----------------------------------------------------------------------------*/
static int ist830x_i2c_rxdata( struct i2c_client *i2c, unsigned char *rxData, int length)
{
    unsigned char addr;
    
    struct i2c_msg msgs[] = {
    {
        .addr = i2c->addr,
        .flags = 0,
        .len = 1,
        .buf = rxData,
    },
    {
        .addr = i2c->addr,
        .flags = I2C_M_RD,
        .len = length,
        .buf = rxData,
    }, };

    addr = rxData[0];

    if (i2c_transfer(i2c->adapter, msgs, 2) < 0) {
        dev_err(&i2c->dev, "%s: transfer failed.", __func__);
        return -EIO;
    }

    dev_vdbg(&i2c->dev, "RxData: len=%02x, addr=%02x, data=%02x", length, addr, rxData[0]);
    return 0;
}
/*----------------------------------------------------------------------------*/
static int ist830x_i2c_txdata(struct i2c_client *i2c, unsigned char *txData, int length)
{
    struct i2c_msg msg[] = {
    {
        .addr = i2c->addr,
        .flags = 0,
        .len = length,
        .buf = txData,
    }, };

    if (i2c_transfer(i2c->adapter, msg, 1) < 0) {
        dev_err(&i2c->dev, "%s: transfer failed.", __func__);
        return -EIO;
    }

    dev_vdbg(&i2c->dev, "TxData: len=%02x, addr=%02x data=%02x", length, txData[0], txData[1]);
    return 0;
}
/*----------------------------------------------------------------------------*/
static void ist8303_power(struct mag_hw *hw, unsigned int on) 
{
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
            if(!hwPowerOn(hw->power_id, hw->power_vol, "IST8303")) 
            {
                MSE_ERR("power on fails!!\n");
            }
        }
        else
        {
            if(!hwPowerDown(hw->power_id, "IST8303")) 
            {
                MSE_ERR("power off fail!!\n");
            }
        }
    }
    power_on = on;
}
/*----------------------------------------------------------------------------*/

static int ist8303_GetOpenStatus(void)
{
    wait_event_interruptible(open_wq, (atomic_read(&open_flag) != 0));
    return atomic_read(&open_flag);
}

static int ist8303_GetCloseStatus(void)
{
    wait_event_interruptible(open_wq, (atomic_read(&open_flag) == 0));
    return atomic_read(&open_flag);
}

#if 0
static int ist8303_gpio_config(void)
{
    //because we donot use EINT ,to support low power
    // config to GPIO input mode + PD    
    //set   GPIO_MSE_EINT_PIN

    mt_set_gpio_mode(GPIO_MSE_EINT_PIN, GPIO_MSE_EINT_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_MSE_EINT_PIN, GPIO_DIR_IN);
    mt_set_gpio_pull_enable(GPIO_MSE_EINT_PIN, GPIO_PULL_ENABLE);
    mt_set_gpio_pull_select(GPIO_MSE_EINT_PIN, GPIO_PULL_DOWN);

    return 0;
}
#endif
/*----------------------------------------------------------------------------*/
static int IST8303_CheckDataReady(struct i2c_client *client)  //  this is used for normal mode, not for force mode
{
    int cResult = 0;
    char buffer[1];
    int  ret;

    buffer[0] = IST8303_REG_STAT1;

    ret = ist830x_i2c_rxdata(client, buffer, 1); // ret can be : -5 or 0

    if (ret < 0) {
        printk(KERN_ERR "IST8303 IST8303_CheckDataReady : I2C failed \n");
        return ret;
    }
    
    cResult = (buffer[0] & 0x1);  //  if TRUE then read data

    return cResult;
}
/*----------------------------------------------------------------------------*/
static int IST8303_GetData(struct i2c_client *client, char *rbuf)
{
    int ret;
    rbuf[0] = IST8303_REG_STAT1;

    ret = ist830x_i2c_rxdata(client, rbuf, 7);

    if (ret < 0) {
        printk(KERN_ERR "IST8303_GetXYZ : I2C failed \n");
        return ret;
    }
    return ret;
}
/*----------------------------------------------------------------------------*/
typedef struct _ODR_ITEM {
    int hz;
    int delayms;
    int regval;
} ODR_ITEM;
/*
 2: Continuous measurement mode with ODR 8Hz
 3: Continuous measurement mode with ODR 10Hz
 5: Continuous measurement mode with ODR 20Hz
 6: Continuous measurement mode with ODR 100Hz
 7: Continuous measurement mode with ODR 50Hz
10: Continuous measurement mode with ODR 1Hz
11: Continuous measurement mode with ODR 200Hz
*/
static ODR_ITEM ODR_TABLE[] = 
{
    {  8,  125,  2},
    { 10,  100,  3},
    { 20,   50,  5},
    {100,   10,  6},
    { 50,   20,  7},
    {  1, 1000, 10},
    {200,    5, 11},
};
static int _GetODR(int nDelayMs)
{
    int i = 0;
    for (i=0; i<sizeof(ODR_TABLE)/sizeof(ODR_ITEM); i++)
    {
        if (ODR_TABLE[i].delayms == nDelayMs)
            return ODR_TABLE[i].regval;
    }

    // ODR 20HZ or default
    return 5;
}

static int IST8303_Chipset_Init(int mode)
{
    char wbuffer[2];
    int ret;

    wbuffer[0] = IST8303_REG_CNTRL1;
    wbuffer[1] = 0x00;  //  stand-by mode  
    ret = ist830x_i2c_txdata(ist8303_i2c_client, wbuffer, 2);
    if (ret<0) {
        printk(KERN_ERR "set register IST8303_REG_CNTRL1 failed.\n");
        return ret;
    }
    
    wbuffer[0] = IST8303_REG_CNTRL2;
    ret = ist830x_i2c_rxdata(ist8303_i2c_client, wbuffer, 1);
    if (ret<0) {
        printk(KERN_ERR "get register IST8303_REG_CNTRL2 failed.\n");
        return ret;
    }
    wbuffer[1] = wbuffer[0] & 0xF3; // clear bit2 & bit3 to disable DRDY external pin
    wbuffer[0] = IST8303_REG_CNTRL2;
    ret = ist830x_i2c_txdata(ist8303_i2c_client, wbuffer, 2);
    if (ret<0) {
        printk(KERN_ERR "set register IST8303_REG_CNTRL2 failed.\n");
        return ret;
    }

    wbuffer[0] = IST8303_REG_SSR;
    wbuffer[1] = 0xC0;  //  set to 1us pulse duration + low power mode  
    ret = ist830x_i2c_txdata(ist8303_i2c_client, wbuffer, 2);
    if(ret<0) {
        printk(KERN_ERR "set register IST8303_REG_SSR failed.\n");
        return ret;
    }

    wbuffer[0] = IST8303_REG_CTR;
    wbuffer[1] = 0x00;  //  use temperature compensation mechanism
    ret = ist830x_i2c_txdata(ist8303_i2c_client, wbuffer, 2);
    if (ret<0) {
        printk(KERN_ERR "set register IST8303_REG_CTR failed.\n");
        return ret;
    }
    
    wbuffer[0] = 0x62;
    wbuffer[1] = 0x00;  //  only for 8303b pulse width
    ret = ist830x_i2c_txdata(ist8303_i2c_client, wbuffer, 2);
    if (ret<0) {
        printk(KERN_ERR "set register IST8303_REG_BTR failed.\n");
        return ret;
    }
    
    return 0;
}

static int IST8303_Chipset_Enable(int mode, int enable)
{
    char wbuffer[2];
    int ret;

    if (enable == 0)
    {
        wbuffer[0] = IST8303_REG_CNTRL1;
        wbuffer[1] = 0x01;  //  Single measurement mode
        ret = ist830x_i2c_txdata(ist8303_i2c_client, wbuffer, 2);
        if (ret<0) {
            printk(KERN_ERR "set register IST8303_REG_CNTRL1 failed.\n");
            return ret;
        }
    }
    else
    {
        if (mode == IST8303_FORCE_MODE)
        {
            wbuffer[0] = IST8303_REG_CNTRL1;
            wbuffer[1] = 0x01;  //  Single measurement mode
            ret = ist830x_i2c_txdata(ist8303_i2c_client, wbuffer, 2);
            if (ret<0) {
                printk(KERN_ERR "set register IST8303_REG_CNTRL1 failed.\n");
                return ret;
            }
        }
        else // normal mode
        {
            wbuffer[0] = IST8303_REG_CNTRL1;
            wbuffer[1] = _GetODR(ODR_DELAY_TIME_MS);
            printk(KERN_INFO "DelayMs=%d, ODR=%d\n", ODR_DELAY_TIME_MS, (int)wbuffer[1]); 
            ret = ist830x_i2c_txdata(ist8303_i2c_client, wbuffer, 2);
            if (ret<0) {
                printk(KERN_ERR "set register IST8303_REG_CNTRL1 failed.\n");
                return ret;
            }
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------*/
static void ist830x_prepare_raw_data(struct work_struct *work)
{
    char sData[7];  //  sensor raw data
    int err;    
    int status;    
    unsigned long start;
    
    schedule_delayed_work(&ist_get_raw_data_work, msecs_to_jiffies(ODR_DELAY_TIME_MS));

    /* Check DRDY bit */
    start = jiffies;
    do {
        status = IST8303_CheckDataReady(ist8303_i2c_client); // status can be : -5 or 0 or 1
        if (status == 1) // DRDY is 1, ok
            break;
    } while (jiffies_to_msecs(jiffies - start) <= ODR_DELAY_TIME_MS);
            
    if (status < 0) // -5 case
    {
        printk("ist830x_i2c_rxdata() fail \n");
        return;
    }

    if (status == 0) // DRDY = 0 case
    {
        printk("DRDY is 0 \n");
        err = IST8303_Chipset_Enable(IST8303_NORMAL_MODE, 1); // -5 : fail 0: succeed
        if(err)
            printk("IST8303_Chipset_Enable fail \n");

        return;
    }

    err = IST8303_GetData(ist8303_i2c_client, sData); // err can be : -5 or 0   
    if (err < 0) {
        dev_err(&ist8303_i2c_client->dev, "%s failed.", __func__);
        return;
    }

    down_write(&ist830x_rawdata_lock);
    memcpy(ist830x_msensor_raw_data, sData, 7);
    up_write(&ist830x_rawdata_lock);

    atomic_set(&ist830x_data_ready, 1);
    wake_up(&data_ready_wq);
}
/*----------------------------------------------------------------------------*/
static void IST8303_SetMode(int newmode)
{
    down_write(&ist830x_data.lock);

    if(ist830x_data.mode != newmode)
    {
        ist830x_data.mode = newmode;
        IST8303_Chipset_Init(newmode);
    }

    up_write(&ist830x_data.lock);
}
/*----------------------------------------------------------------------------*/
static int IST8303_ReadChipInfo(char *buf, int bufsize)
{
    if((!buf)||(bufsize<=30))
    {
        return -1;
    }
    if(!ist8303_i2c_client)
    {
        *buf = 0;
        return -2;
    }

    sprintf(buf, "IST8303 Chip");
    return 0;
}
/*----------------------------------------------------------------------------*/
static int IST8303_ReadSensorData(char *buf, int bufsize)
{
    struct ist8303_i2c_data *data = i2c_get_clientdata(ist8303_i2c_client);
  //  char cmd;
    int mode = 0, err = 0;    
    unsigned char databuf[7];  //  for sensor raw data
    short output[IST8303_AXES_NUM];
    short temp = 0;
    int mag[IST8303_AXES_NUM];

    if ((!buf)||(bufsize<=80))
    {
        return -1;
    }   
    if (NULL == ist8303_i2c_client)
    {
        *buf = 0;
        return -2;
    }

    down_read(&ist830x_data.lock);    
    mode = ist830x_data.mode;
    up_read(&ist830x_data.lock);        

    if(mode == IST8303_FORCE_MODE)
    {
     //   unsigned long start;
//        int status;

        if (ODR_DELAY_TIME_MS <= 10)
        {
            // read first the set single measurement for next reading
            err = IST8303_GetData(ist8303_i2c_client, databuf); // err can be : -5 or 0   
            if (err < 0) {
                dev_err(&ist8303_i2c_client->dev, "%s failed.", __func__);
                return -5;
            }

            IST8303_Chipset_Enable(mode, 1);
        }
        else
        {
            IST8303_Chipset_Enable(mode, 1);

            /* wait DRDY ready */
            usleep_range(4500, 5000);

            err = IST8303_GetData(ist8303_i2c_client, databuf); // err can be : -5 or 0   
            if (err < 0) {
                dev_err(&ist8303_i2c_client->dev, "%s failed.", __func__);
                return -5;
            }
        }

    }
    else  //  IST8303_NORMAL_MODE for ist830x
    {
        err = wait_event_interruptible_timeout(data_ready_wq, atomic_read(&ist830x_data_ready), msecs_to_jiffies(ODR_DELAY_TIME_MS*2));
        
        if (err == 0) {
            printk("wait 100ms timeout \n");
            return err;
        }

        if (err < 0) {
            printk("interrupted by other signal \n");
            return err;
        }
        
        down_read(&ist830x_rawdata_lock);
        memcpy(databuf, ist830x_msensor_raw_data, 7);  //  from : ist830x_prepare_raw_data()
        up_read(&ist830x_rawdata_lock);

        atomic_set(&ist830x_data_ready, 0);
    }

    output[0] = ((int) databuf[2]) << 8 | ((int) databuf[1]);
    output[1] = ((int) databuf[4]) << 8 | ((int) databuf[3]);
    output[2] = ((int) databuf[6]) << 8 | ((int) databuf[5]);

    // swap x/y
    temp = output[0];
    output[0] = output[1];
    output[1] = temp;

    mag[data->cvt.map[IST8303_AXIS_X]] = data->cvt.sign[IST8303_AXIS_X]*output[IST8303_AXIS_X];
    mag[data->cvt.map[IST8303_AXIS_Y]] = data->cvt.sign[IST8303_AXIS_Y]*output[IST8303_AXIS_Y];
    mag[data->cvt.map[IST8303_AXIS_Z]] = data->cvt.sign[IST8303_AXIS_Z]*output[IST8303_AXIS_Z];

    sprintf(buf, "%d %d %d", mag[IST8303_AXIS_X], mag[IST8303_AXIS_Y], mag[IST8303_AXIS_Z]);

    return 1;
}

static int IST8303_SelfTest(void)
{
    int err, i;
    char buf[100];
    int mag_raw_1[3], mag_raw_2[3];
    char wbuffer[2];
    
    memset(buf, 0, 20);
    memset(mag_raw_1, 0, 3);
    memset(mag_raw_2, 0, 3);
    
    for(i=0;i<10;++i)
    {    
        err = IST8303_ReadSensorData(buf, 100);
        if (err != 1)
            printk("IST8303_ReadSensorData error \n");
        
        err = sscanf(buf,"%d %d %d",&mag_raw_1[0],&mag_raw_1[1],&mag_raw_1[2]);
        if (err != 3)
            printk("sscanf error \n");
            
        if(mag_raw_1[0] && mag_raw_1[1] && mag_raw_1[2])
            break;
    }
    
    if(i==10)
        return -1;

    printk("before self-test : mag_raw_1[0] = %d, mag_raw_1[1] = %d, mag_raw_1[2] = %d \n", mag_raw_1[0], mag_raw_1[1], mag_raw_1[2]);
    
    //  start writing self-test register
    wbuffer[0] = IST8303_REG_STCR;
    wbuffer[1] = 0x40;  //  self-test mode  
    err = ist830x_i2c_txdata(ist8303_i2c_client, wbuffer, 2);
    if (err<0)
    {
        printk(KERN_ERR "set register IST8303_REG_STCR failed.\n");
        return -1;
    }
    
    err = IST8303_ReadSensorData(buf, 100);
    if (err != 1)
        printk("IST8303_ReadSensorData error \n");
        
    err = sscanf(buf,"%d %d %d",&mag_raw_2[0],&mag_raw_2[1],&mag_raw_2[2]);
    if (err != 3)
        printk("sscanf error \n");

    printk("after self-test : mag_raw_2[0] = %d, mag_raw_2[1] = %d, mag_raw_2[2] = %d \n", mag_raw_2[0], mag_raw_2[1], mag_raw_2[2]);    
    
    if( (mag_raw_1[0]&mag_raw_2[0])>=0 && (mag_raw_1[1]&mag_raw_2[1])>=0 && (mag_raw_1[2]&mag_raw_2[2])>=0 )
    {
        wbuffer[0] = IST8303_REG_STCR;
        wbuffer[1] = 0x0;  //  self-test mode  
        err = ist830x_i2c_txdata(ist8303_i2c_client, wbuffer, 2);
        if (err<0)
        {
            printk(KERN_ERR "set register IST8303_REG_STCR failed.\n");
            return -1;
        }
        
        return 1;
    }
    else
    {
        wbuffer[0] = IST8303_REG_STCR;
        wbuffer[1] = 0x0;  //  self-test mode  
        err = ist830x_i2c_txdata(ist8303_i2c_client, wbuffer, 2);
        if (err<0)
        {
            printk(KERN_ERR "set register IST8303_REG_STCR failed.\n");
            return -1;
        }
        
        return -1;
    }
}
/*----------------------------------------------------------------------------*/
static int IST8303_ReadPostureData(char *buf, int bufsize)
{
    if((!buf)||(bufsize<=80))
    {
        return -1;
    }
    
    down_read(&ist8303mid_data.datalock);
    sprintf(buf, "%d %d %d %d", ist8303mid_data.yaw, ist8303mid_data.pitch,
        ist8303mid_data.roll, ist8303mid_data.mag_status);
    up_read(&ist8303mid_data.datalock);
    return 0;
}
/*----------------------------------------------------------------------------*/
static int IST8303_ReadCaliData(char *buf, int bufsize)
{
    if((!buf)||(bufsize<=80))
    {
        return -1;
    }
    
    down_read(&ist8303mid_data.datalock);
    sprintf(buf, "%d %d %d %d", ist8303mid_data.nmx, ist8303mid_data.nmy, 
        ist8303mid_data.nmz,ist8303mid_data.mag_status);
    up_read(&ist8303mid_data.datalock);
    return 0;
}
/*----------------------------------------------------------------------------*/
static int IST8303_ReadMiddleControl(char *buf, int bufsize)
{
    if ((!buf)||(bufsize<=80))
    {
        return -1;
    }
    
    down_read(&ist8303mid_data.ctrllock);
    sprintf(buf, "%d %d %d %d %d %d %d %d %d %d",ist8303mid_data.controldata[0],    ist8303mid_data.controldata[1], 
        ist8303mid_data.controldata[2],ist8303mid_data.controldata[3],ist8303mid_data.controldata[4],
        ist8303mid_data.controldata[5], ist8303mid_data.controldata[6], ist8303mid_data.controldata[7],
        ist8303mid_data.controldata[8], ist8303mid_data.controldata[9]);
    up_read(&ist8303mid_data.ctrllock);
    return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_daemon_name(struct device_driver *ddri, char *buf)
{
    char strbuf[IST8303_BUFSIZE];
    sprintf(strbuf, "istd8303");
    return sprintf(buf, "%s", strbuf);      
}

static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
    char strbuf[IST8303_BUFSIZE];
    IST8303_ReadChipInfo(strbuf, IST8303_BUFSIZE);
    return sprintf(buf, "%s\n", strbuf);        
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
    char strbuf[IST8303_BUFSIZE];
    IST8303_ReadSensorData(strbuf, IST8303_BUFSIZE);
    return sprintf(buf, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_posturedata_value(struct device_driver *ddri, char *buf)
{
    char strbuf[IST8303_BUFSIZE];
    IST8303_ReadPostureData(strbuf, IST8303_BUFSIZE);
    return sprintf(buf, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_calidata_value(struct device_driver *ddri, char *buf)
{
    char strbuf[IST8303_BUFSIZE];
    IST8303_ReadCaliData(strbuf, IST8303_BUFSIZE);
    return sprintf(buf, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_midcontrol_value(struct device_driver *ddri, char *buf)
{
    char strbuf[IST8303_BUFSIZE];
    IST8303_ReadMiddleControl(strbuf, IST8303_BUFSIZE);
    return sprintf(buf, "%s\n", strbuf);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_midcontrol_value(struct device_driver *ddri, const char *buf, size_t count)
{   
    int p[10];
    if(10 == sscanf(buf, "%d %d %d %d %d %d %d %d %d %d",&p[0], &p[1], &p[2], &p[3], &p[4], 
        &p[5], &p[6], &p[7], &p[8], &p[9]))
    {
        down_write(&ist8303mid_data.ctrllock);
        memcpy(&ist8303mid_data.controldata[0], &p, sizeof(int)*10);    
        up_write(&ist8303mid_data.ctrllock);        
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
    down_read(&ist8303mid_data.ctrllock);
    len = sprintf(buf, "0x%08X\n", ist8303mid_data.debug);
    up_read(&ist8303mid_data.ctrllock);

    return len;            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_middebug_value(struct device_driver *ddri, const char *buf, size_t count)
{   
    int debug;
    if(1 == sscanf(buf, "0x%x", &debug))
    {
        down_write(&ist8303mid_data.ctrllock);
        ist8303mid_data.debug = debug;
        up_write(&ist8303mid_data.ctrllock);        
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
    down_read(&ist830x_data.lock);
    mode = ist830x_data.mode;
    up_read(&ist830x_data.lock);        
    return sprintf(buf, "%d\n", mode);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_mode_value(struct device_driver *ddri, const char *buf, size_t count)
{
    int mode = 0;
    sscanf(buf, "%d", &mode);    
    IST8303_SetMode(mode);
    return count;            
}
/*----------------------------------------------------------------------------*/
static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
    struct i2c_client *client = ist8303_i2c_client;  
    struct ist8303_i2c_data *data = i2c_get_clientdata(client);

    return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
        data->hw->direction,atomic_read(&data->layout), data->cvt.sign[0], data->cvt.sign[1],
        data->cvt.sign[2],data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);            
}
/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri, const char *buf, size_t count)
{
    struct i2c_client *client = ist8303_i2c_client;  
    struct ist8303_i2c_data *data = i2c_get_clientdata(client);
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
    struct i2c_client *client = ist8303_i2c_client;  
    struct ist8303_i2c_data *data = i2c_get_clientdata(client);
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
/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
    ssize_t res;
    struct ist8303_i2c_data *obj = i2c_get_clientdata(ist8303_i2c_client);
    if(NULL == obj)
    {
        MSE_ERR("ist8303_i2c_data is null!!\n");
        return 0;
    }   
    
    res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));     
    return res;    
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
    struct ist8303_i2c_data *obj = i2c_get_clientdata(ist8303_i2c_client);
    int trace;
    if(NULL == obj)
    {
        MSE_ERR("ist8303_i2c_data is null!!\n");
        return count;
    }
    
    if(1 == sscanf(buf, "0x%x", &trace))
    {
        atomic_set(&obj->trace, trace);
    }
    else 
    {
        MSE_ERR("invalid content: '%s', length = %zu\n", buf, count);
    }
    
    return count;    
}

static ssize_t show_shipment_test(struct device_driver *ddri, char *buf)
{
    char result[10];
    int res = 0;
    
    res = IST8303_SelfTest();
	
    if(1 == res)
    {
        strcpy(result,"y");
    }
    else if(-1 == res)
    {
        strcpy(result,"n");
    }

    return sprintf(buf, "%s\n", result);        
}

static ssize_t store_shipment_test(struct device_driver * ddri,const char * buf, size_t count)
{
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
static DRIVER_ATTR(status,      S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(trace,       S_IRUGO | S_IWUSR, show_trace_value, store_trace_value );
static DRIVER_ATTR(shipmenttest,S_IRUGO | S_IWUSR, show_shipment_test, store_shipment_test);
/*----------------------------------------------------------------------------*/
static struct driver_attribute *ist8303_attr_list[] = {
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
    &driver_attr_shipmenttest,
};
/*----------------------------------------------------------------------------*/
static int ist8303_create_attr(struct device_driver *driver) 
{
    int idx, err = 0;
    int num = (int)(sizeof(ist8303_attr_list)/sizeof(ist8303_attr_list[0]));
    if (driver == NULL)
    {
        return -EINVAL;
    }

    for(idx = 0; idx < num; idx++)
    {
        if((err = driver_create_file(driver, ist8303_attr_list[idx])))
        {            
            MSE_ERR("driver_create_file (%s) = %d\n", ist8303_attr_list[idx]->attr.name, err);
            break;
        }
    }    
    return err;
}
/*----------------------------------------------------------------------------*/
static int ist8303_delete_attr(struct device_driver *driver)
{
    int idx ,err = 0;
    int num = (int)(sizeof(ist8303_attr_list)/sizeof(ist8303_attr_list[0]));

    if(driver == NULL)
    {
        return -EINVAL;
    }
    

    for(idx = 0; idx < num; idx++)
    {
        driver_remove_file(driver, ist8303_attr_list[idx]);
    }
    

    return err;
}


/*----------------------------------------------------------------------------*/
static int ist8303_open(struct inode *inode, struct file *file)
{    
    struct ist8303_i2c_data *obj = i2c_get_clientdata(ist8303_i2c_client);    
    int ret = -1;
    atomic_inc(&dev_open_count);
    
    if(atomic_read(&obj->trace) & IST_TRC_DEBUG)
    {
        MSE_LOG("Open device node:ist8303\n");
    }
    ret = nonseekable_open(inode, file);
    
    return ret;
}
/*----------------------------------------------------------------------------*/
static int ist8303_release(struct inode *inode, struct file *file)
{
    struct ist8303_i2c_data *obj = i2c_get_clientdata(ist8303_i2c_client);
    atomic_dec(&dev_open_count);
    if(atomic_read(&obj->trace) & IST_TRC_DEBUG)
    {
        MSE_LOG("Release device node:ist8303\n");
    }   
    return 0;
}
/*----------------------------------------------------------------------------*/
//static int ist8303_ioctl(struct inode *inode, struct file *file, unsigned int cmd,unsigned long arg)//modified here
static long ist8303_unlocked_ioctl(struct file *file, unsigned int cmd,
       unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    int valuebuf[4];
#ifdef SOFT_GYRO
    int calidata[30];
#else
    int calidata[30];
#endif
    int controlbuf[10];
    char strbuf[IST8303_BUFSIZE];
    void __user *data;
    long retval=0;
    int mode=0;
    hwm_sensor_data* osensor_data;
    uint32_t enable;
    char buff[512]; 
    int status;                 /* for OPEN/CLOSE_STATUS */
//    short sensor_status;        /* for Orientation and Msensor status */

    switch (cmd)
    {
        case MSENSOR_IOCTL_INIT:
            down_read(&ist830x_data.lock);
            mode = ist830x_data.mode;
            up_read(&ist830x_data.lock);
            IST8303_Chipset_Init(mode);         
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
            
            down_write(&ist8303mid_data.datalock);
            ist8303mid_data.yaw   = valuebuf[0];
            ist8303mid_data.pitch = valuebuf[1];
            ist8303mid_data.roll  = valuebuf[2];
            ist8303mid_data.mag_status = valuebuf[3];
            up_write(&ist8303mid_data.datalock);    
            break;

        case ECS_IOCTL_GET_OPEN_STATUS:
            status = ist8303_GetOpenStatus();           
            if(copy_to_user(argp, &status, sizeof(status)))
            {
                MSE_LOG("copy_to_user failed.");
                return -EFAULT;
            }
            break;        

        case ECS_IOCTL_GET_CLOSE_STATUS:
            status = ist8303_GetCloseStatus();           
            if(copy_to_user(argp, &status, sizeof(status)))
            {
                MSE_LOG("copy_to_user failed.");
                return -EFAULT;
            }
            break;        

        case MSENSOR_IOCTL_SET_CALIDATA:
            data = (void __user *) arg;
            if (data == NULL)
            {
                MSE_ERR("IO parameter pointer is NULL!\r\n");
                break;
            }
            if(copy_from_user(calidata, data, sizeof(calidata)))
            {
                retval = -EFAULT;
                goto err_out;
            }

            down_write(&ist8303mid_data.datalock);            
            ist8303mid_data.nmx = calidata[1];
            ist8303mid_data.nmy = calidata[2];
            ist8303mid_data.nmz = calidata[3];
            ist8303mid_data.mag_status = calidata[4];
            ist8303mid_data.nmx_uncali = calidata[5];
            ist8303mid_data.nmy_uncali = calidata[6];
            ist8303mid_data.nmz_uncali = calidata[7];
            ist8303mid_data.mag_status_uncali = calidata[8];
            ist8303mid_data.yaw   = calidata[9];
            ist8303mid_data.pitch = calidata[10];
            ist8303mid_data.roll  = calidata[11];
            ist8303mid_data.ori_status = calidata[12];
#ifdef SOFT_GYRO
            ist8303mid_data.ngx = calidata[13];
            ist8303mid_data.ngy = calidata[14];
            ist8303mid_data.ngz = calidata[15];
            ist8303mid_data.gyro_status = calidata[16];
            ist8303mid_data.rv[0] = calidata[17];
            ist8303mid_data.rv[1] = calidata[18];
            ist8303mid_data.rv[2] = calidata[19];
            ist8303mid_data.rv[3] = calidata[20];
            ist8303mid_data.rv_status = calidata[21];
#ifdef REPLACE_ANDROID_VIRTUAL_SENSOR
            ist8303mid_data.ngrx = calidata[22];
            ist8303mid_data.ngry = calidata[23];
            ist8303mid_data.ngrz = calidata[24];
            ist8303mid_data.gra_status = calidata[25];
            ist8303mid_data.nlax = calidata[26];
            ist8303mid_data.nlay = calidata[27];
            ist8303mid_data.nlaz = calidata[28];
            ist8303mid_data.la_status = calidata[29];
#endif
#endif

            up_write(&ist8303mid_data.datalock);
            //printk("[qnmd] calidata[7] = %d,calidata[8] = %d,calidata[9] = %d\n",calidata[7],calidata[8],calidata[9]);    
            break;                                

        case MSENSOR_IOCTL_READ_CHIPINFO:
            data = (void __user *) arg;
            if(data == NULL)
            {
                MSE_ERR("IO parameter pointer is NULL!\r\n");
                break;
            }
            
            IST8303_ReadChipInfo(strbuf, IST8303_BUFSIZE);
            if(copy_to_user(data, strbuf, strlen(strbuf)+1))
            {
                retval = -EFAULT;
                goto err_out;
            }                
            break;

        case ECOMPASS_IOC_GET_DELAY:
            data = (void __user *) arg;
            if(data == NULL)
            {
                MSE_ERR("IO parameter pointer is NULL!\r\n");
                break;
            }

            status = ODR_DELAY_TIME_MS;
            
            if(copy_to_user(data, &status, sizeof(status)))
            {
                retval = -EFAULT;
                goto err_out;
            }                
            break;

        case MSENSOR_IOCTL_SENSOR_ENABLE:
            
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
                down_write(&ist8303mid_data.ctrllock);
                if(enable == 1)
                {
                    ist8303mid_data.controldata[7] |= SENSOR_ORIENTATION;
                    atomic_set(&open_flag, 1);
                }
                else
                {
                    ist8303mid_data.controldata[7] &= ~SENSOR_ORIENTATION;
                    atomic_set(&open_flag, 0);
                }
                wake_up(&open_wq);
                up_write(&ist8303mid_data.ctrllock);
                
            }
            
            break;
            
        case MSENSOR_IOCTL_READ_SENSORDATA:
            data = (void __user *) arg;
            if(data == NULL)
            {
                MSE_ERR("IO parameter pointer is NULL!\r\n");
                break;    
            }
            
            status = IST8303_ReadSensorData(strbuf, IST8303_BUFSIZE);
            if (status < 0)
            {
                status = IST8303_ReadSensorData(strbuf, IST8303_BUFSIZE);
            }

            if(status == 0) // wait 100ms timeout
            {
                retval = -ETIME;
                goto err_out;
            }
            
            if(copy_to_user(data, strbuf, strlen(strbuf)+1))
            {
                retval = -EFAULT;
                goto err_out;
            }
            break;

        case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
            
            data = (void __user *) arg;
            if (data == NULL)
            {
                MSE_ERR("IO parameter pointer is NULL!\r\n");
                break;
            }
            
            osensor_data = (hwm_sensor_data *)buff;

            down_read(&ist8303mid_data.datalock);
            osensor_data->values[0] = ist8303mid_data.yaw;
            osensor_data->values[1] = ist8303mid_data.pitch;
            osensor_data->values[2] = ist8303mid_data.roll;
            //status = ist8303mid_data.mag_status;
            up_read(&ist8303mid_data.datalock); 
                        
            osensor_data->value_divide = ORIENTATION_ACCURACY_RATE; 

            switch (ist8303mid_data.mag_status)
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
            data = (void __user *) arg;
            if(data == NULL)
            {
                MSE_ERR("IO parameter pointer is NULL!\r\n");
                break;
            }
            
            IST8303_ReadPostureData(strbuf, IST8303_BUFSIZE);
            if(copy_to_user(data, strbuf, strlen(strbuf)+1))
            {
                retval = -EFAULT;
                goto err_out;
            }                
            break;            

        case MSENSOR_IOCTL_READ_CALIDATA:
            data = (void __user *) arg;
            if(data == NULL)
            {
                break;    
            }
            IST8303_ReadCaliData(strbuf, IST8303_BUFSIZE);
            if(copy_to_user(data, strbuf, strlen(strbuf)+1))
            {
                retval = -EFAULT;
                goto err_out;
            }                
            break;

        case MSENSOR_IOCTL_READ_CONTROL:
            down_read(&ist8303mid_data.ctrllock);
            memcpy(controlbuf, &ist8303mid_data.controldata[0], sizeof(controlbuf));
            up_read(&ist8303mid_data.ctrllock);            
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
            down_write(&ist8303mid_data.ctrllock);
            memcpy(&ist8303mid_data.controldata[0], controlbuf, sizeof(controlbuf));
            up_write(&ist8303mid_data.ctrllock);        
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
            
            IST8303_SetMode(mode);                
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
static long ist8303_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    long ret = 0;

    void __user *arg64 = compat_ptr(arg);
    if ( !file->f_op || !file->f_op->unlocked_ioctl )
    {
        printk(KERN_ERR "file->f_op OR file->f_op->unlocked_ioctl is null!\n");
        return -ENOTTY;
    }

    switch (cmd)
    {
        case COMPAT_MSENSOR_IOCTL_INIT:
            ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_INIT, (unsigned long)arg64);
            if (ret < 0)
            {
                printk(KERN_ERR "MSENSOR_IOCTL_INIT is failed!\n");
            }
            break;

        case COMPAT_MSENSOR_IOCTL_SET_POSTURE:
            ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_SET_POSTURE, (unsigned long)arg64);
            if (ret < 0)
            {
                printk(KERN_ERR "MSENSOR_IOCTL_SET_POSTURE is failed!\n");
            }
            break;

        case COMPAT_ECS_IOCTL_GET_OPEN_STATUS:
            ret = file->f_op->unlocked_ioctl(file, ECS_IOCTL_GET_OPEN_STATUS, (unsigned long)arg64);
            if (ret < 0)
            {
                printk(KERN_ERR "ECS_IOCTL_GET_OPEN_STATUS is failed!\n");
            }
            break;

        case COMPAT_ECS_IOCTL_GET_CLOSE_STATUS:
            ret = file->f_op->unlocked_ioctl(file, ECS_IOCTL_GET_CLOSE_STATUS, (unsigned long)arg64);
            if (ret < 0)
            {
                printk(KERN_ERR "ECS_IOCTL_GET_CLOSE_STATUS is failed!\n");
            }
            break;

        case COMPAT_MSENSOR_IOCTL_SET_CALIDATA:
            ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_SET_CALIDATA, (unsigned long)arg64);
            if (ret < 0)
            {
                printk(KERN_ERR "MSENSOR_IOCTL_SET_CALIDATA is failed!\n");
            }
            break;

        case COMPAT_MSENSOR_IOCTL_READ_CHIPINFO:
            ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_CHIPINFO, (unsigned long)arg64);
            if (ret < 0)
            {
                printk(KERN_ERR "MSENSOR_IOCTL_READ_CHIPINFO is failed!\n");
            }
            break;

        case COMPAT_ECOMPASS_IOC_GET_DELAY:
            ret = file->f_op->unlocked_ioctl(file, ECOMPASS_IOC_GET_DELAY, (unsigned long)arg64);
            if (ret < 0)
            {
                printk(KERN_ERR "ECOMPASS_IOC_GET_DELAY is failed!\n");
            }
            break;

        case COMPAT_MSENSOR_IOCTL_SENSOR_ENABLE:
            ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_SENSOR_ENABLE, (unsigned long)arg64);
            if (ret < 0)
            {
                printk(KERN_ERR "MSENSOR_IOCTL_SENSOR_ENABLE is failed!\n");
            }
            break;

        case COMPAT_MSENSOR_IOCTL_READ_SENSORDATA:
            ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_SENSORDATA, (unsigned long)arg64);
            if (ret < 0)
            {
                printk(KERN_ERR "MSENSOR_IOCTL_READ_SENSORDATA is failed!\n");
            }
            break;

        case COMPAT_MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
            ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_FACTORY_SENSORDATA, (unsigned long)arg64);
            if (ret < 0)
            {
                printk(KERN_ERR "MSENSOR_IOCTL_READ_FACTORY_SENSORDATA is failed!\n");
            }
            break;

        case COMPAT_MSENSOR_IOCTL_READ_POSTUREDATA:
            ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_POSTUREDATA, (unsigned long)arg64);
            if (ret < 0)
            {
                printk(KERN_ERR "MSENSOR_IOCTL_READ_POSTUREDATA is failed!\n");
            }
            break;

        case COMPAT_MSENSOR_IOCTL_READ_CALIDATA:
            ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_CALIDATA, (unsigned long)arg64);
            if (ret < 0)
            {
                printk(KERN_ERR "MSENSOR_IOCTL_READ_CALIDATA is failed!\n");
            }
            break;

        case COMPAT_MSENSOR_IOCTL_READ_CONTROL:
            ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_CONTROL, (unsigned long)arg64);
            if (ret < 0)
            {
                printk(KERN_ERR "MSENSOR_IOCTL_READ_CONTROL is failed!\n");
            }
            break;

        case COMPAT_MSENSOR_IOCTL_SET_CONTROL:
            ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_SET_CONTROL, (unsigned long)arg64);
            if (ret < 0)
            {
                printk(KERN_ERR "MSENSOR_IOCTL_SET_CONTROL is failed!\n");
            }
            break;

        case COMPAT_MSENSOR_IOCTL_SET_MODE:
            ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_SET_MODE, (unsigned long)arg64);
            if (ret < 0)
            {
                printk(KERN_ERR "MSENSOR_IOCTL_SET_MODE is failed!\n");
            }
            break;

        default:
            MSE_ERR("%s not supported = 0x%04x", __FUNCTION__, cmd);
            ret = -ENOIOCTLCMD;
            break;
    }

    return ret;
}
#endif

/*----------------------------------------------------------------------------*/
static struct file_operations ist8303_fops = {
//  .owner = THIS_MODULE,
    .open = ist8303_open,
    .release = ist8303_release,
    .unlocked_ioctl = ist8303_unlocked_ioctl,//modified
#ifdef CONFIG_COMPAT
    .compat_ioctl = ist8303_compat_ioctl,
#endif
};
/*----------------------------------------------------------------------------*/
static struct miscdevice ist8303_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "msensor",
    .fops = &ist8303_fops,
};
/*----------------------------------------------------------------------------*/
#ifndef IST8303_M_NEW_ARCH  

int ist8303_operate(void* self, uint32_t command, void* buff_in, int size_in,
        void* buff_out, int size_out, int* actualout)
{
    int err = 0;
    int value, status;
    hwm_sensor_data* msensor_data;

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
                if(value <= 20)
                {
                    value = 20;
                }
                ist8303mid_data.controldata[0] = value;  // Loop Delay
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
                down_write(&ist8303mid_data.ctrllock);
                if (value == 1)
                {
                    if (ist8303mid_data.controldata[7] == 0)
                    {
                        IST8303_Chipset_Enable(ist830x_data.mode, 1);
                        if (ist830x_data.mode == IST8303_NORMAL_MODE)
                        {
                            schedule_delayed_work(&ist_get_raw_data_work, msecs_to_jiffies(ODR_DELAY_TIME_MS));
                        }
                        atomic_set(&open_flag, 1);
                    }
                    ist8303mid_data.controldata[7] |= SENSOR_MAGNETIC;
                }
                else
                {
                    ist8303mid_data.controldata[7] &= ~SENSOR_MAGNETIC;
                    if (ist8303mid_data.controldata[7] == 0)
                    {
                        atomic_set(&open_flag, 0);
                        if (ist830x_data.mode == IST8303_NORMAL_MODE)
                        {
                            cancel_delayed_work_sync(&ist_get_raw_data_work);
                        }
                        IST8303_Chipset_Enable(IST8303_NORMAL_MODE, 0);
                    }
                }
                wake_up(&open_wq);
                up_write(&ist8303mid_data.ctrllock);
                // TODO: turn device into standby or normal mode
            }
            break;

        case SENSOR_GET_DATA:
            //MSE_LOG("++++++++++++++++++++++++MSENSOR_GET_DATA");
            if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
            {
                MSE_ERR("get sensor data parameter error!\n");
                err = -EINVAL;
            }
            else
            {
                msensor_data = (hwm_sensor_data *)buff_out;
                down_read(&ist8303mid_data.datalock);
                msensor_data->values[0] = ist8303mid_data.nmx;
                msensor_data->values[1] = ist8303mid_data.nmy;
                msensor_data->values[2] = ist8303mid_data.nmz;
                status = ist8303mid_data.mag_status;
                up_read(&ist8303mid_data.datalock); 
                
                msensor_data->values[0] = msensor_data->values[0] * CONVERT_M;
                msensor_data->values[1] = msensor_data->values[1] * CONVERT_M;
                msensor_data->values[2] = msensor_data->values[2] * CONVERT_M;
                msensor_data->value_divide = 100;

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
int ist8303_orientation_operate(void* self, uint32_t command, void* buff_in, int size_in,
        void* buff_out, int size_out, int* actualout)
{
    int err = 0;
    int value, status=0;
    hwm_sensor_data* osensor_data=NULL;
    
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
                if(value <= 20)
                {
                    value = 20;
                }
                ist8303mid_data.controldata[0] = value;  // Loop Delay
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
                down_write(&ist8303mid_data.ctrllock);
                if(value == 1)
                {
                    if (ist8303mid_data.controldata[7] == 0)
                    {
                        IST8303_Chipset_Enable(ist830x_data.mode, 1);
                        if (ist830x_data.mode == IST8303_NORMAL_MODE)
                        {
                            schedule_delayed_work(&ist_get_raw_data_work, msecs_to_jiffies(ODR_DELAY_TIME_MS));
                        }
                        atomic_set(&open_flag, 1);
                    }
                    ist8303mid_data.controldata[7] |= SENSOR_ORIENTATION;
                }
                else
                {
                    ist8303mid_data.controldata[7] &= ~SENSOR_ORIENTATION;
                    if (ist8303mid_data.controldata[7] == 0)
                    {
                        atomic_set(&open_flag, 0);
                        if (ist830x_data.mode == IST8303_NORMAL_MODE)
                        {
                            cancel_delayed_work_sync(&ist_get_raw_data_work);
                        }
                        IST8303_Chipset_Enable(IST8303_NORMAL_MODE, 0);
                    }
                }
                wake_up(&open_wq);
                up_write(&ist8303mid_data.ctrllock);
                // Do nothing
            }
            break;

        case SENSOR_GET_DATA:
            //MSE_LOG("+++++++++++MSENSOR_GET_ORIENTATION_DATA");
            if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
            {
                MSE_ERR("get sensor data parameter error!\n");
                err = -EINVAL;
            }
            else
            {
                osensor_data = (hwm_sensor_data *)buff_out;
                down_read(&ist8303mid_data.datalock);
                osensor_data->values[0] = ist8303mid_data.yaw;
                osensor_data->values[1] = ist8303mid_data.pitch;
                osensor_data->values[2] = ist8303mid_data.roll;
                status = ist8303mid_data.mag_status;
                up_read(&ist8303mid_data.datalock); 
                
                
                osensor_data->value_divide = ORIENTATION_ACCURACY_RATE;             
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
            MSE_ERR("gsensor operate function no this parameter %d!\n", command);
            err = -1;
            break;
    }

    return err;
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef SOFT_GYRO
int ist8303_gyro_operate(void* self, uint32_t command, void* buff_in, int size_in,
        void* buff_out, int size_out, int* actualout)
{
    int err = 0;
    int value ,status;
    hwm_sensor_data* gysensor_data; 
    
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
                if(value <= 20)
                {
                    value = 20;
                }
                ist8303mid_data.controldata[0] = value;  // Loop Delay
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
                down_write(&ist8303mid_data.ctrllock);
                if(value == 1)
                {
                    if (ist8303mid_data.controldata[7] == 0)
                    {
                        IST8303_Chipset_Enable(ist830x_data.mode, 1);
                        if (ist830x_data.mode == IST8303_NORMAL_MODE)
                        {
                            schedule_delayed_work(&ist_get_raw_data_work, msecs_to_jiffies(ODR_DELAY_TIME_MS));
                        }
                        atomic_set(&open_flag, 1);
                    }
                    ist8303mid_data.controldata[7] |= SENSOR_GYROSCOPE;
                }
                else
                {
                    ist8303mid_data.controldata[7] &= ~SENSOR_GYROSCOPE;
                    if (ist8303mid_data.controldata[7] == 0)
                    {
                        atomic_set(&open_flag, 0);
                        if (ist830x_data.mode == IST8303_NORMAL_MODE)
                        {
                            cancel_delayed_work_sync(&ist_get_raw_data_work);
                        }
                        IST8303_Chipset_Enable(IST8303_NORMAL_MODE, 0);
                    }
                }
                wake_up(&open_wq);
                up_write(&ist8303mid_data.ctrllock);
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
                gysensor_data = (hwm_sensor_data *)buff_out;

                down_read(&ist8303mid_data.datalock);
                gysensor_data->values[0] = ist8303mid_data.ngx;
                gysensor_data->values[1] = ist8303mid_data.ngy;
                gysensor_data->values[2] = ist8303mid_data.ngz;
                status = ist8303mid_data.gyro_status;
                up_read(&ist8303mid_data.datalock); 
                
                gysensor_data->value_divide = 100000;

                switch (status)
                {
                    case 1: case 2:
                        gysensor_data->status = SENSOR_STATUS_ACCURACY_HIGH;
                        break;
                    case 3:
                        gysensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
                        break;
                    case 4:
                        gysensor_data->status = SENSOR_STATUS_ACCURACY_LOW;
                        break;
                    default:        
                        gysensor_data->status = SENSOR_STATUS_UNRELIABLE;
                        break;    
                }
                
            }
            break;
        default:
            MSE_ERR("sensor operate function no this parameter %d!\n", command);
            err = -1;
            break;
    }
    
    return err;
}

int ist8303_rotation_vector_operate(void* self, uint32_t command, void* buff_in, int size_in,
        void* buff_out, int size_out, int* actualout)
{
    int err = 0;
    int value ,status;
    hwm_sensor_data* gysensor_data; 

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
                if(value <= 20)
                {
                    value = 20;
                }
                ist8303mid_data.controldata[0] = value;  // Loop Delay
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
                down_write(&ist8303mid_data.ctrllock);
                if(value == 1)
                {
                    if (ist8303mid_data.controldata[7] == 0)
                    {
                        IST8303_Chipset_Enable(ist830x_data.mode, 1);
                        if (ist830x_data.mode == IST8303_NORMAL_MODE)
                        {
                            schedule_delayed_work(&ist_get_raw_data_work, msecs_to_jiffies(ODR_DELAY_TIME_MS));
                        }
                        atomic_set(&open_flag, 1);
                    }
                    ist8303mid_data.controldata[7] |= SENSOR_ROTATION_VECTOR;
                }
                else
                {
                    ist8303mid_data.controldata[7] &= ~SENSOR_ROTATION_VECTOR;
                    if (ist8303mid_data.controldata[7] == 0)
                    {
                        atomic_set(&open_flag, 0);
                        if (ist830x_data.mode == IST8303_NORMAL_MODE)
                        {
                            cancel_delayed_work_sync(&ist_get_raw_data_work);
                        }
                        IST8303_Chipset_Enable(IST8303_NORMAL_MODE, 0);
                    }
                }
                wake_up(&open_wq);
                up_write(&ist8303mid_data.ctrllock);
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
                gysensor_data = (hwm_sensor_data *)buff_out;

                down_read(&ist8303mid_data.datalock);
                gysensor_data->values[0] = ist8303mid_data.rv[0];
                gysensor_data->values[1] = ist8303mid_data.rv[1];
                gysensor_data->values[2] = ist8303mid_data.rv[2];
                status = ist8303mid_data.rv_status;
                up_read(&ist8303mid_data.datalock); 

                gysensor_data->value_divide = 1000000;

                switch (status)
                {
                    case 1: case 2:
                        gysensor_data->status = SENSOR_STATUS_ACCURACY_HIGH;
                        break;
                    case 3:
                        gysensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
                        break;
                    case 4:
                        gysensor_data->status = SENSOR_STATUS_ACCURACY_LOW;
                        break;
                    default:        
                        gysensor_data->status = SENSOR_STATUS_UNRELIABLE;
                        break;    
                }
                
            }
            break;
        default:
            MSE_ERR("sensor operate function no this parameter %d!\n", command);
            err = -1;
            break;
    }
    
    return err;
}

#ifdef REPLACE_ANDROID_VIRTUAL_SENSOR
int ist8303_gravity_operate(void* self, uint32_t command, void* buff_in, int size_in,
        void* buff_out, int size_out, int* actualout)
{
    int err = 0;
    int value ,status;
    hwm_sensor_data* gysensor_data; 
    
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
                if(value <= 20)
                {
                    value = 20;
                }
                ist8303mid_data.controldata[0] = value;  // Loop Delay
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
                down_write(&ist8303mid_data.ctrllock);
                if(value == 1)
                {
                    if (ist8303mid_data.controldata[7] == 0)
                    {
                        IST8303_Chipset_Enable(ist830x_data.mode, 1);
                        if (ist830x_data.mode == IST8303_NORMAL_MODE)
                        {
                            schedule_delayed_work(&ist_get_raw_data_work, msecs_to_jiffies(ODR_DELAY_TIME_MS));
                        }
                        atomic_set(&open_flag, 1);
                    }
                    ist8303mid_data.controldata[7] |= SENSOR_GRAVITY;
                }
                else
                {
                    ist8303mid_data.controldata[7] &= ~SENSOR_GRAVITY;
                    if (ist8303mid_data.controldata[7] == 0)
                    {
                        atomic_set(&open_flag, 0);
                        if (ist830x_data.mode == IST8303_NORMAL_MODE)
                        {
                            cancel_delayed_work_sync(&ist_get_raw_data_work);
                        }
                        IST8303_Chipset_Enable(IST8303_NORMAL_MODE, 0);
                    }
                }
                wake_up(&open_wq);
                up_write(&ist8303mid_data.ctrllock);
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
                gysensor_data = (hwm_sensor_data *)buff_out;

                down_read(&ist8303mid_data.datalock);
                gysensor_data->values[0] = ist8303mid_data.ngrx;
                gysensor_data->values[1] = ist8303mid_data.ngry;
                gysensor_data->values[2] = ist8303mid_data.ngrz;
                status = ist8303mid_data.gra_status;
                up_read(&ist8303mid_data.datalock); 
                
                gysensor_data->value_divide = 1000;

                switch (status)
                {
                    case 1: case 2:
                        gysensor_data->status = SENSOR_STATUS_ACCURACY_HIGH;
                        break;
                    case 3:
                        gysensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
                        break;
                    case 4:
                        gysensor_data->status = SENSOR_STATUS_ACCURACY_LOW;
                        break;
                    default:        
                        gysensor_data->status = SENSOR_STATUS_UNRELIABLE;
                        break;    
                }
                
            }
            break;
        default:
            MSE_ERR("sensor operate function no this parameter %d!\n", command);
            err = -1;
            break;
    }
    
    return err;
}

int ist8303_linear_acceleration_operate(void* self, uint32_t command, void* buff_in, int size_in,
        void* buff_out, int size_out, int* actualout)
{
    int err = 0;
    int value ,status;
    hwm_sensor_data* gysensor_data; 
    
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
                if(value <= 20)
                {
                    value = 20;
                }
                ist8303mid_data.controldata[0] = value;  // Loop Delay
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
                down_write(&ist8303mid_data.ctrllock);
                if(value == 1)
                {
                    if (ist8303mid_data.controldata[7] == 0)
                    {
                        IST8303_Chipset_Enable(ist830x_data.mode, 1);
                        if (ist830x_data.mode == IST8303_NORMAL_MODE)
                        {
                            schedule_delayed_work(&ist_get_raw_data_work, msecs_to_jiffies(ODR_DELAY_TIME_MS));
                        }
                        atomic_set(&open_flag, 1);
                    }
                    ist8303mid_data.controldata[7] |= SENSOR_LINEAR_ACCELERATION;
                }
                else
                {
                    ist8303mid_data.controldata[7] &= ~SENSOR_LINEAR_ACCELERATION;
                    if (ist8303mid_data.controldata[7] == 0)
                    {
                        atomic_set(&open_flag, 0);
                        if (ist830x_data.mode == IST8303_NORMAL_MODE)
                        {
                            cancel_delayed_work_sync(&ist_get_raw_data_work);
                        }
                        IST8303_Chipset_Enable(IST8303_NORMAL_MODE, 0);
                    }
                }
                wake_up(&open_wq);
                up_write(&ist8303mid_data.ctrllock);
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
                gysensor_data = (hwm_sensor_data *)buff_out;

                down_read(&ist8303mid_data.datalock);
                gysensor_data->values[0] = ist8303mid_data.nlax;
                gysensor_data->values[1] = ist8303mid_data.nlay;
                gysensor_data->values[2] = ist8303mid_data.nlaz;
                status = ist8303mid_data.la_status;
                up_read(&ist8303mid_data.datalock); 

                gysensor_data->value_divide = 1000;

                switch (status)
                {
                    case 1: case 2:
                        gysensor_data->status = SENSOR_STATUS_ACCURACY_HIGH;
                        break;
                    case 3:
                        gysensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
                        break;
                    case 4:
                        gysensor_data->status = SENSOR_STATUS_ACCURACY_LOW;
                        break;
                    default:        
                        gysensor_data->status = SENSOR_STATUS_UNRELIABLE;
                        break;    
                }
                
            }
            break;
        default:
            MSE_ERR("sensor operate function no this parameter %d!\n", command);
            err = -1;
            break;
    }
    
    return err;
}

#endif //#ifdef REPLACE_ANDROID_VIRTUAL_SENSOR
#endif 
/*--------------------------------------------------------------------*/
#ifdef IST8303_M_NEW_ARCH

static int ist8303_m_open_report_data(int en)
{
    return 0;
}
static int ist8303_m_set_delay(u64 delay)
{
    //int value = (int)delay/1000/1000;
    int value = (int)delay;
    
    if(value <= 20)
    {
        value = 20;
    }
    ist8303mid_data.controldata[0] = value;  // Loop Delay

    return 0;
}
static int ist8303_m_enable(int en)
{
    down_write(&ist8303mid_data.ctrllock);
    if(en == 1)
    {
        if (ist8303mid_data.controldata[7] == 0)
        {
            IST8303_Chipset_Enable(ist830x_data.mode, 1);
            if (ist830x_data.mode == IST8303_NORMAL_MODE)
            {
                schedule_delayed_work(&ist_get_raw_data_work, msecs_to_jiffies(ODR_DELAY_TIME_MS));
            }
            atomic_set(&open_flag, 1);
        }
        ist8303mid_data.controldata[7] |= SENSOR_MAGNETIC;
    }
    else
    {
        ist8303mid_data.controldata[7] &= ~SENSOR_MAGNETIC;
        if (ist8303mid_data.controldata[7] == 0)
        {
            atomic_set(&open_flag, 0);
            if (ist830x_data.mode == IST8303_NORMAL_MODE)
            {
                cancel_delayed_work_sync(&ist_get_raw_data_work);
            }
            IST8303_Chipset_Enable(IST8303_NORMAL_MODE, 0);
        }
    }
    wake_up(&open_wq);
    up_write(&ist8303mid_data.ctrllock);

    return 0;
}
static int ist8303_o_open_report_data(int en)
{
    return 0;
}
static int ist8303_o_set_delay(u64 delay)
{
    int value = (int)delay/1000/1000;
   // int value = (int)delay;

    if(value <= 20)
    {
        value = 20;
    }
    ist8303mid_data.controldata[0] = value;  // Loop Delay

    return 0;
}
static int ist8303_o_enable(int en)
{
    down_write(&ist8303mid_data.ctrllock);
    if(en == 1)
    {
        if (ist8303mid_data.controldata[7] == 0)
        {
            IST8303_Chipset_Enable(ist830x_data.mode, 1);
            if (ist830x_data.mode == IST8303_NORMAL_MODE)
            {
                schedule_delayed_work(&ist_get_raw_data_work, msecs_to_jiffies(ODR_DELAY_TIME_MS));
            }
            atomic_set(&open_flag, 1);
        }
        ist8303mid_data.controldata[7] |= SENSOR_ORIENTATION;
    }
    else
    {
        ist8303mid_data.controldata[7] &= ~SENSOR_ORIENTATION;
        if (ist8303mid_data.controldata[7] == 0)
        {
            atomic_set(&open_flag, 0);
            if (ist830x_data.mode == IST8303_NORMAL_MODE)
            {
                cancel_delayed_work_sync(&ist_get_raw_data_work);
            }
            IST8303_Chipset_Enable(IST8303_NORMAL_MODE, 0);
        }
    }
    wake_up(&open_wq);
    up_write(&ist8303mid_data.ctrllock);

    return 0;
}

static int ist8303_o_get_data(int* x, int* y, int* z, int* status)
{
    int status_temp = 0;

    down_read(&ist8303mid_data.datalock);
    *x = ist8303mid_data.yaw;
    *y = ist8303mid_data.pitch;
    *z = ist8303mid_data.roll;
    status_temp = ist8303mid_data.mag_status;
    up_read(&ist8303mid_data.datalock);

    switch (status_temp)
    {
        case 1: case 2:
            *status = SENSOR_STATUS_ACCURACY_HIGH;
            break;
        case 3:
            *status = SENSOR_STATUS_ACCURACY_MEDIUM;
            break;
        case 4:
            *status = SENSOR_STATUS_ACCURACY_LOW;
            break;
        default:
            *status = SENSOR_STATUS_UNRELIABLE;
            break;
    }

    return 0;
}
static int ist8303_m_get_data(int* x, int* y, int* z, int* status)
{
    int status_temp = 0;

    down_read(&ist8303mid_data.datalock);
    *x = ist8303mid_data.nmx;
    *y = ist8303mid_data.nmy;
    *z = ist8303mid_data.nmz;
    status_temp = ist8303mid_data.mag_status;
    up_read(&ist8303mid_data.datalock);

    *x = *x * CONVERT_M;
    *y = *y * CONVERT_M;
    *z = *z * CONVERT_M;

    switch (status_temp)
    {
        case 1: case 2:
            *status = SENSOR_STATUS_ACCURACY_HIGH;
            break;
        case 3:
            *status = SENSOR_STATUS_ACCURACY_MEDIUM;
            break;
        case 4:
            *status = SENSOR_STATUS_ACCURACY_LOW;
            break;
        default:
            *status = SENSOR_STATUS_UNRELIABLE;
            break;
    }

    return 0;
}

#endif

#ifndef IST8303_M_NEW_ARCH
#ifdef ID_MAGNETIC_UNCALIBRATED
int ist8303_uncalibrated_operate(void* self, uint32_t command, void* buff_in, int size_in,
        void* buff_out, int size_out, int* actualout)
{
    int err = 0;
    int value, status;
    hwm_sensor_data* msensor_data;
    
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
                if(value <= 20)
                {
                    value = 20;
                }
                ist8303mid_data.controldata[0] = value;  // Loop Delay
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
                down_write(&ist8303mid_data.ctrllock);
                if (value == 1)
                {
                    if (ist8303mid_data.controldata[7] == 0)
                    {
                        IST8303_Chipset_Enable(ist830x_data.mode, 1);
                        if (ist830x_data.mode == IST8303_NORMAL_MODE)
                        {
                            schedule_delayed_work(&ist_get_raw_data_work, msecs_to_jiffies(ODR_DELAY_TIME_MS));
                        }
                        atomic_set(&open_flag, 1);
                    }
                    ist8303mid_data.controldata[7] |= SENSOR_MAGNETIC_UNCALIBRATED;
                }
                else
                {
                    ist8303mid_data.controldata[7] &= ~SENSOR_MAGNETIC_UNCALIBRATED;
                    if (ist8303mid_data.controldata[7] == 0)
                    {
                        atomic_set(&open_flag, 0);
                        if (ist830x_data.mode == IST8303_NORMAL_MODE)
                        {
                            cancel_delayed_work_sync(&ist_get_raw_data_work);
                        }
                        IST8303_Chipset_Enable(IST8303_NORMAL_MODE, 0);
                    }
                }
                wake_up(&open_wq);
                up_write(&ist8303mid_data.ctrllock);
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
                down_read(&ist8303mid_data.datalock);
                msensor_data->values[0] = ist8303mid_data.nmx_uncali;
                msensor_data->values[1] = ist8303mid_data.nmy_uncali;
                msensor_data->values[2] = ist8303mid_data.nmz_uncali;
                status = ist8303mid_data.mag_status_uncali;
                up_read(&ist8303mid_data.datalock); 
                
                msensor_data->values[0] = msensor_data->values[0] * CONVERT_M;
                msensor_data->values[1] = msensor_data->values[1] * CONVERT_M;
                msensor_data->values[2] = msensor_data->values[2] * CONVERT_M;
                msensor_data->value_divide = 100;

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
                
            }
            break;
        default:
            MSE_ERR("msensor_uncali operate function no this parameter %d!\n", command);
            err = -1;
            break;
    }
    
    return err;
}
#endif
#endif
/*----------------------------------------------------------------------------*/
//#ifndef   CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int ist8303_suspend(struct i2c_client *client, pm_message_t msg) 
{
    struct ist8303_i2c_data *obj = i2c_get_clientdata(client);
    MSE_FUN();

    if (msg.event == PM_EVENT_SUSPEND)
    {   
        if (ist830x_data.mode == IST8303_NORMAL_MODE)
        {
            cancel_delayed_work_sync(&ist_get_raw_data_work);    
            
            IST8303_Chipset_Enable(ist830x_data.mode, 0);
        }
        ist8303_power(obj->hw, 0);
    }
    return 0;
}
/*----------------------------------------------------------------------------*/
static int ist8303_resume(struct i2c_client *client)
{
    int err;
    struct ist8303_i2c_data *obj = i2c_get_clientdata(client);
    MSE_FUN();

    ist8303_power(obj->hw, 1);
    if (ist830x_data.mode == IST8303_NORMAL_MODE)
    {
        if ((err = IST8303_Chipset_Enable(ist830x_data.mode, 1)) !=0 )
        {
            MSE_ERR("initialize client fail!!\n");
            return err;        
        }
        if(atomic_read(&open_flag) != 0)
        	schedule_delayed_work(&ist_get_raw_data_work, msecs_to_jiffies(ODR_DELAY_TIME_MS));
    }

    return 0;
}
/*----------------------------------------------------------------------------*/
//#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void ist8303_early_suspend(struct early_suspend *h) 
{
    struct ist8303_i2c_data *obj = container_of(h, struct ist8303_i2c_data, early_drv);   
    MSE_FUN();    

    if (NULL == obj)
    {
        MSE_ERR("null pointer!!\n");
        return;
    }

    if (ist830x_data.mode == IST8303_NORMAL_MODE)
    {
        cancel_delayed_work_sync(&ist_get_raw_data_work);    
        
        IST8303_Chipset_Enable(ist830x_data.mode, 0);
    }
    
    ist8303_power(obj->hw, 0);
}
/*----------------------------------------------------------------------------*/
static void ist8303_late_resume(struct early_suspend *h)
{
    struct ist8303_i2c_data *obj = container_of(h, struct ist8303_i2c_data, early_drv);         
    int err;
    MSE_FUN();

    if(NULL == obj)
    {
        MSE_ERR("null pointer!!\n");
        return;
    }

    ist8303_power(obj->hw, 1);
    if (ist830x_data.mode == IST8303_NORMAL_MODE)
    {
        if ((err = IST8303_Chipset_Enable(ist830x_data.mode, 1)) !=0 )
        {
            MSE_ERR("initialize client fail!!\n");
            return;        
        }
        if(atomic_read(&open_flag) != 0)
        	schedule_delayed_work(&ist_get_raw_data_work, msecs_to_jiffies(ODR_DELAY_TIME_MS));
    }
}
/*----------------------------------------------------------------------------*/
//#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/
/*
static int ist8303_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info) 
{    
    strcpy(info->type, IST8303_DEV_NAME);
    return 0;
}
*/

/*----------------------------------------------------------------------------*/
static int ist8303_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct i2c_client *new_client;
    struct ist8303_i2c_data *data;
    int err = 0;
#ifdef IST8303_M_NEW_ARCH
//    struct mag_drv_obj sobj_m, sobj_o, sobj_uncali_m;
    struct mag_control_path ctl={0};
    struct mag_data_path mag_data={0}; 
#else
    struct hwmsen_object sobj_m, sobj_o, sobj_uncali_m;
#endif
    
#ifdef SOFT_GYRO
    struct hwmsen_object sobj_gy, sobj_rv;
#ifdef REPLACE_ANDROID_VIRTUAL_SENSOR
    struct hwmsen_object sobj_gra, sobj_la;
#endif //#ifdef REPLACE_ANDROID_VIRTUAL_SENSOR
#endif

    if (!(data = kmalloc(sizeof(struct ist8303_i2c_data), GFP_KERNEL)))
    {
        err = -ENOMEM;
        goto exit;
    }
    memset(data, 0, sizeof(struct ist8303_i2c_data));

    data->hw = get_cust_mag_hw();
    if((err = hwmsen_get_convert(data->hw->direction, &data->cvt)))
    {
        MSE_ERR("invalid direction: %d\n", data->hw->direction);
        goto exit;
    }
    
    atomic_set(&data->layout, data->hw->direction);
    atomic_set(&data->trace, 0);
    atomic_set(&ist830x_data_ready, 0);
    init_waitqueue_head(&data_ready_wq);
    init_waitqueue_head(&open_wq);

    data->client = client;
    new_client = data->client;
    i2c_set_clientdata(new_client, data);
    
    ist8303_i2c_client = new_client;

    //write_lock(&ist830x_data.lock);
    ist830x_data.mode = IST8303_FORCE_MODE;//IST8303_FORCE_MODE, IST8303_NORMAL_MODE
    //write_unlock(&ist830x_data.lock);

    INIT_DELAYED_WORK(&ist_get_raw_data_work, ist830x_prepare_raw_data);

    if((err = IST8303_Chipset_Init(ist830x_data.mode)))
    {
        printk("ist8303 probe fail \n");
		goto exit_kfree;
    }

    /* Register sysfs attribute */
#ifdef IST8303_M_NEW_ARCH
    if((err = ist8303_create_attr(&(ist8303_init_info.platform_diver_addr->driver))))
#else
    if((err = ist8303_create_attr(&ist_sensor_driver.driver)))
#endif
    {
        MSE_ERR("create attribute err = %d\n", err);
        goto exit_sysfs_create_group_failed;
    }

    if((err = misc_register(&ist8303_device)))
    {
        MSE_ERR("ist8303_device register failed\n");
        goto exit_misc_device_register_failed;
    }    


    
#ifndef IST8303_M_NEW_ARCH   
    sobj_m.self = data;
    sobj_m.polling = 1;

    sobj_m.sensor_operate = ist8303_operate;
    if((err = hwmsen_attach(ID_MAGNETIC, &sobj_m)))
    {
        MSE_ERR("attach fail = %d\n", err);
        goto exit_kfree;
    }
    
    sobj_o.self = data;
    sobj_o.polling = 1;
    sobj_o.sensor_operate = ist8303_orientation_operate;
    if((err = hwmsen_attach(ID_ORIENTATION, &sobj_o)))
    {
        MSE_ERR("attach fail = %d\n", err);
        goto exit_kfree;
    }
    
#else
    ctl.m_enable = ist8303_m_enable;
    ctl.m_set_delay  = ist8303_m_set_delay;
    ctl.m_open_report_data = ist8303_m_open_report_data;
    ctl.o_enable = ist8303_o_enable;
    ctl.o_set_delay  = ist8303_o_set_delay;
    ctl.o_open_report_data = ist8303_o_open_report_data;
    ctl.is_report_input_direct = false;
	ctl.is_support_batch = data->hw->is_batch_supported;
    
    err = mag_register_control_path(&ctl);
    if(err)
    {
        MAG_ERR("register mag control path err\n");
        goto exit_kfree;
    }

    mag_data.div_m = 100;
    mag_data.div_o = ORIENTATION_ACCURACY_RATE;

    mag_data.get_data_o = ist8303_o_get_data;
    mag_data.get_data_m = ist8303_m_get_data;

    err = mag_register_data_path(&mag_data);
    if(err)
    {
        MAG_ERR("register data control path err\n");
        goto exit_kfree;
    }
#endif
    
#ifdef SOFT_GYRO  
    sobj_gy.self = data;
    sobj_gy.polling = 1;
    sobj_gy.sensor_operate = ist8303_gyro_operate;
    if((err = hwmsen_attach(ID_GYROSCOPE, &sobj_gy)))
    {
        printk(KERN_ERR "attach fail = %d\n", err);
        goto exit_kfree;
    }

    sobj_rv.self = data;
    sobj_rv.polling = 1;
    sobj_rv.sensor_operate = ist8303_rotation_vector_operate;
    if((err = hwmsen_attach(ID_ROTATION_VECTOR, &sobj_rv)))
    {
        printk(KERN_ERR "attach fail = %d\n", err);
        goto exit_kfree;
    }

#ifdef REPLACE_ANDROID_VIRTUAL_SENSOR
    sobj_gra.self = data;
    sobj_gra.polling = 1;
    sobj_gra.sensor_operate = ist8303_gravity_operate;
    if((err = hwmsen_attach(ID_GRAVITY, &sobj_gra)))
    {
        printk(KERN_ERR "attach fail = %d\n", err);
        goto exit_kfree;
    }

    sobj_la.self = data;
    sobj_la.polling = 1;
    sobj_la.sensor_operate = ist8303_linear_acceleration_operate;
    if((err = hwmsen_attach(ID_LINEAR_ACCELERATION, &sobj_la)))
    {
        printk(KERN_ERR "attach fail = %d\n", err);
        goto exit_kfree;
    }
#endif //#ifdef REPLACE_ANDROID_VIRTUAL_SENSOR
#endif //#ifdef SOFT_GYRO  

#ifndef IST8303_M_NEW_ARCH
#ifdef ID_MAGNETIC_UNCALIBRATED
    sobj_uncali_m.self = data;
    sobj_uncali_m.polling = 1;
    sobj_uncali_m.sensor_operate = ist8303_uncalibrated_operate;
    if((err = hwmsen_attach(ID_MAGNETIC_UNCALIBRATED, &sobj_uncali_m)))
    {
        MSE_ERR("attach fail = %d\n", err);
    }
#endif //#ifdef ID_MAGNETIC_UNCALIBRATED
#endif //#ifndef IST8303_M_NEW_ARCH

#if CONFIG_HAS_EARLYSUSPEND
    data->early_drv.level    = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
    data->early_drv.suspend  = ist8303_early_suspend,
    data->early_drv.resume   = ist8303_late_resume,    
    register_early_suspend(&data->early_drv);
#endif
#ifdef IST8303_M_NEW_ARCH
    ist8303_init_flag = 0;
#endif
    MSE_LOG("%s: OK\n", __func__);
    return 0;

    exit_sysfs_create_group_failed:   
    exit_init_failed:
    exit_misc_device_register_failed:
    exit_kfree:
    kfree(data);
    exit:
#ifdef IST8303_M_NEW_ARCH
    ist8303_init_flag = -1;
#endif
    MSE_ERR("%s: err = %d\n", __func__, err);
    return err;
}
/*----------------------------------------------------------------------------*/
static int ist8303_i2c_remove(struct i2c_client *client)
{
    int err;    
#ifdef IST8303_M_NEW_ARCH
    if((err = ist8303_delete_attr(&(ist8303_init_info.platform_diver_addr->driver))))
#else
    if((err = ist8303_delete_attr(&ist_sensor_driver.driver)))
#endif
    {
        MSE_ERR("ist8303_delete_attr fail: %d\n", err);
    }
    
    ist8303_i2c_client = NULL;
    i2c_unregister_device(client);
    kfree(i2c_get_clientdata(client));  
    misc_deregister(&ist8303_device);    
    return 0;
}

/*----------------------------------------------------------------------------*/
#ifdef IST8303_M_NEW_ARCH
static int ist_gpio_rst_config(void)
{
#if !defined(GPIO_COMPASS_RST_PIN)
#define GPIO_COMPASS_RST_PIN (0x80000000 | 168)
#else
#error "leechee added to make sure GPIO_COMPASS_RST_PIN is defined corrected!"
#endif
    int ret = 0;    
#ifdef GPIO_COMPASS_RST_PIN
    printk("akm8963 reset pin is used for this project\n");
    ret = mt_set_gpio_mode(GPIO_COMPASS_RST_PIN, GPIO_MODE_00);
    if(ret < 0)
    {
        printk(KERN_ERR "set gpio mode error\n");
    }
    ret = mt_set_gpio_dir(GPIO_COMPASS_RST_PIN, GPIO_DIR_OUT);
    if(ret < 0)
    {
        printk(KERN_ERR "set gpio dir error\n");
    }
    ret = mt_set_gpio_out(GPIO_COMPASS_RST_PIN, GPIO_OUT_ZERO);
    if(ret < 0)
    {
        printk(KERN_ERR "set gpio out value error\n");
    }
    mdelay(15);
    ret = mt_set_gpio_out(GPIO_COMPASS_RST_PIN, 1);
    if(ret < 0)
    {
        printk(KERN_ERR "set gpio out value error\n");
    }
#endif
    return ret;
}

static int ist8303_local_init(void)
{
    struct mag_hw *hw = get_cust_mag_hw();

    ist8303_power(hw, 1);  
    
    ist_gpio_rst_config();
    
    init_rwsem(&ist8303mid_data.ctrllock);
    init_rwsem(&ist8303mid_data.datalock);
    init_rwsem(&ist830x_data.lock);
    memset(&ist8303mid_data.controldata[0], 0, sizeof(int)*10);    
    ist8303mid_data.controldata[0] =    20;  // Loop Delay
    ist8303mid_data.controldata[1] =     0;  // Run   
    ist8303mid_data.controldata[2] =     0;  // Disable Start-AccCali
    ist8303mid_data.controldata[3] =     1;  // Enable Start-Cali
    ist8303mid_data.controldata[4] =   350;  // MW-Timout
    ist8303mid_data.controldata[5] =    10;  // MW-IIRStrength_M
    ist8303mid_data.controldata[6] =    10;  // MW-IIRStrength_G   
    ist8303mid_data.controldata[7] =     0;  // Active Sensors
    ist8303mid_data.controldata[8] =     ROTATION_VECTOR_RESPONSE_MODE; // Rotation Vector response mode
    ist8303mid_data.controldata[9] =     0;  // Wait for define   
    atomic_set(&dev_open_count, 0);
    //ist8303_force[0] = hw->i2c_num;

    if(i2c_add_driver(&ist8303_i2c_driver))
    {
        MSE_ERR("add driver error\n");
        return -1;
    }
    
    if(-1 == ist8303_init_flag)
    {
        MSE_ERR("%s failed!\n",__func__);
       return -1;
    }
    return 0;
}

static int ist8303_remove(void)
{
    struct mag_hw *hw = get_cust_mag_hw();
    
    MSE_FUN(f);  
    ist8303_power(hw, 0);   
    atomic_set(&dev_open_count, 0); 
    i2c_del_driver(&ist8303_i2c_driver);

    return 0;
}
#endif

/*----------------------------------------------------------------------------*/
#ifndef IST8303_M_NEW_ARCH
static int ist_probe(struct platform_device *pdev) 
{
    struct mag_hw *hw = get_cust_mag_hw();

    ist8303_power(hw, 1);    
    init_rwsem(&ist8303mid_data.ctrllock);
    init_rwsem(&ist8303mid_data.datalock);
    init_rwsem(&ist830x_data.lock);
    memset(&ist8303mid_data.controldata[0], 0, sizeof(int)*10);    
    ist8303mid_data.controldata[0] =    20;  // Loop Delay
    ist8303mid_data.controldata[1] =     0;  // Run   
    ist8303mid_data.controldata[2] =     0;  // Disable Start-AccCali
    ist8303mid_data.controldata[3] =     1;  // Enable Start-Cali
    ist8303mid_data.controldata[4] =   350;  // MW-Timout
    ist8303mid_data.controldata[5] =    10;  // MW-IIRStrength_M
    ist8303mid_data.controldata[6] =    10;  // MW-IIRStrength_G   
    ist8303mid_data.controldata[7] =     0;  // Active Sensors
    ist8303mid_data.controldata[8] =     ROTATION_VECTOR_RESPONSE_MODE; // Rotation Vector response mode
    ist8303mid_data.controldata[9] =     0;  // Wait for define   
    atomic_set(&dev_open_count, 0);
    //ist8303_force[0] = hw->i2c_num;

    if(i2c_add_driver(&ist8303_i2c_driver))
    {
        MSE_ERR("add driver error\n");
        return -1;
    } 
    return 0;
}
/*----------------------------------------------------------------------------*/
static int ist_remove(struct platform_device *pdev)
{
    struct mag_hw *hw = get_cust_mag_hw();

    MSE_FUN();    
    ist8303_power(hw, 0);    
    atomic_set(&dev_open_count, 0);  
    i2c_del_driver(&ist8303_i2c_driver);
    return 0;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id msensor_of_match[] = {
	{ .compatible = "mediatek,msensor", },
	{},
};
#endif
static struct platform_driver ist_sensor_driver = {
    .probe      = ist_probe,
    .remove     = ist_remove,    
    .driver     = {
        .name  = "msensor",
//      .owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = msensor_of_match,
#endif

    }
};
#endif

/*----------------------------------------------------------------------------*/
static int __init ist8303_init(void)
{
    struct mag_hw *hw;
    
    hw = get_cust_mag_hw();

    i2c_register_board_info(hw->i2c_num, &i2c_ist8303, 1);
    
#ifdef IST8303_M_NEW_ARCH
    mag_driver_add(&ist8303_init_info);
#else
    if(platform_driver_register(&ist_sensor_driver))
    {
        MSE_ERR("failed to register driver");
        return -ENODEV;
    }
#endif
        
    return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit ist8303_exit(void)
{
    MSE_FUN();
#ifndef IST8303_M_NEW_ARCH
    platform_driver_unregister(&ist_sensor_driver);
#endif
}
/*----------------------------------------------------------------------------*/
module_init(ist8303_init);
module_exit(ist8303_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("iSentek");
MODULE_DESCRIPTION("IST8303 M-Sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
