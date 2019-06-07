/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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
#include <linux/of_device.h>
#include <linux/mfd/msm-cdc-pinctrl.h>
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
#include <linux/qdsp6v2/audio_notifier.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "../codecs/wcd9335.h"
#include "../codecs/wcd934x/wcd934x.h"
#include "../codecs/wcd934x/wcd934x-mbhc.h"
#include "../codecs/wsa881x.h"

#define DRV_NAME "msm8998-asoc-snd"

#define __CHIPSET__ "MSM8998 "
#define MSM_DAILINK_NAME(name) (__CHIPSET__#name)

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
#define SAMPLING_RATE_352P8KHZ  352800
#define SAMPLING_RATE_384KHZ    384000

#define WCD9XXX_MBHC_DEF_BUTTONS    8
#define WCD9XXX_MBHC_DEF_RLOADS     5
#define CODEC_EXT_CLK_RATE          9600000
#define ADSP_STATE_READY_TIMEOUT_MS 3000
#define DEV_NAME_STR_LEN            32

#define WSA8810_NAME_1 "wsa881x.20170211"
#define WSA8810_NAME_2 "wsa881x.20170212"

#define WCN_CDC_SLIM_RX_CH_MAX 2
#define WCN_CDC_SLIM_TX_CH_MAX 3

#define TDM_CHANNEL_MAX 16
#define TDM_SLOT_OFFSET_MAX 32

#define MSM_HIFI_ON 1

enum {
	SLIM_RX_0 = 0,
	SLIM_RX_1,
	SLIM_RX_2,
	SLIM_RX_3,
	SLIM_RX_4,
	SLIM_RX_5,
	SLIM_RX_6,
	SLIM_RX_7,
	SLIM_RX_MAX,
};

enum {
	SLIM_TX_0 = 0,
	SLIM_TX_1,
	SLIM_TX_2,
	SLIM_TX_3,
	SLIM_TX_4,
	SLIM_TX_5,
	SLIM_TX_6,
	SLIM_TX_7,
	SLIM_TX_8,
	SLIM_TX_MAX,
};

enum {
	PRIM_MI2S = 0,
	SEC_MI2S,
	TERT_MI2S,
	QUAT_MI2S,
	MI2S_MAX,
};

enum {
	PRIM_AUX_PCM = 0,
	SEC_AUX_PCM,
	TERT_AUX_PCM,
	QUAT_AUX_PCM,
	AUX_PCM_MAX,
};

enum {
	PCM_I2S_SEL_PRIM = 0,
	PCM_I2S_SEL_SEC,
	PCM_I2S_SEL_TERT,
	PCM_I2S_SEL_QUAT,
	PCM_I2S_SEL_MAX,
};

struct mi2s_aux_pcm_common_conf {
	struct mutex lock;
	void *pcm_i2s_sel_vt_addr;
};

struct mi2s_conf {
	struct mutex lock;
	u32 ref_cnt;
	u32 msm_is_mi2s_master;
};

struct auxpcm_conf {
	struct mutex lock;
	u32 ref_cnt;
};

struct dev_config {
	u32 sample_rate;
	u32 bit_format;
	u32 channels;
};

enum {
	HDMI_RX_IDX = 0,
	DP_RX_IDX,
	EXT_DISP_RX_IDX_MAX,
};

struct msm_wsa881x_dev_info {
	struct device_node *of_node;
	u32 index;
};

enum pinctrl_pin_state {
	STATE_DISABLE = 0, /* All pins are in sleep state */
	STATE_MI2S_ACTIVE,  /* IS2 = active, TDM = sleep */
	STATE_TDM_ACTIVE,  /* IS2 = sleep, TDM = active */
};

struct msm_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *mi2s_disable;
	struct pinctrl_state *tdm_disable;
	struct pinctrl_state *mi2s_active;
	struct pinctrl_state *tdm_active;
	enum pinctrl_pin_state curr_state;
};

struct msm_asoc_mach_data {
	u32 mclk_freq;
	int us_euro_gpio; /* used by gpio driver API */
	struct device_node *us_euro_gpio_p; /* used by pinctrl API */
	struct device_node *hph_en1_gpio_p; /* used by pinctrl API */
	struct device_node *hph_en0_gpio_p; /* used by pinctrl API */
	struct snd_info_entry *codec_root;
	struct msm_pinctrl_info pinctrl_info;
};

struct msm_asoc_wcd93xx_codec {
	void* (*get_afe_config_fn)(struct snd_soc_codec *codec,
				   enum afe_config_type config_type);
	void (*mbhc_hs_detect_exit)(struct snd_soc_codec *codec);
};

static const char *const pin_states[] = {"sleep", "i2s-active",
					 "tdm-active"};

enum {
	TDM_0 = 0,
	TDM_1,
	TDM_2,
	TDM_3,
	TDM_4,
	TDM_5,
	TDM_6,
	TDM_7,
	TDM_PORT_MAX,
};

enum {
	TDM_PRI = 0,
	TDM_SEC,
	TDM_TERT,
	TDM_QUAT,
	TDM_INTERFACE_MAX,
};

struct tdm_port {
	u32 mode;
	u32 channel;
};

/* TDM default config */
static struct dev_config tdm_rx_cfg[TDM_INTERFACE_MAX][TDM_PORT_MAX] = {
	{ /* PRI TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	},
	{ /* SEC TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	},
	{ /* TERT TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	},
	{ /* QUAT TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	}
};

/* TDM default config */
static struct dev_config tdm_tx_cfg[TDM_INTERFACE_MAX][TDM_PORT_MAX] = {
	{ /* PRI TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	},
	{ /* SEC TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	},
	{ /* TERT TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	},
	{ /* QUAT TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	}
};

/*TDM default offset currently only supporting TDM_RX_0 and TDM_TX_0 */
static unsigned int tdm_slot_offset[TDM_PORT_MAX][TDM_SLOT_OFFSET_MAX] = {
	{0, 4, 8, 12, 16, 20, 24, 28},/* TX_0 | RX_0 */
	{AFE_SLOT_MAPPING_OFFSET_INVALID},/* TX_1 | RX_1 */
	{AFE_SLOT_MAPPING_OFFSET_INVALID},/* TX_2 | RX_2 */
	{AFE_SLOT_MAPPING_OFFSET_INVALID},/* TX_3 | RX_3 */
	{AFE_SLOT_MAPPING_OFFSET_INVALID},/* TX_4 | RX_4 */
	{AFE_SLOT_MAPPING_OFFSET_INVALID},/* TX_5 | RX_5 */
	{AFE_SLOT_MAPPING_OFFSET_INVALID},/* TX_6 | RX_6 */
	{AFE_SLOT_MAPPING_OFFSET_INVALID},/* TX_7 | RX_7 */
};

/* Default configuration of slimbus channels */
static struct dev_config slim_rx_cfg[] = {
	[SLIM_RX_0] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_1] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_2] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_3] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_4] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_5] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_6] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_RX_7] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};

static struct dev_config slim_tx_cfg[] = {
	[SLIM_TX_0] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_1] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_2] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_3] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_4] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_5] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_6] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_7] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SLIM_TX_8] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
};


/* Default configuration of external display BE */
static struct dev_config ext_disp_rx_cfg[] = {
	[HDMI_RX_IDX] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[DP_RX_IDX] =   {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
};

static struct dev_config usb_rx_cfg = {
	.sample_rate = SAMPLING_RATE_48KHZ,
	.bit_format = SNDRV_PCM_FORMAT_S16_LE,
	.channels = 2,
};

static struct dev_config usb_tx_cfg = {
	.sample_rate = SAMPLING_RATE_48KHZ,
	.bit_format = SNDRV_PCM_FORMAT_S16_LE,
	.channels = 1,
};

static struct dev_config proxy_rx_cfg = {
	.sample_rate = SAMPLING_RATE_48KHZ,
	.bit_format = SNDRV_PCM_FORMAT_S16_LE,
	.channels = 2,
};

/* Default configuration of MI2S channels */
static struct dev_config mi2s_rx_cfg[] = {
	[PRIM_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[SEC_MI2S]  = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[TERT_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[QUAT_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
};

static struct dev_config mi2s_tx_cfg[] = {
	[PRIM_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SEC_MI2S]  = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[TERT_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUAT_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};

static struct dev_config aux_pcm_rx_cfg[] = {
	[PRIM_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SEC_AUX_PCM]  = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[TERT_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUAT_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};

static struct dev_config aux_pcm_tx_cfg[] = {
	[PRIM_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SEC_AUX_PCM]  = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[TERT_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUAT_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};

/* TDM default slot config */
struct tdm_slot_cfg {
	u32 width;
	u32 num;
};

static struct tdm_slot_cfg tdm_slot[TDM_INTERFACE_MAX] = {
	/* PRI TDM */
	{32, 8},
	/* SEC TDM */
	{32, 8},
	/* TERT TDM */
	{32, 8},
	/* QUAT TDM */
	{32, 8}
};

static unsigned int tdm_rx_slot_offset
	[TDM_INTERFACE_MAX][TDM_PORT_MAX][TDM_SLOT_OFFSET_MAX] = {
	{/* PRI TDM */
		{0, 4, 8, 12, 16, 20, 24, 28,
			32, 36, 40, 44, 48, 52, 56, 60, 0xFFFF},
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* SEC TDM */
		{0, 4, 8, 12, 16, 20, 24, 28,
			32, 36, 40, 44, 48, 52, 56, 60, 0xFFFF},
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* TERT TDM */
		{0, 4, 8, 12, 16, 20, 24, 28,
			32, 36, 40, 44, 48, 52, 56, 60, 0xFFFF},
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* QUAT TDM */
		{0, 4, 8, 12, 16, 20, 24, 28,
			32, 36, 40, 44, 48, 52, 56, 60, 0xFFFF},
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	}
};

static unsigned int tdm_tx_slot_offset
	[TDM_INTERFACE_MAX][TDM_PORT_MAX][TDM_SLOT_OFFSET_MAX] = {
	{/* PRI TDM */
		{0, 4, 8, 12, 16, 20, 24, 28,
			32, 36, 40, 44, 48, 52, 56, 60, 0xFFFF},
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* SEC TDM */
		{0, 4, 8, 12, 16, 20, 24, 28,
			32, 36, 40, 44, 48, 52, 56, 60, 0xFFFF},
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* TERT TDM */
		{0, 4, 8, 12, 16, 20, 24, 28,
			32, 36, 40, 44, 48, 52, 56, 60, 0xFFFF},
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* QUAT TDM */
		{0, 4, 8, 12, 16, 20, 24, 28,
			32, 36, 40, 44, 48, 52, 56, 60, 0xFFFF},/*MIC ARR*/
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	}
};
static int msm_vi_feed_tx_ch = 2;
static const char *const slim_rx_ch_text[] = {"One", "Two", "Three", "Four",
						"Five", "Six", "Seven",
						"Eight"};
static const char *const slim_tx_ch_text[] = {"One", "Two", "Three", "Four",
						"Five", "Six", "Seven",
						"Eight"};
static const char *const vi_feed_ch_text[] = {"One", "Two"};
static char const *bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE",
					  "S32_LE"};
static char const *ext_disp_bit_format_text[] = {"S16_LE", "S24_LE"};
static char const *slim_sample_rate_text[] = {"KHZ_8", "KHZ_16",
					"KHZ_32", "KHZ_44P1", "KHZ_48",
					"KHZ_88P2", "KHZ_96", "KHZ_176P4",
					"KHZ_192", "KHZ_352P8", "KHZ_384"};
static char const *bt_sample_rate_text[] = {"KHZ_8", "KHZ_16", "KHZ_48"};
static char const *bt_sample_rate_rx_text[] = {"KHZ_8", "KHZ_16", "KHZ_48"};
static char const *bt_sample_rate_tx_text[] = {"KHZ_8", "KHZ_16", "KHZ_48"};
static const char *const usb_ch_text[] = {"One", "Two", "Three", "Four",
					   "Five", "Six", "Seven",
					   "Eight"};
static char const *ch_text[] = {"Two", "Three", "Four", "Five",
					"Six", "Seven", "Eight"};
static char const *usb_sample_rate_text[] = {"KHZ_8", "KHZ_11P025",
					"KHZ_16", "KHZ_22P05",
					"KHZ_32", "KHZ_44P1", "KHZ_48",
					"KHZ_88P2", "KHZ_96", "KHZ_176P4",
					"KHZ_192", "KHZ_352P8", "KHZ_384"};
static char const *ext_disp_sample_rate_text[] = {"KHZ_48", "KHZ_96",
					"KHZ_192", "KHZ_32", "KHZ_44P1",
					"KHZ_88P2", "KHZ_176P4"};
static char const *tdm_ch_text[] = {"One", "Two", "Three", "Four",
					"Five", "Six", "Seven", "Eight",
					"Nine", "Ten", "Eleven", "Twelve",
					"Thirteen", "Fourteen", "Fifteen",
					"Sixteen"};
static char const *tdm_bit_format_text[] = {"S16_LE", "S24_LE", "S32_LE"};
static char const *tdm_sample_rate_text[] = {"KHZ_8", "KHZ_16", "KHZ_32",
					     "KHZ_44P1", "KHZ_48", "KHZ_96",
					     "KHZ_192", "KHZ_352P8", "KHZ_384"};
static const char *const tdm_slot_num_text[] = {"One", "Two", "Four",
	"Eight", "Sixteen", "ThirtyTwo"};
static const char *const tdm_slot_width_text[] = {"16", "24", "32"};
static const char *const auxpcm_rate_text[] = {"KHZ_8", "KHZ_16"};
static char const *mi2s_rate_text[] = {"KHZ_8", "KHZ_16",
				      "KHZ_32", "KHZ_44P1", "KHZ_48",
				      "KHZ_88P2", "KHZ_96", "KHZ_176P4",
				      "KHZ_192"};
static const char *const mi2s_ch_text[] = {"One", "Two", "Three", "Four",
					   "Five", "Six", "Seven",
					   "Eight"};
static const char *const hifi_text[] = {"Off", "On"};

static SOC_ENUM_SINGLE_EXT_DECL(slim_0_rx_chs, slim_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_2_rx_chs, slim_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_0_tx_chs, slim_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_1_tx_chs, slim_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_5_rx_chs, slim_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_6_rx_chs, slim_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_rx_chs, usb_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_tx_chs, usb_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(vi_feed_tx_chs, vi_feed_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(ext_disp_rx_chs, ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(proxy_rx_chs, ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_0_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_5_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_6_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_0_tx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_tx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(ext_disp_rx_format, ext_disp_bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_0_rx_sample_rate, slim_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_2_rx_sample_rate, slim_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_0_tx_sample_rate, slim_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_5_rx_sample_rate, slim_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(slim_6_rx_sample_rate, slim_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(bt_sample_rate, bt_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(bt_sample_rate_rx, bt_sample_rate_rx_text);
static SOC_ENUM_SINGLE_EXT_DECL(bt_sample_rate_tx, bt_sample_rate_tx_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_rx_sample_rate, usb_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_tx_sample_rate, usb_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(ext_disp_rx_sample_rate,
				ext_disp_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_tx_chs, tdm_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_tx_format, tdm_bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_tx_sample_rate, tdm_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_rx_chs, tdm_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_rx_format, tdm_bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_rx_sample_rate, tdm_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_slot_num, tdm_slot_num_text);
static SOC_ENUM_SINGLE_EXT_DECL(tdm_slot_width, tdm_slot_width_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(mi2s_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(mi2s_tx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(hifi_function, hifi_text);

static struct platform_device *spdev;
static int msm_hifi_control;

static bool is_initial_boot;
static bool codec_reg_done;
static struct snd_soc_aux_dev *msm_aux_dev;
static struct snd_soc_codec_conf *msm_codec_conf;
static struct msm_asoc_wcd93xx_codec msm_codec_fn;

static void *def_tasha_mbhc_cal(void);
static void *def_tavil_mbhc_cal(void);
static int msm_snd_enable_codec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm);
static int msm_wsa881x_init(struct snd_soc_component *component);

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

static struct snd_soc_dapm_route wcd_audio_paths_tasha[] = {
	{"MIC BIAS1", NULL, "MCLK TX"},
	{"MIC BIAS2", NULL, "MCLK TX"},
	{"MIC BIAS3", NULL, "MCLK TX"},
	{"MIC BIAS4", NULL, "MCLK TX"},
};

static struct snd_soc_dapm_route wcd_audio_paths[] = {
	{"MIC BIAS1", NULL, "MCLK"},
	{"MIC BIAS2", NULL, "MCLK"},
	{"MIC BIAS3", NULL, "MCLK"},
	{"MIC BIAS4", NULL, "MCLK"},
};

static u32 mi2s_ebit_clk[MI2S_MAX] = {
	Q6AFE_LPASS_CLK_ID_PRI_MI2S_EBIT,
	Q6AFE_LPASS_CLK_ID_SEC_MI2S_EBIT,
	Q6AFE_LPASS_CLK_ID_TER_MI2S_EBIT,
	Q6AFE_LPASS_CLK_ID_QUAD_MI2S_EBIT,
};

static struct afe_clk_set mi2s_clk[MI2S_MAX] = {
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_PRI_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_SEC_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_TER_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_QUAD_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	}
};

static struct mi2s_aux_pcm_common_conf mi2s_auxpcm_conf[PCM_I2S_SEL_MAX];
static struct mi2s_conf mi2s_intf_conf[MI2S_MAX];
static struct auxpcm_conf auxpcm_intf_conf[AUX_PCM_MAX];

static int slim_get_sample_rate_val(int sample_rate)
{
	int sample_rate_val = 0;

	switch (sample_rate) {
	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 0;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 8;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 9;
		break;
	case SAMPLING_RATE_384KHZ:
		sample_rate_val = 10;
		break;
	default:
		sample_rate_val = 4;
		break;
	}
	return sample_rate_val;
}

static int slim_get_sample_rate(int value)
{
	int sample_rate = 0;

	switch (value) {
	case 0:
		sample_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 3:
		sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 4:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 5:
		sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 6:
		sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 7:
		sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 8:
		sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 9:
		sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
	case 10:
		sample_rate = SAMPLING_RATE_384KHZ;
		break;
	default:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	return sample_rate;
}

static int slim_get_bit_format_val(int bit_format)
{
	int val = 0;

	switch (bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		val = 3;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		val = 2;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		val = 1;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		val = 0;
		break;
	}
	return val;
}

static int slim_get_bit_format(int val)
{
	int bit_fmt = SNDRV_PCM_FORMAT_S16_LE;

	switch (val) {
	case 0:
		bit_fmt = SNDRV_PCM_FORMAT_S16_LE;
		break;
	case 1:
		bit_fmt = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 2:
		bit_fmt = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 3:
		bit_fmt = SNDRV_PCM_FORMAT_S32_LE;
		break;
	default:
		bit_fmt = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return bit_fmt;
}

static int slim_get_port_idx(struct snd_kcontrol *kcontrol)
{
	int port_id = 0;

	if (strnstr(kcontrol->id.name, "SLIM_0_RX", sizeof("SLIM_0_RX")))
		port_id = SLIM_RX_0;
	else if (strnstr(kcontrol->id.name, "SLIM_2_RX", sizeof("SLIM_2_RX")))
		port_id = SLIM_RX_2;
	else if (strnstr(kcontrol->id.name, "SLIM_5_RX", sizeof("SLIM_5_RX")))
		port_id = SLIM_RX_5;
	else if (strnstr(kcontrol->id.name, "SLIM_6_RX", sizeof("SLIM_6_RX")))
		port_id = SLIM_RX_6;
	else if (strnstr(kcontrol->id.name, "SLIM_0_TX", sizeof("SLIM_0_TX")))
		port_id = SLIM_TX_0;
	else if (strnstr(kcontrol->id.name, "SLIM_1_TX", sizeof("SLIM_1_TX")))
		port_id = SLIM_TX_1;
	else {
		pr_err("%s: unsupported channel: %s",
			__func__, kcontrol->id.name);
		return -EINVAL;
	}

	return port_id;
}

static int slim_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	ucontrol->value.enumerated.item[0] =
		slim_get_sample_rate_val(slim_rx_cfg[ch_num].sample_rate);

	pr_debug("%s: slim[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 ch_num, slim_rx_cfg[ch_num].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	slim_rx_cfg[ch_num].sample_rate =
		slim_get_sample_rate(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: slim[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 ch_num, slim_rx_cfg[ch_num].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	ucontrol->value.enumerated.item[0] =
		slim_get_sample_rate_val(slim_tx_cfg[ch_num].sample_rate);

	pr_debug("%s: slim[%d]_tx_sample_rate = %d, item = %d\n", __func__,
		 ch_num, slim_tx_cfg[ch_num].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate = 0;
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	sample_rate = slim_get_sample_rate(ucontrol->value.enumerated.item[0]);
	if (sample_rate == SAMPLING_RATE_44P1KHZ) {
		pr_err("%s: Unsupported sample rate %d: for Tx path\n",
			__func__, sample_rate);
		return -EINVAL;
	}
	slim_tx_cfg[ch_num].sample_rate = sample_rate;

	pr_debug("%s: slim[%d]_tx_sample_rate = %d, value = %d\n", __func__,
		 ch_num, slim_tx_cfg[ch_num].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_rx_bit_format_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	ucontrol->value.enumerated.item[0] =
			slim_get_bit_format_val(slim_rx_cfg[ch_num].bit_format);

	pr_debug("%s: slim[%d]_rx_bit_format = %d, ucontrol value = %d\n",
		 __func__, ch_num, slim_rx_cfg[ch_num].bit_format,
			ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_rx_bit_format_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	slim_rx_cfg[ch_num].bit_format =
		slim_get_bit_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: slim[%d]_rx_bit_format = %d, ucontrol value = %d\n",
		 __func__, ch_num, slim_rx_cfg[ch_num].bit_format,
			ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_tx_bit_format_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	ucontrol->value.enumerated.item[0] =
			slim_get_bit_format_val(slim_tx_cfg[ch_num].bit_format);

	pr_debug("%s: slim[%d]_tx_bit_format = %d, ucontrol value = %d\n",
		 __func__, ch_num, slim_tx_cfg[ch_num].bit_format,
			ucontrol->value.enumerated.item[0]);

	return 0;
}

static int slim_tx_bit_format_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	slim_tx_cfg[ch_num].bit_format =
		slim_get_bit_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: slim[%d]_tx_bit_format = %d, ucontrol value = %d\n",
		 __func__, ch_num, slim_tx_cfg[ch_num].bit_format,
			ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_slim_rx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	pr_debug("%s: msm_slim_[%d]_rx_ch  = %d\n", __func__,
		 ch_num, slim_rx_cfg[ch_num].channels);
	ucontrol->value.enumerated.item[0] = slim_rx_cfg[ch_num].channels - 1;

	return 0;
}

static int msm_slim_rx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	slim_rx_cfg[ch_num].channels = ucontrol->value.enumerated.item[0] + 1;
	pr_debug("%s: msm_slim_[%d]_rx_ch  = %d\n", __func__,
		 ch_num, slim_rx_cfg[ch_num].channels);

	return 1;
}

static int msm_slim_tx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	pr_debug("%s: msm_slim_[%d]_tx_ch  = %d\n", __func__,
		 ch_num, slim_tx_cfg[ch_num].channels);
	ucontrol->value.enumerated.item[0] = slim_tx_cfg[ch_num].channels - 1;

	return 0;
}

static int msm_slim_tx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = slim_get_port_idx(kcontrol);

	if (ch_num < 0)
		return ch_num;

	slim_tx_cfg[ch_num].channels = ucontrol->value.enumerated.item[0] + 1;
	pr_debug("%s: msm_slim_[%d]_tx_ch = %d\n", __func__,
		 ch_num, slim_tx_cfg[ch_num].channels);

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

static int msm_bt_sample_rate_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	/*
	 * Slimbus_7_Rx/Tx sample rate values should always be in sync (same)
	 * when used for BT_SCO use case. Return either Rx or Tx sample rate
	 * value.
	 */
	switch (slim_rx_cfg[SLIM_RX_7].sample_rate) {
	case SAMPLING_RATE_48KHZ:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SAMPLING_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SAMPLING_RATE_8KHZ:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: sample rate = %d", __func__,
		 slim_rx_cfg[SLIM_RX_7].sample_rate);

	return 0;
}

static int msm_bt_sample_rate_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_16KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_48KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 0:
	default:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_8KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	pr_debug("%s: sample rates: slim7_rx = %d, slim7_tx = %d, value = %d\n",
		 __func__,
		 slim_rx_cfg[SLIM_RX_7].sample_rate,
		 slim_tx_cfg[SLIM_TX_7].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_bt_sample_rate_rx_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	switch (slim_rx_cfg[SLIM_RX_7].sample_rate) {
	case SAMPLING_RATE_48KHZ:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SAMPLING_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SAMPLING_RATE_8KHZ:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: sample rate = %d", __func__,
		slim_rx_cfg[SLIM_RX_7].sample_rate);

	return 0;
}

static int msm_bt_sample_rate_rx_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 0:
	default:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	pr_debug("%s: sample rates: slim7_rx = %d, value = %d\n",
		 __func__,
		slim_rx_cfg[SLIM_RX_7].sample_rate,
		ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_bt_sample_rate_tx_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	switch (slim_tx_cfg[SLIM_TX_7].sample_rate) {
	case SAMPLING_RATE_48KHZ:
		ucontrol->value.integer.value[0] = 2;
		break;
	case SAMPLING_RATE_16KHZ:
		ucontrol->value.integer.value[0] = 1;
		break;
	case SAMPLING_RATE_8KHZ:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}
	pr_debug("%s: sample rate = %d", __func__,
		slim_tx_cfg[SLIM_TX_7].sample_rate);

	return 0;
}

static int msm_bt_sample_rate_tx_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	switch (ucontrol->value.integer.value[0]) {
	case 1:
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 0:
	default:
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	pr_debug("%s: sample rates: slim7_tx = %d, value = %d\n",
		 __func__,
		slim_tx_cfg[SLIM_TX_7].sample_rate,
		ucontrol->value.enumerated.item[0]);

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
	int sample_rate_val;

	switch (usb_rx_cfg.sample_rate) {
	case SAMPLING_RATE_384KHZ:
		sample_rate_val = 12;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 11;
		break;
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
	case 12:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_384KHZ;
		break;
	case 11:
		usb_rx_cfg.sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
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
	case 3:
		usb_rx_cfg.bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
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
	int sample_rate_val;

	switch (usb_tx_cfg.sample_rate) {
	case SAMPLING_RATE_384KHZ:
		sample_rate_val = 12;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 11;
		break;
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
	case 12:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_384KHZ;
		break;
	case 11:
		usb_tx_cfg.sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
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
	case 3:
		usb_tx_cfg.bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
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

static int ext_disp_get_port_idx(struct snd_kcontrol *kcontrol)
{
	int idx;

	if (strnstr(kcontrol->id.name, "HDMI_RX", sizeof("HDMI_RX")))
		idx = HDMI_RX_IDX;
	else if (strnstr(kcontrol->id.name, "Display Port RX",
			 sizeof("Display Port RX")))
		idx = DP_RX_IDX;
	else {
		pr_err("%s: unsupported BE: %s",
			__func__, kcontrol->id.name);
		idx = -EINVAL;
	}

	return idx;
}

static int ext_disp_rx_format_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int idx = ext_disp_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	switch (ext_disp_rx_cfg[idx].bit_format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		ucontrol->value.integer.value[0] = 1;
		break;

	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		ucontrol->value.integer.value[0] = 0;
		break;
	}

	pr_debug("%s: ext_disp_rx[%d].format = %d, ucontrol value = %ld\n",
		 __func__, idx, ext_disp_rx_cfg[idx].bit_format,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int ext_disp_rx_format_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int idx = ext_disp_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	switch (ucontrol->value.integer.value[0]) {
	case 1:
		ext_disp_rx_cfg[idx].bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		ext_disp_rx_cfg[idx].bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: ext_disp_rx[%d].format = %d, ucontrol value = %ld\n",
		 __func__, idx, ext_disp_rx_cfg[idx].bit_format,
		 ucontrol->value.integer.value[0]);

	return 0;
}

static int ext_disp_rx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int idx = ext_disp_get_port_idx(kcontrol);

	 if (idx < 0)
		return idx;

	ucontrol->value.integer.value[0] =
			ext_disp_rx_cfg[idx].channels - 2;

	pr_debug("%s: ext_disp_rx[%d].ch = %d\n", __func__,
		 idx, ext_disp_rx_cfg[idx].channels);

	return 0;
}

static int ext_disp_rx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int idx = ext_disp_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ext_disp_rx_cfg[idx].channels =
			ucontrol->value.integer.value[0] + 2;

	pr_debug("%s: ext_disp_rx[%d].ch = %d\n", __func__,
		 idx, ext_disp_rx_cfg[idx].channels);
	return 1;
}

static int ext_disp_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val;
	int idx = ext_disp_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	switch (ext_disp_rx_cfg[idx].sample_rate) {
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 6;
		break;

	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 5;
		break;

	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 4;
		break;

	case SAMPLING_RATE_32KHZ:
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
	pr_debug("%s: ext_disp_rx[%d].sample_rate = %d\n", __func__,
		 idx, ext_disp_rx_cfg[idx].sample_rate);

	return 0;
}

static int ext_disp_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	int idx = ext_disp_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	switch (ucontrol->value.integer.value[0]) {
	case 6:
		ext_disp_rx_cfg[idx].sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 5:
		ext_disp_rx_cfg[idx].sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 4:
		ext_disp_rx_cfg[idx].sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 3:
		ext_disp_rx_cfg[idx].sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 2:
		ext_disp_rx_cfg[idx].sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 1:
		ext_disp_rx_cfg[idx].sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		ext_disp_rx_cfg[idx].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}

	pr_debug("%s: control value = %ld, ext_disp_rx[%d].sample_rate = %d\n",
		 __func__, ucontrol->value.integer.value[0], idx,
		 ext_disp_rx_cfg[idx].sample_rate);
	return 0;
}

static int proxy_rx_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: proxy_rx channels = %d\n",
		 __func__, proxy_rx_cfg.channels);
	ucontrol->value.integer.value[0] = proxy_rx_cfg.channels - 2;

	return 0;
}

static int proxy_rx_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	proxy_rx_cfg.channels = ucontrol->value.integer.value[0] + 2;
	pr_debug("%s: proxy_rx channels = %d\n",
		 __func__, proxy_rx_cfg.channels);

	return 1;
}

static int tdm_get_sample_rate(int value)
{
	int sample_rate = 0;

	switch (value) {
	case 0:
		sample_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 3:
		sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 4:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 5:
		sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 6:
		sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 7:
		sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
	case 8:
		sample_rate = SAMPLING_RATE_384KHZ;
		break;
	default:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	return sample_rate;
}

static int aux_pcm_get_sample_rate(int value)
{
	int sample_rate;

	switch (value) {
	case 1:
		sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 0:
	default:
		sample_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	return sample_rate;
}

static int tdm_get_sample_rate_val(int sample_rate)
{
	int sample_rate_val = 0;

	switch (sample_rate) {
	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 0;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_384KHZ:
		sample_rate_val = 8;
		break;
	default:
		sample_rate_val = 4;
		break;
	}
	return sample_rate_val;
}

static int aux_pcm_get_sample_rate_val(int sample_rate)
{
	int sample_rate_val;

	switch (sample_rate) {
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_8KHZ:
	default:
		sample_rate_val = 0;
		break;
	}
	return sample_rate_val;
}

static int tdm_get_mode(struct snd_kcontrol *kcontrol)
{
	int mode;

	if (strnstr(kcontrol->id.name, "PRI",
	    sizeof(kcontrol->id.name))) {
		mode = TDM_PRI;
	} else if (strnstr(kcontrol->id.name, "SEC",
	    sizeof(kcontrol->id.name))) {
		mode = TDM_SEC;
	} else if (strnstr(kcontrol->id.name, "TERT",
	    sizeof(kcontrol->id.name))) {
		mode = TDM_TERT;
	} else if (strnstr(kcontrol->id.name, "QUAT",
	    sizeof(kcontrol->id.name))) {
		mode = TDM_QUAT;
	} else {
		pr_err("%s: unsupported mode in: %s",
			__func__, kcontrol->id.name);
		mode = -EINVAL;
	}

	return mode;
}

static int tdm_get_channel(struct snd_kcontrol *kcontrol)
{
	int channel;

	if (strnstr(kcontrol->id.name, "RX_0",
	    sizeof(kcontrol->id.name)) ||
	    strnstr(kcontrol->id.name, "TX_0",
	    sizeof(kcontrol->id.name))) {
		channel = TDM_0;
	} else if (strnstr(kcontrol->id.name, "RX_1",
		   sizeof(kcontrol->id.name)) ||
		   strnstr(kcontrol->id.name, "TX_1",
		   sizeof(kcontrol->id.name))) {
		channel = TDM_1;
	} else if (strnstr(kcontrol->id.name, "RX_2",
		   sizeof(kcontrol->id.name)) ||
		   strnstr(kcontrol->id.name, "TX_2",
		   sizeof(kcontrol->id.name))) {
		channel = TDM_2;
	} else if (strnstr(kcontrol->id.name, "RX_3",
		   sizeof(kcontrol->id.name)) ||
		   strnstr(kcontrol->id.name, "TX_3",
		   sizeof(kcontrol->id.name))) {
		channel = TDM_3;
	} else if (strnstr(kcontrol->id.name, "RX_4",
		   sizeof(kcontrol->id.name)) ||
		   strnstr(kcontrol->id.name, "TX_4",
		   sizeof(kcontrol->id.name))) {
		channel = TDM_4;
	} else if (strnstr(kcontrol->id.name, "RX_5",
		   sizeof(kcontrol->id.name)) ||
		   strnstr(kcontrol->id.name, "TX_5",
		   sizeof(kcontrol->id.name))) {
		channel = TDM_5;
	} else if (strnstr(kcontrol->id.name, "RX_6",
		   sizeof(kcontrol->id.name)) ||
		   strnstr(kcontrol->id.name, "TX_6",
		   sizeof(kcontrol->id.name))) {
		channel = TDM_6;
	} else if (strnstr(kcontrol->id.name, "RX_7",
		   sizeof(kcontrol->id.name)) ||
		   strnstr(kcontrol->id.name, "TX_7",
		   sizeof(kcontrol->id.name))) {
		channel = TDM_7;
	} else {
		pr_err("%s: unsupported channel in: %s",
			__func__, kcontrol->id.name);
		channel = -EINVAL;
	}

	return channel;
}

static int tdm_get_port_idx(struct snd_kcontrol *kcontrol,
			    struct tdm_port *port)
{
	if (port) {
		port->mode = tdm_get_mode(kcontrol);
		if (port->mode < 0)
			return port->mode;

		port->channel = tdm_get_channel(kcontrol);
		if (port->channel < 0)
			return port->channel;
	} else
		return -EINVAL;
	return 0;
}

static int tdm_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		ucontrol->value.enumerated.item[0] = tdm_get_sample_rate_val(
			tdm_rx_cfg[port.mode][port.channel].sample_rate);

		pr_debug("%s: tdm_rx_sample_rate = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].sample_rate,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		tdm_rx_cfg[port.mode][port.channel].sample_rate =
			tdm_get_sample_rate(ucontrol->value.enumerated.item[0]);

		pr_debug("%s: tdm_rx_sample_rate = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].sample_rate,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		ucontrol->value.enumerated.item[0] = tdm_get_sample_rate_val(
			tdm_tx_cfg[port.mode][port.channel].sample_rate);

		pr_debug("%s: tdm_tx_sample_rate = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].sample_rate,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		tdm_tx_cfg[port.mode][port.channel].sample_rate =
			tdm_get_sample_rate(ucontrol->value.enumerated.item[0]);

		pr_debug("%s: tdm_tx_sample_rate = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].sample_rate,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_get_format(int value)
{
	int format = 0;

	switch (value) {
	case 0:
		format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	case 1:
		format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 2:
		format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	default:
		format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return format;
}

static int tdm_get_format_val(int format)
{
	int value = 0;

	switch (format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		value = 0;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		value = 1;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		value = 2;
		break;
	default:
		value = 0;
		break;
	}
	return value;
}

static int tdm_rx_format_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		ucontrol->value.enumerated.item[0] = tdm_get_format_val(
				tdm_rx_cfg[port.mode][port.channel].bit_format);

		pr_debug("%s: tdm_rx_bit_format = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].bit_format,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_rx_format_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		tdm_rx_cfg[port.mode][port.channel].bit_format =
			tdm_get_format(ucontrol->value.enumerated.item[0]);

		pr_debug("%s: tdm_rx_bit_format = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].bit_format,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_tx_format_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		ucontrol->value.enumerated.item[0] = tdm_get_format_val(
				tdm_tx_cfg[port.mode][port.channel].bit_format);

		pr_debug("%s: tdm_tx_bit_format = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].bit_format,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_tx_format_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		tdm_tx_cfg[port.mode][port.channel].bit_format =
			tdm_get_format(ucontrol->value.enumerated.item[0]);

		pr_debug("%s: tdm_tx_bit_format = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].bit_format,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_rx_ch_get(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {

		ucontrol->value.enumerated.item[0] =
			tdm_rx_cfg[port.mode][port.channel].channels - 1;

		pr_debug("%s: tdm_rx_ch = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].channels - 1,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_rx_ch_put(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		tdm_rx_cfg[port.mode][port.channel].channels =
			ucontrol->value.enumerated.item[0] + 1;

		pr_debug("%s: tdm_rx_ch = %d, item = %d\n", __func__,
			 tdm_rx_cfg[port.mode][port.channel].channels,
			 ucontrol->value.enumerated.item[0] + 1);
	}
	return ret;
}

static int tdm_tx_ch_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		ucontrol->value.enumerated.item[0] =
			tdm_tx_cfg[port.mode][port.channel].channels - 1;

		pr_debug("%s: tdm_tx_ch = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].channels - 1,
			 ucontrol->value.enumerated.item[0]);
	}
	return ret;
}

static int tdm_tx_ch_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		tdm_tx_cfg[port.mode][port.channel].channels =
			ucontrol->value.enumerated.item[0] + 1;

		pr_debug("%s: tdm_tx_ch = %d, item = %d\n", __func__,
			 tdm_tx_cfg[port.mode][port.channel].channels,
			 ucontrol->value.enumerated.item[0] + 1);
	}
	return ret;
}

static int tdm_get_slot_num_val(int slot_num)
{
	int slot_num_val;

	switch (slot_num) {
	case 1:
		slot_num_val = 0;
		break;
	case 2:
		slot_num_val = 1;
		break;
	case 4:
		slot_num_val = 2;
		break;
	case 8:
		slot_num_val = 3;
		break;
	case 16:
		slot_num_val = 4;
		break;
	case 32:
		slot_num_val = 5;
		break;
	default:
		slot_num_val = 5;
		break;
	}
	return slot_num_val;
}

static int tdm_slot_num_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	int mode = tdm_get_mode(kcontrol);

	if (mode < 0) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
		return mode;
	}

	ucontrol->value.enumerated.item[0] =
		tdm_get_slot_num_val(tdm_slot[mode].num);

	pr_debug("%s: mode = %d, tdm_slot_num = %d, item = %d\n", __func__,
		mode, tdm_slot[mode].num,
		ucontrol->value.enumerated.item[0]);

	return 0;
}

static int tdm_get_slot_num(int value)
{
	int slot_num;

	switch (value) {
	case 0:
		slot_num = 1;
		break;
	case 1:
		slot_num = 2;
		break;
	case 2:
		slot_num = 4;
		break;
	case 3:
		slot_num = 8;
		break;
	case 4:
		slot_num = 16;
		break;
	case 5:
		slot_num = 32;
		break;
	default:
		slot_num = 8;
		break;
	}
	return slot_num;
}

static int tdm_slot_num_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	int mode = tdm_get_mode(kcontrol);

	if (mode < 0) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
		return mode;
	}

	tdm_slot[mode].num =
		tdm_get_slot_num(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: mode = %d, tdm_slot_num = %d, item = %d\n", __func__,
		mode, tdm_slot[mode].num,
		ucontrol->value.enumerated.item[0]);

	return 0;
}

static int tdm_get_slot_width_val(int slot_width)
{
	int slot_width_val;

	switch (slot_width) {
	case 16:
		slot_width_val = 0;
		break;
	case 24:
		slot_width_val = 1;
		break;
	case 32:
		slot_width_val = 2;
		break;
	default:
		slot_width_val = 2;
		break;
	}
	return slot_width_val;
}

static int tdm_slot_width_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	int mode = tdm_get_mode(kcontrol);

	if (mode < 0) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
		return mode;
	}

	ucontrol->value.enumerated.item[0] =
		tdm_get_slot_width_val(tdm_slot[mode].width);

	pr_debug("%s: mode = %d, tdm_slot_width = %d, item = %d\n", __func__,
		mode, tdm_slot[mode].width,
		ucontrol->value.enumerated.item[0]);

	return 0;
}

static int tdm_get_slot_width(int value)
{
	int slot_width;

	switch (value) {
	case 0:
		slot_width = 16;
		break;
	case 1:
		slot_width = 24;
		break;
	case 2:
		slot_width = 32;
		break;
	default:
		slot_width = 32;
		break;
	}
	return slot_width;
}

static int tdm_slot_width_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	int mode = tdm_get_mode(kcontrol);

	if (mode < 0) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
		return mode;
	}

	tdm_slot[mode].width =
		tdm_get_slot_width(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: mode = %d, tdm_slot_width = %d, item = %d\n", __func__,
		mode, tdm_slot[mode].width,
		ucontrol->value.enumerated.item[0]);

	return 0;
}

static int tdm_rx_slot_mapping_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	unsigned int *slot_offset;
	int i;
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		if (port.mode < TDM_INTERFACE_MAX &&
			port.channel < TDM_PORT_MAX) {
			slot_offset =
				tdm_rx_slot_offset[port.mode][port.channel];
			pr_debug("%s: mode = %d, channel = %d\n",
					__func__, port.mode, port.channel);
			for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
				ucontrol->value.integer.value[i] =
					slot_offset[i];
				pr_debug("%s: offset %d, value %d\n",
						__func__, i, slot_offset[i]);
			}
		} else {
			pr_err("%s: unsupported mode/channel", __func__);
		}
	}
	return ret;
}

static int tdm_rx_slot_mapping_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	unsigned int *slot_offset;
	int i;
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		if (port.mode < TDM_INTERFACE_MAX &&
			port.channel < TDM_PORT_MAX) {
			slot_offset =
				tdm_rx_slot_offset[port.mode][port.channel];
			pr_debug("%s: mode = %d, channel = %d\n",
					__func__, port.mode, port.channel);
			for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
				slot_offset[i] =
					ucontrol->value.integer.value[i];
				pr_debug("%s: offset %d, value %d\n",
						__func__, i, slot_offset[i]);
			}
		} else {
			pr_err("%s: unsupported mode/channel", __func__);
		}
	}
	return ret;
}

static int tdm_tx_slot_mapping_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	unsigned int *slot_offset;
	int i;
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		if (port.mode < TDM_INTERFACE_MAX &&
			port.channel < TDM_PORT_MAX) {
			slot_offset =
				tdm_tx_slot_offset[port.mode][port.channel];
			pr_debug("%s: mode = %d, channel = %d\n",
					__func__, port.mode, port.channel);
			for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
				ucontrol->value.integer.value[i] =
					slot_offset[i];
				pr_debug("%s: offset %d, value %d\n",
						__func__, i, slot_offset[i]);
			}
		} else {
			pr_err("%s: unsupported mode/channel", __func__);
		}
	}
	return ret;
}

static int tdm_tx_slot_mapping_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	unsigned int *slot_offset;
	int i;
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s",
			__func__, kcontrol->id.name);
	} else {
		if (port.mode < TDM_INTERFACE_MAX &&
			port.channel < TDM_PORT_MAX) {
			slot_offset =
				tdm_tx_slot_offset[port.mode][port.channel];
			pr_debug("%s: mode = %d, channel = %d\n",
					__func__, port.mode, port.channel);
			for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
				slot_offset[i] =
					ucontrol->value.integer.value[i];
				pr_debug("%s: offset %d, value %d\n",
						__func__, i, slot_offset[i]);
			}
		} else {
			pr_err("%s: unsupported mode/channel", __func__);
		}
	}
	return ret;
}

static int aux_pcm_get_port_idx(struct snd_kcontrol *kcontrol)
{
	int idx;

	if (strnstr(kcontrol->id.name, "PRIM_AUX_PCM",
		    sizeof("PRIM_AUX_PCM")))
		idx = PRIM_AUX_PCM;
	else if (strnstr(kcontrol->id.name, "SEC_AUX_PCM",
			 sizeof("SEC_AUX_PCM")))
		idx = SEC_AUX_PCM;
	else if (strnstr(kcontrol->id.name, "TERT_AUX_PCM",
			 sizeof("TERT_AUX_PCM")))
		idx = TERT_AUX_PCM;
	else if (strnstr(kcontrol->id.name, "QUAT_AUX_PCM",
			 sizeof("QUAT_AUX_PCM")))
		idx = QUAT_AUX_PCM;
	else {
		pr_err("%s: unsupported port: %s",
			__func__, kcontrol->id.name);
		idx = -EINVAL;
	}

	return idx;
}

static int aux_pcm_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	aux_pcm_rx_cfg[idx].sample_rate =
		aux_pcm_get_sample_rate(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 idx, aux_pcm_rx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int aux_pcm_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
	     aux_pcm_get_sample_rate_val(aux_pcm_rx_cfg[idx].sample_rate);

	pr_debug("%s: idx[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 idx, aux_pcm_rx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int aux_pcm_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	aux_pcm_tx_cfg[idx].sample_rate =
		aux_pcm_get_sample_rate(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_tx_sample_rate = %d, item = %d\n", __func__,
		 idx, aux_pcm_tx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int aux_pcm_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
	     aux_pcm_get_sample_rate_val(aux_pcm_tx_cfg[idx].sample_rate);

	pr_debug("%s: idx[%d]_tx_sample_rate = %d, item = %d\n", __func__,
		 idx, aux_pcm_tx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int mi2s_get_port_idx(struct snd_kcontrol *kcontrol)
{
	int idx;

	if (strnstr(kcontrol->id.name, "PRIM_MI2S_RX",
	    sizeof("PRIM_MI2S_RX")))
		idx = PRIM_MI2S;
	else if (strnstr(kcontrol->id.name, "SEC_MI2S_RX",
		 sizeof("SEC_MI2S_RX")))
		idx = SEC_MI2S;
	else if (strnstr(kcontrol->id.name, "TERT_MI2S_RX",
		 sizeof("TERT_MI2S_RX")))
		idx = TERT_MI2S;
	else if (strnstr(kcontrol->id.name, "QUAT_MI2S_RX",
		 sizeof("QUAT_MI2S_RX")))
		idx = QUAT_MI2S;
	else if (strnstr(kcontrol->id.name, "PRIM_MI2S_TX",
		 sizeof("PRIM_MI2S_TX")))
		idx = PRIM_MI2S;
	else if (strnstr(kcontrol->id.name, "SEC_MI2S_TX",
		 sizeof("SEC_MI2S_TX")))
		idx = SEC_MI2S;
	else if (strnstr(kcontrol->id.name, "TERT_MI2S_TX",
		 sizeof("TERT_MI2S_TX")))
		idx = TERT_MI2S;
	else if (strnstr(kcontrol->id.name, "QUAT_MI2S_TX",
		 sizeof("QUAT_MI2S_TX")))
		idx = QUAT_MI2S;
	else {
		pr_err("%s: unsupported channel: %s",
			__func__, kcontrol->id.name);
		idx = -EINVAL;
	}

	return idx;
}

static int mi2s_get_sample_rate_val(int sample_rate)
{
	int sample_rate_val;

	switch (sample_rate) {
	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 0;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 8;
		break;
	default:
		sample_rate_val = 4;
		break;
	}
	return sample_rate_val;
}

static int mi2s_get_sample_rate(int value)
{
	int sample_rate;

	switch (value) {
	case 0:
		sample_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 2:
		sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 3:
		sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 4:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 5:
		sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 6:
		sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 7:
		sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 8:
		sample_rate = SAMPLING_RATE_192KHZ;
		break;
	default:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	return sample_rate;
}

static int mi2s_get_format(int value)
{
	int format;

	switch (value) {
	case 0:
		format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	case 1:
		format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 2:
		format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 3:
		format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	default:
		format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	return format;
}

static int mi2s_get_format_value(int format)
{
	int value;

	switch (format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		value = 0;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		value = 1;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		value = 2;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		value = 3;
		break;
	default:
		value = 0;
		break;
	}
	return value;
}

static int mi2s_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_rx_cfg[idx].sample_rate =
		mi2s_get_sample_rate(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 idx, mi2s_rx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int mi2s_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		mi2s_get_sample_rate_val(mi2s_rx_cfg[idx].sample_rate);

	pr_debug("%s: idx[%d]_rx_sample_rate = %d, item = %d\n", __func__,
		 idx, mi2s_rx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int mi2s_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_tx_cfg[idx].sample_rate =
		mi2s_get_sample_rate(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_tx_sample_rate = %d, item = %d\n", __func__,
		 idx, mi2s_tx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int mi2s_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		mi2s_get_sample_rate_val(mi2s_tx_cfg[idx].sample_rate);

	pr_debug("%s: idx[%d]_tx_sample_rate = %d, item = %d\n", __func__,
		 idx, mi2s_tx_cfg[idx].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_mi2s_rx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	pr_debug("%s: msm_mi2s_[%d]_rx_ch  = %d\n", __func__,
		 idx, mi2s_rx_cfg[idx].channels);
	ucontrol->value.enumerated.item[0] = mi2s_rx_cfg[idx].channels - 1;

	return 0;
}

static int msm_mi2s_rx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_rx_cfg[idx].channels = ucontrol->value.enumerated.item[0] + 1;
	pr_debug("%s: msm_mi2s_[%d]_rx_ch  = %d\n", __func__,
		 idx, mi2s_rx_cfg[idx].channels);

	return 1;
}

static int msm_mi2s_tx_ch_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	pr_debug("%s: msm_mi2s_[%d]_tx_ch  = %d\n", __func__,
		 idx, mi2s_tx_cfg[idx].channels);
	ucontrol->value.enumerated.item[0] = mi2s_tx_cfg[idx].channels - 1;

	return 0;
}

static int msm_mi2s_tx_ch_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_tx_cfg[idx].channels = ucontrol->value.enumerated.item[0] + 1;
	pr_debug("%s: msm_mi2s_[%d]_tx_ch  = %d\n", __func__,
		 idx, mi2s_tx_cfg[idx].channels);

	return 1;
}

static int msm_mi2s_rx_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		mi2s_get_format_value(mi2s_rx_cfg[idx].bit_format);

	pr_debug("%s: idx[%d]_rx_format = %d, item = %d\n", __func__,
		idx, mi2s_rx_cfg[idx].bit_format,
		ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_mi2s_rx_format_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_rx_cfg[idx].bit_format =
		mi2s_get_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_rx_format = %d, item = %d\n", __func__,
		  idx, mi2s_rx_cfg[idx].bit_format,
		  ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_mi2s_tx_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		mi2s_get_format_value(mi2s_tx_cfg[idx].bit_format);

	pr_debug("%s: idx[%d]_tx_format = %d, item = %d\n", __func__,
		idx, mi2s_tx_cfg[idx].bit_format,
		ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_mi2s_tx_format_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = mi2s_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	mi2s_tx_cfg[idx].bit_format =
		mi2s_get_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_tx_format = %d, item = %d\n", __func__,
		  idx, mi2s_tx_cfg[idx].bit_format,
		  ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_hifi_ctrl(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	struct snd_soc_card *card = codec->component.card;
	struct msm_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);

	pr_debug("%s: msm_hifi_control = %d", __func__,
		 msm_hifi_control);

	if (!pdata || !pdata->hph_en1_gpio_p) {
		pr_err("%s: hph_en1_gpio is invalid\n", __func__);
		return -EINVAL;
	}
	if (msm_hifi_control == MSM_HIFI_ON) {
		msm_cdc_pinctrl_select_active_state(pdata->hph_en1_gpio_p);
		/* 5msec delay needed as per HW requirement */
		usleep_range(5000, 5010);
	} else {
		msm_cdc_pinctrl_select_sleep_state(pdata->hph_en1_gpio_p);
	}
	snd_soc_dapm_sync(dapm);

	return 0;
}

static int msm_hifi_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s: msm_hifi_control = %d\n",
		 __func__, msm_hifi_control);
	ucontrol->value.integer.value[0] = msm_hifi_control;

	return 0;
}

static int msm_hifi_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	pr_debug("%s() ucontrol->value.integer.value[0] = %ld\n",
		 __func__, ucontrol->value.integer.value[0]);

	msm_hifi_control = ucontrol->value.integer.value[0];
	msm_hifi_ctrl(codec);

	return 0;
}

static const struct snd_kcontrol_new msm_snd_controls[] = {
	SOC_ENUM_EXT("SLIM_0_RX Channels", slim_0_rx_chs,
			msm_slim_rx_ch_get, msm_slim_rx_ch_put),
	SOC_ENUM_EXT("SLIM_2_RX Channels", slim_2_rx_chs,
			msm_slim_rx_ch_get, msm_slim_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_TX Channels", slim_0_tx_chs,
			msm_slim_tx_ch_get, msm_slim_tx_ch_put),
	SOC_ENUM_EXT("SLIM_1_TX Channels", slim_1_tx_chs,
			msm_slim_tx_ch_get, msm_slim_tx_ch_put),
	SOC_ENUM_EXT("SLIM_5_RX Channels", slim_5_rx_chs,
			msm_slim_rx_ch_get, msm_slim_rx_ch_put),
	SOC_ENUM_EXT("SLIM_6_RX Channels", slim_6_rx_chs,
			msm_slim_rx_ch_get, msm_slim_rx_ch_put),
	SOC_ENUM_EXT("VI_FEED_TX Channels", vi_feed_tx_chs,
			msm_vi_feed_tx_ch_get, msm_vi_feed_tx_ch_put),
	SOC_ENUM_EXT("USB_AUDIO_RX Channels", usb_rx_chs,
			usb_audio_rx_ch_get, usb_audio_rx_ch_put),
	SOC_ENUM_EXT("USB_AUDIO_TX Channels", usb_tx_chs,
			usb_audio_tx_ch_get, usb_audio_tx_ch_put),
	SOC_ENUM_EXT("HDMI_RX Channels", ext_disp_rx_chs,
			ext_disp_rx_ch_get, ext_disp_rx_ch_put),
	SOC_ENUM_EXT("Display Port RX Channels", ext_disp_rx_chs,
			ext_disp_rx_ch_get, ext_disp_rx_ch_put),
	SOC_ENUM_EXT("PROXY_RX Channels", proxy_rx_chs,
			proxy_rx_ch_get, proxy_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_RX Format", slim_0_rx_format,
			slim_rx_bit_format_get, slim_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_5_RX Format", slim_5_rx_format,
			slim_rx_bit_format_get, slim_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_6_RX Format", slim_6_rx_format,
			slim_rx_bit_format_get, slim_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_0_TX Format", slim_0_tx_format,
			slim_tx_bit_format_get, slim_tx_bit_format_put),
	SOC_ENUM_EXT("USB_AUDIO_RX Format", usb_rx_format,
			usb_audio_rx_format_get, usb_audio_rx_format_put),
	SOC_ENUM_EXT("USB_AUDIO_TX Format", usb_tx_format,
			usb_audio_tx_format_get, usb_audio_tx_format_put),
	SOC_ENUM_EXT("HDMI_RX Bit Format", ext_disp_rx_format,
			ext_disp_rx_format_get, ext_disp_rx_format_put),
	SOC_ENUM_EXT("Display Port RX Bit Format", ext_disp_rx_format,
			ext_disp_rx_format_get, ext_disp_rx_format_put),
	SOC_ENUM_EXT("SLIM_0_RX SampleRate", slim_0_rx_sample_rate,
			slim_rx_sample_rate_get, slim_rx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_2_RX SampleRate", slim_2_rx_sample_rate,
			slim_rx_sample_rate_get, slim_rx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_0_TX SampleRate", slim_0_tx_sample_rate,
			slim_tx_sample_rate_get, slim_tx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_5_RX SampleRate", slim_5_rx_sample_rate,
			slim_rx_sample_rate_get, slim_rx_sample_rate_put),
	SOC_ENUM_EXT("SLIM_6_RX SampleRate", slim_6_rx_sample_rate,
			slim_rx_sample_rate_get, slim_rx_sample_rate_put),
	SOC_ENUM_EXT("BT SampleRate", bt_sample_rate,
			msm_bt_sample_rate_get,
			msm_bt_sample_rate_put),
	SOC_ENUM_EXT("BT SampleRate RX", bt_sample_rate_rx,
			msm_bt_sample_rate_rx_get,
			msm_bt_sample_rate_rx_put),
	SOC_ENUM_EXT("BT SampleRate TX", bt_sample_rate_tx,
			msm_bt_sample_rate_tx_get,
			msm_bt_sample_rate_tx_put),
	SOC_ENUM_EXT("USB_AUDIO_RX SampleRate", usb_rx_sample_rate,
			usb_audio_rx_sample_rate_get,
			usb_audio_rx_sample_rate_put),
	SOC_ENUM_EXT("USB_AUDIO_TX SampleRate", usb_tx_sample_rate,
			usb_audio_tx_sample_rate_get,
			usb_audio_tx_sample_rate_put),
	SOC_ENUM_EXT("HDMI_RX SampleRate", ext_disp_rx_sample_rate,
			ext_disp_rx_sample_rate_get,
			ext_disp_rx_sample_rate_put),
	SOC_ENUM_EXT("Display Port RX SampleRate", ext_disp_rx_sample_rate,
			ext_disp_rx_sample_rate_get,
			ext_disp_rx_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("PRI_TDM SlotNumber", tdm_slot_num,
			tdm_slot_num_get, tdm_slot_num_put),
	SOC_ENUM_EXT("PRI_TDM SlotWidth", tdm_slot_width,
			tdm_slot_width_get, tdm_slot_width_put),
	SOC_ENUM_EXT("SEC_TDM SlotNumber", tdm_slot_num,
			tdm_slot_num_get, tdm_slot_num_put),
	SOC_ENUM_EXT("SEC_TDM SlotWidth", tdm_slot_width,
			tdm_slot_width_get, tdm_slot_width_put),
	SOC_ENUM_EXT("TERT_TDM SlotNumber", tdm_slot_num,
			tdm_slot_num_get, tdm_slot_num_put),
	SOC_ENUM_EXT("TERT_TDM SlotWidth", tdm_slot_width,
			tdm_slot_width_get, tdm_slot_width_put),
	SOC_ENUM_EXT("QUAT_TDM SlotNumber", tdm_slot_num,
			tdm_slot_num_get, tdm_slot_num_put),
	SOC_ENUM_EXT("QUAT_TDM SlotWidth", tdm_slot_width,
			tdm_slot_width_get, tdm_slot_width_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_0 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_1 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_2 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_3 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_4 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_5 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_6 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_RX_7 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_0 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_1 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_2 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_3 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_4 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_5 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_6 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("PRI_TDM_TX_7 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_0 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_1 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_2 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_3 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_4 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_5 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_6 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_RX_7 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_0 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_1 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_2 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_3 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_4 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_5 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_6 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("SEC_TDM_TX_7 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_0 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_1 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_2 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_3 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_4 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_5 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_6 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_RX_7 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_0 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_1 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_2 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_3 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_4 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_5 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_6 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("TERT_TDM_TX_7 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_0 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_1 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_2 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_3 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_4 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_5 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_6 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_RX_7 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_0 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_1 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_2 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_3 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_4 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_5 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_6 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUAT_TDM_TX_7 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_ENUM_EXT("PRIM_AUX_PCM_RX SampleRate", prim_aux_pcm_rx_sample_rate,
			aux_pcm_rx_sample_rate_get,
			aux_pcm_rx_sample_rate_put),
	SOC_ENUM_EXT("SEC_AUX_PCM_RX SampleRate", sec_aux_pcm_rx_sample_rate,
			aux_pcm_rx_sample_rate_get,
			aux_pcm_rx_sample_rate_put),
	SOC_ENUM_EXT("TERT_AUX_PCM_RX SampleRate", tert_aux_pcm_rx_sample_rate,
			aux_pcm_rx_sample_rate_get,
			aux_pcm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_AUX_PCM_RX SampleRate", quat_aux_pcm_rx_sample_rate,
			aux_pcm_rx_sample_rate_get,
			aux_pcm_rx_sample_rate_put),
	SOC_ENUM_EXT("PRIM_AUX_PCM_TX SampleRate", prim_aux_pcm_tx_sample_rate,
			aux_pcm_tx_sample_rate_get,
			aux_pcm_tx_sample_rate_put),
	SOC_ENUM_EXT("SEC_AUX_PCM_TX SampleRate", sec_aux_pcm_tx_sample_rate,
			aux_pcm_tx_sample_rate_get,
			aux_pcm_tx_sample_rate_put),
	SOC_ENUM_EXT("TERT_AUX_PCM_TX SampleRate", tert_aux_pcm_tx_sample_rate,
			aux_pcm_tx_sample_rate_get,
			aux_pcm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_AUX_PCM_TX SampleRate", quat_aux_pcm_tx_sample_rate,
			aux_pcm_tx_sample_rate_get,
			aux_pcm_tx_sample_rate_put),
	SOC_ENUM_EXT("PRIM_MI2S_RX SampleRate", prim_mi2s_rx_sample_rate,
			mi2s_rx_sample_rate_get,
			mi2s_rx_sample_rate_put),
	SOC_ENUM_EXT("SEC_MI2S_RX SampleRate", sec_mi2s_rx_sample_rate,
			mi2s_rx_sample_rate_get,
			mi2s_rx_sample_rate_put),
	SOC_ENUM_EXT("TERT_MI2S_RX SampleRate", tert_mi2s_rx_sample_rate,
			mi2s_rx_sample_rate_get,
			mi2s_rx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_MI2S_RX SampleRate", quat_mi2s_rx_sample_rate,
			mi2s_rx_sample_rate_get,
			mi2s_rx_sample_rate_put),
	SOC_ENUM_EXT("PRIM_MI2S_TX SampleRate", prim_mi2s_tx_sample_rate,
			mi2s_tx_sample_rate_get,
			mi2s_tx_sample_rate_put),
	SOC_ENUM_EXT("SEC_MI2S_TX SampleRate", sec_mi2s_tx_sample_rate,
			mi2s_tx_sample_rate_get,
			mi2s_tx_sample_rate_put),
	SOC_ENUM_EXT("TERT_MI2S_TX SampleRate", tert_mi2s_tx_sample_rate,
			mi2s_tx_sample_rate_get,
			mi2s_tx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_MI2S_TX SampleRate", quat_mi2s_tx_sample_rate,
			mi2s_tx_sample_rate_get,
			mi2s_tx_sample_rate_put),
	SOC_ENUM_EXT("PRIM_MI2S_RX Channels", prim_mi2s_rx_chs,
			msm_mi2s_rx_ch_get, msm_mi2s_rx_ch_put),
	SOC_ENUM_EXT("PRIM_MI2S_TX Channels", prim_mi2s_tx_chs,
			msm_mi2s_tx_ch_get, msm_mi2s_tx_ch_put),
	SOC_ENUM_EXT("SEC_MI2S_RX Channels", sec_mi2s_rx_chs,
			msm_mi2s_rx_ch_get, msm_mi2s_rx_ch_put),
	SOC_ENUM_EXT("SEC_MI2S_TX Channels", sec_mi2s_tx_chs,
			msm_mi2s_tx_ch_get, msm_mi2s_tx_ch_put),
	SOC_ENUM_EXT("TERT_MI2S_RX Channels", tert_mi2s_rx_chs,
			msm_mi2s_rx_ch_get, msm_mi2s_rx_ch_put),
	SOC_ENUM_EXT("TERT_MI2S_TX Channels", tert_mi2s_tx_chs,
			msm_mi2s_tx_ch_get, msm_mi2s_tx_ch_put),
	SOC_ENUM_EXT("QUAT_MI2S_RX Channels", quat_mi2s_rx_chs,
			msm_mi2s_rx_ch_get, msm_mi2s_rx_ch_put),
	SOC_ENUM_EXT("QUAT_MI2S_TX Channels", quat_mi2s_tx_chs,
			msm_mi2s_tx_ch_get, msm_mi2s_tx_ch_put),
	SOC_ENUM_EXT("PRIM_MI2S_RX Format", mi2s_rx_format,
			msm_mi2s_rx_format_get, msm_mi2s_rx_format_put),
	SOC_ENUM_EXT("PRIM_MI2S_TX Format", mi2s_tx_format,
			msm_mi2s_tx_format_get, msm_mi2s_tx_format_put),
	SOC_ENUM_EXT("SEC_MI2S_RX Format", mi2s_rx_format,
			msm_mi2s_rx_format_get, msm_mi2s_rx_format_put),
	SOC_ENUM_EXT("SEC_MI2S_TX Format", mi2s_tx_format,
			msm_mi2s_tx_format_get, msm_mi2s_tx_format_put),
	SOC_ENUM_EXT("TERT_MI2S_RX Format", mi2s_rx_format,
			msm_mi2s_rx_format_get, msm_mi2s_rx_format_put),
	SOC_ENUM_EXT("TERT_MI2S_TX Format", mi2s_tx_format,
			msm_mi2s_tx_format_get, msm_mi2s_tx_format_put),
	SOC_ENUM_EXT("QUAT_MI2S_RX Format", mi2s_rx_format,
			msm_mi2s_rx_format_get, msm_mi2s_rx_format_put),
	SOC_ENUM_EXT("QUAT_MI2S_TX Format", mi2s_tx_format,
			msm_mi2s_tx_format_get, msm_mi2s_tx_format_put),
	SOC_ENUM_EXT("HiFi Function", hifi_function, msm_hifi_get,
			msm_hifi_put),
};

static int msm_snd_enable_codec_ext_clk(struct snd_soc_codec *codec,
					int enable, bool dapm)
{
	int ret = 0;

	if (!strcmp(dev_name(codec->dev), "tasha_codec"))
		ret = tasha_cdc_mclk_enable(codec, enable, dapm);
	else if (!strcmp(dev_name(codec->dev), "tavil_codec"))
		ret = tavil_cdc_mclk_enable(codec, enable);
	else {
		dev_err(codec->dev, "%s: unknown codec to enable ext clk\n",
			__func__);
		ret = -EINVAL;
	}
	return ret;
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

static int msm_mclk_tx_event(struct snd_soc_dapm_widget *w,
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

static int msm_mclk_event(struct snd_soc_dapm_widget *w,
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

static int msm_hifi_ctrl_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *k, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct snd_soc_card *card = codec->component.card;
	struct msm_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);

	pr_debug("%s: msm_hifi_control = %d", __func__, msm_hifi_control);

	if (!pdata || !pdata->hph_en0_gpio_p) {
		pr_err("%s: hph_en0_gpio is invalid\n", __func__);
		return -EINVAL;
	}

	if (msm_hifi_control != MSM_HIFI_ON) {
		pr_debug("%s: HiFi mixer control is not set\n",
			 __func__);
		return 0;
	}

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msm_cdc_pinctrl_select_active_state(pdata->hph_en0_gpio_p);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		msm_cdc_pinctrl_select_sleep_state(pdata->hph_en0_gpio_p);
		break;
	}

	return 0;
}

static const struct snd_soc_dapm_widget msm_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
			    msm_mclk_event,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("MCLK TX",  SND_SOC_NOPM, 0, 0,
	msm_mclk_tx_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SPK("Lineout_1 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_3 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_2 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_4 amp", NULL),
	SND_SOC_DAPM_SPK("hifi amp", msm_hifi_ctrl_event),
	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCRight Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCLeft Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Analog Mic5", NULL),
	SND_SOC_DAPM_MIC("Analog Mic6", NULL),

	SND_SOC_DAPM_MIC("Digital Mic0", NULL),
	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
	SND_SOC_DAPM_MIC("Digital Mic3", NULL),
	SND_SOC_DAPM_MIC("Digital Mic4", NULL),
	SND_SOC_DAPM_MIC("Digital Mic5", NULL),
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

static int msm_slim_get_ch_from_beid(int32_t be_id)
{
	int ch_id = 0;

	switch (be_id) {
	case MSM_BACKEND_DAI_SLIMBUS_0_RX:
		ch_id = SLIM_RX_0;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_1_RX:
		ch_id = SLIM_RX_1;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_2_RX:
		ch_id = SLIM_RX_2;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_3_RX:
		ch_id = SLIM_RX_3;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_4_RX:
		ch_id = SLIM_RX_4;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_6_RX:
		ch_id = SLIM_RX_6;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_0_TX:
		ch_id = SLIM_TX_0;
		break;
	case MSM_BACKEND_DAI_SLIMBUS_3_TX:
		ch_id = SLIM_TX_3;
		break;
	default:
		ch_id = SLIM_RX_0;
		break;
	}

	return ch_id;
}

static int msm_ext_disp_get_idx_from_beid(int32_t be_id)
{
	int idx;

	switch (be_id) {
	case MSM_BACKEND_DAI_HDMI_RX:
		idx = HDMI_RX_IDX;
		break;
	case MSM_BACKEND_DAI_DISPLAY_PORT_RX:
		idx = DP_RX_IDX;
		break;
	default:
		pr_err("%s: Incorrect ext_disp be_id %d\n", __func__, be_id);
		idx = -EINVAL;
		break;
	}

	return idx;
}

static int msm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				  struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	int rc = 0;
	int idx;
	void *config = NULL;
	struct snd_soc_codec *codec = NULL;

	pr_debug("%s: format = %d, rate = %d\n",
		  __func__, params_format(params), params_rate(params));

	switch (dai_link->be_id) {
	case MSM_BACKEND_DAI_SLIMBUS_0_RX:
	case MSM_BACKEND_DAI_SLIMBUS_1_RX:
	case MSM_BACKEND_DAI_SLIMBUS_2_RX:
	case MSM_BACKEND_DAI_SLIMBUS_3_RX:
	case MSM_BACKEND_DAI_SLIMBUS_4_RX:
	case MSM_BACKEND_DAI_SLIMBUS_6_RX:
		idx = msm_slim_get_ch_from_beid(dai_link->be_id);
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_rx_cfg[idx].bit_format);
		rate->min = rate->max = slim_rx_cfg[idx].sample_rate;
		channels->min = channels->max = slim_rx_cfg[idx].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_0_TX:
	case MSM_BACKEND_DAI_SLIMBUS_3_TX:
		idx = msm_slim_get_ch_from_beid(dai_link->be_id);
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_tx_cfg[idx].bit_format);
		rate->min = rate->max = slim_tx_cfg[idx].sample_rate;
		channels->min = channels->max = slim_tx_cfg[idx].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_1_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_tx_cfg[1].bit_format);
		rate->min = rate->max = slim_tx_cfg[1].sample_rate;
		channels->min = channels->max = slim_tx_cfg[1].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_4_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       SNDRV_PCM_FORMAT_S32_LE);
		rate->min = rate->max = SAMPLING_RATE_8KHZ;
		channels->min = channels->max = msm_vi_feed_tx_ch;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_5_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_rx_cfg[5].bit_format);
		rate->min = rate->max = slim_rx_cfg[5].sample_rate;
		channels->min = channels->max = slim_rx_cfg[5].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_5_TX:
		codec = rtd->codec;
		rate->min = rate->max = SAMPLING_RATE_16KHZ;
		channels->min = channels->max = 1;

		config = msm_codec_fn.get_afe_config_fn(codec,
					AFE_SLIMBUS_SLAVE_PORT_CONFIG);
		if (config) {
			rc = afe_set_config(AFE_SLIMBUS_SLAVE_PORT_CONFIG,
					    config, SLIMBUS_5_TX);
			if (rc)
				pr_err("%s: Failed to set slimbus slave port config %d\n",
					__func__, rc);
		}
		break;

	case MSM_BACKEND_DAI_SLIMBUS_7_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_rx_cfg[SLIM_RX_7].bit_format);
		rate->min = rate->max = slim_rx_cfg[SLIM_RX_7].sample_rate;
		channels->min = channels->max =
			slim_rx_cfg[SLIM_RX_7].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_7_TX:
		rate->min = rate->max = slim_tx_cfg[SLIM_TX_7].sample_rate;
		channels->min = channels->max =
			slim_tx_cfg[SLIM_TX_7].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_8_TX:
		rate->min = rate->max = slim_tx_cfg[SLIM_TX_8].sample_rate;
		channels->min = channels->max =
			slim_tx_cfg[SLIM_TX_8].channels;
		break;

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

	case MSM_BACKEND_DAI_HDMI_RX:
	case MSM_BACKEND_DAI_DISPLAY_PORT_RX:
		idx = msm_ext_disp_get_idx_from_beid(dai_link->be_id);
		if (IS_ERR_VALUE(idx)) {
			pr_err("%s: Incorrect ext disp idx %d\n",
			       __func__, idx);
			rc = idx;
			goto done;
		}

		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				ext_disp_rx_cfg[idx].bit_format);
		rate->min = rate->max = ext_disp_rx_cfg[idx].sample_rate;
		channels->min = channels->max = ext_disp_rx_cfg[idx].channels;
		break;

	case MSM_BACKEND_DAI_AFE_PCM_RX:
		channels->min = channels->max = proxy_rx_cfg.channels;
		rate->min = rate->max = SAMPLING_RATE_48KHZ;
		break;

	case MSM_BACKEND_DAI_PRI_TDM_RX_0:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_PRI][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_PRI][TDM_0].bit_format);
		rate->min = rate->max = tdm_rx_cfg[TDM_PRI][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_PRI_TDM_TX_0:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_PRI][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_tx_cfg[TDM_PRI][TDM_0].bit_format);
		rate->min = rate->max = tdm_tx_cfg[TDM_PRI][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_SEC_TDM_RX_0:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_SEC][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_SEC][TDM_0].bit_format);
		rate->min = rate->max = tdm_rx_cfg[TDM_SEC][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_SEC_TDM_TX_0:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_SEC][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_tx_cfg[TDM_SEC][TDM_0].bit_format);
		rate->min = rate->max = tdm_tx_cfg[TDM_SEC][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_TERT_TDM_RX_0:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_TERT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_TERT][TDM_0].bit_format);
		rate->min = rate->max = tdm_rx_cfg[TDM_TERT][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_TERT_TDM_TX_0:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_TERT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_tx_cfg[TDM_TERT][TDM_0].bit_format);
		rate->min = rate->max = tdm_tx_cfg[TDM_TERT][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_QUAT_TDM_RX_0:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUAT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_QUAT][TDM_0].bit_format);
		rate->min = rate->max = tdm_rx_cfg[TDM_QUAT][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_QUAT_TDM_TX_0:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUAT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_tx_cfg[TDM_QUAT][TDM_0].bit_format);
		rate->min = rate->max = tdm_tx_cfg[TDM_QUAT][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_AUXPCM_RX:
		rate->min = rate->max =
			aux_pcm_rx_cfg[PRIM_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[PRIM_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_AUXPCM_TX:
		rate->min = rate->max =
			aux_pcm_tx_cfg[PRIM_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[PRIM_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_SEC_AUXPCM_RX:
		rate->min = rate->max =
			aux_pcm_rx_cfg[SEC_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[SEC_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_SEC_AUXPCM_TX:
		rate->min = rate->max =
			aux_pcm_tx_cfg[SEC_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[SEC_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_TERT_AUXPCM_RX:
		rate->min = rate->max =
			aux_pcm_rx_cfg[TERT_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[TERT_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_TERT_AUXPCM_TX:
		rate->min = rate->max =
			aux_pcm_tx_cfg[TERT_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[TERT_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_QUAT_AUXPCM_RX:
		rate->min = rate->max =
			aux_pcm_rx_cfg[QUAT_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[QUAT_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_QUAT_AUXPCM_TX:
		rate->min = rate->max =
			aux_pcm_tx_cfg[QUAT_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[QUAT_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_PRI_MI2S_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_rx_cfg[PRIM_MI2S].bit_format);
		rate->min = rate->max = mi2s_rx_cfg[PRIM_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_rx_cfg[PRIM_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_PRI_MI2S_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_tx_cfg[PRIM_MI2S].bit_format);
		rate->min = rate->max = mi2s_tx_cfg[PRIM_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_tx_cfg[PRIM_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_SECONDARY_MI2S_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_rx_cfg[SEC_MI2S].bit_format);
		rate->min = rate->max = mi2s_rx_cfg[SEC_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_rx_cfg[SEC_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_SECONDARY_MI2S_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_tx_cfg[SEC_MI2S].bit_format);
		rate->min = rate->max = mi2s_tx_cfg[SEC_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_tx_cfg[SEC_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_TERTIARY_MI2S_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_rx_cfg[TERT_MI2S].bit_format);
		rate->min = rate->max = mi2s_rx_cfg[TERT_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_rx_cfg[TERT_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_TERTIARY_MI2S_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_tx_cfg[TERT_MI2S].bit_format);
		rate->min = rate->max = mi2s_tx_cfg[TERT_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_tx_cfg[TERT_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_QUATERNARY_MI2S_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_rx_cfg[QUAT_MI2S].bit_format);
		rate->min = rate->max = mi2s_rx_cfg[QUAT_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_rx_cfg[QUAT_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_QUATERNARY_MI2S_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_tx_cfg[QUAT_MI2S].bit_format);
		rate->min = rate->max = mi2s_tx_cfg[QUAT_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_tx_cfg[QUAT_MI2S].channels;
		break;

	default:
		rate->min = rate->max = SAMPLING_RATE_48KHZ;
		break;
	}

done:
	return rc;
}

static bool msm_swap_gnd_mic(struct snd_soc_codec *codec)
{
	struct snd_soc_card *card = codec->component.card;
	struct msm_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);
	int value = 0;

	if (pdata->us_euro_gpio_p) {
		value = msm_cdc_pinctrl_get_state(pdata->us_euro_gpio_p);
		if (value)
			msm_cdc_pinctrl_select_sleep_state(
							pdata->us_euro_gpio_p);
		else
			msm_cdc_pinctrl_select_active_state(
							pdata->us_euro_gpio_p);
	} else if (pdata->us_euro_gpio >= 0) {
		value = gpio_get_value_cansleep(pdata->us_euro_gpio);
		gpio_set_value_cansleep(pdata->us_euro_gpio, !value);
	}
	pr_debug("%s: swap select switch %d to %d\n", __func__, value, !value);
	return true;
}

static int msm_afe_set_config(struct snd_soc_codec *codec)
{
	int ret = 0;
	void *config_data = NULL;

	if (!msm_codec_fn.get_afe_config_fn) {
		dev_err(codec->dev, "%s: codec get afe config not init'ed\n",
			__func__);
		return -EINVAL;
	}

	config_data = msm_codec_fn.get_afe_config_fn(codec,
			AFE_CDC_REGISTERS_CONFIG);
	if (config_data) {
		ret = afe_set_config(AFE_CDC_REGISTERS_CONFIG, config_data, 0);
		if (ret) {
			dev_err(codec->dev,
				"%s: Failed to set codec registers config %d\n",
				__func__, ret);
			return ret;
		}
	}

	config_data = msm_codec_fn.get_afe_config_fn(codec,
			AFE_CDC_REGISTER_PAGE_CONFIG);
	if (config_data) {
		ret = afe_set_config(AFE_CDC_REGISTER_PAGE_CONFIG, config_data,
				    0);
		if (ret)
			dev_err(codec->dev,
				"%s: Failed to set cdc register page config\n",
				__func__);
	}

	config_data = msm_codec_fn.get_afe_config_fn(codec,
			AFE_SLIMBUS_SLAVE_CONFIG);
	if (config_data) {
		ret = afe_set_config(AFE_SLIMBUS_SLAVE_CONFIG, config_data, 0);
		if (ret) {
			dev_err(codec->dev,
				"%s: Failed to set slimbus slave config %d\n",
				__func__, ret);
			return ret;
		}
	}

	return 0;
}

static void msm_afe_clear_config(void)
{
	afe_clear_config(AFE_CDC_REGISTERS_CONFIG);
	afe_clear_config(AFE_SLIMBUS_SLAVE_CONFIG);
}

static int msm_adsp_power_up_config(struct snd_soc_codec *codec)
{
	int ret = 0;
	unsigned long timeout;
	int adsp_ready = 0;

	timeout = jiffies +
		msecs_to_jiffies(ADSP_STATE_READY_TIMEOUT_MS);

	do {
		if (q6core_is_adsp_ready()) {
			pr_debug("%s: ADSP Audio is ready\n", __func__);
			adsp_ready = 1;
			break;
		} else {
			/*
			 * ADSP will be coming up after subsystem restart and
			 * it might not be fully up when the control reaches
			 * here. So, wait for 50msec before checking ADSP state
			 */
			msleep(50);
		}
	} while (time_after(timeout, jiffies));

	if (!adsp_ready) {
		pr_err("%s: timed out waiting for ADSP Audio\n", __func__);
		ret = -ETIMEDOUT;
		goto err_fail;
	}

	ret = msm_afe_set_config(codec);
	if (ret)
		pr_err("%s: Failed to set AFE config. err %d\n",
			__func__, ret);

	return 0;

err_fail:
	return ret;
}

static int msm8998_notifier_service_cb(struct notifier_block *this,
					 unsigned long opcode, void *ptr)
{
	int ret;
	struct snd_soc_card *card = NULL;
	const char *be_dl_name = LPASS_BE_SLIMBUS_0_RX;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_codec *codec;

	pr_debug("%s: Service opcode 0x%lx\n", __func__, opcode);

	switch (opcode) {
	case AUDIO_NOTIFIER_SERVICE_DOWN:
		/*
		 * Use flag to ignore initial boot notifications
		 * On initial boot msm_adsp_power_up_config is
		 * called on init. There is no need to clear
		 * and set the config again on initial boot.
		 */
		if (is_initial_boot)
			break;
		msm_afe_clear_config();
		break;
	case AUDIO_NOTIFIER_SERVICE_UP:
		if (is_initial_boot) {
			is_initial_boot = false;
			break;
		}
		if (!spdev)
			return -EINVAL;

		card = platform_get_drvdata(spdev);
		rtd = snd_soc_get_pcm_runtime(card, be_dl_name);
		if (!rtd) {
			dev_err(card->dev,
				"%s: snd_soc_get_pcm_runtime for %s failed!\n",
				__func__, be_dl_name);
			ret = -EINVAL;
			goto done;
		}
		codec = rtd->codec;

		ret = msm_adsp_power_up_config(codec);
		if (ret < 0) {
			dev_err(card->dev,
				"%s: msm_adsp_power_up_config failed ret = %d!\n",
				__func__, ret);
			goto done;
		}
		break;
	default:
		break;
	}
done:
	return NOTIFY_OK;
}

static struct notifier_block service_nb = {
	.notifier_call  = msm8998_notifier_service_cb,
	.priority = -INT_MAX,
};

static int msm_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	void *config_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_pcm_runtime *rtd_aux = rtd->card->rtd_aux;
	struct snd_card *card;
	struct snd_info_entry *entry;
	struct msm_asoc_mach_data *pdata =
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

	/* Tavil Codec SLIMBUS configuration
	 * RX1, RX2, RX3, RX4, RX5, RX6, RX7, RX8
	 * TX1, TX2, TX3, TX4, TX5, TX6, TX7, TX8, TX9, TX10, TX11, TX12, TX13
	 * TX14, TX15, TX16
	 */
	unsigned int rx_ch_tavil[WCD934X_RX_MAX] = {144, 145, 146, 147, 148,
						    149, 150, 151};
	unsigned int tx_ch_tavil[WCD934X_TX_MAX] = {128, 129, 130, 131, 132,
						    133, 134, 135, 136, 137,
						    138, 139, 140, 141, 142,
						    143};

	pr_info("%s: dev_name%s\n", __func__, dev_name(cpu_dai->dev));

	rtd->pmdown_time = 0;

	ret = snd_soc_add_codec_controls(codec, msm_snd_controls,
					 ARRAY_SIZE(msm_snd_controls));
	if (ret < 0) {
		pr_err("%s: add_codec_controls failed, err %d\n",
			__func__, ret);
		return ret;
	}

	snd_soc_dapm_new_controls(dapm, msm_dapm_widgets,
				ARRAY_SIZE(msm_dapm_widgets));

	if (!strcmp(dev_name(codec_dai->dev), "tasha_codec"))
		snd_soc_dapm_add_routes(dapm, wcd_audio_paths_tasha,
					ARRAY_SIZE(wcd_audio_paths_tasha));
	else
		snd_soc_dapm_add_routes(dapm, wcd_audio_paths,
					ARRAY_SIZE(wcd_audio_paths));

	snd_soc_dapm_ignore_suspend(dapm, "Handset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "ANCRight Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "ANCLeft Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic0");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic1");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic2");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic3");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic4");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic5");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic5");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic6");
	snd_soc_dapm_ignore_suspend(dapm, "MADINPUT");
	snd_soc_dapm_ignore_suspend(dapm, "MAD_CPE_INPUT");
	snd_soc_dapm_ignore_suspend(dapm, "MAD_CPE_OUT1");
	snd_soc_dapm_ignore_suspend(dapm, "MAD_CPE_OUT2");
	snd_soc_dapm_ignore_suspend(dapm, "EAR");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT1");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT2");
	snd_soc_dapm_ignore_suspend(dapm, "ANC EAR");
	snd_soc_dapm_ignore_suspend(dapm, "SPK1 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "SPK2 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "HPHL");
	snd_soc_dapm_ignore_suspend(dapm, "HPHR");
	snd_soc_dapm_ignore_suspend(dapm, "AIF4 VI");
	snd_soc_dapm_ignore_suspend(dapm, "VIINPUT");
	snd_soc_dapm_ignore_suspend(dapm, "ANC HPHL");
	snd_soc_dapm_ignore_suspend(dapm, "ANC HPHR");

	if (!strcmp(dev_name(codec_dai->dev), "tasha_codec")) {
		snd_soc_dapm_ignore_suspend(dapm, "LINEOUT3");
		snd_soc_dapm_ignore_suspend(dapm, "LINEOUT4");
		snd_soc_dapm_ignore_suspend(dapm, "ANC LINEOUT1");
		snd_soc_dapm_ignore_suspend(dapm, "ANC LINEOUT2");
	}

	snd_soc_dapm_sync(dapm);

	if (!strcmp(dev_name(codec_dai->dev), "tavil_codec")) {
		snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch_tavil),
					tx_ch_tavil, ARRAY_SIZE(rx_ch_tavil),
					rx_ch_tavil);
	} else {
		snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch),
					tx_ch, ARRAY_SIZE(rx_ch),
					rx_ch);
	}

	if (!strcmp(dev_name(codec_dai->dev), "tavil_codec")) {
		msm_codec_fn.get_afe_config_fn = tavil_get_afe_config;
	} else {
		msm_codec_fn.get_afe_config_fn = tasha_get_afe_config;
		msm_codec_fn.mbhc_hs_detect_exit = tasha_mbhc_hs_detect_exit;
	}

	ret = msm_adsp_power_up_config(codec);
	if (ret) {
		pr_err("%s: Failed to set AFE config %d\n", __func__, ret);
		goto err_afe_cfg;
	}

	config_data = msm_codec_fn.get_afe_config_fn(codec,
						     AFE_AANC_VERSION);
	if (config_data) {
		ret = afe_set_config(AFE_AANC_VERSION, config_data, 0);
		if (ret) {
			pr_err("%s: Failed to set aanc version %d\n",
				__func__, ret);
			goto err_afe_cfg;
		}
	}

	if (!strcmp(dev_name(codec_dai->dev), "tasha_codec")) {
		config_data = msm_codec_fn.get_afe_config_fn(codec,
						AFE_CDC_CLIP_REGISTERS_CONFIG);
		if (config_data) {
			ret = afe_set_config(AFE_CDC_CLIP_REGISTERS_CONFIG,
						 config_data, 0);
			if (ret) {
				pr_err("%s: Failed to set clip registers %d\n",
					__func__, ret);
				goto err_afe_cfg;
			}
		}
		config_data = msm_codec_fn.get_afe_config_fn(codec,
				AFE_CLIP_BANK_SEL);
		if (config_data) {
			ret = afe_set_config(AFE_CLIP_BANK_SEL, config_data, 0);
			if (ret) {
				pr_err("%s: Failed to set AFE bank selection %d\n",
					__func__, ret);
				goto err_afe_cfg;
			}
		}
	}

	/*
	 * Send speaker configuration only for WSA8810.
	 * Defalut configuration is for WSA8815.
	 */
	pr_debug("%s: Number of aux devices: %d\n",
		__func__, rtd->card->num_aux_devs);
	if (!strcmp(dev_name(codec_dai->dev), "tavil_codec")) {
		if (rtd->card->num_aux_devs && rtd_aux && rtd_aux->component)
			if (!strcmp(rtd_aux->component->name, WSA8810_NAME_1) ||
			    !strcmp(rtd_aux->component->name, WSA8810_NAME_2)) {
				tavil_set_spkr_mode(rtd->codec, SPKR_MODE_1);
				tavil_set_spkr_gain_offset(rtd->codec,
							RX_GAIN_OFFSET_M1P5_DB);
		}
		card = rtd->card->snd_card;
		entry = snd_register_module_info(card->module, "codecs",
						 card->proc_root);
		if (!entry) {
			pr_debug("%s: Cannot create codecs module entry\n",
				 __func__);
			pdata->codec_root = NULL;
			goto done;
		}
		pdata->codec_root = entry;
		tavil_codec_info_create_codec_entry(pdata->codec_root, codec);
	} else {
		if (rtd->card->num_aux_devs && rtd_aux && rtd_aux->component)
			if (!strcmp(rtd_aux->component->name, WSA8810_NAME_1) ||
			    !strcmp(rtd_aux->component->name, WSA8810_NAME_2)) {
				tasha_set_spkr_mode(rtd->codec, SPKR_MODE_1);
				tasha_set_spkr_gain_offset(rtd->codec,
							RX_GAIN_OFFSET_M1P5_DB);
		}
		card = rtd->card->snd_card;
		entry = snd_register_module_info(card->module, "codecs",
						 card->proc_root);
		if (!entry) {
			pr_debug("%s: Cannot create codecs module entry\n",
				 __func__);
			ret = 0;
			goto err_snd_module;
		}
		pdata->codec_root = entry;
		tasha_codec_info_create_codec_entry(pdata->codec_root, codec);
	}
done:
	codec_reg_done = true;
	return 0;

err_snd_module:
err_afe_cfg:
	return ret;
}

static int msm_wcn_init(struct snd_soc_pcm_runtime *rtd)
{
	unsigned int rx_ch[WCN_CDC_SLIM_RX_CH_MAX] = {157, 158};
	unsigned int tx_ch[WCN_CDC_SLIM_TX_CH_MAX]  = {159, 160, 161};
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	return snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch),
					   tx_ch, ARRAY_SIZE(rx_ch), rx_ch);
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
	S(v_hs_max, 1600);
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

static void *def_tavil_mbhc_cal(void)
{
	void *tavil_wcd_cal;
	struct wcd_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_high;

	tavil_wcd_cal = kzalloc(WCD_MBHC_CAL_SIZE(WCD_MBHC_DEF_BUTTONS,
				WCD9XXX_MBHC_DEF_RLOADS), GFP_KERNEL);
	if (!tavil_wcd_cal)
		return NULL;

#define S(X, Y) ((WCD_MBHC_CAL_PLUG_TYPE_PTR(tavil_wcd_cal)->X) = (Y))
	S(v_hs_max, 1600);
#undef S
#define S(X, Y) ((WCD_MBHC_CAL_BTN_DET_PTR(tavil_wcd_cal)->X) = (Y))
	S(num_btn, WCD_MBHC_DEF_BUTTONS);
#undef S

	btn_cfg = WCD_MBHC_CAL_BTN_DET_PTR(tavil_wcd_cal);
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

	return tavil_wcd_cal;
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
			goto err_ch_map;
		}
		if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_5_RX) {
			pr_debug("%s: rx_5_ch=%d\n", __func__,
				  slim_rx_cfg[5].channels);
			rx_ch_count = slim_rx_cfg[5].channels;
		} else if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_2_RX) {
			pr_debug("%s: rx_2_ch=%d\n", __func__,
				 slim_rx_cfg[2].channels);
			rx_ch_count = slim_rx_cfg[2].channels;
		} else if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_6_RX) {
			pr_debug("%s: rx_6_ch=%d\n", __func__,
				  slim_rx_cfg[6].channels);
			rx_ch_count = slim_rx_cfg[6].channels;
		} else {
			pr_debug("%s: rx_0_ch=%d\n", __func__,
				  slim_rx_cfg[0].channels);
			rx_ch_count = slim_rx_cfg[0].channels;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
						  rx_ch_count, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
			goto err_ch_map;
		}
	} else {

		pr_debug("%s: %s_tx_dai_id_%d_ch=%d\n", __func__,
			 codec_dai->name, codec_dai->id, user_set_tx_ch);
		ret = snd_soc_dai_get_channel_map(codec_dai,
					 &tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map\n, err:%d\n",
				__func__, ret);
			goto err_ch_map;
		}
		/* For <codec>_tx1 case */
		if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_0_TX)
			user_set_tx_ch = slim_tx_cfg[0].channels;
		/* For <codec>_tx3 case */
		else if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_1_TX)
			user_set_tx_ch = slim_tx_cfg[1].channels;
		else if (dai_link->be_id == MSM_BACKEND_DAI_SLIMBUS_4_TX)
			user_set_tx_ch = msm_vi_feed_tx_ch;
		else
			user_set_tx_ch = tx_ch_cnt;

		pr_debug("%s: msm_slim_0_tx_ch(%d) user_set_tx_ch(%d) tx_ch_cnt(%d), be_id (%d)\n",
			 __func__,  slim_tx_cfg[0].channels, user_set_tx_ch,
			 tx_ch_cnt, dai_link->be_id);

		ret = snd_soc_dai_set_channel_map(cpu_dai,
						  user_set_tx_ch, tx_ch, 0, 0);
		if (ret < 0)
			pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
	}

err_ch_map:
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
		goto err_stream_type;
	}

	pr_debug("%s: %s_tx_dai_id_%d\n", __func__,
		 codec_dai->name, codec_dai->id);
	ret = snd_soc_dai_get_channel_map(codec_dai,
				 &tx_ch_cnt, tx_ch, NULL, NULL);
	if (ret < 0) {
		pr_err("%s: failed to get codec chan map\n, err:%d\n",
			__func__, ret);
		goto err_ch_map;
	}

	user_set_tx_ch = tx_ch_cnt;

	pr_debug("%s: tx_ch_cnt(%d) be_id %d\n",
		 __func__, tx_ch_cnt, dai_link->be_id);

	ret = snd_soc_dai_set_channel_map(cpu_dai,
					  user_set_tx_ch, tx_ch, 0, 0);
	if (ret < 0)
		pr_err("%s: failed to set cpu chan map, err:%d\n",
			__func__, ret);
err_ch_map:
err_stream_type:
	return ret;
}

static int msm_slimbus_2_hw_params(struct snd_pcm_substream *substream,
					  struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int rx_ch[SLIM_MAX_RX_PORTS], tx_ch[SLIM_MAX_TX_PORTS];
	unsigned int rx_ch_cnt = 0, tx_ch_cnt = 0;
	unsigned int num_tx_ch = 0;
	unsigned int num_rx_ch = 0;
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		num_rx_ch =  params_channels(params);
		pr_debug("%s: %s rx_dai_id = %d  num_ch = %d\n", __func__,
			codec_dai->name, codec_dai->id, num_rx_ch);
		ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get codec chan map, err:%d\n",
				__func__, ret);
			goto err_ch_map;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
				num_rx_ch, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
			goto err_ch_map;
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
			goto err_ch_map;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai,
				num_tx_ch, tx_ch, 0, 0);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
			goto err_ch_map;
		}
	}

err_ch_map:
	return ret;
}

static int msm_wcn_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	u32 rx_ch[WCN_CDC_SLIM_RX_CH_MAX], tx_ch[WCN_CDC_SLIM_TX_CH_MAX];
	u32 rx_ch_cnt = 0, tx_ch_cnt = 0;
	int ret;

	dev_dbg(rtd->dev, "%s: %s_tx_dai_id_%d\n", __func__,
		 codec_dai->name, codec_dai->id);
	ret = snd_soc_dai_get_channel_map(codec_dai,
				 &tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
	if (ret) {
		dev_err(rtd->dev,
			"%s: failed to get BTFM codec chan map\n, err:%d\n",
			__func__, ret);
		goto exit;
	}

	dev_dbg(rtd->dev, "%s: tx_ch_cnt(%d) be_id %d\n",
		__func__, tx_ch_cnt, dai_link->be_id);

	ret = snd_soc_dai_set_channel_map(cpu_dai,
					  tx_ch_cnt, tx_ch, rx_ch_cnt, rx_ch);
	if (ret)
		dev_err(rtd->dev, "%s: failed to set cpu chan map, err:%d\n",
			__func__, ret);

exit:
	return ret;
}

static int msm_aux_pcm_snd_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int index = cpu_dai->id - 1;

	dev_dbg(rtd->card->dev,
		"%s: substream = %s  stream = %d, dai name %s, dai ID %d\n",
		__func__, substream->name, substream->stream,
		cpu_dai->name, cpu_dai->id);

	if (index < PRIM_AUX_PCM || index > QUAT_AUX_PCM) {
		ret = -EINVAL;
		dev_err(rtd->card->dev,
			"%s: CPU DAI id (%d) out of range\n",
			__func__, cpu_dai->id);
		goto done;
	}

	mutex_lock(&auxpcm_intf_conf[index].lock);
	if (++auxpcm_intf_conf[index].ref_cnt == 1) {
		if (mi2s_auxpcm_conf[index].pcm_i2s_sel_vt_addr != NULL) {
			mutex_lock(&mi2s_auxpcm_conf[index].lock);
			iowrite32(1,
				mi2s_auxpcm_conf[index].pcm_i2s_sel_vt_addr);
			mutex_unlock(&mi2s_auxpcm_conf[index].lock);
		} else {
			dev_err(rtd->card->dev,
				"%s lpaif_tert_muxsel_virt_addr is NULL\n",
				__func__);
			ret = -EINVAL;
		}
	}
	if (IS_ERR_VALUE(ret))
		auxpcm_intf_conf[index].ref_cnt--;

	mutex_unlock(&auxpcm_intf_conf[index].lock);

done:
	return ret;
}

static void msm_aux_pcm_snd_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int index = rtd->cpu_dai->id - 1;

	dev_dbg(rtd->card->dev,
		"%s: substream = %s  stream = %d, dai name %s, dai ID %d\n",
		__func__,
		substream->name, substream->stream,
		rtd->cpu_dai->name, rtd->cpu_dai->id);

	if (index < PRIM_AUX_PCM || index > QUAT_AUX_PCM) {
		dev_err(rtd->card->dev,
			"%s: CPU DAI id (%d) out of range\n",
			__func__, rtd->cpu_dai->id);
		return;
	}

	mutex_lock(&auxpcm_intf_conf[index].lock);
	if (--auxpcm_intf_conf[index].ref_cnt == 0) {
		if (mi2s_auxpcm_conf[index].pcm_i2s_sel_vt_addr != NULL) {
			mutex_lock(&mi2s_auxpcm_conf[index].lock);
			iowrite32(0,
				mi2s_auxpcm_conf[index].pcm_i2s_sel_vt_addr);
			mutex_unlock(&mi2s_auxpcm_conf[index].lock);
		} else {
			dev_err(rtd->card->dev,
				"%s lpaif_tert_muxsel_virt_addr is NULL\n",
				__func__);
		}
	}
	mutex_unlock(&auxpcm_intf_conf[index].lock);
}

static int msm_get_port_id(int be_id)
{
	int afe_port_id;

	switch (be_id) {
	case MSM_BACKEND_DAI_PRI_MI2S_RX:
		afe_port_id = AFE_PORT_ID_PRIMARY_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_PRI_MI2S_TX:
		afe_port_id = AFE_PORT_ID_PRIMARY_MI2S_TX;
		break;
	case MSM_BACKEND_DAI_SECONDARY_MI2S_RX:
		afe_port_id = AFE_PORT_ID_SECONDARY_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_SECONDARY_MI2S_TX:
		afe_port_id = AFE_PORT_ID_SECONDARY_MI2S_TX;
		break;
	case MSM_BACKEND_DAI_TERTIARY_MI2S_RX:
		afe_port_id = AFE_PORT_ID_TERTIARY_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_TERTIARY_MI2S_TX:
		afe_port_id = AFE_PORT_ID_TERTIARY_MI2S_TX;
		break;
	case MSM_BACKEND_DAI_QUATERNARY_MI2S_RX:
		afe_port_id = AFE_PORT_ID_QUATERNARY_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_QUATERNARY_MI2S_TX:
		afe_port_id = AFE_PORT_ID_QUATERNARY_MI2S_TX;
		break;
	default:
		pr_err("%s: Invalid be_id: %d\n", __func__, be_id);
		afe_port_id = -EINVAL;
	}

	return afe_port_id;
}

static u32 get_mi2s_bits_per_sample(u32 bit_format)
{
	u32 bit_per_sample;

	switch (bit_format) {
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_S24_LE:
		bit_per_sample = 32;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		bit_per_sample = 16;
		break;
	}

	return bit_per_sample;
}

static void update_mi2s_clk_val(int dai_id, int stream)
{
	u32 bit_per_sample;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		bit_per_sample =
		    get_mi2s_bits_per_sample(mi2s_rx_cfg[dai_id].bit_format);
		mi2s_clk[dai_id].clk_freq_in_hz =
		    mi2s_rx_cfg[dai_id].sample_rate * 2 * bit_per_sample;
	} else {
		bit_per_sample =
		    get_mi2s_bits_per_sample(mi2s_tx_cfg[dai_id].bit_format);
		mi2s_clk[dai_id].clk_freq_in_hz =
		    mi2s_tx_cfg[dai_id].sample_rate * 2 * bit_per_sample;
	}
}

static int msm_mi2s_set_sclk(struct snd_pcm_substream *substream, bool enable)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int port_id = 0;
	int index = cpu_dai->id;

	port_id = msm_get_port_id(rtd->dai_link->be_id);
	if (IS_ERR_VALUE(port_id)) {
		dev_err(rtd->card->dev, "%s: Invalid port_id\n", __func__);
		ret = port_id;
		goto done;
	}

	if (enable) {
		update_mi2s_clk_val(index, substream->stream);
		dev_dbg(rtd->card->dev, "%s: clock rate %ul\n", __func__,
			mi2s_clk[index].clk_freq_in_hz);
	}

	mi2s_clk[index].enable = enable;
	ret = afe_set_lpass_clock_v2(port_id,
				     &mi2s_clk[index]);
	if (ret < 0) {
		dev_err(rtd->card->dev,
			"%s: afe lpass clock failed for port 0x%x , err:%d\n",
			__func__, port_id, ret);
		goto done;
	}

done:
	return ret;
}

static int msm_set_pinctrl(struct msm_pinctrl_info *pinctrl_info,
				enum pinctrl_pin_state new_state)
{
	int ret = 0;
	int curr_state = 0;

	if (pinctrl_info == NULL) {
		pr_err("%s: pinctrl_info is NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	if (pinctrl_info->pinctrl == NULL) {
		pr_err("%s: pinctrl_info->pinctrl is NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	curr_state = pinctrl_info->curr_state;
	pinctrl_info->curr_state = new_state;
	pr_debug("%s: curr_state = %s new_state = %s\n", __func__,
		 pin_states[curr_state], pin_states[pinctrl_info->curr_state]);

	if (curr_state == pinctrl_info->curr_state) {
		pr_debug("%s: Already in same state\n", __func__);
		goto err;
	}

	if (curr_state != STATE_DISABLE &&
		pinctrl_info->curr_state != STATE_DISABLE) {
		pr_debug("%s: state already active cannot switch\n", __func__);
		ret = -EIO;
		goto err;
	}

	switch (pinctrl_info->curr_state) {
	case STATE_MI2S_ACTIVE:
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
					pinctrl_info->mi2s_active);
		if (ret) {
			pr_err("%s: MI2S state select failed with %d\n",
				__func__, ret);
			ret = -EIO;
			goto err;
		}
		break;
	case STATE_TDM_ACTIVE:
		ret = pinctrl_select_state(pinctrl_info->pinctrl,
					pinctrl_info->tdm_active);
		if (ret) {
			pr_err("%s: TDM state select failed with %d\n",
				__func__, ret);
			ret = -EIO;
			goto err;
		}
		break;
	case STATE_DISABLE:
		if (curr_state == STATE_MI2S_ACTIVE) {
			ret = pinctrl_select_state(pinctrl_info->pinctrl,
					pinctrl_info->mi2s_disable);
		} else {
			ret = pinctrl_select_state(pinctrl_info->pinctrl,
					pinctrl_info->tdm_disable);
		}
		if (ret) {
			pr_err("%s:  state disable failed with %d\n",
				__func__, ret);
			ret = -EIO;
			goto err;
		}
		break;
	default:
		pr_err("%s: TLMM pin state is invalid\n", __func__);
		return -EINVAL;
	}

err:
	return ret;
}

static void msm_release_pinctrl(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = &pdata->pinctrl_info;

	if (pinctrl_info->pinctrl) {
		devm_pinctrl_put(pinctrl_info->pinctrl);
		pinctrl_info->pinctrl = NULL;
	}
}

static int msm_get_pinctrl(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = NULL;
	struct pinctrl *pinctrl;
	int ret;

	pinctrl_info = &pdata->pinctrl_info;

	if (pinctrl_info == NULL) {
		pr_err("%s: pinctrl_info is NULL\n", __func__);
		return -EINVAL;
	}

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		pr_err("%s: Unable to get pinctrl handle\n", __func__);
		return -EINVAL;
	}
	pinctrl_info->pinctrl = pinctrl;

	/* get all the states handles from Device Tree */
	pinctrl_info->mi2s_disable = pinctrl_lookup_state(pinctrl,
						"quat-mi2s-sleep");
	if (IS_ERR(pinctrl_info->mi2s_disable)) {
		pr_err("%s: could not get mi2s_disable pinstate\n", __func__);
		goto err;
	}
	pinctrl_info->mi2s_active = pinctrl_lookup_state(pinctrl,
						"quat-mi2s-active");
	if (IS_ERR(pinctrl_info->mi2s_active)) {
		pr_err("%s: could not get mi2s_active pinstate\n", __func__);
		goto err;
	}
	pinctrl_info->tdm_disable = pinctrl_lookup_state(pinctrl,
						"quat-tdm-sleep");
	if (IS_ERR(pinctrl_info->tdm_disable)) {
		pr_err("%s: could not get tdm_disable pinstate\n", __func__);
		goto err;
	}
	pinctrl_info->tdm_active = pinctrl_lookup_state(pinctrl,
						"quat-tdm-active");
	if (IS_ERR(pinctrl_info->tdm_active)) {
		pr_err("%s: could not get tdm_active pinstate\n",
			__func__);
		goto err;
	}
	/* Reset the TLMM pins to a default state */
	ret = pinctrl_select_state(pinctrl_info->pinctrl,
					pinctrl_info->mi2s_disable);
	if (ret != 0) {
		pr_err("%s: Disable TLMM pins failed with %d\n",
			__func__, ret);
		ret = -EIO;
		goto err;
	}
	pinctrl_info->curr_state = STATE_DISABLE;

	return 0;

err:
	devm_pinctrl_put(pinctrl);
	pinctrl_info->pinctrl = NULL;
	return -EINVAL;
}

static int msm_tdm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params)
{
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	switch (cpu_dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_PRI][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_PRI][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_PRI][TDM_0].sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_PRI][TDM_1].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_PRI][TDM_1].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_PRI][TDM_1].sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_PRI][TDM_2].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_PRI][TDM_2].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_PRI][TDM_2].sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_PRI][TDM_3].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_PRI][TDM_3].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_PRI][TDM_3].sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_4:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_PRI][TDM_4].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_PRI][TDM_4].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_PRI][TDM_4].sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_5:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_PRI][TDM_5].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_PRI][TDM_5].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_PRI][TDM_5].sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_6:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_PRI][TDM_6].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_PRI][TDM_6].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_PRI][TDM_6].sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_7:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_PRI][TDM_7].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_PRI][TDM_7].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_PRI][TDM_7].sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_PRI][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_PRI][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_PRI][TDM_0].sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_PRI][TDM_1].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_PRI][TDM_1].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_PRI][TDM_1].sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_PRI][TDM_2].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_PRI][TDM_2].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_PRI][TDM_2].sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_PRI][TDM_3].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_PRI][TDM_3].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_PRI][TDM_3].sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_4:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_PRI][TDM_4].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_PRI][TDM_4].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_PRI][TDM_4].sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_5:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_PRI][TDM_5].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_PRI][TDM_5].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_PRI][TDM_5].sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_6:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_PRI][TDM_6].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_PRI][TDM_6].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_PRI][TDM_6].sample_rate;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_7:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_PRI][TDM_7].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_PRI][TDM_7].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_PRI][TDM_7].sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_SEC][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_SEC][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_SEC][TDM_0].sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_1:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_SEC][TDM_1].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_SEC][TDM_1].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_SEC][TDM_1].sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_2:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_SEC][TDM_2].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_SEC][TDM_2].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_SEC][TDM_2].sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_3:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_SEC][TDM_3].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_SEC][TDM_3].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_SEC][TDM_3].sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_4:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_SEC][TDM_4].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_SEC][TDM_4].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_SEC][TDM_4].sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_5:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_SEC][TDM_5].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_SEC][TDM_5].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_SEC][TDM_5].sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_6:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_SEC][TDM_6].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_SEC][TDM_6].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_SEC][TDM_6].sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_7:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_SEC][TDM_7].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_SEC][TDM_7].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_SEC][TDM_7].sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_SEC][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_SEC][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_SEC][TDM_0].sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_1:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_SEC][TDM_1].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_SEC][TDM_1].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_SEC][TDM_1].sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_2:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_SEC][TDM_2].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_SEC][TDM_2].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_SEC][TDM_2].sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_3:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_SEC][TDM_3].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_SEC][TDM_3].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_SEC][TDM_3].sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_4:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_SEC][TDM_4].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_SEC][TDM_4].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_SEC][TDM_4].sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_5:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_SEC][TDM_5].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_SEC][TDM_5].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_SEC][TDM_5].sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_6:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_SEC][TDM_6].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_SEC][TDM_6].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_SEC][TDM_6].sample_rate;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_7:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_SEC][TDM_7].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_SEC][TDM_7].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_SEC][TDM_7].sample_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_TERT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_TERT][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_TERT][TDM_0].sample_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_1:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_TERT][TDM_1].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_TERT][TDM_1].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_TERT][TDM_1].sample_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_2:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_TERT][TDM_2].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_TERT][TDM_2].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_TERT][TDM_2].sample_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_3:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_TERT][TDM_3].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_TERT][TDM_3].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_TERT][TDM_3].sample_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_4:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_TERT][TDM_4].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_TERT][TDM_4].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_TERT][TDM_4].sample_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_5:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_TERT][TDM_5].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_TERT][TDM_5].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_TERT][TDM_5].sample_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_6:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_TERT][TDM_6].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_TERT][TDM_6].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_TERT][TDM_6].sample_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_7:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_TERT][TDM_7].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_TERT][TDM_7].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_TERT][TDM_7].sample_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_TERT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_TERT][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_TERT][TDM_0].sample_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_1:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_TERT][TDM_1].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_TERT][TDM_1].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_TERT][TDM_1].sample_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_2:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_TERT][TDM_2].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_TERT][TDM_2].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_TERT][TDM_2].sample_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_3:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_TERT][TDM_3].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_TERT][TDM_3].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_TERT][TDM_3].sample_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_4:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_TERT][TDM_4].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_TERT][TDM_4].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_TERT][TDM_4].sample_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_5:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_TERT][TDM_5].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_TERT][TDM_5].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_TERT][TDM_5].sample_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_6:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_TERT][TDM_6].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_TERT][TDM_6].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_TERT][TDM_6].sample_rate;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_7:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_TERT][TDM_7].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_TERT][TDM_7].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_TERT][TDM_7].sample_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUAT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_QUAT][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_QUAT][TDM_0].sample_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_1:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUAT][TDM_1].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_QUAT][TDM_1].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_QUAT][TDM_1].sample_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_2:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUAT][TDM_2].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_QUAT][TDM_2].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_QUAT][TDM_2].sample_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_3:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUAT][TDM_3].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_QUAT][TDM_3].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_QUAT][TDM_3].sample_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_4:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUAT][TDM_4].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_QUAT][TDM_4].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_QUAT][TDM_4].sample_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_5:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUAT][TDM_5].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_QUAT][TDM_5].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_QUAT][TDM_5].sample_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_6:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUAT][TDM_6].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_QUAT][TDM_6].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_QUAT][TDM_6].sample_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_7:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUAT][TDM_7].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_QUAT][TDM_7].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_QUAT][TDM_7].sample_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUAT][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_QUAT][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_QUAT][TDM_0].sample_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_1:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUAT][TDM_1].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_QUAT][TDM_1].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_QUAT][TDM_1].sample_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_2:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUAT][TDM_2].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_QUAT][TDM_2].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_QUAT][TDM_2].sample_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_3:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUAT][TDM_3].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_QUAT][TDM_3].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_QUAT][TDM_3].sample_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_4:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUAT][TDM_4].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_QUAT][TDM_4].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_QUAT][TDM_4].sample_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_5:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUAT][TDM_5].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_QUAT][TDM_5].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_QUAT][TDM_5].sample_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_6:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUAT][TDM_6].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_QUAT][TDM_6].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_QUAT][TDM_6].sample_rate;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_7:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUAT][TDM_7].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_QUAT][TDM_7].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_QUAT][TDM_7].sample_rate;
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
	int i = 0;

	if ((slots <= 0) || (slots > 32)) {
		pr_err("%s: invalid slot number %d\n", __func__, slots);
		return -EINVAL;
	}

	for (i = 0; i < slots ; i++)
		slot_mask |= 1 << i;

	return slot_mask;
}

static int msm8998_tdm_snd_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	int channels, slot_width, slots, rate, format;
	unsigned int slot_mask;
	unsigned int *slot_offset;
	int offset_channels = 0;
	int i;
	int clk_freq;

	pr_debug("%s: dai id = 0x%x\n", __func__, cpu_dai->id);

	channels = params_channels(params);
	if (channels < 1 || channels > 32) {
		pr_err("%s: invalid param channels %d\n",
			__func__, channels);
		return -EINVAL;
	}

	format = params_format(params);
	if (format != SNDRV_PCM_FORMAT_S32_LE &&
		format != SNDRV_PCM_FORMAT_S24_LE &&
		format != SNDRV_PCM_FORMAT_S16_LE) {
		/*
		 * Up to 8 channel HW configuration should
		 * use 32 bit slot width for max support of
		 * stream bit width. (slot_width > bit_width)
		 */
		pr_err("%s: invalid param format 0x%x\n",
			__func__, format);
		return -EINVAL;
	}

	switch (cpu_dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_rx_slot_offset[TDM_PRI][TDM_0];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_1:
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_rx_slot_offset[TDM_PRI][TDM_1];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_2:
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_rx_slot_offset[TDM_PRI][TDM_2];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_3:
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_rx_slot_offset[TDM_PRI][TDM_3];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_4:
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_rx_slot_offset[TDM_PRI][TDM_4];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_5:
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_rx_slot_offset[TDM_PRI][TDM_5];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_6:
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_rx_slot_offset[TDM_PRI][TDM_6];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_RX_7:
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_rx_slot_offset[TDM_PRI][TDM_7];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_tx_slot_offset[TDM_PRI][TDM_0];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_1:
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_tx_slot_offset[TDM_PRI][TDM_1];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_2:
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_tx_slot_offset[TDM_PRI][TDM_2];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_3:
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_tx_slot_offset[TDM_PRI][TDM_3];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_4:
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_tx_slot_offset[TDM_PRI][TDM_4];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_5:
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_tx_slot_offset[TDM_PRI][TDM_5];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_6:
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_tx_slot_offset[TDM_PRI][TDM_6];
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX_7:
		slots = tdm_slot[TDM_PRI].num;
		slot_width = tdm_slot[TDM_PRI].width;
		slot_offset = tdm_tx_slot_offset[TDM_PRI][TDM_7];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_rx_slot_offset[TDM_SEC][TDM_0];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_1:
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_rx_slot_offset[TDM_SEC][TDM_1];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_2:
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_rx_slot_offset[TDM_SEC][TDM_2];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_3:
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_rx_slot_offset[TDM_SEC][TDM_3];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_4:
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_rx_slot_offset[TDM_SEC][TDM_4];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_5:
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_rx_slot_offset[TDM_SEC][TDM_5];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_6:
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_rx_slot_offset[TDM_SEC][TDM_6];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX_7:
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_rx_slot_offset[TDM_SEC][TDM_7];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX:
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_tx_slot_offset[TDM_SEC][TDM_0];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_1:
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_tx_slot_offset[TDM_SEC][TDM_1];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_2:
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_tx_slot_offset[TDM_SEC][TDM_2];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_3:
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_tx_slot_offset[TDM_SEC][TDM_3];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_4:
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_tx_slot_offset[TDM_SEC][TDM_4];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_5:
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_tx_slot_offset[TDM_SEC][TDM_5];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_6:
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_tx_slot_offset[TDM_SEC][TDM_6];
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX_7:
		slots = tdm_slot[TDM_SEC].num;
		slot_width = tdm_slot[TDM_SEC].width;
		slot_offset = tdm_tx_slot_offset[TDM_SEC][TDM_7];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX:
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_rx_slot_offset[TDM_TERT][TDM_0];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_1:
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_rx_slot_offset[TDM_TERT][TDM_1];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_2:
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_rx_slot_offset[TDM_TERT][TDM_2];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_3:
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_rx_slot_offset[TDM_TERT][TDM_3];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_4:
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_rx_slot_offset[TDM_TERT][TDM_4];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_5:
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_rx_slot_offset[TDM_TERT][TDM_5];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_6:
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_rx_slot_offset[TDM_TERT][TDM_6];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX_7:
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_rx_slot_offset[TDM_TERT][TDM_7];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX:
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_tx_slot_offset[TDM_TERT][TDM_0];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_1:
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_tx_slot_offset[TDM_TERT][TDM_1];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_2:
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_tx_slot_offset[TDM_TERT][TDM_2];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_3:
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_tx_slot_offset[TDM_TERT][TDM_3];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_4:
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_tx_slot_offset[TDM_TERT][TDM_4];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_5:
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_tx_slot_offset[TDM_TERT][TDM_5];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_6:
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_tx_slot_offset[TDM_TERT][TDM_6];
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX_7:
		slots = tdm_slot[TDM_TERT].num;
		slot_width = tdm_slot[TDM_TERT].width;
		slot_offset = tdm_tx_slot_offset[TDM_TERT][TDM_7];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_rx_slot_offset[TDM_QUAT][TDM_0];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_1:
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_rx_slot_offset[TDM_QUAT][TDM_1];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_2:
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_rx_slot_offset[TDM_QUAT][TDM_2];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_3:
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_rx_slot_offset[TDM_QUAT][TDM_3];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_4:
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_rx_slot_offset[TDM_QUAT][TDM_4];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_5:
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_rx_slot_offset[TDM_QUAT][TDM_5];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_6:
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_rx_slot_offset[TDM_QUAT][TDM_6];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX_7:
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_rx_slot_offset[TDM_QUAT][TDM_7];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_tx_slot_offset[TDM_QUAT][TDM_0];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_1:
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_tx_slot_offset[TDM_QUAT][TDM_1];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_2:
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_tx_slot_offset[TDM_QUAT][TDM_2];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_3:
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_tx_slot_offset[TDM_QUAT][TDM_3];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_4:
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_tx_slot_offset[TDM_QUAT][TDM_4];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_5:
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_tx_slot_offset[TDM_QUAT][TDM_5];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_6:
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_tx_slot_offset[TDM_QUAT][TDM_6];
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX_7:
		slots = tdm_slot[TDM_QUAT].num;
		slot_width = tdm_slot[TDM_QUAT].width;
		slot_offset = tdm_tx_slot_offset[TDM_QUAT][TDM_7];
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
		pr_err("%s: invalid offset_channels %d\n",
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
		pr_debug("%s: slot_width %d\n", __func__, slot_width);
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
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
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
	} else {
		ret = -EINVAL;
		pr_err("%s: invalid use case, err:%d\n",
			__func__, ret);
		goto end;
	}

	rate = params_rate(params);
	clk_freq = rate * slot_width * slots;
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, clk_freq, SND_SOC_CLOCK_OUT);
	if (ret < 0) {
		pr_err("%s: failed to set tdm clk, err:%d\n",
			__func__, ret);
	}

end:
	return ret;
}

static int msm8998_tdm_snd_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = &pdata->pinctrl_info;

	ret = msm_set_pinctrl(pinctrl_info, STATE_TDM_ACTIVE);
	if (ret)
		pr_err("%s: TDM TLMM pinctrl set failed with %d\n",
			__func__, ret);

	return ret;
}

static void msm8998_tdm_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = &pdata->pinctrl_info;

	ret = msm_set_pinctrl(pinctrl_info, STATE_DISABLE);
	if (ret)
		pr_err("%s: TDM TLMM pinctrl set failed with %d\n",
			__func__, ret);

}

static struct snd_soc_ops msm8998_tdm_be_ops = {
	.hw_params = msm8998_tdm_snd_hw_params,
	.startup = msm8998_tdm_snd_startup,
	.shutdown = msm8998_tdm_snd_shutdown
};

static int msm_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int index = cpu_dai->id;
	unsigned int fmt = SND_SOC_DAIFMT_CBS_CFS;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = &pdata->pinctrl_info;
	int ret_pinctrl = 0;

	dev_dbg(rtd->card->dev,
		"%s: substream = %s  stream = %d, dai name %s, dai ID %d\n",
		__func__, substream->name, substream->stream,
		cpu_dai->name, cpu_dai->id);

	if (index < PRIM_MI2S || index > QUAT_MI2S) {
		ret = -EINVAL;
		dev_err(rtd->card->dev,
			"%s: CPU DAI id (%d) out of range\n",
			__func__, cpu_dai->id);
		goto done;
	}
	if (index == QUAT_MI2S) {
		ret_pinctrl = msm_set_pinctrl(pinctrl_info, STATE_MI2S_ACTIVE);
		if (ret_pinctrl) {
			pr_err("%s: MI2S TLMM pinctrl set failed with %d\n",
				__func__, ret_pinctrl);
		}
	}

	/*
	 * Muxtex protection in case the same MI2S
	 * interface using for both TX and RX  so
	 * that the same clock won't be enable twice.
	 */
	mutex_lock(&mi2s_intf_conf[index].lock);
	if (++mi2s_intf_conf[index].ref_cnt == 1) {
		/* Check if msm needs to provide the clock to the interface */
		if (!mi2s_intf_conf[index].msm_is_mi2s_master) {
			fmt = SND_SOC_DAIFMT_CBM_CFM;
			mi2s_clk[index].clk_id = mi2s_ebit_clk[index];
		}
		ret = msm_mi2s_set_sclk(substream, true);
		if (IS_ERR_VALUE(ret)) {
			dev_err(rtd->card->dev,
				"%s: afe lpass clock failed to enable MI2S clock, err:%d\n",
				__func__, ret);
			goto clean_up;
		}
		if (mi2s_auxpcm_conf[index].pcm_i2s_sel_vt_addr != NULL) {
			mutex_lock(&mi2s_auxpcm_conf[index].lock);
			iowrite32(0,
				mi2s_auxpcm_conf[index].pcm_i2s_sel_vt_addr);
			mutex_unlock(&mi2s_auxpcm_conf[index].lock);
		} else {
			dev_err(rtd->card->dev,
				"%s lpaif_muxsel_virt_addr is NULL for dai %d\n",
				__func__, index);
			ret = -EINVAL;
			goto clk_off;
		}
		ret = snd_soc_dai_set_fmt(cpu_dai, fmt);
		if (IS_ERR_VALUE(ret)) {
			pr_err("%s: set fmt cpu dai failed for MI2S (%d), err:%d\n",
				__func__, index, ret);
			goto clk_off;
		}
	}
clk_off:
	if (IS_ERR_VALUE(ret))
		msm_mi2s_set_sclk(substream, false);
clean_up:
	if (IS_ERR_VALUE(ret))
		mi2s_intf_conf[index].ref_cnt--;
	mutex_unlock(&mi2s_intf_conf[index].lock);
done:
	return ret;
}

static void msm_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int index = rtd->cpu_dai->id;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = &pdata->pinctrl_info;
	int ret_pinctrl = 0;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	if (index < PRIM_MI2S || index > QUAT_MI2S) {
		pr_err("%s:invalid MI2S DAI(%d)\n", __func__, index);
		return;
	}

	mutex_lock(&mi2s_intf_conf[index].lock);
	if (--mi2s_intf_conf[index].ref_cnt == 0) {
		ret = msm_mi2s_set_sclk(substream, false);
		if (ret < 0)
			pr_err("%s:clock disable failed for MI2S (%d); ret=%d\n",
				__func__, index, ret);
	}
	mutex_unlock(&mi2s_intf_conf[index].lock);

	if (index == QUAT_MI2S) {
		ret_pinctrl = msm_set_pinctrl(pinctrl_info, STATE_DISABLE);
		if (ret_pinctrl)
			pr_err("%s: MI2S TLMM pinctrl set failed with %d\n",
				__func__, ret_pinctrl);
	}
}

static struct snd_soc_ops msm_mi2s_be_ops = {
	.startup = msm_mi2s_snd_startup,
	.shutdown = msm_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm_aux_pcm_be_ops = {
	.startup = msm_aux_pcm_snd_startup,
	.shutdown = msm_aux_pcm_snd_shutdown,
};

static int msm_tdm_snd_hw_params(struct snd_pcm_substream *substream,
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
	case 5:
	case 6:
	case 7:
	case 8:
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S32_LE:
		case SNDRV_PCM_FORMAT_S24_LE:
		case SNDRV_PCM_FORMAT_S16_LE:
		/*
		 * up to 8 channels HW config should
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
		slots = 8;
		slot_mask = tdm_param_set_slot_mask(slots);
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
	/* currently only supporting TDM_RX_0 and TDM_TX_0 */
	switch (cpu_dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
	case AFE_PORT_ID_SECONDARY_TDM_RX:
	case AFE_PORT_ID_TERTIARY_TDM_RX:
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
	case AFE_PORT_ID_PRIMARY_TDM_TX:
	case AFE_PORT_ID_SECONDARY_TDM_TX:
	case AFE_PORT_ID_TERTIARY_TDM_TX:
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
		slot_offset = tdm_slot_offset[TDM_0];
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

		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, NULL,
						  channels, slot_offset);
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

		ret = snd_soc_dai_set_channel_map(cpu_dai, channels,
						  slot_offset, 0, NULL);
		if (ret < 0) {
			pr_err("%s: failed to set channel map, err:%d\n",
				__func__, ret);
			goto end;
		}
	}
end:
	return ret;
}

static struct snd_soc_ops msm_be_ops = {
	.hw_params = msm_snd_hw_params,
};

static struct snd_soc_ops msm_cpe_ops = {
	.hw_params = msm_snd_cpe_hw_params,
};

static struct snd_soc_ops msm_slimbus_2_be_ops = {
	.hw_params = msm_slimbus_2_hw_params,
};

static struct snd_soc_ops msm_wcn_ops = {
	.hw_params = msm_wcn_hw_params,
};

static struct snd_soc_ops msm_tdm_be_ops = {
	.hw_params = msm_tdm_snd_hw_params
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm_common_dai_links[] = {
	/* FrontEnd DAI Links */
	{
		.name = MSM_DAILINK_NAME(Media1),
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
		.name = MSM_DAILINK_NAME(Media2),
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
		.name = MSM_DAILINK_NAME(ULL),
		.stream_name = "MultiMedia3",
		.cpu_dai_name = "MultiMedia3",
		.platform_name = "msm-pcm-dsp.2",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
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
		.name = "MSM AFE-PCM RX",
		.stream_name = "AFE-PROXY RX",
		.cpu_dai_name = "msm-dai-q6-dev.241",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.platform_name = "msm-pcm-afe",
		.dpcm_playback = 1,
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
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{
		.name = MSM_DAILINK_NAME(Compress1),
		.stream_name = "Compress1",
		.cpu_dai_name = "MultiMedia4",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_HW_PARAMS,
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
		.name = MSM_DAILINK_NAME(LowLatency),
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
		.name = MSM_DAILINK_NAME(Compress2),
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
		.name = MSM_DAILINK_NAME(Compress3),
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
		.name = MSM_DAILINK_NAME(ULL_NOIRQ),
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
		.name = MSM_DAILINK_NAME(Media9),
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
		.name = MSM_DAILINK_NAME(Compress4),
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
		.name = MSM_DAILINK_NAME(Compress5),
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
		.name = MSM_DAILINK_NAME(Compress6),
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
		.name = MSM_DAILINK_NAME(Compress7),
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
		.name = MSM_DAILINK_NAME(Compress8),
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
		.name = MSM_DAILINK_NAME(ULL_NOIRQ_2),
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
		.name = "SLIMBUS_8 Hostless",
		.stream_name = "SLIMBUS8_HOSTLESS Capture",
		.cpu_dai_name = "SLIMBUS8_HOSTLESS",
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

static struct snd_soc_dai_link msm_tasha_fe_dai_links[] = {
	{
		.name = LPASS_BE_SLIMBUS_4_TX,
		.stream_name = "Slimbus4 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16393",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_vifeedback",
		.be_id = MSM_BACKEND_DAI_SLIMBUS_4_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
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
		.ops = &msm_slimbus_2_be_ops,
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
		.ops = &msm_slimbus_2_be_ops,
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
		.ops = &msm_cpe_ops,
	},
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

static struct snd_soc_dai_link msm_tavil_fe_dai_links[] = {
	{
		.name = LPASS_BE_SLIMBUS_4_TX,
		.stream_name = "Slimbus4 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16393",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_vifeedback",
		.be_id = MSM_BACKEND_DAI_SLIMBUS_4_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
	},
	/* Ultrasound RX DAI Link */
	{
		.name = "SLIMBUS_2 Hostless Playback",
		.stream_name = "SLIMBUS_2 Hostless Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16388",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_rx2",
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ops = &msm_slimbus_2_be_ops,
	},
	/* Ultrasound TX DAI Link */
	{
		.name = "SLIMBUS_2 Hostless Capture",
		.stream_name = "SLIMBUS_2 Hostless Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16389",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_tx2",
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ops = &msm_slimbus_2_be_ops,
	},
};

static struct snd_soc_dai_link msm_common_misc_fe_dai_links[] = {
	{
		.name = MSM_DAILINK_NAME(ASM Loopback),
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
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA6,
	},
	{
		.name = "USB Audio Hostless",
		.stream_name = "USB Audio Hostless",
		.cpu_dai_name = "USBAUDIO_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
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
		.name = MSM_DAILINK_NAME(Transcode Loopback Playback),
		.stream_name = "Transcode Loopback Playback",
		.cpu_dai_name = "MultiMedia26",
		.platform_name = "msm-transcode-loopback",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		 /* this dainlink has playback support */
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA26,
	},
	{
		.name = MSM_DAILINK_NAME(Transcode Loopback Capture),
		.stream_name = "Transcode Loopback Capture",
		.cpu_dai_name = "MultiMedia27",
		.platform_name = "msm-transcode-loopback",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			 SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA27,
	},
	{
		.name = "MultiMedia21",
		.stream_name = "MultiMedia21",
		.cpu_dai_name = "MultiMedia21",
		.platform_name = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA21,
	},
	{
		.name = "MultiMedia22",
		.stream_name = "MultiMedia22",
		.cpu_dai_name = "MultiMedia22",
		.platform_name = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA22,
	},
	{
		.name = "MultiMedia23",
		.stream_name = "MultiMedia23",
		.cpu_dai_name = "MultiMedia23",
		.platform_name = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA23,
	},
	{
		.name = "MultiMedia24",
		.stream_name = "MultiMedia24",
		.cpu_dai_name = "MultiMedia24",
		.platform_name = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA24,
	},
	{
		.name = "MultiMedia25",
		.stream_name = "MultiMedia25",
		.cpu_dai_name = "MultiMedia25",
		.platform_name = "msm-pcm-dsp.0",
		.dynamic = 1,
		.async_ops = ASYNC_DPCM_SND_SOC_PREPARE,
		.dpcm_playback = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA25,
	},
};

static struct snd_soc_dai_link msm_common_be_dai_links[] = {
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
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
		.ops = &msm8998_tdm_be_ops,
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
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8998_tdm_be_ops,
		.ignore_suspend = 1,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_TX_0,
		.stream_name = "Tertiary TDM0 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36897",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERT_TDM_TX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_tdm_be_ops,
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
		.ops = &msm8998_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_TDM_TX_0,
		.stream_name = "Quaternary TDM0 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36913",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_TDM_TX_0,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &msm8998_tdm_be_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm_tasha_be_dai_links[] = {
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_be_ops,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm_be_ops,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	/* Slimbus VI Recording */
	{
		.name = LPASS_BE_SLIMBUS_TX_VI,
		.stream_name = "Slimbus4 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16393",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_vifeedback",
		.be_id = MSM_BACKEND_DAI_SLIMBUS_4_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
		.ignore_suspend = 1,
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_pmdown_time = 1,
	},
};

static struct snd_soc_dai_link msm_tavil_be_dai_links[] = {
	{
		.name = LPASS_BE_SLIMBUS_0_RX,
		.stream_name = "Slimbus Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16384",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_RX,
		.init = &msm_audrx_init,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_be_ops,
	},
	{
		.name = LPASS_BE_SLIMBUS_0_TX,
		.stream_name = "Slimbus Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16385",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_tx1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm_be_ops,
	},
	{
		.name = LPASS_BE_SLIMBUS_1_RX,
		.stream_name = "Slimbus1 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16386",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_1_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_1_TX,
		.stream_name = "Slimbus1 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16387",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_tx3",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_1_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_2_RX,
		.stream_name = "Slimbus2 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16388",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_rx2",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_2_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_3_RX,
		.stream_name = "Slimbus3 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16390",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_3_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_3_TX,
		.stream_name = "Slimbus3 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16391",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_tx1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_3_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_4_RX,
		.stream_name = "Slimbus4 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16392",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_4_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_5_RX,
		.stream_name = "Slimbus5 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16394",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_rx3",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_5_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
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
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_mad1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_5_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_6_RX,
		.stream_name = "Slimbus6 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16396",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_rx4",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_6_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	/* Slimbus VI Recording */
	{
		.name = LPASS_BE_SLIMBUS_TX_VI,
		.stream_name = "Slimbus4 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16393",
		.platform_name = "msm-pcm-routing",
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_vifeedback",
		.be_id = MSM_BACKEND_DAI_SLIMBUS_4_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
		.ignore_suspend = 1,
		.no_pcm = 1,
		.dpcm_capture = 1,
		.ignore_pmdown_time = 1,
	},
};

static struct snd_soc_dai_link msm_wcn_be_dai_links[] = {
	{
		.name = LPASS_BE_SLIMBUS_7_RX,
		.stream_name = "Slimbus7 Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16398",
		.platform_name = "msm-pcm-routing",
		.codec_name = "btfmslim_slave",
		/* BT codec driver determines capabilities based on
		 * dai name, bt codecdai name should always contains
		 * supported usecase information
		 */
		.codec_dai_name = "btfm_bt_sco_a2dp_slim_rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_7_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_wcn_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_7_TX,
		.stream_name = "Slimbus7 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16399",
		.platform_name = "msm-pcm-routing",
		.codec_name = "btfmslim_slave",
		.codec_dai_name = "btfm_bt_sco_slim_tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_7_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_wcn_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SLIMBUS_8_TX,
		.stream_name = "Slimbus8 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16401",
		.platform_name = "msm-pcm-routing",
		.codec_name = "btfmslim_slave",
		.codec_dai_name = "btfm_fm_slim_tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_8_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.init = &msm_wcn_init,
		.ops = &msm_wcn_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link ext_disp_be_dai_link[] = {
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	/* DISP PORT BACK END DAI Link */
	{
		.name = LPASS_BE_DISPLAY_PORT,
		.stream_name = "Display Port Playback",
		.cpu_dai_name = "msm-dai-q6-dp.24608",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-ext-disp-audio-codec-rx",
		.codec_dai_name = "msm_dp_audio_codec_rx_dai",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_DISPLAY_PORT_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm_mi2s_be_dai_links[] = {
	{
		.name = LPASS_BE_PRI_MI2S_RX,
		.stream_name = "Primary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_PRI_MI2S_TX,
		.stream_name = "Primary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_PRI_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_MI2S_RX,
		.stream_name = "Tertiary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.2",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_RX,
		.stream_name = "Quaternary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_QUAT_MI2S_TX,
		.stream_name = "Quaternary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm_auxpcm_be_dai_links[] = {
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_aux_pcm_be_ops,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	/* Secondary AUX PCM Backend DAI Links */
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_aux_pcm_be_ops,
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
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	/* Tertiary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_TERT_AUXPCM_RX,
		.stream_name = "Tert AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_TERT_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	{
		.name = LPASS_BE_TERT_AUXPCM_TX,
		.stream_name = "Tert AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.3",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_TERT_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	/* Quaternary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_QUAT_AUXPCM_RX,
		.stream_name = "Quat AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.4",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
	{
		.name = LPASS_BE_QUAT_AUXPCM_TX,
		.stream_name = "Quat AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.4",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.ops = &msm_aux_pcm_be_ops,
	},
};

static struct snd_soc_dai_link msm_tasha_dai_links[
			 ARRAY_SIZE(msm_common_dai_links) +
			 ARRAY_SIZE(msm_tasha_fe_dai_links) +
			 ARRAY_SIZE(msm_common_misc_fe_dai_links) +
			 ARRAY_SIZE(msm_common_be_dai_links) +
			 ARRAY_SIZE(msm_tasha_be_dai_links) +
			 ARRAY_SIZE(msm_wcn_be_dai_links) +
			 ARRAY_SIZE(ext_disp_be_dai_link) +
			 ARRAY_SIZE(msm_mi2s_be_dai_links) +
			 ARRAY_SIZE(msm_auxpcm_be_dai_links)];

static struct snd_soc_dai_link msm_tavil_dai_links[
			 ARRAY_SIZE(msm_common_dai_links) +
			 ARRAY_SIZE(msm_tavil_fe_dai_links) +
			 ARRAY_SIZE(msm_common_misc_fe_dai_links) +
			 ARRAY_SIZE(msm_common_be_dai_links) +
			 ARRAY_SIZE(msm_tavil_be_dai_links) +
			 ARRAY_SIZE(msm_wcn_be_dai_links) +
			 ARRAY_SIZE(ext_disp_be_dai_link) +
			 ARRAY_SIZE(msm_mi2s_be_dai_links) +
			 ARRAY_SIZE(msm_auxpcm_be_dai_links)];

static int msm_snd_card_late_probe(struct snd_soc_card *card)
{
	const char *be_dl_name = LPASS_BE_SLIMBUS_0_RX;
	struct snd_soc_pcm_runtime *rtd;
	int ret = 0;
	void *mbhc_calibration;

	rtd = snd_soc_get_pcm_runtime(card, be_dl_name);
	if (!rtd) {
		dev_err(card->dev,
			"%s: snd_soc_get_pcm_runtime for %s failed!\n",
			__func__, be_dl_name);
		ret = -EINVAL;
		goto err_pcm_runtime;
	}

	mbhc_calibration = def_tasha_mbhc_cal();
	if (!mbhc_calibration) {
		ret = -ENOMEM;
		goto err_mbhc_cal;
	}
	wcd_mbhc_cfg.calibration = mbhc_calibration;
	ret = tasha_mbhc_hs_detect(rtd->codec, &wcd_mbhc_cfg);
	if (ret) {
		dev_err(card->dev, "%s: mbhc hs detect failed, err:%d\n",
			__func__, ret);
		goto err_hs_detect;
	}
	return 0;

err_hs_detect:
	kfree(mbhc_calibration);
err_mbhc_cal:
err_pcm_runtime:
	return ret;
}

static int msm_snd_card_tavil_late_probe(struct snd_soc_card *card)
{
	const char *be_dl_name = LPASS_BE_SLIMBUS_0_RX;
	struct snd_soc_pcm_runtime *rtd;
	int ret = 0;
	void *mbhc_calibration;

	rtd = snd_soc_get_pcm_runtime(card, be_dl_name);
	if (!rtd) {
		dev_err(card->dev,
			"%s: snd_soc_get_pcm_runtime for %s failed!\n",
			__func__, be_dl_name);
		ret = -EINVAL;
		goto err_pcm_runtime;
	}

	mbhc_calibration = def_tavil_mbhc_cal();
	if (!mbhc_calibration) {
		ret = -ENOMEM;
		goto err_mbhc_cal;
	}
	wcd_mbhc_cfg.calibration = mbhc_calibration;
	ret = tavil_mbhc_hs_detect(rtd->codec, &wcd_mbhc_cfg);
	if (ret) {
		dev_err(card->dev, "%s: mbhc hs detect failed, err:%d\n",
			__func__, ret);
		goto err_hs_detect;
	}
	return 0;

err_hs_detect:
	kfree(mbhc_calibration);
err_mbhc_cal:
err_pcm_runtime:
	return ret;
}

struct snd_soc_card snd_soc_card_tasha_msm = {
	.name		= "msm8998-tasha-snd-card",
	.late_probe	= msm_snd_card_late_probe,
};

struct snd_soc_card snd_soc_card_tavil_msm = {
	.name		= "msm8998-tavil-snd-card",
	.late_probe	= msm_snd_card_tavil_late_probe,
};

static int msm_populate_dai_link_component_of_node(
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

static int msm_prepare_us_euro(struct snd_soc_card *card)
{
	struct msm_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);
	int ret = 0;

	if (pdata->us_euro_gpio >= 0) {
		dev_dbg(card->dev, "%s: us_euro gpio request %d", __func__,
			pdata->us_euro_gpio);
		ret = gpio_request(pdata->us_euro_gpio, "TASHA_CODEC_US_EURO");
		if (ret) {
			dev_err(card->dev,
				"%s: Failed to request codec US/EURO gpio %d error %d\n",
				__func__, pdata->us_euro_gpio, ret);
		}
	}

	return ret;
}

static int msm_audrx_stub_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	ret = snd_soc_add_codec_controls(codec, msm_snd_controls,
					 ARRAY_SIZE(msm_snd_controls));
	if (ret < 0) {
		dev_err(codec->dev, "%s: add_codec_controls failed, err%d\n",
			__func__, ret);
		return ret;
	}

	snd_soc_dapm_new_controls(dapm, msm_dapm_widgets,
				ARRAY_SIZE(msm_dapm_widgets));

	return 0;
}

static int msm_snd_stub_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	int ret = 0;
	unsigned int rx_ch[] = {144, 145, 146, 147, 148, 149, 150,
				151, 152, 153, 154, 155, 156};
	unsigned int tx_ch[] = {128, 129, 130, 131, 132, 133,
				134, 135, 136, 137, 138, 139,
				140, 141, 142, 143};

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
						  slim_rx_cfg[0].channels,
						  rx_ch);
		if (ret < 0)
			pr_err("%s: RX failed to set cpu chan map error %d\n",
				__func__, ret);
	} else {
		ret = snd_soc_dai_set_channel_map(cpu_dai,
						  slim_tx_cfg[0].channels,
						  tx_ch, 0, 0);
		if (ret < 0)
			pr_err("%s: TX failed to set cpu chan map error %d\n",
				__func__, ret);
	}

	return ret;
}

static struct snd_soc_ops msm_stub_be_ops = {
	.hw_params = msm_snd_stub_hw_params,
};

static struct snd_soc_dai_link msm_stub_fe_dai_links[] = {

	/* FrontEnd DAI Links */
	{
		.name = "MSMSTUB Media1",
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
};

static struct snd_soc_dai_link msm_stub_be_dai_links[] = {

	/* Backend DAI Links */
	{
		.name = LPASS_BE_SLIMBUS_0_RX,
		.stream_name = "Slimbus Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16384",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_RX,
		.init = &msm_audrx_stub_init,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.ignore_suspend = 1,
		.ops = &msm_stub_be_ops,
	},
	{
		.name = LPASS_BE_SLIMBUS_0_TX,
		.stream_name = "Slimbus Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16385",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm_stub_be_ops,
	},
};

static struct snd_soc_dai_link msm_stub_dai_links[
			 ARRAY_SIZE(msm_stub_fe_dai_links) +
			 ARRAY_SIZE(msm_stub_be_dai_links)];

struct snd_soc_card snd_soc_card_stub_msm = {
	.name		= "msm8998-stub-snd-card",
};

static const struct of_device_id msm8998_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,msm8998-asoc-snd-tasha",
	  .data = "tasha_codec"},
	{ .compatible = "qcom,msm8998-asoc-snd-tavil",
	  .data = "tavil_codec"},
	{ .compatible = "qcom,msm8998-asoc-snd-stub",
	  .data = "stub_codec"},
	{},
};

static struct snd_soc_card *populate_snd_card_dailinks(struct device *dev)
{
	struct snd_soc_card *card = NULL;
	struct snd_soc_dai_link *dailink;
	int len_1, len_2, len_3, len_4;
	int total_links;
	const struct of_device_id *match;

	match = of_match_node(msm8998_asoc_machine_of_match, dev->of_node);
	if (!match) {
		dev_err(dev, "%s: No DT match found for sound card\n",
			__func__);
		return NULL;
	}

	if (!strcmp(match->data, "tasha_codec")) {
		card = &snd_soc_card_tasha_msm;
		len_1 = ARRAY_SIZE(msm_common_dai_links);
		len_2 = len_1 + ARRAY_SIZE(msm_tasha_fe_dai_links);
		len_3 = len_2 + ARRAY_SIZE(msm_common_misc_fe_dai_links);
		len_4 = len_3 + ARRAY_SIZE(msm_common_be_dai_links);
		total_links = len_4 + ARRAY_SIZE(msm_tasha_be_dai_links);
		memcpy(msm_tasha_dai_links,
		       msm_common_dai_links,
		       sizeof(msm_common_dai_links));
		memcpy(msm_tasha_dai_links + len_1,
		       msm_tasha_fe_dai_links,
		       sizeof(msm_tasha_fe_dai_links));
		memcpy(msm_tasha_dai_links + len_2,
		       msm_common_misc_fe_dai_links,
		       sizeof(msm_common_misc_fe_dai_links));
		memcpy(msm_tasha_dai_links + len_3,
		       msm_common_be_dai_links,
		       sizeof(msm_common_be_dai_links));
		memcpy(msm_tasha_dai_links + len_4,
		       msm_tasha_be_dai_links,
		       sizeof(msm_tasha_be_dai_links));

		if (of_property_read_bool(dev->of_node, "qcom,wcn-btfm")) {
			dev_dbg(dev, "%s(): WCN BTFM support present\n",
				__func__);
			memcpy(msm_tasha_dai_links + total_links,
			       msm_wcn_be_dai_links,
			       sizeof(msm_wcn_be_dai_links));
			total_links += ARRAY_SIZE(msm_wcn_be_dai_links);
		}

		if (of_property_read_bool(dev->of_node,
					  "qcom,ext-disp-audio-rx")) {
			dev_dbg(dev, "%s(): External display audio support present\n",
				__func__);
			memcpy(msm_tasha_dai_links + total_links,
			ext_disp_be_dai_link,
			sizeof(ext_disp_be_dai_link));
			total_links += ARRAY_SIZE(ext_disp_be_dai_link);
		}
		if (of_property_read_bool(dev->of_node,
					  "qcom,mi2s-audio-intf")) {
			memcpy(msm_tasha_dai_links + total_links,
			       msm_mi2s_be_dai_links,
			       sizeof(msm_mi2s_be_dai_links));
			total_links += ARRAY_SIZE(msm_mi2s_be_dai_links);
		}
		if (of_property_read_bool(dev->of_node,
					  "qcom,auxpcm-audio-intf")) {
			memcpy(msm_tasha_dai_links + total_links,
			       msm_auxpcm_be_dai_links,
			       sizeof(msm_auxpcm_be_dai_links));
			total_links += ARRAY_SIZE(msm_auxpcm_be_dai_links);
		}
		dailink = msm_tasha_dai_links;
	}  else if (!strcmp(match->data, "tavil_codec")) {
		card = &snd_soc_card_tavil_msm;
		len_1 = ARRAY_SIZE(msm_common_dai_links);
		len_2 = len_1 + ARRAY_SIZE(msm_tavil_fe_dai_links);
		len_3 = len_2 + ARRAY_SIZE(msm_common_misc_fe_dai_links);
		len_4 = len_3 + ARRAY_SIZE(msm_common_be_dai_links);
		total_links = len_4 + ARRAY_SIZE(msm_tavil_be_dai_links);
		memcpy(msm_tavil_dai_links,
		       msm_common_dai_links,
		       sizeof(msm_common_dai_links));
		memcpy(msm_tavil_dai_links + len_1,
		       msm_tavil_fe_dai_links,
		       sizeof(msm_tavil_fe_dai_links));
		memcpy(msm_tavil_dai_links + len_2,
		       msm_common_misc_fe_dai_links,
		       sizeof(msm_common_misc_fe_dai_links));
		memcpy(msm_tavil_dai_links + len_3,
		       msm_common_be_dai_links,
		       sizeof(msm_common_be_dai_links));
		memcpy(msm_tavil_dai_links + len_4,
		       msm_tavil_be_dai_links,
		       sizeof(msm_tavil_be_dai_links));

		if (of_property_read_bool(dev->of_node, "qcom,wcn-btfm")) {
			dev_dbg(dev, "%s(): WCN BTFM support present\n",
				__func__);
			memcpy(msm_tavil_dai_links + total_links,
			       msm_wcn_be_dai_links,
			       sizeof(msm_wcn_be_dai_links));
			total_links += ARRAY_SIZE(msm_wcn_be_dai_links);
		}

		if (of_property_read_bool(dev->of_node,
					  "qcom,ext-disp-audio-rx")) {
			dev_dbg(dev, "%s(): ext disp audio support present\n",
				__func__);
			memcpy(msm_tavil_dai_links + total_links,
			       ext_disp_be_dai_link,
			       sizeof(ext_disp_be_dai_link));
			total_links += ARRAY_SIZE(ext_disp_be_dai_link);
		}
		if (of_property_read_bool(dev->of_node,
					  "qcom,mi2s-audio-intf")) {
			memcpy(msm_tavil_dai_links + total_links,
			       msm_mi2s_be_dai_links,
			       sizeof(msm_mi2s_be_dai_links));
			total_links += ARRAY_SIZE(msm_mi2s_be_dai_links);
		}
		if (of_property_read_bool(dev->of_node,
					  "qcom,auxpcm-audio-intf")) {
			memcpy(msm_tavil_dai_links + total_links,
			msm_auxpcm_be_dai_links,
			sizeof(msm_auxpcm_be_dai_links));
			total_links += ARRAY_SIZE(msm_auxpcm_be_dai_links);
		}
		dailink = msm_tavil_dai_links;
	} else if (!strcmp(match->data, "stub_codec")) {
		card = &snd_soc_card_stub_msm;
		len_1 = ARRAY_SIZE(msm_stub_fe_dai_links);
		len_2 = len_1 + ARRAY_SIZE(msm_stub_be_dai_links);

		memcpy(msm_stub_dai_links,
		       msm_stub_fe_dai_links,
		       sizeof(msm_stub_fe_dai_links));
		memcpy(msm_stub_dai_links + len_1,
		       msm_stub_be_dai_links,
		       sizeof(msm_stub_be_dai_links));

		dailink = msm_stub_dai_links;
		total_links = len_2;
	}

	if (card) {
		card->dai_link = dailink;
		card->num_links = total_links;
	}

	return card;
}

static int msm_wsa881x_init(struct snd_soc_component *component)
{
	u8 spkleft_ports[WSA881X_MAX_SWR_PORTS] = {100, 101, 102, 106};
	u8 spkright_ports[WSA881X_MAX_SWR_PORTS] = {103, 104, 105, 107};
	unsigned int ch_rate[WSA881X_MAX_SWR_PORTS] = {2400, 600, 300, 1200};
	unsigned int ch_mask[WSA881X_MAX_SWR_PORTS] = {0x1, 0xF, 0x3, 0x3};
	struct snd_soc_codec *codec = snd_soc_component_to_codec(component);
	struct msm_asoc_mach_data *pdata;
	struct snd_soc_dapm_context *dapm;
	int ret = 0;

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
		ret = -EINVAL;
		goto err_codec;
	}
	pdata = snd_soc_card_get_drvdata(component->card);
	if (pdata && pdata->codec_root)
		wsa881x_codec_info_create_codec_entry(pdata->codec_root,
						      codec);

err_codec:
	return ret;
}

static int msm_init_wsa_dev(struct platform_device *pdev,
				struct snd_soc_card *card)
{
	struct device_node *wsa_of_node;
	u32 wsa_max_devs;
	u32 wsa_dev_cnt;
	int i;
	struct msm_wsa881x_dev_info *wsa881x_dev_info;
	const char *wsa_auxdev_name_prefix[1];
	char *dev_name_str = NULL;
	int found = 0;
	int ret = 0;

	/* Get maximum WSA device count for this platform */
	ret = of_property_read_u32(pdev->dev.of_node,
				   "qcom,wsa-max-devs", &wsa_max_devs);
	if (ret) {
		dev_dbg(&pdev->dev,
			 "%s: wsa-max-devs property missing in DT %s, ret = %d\n",
			 __func__, pdev->dev.of_node->full_name, ret);
		goto err_dt;
	}
	if (wsa_max_devs == 0) {
		dev_warn(&pdev->dev,
			 "%s: Max WSA devices is 0 for this target?\n",
			 __func__);
		goto err_dt;
	}

	/* Get count of WSA device phandles for this platform */
	wsa_dev_cnt = of_count_phandle_with_args(pdev->dev.of_node,
						 "qcom,wsa-devs", NULL);
	if (wsa_dev_cnt == -ENOENT) {
		dev_warn(&pdev->dev, "%s: No wsa device defined in DT.\n",
			 __func__);
		goto err_dt;
	} else if (wsa_dev_cnt <= 0) {
		dev_err(&pdev->dev,
			"%s: Error reading wsa device from DT. wsa_dev_cnt = %d\n",
			__func__, wsa_dev_cnt);
		ret = -EINVAL;
		goto err_dt;
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
		ret = -EINVAL;
		goto err_dt;
	}

	/*
	 * Alloc mem to store phandle and index info of WSA device, if already
	 * registered with ALSA core
	 */
	wsa881x_dev_info = devm_kcalloc(&pdev->dev, wsa_max_devs,
					sizeof(struct msm_wsa881x_dev_info),
					GFP_KERNEL);
	if (!wsa881x_dev_info) {
		ret = -ENOMEM;
		goto err_mem;
	}

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
			ret = -EINVAL;
			goto err_dev_node;
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
	msm_aux_dev = devm_kcalloc(&pdev->dev, card->num_aux_devs,
				       sizeof(struct snd_soc_aux_dev),
				       GFP_KERNEL);
	if (!msm_aux_dev) {
		ret = -ENOMEM;
		goto err_auxdev_mem;
	}

	/* Alloc array of codec conf struct */
	msm_codec_conf = devm_kcalloc(&pdev->dev, card->num_aux_devs,
					  sizeof(struct snd_soc_codec_conf),
					  GFP_KERNEL);
	if (!msm_codec_conf) {
		ret = -ENOMEM;
		goto err_codec_conf;
	}

	for (i = 0; i < card->num_aux_devs; i++) {
		dev_name_str = devm_kzalloc(&pdev->dev, DEV_NAME_STR_LEN,
					    GFP_KERNEL);
		if (!dev_name_str) {
			ret = -ENOMEM;
			goto err_dev_str;
		}

		ret = of_property_read_string_index(pdev->dev.of_node,
						    "qcom,wsa-aux-dev-prefix",
						    wsa881x_dev_info[i].index,
						    wsa_auxdev_name_prefix);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: failed to read wsa aux dev prefix, ret = %d\n",
				__func__, ret);
			ret = -EINVAL;
			goto err_dt_prop;
		}

		snprintf(dev_name_str, strlen("wsa881x.%d"), "wsa881x.%d", i);
		msm_aux_dev[i].name = dev_name_str;
		msm_aux_dev[i].codec_name = NULL;
		msm_aux_dev[i].codec_of_node =
					wsa881x_dev_info[i].of_node;
		msm_aux_dev[i].init = msm_wsa881x_init;
		msm_codec_conf[i].dev_name = NULL;
		msm_codec_conf[i].name_prefix = wsa_auxdev_name_prefix[0];
		msm_codec_conf[i].of_node =
				wsa881x_dev_info[i].of_node;
	}
	card->codec_conf = msm_codec_conf;
	card->aux_dev = msm_aux_dev;

	return 0;

err_dt_prop:
	devm_kfree(&pdev->dev, dev_name_str);
err_dev_str:
	devm_kfree(&pdev->dev, msm_codec_conf);
err_codec_conf:
	devm_kfree(&pdev->dev, msm_aux_dev);
err_auxdev_mem:
err_dev_node:
	devm_kfree(&pdev->dev, wsa881x_dev_info);
err_mem:
err_dt:
	return ret;
}

static void i2s_auxpcm_init(struct platform_device *pdev)
{
	struct resource *muxsel;
	int count;
	u32 mi2s_master_slave[MI2S_MAX];
	int ret;
	char *str[PCM_I2S_SEL_MAX] = {
		"lpaif_pri_mode_muxsel",
		"lpaif_sec_mode_muxsel",
		"lpaif_tert_mode_muxsel",
		"lpaif_quat_mode_muxsel"
	};

	for (count = 0; count < MI2S_MAX; count++) {
		mutex_init(&mi2s_intf_conf[count].lock);
		mi2s_intf_conf[count].ref_cnt = 0;
	}

	for (count = 0; count < AUX_PCM_MAX; count++) {
		mutex_init(&auxpcm_intf_conf[count].lock);
		auxpcm_intf_conf[count].ref_cnt = 0;
	}

	for (count = 0; count < PCM_I2S_SEL_MAX; count++) {
		mutex_init(&mi2s_auxpcm_conf[count].lock);
		mi2s_auxpcm_conf[count].pcm_i2s_sel_vt_addr = NULL;
	}

	for (count = 0; count < PCM_I2S_SEL_MAX; count++) {
		muxsel = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						      str[count]);
		if (muxsel) {
			mi2s_auxpcm_conf[count].pcm_i2s_sel_vt_addr
				= ioremap(muxsel->start, resource_size(muxsel));
		}
	}

	ret = of_property_read_u32_array(pdev->dev.of_node,
			"qcom,msm-mi2s-master",
			mi2s_master_slave, MI2S_MAX);
	if (ret) {
		dev_dbg(&pdev->dev, "%s: no qcom,msm-mi2s-master in DT node\n",
			__func__);
	} else {
		for (count = 0; count < MI2S_MAX; count++) {
			mi2s_intf_conf[count].msm_is_mi2s_master =
				mi2s_master_slave[count];
		}
	}
}

static void i2s_auxpcm_deinit(void)
{
	int count;

	for (count = 0; count < PCM_I2S_SEL_MAX; count++)
		if (mi2s_auxpcm_conf[count].pcm_i2s_sel_vt_addr !=
			NULL)
			iounmap(
			mi2s_auxpcm_conf[count].pcm_i2s_sel_vt_addr);
}

static int msm_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct msm_asoc_mach_data *pdata;
	const char *mbhc_audio_jack_type = NULL;
	char *mclk_freq_prop_name;
	const struct of_device_id *match;
	int ret;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct msm_asoc_mach_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

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

	match = of_match_node(msm8998_asoc_machine_of_match,
			pdev->dev.of_node);
	if (!match) {
		dev_err(&pdev->dev, "%s: no matched codec is found.\n",
			__func__);
		goto err;
	}

	if (!strcmp(match->data, "tasha_codec"))
		mclk_freq_prop_name = "qcom,tasha-mclk-clk-freq";
	else
		mclk_freq_prop_name = "qcom,tavil-mclk-clk-freq";

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

	ret = msm_populate_dai_link_component_of_node(card);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}
	ret = msm_init_wsa_dev(pdev, card);
	if (ret)
		goto err;

	ret = devm_snd_soc_register_card(&pdev->dev, card);
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
	spdev = pdev;

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret) {
		dev_dbg(&pdev->dev, "%s: failed to add child nodes, ret=%d\n",
			__func__, ret);
	} else {
		pdata->hph_en1_gpio_p = of_parse_phandle(pdev->dev.of_node,
							"qcom,hph-en1-gpio", 0);
		if (!pdata->hph_en1_gpio_p) {
			dev_dbg(&pdev->dev, "property %s not detected in node %s",
				"qcom,hph-en1-gpio",
				pdev->dev.of_node->full_name);
		}

		pdata->hph_en0_gpio_p = of_parse_phandle(pdev->dev.of_node,
							"qcom,hph-en0-gpio", 0);
		if (!pdata->hph_en0_gpio_p) {
			dev_dbg(&pdev->dev, "property %s not detected in node %s",
				"qcom,hph-en0-gpio",
				pdev->dev.of_node->full_name);
		}
	}

	ret = of_property_read_string(pdev->dev.of_node,
		"qcom,mbhc-audio-jack-type", &mbhc_audio_jack_type);
	if (ret) {
		dev_dbg(&pdev->dev, "Looking up %s property in node %s failed",
			"qcom,mbhc-audio-jack-type",
			pdev->dev.of_node->full_name);
		dev_dbg(&pdev->dev, "Jack type properties set to default");
	} else {
		if (!strcmp(mbhc_audio_jack_type, "4-pole-jack")) {
			wcd_mbhc_cfg.enable_anc_mic_detect = false;
			dev_dbg(&pdev->dev, "This hardware has 4 pole jack");
		} else if (!strcmp(mbhc_audio_jack_type, "5-pole-jack")) {
			wcd_mbhc_cfg.enable_anc_mic_detect = true;
			dev_dbg(&pdev->dev, "This hardware has 5 pole jack");
		} else if (!strcmp(mbhc_audio_jack_type, "6-pole-jack")) {
			wcd_mbhc_cfg.enable_anc_mic_detect = true;
			dev_dbg(&pdev->dev, "This hardware has 6 pole jack");
		} else {
			wcd_mbhc_cfg.enable_anc_mic_detect = false;
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
	if (!gpio_is_valid(pdata->us_euro_gpio))
		pdata->us_euro_gpio_p = of_parse_phandle(pdev->dev.of_node,
					"qcom,us-euro-gpios", 0);
	if (!gpio_is_valid(pdata->us_euro_gpio) && (!pdata->us_euro_gpio_p)) {
		dev_dbg(&pdev->dev, "property %s not detected in node %s",
			"qcom,us-euro-gpios", pdev->dev.of_node->full_name);
	} else {
		dev_dbg(&pdev->dev, "%s detected",
			"qcom,us-euro-gpios");
		wcd_mbhc_cfg.swap_gnd_mic = msm_swap_gnd_mic;
	}

	ret = msm_prepare_us_euro(card);
	if (ret)
		dev_dbg(&pdev->dev, "msm_prepare_us_euro failed (%d)\n",
			ret);

	/* Parse pinctrl info from devicetree */
	ret = msm_get_pinctrl(pdev);
	if (!ret) {
		pr_debug("%s: pinctrl parsing successful\n", __func__);
	} else {
		dev_dbg(&pdev->dev,
			"%s: Parsing pinctrl failed with %d. Cannot use Ports\n",
			__func__, ret);
		ret = 0;
	}

	i2s_auxpcm_init(pdev);

	is_initial_boot = true;
	ret = audio_notifier_register("msm8998", AUDIO_NOTIFIER_ADSP_DOMAIN,
				      &service_nb);
	if (ret < 0)
		pr_err("%s: Audio notifier register failed ret = %d\n",
			__func__, ret);

	return 0;
err:
	if (pdata->us_euro_gpio > 0) {
		dev_dbg(&pdev->dev, "%s free us_euro gpio %d\n",
			__func__, pdata->us_euro_gpio);
		gpio_free(pdata->us_euro_gpio);
		pdata->us_euro_gpio = 0;
	}
	msm_release_pinctrl(pdev);
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int msm_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);

	gpio_free(pdata->us_euro_gpio);
	i2s_auxpcm_deinit();

	snd_soc_unregister_card(card);
	return 0;
}

static struct platform_driver msm8998_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = msm8998_asoc_machine_of_match,
	},
	.probe = msm_asoc_machine_probe,
	.remove = msm_asoc_machine_remove,
};
module_platform_driver(msm8998_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, msm8998_asoc_machine_of_match);
