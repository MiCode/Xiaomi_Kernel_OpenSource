/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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
 * This file contains the part of the IOMMUv1 PMU driver that actually touches
 * IOMMU PMU registers.
 */

#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include "msm_iommu_hw-v1.h"
#include "msm_iommu_perfmon.h"
#include <linux/qcom_iommu.h>

#define PMCR_P_MASK		(0x1)
#define PMCR_P_SHIFT		(1)
#define PMCR_P			(PMCR_P_MASK << PMCR_P_SHIFT)
#define PMCFGR_NCG_MASK		(0xFF)
#define PMCFGR_NCG_SHIFT	(24)
#define PMCFGR_NCG		(PMCFGR_NCG_MASK << PMCFGR_NCG_SHIFT)
#define PMCFGR_N_MASK		(0xFF)
#define PMCFGR_N_SHIFT		(0)
#define PMCFGR_N		(PMCFGR_N_MASK << PMCFGR_N_SHIFT)
#define CR_E			0x1
#define CGCR_CEN		0x800
#define CGCR_CEN_SHFT		(1 << 11)
#define PMCGCR_CGNC_MASK	(0x0F)
#define PMCGCR_CGNC_SHIFT	(24)
#define PMCGCR_CGNC		(PMCGCR_CGNC_MASK << PMCGCR_CGNC_SHIFT)
#define PMCGCR_(group)		(PMCGCR_N + group*4)

#define PMOVSCLR_(n)		(PMOVSCLR_N + n*4)
#define PMCNTENSET_(n)		(PMCNTENSET_N + n*4)
#define PMCNTENCLR_(n)		(PMCNTENCLR_N + n*4)
#define PMINTENSET_(n)		(PMINTENSET_N + n*4)
#define PMINTENCLR_(n)		(PMINTENCLR_N + n*4)

#define PMEVCNTR_(n)		(PMEVCNTR_N + n*4)
#define PMEVTYPER_(n)		(PMEVTYPER_N + n*4)


static unsigned int iommu_pm_is_hw_access_OK(const struct iommu_pmon *pmon)
{
	/*
	 * IOMMUv1 is not in the always on domain so we need to make sure
	 * the regulators are turned on in addition to clocks before we allow
	 * access to the hardware thus we check if we have attached to the
	 * IOMMU in addition to checking if we have enabled PMU.
	 */
	return pmon->enabled && (pmon->iommu_attach_count > 0);
}

static void iommu_pm_grp_enable(struct iommu_info *iommu, unsigned int grp_no)
{
	unsigned int pmcgcr;

	pmcgcr = readl_relaxed(iommu->base + PMCGCR_(grp_no));
	pmcgcr |= CGCR_CEN;
	writel_relaxed(pmcgcr, iommu->base + PMCGCR_(grp_no));
}

static void iommu_pm_grp_disable(struct iommu_info *iommu, unsigned int grp_no)
{
	unsigned int pmcgcr;

	pmcgcr = readl_relaxed(iommu->base + PMCGCR_(grp_no));
	pmcgcr &= ~CGCR_CEN;
	writel_relaxed(pmcgcr, iommu->base + PMCGCR_(grp_no));
}

static void iommu_pm_enable(struct iommu_info *iommu)
{
	unsigned int pmcr;

	pmcr = readl_relaxed(iommu->base + PMCR);
	pmcr |= CR_E;
	writel_relaxed(pmcr, iommu->base + PMCR);
}

static void iommu_pm_disable(struct iommu_info *iommu)
{
	unsigned int pmcr;

	pmcr = readl_relaxed(iommu->base + PMCR);
	pmcr &= ~CR_E;
	writel_relaxed(pmcr, iommu->base + PMCR);
}

static void iommu_pm_reset_counters(const struct iommu_info *iommu)
{
	unsigned int pmcr;

	pmcr = readl_relaxed(iommu->base + PMCR);
	pmcr |= PMCR_P;
	writel_relaxed(pmcr, iommu->base + PMCR);
}

static void iommu_pm_check_for_overflow(struct iommu_pmon *pmon)
{
	struct iommu_pmon_counter *counter;
	struct iommu_info *iommu = &pmon->iommu;
	unsigned int reg_no = 0;
	unsigned int bit_no;
	unsigned int reg_value;
	unsigned int i;
	unsigned int j;
	unsigned int curr_reg = 0;

	reg_value = readl_relaxed(iommu->base + PMOVSCLR_(curr_reg));

	for (i = 0; i < pmon->num_groups; ++i) {
		struct iommu_pmon_cnt_group *cnt_grp = &pmon->cnt_grp[i];

		for (j = 0; j < cnt_grp->num_counters; ++j) {
			counter = &cnt_grp->counters[j];
			reg_no = counter->absolute_counter_no / 32;
			bit_no = counter->absolute_counter_no % 32;
			if (reg_no != curr_reg) {
				/* Clear overflow bits */
				writel_relaxed(reg_value, iommu->base +
					       PMOVSCLR_(reg_no));
				curr_reg = reg_no;
				reg_value = readl_relaxed(iommu->base +
							  PMOVSCLR_(curr_reg));
			}

			if (counter->enabled) {
				if (reg_value & (1 << bit_no))
					counter->overflow_count++;
			}
		}
	}

	/* Clear overflow */
	writel_relaxed(reg_value, iommu->base + PMOVSCLR_(reg_no));
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

	iommu->ops->iommu_lock_acquire(0);
	iommu_pm_check_for_overflow(pmon);
	iommu->ops->iommu_lock_release(0);

	mutex_unlock(&pmon->lock);

out:
	return IRQ_HANDLED;
}

static void iommu_pm_counter_enable(struct iommu_info *iommu,
				    struct iommu_pmon_counter *counter)
{
	unsigned int reg_no = counter->absolute_counter_no / 32;
	unsigned int bit_no = counter->absolute_counter_no % 32;
	unsigned int reg_value;

	/* Clear overflow of counter */
	reg_value = 1 << bit_no;
	writel_relaxed(reg_value, iommu->base + PMOVSCLR_(reg_no));

	/* Enable counter */
	writel_relaxed(reg_value, iommu->base + PMCNTENSET_(reg_no));
	counter->enabled = 1;
}

static void iommu_pm_counter_disable(struct iommu_info *iommu,
				     struct iommu_pmon_counter *counter)
{
	unsigned int reg_no = counter->absolute_counter_no / 32;
	unsigned int bit_no = counter->absolute_counter_no % 32;
	unsigned int reg_value;

	counter->enabled = 0;

	/* Disable counter */
	reg_value = 1 << bit_no;
	writel_relaxed(reg_value, iommu->base + PMCNTENCLR_(reg_no));

	/* Clear overflow of counter */
	writel_relaxed(reg_value, iommu->base + PMOVSCLR_(reg_no));
}

/*
 * Must be called after iommu_start_access() is called
 */
static void iommu_pm_ovfl_int_enable(struct iommu_info *iommu,
				     const struct iommu_pmon_counter *counter)
{
	unsigned int reg_no = counter->absolute_counter_no / 32;
	unsigned int bit_no = counter->absolute_counter_no % 32;
	unsigned int reg_value;

	/* Enable overflow interrupt for counter */
	reg_value = (1 << bit_no);
	writel_relaxed(reg_value, iommu->base + PMINTENSET_(reg_no));
}

/*
 * Must be called after iommu_start_access() is called
 */
static void iommu_pm_ovfl_int_disable(struct iommu_info *iommu,
				      const struct iommu_pmon_counter *counter)
{
	unsigned int reg_no = counter->absolute_counter_no / 32;
	unsigned int bit_no = counter->absolute_counter_no % 32;
	unsigned int reg_value;

	/* Disable overflow interrupt for counter */
	reg_value = 1 << bit_no;
	writel_relaxed(reg_value, iommu->base + PMINTENCLR_(reg_no));
}

static void iommu_pm_set_event_class(struct iommu_pmon *pmon,
				    unsigned int count_no,
				    unsigned int event_class)
{
	writel_relaxed(event_class, pmon->iommu.base + PMEVTYPER_(count_no));
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
	/* No initialization needed */
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

struct iommu_pm_hw_ops *iommu_pm_get_hw_ops_v1(void)
{
	return &iommu_pm_hw_ops;
}
EXPORT_SYMBOL(iommu_pm_get_hw_ops_v1);
