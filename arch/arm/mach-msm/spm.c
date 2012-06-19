/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <mach/msm_iomap.h>

#include "spm.h"


enum {
	MSM_SPM_DEBUG_SHADOW = 1U << 0,
	MSM_SPM_DEBUG_VCTL = 1U << 1,
};

static int msm_spm_debug_mask;
module_param_named(
	debug_mask, msm_spm_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);

#define MSM_SPM_PMIC_STATE_IDLE  0

static uint32_t msm_spm_reg_offsets[MSM_SPM_REG_NR] = {
	[MSM_SPM_REG_SAW_AVS_CTL] = 0x04,

	[MSM_SPM_REG_SAW_VCTL] = 0x08,
	[MSM_SPM_REG_SAW_STS] = 0x0C,
	[MSM_SPM_REG_SAW_CFG] = 0x10,

	[MSM_SPM_REG_SAW_SPM_CTL] = 0x14,
	[MSM_SPM_REG_SAW_SPM_SLP_TMR_DLY] = 0x18,
	[MSM_SPM_REG_SAW_SPM_WAKE_TMR_DLY] = 0x1C,

	[MSM_SPM_REG_SAW_SPM_PMIC_CTL] = 0x20,
	[MSM_SPM_REG_SAW_SLP_CLK_EN] = 0x24,
	[MSM_SPM_REG_SAW_SLP_HSFS_PRECLMP_EN] = 0x28,
	[MSM_SPM_REG_SAW_SLP_HSFS_POSTCLMP_EN] = 0x2C,

	[MSM_SPM_REG_SAW_SLP_CLMP_EN] = 0x30,
	[MSM_SPM_REG_SAW_SLP_RST_EN] = 0x34,
	[MSM_SPM_REG_SAW_SPM_MPM_CFG] = 0x38,
};

struct msm_spm_device {
	void __iomem *reg_base_addr;
	uint32_t reg_shadow[MSM_SPM_REG_NR];

	uint8_t awake_vlevel;
	uint8_t retention_vlevel;
	uint8_t collapse_vlevel;
	uint8_t retention_mid_vlevel;
	uint8_t collapse_mid_vlevel;

	uint32_t vctl_timeout_us;

	unsigned int low_power_mode;
	bool notify_rpm;
	bool dirty;
};

static DEFINE_PER_CPU_SHARED_ALIGNED(struct msm_spm_device, msm_spm_devices);
/******************************************************************************
 * Internal helper functions
 *****************************************************************************/

static inline void msm_spm_set_vctl(
	struct msm_spm_device *dev, uint32_t vlevel)
{
	dev->reg_shadow[MSM_SPM_REG_SAW_VCTL] &= ~0xFF;
	dev->reg_shadow[MSM_SPM_REG_SAW_VCTL] |= vlevel;
}

static inline void msm_spm_set_spm_ctl(struct msm_spm_device *dev,
	uint32_t rpm_bypass, uint32_t mode_encoding)
{
	dev->reg_shadow[MSM_SPM_REG_SAW_SPM_CTL] &= ~0x0F;
	dev->reg_shadow[MSM_SPM_REG_SAW_SPM_CTL] |= rpm_bypass << 3;
	dev->reg_shadow[MSM_SPM_REG_SAW_SPM_CTL] |= mode_encoding;
}

static inline void msm_spm_set_pmic_ctl(struct msm_spm_device *dev,
	uint32_t awake_vlevel, uint32_t mid_vlevel, uint32_t sleep_vlevel)
{
	dev->reg_shadow[MSM_SPM_REG_SAW_SPM_PMIC_CTL] =
		(mid_vlevel << 16) | (awake_vlevel << 8) | (sleep_vlevel);
}

static inline void msm_spm_set_slp_rst_en(
	struct msm_spm_device *dev, uint32_t slp_rst_en)
{
	dev->reg_shadow[MSM_SPM_REG_SAW_SLP_RST_EN] = slp_rst_en;
}

static inline void msm_spm_flush_shadow(
	struct msm_spm_device *dev, unsigned int reg_index)
{
	__raw_writel(dev->reg_shadow[reg_index],
		dev->reg_base_addr + msm_spm_reg_offsets[reg_index]);
}

static inline void msm_spm_load_shadow(
	struct msm_spm_device *dev, unsigned int reg_index)
{
	dev->reg_shadow[reg_index] = __raw_readl(dev->reg_base_addr +
					msm_spm_reg_offsets[reg_index]);
}

static inline uint32_t msm_spm_get_sts_pmic_state(struct msm_spm_device *dev)
{
	return (dev->reg_shadow[MSM_SPM_REG_SAW_STS] >> 20) & 0x03;
}

static inline uint32_t msm_spm_get_sts_curr_pmic_data(
	struct msm_spm_device *dev)
{
	return (dev->reg_shadow[MSM_SPM_REG_SAW_STS] >> 10) & 0xFF;
}

/******************************************************************************
 * Public functions
 *****************************************************************************/
int msm_spm_set_low_power_mode(unsigned int mode, bool notify_rpm)
{
	struct msm_spm_device *dev = &__get_cpu_var(msm_spm_devices);
	uint32_t rpm_bypass = notify_rpm ? 0x00 : 0x01;

	if (mode == dev->low_power_mode && notify_rpm == dev->notify_rpm
		&& !dev->dirty)
		return 0;

	switch (mode) {
	case MSM_SPM_MODE_CLOCK_GATING:
		msm_spm_set_spm_ctl(dev, rpm_bypass, 0x00);
		msm_spm_set_slp_rst_en(dev, 0x00);
		break;

	case MSM_SPM_MODE_POWER_RETENTION:
		msm_spm_set_spm_ctl(dev, rpm_bypass, 0x02);
		msm_spm_set_pmic_ctl(dev, dev->awake_vlevel,
			dev->retention_mid_vlevel, dev->retention_vlevel);
		msm_spm_set_slp_rst_en(dev, 0x00);
		break;

	case MSM_SPM_MODE_POWER_COLLAPSE:
		msm_spm_set_spm_ctl(dev, rpm_bypass, 0x02);
		msm_spm_set_pmic_ctl(dev, dev->awake_vlevel,
			dev->collapse_mid_vlevel, dev->collapse_vlevel);
		msm_spm_set_slp_rst_en(dev, 0x01);
		break;

	default:
		BUG();
	}

	msm_spm_flush_shadow(dev, MSM_SPM_REG_SAW_SPM_CTL);
	msm_spm_flush_shadow(dev, MSM_SPM_REG_SAW_SPM_PMIC_CTL);
	msm_spm_flush_shadow(dev, MSM_SPM_REG_SAW_SLP_RST_EN);
	/* Ensure that the registers are written before returning */
	mb();

	dev->low_power_mode = mode;
	dev->notify_rpm = notify_rpm;
	dev->dirty = false;

	if (msm_spm_debug_mask & MSM_SPM_DEBUG_SHADOW) {
		int i;
		for (i = 0; i < MSM_SPM_REG_NR; i++)
			pr_info("%s: reg %02x = 0x%08x\n", __func__,
				msm_spm_reg_offsets[i], dev->reg_shadow[i]);
	}

	return 0;
}

int msm_spm_set_vdd(unsigned int cpu, unsigned int vlevel)
{
	struct msm_spm_device *dev;
	uint32_t timeout_us;

	dev = &per_cpu(msm_spm_devices, cpu);

	if (msm_spm_debug_mask & MSM_SPM_DEBUG_VCTL)
		pr_info("%s: requesting cpu %u vlevel 0x%x\n",
			__func__, cpu, vlevel);

	msm_spm_set_vctl(dev, vlevel);
	msm_spm_flush_shadow(dev, MSM_SPM_REG_SAW_VCTL);

	/* Wait for PMIC state to return to idle or until timeout */
	timeout_us = dev->vctl_timeout_us;
	msm_spm_load_shadow(dev, MSM_SPM_REG_SAW_STS);
	while (msm_spm_get_sts_pmic_state(dev) != MSM_SPM_PMIC_STATE_IDLE) {
		if (!timeout_us)
			goto set_vdd_bail;

		if (timeout_us > 10) {
			udelay(10);
			timeout_us -= 10;
		} else {
			udelay(timeout_us);
			timeout_us = 0;
		}
		msm_spm_load_shadow(dev, MSM_SPM_REG_SAW_STS);
	}

	if (msm_spm_get_sts_curr_pmic_data(dev) != vlevel)
		goto set_vdd_bail;

	dev->awake_vlevel = vlevel;
	dev->dirty = true;

	if (msm_spm_debug_mask & MSM_SPM_DEBUG_VCTL)
		pr_info("%s: cpu %u done, remaining timeout %uus\n",
			__func__, cpu, timeout_us);

	return 0;

set_vdd_bail:
	pr_err("%s: cpu %u failed, remaining timeout %uus, vlevel 0x%x\n",
	       __func__, cpu, timeout_us, msm_spm_get_sts_curr_pmic_data(dev));

	return -EIO;
}

void msm_spm_reinit(void)
{
	struct msm_spm_device *dev = &__get_cpu_var(msm_spm_devices);
	int i;

	for (i = 0; i < MSM_SPM_REG_NR_INITIALIZE; i++)
		msm_spm_flush_shadow(dev, i);

	/* Ensure that the registers are written before returning */
	mb();
}

int __init msm_spm_init(struct msm_spm_platform_data *data, int nr_devs)
{
	unsigned int cpu;

	BUG_ON(nr_devs < num_possible_cpus());
	for_each_possible_cpu(cpu) {
		struct msm_spm_device *dev = &per_cpu(msm_spm_devices, cpu);
		int i;

		dev->reg_base_addr = data[cpu].reg_base_addr;
		memcpy(dev->reg_shadow, data[cpu].reg_init_values,
			sizeof(data[cpu].reg_init_values));

		dev->awake_vlevel = data[cpu].awake_vlevel;
		dev->retention_vlevel = data[cpu].retention_vlevel;
		dev->collapse_vlevel = data[cpu].collapse_vlevel;
		dev->retention_mid_vlevel = data[cpu].retention_mid_vlevel;
		dev->collapse_mid_vlevel = data[cpu].collapse_mid_vlevel;
		dev->vctl_timeout_us = data[cpu].vctl_timeout_us;

		for (i = 0; i < MSM_SPM_REG_NR_INITIALIZE; i++)
			msm_spm_flush_shadow(dev, i);

		/* Ensure that the registers are written before returning */
		mb();

		dev->low_power_mode = MSM_SPM_MODE_CLOCK_GATING;
		dev->notify_rpm = false;
		dev->dirty = true;
	}

	return 0;
}
