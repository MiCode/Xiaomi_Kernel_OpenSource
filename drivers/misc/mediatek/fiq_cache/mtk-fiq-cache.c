/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <mt-plat/mtk_secure_api.h>

unsigned long *aee_rr_rec_fiq_cache_step_pa(void);

#ifdef CONFIG_MACH_MT6763
DEFINE_SPINLOCK(cache_lock);
#else
static DEFINE_MUTEX(cache_mutex);
#endif

void mt_fiq_cache_flush_all(void)
{
#ifdef CONFIG_MACH_MT6763
	unsigned long flags;

	trace_printk("[FIQ_CACHE] starts\n");
	spin_lock_irqsave(&cache_lock, flags);
	mt_secure_call(MTK_SIP_KERNEL_CACHE_FLUSH_FIQ, 0, 0, 0);
	spin_unlock_irqrestore(&cache_lock, flags);
	trace_printk("[FIQ_CACHE] done\n");
#else
	mutex_lock(&cache_mutex);
	trace_printk("[FIQ_CACHE] starts\n");
	mt_secure_call(MTK_SIP_KERNEL_CACHE_FLUSH_FIQ, 0, 0, 0);
	trace_printk("[FIQ_CACHE] done\n");
	mutex_unlock(&cache_mutex);
#endif
}

static int __init fiq_cache_init(void)
{

	mt_secure_call(MTK_SIP_KERNEL_CACHE_FLUSH_INIT,
		(unsigned long)aee_rr_rec_fiq_cache_step_pa(), 0, 0);

	return 0;
}
module_init(fiq_cache_init);
