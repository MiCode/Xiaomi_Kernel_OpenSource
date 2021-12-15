// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

/*
 * Definitions for CM36558 als/ps sensor chip.
 */
#ifndef __CM36558_H__
#define __CM36558_H__

#include <linux/ioctl.h>

/*CM36558 als/ps sensor register related macro*/
#define CM36558_REG_ALS_UV_CONF 0X00
#define CM36558_REG_ALS_THDH 0X01
#define CM36558_REG_ALS_THDL 0X02
#define CM36558_REG_PS_CONF1_2 0X03
#define CM36558_REG_PS_CONF3_MS 0X04
#define CM36558_REG_PS_CANC 0X05
#define CM36558_REG_PS_THDL 0X06
#define CM36558_REG_PS_THDH 0X07
#define CM36558_REG_PS_DATA 0X08
#define CM36558_REG_ALS_DATA 0X09
#define CM36558_REG_UVAS_DATA 0X0B
#define CM36558_REG_UVBS_DATA 0X0C
#define CM36558_REG_INT_FLAG 0X0D
#define CM36558_REG_ID 0X0E

/*CM36558 related driver tag macro*/
#define CM36558_SUCCESS 0
#define CM36558_ERR_I2C -1
#define CM36558_ERR_STATUS -3
#define CM36558_ERR_SETUP_FAILURE -4
#define CM36558_ERR_GETGSENSORDATA -5
#define CM36558_ERR_IDENTIFICATION -6

extern struct platform_device *get_alsps_platformdev(void);
#endif
