/* include/linux/mc6420.h - MC6420 compass driver
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

/*
 * Definitions for mc6420 compass chip.
 */
#ifndef MC6420_H
    #define MC6420_H

/*******************************************************************************
 *** INCLUDE FILES
 *******************************************************************************/
#include <linux/ioctl.h>

/*******************************************************************************
 *** H/W CONFIGURATION
 *******************************************************************************/
/**************************
 *** MAG SENSOR I2C ADDR
 **************************/
#define MCMAG_PIN_LEVEL_GND     0x00
#define MCMAG_PIN_LEVEL_DVDD    0x01

#define MCMAG_I2C_PIN_A0_CONNECT_TO    MCMAG_PIN_LEVEL_GND

#if   (MCMAG_I2C_PIN_A0_CONNECT_TO == MCMAG_PIN_LEVEL_GND)
           #define MCMAG_I2C_ADDR    0x3C
#elif (MCMAG_I2C_PIN_A0_CONNECT_TO == MCMAG_PIN_LEVEL_DVDD)
           #define MCMAG_I2C_ADDR    0x38
#else
           #error "MCMAG_I2C_PIN_A0 should be correctly configured..."
#endif

/**************************************************************
 *** REG MAP (refer to MC6420 Spec.)
 **************************************************************/
#define MCMAG_REG_CH1_OUT_LSB           0x00
#define MCMAG_REG_CH1_OUT_MSB           0x01
#define MCMAG_REG_CH2_OUT_LSB           0x02
#define MCMAG_REG_CH2_OUT_MSB           0x03
#define MCMAG_REG_CH3_OUT_LSB           0x04
#define MCMAG_REG_CH3_OUT_MSB           0x05
#define MCMAG_REG_STATUS1_STATUS        0x06
#define MCMAG_REG_STATUS2_INTR          0x07
#define MCMAG_REG_TEMP_LSB              0x08
#define MCMAG_REG_TEMP_MSB              0x09
#define MCMAG_REG_SETUP1_RATE_MEAS      0x0A
#define MCMAG_REG_SETUP2_STATE          0x0B
#define MCMAG_REG_SETUP3_INTRL          0x0C
#define MCMAG_REG_SETUP4_SKIPCNT        0x0D
#define MCMAG_REG_SETUP5_EXOFF1         0x0E
#define MCMAG_REG_SETUP6_EXOFF2         0x0F
#define MCMAG_REG_SETUP7_EXGAIN1        0x10
#define MCMAG_REG_SETUP8_EXGAIN2        0x11
#define MCMAG_REG_SETUP9_THRLMT         0x12
#define MCMAG_REG_ENABLE1               0x13
#define MCMAG_REG_ENABLE2               0x14
#define MCMAG_REG_ENABLE3_INTREN        0x15
#define MCMAG_REG_PCODE                 0x16
#define MCMAG_REG_SCRATCH_PAD           0x17
#define MCMAG_REG_POWER_ON_RESET        0x18
#define MCMAG_REG_SAMPLE_RATE_CONTRL    0x19
#define MCMAG_REG_PW_PRE                0x1A
#define MCMAG_REG_SETPW                 0x1B
#define MCMAG_REG_CHIPID                0x1C
#define MCMAG_REG_SEC                   0x1D
#define MCMAG_REG_SINCIN1               0x1E
#define MCMAG_REG_TEST1                 0x1F
#define MCMAG_REG_TEST2                 0x20
#define MCMAG_REG_INTRLSEL              0x21
#define MCMAG_REG_RNGCH1                0x22
#define MCMAG_REG_RNGCH23               0x23
#define MCMAG_REG_T0_LSB                0x24
#define MCMAG_REG_T0_MSB                0x25
#define MCMAG_REG_SINCIN2               0x26
#define MCMAG_REG_ANA_CH1A              0x27
#define MCMAG_REG_ANA_CH1B              0x28
#define MCMAG_REG_ANA_CH1C              0x29
#define MCMAG_REG_ANA_CH2A              0x2A
#define MCMAG_REG_ANA_CH2B              0x2B
#define MCMAG_REG_ANA_CH2C              0x2C
#define MCMAG_REG_ANA_CH3A              0x2D
#define MCMAG_REG_ANA_CH3B              0x2E
#define MCMAG_REG_ANA_CH3C              0x2F
#define MCMAG_REG_PW_ADC_RST            0x30
#define MCMAG_REG_UNUSED                0x31
#define MCMAG_REG_LFSR_CFIG             0x32
#define MCMAG_REG_LFSR_CNT              0x33
#define MCMAG_REG_LFSR_SEED             0x34
#define MCMAG_REG_OTPCTRL               0x35
#define MCMAG_REG_OTPDIN                0x36
#define MCMAG_REG_OTPDOUT               0x37
#define MCMAG_REG_DEVIDFREQ             0x38
#define MCMAG_REG_BIASCHMP              0x39
#define MCMAG_REG_DGAIN1                0x3A
#define MCMAG_REG_DGAIN2                0x3B
#define MCMAG_REG_DGAIN3                0x3C
#define MCMAG_REG_AOFF0                 0x3D
#define MCMAG_REG_OFFSET01              0x3E
#define MCMAG_REG_OFFSET23              0x3F

/**************************************************************
 *** [REG STATUS1: 0x06]
 **************************************************************/
    #define MCMAG_STATUS1_ACQ_DATA_READY    0x01

/**************************************************************
 *** [REG SETUP1: 0x0A]
 **************************************************************/
    /**********************************************************
     *** MEASUREMENT CONFIGURATION
     **********************************************************/
    #define MCMAG_SETUP1_MEAS_NORMAL               0x00
    #define MCMAG_SETUP1_MEAS_SELFTEST_POSITIVE    0x01
    #define MCMAG_SETUP1_MEAS_SELFTEST_NEGATIVE    0x02

    /**********************************************************
     *** SAMPLE RATE
     **********************************************************/
    #define MCMAG_SAMPLE_RATE_10Hz     0x00
    #define MCMAG_SAMPLE_RATE_30Hz     0x04
    #define MCMAG_SAMPLE_RATE_100Hz    0x08
    #define MCMAG_SAMPLE_RATE_200Hz    0x0C

/**************************************************************
 *** [REG SETUP2: 0x0B]
 **************************************************************/
    /**********************************************************
     *** MAG STATE
     **********************************************************/
    #define MCMAG_SETUP2_STATE_STANDBY       0x00
    #define MCMAG_SETUP2_STATE_CONTINUOUS    0x01
    #define MCMAG_SETUP2_STATE_SINGLE        0x02

/**************************************************************
 *** [REG SETUP6: 0x0F]
 **************************************************************/
    /**********************************************************
     *** RANGE & SENSITIVITY
     **********************************************************/
    #define MCMAG_SETUP6_FIELD_RANGE_1    0x10
    #define MCMAG_SETUP6_FIELD_RANGE_2    0x20
    #define MCMAG_SETUP6_FIELD_RANGE_3    0x30
    #define MCMAG_SETUP6_FIELD_RANGE_4    0x40
    #define MCMAG_SETUP6_FIELD_RANGE_5    0x50

    #define MCMAG_SETUP6_FIELD_RANGE_MASK    0xF0

/**************************************************************
 *** [REG SETUP7: 0x10]
 **************************************************************/
    /**********************************************************
     *** CLOCK RATE
     **********************************************************/
    #define MCMAG_SETUP7_HF_DIGTAL_CLOCK_1MHz    0x00
    #define MCMAG_SETUP7_HF_DIGTAL_CLOCK_8MHz    0x80

/**************************************************************
 *** [REG ENABLE2: 0x14]
 **************************************************************/
    /**********************************************************
     *** FUNCTIONS
     **********************************************************/
    #define MCMAG_ENABLE2_IPP_OPEN_DRAIN     0x00
    #define MCMAG_ENABLE2_IPP_PUSH_PULL      0x20

    #define MCMAG_ENABLE2_IAH_ACTIVE_LOW     0x00
    #define MCMAG_ENABLE2_IAH_ACTIVE_HIGH    0x40

/**************************************************************
 *** CHIP ID
 **************************************************************/
    #define MCUBE_MARS_CHIPID0       0x10
    #define MCUBE_MARS_CHIPID1       0x11
    #define MCUBE_MARS_CHIPID2       0x12
    #define MCUBE_UNPROGRAMMED_ID    0x00

/*******************************************************************************
 *** S/W CONFIGURATION
 *******************************************************************************/
/**************************************************************
 *** BUFFER SIZE
 **************************************************************/
#define MCMAG_BUFFER_SIZE    64

/**************************************************************
 *** MAG DEFUALTS
 **************************************************************/
#define MCMAG_DEFAULT_SETUP7_HF              0x01
#define MCMAG_DEFAULT_SAMPLE_RATE_DIVIDER    0x0A
#define MCMAG_DEFAULT_ENABLE1                0x07
#define MCMAG_DEFAULT_ENABLE2                0x1F
#define MCMAG_DEFAULT_ENABLE3                0x00

/*******************************************************************************
 *** MCUBE TYPE DEFINITION
 *******************************************************************************/
/**************************************************************
 *** MCUBE RETURN CODE (TODO: replace these by system return code)
 **************************************************************/
typedef enum
{
    /* Generic return codes */
    MCUBE_SUCCESS = 0x00000000,
    MCUBE_FAILURE,
    MCUBE_UNKNOWN_SENSOR,
    MCUBE_NULL_SENS_CFG,
    MCUBE_NULL_SENSOR_PTR,
    MCUBE_NULL_DATA_PTR,
    MCUBE_INVALID_SENS_CNT,
    MCUBE_OFFSETS_UNCHANGED,
    /*Accelerometer Specific Error Codes */
    MCUBE_ACCEL_INVALID_CFG = 0x01000000,
    MCUBE_ACCEL_NULL_CFG,
    MCUBE_ACCEL_UNAVAILABLE,
    MCUBE_ACCEL_UNINITIALIZED,
    MCUBE_ACCEL_ALREADY_INIT,
    MCUBE_ACCEL_PWRDOWN_FAIL,
    MCUBE_ACCEL_DATA_OUTPTR_NULL,
    MCUBE_ACCEL_DATA_INPTR_NULL,
    MCUBE_ACCEL_DATA_INVALID,
    MCUBE_ACCEL_ALREADY_STARTED,
    MCUBE_ACCEL_ALREADY_STOPPED,
    MCUBE_ACCEL_UNKNOWN_CHIPID,
    MCUBE_ACCEL_DRV_STOP_FAIL,
    MCUBE_ACCEL_INVALID_OFFSET,
    /* Gyroscope specific error codes */
    MCUBE_GYRO_INVALID_CFG = 0x02000000,
    MCUBE_GYRO_UNAVAILABLE,
    MCUBE_GYRO_UNINITIALIZED,
    /* Magnetometer specific error codes */
    MCUBE_MAG_INVALID_CFG = 0x04000000,
    MCUBE_MAG_NULL_CFG,
    MCUBE_MAG_UNKNOWN_CHIPID,
    MCUBE_MAG_INVALID_CHIP_ID,
    MCUBE_MAG_PWRDOWN_FAIL,
    MCUBE_MAG_DRV_STOP_FAIL,
    MCUBE_MAG_DATA_INPTR_NULL,
    MCUBE_MAG_DATA_NOT_READY,
    MCUBE_MAG_UNAVAILABLE,
    MCUBE_MAG_UNINITIALIZED
}   mCubeResult_t;

/**************************************************************
 *** DATA STRUCTURE & ENUM
 **************************************************************/
typedef enum
{
    E_MCMAG_INIT_STATUS_UNINITIALIZED = 0,
    E_MCMAG_INIT_STATUS_INITIALIZED,
    E_MCMAG_INIT_STATUS_DUMMY_END = 0xFFFFFFFF
}   e_McmagInitStaus;

typedef enum
{
    E_MCMAG_STATE_INVALID,
    E_MCMAG_STATE_WAKE,
    E_MCMAG_STATE_SINGLE,
    E_MCMAG_STATE_STANDBY,
    E_MCMAG_STATE_DUMMY_END = 0xFFFFFFFF
}   e_McmagSetup2State;

typedef enum
{
    E_MCMAG_MEAS_NORMAL,
    E_MCMAG_MEAS_SELFTEST_POSITIVE,
    E_MCMAG_MEAS_SELFTEST_NEGATIVE,
    E_MCMAG_MEAS_DUMMY_END = 0xFFFFFFFF
}   e_McmagMeasCfg;

typedef enum
{
    E_MCMAG_RATE_10Hz,
    E_MCMAG_RATE_25Hz,
    E_MCMAG_RATE_100Hz,
    E_MCMAG_RATE_200Hz,
    E_MCMAG_RATE_DUMMY_END = 0xFFFFFFFF
}   e_McmagRateCfg;

typedef struct
{
    e_McmagRateCfg    eRateCfg;
    bool              bIsHingFreqON;
    u8                bSampleRateDivider;
    u8                baPadding[2];
}   s_McmagSampleRateCfg;

typedef enum
{
    E_MCMAG_FIELD_RANGE_1,
    E_MCMAG_FIELD_RANGE_2,
    E_MCMAG_FIELD_RANGE_3,
    E_MCMAG_FIELD_RANGE_4,
    E_MCMAG_FIELD_RANGE_5,
    E_MCMAG_FIELD_RANGE_DUMMY_END = 0xFFFFFFFF
}   e_McmagRangeCfg;

typedef enum
{
    E_MCMAG_IPP_OPEN_DRAIN,
    E_MCMAG_IPP_PUSH_PULL,
    E_MCMAG_IPP_DUMMY_END = 0xFFFFFFFF
}   e_McmagIntPinCfg;

typedef enum
{
    E_MCMAG_IAH_ACTIVE_LOW,
    E_MCMAG_IAH_ACTIVE_HIGH,
    E_MCMAG_IAH_DUMMY_END = 0xFFFFFFFF
}   e_McmagIntLevelCfg;

typedef struct
{
    e_McmagIntPinCfg      eIntPinCfg;
    e_McmagIntLevelCfg    eIntPinLevel;
    u16                   wIntEnableFlag;
    u16                   wPadding;
}   s_McmagIntCfg;

typedef struct
{
    e_McmagSetup2State      eActiveState;
    e_McmagMeasCfg          eMeasCfg;
    s_McmagSampleRateCfg    tSampleRateCfg;
    e_McmagRangeCfg         eRangeCfg;
    s_McmagIntCfg           tIntCfg;
    u8                      bEnable1Cfg;
    u8                      baPadding[3];
}   s_McmagConfig;

typedef struct
{
    e_McmagInitStaus      eInitStatus;
    e_McmagSetup2State    ePowerState;
    s_McmagConfig         tMagConfig;
    void                 *pDeviceID;
    bool                  bIsChipIdValid;
    u8                    bChipID;
    u8                    baPadding[2];
}   s_McmagMagControl;

#endif  //END of MC6420_H

