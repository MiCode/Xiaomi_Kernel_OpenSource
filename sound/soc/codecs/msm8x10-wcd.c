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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/pdata.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include "msm8x10-wcd.h"
#include "wcd9xxx-resmgr.h"
#include "msm8x10_wcd_registers.h"

#define MSM8X10_WCD_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000)

#define NUM_DECIMATORS		2
#define NUM_INTERPOLATORS	3
#define BITS_PER_REG		8
#define MSM8X10_WCD_TX_PORT_NUMBER	4

#define MSM8X10_WCD_I2S_MASTER_MODE_MASK	0x08
#define MSM8X10_DINO_CODEC_BASE_ADDR		0xFE043000

#define MAX_MSM8X10_WCD_DEVICE	2
#define CODEC_DT_MAX_PROP_SIZE	40
#define MSM8X10_WCD_I2C_GSBI_SLAVE_ID "2-000d"

enum {
	MSM8X10_WCD_I2C_TOP_LEVEL = 0,
	MSM8X10_WCD_I2C_ANALOG,
	MSM8X10_WCD_I2C_DIGITAL_1,
	MSM8X10_WCD_I2C_DIGITAL_2,
};

enum {
	AIF1_PB = 0,
	AIF1_CAP,
	NUM_CODEC_DAIS,
};

enum {
	RX_MIX1_INP_SEL_ZERO = 0,
	RX_MIX1_INP_SEL_IIR1,
	RX_MIX1_INP_SEL_IIR2,
	RX_MIX1_INP_SEL_RX1,
	RX_MIX1_INP_SEL_RX2,
	RX_MIX1_INP_SEL_RX3,
};

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);
static const DECLARE_TLV_DB_SCALE(line_gain, 0, 7, 1);
static const DECLARE_TLV_DB_SCALE(analog_gain, 0, 25, 1);
static struct snd_soc_dai_driver msm8x10_wcd_i2s_dai[];
static const DECLARE_TLV_DB_SCALE(aux_pga_gain, 0, 2, 0);

/* Codec supports 2 IIR filters */
enum {
	IIR1 = 0,
	IIR2,
	IIR_MAX,
};
/* Codec supports 5 bands */
enum {
	BAND1 = 0,
	BAND2,
	BAND3,
	BAND4,
	BAND5,
	BAND_MAX,
};

struct hpf_work {
	struct msm8x10_wcd_priv *msm8x10_wcd;
	u32 decimator;
	u8 tx_hpf_cut_of_freq;
	struct delayed_work dwork;
};

static struct hpf_work tx_hpf_work[NUM_DECIMATORS];

struct msm8x10_wcd_priv {
	struct snd_soc_codec *codec;
	u32 adc_count;
	u32 rx_bias_count;
	s32 dmic_1_2_clk_cnt;

	/* resmgr module */
	struct wcd9xxx_resmgr resmgr;
	/* mbhc module */
	struct wcd9xxx_mbhc mbhc;
};


static unsigned short rx_digital_gain_reg[] = {
	MSM8X10_WCD_A_CDC_RX1_VOL_CTL_B2_CTL,
	MSM8X10_WCD_A_CDC_RX2_VOL_CTL_B2_CTL,
	MSM8X10_WCD_A_CDC_RX3_VOL_CTL_B2_CTL,
};

static unsigned short tx_digital_gain_reg[] = {
	MSM8X10_WCD_A_CDC_TX1_VOL_CTL_GAIN,
	MSM8X10_WCD_A_CDC_TX2_VOL_CTL_GAIN,
};

struct msm8x10_wcd_i2c {
	struct i2c_client *client;
	struct i2c_msg xfer_msg[2];
	struct mutex xfer_lock;
	int mod_id;
};

/* need to re-visit*/
static char *msm8x10_wcd_supplies[] = {
	"cdc-vdd-mic-bias", "cdc-vdda-h", "cdc-vdd-1p2", "cdc-vdd-px",
	"cdc-vdda-cp",
};

static int msm8x10_wcd_dt_parse_vreg_info(struct device *dev,
	struct msm8x10_wcd_regulator *vreg, const char *vreg_name);
static int msm8x10_wcd_dt_parse_micbias_info(struct device *dev,
	struct msm8x10_wcd_micbias_setting *micbias);
static struct msm8x10_wcd_pdata *msm8x10_wcd_populate_dt_pdata(
	struct device *dev);

struct msm8x10_wcd_i2c msm8x10_wcd_modules[MAX_MSM8X10_WCD_DEVICE];


static int get_i2c_msm8x10_wcd_device_info(u16 reg,
					   struct msm8x10_wcd_i2c **msm8x10_wcd)
{
	int rtn = 0;
	int value = ((reg & 0x0f00) >> 8) & 0x000f;
	pr_debug("%s: reg(0x%x) value(%d)\n", __func__, reg, value);
	switch (value) {
	case 0:
	case 1:
		*msm8x10_wcd = &msm8x10_wcd_modules[value];
		break;
	default:
		rtn = -EINVAL;
		break;
	}
	return rtn;
}

static int msm8x10_wcd_abh_write_device(u16 reg, u8 *value, u32 bytes)
{
	u32 temp = ((u32)(*value)) & 0x000000FF;
	u32 offset = (((u32)(reg)) ^ 0x00000400) & 0x00000FFF;
	iowrite32(temp, ioremap(MSM8X10_DINO_CODEC_BASE_ADDR + offset, 4));
	return 0;
}

static int msm8x10_wcd_abh_read_device(u16 reg, u32 bytes, u8 *value)
{
	u32 offset = (((u32)(reg)) ^ 0x00000400) & 0x00000FFF;
	*value = (u8)ioread32(ioremap(MSM8X10_DINO_CODEC_BASE_ADDR +
				      offset, 4));
	return 0;
}

static int msm8x10_wcd_i2c_write_device(u16 reg, u8 *value, u32 bytes)
{

	struct i2c_msg *msg;
	int ret;
	u8 reg_addr = 0;
	u8 data[bytes + 1];
	struct msm8x10_wcd_i2c *msm8x10_wcd;

	ret = get_i2c_msm8x10_wcd_device_info(reg, &msm8x10_wcd);
	if (!ret) {
		pr_err("%s: Invalid register address\n", __func__);
		return ret;
	}

	if (msm8x10_wcd == NULL || msm8x10_wcd->client == NULL) {
		pr_err("%s: Failed to get device info\n", __func__);
		return -ENODEV;
	}
	reg_addr = (u8)reg;
	msg = &msm8x10_wcd->xfer_msg[0];
	msg->addr = msm8x10_wcd->client->addr;
	msg->len = bytes + 1;
	msg->flags = 0;
	data[0] = reg;
	data[1] = *value;
	msg->buf = data;
	ret = i2c_transfer(msm8x10_wcd->client->adapter,
			   msm8x10_wcd->xfer_msg, 1);
	/* Try again if the write fails */
	if (ret != 1) {
		ret = i2c_transfer(msm8x10_wcd->client->adapter,
						msm8x10_wcd->xfer_msg, 1);
		if (ret != 1) {
			pr_err("failed to write the device\n");
			return ret;
		}
	}
	pr_debug("write sucess register = %x val = %x\n", reg, data[1]);
	return 0;
}


int msm8x10_wcd_i2c_read_device(u32 reg, u32 bytes, u8 *dest)
{
	struct i2c_msg *msg;
	int ret = 0;
	u8 reg_addr = 0;
	struct msm8x10_wcd_i2c *msm8x10_wcd;
	u8 i = 0;

	ret = get_i2c_msm8x10_wcd_device_info(reg, &msm8x10_wcd);
	if (!ret) {
		pr_err("%s: Invalid register address\n", __func__);
		return ret;
	}

	if (msm8x10_wcd == NULL || msm8x10_wcd->client == NULL) {
		pr_err("%s: Failed to get device info\n", __func__);
		return -ENODEV;
	}

	for (i = 0; i < bytes; i++) {
		reg_addr = (u8)reg++;
		msg = &msm8x10_wcd->xfer_msg[0];
		msg->addr = msm8x10_wcd->client->addr;
		msg->len = 1;
		msg->flags = 0;
		msg->buf = &reg_addr;

		msg = &msm8x10_wcd->xfer_msg[1];
		msg->addr = msm8x10_wcd->client->addr;
		msg->len = 1;
		msg->flags = I2C_M_RD;
		msg->buf = dest++;
		ret = i2c_transfer(msm8x10_wcd->client->adapter,
				msm8x10_wcd->xfer_msg, 2);

		/* Try again if read fails first time */
		if (ret != 2) {
			ret = i2c_transfer(msm8x10_wcd->client->adapter,
					   msm8x10_wcd->xfer_msg, 2);
			if (ret != 2) {
				pr_err("failed to read msm8x10_wcd register\n");
				return ret;
			}
		}
	}
	return 0;
}

static int msm8x10_wcd_reg_read(struct msm8x10_wcd *msm8x10_wcd, u16 reg)
{
	u8 val;
	int ret = -EINVAL;

	/* check if use I2C interface for Helicon or AHB for Dino*/
	mutex_lock(&msm8x10_wcd->io_lock);
	if (MSM8X10_WCD_IS_HELICON_REG(reg))
		ret = msm8x10_wcd_i2c_read_device(reg, 1, &val);
	else if (MSM8X10_WCD_IS_DINO_REG(reg))
		ret = msm8x10_wcd_abh_read_device(reg, 1, &val);
	mutex_unlock(&msm8x10_wcd->io_lock);

	if (ret < 0)
		return ret;
	else
		return val;
}


static int msm8x10_wcd_reg_write(struct msm8x10_wcd *msm8x10_wcd, u16  reg,
				 u8 val)
{
	int ret = -EINVAL;

	/* check if use I2C interface for Helicon or AHB for Dino*/
	mutex_lock(&msm8x10_wcd->io_lock);
	if (MSM8X10_WCD_IS_HELICON_REG(reg))
		ret = msm8x10_wcd_i2c_write_device(reg, &val, 1);
	else if (MSM8X10_WCD_IS_DINO_REG(reg))
		ret = msm8x10_wcd_abh_write_device(reg, &val, 1);
	mutex_unlock(&msm8x10_wcd->io_lock);

	return ret;
}

static bool msm8x10_wcd_is_digital_gain_register(unsigned int reg)
{
	bool rtn = false;
	switch (reg) {
	case MSM8X10_WCD_A_CDC_RX1_VOL_CTL_B2_CTL:
	case MSM8X10_WCD_A_CDC_RX2_VOL_CTL_B2_CTL:
	case MSM8X10_WCD_A_CDC_RX3_VOL_CTL_B2_CTL:
	case MSM8X10_WCD_A_CDC_TX1_VOL_CTL_GAIN:
	case MSM8X10_WCD_A_CDC_TX2_VOL_CTL_GAIN:
		rtn = true;
		break;
	default:
		break;
	}
	return rtn;
}

static int msm8x10_wcd_volatile(struct snd_soc_codec *ssc, unsigned int reg)
{
	/* Registers lower than 0x100 are top level registers which can be
	 * written by the Taiko core driver.
	 */

	if ((reg >= MSM8X10_WCD_A_CDC_MBHC_EN_CTL) || (reg < 0x100))
		return 1;

	/* IIR Coeff registers are not cacheable */
	if ((reg >= MSM8X10_WCD_A_CDC_IIR1_COEF_B1_CTL) &&
		(reg <= MSM8X10_WCD_A_CDC_IIR2_COEF_B2_CTL))
		return 1;

	/* Digital gain register is not cacheable so we have to write
	 * the setting even it is the same
	 */
	if (msm8x10_wcd_is_digital_gain_register(reg))
		return 1;

	/* HPH status registers */
	if (reg == MSM8X10_WCD_A_RX_HPH_L_STATUS ||
	    reg == MSM8X10_WCD_A_RX_HPH_R_STATUS)
		return 1;

	if (reg == MSM8X10_WCD_A_MBHC_INSERT_DET_STATUS)
		return 1;

	return 0;
}

static int msm8x10_wcd_readable(struct snd_soc_codec *ssc, unsigned int reg)
{
	return msm8x10_wcd_reg_readable[reg];
}

static int msm8x10_wcd_write(struct snd_soc_codec *codec, unsigned int reg,
			     unsigned int value)
{
	int ret;

	if (reg == SND_SOC_NOPM)
		return 0;

	BUG_ON(reg > MSM8X10_WCD_MAX_REGISTER);

	if (!msm8x10_wcd_volatile(codec, reg)) {
		ret = snd_soc_cache_write(codec, reg, value);
		if (ret != 0)
			dev_err(codec->dev, "Cache write to %x failed: %d\n",
				reg, ret);
	}

	return msm8x10_wcd_reg_write(codec->control_data, reg, value);
}
static unsigned int msm8x10_wcd_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	unsigned int val;
	int ret;

	if (reg == SND_SOC_NOPM)
		return 0;

	BUG_ON(reg > MSM8X10_WCD_MAX_REGISTER);

	if (!msm8x10_wcd_volatile(codec, reg) &&
	    msm8x10_wcd_readable(codec, reg) &&
		reg < codec->driver->reg_cache_size) {
		ret = snd_soc_cache_read(codec, reg, &val);
		if (ret >= 0) {
			return val;
		} else
			dev_err(codec->dev, "Cache read from %x failed: %d\n",
				reg, ret);
	}

	val = msm8x10_wcd_reg_read(codec->control_data, reg);
	return val;
}


static int msm8x10_wcd_dt_parse_vreg_info(struct device *dev,
	struct msm8x10_wcd_regulator *vreg, const char *vreg_name)
{
	int len, ret = 0;
	const __be32 *prop;
	char prop_name[CODEC_DT_MAX_PROP_SIZE];
	struct device_node *regnode = NULL;
	u32 prop_val;

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE, "%s-supply",
		vreg_name);
	regnode = of_parse_phandle(dev->of_node, prop_name, 0);

	if (!regnode) {
		dev_err(dev, "Looking up %s property in node %s failed",
				prop_name, dev->of_node->full_name);
		return -ENODEV;
	}
	vreg->name = vreg_name;

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE,
		"qcom,%s-voltage", vreg_name);
	prop = of_get_property(dev->of_node, prop_name, &len);

	if (!prop || (len != (2 * sizeof(__be32)))) {
		dev_err(dev, "%s %s property\n",
				prop ? "invalid format" : "no", prop_name);
		return -ENODEV;
	} else {
		vreg->min_uV = be32_to_cpup(&prop[0]);
		vreg->max_uV = be32_to_cpup(&prop[1]);
	}

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE,
			"qcom,%s-current", vreg_name);

	ret = of_property_read_u32(dev->of_node, prop_name, &prop_val);
	if (ret) {
		dev_err(dev, "Looking up %s property in node %s failed",
				prop_name, dev->of_node->full_name);
		return -ENODEV;
	}
	vreg->optimum_uA = prop_val;

	dev_info(dev, "%s: vol=[%d %d]uV, curr=[%d]uA\n", vreg->name,
		vreg->min_uV, vreg->max_uV, vreg->optimum_uA);
	return 0;
}

static int msm8x10_wcd_dt_parse_micbias_info(struct device *dev,
	struct msm8x10_wcd_micbias_setting *micbias)
{
	int ret = 0;
	char prop_name[CODEC_DT_MAX_PROP_SIZE];
	u32 prop_val;

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE,
			"qcom,cdc-micbias-ldoh-v");
	ret = of_property_read_u32(dev->of_node, prop_name, &prop_val);
	if (ret) {
		dev_err(dev, "Looking up %s property in node %s failed",
			prop_name, dev->of_node->full_name);
		return -ENODEV;
	}
	micbias->ldoh_v  =  (u8)prop_val;

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE,
			"qcom,cdc-micbias-cfilt1-mv");
	ret = of_property_read_u32(dev->of_node, prop_name,
				   &micbias->cfilt1_mv);
	if (ret) {
		dev_err(dev, "Looking up %s property in node %s failed",
			prop_name, dev->of_node->full_name);
		return -ENODEV;
	}

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE,
			"qcom,cdc-micbias1-cfilt-sel");
	ret = of_property_read_u32(dev->of_node, prop_name, &prop_val);
	if (ret) {
		dev_err(dev, "Looking up %s property in node %s failed",
			prop_name, dev->of_node->full_name);
		return -ENODEV;
	}
	micbias->bias1_cfilt_sel = (u8)prop_val;

	/* micbias external cap */
	micbias->bias1_cap_mode =
	    (of_property_read_bool(dev->of_node, "qcom,cdc-micbias1-ext-cap") ?
	     MICBIAS_EXT_BYP_CAP : MICBIAS_NO_EXT_BYP_CAP);

	dev_dbg(dev, "ldoh_v  %u cfilt1_mv %u\n",
			(u32)micbias->ldoh_v, (u32)micbias->cfilt1_mv);
	dev_dbg(dev, "bias1_cfilt_sel %u\n", (u32)micbias->bias1_cfilt_sel);
	dev_dbg(dev, "bias1_ext_cap %d\n", micbias->bias1_cap_mode);

	return 0;
}

static struct msm8x10_wcd_pdata *msm8x10_wcd_populate_dt_pdata(
						struct device *dev)
{
	struct msm8x10_wcd_pdata *pdata;
	int ret, i;
	char **codec_supplies;
	u32 num_of_supplies = 0;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for platform data\n");
		return NULL;
	}
	if ((!strcmp(dev_name(dev), MSM8X10_WCD_I2C_GSBI_SLAVE_ID))) {
		codec_supplies = msm8x10_wcd_supplies;
		num_of_supplies = ARRAY_SIZE(msm8x10_wcd_supplies);
	} else {
		dev_err(dev, "%s unsupported device %s\n",
				__func__, dev_name(dev));
		goto err;
	}

	if (num_of_supplies > ARRAY_SIZE(pdata->regulator)) {
		dev_err(dev, "%s: Num of supplies %u > max supported %u\n",
		      __func__, num_of_supplies, ARRAY_SIZE(pdata->regulator));

		goto err;
	}

	for (i = 0; i < num_of_supplies; i++) {
		ret = msm8x10_wcd_dt_parse_vreg_info(dev, &pdata->regulator[i],
			codec_supplies[i]);
		if (ret)
			goto err;
	}

	ret = msm8x10_wcd_dt_parse_micbias_info(dev, &pdata->micbias);
	if (ret)
		goto err;

	pdata->reset_gpio = of_get_named_gpio(dev->of_node,
				"qcom,cdc-reset-gpio", 0);
	if (pdata->reset_gpio < 0) {
		dev_err(dev, "Looking up %s property in node %s failed %d\n",
			"qcom, cdc-reset-gpio", dev->of_node->full_name,
			pdata->reset_gpio);
		goto err;
	}
	dev_dbg(dev, "%s: reset gpio %d", __func__, pdata->reset_gpio);
	return pdata;
err:
	devm_kfree(dev, pdata);
	return NULL;
}

static int msm8x10_wcd_codec_enable_charge_pump(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* Enable charge pump clock*/
		snd_soc_update_bits(codec, MSM8X10_WCD_A_CDC_CLK_OTHR_CTL,
				    0x01, 0x01);
		snd_soc_update_bits(codec, MSM8X10_WCD_A_CDC_CLSG_CTL,
				    0x08, 0x08);
		usleep_range(200, 300);
		snd_soc_update_bits(codec, MSM8X10_WCD_A_CP_STATIC,
				    0x10, 0x00);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec,
				    MSM8X10_WCD_A_CDC_CLK_OTHR_RESET_B1_CTL,
				    0x01, 0x01);
		usleep_range(20, 100);
		snd_soc_update_bits(codec,
				    MSM8X10_WCD_A_CP_STATIC, 0x08, 0x08);
		snd_soc_update_bits(codec,
				    MSM8X10_WCD_A_CP_STATIC, 0x10, 0x10);
		snd_soc_update_bits(codec,
				    MSM8X10_WCD_A_CDC_CLSG_CTL, 0x08, 0x00);
		snd_soc_update_bits(codec,
				    MSM8X10_WCD_A_CDC_CLK_OTHR_CTL, 0x01,
				    0x00);
		snd_soc_update_bits(codec,
				    MSM8X10_WCD_A_CP_STATIC, 0x08, 0x00);
		break;
	}
	return 0;
}

static int msm8x10_wcd_pa_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	ear_pa_gain = snd_soc_read(codec, MSM8X10_WCD_A_RX_EAR_GAIN);

	ear_pa_gain = ear_pa_gain >> 5;

	if (ear_pa_gain == 0x00) {
		ucontrol->value.integer.value[0] = 0;
	} else if (ear_pa_gain == 0x04) {
		ucontrol->value.integer.value[0] = 1;
	} else  {
		pr_err("%s: ERROR: Unsupported Ear Gain = 0x%x\n",
				__func__, ear_pa_gain);
		return -EINVAL;
	}

	pr_debug("%s: ear_pa_gain = 0x%x\n", __func__, ear_pa_gain);

	return 0;
}

static int msm8x10_wcd_pa_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	pr_debug("%s: ucontrol->value.integer.value[0]  = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
		ear_pa_gain = 0x00;
		break;
	case 1:
		ear_pa_gain = 0x80;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, MSM8X10_WCD_A_RX_EAR_GAIN,
			    0xE0, ear_pa_gain);
	return 0;
}

static int msm8x10_wcd_get_iir_enable_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] =
		snd_soc_read(codec,
			    (MSM8X10_WCD_A_CDC_IIR1_CTL + 64 * iir_idx)) &
		(1 << band_idx);

	pr_debug("%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0]);
	return 0;
}

static int msm8x10_wcd_put_iir_enable_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];

	/* Mask first 5 bits, 6-8 are reserved */
	snd_soc_update_bits(codec, (MSM8X10_WCD_A_CDC_IIR1_CTL + 64 * iir_idx),
		(1 << band_idx), (value << band_idx));

	pr_debug("%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx, value);
	return 0;
}
static uint32_t get_iir_band_coeff(struct snd_soc_codec *codec,
				int iir_idx, int band_idx,
				int coeff_idx)
{
	/* Address does not automatically update if reading */
	snd_soc_write(codec,
		(MSM8X10_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
		(band_idx * BAND_MAX + coeff_idx) & 0x1F);

	/* Mask bits top 2 bits since they are reserved */
	return ((snd_soc_read(codec,
		(MSM8X10_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx)) << 24)) &
		0x3FFFFFFF;
}

static int msm8x10_wcd_get_iir_band_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 0);
	ucontrol->value.integer.value[1] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 1);
	ucontrol->value.integer.value[2] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 2);
	ucontrol->value.integer.value[3] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 3);
	ucontrol->value.integer.value[4] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 4);

	pr_debug("%s: IIR #%d band #%d b0 = 0x%x\n"
		"%s: IIR #%d band #%d b1 = 0x%x\n"
		"%s: IIR #%d band #%d b2 = 0x%x\n"
		"%s: IIR #%d band #%d a1 = 0x%x\n"
		"%s: IIR #%d band #%d a2 = 0x%x\n",
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[1],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[2],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[3],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[4]);
	return 0;
}

static void set_iir_band_coeff(struct snd_soc_codec *codec,
				int iir_idx, int band_idx,
				int coeff_idx, uint32_t value)
{
	/* Mask top 3 bits, 6-8 are reserved */
	/* Update address manually each time */
	snd_soc_write(codec,
		(MSM8X10_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
		(band_idx * BAND_MAX + coeff_idx) & 0x1F);

	/* Mask top 2 bits, 7-8 are reserved */
	snd_soc_write(codec,
		(MSM8X10_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value >> 24) & 0x3F);

}

static int msm8x10_wcd_put_iir_band_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	set_iir_band_coeff(codec, iir_idx, band_idx, 0,
				ucontrol->value.integer.value[0]);
	set_iir_band_coeff(codec, iir_idx, band_idx, 1,
				ucontrol->value.integer.value[1]);
	set_iir_band_coeff(codec, iir_idx, band_idx, 2,
				ucontrol->value.integer.value[2]);
	set_iir_band_coeff(codec, iir_idx, band_idx, 3,
				ucontrol->value.integer.value[3]);
	set_iir_band_coeff(codec, iir_idx, band_idx, 4,
				ucontrol->value.integer.value[4]);

	pr_debug("%s: IIR #%d band #%d b0 = 0x%x\n"
		"%s: IIR #%d band #%d b1 = 0x%x\n"
		"%s: IIR #%d band #%d b2 = 0x%x\n"
		"%s: IIR #%d band #%d a1 = 0x%x\n"
		"%s: IIR #%d band #%d a2 = 0x%x\n",
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 0),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 1),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 2),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 3),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 4));
	return 0;
}

static const char * const msm8x10_wcd_ear_pa_gain_text[] = {
		"POS_6_DB", "POS_2_DB"};
static const struct soc_enum msm8x10_wcd_ear_pa_gain_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, msm8x10_wcd_ear_pa_gain_text),
};

/*cut of frequency for high pass filter*/
static const char * const cf_text[] = {
	"MIN_3DB_4Hz", "MIN_3DB_75Hz", "MIN_3DB_150Hz"
};

static const struct soc_enum cf_dec1_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_TX1_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec2_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_TX2_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_rxmix1_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_RX1_B4_CTL, 0, 3, cf_text);

static const struct soc_enum cf_rxmix2_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_RX2_B4_CTL, 0, 3, cf_text);

static const struct soc_enum cf_rxmix3_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_RX3_B4_CTL, 0, 3, cf_text);

static const struct snd_kcontrol_new msm8x10_wcd_snd_controls[] = {

	SOC_ENUM_EXT("EAR PA Gain", msm8x10_wcd_ear_pa_gain_enum[0],
		msm8x10_wcd_pa_gain_get, msm8x10_wcd_pa_gain_put),

	SOC_SINGLE_TLV("LINEOUT1 Volume", MSM8X10_WCD_A_RX_LINE_1_GAIN,
		       0, 12, 1, line_gain),

	SOC_SINGLE_TLV("HPHL Volume", MSM8X10_WCD_A_RX_HPH_L_GAIN,
		       0, 12, 1, line_gain),
	SOC_SINGLE_TLV("HPHR Volume", MSM8X10_WCD_A_RX_HPH_R_GAIN,
		       0, 12, 1, line_gain),

	SOC_SINGLE_SX_TLV("RX1 Digital Volume",
			  MSM8X10_WCD_A_CDC_RX1_VOL_CTL_B2_CTL,
			  0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX2 Digital Volume",
			  MSM8X10_WCD_A_CDC_RX2_VOL_CTL_B2_CTL,
			  0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX3 Digital Volume",
			  MSM8X10_WCD_A_CDC_RX3_VOL_CTL_B2_CTL,
			  0, -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("DEC1 Volume",
			  MSM8X10_WCD_A_CDC_TX1_VOL_CTL_GAIN,
			  0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC2 Volume",
			  MSM8X10_WCD_A_CDC_TX2_VOL_CTL_GAIN,
			  0, -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("IIR1 INP1 Volume",
			  MSM8X10_WCD_A_CDC_IIR1_GAIN_B1_CTL,
			  0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP2 Volume",
			  MSM8X10_WCD_A_CDC_IIR1_GAIN_B2_CTL,
			  0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP3 Volume",
			  MSM8X10_WCD_A_CDC_IIR1_GAIN_B3_CTL,
			  0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP4 Volume",
			  MSM8X10_WCD_A_CDC_IIR1_GAIN_B4_CTL,
			  0, -84,	40, digital_gain),

	SOC_SINGLE("MICBIAS1 CAPLESS Switch",
		   MSM8X10_WCD_A_MICB_1_CTL, 4, 1, 1),

	SOC_ENUM("TX1 HPF cut off", cf_dec1_enum),
	SOC_ENUM("TX2 HPF cut off", cf_dec2_enum),

	SOC_SINGLE("TX1 HPF Switch", MSM8X10_WCD_A_CDC_TX1_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX2 HPF Switch", MSM8X10_WCD_A_CDC_TX2_MUX_CTL, 3, 1, 0),

	SOC_SINGLE("RX1 HPF Switch", MSM8X10_WCD_A_CDC_RX1_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX2 HPF Switch", MSM8X10_WCD_A_CDC_RX2_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX3 HPF Switch", MSM8X10_WCD_A_CDC_RX3_B5_CTL, 2, 1, 0),

	SOC_ENUM("RX1 HPF cut off", cf_rxmix1_enum),
	SOC_ENUM("RX2 HPF cut off", cf_rxmix2_enum),
	SOC_ENUM("RX3 HPF cut off", cf_rxmix3_enum),

	SOC_SINGLE_EXT("IIR1 Enable Band1", IIR1, BAND1, 1, 0,
	msm8x10_wcd_get_iir_enable_audio_mixer,
	msm8x10_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band2", IIR1, BAND2, 1, 0,
	msm8x10_wcd_get_iir_enable_audio_mixer,
	msm8x10_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band3", IIR1, BAND3, 1, 0,
	msm8x10_wcd_get_iir_enable_audio_mixer,
	msm8x10_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band4", IIR1, BAND4, 1, 0,
	msm8x10_wcd_get_iir_enable_audio_mixer,
	msm8x10_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band5", IIR1, BAND5, 1, 0,
	msm8x10_wcd_get_iir_enable_audio_mixer,
	msm8x10_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band1", IIR2, BAND1, 1, 0,
	msm8x10_wcd_get_iir_enable_audio_mixer,
	msm8x10_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band2", IIR2, BAND2, 1, 0,
	msm8x10_wcd_get_iir_enable_audio_mixer,
	msm8x10_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band3", IIR2, BAND3, 1, 0,
	msm8x10_wcd_get_iir_enable_audio_mixer,
	msm8x10_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band4", IIR2, BAND4, 1, 0,
	msm8x10_wcd_get_iir_enable_audio_mixer,
	msm8x10_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band5", IIR2, BAND5, 1, 0,
	msm8x10_wcd_get_iir_enable_audio_mixer,
	msm8x10_wcd_put_iir_enable_audio_mixer),

	SOC_SINGLE_MULTI_EXT("IIR1 Band1", IIR1, BAND1, 255, 0, 5,
	msm8x10_wcd_get_iir_band_audio_mixer,
	msm8x10_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band2", IIR1, BAND2, 255, 0, 5,
	msm8x10_wcd_get_iir_band_audio_mixer,
	msm8x10_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band3", IIR1, BAND3, 255, 0, 5,
	msm8x10_wcd_get_iir_band_audio_mixer,
	msm8x10_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band4", IIR1, BAND4, 255, 0, 5,
	msm8x10_wcd_get_iir_band_audio_mixer,
	msm8x10_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band5", IIR1, BAND5, 255, 0, 5,
	msm8x10_wcd_get_iir_band_audio_mixer,
	msm8x10_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band1", IIR2, BAND1, 255, 0, 5,
	msm8x10_wcd_get_iir_band_audio_mixer,
	msm8x10_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band2", IIR2, BAND2, 255, 0, 5,
	msm8x10_wcd_get_iir_band_audio_mixer,
	msm8x10_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band3", IIR2, BAND3, 255, 0, 5,
	msm8x10_wcd_get_iir_band_audio_mixer,
	msm8x10_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band4", IIR2, BAND4, 255, 0, 5,
	msm8x10_wcd_get_iir_band_audio_mixer,
	msm8x10_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band5", IIR2, BAND5, 255, 0, 5,
	msm8x10_wcd_get_iir_band_audio_mixer,
	msm8x10_wcd_put_iir_band_audio_mixer),

};

static const char * const rx_mix1_text[] = {
	"ZERO", "IIR1", "IIR2", "RX1", "RX2", "RX3"
};

static const char * const rx_mix2_text[] = {
	"ZERO", "IIR1", "IIR2"
};

static const char * const dec_mux_text[] = {
	"ZERO", "ADC1", "ADC2", "DMIC1", "DMIC2"
};


static const char * const anc_mux_text[] = {
	"ZERO", "ADC1", "ADC2", "ADC3", "ADC4", "ADC5", "ADC6", "ADC_MB",
		"RSVD_1", "DMIC1", "DMIC2", "DMIC3", "DMIC4", "DMIC5", "DMIC6"
};

static const char * const anc1_fb_mux_text[] = {
	"ZERO", "EAR_HPH_L", "EAR_LINE_1",
};

static const char * const iir1_inp1_text[] = {
	"ZERO", "DEC1", "DEC2", "RX1", "RX2", "RX3"
};

static const struct soc_enum rx_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_CONN_RX1_B1_CTL, 0, 6, rx_mix1_text);

static const struct soc_enum rx_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_CONN_RX1_B1_CTL, 3, 6, rx_mix1_text);

static const struct soc_enum rx_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_CONN_RX1_B2_CTL, 0, 6, rx_mix1_text);

static const struct soc_enum rx2_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_CONN_RX2_B1_CTL, 0, 6, rx_mix1_text);

static const struct soc_enum rx2_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_CONN_RX2_B1_CTL, 3, 6, rx_mix1_text);

static const struct soc_enum rx3_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_CONN_RX3_B1_CTL, 0, 6, rx_mix1_text);

static const struct soc_enum rx3_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_CONN_RX3_B1_CTL, 3, 6, rx_mix1_text);

static const struct soc_enum rx1_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_CONN_RX1_B3_CTL, 0, 3, rx_mix2_text);

static const struct soc_enum rx2_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_CONN_RX2_B3_CTL, 0, 3, rx_mix2_text);

static const struct soc_enum dec1_mux_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_CONN_TX_B1_CTL, 0, 5, dec_mux_text);

static const struct soc_enum dec2_mux_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_CONN_TX_B1_CTL, 3, 5, dec_mux_text);

static const struct soc_enum iir1_inp1_mux_enum =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_CONN_EQ1_B1_CTL, 0, 6,
	iir1_inp1_text);

static const struct snd_kcontrol_new rx_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX1 MIX1 INP1 Mux", rx_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX1 MIX1 INP2 Mux", rx_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx_mix1_inp3_mux =
	SOC_DAPM_ENUM("RX1 MIX1 INP3 Mux", rx_mix1_inp3_chain_enum);

static const struct snd_kcontrol_new rx2_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX2 MIX1 INP1 Mux", rx2_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx2_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX2 MIX1 INP2 Mux", rx2_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx3_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP1 Mux", rx3_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx3_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP2 Mux", rx3_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx1_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX1 MIX2 INP1 Mux", rx1_mix2_inp1_chain_enum);

static const struct snd_kcontrol_new rx2_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX2 MIX2 INP1 Mux", rx2_mix2_inp1_chain_enum);

static int msm8x10_wcd_put_dec_enum(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *w = wlist->widgets[0];
	struct snd_soc_codec *codec = w->codec;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int dec_mux, decimator;
	char *dec_name = NULL;
	char *widget_name = NULL;
	char *temp;
	u16 tx_mux_ctl_reg;
	u8 adc_dmic_sel = 0x0;
	int ret = 0;

	if (ucontrol->value.enumerated.item[0] > e->max - 1)
		return -EINVAL;

	dec_mux = ucontrol->value.enumerated.item[0];

	widget_name = kstrndup(w->name, 15, GFP_KERNEL);
	if (!widget_name)
		return -ENOMEM;
	temp = widget_name;

	dec_name = strsep(&widget_name, " ");
	widget_name = temp;
	if (!dec_name) {
		pr_err("%s: Invalid decimator = %s\n", __func__, w->name);
		ret =  -EINVAL;
		goto out;
	}

	ret = kstrtouint(strpbrk(dec_name, "12"), 10, &decimator);
	if (ret < 0) {
		pr_err("%s: Invalid decimator = %s\n", __func__, dec_name);
		ret =  -EINVAL;
		goto out;
	}

	dev_dbg(w->dapm->dev, "%s(): widget = %s decimator = %u dec_mux = %u\n"
		, __func__, w->name, decimator, dec_mux);

	switch (decimator) {
	case 1:
	case 2:
		if (dec_mux == 1)
			adc_dmic_sel = 0x1;
		else
			adc_dmic_sel = 0x0;
		break;
	default:
		pr_err("%s: Invalid Decimator = %u\n", __func__, decimator);
		ret = -EINVAL;
		goto out;
	}

	tx_mux_ctl_reg = MSM8X10_WCD_A_CDC_TX1_MUX_CTL + 32 * (decimator - 1);

	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x1, adc_dmic_sel);

	ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);

out:
	kfree(widget_name);
	return ret;
}

#define MSM8X10_WCD_DEC_ENUM(xname, xenum) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_enum_double, \
	.get = snd_soc_dapm_get_enum_double, \
	.put = msm8x10_wcd_put_dec_enum, \
	.private_value = (unsigned long)&xenum }

static const struct snd_kcontrol_new dec1_mux =
	MSM8X10_WCD_DEC_ENUM("DEC1 MUX Mux", dec1_mux_enum);

static const struct snd_kcontrol_new dec2_mux =
	MSM8X10_WCD_DEC_ENUM("DEC2 MUX Mux", dec2_mux_enum);

static const struct snd_kcontrol_new iir1_inp1_mux =
	SOC_DAPM_ENUM("IIR1 INP1 Mux", iir1_inp1_mux_enum);

static const struct snd_kcontrol_new dac1_switch[] = {
	SOC_DAPM_SINGLE("Switch", MSM8X10_WCD_A_RX_EAR_EN, 5, 1, 0)
};
static const struct snd_kcontrol_new hphl_switch[] = {
	SOC_DAPM_SINGLE("Switch", MSM8X10_WCD_A_RX_HPH_L_DAC_CTL, 6, 1, 0)
};

/* virtual port entries */
static int slim_tx_mixer_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];

	ucontrol->value.integer.value[0] = widget->value;
	return 0;
}

static int slim_tx_mixer_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int slim_rx_mux_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];

	ucontrol->value.enumerated.item[0] = widget->value;
	return 0;
}

static int slim_rx_mux_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}


static const char *const slim_rx_mux_text[] = {
	"ZERO", "AIF1_PB"
};

static const struct soc_enum slim_rx_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(slim_rx_mux_text), slim_rx_mux_text);

static const struct snd_kcontrol_new slim_rx_mux[MSM8X10_WCD_RX_MAX] = {
	SOC_DAPM_ENUM_EXT("I2S RX1 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("I2S RX2 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("I2S RX3 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
};

static const struct snd_kcontrol_new aif_cap_mixer[] = {
	SOC_SINGLE_EXT("I2S TX1", SND_SOC_NOPM, MSM8X10_WCD_TX1, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("I2S TX2", SND_SOC_NOPM, MSM8X10_WCD_TX2, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("I2S TX3", SND_SOC_NOPM, MSM8X10_WCD_TX3, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("I2S TX4", SND_SOC_NOPM, MSM8X10_WCD_TX4, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
};


static void msm8x10_wcd_codec_enable_adc_block(struct snd_soc_codec *codec,
					 int enable)
{
	struct msm8x10_wcd_priv *taiko = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s %d\n", __func__, enable);

	if (enable) {
		taiko->adc_count++;
		snd_soc_update_bits(codec,
				    MSM8X10_WCD_A_CDC_ANA_CLK_CTL,
				    0x20, 0x20);
	} else {
		taiko->adc_count--;
		if (!taiko->adc_count)
			snd_soc_update_bits(codec,
					    MSM8X10_WCD_A_CDC_ANA_CLK_CTL,
					    0x20, 0x0);
	}
}

static int msm8x10_wcd_codec_enable_adc(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 adc_reg;
	u8 init_bit_shift;

	pr_debug("%s %d\n", __func__, event);
	adc_reg = MSM8X10_WCD_A_TX_1_2_TEST_CTL;

	if (w->reg == MSM8X10_WCD_A_TX_1_EN)
		init_bit_shift = 7;
	else if (adc_reg == MSM8X10_WCD_A_TX_2_EN)
		init_bit_shift = 6;
	else {
		pr_err("%s: Error, invalid adc register\n", __func__);
		return -EINVAL;
	}


	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		msm8x10_wcd_codec_enable_adc_block(codec, 1);
		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift,
				1 << init_bit_shift);
		break;
	case SND_SOC_DAPM_POST_PMU:

		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift, 0x00);

		break;
	case SND_SOC_DAPM_POST_PMD:
		msm8x10_wcd_codec_enable_adc_block(codec, 0);
		break;
	}
	return 0;
}

static int msm8x10_wcd_codec_enable_lineout(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 lineout_gain_reg;

	pr_debug("%s %d %s\n", __func__, event, w->name);

	switch (w->shift) {
	case 0:
		lineout_gain_reg = MSM8X10_WCD_A_RX_LINE_1_GAIN;
		break;
	default:
		pr_err("%s: Error, incorrect lineout register value\n",
			__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, lineout_gain_reg, 0x40, 0x40);
		break;
	case SND_SOC_DAPM_POST_PMU:
		pr_debug("%s: sleeping 16 ms after %s PA turn on\n",
				__func__, w->name);
		usleep_range(16000, 16100);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, lineout_gain_reg, 0x40, 0x00);
		break;
	}
	return 0;
}

static int msm8x10_wcd_codec_enable_spk_pa(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s %d %s\n", __func__, event, w->name);
	return 0;
}

static int msm8x10_wcd_codec_enable_dmic(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x10_wcd_priv *msm8x10_wcd = snd_soc_codec_get_drvdata(codec);
	u8  dmic_clk_en;
	u16 dmic_clk_reg;
	s32 *dmic_clk_cnt;
	unsigned int dmic;
	int ret;

	ret = kstrtouint(strpbrk(w->name, "12"), 10, &dmic);
	if (ret < 0) {
		pr_err("%s: Invalid DMIC line on the codec\n", __func__);
		return -EINVAL;
	}

	switch (dmic) {
	case 1:
	case 2:
		dmic_clk_en = 0x01;
		dmic_clk_cnt = &(msm8x10_wcd->dmic_1_2_clk_cnt);
		dmic_clk_reg = MSM8X10_WCD_A_CDC_CLK_DMIC_B1_CTL;
		pr_debug("%s() event %d DMIC%d dmic_1_2_clk_cnt %d\n",
			__func__, event,  dmic, *dmic_clk_cnt);
		break;
	default:
		pr_err("%s: Invalid DMIC Selection\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:

		(*dmic_clk_cnt)++;
		if (*dmic_clk_cnt == 1)
			snd_soc_update_bits(codec, dmic_clk_reg,
					dmic_clk_en, dmic_clk_en);

		break;
	case SND_SOC_DAPM_POST_PMD:

		(*dmic_clk_cnt)--;
		if (*dmic_clk_cnt  == 0)
			snd_soc_update_bits(codec, dmic_clk_reg,
					dmic_clk_en, 0);
		break;
	}
	return 0;
}

/* re-vistit later*/
static int msm8x10_wcd_codec_enable_micbias(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x10_wcd_priv *msm8x10_wcd = snd_soc_codec_get_drvdata(codec);
	u16 micb_int_reg;
	u8 cfilt_sel_val = 0;
	char *internal1_text = "Internal1";
	char *internal2_text = "Internal2";
	char *internal3_text = "Internal3";
	enum wcd9xxx_notify_event e_post_off, e_pre_on, e_post_on;

	pr_debug("%s %d\n", __func__, event);
	switch (w->reg) {
	case MSM8X10_WCD_A_MICB_1_CTL:
		micb_int_reg = MSM8X10_WCD_A_MICB_1_INT_RBIAS;
		cfilt_sel_val =
			msm8x10_wcd->resmgr.pdata->micbias.bias1_cfilt_sel;
		e_pre_on = WCD9XXX_EVENT_PRE_MICBIAS_1_ON;
		e_post_on = WCD9XXX_EVENT_POST_MICBIAS_1_ON;
		e_post_off = WCD9XXX_EVENT_POST_MICBIAS_1_OFF;
		break;
	default:
		pr_err("%s: Error, invalid micbias register\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Let MBHC module know so micbias switch to be off */
		wcd9xxx_resmgr_notifier_call(&msm8x10_wcd->resmgr, e_pre_on);

		/* Get cfilt */
		wcd9xxx_resmgr_cfilt_get(&msm8x10_wcd->resmgr, cfilt_sel_val);

		if (strnstr(w->name, internal1_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0xE0, 0xE0);
		else if (strnstr(w->name, internal2_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x1C, 0x1C);
		else if (strnstr(w->name, internal3_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x3, 0x3);

		break;
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(20000, 20100);
		/* Let MBHC module know so micbias is on */
		wcd9xxx_resmgr_notifier_call(&msm8x10_wcd->resmgr, e_post_on);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Let MBHC module know so micbias switch to be off */
		wcd9xxx_resmgr_notifier_call(&msm8x10_wcd->resmgr, e_post_off);

		if (strnstr(w->name, internal1_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x80, 0x00);
		else if (strnstr(w->name, internal2_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x10, 0x00);
		else if (strnstr(w->name, internal3_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x2, 0x0);

		/* Put cfilt */
		wcd9xxx_resmgr_cfilt_put(&msm8x10_wcd->resmgr, cfilt_sel_val);
		break;
	}

	return 0;
}

#define  TX_MUX_CTL_CUT_OFF_FREQ_MASK	0x30
#define  CF_MIN_3DB_4HZ			0x0
#define  CF_MIN_3DB_75HZ		0x1
#define  CF_MIN_3DB_150HZ		0x2

static int msm8x10_wcd_codec_enable_dec(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	unsigned int decimator;
	char *dec_name = NULL;
	char *widget_name = NULL;
	char *temp;
	int ret = 0;
	u16 dec_reset_reg, tx_vol_ctl_reg, tx_mux_ctl_reg;
	u8 dec_hpf_cut_of_freq;
	int offset;

	pr_debug("%s %d\n", __func__, event);

	widget_name = kstrndup(w->name, 15, GFP_KERNEL);
	if (!widget_name)
		return -ENOMEM;
	temp = widget_name;

	dec_name = strsep(&widget_name, " ");
	widget_name = temp;
	if (!dec_name) {
		pr_err("%s: Invalid decimator = %s\n", __func__, w->name);
		ret =  -EINVAL;
		goto out;
	}

	ret = kstrtouint(strpbrk(dec_name, "12"), 10, &decimator);
	if (ret < 0) {
		pr_err("%s: Invalid decimator = %s\n", __func__, dec_name);
		ret =  -EINVAL;
		goto out;
	}

	pr_debug("%s(): widget = %s dec_name = %s decimator = %u\n", __func__,
			w->name, dec_name, decimator);

	if (w->reg == MSM8X10_WCD_A_CDC_CLK_TX_CLK_EN_B1_CTL) {
		dec_reset_reg = MSM8X10_WCD_A_CDC_CLK_TX_RESET_B1_CTL;
		offset = 0;
	} else {
		pr_err("%s: Error, incorrect dec\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	tx_vol_ctl_reg = MSM8X10_WCD_A_CDC_TX1_VOL_CTL_CFG +
			 32 * (decimator - 1);
	tx_mux_ctl_reg = MSM8X10_WCD_A_CDC_TX1_MUX_CTL +
			  32 * (decimator - 1);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:

		/* Enableable TX digital mute */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x01);

		snd_soc_update_bits(codec, dec_reset_reg, 1 << w->shift,
			1 << w->shift);
		snd_soc_update_bits(codec, dec_reset_reg, 1 << w->shift, 0x0);

		dec_hpf_cut_of_freq = snd_soc_read(codec, tx_mux_ctl_reg);

		dec_hpf_cut_of_freq = (dec_hpf_cut_of_freq & 0x30) >> 4;

		tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq =
			dec_hpf_cut_of_freq;

		if ((dec_hpf_cut_of_freq != CF_MIN_3DB_150HZ)) {

			/* set cut of freq to CF_MIN_3DB_150HZ (0x1); */
			snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30,
					    CF_MIN_3DB_150HZ << 4);
		}

		/* enable HPF */
		snd_soc_update_bits(codec, tx_mux_ctl_reg , 0x08, 0x00);

		break;

	case SND_SOC_DAPM_POST_PMU:

		/* Disable TX digital mute */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x00);

		if (tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq !=
				CF_MIN_3DB_150HZ) {

			schedule_delayed_work(&tx_hpf_work[decimator - 1].dwork,
					msecs_to_jiffies(300));
		}
		/* apply the digital gain after the decimator is enabled*/
		if ((w->shift) < ARRAY_SIZE(tx_digital_gain_reg))
			snd_soc_write(codec,
				  tx_digital_gain_reg[w->shift + offset],
				  snd_soc_read(codec,
				  tx_digital_gain_reg[w->shift + offset])
				  );

		break;

	case SND_SOC_DAPM_PRE_PMD:

		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x01);
		cancel_delayed_work_sync(&tx_hpf_work[decimator - 1].dwork);
		break;

	case SND_SOC_DAPM_POST_PMD:

		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x08, 0x08);
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30,
			(tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq) << 4);

		break;
	}
out:
	kfree(widget_name);
	return ret;
}

static int msm8x10_wcd_codec_enable_interpolator(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %d %s\n", __func__, event, w->name);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, MSM8X10_WCD_A_CDC_CLK_RX_RESET_CTL,
			1 << w->shift, 1 << w->shift);
		snd_soc_update_bits(codec, MSM8X10_WCD_A_CDC_CLK_RX_RESET_CTL,
			1 << w->shift, 0x0);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* apply the digital gain after the interpolator is enabled*/
		if ((w->shift) < ARRAY_SIZE(rx_digital_gain_reg))
			snd_soc_write(codec,
				  rx_digital_gain_reg[w->shift],
				  snd_soc_read(codec,
				  rx_digital_gain_reg[w->shift])
				  );
		break;
	}
	return 0;
}


/* The register address is the same as other codec so it can use resmgr*/
static int msm8x10_wcd_codec_enable_rx_bias(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x10_wcd_priv *msm8x10_wcd = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd9xxx_resmgr_enable_rx_bias(&msm8x10_wcd->resmgr, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd9xxx_resmgr_enable_rx_bias(&msm8x10_wcd->resmgr, 0);
		break;
	}
	return 0;
}
static int msm8x10_wcd_hphr_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, w->reg, 0x40, 0x40);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, w->reg, 0x40, 0x00);
		break;
	}
	return 0;
}

static int msm8x10_wcd_hph_pa_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x10_wcd_priv *msm8x10_wcd = snd_soc_codec_get_drvdata(codec);
	enum wcd9xxx_notify_event e_pre_on, e_post_off;

	pr_debug("%s: %s event = %d\n", __func__, w->name, event);
	if (w->shift == 5) {
		e_pre_on = WCD9XXX_EVENT_PRE_HPHR_PA_ON;
		e_post_off = WCD9XXX_EVENT_POST_HPHR_PA_OFF;
	} else if (w->shift == 4) {
		e_pre_on = WCD9XXX_EVENT_PRE_HPHL_PA_ON;
		e_post_off = WCD9XXX_EVENT_POST_HPHL_PA_OFF;
	} else {
		pr_err("%s: Invalid w->shift %d\n", __func__, w->shift);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Let MBHC module know PA is turning on */
		wcd9xxx_resmgr_notifier_call(&msm8x10_wcd->resmgr, e_pre_on);
		break;

	case SND_SOC_DAPM_POST_PMU:
		usleep_range(10000, 10100);
		break;

	case SND_SOC_DAPM_POST_PMD:
		/* Let MBHC module know PA turned off */
		wcd9xxx_resmgr_notifier_call(&msm8x10_wcd->resmgr, e_post_off);

		/*
		 * schedule work is required because at the time HPH PA DAPM
		 * event callback is called by DAPM framework, CODEC dapm mutex
		 * would have been locked while snd_soc_jack_report also
		 * attempts to acquire same lock.
		 */
		pr_debug("%s: sleep 10 ms after %s PA disable.\n", __func__,
			 w->name);
		usleep_range(10000, 10100);
		break;
	}
	return 0;
}

static int msm8x10_wcd_lineout_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, w->reg, 0x40, 0x40);
		break;

	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, w->reg, 0x40, 0x00);
		break;
	}
	return 0;
}

static int msm8x10_wcd_spk_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s %s %d\n", __func__, w->name, event);
	return 0;
}

static const struct snd_soc_dapm_route audio_i2s_map[] = {
};

static const struct snd_soc_dapm_route audio_map[] = {
	{"RX_I2S_CLK", NULL, "CDC_CONN"},
	{"I2S RX1", NULL, "RX_I2S_CLK"},
	{"I2S RX2", NULL, "RX_I2S_CLK"},
	{"I2S RX3", NULL, "RX_I2S_CLK"},

	{"I2S TX1", NULL, "TX_I2S_CLK"},
	{"I2S TX2", NULL, "TX_I2S_CLK"},
	{"I2S TX3", NULL, "TX_I2S_CLK"},
	{"I2S TX4", NULL, "TX_I2S_CLK"},

	{"AIF1 CAP", NULL, "AIF1_CAP Mixer"},

	{"AIF1_CAP Mixer", "I2S TX1", "I2S TX1 MUX"},
	{"AIF1_CAP Mixer", "I2S TX2", "I2S TX2 MUX"},
	{"AIF1_CAP Mixer", "I2S TX3", "I2S TX3 MUX"},
	{"AIF1_CAP Mixer", "I2S TX4", "I2S TX4 MUX"},

	{"I2S TX1 MUX", NULL, "DEC1 MUX"},
	{"I2S TX2 MUX", NULL, "DEC2 MUX"},
	{"I2S TX3 MUX", NULL, "RX1 MIX1"},
	{"I2S TX4 MUX", "RMIX2", "RX1 MIX2"},
	{"I2S TX4 MUX", "RMIX3", "RX1 MIX3"},

	/* Earpiece (RX MIX1) */
	{"EAR", NULL, "EAR PA"},
	{"EAR PA", NULL, "DAC1"},
	{"DAC1", NULL, "CP"},

	/* Headset (RX MIX1 and RX MIX2) */
	{"HEADPHONE", NULL, "HPHL"},
	{"HEADPHONE", NULL, "HPHR"},

	{"HPHL", NULL, "HPHL DAC"},

	{"HPHR", NULL, "HPHR DAC"},
	{"HPHR_PA_MIXER", NULL, "HPHR DAC"},

	{"HPHL DAC", NULL, "CP"},

	{"HPHR DAC", NULL, "CP"},

	{"DAC1", "Switch", "RX1 CHAIN"},
	{"HPHL DAC", "Switch", "RX1 CHAIN"},
	{"HPHR DAC", NULL, "RX2 CHAIN"},

	{"LINEOUT1", NULL, "LINEOUT1 PA"},
	{"SPK_OUT", NULL, "SPK PA"},

	{"LINEOUT1 PA", NULL, "CP"},
	{"LINEOUT1 PA", NULL, "LINEOUT1 DAC"},

	{"LINEOUT1 DAC", "RX2 INPUT", "RX2 MIX1"},
	{"LINEOUT1 DAC", "RX3 INPUT", "RX3 MIX1"},

	{"SPK PA", NULL, "SPK DAC"},
	{"SPK DAC", NULL, "RX7 MIX2"},

	{"RX1 CHAIN", NULL, "RX1 MIX2"},
	{"RX2 CHAIN", NULL, "RX2 MIX2"},

	{"LINEOUT1 DAC", NULL, "RX_BIAS"},
	{"SPK DAC", NULL, "RX_BIAS"},

	{"RX1 MIX1", NULL, "RX1 MIX1 INP1"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP2"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP3"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP1"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP2"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP1"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP2"},
	{"RX1 MIX2", NULL, "RX1 MIX1"},
	{"RX1 MIX2", NULL, "RX1 MIX2 INP1"},
	{"RX1 MIX2", NULL, "RX1 MIX2 INP2"},
	{"RX2 MIX2", NULL, "RX2 MIX1"},
	{"RX2 MIX2", NULL, "RX2 MIX2 INP1"},
	{"RX2 MIX2", NULL, "RX2 MIX2 INP2"},

	{"I2S RX1 MUX", "AIF1_PB", "AIF1 PB"},
	{"I2S RX2 MUX", "AIF1_PB", "AIF1 PB"},
	{"I2S RX3 MUX", "AIF1_PB", "AIF1 PB"},

	{"I2S RX1", NULL, "I2S RX1 MUX"},
	{"I2S RX2", NULL, "I2S RX2 MUX"},
	{"I2S RX3", NULL, "I2S RX3 MUX"},

	{"RX1 MIX1 INP1", "RX1", "I2S RX1"},
	{"RX1 MIX1 INP1", "RX2", "I2S RX2"},
	{"RX1 MIX1 INP1", "RX3", "I2S RX3"},
	{"RX1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX1 MIX1 INP2", "RX1", "I2S RX1"},
	{"RX1 MIX1 INP2", "RX2", "I2S RX2"},
	{"RX1 MIX1 INP2", "RX3", "I2S RX3"},
	{"RX1 MIX1 INP2", "IIR1", "IIR1"},
	{"RX1 MIX1 INP3", "RX1", "I2S RX1"},
	{"RX1 MIX1 INP3", "RX2", "I2S RX2"},
	{"RX1 MIX1 INP3", "RX3", "I2S RX3"},

	{"RX2 MIX1 INP1", "RX1", "I2S RX1"},
	{"RX2 MIX1 INP1", "RX2", "I2S RX2"},
	{"RX2 MIX1 INP1", "RX3", "I2S RX3"},
	{"RX2 MIX1 INP1", "IIR1", "IIR1"},
	{"RX2 MIX1 INP2", "RX1", "I2S RX1"},
	{"RX2 MIX1 INP2", "RX2", "I2S RX2"},
	{"RX2 MIX1 INP2", "RX3", "I2S RX3"},
	{"RX2 MIX1 INP2", "IIR1", "IIR1"},

	{"RX3 MIX1 INP1", "RX1", "I2S RX1"},
	{"RX3 MIX1 INP1", "RX2", "I2S RX2"},
	{"RX3 MIX1 INP1", "RX3", "I2S RX3"},
	{"RX3 MIX1 INP1", "IIR1", "IIR1"},
	{"RX3 MIX1 INP2", "RX1", "I2S RX1"},
	{"RX3 MIX1 INP2", "RX2", "I2S RX2"},
	{"RX3 MIX1 INP2", "RX3", "I2S RX3"},
	{"RX3 MIX1 INP2", "IIR1", "IIR1"},

	/* Decimator Inputs */
	{"DEC1 MUX", "DMIC1", "DMIC1"},
	{"DEC1 MUX", "DMIC2", "DMIC2"},
	{"DEC1 MUX", "ADC1", "ADC1"},
	{"DEC1 MUX", "ADC2", "ADC2"},
	{"DEC1 MUX", NULL, "CDC_CONN"},

	{"DEC2 MUX", "DMIC1", "DMIC1"},
	{"DEC2 MUX", "DMIC2", "DMIC2"},
	{"DEC2 MUX", "ADC1", "ADC1"},
	{"DEC2 MUX", "ADC2", "ADC2"},
	{"DEC2 MUX", NULL, "CDC_CONN"},

	/* ADC Connections */
	{"ADC1", NULL, "AMIC1"},
	{"ADC2", NULL, "AMIC2"},

	{"IIR1", NULL, "IIR1 INP1 MUX"},
	{"IIR1 INP1 MUX", "DEC1", "DEC1 MUX"},
	{"IIR1 INP1 MUX", "DEC2", "DEC2 MUX"},

	/* There is no LDO_H in Helicon */
	{"MIC BIAS1 Internal1", NULL, "LDO_H"},
	{"MIC BIAS1 Internal2", NULL, "LDO_H"},
	{"MIC BIAS1 External", NULL, "LDO_H"},
};


#define MSM8X10_WCD_FORMATS (SNDRV_PCM_FMTBIT_S16_LE)


static int msm8x10_wcd_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm8x10_wcd *msm8x10_wcd_core =
		dev_get_drvdata(dai->codec->dev);
	pr_debug("%s(): substream = %s  stream = %d\n" , __func__,
		 substream->name, substream->stream);
	if ((msm8x10_wcd_core != NULL) &&
	    (msm8x10_wcd_core->dev != NULL))
		pm_runtime_get_sync(msm8x10_wcd_core->dev);

	return 0;
}

static void msm8x10_wcd_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm8x10_wcd *msm8x10_wcd_core =
		dev_get_drvdata(dai->codec->dev);
	pr_debug("%s(): substream = %s  stream = %d\n" , __func__,
		 substream->name, substream->stream);
	if ((msm8x10_wcd_core != NULL) &&
	    (msm8x10_wcd_core->dev != NULL)) {
		pm_runtime_mark_last_busy(msm8x10_wcd_core->dev);
		pm_runtime_put(msm8x10_wcd_core->dev);
	}
}

/* working here*/
int msm8x10_wcd_mclk_enable(struct snd_soc_codec *codec,
			    int mclk_enable, bool dapm)
{
	struct msm8x10_wcd_priv *msm8x10_wcd = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: mclk_enable = %u, dapm = %d\n", __func__, mclk_enable,
		 dapm);

	WCD9XXX_BCL_LOCK(&msm8x10_wcd->resmgr);
	if (mclk_enable) {
		wcd9xxx_resmgr_get_bandgap(&msm8x10_wcd->resmgr,
					   WCD9XXX_BANDGAP_AUDIO_MODE);
		wcd9xxx_resmgr_get_clk_block(&msm8x10_wcd->resmgr,
					     WCD9XXX_CLK_MCLK);
	} else {
		/* Put clock and BG */
		wcd9xxx_resmgr_put_clk_block(&msm8x10_wcd->resmgr,
					     WCD9XXX_CLK_MCLK);
		wcd9xxx_resmgr_put_bandgap(&msm8x10_wcd->resmgr,
					   WCD9XXX_BANDGAP_AUDIO_MODE);
	}
	WCD9XXX_BCL_UNLOCK(&msm8x10_wcd->resmgr);

	return 0;
}

static int msm8x10_wcd_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int msm8x10_wcd_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int msm8x10_wcd_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)

{
	pr_debug("%s\n", __func__);

	return 0;
}

static int msm8x10_wcd_get_channel_map(struct snd_soc_dai *dai,
				 unsigned int *tx_num, unsigned int *tx_slot,
				 unsigned int *rx_num, unsigned int *rx_slot)

{
	pr_debug("%s\n", __func__);
	return 0;
}

static int msm8x10_wcd_set_interpolator_rate(struct snd_soc_dai *dai,
	u8 rx_fs_rate_reg_val, u32 sample_rate)
{
	return 0;
}

static int msm8x10_wcd_set_decimator_rate(struct snd_soc_dai *dai,
	u8 tx_fs_rate_reg_val, u32 sample_rate)
{
	return 0;
}

static int msm8x10_wcd_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	u8 tx_fs_rate, rx_fs_rate;
	int ret;

	pr_debug("%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n", __func__,
		 dai->name, dai->id, params_rate(params),
		 params_channels(params));

	switch (params_rate(params)) {
	case 8000:
		tx_fs_rate = 0x00;
		rx_fs_rate = 0x00;
		break;
	case 16000:
		tx_fs_rate = 0x01;
		rx_fs_rate = 0x20;
		break;
	case 32000:
		tx_fs_rate = 0x02;
		rx_fs_rate = 0x40;
		break;
	case 48000:
		tx_fs_rate = 0x03;
		rx_fs_rate = 0x60;
		break;
	case 96000:
		tx_fs_rate = 0x04;
		rx_fs_rate = 0x80;
		break;
	case 192000:
		tx_fs_rate = 0x05;
		rx_fs_rate = 0xA0;
		break;
	default:
		pr_err("%s: Invalid sampling rate %d\n", __func__,
			params_rate(params));
		return -EINVAL;
	}

	switch (substream->stream) {
	case SNDRV_PCM_STREAM_CAPTURE:
		ret = msm8x10_wcd_set_decimator_rate(dai, tx_fs_rate,
					       params_rate(params));
		if (ret < 0) {
			pr_err("%s: set decimator rate failed %d\n", __func__,
				ret);
			return ret;
		}


		break;

	case SNDRV_PCM_STREAM_PLAYBACK:
		ret = msm8x10_wcd_set_interpolator_rate(dai, rx_fs_rate,
						  params_rate(params));
		if (ret < 0) {
			pr_err("%s: set decimator rate failed %d\n", __func__,
				ret);
			return ret;
		}

		break;
	default:
		pr_err("%s: Invalid stream type %d\n", __func__,
			substream->stream);
		return -EINVAL;
	}

	return 0;
}

static struct snd_soc_dai_ops msm8x10_wcd_dai_ops = {
	.startup = msm8x10_wcd_startup,
	.shutdown = msm8x10_wcd_shutdown,
	.hw_params = msm8x10_wcd_hw_params,
	.set_sysclk = msm8x10_wcd_set_dai_sysclk,
	.set_fmt = msm8x10_wcd_set_dai_fmt,
	.set_channel_map = msm8x10_wcd_set_channel_map,
	.get_channel_map = msm8x10_wcd_get_channel_map,
};

static struct snd_soc_dai_driver msm8x10_wcd_i2s_dai[] = {
	{
		.name = "msm8x10_wcd_i2s_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = MSM8X10_WCD_RATES,
			.formats = MSM8X10_WCD_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &msm8x10_wcd_dai_ops,
	},
	{
		.name = "msm8x10_wcd_i2s_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = MSM8X10_WCD_RATES,
			.formats = MSM8X10_WCD_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &msm8x10_wcd_dai_ops,
	},
};

static int msm8x10_wcd_codec_enable_ear_pa(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		pr_debug("%s: Sleeping 20ms after enabling EAR PA\n",
				 __func__);
		msleep(20);
		break;
	case SND_SOC_DAPM_POST_PMD:
		pr_debug("%s: Sleeping 20ms after disabling EAR PA\n",
				 __func__);
		msleep(20);
		break;
	}
	return 0;
}

/* Todo: Have seperate dapm widgets for I2S and Slimbus.
 * Might Need to have callbacks registered only for slimbus
 */
static const struct snd_soc_dapm_widget msm8x10_wcd_dapm_widgets[] = {
	/*RX stuff */
	SND_SOC_DAPM_OUTPUT("EAR"),

	SND_SOC_DAPM_PGA_E("EAR PA", MSM8X10_WCD_A_RX_EAR_EN, 4, 0, NULL, 0,
			msm8x10_wcd_codec_enable_ear_pa, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MIXER("DAC1", MSM8X10_WCD_A_RX_EAR_EN, 6, 0, dac1_switch,
		ARRAY_SIZE(dac1_switch)),

	SND_SOC_DAPM_AIF_IN("AIF1 PB", "AIF1 Playback", 0, SND_SOC_NOPM,
				AIF1_PB, 0),

	SND_SOC_DAPM_MUX("I2S RX1 MUX", SND_SOC_NOPM, MSM8X10_WCD_RX1, 0,
				&slim_rx_mux[MSM8X10_WCD_RX1]),
	SND_SOC_DAPM_MUX("I2S RX2 MUX", SND_SOC_NOPM, MSM8X10_WCD_RX2, 0,
				&slim_rx_mux[MSM8X10_WCD_RX2]),
	SND_SOC_DAPM_MUX("I2S RX3 MUX", SND_SOC_NOPM, MSM8X10_WCD_RX3, 0,
				&slim_rx_mux[MSM8X10_WCD_RX3]),

	SND_SOC_DAPM_MIXER("I2S RX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I2S RX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I2S RX3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I2S RX4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("I2S RX5", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Headphone */
	SND_SOC_DAPM_OUTPUT("HEADPHONE"),
	SND_SOC_DAPM_PGA_E("HPHL", MSM8X10_WCD_A_RX_HPH_CNP_EN,
		5, 0, NULL, 0,
		msm8x10_wcd_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("HPHL DAC", MSM8X10_WCD_A_RX_HPH_L_DAC_CTL,
		7, 0,
		hphl_switch, ARRAY_SIZE(hphl_switch)),

	SND_SOC_DAPM_PGA_E("HPHR", MSM8X10_WCD_A_RX_HPH_CNP_EN,
		4, 0, NULL, 0,
		msm8x10_wcd_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("HPHR DAC", NULL, MSM8X10_WCD_A_RX_HPH_R_DAC_CTL,
		7, 0,
		msm8x10_wcd_hphr_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* Speaker */
	SND_SOC_DAPM_OUTPUT("LINEOUT1"),
	SND_SOC_DAPM_OUTPUT("SPK_OUT"),

	SND_SOC_DAPM_PGA_E("LINEOUT1 PA", MSM8X10_WCD_A_RX_LINE_CNP_EN,
			0, 0, NULL, 0, msm8x10_wcd_codec_enable_lineout,
			SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("SPK PA", MSM8X10_WCD_A_SPKR_DRV_EN,
			7, 0 , NULL, 0, msm8x10_wcd_codec_enable_spk_pa,
			SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("LINEOUT1 DAC", NULL,
		MSM8X10_WCD_A_RX_LINE_1_DAC_CTL, 7, 0,
		msm8x10_wcd_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("SPK DAC", NULL, SND_SOC_NOPM, 0, 0,
			   msm8x10_wcd_spk_dac_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("RX1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX2 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER_E("RX1 MIX2",
		MSM8X10_WCD_A_CDC_CLK_RX_B1_CTL, 0, 0, NULL,
		0, msm8x10_wcd_codec_enable_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("RX2 MIX2",
		MSM8X10_WCD_A_CDC_CLK_RX_B1_CTL, 1, 0, NULL,
		0, msm8x10_wcd_codec_enable_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("RX3 MIX1",
		MSM8X10_WCD_A_CDC_CLK_RX_B1_CTL, 2, 0, NULL,
		0, msm8x10_wcd_codec_enable_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MIXER("RX1 CHAIN", MSM8X10_WCD_A_CDC_RX1_B6_CTL,
		5, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX2 CHAIN", MSM8X10_WCD_A_CDC_RX2_B6_CTL,
		5, 0, NULL, 0),

	SND_SOC_DAPM_MUX("RX1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX1 MIX1 INP3", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp3_mux),
	SND_SOC_DAPM_MUX("RX2 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX2 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX3 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX3 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX1 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx1_mix2_inp1_mux),
	SND_SOC_DAPM_MUX("RX2 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx2_mix2_inp1_mux),

	SND_SOC_DAPM_SUPPLY("CP", MSM8X10_WCD_A_CP_EN, 0, 0,
		msm8x10_wcd_codec_enable_charge_pump, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("RX_BIAS", SND_SOC_NOPM, 0, 0,
		msm8x10_wcd_codec_enable_rx_bias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	/* TX */

	SND_SOC_DAPM_SUPPLY("CDC_CONN", MSM8X10_WCD_A_CDC_CLK_OTHR_CTL,
		2, 0, NULL, 0),


	SND_SOC_DAPM_INPUT("AMIC1"),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 External",
		MSM8X10_WCD_A_MICB_1_CTL, 7, 0,
		msm8x10_wcd_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 Internal1",
		MSM8X10_WCD_A_MICB_1_CTL, 7, 0,
		msm8x10_wcd_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 Internal2",
		MSM8X10_WCD_A_MICB_1_CTL, 7, 0,
		msm8x10_wcd_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC1", NULL, MSM8X10_WCD_A_TX_1_EN, 7, 0,
		msm8x10_wcd_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("ADC1", NULL, MSM8X10_WCD_A_TX_2_EN, 7, 0,
		msm8x10_wcd_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("AMIC3"),

	SND_SOC_DAPM_MUX_E("DEC1 MUX",
		MSM8X10_WCD_A_CDC_CLK_TX_CLK_EN_B1_CTL, 0, 0,
		&dec1_mux, msm8x10_wcd_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC2 MUX",
		MSM8X10_WCD_A_CDC_CLK_TX_CLK_EN_B1_CTL, 1, 0,
		&dec2_mux, msm8x10_wcd_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("AMIC2"),
	SND_SOC_DAPM_AIF_OUT("AIF1 CAP", "AIF1 Capture", 0, SND_SOC_NOPM,
		AIF1_CAP, 0),

	SND_SOC_DAPM_MIXER("AIF1_CAP Mixer", SND_SOC_NOPM, AIF1_CAP, 0,
		aif_cap_mixer, ARRAY_SIZE(aif_cap_mixer)),

	/* Digital Mic Inputs */
	SND_SOC_DAPM_ADC_E("DMIC1", NULL, SND_SOC_NOPM, 0, 0,
		msm8x10_wcd_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC2", NULL, SND_SOC_NOPM, 0, 0,
		msm8x10_wcd_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	/* Sidetone */
	SND_SOC_DAPM_MUX("IIR1 INP1 MUX", SND_SOC_NOPM, 0, 0, &iir1_inp1_mux),
	SND_SOC_DAPM_PGA("IIR1", MSM8X10_WCD_A_CDC_CLK_SD_CTL, 0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("RX_I2S_CLK", MSM8X10_WCD_A_CDC_CLK_RX_I2S_CTL,
		4, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("TX_I2S_CLK", MSM8X10_WCD_A_CDC_CLK_TX_I2S_CTL, 4,
		0, NULL, 0),
};

static const struct msm8x10_wcd_reg_mask_val msm8x10_wcd_reg_defaults[] = {

	/* set MCLk to 9.6 */
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_CHIP_CTL, 0x0A),

	/* EAR PA deafults  */
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_RX_EAR_CMBUFF, 0x05),

	/* RX deafults */
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_CDC_RX1_B5_CTL, 0x78),
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_CDC_RX2_B5_CTL, 0x78),
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_CDC_RX3_B5_CTL, 0x78),

	/* RX1 and RX2 defaults */
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_CDC_RX1_B6_CTL, 0xA0),
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_CDC_RX2_B6_CTL, 0xA0),

	/* RX3 to RX7 defaults */
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_CDC_RX3_B6_CTL, 0x80),

	/* Reduce HPH DAC bias to 70% */
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_RX_HPH_BIAS_PA, 0x7A),
	/*Reduce EAR DAC bias to 70% */
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_RX_EAR_BIAS_PA, 0x76),
	/* Reduce LINE DAC bias to 70% */
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_RX_LINE_BIAS_PA, 0x78),


	/* Disable TX7 internal biasing path which can cause leakage */
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_TX_SUP_SWITCH_CTRL_1, 0xBF),
};

static void msm8x10_wcd_update_reg_defaults(struct snd_soc_codec *codec)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(msm8x10_wcd_reg_defaults); i++)
		snd_soc_write(codec, msm8x10_wcd_reg_defaults[i].reg,
				msm8x10_wcd_reg_defaults[i].val);
}

static const struct msm8x10_wcd_reg_mask_val
	msm8x10_wcd_codec_reg_init_val[] = {
	/* Initialize current threshold to 350MA
	 * number of wait and run cycles to 4096
	 */
	{MSM8X10_WCD_A_RX_HPH_OCP_CTL, 0xE1, 0x61},
	{MSM8X10_WCD_A_RX_COM_OCP_COUNT, 0xFF, 0xFF},

	/* Initialize gain registers to use register gain */
	{MSM8X10_WCD_A_RX_HPH_L_GAIN, 0x20, 0x20},
	{MSM8X10_WCD_A_RX_HPH_R_GAIN, 0x20, 0x20},
	{MSM8X10_WCD_A_RX_LINE_1_GAIN, 0x20, 0x20},

	/*enable HPF filter for TX paths */
	{MSM8X10_WCD_A_CDC_TX1_MUX_CTL, 0x8, 0x0},
	{MSM8X10_WCD_A_CDC_TX2_MUX_CTL, 0x8, 0x0},

	/* config Decimator for DMIC CLK_MODE_1(3.2Mhz@9.6Mhz mclk) */
	{MSM8X10_WCD_A_CDC_TX1_DMIC_CTL, 0x7, 0x1},
	{MSM8X10_WCD_A_CDC_TX2_DMIC_CTL, 0x7, 0x1},

	/* config DMIC clk to CLK_MODE_1 (3.2Mhz@9.6Mhz mclk) */
	{MSM8X10_WCD_A_CDC_CLK_DMIC_B1_CTL, 0xEE, 0x22},

};

static void msm8x10_wcd_codec_init_reg(struct snd_soc_codec *codec)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(msm8x10_wcd_codec_reg_init_val); i++)
		snd_soc_update_bits(codec,
				    msm8x10_wcd_codec_reg_init_val[i].reg,
				    msm8x10_wcd_codec_reg_init_val[i].mask,
				    msm8x10_wcd_codec_reg_init_val[i].val);
}

int msm8x10_wcd_hs_detect(struct snd_soc_codec *codec,
		    struct msm8x10_wcd_mbhc_config *mbhc_cfg)
{
	return 0;
}
EXPORT_SYMBOL_GPL(msm8x10_wcd_hs_detect);

static int msm8x10_wcd_codec_probe(struct snd_soc_codec *codec)
{
	msm8x10_wcd_codec_init_reg(codec);

	msm8x10_wcd_update_reg_defaults(codec);

	dev_dbg(codec->dev, "%s()\n", __func__);

	return 0;
}

static int msm8x10_wcd_codec_remove(struct snd_soc_codec *codec)
{
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_msm8x10_wcd = {
	.probe	= msm8x10_wcd_codec_probe,
	.remove	= msm8x10_wcd_codec_remove,

	.read = msm8x10_wcd_read,
	.write = msm8x10_wcd_write,

	.readable_register = msm8x10_wcd_readable,
	.volatile_register = msm8x10_wcd_volatile,

	.reg_cache_size = MSM8X10_WCD_CACHE_SIZE,
	.reg_cache_default = msm8x10_wcd_reset_reg_defaults,
	.reg_word_size = 1,

	.controls = msm8x10_wcd_snd_controls,
	.num_controls = ARRAY_SIZE(msm8x10_wcd_snd_controls),
	.dapm_widgets = msm8x10_wcd_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(msm8x10_wcd_dapm_widgets),
	.dapm_routes = audio_map,
	.num_dapm_routes = ARRAY_SIZE(audio_map),
};

static int __devinit msm8x10_wcd_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;
	struct msm8x10_wcd_pdata *pdata;
	if (client->dev.of_node) {
		dev_dbg(&client->dev, "%s:Platform data from device tree\n",
			__func__);
		pdata = msm8x10_wcd_populate_dt_pdata(&client->dev);
		client->dev.platform_data = pdata;
	} else {
		dev_dbg(&client->dev, "%s:Platform data from board file\n",
			__func__);
		pdata = client->dev.platform_data;
	}

	ret = snd_soc_register_codec(&client->dev,
		&soc_codec_dev_msm8x10_wcd,
		msm8x10_wcd_i2s_dai, ARRAY_SIZE(msm8x10_wcd_i2s_dai));
	dev_dbg(&client->dev, "%s:ret = 0x%x\n", __func__, ret);

	return ret;
}

static int __devexit msm8x10_wcd_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static struct i2c_device_id msm8x10_wcd_id_table[] = {
	{"msm8x10-wcd-i2c", MSM8X10_WCD_I2C_TOP_LEVEL},
	{"msm8x10-wcd-i2c", MSM8X10_WCD_I2C_ANALOG},
	{"msm8x10-wcd-i2c", MSM8X10_WCD_I2C_DIGITAL_1},
	{"msm8x10-wcd-i2c", MSM8X10_WCD_I2C_DIGITAL_2},
	{}
};
MODULE_DEVICE_TABLE(i2c, msm8x10_wcd_id_table);

static struct of_device_id msm8x10_wcd_of_match[] = {
	{ .compatible = "qcom,msm8x10-wcd-i2c",},
	{ },
};


static struct i2c_driver msm8x10_wcd_i2c_driver = {
	.driver                 = {
		.owner          = THIS_MODULE,
		.name           = "msm8x10-wcd-i2c-core",
		.of_match_table = msm8x10_wcd_of_match
	},
	.id_table               = msm8x10_wcd_id_table,
	.probe                  = msm8x10_wcd_i2c_probe,
	.remove                 = __devexit_p(msm8x10_wcd_i2c_remove),
};

static int __init msm8x10_wcd_codec_init(void)
{
	int ret;

	pr_debug("%s:\n", __func__);
	ret = i2c_add_driver(&msm8x10_wcd_i2c_driver);
	if (ret != 0)
		pr_err("%s: Failed to add msm8x10 wcd I2C driver - error code %d\n",
		       __func__, ret);
	return ret;
}

static void __exit msm8x10_wcd_codec_exit(void)
{
	i2c_del_driver(&msm8x10_wcd_i2c_driver);
}


module_init(msm8x10_wcd_codec_init);
module_exit(msm8x10_wcd_codec_exit);

MODULE_DESCRIPTION("MSM8x10 Audio codec driver");
MODULE_LICENSE("GPL v2");
