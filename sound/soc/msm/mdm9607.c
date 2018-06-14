/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <soc/qcom/socinfo.h>
#include <qdsp6v2/msm-pcm-routing-v2.h>
#include <sound/q6core.h>
#include <linux/mfd/wcd9xxx/wcd-gpio-ctrl.h>
#include "../codecs/wcd9xxx-common.h"
#include "../codecs/wcd9330.h"
#include "../codecs/wcd9306.h"

/* Spk control */
#define MDM_SPK_ON 1

#define WCD9XXX_MBHC_DEF_BUTTONS 8
#define WCD9XXX_MBHC_DEF_RLOADS 5
/*
 * MDM9607 run Tomtom at 12.288 Mhz.
 * At present MDM supports 12.288mhz
 * only. Tomtom supports 9.6 MHz also.
 */
#define MDM_MCLK_CLK_12P288MHZ 12288000
#define MDM_MCLK_CLK_9P6HZ 9600000
#define MDM_MI2S_RATE 48000

#define SAMPLE_RATE_8KHZ 8000
#define SAMPLE_RATE_16KHZ 16000
#define SAMPLE_RATE_48KHZ 48000
#define NO_OF_BITS_PER_SAMPLE  16

#define LPAIF_OFFSET 0x07700000
#define LPAIF_PRI_MODE_MUXSEL (LPAIF_OFFSET + 0x2008)
#define LPAIF_SEC_MODE_MUXSEL (LPAIF_OFFSET + 0x200c)

#define LPASS_CSR_GP_IO_MUX_SPKR_CTL (LPAIF_OFFSET + 0x2004)
#define LPASS_CSR_GP_IO_MUX_MIC_CTL  (LPAIF_OFFSET + 0x2000)

/* SW workaround for MCLK routing issue at HW level in MDM9607
  * Set below registers to route
  * MCLK source (gcc_ultaudio_ext_mclk2)
  * to PLL test pad
*/
#define GCC_CLK_CTL_REG 0x01800000
#define GCC_GCC_DEBUG_CLK_CTL (GCC_CLK_CTL_REG + 0x74000)
#define GCC_PLLTEST_PAD_CFG (GCC_CLK_CTL_REG + 0x7400C)

#define GCC_DEBUG_CLK_CTL_EN_VALUE 0x100ec
#define GCC_PLLTEST_PAD_CFG_EN_VALUE 0x1200

#define I2S_SEL 0
#define I2S_PCM_SEL 1
#define I2S_PCM_SEL_OFFSET 0
#define I2S_PCM_MASTER_MODE 1
#define I2S_PCM_SLAVE_MODE 0

/* Currently enabling only SCLK because
 * MCLK is routed from PLL test pad
 */
#define PRI_TLMM_CLKS_EN_MASTER 0x00020004
#define SEC_TLMM_CLKS_EN_MASTER 0x2
#define PRI_TLMM_CLKS_EN_SLAVE 0x100000
#define SEC_TLMM_CLKS_EN_SLAVE 0x800000
#define CLOCK_ON  1
#define CLOCK_OFF 0

/* Machine driver Name*/
#define DRV_NAME "mdm9607-asoc-snd"
#define TDM_SLOT_OFFSET_MAX    8

enum {
	PRIMARY_TDM_RX_0,
	PRIMARY_TDM_TX_0,
	SECONDARY_TDM_RX_0,
	SECONDARY_TDM_TX_0,
	TDM_MAX,
};

enum mi2s_pcm_mux {
	PRI_MI2S_PCM,
	SEC_MI2S_PCM,
	MI2S_PCM_MAX_INTF
};

struct mdm9607_codec {
	int (*mclk_enable_fn)(struct snd_soc_codec *codec,
			      int mclk_enable, bool dapm);
	int (*mbhc_hs_detect)(struct snd_soc_codec *codec,
			      struct wcd9xxx_mbhc_config *mbhc_cfg);
};

struct mdm_machine_data {
	u32 mclk_freq;
	atomic_t prim_clk_usrs;
	u16 prim_mi2s_mode;
	u16 prim_auxpcm_mode;
	struct device_node *prim_master_p;
	struct device_node *prim_slave_p;
	atomic_t sec_clk_usrs;
	u16 sec_mi2s_mode;
	u16 sec_auxpcm_mode;
	u16 sec_tdm_mode;
	struct device_node *sec_master_p;
	struct device_node *sec_slave_p;
	void *lpaif_pri_muxsel_virt_addr;
	void __iomem *lpaif_sec_muxsel_virt_addr;
	void *lpass_mux_spkr_ctl_virt_addr;
	void __iomem *lpass_mux_mic_ctl_virt_addr;
	void *gcc_debug_clk_ctl_virt_addr;
	void *gcc_plltest_pad_cfg_virt_addr;
	struct mdm9607_codec mdm9607_codec_fn;
};

static const struct afe_clk_set lpass_default = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_CLK_ID_PRI_MI2S_IBIT,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

static int mdm_auxpcm_rate = SAMPLE_RATE_8KHZ;
static struct mutex cdc_mclk_mutex;
static int mdm_mi2s_rx_ch = 1;
static int mdm_mi2s_tx_ch = 1;
static int mdm_mi2s_rate = SAMPLE_RATE_48KHZ;
static int mdm_sec_mi2s_rx_ch = 1;
static int mdm_sec_mi2s_tx_ch = 1;
static int mdm_sec_mi2s_rate = SAMPLE_RATE_48KHZ;
static int mdm_mi2s_mode = I2S_PCM_MASTER_MODE;
static int mdm_sec_mi2s_mode = I2S_PCM_MASTER_MODE;
static int mdm_auxpcm_mode = I2S_PCM_MASTER_MODE;
static int mdm_sec_auxpcm_mode = I2S_PCM_MASTER_MODE;
static int mdm_sec_tdm_mode = I2S_PCM_MASTER_MODE;

/* TDM default channels */
static int mdm_pri_tdm_rx_0_ch = 8;
static int mdm_pri_tdm_tx_0_ch = 8;

static int mdm_sec_tdm_rx_0_ch = 8;
static int mdm_sec_tdm_tx_0_ch = 8;

/* TDM default bit format */
static int mdm_pri_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int mdm_pri_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static int mdm_sec_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int mdm_sec_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;

/* TDM default sampling rate */
static int mdm_pri_tdm_rx_0_sample_rate = SAMPLE_RATE_48KHZ;
static int mdm_pri_tdm_tx_0_sample_rate = SAMPLE_RATE_48KHZ;

static int mdm_sec_tdm_rx_0_sample_rate = SAMPLE_RATE_48KHZ;
static int mdm_sec_tdm_tx_0_sample_rate = SAMPLE_RATE_48KHZ;

static char const *tdm_ch_text[] = {"One", "Two", "Three", "Four",
	"Five", "Six", "Seven", "Eight"};
static char const *tdm_bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE",
					    "S32_LE"};
static char const *tdm_sample_rate_text[] = {"KHZ_16", "KHZ_48"};

static char const *tdm_num_slots_text[] = {"eight", "sixteen"};

/* TDM default Number of Slots */

static int mdm_tdm_num_slots = 8;

/* TDM default offset */
static unsigned int tdm_slot_offset[TDM_MAX][TDM_SLOT_OFFSET_MAX] = {
	/* PRI_TDM_RX */
	{0, 4, 8, 12, 16, 20, 24, 28},
	/* PRI_TDM_TX */
	{0, 4, 8, 12, 16, 20, 24, 28},
	/* SEC_TDM_RX */
	{0, 4, 8, 12, 16, 20, 24, 28},
	/* SEC_TDM_TX */
	{0, 4, 8, 12, 16, 20, 24, 28},
};

static int mdm_spk_control;
static atomic_t aux_ref_count;
static atomic_t sec_aux_ref_count;
static atomic_t mi2s_ref_count;
static atomic_t sec_mi2s_ref_count;
static atomic_t sec_tdm_ref_count;

static int clk_users;

static inline int param_is_mask(int p)
{
	return (p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
			(p <= SNDRV_PCM_HW_PARAM_LAST_MASK);
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p, int n)
{
	return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

static void param_set_mask(struct snd_pcm_hw_params *p, int n, unsigned bit)
{
	if (bit >= SNDRV_MASK_MAX)
		return;
	if (param_is_mask(n)) {
		struct snd_mask *m = param_to_mask(p, n);

		m->bits[0] = 0;
		m->bits[1] = 0;
		m->bits[bit >> 5] |= (1 << (bit & 31));
	}
}

static int mdm_enable_codec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm);

static void *def_codec_mbhc_cal(void);

static struct wcd9xxx_mbhc_config mbhc_cfg = {
	.read_fw_bin = false,
	.calibration = NULL,
	.micbias = MBHC_MICBIAS2,
	.mclk_cb_fn = mdm_enable_codec_ext_clk,
	.mclk_rate = MDM_MCLK_CLK_12P288MHZ,
	.gpio_level_insert = 1,
	.detect_extn_cable = false,
	.micbias_enable_flags = 1 << MBHC_MICBIAS_ENABLE_THRESHOLD_HEADSET,
	.insert_detect = true,
	.swap_gnd_mic = NULL,
	.cs_enable_flags = (1 << MBHC_CS_ENABLE_POLLING |
			    1 << MBHC_CS_ENABLE_INSERTION |
			    1 << MBHC_CS_ENABLE_REMOVAL |
			    1 << MBHC_CS_ENABLE_DET_ANC),
	.do_recalibration = true,
	.use_vddio_meas = true,
	.enable_anc_mic_detect = false,
	.hw_jack_type = FOUR_POLE_JACK,
};

static void mdm_gpio_set_mux_ctl(struct mdm_machine_data *dt)
{
	if (dt->gcc_debug_clk_ctl_virt_addr != NULL)
		iowrite32(GCC_DEBUG_CLK_CTL_EN_VALUE,
			dt->gcc_debug_clk_ctl_virt_addr);
	else
		pr_err("%s: gcc_debug_clk_ctl_virt_addr is NULL\n",
				__func__);

	if (dt->gcc_plltest_pad_cfg_virt_addr != NULL)
		iowrite32(GCC_PLLTEST_PAD_CFG_EN_VALUE,
			dt->gcc_plltest_pad_cfg_virt_addr);
	else
		pr_err("%s:gcc_plltest_pad_cfg_virt_addr is NULL\n",
				__func__);
}

static int mdm_mi2s_clk_ctl(struct snd_soc_pcm_runtime *rtd, bool enable,
				int rate, u16 mode)
{
	struct snd_soc_card *card = rtd->card;
	struct mdm_machine_data *pdata = snd_soc_card_get_drvdata(card);
	struct afe_clk_set *lpass_clk = NULL;
	int ret = 0;
	int bit_clk_freq = (rate * 2 * NO_OF_BITS_PER_SAMPLE);

	if (pdata == NULL) {
		pr_err("%s:platform data is null\n", __func__);

		ret = -ENOMEM;
		goto done;
	}
	lpass_clk = kzalloc(sizeof(struct afe_clk_cfg), GFP_KERNEL);
	if (!lpass_clk) {
		ret = -ENOMEM;
		goto done;
	}
	memcpy(lpass_clk, &lpass_default, sizeof(struct afe_clk_cfg));
	pr_debug("%s enable = %x\n", __func__, enable);

	if (enable) {
		if (atomic_read(&pdata->prim_clk_usrs) == 0) {
			lpass_clk->clk_id = Q6AFE_LPASS_CLK_ID_MCLK_3;
			lpass_clk->clk_freq_in_hz = pdata->mclk_freq;
			lpass_clk->enable = 1;
			ret = afe_set_lpass_clock_v2(
				AFE_PORT_ID_PRIMARY_MI2S_RX, lpass_clk);
			if (ret < 0)
				pr_err("%s:afe set mclk failed\n", __func__);
			else
				atomic_inc(&pdata->prim_clk_usrs);
		} else {
			lpass_clk->enable = 1;
		}
		lpass_clk->clk_freq_in_hz = bit_clk_freq;
	} else {
		if (atomic_read(&pdata->prim_clk_usrs) > 0)
			atomic_dec(&pdata->prim_clk_usrs);

		if (atomic_read(&pdata->prim_clk_usrs) == 0) {
			lpass_clk->clk_id = Q6AFE_LPASS_CLK_ID_MCLK_3;
			lpass_clk->enable = 0;
			ret = afe_set_lpass_clock_v2(
				AFE_PORT_ID_PRIMARY_MI2S_RX, lpass_clk);
		} else {
			lpass_clk->enable = 0;
		}
	}
	if (mode) {
		lpass_clk->clk_id = Q6AFE_LPASS_CLK_ID_PRI_MI2S_IBIT;
		ret = afe_set_lpass_clock_v2(AFE_PORT_ID_PRIMARY_MI2S_RX,
						lpass_clk);
		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock_v2 failed\n", __func__);
	}

	pr_debug("%s:enable = %d mode = %u bit_clk  = %x mclk = %x\n",
		 __func__, enable, mode, bit_clk_freq, pdata->mclk_freq);

	kfree(lpass_clk);
	clk_users = atomic_read(&pdata->prim_clk_usrs);
done:
	return ret;
}

static void mdm_mi2s_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int ret;
	struct snd_soc_card *card = rtd->card;
	struct mdm_machine_data *pdata = snd_soc_card_get_drvdata(card);

	if (atomic_dec_return(&mi2s_ref_count) == 0) {
		ret = mdm_mi2s_clk_ctl(rtd, false, 0, pdata->prim_mi2s_mode);
		if (ret < 0)
			pr_err("%s Clock disable failed\n", __func__);

		if (pdata->prim_mi2s_mode == 1)
			ret = wcd_gpio_ctrl_select_sleep_state
						(pdata->prim_master_p);
		else
			ret = wcd_gpio_ctrl_select_sleep_state
						(pdata->prim_slave_p);
		if (ret)
			pr_err("%s: failed to set pri gpios to sleep: %d\n",
					__func__, ret);
	}
}

static int mdm_mi2s_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_card *card = rtd->card;
	struct mdm_machine_data *pdata = snd_soc_card_get_drvdata(card);
	int ret = 0;

	pdata->prim_mi2s_mode = mdm_mi2s_mode;
	if (atomic_inc_return(&mi2s_ref_count) == 1) {
		if (pdata->lpaif_pri_muxsel_virt_addr != NULL) {
			ret = afe_enable_lpass_core_shared_clock(MI2S_RX,
								 CLOCK_ON);
			if (ret < 0) {
				ret = -EINVAL;
				goto done;
			}
			iowrite32(I2S_SEL << I2S_PCM_SEL_OFFSET,
				  pdata->lpaif_pri_muxsel_virt_addr);

			if (pdata->lpass_mux_spkr_ctl_virt_addr != NULL) {
				if (pdata->prim_mi2s_mode == 1)
					iowrite32(PRI_TLMM_CLKS_EN_MASTER,
					pdata->lpass_mux_spkr_ctl_virt_addr);
				else
					iowrite32(PRI_TLMM_CLKS_EN_SLAVE,
					pdata->lpass_mux_spkr_ctl_virt_addr);
			} else {
				pr_err("%s: mux spkr ctl virt addr is NULL\n",
				       __func__);
				ret = -EINVAL;
				goto err;
			}
			mdm_gpio_set_mux_ctl(pdata);
		} else {
			pr_err("%s lpaif_pri_muxsel_virt_addr is NULL\n",
				__func__);
			ret = -EINVAL;
			goto done;
		}

		/*
		 * This sets the CONFIG PARAMETER WS_SRC.
		 * 1 means internal clock master mode.
		 * 0 means external clock slave mode.
		 */
		if (pdata->prim_mi2s_mode == 1) {
			ret = wcd_gpio_ctrl_select_active_state
						(pdata->prim_master_p);
			if (ret < 0) {
				pr_err("%s pinctrl set failed\n", __func__);
				goto err;
			}
			ret = mdm_mi2s_clk_ctl(rtd, true, mdm_mi2s_rate,
						pdata->prim_mi2s_mode);
			if (ret < 0) {
				pr_err("%s clock enable failed\n", __func__);
				goto err;
			}
			ret = snd_soc_dai_set_fmt(cpu_dai,
					SND_SOC_DAIFMT_CBS_CFS);
			if (ret < 0) {
				pr_err("%s Set fmt for cpu dai failed\n",
					__func__);
				goto err;
			}
			ret = snd_soc_dai_set_fmt(codec_dai,
					SND_SOC_DAIFMT_CBS_CFS);
			if (ret < 0)
				pr_err("%s Set fmt for codec dai failed\n",
					__func__);
		} else {
			/*
			 * Disable bit clk in slave mode for QC codec.
			 * Enable only mclk.
			 */
			ret = wcd_gpio_ctrl_select_active_state
						(pdata->prim_slave_p);
			if (ret < 0) {
				pr_err("%s pinctrl set failed\n", __func__);
				goto err;
			}
			ret = mdm_mi2s_clk_ctl(rtd, true, 0,
						pdata->prim_mi2s_mode);
			if (ret < 0) {
				pr_err("%s clock enable failed\n", __func__);
				goto err;
			}
			ret = snd_soc_dai_set_fmt(cpu_dai,
					SND_SOC_DAIFMT_CBM_CFM);
			if (ret < 0) {
				pr_err("%s Set fmt for cpu dai failed\n",
					__func__);
				goto err;
			}
		}
	}
err:
	afe_enable_lpass_core_shared_clock(MI2S_RX, CLOCK_OFF);
done:
	if (ret < 0)
		atomic_dec(&mi2s_ref_count);
	return ret;
}

static int mdm_sec_mi2s_clk_ctl(struct snd_soc_pcm_runtime *rtd, bool enable,
				int rate, u16 mode)
{
	struct snd_soc_card *card = rtd->card;
	struct mdm_machine_data *pdata = snd_soc_card_get_drvdata(card);
	struct afe_clk_set *lpass_clk = NULL;
	int ret = 0;
	int bit_clk_freq = (rate * 2 * NO_OF_BITS_PER_SAMPLE);

	if (pdata == NULL) {
		pr_err("%s:platform data is null\n", __func__);

		ret = -ENOMEM;
		goto done;
	}
	lpass_clk = kzalloc(sizeof(struct afe_clk_cfg), GFP_KERNEL);
	if (!lpass_clk) {
		ret = -ENOMEM;
		goto done;
	}
	memcpy(lpass_clk, &lpass_default, sizeof(struct afe_clk_cfg));
	pr_debug("%s enable = %x\n", __func__, enable);

	if (enable) {
		if (atomic_read(&pdata->sec_clk_usrs) == 0) {
			lpass_clk->clk_id = Q6AFE_LPASS_CLK_ID_MCLK_3;
			lpass_clk->clk_freq_in_hz = pdata->mclk_freq;
			lpass_clk->enable = 1;
			ret = afe_set_lpass_clock_v2(
				AFE_PORT_ID_SECONDARY_MI2S_RX, lpass_clk);
			if (ret < 0)
				pr_err("%s:afe set mclk failed\n", __func__);
			else
				atomic_inc(&pdata->sec_clk_usrs);
		} else {
			lpass_clk->enable = 1;
		}
		lpass_clk->clk_freq_in_hz = bit_clk_freq;
	} else {
		if (atomic_read(&pdata->sec_clk_usrs) > 0)
			atomic_dec(&pdata->sec_clk_usrs);

		if (atomic_read(&pdata->sec_clk_usrs) == 0) {
			lpass_clk->clk_id = Q6AFE_LPASS_CLK_ID_MCLK_3;
			lpass_clk->enable = 0;
			ret = afe_set_lpass_clock_v2(
				AFE_PORT_ID_SECONDARY_MI2S_RX, lpass_clk);
		} else {
			lpass_clk->enable = 0;
		}
	}
	if (mode) {
		lpass_clk->clk_id = Q6AFE_LPASS_CLK_ID_SEC_MI2S_IBIT;
		ret = afe_set_lpass_clock_v2(AFE_PORT_ID_SECONDARY_MI2S_RX,
						lpass_clk);
		if (ret < 0)
			pr_err("%s:afe_set_lpass_clock_v2 failed\n", __func__);
	}

	pr_debug("%s:enable = %d mode = %u bit_clk  = %x mclk = %x\n",
		 __func__, enable, mode, bit_clk_freq, pdata->mclk_freq);

	kfree(lpass_clk);
done:
	return ret;
}

static void mdm_sec_mi2s_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct mdm_machine_data *pdata = snd_soc_card_get_drvdata(card);
	int ret;

	if (atomic_dec_return(&sec_mi2s_ref_count) == 0) {
		ret = mdm_sec_mi2s_clk_ctl(rtd, false, 0, pdata->sec_mi2s_mode);
		if (ret < 0)
			pr_err("%s Clock disable failed\n", __func__);

		if (pdata->sec_mi2s_mode == 1)
			ret = wcd_gpio_ctrl_select_sleep_state
						(pdata->sec_master_p);
		else
			ret = wcd_gpio_ctrl_select_sleep_state
						(pdata->sec_slave_p);
		if (ret)
			pr_err("%s: failed to set sec gpios to sleep: %d\n",
				__func__, ret);
	}
}

static int mdm_sec_mi2s_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	struct mdm_machine_data *pdata = snd_soc_card_get_drvdata(card);
	int ret = 0;

	pdata->sec_mi2s_mode = mdm_sec_mi2s_mode;
	if (atomic_inc_return(&sec_mi2s_ref_count) == 1) {
		if (pdata->lpaif_sec_muxsel_virt_addr != NULL) {
			ret = afe_enable_lpass_core_shared_clock(
					SECONDARY_I2S_RX, CLOCK_ON);
			if (ret < 0) {
				ret = -EINVAL;
				goto done;
			}
			iowrite32(I2S_SEL << I2S_PCM_SEL_OFFSET,
				  pdata->lpaif_sec_muxsel_virt_addr);

			if (pdata->lpass_mux_mic_ctl_virt_addr != NULL) {
				if (pdata->sec_mi2s_mode == 1)
					iowrite32(SEC_TLMM_CLKS_EN_MASTER,
					pdata->lpass_mux_mic_ctl_virt_addr);
				else
					iowrite32(SEC_TLMM_CLKS_EN_SLAVE,
					pdata->lpass_mux_mic_ctl_virt_addr);
			} else {
				pr_err("%s: mux spkr ctl virt addr is NULL\n",
				       __func__);
				ret = -EINVAL;
				goto err;
			}
			mdm_gpio_set_mux_ctl(pdata);
		} else {
			pr_err("%s lpaif_sec_muxsel_virt_addr is NULL\n",
				__func__);
			ret = -EINVAL;
			goto done;
		}
		/*
		 * This sets the CONFIG PARAMETER WS_SRC.
		 * 1 means internal clock master mode.
		 * 0 means external clock slave mode.
		 */
		if (pdata->sec_mi2s_mode == 1) {
			ret = wcd_gpio_ctrl_select_active_state
						(pdata->sec_master_p);
			if (ret < 0) {
				pr_err("%s pinctrl set failed\n", __func__);
				goto err;
			}
			ret = mdm_sec_mi2s_clk_ctl(rtd, true,
				mdm_sec_mi2s_rate, pdata->sec_mi2s_mode);
			if (ret < 0) {
				pr_err("%s clock enable failed\n", __func__);
				goto err;
			}
			ret = snd_soc_dai_set_fmt(cpu_dai,
					SND_SOC_DAIFMT_CBS_CFS);
			if (ret < 0)
				pr_err("%s Set fmt for cpu dai failed\n",
					__func__);
		} else {
			/*
			 * Disable bit clk in slave mode for QC codec.
			 * Enable only mclk.
			 */
			ret = wcd_gpio_ctrl_select_active_state
						(pdata->sec_slave_p);
			if (ret < 0) {
				pr_err("%s pinctrl set failed\n", __func__);
				goto err;
			}
			ret = mdm_sec_mi2s_clk_ctl(rtd, true,
						0, pdata->sec_mi2s_mode);
			if (ret < 0) {
				pr_err("%s clock enable failed\n", __func__);
				goto err;
			}
			ret = snd_soc_dai_set_fmt(cpu_dai,
					SND_SOC_DAIFMT_CBM_CFM);
			if (ret < 0)
				pr_err("%s Set fmt for cpu dai failed\n",
					__func__);
		}
	}
err:
	afe_enable_lpass_core_shared_clock(SECONDARY_I2S_RX, CLOCK_OFF);
done:
	if (ret < 0)
		atomic_dec(&sec_mi2s_ref_count);
	return ret;
}

static struct snd_soc_ops mdm_mi2s_be_ops = {
	.startup = mdm_mi2s_startup,
	.shutdown = mdm_mi2s_shutdown,
};

static struct snd_soc_ops mdm_sec_mi2s_be_ops = {
	.startup = mdm_sec_mi2s_startup,
	.shutdown = mdm_sec_mi2s_shutdown,
};

static int mdm_mi2s_rate_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: mdm_i2s_rate  = %d", __func__,
		 mdm_mi2s_rate);
	ucontrol->value.integer.value[0] = mdm_mi2s_rate;
	return 0;
}

static int mdm_mi2s_rate_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		mdm_mi2s_rate = SAMPLE_RATE_8KHZ;
		break;
	case 1:
		mdm_mi2s_rate = SAMPLE_RATE_16KHZ;
		break;
	case 2:
	default:
		mdm_mi2s_rate = SAMPLE_RATE_48KHZ;
		break;
	}
	pr_debug("%s: mdm_mi2s_rate = %d ucontrol->value = %d\n",
		 __func__, mdm_mi2s_rate,
		 (int)ucontrol->value.integer.value[0]);
	return 0;
}

static int mdm_mi2s_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rt,
					      struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						      SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	rate->min = rate->max = mdm_mi2s_rate;
	channels->min = channels->max = mdm_mi2s_rx_ch;
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			SNDRV_PCM_FORMAT_S16_LE);
	return 0;
}

static int mdm_mi2s_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rt,
					     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						      SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	rate->min = rate->max = mdm_mi2s_rate;
	channels->min = channels->max = mdm_mi2s_tx_ch;
	return 0;
}


static int mdm_sec_mi2s_rate_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: mdm_sec_mi2s_rate  = %d", __func__,
		 mdm_sec_mi2s_rate);
	ucontrol->value.integer.value[0] = mdm_sec_mi2s_rate;
	return 0;
}

static int mdm_sec_mi2s_rate_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		mdm_sec_mi2s_rate = SAMPLE_RATE_8KHZ;
		break;
	case 1:
		mdm_sec_mi2s_rate = SAMPLE_RATE_16KHZ;
		break;
	case 2:
	default:
		mdm_sec_mi2s_rate = SAMPLE_RATE_48KHZ;
		break;
	}
	pr_debug("%s: mdm_sec_mi2s_rate = %d ucontrol->value = %d\n",
		 __func__, mdm_sec_mi2s_rate,
		 (int)ucontrol->value.integer.value[0]);
	return 0;
}

static int mdm_sec_mi2s_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rt,
					      struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						      SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	rate->min = rate->max = mdm_sec_mi2s_rate;
	channels->min = channels->max = mdm_sec_mi2s_rx_ch;
	return 0;
}

static int mdm_sec_mi2s_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rt,
					     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						      SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
						SNDRV_PCM_HW_PARAM_CHANNELS);
	rate->min = rate->max = mdm_sec_mi2s_rate;
	channels->min = channels->max = mdm_sec_mi2s_tx_ch;
	return 0;
}

static int mdm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rt,
				      struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
						      SNDRV_PCM_HW_PARAM_RATE);
	rate->min = rate->max = MDM_MI2S_RATE;
	return 0;
}

static int mdm_mi2s_rx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s mdm_mi2s_rx_ch %d\n", __func__,
		 mdm_mi2s_rx_ch);

	ucontrol->value.integer.value[0] = mdm_mi2s_rx_ch - 1;
	return 0;
}

static int mdm_mi2s_rx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	mdm_mi2s_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s mdm_mi2s_rx_ch %d\n", __func__,
		 mdm_mi2s_rx_ch);

	return 1;
}

static int mdm_mi2s_tx_ch_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s mdm_mi2s_tx_ch %d\n", __func__,
		 mdm_mi2s_tx_ch);

	ucontrol->value.integer.value[0] = mdm_mi2s_tx_ch - 1;
	return 0;
}

static int mdm_mi2s_tx_ch_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	mdm_mi2s_tx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s mdm_mi2s_tx_ch %d\n", __func__,
		 mdm_mi2s_tx_ch);

	return 1;
}

static int mdm_sec_mi2s_rx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s mdm_sec_mi2s_rx_ch %d\n", __func__,
		 mdm_sec_mi2s_rx_ch);

	ucontrol->value.integer.value[0] = mdm_sec_mi2s_rx_ch - 1;
	return 0;
}

static int mdm_sec_mi2s_rx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	mdm_sec_mi2s_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s mdm_sec_mi2s_rx_ch %d\n", __func__,
		 mdm_sec_mi2s_rx_ch);

	return 1;
}

static int mdm_sec_mi2s_tx_ch_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s mdm_sec_mi2s_tx_ch %d\n", __func__,
		 mdm_sec_mi2s_tx_ch);

	ucontrol->value.integer.value[0] = mdm_sec_mi2s_tx_ch - 1;
	return 0;
}

static int mdm_sec_mi2s_tx_ch_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	mdm_sec_mi2s_tx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s mdm_sec_mi2s_tx_ch %d\n", __func__,
		 mdm_sec_mi2s_tx_ch);

	return 1;
}

static int mdm_mi2s_mode_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s mdm_mi2s_mode %d\n", __func__,
		 mdm_mi2s_mode);

	ucontrol->value.integer.value[0] = mdm_mi2s_mode;
	return 0;
}

static int mdm_mi2s_mode_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		mdm_mi2s_mode = I2S_PCM_MASTER_MODE;
		break;
	case 1:
		mdm_mi2s_mode = I2S_PCM_SLAVE_MODE;
		break;
	default:
		mdm_mi2s_mode = I2S_PCM_MASTER_MODE;
		break;
	}
	pr_debug("%s: mdm_mi2s_mode = %d ucontrol->value = %d\n",
		 __func__, mdm_mi2s_mode,
		 (int)ucontrol->value.integer.value[0]);
	return 0;
}

static int mdm_sec_mi2s_mode_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s mdm_sec_mi2s_mode %d\n", __func__,
		 mdm_sec_mi2s_mode);

	ucontrol->value.integer.value[0] = mdm_sec_mi2s_mode;
	return 0;
}

static int mdm_sec_mi2s_mode_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		mdm_sec_mi2s_mode = I2S_PCM_MASTER_MODE;
		break;
	case 1:
		mdm_sec_mi2s_mode = I2S_PCM_SLAVE_MODE;
		break;
	default:
		mdm_sec_mi2s_mode = I2S_PCM_MASTER_MODE;
		break;
	}
	pr_debug("%s: mdm_sec_mi2s_mode = %d ucontrol->value = %d\n",
		 __func__, mdm_sec_mi2s_mode,
		 (int)ucontrol->value.integer.value[0]);
	return 0;
}

static int mdm_auxpcm_mode_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s mdm_auxpcm_mode %d\n", __func__,
		 mdm_auxpcm_mode);

	ucontrol->value.integer.value[0] = mdm_auxpcm_mode;
	return 0;
}

static int mdm_auxpcm_mode_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		mdm_auxpcm_mode = I2S_PCM_MASTER_MODE;
		break;
	case 1:
		mdm_auxpcm_mode = I2S_PCM_SLAVE_MODE;
		break;
	default:
		mdm_auxpcm_mode = I2S_PCM_MASTER_MODE;
		break;
	}
	pr_debug("%s: mdm_auxpcm_mode = %d ucontrol->value = %d\n",
		 __func__, mdm_auxpcm_mode,
		 (int)ucontrol->value.integer.value[0]);
	return 0;
}

static int mdm_sec_auxpcm_mode_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s mdm_sec_auxpcm_mode %d\n", __func__,
		 mdm_sec_auxpcm_mode);

	ucontrol->value.integer.value[0] = mdm_sec_auxpcm_mode;
	return 0;
}

static int mdm_sec_auxpcm_mode_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		mdm_sec_auxpcm_mode = I2S_PCM_MASTER_MODE;
		break;
	case 1:
		mdm_sec_auxpcm_mode = I2S_PCM_SLAVE_MODE;
		break;
	default:
		mdm_sec_auxpcm_mode = I2S_PCM_MASTER_MODE;
		break;
	}
	pr_debug("%s: mdm_sec_auxpcm_mode = %d ucontrol->value = %d\n",
		 __func__, mdm_sec_auxpcm_mode,
		 (int)ucontrol->value.integer.value[0]);
	return 0;
}

static int mdm_sec_tdm_mode_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s mdm_sec_tdm_mode %d\n", __func__,
		 mdm_sec_tdm_mode);

	ucontrol->value.integer.value[0] = mdm_sec_tdm_mode;
	return 0;
}

static int mdm_sec_tdm_mode_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		mdm_sec_tdm_mode = I2S_PCM_MASTER_MODE;
		break;
	case 1:
		mdm_sec_tdm_mode = I2S_PCM_SLAVE_MODE;
		break;
	default:
		mdm_sec_tdm_mode = I2S_PCM_MASTER_MODE;
		break;
	}
	pr_debug("%s: mdm_sec_tdm_mode = %d ucontrol->value = %d\n",
		 __func__, mdm_sec_tdm_mode,
		 (int)ucontrol->value.integer.value[0]);
	return 0;
}

static int mdm_pri_tdm_rx_0_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: mdm_pri_tdm_rx_0_ch = %d\n", __func__,
		mdm_pri_tdm_rx_0_ch);
	ucontrol->value.integer.value[0] = mdm_pri_tdm_rx_0_ch - 1;
	return 0;
}

static int mdm_pri_tdm_rx_0_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	mdm_pri_tdm_rx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: mdm_pri_tdm_rx_0_ch = %d\n", __func__,
		mdm_pri_tdm_rx_0_ch);
	return 0;
}

static int mdm_pri_tdm_tx_0_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: mdm_pri_tdm_tx_0_ch = %d\n", __func__,
		mdm_pri_tdm_tx_0_ch);
	ucontrol->value.integer.value[0] = mdm_pri_tdm_tx_0_ch - 1;
	return 0;
}

static int mdm_pri_tdm_tx_0_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	mdm_pri_tdm_tx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: mdm_pri_tdm_tx_0_ch = %d\n", __func__,
		mdm_pri_tdm_tx_0_ch);
	return 0;
}

static int mdm_sec_tdm_rx_0_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: mdm_sec_tdm_rx_0_ch = %d\n", __func__,
		mdm_sec_tdm_rx_0_ch);
	ucontrol->value.integer.value[0] = mdm_sec_tdm_rx_0_ch - 1;
	return 0;
}

static int mdm_sec_tdm_rx_0_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	mdm_sec_tdm_rx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: mdm_sec_tdm_rx_0_ch = %d\n", __func__,
		mdm_sec_tdm_rx_0_ch);
	return 0;
}

static int mdm_sec_tdm_tx_0_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: mdm_sec_tdm_tx_0_ch = %d\n", __func__,
		mdm_sec_tdm_tx_0_ch);
	ucontrol->value.integer.value[0] = mdm_sec_tdm_tx_0_ch - 1;
	return 0;
}

static int mdm_sec_tdm_tx_0_ch_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	mdm_sec_tdm_tx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: mdm_sec_tdm_tx_0_ch = %d\n", __func__,
		mdm_sec_tdm_tx_0_ch);
	return 0;
}

static int mdm_pri_tdm_rx_0_bit_format_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (mdm_pri_tdm_rx_0_bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		ucontrol->value.integer.value[0] = 3;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: mdm_pri_tdm_rx_0_bit_format = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int mdm_pri_tdm_rx_0_bit_format_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 3:
		mdm_pri_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	case 2:
		mdm_pri_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		mdm_pri_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		mdm_pri_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: mdm_pri_tdm_rx_0_bit_format = %d\n",
		 __func__, mdm_pri_tdm_rx_0_bit_format);
	return 0;
}

static int mdm_pri_tdm_tx_0_bit_format_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (mdm_pri_tdm_tx_0_bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		ucontrol->value.integer.value[0] = 3;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: mdm_pri_tdm_tx_0_bit_format = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int mdm_pri_tdm_tx_0_bit_format_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 3:
		mdm_pri_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	case 2:
		mdm_pri_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		mdm_pri_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		mdm_pri_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: mdm_pri_tdm_tx_0_bit_format = %d\n",
		 __func__, mdm_pri_tdm_tx_0_bit_format);
	return 0;
}

static int mdm_sec_tdm_rx_0_bit_format_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (mdm_sec_tdm_rx_0_bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		ucontrol->value.integer.value[0] = 3;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: mdm_sec_tdm_rx_0_bit_format = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int mdm_sec_tdm_rx_0_bit_format_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 3:
		mdm_sec_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	case 2:
		mdm_sec_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		mdm_sec_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		mdm_sec_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: mdm_sec_tdm_rx_0_bit_format = %d\n",
		 __func__, mdm_sec_tdm_rx_0_bit_format);
	return 0;
}

static int mdm_sec_tdm_tx_0_bit_format_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (mdm_sec_tdm_tx_0_bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		ucontrol->value.integer.value[0] = 3;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: mdm_sec_tdm_tx_0_bit_format = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int mdm_sec_tdm_tx_0_bit_format_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 3:
		mdm_sec_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	case 2:
		mdm_sec_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		mdm_sec_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		mdm_sec_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: mdm_sec_tdm_tx_0_bit_format = %d\n",
		 __func__, mdm_sec_tdm_tx_0_bit_format);
	return 0;
}

static int mdm_pri_tdm_rx_0_sample_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (mdm_pri_tdm_rx_0_sample_rate) {
	case SAMPLE_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 0;
		break;
	case SAMPLE_RATE_48KHZ:
	default:
		ucontrol->value.integer.value[0] = 1;
		break;
	}
	pr_debug("%s: mdm_pri_tdm_rx_0_sample_rate = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int  mdm_pri_tdm_rx_0_sample_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		mdm_pri_tdm_rx_0_sample_rate = SAMPLE_RATE_16KHZ;
		break;
	case 1:
	default:
		mdm_pri_tdm_rx_0_sample_rate = SAMPLE_RATE_48KHZ;
		break;
	}
	pr_debug("%s: mdm_pri_tdm_rx_0_sample_rate = %d\n",
		 __func__, mdm_pri_tdm_rx_0_sample_rate);
	return 0;
}

static int mdm_sec_tdm_rx_0_sample_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (mdm_sec_tdm_rx_0_sample_rate) {
	case SAMPLE_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 0;
		break;
	case SAMPLE_RATE_48KHZ:
	default:
		ucontrol->value.integer.value[0] = 1;
		break;
	}
	pr_debug("%s: mdm_sec_tdm_rx_0_sample_rate = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int  mdm_sec_tdm_rx_0_sample_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		mdm_sec_tdm_rx_0_sample_rate = SAMPLE_RATE_16KHZ;
		break;
	case 1:
	default:
		mdm_sec_tdm_rx_0_sample_rate = SAMPLE_RATE_48KHZ;
		break;
	}
	pr_debug("%s: mdm_sec_tdm_rx_0_sample_rate = %d\n",
		 __func__, mdm_sec_tdm_rx_0_sample_rate);
	return 0;
}

static int mdm_pri_tdm_tx_0_sample_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (mdm_pri_tdm_tx_0_sample_rate) {
	case SAMPLE_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 0;
		break;
	case SAMPLE_RATE_48KHZ:
	default:
		ucontrol->value.integer.value[0] = 1;
		break;
	}
	pr_debug("%s: mdm_pri_tdm_tx_0_sample_rate = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int  mdm_pri_tdm_tx_0_sample_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		mdm_pri_tdm_tx_0_sample_rate = SAMPLE_RATE_16KHZ;
		break;
	case 1:
	default:
		mdm_pri_tdm_tx_0_sample_rate = SAMPLE_RATE_48KHZ;
		break;
	}
	pr_debug("%s: mdm_pri_tdm_tx_0_sample_rate = %d\n",
		 __func__, mdm_pri_tdm_tx_0_sample_rate);
	return 0;
}

static int mdm_sec_tdm_tx_0_sample_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (mdm_sec_tdm_tx_0_sample_rate) {
	case SAMPLE_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 0;
		break;
	case SAMPLE_RATE_48KHZ:
	default:
		ucontrol->value.integer.value[0] = 1;
		break;
	}
	pr_debug("%s: mdm_sec_tdm_tx_0_sample_rate = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int  mdm_sec_tdm_tx_0_sample_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		mdm_sec_tdm_tx_0_sample_rate = SAMPLE_RATE_16KHZ;
		break;
	case 1:
	default:
		mdm_sec_tdm_tx_0_sample_rate = SAMPLE_RATE_48KHZ;
		break;
	}
	pr_debug("%s: mdm_sec_tdm_tx_0_sample_rate = %d\n",
		 __func__, mdm_sec_tdm_tx_0_sample_rate);
	return 0;
}

static int  mdm_tdm_num_slots_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{

	switch (ucontrol->value.integer.value[0]) {
	case 1:
		mdm_tdm_num_slots = 16;
		break;
	case 0:
	default:
		mdm_tdm_num_slots = 8;
		break;
	}
	pr_debug("%s: mdm_tdm_num_slots = %d\n",
		 __func__, mdm_tdm_num_slots);
	return 0;
}


static int mdm_mi2s_get_spk(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s mdm_spk_control %d", __func__, mdm_spk_control);

	ucontrol->value.integer.value[0] = mdm_spk_control;
	return 0;
}


static int mdm_tdm_num_slots_get(struct snd_kcontrol *kcontrol,
		       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s mdm_tdm_num_slots %d\n", __func__, mdm_tdm_num_slots);

	ucontrol->value.integer.value[0] = mdm_tdm_num_slots;
	return 0;
}

static void mdm_ext_control(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	pr_debug("%s mdm_spk_control %d", __func__, mdm_spk_control);

	mutex_lock(&codec->mutex);
	if (mdm_spk_control == MDM_SPK_ON) {
		snd_soc_dapm_enable_pin(dapm, "Ext Spk Bottom Pos");
		snd_soc_dapm_enable_pin(dapm, "Ext Spk Bottom Neg");
		snd_soc_dapm_enable_pin(dapm, "Ext Spk Top Pos");
		snd_soc_dapm_enable_pin(dapm, "Ext Spk Top Neg");
	} else {
		snd_soc_dapm_disable_pin(dapm, "Ext Spk Bottom Pos");
		snd_soc_dapm_disable_pin(dapm, "Ext Spk Bottom Neg");
		snd_soc_dapm_disable_pin(dapm, "Ext Spk Top Pos");
		snd_soc_dapm_disable_pin(dapm, "Ext Spk Top Neg");
	}
	snd_soc_dapm_sync(dapm);
	mutex_unlock(&codec->mutex);
}

static int mdm_mi2s_set_spk(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	pr_debug("%s()\n", __func__);

	if (mdm_spk_control == ucontrol->value.integer.value[0])
		return 0;
	mdm_spk_control = ucontrol->value.integer.value[0];
	mdm_ext_control(codec);
	return 1;
}

static int mdm_enable_codec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm)
{
	int ret = 0;
	struct snd_soc_card *card = codec->component.card;
	struct mdm_machine_data *pdata =
			snd_soc_card_get_drvdata(card);
	struct afe_clk_set *lpass_clk = NULL;

	pr_debug("%s enable %d  codec name %s\n",
		 __func__, enable, codec->component.name);

	lpass_clk = kzalloc(sizeof(struct afe_clk_set), GFP_KERNEL);
	if (!lpass_clk)
		return -ENOMEM;

	mutex_lock(&cdc_mclk_mutex);
	memcpy(lpass_clk, &lpass_default, sizeof(struct afe_clk_cfg));
	if (enable) {
		if (atomic_read(&pdata->prim_clk_usrs) == 0) {
			lpass_clk->clk_id = Q6AFE_LPASS_CLK_ID_MCLK_3;
			lpass_clk->clk_freq_in_hz = pdata->mclk_freq;
			lpass_clk->enable = enable;
			ret = afe_set_lpass_clock_v2(
				AFE_PORT_ID_PRIMARY_MI2S_RX, lpass_clk);
			if (ret < 0) {
				pr_err("%s afe_set_lpass_clock_v2 failed\n",
				       __func__);

				goto err;
			}
		}
		atomic_inc(&pdata->prim_clk_usrs);
		pdata->mdm9607_codec_fn.mclk_enable_fn(codec, 1, dapm);
	} else {
		if (atomic_read(&pdata->prim_clk_usrs) > 0)
			atomic_dec(&pdata->prim_clk_usrs);
		if (atomic_read(&pdata->prim_clk_usrs) == 0) {
			lpass_clk->clk_id = Q6AFE_LPASS_CLK_ID_MCLK_3;
			lpass_clk->enable = enable;
			ret = afe_set_lpass_clock_v2(
				AFE_PORT_ID_PRIMARY_MI2S_RX, lpass_clk);
			if (ret < 0) {
				pr_err("%s afe_set_lpass_clock_v2 failed\n",
				       __func__);

				goto err;
			}
		}
		pdata->mdm9607_codec_fn.mclk_enable_fn(codec, 0, dapm);
	}
	pr_debug("%s clk %x\n",  __func__, pdata->mclk_freq);
err:
	mutex_unlock(&cdc_mclk_mutex);
	kfree(lpass_clk);
	clk_users = atomic_read(&pdata->prim_clk_usrs);
	return ret;
}

static int mdm_mclk_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s event %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return mdm_enable_codec_ext_clk(w->codec, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return mdm_enable_codec_ext_clk(w->codec, 0, true);
	}
	return 0;
}

static void mdm_auxpcm_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int ret;
	struct snd_soc_card *card = rtd->card;
	struct mdm_machine_data *pdata = snd_soc_card_get_drvdata(card);

	if (atomic_dec_return(&aux_ref_count) == 0) {
		if (pdata->prim_auxpcm_mode == 1)
			ret = wcd_gpio_ctrl_select_sleep_state
						(pdata->prim_master_p);
		else
			ret = wcd_gpio_ctrl_select_sleep_state
						(pdata->prim_slave_p);
		if (ret)
			pr_err("%s: failed to set pri gpios to sleep: %d\n",
					__func__, ret);
	}
}

static int mdm_auxpcm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct mdm_machine_data *pdata = snd_soc_card_get_drvdata(card);
	int ret = 0;

	pdata->prim_auxpcm_mode = mdm_auxpcm_mode;
	if (atomic_inc_return(&aux_ref_count) == 1) {
		if (pdata->lpaif_pri_muxsel_virt_addr != NULL) {
			ret = afe_enable_lpass_core_shared_clock(MI2S_RX,
								 CLOCK_ON);
			if (ret < 0) {
				ret = -EINVAL;
				goto done;
			}
			iowrite32(I2S_PCM_SEL << I2S_PCM_SEL_OFFSET,
				  pdata->lpaif_pri_muxsel_virt_addr);

			if (pdata->lpass_mux_spkr_ctl_virt_addr != NULL) {
				if (pdata->prim_auxpcm_mode == 1)
					iowrite32(PRI_TLMM_CLKS_EN_MASTER,
					pdata->lpass_mux_spkr_ctl_virt_addr);
				else
					iowrite32(PRI_TLMM_CLKS_EN_SLAVE,
					pdata->lpass_mux_spkr_ctl_virt_addr);
			} else {
				pr_err("%s lpass_mux_spkr_ctl_virt_addr is NULL\n",
					__func__);
				ret = -EINVAL;
				goto err;
			}
		} else {
			pr_err("%s lpaif_pri_muxsel_virt_addr is NULL\n",
			       __func__);
			ret = -EINVAL;
			goto done;
		}
		if (pdata->prim_auxpcm_mode == 1) {
			ret = wcd_gpio_ctrl_select_active_state
						(pdata->prim_master_p);
			if (ret < 0) {
				pr_err("%s pinctrl set failed\n", __func__);
				goto err;
			}
		} else {
			ret = wcd_gpio_ctrl_select_active_state
						(pdata->prim_slave_p);
			if (ret < 0) {
				pr_err("%s pinctrl set failed\n", __func__);
				goto err;
			}
		}
	}
err:
	afe_enable_lpass_core_shared_clock(MI2S_RX, CLOCK_OFF);
done:
	if (ret < 0)
		atomic_dec(&aux_ref_count);
	return ret;
}

static void mdm_sec_auxpcm_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int ret;
	struct snd_soc_card *card = rtd->card;
	struct mdm_machine_data *pdata = snd_soc_card_get_drvdata(card);

	if (atomic_dec_return(&sec_aux_ref_count) == 0) {
		if (pdata->sec_auxpcm_mode == 1)
			ret = wcd_gpio_ctrl_select_sleep_state
						(pdata->sec_master_p);
		else
			ret = wcd_gpio_ctrl_select_sleep_state
						(pdata->sec_slave_p);
		if (ret)
			pr_err("%s: failed to set sec gpios to sleep: %d\n",
					__func__, ret);
	}
}

static int mdm_sec_auxpcm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct mdm_machine_data *pdata = snd_soc_card_get_drvdata(card);
	int ret = 0;

	pdata->sec_auxpcm_mode = mdm_sec_auxpcm_mode;
	if (atomic_inc_return(&sec_aux_ref_count) == 1) {
		if (pdata->lpaif_sec_muxsel_virt_addr != NULL) {
			ret = afe_enable_lpass_core_shared_clock(
				SECONDARY_I2S_RX, CLOCK_ON);
			if (ret < 0) {
				ret = -EINVAL;
				goto done;
			}

			iowrite32(I2S_PCM_SEL << I2S_PCM_SEL_OFFSET,
				pdata->lpaif_sec_muxsel_virt_addr);

			if (pdata->lpass_mux_mic_ctl_virt_addr != NULL) {
				if (pdata->sec_auxpcm_mode == 1)
					iowrite32(SEC_TLMM_CLKS_EN_MASTER,
					pdata->lpass_mux_mic_ctl_virt_addr);
				else
					iowrite32(SEC_TLMM_CLKS_EN_SLAVE,
					pdata->lpass_mux_mic_ctl_virt_addr);
			} else {
				pr_err("%s lpass_mux_mic_ctl_virt_addr is NULL\n",
					__func__);
				ret = -EINVAL;
				goto err;
			}
		} else {
			pr_err("%s lpaif_sec_muxsel_virt_addr is NULL\n",
			       __func__);
			ret = -EINVAL;
			goto done;
		}
		if (pdata->sec_auxpcm_mode == 1) {
			ret = wcd_gpio_ctrl_select_active_state
						(pdata->sec_master_p);
			if (ret < 0) {
				pr_err("%s pinctrl set failed\n", __func__);
				goto err;
			}
		} else {
			ret = wcd_gpio_ctrl_select_active_state
						(pdata->sec_slave_p);
			if (ret < 0) {
				pr_err("%s pinctrl set failed\n", __func__);
				goto err;
			}
		}
	}
err:
	afe_enable_lpass_core_shared_clock(SECONDARY_I2S_RX, CLOCK_OFF);
done:
	if (ret < 0)
		atomic_dec(&sec_aux_ref_count);
	return ret;
}

static struct snd_soc_ops mdm_auxpcm_be_ops = {
	.startup = mdm_auxpcm_startup,
	.shutdown = mdm_auxpcm_shutdown,
};

static struct snd_soc_ops mdm_sec_auxpcm_be_ops = {
	.startup = mdm_sec_auxpcm_startup,
	.shutdown = mdm_sec_auxpcm_shutdown,
};

static int mdm_auxpcm_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mdm_auxpcm_rate;
	return 0;
}

static int mdm_auxpcm_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		mdm_auxpcm_rate = SAMPLE_RATE_8KHZ;
		break;
	case 1:
		mdm_auxpcm_rate = SAMPLE_RATE_16KHZ;
		break;
	default:
		mdm_auxpcm_rate = SAMPLE_RATE_8KHZ;
		break;
	}
	return 0;
}

static int mdm_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
					  struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate =
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels =
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = mdm_auxpcm_rate;
	channels->min = channels->max = 1;

	return 0;
}

static const struct snd_soc_dapm_widget mdm9607_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	mdm_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SPK("Lineout_1 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_3 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_2 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_4 amp", NULL),
	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCRight Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCLeft Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Analog Mic4", NULL),
	SND_SOC_DAPM_MIC("Analog Mic6", NULL),
	SND_SOC_DAPM_MIC("Analog Mic7", NULL),
	SND_SOC_DAPM_MIC("Analog Mic8", NULL),

	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
	SND_SOC_DAPM_MIC("Digital Mic3", NULL),
	SND_SOC_DAPM_MIC("Digital Mic4", NULL),
	SND_SOC_DAPM_MIC("Digital Mic5", NULL),
	SND_SOC_DAPM_MIC("Digital Mic6", NULL),
};

static const char *const spk_function[] = {"Off", "On"};
static const char *const mi2s_rx_ch_text[] = {"One", "Two"};
static const char *const mi2s_tx_ch_text[] = {"One", "Two"};

static const char *const auxpcm_rate_text[] = {"rate_8000", "rate_16000"};
static const char *const mi2s_rate_text[] = {"rate_8000",
						"rate_16000", "rate_48000"};
static const char *const mode_text[] = {"master", "slave"};

static const struct soc_enum mdm_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, mi2s_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, mi2s_tx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, auxpcm_rate_text),
	SOC_ENUM_SINGLE_EXT(3, mi2s_rate_text),
	SOC_ENUM_SINGLE_EXT(2, mode_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_ch_text),
				tdm_ch_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_bit_format_text),
				tdm_bit_format_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_sample_rate_text),
				tdm_sample_rate_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_num_slots_text),
				tdm_num_slots_text),

};

static const struct snd_kcontrol_new mdm_snd_controls[] = {
	SOC_ENUM_EXT("Speaker Function",   mdm_enum[0],
				 mdm_mi2s_get_spk,
				 mdm_mi2s_set_spk),
	SOC_ENUM_EXT("MI2S_RX Channels",   mdm_enum[1],
				 mdm_mi2s_rx_ch_get,
				 mdm_mi2s_rx_ch_put),
	SOC_ENUM_EXT("MI2S_TX Channels",   mdm_enum[2],
				 mdm_mi2s_tx_ch_get,
				 mdm_mi2s_tx_ch_put),
	SOC_ENUM_EXT("AUX PCM SampleRate", mdm_enum[3],
				 mdm_auxpcm_rate_get,
				 mdm_auxpcm_rate_put),
	SOC_ENUM_EXT("MI2S SampleRate", mdm_enum[4],
				 mdm_mi2s_rate_get,
				 mdm_mi2s_rate_put),
	SOC_ENUM_EXT("SEC_MI2S_RX Channels", mdm_enum[1],
				 mdm_sec_mi2s_rx_ch_get,
				 mdm_sec_mi2s_rx_ch_put),
	SOC_ENUM_EXT("SEC_MI2S_TX Channels", mdm_enum[2],
				 mdm_sec_mi2s_tx_ch_get,
				 mdm_sec_mi2s_tx_ch_put),
	SOC_ENUM_EXT("SEC_MI2S SampleRate", mdm_enum[4],
				 mdm_sec_mi2s_rate_get,
				 mdm_sec_mi2s_rate_put),
	SOC_ENUM_EXT("MI2S Mode", mdm_enum[5],
				 mdm_mi2s_mode_get,
				 mdm_mi2s_mode_put),
	SOC_ENUM_EXT("SEC_MI2S Mode", mdm_enum[5],
				 mdm_sec_mi2s_mode_get,
				 mdm_sec_mi2s_mode_put),
	SOC_ENUM_EXT("AUXPCM Mode", mdm_enum[5],
				 mdm_auxpcm_mode_get,
				 mdm_auxpcm_mode_put),
	SOC_ENUM_EXT("SEC_AUXPCM Mode", mdm_enum[5],
				 mdm_sec_auxpcm_mode_get,
				 mdm_sec_auxpcm_mode_put),
	SOC_ENUM_EXT("SEC_TDM Mode", mdm_enum[5],
				 mdm_sec_tdm_mode_get,
				 mdm_sec_tdm_mode_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 Channels", mdm_enum[6],
			mdm_pri_tdm_rx_0_ch_get, mdm_pri_tdm_rx_0_ch_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Channels", mdm_enum[6],
			mdm_pri_tdm_tx_0_ch_get, mdm_pri_tdm_tx_0_ch_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 Channels", mdm_enum[6],
			mdm_sec_tdm_rx_0_ch_get, mdm_sec_tdm_rx_0_ch_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 Channels", mdm_enum[6],
			mdm_sec_tdm_tx_0_ch_get, mdm_sec_tdm_tx_0_ch_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 Bit Format", mdm_enum[7],
			mdm_pri_tdm_rx_0_bit_format_get,
			mdm_pri_tdm_rx_0_bit_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Bit Format", mdm_enum[7],
			mdm_pri_tdm_tx_0_bit_format_get,
			mdm_pri_tdm_tx_0_bit_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 Bit Format", mdm_enum[7],
			mdm_sec_tdm_rx_0_bit_format_get,
			mdm_sec_tdm_rx_0_bit_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 Bit Format", mdm_enum[7],
			mdm_sec_tdm_tx_0_bit_format_get,
			mdm_sec_tdm_tx_0_bit_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 SampleRate", mdm_enum[8],
			mdm_pri_tdm_rx_0_sample_rate_get,
			mdm_pri_tdm_rx_0_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 SampleRate", mdm_enum[8],
			mdm_pri_tdm_tx_0_sample_rate_get,
			mdm_pri_tdm_tx_0_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 SampleRate", mdm_enum[8],
			mdm_sec_tdm_rx_0_sample_rate_get,
			mdm_sec_tdm_rx_0_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 SampleRate", mdm_enum[8],
			mdm_sec_tdm_tx_0_sample_rate_get,
			mdm_sec_tdm_tx_0_sample_rate_put),
	SOC_ENUM_EXT("TDM Slots", mdm_enum[9],
			mdm_tdm_num_slots_get,
			mdm_tdm_num_slots_put),
};
static int mdm_tdm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	switch (cpu_dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
		channels->min = channels->max = mdm_pri_tdm_rx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mdm_pri_tdm_rx_0_bit_format);
		rate->min = rate->max = mdm_pri_tdm_rx_0_sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		channels->min = channels->max = mdm_pri_tdm_tx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mdm_pri_tdm_tx_0_bit_format);
		rate->min = rate->max = mdm_pri_tdm_tx_0_sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
		channels->min = channels->max = mdm_sec_tdm_rx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mdm_sec_tdm_rx_0_bit_format);
		rate->min = rate->max = mdm_sec_tdm_rx_0_sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX:
		channels->min = channels->max = mdm_sec_tdm_tx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mdm_sec_tdm_tx_0_bit_format);
		rate->min = rate->max = mdm_sec_tdm_tx_0_sample_rate;
		break;
	default:
		pr_err("%s: dai id 0x%x not supported\n",
			__func__, cpu_dai->id);
		return -EINVAL;
	}

	pr_debug("%s: dai id = 0x%x channels = %d rate = %d\n",
		__func__, cpu_dai->id, channels->max, rate->max);

	return 0;
}

static unsigned int tdm_param_set_slot_mask(u16 port_id,
				int slot_width, int slots)
{
	unsigned int slot_mask = 0;
	int upper, lower, i, j;
	unsigned int *slot_offset;

	switch (port_id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
		lower = PRIMARY_TDM_RX_0;
		upper = PRIMARY_TDM_RX_0;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		lower = PRIMARY_TDM_TX_0;
		upper = PRIMARY_TDM_TX_0;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
		lower = SECONDARY_TDM_RX_0;
		upper = SECONDARY_TDM_RX_0;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX:
		lower = SECONDARY_TDM_TX_0;
		upper = SECONDARY_TDM_TX_0;
		break;
	default:
		return slot_mask;
	}

	for (i = lower; i <= upper; i++) {
		slot_offset = tdm_slot_offset[i];
		for (j = 0; j < TDM_SLOT_OFFSET_MAX; j++) {
			if (slot_offset[j] != AFE_SLOT_MAPPING_OFFSET_INVALID)
				/*
				 * set the mask of active slot according to
				 * the offset table for the group of devices
				 */
				slot_mask |=
				    (1 << ((slot_offset[j] * 8) / slot_width));
			else
				break;
		}
	}

	return slot_mask;
}

static int mdm_tdm_snd_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	int channels, slot_width, slots;
	unsigned int slot_mask;
	unsigned int *slot_offset;
	int offset_channels = 0;
	int i;

	pr_debug("%s: dai id = 0x%x\n", __func__, cpu_dai->id);

	channels = params_channels(params);
	switch (channels) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 6:
	case 8:
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S32_LE:
		case SNDRV_PCM_FORMAT_S24_LE:
		case SNDRV_PCM_FORMAT_S16_LE:
			/*
			 * up to 8 channel HW configuration should
			 * use 32 bit slot width for max support of
			 * stream bit width. (slot_width > bit_width)
			 */
			slot_width = 32;
			break;
		default:
			pr_err("%s: invalid param format 0x%x\n",
				__func__, params_format(params));
			return -EINVAL;
		}
		slots = mdm_tdm_num_slots;
		slot_mask = tdm_param_set_slot_mask(cpu_dai->id,
			slot_width, slots);
		if (!slot_mask) {
			pr_err("%s: invalid slot_mask 0x%x\n",
				__func__, slot_mask);
			return -EINVAL;
		}
		break;
	default:
		pr_err("%s: invalid param channels %d\n",
			__func__, channels);
		return -EINVAL;
	}
	switch (cpu_dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
		slot_offset = tdm_slot_offset[PRIMARY_TDM_RX_0];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		slot_offset = tdm_slot_offset[PRIMARY_TDM_TX_0];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
		slot_offset = tdm_slot_offset[SECONDARY_TDM_RX_0];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX:
		slot_offset = tdm_slot_offset[SECONDARY_TDM_TX_0];
		break;
	default:
		pr_err("%s: dai id 0x%x not supported\n",
			__func__, cpu_dai->id);
		return -EINVAL;
	}

	for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
		if (slot_offset[i] != AFE_SLOT_MAPPING_OFFSET_INVALID)
			offset_channels++;
		else
			break;
	}
	if (offset_channels == 0) {
		pr_err("%s: slot offset not supported, offset_channels %d\n",
			__func__, offset_channels);
		return -EINVAL;
	}

	if (channels > offset_channels) {
		pr_err("%s: channels %d exceed offset_channels %d\n",
			__func__, channels, offset_channels);
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = snd_soc_dai_set_tdm_slot(cpu_dai, 0, slot_mask,
			slots, slot_width);
		if (ret < 0) {
			pr_err("%s: failed to set tdm slot, err:%d\n",
				__func__, ret);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai,
			0, NULL, channels, slot_offset);
		if (ret < 0) {
			pr_err("%s: failed to set channel map, err:%d\n",
				__func__, ret);
			goto end;
		}
	} else {
		ret = snd_soc_dai_set_tdm_slot(cpu_dai, slot_mask, 0,
			slots, slot_width);
		if (ret < 0) {
			pr_err("%s: failed to set tdm slot, err:%d\n",
				__func__, ret);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai,
			channels, slot_offset, 0, NULL);
		if (ret < 0) {
			pr_err("%s: failed to set channel map, err:%d\n",
				__func__, ret);
			goto end;
		}
	}

end:
	return ret;
}

static int mdm_tdm_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct mdm_machine_data *pdata =
			snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
			 substream->name, substream->stream);
	pr_debug("%s: dai id = 0x%x\n", __func__, cpu_dai->id);

	switch (cpu_dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
	case AFE_PORT_ID_PRIMARY_TDM_RX_4:
	case AFE_PORT_ID_PRIMARY_TDM_RX_5:
	case AFE_PORT_ID_PRIMARY_TDM_RX_6:
	case AFE_PORT_ID_PRIMARY_TDM_RX_7:
	case AFE_PORT_ID_PRIMARY_TDM_TX:
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
	case AFE_PORT_ID_PRIMARY_TDM_TX_4:
	case AFE_PORT_ID_PRIMARY_TDM_TX_5:
	case AFE_PORT_ID_PRIMARY_TDM_TX_6:
	case AFE_PORT_ID_PRIMARY_TDM_TX_7:
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
	case AFE_PORT_ID_SECONDARY_TDM_RX_1:
	case AFE_PORT_ID_SECONDARY_TDM_RX_2:
	case AFE_PORT_ID_SECONDARY_TDM_RX_3:
	case AFE_PORT_ID_SECONDARY_TDM_RX_4:
	case AFE_PORT_ID_SECONDARY_TDM_RX_5:
	case AFE_PORT_ID_SECONDARY_TDM_RX_6:
	case AFE_PORT_ID_SECONDARY_TDM_RX_7:
	case AFE_PORT_ID_SECONDARY_TDM_TX:
	case AFE_PORT_ID_SECONDARY_TDM_TX_1:
	case AFE_PORT_ID_SECONDARY_TDM_TX_2:
	case AFE_PORT_ID_SECONDARY_TDM_TX_3:
	case AFE_PORT_ID_SECONDARY_TDM_TX_4:
	case AFE_PORT_ID_SECONDARY_TDM_TX_5:
	case AFE_PORT_ID_SECONDARY_TDM_TX_6:
	case AFE_PORT_ID_SECONDARY_TDM_TX_7:

	pdata->sec_tdm_mode = mdm_sec_tdm_mode;
	if (atomic_inc_return(&sec_tdm_ref_count) == 1) {
		if (pdata->lpaif_sec_muxsel_virt_addr != NULL) {
			iowrite32(I2S_PCM_SEL << I2S_PCM_SEL_OFFSET,
				pdata->lpaif_sec_muxsel_virt_addr);

			if (pdata->lpass_mux_mic_ctl_virt_addr != NULL) {
				if (pdata->sec_tdm_mode == 1) {
					iowrite32(SEC_TLMM_CLKS_EN_MASTER,
					pdata->lpass_mux_mic_ctl_virt_addr);
					}
				else{
					iowrite32(SEC_TLMM_CLKS_EN_SLAVE,
					pdata->lpass_mux_mic_ctl_virt_addr);
				}

			} else {
				pr_err("%s lpass_mux_mic_ctl_virt_addr is NULL\n",
					__func__);
				ret = -EINVAL;
				goto err;
			}
		} else {
			pr_err("%s lpaif_sec_muxsel_virt_addr is NULL\n",
			       __func__);
			ret = -EINVAL;
			goto done;
		}
		if (pdata->sec_tdm_mode == 1) {
			ret = wcd_gpio_ctrl_select_active_state
						(pdata->sec_master_p);
			if (ret < 0) {
				pr_err("%s pinctrl set failed\n", __func__);
				goto err;
			}
		} else {
			ret = wcd_gpio_ctrl_select_active_state
						(pdata->sec_slave_p);
			if (ret < 0) {
				pr_err("%s pinctrl set failed\n", __func__);
				goto err;
			}
		}
	}
		break;
	default:
		pr_err("%s: dai id 0x%x not supported\n",
		       __func__, cpu_dai->id);
		break;
		return -EINVAL;
	}
err:
done:
	if (ret < 0)
		atomic_dec(&sec_tdm_ref_count);
	return ret;

}

static void mdm_tdm_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct mdm_machine_data *pdata =
			snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret;

	switch (cpu_dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
	case AFE_PORT_ID_PRIMARY_TDM_RX_4:
	case AFE_PORT_ID_PRIMARY_TDM_RX_5:
	case AFE_PORT_ID_PRIMARY_TDM_RX_6:
	case AFE_PORT_ID_PRIMARY_TDM_RX_7:
	case AFE_PORT_ID_PRIMARY_TDM_TX:
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
	case AFE_PORT_ID_PRIMARY_TDM_TX_4:
	case AFE_PORT_ID_PRIMARY_TDM_TX_5:
	case AFE_PORT_ID_PRIMARY_TDM_TX_6:
	case AFE_PORT_ID_PRIMARY_TDM_TX_7:
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
	case AFE_PORT_ID_SECONDARY_TDM_RX_1:
	case AFE_PORT_ID_SECONDARY_TDM_RX_2:
	case AFE_PORT_ID_SECONDARY_TDM_RX_3:
	case AFE_PORT_ID_SECONDARY_TDM_RX_4:
	case AFE_PORT_ID_SECONDARY_TDM_RX_5:
	case AFE_PORT_ID_SECONDARY_TDM_RX_6:
	case AFE_PORT_ID_SECONDARY_TDM_RX_7:
	case AFE_PORT_ID_SECONDARY_TDM_TX:
	case AFE_PORT_ID_SECONDARY_TDM_TX_1:
	case AFE_PORT_ID_SECONDARY_TDM_TX_2:
	case AFE_PORT_ID_SECONDARY_TDM_TX_3:
	case AFE_PORT_ID_SECONDARY_TDM_TX_4:
	case AFE_PORT_ID_SECONDARY_TDM_TX_5:
	case AFE_PORT_ID_SECONDARY_TDM_TX_6:
	case AFE_PORT_ID_SECONDARY_TDM_TX_7:
		if (atomic_dec_return(&sec_aux_ref_count) == 0) {
			if (pdata->sec_tdm_mode == 1)
				ret = wcd_gpio_ctrl_select_sleep_state
					(pdata->sec_master_p);
			else
				ret = wcd_gpio_ctrl_select_sleep_state
					(pdata->sec_slave_p);
			if (ret)
				pr_err("failed to set sec gpios to sleep:%d\n",
					ret);
		}
		break;
	}
}

static struct snd_soc_ops mdm_tdm_be_ops = {
	.startup = mdm_tdm_startup,
	.hw_params = mdm_tdm_snd_hw_params,
	.shutdown = mdm_tdm_shutdown,
};

static int msm_snd_get_ext_clk_cnt(void)
{
	return clk_users;
}

static int mdm_mi2s_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	struct mdm_machine_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s dev_name %s\n", __func__, dev_name(cpu_dai->dev));

	rtd->pmdown_time = 0;
	ret = snd_soc_add_codec_controls(codec, mdm_snd_controls,
					 ARRAY_SIZE(mdm_snd_controls));
	if (ret < 0)
		goto done;

	snd_soc_dapm_new_controls(dapm, mdm9607_dapm_widgets,
				  ARRAY_SIZE(mdm9607_dapm_widgets));

	/*
	 * After DAPM Enable pins always
	 * DAPM SYNC needs to be called.
	 */
	snd_soc_dapm_enable_pin(dapm, "Lineout_1 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_3 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_2 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_4 amp");

	snd_soc_dapm_ignore_suspend(dapm, "Lineout_1 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_3 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_2 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_4 amp");
	snd_soc_dapm_ignore_suspend(dapm, "ultrasound amp");
	snd_soc_dapm_ignore_suspend(dapm, "Handset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "ANCRight Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "ANCLeft Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic1");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic2");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic3");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic4");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic5");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic6");

	snd_soc_dapm_ignore_suspend(dapm, "MADINPUT");
	snd_soc_dapm_ignore_suspend(dapm, "EAR");
	snd_soc_dapm_ignore_suspend(dapm, "HEADPHONE");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT1");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT2");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT3");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT4");
	snd_soc_dapm_ignore_suspend(dapm, "SPK_OUT");
	snd_soc_dapm_ignore_suspend(dapm, "ANC HEADPHONE");
	snd_soc_dapm_ignore_suspend(dapm, "ANC EAR");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC4");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC5");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC6");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC4");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC5");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC6");

	snd_soc_dapm_sync(dapm);

	ret = afe_enable_lpass_core_shared_clock(MI2S_RX, CLOCK_ON);
	if (ret < 0) {
		ret = -EINVAL;
		goto done;
	}

	mdm_gpio_set_mux_ctl(pdata);

	ret = afe_enable_lpass_core_shared_clock(MI2S_RX, CLOCK_OFF);

	mbhc_cfg.calibration = def_codec_mbhc_cal();
	if (mbhc_cfg.calibration) {
		ret = pdata->mdm9607_codec_fn.mbhc_hs_detect(codec, &mbhc_cfg);
		if (ret < 0) {
			pr_err("%s: Failed to intialise mbhc %d\n",
				__func__, ret);
			kfree(mbhc_cfg.calibration);
		}
	} else {
		ret = -ENOMEM;
	}

	if (!strcmp(card->name, "mdm9607-tomtom-i2s-snd-card")) {
		tomtom_register_ext_clk_cb(mdm_enable_codec_ext_clk,
					   msm_snd_get_ext_clk_cnt,
					   rtd->codec);

		ret = mdm_enable_codec_ext_clk(rtd->codec, 1, false);
		if (IS_ERR_VALUE(ret)) {
			pr_err("%s: Failed to enable mclk, err = 0x%x\n",
				__func__, ret);
			goto done;
		}

		tomtom_enable_qfuse_sensing(rtd->codec);

		ret = mdm_enable_codec_ext_clk(rtd->codec, 0, false);
		if (IS_ERR_VALUE(ret)) {
			pr_err("%s: Failed to disable mclk, err = 0x%x\n",
				__func__, ret);
		}
	}
done:
	return ret;
}

void *def_codec_mbhc_cal(void)
{
	void *tomtom_cal;
	struct wcd9xxx_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_low, *btn_high;
	u8 *n_ready, *n_cic, *gain;

	tomtom_cal = kzalloc(WCD9XXX_MBHC_CAL_SIZE(WCD9XXX_MBHC_DEF_BUTTONS,
						  WCD9XXX_MBHC_DEF_RLOADS),
			    GFP_KERNEL);
	if (!tomtom_cal) {
		pr_err("%s Out of memory\n", __func__);

		return NULL;
	}

#define S(X, Y) ((WCD9XXX_MBHC_CAL_GENERAL_PTR(tomtom_cal)->X) = (Y))
	S(t_ldoh, 100);
	S(t_bg_fast_settle, 100);
	S(t_shutdown_plug_rem, 255);
	S(mbhc_nsa, 4);
	S(mbhc_navg, 4);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_PLUG_DET_PTR(tomtom_cal)->X) = (Y))
	S(mic_current, TOMTOM_PID_MIC_5_UA);
	S(hph_current, TOMTOM_PID_MIC_5_UA);
	S(t_mic_pid, 100);
	S(t_ins_complete, 250);
	S(t_ins_retry, 200);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_PLUG_TYPE_PTR(tomtom_cal)->X) = (Y))
	S(v_no_mic, 30);
	S(v_hs_max, 2400);
#undef S
#define S(X, Y) ((WCD9XXX_MBHC_CAL_BTN_DET_PTR(tomtom_cal)->X) = (Y))
	S(c[0], 62);
	S(c[1], 124);
	S(nc, 1);
	S(n_meas, 3);
	S(mbhc_nsc, 11);
	S(n_btn_meas, 1);
	S(n_btn_con, 2);
	S(num_btn, WCD9XXX_MBHC_DEF_BUTTONS);
	S(v_btn_press_delta_sta, 100);
	S(v_btn_press_delta_cic, 50);
#undef S
	btn_cfg = WCD9XXX_MBHC_CAL_BTN_DET_PTR(tomtom_cal);
	btn_low = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_V_BTN_LOW);
	btn_high = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg,
					       MBHC_BTN_DET_V_BTN_HIGH);
	btn_low[0] = -50;
	btn_high[0] = 20;
	btn_low[1] = 21;
	btn_high[1] = 61;
	btn_low[2] = 62;
	btn_high[2] = 104;
	btn_low[3] = 105;
	btn_high[3] = 148;
	btn_low[4] = 149;
	btn_high[4] = 189;
	btn_low[5] = 190;
	btn_high[5] = 228;
	btn_low[6] = 229;
	btn_high[6] = 269;
	btn_low[7] = 270;
	btn_high[7] = 500;
	n_ready = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_N_READY);
	n_ready[0] = 80;
	n_ready[1] = 68;
	n_cic = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_N_CIC);
	n_cic[0] = 60;
	n_cic[1] = 47;
	gain = wcd9xxx_mbhc_cal_btn_det_mp(btn_cfg, MBHC_BTN_DET_GAIN);
	gain[0] = 11;
	gain[1] = 9;

	return tomtom_cal;
}

/* Digital audio interface connects codec <---> CPU */
static struct snd_soc_dai_link mdm_dai[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MDM Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name = "MultiMedia1",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* This dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name = "VoIP",
		.platform_name  = "msm-voip-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* This dainlink has VOIP support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_VOIP,
	},
	{
		.name = "Circuit-Switch Voice",
		.stream_name = "CS-Voice",
		.cpu_dai_name   = "CS-VOICE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* This dainlink has Voice support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_CS_VOICE,
	},
	{
		.name = "Primary MI2S RX Hostless",
		.stream_name = "Primary MI2S_RX Hostless Playback",
		.cpu_dai_name = "PRI_MI2S_RX_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "VoLTE",
		.stream_name = "VoLTE",
		.cpu_dai_name   = "VoLTE",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_VOLTE,
	},
	{	.name = "MSM AFE-PCM RX",
		.stream_name = "AFE-PROXY RX",
		.cpu_dai_name = "msm-dai-q6-dev.241",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
	},
	{
		.name = "MSM AFE-PCM TX",
		.stream_name = "AFE-PROXY TX",
		.cpu_dai_name = "msm-dai-q6-dev.240",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.platform_name  = "msm-pcm-afe",
		.ignore_suspend = 1,
	},
	{
		.name = "DTMF RX Hostless",
		.stream_name = "DTMF RX Hostless",
		.cpu_dai_name	= "DTMF_RX_HOSTLESS",
		.platform_name	= "msm-pcm-dtmf",
		.dynamic = 1,
		.dpcm_playback = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_DTMF_RX,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
	},
	{
		.name = "DTMF TX",
		.stream_name = "DTMF TX",
		.cpu_dai_name = "msm-dai-stub-dev.4",
		.platform_name = "msm-pcm-dtmf",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.ignore_suspend = 1,
	},
	{
		.name = "CS-VOICE HOST RX CAPTURE",
		.stream_name = "CS-VOICE HOST RX CAPTURE",
		.cpu_dai_name = "msm-dai-stub-dev.5",
		.platform_name  = "msm-voice-host-pcm",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
	},
	{
		.name = "CS-VOICE HOST RX PLAYBACK",
		.stream_name = "CS-VOICE HOST RX PLAYBACK",
		.cpu_dai_name = "msm-dai-stub-dev.6",
		.platform_name  = "msm-voice-host-pcm",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = "CS-VOICE HOST TX CAPTURE",
		.stream_name = "CS-VOICE HOST TX CAPTURE",
		.cpu_dai_name = "msm-dai-stub-dev.7",
		.platform_name  = "msm-voice-host-pcm",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
	},
	{
		.name = "CS-VOICE HOST TX PLAYBACK",
		.stream_name = "CS-VOICE HOST TX PLAYBACK",
		.cpu_dai_name = "msm-dai-stub-dev.8",
		.platform_name  = "msm-voice-host-pcm",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.ignore_suspend = 1,
		 .ignore_pmdown_time = 1,
	},
	{
		.name = "MDM Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name   = "MultiMedia2",
		.platform_name  = "msm-pcm-dsp.0",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		/* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{
		.name = "MDM Media6",
		.stream_name = "MultiMedia6",
		.cpu_dai_name   = "MultiMedia6",
		.platform_name  = "msm-pcm-loopback",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		/* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA6,
	},
	{
		.name = "Primary MI2S TX Hostless",
		.stream_name = "Primary MI2S_TX Hostless Playback",
		.cpu_dai_name = "PRI_MI2S_TX_HOSTLESS",
		.platform_name  = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "MDM LowLatency",
		.stream_name = "MultiMedia5",
		.cpu_dai_name   = "MultiMedia5",
		.platform_name  = "msm-pcm-dsp.1",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA5,
	},
	{
		.name = "MDM VoiceMMode1",
		.stream_name = "VoiceMMode1",
		.cpu_dai_name   = "VoiceMMode1",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* This dainlink has Voice support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_VOICEMMODE1,
	},
	{
		.name = "MDM VoiceMMode2",
		.stream_name = "VoiceMMode2",
		.cpu_dai_name   = "VoiceMMode2",
		.platform_name  = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* This dainlink has Voice support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_VOICEMMODE2,
	},
	{
		.name = "VoiceMMode1 HOST RX CAPTURE",
		.stream_name = "VoiceMMode1 HOST RX CAPTURE",
		.cpu_dai_name = "msm-dai-stub-dev.5",
		.platform_name  = "msm-voice-host-pcm",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
	},
	{
		.name = "VoiceMMode1 HOST RX PLAYBACK",
		.stream_name = "VoiceMMode1 HOST RX PLAYBACK",
		.cpu_dai_name = "msm-dai-stub-dev.6",
		.platform_name  = "msm-voice-host-pcm",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = "VoiceMMode1 HOST TX CAPTURE",
		.stream_name = "VoiceMMode1 HOST TX CAPTURE",
		.cpu_dai_name = "msm-dai-stub-dev.7",
		.platform_name  = "msm-voice-host-pcm",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
	},
	{
		.name = "VoiceMMode1 HOST TX PLAYBACK",
		.stream_name = "VoiceMMode1 HOST TX PLAYBACK",
		.cpu_dai_name = "msm-dai-stub-dev.8",
		.platform_name  = "msm-voice-host-pcm",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.ignore_suspend = 1,
		 .ignore_pmdown_time = 1,
	},
	{
		.name = "VoiceMMode2 HOST RX CAPTURE",
		.stream_name = "VoiceMMode2 HOST RX CAPTURE",
		.cpu_dai_name = "msm-dai-stub-dev.5",
		.platform_name  = "msm-voice-host-pcm",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
	},
	{
		.name = "VoiceMMode2 HOST RX PLAYBACK",
		.stream_name = "VOiceMMode2 HOST RX PLAYBACK",
		.cpu_dai_name = "msm-dai-stub-dev.6",
		.platform_name  = "msm-voice-host-pcm",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = "VoiceMMode2 HOST TX CAPTURE",
		.stream_name = "VoiceMMode2 HOST TX CAPTURE",
		.cpu_dai_name = "msm-dai-stub-dev.7",
		.platform_name  = "msm-voice-host-pcm",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
	},
	{
		.name = "VoiceMMode2 HOST TX PLAYBACK",
		.stream_name = "VOiceMMode2 HOST TX PLAYBACK",
		.cpu_dai_name = "msm-dai-stub-dev.8",
		.platform_name  = "msm-voice-host-pcm",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.ignore_suspend = 1,
		 .ignore_pmdown_time = 1,
	},
	{
		.name = "MDM Compress1",
		.stream_name = "MultiMedia4",
		.cpu_dai_name	= "MultiMedia4",
		.platform_name  = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		/* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA4,
	},
	{
		.name = "QCHAT",
		.stream_name = "QCHAT",
		.cpu_dai_name = "QCHAT",
		.platform_name = "msm-pcm-voice",
		.dynamic = 1,
		.dpcm_capture = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_QCHAT,
	},
	/* Backend DAI Links */
	{
		.name = LPASS_BE_AFE_PCM_RX,
		.stream_name = "AFE Playback",
		.cpu_dai_name = "msm-dai-q6-dev.224",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_RX,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_AFE_PCM_TX,
		.stream_name = "AFE Capture",
		.cpu_dai_name = "msm-dai-q6-dev.225",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_AFE_PCM_TX,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_AUXPCM_RX,
		.stream_name = "AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_RX,
		.be_hw_params_fixup = mdm_auxpcm_be_params_fixup,
		.ops = &mdm_auxpcm_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_AUXPCM_TX,
		.stream_name = "AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_TX,
		.be_hw_params_fixup = mdm_auxpcm_be_params_fixup,
		.ops = &mdm_auxpcm_be_ops,
		.ignore_suspend = 1,
	},
	/* Incall Record Uplink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_TX,
		.stream_name = "Voice Uplink Capture",
		.cpu_dai_name = "msm-dai-q6-dev.32772",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_TX,
		.be_hw_params_fixup = mdm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Record Downlink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_RX,
		.stream_name = "Voice Downlink Capture",
		.cpu_dai_name = "msm-dai-q6-dev.32771",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_RX,
		.be_hw_params_fixup = mdm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Music BACK END DAI Link */
	{
		.name = LPASS_BE_VOICE_PLAYBACK_TX,
		.stream_name = "Voice Farend Playback",
		.cpu_dai_name = "msm-dai-q6-dev.32773",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
		.be_hw_params_fixup = mdm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_MI2S_RX,
		.stream_name = "Secondary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
		.be_hw_params_fixup = &mdm_sec_mi2s_rx_be_hw_params_fixup,
		.ops = &mdm_sec_mi2s_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_MI2S_TX,
		.stream_name = "Secondary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SECONDARY_MI2S_TX,
		.be_hw_params_fixup = &mdm_sec_mi2s_tx_be_hw_params_fixup,
		.ops = &mdm_sec_mi2s_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_AUXPCM_RX,
		.stream_name = "Sec AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SEC_AUXPCM_RX,
		.be_hw_params_fixup = mdm_auxpcm_be_params_fixup,
		.ops = &mdm_sec_auxpcm_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_AUXPCM_TX,
		.stream_name = "Sec AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SEC_AUXPCM_TX,
		.be_hw_params_fixup = mdm_auxpcm_be_params_fixup,
		.ops = &mdm_sec_auxpcm_be_ops,
		.ignore_suspend = 1,
	},
	/* FE TDM DAI links */
	{
		.name = "Primary TDM RX 0 Hostless",
		.stream_name = "Primary TDM RX 0 Hostless",
		.cpu_dai_name = "PRI_TDM_RX_0_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Primary TDM TX 0 Hostless",
		.stream_name = "Primary TDM TX 0 Hostless",
		.cpu_dai_name = "PRI_TDM_TX_0_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Secondary TDM RX 0 Hostless",
		.stream_name = "Secondary TDM RX 0 Hostless",
		.cpu_dai_name = "SEC_TDM_RX_0_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Secondary TDM TX 0 Hostless",
		.stream_name = "Secondary TDM TX 0 Hostless",
		.cpu_dai_name = "SEC_TDM_TX_0_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
};

static struct snd_soc_dai_link mdm_9330_dai[] = {
		/* Backend DAI Links */
	{
		.name = LPASS_BE_PRI_MI2S_RX,
		.stream_name = "Primary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tomtom_codec",
		.codec_dai_name = "tomtom_i2s_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_RX,
		.init  = &mdm_mi2s_audrx_init,
		.be_hw_params_fixup = &mdm_mi2s_rx_be_hw_params_fixup,
		.ops = &mdm_mi2s_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_MI2S_TX,
		.stream_name = "Primary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tomtom_codec",
		.codec_dai_name = "tomtom_i2s_tx1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_TX,
		.be_hw_params_fixup = &mdm_mi2s_tx_be_hw_params_fixup,
		.ops = &mdm_mi2s_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	/* TDM be dai links */
	{
		.name = LPASS_BE_PRI_TDM_RX_0,
		.stream_name = "Primary TDM0 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36864",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_RX_0,
		.be_hw_params_fixup = mdm_tdm_be_hw_params_fixup,
		.ops = &mdm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_TX_0,
		.stream_name = "Primary TDM0 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36865",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_TX_0,
		.be_hw_params_fixup = mdm_tdm_be_hw_params_fixup,
		.ops = &mdm_tdm_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_SEC_TDM_RX_0,
		.stream_name = "Secondary TDM0 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36880",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SEC_TDM_RX_0,
		.be_hw_params_fixup = mdm_tdm_be_hw_params_fixup,
		.ops = &mdm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_TDM_TX_0,
		.stream_name = "Secondary TDM0 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36881",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SEC_TDM_TX_0,
		.be_hw_params_fixup = mdm_tdm_be_hw_params_fixup,
		.ops = &mdm_tdm_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
};

static struct snd_soc_dai_link mdm_9306_dai[] = {
		/* Backend DAI Links */
	{
		.name = LPASS_BE_PRI_MI2S_RX,
		.stream_name = "Primary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tapan_codec",
		.codec_dai_name = "tapan_i2s_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_RX,
		.init  = &mdm_mi2s_audrx_init,
		.be_hw_params_fixup = &mdm_mi2s_rx_be_hw_params_fixup,
		.ops = &mdm_mi2s_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_MI2S_TX,
		.stream_name = "Primary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tapan_codec",
		.codec_dai_name = "tapan_i2s_tx1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_TX,
		.be_hw_params_fixup = &mdm_mi2s_tx_be_hw_params_fixup,
		.ops = &mdm_mi2s_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link mdm_tomtom_dai_links[
				ARRAY_SIZE(mdm_dai) +
				ARRAY_SIZE(mdm_9330_dai)];

static struct snd_soc_dai_link mdm_tapan_dai_links[
				ARRAY_SIZE(mdm_dai) +
				ARRAY_SIZE(mdm_9306_dai)];

static struct snd_soc_card snd_soc_card_mdm_9330 = {
	.name = "mdm9607-tomtom-i2s-snd-card",
	.dai_link = mdm_tomtom_dai_links,
	.num_links = ARRAY_SIZE(mdm_tomtom_dai_links),
};

static struct snd_soc_card snd_soc_card_mdm_9306 = {
	.name = "mdm9607-tapan-i2s-snd-card",
	.dai_link = mdm_tapan_dai_links,
	.num_links = ARRAY_SIZE(mdm_tapan_dai_links),
};

static const struct of_device_id mdm_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,mdm9607-audio-tomtom",
	  .data = "tomtom_codec"},
	{ .compatible = "qcom,mdm9607-audio-tapan",
	  .data = "tapan_codec"},
	{},
};

static int mdm_populate_dai_link_component_of_node(
					struct snd_soc_card *card)
{
	int i, index, ret = 0;
	struct device *cdev = card->dev;
	struct snd_soc_dai_link *dai_link = card->dai_link;
	struct device_node *np;

	if (!cdev) {
		pr_err("%s: Sound card device memory NULL\n", __func__);
		return -ENODEV;
	}

	for (i = 0; i < card->num_links; i++) {
		if (dai_link[i].platform_of_node && dai_link[i].cpu_of_node)
			continue;

		/* populate platform_of_node for snd card dai links */
		if (dai_link[i].platform_name &&
		    !dai_link[i].platform_of_node) {
			index = of_property_match_string(cdev->of_node,
						"asoc-platform-names",
						dai_link[i].platform_name);
			if (index < 0) {
				pr_debug("%s: No match found for platform name: %s\n",
					__func__, dai_link[i].platform_name);
				ret = index;
				goto err;
			}
			np = of_parse_phandle(cdev->of_node, "asoc-platform",
					      index);
			if (!np) {
				pr_err("%s: retrieving phandle for platform %s, index %d failed\n",
					__func__, dai_link[i].platform_name,
					index);
				ret = -ENODEV;
				goto err;
			}
			dai_link[i].platform_of_node = np;
			dai_link[i].platform_name = NULL;
		}

		/* populate cpu_of_node for snd card dai links */
		if (dai_link[i].cpu_dai_name && !dai_link[i].cpu_of_node) {
			index = of_property_match_string(cdev->of_node,
						 "asoc-cpu-names",
						 dai_link[i].cpu_dai_name);
			if (index >= 0) {
				np = of_parse_phandle(cdev->of_node, "asoc-cpu",
						index);
				if (!np) {
					pr_err("%s: retrieving phandle for cpu dai %s failed\n",
						__func__,
						dai_link[i].cpu_dai_name);
					ret = -ENODEV;
					goto err;
				}
				dai_link[i].cpu_of_node = np;
				dai_link[i].cpu_dai_name = NULL;
			}
		}

		/* populate codec_of_node for snd card dai links */
		if (dai_link[i].codec_name && !dai_link[i].codec_of_node) {
			index = of_property_match_string(cdev->of_node,
						 "asoc-codec-names",
						 dai_link[i].codec_name);
			if (index < 0)
				continue;
			np = of_parse_phandle(cdev->of_node, "asoc-codec",
					      index);
			if (!np) {
				pr_err("%s: retrieving phandle for codec %s failed\n",
					__func__, dai_link[i].codec_name);
				ret = -ENODEV;
				goto err;
			}
			dai_link[i].codec_of_node = np;
			dai_link[i].codec_name = NULL;
		}
	}

err:
	return ret;
}

static struct snd_soc_card *populate_snd_card_dailinks(struct device *dev)
{
	struct snd_soc_card *card = NULL;
	struct snd_soc_dai_link *dailink;
	const struct of_device_id *match;
	int len_1, len_2;

	match = of_match_node(mdm_asoc_machine_of_match, dev->of_node);
	if (!match) {
		dev_err(dev, "%s: No DT match found for sound card\n",
				__func__);
		return NULL;
	}

	if (!strcmp(match->data, "tomtom_codec")) {
		card = &snd_soc_card_mdm_9330;
		len_1 = ARRAY_SIZE(mdm_dai);
		len_2 = len_1 + ARRAY_SIZE(mdm_9330_dai);

		memcpy(mdm_tomtom_dai_links, mdm_dai,
			   sizeof(mdm_dai));
		memcpy(mdm_tomtom_dai_links + len_1, mdm_9330_dai,
			   sizeof(mdm_9330_dai));
		dailink = mdm_tomtom_dai_links;

	} else if (!strcmp(match->data, "tapan_codec")) {
		card = &snd_soc_card_mdm_9306;
		len_1 = ARRAY_SIZE(mdm_dai);
		len_2 = len_1 + ARRAY_SIZE(mdm_9306_dai);

		memcpy(mdm_tapan_dai_links, mdm_dai,
			   sizeof(mdm_dai));
		memcpy(mdm_tapan_dai_links + len_1, mdm_9306_dai,
			   sizeof(mdm_9306_dai));
		dailink = mdm_tapan_dai_links;
	}

	if (card) {
		card->dai_link = dailink;
		card->num_links = len_2;
	}

	return card;
}

static int mdm_asoc_machine_probe(struct platform_device *pdev)
{
	int ret;
	struct snd_soc_card *card;
	struct mdm_machine_data *pdata;
	enum apr_subsys_state q6_state;

	q6_state = apr_get_subsys_state();
	/*
	* mclk is needed during init for mbhc calibration,
	* so wait for modem to get loaded and be ready
	* to accept mclk request command.
	*/
	if (q6_state == APR_SUBSYS_DOWN) {
		dev_err(&pdev->dev, "Defering %s, q6_state %d\n",
					__func__, q6_state);
		return -EPROBE_DEFER;
	}

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev,
			"%s No platform supplied from device tree\n", __func__);

		return -EINVAL;
	}
	pdata = devm_kzalloc(&pdev->dev, sizeof(struct mdm_machine_data),
			     GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = of_property_read_u32(pdev->dev.of_node,
				   "qcom,codec-mclk-clk-freq",
				   &pdata->mclk_freq);
	if (ret) {
		dev_err(&pdev->dev,
			"%s Looking up %s property in node %s failed",
			__func__, "qcom,codec-mclk-clk-freq",
			pdev->dev.of_node->full_name);

		goto err;
	}

	/* At present only 12.288MHz is supported on MDM. */
	if (q6afe_check_osr_clk_freq(pdata->mclk_freq)) {
		dev_err(&pdev->dev, "%s Unsupported tomtom mclk freq %u\n",
			__func__, pdata->mclk_freq);

		ret = -EINVAL;
		goto err;
	}

	pdata->prim_master_p = of_parse_phandle(pdev->dev.of_node,
						"qcom,prim_mi2s_aux_master", 0);
	pdata->prim_slave_p = of_parse_phandle(pdev->dev.of_node,
						"qcom,prim_mi2s_aux_slave", 0);
	pdata->sec_master_p = of_parse_phandle(pdev->dev.of_node,
						"qcom,sec_mi2s_aux_master", 0);
	pdata->sec_slave_p = of_parse_phandle(pdev->dev.of_node,
						"qcom,sec_mi2s_aux_slave", 0);

	mutex_init(&cdc_mclk_mutex);
	atomic_set(&aux_ref_count, 0);
	atomic_set(&sec_aux_ref_count, 0);
	atomic_set(&mi2s_ref_count, 0);
	atomic_set(&sec_mi2s_ref_count, 0);
	atomic_set(&sec_tdm_ref_count, 0);
	atomic_set(&pdata->prim_clk_usrs, 0);
	atomic_set(&pdata->sec_clk_usrs, 0);

	card = populate_snd_card_dailinks(&pdev->dev);
	if (!card) {
		dev_err(&pdev->dev, "%s: Card uninitialized\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	if (!strcmp(card->name, "mdm9607-tapan-i2s-snd-card")) {
		pdata->mdm9607_codec_fn.mclk_enable_fn = tapan_mclk_enable;
		pdata->mdm9607_codec_fn.mbhc_hs_detect = tapan_hs_detect;
	} else if (!strcmp(card->name, "mdm9607-tomtom-i2s-snd-card")) {
		pdata->mdm9607_codec_fn.mclk_enable_fn = tomtom_mclk_enable;
		pdata->mdm9607_codec_fn.mbhc_hs_detect = tomtom_hs_detect;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);

	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret)
		goto err;
	ret = snd_soc_of_parse_audio_routing(card, "qcom,audio-routing");
	if (ret)
		goto err;
	ret = mdm_populate_dai_link_component_of_node(card);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}

	pdata->lpaif_pri_muxsel_virt_addr = ioremap(LPAIF_PRI_MODE_MUXSEL, 4);
	if (pdata->lpaif_pri_muxsel_virt_addr == NULL) {
		pr_err("%s Pri muxsel virt addr is null\n", __func__);

		ret = -EINVAL;
		goto err2;
	}
	pdata->lpass_mux_mic_ctl_virt_addr =
			ioremap(LPASS_CSR_GP_IO_MUX_MIC_CTL, 4);
	if (pdata->lpass_mux_mic_ctl_virt_addr == NULL) {
		pr_err("%s lpass_mux_mic_ctl_virt_addr is null\n",
			__func__);
		ret = -EINVAL;
		goto err3;
	}
	pdata->lpass_mux_spkr_ctl_virt_addr =
				ioremap(LPASS_CSR_GP_IO_MUX_SPKR_CTL, 4);
	if (pdata->lpass_mux_spkr_ctl_virt_addr == NULL) {
		pr_err("%s lpass spkr ctl virt addr is null\n", __func__);

		ret = -EINVAL;
		goto err4;
	}
	pdata->lpaif_sec_muxsel_virt_addr = ioremap(LPAIF_SEC_MODE_MUXSEL, 4);
	if (pdata->lpaif_sec_muxsel_virt_addr == NULL) {
		pr_err("%s Pri muxsel virt addr is null\n", __func__);

		ret = -EINVAL;
		goto err5;
	}

	pdata->gcc_debug_clk_ctl_virt_addr = ioremap(GCC_GCC_DEBUG_CLK_CTL, 4);
	if (pdata->gcc_debug_clk_ctl_virt_addr == NULL) {
		pr_err("%s gcc_debug_clk_ctl_virt_addr is null\n",
			__func__);

		ret = -EINVAL;
		goto err6;
	}

	pdata->gcc_plltest_pad_cfg_virt_addr =
				ioremap(GCC_PLLTEST_PAD_CFG, 4);
	if (pdata->gcc_plltest_pad_cfg_virt_addr == NULL) {
		pr_err("%s gcc_plltest_pad_cfg_virt_addr is null\n", __func__);

		ret = -EINVAL;
		goto err7;
	}

	ret = snd_soc_register_card(card);
	if (ret == -EPROBE_DEFER) {
		goto err7;
	} else if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		goto err7;
	}

	return 0;
err7:
	iounmap(pdata->gcc_plltest_pad_cfg_virt_addr);
err6:
	iounmap(pdata->gcc_debug_clk_ctl_virt_addr);
err5:
	iounmap(pdata->lpaif_sec_muxsel_virt_addr);
err4:
	iounmap(pdata->lpass_mux_spkr_ctl_virt_addr);
err3:
	iounmap(pdata->lpass_mux_mic_ctl_virt_addr);
err2:
	iounmap(pdata->lpaif_pri_muxsel_virt_addr);
err:
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int mdm_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct mdm_machine_data *pdata = snd_soc_card_get_drvdata(card);

	pdata->mclk_freq = 0;
	iounmap(pdata->lpaif_pri_muxsel_virt_addr);
	iounmap(pdata->lpass_mux_spkr_ctl_virt_addr);
	iounmap(pdata->lpaif_sec_muxsel_virt_addr);
	iounmap(pdata->lpass_mux_mic_ctl_virt_addr);
	iounmap(pdata->gcc_debug_clk_ctl_virt_addr);
	iounmap(pdata->gcc_plltest_pad_cfg_virt_addr);
	snd_soc_unregister_card(card);

	return 0;
}

static struct platform_driver mdm_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = mdm_asoc_machine_of_match,
	},
	.probe = mdm_asoc_machine_probe,
	.remove = mdm_asoc_machine_remove,
};


module_platform_driver(mdm_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" MDM_MACHINE_DRV_NAME);
MODULE_DEVICE_TABLE(of, mdm_asoc_machine_of_match);
