/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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

#include "spm_driver.h"

#define MSM_SPM_PMIC_STATE_IDLE  0

enum {
	MSM_SPM_DEBUG_SHADOW = 1U << 0,
	MSM_SPM_DEBUG_VCTL = 1U << 1,
};

static int msm_spm_debug_mask;
module_param_named(
	debug_mask, msm_spm_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);

struct saw2_data {
	const char *ver_name;
	uint32_t major;
	uint32_t minor;
	uint32_t *spm_reg_offset_ptr;
};

static uint32_t msm_spm_reg_offsets_saw2_v2_1[MSM_SPM_REG_NR] = {
	[MSM_SPM_REG_SAW2_SECURE]		= 0x00,
	[MSM_SPM_REG_SAW2_ID]			= 0x04,
	[MSM_SPM_REG_SAW2_CFG]			= 0x08,
	[MSM_SPM_REG_SAW2_SPM_STS]		= 0x0C,
	[MSM_SPM_REG_SAW2_AVS_STS]		= 0x10,
	[MSM_SPM_REG_SAW2_PMIC_STS]		= 0x14,
	[MSM_SPM_REG_SAW2_RST]			= 0x18,
	[MSM_SPM_REG_SAW2_VCTL]			= 0x1C,
	[MSM_SPM_REG_SAW2_AVS_CTL]		= 0x20,
	[MSM_SPM_REG_SAW2_AVS_LIMIT]		= 0x24,
	[MSM_SPM_REG_SAW2_AVS_DLY]		= 0x28,
	[MSM_SPM_REG_SAW2_AVS_HYSTERESIS]	= 0x2C,
	[MSM_SPM_REG_SAW2_SPM_CTL]		= 0x30,
	[MSM_SPM_REG_SAW2_SPM_DLY]		= 0x34,
	[MSM_SPM_REG_SAW2_PMIC_DATA_0]		= 0x40,
	[MSM_SPM_REG_SAW2_PMIC_DATA_1]		= 0x44,
	[MSM_SPM_REG_SAW2_PMIC_DATA_2]		= 0x48,
	[MSM_SPM_REG_SAW2_PMIC_DATA_3]		= 0x4C,
	[MSM_SPM_REG_SAW2_PMIC_DATA_4]		= 0x50,
	[MSM_SPM_REG_SAW2_PMIC_DATA_5]		= 0x54,
	[MSM_SPM_REG_SAW2_PMIC_DATA_6]		= 0x58,
	[MSM_SPM_REG_SAW2_PMIC_DATA_7]		= 0x5C,
	[MSM_SPM_REG_SAW2_SEQ_ENTRY]		= 0x80,
	[MSM_SPM_REG_SAW2_VERSION]		= 0xFD0,
};

static uint32_t msm_spm_reg_offsets_saw2_v3_0[MSM_SPM_REG_NR] = {
	[MSM_SPM_REG_SAW2_SECURE]		= 0x00,
	[MSM_SPM_REG_SAW2_ID]			= 0x04,
	[MSM_SPM_REG_SAW2_CFG]			= 0x08,
	[MSM_SPM_REG_SAW2_SPM_STS]		= 0x0C,
	[MSM_SPM_REG_SAW2_AVS_STS]		= 0x10,
	[MSM_SPM_REG_SAW2_PMIC_STS]		= 0x14,
	[MSM_SPM_REG_SAW2_RST]			= 0x18,
	[MSM_SPM_REG_SAW2_VCTL]			= 0x1C,
	[MSM_SPM_REG_SAW2_AVS_CTL]		= 0x20,
	[MSM_SPM_REG_SAW2_AVS_LIMIT]		= 0x24,
	[MSM_SPM_REG_SAW2_AVS_DLY]		= 0x28,
	[MSM_SPM_REG_SAW2_AVS_HYSTERESIS]	= 0x2C,
	[MSM_SPM_REG_SAW2_SPM_CTL]		= 0x30,
	[MSM_SPM_REG_SAW2_SPM_DLY]		= 0x34,
	[MSM_SPM_REG_SAW2_STS2]			= 0x38,
	[MSM_SPM_REG_SAW2_PMIC_DATA_0]		= 0x40,
	[MSM_SPM_REG_SAW2_PMIC_DATA_1]		= 0x44,
	[MSM_SPM_REG_SAW2_PMIC_DATA_2]		= 0x48,
	[MSM_SPM_REG_SAW2_PMIC_DATA_3]		= 0x4C,
	[MSM_SPM_REG_SAW2_PMIC_DATA_4]		= 0x50,
	[MSM_SPM_REG_SAW2_PMIC_DATA_5]		= 0x54,
	[MSM_SPM_REG_SAW2_PMIC_DATA_6]		= 0x58,
	[MSM_SPM_REG_SAW2_PMIC_DATA_7]		= 0x5C,
	[MSM_SPM_REG_SAW2_SEQ_ENTRY]		= 0x400,
	[MSM_SPM_REG_SAW2_VERSION]		= 0xFD0,
};

static struct saw2_data saw2_info[] = {
	[0] = {
		"SAW2_v2.1",
		0x2,
		0x1,
		msm_spm_reg_offsets_saw2_v2_1,
	},
	[1] = {
		"SAW2_v3.0",
		0x3,
		0x0,
		msm_spm_reg_offsets_saw2_v3_0,
	},
};

static uint32_t num_pmic_data;

static void msm_spm_drv_flush_shadow(struct msm_spm_driver_data *dev,
		unsigned int reg_index)
{
	__raw_writel(dev->reg_shadow[reg_index],
		dev->reg_base_addr + dev->reg_offsets[reg_index]);
}

static void msm_spm_drv_load_shadow(struct msm_spm_driver_data *dev,
		unsigned int reg_index)
{
	dev->reg_shadow[reg_index] =
		__raw_readl(dev->reg_base_addr +
				dev->reg_offsets[reg_index]);
}

static inline uint32_t msm_spm_drv_get_num_spm_entry(
		struct msm_spm_driver_data *dev)
{
	BUG_ON(!dev);
	msm_spm_drv_load_shadow(dev, MSM_SPM_REG_SAW2_ID);
	return (dev->reg_shadow[MSM_SPM_REG_SAW2_ID] >> 24) & 0xFF;
}

static inline void msm_spm_drv_set_start_addr(
		struct msm_spm_driver_data *dev, uint32_t addr, bool pc_mode)
{
	addr &= 0x7F;
	addr <<= 4;
	dev->reg_shadow[MSM_SPM_REG_SAW2_SPM_CTL] &= 0xFFFFF80F;
	dev->reg_shadow[MSM_SPM_REG_SAW2_SPM_CTL] |= addr;

	if (dev->major != 0x3)
		return;

	dev->reg_shadow[MSM_SPM_REG_SAW2_SPM_CTL] &= 0xFFFEFFFF;
	if (pc_mode)
		dev->reg_shadow[MSM_SPM_REG_SAW2_SPM_CTL] |= 0x00010000;
}

static inline bool msm_spm_pmic_arb_present(struct msm_spm_driver_data *dev)
{
	msm_spm_drv_load_shadow(dev, MSM_SPM_REG_SAW2_ID);
	return (dev->reg_shadow[MSM_SPM_REG_SAW2_ID] >> 2) & 0x1;
}

static inline void msm_spm_drv_set_vctl2(struct msm_spm_driver_data *dev,
		uint32_t vlevel)
{
	unsigned int pmic_data = 0;

	/**
	 * VCTL_PORT has to be 0, for PMIC_STS register to be updated.
	 * Ensure that vctl_port is always set to 0.
	 */
	WARN_ON(dev->vctl_port);

	pmic_data |= vlevel;
	pmic_data |= (dev->vctl_port & 0x7) << 16;

	dev->reg_shadow[MSM_SPM_REG_SAW2_VCTL] &= ~0x700FF;
	dev->reg_shadow[MSM_SPM_REG_SAW2_VCTL] |= pmic_data;

	dev->reg_shadow[MSM_SPM_REG_SAW2_PMIC_DATA_3] &= ~0x700FF;
	dev->reg_shadow[MSM_SPM_REG_SAW2_PMIC_DATA_3] |= pmic_data;

	msm_spm_drv_flush_shadow(dev, MSM_SPM_REG_SAW2_VCTL);
	msm_spm_drv_flush_shadow(dev, MSM_SPM_REG_SAW2_PMIC_DATA_3);
}

static inline uint32_t msm_spm_drv_get_num_pmic_data(
		struct msm_spm_driver_data *dev)
{
	msm_spm_drv_load_shadow(dev, MSM_SPM_REG_SAW2_ID);
	mb();
	return (dev->reg_shadow[MSM_SPM_REG_SAW2_ID] >> 4) & 0x7;
}

static inline uint32_t msm_spm_drv_get_sts_pmic_state(
		struct msm_spm_driver_data *dev)
{
	msm_spm_drv_load_shadow(dev, MSM_SPM_REG_SAW2_PMIC_STS);
	return (dev->reg_shadow[MSM_SPM_REG_SAW2_PMIC_STS] >> 16) &
				0x03;
}

uint32_t msm_spm_drv_get_sts_curr_pmic_data(
		struct msm_spm_driver_data *dev)
{
	msm_spm_drv_load_shadow(dev, MSM_SPM_REG_SAW2_PMIC_STS);
	return dev->reg_shadow[MSM_SPM_REG_SAW2_PMIC_STS] & 0xFF;
}

static inline void msm_spm_drv_get_saw2_ver(struct msm_spm_driver_data *dev,
		uint32_t *major, uint32_t *minor)
{
	uint32_t val = 0;

	dev->reg_shadow[MSM_SPM_REG_SAW2_VERSION] =
			__raw_readl(dev->reg_base_addr + dev->ver_reg);

	val = dev->reg_shadow[MSM_SPM_REG_SAW2_VERSION];

	*major = (val >> 28) & 0xF;
	*minor = (val >> 16) & 0xFFF;
}

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
			+ dev->reg_offsets[MSM_SPM_REG_SAW2_SEQ_ENTRY]
			+ 4 * i);
	}
	mb();
}

void dump_regs(struct msm_spm_driver_data *dev, int cpu)
{
	msm_spm_drv_load_shadow(dev, MSM_SPM_REG_SAW2_SPM_STS);
	mb();
	pr_err("CPU%d: spm register MSM_SPM_REG_SAW2_SPM_STS: 0x%x\n", cpu,
			dev->reg_shadow[MSM_SPM_REG_SAW2_SPM_STS]);
	msm_spm_drv_load_shadow(dev, MSM_SPM_REG_SAW2_SPM_CTL);
	mb();
	pr_err("CPU%d: spm register MSM_SPM_REG_SAW2_SPM_CTL: 0x%x\n", cpu,
			dev->reg_shadow[MSM_SPM_REG_SAW2_SPM_CTL]);
}

int msm_spm_drv_write_seq_data(struct msm_spm_driver_data *dev,
		uint8_t *cmd, uint32_t *offset)
{
	uint32_t cmd_w;
	uint32_t offset_w = *offset / 4;
	uint8_t last_cmd;

	if (!cmd)
		return -EINVAL;

	while (1) {
		int i;
		cmd_w = 0;
		last_cmd = 0;
		cmd_w = dev->reg_seq_entry_shadow[offset_w];

		for (i = (*offset % 4); i < 4; i++) {
			last_cmd = *(cmd++);
			cmd_w |=  last_cmd << (i * 8);
			(*offset)++;
			if (last_cmd == 0x0f)
				break;
		}

		dev->reg_seq_entry_shadow[offset_w++] = cmd_w;
		if (last_cmd == 0x0f)
			break;
	}

	return 0;
}

int msm_spm_drv_set_low_power_mode(struct msm_spm_driver_data *dev,
		uint32_t addr, bool pc_mode)
{

	/* SPM is configured to reset start address to zero after end of Program
	 */
	if (!dev)
		return -EINVAL;

	msm_spm_drv_set_start_addr(dev, addr, pc_mode);

	msm_spm_drv_flush_shadow(dev, MSM_SPM_REG_SAW2_SPM_CTL);
	wmb();

	if (msm_spm_debug_mask & MSM_SPM_DEBUG_SHADOW) {
		int i;
		for (i = 0; i < MSM_SPM_REG_NR; i++)
			pr_info("%s: reg %02x = 0x%08x\n", __func__,
				dev->reg_offsets[i], dev->reg_shadow[i]);
	}
	msm_spm_drv_load_shadow(dev, MSM_SPM_REG_SAW2_SPM_STS);

	return 0;
}

#ifdef CONFIG_MSM_AVS_HW
static bool msm_spm_drv_is_avs_enabled(struct msm_spm_driver_data *dev)
{
	msm_spm_drv_load_shadow(dev, MSM_SPM_REG_SAW2_AVS_CTL);
	return dev->reg_shadow[MSM_SPM_REG_SAW2_AVS_CTL] & BIT(0);
}

static void msm_spm_drv_disable_avs(struct msm_spm_driver_data *dev)
{
	msm_spm_drv_load_shadow(dev, MSM_SPM_REG_SAW2_AVS_CTL);
	dev->reg_shadow[MSM_SPM_REG_SAW2_AVS_CTL] &= ~BIT(27);
	msm_spm_drv_flush_shadow(dev, MSM_SPM_REG_SAW2_AVS_CTL);
}

static void msm_spm_drv_enable_avs(struct msm_spm_driver_data *dev)
{
	dev->reg_shadow[MSM_SPM_REG_SAW2_AVS_CTL] |= BIT(27);
	msm_spm_drv_flush_shadow(dev, MSM_SPM_REG_SAW2_AVS_CTL);
}

static void msm_spm_drv_set_avs_vlevel(struct msm_spm_driver_data *dev,
		unsigned int vlevel)
{
	vlevel &= 0x3f;
	dev->reg_shadow[MSM_SPM_REG_SAW2_AVS_CTL] &= ~0x7efc00;
	dev->reg_shadow[MSM_SPM_REG_SAW2_AVS_CTL] |= ((vlevel - 4) << 10);
	dev->reg_shadow[MSM_SPM_REG_SAW2_AVS_CTL] |= (vlevel << 17);
	msm_spm_drv_flush_shadow(dev, MSM_SPM_REG_SAW2_AVS_CTL);
}

#else
static bool msm_spm_drv_is_avs_enabled(struct msm_spm_driver_data *dev)
{
	return false;
}

static void msm_spm_drv_disable_avs(struct msm_spm_driver_data *dev) { }

static void msm_spm_drv_enable_avs(struct msm_spm_driver_data *dev) { }

static void msm_spm_drv_set_avs_vlevel(struct msm_spm_driver_data *dev,
		unsigned int vlevel) { }
#endif

int msm_spm_drv_set_vdd(struct msm_spm_driver_data *dev, unsigned int vlevel)
{
	uint32_t timeout_us, new_level;
	bool avs_enabled;

	if (!dev)
		return -EINVAL;

	avs_enabled  = msm_spm_drv_is_avs_enabled(dev);

	if (!msm_spm_pmic_arb_present(dev))
		return -ENOSYS;

	if (msm_spm_debug_mask & MSM_SPM_DEBUG_VCTL)
		pr_info("%s: requesting vlevel %#x\n", __func__, vlevel);

	if (avs_enabled)
		msm_spm_drv_disable_avs(dev);

	/* Kick the state machine back to idle */
	dev->reg_shadow[MSM_SPM_REG_SAW2_RST] = 1;
	msm_spm_drv_flush_shadow(dev, MSM_SPM_REG_SAW2_RST);

	msm_spm_drv_set_vctl2(dev, vlevel);

	timeout_us = dev->vctl_timeout_us;
	/* Confirm the voltage we set was what hardware sent */
	do {
		new_level = msm_spm_drv_get_sts_curr_pmic_data(dev);
		if (new_level == vlevel)
			break;
		udelay(1);
	} while (--timeout_us);
	if (!timeout_us) {
		pr_info("Wrong level %#x\n", new_level);
		goto set_vdd_bail;
	}

	if (msm_spm_debug_mask & MSM_SPM_DEBUG_VCTL)
		pr_info("%s: done, remaining timeout %u us\n",
			__func__, timeout_us);

	/* Set AVS min/max */
	if (avs_enabled) {
		msm_spm_drv_set_avs_vlevel(dev, vlevel);
		msm_spm_drv_enable_avs(dev);
	}

	return 0;

set_vdd_bail:
	if (avs_enabled)
		msm_spm_drv_enable_avs(dev);

	pr_err("%s: failed %#x, remaining timeout %uus, vlevel %#x\n",
		__func__, vlevel, timeout_us, new_level);
	return -EIO;
}

static int msm_spm_drv_get_pmic_port(struct msm_spm_driver_data *dev,
		enum msm_spm_pmic_port port)
{
	int index = -1;

	switch (port) {
	case MSM_SPM_PMIC_VCTL_PORT:
		index = dev->vctl_port;
		break;
	case MSM_SPM_PMIC_PHASE_PORT:
		index = dev->phase_port;
		break;
	case MSM_SPM_PMIC_PFM_PORT:
		index = dev->pfm_port;
		break;
	default:
		break;
	}

	return index;
}

int msm_spm_drv_set_pmic_data(struct msm_spm_driver_data *dev,
		enum msm_spm_pmic_port port, unsigned int data)
{
	unsigned int pmic_data = 0;
	unsigned int timeout_us = 0;
	int index = 0;

	if (!msm_spm_pmic_arb_present(dev))
		return -ENOSYS;

	index = msm_spm_drv_get_pmic_port(dev, port);
	if (index < 0)
		return -ENODEV;

	pmic_data |= data & 0xFF;
	pmic_data |= (index & 0x7) << 16;

	dev->reg_shadow[MSM_SPM_REG_SAW2_VCTL] &= ~0x700FF;
	dev->reg_shadow[MSM_SPM_REG_SAW2_VCTL] |= pmic_data;
	msm_spm_drv_flush_shadow(dev, MSM_SPM_REG_SAW2_VCTL);
	mb();

	timeout_us = dev->vctl_timeout_us;
	/**
	 * Confirm the pmic data set was what hardware sent by
	 * checking the PMIC FSM state.
	 * We cannot use the sts_pmic_data and check it against
	 * the value like we do fot set_vdd, since the PMIC_STS
	 * is only updated for SAW_VCTL sent with port index 0.
	 */
	do {
		if (msm_spm_drv_get_sts_pmic_state(dev) ==
				MSM_SPM_PMIC_STATE_IDLE)
			break;
		udelay(1);
	} while (--timeout_us);

	if (!timeout_us) {
		pr_err("%s: failed, remaining timeout %u us, data %d\n",
				__func__, timeout_us, data);
		return -EIO;
	}

	return 0;
}

void msm_spm_drv_reinit(struct msm_spm_driver_data *dev)
{
	int i;

	for (i = 0; i < MSM_SPM_REG_SAW2_PMIC_DATA_0 + num_pmic_data; i++)
		msm_spm_drv_flush_shadow(dev, i);

	msm_spm_drv_flush_seq_entry(dev);
	mb();
}

int msm_spm_drv_init(struct msm_spm_driver_data *dev,
		struct msm_spm_platform_data *data)
{
	int i;
	int num_spm_entry;
	bool found = false;

	BUG_ON(!dev || !data);

	dev->vctl_port = data->vctl_port;
	dev->phase_port = data->phase_port;
	dev->pfm_port = data->pfm_port;
	dev->reg_base_addr = data->reg_base_addr;
	memcpy(dev->reg_shadow, data->reg_init_values,
			sizeof(data->reg_init_values));

	dev->vctl_timeout_us = data->vctl_timeout_us;

	msm_spm_drv_get_saw2_ver(dev, &dev->major, &dev->minor);

	for (i = 0; i < ARRAY_SIZE(saw2_info); i++)
		if (dev->major == saw2_info[i].major &&
			dev->minor == saw2_info[i].minor) {
			pr_debug("%s: Version found\n",
					saw2_info[i].ver_name);
			dev->reg_offsets = saw2_info[i].spm_reg_offset_ptr;
			found = true;
			break;
		}

	if (!found) {
		pr_err("%s: No SAW2 version found\n", __func__);
		BUG_ON(!found);
	}

	if (!num_pmic_data)
		num_pmic_data = msm_spm_drv_get_num_pmic_data(dev);

	for (i = 0; i < MSM_SPM_REG_SAW2_PMIC_DATA_0 + num_pmic_data; i++)
		msm_spm_drv_flush_shadow(dev, i);

	/* barrier to ensure write completes before we update shadow
	 * registers
	 */
	mb();

	for (i = 0; i < MSM_SPM_REG_SAW2_PMIC_DATA_0 + num_pmic_data; i++)
		msm_spm_drv_load_shadow(dev, i);

	/* barrier to ensure read completes before we proceed further*/
	mb();

	num_spm_entry = msm_spm_drv_get_num_spm_entry(dev);

	dev->reg_seq_entry_shadow =
		kzalloc(sizeof(*dev->reg_seq_entry_shadow) * num_spm_entry,
				GFP_KERNEL);

	if (!dev->reg_seq_entry_shadow)
		return -ENOMEM;

	return 0;
}
