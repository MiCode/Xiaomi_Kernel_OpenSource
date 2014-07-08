/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/spmi.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/wcd9xxx/pdata.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/qdsp6v2/apr.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <sound/q6afe-v2.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <soc/qcom/subsystem_notif.h>
#include "msm8x16-wcd.h"
#include "wcd-mbhc-v2.h"
#include "msm8916-wcd-irq.h"
#include "msm8x16_wcd_registers.h"
#include "../msm/qdsp6v2/q6core.h"

#define MSM8X16_WCD_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000)
#define MSM8X16_WCD_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE)

#define NUM_DECIMATORS		2
#define NUM_INTERPOLATORS	3
#define BITS_PER_REG		8
#define MSM8X16_WCD_TX_PORT_NUMBER	4

#define MSM8X16_WCD_I2S_MASTER_MODE_MASK	0x08
#define MSM8X16_DIGITAL_CODEC_BASE_ADDR		0x771C000
#define TOMBAK_CORE_0_SPMI_ADDR			0xf000
#define TOMBAK_CORE_1_SPMI_ADDR			0xf100

#define CODEC_DT_MAX_PROP_SIZE			40
#define MSM8X16_DIGITAL_CODEC_REG_SIZE		0x400
#define MAX_ON_DEMAND_SUPPLY_NAME_LENGTH	64

/*
 *50 Milliseconds sufficient for DSP bring up in the modem
 * after Sub System Restart
 */
#define ADSP_STATE_READY_TIMEOUT_MS 50

#define HPHL_PA_DISABLE (0x01 << 1)
#define HPHR_PA_DISABLE (0x01 << 2)
#define EAR_PA_DISABLE (0x01 << 3)
#define SPKR_PA_DISABLE (0x01 << 4)

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
static const DECLARE_TLV_DB_SCALE(analog_gain, 0, 25, 1);
static struct snd_soc_dai_driver msm8x16_wcd_i2s_dai[];

#define MSM8X16_WCD_ACQUIRE_LOCK(x) \
	mutex_lock_nested(&x, SINGLE_DEPTH_NESTING);

#define MSM8X16_WCD_RELEASE_LOCK(x) mutex_unlock(&x);


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
	struct msm8x16_wcd_priv *msm8x16_wcd;
	u32 decimator;
	u8 tx_hpf_cut_of_freq;
	struct delayed_work dwork;
};

static struct hpf_work tx_hpf_work[NUM_DECIMATORS];

static char on_demand_supply_name[][MAX_ON_DEMAND_SUPPLY_NAME_LENGTH] = {
	"cdc-vdd-mic-bias",
};

static unsigned long rx_digital_gain_reg[] = {
	MSM8X16_WCD_A_CDC_RX1_VOL_CTL_B2_CTL,
	MSM8X16_WCD_A_CDC_RX2_VOL_CTL_B2_CTL,
	MSM8X16_WCD_A_CDC_RX3_VOL_CTL_B2_CTL,
};

static unsigned long tx_digital_gain_reg[] = {
	MSM8X16_WCD_A_CDC_TX1_VOL_CTL_GAIN,
	MSM8X16_WCD_A_CDC_TX2_VOL_CTL_GAIN,
};

enum {
	MSM8X16_WCD_SPMI_DIGITAL = 0,
	MSM8X16_WCD_SPMI_ANALOG,
	MAX_MSM8X16_WCD_DEVICE
};

struct msm8x16_wcd_spmi {
	struct spmi_device *spmi;
	int base;
};

static const struct wcd_mbhc_intr intr_ids = {
	.mbhc_sw_intr =  MSM8X16_WCD_IRQ_MBHC_HS_DET,
	.mbhc_btn_press_intr = MSM8X16_WCD_IRQ_MBHC_PRESS,
	.mbhc_btn_release_intr = MSM8X16_WCD_IRQ_MBHC_RELEASE,
	.mbhc_hs_ins_rem_intr = MSM8X16_WCD_IRQ_MBHC_INSREM_DET,
	.hph_left_ocp = MSM8X16_WCD_IRQ_HPHL_OCP,
	.hph_right_ocp = MSM8X16_WCD_IRQ_HPHR_OCP,
};

static int msm8x16_wcd_dt_parse_vreg_info(struct device *dev,
	struct msm8x16_wcd_regulator *vreg,
	const char *vreg_name, bool ondemand);
static struct msm8x16_wcd_pdata *msm8x16_wcd_populate_dt_pdata(
	struct device *dev);
static int msm8x16_wcd_enable_ext_mb_source(struct snd_soc_codec *codec,
					    bool turn_on);

struct msm8x16_wcd_spmi msm8x16_wcd_modules[MAX_MSM8X16_WCD_DEVICE];

static void *modem_state_notifier;

static struct snd_soc_codec *registered_codec;

static const struct wcd_mbhc_cb mbhc_cb = {
	.enable_mb_source = msm8x16_wcd_enable_ext_mb_source,
};

int msm8x16_unregister_notifier(struct snd_soc_codec *codec,
				     struct notifier_block *nblock)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	return blocking_notifier_chain_unregister(&msm8x16_wcd->notifier,
			nblock);
}

int msm8x16_register_notifier(struct snd_soc_codec *codec,
				     struct notifier_block *nblock)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	return blocking_notifier_chain_register(&msm8x16_wcd->notifier, nblock);
}

void msm8x16_notifier_call(struct snd_soc_codec *codec,
				  const enum wcd_notify_event event)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: notifier call event %d\n", __func__, event);
	blocking_notifier_call_chain(&msm8x16_wcd->notifier, event, codec);
}

static int get_spmi_msm8x16_wcd_device_info(u16 *reg,
			struct msm8x16_wcd_spmi **msm8x16_wcd)
{
	int rtn = 0;
	int value = ((*reg & 0x0f00) >> 8) & 0x000f;

	*reg = *reg - (value * 0x100);
	switch (value) {
	case 0:
	case 1:
		*msm8x16_wcd = &msm8x16_wcd_modules[value];
		break;
	default:
		rtn = -EINVAL;
		break;
	}
	return rtn;
}

static int msm8x16_wcd_ahb_write_device(struct msm8x16_wcd *msm8x16_wcd,
					u16 reg, u8 *value, u32 bytes)
{
	u32 temp = ((u32)(*value)) & 0x000000FF;
	u16 offset = (reg ^ 0x0200) & 0x0FFF;
	bool q6_state = false;

	q6_state = q6core_is_adsp_ready();
	if (q6_state != true) {
		pr_debug("%s: q6 not ready %d\n", __func__, q6_state);
		return 0;
	} else
		pr_debug("%s: DSP is ready %d\n", __func__, q6_state);

	iowrite32(temp, msm8x16_wcd->dig_base + offset);
	return 0;
}

static int msm8x16_wcd_ahb_read_device(struct msm8x16_wcd *msm8x16_wcd,
					u16 reg, u32 bytes, u8 *value)
{
	u32 temp;
	u16 offset = (reg ^ 0x0200) & 0x0FFF;
	bool q6_state = false;

	q6_state = q6core_is_adsp_ready();
	if (q6_state != true) {
		pr_debug("%s: q6 not ready %d\n", __func__, q6_state);
		return 0;
	} else
		pr_debug("%s: DSP is ready %d\n", __func__, q6_state);

	temp = ioread32(msm8x16_wcd->dig_base + offset);
	*value = (u8)temp;
	return 0;
}

static int msm8x16_wcd_spmi_write_device(u16 reg, u8 *value, u32 bytes)
{

	int ret;
	struct msm8x16_wcd_spmi *wcd = NULL;

	ret = get_spmi_msm8x16_wcd_device_info(&reg, &wcd);
	if (ret) {
		pr_err("%s: Invalid register address\n", __func__);
		return ret;
	}

	if (wcd == NULL) {
		pr_err("%s: Failed to get device info\n", __func__);
		return -ENODEV;
	}
	ret = spmi_ext_register_writel(wcd->spmi->ctrl, wcd->spmi->sid,
						wcd->base + reg, value, bytes);
	if (ret)
		pr_err("Unable to write to addr=%x, ret(%d)\n", reg, ret);
	/* Try again if the write fails */
	if (ret != 0) {
		usleep(10);
		ret = spmi_ext_register_writel(wcd->spmi->ctrl, wcd->spmi->sid,
						wcd->base + reg, value, 1);
		if (ret != 0) {
			pr_err("failed to write the device\n");
			return ret;
		}
	}
	pr_debug("write sucess register = %x val = %x\n", reg, *value);
	return 0;
}


int msm8x16_wcd_spmi_read_device(u16 reg, u32 bytes, u8 *dest)
{
	int ret = 0;
	struct msm8x16_wcd_spmi *wcd = NULL;

	ret = get_spmi_msm8x16_wcd_device_info(&reg, &wcd);
	if (ret) {
		pr_err("%s: Invalid register address\n", __func__);
		return ret;
	}

	if (wcd == NULL) {
		pr_err("%s: Failed to get device info\n", __func__);
		return -ENODEV;
	}

	ret = spmi_ext_register_readl(wcd->spmi->ctrl, wcd->spmi->sid,
						wcd->base + reg, dest, bytes);
	if (ret != 0) {
		pr_err("failed to read the device\n");
		return ret;
	}
	pr_debug("%s: reg 0x%x = 0x%x\n", __func__, reg, *dest);
	return 0;
}

int msm8x16_wcd_spmi_read(unsigned short reg, int bytes, void *dest)
{
	return msm8x16_wcd_spmi_read_device(reg, bytes, dest);
}

int msm8x16_wcd_spmi_write(unsigned short reg, int bytes, void *src)
{
	return msm8x16_wcd_spmi_write_device(reg, src, bytes);
}

static int __msm8x16_wcd_reg_read(struct snd_soc_codec *codec,
				unsigned short reg)
{
	int ret = -EINVAL;
	u8 temp = 0;
	struct msm8x16_wcd *msm8x16_wcd = codec->control_data;
	struct msm8916_asoc_mach_data *pdata = NULL;

	pr_debug("%s reg = %x\n", __func__, reg);
	mutex_lock(&msm8x16_wcd->io_lock);
	pdata = snd_soc_card_get_drvdata(codec->card);
	if (MSM8X16_WCD_IS_TOMBAK_REG(reg))
		ret = msm8x16_wcd_spmi_read(reg, 1, &temp);
	else if (MSM8X16_WCD_IS_DIGITAL_REG(reg)) {
		mutex_lock(&pdata->cdc_mclk_mutex);
		if (atomic_read(&pdata->mclk_enabled) == false) {
			pdata->digital_cdc_clk.clk_val = pdata->mclk_freq;
			ret = afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_clk);
			if (ret < 0) {
				pr_err("failed to enable the MCLK\n");
				goto err;
			}
			pr_debug("%s: MCLK not enabled\n", __func__);
			ret = msm8x16_wcd_ahb_read_device(
					msm8x16_wcd, reg, 1, &temp);
			atomic_set(&pdata->mclk_enabled, true);
			schedule_delayed_work(&pdata->disable_mclk_work, 50);
err:
			mutex_unlock(&pdata->cdc_mclk_mutex);
			mutex_unlock(&msm8x16_wcd->io_lock);
			return temp;
		}
		ret = msm8x16_wcd_ahb_read_device(msm8x16_wcd, reg, 1, &temp);
		mutex_unlock(&pdata->cdc_mclk_mutex);
	}
	mutex_unlock(&msm8x16_wcd->io_lock);

	if (ret < 0) {
		dev_err(msm8x16_wcd->dev,
				"%s: codec read failed for reg 0x%x\n",
				__func__, reg);
		return ret;
	} else {
		dev_dbg(msm8x16_wcd->dev, "Read 0x%02x from 0x%x\n",
				temp, reg);
	}

	return temp;
}

static int __msm8x16_wcd_reg_write(struct snd_soc_codec *codec,
			unsigned short reg, u8 val)
{
	int ret = -EINVAL;
	struct msm8x16_wcd *msm8x16_wcd = codec->control_data;
	struct msm8916_asoc_mach_data *pdata = NULL;

	mutex_lock(&msm8x16_wcd->io_lock);
	pdata = snd_soc_card_get_drvdata(codec->card);
	if (MSM8X16_WCD_IS_TOMBAK_REG(reg))
		ret = msm8x16_wcd_spmi_write(reg, 1, &val);
	else if (MSM8X16_WCD_IS_DIGITAL_REG(reg)) {
		mutex_lock(&pdata->cdc_mclk_mutex);
		if (atomic_read(&pdata->mclk_enabled) == false) {
			pr_debug("MCLK not enabled %s:\n", __func__);
			pdata->digital_cdc_clk.clk_val = pdata->mclk_freq;
			ret = afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_clk);
			if (ret < 0) {
				pr_err("failed to enable the MCLK\n");
				ret = 0;
				goto err;
			}
			ret = msm8x16_wcd_ahb_write_device(
						msm8x16_wcd, reg, &val, 1);
			atomic_set(&pdata->mclk_enabled, true);
			schedule_delayed_work(&pdata->disable_mclk_work, 50);
err:
			mutex_unlock(&pdata->cdc_mclk_mutex);
			mutex_unlock(&msm8x16_wcd->io_lock);
			return ret;
		}
		ret = msm8x16_wcd_ahb_write_device(msm8x16_wcd, reg, &val, 1);
		mutex_unlock(&pdata->cdc_mclk_mutex);
	}
	mutex_unlock(&msm8x16_wcd->io_lock);

	return ret;
}

static int msm8x16_wcd_volatile(struct snd_soc_codec *codec, unsigned int reg)
{
	dev_dbg(codec->dev, "%s: reg 0x%x\n", __func__, reg);

	return msm8x16_wcd_reg_readonly[reg];
}

static int msm8x16_wcd_readable(struct snd_soc_codec *ssc, unsigned int reg)
{
	return msm8x16_wcd_reg_readable[reg];
}

static int msm8x16_wcd_write(struct snd_soc_codec *codec, unsigned int reg,
			     unsigned int value)
{
	int ret;

	dev_dbg(codec->dev, "%s: Write from reg 0x%x val 0x%x\n",
					__func__, reg, value);
	if (reg == SND_SOC_NOPM)
		return 0;

	BUG_ON(reg > MSM8X16_WCD_MAX_REGISTER);
	if (!msm8x16_wcd_volatile(codec, reg)) {
		ret = snd_soc_cache_write(codec, reg, value);
		if (ret != 0)
			dev_err(codec->dev, "Cache write to %x failed: %d\n",
				reg, ret);
	}
	return __msm8x16_wcd_reg_write(codec, reg, (u8)value);
}

static unsigned int msm8x16_wcd_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	unsigned int val;
	int ret;

	if (reg == SND_SOC_NOPM)
		return 0;

	BUG_ON(reg > MSM8X16_WCD_MAX_REGISTER);

	if (!msm8x16_wcd_volatile(codec, reg) &&
	    msm8x16_wcd_readable(codec, reg) &&
		reg < codec->driver->reg_cache_size) {
		ret = snd_soc_cache_read(codec, reg, &val);
		if (ret >= 0) {
			return val;
		} else
			dev_err(codec->dev, "Cache read from %x failed: %d\n",
				reg, ret);
	}
	val = __msm8x16_wcd_reg_read(codec, reg);
	dev_dbg(codec->dev, "%s: Read from reg 0x%x val 0x%x\n",
					__func__, reg, val);
	return val;
}


static int msm8x16_wcd_dt_parse_vreg_info(struct device *dev,
	struct msm8x16_wcd_regulator *vreg, const char *vreg_name,
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
		vreg->min_uv = be32_to_cpup(&prop[0]);
		vreg->max_uv = be32_to_cpup(&prop[1]);
	}

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE,
		"qcom,%s-current", vreg_name);

	ret = of_property_read_u32(dev->of_node, prop_name, &prop_val);
	if (ret) {
		dev_err(dev, "Looking up %s property in node %s failed",
			prop_name, dev->of_node->full_name);
		return -EFAULT;
	}
	vreg->optimum_ua = prop_val;

	dev_dbg(dev, "%s: vol=[%d %d]uV, curr=[%d]uA, ond %d\n\n", vreg->name,
		 vreg->min_uv, vreg->max_uv, vreg->optimum_ua, vreg->ondemand);
	return 0;
}

static struct msm8x16_wcd_pdata *msm8x16_wcd_populate_dt_pdata(
						struct device *dev)
{
	struct msm8x16_wcd_pdata *pdata;
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
		dev_err(dev, "%s: Num of supplies %u > max supported %zd\n",
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
		ret = msm8x16_wcd_dt_parse_vreg_info(dev,
						&pdata->regulator[idx],
						name, false);
		if (ret) {
			dev_err(dev, "%s:err parsing vreg for %s idx %d\n",
				__func__, name, idx);
			goto err;
		}
	}

	for (i = 0; i < ond_cnt; i++, idx++) {
		ret = of_property_read_string_index(dev->of_node, ond_prop_name,
						    i, &name);
		if (ret) {
			dev_err(dev, "%s: err parsing on_demand for %s idx %d\n",
				__func__, ond_prop_name, i);
			goto err;
		}

		dev_dbg(dev, "%s: Found on-demand cdc supply %s\n", __func__,
			name);
		ret = msm8x16_wcd_dt_parse_vreg_info(dev,
						&pdata->regulator[idx],
						name, true);
		if (ret) {
			dev_err(dev, "%s: err parsing vreg on_demand for %s idx %d\n",
				__func__, name, idx);
			goto err;
		}
	}

	return pdata;
err:
	devm_kfree(dev, pdata);
	dev_err(dev, "%s: Failed to populate DT data ret = %d\n",
		__func__, ret);
	return NULL;
}

static int msm8x16_wcd_codec_enable_on_demand_supply(
		struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);
	struct on_demand_supply *supply;

	if (w->shift >= ON_DEMAND_SUPPLIES_MAX) {
		dev_err(codec->dev, "%s: error index > MAX Demand supplies",
			__func__);
		ret = -EINVAL;
		goto out;
	}
	dev_dbg(codec->dev, "%s: supply: %s event: %d ref: %d\n",
		__func__, on_demand_supply_name[w->shift], event,
		atomic_read(&msm8x16_wcd->on_demand_list[w->shift].ref));

	supply = &msm8x16_wcd->on_demand_list[w->shift];
	WARN_ONCE(!supply->supply, "%s isn't defined\n",
		  on_demand_supply_name[w->shift]);
	if (!supply->supply) {
		dev_err(codec->dev, "%s: err supply not present ond for %d",
			__func__, w->shift);
		goto out;
	}
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

static int msm8x16_wcd_codec_enable_charge_pump(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (!(strcmp(w->name, "EAR CP"))) {
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
					0x80, 0x80);
			if (msm8x16_wcd->ear_pa_boost_set) {
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_SEC_ACCESS,
					0xA5, 0xA5);
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_PERPH_RESET_CTL3,
					0x07, 0x07);
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_BYPASS_MODE,
					0x40, 0x40);
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_BYPASS_MODE,
					0x80, 0x80);
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_BYPASS_MODE,
					0x02, 0x02);
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_BOOST_EN_CTL,
					0xDF, 0xDF);
			}
		} else {
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
					0xC0, 0xC0);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		break;
	case SND_SOC_DAPM_POST_PMD:
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		if (!(strcmp(w->name, "EAR CP"))) {
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
					0x80, 0x00);
			if (msm8x16_wcd->ear_pa_boost_set) {
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_BOOST_EN_CTL,
					0x80, 0x00);
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_BYPASS_MODE,
					0x80, 0x00);
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_BYPASS_MODE,
					0x02, 0x00);
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_BYPASS_MODE,
					0x40, 0x00);
			}
		} else {
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
					0x40, 0x00);
			if (msm8x16_wcd->rx_bias_count == 0)
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
					0x80, 0x00);
			dev_dbg(codec->dev, "%s: rx_bias_count = %d\n",
					__func__, msm8x16_wcd->rx_bias_count);
		}
		break;
	}
	return 0;
}

static int msm8x16_wcd_ear_pa_boost_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] =
		(msm8x16_wcd->ear_pa_boost_set ? 1 : 0);
	dev_dbg(codec->dev, "%s: msm8x16_wcd->ear_pa_boost_set = %d\n",
			__func__, msm8x16_wcd->ear_pa_boost_set);
	return 0;
}

static int msm8x16_wcd_ear_pa_boost_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);
	msm8x16_wcd->ear_pa_boost_set =
		(ucontrol->value.integer.value[0] ? true : false);
	return 0;
}

static int msm8x16_wcd_pa_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	ear_pa_gain = snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_RX_EAR_CTL);

	ear_pa_gain = (ear_pa_gain >> 5) & 0x1;

	if (ear_pa_gain == 0x00) {
		ucontrol->value.integer.value[0] = 0;
	} else if (ear_pa_gain == 0x01) {
		ucontrol->value.integer.value[0] = 1;
	} else  {
		dev_err(codec->dev, "%s: ERROR: Unsupported Ear Gain = 0x%x\n",
			__func__, ear_pa_gain);
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = ear_pa_gain;
	dev_dbg(codec->dev, "%s: ear_pa_gain = 0x%x\n", __func__, ear_pa_gain);
	return 0;
}

static int msm8x16_wcd_pa_gain_put(struct snd_kcontrol *kcontrol,
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
		ear_pa_gain = 0x20;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_RX_EAR_CTL,
			    0x20, ear_pa_gain);
	return 0;
}

static int msm8x16_wcd_spk_boost_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if (msm8x16_wcd->spk_boost_set == false) {
		ucontrol->value.integer.value[0] = 0;
	} else if (msm8x16_wcd->spk_boost_set == true) {
		ucontrol->value.integer.value[0] = 1;
	} else  {
		dev_err(codec->dev, "%s: ERROR: Unsupported Speaker Boost = %d\n",
			__func__, msm8x16_wcd->spk_boost_set);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s: msm8x16_wcd->spk_boost_set = %d\n", __func__,
			msm8x16_wcd->spk_boost_set);
	return 0;
}

static int msm8x16_wcd_spk_boost_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm8x16_wcd->spk_boost_set = false;
		break;
	case 1:
		msm8x16_wcd->spk_boost_set = true;
		break;
	default:
		return -EINVAL;
	}
	dev_dbg(codec->dev, "%s: msm8x16_wcd->spk_boost_set = %d\n",
		__func__, msm8x16_wcd->spk_boost_set);
	return 0;
}

static int msm8x16_wcd_get_iir_enable_audio_mixer(
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
			    (MSM8X16_WCD_A_CDC_IIR1_CTL + 64 * iir_idx)) &
		(1 << band_idx)) != 0;

	dev_dbg(codec->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0]);
	return 0;
}

static int msm8x16_wcd_put_iir_enable_audio_mixer(
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
	snd_soc_update_bits(codec,
		(MSM8X16_WCD_A_CDC_IIR1_CTL + 64 * iir_idx),
			    (1 << band_idx), (value << band_idx));

	dev_dbg(codec->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
	  iir_idx, band_idx,
		((snd_soc_read(codec,
		(MSM8X16_WCD_A_CDC_IIR1_CTL + 64 * iir_idx)) &
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
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t)) & 0x7F);

	value |= snd_soc_read(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx));

	snd_soc_write(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 1) & 0x7F);

	value |= (snd_soc_read(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx)) << 8);

	snd_soc_write(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 2) & 0x7F);

	value |= (snd_soc_read(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx)) << 16);

	snd_soc_write(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 3) & 0x7F);

	/* Mask bits top 2 bits since they are reserved */
	value |= ((snd_soc_read(codec, (MSM8X16_WCD_A_CDC_IIR1_COEF_B2_CTL
		+ 64 * iir_idx)) & 0x3f) << 24);

	return value;

}

static int msm8x16_wcd_get_iir_band_audio_mixer(
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
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value & 0xFF));

	snd_soc_write(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value >> 8) & 0xFF);

	snd_soc_write(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value >> 16) & 0xFF);

	/* Mask top 2 bits, 7-8 are reserved */
	snd_soc_write(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value >> 24) & 0x3F);

}

static int msm8x16_wcd_put_iir_band_audio_mixer(
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
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
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

static const char * const msm8x16_wcd_ear_pa_boost_ctrl_text[] = {
		"DISABLE", "ENABLE"};
static const struct soc_enum msm8x16_wcd_ear_pa_boost_ctl_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, msm8x16_wcd_ear_pa_boost_ctrl_text),
};

static const char * const msm8x16_wcd_ear_pa_gain_text[] = {
		"POS_1P5_DB", "POS_6_DB"};
static const struct soc_enum msm8x16_wcd_ear_pa_gain_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, msm8x16_wcd_ear_pa_gain_text),
};

static const char * const msm8x16_wcd_spk_boost_ctrl_text[] = {
		"DISABLE", "ENABLE"};
static const struct soc_enum msm8x16_wcd_spk_boost_ctl_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, msm8x16_wcd_spk_boost_ctrl_text),
};

/*cut of frequency for high pass filter*/
static const char * const cf_text[] = {
	"MIN_3DB_4Hz", "MIN_3DB_75Hz", "MIN_3DB_150Hz"
};

static const struct soc_enum cf_dec1_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_TX1_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec2_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_TX2_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_rxmix1_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_RX1_B4_CTL, 0, 3, cf_text);

static const struct soc_enum cf_rxmix2_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_RX2_B4_CTL, 0, 3, cf_text);

static const struct soc_enum cf_rxmix3_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_RX3_B4_CTL, 0, 3, cf_text);

static const struct snd_kcontrol_new msm8x16_wcd_snd_controls[] = {

	SOC_ENUM_EXT("EAR PA Boost", msm8x16_wcd_ear_pa_boost_ctl_enum[0],
		msm8x16_wcd_ear_pa_boost_get, msm8x16_wcd_ear_pa_boost_set),

	SOC_ENUM_EXT("EAR PA Gain", msm8x16_wcd_ear_pa_gain_enum[0],
		msm8x16_wcd_pa_gain_get, msm8x16_wcd_pa_gain_put),

	SOC_ENUM_EXT("Speaker Boost", msm8x16_wcd_spk_boost_ctl_enum[0],
		msm8x16_wcd_spk_boost_get, msm8x16_wcd_spk_boost_set),

	SOC_SINGLE_TLV("ADC1 Volume", MSM8X16_WCD_A_ANALOG_TX_1_EN, 3,
					8, 0, analog_gain),
	SOC_SINGLE_TLV("ADC2 Volume", MSM8X16_WCD_A_ANALOG_TX_2_EN, 3,
					8, 0, analog_gain),
	SOC_SINGLE_TLV("ADC3 Volume", MSM8X16_WCD_A_ANALOG_TX_3_EN, 3,
					8, 0, analog_gain),

	SOC_SINGLE_SX_TLV("RX1 Digital Volume",
			  MSM8X16_WCD_A_CDC_RX1_VOL_CTL_B2_CTL,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX2 Digital Volume",
			  MSM8X16_WCD_A_CDC_RX2_VOL_CTL_B2_CTL,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX3 Digital Volume",
			  MSM8X16_WCD_A_CDC_RX3_VOL_CTL_B2_CTL,
			0,  -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("DEC1 Volume",
			  MSM8X16_WCD_A_CDC_TX1_VOL_CTL_GAIN,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC2 Volume",
			  MSM8X16_WCD_A_CDC_TX2_VOL_CTL_GAIN,
			0,  -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("IIR1 INP1 Volume",
			  MSM8X16_WCD_A_CDC_IIR1_GAIN_B1_CTL,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP2 Volume",
			  MSM8X16_WCD_A_CDC_IIR1_GAIN_B2_CTL,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP3 Volume",
			  MSM8X16_WCD_A_CDC_IIR1_GAIN_B3_CTL,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP4 Volume",
			  MSM8X16_WCD_A_CDC_IIR1_GAIN_B4_CTL,
			0,  -84,	40, digital_gain),

	SOC_SINGLE("MICBIAS CAPLESS Switch",
		   MSM8X16_WCD_A_ANALOG_MICB_1_EN, 6, 1, 0),

	SOC_ENUM("TX1 HPF cut off", cf_dec1_enum),
	SOC_ENUM("TX2 HPF cut off", cf_dec2_enum),

	SOC_SINGLE("TX1 HPF Switch",
		MSM8X16_WCD_A_CDC_TX1_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX2 HPF Switch",
		MSM8X16_WCD_A_CDC_TX2_MUX_CTL, 3, 1, 0),

	SOC_SINGLE("RX1 HPF Switch",
		MSM8X16_WCD_A_CDC_RX1_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX2 HPF Switch",
		MSM8X16_WCD_A_CDC_RX2_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX3 HPF Switch",
		MSM8X16_WCD_A_CDC_RX3_B5_CTL, 2, 1, 0),

	SOC_ENUM("RX1 HPF cut off", cf_rxmix1_enum),
	SOC_ENUM("RX2 HPF cut off", cf_rxmix2_enum),
	SOC_ENUM("RX3 HPF cut off", cf_rxmix3_enum),

	SOC_SINGLE_EXT("IIR1 Enable Band1", IIR1, BAND1, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band2", IIR1, BAND2, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band3", IIR1, BAND3, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band4", IIR1, BAND4, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band5", IIR1, BAND5, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band1", IIR2, BAND1, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band2", IIR2, BAND2, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band3", IIR2, BAND3, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band4", IIR2, BAND4, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band5", IIR2, BAND5, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),

	SOC_SINGLE_MULTI_EXT("IIR1 Band1", IIR1, BAND1, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band2", IIR1, BAND2, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band3", IIR1, BAND3, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band4", IIR1, BAND4, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band5", IIR1, BAND5, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band1", IIR2, BAND1, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band2", IIR2, BAND2, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band3", IIR2, BAND3, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band4", IIR2, BAND4, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band5", IIR2, BAND5, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),

};

static const char * const rx_mix1_text[] = {
	"ZERO", "IIR1", "IIR2", "RX1", "RX2", "RX3"
};

static const char * const rx_mix2_text[] = {
	"ZERO", "IIR1", "IIR2"
};

static const char * const dec_mux_text[] = {
	"ZERO", "ADC1", "ADC2", "ADC3", "DMIC1", "DMIC2"
};

static const char * const adc2_mux_text[] = {
	"ZERO", "INP2", "INP3"
};

static const char * const rdac2_mux_text[] = {
	"ZERO", "RX2", "RX1"
};

static const char * const iir1_inp1_text[] = {
	"ZERO", "DEC1", "DEC2", "RX1", "RX2", "RX3"
};

static const struct soc_enum adc2_enum =
	SOC_ENUM_SINGLE(0, 0, ARRAY_SIZE(adc2_mux_text), adc2_mux_text);

/* RX1 MIX1 */
static const struct soc_enum rx_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX1_B1_CTL,
		0, 6, rx_mix1_text);

static const struct soc_enum rx_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX1_B1_CTL,
		3, 6, rx_mix1_text);

static const struct soc_enum rx_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX1_B2_CTL,
		0, 6, rx_mix1_text);

/* RX1 MIX2 */
static const struct soc_enum rx_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX1_B3_CTL,
		0, 3, rx_mix2_text);

/* RX2 MIX1 */
static const struct soc_enum rx2_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX2_B1_CTL,
		0, 6, rx_mix1_text);

static const struct soc_enum rx2_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX2_B1_CTL,
		3, 6, rx_mix1_text);

static const struct soc_enum rx2_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX2_B1_CTL,
		0, 6, rx_mix1_text);

/* RX2 MIX2 */
static const struct soc_enum rx2_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX2_B3_CTL,
		0, 3, rx_mix2_text);

/* RX3 MIX1 */
static const struct soc_enum rx3_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX3_B1_CTL,
		0, 6, rx_mix1_text);

static const struct soc_enum rx3_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX3_B1_CTL,
		3, 6, rx_mix1_text);

static const struct soc_enum rx3_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX3_B1_CTL,
		0, 6, rx_mix1_text);

/* DEC */
static const struct soc_enum dec1_mux_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_TX_B1_CTL,
		0, 6, dec_mux_text);

static const struct soc_enum dec2_mux_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_TX_B1_CTL,
		3, 6, dec_mux_text);

static const struct soc_enum rdac2_mux_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_DIGITAL_CDC_CONN_HPHR_DAC_CTL,
		0, 3, rdac2_mux_text);

static const struct soc_enum iir1_inp1_mux_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_EQ1_B1_CTL,
		0, 6, iir1_inp1_text);

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

static const struct snd_kcontrol_new rx2_mix1_inp3_mux =
	SOC_DAPM_ENUM("RX2 MIX1 INP3 Mux", rx2_mix1_inp3_chain_enum);

static const struct snd_kcontrol_new rx3_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP1 Mux", rx3_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx3_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP2 Mux", rx3_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx3_mix1_inp3_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP3 Mux", rx3_mix1_inp3_chain_enum);

static const struct snd_kcontrol_new rx1_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX1 MIX2 INP1 Mux", rx_mix2_inp1_chain_enum);

static const struct snd_kcontrol_new rx2_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX2 MIX2 INP1 Mux", rx2_mix2_inp1_chain_enum);

static const struct snd_kcontrol_new tx_adc2_mux =
	SOC_DAPM_ENUM_VIRT("ADC2 MUX Mux", adc2_enum);

static int msm8x16_wcd_put_dec_enum(struct snd_kcontrol *kcontrol,
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

	if (ucontrol->value.enumerated.item[0] > e->max - 1) {
		dev_err(codec->dev, "%s: Invalid enum value: %d\n",
			__func__, ucontrol->value.enumerated.item[0]);
		return -EINVAL;
	}
	dec_mux = ucontrol->value.enumerated.item[0];

	widget_name = kstrndup(w->name, 15, GFP_KERNEL);
	if (!widget_name) {
		dev_err(codec->dev, "%s: failed to copy string\n",
			__func__);
		return -ENOMEM;
	}
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
		if ((dec_mux == 4) || (dec_mux == 5))
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

	tx_mux_ctl_reg =
		MSM8X16_WCD_A_CDC_TX1_MUX_CTL + 32 * (decimator - 1);

	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x1, adc_dmic_sel);

	ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);

out:
	kfree(widget_name);
	return ret;
}

#define MSM8X16_WCD_DEC_ENUM(xname, xenum) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_enum_double, \
	.get = snd_soc_dapm_get_enum_double, \
	.put = msm8x16_wcd_put_dec_enum, \
	.private_value = (unsigned long)&xenum }

static const struct snd_kcontrol_new dec1_mux =
	MSM8X16_WCD_DEC_ENUM("DEC1 MUX Mux", dec1_mux_enum);

static const struct snd_kcontrol_new dec2_mux =
	MSM8X16_WCD_DEC_ENUM("DEC2 MUX Mux", dec2_mux_enum);

static const struct snd_kcontrol_new rdac2_mux =
	MSM8X16_WCD_DEC_ENUM("RDAC2 MUX Mux", rdac2_mux_enum);

static const struct snd_kcontrol_new iir1_inp1_mux =
	SOC_DAPM_ENUM("IIR1 INP1 Mux", iir1_inp1_mux_enum);

static const struct snd_kcontrol_new ear_pa_switch[] = {
	SOC_DAPM_SINGLE("Switch",
		MSM8X16_WCD_A_ANALOG_RX_EAR_CTL, 5, 1, 0)
};

static const char * const hph_text[] = {
	"ZERO", "Switch",
};

static const struct soc_enum hph_enum =
	SOC_ENUM_SINGLE(0, 0, ARRAY_SIZE(hph_text), hph_text);

static const struct snd_kcontrol_new hphl_mux[] = {
	SOC_DAPM_ENUM_VIRT("HPHL", hph_enum)
};

static const struct snd_kcontrol_new hphr_mux[] = {
	SOC_DAPM_ENUM_VIRT("HPHR", hph_enum)
};

static const struct snd_kcontrol_new spkr_switch[] = {
	SOC_DAPM_SINGLE("Switch",
		MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 7, 1, 0)
};

static void msm8x16_wcd_codec_enable_adc_block(struct snd_soc_codec *codec,
					 int enable)
{
	struct msm8x16_wcd_priv *wcd8x16 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %d\n", __func__, enable);

	if (enable) {
		wcd8x16->adc_count++;
		snd_soc_update_bits(codec,
				    MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL,
				    0x20, 0x20);
		snd_soc_update_bits(codec,
				    MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
				    0x10, 0x10);
	} else {
		wcd8x16->adc_count--;
		if (!wcd8x16->adc_count) {
			snd_soc_update_bits(codec,
				    MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
				    0x10, 0x00);
			snd_soc_update_bits(codec,
				    MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL,
					    0x20, 0x0);
		}
	}
}

static int msm8x16_wcd_codec_enable_adc(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);
	u16 adc_reg;
	u8 init_bit_shift;

	dev_dbg(codec->dev, "%s %d\n", __func__, event);

	adc_reg = MSM8X16_WCD_A_ANALOG_TX_1_2_TEST_CTL_2;

	if (w->reg == MSM8X16_WCD_A_ANALOG_TX_1_EN)
		init_bit_shift = 5;
	else if ((w->reg == MSM8X16_WCD_A_ANALOG_TX_2_EN) ||
		 (w->reg == MSM8X16_WCD_A_ANALOG_TX_3_EN))
		init_bit_shift = 4;
	else {
		dev_err(codec->dev, "%s: Error, invalid adc register\n",
			__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (!TOMBAK_IS_1_0(msm8x16_wcd->pmic_rev))
			snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_TX_1_2_OPAMP_BIAS, 0x07, 0x00);
		msm8x16_wcd_codec_enable_adc_block(codec, 1);
		if (w->reg == MSM8X16_WCD_A_ANALOG_TX_2_EN)
			snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MICB_1_CTL, 0x02, 0x02);
		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift,
				1 << init_bit_shift);
		if (w->reg == MSM8X16_WCD_A_ANALOG_TX_1_EN)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_DIGITAL_CDC_CONN_TX1_CTL,
				0x03, 0x00);
		else if ((w->reg == MSM8X16_WCD_A_ANALOG_TX_2_EN) ||
			(w->reg == MSM8X16_WCD_A_ANALOG_TX_3_EN))
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_DIGITAL_CDC_CONN_TX2_CTL,
				0x03, 0x00);
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift, 0x00);
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		break;
	case SND_SOC_DAPM_POST_PMD:
		msm8x16_wcd_codec_enable_adc_block(codec, 0);
		if (w->reg == MSM8X16_WCD_A_ANALOG_TX_2_EN)
			snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MICB_1_CTL, 0x02, 0x00);
		if (w->reg == MSM8X16_WCD_A_ANALOG_TX_1_EN)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_DIGITAL_CDC_CONN_TX1_CTL,
				0x03, 0x02);
		else if ((w->reg == MSM8X16_WCD_A_ANALOG_TX_2_EN) ||
			(w->reg == MSM8X16_WCD_A_ANALOG_TX_3_EN))
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_DIGITAL_CDC_CONN_TX2_CTL,
				0x03, 0x02);

		break;
	}
	return 0;
}

static int msm8x16_wcd_codec_enable_spk_pa(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(w->codec->dev, "%s %d %s\n", __func__, event, w->name);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL, 0x10, 0x10);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_SPKR_PWRSTG_CTL, 0x01, 0x01);
		if (!msm8x16_wcd->spk_boost_set)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 0x10, 0x10);
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_SPKR_PWRSTG_CTL, 0xE0, 0xE0);
		if (!TOMBAK_IS_1_0(msm8x16_wcd->pmic_rev))
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_EAR_CTL, 0x01, 0x01);
		break;
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		if (msm8x16_wcd->spk_boost_set)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL, 0xEF, 0xEF);
		else
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 0x10, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_RX3_B6_CTL, 0x01, 0x00);
		snd_soc_update_bits(codec, w->reg, 0x80, 0x80);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_RX3_B6_CTL, 0x01, 0x01);
		msleep(20);
		msm8x16_wcd->mute_mask |= SPKR_PA_DISABLE;
		snd_soc_update_bits(codec, w->reg, 0x80, 0x00);
		if (msm8x16_wcd->spk_boost_set)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL, 0xEF, 0x00);
		else
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 0x10, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_SPKR_PWRSTG_CTL, 0xE0, 0x00);
		if (!TOMBAK_IS_1_0(msm8x16_wcd->pmic_rev))
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_EAR_CTL, 0x01, 0x00);
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_SPKR_PWRSTG_CTL, 0x01, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL, 0x10, 0x00);
		break;
	}
	return 0;
}

static int msm8x16_wcd_codec_enable_dig_clk(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(w->codec->dev, "%s event %d w->name %s\n", __func__,
			event, w->name);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, w->reg, 0x80, 0x80);
		if (msm8x16_wcd->spk_boost_set) {
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_SEC_ACCESS,
					0xA5, 0xA5);
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_PERPH_RESET_CTL3,
					0x0F, 0x0F);
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_CURRENT_LIMIT,
					0x82, 0x82);
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
					0x20, 0x20);
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_BOOST_EN_CTL,
					0xDF, 0xDF);
			usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_CURRENT_LIMIT,
					0x83, 0x83);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (msm8x16_wcd->spk_boost_set) {
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_BOOST_EN_CTL,
					0xDF, 0x5F);
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
					0x20, 0x00);
		}
		break;
	}
	return 0;
}

static int msm8x16_wcd_codec_enable_dmic(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);
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
		dmic_clk_cnt = &(msm8x16_wcd->dmic_1_2_clk_cnt);
		dmic_clk_reg = MSM8X16_WCD_A_CDC_CLK_DMIC_B1_CTL;
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
		if (*dmic_clk_cnt == 1) {
			snd_soc_update_bits(codec, dmic_clk_reg,
					0x0E, 0x02);
			snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_TX1_DMIC_CTL,	0x07, 0x01);
			snd_soc_update_bits(codec, dmic_clk_reg,
					dmic_clk_en, dmic_clk_en);
		}
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

static int msm8x16_wcd_enable_ext_mb_source(struct snd_soc_codec *codec,
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

static int msm8x16_wcd_codec_enable_micbias(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	u16 micb_int_reg;
	char *internal1_text = "Internal1";
	char *internal2_text = "Internal2";
	char *internal3_text = "Internal3";

	dev_dbg(codec->dev, "%s %d\n", __func__, event);
	switch (w->reg) {
	case MSM8X16_WCD_A_ANALOG_MICB_1_EN:
	case MSM8X16_WCD_A_ANALOG_MICB_2_EN:
		micb_int_reg = MSM8X16_WCD_A_ANALOG_MICB_1_INT_RBIAS;
		break;
	default:
		dev_err(codec->dev,
			"%s: Error, invalid micbias register 0x%x\n",
			__func__, w->reg);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (strnstr(w->name, internal1_text, 30)) {
			snd_soc_update_bits(codec, micb_int_reg, 0x80, 0x80);
		} else if (strnstr(w->name, internal2_text, 30)) {
			snd_soc_update_bits(codec, micb_int_reg, 0x10, 0x10);
			snd_soc_update_bits(codec, w->reg, 0x20, 0x00);
		} else if (strnstr(w->name, internal3_text, 30)) {
			snd_soc_update_bits(codec, micb_int_reg, 0x2, 0x2);
		}
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MICB_1_EN, 0x05, 0x04);

		break;
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(20000, 20100);
		if (strnstr(w->name, internal1_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x40, 0x40);
		else if (strnstr(w->name, internal2_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x08, 0x08);
		else if (strnstr(w->name, internal3_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x01, 0x01);
		msm8x16_notifier_call(codec, WCD_EVENT_PRE_MICBIAS_2_ON);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (strnstr(w->name, internal1_text, 30)) {
			snd_soc_update_bits(codec, micb_int_reg, 0xC0, 0x40);
		} else if (strnstr(w->name, internal2_text, 30) &&
			   (!msm8x16_wcd->mbhc.is_hs_inserted)) {
			snd_soc_update_bits(codec, micb_int_reg, 0x18, 0x08);
			snd_soc_update_bits(codec, w->reg, 0x20, 0x20);
		} else if (strnstr(w->name, internal3_text, 30)) {
			snd_soc_update_bits(codec, micb_int_reg, 0x2, 0x0);
		}
		snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_MICB_1_EN,
				0x45, 0x01);
		msm8x16_notifier_call(codec, WCD_EVENT_PRE_MICBIAS_2_OFF);
		break;
	}
	return 0;
}

static void tx_hpf_corner_freq_callback(struct work_struct *work)
{
	struct delayed_work *hpf_delayed_work;
	struct hpf_work *hpf_work;
	struct msm8x16_wcd_priv *msm8x16_wcd;
	struct snd_soc_codec *codec;
	u16 tx_mux_ctl_reg;
	u8 hpf_cut_of_freq;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	msm8x16_wcd = hpf_work->msm8x16_wcd;
	codec = hpf_work->msm8x16_wcd->codec;
	hpf_cut_of_freq = hpf_work->tx_hpf_cut_of_freq;

	tx_mux_ctl_reg = MSM8X16_WCD_A_CDC_TX1_MUX_CTL +
			(hpf_work->decimator - 1) * 32;

	dev_dbg(codec->dev, "%s(): decimator %u hpf_cut_of_freq 0x%x\n",
		 __func__, hpf_work->decimator, (unsigned int)hpf_cut_of_freq);

	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30, hpf_cut_of_freq << 4);
}


#define  TX_MUX_CTL_CUT_OFF_FREQ_MASK	0x30
#define  CF_MIN_3DB_4HZ			0x0
#define  CF_MIN_3DB_75HZ		0x1
#define  CF_MIN_3DB_150HZ		0x2

static int msm8x16_wcd_codec_enable_dec(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	unsigned int decimator;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);
	char *dec_name = NULL;
	char *widget_name = NULL;
	char *temp;
	int ret = 0, i;
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

	if (w->reg == MSM8X16_WCD_A_CDC_CLK_TX_CLK_EN_B1_CTL) {
		dec_reset_reg = MSM8X16_WCD_A_CDC_CLK_TX_RESET_B1_CTL;
		offset = 0;
	} else {
		dev_err(codec->dev, "%s: Error, incorrect dec\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	tx_vol_ctl_reg = MSM8X16_WCD_A_CDC_TX1_VOL_CTL_CFG +
			 32 * (decimator - 1);
	tx_mux_ctl_reg = MSM8X16_WCD_A_CDC_TX1_MUX_CTL +
			  32 * (decimator - 1);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Enableable TX digital mute */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x01);
		for (i = 0; i < NUM_DECIMATORS; i++) {
			if (decimator == i + 1)
				msm8x16_wcd->dec_active[i] = true;
		}

		dec_hpf_cut_of_freq = snd_soc_read(codec, tx_mux_ctl_reg);

		dec_hpf_cut_of_freq = (dec_hpf_cut_of_freq & 0x30) >> 4;

		tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq =
			dec_hpf_cut_of_freq;

		if ((dec_hpf_cut_of_freq != CF_MIN_3DB_150HZ)) {

			/* set cut of freq to CF_MIN_3DB_150HZ (0x1); */
			snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30,
					    CF_MIN_3DB_150HZ << 4);
		}

		break;
	case SND_SOC_DAPM_POST_PMU:
		/* enable HPF */
		snd_soc_update_bits(codec, tx_mux_ctl_reg , 0x08, 0x00);

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
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x00);
		msleep(20);
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x01);
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x08, 0x08);
		cancel_delayed_work_sync(&tx_hpf_work[decimator - 1].dwork);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, dec_reset_reg, 1 << w->shift,
			1 << w->shift);
		snd_soc_update_bits(codec, dec_reset_reg, 1 << w->shift, 0x0);
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x08, 0x08);
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30,
			(tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq) << 4);
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x00);
		for (i = 0; i < NUM_DECIMATORS; i++) {
			if (decimator == i + 1)
				msm8x16_wcd->dec_active[i] = false;
		}
		break;
	}
out:
	kfree(widget_name);
	return ret;
}

static int msm8x16_wcd_codec_enable_interpolator(struct snd_soc_dapm_widget *w,
						 struct snd_kcontrol *kcontrol,
						 int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		/* apply the digital gain after the interpolator is enabled*/
		if ((w->shift) < ARRAY_SIZE(rx_digital_gain_reg))
			snd_soc_write(codec,
				  rx_digital_gain_reg[w->shift],
				  snd_soc_read(codec,
				  rx_digital_gain_reg[w->shift])
				  );
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_CLK_RX_RESET_CTL,
			1 << w->shift, 1 << w->shift);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_CLK_RX_RESET_CTL,
			1 << w->shift, 0x0);
		/*
		 * disable the mute enabled during the PMD of this device
		 */
		if (msm8x16_wcd->mute_mask & HPHL_PA_DISABLE) {
			pr_debug("disabling HPHL mute\n");
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX1_B6_CTL, 0x01, 0x00);
			msm8x16_wcd->mute_mask &= ~(HPHL_PA_DISABLE);
		}
		if (msm8x16_wcd->mute_mask & HPHR_PA_DISABLE) {
			pr_debug("disabling HPHR mute\n");
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX2_B6_CTL, 0x01, 0x00);
			msm8x16_wcd->mute_mask &= ~(HPHR_PA_DISABLE);
		}
		if (msm8x16_wcd->mute_mask & SPKR_PA_DISABLE) {
			pr_debug("disabling SPKR mute\n");
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX3_B6_CTL, 0x01, 0x00);
			msm8x16_wcd->mute_mask &= ~(SPKR_PA_DISABLE);
		}
		if (msm8x16_wcd->mute_mask & EAR_PA_DISABLE) {
			pr_debug("disabling EAR mute\n");
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX1_B6_CTL, 0x01, 0x00);
			msm8x16_wcd->mute_mask &= ~(EAR_PA_DISABLE);
		}
	}
	return 0;
}


/* The register address is the same as other codec so it can use resmgr */
static int msm8x16_wcd_codec_enable_rx_bias(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		msm8x16_wcd->rx_bias_count++;
		if (msm8x16_wcd->rx_bias_count == 1)
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_RX_COM_BIAS_DAC,
					0x81, 0x81);
		break;
	case SND_SOC_DAPM_POST_PMD:
		msm8x16_wcd->rx_bias_count--;
		if (msm8x16_wcd->rx_bias_count == 0)
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_RX_COM_BIAS_DAC,
					0x81, 0x00);
		break;
	}
	dev_dbg(codec->dev, "%s rx_bias_count = %d\n",
			__func__, msm8x16_wcd->rx_bias_count);
	return 0;
}

static int msm8x16_wcd_hphl_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_HPH_L_PA_DAC_CTL, 0x02, 0x02);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL, 0x01, 0x01);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL, 0x02, 0x02);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_HPH_L_PA_DAC_CTL, 0x02, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL, 0x02, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL, 0x01, 0x00);
		break;
	}
	return 0;
}

static int msm8x16_wcd_hphr_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_HPH_R_PA_DAC_CTL, 0x02, 0x02);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL, 0x02, 0x02);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL, 0x01, 0x01);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_HPH_R_PA_DAC_CTL, 0x02, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL, 0x01, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL, 0x02, 0x00);
		break;
	}
	return 0;
}

static int msm8x16_wcd_hph_pa_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: %s event = %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (w->shift == 5)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_L_TEST, 0x04, 0x04);
		else if (w->shift == 4)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_R_TEST, 0x04, 0x04);
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_NCP_FBCTRL, 0x20, 0x20);
		break;

	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_DIGITAL_HDRIVE_CTL, 0x03, 0x03);
		usleep_range(4000, 4100);
		if (w->shift == 5)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX1_B6_CTL, 0x01, 0x00);
		else if (w->shift == 4)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX2_B6_CTL, 0x01, 0x00);
		usleep_range(10000, 10100);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		if (w->shift == 5) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX1_B6_CTL, 0x01, 0x01);
			msleep(20);
			msm8x16_wcd->mute_mask |= HPHL_PA_DISABLE;
		} else if (w->shift == 4) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX2_B6_CTL, 0x01, 0x01);
			msleep(20);
			msm8x16_wcd->mute_mask |= HPHR_PA_DISABLE;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (w->shift == 5) {
			clear_bit(WCD_MBHC_HPHL_PA_OFF_ACK,
				&msm8x16_wcd->mbhc.hph_pa_dac_state);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_L_TEST, 0x04, 0x00);

			msm8x16_notifier_call(codec,
					WCD_EVENT_POST_HPHL_PA_OFF);
		} else if (w->shift == 4) {
			clear_bit(WCD_MBHC_HPHR_PA_OFF_ACK,
				&msm8x16_wcd->mbhc.hph_pa_dac_state);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_R_TEST, 0x04, 0x00);

			msm8x16_notifier_call(codec,
					WCD_EVENT_POST_HPHR_PA_OFF);
		}
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_NCP_FBCTRL, 0x20, 0x00);
		usleep_range(4000, 4100);

		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL, 0x40, 0x40);
		dev_dbg(codec->dev,
			"%s: sleep 10 ms after %s PA disable.\n", __func__,
			w->name);
		usleep_range(10000, 10100);
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

	/* RDAC Connections */
	{"HPHR DAC", NULL, "RDAC2 MUX"},
	{"RDAC2 MUX", "RX1", "RX1 CHAIN"},
	{"RDAC2 MUX", "RX2", "RX2 CHAIN"},

	/* Earpiece (RX MIX1) */
	{"EAR", NULL, "EAR_S"},
	{"EAR_S", "Switch", "EAR PA"},
	{"EAR PA", NULL, "RX_BIAS"},
	{"EAR PA", NULL, "HPHL DAC"},
	{"EAR PA", NULL, "HPHR DAC"},
	{"EAR PA", NULL, "EAR CP"},

	/* Headset (RX MIX1 and RX MIX2) */
	{"HEADPHONE", NULL, "HPHL PA"},
	{"HEADPHONE", NULL, "HPHR PA"},

	{"HPHL PA", NULL, "HPHL"},
	{"HPHR PA", NULL, "HPHR"},
	{"HPHL", "Switch", "HPHL DAC"},
	{"HPHR", "Switch", "HPHR DAC"},
	{"HPHL PA", NULL, "CP"},
	{"HPHL PA", NULL, "RX_BIAS"},
	{"HPHR PA", NULL, "CP"},
	{"HPHR PA", NULL, "RX_BIAS"},
	{"HPHL DAC", NULL, "RX1 CHAIN"},

	{"SPK_OUT", NULL, "SPK PA"},
	{"SPK PA", NULL, "SPK_RX_BIAS"},
	{"SPK PA", NULL, "SPK DAC"},
	{"SPK DAC", "Switch", "RX3 CHAIN"},

	{"RX1 CHAIN", NULL, "RX1 CLK"},
	{"RX2 CHAIN", NULL, "RX2 CLK"},
	{"RX3 CHAIN", NULL, "RX3 CLK"},
	{"RX1 CHAIN", NULL, "RX1 MIX2"},
	{"RX2 CHAIN", NULL, "RX2 MIX2"},
	{"RX1 CHAIN", NULL, "RX1 MIX1"},
	{"RX2 CHAIN", NULL, "RX2 MIX1"},
	{"RX3 CHAIN", NULL, "RX3 MIX1"},

	{"RX1 MIX1", NULL, "RX1 MIX1 INP1"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP2"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP3"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP1"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP2"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP1"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP2"},
	{"RX1 MIX2", NULL, "RX1 MIX2 INP1"},
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
	{"DEC1 MUX", "ADC3", "ADC3"},
	{"DEC1 MUX", NULL, "CDC_CONN"},

	{"DEC2 MUX", "DMIC1", "DMIC1"},
	{"DEC2 MUX", "DMIC2", "DMIC2"},
	{"DEC2 MUX", "ADC1", "ADC1"},
	{"DEC2 MUX", "ADC2", "ADC2"},
	{"DEC2 MUX", "ADC3", "ADC3"},
	{"DEC2 MUX", NULL, "CDC_CONN"},

	/* ADC Connections */
	{"ADC2", NULL, "ADC2 MUX"},
	{"ADC3", NULL, "ADC2 MUX"},
	{"ADC2 MUX", "INP2", "ADC2_INP2"},
	{"ADC2 MUX", "INP3", "ADC2_INP3"},

	{"ADC1", NULL, "AMIC1"},
	{"ADC2_INP2", NULL, "AMIC2"},
	{"ADC2_INP3", NULL, "AMIC3"},

	/* TODO: Fix this */
	{"IIR1", NULL, "IIR1 INP1 MUX"},
	{"IIR1 INP1 MUX", "DEC1", "DEC1 MUX"},
	{"IIR1 INP1 MUX", "DEC2", "DEC2 MUX"},
	{"MIC BIAS Internal1", NULL, "INT_LDO_H"},
	{"MIC BIAS Internal2", NULL, "INT_LDO_H"},
	{"MIC BIAS External", NULL, "INT_LDO_H"},
	{"MIC BIAS External2", NULL, "INT_LDO_H"},
	{"MIC BIAS Internal1", NULL, "MICBIAS_REGULATOR"},
	{"MIC BIAS Internal2", NULL, "MICBIAS_REGULATOR"},
	{"MIC BIAS External", NULL, "MICBIAS_REGULATOR"},
	{"MIC BIAS External2", NULL, "MICBIAS_REGULATOR"},
};

static int msm8x16_wcd_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	dev_dbg(dai->codec->dev, "%s(): substream = %s  stream = %d\n",
		__func__,
		substream->name, substream->stream);
	return 0;
}

static void msm8x16_wcd_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	dev_dbg(dai->codec->dev,
		"%s(): substream = %s  stream = %d\n" , __func__,
		substream->name, substream->stream);
}

static int msm8x16_wcd_codec_enable_clock_block(struct snd_soc_codec *codec,
						int enable)
{
	struct msm8916_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(codec->card);
	if (enable) {
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_CLK_MCLK_CTL, 0x01, 0x01);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_CLK_PDM_CTL, 0x03, 0x03);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MASTER_BIAS_CTL, 0x30, 0x30);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_RST_CTL, 0x80, 0x80);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_TOP_CLK_CTL, 0x0C, 0x0C);
		if (pdata->mclk_freq == 12288000)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_TOP_CTL, 0x01, 0x00);
		else if (pdata->mclk_freq == 9600000)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_TOP_CTL, 0x01, 0x01);
	} else {
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_TOP_CLK_CTL, 0x0C, 0x00);
		snd_soc_update_bits(codec, MSM8X16_WCD_A_CDC_CLK_PDM_CTL,
				    0x03, 0x00);
	}
	return 0;
}

int msm8x16_wcd_mclk_enable(struct snd_soc_codec *codec,
			    int mclk_enable, bool dapm)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: mclk_enable = %u, dapm = %d\n",
		__func__, mclk_enable, dapm);
	if (mclk_enable) {
		msm8x16_wcd->mclk_enabled = true;
		msm8x16_wcd_codec_enable_clock_block(codec, 1);
	} else {
		if (!msm8x16_wcd->mclk_enabled) {
			dev_err(codec->dev, "Error, MCLK already diabled\n");
			return -EINVAL;
		}
		msm8x16_wcd->mclk_enabled = false;
		msm8x16_wcd_codec_enable_clock_block(codec, 0);
	}
	return 0;
}

static int msm8x16_wcd_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int msm8x16_wcd_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int msm8x16_wcd_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)

{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int msm8x16_wcd_get_channel_map(struct snd_soc_dai *dai,
				 unsigned int *tx_num, unsigned int *tx_slot,
				 unsigned int *rx_num, unsigned int *rx_slot)

{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int msm8x16_wcd_set_interpolator_rate(struct snd_soc_dai *dai,
	u8 rx_fs_rate_reg_val, u32 sample_rate)
{
	return 0;
}

static int msm8x16_wcd_set_decimator_rate(struct snd_soc_dai *dai,
	u8 tx_fs_rate_reg_val, u32 sample_rate)
{
	return 0;
}

static int msm8x16_wcd_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	u8 tx_fs_rate, rx_fs_rate;
	int ret;

	dev_dbg(dai->codec->dev,
		"%s: dai_name = %s DAI-ID %x rate %d num_ch %d format %d\n",
		__func__, dai->name, dai->id, params_rate(params),
		params_channels(params), params_format(params));

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
		ret = msm8x16_wcd_set_decimator_rate(dai, tx_fs_rate,
					       params_rate(params));
		if (ret < 0) {
			dev_err(dai->codec->dev,
				"%s: set decimator rate failed %d\n", __func__,
				ret);
			return ret;
		}
		break;
	case SNDRV_PCM_STREAM_PLAYBACK:
		ret = msm8x16_wcd_set_interpolator_rate(dai, rx_fs_rate,
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
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		snd_soc_update_bits(dai->codec,
				MSM8X16_WCD_A_CDC_CLK_RX_I2S_CTL, 0x20, 0x20);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		snd_soc_update_bits(dai->codec,
				MSM8X16_WCD_A_CDC_CLK_RX_I2S_CTL, 0x20, 0x00);
		break;
	default:
		dev_err(dai->codec->dev, "%s: wrong format selected\n",
				__func__);
		return -EINVAL;
		break;
	}

	return 0;
}

int msm8x16_wcd_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = NULL;
	u16 tx_vol_ctl_reg = 0;
	u8 decimator = 0, i;
	struct msm8x16_wcd_priv *msm8x16_wcd;

	pr_debug("%s: Digital Mute val = %d\n", __func__, mute);

	if (!dai || !dai->codec) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}
	codec = dai->codec;
	msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if (dai->id != AIF1_CAP) {
		dev_dbg(codec->dev, "%s: Not capture use case skip\n",
		__func__);
		return 0;
	}

	mute = (mute) ? 1 : 0;
	if (!mute && msm8x16_wcd->mbhc.current_plug == MBHC_PLUG_TYPE_HEADSET) {
		/*
		 * 23 ms is an emperical value for the mute time
		 * that was arrived by checking the pop level
		 * to be inaudible
		 */
		msleep(23);
	}

	for (i = 0; i < NUM_DECIMATORS; i++) {
		if (msm8x16_wcd->dec_active[i])
			decimator = i + 1;
		if (decimator && decimator <= NUM_DECIMATORS) {
			pr_debug("%s: Mute = %d Decimator = %d", __func__,
					mute, decimator);
			tx_vol_ctl_reg = MSM8X16_WCD_A_CDC_TX1_VOL_CTL_CFG +
				32 * (decimator - 1);
			snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, mute);
		}
		decimator = 0;
	}
	return 0;
}

static struct snd_soc_dai_ops msm8x16_wcd_dai_ops = {
	.startup = msm8x16_wcd_startup,
	.shutdown = msm8x16_wcd_shutdown,
	.hw_params = msm8x16_wcd_hw_params,
	.set_sysclk = msm8x16_wcd_set_dai_sysclk,
	.set_fmt = msm8x16_wcd_set_dai_fmt,
	.set_channel_map = msm8x16_wcd_set_channel_map,
	.get_channel_map = msm8x16_wcd_get_channel_map,
	.digital_mute = msm8x16_wcd_digital_mute,
};

static struct snd_soc_dai_driver msm8x16_wcd_i2s_dai[] = {
	{
		.name = "msm8x16_wcd_i2s_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = MSM8X16_WCD_RATES,
			.formats = MSM8X16_WCD_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 3,
		},
		.ops = &msm8x16_wcd_dai_ops,
	},
	{
		.name = "msm8x16_wcd_i2s_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = MSM8X16_WCD_RATES,
			.formats = MSM8X16_WCD_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &msm8x16_wcd_dai_ops,
	},
};

static int msm8x16_wcd_codec_enable_rx_chain(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(w->codec->dev,
			"%s: PMU:Sleeping 20ms after disabling mute\n",
			__func__);
		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(w->codec->dev,
			"%s: PMD:Sleeping 20ms after disabling mute\n",
			__func__);
		snd_soc_update_bits(codec, w->reg,
			    1 << w->shift, 0x00);
		msleep(20);
		break;
	}
	return 0;
}

static int msm8x16_wcd_codec_enable_ear_pa(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(w->codec->dev,
			"%s: Sleeping 20ms after select EAR PA\n",
			__func__);
		snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_RX_EAR_CTL,
			    0x80, 0x80);
		break;
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(w->codec->dev,
			"%s: Sleeping 20ms after enabling EAR PA\n",
			__func__);
		snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_RX_EAR_CTL,
			    0x40, 0x40);
		usleep_range(7000, 7100);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_RX1_B6_CTL, 0x01, 0x00);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_RX1_B6_CTL, 0x01, 0x01);
		msleep(20);
		msm8x16_wcd->mute_mask |= EAR_PA_DISABLE;
		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(w->codec->dev,
			"%s: Sleeping 7ms after disabling EAR PA\n",
			__func__);
		snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_RX_EAR_CTL,
			    0x40, 0x00);
		usleep_range(7000, 7100);
		/*
		 * Reset pa select bit from ear to hph after ear pa
		 * is disabled to reduce ear turn off pop
		 */
		snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_RX_EAR_CTL,
			    0x80, 0x00);
		break;
	}
	return 0;
}

static const struct snd_soc_dapm_widget msm8x16_wcd_dapm_widgets[] = {
	/*RX stuff */
	SND_SOC_DAPM_OUTPUT("EAR"),

	SND_SOC_DAPM_PGA_E("EAR PA", SND_SOC_NOPM,
			0, 0, NULL, 0, msm8x16_wcd_codec_enable_ear_pa,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("EAR_S", SND_SOC_NOPM, 0, 0,
		ear_pa_switch, ARRAY_SIZE(ear_pa_switch)),

	SND_SOC_DAPM_AIF_IN("I2S RX1", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("I2S RX2", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("I2S RX3", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_SUPPLY("INT_LDO_H", SND_SOC_NOPM, 1, 0, NULL, 0),

	SND_SOC_DAPM_OUTPUT("HEADPHONE"),
	SND_SOC_DAPM_PGA_E("HPHL PA", MSM8X16_WCD_A_ANALOG_RX_HPH_CNP_EN,
		5, 0, NULL, 0,
		msm8x16_wcd_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_VIRT_MUX("HPHL", SND_SOC_NOPM, 0, 0,
		hphl_mux),

	SND_SOC_DAPM_MIXER_E("HPHL DAC",
		MSM8X16_WCD_A_ANALOG_RX_HPH_L_PA_DAC_CTL, 3, 0, NULL,
		0, msm8x16_wcd_hphl_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("HPHR PA", MSM8X16_WCD_A_ANALOG_RX_HPH_CNP_EN,
		4, 0, NULL, 0,
		msm8x16_wcd_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_VIRT_MUX("HPHR", SND_SOC_NOPM, 0, 0,
		hphr_mux),

	SND_SOC_DAPM_MIXER_E("HPHR DAC",
		MSM8X16_WCD_A_ANALOG_RX_HPH_R_PA_DAC_CTL, 3, 0, NULL,
		0, msm8x16_wcd_hphr_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("SPK DAC", SND_SOC_NOPM, 0, 0,
		spkr_switch, ARRAY_SIZE(spkr_switch)),

	/* Speaker */
	SND_SOC_DAPM_OUTPUT("SPK_OUT"),

	SND_SOC_DAPM_PGA_E("SPK PA", MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL,
			6, 0 , NULL, 0, msm8x16_wcd_codec_enable_spk_pa,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER_E("RX1 MIX1",
			MSM8X16_WCD_A_CDC_CLK_RX_B1_CTL, 0, 0, NULL, 0,
			msm8x16_wcd_codec_enable_interpolator,
			SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX2 MIX1",
			MSM8X16_WCD_A_CDC_CLK_RX_B1_CTL, 1, 0, NULL, 0,
			msm8x16_wcd_codec_enable_interpolator,
			SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER_E("RX1 MIX2",
		MSM8X16_WCD_A_CDC_CLK_RX_B1_CTL, 0, 0, NULL,
		0, msm8x16_wcd_codec_enable_interpolator,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX2 MIX2",
		MSM8X16_WCD_A_CDC_CLK_RX_B1_CTL, 1, 0, NULL,
		0, msm8x16_wcd_codec_enable_interpolator,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX3 MIX1",
		MSM8X16_WCD_A_CDC_CLK_RX_B1_CTL, 2, 0, NULL,
		0, msm8x16_wcd_codec_enable_interpolator,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("RX1 CLK", MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
		0, 0, NULL, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("RX2 CLK", MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
		1, 0, NULL, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("RX3 CLK", MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
		2, 0, msm8x16_wcd_codec_enable_dig_clk, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX1 CHAIN", MSM8X16_WCD_A_CDC_RX1_B6_CTL, 0, 0,
		NULL, 0,
		msm8x16_wcd_codec_enable_rx_chain,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX2 CHAIN", MSM8X16_WCD_A_CDC_RX2_B6_CTL, 0, 0,
		NULL, 0,
		msm8x16_wcd_codec_enable_rx_chain,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX3 CHAIN", MSM8X16_WCD_A_CDC_RX3_B6_CTL, 0, 0,
		NULL, 0,
		msm8x16_wcd_codec_enable_rx_chain,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

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
	SND_SOC_DAPM_MUX("RX2 MIX1 INP3", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp3_mux),

	SND_SOC_DAPM_MUX("RX3 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX3 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX3 MIX1 INP3", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp3_mux),

	SND_SOC_DAPM_MUX("RX1 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx1_mix2_inp1_mux),
	SND_SOC_DAPM_MUX("RX2 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx2_mix2_inp1_mux),

	SND_SOC_DAPM_SUPPLY("MICBIAS_REGULATOR", SND_SOC_NOPM,
		ON_DEMAND_MICBIAS, 0,
		msm8x16_wcd_codec_enable_on_demand_supply,
		SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("CP", MSM8X16_WCD_A_ANALOG_NCP_EN, 0, 0,
		msm8x16_wcd_codec_enable_charge_pump, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("EAR CP", MSM8X16_WCD_A_ANALOG_NCP_EN, 4, 0,
		msm8x16_wcd_codec_enable_charge_pump, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("RX_BIAS", SND_SOC_NOPM,
		0, 0, msm8x16_wcd_codec_enable_rx_bias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("SPK_RX_BIAS",
		SND_SOC_NOPM, 0, 0,
		msm8x16_wcd_codec_enable_rx_bias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	/* TX */

	SND_SOC_DAPM_SUPPLY("CDC_CONN", MSM8X16_WCD_A_CDC_CLK_OTHR_CTL,
		2, 0, NULL, 0),


	SND_SOC_DAPM_INPUT("AMIC1"),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS Internal1",
		MSM8X16_WCD_A_ANALOG_MICB_1_EN, 7, 0,
		msm8x16_wcd_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS Internal2",
		MSM8X16_WCD_A_ANALOG_MICB_2_EN, 7, 0,
		msm8x16_wcd_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS Internal3",
		MSM8X16_WCD_A_ANALOG_MICB_1_EN, 7, 0,
		msm8x16_wcd_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC1", NULL, MSM8X16_WCD_A_ANALOG_TX_1_EN, 7, 0,
		msm8x16_wcd_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC2_INP2",
		NULL, MSM8X16_WCD_A_ANALOG_TX_2_EN, 7, 0,
		msm8x16_wcd_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC2_INP3",
		NULL, MSM8X16_WCD_A_ANALOG_TX_3_EN, 7, 0,
		msm8x16_wcd_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("ADC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("ADC3", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_VIRT_MUX("ADC2 MUX", SND_SOC_NOPM, 0, 0,
		&tx_adc2_mux),

	SND_SOC_DAPM_MICBIAS("MIC BIAS External",
		MSM8X16_WCD_A_ANALOG_MICB_1_EN, 7, 0),

	SND_SOC_DAPM_MICBIAS("MIC BIAS External2",
		MSM8X16_WCD_A_ANALOG_MICB_2_EN, 7, 0),

	SND_SOC_DAPM_INPUT("AMIC3"),

	SND_SOC_DAPM_MUX_E("DEC1 MUX",
		MSM8X16_WCD_A_CDC_CLK_TX_CLK_EN_B1_CTL, 0, 0,
		&dec1_mux, msm8x16_wcd_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC2 MUX",
		MSM8X16_WCD_A_CDC_CLK_TX_CLK_EN_B1_CTL, 1, 0,
		&dec2_mux, msm8x16_wcd_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("RDAC2 MUX", SND_SOC_NOPM, 0, 0, &rdac2_mux),

	SND_SOC_DAPM_INPUT("AMIC2"),

	SND_SOC_DAPM_AIF_OUT("I2S TX1", "AIF1 Capture", 0, SND_SOC_NOPM,
		0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S TX2", "AIF1 Capture", 0, SND_SOC_NOPM,
		0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S TX3", "AIF1 Capture", 0, SND_SOC_NOPM,
		0, 0),

	/* Digital Mic Inputs */
	SND_SOC_DAPM_ADC_E("DMIC1", NULL, SND_SOC_NOPM, 0, 0,
		msm8x16_wcd_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC2", NULL, SND_SOC_NOPM, 0, 0,
		msm8x16_wcd_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	/* Sidetone */
	SND_SOC_DAPM_MUX("IIR1 INP1 MUX", SND_SOC_NOPM, 0, 0, &iir1_inp1_mux),
	SND_SOC_DAPM_PGA("IIR1",
		MSM8X16_WCD_A_CDC_CLK_SD_CTL, 0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("RX_I2S_CLK",
		MSM8X16_WCD_A_CDC_CLK_RX_I2S_CTL,	4, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("TX_I2S_CLK",
		MSM8X16_WCD_A_CDC_CLK_TX_I2S_CTL, 4, 0, NULL, 0),
};

static const struct msm8x16_wcd_reg_mask_val msm8x16_wcd_reg_defaults[] = {
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 0x03),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_CURRENT_LIMIT, 0x82),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_OCP_CTL, 0xE1),
};

static const struct msm8x16_wcd_reg_mask_val msm8x16_wcd_reg_defaults_2_0[] = {
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_DIGITAL_PERPH_RESET_CTL3, 0x0F),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_TX_1_2_OPAMP_BIAS, 0x4B),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_NCP_FBCTRL, 0x28),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL, 0x69),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DRV_DBG, 0x01),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_BOOST_EN_CTL, 0x5F),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SLOPE_COMP_IP_ZERO, 0x88),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SEC_ACCESS, 0xA5),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_PERPH_RESET_CTL3, 0x0F),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_CURRENT_LIMIT, 0x82),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 0x03),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_OCP_CTL, 0xE1),
};

static void msm8x16_wcd_update_reg_defaults(struct snd_soc_codec *codec)
{
	u32 i;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if (TOMBAK_IS_1_0(msm8x16_wcd->pmic_rev)) {
		for (i = 0; i < ARRAY_SIZE(msm8x16_wcd_reg_defaults); i++)
			snd_soc_write(codec, msm8x16_wcd_reg_defaults[i].reg,
					msm8x16_wcd_reg_defaults[i].val);
	} else {
		for (i = 0; i < ARRAY_SIZE(msm8x16_wcd_reg_defaults_2_0); i++)
			snd_soc_write(codec,
				msm8x16_wcd_reg_defaults_2_0[i].reg,
				msm8x16_wcd_reg_defaults_2_0[i].val);
	}
}

static const struct msm8x16_wcd_reg_mask_val
	msm8x16_wcd_codec_reg_init_val[] = {

	/* Initialize current threshold to 350MA
	 * number of wait and run cycles to 4096
	 */
	{MSM8X16_WCD_A_ANALOG_RX_COM_OCP_CTL, 0xFF, 0xD1},
	{MSM8X16_WCD_A_ANALOG_RX_COM_OCP_COUNT, 0xFF, 0xFF},
};

static void msm8x16_wcd_codec_init_reg(struct snd_soc_codec *codec)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(msm8x16_wcd_codec_reg_init_val); i++)
		snd_soc_update_bits(codec,
				    msm8x16_wcd_codec_reg_init_val[i].reg,
				    msm8x16_wcd_codec_reg_init_val[i].mask,
				    msm8x16_wcd_codec_reg_init_val[i].val);
}

static int msm8x16_wcd_bringup(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, MSM8X16_WCD_A_DIGITAL_PERPH_RESET_CTL4, 0x01);
	snd_soc_write(codec, MSM8X16_WCD_A_ANALOG_PERPH_RESET_CTL4, 0x01);
	return 0;
}

static struct regulator *wcd8x16_wcd_codec_find_regulator(
				const struct msm8x16_wcd *msm8x16,
				const char *name)
{
	int i;

	for (i = 0; i < msm8x16->num_of_supplies; i++) {
		if (msm8x16->supplies[i].supply &&
		    !strcmp(msm8x16->supplies[i].supply, name))
			return msm8x16->supplies[i].consumer;
	}

	dev_err(msm8x16->dev, "Error: regulator not found\n");
	return NULL;
}

static int msm8x16_wcd_device_down(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s: device down!\n", __func__);
	msm8x16_wcd_write(codec,
		MSM8X16_WCD_A_ANALOG_TX_1_EN, 0x3);
	msm8x16_wcd_write(codec,
		MSM8X16_WCD_A_ANALOG_TX_2_EN, 0x3);
	msm8x16_wcd_write(codec,
		MSM8X16_WCD_A_ANALOG_RX_HPH_L_PA_DAC_CTL, 0x20);
	msm8x16_wcd_write(codec,
		MSM8X16_WCD_A_ANALOG_RX_HPH_R_PA_DAC_CTL, 0x20);
	msm8x16_wcd_write(codec,
		MSM8X16_WCD_A_ANALOG_RX_EAR_CTL, 0x12);
	msm8x16_wcd_write(codec,
		MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 0x93);

	msm8x16_wcd_write(codec, MSM8X16_WCD_A_DIGITAL_PERPH_RESET_CTL4, 0x1);
	msm8x16_wcd_write(codec, MSM8X16_WCD_A_ANALOG_PERPH_RESET_CTL4, 0x1);
	snd_soc_card_change_online_state(codec->card, 0);
	return 0;
}

static int msm8x16_wcd_device_up(struct snd_soc_codec *codec)
{
	struct msm8x16_wcd_priv *msm8x16_wcd_priv =
		snd_soc_codec_get_drvdata(codec);
	u32 reg;
	dev_dbg(codec->dev, "%s: device up!\n", __func__);

	mutex_lock(&codec->mutex);

	for (reg = 0; reg < ARRAY_SIZE(msm8x16_wcd_reset_reg_defaults); reg++)
		if (msm8x16_wcd_reg_readable[reg])
			msm8x16_wcd_write(codec,
				reg, msm8x16_wcd_reset_reg_defaults[reg]);

	if (codec->reg_def_copy) {
		pr_debug("%s: Update ASOC cache", __func__);
		kfree(codec->reg_cache);
		codec->reg_cache = kmemdup(codec->reg_def_copy,
						codec->reg_size, GFP_KERNEL);
		if (!codec->reg_cache) {
			pr_err("%s: Cache update failed!\n", __func__);
			mutex_unlock(&codec->mutex);
			return -ENOMEM;
		}
	}

	snd_soc_card_change_online_state(codec->card, 1);
	/* delay is required to make sure sound card state updated */
	usleep_range(5000, 5100);

	msm8x16_wcd_bringup(codec);
	msm8x16_wcd_codec_init_reg(codec);
	msm8x16_wcd_update_reg_defaults(codec);

	wcd_mbhc_stop(&msm8x16_wcd_priv->mbhc);
	wcd_mbhc_start(&msm8x16_wcd_priv->mbhc,
			msm8x16_wcd_priv->mbhc.mbhc_cfg);

	mutex_unlock(&codec->mutex);

	return 0;
}

static int modem_state_callback(struct notifier_block *nb, unsigned long value,
			       void *priv)
{
	bool timedout;
	unsigned long timeout;

	if (value == SUBSYS_BEFORE_SHUTDOWN)
		msm8x16_wcd_device_down(registered_codec);
	else if (value == SUBSYS_AFTER_POWERUP) {

		dev_dbg(registered_codec->dev,
			"ADSP is about to power up. bring up codec\n");

		if (!q6core_is_adsp_ready()) {
			dev_dbg(registered_codec->dev,
				"ADSP isn't ready\n");
			timeout = jiffies +
				  msecs_to_jiffies(ADSP_STATE_READY_TIMEOUT_MS);
			while (!(timedout = time_after(jiffies, timeout))) {
				if (!q6core_is_adsp_ready()) {
					dev_dbg(registered_codec->dev,
						"ADSP isn't ready\n");
				} else {
					dev_dbg(registered_codec->dev,
						"ADSP is ready\n");
					break;
				}
			}
		} else {
			dev_dbg(registered_codec->dev,
				"%s: DSP is ready\n", __func__);
		}

		msm8x16_wcd_device_up(registered_codec);
	}
	return NOTIFY_OK;
}

static struct notifier_block modem_state_notifier_block = {
	.notifier_call = modem_state_callback,
	.priority = -INT_MAX,
};

int msm8x16_wcd_hs_detect(struct snd_soc_codec *codec,
		    struct wcd_mbhc_config *mbhc_cfg)
{
	struct msm8x16_wcd_priv *msm8x16_wcd_priv =
		snd_soc_codec_get_drvdata(codec);

	return wcd_mbhc_start(&msm8x16_wcd_priv->mbhc, mbhc_cfg);
}
EXPORT_SYMBOL(msm8x16_wcd_hs_detect);

void msm8x16_wcd_hs_detect_exit(struct snd_soc_codec *codec)
{
	struct msm8x16_wcd_priv *msm8x16_wcd_priv =
		snd_soc_codec_get_drvdata(codec);

	wcd_mbhc_stop(&msm8x16_wcd_priv->mbhc);
}
EXPORT_SYMBOL(msm8x16_wcd_hs_detect_exit);

static int msm8x16_wcd_codec_probe(struct snd_soc_codec *codec)
{
	struct msm8x16_wcd_priv *msm8x16_wcd_priv;
	struct msm8x16_wcd *msm8x16_wcd;
	int i;

	dev_dbg(codec->dev, "%s()\n", __func__);

	msm8x16_wcd_priv = kzalloc(sizeof(struct msm8x16_wcd_priv), GFP_KERNEL);
	if (!msm8x16_wcd_priv) {
		dev_err(codec->dev, "Failed to allocate private data\n");
		return -ENOMEM;
	}

	for (i = 0; i < NUM_DECIMATORS; i++) {
		tx_hpf_work[i].msm8x16_wcd = msm8x16_wcd_priv;
		tx_hpf_work[i].decimator = i + 1;
		INIT_DELAYED_WORK(&tx_hpf_work[i].dwork,
			tx_hpf_corner_freq_callback);
	}

	codec->control_data = dev_get_drvdata(codec->dev);
	snd_soc_codec_set_drvdata(codec, msm8x16_wcd_priv);
	msm8x16_wcd_priv->codec = codec;

	/* codec resmgr module init */
	msm8x16_wcd = codec->control_data;
	msm8x16_wcd->dig_base = ioremap(MSM8X16_DIGITAL_CODEC_BASE_ADDR,
				  MSM8X16_DIGITAL_CODEC_REG_SIZE);
	if (msm8x16_wcd->dig_base == NULL) {
		dev_err(codec->dev, "%s ioremap failed", __func__);
		kfree(msm8x16_wcd_priv);
		return -ENOMEM;
	}
	msm8x16_wcd_priv->pmic_rev = snd_soc_read(codec,
					MSM8X16_WCD_A_DIGITAL_REVISION1);
	dev_dbg(codec->dev, "%s :PMIC REV: %d", __func__,
					msm8x16_wcd_priv->pmic_rev);
	msm8x16_wcd_bringup(codec);
	msm8x16_wcd_codec_init_reg(codec);
	msm8x16_wcd_update_reg_defaults(codec);

	wcd9xxx_spmi_set_codec(codec);

	msm8x16_wcd_priv->on_demand_list[ON_DEMAND_MICBIAS].supply =
				wcd8x16_wcd_codec_find_regulator(
				codec->control_data,
				on_demand_supply_name[ON_DEMAND_MICBIAS]);
	atomic_set(&msm8x16_wcd_priv->on_demand_list[ON_DEMAND_MICBIAS].ref, 0);

	BLOCKING_INIT_NOTIFIER_HEAD(&msm8x16_wcd_priv->notifier);

	wcd_mbhc_init(&msm8x16_wcd_priv->mbhc, codec, &mbhc_cb, &intr_ids,
			false);

	msm8x16_wcd_priv->mclk_enabled = false;
	msm8x16_wcd_priv->clock_active = false;
	msm8x16_wcd_priv->config_mode_active = false;

	registered_codec = codec;
	modem_state_notifier =
	    subsys_notif_register_notifier("modem",
					   &modem_state_notifier_block);
	if (!modem_state_notifier) {
		dev_err(codec->dev, "Failed to register modem state notifier\n"
			);
		iounmap(msm8x16_wcd->dig_base);
		kfree(msm8x16_wcd_priv);
		registered_codec = NULL;
		return -ENOMEM;
	}
	return 0;
}

static int msm8x16_wcd_codec_remove(struct snd_soc_codec *codec)
{
	struct msm8x16_wcd_priv *msm8x16_wcd_priv =
					snd_soc_codec_get_drvdata(codec);
	struct msm8x16_wcd *msm8x16_wcd;

	msm8x16_wcd = codec->control_data;
	msm8x16_wcd_priv->on_demand_list[ON_DEMAND_MICBIAS].supply = NULL;
	atomic_set(&msm8x16_wcd_priv->on_demand_list[ON_DEMAND_MICBIAS].ref, 0);
	iounmap(msm8x16_wcd->dig_base);

	return 0;
}

static int msm8x16_wcd_enable_static_supplies_to_optimum(
				struct msm8x16_wcd *msm8x16,
				struct msm8x16_wcd_pdata *pdata)
{
	int i;
	int ret = 0;

	for (i = 0; i < msm8x16->num_of_supplies; i++) {
		if (pdata->regulator[i].ondemand)
			continue;
		if (regulator_count_voltages(msm8x16->supplies[i].consumer) <=
			0)
			continue;

		ret = regulator_set_voltage(msm8x16->supplies[i].consumer,
			pdata->regulator[i].min_uv,
			pdata->regulator[i].max_uv);
		if (ret) {
			dev_err(msm8x16->dev,
				"Setting volt failed for regulator %s err %d\n",
				msm8x16->supplies[i].supply, ret);
		}

		ret = regulator_set_optimum_mode(msm8x16->supplies[i].consumer,
			pdata->regulator[i].optimum_ua);
		dev_dbg(msm8x16->dev, "Regulator %s set optimum mode\n",
			 msm8x16->supplies[i].supply);
	}

	return ret;
}

static int msm8x16_wcd_disable_static_supplies_to_optimum(
			struct msm8x16_wcd *msm8x16,
			struct msm8x16_wcd_pdata *pdata)
{
	int i;
	int ret = 0;

	for (i = 0; i < msm8x16->num_of_supplies; i++) {
		if (pdata->regulator[i].ondemand)
			continue;
		if (regulator_count_voltages(msm8x16->supplies[i].consumer) <=
			0)
			continue;
		regulator_set_voltage(msm8x16->supplies[i].consumer, 0,
			pdata->regulator[i].max_uv);
		regulator_set_optimum_mode(msm8x16->supplies[i].consumer, 0);
		dev_dbg(msm8x16->dev, "Regulator %s set optimum mode\n",
				 msm8x16->supplies[i].supply);
	}

	return ret;
}

int msm8x16_wcd_suspend(struct snd_soc_codec *codec)
{
	struct msm8916_asoc_mach_data *pdata = NULL;
	struct msm8x16_wcd *msm8x16 = codec->control_data;
	struct msm8x16_wcd_pdata *msm8x16_pdata = msm8x16->dev->platform_data;

	pdata = snd_soc_card_get_drvdata(codec->card);
	pr_debug("%s: mclk cnt = %d, mclk_enabled = %d\n",
			__func__, atomic_read(&pdata->mclk_rsc_ref),
			atomic_read(&pdata->mclk_enabled));
	if (atomic_read(&pdata->mclk_enabled) == true) {
		cancel_delayed_work_sync(
				&pdata->disable_mclk_work);
		mutex_lock(&pdata->cdc_mclk_mutex);
		if (atomic_read(&pdata->mclk_enabled) == true) {
			pdata->digital_cdc_clk.clk_val = 0;
			afe_set_digital_codec_core_clock(
					AFE_PORT_ID_PRIMARY_MI2S_RX,
					&pdata->digital_cdc_clk);
			atomic_set(&pdata->mclk_enabled, false);
		}
		mutex_unlock(&pdata->cdc_mclk_mutex);
	}
	msm8x16_wcd_disable_static_supplies_to_optimum(msm8x16, msm8x16_pdata);
	return 0;
}

int msm8x16_wcd_resume(struct snd_soc_codec *codec)
{
	struct msm8916_asoc_mach_data *pdata = NULL;
	struct msm8x16_wcd *msm8x16 = codec->control_data;
	struct msm8x16_wcd_pdata *msm8x16_pdata = msm8x16->dev->platform_data;

	pdata = snd_soc_card_get_drvdata(codec->card);
	msm8x16_wcd_enable_static_supplies_to_optimum(msm8x16, msm8x16_pdata);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_msm8x16_wcd = {
	.probe	= msm8x16_wcd_codec_probe,
	.remove	= msm8x16_wcd_codec_remove,

	.read = msm8x16_wcd_read,
	.write = msm8x16_wcd_write,

	.suspend = msm8x16_wcd_suspend,
	.resume = msm8x16_wcd_resume,

	.readable_register = msm8x16_wcd_readable,
	.volatile_register = msm8x16_wcd_volatile,

	.reg_cache_size = MSM8X16_WCD_CACHE_SIZE,
	.reg_cache_default = msm8x16_wcd_reset_reg_defaults,
	.reg_word_size = 1,

	.controls = msm8x16_wcd_snd_controls,
	.num_controls = ARRAY_SIZE(msm8x16_wcd_snd_controls),
	.dapm_widgets = msm8x16_wcd_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(msm8x16_wcd_dapm_widgets),
	.dapm_routes = audio_map,
	.num_dapm_routes = ARRAY_SIZE(audio_map),
};

static int msm8x16_wcd_init_supplies(struct msm8x16_wcd *msm8x16,
				struct msm8x16_wcd_pdata *pdata)
{
	int ret;
	int i;

	msm8x16->supplies = kzalloc(sizeof(struct regulator_bulk_data) *
				   ARRAY_SIZE(pdata->regulator),
				   GFP_KERNEL);
	if (!msm8x16->supplies) {
		ret = -ENOMEM;
		goto err;
	}

	msm8x16->num_of_supplies = 0;

	if (ARRAY_SIZE(pdata->regulator) > MAX_REGULATOR) {
		dev_err(msm8x16->dev, "%s: Array Size out of bound\n",
			__func__);
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(pdata->regulator); i++) {
		if (pdata->regulator[i].name) {
			msm8x16->supplies[i].supply = pdata->regulator[i].name;
			msm8x16->num_of_supplies++;
		}
	}

	ret = regulator_bulk_get(msm8x16->dev, msm8x16->num_of_supplies,
				 msm8x16->supplies);
	if (ret != 0) {
		dev_err(msm8x16->dev, "Failed to get supplies: err = %d\n",
							ret);
		goto err_supplies;
	}

	for (i = 0; i < msm8x16->num_of_supplies; i++) {
		if (regulator_count_voltages(msm8x16->supplies[i].consumer) <=
			0)
			continue;

		ret = regulator_set_voltage(msm8x16->supplies[i].consumer,
			pdata->regulator[i].min_uv,
			pdata->regulator[i].max_uv);
		if (ret) {
			dev_err(msm8x16->dev, "Setting regulator voltage failed for regulator %s err = %d\n",
				msm8x16->supplies[i].supply, ret);
			goto err_get;
		}

		ret = regulator_set_optimum_mode(msm8x16->supplies[i].consumer,
			pdata->regulator[i].optimum_ua);
		if (ret < 0) {
			dev_err(msm8x16->dev, "Setting regulator optimum mode failed for regulator %s err = %d\n",
				msm8x16->supplies[i].supply, ret);
			goto err_get;
		} else {
			ret = 0;
		}
	}

	return ret;

err_get:
	regulator_bulk_free(msm8x16->num_of_supplies, msm8x16->supplies);
err_supplies:
	kfree(msm8x16->supplies);
err:
	return ret;
}

static int msm8x16_wcd_enable_static_supplies(struct msm8x16_wcd *msm8x16,
					  struct msm8x16_wcd_pdata *pdata)
{
	int i;
	int ret = 0;

	for (i = 0; i < msm8x16->num_of_supplies; i++) {
		if (pdata->regulator[i].ondemand)
			continue;
		ret = regulator_enable(msm8x16->supplies[i].consumer);
		if (ret) {
			dev_err(msm8x16->dev, "Failed to enable %s\n",
			       msm8x16->supplies[i].supply);
			break;
		} else {
			dev_dbg(msm8x16->dev, "Enabled regulator %s\n",
				 msm8x16->supplies[i].supply);
		}
	}

	while (ret && --i)
		if (!pdata->regulator[i].ondemand)
			regulator_disable(msm8x16->supplies[i].consumer);

	return ret;
}



static void msm8x16_wcd_disable_supplies(struct msm8x16_wcd *msm8x16,
				     struct msm8x16_wcd_pdata *pdata)
{
	int i;

	regulator_bulk_disable(msm8x16->num_of_supplies,
				    msm8x16->supplies);
	for (i = 0; i < msm8x16->num_of_supplies; i++) {
		if (regulator_count_voltages(msm8x16->supplies[i].consumer) <=
			0)
			continue;
		regulator_set_voltage(msm8x16->supplies[i].consumer, 0,
			pdata->regulator[i].max_uv);
		regulator_set_optimum_mode(msm8x16->supplies[i].consumer, 0);
	}
	regulator_bulk_free(msm8x16->num_of_supplies, msm8x16->supplies);
	kfree(msm8x16->supplies);
}

static int msm8x16_wcd_device_init(struct msm8x16_wcd *msm8x16)
{
	mutex_init(&msm8x16->io_lock);
	return 0;
}

static int msm8x16_wcd_spmi_probe(struct spmi_device *spmi)
{
	int ret = 0;
	struct msm8x16_wcd *msm8x16 = NULL;
	struct msm8x16_wcd_pdata *pdata;
	struct resource *wcd_resource;
	int modem_state;

	dev_dbg(&spmi->dev, "%s(%d):slave ID = 0x%x\n",
		__func__, __LINE__,  spmi->sid);

	modem_state = apr_get_modem_state();
	if (modem_state != APR_SUBSYS_LOADED) {
		dev_dbg(&spmi->dev, "Modem is not loaded yet %d\n",
				modem_state);
		return -EPROBE_DEFER;
	}

	wcd_resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!wcd_resource) {
		dev_err(&spmi->dev, "Unable to get Tombak base address\n");
		return -ENXIO;
	}

	switch (wcd_resource->start) {
	case TOMBAK_CORE_0_SPMI_ADDR:
		msm8x16_wcd_modules[0].spmi = spmi;
		msm8x16_wcd_modules[0].base = (spmi->sid << 16) +
						wcd_resource->start;
		wcd9xxx_spmi_set_dev(msm8x16_wcd_modules[0].spmi, 0);
		break;
	case TOMBAK_CORE_1_SPMI_ADDR:
		msm8x16_wcd_modules[1].spmi = spmi;
		msm8x16_wcd_modules[1].base = (spmi->sid << 16) +
						wcd_resource->start;
		wcd9xxx_spmi_set_dev(msm8x16_wcd_modules[1].spmi, 1);
	if (wcd9xxx_spmi_irq_init()) {
		dev_err(&spmi->dev,
				"%s: irq initialization failed\n", __func__);
	} else {
		dev_dbg(&spmi->dev,
				"%s: irq initialization passed\n", __func__);
	}
		goto rtn;
	default:
		ret = -EINVAL;
		goto rtn;
	}


	dev_dbg(&spmi->dev, "%s(%d):start addr = 0x%pa\n",
		__func__, __LINE__,  &wcd_resource->start);

	if (wcd_resource->start != TOMBAK_CORE_0_SPMI_ADDR)
		goto rtn;

	dev_set_name(&spmi->dev, "%s", MSM8X16_CODEC_NAME);
	if (spmi->dev.of_node) {
		dev_dbg(&spmi->dev, "%s:Platform data from device tree\n",
			__func__);
		pdata = msm8x16_wcd_populate_dt_pdata(&spmi->dev);
		spmi->dev.platform_data = pdata;
	} else {
		dev_dbg(&spmi->dev, "%s:Platform data from board file\n",
			__func__);
		pdata = spmi->dev.platform_data;
	}

	msm8x16 = kzalloc(sizeof(struct msm8x16_wcd), GFP_KERNEL);
	if (msm8x16 == NULL) {
		dev_err(&spmi->dev,
			"%s: error, allocation failed\n", __func__);
		ret = -ENOMEM;
		goto rtn;
	}

	msm8x16->dev = &spmi->dev;
	msm8x16->read_dev = __msm8x16_wcd_reg_read;
	msm8x16->write_dev = __msm8x16_wcd_reg_write;
	ret = msm8x16_wcd_init_supplies(msm8x16, pdata);
	if (ret) {
		dev_err(&spmi->dev, "%s: Fail to enable Codec supplies\n",
			__func__);
		goto err_codec;
	}

	ret = msm8x16_wcd_enable_static_supplies(msm8x16, pdata);
	if (ret) {
		dev_err(&spmi->dev,
			"%s: Fail to enable Codec pre-reset supplies\n",
			   __func__);
		goto err_codec;
	}
	usleep_range(5, 6);

	ret = msm8x16_wcd_device_init(msm8x16);
	if (ret) {
		dev_err(&spmi->dev,
			"%s:msm8x16_wcd_device_init failed with error %d\n",
			__func__, ret);
		goto err_supplies;
	}
	dev_set_drvdata(&spmi->dev, msm8x16);

	ret = snd_soc_register_codec(&spmi->dev, &soc_codec_dev_msm8x16_wcd,
				     msm8x16_wcd_i2s_dai,
				     ARRAY_SIZE(msm8x16_wcd_i2s_dai));
	if (ret) {
		dev_err(&spmi->dev,
			"%s:snd_soc_register_codec failed with error %d\n",
			__func__, ret);
	} else {
		goto rtn;
	}
err_supplies:
	msm8x16_wcd_disable_supplies(msm8x16, pdata);
err_codec:
	kfree(msm8x16);
rtn:
	return ret;
}

static void msm8x16_wcd_device_exit(struct msm8x16_wcd *msm8x16)
{
	mutex_destroy(&msm8x16->io_lock);
	kfree(msm8x16);
}

static int msm8x16_wcd_spmi_remove(struct spmi_device *spmi)
{
	struct msm8x16_wcd *msm8x16 = dev_get_drvdata(&spmi->dev);

	msm8x16_wcd_device_exit(msm8x16);
	return 0;
}

#ifdef CONFIG_PM
static int msm8x16_wcd_spmi_resume(struct spmi_device *spmi)
{
	struct resource *wcd_resource;

	wcd_resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!wcd_resource) {
		dev_err(&spmi->dev, "Unable to get CDC SPMI resource\n");
		return -ENXIO;
	}

	if (wcd_resource->start == TOMBAK_CORE_0_SPMI_ADDR)
		return wcd9xxx_spmi_resume();
	return 0;
}

static int msm8x16_wcd_spmi_suspend(struct spmi_device *spmi,
				    pm_message_t pmesg)
{
	struct resource *wcd_resource;

	wcd_resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!wcd_resource) {
		dev_err(&spmi->dev, "Unable to get CDC SPMI resource\n");
		return -ENXIO;
	}

	if (wcd_resource->start == TOMBAK_CORE_0_SPMI_ADDR)
		return wcd9xxx_spmi_suspend(pmesg);
	return 0;
}
#endif

static struct spmi_device_id msm8x16_wcd_spmi_id_table[] = {
	{"wcd-spmi", MSM8X16_WCD_SPMI_DIGITAL},
	{"wcd-spmi", MSM8X16_WCD_SPMI_ANALOG},
	{}
};

static struct of_device_id msm8x16_wcd_spmi_of_match[] = {
	{ .compatible = "qcom,wcd-spmi",},
	{ },
};

static struct spmi_driver wcd_spmi_driver = {
	.driver                 = {
		.owner          = THIS_MODULE,
		.name           = "wcd-spmi-core",
		.of_match_table = msm8x16_wcd_spmi_of_match
	},
#ifdef CONFIG_PM
		.suspend = msm8x16_wcd_spmi_suspend,
		.resume = msm8x16_wcd_spmi_resume,
#endif
	.id_table               = msm8x16_wcd_spmi_id_table,
	.probe                  = msm8x16_wcd_spmi_probe,
	.remove                 = msm8x16_wcd_spmi_remove,
};

static int __init msm8x16_wcd_codec_init(void)
{
	spmi_driver_register(&wcd_spmi_driver);
	return 0;
}
module_init(msm8x16_wcd_codec_init);

static void __exit msm8x16_wcd_codec_exit(void)
{
	spmi_driver_unregister(&wcd_spmi_driver);
}
module_exit(msm8x16_wcd_codec_exit);

MODULE_DESCRIPTION("MSM8x16 Audio codec driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, msm8x16_wcd_spmi_id_table);

