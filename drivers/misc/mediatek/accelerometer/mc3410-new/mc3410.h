/*****************************************************************************
 *
 * Copyright (c) 2014 mCube, Inc.  All rights reserved.
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
 * Accelerometer Sensor Driver
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
 *****************************************************************************/

#ifndef _MC3XXX_H_
    #define _MC3XXX_H_

#include <linux/ioctl.h>

/***********************************************
 *** REGISTER MAP
 ***********************************************/
#define MC3XXX_REG_XOUT                    0x00
#define MC3XXX_REG_YOUT                    0x01
#define MC3XXX_REG_ZOUT                    0x02
#define MC3XXX_REG_TILT_STATUS             0x03
#define MC3XXX_REG_SAMPLE_RATE_STATUS      0x04
#define MC3XXX_REG_SLEEP_COUNT             0x05
#define MC3XXX_REG_INTERRUPT_ENABLE        0x06
#define MC3XXX_REG_MODE_FEATURE            0x07
#define MC3XXX_REG_SAMPLE_RATE             0x08
#define MC3XXX_REG_TAP_DETECTION_ENABLE    0x09
#define MC3XXX_REG_TAP_DWELL_REJECT        0x0A
#define MC3XXX_REG_DROP_CONTROL            0x0B
#define MC3XXX_REG_SHAKE_DEBOUNCE          0x0C
#define MC3XXX_REG_XOUT_EX_L               0x0D
#define MC3XXX_REG_XOUT_EX_H               0x0E
#define MC3XXX_REG_YOUT_EX_L               0x0F
#define MC3XXX_REG_YOUT_EX_H               0x10
#define MC3XXX_REG_ZOUT_EX_L               0x11
#define MC3XXX_REG_ZOUT_EX_H               0x12
#define MC3XXX_REG_RANGE_CONTROL           0x20
#define MC3XXX_REG_SHAKE_THRESHOLD         0x2B
#define MC3XXX_REG_UD_Z_TH                 0x2C
#define MC3XXX_REG_UD_X_TH                 0x2D
#define MC3XXX_REG_RL_Z_TH                 0x2E
#define MC3XXX_REG_RL_Y_TH                 0x2F
#define MC3XXX_REG_FB_Z_TH                 0x30
#define MC3XXX_REG_DROP_THRESHOLD          0x31
#define MC3XXX_REG_TAP_THRESHOLD           0x32
#define MC3XXX_REG_PRODUCT_CODE            0x3B

/***********************************************
 *** RETURN CODE
 ***********************************************/
#define MC3XXX_RETCODE_SUCCESS                 (0)
#define MC3XXX_RETCODE_ERROR_I2C               (-1)
#define MC3XXX_RETCODE_ERROR_NULL_POINTER      (-2)
#define MC3XXX_RETCODE_ERROR_STATUS            (-3)
#define MC3XXX_RETCODE_ERROR_SETUP             (-4)
#define MC3XXX_RETCODE_ERROR_GET_DATA          (-5)
#define MC3XXX_RETCODE_ERROR_IDENTIFICATION    (-6)

/***********************************************
 *** CONFIGURATION
 ***********************************************/
#define MC3XXX_BUF_SIZE    256

/***********************************************
 *** PRODUCT ID
 ***********************************************/
#define MC3XXX_PCODE_3210     0x90
#define MC3XXX_PCODE_3230     0x19
#define MC3XXX_PCODE_3250     0x88
#define MC3XXX_PCODE_3410     0xA8
#define MC3XXX_PCODE_3410N    0xB8
#define MC3XXX_PCODE_3430     0x29
#define MC3XXX_PCODE_3430N    0x39
#define MC3XXX_PCODE_3510     0x40
#define MC3XXX_PCODE_3530     0x30
#define MC3XXX_PCODE_3216     0x10
#define MC3XXX_PCODE_3236     0x60

#define MC3XXX_PCODE_RESERVE_1    0x20
#define MC3XXX_PCODE_RESERVE_2    0x11
#define MC3XXX_PCODE_RESERVE_3    0x21
#define MC3XXX_PCODE_RESERVE_4    0x61
#define MC3XXX_PCODE_RESERVE_5    0xA0
#define MC3XXX_PCODE_RESERVE_6    0xE0
#define MC3XXX_PCODE_RESERVE_7    0x91
#define MC3XXX_PCODE_RESERVE_8    0xA1
#define MC3XXX_PCODE_RESERVE_9    0xE1

#define MC3XXX_PCODE_RESERVE_10    0x99

#ifdef CONFIG_CUSTOM_KERNEL_ACCELEROMETER_MODULE
extern bool success_Flag;
#endif

#endif    /* END OF _MC3XXX_H_ */

