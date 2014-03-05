/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

/**
 * This file contains the part of the IOMMUv0 PMU driver that actually touches
 * IOMMU PMU registers.
 */

#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/device.h>
#include "msm_iommu_hw-v0.h"
#include "msm_iommu_perfmon.h"
#include <linux/qcom_iommu.h>

#define PM_RESET_MASK		(0xF)
#define PM_RESET_SHIFT		(0x8)
#define PM_RESET		(PM_RESET_MASK << PM_RESET_SHIFT)

#define PM_ENABLE_MASK		(0x1)
#define PM_ENABLE_SHIFT		(0x0)
#define PM_ENABLE		(PM_ENABLE_MASK << PM_ENABLE_SHIFT)

#define PM_OVFL_FLAG_MASK	(0xF)
#define PM_OVFL_FLAG_SHIFT	(0x0)
#define PM_OVFL_FLAG		(PM_OVFL_FLAG_MASK << PM_OVFL_FLAG_SHIFT)

#define PM_EVENT_TYPE_MASK	(0x1F)
#define PM_EVENT_TYPE_SHIFT	(0x2)
#define PM_EVENT_TYPE		(PM_EVENT_TYPE_MASK << PM_EVENT_TYPE_SHIFT)

#define PM_INT_EN_MASK		(0x1)
#define PM_INT_EN_SHIFT		(0x0)
#define PM_INT_EN		(PM_INT_EN_MASK << PM_INT_EN_SHIFT)

#define PM_INT_POL_MASK		(0x1)
#define PM_INT_POL_SHIFT	(0x2)
#define PM_INT_ACTIVE_HIGH	(0x1)

#define PMEVCNTR_(n)		(EMC_N + n*4)
#define PMEVTYPER_(n)		(EMCC_N + n*4)

/**
 * Translate between SMMUv0 event classes and standard ARM SMMU event classes
 */
static int iommu_pm_event_class_translation_table[] = {
	MSM_IOMMU_PMU_NO_EVENT_CLASS,
	MSM_IOMMU_PMU_NO_EVENT_CLASS,
	MSM_IOMMU_PMU_NO_EVENT_CLASS,
	0x8,
	0x9,
	MSM_IOMMU_PMU_NO_EVENT_CLASS,
	0x80,
	MSM_IOMMU_PMU_NO_EVENT_CLASS,
	0x12,
	MSM_IOMMU_PMU_NO_EVENT_CLASS,
	MSM_IOMMU_PMU_NO_EVENT_CLASS,
	MSM_IOMMU_PMU_NO_EVENT_CLASS,
	MSM_IOMMU_PMU_NO_EVENT_CLASS,
	MSM_IOMMU_PMU_NO_EVENT_CLASS,
	MSM_IOMMU_PMU_NO_EVENT_CLASS,
	0x10,
};

static int iommu_pm_translate_event_class(int event_class)
{
	const unsigned int TBL_LEN =
			ARRAY_SIZE(iommu_pm_event_class_translation_table);
	unsigned int i;

	if (event_class < 0)
		return event_class;

	for (i = 0; i < TBL_LEN; ++i) {
		if (iommu_pm_event_class_translation_table[i] == event_class)
			return i;
	}
	return MSM_IOMMU_PMU_NO_EVENT_CLASS;
}

static unsigned int iommu_pm_is_hw_access_OK(const struct iommu_pmon *pmon)
{
	/*
	 * IOMMUv0 is in always ON domain so we don't care whether we are
	 * attached or not. We only care whether the PMU is enabled or
	 * not meaning clocks are turned on.
	 */
	return pmon->enabled;
}

static void iommu_pm_grp_enable(struct iommu_info *iommu, unsigned int grp_no)
{
	/* No group concept in v0. */
}

static void iommu_pm_grp_disable(struct iommu_info *iommu, unsigned int grp_no)
{
	/* No group concept in v0. */
}

static void iommu_pm_set_int_active_high(const struct iommu_info *iommu)
{
	unsigned int emmc;
	emmc = readl_relaxed(iommu->base + EMMC);
	emmc |= (PM_INT_ACTIVE_HIGH & PM_INT_POL_MASK) << PM_INT_POL_SHIFT;
	writel_relaxed(emmc, iommu->base + EMMC);
}

static void iommu_pm_enable(struct iommu_info *iommu)
{
	unsigned int emmc;
	emmc = readl_relaxed(iommu->base + EMMC);
	emmc |= PM_ENABLE;
	writel_relaxed(emmc, iommu->base + EMMC);
}

static void iommu_pm_disable(struct iommu_info *iommu)
{
	unsigned int emmc;
	emmc = readl_relaxed(iommu->base + EMMC);
	emmc &= ~PM_ENABLE;
	writel_relaxed(emmc, iommu->base + EMMC);
}

static void iommu_pm_reset_counters(const struct iommu_info *iommu)
{
	unsigned int emmc;
	emmc = readl_relaxed(iommu->base + EMMC);
	emmc |= PM_RESET;
	writel_relaxed(emmc, iommu->base + EMMC);
}

static void iommu_pm_check_for_overflow(struct iommu_pmon *pmon)
{
	struct iommu_pmon_counter *counter;
	struct iommu_info *iommu = &pmon->iommu;
	unsigned int reg_value;
	unsigned int j;
	struct iommu_pmon_cnt_group *cnt_grp = &pmon->cnt_grp[0];

	reg_value = readl_relaxed(iommu->base + EMCS);
	reg_value &= PM_OVFL_FLAG;

	for (j = 0; j < cnt_grp->num_counters; ++j) {
		counter = &cnt_grp->counters[j];

		if (counter->enabled) {
			if (reg_value & (1 << counter->absolute_counter_no))
				counter->overflow_count++;
		}
	}

	/* Clear overflow */
	writel_relaxed(reg_value, iommu->base + EMCS);
}

static irqreturn_t iommu_pm_evt_ovfl_int_handler(int irq, void *dev_id)
{
	struct iommu_pmon *pmon = dev_id;
	struct iommu_info *iommu = &pmon->iommu;

	mutex_lock(&pmon->lock);

	if (!iommu_pm_is_hw_access_OK(pmon)) {
		mutex_unlock(&pmon->lock);
		goto out;
	}

	iommu->ops->iommu_lock_acquire(1);
	iommu_pm_check_for_overflow(pmon);
	iommu->ops->iommu_lock_release(1);

	mutex_unlock(&pmon->lock);

out:
	return IRQ_HANDLED;
}

static void iommu_pm_counter_enable(struct iommu_info *iommu,
				    struct iommu_pmon_counter *counter)
{
	unsigned int bit_no = counter->absolute_counter_no;
	unsigned int reg_value;

	/* Clear overflow of counter */
	reg_value = readl_relaxed(iommu->base + EMCS);
	reg_value &= (1 << bit_no);
	writel_relaxed(reg_value, iommu->base + EMCS);

	/* Enable counter */
	counter->enabled = 1;
}

static void iommu_pm_counter_disable(struct iommu_info *iommu,
				     struct iommu_pmon_counter *counter)
{
	unsigned int bit_no = counter->absolute_counter_no;
	unsigned int reg_value;

	/* Disable counter */
	counter->enabled = 0;

	/* Clear overflow of counter */
	reg_value = readl_relaxed(iommu->base + EMCS);
	reg_value &= (1 << bit_no);
	writel_relaxed(reg_value, iommu->base + EMCS);
}

/*
 * Must be called after iommu_start_access() is called
 */
static void iommu_pm_ovfl_int_enable(struct iommu_info *iommu,
				     const struct iommu_pmon_counter *counter)
{
	unsigned int reg_no = counter->absolute_counter_no;
	unsigned int reg_value;

	/* Enable overflow interrupt for counter */
	reg_value = readl_relaxed(iommu->base + PMEVTYPER_(reg_no));
	reg_value |= PM_INT_EN;
	writel_relaxed(reg_value, iommu->base + PMEVTYPER_(reg_no));
}

/*
 * Must be called after iommu_start_access() is called
 */
static void iommu_pm_ovfl_int_disable(struct iommu_info *iommu,
				      const struct iommu_pmon_counter *counter)
{
	unsigned int reg_no = counter->absolute_counter_no;
	unsigned int reg_value;

	/* Disable overflow interrupt for counter */
	reg_value = readl_relaxed(iommu->base + PMEVTYPER_(reg_no));
	reg_value &= ~PM_INT_EN;
	writel_relaxed(reg_value, iommu->base + PMEVTYPER_(reg_no));
}

static void iommu_pm_set_event_class(struct iommu_pmon *pmon,
				    unsigned int count_no,
				    unsigned int event_class)
{
	unsigned int reg_no = count_no;
	unsigned int reg_value;
	int event = iommu_pm_translate_event_class(event_class);

	if (event == MSM_IOMMU_PMU_NO_EVENT_CLASS)
		event = 0;

	reg_value = readl_relaxed(pmon->iommu.base + PMEVTYPER_(reg_no));
	reg_value &= ~(PM_EVENT_TYPE_MASK << PM_EVENT_TYPE_SHIFT);
	reg_value |= (event & PM_EVENT_TYPE_MASK) << PM_EVENT_TYPE_SHIFT;
	writel_relaxed(reg_value, pmon->iommu.base + PMEVTYPER_(reg_no));
}

static unsigned int iommu_pm_read_counter(struct iommu_pmon_counter *counter)
{
	struct iommu_pmon *pmon = counter->cnt_group->pmon;
	struct iommu_info *info = &pmon->iommu;
	unsigned int cnt_no = counter->absolute_counter_no;
	return readl_relaxed(info->base + PMEVCNTR_(cnt_no));
}

static void iommu_pm_initialize_hw(const struct iommu_pmon *pmon)
{
	const struct iommu_info *iommu = &pmon->iommu;
	struct msm_iommu_drvdata *iommu_drvdata =
					dev_get_drvdata(iommu->iommu_dev);

	/* This is called during bootup device initialization so no need
	 * for locking here.
	 */
	iommu->ops->iommu_power_on(iommu_drvdata);
	iommu->ops->iommu_clk_on(iommu_drvdata);
	iommu_pm_set_int_active_high(iommu);
	iommu->ops->iommu_clk_off(iommu_drvdata);
	iommu->ops->iommu_power_off(iommu_drvdata);
}

static struct iommu_pm_hw_ops iommu_pm_hw_ops = {
	.initialize_hw = iommu_pm_initialize_hw,
	.is_hw_access_OK = iommu_pm_is_hw_access_OK,
	.grp_enable = iommu_pm_grp_enable,
	.grp_disable = iommu_pm_grp_disable,
	.enable_pm = iommu_pm_enable,
	.disable_pm = iommu_pm_disable,
	.reset_counters = iommu_pm_reset_counters,
	.check_for_overflow = iommu_pm_check_for_overflow,
	.evt_ovfl_int_handler = iommu_pm_evt_ovfl_int_handler,
	.counter_enable = iommu_pm_counter_enable,
	.counter_disable = iommu_pm_counter_disable,
	.ovfl_int_enable = iommu_pm_ovfl_int_enable,
	.ovfl_int_disable = iommu_pm_ovfl_int_disable,
	.set_event_class = iommu_pm_set_event_class,
	.read_counter = iommu_pm_read_counter,
};

struct iommu_pm_hw_ops *iommu_pm_get_hw_ops_v0(void)
{
	return &iommu_pm_hw_ops;
}
EXPORT_SYMBOL(iommu_pm_get_hw_ops_v0);

