/*
 * Copyright (c) 2014-2017, 2019, The Linux Foundation. All rights reserved.
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
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <sound/q6core.h>
#include <sound/pcm_params.h>
#include <sound/info.h>
#include <device_event.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "../codecs/wcd9xxx-common.h"
#include "../codecs/wcd9330.h"
#include "../codecs/wcd9335.h"
#include "../codecs/wsa881x.h"

#define DRV_NAME "msm8996-asoc-snd"

#define SAMPLING_RATE_8KHZ      8000
#define SAMPLING_RATE_11P025KHZ 11025
#define SAMPLING_RATE_16KHZ     16000
#define SAMPLING_RATE_22P05KHZ  22050
#define SAMPLING_RATE_32KHZ     32000
#define SAMPLING_RATE_44P1KHZ   44100
#define SAMPLING_RATE_48KHZ     48000
#define SAMPLING_RATE_88P2KHZ   88200
#define SAMPLING_RATE_96KHZ     96000
#define SAMPLING_RATE_176P4KHZ  176400
#define SAMPLING_RATE_192KHZ    192000

#define MSM8996_SPK_ON     1
#define MSM8996_HIFI_ON    1

#define WCD9XXX_MBHC_DEF_BUTTONS    8
#define WCD9XXX_MBHC_DEF_RLOADS     5
#define CODEC_EXT_CLK_RATE         9600000
#define ADSP_STATE_READY_TIMEOUT_MS    3000
#define DEV_NAME_STR_LEN            32

#define WSA8810_NAME_1 "wsa881x.20170211"
#define WSA8810_NAME_2 "wsa881x.20170212"

static int slim0_rx_sample_rate = SAMPLING_RATE_48KHZ;
static int slim0_tx_sample_rate = SAMPLING_RATE_48KHZ;
static int slim1_tx_sample_rate = SAMPLING_RATE_48KHZ;
static int slim0_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int slim0_tx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int slim1_tx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int hdmi_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm8996_auxpcm_rate = SAMPLING_RATE_8KHZ;
static int slim5_rx_sample_rate = SAMPLING_RATE_48KHZ;
static int slim5_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int slim6_rx_sample_rate = SAMPLING_RATE_48KHZ;
static int slim6_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;

/* TDM default channels */
static int msm_pri_tdm_tx_0_ch = 2;
static int msm_pri_tdm_tx_1_ch = 2;
static int msm_pri_tdm_tx_2_ch = 2;
static int msm_pri_tdm_tx_3_ch = 2;

static int msm_pri_tdm_rx_0_ch = 2;
static int msm_pri_tdm_rx_1_ch = 2;
static int msm_pri_tdm_rx_2_ch = 2;
static int msm_pri_tdm_rx_3_ch = 2;

static int msm_sec_tdm_tx_0_ch = 2;
static int msm_sec_tdm_tx_1_ch = 2;
static int msm_sec_tdm_tx_2_ch = 2;
static int msm_sec_tdm_tx_3_ch = 2;

static int msm_sec_tdm_rx_0_ch = 2;
static int msm_sec_tdm_rx_1_ch = 2;
static int msm_sec_tdm_rx_2_ch = 2;
static int msm_sec_tdm_rx_3_ch = 2;

static int msm_tert_tdm_rx_0_ch = 2;
static int msm_tert_tdm_rx_1_ch = 2;
static int msm_tert_tdm_rx_2_ch = 2;
static int msm_tert_tdm_rx_3_ch = 2;

static int msm_tert_tdm_tx_0_ch = 2;
static int msm_tert_tdm_tx_1_ch = 2;
static int msm_tert_tdm_tx_2_ch = 2;
static int msm_tert_tdm_tx_3_ch = 2;

static int msm_quat_tdm_rx_0_ch = 2;
static int msm_quat_tdm_rx_1_ch = 2;
static int msm_quat_tdm_rx_2_ch = 2;
static int msm_quat_tdm_rx_3_ch = 2;

static int msm_quat_tdm_tx_0_ch = 2;
static int msm_quat_tdm_tx_1_ch = 2;
static int msm_quat_tdm_tx_2_ch = 2;
static int msm_quat_tdm_tx_3_ch = 2;

/* TDM default bit format */
static int msm_pri_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_pri_tdm_tx_1_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_pri_tdm_tx_2_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_pri_tdm_tx_3_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static int msm_pri_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_pri_tdm_rx_1_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_pri_tdm_rx_2_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_pri_tdm_rx_3_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static int msm_sec_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_sec_tdm_tx_1_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_sec_tdm_tx_2_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_sec_tdm_tx_3_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static int msm_sec_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_sec_tdm_rx_1_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_sec_tdm_rx_2_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_sec_tdm_rx_3_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static int msm_tert_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_tert_tdm_rx_1_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_tert_tdm_rx_2_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_tert_tdm_rx_3_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static int msm_tert_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_tert_tdm_tx_1_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_tert_tdm_tx_2_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_tert_tdm_tx_3_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static int msm_quat_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_quat_tdm_rx_1_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_quat_tdm_rx_2_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_quat_tdm_rx_3_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static int msm_quat_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_quat_tdm_tx_1_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_quat_tdm_tx_2_bit_format = SNDRV_PCM_FORMAT_S16_LE;
static int msm_quat_tdm_tx_3_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static int msm_pri_tdm_rate = SAMPLING_RATE_48KHZ;
static int msm_pri_tdm_slot_width = 32;
static int msm_pri_tdm_slot_num = 8;
static int msm_sec_tdm_rate = SAMPLING_RATE_48KHZ;
static int msm_sec_tdm_slot_width = 32;
static int msm_sec_tdm_slot_num = 8;
static int msm_quat_tdm_rate = SAMPLING_RATE_48KHZ;
static int msm_quat_tdm_slot_width = 32;
static int msm_quat_tdm_slot_num = 8;
static int msm_tert_tdm_rate = SAMPLING_RATE_48KHZ;
static int msm_tert_tdm_slot_width = 32;
static int msm_tert_tdm_slot_num = 8;

static int msm_tdm_slot_width = 32;
static int msm_tdm_num_slots = 8;

enum {
	QUATERNARY_TDM_RX_0,
	QUATERNARY_TDM_RX_1,
	QUATERNARY_TDM_RX_2,
	QUATERNARY_TDM_RX_3,
	QUATERNARY_TDM_RX_4,
	QUATERNARY_TDM_RX_5,
	QUATERNARY_TDM_RX_6,
	QUATERNARY_TDM_RX_7,
	QUATERNARY_TDM_TX_0,
	QUATERNARY_TDM_TX_1,
	QUATERNARY_TDM_TX_2,
	QUATERNARY_TDM_TX_3,
	QUATERNARY_TDM_TX_4,
	QUATERNARY_TDM_TX_5,
	QUATERNARY_TDM_TX_6,
	QUATERNARY_TDM_TX_7,
	TERTIARY_TDM_RX_0,
	TERTIARY_TDM_RX_1,
	TERTIARY_TDM_RX_2,
	TERTIARY_TDM_RX_3,
	TERTIARY_TDM_RX_4,
	TERTIARY_TDM_RX_5,
	TERTIARY_TDM_RX_6,
	TERTIARY_TDM_RX_7,
	TERTIARY_TDM_TX_0,
	TERTIARY_TDM_TX_1,
	TERTIARY_TDM_TX_2,
	TERTIARY_TDM_TX_3,
	TERTIARY_TDM_TX_4,
	TERTIARY_TDM_TX_5,
	TERTIARY_TDM_TX_6,
	TERTIARY_TDM_TX_7,
	SECONDARY_TDM_RX_0,
	SECONDARY_TDM_RX_1,
	SECONDARY_TDM_RX_2,
	SECONDARY_TDM_RX_3,
	SECONDARY_TDM_RX_4,
	SECONDARY_TDM_RX_5,
	SECONDARY_TDM_RX_6,
	SECONDARY_TDM_RX_7,
	SECONDARY_TDM_TX_0,
	SECONDARY_TDM_TX_1,
	SECONDARY_TDM_TX_2,
	SECONDARY_TDM_TX_3,
	SECONDARY_TDM_TX_4,
	SECONDARY_TDM_TX_5,
	SECONDARY_TDM_TX_6,
	SECONDARY_TDM_TX_7,
	PRIMARY_TDM_RX_0,
	PRIMARY_TDM_RX_1,
	PRIMARY_TDM_RX_2,
	PRIMARY_TDM_RX_3,
	PRIMARY_TDM_RX_4,
	PRIMARY_TDM_RX_5,
	PRIMARY_TDM_RX_6,
	PRIMARY_TDM_RX_7,
	PRIMARY_TDM_TX_0,
	PRIMARY_TDM_TX_1,
	PRIMARY_TDM_TX_2,
	PRIMARY_TDM_TX_3,
	PRIMARY_TDM_TX_4,
	PRIMARY_TDM_TX_5,
	PRIMARY_TDM_TX_6,
	PRIMARY_TDM_TX_7,
	TDM_MAX,
};

#define TDM_SLOT_OFFSET_MAX    8
/* TDM default offset */
static unsigned int tdm_slot_offset[TDM_MAX][TDM_SLOT_OFFSET_MAX] = {
	/* QUAT_TDM_RX */
	{0, 4, 0xFFFF},
	{8, 12, 0xFFFF},
	{16, 20, 0xFFFF},
	{24, 28, 0xFFFF},
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	/* QUAT_TDM_TX */
	{0, 4, 0xFFFF},
	{8, 12, 0xFFFF},
	{16, 20, 0xFFFF},
	{24, 28, 0xFFFF},
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	/* TERT_TDM_RX */
	{0, 4, 0xFFFF},
	{8, 12, 0xFFFF},
	{16, 20, 0xFFFF},
	{24, 28, 0xFFFF},
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	/* TERT_TDM_TX */
	{0, 4, 0xFFFF},
	{8, 12, 0xFFFF},
	{16, 20, 0xFFFF},
	{24, 28, 0xFFFF},
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	/* SEC_TDM_RX */
	{0, 4, 0xFFFF},
	{8, 12, 0xFFFF},
	{16, 20, 0xFFFF},
	{24, 28, 0xFFFF},
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	/* SEC_TDM_TX */
	{0, 4, 0xFFFF},
	{8, 12, 0xFFFF},
	{16, 20, 0xFFFF},
	{24, 28, 0xFFFF},
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	/* PRI_TDM_RX */
	{0, 4, 0xFFFF},
	{8, 12, 0xFFFF},
	{16, 20, 0xFFFF},
	{24, 28, 0xFFFF},
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	/* PRI_TDM_TX */
	{0, 4, 0xFFFF},
	{8, 12, 0xFFFF},
	{16, 20, 0xFFFF},
	{24, 28, 0xFFFF},
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
	{0xFFFF}, /* not used */
};

static struct platform_device *spdev;
static int ext_us_amp_gpio = -1;
static int msm8996_spk_control = 1;
static int msm_slim_0_rx_ch = 1;
static int msm_slim_0_tx_ch = 1;
static int msm_slim_1_tx_ch = 1;
static int msm_slim_5_rx_ch = 1;
static int msm_slim_6_rx_ch = 1;
static int msm_hifi_control;
static int msm_vi_feed_tx_ch = 2;

static int msm_hdmi_rx_ch = 2;
static int msm_proxy_rx_ch = 2;
static int hdmi_rx_sample_rate = SAMPLING_RATE_48KHZ;
static int msm_tert_mi2s_tx_ch = 2;

static bool codec_reg_done;

static const char *const hifi_function[] = {"Off", "On"};
static const char *const pin_states[] = {"Disable", "active"};
static const char *const spk_function[] = {"Off", "On"};
static const char *const slim0_rx_ch_text[] = {"One", "Two"};
static const char *const slim5_rx_ch_text[] = {"One", "Two"};
static const char *const slim6_rx_ch_text[] = {"One", "Two"};
static const char *const slim0_tx_ch_text[] = {"One", "Two", "Three", "Four",
						"Five", "Six", "Seven",
						"Eight"};
static const char *const vi_feed_ch_text[] = {"One", "Two"};
static char const *hdmi_rx_ch_text[] = {"Two", "Three", "Four", "Five",
					"Six", "Seven", "Eight"};
static char const *rx_bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE"};
static char const *usb_bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE"};
static char const *slim5_rx_bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE"};
static char const *slim6_rx_bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE"};
static char const *slim0_rx_sample_rate_text[] = {"KHZ_48", "KHZ_96",
					"KHZ_192", "KHZ_44P1", "KHZ_8",
					"KHZ_16", "KHZ_32"};
static char const *slim5_rx_sample_rate_text[] = {"KHZ_48", "KHZ_96",
						  "KHZ_192", "KHZ_44P1"};
static char const *slim6_rx_sample_rate_text[] = {"KHZ_48", "KHZ_96",
						  "KHZ_192", "KHZ_44P1"};
static const char *const proxy_rx_ch_text[] = {"One", "Two", "Three", "Four",
	"Five", "Six", "Seven", "Eight"};

static char const *hdmi_rx_sample_rate_text[] = {"KHZ_48", "KHZ_96",
					"KHZ_192"};
static char const *tdm_ch_text[] = {"One", "Two", "Three", "Four",
		"Five", "Six", "Seven", "Eight"};

static char const *tdm_bit_format_text[] = {"S16_LE", "S24_LE"};
static const char *const tdm_rate_text[] = {"8000", "16000", "48000"};

static const char *const tdm_slot_num_text[] = {"One", "Two", "Four",
		"Eight", "Sixteen", "Thirtytwo"};


static const char *const tdm_slot_width_text[] = {"16", "24", "32"};

static const char *const usb_ch_text[] = {"One", "Two", "Three", "Four",
					"Five", "Six", "Seven", "Eight"};
static char const *usb_sample_rate_text[] = {"KHZ_8", "KHZ_11P025",
					"KHZ_16", "KHZ_22P05", "KHZ_32",
					"KHZ_44P1", "KHZ_48", "KHZ_88P2",
					"KHZ_96", "KHZ_176P4", "KHZ_192"};

static SOC_ENUM_SINGLE_EXT_DECL(usb_rx_chs, usb_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_tx_chs, usb_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_rx_format, usb_bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_tx_format, usb_bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_rx_sample_rate, usb_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_tx_sample_rate, usb_sample_rate_text);

static const char *const auxpcm_rate_text[] = {"8000", "16000"};
static const struct soc_enum msm8996_auxpcm_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, auxpcm_rate_text),
};

struct usb_be_config {
	u32 sample_rate;
	u32 bit_format;
	u32 channels;
};

static struct usb_be_config usb_rx_cfg = {
	.sample_rate = SAMPLING_RATE_48KHZ,
	.bit_format = SNDRV_PCM_FORMAT_S16_LE,
	.channels = 2,
};

static struct usb_be_config usb_tx_cfg = {
	.sample_rate = SAMPLING_RATE_48KHZ,
	.bit_format = SNDRV_PCM_FORMAT_S16_LE,
	.channels = 2,
};

static struct afe_clk_set mi2s_tx_clk = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_CLK_ID_TER_MI2S_IBIT,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

struct msm8996_wsa881x_dev_info {
	struct device_node *of_node;
	u32 index;
};

static struct snd_soc_aux_dev *msm8996_aux_dev;
static struct snd_soc_codec_conf *msm8996_codec_conf;

struct msm8996_asoc_mach_data {
	u32 mclk_freq;
	int us_euro_gpio;
	int hph_en1_gpio;
	int hph_en0_gpio;
	struct snd_info_entry *codec_root;
};

struct msm8996_asoc_wcd93xx_codec {
	void* (*get_afe_config_fn)(struct snd_soc_codec *codec,
				   enum afe_config_type config_type);
	void (*mbhc_hs_detect_exit)(struct snd_soc_codec *codec);
};

static struct msm8996_asoc_wcd93xx_codec msm8996_codec_fn;

struct msm8996_liquid_dock_dev {
	int dock_plug_gpio;
	int dock_plug_irq;
	int dock_plug_det;
	struct work_struct irq_work;
	struct switch_dev audio_sdev;
};
static struct msm8996_liquid_dock_dev *msm8996_liquid_dock_dev;

static void *adsp_state_notifier;
static void *def_tasha_mbhc_cal(void);
static int msm_snd_enable_codec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm);
static int msm8996_wsa881x_init(struct snd_soc_component *component);

/*
 * Need to report LINEIN
 * if R/L channel impedance is larger than 5K ohm
 */
static struct wcd_mbhc_config wcd_mbhc_cfg = {
	.read_fw_bin = false,
	.calibration = NULL,
	.detect_extn_cable = true,
	.mono_stero_detection = false,
	.swap_gnd_mic = NULL,
	.hs_ext_micbias = true,
	.key_code[0] = KEY_MEDIA,
	.key_code[1] = KEY_VOICECOMMAND,
	.key_code[2] = KEY_VOLUMEUP,
	.key_code[3] = KEY_VOLUMEDOWN,
	.key_code[4] = 0,
	.key_code[5] = 0,
	.key_code[6] = 0,
	.key_code[7] = 0,
	.linein_th = 5000,
	.moisture_en = true,
	.mbhc_micbias = MIC_BIAS_2,
	.anc_micbias = MIC_BIAS_2,
	.enable_anc_mic_detect = false,
};

static inline int param_is_mask(int p)
{
	return (p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
			(p <= SNDRV_PCM_HW_PARAM_LAST_MASK);
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p,
					     int n)
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

static void msm8996_liquid_docking_irq_work(struct work_struct *work)
{
	struct msm8996_liquid_dock_dev *dock_dev =
		container_of(work, struct msm8996_liquid_dock_dev,
			     irq_work);

	dock_dev->dock_plug_det =
		gpio_get_value(dock_dev->dock_plug_gpio);

	switch_set_state(&dock_dev->audio_sdev, dock_dev->dock_plug_det);
	/* notify to audio deamon */
	sysfs_notify(&dock_dev->audio_sdev.dev->kobj, NULL, "state");
}

static irqreturn_t msm8996_liquid_docking_irq_handler(int irq, void *dev)
{
	struct msm8996_liquid_dock_dev *dock_dev = dev;

	/* switch speakers should not run in interrupt context */
	schedule_work(&dock_dev->irq_work);
	return IRQ_HANDLED;
}

static int msm8996_liquid_init_docking(void)
{
	int ret = 0;
	int dock_plug_gpio = 0;

	/* plug in docking speaker+plug in device OR unplug one of them */
	u32 dock_plug_irq_flags = IRQF_TRIGGER_RISING |
				  IRQF_TRIGGER_FALLING |
				  IRQF_SHARED;

	dock_plug_gpio = of_get_named_gpio(spdev->dev.of_node,
					   "qcom,dock-plug-det-irq", 0);

	if (dock_plug_gpio >= 0) {
		msm8996_liquid_dock_dev =
		 kzalloc(sizeof(*msm8996_liquid_dock_dev), GFP_KERNEL);
		if (!msm8996_liquid_dock_dev) {
			pr_err("msm8996_liquid_dock_dev alloc fail.\n");
			ret = -ENOMEM;
			goto exit;
		}

		msm8996_liquid_dock_dev->dock_plug_gpio = dock_plug_gpio;

		ret = gpio_request(msm8996_liquid_dock_dev->dock_plug_gpio,
					   "dock-plug-det-irq");
		if (ret) {
			pr_err("%s:failed request msm8996_liquid_dock_plug_gpio err = %d\n",
				__func__, ret);
			ret = -EINVAL;
			goto fail_dock_gpio;
		}

		msm8996_liquid_dock_dev->dock_plug_det =
			gpio_get_value(
				msm8996_liquid_dock_dev->dock_plug_gpio);
		msm8996_liquid_dock_dev->dock_plug_irq =
			gpio_to_irq(
				msm8996_liquid_dock_dev->dock_plug_gpio);

		ret = request_irq(msm8996_liquid_dock_dev->dock_plug_irq,
				  msm8996_liquid_docking_irq_handler,
				  dock_plug_irq_flags,
				  "liquid_dock_plug_irq",
				  msm8996_liquid_dock_dev);
		if (ret < 0) {
			pr_err("%s: Request Irq Failed err = %d\n",
				__func__, ret);
			goto fail_dock_gpio;
		}

		msm8996_liquid_dock_dev->audio_sdev.name =
						QC_AUDIO_EXTERNAL_SPK_1_EVENT;

		if (switch_dev_register(
			 &msm8996_liquid_dock_dev->audio_sdev) < 0) {
			pr_err("%s: dock device register in switch diretory failed\n",
				__func__);
			goto fail_switch_dev;
		}

		INIT_WORK(
			&msm8996_liquid_dock_dev->irq_work,
			msm8996_liquid_docking_irq_work);
	}
	return 0;

fail_switch_dev:
	free_irq(msm8996_liquid_dock_dev->dock_plug_irq,
				msm8996_liquid_dock_dev);
fail_dock_gpio:
	gpio_free(msm8996_liquid_dock_dev->dock_plug_gpio);
exit:
	kfree(msm8996_liquid_dock_dev);
	msm8996_liquid_dock_dev = NULL;
	return ret;
}

static void msm8996_ext_control(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	pr_debug("%s: msm8996_spk_control = %d", __func__,
		 msm8996_spk_control);
	if (msm8996_spk_control == MSM8996_SPK_ON) {
		snd_soc_dapm_enable_pin(dapm, "Lineout_1 amp");
		snd_soc_dapm_enable_pin(dapm, "Lineout_2 amp");
	} else {
		snd_soc_dapm_disable_pin(dapm, "Lineout_1 amp");
		snd_soc_dapm_disable_pin(dapm, "Lineout_2 amp");
	}
	snd_soc_dapm_sync(dapm);
}

static int msm8996_get_spk(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm8996_spk_control = %d\n",
			 __func__, msm8996_spk_control);
	ucontrol->value.integer.value[0] = msm8996_spk_control;
	return 0;
}

static int msm8996_set_spk(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	pr_debug("%s() ucontrol->value.integer.value[0] = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);
	if (msm8996_spk_control == ucontrol->value.integer.value[0])
		return 0;

	msm8996_spk_control = ucontrol->value.integer.value[0];
	msm8996_ext_control(codec);
	return 1;
}

static int msm8996_hifi_ctrl(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	struct snd_soc_card *card = codec->component.card;
	struct msm8996_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);

	pr_debug("%s: msm_hifi_control = %d", __func__,
		 msm_hifi_control);
	if (pdata->hph_en1_gpio < 0) {
		pr_err("%s: hph_en1_gpio is invalid\n", __func__);
		return -EINVAL;
	}
	if (msm_hifi_control == MSM8996_HIFI_ON) {
		gpio_direction_output(pdata->hph_en1_gpio, 1);
		/* 5msec delay needed as per HW requirement */
		usleep_range(5000, 5010);
	} else {
		gpio_direction_output(pdata->hph_en1_gpio, 0);
	}
	snd_soc_dapm_sync(dapm);
	return 0;
}

static int msm8996_hifi_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_hifi_control = %d\n",
			 __func__, msm_hifi_control);
	ucontrol->value.integer.value[0] = msm_hifi_control;
	return 0;
}

static int msm8996_hifi_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	pr_debug("%s() ucontrol->value.integer.value[0] = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);

	msm_hifi_control = ucontrol->value.integer.value[0];
	msm8996_hifi_ctrl(codec);
	return 1;
}

static int msm8996_ext_us_amp_init(void)
{
	int ret = 0;

	ext_us_amp_gpio = of_get_named_gpio(spdev->dev.of_node,
				"qcom,ext-ult-spk-amp-gpio", 0);
	if (ext_us_amp_gpio >= 0) {
		ret = gpio_request(ext_us_amp_gpio, "ext_us_amp_gpio");
		if (ret) {
			pr_err("%s: ext_us_amp_gpio request failed, ret:%d\n",
				__func__, ret);
			return ret;
		}
		gpio_direction_output(ext_us_amp_gpio, 0);
	}
	return ret;
}

static void msm8996_ext_us_amp_enable(u32 on)
{
	if (on)
		gpio_direction_output(ext_us_amp_gpio, 1);
	else
		gpio_direction_output(ext_us_amp_gpio, 0);

	pr_debug("%s: US Emitter GPIO enable:%s\n", __func__,
			on ? "Enable" : "Disable");
}

static int msm_ext_ultrasound_event(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *k, int event)
{
	pr_debug("%s()\n", __func__);
	if (strcmp(w->name, "ultrasound amp")) {
		if (!gpio_is_valid(ext_us_amp_gpio)) {
			pr_err("%s: ext_us_amp_gpio isn't configured\n",
				__func__);
			return -EINVAL;
		}
		if (SND_SOC_DAPM_EVENT_ON(event))
			msm8996_ext_us_amp_enable(1);
		else
			msm8996_ext_us_amp_enable(0);
	} else {
		pr_err("%s() Invalid Widget = %s\n",
				__func__, w->name);
		return -EINVAL;
	}
	return 0;
}

static int msm_snd_enable_codec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm)
{
	if (!strcmp(dev_name(codec->dev), "tasha_codec"))
		return tasha_cdc_mclk_enable(codec, enable, dapm);
	else {
		dev_err(codec->dev, "%s: unknown codec to enable ext clk\n",
			__func__);
		return -EINVAL;
	}
}

static int msm8996_mclk_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	pr_debug("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return msm_snd_enable_codec_ext_clk(codec, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return msm_snd_enable_codec_ext_clk(codec, 0, true);
	}
	return 0;
}

static int msm_snd_enable_codec_ext_tx_clk(struct snd_soc_codec *codec,
					   int enable, bool dapm)
{
	int ret = 0;

	if (!strcmp(dev_name(codec->dev), "tasha_codec"))
		ret = tasha_cdc_mclk_tx_enable(codec, enable, dapm);
	else {
		dev_err(codec->dev, "%s: unknown codec to enable ext clk\n",
			__func__);
		ret = -EINVAL;
	}
	return ret;
}

static int msm8996_mclk_tx_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	pr_debug("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return msm_snd_enable_codec_ext_tx_clk(codec, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return msm_snd_enable_codec_ext_tx_clk(codec, 0, true);
	}
	return 0;
}

static int msm_hifi_ctrl_event(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *k, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct snd_soc_card *card = codec->component.card;
	struct msm8996_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);
	int ret = 0;

	pr_debug("%s: msm_hifi_control = %d", __func__,
		 msm_hifi_control);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (msm_hifi_control == MSM8996_HIFI_ON) {
			if (pdata->hph_en0_gpio < 0) {
				pr_err("%s: hph_en0_gpio is invalid\n",
					__func__);
				ret = -EINVAL;
				goto err;
			}
			gpio_direction_output(pdata->hph_en0_gpio, 1);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (msm_hifi_control == MSM8996_HIFI_ON) {
			if (pdata->hph_en0_gpio < 0) {
				pr_err("%s: hph_en0_gpio is invalid\n",
					__func__);
				ret = -EINVAL;
				goto err;
			}
			gpio_direction_output(pdata->hph_en0_gpio, 0);
		}
		break;
	}
err:
	return ret;
}

static const struct snd_soc_dapm_widget msm8996_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
	msm8996_mclk_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("MCLK TX",  SND_SOC_NOPM, 0, 0,
	msm8996_mclk_tx_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SPK("Lineout_1 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_3 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_2 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_4 amp", NULL),
	SND_SOC_DAPM_SPK("ultrasound amp", msm_ext_ultrasound_event),
	SND_SOC_DAPM_SPK("hifi amp", msm_hifi_ctrl_event),
	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCRight Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCLeft Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Analog Mic4", NULL),
	SND_SOC_DAPM_MIC("Analog Mic6", NULL),
	SND_SOC_DAPM_MIC("Analog Mic7", NULL),
	SND_SOC_DAPM_MIC("Analog Mic8", NULL),

	SND_SOC_DAPM_MIC("Digital Mic0", NULL),
	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
	SND_SOC_DAPM_MIC("Digital Mic3", NULL),
	SND_SOC_DAPM_MIC("Digital Mic4", NULL),
	SND_SOC_DAPM_MIC("Digital Mic5", NULL),
	SND_SOC_DAPM_MIC("Digital Mic6", NULL),
};

static struct snd_soc_dapm_route wcd9335_audio_paths[] = {
	{"MIC BIAS1", NULL, "MCLK TX"},
	{"MIC BIAS2", NULL, "MCLK TX"},
	{"MIC BIAS3", NULL, "MCLK TX"},
	{"MIC BIAS4", NULL, "MCLK TX"},
};

static int slim5_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (slim5_rx_sample_rate) {
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 3;
		break;

	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 2;
		break;

	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 1;
		break;

	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 0;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: slim5_rx_sample_rate = %d\n", __func__,
		 slim5_rx_sample_rate);

	return 0;
}

static int slim5_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: ucontrol value = %ld\n", __func__,
		 ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 3:
		slim5_rx_sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 2:
		slim5_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 1:
		slim5_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		slim5_rx_sample_rate = SAMPLING_RATE_48KHZ;
	}

	pr_debug("%s: slim5_rx_sample_rate = %d\n", __func__,
		 slim5_rx_sample_rate);

	return 0;
}

static int slim6_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (slim6_rx_sample_rate) {
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 3;
		break;

	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 2;
		break;

	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 1;
		break;

	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 0;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: slim6_rx_sample_rate = %d\n", __func__,
		 slim6_rx_sample_rate);

	return 0;
}

static int slim6_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 3:
		slim6_rx_sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 2:
		slim6_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 1:
		slim6_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		slim6_rx_sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}

	pr_debug("%s: ucontrol value = %ld, slim6_rx_sample_rate = %d\n",
		 __func__, ucontrol->value.integer.value[0],
		 slim6_rx_sample_rate);

	return 0;
}

static int slim0_tx_bit_format_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	switch (slim0_tx_bit_format) {
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

	pr_debug("%s: slim0_tx_bit_format = %d, ucontrol value = %ld\n",
			 __func__, slim0_tx_bit_format,
			ucontrol->value.integer.value[0]);
	return 0;
}

static int slim0_tx_bit_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;

	switch (ucontrol->value.integer.value[0]) {
	case 2:
		slim0_tx_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		slim0_tx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
		slim0_tx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	default:
		pr_err("%s: invalid value %ld\n", __func__,
		       ucontrol->value.integer.value[0]);
		rc = -EINVAL;
		break;
	}

	pr_debug("%s: ucontrol value = %ld, slim0_tx_bit_format = %d\n",
		 __func__, ucontrol->value.integer.value[0],
		 slim0_tx_bit_format);

	return rc;
}

static int slim0_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (slim0_rx_sample_rate) {
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 6;
		break;

	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 5;
		break;

	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 4;
		break;

	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 3;
		break;

	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 2;
		break;

	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 1;
		break;

	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 0;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: slim0_rx_sample_rate = %d\n", __func__,
		 slim0_rx_sample_rate);

	return 0;
}

static int slim0_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: ucontrol value = %ld\n", __func__,
		 ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 6:
		slim0_rx_sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 5:
		slim0_rx_sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 4:
		slim0_rx_sample_rate = SAMPLING_RATE_8KHZ;
		break;
	case 3:
		slim0_rx_sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 2:
		slim0_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 1:
		slim0_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		slim0_rx_sample_rate = SAMPLING_RATE_48KHZ;
	}

	pr_debug("%s: slim0_rx_sample_rate = %d\n", __func__,
		 slim0_rx_sample_rate);

	return 0;
}

static int slim0_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (slim0_tx_sample_rate) {
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 0;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: slim0_tx_sample_rate = %d\n", __func__,
				slim0_tx_sample_rate);
	return 0;
}

static int slim0_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;

	pr_debug("%s: ucontrol value = %ld\n", __func__,
			ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 2:
		slim0_tx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 1:
		slim0_tx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
		slim0_tx_sample_rate = SAMPLING_RATE_48KHZ;
		break;
	default:
		rc = -EINVAL;
		pr_err("%s: invalid sample rate being passed\n", __func__);
		break;
	}

	pr_debug("%s: slim0_tx_sample_rate = %d\n", __func__,
			slim0_tx_sample_rate);
	return rc;
}

static int slim5_rx_bit_format_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{

	switch (slim5_rx_bit_format) {
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

	pr_debug("%s: slim5_rx_bit_format = %d, ucontrol value = %ld\n",
		 __func__, slim5_rx_bit_format,
			ucontrol->value.integer.value[0]);

	return 0;
}

static int slim5_rx_bit_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 2:
		slim5_rx_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		slim5_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		slim5_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return 0;
}

static int slim6_rx_bit_format_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{

	switch (slim6_rx_bit_format) {
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

	pr_debug("%s: slim6_rx_bit_format = %d, ucontrol value = %ld\n",
		 __func__, slim6_rx_bit_format,
		 ucontrol->value.integer.value[0]);

	return 0;
}

static int slim6_rx_bit_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 2:
		slim6_rx_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		slim6_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		slim6_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return 0;
}

static int slim0_rx_bit_format_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{

	switch (slim0_rx_bit_format) {
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

	pr_debug("%s: slim0_rx_bit_format = %d, ucontrol value = %ld\n",
		 __func__, slim0_rx_bit_format,
			ucontrol->value.integer.value[0]);

	return 0;
}

static int slim0_rx_bit_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 2:
		slim0_rx_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		slim0_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		slim0_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return 0;
}

static int msm_slim_5_rx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_slim_5_rx_ch  = %d\n", __func__,
		 msm_slim_5_rx_ch);
	ucontrol->value.integer.value[0] = msm_slim_5_rx_ch - 1;
	return 0;
}

static int msm_slim_5_rx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	msm_slim_5_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_slim_5_rx_ch = %d\n", __func__,
		 msm_slim_5_rx_ch);
	return 1;
}

static int msm_slim_6_rx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_slim_6_rx_ch  = %d\n", __func__,
		 msm_slim_6_rx_ch);
	ucontrol->value.integer.value[0] = msm_slim_6_rx_ch - 1;
	return 0;
}

static int msm_slim_6_rx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	msm_slim_6_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_slim_6_rx_ch = %d\n", __func__,
		 msm_slim_6_rx_ch);
	return 1;
}

static int msm_slim_0_rx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_slim_0_rx_ch  = %d\n", __func__,
		 msm_slim_0_rx_ch);
	ucontrol->value.integer.value[0] = msm_slim_0_rx_ch - 1;
	return 0;
}

static int msm_slim_0_rx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	msm_slim_0_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_slim_0_rx_ch = %d\n", __func__,
		 msm_slim_0_rx_ch);
	return 1;
}

static int msm_slim_0_tx_ch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_slim_0_tx_ch  = %d\n", __func__,
		 msm_slim_0_tx_ch);
	ucontrol->value.integer.value[0] = msm_slim_0_tx_ch - 1;
	return 0;
}

static int msm_slim_0_tx_ch_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	msm_slim_0_tx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_slim_0_tx_ch = %d\n", __func__, msm_slim_0_tx_ch);
	return 1;
}

static int msm_slim_1_tx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_slim_1_tx_ch  = %d\n", __func__,
		 msm_slim_1_tx_ch);
	ucontrol->value.integer.value[0] = msm_slim_1_tx_ch - 1;
	return 0;
}

static int msm_slim_1_tx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm_slim_1_tx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_slim_1_tx_ch = %d\n", __func__, msm_slim_1_tx_ch);
	return 1;
}

static int msm_vi_feed_tx_ch_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_vi_feed_tx_ch - 1;
	pr_debug("%s: msm_vi_feed_tx_ch = %ld\n", __func__,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_vi_feed_tx_ch_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	msm_vi_feed_tx_ch = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: msm_vi_feed_tx_ch = %d\n", __func__, msm_vi_feed_tx_ch);
	return 1;
}

static int hdmi_rx_bit_format_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{

	switch (hdmi_rx_bit_format) {
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

	pr_debug("%s: hdmi_rx_bit_format = %d, ucontrol value = %ld\n",
		 __func__, hdmi_rx_bit_format,
			ucontrol->value.integer.value[0]);

	return 0;
}

static int hdmi_rx_bit_format_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 2:
		hdmi_rx_bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		hdmi_rx_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		hdmi_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: hdmi_rx_bit_format = %d, ucontrol value = %ld\n",
		 __func__, hdmi_rx_bit_format,
			ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_hdmi_rx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_hdmi_rx_ch  = %d\n", __func__,
		 msm_hdmi_rx_ch);
	ucontrol->value.integer.value[0] = msm_hdmi_rx_ch - 2;

	return 0;
}

static int msm_hdmi_rx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	msm_hdmi_rx_ch = ucontrol->value.integer.value[0] + 2;
	if (msm_hdmi_rx_ch > 8) {
		pr_err("%s: channels %d exceeded 8.Limiting to max chs-8\n",
			__func__, msm_hdmi_rx_ch);
		msm_hdmi_rx_ch = 8;
	}
	pr_debug("%s: msm_hdmi_rx_ch = %d\n", __func__, msm_hdmi_rx_ch);

	return 1;
}

static int hdmi_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (hdmi_rx_sample_rate) {
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 2;
		break;

	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 1;
		break;

	case SAMPLING_RATE_48KHZ:
	default:
		sample_rate_val = 0;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: hdmi_rx_sample_rate = %d\n", __func__,
		 hdmi_rx_sample_rate);

	return 0;
}

static int hdmi_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: ucontrol value = %ld\n", __func__,
		 ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 2:
		hdmi_rx_sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 1:
		hdmi_rx_sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		hdmi_rx_sample_rate = SAMPLING_RATE_48KHZ;
	}

	pr_debug("%s: hdmi_rx_sample_rate = %d\n", __func__,
		 hdmi_rx_sample_rate);

	return 0;
}

static int usb_audio_rx_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: usb_audio_rx_ch  = %d\n", __func__,
			usb_rx_cfg.channels);
	ucontrol->value.integer.value[0] = usb_rx_cfg.channels - 1;
	return 0;
}

static int usb_audio_rx_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	usb_rx_cfg.channels = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: usb_audio_rx_ch = %d\n", __func__, usb_rx_cfg.channels);
	return 1;
}

static int usb_audio_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (usb_rx_cfg.sample_rate) {
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 10;
		break;
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 9;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 8;
		break;
	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_22P05KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_11P025KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_8KHZ:
	default:
		sample_rate_val = 0;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: usb_audio_rx_sample_rate = %d\n", __func__,
		 usb_rx_cfg.sample_rate);
	return 0;
}

static int usb_audio_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 10:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 9:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 8:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 7:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 6:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 5:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 4:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 3:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_22P05KHZ;
		break;
	case 2:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 1:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_11P025KHZ;
		break;
	case 0:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_8KHZ;
		break;
	default:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}

	pr_debug("%s: control value = %ld, usb_audio_rx_sample_rate = %d\n",
		__func__, ucontrol->value.integer.value[0],
		usb_rx_cfg.sample_rate);
	return 0;
}

static int usb_audio_rx_format_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (usb_rx_cfg.bit_format) {
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

	pr_debug("%s: usb_audio_rx_format = %d, ucontrol value = %ld\n",
		 __func__, usb_rx_cfg.bit_format,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int usb_audio_rx_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;

	switch (ucontrol->value.integer.value[0]) {
	case 2:
		usb_rx_cfg.bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		usb_rx_cfg.bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		usb_rx_cfg.bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: usb_audio_rx_format = %d, ucontrol value = %ld\n",
		 __func__, usb_rx_cfg.bit_format,
		 ucontrol->value.integer.value[0]);

	return rc;
}

static int usb_audio_tx_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: usb_audio_tx_ch  = %d\n", __func__,
		 usb_tx_cfg.channels);
	ucontrol->value.integer.value[0] = usb_tx_cfg.channels - 1;
	return 0;
}

static int usb_audio_tx_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	usb_tx_cfg.channels = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: usb_audio_tx_ch = %d\n", __func__, usb_tx_cfg.channels);
	return 1;
}

static int usb_audio_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val = 0;

	switch (usb_tx_cfg.sample_rate) {
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 10;
		break;
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 9;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 8;
		break;
	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_22P05KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_11P025KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 0;
		break;
	default:
		sample_rate_val = 6;
		break;
	}

	ucontrol->value.integer.value[0] = sample_rate_val;
	pr_debug("%s: usb_audio_tx_sample_rate = %d\n", __func__,
		 usb_tx_cfg.sample_rate);
	return 0;
}

static int usb_audio_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 10:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 9:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 8:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 7:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 6:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 5:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 4:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 3:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_22P05KHZ;
		break;
	case 2:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 1:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_11P025KHZ;
		break;
	case 0:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_8KHZ;
		break;
	default:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}

	pr_debug("%s: control value = %ld, usb_audio_tx_sample_rate = %d\n",
		__func__, ucontrol->value.integer.value[0],
		usb_tx_cfg.sample_rate);
	return 0;
}

static int usb_audio_tx_format_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (usb_tx_cfg.bit_format) {
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

	pr_debug("%s: usb_audio_tx_format = %d, ucontrol value = %ld\n",
		 __func__, usb_tx_cfg.bit_format,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int usb_audio_tx_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;

	switch (ucontrol->value.integer.value[0]) {
	case 2:
		usb_tx_cfg.bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		usb_tx_cfg.bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		usb_tx_cfg.bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: usb_audio_tx_format = %d, ucontrol value = %ld\n",
		 __func__, usb_tx_cfg.bit_format,
		 ucontrol->value.integer.value[0]);

	return rc;
}

static int msm8996_auxpcm_rate_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm8996_auxpcm_rate;
	return 0;
}

static int msm8996_auxpcm_rate_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm8996_auxpcm_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		msm8996_auxpcm_rate = SAMPLING_RATE_16KHZ;
		break;
	default:
		msm8996_auxpcm_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	return 0;
}

static int msm_proxy_rx_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_proxy_rx_ch = %d\n", __func__, msm_proxy_rx_ch);
	ucontrol->value.integer.value[0] = msm_proxy_rx_ch - 1;
	return 0;
}

static int msm_proxy_rx_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	msm_proxy_rx_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_proxy_rx_ch = %d\n", __func__, msm_proxy_rx_ch);
	return 1;
}

static int msm_pri_tdm_tx_0_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_pri_tdm_tx_0_ch = %d\n", __func__,
			msm_pri_tdm_tx_0_ch);
	ucontrol->value.integer.value[0] = msm_pri_tdm_tx_0_ch - 1;
	return 0;
}

static int msm_pri_tdm_tx_0_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_pri_tdm_tx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_pri_tdm_tx_0_ch = %d\n", __func__,
			msm_pri_tdm_tx_0_ch);
	return 0;
}

static int msm_pri_tdm_tx_1_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: pri_tdm_tx_1_ch = %d\n", __func__,
			msm_pri_tdm_tx_1_ch);
	ucontrol->value.integer.value[0] = msm_pri_tdm_tx_1_ch - 1;
	return 0;
}

static int msm_pri_tdm_tx_1_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_pri_tdm_tx_1_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_pri_tdm_tx_1_ch = %d\n", __func__,
			msm_pri_tdm_tx_1_ch);
	return 0;
}

static int msm_pri_tdm_tx_2_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_pri_tdm_tx_2_ch = %d\n", __func__,
			msm_pri_tdm_tx_2_ch);
	ucontrol->value.integer.value[0] = msm_pri_tdm_tx_2_ch - 1;
	return 0;
}

static int msm_pri_tdm_tx_2_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_pri_tdm_tx_2_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_pri_tdm_tx_2_ch = %d\n", __func__,
			msm_pri_tdm_tx_2_ch);
	return 0;
}
static int msm_pri_tdm_tx_3_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_pri_tdm_tx_3_ch = %d\n", __func__,
			msm_pri_tdm_tx_3_ch);
	ucontrol->value.integer.value[0] = msm_pri_tdm_tx_3_ch - 1;
	return 0;
}

static int msm_pri_tdm_tx_3_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_pri_tdm_tx_3_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_pri_tdm_tx_3_ch = %d\n", __func__,
			msm_pri_tdm_tx_3_ch);
	return 0;
}

static int msm_pri_tdm_rx_0_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_pri_tdm_rx_0_ch = %d\n", __func__,
			msm_pri_tdm_rx_0_ch);
	ucontrol->value.integer.value[0] = msm_pri_tdm_rx_0_ch - 1;
	return 0;
}

static int msm_pri_tdm_rx_0_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_pri_tdm_rx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_pri_tdm_rx_0_ch = %d\n", __func__,
			msm_pri_tdm_rx_0_ch);
	return 0;
}

static int msm_pri_tdm_rx_1_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_pri_tdm_rx_1_ch = %d\n", __func__,
			msm_pri_tdm_rx_1_ch);
	ucontrol->value.integer.value[0] = msm_pri_tdm_rx_1_ch - 1;
	return 0;
}

static int msm_pri_tdm_rx_1_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_pri_tdm_rx_1_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_pri_tdm_rx_1_ch = %d\n", __func__,
		msm_pri_tdm_rx_1_ch);
	return 0;
}

static int msm_pri_tdm_rx_2_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_pri_tdm_rx_2_ch = %d\n", __func__,
			msm_pri_tdm_rx_2_ch);
	ucontrol->value.integer.value[0] = msm_pri_tdm_rx_2_ch - 1;
	return 0;
}

static int msm_pri_tdm_rx_2_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_pri_tdm_rx_2_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_pri_tdm_rx_2_ch = %d\n", __func__,
		msm_pri_tdm_rx_2_ch);
	return 0;
}

static int msm_pri_tdm_rx_3_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_pri_tdm_rx_3_ch = %d\n", __func__,
			msm_pri_tdm_rx_3_ch);
	ucontrol->value.integer.value[0] = msm_pri_tdm_rx_3_ch - 1;
	return 0;
}

static int msm_pri_tdm_rx_3_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_pri_tdm_rx_3_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_pri_tdm_rx_3_ch = %d\n", __func__,
		msm_pri_tdm_rx_3_ch);
	return 0;
}

static int msm_pri_tdm_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_pri_tdm_rate;
	pr_debug("%s: msm_pri_tdm_rate = %d\n", __func__, msm_pri_tdm_rate);
	return 0;
}
static int msm_pri_tdm_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_pri_tdm_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		msm_pri_tdm_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
	default:
		msm_pri_tdm_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	pr_debug("%s: msm_pri_tdm_rate = %d\n",
		__func__, msm_pri_tdm_rate);
	return 0;
}

static int msm_sec_tdm_rate_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_sec_tdm_rate;
	pr_debug("%s: msm_sec_tdm_rate = %d\n", __func__, msm_sec_tdm_rate);
	return 0;
}

static int msm_sec_tdm_rate_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_sec_tdm_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		msm_sec_tdm_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
	default:
		msm_sec_tdm_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	pr_debug("%s: msm_sec_tdm_rate = %d\n",
			__func__, msm_sec_tdm_rate);
	return 0;
}

static int msm_tert_tdm_rate_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_tert_tdm_rate;
	pr_debug("%s: msm_tert_tdm_rate = %d\n", __func__, msm_tert_tdm_rate);
	return 0;
}

static int msm_tert_tdm_rate_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_tert_tdm_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		msm_tert_tdm_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
	default:
		msm_tert_tdm_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	pr_debug("%s: msm_tert_tdm_rate = %d\n",
			__func__, msm_tert_tdm_rate);
	return 0;
}

static int msm_quat_tdm_rate_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_quat_tdm_rate;
	pr_debug("%s: msm_quat_tdm_rate = %d\n", __func__, msm_quat_tdm_rate);
	return 0;
}
static int msm_quat_tdm_rate_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_quat_tdm_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		msm_quat_tdm_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
	default:
		msm_quat_tdm_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	pr_debug("%s: msm_quat_tdm_rate = %d\n",
			__func__, msm_quat_tdm_rate);
	return 0;
}

static int msm_pri_tdm_slot_width_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_pri_tdm_slot_width;
	pr_debug("%s: msm_pri_tdm_slot_width = %d\n",
		__func__, msm_pri_tdm_slot_width);
	return 0;
}

static int msm_pri_tdm_slot_width_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_pri_tdm_slot_width = 16;
		break;
	case 1:
		msm_pri_tdm_slot_width = 24;
		break;
	case 2:
	default:
		msm_pri_tdm_slot_width = 32;
		break;
	}
	pr_debug("%s: msm_pri_tdm_slot_width= %d\n",
			__func__, msm_pri_tdm_slot_width);
	return 0;
}

static int msm_sec_tdm_slot_width_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_sec_tdm_slot_width;
	pr_debug("%s: msm_sec_tdm_slot_width = %d\n",
		__func__, msm_sec_tdm_slot_width);
	return 0;
}

static int msm_sec_tdm_slot_width_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_sec_tdm_slot_width = 16;
		break;
	case 1:
		msm_sec_tdm_slot_width = 24;
		break;
	case 2:
	default:
		msm_sec_tdm_slot_width = 32;
		break;
	}
	pr_debug("%s: msm_sec_tdm_slot_width= %d\n",
			__func__, msm_sec_tdm_slot_width);
	return 0;
}

static int msm_tert_tdm_slot_width_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_tert_tdm_slot_width;
	pr_debug("%s: msm_tert_tdm_slot_width = %d\n",
		__func__, msm_tert_tdm_slot_width);
	return 0;
}

static int msm_tert_tdm_slot_width_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_tert_tdm_slot_width = 16;
		break;
	case 1:
		msm_tert_tdm_slot_width = 24;
		break;
	case 2:
	default:
		msm_tert_tdm_slot_width = 32;
		break;
	}
	pr_debug("%s: msm_tert_tdm_slot_width= %d\n",
			__func__, msm_tert_tdm_slot_width);
	return 0;
}

static int msm_quat_tdm_slot_width_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = msm_quat_tdm_slot_width;
	pr_debug("%s: msm_quat_tdm_slot_width = %d\n",
	__func__, msm_quat_tdm_slot_width);
	return 0;
}

static int msm_quat_tdm_slot_width_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_quat_tdm_slot_width = 16;
		break;
	case 1:
		msm_quat_tdm_slot_width = 24;
		break;
	case 2:
	default:
		msm_quat_tdm_slot_width = 32;
		break;
	}
	pr_debug("%s: msm_quat_tdm_slot_width= %d\n",
		__func__, msm_quat_tdm_slot_width);
	return 0;
}


static int msm_pri_tdm_slot_num_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_pri_tdm_slot_num) {
	case 1:
		ucontrol->value.integer.value[0] = 0;
		break;
	case 2:
		ucontrol->value.integer.value[0] = 1;
		break;
	case 4:
		ucontrol->value.integer.value[0] = 2;
		break;
	case 8:
		ucontrol->value.integer.value[0] = 3;
		break;
	case 16:
		ucontrol->value.integer.value[0] = 4;
		break;
	case 32:
	default:
		ucontrol->value.integer.value[0] = 5;
		break;
	}

	pr_debug("%s: msm_pri_tdm_slot_num = %d\n",
		__func__, msm_pri_tdm_slot_num);
	return 0;
}

static int msm_pri_tdm_slot_num_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_pri_tdm_slot_num = 1;
		break;
	case 1:
		msm_pri_tdm_slot_num = 2;
		break;
	case 2:
		msm_pri_tdm_slot_num = 4;
		break;
	case 3:
		msm_pri_tdm_slot_num = 8;
		break;
	case 4:
		msm_pri_tdm_slot_num = 16;
		break;
	case 5:
		msm_pri_tdm_slot_num = 32;
		break;
	default:
		msm_pri_tdm_slot_num = 8;
		break;
	}
	pr_debug("%s: msm_pri_tdm_slot_num = %d\n",
			__func__,  msm_pri_tdm_slot_num);
	return 0;
}

static int msm_sec_tdm_slot_num_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_sec_tdm_slot_num) {
	case 1:
		ucontrol->value.integer.value[0] = 0;
		break;
	case 2:
		ucontrol->value.integer.value[0] = 1;
		break;
	case 4:
		ucontrol->value.integer.value[0] = 2;
		break;
	case 8:
		ucontrol->value.integer.value[0] = 3;
		break;
	case 16:
		ucontrol->value.integer.value[0] = 4;
		break;
	case 32:
	default:
		ucontrol->value.integer.value[0] = 5;
		break;
	}

	pr_debug("%s: msm_sec_tdm_slot_num = %d\n",
			__func__, msm_sec_tdm_slot_num);
	return 0;
}

static int msm_sec_tdm_slot_num_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_sec_tdm_slot_num = 1;
		break;
	case 1:
		msm_sec_tdm_slot_num = 2;
		break;
	case 2:
		msm_sec_tdm_slot_num = 4;
		break;
	case 3:
		msm_sec_tdm_slot_num = 8;
		break;
	case 4:
		msm_sec_tdm_slot_num = 16;
		break;
	case 5:
		msm_sec_tdm_slot_num = 32;
		break;
	default:
		msm_sec_tdm_slot_num = 8;
		break;
	}
	pr_debug("%s: msm_sec_tdm_slot_num = %d\n",
				__func__,  msm_sec_tdm_slot_num);
	return 0;
}

static int msm_tert_tdm_slot_num_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_tert_tdm_slot_num) {
	case 1:
		ucontrol->value.integer.value[0] = 0;
		break;
	case 2:
		ucontrol->value.integer.value[0] = 1;
		break;
	case 4:
		ucontrol->value.integer.value[0] = 2;
		break;
	case 8:
		ucontrol->value.integer.value[0] = 3;
		break;
	case 16:
		ucontrol->value.integer.value[0] = 4;
		break;
	case 32:
	default:
		ucontrol->value.integer.value[0] = 5;
		break;
	}

	pr_debug("%s: msm_tert_tdm_slot_num = %d\n",
			__func__, msm_tert_tdm_slot_num);
	return 0;
}

static int msm_tert_tdm_slot_num_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_tert_tdm_slot_num = 1;
		break;
	case 1:
		msm_tert_tdm_slot_num = 2;
		break;
	case 2:
		msm_tert_tdm_slot_num = 4;
		break;
	case 3:
		msm_tert_tdm_slot_num = 8;
		break;
	case 4:
		msm_tert_tdm_slot_num = 16;
		break;
	case 5:
		msm_tert_tdm_slot_num = 32;
		break;
	default:
		msm_tert_tdm_slot_num = 8;
		break;
	}
	pr_debug("%s: msm_tert_tdm_slot_num = %d\n",
			__func__,  msm_tert_tdm_slot_num);
	return 0;
}

static int msm_quat_tdm_slot_num_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_quat_tdm_slot_num) {
	case 1:
		ucontrol->value.integer.value[0] = 0;
		break;
	case 2:
		ucontrol->value.integer.value[0] = 1;
		break;
	case 4:
		ucontrol->value.integer.value[0] = 2;
		break;
	case 8:
		ucontrol->value.integer.value[0] = 3;
		break;
	case 16:
		ucontrol->value.integer.value[0] = 4;
		break;
	case 32:
	default:
		ucontrol->value.integer.value[0] = 5;
		break;
	}

	pr_debug("%s: msm_quat_tdm_slot_num = %d\n",
				__func__, msm_quat_tdm_slot_num);
	return 0;
}
static int msm_quat_tdm_slot_num_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm_quat_tdm_slot_num = 1;
		break;
	case 1:
		msm_quat_tdm_slot_num = 2;
		break;
	case 2:
		msm_quat_tdm_slot_num = 4;
		break;
	case 3:
		msm_quat_tdm_slot_num = 8;
		break;
	case 4:
		msm_quat_tdm_slot_num = 16;
		break;
	case 5:
		msm_quat_tdm_slot_num = 32;
		break;
	default:
		msm_quat_tdm_slot_num = 8;
		break;
	}
	pr_debug("%s: msm_quat_tdm_slot_num = %d\n",
			__func__,  msm_quat_tdm_slot_num);
	return 0;
}

static int msm_tdm_slot_mapping_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_multi_mixer_control *mc =
		(struct soc_multi_mixer_control *)kcontrol->private_value;
	unsigned int *slot_offset;
	int i;

	if (mc->shift >= TDM_MAX) {
		pr_err("%s invalid port index %d\n", __func__, mc->shift);
		return -EINVAL;
	}

	slot_offset = tdm_slot_offset[mc->shift];
	for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
		ucontrol->value.integer.value[i] = slot_offset[i];
		pr_debug("%s port index %d offset %d value %d\n",
				__func__, mc->shift, i, slot_offset[i]);
	}

	return 0;
}

static int msm_tdm_slot_mapping_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_multi_mixer_control *mc =
		(struct soc_multi_mixer_control *)kcontrol->private_value;
	unsigned int *slot_offset;
	int i;

	if (mc->shift >= TDM_MAX) {
		pr_err("%s invalid port index %d\n", __func__, mc->shift);
		return -EINVAL;
	}

	slot_offset = tdm_slot_offset[mc->shift];

	for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
		slot_offset[i] = ucontrol->value.integer.value[i];
		pr_debug("%s port index %d offset %d value %d\n",
				__func__, mc->shift, i, slot_offset[i]);
	}

	return 0;
}

static int msm_sec_tdm_rx_0_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_sec_tdm_rx_0_ch = %d\n", __func__,
			msm_sec_tdm_rx_0_ch);
	ucontrol->value.integer.value[0] = msm_sec_tdm_rx_0_ch - 1;
	return 0;
}

static int msm_sec_tdm_rx_0_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_sec_tdm_rx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_sec_tdm_rx_0_ch = %d\n", __func__,
		msm_sec_tdm_rx_0_ch);
	return 0;
}

static int msm_sec_tdm_rx_1_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_sec_tdm_rx_1_ch = %d\n", __func__,
		msm_sec_tdm_rx_1_ch);
	ucontrol->value.integer.value[0] = msm_sec_tdm_rx_1_ch - 1;
	return 0;
}

static int msm_sec_tdm_rx_1_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_sec_tdm_rx_1_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_sec_tdm_rx_1_ch = %d\n", __func__,
		msm_sec_tdm_rx_1_ch);
	return 0;
}

static int msm_sec_tdm_rx_2_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_sec_tdm_rx_2_ch = %d\n", __func__,
			msm_sec_tdm_rx_2_ch);
	ucontrol->value.integer.value[0] = msm_sec_tdm_rx_2_ch - 1;
	return 0;
}

static int msm_sec_tdm_rx_2_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_sec_tdm_rx_2_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_sec_tdm_rx_2_ch = %d\n", __func__,
			msm_sec_tdm_rx_2_ch);
	return 0;
}

static int msm_sec_tdm_rx_3_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_sec_tdm_rx_3_ch = %d\n", __func__,
			msm_sec_tdm_rx_3_ch);
	ucontrol->value.integer.value[0] = msm_sec_tdm_rx_3_ch - 1;
	return 0;
}

static int msm_sec_tdm_rx_3_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_sec_tdm_rx_3_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_sec_tdm_rx_3_ch = %d\n", __func__,
			msm_sec_tdm_rx_3_ch);
	return 0;
}

static int msm_sec_tdm_tx_0_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_sec_tdm_tx_0_ch = %d\n", __func__,
			msm_sec_tdm_tx_0_ch);
	ucontrol->value.integer.value[0] = msm_sec_tdm_tx_0_ch - 1;
	return 0;
}

static int msm_sec_tdm_tx_0_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_sec_tdm_tx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_sec_tdm_tx_0_ch = %d\n", __func__,
			msm_sec_tdm_tx_0_ch);
	return 0;
}

static int msm_sec_tdm_tx_1_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_sec_tdm_tx_1_ch = %d\n", __func__,
				msm_sec_tdm_tx_1_ch);
	ucontrol->value.integer.value[0] = msm_sec_tdm_tx_1_ch - 1;
	return 0;
}

static int msm_sec_tdm_tx_1_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_sec_tdm_tx_1_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_sec_tdm_tx_1_ch = %d\n", __func__,
		msm_sec_tdm_tx_1_ch);
	return 0;
}
static int msm_sec_tdm_tx_2_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_sec_tdm_tx_2_ch = %d\n", __func__,
			msm_sec_tdm_tx_2_ch);
	ucontrol->value.integer.value[0] = msm_sec_tdm_tx_2_ch - 1;
	return 0;
}

static int msm_sec_tdm_tx_2_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_sec_tdm_tx_2_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_sec_tdm_tx_2_ch = %d\n", __func__,
			msm_sec_tdm_tx_2_ch);
	return 0;
}

static int msm_sec_tdm_tx_3_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_sec_tdm_tx_3_ch = %d\n", __func__,
		msm_sec_tdm_tx_3_ch);
	ucontrol->value.integer.value[0] = msm_sec_tdm_tx_3_ch - 1;
	return 0;
}

static int msm_sec_tdm_tx_3_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_sec_tdm_tx_3_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_sec_tdm_tx_3_ch = %d\n", __func__,
			msm_sec_tdm_tx_3_ch);
	return 0;
}

static int msm_tert_tdm_rx_0_ch_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_tert_tdm_rx_0_ch = %d\n", __func__,
			msm_tert_tdm_rx_0_ch);
	ucontrol->value.integer.value[0] = msm_tert_tdm_rx_0_ch - 1;
	return 0;
}

static int msm_tert_tdm_rx_0_ch_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	msm_tert_tdm_rx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_tert_tdm_rx_0_ch = %d\n", __func__,
			msm_tert_tdm_rx_0_ch);
	return 0;
}
static int msm_tert_tdm_rx_1_ch_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_tert_tdm_rx_1_ch = %d\n", __func__,
			msm_tert_tdm_rx_1_ch);
	ucontrol->value.integer.value[0] = msm_tert_tdm_rx_1_ch - 1;
	return 0;
}

static int msm_tert_tdm_rx_1_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	msm_tert_tdm_rx_1_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_tert_tdm_rx_1_ch = %d\n", __func__,
			msm_tert_tdm_rx_1_ch);
	return 0;
}

static int msm_tert_tdm_rx_2_ch_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_tert_tdm_rx_2_ch = %d\n", __func__,
			msm_tert_tdm_rx_2_ch);
	ucontrol->value.integer.value[0] = msm_tert_tdm_rx_2_ch - 1;
	return 0;
}

static int msm_tert_tdm_rx_2_ch_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	msm_tert_tdm_rx_2_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_tert_tdm_rx_2_ch = %d\n", __func__,
			msm_tert_tdm_rx_2_ch);
	return 0;
}

static int msm_tert_tdm_rx_3_ch_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_tert_tdm_rx_3_ch = %d\n", __func__,
			msm_tert_tdm_rx_3_ch);
	ucontrol->value.integer.value[0] = msm_tert_tdm_rx_3_ch - 1;
	return 0;
}

static int msm_tert_tdm_rx_3_ch_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	msm_tert_tdm_rx_3_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_tert_tdm_rx_3_ch = %d\n", __func__,
			msm_tert_tdm_rx_3_ch);
	return 0;
}

static int msm_tert_tdm_tx_0_ch_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_tert_tdm_tx_0_ch = %d\n", __func__,
			msm_tert_tdm_tx_0_ch);
	ucontrol->value.integer.value[0] = msm_tert_tdm_tx_0_ch - 1;
	return 0;
}

static int msm_tert_tdm_tx_0_ch_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	msm_tert_tdm_tx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_tert_tdm_tx_0_ch = %d\n", __func__,
			msm_tert_tdm_tx_0_ch);
	return 0;
}

static int msm_tert_tdm_tx_1_ch_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_tert_tdm_tx_1_ch = %d\n", __func__,
			msm_tert_tdm_tx_1_ch);
	ucontrol->value.integer.value[0] = msm_tert_tdm_tx_1_ch - 1;
	return 0;
}

static int msm_tert_tdm_tx_1_ch_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	msm_tert_tdm_tx_1_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_tert_tdm_tx_1_ch = %d\n", __func__,
			msm_tert_tdm_tx_1_ch);
	return 0;
}

static int msm_tert_tdm_tx_2_ch_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_tert_tdm_tx_2_ch = %d\n", __func__,
			msm_tert_tdm_tx_2_ch);
	ucontrol->value.integer.value[0] = msm_tert_tdm_tx_2_ch - 1;
	return 0;
}

static int msm_tert_tdm_tx_2_ch_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	msm_tert_tdm_tx_2_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_tert_tdm_tx_2_ch = %d\n", __func__,
			msm_tert_tdm_tx_2_ch);
	return 0;
}

static int msm_tert_tdm_tx_3_ch_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_tert_tdm_tx_3_ch = %d\n", __func__,
			msm_tert_tdm_tx_3_ch);
	ucontrol->value.integer.value[0] = msm_tert_tdm_tx_3_ch - 1;
	return 0;
}

static int msm_tert_tdm_tx_3_ch_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	msm_tert_tdm_tx_3_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_tert_tdm_tx_3_ch = %d\n", __func__,
			msm_tert_tdm_tx_3_ch);
	return 0;
}

static int msm_quat_tdm_rx_0_ch_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_quat_tdm_rx_0_ch = %d\n", __func__,
			msm_quat_tdm_rx_0_ch);
	ucontrol->value.integer.value[0] = msm_quat_tdm_rx_0_ch - 1;
	return 0;
}

static int msm_quat_tdm_rx_0_ch_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	msm_quat_tdm_rx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_quat_tdm_rx_0_ch = %d\n", __func__,
			msm_quat_tdm_rx_0_ch);
	return 0;
}

static int msm_quat_tdm_rx_1_ch_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_quat_tdm_rx_1_ch = %d\n", __func__,
			msm_quat_tdm_rx_1_ch);
	ucontrol->value.integer.value[0] = msm_quat_tdm_rx_1_ch - 1;
	return 0;
}

static int msm_quat_tdm_rx_1_ch_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	msm_quat_tdm_rx_1_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_quat_tdm_rx_1_ch = %d\n", __func__,
			msm_quat_tdm_rx_1_ch);
	return 0;
}

static int msm_quat_tdm_rx_2_ch_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_quat_tdm_rx_2_ch = %d\n", __func__,
			msm_quat_tdm_rx_2_ch);
	ucontrol->value.integer.value[0] = msm_quat_tdm_rx_2_ch - 1;
	return 0;
}

static int msm_quat_tdm_rx_2_ch_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	msm_quat_tdm_rx_2_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_quat_tdm_rx_2_ch = %d\n", __func__,
			msm_quat_tdm_rx_2_ch);
	return 0;
}

static int msm_quat_tdm_rx_3_ch_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_quat_tdm_rx_3_ch = %d\n", __func__,
			msm_quat_tdm_rx_3_ch);
	ucontrol->value.integer.value[0] = msm_quat_tdm_rx_3_ch - 1;
	return 0;
}

static int msm_quat_tdm_rx_3_ch_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	msm_quat_tdm_rx_3_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_quat_tdm_rx_3_ch = %d\n", __func__,
			msm_quat_tdm_rx_3_ch);
	return 0;
}
static int msm_quat_tdm_tx_0_ch_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_quat_tdm_tx_0_ch = %d\n", __func__,
			msm_quat_tdm_tx_0_ch);
	ucontrol->value.integer.value[0] = msm_quat_tdm_tx_0_ch - 1;
	return 0;
}

static int msm_quat_tdm_tx_0_ch_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	msm_quat_tdm_tx_0_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_quat_tdm_tx_0_ch = %d\n", __func__,
			msm_quat_tdm_tx_0_ch);
	return 0;
}

static int msm_quat_tdm_tx_1_ch_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_quat_tdm_tx_1_ch = %d\n", __func__,
			msm_quat_tdm_tx_1_ch);
	ucontrol->value.integer.value[0] = msm_quat_tdm_tx_1_ch - 1;
	return 0;
}

static int msm_quat_tdm_tx_1_ch_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	msm_quat_tdm_tx_1_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_quat_tdm_tx_1_ch = %d\n", __func__,
			msm_quat_tdm_tx_1_ch);
	return 0;
}

static int msm_quat_tdm_tx_2_ch_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_quat_tdm_tx_2_ch = %d\n", __func__,
			msm_quat_tdm_tx_2_ch);
	ucontrol->value.integer.value[0] = msm_quat_tdm_tx_2_ch - 1;
	return 0;
}

static int msm_quat_tdm_tx_2_ch_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	msm_quat_tdm_tx_2_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_quat_tdm_tx_2_ch = %d\n", __func__,
			msm_quat_tdm_tx_2_ch);
	return 0;
}

static int msm_quat_tdm_tx_3_ch_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_quat_tdm_tx_3_ch = %d\n", __func__,
			msm_quat_tdm_tx_3_ch);
	ucontrol->value.integer.value[0] = msm_quat_tdm_tx_3_ch - 1;
	return 0;
}

static int msm_quat_tdm_tx_3_ch_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	msm_quat_tdm_tx_3_ch = ucontrol->value.integer.value[0] + 1;
	pr_debug("%s: msm_quat_tdm_tx_3_ch = %d\n", __func__,
			msm_quat_tdm_tx_3_ch);
	return 0;
}

static int msm_pri_tdm_tx_0_bit_format_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_pri_tdm_tx_0_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_pri_tdm_tx_0_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_pri_tdm_tx_0_bit_format_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_pri_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_pri_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_pri_tdm_tx_0_bit_format = %d\n",
			__func__, msm_pri_tdm_tx_0_bit_format);
	return 0;
}

static int msm_pri_tdm_tx_1_bit_format_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_pri_tdm_tx_1_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_pri_tdm_tx_1_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_pri_tdm_tx_1_bit_format_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_pri_tdm_tx_1_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_pri_tdm_tx_1_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_pri_tdm_tx_1_bit_format = %d\n",
			__func__, msm_pri_tdm_tx_1_bit_format);
	return 0;
}

static int msm_pri_tdm_tx_2_bit_format_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_pri_tdm_tx_2_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_pri_tdm_tx_2_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_pri_tdm_tx_2_bit_format_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_pri_tdm_tx_2_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_pri_tdm_tx_2_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_pri_tdm_tx_2_bit_format = %d\n",
			__func__, msm_pri_tdm_tx_2_bit_format);
	return 0;
}

static int msm_pri_tdm_tx_3_bit_format_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_pri_tdm_tx_3_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_pri_tdm_tx_3_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_pri_tdm_tx_3_bit_format_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_pri_tdm_tx_3_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_pri_tdm_tx_3_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_pri_tdm_tx_3_bit_format = %d\n",
			__func__, msm_pri_tdm_tx_3_bit_format);
	return 0;
}

static int msm_pri_tdm_rx_0_bit_format_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_pri_tdm_rx_0_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_pri_tdm_rx_0_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_pri_tdm_rx_0_bit_format_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_pri_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_pri_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_pri_tdm_rx_0_bit_format = %d\n",
			__func__, msm_pri_tdm_rx_0_bit_format);
	return 0;
}

static int msm_pri_tdm_rx_1_bit_format_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_pri_tdm_rx_1_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_pri_tdm_rx_1_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_pri_tdm_rx_1_bit_format_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_pri_tdm_rx_1_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_pri_tdm_rx_1_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_pri_tdm_rx_1_bit_format = %d\n",
			__func__, msm_pri_tdm_rx_1_bit_format);
	return 0;
}

static int msm_pri_tdm_rx_2_bit_format_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_pri_tdm_rx_2_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_pri_tdm_rx_2_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_pri_tdm_rx_2_bit_format_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_pri_tdm_rx_2_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_pri_tdm_rx_2_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_pri_tdm_rx_2_bit_format = %d\n",
			__func__, msm_pri_tdm_rx_2_bit_format);
	return 0;
}

static int msm_pri_tdm_rx_3_bit_format_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_pri_tdm_rx_3_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_pri_tdm_rx_3_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_pri_tdm_rx_3_bit_format_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_pri_tdm_rx_3_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_pri_tdm_rx_3_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_pri_tdm_rx_3_bit_format = %d\n",
			__func__, msm_pri_tdm_rx_3_bit_format);
	return 0;
}

static int msm_sec_tdm_rx_0_bit_format_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_sec_tdm_rx_0_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_sec_tdm_rx_0_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_sec_tdm_rx_0_bit_format_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_sec_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_sec_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_sec_tdm_rx_0_bit_format = %d\n",
			__func__, msm_sec_tdm_rx_0_bit_format);
	return 0;
}

static int msm_sec_tdm_rx_1_bit_format_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_sec_tdm_rx_1_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_sec_tdm_rx_1_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_sec_tdm_rx_1_bit_format_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_sec_tdm_rx_1_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_sec_tdm_rx_1_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_sec_tdm_rx_1_bit_format = %d\n",
			__func__, msm_sec_tdm_rx_1_bit_format);
	return 0;
}

static int msm_sec_tdm_rx_2_bit_format_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_sec_tdm_rx_2_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_sec_tdm_rx_2_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_sec_tdm_rx_2_bit_format_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_sec_tdm_rx_2_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_sec_tdm_rx_2_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_sec_tdm_rx_2_bit_format = %d\n",
			__func__, msm_sec_tdm_rx_2_bit_format);
	return 0;
}

static int msm_sec_tdm_rx_3_bit_format_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_sec_tdm_rx_3_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_sec_tdm_rx_3_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_sec_tdm_rx_3_bit_format_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_sec_tdm_rx_3_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_sec_tdm_rx_3_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_sec_tdm_rx_3_bit_format = %d\n",
			__func__, msm_sec_tdm_rx_3_bit_format);
	return 0;
}

static int msm_sec_tdm_tx_0_bit_format_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_sec_tdm_tx_0_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_sec_tdm_tx_0_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_sec_tdm_tx_0_bit_format_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_sec_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_sec_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_sec_tdm_tx_0_bit_format = %d\n",
			__func__, msm_sec_tdm_tx_0_bit_format);
	return 0;
}

static int msm_sec_tdm_tx_1_bit_format_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_sec_tdm_tx_1_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_sec_tdm_tx_1_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_sec_tdm_tx_1_bit_format_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_sec_tdm_tx_1_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_sec_tdm_tx_1_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_sec_tdm_tx_1_bit_format = %d\n",
			__func__, msm_sec_tdm_tx_1_bit_format);
	return 0;
}

static int msm_sec_tdm_tx_2_bit_format_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_sec_tdm_tx_2_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_sec_tdm_tx_2_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}
static int msm_sec_tdm_tx_2_bit_format_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_sec_tdm_tx_2_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_sec_tdm_tx_2_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_sec_tdm_tx_2_bit_format = %d\n",
			__func__, msm_sec_tdm_tx_2_bit_format);
	return 0;
}

static int msm_sec_tdm_tx_3_bit_format_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_sec_tdm_tx_3_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_sec_tdm_tx_3_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_sec_tdm_tx_3_bit_format_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_sec_tdm_tx_3_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_sec_tdm_tx_3_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_sec_tdm_tx_3_bit_format = %d\n",
			__func__, msm_sec_tdm_tx_3_bit_format);
	return 0;
}

static int msm_tert_tdm_rx_0_bit_format_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_tert_tdm_rx_0_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_tert_tdm_rx_0_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_tert_tdm_rx_0_bit_format_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_tert_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_tert_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_tert_tdm_rx_0_bit_format = %d\n",
			__func__, msm_tert_tdm_rx_0_bit_format);
	return 0;
}

static int msm_tert_tdm_rx_1_bit_format_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_tert_tdm_rx_1_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_tert_tdm_rx_1_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_tert_tdm_rx_1_bit_format_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_tert_tdm_rx_1_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_tert_tdm_rx_1_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_tert_tdm_rx_1_bit_format = %d\n",
			__func__, msm_tert_tdm_rx_1_bit_format);
	return 0;
}

static int msm_tert_tdm_rx_2_bit_format_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_tert_tdm_rx_2_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_tert_tdm_rx_2_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_tert_tdm_rx_2_bit_format_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_tert_tdm_rx_2_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_tert_tdm_rx_2_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_tert_tdm_rx_2_bit_format = %d\n",
			__func__, msm_tert_tdm_rx_2_bit_format);
	return 0;
}

static int msm_tert_tdm_rx_3_bit_format_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_tert_tdm_rx_3_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_tert_tdm_rx_3_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_tert_tdm_rx_3_bit_format_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_tert_tdm_rx_3_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_tert_tdm_rx_3_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_tert_tdm_rx_3_bit_format = %d\n",
			__func__, msm_tert_tdm_rx_3_bit_format);
	return 0;
}

static int msm_tert_tdm_tx_0_bit_format_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_tert_tdm_tx_0_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_tert_tdm_tx_0_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_tert_tdm_tx_0_bit_format_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_tert_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_tert_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_tert_tdm_tx_0_bit_format = %d\n",
			__func__, msm_tert_tdm_tx_0_bit_format);
	return 0;
}

static int msm_tert_tdm_tx_1_bit_format_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_tert_tdm_tx_1_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_tert_tdm_tx_1_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_tert_tdm_tx_1_bit_format_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_tert_tdm_tx_1_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_tert_tdm_tx_1_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_tert_tdm_tx_1_bit_format = %d\n",
			__func__, msm_tert_tdm_tx_1_bit_format);
	return 0;
}

static int msm_tert_tdm_tx_2_bit_format_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_tert_tdm_tx_2_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_tert_tdm_tx_2_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_tert_tdm_tx_2_bit_format_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_tert_tdm_tx_2_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_tert_tdm_tx_2_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_tert_tdm_tx_2_bit_format = %d\n",
			__func__, msm_tert_tdm_tx_2_bit_format);
	return 0;
}

static int msm_tert_tdm_tx_3_bit_format_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_tert_tdm_tx_3_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_tert_tdm_tx_3_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_tert_tdm_tx_3_bit_format_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_tert_tdm_tx_3_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_tert_tdm_tx_3_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_tert_tdm_tx_3_bit_format = %d\n",
			__func__, msm_tert_tdm_tx_3_bit_format);
	return 0;
}

static int msm_quat_tdm_rx_0_bit_format_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_quat_tdm_rx_0_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_quat_tdm_rx_0_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_quat_tdm_rx_0_bit_format_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_quat_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_quat_tdm_rx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_quat_tdm_rx_0_bit_format = %d\n",
			__func__, msm_quat_tdm_rx_0_bit_format);
	return 0;
}

static int msm_quat_tdm_rx_1_bit_format_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_quat_tdm_rx_1_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_quat_tdm_rx_1_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}
static int msm_quat_tdm_rx_1_bit_format_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_quat_tdm_rx_1_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_quat_tdm_rx_1_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_quat_tdm_rx_1_bit_format = %d\n",
			__func__, msm_quat_tdm_rx_1_bit_format);
	return 0;
}

static int msm_quat_tdm_rx_2_bit_format_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_quat_tdm_rx_2_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_quat_tdm_rx_2_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_quat_tdm_rx_2_bit_format_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_quat_tdm_rx_2_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_quat_tdm_rx_2_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_quat_tdm_rx_2_bit_format = %d\n",
			__func__, msm_quat_tdm_rx_2_bit_format);
	return 0;
}

static int msm_quat_tdm_rx_3_bit_format_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_quat_tdm_rx_3_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_quat_tdm_rx_3_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_quat_tdm_rx_3_bit_format_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_quat_tdm_rx_3_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_quat_tdm_rx_3_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_quat_tdm_rx_3_bit_format = %d\n",
			__func__, msm_quat_tdm_rx_3_bit_format);
	return 0;
}

static int msm_quat_tdm_tx_0_bit_format_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_quat_tdm_tx_0_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_quat_tdm_tx_0_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}
static int msm_quat_tdm_tx_0_bit_format_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_quat_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_quat_tdm_tx_0_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_quat_tdm_tx_0_bit_format = %d\n",
			__func__, msm_quat_tdm_tx_0_bit_format);
	return 0;
}

static int msm_quat_tdm_tx_1_bit_format_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_quat_tdm_tx_1_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_quat_tdm_tx_1_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_quat_tdm_tx_1_bit_format_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_quat_tdm_tx_1_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_quat_tdm_tx_1_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_quat_tdm_tx_1_bit_format = %d\n",
			__func__, msm_quat_tdm_tx_1_bit_format);
	return 0;
}
static int msm_quat_tdm_tx_2_bit_format_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_quat_tdm_tx_2_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_quat_tdm_tx_2_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}

static int msm_quat_tdm_tx_2_bit_format_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_quat_tdm_tx_2_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_quat_tdm_tx_2_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_quat_tdm_tx_2_bit_format = %d\n",
			__func__, msm_quat_tdm_tx_2_bit_format);
	return 0;
}

static int msm_quat_tdm_tx_3_bit_format_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (msm_quat_tdm_tx_3_bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: msm_quat_tdm_tx_3_bit_format = %ld\n",
			__func__, ucontrol->value.integer.value[0]);
	return 0;
}
static int msm_quat_tdm_tx_3_bit_format_put(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		msm_quat_tdm_tx_3_bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		msm_quat_tdm_tx_3_bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: msm_quat_tdm_tx_3_bit_format = %d\n",
			__func__, msm_quat_tdm_tx_3_bit_format);
	return 0;
}


static int msm_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = msm8996_auxpcm_rate;
	channels->min = channels->max = 1;

	return 0;
}

static int msm_proxy_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					   struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s: msm_proxy_rx_ch =%d\n", __func__, msm_proxy_rx_ch);

	if (channels->max < 2)
		channels->min = channels->max = 2;
	channels->min = channels->max = msm_proxy_rx_ch;
	rate->min = rate->max = SAMPLING_RATE_48KHZ;
	return 0;
}

static int msm_proxy_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					   struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	rate->min = rate->max = SAMPLING_RATE_48KHZ;
	return 0;
}

static int msm8996_hdmi_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					      struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s channels->min %u channels->max %u ()\n", __func__,
		 channels->min, channels->max);

	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				hdmi_rx_bit_format);
	if (channels->max < 2)
		channels->min = channels->max = 2;
	rate->min = rate->max = hdmi_rx_sample_rate;
	channels->min = channels->max = msm_hdmi_rx_ch;

	return 0;
}

static int msm_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				     struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s: channel:%d\n", __func__, msm_tert_mi2s_tx_ch);
	rate->min = rate->max = SAMPLING_RATE_48KHZ;
	channels->min = channels->max = msm_tert_mi2s_tx_ch;
	return 0;
}

static int msm8996_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	pr_debug("%s: substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	mi2s_tx_clk.enable = 1;
	ret = afe_set_lpass_clock_v2(AFE_PORT_ID_TERTIARY_MI2S_TX,
				&mi2s_tx_clk);
	if (ret < 0) {
		pr_err("%s: afe lpass clock failed, err:%d\n", __func__, ret);
		goto err;
	}
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		pr_err("%s: set fmt cpu dai failed, err:%d\n", __func__, ret);
err:
	return ret;
}

static void msm8996_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret = 0;

	pr_debug("%s: substream = %s  stream = %d\n", __func__,
		substream->name, substream->stream);

	mi2s_tx_clk.enable = 0;
	ret = afe_set_lpass_clock_v2(AFE_PORT_ID_TERTIARY_MI2S_TX,
				&mi2s_tx_clk);
	if (ret < 0)
		pr_err("%s: afe lpass clock failed, err:%d\n", __func__, ret);
}

static struct snd_soc_ops msm8996_mi2s_be_ops = {
	.startup = msm8996_mi2s_snd_startup,
	.shutdown = msm8996_mi2s_snd_shutdown,
};

static int msm_slim_5_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				   slim5_rx_bit_format);
	rate->min = rate->max = slim5_rx_sample_rate;
	channels->min = channels->max = msm_slim_5_rx_ch;

	 pr_debug("%s: format = %d, rate = %d, channels = %d\n",
		  __func__, params_format(params), params_rate(params),
		  msm_slim_5_rx_ch);

	return 0;
}

static int msm_slim_6_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				   slim6_rx_bit_format);
	rate->min = rate->max = slim6_rx_sample_rate;
	channels->min = channels->max = msm_slim_6_rx_ch;

	pr_debug("%s: format = %d, rate = %d, channels = %d\n",
		 __func__, params_format(params), params_rate(params),
		 msm_slim_6_rx_ch);

	return 0;
}

static int msm_slim_0_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				   slim0_rx_bit_format);
	rate->min = rate->max = slim0_rx_sample_rate;
	channels->min = channels->max = msm_slim_0_rx_ch;

	 pr_debug("%s: format = %d, rate = %d, channels = %d\n",
		  __func__, params_format(params), params_rate(params),
		  msm_slim_0_rx_ch);

	return 0;
}

static int msm_slim_0_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT, slim0_tx_bit_format);
	rate->min = rate->max = slim0_tx_sample_rate;
	channels->min = channels->max = msm_slim_0_tx_ch;

	return 0;
}

static int msm_slim_1_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT, slim1_tx_bit_format);
	rate->min = rate->max = slim1_tx_sample_rate;
	channels->min = channels->max = msm_slim_1_tx_ch;

	return 0;
}

static int msm_slim_4_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
		       SNDRV_PCM_FORMAT_S32_LE);

	rate->min = rate->max = SAMPLING_RATE_8KHZ;
	channels->min = channels->max = msm_vi_feed_tx_ch;
	pr_debug("%s: msm_vi_feed_tx_ch: %d\n", __func__, msm_vi_feed_tx_ch);

	return 0;
}

static int msm_slim_5_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	int rc = 0;
	void *config = NULL;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s: enter\n", __func__);
	rate->min = rate->max = SAMPLING_RATE_16KHZ;
	channels->min = channels->max = 1;

	config = msm8996_codec_fn.get_afe_config_fn(codec,
				AFE_SLIMBUS_SLAVE_PORT_CONFIG);
	if (config) {
		rc = afe_set_config(AFE_SLIMBUS_SLAVE_PORT_CONFIG, config,
				    SLIMBUS_5_TX);
		if (rc) {
			pr_err("%s: Failed to set slimbus slave port config %d\n",
				__func__, rc);
		}
	}

	return rc;
}

static int msm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				  struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s: format = %d, rate = %d\n",
			__func__, params_format(params), params_rate(params));

	switch (dai_link->be_id) {
	case MSM_BACKEND_DAI_USB_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				usb_rx_cfg.bit_format);
		rate->min = rate->max = usb_rx_cfg.sample_rate;
		channels->min = channels->max = usb_rx_cfg.channels;
		break;

	case MSM_BACKEND_DAI_USB_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				usb_tx_cfg.bit_format);
		rate->min = rate->max = usb_tx_cfg.sample_rate;
		channels->min = channels->max = usb_tx_cfg.channels;
		break;

	default:
		rate->min = rate->max = SAMPLING_RATE_48KHZ;
		break;
	}
	return 0;
}
static int msm_tdm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_interval *rate = hw_param_interval(params,
				   SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
				   SNDRV_PCM_HW_PARAM_CHANNELS);

	rate->min = rate->max = SAMPLING_RATE_48KHZ;

	switch (cpu_dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		channels->min = channels->max = msm_pri_tdm_tx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_pri_tdm_tx_0_bit_format);
		rate->min = rate->max = msm_pri_tdm_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
		channels->min = channels->max = msm_pri_tdm_tx_1_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
		msm_pri_tdm_tx_1_bit_format);
		rate->min = rate->max = msm_pri_tdm_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
		channels->min = channels->max = msm_pri_tdm_tx_2_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_pri_tdm_tx_2_bit_format);
		rate->min = rate->max = msm_pri_tdm_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
		channels->min = channels->max = msm_pri_tdm_tx_3_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_pri_tdm_tx_3_bit_format);
		rate->min = rate->max = msm_pri_tdm_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX:
		channels->min = channels->max = msm_pri_tdm_rx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
		msm_pri_tdm_rx_0_bit_format);
		rate->min = rate->max = msm_pri_tdm_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
		channels->min = channels->max = msm_pri_tdm_rx_1_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_pri_tdm_rx_1_bit_format);
		rate->min = rate->max = msm_pri_tdm_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
		channels->min = channels->max = msm_pri_tdm_rx_2_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_pri_tdm_rx_2_bit_format);
		rate->min = rate->max = msm_pri_tdm_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
		channels->min = channels->max = msm_pri_tdm_rx_3_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_pri_tdm_rx_3_bit_format);
		rate->min = rate->max = msm_pri_tdm_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
		channels->min = channels->max = msm_sec_tdm_rx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_sec_tdm_rx_0_bit_format);
		rate->min = rate->max = msm_sec_tdm_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_1:
		channels->min = channels->max = msm_sec_tdm_rx_1_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_sec_tdm_rx_1_bit_format);
		rate->min = rate->max = msm_sec_tdm_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_2:
		channels->min = channels->max = msm_sec_tdm_rx_2_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_sec_tdm_rx_2_bit_format);
		rate->min = rate->max = msm_sec_tdm_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_3:
		channels->min = channels->max = msm_sec_tdm_rx_3_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_sec_tdm_rx_3_bit_format);
		rate->min = rate->max = msm_sec_tdm_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX:
		channels->min = channels->max = msm_sec_tdm_tx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_sec_tdm_tx_0_bit_format);
		rate->min = rate->max = msm_sec_tdm_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_1:
		channels->min = channels->max = msm_sec_tdm_tx_1_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_sec_tdm_tx_1_bit_format);
		rate->min = rate->max = msm_sec_tdm_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_2:
		channels->min = channels->max = msm_sec_tdm_tx_2_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_sec_tdm_tx_2_bit_format);
		rate->min = rate->max = msm_sec_tdm_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_3:
		channels->min = channels->max = msm_sec_tdm_tx_3_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_sec_tdm_tx_3_bit_format);
		rate->min = rate->max = msm_sec_tdm_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX:
		channels->min = channels->max = msm_tert_tdm_rx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_tert_tdm_rx_0_bit_format);
		rate->min = rate->max = msm_tert_tdm_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_1:
		channels->min = channels->max = msm_tert_tdm_rx_1_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_tert_tdm_rx_1_bit_format);
		rate->min = rate->max = msm_tert_tdm_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_2:
		channels->min = channels->max = msm_tert_tdm_rx_2_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_tert_tdm_rx_2_bit_format);
		rate->min = rate->max = msm_tert_tdm_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_3:
		channels->min = channels->max = msm_tert_tdm_rx_3_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_tert_tdm_rx_3_bit_format);
		rate->min = rate->max = msm_tert_tdm_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX:
		channels->min = channels->max = msm_tert_tdm_tx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_tert_tdm_tx_0_bit_format);
		rate->min = rate->max = msm_tert_tdm_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_1:
		channels->min = channels->max = msm_tert_tdm_tx_1_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_tert_tdm_tx_1_bit_format);
		rate->min = rate->max = msm_tert_tdm_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_2:
		channels->min = channels->max = msm_tert_tdm_tx_2_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_tert_tdm_tx_2_bit_format);
		rate->min = rate->max = msm_tert_tdm_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_3:
		channels->min = channels->max = msm_tert_tdm_tx_3_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_tert_tdm_tx_3_bit_format);
		rate->min = rate->max = msm_tert_tdm_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
		channels->min = channels->max = msm_quat_tdm_rx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_quat_tdm_rx_0_bit_format);
		rate->min = rate->max = msm_quat_tdm_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_1:
		channels->min = channels->max = msm_quat_tdm_rx_1_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_quat_tdm_rx_1_bit_format);
		rate->min = rate->max = msm_quat_tdm_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_2:
		channels->min = channels->max = msm_quat_tdm_rx_2_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_quat_tdm_rx_2_bit_format);
		rate->min = rate->max = msm_quat_tdm_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_3:
		channels->min = channels->max = msm_quat_tdm_rx_3_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_quat_tdm_rx_3_bit_format);
		rate->min = rate->max = msm_quat_tdm_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
		channels->min = channels->max = msm_quat_tdm_tx_0_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_quat_tdm_tx_0_bit_format);
		rate->min = rate->max = msm_quat_tdm_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_1:
		channels->min = channels->max = msm_quat_tdm_tx_1_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_quat_tdm_tx_1_bit_format);
		rate->min = rate->max = msm_quat_tdm_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_2:
		channels->min = channels->max = msm_quat_tdm_tx_2_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
		msm_quat_tdm_tx_2_bit_format);
		rate->min = rate->max = msm_quat_tdm_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_3:
		channels->min = channels->max = msm_quat_tdm_tx_3_ch;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				msm_quat_tdm_tx_3_bit_format);
		rate->min = rate->max = msm_quat_tdm_rate;
		break;
	default:
		pr_err("%s: dai id 0x%x not supported\n",
				__func__, cpu_dai->id);
	return -EINVAL;
	}

	pr_debug("%s: dai id = 0x%x channels = %d rate = %d format = 0x%x\n",
			__func__, cpu_dai->id, channels->max, rate->max,
			params_format(params));

	return 0;
}

static unsigned int tdm_param_set_slot_mask(int slots)
{
	unsigned int slot_mask = 0;
	unsigned int i = 0;

	if ((slots != 16) && (slots != 8)) {
		pr_err("%s: invalid slot number %d\n", __func__, slots);
		return -EINVAL;
	}

	for (i = 0; i < slots ; i++)
		slot_mask |= 1 << i;
	return slot_mask;
}

static int msm8996_tdm_snd_hw_params(struct snd_pcm_substream *substream,
					struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	int channels, slot_width, slots, rate;
	unsigned int slot_mask;
	unsigned int *slot_offset;
	int offset_channels = 0;
	int i;
	int clk_freq;

	pr_debug("%s: dai id = 0x%x\n", __func__, cpu_dai->id);

	rate = params_rate(params);
	channels = params_channels(params);
	if (channels < 1 || channels > 8) {
		pr_err("%s: invalid param channels %d\n",
			__func__, channels);
		return -EINVAL;
	}

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		/*
		* up to 8 channel HW configuration should
		* use 32 bit slot width for max support of
		* stream bit width. (slot_width > bit_width)
		*/
		slot_width = msm_tdm_slot_width;
		break;
	default:
		pr_err("%s: invalid param format 0x%x\n",
			__func__, params_format(params));
		return -EINVAL;
	}

	slots = msm_tdm_num_slots;

	switch (cpu_dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
		slots = msm_pri_tdm_slot_num;
		slot_width = msm_pri_tdm_slot_width;
		slot_offset = tdm_slot_offset[PRIMARY_TDM_RX_0];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
		slots = msm_pri_tdm_slot_num;
		slot_width = msm_pri_tdm_slot_width;
		slot_offset = tdm_slot_offset[PRIMARY_TDM_RX_1];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
		slots = msm_pri_tdm_slot_num;
		slot_width = msm_pri_tdm_slot_width;
		slot_offset = tdm_slot_offset[PRIMARY_TDM_RX_2];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
		slots = msm_pri_tdm_slot_num;
		slot_width = msm_pri_tdm_slot_width;
		slot_offset = tdm_slot_offset[PRIMARY_TDM_RX_3];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_4:
		slots = msm_pri_tdm_slot_num;
		slot_width = msm_pri_tdm_slot_width;
		slot_offset = tdm_slot_offset[PRIMARY_TDM_RX_4];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_5:
		slots = msm_pri_tdm_slot_num;
		slot_width = msm_pri_tdm_slot_width;
		slot_offset = tdm_slot_offset[PRIMARY_TDM_RX_5];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_6:
		slots = msm_pri_tdm_slot_num;
		slot_width = msm_pri_tdm_slot_width;
		slot_offset = tdm_slot_offset[PRIMARY_TDM_RX_6];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_7:
		slots = msm_pri_tdm_slot_num;
		slot_width = msm_pri_tdm_slot_width;
		slot_offset = tdm_slot_offset[PRIMARY_TDM_RX_7];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		slots = msm_pri_tdm_slot_num;
		slot_width = msm_pri_tdm_slot_width;
		slot_offset = tdm_slot_offset[PRIMARY_TDM_TX_0];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
		slots = msm_pri_tdm_slot_num;
		slot_width = msm_pri_tdm_slot_width;
		slot_offset = tdm_slot_offset[PRIMARY_TDM_TX_1];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
		slots = msm_pri_tdm_slot_num;
		slot_width = msm_pri_tdm_slot_width;
		slot_offset = tdm_slot_offset[PRIMARY_TDM_TX_2];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
		slots = msm_pri_tdm_slot_num;
		slot_width = msm_pri_tdm_slot_width;
		slot_offset = tdm_slot_offset[PRIMARY_TDM_TX_3];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_4:
		slots = msm_pri_tdm_slot_num;
		slot_width = msm_pri_tdm_slot_width;
		slot_offset = tdm_slot_offset[PRIMARY_TDM_TX_4];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_5:
		slots = msm_pri_tdm_slot_num;
		slot_width = msm_pri_tdm_slot_width;
		slot_offset = tdm_slot_offset[PRIMARY_TDM_TX_5];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_6:
		slots = msm_pri_tdm_slot_num;
		slot_width = msm_pri_tdm_slot_width;
		slot_offset = tdm_slot_offset[PRIMARY_TDM_TX_6];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_7:
		slots = msm_pri_tdm_slot_num;
		slot_width = msm_pri_tdm_slot_width;
		slot_offset = tdm_slot_offset[PRIMARY_TDM_TX_7];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
		slots = msm_sec_tdm_slot_num;
		slot_width = msm_sec_tdm_slot_width;
		slot_offset = tdm_slot_offset[SECONDARY_TDM_RX_0];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_1:
		slots = msm_sec_tdm_slot_num;
		slot_width = msm_sec_tdm_slot_width;
		slot_offset = tdm_slot_offset[SECONDARY_TDM_RX_1];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_2:
		slots = msm_sec_tdm_slot_num;
		slot_width = msm_sec_tdm_slot_width;
		slot_offset = tdm_slot_offset[SECONDARY_TDM_RX_2];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_3:
		slots = msm_sec_tdm_slot_num;
		slot_width = msm_sec_tdm_slot_width;
		slot_offset = tdm_slot_offset[SECONDARY_TDM_RX_3];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_4:
		slots = msm_sec_tdm_slot_num;
		slot_width = msm_sec_tdm_slot_width;
		slot_offset = tdm_slot_offset[SECONDARY_TDM_RX_4];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_5:
		slots = msm_sec_tdm_slot_num;
		slot_width = msm_sec_tdm_slot_width;
		slot_offset = tdm_slot_offset[SECONDARY_TDM_RX_5];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_6:
		slots = msm_sec_tdm_slot_num;
		slot_width = msm_sec_tdm_slot_width;
		slot_offset = tdm_slot_offset[SECONDARY_TDM_RX_6];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_7:
		slots = msm_sec_tdm_slot_num;
		slot_width = msm_sec_tdm_slot_width;
		slot_offset = tdm_slot_offset[SECONDARY_TDM_RX_7];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX:
		slots = msm_sec_tdm_slot_num;
		slot_width = msm_sec_tdm_slot_width;
		slot_offset = tdm_slot_offset[SECONDARY_TDM_TX_0];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_1:
		slots = msm_sec_tdm_slot_num;
		slot_width = msm_sec_tdm_slot_width;
		slot_offset = tdm_slot_offset[SECONDARY_TDM_TX_1];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_2:
		slots = msm_sec_tdm_slot_num;
		slot_width = msm_sec_tdm_slot_width;
		slot_offset = tdm_slot_offset[SECONDARY_TDM_TX_2];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_3:
		slots = msm_sec_tdm_slot_num;
		slot_width = msm_sec_tdm_slot_width;
		slot_offset = tdm_slot_offset[SECONDARY_TDM_TX_3];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_4:
		slots = msm_sec_tdm_slot_num;
		slot_width = msm_sec_tdm_slot_width;
		slot_offset = tdm_slot_offset[SECONDARY_TDM_TX_4];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_5:
		slots = msm_sec_tdm_slot_num;
		slot_width = msm_sec_tdm_slot_width;
		slot_offset = tdm_slot_offset[SECONDARY_TDM_TX_5];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_6:
		slots = msm_sec_tdm_slot_num;
		slot_width = msm_sec_tdm_slot_width;
		slot_offset = tdm_slot_offset[SECONDARY_TDM_TX_6];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_7:
		slots = msm_sec_tdm_slot_num;
		slot_width = msm_sec_tdm_slot_width;
		slot_offset = tdm_slot_offset[SECONDARY_TDM_TX_7];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX:
		slots = msm_tert_tdm_slot_num;
		slot_width = msm_tert_tdm_slot_width;
		slot_offset = tdm_slot_offset[TERTIARY_TDM_RX_0];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_1:
		slots = msm_tert_tdm_slot_num;
		slot_width = msm_tert_tdm_slot_width;
		slot_offset = tdm_slot_offset[TERTIARY_TDM_RX_1];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_2:
		slots = msm_tert_tdm_slot_num;
		slot_width = msm_tert_tdm_slot_width;
		slot_offset = tdm_slot_offset[TERTIARY_TDM_RX_2];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_3:
		slots = msm_tert_tdm_slot_num;
		slot_width = msm_tert_tdm_slot_width;
		slot_offset = tdm_slot_offset[TERTIARY_TDM_RX_3];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_4:
		slots = msm_tert_tdm_slot_num;
		slot_width = msm_tert_tdm_slot_width;
		slot_offset = tdm_slot_offset[TERTIARY_TDM_RX_4];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_5:
		slots = msm_tert_tdm_slot_num;
		slot_width = msm_tert_tdm_slot_width;
		slot_offset = tdm_slot_offset[TERTIARY_TDM_RX_5];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_6:
		slots = msm_tert_tdm_slot_num;
		slot_width = msm_tert_tdm_slot_width;
		slot_offset = tdm_slot_offset[TERTIARY_TDM_RX_6];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_7:
		slots = msm_tert_tdm_slot_num;
		slot_width = msm_tert_tdm_slot_width;
		slot_offset = tdm_slot_offset[TERTIARY_TDM_RX_7];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX:
		slots = msm_tert_tdm_slot_num;
		slot_width = msm_tert_tdm_slot_width;
		slot_offset = tdm_slot_offset[TERTIARY_TDM_TX_0];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_1:
		slots = msm_tert_tdm_slot_num;
		slot_width = msm_tert_tdm_slot_width;
		slot_offset = tdm_slot_offset[TERTIARY_TDM_TX_1];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_2:
		slots = msm_tert_tdm_slot_num;
		slot_width = msm_tert_tdm_slot_width;
		slot_offset = tdm_slot_offset[TERTIARY_TDM_TX_2];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_3:
		slots = msm_tert_tdm_slot_num;
		slot_width = msm_tert_tdm_slot_width;
		slot_offset = tdm_slot_offset[TERTIARY_TDM_TX_3];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_4:
		slots = msm_tert_tdm_slot_num;
		slot_width = msm_tert_tdm_slot_width;
		slot_offset = tdm_slot_offset[TERTIARY_TDM_TX_4];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_5:
		slots = msm_tert_tdm_slot_num;
		slot_width = msm_tert_tdm_slot_width;
		slot_offset = tdm_slot_offset[TERTIARY_TDM_TX_5];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_6:
		slots = msm_tert_tdm_slot_num;
		slot_width = msm_tert_tdm_slot_width;
		slot_offset = tdm_slot_offset[TERTIARY_TDM_TX_6];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_7:
		slots = msm_tert_tdm_slot_num;
		slot_width = msm_tert_tdm_slot_width;
		slot_offset = tdm_slot_offset[TERTIARY_TDM_TX_7];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
		slots = msm_quat_tdm_slot_num;
		slot_width = msm_quat_tdm_slot_width;
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_RX_0];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_1:
		slots = msm_quat_tdm_slot_num;
		slot_width = msm_quat_tdm_slot_width;
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_RX_1];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_2:
		slots = msm_quat_tdm_slot_num;
		slot_width = msm_quat_tdm_slot_width;
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_RX_2];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_3:
		slots = msm_quat_tdm_slot_num;
		slot_width = msm_quat_tdm_slot_width;
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_RX_3];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_4:
		slots = msm_quat_tdm_slot_num;
		slot_width = msm_quat_tdm_slot_width;
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_RX_4];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_5:
		slots = msm_quat_tdm_slot_num;
		slot_width = msm_quat_tdm_slot_width;
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_RX_5];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_6:
		slots = msm_quat_tdm_slot_num;
		slot_width = msm_quat_tdm_slot_width;
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_RX_6];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_7:
		slots = msm_quat_tdm_slot_num;
		slot_width = msm_quat_tdm_slot_width;
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_RX_7];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
		slots = msm_quat_tdm_slot_num;
		slot_width = msm_quat_tdm_slot_width;
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_TX_0];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_1:
		slots = msm_quat_tdm_slot_num;
		slot_width = msm_quat_tdm_slot_width;
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_TX_1];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_2:
		slots = msm_quat_tdm_slot_num;
		slot_width = msm_quat_tdm_slot_width;
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_TX_2];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_3:
		slots = msm_quat_tdm_slot_num;
		slot_width = msm_quat_tdm_slot_width;
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_TX_3];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_4:
		slots = msm_quat_tdm_slot_num;
		slot_width = msm_quat_tdm_slot_width;
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_TX_4];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_5:
		slots = msm_quat_tdm_slot_num;
		slot_width = msm_quat_tdm_slot_width;
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_TX_5];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_6:
		slots = msm_quat_tdm_slot_num;
		slot_width = msm_quat_tdm_slot_width;
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_TX_6];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_7:
		slots = msm_quat_tdm_slot_num;
		slot_width = msm_quat_tdm_slot_width;
		slot_offset = tdm_slot_offset[QUATERNARY_TDM_TX_7];
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

	slot_mask = tdm_param_set_slot_mask(slots);
	if (!slot_mask) {
		pr_err("%s: invalid slot_mask 0x%x\n",
			__func__, slot_mask);
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

	clk_freq = rate * slot_width * slots;
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, clk_freq, SND_SOC_CLOCK_OUT);
	if (ret < 0) {
		pr_err("%s: failed to set tdm clk, err:%d\n",
				__func__, ret);
	}

end:
	return ret;
}

static struct snd_soc_ops msm8996_tdm_be_ops = {
	.hw_params = msm8996_tdm_snd_hw_params,
};


static const struct soc_enum msm_snd_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, spk_function),
	SOC_ENUM_SINGLE_EXT(2, slim0_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(8, slim0_tx_ch_text),
	SOC_ENUM_SINGLE_EXT(7, hdmi_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rx_bit_format_text),
			    rx_bit_format_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(slim0_rx_sample_rate_text),
			    slim0_rx_sample_rate_text),
	SOC_ENUM_SINGLE_EXT(8, proxy_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(3, hdmi_rx_sample_rate_text),
	SOC_ENUM_SINGLE_EXT(4, slim5_rx_sample_rate_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(slim5_rx_bit_format_text),
			    slim5_rx_bit_format_text),
	SOC_ENUM_SINGLE_EXT(2, slim5_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(2, hifi_function),
	SOC_ENUM_SINGLE_EXT(2, vi_feed_ch_text),
	SOC_ENUM_SINGLE_EXT(4, slim6_rx_sample_rate_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(slim6_rx_bit_format_text),
			    slim6_rx_bit_format_text),
	SOC_ENUM_SINGLE_EXT(2, slim6_rx_ch_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_ch_text), tdm_ch_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_bit_format_text),
				tdm_bit_format_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_rate_text), tdm_rate_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_slot_num_text),
				tdm_slot_num_text),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tdm_slot_width_text),
				tdm_slot_width_text),
};

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("Speaker Function", msm_snd_enum[0], msm8996_get_spk,
			msm8996_set_spk),
	SOC_ENUM_EXT("SLIM_0_RX Channels", msm_snd_enum[1],
			msm_slim_0_rx_ch_get, msm_slim_0_rx_ch_put),
	SOC_ENUM_EXT("SLIM_5_RX Channels", msm_snd_enum[10],
			msm_slim_5_rx_ch_get, msm_slim_5_rx_ch_put),
	SOC_ENUM_EXT("SLIM_6_RX Channels", msm_snd_enum[15],
			msm_slim_6_rx_ch_get, msm_slim_6_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_TX Channels", msm_snd_enum[2],
			msm_slim_0_tx_ch_get, msm_slim_0_tx_ch_put),
	SOC_ENUM_EXT("SLIM_1_TX Channels", msm_snd_enum[2],
			msm_slim_1_tx_ch_get, msm_slim_1_tx_ch_put),
	SOC_ENUM_EXT("AUX PCM SampleRate", msm8996_auxpcm_enum[0],
			msm8996_auxpcm_rate_get,
			msm8996_auxpcm_rate_put),
	SOC_ENUM_EXT("HDMI_RX Channels", msm_snd_enum[3],
			msm_hdmi_rx_ch_get, msm_hdmi_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_RX Format", msm_snd_enum[4],
			slim0_rx_bit_format_get, slim0_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_5_RX Format", msm_snd_enum[9],
			slim5_rx_bit_format_get, slim5_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_6_RX Format", msm_snd_enum[14],
			slim6_rx_bit_format_get, slim6_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_0_RX SampleRate", msm_snd_enum[5],
			slim0_rx_sample_rate_get, slim0_rx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_5_RX SampleRate", msm_snd_enum[8],
			slim5_rx_sample_rate_get, slim5_rx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_6_RX SampleRate", msm_snd_enum[13],
			slim6_rx_sample_rate_get, slim6_rx_sample_rate_put),
	SOC_ENUM_EXT("HDMI_RX Bit Format", msm_snd_enum[4],
			hdmi_rx_bit_format_get, hdmi_rx_bit_format_put),
	SOC_ENUM_EXT("PROXY_RX Channels", msm_snd_enum[6],
			msm_proxy_rx_ch_get, msm_proxy_rx_ch_put),
	SOC_ENUM_EXT("HDMI_RX SampleRate", msm_snd_enum[7],
			hdmi_rx_sample_rate_get, hdmi_rx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_0_TX SampleRate", msm_snd_enum[5],
			slim0_tx_sample_rate_get, slim0_tx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_0_TX Format", msm_snd_enum[4],
			slim0_tx_bit_format_get, slim0_tx_bit_format_put),
	SOC_ENUM_EXT("HiFi Function", msm_snd_enum[11], msm8996_hifi_get,
			msm8996_hifi_put),
	SOC_ENUM_EXT("VI_FEED_TX Channels", msm_snd_enum[12],
			msm_vi_feed_tx_ch_get, msm_vi_feed_tx_ch_put),
	SOC_ENUM_EXT("USB_AUDIO_RX Channels", usb_rx_chs,
			usb_audio_rx_ch_get, usb_audio_rx_ch_put),
	SOC_ENUM_EXT("USB_AUDIO_TX Channels", usb_tx_chs,
			usb_audio_tx_ch_get, usb_audio_tx_ch_put),
	SOC_ENUM_EXT("USB_AUDIO_RX SampleRate", usb_rx_sample_rate,
			usb_audio_rx_sample_rate_get,
			usb_audio_rx_sample_rate_put),
	SOC_ENUM_EXT("USB_AUDIO_TX SampleRate", usb_tx_sample_rate,
			usb_audio_tx_sample_rate_get,
			usb_audio_tx_sample_rate_put),
	SOC_ENUM_EXT("USB_AUDIO_RX Format", usb_rx_format,
			usb_audio_rx_format_get, usb_audio_rx_format_put),
	SOC_ENUM_EXT("USB_AUDIO_TX Format", usb_tx_format,
			usb_audio_tx_format_get, usb_audio_tx_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Channels", msm_snd_enum[16],
			msm_pri_tdm_tx_0_ch_get, msm_pri_tdm_tx_0_ch_put),
	SOC_ENUM_EXT("PRI_TDM_TX_1 Channels", msm_snd_enum[16],
			msm_pri_tdm_tx_1_ch_get, msm_pri_tdm_tx_1_ch_put),
	SOC_ENUM_EXT("PRI_TDM_TX_2 Channels", msm_snd_enum[16],
			msm_pri_tdm_tx_2_ch_get, msm_pri_tdm_tx_2_ch_put),
	SOC_ENUM_EXT("PRI_TDM_TX_3 Channels", msm_snd_enum[16],
			msm_pri_tdm_tx_3_ch_get, msm_pri_tdm_tx_3_ch_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 Channels", msm_snd_enum[16],
			msm_pri_tdm_rx_0_ch_get, msm_pri_tdm_rx_0_ch_put),
	SOC_ENUM_EXT("PRI_TDM_RX_1 Channels", msm_snd_enum[16],
			msm_pri_tdm_rx_1_ch_get, msm_pri_tdm_rx_1_ch_put),
	SOC_ENUM_EXT("PRI_TDM_RX_2 Channels", msm_snd_enum[16],
			msm_pri_tdm_rx_2_ch_get, msm_pri_tdm_rx_2_ch_put),
	SOC_ENUM_EXT("PRI_TDM_RX_3 Channels", msm_snd_enum[16],
			msm_pri_tdm_rx_3_ch_get, msm_pri_tdm_rx_3_ch_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 Channels", msm_snd_enum[16],
			msm_sec_tdm_rx_0_ch_get, msm_sec_tdm_rx_0_ch_put),
	SOC_ENUM_EXT("SEC_TDM_RX_1 Channels", msm_snd_enum[16],
			msm_sec_tdm_rx_1_ch_get, msm_sec_tdm_rx_1_ch_put),
	SOC_ENUM_EXT("SEC_TDM_RX_2 Channels", msm_snd_enum[16],
			msm_sec_tdm_rx_2_ch_get, msm_sec_tdm_rx_2_ch_put),
	SOC_ENUM_EXT("SEC_TDM_RX_3 Channels", msm_snd_enum[16],
			msm_sec_tdm_rx_3_ch_get, msm_sec_tdm_rx_3_ch_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 Channels", msm_snd_enum[16],
			msm_sec_tdm_tx_0_ch_get, msm_sec_tdm_tx_0_ch_put),
	SOC_ENUM_EXT("SEC_TDM_TX_1 Channels", msm_snd_enum[16],
			msm_sec_tdm_tx_1_ch_get, msm_sec_tdm_tx_1_ch_put),
	SOC_ENUM_EXT("SEC_TDM_TX_2 Channels", msm_snd_enum[16],
			msm_sec_tdm_tx_2_ch_get, msm_sec_tdm_tx_2_ch_put),
	SOC_ENUM_EXT("SEC_TDM_TX_3 Channels", msm_snd_enum[16],
			msm_sec_tdm_tx_3_ch_get, msm_sec_tdm_tx_3_ch_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 Channels", msm_snd_enum[16],
			msm_tert_tdm_rx_0_ch_get, msm_tert_tdm_rx_0_ch_put),
	SOC_ENUM_EXT("TERT_TDM_RX_1 Channels", msm_snd_enum[16],
			msm_tert_tdm_rx_1_ch_get, msm_tert_tdm_rx_1_ch_put),
	SOC_ENUM_EXT("TERT_TDM_RX_2 Channels", msm_snd_enum[16],
			msm_tert_tdm_rx_2_ch_get, msm_tert_tdm_rx_2_ch_put),
	SOC_ENUM_EXT("TERT_TDM_RX_3 Channels", msm_snd_enum[16],
			msm_tert_tdm_rx_3_ch_get, msm_tert_tdm_rx_3_ch_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 Channels", msm_snd_enum[16],
			msm_tert_tdm_tx_0_ch_get, msm_tert_tdm_tx_0_ch_put),
	SOC_ENUM_EXT("TERT_TDM_TX_1 Channels", msm_snd_enum[16],
			msm_tert_tdm_tx_1_ch_get, msm_tert_tdm_tx_1_ch_put),
	SOC_ENUM_EXT("TERT_TDM_TX_2 Channels", msm_snd_enum[16],
			msm_tert_tdm_tx_2_ch_get, msm_tert_tdm_tx_2_ch_put),
	SOC_ENUM_EXT("TERT_TDM_TX_3 Channels", msm_snd_enum[16],
			msm_tert_tdm_tx_3_ch_get, msm_tert_tdm_tx_3_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 Channels", msm_snd_enum[16],
			msm_quat_tdm_rx_0_ch_get, msm_quat_tdm_rx_0_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_1 Channels", msm_snd_enum[16],
			msm_quat_tdm_rx_1_ch_get, msm_quat_tdm_rx_1_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_2 Channels", msm_snd_enum[16],
			msm_quat_tdm_rx_2_ch_get, msm_quat_tdm_rx_2_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_3 Channels", msm_snd_enum[16],
			msm_quat_tdm_rx_3_ch_get, msm_quat_tdm_rx_3_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 Channels", msm_snd_enum[16],
			msm_quat_tdm_tx_0_ch_get, msm_quat_tdm_tx_0_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_1 Channels", msm_snd_enum[16],
			msm_quat_tdm_tx_1_ch_get, msm_quat_tdm_tx_1_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_2 Channels", msm_snd_enum[16],
			msm_quat_tdm_tx_2_ch_get, msm_quat_tdm_tx_2_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_3 Channels", msm_snd_enum[16],
			msm_quat_tdm_tx_3_ch_get, msm_quat_tdm_tx_3_ch_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Bit Format", msm_snd_enum[17],
			msm_pri_tdm_tx_0_bit_format_get,
			msm_pri_tdm_tx_0_bit_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_1 Bit Format", msm_snd_enum[17],
			msm_pri_tdm_tx_1_bit_format_get,
			msm_pri_tdm_tx_1_bit_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_2 Bit Format", msm_snd_enum[17],
			msm_pri_tdm_tx_2_bit_format_get,
			msm_pri_tdm_tx_2_bit_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_3 Bit Format", msm_snd_enum[17],
			msm_pri_tdm_tx_3_bit_format_get,
			msm_pri_tdm_tx_3_bit_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 Bit Format", msm_snd_enum[17],
			msm_pri_tdm_rx_0_bit_format_get,
			msm_pri_tdm_rx_0_bit_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_1 Bit Format", msm_snd_enum[17],
			msm_pri_tdm_rx_1_bit_format_get,
			msm_pri_tdm_rx_1_bit_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_2 Bit Format", msm_snd_enum[17],
			msm_pri_tdm_rx_2_bit_format_get,
			msm_pri_tdm_rx_2_bit_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_3 Bit Format", msm_snd_enum[17],
			msm_pri_tdm_rx_3_bit_format_get,
			msm_pri_tdm_rx_3_bit_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 Bit Format", msm_snd_enum[17],
			msm_sec_tdm_rx_0_bit_format_get,
			msm_sec_tdm_rx_0_bit_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_1 Bit Format", msm_snd_enum[17],
			msm_sec_tdm_rx_1_bit_format_get,
			msm_sec_tdm_rx_1_bit_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_2 Bit Format", msm_snd_enum[17],
			msm_sec_tdm_rx_2_bit_format_get,
			msm_sec_tdm_rx_2_bit_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_3 Bit Format", msm_snd_enum[17],
			msm_sec_tdm_rx_3_bit_format_get,
			msm_sec_tdm_rx_3_bit_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 Bit Format", msm_snd_enum[17],
			msm_sec_tdm_tx_0_bit_format_get,
			msm_sec_tdm_tx_0_bit_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_1 Bit Format", msm_snd_enum[17],
			msm_sec_tdm_tx_1_bit_format_get,
			msm_sec_tdm_tx_1_bit_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_2 Bit Format", msm_snd_enum[17],
			msm_sec_tdm_tx_2_bit_format_get,
			msm_sec_tdm_tx_2_bit_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_3 Bit Format", msm_snd_enum[17],
			msm_sec_tdm_tx_3_bit_format_get,
			msm_sec_tdm_tx_3_bit_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 Bit Format", msm_snd_enum[17],
			msm_tert_tdm_rx_0_bit_format_get,
			msm_tert_tdm_rx_0_bit_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_1 Bit Format", msm_snd_enum[17],
			msm_tert_tdm_rx_1_bit_format_get,
			msm_tert_tdm_rx_1_bit_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_2 Bit Format", msm_snd_enum[17],
			msm_tert_tdm_rx_2_bit_format_get,
			msm_tert_tdm_rx_2_bit_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_3 Bit Format", msm_snd_enum[17],
			msm_tert_tdm_rx_3_bit_format_get,
			msm_tert_tdm_rx_3_bit_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 Bit Format", msm_snd_enum[17],
			msm_tert_tdm_tx_0_bit_format_get,
			msm_tert_tdm_tx_0_bit_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_1 Bit Format", msm_snd_enum[17],
			msm_tert_tdm_tx_1_bit_format_get,
			msm_tert_tdm_tx_1_bit_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_2 Bit Format", msm_snd_enum[17],
			msm_tert_tdm_tx_2_bit_format_get,
			msm_tert_tdm_tx_2_bit_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_3 Bit Format", msm_snd_enum[17],
			msm_tert_tdm_tx_3_bit_format_get,
			msm_tert_tdm_tx_3_bit_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 Bit Format", msm_snd_enum[17],
			msm_quat_tdm_rx_0_bit_format_get,
			msm_quat_tdm_rx_0_bit_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_1 Bit Format", msm_snd_enum[17],
			msm_quat_tdm_rx_1_bit_format_get,
			msm_quat_tdm_rx_1_bit_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_2 Bit Format", msm_snd_enum[17],
			msm_quat_tdm_rx_2_bit_format_get,
			msm_quat_tdm_rx_2_bit_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_3 Bit Format", msm_snd_enum[17],
			msm_quat_tdm_rx_3_bit_format_get,
			msm_quat_tdm_rx_3_bit_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 Bit Format", msm_snd_enum[17],
			msm_quat_tdm_tx_0_bit_format_get,
			msm_quat_tdm_tx_0_bit_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_1 Bit Format", msm_snd_enum[17],
			msm_quat_tdm_tx_1_bit_format_get,
			msm_quat_tdm_tx_1_bit_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_2 Bit Format", msm_snd_enum[17],
			msm_quat_tdm_tx_2_bit_format_get,
			msm_quat_tdm_tx_2_bit_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_3 Bit Format", msm_snd_enum[17],
			msm_quat_tdm_tx_3_bit_format_get,
			msm_quat_tdm_tx_3_bit_format_put),
	SOC_ENUM_EXT("PRI_TDM SampleRate", msm_snd_enum[18],
			msm_pri_tdm_rate_get, msm_pri_tdm_rate_put),
	SOC_ENUM_EXT("PRI_TDM Slot Number", msm_snd_enum[19],
			msm_pri_tdm_slot_num_get, msm_pri_tdm_slot_num_put),
	SOC_ENUM_EXT("PRI_TDM Slot Width", msm_snd_enum[20],
			msm_pri_tdm_slot_width_get, msm_pri_tdm_slot_width_put),
	SOC_ENUM_EXT("SEC_TDM SampleRate", msm_snd_enum[18],
			msm_sec_tdm_rate_get, msm_sec_tdm_rate_put),
	SOC_ENUM_EXT("SEC_TDM Slot Number", msm_snd_enum[19],
			msm_sec_tdm_slot_num_get, msm_sec_tdm_slot_num_put),
	SOC_ENUM_EXT("SEC_TDM Slot Width", msm_snd_enum[20],
			msm_sec_tdm_slot_width_get, msm_sec_tdm_slot_width_put),
	SOC_ENUM_EXT("TERT_TDM SampleRate", msm_snd_enum[18],
			msm_tert_tdm_rate_get, msm_tert_tdm_rate_put),
	SOC_ENUM_EXT("TERT_TDM Slot Number", msm_snd_enum[19],
			msm_tert_tdm_slot_num_get, msm_tert_tdm_slot_num_put),
	SOC_ENUM_EXT("TERT_TDM Slot Width", msm_snd_enum[20],
			msm_tert_tdm_slot_width_get,
			msm_tert_tdm_slot_width_put),
	SOC_ENUM_EXT("QUAT_TDM SampleRate", msm_snd_enum[18],
			msm_quat_tdm_rate_get, msm_quat_tdm_rate_put),
	SOC_ENUM_EXT("QUAT_TDM Slot Number", msm_snd_enum[19],
			msm_quat_tdm_slot_num_get, msm_quat_tdm_slot_num_put),
	SOC_ENUM_EXT("QUAT_TDM Slot Width", msm_snd_enum[20],
			msm_quat_tdm_slot_width_get,
			msm_quat_tdm_slot_width_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_0 Slot Mapping", SND_SOC_NOPM,
			PRIMARY_TDM_RX_0, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_1 Slot Mapping", SND_SOC_NOPM,
			PRIMARY_TDM_RX_1, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_2 Slot Mapping", SND_SOC_NOPM,
			PRIMARY_TDM_RX_2, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_3 Slot Mapping", SND_SOC_NOPM,
			PRIMARY_TDM_RX_3, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_4 Slot Mapping", SND_SOC_NOPM,
			PRIMARY_TDM_RX_4, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_5 Slot Mapping", SND_SOC_NOPM,
			PRIMARY_TDM_RX_5, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_6 Slot Mapping", SND_SOC_NOPM,
			PRIMARY_TDM_RX_6, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_7 Slot Mapping", SND_SOC_NOPM,
			PRIMARY_TDM_RX_7, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_0 Slot Mapping", SND_SOC_NOPM,
			PRIMARY_TDM_TX_0, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_1 Slot Mapping", SND_SOC_NOPM,
			PRIMARY_TDM_TX_1, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_2 Slot Mapping", SND_SOC_NOPM,
			PRIMARY_TDM_TX_2, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_3 Slot Mapping", SND_SOC_NOPM,
			PRIMARY_TDM_TX_3, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_4 Slot Mapping", SND_SOC_NOPM,
			PRIMARY_TDM_TX_4, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_5 Slot Mapping", SND_SOC_NOPM,
			PRIMARY_TDM_TX_5, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_6 Slot Mapping", SND_SOC_NOPM,
			PRIMARY_TDM_TX_6, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_7 Slot Mapping", SND_SOC_NOPM,
			PRIMARY_TDM_TX_7, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_0 Slot Mapping", SND_SOC_NOPM,
			SECONDARY_TDM_RX_0, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_1 Slot Mapping", SND_SOC_NOPM,
			SECONDARY_TDM_RX_1, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_2 Slot Mapping", SND_SOC_NOPM,
			SECONDARY_TDM_RX_2, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_3 Slot Mapping", SND_SOC_NOPM,
			SECONDARY_TDM_RX_3, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_4 Slot Mapping", SND_SOC_NOPM,
			SECONDARY_TDM_RX_4, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_5 Slot Mapping", SND_SOC_NOPM,
			SECONDARY_TDM_RX_5, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_6 Slot Mapping", SND_SOC_NOPM,
			SECONDARY_TDM_RX_6, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_7 Slot Mapping", SND_SOC_NOPM,
			SECONDARY_TDM_RX_7, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_0 Slot Mapping", SND_SOC_NOPM,
			SECONDARY_TDM_TX_0, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_1 Slot Mapping", SND_SOC_NOPM,
			SECONDARY_TDM_TX_1, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_2 Slot Mapping", SND_SOC_NOPM,
			SECONDARY_TDM_TX_2, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_3 Slot Mapping", SND_SOC_NOPM,
			SECONDARY_TDM_TX_3, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_4 Slot Mapping", SND_SOC_NOPM,
			SECONDARY_TDM_TX_4, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_5 Slot Mapping", SND_SOC_NOPM,
			SECONDARY_TDM_TX_5, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_6 Slot Mapping", SND_SOC_NOPM,
			SECONDARY_TDM_TX_6, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_7 Slot Mapping", SND_SOC_NOPM,
			SECONDARY_TDM_TX_7, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_0 Slot Mapping", SND_SOC_NOPM,
			TERTIARY_TDM_RX_0, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_1 Slot Mapping", SND_SOC_NOPM,
			TERTIARY_TDM_RX_1, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_2 Slot Mapping", SND_SOC_NOPM,
			TERTIARY_TDM_RX_2, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_3 Slot Mapping", SND_SOC_NOPM,
			TERTIARY_TDM_RX_3, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_4 Slot Mapping", SND_SOC_NOPM,
			TERTIARY_TDM_RX_4, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_5 Slot Mapping", SND_SOC_NOPM,
			TERTIARY_TDM_RX_5, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_6 Slot Mapping", SND_SOC_NOPM,
			TERTIARY_TDM_RX_6, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_7 Slot Mapping", SND_SOC_NOPM,
			TERTIARY_TDM_RX_7, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_0 Slot Mapping", SND_SOC_NOPM,
			TERTIARY_TDM_TX_0, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_1 Slot Mapping", SND_SOC_NOPM,
			TERTIARY_TDM_TX_1, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_2 Slot Mapping", SND_SOC_NOPM,
			TERTIARY_TDM_TX_2, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_3 Slot Mapping", SND_SOC_NOPM,
			TERTIARY_TDM_TX_3, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_4 Slot Mapping", SND_SOC_NOPM,
			TERTIARY_TDM_TX_4, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_5 Slot Mapping", SND_SOC_NOPM,
			TERTIARY_TDM_TX_5, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_6 Slot Mapping", SND_SOC_NOPM,
			TERTIARY_TDM_TX_6, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_7 Slot Mapping", SND_SOC_NOPM,
			TERTIARY_TDM_TX_7, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_0 Slot Mapping", SND_SOC_NOPM,
			QUATERNARY_TDM_RX_0, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_1 Slot Mapping", SND_SOC_NOPM,
			QUATERNARY_TDM_RX_1, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_2 Slot Mapping", SND_SOC_NOPM,
			QUATERNARY_TDM_RX_2, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_3 Slot Mapping", SND_SOC_NOPM,
			QUATERNARY_TDM_RX_3, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_4 Slot Mapping", SND_SOC_NOPM,
			QUATERNARY_TDM_RX_4, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_5 Slot Mapping", SND_SOC_NOPM,
			QUATERNARY_TDM_RX_5, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_6 Slot Mapping", SND_SOC_NOPM,
			QUATERNARY_TDM_RX_6, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_7 Slot Mapping", SND_SOC_NOPM,
			QUATERNARY_TDM_RX_7, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_0 Slot Mapping", SND_SOC_NOPM,
			QUATERNARY_TDM_TX_0, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_1 Slot Mapping", SND_SOC_NOPM,
			QUATERNARY_TDM_TX_1, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_2 Slot Mapping", SND_SOC_NOPM,
			QUATERNARY_TDM_TX_2, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_3 Slot Mapping", SND_SOC_NOPM,
			QUATERNARY_TDM_TX_3, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_4 Slot Mapping", SND_SOC_NOPM,
			QUATERNARY_TDM_TX_4, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_5 Slot Mapping", SND_SOC_NOPM,
			QUATERNARY_TDM_TX_5, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_6 Slot Mapping", SND_SOC_NOPM,
			QUATERNARY_TDM_TX_6, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_7 Slot Mapping", SND_SOC_NOPM,
			QUATERNARY_TDM_TX_7, 0xFFFF,
			0, 8, msm_tdm_slot_mapping_get,
			msm_tdm_slot_mapping_put),
};

static bool msm8996_swap_gnd_mic(struct snd_soc_codec *codec)
{
	struct snd_soc_card *card = codec->component.card;
	struct msm8996_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);
	int value = gpio_get_value_cansleep(pdata->us_euro_gpio);

	pr_debug("%s: swap select switch %d to %d\n", __func__, value, !value);
	gpio_set_value_cansleep(pdata->us_euro_gpio, !value);
	return true;
}

static int msm_afe_set_config(struct snd_soc_codec *codec)
{
	int rc;
	void *config_data = NULL;

	pr_debug("%s: enter\n", __func__);

	if (!msm8996_codec_fn.get_afe_config_fn) {
		dev_err(codec->dev, "%s: codec get afe config not init'ed\n",
			__func__);
		return -EINVAL;
	}

	config_data = msm8996_codec_fn.get_afe_config_fn(codec,
			AFE_CDC_REGISTERS_CONFIG);
	if (config_data) {
		rc = afe_set_config(AFE_CDC_REGISTERS_CONFIG, config_data, 0);
		if (rc) {
			pr_err("%s: Failed to set codec registers config %d\n",
					__func__, rc);
			return rc;
		}
	}

	config_data = msm8996_codec_fn.get_afe_config_fn(codec,
			AFE_CDC_REGISTER_PAGE_CONFIG);
	if (config_data) {
		rc = afe_set_config(AFE_CDC_REGISTER_PAGE_CONFIG, config_data,
				    0);
		if (rc)
			pr_err("%s: Failed to set cdc register page config\n",
				__func__);
	}

	config_data = msm8996_codec_fn.get_afe_config_fn(codec,
			AFE_SLIMBUS_SLAVE_CONFIG);
	if (config_data) {
		rc = afe_set_config(AFE_SLIMBUS_SLAVE_CONFIG, config_data, 0);
		if (rc) {
			pr_err("%s: Failed to set slimbus slave config %d\n",
					__func__, rc);
			return rc;
		}
	}

	return 0;
}

static void msm_afe_clear_config(void)
{
	afe_clear_config(AFE_CDC_REGISTERS_CONFIG);
	afe_clear_config(AFE_SLIMBUS_SLAVE_CONFIG);
}

static int  msm8996_adsp_state_callback(struct notifier_block *nb,
					   unsigned long value, void *priv)
{
	if (value == SUBSYS_BEFORE_SHUTDOWN) {
		pr_debug("%s: ADSP is about to shutdown. Clearing AFE config\n",
			 __func__);
		msm_afe_clear_config();
	} else if (value == SUBSYS_AFTER_POWERUP) {
		pr_debug("%s: ADSP is up\n", __func__);
	}

	return NOTIFY_OK;
}

static struct notifier_block adsp_state_notifier_block = {
	.notifier_call = msm8996_adsp_state_callback,
	.priority = -INT_MAX,
};

static int msm8996_wcd93xx_codec_up(struct snd_soc_codec *codec)
{
	int err;
	unsigned long timeout;
	int adsp_ready = 0;

	timeout = jiffies +
		msecs_to_jiffies(ADSP_STATE_READY_TIMEOUT_MS);

	do {
		if (!q6core_is_adsp_ready()) {
			pr_err_ratelimited("%s: ADSP Audio isn't ready\n",
					   __func__);
			/*
			 * ADSP will be coming up after subsystem restart and
			 * it might not be fully up when the control reaches
			 * here. So, wait for 50msec before checking ADSP state
			 */
			msleep(50);
		} else {
			pr_debug("%s: ADSP Audio is ready\n", __func__);
			adsp_ready = 1;
			break;
		}
	} while (time_after(timeout, jiffies));

	if (!adsp_ready) {
		pr_err("%s: timed out waiting for ADSP Audio\n", __func__);
		return -ETIMEDOUT;
	}

	err = msm_afe_set_config(codec);
	if (err)
		pr_err("%s: Failed to set AFE config. err %d\n",
			__func__, err);
	return err;
}

static int msm8996_tasha_codec_event_cb(struct snd_soc_codec *codec,
					enum wcd9335_codec_event codec_event)
{
	switch (codec_event) {
	case WCD9335_CODEC_EVENT_CODEC_UP:
		return msm8996_wcd93xx_codec_up(codec);
	default:
		pr_err("%s: UnSupported codec event %d\n",
			__func__, codec_event);
		return -EINVAL;
	}
}

static int msm8996_config_hph_en0_gpio(struct snd_soc_codec *codec, bool high)
{
	struct snd_soc_card *card = codec->component.card;
	struct msm8996_asoc_mach_data *pdata;
	int val;

	if (!card)
		return 0;

	pdata = snd_soc_card_get_drvdata(card);
	if (!pdata || !gpio_is_valid(pdata->hph_en0_gpio))
		return 0;

	val = gpio_get_value_cansleep(pdata->hph_en0_gpio);
	if ((!!val) == high)
		return 0;

	gpio_direction_output(pdata->hph_en0_gpio, (int)high);

	return 1;
}

static int msm_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	void *config_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_pcm_runtime *rtd_aux = rtd->card->rtd_aux;
	void *mbhc_calibration;
	struct snd_card *card;
	struct snd_info_entry *entry;
	struct msm8996_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(rtd->card);

	/* Codec SLIMBUS configuration
	 * RX1, RX2, RX3, RX4, RX5, RX6, RX7, RX8, RX9, RX10, RX11, RX12, RX13
	 * TX1, TX2, TX3, TX4, TX5, TX6, TX7, TX8, TX9, TX10, TX11, TX12, TX13
	 * TX14, TX15, TX16
	 */
	unsigned int rx_ch[TASHA_RX_MAX] = {144, 145, 146, 147, 148, 149, 150,
					    151, 152, 153, 154, 155, 156};
	unsigned int tx_ch[TASHA_TX_MAX] = {128, 129, 130, 131, 132, 133,
					    134, 135, 136, 137, 138, 139,
					    140, 141, 142, 143};

	pr_info("%s: dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	rtd->pmdown_time = 0;

	err = snd_soc_add_codec_controls(codec, msm_snd_controls,
					 ARRAY_SIZE(msm_snd_controls));
	if (err < 0) {
		pr_err("%s: add_codec_controls failed, err %d\n",
			__func__, err);
		return err;
	}

	err = msm8996_liquid_init_docking();
	if (err) {
		pr_err("%s: 8996 init Docking stat IRQ failed (%d)\n",
			__func__, err);
		return err;
	}

	err = msm8996_ext_us_amp_init();
	if (err) {
		pr_err("%s: 8996 US Emitter GPIO init failed (%d)\n",
			__func__, err);
		return err;
	}

	snd_soc_dapm_new_controls(dapm, msm8996_dapm_widgets,
				ARRAY_SIZE(msm8996_dapm_widgets));

	snd_soc_dapm_add_routes(dapm, wcd9335_audio_paths,
				ARRAY_SIZE(wcd9335_audio_paths));
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
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic4");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic6");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic7");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic8");
	snd_soc_dapm_ignore_suspend(dapm, "MADINPUT");
	snd_soc_dapm_ignore_suspend(dapm, "MAD_CPE_INPUT");
	snd_soc_dapm_ignore_suspend(dapm, "EAR");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT1");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT2");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT3");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT4");
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
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic0");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC0");
	snd_soc_dapm_ignore_suspend(dapm, "SPK1 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "SPK2 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "HPHL");
	snd_soc_dapm_ignore_suspend(dapm, "HPHR");
	snd_soc_dapm_ignore_suspend(dapm, "ANC HPHL");
	snd_soc_dapm_ignore_suspend(dapm, "ANC HPHR");
	snd_soc_dapm_ignore_suspend(dapm, "ANC LINEOUT1");
	snd_soc_dapm_ignore_suspend(dapm, "ANC LINEOUT2");
	snd_soc_dapm_ignore_suspend(dapm, "AIF4 VI");
	snd_soc_dapm_ignore_suspend(dapm, "VIINPUT");

	snd_soc_dapm_sync(dapm);

	snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch),
				    tx_ch, ARRAY_SIZE(rx_ch), rx_ch);

	msm8996_codec_fn.get_afe_config_fn = tasha_get_afe_config;
	msm8996_codec_fn.mbhc_hs_detect_exit = tasha_mbhc_hs_detect_exit;

	err = msm_afe_set_config(codec);
	if (err) {
		pr_err("%s: Failed to set AFE config %d\n", __func__, err);
		goto out;
	}

	config_data = msm8996_codec_fn.get_afe_config_fn(codec,
						AFE_AANC_VERSION);
	if (config_data) {
		err = afe_set_config(AFE_AANC_VERSION, config_data, 0);
		if (err) {
			pr_err("%s: Failed to set aanc version %d\n",
				__func__, err);
			goto out;
		}
	}
	config_data = msm8996_codec_fn.get_afe_config_fn(codec,
					    AFE_CDC_CLIP_REGISTERS_CONFIG);
	if (config_data) {
		err = afe_set_config(AFE_CDC_CLIP_REGISTERS_CONFIG,
				     config_data, 0);
		if (err) {
			pr_err("%s: Failed to set clip registers %d\n",
				__func__, err);
			goto out;
		}
	}
	config_data = msm8996_codec_fn.get_afe_config_fn(codec,
			AFE_CLIP_BANK_SEL);
	if (config_data) {
		err = afe_set_config(AFE_CLIP_BANK_SEL, config_data, 0);
		if (err) {
			pr_err("%s: Failed to set AFE bank selection %d\n",
				__func__, err);
			goto out;
		}
	}
	/* Start mbhc */
	tasha_mbhc_zdet_gpio_ctrl(msm8996_config_hph_en0_gpio, rtd->codec);
	mbhc_calibration = def_tasha_mbhc_cal();
	if (mbhc_calibration) {
		wcd_mbhc_cfg.calibration = mbhc_calibration;
		err = tasha_mbhc_hs_detect(codec, &wcd_mbhc_cfg);
		if (err) {
			pr_err("%s: mbhc hs detect failed, err:%d\n",
				__func__, err);
			goto out;
		}
	} else {
		pr_err("%s: mbhc_cfg calibration is NULL\n", __func__);
		err = -ENOMEM;
		goto out;
	}
	adsp_state_notifier = subsys_notif_register_notifier("adsp",
						&adsp_state_notifier_block);
	if (!adsp_state_notifier) {
		pr_err("%s: Failed to register adsp state notifier\n",
		       __func__);
		err = -EFAULT;
		msm8996_codec_fn.mbhc_hs_detect_exit(codec);
		goto out;
	}

	tasha_event_register(msm8996_tasha_codec_event_cb, rtd->codec);

	/*
	 * Send speaker configuration only for WSA8810.
	 * Defalut configuration is for WSA8815.
	 */
	if (rtd_aux && rtd_aux->component)
		if (!strcmp(rtd_aux->component->name, WSA8810_NAME_1) ||
		    !strcmp(rtd_aux->component->name, WSA8810_NAME_2)) {
			tasha_set_spkr_mode(rtd->codec, SPKR_MODE_1);
			tasha_set_spkr_gain_offset(rtd->codec,
						   RX_GAIN_OFFSET_M1P5_DB);
	}
	codec_reg_done = true;

	card = rtd->card->snd_card;
	entry = snd_register_module_info(card->module, "codecs",
					 card->proc_root);
	if (!entry) {
		pr_debug("%s: Cannot create codecs module entry\n",
			 __func__);
		err = 0;
		goto out;
	}
	pdata->codec_root = entry;
	tasha_codec_info_create_codec_entry(pdata->codec_root, codec);

	return 0;
out:
	return err;
}

static void *def_tasha_mbhc_cal(void)
{
	void *tasha_wcd_cal;
	struct wcd_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_high;

	tasha_wcd_cal = kzalloc(WCD_MBHC_CAL_SIZE(WCD_MBHC_DEF_BUTTONS,
				WCD9XXX_MBHC_DEF_RLOADS), GFP_KERNEL);
	if (!tasha_wcd_cal)
		return NULL;

#define S(X, Y) ((WCD_MBHC_CAL_PLUG_TYPE_PTR(tasha_wcd_cal)->X) = (Y))
	S(v_hs_max, 1500);
#undef S
#define S(X, Y) ((WCD_MBHC_CAL_BTN_DET_PTR(tasha_wcd_cal)->X) = (Y))
	S(num_btn, WCD_MBHC_DEF_BUTTONS);
#undef S

	btn_cfg = WCD_MBHC_CAL_BTN_DET_PTR(tasha_wcd_cal);
	btn_high = ((void *)&btn_cfg->_v_btn_low) +
		(sizeof(btn_cfg->_v_btn_low[0]) * btn_cfg->num_btn);

	btn_high[0] = 75;
	btn_high[1] = 150;
	btn_high[2] = 237;
	btn_high[3] = 500;
	btn_high[4] = 500;
	btn_high[5] = 500;
	btn_high[6] = 500;
	btn_high[7] = 500;

	return tasha_wcd_cal;
}

static int msm_snd_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;

	int ret = 0;
	u32 rx_ch[SLIM_MAX_RX_PORTS], tx_ch[SLIM_MAX_TX_PORTS];
	u32 rx_ch_cnt = 0, tx_ch_cnt = 0;
	u32 user_set_tx_ch = 0;
	u32 rx_ch_count;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = snd_soc_dai_get_channel_map(codec_dai,
					&tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map, err:%d\n",
				__func__, ret);
			goto end;
		}
		if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_5_RX) {
			pr_debug("%s: rx_5_ch=%d\n", __func__,
				  msm_slim_5_rx_ch);
			rx_ch_count = msm_slim_5_rx_ch;
		} else if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_6_RX) {
			pr_debug("%s: rx_6_ch=%d\n", __func__,
				  msm_slim_6_rx_ch);
			rx_ch_count = msm_slim_6_rx_ch;
		} else {
			pr_debug("%s: rx_0_ch=%d\n", __func__,
				  msm_slim_0_rx_ch);
			rx_ch_count = msm_slim_0_rx_ch;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, NULL,
						  rx_ch_count, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
			goto end;
		}
	} else {

		pr_debug("%s: %s_tx_dai_id_%d_ch=%d\n", __func__,
			 codec_dai->name, codec_dai->id, user_set_tx_ch);
		ret = snd_soc_dai_get_channel_map(codec_dai,
					 &tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n, err:%d\n",
				__func__, ret);
			goto end;
		}
		/* For <codec>_tx1 case */
		if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_0_TX)
			user_set_tx_ch = msm_slim_0_tx_ch;
		/* For <codec>_tx3 case */
		else if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_1_TX)
			user_set_tx_ch = msm_slim_1_tx_ch;
		else if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_3_TX)
			/* DAI 5 is used for external EC reference from codec.
			 * Since Rx is fed as reference for EC, the config of
			 * this DAI is based on that of the Rx path.
			 */
			user_set_tx_ch = msm_slim_0_rx_ch;
		else if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_4_TX)
			user_set_tx_ch = msm_vi_feed_tx_ch;
		else
			user_set_tx_ch = tx_ch_cnt;

		pr_debug("%s: msm_slim_0_tx_ch(%d) user_set_tx_ch(%d) tx_ch_cnt(%d), be_id (%d)\n",
			 __func__, msm_slim_0_tx_ch, user_set_tx_ch,
			 tx_ch_cnt, dai_link->be_id);

		ret = snd_soc_dai_set_channel_map(cpu_dai,
						  user_set_tx_ch,
						  tx_ch, 0, NULL);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
			goto end;
		}
	}
end:
	return ret;
}

static int msm_snd_cpe_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;

	int ret = 0;
	u32 tx_ch[SLIM_MAX_TX_PORTS];
	u32 tx_ch_cnt = 0;
	u32 user_set_tx_ch = 0;

	if (substream->stream != SNDRV_PCM_STREAM_CAPTURE) {
		pr_err("%s: Invalid stream type %d\n",
			__func__, substream->stream);
		ret = -EINVAL;
		goto end;
	}

	pr_debug("%s: %s_tx_dai_id_%d\n", __func__,
		 codec_dai->name, codec_dai->id);
	ret = snd_soc_dai_get_channel_map(codec_dai,
				 &tx_ch_cnt, tx_ch, NULL, NULL);
	if (ret < 0) {
		pr_err("%s: failed to get codec chan map\n, err:%d\n",
			__func__, ret);
		goto end;
	}

	user_set_tx_ch = tx_ch_cnt;

	pr_debug("%s: tx_ch_cnt(%d) be_id %d\n",
		 __func__, tx_ch_cnt, dai_link->be_id);

	ret = snd_soc_dai_set_channel_map(cpu_dai,
					  user_set_tx_ch, tx_ch, 0, NULL);
	if (ret < 0)
		pr_err("%s: failed to set cpu chan map, err:%d\n",
			__func__, ret);
end:
	return ret;
}

static struct snd_soc_ops msm8996_be_ops = {
	.hw_params = msm_snd_hw_params,
};

static struct snd_soc_ops msm8996_cpe_ops = {
	.hw_params = msm_snd_cpe_hw_params,
};

static int msm8996_slimbus_2_hw_params(struct snd_pcm_substream *substream,
					  struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned int rx_ch[SLIM_MAX_RX_PORTS], tx_ch[SLIM_MAX_TX_PORTS];
	unsigned int rx_ch_cnt = 0, tx_ch_cnt = 0;
	unsigned int num_tx_ch = 0;
	unsigned int num_rx_ch = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		num_rx_ch =  params_channels(params);
		pr_debug("%s: %s rx_dai_id = %d  num_ch = %d\n", __func__,
			codec_dai->name, codec_dai->id, num_rx_ch);
		ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map, err:%d\n",
				__func__, ret);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, NULL,
				num_rx_ch, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
			goto end;
		}
	} else {
		num_tx_ch =  params_channels(params);
		pr_debug("%s: %s  tx_dai_id = %d  num_ch = %d\n", __func__,
			codec_dai->name, codec_dai->id, num_tx_ch);
		ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map, err:%d\n",
				__func__, ret);
			goto end;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai,
				num_tx_ch, tx_ch, 0, NULL);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
			goto end;
		}
	}
end:
	return ret;
}

static struct snd_soc_ops msm8996_slimbus_2_be_ops = {
	.hw_params = msm8996_slimbus_2_hw_params,
};

static int msm8996_get_ll_qos_val(struct snd_pcm_runtime *runtime)
{
	int usecs;

	/* take 10% of period time as the deadline */
	usecs = (100000 / runtime->rate) * runtime->period_size;
	usecs += ((100000 % runtime->rate) * runtime->period_size) /
		runtime->rate;

	return usecs;
}

static int msm8996_mm5_prepare(struct snd_pcm_substream *substream)
{
	if (pm_qos_request_active(&substream->latency_pm_qos_req))
		pm_qos_remove_request(&substream->latency_pm_qos_req);
	pm_qos_add_request(&substream->latency_pm_qos_req,
			   PM_QOS_CPU_DMA_LATENCY,
			   msm8996_get_ll_qos_val(substream->runtime));
	return 0;
}

static struct snd_soc_ops msm8996_mm5_ops = {
	.prepare = msm8996_mm5_prepare,
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm8996_common_dai_links[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MSM8996 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name = "MultiMedia1",
		.platform_name = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{
		.name = "MSM8996 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name = "MultiMedia2",
		.platform_name = "msm-pcm-dsp.0",
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
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{
		.name = "VoiceMMode1",
		.stream_name = "VoiceMMode1",
		.cpu_dai_name = "VoiceMMode1",
		.platform_name = "msm-pcm-voice",
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
		.be_id = MSM_FRONTEND_DAI_VOICEMMODE1,
	},
	{
		.name = "MSM VoIP",
		.stream_name = "VoIP",
		.cpu_dai_name = "VoIP",
		.platform_name = "msm-voip-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_VOIP,
	},
	{
		.name = "MSM8996 ULL",
		.stream_name = "MultiMedia3",
		.cpu_dai_name = "MultiMedia3",
		.platform_name = "msm-pcm-dsp.2",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA3,
	},
	/* Hostless PCM purpose */
	{
		.name = "SLIMBUS_0 Hostless",
		.stream_name = "SLIMBUS_0 Hostless",
		.cpu_dai_name = "SLIMBUS0_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "Tertiary MI2S TX_Hostless",
		.stream_name = "Tertiary MI2S_TX Hostless Capture",
		.cpu_dai_name = "TERT_MI2S_TX_HOSTLESS",
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
		.name = "MSM AFE-PCM RX",
		.stream_name = "AFE-PROXY RX",
		.cpu_dai_name = "msm-dai-q6-dev.241",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.platform_name = "msm-pcm-afe",
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
		.name = "MSM8996 Compress1",
		.stream_name = "Compress1",
		.cpu_dai_name = "MultiMedia4",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_HW_PARAMS,
		.dpcm_playback = 1,
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
		.name = "AUXPCM Hostless",
		.stream_name = "AUXPCM Hostless",
		.cpu_dai_name = "AUXPCM_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
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
	},
	{
		.name = "SLIMBUS_1 Hostless",
		.stream_name = "SLIMBUS_1 Hostless",
		.cpu_dai_name = "SLIMBUS1_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "SLIMBUS_3 Hostless",
		.stream_name = "SLIMBUS_3 Hostless",
		.cpu_dai_name = "SLIMBUS3_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "SLIMBUS_4 Hostless",
		.stream_name = "SLIMBUS_4 Hostless",
		.cpu_dai_name = "SLIMBUS4_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{
		.name = "VoLTE",
		.stream_name = "VoLTE",
		.cpu_dai_name = "VoLTE",
		.platform_name = "msm-pcm-voice",
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
	{
		.name = "MSM8996 LowLatency",
		.stream_name = "MultiMedia5",
		.cpu_dai_name = "MultiMedia5",
		.platform_name = "msm-pcm-dsp.1",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
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
		.ops = &msm8996_mm5_ops,
	},
	{
		.name = "Listen 1 Audio Service",
		.stream_name = "Listen 1 Audio Service",
		.cpu_dai_name = "LSM1",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
			     SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM1,
	},
	/* Multiple Tunnel instances */
	{
		.name = "MSM8996 Compress2",
		.stream_name = "Compress2",
		.cpu_dai_name = "MultiMedia7",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA7,
	},
	{
		.name = "MSM8996 Compress3",
		.stream_name = "Compress3",
		.cpu_dai_name = "MultiMedia10",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA10,
	},
	{
		.name = "MSM8996 ULL NOIRQ",
		.stream_name = "MM_NOIRQ",
		.cpu_dai_name = "MultiMedia8",
		.platform_name = "msm-pcm-dsp-noirq",
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
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA8,
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
	/* HDMI Hostless */
	{
		.name = "HDMI_RX_HOSTLESS",
		.stream_name = "HDMI_RX_HOSTLESS",
		.cpu_dai_name = "HDMI_HOSTLESS",
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
		.name = "VoiceMMode2",
		.stream_name = "VoiceMMode2",
		.cpu_dai_name = "VoiceMMode2",
		.platform_name = "msm-pcm-voice",
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
		.be_id = MSM_FRONTEND_DAI_VOICEMMODE2,
	},
	{
		.name = "INT_HFP_BT Hostless",
		.stream_name = "INT_HFP_BT Hostless",
		.cpu_dai_name = "INT_HFP_BT_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
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
	},
	{
		.name = "MSM8996 HFP TX",
		.stream_name = "MultiMedia6",
		.cpu_dai_name = "MultiMedia6",
		.platform_name = "msm-pcm-loopback",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA6,
	},
	/* LSM FE */
	{
		.name = "Listen 2 Audio Service",
		.stream_name = "Listen 2 Audio Service",
		.cpu_dai_name = "LSM2",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM2,
	},
	{
		.name = "Listen 3 Audio Service",
		.stream_name = "Listen 3 Audio Service",
		.cpu_dai_name = "LSM3",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM3,
	},
	{
		.name = "Listen 4 Audio Service",
		.stream_name = "Listen 4 Audio Service",
		.cpu_dai_name = "LSM4",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM4,
	},
	{
		.name = "Listen 5 Audio Service",
		.stream_name = "Listen 5 Audio Service",
		.cpu_dai_name = "LSM5",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM5,
	},
	{
		.name = "Listen 6 Audio Service",
		.stream_name = "Listen 6 Audio Service",
		.cpu_dai_name = "LSM6",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM6,
	},
	{
		.name = "Listen 7 Audio Service",
		.stream_name = "Listen 7 Audio Service",
		.cpu_dai_name = "LSM7",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM7,
	},
	{
		.name = "Listen 8 Audio Service",
		.stream_name = "Listen 8 Audio Service",
		.cpu_dai_name = "LSM8",
		.platform_name = "msm-lsm-client",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = { SND_SOC_DPCM_TRIGGER_POST,
				 SND_SOC_DPCM_TRIGGER_POST },
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.be_id = MSM_FRONTEND_DAI_LSM8,
	},
	{
		.name = "MSM8996 Media9",
		.stream_name = "MultiMedia9",
		.cpu_dai_name = "MultiMedia9",
		.platform_name = "msm-pcm-dsp.0",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA9,
	},
	{
		.name = "VoWLAN",
		.stream_name = "VoWLAN",
		.cpu_dai_name = "VoWLAN",
		.platform_name = "msm-pcm-voice",
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
		.be_id = MSM_FRONTEND_DAI_VOWLAN,
	},
	{
		.name = "MSM8996 Compress4",
		.stream_name = "Compress4",
		.cpu_dai_name = "MultiMedia11",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA11,
	},
	{
		.name = "MSM8996 Compress5",
		.stream_name = "Compress5",
		.cpu_dai_name = "MultiMedia12",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA12,
	},
	{
		.name = "MSM8996 Compress6",
		.stream_name = "Compress6",
		.cpu_dai_name = "MultiMedia13",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA13,
	},
	{
		.name = "MSM8996 Compress7",
		.stream_name = "Compress7",
		.cpu_dai_name = "MultiMedia14",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA14,
	},
	{
		.name = "MSM8996 Compress8",
		.stream_name = "Compress8",
		.cpu_dai_name = "MultiMedia15",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA15,
	},
	{
		.name = "MSM8996 ULL NOIRQ_2",
		.stream_name = "MM_NOIRQ_2",
		.cpu_dai_name = "MultiMedia16",
		.platform_name = "msm-pcm-dsp-noirq",
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
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA16,
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
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_CS_VOICE,
	},
	{
		.name = "Voice2",
		.stream_name = "Voice2",
		.cpu_dai_name = "Voice2",
		.platform_name = "msm-pcm-voice",
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
		.be_id = MSM_FRONTEND_DAI_VOICE2,
	},
};
static struct snd_soc_dai_link msm8996_tdm_fe_dai_links[] = {
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
		.name = "Primary TDM RX 1 Hostless",
		.stream_name = "Primary TDM RX 1 Hostless",
		.cpu_dai_name = "PRI_TDM_RX_1_HOSTLESS",
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
		.name = "Primary TDM RX 2 Hostless",
		.stream_name = "Primary TDM RX 2 Hostless",
		.cpu_dai_name = "PRI_TDM_RX_2_HOSTLESS",
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
		.name = "Primary TDM RX 3 Hostless",
		.stream_name = "Primary TDM RX 3 Hostless",
		.cpu_dai_name = "PRI_TDM_RX_3_HOSTLESS",
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
		.name = "Primary TDM TX 1 Hostless",
		.stream_name = "Primary TDM TX 1 Hostless",
		.cpu_dai_name = "PRI_TDM_TX_1_HOSTLESS",
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
		.name = "Primary TDM TX 2 Hostless",
		.stream_name = "Primary TDM TX 2 Hostless",
		.cpu_dai_name = "PRI_TDM_TX_2_HOSTLESS",
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
		.name = "Primary TDM TX 3 Hostless",
		.stream_name = "Primary TDM TX 3 Hostless",
		.cpu_dai_name = "PRI_TDM_TX_3_HOSTLESS",
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
		.name = "Secondary TDM RX 1 Hostless",
		.stream_name = "Secondary TDM RX 1 Hostless",
		.cpu_dai_name = "SEC_TDM_RX_1_HOSTLESS",
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
		.name = "Secondary TDM RX 2 Hostless",
		.stream_name = "Secondary TDM RX 2 Hostless",
		.cpu_dai_name = "SEC_TDM_RX_2_HOSTLESS",
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
		.name = "Secondary TDM RX 3 Hostless",
		.stream_name = "Secondary TDM RX 3 Hostless",
		.cpu_dai_name = "SEC_TDM_RX_3_HOSTLESS",
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
	{
		.name = "Secondary TDM TX 1 Hostless",
		.stream_name = "Secondary TDM TX 1 Hostless",
		.cpu_dai_name = "SEC_TDM_TX_1_HOSTLESS",
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
		.name = "Secondary TDM TX 2 Hostless",
		.stream_name = "Secondary TDM TX 2 Hostless",
		.cpu_dai_name = "SEC_TDM_TX_2_HOSTLESS",
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
		.name = "Secondary TDM TX 3 Hostless",
		.stream_name = "Secondary TDM TX 3 Hostless",
		.cpu_dai_name = "SEC_TDM_TX_3_HOSTLESS",
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
		.name = "Tertiary TDM RX 0 Hostless",
		.stream_name = "Tertiary TDM RX 0 Hostless",
		.cpu_dai_name = "TERT_TDM_RX_0_HOSTLESS",
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
		.name = "Tertiary TDM RX 1 Hostless",
		.stream_name = "Tertiary TDM RX 1 Hostless",
		.cpu_dai_name = "TERT_TDM_RX_1_HOSTLESS",
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
		.name = "Tertiary TDM RX 2 Hostless",
		.stream_name = "Tertiary TDM RX 2 Hostless",
		.cpu_dai_name = "TERT_TDM_RX_2_HOSTLESS",
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
		.name = "Tertiary TDM RX 3 Hostless",
		.stream_name = "Tertiary TDM RX 3 Hostless",
		.cpu_dai_name = "TERT_TDM_RX_3_HOSTLESS",
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
		.name = "Tertiary TDM TX 0 Hostless",
		.stream_name = "Tertiary TDM TX 0 Hostless",
		.cpu_dai_name = "TERT_TDM_TX_0_HOSTLESS",
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
		.name = "Tertiary TDM TX 1 Hostless",
		.stream_name = "Tertiary TDM TX 1 Hostless",
		.cpu_dai_name = "TERT_TDM_TX_1_HOSTLESS",
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
		.name = "Tertiary TDM TX 2 Hostless",
		.stream_name = "Tertiary TDM TX 2 Hostless",
		.cpu_dai_name = "TERT_TDM_TX_2_HOSTLESS",
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
		.name = "Tertiary TDM TX 3 Hostless",
		.stream_name = "Tertiary TDM TX 3 Hostless",
		.cpu_dai_name = "TERT_TDM_TX_3_HOSTLESS",
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
		.name = "Quaternary TDM RX 0 Hostless",
		.stream_name = "Quaternary TDM RX 0 Hostless",
		.cpu_dai_name = "QUAT_TDM_RX_0_HOSTLESS",
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
		.name = "Quaternary TDM RX 1 Hostless",
		.stream_name = "Quaternary TDM RX 1 Hostless",
		.cpu_dai_name = "QUAT_TDM_RX_1_HOSTLESS",
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
		.name = "Quaternary TDM RX 2 Hostless",
		.stream_name = "Quaternary TDM RX 2 Hostless",
		.cpu_dai_name = "QUAT_TDM_RX_2_HOSTLESS",
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
		.name = "Quaternary TDM RX 3 Hostless",
		.stream_name = "Quaternary TDM RX 3 Hostless",
		.cpu_dai_name = "QUAT_TDM_RX_3_HOSTLESS",
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
		.name = "Quaternary TDM TX 0 Hostless",
		.stream_name = "Quaternary TDM TX 0 Hostless",
		.cpu_dai_name = "QUAT_TDM_TX_0_HOSTLESS",
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
		.name = "Quaternary TDM TX 1 Hostless",
		.stream_name = "Quaternary TDM TX 1 Hostless",
		.cpu_dai_name = "QUAT_TDM_TX_1_HOSTLESS",
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
		.name = "Quaternary TDM TX 2 Hostless",
		.stream_name = "Quaternary TDM TX 2 Hostless",
		.cpu_dai_name = "QUAT_TDM_TX_2_HOSTLESS",
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
		.name = "Quaternary TDM TX 3 Hostless",
		.stream_name = "Quaternary TDM TX 3 Hostless",
		.cpu_dai_name = "QUAT_TDM_TX_3_HOSTLESS",
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
static struct snd_soc_dai_link msm8996_tasha_fe_dai_links[] = {
	{
		.name = LPASS_BE_SLIMBUS_4_TX,
		.stream_name = "Slimbus4 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16393",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_vifeedback",
		.be_id = MSM_BACKEND_DAI_SLIMBUS_4_TX,
		.be_hw_params_fixup = msm_slim_4_tx_be_hw_params_fixup,
		.ops = &msm8996_be_ops,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
	},
	/* Ultrasound RX DAI Link */
	{
		.name = "SLIMBUS_2 Hostless Playback",
		.stream_name = "SLIMBUS_2 Hostless Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16388",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_rx2",
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ops = &msm8996_slimbus_2_be_ops,
	},
	/* Ultrasound TX DAI Link */
	{
		.name = "SLIMBUS_2 Hostless Capture",
		.stream_name = "SLIMBUS_2 Hostless Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16389",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_tx2",
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ops = &msm8996_slimbus_2_be_ops,
	},
	/* CPE LSM direct dai-link */
	{
		.name = "CPE Listen service",
		.stream_name = "CPE Listen Audio Service",
		.cpu_dai_name = "msm-dai-slim",
		.platform_name = "msm-cpe-lsm",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "tasha_mad1",
		.codec_name = "tasha_codec",
		.ops = &msm8996_cpe_ops,
	},
	/* slimbus rx 6 hostless */
	{
		.name = "SLIMBUS_6 Hostless Playback",
		.stream_name = "SLIMBUS_6 Hostless",
		.cpu_dai_name = "SLIMBUS6_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		 /* this dailink has playback support */
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	/* CPE LSM EC PP direct dai-link */
	{
		.name = "CPE Listen service ECPP",
		.stream_name = "CPE Listen Audio Service ECPP",
		.cpu_dai_name = "CPE_LSM_NOHOST",
		.platform_name = "msm-cpe-lsm.3",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "tasha_cpe",
		.codec_name = "tasha_codec",
	},
};

static struct snd_soc_dai_link msm8996_common_be_dai_links[] = {
	/* Backend AFE DAI Links */
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
		.be_hw_params_fixup = msm_proxy_rx_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
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
		.be_hw_params_fixup = msm_proxy_tx_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Primary AUX PCM Backend DAI Links */
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
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
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
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Record Uplink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_TX,
		.stream_name = "Voice Uplink Capture",
		.cpu_dai_name = "msm-dai-q6-dev.32772",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Record Downlink BACK END DAI Link */
	{
		.name = LPASS_BE_INCALL_RECORD_RX,
		.stream_name = "Voice Downlink Capture",
		.cpu_dai_name = "msm-dai-q6-dev.32771",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_INCALL_RECORD_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Music BACK END DAI Link */
	{
		.name = LPASS_BE_VOICE_PLAYBACK_TX,
		.stream_name = "Voice Farend Playback",
		.cpu_dai_name = "msm-dai-q6-dev.32773",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Incall Music 2 BACK END DAI Link */
	{
		.name = LPASS_BE_VOICE2_PLAYBACK_TX,
		.stream_name = "Voice2 Farend Playback",
		.cpu_dai_name = "msm-dai-q6-dev.32770",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_VOICE2_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_MI2S_TX,
		.stream_name = "Tertiary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
		.be_hw_params_fixup = msm_tx_be_hw_params_fixup,
		.ops = &msm8996_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_USB_AUDIO_RX,
		.stream_name = "USB Audio Playback",
		.cpu_dai_name = "msm-dai-q6-dev.28672",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_USB_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_USB_AUDIO_TX,
		.stream_name = "USB Audio Capture",
		.cpu_dai_name = "msm-dai-q6-dev.28673",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_USB_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	}
};

static struct snd_soc_dai_link msm8996_tasha_be_dai_links[] = {
	/* Backend DAI Links */
	{
		.name = LPASS_BE_SLIMBUS_0_RX,
		.stream_name = "Slimbus Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16384",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_mix_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_RX,
		.init = &msm_audrx_init,
		.be_hw_params_fixup = msm_slim_0_rx_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm8996_be_ops,
	},
	{
		.name = LPASS_BE_SLIMBUS_0_TX,
		.stream_name = "Slimbus Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16385",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_tx1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_TX,
		.be_hw_params_fixup = msm_slim_0_tx_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm8996_be_ops,
	},
	{
		.name = LPASS_BE_SLIMBUS_1_RX,
		.stream_name = "Slimbus1 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16386",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_mix_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_1_RX,
		.be_hw_params_fixup = msm_slim_0_rx_be_hw_params_fixup,
		.ops = &msm8996_be_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_1_TX,
		.stream_name = "Slimbus1 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16387",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_tx3",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_1_TX,
		.be_hw_params_fixup = msm_slim_1_tx_be_hw_params_fixup,
		.ops = &msm8996_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_3_RX,
		.stream_name = "Slimbus3 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16390",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_mix_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_3_RX,
		.be_hw_params_fixup = msm_slim_0_rx_be_hw_params_fixup,
		.ops = &msm8996_be_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_3_TX,
		.stream_name = "Slimbus3 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16391",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_tx1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_3_TX,
		.be_hw_params_fixup = msm_slim_0_tx_be_hw_params_fixup,
		.ops = &msm8996_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_4_RX,
		.stream_name = "Slimbus4 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16392",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_mix_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_4_RX,
		.be_hw_params_fixup = msm_slim_0_rx_be_hw_params_fixup,
		.ops = &msm8996_be_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_5_RX,
		.stream_name = "Slimbus5 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16394",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_rx3",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_5_RX,
		.be_hw_params_fixup = msm_slim_5_rx_be_hw_params_fixup,
		.ops = &msm8996_be_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	/* MAD BE */
	{
		.name = LPASS_BE_SLIMBUS_5_TX,
		.stream_name = "Slimbus5 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16395",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_mad1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_5_TX,
		.be_hw_params_fixup = msm_slim_5_tx_be_hw_params_fixup,
		.ops = &msm8996_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_6_RX,
		.stream_name = "Slimbus6 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16396",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_rx4",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_6_RX,
		.be_hw_params_fixup = msm_slim_6_rx_be_hw_params_fixup,
		.ops = &msm8996_be_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	}
};
static struct snd_soc_dai_link msm8996_tdm_be_dai_links[] = {
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
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_TDM_RX_1,
		.stream_name = "Secondary TDM1 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36882",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SEC_TDM_RX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_TDM_RX_2,
		.stream_name = "Secondary TDM2 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36884",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SEC_TDM_RX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_TDM_RX_3,
		.stream_name = "Secondary TDM3 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36886",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SEC_TDM_RX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_TDM_TX_0,
		.stream_name = "Secondary TDM0 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36881",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SEC_TDM_TX_0,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_TDM_TX_1,
		.stream_name = "Secondary TDM1 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36883",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SEC_TDM_TX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_TDM_TX_2,
		.stream_name = "Secondary TDM2 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36885",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SEC_TDM_TX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_TDM_TX_3,
		.stream_name = "Secondary TDM3 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36887",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SEC_TDM_TX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_RX_0,
		.stream_name = "Tertiary TDM0 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36896",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_RX_0,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_RX_1,
		.stream_name = "Tertiary TDM1 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36898",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_RX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_RX_2,
		.stream_name = "Tertiary TDM2 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36900",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_RX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_RX_3,
		.stream_name = "Tertiary TDM3 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36902",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_RX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_TX_0,
		.stream_name = "Tertiary TDM0 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36897",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_TX_0,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_TX_1,
		.stream_name = "Tertiary TDM1 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36899",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_TX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_TX_2,
		.stream_name = "Tertiary TDM2 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36901",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_TX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_TX_3,
		.stream_name = "Tertiary TDM3 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36903",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_TX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_RX_0,
		.stream_name = "Quaternary TDM0 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36912",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_RX_1,
		.stream_name = "Quaternary TDM1 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36914",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_RX_2,
		.stream_name = "Quaternary TDM2 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36916",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_RX_3,
		.stream_name = "Quaternary TDM3 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36918",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_TX_0,
		.stream_name = "Quaternary TDM0 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36913",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_TX_0,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_TX_1,
		.stream_name = "Quaternary TDM1 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36915",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_TX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_TX_2,
		.stream_name = "Quaternary TDM2 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36917",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_TX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_TX_3,
		.stream_name = "Quaternary TDM3 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36919",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_TX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
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
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_RX_1,
		.stream_name = "Primary TDM1 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36866",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_RX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_RX_2,
		.stream_name = "Primary TDM2 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36868",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_RX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_RX_3,
		.stream_name = "Primary TDM3 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36870",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_RX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_TX_0,
		.stream_name = "Primary TDM0 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36865",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_TX_0,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_TX_1,
		.stream_name = "Primary TDM1 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36867",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_TX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_TX_2,
		.stream_name = "Primary TDM2 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36869",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_TX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_PRI_TDM_TX_3,
		.stream_name = "Primary TDM3 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36871",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_PRI_TDM_TX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8996_tdm_be_ops,
		.ignore_suspend = 1,
	}

};

static struct snd_soc_dai_link msm8996_hdmi_dai_link[] = {
	/* HDMI BACK END DAI Link */
	{
		.name = LPASS_BE_HDMI,
		.stream_name = "HDMI Playback",
		.cpu_dai_name = "msm-dai-q6-hdmi.8",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-ext-disp-audio-codec-rx",
		.codec_dai_name = "msm_hdmi_audio_codec_rx_dai",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_HDMI_RX,
		.be_hw_params_fixup = msm8996_hdmi_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm8996_tasha_dai_links[
			 ARRAY_SIZE(msm8996_common_dai_links) +
			 ARRAY_SIZE(msm8996_tasha_fe_dai_links) +
			 ARRAY_SIZE(msm8996_tdm_fe_dai_links) +
			 ARRAY_SIZE(msm8996_common_be_dai_links) +
			 ARRAY_SIZE(msm8996_tasha_be_dai_links) +
			 ARRAY_SIZE(msm8996_tdm_be_dai_links) +
			 ARRAY_SIZE(msm8996_hdmi_dai_link)];

static int msm8996_wsa881x_init(struct snd_soc_component *component)
{
	u8 spkleft_ports[WSA881X_MAX_SWR_PORTS] = {100, 101, 102, 106};
	u8 spkright_ports[WSA881X_MAX_SWR_PORTS] = {103, 104, 105, 107};
	unsigned int ch_rate[WSA881X_MAX_SWR_PORTS] = {2400, 600, 300, 1200};
	unsigned int ch_mask[WSA881X_MAX_SWR_PORTS] = {0x1, 0xF, 0x3, 0x3};
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct msm8996_asoc_mach_data *pdata;
	struct snd_soc_dapm_context *dapm;

	if (!codec) {
		pr_err("%s codec is NULL\n", __func__);
		return -EINVAL;
	}

	dapm = snd_soc_codec_get_dapm(codec);

	if (!strcmp(component->name_prefix, "SpkrLeft")) {
		dev_dbg(codec->dev, "%s: setting left ch map to codec %s\n",
			__func__, codec->component.name);
		wsa881x_set_channel_map(codec, &spkleft_ports[0],
				WSA881X_MAX_SWR_PORTS, &ch_mask[0],
				&ch_rate[0]);
		if (dapm->component) {
			snd_soc_dapm_ignore_suspend(dapm, "SpkrLeft IN");
			snd_soc_dapm_ignore_suspend(dapm, "SpkrLeft SPKR");
		}
	} else if (!strcmp(component->name_prefix, "SpkrRight")) {
		dev_dbg(codec->dev, "%s: setting right ch map to codec %s\n",
			__func__, codec->component.name);
		wsa881x_set_channel_map(codec, &spkright_ports[0],
				WSA881X_MAX_SWR_PORTS, &ch_mask[0],
				&ch_rate[0]);
		if (dapm->component) {
			snd_soc_dapm_ignore_suspend(dapm, "SpkrRight IN");
			snd_soc_dapm_ignore_suspend(dapm, "SpkrRight SPKR");
		}
	} else {
		dev_err(codec->dev, "%s: wrong codec name %s\n", __func__,
			codec->component.name);
		return -EINVAL;
	}
	pdata = snd_soc_card_get_drvdata(component->card);
	if (pdata && pdata->codec_root)
		wsa881x_codec_info_create_codec_entry(pdata->codec_root,
						      codec);

	return 0;
}

struct snd_soc_card snd_soc_card_tasha_msm8996 = {
	.name		= "msm8996-tasha-snd-card",
};

static int msm8996_populate_dai_link_component_of_node(
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
				pr_err("%s: No match found for platform name: %s\n",
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

static int msm8996_prepare_us_euro(struct snd_soc_card *card)
{
	struct msm8996_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);
	int ret;
	if (pdata->us_euro_gpio >= 0) {
		dev_dbg(card->dev, "%s: us_euro gpio request %d", __func__,
			pdata->us_euro_gpio);
		ret = gpio_request(pdata->us_euro_gpio, "TASHA_CODEC_US_EURO");
		if (ret) {
			dev_err(card->dev,
				"%s: Failed to request codec US/EURO gpio %d error %d\n",
				__func__, pdata->us_euro_gpio, ret);
			return ret;
		}
	}

	return 0;
}

static int msm8996_prepare_hifi(struct snd_soc_card *card)
{
	struct msm8996_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);
	int ret;

	if (gpio_is_valid(pdata->hph_en1_gpio)) {
		dev_dbg(card->dev, "%s: hph_en1_gpio request %d\n", __func__,
			pdata->hph_en1_gpio);
		ret = gpio_request(pdata->hph_en1_gpio, "hph_en1_gpio");
		if (ret) {
			dev_err(card->dev,
				"%s: hph_en1_gpio request failed, ret:%d\n",
				__func__, ret);
			return ret;
		}
	}
	if (gpio_is_valid(pdata->hph_en0_gpio)) {
		dev_dbg(card->dev, "%s: hph_en0_gpio request %d\n", __func__,
			pdata->hph_en0_gpio);
		ret = gpio_request(pdata->hph_en0_gpio, "hph_en0_gpio");
		if (ret) {
			dev_err(card->dev,
				"%s: hph_en0_gpio request failed, ret:%d\n",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}

static const struct of_device_id msm8996_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,msm8996-asoc-snd-tasha",
	  .data = "tasha_codec"},
	{},
};

static struct snd_soc_card *populate_snd_card_dailinks(struct device *dev)
{
	struct snd_soc_card *card = NULL;
	struct snd_soc_dai_link *dailink = NULL;
	int len_1 = 0, len_2 = 0, len_3 = 0, len_4 = 0, len_5 = 0;
	const struct of_device_id *match;

	match = of_match_node(msm8996_asoc_machine_of_match, dev->of_node);
	if (!match) {
		dev_err(dev, "%s: No DT match found for sound card\n",
			__func__);
		return NULL;
	}

	if (!strcmp(match->data, "tasha_codec")) {
		card = &snd_soc_card_tasha_msm8996;
		len_1 = ARRAY_SIZE(msm8996_common_dai_links);
		len_2 = len_1 + ARRAY_SIZE(msm8996_tasha_fe_dai_links);
		len_3 = len_2 + ARRAY_SIZE(msm8996_tdm_fe_dai_links);
		len_4 = len_3 + ARRAY_SIZE(msm8996_common_be_dai_links);

		memcpy(msm8996_tasha_dai_links,
		       msm8996_common_dai_links,
		       sizeof(msm8996_common_dai_links));
		memcpy(msm8996_tasha_dai_links + len_1,
		       msm8996_tasha_fe_dai_links,
		       sizeof(msm8996_tasha_fe_dai_links));
		memcpy(msm8996_tasha_dai_links + len_2,
		       msm8996_tdm_fe_dai_links,
		       sizeof(msm8996_tdm_fe_dai_links));
		memcpy(msm8996_tasha_dai_links + len_3,
		       msm8996_common_be_dai_links,
		       sizeof(msm8996_common_be_dai_links));
		memcpy(msm8996_tasha_dai_links + len_4,
		       msm8996_tasha_be_dai_links,
		       sizeof(msm8996_tasha_be_dai_links));

		dailink = msm8996_tasha_dai_links;
		len_5 = len_4 + ARRAY_SIZE(msm8996_tasha_be_dai_links);
	}

	if (of_property_read_bool(dev->of_node, "qcom,hdmi-audio-rx")) {
		dev_dbg(dev, "%s(): hdmi audio support present\n",
				__func__);
		memcpy(dailink + len_5, msm8996_hdmi_dai_link,
			sizeof(msm8996_hdmi_dai_link));
		len_5 += ARRAY_SIZE(msm8996_hdmi_dai_link);
	} else {
		dev_dbg(dev, "%s(): No hdmi audio support\n", __func__);
	}
	if (of_property_read_bool(dev->of_node, "qcom,tdm-audio-intf")) {
		dev_dbg(dev, "%s(): TDM support present\n",
				__func__);
		memcpy(dailink + len_5, msm8996_tdm_be_dai_links,
			sizeof(msm8996_tdm_be_dai_links));
		len_5 += ARRAY_SIZE(msm8996_tdm_be_dai_links);
	} else {
		dev_dbg(dev, "%s(): No TDM support\n", __func__);
	}
	if (card) {
		card->dai_link = dailink;
		card->num_links = len_5;
	}

	return card;
}

static int msm8996_init_wsa_dev(struct platform_device *pdev,
				struct snd_soc_card *card)
{
	struct device_node *wsa_of_node;
	u32 wsa_max_devs;
	u32 wsa_dev_cnt;
	char *dev_name_str = NULL;
	struct msm8996_wsa881x_dev_info *wsa881x_dev_info;
	const char *wsa_auxdev_name_prefix[1];
	int found = 0;
	int i;
	int ret;

	/* Get maximum WSA device count for this platform */
	ret = of_property_read_u32(pdev->dev.of_node,
				   "qcom,wsa-max-devs", &wsa_max_devs);
	if (ret) {
		dev_dbg(&pdev->dev,
			 "%s: wsa-max-devs property missing in DT %s, ret = %d\n",
			 __func__, pdev->dev.of_node->full_name, ret);
		return 0;
	}
	if (wsa_max_devs == 0) {
		dev_warn(&pdev->dev,
			 "%s: Max WSA devices is 0 for this target?\n",
			 __func__);
		return 0;
	}

	/* Get count of WSA device phandles for this platform */
	wsa_dev_cnt = of_count_phandle_with_args(pdev->dev.of_node,
						 "qcom,wsa-devs", NULL);
	if (wsa_dev_cnt == -ENOENT) {
		dev_warn(&pdev->dev, "%s: No wsa device defined in DT.\n",
			 __func__);
		return 0;
	} else if (wsa_dev_cnt <= 0) {
		dev_err(&pdev->dev,
			"%s: Error reading wsa device from DT. wsa_dev_cnt = %d\n",
			__func__, wsa_dev_cnt);
		return -EINVAL;
	}

	/*
	 * Expect total phandles count to be NOT less than maximum possible
	 * WSA count. However, if it is less, then assign same value to
	 * max count as well.
	 */
	if (wsa_dev_cnt < wsa_max_devs) {
		dev_dbg(&pdev->dev,
			"%s: wsa_max_devs = %d cannot exceed wsa_dev_cnt = %d\n",
			__func__, wsa_max_devs, wsa_dev_cnt);
		wsa_max_devs = wsa_dev_cnt;
	}

	/* Make sure prefix string passed for each WSA device */
	ret = of_property_count_strings(pdev->dev.of_node,
					"qcom,wsa-aux-dev-prefix");
	if (ret != wsa_dev_cnt) {
		dev_err(&pdev->dev,
			"%s: expecting %d wsa prefix. Defined only %d in DT\n",
			__func__, wsa_dev_cnt, ret);
		return -EINVAL;
	}

	/*
	 * Alloc mem to store phandle and index info of WSA device, if already
	 * registered with ALSA core
	 */
	wsa881x_dev_info = devm_kcalloc(&pdev->dev, wsa_max_devs,
					sizeof(struct msm8996_wsa881x_dev_info),
					GFP_KERNEL);
	if (!wsa881x_dev_info)
		return -ENOMEM;

	/*
	 * search and check whether all WSA devices are already
	 * registered with ALSA core or not. If found a node, store
	 * the node and the index in a local array of struct for later
	 * use.
	 */
	for (i = 0; i < wsa_dev_cnt; i++) {
		wsa_of_node = of_parse_phandle(pdev->dev.of_node,
					    "qcom,wsa-devs", i);
		if (unlikely(!wsa_of_node)) {
			/* we should not be here */
			dev_err(&pdev->dev,
				"%s: wsa dev node is not present\n",
				__func__);
			return -EINVAL;
		}
		if (soc_find_component(wsa_of_node, NULL)) {
			/* WSA device registered with ALSA core */
			wsa881x_dev_info[found].of_node = wsa_of_node;
			wsa881x_dev_info[found].index = i;
			found++;
			if (found == wsa_max_devs)
				break;
		}
	}

	if (found < wsa_max_devs) {
		dev_dbg(&pdev->dev,
			"%s: failed to find %d components. Found only %d\n",
			__func__, wsa_max_devs, found);
		return -EPROBE_DEFER;
	}
	dev_info(&pdev->dev,
		"%s: found %d wsa881x devices registered with ALSA core\n",
		__func__, found);

	card->num_aux_devs = wsa_max_devs;
	card->num_configs = wsa_max_devs;

	/* Alloc array of AUX devs struct */
	msm8996_aux_dev = devm_kcalloc(&pdev->dev, card->num_aux_devs,
				       sizeof(struct snd_soc_aux_dev),
				       GFP_KERNEL);
	if (!msm8996_aux_dev)
		return -ENOMEM;

	/* Alloc array of codec conf struct */
	msm8996_codec_conf = devm_kcalloc(&pdev->dev, card->num_aux_devs,
					  sizeof(struct snd_soc_codec_conf),
					  GFP_KERNEL);
	if (!msm8996_codec_conf)
		return -ENOMEM;

	for (i = 0; i < card->num_aux_devs; i++) {
		dev_name_str = devm_kzalloc(&pdev->dev, DEV_NAME_STR_LEN,
					    GFP_KERNEL);
		if (!dev_name_str)
			return -ENOMEM;

		ret = of_property_read_string_index(pdev->dev.of_node,
						    "qcom,wsa-aux-dev-prefix",
						    wsa881x_dev_info[i].index,
						    wsa_auxdev_name_prefix);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: failed to read wsa aux dev prefix, ret = %d\n",
				__func__, ret);
			return -EINVAL;
		}

		snprintf(dev_name_str, strlen("wsa881x.%d"), "wsa881x.%d", i);
		msm8996_aux_dev[i].name = dev_name_str;
		msm8996_aux_dev[i].codec_name = NULL;
		msm8996_aux_dev[i].codec_of_node =
					wsa881x_dev_info[i].of_node;
		msm8996_aux_dev[i].init = msm8996_wsa881x_init;
		msm8996_codec_conf[i].dev_name = NULL;
		msm8996_codec_conf[i].name_prefix = wsa_auxdev_name_prefix[0];
		msm8996_codec_conf[i].of_node =
					wsa881x_dev_info[i].of_node;
	}
	card->codec_conf = msm8996_codec_conf;
	card->aux_dev = msm8996_aux_dev;

	return 0;
}

static int msm8996_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct msm8996_asoc_mach_data *pdata;
	const char *mbhc_audio_jack_type = NULL;
	char *mclk_freq_prop_name;
	const struct of_device_id *match;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct msm8996_asoc_mach_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Can't allocate msm8996_asoc_mach_data\n");
		return -ENOMEM;
	}

	card = populate_snd_card_dailinks(&pdev->dev);
	if (!card) {
		dev_err(&pdev->dev, "%s: Card uninitialized\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);

	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret) {
		dev_err(&pdev->dev, "parse card name failed, err:%d\n",
			ret);
		goto err;
	}

	ret = snd_soc_of_parse_audio_routing(card, "qcom,audio-routing");
	if (ret) {
		dev_err(&pdev->dev, "parse audio routing failed, err:%d\n",
			ret);
		goto err;
	}

	match = of_match_node(msm8996_asoc_machine_of_match,
			pdev->dev.of_node);
	if (!match) {
		dev_err(&pdev->dev, "%s: no matched codec is found.\n",
			__func__);
		goto err;
	}

	mclk_freq_prop_name = "qcom,tasha-mclk-clk-freq";

	ret = of_property_read_u32(pdev->dev.of_node,
			mclk_freq_prop_name, &pdata->mclk_freq);
	if (ret) {
		dev_err(&pdev->dev,
			"Looking up %s property in node %s failed, err%d\n",
			mclk_freq_prop_name,
			pdev->dev.of_node->full_name, ret);
		goto err;
	}

	if (pdata->mclk_freq != CODEC_EXT_CLK_RATE) {
		dev_err(&pdev->dev, "unsupported mclk freq %u\n",
			pdata->mclk_freq);
		ret = -EINVAL;
		goto err;
	}

	spdev = pdev;

	ret = msm8996_populate_dai_link_component_of_node(card);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}

	ret = msm8996_init_wsa_dev(pdev, card);
	if (ret)
		goto err;

	pdata->hph_en1_gpio = of_get_named_gpio(pdev->dev.of_node,
						"qcom,hph-en1-gpio", 0);
	if (pdata->hph_en1_gpio < 0) {
		dev_dbg(&pdev->dev, "%s: %s property not found %d\n",
			__func__, "qcom,hph-en1-gpio", pdata->hph_en1_gpio);
	}

	pdata->hph_en0_gpio = of_get_named_gpio(pdev->dev.of_node,
						"qcom,hph-en0-gpio", 0);
	if (pdata->hph_en0_gpio < 0) {
		dev_dbg(&pdev->dev, "%s: %s property not found %d\n",
			__func__, "qcom,hph-en0-gpio", pdata->hph_en0_gpio);
	}
	ret = msm8996_prepare_hifi(card);
	if (ret)
		dev_dbg(&pdev->dev, "msm8996_prepare_hifi failed (%d)\n",
			ret);

	ret = snd_soc_register_card(card);
	if (ret == -EPROBE_DEFER) {
		if (codec_reg_done)
			ret = -EINVAL;
		goto err;
	} else if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}
	dev_info(&pdev->dev, "Sound card %s registered\n", card->name);

	ret = of_property_read_string(pdev->dev.of_node,
		"qcom,mbhc-audio-jack-type", &mbhc_audio_jack_type);
	if (ret) {
		dev_dbg(&pdev->dev, "Looking up %s property in node %s failed",
			"qcom,mbhc-audio-jack-type",
			pdev->dev.of_node->full_name);
		dev_dbg(&pdev->dev, "Jack type properties set to default");
	} else {
		if (!strcmp(mbhc_audio_jack_type, "4-pole-jack")) {
			dev_dbg(&pdev->dev, "This hardware has 4 pole jack");
		} else if (!strcmp(mbhc_audio_jack_type, "5-pole-jack")) {
			dev_dbg(&pdev->dev, "This hardware has 5 pole jack");
		} else if (!strcmp(mbhc_audio_jack_type, "6-pole-jack")) {
			dev_dbg(&pdev->dev, "This hardware has 6 pole jack");
		} else {
			dev_dbg(&pdev->dev, "Unknown value, set to default");
		}
	}
	/*
	 * Parse US-Euro gpio info from DT. Report no error if us-euro
	 * entry is not found in DT file as some targets do not support
	 * US-Euro detection
	 */
	pdata->us_euro_gpio = of_get_named_gpio(pdev->dev.of_node,
				"qcom,us-euro-gpios", 0);
	if (pdata->us_euro_gpio < 0) {
		dev_info(&pdev->dev, "property %s not detected in node %s",
			"qcom,us-euro-gpios",
			pdev->dev.of_node->full_name);
	} else {
		dev_dbg(&pdev->dev, "%s detected %d",
			"qcom,us-euro-gpios", pdata->us_euro_gpio);
		wcd_mbhc_cfg.swap_gnd_mic = msm8996_swap_gnd_mic;
	}

	ret = msm8996_prepare_us_euro(card);
	if (ret)
		dev_info(&pdev->dev, "msm8996_prepare_us_euro failed (%d)\n",
			ret);
	return 0;
err:
	if (pdata->us_euro_gpio > 0) {
		dev_dbg(&pdev->dev, "%s free us_euro gpio %d\n",
			__func__, pdata->us_euro_gpio);
		gpio_free(pdata->us_euro_gpio);
		pdata->us_euro_gpio = 0;
	}
	if (pdata->hph_en1_gpio > 0) {
		dev_dbg(&pdev->dev, "%s free hph_en1_gpio %d\n",
			__func__, pdata->hph_en1_gpio);
		gpio_free(pdata->hph_en1_gpio);
		pdata->hph_en1_gpio = 0;
	}
	if (pdata->hph_en0_gpio > 0) {
		dev_dbg(&pdev->dev, "%s free hph_en0_gpio %d\n",
			__func__, pdata->hph_en0_gpio);
		gpio_free(pdata->hph_en0_gpio);
		pdata->hph_en0_gpio = 0;
	}
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int msm8996_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm8996_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);

	if (gpio_is_valid(ext_us_amp_gpio))
		gpio_free(ext_us_amp_gpio);

	gpio_free(pdata->us_euro_gpio);
	gpio_free(pdata->hph_en1_gpio);
	gpio_free(pdata->hph_en0_gpio);

	if (msm8996_liquid_dock_dev != NULL) {
		switch_dev_unregister(&msm8996_liquid_dock_dev->audio_sdev);

		if (msm8996_liquid_dock_dev->dock_plug_irq)
			free_irq(msm8996_liquid_dock_dev->dock_plug_irq,
				 msm8996_liquid_dock_dev);

		if (msm8996_liquid_dock_dev->dock_plug_gpio)
			gpio_free(msm8996_liquid_dock_dev->dock_plug_gpio);

		kfree(msm8996_liquid_dock_dev);
		msm8996_liquid_dock_dev = NULL;
	}
	snd_soc_unregister_card(card);

	return 0;
}

static struct platform_driver msm8996_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = msm8996_asoc_machine_of_match,
	},
	.probe = msm8996_asoc_machine_probe,
	.remove = msm8996_asoc_machine_remove,
};
module_platform_driver(msm8996_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, msm8996_asoc_machine_of_match);
