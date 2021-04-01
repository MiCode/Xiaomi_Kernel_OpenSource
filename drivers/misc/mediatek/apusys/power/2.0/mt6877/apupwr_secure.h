/*
 * Copyright (C) 2021 MediaTek Inc.
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

#ifndef __APUPWR_SECURE_H__
#define __APUPWR_SECURE_H__

#include <mt-plat/mtk_secure_api.h>

#define APUPWR_SECURE

enum MTK_APUPWR_SMC_OP {
	MTK_APUPWR_SMC_OP_ACC_INIT = 0,
	MTK_APUPWR_SMC_OP_ACC_TOGGLE,
	MTK_APUPWR_SMC_OP_ACC_SET_PARENT,
	MTK_APUPWR_SMC_OP_PLL_SET_RATE,
	MTK_APUPWR_SMC_OP_FMETER_CTL,
	MTK_APUPWR_SMC_OP_NUM
};

#endif
