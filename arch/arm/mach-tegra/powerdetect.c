/*
 * arch/arm/mach-tegra/powerdetect.c
 *
 * Copyright (c) 2011 - 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/notifier.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/tegra-soc.h>
#include <linux/tegra-fuse.h>

#include "board.h"
#include "iomap.h"

#define PMC_PWR_IO_DISABLE	0x44
#define PMC_PWR_DET_ENABLE	0x48
#define PMC_PWR_DET_LATCH	0x4C
#define PMC_PWR_DET_VAL		0xE4

struct pwr_detect_cell {
	const char		*reg_id;
	u32			pwrdet_mask;
	u32			pwrio_mask;
	u32			package_mask;

	struct notifier_block	regulator_nb;
};

static bool pwrdet_rails_found;
static bool pwrdet_always_on;
static bool pwrio_always_on;
static u32 pwrdet_val;
static u32 pwrio_val;
static u32 pwrio_disabled_mask;

static DEFINE_SPINLOCK(pwr_lock);

static void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);

static inline void pmc_writel(u32 val, unsigned long addr)
{
	writel(val, pmc_base + addr);
}
static inline u32 pmc_readl(unsigned long addr)
{
	return readl(pmc_base + addr);
}


#define POWER_CELL(_reg_id, _pwrdet_mask, _pwrio_mask, _package_mask)	\
	{								\
		.reg_id = _reg_id,					\
		.pwrdet_mask = _pwrdet_mask,				\
		.pwrio_mask = _pwrio_mask,				\
		.package_mask = _package_mask,				\
	}

/* Some IO pads does not have power detect cells, but still can/should be
 * turned off when no power - set pwrdet_mask=0 for such pads */
static struct pwr_detect_cell pwr_detect_cells[] = {
#ifndef CONFIG_ARCH_TEGRA_14x_SOC
	POWER_CELL("pwrdet_nand",	(0x1 <<  1), (0x1 <<  1), 0xFFFFFFFF),
#endif
	POWER_CELL("pwrdet_uart",	(0x1 <<  2), (0x1 <<  2), 0xFFFFFFFF),
#ifndef CONFIG_ARCH_TEGRA_14x_SOC
	POWER_CELL("pwrdet_bb",		(0x1 <<  3), (0x1 <<  3), 0xFFFFFFFF),
#endif
#ifdef	CONFIG_ARCH_TEGRA_2x_SOC
	POWER_CELL("pwrdet_vi",			  0, (0x1 <<  4), 0xFFFFFFFF),
#elif defined(CONFIG_ARCH_TEGRA_3x_SOC)
	/* Tegra3 VI is connected on MID package only (id = 1, mask = 0x2) */
	POWER_CELL("pwrdet_vi",			  0, (0x1 <<  4), 0x00000002),
#endif
	POWER_CELL("pwrdet_audio",	(0x1 <<  5), (0x1 <<  5), 0xFFFFFFFF),
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
	POWER_CELL("pwrdet_lcd",	(0x1 <<  6), (0x1 <<  6), 0xFFFFFFFF),
#endif
#ifdef	CONFIG_ARCH_TEGRA_2x_SOC
	POWER_CELL("pwrdet_sd",			  0, (0x1 <<  8), 0xFFFFFFFF),
#endif
#ifndef CONFIG_ARCH_TEGRA_14x_SOC
	POWER_CELL("pwrdet_mipi",		  0, (0x1 <<  9), 0xFFFFFFFF),
#endif
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	POWER_CELL("pwrdet_cam",	(0x1 << 10), (0x1 << 10), 0xFFFFFFFF),
#if defined(CONFIG_ARCH_TEGRA_3x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
	POWER_CELL("pwrdet_pex_ctl",	(0x1 << 11), (0x1 << 11), 0xFFFFFFFF),
#endif
	POWER_CELL("pwrdet_sdmmc1",	(0x1 << 12), (0x1 << 12), 0xFFFFFFFF),
	POWER_CELL("pwrdet_sdmmc3",	(0x1 << 13), (0x1 << 13), 0xFFFFFFFF),
	POWER_CELL("pwrdet_sdmmc4",		  0, (0x1 << 14), 0xFFFFFFFF),
#if defined(CONFIG_ARCH_TEGRA_11x_SOC) || defined(CONFIG_ARCH_TEGRA_12x_SOC)
	POWER_CELL("pwrdet_hv",		(0x1 << 15), (0x1 << 15), 0xFFFFFFFF),
#endif
#endif
};

static void pwr_detect_reset(u32 pwrdet_mask)
{
	pmc_writel(pwrdet_mask, PMC_PWR_DET_ENABLE);
	barrier();
	pmc_writel(pwrdet_mask, PMC_PWR_DET_VAL);

	pmc_readl(PMC_PWR_DET_VAL);
	pmc_writel(0, PMC_PWR_DET_ENABLE);
}

static void pwr_detect_start(u32 pwrdet_mask)
{
	pmc_writel(pwrdet_mask, PMC_PWR_DET_ENABLE);
	udelay(4);

	pmc_writel(1, PMC_PWR_DET_LATCH);
	pmc_readl(PMC_PWR_DET_LATCH);
}

static void pwr_detect_latch(void)
{
	pmc_writel(0, PMC_PWR_DET_LATCH);

	pmc_readl(PMC_PWR_DET_VAL);
	pmc_writel(0, PMC_PWR_DET_ENABLE);
}

static void pwr_io_enable(u32 pwrio_mask)
{
	u32 val = pmc_readl(PMC_PWR_IO_DISABLE);
	val &= ~pwrio_mask;
	pmc_writel(val, PMC_PWR_IO_DISABLE);
}

static void pwr_io_disable(u32 pwrio_mask)
{
	u32 val = pmc_readl(PMC_PWR_IO_DISABLE);
	val |= pwrio_mask;
	pmc_writel(val, PMC_PWR_IO_DISABLE);
}

static int pwrdet_always_on_set(const char *arg, const struct kernel_param *kp)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&pwr_lock, flags);

	ret = param_set_bool(arg, kp);
	if (ret) {
		spin_unlock_irqrestore(&pwr_lock, flags);
		return ret;
	}

	if (pwrdet_always_on)
		pwr_detect_start(0xFFFFFFFF);
	else
		pwr_detect_latch();

	spin_unlock_irqrestore(&pwr_lock, flags);
	return 0;
}

static int pwrio_always_on_set(const char *arg, const struct kernel_param *kp)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&pwr_lock, flags);

	ret = param_set_bool(arg, kp);
	if (ret) {
		spin_unlock_irqrestore(&pwr_lock, flags);
		return ret;
	}

	if (pwrio_always_on)
		pwr_io_enable(0xFFFFFFFF);
	else
		pwr_io_disable(pwrio_disabled_mask);

	spin_unlock_irqrestore(&pwr_lock, flags);
	return 0;
}

static int pwrdet_always_on_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_bool(buffer, kp);
}

static struct kernel_param_ops pwrdet_always_on_ops = {
	.set = pwrdet_always_on_set,
	.get = pwrdet_always_on_get,
};
static struct kernel_param_ops pwrio_always_on_ops = {
	.set = pwrio_always_on_set,
	.get = pwrdet_always_on_get,
};
module_param_cb(pwrdet_always_on, &pwrdet_always_on_ops,
	&pwrdet_always_on, 0644);
module_param_cb(pwrio_always_on, &pwrio_always_on_ops,
	&pwrio_always_on, 0644);

static int pwrdet_val_get(char *buffer, const struct kernel_param *kp)
{
	pwrdet_val = pmc_readl(PMC_PWR_DET_VAL);
	return param_get_ulong(buffer, kp);
}
static struct kernel_param_ops pwrdet_val_ops = {
	.get = pwrdet_val_get,
};
module_param_cb(pwrdet_val, &pwrdet_val_ops, &pwrdet_val, 0444);

static int pwrio_val_get(char *buffer, const struct kernel_param *kp)
{
	pwrio_val = pmc_readl(PMC_PWR_IO_DISABLE);
	return param_get_ulong(buffer, kp);
}
static struct kernel_param_ops pwrio_val_ops = {
	.get = pwrio_val_get,
};
module_param_cb(pwrio_val, &pwrio_val_ops, &pwrio_val, 0444);


static int pwrdet_notify_cb(
	struct notifier_block *nb, unsigned long event, void *v)
{
	unsigned long flags;
	struct pwr_detect_cell *cell;

	if (!pwrdet_rails_found)
		return NOTIFY_OK;

	cell = container_of(nb, struct pwr_detect_cell, regulator_nb);

	spin_lock_irqsave(&pwr_lock, flags);

	if (event & REGULATOR_EVENT_PRE_ENABLE) {
		pwrio_disabled_mask &= ~cell->pwrio_mask;
		if (!pwrio_always_on)
			pwr_io_enable(cell->pwrio_mask);
	}
	if (event & (REGULATOR_EVENT_PRE_ENABLE |
		     REGULATOR_EVENT_OUT_PRECHANGE)) {
		if (!pwrdet_always_on && cell->pwrdet_mask)
			pwr_detect_reset(cell->pwrdet_mask);
	}
	if (event & (REGULATOR_EVENT_POST_ENABLE |
		     REGULATOR_EVENT_OUT_POSTCHANGE)) {
		if (!pwrdet_always_on && cell->pwrdet_mask) {
			pwr_detect_start(cell->pwrdet_mask);
			pwr_detect_latch();
		}
	}
	if (event & (REGULATOR_EVENT_DISABLE |
		     REGULATOR_EVENT_FORCE_DISABLE)) {
		pwrio_disabled_mask |= cell->pwrio_mask;
		if (!pwrio_always_on)
			pwr_io_disable(cell->pwrio_mask);
	}

	pr_debug("tegra: %s: event %lu, pwrdet 0x%x, pwrio 0x%x\n",
		cell->reg_id, event,
		pmc_readl(PMC_PWR_DET_VAL), pmc_readl(PMC_PWR_IO_DISABLE));
	spin_unlock_irqrestore(&pwr_lock, flags);

	return NOTIFY_OK;
}

static int __init pwr_detect_cell_init_one(
	struct pwr_detect_cell *cell, u32 *disabled_mask)
{
	int ret;
	struct regulator *regulator = regulator_get(NULL, cell->reg_id);

	if (IS_ERR(regulator))
		return PTR_ERR(regulator);

	cell->regulator_nb.notifier_call = pwrdet_notify_cb;
	ret = regulator_register_notifier(regulator, &cell->regulator_nb);
	if (ret) {
		regulator_put(regulator);
		return ret;
	}

	if (!regulator_is_enabled(regulator))
		*disabled_mask |= cell->pwrio_mask;

	regulator_put(regulator);
	return 0;
}

int __init tegra_pwr_detect_cell_init(void)
{
	int i, ret;
	u32 package_mask;
	unsigned long flags;
	bool rails_found = true;

	if (!tegra_platform_is_silicon())
		return -ENOSYS;

	i = tegra_package_id();
	if ((i != -1) && (i & (~0x1F))) {
		pr_err("tegra: not supported package id %d - io power detection"
		       " is left always on\n", i);
		return 0;
	}
	package_mask = (i == -1) ? i : (0x1 << i);

	for (i = 0; i < ARRAY_SIZE(pwr_detect_cells); i++) {
		struct pwr_detect_cell *cell = &pwr_detect_cells[i];

		if (!(cell->package_mask & package_mask)) {
			pwrio_disabled_mask |= cell->pwrio_mask;
			continue;
		}

		ret = pwr_detect_cell_init_one(cell, &pwrio_disabled_mask);
		if (ret) {
			pr_err("tegra: failed to map regulator to power detect"
			       " cell %s(%d)\n", cell->reg_id, ret);
			rails_found = false;
		}
	}

	if (!rails_found) {
		pr_err("tegra: failed regulators mapping - io power detection"
		       " is left always on\n");
		return 0;
	}
	pwrdet_rails_found = true;

	/* Latch initial i/o power levels, disable all detection cells
	   and not powered interfaces */
	spin_lock_irqsave(&pwr_lock, flags);
	if (!pwrdet_always_on)
		pwr_detect_latch();
	if (!pwrio_always_on)
		pwr_io_disable(pwrio_disabled_mask);
	spin_unlock_irqrestore(&pwr_lock, flags);

	pr_info("tegra: started io power detection dynamic control\n");
	pr_info("tegra: NO_IO_POWER setting 0x%x\n", pwrio_disabled_mask);

	return 0;
}

fs_initcall(tegra_pwr_detect_cell_init);
