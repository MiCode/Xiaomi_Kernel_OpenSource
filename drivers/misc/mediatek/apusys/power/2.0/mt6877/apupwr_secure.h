// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUPWR_SECURE_H__
#define __APUPWR_SECURE_H__

#include <mt-plat/mtk_secure_api.h>
#include <linux/arm-smccc.h>

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
