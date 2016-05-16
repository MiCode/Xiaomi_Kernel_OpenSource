/*
 * Copyright (C) 2012 Senodia.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Author: Tori Xu <xuezhi_xu@senodia.com>
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
 * Definitions for senodia compass chip.
 */
#ifndef MAGNETIC_H
#define MAGNETIC_H

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/err.h>

/*
 * ABS(min~max)
 */
#define ABSMIN_MAG	-32767
#define ABSMAX_MAG	32767

/*
 * device id
 */
#define ST480_DEVICE_ID 0x7C
#define IC_CHECK 0

/*
 *
 */
#define ST480_I2C_ADDRESS 0x0c

/*
 * I2C name
 */
#define ST480_I2C_NAME "st480"

/*
 * IC Package size(choose your ic size)
 */
#define ST480MB_SIZE_2X2	 0
#define ST480MW_SIZE_1_6X1_6 0
#define ST480MC_SIZE_1_2X1_2 1

/*
 * IC Position
 */






#define CONFIG_ST480_BOARD_LOCATION_BACK
#define CONFIG_ST480_BOARD_LOCATION_BACK_DEGREE_0




/*
 * register shift
 */
#define ST480_REG_DRR_SHIFT 2

/*
 * BURST MODE(INT)
 */
#define ST480_BURST_MODE 0
#define BURST_MODE_CMD 0x1F
#define BURST_MODE_DATA_LOW 0x01

/*
 * SINGLE MODE
 */
#define ST480_SINGLE_MODE 1
#define SINGLE_MEASUREMENT_MODE_CMD 0x3F

/*
 * register
 */
#define READ_MEASUREMENT_CMD 0x4F
#define WRITE_REGISTER_CMD 0x60
#define READ_REGISTER_CMD 0x50
#define EXIT_REGISTER_CMD 0x80
#define MEMORY_RECALL_CMD 0xD0
#define MEMORY_STORE_CMD 0xE0
#define RESET_CMD 0xF0

#define CALIBRATION_REG (0x02 << ST480_REG_DRR_SHIFT)
#define CALIBRATION_DATA_LOW 0x1C
#define CALIBRATION_DATA_HIGH 0x00

#define ONE_INIT_DATA_LOW 0x7C
#define ONE_INIT_DATA_HIGH 0x00
#define ONE_INIT_REG (0x00 << ST480_REG_DRR_SHIFT)

#define TWO_INIT_DATA_LOW 0x00
#define TWO_INIT_DATA_HIGH 0x00
#define TWO_INIT_REG (0x02 << ST480_REG_DRR_SHIFT)

#define TEMP_DATA_LOW 0x00
#define TEMP_DATA_HIGH 0x00
#define TEMP_REG (0x01 << ST480_REG_DRR_SHIFT)

/*
 * Miscellaneous set.
 */
#define MAX_FAILURE_COUNT 3
#define ST480_DEFAULT_DELAY   40
#define ST480_AUTO_TEST 0
#define OLD_KERNEL_VERSION 0

#if ST480_AUTO_TEST
#include <linux/kthread.h>
#endif

/*
 * Debug
 */
#define SENODIA_DEBUG_MSG	   0
#define SENODIA_DEBUG_FUNC	  0

#if SENODIA_DEBUG_MSG
#define SENODIADBG(format, ...) printk(KERN_INFO "SENODIA " format "\n", ## __VA_ARGS__)
#else
#define SENODIADBG(format, ...)
#endif

#if SENODIA_DEBUG_FUNC
#define SENODIAFUNC(func) printk(KERN_INFO "SENODIA " func " is called\n")
#else
#define SENODIAFUNC(func)
#endif
/*******************************************************************/

#define ST480IO				   0xA1

/* IOCTLs for hal */
#define MSENSOR_IOCTL_ST480_SET_MFLAG   _IOW(ST480IO, 0x10, short)
#define MSENSOR_IOCTL_ST480_GET_MFLAG   _IOR(ST480IO, 0x11, short)
#define MSENSOR_IOCTL_ST480_SET_DELAY   _IOW(ST480IO, 0x12, short)
#define MSENSOR_IOCTL_ST480_GET_DELAY   _IOR(ST480IO, 0x13, short)
#define MSENSOR_IOCTL_ST480_SET_MVFLAG  _IOW(ST480IO, 0x14, short)
#define MSENSOR_IOCTL_ST480_GET_MVFLAG  _IOR(ST480IO, 0x15, short)
#define MSENSOR_IOCTL_ST480_SET_RMFLAG  _IOW(ST480IO, 0x16, short)
#define MSENSOR_IOCTL_ST480_GET_RMFLAG  _IOR(ST480IO, 0x17, short)
#define MSENSOR_IOCTL_ST480_SET_MRVFLAG _IOW(ST480IO, 0x18, short)
#define MSENSOR_IOCTL_ST480_GET_MRVFLAG _IOR(ST480IO, 0x19, short)

#endif

