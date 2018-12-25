/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/printk.h>

/* Memory lowpower private header file */
#include "../internal.h"

/* PASR private header file */
#include "mtkpasr_drv.h"

/* Header file for DRAMC PASR configuration */
#include <mtk_dramc.h>

#ifdef MTK_PASR_VCORE_DVFS_CONTROL
#include <mtk_vcorefs_manager.h>
#endif

#ifdef CONFIG_MTK_DCS
#include <mt-plat/mtk_meminfo.h>
#endif

static struct mtkpasr_bank *mtkpasr_banks;
static int num_banks;
static int mtkpasr_enable = 1;
static unsigned long max_bank_pfns;

#ifdef CONFIG_MTK_DCS
static unsigned int max_channel_num;
static struct pasrvec *mtkpasr_vec;
static int dcs_acquired;
static enum dcs_status dcs_status = DCS_BUSY;
#endif

/* Internal control parameters */
static unsigned long mtkpasr_triggered, mtkpasr_on, mtkpasr_srmask;

/* Count the number of free pages */
static void count_free_pages(unsigned long *spfn, unsigned long *epfn)
{
	int bank;
	unsigned long max_spfn, min_epfn;

	MTKPASR_PRINT("%s: spfn[%lu] epfn[%lu] -\n", __func__, *spfn, *epfn);

	for (bank = 0; bank < num_banks; bank++) {
		max_spfn = max(mtkpasr_banks[bank].start_pfn, *spfn);
		min_epfn = min(mtkpasr_banks[bank].end_pfn, *epfn);
		if (min_epfn <= max_spfn)
			continue;
		mtkpasr_banks[bank].free += (min_epfn - max_spfn);
		MTKPASR_PRINT("@@@ bank[%d] free[%lu]\n",
				bank, mtkpasr_banks[bank].free);
	}

	MTKPASR_PRINT("\n");
}

#ifdef DEBUG_FOR_CHANNEL_SWITCH
static void mtkpasr_debug_channel_switch(void)
{
	int which, i, chc;
	struct pasrvec *vec;

	vec = kcalloc(get_channel_num(), sizeof(struct pasrvec), GFP_KERNEL);
	if (!vec)
		return;

	which = 16;
	do {
		pr_info("(@)%s: MASK[0x%lx]\n", __func__, mtkpasr_on);
		for (chc = 1; chc <= 4; chc <<= 1) {
			pr_info("(@@@)chconfig[%d]\n", chc);
			if (fill_pasr_on_by_chconfig(chc, vec, mtkpasr_on)) {
				pr_info("Bad chconfig!\n");
				continue;
			}
			for (i = 0; i < get_channel_num(); i++)
				pr_info("%d[%d][0x%lx]\n", i, vec[i].channel,
						vec[i].pasr_on);
		}
		mtkpasr_on = ((mtkpasr_on >> 15) | (mtkpasr_on << 1)) & 0xFFFF;
	} while (--which > 0);

	kfree(vec);
}
#endif

static void restore_pasr(void)
{
retry:
	/* APMCU flow */
	if (exit_pasr_dpd_config() != 0)
		pr_info("%s: failed to program DRAMC!\n", __func__);
	else
		mtkpasr_on = 0x0;

	/* Retry until success */
	if (mtkpasr_on != 0)
		goto retry;
}

#ifdef CONFIG_MTK_DCS
static void restore_dcs_pasr(void)
{
retry:
	/* APMCU flow */
	if (exit_dcs_pasr_dpd_config() != 0)
		pr_info("%s: failed to program DRAMC!\n", __func__);
	else
		mtkpasr_on = 0x0;

	/* Retry until success */
	if (mtkpasr_on != 0)
		goto retry;
}

static void enable_dcs_pasr(void)
{
	int chconfig, ret;
	int chid;

	if (!dcs_initialied()) {
		dcs_status = DCS_NORMAL;
		dcs_acquired = 1;
		goto bypass_dcs;
	}

	/* Step0 - switch to lowpower mode */
#ifdef DCS_SCREENOFF_ONLY_MODE
	ret = dcs_exit_perf(DCS_KICKER_DEBUG);
	if (ret)
		pr_info("exit perf failed, kick=%d\n", DCS_KICKER_DEBUG);
#endif

	ret = dcs_switch_to_lowpower();
	if (ret != 0) {
		pr_info("%s: failed to swtich to lowpower mode, error (%d)\n",
				__func__, ret);
		return;
	}

	/* Step1 - Acquire DCS status */
	ret = dcs_get_dcs_status_lock(&chconfig, &dcs_status);
	if (ret != 0) {
		pr_info("%s: failed to get DCS status, error (%d)\n",
				__func__, ret);
		return;
	}

	/* We got DCS status */
	dcs_acquired = 1;
	pr_debug("%s: DCS status (%d, %d)\n", __func__, chconfig, dcs_status);

	/* Sanity check */
	if (dcs_status == DCS_NORMAL && chconfig != max_channel_num) {
		pr_info("%s: max_channel_num(%u) DCS status (%d, %d) mismatched\n",
				__func__, max_channel_num,
				chconfig, dcs_status);
		goto err;
	}

	/* Step2 - Configure PASR by current stable channel setting (DRAFT) */
	if (dcs_status == DCS_NORMAL) {
bypass_dcs:
		if (enter_pasr_dpd_config(mtkpasr_on & 0xFF, mtkpasr_on >> 0x8))
			pr_info("%s: failed to program DRAMC!\n", __func__);
	} else if (dcs_status == DCS_LOWPOWER) {
		/* Get channel-based PASR configuration */
		ret = fill_pasr_on_by_chconfig(chconfig, mtkpasr_vec,
				mtkpasr_on);
		if (ret != 0) {
			pr_info("%s: failed to configure PASR, error (%d)\n",
					__func__, ret);
			goto err;
		}

		/* Configure DRAMC */
		for (chid = 0; chid < max_channel_num; chid++)
			enter_dcs_pasr_dpd_config(
					mtkpasr_vec[chid].pasr_on & 0xFF,
					mtkpasr_vec[chid].pasr_on >> 0x8, chid);
	} else {
		pr_info("%s: should not be here\n", __func__);
		goto err;
	}

	return;

err:
	dcs_acquired = 0;
	dcs_get_dcs_status_unlock();
}

static void disable_dcs_pasr(void)
{
#ifdef DCS_SCREENOFF_ONLY_MODE
	int ret;
#endif

	if (!dcs_acquired)
		return;

	/* Restore PASR */
	if (dcs_status == DCS_NORMAL)
		restore_pasr();
	else if (dcs_status == DCS_LOWPOWER)
		restore_dcs_pasr();
	else
		pr_info("%s: should not be here\n", __func__);

	/* Unlock DCS */
	dcs_acquired = 0;
	dcs_status = DCS_BUSY;
	if (dcs_initialied())
		dcs_get_dcs_status_unlock();

#ifdef DCS_SCREENOFF_ONLY_MODE
	/* enter performance mode */
	ret = dcs_enter_perf(DCS_KICKER_DEBUG);
	if (ret)
		pr_info("enter perf failed, kick=%d\n", DCS_KICKER_DEBUG);
#endif
}
#endif

/*
 * config - Identify banks/ranks, trigger APMCU flow
 * (Suppose mtkpasr_banks[].start_pfn <= mtkpasr_banks[].end_pfn)
 */
static int mtkpasr_config(int times, get_range_t func)
{
	unsigned long spfn, epfn;
	int which, i;
#ifndef CONFIG_MTK_DCS
	int retry = 3;
#endif

	/* Not enable */
	if (!mtkpasr_enable)
		return 1;

	/* Sanity check */
	if (func == NULL)
		return 1;

	MTKPASR_PRINT("%s:+\n", __func__);

	/* Reset the number of free pages to 0 */
	for (i = 0; i < num_banks; i++)
		mtkpasr_banks[i].free = 0;

	/* Find PASR-imposed range */
	for (which = 0; which < times; which++) {
		/* Query memory lowpower range */
		func(which, &spfn, &epfn);
		count_free_pages(&spfn, &epfn);
	}

	/* Find valid PASR segment */
	mtkpasr_on = 0x0;
	for (i = 0; i < num_banks; i++)
		if (mtkpasr_banks[i].free == (mtkpasr_banks[i].end_pfn -
					mtkpasr_banks[i].start_pfn))
			mtkpasr_on |= (1 << mtkpasr_banks[i].segment);

#ifdef MTK_PASR_VCORE_DVFS_CONTROL
	vcorefs_request_dvfs_opp(KIR_PASR, OPP_0);
#endif

#ifndef CONFIG_MTK_DCS
retry_pasr:
	/* APMCU flow */
	MTKPASR_PRINT("%s: PASR[0x%lx]\n", __func__, mtkpasr_on);
	if (enter_pasr_dpd_config(mtkpasr_on & 0xFF, mtkpasr_on >> 0x8) != 0) {
		if (--retry)
			goto retry_pasr;
		MTKPASR_PRINT("%s: failed to program DRAMC!\n", __func__);
	}
#else
	/* Channel based PASR configuration */
	enable_dcs_pasr();
#endif

#ifdef MTK_PASR_VCORE_DVFS_CONTROL
	vcorefs_request_dvfs_opp(KIR_PASR, OPP_UNREQ);
#endif

	++mtkpasr_triggered;

	MTKPASR_PRINT("%s:-\n", __func__);

#ifdef DEBUG_FOR_CHANNEL_SWITCH
	mtkpasr_debug_channel_switch();
#endif

	return 0;
}

/*
 * restore - Trigger APMCU flow for reset
 */
static int mtkpasr_restore(void)
{
	/* Not enable */
	if (!mtkpasr_enable)
		return 0;

	MTKPASR_PRINT("%s:+\n", __func__);

#ifdef MTK_PASR_VCORE_DVFS_CONTROL
	vcorefs_request_dvfs_opp(KIR_PASR, OPP_0);
#endif

#ifndef CONFIG_MTK_DCS
	restore_pasr();
#else
	disable_dcs_pasr();
#endif

#ifdef MTK_PASR_VCORE_DVFS_CONTROL
	vcorefs_request_dvfs_opp(KIR_PASR, OPP_UNREQ);
#endif

	MTKPASR_PRINT("%s:-\n", __func__);

	return 0;
}

static struct memory_lowpower_operation mtkpasr_handler = {
	.level = MLP_LEVEL_PASR,
	.config = mtkpasr_config,
	.restore = mtkpasr_restore,
};

/* ++ SYSFS Interface ++ */
int mtkpasr_show_banks(char *buf)
{
#define MTKPASR_SHOW_BANKS_LIMIT	(128)
	int i, len = 0, tmp;

	/* Show banks */
	for (i = 0; i < num_banks; i++) {
		tmp = snprintf(buf, MTKPASR_SHOW_BANKS_LIMIT,
				"Bank[%2d] - start_pfn[%6lx] end_pfn[%6lx] segment[%2d] rank[%d] %s\n",
				i, mtkpasr_banks[i].start_pfn,
				mtkpasr_banks[i].end_pfn - 1,
				mtkpasr_banks[i].segment,
				mtkpasr_banks[i].rank,
				((mtkpasr_on >> mtkpasr_banks[i].segment) & 0x1)
				? "[ON]" : "");
		buf += tmp;
		len += tmp;
	}

#undef MTKPASR_SHOW_BANKS_LIMIT

	return len;
}

static ssize_t membank_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return mtkpasr_show_banks(buf);
}

static ssize_t enable_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%d\n", mtkpasr_enable);
}

/* 0: disable, 1: enable */
static ssize_t enable_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t len)
{
	int ret;
	int value;

	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	mtkpasr_enable = (value & 0x1) ? 1 : 0;

	return len;
}

/* MTKPASR status */
static int show_pasr_status(char *buf)
{
	int len;

	len = sprintf(buf, "Triggered [%lu]times :: Last PASR[0x%lx]\n",
			mtkpasr_triggered, mtkpasr_on);

#ifdef CONFIG_MTK_DCS
	if (mtkpasr_vec) {
		int i, tmp;

		buf += len;
		tmp = sprintf(buf,
				"Channel-based PASR, max channel number[%d]\n",
				max_channel_num);
		buf += tmp;
		len += tmp;
		for (i = 0; i < max_channel_num; i++) {
			tmp = sprintf(buf, "ch[%d] :: PASR-masked[0x%lx]\n",
					i, mtkpasr_vec[i].pasr_on);
			buf += tmp;
			len += tmp;
		}
	}
#endif
	return len;
}

static ssize_t mtkpasr_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return show_pasr_status(buf);
}

static ssize_t srmask_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return sprintf(buf, "%lu\n", mtkpasr_srmask);
}

static ssize_t srmask_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t len)
{
	int ret;
	int value;

	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	mtkpasr_srmask = value & 0xFFFF;
	return len;
}

/* Show overall executing status */
static ssize_t execstate_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int len = 0, tmp;

	/* Enable status */
	tmp = sprintf(buf, "%d\n", mtkpasr_enable);
	buf += tmp;
	len += tmp;

	/* Bank information */
	tmp = mtkpasr_show_banks(buf);
	buf += tmp;
	len += tmp;

	/* MTKPASR status */
	tmp = show_pasr_status(buf);
	buf += tmp;
	len += tmp;

	/* SR mask */
	tmp = sprintf(buf, "%lu\n", mtkpasr_srmask);
	buf += tmp;
	len += tmp;

	return len;
}

static DEVICE_ATTR(membank, 0444, membank_show, NULL);
static DEVICE_ATTR(enable, 0644, enable_show, enable_store);
static DEVICE_ATTR(mtkpasr_status, 0444, mtkpasr_status_show, NULL);
static DEVICE_ATTR(srmask, 0644, srmask_show, srmask_store);
static DEVICE_ATTR(execstate, 0444, execstate_show, NULL);

static struct attribute *mtkpasr_attrs[] = {
	&dev_attr_membank.attr,
	&dev_attr_enable.attr,
	&dev_attr_mtkpasr_status.attr,
	&dev_attr_srmask.attr,
	&dev_attr_execstate.attr,
	NULL,
};

struct attribute_group mtkpasr_attr_group = {
	.attrs = mtkpasr_attrs,
	.name = "mtkpasr",
};

/* -- SYSFS Interface -- */

static int __init mtkpasr_construct_bankrank(void)
{
	int ret = 0, i, segn;
	unsigned long start_pfn, end_pfn;

	/* Init mtkpasr range */
	start_pfn = memory_lowpower_base() >> PAGE_SHIFT;
	end_pfn = start_pfn + (memory_lowpower_size() >> PAGE_SHIFT);
	max_bank_pfns = 0;
	ret = mtkpasr_init_range(start_pfn, end_pfn, &max_bank_pfns);
	if (ret <= 0 || max_bank_pfns == 0) {
		MTKPASR_PRINT(
		"%s: failed to init mtkpasr range ret[%d] max_bank_pfns[%lu]\n",
				__func__, ret, max_bank_pfns);
		return -1;
	}

	/* Allocate buffer for banks */
	num_banks = ret;
	mtkpasr_banks = kcalloc(num_banks, sizeof(struct mtkpasr_bank),
			GFP_KERNEL);
	if (!mtkpasr_banks) {
		MTKPASR_PRINT("%s: failed to allocate mtkpasr_banks\n",
				__func__);
		return -1;
	}

	/* Query bank, rank information */
	for (i = 0; ; i++) {
		ret = query_bank_rank_information(i, &start_pfn, &end_pfn,
				&segn);

		/* No valid bank, just break */
		if (ret < 0) {
			MTKPASR_PRINT("%s bank[%d] ret[%d]\n",
					__func__, i, ret);
			break;
		}

		/* Set mtkpasr_banks */
		mtkpasr_banks[i].start_pfn = start_pfn;
		mtkpasr_banks[i].end_pfn = end_pfn;
		mtkpasr_banks[i].segment = segn;
		/* Minus 1 to indicate which rank */
		mtkpasr_banks[i].rank = ret - 1;
	}

#ifdef CONFIG_MTK_DCS
	max_channel_num = get_channel_num();
	mtkpasr_vec = kcalloc(max_channel_num, sizeof(struct pasrvec),
			GFP_KERNEL);
	if (!mtkpasr_vec) {
		MTKPASR_PRINT("%s: failed to allocate mtkpasr_vec\n", __func__);
		kfree(mtkpasr_banks);
		mtkpasr_banks = NULL;
		return -1;
	}
#endif
	/* Aligned allocation is preferred */
	if (i != 0)
		set_memory_lowpower_aligned(
				get_order(max_bank_pfns << PAGE_SHIFT));

	/* Sanity check */
	if (i != num_banks)
		MTKPASR_PRINT("%s a[%d] != b[%d]\n", __func__, i, num_banks);

	return 0;
}

static int __init mtkpasr_init(void)
{
	MTKPASR_PRINT("%s ++\n", __func__);

	/* Check whether memory lowpower task is initialized */
	if (!memory_lowpower_task_inited())
		goto out;

	/* Create SYSFS interface */
	if (sysfs_create_group(power_kobj, &mtkpasr_attr_group))
		goto out;

	/* Construct PASR ranks/banks */
	if (mtkpasr_construct_bankrank())
		goto out;

	/* Register feature operations */
	register_memory_lowpower_operation(&mtkpasr_handler);
out:
	MTKPASR_PRINT("%s --\n", __func__);
	return 0;
}

static void __exit mtkpasr_exit(void)
{
	unregister_memory_lowpower_operation(&mtkpasr_handler);

	/* Release mtkpasr_banks */
	if (mtkpasr_banks != NULL) {
		kfree(mtkpasr_banks);
		mtkpasr_banks = NULL;
	}

	/* Remove SYSFS interface */
	sysfs_remove_group(power_kobj, &mtkpasr_attr_group);
}

late_initcall(mtkpasr_init);
module_exit(mtkpasr_exit);
