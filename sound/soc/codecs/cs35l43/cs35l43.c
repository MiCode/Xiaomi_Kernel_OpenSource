// SPDX-License-Identifier: GPL-2.0

/*
 * cs35l43.c -- CS35l43 ALSA SoC audio driver
 *
 * Copyright 2021 Cirrus Logic, Inc.
 *
 * Author:	David Rhodes	<david.rhodes@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/gpio.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/of_irq.h>
#include <linux/completion.h>
#include <linux/spi/spi.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/timekeeping.h>

#include "wm_adsp.h"
#include "cs35l43.h"
#include <sound/cs35l43.h>
#define CS35L43_SP_WL_MAX 24
static const char * const cs35l43_supplies[] = {
	"VA",
	"VP",
};

static int cs35l43_exit_hibernate(struct cs35l43_private *cs35l43);

static const DECLARE_TLV_DB_RANGE(dig_vol_tlv,
		0, 0, TLV_DB_SCALE_ITEM(TLV_DB_GAIN_MUTE, 0, 1),
		1, 913, TLV_DB_SCALE_ITEM(-10200, 25, 0));
static DECLARE_TLV_DB_SCALE(amp_gain_tlv, 0, 1, 1);

static const struct snd_kcontrol_new amp_enable_ctrl =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const char * const cs35l43_tx_input_texts[] = {
	"Zero", "ASPRX1", "ASPRX2", "VMON", "IMON", "VMON FS2", "IMON FS2",
	"VPMON", "VBSTMON", "DSP", "DSP FS2"};

static const unsigned int cs35l43_tx_input_values[] = {0x00,
						CS35L43_INPUT_SRC_ASPRX1,
						CS35L43_INPUT_SRC_ASPRX2,
						CS35L43_INPUT_SRC_VMON,
						CS35L43_INPUT_SRC_IMON,
						CS35L43_INPUT_SRC_VMON_FS2,
						CS35L43_INPUT_SRC_IMON_FS2,
						CS35L43_INPUT_SRC_VPMON,
						CS35L43_INPUT_SRC_VBSTMON,
						CS35L43_INPUT_DSP_TX5,
						CS35L43_INPUT_DSP_TX6};

static SOC_VALUE_ENUM_SINGLE_DECL(cs35l43_asptx1_enum,
				CS35L43_ASPTX1_INPUT,
				0, CS35L43_INPUT_MASK,
				cs35l43_tx_input_texts,
				cs35l43_tx_input_values);

static const struct snd_kcontrol_new asp_tx1_mux =
	SOC_DAPM_ENUM("ASPTX1 SRC", cs35l43_asptx1_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(cs35l43_asptx2_enum,
				CS35L43_ASPTX2_INPUT,
				0, CS35L43_INPUT_MASK,
				cs35l43_tx_input_texts,
				cs35l43_tx_input_values);

static const struct snd_kcontrol_new asp_tx2_mux =
	SOC_DAPM_ENUM("ASPTX2 SRC", cs35l43_asptx2_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(cs35l43_asptx3_enum,
				CS35L43_ASPTX3_INPUT,
				0, CS35L43_INPUT_MASK,
				cs35l43_tx_input_texts,
				cs35l43_tx_input_values);

static const struct snd_kcontrol_new asp_tx3_mux =
	SOC_DAPM_ENUM("ASPTX3 SRC", cs35l43_asptx3_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(cs35l43_asptx4_enum,
				CS35L43_ASPTX4_INPUT,
				0, CS35L43_INPUT_MASK,
				cs35l43_tx_input_texts,
				cs35l43_tx_input_values);

static const struct snd_kcontrol_new asp_tx4_mux =
	SOC_DAPM_ENUM("ASPTX4 SRC", cs35l43_asptx4_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(cs35l43_dsprx1_enum,
				CS35L43_DSP1RX1_INPUT,
				0, CS35L43_INPUT_MASK,
				cs35l43_tx_input_texts,
				cs35l43_tx_input_values);

static const struct snd_kcontrol_new dsp_rx1_mux =
	SOC_DAPM_ENUM("DSPRX1 SRC", cs35l43_dsprx1_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(cs35l43_dsprx2_enum,
				CS35L43_DSP1RX2_INPUT,
				0, CS35L43_INPUT_MASK,
				cs35l43_tx_input_texts,
				cs35l43_tx_input_values);

static const struct snd_kcontrol_new dsp_rx2_mux =
	SOC_DAPM_ENUM("DSPRX2 SRC", cs35l43_dsprx2_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(cs35l43_dacpcm_enum,
				CS35L43_DACPCM1_INPUT,
				0, CS35L43_INPUT_MASK,
				cs35l43_tx_input_texts,
				cs35l43_tx_input_values);

static const struct snd_kcontrol_new dacpcm_mux =
	SOC_DAPM_ENUM("PCM Source", cs35l43_dacpcm_enum);

static SOC_VALUE_ENUM_SINGLE_DECL(cs35l43_dacpcm2_enum,
				CS35L43_DACPCM2_INPUT,
				0, CS35L43_INPUT_MASK,
				cs35l43_tx_input_texts,
				cs35l43_tx_input_values);

static const struct snd_kcontrol_new dacpcm2_mux =
	SOC_DAPM_ENUM("High Rate PCM Source", cs35l43_dacpcm2_enum);

static const char * const cs35l43_ultrasonic_mode_texts[] = {
	"Disabled", "In Band", "Out of Band"
};
static SOC_ENUM_SINGLE_DECL(cs35l43_ultrasonic_mode_enum, SND_SOC_NOPM, 0,
				cs35l43_ultrasonic_mode_texts);

static const struct snd_kcontrol_new ultra_mux =
	SOC_DAPM_ENUM("Ultrasonic Mode", cs35l43_ultrasonic_mode_enum);

static const char * const cs35l43_wd_mode_texts[] = {"Normal", "Mute"};
static const unsigned int cs35l43_wd_mode_values[] = {0x0, 0x3};
static SOC_VALUE_ENUM_SINGLE_DECL(cs35l43_dc_wd_mode_enum, CS35L43_ALIVE_DCIN_WD,
			CS35L43_WD_MODE_SHIFT, 0x3,
			cs35l43_wd_mode_texts, cs35l43_wd_mode_values);

static int cs35l43_ultrasonic_mode_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l43_private *cs35l43;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l43 = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = cs35l43->ultrasonic_mode;

	return 0;
}

static int cs35l43_ultrasonic_mode_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component;
	struct cs35l43_private *cs35l43;
	unsigned int mon_rates, rx_rates, tx_rates, high_rate_enable;

	component = snd_soc_kcontrol_component(kcontrol);
	cs35l43 = snd_soc_component_get_drvdata(component);

	cs35l43->ultrasonic_mode = ucontrol->value.integer.value[0];

	switch (cs35l43->ultrasonic_mode) {
	case CS35L43_ULTRASONIC_MODE_INBAND:
		mon_rates = CS35L43_BASE_RATE;
		rx_rates = CS35L43_HIGH_RATE;
		tx_rates = CS35L43_HIGH_RATE;
		high_rate_enable = 1;

		break;
	case CS35L43_ULTRASONIC_MODE_OUT_OF_BAND:
		mon_rates = CS35L43_BASE_RATE;
		rx_rates = CS35L43_HIGH_RATE;
		tx_rates = CS35L43_HIGH_RATE;
		high_rate_enable = 1;
		break;
	case CS35L43_ULTRASONIC_MODE_DISABLED:
	default:
		mon_rates = CS35L43_BASE_RATE;
		rx_rates = CS35L43_BASE_RATE;
		tx_rates = CS35L43_BASE_RATE;
		high_rate_enable = 0;
		break;
	}

	regmap_update_bits(cs35l43->regmap,
			CS35L43_DSP1_SAMPLE_RATE_RX1,
			CS35L43_DSP_RX1_RATE_MASK,
			rx_rates << CS35L43_DSP_RX1_RATE_SHIFT);
	regmap_update_bits(cs35l43->regmap,
			CS35L43_DSP1_SAMPLE_RATE_RX1,
			CS35L43_DSP_RX2_RATE_MASK,
			rx_rates << CS35L43_DSP_RX2_RATE_SHIFT);
	regmap_update_bits(cs35l43->regmap,
			CS35L43_DSP1_SAMPLE_RATE_RX1,
			CS35L43_DSP_RX3_RATE_MASK,
			rx_rates << CS35L43_DSP_RX3_RATE_SHIFT);
	regmap_update_bits(cs35l43->regmap,
			CS35L43_DSP1_SAMPLE_RATE_RX1,
			CS35L43_DSP_RX4_RATE_MASK,
			mon_rates << CS35L43_DSP_RX4_RATE_SHIFT);
	regmap_update_bits(cs35l43->regmap,
			CS35L43_DSP1_SAMPLE_RATE_RX2,
			CS35L43_DSP_RX5_RATE_MASK,
			mon_rates << CS35L43_DSP_RX5_RATE_SHIFT);
	regmap_update_bits(cs35l43->regmap,
			CS35L43_DSP1_SAMPLE_RATE_RX2,
			CS35L43_DSP_RX6_RATE_MASK,
			mon_rates << CS35L43_DSP_RX6_RATE_SHIFT);

	regmap_update_bits(cs35l43->regmap,
			CS35L43_DSP1_SAMPLE_RATE_TX1,
			CS35L43_DSP_TX1_RATE_MASK,
			tx_rates << CS35L43_DSP_TX1_RATE_SHIFT);
	regmap_update_bits(cs35l43->regmap,
			CS35L43_DSP1_SAMPLE_RATE_TX1,
			CS35L43_DSP_TX2_RATE_MASK,
			tx_rates << CS35L43_DSP_TX2_RATE_SHIFT);
	regmap_update_bits(cs35l43->regmap,
			CS35L43_DSP1_SAMPLE_RATE_TX1,
			CS35L43_DSP_TX3_RATE_MASK,
			tx_rates << CS35L43_DSP_TX3_RATE_SHIFT);
	regmap_update_bits(cs35l43->regmap,
			CS35L43_DSP1_SAMPLE_RATE_TX1,
			CS35L43_DSP_TX4_RATE_MASK,
			tx_rates << CS35L43_DSP_TX4_RATE_SHIFT);
	regmap_update_bits(cs35l43->regmap,
			CS35L43_DSP1_SAMPLE_RATE_TX2,
			CS35L43_DSP_TX5_RATE_MASK,
			CS35L43_BASE_RATE << CS35L43_DSP_TX5_RATE_SHIFT);
	regmap_update_bits(cs35l43->regmap,
			CS35L43_DSP1_SAMPLE_RATE_TX2,
			CS35L43_DSP_TX6_RATE_MASK,
			tx_rates << CS35L43_DSP_TX6_RATE_SHIFT);

	if (high_rate_enable) {
		regmap_update_bits(cs35l43->regmap, CS35L43_DAC_MSM_CONFIG,
				CS35L43_AMP_PCM_FSX2_EN_MASK,
				CS35L43_AMP_PCM_FSX2_EN_MASK);
		regmap_update_bits(cs35l43->regmap, CS35L43_MONITOR_FILT,
				CS35L43_VIMON_DUAL_RATE_MASK,
				CS35L43_VIMON_DUAL_RATE_MASK);
	} else {
		regmap_update_bits(cs35l43->regmap, CS35L43_DAC_MSM_CONFIG,
				CS35L43_AMP_PCM_FSX2_EN_MASK, 0);
		regmap_update_bits(cs35l43->regmap, CS35L43_MONITOR_FILT,
				CS35L43_VIMON_DUAL_RATE_MASK, 0);
	}

	regmap_write(cs35l43->regmap, CS35L43_DSP_VIRTUAL1_MBOX_1,
			CS35L43_MBOX_CMD_AUDIO_REINIT);

	return 0;
}

static const struct snd_kcontrol_new cs35l43_aud_controls[] = {
	SOC_SINGLE("DC Watchdog Enable", CS35L43_ALIVE_DCIN_WD,
			CS35L43_DCIN_WD_EN_SHIFT, 1, 0),
	SOC_SINGLE("DC Watchdog Threshold", CS35L43_ALIVE_DCIN_WD,
			CS35L43_DCIN_WD_THLD_SHIFT, 0x28, 0),
	SOC_SINGLE("DC Watchdog Duration", CS35L43_ALIVE_DCIN_WD,
			CS35L43_DCIN_WD_DUR_SHIFT, 0x7, 0),
	SOC_ENUM("DC Watchdog Mode", cs35l43_dc_wd_mode_enum),
	SOC_SINGLE_SX_TLV("Digital PCM Volume", CS35L43_AMP_CTRL,
				CS35L43_AMP_VOL_PCM_SHIFT,
				0x4CF, 0x391, dig_vol_tlv),
	SOC_SINGLE_TLV("Amp Gain", CS35L43_AMP_GAIN,
			CS35L43_AMP_GAIN_PCM_SHIFT, 20, 0,
			amp_gain_tlv),
	SOC_ENUM_EXT("Ultrasonic Mode", cs35l43_ultrasonic_mode_enum,
			cs35l43_ultrasonic_mode_get, cs35l43_ultrasonic_mode_put),
	WM_ADSP2_PRELOAD_SWITCH("DSP1", 1),
	WM_ADSP_FW_CONTROL("DSP1", 0),
};

static int cs35l43_write_seq_elem_update(struct cs35l43_write_seq_elem *write_seq_elem,
						unsigned int addr, unsigned int value)
{
	switch (write_seq_elem->operation) {
	case CS35L43_POWER_SEQ_OP_WRITE_REG_FULL:
		write_seq_elem->words[0] = (addr & 0xFFFF0000) >> 16;
		write_seq_elem->words[1] = ((addr & 0xFFFF) << 8) |
						((value & 0xFF000000) >> 24);
		write_seq_elem->words[2] = (value & 0xFFFFFF);

		break;
	case CS35L43_POWER_SEQ_OP_WRITE_REG_ADDR8:
		write_seq_elem->words[0] = (CS35L43_POWER_SEQ_OP_WRITE_REG_ADDR8 << 16) |
						((addr & 0xFF) << 8) |
						((value & 0xFF000000) >> 24);
		write_seq_elem->words[1] = (value & 0xFFFFFF);
		break;
	case CS35L43_POWER_SEQ_OP_WRITE_REG_L16:
		write_seq_elem->words[0] = (CS35L43_POWER_SEQ_OP_WRITE_REG_L16 << 16) |
						((addr & 0xFFFF00) >> 8);
		write_seq_elem->words[1] = ((addr & 0xFF) << 16) | (value & 0xFFFF);
		break;
	default:
		break;
	}

	return 0;
}

static int cs35l43_write_seq_add(struct cs35l43_private *cs35l43,
				struct cs35l43_write_seq *sequence,
				unsigned int update_reg, unsigned int update_value,
				bool read)
{
	struct device *dev = cs35l43->dev;
	u32 *buf, *op_words, addr, prev_addr, value;
	u8 operation;
	unsigned int i, j, num_words, ret = 0;
	struct cs35l43_write_seq_elem *write_seq_elem;

	buf = kzalloc(sizeof(u32) * sequence->length, GFP_KERNEL);
	if (!buf) {
		dev_info(cs35l43->dev, "%s: failed to alloc write seq\n",
			__func__);
		return -ENOMEM;
	}

	ret = wm_adsp_read_ctl(&cs35l43->dsp, sequence->name,
			WMFW_ADSP2_XM, CS35L43_ALG_ID_PM, (void *)buf,
			sequence->length * sizeof(u32));
	if (ret != 0) {
		dev_info(dev, "%s: Failed to read control\n", __func__);
		goto exit;
	}

	for (i = 0; i < sequence->length; i++) {
		buf[i] = be32_to_cpu(buf[i]);
		dev_dbg(dev, "%s[%d] = 0x%x\n", sequence->name, i, buf[i]);
	}

	list_for_each_entry(write_seq_elem, &sequence->list_head, list) {
		switch (write_seq_elem->operation) {
		case CS35L43_POWER_SEQ_OP_WRITE_REG_FULL:
			addr = ((write_seq_elem->words[0] & 0xFFFF) << 16) |
				((write_seq_elem->words[1] & 0xFFFF00) >> 8);
			value = ((write_seq_elem->words[1] & 0xFF) << 24) |
				(write_seq_elem->words[2] & 0xFFFFFF);
			break;
		case CS35L43_POWER_SEQ_OP_WRITE_REG_ADDR8:
			addr = (prev_addr & 0xFFFFFF00) |
				((write_seq_elem->words[0] & 0xFF00) >> 8);
			value = ((write_seq_elem->words[0] & 0xFF) << 24) |
				(write_seq_elem->words[1] & 0xFFFFFF);
			break;
		case CS35L43_POWER_SEQ_OP_WRITE_REG_L16:
			addr = ((write_seq_elem->words[0] & 0xFFFF) << 8) |
				((write_seq_elem->words[1] & 0xFF0000) >> 16);
			value = (write_seq_elem->words[1] & 0xFFFF);
			break;
		default:
			break;
		}
		dev_dbg(dev, "write seq elem: addr=0x%x, prev_addr=0x%x, val=0x%x\n",
								addr, prev_addr, value);
		prev_addr = addr;

		if (addr == update_reg) {
			if (read)
				regmap_read(cs35l43->regmap, addr, &update_value);

			dev_info(dev, "%s: Updating register 0x%x with value 0x%x\n",
					__func__, addr, update_value);
			cs35l43_write_seq_elem_update(write_seq_elem, update_reg, update_value);
			memcpy(buf + write_seq_elem->offset, write_seq_elem->words,
					write_seq_elem->size * sizeof(u32));
			goto write_exit;
		}
	}

	i = 0;
	while (i < sequence->length) {
		operation = (buf[i] & CS35L43_POWER_SEQ_OP_MASK) >>
			CS35L43_POWER_SEQ_OP_SHIFT;

		if (operation == CS35L43_POWER_SEQ_OP_END)
			break;

		/* get num words for given operation */
		for (j = 0; j < CS35L43_POWER_SEQ_NUM_OPS; j++) {
			if (cs35l43_write_seq_op_sizes[j][0] == operation) {
				num_words = cs35l43_write_seq_op_sizes[j][1];
				break;
			}
		}

		i += num_words;
	}

	if (operation != CS35L43_POWER_SEQ_OP_END ||
		i + CS35L43_POWER_SEQ_OP_WRITE_REG_FULL_WORDS +
		CS35L43_POWER_SEQ_OP_END_WORDS > sequence->length) {
		dev_info(dev, "WRITE SEQ END_OF_SCRIPT not found or sequence full\n");
		ret = -E2BIG;
		goto exit;
	}

	write_seq_elem = devm_kzalloc(dev, sizeof(*write_seq_elem), GFP_KERNEL);
	if (!write_seq_elem) {
		ret = -ENOMEM;
		goto exit;
	}

	write_seq_elem->size = CS35L43_POWER_SEQ_OP_WRITE_REG_FULL_WORDS;
	write_seq_elem->offset = i;
	write_seq_elem->operation = CS35L43_POWER_SEQ_OP_WRITE_REG_FULL;

	op_words = kzalloc(write_seq_elem->size * sizeof(u32), GFP_KERNEL);
	if (!op_words) {
		ret =  -ENOMEM;
		goto err_elem;
	}

	write_seq_elem->words = op_words;

	if (read)
		regmap_read(cs35l43->regmap, update_reg, &update_value);

	cs35l43_write_seq_elem_update(write_seq_elem, update_reg, update_value);
	list_add_tail(&write_seq_elem->list, &sequence->list_head);

	sequence->num_ops++;

	memcpy(&buf[i], op_words, write_seq_elem->size * sizeof(u32));

	dev_info(dev, "%s: Added register 0x%x with value 0x%x\n",
			__func__, update_reg, update_value);
	for (i = 0; i < write_seq_elem->size; i++)
		dev_dbg(dev, "elem[%d]: 0x%x\n", i, write_seq_elem->words[i]);

	buf[write_seq_elem->offset + write_seq_elem->size] = 0xFFFFFFFF;

write_exit:
	for (i = 0; i < sequence->length; i++) {
		dev_dbg(dev, "%s[%d] = 0x%x\n", sequence->name, i, buf[i]);
		buf[i] = cpu_to_be32(buf[i]);
	}

	ret = wm_adsp_write_ctl(&cs35l43->dsp, sequence->name,
		WMFW_ADSP2_XM, CS35L43_ALG_ID_PM, (void *)buf,
		sequence->length * sizeof(u32));
	goto exit;

err_elem:
	kfree(write_seq_elem);
exit:
	kfree(buf);
	return ret;
}

static int cs35l43_write_seq_update(struct cs35l43_private *cs35l43,
					struct cs35l43_write_seq *sequence)
{
	struct device *dev = cs35l43->dev;
	u32 *buf;
	u32 addr, prev_addr, value, reg_value;
	unsigned int ret = 0, i;
	struct cs35l43_write_seq_elem *write_seq_elem;

	buf = kzalloc(sizeof(u32) * sequence->length, GFP_KERNEL);
	if (!buf) {
		dev_info(cs35l43->dev, "%s: failed to alloc write seq\n",
			__func__);
		return -ENOMEM;
	}

	ret = wm_adsp_read_ctl(&cs35l43->dsp, sequence->name,
			WMFW_ADSP2_XM, CS35L43_ALG_ID_PM, (void *)buf,
			sequence->length * sizeof(u32));
	if (ret != 0) {
		dev_info(dev, "%s: Failed to read control\n", __func__);
		goto err_free;
	}

	for (i = 0; i < sequence->length; i++) {
		buf[i] = be32_to_cpu(buf[i]);
		dev_dbg(dev, "%s[%d] = 0x%x\n", sequence->name, i, buf[i]);
	}

	dev_dbg(dev, "%s num ops: %d\n", sequence->name, sequence->num_ops);
	dev_dbg(dev, "offset\tsize\twords\n");
	list_for_each_entry(write_seq_elem, &sequence->list_head, list) {
		switch (write_seq_elem->operation) {
		case CS35L43_POWER_SEQ_OP_WRITE_REG_FULL:
			addr = ((write_seq_elem->words[0] & 0xFFFF) << 16) |
				((write_seq_elem->words[1] & 0xFFFF00) >> 8);
			value = ((write_seq_elem->words[1] & 0xFF) << 24) |
				(write_seq_elem->words[2] & 0xFFFFFF);
			break;
		case CS35L43_POWER_SEQ_OP_WRITE_REG_ADDR8:
			addr = (prev_addr & 0xFFFFFF00) |
				((write_seq_elem->words[0] & 0xFF00) >> 8);
			value = ((write_seq_elem->words[0] & 0xFF) << 24) |
				(write_seq_elem->words[1] & 0xFFFFFF);
			break;
		case CS35L43_POWER_SEQ_OP_WRITE_REG_L16:
			addr = ((write_seq_elem->words[0] & 0xFFFF) << 8) |
				((write_seq_elem->words[1] & 0xFF0000) >> 16);
			value = (write_seq_elem->words[1] & 0xFFFF);
			break;
		default:
			break;
		}
		dev_dbg(dev, "write seq elem: addr=0x%x, prev_addr=0x%x, val=0x%x\n",
								addr, prev_addr, value);
		prev_addr = addr;

		regmap_read(cs35l43->regmap, addr, &reg_value);

		if (reg_value != value && addr != CS35L43_TEST_KEY_CTRL &&
					cs35l43_readable_reg(dev, addr)) {
			dev_info(dev,
				"%s: Updating register 0x%x with value 0x%x\t(prev value: 0x%x)\n",
					__func__, addr, reg_value, value);
			cs35l43_write_seq_elem_update(write_seq_elem, addr, reg_value);
			memcpy(buf + write_seq_elem->offset, write_seq_elem->words,
					write_seq_elem->size * sizeof(u32));
			for (i = 0; i < write_seq_elem->size; i++)
				dev_dbg(dev, "elem[%d]: 0x%x\n", i, write_seq_elem->words[i]);
		}
	}

	for (i = 0; i < sequence->length; i++) {
		dev_dbg(dev, "%s[%d] = 0x%x\n", sequence->name, i, buf[i]);
		buf[i] = cpu_to_be32(buf[i]);
	}

	ret = wm_adsp_write_ctl(&cs35l43->dsp, sequence->name,
			WMFW_ADSP2_XM, CS35L43_ALG_ID_PM, (void *)buf,
			sequence->length * sizeof(u32));

err_free:
	kfree(buf);
	return ret;

}

static int cs35l43_write_seq_init(struct cs35l43_private *cs35l43,
					struct cs35l43_write_seq *sequence)
{
	struct device *dev = cs35l43->dev;
	u32 *buf, *op_words;
	u8 operation;
	unsigned int i, j, num_words, ret = 0;
	struct cs35l43_write_seq_elem *write_seq_elem;

	INIT_LIST_HEAD(&sequence->list_head);
	sequence->num_ops = 0;

	buf = kzalloc(sizeof(u32) * sequence->length, GFP_KERNEL);
	if (!buf) {
		dev_info(cs35l43->dev, "%s: failed to alloc write seq\n",
			__func__);
		return -ENOMEM;
	}

	ret = wm_adsp_read_ctl(&cs35l43->dsp, sequence->name,
			WMFW_ADSP2_XM, CS35L43_ALG_ID_PM, (void *)buf,
			sequence->length * sizeof(u32));
	if (ret != 0) {
		dev_info(dev, "%s: Failed to read control\n", __func__);
		goto err_free;
	}


	for (i = 0; i < sequence->length; i++) {
		buf[i] = be32_to_cpu(buf[i]);
		dev_dbg(dev, "%s[%d] = 0x%x\n", sequence->name, i, buf[i]);
	}

	i = 0;
	while (i < sequence->length) {
		operation = (buf[i] & CS35L43_POWER_SEQ_OP_MASK) >>
			CS35L43_POWER_SEQ_OP_SHIFT;

		if (operation == CS35L43_POWER_SEQ_OP_END)
			break;

		/* get num words for given operation */
		for (j = 0; j < CS35L43_POWER_SEQ_NUM_OPS; j++) {
			if (cs35l43_write_seq_op_sizes[j][0] == operation) {
				num_words = cs35l43_write_seq_op_sizes[j][1];
				break;
			}
		}

		if (j == CS35L43_POWER_SEQ_NUM_OPS) {
			dev_info(dev, "Failed to determine op size\n");
			return -EINVAL;
		}

		op_words = kzalloc(num_words * sizeof(u32), GFP_KERNEL);
		if (!op_words)
			return -ENOMEM;
		memcpy(op_words, &buf[i], num_words * sizeof(u32));

		write_seq_elem = devm_kzalloc(dev, sizeof(*write_seq_elem), GFP_KERNEL);
		if (!write_seq_elem) {
			ret = -ENOMEM;
			goto err_parse;
		}

		write_seq_elem->size = num_words;
		write_seq_elem->offset = i;
		write_seq_elem->operation = operation;
		write_seq_elem->words = op_words;
		list_add_tail(&write_seq_elem->list, &sequence->list_head);

		sequence->num_ops++;
		i += num_words;
	}

	dev_dbg(dev, "%s num ops: %d\n", sequence->name, sequence->num_ops);
	dev_dbg(dev, "offset\tsize\twords\n");
	list_for_each_entry(write_seq_elem, &sequence->list_head, list) {
		dev_dbg(dev, "0x%04X\t%d", write_seq_elem->offset,
							write_seq_elem->size);
		for (j = 0; j < write_seq_elem->size; j++)
			dev_dbg(dev, "0x%08X", *(write_seq_elem->words + j));
	}

	if (operation != CS35L43_POWER_SEQ_OP_END) {
		dev_info(dev, "WRITE SEQ END_OF_SCRIPT not found\n");
		ret = -E2BIG;
	}

	kfree(buf);
	return ret;

err_parse:
	kfree(op_words);
err_free:
	kfree(buf);
	return ret;
}

static int cs35l43_dsp_preload_ev(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cs35l43_private *cs35l43 =
		snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wm_adsp_early_event(w, kcontrol, event);
		break;
	case SND_SOC_DAPM_POST_PMU:
		regmap_write(cs35l43->regmap, CS35L43_PWRMGT_CTL, CS35L43_MEM_RDY);
		wm_adsp_event(w, kcontrol, event);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		wm_adsp_early_event(w, kcontrol, event);
		wm_adsp_event(w, kcontrol, event);
		cs35l43->hibernate_state = CS35L43_HIBERNATE_NOT_LOADED;
		break;
	default:
		break;
	}

	return 0;
}

static int cs35l43_dsp_audio_ev(struct snd_soc_dapm_widget *w,
		       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cs35l43_private *cs35l43 =
		snd_soc_component_get_drvdata(component);

	dev_dbg(cs35l43->dev, "%s\n", __func__);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		break;
	default:
		break;
	}

	return 0;
}

static void cs35l43_pll_config(struct cs35l43_private *cs35l43)
{

	regmap_update_bits(cs35l43->regmap, CS35L43_REFCLK_INPUT,
			CS35L43_PLL_OPEN_LOOP_MASK,
			CS35L43_PLL_OPEN_LOOP_MASK);
	regmap_update_bits(cs35l43->regmap, CS35L43_REFCLK_INPUT,
			CS35L43_PLL_REFCLK_FREQ_MASK,
			cs35l43->extclk_cfg << CS35L43_PLL_REFCLK_FREQ_SHIFT);
	regmap_update_bits(cs35l43->regmap, CS35L43_REFCLK_INPUT,
			CS35L43_PLL_REFCLK_EN_MASK, 0);
	regmap_update_bits(cs35l43->regmap, CS35L43_REFCLK_INPUT,
			CS35L43_PLL_REFCLK_SEL_MASK, cs35l43->clk_id);
	regmap_update_bits(cs35l43->regmap, CS35L43_REFCLK_INPUT,
			CS35L43_PLL_OPEN_LOOP_MASK,
			0);
	regmap_update_bits(cs35l43->regmap, CS35L43_REFCLK_INPUT,
			CS35L43_PLL_REFCLK_EN_MASK,
			CS35L43_PLL_REFCLK_EN_MASK);
}

static int cs35l45_check_mailbox(struct cs35l43_private *cs35l43)
{
	unsigned int *mbox;
	int i;

	mbox = kmalloc_array(8, sizeof(*mbox), GFP_KERNEL);
	if (!mbox)
		return -ENOMEM;

	regmap_bulk_read(cs35l43->regmap, CS35L43_DSP_MBOX_1, mbox, 8);
	for (i = 0; i < 8; i++)
		dev_dbg(cs35l43->dev, "mbox[%d]: 0x%x\n", i + 1, mbox[i]);

	kfree(mbox);
	return 0;
}

static int cs35l43_enter_hibernate(struct cs35l43_private *cs35l43)
{
	if (cs35l43->hibernate_state != CS35L43_HIBERNATE_AWAKE)
		return 0;

	dev_info(cs35l43->dev, "%s\n", __func__);

	cs35l43_write_seq_update(cs35l43, &cs35l43->power_on_seq);

	regmap_write(cs35l43->regmap, CS35L43_DSP_VIRTUAL1_MBOX_1,
					CS35L43_MBOX_CMD_ALLOW_HIBERNATE);
	regmap_write(cs35l43->regmap, CS35L43_DSP_VIRTUAL1_MBOX_1,
					CS35L43_MBOX_CMD_HIBERNATE);

	cs35l43->hibernate_state = CS35L43_HIBERNATE_STANDBY;
	return 0;
}

static int cs35l43_exit_hibernate(struct cs35l43_private *cs35l43)
{
	if (cs35l43->hibernate_state != CS35L43_HIBERNATE_STANDBY &&
		cs35l43->hibernate_state != CS35L43_HIBERNATE_UPDATE)
		return 0;

	dev_info(cs35l43->dev, "%s\n", __func__);

	regmap_write(cs35l43->regmap, CS35L43_DSP_VIRTUAL1_MBOX_1,
					CS35L43_MBOX_CMD_WAKEUP);
	regmap_write(cs35l43->regmap, CS35L43_DSP_VIRTUAL1_MBOX_1,
					CS35L43_MBOX_CMD_PREVENT_HIBERNATE);
	regcache_drop_region(cs35l43->regmap, CS35L43_DEVID,
					CS35L43_MIXER_NGATE_CH2_CFG);

	cs35l43->hibernate_state = CS35L43_HIBERNATE_AWAKE;

	regmap_write(cs35l43->regmap, CS35L43_IRQ1_MASK_1, 0xFFFFFFFF);
	regmap_update_bits(cs35l43->regmap, CS35L43_IRQ1_MASK_1,
				CS35L43_AMP_ERR_EINT1_MASK |
				CS35L43_BST_SHORT_ERR_EINT1_MASK |
				CS35L43_BST_DCM_UVP_ERR_EINT1_MASK |
				CS35L43_BST_OVP_ERR_EINT1_MASK |
				CS35L43_DSP_VIRTUAL2_MBOX_WR_EINT1_MASK |
				CS35L43_DC_WATCHDOG_IRQ_RISE_EINT1_MASK |
				CS35L43_WKSRC_STATUS6_EINT1_MASK |
				CS35L43_WKSRC_STATUS_ANY_EINT1_MASK, 0);

	return 0;
}

static void cs35l43_hibernate_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct cs35l43_private *cs35l43 =
		container_of(dwork, struct cs35l43_private, hb_work);

	mutex_lock(&cs35l43->hb_lock);
	cs35l43_enter_hibernate(cs35l43);
	mutex_unlock(&cs35l43->hb_lock);
}

static int cs35l43_hibernate(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	struct cs35l43_private *cs35l43 =
		snd_soc_component_get_drvdata(component);
	int ret = 0, i;

	if (cs35l43->hibernate_state == CS35L43_HIBERNATE_DISABLED)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (cs35l43->hibernate_state == CS35L43_HIBERNATE_NOT_LOADED &&
							cs35l43->dsp.running) {
			cs35l43->power_on_seq.name = "POWER_ON_SEQUENCE";
			cs35l43->power_on_seq.length = CS35L43_POWER_SEQ_MAX_WORDS;
			cs35l43_write_seq_init(cs35l43, &cs35l43->power_on_seq);
			cs35l43_write_seq_update(cs35l43, &cs35l43->power_on_seq);

			for (i = 0; i < ARRAY_SIZE(cs35l43_hibernate_update_regs); i++) {
				if (cs35l43_hibernate_update_regs[i] == 0)
					break;
				cs35l43_write_seq_add(cs35l43, &cs35l43->power_on_seq,
						cs35l43_hibernate_update_regs[i],
						0, true);
			}

			cs35l43->hibernate_state = CS35L43_HIBERNATE_AWAKE;
			cs35l43->hibernate_delay_ms = 2000;
		}
		if (cs35l43->hibernate_state == CS35L43_HIBERNATE_AWAKE &&
							cs35l43->dsp.running)
			queue_delayed_work(cs35l43->wq, &cs35l43->hb_work,
				msecs_to_jiffies(cs35l43->hibernate_delay_ms));
		break;
	default:
		dev_info(cs35l43->dev, "Invalid event = 0x%x\n", event);
		ret = -EINVAL;
	}
	return ret;
}

static int cs35l43_main_amp_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	struct cs35l43_private *cs35l43 =
				snd_soc_component_get_drvdata(component);
	int ret = 0;

	dev_dbg(cs35l43->dev, "%s\n", __func__);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		regmap_update_bits(cs35l43->regmap,
					CS35L43_BLOCK_ENABLES, 1, 1);
		regmap_write(cs35l43->regmap, CS35L43_GLOBAL_ENABLES, 1);
		regmap_write(cs35l43->regmap, CS35L43_DSP_VIRTUAL1_MBOX_1,
				CS35L43_MBOX_CMD_AUDIO_PLAY);

		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_write(cs35l43->regmap, CS35L43_GLOBAL_ENABLES, 0);
		regmap_write(cs35l43->regmap, CS35L43_BLOCK_ENABLES, 0);
		regmap_write(cs35l43->regmap, CS35L43_DSP_VIRTUAL1_MBOX_1,
				CS35L43_MBOX_CMD_AUDIO_PAUSE);
		cs35l45_check_mailbox(cs35l43);
		break;
	default:
		dev_info(cs35l43->dev, "Invalid event = 0x%x\n", event);
		ret = -EINVAL;
	}
	return ret;
}

static const struct snd_soc_dapm_widget cs35l43_dapm_widgets[] = {

	SND_SOC_DAPM_OUT_DRV_E("Main AMP", SND_SOC_NOPM, 0, 0, NULL, 0,
			cs35l43_main_amp_event,
			SND_SOC_DAPM_POST_PMD |	SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_SUPPLY("Hibernate",  SND_SOC_NOPM, 0, 0,
			    cs35l43_hibernate,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUTPUT("SPK"),

	SND_SOC_DAPM_SPK("DSP1 Preload", NULL),
	SND_SOC_DAPM_SUPPLY_S("DSP1 Preloader", 100,
				SND_SOC_NOPM, 0, 0, cs35l43_dsp_preload_ev,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUT_DRV_E("DSP1", SND_SOC_NOPM, 0, 0, NULL, 0,
				cs35l43_dsp_audio_ev, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_AIF_IN("ASPRX1", NULL, 0, CS35L43_ASP_ENABLES1,
					CS35L43_ASP_RX1_EN_SHIFT, 0),
	SND_SOC_DAPM_AIF_IN("ASPRX2", NULL, 0, CS35L43_ASP_ENABLES1,
					CS35L43_ASP_RX2_EN_SHIFT, 0),
	SND_SOC_DAPM_AIF_OUT("ASPTX1", NULL, 0, CS35L43_ASP_ENABLES1,
					CS35L43_ASP_TX1_EN_SHIFT, 0),
	SND_SOC_DAPM_AIF_OUT("ASPTX2", NULL, 0, CS35L43_ASP_ENABLES1,
					CS35L43_ASP_TX2_EN_SHIFT, 0),

	SND_SOC_DAPM_MUX("ASP TX1 Source", SND_SOC_NOPM, 0, 0, &asp_tx1_mux),
	SND_SOC_DAPM_MUX("ASP TX2 Source", SND_SOC_NOPM, 0, 0, &asp_tx2_mux),
	SND_SOC_DAPM_MUX("ASP TX3 Source", SND_SOC_NOPM, 0, 0, &asp_tx3_mux),
	SND_SOC_DAPM_MUX("ASP TX4 Source", SND_SOC_NOPM, 0, 0, &asp_tx4_mux),
	SND_SOC_DAPM_MUX("DSP RX1 Source", SND_SOC_NOPM, 0, 0, &dsp_rx1_mux),
	SND_SOC_DAPM_MUX("DSP RX2 Source", SND_SOC_NOPM, 0, 0, &dsp_rx2_mux),
	SND_SOC_DAPM_MUX("PCM Source", SND_SOC_NOPM, 0, 0, &dacpcm_mux),
	SND_SOC_DAPM_MUX("High Rate PCM Source", SND_SOC_NOPM, 0, 0, &dacpcm2_mux),
	SND_SOC_DAPM_MUX("Ultrasonic Mode", SND_SOC_NOPM, 0, 0, &ultra_mux),

	SND_SOC_DAPM_ADC("VMON ADC", NULL, CS35L43_BLOCK_ENABLES,
					CS35L43_VMON_EN_SHIFT, 0),
	SND_SOC_DAPM_ADC("IMON ADC", NULL, CS35L43_BLOCK_ENABLES,
					CS35L43_IMON_EN_SHIFT, 0),
	SND_SOC_DAPM_ADC("VPMON ADC", NULL, CS35L43_BLOCK_ENABLES,
					CS35L43_VPMON_EN_SHIFT, 0),
	SND_SOC_DAPM_ADC("VBSTMON ADC", NULL, CS35L43_BLOCK_ENABLES,
					CS35L43_VBSTMON_EN_SHIFT, 0),
	SND_SOC_DAPM_ADC("TEMPMON ADC", NULL, CS35L43_BLOCK_ENABLES,
					CS35L43_TEMPMON_EN_SHIFT, 0),
	SND_SOC_DAPM_SWITCH("AMP Enable", SND_SOC_NOPM, 0, 1, &amp_enable_ctrl),
};

static const struct snd_soc_dapm_route cs35l43_audio_map[] = {


	{ "DSP1", NULL, "DSP1 Preloader" },
	{ "DSP1 Preload", NULL, "DSP1 Preloader" },

	{"DSP RX1 Source", "VMON", "VMON ADC"},
	{"DSP RX1 Source", "IMON", "IMON ADC"},
	{"DSP RX1 Source", "VPMON", "VPMON ADC"},
	{"DSP RX1 Source", "ASPRX1", "ASPRX1"},
	{"DSP RX1 Source", "ASPRX2", "ASPRX2"},
	{"DSP RX1 Source", "Zero", "ASPRX1"},
	{"DSP1", NULL, "DSP RX1 Source"},

	{"DSP RX2 Source", "VMON", "VMON ADC"},
	{"DSP RX2 Source", "IMON", "IMON ADC"},
	{"DSP RX2 Source", "VPMON", "VPMON ADC"},
	{"DSP RX2 Source", "ASPRX1", "ASPRX1"},
	{"DSP RX2 Source", "ASPRX2", "ASPRX2"},
	{"DSP RX2 Source", "Zero", "ASPRX1"},
	{"DSP1", NULL, "DSP RX2 Source"},

	{"PCM Source", "ASPRX1", "ASPRX1"},
	{"PCM Source", "ASPRX1", "ASPRX1"},
	{"PCM Source", "DSP", "DSP1"},
	{"PCM Source", "DSP FS2", "DSP1"},
	{"High Rate PCM Source", "ASPRX1", "ASPRX1"},
	{"High Rate PCM Source", "ASPRX1", "ASPRX1"},
	{"High Rate PCM Source", "DSP", "DSP1"},
	{"High Rate PCM Source", "DSP FS2", "DSP1"},
	{"Ultrasonic Mode", "In Band", "High Rate PCM Source"},
	{"Ultrasonic Mode", "Out of Band", "High Rate PCM Source"},
	{"Main AMP", NULL, "Ultrasonic Mode"},
	{"Main AMP", NULL, "PCM Source"},
	{"SPK", NULL, "Main AMP"},
	{"SPK", NULL, "Hibernate"},

	{"ASP TX1 Source", "ASPRX1", "ASPRX1"},
	{"ASP TX2 Source", "ASPRX1", "ASPRX1"},
	{"ASP TX3 Source", "ASPRX1", "ASPRX1"},
	{"ASP TX4 Source", "ASPRX1", "ASPRX1"},
	{"DSP RX1 Source", "ASPRX1", "ASPRX1"},
	{"DSP RX2 Source", "ASPRX1", "ASPRX1"},
	{"ASP TX1 Source", "ASPRX2", "ASPRX2"},
	{"ASP TX2 Source", "ASPRX2", "ASPRX2"},
	{"ASP TX3 Source", "ASPRX2", "ASPRX2"},
	{"ASP TX4 Source", "ASPRX2", "ASPRX2"},
	{"DSP RX1 Source", "ASPRX2", "ASPRX2"},
	{"DSP RX2 Source", "ASPRX2", "ASPRX2"},
	{"ASP TX1 Source", "VMON", "VMON ADC"},
	{"ASP TX2 Source", "VMON", "VMON ADC"},
	{"ASP TX3 Source", "VMON", "VMON ADC"},
	{"ASP TX4 Source", "VMON", "VMON ADC"},
	{"DSP RX1 Source", "VMON", "VMON ADC"},
	{"DSP RX2 Source", "VMON", "VMON ADC"},
	{"ASP TX1 Source", "VMON FS2", "VMON ADC"},
	{"ASP TX2 Source", "VMON FS2", "VMON ADC"},
	{"ASP TX3 Source", "VMON FS2", "VMON ADC"},
	{"ASP TX4 Source", "VMON FS2", "VMON ADC"},
	{"DSP RX1 Source", "VMON FS2", "VMON ADC"},
	{"DSP RX2 Source", "VMON FS2", "VMON ADC"},
	{"ASP TX1 Source", "IMON", "IMON ADC"},
	{"ASP TX2 Source", "IMON", "IMON ADC"},
	{"ASP TX3 Source", "IMON", "IMON ADC"},
	{"ASP TX4 Source", "IMON", "IMON ADC"},
	{"DSP RX1 Source", "IMON", "IMON ADC"},
	{"DSP RX2 Source", "IMON", "IMON ADC"},
	{"ASP TX1 Source", "IMON FS2", "IMON ADC"},
	{"ASP TX2 Source", "IMON FS2", "IMON ADC"},
	{"ASP TX3 Source", "IMON FS2", "IMON ADC"},
	{"ASP TX4 Source", "IMON FS2", "IMON ADC"},
	{"DSP RX1 Source", "IMON FS2", "IMON ADC"},
	{"DSP RX2 Source", "IMON FS2", "IMON ADC"},
	{"ASP TX1 Source", "VPMON", "VPMON ADC"},
	{"ASP TX2 Source", "VPMON", "VPMON ADC"},
	{"ASP TX3 Source", "VPMON", "VPMON ADC"},
	{"ASP TX4 Source", "VPMON", "VPMON ADC"},
	{"DSP RX1 Source", "VPMON", "VPMON ADC"},
	{"DSP RX2 Source", "VPMON", "VPMON ADC"},
	{"ASP TX1 Source", "VBSTMON", "VBSTMON ADC"},
	{"ASP TX2 Source", "VBSTMON", "VBSTMON ADC"},
	{"ASP TX3 Source", "VBSTMON", "VBSTMON ADC"},
	{"ASP TX4 Source", "VBSTMON", "VBSTMON ADC"},
	{"DSP RX1 Source", "VBSTMON", "VBSTMON ADC"},
	{"DSP RX2 Source", "VBSTMON", "VBSTMON ADC"},
	{"ASP TX1 Source", "DSP", "DSP1"},
	{"ASP TX2 Source", "DSP", "DSP1"},
	{"ASP TX3 Source", "DSP", "DSP1"},
	{"ASP TX4 Source", "DSP", "DSP1"},
	{"ASP TX1 Source", "DSP FS2", "DSP1"},
	{"ASP TX2 Source", "DSP FS2", "DSP1"},
	{"ASP TX3 Source", "DSP FS2", "DSP1"},
	{"ASP TX4 Source", "DSP FS2", "DSP1"},
	{"ASPTX1", NULL, "ASP TX1 Source"},
	{"ASPTX2", NULL, "ASP TX2 Source"},
	{"AMP Capture", NULL, "ASPTX1"},
	{"AMP Capture", NULL, "ASPTX2"},

	{"DSP1", NULL, "IMON ADC"},
	{"DSP1", NULL, "VMON ADC"},
	{"DSP1", NULL, "VBSTMON ADC"},
	{"DSP1", NULL, "VPMON ADC"},
	{"DSP1", NULL, "TEMPMON ADC"},

	{"AMP Enable", "Switch", "AMP Playback"},

	{"ASPRX1", NULL, "AMP Enable"},
	{"ASPRX2", NULL, "AMP Enable"},
	{"VMON ADC", NULL, "AMP Enable"},
	{"IMON ADC", NULL, "AMP Enable"},
	{"VPMON ADC", NULL, "AMP Enable"},
	{"VBSTMON ADC", NULL, "AMP Enable"},
	{"TEMPMON ADC", NULL, "AMP Enable"},
};


static irqreturn_t cs35l43_irq(int irq, void *data)
{
	struct cs35l43_private *cs35l43 = data;
	unsigned int status, mask;


	regmap_read(cs35l43->regmap, CS35L43_IRQ1_EINT_1, &status);
	regmap_read(cs35l43->regmap, CS35L43_IRQ1_MASK_1, &mask);

	/* Check to see if unmasked bits are active */
	if (!(status & ~mask))
		return IRQ_NONE;

	/*
	 * The following interrupts require a
	 * protection release cycle to get the
	 * speaker out of Safe-Mode.
	 */
	if (status & CS35L43_AMP_ERR_EINT1_MASK) {
		dev_info(cs35l43->dev, "Amp short error\n");
		regmap_write(cs35l43->regmap, CS35L43_IRQ1_EINT_1,
					CS35L43_AMP_ERR_EINT1_MASK);
		regmap_write(cs35l43->regmap, CS35L43_ERROR_RELEASE, 0);
		regmap_update_bits(cs35l43->regmap, CS35L43_ERROR_RELEASE,
					CS35L43_AMP_SHORT_ERR_RLS_MASK,
					CS35L43_AMP_SHORT_ERR_RLS_MASK);
		regmap_update_bits(cs35l43->regmap, CS35L43_ERROR_RELEASE,
					CS35L43_AMP_SHORT_ERR_RLS_MASK, 0);
	}

	if (status & CS35L43_BST_OVP_ERR_EINT1_MASK) {
		dev_info(cs35l43->dev, "VBST Over Voltage error\n");
		regmap_update_bits(cs35l43->regmap, CS35L43_BLOCK_ENABLES,
					CS35L43_BST_EN_MASK <<
					CS35L43_BST_EN_SHIFT, 0);
		regmap_write(cs35l43->regmap, CS35L43_IRQ1_EINT_1,
					CS35L43_BST_OVP_ERR_EINT1_MASK);
		regmap_write(cs35l43->regmap, CS35L43_ERROR_RELEASE, 0);
		regmap_update_bits(cs35l43->regmap, CS35L43_ERROR_RELEASE,
					CS35L43_BST_OVP_ERR_RLS_MASK,
					CS35L43_BST_OVP_ERR_RLS_MASK);
		regmap_update_bits(cs35l43->regmap, CS35L43_ERROR_RELEASE,
					CS35L43_BST_OVP_ERR_RLS_MASK, 0);
		regmap_update_bits(cs35l43->regmap, CS35L43_BLOCK_ENABLES,
					CS35L43_BST_EN_MASK <<
					CS35L43_BST_EN_SHIFT,
					CS35L43_BST_EN_DEFAULT <<
					CS35L43_BST_EN_SHIFT);
	}

	if (status & CS35L43_BST_DCM_UVP_ERR_EINT1_MASK) {
		dev_info(cs35l43->dev, "DCM VBST Under Voltage Error\n");
		regmap_update_bits(cs35l43->regmap, CS35L43_BLOCK_ENABLES,
					CS35L43_BST_EN_MASK <<
					CS35L43_BST_EN_SHIFT, 0);
		regmap_write(cs35l43->regmap, CS35L43_IRQ1_EINT_1,
					CS35L43_BST_DCM_UVP_ERR_EINT1_MASK);
		regmap_write(cs35l43->regmap, CS35L43_ERROR_RELEASE, 0);
		regmap_update_bits(cs35l43->regmap, CS35L43_ERROR_RELEASE,
					CS35L43_BST_UVP_ERR_RLS_MASK,
					CS35L43_BST_UVP_ERR_RLS_MASK);
		regmap_update_bits(cs35l43->regmap, CS35L43_ERROR_RELEASE,
					CS35L43_BST_UVP_ERR_RLS_MASK, 0);
		regmap_update_bits(cs35l43->regmap, CS35L43_BLOCK_ENABLES,
					CS35L43_BST_EN_MASK <<
					CS35L43_BST_EN_SHIFT,
					CS35L43_BST_EN_DEFAULT <<
					CS35L43_BST_EN_SHIFT);
	}

	if (status & CS35L43_BST_SHORT_ERR_EINT1_MASK) {
		dev_info(cs35l43->dev, "LBST error: powering off!\n");
		regmap_update_bits(cs35l43->regmap, CS35L43_BLOCK_ENABLES,
					CS35L43_BST_EN_MASK <<
					CS35L43_BST_EN_SHIFT, 0);
		regmap_write(cs35l43->regmap, CS35L43_IRQ1_EINT_1,
					CS35L43_BST_SHORT_ERR_EINT1_MASK);
		regmap_write(cs35l43->regmap, CS35L43_ERROR_RELEASE, 0);
		regmap_update_bits(cs35l43->regmap, CS35L43_ERROR_RELEASE,
					CS35L43_BST_SHORT_ERR_RLS_MASK,
					CS35L43_BST_SHORT_ERR_RLS_MASK);
		regmap_update_bits(cs35l43->regmap, CS35L43_ERROR_RELEASE,
					CS35L43_BST_SHORT_ERR_RLS_MASK, 0);
		regmap_update_bits(cs35l43->regmap, CS35L43_BLOCK_ENABLES,
					CS35L43_BST_EN_MASK <<
					CS35L43_BST_EN_SHIFT,
					CS35L43_BST_EN_DEFAULT <<
					CS35L43_BST_EN_SHIFT);
	}

	if (status & CS35L43_DC_WATCHDOG_IRQ_RISE_EINT1_MASK) {
		dev_info(cs35l43->dev, "DC Detect INT\n");
		regmap_write(cs35l43->regmap, CS35L43_IRQ1_EINT_1,
				CS35L43_DC_WATCHDOG_IRQ_RISE_EINT1_MASK);
		regmap_write(cs35l43->regmap, CS35L43_IRQ1_EINT_1,
				CS35L43_DC_WATCHDOG_IRQ_RISE_EINT1_MASK);
	}

	if (status & CS35L43_DSP_VIRTUAL2_MBOX_WR_EINT1_MASK ||
		status & CS35L43_WKSRC_STATUS6_EINT1_MASK) {
		dev_info(cs35l43->dev, "Wakeup INT\n");
		regmap_write(cs35l43->regmap, CS35L43_IRQ1_EINT_1,
				CS35L43_WKSRC_STATUS_ANY_EINT1_MASK);
		regmap_write(cs35l43->regmap, CS35L43_IRQ1_EINT_1,
				CS35L43_WKSRC_STATUS6_EINT1_MASK);
	}


	if (status & CS35L43_DSP_VIRTUAL2_MBOX_WR_EINT1_MASK) {
		dev_info(cs35l43->dev, "Received Mailbox INT\n");
		regmap_write(cs35l43->regmap, CS35L43_IRQ1_EINT_1,
				CS35L43_DSP_VIRTUAL2_MBOX_WR_EINT1_MASK);
		cs35l45_check_mailbox(cs35l43);
	}

	return IRQ_HANDLED;
}

static int cs35l43_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct cs35l43_private *cs35l43 =
			snd_soc_component_get_drvdata(codec_dai->component);

	dev_dbg(cs35l43->dev, "%s\n", __func__);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		dev_info(cs35l43->dev,
			"%s: Master mode unsupported\n", __func__);
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		cs35l43->asp_fmt = 0;
		break;
	case SND_SOC_DAIFMT_I2S:
		cs35l43->asp_fmt = 2;
		break;
	default:
		dev_info(cs35l43->dev,
			"%s: Invalid or unsupported DAI format\n", __func__);
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_IF:
		cs35l43->lrclk_fmt = 1;
		cs35l43->sclk_fmt = 0;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		cs35l43->lrclk_fmt = 0;
		cs35l43->sclk_fmt = 1;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		cs35l43->lrclk_fmt = 1;
		cs35l43->sclk_fmt = 1;
		break;
	case SND_SOC_DAIFMT_NB_NF:
		cs35l43->lrclk_fmt = 0;
		cs35l43->sclk_fmt = 0;
		break;
	default:
		dev_info(cs35l43->dev,
			"%s: Invalid DAI clock INV\n", __func__);
		return -EINVAL;
	}

	regmap_update_bits(cs35l43->regmap, CS35L43_ASP_CONTROL2,
				CS35L43_ASP_FMT_MASK | CS35L43_ASP_BCLK_INV_MASK |
				CS35L43_ASP_FSYNC_INV_MASK,
				(cs35l43->asp_fmt << CS35L43_ASP_FMT_SHIFT) |
				(cs35l43->lrclk_fmt << CS35L43_ASP_FSYNC_INV_SHIFT) |
				(cs35l43->sclk_fmt << CS35L43_ASP_BCLK_INV_SHIFT));
	return 0;
}

struct cs35l43_global_fs_config {
	int rate;
	int fs_cfg;
};

static const struct cs35l43_global_fs_config cs35l43_fs_rates[] = {
	{ 12000,	0x01 },
	{ 24000,	0x02 },
	{ 48000,	0x03 },
	{ 96000,	0x04 },
	{ 192000,	0x05 },
	{ 11025,	0x09 },
	{ 22050,	0x0A },
	{ 44100,	0x0B },
	{ 88200,	0x0C },
	{ 176400,	0x0D },
	{ 8000,		0x11 },
	{ 16000,	0x12 },
	{ 32000,	0x13 },
};

static int cs35l43_pcm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	int i;
	unsigned int rate = params_rate(params);
	u8 asp_width, asp_wl;
	struct cs35l43_private *cs35l43 =
				snd_soc_component_get_drvdata(dai->component);

	dev_dbg(cs35l43->dev, "%s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(cs35l43_fs_rates); i++) {
		if (rate == cs35l43_fs_rates[i].rate)
			break;
	}

	if (i < ARRAY_SIZE(cs35l43_fs_rates) &&
			cs35l43->ultrasonic_mode == CS35L43_ULTRASONIC_MODE_DISABLED)
		regmap_update_bits(cs35l43->regmap, CS35L43_GLOBAL_SAMPLE_RATE,
			CS35L43_GLOBAL_FS_MASK, cs35l43_fs_rates[i].fs_cfg);
	else if (cs35l43->ultrasonic_mode != CS35L43_ULTRASONIC_MODE_DISABLED)
		/* Assume 48k base rate */
		regmap_update_bits(cs35l43->regmap, CS35L43_GLOBAL_SAMPLE_RATE,
			CS35L43_GLOBAL_FS_MASK, 0x03);
	else {
		dev_info(cs35l43->dev, "%s: Unsupported rate\n", __func__);
		return -EINVAL;
	}

	asp_wl = params_width(params);
	asp_wl = asp_wl > CS35L43_SP_WL_MAX ? CS35L43_SP_WL_MAX : asp_wl;
	asp_width = params_physical_width(params);
	dev_dbg(cs35l43->dev, "%s\n wl=%d, width=%d, rate=%d", __func__,
				asp_wl, asp_width, rate);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		regmap_update_bits(cs35l43->regmap, CS35L43_ASP_CONTROL2,
				CS35L43_ASP_RX_WIDTH_MASK, asp_width <<
				CS35L43_ASP_RX_WIDTH_SHIFT);
		regmap_update_bits(cs35l43->regmap, CS35L43_ASP_DATA_CONTROL5,
				CS35L43_ASP_RX_WL_MASK, asp_wl);
	} else {
		regmap_update_bits(cs35l43->regmap, CS35L43_ASP_CONTROL2,
				CS35L43_ASP_TX_WIDTH_MASK, asp_width <<
				CS35L43_ASP_TX_WIDTH_SHIFT);
		regmap_update_bits(cs35l43->regmap, CS35L43_ASP_DATA_CONTROL1,
				CS35L43_ASP_TX_WL_MASK, asp_wl);
	}

	return 0;
}

static int cs35l43_get_clk_config(int freq)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cs35l43_pll_sysclk); i++) {
		if (cs35l43_pll_sysclk[i].freq == freq)
			return cs35l43_pll_sysclk[i].clk_cfg;
	}

	return -EINVAL;
}

static const unsigned int cs35l43_src_rates[] = {
	8000, 12000, 11025, 16000, 22050, 24000, 32000,
	44100, 48000, 88200, 96000, 176400, 192000
};

static const struct snd_pcm_hw_constraint_list cs35l43_constraints = {
	.count = ARRAY_SIZE(cs35l43_src_rates),
	.list = cs35l43_src_rates,
};

static int cs35l43_pcm_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct cs35l43_private *cs35l43 =
				snd_soc_component_get_drvdata(dai->component);
	int ret = 0;

	dev_info(cs35l43->dev, "%s\n", __func__);

	if (cs35l43->hibernate_state == CS35L43_HIBERNATE_STANDBY) {
		cancel_delayed_work(&cs35l43->hb_work);
		mutex_lock(&cs35l43->hb_lock);
		ret = cs35l43_exit_hibernate(cs35l43);
		mutex_unlock(&cs35l43->hb_lock);
	}

	if (substream->runtime)
		return snd_pcm_hw_constraint_list(substream->runtime, 0,
				SNDRV_PCM_HW_PARAM_RATE, &cs35l43_constraints);
	return ret;
}

static int cs35l41_get_fs_mon_config_index(int freq)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cs35l43_fs_mon); i++) {
		if (cs35l43_fs_mon[i].freq == freq)
			return i;
	}

	return -EINVAL;
}

static int cs35l43_component_set_sysclk(struct snd_soc_component *component,
				int clk_id, int source, unsigned int freq,
				int dir)
{
	struct cs35l43_private *cs35l43 =
				snd_soc_component_get_drvdata(component);
	unsigned int fs1_val;
	unsigned int fs2_val;
	unsigned int val;
	int fsIndex;

	dev_dbg(cs35l43->dev, "%s\n", __func__);
	dev_dbg(cs35l43->dev, "%s id = %d, freq=%d\n", __func__, clk_id, freq);

	cs35l43->extclk_cfg = cs35l43_get_clk_config(freq);
	cs35l43->clk_id = clk_id;

	if (freq <= 6000000) {
		/* Use the lookup table */
		fsIndex = cs35l41_get_fs_mon_config_index(freq);
		if (fsIndex < 0) {
			dev_info(cs35l43->dev, "Invalid CLK Config freq: %u\n", freq);
			return -EINVAL;
		}

		fs1_val = cs35l43_fs_mon[fsIndex].fs1;
		fs2_val = cs35l43_fs_mon[fsIndex].fs2;
	} else {
		/* Use hard-coded values */
		fs1_val = 18;
		fs2_val = 33;
	}

	val = fs1_val;
	val |= (fs2_val << CS35L43_FS2_START_WINDOW_SHIFT) & CS35L43_FS2_START_WINDOW_MASK;

	if (cs35l43->extclk_cfg < 0) {
		dev_info(cs35l43->dev, "Invalid CLK Config: %d, freq: %u\n",
			cs35l43->extclk_cfg, freq);
		return -EINVAL;
	}

	if (cs35l43->hibernate_state != CS35L43_HIBERNATE_STANDBY) {
		cs35l43_pll_config(cs35l43);
		regmap_write(cs35l43->regmap, CS35L43_FS_MON_0, val);
	}

	return 0;
}

static int cs35l43_dai_set_sysclk(struct snd_soc_dai *dai,
					int clk_id, unsigned int freq, int dir)
{
	struct cs35l43_private *cs35l43 =
				snd_soc_component_get_drvdata(dai->component);

	dev_dbg(cs35l43->dev, "%s\n", __func__);

	return 0;
}

int cs35l43_component_write(struct snd_soc_component *component,
				unsigned int reg, unsigned int val)
{
	struct cs35l43_private *cs35l43 =
				snd_soc_component_get_drvdata(component);
	int ret = 0;

	if (cs35l43->hibernate_state == CS35L43_HIBERNATE_STANDBY) {
		mutex_lock(&cs35l43->hb_lock);
		ret = cs35l43_exit_hibernate(cs35l43);
		regmap_write(cs35l43->regmap, reg, val);
		mutex_unlock(&cs35l43->hb_lock);
		queue_delayed_work(cs35l43->wq, &cs35l43->hb_work,
				msecs_to_jiffies(cs35l43->hibernate_delay_ms));
		return ret;
	} else
		return regmap_write(cs35l43->regmap, reg, val);
}

unsigned int cs35l43_component_read(struct snd_soc_component *component,
				unsigned int reg)
{
	struct cs35l43_private *cs35l43 =
				snd_soc_component_get_drvdata(component);
	unsigned int val;

	regmap_read(cs35l43->regmap, reg, &val);

	return val;
}

static int cs35l43_irq_gpio_config(struct cs35l43_private *cs35l43)
{
	int irq_pol = IRQF_TRIGGER_NONE;

	if (cs35l43->pdata.gpio1_out_enable)
		regmap_update_bits(cs35l43->regmap,
					CS35L43_GPIO1_CTRL1,
					CS35L43_GP1_DIR_MASK,
					0);
	if (cs35l43->pdata.gpio1_src_sel)
		regmap_update_bits(cs35l43->regmap,
					CS35L43_GPIO_PAD_CONTROL,
					CS35L43_GP1_CTRL_MASK,
					cs35l43->pdata.gpio1_src_sel <<
					CS35L43_GP1_CTRL_SHIFT);

	if (cs35l43->pdata.gpio2_out_enable)
			regmap_update_bits(cs35l43->regmap,
						CS35L43_GPIO2_CTRL1,
						CS35L43_GP2_DIR_MASK,
						0);
	if (cs35l43->pdata.gpio2_src_sel)
		regmap_update_bits(cs35l43->regmap,
					CS35L43_GPIO_PAD_CONTROL,
					CS35L43_GP2_CTRL_MASK,
					cs35l43->pdata.gpio2_src_sel <<
					CS35L43_GP2_CTRL_SHIFT);

	if (cs35l43->pdata.gpio2_src_sel ==
		  (CS35L43_GP2_CTRL_OPEN_DRAIN_ACTV_LO | CS35L43_VALID_PDATA) ||
		  cs35l43->pdata.gpio2_src_sel ==
		  (CS35L43_GP2_CTRL_PUSH_PULL_ACTV_LO | CS35L43_VALID_PDATA))
		irq_pol = IRQF_TRIGGER_LOW;
	else if (cs35l43->pdata.gpio2_src_sel ==
		     (CS35L43_GP2_CTRL_PUSH_PULL_ACTV_HI | CS35L43_VALID_PDATA))
		irq_pol = IRQF_TRIGGER_HIGH;

	return irq_pol;
}

static int cs35l43_set_pdata(struct cs35l43_private *cs35l43)
{
	if (cs35l43->pdata.bst_vctrl)
		regmap_update_bits(cs35l43->regmap, CS35L43_VBST_CTL_1,
				CS35L43_BST_CTL_MASK, cs35l43->pdata.bst_vctrl);

	if (cs35l43->pdata.classh_disable)
		regmap_update_bits(cs35l43->regmap, CS35L43_VBST_CTL_2,
				CS35L43_BST_CTL_SEL_MASK, 0);
	else {
		if (cs35l43->pdata.bst_vctrl)
			regmap_update_bits(cs35l43->regmap, CS35L43_VBST_CTL_2,
				CS35L43_BST_CTL_LIM_EN_MASK,
				CS35L43_BST_CTL_LIM_EN_MASK);
	}

	if (cs35l43->pdata.dsp_ng_enable) {
		regmap_update_bits(cs35l43->regmap,
				CS35L43_MIXER_NGATE_CH1_CFG,
				CS35L43_AUX_NGATE_CH1_EN_MASK,
				CS35L43_AUX_NGATE_CH1_EN_MASK);
		regmap_update_bits(cs35l43->regmap,
				CS35L43_MIXER_NGATE_CH2_CFG,
				CS35L43_AUX_NGATE_CH2_EN_MASK,
				CS35L43_AUX_NGATE_CH2_EN_MASK);

		if (cs35l43->pdata.dsp_ng_pcm_thld) {
			regmap_update_bits(cs35l43->regmap,
				CS35L43_MIXER_NGATE_CH1_CFG,
				CS35L43_AUX_NGATE_CH1_THR_MASK,
				cs35l43->pdata.dsp_ng_pcm_thld);
			regmap_update_bits(cs35l43->regmap,
				CS35L43_MIXER_NGATE_CH2_CFG,
				CS35L43_AUX_NGATE_CH2_THR_MASK,
				cs35l43->pdata.dsp_ng_pcm_thld);
		}

		if (cs35l43->pdata.dsp_ng_delay) {
			regmap_update_bits(cs35l43->regmap,
				CS35L43_MIXER_NGATE_CH1_CFG,
				CS35L43_AUX_NGATE_CH1_HOLD_MASK,
				cs35l43->pdata.dsp_ng_delay <<
				CS35L43_AUX_NGATE_CH1_HOLD_SHIFT);
			regmap_update_bits(cs35l43->regmap,
				CS35L43_MIXER_NGATE_CH2_CFG,
				CS35L43_AUX_NGATE_CH2_HOLD_MASK,
				cs35l43->pdata.dsp_ng_delay <<
				CS35L43_AUX_NGATE_CH2_HOLD_SHIFT);
		}
	}

	if (cs35l43->pdata.asp_sdout_hiz)
		regmap_update_bits(cs35l43->regmap,
				CS35L43_ASP_CONTROL3,
				CS35L41_ASP_DOUT_HIZ_CTRL_MASK,
				cs35l43->pdata.asp_sdout_hiz);

	if (cs35l43->pdata.hw_ng_sel)
		regmap_update_bits(cs35l43->regmap,
				CS35L43_NG_CONFIG,
				CS35L43_NG_EN_SEL_MASK,
				cs35l43->pdata.hw_ng_sel <<
				CS35L43_NG_EN_SEL_SHIFT);

	if (cs35l43->pdata.hw_ng_thld)
		regmap_update_bits(cs35l43->regmap,
				CS35L43_NG_CONFIG,
				CS35L43_NG_PCM_THLD_MASK,
				cs35l43->pdata.hw_ng_thld <<
				CS35L43_NG_PCM_THLD_SHIFT);

	if (cs35l43->pdata.hw_ng_delay)
		regmap_update_bits(cs35l43->regmap,
				CS35L43_NG_CONFIG,
				CS35L43_NG_DELAY_MASK,
				cs35l43->pdata.hw_ng_delay <<
				CS35L43_NG_DELAY_SHIFT);

	return 0;
}

static int cs35l43_handle_of_data(struct device *dev,
				  struct cs35l43_platform_data *pdata,
				  struct cs35l43_private *cs35l43)
{
	struct device_node *np = dev->of_node;
	int ret, val;

	if (!np)
		return 0;

	pdata->dsp_ng_enable = of_property_read_bool(np,
					"cirrus,dsp-noise-gate-enable");
	if (of_property_read_u32(np,
				"cirrus,dsp-noise-gate-threshold", &val) >= 0)
		pdata->dsp_ng_pcm_thld = val | CS35L43_VALID_PDATA;
	if (of_property_read_u32(np, "cirrus,dsp-noise-gate-delay", &val) >= 0)
		pdata->dsp_ng_delay = val | CS35L43_VALID_PDATA;

	if (of_property_read_u32(np, "cirrus,hw-noise-gate-select", &val) >= 0)
		pdata->hw_ng_sel = val | CS35L43_VALID_PDATA;
	if (of_property_read_u32(np,
				"cirrus,hw-noise-gate-threshold", &val) >= 0)
		pdata->hw_ng_thld = val | CS35L43_VALID_PDATA;
	if (of_property_read_u32(np, "cirrus,hw-noise-gate-delay", &val) >= 0)
		pdata->hw_ng_delay = val | CS35L43_VALID_PDATA;

	if (of_property_read_u32(np, "cirrus,gpio1-src-sel", &val) >= 0)
		pdata->gpio1_src_sel = val | CS35L43_VALID_PDATA;
	if (of_property_read_u32(np, "cirrus,gpio2-src-sel", &val) >= 0)
		pdata->gpio2_src_sel = val | CS35L43_VALID_PDATA;
	pdata->gpio1_out_enable = of_property_read_bool(np,
					"cirrus,gpio1-output-enable");
	pdata->gpio2_out_enable = of_property_read_bool(np,
					"cirrus,gpio2-output-enable");

	if (of_property_read_u32(np, "cirrus,asp-sdout-hiz", &val) >= 0)
		pdata->asp_sdout_hiz = val | CS35L43_VALID_PDATA;

	pdata->classh_disable = of_property_read_bool(np,
						"cirrus,classh-disable");
	ret = of_property_read_u32(np, "cirrus,boost-ctl-millivolt", &val);
	if (ret >= 0) {
		if (val < 2550 || val > 11000) {
			dev_info(dev,
				"Invalid Boost Voltage %u mV\n", val);
			return -EINVAL;
		}
		pdata->bst_vctrl = ((val - 2550) / 100) + 1;
	}

	return 0;
}


static int cs35l43_component_probe(struct snd_soc_component *component)
{
	int ret = 0;
	struct cs35l43_private *cs35l43 =
		snd_soc_component_get_drvdata(component);

	cs35l43_set_pdata(cs35l43);
	cs35l43->component = component;
	wm_adsp2_component_probe(&cs35l43->dsp, component);


	return ret;
}

static void cs35l43_component_remove(struct snd_soc_component *component)
{

}

static const struct wm_adsp_region cs35l43_dsp1_regions[] = {
	{ .type = WMFW_HALO_PM_PACKED,	.base = CS35L43_DSP1_PMEM_0 },
	{ .type = WMFW_HALO_XM_PACKED,	.base = CS35L43_DSP1_XMEM_PACKED_0 },
	{ .type = WMFW_HALO_YM_PACKED,	.base = CS35L43_DSP1_YMEM_PACKED_0 },
	{. type = WMFW_ADSP2_XM,	.base = CS35L43_DSP1_XMEM_UNPACKED24_0},
	{. type = WMFW_ADSP2_YM,	.base = CS35L43_DSP1_YMEM_UNPACKED24_0},
};

static int cs35l43_dsp_init(struct cs35l43_private *cs35l43)
{
	struct wm_adsp *dsp;
	int ret;

	dsp = &cs35l43->dsp;
	dsp->part = "cs35l43";
	dsp->num = 1;
	dsp->type = WMFW_HALO;
	dsp->rev = 0;
	dsp->fw = 9; /* 9 is WM_ADSP_FW_SPK_PROT in wm_adsp.c */
	dsp->dev = cs35l43->dev;
	dsp->regmap = cs35l43->regmap;
	dsp->base = CS35L43_DSP1_CLOCK_FREQ;
	dsp->base_sysinfo = CS35L43_DSP1_SYS_INFO_ID;
	dsp->mem = cs35l43_dsp1_regions;
	dsp->num_mems = ARRAY_SIZE(cs35l43_dsp1_regions);
	dsp->lock_regions = 0xFFFFFFFF;

	mutex_init(&cs35l43->rate_lock);
	ret = wm_halo_init(dsp, &cs35l43->rate_lock);

	if (ret != 0) {
		dev_info(cs35l43->dev, "wm_halo_init failed\n");
		goto err;
	}

	dsp->ops->stop_core(dsp);

	regmap_write(cs35l43->regmap, CS35L43_DSP1RX3_INPUT, 0x00);
	regmap_write(cs35l43->regmap, CS35L43_DSP1RX4_INPUT, CS35L43_INPUT_SRC_IMON);
	regmap_write(cs35l43->regmap, CS35L43_DSP1RX5_INPUT, CS35L43_INPUT_SRC_VMON);
	regmap_write(cs35l43->regmap, CS35L43_DSP1RX6_INPUT, CS35L43_INPUT_SRC_VPMON);

	return 0;

err:
	mutex_destroy(&cs35l43->rate_lock);
	return ret;
}

static const struct snd_soc_dai_ops cs35l43_ops = {
	.startup = cs35l43_pcm_startup,
	.set_fmt = cs35l43_set_dai_fmt,
	.hw_params = cs35l43_pcm_hw_params,
	.set_sysclk = cs35l43_dai_set_sysclk,
};

static struct snd_soc_dai_driver cs35l43_dai[] = {
	{
		.name = "cs35l43-pcm",
		.id = 0,
		.playback = {
			.stream_name = "AMP Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_KNOT,
			.formats = CS35L43_RX_FORMATS,
		},
		.capture = {
			.stream_name = "AMP Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_KNOT,
			.formats = CS35L43_TX_FORMATS,
		},
		.ops = &cs35l43_ops,
		.symmetric_rates = 1,
	},
};

static const struct snd_soc_component_driver soc_component_dev_cs35l43 = {
	.probe = cs35l43_component_probe,
	.remove = cs35l43_component_remove,

	.dapm_widgets = cs35l43_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(cs35l43_dapm_widgets),
	.dapm_routes = cs35l43_audio_map,
	.num_dapm_routes = ARRAY_SIZE(cs35l43_audio_map),

	.controls = cs35l43_aud_controls,
	.num_controls = ARRAY_SIZE(cs35l43_aud_controls),

	.set_sysclk = cs35l43_component_set_sysclk,

	.write = cs35l43_component_write,
	.read = cs35l43_component_read,
};

static struct reg_sequence cs35l43_errata_patch[] = {
	{0x7404,	0x11330000},
};

int cs35l43_probe(struct cs35l43_private *cs35l43,
				struct cs35l43_platform_data *pdata)
{
	int ret, i;
	unsigned int regid, revid;
	int irq_pol = IRQF_TRIGGER_HIGH;

	for (i = 0; i < ARRAY_SIZE(cs35l43_supplies); i++)
		cs35l43->supplies[i].supply = cs35l43_supplies[i];

	cs35l43->num_supplies = ARRAY_SIZE(cs35l43_supplies);

	ret = devm_regulator_bulk_get(cs35l43->dev, cs35l43->num_supplies,
					cs35l43->supplies);
	if (ret != 0) {
		dev_info(cs35l43->dev,
			"Failed to request core supplies: %d\n",
			ret);
		return ret;
	}

	if (pdata) {
		cs35l43->pdata = *pdata;
	} else if (cs35l43->dev->of_node) {
		ret = cs35l43_handle_of_data(cs35l43->dev, &cs35l43->pdata,
					     cs35l43);
		if (ret != 0)
			return ret;
	} else {
		ret = -EINVAL;
		goto err;
	}

	ret = regulator_bulk_enable(cs35l43->num_supplies, cs35l43->supplies);
	if (ret != 0) {
		dev_info(cs35l43->dev,
			"Failed to enable core supplies: %d\n", ret);
		return ret;
	}

	/* returning NULL can be an option if in stereo mode */
	cs35l43->reset_gpio = devm_gpiod_get_optional(cs35l43->dev, "reset",
							GPIOD_OUT_LOW);
	if (IS_ERR(cs35l43->reset_gpio)) {
		ret = PTR_ERR(cs35l43->reset_gpio);
		cs35l43->reset_gpio = NULL;
		if (ret == -EBUSY) {
			dev_info(cs35l43->dev,
				 "Reset line busy, assuming shared reset\n");
		} else {
			dev_info(cs35l43->dev,
				"Failed to get reset GPIO: %d\n", ret);
			goto err;
		}
	}
	if (cs35l43->reset_gpio) {
		/* satisfy minimum reset pulse width spec */
		usleep_range(2000, 2100);
		gpiod_set_value_cansleep(cs35l43->reset_gpio, 1);
	}

	usleep_range(2000, 2100);

	ret = regmap_read(cs35l43->regmap, CS35L43_DEVID, &regid);
	if (ret < 0) {
		dev_info(cs35l43->dev, "Get Device ID failed\n");
		goto err;
	}

	ret = regmap_read(cs35l43->regmap, CS35L43_REVID, &revid);
	if (ret < 0) {
		dev_info(cs35l43->dev, "Get Revision ID failed\n");
		goto err;
	}

	ret = regmap_register_patch(cs35l43->regmap,
			cs35l43_errata_patch,
			ARRAY_SIZE(cs35l43_errata_patch));
	if (ret < 0) {
		dev_info(cs35l43->dev, "Failed to apply errata patch %d\n", ret);
		goto err;
	}

	irq_pol = cs35l43_irq_gpio_config(cs35l43);
	regmap_write(cs35l43->regmap, CS35L43_IRQ1_MASK_1, 0xFFFFFFFF);
	regmap_update_bits(cs35l43->regmap, CS35L43_IRQ1_MASK_1,
				CS35L43_AMP_ERR_EINT1_MASK |
				CS35L43_BST_SHORT_ERR_EINT1_MASK |
				CS35L43_BST_DCM_UVP_ERR_EINT1_MASK |
				CS35L43_BST_OVP_ERR_EINT1_MASK |
				CS35L43_DSP_VIRTUAL2_MBOX_WR_EINT1_MASK |
				CS35L43_DC_WATCHDOG_IRQ_RISE_EINT1_MASK |
				CS35L43_WKSRC_STATUS6_EINT1_MASK |
				CS35L43_WKSRC_STATUS_ANY_EINT1_MASK, 0);

	regmap_update_bits(cs35l43->regmap, CS35L43_ALIVE_DCIN_WD,
				CS35L43_DCIN_WD_EN_MASK,
				CS35L43_DCIN_WD_EN_MASK);
	regmap_update_bits(cs35l43->regmap, CS35L43_ALIVE_DCIN_WD,
				CS35L43_DCIN_WD_THLD_MASK,
				1 << CS35L43_DCIN_WD_THLD_SHIFT);

	ret = devm_request_threaded_irq(cs35l43->dev, cs35l43->irq, NULL,
				cs35l43_irq, IRQF_ONESHOT | IRQF_SHARED |
				irq_pol, "cs35l43", cs35l43);

	cs35l43->hibernate_state = CS35L43_HIBERNATE_NOT_LOADED;
	INIT_DELAYED_WORK(&cs35l43->hb_work, cs35l43_hibernate_work);
	mutex_init(&cs35l43->hb_lock);

	cs35l43->wq = create_singlethread_workqueue("cs35l43");
	if (cs35l43->wq == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	cs35l43_dsp_init(cs35l43);

	ret = snd_soc_register_component(cs35l43->dev,
					&soc_component_dev_cs35l43,
					cs35l43_dai, ARRAY_SIZE(cs35l43_dai));
	if (ret < 0) {
		dev_info(cs35l43->dev, "%s: Register codec failed\n", __func__);
		goto err;
	}

	dev_info(cs35l43->dev, "Cirrus Logic cs35l43 (%x), Revision: %02X\n",
			regid, revid);

err:
	regulator_bulk_disable(cs35l43->num_supplies, cs35l43->supplies);
	return ret;
}

int cs35l43_remove(struct cs35l43_private *cs35l43)
{
	regulator_bulk_disable(cs35l43->num_supplies, cs35l43->supplies);
	snd_soc_unregister_component(cs35l43->dev);
	return 0;
}

MODULE_DESCRIPTION("ASoC CS35L43 driver");
MODULE_AUTHOR("David Rhodes, Cirrus Logic Inc, <david.rhodes@cirrus.com>");
MODULE_LICENSE("GPL");
