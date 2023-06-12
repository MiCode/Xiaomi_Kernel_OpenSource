// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <soc/swr-common.h>
#include <soc/swr-wcd.h>
#include <asoc/msm-cdc-pinctrl.h>
#include "bolero-cdc.h"
#include "bolero-cdc-registers.h"
#include "bolero-clk-rsc.h"

#define AUTO_SUSPEND_DELAY  50 /* delay in msec */
#define TX_MACRO_MAX_OFFSET 0x1000

#define NUM_DECIMATORS 8

#define TX_MACRO_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)
#define TX_MACRO_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE |\
		SNDRV_PCM_FMTBIT_S24_3LE)

#define  TX_HPF_CUT_OFF_FREQ_MASK	0x60
#define  CF_MIN_3DB_4HZ			0x0
#define  CF_MIN_3DB_75HZ		0x1
#define  CF_MIN_3DB_150HZ		0x2

#define TX_MACRO_DMIC_SAMPLE_RATE_UNDEFINED 0
#define TX_MACRO_MCLK_FREQ 9600000
#define TX_MACRO_TX_PATH_OFFSET 0x80
#define TX_MACRO_SWR_MIC_MUX_SEL_MASK 0xF
#define TX_MACRO_ADC_MUX_CFG_OFFSET 0x8
#define TX_MACRO_ADC_MODE_CFG0_SHIFT 1

#define TX_MACRO_DMIC_UNMUTE_DELAY_MS	40
#define TX_MACRO_AMIC_UNMUTE_DELAY_MS	100
#define TX_MACRO_DMIC_HPF_DELAY_MS	300
#define TX_MACRO_AMIC_HPF_DELAY_MS	300

static int tx_amic_unmute_delay = TX_MACRO_AMIC_UNMUTE_DELAY_MS;
module_param(tx_amic_unmute_delay, int, 0664);
MODULE_PARM_DESC(tx_amic_unmute_delay, "delay to unmute the tx amic path");

static int tx_dmic_unmute_delay = TX_MACRO_DMIC_UNMUTE_DELAY_MS;
module_param(tx_dmic_unmute_delay, int, 0664);
MODULE_PARM_DESC(tx_dmic_unmute_delay, "delay to unmute the tx dmic path");

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);

static int tx_macro_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai);
static int tx_macro_get_channel_map(struct snd_soc_dai *dai,
				unsigned int *tx_num, unsigned int *tx_slot,
				unsigned int *rx_num, unsigned int *rx_slot);

#define TX_MACRO_SWR_STRING_LEN 80
#define TX_MACRO_CHILD_DEVICES_MAX 3

/* Hold instance to soundwire platform device */
struct tx_macro_swr_ctrl_data {
	struct platform_device *tx_swr_pdev;
};

struct tx_macro_swr_ctrl_platform_data {
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

enum {
	TX_MACRO_AIF_INVALID = 0,
	TX_MACRO_AIF1_CAP,
	TX_MACRO_AIF2_CAP,
	TX_MACRO_AIF3_CAP,
	TX_MACRO_MAX_DAIS
};

enum {
	TX_MACRO_DEC0,
	TX_MACRO_DEC1,
	TX_MACRO_DEC2,
	TX_MACRO_DEC3,
	TX_MACRO_DEC4,
	TX_MACRO_DEC5,
	TX_MACRO_DEC6,
	TX_MACRO_DEC7,
	TX_MACRO_DEC_MAX,
};

enum {
	TX_MACRO_CLK_DIV_2,
	TX_MACRO_CLK_DIV_3,
	TX_MACRO_CLK_DIV_4,
	TX_MACRO_CLK_DIV_6,
	TX_MACRO_CLK_DIV_8,
	TX_MACRO_CLK_DIV_16,
};

enum {
	MSM_DMIC,
	SWR_MIC,
	ANC_FB_TUNE1
};

enum {
	TX_MCLK,
	VA_MCLK,
};

struct tx_macro_reg_mask_val {
	u16 reg;
	u8 mask;
	u8 val;
};

struct tx_mute_work {
	struct tx_macro_priv *tx_priv;
	u32 decimator;
	struct delayed_work dwork;
};

struct hpf_work {
	struct tx_macro_priv *tx_priv;
	u8 decimator;
	u8 hpf_cut_off_freq;
	struct delayed_work dwork;
};

struct tx_macro_priv {
	struct device *dev;
	bool dec_active[NUM_DECIMATORS];
	int tx_mclk_users;
	int swr_clk_users;
	bool dapm_mclk_enable;
	bool reset_swr;
	struct mutex mclk_lock;
	struct mutex swr_clk_lock;
	struct snd_soc_component *component;
	struct device_node *tx_swr_gpio_p;
	struct tx_macro_swr_ctrl_data *swr_ctrl_data;
	struct tx_macro_swr_ctrl_platform_data swr_plat_data;
	struct work_struct tx_macro_add_child_devices_work;
	struct hpf_work tx_hpf_work[NUM_DECIMATORS];
	struct tx_mute_work tx_mute_dwork[NUM_DECIMATORS];
	u16 dmic_clk_div;
	u32 version;
	u32 is_used_tx_swr_gpio;
	unsigned long active_ch_mask[TX_MACRO_MAX_DAIS];
	unsigned long active_ch_cnt[TX_MACRO_MAX_DAIS];
	char __iomem *tx_io_base;
	struct platform_device *pdev_child_devices
			[TX_MACRO_CHILD_DEVICES_MAX];
	int child_count;
	int tx_swr_clk_cnt;
	int va_swr_clk_cnt;
	int va_clk_status;
	int tx_clk_status;
	bool bcs_enable;
	int dec_mode[NUM_DECIMATORS];
	int bcs_ch;
	bool bcs_clk_en;
	bool hs_slow_insert_complete;
	int pcm_rate[NUM_DECIMATORS];
	bool lpi_enable;
	bool register_event_listener;
	u16 current_clk_id;
	int disable_afe_wakeup_event_listener;
};

static bool tx_macro_get_data(struct snd_soc_component *component,
			      struct device **tx_dev,
			      struct tx_macro_priv **tx_priv,
			      const char *func_name)
{
	*tx_dev = bolero_get_device_ptr(component->dev, TX_MACRO);
	if (!(*tx_dev)) {
		dev_err(component->dev,
			"%s: null device for macro!\n", func_name);
		return false;
	}

	*tx_priv = dev_get_drvdata((*tx_dev));
	if (!(*tx_priv)) {
		dev_err(component->dev,
			"%s: priv is null for macro!\n", func_name);
		return false;
	}

	if (!(*tx_priv)->component) {
		dev_err(component->dev,
			"%s: tx_priv->component not initialized!\n", func_name);
		return false;
	}

	return true;
}

static int tx_macro_mclk_enable(struct tx_macro_priv *tx_priv,
				bool mclk_enable)
{
	struct regmap *regmap = dev_get_regmap(tx_priv->dev->parent, NULL);
	int ret = 0;

	if (regmap == NULL) {
		dev_err(tx_priv->dev, "%s: regmap is NULL\n", __func__);
		return -EINVAL;
	}

	dev_dbg(tx_priv->dev, "%s: mclk_enable = %u,clk_users= %d\n",
		__func__, mclk_enable, tx_priv->tx_mclk_users);

	mutex_lock(&tx_priv->mclk_lock);
	if (mclk_enable) {
		ret = bolero_clk_rsc_request_clock(tx_priv->dev,
						TX_CORE_CLK,
						TX_CORE_CLK,
						true);
		if (ret < 0) {
			dev_err_ratelimited(tx_priv->dev,
				"%s: request clock enable failed\n",
				__func__);
			goto exit;
		}
		bolero_clk_rsc_fs_gen_request(tx_priv->dev,
					true);
		regcache_mark_dirty(regmap);
		regcache_sync_region(regmap,
				TX_START_OFFSET,
				TX_MAX_OFFSET);
		if (tx_priv->tx_mclk_users == 0) {
			/* 9.6MHz MCLK, set value 0x00 if other frequency */
			regmap_update_bits(regmap,
				BOLERO_CDC_TX_TOP_CSR_FREQ_MCLK, 0x01, 0x01);
			regmap_update_bits(regmap,
				BOLERO_CDC_TX_CLK_RST_CTRL_MCLK_CONTROL,
				0x01, 0x01);
			regmap_update_bits(regmap,
				BOLERO_CDC_TX_CLK_RST_CTRL_FS_CNT_CONTROL,
				0x01, 0x01);
		}
		tx_priv->tx_mclk_users++;
	} else {
		if (tx_priv->tx_mclk_users <= 0) {
			dev_err(tx_priv->dev, "%s: clock already disabled\n",
				__func__);
			tx_priv->tx_mclk_users = 0;
			goto exit;
		}
		tx_priv->tx_mclk_users--;
		if (tx_priv->tx_mclk_users == 0) {
			regmap_update_bits(regmap,
				BOLERO_CDC_TX_CLK_RST_CTRL_FS_CNT_CONTROL,
				0x01, 0x00);
			regmap_update_bits(regmap,
				BOLERO_CDC_TX_CLK_RST_CTRL_MCLK_CONTROL,
				0x01, 0x00);
		}

		bolero_clk_rsc_fs_gen_request(tx_priv->dev,
				false);
		bolero_clk_rsc_request_clock(tx_priv->dev,
				 TX_CORE_CLK,
				 TX_CORE_CLK,
				 false);
	}
exit:
	mutex_unlock(&tx_priv->mclk_lock);
	return ret;
}

static int __tx_macro_mclk_enable(struct snd_soc_component *component,
				  bool enable)
{
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	return tx_macro_mclk_enable(tx_priv, enable);
}

static int tx_macro_va_swr_clk_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	if (SND_SOC_DAPM_EVENT_ON(event))
		++tx_priv->va_swr_clk_cnt;
	if (SND_SOC_DAPM_EVENT_OFF(event))
		--tx_priv->va_swr_clk_cnt;

	return 0;
}

static int tx_macro_tx_swr_clk_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	if (SND_SOC_DAPM_EVENT_ON(event))
		++tx_priv->tx_swr_clk_cnt;
	if (SND_SOC_DAPM_EVENT_OFF(event))
		--tx_priv->tx_swr_clk_cnt;

	return 0;
}

static int tx_macro_swr_pwr_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	int ret = 0;
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	dev_dbg(tx_dev, "%s: event = %d, lpi_enable = %d\n",
		__func__, event, tx_priv->lpi_enable);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (tx_priv->lpi_enable) {
			bolero_register_event_listener(component, true);
			tx_priv->register_event_listener = true;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (tx_priv->register_event_listener) {
			tx_priv->register_event_listener = false;
			bolero_register_event_listener(component, false);
		}
		break;
	default:
		dev_err(tx_priv->dev,
			"%s: invalid DAPM event %d\n", __func__, event);
		ret = -EINVAL;
	}
	return ret;
}

static int tx_macro_mclk_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	int ret = 0;
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	dev_dbg(tx_dev, "%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = tx_macro_mclk_enable(tx_priv, 1);
		if (ret)
			tx_priv->dapm_mclk_enable = false;
		else
			tx_priv->dapm_mclk_enable = true;
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (tx_priv->dapm_mclk_enable)
			ret = tx_macro_mclk_enable(tx_priv, 0);
		break;
	default:
		dev_err(tx_priv->dev,
			"%s: invalid DAPM event %d\n", __func__, event);
		ret = -EINVAL;
	}
	return ret;
}

static int tx_macro_event_handler(struct snd_soc_component *component,
				u16 event, u32 data)
{
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;
	int ret = 0;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	switch (event) {
	case BOLERO_MACRO_EVT_SSR_DOWN:
		trace_printk("%s, enter SSR down\n", __func__);
		if (tx_priv->swr_ctrl_data) {
			swrm_wcd_notify(
				tx_priv->swr_ctrl_data[0].tx_swr_pdev,
				SWR_DEVICE_SSR_DOWN, NULL);
		}
		if ((!pm_runtime_enabled(tx_dev) ||
		     !pm_runtime_suspended(tx_dev))) {
			ret = bolero_runtime_suspend(tx_dev);
			if (!ret) {
				pm_runtime_disable(tx_dev);
				pm_runtime_set_suspended(tx_dev);
				pm_runtime_enable(tx_dev);
			}
		}
		break;
	case BOLERO_MACRO_EVT_SSR_UP:
		trace_printk("%s, enter SSR up\n", __func__);
		/* reset swr after ssr/pdr */
		tx_priv->reset_swr = true;
		if (tx_priv->swr_ctrl_data)
			swrm_wcd_notify(
				tx_priv->swr_ctrl_data[0].tx_swr_pdev,
				SWR_DEVICE_SSR_UP, NULL);
		break;
	case BOLERO_MACRO_EVT_CLK_RESET:
		bolero_rsc_clk_reset(tx_dev, TX_CORE_CLK);
		break;
	case BOLERO_MACRO_EVT_BCS_CLK_OFF:
		if (tx_priv->bcs_clk_en)
			snd_soc_component_update_bits(component,
				BOLERO_CDC_TX0_TX_PATH_SEC7, 0x40, data << 6);
		if (data)
			tx_priv->hs_slow_insert_complete = true;
		else
			tx_priv->hs_slow_insert_complete = false;
		break;
	default:
		pr_debug("%s Invalid Event\n", __func__);
		break;
	}
	return 0;
}

static int tx_macro_reg_wake_irq(struct snd_soc_component *component,
				 u32 data)
{
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;
	u32 ipc_wakeup = data;
	int ret = 0;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	if (tx_priv->swr_ctrl_data)
		ret = swrm_wcd_notify(
			tx_priv->swr_ctrl_data[0].tx_swr_pdev,
			SWR_REGISTER_WAKE_IRQ, &ipc_wakeup);

	return ret;
}

static bool is_smic_enabled(struct snd_soc_component *component, int decimator)
{
	u16 adc_mux_reg = 0, adc_reg = 0;
	u16 adc_n = BOLERO_ADC_MAX;
	bool ret = false;
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return ret;

	adc_mux_reg = BOLERO_CDC_TX_INP_MUX_ADC_MUX0_CFG1 +
			TX_MACRO_ADC_MUX_CFG_OFFSET * decimator;
	if (snd_soc_component_read32(component, adc_mux_reg) & SWR_MIC) {
		if (tx_priv->version == BOLERO_VERSION_2_1 ||
			tx_priv->version == BOLERO_VERSION_2_0)
			return true;
		adc_reg = BOLERO_CDC_TX_INP_MUX_ADC_MUX0_CFG0 +
			TX_MACRO_ADC_MUX_CFG_OFFSET * decimator;
		adc_n = snd_soc_component_read32(component, adc_reg) &
				TX_MACRO_SWR_MIC_MUX_SEL_MASK;
		if (adc_n < BOLERO_ADC_MAX)
			return true;
	}

	return ret;
}

static void tx_macro_tx_hpf_corner_freq_callback(struct work_struct *work)
{
	struct delayed_work *hpf_delayed_work = NULL;
	struct hpf_work *hpf_work = NULL;
	struct tx_macro_priv *tx_priv = NULL;
	struct snd_soc_component *component = NULL;
	u16 dec_cfg_reg = 0, hpf_gate_reg = 0;
	u8 hpf_cut_off_freq = 0;
	u16 adc_reg = 0, adc_n = 0;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	tx_priv = hpf_work->tx_priv;
	component = tx_priv->component;
	hpf_cut_off_freq = hpf_work->hpf_cut_off_freq;

	dec_cfg_reg = BOLERO_CDC_TX0_TX_PATH_CFG0 +
			TX_MACRO_TX_PATH_OFFSET * hpf_work->decimator;
	hpf_gate_reg = BOLERO_CDC_TX0_TX_PATH_SEC2 +
			TX_MACRO_TX_PATH_OFFSET * hpf_work->decimator;

	dev_dbg(component->dev, "%s: decimator %u hpf_cut_of_freq 0x%x\n",
		__func__, hpf_work->decimator, hpf_cut_off_freq);

	if (is_smic_enabled(component, hpf_work->decimator)) {
		adc_reg = BOLERO_CDC_TX_INP_MUX_ADC_MUX0_CFG0 +
			TX_MACRO_ADC_MUX_CFG_OFFSET * hpf_work->decimator;
		adc_n = snd_soc_component_read32(component, adc_reg) &
				TX_MACRO_SWR_MIC_MUX_SEL_MASK;
		/* analog mic clear TX hold */
		bolero_clear_amic_tx_hold(component->dev, adc_n);
		snd_soc_component_update_bits(component,
				dec_cfg_reg, TX_HPF_CUT_OFF_FREQ_MASK,
				hpf_cut_off_freq << 5);
		snd_soc_component_update_bits(component, hpf_gate_reg,
						0x03, 0x02);
		/* Add delay between toggle hpf gate based on sample rate */
		switch (tx_priv->pcm_rate[hpf_work->decimator]) {
		case 0:
			usleep_range(125, 130);
			break;
		case 1:
			usleep_range(62, 65);
			break;
		case 3:
			usleep_range(31, 32);
			break;
		case 4:
			usleep_range(20, 21);
			break;
		case 5:
			usleep_range(10, 11);
			break;
		case 6:
			usleep_range(5, 6);
			break;
		default:
			usleep_range(125, 130);
		}
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

static void tx_macro_mute_update_callback(struct work_struct *work)
{
	struct tx_mute_work *tx_mute_dwork = NULL;
	struct snd_soc_component *component = NULL;
	struct tx_macro_priv *tx_priv = NULL;
	struct delayed_work *delayed_work = NULL;
	u16 tx_vol_ctl_reg = 0;
	u8 decimator = 0;

	delayed_work = to_delayed_work(work);
	tx_mute_dwork = container_of(delayed_work, struct tx_mute_work, dwork);
	tx_priv = tx_mute_dwork->tx_priv;
	component = tx_priv->component;
	decimator = tx_mute_dwork->decimator;

	tx_vol_ctl_reg =
		BOLERO_CDC_TX0_TX_PATH_CTL +
			TX_MACRO_TX_PATH_OFFSET * decimator;
	snd_soc_component_update_bits(component, tx_vol_ctl_reg, 0x10, 0x00);
	dev_dbg(tx_priv->dev, "%s: decimator %u unmute\n",
		__func__, decimator);
}

static int tx_macro_put_dec_enum(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(widget->dapm);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val = 0;
	u16 mic_sel_reg = 0;
	u16 dmic_clk_reg = 0;
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	val = ucontrol->value.enumerated.item[0];
	if (val > e->items - 1)
		return -EINVAL;

	dev_dbg(component->dev, "%s: wname: %s, val: 0x%x\n", __func__,
		widget->name, val);

	switch (e->reg) {
	case BOLERO_CDC_TX_INP_MUX_ADC_MUX0_CFG0:
		mic_sel_reg = BOLERO_CDC_TX0_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_TX_INP_MUX_ADC_MUX1_CFG0:
		mic_sel_reg = BOLERO_CDC_TX1_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_TX_INP_MUX_ADC_MUX2_CFG0:
		mic_sel_reg = BOLERO_CDC_TX2_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_TX_INP_MUX_ADC_MUX3_CFG0:
		mic_sel_reg = BOLERO_CDC_TX3_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_TX_INP_MUX_ADC_MUX4_CFG0:
		mic_sel_reg = BOLERO_CDC_TX4_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_TX_INP_MUX_ADC_MUX5_CFG0:
		mic_sel_reg = BOLERO_CDC_TX5_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_TX_INP_MUX_ADC_MUX6_CFG0:
		mic_sel_reg = BOLERO_CDC_TX6_TX_PATH_CFG0;
		break;
	case BOLERO_CDC_TX_INP_MUX_ADC_MUX7_CFG0:
		mic_sel_reg = BOLERO_CDC_TX7_TX_PATH_CFG0;
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
					0x0E, tx_priv->dmic_clk_div << 0x1);
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

static int tx_macro_tx_mixer_get(struct snd_kcontrol *kcontrol,
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
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	if (test_bit(dec_id, &tx_priv->active_ch_mask[dai_id]))
		ucontrol->value.integer.value[0] = 1;
	else
		ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int tx_macro_tx_mixer_put(struct snd_kcontrol *kcontrol,
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
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	if (enable) {
		set_bit(dec_id, &tx_priv->active_ch_mask[dai_id]);
		tx_priv->active_ch_cnt[dai_id]++;
	} else {
		tx_priv->active_ch_cnt[dai_id]--;
		clear_bit(dec_id, &tx_priv->active_ch_mask[dai_id]);
	}
	snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, enable, update);

	return 0;
}

static inline int tx_macro_path_get(const char *wname,
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
		pr_err("%s: tx path index not found\n",
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

static int tx_macro_dec_mode_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct tx_macro_priv *tx_priv = NULL;
	struct device *tx_dev = NULL;
	int ret = 0;
	int path = 0;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	ret = tx_macro_path_get(kcontrol->id.name, &path);
	if (ret)
		return ret;

	ucontrol->value.integer.value[0] = tx_priv->dec_mode[path];

	return 0;
}

static int tx_macro_dec_mode_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct tx_macro_priv *tx_priv = NULL;
	struct device *tx_dev = NULL;
	int value = ucontrol->value.integer.value[0];
	int ret = 0;
	int path = 0;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	ret = tx_macro_path_get(kcontrol->id.name, &path);
	if (ret)
		return ret;

	tx_priv->dec_mode[path] = value;

	return 0;
}

static int tx_macro_lpi_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	ucontrol->value.integer.value[0] = tx_priv->lpi_enable;

	return 0;
}

static int tx_macro_lpi_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	tx_priv->lpi_enable = ucontrol->value.integer.value[0];

	return 0;
}

static int tx_macro_bcs_ch_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct tx_macro_priv *tx_priv = NULL;
	struct device *tx_dev = NULL;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	ucontrol->value.enumerated.item[0] = tx_priv->bcs_ch;

	return 0;
}

static int tx_macro_bcs_ch_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct tx_macro_priv *tx_priv = NULL;
	struct device *tx_dev = NULL;
	int value = ucontrol->value.enumerated.item[0];

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	tx_priv->bcs_ch = value;

	return 0;
}

static int tx_macro_get_bcs(struct snd_kcontrol *kcontrol,
                            struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct tx_macro_priv *tx_priv = NULL;
	struct device *tx_dev = NULL;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
	return -EINVAL;

	ucontrol->value.integer.value[0] = tx_priv->bcs_enable;

	return 0;
}

static int tx_macro_set_bcs(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct tx_macro_priv *tx_priv = NULL;
	struct device *tx_dev = NULL;
	int value = ucontrol->value.integer.value[0];

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	tx_priv->bcs_enable = value;

	return 0;
}

static const char * const bcs_ch_sel_mux_text[] = {
	"SWR_MIC0", "SWR_MIC1", "SWR_MIC2", "SWR_MIC3",
	"SWR_MIC4", "SWR_MIC5", "SWR_MIC6", "SWR_MIC7",
	"SWR_MIC8", "SWR_MIC9", "SWR_MIC10", "SWR_MIC11",
};

static const struct soc_enum bcs_ch_sel_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(bcs_ch_sel_mux_text),
			    bcs_ch_sel_mux_text);

static int tx_macro_get_bcs_ch_sel(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct tx_macro_priv *tx_priv = NULL;
	struct device *tx_dev = NULL;
	int value = 0;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	if (tx_priv->version == BOLERO_VERSION_2_1)
		value = (snd_soc_component_read32(component,
			BOLERO_CDC_VA_TOP_CSR_SWR_CTRL)) & 0x0F;
	else if (tx_priv->version == BOLERO_VERSION_2_0)
		value = (snd_soc_component_read32(component,
			BOLERO_CDC_TX_TOP_CSR_SWR_CTRL)) & 0x0F;

	ucontrol->value.integer.value[0] = value;
	return 0;
}

static int tx_macro_put_bcs_ch_sel(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct tx_macro_priv *tx_priv = NULL;
	struct device *tx_dev = NULL;
	int value;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	if (ucontrol->value.integer.value[0] < 0 ||
	    ucontrol->value.integer.value[0] > ARRAY_SIZE(bcs_ch_sel_mux_text))
		return -EINVAL;

	value = ucontrol->value.integer.value[0];
	if (tx_priv->version == BOLERO_VERSION_2_1)
		snd_soc_component_update_bits(component,
			BOLERO_CDC_VA_TOP_CSR_SWR_CTRL, 0x0F, value);
	else if (tx_priv->version == BOLERO_VERSION_2_0)
		snd_soc_component_update_bits(component,
			BOLERO_CDC_TX_TOP_CSR_SWR_CTRL, 0x0F, value);

	return 0;
}

static int tx_macro_enable_dmic(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event, u16 adc_mux0_cfg)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	unsigned int dmic = 0;

	dmic = (snd_soc_component_read32(component, adc_mux0_cfg) >> 4) - 1;

	dev_dbg(component->dev, "%s: event %d DMIC%d\n",
			__func__, event,  dmic);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		bolero_dmic_clk_enable(component, dmic, DMIC_TX, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		bolero_dmic_clk_enable(component, dmic, DMIC_TX, false);
		break;
	}

	return 0;
}

static int tx_macro_enable_dec(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	unsigned int decimator = 0;
	u16 tx_vol_ctl_reg = 0;
	u16 dec_cfg_reg = 0;
	u16 hpf_gate_reg = 0;
	u16 tx_gain_ctl_reg = 0;
	u16 tx_fs_reg = 0;
	u8 hpf_cut_off_freq = 0;
	u16 adc_mux_reg = 0;
	u16 adc_mux0_reg = 0;
	int hpf_delay = TX_MACRO_DMIC_HPF_DELAY_MS;
	int unmute_delay = TX_MACRO_DMIC_UNMUTE_DELAY_MS;
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	decimator = w->shift;

	dev_dbg(component->dev, "%s(): widget = %s decimator = %u\n", __func__,
			w->name, decimator);

	tx_vol_ctl_reg = BOLERO_CDC_TX0_TX_PATH_CTL +
				TX_MACRO_TX_PATH_OFFSET * decimator;
	hpf_gate_reg = BOLERO_CDC_TX0_TX_PATH_SEC2 +
				TX_MACRO_TX_PATH_OFFSET * decimator;
	dec_cfg_reg = BOLERO_CDC_TX0_TX_PATH_CFG0 +
				TX_MACRO_TX_PATH_OFFSET * decimator;
	tx_gain_ctl_reg = BOLERO_CDC_TX0_TX_VOL_CTL +
				TX_MACRO_TX_PATH_OFFSET * decimator;
	adc_mux_reg = BOLERO_CDC_TX_INP_MUX_ADC_MUX0_CFG1 +
			TX_MACRO_ADC_MUX_CFG_OFFSET * decimator;
	adc_mux0_reg = BOLERO_CDC_TX_INP_MUX_ADC_MUX0_CFG0 +
			TX_MACRO_ADC_MUX_CFG_OFFSET * decimator;
	tx_fs_reg = BOLERO_CDC_TX0_TX_PATH_CTL +
				TX_MACRO_TX_PATH_OFFSET * decimator;

	tx_priv->pcm_rate[decimator] = (snd_soc_component_read32(component,
				     tx_fs_reg) & 0x0F);
	if(!is_smic_enabled(component, decimator))
		tx_macro_enable_dmic(w, kcontrol, event, adc_mux0_reg);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
			dec_cfg_reg, 0x06, tx_priv->dec_mode[decimator] <<
			TX_MACRO_ADC_MODE_CFG0_SHIFT);
		/* Enable TX PGA Mute */
		snd_soc_component_update_bits(component,
			tx_vol_ctl_reg, 0x10, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_component_update_bits(component,
			tx_vol_ctl_reg, 0x20, 0x20);
		if (!is_smic_enabled(component, decimator)) {
			snd_soc_component_update_bits(component,
				hpf_gate_reg, 0x01, 0x00);
			/*
		 	 * Minimum 1 clk cycle delay is required as per HW spec
		 	 */
			usleep_range(1000, 1010);
		}
		hpf_cut_off_freq = (
			snd_soc_component_read32(component, dec_cfg_reg) &
				TX_HPF_CUT_OFF_FREQ_MASK) >> 5;

		tx_priv->tx_hpf_work[decimator].hpf_cut_off_freq =
						hpf_cut_off_freq;

		if (hpf_cut_off_freq != CF_MIN_3DB_150HZ)
			snd_soc_component_update_bits(component, dec_cfg_reg,
						TX_HPF_CUT_OFF_FREQ_MASK,
						CF_MIN_3DB_150HZ << 5);

		if (is_smic_enabled(component, decimator)) {
			hpf_delay = TX_MACRO_AMIC_HPF_DELAY_MS;
			unmute_delay = TX_MACRO_AMIC_UNMUTE_DELAY_MS;
			if (unmute_delay < tx_amic_unmute_delay)
				unmute_delay = tx_amic_unmute_delay;
		} else {
			if (unmute_delay < tx_dmic_unmute_delay)
				unmute_delay = tx_dmic_unmute_delay;
		}
		/* schedule work queue to Remove Mute */
		queue_delayed_work(system_freezable_wq,
				   &tx_priv->tx_mute_dwork[decimator].dwork,
				   msecs_to_jiffies(unmute_delay));
		if (tx_priv->tx_hpf_work[decimator].hpf_cut_off_freq !=
							CF_MIN_3DB_150HZ)
			queue_delayed_work(system_freezable_wq,
				&tx_priv->tx_hpf_work[decimator].dwork,
				msecs_to_jiffies(hpf_delay));
		snd_soc_component_update_bits(component,
				hpf_gate_reg, 0x03, 0x02);
		if (!is_smic_enabled(component, decimator))
			snd_soc_component_update_bits(component,
				hpf_gate_reg, 0x03, 0x00);
		snd_soc_component_update_bits(component,
				hpf_gate_reg, 0x03, 0x01);
		/*
		 * 6ms delay is required as per HW spec
		 */
		usleep_range(6000, 6010);
		/* apply gain after decimator is enabled */
		snd_soc_component_write(component, tx_gain_ctl_reg,
			      snd_soc_component_read32(component,
					tx_gain_ctl_reg));
		if (tx_priv->bcs_enable) {
			if (tx_priv->version == BOLERO_VERSION_2_1)
				snd_soc_component_update_bits(component,
					BOLERO_CDC_VA_TOP_CSR_SWR_CTRL, 0x0F,
					tx_priv->bcs_ch);
			else if (tx_priv->version == BOLERO_VERSION_2_0)
				snd_soc_component_update_bits(component,
					BOLERO_CDC_TX_TOP_CSR_SWR_CTRL, 0xF0,
					(tx_priv->bcs_ch << 4));

			snd_soc_component_update_bits(component, dec_cfg_reg,
					0x01, 0x01);
			tx_priv->bcs_clk_en = true;
			if (tx_priv->hs_slow_insert_complete)
				snd_soc_component_update_bits(component,
					BOLERO_CDC_TX0_TX_PATH_SEC7, 0x40,
					0x40);
		}
		if (tx_priv->version == BOLERO_VERSION_2_0) {
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
			tx_priv->tx_hpf_work[decimator].hpf_cut_off_freq;
		snd_soc_component_update_bits(component,
				tx_vol_ctl_reg, 0x10, 0x10);
		if (cancel_delayed_work_sync(
		    &tx_priv->tx_hpf_work[decimator].dwork)) {
			if (hpf_cut_off_freq != CF_MIN_3DB_150HZ) {
				snd_soc_component_update_bits(
						component, dec_cfg_reg,
						TX_HPF_CUT_OFF_FREQ_MASK,
						hpf_cut_off_freq << 5);
				if (is_smic_enabled(component, decimator))
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
				&tx_priv->tx_mute_dwork[decimator].dwork);

		if (tx_priv->version == BOLERO_VERSION_2_0) {
			if (snd_soc_component_read32(component, adc_mux_reg)
							& SWR_MIC)
				snd_soc_component_update_bits(component,
					BOLERO_CDC_TX_TOP_CSR_SWR_CTRL,
					0x01, 0x00);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component, tx_vol_ctl_reg,
						0x20, 0x00);
		snd_soc_component_update_bits(component,
			dec_cfg_reg, 0x06, 0x00);
		snd_soc_component_update_bits(component, tx_vol_ctl_reg,
						0x10, 0x00);
		if (tx_priv->bcs_enable) {
			snd_soc_component_update_bits(component, dec_cfg_reg,
					0x01, 0x00);
			snd_soc_component_update_bits(component,
				BOLERO_CDC_TX0_TX_PATH_SEC7, 0x40, 0x00);
			tx_priv->bcs_clk_en = false;
			if (tx_priv->version == BOLERO_VERSION_2_1)
				snd_soc_component_update_bits(component,
					BOLERO_CDC_VA_TOP_CSR_SWR_CTRL, 0x0F,
					0x00);
			else if (tx_priv->version == BOLERO_VERSION_2_0)
				snd_soc_component_update_bits(component,
					BOLERO_CDC_TX_TOP_CSR_SWR_CTRL, 0xF0,
					0x00);
		}
		break;
	}
	return 0;
}

static int tx_macro_enable_micbias(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	return 0;
}

/* Cutoff frequency for high pass filter */
static const char * const cf_text[] = {
	"CF_NEG_3DB_4HZ", "CF_NEG_3DB_75HZ", "CF_NEG_3DB_150HZ"
};

static SOC_ENUM_SINGLE_DECL(cf_dec0_enum, BOLERO_CDC_TX0_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec1_enum, BOLERO_CDC_TX1_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec2_enum, BOLERO_CDC_TX2_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec3_enum, BOLERO_CDC_TX3_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec4_enum, BOLERO_CDC_TX4_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec5_enum, BOLERO_CDC_TX5_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec6_enum, BOLERO_CDC_TX6_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec7_enum, BOLERO_CDC_TX7_TX_PATH_CFG0, 5,
							cf_text);

static int tx_macro_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	int tx_fs_rate = -EINVAL;
	struct snd_soc_component *component = dai->component;
	u32 decimator = 0;
	u32 sample_rate = 0;
	u16 tx_fs_reg = 0;
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	pr_debug("%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n", __func__,
		 dai->name, dai->id, params_rate(params),
		 params_channels(params));

	sample_rate = params_rate(params);
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
		dev_err(component->dev, "%s: Invalid TX sample rate: %d\n",
			__func__, params_rate(params));
		return -EINVAL;
	}
	for_each_set_bit(decimator, &tx_priv->active_ch_mask[dai->id],
			 TX_MACRO_DEC_MAX) {
		if (decimator >= 0) {
			tx_fs_reg = BOLERO_CDC_TX0_TX_PATH_CTL +
				    TX_MACRO_TX_PATH_OFFSET * decimator;
			dev_dbg(component->dev, "%s: set DEC%u rate to %u\n",
				__func__, decimator, sample_rate);
			snd_soc_component_update_bits(component, tx_fs_reg,
						0x0F, tx_fs_rate);
		} else {
			dev_err(component->dev,
				"%s: ERROR: Invalid decimator: %d\n",
				__func__, decimator);
			return -EINVAL;
		}
	}
	return 0;
}

static int tx_macro_get_channel_map(struct snd_soc_dai *dai,
				unsigned int *tx_num, unsigned int *tx_slot,
				unsigned int *rx_num, unsigned int *rx_slot)
{
	struct snd_soc_component *component = dai->component;
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	switch (dai->id) {
	case TX_MACRO_AIF1_CAP:
	case TX_MACRO_AIF2_CAP:
	case TX_MACRO_AIF3_CAP:
		*tx_slot = tx_priv->active_ch_mask[dai->id];
		*tx_num = tx_priv->active_ch_cnt[dai->id];
		break;
	default:
		dev_err(tx_dev, "%s: Invalid AIF\n", __func__);
		break;
	}
	return 0;
}

static struct snd_soc_dai_ops tx_macro_dai_ops = {
	.hw_params = tx_macro_hw_params,
	.get_channel_map = tx_macro_get_channel_map,
};

static struct snd_soc_dai_driver tx_macro_dai[] = {
	{
		.name = "tx_macro_tx1",
		.id = TX_MACRO_AIF1_CAP,
		.capture = {
			.stream_name = "TX_AIF1 Capture",
			.rates = TX_MACRO_RATES,
			.formats = TX_MACRO_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 8,
		},
		.ops = &tx_macro_dai_ops,
	},
	{
		.name = "tx_macro_tx2",
		.id = TX_MACRO_AIF2_CAP,
		.capture = {
			.stream_name = "TX_AIF2 Capture",
			.rates = TX_MACRO_RATES,
			.formats = TX_MACRO_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 8,
		},
		.ops = &tx_macro_dai_ops,
	},
	{
		.name = "tx_macro_tx3",
		.id = TX_MACRO_AIF3_CAP,
		.capture = {
			.stream_name = "TX_AIF3 Capture",
			.rates = TX_MACRO_RATES,
			.formats = TX_MACRO_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 8,
		},
		.ops = &tx_macro_dai_ops,
	},
};

#define STRING(name) #name
#define TX_MACRO_DAPM_ENUM(name, reg, offset, text) \
static SOC_ENUM_SINGLE_DECL(name##_enum, reg, offset, text); \
static const struct snd_kcontrol_new name##_mux = \
		SOC_DAPM_ENUM(STRING(name), name##_enum)

#define TX_MACRO_DAPM_ENUM_EXT(name, reg, offset, text, getname, putname) \
static SOC_ENUM_SINGLE_DECL(name##_enum, reg, offset, text); \
static const struct snd_kcontrol_new name##_mux = \
		SOC_DAPM_ENUM_EXT(STRING(name), name##_enum, getname, putname)

#define TX_MACRO_DAPM_MUX(name, shift, kctl) \
		SND_SOC_DAPM_MUX(name, SND_SOC_NOPM, shift, 0, &kctl##_mux)

static const char * const adc_mux_text[] = {
	"MSM_DMIC", "SWR_MIC", "ANC_FB_TUNE1"
};

TX_MACRO_DAPM_ENUM(tx_dec0, BOLERO_CDC_TX_INP_MUX_ADC_MUX0_CFG1,
		   0, adc_mux_text);
TX_MACRO_DAPM_ENUM(tx_dec1, BOLERO_CDC_TX_INP_MUX_ADC_MUX1_CFG1,
		   0, adc_mux_text);
TX_MACRO_DAPM_ENUM(tx_dec2, BOLERO_CDC_TX_INP_MUX_ADC_MUX2_CFG1,
		   0, adc_mux_text);
TX_MACRO_DAPM_ENUM(tx_dec3, BOLERO_CDC_TX_INP_MUX_ADC_MUX3_CFG1,
		   0, adc_mux_text);
TX_MACRO_DAPM_ENUM(tx_dec4, BOLERO_CDC_TX_INP_MUX_ADC_MUX4_CFG1,
		   0, adc_mux_text);
TX_MACRO_DAPM_ENUM(tx_dec5, BOLERO_CDC_TX_INP_MUX_ADC_MUX5_CFG1,
		   0, adc_mux_text);
TX_MACRO_DAPM_ENUM(tx_dec6, BOLERO_CDC_TX_INP_MUX_ADC_MUX6_CFG1,
		   0, adc_mux_text);
TX_MACRO_DAPM_ENUM(tx_dec7, BOLERO_CDC_TX_INP_MUX_ADC_MUX7_CFG1,
		   0, adc_mux_text);


static const char * const dmic_mux_text[] = {
	"ZERO", "DMIC0", "DMIC1", "DMIC2", "DMIC3",
	"DMIC4", "DMIC5", "DMIC6", "DMIC7"
};

TX_MACRO_DAPM_ENUM_EXT(tx_dmic0, BOLERO_CDC_TX_INP_MUX_ADC_MUX0_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_dmic1, BOLERO_CDC_TX_INP_MUX_ADC_MUX1_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_dmic2, BOLERO_CDC_TX_INP_MUX_ADC_MUX2_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_dmic3, BOLERO_CDC_TX_INP_MUX_ADC_MUX3_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_dmic4, BOLERO_CDC_TX_INP_MUX_ADC_MUX4_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_dmic5, BOLERO_CDC_TX_INP_MUX_ADC_MUX5_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_dmic6, BOLERO_CDC_TX_INP_MUX_ADC_MUX6_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_dmic7, BOLERO_CDC_TX_INP_MUX_ADC_MUX7_CFG0,
			4, dmic_mux_text, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

static const char * const smic_mux_text[] = {
	"ZERO", "ADC0", "ADC1", "ADC2", "ADC3", "SWR_DMIC0",
	"SWR_DMIC1", "SWR_DMIC2", "SWR_DMIC3", "SWR_DMIC4",
	"SWR_DMIC5", "SWR_DMIC6", "SWR_DMIC7"
};

TX_MACRO_DAPM_ENUM_EXT(tx_smic0, BOLERO_CDC_TX_INP_MUX_ADC_MUX0_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_smic1, BOLERO_CDC_TX_INP_MUX_ADC_MUX1_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_smic2, BOLERO_CDC_TX_INP_MUX_ADC_MUX2_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_smic3, BOLERO_CDC_TX_INP_MUX_ADC_MUX3_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_smic4, BOLERO_CDC_TX_INP_MUX_ADC_MUX4_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_smic5, BOLERO_CDC_TX_INP_MUX_ADC_MUX5_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_smic6, BOLERO_CDC_TX_INP_MUX_ADC_MUX6_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_smic7, BOLERO_CDC_TX_INP_MUX_ADC_MUX7_CFG0,
			0, smic_mux_text, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

static const char * const smic_mux_text_v2[] = {
	"ZERO", "SWR_MIC0", "SWR_MIC1", "SWR_MIC2", "SWR_MIC3",
	"SWR_MIC4", "SWR_MIC5", "SWR_MIC6", "SWR_MIC7",
	"SWR_MIC8", "SWR_MIC9", "SWR_MIC10", "SWR_MIC11"
};

TX_MACRO_DAPM_ENUM_EXT(tx_smic0_v2, BOLERO_CDC_TX_INP_MUX_ADC_MUX0_CFG0,
			0, smic_mux_text_v2, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_smic1_v2, BOLERO_CDC_TX_INP_MUX_ADC_MUX1_CFG0,
			0, smic_mux_text_v2, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_smic2_v2, BOLERO_CDC_TX_INP_MUX_ADC_MUX2_CFG0,
			0, smic_mux_text_v2, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_smic3_v2, BOLERO_CDC_TX_INP_MUX_ADC_MUX3_CFG0,
			0, smic_mux_text_v2, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_smic4_v3, BOLERO_CDC_TX_INP_MUX_ADC_MUX4_CFG0,
			0, smic_mux_text_v2, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_smic5_v3, BOLERO_CDC_TX_INP_MUX_ADC_MUX5_CFG0,
			0, smic_mux_text_v2, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_smic6_v3, BOLERO_CDC_TX_INP_MUX_ADC_MUX6_CFG0,
			0, smic_mux_text_v2, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

TX_MACRO_DAPM_ENUM_EXT(tx_smic7_v3, BOLERO_CDC_TX_INP_MUX_ADC_MUX7_CFG0,
			0, smic_mux_text_v2, snd_soc_dapm_get_enum_double,
			tx_macro_put_dec_enum);

static const char * const dec_mode_mux_text[] = {
	"ADC_DEFAULT", "ADC_LOW_PWR", "ADC_HIGH_PERF",
};

static const struct soc_enum dec_mode_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dec_mode_mux_text),
			    dec_mode_mux_text);

static const char * const bcs_ch_enum_text[] = {
	"CH0", "CH1", "CH2", "CH3", "CH4", "CH5", "CH6", "CH7", "CH8", "CH9",
	"CH10", "CH11",
};

static const struct soc_enum bcs_ch_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(bcs_ch_enum_text),
			    bcs_ch_enum_text);

static const struct snd_kcontrol_new tx_aif1_cap_mixer[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, TX_MACRO_DEC0, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, TX_MACRO_DEC1, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC2", SND_SOC_NOPM, TX_MACRO_DEC2, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC3", SND_SOC_NOPM, TX_MACRO_DEC3, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC4", SND_SOC_NOPM, TX_MACRO_DEC4, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC5", SND_SOC_NOPM, TX_MACRO_DEC5, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC6", SND_SOC_NOPM, TX_MACRO_DEC6, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC7", SND_SOC_NOPM, TX_MACRO_DEC7, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
};

static const struct snd_kcontrol_new tx_aif2_cap_mixer[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, TX_MACRO_DEC0, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, TX_MACRO_DEC1, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC2", SND_SOC_NOPM, TX_MACRO_DEC2, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC3", SND_SOC_NOPM, TX_MACRO_DEC3, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC4", SND_SOC_NOPM, TX_MACRO_DEC4, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC5", SND_SOC_NOPM, TX_MACRO_DEC5, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC6", SND_SOC_NOPM, TX_MACRO_DEC6, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC7", SND_SOC_NOPM, TX_MACRO_DEC7, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
};

static const struct snd_kcontrol_new tx_aif3_cap_mixer[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, TX_MACRO_DEC0, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, TX_MACRO_DEC1, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC2", SND_SOC_NOPM, TX_MACRO_DEC2, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC3", SND_SOC_NOPM, TX_MACRO_DEC3, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC4", SND_SOC_NOPM, TX_MACRO_DEC4, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC5", SND_SOC_NOPM, TX_MACRO_DEC5, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC6", SND_SOC_NOPM, TX_MACRO_DEC6, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC7", SND_SOC_NOPM, TX_MACRO_DEC7, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
};

static const struct snd_kcontrol_new tx_aif1_cap_mixer_v2[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, TX_MACRO_DEC0, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, TX_MACRO_DEC1, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC2", SND_SOC_NOPM, TX_MACRO_DEC2, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC3", SND_SOC_NOPM, TX_MACRO_DEC3, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
};

static const struct snd_kcontrol_new tx_aif2_cap_mixer_v2[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, TX_MACRO_DEC0, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, TX_MACRO_DEC1, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC2", SND_SOC_NOPM, TX_MACRO_DEC2, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC3", SND_SOC_NOPM, TX_MACRO_DEC3, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
};

static const struct snd_kcontrol_new tx_aif3_cap_mixer_v2[] = {
	SOC_SINGLE_EXT("DEC0", SND_SOC_NOPM, TX_MACRO_DEC0, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC1", SND_SOC_NOPM, TX_MACRO_DEC1, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC2", SND_SOC_NOPM, TX_MACRO_DEC2, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
	SOC_SINGLE_EXT("DEC3", SND_SOC_NOPM, TX_MACRO_DEC3, 1, 0,
			tx_macro_tx_mixer_get, tx_macro_tx_mixer_put),
};

static const struct snd_soc_dapm_widget tx_macro_dapm_widgets_common[] = {
	SND_SOC_DAPM_AIF_OUT("TX_AIF1 CAP", "TX_AIF1 Capture", 0,
		SND_SOC_NOPM, TX_MACRO_AIF1_CAP, 0),

	SND_SOC_DAPM_AIF_OUT("TX_AIF2 CAP", "TX_AIF2 Capture", 0,
		SND_SOC_NOPM, TX_MACRO_AIF2_CAP, 0),

	SND_SOC_DAPM_AIF_OUT("TX_AIF3 CAP", "TX_AIF3 Capture", 0,
		SND_SOC_NOPM, TX_MACRO_AIF3_CAP, 0),

	TX_MACRO_DAPM_MUX("TX DMIC MUX0", 0, tx_dmic0),
	TX_MACRO_DAPM_MUX("TX DMIC MUX1", 0, tx_dmic1),
	TX_MACRO_DAPM_MUX("TX DMIC MUX2", 0, tx_dmic2),
	TX_MACRO_DAPM_MUX("TX DMIC MUX3", 0, tx_dmic3),

	TX_MACRO_DAPM_MUX("TX SMIC MUX0", 0, tx_smic0_v2),
	TX_MACRO_DAPM_MUX("TX SMIC MUX1", 0, tx_smic1_v2),
	TX_MACRO_DAPM_MUX("TX SMIC MUX2", 0, tx_smic2_v2),
	TX_MACRO_DAPM_MUX("TX SMIC MUX3", 0, tx_smic3_v2),

	SND_SOC_DAPM_SUPPLY("TX MIC BIAS1", SND_SOC_NOPM, 0, 0,
		tx_macro_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC("TX DMIC0", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("TX DMIC1", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("TX DMIC2", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("TX DMIC3", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("TX DMIC4", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("TX DMIC5", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("TX DMIC6", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("TX DMIC7", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_INPUT("TX SWR_INPUT"),

	SND_SOC_DAPM_MUX_E("TX DEC0 MUX", SND_SOC_NOPM,
			   TX_MACRO_DEC0, 0,
			   &tx_dec0_mux, tx_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("TX DEC1 MUX", SND_SOC_NOPM,
			   TX_MACRO_DEC1, 0,
			   &tx_dec1_mux, tx_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("TX DEC2 MUX", SND_SOC_NOPM,
			   TX_MACRO_DEC2, 0,
			   &tx_dec2_mux, tx_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("TX DEC3 MUX", SND_SOC_NOPM,
			   TX_MACRO_DEC3, 0,
			   &tx_dec3_mux, tx_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("TX_MCLK", 0, SND_SOC_NOPM, 0, 0,
	tx_macro_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("TX_SWR_PWR", -1, SND_SOC_NOPM, 0, 0,
			      tx_macro_swr_pwr_event,
			      SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_widget tx_macro_dapm_widgets_v2[] = {
	SND_SOC_DAPM_MIXER("TX_AIF1_CAP Mixer", SND_SOC_NOPM,
		TX_MACRO_AIF1_CAP, 0,
		tx_aif1_cap_mixer_v2, ARRAY_SIZE(tx_aif1_cap_mixer_v2)),

	SND_SOC_DAPM_MIXER("TX_AIF2_CAP Mixer", SND_SOC_NOPM,
		TX_MACRO_AIF2_CAP, 0,
		tx_aif2_cap_mixer_v2, ARRAY_SIZE(tx_aif2_cap_mixer_v2)),

	SND_SOC_DAPM_MIXER("TX_AIF3_CAP Mixer", SND_SOC_NOPM,
		TX_MACRO_AIF3_CAP, 0,
		tx_aif3_cap_mixer_v2, ARRAY_SIZE(tx_aif3_cap_mixer_v2)),
};

static const struct snd_soc_dapm_widget tx_macro_dapm_widgets_v3[] = {
	SND_SOC_DAPM_MIXER("TX_AIF1_CAP Mixer", SND_SOC_NOPM,
		TX_MACRO_AIF1_CAP, 0,
		tx_aif1_cap_mixer, ARRAY_SIZE(tx_aif1_cap_mixer)),

	SND_SOC_DAPM_MIXER("TX_AIF2_CAP Mixer", SND_SOC_NOPM,
		TX_MACRO_AIF2_CAP, 0,
		tx_aif2_cap_mixer, ARRAY_SIZE(tx_aif2_cap_mixer)),

	SND_SOC_DAPM_MIXER("TX_AIF3_CAP Mixer", SND_SOC_NOPM,
		TX_MACRO_AIF3_CAP, 0,
		tx_aif3_cap_mixer, ARRAY_SIZE(tx_aif3_cap_mixer)),

	TX_MACRO_DAPM_MUX("TX DMIC MUX4", 0, tx_dmic4),
	TX_MACRO_DAPM_MUX("TX DMIC MUX5", 0, tx_dmic5),
	TX_MACRO_DAPM_MUX("TX DMIC MUX6", 0, tx_dmic6),
	TX_MACRO_DAPM_MUX("TX DMIC MUX7", 0, tx_dmic7),

	TX_MACRO_DAPM_MUX("TX SMIC MUX4", 0, tx_smic4_v3),
	TX_MACRO_DAPM_MUX("TX SMIC MUX5", 0, tx_smic5_v3),
	TX_MACRO_DAPM_MUX("TX SMIC MUX6", 0, tx_smic6_v3),
	TX_MACRO_DAPM_MUX("TX SMIC MUX7", 0, tx_smic7_v3),

	SND_SOC_DAPM_MUX_E("TX DEC4 MUX", SND_SOC_NOPM,
			   TX_MACRO_DEC4, 0,
			   &tx_dec4_mux, tx_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("TX DEC5 MUX", SND_SOC_NOPM,
			   TX_MACRO_DEC5, 0,
			   &tx_dec5_mux, tx_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("TX DEC6 MUX", SND_SOC_NOPM,
			   TX_MACRO_DEC6, 0,
			   &tx_dec6_mux, tx_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("TX DEC7 MUX", SND_SOC_NOPM,
			   TX_MACRO_DEC7, 0,
			   &tx_dec7_mux, tx_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("TX_SWR_CLK", -1, SND_SOC_NOPM, 0, 0,
			tx_macro_tx_swr_clk_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("VA_SWR_CLK", -1, SND_SOC_NOPM, 0, 0,
			tx_macro_va_swr_clk_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_widget tx_macro_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_OUT("TX_AIF1 CAP", "TX_AIF1 Capture", 0,
		SND_SOC_NOPM, TX_MACRO_AIF1_CAP, 0),

	SND_SOC_DAPM_AIF_OUT("TX_AIF2 CAP", "TX_AIF2 Capture", 0,
		SND_SOC_NOPM, TX_MACRO_AIF2_CAP, 0),

	SND_SOC_DAPM_AIF_OUT("TX_AIF3 CAP", "TX_AIF3 Capture", 0,
		SND_SOC_NOPM, TX_MACRO_AIF3_CAP, 0),

	SND_SOC_DAPM_MIXER("TX_AIF1_CAP Mixer", SND_SOC_NOPM, TX_MACRO_AIF1_CAP, 0,
		tx_aif1_cap_mixer, ARRAY_SIZE(tx_aif1_cap_mixer)),

	SND_SOC_DAPM_MIXER("TX_AIF2_CAP Mixer", SND_SOC_NOPM, TX_MACRO_AIF2_CAP, 0,
		tx_aif2_cap_mixer, ARRAY_SIZE(tx_aif2_cap_mixer)),

	SND_SOC_DAPM_MIXER("TX_AIF3_CAP Mixer", SND_SOC_NOPM, TX_MACRO_AIF3_CAP, 0,
		tx_aif3_cap_mixer, ARRAY_SIZE(tx_aif3_cap_mixer)),


	TX_MACRO_DAPM_MUX("TX DMIC MUX0", 0, tx_dmic0),
	TX_MACRO_DAPM_MUX("TX DMIC MUX1", 0, tx_dmic1),
	TX_MACRO_DAPM_MUX("TX DMIC MUX2", 0, tx_dmic2),
	TX_MACRO_DAPM_MUX("TX DMIC MUX3", 0, tx_dmic3),
	TX_MACRO_DAPM_MUX("TX DMIC MUX4", 0, tx_dmic4),
	TX_MACRO_DAPM_MUX("TX DMIC MUX5", 0, tx_dmic5),
	TX_MACRO_DAPM_MUX("TX DMIC MUX6", 0, tx_dmic6),
	TX_MACRO_DAPM_MUX("TX DMIC MUX7", 0, tx_dmic7),

	TX_MACRO_DAPM_MUX("TX SMIC MUX0", 0, tx_smic0),
	TX_MACRO_DAPM_MUX("TX SMIC MUX1", 0, tx_smic1),
	TX_MACRO_DAPM_MUX("TX SMIC MUX2", 0, tx_smic2),
	TX_MACRO_DAPM_MUX("TX SMIC MUX3", 0, tx_smic3),
	TX_MACRO_DAPM_MUX("TX SMIC MUX4", 0, tx_smic4),
	TX_MACRO_DAPM_MUX("TX SMIC MUX5", 0, tx_smic5),
	TX_MACRO_DAPM_MUX("TX SMIC MUX6", 0, tx_smic6),
	TX_MACRO_DAPM_MUX("TX SMIC MUX7", 0, tx_smic7),

	SND_SOC_DAPM_SUPPLY("TX MIC BIAS1", SND_SOC_NOPM, 0, 0,
		tx_macro_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC("TX DMIC0", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("TX DMIC1", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("TX DMIC2", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("TX DMIC3", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("TX DMIC4", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("TX DMIC5", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("TX DMIC6", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_ADC("TX DMIC7", NULL, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_INPUT("TX SWR_ADC0"),
	SND_SOC_DAPM_INPUT("TX SWR_ADC1"),
	SND_SOC_DAPM_INPUT("TX SWR_ADC2"),
	SND_SOC_DAPM_INPUT("TX SWR_ADC3"),
	SND_SOC_DAPM_INPUT("TX SWR_DMIC0"),
	SND_SOC_DAPM_INPUT("TX SWR_DMIC1"),
	SND_SOC_DAPM_INPUT("TX SWR_DMIC2"),
	SND_SOC_DAPM_INPUT("TX SWR_DMIC3"),
	SND_SOC_DAPM_INPUT("TX SWR_DMIC4"),
	SND_SOC_DAPM_INPUT("TX SWR_DMIC5"),
	SND_SOC_DAPM_INPUT("TX SWR_DMIC6"),
	SND_SOC_DAPM_INPUT("TX SWR_DMIC7"),

	SND_SOC_DAPM_MUX_E("TX DEC0 MUX", SND_SOC_NOPM,
			   TX_MACRO_DEC0, 0,
			   &tx_dec0_mux, tx_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("TX DEC1 MUX", SND_SOC_NOPM,
			   TX_MACRO_DEC1, 0,
			   &tx_dec1_mux, tx_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("TX DEC2 MUX", SND_SOC_NOPM,
			   TX_MACRO_DEC2, 0,
			   &tx_dec2_mux, tx_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("TX DEC3 MUX", SND_SOC_NOPM,
			   TX_MACRO_DEC3, 0,
			   &tx_dec3_mux, tx_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("TX DEC4 MUX", SND_SOC_NOPM,
			   TX_MACRO_DEC4, 0,
			   &tx_dec4_mux, tx_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("TX DEC5 MUX", SND_SOC_NOPM,
			   TX_MACRO_DEC5, 0,
			   &tx_dec5_mux, tx_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("TX DEC6 MUX", SND_SOC_NOPM,
			   TX_MACRO_DEC6, 0,
			   &tx_dec6_mux, tx_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("TX DEC7 MUX", SND_SOC_NOPM,
			   TX_MACRO_DEC7, 0,
			   &tx_dec7_mux, tx_macro_enable_dec,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			   SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("TX_MCLK", 0, SND_SOC_NOPM, 0, 0,
	tx_macro_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("TX_SWR_CLK", 0, SND_SOC_NOPM, 0, 0,
			tx_macro_tx_swr_clk_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("VA_SWR_CLK", 0, SND_SOC_NOPM, 0, 0,
			tx_macro_va_swr_clk_event,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route tx_audio_map_common[] = {
	{"TX_AIF1 CAP", NULL, "TX_MCLK"},
	{"TX_AIF2 CAP", NULL, "TX_MCLK"},
	{"TX_AIF3 CAP", NULL, "TX_MCLK"},

	{"TX_AIF1 CAP", NULL, "TX_AIF1_CAP Mixer"},
	{"TX_AIF2 CAP", NULL, "TX_AIF2_CAP Mixer"},
	{"TX_AIF3 CAP", NULL, "TX_AIF3_CAP Mixer"},

	{"TX_AIF1_CAP Mixer", "DEC0", "TX DEC0 MUX"},
	{"TX_AIF1_CAP Mixer", "DEC1", "TX DEC1 MUX"},
	{"TX_AIF1_CAP Mixer", "DEC2", "TX DEC2 MUX"},
	{"TX_AIF1_CAP Mixer", "DEC3", "TX DEC3 MUX"},

	{"TX_AIF2_CAP Mixer", "DEC0", "TX DEC0 MUX"},
	{"TX_AIF2_CAP Mixer", "DEC1", "TX DEC1 MUX"},
	{"TX_AIF2_CAP Mixer", "DEC2", "TX DEC2 MUX"},
	{"TX_AIF2_CAP Mixer", "DEC3", "TX DEC3 MUX"},

	{"TX_AIF3_CAP Mixer", "DEC0", "TX DEC0 MUX"},
	{"TX_AIF3_CAP Mixer", "DEC1", "TX DEC1 MUX"},
	{"TX_AIF3_CAP Mixer", "DEC2", "TX DEC2 MUX"},
	{"TX_AIF3_CAP Mixer", "DEC3", "TX DEC3 MUX"},

	{"TX DEC0 MUX", NULL, "TX_MCLK"},
	{"TX DEC1 MUX", NULL, "TX_MCLK"},
	{"TX DEC2 MUX", NULL, "TX_MCLK"},
	{"TX DEC3 MUX", NULL, "TX_MCLK"},

	{"TX DEC0 MUX", "MSM_DMIC", "TX DMIC MUX0"},
	{"TX DMIC MUX0", "DMIC0", "TX DMIC0"},
	{"TX DMIC MUX0", "DMIC1", "TX DMIC1"},
	{"TX DMIC MUX0", "DMIC2", "TX DMIC2"},
	{"TX DMIC MUX0", "DMIC3", "TX DMIC3"},
	{"TX DMIC MUX0", "DMIC4", "TX DMIC4"},
	{"TX DMIC MUX0", "DMIC5", "TX DMIC5"},
	{"TX DMIC MUX0", "DMIC6", "TX DMIC6"},
	{"TX DMIC MUX0", "DMIC7", "TX DMIC7"},

	{"TX DEC0 MUX", "SWR_MIC", "TX SMIC MUX0"},
	{"TX SMIC MUX0", "SWR_MIC0", "TX SWR_INPUT"},
	{"TX SMIC MUX0", "SWR_MIC1", "TX SWR_INPUT"},
	{"TX SMIC MUX0", "SWR_MIC2", "TX SWR_INPUT"},
	{"TX SMIC MUX0", "SWR_MIC3", "TX SWR_INPUT"},
	{"TX SMIC MUX0", "SWR_MIC4", "TX SWR_INPUT"},
	{"TX SMIC MUX0", "SWR_MIC5", "TX SWR_INPUT"},
	{"TX SMIC MUX0", "SWR_MIC6", "TX SWR_INPUT"},
	{"TX SMIC MUX0", "SWR_MIC7", "TX SWR_INPUT"},
	{"TX SMIC MUX0", "SWR_MIC8", "TX SWR_INPUT"},
	{"TX SMIC MUX0", "SWR_MIC9", "TX SWR_INPUT"},
	{"TX SMIC MUX0", "SWR_MIC10", "TX SWR_INPUT"},
	{"TX SMIC MUX0", "SWR_MIC11", "TX SWR_INPUT"},

	{"TX DEC1 MUX", "MSM_DMIC", "TX DMIC MUX1"},
	{"TX DMIC MUX1", "DMIC0", "TX DMIC0"},
	{"TX DMIC MUX1", "DMIC1", "TX DMIC1"},
	{"TX DMIC MUX1", "DMIC2", "TX DMIC2"},
	{"TX DMIC MUX1", "DMIC3", "TX DMIC3"},
	{"TX DMIC MUX1", "DMIC4", "TX DMIC4"},
	{"TX DMIC MUX1", "DMIC5", "TX DMIC5"},
	{"TX DMIC MUX1", "DMIC6", "TX DMIC6"},
	{"TX DMIC MUX1", "DMIC7", "TX DMIC7"},

	{"TX DEC1 MUX", "SWR_MIC", "TX SMIC MUX1"},
	{"TX SMIC MUX1", "SWR_MIC0", "TX SWR_INPUT"},
	{"TX SMIC MUX1", "SWR_MIC1", "TX SWR_INPUT"},
	{"TX SMIC MUX1", "SWR_MIC2", "TX SWR_INPUT"},
	{"TX SMIC MUX1", "SWR_MIC3", "TX SWR_INPUT"},
	{"TX SMIC MUX1", "SWR_MIC4", "TX SWR_INPUT"},
	{"TX SMIC MUX1", "SWR_MIC5", "TX SWR_INPUT"},
	{"TX SMIC MUX1", "SWR_MIC6", "TX SWR_INPUT"},
	{"TX SMIC MUX1", "SWR_MIC7", "TX SWR_INPUT"},
	{"TX SMIC MUX1", "SWR_MIC8", "TX SWR_INPUT"},
	{"TX SMIC MUX1", "SWR_MIC9", "TX SWR_INPUT"},
	{"TX SMIC MUX1", "SWR_MIC10", "TX SWR_INPUT"},
	{"TX SMIC MUX1", "SWR_MIC11", "TX SWR_INPUT"},

	{"TX DEC2 MUX", "MSM_DMIC", "TX DMIC MUX2"},
	{"TX DMIC MUX2", "DMIC0", "TX DMIC0"},
	{"TX DMIC MUX2", "DMIC1", "TX DMIC1"},
	{"TX DMIC MUX2", "DMIC2", "TX DMIC2"},
	{"TX DMIC MUX2", "DMIC3", "TX DMIC3"},
	{"TX DMIC MUX2", "DMIC4", "TX DMIC4"},
	{"TX DMIC MUX2", "DMIC5", "TX DMIC5"},
	{"TX DMIC MUX2", "DMIC6", "TX DMIC6"},
	{"TX DMIC MUX2", "DMIC7", "TX DMIC7"},

	{"TX DEC2 MUX", "SWR_MIC", "TX SMIC MUX2"},
	{"TX SMIC MUX2", "SWR_MIC0", "TX SWR_INPUT"},
	{"TX SMIC MUX2", "SWR_MIC1", "TX SWR_INPUT"},
	{"TX SMIC MUX2", "SWR_MIC2", "TX SWR_INPUT"},
	{"TX SMIC MUX2", "SWR_MIC3", "TX SWR_INPUT"},
	{"TX SMIC MUX2", "SWR_MIC4", "TX SWR_INPUT"},
	{"TX SMIC MUX2", "SWR_MIC5", "TX SWR_INPUT"},
	{"TX SMIC MUX2", "SWR_MIC6", "TX SWR_INPUT"},
	{"TX SMIC MUX2", "SWR_MIC7", "TX SWR_INPUT"},
	{"TX SMIC MUX2", "SWR_MIC8", "TX SWR_INPUT"},
	{"TX SMIC MUX2", "SWR_MIC9", "TX SWR_INPUT"},
	{"TX SMIC MUX2", "SWR_MIC10", "TX SWR_INPUT"},
	{"TX SMIC MUX2", "SWR_MIC11", "TX SWR_INPUT"},

	{"TX DEC3 MUX", "MSM_DMIC", "TX DMIC MUX3"},
	{"TX DMIC MUX3", "DMIC0", "TX DMIC0"},
	{"TX DMIC MUX3", "DMIC1", "TX DMIC1"},
	{"TX DMIC MUX3", "DMIC2", "TX DMIC2"},
	{"TX DMIC MUX3", "DMIC3", "TX DMIC3"},
	{"TX DMIC MUX3", "DMIC4", "TX DMIC4"},
	{"TX DMIC MUX3", "DMIC5", "TX DMIC5"},
	{"TX DMIC MUX3", "DMIC6", "TX DMIC6"},
	{"TX DMIC MUX3", "DMIC7", "TX DMIC7"},

	{"TX DEC3 MUX", "SWR_MIC", "TX SMIC MUX3"},
	{"TX SMIC MUX3", "SWR_MIC0", "TX SWR_INPUT"},
	{"TX SMIC MUX3", "SWR_MIC1", "TX SWR_INPUT"},
	{"TX SMIC MUX3", "SWR_MIC2", "TX SWR_INPUT"},
	{"TX SMIC MUX3", "SWR_MIC3", "TX SWR_INPUT"},
	{"TX SMIC MUX3", "SWR_MIC4", "TX SWR_INPUT"},
	{"TX SMIC MUX3", "SWR_MIC5", "TX SWR_INPUT"},
	{"TX SMIC MUX3", "SWR_MIC6", "TX SWR_INPUT"},
	{"TX SMIC MUX3", "SWR_MIC7", "TX SWR_INPUT"},
	{"TX SMIC MUX3", "SWR_MIC8", "TX SWR_INPUT"},
	{"TX SMIC MUX3", "SWR_MIC9", "TX SWR_INPUT"},
	{"TX SMIC MUX3", "SWR_MIC10", "TX SWR_INPUT"},
	{"TX SMIC MUX3", "SWR_MIC11", "TX SWR_INPUT"},
};

static const struct snd_soc_dapm_route tx_audio_map_v3[] = {
	{"TX_AIF1_CAP Mixer", "DEC4", "TX DEC4 MUX"},
	{"TX_AIF1_CAP Mixer", "DEC5", "TX DEC5 MUX"},
	{"TX_AIF1_CAP Mixer", "DEC6", "TX DEC6 MUX"},
	{"TX_AIF1_CAP Mixer", "DEC7", "TX DEC7 MUX"},

	{"TX_AIF2_CAP Mixer", "DEC4", "TX DEC4 MUX"},
	{"TX_AIF2_CAP Mixer", "DEC5", "TX DEC5 MUX"},
	{"TX_AIF2_CAP Mixer", "DEC6", "TX DEC6 MUX"},
	{"TX_AIF2_CAP Mixer", "DEC7", "TX DEC7 MUX"},

	{"TX_AIF3_CAP Mixer", "DEC4", "TX DEC4 MUX"},
	{"TX_AIF3_CAP Mixer", "DEC5", "TX DEC5 MUX"},
	{"TX_AIF3_CAP Mixer", "DEC6", "TX DEC6 MUX"},
	{"TX_AIF3_CAP Mixer", "DEC7", "TX DEC7 MUX"},

	{"TX DEC4 MUX", NULL, "TX_MCLK"},
	{"TX DEC5 MUX", NULL, "TX_MCLK"},
	{"TX DEC6 MUX", NULL, "TX_MCLK"},
	{"TX DEC7 MUX", NULL, "TX_MCLK"},

	{"TX DEC4 MUX", "MSM_DMIC", "TX DMIC MUX4"},
	{"TX DMIC MUX4", "DMIC0", "TX DMIC0"},
	{"TX DMIC MUX4", "DMIC1", "TX DMIC1"},
	{"TX DMIC MUX4", "DMIC2", "TX DMIC2"},
	{"TX DMIC MUX4", "DMIC3", "TX DMIC3"},
	{"TX DMIC MUX4", "DMIC4", "TX DMIC4"},
	{"TX DMIC MUX4", "DMIC5", "TX DMIC5"},
	{"TX DMIC MUX4", "DMIC6", "TX DMIC6"},
	{"TX DMIC MUX4", "DMIC7", "TX DMIC7"},

	{"TX DEC4 MUX", "SWR_MIC", "TX SMIC MUX4"},
	{"TX SMIC MUX4", "SWR_MIC0", "TX SWR_INPUT"},
	{"TX SMIC MUX4", "SWR_MIC1", "TX SWR_INPUT"},
	{"TX SMIC MUX4", "SWR_MIC2", "TX SWR_INPUT"},
	{"TX SMIC MUX4", "SWR_MIC3", "TX SWR_INPUT"},
	{"TX SMIC MUX4", "SWR_MIC4", "TX SWR_INPUT"},
	{"TX SMIC MUX4", "SWR_MIC5", "TX SWR_INPUT"},
	{"TX SMIC MUX4", "SWR_MIC6", "TX SWR_INPUT"},
	{"TX SMIC MUX4", "SWR_MIC7", "TX SWR_INPUT"},
	{"TX SMIC MUX4", "SWR_MIC8", "TX SWR_INPUT"},
	{"TX SMIC MUX4", "SWR_MIC9", "TX SWR_INPUT"},
	{"TX SMIC MUX4", "SWR_MIC10", "TX SWR_INPUT"},
	{"TX SMIC MUX4", "SWR_MIC11", "TX SWR_INPUT"},

	{"TX DEC5 MUX", "MSM_DMIC", "TX DMIC MUX5"},
	{"TX DMIC MUX5", "DMIC0", "TX DMIC0"},
	{"TX DMIC MUX5", "DMIC1", "TX DMIC1"},
	{"TX DMIC MUX5", "DMIC2", "TX DMIC2"},
	{"TX DMIC MUX5", "DMIC3", "TX DMIC3"},
	{"TX DMIC MUX5", "DMIC4", "TX DMIC4"},
	{"TX DMIC MUX5", "DMIC5", "TX DMIC5"},
	{"TX DMIC MUX5", "DMIC6", "TX DMIC6"},
	{"TX DMIC MUX5", "DMIC7", "TX DMIC7"},

	{"TX DEC5 MUX", "SWR_MIC", "TX SMIC MUX5"},
	{"TX SMIC MUX5", "SWR_MIC0", "TX SWR_INPUT"},
	{"TX SMIC MUX5", "SWR_MIC1", "TX SWR_INPUT"},
	{"TX SMIC MUX5", "SWR_MIC2", "TX SWR_INPUT"},
	{"TX SMIC MUX5", "SWR_MIC3", "TX SWR_INPUT"},
	{"TX SMIC MUX5", "SWR_MIC4", "TX SWR_INPUT"},
	{"TX SMIC MUX5", "SWR_MIC5", "TX SWR_INPUT"},
	{"TX SMIC MUX5", "SWR_MIC6", "TX SWR_INPUT"},
	{"TX SMIC MUX5", "SWR_MIC7", "TX SWR_INPUT"},
	{"TX SMIC MUX5", "SWR_MIC8", "TX SWR_INPUT"},
	{"TX SMIC MUX5", "SWR_MIC9", "TX SWR_INPUT"},
	{"TX SMIC MUX5", "SWR_MIC10", "TX SWR_INPUT"},
	{"TX SMIC MUX5", "SWR_MIC11", "TX SWR_INPUT"},

	{"TX DEC6 MUX", "MSM_DMIC", "TX DMIC MUX6"},
	{"TX DMIC MUX6", "DMIC0", "TX DMIC0"},
	{"TX DMIC MUX6", "DMIC1", "TX DMIC1"},
	{"TX DMIC MUX6", "DMIC2", "TX DMIC2"},
	{"TX DMIC MUX6", "DMIC3", "TX DMIC3"},
	{"TX DMIC MUX6", "DMIC4", "TX DMIC4"},
	{"TX DMIC MUX6", "DMIC5", "TX DMIC5"},
	{"TX DMIC MUX6", "DMIC6", "TX DMIC6"},
	{"TX DMIC MUX6", "DMIC7", "TX DMIC7"},

	{"TX DEC6 MUX", "SWR_MIC", "TX SMIC MUX6"},
	{"TX SMIC MUX6", "SWR_MIC0", "TX SWR_INPUT"},
	{"TX SMIC MUX6", "SWR_MIC1", "TX SWR_INPUT"},
	{"TX SMIC MUX6", "SWR_MIC2", "TX SWR_INPUT"},
	{"TX SMIC MUX6", "SWR_MIC3", "TX SWR_INPUT"},
	{"TX SMIC MUX6", "SWR_MIC4", "TX SWR_INPUT"},
	{"TX SMIC MUX6", "SWR_MIC5", "TX SWR_INPUT"},
	{"TX SMIC MUX6", "SWR_MIC6", "TX SWR_INPUT"},
	{"TX SMIC MUX6", "SWR_MIC7", "TX SWR_INPUT"},
	{"TX SMIC MUX6", "SWR_MIC8", "TX SWR_INPUT"},
	{"TX SMIC MUX6", "SWR_MIC9", "TX SWR_INPUT"},
	{"TX SMIC MUX6", "SWR_MIC10", "TX SWR_INPUT"},
	{"TX SMIC MUX6", "SWR_MIC11", "TX SWR_INPUT"},

	{"TX DEC7 MUX", "MSM_DMIC", "TX DMIC MUX7"},
	{"TX DMIC MUX7", "DMIC0", "TX DMIC0"},
	{"TX DMIC MUX7", "DMIC1", "TX DMIC1"},
	{"TX DMIC MUX7", "DMIC2", "TX DMIC2"},
	{"TX DMIC MUX7", "DMIC3", "TX DMIC3"},
	{"TX DMIC MUX7", "DMIC4", "TX DMIC4"},
	{"TX DMIC MUX7", "DMIC5", "TX DMIC5"},
	{"TX DMIC MUX7", "DMIC6", "TX DMIC6"},
	{"TX DMIC MUX7", "DMIC7", "TX DMIC7"},

	{"TX DEC7 MUX", "SWR_MIC", "TX SMIC MUX7"},
	{"TX SMIC MUX7", "SWR_MIC0", "TX SWR_INPUT"},
	{"TX SMIC MUX7", "SWR_MIC1", "TX SWR_INPUT"},
	{"TX SMIC MUX7", "SWR_MIC2", "TX SWR_INPUT"},
	{"TX SMIC MUX7", "SWR_MIC3", "TX SWR_INPUT"},
	{"TX SMIC MUX7", "SWR_MIC4", "TX SWR_INPUT"},
	{"TX SMIC MUX7", "SWR_MIC5", "TX SWR_INPUT"},
	{"TX SMIC MUX7", "SWR_MIC6", "TX SWR_INPUT"},
	{"TX SMIC MUX7", "SWR_MIC7", "TX SWR_INPUT"},
	{"TX SMIC MUX7", "SWR_MIC8", "TX SWR_INPUT"},
	{"TX SMIC MUX7", "SWR_MIC9", "TX SWR_INPUT"},
	{"TX SMIC MUX7", "SWR_MIC10", "TX SWR_INPUT"},
	{"TX SMIC MUX7", "SWR_MIC11", "TX SWR_INPUT"},

	{"TX SMIC MUX0", NULL, "TX_SWR_CLK"},
	{"TX SMIC MUX1", NULL, "TX_SWR_CLK"},
	{"TX SMIC MUX2", NULL, "TX_SWR_CLK"},
	{"TX SMIC MUX3", NULL, "TX_SWR_CLK"},
	{"TX SMIC MUX4", NULL, "TX_SWR_CLK"},
	{"TX SMIC MUX5", NULL, "TX_SWR_CLK"},
	{"TX SMIC MUX6", NULL, "TX_SWR_CLK"},
	{"TX SMIC MUX7", NULL, "TX_SWR_CLK"},

	{"TX SWR_INPUT", NULL, "TX_SWR_PWR"},
	{"TX SWR_INPUT", NULL, "TX_SWR_PWR"},
	{"TX SWR_INPUT", NULL, "TX_SWR_PWR"},
	{"TX SWR_INPUT", NULL, "TX_SWR_PWR"},
	{"TX SWR_INPUT", NULL, "TX_SWR_PWR"},
	{"TX SWR_INPUT", NULL, "TX_SWR_PWR"},
	{"TX SWR_INPUT", NULL, "TX_SWR_PWR"},
	{"TX SWR_INPUT", NULL, "TX_SWR_PWR"},
	{"TX SWR_INPUT", NULL, "TX_SWR_PWR"},
	{"TX SWR_INPUT", NULL, "TX_SWR_PWR"},
	{"TX SWR_INPUT", NULL, "TX_SWR_PWR"},
	{"TX SWR_INPUT", NULL, "TX_SWR_PWR"},
};

static const struct snd_soc_dapm_route tx_audio_map[] = {
	{"TX_AIF1 CAP", NULL, "TX_MCLK"},
	{"TX_AIF2 CAP", NULL, "TX_MCLK"},
	{"TX_AIF3 CAP", NULL, "TX_MCLK"},

	{"TX_AIF1 CAP", NULL, "TX_AIF1_CAP Mixer"},
	{"TX_AIF2 CAP", NULL, "TX_AIF2_CAP Mixer"},
	{"TX_AIF3 CAP", NULL, "TX_AIF3_CAP Mixer"},

	{"TX_AIF1_CAP Mixer", "DEC0", "TX DEC0 MUX"},
	{"TX_AIF1_CAP Mixer", "DEC1", "TX DEC1 MUX"},
	{"TX_AIF1_CAP Mixer", "DEC2", "TX DEC2 MUX"},
	{"TX_AIF1_CAP Mixer", "DEC3", "TX DEC3 MUX"},
	{"TX_AIF1_CAP Mixer", "DEC4", "TX DEC4 MUX"},
	{"TX_AIF1_CAP Mixer", "DEC5", "TX DEC5 MUX"},
	{"TX_AIF1_CAP Mixer", "DEC6", "TX DEC6 MUX"},
	{"TX_AIF1_CAP Mixer", "DEC7", "TX DEC7 MUX"},

	{"TX_AIF2_CAP Mixer", "DEC0", "TX DEC0 MUX"},
	{"TX_AIF2_CAP Mixer", "DEC1", "TX DEC1 MUX"},
	{"TX_AIF2_CAP Mixer", "DEC2", "TX DEC2 MUX"},
	{"TX_AIF2_CAP Mixer", "DEC3", "TX DEC3 MUX"},
	{"TX_AIF2_CAP Mixer", "DEC4", "TX DEC4 MUX"},
	{"TX_AIF2_CAP Mixer", "DEC5", "TX DEC5 MUX"},
	{"TX_AIF2_CAP Mixer", "DEC6", "TX DEC6 MUX"},
	{"TX_AIF2_CAP Mixer", "DEC7", "TX DEC7 MUX"},

	{"TX_AIF3_CAP Mixer", "DEC0", "TX DEC0 MUX"},
	{"TX_AIF3_CAP Mixer", "DEC1", "TX DEC1 MUX"},
	{"TX_AIF3_CAP Mixer", "DEC2", "TX DEC2 MUX"},
	{"TX_AIF3_CAP Mixer", "DEC3", "TX DEC3 MUX"},
	{"TX_AIF3_CAP Mixer", "DEC4", "TX DEC4 MUX"},
	{"TX_AIF3_CAP Mixer", "DEC5", "TX DEC5 MUX"},
	{"TX_AIF3_CAP Mixer", "DEC6", "TX DEC6 MUX"},
	{"TX_AIF3_CAP Mixer", "DEC7", "TX DEC7 MUX"},

	{"TX DEC0 MUX", NULL, "TX_MCLK"},
	{"TX DEC1 MUX", NULL, "TX_MCLK"},
	{"TX DEC2 MUX", NULL, "TX_MCLK"},
	{"TX DEC3 MUX", NULL, "TX_MCLK"},
	{"TX DEC4 MUX", NULL, "TX_MCLK"},
	{"TX DEC5 MUX", NULL, "TX_MCLK"},
	{"TX DEC6 MUX", NULL, "TX_MCLK"},
	{"TX DEC7 MUX", NULL, "TX_MCLK"},

	{"TX DEC0 MUX", "MSM_DMIC", "TX DMIC MUX0"},
	{"TX DMIC MUX0", "DMIC0", "TX DMIC0"},
	{"TX DMIC MUX0", "DMIC1", "TX DMIC1"},
	{"TX DMIC MUX0", "DMIC2", "TX DMIC2"},
	{"TX DMIC MUX0", "DMIC3", "TX DMIC3"},
	{"TX DMIC MUX0", "DMIC4", "TX DMIC4"},
	{"TX DMIC MUX0", "DMIC5", "TX DMIC5"},
	{"TX DMIC MUX0", "DMIC6", "TX DMIC6"},
	{"TX DMIC MUX0", "DMIC7", "TX DMIC7"},

	{"TX DEC0 MUX", "SWR_MIC", "TX SMIC MUX0"},
	{"TX SMIC MUX0", NULL, "TX_SWR_CLK"},
	{"TX SMIC MUX0", "ADC0", "TX SWR_ADC0"},
	{"TX SMIC MUX0", "ADC1", "TX SWR_ADC1"},
	{"TX SMIC MUX0", "ADC2", "TX SWR_ADC2"},
	{"TX SMIC MUX0", "ADC3", "TX SWR_ADC3"},
	{"TX SMIC MUX0", "SWR_DMIC0", "TX SWR_DMIC0"},
	{"TX SMIC MUX0", "SWR_DMIC1", "TX SWR_DMIC1"},
	{"TX SMIC MUX0", "SWR_DMIC2", "TX SWR_DMIC2"},
	{"TX SMIC MUX0", "SWR_DMIC3", "TX SWR_DMIC3"},
	{"TX SMIC MUX0", "SWR_DMIC4", "TX SWR_DMIC4"},
	{"TX SMIC MUX0", "SWR_DMIC5", "TX SWR_DMIC5"},
	{"TX SMIC MUX0", "SWR_DMIC6", "TX SWR_DMIC6"},
	{"TX SMIC MUX0", "SWR_DMIC7", "TX SWR_DMIC7"},

	{"TX DEC1 MUX", "MSM_DMIC", "TX DMIC MUX1"},
	{"TX DMIC MUX1", "DMIC0", "TX DMIC0"},
	{"TX DMIC MUX1", "DMIC1", "TX DMIC1"},
	{"TX DMIC MUX1", "DMIC2", "TX DMIC2"},
	{"TX DMIC MUX1", "DMIC3", "TX DMIC3"},
	{"TX DMIC MUX1", "DMIC4", "TX DMIC4"},
	{"TX DMIC MUX1", "DMIC5", "TX DMIC5"},
	{"TX DMIC MUX1", "DMIC6", "TX DMIC6"},
	{"TX DMIC MUX1", "DMIC7", "TX DMIC7"},

	{"TX DEC1 MUX", "SWR_MIC", "TX SMIC MUX1"},
	{"TX SMIC MUX1", NULL, "TX_SWR_CLK"},
	{"TX SMIC MUX1", "ADC0", "TX SWR_ADC0"},
	{"TX SMIC MUX1", "ADC1", "TX SWR_ADC1"},
	{"TX SMIC MUX1", "ADC2", "TX SWR_ADC2"},
	{"TX SMIC MUX1", "ADC3", "TX SWR_ADC3"},
	{"TX SMIC MUX1", "SWR_DMIC0", "TX SWR_DMIC0"},
	{"TX SMIC MUX1", "SWR_DMIC1", "TX SWR_DMIC1"},
	{"TX SMIC MUX1", "SWR_DMIC2", "TX SWR_DMIC2"},
	{"TX SMIC MUX1", "SWR_DMIC3", "TX SWR_DMIC3"},
	{"TX SMIC MUX1", "SWR_DMIC4", "TX SWR_DMIC4"},
	{"TX SMIC MUX1", "SWR_DMIC5", "TX SWR_DMIC5"},
	{"TX SMIC MUX1", "SWR_DMIC6", "TX SWR_DMIC6"},
	{"TX SMIC MUX1", "SWR_DMIC7", "TX SWR_DMIC7"},

	{"TX DEC2 MUX", "MSM_DMIC", "TX DMIC MUX2"},
	{"TX DMIC MUX2", "DMIC0", "TX DMIC0"},
	{"TX DMIC MUX2", "DMIC1", "TX DMIC1"},
	{"TX DMIC MUX2", "DMIC2", "TX DMIC2"},
	{"TX DMIC MUX2", "DMIC3", "TX DMIC3"},
	{"TX DMIC MUX2", "DMIC4", "TX DMIC4"},
	{"TX DMIC MUX2", "DMIC5", "TX DMIC5"},
	{"TX DMIC MUX2", "DMIC6", "TX DMIC6"},
	{"TX DMIC MUX2", "DMIC7", "TX DMIC7"},

	{"TX DEC2 MUX", "SWR_MIC", "TX SMIC MUX2"},
	{"TX SMIC MUX2", NULL, "TX_SWR_CLK"},
	{"TX SMIC MUX2", "ADC0", "TX SWR_ADC0"},
	{"TX SMIC MUX2", "ADC1", "TX SWR_ADC1"},
	{"TX SMIC MUX2", "ADC2", "TX SWR_ADC2"},
	{"TX SMIC MUX2", "ADC3", "TX SWR_ADC3"},
	{"TX SMIC MUX2", "SWR_DMIC0", "TX SWR_DMIC0"},
	{"TX SMIC MUX2", "SWR_DMIC1", "TX SWR_DMIC1"},
	{"TX SMIC MUX2", "SWR_DMIC2", "TX SWR_DMIC2"},
	{"TX SMIC MUX2", "SWR_DMIC3", "TX SWR_DMIC3"},
	{"TX SMIC MUX2", "SWR_DMIC4", "TX SWR_DMIC4"},
	{"TX SMIC MUX2", "SWR_DMIC5", "TX SWR_DMIC5"},
	{"TX SMIC MUX2", "SWR_DMIC6", "TX SWR_DMIC6"},
	{"TX SMIC MUX2", "SWR_DMIC7", "TX SWR_DMIC7"},

	{"TX DEC3 MUX", "MSM_DMIC", "TX DMIC MUX3"},
	{"TX DMIC MUX3", "DMIC0", "TX DMIC0"},
	{"TX DMIC MUX3", "DMIC1", "TX DMIC1"},
	{"TX DMIC MUX3", "DMIC2", "TX DMIC2"},
	{"TX DMIC MUX3", "DMIC3", "TX DMIC3"},
	{"TX DMIC MUX3", "DMIC4", "TX DMIC4"},
	{"TX DMIC MUX3", "DMIC5", "TX DMIC5"},
	{"TX DMIC MUX3", "DMIC6", "TX DMIC6"},
	{"TX DMIC MUX3", "DMIC7", "TX DMIC7"},

	{"TX DEC3 MUX", "SWR_MIC", "TX SMIC MUX3"},
	{"TX SMIC MUX3", NULL, "TX_SWR_CLK"},
	{"TX SMIC MUX3", "ADC0", "TX SWR_ADC0"},
	{"TX SMIC MUX3", "ADC1", "TX SWR_ADC1"},
	{"TX SMIC MUX3", "ADC2", "TX SWR_ADC2"},
	{"TX SMIC MUX3", "ADC3", "TX SWR_ADC3"},
	{"TX SMIC MUX3", "SWR_DMIC0", "TX SWR_DMIC0"},
	{"TX SMIC MUX3", "SWR_DMIC1", "TX SWR_DMIC1"},
	{"TX SMIC MUX3", "SWR_DMIC2", "TX SWR_DMIC2"},
	{"TX SMIC MUX3", "SWR_DMIC3", "TX SWR_DMIC3"},
	{"TX SMIC MUX3", "SWR_DMIC4", "TX SWR_DMIC4"},
	{"TX SMIC MUX3", "SWR_DMIC5", "TX SWR_DMIC5"},
	{"TX SMIC MUX3", "SWR_DMIC6", "TX SWR_DMIC6"},
	{"TX SMIC MUX3", "SWR_DMIC7", "TX SWR_DMIC7"},

	{"TX DEC4 MUX", "MSM_DMIC", "TX DMIC MUX4"},
	{"TX DMIC MUX4", "DMIC0", "TX DMIC0"},
	{"TX DMIC MUX4", "DMIC1", "TX DMIC1"},
	{"TX DMIC MUX4", "DMIC2", "TX DMIC2"},
	{"TX DMIC MUX4", "DMIC3", "TX DMIC3"},
	{"TX DMIC MUX4", "DMIC4", "TX DMIC4"},
	{"TX DMIC MUX4", "DMIC5", "TX DMIC5"},
	{"TX DMIC MUX4", "DMIC6", "TX DMIC6"},
	{"TX DMIC MUX4", "DMIC7", "TX DMIC7"},

	{"TX DEC4 MUX", "SWR_MIC", "TX SMIC MUX4"},
	{"TX SMIC MUX4", NULL, "TX_SWR_CLK"},
	{"TX SMIC MUX4", "ADC0", "TX SWR_ADC0"},
	{"TX SMIC MUX4", "ADC1", "TX SWR_ADC1"},
	{"TX SMIC MUX4", "ADC2", "TX SWR_ADC2"},
	{"TX SMIC MUX4", "ADC3", "TX SWR_ADC3"},
	{"TX SMIC MUX4", "SWR_DMIC0", "TX SWR_DMIC0"},
	{"TX SMIC MUX4", "SWR_DMIC1", "TX SWR_DMIC1"},
	{"TX SMIC MUX4", "SWR_DMIC2", "TX SWR_DMIC2"},
	{"TX SMIC MUX4", "SWR_DMIC3", "TX SWR_DMIC3"},
	{"TX SMIC MUX4", "SWR_DMIC4", "TX SWR_DMIC4"},
	{"TX SMIC MUX4", "SWR_DMIC5", "TX SWR_DMIC5"},
	{"TX SMIC MUX4", "SWR_DMIC6", "TX SWR_DMIC6"},
	{"TX SMIC MUX4", "SWR_DMIC7", "TX SWR_DMIC7"},

	{"TX DEC5 MUX", "MSM_DMIC", "TX DMIC MUX5"},
	{"TX DMIC MUX5", "DMIC0", "TX DMIC0"},
	{"TX DMIC MUX5", "DMIC1", "TX DMIC1"},
	{"TX DMIC MUX5", "DMIC2", "TX DMIC2"},
	{"TX DMIC MUX5", "DMIC3", "TX DMIC3"},
	{"TX DMIC MUX5", "DMIC4", "TX DMIC4"},
	{"TX DMIC MUX5", "DMIC5", "TX DMIC5"},
	{"TX DMIC MUX5", "DMIC6", "TX DMIC6"},
	{"TX DMIC MUX5", "DMIC7", "TX DMIC7"},

	{"TX DEC5 MUX", "SWR_MIC", "TX SMIC MUX5"},
	{"TX SMIC MUX5", NULL, "TX_SWR_CLK"},
	{"TX SMIC MUX5", "ADC0", "TX SWR_ADC0"},
	{"TX SMIC MUX5", "ADC1", "TX SWR_ADC1"},
	{"TX SMIC MUX5", "ADC2", "TX SWR_ADC2"},
	{"TX SMIC MUX5", "ADC3", "TX SWR_ADC3"},
	{"TX SMIC MUX5", "SWR_DMIC0", "TX SWR_DMIC0"},
	{"TX SMIC MUX5", "SWR_DMIC1", "TX SWR_DMIC1"},
	{"TX SMIC MUX5", "SWR_DMIC2", "TX SWR_DMIC2"},
	{"TX SMIC MUX5", "SWR_DMIC3", "TX SWR_DMIC3"},
	{"TX SMIC MUX5", "SWR_DMIC4", "TX SWR_DMIC4"},
	{"TX SMIC MUX5", "SWR_DMIC5", "TX SWR_DMIC5"},
	{"TX SMIC MUX5", "SWR_DMIC6", "TX SWR_DMIC6"},
	{"TX SMIC MUX5", "SWR_DMIC7", "TX SWR_DMIC7"},

	{"TX DEC6 MUX", "MSM_DMIC", "TX DMIC MUX6"},
	{"TX DMIC MUX6", "DMIC0", "TX DMIC0"},
	{"TX DMIC MUX6", "DMIC1", "TX DMIC1"},
	{"TX DMIC MUX6", "DMIC2", "TX DMIC2"},
	{"TX DMIC MUX6", "DMIC3", "TX DMIC3"},
	{"TX DMIC MUX6", "DMIC4", "TX DMIC4"},
	{"TX DMIC MUX6", "DMIC5", "TX DMIC5"},
	{"TX DMIC MUX6", "DMIC6", "TX DMIC6"},
	{"TX DMIC MUX6", "DMIC7", "TX DMIC7"},

	{"TX DEC6 MUX", "SWR_MIC", "TX SMIC MUX6"},
	{"TX SMIC MUX6", NULL, "TX_SWR_CLK"},
	{"TX SMIC MUX6", "ADC0", "TX SWR_ADC0"},
	{"TX SMIC MUX6", "ADC1", "TX SWR_ADC1"},
	{"TX SMIC MUX6", "ADC2", "TX SWR_ADC2"},
	{"TX SMIC MUX6", "ADC3", "TX SWR_ADC3"},
	{"TX SMIC MUX6", "SWR_DMIC0", "TX SWR_DMIC0"},
	{"TX SMIC MUX6", "SWR_DMIC1", "TX SWR_DMIC1"},
	{"TX SMIC MUX6", "SWR_DMIC2", "TX SWR_DMIC2"},
	{"TX SMIC MUX6", "SWR_DMIC3", "TX SWR_DMIC3"},
	{"TX SMIC MUX6", "SWR_DMIC4", "TX SWR_DMIC4"},
	{"TX SMIC MUX6", "SWR_DMIC5", "TX SWR_DMIC5"},
	{"TX SMIC MUX6", "SWR_DMIC6", "TX SWR_DMIC6"},
	{"TX SMIC MUX6", "SWR_DMIC7", "TX SWR_DMIC7"},

	{"TX DEC7 MUX", "MSM_DMIC", "TX DMIC MUX7"},
	{"TX DMIC MUX7", "DMIC0", "TX DMIC0"},
	{"TX DMIC MUX7", "DMIC1", "TX DMIC1"},
	{"TX DMIC MUX7", "DMIC2", "TX DMIC2"},
	{"TX DMIC MUX7", "DMIC3", "TX DMIC3"},
	{"TX DMIC MUX7", "DMIC4", "TX DMIC4"},
	{"TX DMIC MUX7", "DMIC5", "TX DMIC5"},
	{"TX DMIC MUX7", "DMIC6", "TX DMIC6"},
	{"TX DMIC MUX7", "DMIC7", "TX DMIC7"},

	{"TX DEC7 MUX", "SWR_MIC", "TX SMIC MUX7"},
	{"TX SMIC MUX7", NULL, "TX_SWR_CLK"},
	{"TX SMIC MUX7", "ADC0", "TX SWR_ADC0"},
	{"TX SMIC MUX7", "ADC1", "TX SWR_ADC1"},
	{"TX SMIC MUX7", "ADC2", "TX SWR_ADC2"},
	{"TX SMIC MUX7", "ADC3", "TX SWR_ADC3"},
	{"TX SMIC MUX7", "SWR_DMIC0", "TX SWR_DMIC0"},
	{"TX SMIC MUX7", "SWR_DMIC1", "TX SWR_DMIC1"},
	{"TX SMIC MUX7", "SWR_DMIC2", "TX SWR_DMIC2"},
	{"TX SMIC MUX7", "SWR_DMIC3", "TX SWR_DMIC3"},
	{"TX SMIC MUX7", "SWR_DMIC4", "TX SWR_DMIC4"},
	{"TX SMIC MUX7", "SWR_DMIC5", "TX SWR_DMIC5"},
	{"TX SMIC MUX7", "SWR_DMIC6", "TX SWR_DMIC6"},
	{"TX SMIC MUX7", "SWR_DMIC7", "TX SWR_DMIC7"},
};

static const struct snd_kcontrol_new tx_macro_snd_controls_common[] = {
	SOC_SINGLE_S8_TLV("TX_DEC0 Volume",
			  BOLERO_CDC_TX0_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("TX_DEC1 Volume",
			  BOLERO_CDC_TX1_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("TX_DEC2 Volume",
			  BOLERO_CDC_TX2_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("TX_DEC3 Volume",
			  BOLERO_CDC_TX3_TX_VOL_CTL,
			  -84, 40, digital_gain),

	SOC_SINGLE_EXT("TX LPI Enable", 0, 0, 1, 0,
		tx_macro_lpi_get, tx_macro_lpi_put),

	SOC_ENUM_EXT("DEC0 MODE", dec_mode_mux_enum,
			tx_macro_dec_mode_get, tx_macro_dec_mode_put),

	SOC_ENUM_EXT("DEC1 MODE", dec_mode_mux_enum,
			tx_macro_dec_mode_get, tx_macro_dec_mode_put),

	SOC_ENUM_EXT("DEC2 MODE", dec_mode_mux_enum,
			tx_macro_dec_mode_get, tx_macro_dec_mode_put),

	SOC_ENUM_EXT("DEC3 MODE", dec_mode_mux_enum,
			tx_macro_dec_mode_get, tx_macro_dec_mode_put),

	SOC_SINGLE_EXT("DEC0_BCS Switch", SND_SOC_NOPM, 0, 1, 0,
		       tx_macro_get_bcs, tx_macro_set_bcs),

	SOC_ENUM_EXT("BCS Channel", bcs_ch_enum,
			tx_macro_bcs_ch_get, tx_macro_bcs_ch_put),

	SOC_ENUM_EXT("BCS CH_SEL", bcs_ch_sel_mux_enum,
			tx_macro_get_bcs_ch_sel, tx_macro_put_bcs_ch_sel),
};

static const struct snd_kcontrol_new tx_macro_snd_controls_v3[] = {
	SOC_SINGLE_S8_TLV("TX_DEC4 Volume",
			  BOLERO_CDC_TX4_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("TX_DEC5 Volume",
			  BOLERO_CDC_TX5_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("TX_DEC6 Volume",
			  BOLERO_CDC_TX6_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("TX_DEC7 Volume",
			  BOLERO_CDC_TX7_TX_VOL_CTL,
			  -84, 40, digital_gain),

	SOC_ENUM_EXT("DEC4 MODE", dec_mode_mux_enum,
			tx_macro_dec_mode_get, tx_macro_dec_mode_put),

	SOC_ENUM_EXT("DEC5 MODE", dec_mode_mux_enum,
			tx_macro_dec_mode_get, tx_macro_dec_mode_put),

	SOC_ENUM_EXT("DEC6 MODE", dec_mode_mux_enum,
			tx_macro_dec_mode_get, tx_macro_dec_mode_put),

	SOC_ENUM_EXT("DEC7 MODE", dec_mode_mux_enum,
			tx_macro_dec_mode_get, tx_macro_dec_mode_put),
};

static const struct snd_kcontrol_new tx_macro_snd_controls[] = {
	SOC_SINGLE_S8_TLV("TX_DEC0 Volume",
			  BOLERO_CDC_TX0_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("TX_DEC1 Volume",
			  BOLERO_CDC_TX1_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("TX_DEC2 Volume",
			  BOLERO_CDC_TX2_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("TX_DEC3 Volume",
			  BOLERO_CDC_TX3_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("TX_DEC4 Volume",
			  BOLERO_CDC_TX4_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("TX_DEC5 Volume",
			  BOLERO_CDC_TX5_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("TX_DEC6 Volume",
			  BOLERO_CDC_TX6_TX_VOL_CTL,
			  -84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("TX_DEC7 Volume",
			  BOLERO_CDC_TX7_TX_VOL_CTL,
			  -84, 40, digital_gain),

	SOC_ENUM_EXT("DEC0 MODE", dec_mode_mux_enum,
			tx_macro_dec_mode_get, tx_macro_dec_mode_put),

	SOC_ENUM_EXT("DEC1 MODE", dec_mode_mux_enum,
			tx_macro_dec_mode_get, tx_macro_dec_mode_put),

	SOC_ENUM_EXT("DEC2 MODE", dec_mode_mux_enum,
			tx_macro_dec_mode_get, tx_macro_dec_mode_put),

	SOC_ENUM_EXT("DEC3 MODE", dec_mode_mux_enum,
			tx_macro_dec_mode_get, tx_macro_dec_mode_put),

	SOC_ENUM_EXT("DEC4 MODE", dec_mode_mux_enum,
			tx_macro_dec_mode_get, tx_macro_dec_mode_put),

	SOC_ENUM_EXT("DEC5 MODE", dec_mode_mux_enum,
			tx_macro_dec_mode_get, tx_macro_dec_mode_put),

	SOC_ENUM_EXT("DEC6 MODE", dec_mode_mux_enum,
			tx_macro_dec_mode_get, tx_macro_dec_mode_put),

	SOC_ENUM_EXT("DEC7 MODE", dec_mode_mux_enum,
			tx_macro_dec_mode_get, tx_macro_dec_mode_put),
	SOC_ENUM("TX0 HPF cut off", cf_dec0_enum),

	SOC_ENUM("TX1 HPF cut off", cf_dec1_enum),

	SOC_ENUM("TX2 HPF cut off", cf_dec2_enum),

	SOC_ENUM("TX3 HPF cut off", cf_dec3_enum),

	SOC_ENUM("TX4 HPF cut off", cf_dec4_enum),

	SOC_ENUM("TX5 HPF cut off", cf_dec5_enum),

	SOC_ENUM("TX6 HPF cut off", cf_dec6_enum),

	SOC_ENUM("TX7 HPF cut off", cf_dec7_enum),

	SOC_SINGLE_EXT("DEC0_BCS Switch", SND_SOC_NOPM, 0, 1, 0,
		       tx_macro_get_bcs, tx_macro_set_bcs),
};

static int tx_macro_register_event_listener(struct snd_soc_component *component,
					    bool enable)
{
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;
	int ret = 0;

	if (!component)
		return -EINVAL;

	tx_dev = bolero_get_device_ptr(component->dev, TX_MACRO);
	if (!tx_dev) {
		dev_err(component->dev,
			"%s: null device for macro!\n", __func__);
		return -EINVAL;
	}
	tx_priv = dev_get_drvdata(tx_dev);
	if (!tx_priv) {
		dev_err(component->dev,
			"%s: priv is null for macro!\n", __func__);
		return -EINVAL;
	}
	if (tx_priv->swr_ctrl_data &&
		(!tx_priv->tx_swr_clk_cnt || !tx_priv->va_swr_clk_cnt)) {
		if (enable) {
			if (!tx_priv->disable_afe_wakeup_event_listener)
				ret = swrm_wcd_notify(
					tx_priv->swr_ctrl_data[0].tx_swr_pdev,
					SWR_REGISTER_WAKEUP, NULL);
		} else {
			if (!tx_priv->disable_afe_wakeup_event_listener)
				ret = swrm_wcd_notify(
					tx_priv->swr_ctrl_data[0].tx_swr_pdev,
					SWR_DEREGISTER_WAKEUP, NULL);
		}
	}

	return ret;
}

static int tx_macro_tx_va_mclk_enable(struct tx_macro_priv *tx_priv,
				      struct regmap *regmap, int clk_type,
				      bool enable)
{
	int ret = 0, clk_tx_ret = 0;

	trace_printk("%s: clock type %s, enable: %s tx_mclk_users: %d\n",
		__func__, (clk_type ? "VA_MCLK" : "TX_MCLK"),
		(enable ? "enable" : "disable"), tx_priv->tx_mclk_users);
	dev_dbg(tx_priv->dev,
		"%s: clock type %s, enable: %s tx_mclk_users: %d\n",
		__func__, (clk_type ? "VA_MCLK" : "TX_MCLK"),
		(enable ? "enable" : "disable"), tx_priv->tx_mclk_users);

	if (enable) {
		if (tx_priv->swr_clk_users == 0) {
			trace_printk("%s: tx swr clk users 0\n", __func__);
			ret = msm_cdc_pinctrl_select_active_state(
						tx_priv->tx_swr_gpio_p);
			if (ret < 0) {
				dev_err_ratelimited(tx_priv->dev,
					"%s: tx swr pinctrl enable failed\n",
					__func__);
				goto exit;
			}
			msm_cdc_pinctrl_set_wakeup_capable(
					tx_priv->tx_swr_gpio_p, false);
		}

		clk_tx_ret = bolero_clk_rsc_request_clock(tx_priv->dev,
						   TX_CORE_CLK,
						   TX_CORE_CLK,
						   true);
		if (clk_type == TX_MCLK) {
			trace_printk("%s: requesting TX_MCLK\n", __func__);
			ret = tx_macro_mclk_enable(tx_priv, 1);
			if (ret < 0) {
				if (tx_priv->swr_clk_users == 0)
					msm_cdc_pinctrl_select_sleep_state(
							tx_priv->tx_swr_gpio_p);
				dev_err_ratelimited(tx_priv->dev,
					"%s: request clock enable failed\n",
					__func__);
				goto done;
			}
		}
		if (clk_type == VA_MCLK) {
			trace_printk("%s: requesting VA_MCLK\n", __func__);
			ret = bolero_clk_rsc_request_clock(tx_priv->dev,
							   TX_CORE_CLK,
							   VA_CORE_CLK,
							   true);
			if (ret < 0) {
				if (tx_priv->swr_clk_users == 0)
					msm_cdc_pinctrl_select_sleep_state(
							tx_priv->tx_swr_gpio_p);
				dev_err_ratelimited(tx_priv->dev,
					"%s: swr request clk failed\n",
					__func__);
				goto done;
			}
			bolero_clk_rsc_fs_gen_request(tx_priv->dev,
						  true);
			mutex_lock(&tx_priv->mclk_lock);
			if (tx_priv->tx_mclk_users == 0) {
				regmap_update_bits(regmap,
					BOLERO_CDC_TX_TOP_CSR_FREQ_MCLK,
					0x01, 0x01);
				regmap_update_bits(regmap,
					BOLERO_CDC_TX_CLK_RST_CTRL_MCLK_CONTROL,
					0x01, 0x01);
				regmap_update_bits(regmap,
					BOLERO_CDC_TX_CLK_RST_CTRL_FS_CNT_CONTROL,
					0x01, 0x01);
			}
			tx_priv->tx_mclk_users++;
			mutex_unlock(&tx_priv->mclk_lock);
		}
		if (tx_priv->swr_clk_users == 0) {
			dev_dbg(tx_priv->dev, "%s: reset_swr: %d\n",
				__func__, tx_priv->reset_swr);
			trace_printk("%s: reset_swr: %d\n",
				__func__, tx_priv->reset_swr);
			if (tx_priv->reset_swr)
				regmap_update_bits(regmap,
					BOLERO_CDC_TX_CLK_RST_CTRL_SWR_CONTROL,
					0x02, 0x02);
			regmap_update_bits(regmap,
				BOLERO_CDC_TX_CLK_RST_CTRL_SWR_CONTROL,
				0x01, 0x01);
			if (tx_priv->reset_swr)
				regmap_update_bits(regmap,
					BOLERO_CDC_TX_CLK_RST_CTRL_SWR_CONTROL,
					0x02, 0x00);
			tx_priv->reset_swr = false;
		}
		if (!clk_tx_ret)
			ret = bolero_clk_rsc_request_clock(tx_priv->dev,
						   TX_CORE_CLK,
						   TX_CORE_CLK,
						   false);
		tx_priv->swr_clk_users++;
	} else {
		if (tx_priv->swr_clk_users <= 0) {
			dev_err_ratelimited(tx_priv->dev,
				"tx swrm clock users already 0\n");
			tx_priv->swr_clk_users = 0;
			return 0;
		}
		clk_tx_ret = bolero_clk_rsc_request_clock(tx_priv->dev,
						   TX_CORE_CLK,
						   TX_CORE_CLK,
						   true);
		tx_priv->swr_clk_users--;
		if (tx_priv->swr_clk_users == 0)
			regmap_update_bits(regmap,
				BOLERO_CDC_TX_CLK_RST_CTRL_SWR_CONTROL,
				0x01, 0x00);
		if (clk_type == TX_MCLK)
			tx_macro_mclk_enable(tx_priv, 0);
		if (clk_type == VA_MCLK) {
			mutex_lock(&tx_priv->mclk_lock);
			if (tx_priv->tx_mclk_users <= 0) {
				dev_err(tx_priv->dev, "%s: clock already disabled\n",
						__func__);
				tx_priv->tx_mclk_users = 0;
				mutex_unlock(&tx_priv->mclk_lock);
				goto tx_clk;
			}
			tx_priv->tx_mclk_users--;
			if (tx_priv->tx_mclk_users == 0) {
				regmap_update_bits(regmap,
					BOLERO_CDC_TX_CLK_RST_CTRL_FS_CNT_CONTROL,
					0x01, 0x00);
				regmap_update_bits(regmap,
					BOLERO_CDC_TX_CLK_RST_CTRL_MCLK_CONTROL,
					0x01, 0x00);
			}
			mutex_unlock(&tx_priv->mclk_lock);
			bolero_clk_rsc_fs_gen_request(tx_priv->dev,
						false);
			ret = bolero_clk_rsc_request_clock(tx_priv->dev,
							   TX_CORE_CLK,
							   VA_CORE_CLK,
							   false);
			if (ret < 0) {
				dev_err_ratelimited(tx_priv->dev,
					"%s: swr request clk failed\n",
					__func__);
				goto done;
			}
		}
tx_clk:
		if (!clk_tx_ret)
			ret = bolero_clk_rsc_request_clock(tx_priv->dev,
						   TX_CORE_CLK,
						   TX_CORE_CLK,
						   false);
		if (tx_priv->swr_clk_users == 0) {
			msm_cdc_pinctrl_set_wakeup_capable(
					tx_priv->tx_swr_gpio_p, true);
			ret = msm_cdc_pinctrl_select_sleep_state(
						tx_priv->tx_swr_gpio_p);
			if (ret < 0) {
				dev_err_ratelimited(tx_priv->dev,
					"%s: tx swr pinctrl disable failed\n",
					__func__);
				goto exit;
			}
		}
	}
	return 0;

done:
	if (!clk_tx_ret)
		bolero_clk_rsc_request_clock(tx_priv->dev,
				TX_CORE_CLK,
				TX_CORE_CLK,
				false);
exit:
	trace_printk("%s: exit\n", __func__);
	return ret;
}

static int tx_macro_clk_div_get(struct snd_soc_component *component)
{
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	return tx_priv->dmic_clk_div;
}

static int tx_macro_clk_switch(struct snd_soc_component *component, int clk_src)
{
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;
	int ret = 0;

	if (!component)
		return -EINVAL;

	tx_dev = bolero_get_device_ptr(component->dev, TX_MACRO);
	if (!tx_dev) {
		dev_err(component->dev,
			"%s: null device for macro!\n", __func__);
		return -EINVAL;
	}
	tx_priv = dev_get_drvdata(tx_dev);
	if (!tx_priv) {
		dev_err(component->dev,
			"%s: priv is null for macro!\n", __func__);
		return -EINVAL;
	}
	dev_dbg(component->dev,
		"%s: va_swr_clk_cnt %d, tx_swr_clk_cnt %d, tx_clk_status %d\n",
		__func__, tx_priv->va_swr_clk_cnt,
		tx_priv->tx_swr_clk_cnt, tx_priv->tx_clk_status);
	if (tx_priv->current_clk_id == clk_src) {
		dev_dbg(component->dev,
			"%s: requested clk %d is same as current\n",
			__func__, clk_src);
		return 0;
	} else if (tx_priv->va_swr_clk_cnt != 0 && tx_priv->tx_clk_status) {
		ret = bolero_clk_rsc_request_clock(tx_priv->dev,
				TX_CORE_CLK,
				clk_src,
				true);
		if (ret) {
			dev_dbg(component->dev,
				"%s: request clock %d enable failed\n",
				__func__, clk_src);
			goto ret;
		}
		ret = bolero_clk_rsc_request_clock(tx_priv->dev,
				TX_CORE_CLK,
				tx_priv->current_clk_id,
				false);
		if (ret) {
			dev_dbg(component->dev,
				"%s: request clock  disable failed\n",
				__func__);
			bolero_clk_rsc_request_clock(tx_priv->dev,
				TX_CORE_CLK,
				clk_src,
				false);
			goto ret;
		}
		tx_priv->current_clk_id = clk_src;
	} else {
		ret = -EBUSY;
	}

ret:
	return ret;
}

static int tx_macro_core_vote(void *handle, bool enable)
{
	int rc = 0;
	struct tx_macro_priv *tx_priv = (struct tx_macro_priv *) handle;

	if (tx_priv == NULL) {
		pr_err("%s: tx priv data is NULL\n", __func__);
		return -EINVAL;
	}

	if (enable) {
		pm_runtime_get_sync(tx_priv->dev);
		if (bolero_check_core_votes(tx_priv->dev))
			rc = 0;
		else
			rc = -ENOTSYNC;
	} else {
		pm_runtime_put_autosuspend(tx_priv->dev);
		pm_runtime_mark_last_busy(tx_priv->dev);
	}
	return rc;
}

static int tx_macro_swrm_clock(void *handle, bool enable)
{
	struct tx_macro_priv *tx_priv = (struct tx_macro_priv *) handle;
	struct regmap *regmap = dev_get_regmap(tx_priv->dev->parent, NULL);
	int ret = 0;

	if (regmap == NULL) {
		dev_err(tx_priv->dev, "%s: regmap is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&tx_priv->swr_clk_lock);
	trace_printk("%s: swrm clock %s tx_swr_clk_cnt: %d va_swr_clk_cnt: %d\n",
		__func__,
		(enable ? "enable" : "disable"),
		tx_priv->tx_swr_clk_cnt, tx_priv->va_swr_clk_cnt);
	dev_dbg(tx_priv->dev,
		"%s: swrm clock %s tx_swr_clk_cnt: %d va_swr_clk_cnt: %d\n",
		__func__, (enable ? "enable" : "disable"),
		tx_priv->tx_swr_clk_cnt, tx_priv->va_swr_clk_cnt);

	if (enable) {
		pm_runtime_get_sync(tx_priv->dev);
		if (tx_priv->va_swr_clk_cnt && !tx_priv->tx_swr_clk_cnt) {
			ret = tx_macro_tx_va_mclk_enable(tx_priv, regmap,
							VA_MCLK, enable);
			if (ret) {
				pm_runtime_mark_last_busy(tx_priv->dev);
				pm_runtime_put_autosuspend(tx_priv->dev);
				goto done;
			}
			tx_priv->va_clk_status++;
		} else {
			ret = tx_macro_tx_va_mclk_enable(tx_priv, regmap,
							TX_MCLK, enable);
			if (ret) {
				pm_runtime_mark_last_busy(tx_priv->dev);
				pm_runtime_put_autosuspend(tx_priv->dev);
				goto done;
			}
			tx_priv->tx_clk_status++;
		}
		pm_runtime_mark_last_busy(tx_priv->dev);
		pm_runtime_put_autosuspend(tx_priv->dev);
	} else {
		if (tx_priv->va_clk_status && !tx_priv->tx_clk_status) {
			ret = tx_macro_tx_va_mclk_enable(tx_priv, regmap,
							VA_MCLK, enable);
			if (ret)
				goto done;
			--tx_priv->va_clk_status;
		} else if (!tx_priv->va_clk_status && tx_priv->tx_clk_status) {
			ret = tx_macro_tx_va_mclk_enable(tx_priv, regmap,
							TX_MCLK, enable);
			if (ret)
				goto done;
			--tx_priv->tx_clk_status;
		} else if (tx_priv->va_clk_status && tx_priv->tx_clk_status) {
			if (!tx_priv->va_swr_clk_cnt && tx_priv->tx_swr_clk_cnt) {
				ret = tx_macro_tx_va_mclk_enable(tx_priv, regmap,
								VA_MCLK, enable);
				if (ret)
					goto done;
				--tx_priv->va_clk_status;
			} else {
				ret = tx_macro_tx_va_mclk_enable(tx_priv, regmap,
								TX_MCLK, enable);
				if (ret)
					goto done;
				--tx_priv->tx_clk_status;
			}

		} else {
			dev_dbg(tx_priv->dev,
				"%s: Both clocks are disabled\n", __func__);
		}
	}

	trace_printk("%s: swrm clock users %d tx_clk_sts_cnt: %d va_clk_sts_cnt: %d\n",
		__func__, tx_priv->swr_clk_users, tx_priv->tx_clk_status,
                tx_priv->va_clk_status);
	dev_dbg(tx_priv->dev,
		"%s: swrm clock users %d tx_clk_sts_cnt: %d va_clk_sts_cnt: %d\n",
		__func__, tx_priv->swr_clk_users, tx_priv->tx_clk_status,
		tx_priv->va_clk_status);
done:
	mutex_unlock(&tx_priv->swr_clk_lock);
	return ret;
}

static int tx_macro_validate_dmic_sample_rate(u32 dmic_sample_rate,
				      struct tx_macro_priv *tx_priv)
{
	u32 div_factor = TX_MACRO_CLK_DIV_2;
	u32 mclk_rate = TX_MACRO_MCLK_FREQ;

	if (dmic_sample_rate == TX_MACRO_DMIC_SAMPLE_RATE_UNDEFINED ||
	    mclk_rate % dmic_sample_rate != 0)
		goto undefined_rate;

	div_factor = mclk_rate / dmic_sample_rate;

	switch (div_factor) {
	case 2:
		tx_priv->dmic_clk_div = TX_MACRO_CLK_DIV_2;
		break;
	case 3:
		tx_priv->dmic_clk_div = TX_MACRO_CLK_DIV_3;
		break;
	case 4:
		tx_priv->dmic_clk_div = TX_MACRO_CLK_DIV_4;
		break;
	case 6:
		tx_priv->dmic_clk_div = TX_MACRO_CLK_DIV_6;
		break;
	case 8:
		tx_priv->dmic_clk_div = TX_MACRO_CLK_DIV_8;
		break;
	case 16:
		tx_priv->dmic_clk_div = TX_MACRO_CLK_DIV_16;
		break;
	default:
		/* Any other DIV factor is invalid */
		goto undefined_rate;
	}

	/* Valid dmic DIV factors */
	dev_dbg(tx_priv->dev, "%s: DMIC_DIV = %u, mclk_rate = %u\n",
		__func__, div_factor, mclk_rate);

	return dmic_sample_rate;

undefined_rate:
	dev_dbg(tx_priv->dev, "%s: Invalid rate %d, for mclk %d\n",
		 __func__, dmic_sample_rate, mclk_rate);
	dmic_sample_rate = TX_MACRO_DMIC_SAMPLE_RATE_UNDEFINED;

	return dmic_sample_rate;
}

static const struct tx_macro_reg_mask_val tx_macro_reg_init[] = {
	{BOLERO_CDC_TX0_TX_PATH_SEC7, 0x3F, 0x0A},
};

static int tx_macro_init(struct snd_soc_component *component)
{
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(component);
	int ret = 0, i = 0;
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;

	tx_dev = bolero_get_device_ptr(component->dev, TX_MACRO);
	if (!tx_dev) {
		dev_err(component->dev,
			"%s: null device for macro!\n", __func__);
		return -EINVAL;
	}
	tx_priv = dev_get_drvdata(tx_dev);
	if (!tx_priv) {
		dev_err(component->dev,
			"%s: priv is null for macro!\n", __func__);
		return -EINVAL;
	}
	tx_priv->lpi_enable = false;
	tx_priv->register_event_listener = false;
	tx_priv->version = bolero_get_version(tx_dev);
	if (tx_priv->version >= BOLERO_VERSION_2_0) {
		ret = snd_soc_dapm_new_controls(dapm,
				tx_macro_dapm_widgets_common,
				ARRAY_SIZE(tx_macro_dapm_widgets_common));
		if (ret < 0) {
			dev_err(tx_dev, "%s: Failed to add controls\n",
				__func__);
			return ret;
		}
		if (tx_priv->version == BOLERO_VERSION_2_1)
			ret = snd_soc_dapm_new_controls(dapm,
				tx_macro_dapm_widgets_v2,
				ARRAY_SIZE(tx_macro_dapm_widgets_v2));
		else if (tx_priv->version == BOLERO_VERSION_2_0)
			ret = snd_soc_dapm_new_controls(dapm,
				tx_macro_dapm_widgets_v3,
				ARRAY_SIZE(tx_macro_dapm_widgets_v3));
		if (ret < 0) {
			dev_err(tx_dev, "%s: Failed to add controls\n",
				__func__);
			return ret;
		}
	} else {
		ret = snd_soc_dapm_new_controls(dapm, tx_macro_dapm_widgets,
					ARRAY_SIZE(tx_macro_dapm_widgets));
		if (ret < 0) {
			dev_err(tx_dev, "%s: Failed to add controls\n",
				__func__);
			return ret;
		}
	}

	if (tx_priv->version >= BOLERO_VERSION_2_0) {
		ret = snd_soc_dapm_add_routes(dapm,
					tx_audio_map_common,
					ARRAY_SIZE(tx_audio_map_common));
		if (ret < 0) {
			dev_err(tx_dev, "%s: Failed to add routes\n",
				__func__);
			return ret;
		}
		if (tx_priv->version == BOLERO_VERSION_2_0)
			ret = snd_soc_dapm_add_routes(dapm,
					tx_audio_map_v3,
					ARRAY_SIZE(tx_audio_map_v3));
		if (ret < 0) {
			dev_err(tx_dev, "%s: Failed to add routes\n",
				__func__);
			return ret;
		}
	} else {
		ret = snd_soc_dapm_add_routes(dapm, tx_audio_map,
					ARRAY_SIZE(tx_audio_map));
		if (ret < 0) {
			dev_err(tx_dev, "%s: Failed to add routes\n",
				__func__);
			return ret;
		}
	}

	ret = snd_soc_dapm_new_widgets(dapm->card);
	if (ret < 0) {
		dev_err(tx_dev, "%s: Failed to add widgets\n", __func__);
		return ret;
	}

	if (tx_priv->version >= BOLERO_VERSION_2_0) {
		ret = snd_soc_add_component_controls(component,
			tx_macro_snd_controls_common,
			ARRAY_SIZE(tx_macro_snd_controls_common));
		if (ret < 0) {
			dev_err(tx_dev, "%s: Failed to add snd_ctls\n",
				__func__);
			return ret;
		}
		if (tx_priv->version == BOLERO_VERSION_2_0)
			ret = snd_soc_add_component_controls(component,
				tx_macro_snd_controls_v3,
				ARRAY_SIZE(tx_macro_snd_controls_v3));
		if (ret < 0) {
			dev_err(tx_dev, "%s: Failed to add snd_ctls\n",
				__func__);
			return ret;
		}
	} else {
		ret = snd_soc_add_component_controls(component,
				tx_macro_snd_controls,
				ARRAY_SIZE(tx_macro_snd_controls));
		if (ret < 0) {
			dev_err(tx_dev, "%s: Failed to add snd_ctls\n",
				__func__);
			return ret;
		}
	}

	snd_soc_dapm_ignore_suspend(dapm, "TX_AIF1 Capture");
	snd_soc_dapm_ignore_suspend(dapm, "TX_AIF2 Capture");
	snd_soc_dapm_ignore_suspend(dapm, "TX_AIF3 Capture");
	if (tx_priv->version >= BOLERO_VERSION_2_0) {
		snd_soc_dapm_ignore_suspend(dapm, "TX SWR_INPUT");
	} else {
		snd_soc_dapm_ignore_suspend(dapm, "TX SWR_ADC0");
		snd_soc_dapm_ignore_suspend(dapm, "TX SWR_ADC1");
		snd_soc_dapm_ignore_suspend(dapm, "TX SWR_ADC2");
		snd_soc_dapm_ignore_suspend(dapm, "TX SWR_ADC3");
		snd_soc_dapm_ignore_suspend(dapm, "TX SWR_DMIC0");
		snd_soc_dapm_ignore_suspend(dapm, "TX SWR_DMIC1");
		snd_soc_dapm_ignore_suspend(dapm, "TX SWR_DMIC2");
		snd_soc_dapm_ignore_suspend(dapm, "TX SWR_DMIC3");
		snd_soc_dapm_ignore_suspend(dapm, "TX SWR_DMIC4");
		snd_soc_dapm_ignore_suspend(dapm, "TX SWR_DMIC5");
		snd_soc_dapm_ignore_suspend(dapm, "TX SWR_DMIC6");
		snd_soc_dapm_ignore_suspend(dapm, "TX SWR_DMIC7");
	}
	snd_soc_dapm_sync(dapm);

	for (i = 0; i < NUM_DECIMATORS; i++) {
		tx_priv->tx_hpf_work[i].tx_priv = tx_priv;
		tx_priv->tx_hpf_work[i].decimator = i;
		INIT_DELAYED_WORK(&tx_priv->tx_hpf_work[i].dwork,
			tx_macro_tx_hpf_corner_freq_callback);
	}

	for (i = 0; i < NUM_DECIMATORS; i++) {
		tx_priv->tx_mute_dwork[i].tx_priv = tx_priv;
		tx_priv->tx_mute_dwork[i].decimator = i;
		INIT_DELAYED_WORK(&tx_priv->tx_mute_dwork[i].dwork,
			  tx_macro_mute_update_callback);
	}
	tx_priv->component = component;

	for (i = 0; i < ARRAY_SIZE(tx_macro_reg_init); i++)
		snd_soc_component_update_bits(component,
				tx_macro_reg_init[i].reg,
				tx_macro_reg_init[i].mask,
				tx_macro_reg_init[i].val);

	return 0;
}

static int tx_macro_deinit(struct snd_soc_component *component)
{
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	tx_priv->component = NULL;
	return 0;
}

static void tx_macro_add_child_devices(struct work_struct *work)
{
	struct tx_macro_priv *tx_priv = NULL;
	struct platform_device *pdev = NULL;
	struct device_node *node = NULL;
	struct tx_macro_swr_ctrl_data *swr_ctrl_data = NULL, *temp = NULL;
	int ret = 0;
	u16 count = 0, ctrl_num = 0;
	struct tx_macro_swr_ctrl_platform_data *platdata = NULL;
	char plat_dev_name[TX_MACRO_SWR_STRING_LEN] = "";
	bool tx_swr_master_node = false;

	tx_priv = container_of(work, struct tx_macro_priv,
			     tx_macro_add_child_devices_work);
	if (!tx_priv) {
		pr_err("%s: Memory for tx_priv does not exist\n",
			__func__);
		return;
	}

	if (!tx_priv->dev) {
		pr_err("%s: tx dev does not exist\n", __func__);
		return;
	}

	if (!tx_priv->dev->of_node) {
		dev_err(tx_priv->dev,
			"%s: DT node for tx_priv does not exist\n", __func__);
		return;
	}

	platdata = &tx_priv->swr_plat_data;
	tx_priv->child_count = 0;

	for_each_available_child_of_node(tx_priv->dev->of_node, node) {
		tx_swr_master_node = false;
		if (strnstr(node->name, "tx_swr_master",
                                strlen("tx_swr_master")) != NULL)
			tx_swr_master_node = true;

		if (tx_swr_master_node)
			strlcpy(plat_dev_name, "tx_swr_ctrl",
				(TX_MACRO_SWR_STRING_LEN - 1));
		else
			strlcpy(plat_dev_name, node->name,
				(TX_MACRO_SWR_STRING_LEN - 1));

		pdev = platform_device_alloc(plat_dev_name, -1);
		if (!pdev) {
			dev_err(tx_priv->dev, "%s: pdev memory alloc failed\n",
				__func__);
			ret = -ENOMEM;
			goto err;
		}
		pdev->dev.parent = tx_priv->dev;
		pdev->dev.of_node = node;

		if (tx_swr_master_node) {
			ret = platform_device_add_data(pdev, platdata,
						       sizeof(*platdata));
			if (ret) {
				dev_err(&pdev->dev,
					"%s: cannot add plat data ctrl:%d\n",
					__func__, ctrl_num);
				goto fail_pdev_add;
			}

			temp = krealloc(swr_ctrl_data,
					(ctrl_num + 1) * sizeof(
					struct tx_macro_swr_ctrl_data),
					GFP_KERNEL);
			if (!temp) {
				ret = -ENOMEM;
				goto fail_pdev_add;
			}
			swr_ctrl_data = temp;
			swr_ctrl_data[ctrl_num].tx_swr_pdev = pdev;
			ctrl_num++;
			dev_dbg(&pdev->dev,
				"%s: Adding soundwire ctrl device(s)\n",
				__func__);
			tx_priv->swr_ctrl_data = swr_ctrl_data;
		}

		ret = platform_device_add(pdev);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: Cannot add platform device\n",
				__func__);
			goto fail_pdev_add;
		}

		if (tx_priv->child_count < TX_MACRO_CHILD_DEVICES_MAX)
			tx_priv->pdev_child_devices[
					tx_priv->child_count++] = pdev;
		else
			goto err;
	}
	return;
fail_pdev_add:
	for (count = 0; count < tx_priv->child_count; count++)
		platform_device_put(tx_priv->pdev_child_devices[count]);
err:
	return;
}

static int tx_macro_set_port_map(struct snd_soc_component *component,
				u32 usecase, u32 size, void *data)
{
	struct device *tx_dev = NULL;
	struct tx_macro_priv *tx_priv = NULL;
	struct swrm_port_config port_cfg;
	int ret = 0;

	if (!tx_macro_get_data(component, &tx_dev, &tx_priv, __func__))
		return -EINVAL;

	memset(&port_cfg, 0, sizeof(port_cfg));
	port_cfg.uc = usecase;
	port_cfg.size = size;
	port_cfg.params = data;

	if (tx_priv->swr_ctrl_data)
		ret = swrm_wcd_notify(
			tx_priv->swr_ctrl_data[0].tx_swr_pdev,
			SWR_SET_PORT_MAP, &port_cfg);

	return ret;
}

static void tx_macro_init_ops(struct macro_ops *ops,
			       char __iomem *tx_io_base)
{
	memset(ops, 0, sizeof(struct macro_ops));
	ops->init = tx_macro_init;
	ops->exit = tx_macro_deinit;
	ops->io_base = tx_io_base;
	ops->dai_ptr = tx_macro_dai;
	ops->num_dais = ARRAY_SIZE(tx_macro_dai);
	ops->event_handler = tx_macro_event_handler;
	ops->reg_wake_irq = tx_macro_reg_wake_irq;
	ops->set_port_map = tx_macro_set_port_map;
	ops->clk_div_get = tx_macro_clk_div_get;
	ops->clk_switch = tx_macro_clk_switch;
	ops->reg_evt_listener = tx_macro_register_event_listener;
	ops->clk_enable = __tx_macro_mclk_enable;
}

static int tx_macro_probe(struct platform_device *pdev)
{
	struct macro_ops ops = {0};
	struct tx_macro_priv *tx_priv = NULL;
	u32 tx_base_addr = 0, sample_rate = 0;
	char __iomem *tx_io_base = NULL;
	int ret = 0;
	const char *dmic_sample_rate = "qcom,tx-dmic-sample-rate";
	u32 is_used_tx_swr_gpio = 1;
	const char *is_used_tx_swr_gpio_dt = "qcom,is-used-swr-gpio";
	u32 disable_afe_wakeup_event_listener = 0;
	const char *disable_afe_wakeup_event_listener_dt =
			"qcom,disable-afe-wakeup-event-listener";

	if (!bolero_is_va_macro_registered(&pdev->dev)) {
		dev_err(&pdev->dev,
			"%s: va-macro not registered yet, defer\n", __func__);
		return -EPROBE_DEFER;
	}

	tx_priv = devm_kzalloc(&pdev->dev, sizeof(struct tx_macro_priv),
			    GFP_KERNEL);
	if (!tx_priv)
		return -ENOMEM;
	platform_set_drvdata(pdev, tx_priv);

	tx_priv->dev = &pdev->dev;
	ret = of_property_read_u32(pdev->dev.of_node, "reg",
				   &tx_base_addr);
	if (ret) {
		dev_err(&pdev->dev, "%s: could not find %s entry in dt\n",
			__func__, "reg");
		return ret;
	}
	dev_set_drvdata(&pdev->dev, tx_priv);
	if (of_find_property(pdev->dev.of_node, is_used_tx_swr_gpio_dt,
			     NULL)) {
		ret = of_property_read_u32(pdev->dev.of_node,
					   is_used_tx_swr_gpio_dt,
					   &is_used_tx_swr_gpio);
		if (ret) {
			dev_err(&pdev->dev, "%s: error reading %s in dt\n",
				__func__, is_used_tx_swr_gpio_dt);
			is_used_tx_swr_gpio = 1;
		}
	}
	tx_priv->tx_swr_gpio_p = of_parse_phandle(pdev->dev.of_node,
					"qcom,tx-swr-gpios", 0);
	if (!tx_priv->tx_swr_gpio_p && is_used_tx_swr_gpio) {
		dev_err(&pdev->dev, "%s: swr_gpios handle not provided!\n",
			__func__);
		return -EINVAL;
	}
	if (msm_cdc_pinctrl_get_state(tx_priv->tx_swr_gpio_p) < 0 &&
			is_used_tx_swr_gpio) {
		dev_err(&pdev->dev, "%s: failed to get swr pin state\n",
			__func__);
		return -EPROBE_DEFER;
	}

	tx_io_base = devm_ioremap(&pdev->dev,
				   tx_base_addr, TX_MACRO_MAX_OFFSET);
	if (!tx_io_base) {
		dev_err(&pdev->dev, "%s: ioremap failed\n", __func__);
		return -ENOMEM;
	}
	tx_priv->tx_io_base = tx_io_base;
	ret = of_property_read_u32(pdev->dev.of_node, dmic_sample_rate,
				   &sample_rate);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: could not find sample_rate entry in dt\n",
			__func__);
		tx_priv->dmic_clk_div = TX_MACRO_CLK_DIV_2;
	} else {
		if (tx_macro_validate_dmic_sample_rate(
		sample_rate, tx_priv) == TX_MACRO_DMIC_SAMPLE_RATE_UNDEFINED)
			return -EINVAL;
	}

	if (of_find_property(pdev->dev.of_node,
			     disable_afe_wakeup_event_listener_dt, NULL)) {
		ret = of_property_read_u32(pdev->dev.of_node,
					   disable_afe_wakeup_event_listener_dt,
					   &disable_afe_wakeup_event_listener);
		if (ret)
			dev_dbg(&pdev->dev, "%s: error reading %s in dt\n",
				__func__, disable_afe_wakeup_event_listener_dt);
	}
	tx_priv->disable_afe_wakeup_event_listener =
			disable_afe_wakeup_event_listener;

	if (is_used_tx_swr_gpio) {
		tx_priv->reset_swr = true;
		INIT_WORK(&tx_priv->tx_macro_add_child_devices_work,
			  tx_macro_add_child_devices);
		tx_priv->swr_plat_data.handle = (void *) tx_priv;
		tx_priv->swr_plat_data.read = NULL;
		tx_priv->swr_plat_data.write = NULL;
		tx_priv->swr_plat_data.bulk_write = NULL;
		tx_priv->swr_plat_data.clk = tx_macro_swrm_clock;
		tx_priv->swr_plat_data.core_vote = tx_macro_core_vote;
		tx_priv->swr_plat_data.handle_irq = NULL;
		mutex_init(&tx_priv->swr_clk_lock);
	}
	tx_priv->is_used_tx_swr_gpio = is_used_tx_swr_gpio;
	mutex_init(&tx_priv->mclk_lock);
	tx_macro_init_ops(&ops, tx_io_base);
	ops.clk_id_req = TX_CORE_CLK;
	ops.default_clk_id = TX_CORE_CLK;
	tx_priv->current_clk_id = TX_CORE_CLK;
	ret = bolero_register_macro(&pdev->dev, TX_MACRO, &ops);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: register macro failed\n", __func__);
		goto err_reg_macro;
	}
	pm_runtime_set_autosuspend_delay(&pdev->dev, AUTO_SUSPEND_DELAY);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, true);
	pm_runtime_enable(&pdev->dev);
	if (is_used_tx_swr_gpio)
		schedule_work(&tx_priv->tx_macro_add_child_devices_work);

	return 0;
err_reg_macro:
	mutex_destroy(&tx_priv->mclk_lock);
	if (is_used_tx_swr_gpio)
		mutex_destroy(&tx_priv->swr_clk_lock);
	return ret;
}

static int tx_macro_remove(struct platform_device *pdev)
{
	struct tx_macro_priv *tx_priv = NULL;
	u16 count = 0;

	tx_priv = platform_get_drvdata(pdev);

	if (!tx_priv)
		return -EINVAL;

	if (tx_priv->is_used_tx_swr_gpio) {
		if (tx_priv->swr_ctrl_data)
			kfree(tx_priv->swr_ctrl_data);
		for (count = 0; count < tx_priv->child_count &&
			count < TX_MACRO_CHILD_DEVICES_MAX; count++)
			platform_device_unregister(
				tx_priv->pdev_child_devices[count]);
	}

	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	mutex_destroy(&tx_priv->mclk_lock);
	if (tx_priv->is_used_tx_swr_gpio)
		mutex_destroy(&tx_priv->swr_clk_lock);
	bolero_unregister_macro(&pdev->dev, TX_MACRO);
	return 0;
}


static const struct of_device_id tx_macro_dt_match[] = {
	{.compatible = "qcom,tx-macro"},
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

static struct platform_driver tx_macro_driver = {
	.driver = {
		.name = "tx_macro",
		.owner = THIS_MODULE,
		.pm = &bolero_dev_pm_ops,
		.of_match_table = tx_macro_dt_match,
		.suppress_bind_attrs = true,
	},
	.probe = tx_macro_probe,
	.remove = tx_macro_remove,
};

module_platform_driver(tx_macro_driver);

MODULE_DESCRIPTION("TX macro driver");
MODULE_LICENSE("GPL v2");
