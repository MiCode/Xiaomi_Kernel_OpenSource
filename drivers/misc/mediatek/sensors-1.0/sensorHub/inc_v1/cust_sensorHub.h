/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CUST_ACC_H__
#define __CUST_ACC_H__

#define G_CUST_I2C_ADDR_NUM 2

struct sensorHub_hw {
	int is_batch_enabled;
};

extern struct sensorHub_hw *get_cust_sensorHub_hw(void);
#endif
