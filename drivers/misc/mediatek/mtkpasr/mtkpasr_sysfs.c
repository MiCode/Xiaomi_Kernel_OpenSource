/*
 * Compressed RAM block device
 *
 * Copyright (C) 2008, 2009, 2010  Nitin Gupta
 *
 * This code is released using a dual license strategy: BSD/GPL
 * You can choose the licence that better fits your requirements.
 *
 * Released under the terms of 3-clause BSD License
 * Released under the terms of GNU General Public License Version 2.0
 *
 * Project home: http://compcache.googlecode.com/
 */

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <linux/module.h>
#include <linux/device.h>
#include <linux/genhd.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/suspend.h>
#include <asm/cacheflush.h>
#include <mach/wd_api.h>

#include "mtkpasr_drv.h"

#define  CONFIG_MTKPASR_PROFILE

/* Timing profiling */
#ifdef CONFIG_MTKPASR_PROFILE
static unsigned long long mtkpasr_start_ns, mtkpasr_end_ns;
#define MTKPASR_START_PROFILE()	do {											\
					mtkpasr_start_ns = sched_clock();						\
				} while (0)
#define MTKPASR_END_PROFILE()	do {											\
					mtkpasr_end_ns = sched_clock();							\
					mtkpasr_log(" {{{Elapsed[%llu]ns}}}\n", (mtkpasr_end_ns - mtkpasr_start_ns));	\
				} while (0)
#else
#define MTKPASR_START_PROFILE()	do {} while (0)
#define MTKPASR_END_PROFILE()	do {} while (0)
#endif

/* Statistics */
unsigned long mtkpasr_triggered;
unsigned long failed_mtkpasr;
static int mtkpasr_sroff;
static int mtkpasr_dpd;

/*
 * Flow Control -
 *
 * [31]: Notification bit on whether PASR flow is triggered (default:0, means not triggered yet )
 * [15..0]: SR pass mask of 16 banks (default: 0xFFFF, means all pass)
 */
#define MTKPASR_SET_TRIGGERED	0x80000000
#define MTKPASR_CLEAR_TRIGGERED	0x7FFFFFFF
#define MTKPASR_FORCE_RANK1	0x00020000

static unsigned long mtkpasr_control = 0xFFFF;

void set_mtkpasr_triggered(void)
{
	mtkpasr_control |= MTKPASR_SET_TRIGGERED;
}

void clear_mtkpasr_triggered(void)
{
	mtkpasr_control &= MTKPASR_CLEAR_TRIGGERED;
}

bool is_mtkpasr_triggered(void)
{
	return !!(mtkpasr_control & MTKPASR_SET_TRIGGERED);
}

static struct mtkpasr *dev_to_mtkpasr(struct device *dev)
{
	return mtkpasr_device;
}

static ssize_t mem_used_total_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u64 val = 0;

	val = (u64)(mtkpasr_acquire_total() - mtkpasr_acquire_frees());
	return sprintf(buf, "%llu\n", val);
}

static ssize_t compr_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mtkpasr *mtkpasr = dev_to_mtkpasr(dev);

	return sprintf(buf, "Good compress [%u] : Bad compress [%u]\n", mtkpasr->stats.good_compress, mtkpasr->stats.bad_compress);
}

static ssize_t membank_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return mtkpasr_show_banks(buf);
}

#ifdef CONFIG_MTKPASR_MAFL
static ssize_t page_reserved_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "Reserved pages [%lu]\n", (unsigned long)0/*mtkpasr_show_page_reserved()*/);
}
#endif

static ssize_t enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d [%lu]\n", mtkpasr_enable, mtkpasr_enable_sr);
}

/* 0: all disabled 1: enable MTKPASR 2: enable SR control 3: all enabled*/
static ssize_t enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	int ret;
	int value;

	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	mtkpasr_enable = (value & 0x1) ? 1 : 0;
	mtkpasr_enable_sr = ((unsigned long)value & 0x2) ? 1 : 0;
	return len;
}

static ssize_t debug_level_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", mtkpasr_debug_level);
}

static ssize_t debug_level_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	int ret;
	int value;

	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	mtkpasr_debug_level = (value <= 0) ? 0 : value;

	return len;
}

static ssize_t mtkpasr_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "Enter [%lu]times - Fail [%lu]times :: Last Success - SR-OFF[0x%x] DPD[0x%x]\n"
			, mtkpasr_triggered, failed_mtkpasr, mtkpasr_sroff, mtkpasr_dpd);
}

/* 1: pass, 0: mask */
static ssize_t srmask_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", mtkpasr_control);
}

static ssize_t srmask_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
	int ret;
	int value;

	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	mtkpasr_control = value & 0x3FFFF;
	return len;
}

/* Show overall executing status */
static ssize_t execstate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mtkpasr *mtkpasr = dev_to_mtkpasr(dev);
	u64 val = 0;
	int len = 0, tmp;

	/* Compression status */
	tmp = sprintf(buf, "Good compress [%u] : Bad compress [%u]\n", mtkpasr->stats.good_compress, mtkpasr->stats.bad_compress);
	buf += tmp;
	len += tmp;

	/* Debug level */
	tmp = sprintf(buf, "%d\n", mtkpasr_debug_level);
	buf += tmp;
	len += tmp;

	/* Enable status */
	tmp = sprintf(buf, "%d [%lu]\n", mtkpasr_enable, mtkpasr_enable_sr);
	buf += tmp;
	len += tmp;

	/* Available size for external compression */
	val = (u64)(mtkpasr_acquire_total() - mtkpasr_acquire_frees());
	tmp = sprintf(buf, "%llu\n", val);
	buf += tmp;
	len += tmp;

	/* Bank/Rank information */
	tmp = mtkpasr_show_banks(buf);
	buf += tmp;
	len += tmp;

	/* MTKPASR status */
	tmp = sprintf(buf, "Enter [%lu]times - Fail [%lu]times :: Last Success - SR-OFF[0x%x] DPD[0x%x]\n"
			, mtkpasr_triggered, failed_mtkpasr, mtkpasr_sroff, mtkpasr_dpd);
	buf += tmp;
	len += tmp;

	/* Page reserved by MTKPASR */
	tmp = sprintf(buf, "Page reserved[%lu]\n", mtkpasr_show_page_reserved());
	buf += tmp;
	len += tmp;

	/* SR mask */
	tmp = sprintf(buf, "%lu\n", mtkpasr_control);
	buf += tmp;
	len += tmp;

	return len;
}

#ifdef CONFIG_MTKPASR
/*extern void try_to_shrink_slab(void);*/
extern void mtkpasr_reset_state(void);

/* Hook to Linux PM */
void mtkpasr_phaseone_ops(void)
{
	struct wd_api *wd_api = NULL;

	/* To restart wdt */
	if (get_wd_api(&wd_api) == 0) {
		mtkpasr_log("PASR kicks WDT!\n");
		wd_api->wd_restart(WD_TYPE_NORMAL);
	}

	IS_MTKPASR_ENABLED_NORV;

	/* It means no need to apply this op (Simply for paging or other periodic wakeups) */
	if (is_mtkpasr_triggered()) {
		return;
	}

	MTKPASR_START_PROFILE();

	/* It will go to MTKPASR stage */
	current->flags |= PF_MTKPASR;

	/* Inform all other memory pools to release their memory
	try_to_shrink_slab();*/

	/* It will leave MTKPASR stage */
	current->flags &= ~PF_MTKPASR;

#ifdef CONFIG_MTKPASR_MAFL
	if (mtkpasr_no_phaseone_ops())
		goto no_phaseone;
#endif

	mtkpasr_info("\n");
	/* Drop cache - linux/mm.h */
	drop_pagecache();

#ifdef CONFIG_MTKPASR_MAFL
no_phaseone:
#endif
	MTKPASR_END_PROFILE();
}

/*
 * Exported to Kernel/Platform PM -
 * It will fail only when some pending wakeup source is detected.
 * This op may enable irq, so we should recover it if needed.
 */
int pasr_enter(u32 *sr, u32 *dpd)
{
	enum mtkpasr_phase result;
	int ret = 0;
	int irq_disabled = 0;		/* MTKPASR_FLUSH -> drain_all_pages -> on_each_cpu_mask will enable local irq */

	IS_MTKPASR_ENABLED;

	/* Check whether we are in irq-disabled environment */
	if (irqs_disabled()) {
		irq_disabled = 1;
	}

	MTKPASR_START_PROFILE();

	/* SR-Off/DPD - Check which banks/ranks can enter PASR/DPD - State change:MTKPASR_DISABLINGSR -> MTKPASR_ON (-> MTKPASR_DPD_ON) */
	result = mtkpasr_disablingSR(sr, dpd);
	mtkpasr_sroff = *sr;
	mtkpasr_dpd = *dpd;

	MTKPASR_END_PROFILE();

	/* To mask some banks */
	set_mtkpasr_triggered();
	*sr = *sr & mtkpasr_control;

#ifdef CONFIG_MTKPASR_DEBUG
	/* Force RANK1 to be all PASRed */
	if (mtkpasr_control & MTKPASR_FORCE_RANK1) {
		*sr |= 0xFF00;
	}
#endif

	if (result == MTKPASR_GET_WAKEUP) {
		mtkpasr_restoring();
		mtkpasr_err("PM: Failed to enter SR_OFF/DPD\n");
		++failed_mtkpasr;
		ret = -1;
	} else if (result == MTKPASR_WRONG_STATE) {
		mtkpasr_reset_state();
		mtkpasr_err("Wrong state!\n");
		++failed_mtkpasr;
	}

	/* Recover it to irq-disabled environment if needed */
	if (irq_disabled == 1) {
		if (!irqs_disabled()) {
			mtkpasr_log("IRQ is enabled! To disable it here!\n");
			arch_suspend_disable_irqs();
		}
	}

	return ret;
}

/* Exported to Kernel/Platform PM */
int pasr_exit(void)
{
	enum mtkpasr_phase result;

	IS_MTKPASR_ENABLED;

	MTKPASR_START_PROFILE();

	/* SR on / Disable DPD - State change: MTKPASR_ON(MTKPASR_DPD_ON) -> MTKPASR_EXITING */
	result = mtkpasr_enablingSR();

	MTKPASR_END_PROFILE();

	if (result == MTKPASR_WRONG_STATE) {
		mtkpasr_err("Wrong state!\n");
	}
	
	return 0;
}
#endif

static DEVICE_ATTR(mem_used_total, S_IRUGO, mem_used_total_show, NULL);
static DEVICE_ATTR(compr_status, S_IRUGO, compr_status_show, NULL);
static DEVICE_ATTR(membank, S_IRUGO, membank_show, NULL);
#ifdef CONFIG_MTKPASR_MAFL
static DEVICE_ATTR(page_reserved, S_IRUGO, page_reserved_show, NULL);
#endif
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, enable_show, enable_store);
static DEVICE_ATTR(debug_level, S_IRUGO | S_IWUSR, debug_level_show, debug_level_store);
static DEVICE_ATTR(mtkpasr_status, S_IRUGO, mtkpasr_status_show, NULL);
static DEVICE_ATTR(srmask, S_IRUGO | S_IWUSR, srmask_show, srmask_store);
static DEVICE_ATTR(execstate, S_IRUGO, execstate_show, NULL);

static struct attribute *mtkpasr_attrs[] = {
	&dev_attr_mem_used_total.attr,
	&dev_attr_compr_status.attr,
	&dev_attr_membank.attr,
#ifdef CONFIG_MTKPASR_MAFL
	&dev_attr_page_reserved.attr,
#endif
	&dev_attr_enable.attr,
	&dev_attr_debug_level.attr,
	&dev_attr_mtkpasr_status.attr,
	&dev_attr_srmask.attr,
	&dev_attr_execstate.attr,
	NULL,
};

struct attribute_group mtkpasr_attr_group = {
	.attrs = mtkpasr_attrs,
	.name = "mtkpasr",
};
