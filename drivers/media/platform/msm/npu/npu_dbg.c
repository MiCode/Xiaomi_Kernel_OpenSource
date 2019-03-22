// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

/* -------------------------------------------------------------------------
 * Includes
 * -------------------------------------------------------------------------
 */
#include "npu_common.h"
#include "npu_firmware.h"
#include "npu_hw.h"
#include "npu_hw_access.h"
#include "npu_mgr.h"

/* -------------------------------------------------------------------------
 * Function Definitions - Debug
 * -------------------------------------------------------------------------
 */
void npu_dump_debug_timeout_stats(struct npu_device *npu_dev)
{
	uint32_t reg_val;

	reg_val = REGR(npu_dev, REG_FW_JOB_CNT_START);
	NPU_INFO("fw jobs execute started count = %d\n", reg_val);
	reg_val = REGR(npu_dev, REG_FW_JOB_CNT_END);
	NPU_INFO("fw jobs execute finished count = %d\n", reg_val);
	reg_val = REGR(npu_dev, REG_NPU_FW_DEBUG_DATA);
	NPU_INFO("fw jobs aco parser debug = %d\n", reg_val);
}
