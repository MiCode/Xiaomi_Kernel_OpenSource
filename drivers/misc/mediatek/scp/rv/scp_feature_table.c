// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include "scp_feature_define.h"
#include "scp_ipi_pin.h"
#include "scp.h"

/*scp feature list*/
struct scp_feature_tb feature_table[NUM_FEATURE_ID] = {
/* VFFP:20 + default:5 */
	{
		.feature	= VOW_FEATURE_ID,
	},
	{
		.feature	= SENS_FEATURE_ID,
	},
	{
		.feature	= FLP_FEATURE_ID,
	},
	{
		.feature	= RTOS_FEATURE_ID,
	},
	{
		.feature	= SPEAKER_PROTECT_FEATURE_ID,
	},
	{
		.feature	= VCORE_TEST_FEATURE_ID,
	},
	{
		.feature	= VOW_BARGEIN_FEATURE_ID,
	},
	{
		.feature	= VOW_DUMP_FEATURE_ID,
	},
	{
		.feature        = VOW_VENDOR_M_FEATURE_ID,
	},
	{
		.feature        = VOW_VENDOR_A_FEATURE_ID,
	},
	{
		.feature        = VOW_VENDOR_G_FEATURE_ID,
	},
	{
		.feature        = VOW_DUAL_MIC_FEATURE_ID,
	},
	{
		.feature        = VOW_DUAL_MIC_BARGE_IN_FEATURE_ID,
	},
	{
		.feature        = ULTRA_FEATURE_ID,
	},
	{
		.feature        = RVSPKPROCESS_FEATURE_ID,
	},
	{
		.feature        = RVVOICE_CALL_FEATURE_ID,
	},
};

