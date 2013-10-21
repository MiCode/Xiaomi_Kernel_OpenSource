/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/mfd/wcd9xxx/pdata.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <mach/qdsp6v2/apr.h>
#include <mach/subsystem_notif.h>
#include "msm8x10-wcd.h"
#include "wcd9xxx-resmgr.h"
#include "msm8x10_wcd_registers.h"
#include "../msm/qdsp6v2/q6core.h"
#include "wcd9xxx-common.h"

#define MSM8X10_WCD_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000)
#define MSM8X10_WCD_FORMATS (SNDRV_PCM_FMTBIT_S16_LE)

#define NUM_DECIMATORS		2
#define NUM_INTERPOLATORS	3
#define BITS_PER_REG		8
#define MSM8X10_WCD_TX_PORT_NUMBER	4

#define DAPM_MICBIAS_EXTERNAL_STANDALONE "MIC BIAS External Standalone"

#define MSM8X10_WCD_I2S_MASTER_MODE_MASK	0x08
#define MSM8X10_DINO_CODEC_BASE_ADDR		0xFE043000
#define MSM8X10_DINO_CODEC_REG_SIZE		0x200
#define MSM8x10_TLMM_CDC_PULL_CTL			0xFD512050
#define HELICON_CORE_0_I2C_ADDR				0x0d
#define HELICON_CORE_1_I2C_ADDR				0x77
#define HELICON_CORE_2_I2C_ADDR				0x66
#define HELICON_CORE_3_I2C_ADDR				0x55

#define MAX_MSM8X10_WCD_DEVICE	4
#define CODEC_DT_MAX_PROP_SIZE	40
#define MAX_ON_DEMAND_SUPPLY_NAME_LENGTH 64
#define HELICON_MCLK_CLK_9P6MHZ				9600000

/*
 * Multiplication factor to compute impedance on codec
 * This is computed from (Vx / (m*Ical)) = (10mV/(180*30uA))
 */
#define MSM8X10_WCD_ZDET_MUL_FACTOR 1852

/* RX_HPH_CNP_WG_TIME increases by 0.24ms */
#define MSM8X10_WCD_WG_TIME_FACTOR_US  240

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

#define MSM8X10_WCD_ACQUIRE_LOCK(x) do { \
	mutex_lock_nested(&x, SINGLE_DEPTH_NESTING); \
} while (0)
#define MSM8X10_WCD_RELEASE_LOCK(x) do { mutex_unlock(&x); } while (0)


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

enum msm8x10_wcd_bandgap_type {
	MSM8X10_WCD_BANDGAP_OFF = 0,
	MSM8X10_WCD_BANDGAP_AUDIO_MODE,
	MSM8X10_WCD_BANDGAP_MBHC_MODE,
};

enum {
	ON_DEMAND_MICBIAS = 0,
	ON_DEMAND_CP,
	ON_DEMAND_SUPPLIES_MAX,
};

/*
 * The delay list is per codec HW specification.
 * Please add delay in the list in the future instead
 * of magic number
 */
enum {
	CODEC_DELAY_1_MS = 1000,
	CODEC_DELAY_1_1_MS  = 1100,
};

struct hpf_work {
	struct msm8x10_wcd_priv *msm8x10_wcd;
	u32 decimator;
	u8 tx_hpf_cut_of_freq;
	struct delayed_work dwork;
};

static struct hpf_work tx_hpf_work[NUM_DECIMATORS];

struct on_demand_supply {
	struct regulator *supply;
	atomic_t ref;
};

static char on_demand_supply_name[][MAX_ON_DEMAND_SUPPLY_NAME_LENGTH] = {
	"cdc-vdd-mic-bias",
	"cdc-vdda-cp",
};

struct msm8x10_wcd_priv {
	struct snd_soc_codec *codec;
	u32 adc_count;
	u32 rx_bias_count;
	s32 dmic_1_2_clk_cnt;
	struct on_demand_supply on_demand_list[ON_DEMAND_SUPPLIES_MAX];
	/* resmgr module */
	struct wcd9xxx_resmgr resmgr;
	/* mbhc module */
	struct wcd9xxx_mbhc mbhc;

	struct delayed_work hs_detect_work;
	struct wcd9xxx_mbhc_config *mbhc_cfg;

	/*
	 * list used to save/restore registers at start and
	 * end of impedance measurement
	 */
	struct list_head reg_save_restore;
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

static int msm8x10_wcd_dt_parse_vreg_info(struct device *dev,
	struct msm8x10_wcd_regulator *vreg,
	const char *vreg_name, bool ondemand);
static int msm8x10_wcd_dt_parse_micbias_info(struct device *dev,
	struct wcd9xxx_micbias_setting *micbias);
static struct msm8x10_wcd_pdata *msm8x10_wcd_populate_dt_pdata(
	struct device *dev);

struct msm8x10_wcd_i2c msm8x10_wcd_modules[MAX_MSM8X10_WCD_DEVICE];

static void *adsp_state_notifier;

static struct snd_soc_codec *registered_codec;
#define ADSP_STATE_READY_TIMEOUT_MS 2000


static int get_i2c_msm8x10_wcd_device_info(u16 reg,
					   struct msm8x10_wcd_i2c **msm8x10_wcd)
{
	int rtn = 0;
	int value = ((reg & 0x0f00) >> 8) & 0x000f;
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

static int msm8x10_wcd_abh_write_device(struct msm8x10_wcd *msm8x10_wcd,
					u16 reg, u8 *value, u32 bytes)
{
	u32 temp = ((u32)(*value)) & 0x000000FF;
	u32 offset = (((u32)(reg)) ^ 0x00000400) & 0x00000FFF;

	iowrite32(temp, (msm8x10_wcd->pdino_base+offset));
	return 0;
}

static int msm8x10_wcd_abh_read_device(struct msm8x10_wcd *msm8x10_wcd,
					u16 reg, u32 bytes, u8 *value)
{
	u32 temp;
	u32 offset = (((u32)(reg)) ^ 0x00000400) & 0x00000FFF;

	temp = ioread32((msm8x10_wcd->pdino_base+offset));
	*value = (u8)temp;
	return 0;
}

static int msm8x10_wcd_i2c_write_device(u16 reg, u8 *value, u32 bytes)
{

	struct i2c_msg *msg;
	int ret;
	u8 reg_addr = 0;
	u8 data[bytes + 1];
	struct msm8x10_wcd_i2c *msm8x10_wcd = NULL;

	ret = get_i2c_msm8x10_wcd_device_info(reg, &msm8x10_wcd);
	if (ret) {
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
	struct msm8x10_wcd_i2c *msm8x10_wcd = NULL;
	u8 i = 0;

	ret = get_i2c_msm8x10_wcd_device_info(reg, &msm8x10_wcd);
	if (ret) {
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
	pr_debug("%s: reg 0x%x = 0x%x\n", __func__, reg, *dest);
	return 0;
}

int msm8x10_wcd_i2c_read(unsigned short reg, int bytes, void *dest)
{
	return msm8x10_wcd_i2c_read_device(reg, bytes, dest);
}

int msm8x10_wcd_i2c_write(unsigned short reg, int bytes, void *src)
{
	return msm8x10_wcd_i2c_write_device(reg, src, bytes);
}

static unsigned short msm8x10_wcd_mask_reg(unsigned short reg)
{
	if (reg >= 0x3C0 && reg <= 0x3DF)
		reg = reg & 0x00FF;
	return reg;
}

static int __msm8x10_wcd_reg_read(struct msm8x10_wcd *msm8x10_wcd,
				unsigned short reg)
{
	int ret = -EINVAL;
	u8 temp;

	reg = msm8x10_wcd_mask_reg(reg);

	/* check if use I2C interface for Helicon or AHB for Dino */
	mutex_lock(&msm8x10_wcd->io_lock);
	if (MSM8X10_WCD_IS_HELICON_REG(reg))
		ret = msm8x10_wcd_i2c_read(reg, 1, &temp);
	else if (MSM8X10_WCD_IS_DINO_REG(reg))
		ret = msm8x10_wcd_abh_read_device(msm8x10_wcd, reg, 1, &temp);
	mutex_unlock(&msm8x10_wcd->io_lock);

	if (ret < 0) {
		dev_err(msm8x10_wcd->dev,
				"%s: codec read failed for reg 0x%x\n",
				__func__, reg);
		return ret;
	} else {
		dev_dbg(msm8x10_wcd->dev, "Read 0x%02x from 0x%x\n",
				temp, reg);
	}

	return temp;
}

static int __msm8x10_wcd_bulk_write(struct msm8x10_wcd *msm8x10_wcd,
		unsigned short reg, int count, u8 *buf)
{
	int ret = -EINVAL;
	mutex_lock(&msm8x10_wcd->io_lock);
	if (MSM8X10_WCD_IS_HELICON_REG(reg))
		ret = msm8x10_wcd_i2c_write(reg, count, buf);
	else if (MSM8X10_WCD_IS_DINO_REG(reg))
		ret = msm8x10_wcd_abh_write_device(msm8x10_wcd, reg,
						buf, count);
	if (ret < 0)
		dev_err(msm8x10_wcd->dev,
				"%s: codec bulk write failed\n", __func__);
	mutex_unlock(&msm8x10_wcd->io_lock);
	return ret;
}

int msm8x10_wcd_bulk_write(struct wcd9xxx_core_resource *core_res,
			unsigned short reg, int count, u8 *buf)
{
	struct msm8x10_wcd *msm8x10_wcd =
				(struct msm8x10_wcd *) core_res->parent;
	return __msm8x10_wcd_bulk_write(msm8x10_wcd, reg, count, buf);
}
EXPORT_SYMBOL(msm8x10_wcd_bulk_write);

int msm8x10_wcd_reg_read(struct wcd9xxx_core_resource *core_res,
				unsigned short reg)
{
	struct msm8x10_wcd *msm8x10_wcd = core_res->parent;
	return __msm8x10_wcd_reg_read(msm8x10_wcd, reg);
}
EXPORT_SYMBOL(msm8x10_wcd_reg_read);

static int __msm8x10_wcd_bulk_read(struct msm8x10_wcd *msm8x10_wcd,
		unsigned short reg, int count, u8 *buf)
{
	int ret = -EINVAL;
	mutex_lock(&msm8x10_wcd->io_lock);
	if (MSM8X10_WCD_IS_HELICON_REG(reg))
		ret = msm8x10_wcd_i2c_read(reg, count, buf);
	else if (MSM8X10_WCD_IS_DINO_REG(reg))
		ret = msm8x10_wcd_abh_read_device(msm8x10_wcd, reg,
						count, buf);
	mutex_unlock(&msm8x10_wcd->io_lock);

	if (ret < 0)
		dev_err(msm8x10_wcd->dev,
				"%s: codec bulk read failed\n", __func__);
	return ret;
}

int msm8x10_wcd_bulk_read(struct wcd9xxx_core_resource *core_res,
			unsigned short reg, int count, u8 *buf)
{
	struct msm8x10_wcd *msm8x10_wcd =
				(struct msm8x10_wcd *) core_res->parent;
	return __msm8x10_wcd_bulk_read(msm8x10_wcd, reg, count, buf);
}
EXPORT_SYMBOL(msm8x10_wcd_bulk_read);

static int __msm8x10_wcd_reg_write(struct msm8x10_wcd *msm8x10_wcd,
			unsigned short reg, u8 val)
{
	int ret = -EINVAL;

	reg = msm8x10_wcd_mask_reg(reg);

	/* check if use I2C interface for Helicon or AHB for Dino */
	mutex_lock(&msm8x10_wcd->io_lock);
	if (MSM8X10_WCD_IS_HELICON_REG(reg))
		ret = msm8x10_wcd_i2c_write(reg, 1, &val);
	else if (MSM8X10_WCD_IS_DINO_REG(reg))
		ret = msm8x10_wcd_abh_write_device(msm8x10_wcd, reg, &val, 1);
	mutex_unlock(&msm8x10_wcd->io_lock);

	if (ret < 0)
		dev_err(msm8x10_wcd->dev,
				"%s: codec write to reg 0x%x failed\n",
				__func__, reg);
	else
		dev_dbg(msm8x10_wcd->dev,
			"%s: Write %x to R%d(0x%x)\n",
			__func__, val, reg, reg);

	return ret;
}

int msm8x10_wcd_reg_write(struct wcd9xxx_core_resource *core_res,
	unsigned short reg, u8 val)
{
	struct msm8x10_wcd *msm8x10_wcd = core_res->parent;
	return __msm8x10_wcd_reg_write(msm8x10_wcd, reg, val);
}
EXPORT_SYMBOL(msm8x10_wcd_reg_write);

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

static int msm8x10_wcd_volatile(struct snd_soc_codec *codec, unsigned int reg)
{
	/*
	 * Registers lower than 0x100 are top level registers which can be
	 * written by the Taiko core driver.
	 */
	dev_dbg(codec->dev, "%s: reg 0x%x\n", __func__, reg);

	if ((reg >= MSM8X10_WCD_A_CDC_MBHC_EN_CTL) || (reg < 0x100))
		return 1;

	/* IIR Coeff registers are not cacheable */
	if ((reg >= MSM8X10_WCD_A_CDC_IIR1_COEF_B1_CTL) &&
		(reg <= MSM8X10_WCD_A_CDC_IIR2_COEF_B2_CTL))
		return 1;

	/*
	 * Digital gain register is not cacheable so we have to write
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
	dev_dbg(codec->dev, "%s: Write from reg 0x%x\n", __func__, reg);
	if (reg == SND_SOC_NOPM)
		return 0;

	BUG_ON(reg > MSM8X10_WCD_MAX_REGISTER);

	if (!msm8x10_wcd_volatile(codec, reg)) {
		ret = snd_soc_cache_write(codec, reg, value);
		if (ret != 0)
			dev_err(codec->dev, "Cache write to %x failed: %d\n",
				reg, ret);
	}

	return __msm8x10_wcd_reg_write(codec->control_data, reg, (u8)value);
}

static unsigned int msm8x10_wcd_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	unsigned int val;
	int ret;

	dev_dbg(codec->dev, "%s: Read from reg 0x%x\n", __func__, reg);
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

	val = __msm8x10_wcd_reg_read(codec->control_data, reg);
	return val;
}


static int msm8x10_wcd_dt_parse_vreg_info(struct device *dev,
	struct msm8x10_wcd_regulator *vreg, const char *vreg_name,
	bool ondemand)
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
		dev_err(dev, "Looking up %s property in node %s failed\n",
			prop_name, dev->of_node->full_name);
		return -ENODEV;
	}

	dev_dbg(dev, "Looking up %s property in node %s\n",
		prop_name, dev->of_node->full_name);

	vreg->name = vreg_name;
	vreg->ondemand = ondemand;

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE,
		"qcom,%s-voltage", vreg_name);
	prop = of_get_property(dev->of_node, prop_name, &len);

	if (!prop || (len != (2 * sizeof(__be32)))) {
		dev_err(dev, "%s %s property\n",
			prop ? "invalid format" : "no", prop_name);
		return -EINVAL;
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
		return -EFAULT;
	}
	vreg->optimum_uA = prop_val;

	dev_info(dev, "%s: vol=[%d %d]uV, curr=[%d]uA, ond %d\n\n", vreg->name,
		 vreg->min_uV, vreg->max_uV, vreg->optimum_uA, vreg->ondemand);
	return 0;
}

static int msm8x10_wcd_dt_parse_micbias_info(struct device *dev,
	struct wcd9xxx_micbias_setting *micbias)
{
	int ret = 0;
	char prop_name[CODEC_DT_MAX_PROP_SIZE];
	u32 prop_val;

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE,
		 "qcom,cdc-micbias-ldoh-v");
	ret = of_property_read_u32(dev->of_node, prop_name,
				   &prop_val);
	if (ret) {
		dev_err(dev, "Looking up %s property in node %s failed",
			prop_name, dev->of_node->full_name);
		return -ENODEV;
	}
	micbias->ldoh_v = (u8) prop_val;

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE,
		 "qcom,cdc-micbias-cfilt-mv");
	ret = of_property_read_u32(dev->of_node, prop_name,
				   &micbias->cfilt1_mv);
	if (ret) {
		dev_err(dev, "Looking up %s property in node %s failed",
			prop_name, dev->of_node->full_name);
		return -ENODEV;
	}

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE,
		 "qcom,cdc-micbias-cfilt-sel");
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
	int ret, static_cnt, ond_cnt, idx, i;
	const char *name = NULL;
	const char *static_prop_name = "qcom,cdc-static-supplies";
	const char *ond_prop_name = "qcom,cdc-on-demand-supplies";

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "could not allocate memory for platform data\n");
		return NULL;
	}

	static_cnt = of_property_count_strings(dev->of_node, static_prop_name);
	if (IS_ERR_VALUE(static_cnt)) {
		dev_err(dev, "%s: Failed to get static supplies %d\n", __func__,
			static_cnt);
		ret = -EINVAL;
		goto err;
	}

	/* On-demand supply list is an optional property */
	ond_cnt = of_property_count_strings(dev->of_node, ond_prop_name);
	if (IS_ERR_VALUE(ond_cnt))
		ond_cnt = 0;

	BUG_ON(static_cnt <= 0 || ond_cnt < 0);
	if ((static_cnt + ond_cnt) > ARRAY_SIZE(pdata->regulator)) {
		dev_err(dev, "%s: Num of supplies %u > max supported %u\n",
			__func__, static_cnt, ARRAY_SIZE(pdata->regulator));
		ret = -EINVAL;
		goto err;
	}

	for (idx = 0; idx < static_cnt; idx++) {
		ret = of_property_read_string_index(dev->of_node,
						    static_prop_name, idx,
						    &name);
		if (ret) {
			dev_err(dev, "%s: of read string %s idx %d error %d\n",
				__func__, static_prop_name, idx, ret);
			goto err;
		}

		dev_dbg(dev, "%s: Found static cdc supply %s\n", __func__,
			name);
		ret = msm8x10_wcd_dt_parse_vreg_info(dev,
						&pdata->regulator[idx],
						name, false);
		if (ret)
			goto err;
	}

	for (i = 0; i < ond_cnt; i++, idx++) {
		ret = of_property_read_string_index(dev->of_node, ond_prop_name,
						    i, &name);
		if (ret)
			goto err;

		dev_dbg(dev, "%s: Found on-demand cdc supply %s\n", __func__,
			name);
		ret = msm8x10_wcd_dt_parse_vreg_info(dev,
						&pdata->regulator[idx],
						name, true);
		if (ret)
			goto err;
	}

	ret = msm8x10_wcd_dt_parse_micbias_info(dev, &pdata->micbias);
	if (ret)
		goto err;
	return pdata;
err:
	devm_kfree(dev, pdata);
	dev_err(dev, "%s: Failed to populate DT data ret = %d\n",
		__func__, ret);
	return NULL;
}

static int msm8x10_wcd_codec_enable_on_demand_supply(
		struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = w->codec;
	struct msm8x10_wcd_priv *msm8x10_wcd = snd_soc_codec_get_drvdata(codec);
	struct on_demand_supply *supply;

	if (w->shift >= ON_DEMAND_SUPPLIES_MAX) {
		ret = -EINVAL;
		goto out;
	}
	dev_dbg(codec->dev, "%s: supply: %s event: %d ref: %d\n",
		__func__, on_demand_supply_name[w->shift], event,
		atomic_read(&msm8x10_wcd->on_demand_list[w->shift].ref));

	supply = &msm8x10_wcd->on_demand_list[w->shift];
	WARN_ONCE(!supply->supply, "%s isn't defined\n",
		  on_demand_supply_name[w->shift]);
	if (!supply->supply)
		goto out;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (atomic_inc_return(&supply->ref) == 1)
			ret = regulator_enable(supply->supply);
		if (ret)
			dev_err(codec->dev, "%s: Failed to enable %s\n",
				__func__,
				on_demand_supply_name[w->shift]);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (atomic_read(&supply->ref) == 0) {
			dev_dbg(codec->dev, "%s: %s supply has been disabled.\n",
				 __func__, on_demand_supply_name[w->shift]);
			goto out;
		}
		if (atomic_dec_return(&supply->ref) == 0)
			ret = regulator_disable(supply->supply);
			if (ret)
				dev_err(codec->dev, "%s: Failed to disable %s\n",
					__func__,
					on_demand_supply_name[w->shift]);
		break;
	default:
		break;
	}
out:
	return ret;
}

static int msm8x10_wcd_codec_enable_charge_pump(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	dev_dbg(codec->dev, "%s: event = %d\n", __func__, event);
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
				    MSM8X10_WCD_A_CDC_CLK_OTHR_RESET_B1_CTL,
				    0x01, 0x00);
		snd_soc_update_bits(codec,
				    MSM8X10_WCD_A_CP_STATIC, 0x08, 0x00);
		break;
	default:
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
		dev_err(codec->dev, "%s: ERROR: Unsupported Ear Gain = 0x%x\n",
			__func__, ear_pa_gain);
		return -EINVAL;
	}
	dev_dbg(codec->dev, "%s: ear_pa_gain = 0x%x\n", __func__, ear_pa_gain);
	return 0;
}

static int msm8x10_wcd_pa_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
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
		(snd_soc_read(codec,
			    (MSM8X10_WCD_A_CDC_IIR1_CTL + 64 * iir_idx)) &
		(1 << band_idx)) != 0;

	dev_dbg(codec->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
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

	dev_dbg(codec->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
	  iir_idx, band_idx,
	  ((snd_soc_read(codec, (MSM8X10_WCD_A_CDC_IIR1_CTL + 64 * iir_idx)) &
	  (1 << band_idx)) != 0));

	return 0;
}
static uint32_t get_iir_band_coeff(struct snd_soc_codec *codec,
				   int iir_idx, int band_idx,
				   int coeff_idx)
{
	uint32_t value = 0;

	/* Address does not automatically update if reading */
	snd_soc_write(codec,
		(MSM8X10_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t)) & 0x7F);

	value |= snd_soc_read(codec,
		(MSM8X10_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx));

	snd_soc_write(codec,
		(MSM8X10_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 1) & 0x7F);

	value |= (snd_soc_read(codec,
		(MSM8X10_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx)) << 8);

	snd_soc_write(codec,
		(MSM8X10_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 2) & 0x7F);

	value |= (snd_soc_read(codec,
		(MSM8X10_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx)) << 16);

	snd_soc_write(codec,
		(MSM8X10_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 3) & 0x7F);

	/* Mask bits top 2 bits since they are reserved */
	value |= ((snd_soc_read(codec,
	  (MSM8X10_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx)) & 0x3f) << 24);

	return value;

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

	dev_dbg(codec->dev, "%s: IIR #%d band #%d b0 = 0x%x\n"
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
				uint32_t value)
{
	snd_soc_write(codec,
		(MSM8X10_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value & 0xFF));

	snd_soc_write(codec,
		(MSM8X10_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value >> 8) & 0xFF);

	snd_soc_write(codec,
		(MSM8X10_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value >> 16) & 0xFF);

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

	/* Mask top bit it is reserved */
	/* Updates addr automatically for each B2 write */
	snd_soc_write(codec,
		(MSM8X10_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
		(band_idx * BAND_MAX * sizeof(uint32_t)) & 0x7F);


	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[0]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[1]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[2]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[3]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[4]);

	dev_dbg(codec->dev, "%s: IIR #%d band #%d b0 = 0x%x\n"
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

	SOC_SINGLE_TLV("LINEOUT Volume", MSM8X10_WCD_A_RX_LINE_1_GAIN,
		       0, 12, 1, line_gain),

	SOC_SINGLE_TLV("HPHL Volume", MSM8X10_WCD_A_RX_HPH_L_GAIN,
		       0, 12, 1, line_gain),
	SOC_SINGLE_TLV("HPHR Volume", MSM8X10_WCD_A_RX_HPH_R_GAIN,
		       0, 12, 1, line_gain),

	SOC_SINGLE_S8_TLV("RX1 Digital Volume",
			  MSM8X10_WCD_A_CDC_RX1_VOL_CTL_B2_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX2 Digital Volume",
			  MSM8X10_WCD_A_CDC_RX2_VOL_CTL_B2_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX3 Digital Volume",
			  MSM8X10_WCD_A_CDC_RX3_VOL_CTL_B2_CTL,
			  -84, 40, digital_gain),

	SOC_SINGLE_S8_TLV("DEC1 Volume",
			  MSM8X10_WCD_A_CDC_TX1_VOL_CTL_GAIN,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("DEC2 Volume",
			  MSM8X10_WCD_A_CDC_TX2_VOL_CTL_GAIN,
			  -84, 40, digital_gain),

	SOC_SINGLE_S8_TLV("IIR1 INP1 Volume",
			  MSM8X10_WCD_A_CDC_IIR1_GAIN_B1_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP2 Volume",
			  MSM8X10_WCD_A_CDC_IIR1_GAIN_B2_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP3 Volume",
			  MSM8X10_WCD_A_CDC_IIR1_GAIN_B3_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP4 Volume",
			  MSM8X10_WCD_A_CDC_IIR1_GAIN_B4_CTL,
			  -84,	40, digital_gain),

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

static const char * const adc2_mux_text[] = {
	"ZERO", "INP2", "INP3"
};

static const char * const iir1_inp1_text[] = {
	"ZERO", "DEC1", "DEC2", "RX1", "RX2", "RX3"
};

/*
  * There is only one bit to select RX2 (0) or RX3 (1) so add 'ZERO' won't
  * cause any issue to select the right input, but it eliminates that lineout
  * is powered-up when HPH is enabled if the 'ZERO" is used in the disable
  * sequence for lineout.
  */
static const char * const rx_rdac4_text[] = {
	"ZERO", "RX3", "RX2"
};

static const char * const rx_rdac3_text[] = {
	"RX1", "RX2"
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

static const struct soc_enum rx_rdac4_enum  =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_CONN_LO_DAC_CTL, 0, 3,
	rx_rdac4_text);

static const struct soc_enum rx_rdac3_enum  =
	SOC_ENUM_SINGLE(MSM8X10_WCD_A_CDC_CONN_HPHR_DAC_CTL, 0, 2,
	rx_rdac3_text);

static const struct soc_enum adc2_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(adc2_mux_text), adc2_mux_text);

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

static const struct snd_kcontrol_new rx_dac4_mux =
	SOC_DAPM_ENUM("RDAC4 MUX Mux", rx_rdac4_enum);

static const struct snd_kcontrol_new rx_dac3_mux =
	SOC_DAPM_ENUM("RDAC3 MUX Mux", rx_rdac3_enum);

static const struct snd_kcontrol_new tx_adc2_mux =
	SOC_DAPM_ENUM("ADC2 MUX Mux", adc2_enum);

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
		dev_err(codec->dev, "%s: Invalid decimator = %s\n",
			__func__, w->name);
		ret =  -EINVAL;
		goto out;
	}

	ret = kstrtouint(strpbrk(dec_name, "12"), 10, &decimator);
	if (ret < 0) {
		dev_err(codec->dev, "%s: Invalid decimator = %s\n",
			__func__, dec_name);
		ret =  -EINVAL;
		goto out;
	}

	dev_dbg(w->dapm->dev, "%s(): widget = %s decimator = %u dec_mux = %u\n"
		, __func__, w->name, decimator, dec_mux);

	switch (decimator) {
	case 1:
	case 2:
			if ((dec_mux == 3) || (dec_mux == 4))
				adc_dmic_sel = 0x1;
			else
				adc_dmic_sel = 0x0;
		break;
	default:
		dev_err(codec->dev, "%s: Invalid Decimator = %u\n",
			__func__, decimator);
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

static const struct snd_kcontrol_new spkr_switch[] = {
	SOC_DAPM_SINGLE("Switch", MSM8X10_WCD_A_SPKR_DRV_DAC_CTL, 2, 1, 0)
};

static void msm8x10_wcd_codec_enable_adc_block(struct snd_soc_codec *codec,
					 int enable)
{
	struct msm8x10_wcd_priv *wcd8x10 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %d\n", __func__, enable);

	if (enable) {
		wcd8x10->adc_count++;
		snd_soc_update_bits(codec,
				    MSM8X10_WCD_A_CDC_ANA_CLK_CTL,
				    0x20, 0x20);
	} else
		wcd8x10->adc_count--;
}

static int msm8x10_wcd_codec_enable_adc(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 adc_reg;
	u8 init_bit_shift;

	dev_dbg(codec->dev, "%s %d\n", __func__, event);
	adc_reg = MSM8X10_WCD_A_TX_1_2_TEST_CTL;

	if (w->reg == MSM8X10_WCD_A_TX_1_EN)
		init_bit_shift = 7;
	else if ((w->reg == MSM8X10_WCD_A_TX_2_EN) ||
		 (w->reg == MSM8X10_WCD_A_TX_3_EN))
		init_bit_shift = 6;
	else {
		dev_err(codec->dev, "%s: Error, invalid adc register\n",
			__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		msm8x10_wcd_codec_enable_adc_block(codec, 1);
		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift,
				1 << init_bit_shift);
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift, 0x00);
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
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

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);

	switch (w->shift) {
	case 0:
		lineout_gain_reg = MSM8X10_WCD_A_RX_LINE_1_GAIN;
		break;
	default:
		dev_err(codec->dev,
			"%s: Error, incorrect lineout register value\n",
			__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, lineout_gain_reg, 0x40, 0x40);
		break;
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(codec->dev, "%s: sleeping 16 ms after %s PA turn on\n",
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
	dev_dbg(w->codec->dev, "%s %d %s\n", __func__, event, w->name);
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
		dev_err(codec->dev,
			"%s: Invalid DMIC line on the codec\n", __func__);
		return -EINVAL;
	}

	switch (dmic) {
	case 1:
	case 2:
		dmic_clk_en = 0x01;
		dmic_clk_cnt = &(msm8x10_wcd->dmic_1_2_clk_cnt);
		dmic_clk_reg = MSM8X10_WCD_A_CDC_CLK_DMIC_B1_CTL;
		dev_dbg(codec->dev,
			"%s() event %d DMIC%d dmic_1_2_clk_cnt %d\n",
			__func__, event,  dmic, *dmic_clk_cnt);
		break;
	default:
		dev_err(codec->dev, "%s: Invalid DMIC Selection\n", __func__);
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

static int msm8x10_wcd_codec_enable_micbias(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x10_wcd_priv *msm8x10_wcd = snd_soc_codec_get_drvdata(codec);
	u16 micb_int_reg;
	char *internal1_text = "Internal1";
	char *internal2_text = "Internal2";
	char *internal3_text = "Internal3";
	enum wcd9xxx_notify_event e_post_off, e_pre_on, e_post_on;

	dev_dbg(codec->dev, "%s %d\n", __func__, event);
	switch (w->reg) {
	case MSM8X10_WCD_A_MICB_1_CTL:
		micb_int_reg = MSM8X10_WCD_A_MICB_1_INT_RBIAS;
		e_pre_on = WCD9XXX_EVENT_PRE_MICBIAS_1_ON;
		e_post_on = WCD9XXX_EVENT_POST_MICBIAS_1_ON;
		e_post_off = WCD9XXX_EVENT_POST_MICBIAS_1_OFF;
		break;
	default:
		dev_err(codec->dev,
			"%s: Error, invalid micbias register 0x%x\n",
			__func__, w->reg);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Let MBHC module know micbias is about to turn ON */
		wcd9xxx_resmgr_notifier_call(&msm8x10_wcd->resmgr, e_pre_on);

		if (strnstr(w->name, internal1_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x80, 0x80);
		else if (strnstr(w->name, internal2_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x10, 0x10);
		else if (strnstr(w->name, internal3_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x2, 0x2);

		/* Always pull up TxFe for TX2 to Micbias */
		snd_soc_update_bits(codec, micb_int_reg, 0x04, 0x04);
		snd_soc_update_bits(codec, MSM8X10_WCD_A_MICB_1_CTL,
					0x80, 0x80);
		break;
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(20000, 20100);
		/* Let MBHC module know so micbias is on */
		wcd9xxx_resmgr_notifier_call(&msm8x10_wcd->resmgr, e_post_on);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, MSM8X10_WCD_A_MICB_1_CTL,
					0x80, 0x00);
		/* Let MBHC module know so micbias switch to be off */
		wcd9xxx_resmgr_notifier_call(&msm8x10_wcd->resmgr, e_post_off);

		if (strnstr(w->name, internal1_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x80, 0x00);
		else if (strnstr(w->name, internal2_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x10, 0x00);
		else if (strnstr(w->name, internal3_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x2, 0x0);

		/* Disable pull up TxFe for TX2 to Micbias */
		snd_soc_update_bits(codec, micb_int_reg, 0x04, 0x00);
		break;
	}
	return 0;
}

static void tx_hpf_corner_freq_callback(struct work_struct *work)
{
	struct delayed_work *hpf_delayed_work;
	struct hpf_work *hpf_work;
	struct msm8x10_wcd_priv *msm8x10_wcd;
	struct snd_soc_codec *codec;
	u16 tx_mux_ctl_reg;
	u8 hpf_cut_of_freq;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	msm8x10_wcd = hpf_work->msm8x10_wcd;
	codec = hpf_work->msm8x10_wcd->codec;
	hpf_cut_of_freq = hpf_work->tx_hpf_cut_of_freq;

	tx_mux_ctl_reg = MSM8X10_WCD_A_CDC_TX1_MUX_CTL +
			(hpf_work->decimator - 1) * 32;

	dev_info(codec->dev, "%s(): decimator %u hpf_cut_of_freq 0x%x\n",
		 __func__, hpf_work->decimator, (unsigned int)hpf_cut_of_freq);

	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30, hpf_cut_of_freq << 4);
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

	dev_dbg(codec->dev, "%s %d\n", __func__, event);

	widget_name = kstrndup(w->name, 15, GFP_KERNEL);
	if (!widget_name)
		return -ENOMEM;
	temp = widget_name;

	dec_name = strsep(&widget_name, " ");
	widget_name = temp;
	if (!dec_name) {
		dev_err(codec->dev,
			"%s: Invalid decimator = %s\n", __func__, w->name);
		ret = -EINVAL;
		goto out;
	}

	ret = kstrtouint(strpbrk(dec_name, "12"), 10, &decimator);
	if (ret < 0) {
		dev_err(codec->dev,
			"%s: Invalid decimator = %s\n", __func__, dec_name);
		ret = -EINVAL;
		goto out;
	}

	dev_dbg(codec->dev,
		"%s(): widget = %s dec_name = %s decimator = %u\n", __func__,
		w->name, dec_name, decimator);

	if (w->reg == MSM8X10_WCD_A_CDC_CLK_TX_CLK_EN_B1_CTL) {
		dec_reset_reg = MSM8X10_WCD_A_CDC_CLK_TX_RESET_B1_CTL;
		offset = 0;
	} else {
		dev_err(codec->dev, "%s: Error, incorrect dec\n", __func__);
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
						 struct snd_kcontrol *kcontrol,
						 int event)
{
	struct snd_soc_codec *codec = w->codec;

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);

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


/* The register address is the same as other codec so it can use resmgr */
static int msm8x10_wcd_codec_enable_rx_bias(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x10_wcd_priv *msm8x10_wcd = snd_soc_codec_get_drvdata(codec);
	msm8x10_wcd->resmgr.codec = codec;

	dev_dbg(codec->dev, "%s %d\n", __func__, event);

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

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

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

	dev_dbg(codec->dev, "%s: %s event = %d\n", __func__, w->name, event);
	if (w->shift == 5) {
		e_pre_on = WCD9XXX_EVENT_PRE_HPHR_PA_ON;
		e_post_off = WCD9XXX_EVENT_POST_HPHR_PA_OFF;
	} else if (w->shift == 4) {
		e_pre_on = WCD9XXX_EVENT_PRE_HPHL_PA_ON;
		e_post_off = WCD9XXX_EVENT_POST_HPHL_PA_OFF;
	} else {
		dev_err(codec->dev,
			"%s: Invalid w->shift %d\n", __func__, w->shift);
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

		dev_dbg(codec->dev,
			"%s: sleep 10 ms after %s PA disable.\n", __func__,
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

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

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

static const struct snd_soc_dapm_route audio_map[] = {
	{"RX_I2S_CLK", NULL, "CDC_CONN"},
	{"I2S RX1", NULL, "RX_I2S_CLK"},
	{"I2S RX2", NULL, "RX_I2S_CLK"},
	{"I2S RX3", NULL, "RX_I2S_CLK"},

	{"I2S TX1", NULL, "TX_I2S_CLK"},
	{"I2S TX2", NULL, "TX_I2S_CLK"},

	{"I2S TX1", NULL, "DEC1 MUX"},
	{"I2S TX2", NULL, "DEC2 MUX"},

	/* Earpiece (RX MIX1) */
	{"EAR", NULL, "EAR PA"},
	{"EAR PA", NULL, "DAC1"},
	{"DAC1", NULL, "CP"},

	/* Clocks for playback path */
	{"DAC1", NULL, "EAR CLK"},
	{"HPHL DAC", NULL, "HPHL CLK"},
	{"HPHR DAC", NULL, "HPHR CLK"},
	{"SPK DAC", NULL, "SPK CLK"},
	{"LINEOUT DAC", NULL, "LINEOUT CLK"},

	/* Headset (RX MIX1 and RX MIX2) */
	{"HEADPHONE", NULL, "HPHL"},
	{"HEADPHONE", NULL, "HPHR"},

	{"HPHL", NULL, "HPHL DAC"},

	{"HPHR", NULL, "HPHR DAC"},

	{"HPHL DAC", NULL, "CP"},

	{"HPHR DAC", NULL, "CP"},
	{"SPK DAC", NULL, "CP"},

	{"DAC1", "Switch", "RX1 CHAIN"},
	{"HPHL DAC", "Switch", "RX1 CHAIN"},
	{"HPHR DAC", NULL, "RDAC3 MUX"},

	{"RDAC3 MUX", "RX1", "RX1 CHAIN"},
	{"RDAC3 MUX", "RX2", "RX2 CHAIN"},

	{"LINEOUT", NULL, "LINEOUT PA"},
	{"SPK_OUT", NULL, "SPK PA"},

	{"LINEOUT PA", NULL, "CP"},
	{"LINEOUT PA", NULL, "LINEOUT DAC"},
	{"LINEOUT DAC", NULL, "RDAC4 MUX"},

	{"RDAC4 MUX", "RX2", "RX2 CHAIN"},
	{"RDAC4 MUX", "RX3", "RX3 CHAIN"},

	{"CP", NULL, "CP_REGULATOR"},
	{"CP", NULL, "RX_BIAS"},
	{"SPK PA", NULL, "SPK DAC"},
	{"SPK DAC", "Switch", "RX3 CHAIN"},

	{"RX1 CHAIN", NULL, "RX1 CLK"},
	{"RX2 CHAIN", NULL, "RX2 CLK"},
	{"RX3 CHAIN", NULL, "RX3 CLK"},
	{"RX1 CHAIN", NULL, "RX1 MIX2"},
	{"RX2 CHAIN", NULL, "RX2 MIX2"},
	{"RX3 CHAIN", NULL, "RX3 MIX1"},

	{"RX1 MIX1", NULL, "RX1 MIX1 INP1"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP2"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP3"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP1"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP2"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP1"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP2"},
	{"RX1 MIX2", NULL, "RX1 MIX1"},
	{"RX1 MIX2", NULL, "RX1 MIX2 INP1"},
	{"RX2 MIX2", NULL, "RX2 MIX1"},
	{"RX2 MIX2", NULL, "RX2 MIX2 INP1"},

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

	{"RX1 MIX2 INP1", "IIR1", "IIR1"},
	{"RX2 MIX2 INP1", "IIR1", "IIR1"},

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

	{"ADC2", NULL, "ADC2 MUX"},
	{"ADC2 MUX", "INP2", "ADC2_INP2"},
	{"ADC2 MUX", "INP3", "ADC2_INP3"},

	/* ADC Connections */
	{"ADC1", NULL, "AMIC1"},
	{"ADC2_INP2", NULL, "AMIC2"},
	{"ADC2_INP3", NULL, "AMIC3"},

	{"IIR1", NULL, "IIR1 INP1 MUX"},
	{"IIR1 INP1 MUX", "DEC1", "DEC1 MUX"},
	{"IIR1 INP1 MUX", "DEC2", "DEC2 MUX"},
	{"MIC BIAS Internal1", NULL, "INT_LDO_H"},
	{"MIC BIAS Internal2", NULL, "INT_LDO_H"},
	{"MIC BIAS External", NULL, "INT_LDO_H"},
	{"MIC BIAS Internal1", NULL, "MICBIAS_REGULATOR"},
	{"MIC BIAS Internal2", NULL, "MICBIAS_REGULATOR"},
	{"MIC BIAS External", NULL, "MICBIAS_REGULATOR"},
};

static int msm8x10_wcd_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	dev_dbg(dai->codec->dev, "%s(): substream = %s  stream = %d\n",
		__func__,
		substream->name, substream->stream);
	return 0;
}

static void msm8x10_wcd_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	dev_dbg(dai->codec->dev,
		"%s(): substream = %s  stream = %d\n" , __func__,
		substream->name, substream->stream);
}

int msm8x10_wcd_mclk_enable(struct snd_soc_codec *codec,
			    int mclk_enable, bool dapm)
{
	struct msm8x10_wcd_priv *msm8x10_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: mclk_enable = %u, dapm = %d\n",
		__func__, mclk_enable, dapm);

	WCD9XXX_BG_CLK_LOCK(&msm8x10_wcd->resmgr);

	if (mclk_enable) {
		wcd9xxx_resmgr_get_bandgap(&msm8x10_wcd->resmgr,
				WCD9XXX_BANDGAP_AUDIO_MODE);
		wcd9xxx_resmgr_get_clk_block(&msm8x10_wcd->resmgr,
				WCD9XXX_CLK_MCLK);
	} else {
		wcd9xxx_resmgr_put_clk_block(&msm8x10_wcd->resmgr,
					WCD9XXX_CLK_MCLK);
		wcd9xxx_resmgr_put_bandgap(&msm8x10_wcd->resmgr,
					WCD9XXX_BANDGAP_AUDIO_MODE);
	}
	WCD9XXX_BG_CLK_UNLOCK(&msm8x10_wcd->resmgr);
	return 0;
}

static int msm8x10_wcd_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int msm8x10_wcd_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int msm8x10_wcd_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)

{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int msm8x10_wcd_get_channel_map(struct snd_soc_dai *dai,
				 unsigned int *tx_num, unsigned int *tx_slot,
				 unsigned int *rx_num, unsigned int *rx_slot)

{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
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

	dev_dbg(dai->codec->dev,
		"%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n", __func__,
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
		dev_err(dai->codec->dev,
			"%s: Invalid sampling rate %d\n", __func__,
			params_rate(params));
		return -EINVAL;
	}

	switch (substream->stream) {
	case SNDRV_PCM_STREAM_CAPTURE:
		ret = msm8x10_wcd_set_decimator_rate(dai, tx_fs_rate,
					       params_rate(params));
		if (ret < 0) {
			dev_err(dai->codec->dev,
				"%s: set decimator rate failed %d\n", __func__,
				ret);
			return ret;
		}
		break;
	case SNDRV_PCM_STREAM_PLAYBACK:
		ret = msm8x10_wcd_set_interpolator_rate(dai, rx_fs_rate,
						  params_rate(params));
		if (ret < 0) {
			dev_err(dai->codec->dev,
				"%s: set decimator rate failed %d\n", __func__,
				ret);
			return ret;
		}
		break;
	default:
		dev_err(dai->codec->dev,
			"%s: Invalid stream type %d\n", __func__,
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
			.channels_max = 3,
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
		dev_dbg(w->codec->dev,
			"%s: Sleeping 20ms after enabling EAR PA\n",
			__func__);
		msleep(20);
		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(w->codec->dev,
			"%s: Sleeping 20ms after disabling EAR PA\n",
			__func__);
		msleep(20);
		break;
	}
	return 0;
}

static const struct snd_soc_dapm_widget msm8x10_wcd_dapm_widgets[] = {
	/*RX stuff */
	SND_SOC_DAPM_OUTPUT("EAR"),

	SND_SOC_DAPM_PGA_E("EAR PA", MSM8X10_WCD_A_RX_EAR_EN, 4, 0, NULL, 0,
			msm8x10_wcd_codec_enable_ear_pa,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("DAC1", MSM8X10_WCD_A_RX_EAR_EN, 6, 0, dac1_switch,
		ARRAY_SIZE(dac1_switch)),

	SND_SOC_DAPM_AIF_IN("I2S RX1", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("I2S RX2", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("I2S RX3", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_SUPPLY("INT_LDO_H", SND_SOC_NOPM, 1, 0, NULL, 0),

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

	SND_SOC_DAPM_MIXER("SPK DAC", SND_SOC_NOPM, 0, 0,
		spkr_switch, ARRAY_SIZE(spkr_switch)),

	/* Speaker */
	SND_SOC_DAPM_OUTPUT("LINEOUT"),
	SND_SOC_DAPM_OUTPUT("SPK_OUT"),

	SND_SOC_DAPM_PGA_E("LINEOUT PA", MSM8X10_WCD_A_RX_LINE_CNP_EN,
			0, 0, NULL, 0, msm8x10_wcd_codec_enable_lineout,
			SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("SPK PA", MSM8X10_WCD_A_SPKR_DRV_EN,
			7, 0 , NULL, 0, msm8x10_wcd_codec_enable_spk_pa,
			SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("LINEOUT DAC", NULL,
		MSM8X10_WCD_A_RX_LINE_1_DAC_CTL, 7, 0,
		msm8x10_wcd_lineout_dac_event,
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

	SND_SOC_DAPM_SUPPLY("RX1 CLK", MSM8X10_WCD_A_CDC_DIG_CLK_CTL,
		0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("RX2 CLK", MSM8X10_WCD_A_CDC_DIG_CLK_CTL,
		1, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("RX3 CLK", MSM8X10_WCD_A_CDC_DIG_CLK_CTL,
		2, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX1 CHAIN", MSM8X10_WCD_A_CDC_RX1_B6_CTL,
		5, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX2 CHAIN", MSM8X10_WCD_A_CDC_RX2_B6_CTL,
		5, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX3 CHAIN", MSM8X10_WCD_A_CDC_RX3_B6_CTL,
		5, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("HPHR CLK", MSM8X10_WCD_A_CDC_ANA_CLK_CTL,
		0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("HPHL CLK", MSM8X10_WCD_A_CDC_ANA_CLK_CTL,
		1, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("EAR CLK", MSM8X10_WCD_A_CDC_ANA_CLK_CTL,
		2, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("LINEOUT CLK", MSM8X10_WCD_A_CDC_ANA_CLK_CTL,
		3, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("SPK CLK", MSM8X10_WCD_A_CDC_ANA_CLK_CTL,
		4, 0, NULL, 0),

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
	SND_SOC_DAPM_MUX("RDAC4 MUX", SND_SOC_NOPM, 0, 0,
		&rx_dac4_mux),
	SND_SOC_DAPM_MUX("RDAC3 MUX", SND_SOC_NOPM, 0, 0,
		&rx_dac3_mux),

	SND_SOC_DAPM_SUPPLY("MICBIAS_REGULATOR", SND_SOC_NOPM,
		ON_DEMAND_MICBIAS, 0,
		msm8x10_wcd_codec_enable_on_demand_supply,
		SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("CP_REGULATOR", SND_SOC_NOPM,
		ON_DEMAND_CP, 0,
		msm8x10_wcd_codec_enable_on_demand_supply,
		SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("CP", MSM8X10_WCD_A_CP_EN, 0, 0,
		msm8x10_wcd_codec_enable_charge_pump, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("RX_BIAS", SND_SOC_NOPM, 0, 0,
		msm8x10_wcd_codec_enable_rx_bias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	/* TX */

	SND_SOC_DAPM_SUPPLY("CDC_CONN", MSM8X10_WCD_A_CDC_CLK_OTHR_CTL,
		2, 0, NULL, 0),


	SND_SOC_DAPM_INPUT("AMIC1"),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS Internal1",
		MSM8X10_WCD_A_MICB_1_CTL, 7, 0,
		msm8x10_wcd_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS Internal2",
		MSM8X10_WCD_A_MICB_1_CTL, 7, 0,
		msm8x10_wcd_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS Internal3",
		MSM8X10_WCD_A_MICB_1_CTL, 7, 0,
		msm8x10_wcd_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS External",
		MSM8X10_WCD_A_MICB_1_CTL, 7, 0,
		msm8x10_wcd_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E(DAPM_MICBIAS_EXTERNAL_STANDALONE,
		MSM8X10_WCD_A_MICB_1_CTL,
		7, 0, msm8x10_wcd_codec_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("ADC1", NULL, MSM8X10_WCD_A_TX_1_EN, 7, 0,
		msm8x10_wcd_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC2_INP2", NULL, MSM8X10_WCD_A_TX_2_EN, 7, 0,
		msm8x10_wcd_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC2_INP3", NULL, MSM8X10_WCD_A_TX_3_EN, 7, 0,
		msm8x10_wcd_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("ADC2", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("ADC2 MUX", SND_SOC_NOPM, 0, 0,
		&tx_adc2_mux),

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

	SND_SOC_DAPM_AIF_OUT("I2S TX1", "AIF1 Capture", 0, SND_SOC_NOPM,
		0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S TX2", "AIF1 Capture", 0, SND_SOC_NOPM,
		0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S TX3", "AIF1 Capture", 0, SND_SOC_NOPM,
		0, 0),

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
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_CHIP_CTL, 0x00),

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

	/* Disable internal biasing path which can cause leakage */
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_BIAS_CURR_CTL_2, 0x04),

	/* Enable pulldown to reduce leakage */
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_MICB_1_CTL, 0x82),
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_TX_COM_BIAS, 0xE0),
	/* Keep the same default gain settings for TX paths */
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_TX_1_EN, 0x32),
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_TX_2_EN, 0x32),
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_TX_3_EN, 0x30),

	/* ClassG fine tuning setting for 16 ohm HPH */
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_CDC_CLSG_FREQ_THRESH_B1_CTL, 0x05),
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_CDC_CLSG_FREQ_THRESH_B2_CTL, 0x0C),
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_CDC_CLSG_FREQ_THRESH_B3_CTL, 0x1A),
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_CDC_CLSG_FREQ_THRESH_B4_CTL, 0x47),
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_CDC_CLSG_GAIN_THRESH_CTL, 0x23),

	/* Always set TXD_CLK_EN bit to reduce the leakage */
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_CDC_DIG_CLK_CTL, 0x10),

	/* Always disable clock gating for MCLK to mbhc clock gate */
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_CDC_ANA_CLK_CTL, 0x20),
	MSM8X10_WCD_REG_VAL(MSM8X10_WCD_A_CDC_DIG_CLK_CTL, 0x10),
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
	{MSM8X10_WCD_A_RX_HPH_L_TEST, 0x01, 0x01},
	{MSM8X10_WCD_A_RX_HPH_R_TEST, 0x01, 0x01},

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

	/* Disable REF_EN for MSM8X10_WCD_A_SPKR_DRV_DAC_CTL */
	{MSM8X10_WCD_A_SPKR_DRV_DAC_CTL, 0x04, 0x00},
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

static void msm8x10_wcd_enable_mux_bias_block(
		struct snd_soc_codec *codec)
{
	snd_soc_update_bits(codec, WCD9XXX_A_MBHC_SCALING_MUX_1,
				0x80, 0x00);
}

static void msm8x10_wcd_put_cfilt_fast_mode(
	struct snd_soc_codec *codec,
	struct wcd9xxx_mbhc *mbhc)
{
	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.cfilt_ctl,
			0x30, 0x30);
}

static void msm8x10_wcd_codec_specific_cal_setup(
	struct snd_soc_codec *codec,
	struct wcd9xxx_mbhc *mbhc)
{
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL,
			0x04, 0x04);
	snd_soc_update_bits(codec, WCD9XXX_A_TX_7_MBHC_EN,
			0xE0, 0xE0);
}

static struct wcd9xxx_cfilt_mode msm8x10_wcd_switch_cfilt_mode(
	struct wcd9xxx_mbhc *mbhc, bool fast)
{
	struct snd_soc_codec *codec = mbhc->codec;
	struct wcd9xxx_cfilt_mode cfilt_mode;

	if (fast)
		cfilt_mode.reg_mode_val = WCD9XXX_CFILT_EXT_PRCHG_EN;
	else
		cfilt_mode.reg_mode_val = WCD9XXX_CFILT_EXT_PRCHG_DSBL;

	cfilt_mode.cur_mode_val =
		snd_soc_read(codec, mbhc->mbhc_bias_regs.cfilt_ctl) & 0x30;
	cfilt_mode.reg_mask = 0x30;
	return cfilt_mode;
}

static void msm8x10_wcd_select_cfilt(struct snd_soc_codec *codec,
	struct wcd9xxx_mbhc *mbhc)
{
	snd_soc_update_bits(codec,
			mbhc->mbhc_bias_regs.ctl_reg, 0x60, 0x00);
}

enum wcd9xxx_cdc_type msm8x10_wcd_get_cdc_type(void)
{
	return WCD9XXX_CDC_TYPE_HELICON;
}

static void msm8x10_wcd_mbhc_clk_gate(struct snd_soc_codec *codec,
		bool on)
{
	snd_soc_update_bits(codec, MSM8X10_WCD_A_CDC_TOP_CLK_CTL, 0x10, 0x10);
}

static void msm8x10_wcd_mbhc_txfe(struct snd_soc_codec *codec, bool on)
{
	snd_soc_update_bits(codec, MSM8X10_WCD_A_TX_7_MBHC_EN_ATEST_CTRL,
			    0x80, on ? 0x80 : 0x00);
}

static int msm8x10_wcd_enable_ext_mb_source(struct snd_soc_codec *codec,
	bool turn_on)
{
	int ret = 0;

	if (turn_on)
		ret = snd_soc_dapm_force_enable_pin(&codec->dapm,
				"MICBIAS_REGULATOR");
	else
		ret = snd_soc_dapm_disable_pin(&codec->dapm,
				"MICBIAS_REGULATOR");

	snd_soc_dapm_sync(&codec->dapm);

	if (ret)
		dev_err(codec->dev, "%s: Failed to %s external micbias source\n",
			__func__, turn_on ? "enable" : "disabled");
	else
		dev_dbg(codec->dev, "%s: %s external micbias source\n",
			 __func__, turn_on ? "Enabled" : "Disabled");

	return ret;
}

static int msm8x10_wcd_enable_mbhc_micbias(struct snd_soc_codec *codec,
					   bool enable,
					   enum wcd9xxx_micbias_num micb_num)
{
	int rc;

	if (micb_num != MBHC_MICBIAS1) {
		rc = -EINVAL;
		goto err;
	}

	if (enable)
		rc = snd_soc_dapm_force_enable_pin(&codec->dapm,
			DAPM_MICBIAS_EXTERNAL_STANDALONE);
	else
		rc = snd_soc_dapm_disable_pin(&codec->dapm,
			DAPM_MICBIAS_EXTERNAL_STANDALONE);
	snd_soc_dapm_sync(&codec->dapm);

	snd_soc_update_bits(codec, WCD9XXX_A_MICB_1_CTL,
		0x80, enable ? 0x80 : 0x00);
err:
	if (rc)
		pr_debug("%s: Failed to force %s micbias", __func__,
			enable ? "enable" : "disable");
	else
		pr_debug("%s: Trying force %s micbias", __func__,
			enable ? "enable" : "disable");
	return rc;
}

static void msm8x10_wcd_micb_internal(struct snd_soc_codec *codec, bool on)
{
	snd_soc_update_bits(codec, MSM8X10_WCD_A_MICB_1_INT_RBIAS,
			    0x1C, on ? 0x14 : 0x00);
}

static void msm8x10_wcd_enable_mb_vddio(struct snd_soc_codec *codec, bool on)
{
	snd_soc_update_bits(codec, MSM8X10_WCD_A_MICB_CFILT_1_CTL,
			    0x40, on ? 0x40 : 0x00);
}

static void msm8x10_wcd_prepare_hph_pa(struct snd_soc_codec *codec,
				       struct list_head *lh)
{
	int i;
	u32 delay;

	const struct wcd9xxx_reg_mask_val reg_set_paon[] = {
		{MSM8X10_WCD_A_CDC_RX1_B6_CTL, 0xFF, 0x01},
		{MSM8X10_WCD_A_CDC_RX2_B6_CTL, 0xFF, 0x01},
		{MSM8X10_WCD_A_RX_HPH_L_GAIN, 0xFF, 0x2C},
		{MSM8X10_WCD_A_RX_HPH_R_GAIN, 0xFF, 0x2C},
		{MSM8X10_WCD_A_CDC_CLK_RX_B1_CTL, 0xFF, 0x01},
		{MSM8X10_WCD_A_RX_COM_BIAS, 0xFF, 0x80},
		{MSM8X10_WCD_A_CP_EN, 0xFF, 0xE7},
		{MSM8X10_WCD_A_CP_STATIC, 0xFF, 0x13},
		{MSM8X10_WCD_A_CP_STATIC, 0xFF, 0x1B},
		{MSM8X10_WCD_A_CDC_RX2_B6_CTL, 0xFF, 0x01},
		{MSM8X10_WCD_A_CDC_CLK_RX_B1_CTL, 0xFF, 0x03},
		{MSM8X10_WCD_A_CDC_ANA_CLK_CTL, 0xFF, 0x22},
		{MSM8X10_WCD_A_CDC_ANA_CLK_CTL, 0xFF, 0x23},
		{MSM8X10_WCD_A_RX_HPH_CNP_WG_CTL, 0xFF, 0xDA},
		{MSM8X10_WCD_A_CDC_DIG_CLK_CTL, 0xFF, 0x01},
		{MSM8X10_WCD_A_CDC_DIG_CLK_CTL, 0xFF, 0x03},
		{MSM8X10_WCD_A_RX_HPH_CHOP_CTL, 0xFF, 0xA4},
		{MSM8X10_WCD_A_RX_HPH_OCP_CTL, 0xFF, 0x67},
		{MSM8X10_WCD_A_RX_HPH_L_TEST, 0x01, 0x00},
		{MSM8X10_WCD_A_RX_HPH_R_TEST, 0x01, 0x00},
		{MSM8X10_WCD_A_RX_HPH_BIAS_WG_OCP, 0xFF, 0x1A},
		{MSM8X10_WCD_A_RX_HPH_CNP_WG_CTL, 0xFF, 0xDB},
		{MSM8X10_WCD_A_RX_HPH_CNP_WG_TIME, 0xFF, 0xDB},
		{MSM8X10_WCD_A_RX_HPH_L_DAC_CTL, 0xFF, 0x40},
		{MSM8X10_WCD_A_RX_HPH_L_DAC_CTL, 0xFF, 0xC0},
		{MSM8X10_WCD_A_RX_HPH_R_DAC_CTL, 0xFF, 0x40},
		{MSM8X10_WCD_A_RX_HPH_R_DAC_CTL, 0xFF, 0xC0},
		{MSM8X10_WCD_A_RX_HPH_L_DAC_CTL, 0x03, 0x01},
		{MSM8X10_WCD_A_RX_HPH_R_DAC_CTL, 0x03, 0x01},
	};

	for (i = 0; i < ARRAY_SIZE(reg_set_paon); i++) {
		delay = 0;
		wcd9xxx_soc_update_bits_push(codec, lh,
					     reg_set_paon[i].reg,
					     reg_set_paon[i].mask,
					     reg_set_paon[i].val, delay);
	}
	dev_dbg(codec->dev, "%s: PAs are prepared\n", __func__);
	return;
}

static int msm8x10_wcd_enable_static_pa(struct snd_soc_codec *codec,
					bool enable)
{
	int wg_time = snd_soc_read(codec, WCD9XXX_A_RX_HPH_CNP_WG_TIME) *
				MSM8X10_WCD_WG_TIME_FACTOR_US;

	wg_time += (int) (wg_time * 35) / 100;

	snd_soc_update_bits(codec, MSM8X10_WCD_A_RX_HPH_CNP_EN, 0x30,
			    enable ? 0x30 : 0x0);
	/* Wait for wave gen time to avoid pop noise */
	usleep_range(wg_time, wg_time + WCD9XXX_USLEEP_RANGE_MARGIN_US);
	snd_soc_update_bits(codec, MSM8X10_WCD_A_CDC_RX1_B6_CTL, 0xFF, 0x00);
	snd_soc_update_bits(codec, MSM8X10_WCD_A_CDC_RX2_B6_CTL, 0xFF, 0x00);

	dev_dbg(codec->dev, "%s: PAs are %s as static mode (wg_time %d)\n",
		__func__, enable ? "enabled" : "disabled", wg_time);
	return 0;
}

static int msm8x10_wcd_setup_zdet(struct wcd9xxx_mbhc *mbhc,
				  enum mbhc_impedance_detect_stages stage)
{
	int ret = 0;
	struct snd_soc_codec *codec = mbhc->codec;
	struct msm8x10_wcd_priv *wcd_priv = snd_soc_codec_get_drvdata(codec);
	const int mux_wait_us = 25;

#define __wr(reg, mask, value)					\
	do {							\
		ret = wcd9xxx_soc_update_bits_push(codec,	\
				&wcd_priv->reg_save_restore,	\
				reg, mask, value, 0);		\
		if (ret < 0)					\
			return ret;				\
	 } while (0)

	switch (stage) {
	case PRE_MEAS:
		dev_dbg(codec->dev, "%s: PRE_MEAS\n", __func__);
		INIT_LIST_HEAD(&wcd_priv->reg_save_restore);
		/* Configure PA */
		msm8x10_wcd_prepare_hph_pa(mbhc->codec,
					   &wcd_priv->reg_save_restore);

		/* Setup MBHC */
		__wr(WCD9XXX_A_MBHC_SCALING_MUX_1, 0x7F, 0x40);
		__wr(WCD9XXX_A_MBHC_SCALING_MUX_2, 0xFF, 0xF0);
		__wr(0x171, 0xFF, 0x90);
		__wr(WCD9XXX_A_TX_7_MBHC_EN, 0xFF, 0xF0);
		__wr(WCD9XXX_A_CDC_MBHC_TIMER_B4_CTL, 0xFF, 0x45);
		__wr(WCD9XXX_A_CDC_MBHC_TIMER_B5_CTL, 0xFF, 0x80);

		__wr(WCD9XXX_A_CDC_MBHC_CLK_CTL, 0xFF, 0x0A);
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_EN_CTL, 0x2);
		__wr(WCD9XXX_A_CDC_MBHC_CLK_CTL, 0xFF, 0x02);

		/* Enable Impedance Detection */
		__wr(WCD9XXX_A_MBHC_HPH, 0xFF, 0xC8);

		/*
		 * CnP setup for 0mV
		 * Route static data as input to noise shaper
		 */
		__wr(MSM8X10_WCD_A_CDC_RX1_B3_CTL, 0xFF, 0x02);
		__wr(MSM8X10_WCD_A_CDC_RX2_B3_CTL, 0xFF, 0x02);

		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_L_TEST,
				    0x02, 0x00);
		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_R_TEST,
				    0x02, 0x00);

		/* Reset the HPHL static data pointer */
		__wr(MSM8X10_WCD_A_CDC_RX1_B2_CTL, 0xFF, 0x00);
		/* Four consecutive writes to set 0V as static data input */
		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX1_B1_CTL, 0x00);
		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX1_B1_CTL, 0x00);
		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX1_B1_CTL, 0x00);
		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX1_B1_CTL, 0x00);

		/* Reset the HPHR static data pointer */
		__wr(MSM8X10_WCD_A_CDC_RX2_B2_CTL, 0xFF, 0x00);
		/* Four consecutive writes to set 0V as static data input */
		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX2_B1_CTL, 0x00);
		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX2_B1_CTL, 0x00);
		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX2_B1_CTL, 0x00);
		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX2_B1_CTL, 0x00);

		/* Enable the HPHL and HPHR PA */
		msm8x10_wcd_enable_static_pa(mbhc->codec, true);
		break;

	case POST_MEAS:
		dev_dbg(codec->dev, "%s: POST_MEAS\n", __func__);
		/* Turn off ICAL */
		snd_soc_write(codec, WCD9XXX_A_MBHC_SCALING_MUX_2, 0xF0);

		msm8x10_wcd_enable_static_pa(mbhc->codec, false);

		/*
		 * Setup CnP wavegen to ramp to the desired
		 * output using a 40ms ramp
		 */

		/* CnP wavegen current to 0.5uA */
		snd_soc_write(codec, WCD9XXX_A_RX_HPH_BIAS_WG_OCP, 0x1A);
		/* Set the current division ratio to 2000 */
		snd_soc_write(codec, WCD9XXX_A_RX_HPH_CNP_WG_CTL, 0xDF);
		/* Set the wavegen timer to max (60msec) */
		snd_soc_write(codec, WCD9XXX_A_RX_HPH_CNP_WG_TIME, 0xA0);
		/* Set the CnP reference current to sc_bias */
		snd_soc_write(codec, WCD9XXX_A_RX_HPH_OCP_CTL, 0x6D);

		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX1_B2_CTL, 0x00);
		/* Four consecutive writes to set -10mV as static data input */
		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX1_B1_CTL, 0x00);
		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX1_B1_CTL, 0x1F);
		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX1_B1_CTL, 0xE3);
		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX1_B1_CTL, 0x08);

		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX2_B2_CTL, 0x00);
		/* Four consecutive writes to set -10mV as static data input */
		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX2_B1_CTL, 0x00);
		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX2_B1_CTL, 0x1F);
		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX2_B1_CTL, 0xE3);
		snd_soc_write(codec, MSM8X10_WCD_A_CDC_RX2_B1_CTL, 0x08);

		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_L_TEST,
				    0x02, 0x02);
		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_R_TEST,
				    0x02, 0x02);
		/* Enable the HPHL and HPHR PA and wait for 60mS */
		msm8x10_wcd_enable_static_pa(mbhc->codec, true);

		snd_soc_update_bits(codec, WCD9XXX_A_MBHC_SCALING_MUX_1,
				    0x7F, 0x40);
		usleep_range(mux_wait_us,
				mux_wait_us + WCD9XXX_USLEEP_RANGE_MARGIN_US);
		break;
	case PA_DISABLE:
		dev_dbg(codec->dev, "%s: PA_DISABLE\n", __func__);
		msm8x10_wcd_enable_static_pa(mbhc->codec, false);
		wcd9xxx_restore_registers(codec, &wcd_priv->reg_save_restore);
		break;
	}
#undef __wr

	return ret;
}

static void msm8x10_wcd_compute_impedance(s16 *l, s16 *r, uint32_t *zl,
					  uint32_t *zr)
{
	int zln, zld;
	int zrn, zrd;
	int rl = 0, rr = 0;

	zln = (l[1] - l[0]) * MSM8X10_WCD_ZDET_MUL_FACTOR;
	zld = (l[2] - l[0]);
	if (zld)
		rl = zln / zld;

	zrn = (r[1] - r[0]) * MSM8X10_WCD_ZDET_MUL_FACTOR;
	zrd = (r[2] - r[0]);
	if (zrd)
		rr = zrn / zrd;

	*zl = rl;
	*zr = rr;
}



static const struct wcd9xxx_mbhc_cb mbhc_cb = {
	.enable_mux_bias_block = msm8x10_wcd_enable_mux_bias_block,
	.cfilt_fast_mode = msm8x10_wcd_put_cfilt_fast_mode,
	.codec_specific_cal = msm8x10_wcd_codec_specific_cal_setup,
	.switch_cfilt_mode = msm8x10_wcd_switch_cfilt_mode,
	.select_cfilt = msm8x10_wcd_select_cfilt,
	.get_cdc_type = msm8x10_wcd_get_cdc_type,
	.enable_clock_gate = msm8x10_wcd_mbhc_clk_gate,
	.enable_mbhc_txfe = msm8x10_wcd_mbhc_txfe,
	.enable_mb_source = msm8x10_wcd_enable_ext_mb_source,
	.setup_int_rbias = msm8x10_wcd_micb_internal,
	.pull_mb_to_vddio = msm8x10_wcd_enable_mb_vddio,
	.setup_zdet = msm8x10_wcd_setup_zdet,
	.compute_impedance = msm8x10_wcd_compute_impedance,
};

static void delayed_hs_detect_fn(struct work_struct *work)
{
	struct delayed_work *delayed_work;
	struct msm8x10_wcd_priv *wcd_priv;

	delayed_work = to_delayed_work(work);
	wcd_priv = container_of(delayed_work, struct msm8x10_wcd_priv,
				hs_detect_work);

	if (!wcd_priv) {
		pr_err("%s: Invalid private data for codec\n", __func__);
		return;
	}

	wcd9xxx_mbhc_start(&wcd_priv->mbhc, wcd_priv->mbhc_cfg);
}


int msm8x10_wcd_hs_detect(struct snd_soc_codec *codec,
		    struct wcd9xxx_mbhc_config *mbhc_cfg)
{
	struct msm8x10_wcd_priv *wcd = snd_soc_codec_get_drvdata(codec);

	wcd->mbhc_cfg = mbhc_cfg;
	schedule_delayed_work(&wcd->hs_detect_work,
			msecs_to_jiffies(5000));
	return 0;
}
EXPORT_SYMBOL_GPL(msm8x10_wcd_hs_detect);

static int msm8x10_wcd_bringup(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, MSM8X10_WCD_A_CDC_RST_CTL, 0x02);
	snd_soc_write(codec, MSM8X10_WCD_A_CHIP_CTL, 0x00);
	usleep_range(5000, 5000);
	snd_soc_write(codec, MSM8X10_WCD_A_CDC_RST_CTL, 0x03);
	return 0;
}

static struct regulator *wcd8x10_wcd_codec_find_regulator(
				const struct msm8x10_wcd *msm8x10,
				const char *name)
{
	int i;

	for (i = 0; i < msm8x10->num_of_supplies; i++) {
		if (msm8x10->supplies[i].supply &&
		    !strncmp(msm8x10->supplies[i].supply, name, strlen(name)))
			return msm8x10->supplies[i].consumer;
	}

	return NULL;
}
static int msm8x10_wcd_device_down(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s: device down!\n", __func__);

	snd_soc_card_change_online_state(codec->card, 0);
	return 0;
}

static int msm8x10_wcd_device_up(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s: device up!\n", __func__);

	snd_soc_card_change_online_state(codec->card, 1);
	/* delay is required to make sure sound card state updated */
	usleep_range(5000, 5100);

	mutex_lock(&codec->mutex);

	msm8x10_wcd_bringup(codec);
	msm8x10_wcd_codec_init_reg(codec);
	msm8x10_wcd_update_reg_defaults(codec);

	mutex_unlock(&codec->mutex);

	return 0;
}

static int adsp_state_callback(struct notifier_block *nb, unsigned long value,
			       void *priv)
{
	bool timedout;
	unsigned long timeout;

	if (value == SUBSYS_BEFORE_SHUTDOWN)
		msm8x10_wcd_device_down(registered_codec);
	else if (value == SUBSYS_AFTER_POWERUP) {
		pr_debug("%s: ADSP is about to power up. bring up codec\n",
			 __func__);

		timeout = jiffies +
			  msecs_to_jiffies(ADSP_STATE_READY_TIMEOUT_MS);
		while (!(timedout = time_after(jiffies, timeout))) {
			if (!q6core_is_adsp_ready()) {
				pr_debug("%s: ADSP isn't ready\n", __func__);
			} else {
				pr_debug("%s: ADSP is ready\n", __func__);
				msm8x10_wcd_device_up(registered_codec);
				break;
			}
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block adsp_state_notifier_block = {
	.notifier_call = adsp_state_callback,
	.priority = -INT_MAX,
};

static const struct wcd9xxx_mbhc_intr cdc_intr_ids = {
	.poll_plug_rem = MSM8X10_WCD_IRQ_MBHC_REMOVAL,
	.shortavg_complete = MSM8X10_WCD_IRQ_MBHC_SHORT_TERM,
	.potential_button_press = MSM8X10_WCD_IRQ_MBHC_PRESS,
	.button_release = MSM8X10_WCD_IRQ_MBHC_RELEASE,
	.dce_est_complete = MSM8X10_WCD_IRQ_MBHC_POTENTIAL,
	.insertion = MSM8X10_WCD_IRQ_MBHC_INSERTION,
	.hph_left_ocp = MSM8X10_WCD_IRQ_HPH_PA_OCPL_FAULT,
	.hph_right_ocp = MSM8X10_WCD_IRQ_HPH_PA_OCPR_FAULT,
	.hs_jack_switch = MSM8X10_WCD_IRQ_MBHC_HS_DET,
};

static int msm8x10_wcd_handle_pdata(struct snd_soc_codec *codec,
	struct msm8x10_wcd_pdata *pdata)
{
	int k1, rc = 0;
	struct msm8x10_wcd_priv *msm8x10_wcd_priv;

	msm8x10_wcd_priv = snd_soc_codec_get_drvdata(codec);

	/* Make sure settings are correct */
	if (pdata->micbias.ldoh_v > WCD9XXX_LDOH_3P0_V ||
	    pdata->micbias.bias1_cfilt_sel > WCD9XXX_CFILT1_SEL) {
		rc = -EINVAL;
		goto done;
	}

	/* figure out k value */
	k1 = wcd9xxx_resmgr_get_k_val(&msm8x10_wcd_priv->resmgr,
				 pdata->micbias.cfilt1_mv);
	if (IS_ERR_VALUE(k1)) {
		rc = -EINVAL;
		goto done;
	}

	/* Set voltage level */
	snd_soc_update_bits(codec, MSM8X10_WCD_A_MICB_CFILT_1_VAL,
			    0xFC, (k1 << 2));

	/* update micbias capless mode */
	snd_soc_update_bits(codec, MSM8X10_WCD_A_MICB_1_CTL, 0x10,
			    pdata->micbias.bias1_cap_mode << 4);

done:
	return rc;
}

static int msm8x10_wcd_codec_probe(struct snd_soc_codec *codec)
{
	struct msm8x10_wcd_priv *msm8x10_wcd_priv;
	struct msm8x10_wcd *msm8x10_wcd;
	struct wcd9xxx_core_resource *core_res;
	int i, ret = 0;
	struct msm8x10_wcd_pdata *pdata;

	dev_dbg(codec->dev, "%s()\n", __func__);

	msm8x10_wcd_priv = devm_kzalloc(codec->dev,
			sizeof(struct msm8x10_wcd_priv), GFP_KERNEL);

	if (!msm8x10_wcd_priv) {
		dev_err(codec->dev, "Failed to allocate private data\n");
		return -ENOMEM;
	}

	for (i = 0 ; i < NUM_DECIMATORS; i++) {
		tx_hpf_work[i].msm8x10_wcd = msm8x10_wcd_priv;
		tx_hpf_work[i].decimator = i + 1;
		INIT_DELAYED_WORK(&tx_hpf_work[i].dwork,
			tx_hpf_corner_freq_callback);
	}

	codec->control_data = dev_get_drvdata(codec->dev);
	snd_soc_codec_set_drvdata(codec, msm8x10_wcd_priv);
	msm8x10_wcd_priv->codec = codec;

	/* map digital codec registers once */
	msm8x10_wcd = codec->control_data;
	msm8x10_wcd->pdino_base = ioremap(MSM8X10_DINO_CODEC_BASE_ADDR,
					  MSM8X10_DINO_CODEC_REG_SIZE);
	INIT_DELAYED_WORK(&msm8x10_wcd_priv->hs_detect_work,
			delayed_hs_detect_fn);

	pdata = dev_get_platdata(msm8x10_wcd->dev);
	if (!pdata) {
		dev_err(msm8x10_wcd->dev, "%s: platform data not found\n",
			__func__);
	}

	/* codec resmgr module init */
	msm8x10_wcd = codec->control_data;
	core_res = &msm8x10_wcd->wcd9xxx_res;
	ret = wcd9xxx_resmgr_init(&msm8x10_wcd_priv->resmgr,
				codec, core_res, NULL, &pdata->micbias,
				NULL, WCD9XXX_CDC_TYPE_HELICON);
	if (ret) {
		dev_err(codec->dev,
				"%s: wcd9xxx init failed %d\n",
				__func__, ret);
		goto exit_probe;
	}

	msm8x10_wcd_bringup(codec);
	msm8x10_wcd_codec_init_reg(codec);
	msm8x10_wcd_update_reg_defaults(codec);

	msm8x10_wcd_priv->on_demand_list[ON_DEMAND_CP].supply =
				wcd8x10_wcd_codec_find_regulator(
				codec->control_data,
				on_demand_supply_name[ON_DEMAND_CP]);
	atomic_set(&msm8x10_wcd_priv->on_demand_list[ON_DEMAND_CP].ref, 0);
	msm8x10_wcd_priv->on_demand_list[ON_DEMAND_MICBIAS].supply =
				wcd8x10_wcd_codec_find_regulator(
				codec->control_data,
				on_demand_supply_name[ON_DEMAND_MICBIAS]);
	atomic_set(&msm8x10_wcd_priv->on_demand_list[ON_DEMAND_MICBIAS].ref, 0);

	ret = wcd9xxx_mbhc_init(&msm8x10_wcd_priv->mbhc,
				&msm8x10_wcd_priv->resmgr,
				codec, msm8x10_wcd_enable_mbhc_micbias,
				&mbhc_cb, &cdc_intr_ids,
				HELICON_MCLK_CLK_9P6MHZ, true);
	if (ret) {
		dev_err(msm8x10_wcd->dev, "%s: Failed to initialize mbhc\n",
			__func__);
		goto exit_probe;
	}

	/* Handle the Pdata */
	ret = msm8x10_wcd_handle_pdata(codec, pdata);
	if (IS_ERR_VALUE(ret))
		dev_err(msm8x10_wcd->dev, "%s: Bad Pdata\n", __func__);

	registered_codec = codec;
	adsp_state_notifier =
	    subsys_notif_register_notifier("adsp",
					   &adsp_state_notifier_block);
	if (!adsp_state_notifier) {
		pr_err("%s: Failed to register adsp state notifier\n",
		       __func__);
		registered_codec = NULL;
		return -ENOMEM;
	}
	return 0;

exit_probe:
	return ret;

}

static int msm8x10_wcd_codec_remove(struct snd_soc_codec *codec)
{
	struct msm8x10_wcd_priv *pwcd_priv = snd_soc_codec_get_drvdata(codec);
	struct msm8x10_wcd *msm8x10_wcd = pwcd_priv->codec->control_data;
	pwcd_priv->on_demand_list[ON_DEMAND_CP].supply = NULL;
	atomic_set(&pwcd_priv->on_demand_list[ON_DEMAND_CP].ref, 0);
	pwcd_priv->on_demand_list[ON_DEMAND_MICBIAS].supply = NULL;
	atomic_set(&pwcd_priv->on_demand_list[ON_DEMAND_MICBIAS].ref, 0);

	/* cleanup resmgr */
	wcd9xxx_resmgr_deinit(&pwcd_priv->resmgr);

	iounmap(msm8x10_wcd->pdino_base);
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

static int msm8x10_wcd_init_supplies(struct msm8x10_wcd *msm8x10,
				struct msm8x10_wcd_pdata *pdata)
{
	int ret;
	int i;
	msm8x10->supplies = kzalloc(sizeof(struct regulator_bulk_data) *
				   ARRAY_SIZE(pdata->regulator),
				   GFP_KERNEL);
	if (!msm8x10->supplies) {
		ret = -ENOMEM;
		goto err;
	}

	msm8x10->num_of_supplies = 0;

	if (ARRAY_SIZE(pdata->regulator) > MAX_REGULATOR) {
		dev_err(msm8x10->dev, "%s: Array Size out of bound\n",
			__func__);
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(pdata->regulator); i++) {
		if (pdata->regulator[i].name) {
			msm8x10->supplies[i].supply = pdata->regulator[i].name;
			msm8x10->num_of_supplies++;
		}
	}

	ret = regulator_bulk_get(msm8x10->dev, msm8x10->num_of_supplies,
				 msm8x10->supplies);
	if (ret != 0) {
		dev_err(msm8x10->dev, "Failed to get supplies: err = %d\n",
							ret);
		goto err_supplies;
	}

	for (i = 0; i < msm8x10->num_of_supplies; i++) {
		if (regulator_count_voltages(msm8x10->supplies[i].consumer) <=
			0)
			continue;

		ret = regulator_set_voltage(msm8x10->supplies[i].consumer,
			pdata->regulator[i].min_uV,
			pdata->regulator[i].max_uV);
		if (ret) {
			dev_err(msm8x10->dev, "%s: Setting regulator voltage failed for regulator %s err = %d\n",
				__func__, msm8x10->supplies[i].supply, ret);
			goto err_get;
		}

		ret = regulator_set_optimum_mode(msm8x10->supplies[i].consumer,
			pdata->regulator[i].optimum_uA);
		if (ret < 0) {
			dev_err(msm8x10->dev, "%s: Setting regulator optimum mode failed for regulator %s err = %d\n",
				__func__, msm8x10->supplies[i].supply, ret);
			goto err_get;
		} else {
			ret = 0;
		}
	}

	return ret;

err_get:
	regulator_bulk_free(msm8x10->num_of_supplies, msm8x10->supplies);
err_supplies:
	kfree(msm8x10->supplies);
err:
	return ret;
}

static int msm8x10_wcd_enable_static_supplies(struct msm8x10_wcd *msm8x10,
					  struct msm8x10_wcd_pdata *pdata)
{
	int i;
	int ret = 0;

	for (i = 0; i < msm8x10->num_of_supplies; i++) {
		if (pdata->regulator[i].ondemand)
			continue;
		ret = regulator_enable(msm8x10->supplies[i].consumer);
		if (ret) {
			pr_err("%s: Failed to enable %s\n", __func__,
			       msm8x10->supplies[i].supply);
			break;
		} else {
			pr_debug("%s: Enabled regulator %s\n", __func__,
				 msm8x10->supplies[i].supply);
		}
	}

	while (ret && --i)
		if (!pdata->regulator[i].ondemand)
			regulator_disable(msm8x10->supplies[i].consumer);

	return ret;
}



static void msm8x10_wcd_disable_supplies(struct msm8x10_wcd *msm8x10,
				     struct msm8x10_wcd_pdata *pdata)
{
	int i;

	regulator_bulk_disable(msm8x10->num_of_supplies,
				    msm8x10->supplies);
	for (i = 0; i < msm8x10->num_of_supplies; i++) {
		if (regulator_count_voltages(msm8x10->supplies[i].consumer) <=
			0)
			continue;
		regulator_set_voltage(msm8x10->supplies[i].consumer, 0,
			pdata->regulator[i].max_uV);
		regulator_set_optimum_mode(msm8x10->supplies[i].consumer, 0);
	}
	regulator_bulk_free(msm8x10->num_of_supplies, msm8x10->supplies);
	kfree(msm8x10->supplies);
}

static int msm8x10_wcd_pads_config(void)
{
	void __iomem *ppull = ioremap(MSM8x10_TLMM_CDC_PULL_CTL, 4);
	/* Set I2C pads as pull up and rest of pads as no pull */
	iowrite32(0x03C00000, ppull);
	usleep_range(100, 200);

	iounmap(ppull);
	return 0;
}


static int msm8x10_wcd_clk_init(void)
{
	void __iomem *pdig1 = ioremap(MSM8X10_DINO_LPASS_DIGCODEC_CFG_RCGR, 4);
	void __iomem *pdig2 = ioremap(MSM8X10_DINO_LPASS_DIGCODEC_M, 4);
	void __iomem *pdig3 = ioremap(MSM8X10_DINO_LPASS_DIGCODEC_N, 4);
	void __iomem *pdig4 = ioremap(MSM8X10_DINO_LPASS_DIGCODEC_D, 4);
	void __iomem *pdig5 = ioremap(MSM8X10_DINO_LPASS_DIGCODEC_CBCR, 4);
	void __iomem *pdig6 = ioremap(MSM8X10_DINO_LPASS_DIGCODEC_CMD_RCGR, 4);
	/* Div-2 */
	iowrite32(0x3, pdig1);
	iowrite32(0x0, pdig2);
	iowrite32(0x0, pdig3);
	iowrite32(0x0, pdig4);
	/* Digital codec clock enable */
	iowrite32(0x1, pdig5);
	/* Set the update bit to make the settings go through */
	iowrite32(0x1, pdig6);
	usleep_range(100, 200);

	iounmap(pdig1);
	iounmap(pdig2);
	iounmap(pdig3);
	iounmap(pdig4);
	iounmap(pdig5);
	iounmap(pdig6);
	return 0;
}

static int msm8x10_wcd_device_init(struct msm8x10_wcd *msm8x10)
{
	mutex_init(&msm8x10->io_lock);
	mutex_init(&msm8x10->xfer_lock);
	msm8x10_wcd_pads_config();
	msm8x10_wcd_clk_init();
	return 0;
}

static struct intr_data interrupt_table[] = {
	{MSM8X10_WCD_IRQ_MBHC_INSERTION, true},
	{MSM8X10_WCD_IRQ_MBHC_POTENTIAL, true},
	{MSM8X10_WCD_IRQ_MBHC_RELEASE, true},
	{MSM8X10_WCD_IRQ_MBHC_PRESS, true},
	{MSM8X10_WCD_IRQ_MBHC_SHORT_TERM, true},
	{MSM8X10_WCD_IRQ_MBHC_REMOVAL, true},
	{MSM8X10_WCD_IRQ_MBHC_HS_DET, true},
	{MSM8X10_WCD_IRQ_RESERVED_0, false},
	{MSM8X10_WCD_IRQ_PA_STARTUP, false},
	{MSM8X10_WCD_IRQ_BG_PRECHARGE, false},
	{MSM8X10_WCD_IRQ_RESERVED_1, false},
	{MSM8X10_WCD_IRQ_EAR_PA_OCPL_FAULT, false},
	{MSM8X10_WCD_IRQ_EAR_PA_STARTUP, false},
	{MSM8X10_WCD_IRQ_SPKR_PA_OCPL_FAULT, false},
	{MSM8X10_WCD_IRQ_SPKR_CLIP_FAULT, false},
	{MSM8X10_WCD_IRQ_RESERVED_2, false},
	{MSM8X10_WCD_IRQ_HPH_L_PA_STARTUP, false},
	{MSM8X10_WCD_IRQ_HPH_R_PA_STARTUP, false},
	{MSM8X10_WCD_IRQ_HPH_PA_OCPL_FAULT, false},
	{MSM8X10_WCD_IRQ_HPH_PA_OCPR_FAULT, false},
	{MSM8X10_WCD_IRQ_RESERVED_3, false},
	{MSM8X10_WCD_IRQ_RESERVED_4, false},
	{MSM8X10_WCD_IRQ_RESERVED_5, false},
	{MSM8X10_WCD_IRQ_RESERVED_6, false},
};

static int __devinit msm8x10_wcd_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	struct msm8x10_wcd *msm8x10 = NULL;
	struct msm8x10_wcd_pdata *pdata;
	static int device_id;
	struct device *dev;
	enum apr_subsys_state q6_state;
	struct wcd9xxx_core_resource *core_res;

	dev_dbg(&client->dev, "%s(%d):slave addr = 0x%x device_id = %d\n",
		__func__, __LINE__,  client->addr, device_id);

	switch (client->addr) {
	case HELICON_CORE_0_I2C_ADDR:
		msm8x10_wcd_modules[0].client = client;
		break;
	case HELICON_CORE_1_I2C_ADDR:
		msm8x10_wcd_modules[1].client = client;
		goto rtn;
	case HELICON_CORE_2_I2C_ADDR:
		msm8x10_wcd_modules[2].client = client;
		goto rtn;
	case HELICON_CORE_3_I2C_ADDR:
		msm8x10_wcd_modules[3].client = client;
		goto rtn;
	default:
		ret = -EINVAL;
		goto rtn;
	}

	q6_state = apr_get_q6_state();
	if ((q6_state == APR_SUBSYS_DOWN) &&
	    (client->addr == HELICON_CORE_0_I2C_ADDR)) {
		dev_info(&client->dev, "defering %s, adsp_state %d\n", __func__,
			q6_state);
		return -EPROBE_DEFER;
	} else
		dev_info(&client->dev, "adsp is ready\n");

	dev_dbg(&client->dev, "%s(%d):slave addr = 0x%x device_id = %d\n",
		__func__, __LINE__,  client->addr, device_id);

	if (client->addr != HELICON_CORE_0_I2C_ADDR)
		goto rtn;

	dev_set_name(&client->dev, "%s", MSM8X10_CODEC_NAME);
	dev = &client->dev;
	if (client->dev.of_node) {
		dev_dbg(&client->dev, "%s:Platform data from device tree\n",
			__func__);
		pdata = msm8x10_wcd_populate_dt_pdata(&client->dev);
		if (!pdata) {
			dev_err(&client->dev, "%s: Failed to parse pdata from device tree\n",
				__func__);
			goto rtn;
		}
		client->dev.platform_data = pdata;
	} else {
		dev_dbg(&client->dev, "%s:Platform data from board file\n",
			__func__);
		pdata = client->dev.platform_data;
	}

	msm8x10 = kzalloc(sizeof(struct msm8x10_wcd), GFP_KERNEL);
	if (msm8x10 == NULL) {
		dev_err(&client->dev,
			"%s: error, allocation failed\n", __func__);
		ret = -ENOMEM;
		goto rtn;
	}

	msm8x10->dev = &client->dev;
	msm8x10->read_dev = __msm8x10_wcd_reg_read;
	msm8x10->write_dev = __msm8x10_wcd_reg_write;
	ret = msm8x10_wcd_init_supplies(msm8x10, pdata);
	if (ret) {
		dev_err(&client->dev, "%s: Fail to enable Codec supplies\n",
			__func__);
		goto err_codec;
	}

	ret = msm8x10_wcd_enable_static_supplies(msm8x10, pdata);
	if (ret) {
		pr_err("%s: Fail to enable Codec pre-reset supplies\n",
			   __func__);
		goto err_codec;
	}
	usleep_range(5, 5);

	ret = msm8x10_wcd_device_init(msm8x10);
	if (ret) {
		dev_err(&client->dev,
			"%s:msm8x10_wcd_device_init failed with error %d\n",
			__func__, ret);
		goto err_supplies;
	}
	dev_set_drvdata(&client->dev, msm8x10);
	core_res = &msm8x10->wcd9xxx_res;
	core_res->parent = msm8x10;
	core_res->dev = msm8x10->dev;
	core_res->intr_table = interrupt_table;
	core_res->intr_table_size = ARRAY_SIZE(interrupt_table);

	wcd9xxx_core_res_init(core_res,
					MSM8X10_WCD_NUM_IRQS,
					MSM8X10_WCD_NUM_IRQ_REGS,
					msm8x10_wcd_reg_read,
					msm8x10_wcd_reg_write,
					msm8x10_wcd_bulk_read,
					msm8x10_wcd_bulk_write);
	if (wcd9xxx_core_irq_init(core_res)) {
		dev_err(msm8x10->dev,
				"%s: irq initialization failed\n", __func__);
	} else {
		dev_info(msm8x10->dev,
				"%s: irq initialization passed\n", __func__);
	}

	ret = snd_soc_register_codec(&client->dev, &soc_codec_dev_msm8x10_wcd,
				     msm8x10_wcd_i2s_dai,
				     ARRAY_SIZE(msm8x10_wcd_i2s_dai));
	if (ret) {
		dev_err(&client->dev,
			"%s:snd_soc_register_codec failed with error %d\n",
			__func__, ret);
	} else {
		wcd9xxx_set_intf_type(WCD9XXX_INTERFACE_TYPE_I2C);
		goto rtn;
	}

err_supplies:
	msm8x10_wcd_disable_supplies(msm8x10, pdata);
err_codec:
	kfree(msm8x10);
rtn:
	return ret;
}

static void msm8x10_wcd_device_exit(struct msm8x10_wcd *msm8x10)
{
	mutex_destroy(&msm8x10->io_lock);
	mutex_destroy(&msm8x10->xfer_lock);
	kfree(msm8x10);
}

static int __devexit msm8x10_wcd_i2c_remove(struct i2c_client *client)
{
	struct msm8x10_wcd *msm8x10 = dev_get_drvdata(&client->dev);

	msm8x10_wcd_device_exit(msm8x10);
	return 0;
}

static struct i2c_device_id msm8x10_wcd_id_table[] = {
	{"msm8x10-wcd-i2c", MSM8X10_WCD_I2C_TOP_LEVEL},
	{"msm8x10-wcd-i2c", MSM8X10_WCD_I2C_ANALOG},
	{"msm8x10-wcd-i2c", MSM8X10_WCD_I2C_DIGITAL_1},
	{"msm8x10-wcd-i2c", MSM8X10_WCD_I2C_DIGITAL_2},
	{}
};

static struct of_device_id msm8x10_wcd_of_match[] = {
	{ .compatible = "qcom,msm8x10-wcd-i2c",},
	{ },
};

#ifdef CONFIG_PM
static int msm8x10_wcd_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct msm8x10_wcd_priv *priv = i2c_get_clientdata(client);
	struct msm8x10_wcd *msm8x10;
	int ret =  0;

	if (client->addr == HELICON_CORE_0_I2C_ADDR) {
		if (!priv || !priv->codec || !priv->codec->control_data) {
			ret = -EINVAL;
			dev_err(dev, "%s: Invalid client data\n", __func__);
			goto rtn;
		}
		msm8x10 = priv->codec->control_data;
		return wcd9xxx_core_res_resume(&msm8x10->wcd9xxx_res);
	}
rtn:
	return 0;
}

static int msm8x10_wcd_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct msm8x10_wcd_priv *priv = i2c_get_clientdata(client);
	struct msm8x10_wcd *msm8x10;
	int ret = 0;

	if (client->addr == HELICON_CORE_0_I2C_ADDR) {
		if (!priv || !priv->codec || !priv->codec->control_data) {
			ret = -EINVAL;
			dev_err(dev, "%s: Invalid client data\n", __func__);
			goto rtn;
		}
		msm8x10 = priv->codec->control_data;
		return wcd9xxx_core_res_suspend(&msm8x10->wcd9xxx_res,
						PMSG_SUSPEND);
	}

rtn:
	return ret;
}

static SIMPLE_DEV_PM_OPS(msm8x1_wcd_pm_ops, msm8x10_wcd_i2c_suspend,
			 msm8x10_wcd_i2c_resume);
#endif

static struct i2c_driver msm8x10_wcd_i2c_driver = {
	.driver                 = {
		.owner          = THIS_MODULE,
		.name           = "msm8x10-wcd-i2c-core",
		.of_match_table = msm8x10_wcd_of_match,
#ifdef CONFIG_PM
		.pm = &msm8x1_wcd_pm_ops,
#endif
	},
	.id_table               = msm8x10_wcd_id_table,
	.probe                  = msm8x10_wcd_i2c_probe,
	.remove                 = __devexit_p(msm8x10_wcd_i2c_remove),
};

static int __init msm8x10_wcd_codec_init(void)
{
	int ret;

	pr_debug("%s:\n", __func__);
	wcd9xxx_set_intf_type(WCD9XXX_INTERFACE_TYPE_PROBING);
	ret = i2c_add_driver(&msm8x10_wcd_i2c_driver);
	if (ret != 0)
		pr_err("%s: Failed to add msm8x10 wcd I2C driver - error %d\n",
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
MODULE_DEVICE_TABLE(i2c, msm8x10_wcd_id_table);

