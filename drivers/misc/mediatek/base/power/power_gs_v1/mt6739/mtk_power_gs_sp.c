/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

#include "mtk_power_gs.h"

void mt_power_gs_sp_dump(void)
{
	mt_power_gs_pmic_manual_dump();
#if 0
	pr_info("0x1023028C = 0x%08x\n", _golden_read_reg(0x1023028C));
#endif
}
