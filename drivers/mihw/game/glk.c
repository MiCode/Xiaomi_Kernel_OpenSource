/*
  * Copyright (c) Xiaomi Technologies Co., Ltd. 2019. All rights reserved.
  * Copyright (C) 2021 XiaoMi, Inc.
  *
  * File name: glk.c
  * Descrviption: Game load tracking
  * Author: guchao1@xiaomi.com
  * Version: 1.0
  * Date:  2019/12/03
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License version 2 as
  * published by the Free Software Foundation.
  */
#define pr_fmt(fmt) "glk: " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/miscdevice.h>
#include <linux/security.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/syscore_ops.h>
#include <linux/irq_work.h>
#include <trace/hooks/sched.h>

static void set_next_freq(void *nouse, unsigned long util, unsigned long freq, unsigned long max,
	unsigned long *next_freq, struct cpufreq_policy *policy, bool *need_freq_update)
{
	return;
}

int game_load_init(void)
{
	register_trace_android_vh_map_util_freq(set_next_freq, NULL);
	pr_err("game load init success\n");
	return 0;
}
EXPORT_SYMBOL_GPL(game_load_init);
//late_initcall(game_load_init);

