/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

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
	pr_info("fw jobs execute started count = %d\n", reg_val);
	reg_val = REGR(npu_dev, REG_FW_JOB_CNT_END);
	pr_info("fw jobs execute finished count = %d\n", reg_val);
	reg_val = REGR(npu_dev, REG_NPU_FW_DEBUG_DATA);
	pr_info("fw jobs aco parser debug = %d\n", reg_val);
}
