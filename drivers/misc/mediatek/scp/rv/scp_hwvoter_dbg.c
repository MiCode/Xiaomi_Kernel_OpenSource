// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/regmap.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/suspend.h>
#include <linux/uaccess.h>
#include <linux/bits.h>
#include <linux/build_bug.h>
#include <clk-fmeter.h>

#if IS_ENABLED(CONFIG_OF)
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#endif

#include "scp_ipi.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp.h"
#include "scp_hwvoter_dbg.h"


#if IS_ENABLED(CONFIG_PROC_FS)
/**********************************
 * hw_voter debug
 ***********************************/
int scp_hw_voter_create_procfs(void)
{
	return 0;
}
#endif /* CONFIG_PROC_FS */

int scp_hw_voter_dbg_init(void)
{
	return 0;
}
