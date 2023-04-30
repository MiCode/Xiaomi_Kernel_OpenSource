/*
 * Copyright (c) Xiaomi Technologies Co., Ltd. 2019. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * File name: millet.c
 * Description: smart frozen control
 * Author: guchao1@xiaomi.com
 * Version: 1.0
 * Date:  2019/11/27
 */

#define pr_fmt(fmt) "millet_millet-core: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/freezer.h>
#include <net/sock.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/proc_fs.h>


static int __init millet_init(void)
{

	pr_err("enter millet_init func!\n");

	return 0;
}

late_initcall(millet_init);

MODULE_LICENSE("GPL");
