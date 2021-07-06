/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef MTK_PHY_H
#define MTK_PHY_H

#include "mu3d_hal_comm.h"
#include "mtk-phy.h"

#undef EXTERN

#define ENTER_U0_TH				10
#define MAX_PHASE_RANGE		31
#define MAX_TIMEOUT_COUNT		100

#ifdef _MTK_PHY_EXT_
#define EXTERN
#else
#define EXTERN \
extern
#endif

#define U3_PHY_I2C_PCLK_DRV_REG	    0x0A
#define U3_PHY_I2C_PCLK_PHASE_REG	0x0B

EXTERN int mu3d_hal_phy_scan(int latch_val, DEV_UINT8 driving);
EXTERN PHY_INT32 _U3Read_Reg(PHY_INT32 address);
EXTERN PHY_INT32 _U3Write_Reg(PHY_INT32 address, PHY_INT32 value);

#undef EXTERN

#endif
