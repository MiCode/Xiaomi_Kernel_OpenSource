/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include "board-dt.h"
#include <asm/mach/map.h>
#include <asm/mach/arch.h>

static const char *qcs403_dt_match[] __initconst = {
	"qcom,qcs403",
	"qcom,qcs404",
	"qcom,qcs405",
	"qcom,qcs407",
	NULL
};

static void __init qcs403_init(void)
{
	board_dt_populate(NULL);
}

DT_MACHINE_START(QCS403_DT,
	"Qualcomm Technologies, Inc. QCS403 (Flattened Device Tree)")
	.init_machine           = qcs403_init,
	.dt_compat              = qcs403_dt_match,
MACHINE_END
