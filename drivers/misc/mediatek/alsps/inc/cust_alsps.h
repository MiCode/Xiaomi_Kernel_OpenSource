/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef __CUST_ALSPS_H__
#define __CUST_ALSPS_H__

#include <linux/types.h>

#define C_CUST_ALS_LEVEL    16
#define C_CUST_I2C_ADDR_NUM 4

#define MAX_THRESHOLD_HIGH 0xffff
#define MIN_THRESHOLD_LOW 0x0

struct alsps_hw {
	int i2c_num;                                    /*!< the i2c bus used by ALS/PS */
	int power_id;                                   /*!< the VDD power id of the als chip */
	int power_vol;                                  /*!< the VDD power voltage of the als chip */
	int polling_mode;                               /*!< 1: polling mode ; 0:interrupt mode*/
	int polling_mode_ps;                               /*!< 1: polling mode ; 0:interrupt mode*/
	int polling_mode_als;                               /*!< 1: polling mode ; 0:interrupt mode*/
	unsigned char   i2c_addr[C_CUST_I2C_ADDR_NUM];/*!< i2c address list, some chip will have multiple address */
	/*!< (C_CUST_ALS_LEVEL-1) levels divides all range into C_CUST_ALS_LEVEL levels*/
	unsigned int    als_level[C_CUST_ALS_LEVEL-1];
	unsigned int    als_value[C_CUST_ALS_LEVEL];    /*!< the value reported in each level */
	unsigned int    ps_threshold;                   /*!< the threshold of proximity sensor */
	unsigned int    als_window_loss;                /*!< the window loss  */
	unsigned int    ps_threshold_high;
	unsigned int    ps_threshold_low;
	unsigned int    als_threshold_high;
	unsigned int    als_threshold_low;
	int als_power_vio_id;                                   /*!< the VIO power id of the als chip */
	int als_power_vio_vol;                                  /*!< the VIO power voltage of the als chip */
	int ps_power_vdd_id;                                   /*!< the VDD power id of the ps chip */
	int ps_power_vdd_vol;                                  /*!< the VDD power voltage of the ps chip */
	int ps_power_vio_id;                                   /*!< the VIO power id of the ps chip */
	int ps_power_vio_vol;                                  /*!< the VIO power voltage of the ps chip */
	int power_lp_mode_ctrl;         /*!< 1: disable ldo low power mode when p sensor enabled ; 0: no action*/
	bool is_batch_supported_ps;
	bool is_batch_supported_als;
};

struct alsps_hw *get_alsps_dts_func(const char *, struct alsps_hw*);

#endif
