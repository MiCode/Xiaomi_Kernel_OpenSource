/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <linux/slab.h>
#include <mach/msm_iomap.h>

#include "spm_driver.h"

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
	[MSM_SPM_REG_SAW2_SECURE] = 0x00,

	[MSM_SPM_REG_SAW2_ID] = 0x04,
	[MSM_SPM_REG_SAW2_CFG] = 0x08,
	[MSM_SPM_REG_SAW2_STS0] = 0x0C,
	[MSM_SPM_REG_SAW2_STS1] = 0x10,

	[MSM_SPM_REG_SAW2_VCTL] = 0x14,

	[MSM_SPM_REG_SAW2_AVS_CTL] = 0x18,
	[MSM_SPM_REG_SAW2_AVS_HYSTERESIS] = 0x1C,

	[MSM_SPM_REG_SAW2_SPM_CTL] = 0x20,
	[MSM_SPM_REG_SAW2_PMIC_DLY] = 0x24,
	[MSM_SPM_REG_SAW2_PMIC_DATA_0] = 0x28,
	[MSM_SPM_REG_SAW2_PMIC_DATA_1] = 0x2C,
	[MSM_SPM_REG_SAW2_RST] = 0x30,

	[MSM_SPM_REG_SAW2_SEQ_ENTRY] = 0x80,
};

/******************************************************************************
 * Internal helper functions
 *****************************************************************************/

static inline void msm_spm_drv_set_vctl(struct msm_spm_driver_data *dev,
		uint32_t vlevel)
{
	dev->reg_shadow[MSM_SPM_REG_SAW2_VCTL] &= ~0xFF;
	dev->reg_shadow[MSM_SPM_REG_SAW2_VCTL] |= vlevel;

	dev->reg_shadow[MSM_SPM_REG_SAW2_PMIC_DATA_0] &= ~0xFF;
	dev->reg_shadow[MSM_SPM_REG_SAW2_PMIC_DATA_0] |= vlevel;
}

static void msm_spm_drv_flush_shadow(struct msm_spm_driver_data *dev,
		unsigned int reg_index)
{
	__raw_writel(dev->reg_shadow[reg_index],
		dev->reg_base_addr + msm_spm_reg_offsets[reg_index]);
}

static void msm_spm_drv_load_shadow(struct msm_spm_driver_data *dev,
		unsigned int reg_index)
{
	dev->reg_shadow[reg_index] =
		__raw_readl(dev->reg_base_addr +
				msm_spm_reg_offsets[reg_index]);
}

static inline uint32_t msm_spm_drv_get_awake_vlevel(
		struct msm_spm_driver_data *dev)
{
	return dev->reg_shadow[MSM_SPM_REG_SAW2_PMIC_DATA_0] & 0xFF;
}

static inline uint32_t msm_spm_drv_get_sts_pmic_state(
		struct msm_spm_driver_data *dev)
{
	return (dev->reg_shadow[MSM_SPM_REG_SAW2_STS0] >> 10) & 0x03;
}

static inline uint32_t msm_spm_drv_get_sts_curr_pmic_data(
		struct msm_spm_driver_data *dev)
{
	return dev->reg_shadow[MSM_SPM_REG_SAW2_STS1] & 0xFF;
}

static inline uint32_t msm_spm_drv_get_num_spm_entry(
		struct msm_spm_driver_data *dev)
{
	return 32;
}

static inline void msm_spm_drv_set_start_addr(
		struct msm_spm_driver_data *dev, uint32_t addr)
{
	addr &= 0x7F;
	addr <<= 4;
	dev->reg_shadow[MSM_SPM_REG_SAW2_SPM_CTL] &= 0xFFFFF80F;
	dev->reg_shadow[MSM_SPM_REG_SAW2_SPM_CTL] |= addr;
}


/******************************************************************************
 * Public functions
 *****************************************************************************/
inline int msm_spm_drv_set_spm_enable(
		struct msm_spm_driver_data *dev, bool enable)
{
	uint32_t value = enable ? 0x01 : 0x00;

	if (!dev)
		return -EINVAL;

	if ((dev->reg_shadow[MSM_SPM_REG_SAW2_SPM_CTL] & 0x01) ^ value) {

		dev->reg_shadow[MSM_SPM_REG_SAW2_SPM_CTL] &= ~0x1;
		dev->reg_shadow[MSM_SPM_REG_SAW2_SPM_CTL] |= value;

		msm_spm_drv_flush_shadow(dev, MSM_SPM_REG_SAW2_SPM_CTL);
		wmb();
	}
	return 0;
}
void msm_spm_drv_flush_seq_entry(struct msm_spm_driver_data *dev)
{
	int i;
	int num_spm_entry = msm_spm_drv_get_num_spm_entry(dev);

	if (!dev) {
		__WARN();
		return;
	}

	for (i = 0; i < num_spm_entry; i++) {
		__raw_writel(dev->reg_seq_entry_shadow[i],
			dev->reg_base_addr
			+ msm_spm_reg_offsets[MSM_SPM_REG_SAW2_SEQ_ENTRY]
			+ 4 * i);
	}
	mb();
}

int msm_spm_drv_write_seq_data(struct msm_spm_driver_data *dev,
		uint8_t *cmd, uint32_t offset)
{
	uint32_t offset_w = offset / 4;
	int ret = 0;

	if (!cmd || !dev) {
		__WARN();
		goto failed_write_seq_data;
	};

	while (1) {
		int i;
		uint32_t cmd_w = 0;
		uint8_t last_cmd = 0;

		for (i = 0; i < 4; i++) {
			last_cmd = (last_cmd == 0x0f) ? 0x0f : *(cmd + i);
			cmd_w |= last_cmd << (i * 8);
			ret++;
		}

		if (offset_w >=  msm_spm_drv_get_num_spm_entry(dev)) {
			__WARN();
			goto failed_write_seq_data;
		}

		cmd += i;
		dev->reg_seq_entry_shadow[offset_w++] = cmd_w;
		if (last_cmd == 0x0f)
			break;
	}
	return ret;

failed_write_seq_data:
	 return -EINVAL;
}

int msm_spm_drv_set_low_power_mode(struct msm_spm_driver_data *dev,
		uint32_t addr)
{

	/* SPM is configured to reset start address to zero after end of Program
	 */
	if (!dev)
		return -EINVAL;

	msm_spm_drv_set_start_addr(dev, addr);

	msm_spm_drv_flush_shadow(dev, MSM_SPM_REG_SAW2_SPM_CTL);
	wmb();

	if (msm_spm_debug_mask & MSM_SPM_DEBUG_SHADOW) {
		int i;
		for (i = 0; i < MSM_SPM_REG_NR; i++)
			pr_info("%s: reg %02x = 0x%08x\n", __func__,
				msm_spm_reg_offsets[i], dev->reg_shadow[i]);
	}

	return 0;
}

int msm_spm_drv_set_vdd(struct msm_spm_driver_data *dev, unsigned int vlevel)
{
	uint32_t timeout_us;

	if (!dev)
		return -EINVAL;

	if (msm_spm_debug_mask & MSM_SPM_DEBUG_VCTL)
		pr_info("%s: requesting vlevel 0x%x\n",
			__func__, vlevel);

	msm_spm_drv_set_vctl(dev, vlevel);
	msm_spm_drv_flush_shadow(dev, MSM_SPM_REG_SAW2_VCTL);
	msm_spm_drv_flush_shadow(dev, MSM_SPM_REG_SAW2_PMIC_DATA_0);
	mb();

	/* Wait for PMIC state to return to idle or until timeout */
	timeout_us = dev->vctl_timeout_us;
	msm_spm_drv_load_shadow(dev, MSM_SPM_REG_SAW2_STS0);
	while (msm_spm_drv_get_sts_pmic_state(dev) != MSM_SPM_PMIC_STATE_IDLE) {
		if (!timeout_us)
			goto set_vdd_bail;

		if (timeout_us > 10) {
			udelay(10);
			timeout_us -= 10;
		} else {
			udelay(timeout_us);
			timeout_us = 0;
		}
		msm_spm_drv_load_shadow(dev, MSM_SPM_REG_SAW2_STS0);
	}

	msm_spm_drv_load_shadow(dev, MSM_SPM_REG_SAW2_STS1);

	if (msm_spm_drv_get_sts_curr_pmic_data(dev) != vlevel)
		goto set_vdd_bail;

	if (msm_spm_debug_mask & MSM_SPM_DEBUG_VCTL)
		pr_info("%s: done, remaining timeout %uus\n",
			__func__, timeout_us);

	return 0;

set_vdd_bail:
	pr_err("%s: failed, remaining timeout %uus, vlevel 0x%x\n",
	       __func__, timeout_us, msm_spm_drv_get_sts_curr_pmic_data(dev));
	return -EIO;
}

int __init msm_spm_drv_init(struct msm_spm_driver_data *dev,
		struct msm_spm_platform_data *data)
{

	int i;
	int num_spm_entry;

	BUG_ON(!dev || !data);

	dev->reg_base_addr = data->reg_base_addr;
	memcpy(dev->reg_shadow, data->reg_init_values,
			sizeof(data->reg_init_values));

	dev->vctl_timeout_us = data->vctl_timeout_us;

	for (i = 0; i < MSM_SPM_REG_NR_INITIALIZE; i++)
		msm_spm_drv_flush_shadow(dev, i);
	/* barrier to ensure write completes before we update shadow
	 * registers
	 */
	mb();

	for (i = 0; i < MSM_SPM_REG_NR_INITIALIZE; i++)
		msm_spm_drv_load_shadow(dev, i);

	/* barrier to ensure read completes before we proceed further*/
	mb();

	num_spm_entry = msm_spm_drv_get_num_spm_entry(dev);

	dev->reg_seq_entry_shadow =
		kmalloc(sizeof(*dev->reg_seq_entry_shadow) * num_spm_entry,
				GFP_KERNEL);

	if (!dev->reg_seq_entry_shadow)
		return -ENOMEM;


	memset(dev->reg_seq_entry_shadow, 0x0f,
			num_spm_entry * sizeof(*dev->reg_seq_entry_shadow));

	return 0;
}
