/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/types.h>
#include "mt_afe_def.h"
#include "mt_afe_reg.h"
#include "mt_afe_clk.h"
#include "mt_afe_control.h"
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/pm_runtime.h>

/* #define DEBUG_IRQ_STATUS */
#ifdef DEBUG_IRQ_STATUS
#include <mach/mt_gpt.h>
static unsigned long long irq1_counter;
static unsigned int pre_irq1_gpt_cnt;
#endif

static DEFINE_SPINLOCK(afe_control_lock);

#define MT8173_AFE_MCU_IRQ_LINE (134 + 32)
#define BOARD_CHANNEL_TYPE_PROPERTY "mediatek,board-channel-type"

/*
 *    global variable control
 */

/* static variable */
static struct mt_afe_merge_interface *audio_mrg;
static struct mt_afe_digital_dai_bt *audio_dai_bt;

static struct mt_afe_irq_status *audio_mcu_mode[MT_AFE_IRQ_MCU_MODE_NUM] = { NULL };
static struct mt_afe_mem_if_attribute *audio_mem_if[MT_AFE_DIGITAL_BLOCK_NUM] = { NULL };
static struct mt_afe_mem_control_t *afe_mem_control_context[MT_AFE_MEM_CTX_COUNT] = { NULL };
static int apll_clock_divider_power_refcount[MT_AFE_APLL_CLOCK_TYPE_NUM] = { 0 };

static struct mt_afe_suspend_reg suspend_reg;
static bool aud_drv_suspend_status;
static unsigned int audio_irq_id = MT8173_AFE_MCU_IRQ_LINE;
static unsigned int board_channel_type;

static struct device *mach_dev;
static bool audio_power_status;

static bool cdc_initialized;


/*
 *    static function declaration
 */
static void mt_afe_init_control(void *dev);
static int mt_afe_register_irq(void *dev);
static irqreturn_t mt_afe_irq_handler(int irq, void *dev_id);
static uint32_t mt_afe_rate_to_idx(uint32_t sample_rate);
static void mt_afe_dl_interrupt_handler(void);
static void mt_afe_dl2_interrupt_handler(void);
static void mt_afe_ul_interrupt_handler(void);
static void mt_afe_hdmi_interrupt_handler(void);
static void mt_afe_hdmi_raw_interrupt_handler(void);
static void mt_afe_spdif_interrupt_handler(void);
static void mt_afe_handle_mem_context(enum mt_afe_mem_context mem_context);
static void mt_afe_clean_predistortion(void);
static bool mt_afe_set_dl_src2(uint32_t sample_rate);
static bool mt_afe_is_memif_enable(void);
static bool mt_afe_is_ul_memif_enable(void);
static uint32_t mt_afe_get_apll_by_rate(uint32_t sample_rate);
static void mt_afe_store_reg(struct mt_afe_suspend_reg *backup_reg);
static void mt_afe_recover_reg(struct mt_afe_suspend_reg *backup_reg);
static void mt_afe_enable_i2s_div_power(uint32_t divider);
static void mt_afe_disable_i2s_div_power(uint32_t divider);


/*
 *    function implementation
 */

int mt_afe_platform_init(void *dev)
{
	struct device *pdev = dev;
	int ret = 0;
	unsigned int irq_id = 0;

	if (!pdev->of_node) {
		pr_warn("%s invalid of_node\n", __func__);
		return -ENODEV;
	}

	if (!pdev->pm_domain) {
		pr_warn("%s invalid pm_domain\n", __func__);
		return -EPROBE_DEFER;
	}

	irq_id = irq_of_parse_and_map(pdev->of_node, 0);
	if (irq_id)
		audio_irq_id = irq_id;
	else
		pr_warn("%s irq_of_parse_and_map invalid irq\n", __func__);

	ret = of_property_read_u32(pdev->of_node, BOARD_CHANNEL_TYPE_PROPERTY,
				 &board_channel_type);
	if (ret) {
		pr_warn("%s read property %s fail in node %s\n", __func__,
			BOARD_CHANNEL_TYPE_PROPERTY, pdev->of_node->full_name);
	}

	ret = mt_afe_reg_remap(dev);
	if (ret)
		return ret;

	ret = mt_afe_init_clock(dev);
	if (ret)
		return ret;

	pm_runtime_enable(dev);

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		pr_warn("%s pm_runtime_get_sync fail %d\n", __func__, ret);
		return ret;
	}

	mt_afe_power_off_default_clock();

	mt_afe_register_irq(dev);

	mt_afe_apb_bus_init();

	mt_afe_init_control(dev);

	mach_dev = dev;

	audio_power_status = true;

	return ret;
}

void mt_afe_platform_deinit(void *dev)
{
	mt_afe_reg_unmap();

	if (audio_power_status) {
		pm_runtime_put_sync(dev);
		audio_power_status = false;
	}

	pm_runtime_disable(dev);
	mt_afe_deinit_clock(dev);
}

void mt_afe_set_sample_rate(uint32_t aud_block, uint32_t sample_rate)
{
	pr_debug("%s aud_block = %u sample_rate = %u\n", __func__, aud_block, sample_rate);
	sample_rate = mt_afe_rate_to_idx(sample_rate);

	switch (aud_block) {
	case MT_AFE_DIGITAL_BLOCK_MEM_DL1:
		mt_afe_set_reg(AFE_DAC_CON1, sample_rate, 0x0000000f);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_DL1_DATA2:
		mt_afe_set_reg(AFE_DAC_CON0, sample_rate << 16, 0x000f0000);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_DL2:
		mt_afe_set_reg(AFE_DAC_CON1, sample_rate << 4, 0x000000f0);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_I2S:
		mt_afe_set_reg(AFE_DAC_CON1, sample_rate << 8, 0x00000f00);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_AWB:
		mt_afe_set_reg(AFE_DAC_CON1, sample_rate << 12, 0x0000f000);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_VUL:
		mt_afe_set_reg(AFE_DAC_CON1, sample_rate << 16, 0x000f0000);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_VUL_DATA2:
		mt_afe_set_reg(AFE_DAC_CON0, sample_rate << 20, 0x00f00000);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_DAI:
		if (sample_rate == MT_AFE_I2S_SAMPLERATE_8K)
			mt_afe_set_reg(AFE_DAC_CON0, 0 << 24, 1 << 24);
		else if (sample_rate == MT_AFE_I2S_SAMPLERATE_16K)
			mt_afe_set_reg(AFE_DAC_CON0, 1 << 24, 1 << 24);
		else if (sample_rate == MT_AFE_I2S_SAMPLERATE_32K)
			mt_afe_set_reg(AFE_DAC_CON0, 2 << 24, 1 << 24);
		else
			pr_warn("%s aud_block = %u invalid sample_rate = %u\n", __func__,
				aud_block, sample_rate);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_MOD_DAI:
		if (sample_rate == MT_AFE_I2S_SAMPLERATE_8K)
			mt_afe_set_reg(AFE_DAC_CON1, 0 << 30, 1 << 30);
		else if (sample_rate == MT_AFE_I2S_SAMPLERATE_16K)
			mt_afe_set_reg(AFE_DAC_CON1, 1 << 30, 1 << 30);
		else if (sample_rate == MT_AFE_I2S_SAMPLERATE_32K)
			mt_afe_set_reg(AFE_DAC_CON1, 2 << 30, 1 << 30);
		else
			pr_warn("%s aud_block = %u invalid sample_rate = %u\n", __func__,
				aud_block, sample_rate);
		break;
	default:
		pr_debug("%s unexpected aud_block = %u\n", __func__, aud_block);
		break;
	}
}

void mt_afe_set_channels(uint32_t memory_interface, uint32_t channel)
{
	uint32_t mono = (channel == 1) ? 1 : 0;

	switch (memory_interface) {
	case MT_AFE_DIGITAL_BLOCK_MEM_DL1:
		mt_afe_set_reg(AFE_DAC_CON1, mono << 21, 1 << 21);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_DL1_DATA2:
		mt_afe_set_reg(AFE_DAC_CON1, mono << 20, 1 << 20);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_DL2:
		mt_afe_set_reg(AFE_DAC_CON1, mono << 22, 1 << 22);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_AWB:
		mt_afe_set_reg(AFE_DAC_CON1, mono << 24, 1 << 24);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_VUL:
		mt_afe_set_reg(AFE_DAC_CON1, mono << 27, 1 << 27);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_VUL_DATA2:
		mt_afe_set_reg(AFE_DAC_CON0, mono << 10, 1 << 10);
		break;
	default:
		pr_warn("%s unexpected memory interface = %u channel = %u\n",
			__func__, memory_interface, channel);
		break;
	}
}

void mt_afe_set_mono_type(uint32_t memory_interface, uint32_t mono_type)
{
	switch (memory_interface) {
	case MT_AFE_DIGITAL_BLOCK_MEM_AWB:
		mt_afe_set_reg(AFE_DAC_CON1, mono_type << 25, 1 << 25);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_VUL:
		mt_afe_set_reg(AFE_DAC_CON1, mono_type << 28, 1 << 28);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_VUL_DATA2:
		mt_afe_set_reg(AFE_DAC_CON0, mono_type << 11, 1 << 11);
		break;
	default:
		pr_warn("%s unexpected memory interface = %u\n",
			__func__, memory_interface);
		break;
	}
}

void mt_afe_set_irq_counter(uint32_t irq_mode, uint32_t counter)
{
	switch (irq_mode) {
	case MT_AFE_IRQ_MCU_MODE_IRQ1:
		mt_afe_set_reg(AFE_IRQ_MCU_CNT1, counter, 0x0003ffff);
		break;
	case MT_AFE_IRQ_MCU_MODE_IRQ2:
		mt_afe_set_reg(AFE_IRQ_MCU_CNT2, counter, 0x0003ffff);
		break;
	case MT_AFE_IRQ_MCU_MODE_IRQ5:
		mt_afe_set_reg(AFE_IRQ_MCU_CNT5, counter, 0x0003ffff);
		break;
	case MT_AFE_IRQ_MCU_MODE_IRQ3:
		mt_afe_set_reg(AFE_IRQ_MCU_CNT1, counter << 20, 0xfff00000);
		break;
	case MT_AFE_IRQ_MCU_MODE_IRQ4:
		mt_afe_set_reg(AFE_IRQ_MCU_CNT2, counter << 20, 0xfff00000);
		break;
	case MT_AFE_IRQ_MCU_MODE_IRQ7:
		mt_afe_set_reg(AFE_IRQ_MCU_CNT7, counter, 0x0003ffff);
		break;
	default:
		break;
	}
}

void mt_afe_set_irq_rate(uint32_t irq_mode, uint32_t sample_rate)
{
	switch (irq_mode) {
	case MT_AFE_IRQ_MCU_MODE_IRQ1:
		mt_afe_set_reg(AFE_IRQ_MCU_CON, (mt_afe_rate_to_idx(sample_rate) << 4),
			    0x000000f0);
		break;
	case MT_AFE_IRQ_MCU_MODE_IRQ2:
		mt_afe_set_reg(AFE_IRQ_MCU_CON, (mt_afe_rate_to_idx(sample_rate) << 8),
			    0x00000f00);
		break;
	case MT_AFE_IRQ_MCU_MODE_IRQ3:
		mt_afe_set_reg(AFE_IRQ_MCU_CON, (mt_afe_rate_to_idx(sample_rate) << 16),
			    0x000f0000);
		break;
	case MT_AFE_IRQ_MCU_MODE_IRQ4:
		mt_afe_set_reg(AFE_IRQ_MCU_CON, (mt_afe_rate_to_idx(sample_rate) << 20),
			    0x00f00000);
		break;
	case MT_AFE_IRQ_MCU_MODE_IRQ7:
		mt_afe_set_reg(AFE_IRQ_MCU_CON, (mt_afe_rate_to_idx(sample_rate) << 24),
			    0x0f000000);
		break;
	default:
		break;
	}
}

void mt_afe_set_irq_state(uint32_t irq_mode, bool enable)
{
	pr_debug("%s irq_mode = %d enable = %d\n", __func__, irq_mode, enable);

	switch (irq_mode) {
	case MT_AFE_IRQ_MCU_MODE_IRQ2:
		if (unlikely(!enable && mt_afe_is_ul_memif_enable())) {
			/* IRQ2 is in used */
			pr_debug("skip disable IRQ2, AFE_DAC_CON0 = 0x%x\n",
				mt_afe_get_reg(AFE_DAC_CON0));
			break;
		}
		/* fall through */
	case MT_AFE_IRQ_MCU_MODE_IRQ1:
	case MT_AFE_IRQ_MCU_MODE_IRQ3:
	case MT_AFE_IRQ_MCU_MODE_IRQ4:
#ifdef DEBUG_IRQ_STATUS
		if (irq_mode == MT_AFE_IRQ_MCU_MODE_IRQ1 && enable) {
			gpt_get_cnt(GPT2, &pre_irq1_gpt_cnt);
			irq1_counter = 0;
		}
#endif
		mt_afe_set_reg(AFE_IRQ_MCU_CON, (enable << irq_mode), (1 << irq_mode));
		audio_mcu_mode[irq_mode]->status = enable;
		break;
	case MT_AFE_IRQ_MCU_MODE_IRQ5:
		mt_afe_set_reg(AFE_IRQ_MCU_CON, (enable << 12), (1 << 12));
		audio_mcu_mode[irq_mode]->status = enable;
		break;
	case MT_AFE_IRQ_MCU_MODE_IRQ6:
		mt_afe_set_reg(AFE_IRQ_MCU_CON, (enable << 13), (1 << 13));
		audio_mcu_mode[irq_mode]->status = enable;
		break;
	case MT_AFE_IRQ_MCU_MODE_IRQ7:
		mt_afe_set_reg(AFE_IRQ_MCU_CON, (enable << 14), (1 << 14));
		audio_mcu_mode[irq_mode]->status = enable;
		break;
	case MT_AFE_IRQ_MCU_MODE_IRQ8:
		mt_afe_set_reg(AFE_IRQ_MCU_CON, (enable << 15), (1 << 15));
		audio_mcu_mode[irq_mode]->status = enable;
		break;
	default:
		pr_warn("%s unexpected irq_mode = %d\n", __func__, irq_mode);
		break;
	}

	if (!enable && irq_mode < MT_AFE_IRQ_MCU_MODE_NUM)
		mt_afe_set_reg(AFE_IRQ_MCU_CLR, 1 << irq_mode, 1 << irq_mode);
}

int mt_afe_get_irq_state(uint32_t irq_mode, struct mt_afe_irq_status *mcu_mode)
{
	if (irq_mode < MT_AFE_IRQ_MCU_MODE_NUM && mcu_mode) {
		memcpy((void *)mcu_mode, (const void *)audio_mcu_mode[irq_mode],
		       sizeof(struct mt_afe_irq_status));
		return 0;
	} else {
		return -EINVAL;
	}
}

int mt_afe_enable_memory_path(uint32_t block)
{
	unsigned long flags;

	if (block >= MT_AFE_DIGITAL_BLOCK_NUM)
		return -EINVAL;

	spin_lock_irqsave(&afe_control_lock, flags);

	if (audio_mem_if[block]->user_count == 0)
		audio_mem_if[block]->state = true;

	audio_mem_if[block]->user_count++;

	if (block < MT_AFE_MEM_INTERFACE_NUM)
		mt_afe_set_reg(AFE_DAC_CON0, 1 << (block + 1), 1 << (block + 1));

	spin_unlock_irqrestore(&afe_control_lock, flags);

	return 0;
}

int mt_afe_disable_memory_path(uint32_t block)
{
	unsigned long flags;

	if (block >= MT_AFE_DIGITAL_BLOCK_NUM)
		return -EINVAL;

	spin_lock_irqsave(&afe_control_lock, flags);

	audio_mem_if[block]->user_count--;
	if (audio_mem_if[block]->user_count == 0)
		audio_mem_if[block]->state = false;

	if (audio_mem_if[block]->user_count < 0) {
		pr_warn("%s block %u user count %d < 0\n",
			__func__, block, audio_mem_if[block]->user_count);
		audio_mem_if[block]->user_count = 0;
	}

	if (block < MT_AFE_MEM_INTERFACE_NUM)
		mt_afe_set_reg(AFE_DAC_CON0, 0, 1 << (block + 1));

	spin_unlock_irqrestore(&afe_control_lock, flags);

	return 0;
}

bool mt_afe_get_memory_path_state(uint32_t block)
{
	unsigned long flags;
	bool state = false;

	spin_lock_irqsave(&afe_control_lock, flags);

	if (block < MT_AFE_DIGITAL_BLOCK_NUM)
		state = audio_mem_if[block]->state;

	spin_unlock_irqrestore(&afe_control_lock, flags);

	return state;
}

void mt_afe_set_i2s_dac_out(uint32_t sample_rate, uint32_t clock_mode, uint32_t wlen)
{
	uint32_t audio_i2s_dac = 0;

	mt_afe_clean_predistortion();
	mt_afe_set_dl_src2(sample_rate);

	audio_i2s_dac |= (MT_AFE_LR_SWAP_NO_SWAP << 31);
	audio_i2s_dac |= (clock_mode << 12);
	audio_i2s_dac |= (mt_afe_rate_to_idx(sample_rate) << 8);
	audio_i2s_dac |= (MT_AFE_INV_LRCK_NO_INVERSE << 5);
	audio_i2s_dac |= (MT_AFE_I2S_FORMAT_I2S << 3);
	audio_i2s_dac |= (wlen << 1);
	mt_afe_set_reg(AFE_I2S_CON1, audio_i2s_dac, MASK_ALL);
}

int mt_afe_enable_i2s_dac(void)
{
	mt_afe_set_reg(AFE_ADDA_DL_SRC2_CON0, 0x1, 0x1);
	mt_afe_set_reg(AFE_I2S_CON1, 0x1, 0x1);
	mt_afe_set_reg(AFE_ADDA_UL_DL_CON0, 0x1, 0x1);
	/* For FPGA Pin the same with DAC */
	/* mt_afe_set_reg(FPGA_CFG1, 0, 0x10); */
	return 0;
}

int mt_afe_disable_i2s_dac(void)
{
	mt_afe_set_reg(AFE_ADDA_DL_SRC2_CON0, 0x0, 0x01);
	mt_afe_set_reg(AFE_I2S_CON1, 0x0, 0x1);

	if (!audio_mem_if[MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC]->state &&
	    !audio_mem_if[MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC]->state) {
		mt_afe_set_reg(AFE_ADDA_UL_DL_CON0, 0x0, 0x1);
	}
	/* For FPGA Pin the same with DAC */
	/* mt_afe_set_reg(FPGA_CFG1, 1 << 4, 0x10); */
	return 0;
}

void mt_afe_enable_afe(bool enable)
{
	unsigned long flags;
	bool memif_enable;

	spin_lock_irqsave(&afe_control_lock, flags);
	memif_enable = mt_afe_is_memif_enable();

	if (!enable && !memif_enable)
		mt_afe_set_reg(AFE_DAC_CON0, 0x0, 0x1);
	else if (enable && memif_enable)
		mt_afe_set_reg(AFE_DAC_CON0, 0x1, 0x1);

	spin_unlock_irqrestore(&afe_control_lock, flags);
}

void mt_afe_set_mtkif_adc_in(uint32_t sample_rate)
{
	uint32_t sample_rate_index = mt_afe_rate_to_idx(sample_rate);
	uint32_t voice_mode_select = 0;

	/* I_03/I_04 source from internal ADC */
	mt_afe_set_reg(AFE_ADDA_TOP_CON0, 0, 0x1);
	if (sample_rate_index == MT_AFE_I2S_SAMPLERATE_8K)
		voice_mode_select = 0;
	else if (sample_rate_index == MT_AFE_I2S_SAMPLERATE_16K)
		voice_mode_select = 1;
	else if (sample_rate_index == MT_AFE_I2S_SAMPLERATE_32K)
		voice_mode_select = 2;
	else if (sample_rate_index == MT_AFE_I2S_SAMPLERATE_48K)
		voice_mode_select = 3;

	mt_afe_set_reg(AFE_ADDA_UL_SRC_CON0,
		    ((voice_mode_select << 2) | voice_mode_select) << 17, 0x001E0000);
	/* up8x txif sat on */
	/* mt_afe_set_reg(AFE_ADDA_NEWIF_CFG0, 0x03F87201, 0xFFFFFFFF); */
	mt_afe_set_reg(AFE_ADDA_NEWIF_CFG1, ((voice_mode_select < 3) ? 1 : 3) << 10,
		    0x00000C00);
}

void mt_afe_enable_mtkif_adc(void)
{
	mt_afe_set_reg(AFE_ADDA_UL_SRC_CON0, 0x1, 0x1);
	mt_afe_set_reg(AFE_ADDA_UL_DL_CON0, 0x1, 0x1);
}

void mt_afe_disable_mtkif_adc(void)
{
	mt_afe_set_reg(AFE_ADDA_UL_SRC_CON0, 0x0, 0x1);

	if (audio_mem_if[MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC]->state == false &&
	    audio_mem_if[MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC]->state == false) {
		mt_afe_set_reg(AFE_ADDA_UL_DL_CON0, 0x0, 0x1);
	}
}

void mt_afe_set_i2s_adc_in(uint32_t sample_rate, uint32_t clock_mode)
{
	uint32_t reg_val = 0;

	/* I_03/I_04 source from external ADC */
	mt_afe_set_reg(AFE_ADDA_TOP_CON0, 1, 0x1);

	reg_val |= (MT_AFE_LR_SWAP_NO_SWAP << 31);
	reg_val |= (8 << 24);
	reg_val |= (MT_AFE_INV_LRCK_NO_INVERSE << 23);
	reg_val |= (0 << 22);
	reg_val |= (0 << 21);
	reg_val |= (0 << 20);
	reg_val |= (clock_mode << 12);
	reg_val |= (mt_afe_rate_to_idx(sample_rate) << 8);
	reg_val |= (MT_AFE_I2S_FORMAT_I2S << 3);
	reg_val |= (MT_AFE_I2S_WLEN_16BITS << 1);
	mt_afe_set_reg(AFE_I2S_CON2, reg_val, MASK_ALL);
}

void mt_afe_enable_i2s_adc(void)
{
	mt_afe_set_reg(AFE_I2S_CON2, 0x1, 0x1);
}

void mt_afe_disable_i2s_adc(void)
{
	if (audio_mem_if[MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC]->state == false &&
	    audio_mem_if[MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC2]->state == false)
		mt_afe_set_reg(AFE_I2S_CON2, 0x0, 0x1);
}

void mt_afe_set_i2s_adc2_in(uint32_t sample_rate, uint32_t clock_mode)
{
	uint32_t reg_value = 0;

	reg_value |= (MT_AFE_LR_SWAP_NO_SWAP << 31);
	reg_value |= (8 << 24);
	reg_value |= (MT_AFE_BCK_INV_NO_INVERSE << 23);
	reg_value |= (clock_mode << 12);
	reg_value |= (mt_afe_rate_to_idx(sample_rate) << 8);
	reg_value |= (MT_AFE_I2S_FORMAT_I2S << 3);
	reg_value |= (MT_AFE_I2S_WLEN_16BITS << 1);
	mt_afe_set_reg(AFE_I2S_CON2, reg_value, 0xFFFFFFFE);
	/* I_17/I_18 source from external ADC */
	mt_afe_set_reg(AFE_ADDA2_TOP_CON0, 0x1, 0x1);
}

void mt_afe_enable_i2s_adc2(void)
{
	mt_afe_set_reg(AFE_I2S_CON2, 0x1, 0x1);
}

void mt_afe_disable_i2s_adc2(void)
{
	if (audio_mem_if[MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC]->state == false &&
	    audio_mem_if[MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC2]->state == false)
		mt_afe_set_reg(AFE_I2S_CON2, 0x0, 0x1);
}

void mt_afe_set_2nd_i2s_out(uint32_t sample_rate, uint32_t clock_mode, uint32_t wlen)
{
	uint32_t reg_value = 0;

	reg_value |= (MT_AFE_LR_SWAP_NO_SWAP << 31);
	reg_value |= (clock_mode << 12);
	reg_value |= (mt_afe_rate_to_idx(sample_rate) << 8);
	reg_value |= (MT_AFE_INV_LRCK_NO_INVERSE << 5);
	reg_value |= (MT_AFE_I2S_FORMAT_I2S << 3);
	reg_value |= (wlen << 1);
	mt_afe_set_reg(AFE_I2S_CON3, reg_value, 0xFFFFFFFE);
}

int mt_afe_enable_2nd_i2s_out(void)
{
	mt_afe_set_reg(AFE_I2S_CON3, 0x1, 0x1);
	return 0;
}

int mt_afe_disable_2nd_i2s_out(void)
{
	mt_afe_set_reg(AFE_I2S_CON3, 0x0, 0x1);
	return 0;
}

void mt_afe_set_2nd_i2s_in(uint32_t wlen, uint32_t src_mode,
			uint32_t bck_inv, uint32_t clock_mode)
{
	uint32_t reg_value = 0;

	reg_value |= (1 << 31);	/* enable phase_shift_fix for better quality */
	reg_value |= (bck_inv << 29);
	reg_value |= (MT_AFE_I2S_IN_FROM_IO_MUX << 28);
	reg_value |= (clock_mode << 12);
	reg_value |= (MT_AFE_INV_LRCK_NO_INVERSE << 5);
	reg_value |= (MT_AFE_I2S_FORMAT_I2S << 3);
	reg_value |= (src_mode << 2);
	reg_value |= (wlen << 1);
	mt_afe_set_reg(AFE_I2S_CON, reg_value, 0xFFFFFFFE);
}

int mt_afe_enable_2nd_i2s_in(void)
{
	mt_afe_set_reg(AFE_I2S_CON, 0x1, 0x1);
	return 0;
}

int mt_afe_disable_2nd_i2s_in(void)
{
	mt_afe_set_reg(AFE_I2S_CON, 0x0, 0x1);
	return 0;
}

void mt_afe_set_i2s_asrc_config(unsigned int sample_rate)
{
	switch (sample_rate) {
	case 44100:
		mt_afe_set_reg(AFE_ASRC_CON13, 0x0, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON14, 0x1B9000, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON15, 0x1B9000, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON16, 0x3F5987, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON17, 0x1FBD, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON20, 0x9C00, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON21, 0x8B00, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON0, 0x71, 0xFFFFFFFF);
		break;
	case 48000:
		mt_afe_set_reg(AFE_ASRC_CON13, 0x0, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON14, 0x1E0000, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON15, 0x1E0000, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON16, 0x3F5987, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON17, 0x1FBD, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON20, 0x8F00, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON21, 0x7F00, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON0, 0x71, 0xFFFFFFFF);
		break;
	case 32000:
		mt_afe_set_reg(AFE_ASRC_CON13, 0x0, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON14, 0x140000, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON15, 0x140000, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON16, 0x3F5987, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON17, 0x1FBD, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON20, 0xD800, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON21, 0xBD00, 0xFFFFFFFF);
		mt_afe_set_reg(AFE_ASRC_CON0, 0x71, 0xFFFFFFFF);
		break;
	default:
		pr_warn("%s sample rate %u not handled\n", __func__, sample_rate);
		break;
	}
}

void mt_afe_set_hw_digital_gain_mode(uint32_t gain_type, uint32_t sample_rate,
				uint32_t sample_per_step)
{
	uint32_t value = sample_per_step << 8 | (mt_afe_rate_to_idx(sample_rate) << 4);

	switch (gain_type) {
	case MT_AFE_HW_DIGITAL_GAIN1:
		mt_afe_set_reg(AFE_GAIN1_CON0, value, 0xfff0);
		break;
	case MT_AFE_HW_DIGITAL_GAIN2:
		mt_afe_set_reg(AFE_GAIN2_CON0, value, 0xfff0);
		break;
	default:
		break;
	}
}

void mt_afe_set_hw_digital_gain_state(int gain_type, bool enable)
{
	switch (gain_type) {
	case MT_AFE_HW_DIGITAL_GAIN1:
		if (enable) {
			/* Let current gain be 0 to ramp up */
			mt_afe_set_reg(AFE_GAIN1_CUR, 0, 0xFFFFFFFF);
		}
		mt_afe_set_reg(AFE_GAIN1_CON0, enable, 0x1);
		break;
	case MT_AFE_HW_DIGITAL_GAIN2:
		if (enable) {
			/* Let current gain be 0 to ramp up */
			mt_afe_set_reg(AFE_GAIN2_CUR, 0, 0xFFFFFFFF);
		}
		mt_afe_set_reg(AFE_GAIN2_CON0, enable, 0x1);
		break;
	default:
		pr_warn("%s with no match type\n", __func__);
		break;
	}
}

void mt_afe_set_hw_digital_gain(uint32_t gain, int gain_type)
{
	switch (gain_type) {
	case MT_AFE_HW_DIGITAL_GAIN1:
		mt_afe_set_reg(AFE_GAIN1_CON1, gain, 0xffffffff);
		break;
	case MT_AFE_HW_DIGITAL_GAIN2:
		mt_afe_set_reg(AFE_GAIN2_CON1, gain, 0xffffffff);
		break;
	default:
		pr_warn("%s with no match type\n", __func__);
		break;
	}
}

int mt_afe_enable_sinegen_hw(uint32_t connection, uint32_t direction)
{
	if (direction == MT_AFE_MEMIF_DIRECTION_INPUT) {
		switch (connection) {
		case INTER_CONN_I00:
		case INTER_CONN_I01:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x048C2762, 0xffffffff);
			break;
		case INTER_CONN_I02:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x146C2662, 0xffffffff);
			break;
		case INTER_CONN_I03:
		case INTER_CONN_I04:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x24862862, 0xffffffff);
			break;
		case INTER_CONN_I05:
		case INTER_CONN_I06:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x346C2662, 0xffffffff);
			break;
		case INTER_CONN_I07:
		case INTER_CONN_I08:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x446C2662, 0xffffffff);
			break;
		case INTER_CONN_I09:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x546C2662, 0xffffffff);
			break;
		case INTER_CONN_I10:
		case INTER_CONN_I11:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x646C2662, 0xffffffff);
			break;
		case INTER_CONN_I12:
		case INTER_CONN_I13:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x746C2662, 0xffffffff);
			break;
		case INTER_CONN_I15:
		case INTER_CONN_I16:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x946C2662, 0xffffffff);
			break;
		case INTER_CONN_I17:
		case INTER_CONN_I18:
			mt_afe_set_reg(AFE_SGEN_CON0, 0xa46C2662, 0xffffffff);
			break;
		case INTER_CONN_I19:
		case INTER_CONN_I20:
			mt_afe_set_reg(AFE_SGEN_CON0, 0xb46C2662, 0xffffffff);
			break;
		default:
			break;
		}
	} else {
		switch (connection) {
		case INTER_CONN_O00:
		case INTER_CONN_O01:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x0c7c27c2, 0xffffffff);
			break;
		case INTER_CONN_O02:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x1c6c26c2, 0xffffffff);
			break;
		case INTER_CONN_O03:
		case INTER_CONN_O04:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x2c8c28c2, 0xffffffff);
			break;
		case INTER_CONN_O05:
		case INTER_CONN_O06:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x3c6c26c2, 0xffffffff);
			break;
		case INTER_CONN_O07:
		case INTER_CONN_O08:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x4c6c26c2, 0xffffffff);
			break;
		case INTER_CONN_O09:
		case INTER_CONN_O10:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x5c6c26c2, 0xffffffff);
			break;
		case INTER_CONN_O11:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x6c6c26c2, 0xffffffff);
			break;
		case INTER_CONN_O12:
			/* MD connect BT Verify (8K SamplingRate) */
			if (MT_AFE_I2S_SAMPLERATE_8K ==
			    audio_mem_if[MT_AFE_DIGITAL_BLOCK_MEM_MOD_DAI]->sample_rate)
				mt_afe_set_reg(AFE_SGEN_CON0, 0x7c0e80e8, 0xffffffff);
			else if (MT_AFE_I2S_SAMPLERATE_16K ==
				 audio_mem_if[MT_AFE_DIGITAL_BLOCK_MEM_MOD_DAI]->sample_rate)
				mt_afe_set_reg(AFE_SGEN_CON0, 0x7c0f00f0, 0xffffffff);
			else
				mt_afe_set_reg(AFE_SGEN_CON0, 0x7c6c26c2, 0xffffffff);
			break;
		case INTER_CONN_O13:
		case INTER_CONN_O14:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x8c6c26c2, 0xffffffff);
			break;
		case INTER_CONN_O15:
		case INTER_CONN_O16:
			mt_afe_set_reg(AFE_SGEN_CON0, 0x9c6c26c2, 0xffffffff);
			break;
		case INTER_CONN_O17:
		case INTER_CONN_O18:
			mt_afe_set_reg(AFE_SGEN_CON0, 0xac6c26c2, 0xffffffff);
			break;
		case INTER_CONN_O19:
		case INTER_CONN_O20:
			mt_afe_set_reg(AFE_SGEN_CON0, 0xbc6c26c2, 0xffffffff);
			break;
		case INTER_CONN_O21:
		case INTER_CONN_O22:
			mt_afe_set_reg(AFE_SGEN_CON0, 0xcc6c26c2, 0xffffffff);
			break;
		default:
			break;
		}
	}
	return 0;
}

int mt_afe_disable_sinegen_hw(void)
{
	/* don't set [31:28] as 0 when disable sinetone HW */
	/* because it will repalce i00/i01 input with sine gen output. */
	/* Set 0xf is correct way to disconnect sinetone HW to any I/O. */
	mt_afe_set_reg(AFE_SGEN_CON0, 0xf0000000, 0xffffffff);
	return 0;
}

void mt_afe_set_memif_fetch_format(uint32_t interface_type, uint32_t fetch_format)
{
	audio_mem_if[interface_type]->fetch_format_per_sample = fetch_format;

	switch (interface_type) {
	case MT_AFE_DIGITAL_BLOCK_MEM_DL1:
		mt_afe_set_reg(AFE_MEMIF_PBUF_SIZE, fetch_format << 16, 0x00030000);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_DL1_DATA2:
		mt_afe_set_reg(AFE_MEMIF_PBUF_SIZE, fetch_format << 12, 0x00003000);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_DL2:
		mt_afe_set_reg(AFE_MEMIF_PBUF_SIZE, fetch_format << 18, 0x000C0000);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_AWB:
		mt_afe_set_reg(AFE_MEMIF_PBUF_SIZE, fetch_format << 20, 0x00300000);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_VUL:
		mt_afe_set_reg(AFE_MEMIF_PBUF_SIZE, fetch_format << 22, 0x00C00000);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_VUL_DATA2:
		mt_afe_set_reg(AFE_MEMIF_PBUF_SIZE, fetch_format << 14, 0x0000C000);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_DAI:
		mt_afe_set_reg(AFE_MEMIF_PBUF_SIZE, fetch_format << 24, 0x03000000);
		break;
	case MT_AFE_DIGITAL_BLOCK_MEM_MOD_DAI:
		mt_afe_set_reg(AFE_MEMIF_PBUF_SIZE, fetch_format << 26, 0x0C000000);
		break;
	case MT_AFE_DIGITAL_BLOCK_HDMI:
		mt_afe_set_reg(AFE_MEMIF_PBUF_SIZE, fetch_format << 28, 0x30000000);
		break;
	default:
		break;
	}
}

void mt_afe_set_out_conn_format(uint32_t connection_format, uint32_t output)
{
	mt_afe_set_reg(AFE_CONN_24BIT, (connection_format << output), (1 << output));
}

void mt_afe_enable_apll(uint32_t sample_rate)
{
	if (MT_AFE_APLL1 == (mt_afe_get_apll_by_rate(sample_rate)))
		mt_afe_apll22m_clk_on();
	else
		mt_afe_apll24m_clk_on();
}

void mt_afe_disable_apll(uint32_t sample_rate)
{
	if (MT_AFE_APLL1 == (mt_afe_get_apll_by_rate(sample_rate)))
		mt_afe_apll22m_clk_off();
	else
		mt_afe_apll24m_clk_off();
}

void mt_afe_enable_apll_tuner(uint32_t sample_rate)
{
	if (MT_AFE_APLL1 == (mt_afe_get_apll_by_rate(sample_rate)))
		mt_afe_apll1tuner_clk_on();
	else
		mt_afe_apll2tuner_clk_on();
}

void mt_afe_disable_apll_tuner(uint32_t sample_rate)
{
	if (MT_AFE_APLL1 == (mt_afe_get_apll_by_rate(sample_rate)))
		mt_afe_apll1tuner_clk_off();
	else
		mt_afe_apll2tuner_clk_off();
}

void mt_afe_enable_apll_div_power(uint32_t clock_type, uint32_t sample_rate)
{
	uint32_t apll_type = mt_afe_get_apll_by_rate(sample_rate);

	apll_clock_divider_power_refcount[clock_type]++;
	if (apll_clock_divider_power_refcount[clock_type] > 1)
		return;

	switch (clock_type) {
	case MT_AFE_ENGEN:
		if (apll_type == MT_AFE_APLL1)
			mt_afe_enable_i2s_div_power(MT_AFE_APLL1_DIV0);
		else
			mt_afe_enable_i2s_div_power(MT_AFE_APLL2_DIV0);
		break;
	case MT_AFE_I2S0:
		if (apll_type == MT_AFE_APLL1)
			mt_afe_enable_i2s_div_power(MT_AFE_APLL1_DIV1);
		else
			mt_afe_enable_i2s_div_power(MT_AFE_APLL2_DIV1);
		break;
	case MT_AFE_I2S1:
		if (apll_type == MT_AFE_APLL1)
			mt_afe_enable_i2s_div_power(MT_AFE_APLL1_DIV2);
		else
			mt_afe_enable_i2s_div_power(MT_AFE_APLL2_DIV2);
		break;
	case MT_AFE_I2S2:
		if (apll_type == MT_AFE_APLL1)
			mt_afe_enable_i2s_div_power(MT_AFE_APLL1_DIV3);
		else
			mt_afe_enable_i2s_div_power(MT_AFE_APLL2_DIV3);
		break;
	case MT_AFE_I2S3:
		if (apll_type == MT_AFE_APLL1)
			mt_afe_enable_i2s_div_power(MT_AFE_APLL1_DIV4);
		else
			mt_afe_enable_i2s_div_power(MT_AFE_APLL2_DIV4);
		break;
	case MT_AFE_I2S3_BCK:
		if (apll_type == MT_AFE_APLL1)
			mt_afe_enable_i2s_div_power(MT_AFE_APLL1_DIV5);
		else
			mt_afe_enable_i2s_div_power(MT_AFE_APLL2_DIV5);
		break;
	case MT_AFE_SPDIF:
		mt_afe_enable_i2s_div_power(MT_AFE_SPDIF_DIV);
		break;
	case MT_AFE_SPDIF2:
		mt_afe_enable_i2s_div_power(MT_AFE_SPDIF2_DIV);
		break;
	default:
		break;
	}
}

void mt_afe_disable_apll_div_power(uint32_t clock_type, uint32_t sample_rate)
{
	uint32_t apll_type = mt_afe_get_apll_by_rate(sample_rate);

	apll_clock_divider_power_refcount[clock_type]--;
	if (apll_clock_divider_power_refcount[clock_type] > 0) {
		return;
	} else if (apll_clock_divider_power_refcount[clock_type] < 0) {
		pr_warn("%s unexpected refcount(%u,%d)\n", __func__, clock_type,
			apll_clock_divider_power_refcount[clock_type]);
		apll_clock_divider_power_refcount[clock_type] = 0;
		return;
	}

	switch (clock_type) {
	case MT_AFE_ENGEN:
		if (apll_type == MT_AFE_APLL1)
			mt_afe_disable_i2s_div_power(MT_AFE_APLL1_DIV0);
		else
			mt_afe_disable_i2s_div_power(MT_AFE_APLL2_DIV0);
		break;
	case MT_AFE_I2S0:
		if (apll_type == MT_AFE_APLL1)
			mt_afe_disable_i2s_div_power(MT_AFE_APLL1_DIV1);
		else
			mt_afe_disable_i2s_div_power(MT_AFE_APLL2_DIV1);
		break;
	case MT_AFE_I2S1:
		if (apll_type == MT_AFE_APLL1)
			mt_afe_disable_i2s_div_power(MT_AFE_APLL1_DIV2);
		else
			mt_afe_disable_i2s_div_power(MT_AFE_APLL2_DIV2);
		break;
	case MT_AFE_I2S2:
		if (apll_type == MT_AFE_APLL1)
			mt_afe_disable_i2s_div_power(MT_AFE_APLL1_DIV3);
		else
			mt_afe_disable_i2s_div_power(MT_AFE_APLL2_DIV3);
		break;
	case MT_AFE_I2S3:
		if (apll_type == MT_AFE_APLL1)
			mt_afe_disable_i2s_div_power(MT_AFE_APLL1_DIV4);
		else
			mt_afe_disable_i2s_div_power(MT_AFE_APLL2_DIV4);
		break;
	case MT_AFE_I2S3_BCK:
		if (apll_type == MT_AFE_APLL1)
			mt_afe_disable_i2s_div_power(MT_AFE_APLL1_DIV5);
		else
			mt_afe_disable_i2s_div_power(MT_AFE_APLL2_DIV5);
		break;
	case MT_AFE_SPDIF:
		mt_afe_disable_i2s_div_power(MT_AFE_SPDIF_DIV);
		break;
	case MT_AFE_SPDIF2:
		mt_afe_disable_i2s_div_power(MT_AFE_SPDIF2_DIV);
		break;
	default:
		break;
	}
}

uint32_t mt_afe_set_mclk(uint32_t clock_type, uint32_t sample_rate)
{
	uint32_t apll_type = mt_afe_get_apll_by_rate(sample_rate);
	uint32_t apll_clock = 0;
	uint32_t mclk_div = 0;

	apll_clock =
	    (apll_type == MT_AFE_APLL1) ? MT_AFE_APLL1_CLOCK_FREQ : MT_AFE_APLL2_CLOCK_FREQ;

	/* set up mclk mux select / ck div */
	switch (clock_type) {
	case MT_AFE_ENGEN:
		mclk_div = 7;
		if (apll_type == MT_AFE_APLL1)
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_0, mclk_div << 24, 0x0f000000);
		else
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_0, mclk_div << 28, 0xf0000000);
		break;
	case MT_AFE_I2S0:
		mclk_div = (apll_clock / 256 / sample_rate) - 1;
		if (apll_type == MT_AFE_APLL1) {
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_0, 0 << 4, 1 << 4);
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_1, mclk_div, 0x000000ff);
		} else {
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_0, 1 << 4, 1 << 4);
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_2, mclk_div, 0x000000ff);
		}
		break;
	case MT_AFE_I2S1:
		mclk_div = (apll_clock / 256 / sample_rate) - 1;
		if (apll_type == MT_AFE_APLL1) {
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_0, 0 << 5, 1 << 5);
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_1, mclk_div << 8, 0x0000ff00);
		} else {
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_0, 1 << 5, 1 << 5);
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_2, mclk_div << 8, 0x0000ff00);
		}
		break;
	case MT_AFE_I2S2:
		mclk_div = (apll_clock / 256 / sample_rate) - 1;
		if (apll_type == MT_AFE_APLL1) {
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_0, 0 << 6, 1 << 6);
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_1, mclk_div << 16, 0x00ff0000);
		} else {
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_0, 1 << 6, 1 << 6);
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_2, mclk_div << 16, 0x00ff0000);
		}
		break;
	case MT_AFE_I2S3:
		mclk_div = (apll_clock / 128 / sample_rate) - 1;
		if (apll_type == MT_AFE_APLL1) {
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_0, 0 << 7, 1 << 7);
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_1, mclk_div << 24, 0xff000000);
		} else {
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_0, 1 << 7, 1 << 7);
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_2, mclk_div << 24, 0xff000000);
		}
		break;
	case MT_AFE_SPDIF:
		mclk_div = (apll_clock / 128 / sample_rate) - 1;
		if (apll_type == MT_AFE_APLL1) {
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_0, 0 << 9, 1 << 9);
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_3, mclk_div << 24, 0xff000000);
		} else {
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_0, 1 << 9, 1 << 9);
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_3, mclk_div << 24, 0xff000000);
		}
		break;
	case MT_AFE_SPDIF2:
		mclk_div = (apll_clock / 128 / sample_rate) - 1;
		if (apll_type == MT_AFE_APLL1) {
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_4, 0 << 8, 1 << 8);
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_4, mclk_div, 0x000000ff);
		} else {
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_4, 1 << 8, 1 << 8);
			mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_4, mclk_div, 0x000000ff);
		}
		break;
	default:
		break;
	}

	return mclk_div;
}

void mt_afe_set_8173_mclk(bool enable)
{
	static bool is_mclk_on;

	printk(KERN_INFO "%s enable: %d is_mclk_on: %d cdc_initialized: %d\n",
			__func__, enable, is_mclk_on, cdc_initialized);

	if (!cdc_initialized) {
		return;
	}

	if (enable == is_mclk_on)
		return;

	if (enable) {
		mt_afe_enable_apll(48000);
		mt_afe_enable_apll_tuner(48000);
		mt_afe_set_mclk(MT_AFE_I2S1, 48000);
		mt_afe_set_mclk(MT_AFE_ENGEN, 48000);
		mt_afe_enable_apll_div_power(MT_AFE_I2S1, 48000);
		mt_afe_enable_apll_div_power(MT_AFE_ENGEN, 48000);
		is_mclk_on = true;
	} else {
		mt_afe_disable_apll_div_power(MT_AFE_I2S1, 48000);
		mt_afe_disable_apll_div_power(MT_AFE_ENGEN, 48000);
		mt_afe_disable_apll_tuner(48000);
		mt_afe_disable_apll(48000);
		is_mclk_on = false;
	}

}
EXPORT_SYMBOL_GPL(mt_afe_set_mclk);

void mt_afe_set_init(void)
{
	cdc_initialized = true;
}
EXPORT_SYMBOL_GPL(mt_afe_set_init);

void mt_afe_set_i2s3_bclk(uint32_t mck_div, uint32_t sample_rate, uint32_t channels,
		       uint32_t sample_bits)
{
	uint32_t apll_type = mt_afe_get_apll_by_rate(sample_rate);
	uint32_t apll_clock = 0;
	uint32_t bck = 0;
	uint32_t bck_div = 0;

	apll_clock =
	    (apll_type == MT_AFE_APLL1) ? MT_AFE_APLL1_CLOCK_FREQ : MT_AFE_APLL2_CLOCK_FREQ;

	bck = sample_rate * channels * sample_bits;
	bck_div = ((apll_clock / (mck_div + 1)) / bck) - 1;

	if (apll_type == MT_AFE_APLL1) {
		mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_0, 0 << 8, 1 << 8);
		mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_3, bck_div, 0x0000000f);
	} else {
		mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_0, 1 << 8, 1 << 8);
		mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_3, bck_div << 4, 0x000000f0);
	}
}

void mt_afe_set_pcmif_asrc(struct mt_afe_pcm_info *pcm_info)
{
	switch (pcm_info->mode) {
	case PCM_8K:
		mt_afe_set_reg(AFE_ASRC_CON1, 0x00098580, 0xffffffff);
		mt_afe_set_reg(AFE_ASRC_CON2, 0x00400000, 0xffffffff);
		mt_afe_set_reg(AFE_ASRC_CON3, 0x00400000, 0xffffffff);
		mt_afe_set_reg(AFE_ASRC_CON4, 0x00098580, 0xffffffff);
		mt_afe_set_reg(AFE_ASRC_CON7, 0x0004c2c0, 0xffffffff);
		break;
	case PCM_16K:
		mt_afe_set_reg(AFE_ASRC_CON1, 0x0004c2c0, 0xffffffff);
		mt_afe_set_reg(AFE_ASRC_CON2, 0x00400000, 0xffffffff);
		mt_afe_set_reg(AFE_ASRC_CON3, 0x00400000, 0xffffffff);
		mt_afe_set_reg(AFE_ASRC_CON4, 0x0004c2c0, 0xffffffff);
		mt_afe_set_reg(AFE_ASRC_CON7, 0x00026160, 0xffffffff);
		break;
	case PCM_32K:
		mt_afe_set_reg(AFE_ASRC_CON1, 0x00026160, 0xffffffff);
		mt_afe_set_reg(AFE_ASRC_CON2, 0x00400000, 0xffffffff);
		mt_afe_set_reg(AFE_ASRC_CON3, 0x00400000, 0xffffffff);
		mt_afe_set_reg(AFE_ASRC_CON4, 0x00026160, 0xffffffff);
		mt_afe_set_reg(AFE_ASRC_CON7, 0x000130b0, 0xffffffff);
		break;
	default:
		pr_debug("%s samp_mode err\n", __func__);
		break;
	}
}

void mt_afe_enable_pcmif_asrc(struct mt_afe_pcm_info *pcm_info)
{
	if (pcm_info->mode != PCM_32K) {
		if (pcm_info->vbat_16k_mode == PCM_VBT_16K_MODE_ENABLE)
			mt_afe_set_reg(AFE_ASRC_CON6, 0x005f188f, 0xffffffff);
		else
			mt_afe_set_reg(AFE_ASRC_CON6, 0x00bf188f, 0xffffffff);
		mt_afe_set_reg(AFE_ASRC_CON0, 0x06003031, 0xffffffff);
	} else {
		mt_afe_set_reg(AFE_ASRC_CON6, 0x00bf188f, 0xffffffff);
		mt_afe_set_reg(AFE_ASRC_CON0, 0x06003031, 0xffffffff);
	}
	if (pcm_info->bit24 == PCM_16BIT) {
		mt_afe_set_reg(AFE_ASRC_CON0, 0x1 << 31, 0x1 << 31);
		mt_afe_set_reg(AFE_ASRC_CON0, 0x1 << 19, 0x1 << 19);
	} else {
		mt_afe_set_reg(AFE_ASRC_CON0, 0x0 << 31, 0x1 << 31);
		mt_afe_set_reg(AFE_ASRC_CON0, 0x0 << 19, 0x1 << 19);
	}
}

void mt_afe_disable_pcmif_asrc(void)
{
	mt_afe_set_reg(AFE_ASRC_CON6, 0x00000000, 0xffffffff);
	mt_afe_set_reg(AFE_ASRC_CON0, 0x06003030, 0xffffffff);
}

void mt_afe_set_pcmif(struct mt_afe_pcm_info *pcm_info)
{
	unsigned int reg_pcm_intf_con1_val = 0;

	mt_afe_set_reg(PCM_INTF_CON1, 0, 0xffffffff);
	reg_pcm_intf_con1_val |= (pcm_info->fmt & 0x03) << 1;
	reg_pcm_intf_con1_val |= (pcm_info->mode & 0x03) << 3;
	reg_pcm_intf_con1_val |= (pcm_info->slave & 0x01) << 5;
	reg_pcm_intf_con1_val |= (pcm_info->byp_asrc & 0x01) << 6;
	reg_pcm_intf_con1_val |= (pcm_info->bt_mode & 0x01) << 7;
	reg_pcm_intf_con1_val |= (pcm_info->sync_type & 0x01) << 8;
	reg_pcm_intf_con1_val |= (pcm_info->sync_length & 0x3e) << 9;
	reg_pcm_intf_con1_val |= (pcm_info->wlen & 0x03) << 14;
	reg_pcm_intf_con1_val |= (pcm_info->bit24 & 0x01) << 16;
	reg_pcm_intf_con1_val |= (pcm_info->ext_modem & 0x01) << 17;
	reg_pcm_intf_con1_val |= (pcm_info->vbat_16k_mode & 0x01) << 18;
	reg_pcm_intf_con1_val |= (pcm_info->tx_lch_rpt & 0x01) << 19;
	reg_pcm_intf_con1_val |= (pcm_info->bck_in_inv & 0x01) << 20;
	reg_pcm_intf_con1_val |= (pcm_info->sync_in_inv & 0x01) << 21;
	reg_pcm_intf_con1_val |= (pcm_info->bck_out_inv & 0x01) << 22;
	reg_pcm_intf_con1_val |= (pcm_info->sync_out_inv & 0x01) << 23;
	mt_afe_set_reg(PCM_INTF_CON1, reg_pcm_intf_con1_val, 0xffffffff);
}

void mt_afe_enable_pcmif(bool enable)
{
	if (enable) {
		mt_afe_set_reg(PCM_INTF_CON1, 0x1 << 0, 0x1 << 0);
	} else {
		mt_afe_set_reg(PCM_INTF_CON1, 0x0 << 0, 0x1 << 0);
	}
}
void mt_afe_set_dai_bt(struct mt_afe_digital_dai_bt *dai_bt)
{
	audio_dai_bt->use_mrgif_input = dai_bt->use_mrgif_input;
	audio_dai_bt->dai_bt_mode = dai_bt->dai_bt_mode;
	audio_dai_bt->dai_del = dai_bt->dai_del;
	audio_dai_bt->bt_len = dai_bt->bt_len;
	audio_dai_bt->data_rdy = dai_bt->data_rdy;
	audio_dai_bt->bt_sync = dai_bt->bt_sync;
}

int mt_afe_enable_dai_bt(void)
{
	mt_afe_set_reg(AFE_DAIBT_CON0, (audio_dai_bt->dai_bt_mode ? 1 : 0) << 9, 0x1 << 9);
	if (audio_mrg->mrgif_en == true) {
		/* use merge */
		mt_afe_set_reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12);
		/* data ready */
		mt_afe_set_reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);
		/* turn on DAIBT */
		mt_afe_set_reg(AFE_DAIBT_CON0, 0x3, 0x3);
	} else {
		/* set Mrg_I2S Samping Rate */
		mt_afe_set_reg(AFE_MRGIF_CON, audio_mrg->mrg_i2s_sample_rate << 20,
				0xF00000);
		/* set Mrg_I2S enable */
		mt_afe_set_reg(AFE_MRGIF_CON, 1 << 16, 1 << 16);
		/* turn on Merge Interface */
		mt_afe_set_reg(AFE_MRGIF_CON, 1, 0x1);
		udelay(100);
		/* use merge */
		mt_afe_set_reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12);
		/* data ready */
		mt_afe_set_reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);
		/* turn on DAIBT */
		mt_afe_set_reg(AFE_DAIBT_CON0, 0x3, 0x3);
	}
	audio_dai_bt->bt_on = true;
	audio_dai_bt->dai_bt_on = true;
	audio_mrg->mrgif_en = true;
	return 0;
}

int mt_afe_disable_dai_bt(void)
{
	if (audio_mrg->mergeif_i2s_enable == true) {
		/* turn off DAIBT */
		mt_afe_set_reg(AFE_DAIBT_CON0, 0, 0x3);
	} else {
		/* turn off DAIBT */
		mt_afe_set_reg(AFE_DAIBT_CON0, 0, 0x3);
		udelay(100);
		/* set Mrg_I2S disable */
		mt_afe_set_reg(AFE_MRGIF_CON, 0 << 16, 1 << 16);
		/* turn off Merge Interface */
		mt_afe_set_reg(AFE_MRGIF_CON, 0, 0x1);
		audio_mrg->mrgif_en = false;
	}
	audio_dai_bt->bt_on = false;
	audio_dai_bt->dai_bt_on = false;
	return 0;
}

int mt_afe_enable_merge_i2s(uint32_t sample_rate)
{
	/* To enable MrgI2S */
	if (audio_mrg->mrgif_en == true) {
		/* Merge Interface already turn on. */
		/* if sample Rate change, then it need to restart with new setting */
		if (audio_mrg->mrg_i2s_sample_rate != mt_afe_rate_to_idx(sample_rate)) {
			/* Turn off Merge Interface first to switch I2S sampling rate */
			mt_afe_set_reg(AFE_MRGIF_CON, 0, 1 << 16);
			if (audio_dai_bt->dai_bt_on == true) {
				/* Turn off DAIBT first */
				mt_afe_set_reg(AFE_DAIBT_CON0, 0, 0x1);
			}
			udelay(100);
			/* Turn off Merge Interface */
			mt_afe_set_reg(AFE_MRGIF_CON, 0, 0x1);
			udelay(100);
			/* Turn on Merge Interface */
			mt_afe_set_reg(AFE_MRGIF_CON, 1, 0x1);
			if (audio_dai_bt->dai_bt_on == true) {
				/* use merge */
				mt_afe_set_reg(AFE_DAIBT_CON0,
					       audio_dai_bt->dai_bt_mode << 9,
					       0x1 << 9);
				mt_afe_set_reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12);
				/* data ready */
				mt_afe_set_reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);
				/* Turn on DAIBT */
				mt_afe_set_reg(AFE_DAIBT_CON0, 0x3, 0x3);
			}
			audio_mrg->mrg_i2s_sample_rate = mt_afe_rate_to_idx(sample_rate);
			/* set Mrg_I2S Samping Rate */
			mt_afe_set_reg(AFE_MRGIF_CON, audio_mrg->mrg_i2s_sample_rate << 20,
				    0xF00000);
			/* set Mrg_I2S enable */
			mt_afe_set_reg(AFE_MRGIF_CON, 1 << 16, 1 << 16);
		}
	} else {
		/* turn on merge Interface from off state */
		audio_mrg->mrg_i2s_sample_rate = mt_afe_rate_to_idx(sample_rate);
		/* set Mrg_I2S Samping rates */
		mt_afe_set_reg(AFE_MRGIF_CON, audio_mrg->mrg_i2s_sample_rate << 20,
			       0xF00000);
		/* set Mrg_I2S enable */
		mt_afe_set_reg(AFE_MRGIF_CON, 1 << 16, 1 << 16);
		udelay(100);
		mt_afe_set_reg(AFE_MRGIF_CON, 1, 0x1);	/* Turn on Merge Interface */
		udelay(100);
		if (audio_dai_bt->dai_bt_on == true) {
			/* use merge */
			mt_afe_set_reg(AFE_DAIBT_CON0, audio_dai_bt->dai_bt_mode << 9,
				    0x1 << 9);
			mt_afe_set_reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12);
			/* data ready */
			mt_afe_set_reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);
			/* Turn on DAIBT */
			mt_afe_set_reg(AFE_DAIBT_CON0, 0x3, 0x3);
		}
	}
	audio_mrg->mrgif_en = true;
	audio_mrg->mergeif_i2s_enable = true;
	return 0;
}

int mt_afe_disable_merge_i2s(void)
{
	if (audio_mrg->mrgif_en == true) {
		/* turn off I2S */
		mt_afe_set_reg(AFE_MRGIF_CON, 0, 1 << 16);
		if (audio_dai_bt->dai_bt_on == false) {
			udelay(100);
			/* turn off Merge Interface */
			mt_afe_set_reg(AFE_MRGIF_CON, 0, 0x1);
			audio_mrg->mrgif_en = false;
		}
	}
	audio_mrg->mergeif_i2s_enable = false;
	return 0;
}

void mt_afe_suspend(void)
{
	if (aud_drv_suspend_status)
		return;

	pr_debug("+%s\n", __func__);

	mt_afe_store_reg(&suspend_reg);

	mt_afe_suspend_clk_off();

	aud_drv_suspend_status = true;

	pr_debug("-%s\n", __func__);
}

void mt_afe_resume(void)
{
	if (!aud_drv_suspend_status)
		return;

	pr_debug("+%s\n", __func__);

	mt_afe_suspend_clk_on();

	mt_afe_recover_reg(&suspend_reg);

	aud_drv_suspend_status = false;

	pr_debug("-%s\n", __func__);
}

struct mt_afe_mem_control_t *mt_afe_get_mem_ctx(enum mt_afe_mem_context mem_context)
{
	if (mem_context >= 0 && mem_context < MT_AFE_MEM_CTX_COUNT)
		return afe_mem_control_context[mem_context];

	pr_err("%s out of boundary\n", __func__);
	return NULL;
}

void mt_afe_add_ctx_substream(enum mt_afe_mem_context mem_context,
			 struct snd_pcm_substream *substream)
{
	if (likely(mem_context < MT_AFE_MEM_CTX_COUNT))
		afe_mem_control_context[mem_context]->substream = substream;
	else
		pr_err("%s unexpected mem_context = %d\n", __func__, mem_context);
}

void mt_afe_remove_ctx_substream(enum mt_afe_mem_context mem_context)
{
	if (likely(mem_context < MT_AFE_MEM_CTX_COUNT))
		afe_mem_control_context[mem_context]->substream = NULL;
	else
		pr_err("%s unexpected mem_context = %d\n", __func__, mem_context);
}

void mt_afe_init_dma_buffer(enum mt_afe_mem_context mem_context,
			struct snd_pcm_runtime *runtime)
{
	struct mt_afe_block_t *block;
	unsigned int memory_addr_bit33 = 0;

	if (mem_context >= MT_AFE_MEM_CTX_COUNT)
		return;

	if (sizeof(dma_addr_t) > 4)
		memory_addr_bit33 = (runtime->dma_addr & 0x100000000) ? 1 : 0;

	block = &(mt_afe_get_mem_ctx(mem_context)->block);

	block->phy_buf_addr = runtime->dma_addr & 0xFFFFFFFF;
	block->virtual_buf_addr = runtime->dma_area;
	block->buffer_size = runtime->dma_bytes;
	block->data_remained = 0;
	block->write_index = 0;
	block->read_index = 0;
	block->iec_nsadr = 0;

	switch (mem_context) {
	case MT_AFE_MEM_CTX_DL1:
		mt_afe_set_reg(AFE_DL1_BASE, block->phy_buf_addr, 0xffffffff);
		mt_afe_set_reg(AFE_DL1_END, block->phy_buf_addr + (block->buffer_size - 1),
			0xffffffff);
		mt_afe_set_reg(AFE_MEMIF_MSB, memory_addr_bit33, 0x1);
		break;
	case MT_AFE_MEM_CTX_DL2:
		mt_afe_set_reg(AFE_DL2_BASE, block->phy_buf_addr, 0xffffffff);
		mt_afe_set_reg(AFE_DL2_END, block->phy_buf_addr + (block->buffer_size - 1),
			0xffffffff);
		mt_afe_set_reg(AFE_MEMIF_MSB, memory_addr_bit33 << 1, 1 << 1);
		break;
	case MT_AFE_MEM_CTX_VUL:
		mt_afe_set_reg(AFE_VUL_BASE, block->phy_buf_addr, 0xffffffff);
		mt_afe_set_reg(AFE_VUL_END, block->phy_buf_addr + (block->buffer_size - 1),
			0xffffffff);
		mt_afe_set_reg(AFE_MEMIF_MSB, memory_addr_bit33 << 6, 1 << 6);
		break;
	case MT_AFE_MEM_CTX_VUL2:
		mt_afe_set_reg(AFE_VUL_D2_BASE, block->phy_buf_addr, 0xffffffff);
		mt_afe_set_reg(AFE_VUL_D2_END, block->phy_buf_addr + (block->buffer_size - 1),
			0xffffffff);
		mt_afe_set_reg(AFE_MEMIF_MSB, memory_addr_bit33 << 7, 1 << 7);
		break;
	case MT_AFE_MEM_CTX_DAI:
		mt_afe_set_reg(AFE_DAI_BASE, block->phy_buf_addr, 0xffffffff);
		mt_afe_set_reg(AFE_DAI_END, block->phy_buf_addr + (block->buffer_size - 1),
			0xffffffff);
		mt_afe_set_reg(AFE_MEMIF_MSB, memory_addr_bit33 << 5, 1 << 5);
		break;
	case MT_AFE_MEM_CTX_MOD_DAI:
		mt_afe_set_reg(AFE_MOD_DAI_BASE, block->phy_buf_addr, 0xffffffff);
		mt_afe_set_reg(AFE_MOD_DAI_END, block->phy_buf_addr + (block->buffer_size - 1),
			0xffffffff);
		mt_afe_set_reg(AFE_MEMIF_MSB, memory_addr_bit33 << 4, 1 << 4);
		break;
	case MT_AFE_MEM_CTX_AWB:
		mt_afe_set_reg(AFE_AWB_BASE, block->phy_buf_addr, 0xffffffff);
		mt_afe_set_reg(AFE_AWB_END, block->phy_buf_addr + (block->buffer_size - 1),
			0xffffffff);
		mt_afe_set_reg(AFE_MEMIF_MSB, memory_addr_bit33 << 3, 1 << 3);
		break;
	case MT_AFE_MEM_CTX_HDMI:
		mt_afe_set_reg(AFE_HDMI_OUT_BASE, block->phy_buf_addr, 0xffffffff);
		mt_afe_set_reg(AFE_HDMI_OUT_END, block->phy_buf_addr + (block->buffer_size - 1),
			0xffffffff);
		mt_afe_set_reg(AFE_MEMIF_MSB, memory_addr_bit33 << 8, 1 << 8);
		break;
	case MT_AFE_MEM_CTX_HDMI_RAW:
		mt_afe_set_reg(AFE_SPDIF_BASE, block->phy_buf_addr, 0xffffffff);
		mt_afe_set_reg(AFE_SPDIF_END, block->phy_buf_addr + block->buffer_size,
			0xffffffff);
		mt_afe_set_reg(AFE_MEMIF_MSB, memory_addr_bit33 << 9, 1 << 9);
		break;
	case MT_AFE_MEM_CTX_SPDIF:
		mt_afe_set_reg(AFE_SPDIF2_BASE, block->phy_buf_addr, 0xffffffff);
		mt_afe_set_reg(AFE_SPDIF2_END, block->phy_buf_addr + block->buffer_size,
			0xffffffff);
		mt_afe_set_reg(AFE_MEMIF_MSB, memory_addr_bit33 << 10, 1 << 10);
		break;
	default:
		break;
	}
}

void mt_afe_reset_dma_buffer(enum mt_afe_mem_context mem_context)
{

	if (likely(mem_context < MT_AFE_MEM_CTX_COUNT)) {
		struct mt_afe_block_t *afe_block = &(afe_mem_control_context[mem_context]->block);

		if (afe_block) {
			memset_io(afe_block->virtual_buf_addr, 0, afe_block->buffer_size);
			afe_block->read_index = 0;
			afe_block->write_index = 0;
			afe_block->data_remained = 0;
		}
	} else
		pr_err("%s unexpected mem_context = %d\n", __func__, mem_context);
}

int mt_afe_update_hw_ptr(enum mt_afe_mem_context mem_context)
{
	struct mt_afe_block_t *afe_block;
	struct snd_pcm_runtime *runtime;
	int rc = 0;

	if (mem_context >= MT_AFE_MEM_CTX_COUNT || mem_context < 0)
		return rc;

	afe_block = &(afe_mem_control_context[mem_context]->block);
	runtime = afe_mem_control_context[mem_context]->substream->runtime;

	switch (mem_context) {
	case MT_AFE_MEM_CTX_DL1:
	case MT_AFE_MEM_CTX_DL2:
	case MT_AFE_MEM_CTX_HDMI:
	case MT_AFE_MEM_CTX_HDMI_RAW:
	case MT_AFE_MEM_CTX_SPDIF:
		rc = bytes_to_frames(runtime, afe_block->read_index);
		break;
	case MT_AFE_MEM_CTX_VUL:
	case MT_AFE_MEM_CTX_VUL2:
	case MT_AFE_MEM_CTX_DAI:
	case MT_AFE_MEM_CTX_MOD_DAI:
	case MT_AFE_MEM_CTX_AWB:
		rc = bytes_to_frames(runtime, afe_block->write_index);
		break;
	default:
		break;
	}

	return rc;
}

unsigned int mt_afe_get_board_channel_type(void)
{
	return board_channel_type;
}

void mt_afe_set_hdmi_out_channel(unsigned int channels)
{
	unsigned int register_value = 0;

	register_value |= (channels << 4);
	mt_afe_set_reg(AFE_HDMI_OUT_CON0, register_value, 0x000000F0);
}

int mt_afe_enable_hdmi_out(void)
{
	mt_afe_set_reg(AFE_HDMI_OUT_CON0, 0x1, 0x1);
	return 0;
}

int mt_afe_disable_hdmi_out(void)
{
	mt_afe_set_reg(AFE_HDMI_OUT_CON0, 0x0, 0x1);
	return 0;
}

void mt_afe_set_hdmi_tdm1_config(unsigned int channels, unsigned int i2s_wlen)
{
	unsigned int register_value = 0;

	register_value |= (MT_AFE_TDM_BCK_INVERSE << 1);
	register_value |= (MT_AFE_TDM_LRCK_INVERSE << 2);
	register_value |= (MT_AFE_TDM_1_BCK_CYCLE_DELAY << 3);
	/* aligned for I2S mode */
	register_value |= (MT_AFE_TDM_ALIGNED_TO_MSB << 4);
	register_value |= (MT_AFE_TDM_2CH_FOR_EACH_SDATA << 10);

	if (i2s_wlen == MT_AFE_I2S_WLEN_32BITS) {
		register_value |= (MT_AFE_TDM_WLLEN_32BIT << 8);
		register_value |= (MT_AFE_TDM_32_BCK_CYCLES << 12);
		/* LRCK TDM WIDTH */
		register_value |= (((MT_AFE_TDM_WLLEN_32BIT << 4) - 1) << 24);
	} else {
		register_value |= (MT_AFE_TDM_WLLEN_16BIT << 8);
		register_value |= (MT_AFE_TDM_16_BCK_CYCLES << 12);
		/* LRCK TDM WIDTH */
		register_value |= (((MT_AFE_TDM_WLLEN_16BIT << 4) - 1) << 24);
	}

	mt_afe_set_reg(AFE_TDM_CON1, register_value, 0xFFFFFFFE);
}

void mt_afe_set_hdmi_tdm2_config(unsigned int channels)
{
	unsigned int register_value = 0;

	switch (channels) {
	case 7:
	case 8:
		register_value |= CHANNEL_START_FROM_030_O31;
		register_value |= (CHANNEL_START_FROM_032_O33 << 4);
		register_value |= (CHANNEL_START_FROM_034_O35 << 8);
		register_value |= (CHANNEL_START_FROM_036_O37 << 12);
		break;
	case 5:
	case 6:
		register_value |= CHANNEL_START_FROM_030_O31;
		register_value |= (CHANNEL_START_FROM_032_O33 << 4);
		register_value |= (CHANNEL_START_FROM_034_O35 << 8);
		register_value |= (CHANNEL_DATA_IS_ZERO << 12);
		break;
	case 3:
	case 4:
		register_value |= CHANNEL_START_FROM_030_O31;
		register_value |= (CHANNEL_START_FROM_032_O33 << 4);
		register_value |= (CHANNEL_DATA_IS_ZERO << 8);
		register_value |= (CHANNEL_DATA_IS_ZERO << 12);
		break;
	case 1:
	case 2:
		register_value |= CHANNEL_START_FROM_030_O31;
		register_value |= (CHANNEL_DATA_IS_ZERO << 4);
		register_value |= (CHANNEL_DATA_IS_ZERO << 8);
		register_value |= (CHANNEL_DATA_IS_ZERO << 12);
		break;
	default:
		return;
	}

	mt_afe_set_reg(AFE_TDM_CON2, register_value, 0x0000FFFF);
}

int mt_afe_enable_hdmi_tdm(void)
{
	mt_afe_set_reg(AFE_TDM_CON1, 0x1, 0x1);
	return 0;
}

int mt_afe_disable_hdmi_tdm(void)
{
	mt_afe_set_reg(AFE_TDM_CON1, 0x0, 0x1);
	return 0;
}

int mt_afe_enable_hdmi_tdm_i2s_loopback(void)
{
	mt_afe_set_reg(AFE_TDM_CON2, 1 << 20, 1 << 20);
	return 0;
}

int mt_afe_disable_hdmi_tdm_i2s_loopback(void)
{
	mt_afe_set_reg(AFE_TDM_CON2, 0 << 20, 1 << 20);
	return 0;
}

void mt_afe_set_hdmi_tdm_i2s_loopback_data(unsigned int sdata_index)
{
	if (sdata_index < 4)
		mt_afe_set_reg(AFE_TDM_CON2, sdata_index << 21, 1 << 21);
}


/*
 *    static function implementation
 */

static void mt_afe_init_control(void *dev)
{
	int i = 0;

	audio_mrg = devm_kzalloc(dev, sizeof(struct mt_afe_merge_interface), GFP_KERNEL);
	audio_dai_bt = devm_kzalloc(dev, sizeof(struct mt_afe_digital_dai_bt), GFP_KERNEL);

	for (i = 0; i < MT_AFE_IRQ_MCU_MODE_NUM; i++)
		audio_mcu_mode[i] = devm_kzalloc(dev, sizeof(struct mt_afe_irq_status),
						 GFP_KERNEL);

	for (i = 0; i < MT_AFE_DIGITAL_BLOCK_NUM; i++)
		audio_mem_if[i] =
		    devm_kzalloc(dev, sizeof(struct mt_afe_mem_if_attribute), GFP_KERNEL);

	for (i = 0; i < MT_AFE_MEM_CTX_COUNT; i++)
		afe_mem_control_context[i] =
		    devm_kzalloc(dev, sizeof(struct mt_afe_mem_control_t), GFP_KERNEL);

	audio_mrg->mrg_i2s_sample_rate = mt_afe_rate_to_idx(44100);

	mt_afe_main_clk_on();

	/* power down all dividers */
	for (i = MT_AFE_APLL1_DIV0; i < MT_AFE_APLL_DIV_COUNT; i++)
		mt_afe_disable_i2s_div_power(i);

	mt_afe_set_reg(AFE_IRQ_MCU_EN, 1 << 2, 1 << 2);

	mt_afe_main_clk_off();
}

static int mt_afe_register_irq(void *dev)
{
	const int ret = request_irq(audio_irq_id, mt_afe_irq_handler,
				IRQF_TRIGGER_LOW, "Afe_ISR_Handle", dev);
	if (unlikely(ret < 0))
		pr_err("%s %d\n", __func__, ret);

	return ret;
}

static irqreturn_t mt_afe_irq_handler(int irq, void *dev_id)
{
	const uint32_t reg_value = (mt_afe_get_reg(AFE_IRQ_MCU_STATUS) & IRQ_STATUS_BIT);

	if (unlikely(reg_value == 0)) {
		pr_warn("%s reg_value = 0\n", __func__);
		goto irq_handler_exit;
	}

	if (reg_value & MT_AFE_IRQ1_MCU)
		mt_afe_dl_interrupt_handler();

	if (reg_value & MT_AFE_IRQ2_MCU)
		mt_afe_ul_interrupt_handler();

	if (reg_value & MT_AFE_IRQ7_MCU)
		mt_afe_dl2_interrupt_handler();

	if (reg_value & MT_AFE_IRQ5_MCU)
		mt_afe_hdmi_interrupt_handler();

	if (reg_value & MT_AFE_IRQ6_MCU)
		mt_afe_hdmi_raw_interrupt_handler();

	if (reg_value & MT_AFE_IRQ8_MCU)
		mt_afe_spdif_interrupt_handler();

	/* clear irq */
	mt_afe_set_reg(AFE_IRQ_MCU_CLR, reg_value, 0xff);

irq_handler_exit:
	return IRQ_HANDLED;
}

static uint32_t mt_afe_rate_to_idx(uint32_t sample_rate)
{
	switch (sample_rate) {
	case 8000:
		return MT_AFE_I2S_SAMPLERATE_8K;
	case 11025:
		return MT_AFE_I2S_SAMPLERATE_11K;
	case 12000:
		return MT_AFE_I2S_SAMPLERATE_12K;
	case 16000:
		return MT_AFE_I2S_SAMPLERATE_16K;
	case 22050:
		return MT_AFE_I2S_SAMPLERATE_22K;
	case 24000:
		return MT_AFE_I2S_SAMPLERATE_24K;
	case 32000:
		return MT_AFE_I2S_SAMPLERATE_32K;
	case 44100:
		return MT_AFE_I2S_SAMPLERATE_44K;
	case 48000:
		return MT_AFE_I2S_SAMPLERATE_48K;
	case 88000:
		return MT_AFE_I2S_SAMPLERATE_88K;
	case 96000:
		return MT_AFE_I2S_SAMPLERATE_96K;
	case 174000:
		return MT_AFE_I2S_SAMPLERATE_174K;
	case 192000:
		return MT_AFE_I2S_SAMPLERATE_192K;
	default:
		break;
	}
	return MT_AFE_I2S_SAMPLERATE_44K;
}

#ifdef DEBUG_IRQ_STATUS
static unsigned int gpt_cnt_to_ms(unsigned int cnt1, unsigned int cnt2)
{
	unsigned int diff;

	if (cnt1 > cnt2)
		diff = cnt1 - cnt2;
	else
		diff = 4294967295 - cnt2 + cnt1;

	return diff / 13000;
}
#endif

static void mt_afe_dl_interrupt_handler(void)
{
	int afe_consumed_bytes;
	int hw_memory_index;
	int hw_cur_read_index = 0;
	struct mt_afe_block_t *const afe_block =
		&(mt_afe_get_mem_ctx(MT_AFE_MEM_CTX_DL1)->block);
	struct snd_pcm_substream *substream =
		mt_afe_get_mem_ctx(MT_AFE_MEM_CTX_DL1)->substream;

	if (!substream)
		return;

#ifdef DEBUG_IRQ_STATUS
	{
		unsigned int current_gpt_cnt;

		irq1_counter++;
		gpt_get_cnt(GPT2, &current_gpt_cnt);
		if (gpt_cnt_to_ms(current_gpt_cnt, pre_irq1_gpt_cnt) > 50) {
			pr_warn("%s[%llu] irq1 diff: %u ms\n", __func__, irq1_counter,
				gpt_cnt_to_ms(current_gpt_cnt, pre_irq1_gpt_cnt));
		}
	}
#endif

	hw_cur_read_index = mt_afe_get_reg(AFE_DL1_CUR);

	if (hw_cur_read_index == 0) {
		pr_warn("%s hw_cur_read_index == 0\n", __func__);
		hw_cur_read_index = afe_block->phy_buf_addr;
	}

	hw_memory_index = (hw_cur_read_index - afe_block->phy_buf_addr);

	/*
	   pr_debug("%s hw_cur_read_index = 0x%x hw_memory_index = 0x%x addr = 0x%x\n",
	   __func__, hw_cur_read_index, hw_memory_index, afe_block->physical_buffer_addr);
	 */

	/* get hw consume bytes */
	if (hw_memory_index > afe_block->read_index) {
		afe_consumed_bytes = hw_memory_index - afe_block->read_index;
	} else {
		afe_consumed_bytes =
		    afe_block->buffer_size + hw_memory_index - afe_block->read_index;
	}

	if (unlikely((afe_consumed_bytes & 0x7) != 0))
		pr_warn("%s DMA address is not aligned 8 bytes (%d)\n", __func__,
			afe_consumed_bytes);

#ifdef AUDIO_MEMORY_SRAM
	afe_consumed_bytes = afe_consumed_bytes & (~63);
#endif

	afe_block->read_index += afe_consumed_bytes;
	afe_block->read_index %= afe_block->buffer_size;

	snd_pcm_period_elapsed(substream);

#ifdef DEBUG_IRQ_STATUS
	gpt_get_cnt(GPT2, &pre_irq1_gpt_cnt);
#endif
}

static void mt_afe_dl2_interrupt_handler(void)
{
	int afe_consumed_bytes;
	int hw_memory_index;
	int hw_cur_read_index = 0;
	struct mt_afe_block_t *const afe_block =
		&(mt_afe_get_mem_ctx(MT_AFE_MEM_CTX_DL2)->block);
	struct snd_pcm_substream *substream =
		mt_afe_get_mem_ctx(MT_AFE_MEM_CTX_DL2)->substream;

	if (!substream)
		return;

	hw_cur_read_index = mt_afe_get_reg(AFE_DL2_CUR);

	if (hw_cur_read_index == 0) {
		pr_warn("%s hw_cur_read_index == 0\n", __func__);
		hw_cur_read_index = afe_block->phy_buf_addr;
	}

	hw_memory_index = (hw_cur_read_index - afe_block->phy_buf_addr);

	/*
	   pr_debug("%s hw_cur_read_index = 0x%x hw_memory_index = 0x%x addr = 0x%x\n",
	   __func__, hw_cur_read_index, hw_memory_index, afe_block->physical_buffer_addr);
	 */

	/* get hw consume bytes */
	if (hw_memory_index > afe_block->read_index) {
		afe_consumed_bytes = hw_memory_index - afe_block->read_index;
	} else {
		afe_consumed_bytes =
		    afe_block->buffer_size + hw_memory_index - afe_block->read_index;
	}

	if (unlikely((afe_consumed_bytes & 0x7) != 0))
		pr_warn("%s DMA address is not aligned 8 bytes (%d)\n", __func__,
			afe_consumed_bytes);

	afe_block->read_index += afe_consumed_bytes;
	afe_block->read_index %= afe_block->buffer_size;

	snd_pcm_period_elapsed(substream);
}

static void mt_afe_ul_interrupt_handler(void)
{
	/* irq2 ISR handler */
	const uint32_t afe_dac_con0 = mt_afe_get_reg(AFE_DAC_CON0);

	if (afe_dac_con0 & 0x8) {
		/* handle VUL Context */
		mt_afe_handle_mem_context(MT_AFE_MEM_CTX_VUL);
	}
	if (afe_dac_con0 & 0x10) {
		/* handle DAI Context */
		mt_afe_handle_mem_context(MT_AFE_MEM_CTX_DAI);
	}
	if (afe_dac_con0 & 0x80) {
		/* handle MOD DAI Context */
		mt_afe_handle_mem_context(MT_AFE_MEM_CTX_MOD_DAI);
	}
	if (afe_dac_con0 & 0x40) {
		/* handle AWB Context */
		mt_afe_handle_mem_context(MT_AFE_MEM_CTX_AWB);
	}
	if (afe_dac_con0 & 0x200) {
		/* handle VUL2 Context */
		mt_afe_handle_mem_context(MT_AFE_MEM_CTX_VUL2);
	}
}

static void mt_afe_hdmi_interrupt_handler(void)
{
	int afe_consumed_bytes = 0;
	int hw_memory_index = 0;
	int hw_cur_read_index = 0;
	struct mt_afe_block_t *const afe_block =
		&(mt_afe_get_mem_ctx(MT_AFE_MEM_CTX_HDMI)->block);
	struct snd_pcm_substream *substream =
		mt_afe_get_mem_ctx(MT_AFE_MEM_CTX_HDMI)->substream;

	if (!substream)
		return;

	hw_cur_read_index = mt_afe_get_reg(AFE_HDMI_OUT_CUR);
	if (hw_cur_read_index == 0) {
		pr_warn("%s hw_cur_read_index = 0\n", __func__);
		hw_cur_read_index = afe_block->phy_buf_addr;
	}

	hw_memory_index = (hw_cur_read_index - afe_block->phy_buf_addr);

	/*
	   pr_debug("%s hw_cur_read_index = 0x%x hw_memory_index = 0x%x addr = 0x%x\n",
	   __func__, hw_cur_read_index, hw_memory_index, afe_block->physical_buffer_addr);
	 */

	/* get hw consume bytes */
	if (hw_memory_index > afe_block->read_index) {
		afe_consumed_bytes = hw_memory_index - afe_block->read_index;
	} else {
		afe_consumed_bytes =
		    afe_block->buffer_size + hw_memory_index - afe_block->read_index;
	}

	if ((afe_consumed_bytes & 0xf) != 0)
		pr_warn("%s DMA address is not aligned 16 bytes\n", __func__);

	/*
	   pr_debug("%s read_index:%x afe_consumed_bytes:%x hw_memory_index:%x\n",
	   __func__, afe_block->read_index, afe_consumed_bytes, hw_memory_index);
	 */

	afe_block->read_index += afe_consumed_bytes;
	afe_block->read_index %= afe_block->buffer_size;

	snd_pcm_period_elapsed(substream);
}

static void mt_afe_hdmi_raw_interrupt_handler(void)
{
	int afe_consumed_bytes = 0;
	int hw_memory_index = 0;
	int hw_cur_read_index = 0;
	unsigned int burst_len = 0;

	struct mt_afe_block_t *const afe_block =
		&(mt_afe_get_mem_ctx(MT_AFE_MEM_CTX_HDMI_RAW)->block);
	struct snd_pcm_substream *substream =
		mt_afe_get_mem_ctx(MT_AFE_MEM_CTX_HDMI_RAW)->substream;

	if (!substream)
		return;

	if (mt_afe_get_reg(AFE_IEC_BURST_INFO) & 0x000010000) {
		pr_debug("%s HW is Not ready to get next burst info\n", __func__);
		return;
	}

	hw_cur_read_index = mt_afe_get_reg(AFE_SPDIF_CUR);
	if (hw_cur_read_index == 0) {
		pr_warn("%s hw_cur_read_index = 0\n", __func__);
		hw_cur_read_index = afe_block->phy_buf_addr;
	}

	hw_memory_index = (hw_cur_read_index - afe_block->phy_buf_addr);

	/*
	   pr_debug("%s hw_cur_read_index = 0x%x hw_memory_index = 0x%x addr = 0x%x\n",
	   __func__, hw_cur_read_index, hw_memory_index, afe_block->pucPhysBufAddr);
	 */

	/* get hw consume bytes */
	if (hw_memory_index > afe_block->read_index) {
		afe_consumed_bytes = hw_memory_index - afe_block->read_index;
	} else {
		afe_consumed_bytes =
		    afe_block->buffer_size + hw_memory_index - afe_block->read_index;
	}

	/*
	   pr_debug("%s read_index:%x afe_consumed_bytes:%x hw_memory_index:%x\n",
	   __func__, afe_block->read_index, afe_consumed_bytes, hw_memory_index);
	 */

	afe_block->read_index += afe_consumed_bytes;
	afe_block->read_index %= afe_block->buffer_size;

	burst_len = (mt_afe_get_reg(AFE_IEC_BURST_LEN) & 0x0007ffff) >> 3;
	afe_block->iec_nsadr += burst_len;
	if (afe_block->iec_nsadr >= mt_afe_get_reg(AFE_SPDIF_END))
		afe_block->iec_nsadr = mt_afe_get_reg(AFE_SPDIF_BASE);

	/* set NSADR for next period */
	mt_afe_set_reg(AFE_IEC_NSADR, afe_block->iec_nsadr, 0xffffffff);

	/* set IEC data ready bit */
	mt_afe_set_reg(AFE_IEC_BURST_INFO, mt_afe_get_reg(AFE_IEC_BURST_INFO) | (0x1 << 16),
			0xffffffff);

	/*
	   pr_debug("%s burst_len 0x%x iec_nsadr 0x%x read_index 0x%x afe_consumed_bytes 0x%x\n",
	   __func__, burst_len, afe_block->iec_nsadr, afe_block->read_index, afe_consumed_bytes);
	 */

	snd_pcm_period_elapsed(substream);
}

static void mt_afe_spdif_interrupt_handler(void)
{
	int afe_consumed_bytes = 0;
	int hw_memory_index = 0;
	int hw_cur_read_index = 0;
	unsigned int burst_len = 0;

	struct mt_afe_block_t *const afe_block =
		&(mt_afe_get_mem_ctx(MT_AFE_MEM_CTX_SPDIF)->block);
	struct snd_pcm_substream *substream =
		mt_afe_get_mem_ctx(MT_AFE_MEM_CTX_SPDIF)->substream;

	if (!substream)
		return;

	if (mt_afe_get_reg(AFE_IEC2_BURST_INFO) & 0x000010000) {
		pr_debug("%s HW is Not ready to get next burst info\n", __func__);
		return;
	}

	hw_cur_read_index = mt_afe_get_reg(AFE_SPDIF2_CUR);
	if (hw_cur_read_index == 0) {
		pr_warn("%s hw_cur_read_index = 0\n", __func__);
		hw_cur_read_index = afe_block->phy_buf_addr;
	}

	hw_memory_index = (hw_cur_read_index - afe_block->phy_buf_addr);

	/*
	   pr_debug("%s hw_cur_read_index = 0x%x hw_memory_index = 0x%x addr = 0x%x\n",
	   __func__, hw_cur_read_index, hw_memory_index, afe_block->pucPhysBufAddr);
	 */

	/* get hw consume bytes */
	if (hw_memory_index > afe_block->read_index) {
		afe_consumed_bytes = hw_memory_index - afe_block->read_index;
	} else {
		afe_consumed_bytes =
		    afe_block->buffer_size + hw_memory_index - afe_block->read_index;
	}

	/*
	   pr_debug("%s read_index:%x afe_consumed_bytes:%x hw_memory_index:%x\n",
	   __func__, afe_block->read_index, afe_consumed_bytes, hw_memory_index);
	 */

	afe_block->read_index += afe_consumed_bytes;
	afe_block->read_index %= afe_block->buffer_size;

	burst_len = (mt_afe_get_reg(AFE_IEC2_BURST_LEN) & 0x0007ffff) >> 3;
	afe_block->iec_nsadr += burst_len;
	if (afe_block->iec_nsadr >= mt_afe_get_reg(AFE_SPDIF2_END))
		afe_block->iec_nsadr = mt_afe_get_reg(AFE_SPDIF2_BASE);

	/* set NSADR for next period */
	mt_afe_set_reg(AFE_IEC2_NSADR, afe_block->iec_nsadr, 0xffffffff);

	/* set IEC data ready bit */
	mt_afe_set_reg(AFE_IEC2_BURST_INFO, mt_afe_get_reg(AFE_IEC2_BURST_INFO) | (0x1 << 16),
		    0xffffffff);

	/*
	   pr_debug("%s burst_len 0x%x iec_nsadr 0x%x read_index 0x%x afe_consumed_bytes 0x%x\n",
	   __func__, burst_len, afe_block->iec_nsadr, afe_block->read_index, afe_consumed_bytes);
	 */

	snd_pcm_period_elapsed(substream);
}

static void mt_afe_handle_mem_context(enum mt_afe_mem_context mem_context)
{
	uint32_t hw_cur_read_index = 0;
	int hw_get_bytes = 0;
	struct mt_afe_block_t *block = NULL;
	struct snd_pcm_substream *substream =
		mt_afe_get_mem_ctx(mem_context)->substream;

	if (!substream)
		return;

	switch (mem_context) {
	case MT_AFE_MEM_CTX_VUL:
		hw_cur_read_index = mt_afe_get_reg(AFE_VUL_CUR);
		break;
	case MT_AFE_MEM_CTX_DAI:
		hw_cur_read_index = mt_afe_get_reg(AFE_DAI_CUR);
		break;
	case MT_AFE_MEM_CTX_MOD_DAI:
		hw_cur_read_index = mt_afe_get_reg(AFE_MOD_DAI_CUR);
		break;
	case MT_AFE_MEM_CTX_AWB:
		hw_cur_read_index = mt_afe_get_reg(AFE_AWB_CUR);
		break;
	case MT_AFE_MEM_CTX_VUL2:
		hw_cur_read_index = mt_afe_get_reg(AFE_VUL_D2_CUR);
		break;
	default:
		return;
	}

	block = &(afe_mem_control_context[mem_context]->block);

	if (unlikely(hw_cur_read_index == 0))
		return;

	if (unlikely(block->virtual_buf_addr == NULL))
		return;

	/* HW already fill in */
	hw_get_bytes = (hw_cur_read_index - block->phy_buf_addr) - block->write_index;

	if (hw_get_bytes < 0)
		hw_get_bytes += block->buffer_size;

	/*
	   pr_debug("%s hw_get_bytes:%x hw_cur_read_index:%x read_index:%x write_index:0x%x\n",
	   __func__, hw_get_bytes, hw_cur_read_index, block->read_index, block->write_index);
	   pr_debug("%s physical_buffer_addr:%x mem_context = %d\n",
	   __func__, block->physical_buffer_addr, mem_context); */

	block->write_index += hw_get_bytes;
	block->write_index %= block->buffer_size;

	snd_pcm_period_elapsed(substream);
}

static void mt_afe_clean_predistortion(void)
{
	mt_afe_set_reg(AFE_ADDA_PREDIS_CON0, 0, MASK_ALL);
	mt_afe_set_reg(AFE_ADDA_PREDIS_CON1, 0, MASK_ALL);
}

static bool mt_afe_set_dl_src2(uint32_t sample_rate)
{
	uint32_t afe_adda_dl_src2_con0, afe_adda_dl_src2_con1;

	if (likely(sample_rate == 44100))
		afe_adda_dl_src2_con0 = 7;
	else if (sample_rate == 8000)
		afe_adda_dl_src2_con0 = 0;
	else if (sample_rate == 11025)
		afe_adda_dl_src2_con0 = 1;
	else if (sample_rate == 12000)
		afe_adda_dl_src2_con0 = 2;
	else if (sample_rate == 16000)
		afe_adda_dl_src2_con0 = 3;
	else if (sample_rate == 22050)
		afe_adda_dl_src2_con0 = 4;
	else if (sample_rate == 24000)
		afe_adda_dl_src2_con0 = 5;
	else if (sample_rate == 32000)
		afe_adda_dl_src2_con0 = 6;
	else if (sample_rate == 48000)
		afe_adda_dl_src2_con0 = 8;
	else
		afe_adda_dl_src2_con0 = 7;	/* Default 44100 */

	/* ASSERT(0); */
	if (afe_adda_dl_src2_con0 == 0 || afe_adda_dl_src2_con0 == 3) {	/* 8k or 16k voice mode */
		afe_adda_dl_src2_con0 =
		    (afe_adda_dl_src2_con0 << 28) | (0x03 << 24) | (0x03 << 11) | (0x01 << 5);
	} else {
		afe_adda_dl_src2_con0 = (afe_adda_dl_src2_con0 << 28) | (0x03 << 24) | (0x03 << 11);
	}
	/* SA suggest apply -0.3db to audio/speech path */
	/* 2013.02.22 for voice mode degrade 0.3 db */
	afe_adda_dl_src2_con0 = afe_adda_dl_src2_con0 | (0x01 << 1);
	afe_adda_dl_src2_con1 = 0xf74f0000;

	mt_afe_set_reg(AFE_ADDA_DL_SRC2_CON0, afe_adda_dl_src2_con0, MASK_ALL);
	mt_afe_set_reg(AFE_ADDA_DL_SRC2_CON1, afe_adda_dl_src2_con1, MASK_ALL);
	return true;
}

static bool mt_afe_is_memif_enable(void)
{
	int i;

	for (i = 0; i < MT_AFE_DIGITAL_BLOCK_NUM; i++) {
		if ((audio_mem_if[i]->state) == true)
			return true;
	}
	return false;
}

static bool mt_afe_is_ul_memif_enable(void)
{
	if (audio_mem_if[MT_AFE_DIGITAL_BLOCK_MEM_VUL]->state ||
	    audio_mem_if[MT_AFE_DIGITAL_BLOCK_MEM_DAI]->state ||
	    audio_mem_if[MT_AFE_DIGITAL_BLOCK_MEM_AWB]->state ||
	    audio_mem_if[MT_AFE_DIGITAL_BLOCK_MEM_MOD_DAI]->state ||
	    audio_mem_if[MT_AFE_DIGITAL_BLOCK_MEM_VUL_DATA2]->state)
		return true;
	else
		return false;
}

static uint32_t mt_afe_get_apll_by_rate(uint32_t sample_rate)
{
	if (sample_rate == 176400 || sample_rate == 88200 || sample_rate == 44100 ||
	    sample_rate == 22050 || sample_rate == 11025)
		return MT_AFE_APLL1;
	else
		return MT_AFE_APLL2;
}

static void mt_afe_recover_reg(struct mt_afe_suspend_reg *backup_reg)
{
	pr_debug("+%s\n", __func__);

	if (!backup_reg) {
		pr_warn("%s backup_reg is null\n", __func__);
		return;
	}

	mt_afe_main_clk_on();
	/* Digital register setting */
	mt_afe_set_reg(AUDIO_TOP_CON0, backup_reg->reg_AUDIO_TOP_CON0, MASK_ALL);
	mt_afe_set_reg(AUDIO_TOP_CON1, backup_reg->reg_AUDIO_TOP_CON1, MASK_ALL);
	mt_afe_set_reg(AUDIO_TOP_CON2, backup_reg->reg_AUDIO_TOP_CON2, MASK_ALL);
	mt_afe_set_reg(AUDIO_TOP_CON3, backup_reg->reg_AUDIO_TOP_CON3, MASK_ALL);
	mt_afe_set_reg(AFE_DAC_CON0, backup_reg->reg_AFE_DAC_CON0, MASK_ALL);
	mt_afe_set_reg(AFE_DAC_CON1, backup_reg->reg_AFE_DAC_CON1, MASK_ALL);
	mt_afe_set_reg(AFE_I2S_CON, backup_reg->reg_AFE_I2S_CON, MASK_ALL);
	mt_afe_set_reg(AFE_DAIBT_CON0, backup_reg->reg_AFE_DAIBT_CON0, MASK_ALL);

	mt_afe_set_reg(AFE_CONN0, backup_reg->reg_AFE_CONN0, MASK_ALL);
	mt_afe_set_reg(AFE_CONN1, backup_reg->reg_AFE_CONN1, MASK_ALL);
	mt_afe_set_reg(AFE_CONN2, backup_reg->reg_AFE_CONN2, MASK_ALL);
	mt_afe_set_reg(AFE_CONN3, backup_reg->reg_AFE_CONN3, MASK_ALL);
	mt_afe_set_reg(AFE_CONN4, backup_reg->reg_AFE_CONN4, MASK_ALL);
	mt_afe_set_reg(AFE_CONN5, backup_reg->reg_AFE_CONN5, MASK_ALL);
	mt_afe_set_reg(AFE_CONN6, backup_reg->reg_AFE_CONN6, MASK_ALL);
	mt_afe_set_reg(AFE_CONN7, backup_reg->reg_AFE_CONN7, MASK_ALL);
	mt_afe_set_reg(AFE_CONN8, backup_reg->reg_AFE_CONN8, MASK_ALL);
	mt_afe_set_reg(AFE_CONN9, backup_reg->reg_AFE_CONN9, MASK_ALL);
	mt_afe_set_reg(AFE_CONN_24BIT, backup_reg->reg_AFE_CONN_24BIT, MASK_ALL);
	mt_afe_set_reg(AFE_I2S_CON1, backup_reg->reg_AFE_I2S_CON1, MASK_ALL);
	mt_afe_set_reg(AFE_I2S_CON2, backup_reg->reg_AFE_I2S_CON2, MASK_ALL);
	mt_afe_set_reg(AFE_I2S_CON3, backup_reg->reg_AFE_I2S_CON3, MASK_ALL);
	mt_afe_set_reg(AFE_MRGIF_CON, backup_reg->reg_AFE_MRGIF_CON, MASK_ALL);

	mt_afe_set_reg(AFE_DL1_BASE, backup_reg->reg_AFE_DL1_BASE, MASK_ALL);
	mt_afe_set_reg(AFE_DL1_CUR, backup_reg->reg_AFE_DL1_CUR, MASK_ALL);
	mt_afe_set_reg(AFE_DL1_END, backup_reg->reg_AFE_DL1_END, MASK_ALL);
	mt_afe_set_reg(AFE_DL2_BASE, backup_reg->reg_AFE_DL2_BASE, MASK_ALL);
	mt_afe_set_reg(AFE_DL2_CUR, backup_reg->reg_AFE_DL2_CUR, MASK_ALL);
	mt_afe_set_reg(AFE_DL2_END, backup_reg->reg_AFE_DL2_END, MASK_ALL);
	mt_afe_set_reg(AFE_AWB_BASE, backup_reg->reg_AFE_AWB_BASE, MASK_ALL);
	mt_afe_set_reg(AFE_AWB_CUR, backup_reg->reg_AFE_AWB_CUR, MASK_ALL);
	mt_afe_set_reg(AFE_AWB_END, backup_reg->reg_AFE_AWB_END, MASK_ALL);
	mt_afe_set_reg(AFE_VUL_BASE, backup_reg->reg_AFE_VUL_BASE, MASK_ALL);
	mt_afe_set_reg(AFE_VUL_CUR, backup_reg->reg_AFE_VUL_CUR, MASK_ALL);
	mt_afe_set_reg(AFE_VUL_END, backup_reg->reg_AFE_VUL_END, MASK_ALL);
	mt_afe_set_reg(AFE_VUL_D2_BASE, backup_reg->reg_AFE_VUL_D2_BASE, MASK_ALL);
	mt_afe_set_reg(AFE_VUL_D2_CUR, backup_reg->reg_AFE_VUL_D2_CUR, MASK_ALL);
	mt_afe_set_reg(AFE_VUL_D2_END, backup_reg->reg_AFE_VUL_D2_END, MASK_ALL);
	mt_afe_set_reg(AFE_DAI_BASE, backup_reg->reg_AFE_DAI_BASE, MASK_ALL);
	mt_afe_set_reg(AFE_DAI_CUR, backup_reg->reg_AFE_DAI_CUR, MASK_ALL);
	mt_afe_set_reg(AFE_DAI_END, backup_reg->reg_AFE_DAI_END, MASK_ALL);
	mt_afe_set_reg(AFE_MEMIF_MSB, backup_reg->reg_AFE_MEMIF_MSB, MASK_ALL);

	mt_afe_set_reg(AFE_ADDA_DL_SRC2_CON0, backup_reg->reg_AFE_ADDA_DL_SRC2_CON0, MASK_ALL);
	mt_afe_set_reg(AFE_ADDA_DL_SRC2_CON1, backup_reg->reg_AFE_ADDA_DL_SRC2_CON1, MASK_ALL);
	mt_afe_set_reg(AFE_ADDA_UL_SRC_CON0, backup_reg->reg_AFE_ADDA_UL_SRC_CON0, MASK_ALL);
	mt_afe_set_reg(AFE_ADDA_UL_SRC_CON1, backup_reg->reg_AFE_ADDA_UL_SRC_CON1, MASK_ALL);
	mt_afe_set_reg(AFE_ADDA_TOP_CON0, backup_reg->reg_AFE_ADDA_TOP_CON0, MASK_ALL);
	mt_afe_set_reg(AFE_ADDA_UL_DL_CON0, backup_reg->reg_AFE_ADDA_UL_DL_CON0, MASK_ALL);
	mt_afe_set_reg(AFE_ADDA_NEWIF_CFG0, backup_reg->reg_AFE_ADDA_NEWIF_CFG0, MASK_ALL);
	mt_afe_set_reg(AFE_ADDA_NEWIF_CFG1, backup_reg->reg_AFE_ADDA_NEWIF_CFG1, MASK_ALL);
	mt_afe_set_reg(AFE_ADDA2_TOP_CON0, backup_reg->reg_AFE_ADDA2_TOP_CON0, MASK_ALL);

	mt_afe_set_reg(AFE_SIDETONE_CON0, backup_reg->reg_AFE_SIDETONE_CON0, MASK_ALL);
	mt_afe_set_reg(AFE_SIDETONE_COEFF, backup_reg->reg_AFE_SIDETONE_COEFF, MASK_ALL);
	mt_afe_set_reg(AFE_SIDETONE_CON1, backup_reg->reg_AFE_SIDETONE_CON1, MASK_ALL);
	mt_afe_set_reg(AFE_SIDETONE_GAIN, backup_reg->reg_AFE_SIDETONE_GAIN, MASK_ALL);
	mt_afe_set_reg(AFE_SGEN_CON0, backup_reg->reg_AFE_SGEN_CON0, MASK_ALL);
	mt_afe_set_reg(AFE_SGEN_CON1, backup_reg->reg_AFE_SGEN_CON1, MASK_ALL);
	mt_afe_set_reg(AFE_TOP_CON0, backup_reg->reg_AFE_TOP_CON0, MASK_ALL);
	mt_afe_set_reg(AFE_ADDA_PREDIS_CON0, backup_reg->reg_AFE_ADDA_PREDIS_CON0, MASK_ALL);
	mt_afe_set_reg(AFE_ADDA_PREDIS_CON1, backup_reg->reg_AFE_ADDA_PREDIS_CON1, MASK_ALL);

	mt_afe_set_reg(AFE_MOD_DAI_BASE, backup_reg->reg_AFE_MOD_DAI_BASE, MASK_ALL);
	mt_afe_set_reg(AFE_MOD_DAI_END, backup_reg->reg_AFE_MOD_DAI_END, MASK_ALL);
	mt_afe_set_reg(AFE_MOD_DAI_CUR, backup_reg->reg_AFE_MOD_DAI_CUR, MASK_ALL);
	mt_afe_set_reg(AFE_HDMI_OUT_CON0, backup_reg->reg_AFE_HDMI_OUT_CON0, MASK_ALL);
	mt_afe_set_reg(AFE_HDMI_OUT_BASE, backup_reg->reg_AFE_HDMI_OUT_BASE, MASK_ALL);
	mt_afe_set_reg(AFE_HDMI_OUT_CUR, backup_reg->reg_AFE_HDMI_OUT_CUR, MASK_ALL);
	mt_afe_set_reg(AFE_HDMI_OUT_END, backup_reg->reg_AFE_HDMI_OUT_END, MASK_ALL);
	mt_afe_set_reg(AFE_SPDIF_OUT_CON0, backup_reg->reg_AFE_SPDIF_OUT_CON0, MASK_ALL);
	mt_afe_set_reg(AFE_SPDIF_BASE, backup_reg->reg_AFE_SPDIF_BASE, MASK_ALL);
	mt_afe_set_reg(AFE_SPDIF_CUR, backup_reg->reg_AFE_SPDIF_CUR, MASK_ALL);
	mt_afe_set_reg(AFE_SPDIF_END, backup_reg->reg_AFE_SPDIF_END, MASK_ALL);
	mt_afe_set_reg(AFE_SPDIF2_OUT_CON0, backup_reg->reg_AFE_SPDIF2_OUT_CON0, MASK_ALL);
	mt_afe_set_reg(AFE_SPDIF2_BASE, backup_reg->reg_AFE_SPDIF2_BASE, MASK_ALL);
	mt_afe_set_reg(AFE_SPDIF2_CUR, backup_reg->reg_AFE_SPDIF2_CUR, MASK_ALL);
	mt_afe_set_reg(AFE_SPDIF2_END, backup_reg->reg_AFE_SPDIF2_END, MASK_ALL);
	mt_afe_set_reg(AFE_HDMI_CONN0, backup_reg->reg_AFE_HDMI_CONN0, MASK_ALL);

	mt_afe_set_reg(AFE_IRQ_MCU_CON, backup_reg->reg_AFE_IRQ_MCU_CON, MASK_ALL);
	mt_afe_set_reg(AFE_IRQ_MCU_CNT1, backup_reg->reg_AFE_IRQ_MCU_CNT1, MASK_ALL);
	mt_afe_set_reg(AFE_IRQ_MCU_CNT2, backup_reg->reg_AFE_IRQ_MCU_CNT2, MASK_ALL);
	mt_afe_set_reg(AFE_IRQ_MCU_EN, backup_reg->reg_AFE_IRQ_MCU_EN, MASK_ALL);
	mt_afe_set_reg(AFE_IRQ_MCU_CNT5, backup_reg->reg_AFE_IRQ_MCU_CNT5, MASK_ALL);
	mt_afe_set_reg(AFE_MEMIF_MAXLEN, backup_reg->reg_AFE_MEMIF_MAXLEN, MASK_ALL);
	mt_afe_set_reg(AFE_MEMIF_PBUF_SIZE, backup_reg->reg_AFE_MEMIF_PBUF_SIZE, MASK_ALL);
	mt_afe_set_reg(AFE_MEMIF_PBUF2_SIZE, backup_reg->reg_AFE_MEMIF_PBUF2_SIZE, MASK_ALL);
	mt_afe_set_reg(AFE_APLL1_TUNER_CFG, backup_reg->reg_AFE_APLL1_TUNER_CFG, MASK_ALL);
	mt_afe_set_reg(AFE_APLL2_TUNER_CFG, backup_reg->reg_AFE_APLL2_TUNER_CFG, MASK_ALL);

	mt_afe_set_reg(AFE_GAIN1_CON0, backup_reg->reg_AFE_GAIN1_CON0, MASK_ALL);
	mt_afe_set_reg(AFE_GAIN1_CON1, backup_reg->reg_AFE_GAIN1_CON1, MASK_ALL);
	mt_afe_set_reg(AFE_GAIN1_CON2, backup_reg->reg_AFE_GAIN1_CON2, MASK_ALL);
	mt_afe_set_reg(AFE_GAIN1_CON3, backup_reg->reg_AFE_GAIN1_CON3, MASK_ALL);
	mt_afe_set_reg(AFE_GAIN1_CONN, backup_reg->reg_AFE_GAIN1_CONN, MASK_ALL);
	mt_afe_set_reg(AFE_GAIN1_CUR, backup_reg->reg_AFE_GAIN1_CUR, MASK_ALL);
	mt_afe_set_reg(AFE_GAIN2_CON0, backup_reg->reg_AFE_GAIN2_CON0, MASK_ALL);
	mt_afe_set_reg(AFE_GAIN2_CON1, backup_reg->reg_AFE_GAIN2_CON1, MASK_ALL);
	mt_afe_set_reg(AFE_GAIN2_CON2, backup_reg->reg_AFE_GAIN2_CON2, MASK_ALL);
	mt_afe_set_reg(AFE_GAIN2_CON3, backup_reg->reg_AFE_GAIN2_CON3, MASK_ALL);
	mt_afe_set_reg(AFE_GAIN2_CONN, backup_reg->reg_AFE_GAIN2_CONN, MASK_ALL);
	mt_afe_set_reg(AFE_GAIN2_CUR, backup_reg->reg_AFE_GAIN2_CUR, MASK_ALL);

	mt_afe_set_reg(AFE_IEC_CFG, backup_reg->reg_AFE_IEC_CFG, MASK_ALL);
	mt_afe_set_reg(AFE_IEC_NSNUM, backup_reg->reg_AFE_IEC_NSNUM, MASK_ALL);
	mt_afe_set_reg(AFE_IEC_BURST_INFO, backup_reg->reg_AFE_IEC_BURST_INFO, MASK_ALL);
	mt_afe_set_reg(AFE_IEC_BURST_LEN, backup_reg->reg_AFE_IEC_BURST_LEN, MASK_ALL);
	mt_afe_set_reg(AFE_IEC_NSADR, backup_reg->reg_AFE_IEC_NSADR, MASK_ALL);
	mt_afe_set_reg(AFE_IEC_CHL_STAT0, backup_reg->reg_AFE_IEC_CHL_STAT0, MASK_ALL);
	mt_afe_set_reg(AFE_IEC_CHL_STAT1, backup_reg->reg_AFE_IEC_CHL_STAT1, MASK_ALL);
	mt_afe_set_reg(AFE_IEC_CHR_STAT0, backup_reg->reg_AFE_IEC_CHR_STAT0, MASK_ALL);
	mt_afe_set_reg(AFE_IEC_CHR_STAT1, backup_reg->reg_AFE_IEC_CHR_STAT1, MASK_ALL);
	mt_afe_set_reg(AFE_IEC2_CFG, backup_reg->reg_AFE_IEC2_CFG, MASK_ALL);
	mt_afe_set_reg(AFE_IEC2_NSNUM, backup_reg->reg_AFE_IEC2_NSNUM, MASK_ALL);
	mt_afe_set_reg(AFE_IEC2_BURST_INFO, backup_reg->reg_AFE_IEC2_BURST_INFO, MASK_ALL);
	mt_afe_set_reg(AFE_IEC2_BURST_LEN, backup_reg->reg_AFE_IEC2_BURST_LEN, MASK_ALL);
	mt_afe_set_reg(AFE_IEC2_NSADR, backup_reg->reg_AFE_IEC2_NSADR, MASK_ALL);
	mt_afe_set_reg(AFE_IEC2_CHL_STAT0, backup_reg->reg_AFE_IEC2_CHL_STAT0, MASK_ALL);
	mt_afe_set_reg(AFE_IEC2_CHL_STAT1, backup_reg->reg_AFE_IEC2_CHL_STAT1, MASK_ALL);
	mt_afe_set_reg(AFE_IEC2_CHR_STAT0, backup_reg->reg_AFE_IEC2_CHR_STAT0, MASK_ALL);
	mt_afe_set_reg(AFE_IEC2_CHR_STAT1, backup_reg->reg_AFE_IEC2_CHR_STAT1, MASK_ALL);

	mt_afe_set_reg(AFE_ASRC_CON0, backup_reg->reg_AFE_ASRC_CON0, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON1, backup_reg->reg_AFE_ASRC_CON1, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON2, backup_reg->reg_AFE_ASRC_CON2, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON3, backup_reg->reg_AFE_ASRC_CON3, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON4, backup_reg->reg_AFE_ASRC_CON4, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON5, backup_reg->reg_AFE_ASRC_CON5, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON6, backup_reg->reg_AFE_ASRC_CON6, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON7, backup_reg->reg_AFE_ASRC_CON7, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON8, backup_reg->reg_AFE_ASRC_CON8, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON9, backup_reg->reg_AFE_ASRC_CON9, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON10, backup_reg->reg_AFE_ASRC_CON10, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON13, backup_reg->reg_AFE_ASRC_CON11, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON14, backup_reg->reg_AFE_ASRC_CON13, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON14, backup_reg->reg_AFE_ASRC_CON14, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON15, backup_reg->reg_AFE_ASRC_CON15, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON16, backup_reg->reg_AFE_ASRC_CON16, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON17, backup_reg->reg_AFE_ASRC_CON17, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON18, backup_reg->reg_AFE_ASRC_CON18, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON19, backup_reg->reg_AFE_ASRC_CON19, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON20, backup_reg->reg_AFE_ASRC_CON20, MASK_ALL);
	mt_afe_set_reg(AFE_ASRC_CON21, backup_reg->reg_AFE_ASRC_CON21, MASK_ALL);
	mt_afe_set_reg(PCM_INTF_CON1, backup_reg->reg_PCM_INTF_CON1, MASK_ALL);
	mt_afe_set_reg(PCM_INTF_CON2, backup_reg->reg_PCM_INTF_CON2, MASK_ALL);
	mt_afe_set_reg(PCM2_INTF_CON, backup_reg->reg_PCM2_INTF_CON, MASK_ALL);
	mt_afe_set_reg(AFE_TDM_CON1, backup_reg->reg_AFE_TDM_CON1, MASK_ALL);
	mt_afe_set_reg(AFE_TDM_CON2, backup_reg->reg_AFE_TDM_CON2, MASK_ALL);

	mt_afe_main_clk_off();
	pr_debug("-%s\n", __func__);
}

static void mt_afe_store_reg(struct mt_afe_suspend_reg *backup_reg)
{
	pr_debug("+%s\n", __func__);

	if (!backup_reg) {
		pr_warn("%s backup_reg is null\n", __func__);
		return;
	}

	mt_afe_main_clk_on();

	backup_reg->reg_AUDIO_TOP_CON0 = mt_afe_get_reg(AUDIO_TOP_CON0);
	backup_reg->reg_AUDIO_TOP_CON1 = mt_afe_get_reg(AUDIO_TOP_CON1);
	backup_reg->reg_AUDIO_TOP_CON2 = mt_afe_get_reg(AUDIO_TOP_CON2);
	backup_reg->reg_AUDIO_TOP_CON3 = mt_afe_get_reg(AUDIO_TOP_CON3);
	backup_reg->reg_AFE_DAC_CON0 = mt_afe_get_reg(AFE_DAC_CON0);
	backup_reg->reg_AFE_DAC_CON1 = mt_afe_get_reg(AFE_DAC_CON1);
	backup_reg->reg_AFE_I2S_CON = mt_afe_get_reg(AFE_I2S_CON);
	backup_reg->reg_AFE_DAIBT_CON0 = mt_afe_get_reg(AFE_DAIBT_CON0);

	backup_reg->reg_AFE_CONN0 = mt_afe_get_reg(AFE_CONN0);
	backup_reg->reg_AFE_CONN1 = mt_afe_get_reg(AFE_CONN1);
	backup_reg->reg_AFE_CONN2 = mt_afe_get_reg(AFE_CONN2);
	backup_reg->reg_AFE_CONN3 = mt_afe_get_reg(AFE_CONN3);
	backup_reg->reg_AFE_CONN4 = mt_afe_get_reg(AFE_CONN4);
	backup_reg->reg_AFE_CONN5 = mt_afe_get_reg(AFE_CONN5);
	backup_reg->reg_AFE_CONN6 = mt_afe_get_reg(AFE_CONN6);
	backup_reg->reg_AFE_CONN7 = mt_afe_get_reg(AFE_CONN7);
	backup_reg->reg_AFE_CONN8 = mt_afe_get_reg(AFE_CONN8);
	backup_reg->reg_AFE_CONN9 = mt_afe_get_reg(AFE_CONN9);
	backup_reg->reg_AFE_CONN_24BIT = mt_afe_get_reg(AFE_CONN_24BIT);
	backup_reg->reg_AFE_I2S_CON1 = mt_afe_get_reg(AFE_I2S_CON1);
	backup_reg->reg_AFE_I2S_CON2 = mt_afe_get_reg(AFE_I2S_CON2);
	backup_reg->reg_AFE_I2S_CON3 = mt_afe_get_reg(AFE_I2S_CON3);
	backup_reg->reg_AFE_MRGIF_CON = mt_afe_get_reg(AFE_MRGIF_CON);

	backup_reg->reg_AFE_DL1_BASE = mt_afe_get_reg(AFE_DL1_BASE);
	backup_reg->reg_AFE_DL1_CUR = mt_afe_get_reg(AFE_DL1_CUR);
	backup_reg->reg_AFE_DL1_END = mt_afe_get_reg(AFE_DL1_END);
	backup_reg->reg_AFE_DL2_BASE = mt_afe_get_reg(AFE_DL2_BASE);
	backup_reg->reg_AFE_DL2_CUR = mt_afe_get_reg(AFE_DL2_CUR);
	backup_reg->reg_AFE_DL2_END = mt_afe_get_reg(AFE_DL2_END);
	backup_reg->reg_AFE_AWB_BASE = mt_afe_get_reg(AFE_AWB_BASE);
	backup_reg->reg_AFE_AWB_CUR = mt_afe_get_reg(AFE_AWB_CUR);
	backup_reg->reg_AFE_AWB_END = mt_afe_get_reg(AFE_AWB_END);
	backup_reg->reg_AFE_VUL_BASE = mt_afe_get_reg(AFE_VUL_BASE);
	backup_reg->reg_AFE_VUL_CUR = mt_afe_get_reg(AFE_VUL_CUR);
	backup_reg->reg_AFE_VUL_END = mt_afe_get_reg(AFE_VUL_END);
	backup_reg->reg_AFE_VUL_D2_BASE = mt_afe_get_reg(AFE_VUL_D2_BASE);
	backup_reg->reg_AFE_VUL_D2_CUR = mt_afe_get_reg(AFE_VUL_D2_CUR);
	backup_reg->reg_AFE_VUL_D2_END = mt_afe_get_reg(AFE_VUL_D2_END);
	backup_reg->reg_AFE_DAI_BASE = mt_afe_get_reg(AFE_DAI_BASE);
	backup_reg->reg_AFE_DAI_CUR = mt_afe_get_reg(AFE_DAI_CUR);
	backup_reg->reg_AFE_DAI_END = mt_afe_get_reg(AFE_DAI_END);
	backup_reg->reg_AFE_MEMIF_MSB = mt_afe_get_reg(AFE_MEMIF_MSB);

	backup_reg->reg_AFE_ADDA_DL_SRC2_CON0 = mt_afe_get_reg(AFE_ADDA_DL_SRC2_CON0);
	backup_reg->reg_AFE_ADDA_DL_SRC2_CON1 = mt_afe_get_reg(AFE_ADDA_DL_SRC2_CON1);
	backup_reg->reg_AFE_ADDA_UL_SRC_CON0 = mt_afe_get_reg(AFE_ADDA_UL_SRC_CON0);
	backup_reg->reg_AFE_ADDA_UL_SRC_CON1 = mt_afe_get_reg(AFE_ADDA_UL_SRC_CON1);
	backup_reg->reg_AFE_ADDA_TOP_CON0 = mt_afe_get_reg(AFE_ADDA_TOP_CON0);
	backup_reg->reg_AFE_ADDA_UL_DL_CON0 = mt_afe_get_reg(AFE_ADDA_UL_DL_CON0);
	backup_reg->reg_AFE_ADDA_NEWIF_CFG0 = mt_afe_get_reg(AFE_ADDA_NEWIF_CFG0);
	backup_reg->reg_AFE_ADDA_NEWIF_CFG1 = mt_afe_get_reg(AFE_ADDA_NEWIF_CFG1);
	backup_reg->reg_AFE_ADDA2_TOP_CON0 = mt_afe_get_reg(AFE_ADDA2_TOP_CON0);

	backup_reg->reg_AFE_SIDETONE_CON0 = mt_afe_get_reg(AFE_SIDETONE_CON0);
	backup_reg->reg_AFE_SIDETONE_COEFF = mt_afe_get_reg(AFE_SIDETONE_COEFF);
	backup_reg->reg_AFE_SIDETONE_CON1 = mt_afe_get_reg(AFE_SIDETONE_CON1);
	backup_reg->reg_AFE_SIDETONE_GAIN = mt_afe_get_reg(AFE_SIDETONE_GAIN);
	backup_reg->reg_AFE_SGEN_CON0 = mt_afe_get_reg(AFE_SGEN_CON0);
	backup_reg->reg_AFE_SGEN_CON1 = mt_afe_get_reg(AFE_SGEN_CON1);
	backup_reg->reg_AFE_TOP_CON0 = mt_afe_get_reg(AFE_TOP_CON0);
	backup_reg->reg_AFE_ADDA_PREDIS_CON0 = mt_afe_get_reg(AFE_ADDA_PREDIS_CON0);
	backup_reg->reg_AFE_ADDA_PREDIS_CON1 = mt_afe_get_reg(AFE_ADDA_PREDIS_CON1);

	backup_reg->reg_AFE_MOD_DAI_BASE = mt_afe_get_reg(AFE_MOD_DAI_BASE);
	backup_reg->reg_AFE_MOD_DAI_END = mt_afe_get_reg(AFE_MOD_DAI_END);
	backup_reg->reg_AFE_MOD_DAI_CUR = mt_afe_get_reg(AFE_MOD_DAI_CUR);
	backup_reg->reg_AFE_HDMI_OUT_CON0 = mt_afe_get_reg(AFE_HDMI_OUT_CON0);
	backup_reg->reg_AFE_HDMI_OUT_BASE = mt_afe_get_reg(AFE_HDMI_OUT_BASE);
	backup_reg->reg_AFE_HDMI_OUT_CUR = mt_afe_get_reg(AFE_HDMI_OUT_CUR);
	backup_reg->reg_AFE_HDMI_OUT_END = mt_afe_get_reg(AFE_HDMI_OUT_END);
	backup_reg->reg_AFE_SPDIF_OUT_CON0 = mt_afe_get_reg(AFE_SPDIF_OUT_CON0);
	backup_reg->reg_AFE_SPDIF_BASE = mt_afe_get_reg(AFE_SPDIF_BASE);
	backup_reg->reg_AFE_SPDIF_CUR = mt_afe_get_reg(AFE_SPDIF_CUR);
	backup_reg->reg_AFE_SPDIF_END = mt_afe_get_reg(AFE_SPDIF_END);
	backup_reg->reg_AFE_SPDIF2_OUT_CON0 = mt_afe_get_reg(AFE_SPDIF2_OUT_CON0);
	backup_reg->reg_AFE_SPDIF2_BASE = mt_afe_get_reg(AFE_SPDIF2_BASE);
	backup_reg->reg_AFE_SPDIF2_CUR = mt_afe_get_reg(AFE_SPDIF2_CUR);
	backup_reg->reg_AFE_SPDIF2_END = mt_afe_get_reg(AFE_SPDIF2_END);
	backup_reg->reg_AFE_HDMI_CONN0 = mt_afe_get_reg(AFE_HDMI_CONN0);

	backup_reg->reg_AFE_IRQ_MCU_CON = mt_afe_get_reg(AFE_IRQ_MCU_CON);
	backup_reg->reg_AFE_IRQ_MCU_CNT1 = mt_afe_get_reg(AFE_IRQ_MCU_CNT1);
	backup_reg->reg_AFE_IRQ_MCU_CNT2 = mt_afe_get_reg(AFE_IRQ_MCU_CNT2);
	backup_reg->reg_AFE_IRQ_MCU_EN = mt_afe_get_reg(AFE_IRQ_MCU_EN);
	backup_reg->reg_AFE_IRQ_MCU_CNT5 = mt_afe_get_reg(AFE_IRQ_MCU_CNT5);
	backup_reg->reg_AFE_MEMIF_MAXLEN = mt_afe_get_reg(AFE_MEMIF_MAXLEN);
	backup_reg->reg_AFE_MEMIF_PBUF_SIZE = mt_afe_get_reg(AFE_MEMIF_PBUF_SIZE);
	backup_reg->reg_AFE_MEMIF_PBUF2_SIZE = mt_afe_get_reg(AFE_MEMIF_PBUF2_SIZE);
	backup_reg->reg_AFE_APLL1_TUNER_CFG = mt_afe_get_reg(AFE_APLL1_TUNER_CFG);
	backup_reg->reg_AFE_APLL2_TUNER_CFG = mt_afe_get_reg(AFE_APLL2_TUNER_CFG);

	backup_reg->reg_AFE_GAIN1_CON0 = mt_afe_get_reg(AFE_GAIN1_CON0);
	backup_reg->reg_AFE_GAIN1_CON1 = mt_afe_get_reg(AFE_GAIN1_CON1);
	backup_reg->reg_AFE_GAIN1_CON2 = mt_afe_get_reg(AFE_GAIN1_CON2);
	backup_reg->reg_AFE_GAIN1_CON3 = mt_afe_get_reg(AFE_GAIN1_CON3);
	backup_reg->reg_AFE_GAIN1_CONN = mt_afe_get_reg(AFE_GAIN1_CONN);
	backup_reg->reg_AFE_GAIN1_CUR = mt_afe_get_reg(AFE_GAIN1_CUR);
	backup_reg->reg_AFE_GAIN2_CON0 = mt_afe_get_reg(AFE_GAIN2_CON0);
	backup_reg->reg_AFE_GAIN2_CON1 = mt_afe_get_reg(AFE_GAIN2_CON1);
	backup_reg->reg_AFE_GAIN2_CON2 = mt_afe_get_reg(AFE_GAIN2_CON2);
	backup_reg->reg_AFE_GAIN2_CON3 = mt_afe_get_reg(AFE_GAIN2_CON3);
	backup_reg->reg_AFE_GAIN2_CONN = mt_afe_get_reg(AFE_GAIN2_CONN);
	backup_reg->reg_AFE_GAIN2_CUR = mt_afe_get_reg(AFE_GAIN2_CUR);

	backup_reg->reg_AFE_IEC_CFG = mt_afe_get_reg(AFE_IEC_CFG);
	backup_reg->reg_AFE_IEC_NSNUM = mt_afe_get_reg(AFE_IEC_NSNUM);
	backup_reg->reg_AFE_IEC_BURST_INFO = mt_afe_get_reg(AFE_IEC_BURST_INFO);
	backup_reg->reg_AFE_IEC_BURST_LEN = mt_afe_get_reg(AFE_IEC_BURST_LEN);
	backup_reg->reg_AFE_IEC_NSADR = mt_afe_get_reg(AFE_IEC_NSADR);
	backup_reg->reg_AFE_IEC_CHL_STAT0 = mt_afe_get_reg(AFE_IEC_CHL_STAT0);
	backup_reg->reg_AFE_IEC_CHL_STAT1 = mt_afe_get_reg(AFE_IEC_CHL_STAT1);
	backup_reg->reg_AFE_IEC_CHR_STAT0 = mt_afe_get_reg(AFE_IEC_CHR_STAT0);
	backup_reg->reg_AFE_IEC_CHR_STAT1 = mt_afe_get_reg(AFE_IEC_CHR_STAT1);
	backup_reg->reg_AFE_IEC2_CFG = mt_afe_get_reg(AFE_IEC2_CFG);
	backup_reg->reg_AFE_IEC2_NSNUM = mt_afe_get_reg(AFE_IEC2_NSNUM);
	backup_reg->reg_AFE_IEC2_BURST_INFO = mt_afe_get_reg(AFE_IEC2_BURST_INFO);
	backup_reg->reg_AFE_IEC2_BURST_LEN = mt_afe_get_reg(AFE_IEC2_BURST_LEN);
	backup_reg->reg_AFE_IEC2_NSADR = mt_afe_get_reg(AFE_IEC2_NSADR);
	backup_reg->reg_AFE_IEC2_CHL_STAT0 = mt_afe_get_reg(AFE_IEC2_CHL_STAT0);
	backup_reg->reg_AFE_IEC2_CHL_STAT1 = mt_afe_get_reg(AFE_IEC2_CHL_STAT1);
	backup_reg->reg_AFE_IEC2_CHR_STAT0 = mt_afe_get_reg(AFE_IEC2_CHR_STAT0);
	backup_reg->reg_AFE_IEC2_CHR_STAT1 = mt_afe_get_reg(AFE_IEC2_CHR_STAT1);

	backup_reg->reg_AFE_ASRC_CON0 = mt_afe_get_reg(AFE_ASRC_CON0);
	backup_reg->reg_AFE_ASRC_CON1 = mt_afe_get_reg(AFE_ASRC_CON1);
	backup_reg->reg_AFE_ASRC_CON2 = mt_afe_get_reg(AFE_ASRC_CON2);
	backup_reg->reg_AFE_ASRC_CON3 = mt_afe_get_reg(AFE_ASRC_CON3);
	backup_reg->reg_AFE_ASRC_CON4 = mt_afe_get_reg(AFE_ASRC_CON4);
	backup_reg->reg_AFE_ASRC_CON5 = mt_afe_get_reg(AFE_ASRC_CON5);
	backup_reg->reg_AFE_ASRC_CON6 = mt_afe_get_reg(AFE_ASRC_CON6);
	backup_reg->reg_AFE_ASRC_CON7 = mt_afe_get_reg(AFE_ASRC_CON7);
	backup_reg->reg_AFE_ASRC_CON8 = mt_afe_get_reg(AFE_ASRC_CON8);
	backup_reg->reg_AFE_ASRC_CON9 = mt_afe_get_reg(AFE_ASRC_CON9);
	backup_reg->reg_AFE_ASRC_CON10 = mt_afe_get_reg(AFE_ASRC_CON10);
	backup_reg->reg_AFE_ASRC_CON11 = mt_afe_get_reg(AFE_ASRC_CON11);
	backup_reg->reg_AFE_ASRC_CON11 = mt_afe_get_reg(AFE_ASRC_CON13);
	backup_reg->reg_AFE_ASRC_CON11 = mt_afe_get_reg(AFE_ASRC_CON14);
	backup_reg->reg_AFE_ASRC_CON11 = mt_afe_get_reg(AFE_ASRC_CON15);
	backup_reg->reg_AFE_ASRC_CON11 = mt_afe_get_reg(AFE_ASRC_CON16);
	backup_reg->reg_AFE_ASRC_CON11 = mt_afe_get_reg(AFE_ASRC_CON17);
	backup_reg->reg_AFE_ASRC_CON11 = mt_afe_get_reg(AFE_ASRC_CON18);
	backup_reg->reg_AFE_ASRC_CON11 = mt_afe_get_reg(AFE_ASRC_CON19);
	backup_reg->reg_AFE_ASRC_CON11 = mt_afe_get_reg(AFE_ASRC_CON20);
	backup_reg->reg_AFE_ASRC_CON11 = mt_afe_get_reg(AFE_ASRC_CON21);
	backup_reg->reg_PCM_INTF_CON1 = mt_afe_get_reg(PCM_INTF_CON1);
	backup_reg->reg_PCM_INTF_CON2 = mt_afe_get_reg(PCM_INTF_CON2);
	backup_reg->reg_PCM2_INTF_CON = mt_afe_get_reg(PCM2_INTF_CON);
	backup_reg->reg_AFE_TDM_CON1 = mt_afe_get_reg(AFE_TDM_CON1);
	backup_reg->reg_AFE_TDM_CON2 = mt_afe_get_reg(AFE_TDM_CON2);

	mt_afe_main_clk_off();
	pr_debug("-%s\n", __func__);
}

static void mt_afe_enable_i2s_div_power(uint32_t divider)
{
	mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_3, 0 << divider, 1 << divider);
}

static void mt_afe_disable_i2s_div_power(uint32_t divider)
{
	mt_afe_topck_set_reg(AUDIO_CLK_AUDDIV_3, 1 << divider, 1 << divider);
}
