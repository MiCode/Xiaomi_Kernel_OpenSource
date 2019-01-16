/* 
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
 * Definitions for APDS9930 als/ps sensor chip.
 */
#ifndef __APDS9930_H__
#define __APDS9930_H__

#include <linux/ioctl.h>

extern int APDS9930_CMM_PPCOUNT_VALUE;
extern int APDS9930_CMM_CONTROL_VALUE;
extern int ZOOM_TIME;

#define APDS9930_CMM_ENABLE 		0X80
#define APDS9930_CMM_ATIME 		0X81
#define APDS9930_CMM_PTIME 		0X82
#define APDS9930_CMM_WTIME 		0X83
/*for interrup work mode support -- by liaoxl.lenovo 12.08.2011*/
#define APDS9930_CMM_INT_LOW_THD_LOW   0X88
#define APDS9930_CMM_INT_LOW_THD_HIGH  0X89
#define APDS9930_CMM_INT_HIGH_THD_LOW  0X8A
#define APDS9930_CMM_INT_HIGH_THD_HIGH 0X8B
#define APDS9930_CMM_Persistence       0X8C
#define APDS9930_CMM_STATUS            0X93
#define TAOS_TRITON_CMD_REG           0X80
#define TAOS_TRITON_CMD_SPL_FN        0x60

#define APDS9930_CMM_CONFIG 		0X8D
#define APDS9930_CMM_PPCOUNT 		0X8E
#define APDS9930_CMM_CONTROL 		0X8F

#define APDS9930_CMM_PDATA_L 		0X98
#define APDS9930_CMM_PDATA_H 		0X99
#define APDS9930_CMM_C0DATA_L 	0X94
#define APDS9930_CMM_C0DATA_H 	0X95
#define APDS9930_CMM_C1DATA_L 	0X96
#define APDS9930_CMM_C1DATA_H 	0X97


#define APDS9930_SUCCESS						0
#define APDS9930_ERR_I2C						-1
#define APDS9930_ERR_STATUS					-3
#define APDS9930_ERR_SETUP_FAILURE				-4
#define APDS9930_ERR_GETGSENSORDATA			-5
#define APDS9930_ERR_IDENTIFICATION			-6


#endif

