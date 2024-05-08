/*
 * Copyright (c) 2020, Xiaomi, Inc. All rights reserved.
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/prefetch.h>
#include <linux/notifier.h>
#include <linux/mfd/core.h>
#include <linux/clk.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include "ispv4_notify.h"

static RAW_NOTIFIER_HEAD(ispv4_notify_chain);

__maybe_unused int ispv4_register_notifier(void *data)
{
	struct notifier_block *nb = data;
	return raw_notifier_chain_register(&ispv4_notify_chain, nb);
}
EXPORT_SYMBOL_GPL(ispv4_register_notifier);

 __maybe_unused int ispv4_unregister_notifier(void *data)
{
	struct notifier_block *nb = data;
	return raw_notifier_chain_unregister(&ispv4_notify_chain, nb);
}
EXPORT_SYMBOL_GPL(ispv4_unregister_notifier);

 __maybe_unused int ispv4_notifier_call_chain(unsigned int val, void *v)
{
	return raw_notifier_call_chain(&ispv4_notify_chain, val, v);
}
EXPORT_SYMBOL_GPL(ispv4_notifier_call_chain);

//static int __init ispv4_notifier_init(void)
//{
//	pr_info("%s", __func__);
//	return 0;
//}
//
//static void __exit ispv4_notifier_exit(void)
//{
//	pr_info("%s", __func__);
//}
//
//module_init(ispv4_notifier_init);
//module_exit(ispv4_notifier_exit);

MODULE_LICENSE("GPL v2");
