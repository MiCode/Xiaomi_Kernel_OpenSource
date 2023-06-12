// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/component.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <soc/soundwire.h>
#include <linux/regmap.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <asoc/wcdcal-hwdep.h>
#include <asoc/msm-cdc-pinctrl.h>
#include <asoc/msm-cdc-supply.h>
#include <dt-bindings/sound/audio-codec-port-types.h>

#include "wcd938x-registers.h"
#include "wcd938x.h"
#include "internal.h"
#include "asoc/bolero-slave-internal.h"

#define NUM_SWRS_DT_PARAMS 5
#define WCD938X_VARIANT_ENTRY_SIZE 32

#define WCD938X_VERSION_1_0 1
#define WCD938X_VERSION_ENTRY_SIZE 32
#define EAR_RX_PATH_AUX 1

#define ADC_MODE_VAL_HIFI     0x01
#define ADC_MODE_VAL_LO_HIF   0x02
#define ADC_MODE_VAL_NORMAL   0x03
#define ADC_MODE_VAL_LP       0x05
#define ADC_MODE_VAL_ULP1     0x09
#define ADC_MODE_VAL_ULP2     0x0B

#define NUM_ATTEMPTS 5

#define DAPM_MICBIAS1_STANDALONE "MIC BIAS1 Standalone"
#define DAPM_MICBIAS2_STANDALONE "MIC BIAS2 Standalone"
#define DAPM_MICBIAS3_STANDALONE "MIC BIAS3 Standalone"
#define DAPM_MICBIAS4_STANDALONE "MIC BIAS4 Standalone"

#define WCD938X_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000 |\
			SNDRV_PCM_RATE_384000)
/* Fractional Rates */
#define WCD938X_FRAC_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_88200 |\
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800)

#define WCD938X_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE |\
		SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

enum {
	CODEC_TX = 0,
	CODEC_RX,
};

enum {
	WCD_ADC1 = 0,
	WCD_ADC2,
	WCD_ADC3,
	WCD_ADC4,
	ALLOW_BUCK_DISABLE,
	HPH_COMP_DELAY,
	HPH_PA_DELAY,
	AMIC2_BCS_ENABLE,
	WCD_SUPPLIES_LPM_MODE,
	WCD_ADC1_MODE,
	WCD_ADC2_MODE,
	WCD_ADC3_MODE,
	WCD_ADC4_MODE,
};

enum {
	ADC_MODE_INVALID = 0,
	ADC_MODE_HIFI,
	ADC_MODE_LO_HIF,
	ADC_MODE_NORMAL,
	ADC_MODE_LP,
	ADC_MODE_ULP1,
	ADC_MODE_ULP2,
};

static u8 tx_mode_bit[] = {
	[ADC_MODE_INVALID] = 0x00,
	[ADC_MODE_HIFI] = 0x01,
	[ADC_MODE_LO_HIF] = 0x02,
	[ADC_MODE_NORMAL] = 0x04,
	[ADC_MODE_LP] = 0x08,
	[ADC_MODE_ULP1] = 0x10,
	[ADC_MODE_ULP2] = 0x20,
};

static const DECLARE_TLV_DB_SCALE(line_gain, 0, 7, 1);
static const DECLARE_TLV_DB_SCALE(analog_gain, 0, 25, 1);

static int wcd938x_handle_post_irq(void *data);
static int wcd938x_reset(struct device *dev);
static int wcd938x_reset_low(struct device *dev);
static int wcd938x_get_adc_mode(int val);

static const struct regmap_irq wcd938x_irqs[WCD938X_NUM_IRQS] = {
	REGMAP_IRQ_REG(WCD938X_IRQ_MBHC_BUTTON_PRESS_DET, 0, 0x01),
	REGMAP_IRQ_REG(WCD938X_IRQ_MBHC_BUTTON_RELEASE_DET, 0, 0x02),
	REGMAP_IRQ_REG(WCD938X_IRQ_MBHC_ELECT_INS_REM_DET, 0, 0x04),
	REGMAP_IRQ_REG(WCD938X_IRQ_MBHC_ELECT_INS_REM_LEG_DET, 0, 0x08),
	REGMAP_IRQ_REG(WCD938X_IRQ_MBHC_SW_DET, 0, 0x10),
	REGMAP_IRQ_REG(WCD938X_IRQ_HPHR_OCP_INT, 0, 0x20),
	REGMAP_IRQ_REG(WCD938X_IRQ_HPHR_CNP_INT, 0, 0x40),
	REGMAP_IRQ_REG(WCD938X_IRQ_HPHL_OCP_INT, 0, 0x80),
	REGMAP_IRQ_REG(WCD938X_IRQ_HPHL_CNP_INT, 1, 0x01),
	REGMAP_IRQ_REG(WCD938X_IRQ_EAR_CNP_INT, 1, 0x02),
	REGMAP_IRQ_REG(WCD938X_IRQ_EAR_SCD_INT, 1, 0x04),
	REGMAP_IRQ_REG(WCD938X_IRQ_AUX_CNP_INT, 1, 0x08),
	REGMAP_IRQ_REG(WCD938X_IRQ_AUX_SCD_INT, 1, 0x10),
	REGMAP_IRQ_REG(WCD938X_IRQ_HPHL_PDM_WD_INT, 1, 0x20),
	REGMAP_IRQ_REG(WCD938X_IRQ_HPHR_PDM_WD_INT, 1, 0x40),
	REGMAP_IRQ_REG(WCD938X_IRQ_AUX_PDM_WD_INT, 1, 0x80),
	REGMAP_IRQ_REG(WCD938X_IRQ_LDORT_SCD_INT, 2, 0x01),
	REGMAP_IRQ_REG(WCD938X_IRQ_MBHC_MOISTURE_INT, 2, 0x02),
	REGMAP_IRQ_REG(WCD938X_IRQ_HPHL_SURGE_DET_INT, 2, 0x04),
	REGMAP_IRQ_REG(WCD938X_IRQ_HPHR_SURGE_DET_INT, 2, 0x08),
};

static struct regmap_irq_chip wcd938x_regmap_irq_chip = {
	.name = "wcd938x",
	.irqs = wcd938x_irqs,
	.num_irqs = ARRAY_SIZE(wcd938x_irqs),
	.num_regs = 3,
	.status_base = WCD938X_DIGITAL_INTR_STATUS_0,
	.mask_base = WCD938X_DIGITAL_INTR_MASK_0,
	.type_base = WCD938X_DIGITAL_INTR_LEVEL_0,
	.ack_base = WCD938X_DIGITAL_INTR_CLEAR_0,
	.use_ack = 1,
	.runtime_pm = false,
	.handle_post_irq = wcd938x_handle_post_irq,
	.irq_drv_data = NULL,
};

static int wcd938x_handle_post_irq(void *data)
{
	struct wcd938x_priv *wcd938x = data;
	u32 sts1 = 0, sts2 = 0, sts3 = 0;

	regmap_read(wcd938x->regmap, WCD938X_DIGITAL_INTR_STATUS_0, &sts1);
	regmap_read(wcd938x->regmap, WCD938X_DIGITAL_INTR_STATUS_1, &sts2);
	regmap_read(wcd938x->regmap, WCD938X_DIGITAL_INTR_STATUS_2, &sts3);

	wcd938x->tx_swr_dev->slave_irq_pending =
			((sts1 || sts2 || sts3) ? true : false);

	return IRQ_HANDLED;
}

static int wcd938x_swr_slv_get_current_bank(struct swr_device *dev, u8 devnum)
{
	int ret = 0;
	int bank = 0;

	ret = swr_read(dev, devnum, SWR_SCP_CONTROL, &bank, 1);
	if (ret)
		return -EINVAL;

	return ((bank & 0x40) ? 1: 0);
}

static int wcd938x_get_clk_rate(int mode)
{
	int rate;

	switch (mode) {
	case ADC_MODE_ULP2:
		rate = SWR_CLK_RATE_0P6MHZ;
		break;
	case ADC_MODE_ULP1:
		rate = SWR_CLK_RATE_1P2MHZ;
		break;
	case ADC_MODE_LP:
		rate = SWR_CLK_RATE_4P8MHZ;
		break;
	case ADC_MODE_NORMAL:
	case ADC_MODE_LO_HIF:
	case ADC_MODE_HIFI:
	case ADC_MODE_INVALID:
	default:
		rate = SWR_CLK_RATE_9P6MHZ;
		break;
	}

	return rate;
}

static int wcd938x_set_swr_clk_rate(struct snd_soc_component *component,
					int rate, int bank)
{
	u8 mask = (bank ? 0xF0 : 0x0F);
	u8 val = 0;

	switch (rate) {
	case SWR_CLK_RATE_0P6MHZ:
		val = (bank ? 0x60 : 0x06);
		break;
	case SWR_CLK_RATE_1P2MHZ:
		val = (bank ? 0x50 : 0x05);
		break;
	case SWR_CLK_RATE_2P4MHZ:
		val = (bank ? 0x30 : 0x03);
		break;
	case SWR_CLK_RATE_4P8MHZ:
		val = (bank ? 0x10 : 0x01);
		break;
	case SWR_CLK_RATE_9P6MHZ:
	default:
		val = 0x00;
		break;
	}
	snd_soc_component_update_bits(component,
				      WCD938X_DIGITAL_SWR_TX_CLK_RATE,
				      mask, val);

	return 0;
}

static int wcd938x_init_reg(struct snd_soc_component *component)
{
	snd_soc_component_update_bits(component, WCD938X_SLEEP_CTL, 0x0E, 0x0E);
	snd_soc_component_update_bits(component, WCD938X_SLEEP_CTL, 0x80, 0x80);
	/* 1 msec delay as per HW requirement */
	usleep_range(1000, 1010);
	snd_soc_component_update_bits(component, WCD938X_SLEEP_CTL, 0x40, 0x40);
	/* 1 msec delay as per HW requirement */
	usleep_range(1000, 1010);
	snd_soc_component_update_bits(component, WCD938X_LDORXTX_CONFIG,
								0x10, 0x00);
	snd_soc_component_update_bits(component, WCD938X_BIAS_VBG_FINE_ADJ,
								0xF0, 0x80);
	snd_soc_component_update_bits(component, WCD938X_ANA_BIAS, 0x80, 0x80);
	snd_soc_component_update_bits(component, WCD938X_ANA_BIAS, 0x40, 0x40);
	/* 10 msec delay as per HW requirement */
	usleep_range(10000, 10010);
	snd_soc_component_update_bits(component, WCD938X_ANA_BIAS, 0x40, 0x00);
	snd_soc_component_update_bits(component,
				      WCD938X_HPH_NEW_INT_RDAC_GAIN_CTL,
				      0xF0, 0x00);
	snd_soc_component_update_bits(component,
				      WCD938X_HPH_NEW_INT_RDAC_HD2_CTL_L_NEW,
				      0x1F, 0x15);
	snd_soc_component_update_bits(component,
				      WCD938X_HPH_NEW_INT_RDAC_HD2_CTL_R_NEW,
				      0x1F, 0x15);
	snd_soc_component_update_bits(component, WCD938X_HPH_REFBUFF_UHQA_CTL,
				      0xC0, 0x80);
	snd_soc_component_update_bits(component, WCD938X_DIGITAL_CDC_DMIC_CTL,
				      0x02, 0x02);
	snd_soc_component_update_bits(component,
				WCD938X_TX_COM_NEW_INT_TXFE_ICTRL_STG2CASC_ULP,
				0xFF, 0x14);
	snd_soc_component_update_bits(component,
				WCD938X_TX_COM_NEW_INT_TXFE_ICTRL_STG2MAIN_ULP,
				0x1F, 0x08);
	snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_TX_REQ_FB_CTL_0, 0xFF, 0x55);
	snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_TX_REQ_FB_CTL_1, 0xFF, 0x44);
	snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_TX_REQ_FB_CTL_2, 0xFF, 0x11);
	snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_TX_REQ_FB_CTL_3, 0xFF, 0x00);
	snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_TX_REQ_FB_CTL_4, 0xFF, 0x00);
	snd_soc_component_update_bits(component,
				WCD938X_MICB1_TEST_CTL_1, 0xE0, 0xE0);
	snd_soc_component_update_bits(component,
				WCD938X_MICB2_TEST_CTL_1, 0xE0, 0xE0);
	snd_soc_component_update_bits(component,
				WCD938X_MICB3_TEST_CTL_1, 0xE0, 0xE0);
	snd_soc_component_update_bits(component,
				WCD938X_MICB4_TEST_CTL_1, 0xE0, 0xE0);
	snd_soc_component_update_bits(component,
				WCD938X_TX_3_4_TEST_BLK_EN2, 0x01, 0x00);
	snd_soc_component_update_bits(component, WCD938X_SLEEP_CTL, 0x0E,
				((snd_soc_component_read32(component,
				WCD938X_DIGITAL_EFUSE_REG_30) & 0x07) << 1));
	snd_soc_component_update_bits(component,
				WCD938X_HPH_SURGE_HPHLR_SURGE_EN, 0xC0, 0xC0);

	return 0;
}

static int wcd938x_set_port_params(struct snd_soc_component *component,
			u8 slv_prt_type, u8 *port_id, u8 *num_ch,
			u8 *ch_mask, u32 *ch_rate,
			u8 *port_type, u8 path)
{
	int i, j;
	u8 num_ports = 0;
	struct codec_port_info (*map)[MAX_PORT][MAX_CH_PER_PORT];
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	switch (path) {
	case CODEC_RX:
		map = &wcd938x->rx_port_mapping;
		num_ports = wcd938x->num_rx_ports;
		break;
	case CODEC_TX:
		map = &wcd938x->tx_port_mapping;
		num_ports = wcd938x->num_tx_ports;
		break;
	default:
		dev_err(component->dev, "%s Invalid path selected %u\n",
					__func__, path);
		return -EINVAL;
	}

	for (i = 0; i <= num_ports; i++) {
		for (j = 0; j < MAX_CH_PER_PORT; j++) {
			if ((*map)[i][j].slave_port_type == slv_prt_type)
				goto found;
		}
	}
found:
	if (i > num_ports || j == MAX_CH_PER_PORT) {
		dev_err(component->dev, "%s Failed to find slave port for type %u\n",
						__func__, slv_prt_type);
		return -EINVAL;
	}
	*port_id = i;
	*num_ch = (*map)[i][j].num_ch;
	*ch_mask = (*map)[i][j].ch_mask;
	*ch_rate = (*map)[i][j].ch_rate;
	*port_type = (*map)[i][j].master_port_type;

	return 0;
}

static int wcd938x_parse_port_mapping(struct device *dev,
			char *prop, u8 path)
{
	u32 *dt_array, map_size, map_length;
	u32 port_num = 0, ch_mask, ch_rate, old_port_num = 0;
	u32 slave_port_type, master_port_type;
	u32 i, ch_iter = 0;
	int ret = 0;
	u8 *num_ports = NULL;
	struct codec_port_info (*map)[MAX_PORT][MAX_CH_PER_PORT];
	struct wcd938x_priv *wcd938x = dev_get_drvdata(dev);

	switch (path) {
	case CODEC_RX:
		map = &wcd938x->rx_port_mapping;
		num_ports = &wcd938x->num_rx_ports;
		break;
	case CODEC_TX:
		map = &wcd938x->tx_port_mapping;
		num_ports = &wcd938x->num_tx_ports;
		break;
	default:
		dev_err(dev, "%s Invalid path selected %u\n",
			      __func__, path);
		return -EINVAL;
	}

	if (!of_find_property(dev->of_node, prop,
				&map_size)) {
		dev_err(dev, "missing port mapping prop %s\n", prop);
		ret = -EINVAL;
		goto err_port_map;
	}

	map_length = map_size / (NUM_SWRS_DT_PARAMS * sizeof(u32));

	dt_array = kzalloc(map_size, GFP_KERNEL);

	if (!dt_array) {
		ret = -ENOMEM;
		goto err_alloc;
	}
	ret = of_property_read_u32_array(dev->of_node, prop, dt_array,
				NUM_SWRS_DT_PARAMS * map_length);
	if (ret) {
		dev_err(dev, "%s: Failed to read  port mapping from prop %s\n",
					__func__, prop);
		goto err_pdata_fail;
	}

	for (i = 0; i < map_length; i++) {
		port_num = dt_array[NUM_SWRS_DT_PARAMS * i];
		slave_port_type = dt_array[NUM_SWRS_DT_PARAMS * i + 1];
		ch_mask = dt_array[NUM_SWRS_DT_PARAMS * i + 2];
		ch_rate = dt_array[NUM_SWRS_DT_PARAMS * i + 3];
		master_port_type = dt_array[NUM_SWRS_DT_PARAMS * i + 4];

		if (port_num != old_port_num)
			ch_iter = 0;

		(*map)[port_num][ch_iter].slave_port_type = slave_port_type;
		(*map)[port_num][ch_iter].ch_mask = ch_mask;
		(*map)[port_num][ch_iter].master_port_type = master_port_type;
		(*map)[port_num][ch_iter].num_ch = __sw_hweight8(ch_mask);
		(*map)[port_num][ch_iter++].ch_rate = ch_rate;
		old_port_num = port_num;
	}
	*num_ports = port_num;
	kfree(dt_array);
	return 0;

err_pdata_fail:
	kfree(dt_array);
err_alloc:
err_port_map:
	return ret;
}

static int wcd938x_tx_connect_port(struct snd_soc_component *component,
					u8 slv_port_type, int clk_rate,
					u8 enable)
{
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	u8 port_id, num_ch, ch_mask;
	u8 ch_type = 0;
	u32 ch_rate;
	int slave_ch_idx;
	u8 num_port = 1;
	int ret = 0;

	ret = wcd938x_set_port_params(component, slv_port_type, &port_id,
				&num_ch, &ch_mask, &ch_rate,
				&ch_type, CODEC_TX);
	if (ret)
		return ret;

	if (clk_rate)
		ch_rate = clk_rate;

	slave_ch_idx = wcd938x_slave_get_slave_ch_val(slv_port_type);
	if (slave_ch_idx != -EINVAL)
		ch_type = wcd938x->tx_master_ch_map[slave_ch_idx];

	dev_dbg(component->dev, "%s slv_ch_idx: %d, mstr_ch_type: %d\n",
		__func__, slave_ch_idx, ch_type);

	if (enable)
		ret = swr_connect_port(wcd938x->tx_swr_dev, &port_id,
					num_port, &ch_mask, &ch_rate,
					 &num_ch, &ch_type);
	else
		ret = swr_disconnect_port(wcd938x->tx_swr_dev, &port_id,
					num_port, &ch_mask, &ch_type);
	return ret;

}
static int wcd938x_rx_connect_port(struct snd_soc_component *component,
					u8 slv_port_type, u8 enable)
{
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	u8 port_id, num_ch, ch_mask, port_type;
	u32 ch_rate;
	u8 num_port = 1;
	int ret = 0;

	ret = wcd938x_set_port_params(component, slv_port_type, &port_id,
				&num_ch, &ch_mask, &ch_rate,
				&port_type, CODEC_RX);

	if (ret)
		return ret;

	if (enable)
		ret = swr_connect_port(wcd938x->rx_swr_dev, &port_id,
					num_port, &ch_mask, &ch_rate,
					&num_ch, &port_type);
	else
		ret = swr_disconnect_port(wcd938x->rx_swr_dev, &port_id,
					num_port, &ch_mask, &port_type);
	return ret;
}

static int wcd938x_rx_clk_enable(struct snd_soc_component *component)
{

	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	if (wcd938x->rx_clk_cnt == 0) {
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_ANA_CLK_CTL, 0x01, 0x01);
		snd_soc_component_update_bits(component,
				WCD938X_ANA_RX_SUPPLIES, 0x01, 0x01);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_RX0_CTL, 0x40, 0x00);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_RX1_CTL, 0x40, 0x00);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_RX2_CTL, 0x40, 0x00);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_ANA_CLK_CTL, 0x02, 0x02);
		snd_soc_component_update_bits(component,
				WCD938X_AUX_AUXPA, 0x10, 0x10);
	}
	wcd938x->rx_clk_cnt++;

	return 0;
}

static int wcd938x_rx_clk_disable(struct snd_soc_component *component)
{
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	wcd938x->rx_clk_cnt--;
	if (wcd938x->rx_clk_cnt == 0) {
		snd_soc_component_update_bits(component,
				WCD938X_ANA_RX_SUPPLIES, 0x40, 0x00);
		snd_soc_component_update_bits(component,
				WCD938X_ANA_RX_SUPPLIES, 0x80, 0x00);
		snd_soc_component_update_bits(component,
				WCD938X_ANA_RX_SUPPLIES, 0x01, 0x00);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_ANA_CLK_CTL, 0x02, 0x00);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_ANA_CLK_CTL, 0x01, 0x00);
	}
	return 0;
}

/*
 * wcd938x_soc_get_mbhc: get wcd938x_mbhc handle of corresponding component
 * @component: handle to snd_soc_component *
 *
 * return wcd938x_mbhc handle or error code in case of failure
 */
struct wcd938x_mbhc *wcd938x_soc_get_mbhc(struct snd_soc_component *component)
{
	struct wcd938x_priv *wcd938x;

	if (!component) {
		pr_err("%s: Invalid params, NULL component\n", __func__);
		return NULL;
	}
	wcd938x = snd_soc_component_get_drvdata(component);

	if (!wcd938x) {
		pr_err("%s: wcd938x is NULL\n", __func__);
		return NULL;
	}

	return wcd938x->mbhc;
}
EXPORT_SYMBOL(wcd938x_soc_get_mbhc);

static int wcd938x_codec_hphl_dac_event(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd938x_rx_clk_enable(component);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0x01, 0x01);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_HPH_GAIN_CTL, 0x04, 0x04);
		snd_soc_component_update_bits(component,
				WCD938X_HPH_RDAC_CLK_CTL1, 0x80, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_component_update_bits(component,
				WCD938X_HPH_NEW_INT_RDAC_HD2_CTL_L, 0x0F, 0x02);
		if (wcd938x->comp1_enable) {
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_COMP_CTL_0, 0x02, 0x02);
			/* 5msec compander delay as per HW requirement */
			if (!wcd938x->comp2_enable ||
				(snd_soc_component_read32(component,
					WCD938X_DIGITAL_CDC_COMP_CTL_0) & 0x01))
			usleep_range(5000, 5010);
			snd_soc_component_update_bits(component,
				WCD938X_HPH_NEW_INT_HPH_TIMER1, 0x02, 0x00);
		} else {
			snd_soc_component_update_bits(component,
					WCD938X_DIGITAL_CDC_COMP_CTL_0,
					0x02, 0x00);
			snd_soc_component_update_bits(component,
					WCD938X_HPH_L_EN, 0x20, 0x20);

		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			WCD938X_HPH_NEW_INT_RDAC_HD2_CTL_R,
			0x0F, 0x01);
		break;
	}

	return 0;
}

static int wcd938x_codec_hphr_dac_event(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd938x_rx_clk_enable(component);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0x02, 0x02);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_HPH_GAIN_CTL, 0x08, 0x08);
		snd_soc_component_update_bits(component,
				WCD938X_HPH_RDAC_CLK_CTL1, 0x80, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_component_update_bits(component,
				WCD938X_HPH_NEW_INT_RDAC_HD2_CTL_R, 0x0F, 0x02);
		if (wcd938x->comp2_enable) {
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_COMP_CTL_0, 0x01, 0x01);
			/* 5msec compander delay as per HW requirement */
			if (!wcd938x->comp1_enable ||
				(snd_soc_component_read32(component,
					WCD938X_DIGITAL_CDC_COMP_CTL_0) & 0x02))
				usleep_range(5000, 5010);
			snd_soc_component_update_bits(component,
				WCD938X_HPH_NEW_INT_HPH_TIMER1, 0x02, 0x00);
		} else {
			snd_soc_component_update_bits(component,
					WCD938X_DIGITAL_CDC_COMP_CTL_0,
					0x01, 0x00);
			snd_soc_component_update_bits(component,
					WCD938X_HPH_R_EN, 0x20, 0x20);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
			WCD938X_HPH_NEW_INT_RDAC_HD2_CTL_R,
			0x0F, 0x01);
		break;
	}

	return 0;
}

static int wcd938x_codec_ear_dac_event(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd938x_rx_clk_enable(component);
		wcd938x->ear_rx_path =
			snd_soc_component_read32(
				component, WCD938X_DIGITAL_CDC_EAR_PATH_CTL);
		if (wcd938x->ear_rx_path & EAR_RX_PATH_AUX) {
			snd_soc_component_update_bits(component,
				WCD938X_EAR_EAR_DAC_CON, 0x80, 0x00);
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_AUX_GAIN_CTL, 0x01, 0x01);
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0x04, 0x04);
			snd_soc_component_update_bits(component,
				WCD938X_ANA_EAR_COMPANDER_CTL, 0x80, 0x80);
		} else {
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_HPH_GAIN_CTL, 0x04, 0x04);
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0x01, 0x01);
			if (wcd938x->comp1_enable)
				snd_soc_component_update_bits(component,
					WCD938X_DIGITAL_CDC_COMP_CTL_0,
					0x02, 0x02);
		}
		/* 5 msec delay as per HW requirement */
		usleep_range(5000, 5010);
		if (wcd938x->flyback_cur_det_disable == 0)
			snd_soc_component_update_bits(component,
					WCD938X_FLYBACK_EN,
					0x04, 0x00);
		wcd938x->flyback_cur_det_disable++;
		wcd_cls_h_fsm(component, &wcd938x->clsh_info,
			     WCD_CLSH_EVENT_PRE_DAC,
			     WCD_CLSH_STATE_EAR,
			     wcd938x->hph_mode);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (wcd938x->ear_rx_path & EAR_RX_PATH_AUX) {
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_AUX_GAIN_CTL, 0x01, 0x00);
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0x04, 0x00);
		} else {
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_HPH_GAIN_CTL, 0x04, 0x00);
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0x01, 0x00);
			if (wcd938x->comp1_enable)
				snd_soc_component_update_bits(component,
					WCD938X_DIGITAL_CDC_COMP_CTL_0,
					0x02, 0x00);
		}
		snd_soc_component_update_bits(component,
				WCD938X_ANA_EAR_COMPANDER_CTL, 0x80, 0x00);
		snd_soc_component_update_bits(component,
				WCD938X_EAR_EAR_DAC_CON, 0x80, 0x80);
		break;
	};
	return 0;

}

static int wcd938x_codec_aux_dac_event(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	int ret = 0;

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd938x_rx_clk_enable(component);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_ANA_CLK_CTL, 0x04, 0x04);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0x04, 0x04);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_AUX_GAIN_CTL, 0x01, 0x01);
		if (wcd938x->flyback_cur_det_disable == 0)
			snd_soc_component_update_bits(component,
					WCD938X_FLYBACK_EN,
					0x04, 0x00);
		wcd938x->flyback_cur_det_disable++;
		wcd_cls_h_fsm(component, &wcd938x->clsh_info,
			     WCD_CLSH_EVENT_PRE_DAC,
			     WCD_CLSH_STATE_AUX,
			     wcd938x->hph_mode);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_ANA_CLK_CTL, 0x04, 0x00);
		break;
	};
	return ret;

}

static int wcd938x_codec_enable_hphr_pa(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	int ret = 0;
	int hph_mode = wcd938x->hph_mode;

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (wcd938x->ldoh)
			snd_soc_component_update_bits(component,
						WCD938X_LDOH_MODE,
						0x80, 0x80);
		if (wcd938x->update_wcd_event)
			wcd938x->update_wcd_event(wcd938x->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX2 << 0x10 | 0x1));
		ret = swr_slvdev_datapath_control(wcd938x->rx_swr_dev,
				    wcd938x->rx_swr_dev->dev_num,
				    true);
		wcd_cls_h_fsm(component, &wcd938x->clsh_info,
			     WCD_CLSH_EVENT_PRE_DAC,
			     WCD_CLSH_STATE_HPHR,
			     hph_mode);
		wcd_clsh_set_hph_mode(component, CLS_H_HIFI);
		if (hph_mode == CLS_H_LP || hph_mode == CLS_H_LOHIFI ||
		    hph_mode == CLS_H_ULP) {
			snd_soc_component_update_bits(component,
				WCD938X_HPH_REFBUFF_LP_CTL, 0x01, 0x01);
		}
		snd_soc_component_update_bits(component, WCD938X_ANA_HPH,
					      0x10, 0x10);
		wcd_clsh_set_hph_mode(component, hph_mode);
		/* 100 usec delay as per HW requirement */
		usleep_range(100, 110);
		set_bit(HPH_PA_DELAY, &wcd938x->status_mask);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_PDM_WD_CTL1, 0x07, 0x03);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/*
		 * 7ms sleep is required if compander is enabled as per
		 * HW requirement. If compander is disabled, then
		 * 20ms delay is required.
		 */
		if (test_bit(HPH_PA_DELAY, &wcd938x->status_mask)) {
			if (!wcd938x->comp2_enable)
				usleep_range(20000, 20100);
			else
				usleep_range(7000, 7100);
			if (hph_mode == CLS_H_LP ||
			    hph_mode == CLS_H_LOHIFI ||
			    hph_mode == CLS_H_ULP)
				snd_soc_component_update_bits(component,
					WCD938X_HPH_REFBUFF_LP_CTL, 0x01,
					0x00);
			clear_bit(HPH_PA_DELAY, &wcd938x->status_mask);
		}
		snd_soc_component_update_bits(component,
				WCD938X_HPH_NEW_INT_HPH_TIMER1, 0x02, 0x02);
		if (hph_mode == CLS_AB || hph_mode == CLS_AB_HIFI ||
			hph_mode == CLS_AB_LP || hph_mode == CLS_AB_LOHIFI)
			snd_soc_component_update_bits(component,
					WCD938X_ANA_RX_SUPPLIES, 0x02, 0x02);
		if (wcd938x->update_wcd_event)
			wcd938x->update_wcd_event(wcd938x->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX2 << 0x10));
		wcd_enable_irq(&wcd938x->irq_info,
					WCD938X_IRQ_HPHR_PDM_WD_INT);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (wcd938x->update_wcd_event)
			wcd938x->update_wcd_event(wcd938x->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX2 << 0x10 | 0x1));
		wcd_disable_irq(&wcd938x->irq_info,
					WCD938X_IRQ_HPHR_PDM_WD_INT);
		if (wcd938x->update_wcd_event && wcd938x->comp2_enable)
			wcd938x->update_wcd_event(wcd938x->handle,
					SLV_BOLERO_EVT_RX_COMPANDER_SOFT_RST,
					(WCD_RX2 << 0x10));
		/*
		 * 7ms sleep is required if compander is enabled as per
		 * HW requirement. If compander is disabled, then
		 * 20ms delay is required.
		 */
		if (!wcd938x->comp2_enable)
			usleep_range(20000, 20100);
		else
			usleep_range(7000, 7100);
		snd_soc_component_update_bits(component, WCD938X_ANA_HPH,
						0x40, 0x00);
		blocking_notifier_call_chain(&wcd938x->mbhc->notifier,
					     WCD_EVENT_PRE_HPHR_PA_OFF,
					     &wcd938x->mbhc->wcd_mbhc);
		set_bit(HPH_PA_DELAY, &wcd938x->status_mask);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * 7ms sleep is required if compander is enabled as per
		 * HW requirement. If compander is disabled, then
		 * 20ms delay is required.
		 */
		if (test_bit(HPH_PA_DELAY, &wcd938x->status_mask)) {
			if (!wcd938x->comp2_enable)
				usleep_range(20000, 20100);
			else
				usleep_range(7000, 7100);
			clear_bit(HPH_PA_DELAY, &wcd938x->status_mask);
		}
		blocking_notifier_call_chain(&wcd938x->mbhc->notifier,
					     WCD_EVENT_POST_HPHR_PA_OFF,
					     &wcd938x->mbhc->wcd_mbhc);
		snd_soc_component_update_bits(component, WCD938X_ANA_HPH,
						0x10, 0x00);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_PDM_WD_CTL1, 0x07, 0x00);
		wcd_cls_h_fsm(component, &wcd938x->clsh_info,
			     WCD_CLSH_EVENT_POST_PA,
			     WCD_CLSH_STATE_HPHR,
			     hph_mode);
		if (wcd938x->ldoh)
			snd_soc_component_update_bits(component,
						WCD938X_LDOH_MODE,
						0x80, 0x00);
		break;
	};
	return ret;
}

static int wcd938x_codec_enable_hphl_pa(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	int ret = 0;
	int hph_mode = wcd938x->hph_mode;

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (wcd938x->ldoh)
			snd_soc_component_update_bits(component,
						WCD938X_LDOH_MODE,
						0x80, 0x80);
		if (wcd938x->update_wcd_event)
			wcd938x->update_wcd_event(wcd938x->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX1 << 0x10 | 0x01));
		ret = swr_slvdev_datapath_control(wcd938x->rx_swr_dev,
				    wcd938x->rx_swr_dev->dev_num,
				    true);
		wcd_cls_h_fsm(component, &wcd938x->clsh_info,
			     WCD_CLSH_EVENT_PRE_DAC,
			     WCD_CLSH_STATE_HPHL,
			     hph_mode);
		wcd_clsh_set_hph_mode(component, CLS_H_HIFI);
		if (hph_mode == CLS_H_LP || hph_mode == CLS_H_LOHIFI ||
		    hph_mode == CLS_H_ULP) {
			snd_soc_component_update_bits(component,
				WCD938X_HPH_REFBUFF_LP_CTL, 0x01, 0x01);
		}
		snd_soc_component_update_bits(component, WCD938X_ANA_HPH,
						0x20, 0x20);
		wcd_clsh_set_hph_mode(component, hph_mode);
		/* 100 usec delay as per HW requirement */
		usleep_range(100, 110);
		set_bit(HPH_PA_DELAY, &wcd938x->status_mask);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_PDM_WD_CTL0, 0x07, 0x03);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/*
		 * 7ms sleep is required if compander is enabled as per
		 * HW requirement. If compander is disabled, then
		 * 20ms delay is required.
		 */
		if (test_bit(HPH_PA_DELAY, &wcd938x->status_mask)) {
			if (!wcd938x->comp1_enable)
				usleep_range(20000, 20100);
			else
				usleep_range(7000, 7100);
			if (hph_mode == CLS_H_LP ||
			    hph_mode == CLS_H_LOHIFI ||
			    hph_mode == CLS_H_ULP)
				snd_soc_component_update_bits(component,
					WCD938X_HPH_REFBUFF_LP_CTL,
					0x01, 0x00);
			clear_bit(HPH_PA_DELAY, &wcd938x->status_mask);
		}
		snd_soc_component_update_bits(component,
				WCD938X_HPH_NEW_INT_HPH_TIMER1, 0x02, 0x02);
		if (hph_mode == CLS_AB || hph_mode == CLS_AB_HIFI ||
			hph_mode == CLS_AB_LP || hph_mode == CLS_AB_LOHIFI)
			snd_soc_component_update_bits(component,
					WCD938X_ANA_RX_SUPPLIES, 0x02, 0x02);
		if (wcd938x->update_wcd_event)
			wcd938x->update_wcd_event(wcd938x->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX1 << 0x10));
		wcd_enable_irq(&wcd938x->irq_info,
					WCD938X_IRQ_HPHL_PDM_WD_INT);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (wcd938x->update_wcd_event)
			wcd938x->update_wcd_event(wcd938x->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX1 << 0x10 | 0x1));
		wcd_disable_irq(&wcd938x->irq_info,
					WCD938X_IRQ_HPHL_PDM_WD_INT);
		if (wcd938x->update_wcd_event && wcd938x->comp1_enable)
			wcd938x->update_wcd_event(wcd938x->handle,
					SLV_BOLERO_EVT_RX_COMPANDER_SOFT_RST,
					(WCD_RX1 << 0x10));
		/*
		 * 7ms sleep is required if compander is enabled as per
		 * HW requirement. If compander is disabled, then
		 * 20ms delay is required.
		 */
		if (!wcd938x->comp1_enable)
			usleep_range(20000, 20100);
		else
			usleep_range(7000, 7100);
		snd_soc_component_update_bits(component, WCD938X_ANA_HPH,
						0x80, 0x00);
		blocking_notifier_call_chain(&wcd938x->mbhc->notifier,
					     WCD_EVENT_PRE_HPHL_PA_OFF,
					     &wcd938x->mbhc->wcd_mbhc);
		set_bit(HPH_PA_DELAY, &wcd938x->status_mask);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * 7ms sleep is required if compander is enabled as per
		 * HW requirement. If compander is disabled, then
		 * 20ms delay is required.
		 */
		if (test_bit(HPH_PA_DELAY, &wcd938x->status_mask)) {
			if (!wcd938x->comp1_enable)
				usleep_range(21000, 21100);
			else
				usleep_range(7000, 7100);
			clear_bit(HPH_PA_DELAY, &wcd938x->status_mask);
		}
		blocking_notifier_call_chain(&wcd938x->mbhc->notifier,
					     WCD_EVENT_POST_HPHL_PA_OFF,
					     &wcd938x->mbhc->wcd_mbhc);
		snd_soc_component_update_bits(component, WCD938X_ANA_HPH,
						0x20, 0x00);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_PDM_WD_CTL0, 0x07, 0x00);
		wcd_cls_h_fsm(component, &wcd938x->clsh_info,
			     WCD_CLSH_EVENT_POST_PA,
			     WCD_CLSH_STATE_HPHL,
			     hph_mode);
		if (wcd938x->ldoh)
			snd_soc_component_update_bits(component,
						WCD938X_LDOH_MODE,
						0x80, 0x00);
		break;
	};
	return ret;
}

static int wcd938x_codec_enable_aux_pa(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	int hph_mode = wcd938x->hph_mode;
	int ret = 0;

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = swr_slvdev_datapath_control(wcd938x->rx_swr_dev,
			    wcd938x->rx_swr_dev->dev_num,
			    true);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_PDM_WD_CTL2, 0x01, 0x01);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* 1 msec delay as per HW requirement */
		usleep_range(1000, 1010);
		if (hph_mode == CLS_AB || hph_mode == CLS_AB_HIFI ||
			hph_mode == CLS_AB_LP || hph_mode == CLS_AB_LOHIFI)
			snd_soc_component_update_bits(component,
					WCD938X_ANA_RX_SUPPLIES,
					0x02, 0x02);
		if (wcd938x->update_wcd_event)
			wcd938x->update_wcd_event(wcd938x->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX3 << 0x10));
		wcd_enable_irq(&wcd938x->irq_info, WCD938X_IRQ_AUX_PDM_WD_INT);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		wcd_disable_irq(&wcd938x->irq_info,
					WCD938X_IRQ_AUX_PDM_WD_INT);
		if (wcd938x->update_wcd_event)
			wcd938x->update_wcd_event(wcd938x->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX3 << 0x10 | 0x1));
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* 1 msec delay as per HW requirement */
		usleep_range(1000, 1010);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_PDM_WD_CTL2, 0x01, 0x00);
		wcd_cls_h_fsm(component, &wcd938x->clsh_info,
			     WCD_CLSH_EVENT_POST_PA,
			     WCD_CLSH_STATE_AUX,
			     hph_mode);

		wcd938x->flyback_cur_det_disable--;
		if (wcd938x->flyback_cur_det_disable == 0)
			snd_soc_component_update_bits(component,
						      WCD938X_FLYBACK_EN,
						      0x04, 0x04);
		break;
	};
	return ret;
}

static int wcd938x_codec_enable_ear_pa(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	struct snd_soc_component *component =
					snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	int hph_mode = wcd938x->hph_mode;
	int ret = 0;

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = swr_slvdev_datapath_control(wcd938x->rx_swr_dev,
			    wcd938x->rx_swr_dev->dev_num,
			    true);
		/*
		 * Enable watchdog interrupt for HPHL or AUX
		 * depending on mux value
		 */
		wcd938x->ear_rx_path =
			snd_soc_component_read32(
				component, WCD938X_DIGITAL_CDC_EAR_PATH_CTL);
		if (wcd938x->ear_rx_path & EAR_RX_PATH_AUX)
			snd_soc_component_update_bits(component,
					WCD938X_DIGITAL_PDM_WD_CTL2,
					0x01, 0x01);
		else
			snd_soc_component_update_bits(component,
					WCD938X_DIGITAL_PDM_WD_CTL0,
					0x07, 0x03);
		if (!wcd938x->comp1_enable)
			snd_soc_component_update_bits(component,
				WCD938X_ANA_EAR_COMPANDER_CTL, 0x80, 0x80);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* 6 msec delay as per HW requirement */
		usleep_range(6000, 6010);
		if (hph_mode == CLS_AB || hph_mode == CLS_AB_HIFI ||
			hph_mode == CLS_AB_LP || hph_mode == CLS_AB_LOHIFI)
			snd_soc_component_update_bits(component,
					WCD938X_ANA_RX_SUPPLIES,
					0x02, 0x02);
		if (wcd938x->ear_rx_path & EAR_RX_PATH_AUX) {
			if (wcd938x->update_wcd_event)
				wcd938x->update_wcd_event(wcd938x->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX3 << 0x10));
			wcd_enable_irq(&wcd938x->irq_info,
					WCD938X_IRQ_AUX_PDM_WD_INT);
		} else {
			if (wcd938x->update_wcd_event)
				wcd938x->update_wcd_event(wcd938x->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX1 << 0x10));
			wcd_enable_irq(&wcd938x->irq_info,
					WCD938X_IRQ_HPHL_PDM_WD_INT);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (wcd938x->ear_rx_path & EAR_RX_PATH_AUX) {
			wcd_disable_irq(&wcd938x->irq_info,
					WCD938X_IRQ_AUX_PDM_WD_INT);
			if (wcd938x->update_wcd_event)
				wcd938x->update_wcd_event(wcd938x->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX3 << 0x10 | 0x1));
		} else {
			wcd_disable_irq(&wcd938x->irq_info,
					WCD938X_IRQ_HPHL_PDM_WD_INT);
			if (wcd938x->update_wcd_event)
				wcd938x->update_wcd_event(wcd938x->handle,
						SLV_BOLERO_EVT_RX_MUTE,
						(WCD_RX1 << 0x10 | 0x1));
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (!wcd938x->comp1_enable)
			snd_soc_component_update_bits(component,
				WCD938X_ANA_EAR_COMPANDER_CTL, 0x80, 0x00);
		/* 7 msec delay as per HW requirement */
		usleep_range(7000, 7010);
		if (wcd938x->ear_rx_path & EAR_RX_PATH_AUX)
			snd_soc_component_update_bits(component,
					WCD938X_DIGITAL_PDM_WD_CTL2,
					0x01, 0x00);
		else
			snd_soc_component_update_bits(component,
					WCD938X_DIGITAL_PDM_WD_CTL0,
					0x07, 0x00);
		wcd_cls_h_fsm(component, &wcd938x->clsh_info,
			     WCD_CLSH_EVENT_POST_PA,
			     WCD_CLSH_STATE_EAR,
			     hph_mode);

		wcd938x->flyback_cur_det_disable--;
		if (wcd938x->flyback_cur_det_disable == 0)
			snd_soc_component_update_bits(component,
						      WCD938X_FLYBACK_EN,
						      0x04, 0x04);
		break;
	};
	return ret;
}

static int wcd938x_enable_clsh(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *kcontrol,
			       int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	int mode = wcd938x->hph_mode;
	int ret = 0;

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	if (mode == CLS_H_LOHIFI || mode == CLS_H_ULP ||
		mode == CLS_H_HIFI || mode == CLS_H_LP) {
		wcd938x_rx_connect_port(component, CLSH,
				SND_SOC_DAPM_EVENT_ON(event));
	}
	if (SND_SOC_DAPM_EVENT_OFF(event))
		ret = swr_slvdev_datapath_control(
				wcd938x->rx_swr_dev,
				wcd938x->rx_swr_dev->dev_num,
				false);
	return ret;
}

static int wcd938x_enable_rx1(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	struct snd_soc_component *component =
					snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd938x_rx_connect_port(component, HPH_L, true);
		if (wcd938x->comp1_enable)
			wcd938x_rx_connect_port(component, COMP_L, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd938x_rx_connect_port(component, HPH_L, false);
		if (wcd938x->comp1_enable)
			wcd938x_rx_connect_port(component, COMP_L, false);
		wcd938x_rx_clk_disable(component);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL,
				0x01, 0x00);
		break;
	};

	return 0;
}

static int wcd938x_enable_rx2(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd938x_rx_connect_port(component, HPH_R, true);
		if (wcd938x->comp2_enable)
			wcd938x_rx_connect_port(component, COMP_R, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd938x_rx_connect_port(component, HPH_R, false);
		if (wcd938x->comp2_enable)
			wcd938x_rx_connect_port(component, COMP_R, false);
		wcd938x_rx_clk_disable(component);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL,
				0x02, 0x00);
		break;
	};

	return 0;
}

static int wcd938x_enable_rx3(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol,
			      int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd938x_rx_connect_port(component, LO, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd938x_rx_connect_port(component, LO, false);
		/* 6 msec delay as per HW requirement */
		usleep_range(6000, 6010);
		wcd938x_rx_clk_disable(component);
		snd_soc_component_update_bits(component,
			WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0x04, 0x00);
		break;
	}

	return 0;
}

static int wcd938x_codec_enable_dmic(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	u16 dmic_clk_reg, dmic_clk_en_reg;
	s32 *dmic_clk_cnt;
	u8 dmic_ctl_shift = 0;
	u8 dmic_clk_shift = 0;
	u8 dmic_clk_mask = 0;
	u16 dmic2_left_en = 0;
	int ret = 0;

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (w->shift) {
	case 0:
	case 1:
		dmic_clk_cnt = &(wcd938x->dmic_0_1_clk_cnt);
		dmic_clk_reg = WCD938X_DIGITAL_CDC_DMIC_RATE_1_2;
		dmic_clk_en_reg = WCD938X_DIGITAL_CDC_DMIC1_CTL;
		dmic_clk_mask = 0x0F;
		dmic_clk_shift = 0x00;
		dmic_ctl_shift = 0x00;
		break;
	case 2:
		dmic2_left_en = WCD938X_DIGITAL_CDC_DMIC2_CTL;
	case 3:
		dmic_clk_cnt = &(wcd938x->dmic_2_3_clk_cnt);
		dmic_clk_reg = WCD938X_DIGITAL_CDC_DMIC_RATE_1_2;
		dmic_clk_en_reg = WCD938X_DIGITAL_CDC_DMIC2_CTL;
		dmic_clk_mask = 0xF0;
		dmic_clk_shift = 0x04;
		dmic_ctl_shift = 0x01;
		break;
	case 4:
	case 5:
		dmic_clk_cnt = &(wcd938x->dmic_4_5_clk_cnt);
		dmic_clk_reg = WCD938X_DIGITAL_CDC_DMIC_RATE_3_4;
		dmic_clk_en_reg = WCD938X_DIGITAL_CDC_DMIC3_CTL;
		dmic_clk_mask = 0x0F;
		dmic_clk_shift = 0x00;
		dmic_ctl_shift = 0x02;
		break;
	case 6:
	case 7:
		dmic_clk_cnt = &(wcd938x->dmic_6_7_clk_cnt);
		dmic_clk_reg = WCD938X_DIGITAL_CDC_DMIC_RATE_3_4;
		dmic_clk_en_reg = WCD938X_DIGITAL_CDC_DMIC4_CTL;
		dmic_clk_mask = 0xF0;
		dmic_clk_shift = 0x04;
		dmic_ctl_shift = 0x03;
		break;
	default:
		dev_err(component->dev, "%s: Invalid DMIC Selection\n",
			__func__);
		return -EINVAL;
	};
	dev_dbg(component->dev, "%s: event %d DMIC%d dmic_clk_cnt %d\n",
			__func__, event,  (w->shift +1), *dmic_clk_cnt);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_AMIC_CTL,
				(0x01 << dmic_ctl_shift), 0x00);
		/* 250us sleep as per HW requirement */
		usleep_range(250, 260);
		if (dmic2_left_en)
			snd_soc_component_update_bits(component,
				dmic2_left_en, 0x80, 0x80);
		/* Setting DMIC clock rate to 2.4MHz */
		snd_soc_component_update_bits(component,
					      dmic_clk_reg, dmic_clk_mask,
					      (0x03 << dmic_clk_shift));
		snd_soc_component_update_bits(component,
					      dmic_clk_en_reg, 0x08, 0x08);
		/* enable clock scaling */
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DMIC_CTL, 0x06, 0x06);
		ret = swr_slvdev_datapath_control(wcd938x->tx_swr_dev,
				wcd938x->tx_swr_dev->dev_num,
				true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd938x_tx_connect_port(component, DMIC0 + (w->shift), 0,
				false);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_AMIC_CTL,
				(0x01 << dmic_ctl_shift),
				(0x01 << dmic_ctl_shift));
		if (dmic2_left_en)
			snd_soc_component_update_bits(component,
				dmic2_left_en, 0x80, 0x00);
		snd_soc_component_update_bits(component,
					      dmic_clk_en_reg, 0x08, 0x00);
		break;
	};
	return ret;
}

/*
 * wcd938x_get_micb_vout_ctl_val: converts micbias from volts to register value
 * @micb_mv: micbias in mv
 *
 * return register value converted
 */
int wcd938x_get_micb_vout_ctl_val(u32 micb_mv)
{
	/* min micbias voltage is 1V and maximum is 2.85V */
	if (micb_mv < 1000 || micb_mv > 2850) {
		pr_err("%s: unsupported micbias voltage\n", __func__);
		return -EINVAL;
	}

	return (micb_mv - 1000) / 50;
}
EXPORT_SYMBOL(wcd938x_get_micb_vout_ctl_val);

/*
 * wcd938x_mbhc_micb_adjust_voltage: adjust specific micbias voltage
 * @component: handle to snd_soc_component *
 * @req_volt: micbias voltage to be set
 * @micb_num: micbias to be set, e.g. micbias1 or micbias2
 *
 * return 0 if adjustment is success or error code in case of failure
 */
int wcd938x_mbhc_micb_adjust_voltage(struct snd_soc_component *component,
				   int req_volt, int micb_num)
{
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	int cur_vout_ctl, req_vout_ctl;
	int micb_reg, micb_val, micb_en;
	int ret = 0;

	switch (micb_num) {
	case MIC_BIAS_1:
		micb_reg = WCD938X_ANA_MICB1;
		break;
	case MIC_BIAS_2:
		micb_reg = WCD938X_ANA_MICB2;
		break;
	case MIC_BIAS_3:
		micb_reg = WCD938X_ANA_MICB3;
		break;
	case MIC_BIAS_4:
		micb_reg = WCD938X_ANA_MICB4;
		break;
	default:
		return -EINVAL;
	}
	mutex_lock(&wcd938x->micb_lock);

	/*
	 * If requested micbias voltage is same as current micbias
	 * voltage, then just return. Otherwise, adjust voltage as
	 * per requested value. If micbias is already enabled, then
	 * to avoid slow micbias ramp-up or down enable pull-up
	 * momentarily, change the micbias value and then re-enable
	 * micbias.
	 */
	micb_val = snd_soc_component_read32(component, micb_reg);
	micb_en = (micb_val & 0xC0) >> 6;
	cur_vout_ctl = micb_val & 0x3F;

	req_vout_ctl = wcd938x_get_micb_vout_ctl_val(req_volt);
	if (req_vout_ctl < 0) {
		ret = -EINVAL;
		goto exit;
	}
	if (cur_vout_ctl == req_vout_ctl) {
		ret = 0;
		goto exit;
	}

	dev_dbg(component->dev, "%s: micb_num: %d, cur_mv: %d, req_mv: %d, micb_en: %d\n",
		 __func__, micb_num, WCD_VOUT_CTL_TO_MICB(cur_vout_ctl),
		 req_volt, micb_en);

	if (micb_en == 0x1)
		snd_soc_component_update_bits(component, micb_reg, 0xC0, 0x80);

	snd_soc_component_update_bits(component, micb_reg, 0x3F, req_vout_ctl);

	if (micb_en == 0x1) {
		snd_soc_component_update_bits(component, micb_reg, 0xC0, 0x40);
		/*
		 * Add 2ms delay as per HW requirement after enabling
		 * micbias
		 */
		usleep_range(2000, 2100);
	}
exit:
	mutex_unlock(&wcd938x->micb_lock);
	return ret;
}
EXPORT_SYMBOL(wcd938x_mbhc_micb_adjust_voltage);

static int wcd938x_tx_swr_ctrl(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol,
				    int event)
{
	struct snd_soc_component *component =
					snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	int ret = 0;
	int bank = 0;
	u8 mode = 0;
	int i = 0;
	int rate = 0;

	bank = (wcd938x_swr_slv_get_current_bank(wcd938x->tx_swr_dev,
					wcd938x->tx_swr_dev->dev_num) ? 0 : 1);

	/* power mode is applicable only to analog mics */
	if (strnstr(w->name, "ADC", sizeof("ADC"))) {
		/* Get channel rate */
		rate = wcd938x_get_clk_rate(wcd938x->tx_mode[w->shift - ADC1]);
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Check AMIC2 is connected to ADC2 to take an action on BCS */
		if (w->shift == ADC2 && !(snd_soc_component_read32(component,
			WCD938X_TX_NEW_AMIC_MUX_CFG) & 0x80)) {
			if (!wcd938x->bcs_dis)
				wcd938x_tx_connect_port(component, MBHC,
					SWR_CLK_RATE_4P8MHZ, true);
			set_bit(AMIC2_BCS_ENABLE, &wcd938x->status_mask);
		}
		if (strnstr(w->name, "ADC", sizeof("ADC"))) {
			set_bit(w->shift - ADC1, &wcd938x->status_mask);
			wcd938x_tx_connect_port(component, w->shift, rate,
					true);
		} else {
			wcd938x_tx_connect_port(component, w->shift,
					SWR_CLK_RATE_2P4MHZ, true);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (strnstr(w->name, "ADC", sizeof("ADC"))) {
			if (strnstr(w->name, "ADC1", sizeof("ADC1"))) {
				clear_bit(WCD_ADC1, &wcd938x->status_mask);
				clear_bit(WCD_ADC1_MODE, &wcd938x->status_mask);
			} else if (strnstr(w->name, "ADC2", sizeof("ADC2"))) {
				clear_bit(WCD_ADC2, &wcd938x->status_mask);
				clear_bit(WCD_ADC2_MODE, &wcd938x->status_mask);
			} else if (strnstr(w->name, "ADC3", sizeof("ADC3"))) {
				clear_bit(WCD_ADC3, &wcd938x->status_mask);
				clear_bit(WCD_ADC3_MODE, &wcd938x->status_mask);
			} else if (strnstr(w->name, "ADC4", sizeof("ADC4"))) {
				clear_bit(WCD_ADC4, &wcd938x->status_mask);
				clear_bit(WCD_ADC4_MODE, &wcd938x->status_mask);
			}
		}
		if (strnstr(w->name, "ADC", sizeof("ADC"))) {
			if (test_bit(WCD_ADC1, &wcd938x->status_mask) ||
				test_bit(WCD_ADC1_MODE, &wcd938x->status_mask))
				mode |= tx_mode_bit[wcd938x->tx_mode[WCD_ADC1]];
			if (test_bit(WCD_ADC2, &wcd938x->status_mask) ||
				test_bit(WCD_ADC2_MODE, &wcd938x->status_mask))
				mode |= tx_mode_bit[wcd938x->tx_mode[WCD_ADC2]];
			if (test_bit(WCD_ADC3, &wcd938x->status_mask) ||
				test_bit(WCD_ADC3_MODE, &wcd938x->status_mask))
				mode |= tx_mode_bit[wcd938x->tx_mode[WCD_ADC3]];
			if (test_bit(WCD_ADC4, &wcd938x->status_mask) ||
				test_bit(WCD_ADC4_MODE, &wcd938x->status_mask))
				mode |= tx_mode_bit[wcd938x->tx_mode[WCD_ADC4]];

			if (mode != 0) {
				for (i = 0; i < ADC_MODE_ULP2; i++) {
					if (mode & (1 << i)) {
						i++;
						break;
					}
				}
			}
			rate = wcd938x_get_clk_rate(i);
			if (wcd938x->adc_count) {
				rate =  (wcd938x->adc_count * rate);
				if (rate > SWR_CLK_RATE_9P6MHZ)
					rate = SWR_CLK_RATE_9P6MHZ;
			}
			wcd938x_set_swr_clk_rate(component, rate, bank);
		}
		ret = swr_slvdev_datapath_control(wcd938x->tx_swr_dev,
				wcd938x->tx_swr_dev->dev_num,
				false);

		if (strnstr(w->name, "ADC", sizeof("ADC")))
			wcd938x_set_swr_clk_rate(component, rate, !bank);
		break;
	};

	return ret;
}

static int wcd938x_get_adc_mode(int val)
{
	int ret = 0;

	switch (val) {
	case ADC_MODE_INVALID:
		ret = ADC_MODE_VAL_NORMAL;
		break;
	case ADC_MODE_HIFI:
		ret = ADC_MODE_VAL_HIFI;
		break;
	case ADC_MODE_LO_HIF:
		ret = ADC_MODE_VAL_LO_HIF;
		break;
	case ADC_MODE_NORMAL:
		ret = ADC_MODE_VAL_NORMAL;
		break;
	case ADC_MODE_LP:
		ret = ADC_MODE_VAL_LP;
		break;
	case ADC_MODE_ULP1:
		ret = ADC_MODE_VAL_ULP1;
		break;
	case ADC_MODE_ULP2:
		ret = ADC_MODE_VAL_ULP2;
		break;
	default:
		ret = -EINVAL;
		pr_err("%s: invalid ADC mode value %d\n", __func__, val);
		break;
	}
	return ret;
}

int wcd938x_tx_channel_config(struct snd_soc_component *component,
			      int channel, int mode)
{
	int reg = WCD938X_ANA_TX_CH2, mask = 0, val = 0;
	int ret = 0;

	switch (channel) {
	case 0:
		reg = WCD938X_ANA_TX_CH2;
		mask = 0x40;
		break;
	case 1:
		reg = WCD938X_ANA_TX_CH2;
		mask = 0x20;
		break;
	case 2:
		reg = WCD938X_ANA_TX_CH4;
		mask = 0x40;
		break;
	case 3:
		reg = WCD938X_ANA_TX_CH4;
		mask = 0x20;
		break;
	default:
		pr_err("%s: Invalid channel num %d\n", __func__, channel);
		ret = -EINVAL;
		break;
	}

	if (!mode)
		val = 0x00;
	else
		val = mask;

	if (!ret)
		snd_soc_component_update_bits(component, reg, mask, val);

	return ret;
}

static int wcd938x_codec_enable_adc(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol,
				    int event){
	struct snd_soc_component *component =
					snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	int clk_rate = 0, ret = 0;
	int mode = 0, i = 0, bank = 0;

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);
	bank = (wcd938x_swr_slv_get_current_bank(wcd938x->tx_swr_dev,
		wcd938x->tx_swr_dev->dev_num) ? 0 : 1);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd938x->adc_count++;
		if (test_bit(WCD_ADC1, &wcd938x->status_mask) ||
			test_bit(WCD_ADC1_MODE, &wcd938x->status_mask))
			mode |= tx_mode_bit[wcd938x->tx_mode[WCD_ADC1]];
		if (test_bit(WCD_ADC2, &wcd938x->status_mask) ||
			test_bit(WCD_ADC2_MODE, &wcd938x->status_mask))
			mode |= tx_mode_bit[wcd938x->tx_mode[WCD_ADC2]];
		if (test_bit(WCD_ADC3, &wcd938x->status_mask) ||
			test_bit(WCD_ADC3_MODE, &wcd938x->status_mask))
			mode |= tx_mode_bit[wcd938x->tx_mode[WCD_ADC3]];
		if (test_bit(WCD_ADC4, &wcd938x->status_mask) ||
			test_bit(WCD_ADC4_MODE, &wcd938x->status_mask))
			mode |= tx_mode_bit[wcd938x->tx_mode[WCD_ADC4]];

		if (mode != 0) {
			for (i = 0; i < ADC_MODE_ULP2; i++) {
				if (mode & (1 << i)) {
					i++;
					break;
				}
			}
		}
		clk_rate = wcd938x_get_clk_rate(i);

		/* clk_rate depends on number of paths getting enabled */
		clk_rate =  (wcd938x->adc_count * clk_rate);
		if (clk_rate > SWR_CLK_RATE_9P6MHZ)
			clk_rate = SWR_CLK_RATE_9P6MHZ;
		wcd938x_set_swr_clk_rate(component, clk_rate, bank);
		ret = swr_slvdev_datapath_control(wcd938x->tx_swr_dev,
				 wcd938x->tx_swr_dev->dev_num,
				 true);
		wcd938x_set_swr_clk_rate(component, clk_rate, !bank);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd938x->adc_count--;
		if (wcd938x->adc_count < 0)
			wcd938x->adc_count = 0;

		wcd938x_tx_connect_port(component, ADC1 + w->shift, 0, false);
		if (w->shift + ADC1 == ADC2 &&
			test_bit(AMIC2_BCS_ENABLE, &wcd938x->status_mask)) {
			if (!wcd938x->bcs_dis)
				wcd938x_tx_connect_port(component, MBHC, 0,
					false);
			clear_bit(AMIC2_BCS_ENABLE, &wcd938x->status_mask);
		}
		break;
	};

	return ret;
}

void wcd938x_disable_bcs_before_slow_insert(struct snd_soc_component *component,
					    bool bcs_disable)
{
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	if (wcd938x->update_wcd_event) {
		if (bcs_disable)
			wcd938x->update_wcd_event(wcd938x->handle,
						SLV_BOLERO_EVT_BCS_CLK_OFF, 0);
		else
			wcd938x->update_wcd_event(wcd938x->handle,
						SLV_BOLERO_EVT_BCS_CLK_OFF, 1);
	}
}

static int wcd938x_enable_req(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
					snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x =
					snd_soc_component_get_drvdata(component);
	int ret = 0;
	u8 mode = 0;

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_ANA_CLK_CTL, 0x08, 0x08);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_ANA_CLK_CTL, 0x10, 0x10);

		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_REQ_CTL, 0x02, 0x02);
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_REQ_CTL, 0x01, 0x00);

		ret = wcd938x_tx_channel_config(component, w->shift, 1);
		mode = wcd938x_get_adc_mode(wcd938x->tx_mode[w->shift]);
		if (mode < 0) {
			dev_info(component->dev,
				 "%s: invalid mode, setting to normal mode\n",
				 __func__);
			mode = ADC_MODE_VAL_NORMAL;
		}
		switch (w->shift) {
		case 0:
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_TX_ANA_MODE_0_1, 0x0F,
				mode);
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0x10, 0x10);
			break;
		case 1:
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_TX_ANA_MODE_0_1, 0xF0,
				mode << 4);
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0x20, 0x20);
			break;
		case 2:
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_TX_ANA_MODE_2_3, 0x0F,
				mode);
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0x40, 0x40);
			break;
		case 3:
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_TX_ANA_MODE_2_3, 0xF0,
				mode << 4);
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0x80, 0x80);
			break;
		default:
			break;
		}
		ret |= wcd938x_tx_channel_config(component, w->shift, 0);
		break;
	case SND_SOC_DAPM_POST_PMD:
		switch (w->shift) {
		case 0:
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_TX_ANA_MODE_0_1, 0x0F,
				0x00);
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0x10, 0x00);
			break;
		case 1:
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_TX_ANA_MODE_0_1, 0xF0,
				0x00);
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0x20, 0x00);
			break;
		case 2:
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_TX_ANA_MODE_2_3, 0x0F,
				0x00);
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0x40, 0x00);
			break;
		case 3:
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_TX_ANA_MODE_2_3, 0xF0,
				0x00);
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0x80, 0x00);
			break;
		default:
			break;
		}
		if (wcd938x->adc_count == 0) {
			snd_soc_component_update_bits(component,
					WCD938X_DIGITAL_CDC_ANA_CLK_CTL, 0x10, 0x00);
			snd_soc_component_update_bits(component,
					WCD938X_DIGITAL_CDC_ANA_CLK_CTL, 0x08, 0x00);
		}
		break;
	};
	return ret;
}

int wcd938x_micbias_control(struct snd_soc_component *component,
				int micb_num, int req, bool is_dapm)
{

	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	int micb_index = micb_num - 1;
	u16 micb_reg;
	int pre_off_event = 0, post_off_event = 0;
	int post_on_event = 0, post_dapm_off = 0;
	int post_dapm_on = 0;
	int ret = 0;

	if ((micb_index < 0) || (micb_index > WCD938X_MAX_MICBIAS - 1)) {
		dev_err(component->dev,
			"%s: Invalid micbias index, micb_ind:%d\n",
			__func__, micb_index);
		return -EINVAL;
	}

	if (NULL == wcd938x) {
		dev_err(component->dev,
			"%s: wcd938x private data is NULL\n", __func__);
		return -EINVAL;
	}

	switch (micb_num) {
	case MIC_BIAS_1:
		micb_reg = WCD938X_ANA_MICB1;
		break;
	case MIC_BIAS_2:
		micb_reg = WCD938X_ANA_MICB2;
		pre_off_event = WCD_EVENT_PRE_MICBIAS_2_OFF;
		post_off_event = WCD_EVENT_POST_MICBIAS_2_OFF;
		post_on_event = WCD_EVENT_POST_MICBIAS_2_ON;
		post_dapm_on = WCD_EVENT_POST_DAPM_MICBIAS_2_ON;
		post_dapm_off = WCD_EVENT_POST_DAPM_MICBIAS_2_OFF;
		break;
	case MIC_BIAS_3:
		micb_reg = WCD938X_ANA_MICB3;
		break;
	case MIC_BIAS_4:
		micb_reg = WCD938X_ANA_MICB4;
		break;
	default:
		dev_err(component->dev, "%s: Invalid micbias number: %d\n",
			__func__, micb_num);
		return -EINVAL;
	};
	mutex_lock(&wcd938x->micb_lock);

	switch (req) {
	case MICB_PULLUP_ENABLE:
		if (!wcd938x->dev_up) {
			dev_dbg(component->dev, "%s: enable req %d wcd device down\n",
				__func__, req);
			ret = -ENODEV;
			goto done;
		}
		wcd938x->pullup_ref[micb_index]++;
		if ((wcd938x->pullup_ref[micb_index] == 1) &&
		    (wcd938x->micb_ref[micb_index] == 0))
			snd_soc_component_update_bits(component, micb_reg,
							0xC0, 0x80);
		break;
	case MICB_PULLUP_DISABLE:
		if (wcd938x->pullup_ref[micb_index] > 0)
			wcd938x->pullup_ref[micb_index]--;
		if (!wcd938x->dev_up) {
			dev_dbg(component->dev, "%s: enable req %d wcd device down\n",
				__func__, req);
			ret = -ENODEV;
			goto done;
		}
		if ((wcd938x->pullup_ref[micb_index] == 0) &&
		    (wcd938x->micb_ref[micb_index] == 0))
			snd_soc_component_update_bits(component, micb_reg,
							0xC0, 0x00);
		break;
	case MICB_ENABLE:
		if (!wcd938x->dev_up) {
			dev_dbg(component->dev, "%s: enable req %d wcd device down\n",
				__func__, req);
			ret = -ENODEV;
			goto done;
		}
		wcd938x->micb_ref[micb_index]++;
		if (wcd938x->micb_ref[micb_index] == 1) {
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0xF0, 0xF0);
			snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_CDC_ANA_CLK_CTL, 0x10, 0x10);
			snd_soc_component_update_bits(component,
			       WCD938X_DIGITAL_CDC_ANA_TX_CLK_CTL, 0x01, 0x01);
			snd_soc_component_update_bits(component,
				WCD938X_MICB1_TEST_CTL_2, 0x01, 0x01);
			snd_soc_component_update_bits(component,
				WCD938X_MICB2_TEST_CTL_2, 0x01, 0x01);
			snd_soc_component_update_bits(component,
				WCD938X_MICB3_TEST_CTL_2, 0x01, 0x01);
			snd_soc_component_update_bits(component,
				WCD938X_MICB4_TEST_CTL_2, 0x01, 0x01);
			snd_soc_component_update_bits(component,
				micb_reg, 0xC0, 0x40);
			if (post_on_event)
				blocking_notifier_call_chain(
						&wcd938x->mbhc->notifier,
						post_on_event,
						&wcd938x->mbhc->wcd_mbhc);
		}
		if (is_dapm && post_dapm_on && wcd938x->mbhc)
			blocking_notifier_call_chain(&wcd938x->mbhc->notifier,
						     post_dapm_on,
						     &wcd938x->mbhc->wcd_mbhc);
		break;
	case MICB_DISABLE:
		if (wcd938x->micb_ref[micb_index] > 0)
			wcd938x->micb_ref[micb_index]--;
		if (!wcd938x->dev_up) {
			dev_dbg(component->dev, "%s: enable req %d wcd device down\n",
				__func__, req);
			ret = -ENODEV;
			goto done;
		}
		if ((wcd938x->micb_ref[micb_index] == 0) &&
		    (wcd938x->pullup_ref[micb_index] > 0))
			snd_soc_component_update_bits(component, micb_reg,
							0xC0, 0x80);
		else if ((wcd938x->micb_ref[micb_index] == 0) &&
			 (wcd938x->pullup_ref[micb_index] == 0)) {
			if (pre_off_event && wcd938x->mbhc)
				blocking_notifier_call_chain(
						&wcd938x->mbhc->notifier,
						pre_off_event,
						&wcd938x->mbhc->wcd_mbhc);
			snd_soc_component_update_bits(component, micb_reg,
							0xC0, 0x00);
			if (post_off_event && wcd938x->mbhc)
				blocking_notifier_call_chain(
						&wcd938x->mbhc->notifier,
						post_off_event,
						&wcd938x->mbhc->wcd_mbhc);
		}
		if (is_dapm && post_dapm_off && wcd938x->mbhc)
			blocking_notifier_call_chain(&wcd938x->mbhc->notifier,
						post_dapm_off,
						&wcd938x->mbhc->wcd_mbhc);
		break;
	};

	dev_dbg(component->dev,
		"%s: micb_num:%d, micb_ref: %d, pullup_ref: %d\n",
		__func__, micb_num, wcd938x->micb_ref[micb_index],
		wcd938x->pullup_ref[micb_index]);

done:
	mutex_unlock(&wcd938x->micb_lock);
	return ret;
}
EXPORT_SYMBOL(wcd938x_micbias_control);

static int wcd938x_get_logical_addr(struct swr_device *swr_dev)
{
	int ret = 0;
	uint8_t devnum = 0;
	int num_retry = NUM_ATTEMPTS;

	do {
		/* retry after 1ms */
		usleep_range(1000, 1010);
		ret = swr_get_logical_dev_num(swr_dev, swr_dev->addr, &devnum);
	} while (ret && --num_retry);

	if (ret)
		dev_err(&swr_dev->dev,
			"%s get devnum %d for dev addr %llx failed\n",
			__func__, devnum, swr_dev->addr);

	swr_dev->dev_num = devnum;
	return 0;
}

static bool get_usbc_hs_status(struct snd_soc_component *component,
			struct wcd_mbhc_config *mbhc_cfg)
{
	if (mbhc_cfg->enable_usbc_analog) {
		if (!(snd_soc_component_read32(component, WCD938X_ANA_MBHC_MECH)
			& 0x20))
			return true;
	}
	return false;
}

int wcd938x_swr_dmic_register_notifier(struct snd_soc_component *component,
					struct notifier_block *nblock,
					bool enable)
{
	struct wcd938x_priv *wcd938x_priv;
	if(NULL == component) {
		pr_err("%s: wcd938x component is NULL\n", __func__);
		return -EINVAL;
	}

	wcd938x_priv = snd_soc_component_get_drvdata(component);
	wcd938x_priv->notify_swr_dmic = enable;
	if (enable)
		return blocking_notifier_chain_register(&wcd938x_priv->notifier,
							nblock);
	else
		return blocking_notifier_chain_unregister(
				&wcd938x_priv->notifier, nblock);
}
EXPORT_SYMBOL(wcd938x_swr_dmic_register_notifier);

static int wcd938x_event_notify(struct notifier_block *block,
				unsigned long val,
				void *data)
{
	u16 event = (val & 0xffff);
	int ret = 0;
	struct wcd938x_priv *wcd938x = dev_get_drvdata((struct device *)data);
	struct snd_soc_component *component = wcd938x->component;
	struct wcd_mbhc *mbhc;

	switch (event) {
	case BOLERO_SLV_EVT_TX_CH_HOLD_CLEAR:
		if (test_bit(WCD_ADC1, &wcd938x->status_mask)) {
			snd_soc_component_update_bits(component,
					WCD938X_ANA_TX_CH2, 0x40, 0x00);
			set_bit(WCD_ADC1_MODE, &wcd938x->status_mask);
			clear_bit(WCD_ADC1, &wcd938x->status_mask);
		}
		if (test_bit(WCD_ADC2, &wcd938x->status_mask)) {
			snd_soc_component_update_bits(component,
					WCD938X_ANA_TX_CH2, 0x20, 0x00);
			set_bit(WCD_ADC2_MODE, &wcd938x->status_mask);
			clear_bit(WCD_ADC2, &wcd938x->status_mask);
		}
		if (test_bit(WCD_ADC3, &wcd938x->status_mask)) {
			snd_soc_component_update_bits(component,
					WCD938X_ANA_TX_CH4, 0x40, 0x00);
			set_bit(WCD_ADC3_MODE, &wcd938x->status_mask);
			clear_bit(WCD_ADC3, &wcd938x->status_mask);
		}
		if (test_bit(WCD_ADC4, &wcd938x->status_mask)) {
			snd_soc_component_update_bits(component,
					WCD938X_ANA_TX_CH4, 0x20, 0x00);
			set_bit(WCD_ADC4_MODE, &wcd938x->status_mask);
			clear_bit(WCD_ADC4, &wcd938x->status_mask);
		}
		break;
	case BOLERO_SLV_EVT_PA_OFF_PRE_SSR:
		snd_soc_component_update_bits(component, WCD938X_ANA_HPH,
					0xC0, 0x00);
		snd_soc_component_update_bits(component, WCD938X_ANA_EAR,
					0x80, 0x00);
		snd_soc_component_update_bits(component, WCD938X_AUX_AUXPA,
					0x80, 0x00);
		break;
	case BOLERO_SLV_EVT_SSR_DOWN:
		wcd938x->dev_up = false;
		if(wcd938x->notify_swr_dmic)
			blocking_notifier_call_chain(&wcd938x->notifier,
						     WCD938X_EVT_SSR_DOWN,
						     NULL);
		wcd938x->mbhc->wcd_mbhc.deinit_in_progress = true;
		mbhc = &wcd938x->mbhc->wcd_mbhc;
		wcd938x->usbc_hs_status = get_usbc_hs_status(component,
						mbhc->mbhc_cfg);
		wcd938x_mbhc_ssr_down(wcd938x->mbhc, component);
		wcd938x_reset_low(wcd938x->dev);
		break;
	case BOLERO_SLV_EVT_SSR_UP:
		wcd938x_reset(wcd938x->dev);
		/* allow reset to take effect */
		usleep_range(10000, 10010);

		wcd938x_get_logical_addr(wcd938x->tx_swr_dev);
		wcd938x_get_logical_addr(wcd938x->rx_swr_dev);

		wcd938x_init_reg(component);
		regcache_mark_dirty(wcd938x->regmap);
		regcache_sync(wcd938x->regmap);
		/* Initialize MBHC module */
		mbhc = &wcd938x->mbhc->wcd_mbhc;
		ret = wcd938x_mbhc_post_ssr_init(wcd938x->mbhc, component);
		if (ret) {
			dev_err(component->dev, "%s: mbhc initialization failed\n",
				__func__);
		} else {
			wcd938x_mbhc_hs_detect(component, mbhc->mbhc_cfg);
		}
		wcd938x->mbhc->wcd_mbhc.deinit_in_progress = false;
		wcd938x->dev_up = true;
		if(wcd938x->notify_swr_dmic)
			blocking_notifier_call_chain(&wcd938x->notifier,
						     WCD938X_EVT_SSR_UP,
						     NULL);
		if (wcd938x->usbc_hs_status)
			mdelay(500);
		break;
	case BOLERO_SLV_EVT_CLK_NOTIFY:
		snd_soc_component_update_bits(component,
				WCD938X_DIGITAL_TOP_CLK_CFG, 0x06,
				((val >> 0x10) << 0x01));
		break;
	default:
		dev_dbg(component->dev, "%s: invalid event %d\n", __func__, event);
		break;
	}
	return 0;
}

static int __wcd938x_codec_enable_micbias(struct snd_soc_dapm_widget *w,
					  int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	int micb_num;

	dev_dbg(component->dev, "%s: wname: %s, event: %d\n",
		__func__, w->name, event);

	if (strnstr(w->name, "MIC BIAS1", sizeof("MIC BIAS1")))
		micb_num = MIC_BIAS_1;
	else if (strnstr(w->name, "MIC BIAS2", sizeof("MIC BIAS2")))
		micb_num = MIC_BIAS_2;
	else if (strnstr(w->name, "MIC BIAS3", sizeof("MIC BIAS3")))
		micb_num = MIC_BIAS_3;
	else if (strnstr(w->name, "MIC BIAS4", sizeof("MIC BIAS4")))
		micb_num = MIC_BIAS_4;
	else
		return -EINVAL;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd938x_micbias_control(component, micb_num,
						MICB_ENABLE, true);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* 1 msec delay as per HW requirement */
		usleep_range(1000, 1100);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd938x_micbias_control(component, micb_num,
						MICB_DISABLE, true);
		break;
	};

	return 0;

}

static int wcd938x_codec_enable_micbias(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	return __wcd938x_codec_enable_micbias(w, event);
}

static int __wcd938x_codec_enable_micbias_pullup(struct snd_soc_dapm_widget *w,
						 int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	int micb_num;

	dev_dbg(component->dev, "%s: wname: %s, event: %d\n",
		__func__, w->name, event);

	if (strnstr(w->name, "VA MIC BIAS1", sizeof("VA MIC BIAS1")))
		micb_num = MIC_BIAS_1;
	else if (strnstr(w->name, "VA MIC BIAS2", sizeof("VA MIC BIAS2")))
		micb_num = MIC_BIAS_2;
	else if (strnstr(w->name, "VA MIC BIAS3", sizeof("VA MIC BIAS3")))
		micb_num = MIC_BIAS_3;
	else if (strnstr(w->name, "VA MIC BIAS4", sizeof("VA MIC BIAS4")))
		micb_num = MIC_BIAS_4;
	else
		return -EINVAL;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd938x_micbias_control(component, micb_num,
					MICB_PULLUP_ENABLE, true);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* 1 msec delay as per HW requirement */
		usleep_range(1000, 1100);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd938x_micbias_control(component, micb_num,
					MICB_PULLUP_DISABLE, true);
		break;
	};

	return 0;

}

static int wcd938x_codec_enable_micbias_pullup(struct snd_soc_dapm_widget *w,
					       struct snd_kcontrol *kcontrol,
					       int event)
{
	return __wcd938x_codec_enable_micbias_pullup(w, event);
}

static int wcd938x_wakeup(void *handle, bool enable)
{
	struct wcd938x_priv *priv;
	int ret = 0;

	if (!handle) {
		pr_err("%s: NULL handle\n", __func__);
		return -EINVAL;
	}
	priv = (struct wcd938x_priv *)handle;
	if (!priv->tx_swr_dev) {
		pr_err("%s: tx swr dev is NULL\n", __func__);
		return -EINVAL;
	}
	mutex_lock(&priv->wakeup_lock);
	if (enable)
		ret = swr_device_wakeup_vote(priv->tx_swr_dev);
	else
		ret = swr_device_wakeup_unvote(priv->tx_swr_dev);
	mutex_unlock(&priv->wakeup_lock);

	return ret;
}

static int wcd938x_codec_force_enable_micbias(struct snd_soc_dapm_widget *w,
					    struct snd_kcontrol *kcontrol,
					    int event)
{
	int ret = 0;
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd938x_wakeup(wcd938x, true);
		ret = __wcd938x_codec_enable_micbias(w, SND_SOC_DAPM_PRE_PMU);
		wcd938x_wakeup(wcd938x, false);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd938x_wakeup(wcd938x, true);
		ret = __wcd938x_codec_enable_micbias(w, SND_SOC_DAPM_POST_PMD);
		wcd938x_wakeup(wcd938x, false);
		break;
	}

	return ret;
}

static int wcd938x_enable_micbias(struct wcd938x_priv *wcd938x,
					int micb_num, int req)
{
	int micb_index = micb_num - 1;
	u16 micb_reg;

	if (NULL == wcd938x) {
		pr_err("%s: wcd938x private data is NULL\n", __func__);
		return -EINVAL;
	}

	switch (micb_num) {
	case MIC_BIAS_1:
		micb_reg = WCD938X_ANA_MICB1;
		break;
	case MIC_BIAS_2:
		micb_reg = WCD938X_ANA_MICB2;
		break;
	case MIC_BIAS_3:
		micb_reg = WCD938X_ANA_MICB3;
		break;
	case MIC_BIAS_4:
		micb_reg = WCD938X_ANA_MICB4;
		break;
	default:
		pr_err("%s: Invalid micbias number: %d\n", __func__, micb_num);
		return -EINVAL;
	};

	pr_debug("%s: req: %d micb_num: %d  micb_ref: %d pullup_ref: %d\n",
		__func__, req, micb_num, wcd938x->micb_ref[micb_index],
		wcd938x->pullup_ref[micb_index]);
	mutex_lock(&wcd938x->micb_lock);

	switch (req) {
	case MICB_ENABLE:
		wcd938x->micb_ref[micb_index]++;
		if (wcd938x->micb_ref[micb_index] == 1) {
			regmap_update_bits(wcd938x->regmap,
				WCD938X_DIGITAL_CDC_DIG_CLK_CTL, 0xE0, 0xE0);
			regmap_update_bits(wcd938x->regmap,
				WCD938X_DIGITAL_CDC_ANA_CLK_CTL, 0x10, 0x10);
			regmap_update_bits(wcd938x->regmap,
			       WCD938X_DIGITAL_CDC_ANA_TX_CLK_CTL, 0x01, 0x01);
			regmap_update_bits(wcd938x->regmap,
				WCD938X_MICB1_TEST_CTL_2, 0x01, 0x01);
			regmap_update_bits(wcd938x->regmap,
				WCD938X_MICB2_TEST_CTL_2, 0x01, 0x01);
			regmap_update_bits(wcd938x->regmap,
				WCD938X_MICB3_TEST_CTL_2, 0x01, 0x01);
			regmap_update_bits(wcd938x->regmap,
				WCD938X_MICB4_TEST_CTL_2, 0x01, 0x01);
			regmap_update_bits(wcd938x->regmap,
				micb_reg, 0xC0, 0x40);
			regmap_update_bits(wcd938x->regmap, micb_reg, 0x3F, 0x10);
		}
		break;
	case MICB_PULLUP_ENABLE:
		wcd938x->pullup_ref[micb_index]++;
		if ((wcd938x->pullup_ref[micb_index] == 1) &&
		    (wcd938x->micb_ref[micb_index] == 0))
			regmap_update_bits(wcd938x->regmap, micb_reg,
							0xC0, 0x80);
		break;
	case MICB_PULLUP_DISABLE:
		if (wcd938x->pullup_ref[micb_index] > 0)
			wcd938x->pullup_ref[micb_index]--;

		if ((wcd938x->pullup_ref[micb_index] == 0) &&
		    (wcd938x->micb_ref[micb_index] == 0))
			regmap_update_bits(wcd938x->regmap, micb_reg,
							0xC0, 0x00);
		break;
	case MICB_DISABLE:
		if (wcd938x->micb_ref[micb_index] > 0)
			wcd938x->micb_ref[micb_index]--;

		if ((wcd938x->micb_ref[micb_index] == 0) &&
		    (wcd938x->pullup_ref[micb_index] > 0))
			regmap_update_bits(wcd938x->regmap, micb_reg,
							0xC0, 0x80);
		else if ((wcd938x->micb_ref[micb_index] == 0) &&
			 (wcd938x->pullup_ref[micb_index] == 0))
			regmap_update_bits(wcd938x->regmap, micb_reg,
							0xC0, 0x00);
		break;
	};

	mutex_unlock(&wcd938x->micb_lock);
	return 0;
}

int wcd938x_codec_force_enable_micbias_v2(struct snd_soc_component *component,
					int event, int micb_num)
{
	struct wcd938x_priv *wcd938x_priv = NULL;
	int ret = 0;
	int micb_index = micb_num - 1;

	if(NULL == component) {
		pr_err("%s: wcd938x component is NULL\n", __func__);
		return -EINVAL;
	}
	if(event != SND_SOC_DAPM_PRE_PMU && event != SND_SOC_DAPM_POST_PMD) {
		pr_err("%s: invalid event: %d\n", __func__, event);
		return -EINVAL;
	}
	if(micb_num < MIC_BIAS_1 || micb_num > MIC_BIAS_4) {
		pr_err("%s: invalid mic bias num: %d\n", __func__, micb_num);
		return -EINVAL;
	}

	wcd938x_priv = snd_soc_component_get_drvdata(component);

	if (!wcd938x_priv->dev_up) {
		if ((wcd938x_priv->pullup_ref[micb_index] > 0) &&
			(event == SND_SOC_DAPM_POST_PMD)) {
			wcd938x_priv->pullup_ref[micb_index]--;
			ret = -ENODEV;
			goto done;
		}
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd938x_wakeup(wcd938x_priv, true);
		wcd938x_enable_micbias(wcd938x_priv, micb_num, MICB_PULLUP_ENABLE);
		wcd938x_wakeup(wcd938x_priv, false);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd938x_wakeup(wcd938x_priv, true);
		wcd938x_enable_micbias(wcd938x_priv, micb_num, MICB_PULLUP_DISABLE);
		wcd938x_wakeup(wcd938x_priv, false);
		break;
	}

done:
	return ret;
}
EXPORT_SYMBOL(wcd938x_codec_force_enable_micbias_v2);

static inline int wcd938x_tx_path_get(const char *wname,
				      unsigned int *path_num)
{
	int ret = 0;
	char *widget_name = NULL;
	char *w_name = NULL;
	char *path_num_char = NULL;
	char *path_name = NULL;

	widget_name = kstrndup(wname, 9, GFP_KERNEL);
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
	path_num_char = strpbrk(path_name, "0123");
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

static int wcd938x_tx_mode_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct wcd938x_priv *wcd938x = NULL;
	int ret = 0;
	unsigned int path = 0;

	if (!component)
		return -EINVAL;

	wcd938x = snd_soc_component_get_drvdata(component);

	if (!wcd938x)
		return -EINVAL;

	ret = wcd938x_tx_path_get(kcontrol->id.name, &path);
	if (ret < 0)
		return ret;

	ucontrol->value.integer.value[0] = wcd938x->tx_mode[path];

	return 0;
}

static int wcd938x_tx_mode_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct wcd938x_priv *wcd938x = NULL;
	u32 mode_val;
	unsigned int path = 0;
	int ret = 0;

	if (!component)
		return -EINVAL;

	wcd938x  = snd_soc_component_get_drvdata(component);

	if (!wcd938x)
		return -EINVAL;

	ret = wcd938x_tx_path_get(kcontrol->id.name, &path);
	if (ret)
		return ret;

	mode_val = ucontrol->value.enumerated.item[0];

	dev_dbg(component->dev, "%s: mode: %d\n", __func__, mode_val);

	wcd938x->tx_mode[path] = mode_val;

	return 0;
}

static int wcd938x_rx_hph_mode_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = wcd938x->hph_mode;
	return 0;
}

static int wcd938x_rx_hph_mode_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	u32 mode_val;

	mode_val = ucontrol->value.enumerated.item[0];

	dev_dbg(component->dev, "%s: mode: %d\n", __func__, mode_val);

	if (wcd938x->variant == WCD9380) {
		if (mode_val == CLS_H_HIFI || mode_val == CLS_AB_HIFI) {
			dev_info(component->dev,
				"%s:Invalid HPH Mode, default to CLS_H_ULP\n",
				__func__);
			mode_val = CLS_H_ULP;
		}
	}
	if (mode_val == CLS_H_NORMAL) {
		dev_info(component->dev,
			"%s:Invalid HPH Mode, default to class_AB\n",
			__func__);
		mode_val = CLS_H_ULP;
	}
	wcd938x->hph_mode = mode_val;
	return 0;
}

static int wcd938x_ear_pa_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain = 0;
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);

	ear_pa_gain = snd_soc_component_read32(component,
				WCD938X_ANA_EAR_COMPANDER_CTL);

	ear_pa_gain = (ear_pa_gain & 0x7C) >> 2;

	ucontrol->value.integer.value[0] = ear_pa_gain;

	dev_dbg(component->dev, "%s: ear_pa_gain = 0x%x\n", __func__,
		ear_pa_gain);

	return 0;
}

static int wcd938x_ear_pa_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain = 0;
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
			__func__, ucontrol->value.integer.value[0]);

	ear_pa_gain =  ucontrol->value.integer.value[0] << 2;

	if (!wcd938x->comp1_enable) {
		snd_soc_component_update_bits(component,
				WCD938X_ANA_EAR_COMPANDER_CTL,
				0x7C, ear_pa_gain);
	}

	return 0;
}

static int wcd938x_get_compander(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	bool hphr;
	struct soc_multi_mixer_control *mc;

	mc = (struct soc_multi_mixer_control *)(kcontrol->private_value);
	hphr = mc->shift;

	ucontrol->value.integer.value[0] = hphr ? wcd938x->comp2_enable :
						wcd938x->comp1_enable;
	return 0;
}

static int wcd938x_set_compander(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	int value = ucontrol->value.integer.value[0];
	bool hphr;
	struct soc_multi_mixer_control *mc;

	mc = (struct soc_multi_mixer_control *)(kcontrol->private_value);
	hphr = mc->shift;
	if (hphr)
		wcd938x->comp2_enable = value;
	else
		wcd938x->comp1_enable = value;

	return 0;
}

static int wcd938x_codec_enable_vdd_buck(struct snd_soc_dapm_widget *w,
					 struct snd_kcontrol *kcontrol,
					 int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	struct wcd938x_pdata *pdata = NULL;
	int ret = 0;

	pdata = dev_get_platdata(wcd938x->dev);

	if (!pdata) {
		dev_err(component->dev, "%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}

	if (!msm_cdc_is_ondemand_supply(wcd938x->dev,
					wcd938x->supplies,
					pdata->regulator,
					pdata->num_supplies,
					"cdc-vdd-buck"))
		return 0;

	dev_dbg(component->dev, "%s wname: %s event: %d\n", __func__,
		w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (test_bit(ALLOW_BUCK_DISABLE, &wcd938x->status_mask)) {
			dev_dbg(component->dev,
				"%s: buck already in enabled state\n",
				__func__);
			clear_bit(ALLOW_BUCK_DISABLE, &wcd938x->status_mask);
			return 0;
		}
		ret = msm_cdc_enable_ondemand_supply(wcd938x->dev,
						wcd938x->supplies,
						pdata->regulator,
						pdata->num_supplies,
						"cdc-vdd-buck");
		if (ret == -EINVAL) {
			dev_err(component->dev, "%s: vdd buck is not enabled\n",
				__func__);
			return ret;
		}
		clear_bit(ALLOW_BUCK_DISABLE, &wcd938x->status_mask);
		/*
		 * 200us sleep is required after LDO is enabled as per
		 * HW requirement
		 */
		usleep_range(200, 250);
		break;
	case SND_SOC_DAPM_POST_PMD:
		set_bit(ALLOW_BUCK_DISABLE, &wcd938x->status_mask);
		break;
	}
	return 0;
}


static int wcd938x_ldoh_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = wcd938x->ldoh;

	return 0;
}

static int wcd938x_ldoh_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	wcd938x->ldoh = ucontrol->value.integer.value[0];

	return 0;
}

const char * const tx_master_ch_text[] = {
	"ZERO", "SWRM_TX1_CH1", "SWRM_TX1_CH2", "SWRM_TX1_CH3", "SWRM_TX1_CH4",
	"SWRM_TX2_CH1", "SWRM_TX2_CH2", "SWRM_TX2_CH3", "SWRM_TX2_CH4",
	"SWRM_TX3_CH1", "SWRM_TX3_CH2", "SWRM_TX3_CH3", "SWRM_TX3_CH4",
	"SWRM_PCM_IN",
};

const struct soc_enum tx_master_ch_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tx_master_ch_text),
					tx_master_ch_text);

static void wcd938x_tx_get_slave_ch_type_idx(const char *wname, int *ch_idx)
{
	u8 ch_type = 0;

	if (strnstr(wname, "ADC1", sizeof("ADC1")))
		ch_type = ADC1;
	else if (strnstr(wname, "ADC2", sizeof("ADC2")))
		ch_type = ADC2;
	else if (strnstr(wname, "ADC3", sizeof("ADC3")))
		ch_type = ADC3;
	else if (strnstr(wname, "ADC4", sizeof("ADC4")))
		ch_type = ADC4;
	else if (strnstr(wname, "DMIC0", sizeof("DMIC0")))
		ch_type = DMIC0;
	else if (strnstr(wname, "DMIC1", sizeof("DMIC1")))
		ch_type = DMIC1;
	else if (strnstr(wname, "MBHC", sizeof("MBHC")))
		ch_type = MBHC;
	else if (strnstr(wname, "DMIC2", sizeof("DMIC2")))
		ch_type = DMIC2;
	else if (strnstr(wname, "DMIC3", sizeof("DMIC3")))
		ch_type = DMIC3;
	else if (strnstr(wname, "DMIC4", sizeof("DMIC4")))
		ch_type = DMIC4;
	else if (strnstr(wname, "DMIC5", sizeof("DMIC5")))
		ch_type = DMIC5;
	else if (strnstr(wname, "DMIC6", sizeof("DMIC6")))
		ch_type = DMIC6;
	else if (strnstr(wname, "DMIC7", sizeof("DMIC7")))
		ch_type = DMIC7;
	else
		pr_err("%s: port name: %s is not listed\n", __func__, wname);

	if (ch_type)
		*ch_idx = wcd938x_slave_get_slave_ch_val(ch_type);
	else
		*ch_idx = -EINVAL;
}

static int wcd938x_tx_master_ch_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct wcd938x_priv *wcd938x = NULL;
	int slave_ch_idx = -EINVAL;

	if (component == NULL)
		return -EINVAL;

	wcd938x = snd_soc_component_get_drvdata(component);
	if (wcd938x == NULL)
		return -EINVAL;

	wcd938x_tx_get_slave_ch_type_idx(kcontrol->id.name, &slave_ch_idx);
	if (slave_ch_idx < 0 || slave_ch_idx >= WCD938X_MAX_SLAVE_CH_TYPES)
		return -EINVAL;

	ucontrol->value.integer.value[0] = wcd938x_slave_get_master_ch_val(
			wcd938x->tx_master_ch_map[slave_ch_idx]);

	return 0;
}

static int wcd938x_tx_master_ch_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct wcd938x_priv *wcd938x = NULL;
	int slave_ch_idx = -EINVAL, idx = 0;

	if (component == NULL)
		return -EINVAL;

	wcd938x = snd_soc_component_get_drvdata(component);
	if (wcd938x == NULL)
		return -EINVAL;

	wcd938x_tx_get_slave_ch_type_idx(kcontrol->id.name, &slave_ch_idx);

	if (slave_ch_idx < 0 || slave_ch_idx >= WCD938X_MAX_SLAVE_CH_TYPES)
		return -EINVAL;

	dev_dbg(component->dev, "%s: slave_ch_idx: %d", __func__, slave_ch_idx);
	dev_dbg(component->dev, "%s: ucontrol->value.enumerated.item[0] = %ld\n",
			__func__, ucontrol->value.enumerated.item[0]);

	idx = ucontrol->value.enumerated.item[0];
	if (idx < 0 || idx >= ARRAY_SIZE(swr_master_ch_map))
		return -EINVAL;

	wcd938x->tx_master_ch_map[slave_ch_idx] = wcd938x_slave_get_master_ch(idx);
	return 0;
}

static int wcd938x_bcs_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = wcd938x->bcs_dis;

	return 0;
}

static int wcd938x_bcs_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	wcd938x->bcs_dis = ucontrol->value.integer.value[0];

	return 0;
}

static const char * const tx_mode_mux_text_wcd9380[] = {
	"ADC_INVALID", "ADC_HIFI", "ADC_LO_HIF", "ADC_NORMAL", "ADC_LP",
};

static const struct soc_enum tx_mode_mux_enum_wcd9380 =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tx_mode_mux_text_wcd9380),
			    tx_mode_mux_text_wcd9380);

static const char * const tx_mode_mux_text[] = {
	"ADC_INVALID", "ADC_HIFI", "ADC_LO_HIF", "ADC_NORMAL", "ADC_LP",
	"ADC_ULP1", "ADC_ULP2",
};

static const struct soc_enum tx_mode_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tx_mode_mux_text),
			    tx_mode_mux_text);

static const char * const rx_hph_mode_mux_text_wcd9380[] = {
	"CLS_H_INVALID", "CLS_H_INVALID_1", "CLS_H_LP", "CLS_AB",
	"CLS_H_LOHIFI", "CLS_H_ULP", "CLS_H_INVALID_2", "CLS_AB_LP",
	"CLS_AB_LOHIFI",
};

static const char * const wcd938x_ear_pa_gain_text[] = {
	"G_6_DB", "G_4P5_DB", "G_3_DB", "G_1P5_DB", "G_0_DB",
	"G_M1P5_DB", "G_M3_DB", "G_M4P5_DB",
	"G_M6_DB", "G_7P5_DB", "G_M9_DB",
	"G_M10P5_DB", "G_M12_DB", "G_M13P5_DB",
	"G_M15_DB", "G_M16P5_DB", "G_M18_DB",
};

static const struct soc_enum rx_hph_mode_mux_enum_wcd9380 =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rx_hph_mode_mux_text_wcd9380),
			    rx_hph_mode_mux_text_wcd9380);

static SOC_ENUM_SINGLE_EXT_DECL(wcd938x_ear_pa_gain_enum,
				wcd938x_ear_pa_gain_text);

static const char * const rx_hph_mode_mux_text[] = {
	"CLS_H_INVALID", "CLS_H_HIFI", "CLS_H_LP", "CLS_AB", "CLS_H_LOHIFI",
	"CLS_H_ULP", "CLS_AB_HIFI", "CLS_AB_LP", "CLS_AB_LOHIFI",
};

static const struct soc_enum rx_hph_mode_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rx_hph_mode_mux_text),
			    rx_hph_mode_mux_text);

static const struct snd_kcontrol_new wcd9380_snd_controls[] = {
	SOC_ENUM_EXT("EAR PA GAIN", wcd938x_ear_pa_gain_enum,
		wcd938x_ear_pa_gain_get, wcd938x_ear_pa_gain_put),

	SOC_ENUM_EXT("RX HPH Mode", rx_hph_mode_mux_enum_wcd9380,
		wcd938x_rx_hph_mode_get, wcd938x_rx_hph_mode_put),

	SOC_ENUM_EXT("TX0 MODE", tx_mode_mux_enum_wcd9380,
			wcd938x_tx_mode_get, wcd938x_tx_mode_put),
	SOC_ENUM_EXT("TX1 MODE", tx_mode_mux_enum_wcd9380,
			wcd938x_tx_mode_get, wcd938x_tx_mode_put),
	SOC_ENUM_EXT("TX2 MODE", tx_mode_mux_enum_wcd9380,
			wcd938x_tx_mode_get, wcd938x_tx_mode_put),
	SOC_ENUM_EXT("TX3 MODE", tx_mode_mux_enum_wcd9380,
			wcd938x_tx_mode_get, wcd938x_tx_mode_put),
};

static const struct snd_kcontrol_new wcd9385_snd_controls[] = {
	SOC_ENUM_EXT("RX HPH Mode", rx_hph_mode_mux_enum,
		wcd938x_rx_hph_mode_get, wcd938x_rx_hph_mode_put),

	SOC_ENUM_EXT("TX0 MODE", tx_mode_mux_enum,
			wcd938x_tx_mode_get, wcd938x_tx_mode_put),
	SOC_ENUM_EXT("TX1 MODE", tx_mode_mux_enum,
			wcd938x_tx_mode_get, wcd938x_tx_mode_put),
	SOC_ENUM_EXT("TX2 MODE", tx_mode_mux_enum,
			wcd938x_tx_mode_get, wcd938x_tx_mode_put),
	SOC_ENUM_EXT("TX3 MODE", tx_mode_mux_enum,
			wcd938x_tx_mode_get, wcd938x_tx_mode_put),
};

static const struct snd_kcontrol_new wcd938x_snd_controls[] = {
	SOC_SINGLE_EXT("HPHL_COMP Switch", SND_SOC_NOPM, 0, 1, 0,
		wcd938x_get_compander, wcd938x_set_compander),
	SOC_SINGLE_EXT("HPHR_COMP Switch", SND_SOC_NOPM, 1, 1, 0,
		wcd938x_get_compander, wcd938x_set_compander),
	SOC_SINGLE_EXT("LDOH Enable", SND_SOC_NOPM, 0, 1, 0,
		wcd938x_ldoh_get, wcd938x_ldoh_put),

	SOC_SINGLE_EXT("ADC2_BCS Disable", SND_SOC_NOPM, 0, 1, 0,
		wcd938x_bcs_get, wcd938x_bcs_put),

	SOC_SINGLE_TLV("HPHL Volume", WCD938X_HPH_L_EN, 0, 20, 1, line_gain),
	SOC_SINGLE_TLV("HPHR Volume", WCD938X_HPH_R_EN, 0, 20, 1, line_gain),
	SOC_SINGLE_TLV("ADC1 Volume", WCD938X_ANA_TX_CH1, 0, 20, 0,
			analog_gain),
	SOC_SINGLE_TLV("ADC2 Volume", WCD938X_ANA_TX_CH2, 0, 20, 0,
			analog_gain),
	SOC_SINGLE_TLV("ADC3 Volume", WCD938X_ANA_TX_CH3, 0, 20, 0,
			analog_gain),
	SOC_SINGLE_TLV("ADC4 Volume", WCD938X_ANA_TX_CH4, 0, 20, 0,
			analog_gain),

	SOC_ENUM_EXT("ADC1 ChMap", tx_master_ch_enum,
			wcd938x_tx_master_ch_get, wcd938x_tx_master_ch_put),
	SOC_ENUM_EXT("ADC2 ChMap", tx_master_ch_enum,
			wcd938x_tx_master_ch_get, wcd938x_tx_master_ch_put),
	SOC_ENUM_EXT("ADC3 ChMap", tx_master_ch_enum,
			wcd938x_tx_master_ch_get, wcd938x_tx_master_ch_put),
	SOC_ENUM_EXT("ADC4 ChMap", tx_master_ch_enum,
			wcd938x_tx_master_ch_get, wcd938x_tx_master_ch_put),
	SOC_ENUM_EXT("DMIC0 ChMap", tx_master_ch_enum,
			wcd938x_tx_master_ch_get, wcd938x_tx_master_ch_put),
	SOC_ENUM_EXT("DMIC1 ChMap", tx_master_ch_enum,
			wcd938x_tx_master_ch_get, wcd938x_tx_master_ch_put),
	SOC_ENUM_EXT("MBHC ChMap", tx_master_ch_enum,
			wcd938x_tx_master_ch_get, wcd938x_tx_master_ch_put),
	SOC_ENUM_EXT("DMIC2 ChMap", tx_master_ch_enum,
			wcd938x_tx_master_ch_get, wcd938x_tx_master_ch_put),
	SOC_ENUM_EXT("DMIC3 ChMap", tx_master_ch_enum,
			wcd938x_tx_master_ch_get, wcd938x_tx_master_ch_put),
	SOC_ENUM_EXT("DMIC4 ChMap", tx_master_ch_enum,
			wcd938x_tx_master_ch_get, wcd938x_tx_master_ch_put),
	SOC_ENUM_EXT("DMIC5 ChMap", tx_master_ch_enum,
			wcd938x_tx_master_ch_get, wcd938x_tx_master_ch_put),
	SOC_ENUM_EXT("DMIC6 ChMap", tx_master_ch_enum,
			wcd938x_tx_master_ch_get, wcd938x_tx_master_ch_put),
	SOC_ENUM_EXT("DMIC7 ChMap", tx_master_ch_enum,
			wcd938x_tx_master_ch_get, wcd938x_tx_master_ch_put),
};

static const struct snd_kcontrol_new adc1_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new adc2_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new adc3_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new adc4_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new dmic1_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new dmic2_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new dmic3_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new dmic4_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new dmic5_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new dmic6_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new dmic7_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new dmic8_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new ear_rdac_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new aux_rdac_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new hphl_rdac_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new hphr_rdac_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const char * const adc2_mux_text[] = {
	"INP2", "INP3"
};

static const struct soc_enum adc2_enum =
	SOC_ENUM_SINGLE(WCD938X_TX_NEW_AMIC_MUX_CFG, 7,
		ARRAY_SIZE(adc2_mux_text), adc2_mux_text);

static const struct snd_kcontrol_new tx_adc2_mux =
	SOC_DAPM_ENUM("ADC2 MUX Mux", adc2_enum);

static const char * const adc3_mux_text[] = {
	"INP4", "INP6"
};

static const struct soc_enum adc3_enum =
	SOC_ENUM_SINGLE(WCD938X_TX_NEW_AMIC_MUX_CFG, 6,
		ARRAY_SIZE(adc3_mux_text), adc3_mux_text);

static const struct snd_kcontrol_new tx_adc3_mux =
	SOC_DAPM_ENUM("ADC3 MUX Mux", adc3_enum);

static const char * const adc4_mux_text[] = {
	"INP5", "INP7"
};

static const struct soc_enum adc4_enum =
	SOC_ENUM_SINGLE(WCD938X_TX_NEW_AMIC_MUX_CFG, 5,
		ARRAY_SIZE(adc4_mux_text), adc4_mux_text);

static const struct snd_kcontrol_new tx_adc4_mux =
	SOC_DAPM_ENUM("ADC4 MUX Mux", adc4_enum);

static const char * const rdac3_mux_text[] = {
	"RX1", "RX3"
};

static const char * const hdr12_mux_text[] = {
	"NO_HDR12", "HDR12"
};

static const struct soc_enum hdr12_enum =
	SOC_ENUM_SINGLE(WCD938X_TX_NEW_AMIC_MUX_CFG, 4,
		ARRAY_SIZE(hdr12_mux_text), hdr12_mux_text);

static const struct snd_kcontrol_new tx_hdr12_mux =
	SOC_DAPM_ENUM("HDR12 MUX Mux", hdr12_enum);

static const char * const hdr34_mux_text[] = {
	"NO_HDR34", "HDR34"
};

static const struct soc_enum hdr34_enum =
	SOC_ENUM_SINGLE(WCD938X_TX_NEW_AMIC_MUX_CFG, 3,
		ARRAY_SIZE(hdr34_mux_text), hdr34_mux_text);

static const struct snd_kcontrol_new tx_hdr34_mux =
	SOC_DAPM_ENUM("HDR34 MUX Mux", hdr34_enum);

static const struct soc_enum rdac3_enum =
	SOC_ENUM_SINGLE(WCD938X_DIGITAL_CDC_EAR_PATH_CTL, 0,
		ARRAY_SIZE(rdac3_mux_text), rdac3_mux_text);

static const struct snd_kcontrol_new rx_rdac3_mux =
	SOC_DAPM_ENUM("RDAC3_MUX Mux", rdac3_enum);

static const struct snd_soc_dapm_widget wcd938x_dapm_widgets[] = {

	/*input widgets*/
	SND_SOC_DAPM_INPUT("AMIC1"),
	SND_SOC_DAPM_INPUT("AMIC2"),
	SND_SOC_DAPM_INPUT("AMIC3"),
	SND_SOC_DAPM_INPUT("AMIC4"),
	SND_SOC_DAPM_INPUT("AMIC5"),
	SND_SOC_DAPM_INPUT("AMIC6"),
	SND_SOC_DAPM_INPUT("AMIC7"),

	SND_SOC_DAPM_INPUT("IN1_HPHL"),
	SND_SOC_DAPM_INPUT("IN2_HPHR"),
	SND_SOC_DAPM_INPUT("IN3_AUX"),
	/*
	 * These dummy widgets are null connected to WCD938x dapm input and
	 * output widgets which are not actual path endpoints. This ensures
	 * dapm doesnt set these dapm input and output widgets as endpoints.
	 */
	SND_SOC_DAPM_INPUT("WCD_TX_DUMMY"),
	SND_SOC_DAPM_OUTPUT("WCD_RX_DUMMY"),

	/*tx widgets*/
	SND_SOC_DAPM_ADC_E("ADC1", NULL, SND_SOC_NOPM, 0, 0,
				wcd938x_codec_enable_adc,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC2", NULL, SND_SOC_NOPM, 1, 0,
				wcd938x_codec_enable_adc,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC3", NULL, SND_SOC_NOPM, 2, 0,
				wcd938x_codec_enable_adc,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC4", NULL, SND_SOC_NOPM, 3, 0,
				wcd938x_codec_enable_adc,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC1", NULL, SND_SOC_NOPM, 0, 0,
				wcd938x_codec_enable_dmic,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC2", NULL, SND_SOC_NOPM, 1, 0,
				wcd938x_codec_enable_dmic,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC3", NULL, SND_SOC_NOPM, 2, 0,
				wcd938x_codec_enable_dmic,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC4", NULL, SND_SOC_NOPM, 3, 0,
				wcd938x_codec_enable_dmic,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC5", NULL, SND_SOC_NOPM, 4, 0,
				wcd938x_codec_enable_dmic,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC6", NULL, SND_SOC_NOPM, 5, 0,
				wcd938x_codec_enable_dmic,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC7", NULL, SND_SOC_NOPM, 6, 0,
				wcd938x_codec_enable_dmic,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC8", NULL, SND_SOC_NOPM, 7, 0,
				wcd938x_codec_enable_dmic,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER_E("ADC1 REQ", SND_SOC_NOPM, 0, 0,
				NULL, 0, wcd938x_enable_req,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("ADC2 REQ", SND_SOC_NOPM, 1, 0,
				NULL, 0, wcd938x_enable_req,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("ADC3 REQ", SND_SOC_NOPM, 2, 0,
				NULL, 0, wcd938x_enable_req,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("ADC4 REQ", SND_SOC_NOPM, 3, 0,
				NULL, 0, wcd938x_enable_req,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("ADC2 MUX", SND_SOC_NOPM, 0, 0,
				&tx_adc2_mux),
	SND_SOC_DAPM_MUX("ADC3 MUX", SND_SOC_NOPM, 0, 0,
				&tx_adc3_mux),
	SND_SOC_DAPM_MUX("ADC4 MUX", SND_SOC_NOPM, 0, 0,
				&tx_adc4_mux),
	SND_SOC_DAPM_MUX("HDR12 MUX", SND_SOC_NOPM, 0, 0,
				&tx_hdr12_mux),
	SND_SOC_DAPM_MUX("HDR34 MUX", SND_SOC_NOPM, 0, 0,
				&tx_hdr34_mux),
	/*tx mixers*/
	SND_SOC_DAPM_MIXER_E("ADC1_MIXER", SND_SOC_NOPM, ADC1, 0,
				adc1_switch, ARRAY_SIZE(adc1_switch),
				wcd938x_tx_swr_ctrl, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("ADC2_MIXER", SND_SOC_NOPM, ADC2, 0,
				adc2_switch, ARRAY_SIZE(adc2_switch),
				wcd938x_tx_swr_ctrl, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("ADC3_MIXER", SND_SOC_NOPM, ADC3, 0, adc3_switch,
				ARRAY_SIZE(adc3_switch), wcd938x_tx_swr_ctrl,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("ADC4_MIXER", SND_SOC_NOPM, ADC4, 0, adc4_switch,
				ARRAY_SIZE(adc4_switch), wcd938x_tx_swr_ctrl,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("DMIC1_MIXER", SND_SOC_NOPM, DMIC1,
				0, dmic1_switch, ARRAY_SIZE(dmic1_switch),
				wcd938x_tx_swr_ctrl, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("DMIC2_MIXER", SND_SOC_NOPM, DMIC2,
				0, dmic2_switch, ARRAY_SIZE(dmic2_switch),
				wcd938x_tx_swr_ctrl, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("DMIC3_MIXER", SND_SOC_NOPM, DMIC3,
				0, dmic3_switch, ARRAY_SIZE(dmic3_switch),
				wcd938x_tx_swr_ctrl, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("DMIC4_MIXER", SND_SOC_NOPM, DMIC4,
				0, dmic4_switch, ARRAY_SIZE(dmic4_switch),
				wcd938x_tx_swr_ctrl, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("DMIC5_MIXER", SND_SOC_NOPM, DMIC5,
				0, dmic5_switch, ARRAY_SIZE(dmic5_switch),
				wcd938x_tx_swr_ctrl, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("DMIC6_MIXER", SND_SOC_NOPM, DMIC6,
				0, dmic6_switch, ARRAY_SIZE(dmic6_switch),
				wcd938x_tx_swr_ctrl, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("DMIC7_MIXER", SND_SOC_NOPM, DMIC7,
				0, dmic7_switch, ARRAY_SIZE(dmic7_switch),
				wcd938x_tx_swr_ctrl, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("DMIC8_MIXER", SND_SOC_NOPM, DMIC8,
				0, dmic8_switch, ARRAY_SIZE(dmic8_switch),
				wcd938x_tx_swr_ctrl, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),
	/* micbias widgets*/
	SND_SOC_DAPM_SUPPLY("MIC BIAS1", SND_SOC_NOPM, 0, 0,
				wcd938x_codec_enable_micbias,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("MIC BIAS2", SND_SOC_NOPM, 0, 0,
				wcd938x_codec_enable_micbias,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("MIC BIAS3", SND_SOC_NOPM, 0, 0,
				wcd938x_codec_enable_micbias,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("MIC BIAS4", SND_SOC_NOPM, 0, 0,
				wcd938x_codec_enable_micbias,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY(DAPM_MICBIAS1_STANDALONE, SND_SOC_NOPM, 0, 0,
		wcd938x_codec_force_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY(DAPM_MICBIAS2_STANDALONE, SND_SOC_NOPM, 0, 0,
		wcd938x_codec_force_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY(DAPM_MICBIAS3_STANDALONE, SND_SOC_NOPM, 0, 0,
		wcd938x_codec_force_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY(DAPM_MICBIAS4_STANDALONE, SND_SOC_NOPM, 0, 0,
		wcd938x_codec_force_enable_micbias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("VDD_BUCK", SND_SOC_NOPM, 0, 0,
			     wcd938x_codec_enable_vdd_buck,
			     SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("CLS_H_PORT", 1, SND_SOC_NOPM, 0, 0,
			     wcd938x_enable_clsh,
			     SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/*rx widgets*/
	SND_SOC_DAPM_PGA_E("EAR PGA", WCD938X_ANA_EAR, 7, 0, NULL, 0,
				wcd938x_codec_enable_ear_pa,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("AUX PGA", WCD938X_AUX_AUXPA, 7, 0, NULL, 0,
				wcd938x_codec_enable_aux_pa,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("HPHL PGA", WCD938X_ANA_HPH, 7, 0, NULL, 0,
				wcd938x_codec_enable_hphl_pa,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("HPHR PGA", WCD938X_ANA_HPH, 6, 0, NULL, 0,
				wcd938x_codec_enable_hphr_pa,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("RDAC1", NULL, SND_SOC_NOPM, 0, 0,
				wcd938x_codec_hphl_dac_event,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("RDAC2", NULL, SND_SOC_NOPM, 0, 0,
				wcd938x_codec_hphr_dac_event,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("RDAC3", NULL, SND_SOC_NOPM, 0, 0,
				wcd938x_codec_ear_dac_event,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("RDAC4", NULL, SND_SOC_NOPM, 0, 0,
				wcd938x_codec_aux_dac_event,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("RDAC3_MUX", SND_SOC_NOPM, 0, 0, &rx_rdac3_mux),

	SND_SOC_DAPM_MIXER_E("RX1", SND_SOC_NOPM, 0, 0, NULL, 0,
				wcd938x_enable_rx1, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX2", SND_SOC_NOPM, 0, 0, NULL, 0,
				wcd938x_enable_rx2, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX3", SND_SOC_NOPM, 0, 0, NULL, 0,
				wcd938x_enable_rx3, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD),

	/* rx mixer widgets*/

	SND_SOC_DAPM_MIXER("EAR_RDAC", SND_SOC_NOPM, 0, 0,
			   ear_rdac_switch, ARRAY_SIZE(ear_rdac_switch)),
	SND_SOC_DAPM_MIXER("AUX_RDAC", SND_SOC_NOPM, 0, 0,
			   aux_rdac_switch, ARRAY_SIZE(aux_rdac_switch)),
	SND_SOC_DAPM_MIXER("HPHL_RDAC", SND_SOC_NOPM, 0, 0,
			   hphl_rdac_switch, ARRAY_SIZE(hphl_rdac_switch)),
	SND_SOC_DAPM_MIXER("HPHR_RDAC", SND_SOC_NOPM, 0, 0,
			   hphr_rdac_switch, ARRAY_SIZE(hphr_rdac_switch)),

	/*output widgets tx*/
	SND_SOC_DAPM_OUTPUT("WCD_TX_OUTPUT"),

	/*output widgets rx*/
	SND_SOC_DAPM_OUTPUT("EAR"),
	SND_SOC_DAPM_OUTPUT("AUX"),
	SND_SOC_DAPM_OUTPUT("HPHL"),
	SND_SOC_DAPM_OUTPUT("HPHR"),

	/* micbias pull up widgets*/
	SND_SOC_DAPM_SUPPLY("VA MIC BIAS1", SND_SOC_NOPM, 0, 0,
				wcd938x_codec_enable_micbias_pullup,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("VA MIC BIAS2", SND_SOC_NOPM, 0, 0,
				wcd938x_codec_enable_micbias_pullup,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("VA MIC BIAS3", SND_SOC_NOPM, 0, 0,
				wcd938x_codec_enable_micbias_pullup,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("VA MIC BIAS4", SND_SOC_NOPM, 0, 0,
				wcd938x_codec_enable_micbias_pullup,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_route wcd938x_audio_map[] = {

	{"WCD_TX_DUMMY", NULL, "WCD_TX_OUTPUT"},
	{"WCD_TX_OUTPUT", NULL, "ADC1_MIXER"},
	{"ADC1_MIXER", "Switch", "ADC1 REQ"},
	{"ADC1 REQ", NULL, "ADC1"},
	{"ADC1", NULL, "AMIC1"},

	{"WCD_TX_OUTPUT", NULL, "ADC2_MIXER"},
	{"ADC2_MIXER", "Switch", "ADC2 REQ"},
	{"ADC2 REQ", NULL, "ADC2"},
	{"ADC2", NULL, "HDR12 MUX"},
	{"HDR12 MUX", "NO_HDR12", "ADC2 MUX"},
	{"HDR12 MUX", "HDR12", "AMIC1"},
	{"ADC2 MUX", "INP3", "AMIC3"},
	{"ADC2 MUX", "INP2", "AMIC2"},

	{"WCD_TX_OUTPUT", NULL, "ADC3_MIXER"},
	{"ADC3_MIXER", "Switch", "ADC3 REQ"},
	{"ADC3 REQ", NULL, "ADC3"},
	{"ADC3", NULL, "HDR34 MUX"},
	{"HDR34 MUX", "NO_HDR34", "ADC3 MUX"},
	{"HDR34 MUX", "HDR34", "AMIC5"},
	{"ADC3 MUX", "INP4", "AMIC4"},
	{"ADC3 MUX", "INP6", "AMIC6"},

	{"WCD_TX_OUTPUT", NULL, "ADC4_MIXER"},
	{"ADC4_MIXER", "Switch", "ADC4 REQ"},
	{"ADC4 REQ", NULL, "ADC4"},
	{"ADC4", NULL, "ADC4 MUX"},
	{"ADC4 MUX", "INP5", "AMIC5"},
	{"ADC4 MUX", "INP7", "AMIC7"},

	{"WCD_TX_OUTPUT", NULL, "DMIC1_MIXER"},
	{"DMIC1_MIXER", "Switch", "DMIC1"},

	{"WCD_TX_OUTPUT", NULL, "DMIC2_MIXER"},
	{"DMIC2_MIXER", "Switch", "DMIC2"},

	{"WCD_TX_OUTPUT", NULL, "DMIC3_MIXER"},
	{"DMIC3_MIXER", "Switch", "DMIC3"},

	{"WCD_TX_OUTPUT", NULL, "DMIC4_MIXER"},
	{"DMIC4_MIXER", "Switch", "DMIC4"},

	{"WCD_TX_OUTPUT", NULL, "DMIC5_MIXER"},
	{"DMIC5_MIXER", "Switch", "DMIC5"},

	{"WCD_TX_OUTPUT", NULL, "DMIC6_MIXER"},
	{"DMIC6_MIXER", "Switch", "DMIC6"},

	{"WCD_TX_OUTPUT", NULL, "DMIC7_MIXER"},
	{"DMIC7_MIXER", "Switch", "DMIC7"},

	{"WCD_TX_OUTPUT", NULL, "DMIC8_MIXER"},
	{"DMIC8_MIXER", "Switch", "DMIC8"},

	{"IN1_HPHL", NULL, "WCD_RX_DUMMY"},
	{"IN1_HPHL", NULL, "VDD_BUCK"},
	{"IN1_HPHL", NULL, "CLS_H_PORT"},
	{"RX1", NULL, "IN1_HPHL"},
	{"RDAC1", NULL, "RX1"},
	{"HPHL_RDAC", "Switch", "RDAC1"},
	{"HPHL PGA", NULL, "HPHL_RDAC"},
	{"HPHL", NULL, "HPHL PGA"},

	{"IN2_HPHR", NULL, "WCD_RX_DUMMY"},
	{"IN2_HPHR", NULL, "VDD_BUCK"},
	{"IN2_HPHR", NULL, "CLS_H_PORT"},
	{"RX2", NULL, "IN2_HPHR"},
	{"RDAC2", NULL, "RX2"},
	{"HPHR_RDAC", "Switch", "RDAC2"},
	{"HPHR PGA", NULL, "HPHR_RDAC"},
	{"HPHR", NULL, "HPHR PGA"},

	{"IN3_AUX", NULL, "WCD_RX_DUMMY"},
	{"IN3_AUX", NULL, "VDD_BUCK"},
	{"IN3_AUX", NULL, "CLS_H_PORT"},
	{"RX3", NULL, "IN3_AUX"},
	{"RDAC4", NULL, "RX3"},
	{"AUX_RDAC", "Switch", "RDAC4"},
	{"AUX PGA", NULL, "AUX_RDAC"},
	{"AUX", NULL, "AUX PGA"},

	{"RDAC3_MUX", "RX3", "RX3"},
	{"RDAC3_MUX", "RX1", "RX1"},
	{"RDAC3", NULL, "RDAC3_MUX"},
	{"EAR_RDAC", "Switch", "RDAC3"},
	{"EAR PGA", NULL, "EAR_RDAC"},
	{"EAR", NULL, "EAR PGA"},
};

static ssize_t wcd938x_version_read(struct snd_info_entry *entry,
				   void *file_private_data,
				   struct file *file,
				   char __user *buf, size_t count,
				   loff_t pos)
{
	struct wcd938x_priv *priv;
	char buffer[WCD938X_VERSION_ENTRY_SIZE];
	int len = 0;

	priv = (struct wcd938x_priv *) entry->private_data;
	if (!priv) {
		pr_err("%s: wcd938x priv is null\n", __func__);
		return -EINVAL;
	}

	switch (priv->version) {
	case WCD938X_VERSION_1_0:
		len = snprintf(buffer, sizeof(buffer), "WCD938X_1_0\n");
		break;
	default:
		len = snprintf(buffer, sizeof(buffer), "VER_UNDEFINED\n");
	}

	return simple_read_from_buffer(buf, count, &pos, buffer, len);
}

static struct snd_info_entry_ops wcd938x_info_ops = {
	.read = wcd938x_version_read,
};

static ssize_t wcd938x_variant_read(struct snd_info_entry *entry,
				    void *file_private_data,
				    struct file *file,
				    char __user *buf, size_t count,
				    loff_t pos)
{
	struct wcd938x_priv *priv;
	char buffer[WCD938X_VARIANT_ENTRY_SIZE];
	int len = 0;

	priv = (struct wcd938x_priv *) entry->private_data;
	if (!priv) {
		pr_err("%s: wcd938x priv is null\n", __func__);
		return -EINVAL;
	}

	switch (priv->variant) {
	case WCD9380:
		len = snprintf(buffer, sizeof(buffer), "WCD9380\n");
		break;
	case WCD9385:
		len = snprintf(buffer, sizeof(buffer), "WCD9385\n");
		break;
	default:
		len = snprintf(buffer, sizeof(buffer), "VER_UNDEFINED\n");
	}

	return simple_read_from_buffer(buf, count, &pos, buffer, len);
}

static struct snd_info_entry_ops wcd938x_variant_ops = {
	.read = wcd938x_variant_read,
};

/*
 * wcd938x_get_codec_variant
 * @component: component instance
 *
 * Return: codec variant or -EINVAL in error.
 */
int wcd938x_get_codec_variant(struct snd_soc_component *component)
{
	struct wcd938x_priv *priv = NULL;

	if (!component)
		return -EINVAL;

	priv = snd_soc_component_get_drvdata(component);
	if (!priv) {
		dev_err(component->dev,
			"%s:wcd938x not probed\n", __func__);
		return 0;
	}

	return priv->variant;
}
EXPORT_SYMBOL(wcd938x_get_codec_variant);

/*
 * wcd938x_info_create_codec_entry - creates wcd938x module
 * @codec_root: The parent directory
 * @component: component instance
 *
 * Creates wcd938x module, variant and version entry under the given
 * parent directory.
 *
 * Return: 0 on success or negative error code on failure.
 */
int wcd938x_info_create_codec_entry(struct snd_info_entry *codec_root,
				   struct snd_soc_component *component)
{
	struct snd_info_entry *version_entry;
	struct snd_info_entry *variant_entry;
	struct wcd938x_priv *priv;
	struct snd_soc_card *card;

	if (!codec_root || !component)
		return -EINVAL;

	priv = snd_soc_component_get_drvdata(component);
	if (priv->entry) {
		dev_dbg(priv->dev,
			"%s:wcd938x module already created\n", __func__);
		return 0;
	}
	card = component->card;

	priv->entry = snd_info_create_module_entry(codec_root->module,
					     "wcd938x", codec_root);
	if (!priv->entry) {
		dev_dbg(component->dev, "%s: failed to create wcd938x entry\n",
			__func__);
		return -ENOMEM;
	}
	priv->entry->mode = S_IFDIR | 0555;
	if (snd_info_register(priv->entry) < 0) {
		snd_info_free_entry(priv->entry);
		return -ENOMEM;
	}
	version_entry = snd_info_create_card_entry(card->snd_card,
						   "version",
						   priv->entry);
	if (!version_entry) {
		dev_dbg(component->dev, "%s: failed to create wcd938x version entry\n",
			__func__);
		snd_info_free_entry(priv->entry);
		return -ENOMEM;
	}

	version_entry->private_data = priv;
	version_entry->size = WCD938X_VERSION_ENTRY_SIZE;
	version_entry->content = SNDRV_INFO_CONTENT_DATA;
	version_entry->c.ops = &wcd938x_info_ops;

	if (snd_info_register(version_entry) < 0) {
		snd_info_free_entry(version_entry);
		snd_info_free_entry(priv->entry);
		return -ENOMEM;
	}
	priv->version_entry = version_entry;

	variant_entry = snd_info_create_card_entry(card->snd_card,
						   "variant",
						   priv->entry);
	if (!variant_entry) {
		dev_dbg(component->dev, "%s: failed to create wcd938x variant entry\n",
			__func__);
		snd_info_free_entry(version_entry);
		snd_info_free_entry(priv->entry);
		return -ENOMEM;
	}

	variant_entry->private_data = priv;
	variant_entry->size = WCD938X_VARIANT_ENTRY_SIZE;
	variant_entry->content = SNDRV_INFO_CONTENT_DATA;
	variant_entry->c.ops = &wcd938x_variant_ops;

	if (snd_info_register(variant_entry) < 0) {
		snd_info_free_entry(variant_entry);
		snd_info_free_entry(version_entry);
		snd_info_free_entry(priv->entry);
		return -ENOMEM;
	}
	priv->variant_entry = variant_entry;

	return 0;
}
EXPORT_SYMBOL(wcd938x_info_create_codec_entry);

static int wcd938x_set_micbias_data(struct wcd938x_priv *wcd938x,
			      struct wcd938x_pdata *pdata)
{
	int vout_ctl_1 = 0, vout_ctl_2 = 0, vout_ctl_3 = 0, vout_ctl_4 = 0;
	int rc = 0;

	if (!pdata) {
		dev_err(wcd938x->dev, "%s: NULL pdata\n", __func__);
		return -ENODEV;
	}

	/* set micbias voltage */
	vout_ctl_1 = wcd938x_get_micb_vout_ctl_val(pdata->micbias.micb1_mv);
	vout_ctl_2 = wcd938x_get_micb_vout_ctl_val(pdata->micbias.micb2_mv);
	vout_ctl_3 = wcd938x_get_micb_vout_ctl_val(pdata->micbias.micb3_mv);
	vout_ctl_4 = wcd938x_get_micb_vout_ctl_val(pdata->micbias.micb4_mv);
	if (vout_ctl_1 < 0 || vout_ctl_2 < 0 || vout_ctl_3 < 0 ||
	    vout_ctl_4 < 0) {
		rc = -EINVAL;
		goto done;
	}
	regmap_update_bits(wcd938x->regmap, WCD938X_ANA_MICB1, 0x3F,
			   vout_ctl_1);
	regmap_update_bits(wcd938x->regmap, WCD938X_ANA_MICB2, 0x3F,
			   vout_ctl_2);
	regmap_update_bits(wcd938x->regmap, WCD938X_ANA_MICB3, 0x3F,
			   vout_ctl_3);
	regmap_update_bits(wcd938x->regmap, WCD938X_ANA_MICB4, 0x3F,
			   vout_ctl_4);

done:
	return rc;
}

static int wcd938x_soc_codec_probe(struct snd_soc_component *component)
{
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(component);
	int variant;
	int ret = -EINVAL;

	dev_info(component->dev, "%s()\n", __func__);
	wcd938x = snd_soc_component_get_drvdata(component);

	if (!wcd938x)
		return -EINVAL;

	wcd938x->component = component;
	snd_soc_component_init_regmap(component, wcd938x->regmap);

	variant = (snd_soc_component_read32(component,
			WCD938X_DIGITAL_EFUSE_REG_0) & 0x1E) >> 1;
	wcd938x->variant = variant;

	wcd938x->fw_data = devm_kzalloc(component->dev,
					sizeof(*(wcd938x->fw_data)),
					GFP_KERNEL);
	if (!wcd938x->fw_data) {
		dev_err(component->dev, "Failed to allocate fw_data\n");
		ret = -ENOMEM;
		goto err;
	}

	set_bit(WCD9XXX_MBHC_CAL, wcd938x->fw_data->cal_bit);
	ret = wcd_cal_create_hwdep(wcd938x->fw_data,
				   WCD9XXX_CODEC_HWDEP_NODE, component);

	if (ret < 0) {
		dev_err(component->dev, "%s hwdep failed %d\n", __func__, ret);
		goto err_hwdep;
	}

	ret = wcd938x_mbhc_init(&wcd938x->mbhc, component, wcd938x->fw_data);
	if (ret) {
		pr_err("%s: mbhc initialization failed\n", __func__);
		goto err_hwdep;
	}

	snd_soc_dapm_ignore_suspend(dapm, "WCD938X_AIF Playback");
	snd_soc_dapm_ignore_suspend(dapm, "WCD938X_AIF Capture");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC4");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC5");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC6");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC7");
	snd_soc_dapm_ignore_suspend(dapm, "WCD_TX_OUTPUT");
	snd_soc_dapm_ignore_suspend(dapm, "IN1_HPHL");
	snd_soc_dapm_ignore_suspend(dapm, "IN2_HPHR");
	snd_soc_dapm_ignore_suspend(dapm, "IN3_AUX");
	snd_soc_dapm_ignore_suspend(dapm, "EAR");
	snd_soc_dapm_ignore_suspend(dapm, "AUX");
	snd_soc_dapm_ignore_suspend(dapm, "HPHL");
	snd_soc_dapm_ignore_suspend(dapm, "HPHR");
	snd_soc_dapm_ignore_suspend(dapm, "WCD_TX_DUMMY");
	snd_soc_dapm_ignore_suspend(dapm, "WCD_RX_DUMMY");
	snd_soc_dapm_sync(dapm);

	wcd_cls_h_init(&wcd938x->clsh_info);
	wcd938x_init_reg(component);

	if (wcd938x->variant == WCD9380) {
		ret = snd_soc_add_component_controls(component, wcd9380_snd_controls,
					ARRAY_SIZE(wcd9380_snd_controls));
		if (ret < 0) {
			dev_err(component->dev,
				"%s: Failed to add snd ctrls for variant: %d\n",
				__func__, wcd938x->variant);
			goto err_hwdep;
		}
	}
	if (wcd938x->variant == WCD9385) {
		ret = snd_soc_add_component_controls(component, wcd9385_snd_controls,
					ARRAY_SIZE(wcd9385_snd_controls));
		if (ret < 0) {
			dev_err(component->dev,
				"%s: Failed to add snd ctrls for variant: %d\n",
				__func__, wcd938x->variant);
			goto err_hwdep;
		}
	}
	wcd938x->version = WCD938X_VERSION_1_0;
       /* Register event notifier */
	wcd938x->nblock.notifier_call = wcd938x_event_notify;
	if (wcd938x->register_notifier) {
		ret = wcd938x->register_notifier(wcd938x->handle,
						&wcd938x->nblock,
						true);
		if (ret) {
			dev_err(component->dev,
				"%s: Failed to register notifier %d\n",
				__func__, ret);
			return ret;
		}
	}
	return ret;

err_hwdep:
	wcd938x->fw_data = NULL;

err:
	return ret;
}

static void wcd938x_soc_codec_remove(struct snd_soc_component *component)
{
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	if (!wcd938x) {
		dev_err(component->dev, "%s: wcd938x is already NULL\n",
			__func__);
		return;
	}
	if (wcd938x->register_notifier)
		wcd938x->register_notifier(wcd938x->handle,
						&wcd938x->nblock,
						false);
}

static int wcd938x_soc_codec_suspend(struct snd_soc_component *component)
{
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	if (!wcd938x)
		return 0;
	wcd938x->dapm_bias_off = true;
	return 0;
}

static int wcd938x_soc_codec_resume(struct snd_soc_component *component)
{
	struct wcd938x_priv *wcd938x = snd_soc_component_get_drvdata(component);

	if (!wcd938x)
		return 0;
	wcd938x->dapm_bias_off = false;
	return 0;
}

static struct snd_soc_component_driver soc_codec_dev_wcd938x = {
	.name = WCD938X_DRV_NAME,
	.probe = wcd938x_soc_codec_probe,
	.remove = wcd938x_soc_codec_remove,
	.controls = wcd938x_snd_controls,
	.num_controls = ARRAY_SIZE(wcd938x_snd_controls),
	.dapm_widgets = wcd938x_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(wcd938x_dapm_widgets),
	.dapm_routes = wcd938x_audio_map,
	.num_dapm_routes = ARRAY_SIZE(wcd938x_audio_map),
	.suspend =  wcd938x_soc_codec_suspend,
	.resume = wcd938x_soc_codec_resume,
};

static int wcd938x_reset(struct device *dev)
{
	struct wcd938x_priv *wcd938x = NULL;
	int rc = 0;
	int value = 0;

	if (!dev)
		return -ENODEV;

	wcd938x = dev_get_drvdata(dev);
	if (!wcd938x)
		return -EINVAL;

	if (!wcd938x->rst_np) {
		dev_err(dev, "%s: reset gpio device node not specified\n",
				__func__);
		return -EINVAL;
	}

	value = msm_cdc_pinctrl_get_state(wcd938x->rst_np);
	if (value > 0)
		return 0;

	rc = msm_cdc_pinctrl_select_sleep_state(wcd938x->rst_np);
	if (rc) {
		dev_err(dev, "%s: wcd sleep state request fail!\n",
				__func__);
		return rc;
	}
	/* 20us sleep required after pulling the reset gpio to LOW */
	usleep_range(20, 30);

	rc = msm_cdc_pinctrl_select_active_state(wcd938x->rst_np);
	if (rc) {
		dev_err(dev, "%s: wcd active state request fail!\n",
				__func__);
		return rc;
	}
	/* 20us sleep required after pulling the reset gpio to HIGH */
	usleep_range(20, 30);

	return rc;
}

static int wcd938x_read_of_property_u32(struct device *dev, const char *name,
					u32 *val)
{
	int rc = 0;

	rc = of_property_read_u32(dev->of_node, name, val);
	if (rc)
		dev_err(dev, "%s: Looking up %s property in node %s failed\n",
			__func__, name, dev->of_node->full_name);

	return rc;
}

static void wcd938x_dt_parse_micbias_info(struct device *dev,
					  struct wcd938x_micbias_setting *mb)
{
	u32 prop_val = 0;
	int rc = 0;

	/* MB1 */
	if (of_find_property(dev->of_node, "qcom,cdc-micbias1-mv",
				    NULL)) {
		rc = wcd938x_read_of_property_u32(dev,
						  "qcom,cdc-micbias1-mv",
						  &prop_val);
		if (!rc)
			mb->micb1_mv = prop_val;
	} else {
		dev_info(dev, "%s: Micbias1 DT property not found\n",
			__func__);
	}

	/* MB2 */
	if (of_find_property(dev->of_node, "qcom,cdc-micbias2-mv",
				    NULL)) {
		rc = wcd938x_read_of_property_u32(dev,
						  "qcom,cdc-micbias2-mv",
						  &prop_val);
		if (!rc)
			mb->micb2_mv = prop_val;
	} else {
		dev_info(dev, "%s: Micbias2 DT property not found\n",
			__func__);
	}

	/* MB3 */
	if (of_find_property(dev->of_node, "qcom,cdc-micbias3-mv",
				    NULL)) {
		rc = wcd938x_read_of_property_u32(dev,
						  "qcom,cdc-micbias3-mv",
						  &prop_val);
		if (!rc)
			mb->micb3_mv = prop_val;
	} else {
		dev_info(dev, "%s: Micbias3 DT property not found\n",
			__func__);
	}

	/* MB4 */
	if (of_find_property(dev->of_node, "qcom,cdc-micbias4-mv",
				    NULL)) {
		rc = wcd938x_read_of_property_u32(dev,
						  "qcom,cdc-micbias4-mv",
						  &prop_val);
		if (!rc)
			mb->micb4_mv = prop_val;
	} else {
		dev_info(dev, "%s: Micbias4 DT property not found\n",
			__func__);
	}
}

static int wcd938x_reset_low(struct device *dev)
{
	struct wcd938x_priv *wcd938x = NULL;
	int rc = 0;

	if (!dev)
		return -ENODEV;

	wcd938x = dev_get_drvdata(dev);
	if (!wcd938x)
		return -EINVAL;

	if (!wcd938x->rst_np) {
		dev_err(dev, "%s: reset gpio device node not specified\n",
				__func__);
		return -EINVAL;
	}

	rc = msm_cdc_pinctrl_select_sleep_state(wcd938x->rst_np);
	if (rc) {
		dev_err(dev, "%s: wcd sleep state request fail!\n",
				__func__);
		return rc;
	}
	/* 20us sleep required after pulling the reset gpio to LOW */
	usleep_range(20, 30);

	return rc;
}

struct wcd938x_pdata *wcd938x_populate_dt_data(struct device *dev)
{
	struct wcd938x_pdata *pdata = NULL;

	pdata = devm_kzalloc(dev, sizeof(struct wcd938x_pdata),
				GFP_KERNEL);
	if (!pdata)
		return NULL;

	pdata->rst_np = of_parse_phandle(dev->of_node,
			"qcom,wcd-rst-gpio-node", 0);

	if (!pdata->rst_np) {
		dev_err(dev, "%s: Looking up %s property in node %s failed\n",
				__func__, "qcom,wcd-rst-gpio-node",
				dev->of_node->full_name);
		return NULL;
	}

	/* Parse power supplies */
	msm_cdc_get_power_supplies(dev, &pdata->regulator,
				   &pdata->num_supplies);
	if (!pdata->regulator || (pdata->num_supplies <= 0)) {
		dev_err(dev, "%s: no power supplies defined for codec\n",
			__func__);
		return NULL;
	}

	pdata->rx_slave = of_parse_phandle(dev->of_node, "qcom,rx-slave", 0);
	pdata->tx_slave = of_parse_phandle(dev->of_node, "qcom,tx-slave", 0);

	wcd938x_dt_parse_micbias_info(dev, &pdata->micbias);

	return pdata;
}

static irqreturn_t wcd938x_wd_handle_irq(int irq, void *data)
{
	pr_err_ratelimited("%s: Watchdog interrupt for irq =%d triggered\n",
			   __func__, irq);
	return IRQ_HANDLED;
}

static struct snd_soc_dai_driver wcd938x_dai[] = {
	{
		.name = "wcd938x_cdc",
		.playback = {
			.stream_name = "WCD938X_AIF Playback",
                        .rates = WCD938X_RATES | WCD938X_FRAC_RATES,
			.formats = WCD938X_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.capture = {
			.stream_name = "WCD938X_AIF Capture",
                        .rates = WCD938X_RATES | WCD938X_FRAC_RATES,
			.formats = WCD938X_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
	},
};

static int wcd938x_bind(struct device *dev)
{
	int ret = 0, i = 0;
	struct wcd938x_pdata *pdata = dev_get_platdata(dev);
	struct wcd938x_priv *wcd938x = dev_get_drvdata(dev);

	/*
	 * Add 5msec delay to provide sufficient time for
	 * soundwire auto enumeration of slave devices as
	 * as per HW requirement.
	 */
	usleep_range(5000, 5010);

	ret = component_bind_all(dev, wcd938x);
	if (ret) {
		dev_err(dev, "%s: Slave bind failed, ret = %d\n",
			__func__, ret);
		return ret;
	}

	wcd938x->rx_swr_dev = get_matching_swr_slave_device(pdata->rx_slave);
	if (!wcd938x->rx_swr_dev) {
		dev_err(dev, "%s: Could not find RX swr slave device\n",
			 __func__);
		ret = -ENODEV;
		goto err;
	}

	wcd938x->tx_swr_dev = get_matching_swr_slave_device(pdata->tx_slave);
	if (!wcd938x->tx_swr_dev) {
		dev_err(dev, "%s: Could not find TX swr slave device\n",
			__func__);
		ret = -ENODEV;
		goto err;
	}

	wcd938x->regmap = devm_regmap_init_swr(wcd938x->tx_swr_dev,
					       &wcd938x_regmap_config);
	if (!wcd938x->regmap) {
		dev_err(dev, "%s: Regmap init failed\n",
				__func__);
		goto err;
	}

	/* Set all interupts as edge triggered */
	for (i = 0; i < wcd938x_regmap_irq_chip.num_regs; i++)
		regmap_write(wcd938x->regmap,
			     (WCD938X_DIGITAL_INTR_LEVEL_0 + i), 0);

	wcd938x_regmap_irq_chip.irq_drv_data = wcd938x;
	wcd938x->irq_info.wcd_regmap_irq_chip = &wcd938x_regmap_irq_chip;
	wcd938x->irq_info.codec_name = "WCD938X";
	wcd938x->irq_info.regmap = wcd938x->regmap;
	wcd938x->irq_info.dev = dev;
	ret = wcd_irq_init(&wcd938x->irq_info, &wcd938x->virq);

	if (ret) {
		dev_err(wcd938x->dev, "%s: IRQ init failed: %d\n",
			__func__, ret);
		goto err;
	}
	wcd938x->tx_swr_dev->slave_irq = wcd938x->virq;

	ret = wcd938x_set_micbias_data(wcd938x, pdata);
	if (ret < 0) {
		dev_err(dev, "%s: bad micbias pdata\n", __func__);
		goto err_irq;
	}

	/* Request for watchdog interrupt */
	wcd_request_irq(&wcd938x->irq_info, WCD938X_IRQ_HPHR_PDM_WD_INT,
			"HPHR PDM WD INT", wcd938x_wd_handle_irq, NULL);
	wcd_request_irq(&wcd938x->irq_info, WCD938X_IRQ_HPHL_PDM_WD_INT,
			"HPHL PDM WD INT", wcd938x_wd_handle_irq, NULL);
	wcd_request_irq(&wcd938x->irq_info, WCD938X_IRQ_AUX_PDM_WD_INT,
			"AUX PDM WD INT", wcd938x_wd_handle_irq, NULL);
	/* Disable watchdog interrupt for HPH and AUX */
	wcd_disable_irq(&wcd938x->irq_info, WCD938X_IRQ_HPHR_PDM_WD_INT);
	wcd_disable_irq(&wcd938x->irq_info, WCD938X_IRQ_HPHL_PDM_WD_INT);
	wcd_disable_irq(&wcd938x->irq_info, WCD938X_IRQ_AUX_PDM_WD_INT);

	ret = snd_soc_register_component(dev, &soc_codec_dev_wcd938x,
					wcd938x_dai, ARRAY_SIZE(wcd938x_dai));
	if (ret) {
		dev_err(dev, "%s: Codec registration failed\n",
				__func__);
		goto err_irq;
	}
	wcd938x->dev_up = true;

	return ret;
err_irq:
	wcd_irq_exit(&wcd938x->irq_info, wcd938x->virq);
err:
	component_unbind_all(dev, wcd938x);
	return ret;
}

static void wcd938x_unbind(struct device *dev)
{
	struct wcd938x_priv *wcd938x = dev_get_drvdata(dev);

	wcd_free_irq(&wcd938x->irq_info, WCD938X_IRQ_HPHR_PDM_WD_INT, NULL);
	wcd_free_irq(&wcd938x->irq_info, WCD938X_IRQ_HPHL_PDM_WD_INT, NULL);
	wcd_free_irq(&wcd938x->irq_info, WCD938X_IRQ_AUX_PDM_WD_INT, NULL);
	wcd_irq_exit(&wcd938x->irq_info, wcd938x->virq);
	snd_soc_unregister_component(dev);
	component_unbind_all(dev, wcd938x);
}

static const struct of_device_id wcd938x_dt_match[] = {
	{ .compatible = "qcom,wcd938x-codec", .data = "wcd938x"},
	{}
};

static const struct component_master_ops wcd938x_comp_ops = {
	.bind   = wcd938x_bind,
	.unbind = wcd938x_unbind,
};

static int wcd938x_compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static void wcd938x_release_of(struct device *dev, void *data)
{
	of_node_put(data);
}

static int wcd938x_add_slave_components(struct device *dev,
				struct component_match **matchptr)
{
	struct device_node *np, *rx_node, *tx_node;

	np = dev->of_node;

	rx_node = of_parse_phandle(np, "qcom,rx-slave", 0);
	if (!rx_node) {
		dev_err(dev, "%s: Rx-slave node not defined\n", __func__);
		return -ENODEV;
	}
	of_node_get(rx_node);
	component_match_add_release(dev, matchptr,
			wcd938x_release_of,
			wcd938x_compare_of,
			rx_node);

	tx_node = of_parse_phandle(np, "qcom,tx-slave", 0);
	if (!tx_node) {
		dev_err(dev, "%s: Tx-slave node not defined\n", __func__);
			return -ENODEV;
	}
	of_node_get(tx_node);
	component_match_add_release(dev, matchptr,
			wcd938x_release_of,
			wcd938x_compare_of,
			tx_node);
	return 0;
}

static int wcd938x_probe(struct platform_device *pdev)
{
	struct component_match *match = NULL;
	struct wcd938x_priv *wcd938x = NULL;
	struct wcd938x_pdata *pdata = NULL;
	struct wcd_ctrl_platform_data *plat_data = NULL;
	struct device *dev = &pdev->dev;
	int ret;

	wcd938x = devm_kzalloc(dev, sizeof(struct wcd938x_priv),
				GFP_KERNEL);
	if (!wcd938x)
		return -ENOMEM;

	dev_set_drvdata(dev, wcd938x);
	wcd938x->dev = dev;

	pdata = wcd938x_populate_dt_data(dev);
	if (!pdata) {
		dev_err(dev, "%s: Fail to obtain platform data\n", __func__);
		return -EINVAL;
	}
	dev->platform_data = pdata;

	wcd938x->rst_np = pdata->rst_np;
	ret = msm_cdc_init_supplies(dev, &wcd938x->supplies,
				    pdata->regulator, pdata->num_supplies);
	if (!wcd938x->supplies) {
		dev_err(dev, "%s: Cannot init wcd supplies\n",
			__func__);
		return ret;
	}

	plat_data = dev_get_platdata(dev->parent);
	if (!plat_data) {
		dev_err(dev, "%s: platform data from parent is NULL\n",
			__func__);
		return -EINVAL;
	}
	wcd938x->handle = (void *)plat_data->handle;
	if (!wcd938x->handle) {
		dev_err(dev, "%s: handle is NULL\n", __func__);
		return -EINVAL;
	}

	wcd938x->update_wcd_event = plat_data->update_wcd_event;
	if (!wcd938x->update_wcd_event) {
		dev_err(dev, "%s: update_wcd_event api is null!\n",
			__func__);
		return -EINVAL;
	}
	wcd938x->register_notifier = plat_data->register_notifier;
	if (!wcd938x->register_notifier) {
		dev_err(dev, "%s: register_notifier api is null!\n",
			__func__);
		return -EINVAL;
	}

	ret = msm_cdc_enable_static_supplies(&pdev->dev, wcd938x->supplies,
					     pdata->regulator,
					     pdata->num_supplies);
	if (ret) {
		dev_err(dev, "%s: wcd static supply enable failed!\n",
			__func__);
		return ret;
	}

	ret = wcd938x_parse_port_mapping(dev, "qcom,rx_swr_ch_map",
					CODEC_RX);
	ret |= wcd938x_parse_port_mapping(dev, "qcom,tx_swr_ch_map",
					CODEC_TX);

	if (ret) {
		dev_err(dev, "Failed to read port mapping\n");
		goto err;
	}

	mutex_init(&wcd938x->wakeup_lock);
	mutex_init(&wcd938x->micb_lock);
	ret = wcd938x_add_slave_components(dev, &match);
	if (ret)
		goto err_lock_init;

	wcd938x_reset(dev);

	wcd938x->wakeup = wcd938x_wakeup;

	return component_master_add_with_match(dev,
					&wcd938x_comp_ops, match);

err_lock_init:
	mutex_destroy(&wcd938x->micb_lock);
	mutex_destroy(&wcd938x->wakeup_lock);
err:
	return ret;
}

static int wcd938x_remove(struct platform_device *pdev)
{
	struct wcd938x_priv *wcd938x = NULL;

	wcd938x = platform_get_drvdata(pdev);
	component_master_del(&pdev->dev, &wcd938x_comp_ops);
	mutex_destroy(&wcd938x->micb_lock);
	mutex_destroy(&wcd938x->wakeup_lock);
	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int wcd938x_suspend(struct device *dev)
{
	struct wcd938x_priv *wcd938x = NULL;
	int ret = 0;
	struct wcd938x_pdata *pdata = NULL;

	if (!dev)
		return -ENODEV;

	wcd938x = dev_get_drvdata(dev);
	if (!wcd938x)
		return -EINVAL;

	pdata = dev_get_platdata(wcd938x->dev);

	if (!pdata) {
		dev_err(dev, "%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}

	if (test_bit(ALLOW_BUCK_DISABLE, &wcd938x->status_mask)) {
		ret = msm_cdc_disable_ondemand_supply(wcd938x->dev,
						wcd938x->supplies,
						pdata->regulator,
						pdata->num_supplies,
						"cdc-vdd-buck");
		if (ret == -EINVAL) {
			dev_err(dev, "%s: vdd buck is not disabled\n",
				__func__);
			return 0;
		}
		clear_bit(ALLOW_BUCK_DISABLE, &wcd938x->status_mask);
	}
	if (wcd938x->dapm_bias_off) {
		msm_cdc_set_supplies_lpm_mode(wcd938x->dev,
					      wcd938x->supplies,
					      pdata->regulator,
					      pdata->num_supplies,
					      true);
		set_bit(WCD_SUPPLIES_LPM_MODE, &wcd938x->status_mask);
	}
	return 0;
}

static int wcd938x_resume(struct device *dev)
{
	struct wcd938x_priv *wcd938x = NULL;
	struct wcd938x_pdata *pdata = NULL;

	if (!dev)
		return -ENODEV;

	wcd938x = dev_get_drvdata(dev);
	if (!wcd938x)
		return -EINVAL;

	pdata = dev_get_platdata(wcd938x->dev);

	if (!pdata) {
		dev_err(dev, "%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}

	if (test_bit(WCD_SUPPLIES_LPM_MODE, &wcd938x->status_mask)) {
		msm_cdc_set_supplies_lpm_mode(wcd938x->dev,
					      wcd938x->supplies,
					      pdata->regulator,
					      pdata->num_supplies,
					      false);
		clear_bit(WCD_SUPPLIES_LPM_MODE, &wcd938x->status_mask);
	}

	return 0;
}

static const struct dev_pm_ops wcd938x_dev_pm_ops = {
	.suspend_late = wcd938x_suspend,
	.resume_early = wcd938x_resume,
};
#endif

static struct platform_driver wcd938x_codec_driver = {
	.probe = wcd938x_probe,
	.remove = wcd938x_remove,
	.driver = {
		.name = "wcd938x_codec",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(wcd938x_dt_match),
#ifdef CONFIG_PM_SLEEP
		.pm = &wcd938x_dev_pm_ops,
#endif
		.suppress_bind_attrs = true,
	},
};

module_platform_driver(wcd938x_codec_driver);
MODULE_DESCRIPTION("WCD938X Codec driver");
MODULE_LICENSE("GPL v2");
