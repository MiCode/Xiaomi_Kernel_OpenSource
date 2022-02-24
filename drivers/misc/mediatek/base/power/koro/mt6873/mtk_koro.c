// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/**
 * @file	mkt_koro.c
 * @brief   Driver for koro
 *
 */

#define __MTK_KORO_C__

/*
 *=============================================================
 * Include files
 *=============================================================
 */

/* system includes */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#ifdef __KERNEL__
	#include <linux/topology.h>
	#include <mt-plat/mtk_chip.h>
	#include "mtk_koro.h"
	#include <mt-plat/mtk_devinfo.h>
#endif

#ifdef CONFIG_OF
	#include <linux/cpu.h>
	#include <linux/cpu_pm.h>
	#include <linux/of.h>
	#include <linux/of_irq.h>
	#include <linux/of_address.h>
	#include <linux/of_fdt.h>
	#include <mt-plat/aee.h>
#endif

/************************************************
 * Marco definition
 ************************************************/

/************************************************
 * LOG
 ************************************************/
#define KORO_TAG	 "[xxxx_koro] "

#define koro_info(fmt, args...)		pr_info(KORO_TAG fmt, ##args)
#define koro_debug(fmt, args...)	\
	pr_debug(KORO_TAG"(%d)" fmt, __LINE__, ##args)

void mtk_koro_disable(void)
{
	struct arm_smccc_res result;

	arm_smccc_smc(MTK_SIP_MCUSYS_CPU_CONTROL,
		1, 0, 0, 0, 0, 0, 0, &result);
}

static int __init koro_init(void)
{
	koro_debug("koro initialization\n");
	/* mtk_koro_disable(); */
	return 0;
}

static void __exit koro_exit(void)
{
	koro_debug("koro de-initialization\n");
}


late_initcall(koro_init);

MODULE_DESCRIPTION("MediaTek KORO Driver v0.1");
MODULE_LICENSE("GPL");

#undef __MTK_KORO_C__
