/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/mfd/wcd9xxx/core-resource.h>


static enum wcd9xxx_intf_status wcd9xxx_intf = -1;

int wcd9xxx_core_irq_init(
	struct wcd9xxx_core_resource *wcd9xxx_core_res)
{
	int ret = 0;

	if (wcd9xxx_core_res->irq != 1) {
		ret = wcd9xxx_irq_init(wcd9xxx_core_res);
		if (ret)
			pr_err("IRQ initialization failed\n");
	}

	return ret;
}
EXPORT_SYMBOL(wcd9xxx_core_irq_init);

int wcd9xxx_initialize_irq(
	struct wcd9xxx_core_resource *wcd9xxx_core_res,
	unsigned int irq,
	unsigned int irq_base)
{
	wcd9xxx_core_res->irq = irq;
	wcd9xxx_core_res->irq_base = irq_base;

	return 0;
}
EXPORT_SYMBOL(wcd9xxx_initialize_irq);

int wcd9xxx_core_res_init(
	struct wcd9xxx_core_resource *wcd9xxx_core_res,
	int num_irqs, int num_irq_regs,
	int (*codec_read)(struct wcd9xxx_core_resource*, unsigned short),
	int (*codec_write)(struct wcd9xxx_core_resource*, unsigned short, u8),
	int (*codec_bulk_read) (struct wcd9xxx_core_resource*, unsigned short,
							int, u8*))
{
	mutex_init(&wcd9xxx_core_res->pm_lock);
	wcd9xxx_core_res->wlock_holders = 0;
	wcd9xxx_core_res->pm_state = WCD9XXX_PM_SLEEPABLE;
	init_waitqueue_head(&wcd9xxx_core_res->pm_wq);
	pm_qos_add_request(&wcd9xxx_core_res->pm_qos_req,
				PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);

	wcd9xxx_core_res->codec_reg_read = codec_read;
	wcd9xxx_core_res->codec_reg_write = codec_write;
	wcd9xxx_core_res->codec_bulk_read = codec_bulk_read;
	wcd9xxx_core_res->num_irqs = num_irqs;
	wcd9xxx_core_res->num_irq_regs = num_irq_regs;

	pr_info("%s: num_irqs = %d, num_irq_regs = %d\n",
			__func__, wcd9xxx_core_res->num_irqs,
			wcd9xxx_core_res->num_irq_regs);

	return 0;
}
EXPORT_SYMBOL(wcd9xxx_core_res_init);

void wcd9xxx_core_res_deinit(struct wcd9xxx_core_resource *wcd9xxx_core_res)
{
	pm_qos_remove_request(&wcd9xxx_core_res->pm_qos_req);
	mutex_destroy(&wcd9xxx_core_res->pm_lock);
	wcd9xxx_core_res->codec_reg_read = NULL;
	wcd9xxx_core_res->codec_reg_write = NULL;
	wcd9xxx_core_res->codec_bulk_read = NULL;
}
EXPORT_SYMBOL(wcd9xxx_core_res_deinit);

enum wcd9xxx_pm_state wcd9xxx_pm_cmpxchg(
		struct wcd9xxx_core_resource *wcd9xxx_core_res,
		enum wcd9xxx_pm_state o,
		enum wcd9xxx_pm_state n)
{
	enum wcd9xxx_pm_state old;
	mutex_lock(&wcd9xxx_core_res->pm_lock);
	old = wcd9xxx_core_res->pm_state;
	if (old == o)
		wcd9xxx_core_res->pm_state = n;
	mutex_unlock(&wcd9xxx_core_res->pm_lock);
	return old;
}
EXPORT_SYMBOL(wcd9xxx_pm_cmpxchg);

int wcd9xxx_core_res_suspend(
	struct wcd9xxx_core_resource *wcd9xxx_core_res,
	pm_message_t pmesg)
{
	int ret = 0;

	pr_debug("%s: enter\n", __func__);
	/*
	 * pm_qos_update_request() can be called after this suspend chain call
	 * started. thus suspend can be called while lock is being held
	 */
	mutex_lock(&wcd9xxx_core_res->pm_lock);
	if (wcd9xxx_core_res->pm_state == WCD9XXX_PM_SLEEPABLE) {
		pr_debug("%s: suspending system, state %d, wlock %d\n",
			 __func__, wcd9xxx_core_res->pm_state,
			 wcd9xxx_core_res->wlock_holders);
		wcd9xxx_core_res->pm_state = WCD9XXX_PM_ASLEEP;
	} else if (wcd9xxx_core_res->pm_state == WCD9XXX_PM_AWAKE) {
		/*
		 * unlock to wait for pm_state == WCD9XXX_PM_SLEEPABLE
		 * then set to WCD9XXX_PM_ASLEEP
		 */
		pr_debug("%s: waiting to suspend system, state %d, wlock %d\n",
			 __func__, wcd9xxx_core_res->pm_state,
			 wcd9xxx_core_res->wlock_holders);
		mutex_unlock(&wcd9xxx_core_res->pm_lock);
		if (!(wait_event_timeout(wcd9xxx_core_res->pm_wq,
					 wcd9xxx_pm_cmpxchg(wcd9xxx_core_res,
						  WCD9XXX_PM_SLEEPABLE,
						  WCD9XXX_PM_ASLEEP) ==
							WCD9XXX_PM_SLEEPABLE,
					 HZ))) {
			pr_debug("%s: suspend failed state %d, wlock %d\n",
				 __func__, wcd9xxx_core_res->pm_state,
				 wcd9xxx_core_res->wlock_holders);
			ret = -EBUSY;
		} else {
			pr_debug("%s: done, state %d, wlock %d\n", __func__,
				 wcd9xxx_core_res->pm_state,
				 wcd9xxx_core_res->wlock_holders);
		}
		mutex_lock(&wcd9xxx_core_res->pm_lock);
	} else if (wcd9xxx_core_res->pm_state == WCD9XXX_PM_ASLEEP) {
		pr_warn("%s: system is already suspended, state %d, wlock %dn",
			__func__, wcd9xxx_core_res->pm_state,
			wcd9xxx_core_res->wlock_holders);
	}
	mutex_unlock(&wcd9xxx_core_res->pm_lock);

	return ret;
}
EXPORT_SYMBOL(wcd9xxx_core_res_suspend);

int wcd9xxx_core_res_resume(
	struct wcd9xxx_core_resource *wcd9xxx_core_res)
{
	int ret = 0;

	pr_debug("%s: enter\n", __func__);
	mutex_lock(&wcd9xxx_core_res->pm_lock);
	if (wcd9xxx_core_res->pm_state == WCD9XXX_PM_ASLEEP) {
		pr_debug("%s: resuming system, state %d, wlock %d\n", __func__,
				wcd9xxx_core_res->pm_state,
				wcd9xxx_core_res->wlock_holders);
		wcd9xxx_core_res->pm_state = WCD9XXX_PM_SLEEPABLE;
	} else {
		pr_warn("%s: system is already awake, state %d wlock %d\n",
				__func__, wcd9xxx_core_res->pm_state,
				wcd9xxx_core_res->wlock_holders);
	}
	mutex_unlock(&wcd9xxx_core_res->pm_lock);
	wake_up_all(&wcd9xxx_core_res->pm_wq);

	return ret;
}
EXPORT_SYMBOL(wcd9xxx_core_res_resume);

enum wcd9xxx_intf_status wcd9xxx_get_intf_type(void)
{
	return wcd9xxx_intf;
}
EXPORT_SYMBOL(wcd9xxx_get_intf_type);

void wcd9xxx_set_intf_type(enum wcd9xxx_intf_status intf_status)
{
	wcd9xxx_intf = intf_status;
}
EXPORT_SYMBOL(wcd9xxx_set_intf_type);
