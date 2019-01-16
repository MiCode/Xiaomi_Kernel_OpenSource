/* drivers/i2c/chips/mc6420.c - MC6420 compass driver
 *
 * Copyright (C) 2012 mCube Inc.
 * Author:
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
#include <linux/time.h>
#include <linux/hrtimer.h>
 
 
 
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
 
 
#define POWER_NONE_MACRO MT65XX_POWER_NONE
 
#include <cust_mag.h>
#include <linux/hwmsen_helper.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include "mc6420.h"


/*******************************************************************************
 *** CONSTANT / DEFINITION
 *******************************************************************************/
/**************************
 *** CONFIGURATION
 **************************/
#define MCMAG_DRV_SYSCFG_LINUX_KERNEL_2_6_X    (1)
#define MCMAG_DRV_SYSCFG_LINUX_KERNEL_3_0_X    (2)

#define MCMAG_DRV_SYSCFG_KERNEL_VERSION    (MCMAG_DRV_SYSCFG_LINUX_KERNEL_3_0_X)

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

//#define _MCMAG_DRV_AMI_COMPATIBLE_MODE_
    #define MCMAG_DRV_AMI304MODE_DATA_CONVERSION_FACTOR    3    // recommended value: 2, or 3
    #define MCMAG_CONVERT_M                                25
    #define MCMAG_ORIENTATION_ACCURACY_RATE                10

/**************************
 *** MTK
 **************************/

/**************************
 *** mCube
 **************************/
#define MCMAG_DRV_DISABLE    0
#define MCMAG_DRV_ENABLE     1

#define MCMAG_DRV_AXIS_X      0
#define MCMAG_DRV_AXIS_Y      1
#define MCMAG_DRV_AXIS_Z      2
#define MCMAG_DRV_AXES_NUM    3

#define MCMAG_DRV_DATA_LEN       6
#define MCMAG_DRV_MAX_MSG_LEN    10

#define MCMAG_DRV_DEVICE_NAME       "MCMAG"
#define MCMAG_DRV_DRIVER_VERSION    "1.0.1"

#define MCMAG_DRV_DAEMON_NAME       "mc6420d"

#define MCMAG_DRV_DEFAULT_SAMPLE_DELAY    (10)

#define MCMAG_DRV_SELFTEST_STATUS_INACTIVE            (0x00000000)
#define MCMAG_DRV_SELFTEST_STATUS_RUNNING_POSITIVE    (0x00000001)
#define MCMAG_DRV_SELFTEST_STATUS_RUNNING_NEGATIVE    (0x00000002)
#define MCMAG_DRV_SELFTEST_STATUS_RUNNING_END         (0x00000003)

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

    // control block
    s_McmagMagControl    tControl;

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

static u8     s_baMsgBuffer[MCMAG_DRV_MAX_MSG_LEN];
static u32    s_dwMamagRangeMultiplier = 40;
static u32    s_dwIsSelfTestRunning    = 0;

static atomic_t    s_tMcmag_DeviceOpenCount       = ATOMIC_INIT(0);
static atomic_t    s_tMcmag_FlagOpen              = ATOMIC_INIT(0);
static atomic_t    s_tMcmag_FlagMagSensor         = ATOMIC_INIT(0);
static atomic_t    s_tMcmag_FlagOrientationSensor = ATOMIC_INIT(0);

static struct _mcmagmid_data     s_tMcmag_MiddlewareData;
static struct platform_driver    s_tMcmag_SensorDriver;

static struct i2c_client                *s_ptMcmag_i2c_client = NULL;
static const struct i2c_device_id        s_tMcmag_i2c_id[]    = { {MCMAG_DRV_DEVICE_NAME, 0}, {} };

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

//====================================================================================================
const s_McmagConfig    c_tMcmagDefaults = { E_MCMAG_STATE_STANDBY,
                                            E_MCMAG_MEAS_NORMAL,
                                            { E_MCMAG_RATE_100Hz, MCMAG_DEFAULT_SETUP7_HF, MCMAG_DEFAULT_SAMPLE_RATE_DIVIDER },
                                            E_MCMAG_FIELD_RANGE_2,
                                            { E_MCMAG_IPP_OPEN_DRAIN, E_MCMAG_IAH_ACTIVE_LOW, MCMAG_DEFAULT_ENABLE3 },
                                            MCMAG_DEFAULT_ENABLE1,
                                          };

/*******************************************************************************
 *** FUNCTIONs
 *******************************************************************************/
/*****************************************
 *** mCube_SensorWrite
 *****************************************/
mCubeResult_t    mCube_SensorWrite(void *pDevID, u8 *pbDataBuf, int nDataLength)
{
    if ((NULL == pbDataBuf) || (nDataLength < 2))
        return (MCUBE_FAILURE);

    if (0 > hwmsen_write_block(s_ptMcmag_i2c_client, pbDataBuf[0], &pbDataBuf[1], (nDataLength - 1)))
    {
        MSE_ERR("[%s] ERR: i2c write block fail\n", __FUNCTION__);
        return (MCUBE_FAILURE);
    }

    return (MCUBE_SUCCESS);
}

/*****************************************
 *** mCube_SensorRead
 *****************************************/
mCubeResult_t    mCube_SensorRead(void *pDevID, u8 bCmd, int nRcvBufLength, u8 *pbRcvBuf)
{
    if ((NULL == pbRcvBuf) || (nRcvBufLength < 1))
        return (MCUBE_FAILURE);

    if (0 > hwmsen_read_block(s_ptMcmag_i2c_client, bCmd, pbRcvBuf, nRcvBufLength))
    {
        MSE_ERR("[%s] ERR: i2c read fail\n", __FUNCTION__);
        return (MCUBE_FAILURE);
    }
    
    return (MCUBE_SUCCESS);
}

/*****************************************
 *** _mCube_ClearMessageBuffer
 *****************************************/
void    _mCube_ClearMessageBuffer(void)
{
    memset(s_baMsgBuffer, 0, MCMAG_DRV_MAX_MSG_LEN);
}

/*****************************************
 *** MCMAG_ConfigRegSetup1_MeasRate
 *****************************************/
mCubeResult_t    MCMAG_ConfigRegSetup1_MeasRate(s_McmagMagControl *ptMagControl)
{
    u8   *_pbMsgBuf = s_baMsgBuffer;

    MSE_FUN();

    _mCube_ClearMessageBuffer();
    
    *_pbMsgBuf = MCMAG_REG_SETUP1_RATE_MEAS;
    
    _pbMsgBuf++;
    
    switch (ptMagControl->tMagConfig.eMeasCfg)
    {
        case E_MCMAG_MEAS_NORMAL           :   *_pbMsgBuf |= MCMAG_SETUP1_MEAS_NORMAL;               break;
        case E_MCMAG_MEAS_SELFTEST_POSITIVE:   *_pbMsgBuf |= MCMAG_SETUP1_MEAS_SELFTEST_POSITIVE;    break;
        case E_MCMAG_MEAS_SELFTEST_NEGATIVE:   *_pbMsgBuf |= MCMAG_SETUP1_MEAS_SELFTEST_NEGATIVE;    break;
        default                            :    return (MCUBE_FAILURE);                              break;
    }
    
    switch (ptMagControl->tMagConfig.tSampleRateCfg.eRateCfg)
    {
        case E_MCMAG_RATE_10Hz :   *_pbMsgBuf |= MCMAG_SAMPLE_RATE_10Hz;     break;
        case E_MCMAG_RATE_25Hz :   *_pbMsgBuf |= MCMAG_SAMPLE_RATE_30Hz;     break;
        case E_MCMAG_RATE_100Hz:   *_pbMsgBuf |= MCMAG_SAMPLE_RATE_100Hz;    break;
        case E_MCMAG_RATE_200Hz:   *_pbMsgBuf |= MCMAG_SAMPLE_RATE_200Hz;    break;
        default                :    return (MCUBE_FAILURE);                  break;
    }
    
    _pbMsgBuf++;
    
    return (mCube_SensorWrite(ptMagControl->pDeviceID, s_baMsgBuffer, (_pbMsgBuf - s_baMsgBuffer)));
}

/*****************************************
 *** MCMAG_ConfigRegSetup2_State
 *****************************************/
mCubeResult_t    MCMAG_ConfigRegSetup2_State(s_McmagMagControl *ptMagControl)
{
    u8   *_pbMsgBuf = s_baMsgBuffer;

    MSE_FUN();

    _mCube_ClearMessageBuffer();
    
    *_pbMsgBuf = MCMAG_REG_SETUP2_STATE;
    
    _pbMsgBuf++;
    
    switch (ptMagControl->tMagConfig.eActiveState)
    {
        case E_MCMAG_STATE_WAKE   :    *_pbMsgBuf |= MCMAG_SETUP2_STATE_CONTINUOUS;    break;
        case E_MCMAG_STATE_SINGLE :    *_pbMsgBuf |= MCMAG_SETUP2_STATE_SINGLE;        break;
        case E_MCMAG_STATE_STANDBY:    *_pbMsgBuf |= MCMAG_SETUP2_STATE_STANDBY;       break;
        default                   :     return (MCUBE_FAILURE);                        break;
    }
    
    _pbMsgBuf++;
    
    return (mCube_SensorWrite(ptMagControl->pDeviceID, s_baMsgBuffer, (_pbMsgBuf - s_baMsgBuffer)));
}

/*****************************************
 *** MCMAG_ConfigRegSetup6_Range
 *****************************************/
mCubeResult_t    MCMAG_ConfigRegSetup6_Range(s_McmagMagControl *ptMagControl)
{
    u8   *_pbMsgBuf = s_baMsgBuffer;

    MSE_FUN();

    _mCube_ClearMessageBuffer();
    
    *_pbMsgBuf = MCMAG_REG_SETUP6_EXOFF2;
    
    _pbMsgBuf++;
    
    switch (ptMagControl->tMagConfig.eRangeCfg)
    {
        case E_MCMAG_FIELD_RANGE_1:   *_pbMsgBuf |= MCMAG_SETUP6_FIELD_RANGE_1;    break;
        case E_MCMAG_FIELD_RANGE_2:   *_pbMsgBuf |= MCMAG_SETUP6_FIELD_RANGE_2;    break;
        case E_MCMAG_FIELD_RANGE_3:   *_pbMsgBuf |= MCMAG_SETUP6_FIELD_RANGE_3;    break;
        case E_MCMAG_FIELD_RANGE_4:   *_pbMsgBuf |= MCMAG_SETUP6_FIELD_RANGE_4;    break;
        case E_MCMAG_FIELD_RANGE_5:   *_pbMsgBuf |= MCMAG_SETUP6_FIELD_RANGE_5;    break;
        default                   :    return (MCUBE_FAILURE);                     break;
    }
    
    _pbMsgBuf++;
    
    return (mCube_SensorWrite(ptMagControl->pDeviceID, s_baMsgBuffer, (_pbMsgBuf - s_baMsgBuffer)));
}

/*****************************************
 *** MCMAG_ConfigRegSetup7ClockRate
 *****************************************/
mCubeResult_t    MCMAG_ConfigRegSetup7ClockRate(s_McmagMagControl *ptMagControl)
{
    u8   *_pbMsgBuf = s_baMsgBuffer;

    MSE_FUN();

    _mCube_ClearMessageBuffer();
    
    *_pbMsgBuf = MCMAG_REG_SETUP7_EXGAIN1;
    
    _pbMsgBuf++;
    
    if (ptMagControl->tMagConfig.tSampleRateCfg.bIsHingFreqON)
        *_pbMsgBuf |= MCMAG_SETUP7_HF_DIGTAL_CLOCK_8MHz;
    
    _pbMsgBuf++;

    return (mCube_SensorWrite(ptMagControl->pDeviceID, s_baMsgBuffer, (_pbMsgBuf - s_baMsgBuffer)));
}

/*****************************************
 *** MCMAG_ConfigRegEnable1
 *****************************************/
mCubeResult_t    MCMAG_ConfigRegEnable1(s_McmagMagControl *ptMagControl)
{
    u8   *_pbMsgBuf = s_baMsgBuffer;

    MSE_FUN();

    _mCube_ClearMessageBuffer();
    
    *_pbMsgBuf = MCMAG_REG_ENABLE1;
    
    _pbMsgBuf++;
    
    *_pbMsgBuf |= ptMagControl->tMagConfig.bEnable1Cfg;
    
    _pbMsgBuf++;
    
    return (mCube_SensorWrite(ptMagControl->pDeviceID, s_baMsgBuffer, (_pbMsgBuf - s_baMsgBuffer)));
}

/*****************************************
 *** MCMAG_ConfigRegEnable2
 *****************************************/
mCubeResult_t    MCMAG_ConfigRegEnable2(s_McmagMagControl *ptMagControl)
{
    u8   *_pbMsgBuf = s_baMsgBuffer;

    MSE_FUN();

    _mCube_ClearMessageBuffer();
    
    *_pbMsgBuf = MCMAG_REG_ENABLE2;
    
    _pbMsgBuf++;
    
    switch (ptMagControl->tMagConfig.tIntCfg.eIntPinCfg)
    {
        case E_MCMAG_IPP_OPEN_DRAIN:   *_pbMsgBuf |= MCMAG_ENABLE2_IPP_OPEN_DRAIN;    break;
        case E_MCMAG_IPP_PUSH_PULL :   *_pbMsgBuf |= MCMAG_ENABLE2_IPP_PUSH_PULL;     break;
        default                    :   *_pbMsgBuf |= MCMAG_ENABLE2_IPP_OPEN_DRAIN;    break;
    }
    
    switch (ptMagControl->tMagConfig.tIntCfg.eIntPinLevel)
    {
        case E_MCMAG_IAH_ACTIVE_LOW :   *_pbMsgBuf |= MCMAG_ENABLE2_IAH_ACTIVE_LOW;     break;
        case E_MCMAG_IAH_ACTIVE_HIGH:   *_pbMsgBuf |= MCMAG_ENABLE2_IAH_ACTIVE_HIGH;    break;
        default                     :   *_pbMsgBuf |= MCMAG_ENABLE2_IAH_ACTIVE_HIGH;    break;
    }
    
    _pbMsgBuf++;
    
    return (mCube_SensorWrite(ptMagControl->pDeviceID, s_baMsgBuffer, (_pbMsgBuf - s_baMsgBuffer)));
}

/*****************************************
 *** MCMAG_ConfigRegEnable3
 *****************************************/
mCubeResult_t    MCMAG_ConfigRegEnable3(s_McmagMagControl *ptMagControl)
{
    u8   *_pbMsgBuf = s_baMsgBuffer;

    MSE_FUN();

    _mCube_ClearMessageBuffer();
    
    *_pbMsgBuf = MCMAG_REG_ENABLE3_INTREN;
    
    _pbMsgBuf++;
    
    *_pbMsgBuf |= ((u8) ptMagControl->tMagConfig.tIntCfg.wIntEnableFlag);
    
    _pbMsgBuf++;
    
    return (mCube_SensorWrite(ptMagControl->pDeviceID, s_baMsgBuffer, (_pbMsgBuf - s_baMsgBuffer)));
}

/*****************************************
 *** MCMAG_ConfigRegPowerOnReset
 *****************************************/
mCubeResult_t    MCMAG_ConfigRegPowerOnReset(s_McmagMagControl *ptMagControl)
{
    u8   *_pbMsgBuf = s_baMsgBuffer;

    MSE_FUN();

    _mCube_ClearMessageBuffer();
    
    *_pbMsgBuf = MCMAG_REG_POWER_ON_RESET;
    
    _pbMsgBuf++;
    
    *_pbMsgBuf = MCMAG_DRV_ENABLE;
    
    _pbMsgBuf++;
    
    return (mCube_SensorWrite(ptMagControl->pDeviceID, s_baMsgBuffer, (_pbMsgBuf - s_baMsgBuffer)));
}

/*****************************************
 *** MCMAG_ReadChipDefaults
 *****************************************/
mCubeResult_t    MCMAG_ReadChipDefaults(u8 bChipID, s_McmagConfig *ptMagCfg)
{
    MSE_FUN();

    switch (bChipID)
    {
        case MCUBE_UNPROGRAMMED_ID:
        case MCUBE_MARS_CHIPID0:
        case MCUBE_MARS_CHIPID1:
        case MCUBE_MARS_CHIPID2:
             *ptMagCfg = c_tMcmagDefaults;
             break;
    
        default:
             return (MCUBE_ACCEL_UNKNOWN_CHIPID);
             break;
    }
    
    return (MCUBE_SUCCESS);
}

/*****************************************
 *** MCMAG_SetFieldRange
 *****************************************/
void    MCMAG_SetFieldRange(void)
{
    u8    _bRange;

    MSE_FUN();

    mCube_SensorRead(s_tMcmag_MiddlewareData.tControl.pDeviceID, MCMAG_REG_SETUP6_EXOFF2, sizeof(_bRange), &_bRange);
    
    switch (_bRange & MCMAG_SETUP6_FIELD_RANGE_MASK)
    {
        case MCMAG_SETUP6_FIELD_RANGE_1:    s_dwMamagRangeMultiplier = 80;    break;
        case MCMAG_SETUP6_FIELD_RANGE_2:    s_dwMamagRangeMultiplier = 40;    break;
        case MCMAG_SETUP6_FIELD_RANGE_3:    s_dwMamagRangeMultiplier = 30;    break;
        case MCMAG_SETUP6_FIELD_RANGE_4:    s_dwMamagRangeMultiplier = 25;    break;
        case MCMAG_SETUP6_FIELD_RANGE_5:    s_dwMamagRangeMultiplier = 20;    break;
        default                        :    s_dwMamagRangeMultiplier = 40;    break;
    }
}

/*****************************************
 *** MCMAG_SetMode
 *****************************************/
static int    MCMAG_SetMode(int nNewMode)
{
    MSE_LOG("eActiveState: %d, nNewMode: %d", s_tMcmag_MiddlewareData.tControl.tMagConfig.eActiveState, nNewMode);

    //if (nNewMode == s_tMcmag_MiddlewareData.tControl.tMagConfig.eActiveState)
    //    return (-1);

    s_tMcmag_MiddlewareData.tControl.tMagConfig.eActiveState = nNewMode;

    return (MCMAG_ConfigRegSetup2_State(&s_tMcmag_MiddlewareData.tControl));
}

/*****************************************
 *** MCMAG_InitMagSensor
 *****************************************/
mCubeResult_t    MCMAG_InitMagSensor(s_McmagMagControl *ptMagControl)
{
    e_McmagSetup2State    _eActiveStateBackup;

    MSE_FUN();

    if (NULL == ptMagControl)
        return (MCUBE_FAILURE);

    if (E_MCMAG_INIT_STATUS_INITIALIZED == ptMagControl->eInitStatus)
        return (MCUBE_SUCCESS);

    if (!ptMagControl->bIsChipIdValid)
    {
        if (MCUBE_SUCCESS != mCube_SensorRead(ptMagControl->pDeviceID      ,
                                              ((u8) MCMAG_REG_CHIPID)      ,
                                              sizeof(ptMagControl->bChipID),
                                              &(ptMagControl->bChipID)      ))
        {
            return (MCUBE_MAG_INVALID_CHIP_ID);
        }

        ptMagControl->bIsChipIdValid = true;
    }

    _eActiveStateBackup = ptMagControl->tMagConfig.eActiveState;

    MCMAG_SetMode(E_MCMAG_STATE_STANDBY);

    MCMAG_ConfigRegSetup1_MeasRate(ptMagControl);
    MCMAG_ConfigRegSetup6_Range(ptMagControl);
    MCMAG_ConfigRegEnable1(ptMagControl);
    MCMAG_ConfigRegSetup7ClockRate(ptMagControl);
    MCMAG_ConfigRegEnable2(ptMagControl);
    MCMAG_ConfigRegEnable3(ptMagControl);
    
    ptMagControl->ePowerState = E_MCMAG_STATE_STANDBY;
    ptMagControl->eInitStatus = E_MCMAG_INIT_STATUS_INITIALIZED;

    MCMAG_SetMode(_eActiveStateBackup);

    return (MCUBE_SUCCESS);
}

/*****************************************
 *** MCMAG_Start
 *****************************************/
mCubeResult_t    MCMAG_Start(s_McmagMagControl *ptMagControl)
{
    MSE_FUN();

    if (NULL == ptMagControl)
        return (MCUBE_FAILURE);

    if (E_MCMAG_INIT_STATUS_INITIALIZED != ptMagControl->eInitStatus)
        return (MCUBE_FAILURE);

    if (E_MCMAG_STATE_STANDBY == ptMagControl->ePowerState)
    {
        ptMagControl->tMagConfig.eActiveState = E_MCMAG_STATE_WAKE;
        
        if (MCUBE_SUCCESS == MCMAG_ConfigRegSetup2_State(ptMagControl))
            ptMagControl->ePowerState = E_MCMAG_STATE_WAKE;
    }

    return (MCUBE_SUCCESS);
}

/*****************************************
 *** MCMAG_Stop
 *****************************************/
mCubeResult_t    MCMAG_Stop(s_McmagMagControl *ptMagControl)
{
    MSE_FUN();

    if (NULL == ptMagControl)
        return (MCUBE_FAILURE);

    if (E_MCMAG_INIT_STATUS_INITIALIZED != ptMagControl->eInitStatus)
        return (MCUBE_MAG_UNINITIALIZED);

    ptMagControl->tMagConfig.eActiveState = E_MCMAG_STATE_STANDBY;
    ptMagControl->ePowerState             = E_MCMAG_STATE_STANDBY;

    MCMAG_ConfigRegSetup2_State(ptMagControl);

    return (MCUBE_SUCCESS);
}

/*****************************************
 *** MCMAG_ReadMagSensorData
 *****************************************/
static int    MCMAG_ReadMagSensorData(char *pszBuf, int nBufSize)
{
    u8     _bDataReadyStatus = 0;
    u8     _baDataBuffer[MCMAG_DRV_DATA_LEN]    = { 0 };
    s16    _waRawData[MCMAG_DRV_AXES_NUM]       = { 0 };
    s32    _naConvertedData[MCMAG_DRV_AXES_NUM] = { 0 };
    int    _naFinalData[MCMAG_DRV_AXES_NUM]     = { 0 };

    struct timeval            _tTimeStamp;
    struct _mcmag_i2c_data   *_pt_i2c_data = i2c_get_clientdata(s_ptMcmag_i2c_client);

    MSE_FUN();

    MCMAG_Start(&s_tMcmag_MiddlewareData.tControl);

    if (MCUBE_SUCCESS != mCube_SensorRead(s_tMcmag_MiddlewareData.tControl.pDeviceID, MCMAG_REG_STATUS1_STATUS, sizeof(_bDataReadyStatus), &_bDataReadyStatus))
        return (-1);

    if (!(_bDataReadyStatus & MCMAG_STATUS1_ACQ_DATA_READY))
        return (-1);

    mCube_SensorRead(s_tMcmag_MiddlewareData.tControl.pDeviceID, MCMAG_REG_CH1_OUT_LSB, sizeof(_baDataBuffer), _baDataBuffer);

    _waRawData[MCMAG_DRV_AXIS_X] = (_baDataBuffer[0] | (_baDataBuffer[1] << 8));
    _waRawData[MCMAG_DRV_AXIS_Y] = (_baDataBuffer[2] | (_baDataBuffer[3] << 8));
    _waRawData[MCMAG_DRV_AXIS_Z] = (_baDataBuffer[4] | (_baDataBuffer[5] << 8));
    
    #ifdef _MCMAG_DRV_AMI_COMPATIBLE_MODE_
        _naConvertedData[MCMAG_DRV_AXIS_X] = (((_waRawData[MCMAG_DRV_AXIS_X] / MCMAG_DRV_AMI304MODE_DATA_CONVERSION_FACTOR) * 2048) / 32768);
        _naConvertedData[MCMAG_DRV_AXIS_Y] = (((_waRawData[MCMAG_DRV_AXIS_Y] / MCMAG_DRV_AMI304MODE_DATA_CONVERSION_FACTOR) * 2048) / 32768);
        _naConvertedData[MCMAG_DRV_AXIS_Z] = (((_waRawData[MCMAG_DRV_AXIS_Z] / MCMAG_DRV_AMI304MODE_DATA_CONVERSION_FACTOR) * 2048) / 32768);
    #else
        _naConvertedData[MCMAG_DRV_AXIS_X] = (int)(_waRawData[MCMAG_DRV_AXIS_X] * (1000 / s_dwMamagRangeMultiplier));
        _naConvertedData[MCMAG_DRV_AXIS_Y] = (int)(_waRawData[MCMAG_DRV_AXIS_Y] * (1000 / s_dwMamagRangeMultiplier));
        _naConvertedData[MCMAG_DRV_AXIS_Z] = (int)(_waRawData[MCMAG_DRV_AXIS_Z] * (1000 / s_dwMamagRangeMultiplier));
        
        do_gettimeofday(&_tTimeStamp);
    #endif

    _naFinalData[_pt_i2c_data->tCvt.map[MCMAG_DRV_AXIS_X]] = (_pt_i2c_data->tCvt.sign[MCMAG_DRV_AXIS_X] * _naConvertedData[MCMAG_DRV_AXIS_X]);
    _naFinalData[_pt_i2c_data->tCvt.map[MCMAG_DRV_AXIS_Y]] = (_pt_i2c_data->tCvt.sign[MCMAG_DRV_AXIS_Y] * _naConvertedData[MCMAG_DRV_AXIS_Y]);
    _naFinalData[_pt_i2c_data->tCvt.map[MCMAG_DRV_AXIS_Z]] = (_pt_i2c_data->tCvt.sign[MCMAG_DRV_AXIS_Z] * _naConvertedData[MCMAG_DRV_AXIS_Z]);
    
    sprintf(pszBuf, "%04x %04x %04x %ld %ld", _naFinalData[MCMAG_DRV_AXIS_X], _naFinalData[MCMAG_DRV_AXIS_Y], _naFinalData[MCMAG_DRV_AXIS_Z], _tTimeStamp.tv_sec, _tTimeStamp.tv_usec);

    return (0);
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
 *** MCMAG_ConfigGPIO
 *****************************************/
#if 0
static int    MCMAG_ConfigGPIO(void)
{
    mt_set_gpio_mode       (GPIO_MSE_EINT_PIN, GPIO_MSE_EINT_PIN_M_GPIO);
    mt_set_gpio_dir        (GPIO_MSE_EINT_PIN, GPIO_DIR_IN             );
	mt_set_gpio_pull_enable(GPIO_MSE_EINT_PIN, GPIO_PULL_ENABLE        );
	mt_set_gpio_pull_select(GPIO_MSE_EINT_PIN, GPIO_PULL_DOWN          );

	return 0;
}
#endif
/*****************************************
 *** MCMAG_InitChipset
 *****************************************/
static int    MCMAG_InitChipset(int nMode)
{
    MSE_FUN();

    //MCMAG_ConfigGPIO();

    s_ptMcmag_i2c_client->addr = ((s_ptMcmag_i2c_client->addr & I2C_MASK_FLAG) | (I2C_ENEXT_FLAG));

    MCMAG_ReadChipDefaults(s_tMcmag_MiddlewareData.tControl.bChipID, &s_tMcmag_MiddlewareData.tControl.tMagConfig);

    if (MCMAG_DRV_SELFTEST_STATUS_RUNNING_POSITIVE == s_dwIsSelfTestRunning)
        s_tMcmag_MiddlewareData.tControl.tMagConfig.eMeasCfg = E_MCMAG_MEAS_SELFTEST_POSITIVE;
    else if (MCMAG_DRV_SELFTEST_STATUS_RUNNING_NEGATIVE == s_dwIsSelfTestRunning)
        s_tMcmag_MiddlewareData.tControl.tMagConfig.eMeasCfg = E_MCMAG_MEAS_SELFTEST_NEGATIVE;

    s_tMcmag_MiddlewareData.tControl.eInitStatus = E_MCMAG_INIT_STATUS_UNINITIALIZED;

    MCMAG_InitMagSensor(&s_tMcmag_MiddlewareData.tControl);

    return (MCUBE_SUCCESS);
}

/*****************************************
 *** MCMAG_ReadChipInfo
 *****************************************/
static int    MCMAG_ReadChipInfo(char *pszBuf, int nBufSize)
{
    if ((NULL == pszBuf) || (nBufSize <= 16))
        return (-1);
    
    sprintf(pszBuf, "MC6420 Chip");
    
    return (0);
}

/*****************************************
 *** MCMAG_ReadPostureData
 *****************************************/
static int    MCMAG_ReadPostureData(char *pszBuf, int nBufSize)
{
    MSE_FUN();

    if ((NULL == pszBuf) || (nBufSize < 32))
        return (-1);
    
    read_lock(&s_tMcmag_MiddlewareData.tDataLock);
    
        sprintf(pszBuf                                      ,
                "%d %d %d %d"                               ,
                s_tMcmag_MiddlewareData.nOrientation_Heading,
                s_tMcmag_MiddlewareData.nOrientation_Pitch  ,
                s_tMcmag_MiddlewareData.nOrientation_Roll   ,
                s_tMcmag_MiddlewareData.nMag_Status          );
    
    read_unlock(&s_tMcmag_MiddlewareData.tDataLock);
    
    return (0);
}

/*****************************************
 *** MCMAG_ReadCaliData
 *****************************************/
static int    MCMAG_ReadCaliData(char *pszBuf, int nBufSize)
{
    MSE_FUN();

    if ((NULL == pszBuf) || (nBufSize < 64))
        return (-1);
    
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
    
    return (0);
}

/*****************************************
 *** MCMAG_ReadMiddleControl
 *****************************************/
static int    MCMAG_ReadMiddleControl(char *pszBuf, int nBufSize)
{
    if ((NULL == pszBuf) || (nBufSize < 64))
        return (-1);
    
    read_lock(&s_tMcmag_MiddlewareData.tControlLock);

        sprintf(pszBuf, "%d %d %d %d %d %d %d %d %d %d",
                s_tMcmag_MiddlewareData.naMidControlData[0], s_tMcmag_MiddlewareData.naMidControlData[1],
                s_tMcmag_MiddlewareData.naMidControlData[2], s_tMcmag_MiddlewareData.naMidControlData[3],
                s_tMcmag_MiddlewareData.naMidControlData[4], s_tMcmag_MiddlewareData.naMidControlData[5],
                s_tMcmag_MiddlewareData.naMidControlData[6], s_tMcmag_MiddlewareData.naMidControlData[7],
                s_tMcmag_MiddlewareData.naMidControlData[8], s_tMcmag_MiddlewareData.naMidControlData[9] );

    read_unlock(&s_tMcmag_MiddlewareData.tControlLock);

    return (0);
}

/*****************************************
 *** MCMAG_DoSelfTest: MACRO
 *****************************************/
#define _MCMAG_SELFTEST_INVALID_POSITIVE(data)    (( 1000 > data) || (data >  3500))
#define _MCMAG_SELFTEST_INVALID_NEGATIVE(data)    ((-1000 < data) || (data < -3500))

/*****************************************
 *** MCMAG_DoSelfTest
 *****************************************/
static void    MCMAG_DoSelfTest(char *pszBuf, int nBufSize)
{
    s16    _wRaw_PX = 0;
    s16    _wRaw_PY = 0;
    s16    _wRaw_PZ = 0;
    s16    _wRaw_NX = 0;
    s16    _wRaw_NY = 0;
    s16    _wRaw_NZ = 0;

    u32    _dwTestStatus;

    MSE_FUN();    

    if (s_dwIsSelfTestRunning)
        return;

    memset(pszBuf, 0, nBufSize);

    pszBuf[0] = 'P';
    pszBuf[1] = 'A';
    pszBuf[2] = 'S';
    pszBuf[3] = 'S';

    for (_dwTestStatus = MCMAG_DRV_SELFTEST_STATUS_RUNNING_POSITIVE; _dwTestStatus < MCMAG_DRV_SELFTEST_STATUS_RUNNING_END; _dwTestStatus++)
    {
        u8     _bStatus = 0xFF;
        u8     _baRawDataBuf[MCMAG_DRV_DATA_LEN] = { 0, 0, 0, 0, 0, 0 };

        MCMAG_SetMode(E_MCMAG_STATE_STANDBY);
        MCMAG_ConfigRegPowerOnReset(&s_tMcmag_MiddlewareData.tControl);

        s_dwIsSelfTestRunning = _dwTestStatus;
        MCMAG_InitChipset(E_MCMAG_STATE_STANDBY);

        MCMAG_SetMode(E_MCMAG_STATE_SINGLE);
        msleep(3);
        mCube_SensorRead(s_tMcmag_MiddlewareData.tControl.pDeviceID, MCMAG_REG_SETUP2_STATE, sizeof(_bStatus), &_bStatus);

        if (_bStatus & 0x03)
            goto _SELFTEST_ON_ERROR;

        mCube_SensorRead(s_tMcmag_MiddlewareData.tControl.pDeviceID, MCMAG_REG_CH1_OUT_LSB, sizeof(_baRawDataBuf), _baRawDataBuf);

        if (MCMAG_DRV_SELFTEST_STATUS_RUNNING_POSITIVE == s_dwIsSelfTestRunning)
        {
            _wRaw_PX = (_baRawDataBuf[0] | (_baRawDataBuf[1] << 8));
            _wRaw_PY = (_baRawDataBuf[2] | (_baRawDataBuf[3] << 8));
            _wRaw_PZ = (_baRawDataBuf[4] | (_baRawDataBuf[5] << 8));

            if (   _MCMAG_SELFTEST_INVALID_POSITIVE(_wRaw_PX)
                || _MCMAG_SELFTEST_INVALID_POSITIVE(_wRaw_PY)
                || _MCMAG_SELFTEST_INVALID_POSITIVE(_wRaw_PZ))
            {
                goto _SELFTEST_ON_ERROR;
            }
        }
        else //if (MCMAG_DRV_SELFTEST_STATUS_RUNNING_NEGATIVE == s_dwIsSelfTestRunning)
        {
            _wRaw_NX = (_baRawDataBuf[0] | (_baRawDataBuf[1] << 8));
            _wRaw_NY = (_baRawDataBuf[2] | (_baRawDataBuf[3] << 8));
            _wRaw_NZ = (_baRawDataBuf[4] | (_baRawDataBuf[5] << 8));

            if (   _MCMAG_SELFTEST_INVALID_NEGATIVE(_wRaw_NX)
                || _MCMAG_SELFTEST_INVALID_NEGATIVE(_wRaw_NY)
                || _MCMAG_SELFTEST_INVALID_NEGATIVE(_wRaw_NZ))
            {
                goto _SELFTEST_ON_ERROR;
            }
        }
    }

    goto _SELFTEST_DONE;

_SELFTEST_ON_ERROR:
    pszBuf[0] = 'F';
    pszBuf[1] = 'A';
    pszBuf[2] = 'I';
    pszBuf[3] = 'L';

_SELFTEST_DONE:
    sprintf(&pszBuf[4]                  ,
            "\nP: %d %d %d\nN: %d %d %d\n",
            _wRaw_PX, _wRaw_PY, _wRaw_PZ,
            _wRaw_NX, _wRaw_NY, _wRaw_NZ );

    MSE_LOG(">>> %s", pszBuf);

    s_dwIsSelfTestRunning = MCMAG_DRV_SELFTEST_STATUS_INACTIVE;
    MCMAG_InitChipset(E_MCMAG_STATE_STANDBY);
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
    int    _naNewControlData[10];

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

        return (0);
    }

    return ((sizeof(int) * 10));
}

/*****************************************
 *** show_middebug_value
 *****************************************/
static ssize_t    show_middebug_value(struct device_driver *ptDeviceDriver, char *pszBuf)
{
    return (0);
}

/*****************************************
 *** store_middebug_value
 *****************************************/
static ssize_t    store_middebug_value(struct device_driver *ptDeviceDriver, const char *pszBuf, size_t tCount)
{   
    return (0);
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
    return (0);
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
        return (-1);

    atomic_set(&_pt_i2c_data->tLayout, _nLayout);
    
    if (!hwmsen_get_convert(_nLayout, &_pt_i2c_data->tCvt))
        MSE_ERR("HWMSEN_GET_CONVERT function error!\r\n");
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
    return (0);
}

/*****************************************
 *** show_trace_value
 *****************************************/
static ssize_t    show_trace_value(struct device_driver *ptDeviceDriver, char *pszBuf)
{
    struct _mcmag_i2c_data   *_pt_i2c_data = i2c_get_clientdata(s_ptMcmag_i2c_client);
    
    if (NULL == _pt_i2c_data)
        return (-1);
    
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
        return (-1);
    
    if (1 != sscanf(pszBuf, "0x%x", &_nTrace))
        return (-1);

    atomic_set(&_pt_i2c_data->tTrace, _nTrace);
    
    return (tCount);
}

/*****************************************
 *** show_self_test
 *****************************************/
static ssize_t    show_self_test(struct device_driver *ptDeviceDriver, char *pszBuf)
{
    char    _szSelfTestDataBuf[MCMAG_BUFFER_SIZE] = { 0 };

    MCMAG_DoSelfTest(_szSelfTestDataBuf, MCMAG_BUFFER_SIZE);

    return (sprintf(pszBuf, "%s\n", _szSelfTestDataBuf));
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
    char    _szSelfTestDataBuf[MCMAG_BUFFER_SIZE] = { 0 };

    MCMAG_DoSelfTest(_szSelfTestDataBuf, MCMAG_BUFFER_SIZE);

    if (   ('P' == _szSelfTestDataBuf[0])
        && ('A' == _szSelfTestDataBuf[1])
        && ('S' == _szSelfTestDataBuf[2])
        && ('S' == _szSelfTestDataBuf[3]))
    {
        return (sprintf(pszBuf, "y\n"));
    }

    return (sprintf(pszBuf, "n\n"));
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
    
    return (0);
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
    
    return (0);
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

    return (0);
}

/*****************************************
 *** _operate_sensor_enable
 *****************************************/
void    _operate_sensor_enable(u32 dwType, int nSwitch)
{
    MSE_FUN();

    read_lock(&s_tMcmag_MiddlewareData.tControlLock);
    
        if (MCMAG_DRV_ENABLE == nSwitch)
        {
            if (SENSOR_ORIENTATION == dwType)
            {
                atomic_set(&s_tMcmag_FlagOrientationSensor, 1);

                #ifdef _MCMAG_DRV_AMI_COMPATIBLE_MODE_
                    s_tMcmag_MiddlewareData.naMidControlData[7] |= SENSOR_ORIENTATION;
                #endif
            }
            else    // if (SENSOR_MAGNETIC == _sensor_type)
            {
                atomic_set(&s_tMcmag_FlagMagSensor, 1);

                #ifdef _MCMAG_DRV_AMI_COMPATIBLE_MODE_
                    s_tMcmag_MiddlewareData.naMidControlData[7] |= SENSOR_MAGNETIC;
                #endif
            }

            atomic_set(&s_tMcmag_FlagOpen, 1);
        }
        else
        {
            if (SENSOR_ORIENTATION == dwType)
            {
                atomic_set(&s_tMcmag_FlagOrientationSensor, 0);

                #ifdef _MCMAG_DRV_AMI_COMPATIBLE_MODE_
                    s_tMcmag_MiddlewareData.naMidControlData[7] &= ~SENSOR_ORIENTATION;
                #endif

                if (0 == atomic_read(&s_tMcmag_FlagMagSensor))
                    atomic_set(&s_tMcmag_FlagOpen, 0);
            }
            else    // if (SENSOR_MAGNETIC == _sensor_type)
            {
                atomic_set(&s_tMcmag_FlagMagSensor, 0);

                #ifdef _MCMAG_DRV_AMI_COMPATIBLE_MODE_
                    s_tMcmag_MiddlewareData.naMidControlData[7] &= ~SENSOR_MAGNETIC;
                #endif

                if (0 == atomic_read(&s_tMcmag_FlagOrientationSensor))
                    atomic_set(&s_tMcmag_FlagOpen, 0);
            }
        }
        
        wake_up(&wqMcmag_Open);

    read_unlock(&s_tMcmag_MiddlewareData.tControlLock);
    
    if (MCMAG_DRV_ENABLE == nSwitch)
        MCMAG_Start(&s_tMcmag_MiddlewareData.tControl);
    else
        MCMAG_Stop(&s_tMcmag_MiddlewareData.tControl);
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
 *** mcmag_unlocked_ioctl
 *****************************************/
 
static long mcmag_unlocked_ioctl(struct file *ptFile, unsigned int nCmd, unsigned long lArg)
{
    void __user   *_pUserData;

    MSE_LOG("[%s] cmd: 0x%X\n", __func__, nCmd);

    switch (nCmd)
    {
        //================================================================    
        case MSENSOR_IOCTL_INIT:
        
             MCMAG_InitChipset(E_MCMAG_STATE_INVALID);         
             
             break;

        //================================================================    
        case ECOMPASS_IOC_GET_OFLAG:
        case ECOMPASS_IOC_GET_MFLAG:
             {
                 short          _wSensorStatus;
                 void __user   *_pArg = ((void __user *) lArg);

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

                 _nSensorsOpenStatus = MCMAG_GetOpenStatus();			
        
                 if(copy_to_user(_pArg, &_nSensorsOpenStatus, sizeof(_nSensorsOpenStatus)))
                     return (-EFAULT);
             }
             break;        

        //================================================================    
        case MSENSOR_IOCTL_SET_POSTURE:
             {
                 int    _naPostureData[4] = { 0 };

                 _IOCTL_CHK_ARG();
                 _IOCTL_COPY_FROM_USER(_naPostureData);
                 
                 write_lock(&s_tMcmag_MiddlewareData.tDataLock);
                     s_tMcmag_MiddlewareData.nOrientation_Heading = _naPostureData[0];
                     s_tMcmag_MiddlewareData.nOrientation_Pitch   = _naPostureData[1];
                     s_tMcmag_MiddlewareData.nOrientation_Roll    = _naPostureData[2];
                     s_tMcmag_MiddlewareData.nMag_Status          = _naPostureData[3];
                 write_unlock(&s_tMcmag_MiddlewareData.tDataLock);    
             }
             break;

        //================================================================    
        case MSENSOR_IOCTL_SET_CALIDATA:
             {
                 int    _naCaliData[7] = { 0 };

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
                 write_unlock(&s_tMcmag_MiddlewareData.tDataLock);    
             }
             break;                                

        //================================================================    
        case MSENSOR_IOCTL_SENSOR_ENABLE:
             {
                 int    _nEnable;

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
             {
                 int    _nMode;

                 _IOCTL_CHK_ARG();
                 _IOCTL_COPY_FROM_USER(_nMode)
                 
                 MCMAG_SetMode(_nMode);                
             }
             break;

        //=== DISABLE THIS, DUE TO CODE MANAGEMENT =======================    
        #if 0
        case MSENSOR_IOCTL_MCUBE_SET_SELF_TEST:
             {
                 char    _szStrBuf[MCMAG_BUFFER_SIZE] = { 0 };

                 _IOCTL_CHK_ARG();

                 MCMAG_DoSelfTest(_szStrBuf, MCMAG_BUFFER_SIZE);

                 if (copy_to_user(_pUserData, _szStrBuf, (strlen(_szStrBuf) + 1)))
                     return (-EFAULT);
             }

             break;
        #endif

        //================================================================    
        default:

             MSE_ERR("ERR: 0x%4x CMD not supported!", nCmd);
             return (-ENOIOCTLCMD);

             break;
    }

    return (0);
}

/*****************************************
 *** STATIC STRUCTURE:: fops
 *****************************************/
static struct file_operations    mcmag_fops = {
                                                  .owner          = THIS_MODULE,
                                                  .open           = mcmag_open,
                                                  .release        = mcmag_release,
                                                  .unlocked_ioctl = mcmag_unlocked_ioctl,
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
 *** INLINE:: _operate_accuracy_status_transformer
 *****************************************/
__inline void    _operate_accuracy_status_transformer(int nStatus, hwm_sensor_data *ptSensorData)
{
    switch (nStatus)
    {
        case 1 :    ptSensorData->status = SENSOR_STATUS_ACCURACY_HIGH;      break;
        case 2 :    ptSensorData->status = SENSOR_STATUS_ACCURACY_HIGH;      break;
        case 3 :    ptSensorData->status = SENSOR_STATUS_ACCURACY_MEDIUM;    break;
        case 4 :    ptSensorData->status = SENSOR_STATUS_ACCURACY_LOW;       break;
        default:    ptSensorData->status = SENSOR_STATUS_UNRELIABLE;         break;
    }
}

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
                 int    _nSampleDelay;

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
                 int                _nStatus         = 0;
                 hwm_sensor_data   *_ptMagSensorData = NULL;

                 _OPERATE_CHK_BUFF_AND_SIZE(pOutBuf, nOutBufSize, hwm_sensor_data);

                 _ptMagSensorData = ((hwm_sensor_data *) pOutBuf);
    
                 read_lock(&s_tMcmag_MiddlewareData.tDataLock);
                     _ptMagSensorData->values[0] = s_tMcmag_MiddlewareData.nMag_X;
                     _ptMagSensorData->values[1] = s_tMcmag_MiddlewareData.nMag_Y;
                     _ptMagSensorData->values[2] = s_tMcmag_MiddlewareData.nMag_Z;
                     _nStatus                    = s_tMcmag_MiddlewareData.nMag_Status;
                 read_unlock(&s_tMcmag_MiddlewareData.tDataLock); 
    
                 #ifdef _MCMAG_DRV_AMI_COMPATIBLE_MODE_
                     _ptMagSensorData->values[0]    = (_ptMagSensorData->values[0] * MCMAG_CONVERT_M);
                     _ptMagSensorData->values[1]    = (_ptMagSensorData->values[1] * MCMAG_CONVERT_M);
                     _ptMagSensorData->values[2]    = (_ptMagSensorData->values[2] * MCMAG_CONVERT_M);
                     _ptMagSensorData->value_divide = 1000;

                     _operate_accuracy_status_transformer(_nStatus, _ptMagSensorData);
                 #else
                     _ptMagSensorData->value_divide = 1;
                     _ptMagSensorData->status       = _nStatus;
                 #endif    // END of _MCMAG_DRV_AMI_COMPATIBLE_MODE_
             }

             break;    // END of SENSOR_GET_DATA

        //================================================================    
         default:

             MSE_ERR("ERR: 0x%4x CMD not supported!", dwCommand);
             return (-1);                                     \

             break;
    }

    return (0);
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
                 int    _nSampleDelay;

                 _OPERATE_CHK_BUFF_AND_SIZE(pInBuf, nInBufSize, int);

                 _nSampleDelay = *((int *) pInBuf);

                 if (_nSampleDelay < MCMAG_DRV_DEFAULT_SAMPLE_DELAY)
                     _nSampleDelay = MCMAG_DRV_DEFAULT_SAMPLE_DELAY;

                 s_tMcmag_MiddlewareData.naMidControlData[0] = _nSampleDelay;

                 MSE_LOG("_nSampleDelay: %d", _nSampleDelay);
             }
             break;

             break;

        //================================================================    
        case SENSOR_ENABLE:
    
             _OPERATE_CHK_BUFF_AND_SIZE(pInBuf, nInBufSize, int);
             _operate_sensor_enable(SENSOR_ORIENTATION, *(int *)pInBuf);

             break;

        //================================================================    
        case SENSOR_GET_DATA:
             {
                 int                _nStatus                 = 0;
                 hwm_sensor_data   *_ptOrientationSensorData = NULL;

                 _OPERATE_CHK_BUFF_AND_SIZE(pOutBuf, nOutBufSize, hwm_sensor_data);

                 _ptOrientationSensorData = ((hwm_sensor_data *) pOutBuf);
    
                 read_lock(&s_tMcmag_MiddlewareData.tDataLock);
                     _ptOrientationSensorData->values[0] = s_tMcmag_MiddlewareData.nOrientation_Heading;
                     _ptOrientationSensorData->values[1] = s_tMcmag_MiddlewareData.nOrientation_Pitch;
                     _ptOrientationSensorData->values[2] = s_tMcmag_MiddlewareData.nOrientation_Roll;
                     _nStatus                            = s_tMcmag_MiddlewareData.nMag_Status;
                 read_unlock(&s_tMcmag_MiddlewareData.tDataLock);

                 #ifdef _MCMAG_DRV_AMI_COMPATIBLE_MODE_
                      _ptOrientationSensorData->value_divide = MCMAG_ORIENTATION_ACCURACY_RATE;				
                      _operate_accuracy_status_transformer(_nStatus, _ptOrientationSensorData);
                  #else
                      _ptOrientationSensorData->value_divide = 1;
                      _ptOrientationSensorData->status       = _nStatus;
                  #endif
             }
    
             break;    // END of SENSOR_GET_DATA

        //================================================================    
         default:

             MSE_ERR("ERR: 0x%4x CMD not supported!", dwCommand);
             return (-1);                                     \

             break;
    }

    return (0);
}

/*****************************************
 *** MCMAG_power (MTK Reference Code)
 *****************************************/
static void    MCMAG_power(struct mag_hw *ptHW, unsigned int on) 
{
    static unsigned int power_on = 0;

    if(ptHW->power_id != MT65XX_POWER_NONE)    // have externel LDO
    {        
        MSE_LOG("power %s\n", on ? "on" : "off");

        if(power_on == on)    // power status not change
        {
            MSE_LOG("ignore power control: %d\n", on);
        }
        else if(on)    // power on
        {
            if(!hwPowerOn(ptHW->power_id, ptHW->power_vol, MCMAG_DRV_DEVICE_NAME))
            {
                MSE_ERR("power on fails!!\n");
            }
        }
        else    // power off
        {
            if (!hwPowerDown(ptHW->power_id, MCMAG_DRV_DEVICE_NAME))
            {
                MSE_ERR("power off fail!!\n");
            }			  
        }
    }

    power_on = on;    
}

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
    {   
        MCMAG_Stop(&s_tMcmag_MiddlewareData.tControl);
        MCMAG_power(_pt_i2c_data->ptHW, 0);
    }

    return (0);
}
/*----------------------------------------------------------------------------*/
static int mcmag_resume(struct i2c_client *ptClient)
{
    struct _mcmag_i2c_data *_pt_i2c_data = i2c_get_clientdata(ptClient)
    
    MSE_FUN();

    MCMAG_power(_pt_i2c_data->ptHW, 1);
	MCMAG_InitChipset(E_MCMAG_STATE_INVALID);

    return (0);
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

    MCMAG_Stop(&s_tMcmag_MiddlewareData.tControl);
    MCMAG_power(_pt_i2c_data->ptHW, 0);
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

	MCMAG_power(_pt_i2c_data->ptHW, 1);
	MCMAG_InitChipset(E_MCMAG_STATE_INVALID);
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
    
        return (0);
    }
#endif

/*****************************************
 *** mcmag_init_hardware
 *****************************************/
#if (MCMAG_DRV_SYSCFG_KERNEL_VERSION == MCMAG_DRV_SYSCFG_LINUX_KERNEL_2_6_X)
    static void    mcmag_init_hardware(void)
    {
        #if defined (K8_DH_P110)
            mt_set_gpio_mode(GPIO_GSE_1_EINT_PIN, GPIO_MODE_01);
            mt_set_gpio_dir (GPIO_GSE_1_EINT_PIN, GPIO_DIR_OUT);
            mt_set_gpio_out (GPIO_GSE_1_EINT_PIN, GPIO_OUT_ONE);
        #endif	
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

    #if (MCMAG_DRV_SYSCFG_KERNEL_VERSION == MCMAG_DRV_SYSCFG_LINUX_KERNEL_2_6_X)
        mcmag_init_hardware();
    #endif

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
    
    if ((err = MCMAG_InitChipset(E_MCMAG_STATE_INVALID)))
        goto exit_init_failed;

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
        goto exit_kfree;
    }
    
    #if CONFIG_HAS_EARLYSUSPEND
        data->tDrv_EarlySuspend.level   = (EARLY_SUSPEND_LEVEL_DISABLE_FB - 1),
        data->tDrv_EarlySuspend.suspend = mcmag_early_suspend,
        data->tDrv_EarlySuspend.resume  = mcmag_late_resume,    
        register_early_suspend(&data->tDrv_EarlySuspend);
    #endif
    
    MSE_LOG("%s: OK\n", __func__);
    return (0);
    
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

    return (0);
}

/*****************************************
 *** mcmag_probe
 *****************************************/
static int    mcmag_probe(struct platform_device *pPlatformDevice) 
{
    struct mag_hw   *_ptHW = get_cust_mag_hw();
    
    MSE_FUN();    

    MCMAG_power(_ptHW, 1);

    rwlock_init(&s_tMcmag_MiddlewareData.tControlLock);
    rwlock_init(&s_tMcmag_MiddlewareData.tDataLock);

    s_tMcmag_MiddlewareData.naMidControlData[0] = 20;  // sample delay
    s_tMcmag_MiddlewareData.naMidControlData[1] =  0;  // run / stop daemon (HAL, middleware)
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
        return (-1);
    } 

    return (0);
}

/*****************************************
 *** mcmag_remove
 *****************************************/
static int    mcmag_remove(struct platform_device *pPlatformDevice)
{
    struct mag_hw   *_ptHW = get_cust_mag_hw();
    
    MSE_FUN();    

    MCMAG_power(_ptHW, 0);
    atomic_set(&s_tMcmag_DeviceOpenCount, 0);  
    i2c_del_driver(&s_tMcmag_i2c_driver);

    return (0);
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
                                                                           //.owner = THIS_MODULE,
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

    return (0);    
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
MODULE_AUTHOR("Louis Li");
MODULE_DESCRIPTION("MC6420 M-Sensor Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(MCMAG_DRV_DRIVER_VERSION);
