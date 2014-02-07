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

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/export.h>
#include <linux/iommu.h>
#include <mach/iommu.h>

static DEFINE_MUTEX(iommu_list_lock);
static LIST_HEAD(iommu_list);

#define MRC(reg, processor, op1, crn, crm, op2)				\
__asm__ __volatile__ (							\
"   mrc   "   #processor "," #op1 ", %0,"  #crn "," #crm "," #op2 "\n"  \
: "=r" (reg))

#define RCP15_PRRR(reg)   MRC(reg, p15, 0, c10, c2, 0)
#define RCP15_NMRR(reg)   MRC(reg, p15, 0, c10, c2, 1)

#define RCP15_MAIR0(reg)   MRC(reg, p15, 0, c10, c2, 0)
#define RCP15_MAIR1(reg)   MRC(reg, p15, 0, c10, c2, 1)

static struct iommu_access_ops *iommu_access_ops;

struct bus_type msm_iommu_sec_bus_type = {
	.name = "msm_iommu_sec_bus",
};

void msm_set_iommu_access_ops(struct iommu_access_ops *ops)
{
	iommu_access_ops = ops;
}

struct iommu_access_ops *msm_get_iommu_access_ops()
{
	BUG_ON(iommu_access_ops == NULL);
	return iommu_access_ops;
}
EXPORT_SYMBOL(msm_get_iommu_access_ops);

void msm_iommu_add_drv(struct msm_iommu_drvdata *drv)
{
	mutex_lock(&iommu_list_lock);
	list_add(&drv->list, &iommu_list);
	mutex_unlock(&iommu_list_lock);
}

void msm_iommu_remove_drv(struct msm_iommu_drvdata *drv)
{
	mutex_lock(&iommu_list_lock);
	list_del(&drv->list);
	mutex_unlock(&iommu_list_lock);
}

static int find_iommu_ctx(struct device *dev, void *data)
{
	struct msm_iommu_ctx_drvdata *c;

	c = dev_get_drvdata(dev);
	if (!c || !c->name)
		return 0;

	return !strcmp(data, c->name);
}

static struct device *find_context(struct device *dev, const char *name)
{
	return device_find_child(dev, (void *)name, find_iommu_ctx);
}

struct device *msm_iommu_get_ctx(const char *ctx_name)
{
	struct msm_iommu_drvdata *drv;
	struct device *dev = NULL;

	mutex_lock(&iommu_list_lock);
	list_for_each_entry(drv, &iommu_list, list) {
		dev = find_context(drv->dev, ctx_name);
		if (dev)
			break;
	}
	mutex_unlock(&iommu_list_lock);

	put_device(dev);

	if (!dev || !dev_get_drvdata(dev)) {
		pr_debug("Could not find context <%s>\n", ctx_name);
		dev = ERR_PTR(-EPROBE_DEFER);
	}

	return dev;
}
EXPORT_SYMBOL(msm_iommu_get_ctx);

#ifdef CONFIG_ARM
/* These values come from proc-v7-2level.S */
#define PRRR_VALUE 0xff0a81a8
#define NMRR_VALUE 0x40e040e0

/* These values come from proc-v7-3level.S */
#define MAIR0_VALUE 0xeeaa4400
#define MAIR1_VALUE 0xff000004

#ifdef CONFIG_IOMMU_LPAE
#ifdef CONFIG_ARM_LPAE
/*
 * If CONFIG_ARM_LPAE AND CONFIG_IOMMU_LPAE are enabled we can use the MAIR
 * register directly
 */
u32 msm_iommu_get_mair0(void)
{
	unsigned int mair0;

	RCP15_MAIR0(mair0);
	return mair0;
}

u32 msm_iommu_get_mair1(void)
{
	unsigned int mair1;

	RCP15_MAIR1(mair1);
	return mair1;
}
#else
/*
 * However, If CONFIG_ARM_LPAE is not enabled but CONFIG_IOMMU_LPAE is enabled
 * we'll just use the hard coded values directly..
 */
u32 msm_iommu_get_mair0(void)
{
	return MAIR0_VALUE;
}

u32 msm_iommu_get_mair1(void)
{
	return MAIR1_VALUE;
}
#endif

#else
#ifdef CONFIG_ARM_LPAE
/*
 * If CONFIG_ARM_LPAE is enabled AND CONFIG_IOMMU_LPAE is disabled
 * we must use the hardcoded values.
 */
u32 msm_iommu_get_prrr(void)
{
	return PRRR_VALUE;
}

u32 msm_iommu_get_nmrr(void)
{
	return NMRR_VALUE;
}
#else
/*
 * If both CONFIG_ARM_LPAE AND CONFIG_IOMMU_LPAE are disabled
 * we can use the registers directly.
 */
#define RCP15_PRRR(reg)		MRC(reg, p15, 0, c10, c2, 0)
#define RCP15_NMRR(reg)		MRC(reg, p15, 0, c10, c2, 1)

u32 msm_iommu_get_prrr(void)
{
	u32 prrr;

	RCP15_PRRR(prrr);
	return prrr;
}

u32 msm_iommu_get_nmrr(void)
{
	u32 nmrr;

	RCP15_NMRR(nmrr);
	return nmrr;
}
#endif
#endif
#endif
#ifdef CONFIG_ARM64
u32 msm_iommu_get_prrr(void)
{
	unsigned int mair0;
	u64 tmp;

	asm volatile(
	"	mrs	%0, mair_el1\n"
	: "=&r" (tmp));

	mair0 = tmp & 0xffffffff;
	return mair0;
}

u32 msm_iommu_get_nmrr(void)
{
	unsigned int mair1;
	u64 tmp;

	asm volatile(
	"	mrs	%0, mair_el1\n"
	: "=&r" (tmp));

	mair1 = tmp >> 32;
	return mair1;
}
#endif
