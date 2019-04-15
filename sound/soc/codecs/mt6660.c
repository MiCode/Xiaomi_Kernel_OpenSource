// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/pcm_params.h>

#include "mt6660.h"

union mt6660_multi_byte_data {
	u32 data_u32;
	u16 data_u16;
	u8 data_u8;
	u8 data[4];
};

struct codec_reg_val {
	u32 addr;
	u32 mask;
	u32 data;
};

struct reg_size_table {
	u32 addr;
	u8 size;
};

static const struct reg_size_table mt6660_reg_size_table[] = {
	{ MT6660_REG_HPF1_COEF, 4 },
	{ MT6660_REG_HPF2_COEF, 4 },
	{ MT6660_REG_TDM_CFG3, 2 },
	{ MT6660_REG_RESV17, 2 },
	{ MT6660_REG_RESV23, 2 },
	{ MT6660_REG_SIGMAX, 2 },
	{ MT6660_REG_DEVID, 2},
	{ MT6660_REG_TDM_CFG3, 2},
	{ MT6660_REG_HCLIP_CTRL, 2},
	{ MT6660_REG_DA_GAIN, 2},
};

static int mt6660_get_reg_size(uint32_t addr)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(mt6660_reg_size_table); i++) {
		if (mt6660_reg_size_table[i].addr == addr)
			return mt6660_reg_size_table[i].size;
	}
	return 1;
}

static int32_t mt6660_i2c_update_bits(struct mt6660_chip *chip,
	uint32_t addr, uint32_t mask, uint32_t data)
{
	int ret = 0;
	uint32_t value;
	int size = mt6660_get_reg_size(addr);
	union mt6660_multi_byte_data mdata;

	memcpy(&mdata, &data, sizeof(uint32_t));

	mutex_lock(&chip->io_lock);
	ret = i2c_smbus_read_i2c_block_data(
		chip->i2c, addr, size, (u8 *)&value);
	if (ret < 0) {
		mutex_unlock(&chip->io_lock);
		return ret;
	}
	switch (size) {
	case 1:
		value &= ~mask;
		value |= (mdata.data_u8 & mask);
		break;
	case 2:
		value = be16_to_cpu(value);
		value &= ~mask;
		value |= (mdata.data_u16 & mask);
		value = be16_to_cpu(value);
		break;
	case 4:
		value = be32_to_cpu(value);
		value &= ~mask;
		value |= (mdata.data_u32 & mask);
		value = be32_to_cpu(value);
		break;
	default:
		dev_err(chip->dev, "%s Invalid bytes\n", __func__);
		break;
	}

	ret = i2c_smbus_write_i2c_block_data(
		chip->i2c, addr, size, (u8 *)&value);
	if (ret < 0) {
		mutex_unlock(&chip->io_lock);
		return ret;
	}
	mutex_unlock(&chip->io_lock);

	return 0;
}

static unsigned int mt6660_i2c_read(struct mt6660_chip *chip, unsigned int reg)
{
	int size = mt6660_get_reg_size(reg);
	int i = 0, ret = 0;
	u8 data[4] = {0};
	u32 reg_data = 0;

	ret = i2c_smbus_read_i2c_block_data(chip->i2c, reg, size, data);
	if (ret < 0)
		return ret;
	for (i = 0; i < size; i++) {
		reg_data <<= 8;
		reg_data |= data[i];
	}
	return reg_data;
}

static unsigned int mt6660_component_io_read(
	struct snd_soc_component *component, unsigned int reg)
{
	struct mt6660_chip *chip = snd_soc_component_get_drvdata(component);

	return mt6660_i2c_read(chip, reg);
}

static int mt6660_component_io_write(struct snd_soc_component *component,
	unsigned int reg, unsigned int data)
{
	struct mt6660_chip *chip = snd_soc_component_get_drvdata(component);
	int size = mt6660_get_reg_size(reg);
	u8 reg_data[4] = {0};
	int i = 0;

	for (i = 0; i < size; i++)
		reg_data[size - i - 1] = (data >> (8 * i)) & 0xff;

	return i2c_smbus_write_i2c_block_data(chip->i2c, reg, size, reg_data);
}

static const int mt6660_dump_table[] = {
	MT6660_REG_DEVID,
	MT6660_REG_SYSTEM_CTRL,
	MT6660_REG_IRQ_STATUS1,
	MT6660_REG_SERIAL_CFG1,
	MT6660_REG_DATAO_SEL,
	MT6660_REG_TDM_CFG3,
	MT6660_REG_HPF_CTRL,
	MT6660_REG_HPF1_COEF,
	MT6660_REG_HPF2_COEF,
	MT6660_REG_PATH_BYPASS,
	MT6660_REG_WDT_CTRL,
	MT6660_REG_HCLIP_CTRL,
	MT6660_REG_VOL_CTRL,
	MT6660_REG_SPS_CTRL,
	MT6660_REG_SIGMAX,
	MT6660_REG_CALI_T0,
	MT6660_REG_BST_CTRL,
	MT6660_REG_PROTECTION_CFG,
	MT6660_REG_DA_GAIN,
	MT6660_REG_AUDIO_IN2_SEL,
	MT6660_REG_SIG_GAIN,
	MT6660_REG_PLL_CFG1,
	MT6660_REG_DRE_CTRL,
	MT6660_REG_DRE_CORASE,
	MT6660_REG_DRE_THDMODE,
	MT6660_REG_PWM_CTRL,
	MT6660_REG_DC_PROTECT_CTRL,
	MT6660_REG_ADC_USB_MODE,
	MT6660_REG_INTERNAL_CFG,
	MT6660_REG_RESV0,
	MT6660_REG_RESV1,
	MT6660_REG_RESV2,
	MT6660_REG_RESV3,
	MT6660_REG_RESV7,
	MT6660_REG_RESV10,
	MT6660_REG_RESV11,
	MT6660_REG_RESV16,
	MT6660_REG_RESV17,
	MT6660_REG_RESV19,
	MT6660_REG_RESV21,
	MT6660_REG_RESV23,
	MT6660_REG_RESV31,
	MT6660_REG_RESV40,
};

static ssize_t mt6660_dumps_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mt6660_chip *chip = dev_get_drvdata(dev);
	int i = 0;
	int max_size = 512;
	int ret;

	for (i = 0; i < ARRAY_SIZE(mt6660_dump_table); i++) {
		ret = mt6660_i2c_read(chip, mt6660_dump_table[i]);
		snprintf(buf+strnlen(buf, max_size), max_size,
			"reg 0x%02x : 0x%x\n", mt6660_dump_table[i], ret);
	}
	return strnlen(buf, max_size);
}

DEVICE_ATTR_RO(mt6660_dumps);

static const struct codec_reg_val e1_reg_inits[] = {
	{ MT6660_REG_WDT_CTRL, 0x80, 0x00 },
	{ MT6660_REG_SPS_CTRL, 0x01, 0x00 },
	{ MT6660_REG_HPF1_COEF, 0xffffffff, 0x7fdb7ffe },
	{ MT6660_REG_HPF2_COEF, 0xffffffff, 0x7fdb7ffe },
	{ MT6660_REG_SIG_GAIN, 0xff, 0x7b },
	{ MT6660_REG_PWM_CTRL, 0x08, 0x00 },
	{ MT6660_REG_TDM_CFG3, 0x400, 0x400 },
	{ MT6660_REG_AUDIO_IN2_SEL, 0x1c, 0x04 },
	{ MT6660_REG_RESV1, 0xc0, 0x00 },
	{ MT6660_REG_RESV2, 0xe0, 0x20 },
	{ MT6660_REG_RESV3, 0xc0, 0x80 },
	{ MT6660_REG_RESV11, 0x0c, 0x00 },
	{ MT6660_REG_RESV17, 0x7777, 0x7272 },
	{ MT6660_REG_RESV19, 0x08, 0x08 },
	{ MT6660_REG_RESV21, 0x8f, 0x0f },
	{ MT6660_REG_RESV31, 0x03, 0x03 },
	{ MT6660_REG_RESV40, 0x01, 0x00 },
};

static const struct codec_reg_val e2_reg_inits[] = {
	{ MT6660_REG_WDT_CTRL, 0x80, 0x00 },
	{ MT6660_REG_SPS_CTRL, 0x01, 0x01 },
	{ MT6660_REG_AUDIO_IN2_SEL, 0x1c, 0x04 },
	{ MT6660_REG_RESV11, 0x0c, 0x00 },
	{ MT6660_REG_RESV31, 0x03, 0x03 },
	{ MT6660_REG_RESV40, 0x01, 0x00 },
	{ MT6660_REG_RESV0, 0x44, 0x04 },
	{ MT6660_REG_RESV17, 0x7777, 0x7273 },
	{ MT6660_REG_RESV16, 0x07, 0x03 },
	{ MT6660_REG_DRE_CORASE, 0xe0, 0x20 },
	{ MT6660_REG_ADDA_CLOCK, 0xff, 0x70 },
	{ MT6660_REG_RESV21, 0xff, 0x20 },
	{ MT6660_REG_DRE_THDMODE, 0xff, 0xa2 },
	{ MT6660_REG_RESV23, 0xffff, 0x17f8 },
	{ MT6660_REG_PWM_CTRL, 0xff, 0x04 },
	{ MT6660_REG_INTERNAL_CFG, 0xff, 0x42 },
	{ MT6660_REG_ADC_USB_MODE, 0xff, 0x00 },
	{ MT6660_REG_PROTECTION_CFG, 0xff, 0x1d },
	{ MT6660_REG_HPF1_COEF, 0xffffffff, 0x7fdb7ffe },
	{ MT6660_REG_HPF2_COEF, 0xffffffff, 0x7fdb7ffe },
	{ MT6660_REG_SIGMAX, 0xffff, 0x7fff },
	{ MT6660_REG_DA_GAIN, 0xffff, 0x0116 },
	{ MT6660_REG_SIG_GAIN, 0xff, 0x58 },
	{ MT6660_REG_RESV6, 0xff, 0xce },
};

static const struct codec_reg_val e3_reg_inits[] = {
	{ MT6660_REG_WDT_CTRL, 0x80, 0x00 },
	{ MT6660_REG_SPS_CTRL, 0x01, 0x01 },
	{ MT6660_REG_AUDIO_IN2_SEL, 0x1c, 0x04 },
	{ MT6660_REG_RESV11, 0x0c, 0x00 },
	{ MT6660_REG_RESV31, 0x03, 0x03 },
	{ MT6660_REG_RESV40, 0x01, 0x00 },
	{ MT6660_REG_RESV0, 0x44, 0x04 },
	{ MT6660_REG_RESV17, 0x7777, 0x7273 },
	{ MT6660_REG_RESV16, 0x07, 0x03 },
	{ MT6660_REG_DRE_CORASE, 0xe0, 0x20 },
	{ MT6660_REG_ADDA_CLOCK, 0xff, 0x70 },
	{ MT6660_REG_RESV21, 0xff, 0x20 },
	{ MT6660_REG_DRE_THDMODE, 0xff, 0xa2 },
	{ MT6660_REG_RESV23, 0xffff, 0x17f8 },
	{ MT6660_REG_PWM_CTRL, 0xff, 0x04 },
	{ MT6660_REG_INTERNAL_CFG, 0xff, 0x42 },
	{ MT6660_REG_ADC_USB_MODE, 0xff, 0x00 },
	{ MT6660_REG_PROTECTION_CFG, 0xff, 0x1d },
	{ MT6660_REG_HPF1_COEF, 0xffffffff, 0x7fdb7ffe },
	{ MT6660_REG_HPF2_COEF, 0xffffffff, 0x7fdb7ffe },
	{ MT6660_REG_SIG_GAIN, 0xff, 0x58 },
	{ MT6660_REG_RESV6, 0xff, 0xce },
};

static const struct codec_reg_val e4_reg_inits[] = {
	{ MT6660_REG_WDT_CTRL, 0x80, 0x00 },
	{ MT6660_REG_SPS_CTRL, 0x01, 0x01 },
	{ MT6660_REG_AUDIO_IN2_SEL, 0x1c, 0x04 },
	{ MT6660_REG_RESV11, 0x0c, 0x00 },
	{ MT6660_REG_RESV31, 0x03, 0x03 },
	{ MT6660_REG_RESV40, 0x01, 0x00 },
	{ MT6660_REG_RESV0, 0x44, 0x04 },
	{ MT6660_REG_RESV19, 0xff, 0x82 },
	{ MT6660_REG_RESV17, 0x7777, 0x7273 },
	{ MT6660_REG_RESV16, 0x07, 0x03 },
	{ MT6660_REG_DRE_CORASE, 0xe0, 0x20 },
	{ MT6660_REG_ADDA_CLOCK, 0xff, 0x70 },
	{ MT6660_REG_RESV21, 0xff, 0x20 },
	{ MT6660_REG_DRE_THDMODE, 0xff, 0xa2 },
	{ MT6660_REG_RESV23, 0xffff, 0x17f8 },
	{ MT6660_REG_PWM_CTRL, 0xff, 0x15 },
	{ MT6660_REG_ADC_USB_MODE, 0xff, 0x00 },
	{ MT6660_REG_PROTECTION_CFG, 0xff, 0x1d },
	{ MT6660_REG_HPF1_COEF, 0xffffffff, 0x7fdb7ffe },
	{ MT6660_REG_HPF2_COEF, 0xffffffff, 0x7fdb7ffe },
	{ MT6660_REG_SIG_GAIN, 0xff, 0x58 },
	{ MT6660_REG_RESV6, 0xff, 0xce },
};

static int mt6660_i2c_init_setting(struct mt6660_chip *chip)
{
	int i, len, ret;
	const struct codec_reg_val *init_table;

	if (chip->chip_rev >= 0x01e2) {
		init_table = e4_reg_inits;
		len = ARRAY_SIZE(e4_reg_inits);
	} else if (chip->chip_rev >= 0x00e2) {
		init_table = e3_reg_inits;
		len = ARRAY_SIZE(e3_reg_inits);
	} else if (chip->chip_rev >= 0x00e1) {
		init_table = e2_reg_inits;
		len = ARRAY_SIZE(e2_reg_inits);
	} else {
		init_table = e1_reg_inits;
		len = ARRAY_SIZE(e1_reg_inits);
	}
	for (i = 0; i < len; i++) {
		ret = mt6660_i2c_update_bits(chip, init_table[i].addr,
				init_table[i].mask, init_table[i].data);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int mt6660_chip_power_on(
	struct snd_soc_component *component, int on_off)
{
	struct mt6660_chip *chip = (struct mt6660_chip *)
		snd_soc_component_get_drvdata(component);
	int ret = 0;
	unsigned int val;

	dev_dbg(component->dev, "%s: on_off = %d\n", __func__, on_off);
	mutex_lock(&chip->var_lock);
	if (on_off) {
		if (chip->pwr_cnt == 0) {
			ret = mt6660_i2c_update_bits(chip,
				MT6660_REG_SYSTEM_CTRL, 0x01, 0x00);
			val = mt6660_i2c_read(chip, MT6660_REG_IRQ_STATUS1);
			dev_info(chip->dev,
				"%s reg0x05 = 0x%x\n", __func__, val);
		}
		chip->pwr_cnt++;
	} else {
		chip->pwr_cnt--;
		if (chip->pwr_cnt == 0) {
			ret = mt6660_i2c_update_bits(chip,
				MT6660_REG_SYSTEM_CTRL, 0x01, 0xff);
		}
		if (chip->pwr_cnt < 0) {
			dev_warn(chip->dev, "not paired on/off\n");
			chip->pwr_cnt = 0;
		}
	}
	mutex_unlock(&chip->var_lock);
	if (ret < 0)
		pr_err("%s ret = %d\n", __func__, ret);
	return ret;
}

static int mt6660_component_set_bias_level(struct snd_soc_component *component,
	enum snd_soc_bias_level level)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(component);
	int ret;
	unsigned int val;
	struct mt6660_chip *chip = snd_soc_component_get_drvdata(component);

	if (dapm->bias_level == level) {
		dev_warn(component->dev, "%s: repeat level change\n", __func__);
		return 0;
	}
	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		if (dapm->bias_level != SND_SOC_BIAS_OFF)
			break;
		dev_dbg(component->dev, "exit low power mode\n");
		ret = mt6660_chip_power_on(component, 1);
		if (ret < 0) {
			dev_err(component->dev, "power on fail\n");
			return ret;
		}
		break;
	case SND_SOC_BIAS_OFF:
		dev_dbg(component->dev, "enter low power mode\n");
		val = mt6660_i2c_read(chip, MT6660_REG_IRQ_STATUS1);
		dev_info(component->dev,
			"%s reg0x05 = 0x%x\n", __func__, val);
		ret = mt6660_chip_power_on(component, 0);
		if (ret < 0) {
			dev_err(component->dev, "power off fail\n");
			return ret;
		}
		break;
	default:
		return -EINVAL;
	}
	dapm->bias_level = level;
	dev_dbg(component->dev, "c bias_level = %d\n", level);
	return 0;
}

static int mt6660_component_probe(struct snd_soc_component *component)
{
	struct mt6660_chip *chip = snd_soc_component_get_drvdata(component);
	int ret = 0;

	ret = mt6660_component_set_bias_level(component, SND_SOC_BIAS_STANDBY);
	if (ret < 0) {
		dev_err(component->dev, "config bias standby fail\n");
		return ret;
	}

	ret = mt6660_component_set_bias_level(component, SND_SOC_BIAS_OFF);
	if (ret < 0) {
		dev_err(component->dev, "config bias off fail\n");
		return ret;
	}
	chip->component = component;
	return 0;
}

static void mt6660_component_remove(struct snd_soc_component *component)
{
	struct mt6660_chip *chip = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s++\n", __func__);
	chip->component = NULL;
	dev_dbg(component->dev, "%s--\n", __func__);
}

static int mt6660_codec_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(1000, 1100);
		break;
	}
	return 0;
}

static int mt6660_codec_classd_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
		snd_soc_dapm_to_component(w->dapm);
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(component->dev,
			"%s: before classd turn on\n", __func__);
		/* config to adaptive mode */
		ret = snd_soc_component_update_bits(component,
			MT6660_REG_BST_CTRL, 0x03, 0x03);
		if (ret < 0) {
			dev_err(component->dev, "config mode adaptive fail\n");
			return ret;
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* voltage sensing enable */
		ret = snd_soc_component_update_bits(component,
			MT6660_REG_RESV7, 0x04, 0x04);
		if (ret < 0) {
			dev_err(component->dev,
				"enable voltage sensing fail\n");
			return ret;
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* voltage sensing disable */
		ret = snd_soc_component_update_bits(component,
			MT6660_REG_RESV7, 0x04, 0x00);
		if (ret < 0) {
			dev_err(component->dev,
				"disable voltage sensing fail\n");
			return ret;
		}
		/* pop-noise improvement 1 */
		ret = snd_soc_component_update_bits(component,
			MT6660_REG_RESV10, 0x10, 0x10);
		if (ret < 0) {
			dev_err(component->dev,
				"pop-noise improvement 1 fail\n");
			return ret;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(component->dev,
			"%s: after classd turn off\n", __func__);
		/* pop-noise improvement 2 */
		ret = snd_soc_component_update_bits(component,
			MT6660_REG_RESV10, 0x10, 0x00);
		if (ret < 0) {
			dev_err(component->dev,
				"pop-noise improvement 2 fail\n");
			return ret;
		}
		/* config to off mode */
		ret = snd_soc_component_update_bits(component,
			MT6660_REG_BST_CTRL, 0x03, 0x00);
		if (ret < 0) {
			dev_err(component->dev, "config mode off fail\n");
			return ret;
		}
		break;
	}
	return 0;
}

static const struct snd_soc_dapm_widget mt6660_component_dapm_widgets[] = {
	SND_SOC_DAPM_DAC_E("DAC", NULL, MT6660_REG_PLL_CFG1,
		0, 1, mt6660_codec_dac_event, SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_ADC("VI ADC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA("PGA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV_E("ClassD", MT6660_REG_SYSTEM_CTRL, 2, 0,
			       NULL, 0, mt6660_codec_classd_event,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			       SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SPK("SPK", NULL),
};

static const struct snd_soc_dapm_route mt6660_component_dapm_routes[] = {
	{ "DAC", NULL, "aif_playback"},
	{ "PGA", NULL, "DAC"},
	{ "ClassD", NULL, "PGA"},
	{ "SPK", NULL, "ClassD"},
	{ "VI ADC", NULL, "ClassD"},
	{ "aif_capture", NULL, "VI ADC"},
};

static int mt6660_component_put_volsw(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	int ret, put_ret = 0;

	ret = mt6660_chip_power_on(component, 1);
	if (ret < 0)
		dev_err(component->dev, "%s: pwr on fail\n", __func__);
	put_ret = snd_soc_put_volsw(kcontrol, ucontrol);
	if (ret < 0)
		return ret;
	ret = mt6660_chip_power_on(component, 0);
	if (ret < 0)
		dev_err(component->dev, "%s: pwr off fail\n", __func__);
	return put_ret;
}

static int mt6660_component_get_volsw(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
		snd_soc_kcontrol_component(kcontrol);
	struct mt6660_chip *chip = (struct mt6660_chip *)
		snd_soc_component_get_drvdata(component);
	int ret = -EINVAL;

	if (!strcmp(kcontrol->id.name, "Chip_Rev")) {
		ucontrol->value.integer.value[0] = chip->chip_rev & 0x0f;
		ret = 0;
	}
	return ret;
}

static const DECLARE_TLV_DB_SCALE(vol_ctl_tlv, -1155, 5, 0);

static const struct snd_kcontrol_new mt6660_component_snd_controls[] = {
	SOC_SINGLE_EXT_TLV("Volume_Ctrl", MT6660_REG_VOL_CTRL, 0, 255,
			   1, snd_soc_get_volsw, mt6660_component_put_volsw,
			   vol_ctl_tlv),
	SOC_SINGLE_EXT("WDT_Enable", MT6660_REG_WDT_CTRL, 7, 1, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("Hard_Clip_Enable", MT6660_REG_HCLIP_CTRL, 8, 1, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("Clip_Enable", MT6660_REG_SPS_CTRL, 0, 1, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("BoostMode", MT6660_REG_BST_CTRL, 0, 3, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("DRE_Enable", MT6660_REG_DRE_CTRL, 0, 1, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("DC_Protect_Enable",
		MT6660_REG_DC_PROTECT_CTRL, 3, 1, 0,
		snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("I2SLRS", MT6660_REG_DATAO_SEL, 6, 3, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("I2SDOLS", MT6660_REG_DATAO_SEL, 3, 7, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("I2SDORS", MT6660_REG_DATAO_SEL, 0, 7, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	/* for debug purpose */
	SOC_SINGLE_EXT("HPF_AUD_IN_EN", MT6660_REG_HPF_CTRL, 0, 1, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("AUD_LOOP_BACK", MT6660_REG_PATH_BYPASS, 4, 1, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("Mute_Enable", MT6660_REG_SYSTEM_CTRL, 1, 1, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("CS_Comp_Disable", MT6660_REG_PATH_BYPASS, 2, 1, 0,
		       snd_soc_get_volsw, mt6660_component_put_volsw),
	SOC_SINGLE_EXT("T0_SEL", MT6660_REG_CALI_T0, 0, 7, 0,
		       snd_soc_get_volsw, NULL),
	SOC_SINGLE_EXT("Chip_Rev", SND_SOC_NOPM, 0, 16, 0,
		       mt6660_component_get_volsw, NULL),
};


static const struct snd_soc_component_driver mt6660_component_driver = {
	.probe = mt6660_component_probe,
	.remove = mt6660_component_remove,

	.read = mt6660_component_io_read,
	.write = mt6660_component_io_write,

	.controls = mt6660_component_snd_controls,
	.num_controls = ARRAY_SIZE(mt6660_component_snd_controls),
	.dapm_widgets = mt6660_component_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt6660_component_dapm_widgets),
	.dapm_routes = mt6660_component_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(mt6660_component_dapm_routes),

	.set_bias_level = mt6660_component_set_bias_level,
	.idle_bias_on = false, /* idle_bias_off = true */
};

static int mt6660_component_aif_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(dai->component);
	int ret = 0;

	dev_dbg(dai->dev, "%s\n", __func__);
	if (dapm->bias_level == SND_SOC_BIAS_OFF)
		ret = mt6660_component_set_bias_level(dai->component,
						  SND_SOC_BIAS_STANDBY);
	return ret;
}

static int mt6660_component_aif_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *hw_params, struct snd_soc_dai *dai)
{
	int word_len = params_physical_width(hw_params);
	int aud_bit = params_width(hw_params);
	u16 reg_data = 0;
	int ret = 0;

	dev_dbg(dai->dev, "%s: ++\n", __func__);
	dev_dbg(dai->dev, "format: 0x%08x\n", params_format(hw_params));
	dev_dbg(dai->dev, "rate: 0x%08x\n", params_rate(hw_params));
	dev_dbg(dai->dev, "word_len: %d, aud_bit: %d\n", word_len, aud_bit);
	if (word_len > 32 || word_len < 16) {
		dev_err(dai->dev, "not supported word length\n");
		return -ENOTSUPP;
	}
	switch (aud_bit) {
	case 16:
		reg_data = 3;
		break;
	case 18:
		reg_data = 2;
		break;
	case 20:
		reg_data = 1;
		break;
	case 24:
	case 32:
		reg_data = 0;
		break;
	default:
		return -ENOTSUPP;
	}
	ret = snd_soc_component_update_bits(dai->component,
		MT6660_REG_SERIAL_CFG1, 0xc0, (reg_data << 6));
	if (ret < 0) {
		dev_err(dai->dev, "config aud bit fail\n");
		return ret;
	}
	ret = snd_soc_component_update_bits(dai->component,
		MT6660_REG_TDM_CFG3, 0x3f0, word_len << 4);
	if (ret < 0) {
		dev_err(dai->dev, "config word len fail\n");
		return ret;
	}
	dev_dbg(dai->dev, "%s: --\n", __func__);
	return 0;
}

static const struct snd_soc_dai_ops mt6660_component_aif_ops = {
	.startup = mt6660_component_aif_startup,
	.hw_params = mt6660_component_aif_hw_params,
};

#define STUB_RATES	SNDRV_PCM_RATE_8000_192000
#define STUB_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_U16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_U24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE | \
			SNDRV_PCM_FMTBIT_U32_LE)

static struct snd_soc_dai_driver mt6660_codec_dai = {
	.name = "mt6660-aif",
	.playback = {
		.stream_name	= "aif_playback",
		.channels_min	= 1,
		.channels_max	= 2,
		.rates		= STUB_RATES,
		.formats	= STUB_FORMATS,
	},
	.capture = {
		.stream_name	= "aif_capture",
		.channels_min	= 1,
		.channels_max	= 2,
		.rates = STUB_RATES,
		.formats = STUB_FORMATS,
	},
	/* dai properties */
	.symmetric_rates = 1,
	.symmetric_channels = 1,
	.symmetric_samplebits = 1,
	/* dai operations */
	.ops = &mt6660_component_aif_ops,
};

static inline int mt6660_chip_id_check(struct i2c_client *i2c)
{
	u8 id[2] = {0};
	int ret = 0;

	i2c_smbus_write_byte_data(i2c, 0x03, 0x00);
	ret = i2c_smbus_read_i2c_block_data(i2c, MT6660_REG_DEVID, 2, id);
	if (ret < 0)
		return ret;
	ret = (id[0] << 8) + id[1];
	ret &= 0x0ff0;
	if (ret != 0x00e0 && ret != 0x01e0)
		return -ENODEV;
	i2c_smbus_write_byte_data(i2c, 0x03, 0x01);
	return 0;
}

static inline int _mt6660_chip_sw_reset(struct mt6660_chip *chip)
{
	i2c_smbus_write_byte_data(chip->i2c, MT6660_REG_SYSTEM_CTRL, 0x80);
	msleep(30);
	return 0;
}

static inline int _mt6660_chip_power_on(struct mt6660_chip *chip, int on_off)
{
	u8 reg_data = 0;
	int ret = 0;

	ret = i2c_smbus_read_byte_data(chip->i2c, MT6660_REG_SYSTEM_CTRL);
	if (ret < 0)
		return ret;
	reg_data = (u8)ret;
	if (on_off)
		reg_data &= (~0x01);
	else
		reg_data |= 0x01;
	return i2c_smbus_write_byte_data(
		chip->i2c, MT6660_REG_SYSTEM_CTRL, reg_data);
}

static inline int _mt6660_read_chip_revision(struct mt6660_chip *chip)
{
	u8 reg_data[2] = {0};
	int ret = 0;

	ret = i2c_smbus_read_i2c_block_data(
		chip->i2c, MT6660_REG_DEVID, 2, reg_data);
	if (ret < 0) {
		dev_err(chip->dev, "get chip revision fail\n");
		return ret;
	}
	chip->chip_rev = reg_data[1];
	return 0;
}

int mt6660_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct mt6660_chip *chip = NULL;
	int ret = 0;

	ret = mt6660_chip_id_check(client);
	if (ret < 0) {
		dev_err(&client->dev, "chip id check fail\n");
		return ret;
	}
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->i2c = client;
	chip->dev = &client->dev;
	mutex_init(&chip->var_lock);
	mutex_init(&chip->io_lock);
	i2c_set_clientdata(client, chip);

	/* chip power on */
	ret = _mt6660_chip_power_on(chip, 1);
	if (ret < 0) {
		dev_err(chip->dev, "chip power on 1 fail\n");
		goto probe_fail;
	}
	/* chip reset first */
	ret = _mt6660_chip_sw_reset(chip);
	if (ret < 0) {
		dev_err(chip->dev, "chip reset fail\n");
		goto probe_fail;
	}
	/* chip power on */
	ret = _mt6660_chip_power_on(chip, 1);
	if (ret < 0) {
		dev_err(chip->dev, "chip power on 2 fail\n");
		goto probe_fail;
	}
	ret = _mt6660_read_chip_revision(chip);
	if (ret < 0) {
		dev_err(chip->dev, "read chip revision fail\n");
		goto probe_fail;
	}
	ret = mt6660_i2c_init_setting(chip);
	if (ret < 0) {
		dev_err(chip->dev, "chip i2c init setting fail\n");
		goto probe_fail;
	}
	ret = device_create_file(chip->dev, &dev_attr_mt6660_dumps);
	if (ret < 0) {
		dev_err(chip->dev, "chip dumps attr create fail\n");
		goto probe_fail;
	}
	ret = _mt6660_chip_power_on(chip, 0);
	if (ret < 0) {
		dev_err(chip->dev, "chip power off fail\n");
		goto probe_fail;
	}
	return snd_soc_register_component(chip->dev,
		&mt6660_component_driver, &mt6660_codec_dai, 1);
probe_fail:
	mutex_destroy(&chip->var_lock);
	return ret;
}

int mt6660_i2c_remove(struct i2c_client *client)
{
	struct mt6660_chip *chip = i2c_get_clientdata(client);

	dev_dbg(chip->dev, "%s++\n", __func__);
	snd_soc_unregister_component(chip->dev);
	mutex_destroy(&chip->var_lock);
	dev_dbg(chip->dev, "%s--\n", __func__);
	return 0;
}

static const struct of_device_id __maybe_unused mt6660_of_id[] = {
	{ .compatible = "mediatek,mt6660",},
	{},
};
MODULE_DEVICE_TABLE(of, mt6660_of_id);

static const struct i2c_device_id mt6660_i2c_id[] = {
	{"mt6660", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, mt6660_i2c_id);

static struct i2c_driver mt6660_i2c_driver = {
	.driver = {
		.name = "mt6660",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mt6660_of_id),
	},
	.probe = mt6660_i2c_probe,
	.remove = mt6660_i2c_remove,
	.id_table = mt6660_i2c_id,
};
module_i2c_driver(mt6660_i2c_driver);

MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_DESCRIPTION("MT6660 SPKAMP Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.5_G");
