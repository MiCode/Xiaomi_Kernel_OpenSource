/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

/**
 * @file	mkt_ptp3_main.c
 * @brief   Driver for ptp3
 *
 */

#define __MTK_PTP3_C__

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
#include <linux/mutex.h>
#include <linux/arm-smccc.h>
#include <linux/io.h>
#include <linux/mod_devicetable.h>

static int ptp3_probe(struct platform_device *pdev)
{
	return 0;
}

static int ptp3_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int ptp3_resume(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_ptp3_of_match[] = {
	{ .compatible = "mediatek,ptp3", },
	{},
};
#endif

static struct platform_driver ptp3_driver = {
	.remove		= NULL,
	.shutdown	= NULL,
	.probe		= ptp3_probe,
	.suspend	= ptp3_suspend,
	.resume		= ptp3_resume,
	.driver		= {
		.name   = "mt-ptp3",
#ifdef CONFIG_OF
		.of_match_table = mt_ptp3_of_match,
#endif
	},
};

static int __init __ptp3_init(void)
{
	int err = 0;

	err = platform_driver_register(&ptp3_driver);
	if (err)
		return err;

	return 0;
}

static void __exit __ptp3_exit(void)
{
	/* TBD */
}

module_init(__ptp3_init);
module_exit(__ptp3_exit);

MODULE_DESCRIPTION("MediaTek PTP3 Driver v2");
MODULE_LICENSE("GPL");

#undef __MTK_PTP3_C__
