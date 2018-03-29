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

#ifndef __CUST_ACC_H__
#define __CUST_ACC_H__

#define G_CUST_I2C_ADDR_NUM 2

struct sensorHub_hw {
	int is_batch_enabled;
};

extern struct sensorHub_hw *get_cust_sensorHub_hw(void);
#endif
