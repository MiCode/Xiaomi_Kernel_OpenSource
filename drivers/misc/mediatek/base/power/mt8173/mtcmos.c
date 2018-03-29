/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/soc/mediatek/infracfg.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <mt_chip.h>
#include "mt_dcm.h"
#include "mtcmos.h"

static struct regmap *regmap_spm;
static struct regmap *regmap_mcucfg;
static struct regmap *regmap_infracfg;

static DEFINE_SPINLOCK(spm_cpu_lock);

int __init mt_spm_mtcmos_init(void)
{
	static int init;
	struct device_node *spm_node;
	struct device_node *infracfg_node;
	struct device_node *mcucfg_node;

	if (init)
		return 0;

	spm_node = of_find_compatible_node(NULL, NULL,
				"mediatek,mt8173-scpsys");
	if (!spm_node) {
		pr_err("Cannot found:%s!\n", spm_node->full_name);
		return -1;
	}

	infracfg_node = of_find_compatible_node(NULL, NULL,
				"mediatek,mt8173-infracfg");
	if (!infracfg_node) {
		pr_err("Cannot found:%s!\n", infracfg_node->full_name);
		return -1;
	}

	mcucfg_node = of_find_compatible_node(NULL, NULL,
				"mediatek,mt8173-mcucfg");
	if (!mcucfg_node) {
		pr_err("Cannot found:%s!\n", mcucfg_node->full_name);
		return -1;
	}

	regmap_spm = syscon_node_to_regmap(spm_node);
	if (IS_ERR(regmap_spm)) {
		pr_err("Cannot find regmap %s: %ld.\n",
			    spm_node->full_name,
			PTR_ERR(regmap_spm));
		return PTR_ERR(regmap_spm);
	}

	regmap_infracfg = syscon_node_to_regmap(infracfg_node);
	if (IS_ERR(regmap_infracfg)) {
		pr_err("Cannot find regmap %s: %ld.\n",
			    infracfg_node->full_name,
			PTR_ERR(regmap_infracfg));
		return PTR_ERR(regmap_infracfg);
	}

	regmap_mcucfg = syscon_node_to_regmap(mcucfg_node);
	if (IS_ERR(regmap_mcucfg)) {
		pr_err("Cannot find regmap %s: %ld.\n",
			    mcucfg_node->full_name,
			PTR_ERR(regmap_mcucfg));
		return PTR_ERR(regmap_mcucfg);
	}

	init = 1;

	return 0;
}

static unsigned int mtcmos_read(struct regmap *regmap, unsigned int off)
{
	unsigned int val = 0;

	regmap_read(regmap, off, &val);
	return val;
}
static void mtcmos_write(struct regmap *regmap, unsigned int off,
				unsigned int val)
{
	regmap_write(regmap, off, val);
}

#define spm_read(reg)			mtcmos_read(regmap_spm, reg)
#define spm_write(reg, val)		mtcmos_write(regmap_spm, reg, val)
#define mcucfg_read(reg)		mtcmos_read(regmap_mcucfg, reg)
#define mcucfg_write(reg, val)		mtcmos_write(regmap_mcucfg, reg, val)
#define infracfg_read(reg)		mtcmos_read(regmap_infracfg, reg)
#define infracfg_write(reg, val)	mtcmos_write(regmap_infracfg, reg, val)

void spm_mtcmos_cpu_lock(unsigned long *flags)
{
	spin_lock_irqsave(&spm_cpu_lock, *flags);
}

void spm_mtcmos_cpu_unlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&spm_cpu_lock, *flags);
}

typedef int (*spm_cpu_mtcmos_ctrl_func) (int state, int chkWfiBeforePdn);
static spm_cpu_mtcmos_ctrl_func spm_cpu_mtcmos_ctrl_funcs[] = {
	spm_mtcmos_ctrl_cpu0,
	spm_mtcmos_ctrl_cpu1,
	spm_mtcmos_ctrl_cpu2,
	spm_mtcmos_ctrl_cpu3,
	spm_mtcmos_ctrl_cpu4,
	spm_mtcmos_ctrl_cpu5,
	spm_mtcmos_ctrl_cpu6,
	spm_mtcmos_ctrl_cpu7
};

__init int spm_mtcmos_cpu_init(void)
{
	unsigned int i, cpu_id;
	u64 hwid;

	mt_spm_mtcmos_init();

	pr_debug("CPU num: %d\n", num_possible_cpus());

	for (i = 1; i < num_possible_cpus(); i++) {
		hwid = cpu_logical_map(i);
		cpu_id = MPIDR_AFFINITY_LEVEL(hwid, 0) + ((MPIDR_AFFINITY_LEVEL(hwid, 1)) << 2);
		spm_cpu_mtcmos_ctrl_funcs[i] = spm_cpu_mtcmos_ctrl_funcs[cpu_id];
	}

	return 0;
}


int spm_mtcmos_ctrl_cpu(unsigned int cpu, int state, int chkWfiBeforePdn)
{
	return (*spm_cpu_mtcmos_ctrl_funcs[cpu]) (state, chkWfiBeforePdn);
}

int spm_mtcmos_ctrl_cpu0(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) &
					CA7_CPU0_STANDBYWFI) == 0)
				;

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU0_L1_PDN,
			spm_read(SPM_CA7_CPU0_L1_PDN) | L1_PDN);

		while ((spm_read(SPM_CA7_CPU0_L1_PDN) & L1_PDN_ACK)
				!= L1_PDN_ACK)
			;

		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_RST_B);
		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_CLK_DIS);

		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU0) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU0) != 0))
			;

		spm_mtcmos_cpu_unlock(&flags);
	} else {		/* STA_POWER_ON */

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU0) !=
					CA7_CPU0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU0) !=
					CA7_CPU0))
			;

		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA7_CPU0_L1_PDN,
			spm_read(SPM_CA7_CPU0_L1_PDN) & ~L1_PDN);

		while ((spm_read(SPM_CA7_CPU0_L1_PDN) & L1_PDN_ACK) != 0)
			;

		udelay(1);
		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) | SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA7_CPU0_PWR_CON,
			spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu1(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET,
		(SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) &
					CA7_CPU1_STANDBYWFI) == 0)
				;

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU1_L1_PDN,
			spm_read(SPM_CA7_CPU1_L1_PDN) | L1_PDN);

		while ((spm_read(SPM_CA7_CPU1_L1_PDN) & L1_PDN_ACK) !=
				L1_PDN_ACK)
			;

		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_RST_B);
		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_CLK_DIS);

		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU1) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU1) != 0))
			;

		spm_mtcmos_cpu_unlock(&flags);
	} else {

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU1) != CA7_CPU1)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU1) !=
					CA7_CPU1))
			;

		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA7_CPU1_L1_PDN,
			spm_read(SPM_CA7_CPU1_L1_PDN) & ~L1_PDN);

		while ((spm_read(SPM_CA7_CPU1_L1_PDN) & L1_PDN_ACK) != 0)
			;

		udelay(1);
		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) | SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA7_CPU1_PWR_CON,
			spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu2(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET,
		(SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) &
					CA7_CPU2_STANDBYWFI) == 0)
				;

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU2_L1_PDN,
			spm_read(SPM_CA7_CPU2_L1_PDN) | L1_PDN);

		while ((spm_read(SPM_CA7_CPU2_L1_PDN) & L1_PDN_ACK) !=
				L1_PDN_ACK)
			;

		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_RST_B);
		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_CLK_DIS);

		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU2) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU2) != 0))
			;

		spm_mtcmos_cpu_unlock(&flags);
	} else {		/* STA_POWER_ON */

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU2) != CA7_CPU2)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU2) !=
					CA7_CPU2))
			;

		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA7_CPU2_L1_PDN,
			spm_read(SPM_CA7_CPU2_L1_PDN) & ~L1_PDN);

		while ((spm_read(SPM_CA7_CPU2_L1_PDN) & L1_PDN_ACK) != 0)
			;

		udelay(1);
		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) | SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA7_CPU2_PWR_CON,
			spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu3(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET,
		(SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) &
					CA7_CPU3_STANDBYWFI) == 0)
				;

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU3_L1_PDN,
			spm_read(SPM_CA7_CPU3_L1_PDN) | L1_PDN);

		while ((spm_read(SPM_CA7_CPU3_L1_PDN) & L1_PDN_ACK) !=
				L1_PDN_ACK)
			;

		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_RST_B);
		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_CLK_DIS);

		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU3) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU3) != 0))
			;

		spm_mtcmos_cpu_unlock(&flags);
	} else {

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPU3) != CA7_CPU3)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU3) !=
				   CA7_CPU3))
			;

		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA7_CPU3_L1_PDN,
			spm_read(SPM_CA7_CPU3_L1_PDN) & ~L1_PDN);

		while ((spm_read(SPM_CA7_CPU3_L1_PDN) & L1_PDN_ACK) != 0)
			;

		udelay(1);
		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) | SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA7_CPU3_PWR_CON,
			spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu4(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET,
		(SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) &
					CA15_CPU0_STANDBYWFI) == 0)
				;

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPU0_PWR_CON,
			spm_read(SPM_CA15_CPU0_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA15_L1_PWR_CON,
			spm_read(SPM_CA15_L1_PWR_CON) | CPU0_CA15_L1_PDN);

		while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU0_CA15_L1_PDN_ACK) !=
		       CPU0_CA15_L1_PDN_ACK)
			;

		spm_write(SPM_CA15_CPU0_PWR_CON,
			spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA15_CPU0_PWR_CON,
			spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA15_CPU0_PWR_CON,
			spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA15_CPU0) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU0) != 0))
			;

		spm_write(SPM_CA15_CPU0_PWR_CON,
			spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);

	if (!(spm_read(SPM_PWR_STATUS) &
			(CA15_CPU1 | CA15_CPU2 | CA15_CPU3)) &&
		!(spm_read(SPM_PWR_STATUS_2ND) &
			(CA15_CPU1 | CA15_CPU2 | CA15_CPU3)))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
	} else {

		if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) &&
		    !(spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPU0_PWR_CON,
			spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_ON);

		while ((spm_read(SPM_PWR_STATUS) & CA15_CPU0) != CA15_CPU0)
			;

		spm_write(SPM_CA15_CPU0_PWR_CON,
			spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_ON_2ND);

		while ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU0) != CA15_CPU0)
			;

		spm_write(SPM_CA15_L1_PWR_CON,
			spm_read(SPM_CA15_L1_PWR_CON) & ~CPU0_CA15_L1_PDN);

		while ((spm_read(SPM_CA15_L1_PWR_CON) &
				CPU0_CA15_L1_PDN_ACK) != 0)
			;

		spm_write(SPM_CA15_CPU0_PWR_CON,
			spm_read(SPM_CA15_CPU0_PWR_CON) & ~SRAM_CKISO);
		spm_write(SPM_CA15_CPU0_PWR_CON,
			spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA15_CPU0_PWR_CON,
			spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu5(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET,
		(SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) &
					CA15_CPU1_STANDBYWFI) == 0)
				;

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPU1_PWR_CON,
			spm_read(SPM_CA15_CPU1_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA15_L1_PWR_CON,
			spm_read(SPM_CA15_L1_PWR_CON) | CPU1_CA15_L1_PDN);

		while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU1_CA15_L1_PDN_ACK) !=
		       CPU1_CA15_L1_PDN_ACK)
			;

		spm_write(SPM_CA15_CPU1_PWR_CON,
			spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA15_CPU1_PWR_CON,
			spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA15_CPU1_PWR_CON,
			spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA15_CPU1) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU1) != 0))
			;

		spm_write(SPM_CA15_CPU1_PWR_CON,
			spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);

	if (!(spm_read(SPM_PWR_STATUS) &
			(CA15_CPU0 | CA15_CPU2 | CA15_CPU3)) &&
		!(spm_read(SPM_PWR_STATUS_2ND) &
			(CA15_CPU0 | CA15_CPU2 | CA15_CPU3)))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
	} else {

		if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) &&
		    !(spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPU1_PWR_CON,
			spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_ON);

		while ((spm_read(SPM_PWR_STATUS) & CA15_CPU1) != CA15_CPU1)
			;

		spm_write(SPM_CA15_CPU1_PWR_CON,
			spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_ON_2ND);

		while ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU1) != CA15_CPU1)
			;

		spm_write(SPM_CA15_L1_PWR_CON,
			spm_read(SPM_CA15_L1_PWR_CON) & ~CPU1_CA15_L1_PDN);

		while ((spm_read(SPM_CA15_L1_PWR_CON) &
				CPU1_CA15_L1_PDN_ACK) != 0)
			;

		spm_write(SPM_CA15_CPU1_PWR_CON,
			spm_read(SPM_CA15_CPU1_PWR_CON) & ~SRAM_CKISO);
		spm_write(SPM_CA15_CPU1_PWR_CON,
			spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA15_CPU1_PWR_CON,
			spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu6(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET,
		(SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) &
					CA15_CPU2_STANDBYWFI) == 0)
				;

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPU2_PWR_CON,
			spm_read(SPM_CA15_CPU2_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA15_L1_PWR_CON,
			spm_read(SPM_CA15_L1_PWR_CON) | CPU2_CA15_L1_PDN);

		while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU2_CA15_L1_PDN_ACK) !=
		       CPU2_CA15_L1_PDN_ACK)
			;

		spm_write(SPM_CA15_CPU2_PWR_CON,
			spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA15_CPU2_PWR_CON,
			spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA15_CPU2_PWR_CON,
			spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA15_CPU2) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU2) != 0))
			;

		spm_write(SPM_CA15_CPU2_PWR_CON,
			spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);

	if (!(spm_read(SPM_PWR_STATUS) &
			(CA15_CPU2 | CA15_CPU1 | CA15_CPU3)) &&
		!(spm_read(SPM_PWR_STATUS_2ND) &
			(CA15_CPU2 | CA15_CPU1 | CA15_CPU3)))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
	} else {

		if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) &&
		    !(spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPU2_PWR_CON,
			spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_ON);

		while ((spm_read(SPM_PWR_STATUS) & CA15_CPU2) != CA15_CPU2)
			;

		spm_write(SPM_CA15_CPU2_PWR_CON,
			spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_ON_2ND);

		while ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU2) != CA15_CPU2)
			;

		spm_write(SPM_CA15_L1_PWR_CON,
			spm_read(SPM_CA15_L1_PWR_CON) & ~CPU2_CA15_L1_PDN);

		while ((spm_read(SPM_CA15_L1_PWR_CON) &
				CPU2_CA15_L1_PDN_ACK) != 0)
			;

		spm_write(SPM_CA15_CPU2_PWR_CON,
			spm_read(SPM_CA15_CPU2_PWR_CON) & ~SRAM_CKISO);
		spm_write(SPM_CA15_CPU2_PWR_CON,
			spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA15_CPU2_PWR_CON,
			spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpu7(int state, int chkWfiBeforePdn)
{
	unsigned long flags;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET,
		(SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) &
					CA15_CPU3_STANDBYWFI) == 0)
				;

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPU3_PWR_CON,
			spm_read(SPM_CA15_CPU3_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA15_L1_PWR_CON,
			spm_read(SPM_CA15_L1_PWR_CON) | CPU3_CA15_L1_PDN);

		while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU3_CA15_L1_PDN_ACK) !=
		       CPU3_CA15_L1_PDN_ACK)
			;

		spm_write(SPM_CA15_CPU3_PWR_CON,
			spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA15_CPU3_PWR_CON,
			spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA15_CPU3_PWR_CON,
			spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA15_CPU3) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU3) != 0))
			;

		spm_write(SPM_CA15_CPU3_PWR_CON,
			spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);

	if (!(spm_read(SPM_PWR_STATUS) &
			(CA15_CPU3 | CA15_CPU1 | CA15_CPU2)) &&
		!(spm_read(SPM_PWR_STATUS_2ND) &
			(CA15_CPU3 | CA15_CPU1 | CA15_CPU2)))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
	} else {

		if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) &&
		    !(spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP))
			spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPU3_PWR_CON,
			spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_ON);

		while ((spm_read(SPM_PWR_STATUS) & CA15_CPU3) != CA15_CPU3)
			;

		spm_write(SPM_CA15_CPU3_PWR_CON,
			spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_ON_2ND);

		while ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU3) != CA15_CPU3)
			;

		spm_write(SPM_CA15_L1_PWR_CON,
			spm_read(SPM_CA15_L1_PWR_CON) & ~CPU3_CA15_L1_PDN);

		while ((spm_read(SPM_CA15_L1_PWR_CON) &
				CPU3_CA15_L1_PDN_ACK) != 0)
			;

		spm_write(SPM_CA15_CPU3_PWR_CON,
			spm_read(SPM_CA15_CPU3_PWR_CON) & ~SRAM_CKISO);
		spm_write(SPM_CA15_CPU3_PWR_CON,
			spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA15_CPU3_PWR_CON,
			spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpusys0(int state, int chkWfiBeforePdn)
{
	unsigned long flags;
	int ret;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET,
		(SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {

		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) &
					CA7_CPUTOP_STANDBYWFI) == 0)
				;

		ret = mtk_infracfg_set_bus_protection1(regmap_infracfg, MT8173_TOP_AXI_PROT_EN1_L2C_SRAM);
		if (ret)
			pr_err("%s():%d\n", __func__, __LINE__);

		mtk_infracfg_set_bus_protection(regmap_infracfg, MT8173_TOP_AXI_PROT_EN_CA7_ADB);
		if (ret)
			pr_err("%s():%d\n", __func__, __LINE__);

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~SRAM_ISOINT_B);
		spm_write(SPM_CA7_CPUTOP_L2_PDN,
			spm_read(SPM_CA7_CPUTOP_L2_PDN) | L2_SRAM_PDN);

		while ((spm_read(SPM_CA7_CPUTOP_L2_PDN) & L2_SRAM_PDN_ACK) !=
					L2_SRAM_PDN_ACK)
			;
		ndelay(1500);

		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_RST_B);
		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_CLK_DIS);

		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPUTOP) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPUTOP) != 0))
			;

		spm_mtcmos_cpu_unlock(&flags);
	} else {

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_ON);
		udelay(1);
		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_ON_2ND);

		while (((spm_read(SPM_PWR_STATUS) & CA7_CPUTOP) != CA7_CPUTOP)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPUTOP) !=
					CA7_CPUTOP))
			;

		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA7_CPUTOP_L2_PDN,
			spm_read(SPM_CA7_CPUTOP_L2_PDN) & ~L2_SRAM_PDN);

		while ((spm_read(SPM_CA7_CPUTOP_L2_PDN) & L2_SRAM_PDN_ACK) != 0)
			;

		ndelay(900);
		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) | SRAM_ISOINT_B);
		ndelay(100);
		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~SRAM_CKISO);

		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_CLK_DIS);
		spm_write(SPM_CA7_CPUTOP_PWR_CON,
			spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);

		ret = mtk_infracfg_clear_bus_protection1(regmap_infracfg, MT8173_TOP_AXI_PROT_EN1_L2C_SRAM);
		if (ret)
			pr_err("%s():%d\n", __func__, __LINE__);

		ret = mtk_infracfg_clear_bus_protection(regmap_infracfg, MT8173_TOP_AXI_PROT_EN_CA7_ADB);
		if (ret)
			pr_err("%s():%d\n", __func__, __LINE__);
	}

	return 0;
}

int spm_mtcmos_ctrl_cpusys1(int state, int chkWfiBeforePdn)
{
	unsigned long flags;
	int ret;

	/* enable register control */
	spm_write(SPM_POWERON_CONFIG_SET,
		(SPM_PROJECT_CODE << 16) | (1U << 0));

	if (state == STA_POWER_DOWN) {
		/* turn_off_SPARK("spm_mtcmos_ctrl_cpsys1-----0"); */

		/* assert ACINCATM before wait for WFIL2 */
		mcucfg_write(CA15L_MISCDBG,
			mcucfg_read(CA15L_MISCDBG) | CA15L_ACINACTM);

		if (chkWfiBeforePdn)
			while ((spm_read(SPM_SLEEP_TIMER_STA) &
					CA15_CPUTOP_STANDBYWFI) == 0)
				;

		ret = mtk_infracfg_set_bus_protection(regmap_infracfg, MT8173_TOP_AXI_PROT_EN_CA15_ADB);
		if (ret)
			pr_err("%s():%d\n", __func__, __LINE__);

		spm_mtcmos_cpu_lock(&flags);

		/* turn_off_FBB(); */

		spm_write(SPM_CA15_CPUTOP_PWR_CON,
			spm_read(SPM_CA15_CPUTOP_PWR_CON) | SRAM_CKISO);
		spm_write(SPM_CA15_CPUTOP_PWR_CON,
			spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~SRAM_ISOINT_B);

		spm_write(SPM_CA15_L2_PWR_CON,
			spm_read(SPM_CA15_L2_PWR_CON) | CA15_L2_PDN);
		while ((spm_read(SPM_CA15_L2_PWR_CON) & CA15_L2_PDN_ACK) !=
				CA15_L2_PDN_ACK)
			;

		spm_write(SPM_CA15_CPUTOP_PWR_CON,
			spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_RST_B);
		spm_write(SPM_CA15_CPUTOP_PWR_CON,
			spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_CLK_DIS);
		spm_write(SPM_CA15_CPUTOP_PWR_CON,
			spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_ISO);

		spm_write(SPM_CA15_CPUTOP_PWR_CON,
			spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_ON);
		spm_write(SPM_CA15_CPUTOP_PWR_CON,
			spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_ON_2ND);
		while (((spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) != 0)
		       || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP) != 0))
			;

		spm_mtcmos_cpu_unlock(&flags);

		if ((spm_read(SPM_SLEEP_DUAL_VCORE_PWR_CON) &
				VCA15_PWR_ISO) == 0) {

			/* enable dcm for low power */
			if (CHIP_SW_VER_01 == mt_get_chip_sw_ver())
				enable_cpu_dcm();

			spm_write(SPM_SLEEP_DUAL_VCORE_PWR_CON,
				  spm_read(SPM_SLEEP_DUAL_VCORE_PWR_CON) |
					VCA15_PWR_ISO);
		}
	} else {

		if ((spm_read(SPM_SLEEP_DUAL_VCORE_PWR_CON) & VCA15_PWR_ISO) ==
				VCA15_PWR_ISO) {

			spm_write(SPM_SLEEP_DUAL_VCORE_PWR_CON,
				  spm_read(SPM_SLEEP_DUAL_VCORE_PWR_CON) &
					~VCA15_PWR_ISO);

			/* disable dcm for performance */
			if (CHIP_SW_VER_01 == mt_get_chip_sw_ver())
				disable_cpu_dcm();
		}

		spm_mtcmos_cpu_lock(&flags);

		spm_write(SPM_CA15_CPUTOP_PWR_CON,
			spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_ON);
		while ((spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) != CA15_CPUTOP)
			;

		spm_write(SPM_CA15_CPUTOP_PWR_CON,
			spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_ON_2ND);
		while ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP) !=
				CA15_CPUTOP)
			;

		spm_write(SPM_CA15_CPUTOP_PWR_CON,
			  spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_CLK_DIS);

		spm_write(SPM_CA15_L2_PWR_CON,
			spm_read(SPM_CA15_L2_PWR_CON) & ~CA15_L2_PDN);
		while ((spm_read(SPM_CA15_L2_PWR_CON) & CA15_L2_PDN_ACK) != 0)
			;

		spm_write(SPM_CA15_CPUTOP_PWR_CON,
			  spm_read(SPM_CA15_CPUTOP_PWR_CON) | SRAM_ISOINT_B);

		spm_write(SPM_CA15_CPUTOP_PWR_CON,
			spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~SRAM_CKISO);

		/* ptp2_pre_init(); */

		spm_write(SPM_CA15_CPUTOP_PWR_CON,
			spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_ISO);

		spm_write(SPM_CA15_CPUTOP_PWR_CON,
			spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_RST_B);

		spm_mtcmos_cpu_unlock(&flags);

		ret = mtk_infracfg_clear_bus_protection(regmap_infracfg, MT8173_TOP_AXI_PROT_EN_CA15_ADB);
		if (ret)
			pr_err("%s():%d\n", __func__, __LINE__);

		mcucfg_write(CA15L_MISCDBG, mcucfg_read(CA15L_MISCDBG) & ~CA15L_ACINACTM);
	}

	return 0;

}

void spm_mtcmos_ctrl_cpusys1_init_1st_bring_up(int state)
{

	if (state == STA_POWER_DOWN) {
		spm_mtcmos_ctrl_cpu7(STA_POWER_DOWN, 0);
		spm_mtcmos_ctrl_cpu6(STA_POWER_DOWN, 0);
		spm_mtcmos_ctrl_cpu5(STA_POWER_DOWN, 0);
		spm_mtcmos_ctrl_cpu4(STA_POWER_DOWN, 0);
	} else {	/* STA_POWER_ON */

		spm_mtcmos_ctrl_cpu4(STA_POWER_ON, 1);
		spm_mtcmos_ctrl_cpu5(STA_POWER_ON, 1);
		spm_mtcmos_ctrl_cpu6(STA_POWER_ON, 1);
		spm_mtcmos_ctrl_cpu7(STA_POWER_ON, 1);
	}
}

bool spm_cpusys0_can_power_down(void)
{
	return !(spm_read(SPM_PWR_STATUS) &
		 (CA15_CPU0 | CA15_CPU1 | CA15_CPU2 | CA15_CPU3
		  | CA15_CPUTOP | CA7_CPU1 | CA7_CPU2
		  | CA7_CPU3))
	    && !(spm_read(SPM_PWR_STATUS_2ND) &
		 (CA15_CPU0 | CA15_CPU1 | CA15_CPU2 | CA15_CPU3
		  | CA15_CPUTOP | CA7_CPU1 | CA7_CPU2
		  | CA7_CPU3));
}

bool spm_cpusys1_can_power_down(void)
{
	return !(spm_read(SPM_PWR_STATUS) &
		 (CA7_CPU0 | CA7_CPU1 | CA7_CPU2 | CA7_CPU3
		  | CA7_CPUTOP | CA15_CPU1 | CA15_CPU2 |
		  CA15_CPU3))
	    && !(spm_read(SPM_PWR_STATUS_2ND) &
		 (CA7_CPU0 | CA7_CPU1 | CA7_CPU2 | CA7_CPU3
		  | CA7_CPUTOP | CA15_CPU1 | CA15_CPU2 |
		  CA15_CPU3));
}
