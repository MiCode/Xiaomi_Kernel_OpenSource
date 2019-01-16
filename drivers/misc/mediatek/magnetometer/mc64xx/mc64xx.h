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

 #ifndef MC64XX_H
    #define MC64XX_H

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
#define MCMAG_I2C_ADDR    (0x0C << 1)

/**************************************************************
 *** REG MAP (refer to MC64xx Spec.)
 **************************************************************/
#define MCMAG_REG_STB             0x0C
//================================
#define MCMAG_REG_MORE_INFO       0x0D
#define MCMAG_REG_INFO_VERSION    0x0D
#define MCMAG_REG_INFO_ALPS       0x0E
#define MCMAG_REG_WHO_I_AM        0x0F
//================================
#define MCMAG_REG_XOUT            0x10
#define MCMAG_REG_XOUT_L          0x10
#define MCMAG_REG_XOUT_H          0x11
#define MCMAG_REG_YOUT            0x12
#define MCMAG_REG_YOUT_L          0x12
#define MCMAG_REG_YOUT_H          0x13
#define MCMAG_REG_ZOUT            0x14
#define MCMAG_REG_ZOUT_L          0x14
#define MCMAG_REG_ZOUT_H          0x15
//================================
#define MCMAG_REG_STATUS          0x18
//================================
#define MCMAG_REG_CTRL1           0x1B
#define MCMAG_REG_CTRL2           0x1C
#define MCMAG_REG_CTRL3           0x1D
#define MCMAG_REG_CTRL4           0x1E
//================================
#define MCMAG_REG_XOFF            0x20
#define MCMAG_REG_XOFF_L          0x20
#define MCMAG_REG_XOFF_H          0x21
#define MCMAG_REG_YOFF            0x22
#define MCMAG_REG_YOFF_L          0x22
#define MCMAG_REG_YOFF_H          0x23
#define MCMAG_REG_ZOFF            0x24
#define MCMAG_REG_ZOFF_L          0x24
#define MCMAG_REG_ZOFF_H          0x25
//================================
#define MCMAG_REG_ITHR            0x26
#define MCMAG_REG_ITHR_L          0x26
#define MCMAG_REG_ITHR_H          0x27
//================================
#define MCMAG_REG_TEMP            0x31


/**************************************************************
 *** [REG STATUS: 0x18]
 **************************************************************/
    /**********************************************************
     *** DATA READY DETECTION
     **********************************************************/
    #define MCMAG_STATUS_DATA_READY_NOT_DETECTED    0x00
    #define MCMAG_STATUS_DATA_READY_DETECTED        0x40

    /**********************************************************
     *** DATA OVERRUN DETECTION
     **********************************************************/
    #define MCMAG_STATUS_DATA_OVERRUN_NOT_DETECTED    0x00
    #define MCMAG_STATUS_DATA_OVERRUN_DETECTED        0x20

/**************************************************************
 *** [REG CONTROL1: 0x1B]
 **************************************************************/
    /**********************************************************
     *** POWER MODE CONFIGURATION
     **********************************************************/
    #define MCMAG_CTRL1_POWER_MODE_STANDBY    0x00
    #define MCMAG_CTRL1_POWER_MODE_ACTIVE     0x80

    /**********************************************************
     *** OUTPUT DATA RATE CONFIGURATION
     **********************************************************/
    #define MCMAG_CTRL1_DAA_RATE_0p5Hz    0x00
    #define MCMAG_CTRL1_DAA_RATE_10Hz     0x08
    #define MCMAG_CTRL1_DAA_RATE_20Hz     0x10
    #define MCMAG_CTRL1_DAA_RATE_100Hz    0x18

    /**********************************************************
     *** STATE CONFIGURATION
     **********************************************************/
    #define MCMAG_CTRL1_STATE_NORMAL    0x00
    #define MCMAG_CTRL1_STATE_FORCE     0x02

/**************************************************************
 *** [REG CONTROL3: 0x1D]
 **************************************************************/
    #define MCMAG_CTRL3_ENABLE_SOFT_RESET    0x80

    #define MCMAG_CTRL3_SET_FORCE_STATE    0x40

/**************************************************************
 *** [REG CONTROL4: 0x1E]
 **************************************************************/
    #define MCMAG_CTRL4_MUST_DEFAULT_SETTING    0x80

    /**********************************************************
     *** DYNAMIC RANGE CONFIGURATION
     **********************************************************/
    #define MCMAG_CTRL4_DYNAMIC_RANGE_14bit    0x00
    #define MCMAG_CTRL4_DYNAMIC_RANGE_15bit    0x10

/*******************************************************************************
 *** S/W CONFIGURATION
 *******************************************************************************/
/**************************************************************
 *** BUFFER SIZE
 **************************************************************/
#define MCMAG_BUFFER_SIZE    64

/*******************************************************************************
 *** DATA TYPE / ENUM
 *******************************************************************************/
typedef enum
{
    E_MC64XX_DYNAMIC_RANGE_14bit = 0,
    E_MC64XX_DYNAMIC_RANGE_15bit,
    E_MC64XX_DYNAMIC_RANGE_DUMMY_END = 0xFFFFFFFF
}   E_MC64XX_DynamicRangeConfig;

#endif  //END of MC64XX_H

