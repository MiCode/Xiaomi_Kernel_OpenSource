/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef __LCM_I2C_H__
#define __LCM_I2C_H__

#include "lcm_drv.h"
#include "lcm_common.h"


#if defined(MTK_LCM_DEVICE_TREE_SUPPORT)
enum LCM_STATUS lcm_i2c_set_data(char type, const struct LCM_DATA_T2 *t2);
#endif
extern struct i2c_client *_lcm_i2c_client;
#endif

