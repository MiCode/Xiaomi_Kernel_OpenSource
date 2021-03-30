// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <linux/pm_runtime.h>
#include <asoc/msm-cdc-pinctrl.h>
#include <soc/swr-common.h>
#include <soc/swr-wcd.h>
#include <dsp/digital-cdc-rsc-mgr.h>
#include "bolero-cdc.h"
#include "bolero-cdc-registers.h"
#include "bolero-clk-rsc.h"

/* pm runtime auto suspend timer in msecs */
#define VA_AUTO_SUSPEND_DELAY          100 /* delay in msec */
#define VA_MACRO_MAX_OFFSET 0x1000

#define VA_MACRO_NUM_DECIMATORS 8

#define VA_MACRO_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)
#define VA_MACRO_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE |\
		SNDRV_PCM_FMTBIT_S24_3LE)

#define  TX_HPF_CUT_OFF_FREQ_MASK	0x60
#define  CF_MIN_3DB_4HZ			0x0
#define  CF_MIN_3DB_75HZ		0x1
#define  CF_MIN_3DB_150HZ		0x2

#define VA_MACRO_DMIC_SAMPLE_RATE_UNDEFINED 0
#define VA_MACRO_MCLK_FREQ 9600000
#define VA_MACRO_TX_PATH_OFFSET 0x80
#define VA_MACRO_TX_DMIC_CLK_DIV_MASK 0x0E
#define VA_MACRO_TX_DMIC_CLK_DIV_SHFT 0x01
#define VA_MACRO_SWR_MIC_MUX_SEL_MASK 0xF
#define VA_MACRO_ADC_MUX_CFG_OFFSET 0x8
#define VA_MACRO_ADC_MODE_CFG0_SHIFT 1

#define BOLERO_CDC_VA_TX_DMIC_UNMUTE_DELAY_MS       40
#define BOLERO_CDC_VA_TX_AMIC_UNMUTE_DELAY_MS       100
#define BOLERO_CDC_VA_TX_DMIC_HPF_DELAY_MS       300
#define BOLERO_CDC_VA_TX_AMIC_HPF_DELAY_MS       300
#define MAX_RETRY_ATTEMPTS 500
#define VA_MACRO_SWR_STRING_LEN 80
#define VA_MACRO_CHILD_DEVICES_MAX 3

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);
static int va_tx_unmute_delay = BOLERO_CDC_VA_TX_DMIC_UNMUTE_DELAY_MS;
module_param(va_tx_unmute_delay, int, 0664);
MODULE_PARM_DESC(va_tx_unmute_delay, "delay to unmute the tx path");

static int va_macro_core_vote(void *handle, bool enable);

enum {
	VA_MACRO_AIF_INVALID = 0,
	VA_MACRO_AIF1_CAP,
	VA_MACRO_AIF2_CAP,
	VA_MACRO_AIF3_CAP,
	VA_MACRO_MAX_DAIS,
};

enum {
	VA_MACRO_DEC0,
	VA_MACRO_DEC1,
	VA_MACRO_DEC2,
	VA_MACRO_DEC3,
	VA_MACRO_DEC4,
	VA_MACRO_DEC5,
	VA_MACRO_DEC6,
	VA_MACRO_DEC7,
	VA_MACRO_DEC_MAX,
};

enum {
	VA_MACRO_CLK_DIV_2,
	VA_MACRO_CLK_DIV_3,
	VA_MACRO_CLK_DIV_4,
	VA_MACRO_CLK_DIV_6,
	VA_MACRO_CLK_DIV_8,
	VA_MACRO_CLK_DIV_16,
};

enum {
	MSM_DMIC,
	SWR_MIC,
};

enum {
	TX_MCLK,
	VA_MCLK,
};

struct va_mute_work {
	struct va_macro_priv *va_priv;
	u32 decimator;
	struct delayed_work dwork;
};

struct hpf_work {
	struct va_macro_priv *va_priv;
	u8 decimator;
	u8 hpf_cut_off_freq;
	struct delayed_work dwork;
};

/* Hold instance to soundwire platform device */
struct va_macro_swr_ctrl_data {
	struct platform_device *va_swr_pdev;
};

struct va_macro_swr_ctrl_platform_data {
	void *handle; /* holds codec private data */
	int (*read)(void *handle, int reg);
	int (*write)(void *handle, int reg, int val);
	int (*bulk_write)(void *handle, u32 *reg, u32 *val, size_t len);
	int (*clk)(void *handle, bool enable);
	int (*core_vote)(void *handle, bool enable);
	int (*handle_irq)(void *handle,
			  irqreturn_t (*swrm_irq_handler)(int irq,
							  void *data),
			  void *swrm_handle,
			  int action);
};

struct va_macro_priv {
	struct device *dev;
	bool dec_active[VA_MACRO_NUM_DECIMATORS];
	bool va_without_decimation;
	struct clk *lpass_audio_hw_vote;
	struct mutex mclk_lock;
	struct mutex swr_clk_lock;
	struct snd_soc_component *component;
	struct hpf_work va_hpf_work[VA_MACRO_NUM_DECIMATORS];
	struct va_mute_work va_mute_dwork[VA_MACRO_NUM_DECIMATORS];
	unsigned long active_ch_mask[VA_MACRO_MAX_DAIS];
	unsigned long active_ch_cnt[VA_MACRO_MAX_DAIS];
	u16 dmic_clk_div;
	u16 va_mclk_users;
	int swr_clk_users;
	bool reset_swr;
	struct device_node *va_swr_gpio_p;
	struct va_macro_swr_ctrl_data *swr_ctrl_data;
	struct va_macro_swr_ctrl_platform_data swr_plat_data;
	struct work_struct va_macro_add_child_devices_work;
	int child_count;
	u16 mclk_mux_sel;
	char __iomem *va_io_base;
	char __iomem *va_island_mode_muxsel;
	struct platform_device *pdev_child_devices
			[VA_MACRO_CHILD_DEVICES_MAX];
	struct regulator *micb_supply;
	u32 micb_voltage;
	u32 micb_current;
	u32 version;
	u32 is_used_va_swr_gpio;
	int micb_users;
	u16 default_clk_id;
	u16 clk_id;
	int tx_swr_clk_cnt;
	int va_swr_clk_cnt;
	int va_clk_status;
	int tx_clk_status;
	int dapm_tx_clk_status;
	bool lpi_enable;
	bool register_event_listener;
	bool clk_div_switch;
	int dec_mode[VA_MACRO_NUM_DECIMATORS];
	u16 current_clk_id;
};

static bool va_macro_get_data(struct snd_soc_component *component,
			      struct device **va_dev,
			      struct va_macro_priv **va_priv,
			      const char *func_name)
{
	*va_dev = bolero_get_device_ptr(component->dev, VA_MACRO);
	if (!(*va_dev)) {
		dev_err(component->dev,
			"%s: null device for macro!\n", func_name);
		return false;
	}
	*va_priv = dev_get_drvdata((*va_dev));
	if (!(*va_priv) || !(*va_priv)->component) {
		dev_err(component->dev,
			"%s: priv is null for macro!\n", func_name);
		return false;
	}
	return true;
}

static int va_macro_clk_div_get(struct snd_soc_component *component)
{
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	if ((va_priv->version >= BOLERO_VERSION_2_0)
		&& va_priv->clk_div_switch
		&& (va_priv->dmic_clk_div == VA_MACRO_CLK_DIV_16))
		return VA_MACRO_CLK_DIV_8;

	return va_priv->dmic_clk_div;
}

static int va_macro_mclk_enable(struct va_macro_priv *va_priv,
				 bool mclk_enable, bool dapm)
{
	struct regmap *regmap = dev_get_regmap(va_priv->dev->parent, NULL);
	int ret = 0;

	if (regmap == NULL) {
		dev_err(va_priv->dev, "%s: regmap is NULL\n", __func__);
		return -EINVAL;
	}

	dev_dbg(va_priv->dev, "%s: mclk_enable = %u, dapm = %d clk_users= %d\n",
		__func__, mclk_enable, dapm, va_priv->va_mclk_users);

	mutex_lock(&va_priv->mclk_lock);
	if (mclk_enable) {
		ret = bolero_clk_rsc_request_clock(va_priv->dev,
						   va_priv->default_clk_id,
						   va_priv->clk_id,
						   true);
		if (ret < 0) {
			dev_err(va_priv->dev,
				"%s: va request clock en failed\n",
				__func__);
			goto exit;
		}
		bolero_clk_rsc_fs_gen_request(va_priv->dev,
					      true);
		if (va_priv->va_mclk_users == 0) {
			regcache_mark_dirty(regmap);
			regcache_sync_region(regmap,
					VA_START_OFFSET,
					VA_MAX_OFFSET);
		}
		va_priv->va_mclk_users++;
	} else {
		if (va_priv->va_mclk_users <= 0) {
			dev_err(va_priv->dev, "%s: clock already disabled\n",
			__func__);
			va_priv->va_mclk_users = 0;
			goto exit;
		}
		va_priv->va_mclk_users--;
		bolero_clk_rsc_fs_gen_request(va_priv->dev,
					  false);
		bolero_clk_rsc_request_clock(va_priv->dev,
					va_priv->default_clk_id,
					va_priv->clk_id,
					false);
	}
exit:
	mutex_unlock(&va_priv->mclk_lock);
	return ret;
}

static int va_macro_event_handler(struct snd_soc_component *component,
				  u16 event, u32 data)
{
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;
	int retry_cnt = MAX_RETRY_ATTEMPTS;
	int ret = 0;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	switch (event) {
	case BOLERO_MACRO_EVT_WAIT_VA_CLK_RESET:
		while ((va_priv->va_mclk_users != 0) && (retry_cnt != 0)) {
			dev_dbg_ratelimited(va_dev, "%s:retry_cnt: %d\n",
				__func__, retry_cnt);
			/*
			 * Userspace takes 10 seconds to close
			 * the session when pcm_start fails due to concurrency
			 * with PDR/SSR. Loop and check every 20ms till 10
			 * seconds for va_mclk user count to get reset to 0
			 * which ensures userspace teardown is done and SSR
			 * powerup seq can proceed.
			 */
			msleep(20);
			retry_cnt--;
		}
		if (retry_cnt == 0)
			dev_err(va_dev,
				"%s: va_mclk_users is non-zero still, audio SSR fail!!\n",
				__func__);
		break;
	case BOLERO_MACRO_EVT_PRE_SSR_UP:
		/* enable&disable VA_CORE_CLK to reset GFMUX reg */
		va_macro_core_vote(va_priv, true);
		ret = bolero_clk_rsc_request_clock(va_priv->dev,
						va_priv->default_clk_id,
						VA_CORE_CLK, true);
		if (ret < 0)
			dev_err_ratelimited(va_priv->dev,
				"%s, failed to enable clk, ret:%d\n",
				__func__, ret);
		else
			bolero_clk_rsc_request_clock(va_priv->dev,
						va_priv->default_clk_id,
						VA_CORE_CLK, false);
		va_macro_core_vote(va_priv, false);
		break;
	case BOLERO_MACRO_EVT_SSR_UP:
		trace_printk("%s, enter SSR up\n", __func__);
		/* reset swr after ssr/pdr */
		va_priv->reset_swr = true;
		if (va_priv->swr_ctrl_data)
			swrm_wcd_notify(
				va_priv->swr_ctrl_data[0].va_swr_pdev,
				SWR_DEVICE_SSR_UP, NULL);
		break;
	case BOLERO_MACRO_EVT_CLK_RESET:
		bolero_rsc_clk_reset(va_dev, VA_CORE_CLK);
		break;
	case BOLERO_MACRO_EVT_SSR_DOWN:
		if (va_priv->swr_ctrl_data) {
			swrm_wcd_notify(
				va_priv->swr_ctrl_data[0].va_swr_pdev,
				SWR_DEVICE_SSR_DOWN, NULL);
		}
		if ((!pm_runtime_enabled(va_dev) ||
		     !pm_runtime_suspended(va_dev))) {
			ret = bolero_runtime_suspend(va_dev);
			if (!ret) {
				pm_runtime_disable(va_dev);
				pm_runtime_set_suspended(va_dev);
				pm_runtime_enable(va_dev);
			}
		}
		break;
	default:
		break;
	}
	return 0;
}

static int va_macro_swr_clk_event_v2(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	dev_dbg(va_dev, "%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		va_priv->va_swr_clk_cnt++;
		break;
	case SND_SOC_DAPM_POST_PMD:
		va_priv->va_swr_clk_cnt--;
		break;
	default:
		break;
	}
	return 0;
}

static int va_macro_swr_pwr_event_v2(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	int ret = 0;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	dev_dbg(va_dev, "%s: event = %d, lpi_enable = %d\n",
		__func__, event, va_priv->lpi_enable);

	if (!va_priv->lpi_enable)
		return ret;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(component->dev,
			"%s: va_swr_clk_cnt %d, tx_swr_clk_cnt %d, tx_clk_status %d\n",
			__func__, va_priv->va_swr_clk_cnt,
			va_priv->tx_swr_clk_cnt, va_priv->tx_clk_status);
		if (va_priv->current_clk_id == VA_CORE_CLK) {
			return 0;
		} else if ( va_priv->va_swr_clk_cnt != 0 &&
				va_priv->tx_clk_status) {
			ret = bolero_clk_rsc_request_clock(va_priv->dev,
					va_priv->default_clk_id,
					VA_CORE_CLK,
					true);
			if (ret) {
				dev_dbg(component->dev,
					"%s: request clock VA_CLK enable failed\n",
					__func__);
				break;
			}
			ret = bolero_clk_rsc_request_clock(va_priv->dev,
					va_priv->default_clk_id,
					TX_CORE_CLK,
					false);
			if (ret) {
				dev_dbg(component->dev,
					"%s: request clock TX_CLK enable failed\n",
					__func__);
				bolero_clk_rsc_request_clock(va_priv->dev,
					va_priv->default_clk_id,
					VA_CORE_CLK,
					false);
				break;
			}
			va_priv->current_clk_id = VA_CORE_CLK;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (va_priv->current_clk_id == VA_CORE_CLK &&
			va_priv->va_swr_clk_cnt != 0 &&
			va_priv->tx_clk_status) {
			ret = bolero_clk_rsc_request_clock(va_priv->dev,
					va_priv->default_clk_id,
					TX_CORE_CLK,
					true);
			if (ret) {
				dev_dbg(component->dev,
					"%s: request clock TX_CLK disable failed\n",
					__func__);
				break;
			}
			ret = bolero_clk_rsc_request_clock(va_priv->dev,
					va_priv->default_clk_id,
					VA_CORE_CLK,
					false);
			if (ret) {
				dev_dbg(component->dev,
					"%s: request clock VA_CLK disable failed\n",
					__func__);
				bolero_clk_rsc_request_clock(va_priv->dev,
					TX_CORE_CLK,
					TX_CORE_CLK,
					false);
				break;
			}
			va_priv->current_clk_id = TX_CORE_CLK;
		}
		break;
	default:
		dev_err(va_priv->dev,
			"%s: invalid DAPM event %d\n", __func__, event);
		ret = -EINVAL;
	}
	return ret;
}

static int va_macro_swr_pwr_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	int ret = 0;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	dev_dbg(va_dev, "%s: event = %d, lpi_enable = %d\n",
		__func__, event, va_priv->lpi_enable);

	if (!va_priv->lpi_enable)
		return ret;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (va_priv->lpass_audio_hw_vote) {
			ret = digital_cdc_rsc_mgr_hw_vote_enable(
					va_priv->lpass_audio_hw_vote);
			if (ret)
				dev_err(va_dev,
					"%s: lpass audio hw enable failed\n",
					__func__);
		}
		if (!ret) {
			if (bolero_tx_clk_switch(component, VA_CORE_CLK))
				dev_dbg(va_dev, "%s: clock switch failed\n",
					__func__);
		}
		if (va_priv->lpi_enable) {
			bolero_register_event_listener(component, true);
			va_priv->register_event_listener = true;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (va_priv->register_event_listener) {
			va_priv->register_event_listener = false;
			bolero_register_event_listener(component, false);
		}
		if (bolero_tx_clk_switch(component, TX_CORE_CLK))
			dev_dbg(va_dev, "%s: clock switch failed\n",__func__);
		if (va_priv->lpass_audio_hw_vote)
			digital_cdc_rsc_mgr_hw_vote_disable(
				va_priv->lpass_audio_hw_vote);
		break;
	default:
		dev_err(va_priv->dev,
			"%s: invalid DAPM event %d\n", __func__, event);
		ret = -EINVAL;
	}
	return ret;
}

static int va_macro_tx_swr_clk_event_v2(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	if (SND_SOC_DAPM_EVENT_ON(event))
		++va_priv->tx_swr_clk_cnt;
	if (SND_SOC_DAPM_EVENT_OFF(event))
		--va_priv->tx_swr_clk_cnt;

	return 0;
}

static int va_macro_mclk_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	int ret = 0;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	dev_dbg(va_dev, "%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = bolero_clk_rsc_request_clock(va_priv->dev,
						   va_priv->default_clk_id,
						   TX_CORE_CLK,
						   true);
		if (!ret)
			va_priv->dapm_tx_clk_status++;

		if (va_priv->lpi_enable)
			ret = va_macro_mclk_enable(va_priv, 1, true);
		else
			ret = bolero_tx_mclk_enable(component, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (va_priv->lpi_enable) {
			va_macro_mclk_enable(va_priv, 0, true);
		} else {
			bolero_tx_mclk_enable(component, 0);
		}

		if (va_priv->dapm_tx_clk_status > 0) {
			bolero_clk_rsc_request_clock(va_priv->dev,
					   va_priv->default_clk_id,
					   TX_CORE_CLK,
					   false);
			va_priv->dapm_tx_clk_status--;
		}
		break;
	default:
		dev_err(va_priv->dev,
			"%s: invalid DAPM event %d\n", __func__, event);
		ret = -EINVAL;
	}
	return ret;
}

static int va_macro_tx_va_mclk_enable(struct va_macro_priv *va_priv,
				      struct regmap *regmap, int clk_type,
				      bool enable)
{
	int ret = 0, clk_tx_ret = 0;

	dev_dbg(va_priv->dev,
		"%s: clock type %s, enable: %s tx_mclk_users: %d\n",
		__func__, (clk_type ? "VA_MCLK" : "TX_MCLK"),
		(enable ? "enable" : "disable"), va_priv->va_mclk_users);

	if (enable) {
		if (va_priv->swr_clk_users == 0) {
			msm_cdc_pinctrl_select_active_state(
						va_priv->va_swr_gpio_p);
			msm_cdc_pinctrl_set_wakeup_capable(
					va_priv->va_swr_gpio_p, false);
		}
		clk_tx_ret = bolero_clk_rsc_request_clock(va_priv->dev,
						   TX_CORE_CLK,
						   TX_CORE_CLK,
						   true);
		if (clk_type == TX_MCLK) {
			ret = bolero_clk_rsc_request_clock(va_priv->dev,
							   TX_CORE_CLK,
							   TX_CORE_CLK,
							   true);
			if (ret < 0) {
				if (va_priv->swr_clk_users == 0)
					msm_cdc_pinctrl_select_sleep_state(
							va_priv->va_swr_gpio_p);
				dev_err_ratelimited(va_priv->dev,
					"%s: swr request clk failed\n",
					__func__);
				goto done;
			}
			bolero_clk_rsc_fs_gen_request(va_priv->dev,
						  true);
		}
		if (clk_type == VA_MCLK) {
			ret = va_macro_mclk_enable(va_priv, 1, true);
			if (ret < 0) {
				if (va_priv->swr_clk_users == 0)
					msm_cdc_pinctrl_select_sleep_state(
							va_priv->va_swr_gpio_p);
				dev_err_ratelimited(va_priv->dev,
					"%s: request clock enable failed\n",
					__func__);
				goto done;
			}
		}
		if (va_priv->swr_clk_users == 0) {
			dev_dbg(va_priv->dev, "%s: reset_swr: %d\n",
				__func__, va_priv->reset_swr);
			if (va_priv->reset_swr)
				regmap_update_bits(regmap,
					BOLERO_CDC_VA_CLK_RST_CTRL_SWR_CONTROL,
					0x02, 0x02);
			regmap_update_bits(regmap,
				BOLERO_CDC_VA_CLK_RST_CTRL_SWR_CONTROL,
				0x01, 0x01);
			if (va_priv->reset_swr)
				regmap_update_bits(regmap,
					BOLERO_CDC_VA_CLK_RST_CTRL_SWR_CONTROL,
					0x02, 0x00);
			va_priv->reset_swr = false;
		}
		if (!clk_tx_ret)
			ret = bolero_clk_rsc_request_clock(va_priv->dev,
						   TX_CORE_CLK,
						   TX_CORE_CLK,
						   false);
		va_priv->swr_clk_users++;
	} else {
		if (va_priv->swr_clk_users <= 0) {
			dev_err_ratelimited(va_priv->dev,
				"va swrm clock users already 0\n");
			va_priv->swr_clk_users = 0;
			return 0;
		}
		clk_tx_ret = bolero_clk_rsc_request_clock(va_priv->dev,
						   TX_CORE_CLK,
						   TX_CORE_CLK,
						   true);
		va_priv->swr_clk_users--;
		if (va_priv->swr_clk_users == 0)
			regmap_update_bits(regmap,
				BOLERO_CDC_VA_CLK_RST_CTRL_SWR_CONTROL,
				0x01, 0x00);
		if (clk_type == VA_MCLK)
			va_macro_mclk_enable(va_priv, 0, true);
		if (clk_type == TX_MCLK) {
			bolero_clk_rsc_fs_gen_request(va_priv->dev,
						  false);
			ret = bolero_clk_rsc_request_clock(va_priv->dev,
							   TX_CORE_CLK,
							   TX_CORE_CLK,
							   false);
			if (ret < 0) {
				dev_err_ratelimited(va_priv->dev,
					"%s: swr request clk failed\n",
					__func__);
				goto done;
			}
		}
		if (!clk_tx_ret)
			ret = bolero_clk_rsc_request_clock(va_priv->dev,
						   TX_CORE_CLK,
						   TX_CORE_CLK,
						   false);
		if (va_priv->swr_clk_users == 0) {
			msm_cdc_pinctrl_set_wakeup_capable(
					va_priv->va_swr_gpio_p, true);
			msm_cdc_pinctrl_select_sleep_state(
						va_priv->va_swr_gpio_p);
		}
	}
	return 0;

done:
	if (!clk_tx_ret)
		bolero_clk_rsc_request_clock(va_priv->dev,
				TX_CORE_CLK,
				TX_CORE_CLK,
				false);
	return ret;
}

static int va_macro_core_vote(void *handle, bool enable)
{
	int rc = 0;
	struct va_macro_priv *va_priv = (struct va_macro_priv *) handle;

	if (va_priv == NULL) {
		pr_err("%s: va priv data is NULL\n", __func__);
		return -EINVAL;
	}

	if (enable) {
		pm_runtime_get_sync(va_priv->dev);
		if (bolero_check_core_votes(va_priv->dev))
			rc = 0;
		else
			rc = -ENOTSYNC;
	} else {
		pm_runtime_put_autosuspend(va_priv->dev);
		pm_runtime_mark_last_busy(va_priv->dev);
	}
	return rc;
}

static int va_macro_swrm_clock(void *handle, bool enable)
{
	struct va_macro_priv *va_priv = (struct va_macro_priv *) handle;
	struct regmap *regmap = dev_get_regmap(va_priv->dev->parent, NULL);
	int ret = 0;

	if (regmap == NULL) {
		dev_err(va_priv->dev, "%s: regmap is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&va_priv->swr_clk_lock);
	dev_dbg(va_priv->dev,
		"%s: swrm clock %s tx_swr_clk_cnt: %d va_swr_clk_cnt: %d\n",
		__func__, (enable ? "enable" : "disable"),
		va_priv->tx_swr_clk_cnt, va_priv->va_swr_clk_cnt);

	if (enable) {
		pm_runtime_get_sync(va_priv->dev);
		if (va_priv->va_swr_clk_cnt && !va_priv->tx_swr_clk_cnt) {
			ret = va_macro_tx_va_mclk_enable(va_priv, regmap,
							VA_MCLK, enable);
			if (ret) {
				pm_runtime_mark_last_busy(va_priv->dev);
				pm_runtime_put_autosuspend(va_priv->dev);
				goto done;
			}
			va_priv->va_clk_status++;
		} else {
			ret = va_macro_tx_va_mclk_enable(va_priv, regmap,
							TX_MCLK, enable);
			if (ret) {
				pm_runtime_mark_last_busy(va_priv->dev);
				pm_runtime_put_autosuspend(va_priv->dev);
				goto done;
			}
			va_priv->tx_clk_status++;
		}
		pm_runtime_mark_last_busy(va_priv->dev);
		pm_runtime_put_autosuspend(va_priv->dev);
	} else {
		if (va_priv->va_clk_status && !va_priv->tx_clk_status) {
			ret = va_macro_tx_va_mclk_enable(va_priv, regmap,
							VA_MCLK, enable);
			if (ret)
				goto done;
			--va_priv->va_clk_status;
		} else if (!va_priv->va_clk_status && va_priv->tx_clk_status) {
			ret = va_macro_tx_va_mclk_enable(va_priv, regmap,
							TX_MCLK, enable);
			if (ret)
				goto done;
			--va_priv->tx_clk_status;
		} else if (va_priv->va_clk_status && va_priv->tx_clk_status) {
			if (!va_priv->va_swr_clk_cnt && va_priv->tx_swr_clk_cnt) {
				ret = va_macro_tx_va_mclk_enable(va_priv, regmap,
								VA_MCLK, enable);
				if (ret)
					goto done;
				--va_priv->va_clk_status;
			} else {
				ret = va_macro_tx_va_mclk_enable(va_priv, regmap,
								TX_MCLK, enable);
				if (ret)
					goto done;
				--va_priv->tx_clk_status;
			}

		} else {
			dev_dbg(va_priv->dev,
				"%s: Both clocks are disabled\n", __func__);
		}
	}
	dev_dbg(va_priv->dev,
		"%s: swrm clock users %d tx_clk_sts_cnt: %d va_clk_sts_cnt: %d\n",
		__func__, va_priv->swr_clk_users, va_priv->tx_clk_status,
		va_priv->va_clk_status);
done:
	mutex_unlock(&va_priv->swr_clk_lock);
	return ret;
}

static bool is_amic_enabled(struct snd_soc_component *component, int decimator)
{
	u16 adc_mux_reg = 0, adc_reg = 0;
	u16 adc_n = BOLERO_ADC_MAX;
	bool ret = false;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return ret;

	adc_mux_reg = BOLERO_CDC_VA_INP_MUX_ADC_MUX0_CFG1 +
			VA_MACRO_ADC_MUX_CFG_OFFSET * decimator;
	if (snd_soc_component_read32(component, adc_mux_reg) & SWR_MIC) {
		if (va_priv->version == BOLERO_VERSION_2_1)
			return true;
		adc_reg = BOLERO_CDC_VA_INP_MUX_ADC_MUX0_CFG0 +
			VA_MACRO_ADC_MUX_CFG_OFFSET * decimator;
		adc_n = snd_soc_component_read32(component, adc_reg) &
				VA_MACRO_SWR_MIC_MUX_SEL_MASK;
		if (adc_n < BOLERO_ADC_MAX)
			return true;
	}

	return ret;
}

static void va_macro_tx_hpf_corner_freq_callback(struct work_struct *work)
{
	struct delayed_work *hpf_delayed_work;
	struct hpf_work *hpf_work;
	struct va_macro_priv *va_priv;
	struct snd_soc_component *component;
	u16 dec_cfg_reg, hpf_gate_reg;
	u8 hpf_cut_off_freq;
	u16 adc_reg = 0, adc_n = 0;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	va_priv = hpf_work->va_priv;
	component = va_priv->component;
	hpf_cut_off_freq = hpf_work->hpf_cut_off_freq;

	dec_cfg_reg = BOLERO_CDC_VA_TX0_TX_PATH_CFG0 +
			VA_MACRO_TX_PATH_OFFSET * hpf_work->decimator;
	hpf_gate_reg = BOLERO_CDC_VA_TX0_TX_PATH_SEC2 +
			VA_MACRO_TX_PATH_OFFSET * hpf_work->decimator;

	dev_dbg(va_priv->dev, "%s: decimator %u hpf_cut_of_freq 0x%x\n",
		__func__, hpf_work->decimator, hpf_cut_off_freq);

	if (is_amic_enabled(component, hpf_work->decimator)) {
		adc_reg = BOLERO_CDC_VA_INP_MUX_ADC_MUX0_CFG0 +
			VA_MACRO_ADC_MUX_CFG_OFFSET * hpf_work->decimator;
		adc_n = snd_soc_component_read32(component, adc_reg) &
				VA_MACRO_SWR_MIC_MUX_SEL_MASK;
		/* analog mic clear TX hold */
		bolero_clear_amic_tx_hold(component->dev, adc_n);
		snd_soc_component_update_bits(component,
				dec_cfg_reg, TX_HPF_CUT_OFF_FREQ_MASK,
				hpf_cut_off_freq << 5);
		snd_soc_component_update_bits(component, hpf_gate_reg,
					      0x03, 0x02);
		/* Minimum 1 clk cycle delay is required as per HW spec */
		usleep_range(1000, 1010);
		snd_soc_component_update_bits(component, hpf_gate_reg,
					      0x03, 0x01);
	} else {
		snd_soc_component_update_bits(component,
				dec_cfg_reg, TX_HPF_CUT_OFF_FREQ_MASK,
				hpf_cut_off_freq << 5);
		snd_soc_component_update_bits(component, hpf_gate_reg,
					      0x02, 0x02);
		/* Minimum 1 clk cycle delay is required as per HW spec */
		usleep_range(1000, 1010);
		snd_soc_component_update_bits(component, hpf_gate_reg,
					      0x02, 0x00);
	}
}

static void va_macro_mute_update_callback(struct work_struct *work)
{
	struct va_mute_work *va_mute_dwork;
	struct snd_soc_component *component = NULL;
	struct va_macro_priv *va_priv;
	struct delayed_work *delayed_work;
	u16 tx_vol_ctl_reg, decimator;

	delayed_work = to_delayed_work(work);
	va_mute_dwork = container_of(delayed_work, struct va_mute_work, dwork);
	va_priv = va_mute_dwork->va_priv;
	component = va_priv->component;
	decimator = va_mute_dwork->decimator;

	tx_vol_ctl_reg =
		BOLERO_CDC_VA_TX0_TX_PATH_CTL +
			VA_MACRO_TX_PATH_OFFSET * decimator;
	snd_soc_component_update_bits(component, tx_vol_ctl_reg, 0x10, 0x00);
	dev_dbg(va_priv->dev, "%s: decimator %u unmute\n",
		__func__, decimator);
}

static int va_macro_put_dec_enum(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(widget->dapm);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;
	u16 mic_sel_reg, dmic_clk_reg;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	val = ucontrol->value.enumerated.item[0];
	if (val > e->items - 1)
		return -EINVAL;

	dev_dbg(component->dev, "%s: wname: %s, val: 0x%x\n", __func__,
		widget->name, val);

	switch (e->reg) {
	case BOLERO_CDC_VA_INP_MUX_ADC_MUX0_CFG0:
		mic_sel_reg = BOLERO_CDC_VA_TX0_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_VA_INP_MUX_ADC_MUX1_CFG0:
		mic_sel_reg = BOLERO_CDC_VA_TX1_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_VA_INP_MUX_ADC_MUX2_CFG0:
		mic_sel_reg = BOLERO_CDC_VA_TX2_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_VA_INP_MUX_ADC_MUX3_CFG0:
		mic_sel_reg = BOLERO_CDC_VA_TX3_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_VA_INP_MUX_ADC_MUX4_CFG0:
		mic_sel_reg = BOLERO_CDC_VA_TX4_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_VA_INP_MUX_ADC_MUX5_CFG0:
		mic_sel_reg = BOLERO_CDC_VA_TX5_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_VA_INP_MUX_ADC_MUX6_CFG0:
		mic_sel_reg = BOLERO_CDC_VA_TX6_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_VA_INP_MUX_ADC_MUX7_CFG0:
		mic_sel_reg = BOLERO_CDC_VA_TX7_TX_PATH_CFG0;
		break;
	default:
		dev_err(component->dev, "%s: e->reg: 0x%x not expected\n",
			__func__, e->reg);
		return -EINVAL;
	}
	if (strnstr(widget->name, "SMIC", strlen(widget->name))) {
		if (val != 0) {
			if (val < 5) {
				snd_soc_component_update_bits(component,
							mic_sel_reg,
							1 << 7, 0x0 << 7);
			} else {
				snd_soc_component_update_bits(component,
							mic_sel_reg,
							1 << 7, 0x1 << 7);
				snd_soc_component_update_bits(component,
					BOLERO_CDC_VA_TOP_CSR_DMIC_CFG,
					0x80, 0x00);
				dmic_clk_reg =
					BOLERO_CDC_TX_TOP_CSR_SWR_DMIC0_CTL +
						((val - 5)/2) * 4;
				snd_soc_component_update_bits(component,
					dmic_clk_reg,
					0x0E, va_priv->dmic_clk_div << 0x1);
			}
		}
	} else {
		/* DMIC selected */
		if (val != 0)
			snd_soc_component_update_bits(component, mic_sel_reg,
					1 << 7, 1 << 7);
	}

	return snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
}

static int va_macro_lpi_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	ucontrol->value.integer.value[0] = va_priv->lpi_enable;

	return 0;
}

static int va_macro_lpi_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	va_priv->lpi_enable = ucontrol->value.integer.value[0];

	return 0;
}

static int va_macro_tx_mixer_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(widget->dapm);
	struct soc_multi_mixer_control *mixer =
		((struct soc_multi_mixer_control *)kcontrol->private_value);
	u32 dai_id = widget->shift;
	u32 dec_id = mixer->shift;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	if (test_bit(dec_id, &va_priv->active_ch_mask[dai_id]))
		ucontrol->value.integer.value[0] = 1;
	else
		ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int va_macro_tx_mixer_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(widget->dapm);
	struct snd_soc_dapm_update *update = NULL;
	struct soc_multi_mixer_control *mixer =
		((struct soc_multi_mixer_control *)kcontrol->private_value);
	u32 dai_id = widget->shift;
	u32 dec_id = mixer->shift;
	u32 enable = ucontrol->value.integer.value[0];
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	if (enable) {
		set_bit(dec_id, &va_priv->active_ch_mask[dai_id]);
		va_priv->active_ch_cnt[dai_id]++;
	} else {
		clear_bit(dec_id, &va_priv->active_ch_mask[dai_id]);
		va_priv->active_ch_cnt[dai_id]--;
	}

	snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, enable, update);

	return 0;
}

static int va_macro_enable_dmic(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	unsigned int dmic = 0;
	int ret = 0;
	char *wname;

	wname = strpbrk(w->name, "01234567");
	if (!wname) {
		dev_err(component->dev, "%s: widget not found\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(wname, 10, &dmic);
	if (ret < 0) {
		dev_err(component->dev, "%s: Invalid DMIC line on the codec\n",
			__func__);
		return -EINVAL;
	}

	dev_dbg(component->dev, "%s: event %d DMIC%d\n",
		__func__, event,  dmic);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		bolero_dmic_clk_enable(component, dmic, DMIC_VA, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		bolero_dmic_clk_enable(component, dmic, DMIC_VA, false);
		break;
	}

	return 0;
}

static int va_macro_enable_dec(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	unsigned int decimator;
	u16 tx_vol_ctl_reg, dec_cfg_reg, hpf_gate_reg;
	u16 tx_gain_ctl_reg;
	u8 hpf_cut_off_freq;
	u16 adc_mux_reg = 0;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;
	int hpf_delay = BOLERO_CDC_VA_TX_DMIC_HPF_DELAY_MS;
	int unmute_delay = BOLERO_CDC_VA_TX_DMIC_UNMUTE_DELAY_MS;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	decimator = w->shift;

	dev_dbg(va_dev, "%s(): widget = %s decimator = %u\n", __func__,
		w->name, decimator);

	tx_vol_ctl_reg = BOLERO_CDC_VA_TX0_TX_PATH_CTL +
				VA_MACRO_TX_PATH_OFFSET * decimator;
	hpf_gate_reg = BOLERO_CDC_VA_TX0_TX_PATH_SEC2 +
				VA_MACRO_TX_PATH_OFFSET * decimator;
	dec_cfg_reg = BOLERO_CDC_VA_TX0_TX_PATH_CFG0 +
				VA_MACRO_TX_PATH_OFFSET * decimator;
	tx_gain_ctl_reg = BOLERO_CDC_VA_TX0_TX_VOL_CTL +
				VA_MACRO_TX_PATH_OFFSET * decimator;
	adc_mux_reg = BOLERO_CDC_VA_INP_MUX_ADC_MUX0_CFG1 +
				VA_MACRO_ADC_MUX_CFG_OFFSET * decimator;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			dec_cfg_reg, 0x06, va_priv->dec_mode[decimator] <<
			VA_MACRO_ADC_MODE_CFG0_SHIFT);
		/* Enable TX PGA Mute */
		snd_soc_component_update_bits(component,
				tx_vol_ctl_reg, 0x10, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* Enable TX CLK */
		snd_soc_component_update_bits(component,
				tx_vol_ctl_reg, 0x20, 0x20);
		if (!is_amic_enabled(component, decimator)) {
			snd_soc_component_update_bits(component,
				hpf_gate_reg, 0x01, 0x00);
			/*
		 	 * Minimum 1 clk cycle delay is required as per HW spec
		 	 */
			usleep_range(1000, 1010);
		}
		hpf_cut_off_freq = (snd_soc_component_read32(
					component, dec_cfg_reg) &
				   TX_HPF_CUT_OFF_FREQ_MASK) >> 5;
		va_priv->va_hpf_work[decimator].hpf_cut_off_freq =
							hpf_cut_off_freq;

		if (hpf_cut_off_freq != CF_MIN_3DB_150HZ) {
			snd_soc_component_update_bits(component, dec_cfg_reg,
					    TX_HPF_CUT_OFF_FREQ_MASK,
					    CF_MIN_3DB_150HZ << 5);
		}
		if (is_amic_enabled(component, decimator) < BOLERO_ADC_MAX) {
			hpf_delay = BOLERO_CDC_VA_TX_AMIC_HPF_DELAY_MS;
			unmute_delay = BOLERO_CDC_VA_TX_AMIC_UNMUTE_DELAY_MS;
			if (va_tx_unmute_delay < unmute_delay)
				va_tx_unmute_delay = unmute_delay;
		}
		snd_soc_component_update_bits(component,
				hpf_gate_reg, 0x03, 0x02);
		if (!is_amic_enabled(component, decimator))
			snd_soc_component_update_bits(component,
				hpf_gate_reg, 0x03, 0x00);
		/*
		 * Minimum 1 clk cycle delay is required as per HW spec
		 */
		usleep_range(1000, 1010);
		snd_soc_component_update_bits(component,
			hpf_gate_reg, 0x03, 0x01);
		/*
		 * 6ms delay is required as per HW spec
		 */
		usleep_range(6000, 6010);
		/* schedule work queue to Remove Mute */
		queue_delayed_work(system_freezable_wq,
				   &va_priv->va_mute_dwork[decimator].dwork,
				   msecs_to_jiffies(va_tx_unmute_delay));
		if (va_priv->va_hpf_work[decimator].hpf_cut_off_freq !=
							CF_MIN_3DB_150HZ)
			queue_delayed_work(system_freezable_wq,
					&va_priv->va_hpf_work[decimator].dwork,
					msecs_to_jiffies(hpf_delay));
		/* apply gain after decimator is enabled */
		snd_soc_component_write(component, tx_gain_ctl_reg,
			snd_soc_component_read32(component, tx_gain_ctl_reg));
		if (va_priv->version == BOLERO_VERSION_2_0) {
			if (snd_soc_component_read32(component, adc_mux_reg)
							& SWR_MIC) {
				snd_soc_component_update_bits(component,
					BOLERO_CDC_TX_TOP_CSR_SWR_CTRL,
					0x01, 0x01);
				snd_soc_component_update_bits(component,
					BOLERO_CDC_TX_TOP_CSR_SWR_MIC0_CTL,
					0x0E, 0x0C);
				snd_soc_component_update_bits(component,
					BOLERO_CDC_TX_TOP_CSR_SWR_MIC1_CTL,
					0x0E, 0x0C);
				snd_soc_component_update_bits(component,
					BOLERO_CDC_TX_TOP_CSR_SWR_MIC2_CTL,
					0x0E, 0x00);
				snd_soc_component_update_bits(component,
					BOLERO_CDC_TX_TOP_CSR_SWR_MIC3_CTL,
					0x0E, 0x00);
				snd_soc_component_update_bits(component,
					BOLERO_CDC_TX_TOP_CSR_SWR_MIC4_CTL,
					0x0E, 0x00);
				snd_soc_component_update_bits(component,
					BOLERO_CDC_TX_TOP_CSR_SWR_MIC5_CTL,
					0x0E, 0x00);
			}
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		hpf_cut_off_freq =
			va_priv->va_hpf_work[decimator].hpf_cut_off_freq;
		snd_soc_component_update_bits(component, tx_vol_ctl_reg,
					0x10, 0x10);
		if (cancel_delayed_work_sync(
		    &va_priv->va_hpf_work[decimator].dwork)) {
			if (hpf_cut_off_freq != CF_MIN_3DB_150HZ) {
				snd_soc_component_update_bits(component,
						dec_cfg_reg,
						TX_HPF_CUT_OFF_FREQ_MASK,
						hpf_cut_off_freq << 5);
				if (is_amic_enabled(component, decimator))
					snd_soc_component_update_bits(component,
						hpf_gate_reg,
						0x03, 0x02);
				else
					snd_soc_component_update_bits(component,
						hpf_gate_reg,
						0x03, 0x03);
				/*
				 * Minimum 1 clk cycle delay is required
				 * as per HW spec
				 */
				usleep_range(1000, 1010);
				snd_soc_component_update_bits(component,
						hpf_gate_reg,
						0x03, 0x01);
			}
		}
		cancel_delayed_work_sync(
				&va_priv->va_mute_dwork[decimator].dwork);
		if (va_priv->version == BOLERO_VERSION_2_0) {
			if (snd_soc_component_read32(component, adc_mux_reg)
							& SWR_MIC)
				snd_soc_component_update_bits(component,
					BOLERO_CDC_TX_TOP_CSR_SWR_CTRL,
					0x01, 0x00);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Disable TX CLK */
		snd_soc_component_update_bits(component, tx_vol_ctl_reg,
					0x20, 0x00);
		snd_soc_component_update_bits(component, tx_vol_ctl_reg,
					0x10, 0x00);
		break;
	}
	return 0;
}

static int va_macro_enable_tx(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;
	int ret = 0;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	dev_dbg(va_dev, "%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (va_priv->dapm_tx_clk_status > 0) {
			ret = bolero_clk_rsc_request_clock(va_priv->dev,
						   va_priv->default_clk_id,
						   TX_CORE_CLK,
						   false);
			va_priv->dapm_tx_clk_status--;
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		ret = bolero_clk_rsc_request_clock(va_priv->dev,
						   va_priv->default_clk_id,
						   TX_CORE_CLK,
						   true);
		if (!ret)
			va_priv->dapm_tx_clk_status++;
		break;
	default:
		dev_err(va_priv->dev,
			"%s: invalid DAPM event %d\n", __func__, event);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int va_macro_enable_micbias(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;
	int ret = 0;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	if (!va_priv->micb_supply) {
		dev_err(va_dev,
			"%s:regulator not provided in dtsi\n", __func__);
		return -EINVAL;
	}
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (va_priv->micb_users++ > 0)
			return 0;
		ret = regulator_set_voltage(va_priv->micb_supply,
				      va_priv->micb_voltage,
				      va_priv->micb_voltage);
		if (ret) {
			dev_err(va_dev, "%s: Setting voltage failed, err = %d\n",
				__func__, ret);
			return ret;
		}
		ret = regulator_set_load(va_priv->micb_supply,
					 va_priv->micb_current);
		if (ret) {
			dev_err(va_dev, "%s: Setting current failed, err = %d\n",
				__func__, ret);
			return ret;
		}
		ret = regulator_enable(va_priv->micb_supply);
		if (ret) {
			dev_err(va_dev, "%s: regulator enable failed, err = %d\n",
				__func__, ret);
			return ret;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (--va_priv->micb_users > 0)
			return 0;
		if (va_priv->micb_users < 0) {
			va_priv->micb_users = 0;
			dev_dbg(va_dev, "%s: regulator already disabled\n",
				__func__);
			return 0;
		}
		ret = regulator_disable(va_priv->micb_supply);
		if (ret) {
			dev_err(va_dev, "%s: regulator disable failed, err = %d\n",
				__func__, ret);
			return ret;
		}
		regulator_set_voltage(va_priv->micb_supply, 0,
				va_priv->micb_voltage);
		regulator_set_load(va_priv->micb_supply, 0);
		break;
	}
	return 0;
}

static inline int va_macro_path_get(const char *wname,
				    unsigned int *path_num)
{
	int ret = 0;
	char *widget_name = NULL;
	char *w_name = NULL;
	char *path_num_char = NULL;
	char *path_name = NULL;

	widget_name = kstrndup(wname, 10, GFP_KERNEL);
	if (!widget_name)
		return -EINVAL;

	w_name = widget_name;

	path_name = strsep(&widget_name, " ");
	if (!path_name) {
		pr_err("%s: Invalid widget name = %s\n",
			__func__, widget_name);
		ret = -EINVAL;
		goto err;
	}
	path_num_char = strpbrk(path_name, "01234567");
	if (!path_num_char) {
		pr_err("%s: va path index not found\n",
			__func__);
		ret = -EINVAL;
		goto err;
	}
	ret = kstrtouint(path_num_char, 10, path_num);
	if (ret < 0)
		pr_err("%s: Invalid tx path = %s\n",
			__func__, w_name);

err:
	kfree(w_name);
	return ret;
}

static int va_macro_dec_mode_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct va_macro_priv *priv = NULL;
	struct device *va_dev = NULL;
	int ret = 0;
	int path = 0;

	if (!va_macro_get_data(component, &va_dev, &priv, __func__))
		return -EINVAL;

	ret = va_macro_path_get(kcontrol->id.name, &path);
	if (ret)
		return ret;

	ucontrol->value.integer.value[0] = priv->dec_mode[path];

	return 0;
}

static int va_macro_dec_mode_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct va_macro_priv *priv = NULL;
	struct device *va_dev = NULL;
	int value = ucontrol->value.integer.value[0];
	int ret = 0;
	int path = 0;

	if (!va_macro_get_data(component, &va_dev, &priv, __func__))
		return -EINVAL;

	ret = va_macro_path_get(kcontrol->id.name, &path);
	if (ret)
		return ret;

	priv->dec_mode[path] = value;

	return 0;
}

static int va_macro_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	int tx_fs_rate = -EINVAL;
	struct snd_soc_component *component = dai->component;
	u32 decimator, sample_rate;
	u16 tx_fs_reg = 0;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	dev_dbg(va_dev,
		"%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n", __func__,
		dai->name, dai->id, params_rate(params),
		params_channels(params));

	sample_rate = params_rate(params);
	if (sample_rate > 16000)
		va_priv->clk_div_switch = true;
	else
		va_priv->clk_div_switch = false;
	switch (sample_rate) {
	case 8000:
		tx_fs_rate = 0;
		break;
	case 16000:
		tx_fs_rate = 1;
		break;
	case 32000:
		tx_fs_rate = 3;
		break;
	case 48000:
		tx_fs_rate = 4;
		break;
	case 96000:
		tx_fs_rate = 5;
		break;
	case 192000:
		tx_fs_rate = 6;
		break;
	case 384000:
		tx_fs_rate = 7;
		break;
	default:
		dev_err(va_dev, "%s: Invalid TX sample rate: %d\n",
			__func__, params_rate(params));
		return -EINVAL;
	}
	for_each_set_bit(decimator, &va_priv->active_ch_mask[dai->id],
			 VA_MACRO_DEC_MAX) {
		if (decimator >= 0) {
			tx_fs_reg = BOLERO_CDC_VA_TX0_TX_PATH_CTL +
				    VA_MACRO_TX_PATH_OFFSET * decimator;
			dev_dbg(va_dev, "%s: set DEC%u rate to %u\n",
				__func__, decimator, sample_rate);
			snd_soc_component_update_bits(component, tx_fs_reg,
						0x0F, tx_fs_rate);
		} else {
			dev_err(va_dev,
				"%s: ERROR: Invalid decimator: %d\n",
				__func__, decimator);
			return -EINVAL;
		}
	}
	return 0;
}

static int va_macro_get_channel_map(struct snd_soc_dai *dai,
				unsigned int *tx_num, unsigned int *tx_slot,
				unsigned int *rx_num, unsigned int *rx_slot)
{
	struct snd_soc_component *component = dai->component;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	switch (dai->id) {
	case VA_MACRO_AIF1_CAP:
	case VA_MACRO_AIF2_CAP:
	case VA_MACRO_AIF3_CAP:
		*tx_slot = va_priv->active_ch_mask[dai->id];
		*tx_num = va_priv->active_ch_cnt[dai->id];
		break;
	default:
		dev_err(va_dev, "%s: Invalid AIF\n", __func__);
		break;
	}
	return 0;
}

static struct snd_soc_dai_ops va_macro_dai_ops = {
	.hw_params = va_macro_hw_params,
	.get_channel_map = va_macro_get_channel_map,
};

static struct snd_soc_dai_driver va_macro_dai[] = {
	{
		.name = "va_macro_tx1",
		.id = VA_MACRO_AIF1_CAP,
		.capture = {
			.stream_name = "VA_AIF1 Capture",
			.rates = VA_MACRO_RATES,
			.formats = VA_MACRO_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 8,
		},
		.ops = &va_macro_dai_ops,
	},
	{
		.name = "va_macro_tx2",
		.id = VA_MACRO_AIF2_CAP,
		.capture = {
			.stream_name = "VA_AIF2 Capture",
			.rates = VA_MACRO_RATES,
			.formats = VA_MACRO_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 8,
		},
		.ops = &va_macro_dai_ops,
	},
	{
		.name = "va_macro_tx3",
		.id = VA_MACRO_AIF3_CAP,
		.capture = {
			.stream_name = "VA_AIF3 Capture",
			.rates = VA_MACRO_RATES,
			.formats = VA_MACRO_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 8,
		},
		.ops = &va_macro_dai_ops,
	},
};

#define STRING(name) #name
#define VA_MACRO_DAPM_ENUM(name, reg, offset, text) \
static SOC_ENUM_SINGLE_DECL(name##_enum, reg, offset, text); \
static const struct snd_kcontrol_new name##_mux = \
		SOC_DAPM_ENUM(STRING(name), name##_enum)

#define VA_MACRO_DAPM_ENUM_EXT(name, reg, offset, text, getname, putname) \
static SOC_ENUM_SINGLE_DECL(name##_enum, reg, offset, text); \
static const struct snd_kcontrol_new name##_mux = \
		SOC_DAPM_ENUM_EXT(STRING(name), name##_enum, getname, putname)

#define VA_MACRO_DAPM_MUX(name, shift, kctl) \
		SND_SOC_DAPM_MUX(name, SND_SOC_NOPM, shift, 0, &kctl##_mux)

static const char * const adc_mux_text[] = {
	"MSM_DMIC", "SWR_MIC"
};

VA_MACRO_DAPM_ENUM(va_dec0, BOLERO_CDC_VA_INP_MUX_ADC_MUX0_CFG1,
		   0, adc_mux_text);
VA_MACRO_DAPM_ENUM(va_dec1, BOLERO_CDC_VA_INP_MUX_ADC_MUX1_CFG1,
		   0, adc_mux_text);
VA_MACRO_DAPM_ENUM(va_dec2, BOLERO_CDC_VA_INP_MUX_ADC_MUX2_CFG1,
		   0, adc_mux_text);
VA_MACRO_DAPM_ENUM(va_dec3, BOLERO_CDC_VA_INP_MUX_ADC_MUX3_CFG1,
		   0, adc_mux_text);
VA_MACRO_DAPM_ENUM(va_dec4, BOLERO_CDC_VA_INP_MUX_ADC_MUX4_CFG1,
		   0, adc_mux_text);
VA_MACRO_DAPM_ENUM(va_dec5, BOLERO_CDC_VA_INP_MUX_ADC_MUX5_CFG1,
		   0, adc_mux_text);
VA_MACRO_DAPM_ENUM(va_dec6, BOLERO_CDC_VA_INP_MUX_ADC_MUX6_CFG1,
		   0, adc_mux_text);
VA_MACRO_DAPM_ENUM(va_dec7, BOLERO_CDC_VA_INP_MUX_ADC_MUX7_CFG1,
		   0, adc_mux_text);

static const char * const dmic_mux_text[] = {
	"ZERO", "DMIC0", "DMIC1", "DMIC2", "DMIC3",
	"DMIC4", "DMIC5", "DMIC6", "DMIC7"
};

VA_MACRO_DAPM_ENUM_EXT(va_dmic0, BOLERO_CDC_VA_INP_MUX_ADC_MUX0_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_dmic1, BOLERO_CDC_VA_INP_MUX_ADC_MUX1_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_dmic2, BOLERO_CDC_VA_INP_MUX_ADC_MUX2_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_dmic3, BOLERO_CDC_VA_INP_MUX_ADC_MUX3_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_dmic4, BOLERO_CDC_VA_INP_MUX_ADC_MUX4_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_dmic5, BOLERO_CDC_VA_INP_MUX_ADC_MUX5_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_dmic6, BOLERO_CDC_VA_INP_MUX_ADC_MUX6_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_dmic7, BOLERO_CDC_VA_INP_MUX_ADC_MUX7_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

static const char * const smic_mux_text[] = {
	"ZERO", "ADC0", "ADC1", "ADC2", "ADC3",
	"SWR_DMIC0", "SWR_DMIC1", "SWR_DMIC2", "SWR_DMIC3",
	"SWR_DMIC4", "SWR_DMIC5", "SWR_DMIC6", "SWR_DMIC7"
};

VA_MACRO_DAPM_ENUM_EXT(va_smic0, BOLERO_CDC_VA_INP_MUX_ADC_MUX0_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic1, BOLERO_CDC_VA_INP_MUX_ADC_MUX1_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic2, BOLERO_CDC_VA_INP_MUX_ADC_MUX2_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic3, BOLERO_CDC_VA_INP_MUX_ADC_MUX3_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic4, BOLERO_CDC_VA_INP_MUX_ADC_MUX4_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic5, BOLERO_CDC_VA_INP_MUX_ADC_MUX5_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic6, BOLERO_CDC_VA_INP_MUX_ADC_MUX6_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic7, BOLERO_CDC_VA_INP_MUX_ADC_MUX7_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

static const char * const smic_mux_text_v2[] = {
	"ZERO", "SWR_MIC0", "SWR_MIC1", "SWR_MIC2", "SWR_MIC3",
	"SWR_MIC4", "SWR_MIC5", "SWR_MIC6", "SWR_MIC7",
	"SWR_MIC8", "SWR_MIC9", "SWR_MIC10", "SWR_MIC11"
};

VA_MACRO_DAPM_ENUM_EXT(va_smic0_v2, BOLERO_CDC_VA_INP_MUX_ADC_MUX0_CFG0,
			0, smic_mux_text_v2, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic1_v2, BOLERO_CDC_VA_INP_MUX_ADC_MUX1_CFG0,
			0, smic_mux_text_v2, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic2_v3, BOLERO_CDC_VA_INP_MUX_ADC_MUX2_CFG0,
			0, smic_mux_text_v2, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

VA_MACRO_DAPM_ENUM_EXT(va_smic3_v3, BOLERO_CDC_VA_INP_MUX_ADC_MUX3_CFG0,
			0, smic_mux_text_v2, snd_soc_dapm_get_enum_double,
			va_macro_put_dec_enum);

static const struct snd_kcontrol_new va_aif1_cap_mixer[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, VA_MACRO_DEC0, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, VA_MACRO_DEC1, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC2", SND_SOC_NOPM, VA_MACRO_DEC2, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC3", SND_SOC_NOPM, VA_MACRO_DEC3, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC4", SND_SOC_NOPM, VA_MACRO_DEC4, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC5", SND_SOC_NOPM, VA_MACRO_DEC5, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC6", SND_SOC_NOPM, VA_MACRO_DEC6, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC7", SND_SOC_NOPM, VA_MACRO_DEC7, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
};

static const struct snd_kcontrol_new va_aif2_cap_mixer[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, VA_MACRO_DEC0, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, VA_MACRO_DEC1, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC2", SND_SOC_NOPM, VA_MACRO_DEC2, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC3", SND_SOC_NOPM, VA_MACRO_DEC3, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC4", SND_SOC_NOPM, VA_MACRO_DEC4, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC5", SND_SOC_NOPM, VA_MACRO_DEC5, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC6", SND_SOC_NOPM, VA_MACRO_DEC6, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC7", SND_SOC_NOPM, VA_MACRO_DEC7, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
};

static const struct snd_kcontrol_new va_aif3_cap_mixer[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, VA_MACRO_DEC0, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, VA_MACRO_DEC1, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC2", SND_SOC_NOPM, VA_MACRO_DEC2, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC3", SND_SOC_NOPM, VA_MACRO_DEC3, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC4", SND_SOC_NOPM, VA_MACRO_DEC4, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC5", SND_SOC_NOPM, VA_MACRO_DEC5, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC6", SND_SOC_NOPM, VA_MACRO_DEC6, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC7", SND_SOC_NOPM, VA_MACRO_DEC7, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
};

static const struct snd_kcontrol_new va_aif1_cap_mixer_v2[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, VA_MACRO_DEC0, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, VA_MACRO_DEC1, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
};

static const struct snd_kcontrol_new va_aif2_cap_mixer_v2[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, VA_MACRO_DEC0, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, VA_MACRO_DEC1, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
};

static const struct snd_kcontrol_new va_aif3_cap_mixer_v2[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, VA_MACRO_DEC0, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, VA_MACRO_DEC1, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
};

static const struct snd_kcontrol_new va_aif1_cap_mixer_v3[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, VA_MACRO_DEC0, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, VA_MACRO_DEC1, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC2", SND_SOC_NOPM, VA_MACRO_DEC2, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC3", SND_SOC_NOPM, VA_MACRO_DEC3, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
};

static const struct snd_kcontrol_new va_aif2_cap_mixer_v3[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, VA_MACRO_DEC0, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, VA_MACRO_DEC1, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC2", SND_SOC_NOPM, VA_MACRO_DEC2, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC3", SND_SOC_NOPM, VA_MACRO_DEC3, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
};

static const struct snd_kcontrol_new va_aif3_cap_mixer_v3[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, VA_MACRO_DEC0, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, VA_MACRO_DEC1, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC2", SND_SOC_NOPM, VA_MACRO_DEC2, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC3", SND_SOC_NOPM, VA_MACRO_DEC3, 1, 0,
			va_macro_tx_mixer_get, va_macro_tx_mixer_put),
};

static const struct snd_soc_dapm_widget va_macro_dapm_widgets_common[] = {
	SND_SOC_DAPM_AIF_OUT_E("VA_AIF1 CAP", "VA_AIF1 Capture", 0,
		SND_SOC_NOPM, VA_MACRO_AIF1_CAP, 0,
		va_macro_enable_tx, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_AIF_OUT_E("VA_AIF2 CAP", "VA_AIF2 Capture", 0,
		SND_SOC_NOPM, VA_MACRO_AIF2_CAP, 0,
		va_macro_enable_tx, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_AIF_OUT_E("VA_AIF3 CAP", "VA_AIF3 Capture", 0,
		SND_SOC_NOPM, VA_MACRO_AIF3_CAP, 0,
		va_macro_enable_tx, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD),

	VA_MACRO_DAPM_MUX("VA DMIC MUX0", 0, va_dmic0),
	VA_MACRO_DAPM_MUX("VA DMIC MUX1", 0, va_dmic1),

	VA_MACRO_DAPM_MUX("VA SMIC MUX0", 0, va_smic0_v2),
	VA_MACRO_DAPM_MUX("VA SMIC MUX1", 0, va_smic1_v2),

	SND_SOC_DAPM_INPUT("VA SWR_INPUT"),

	SND_SOC_DAPM_SUPPLY("VA MIC BIAS1", SND_SOC_NOPM, 0, 0,
		va_macro_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC0", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC1", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC2", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC3", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC4", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC5", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC6", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC7", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC0 MUX", SND_SOC_NOPM, VA_MACRO_DEC0, 0,
			   &va_dec0_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC1 MUX", SND_SOC_NOPM, VA_MACRO_DEC1, 0,
			   &va_dec1_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("VA_MCLK", -1, SND_SOC_NOPM, 0, 0,
			      va_macro_mclk_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_widget va_macro_dapm_widgets_v2[] = {
	SND_SOC_DAPM_MIXER("VA_AIF1_CAP Mixer", SND_SOC_NOPM,
		VA_MACRO_AIF1_CAP, 0,
		va_aif1_cap_mixer_v2, ARRAY_SIZE(va_aif1_cap_mixer_v2)),

	SND_SOC_DAPM_MIXER("VA_AIF2_CAP Mixer", SND_SOC_NOPM,
		VA_MACRO_AIF2_CAP, 0,
		va_aif2_cap_mixer_v2, ARRAY_SIZE(va_aif2_cap_mixer_v2)),

	SND_SOC_DAPM_MIXER("VA_AIF3_CAP Mixer", SND_SOC_NOPM,
		VA_MACRO_AIF3_CAP, 0,
		va_aif3_cap_mixer_v2, ARRAY_SIZE(va_aif3_cap_mixer_v2)),

	SND_SOC_DAPM_SUPPLY_S("VA_SWR_PWR", 0, SND_SOC_NOPM, 0, 0,
			      va_macro_swr_pwr_event_v2,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("VA_TX_SWR_CLK", -1, SND_SOC_NOPM, 0, 0,
			      va_macro_tx_swr_clk_event_v2,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("VA_SWR_CLK", -1, SND_SOC_NOPM, 0, 0,
			      va_macro_swr_clk_event_v2,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_widget va_macro_dapm_widgets_v3[] = {
	SND_SOC_DAPM_MIXER("VA_AIF1_CAP Mixer", SND_SOC_NOPM,
		VA_MACRO_AIF1_CAP, 0,
		va_aif1_cap_mixer_v3, ARRAY_SIZE(va_aif1_cap_mixer_v3)),

	SND_SOC_DAPM_MIXER("VA_AIF2_CAP Mixer", SND_SOC_NOPM,
		VA_MACRO_AIF2_CAP, 0,
		va_aif2_cap_mixer_v3, ARRAY_SIZE(va_aif2_cap_mixer_v3)),

	SND_SOC_DAPM_MIXER("VA_AIF3_CAP Mixer", SND_SOC_NOPM,
		VA_MACRO_AIF3_CAP, 0,
		va_aif3_cap_mixer_v3, ARRAY_SIZE(va_aif3_cap_mixer_v3)),

	VA_MACRO_DAPM_MUX("VA DMIC MUX2", 0, va_dmic2),
	VA_MACRO_DAPM_MUX("VA DMIC MUX3", 0, va_dmic3),

	VA_MACRO_DAPM_MUX("VA SMIC MUX2", 0, va_smic2_v3),
	VA_MACRO_DAPM_MUX("VA SMIC MUX3", 0, va_smic3_v3),

	SND_SOC_DAPM_MUX_E("VA DEC2 MUX", SND_SOC_NOPM, VA_MACRO_DEC2, 0,
			   &va_dec2_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC3 MUX", SND_SOC_NOPM, VA_MACRO_DEC3, 0,
			   &va_dec3_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("VA_SWR_PWR", 0, SND_SOC_NOPM, 0, 0,
			      va_macro_swr_pwr_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_widget va_macro_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_OUT_E("VA_AIF1 CAP", "VA_AIF1 Capture", 0,
		SND_SOC_NOPM, VA_MACRO_AIF1_CAP, 0,
		va_macro_enable_tx, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_AIF_OUT_E("VA_AIF2 CAP", "VA_AIF2 Capture", 0,
		SND_SOC_NOPM, VA_MACRO_AIF2_CAP, 0,
		va_macro_enable_tx, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_AIF_OUT_E("VA_AIF3 CAP", "VA_AIF3 Capture", 0,
		SND_SOC_NOPM, VA_MACRO_AIF3_CAP, 0,
		va_macro_enable_tx, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_MIXER("VA_AIF1_CAP Mixer", SND_SOC_NOPM,
		VA_MACRO_AIF1_CAP, 0,
		va_aif1_cap_mixer, ARRAY_SIZE(va_aif1_cap_mixer)),

	SND_SOC_DAPM_MIXER("VA_AIF2_CAP Mixer", SND_SOC_NOPM,
		VA_MACRO_AIF2_CAP, 0,
		va_aif2_cap_mixer, ARRAY_SIZE(va_aif2_cap_mixer)),

	SND_SOC_DAPM_MIXER("VA_AIF3_CAP Mixer", SND_SOC_NOPM,
		VA_MACRO_AIF3_CAP, 0,
		va_aif3_cap_mixer, ARRAY_SIZE(va_aif3_cap_mixer)),

	VA_MACRO_DAPM_MUX("VA DMIC MUX0", 0, va_dmic0),
	VA_MACRO_DAPM_MUX("VA DMIC MUX1", 0, va_dmic1),
	VA_MACRO_DAPM_MUX("VA DMIC MUX2", 0, va_dmic2),
	VA_MACRO_DAPM_MUX("VA DMIC MUX3", 0, va_dmic3),
	VA_MACRO_DAPM_MUX("VA DMIC MUX4", 0, va_dmic4),
	VA_MACRO_DAPM_MUX("VA DMIC MUX5", 0, va_dmic5),
	VA_MACRO_DAPM_MUX("VA DMIC MUX6", 0, va_dmic6),
	VA_MACRO_DAPM_MUX("VA DMIC MUX7", 0, va_dmic7),

	VA_MACRO_DAPM_MUX("VA SMIC MUX0", 0, va_smic0),
	VA_MACRO_DAPM_MUX("VA SMIC MUX1", 0, va_smic1),
	VA_MACRO_DAPM_MUX("VA SMIC MUX2", 0, va_smic2),
	VA_MACRO_DAPM_MUX("VA SMIC MUX3", 0, va_smic3),
	VA_MACRO_DAPM_MUX("VA SMIC MUX4", 0, va_smic4),
	VA_MACRO_DAPM_MUX("VA SMIC MUX5", 0, va_smic5),
	VA_MACRO_DAPM_MUX("VA SMIC MUX6", 0, va_smic6),
	VA_MACRO_DAPM_MUX("VA SMIC MUX7", 0, va_smic7),

	SND_SOC_DAPM_SUPPLY("VA MIC BIAS1", SND_SOC_NOPM, 0, 0,
		va_macro_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC0", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC1", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC2", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC3", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC4", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC5", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC6", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("VA DMIC7", NULL, SND_SOC_NOPM, 0, 0,
		va_macro_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("VA SWR_ADC0"),
	SND_SOC_DAPM_INPUT("VA SWR_ADC1"),
	SND_SOC_DAPM_INPUT("VA SWR_ADC2"),
	SND_SOC_DAPM_INPUT("VA SWR_ADC3"),
	SND_SOC_DAPM_INPUT("VA SWR_MIC0"),
	SND_SOC_DAPM_INPUT("VA SWR_MIC1"),
	SND_SOC_DAPM_INPUT("VA SWR_MIC2"),
	SND_SOC_DAPM_INPUT("VA SWR_MIC3"),
	SND_SOC_DAPM_INPUT("VA SWR_MIC4"),
	SND_SOC_DAPM_INPUT("VA SWR_MIC5"),
	SND_SOC_DAPM_INPUT("VA SWR_MIC6"),
	SND_SOC_DAPM_INPUT("VA SWR_MIC7"),

	SND_SOC_DAPM_MUX_E("VA DEC0 MUX", SND_SOC_NOPM, VA_MACRO_DEC0, 0,
			   &va_dec0_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC1 MUX", SND_SOC_NOPM, VA_MACRO_DEC1, 0,
			   &va_dec1_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC2 MUX", SND_SOC_NOPM, VA_MACRO_DEC2, 0,
			   &va_dec2_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC3 MUX", SND_SOC_NOPM, VA_MACRO_DEC3, 0,
			   &va_dec3_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC4 MUX", SND_SOC_NOPM, VA_MACRO_DEC4, 0,
			   &va_dec4_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC5 MUX", SND_SOC_NOPM, VA_MACRO_DEC5, 0,
			   &va_dec5_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC6 MUX", SND_SOC_NOPM, VA_MACRO_DEC6, 0,
			   &va_dec6_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("VA DEC7 MUX", SND_SOC_NOPM, VA_MACRO_DEC7, 0,
			   &va_dec7_mux, va_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("VA_SWR_PWR", -1, SND_SOC_NOPM, 0, 0,
			      va_macro_swr_pwr_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("VA_MCLK", -1, SND_SOC_NOPM, 0, 0,
			      va_macro_mclk_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_widget va_macro_wod_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY_S("VA_MCLK", -1, SND_SOC_NOPM, 0, 0,
			      va_macro_mclk_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route va_audio_map_common[] = {
	{"VA_AIF1 CAP", NULL, "VA_MCLK"},
	{"VA_AIF2 CAP", NULL, "VA_MCLK"},
	{"VA_AIF3 CAP", NULL, "VA_MCLK"},

	{"VA_AIF1 CAP", NULL, "VA_AIF1_CAP Mixer"},
	{"VA_AIF2 CAP", NULL, "VA_AIF2_CAP Mixer"},
	{"VA_AIF3 CAP", NULL, "VA_AIF3_CAP Mixer"},

	{"VA_AIF1_CAP Mixer", "DEC0", "VA DEC0 MUX"},
	{"VA_AIF1_CAP Mixer", "DEC1", "VA DEC1 MUX"},

	{"VA_AIF2_CAP Mixer", "DEC0", "VA DEC0 MUX"},
	{"VA_AIF2_CAP Mixer", "DEC1", "VA DEC1 MUX"},

	{"VA_AIF3_CAP Mixer", "DEC0", "VA DEC0 MUX"},
	{"VA_AIF3_CAP Mixer", "DEC1", "VA DEC1 MUX"},

	{"VA DEC0 MUX", "MSM_DMIC", "VA DMIC MUX0"},
	{"VA DMIC MUX0", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX0", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX0", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX0", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX0", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX0", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX0", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX0", "DMIC7", "VA DMIC7"},

	{"VA DEC0 MUX", "SWR_MIC", "VA SMIC MUX0"},
	{"VA SMIC MUX0", "SWR_MIC0", "VA SWR_INPUT"},
	{"VA SMIC MUX0", "SWR_MIC1", "VA SWR_INPUT"},
	{"VA SMIC MUX0", "SWR_MIC2", "VA SWR_INPUT"},
	{"VA SMIC MUX0", "SWR_MIC3", "VA SWR_INPUT"},
	{"VA SMIC MUX0", "SWR_MIC4", "VA SWR_INPUT"},
	{"VA SMIC MUX0", "SWR_MIC5", "VA SWR_INPUT"},
	{"VA SMIC MUX0", "SWR_MIC6", "VA SWR_INPUT"},
	{"VA SMIC MUX0", "SWR_MIC7", "VA SWR_INPUT"},
	{"VA SMIC MUX0", "SWR_MIC8", "VA SWR_INPUT"},
	{"VA SMIC MUX0", "SWR_MIC9", "VA SWR_INPUT"},
	{"VA SMIC MUX0", "SWR_MIC10", "VA SWR_INPUT"},
	{"VA SMIC MUX0", "SWR_MIC11", "VA SWR_INPUT"},

	{"VA DEC1 MUX", "MSM_DMIC", "VA DMIC MUX1"},
	{"VA DMIC MUX1", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX1", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX1", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX1", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX1", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX1", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX1", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX1", "DMIC7", "VA DMIC7"},

	{"VA DEC1 MUX", "SWR_MIC", "VA SMIC MUX1"},
	{"VA SMIC MUX1", "SWR_MIC0", "VA SWR_INPUT"},
	{"VA SMIC MUX1", "SWR_MIC1", "VA SWR_INPUT"},
	{"VA SMIC MUX1", "SWR_MIC2", "VA SWR_INPUT"},
	{"VA SMIC MUX1", "SWR_MIC3", "VA SWR_INPUT"},
	{"VA SMIC MUX1", "SWR_MIC4", "VA SWR_INPUT"},
	{"VA SMIC MUX1", "SWR_MIC5", "VA SWR_INPUT"},
	{"VA SMIC MUX1", "SWR_MIC6", "VA SWR_INPUT"},
	{"VA SMIC MUX1", "SWR_MIC7", "VA SWR_INPUT"},
	{"VA SMIC MUX1", "SWR_MIC8", "VA SWR_INPUT"},
	{"VA SMIC MUX1", "SWR_MIC9", "VA SWR_INPUT"},
	{"VA SMIC MUX1", "SWR_MIC10", "VA SWR_INPUT"},
	{"VA SMIC MUX1", "SWR_MIC11", "VA SWR_INPUT"},

	{"VA SWR_INPUT", NULL, "VA_SWR_PWR"},
};

static const struct snd_soc_dapm_route va_audio_map_v3[] = {
	{"VA_AIF1_CAP Mixer", "DEC2", "VA DEC2 MUX"},
	{"VA_AIF1_CAP Mixer", "DEC3", "VA DEC3 MUX"},

	{"VA_AIF2_CAP Mixer", "DEC2", "VA DEC2 MUX"},
	{"VA_AIF2_CAP Mixer", "DEC3", "VA DEC3 MUX"},

	{"VA_AIF3_CAP Mixer", "DEC2", "VA DEC2 MUX"},
	{"VA_AIF3_CAP Mixer", "DEC3", "VA DEC3 MUX"},

	{"VA DEC2 MUX", "MSM_DMIC", "VA DMIC MUX2"},
	{"VA DMIC MUX2", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX2", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX2", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX2", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX2", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX2", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX2", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX2", "DMIC7", "VA DMIC7"},

	{"VA DEC2 MUX", "SWR_MIC", "VA SMIC MUX2"},
	{"VA SMIC MUX2", "SWR_MIC0", "VA SWR_INPUT"},
	{"VA SMIC MUX2", "SWR_MIC1", "VA SWR_INPUT"},
	{"VA SMIC MUX2", "SWR_MIC2", "VA SWR_INPUT"},
	{"VA SMIC MUX2", "SWR_MIC3", "VA SWR_INPUT"},
	{"VA SMIC MUX2", "SWR_MIC4", "VA SWR_INPUT"},
	{"VA SMIC MUX2", "SWR_MIC5", "VA SWR_INPUT"},
	{"VA SMIC MUX2", "SWR_MIC6", "VA SWR_INPUT"},
	{"VA SMIC MUX2", "SWR_MIC7", "VA SWR_INPUT"},
	{"VA SMIC MUX2", "SWR_MIC8", "VA SWR_INPUT"},
	{"VA SMIC MUX2", "SWR_MIC9", "VA SWR_INPUT"},
	{"VA SMIC MUX2", "SWR_MIC10", "VA SWR_INPUT"},
	{"VA SMIC MUX2", "SWR_MIC11", "VA SWR_INPUT"},

	{"VA DEC3 MUX", "MSM_DMIC", "VA DMIC MUX3"},
	{"VA DMIC MUX3", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX3", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX3", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX3", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX3", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX3", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX3", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX3", "DMIC7", "VA DMIC7"},

	{"VA DEC3 MUX", "SWR_MIC", "VA SMIC MUX3"},
	{"VA SMIC MUX3", "SWR_MIC0", "VA SWR_INPUT"},
	{"VA SMIC MUX3", "SWR_MIC1", "VA SWR_INPUT"},
	{"VA SMIC MUX3", "SWR_MIC2", "VA SWR_INPUT"},
	{"VA SMIC MUX3", "SWR_MIC3", "VA SWR_INPUT"},
	{"VA SMIC MUX3", "SWR_MIC4", "VA SWR_INPUT"},
	{"VA SMIC MUX3", "SWR_MIC5", "VA SWR_INPUT"},
	{"VA SMIC MUX3", "SWR_MIC6", "VA SWR_INPUT"},
	{"VA SMIC MUX3", "SWR_MIC7", "VA SWR_INPUT"},
	{"VA SMIC MUX3", "SWR_MIC8", "VA SWR_INPUT"},
	{"VA SMIC MUX3", "SWR_MIC9", "VA SWR_INPUT"},
	{"VA SMIC MUX3", "SWR_MIC10", "VA SWR_INPUT"},
	{"VA SMIC MUX3", "SWR_MIC11", "VA SWR_INPUT"},
};

static const struct snd_soc_dapm_route va_audio_map_v2[] = {
	{"VA SWR_INPUT", NULL, "VA_SWR_CLK"},
};

static const struct snd_soc_dapm_route va_audio_map[] = {
	{"VA_AIF1 CAP", NULL, "VA_MCLK"},
	{"VA_AIF2 CAP", NULL, "VA_MCLK"},
	{"VA_AIF3 CAP", NULL, "VA_MCLK"},

	{"VA_AIF1 CAP", NULL, "VA_AIF1_CAP Mixer"},
	{"VA_AIF2 CAP", NULL, "VA_AIF2_CAP Mixer"},
	{"VA_AIF3 CAP", NULL, "VA_AIF3_CAP Mixer"},

	{"VA_AIF1_CAP Mixer", "DEC0", "VA DEC0 MUX"},
	{"VA_AIF1_CAP Mixer", "DEC1", "VA DEC1 MUX"},
	{"VA_AIF1_CAP Mixer", "DEC2", "VA DEC2 MUX"},
	{"VA_AIF1_CAP Mixer", "DEC3", "VA DEC3 MUX"},
	{"VA_AIF1_CAP Mixer", "DEC4", "VA DEC4 MUX"},
	{"VA_AIF1_CAP Mixer", "DEC5", "VA DEC5 MUX"},
	{"VA_AIF1_CAP Mixer", "DEC6", "VA DEC6 MUX"},
	{"VA_AIF1_CAP Mixer", "DEC7", "VA DEC7 MUX"},

	{"VA_AIF2_CAP Mixer", "DEC0", "VA DEC0 MUX"},
	{"VA_AIF2_CAP Mixer", "DEC1", "VA DEC1 MUX"},
	{"VA_AIF2_CAP Mixer", "DEC2", "VA DEC2 MUX"},
	{"VA_AIF2_CAP Mixer", "DEC3", "VA DEC3 MUX"},
	{"VA_AIF2_CAP Mixer", "DEC4", "VA DEC4 MUX"},
	{"VA_AIF2_CAP Mixer", "DEC5", "VA DEC5 MUX"},
	{"VA_AIF2_CAP Mixer", "DEC6", "VA DEC6 MUX"},
	{"VA_AIF2_CAP Mixer", "DEC7", "VA DEC7 MUX"},

	{"VA_AIF3_CAP Mixer", "DEC0", "VA DEC0 MUX"},
	{"VA_AIF3_CAP Mixer", "DEC1", "VA DEC1 MUX"},
	{"VA_AIF3_CAP Mixer", "DEC2", "VA DEC2 MUX"},
	{"VA_AIF3_CAP Mixer", "DEC3", "VA DEC3 MUX"},
	{"VA_AIF3_CAP Mixer", "DEC4", "VA DEC4 MUX"},
	{"VA_AIF3_CAP Mixer", "DEC5", "VA DEC5 MUX"},
	{"VA_AIF3_CAP Mixer", "DEC6", "VA DEC6 MUX"},
	{"VA_AIF3_CAP Mixer", "DEC7", "VA DEC7 MUX"},

	{"VA DEC0 MUX", "MSM_DMIC", "VA DMIC MUX0"},
	{"VA DMIC MUX0", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX0", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX0", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX0", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX0", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX0", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX0", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX0", "DMIC7", "VA DMIC7"},

	{"VA DEC0 MUX", "SWR_MIC", "VA SMIC MUX0"},
	{"VA SMIC MUX0", "ADC0", "VA SWR_ADC0"},
	{"VA SMIC MUX0", "ADC1", "VA SWR_ADC1"},
	{"VA SMIC MUX0", "ADC2", "VA SWR_ADC2"},
	{"VA SMIC MUX0", "ADC3", "VA SWR_ADC3"},
	{"VA SMIC MUX0", "SWR_DMIC0", "VA SWR_MIC0"},
	{"VA SMIC MUX0", "SWR_DMIC1", "VA SWR_MIC1"},
	{"VA SMIC MUX0", "SWR_DMIC2", "VA SWR_MIC2"},
	{"VA SMIC MUX0", "SWR_DMIC3", "VA SWR_MIC3"},
	{"VA SMIC MUX0", "SWR_DMIC4", "VA SWR_MIC4"},
	{"VA SMIC MUX0", "SWR_DMIC5", "VA SWR_MIC5"},
	{"VA SMIC MUX0", "SWR_DMIC6", "VA SWR_MIC6"},
	{"VA SMIC MUX0", "SWR_DMIC7", "VA SWR_MIC7"},

	{"VA DEC1 MUX", "MSM_DMIC", "VA DMIC MUX1"},
	{"VA DMIC MUX1", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX1", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX1", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX1", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX1", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX1", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX1", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX1", "DMIC7", "VA DMIC7"},

	{"VA DEC1 MUX", "SWR_MIC", "VA SMIC MUX1"},
	{"VA SMIC MUX1", "ADC0", "VA SWR_ADC0"},
	{"VA SMIC MUX1", "ADC1", "VA SWR_ADC1"},
	{"VA SMIC MUX1", "ADC2", "VA SWR_ADC2"},
	{"VA SMIC MUX1", "ADC3", "VA SWR_ADC3"},
	{"VA SMIC MUX1", "SWR_DMIC0", "VA SWR_MIC0"},
	{"VA SMIC MUX1", "SWR_DMIC1", "VA SWR_MIC1"},
	{"VA SMIC MUX1", "SWR_DMIC2", "VA SWR_MIC2"},
	{"VA SMIC MUX1", "SWR_DMIC3", "VA SWR_MIC3"},
	{"VA SMIC MUX1", "SWR_DMIC4", "VA SWR_MIC4"},
	{"VA SMIC MUX1", "SWR_DMIC5", "VA SWR_MIC5"},
	{"VA SMIC MUX1", "SWR_DMIC6", "VA SWR_MIC6"},
	{"VA SMIC MUX1", "SWR_DMIC7", "VA SWR_MIC7"},

	{"VA DEC2 MUX", "MSM_DMIC", "VA DMIC MUX2"},
	{"VA DMIC MUX2", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX2", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX2", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX2", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX2", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX2", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX2", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX2", "DMIC7", "VA DMIC7"},

	{"VA DEC2 MUX", "SWR_MIC", "VA SMIC MUX2"},
	{"VA SMIC MUX2", "ADC0", "VA SWR_ADC0"},
	{"VA SMIC MUX2", "ADC1", "VA SWR_ADC1"},
	{"VA SMIC MUX2", "ADC2", "VA SWR_ADC2"},
	{"VA SMIC MUX2", "ADC3", "VA SWR_ADC3"},
	{"VA SMIC MUX2", "SWR_DMIC0", "VA SWR_MIC0"},
	{"VA SMIC MUX2", "SWR_DMIC1", "VA SWR_MIC1"},
	{"VA SMIC MUX2", "SWR_DMIC2", "VA SWR_MIC2"},
	{"VA SMIC MUX2", "SWR_DMIC3", "VA SWR_MIC3"},
	{"VA SMIC MUX2", "SWR_DMIC4", "VA SWR_MIC4"},
	{"VA SMIC MUX2", "SWR_DMIC5", "VA SWR_MIC5"},
	{"VA SMIC MUX2", "SWR_DMIC6", "VA SWR_MIC6"},
	{"VA SMIC MUX2", "SWR_DMIC7", "VA SWR_MIC7"},

	{"VA DEC3 MUX", "MSM_DMIC", "VA DMIC MUX3"},
	{"VA DMIC MUX3", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX3", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX3", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX3", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX3", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX3", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX3", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX3", "DMIC7", "VA DMIC7"},

	{"VA DEC3 MUX", "SWR_MIC", "VA SMIC MUX3"},
	{"VA SMIC MUX3", "ADC0", "VA SWR_ADC0"},
	{"VA SMIC MUX3", "ADC1", "VA SWR_ADC1"},
	{"VA SMIC MUX3", "ADC2", "VA SWR_ADC2"},
	{"VA SMIC MUX3", "ADC3", "VA SWR_ADC3"},
	{"VA SMIC MUX3", "SWR_DMIC0", "VA SWR_MIC0"},
	{"VA SMIC MUX3", "SWR_DMIC1", "VA SWR_MIC1"},
	{"VA SMIC MUX3", "SWR_DMIC2", "VA SWR_MIC2"},
	{"VA SMIC MUX3", "SWR_DMIC3", "VA SWR_MIC3"},
	{"VA SMIC MUX3", "SWR_DMIC4", "VA SWR_MIC4"},
	{"VA SMIC MUX3", "SWR_DMIC5", "VA SWR_MIC5"},
	{"VA SMIC MUX3", "SWR_DMIC6", "VA SWR_MIC6"},
	{"VA SMIC MUX3", "SWR_DMIC7", "VA SWR_MIC7"},

	{"VA DEC4 MUX", "MSM_DMIC", "VA DMIC MUX4"},
	{"VA DMIC MUX4", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX4", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX4", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX4", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX4", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX4", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX4", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX4", "DMIC7", "VA DMIC7"},

	{"VA DEC4 MUX", "SWR_MIC", "VA SMIC MUX4"},
	{"VA SMIC MUX4", "ADC0", "VA SWR_ADC0"},
	{"VA SMIC MUX4", "ADC1", "VA SWR_ADC1"},
	{"VA SMIC MUX4", "ADC2", "VA SWR_ADC2"},
	{"VA SMIC MUX4", "ADC3", "VA SWR_ADC3"},
	{"VA SMIC MUX4", "SWR_DMIC0", "VA SWR_MIC0"},
	{"VA SMIC MUX4", "SWR_DMIC1", "VA SWR_MIC1"},
	{"VA SMIC MUX4", "SWR_DMIC2", "VA SWR_MIC2"},
	{"VA SMIC MUX4", "SWR_DMIC3", "VA SWR_MIC3"},
	{"VA SMIC MUX4", "SWR_DMIC4", "VA SWR_MIC4"},
	{"VA SMIC MUX4", "SWR_DMIC5", "VA SWR_MIC5"},
	{"VA SMIC MUX4", "SWR_DMIC6", "VA SWR_MIC6"},
	{"VA SMIC MUX4", "SWR_DMIC7", "VA SWR_MIC7"},

	{"VA DEC5 MUX", "MSM_DMIC", "VA DMIC MUX5"},
	{"VA DMIC MUX5", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX5", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX5", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX5", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX5", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX5", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX5", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX5", "DMIC7", "VA DMIC7"},

	{"VA DEC5 MUX", "SWR_MIC", "VA SMIC MUX5"},
	{"VA SMIC MUX5", "ADC0", "VA SWR_ADC0"},
	{"VA SMIC MUX5", "ADC1", "VA SWR_ADC1"},
	{"VA SMIC MUX5", "ADC2", "VA SWR_ADC2"},
	{"VA SMIC MUX5", "ADC3", "VA SWR_ADC3"},
	{"VA SMIC MUX5", "SWR_DMIC0", "VA SWR_MIC0"},
	{"VA SMIC MUX5", "SWR_DMIC1", "VA SWR_MIC1"},
	{"VA SMIC MUX5", "SWR_DMIC2", "VA SWR_MIC2"},
	{"VA SMIC MUX5", "SWR_DMIC3", "VA SWR_MIC3"},
	{"VA SMIC MUX5", "SWR_DMIC4", "VA SWR_MIC4"},
	{"VA SMIC MUX5", "SWR_DMIC5", "VA SWR_MIC5"},
	{"VA SMIC MUX5", "SWR_DMIC6", "VA SWR_MIC6"},
	{"VA SMIC MUX5", "SWR_DMIC7", "VA SWR_MIC7"},

	{"VA DEC6 MUX", "MSM_DMIC", "VA DMIC MUX6"},
	{"VA DMIC MUX6", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX6", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX6", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX6", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX6", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX6", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX6", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX6", "DMIC7", "VA DMIC7"},

	{"VA DEC6 MUX", "SWR_MIC", "VA SMIC MUX6"},
	{"VA SMIC MUX6", "ADC0", "VA SWR_ADC0"},
	{"VA SMIC MUX6", "ADC1", "VA SWR_ADC1"},
	{"VA SMIC MUX6", "ADC2", "VA SWR_ADC2"},
	{"VA SMIC MUX6", "ADC3", "VA SWR_ADC3"},
	{"VA SMIC MUX6", "SWR_DMIC0", "VA SWR_MIC0"},
	{"VA SMIC MUX6", "SWR_DMIC1", "VA SWR_MIC1"},
	{"VA SMIC MUX6", "SWR_DMIC2", "VA SWR_MIC2"},
	{"VA SMIC MUX6", "SWR_DMIC3", "VA SWR_MIC3"},
	{"VA SMIC MUX6", "SWR_DMIC4", "VA SWR_MIC4"},
	{"VA SMIC MUX6", "SWR_DMIC5", "VA SWR_MIC5"},
	{"VA SMIC MUX6", "SWR_DMIC6", "VA SWR_MIC6"},
	{"VA SMIC MUX6", "SWR_DMIC7", "VA SWR_MIC7"},

	{"VA DEC7 MUX", "MSM_DMIC", "VA DMIC MUX7"},
	{"VA DMIC MUX7", "DMIC0", "VA DMIC0"},
	{"VA DMIC MUX7", "DMIC1", "VA DMIC1"},
	{"VA DMIC MUX7", "DMIC2", "VA DMIC2"},
	{"VA DMIC MUX7", "DMIC3", "VA DMIC3"},
	{"VA DMIC MUX7", "DMIC4", "VA DMIC4"},
	{"VA DMIC MUX7", "DMIC5", "VA DMIC5"},
	{"VA DMIC MUX7", "DMIC6", "VA DMIC6"},
	{"VA DMIC MUX7", "DMIC7", "VA DMIC7"},

	{"VA DEC7 MUX", "SWR_MIC", "VA SMIC MUX7"},
	{"VA SMIC MUX7", "ADC0", "VA SWR_ADC0"},
	{"VA SMIC MUX7", "ADC1", "VA SWR_ADC1"},
	{"VA SMIC MUX7", "ADC2", "VA SWR_ADC2"},
	{"VA SMIC MUX7", "ADC3", "VA SWR_ADC3"},
	{"VA SMIC MUX7", "SWR_DMIC0", "VA SWR_MIC0"},
	{"VA SMIC MUX7", "SWR_DMIC1", "VA SWR_MIC1"},
	{"VA SMIC MUX7", "SWR_DMIC2", "VA SWR_MIC2"},
	{"VA SMIC MUX7", "SWR_DMIC3", "VA SWR_MIC3"},
	{"VA SMIC MUX7", "SWR_DMIC4", "VA SWR_MIC4"},
	{"VA SMIC MUX7", "SWR_DMIC5", "VA SWR_MIC5"},
	{"VA SMIC MUX7", "SWR_DMIC6", "VA SWR_MIC6"},
	{"VA SMIC MUX7", "SWR_DMIC7", "VA SWR_MIC7"},

	{"VA SWR_ADC0", NULL, "VA_SWR_PWR"},
	{"VA SWR_ADC1", NULL, "VA_SWR_PWR"},
	{"VA SWR_ADC2", NULL, "VA_SWR_PWR"},
	{"VA SWR_ADC3", NULL, "VA_SWR_PWR"},
	{"VA SWR_MIC0", NULL, "VA_SWR_PWR"},
	{"VA SWR_MIC1", NULL, "VA_SWR_PWR"},
	{"VA SWR_MIC2", NULL, "VA_SWR_PWR"},
	{"VA SWR_MIC3", NULL, "VA_SWR_PWR"},
	{"VA SWR_MIC4", NULL, "VA_SWR_PWR"},
	{"VA SWR_MIC5", NULL, "VA_SWR_PWR"},
	{"VA SWR_MIC6", NULL, "VA_SWR_PWR"},
	{"VA SWR_MIC7", NULL, "VA_SWR_PWR"},
};

static const char * const dec_mode_mux_text[] = {
	"ADC_DEFAULT", "ADC_LOW_PWR", "ADC_HIGH_PERF",
};

static const struct soc_enum dec_mode_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dec_mode_mux_text),
			    dec_mode_mux_text);

static const struct snd_kcontrol_new va_macro_snd_controls[] = {
	SOC_SINGLE_S8_TLV("VA_DEC0 Volume",
			  BOLERO_CDC_VA_TX0_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("VA_DEC1 Volume",
			  BOLERO_CDC_VA_TX1_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("VA_DEC2 Volume",
			  BOLERO_CDC_VA_TX2_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("VA_DEC3 Volume",
			  BOLERO_CDC_VA_TX3_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("VA_DEC4 Volume",
			  BOLERO_CDC_VA_TX4_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("VA_DEC5 Volume",
			  BOLERO_CDC_VA_TX5_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("VA_DEC6 Volume",
			  BOLERO_CDC_VA_TX6_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("VA_DEC7 Volume",
			  BOLERO_CDC_VA_TX7_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_EXT("LPI Enable", 0, 0, 1, 0,
		va_macro_lpi_get, va_macro_lpi_put),

	SOC_ENUM_EXT("VA_DEC0 MODE", dec_mode_mux_enum,
			va_macro_dec_mode_get, va_macro_dec_mode_put),

	SOC_ENUM_EXT("VA_DEC1 MODE", dec_mode_mux_enum,
			va_macro_dec_mode_get, va_macro_dec_mode_put),

	SOC_ENUM_EXT("VA_DEC2 MODE", dec_mode_mux_enum,
			va_macro_dec_mode_get, va_macro_dec_mode_put),

	SOC_ENUM_EXT("VA_DEC3 MODE", dec_mode_mux_enum,
			va_macro_dec_mode_get, va_macro_dec_mode_put),
};

static const struct snd_kcontrol_new va_macro_snd_controls_common[] = {
	SOC_SINGLE_S8_TLV("VA_DEC0 Volume",
			  BOLERO_CDC_VA_TX0_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("VA_DEC1 Volume",
			  BOLERO_CDC_VA_TX1_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_EXT("LPI Enable", 0, 0, 1, 0,
		va_macro_lpi_get, va_macro_lpi_put),
};

static const struct snd_kcontrol_new va_macro_snd_controls_v3[] = {
	SOC_SINGLE_S8_TLV("VA_DEC2 Volume",
			  BOLERO_CDC_VA_TX2_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("VA_DEC3 Volume",
			  BOLERO_CDC_VA_TX3_TX_VOL_CTL,
			  -84, 40, digital_gain),
};

static int va_macro_validate_dmic_sample_rate(u32 dmic_sample_rate,
				      struct va_macro_priv *va_priv)
{
	u32 div_factor;
	u32 mclk_rate = VA_MACRO_MCLK_FREQ;

	if (dmic_sample_rate == VA_MACRO_DMIC_SAMPLE_RATE_UNDEFINED ||
	    mclk_rate % dmic_sample_rate != 0)
		goto undefined_rate;

	div_factor = mclk_rate / dmic_sample_rate;

	switch (div_factor) {
	case 2:
		va_priv->dmic_clk_div = VA_MACRO_CLK_DIV_2;
		break;
	case 3:
		va_priv->dmic_clk_div = VA_MACRO_CLK_DIV_3;
		break;
	case 4:
		va_priv->dmic_clk_div = VA_MACRO_CLK_DIV_4;
		break;
	case 6:
		va_priv->dmic_clk_div = VA_MACRO_CLK_DIV_6;
		break;
	case 8:
		va_priv->dmic_clk_div = VA_MACRO_CLK_DIV_8;
		break;
	case 16:
		va_priv->dmic_clk_div = VA_MACRO_CLK_DIV_16;
		break;
	default:
		/* Any other DIV factor is invalid */
		goto undefined_rate;
	}

	/* Valid dmic DIV factors */
	dev_dbg(va_priv->dev, "%s: DMIC_DIV = %u, mclk_rate = %u\n",
		__func__, div_factor, mclk_rate);

	return dmic_sample_rate;

undefined_rate:
	dev_dbg(va_priv->dev, "%s: Invalid rate %d, for mclk %d\n",
		 __func__, dmic_sample_rate, mclk_rate);
	dmic_sample_rate = VA_MACRO_DMIC_SAMPLE_RATE_UNDEFINED;

	return dmic_sample_rate;
}

static int va_macro_init(struct snd_soc_component *component)
{
	struct snd_soc_dapm_context *dapm =
				snd_soc_component_get_dapm(component);
	int ret, i;
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	va_dev = bolero_get_device_ptr(component->dev, VA_MACRO);
	if (!va_dev) {
		dev_err(component->dev,
			"%s: null device for macro!\n", __func__);
		return -EINVAL;
	}
	va_priv = dev_get_drvdata(va_dev);
	if (!va_priv) {
		dev_err(component->dev,
			"%s: priv is null for macro!\n", __func__);
		return -EINVAL;
	}

	va_priv->lpi_enable = false;
	va_priv->register_event_listener = false;

	if (va_priv->va_without_decimation) {
		ret = snd_soc_dapm_new_controls(dapm, va_macro_wod_dapm_widgets,
					ARRAY_SIZE(va_macro_wod_dapm_widgets));
		if (ret < 0) {
			dev_err(va_dev,
				"%s: Failed to add without dec controls\n",
				__func__);
			return ret;
		}
		va_priv->component = component;
		return 0;
	}

	va_priv->version = bolero_get_version(va_dev);
	if (va_priv->version >= BOLERO_VERSION_2_0) {
		ret = snd_soc_dapm_new_controls(dapm,
				va_macro_dapm_widgets_common,
				ARRAY_SIZE(va_macro_dapm_widgets_common));
		if (ret < 0) {
			dev_err(va_dev, "%s: Failed to add controls\n",
				__func__);
			return ret;
		}
		if (va_priv->version == BOLERO_VERSION_2_1)
			ret = snd_soc_dapm_new_controls(dapm,
				va_macro_dapm_widgets_v2,
				ARRAY_SIZE(va_macro_dapm_widgets_v2));
		else if (va_priv->version == BOLERO_VERSION_2_0)
			ret = snd_soc_dapm_new_controls(dapm,
				va_macro_dapm_widgets_v3,
				ARRAY_SIZE(va_macro_dapm_widgets_v3));
		if (ret < 0) {
			dev_err(va_dev, "%s: Failed to add controls\n",
				__func__);
			return ret;
		}
	} else {
		ret = snd_soc_dapm_new_controls(dapm, va_macro_dapm_widgets,
					ARRAY_SIZE(va_macro_dapm_widgets));
		if (ret < 0) {
			dev_err(va_dev, "%s: Failed to add controls\n",
				__func__);
			return ret;
		}
	}

	if (va_priv->version >= BOLERO_VERSION_2_0) {
		ret = snd_soc_dapm_add_routes(dapm,
					va_audio_map_common,
					ARRAY_SIZE(va_audio_map_common));
		if (ret < 0) {
			dev_err(va_dev, "%s: Failed to add routes\n",
				__func__);
			return ret;
		}
		if (va_priv->version == BOLERO_VERSION_2_0) {
			ret = snd_soc_dapm_add_routes(dapm,
					va_audio_map_v3,
					ARRAY_SIZE(va_audio_map_v3));
			if (ret < 0) {
				dev_err(va_dev, "%s: Failed to add routes\n",
					__func__);
				return ret;
			}
		}
		if (va_priv->version == BOLERO_VERSION_2_1) {
			ret = snd_soc_dapm_add_routes(dapm,
					va_audio_map_v2,
					ARRAY_SIZE(va_audio_map_v2));
			if (ret < 0) {
				dev_err(va_dev, "%s: Failed to add routes\n",
					__func__);
				return ret;
			}
		}
	} else {
		ret = snd_soc_dapm_add_routes(dapm, va_audio_map,
					ARRAY_SIZE(va_audio_map));
		if (ret < 0) {
			dev_err(va_dev, "%s: Failed to add routes\n",
				__func__);
			return ret;
		}
	}

	ret = snd_soc_dapm_new_widgets(dapm->card);
	if (ret < 0) {
		dev_err(va_dev, "%s: Failed to add widgets\n", __func__);
		return ret;
	}
	if (va_priv->version >= BOLERO_VERSION_2_0) {
		ret = snd_soc_add_component_controls(component,
				va_macro_snd_controls_common,
				ARRAY_SIZE(va_macro_snd_controls_common));
		if (ret < 0) {
			dev_err(va_dev, "%s: Failed to add snd_ctls\n",
				__func__);
			return ret;
		}
		if (va_priv->version == BOLERO_VERSION_2_0)
			ret = snd_soc_add_component_controls(component,
					va_macro_snd_controls_v3,
					ARRAY_SIZE(va_macro_snd_controls_v3));
		if (ret < 0) {
			dev_err(va_dev, "%s: Failed to add snd_ctls\n",
				__func__);
			return ret;
		}
	} else {
		ret = snd_soc_add_component_controls(component,
				va_macro_snd_controls,
				ARRAY_SIZE(va_macro_snd_controls));
		if (ret < 0) {
			dev_err(va_dev, "%s: Failed to add snd_ctls\n",
				__func__);
			return ret;
		}
	}

	snd_soc_dapm_ignore_suspend(dapm, "VA_AIF1 Capture");
	snd_soc_dapm_ignore_suspend(dapm, "VA_AIF2 Capture");
	snd_soc_dapm_ignore_suspend(dapm, "VA_AIF3 Capture");
	if (va_priv->version >= BOLERO_VERSION_2_0) {
		snd_soc_dapm_ignore_suspend(dapm, "VA SWR_INPUT");
	} else {
		snd_soc_dapm_ignore_suspend(dapm, "VA SWR_ADC0");
		snd_soc_dapm_ignore_suspend(dapm, "VA SWR_ADC1");
		snd_soc_dapm_ignore_suspend(dapm, "VA SWR_ADC2");
		snd_soc_dapm_ignore_suspend(dapm, "VA SWR_ADC3");
		snd_soc_dapm_ignore_suspend(dapm, "VA SWR_MIC0");
		snd_soc_dapm_ignore_suspend(dapm, "VA SWR_MIC1");
		snd_soc_dapm_ignore_suspend(dapm, "VA SWR_MIC2");
		snd_soc_dapm_ignore_suspend(dapm, "VA SWR_MIC3");
		snd_soc_dapm_ignore_suspend(dapm, "VA SWR_MIC4");
		snd_soc_dapm_ignore_suspend(dapm, "VA SWR_MIC5");
		snd_soc_dapm_ignore_suspend(dapm, "VA SWR_MIC6");
		snd_soc_dapm_ignore_suspend(dapm, "VA SWR_MIC7");
	}
	snd_soc_dapm_sync(dapm);

	for (i = 0; i < VA_MACRO_NUM_DECIMATORS; i++) {
		va_priv->va_hpf_work[i].va_priv = va_priv;
		va_priv->va_hpf_work[i].decimator = i;
		INIT_DELAYED_WORK(&va_priv->va_hpf_work[i].dwork,
			va_macro_tx_hpf_corner_freq_callback);
	}

	for (i = 0; i < VA_MACRO_NUM_DECIMATORS; i++) {
		va_priv->va_mute_dwork[i].va_priv = va_priv;
		va_priv->va_mute_dwork[i].decimator = i;
		INIT_DELAYED_WORK(&va_priv->va_mute_dwork[i].dwork,
			  va_macro_mute_update_callback);
	}
	va_priv->component = component;

	if (va_priv->version == BOLERO_VERSION_2_1) {
		snd_soc_component_update_bits(component,
			BOLERO_CDC_VA_TOP_CSR_SWR_MIC_CTL0, 0xEE, 0xCC);
		snd_soc_component_update_bits(component,
			BOLERO_CDC_VA_TOP_CSR_SWR_MIC_CTL1, 0xEE, 0xCC);
		snd_soc_component_update_bits(component,
			BOLERO_CDC_VA_TOP_CSR_SWR_MIC_CTL2, 0xEE, 0xCC);
	}

	return 0;
}

static int va_macro_deinit(struct snd_soc_component *component)
{
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	va_priv->component = NULL;
	return 0;
}

static void va_macro_add_child_devices(struct work_struct *work)
{
	struct va_macro_priv *va_priv = NULL;
	struct platform_device *pdev = NULL;
	struct device_node *node = NULL;
	struct va_macro_swr_ctrl_data *swr_ctrl_data = NULL, *temp = NULL;
	int ret = 0;
	u16 count = 0, ctrl_num = 0;
	struct va_macro_swr_ctrl_platform_data *platdata = NULL;
	char plat_dev_name[VA_MACRO_SWR_STRING_LEN] = "";
	bool va_swr_master_node = false;

	va_priv = container_of(work, struct va_macro_priv,
			     va_macro_add_child_devices_work);
	if (!va_priv) {
		pr_err("%s: Memory for va_priv does not exist\n",
			__func__);
		return;
	}

	if (!va_priv->dev) {
		pr_err("%s: VA dev does not exist\n", __func__);
		return;
	}

	if (!va_priv->dev->of_node) {
		dev_err(va_priv->dev,
			"%s: DT node for va_priv does not exist\n", __func__);
		return;
	}

	platdata = &va_priv->swr_plat_data;
	va_priv->child_count = 0;

	for_each_available_child_of_node(va_priv->dev->of_node, node) {
		va_swr_master_node = false;
		if (strnstr(node->name, "va_swr_master",
                                strlen("va_swr_master")) != NULL)
			va_swr_master_node = true;

		if (va_swr_master_node)
			strlcpy(plat_dev_name, "va_swr_ctrl",
				(VA_MACRO_SWR_STRING_LEN - 1));
		else
			strlcpy(plat_dev_name, node->name,
				(VA_MACRO_SWR_STRING_LEN - 1));

		pdev = platform_device_alloc(plat_dev_name, -1);
		if (!pdev) {
			dev_err(va_priv->dev, "%s: pdev memory alloc failed\n",
				__func__);
			ret = -ENOMEM;
			goto err;
		}
		pdev->dev.parent = va_priv->dev;
		pdev->dev.of_node = node;

		if (va_swr_master_node) {
			ret = platform_device_add_data(pdev, platdata,
						       sizeof(*platdata));
			if (ret) {
				dev_err(&pdev->dev,
					"%s: cannot add plat data ctrl:%d\n",
					__func__, ctrl_num);
				goto fail_pdev_add;
			}
		}

		ret = platform_device_add(pdev);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: Cannot add platform device\n",
				__func__);
			goto fail_pdev_add;
		}

		if (va_swr_master_node) {
			temp = krealloc(swr_ctrl_data,
					(ctrl_num + 1) * sizeof(
					struct va_macro_swr_ctrl_data),
					GFP_KERNEL);
			if (!temp) {
				ret = -ENOMEM;
				goto fail_pdev_add;
			}
			swr_ctrl_data = temp;
			swr_ctrl_data[ctrl_num].va_swr_pdev = pdev;
			ctrl_num++;
			dev_dbg(&pdev->dev,
				"%s: Added soundwire ctrl device(s)\n",
				__func__);
			va_priv->swr_ctrl_data = swr_ctrl_data;
		}
		if (va_priv->child_count < VA_MACRO_CHILD_DEVICES_MAX)
			va_priv->pdev_child_devices[
					va_priv->child_count++] = pdev;
		else
			goto err;
	}
	return;
fail_pdev_add:
	for (count = 0; count < va_priv->child_count; count++)
		platform_device_put(va_priv->pdev_child_devices[count]);
err:
	return;
}

static int va_macro_set_port_map(struct snd_soc_component *component,
				u32 usecase, u32 size, void *data)
{
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;
	struct swrm_port_config port_cfg;
	int ret = 0;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	memset(&port_cfg, 0, sizeof(port_cfg));
	port_cfg.uc = usecase;
	port_cfg.size = size;
	port_cfg.params = data;

	if (va_priv->swr_ctrl_data)
		ret = swrm_wcd_notify(
			va_priv->swr_ctrl_data[0].va_swr_pdev,
			SWR_SET_PORT_MAP, &port_cfg);

	return ret;
}

static int va_macro_reg_wake_irq(struct snd_soc_component *component,
				 u32 data)
{
	struct device *va_dev = NULL;
	struct va_macro_priv *va_priv = NULL;
	u32 ipc_wakeup = data;
	int ret = 0;

	if (!va_macro_get_data(component, &va_dev, &va_priv, __func__))
		return -EINVAL;

	if (va_priv->swr_ctrl_data)
		ret = swrm_wcd_notify(
			va_priv->swr_ctrl_data[0].va_swr_pdev,
			SWR_REGISTER_WAKE_IRQ, &ipc_wakeup);

	return ret;
}

static void va_macro_init_ops(struct macro_ops *ops,
			      char __iomem *va_io_base,
			      bool va_without_decimation)
{
	memset(ops, 0, sizeof(struct macro_ops));
	if (!va_without_decimation) {
		ops->dai_ptr = va_macro_dai;
		ops->num_dais = ARRAY_SIZE(va_macro_dai);
	} else {
		ops->dai_ptr = NULL;
		ops->num_dais = 0;
	}
	ops->init = va_macro_init;
	ops->exit = va_macro_deinit;
	ops->io_base = va_io_base;
	ops->event_handler = va_macro_event_handler;
	ops->set_port_map = va_macro_set_port_map;
	ops->reg_wake_irq = va_macro_reg_wake_irq;
	ops->clk_div_get = va_macro_clk_div_get;
}

static int va_macro_probe(struct platform_device *pdev)
{
	struct macro_ops ops;
	struct va_macro_priv *va_priv;
	u32 va_base_addr, sample_rate = 0;
	char __iomem *va_io_base;
	bool va_without_decimation = false;
	const char *micb_supply_str = "va-vdd-micb-supply";
	const char *micb_supply_str1 = "va-vdd-micb";
	const char *micb_voltage_str = "qcom,va-vdd-micb-voltage";
	const char *micb_current_str = "qcom,va-vdd-micb-current";
	int ret = 0;
	const char *dmic_sample_rate = "qcom,va-dmic-sample-rate";
	u32 default_clk_id = 0;
	struct clk *lpass_audio_hw_vote = NULL;
	u32 is_used_va_swr_gpio = 0;
	const char *is_used_va_swr_gpio_dt = "qcom,is-used-swr-gpio";

	va_priv = devm_kzalloc(&pdev->dev, sizeof(struct va_macro_priv),
			    GFP_KERNEL);
	if (!va_priv)
		return -ENOMEM;

	va_priv->dev = &pdev->dev;
	ret = of_property_read_u32(pdev->dev.of_node, "reg",
				   &va_base_addr);
	if (ret) {
		dev_err(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "reg");
		return ret;
	}
	va_without_decimation = of_property_read_bool(pdev->dev.parent->of_node,
					"qcom,va-without-decimation");

	va_priv->va_without_decimation = va_without_decimation;
	ret = of_property_read_u32(pdev->dev.of_node, dmic_sample_rate,
				   &sample_rate);
	if (ret) {
		dev_err(&pdev->dev, "%s: could not find %d entry in dt\n",
			__func__, sample_rate);
		va_priv->dmic_clk_div = VA_MACRO_CLK_DIV_2;
	} else {
		if (va_macro_validate_dmic_sample_rate(
		sample_rate, va_priv) == VA_MACRO_DMIC_SAMPLE_RATE_UNDEFINED)
			return -EINVAL;
	}

	if (of_find_property(pdev->dev.of_node, is_used_va_swr_gpio_dt,
			     NULL)) {
		ret = of_property_read_u32(pdev->dev.of_node,
					   is_used_va_swr_gpio_dt,
					   &is_used_va_swr_gpio);
		if (ret) {
			dev_err(&pdev->dev, "%s: error reading %s in dt\n",
				__func__, is_used_va_swr_gpio_dt);
			is_used_va_swr_gpio = 0;
		}
	}

	va_priv->va_swr_gpio_p = of_parse_phandle(pdev->dev.of_node,
					"qcom,va-swr-gpios", 0);
	if (!va_priv->va_swr_gpio_p && is_used_va_swr_gpio) {
		dev_err(&pdev->dev, "%s: swr_gpios handle not provided!\n",
			__func__);
		return -EINVAL;
	}
	if ((msm_cdc_pinctrl_get_state(va_priv->va_swr_gpio_p) < 0) &&
		is_used_va_swr_gpio) {
		dev_err(&pdev->dev, "%s: failed to get swr pin state\n",
			__func__);
		return -EPROBE_DEFER;
	}

	va_io_base = devm_ioremap(&pdev->dev, va_base_addr,
				  VA_MACRO_MAX_OFFSET);
	if (!va_io_base) {
		dev_err(&pdev->dev, "%s: ioremap failed\n", __func__);
		return -EINVAL;
	}
	va_priv->va_io_base = va_io_base;

	lpass_audio_hw_vote = devm_clk_get(&pdev->dev, "lpass_audio_hw_vote");
	if (IS_ERR(lpass_audio_hw_vote)) {
		ret = PTR_ERR(lpass_audio_hw_vote);
		dev_dbg(&pdev->dev, "%s: clk get %s failed %d\n",
			__func__, "lpass_audio_hw_vote", ret);
		lpass_audio_hw_vote = NULL;
		ret = 0;
	}
	va_priv->lpass_audio_hw_vote = lpass_audio_hw_vote;

	if (of_parse_phandle(pdev->dev.of_node, micb_supply_str, 0)) {
		va_priv->micb_supply = devm_regulator_get(&pdev->dev,
						micb_supply_str1);
		if (IS_ERR(va_priv->micb_supply)) {
			ret = PTR_ERR(va_priv->micb_supply);
			dev_err(&pdev->dev,
				"%s:Failed to get micbias supply for VA Mic %d\n",
				__func__, ret);
			return ret;
		}
		ret = of_property_read_u32(pdev->dev.of_node,
					micb_voltage_str,
					&va_priv->micb_voltage);
		if (ret) {
			dev_err(&pdev->dev,
				"%s:Looking up %s property in node %s failed\n",
				__func__, micb_voltage_str,
				pdev->dev.of_node->full_name);
			return ret;
		}
		ret = of_property_read_u32(pdev->dev.of_node,
					micb_current_str,
					&va_priv->micb_current);
		if (ret) {
			dev_err(&pdev->dev,
				"%s:Looking up %s property in node %s failed\n",
				__func__, micb_current_str,
				pdev->dev.of_node->full_name);
			return ret;
		}
	}
	ret = of_property_read_u32(pdev->dev.of_node, "qcom,default-clk-id",
				   &default_clk_id);
	if (ret) {
		dev_err(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "qcom,default-clk-id");
		default_clk_id = VA_CORE_CLK;
	}
	va_priv->clk_id = VA_CORE_CLK;
	va_priv->default_clk_id = default_clk_id;
	va_priv->current_clk_id = TX_CORE_CLK;

	if (is_used_va_swr_gpio) {
		va_priv->reset_swr = true;
		INIT_WORK(&va_priv->va_macro_add_child_devices_work,
			  va_macro_add_child_devices);
		va_priv->swr_plat_data.handle = (void *) va_priv;
		va_priv->swr_plat_data.read = NULL;
		va_priv->swr_plat_data.write = NULL;
		va_priv->swr_plat_data.bulk_write = NULL;
		va_priv->swr_plat_data.clk = va_macro_swrm_clock;
		va_priv->swr_plat_data.core_vote = va_macro_core_vote;
		va_priv->swr_plat_data.handle_irq = NULL;
		mutex_init(&va_priv->swr_clk_lock);
	}
	va_priv->is_used_va_swr_gpio = is_used_va_swr_gpio;

	mutex_init(&va_priv->mclk_lock);
	dev_set_drvdata(&pdev->dev, va_priv);
	va_macro_init_ops(&ops, va_io_base, va_without_decimation);
	ops.clk_id_req = va_priv->default_clk_id;
	ops.default_clk_id = va_priv->default_clk_id;
	ret = bolero_register_macro(&pdev->dev, VA_MACRO, &ops);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: register macro failed\n", __func__);
		goto reg_macro_fail;
	}
	if (is_used_va_swr_gpio)
		schedule_work(&va_priv->va_macro_add_child_devices_work);
	pm_runtime_set_autosuspend_delay(&pdev->dev, VA_AUTO_SUSPEND_DELAY);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, true);
	pm_runtime_enable(&pdev->dev);
	return ret;

reg_macro_fail:
	mutex_destroy(&va_priv->mclk_lock);
	if (is_used_va_swr_gpio)
		mutex_destroy(&va_priv->swr_clk_lock);
	return ret;
}

static int va_macro_remove(struct platform_device *pdev)
{
	struct va_macro_priv *va_priv;
	int count = 0;

	va_priv = dev_get_drvdata(&pdev->dev);

	if (!va_priv)
		return -EINVAL;
	if (va_priv->is_used_va_swr_gpio) {
		if (va_priv->swr_ctrl_data)
			kfree(va_priv->swr_ctrl_data);
		for (count = 0; count < va_priv->child_count &&
			count < VA_MACRO_CHILD_DEVICES_MAX; count++)
			platform_device_unregister(
				va_priv->pdev_child_devices[count]);
	}

	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	bolero_unregister_macro(&pdev->dev, VA_MACRO);
	mutex_destroy(&va_priv->mclk_lock);
	if (va_priv->is_used_va_swr_gpio)
		mutex_destroy(&va_priv->swr_clk_lock);
	return 0;
}


static const struct of_device_id va_macro_dt_match[] = {
	{.compatible = "qcom,va-macro"},
	{}
};

static const struct dev_pm_ops bolero_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		pm_runtime_force_suspend,
		pm_runtime_force_resume
	)
	SET_RUNTIME_PM_OPS(
		bolero_runtime_suspend,
		bolero_runtime_resume,
		NULL
	)
};

static struct platform_driver va_macro_driver = {
	.driver = {
		.name = "va_macro",
		.owner = THIS_MODULE,
		.pm = &bolero_dev_pm_ops,
		.of_match_table = va_macro_dt_match,
		.suppress_bind_attrs = true,
	},
	.probe = va_macro_probe,
	.remove = va_macro_remove,
};

module_platform_driver(va_macro_driver);

MODULE_DESCRIPTION("VA macro driver");
MODULE_LICENSE("GPL v2");
