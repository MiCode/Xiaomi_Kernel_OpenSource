/*
 * arch/arm/mach-tegra/tegra_rst_reason.c
 *
 * Copyright (c) 2013-2015, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

#include <mach/iomap.h>
#include <mach/tegra_rst_reason.h>

#include "board.h"

static enum pmic_rst_reason pmic_rst_reason_data = INVALID;

static struct kobject *rst_reason_kobj;

static ssize_t pmc_rst_reason_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
static ssize_t pmc_rst_flag_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
static ssize_t pmic_rst_reason_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

static struct kobj_attribute pmc_rst_reason_attr = __ATTR_RO(pmc_rst_reason);
static struct kobj_attribute pmc_rst_flag_attr = __ATTR_RO(pmc_rst_flag);
static struct kobj_attribute pmic_rst_reason_attr = __ATTR_RO(pmic_rst_reason);

const struct attribute *rst_reason_attributes[] = {
	&pmc_rst_reason_attr.attr,
	&pmc_rst_flag_attr.attr,
	&pmic_rst_reason_attr.attr,
	NULL,
};

static ssize_t pmc_rst_reason_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u32 val;

#if defined(CONFIG_ARCH_TEGRA_3x_SOC) || defined(CONFIG_ARCH_TEGRA_11x_SOC)
	val = readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_RST_STATUS) & PMC_RST_MASK;
	if (val >= ARRAY_SIZE(pmc_rst_reason_msg))
		return sprintf(buf, "invalid 0x%08x\n", val);
	else
		return sprintf(buf, "%s\n", pmc_rst_reason_msg[val]);
#else
	return sprintf(buf, "not supported\n");
#endif
}

static ssize_t pmc_rst_flag_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u32 val;

#if defined(CONFIG_ARCH_TEGRA_3x_SOC) || defined(CONFIG_ARCH_TEGRA_11x_SOC)
	val = readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_RST_FLAG);
	return sprintf(buf, "0x%08x\n", val);
#else
	return sprintf(buf, "not supported\n");
#endif
}

static ssize_t pmic_rst_reason_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	pmic_rst_reason_data = tegra_get_pmic_rst_reason();
	return sprintf(buf, "%s\n", pmic_rst_reason_msg[pmic_rst_reason_data]);
}

static int __init tegra_rst_reason_init(void)
{
	rst_reason_kobj = kobject_create_and_add("tegra_rst_reason",
		kernel_kobj);
	if (!rst_reason_kobj ||
			sysfs_create_files(rst_reason_kobj,
				rst_reason_attributes)) {
		pr_err("%s: rst_reason_kobj create fail\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

late_initcall(tegra_rst_reason_init);
