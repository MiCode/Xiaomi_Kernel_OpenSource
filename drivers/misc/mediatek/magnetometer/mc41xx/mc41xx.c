/*****************************************************************************
 *
 * Copyright (c) 2013 mCube, Inc.  All rights reserved.
 *
 * This source is subject to the mCube Software License.
 * This software is protected by Copyright and the information and source code
 * contained herein is confidential. The software including the source code
 * may not be copied and the information contained herein may not be used or
 * disclosed except with the written permission of mCube Inc.
 *
 * All other rights reserved.
 *
 * This code and information are provided "as is" without warranty of any
 * kind, either expressed or implied, including but not limited to the
 * implied warranties of merchantability and/or fitness for a
 * particular purpose.
 *
 * The following software/firmware and/or related documentation ("mCube Software")
 * have been modified by mCube Inc. All revisions are subject to any receiver's
 * applicable license agreements with mCube Inc.
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
 *
 *****************************************************************************/
 
/*******************************************************************************
 *** INCLUDE FILES
 *******************************************************************************/
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
#include <linux/hwmsen_helper.h>
#include <linux/sensors_io.h>

#include <cust_mag.h>
#include "mc41xx.h"

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>


/*******************************************************************************
 *** CONSTANT / DEFINITION
 *******************************************************************************/
/**************************
 *** CONFIGURATION
 **************************/
#define MCMAG_DRV_SYSCFG_LINUX_KERNEL_2_6_X    (1)
#define MCMAG_DRV_SYSCFG_LINUX_KERNEL_3_0_X    (2)

#define MCMAG_DRV_SYSCFG_KERNEL_VERSION    (MCMAG_DRV_SYSCFG_LINUX_KERNEL_3_0_X)

#define MCMAG_DRV_SUPPORT_CONCURRENCY_PROTECTION

#define MCMAG_DRV_DRVCFG_SINGLESHOT

//#define MCMAG_DRV_SUPPORT_VGYRO

/**************************
 *** DEBUG
 **************************/
#if 0
    #define MSE_TAG                  "[MSENSOR] "
    #define MSE_FUN(f)               printk(KERN_INFO MSE_TAG "[%s]", __FUNCTION__)
    #define MSE_ERR(fmt, args...)    printk(KERN_ERR  MSE_TAG "[%s] %d: "fmt, __FUNCTION__, __LINE__, ##args)
    #define MSE_LOG(fmt, args...)    printk(KERN_INFO MSE_TAG fmt, ##args)
    #define MSE_VER(fmt, args...)    ((void)0)
#else
    #define MSE_TAG
    #define MSE_FUN(f)
    #define MSE_ERR(fmt, args...)    do {} while (0)
    #define MSE_LOG(fmt, args...)    do {} while (0)
    #define MSE_VER(fmt, args...)    do {} while (0)
#endif

/**************************
 *** MTK
 **************************/
#define POWER_NONE_MACRO    MT65XX_POWER_NONE

/**************************
 *** mCube
 **************************/
#define MCMAG_DRV_RETCODE_OK     0

#define MCMAG_DRV_DISABLE    0
#define MCMAG_DRV_ENABLE     1

#define MCMAG_DRV_AXIS_X      0
#define MCMAG_DRV_AXIS_Y      1
#define MCMAG_DRV_AXIS_Z      2
#define MCMAG_DRV_AXES_NUM    3

#define MCMAG_DRV_DATA_LEN       6
#define MCMAG_DRV_MAX_MSG_LEN    10

#define MCMAG_DRV_DEVICE_NAME       "MCMAG"
#define MCMAG_DRV_DRIVER_VERSION    "1.1.0"

#define MCMAG_DRV_DAEMON_NAME       "mc41xxd"

#define MCMAG_DRV_DEFAULT_SAMPLE_DELAY    (10)

/*******************************************************************************
 *** DATA TYPE & STRUCTURE DEFINITION
 *******************************************************************************/
struct _mcmag_i2c_data
{
    struct i2c_client       *ptClient;
    struct mag_hw           *ptHW;
    struct hwmsen_convert    tCvt;

    atomic_t    tLayout;
    atomic_t    tTrace;
    
    #if defined(CONFIG_HAS_EARLYSUSPEND)    
        struct early_suspend    tDrv_EarlySuspend;
    #endif
};

struct _mcmagmid_data
{
    // spin-lock to protect data and control block
    rwlock_t    tDataLock;
    rwlock_t    tControlLock;    

    // data block (Orientaion Sensor)
    int    nOrientation_Heading;
    int    nOrientation_Roll;
    int    nOrientation_Pitch;

    // data block (Accelemeter Sensor)
    int    nAccel_X;
    int    nAccel_Y;
    int    nAccel_Z;

    // data block (Magnetometer Sensor)
    int    nMag_X;
    int    nMag_Y;
    int    nMag_Z;
    int    nMag_Status;

    #ifdef MCMAG_DRV_SUPPORT_VGYRO
        // data block (Gyro Sensor)
        int    nGyro_X;
        int    nGyro_Y;
        int    nGyro_Z;
    #endif

    // control data between middleware and driver
    int    naMidControlData[10];
};

/*******************************************************************************
 *** STATIC FUNCTION DECLARATION
 *******************************************************************************/
static int    mcmag_i2c_probe (struct i2c_client *ptClient, const struct i2c_device_id *id); 
static int    mcmag_i2c_remove(struct i2c_client *ptClient);

#if (MCMAG_DRV_SYSCFG_KERNEL_VERSION == MCMAG_DRV_SYSCFG_LINUX_KERNEL_2_6_X)
    static int    mcmag_i2c_detect(struct i2c_client *ptClient, int kind, struct i2c_board_info *info);
#endif

/*******************************************************************************
 *** STATIC VARIABLE & CONTROL BLOCK DECLARATION
 *******************************************************************************/
static DECLARE_WAIT_QUEUE_HEAD(wqMcmag_Open);
static DECLARE_WAIT_QUEUE_HEAD(wqMcmag_DataReady);

#ifdef MCMAG_DRV_SUPPORT_CONCURRENCY_PROTECTION
    static struct semaphore    s_tSemaProtect;
#endif

static atomic_t    s_tMcmag_DeviceOpenCount       = ATOMIC_INIT(0);
static atomic_t    s_tMcmag_FlagOpen              = ATOMIC_INIT(0);
static atomic_t    s_tMcmag_FlagMagSensor         = ATOMIC_INIT(0);
static atomic_t    s_tMcmag_FlagOrientationSensor = ATOMIC_INIT(0);

#ifdef MCMAG_DRV_SUPPORT_VGYRO
    static atomic_t    s_tMcmag_FlagGyroSensor = ATOMIC_INIT(0);
#endif

static struct _mcmagmid_data     s_tMcmag_MiddlewareData;
static struct platform_driver    s_tMcmag_SensorDriver;

static struct i2c_client            *s_ptMcmag_i2c_client = NULL;
static const struct i2c_device_id    s_tMcmag_i2c_id[]    = { {MCMAG_DRV_DEVICE_NAME, 0}, {} };

#if (MCMAG_DRV_SYSCFG_KERNEL_VERSION == MCMAG_DRV_SYSCFG_LINUX_KERNEL_2_6_X)
    static unsigned short                    s_tMcmag_force[]   = { 0x00, MCMAG_I2C_ADDR, I2C_CLIENT_END, I2C_CLIENT_END };
    static const unsigned short *const       s_ptMcmag_forces[] = { s_tMcmag_force, NULL };
    static struct i2c_client_address_data    s_tMcmag_addr_data = { .forces = s_ptMcmag_forces, };
#elif (MCMAG_DRV_SYSCFG_KERNEL_VERSION == MCMAG_DRV_SYSCFG_LINUX_KERNEL_3_0_X)
    static struct i2c_board_info __initdata    s_tMcmag_i2c_board_info = { I2C_BOARD_INFO(MCMAG_DRV_DEVICE_NAME, (MCMAG_I2C_ADDR >> 1)) };
#else
    #error "MCMAG_DRV_SYSCFG_KERNEL_VERSION should be correctly configured..."
#endif

//====================================================================================================
static struct i2c_driver    s_tMcmag_i2c_driver = {
                                                      .driver = {
                                                                    #if (MCMAG_DRV_SYSCFG_KERNEL_VERSION == MCMAG_DRV_SYSCFG_LINUX_KERNEL_2_6_X)
                                                                        .owner = THIS_MODULE,
                                                                    #endif

                                                                    .name  = MCMAG_DRV_DEVICE_NAME,
                                                                },
   
                                                      .probe  = mcmag_i2c_probe,
                                                      .remove = mcmag_i2c_remove,

                                                      #if (MCMAG_DRV_SYSCFG_KERNEL_VERSION == MCMAG_DRV_SYSCFG_LINUX_KERNEL_2_6_X)
                                                          .detect = mcmag_i2c_detect,
                                                      #endif

                                                      #if !defined(CONFIG_HAS_EARLYSUSPEND)
                                                          .suspend = mcmag_suspend,
                                                          .resume  = mcmag_resume,
                                                      #endif 
   
                                                      .id_table = s_tMcmag_i2c_id,

                                                      #if (MCMAG_DRV_SYSCFG_KERNEL_VERSION == MCMAG_DRV_SYSCFG_LINUX_KERNEL_2_6_X)
                                                          .address_data = &s_tMcmag_addr_data,
                                                      #endif
                                                  };

/*******************************************************************************
 *** MACROs
 *******************************************************************************/
#ifdef MCMAG_DRV_SUPPORT_CONCURRENCY_PROTECTION
    #define MC41XX_MUTEX_INIT()   sema_init(&s_tSemaProtect, 1)

    #define MC41XX_MUTEX_LOCK()                             \
                if (down_interruptible(&s_tSemaProtect))    \
                    return (-ERESTARTSYS)

    #define MC41XX_MUTEX_UNLOCK()   up(&s_tSemaProtect)
#else
    #define MC41XX_MUTEX_INIT()      do {} while (0)
    #define MC41XX_MUTEX_LOCK()      do {} while (0)
    #define MC41XX_MUTEX_UNLOCK()    do {} while (0)
#endif

/*******************************************************************************
 *** FUNCTIONs
 *******************************************************************************/

/*****************************************
 *** mCube_SensorWrite
 *****************************************/
int    mCube_SensorWrite(u8 *pbDataBuf, int nDataLength)
{
    if ((NULL == pbDataBuf) || (nDataLength < 2))
        return (-EFAULT);

    if (0 > hwmsen_write_block(s_ptMcmag_i2c_client, pbDataBuf[0], &pbDataBuf[1], (nDataLength - 1)))
    {
        MSE_ERR("[%s] ERR: i2c write block fail\n", __FUNCTION__);
        return (-EIO);
    }

    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** mCube_SensorRead
 *****************************************/
int    mCube_SensorRead(u8 bCmd, int nRcvBufLength, u8 *pbRcvBuf)
{
    if ((NULL == pbRcvBuf) || (nRcvBufLength < 1))
        return (-EFAULT);

    if (0 > hwmsen_read_block(s_ptMcmag_i2c_client, bCmd, pbRcvBuf, nRcvBufLength))
    {
        MSE_ERR("[%s] ERR: i2c read fail\n", __FUNCTION__);
        return (-EIO);
    }

    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** MCMAG_DumpRegisters
 *****************************************/
#if 0
static void    MCMAG_DumpRegisters(void)
{
    u8     _baRegData[8];
    u32    _dwIndex;

    for (_dwIndex = 0; _dwIndex < 0x40; _dwIndex++)
    {
        if (0 == (_dwIndex % 8))
            memset(_baRegData, 0, 8);

        mCube_SensorRead(s_tMcmag_MiddlewareData.tControl.pDeviceID, _dwIndex, sizeof(u8), &_baRegData[(_dwIndex % 8)]);

        if (7 == (_dwIndex % 8))
        {
            MSE_ERR("[0x%X] 0x%X, [0x%X] 0x%X, [0x%X] 0x%X, [0x%X] 0x%X, [0x%X] 0x%X, [0x%X] 0x%X, [0x%X] 0x%X, [0x%X] 0x%X\n",
                    (_dwIndex - 7), _baRegData[0], (_dwIndex - 6), _baRegData[1],
                    (_dwIndex - 5), _baRegData[2], (_dwIndex - 4), _baRegData[3],
                    (_dwIndex - 3), _baRegData[4], (_dwIndex - 2), _baRegData[5],
                    (_dwIndex - 1), _baRegData[6], (_dwIndex    ), _baRegData[7] );
        }
    }
}
#endif

/*****************************************
 *** _mCube_ClearMessageBuffer
 *****************************************/
static void    _mCube_ClearMessageBuffer(u8 *pbMsgBuffer)
{
    memset(pbMsgBuffer, 0, MCMAG_DRV_MAX_MSG_LEN);
}

/*****************************************
 *** MCMAG_ConfigRegControl1_SetContMode
 *****************************************/
int    MCMAG_ConfigRegControl1_SetContMode(void)
{
    u8    _baMsgBuffer[MCMAG_DRV_MAX_MSG_LEN];
    u8   *_pbMsgBuf = _baMsgBuffer;

    MSE_FUN();

    _mCube_ClearMessageBuffer(_pbMsgBuf);
    
    *_pbMsgBuf = MCMAG_REG_CTRL1;
    
    _pbMsgBuf++;

    *_pbMsgBuf = (MCMAG_CTRL1_POWER_MODE_ACTIVE | MCMAG_CTRL1_DAA_RATE_100Hz | MCMAG_CTRL1_STATE_NORMAL);

    _pbMsgBuf++;
    
    return (mCube_SensorWrite(_baMsgBuffer, (_pbMsgBuf - _baMsgBuffer)));
}

/*****************************************
 *** MCMAG_ConfigRegControl1_SetSingleShotMode
 *****************************************/
int    MCMAG_ConfigRegControl1_SetSingleShotMode(void)
{
    u8    _baMsgBuffer[MCMAG_DRV_MAX_MSG_LEN];
    u8   *_pbMsgBuf = _baMsgBuffer;

    MSE_FUN();

    _mCube_ClearMessageBuffer(_pbMsgBuf);
    
    *_pbMsgBuf = MCMAG_REG_CTRL1;
    
    _pbMsgBuf++;

    *_pbMsgBuf = (MCMAG_CTRL1_POWER_MODE_ACTIVE | MCMAG_CTRL1_DAA_RATE_100Hz | MCMAG_CTRL1_STATE_FORCE);

    _pbMsgBuf++;
    
    return (mCube_SensorWrite(_baMsgBuffer, (_pbMsgBuf - _baMsgBuffer)));
}

/*****************************************
 *** MCMAG_ConfigRegControl1_SetStandbyMode
 *****************************************/
int    MCMAG_ConfigRegControl1_SetStandbyMode(void)
{
    u8    _baMsgBuffer[MCMAG_DRV_MAX_MSG_LEN];
    u8   *_pbMsgBuf = _baMsgBuffer;

    MSE_FUN();

    _mCube_ClearMessageBuffer(_pbMsgBuf);
    
    *_pbMsgBuf = MCMAG_REG_CTRL1;
    
    _pbMsgBuf++;

    *_pbMsgBuf = MCMAG_CTRL1_POWER_MODE_STANDBY;

    _pbMsgBuf++;
    
    return (mCube_SensorWrite(_baMsgBuffer, (_pbMsgBuf - _baMsgBuffer)));
}

/*****************************************
 *** MCMAG_ConfigRegControl3_EnableSoftReset
 *****************************************/
int    MCMAG_ConfigRegControl3_EnableSoftReset(void)
{
    u8    _baMsgBuffer[MCMAG_DRV_MAX_MSG_LEN];
    u8   *_pbMsgBuf = _baMsgBuffer;

    MSE_FUN();

    _mCube_ClearMessageBuffer(_pbMsgBuf);
    
    *_pbMsgBuf = MCMAG_REG_CTRL3;
    
    _pbMsgBuf++;

    *_pbMsgBuf = MCMAG_CTRL3_ENABLE_SOFT_RESET;

    _pbMsgBuf++;
    
    return (mCube_SensorWrite(_baMsgBuffer, (_pbMsgBuf - _baMsgBuffer)));
}

/*****************************************
 *** MCMAG_ConfigRegControl3_SetForceState
 *****************************************/
int    MCMAG_ConfigRegControl3_SetForceState(void)
{
    u8    _baMsgBuffer[MCMAG_DRV_MAX_MSG_LEN];
    u8   *_pbMsgBuf = _baMsgBuffer;

    MSE_FUN();

    _mCube_ClearMessageBuffer(_pbMsgBuf);
    
    *_pbMsgBuf = MCMAG_REG_CTRL3;
    
    _pbMsgBuf++;

    *_pbMsgBuf = MCMAG_CTRL3_SET_FORCE_STATE;

    _pbMsgBuf++;
    
    return (mCube_SensorWrite(_baMsgBuffer, (_pbMsgBuf - _baMsgBuffer)));
}

/*****************************************
 *** MCMAG_ConfigRegControl3_EnableSoftReset
 *****************************************/
int    MCMAG_ConfigRegControl4_SetDynamicRange(E_MC41XX_DynamicRangeConfig eDynamicRangeConfig)
{
    u8    _baMsgBuffer[MCMAG_DRV_MAX_MSG_LEN];
    u8   *_pbMsgBuf = _baMsgBuffer;

    MSE_FUN();

    if (eDynamicRangeConfig > E_MC41XX_DYNAMIC_RANGE_15bit)
        MSE_ERR("ERR: invalid argument: %d\n", eDynamicRangeConfig);

    _mCube_ClearMessageBuffer(_pbMsgBuf);
    
    *_pbMsgBuf = MCMAG_REG_CTRL4;
    
    _pbMsgBuf++;

    *_pbMsgBuf = MCMAG_CTRL4_MUST_DEFAULT_SETTING;

    if (E_MC41XX_DYNAMIC_RANGE_15bit == eDynamicRangeConfig)
        *_pbMsgBuf |= MCMAG_CTRL4_DYNAMIC_RANGE_15bit;

    _pbMsgBuf++;
    
    return (mCube_SensorWrite(_baMsgBuffer, (_pbMsgBuf - _baMsgBuffer)));
}

/*****************************************
 *** MCMAG_StartSingleRead
 *****************************************/
int    MCMAG_StartSingleRead(void)
{
    int    _nRetCode = MCMAG_DRV_RETCODE_OK;

    MSE_FUN();

    _nRetCode  = MCMAG_ConfigRegControl3_SetForceState();
    _nRetCode |= MCMAG_ConfigRegControl4_SetDynamicRange(E_MC41XX_DYNAMIC_RANGE_15bit);
    _nRetCode |= MCMAG_ConfigRegControl1_SetSingleShotMode();

    return (_nRetCode);
}

/*****************************************
 *** MCMAG_Start
 *****************************************/
int    MCMAG_Start(void)
{
    int    _nRetCode = MCMAG_DRV_RETCODE_OK;

    MSE_FUN();

    #ifdef MCMAG_DRV_DRVCFG_SINGLESHOT
        _nRetCode = MCMAG_StartSingleRead();
    #else
        _nRetCode = MCMAG_ConfigRegControl1_SetContMode();
    #endif

    return (_nRetCode);
}

/*****************************************
 *** MCMAG_Stop
 *****************************************/
int    MCMAG_Stop(void)
{
    MSE_FUN();

    return (MCMAG_ConfigRegControl1_SetStandbyMode());
}

/*****************************************
 *** MCMAG_ReadMagSensorData
 *****************************************/
static int    MCMAG_ReadMagSensorData(char *pszBuf, int nBufSize)
{
    u8     _bDataReadyStatus                    = 0;
    u8     _baDataBuffer[MCMAG_DRV_DATA_LEN]    = { 0 };
    s16    _waRawData[MCMAG_DRV_AXES_NUM]       = { 0 };
    s32    _naConvertedData[MCMAG_DRV_AXES_NUM] = { 0 };
    int    _naFinalData[MCMAG_DRV_AXES_NUM]     = { 0 };
    int    _nWaitDataReadyCount                 = 0;

    struct _mcmag_i2c_data   *_pt_i2c_data = i2c_get_clientdata(s_ptMcmag_i2c_client);

    MSE_FUN();

    MC41XX_MUTEX_LOCK();
    MCMAG_Start();
    MC41XX_MUTEX_UNLOCK();

    while (1)
    {
        _nWaitDataReadyCount++;

        mCube_SensorRead(MCMAG_REG_STATUS, sizeof(_bDataReadyStatus), &_bDataReadyStatus);

        if (_bDataReadyStatus & MCMAG_STATUS_DATA_READY_DETECTED)
        {
            MSE_LOG("DATA READY: %d\n", _nWaitDataReadyCount);
            break;
        }
    
        if (_nWaitDataReadyCount > 128)
        {
            MSE_LOG("ERR: DataReady never detected...\n");
            break;
        }
    }

    mCube_SensorRead(MCMAG_REG_XOUT, sizeof(_baDataBuffer), _baDataBuffer);

    _waRawData[MCMAG_DRV_AXIS_X] = (_baDataBuffer[0] | (_baDataBuffer[1] << 8));
    _waRawData[MCMAG_DRV_AXIS_Y] = (_baDataBuffer[2] | (_baDataBuffer[3] << 8));
    _waRawData[MCMAG_DRV_AXIS_Z] = (_baDataBuffer[4] | (_baDataBuffer[5] << 8));

    MSE_LOG(">>>>>> %d %d %d\n", _waRawData[MCMAG_DRV_AXIS_X], _waRawData[MCMAG_DRV_AXIS_Y], _waRawData[MCMAG_DRV_AXIS_Z]);

    _naConvertedData[MCMAG_DRV_AXIS_X] = (int)(_waRawData[MCMAG_DRV_AXIS_X] * 150);
    _naConvertedData[MCMAG_DRV_AXIS_Y] = (int)(_waRawData[MCMAG_DRV_AXIS_Y] * 150);
    _naConvertedData[MCMAG_DRV_AXIS_Z] = (int)(_waRawData[MCMAG_DRV_AXIS_Z] * 150);

    _naFinalData[_pt_i2c_data->tCvt.map[MCMAG_DRV_AXIS_X]] = (_pt_i2c_data->tCvt.sign[MCMAG_DRV_AXIS_X] * _naConvertedData[MCMAG_DRV_AXIS_X]);
    _naFinalData[_pt_i2c_data->tCvt.map[MCMAG_DRV_AXIS_Y]] = (_pt_i2c_data->tCvt.sign[MCMAG_DRV_AXIS_Y] * _naConvertedData[MCMAG_DRV_AXIS_Y]);
    _naFinalData[_pt_i2c_data->tCvt.map[MCMAG_DRV_AXIS_Z]] = (_pt_i2c_data->tCvt.sign[MCMAG_DRV_AXIS_Z] * _naConvertedData[MCMAG_DRV_AXIS_Z]);
    
    sprintf(pszBuf, "%04x %04x %04x", _naFinalData[MCMAG_DRV_AXIS_X], _naFinalData[MCMAG_DRV_AXIS_Y], _naFinalData[MCMAG_DRV_AXIS_Z]);

    MSE_LOG(">>>>>> %d %d %d\n", _naFinalData[MCMAG_DRV_AXIS_X], _naFinalData[MCMAG_DRV_AXIS_Y], _naFinalData[MCMAG_DRV_AXIS_Z]);

    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** MCMAG_GetOpenStatus
 *****************************************/
static int    MCMAG_GetOpenStatus(void)
{
    wait_event_interruptible(wqMcmag_Open, (atomic_read(&s_tMcmag_FlagOpen) != 0));
    
    return (atomic_read(&s_tMcmag_FlagOpen));
}

/*****************************************
 *** MCMAG_InitChipset
 *****************************************/
static int    MCMAG_InitChipset(void)
{
    int    _nRetCode = MCMAG_DRV_RETCODE_OK;

    MSE_FUN();

    _nRetCode = MCMAG_ConfigRegControl3_EnableSoftReset();

    if (MCMAG_DRV_RETCODE_OK != _nRetCode)
    {
        MSE_ERR("ERR: fail to soft-reset!\n");

        return (_nRetCode);
    }

    msleep(50);

    _nRetCode = MCMAG_ConfigRegControl4_SetDynamicRange(E_MC41XX_DYNAMIC_RANGE_15bit);

    if (MCMAG_DRV_RETCODE_OK != _nRetCode)
        MSE_ERR("ERR: fail to set dynamic range!\n");

    return (_nRetCode);
}

/*****************************************
 *** MCMAG_ReadChipInfo
 *****************************************/
static int    MCMAG_ReadChipInfo(char *pszBuf, int nBufSize)
{
    u8    _bWhoIam = 0;

    if ((NULL == pszBuf) || (nBufSize <= 16))
        return (-EFAULT);

    MC41XX_MUTEX_LOCK();
    MCMAG_Start();
    mCube_SensorRead(MCMAG_REG_WHO_I_AM, sizeof(_bWhoIam), &_bWhoIam);
    MC41XX_MUTEX_UNLOCK();

    sprintf(pszBuf, "MC41XX - 0x%X", _bWhoIam);

    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** MCMAG_ReadPostureData
 *****************************************/
static int    MCMAG_ReadPostureData(char *pszBuf, int nBufSize)
{
    MSE_FUN();

    if ((NULL == pszBuf) || (nBufSize < 32))
        return (-EFAULT);
    
    read_lock(&s_tMcmag_MiddlewareData.tDataLock);
    
        sprintf(pszBuf                                      ,
                "%d %d %d %d"                               ,
                s_tMcmag_MiddlewareData.nOrientation_Heading,
                s_tMcmag_MiddlewareData.nOrientation_Pitch  ,
                s_tMcmag_MiddlewareData.nOrientation_Roll   ,
                s_tMcmag_MiddlewareData.nMag_Status          );
    
    read_unlock(&s_tMcmag_MiddlewareData.tDataLock);

    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** MCMAG_ReadCaliData
 *****************************************/
static int    MCMAG_ReadCaliData(char *pszBuf, int nBufSize)
{
    MSE_FUN();

    if ((NULL == pszBuf) || (nBufSize < 64))
        return (-EFAULT);
    
    read_lock(&s_tMcmag_MiddlewareData.tDataLock);

        sprintf(pszBuf                             ,
                "%d %d %d %d %d %d %d"             ,
                s_tMcmag_MiddlewareData.nMag_X     ,
                s_tMcmag_MiddlewareData.nMag_Y     ,
                s_tMcmag_MiddlewareData.nMag_Z     ,
                s_tMcmag_MiddlewareData.nAccel_X   ,
                s_tMcmag_MiddlewareData.nAccel_Y   , 
                s_tMcmag_MiddlewareData.nAccel_Z   ,
                s_tMcmag_MiddlewareData.nMag_Status );

    read_unlock(&s_tMcmag_MiddlewareData.tDataLock);

    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** MCMAG_ReadMiddleControl
 *****************************************/
static int    MCMAG_ReadMiddleControl(char *pszBuf, int nBufSize)
{
    if ((NULL == pszBuf) || (nBufSize < 64))
        return (-EFAULT);
    
    read_lock(&s_tMcmag_MiddlewareData.tControlLock);

        sprintf(pszBuf, "%d %d %d %d %d %d %d %d %d %d",
                s_tMcmag_MiddlewareData.naMidControlData[0], s_tMcmag_MiddlewareData.naMidControlData[1],
                s_tMcmag_MiddlewareData.naMidControlData[2], s_tMcmag_MiddlewareData.naMidControlData[3],
                s_tMcmag_MiddlewareData.naMidControlData[4], s_tMcmag_MiddlewareData.naMidControlData[5],
                s_tMcmag_MiddlewareData.naMidControlData[6], s_tMcmag_MiddlewareData.naMidControlData[7],
                s_tMcmag_MiddlewareData.naMidControlData[8], s_tMcmag_MiddlewareData.naMidControlData[9] );

    read_unlock(&s_tMcmag_MiddlewareData.tControlLock);

    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** show_daemon_name
 *****************************************/
static ssize_t    show_daemon_name(struct device_driver *ptDeviceDriver, char *pszBuf)
{
    return (sprintf(pszBuf, MCMAG_DRV_DAEMON_NAME));
}

/*****************************************
 *** show_chipinfo_value
 *****************************************/
static ssize_t    show_chipinfo_value(struct device_driver *ptDeviceDriver, char *pszBuf)
{
    char    _szChipInfoBuf[MCMAG_BUFFER_SIZE] = { 0 };
    
    MCMAG_ReadChipInfo(_szChipInfoBuf, MCMAG_BUFFER_SIZE);
    
    return (sprintf(pszBuf, "%s\n", _szChipInfoBuf));
}

/*****************************************
 *** show_sensordata_value
 *****************************************/
static ssize_t    show_sensordata_value(struct device_driver *ptDeviceDriver, char *pszBuf)
{
    char    _szSensorDataBuf[MCMAG_BUFFER_SIZE] = { 0 };

    memset(_szSensorDataBuf, 0, MCMAG_BUFFER_SIZE);
    MCMAG_ReadMagSensorData(_szSensorDataBuf, MCMAG_BUFFER_SIZE);

    return (sprintf(pszBuf, "%s\n", _szSensorDataBuf));
}

/*****************************************
 *** show_posturedata_value
 *****************************************/
static ssize_t    show_posturedata_value(struct device_driver *ptDeviceDriver, char *pszBuf)
{
    char    _szPostureDataBuf[MCMAG_BUFFER_SIZE] = { 0 };

    memset(_szPostureDataBuf, 0, MCMAG_BUFFER_SIZE);
    MCMAG_ReadPostureData(_szPostureDataBuf, MCMAG_BUFFER_SIZE);

    return (sprintf(pszBuf, "%s\n", _szPostureDataBuf));
}

/*****************************************
 *** show_calidata_value
 *****************************************/
static ssize_t    show_calidata_value(struct device_driver *ptDeviceDriver, char *pszBuf)
{
    char    _szCaliDataBuf[MCMAG_BUFFER_SIZE] = { 0 };

    memset(_szCaliDataBuf, 0, MCMAG_BUFFER_SIZE);
    MCMAG_ReadCaliData(_szCaliDataBuf, MCMAG_BUFFER_SIZE);

    return (sprintf(pszBuf, "%s\n", _szCaliDataBuf));
}

/*****************************************
 *** show_midcontrol_value
 *****************************************/
static ssize_t    show_midcontrol_value(struct device_driver *ptDeviceDriver, char *pszBuf)
{
    char    _szControlDataBuf[MCMAG_BUFFER_SIZE] = { 0 };

    memset(_szControlDataBuf, 0, MCMAG_BUFFER_SIZE);

    MCMAG_ReadMiddleControl(_szControlDataBuf, MCMAG_BUFFER_SIZE);

    return (sprintf(pszBuf, "%s\n", _szControlDataBuf));
}

/*****************************************
 *** store_midcontrol_value
 *****************************************/
static ssize_t    store_midcontrol_value(struct device_driver *ptDeviceDriver, const char *pszBuf, size_t tCount)
{
    int    _naNewControlData[10] = { 0 };

    if (10 == sscanf(pszBuf,
                     "%d %d %d %d %d %d %d %d %d %d",
                     &_naNewControlData[0], &_naNewControlData[1],
                     &_naNewControlData[2], &_naNewControlData[3],
                     &_naNewControlData[4], &_naNewControlData[5],
                     &_naNewControlData[6], &_naNewControlData[7],
                     &_naNewControlData[8], &_naNewControlData[9] ))
    {
        write_lock(&s_tMcmag_MiddlewareData.tControlLock);

            memcpy(s_tMcmag_MiddlewareData.naMidControlData, _naNewControlData, (sizeof(int) * 10));
    
        write_unlock(&s_tMcmag_MiddlewareData.tControlLock);
    }
    else
    {
        MSE_ERR("ERR: invalid format of arguments\n");

        return (-EINVAL);
    }

    return ((sizeof(int) * 10));
}

/*****************************************
 *** show_middebug_value
 *****************************************/
static ssize_t    show_middebug_value(struct device_driver *ptDeviceDriver, char *pszBuf)
{
    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** store_middebug_value
 *****************************************/
static ssize_t    store_middebug_value(struct device_driver *ptDeviceDriver, const char *pszBuf, size_t tCount)
{   
    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** show_mode_value
 *****************************************/
static ssize_t    show_mode_value(struct device_driver *ptDeviceDriver, char *pszBuf)
{
    int    _nMode = 0;
    
    return (sprintf(pszBuf, "%d\n", _nMode));
}

/*****************************************
 *** store_mode_value
 *****************************************/
static ssize_t    store_mode_value(struct device_driver *ptDeviceDriver, const char *pszBuf, size_t tCount)
{
    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** show_layout_value
 *****************************************/
static ssize_t    show_layout_value(struct device_driver *ptDeviceDriver, char *pszBuf)
{
    struct i2c_client        *_ptClient    = s_ptMcmag_i2c_client;  
    struct _mcmag_i2c_data   *_pt_i2c_data = i2c_get_clientdata(_ptClient);

    return (sprintf(pszBuf                                                                            ,
                    "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n"                                  ,
                    _pt_i2c_data->ptHW->direction, atomic_read(&_pt_i2c_data->tLayout)                ,
                    _pt_i2c_data->tCvt.sign[0], _pt_i2c_data->tCvt.sign[1], _pt_i2c_data->tCvt.sign[2],
                    _pt_i2c_data->tCvt.map[0] , _pt_i2c_data->tCvt.map[1] , _pt_i2c_data->tCvt.map[2]  ));
}

/*****************************************
 *** store_layout_value
 *****************************************/
static ssize_t    store_layout_value(struct device_driver *ptDeviceDriver, const char *pszBuf, size_t tCount)
{
    int    _nLayout;

    struct _mcmag_i2c_data   *_pt_i2c_data = i2c_get_clientdata(s_ptMcmag_i2c_client);

    if (1 != sscanf(pszBuf, "%d", &_nLayout))
        return (-EINVAL);

    atomic_set(&_pt_i2c_data->tLayout, _nLayout);
    
    if (!hwmsen_get_convert(_nLayout, &_pt_i2c_data->tCvt))
        MSE_ERR("HWMSEN_GET_CONVERT function error!\n");
    else if (!hwmsen_get_convert(_pt_i2c_data->ptHW->direction, &_pt_i2c_data->tCvt))
        MSE_ERR("invalid tLayout: %d, restore to %d\n", _nLayout, _pt_i2c_data->ptHW->direction);
    else
    {
        MSE_ERR("invalid tLayout: (%d, %d)\n", _nLayout, _pt_i2c_data->ptHW->direction);

        hwmsen_get_convert(0, &_pt_i2c_data->tCvt);
    }
    
    return (tCount);
}

/*****************************************
 *** show_status_value
 *****************************************/
static ssize_t    show_status_value(struct device_driver *ptDeviceDriver, char *pszBuf)
{
    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** show_trace_value
 *****************************************/
static ssize_t    show_trace_value(struct device_driver *ptDeviceDriver, char *pszBuf)
{
    struct _mcmag_i2c_data   *_pt_i2c_data = i2c_get_clientdata(s_ptMcmag_i2c_client);
    
    if (NULL == _pt_i2c_data)
        return (-EFAULT);
    
    return (snprintf(pszBuf, PAGE_SIZE, "0x%04X\n", atomic_read(&_pt_i2c_data->tTrace)));
}

/*****************************************
 *** store_trace_value
 *****************************************/
static ssize_t    store_trace_value(struct device_driver *ptDeviceDriver, const char *pszBuf, size_t tCount)
{
    int    _nTrace;

    struct _mcmag_i2c_data *_pt_i2c_data = i2c_get_clientdata(s_ptMcmag_i2c_client);

    if (NULL == _pt_i2c_data)
        return (-EFAULT);
    
    if (1 != sscanf(pszBuf, "0x%x", &_nTrace))
        return (-EINVAL);

    atomic_set(&_pt_i2c_data->tTrace, _nTrace);
    
    return (tCount);
}

/*****************************************
 *** show_self_test
 *****************************************/
static ssize_t    show_self_test(struct device_driver *ptDeviceDriver, char *pszBuf)
{
    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** store_shipment_test
 *****************************************/
static ssize_t    store_shipment_test(struct device_driver * ptDeviceDriver, const char * pszBuf, size_t tCount)
{
    // add code, if this is supported in future

    return (tCount);
}

/*****************************************
 *** show_shipment_test
 *****************************************/
static ssize_t    show_shipment_test(struct device_driver *ptDeviceDriver, char *pszBuf)
{
    return (sprintf(pszBuf, "y\n"));
}

/*****************************************
 *** DRIVER ATTRIBUTE LIST TABLE
 *****************************************/
static DRIVER_ATTR(daemon      , S_IRUGO          , show_daemon_name      , NULL                   );
static DRIVER_ATTR(chipinfo    , S_IRUGO          , show_chipinfo_value   , NULL                   );
static DRIVER_ATTR(sensordata  , S_IRUGO          , show_sensordata_value , NULL                   );
static DRIVER_ATTR(posturedata , S_IRUGO          , show_posturedata_value, NULL                   );
static DRIVER_ATTR(calidata    , S_IRUGO          , show_calidata_value   , NULL                   );
static DRIVER_ATTR(midcontrol  , S_IRUGO | S_IWUSR, show_midcontrol_value , store_midcontrol_value );
static DRIVER_ATTR(middebug    , S_IRUGO | S_IWUSR, show_middebug_value   , store_middebug_value   );
static DRIVER_ATTR(mode        , S_IRUGO | S_IWUSR, show_mode_value       , store_mode_value       );
static DRIVER_ATTR(layout      , S_IRUGO | S_IWUSR, show_layout_value     , store_layout_value     );
static DRIVER_ATTR(status      , S_IRUGO          , show_status_value     , NULL                   );
static DRIVER_ATTR(trace       , S_IRUGO | S_IWUSR, show_trace_value      , store_trace_value      );
static DRIVER_ATTR(selftest    , S_IRUGO          , show_self_test        , NULL                   );
static DRIVER_ATTR(shipmenttest, S_IRUGO | S_IWUSR, show_shipment_test    , store_shipment_test    );

static struct driver_attribute   *ptMcmagDriverAttributeList[] = {   &driver_attr_daemon      ,
                                                                     &driver_attr_chipinfo    ,
                                                                     &driver_attr_sensordata  ,
                                                                     &driver_attr_posturedata ,
                                                                     &driver_attr_calidata    ,
                                                                     &driver_attr_midcontrol  ,
                                                                     &driver_attr_middebug    ,
                                                                     &driver_attr_mode        ,
                                                                     &driver_attr_layout      ,
                                                                     &driver_attr_status      ,
                                                                     &driver_attr_trace       ,
                                                                     &driver_attr_selftest    ,
                                                                     &driver_attr_shipmenttest
                                                                 };

/*****************************************
 *** mcmag_create_attr
 *****************************************/
static int    mcmag_create_attr(struct device_driver *ptDeviceDriver) 
{
    int    _nIdex;
    int    _nNumOfAttributes = (int)(sizeof(ptMcmagDriverAttributeList) / sizeof(ptMcmagDriverAttributeList[0]));

    MSE_FUN();

    if (NULL == ptDeviceDriver)
        return (-EINVAL);
    
    for (_nIdex = 0; _nIdex < _nNumOfAttributes; _nIdex++)
    {
        if(driver_create_file(ptDeviceDriver, ptMcmagDriverAttributeList[_nIdex]))
            break;
    }    
    
    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** mcmag_delete_attr
 *****************************************/
static int    mcmag_delete_attr(struct device_driver *ptDeviceDriver)
{
    int    _nIdex;
    int    _nNumOfAttributes = (int)(sizeof(ptMcmagDriverAttributeList) / sizeof(ptMcmagDriverAttributeList[0]));

    MSE_FUN();

    if (NULL == ptDeviceDriver)
        return (-EINVAL);

    for (_nIdex = 0; _nIdex < _nNumOfAttributes; _nIdex++)
        driver_remove_file(ptDeviceDriver, ptMcmagDriverAttributeList[_nIdex]);
    
    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** mcmag_open
 *****************************************/
static int    mcmag_open(struct inode *ptINode, struct file *ptFile)
{
    MSE_FUN();

    atomic_inc(&s_tMcmag_DeviceOpenCount);
    
    return (nonseekable_open(ptINode, ptFile));
}

/*****************************************
 *** mcmag_release
 *****************************************/
static int    mcmag_release(struct inode *ptINode, struct file *ptFile)
{
    MSE_FUN();

    atomic_dec(&s_tMcmag_DeviceOpenCount);

    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** _operate_sensor_enable
 *****************************************/
int    _operate_sensor_enable(u32 dwType, int nSwitch)
{
    MSE_FUN();

    read_lock(&s_tMcmag_MiddlewareData.tControlLock);
    
        if (MCMAG_DRV_ENABLE == nSwitch)
        {
            if (SENSOR_ORIENTATION == dwType)
                atomic_set(&s_tMcmag_FlagOrientationSensor, 1);
            else if (SENSOR_MAGNETIC == dwType)
                atomic_set(&s_tMcmag_FlagMagSensor, 1);

            #ifdef MCMAG_DRV_SUPPORT_VGYRO
                else if (SENSOR_GYROSCOPE == dwType)
                    atomic_set(&s_tMcmag_FlagGyroSensor, 1);
            #endif

            atomic_set(&s_tMcmag_FlagOpen, 1);
        }
        else
        {
            if (SENSOR_ORIENTATION == dwType)
                atomic_set(&s_tMcmag_FlagOrientationSensor, 0);
            else if (SENSOR_MAGNETIC == dwType)
                atomic_set(&s_tMcmag_FlagMagSensor, 0);

            #ifdef MCMAG_DRV_SUPPORT_VGYRO
                else if (SENSOR_GYROSCOPE == dwType)
                    atomic_set(&s_tMcmag_FlagGyroSensor, 0);
            #endif

            if (   (0 == atomic_read(&s_tMcmag_FlagMagSensor))

                #ifdef MCMAG_DRV_SUPPORT_VGYRO
                    && (0 == atomic_read(&s_tMcmag_FlagGyroSensor))
                #endif

                && (0 == atomic_read(&s_tMcmag_FlagOrientationSensor)))
            {
                atomic_set(&s_tMcmag_FlagOpen, 0);
            }
        }
        
        wake_up(&wqMcmag_Open);

    read_unlock(&s_tMcmag_MiddlewareData.tControlLock);

    MC41XX_MUTEX_LOCK();
        if (MCMAG_DRV_ENABLE == nSwitch)
            MCMAG_Start();
        else
            MCMAG_Stop();
    MC41XX_MUTEX_UNLOCK();

    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** MACRO :: _IOCTL_CHK_ARG
 *****************************************/
#define _IOCTL_CHK_ARG()                            \
            _pUserData = ((void __user *) lArg);    \
                                                    \
            if (NULL == _pUserData)                 \
                 return (-EFAULT);

/*****************************************
 *** MACRO :: _IOCTL_COPY_FROM_USER
 *****************************************/
#define _IOCTL_COPY_FROM_USER(buff)                                  \
             if (copy_from_user(&buff, _pUserData, sizeof(buff)))    \
                 return (-EFAULT);

/*****************************************
 *** mcmag_ioctl
 *****************************************/
static long   mcmag_ioctl(struct file *ptFile, unsigned int nCmd, unsigned long lArg)
{
    void __user   *_pUserData = NULL;

    switch (nCmd)
    {
        //================================================================    
        case MSENSOR_IOCTL_INIT:

             MSE_LOG("[%s] MSENSOR_IOCTL_INIT\n", __func__);
        
             MC41XX_MUTEX_LOCK();
             MCMAG_InitChipset();         
             MC41XX_MUTEX_UNLOCK();

             break;

        //================================================================    
        case ECOMPASS_IOC_GET_OFLAG:
        case ECOMPASS_IOC_GET_MFLAG:
             {
                 short          _wSensorStatus;
                 void __user   *_pArg = ((void __user *) lArg);

                 MSE_LOG("[%s] ECOMPASS_IOC_GET_FLAGs...\n", __func__);

                 if (ECOMPASS_IOC_GET_OFLAG == nCmd)
                     _wSensorStatus = atomic_read(&s_tMcmag_FlagOrientationSensor);
                 else //if (ECOMPASS_IOC_GET_MFLAG == nCmd)
                     _wSensorStatus = atomic_read(&s_tMcmag_FlagMagSensor);
            
                 if(copy_to_user(_pArg, &_wSensorStatus, sizeof(_wSensorStatus)))
                     return (-EFAULT);
             }    
             break;

        //================================================================    
        case ECOMPASS_IOC_GET_OPEN_STATUS:
             {
                 int            _nSensorsOpenStatus;
                 void __user   *_pArg = ((void __user *) lArg);

                 MSE_LOG("[%s] ECOMPASS_IOC_GET_OPEN_STATUS\n", __func__);

                 _nSensorsOpenStatus = MCMAG_GetOpenStatus();			
        
                 if(copy_to_user(_pArg, &_nSensorsOpenStatus, sizeof(_nSensorsOpenStatus)))
                     return (-EFAULT);
             }
             break;        

        //================================================================    
        case MSENSOR_IOCTL_SET_POSTURE:
             {
                 int    _naPostureData[4] = { 0 };

                 MSE_LOG("[%s] MSENSOR_IOCTL_SET_POSTURE\n", __func__);

                 _IOCTL_CHK_ARG();
                 _IOCTL_COPY_FROM_USER(_naPostureData);
                 
                 write_lock(&s_tMcmag_MiddlewareData.tDataLock);
                     s_tMcmag_MiddlewareData.nOrientation_Heading = _naPostureData[0];
                     s_tMcmag_MiddlewareData.nOrientation_Pitch   = _naPostureData[1];
                     s_tMcmag_MiddlewareData.nOrientation_Roll    = _naPostureData[2];
                     s_tMcmag_MiddlewareData.nMag_Status          = _naPostureData[3];

                     if (SENSOR_STATUS_ACCURACY_MEDIUM == s_tMcmag_MiddlewareData.nMag_Status)
                         s_tMcmag_MiddlewareData.nMag_Status = SENSOR_STATUS_ACCURACY_HIGH;
                 write_unlock(&s_tMcmag_MiddlewareData.tDataLock);    
             }
             break;

        //================================================================    
        case MSENSOR_IOCTL_SET_CALIDATA:
             {
                 int    _naCaliData[7] = { 0 };

                 MSE_LOG("[%s] MSENSOR_IOCTL_SET_CALIDATA\n", __func__);

                 _IOCTL_CHK_ARG();
                 _IOCTL_COPY_FROM_USER(_naCaliData);
                 
                 write_lock(&s_tMcmag_MiddlewareData.tDataLock);            
                     s_tMcmag_MiddlewareData.nMag_X      = _naCaliData[0];
                     s_tMcmag_MiddlewareData.nMag_Y      = _naCaliData[1];
                     s_tMcmag_MiddlewareData.nMag_Z      = _naCaliData[2];
                     s_tMcmag_MiddlewareData.nAccel_X    = _naCaliData[3];
                     s_tMcmag_MiddlewareData.nAccel_Y    = _naCaliData[4];
                     s_tMcmag_MiddlewareData.nAccel_Z    = _naCaliData[5];
                     s_tMcmag_MiddlewareData.nMag_Status = _naCaliData[6];

                     if (SENSOR_STATUS_ACCURACY_MEDIUM == s_tMcmag_MiddlewareData.nMag_Status)
                         s_tMcmag_MiddlewareData.nMag_Status = SENSOR_STATUS_ACCURACY_HIGH;
                 write_unlock(&s_tMcmag_MiddlewareData.tDataLock);    
             }
             break;                                

        //================================================================    
        case MSENSOR_IOCTL_SENSOR_ENABLE:
             {
                 int    _nEnable;

                 MSE_LOG("[%s] MSENSOR_IOCTL_SENSOR_ENABLE\n", __func__);

                 _IOCTL_CHK_ARG();
                 _IOCTL_COPY_FROM_USER(_nEnable)
                 _operate_sensor_enable(SENSOR_ORIENTATION, _nEnable);
             }
             break;

        //================================================================    
        case MSENSOR_IOCTL_READ_SENSORDATA:
        case MSENSOR_IOCTL_READ_POSTUREDATA:
        case MSENSOR_IOCTL_READ_CALIDATA:
        case MSENSOR_IOCTL_READ_CHIPINFO:
        case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
             {
                 char    _szStrBuf[MCMAG_BUFFER_SIZE] = { 0 };

                 MSE_LOG("[%s] MSENSOR_IOCTL_READ_SENSORDATAs...\n", __func__);

                 _IOCTL_CHK_ARG();

                 if (MSENSOR_IOCTL_READ_SENSORDATA == nCmd)
                     MCMAG_ReadMagSensorData(_szStrBuf, MCMAG_BUFFER_SIZE);
                 else if ((MSENSOR_IOCTL_READ_POSTUREDATA == nCmd) || (MSENSOR_IOCTL_READ_FACTORY_SENSORDATA == nCmd))
                     MCMAG_ReadPostureData(_szStrBuf, MCMAG_BUFFER_SIZE);
                 else if (MSENSOR_IOCTL_READ_CALIDATA == nCmd)
                     MCMAG_ReadCaliData(_szStrBuf, MCMAG_BUFFER_SIZE);
                 else //if (MSENSOR_IOCTL_READ_CHIPINFO == nCmd)
                     MCMAG_ReadChipInfo(_szStrBuf, MCMAG_BUFFER_SIZE);

                 if (copy_to_user(_pUserData, _szStrBuf, (strlen(_szStrBuf) + 1)))
                     return (-EFAULT);
             }
             break;

        //================================================================    
        case MSENSOR_IOCTL_READ_CONTROL:
             {
                 int    _naControlData[10] = { 0 };

                 MSE_LOG("[%s] MSENSOR_IOCTL_READ_CONTROL\n", __func__);

                 _IOCTL_CHK_ARG();

                 read_lock(&s_tMcmag_MiddlewareData.tControlLock);

                     memcpy(_naControlData, &s_tMcmag_MiddlewareData.naMidControlData[0], sizeof(_naControlData));

                 read_unlock(&s_tMcmag_MiddlewareData.tControlLock);            
    
                 if (copy_to_user(_pUserData, _naControlData, sizeof(_naControlData)))
                     return (-EFAULT);
             }
             break;
        
        //================================================================    
        case MSENSOR_IOCTL_SET_CONTROL:
    
             break;
        
        //================================================================    
        case MSENSOR_IOCTL_SET_MODE:
             #if 0
             {
                 int    _nMode;

                 _IOCTL_CHK_ARG();
                 _IOCTL_COPY_FROM_USER(_nMode)
                 
                 MCMAG_SetMode(_nMode);                
             }
             #endif
             break;

        //================================================================    
        default:

             MSE_ERR("ERR: 0x%4x CMD not supported!", nCmd);
             return (-ENOIOCTLCMD);

             break;
    }

    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** STATIC STRUCTURE:: fops
 *****************************************/
static struct file_operations    mcmag_fops = {
                                                  .owner          = THIS_MODULE,
                                                  .open           = mcmag_open,
                                                  .release        = mcmag_release,
                                                  .unlocked_ioctl = mcmag_ioctl,
                                              };

/*****************************************
 *** STATIC STRUCTURE:: misc-device
 *****************************************/
static struct miscdevice    mcmag_device = {
                                               .minor = MISC_DYNAMIC_MINOR,
                                               .name  = "msensor",
                                               .fops  = &mcmag_fops,
                                           };

/*****************************************
 *** MACRO :: _OPERATE_CHK_BUFF_AND_SIZE
 *****************************************/
#define _OPERATE_CHK_BUFF_AND_SIZE(buff, buffsize, TypeSize)           \
             if ((NULL == buff) || (buffsize < sizeof(TypeSize)))      \
                 return (-EINVAL);                                     \

/*****************************************
 *** mcmag_operate
 *****************************************/
int    mcmag_operate(void       *pSelf      ,
                     uint32_t    dwCommand  ,
                     void       *pInBuf     ,
                     int         nInBufSize ,
                     void       *pOutBuf    ,
                     int         nOutBufSize,
                     int        *pnActualOut )
{
    MSE_LOG("[%s] cmd: 0x%X\n", __func__, dwCommand);

    switch (dwCommand)
    {
        //================================================================
        case SENSOR_DELAY:
             {
                 int    _nSampleDelay = 0;

                 _OPERATE_CHK_BUFF_AND_SIZE(pInBuf, nInBufSize, int);

                 _nSampleDelay = *((int *) pInBuf);

                 if (_nSampleDelay < MCMAG_DRV_DEFAULT_SAMPLE_DELAY)
                     _nSampleDelay = MCMAG_DRV_DEFAULT_SAMPLE_DELAY;

                 s_tMcmag_MiddlewareData.naMidControlData[0] = _nSampleDelay;

                 MSE_LOG("_nSampleDelay: %d", _nSampleDelay);
             }
             break;

        //================================================================    
        case SENSOR_ENABLE:
    
             _OPERATE_CHK_BUFF_AND_SIZE(pInBuf, nInBufSize, int);
             _operate_sensor_enable(SENSOR_MAGNETIC, *(int *)pInBuf);

             break;

        //================================================================    
        case SENSOR_GET_DATA:
             {
                 hwm_sensor_data   *_ptMagSensorData = NULL;

                 _OPERATE_CHK_BUFF_AND_SIZE(pOutBuf, nOutBufSize, hwm_sensor_data);

                 _ptMagSensorData = ((hwm_sensor_data *) pOutBuf);
    
                 read_lock(&s_tMcmag_MiddlewareData.tDataLock);
                     _ptMagSensorData->values[0] = s_tMcmag_MiddlewareData.nMag_X;
                     _ptMagSensorData->values[1] = s_tMcmag_MiddlewareData.nMag_Y;
                     _ptMagSensorData->values[2] = s_tMcmag_MiddlewareData.nMag_Z;
                     _ptMagSensorData->status    = s_tMcmag_MiddlewareData.nMag_Status;
                 read_unlock(&s_tMcmag_MiddlewareData.tDataLock); 
    
                 _ptMagSensorData->value_divide = 1;
             }

             break;    // END of SENSOR_GET_DATA

        //================================================================    
         default:

             MSE_ERR("ERR: 0x%4x CMD not supported!", dwCommand);
             return (-ENOIOCTLCMD);

             break;
    }

    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** mcmag_orientation_operate
 *****************************************/
int mcmag_orientation_operate(void       *pSelf      , 
                              uint32_t    dwCommand  ,
                              void       *pInBuf     ,
                              int         nInBufSize ,
                              void       *pOutBuf    ,
                              int         nOutBufSize,
                              int        *pnActualOut )
{
    MSE_LOG("[%s] cmd: 0x%X\n", __func__, dwCommand);

    switch (dwCommand)
    {
        //================================================================
        case SENSOR_DELAY:
             {
                 int    _nSampleDelay = 0;

                 _OPERATE_CHK_BUFF_AND_SIZE(pInBuf, nInBufSize, int);

                 _nSampleDelay = *((int *) pInBuf);

                 if (_nSampleDelay < MCMAG_DRV_DEFAULT_SAMPLE_DELAY)
                     _nSampleDelay = MCMAG_DRV_DEFAULT_SAMPLE_DELAY;

                 s_tMcmag_MiddlewareData.naMidControlData[0] = _nSampleDelay;

                 MSE_LOG("_nSampleDelay: %d", _nSampleDelay);
             }
             break;

        //================================================================    
        case SENSOR_ENABLE:
    
             _OPERATE_CHK_BUFF_AND_SIZE(pInBuf, nInBufSize, int);
             _operate_sensor_enable(SENSOR_ORIENTATION, *(int *)pInBuf);

             break;

        //================================================================    
        case SENSOR_GET_DATA:
             {
                 hwm_sensor_data   *_ptOrientationSensorData = NULL;

                 _OPERATE_CHK_BUFF_AND_SIZE(pOutBuf, nOutBufSize, hwm_sensor_data);

                 _ptOrientationSensorData = ((hwm_sensor_data *) pOutBuf);
    
                 read_lock(&s_tMcmag_MiddlewareData.tDataLock);
                     _ptOrientationSensorData->values[0] = s_tMcmag_MiddlewareData.nOrientation_Heading;
                     _ptOrientationSensorData->values[1] = s_tMcmag_MiddlewareData.nOrientation_Pitch;
                     _ptOrientationSensorData->values[2] = s_tMcmag_MiddlewareData.nOrientation_Roll;
                     _ptOrientationSensorData->status    = s_tMcmag_MiddlewareData.nMag_Status;
                 read_unlock(&s_tMcmag_MiddlewareData.tDataLock);

                 _ptOrientationSensorData->value_divide = 1;
             }
    
             break;    // END of SENSOR_GET_DATA

        //================================================================    
         default:

             MSE_ERR("ERR: 0x%4x CMD not supported!", dwCommand);
             return (-ENOIOCTLCMD);

             break;
    }

    return (MCMAG_DRV_RETCODE_OK);
}

#ifdef MCMAG_DRV_SUPPORT_VGYRO
/*****************************************
 *** mcmag_vgyro_ioctl
 *****************************************/
static long    mcmag_vgyro_ioctl(struct file *pFile, unsigned int nCmd, unsigned long lArg)
{ 
    int            _nErrCode        = MCMAG_DRV_RETCODE_OK;
    int            _naSensorData[3] = { 0 };
    void __user   *_pUserData       = NULL;
    
    if (_IOC_DIR(nCmd) & _IOC_READ)
    {
        _nErrCode = !access_ok(VERIFY_WRITE, (void __user *)lArg, _IOC_SIZE(nCmd));
    }
    else if (_IOC_DIR(nCmd) & _IOC_WRITE)
    {
        _nErrCode = !access_ok(VERIFY_READ, (void __user *)lArg, _IOC_SIZE(nCmd));
    }
    
    if (_nErrCode)
    {
         MSE_LOG("[%s] ERR: fail to access\n", __func__);

        return (-EFAULT);
    }

	switch (nCmd)
	{
        //================================================================    
        case GYROSCOPE_MCUBE_IOCTL_GET_GFLAG:
             {
                 short          _wSensorStatus;
                 void __user   *_pArg = ((void __user *) lArg);

                 MSE_LOG("[%s] GYROSCOPE_MCUBE_IOCTL_GET_GFLAG...\n", __func__);

                 _wSensorStatus = atomic_read(&s_tMcmag_FlagGyroSensor);
            
                 if(copy_to_user(_pArg, &_wSensorStatus, sizeof(_wSensorStatus)))
                     return (-EFAULT);
             }    
             break;

        //================================================================    
        case GYROSCOPE_MCUBE_IOCTL_SET_SENSORDATA:

             MSE_LOG("[%s] GYROSCOPE_MCUBE_IOCTL_SET_SENSORDATA\n", __func__);
             
             _pUserData = ((void __user*) lArg);
             
             if (NULL == _pUserData)
             {
                 _nErrCode = (-EINVAL);
             
                 break;	  
             }
             
             if (copy_from_user(_naSensorData, _pUserData, sizeof(_naSensorData)))
             {
                 _nErrCode = (-EFAULT);
                 
                 break;	  
             }
             
             write_lock(&s_tMcmag_MiddlewareData.tDataLock);
                 s_tMcmag_MiddlewareData.nGyro_X = _naSensorData[0];
                 s_tMcmag_MiddlewareData.nGyro_Y = _naSensorData[1];
                 s_tMcmag_MiddlewareData.nGyro_Z = _naSensorData[2];
             write_unlock(&s_tMcmag_MiddlewareData.tDataLock);    

             break;

        //================================================================    
        default:

             MSE_LOG("[%s] ERR: unknown IOCMD: 0x%X\n", __func__, nCmd);

             _nErrCode = (-ENOIOCTLCMD);

             break;	
	}

    return (_nErrCode);
}

/*****************************************
 *** STATIC STRUCTURE:: fops
 *****************************************/
static struct file_operations    mcmag_vgyro_fops = {
                                                        .owner          = THIS_MODULE,
                                                        .unlocked_ioctl = mcmag_vgyro_ioctl,
                                                    };

/*****************************************
 *** STATIC STRUCTURE:: misc-device
 *****************************************/
static struct miscdevice    mcmag_vgyro_device = {
                                                      .minor = MISC_DYNAMIC_MINOR,
                                                      .name  = "gyroscope",
                                                      .fops  = &mcmag_vgyro_fops,
                                                  };

/*****************************************
 *** mcmag_vgyro_operate
 *****************************************/
int mcmag_vgyro_operate(void       *pSelf      , 
                        uint32_t    dwCommand  ,
                        void       *pInBuf     ,
                        int         nInBufSize ,
                        void       *pOutBuf    ,
                        int         nOutBufSize,
                        int        *pnActualOut )
{
    int    _nErrCode = MCMAG_DRV_RETCODE_OK;

    switch (dwCommand)
    {
        //================================================================
        case SENSOR_DELAY:
             MSE_LOG("[%s] SENSOR_DELAY\n", __func__);
             break;

        //================================================================    
        case SENSOR_ENABLE:
             MSE_LOG("[%s] SENSOR_ENABLE\n", __func__);

             _OPERATE_CHK_BUFF_AND_SIZE(pInBuf, nInBufSize, int);
             _operate_sensor_enable(SENSOR_GYROSCOPE, *(int *)pInBuf);

             break;

        //================================================================    
        case SENSOR_GET_DATA:
             {
                 hwm_sensor_data   *_ptGyroSensorData = NULL;

                 MSE_LOG("[%s] SENSOR_GET_DATA\n", __func__);

                 _OPERATE_CHK_BUFF_AND_SIZE(pOutBuf, nOutBufSize, hwm_sensor_data);

                 _ptGyroSensorData = ((hwm_sensor_data *) pOutBuf);
    
                 read_lock(&s_tMcmag_MiddlewareData.tDataLock);
                     _ptGyroSensorData->values[0] = s_tMcmag_MiddlewareData.nGyro_X;
                     _ptGyroSensorData->values[1] = s_tMcmag_MiddlewareData.nGyro_Y;
                     _ptGyroSensorData->values[2] = s_tMcmag_MiddlewareData.nGyro_Z;
                 read_unlock(&s_tMcmag_MiddlewareData.tDataLock);

                 _ptGyroSensorData->value_divide = 7506;	//DEGREE_TO_RAD;
                 _ptGyroSensorData->status       = SENSOR_STATUS_ACCURACY_MEDIUM;
             }

             break;    // END of SENSOR_GET_DATA

        //================================================================    
        default:

             MSE_LOG("[%s] ERR: unknown CMD: 0x%X\n", __func__, dwCommand);

             _nErrCode = (-ENOIOCTLCMD);

             break;	
	}
	
	return (_nErrCode);
}
#endif  // end of MCMAG_DRV_SUPPORT_VGYRO


/*****************************************
 *** POWER SAVING (MTK Reference Code)
 *****************************************/
/*----------------------------------------------------------------------------*/
#ifndef	CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int mcmag_suspend(struct i2c_client *ptClient, pm_message_t msg) 
{
    struct _mcmag_i2c_data   *_pt_i2c_data = i2c_get_clientdata(ptClient)
    
    MSE_FUN();    
    
    if (PM_EVENT_SUSPEND == msg.event)
        MCMAG_Stop();

    return (MCMAG_DRV_RETCODE_OK);
}
/*----------------------------------------------------------------------------*/
static int mcmag_resume(struct i2c_client *ptClient)
{
    struct _mcmag_i2c_data *_pt_i2c_data = i2c_get_clientdata(ptClient)
    
    MSE_FUN();

	return (MCMAG_Start());
}
/*----------------------------------------------------------------------------*/
#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void mcmag_early_suspend(struct early_suspend *h) 
{
    struct _mcmag_i2c_data *_pt_i2c_data = container_of(h, struct _mcmag_i2c_data, tDrv_EarlySuspend);   

    MSE_FUN();    

    if (_pt_i2c_data == NULL)
    {
        MSE_ERR("null pointer!!\n");
		    return;
    }

    MCMAG_Stop();
}
/*----------------------------------------------------------------------------*/
static void mcmag_late_resume(struct early_suspend *h)
{
    struct _mcmag_i2c_data *_pt_i2c_data = container_of(h, struct _mcmag_i2c_data, tDrv_EarlySuspend);         

    MSE_FUN();

    if (_pt_i2c_data == NULL)
    {
		    MSE_ERR("null pointer!!\n");
		    return;
    }

    MCMAG_Start();
}
/*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/
/*----------------------------------------------------------------------------*/

/*****************************************
 *** mcmag_i2c_detect
 *****************************************/
#if (MCMAG_DRV_SYSCFG_KERNEL_VERSION == MCMAG_DRV_SYSCFG_LINUX_KERNEL_2_6_X)
    static int    mcmag_i2c_detect(struct i2c_client *ptClient, int kind, struct i2c_board_info *info) 
    {    
        strcpy(info->type, MCMAG_DRV_DEVICE_NAME);
    
        return (MCMAG_DRV_RETCODE_OK);
    }
#endif

/*****************************************
 *** mcmag_i2c_probe
 *****************************************/
static int    mcmag_i2c_probe(struct i2c_client *ptClient, const struct i2c_device_id *id)
{
    int err = 0;

    struct i2c_client      *new_client;
    struct _mcmag_i2c_data *data;
    struct hwmsen_object    sobj_m, sobj_o;

    MSE_FUN();    

    if (!(data = kmalloc(sizeof(struct _mcmag_i2c_data), GFP_KERNEL)))
    {
        err = -ENOMEM;
        goto exit;
    }

    memset(data, 0, sizeof(struct _mcmag_i2c_data));
    
    data->ptHW = get_cust_mag_hw();

    if ((err = hwmsen_get_convert(data->ptHW->direction, &data->tCvt)))
    {
        MSE_ERR("invalid direction: %d\n", data->ptHW->direction);
        goto exit;
    }

    atomic_set(&data->tLayout, data->ptHW->direction);
    atomic_set(&data->tTrace, 0);
    init_waitqueue_head(&wqMcmag_DataReady);
    init_waitqueue_head(&wqMcmag_Open);

    data->ptClient = ptClient;
    new_client     = data->ptClient;
    i2c_set_clientdata(new_client, data);
    
    s_ptMcmag_i2c_client = new_client;	
    
    if ((err = MCMAG_InitChipset()))
        goto exit_init_failed;

    MC41XX_MUTEX_INIT();

    if ((err = mcmag_create_attr(&s_tMcmag_SensorDriver.driver)))
    {
        MSE_ERR("create attribute err = %d\n", err);
        goto exit_sysfs_create_group_failed;
    }
    
    if ((err = misc_register(&mcmag_device)))
    {
        MSE_ERR("mcmag_device register failed\n");
        goto exit_misc_device_register_failed;
    }    

    sobj_m.self           = data;
    sobj_m.polling        = 1;
    sobj_m.sensor_operate = mcmag_operate;

    if ((err = hwmsen_attach(ID_MAGNETIC, &sobj_m)))
    {
        MSE_ERR("attach fail = %d\n", err);
        goto exit_kfree;
    }
    
    sobj_o.self           = data;
    sobj_o.polling        = 1;
    sobj_o.sensor_operate = mcmag_orientation_operate;

    if ((err = hwmsen_attach(ID_ORIENTATION, &sobj_o)))
    {
        MSE_ERR("attach fail = %d\n", err);
//        goto exit_kfree;
    }


    #ifdef MCMAG_DRV_SUPPORT_VGYRO
    {
        struct hwmsen_object    _tSensorObject;
    
        if ((err = misc_register(&mcmag_vgyro_device)))
            MSE_ERR("mcmag_vgyro_device register failed\n");
    
        _tSensorObject.polling = 1;
        _tSensorObject.sensor_operate = mcmag_vgyro_operate;
    
        if((err = hwmsen_attach(ID_GYROSCOPE, &_tSensorObject)))
            MSE_ERR("attach fail = %d\n", err);
    }
    #endif

    
    #if CONFIG_HAS_EARLYSUSPEND
        data->tDrv_EarlySuspend.level   = (EARLY_SUSPEND_LEVEL_DISABLE_FB - 1),
        data->tDrv_EarlySuspend.suspend = mcmag_early_suspend,
        data->tDrv_EarlySuspend.resume  = mcmag_late_resume,    
        register_early_suspend(&data->tDrv_EarlySuspend);
    #endif
    
    MSE_LOG("%s: OK\n", __func__);
    return (MCMAG_DRV_RETCODE_OK);

exit_sysfs_create_group_failed:   
exit_init_failed:
    //i2c_detach_client(new_client);
exit_misc_device_register_failed:
exit_kfree:
    kfree(data);
exit:
    MSE_ERR("[%s] err = %d\n", __func__, err);

    return (err);
}

/*****************************************
 *** mcmag_i2c_remove
 *****************************************/
static int    mcmag_i2c_remove(struct i2c_client *ptClient)
{
    MSE_FUN();    

    if (mcmag_delete_attr(&s_tMcmag_SensorDriver.driver))
        MSE_ERR("ERR: mcmag_delete_attr fail\n");
    
    s_ptMcmag_i2c_client = NULL;

    i2c_unregister_device(ptClient);
    kfree(i2c_get_clientdata(ptClient));	
    misc_deregister(&mcmag_device);

    #ifdef MCMAG_DRV_SUPPORT_VGYRO
    	misc_deregister(&mcmag_vgyro_device);
    #endif

    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** mcmag_probe
 *****************************************/
static int    mcmag_probe(struct platform_device *pPlatformDevice) 
{
    #if (MCMAG_DRV_SYSCFG_KERNEL_VERSION == MCMAG_DRV_SYSCFG_LINUX_KERNEL_2_6_X)
        struct mag_hw   *_ptHW = get_cust_mag_hw();
    #endif
    
    MSE_FUN();    

    rwlock_init(&s_tMcmag_MiddlewareData.tControlLock);
    rwlock_init(&s_tMcmag_MiddlewareData.tDataLock);

    s_tMcmag_MiddlewareData.naMidControlData[0] = 20;  // sample delay
    s_tMcmag_MiddlewareData.naMidControlData[1] =  0;  // reserved
    s_tMcmag_MiddlewareData.naMidControlData[2] =  0;  // reserved
    s_tMcmag_MiddlewareData.naMidControlData[3] =  0;  // reserved
    s_tMcmag_MiddlewareData.naMidControlData[4] =  0;  // reserved
    s_tMcmag_MiddlewareData.naMidControlData[5] =  0;  // reserved
    s_tMcmag_MiddlewareData.naMidControlData[6] =  0;  // reserved
    s_tMcmag_MiddlewareData.naMidControlData[7] =  0;  // reserved
    s_tMcmag_MiddlewareData.naMidControlData[8] =  0;  // reserved
    s_tMcmag_MiddlewareData.naMidControlData[9] =  0;  // reserved

    #if (MCMAG_DRV_SYSCFG_KERNEL_VERSION == MCMAG_DRV_SYSCFG_LINUX_KERNEL_2_6_X)
        s_tMcmag_force[0] = _ptHW->i2c_num;
    #endif

    if (i2c_add_driver(&s_tMcmag_i2c_driver))
    {
        MSE_ERR("ERR: add driver error\n");
        return (-EIO);
    } 

    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** mcmag_remove
 *****************************************/
static int    mcmag_remove(struct platform_device *pPlatformDevice)
{
    MSE_FUN();    

    atomic_set(&s_tMcmag_DeviceOpenCount, 0);  
    i2c_del_driver(&s_tMcmag_i2c_driver);

    return (MCMAG_DRV_RETCODE_OK);
}

/*****************************************
 *** PLATFORM DRIVER:: s_tMcmag_SensorDriver
 *****************************************/
 #if 0
static struct platform_driver s_tMcmag_SensorDriver = {
                                                          .probe     = mcmag_probe,
                                                          .remove    = mcmag_remove,    
                                                          .driver    = {
                                                                           .name  = "msensor",
                                                                           .owner = THIS_MODULE,
                                                                       }
                                                      };
#endif

#ifdef CONFIG_OF
static const struct of_device_id mcmag_of_match[] = {
	{ .compatible = "mediatek,msensor", },
	{},
};
#endif

static struct platform_driver s_tMcmag_SensorDriver =
{
	.probe      = mcmag_probe,
	.remove     = mcmag_remove,    
	.driver     = 
	{
		.name = "msensor",
        #ifdef CONFIG_OF
		.of_match_table = mcmag_of_match,
		#endif
	}
};


/*****************************************
 *** mcmag_init
 *****************************************/
static int __init    mcmag_init(void)
{
    #if (MCMAG_DRV_SYSCFG_KERNEL_VERSION == MCMAG_DRV_SYSCFG_LINUX_KERNEL_3_0_X)
        struct mag_hw   *_ptHW = get_cust_mag_hw();
    #endif

    MSE_FUN();

    #if (MCMAG_DRV_SYSCFG_KERNEL_VERSION == MCMAG_DRV_SYSCFG_LINUX_KERNEL_3_0_X)
        i2c_register_board_info(_ptHW->i2c_num, &s_tMcmag_i2c_board_info, 1);
    #endif

    if (platform_driver_register(&s_tMcmag_SensorDriver))
    {
        MSE_ERR("ERR: failed to register driver");
        return (-ENODEV);
    }

    return (MCMAG_DRV_RETCODE_OK);    
}

/*****************************************
 *** mcmag_exit
 *****************************************/
static void __exit    mcmag_exit(void)
{
    MSE_FUN();

    platform_driver_unregister(&s_tMcmag_SensorDriver);
}

/*----------------------------------------------------------------------------*/
module_init(mcmag_init);
module_exit(mcmag_exit);
/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("mCube-inc");
MODULE_DESCRIPTION("MC41XX M-Sensor Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(MCMAG_DRV_DRIVER_VERSION);

