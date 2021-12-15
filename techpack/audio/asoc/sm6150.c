// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/of_device.h>
#include <linux/pm_qos.h>
#include <linux/soc/qcom/fsa4480-i2c.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/info.h>
#include <soc/snd_event.h>
#include <soc/qcom/socinfo.h>
#include <dsp/q6afe-v2.h>
#include <dsp/q6core.h>
#include "device_event.h"
#include "msm-pcm-routing-v2.h"
#include <asoc/msm-cdc-pinctrl.h>
#include "codecs/wcd934x/wcd934x.h"
#include "codecs/wcd9335.h"
#include "codecs/wcd934x/wcd934x-mbhc.h"
#include "codecs/wcd937x/wcd937x-mbhc.h"
#include "codecs/wsa881x.h"
#include "codecs/bolero/bolero-cdc.h"
#include <dt-bindings/sound/audio-codec-port-types.h>
#include "codecs/bolero/wsa-macro.h"
#include "codecs/wcd937x/wcd937x.h"

#define DRV_NAME "sm6150-asoc-snd"

#define __CHIPSET__ "SM6150 "
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
#define TDM_CHANNEL_MAX 8

#define ADSP_STATE_READY_TIMEOUT_MS 3000
#define MSM_LL_QOS_VALUE 300 /* time in us to ensure LPM doesn't go in C3/C4 */
#define MSM_HIFI_ON 1

#define SM6150_SOC_VERSION_1_0 0x00010000
#define SM6150_SOC_MSM_ID 0x163

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
	QUIN_MI2S,
	MI2S_MAX,
};

enum {
	PRIM_AUX_PCM = 0,
	SEC_AUX_PCM,
	TERT_AUX_PCM,
	QUAT_AUX_PCM,
	QUIN_AUX_PCM,
	AUX_PCM_MAX,
};

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
	TDM_QUIN,
	TDM_INTERFACE_MAX,
};

struct tdm_port {
	u32 mode;
	u32 channel;
};

enum {
	WSA_CDC_DMA_RX_0 = 0,
	WSA_CDC_DMA_RX_1,
	RX_CDC_DMA_RX_0,
	RX_CDC_DMA_RX_1,
	RX_CDC_DMA_RX_2,
	RX_CDC_DMA_RX_3,
	RX_CDC_DMA_RX_5,
	CDC_DMA_RX_MAX,
};

enum {
	WSA_CDC_DMA_TX_0 = 0,
	WSA_CDC_DMA_TX_1,
	WSA_CDC_DMA_TX_2,
	TX_CDC_DMA_TX_0,
	TX_CDC_DMA_TX_3,
	TX_CDC_DMA_TX_4,
	CDC_DMA_TX_MAX,
};

struct mi2s_conf {
	struct mutex lock;
	u32 ref_cnt;
	u32 msm_is_mi2s_master;
	u32 msm_is_ext_mclk;
};

static u32 mi2s_ebit_clk[MI2S_MAX] = {
	Q6AFE_LPASS_CLK_ID_PRI_MI2S_EBIT,
	Q6AFE_LPASS_CLK_ID_SEC_MI2S_EBIT,
	Q6AFE_LPASS_CLK_ID_TER_MI2S_EBIT,
	Q6AFE_LPASS_CLK_ID_QUAD_MI2S_EBIT,
	Q6AFE_LPASS_CLK_ID_QUI_MI2S_EBIT
};

struct dev_config {
	u32 sample_rate;
	u32 bit_format;
	u32 channels;
};

enum {
	DP_RX_IDX = 0,
	EXT_DISP_RX_IDX_MAX,
};

struct msm_wsa881x_dev_info {
	struct device_node *of_node;
	u32 index;
};

struct aux_codec_dev_info {
	struct device_node *of_node;
	u32 index;
};

struct msm_asoc_mach_data {
	struct snd_info_entry *codec_root;
	int usbc_en2_gpio; /* used by gpio driver API */
	struct device_node *mi2s_gpio_p[MI2S_MAX]; /* used by pinctrl API */
	int hph_en1_gpio;
	int hph_en0_gpio;
	struct device_node *dmic01_gpio_p; /* used by pinctrl API */
	struct device_node *dmic23_gpio_p; /* used by pinctrl API */
	struct device_node *us_euro_gpio_p; /* used by pinctrl API */
	struct pinctrl *usbc_en2_gpio_p; /* used by pinctrl API */
	struct device_node *hph_en1_gpio_p; /* used by pinctrl API */
	struct device_node *hph_en0_gpio_p; /* used by pinctrl API */
	bool is_afe_config_done;
	struct device_node *fsa_handle;
};

struct msm_asoc_wcd93xx_codec {
	void* (*get_afe_config_fn)(struct snd_soc_component *component,
				   enum afe_config_type config_type);
};

static struct snd_soc_card snd_soc_card_sm6150_msm;

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
	},
	{ /* QUIN TDM */
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
	},
	{ /* QUIN TDM */
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

/* Default configuration of Codec DMA Interface Tx */
static struct dev_config cdc_dma_rx_cfg[] = {
	[WSA_CDC_DMA_RX_0] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[WSA_CDC_DMA_RX_1] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[RX_CDC_DMA_RX_0] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[RX_CDC_DMA_RX_1] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[RX_CDC_DMA_RX_2] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[RX_CDC_DMA_RX_3] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[RX_CDC_DMA_RX_5] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
};

/* Default configuration of Codec DMA Interface Rx */
static struct dev_config cdc_dma_tx_cfg[] = {
	[WSA_CDC_DMA_TX_0] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[WSA_CDC_DMA_TX_1] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[WSA_CDC_DMA_TX_2] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[TX_CDC_DMA_TX_0] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[TX_CDC_DMA_TX_3] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
	[TX_CDC_DMA_TX_4] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
};

/* Default configuration of external display BE */
static struct dev_config ext_disp_rx_cfg[] = {
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
	[QUIN_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2},
};

static struct dev_config mi2s_tx_cfg[] = {
	[PRIM_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SEC_MI2S]  = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[TERT_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUAT_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUIN_MI2S] = {SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};

static struct dev_config aux_pcm_rx_cfg[] = {
	[PRIM_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SEC_AUX_PCM]  = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[TERT_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUAT_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUIN_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};

static struct dev_config aux_pcm_tx_cfg[] = {
	[PRIM_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[SEC_AUX_PCM]  = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[TERT_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUAT_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
	[QUIN_AUX_PCM] = {SAMPLING_RATE_8KHZ, SNDRV_PCM_FORMAT_S16_LE, 1},
};
static int msm_vi_feed_tx_ch = 2;
static const char *const slim_rx_ch_text[] = {"One", "Two"};
static const char *const slim_tx_ch_text[] = {"One", "Two", "Three", "Four",
						"Five", "Six", "Seven",
						"Eight"};
static const char *const vi_feed_ch_text[] = {"One", "Two"};
static char const *bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE",
					  "S32_LE"};
static char const *ext_disp_bit_format_text[] = {"S16_LE", "S24_LE",
						 "S24_3LE"};
static char const *slim_sample_rate_text[] = {"KHZ_8", "KHZ_16",
					"KHZ_32", "KHZ_44P1", "KHZ_48",
					"KHZ_88P2", "KHZ_96", "KHZ_176P4",
					"KHZ_192", "KHZ_352P8", "KHZ_384"};
static char const *bt_sample_rate_text[] = {"KHZ_8", "KHZ_16",
					"KHZ_44P1", "KHZ_48",
					"KHZ_88P2", "KHZ_96"};
static char const *bt_sample_rate_rx_text[] = {"KHZ_8", "KHZ_16",
					"KHZ_44P1", "KHZ_48",
					"KHZ_88P2", "KHZ_96"};
static char const *bt_sample_rate_tx_text[] = {"KHZ_8", "KHZ_16",
					"KHZ_44P1", "KHZ_48",
					"KHZ_88P2", "KHZ_96"};
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
						"KHZ_88P2", "KHZ_176P4"	};
static char const *tdm_ch_text[] = {"One", "Two", "Three", "Four",
				    "Five", "Six", "Seven", "Eight"};
static char const *tdm_bit_format_text[] = {"S16_LE", "S24_LE", "S32_LE"};
static char const *tdm_sample_rate_text[] = {"KHZ_8", "KHZ_16", "KHZ_32",
					     "KHZ_48", "KHZ_176P4",
					     "KHZ_352P8"};
static const char *const auxpcm_rate_text[] = {"KHZ_8", "KHZ_16"};
static char const *mi2s_rate_text[] = {"KHZ_8", "KHZ_11P025", "KHZ_16",
				      "KHZ_22P05", "KHZ_32", "KHZ_44P1",
				      "KHZ_48", "KHZ_96", "KHZ_192"};
static const char *const mi2s_ch_text[] = {"One", "Two", "Three", "Four",
					   "Five", "Six", "Seven",
					   "Eight"};
static const char *const hifi_text[] = {"Off", "On"};
static const char *const qos_text[] = {"Disable", "Enable"};

static const char *const cdc_dma_rx_ch_text[] = {"One", "Two"};
static const char *const cdc_dma_tx_ch_text[] = {"One", "Two", "Three", "Four",
						"Five", "Six", "Seven",
						"Eight"};
static char const *cdc_dma_sample_rate_text[] = {"KHZ_8", "KHZ_11P025",
						 "KHZ_16", "KHZ_22P05",
						 "KHZ_32", "KHZ_44P1", "KHZ_48",
						 "KHZ_88P2", "KHZ_96",
						 "KHZ_176P4", "KHZ_192",
						 "KHZ_352P8", "KHZ_384"};


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
static SOC_ENUM_SINGLE_EXT_DECL(prim_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quin_aux_pcm_rx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quin_aux_pcm_tx_sample_rate, auxpcm_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quin_mi2s_rx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(quin_mi2s_tx_sample_rate, mi2s_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(prim_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(sec_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tert_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(quat_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(quin_mi2s_rx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(quin_mi2s_tx_chs, mi2s_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(mi2s_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(mi2s_tx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(aux_pcm_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(aux_pcm_tx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(hifi_function, hifi_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_rx_0_chs, cdc_dma_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_rx_1_chs, cdc_dma_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_0_chs, cdc_dma_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_1_chs, cdc_dma_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_2_chs, cdc_dma_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_3_chs, cdc_dma_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_5_chs, cdc_dma_rx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_tx_0_chs, cdc_dma_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_tx_1_chs, cdc_dma_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_tx_2_chs, cdc_dma_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_0_chs, cdc_dma_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_3_chs, cdc_dma_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_4_chs, cdc_dma_tx_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_rx_0_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_rx_1_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_0_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_1_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_2_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_3_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_5_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_tx_1_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_tx_2_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_0_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_3_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_4_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_rx_0_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_rx_1_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_0_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_1_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_2_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_3_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(rx_cdc_dma_rx_5_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_tx_0_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_tx_1_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(wsa_cdc_dma_tx_2_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_0_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_3_sample_rate,
				cdc_dma_sample_rate_text);
static SOC_ENUM_SINGLE_EXT_DECL(tx_cdc_dma_tx_4_sample_rate,
				cdc_dma_sample_rate_text);

static int msm_hifi_control;
static bool codec_reg_done;
static struct snd_soc_aux_dev *msm_aux_dev;
static struct snd_soc_codec_conf *msm_codec_conf;
static struct msm_asoc_wcd93xx_codec msm_codec_fn;

static int dmic_0_1_gpio_cnt;
static int dmic_2_3_gpio_cnt;

static void *def_wcd_mbhc_cal(void);
static int msm_snd_enable_codec_ext_clk(struct snd_soc_component *component,
					int enable, bool dapm);
static int msm_wsa881x_init(struct snd_soc_component *component);
static int msm_aux_codec_init(struct snd_soc_component *component);

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
	.moisture_en = false,
	.mbhc_micbias = MIC_BIAS_2,
	.anc_micbias = MIC_BIAS_2,
	.enable_anc_mic_detect = false,
	.moisture_duty_cycle_en = true,
};

static struct snd_soc_dapm_route wcd_audio_paths[] = {
	{"MIC BIAS1", NULL, "MCLK TX"},
	{"MIC BIAS2", NULL, "MCLK TX"},
	{"MIC BIAS3", NULL, "MCLK TX"},
	{"MIC BIAS4", NULL, "MCLK TX"},
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
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_QUI_MI2S_IBIT,
		Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	}

};

static struct afe_clk_set mi2s_mclk[MI2S_MAX] = {
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_MCLK_3,
		Q6AFE_LPASS_OSR_CLK_9_P600_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_MCLK_2,
		Q6AFE_LPASS_OSR_CLK_9_P600_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_MCLK_1,
		Q6AFE_LPASS_OSR_CLK_9_P600_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_MCLK_1,
		Q6AFE_LPASS_OSR_CLK_9_P600_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	},
	{
		AFE_API_VERSION_I2S_CONFIG,
		Q6AFE_LPASS_CLK_ID_QUI_MI2S_OSR,
		Q6AFE_LPASS_OSR_CLK_9_P600_MHZ,
		Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
		Q6AFE_LPASS_CLK_ROOT_DEFAULT,
		0,
	}
};

static struct mi2s_conf mi2s_intf_conf[MI2S_MAX];

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

	if (strnstr(kcontrol->id.name, "SLIM_0_RX", sizeof("SLIM_0_RX"))) {
		port_id = SLIM_RX_0;
	} else if (strnstr(kcontrol->id.name,
					   "SLIM_2_RX", sizeof("SLIM_2_RX"))) {
		port_id = SLIM_RX_2;
	} else if (strnstr(kcontrol->id.name,
					   "SLIM_5_RX", sizeof("SLIM_5_RX"))) {
		port_id = SLIM_RX_5;
	} else if (strnstr(kcontrol->id.name,
					   "SLIM_6_RX", sizeof("SLIM_6_RX"))) {
		port_id = SLIM_RX_6;
	} else if (strnstr(kcontrol->id.name,
					   "SLIM_0_TX", sizeof("SLIM_0_TX"))) {
		port_id = SLIM_TX_0;
	} else if (strnstr(kcontrol->id.name,
					   "SLIM_1_TX", sizeof("SLIM_1_TX"))) {
		port_id = SLIM_TX_1;
	} else {
		pr_err("%s: unsupported channel: %s\n",
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

static int slim_rx_ch_get(struct snd_kcontrol *kcontrol,
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

static int slim_rx_ch_put(struct snd_kcontrol *kcontrol,
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

static int slim_tx_ch_get(struct snd_kcontrol *kcontrol,
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

static int slim_tx_ch_put(struct snd_kcontrol *kcontrol,
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
	case SAMPLING_RATE_96KHZ:
		ucontrol->value.integer.value[0] = 5;
		break;
	case SAMPLING_RATE_88P2KHZ:
		ucontrol->value.integer.value[0] = 4;
		break;
	case SAMPLING_RATE_48KHZ:
		ucontrol->value.integer.value[0] = 3;
		break;
	case SAMPLING_RATE_44P1KHZ:
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
	pr_debug("%s: sample rate = %d\n", __func__,
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
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_44P1KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 3:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_48KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 4:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_88P2KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 5:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_96KHZ;
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_96KHZ;
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
	case SAMPLING_RATE_96KHZ:
		ucontrol->value.integer.value[0] = 5;
		break;
	case SAMPLING_RATE_88P2KHZ:
		ucontrol->value.integer.value[0] = 4;
		break;
	case SAMPLING_RATE_48KHZ:
		ucontrol->value.integer.value[0] = 3;
		break;
	case SAMPLING_RATE_44P1KHZ:
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
	pr_debug("%s: sample rate rx = %d", __func__,
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
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 3:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 4:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 5:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		slim_rx_cfg[SLIM_RX_7].sample_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	pr_debug("%s: sample rate: slim7_rx = %d, value = %d\n",
		 __func__,
		 slim_rx_cfg[SLIM_RX_7].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_bt_sample_rate_tx_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	switch (slim_tx_cfg[SLIM_TX_7].sample_rate) {
	case SAMPLING_RATE_96KHZ:
		ucontrol->value.integer.value[0] = 5;
		break;
	case SAMPLING_RATE_88P2KHZ:
		ucontrol->value.integer.value[0] = 4;
		break;
	case SAMPLING_RATE_48KHZ:
		ucontrol->value.integer.value[0] = 3;
		break;
	case SAMPLING_RATE_44P1KHZ:
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
	pr_debug("%s: sample rate tx = %d", __func__,
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
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 3:
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 4:
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 5:
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 0:
	default:
		slim_tx_cfg[SLIM_TX_7].sample_rate = SAMPLING_RATE_8KHZ;
		break;
	}
	pr_debug("%s: sample rate: slim7_tx = %d, value = %d\n",
		 __func__,
		 slim_tx_cfg[SLIM_TX_7].sample_rate,
		 ucontrol->value.enumerated.item[0]);

	return 0;
}
static int cdc_dma_get_port_idx(struct snd_kcontrol *kcontrol)
{
	int idx = 0;

	if (strnstr(kcontrol->id.name, "WSA_CDC_DMA_RX_0",
		sizeof("WSA_CDC_DMA_RX_0")))
		idx = WSA_CDC_DMA_RX_0;
	else if (strnstr(kcontrol->id.name, "WSA_CDC_DMA_RX_1",
		sizeof("WSA_CDC_DMA_RX_0")))
		idx = WSA_CDC_DMA_RX_1;
	else if (strnstr(kcontrol->id.name, "RX_CDC_DMA_RX_0",
		sizeof("RX_CDC_DMA_RX_0")))
		idx = RX_CDC_DMA_RX_0;
	else if (strnstr(kcontrol->id.name, "RX_CDC_DMA_RX_1",
		sizeof("RX_CDC_DMA_RX_1")))
		idx = RX_CDC_DMA_RX_1;
	else if (strnstr(kcontrol->id.name, "RX_CDC_DMA_RX_2",
		sizeof("RX_CDC_DMA_RX_2")))
		idx = RX_CDC_DMA_RX_2;
	else if (strnstr(kcontrol->id.name, "RX_CDC_DMA_RX_3",
		sizeof("RX_CDC_DMA_RX_3")))
		idx = RX_CDC_DMA_RX_3;
	else if (strnstr(kcontrol->id.name, "RX_CDC_DMA_RX_5",
		sizeof("RX_CDC_DMA_RX_5")))
		idx = RX_CDC_DMA_RX_5;
	else if (strnstr(kcontrol->id.name, "WSA_CDC_DMA_TX_0",
		sizeof("WSA_CDC_DMA_TX_0")))
		idx = WSA_CDC_DMA_TX_0;
	else if (strnstr(kcontrol->id.name, "WSA_CDC_DMA_TX_1",
		sizeof("WSA_CDC_DMA_TX_1")))
		idx = WSA_CDC_DMA_TX_1;
	else if (strnstr(kcontrol->id.name, "WSA_CDC_DMA_TX_2",
		sizeof("WSA_CDC_DMA_TX_2")))
		idx = WSA_CDC_DMA_TX_2;
	else if (strnstr(kcontrol->id.name, "TX_CDC_DMA_TX_0",
		sizeof("TX_CDC_DMA_TX_0")))
		idx = TX_CDC_DMA_TX_0;
	else if (strnstr(kcontrol->id.name, "TX_CDC_DMA_TX_3",
		sizeof("TX_CDC_DMA_TX_3")))
		idx = TX_CDC_DMA_TX_3;
	else if (strnstr(kcontrol->id.name, "TX_CDC_DMA_TX_4",
		sizeof("TX_CDC_DMA_TX_4")))
		idx = TX_CDC_DMA_TX_4;
	else {
		pr_err("%s: unsupported channel: %s\n",
			__func__, kcontrol->id.name);
		return -EINVAL;
	}

	return idx;
}

static int cdc_dma_rx_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	pr_debug("%s: cdc_dma_rx_ch  = %d\n", __func__,
		 cdc_dma_rx_cfg[ch_num].channels - 1);
	ucontrol->value.integer.value[0] = cdc_dma_rx_cfg[ch_num].channels - 1;
	return 0;
}

static int cdc_dma_rx_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	cdc_dma_rx_cfg[ch_num].channels = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: cdc_dma_rx_ch = %d\n", __func__,
		cdc_dma_rx_cfg[ch_num].channels);
	return 1;
}

static int cdc_dma_rx_format_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	switch (cdc_dma_rx_cfg[ch_num].bit_format) {
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

	pr_debug("%s: cdc_dma_rx_format = %d, ucontrol value = %ld\n",
		 __func__, cdc_dma_rx_cfg[ch_num].bit_format,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int cdc_dma_rx_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	switch (ucontrol->value.integer.value[0]) {
	case 3:
		cdc_dma_rx_cfg[ch_num].bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	case 2:
		cdc_dma_rx_cfg[ch_num].bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		cdc_dma_rx_cfg[ch_num].bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		cdc_dma_rx_cfg[ch_num].bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: cdc_dma_rx_format = %d, ucontrol value = %ld\n",
		 __func__, cdc_dma_rx_cfg[ch_num].bit_format,
		 ucontrol->value.integer.value[0]);

	return rc;
}


static int cdc_dma_get_sample_rate_val(int sample_rate)
{
	int sample_rate_val = 0;

	switch (sample_rate) {
	case SAMPLING_RATE_8KHZ:
		sample_rate_val = 0;
		break;
	case SAMPLING_RATE_11P025KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_22P05KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_88P2KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 8;
		break;
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 9;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 10;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 11;
		break;
	case SAMPLING_RATE_384KHZ:
		sample_rate_val = 12;
		break;
	default:
		sample_rate_val = 6;
		break;
	}
	return sample_rate_val;
}

static int cdc_dma_get_sample_rate(int value)
{
	int sample_rate = 0;

	switch (value) {
	case 0:
		sample_rate = SAMPLING_RATE_8KHZ;
		break;
	case 1:
		sample_rate = SAMPLING_RATE_11P025KHZ;
		break;
	case 2:
		sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 3:
		sample_rate = SAMPLING_RATE_22P05KHZ;
		break;
	case 4:
		sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 5:
		sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 6:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 7:
		sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 8:
		sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 9:
		sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 10:
		sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 11:
		sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
	case 12:
		sample_rate = SAMPLING_RATE_384KHZ;
		break;
	default:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}
	return sample_rate;
}

static int cdc_dma_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	ucontrol->value.enumerated.item[0] =
		cdc_dma_get_sample_rate_val(cdc_dma_rx_cfg[ch_num].sample_rate);

	pr_debug("%s: cdc_dma_rx_sample_rate = %d\n", __func__,
		 cdc_dma_rx_cfg[ch_num].sample_rate);
	return 0;
}

static int cdc_dma_rx_sample_rate_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	cdc_dma_rx_cfg[ch_num].sample_rate =
		cdc_dma_get_sample_rate(ucontrol->value.enumerated.item[0]);


	pr_debug("%s: control value = %d, cdc_dma_rx_sample_rate = %d\n",
		__func__, ucontrol->value.enumerated.item[0],
		cdc_dma_rx_cfg[ch_num].sample_rate);
	return 0;
}

static int cdc_dma_tx_ch_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	pr_debug("%s: cdc_dma_tx_ch  = %d\n", __func__,
		 cdc_dma_tx_cfg[ch_num].channels);
	ucontrol->value.integer.value[0] = cdc_dma_tx_cfg[ch_num].channels - 1;
	return 0;
}

static int cdc_dma_tx_ch_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	cdc_dma_tx_cfg[ch_num].channels = ucontrol->value.integer.value[0] + 1;

	pr_debug("%s: cdc_dma_tx_ch = %d\n", __func__,
		cdc_dma_tx_cfg[ch_num].channels);
	return 1;
}

static int cdc_dma_tx_sample_rate_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int sample_rate_val;
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	switch (cdc_dma_tx_cfg[ch_num].sample_rate) {
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
	pr_debug("%s: cdc_dma_tx_sample_rate = %d\n", __func__,
		 cdc_dma_tx_cfg[ch_num].sample_rate);
	return 0;
}

static int cdc_dma_tx_sample_rate_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	switch (ucontrol->value.integer.value[0]) {
	case 12:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_384KHZ;
		break;
	case 11:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_352P8KHZ;
		break;
	case 10:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_192KHZ;
		break;
	case 9:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 8:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_96KHZ;
		break;
	case 7:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_88P2KHZ;
		break;
	case 6:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 5:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 4:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 3:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_22P05KHZ;
		break;
	case 2:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 1:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_11P025KHZ;
		break;
	case 0:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_8KHZ;
		break;
	default:
		cdc_dma_tx_cfg[ch_num].sample_rate = SAMPLING_RATE_48KHZ;
		break;
	}

	pr_debug("%s: control value = %ld, cdc_dma_tx_sample_rate = %d\n",
		__func__, ucontrol->value.integer.value[0],
		cdc_dma_tx_cfg[ch_num].sample_rate);
	return 0;
}

static int cdc_dma_tx_format_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	switch (cdc_dma_tx_cfg[ch_num].bit_format) {
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

	pr_debug("%s: cdc_dma_tx_format = %d, ucontrol value = %ld\n",
		 __func__, cdc_dma_tx_cfg[ch_num].bit_format,
		 ucontrol->value.integer.value[0]);
	return 0;
}

static int cdc_dma_tx_format_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	int ch_num = cdc_dma_get_port_idx(kcontrol);

	if (ch_num < 0) {
		pr_err("%s: ch_num: %d is invalid\n", __func__, ch_num);
		return ch_num;
	}

	switch (ucontrol->value.integer.value[0]) {
	case 3:
		cdc_dma_tx_cfg[ch_num].bit_format = SNDRV_PCM_FORMAT_S32_LE;
		break;
	case 2:
		cdc_dma_tx_cfg[ch_num].bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
	case 1:
		cdc_dma_tx_cfg[ch_num].bit_format = SNDRV_PCM_FORMAT_S24_LE;
		break;
	case 0:
	default:
		cdc_dma_tx_cfg[ch_num].bit_format = SNDRV_PCM_FORMAT_S16_LE;
		break;
	}
	pr_debug("%s: cdc_dma_tx_format = %d, ucontrol value = %ld\n",
		 __func__, cdc_dma_tx_cfg[ch_num].bit_format,
		 ucontrol->value.integer.value[0]);

	return rc;
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

	if (strnstr(kcontrol->id.name, "Display Port RX",
		    sizeof("Display Port RX"))) {
		idx = DP_RX_IDX;
	} else {
		pr_err("%s: unsupported BE: %s\n",
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
	case 2:
		ext_disp_rx_cfg[idx].bit_format = SNDRV_PCM_FORMAT_S24_3LE;
		break;
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
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 4:
		sample_rate = SAMPLING_RATE_176P4KHZ;
		break;
	case 5:
		sample_rate = SAMPLING_RATE_352P8KHZ;
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
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_176P4KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_352P8KHZ:
		sample_rate_val = 5;
		break;
	default:
		sample_rate_val = 3;
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

static int tdm_get_port_idx(struct snd_kcontrol *kcontrol,
			    struct tdm_port *port)
{
	if (port) {
		if (strnstr(kcontrol->id.name, "PRI",
		    sizeof(kcontrol->id.name))) {
			port->mode = TDM_PRI;
		} else if (strnstr(kcontrol->id.name, "SEC",
		    sizeof(kcontrol->id.name))) {
			port->mode = TDM_SEC;
		} else if (strnstr(kcontrol->id.name, "TERT",
		    sizeof(kcontrol->id.name))) {
			port->mode = TDM_TERT;
		} else if (strnstr(kcontrol->id.name, "QUAT",
		    sizeof(kcontrol->id.name))) {
			port->mode = TDM_QUAT;
		} else if (strnstr(kcontrol->id.name, "QUIN",
		    sizeof(kcontrol->id.name))) {
			port->mode = TDM_QUIN;
		} else {
			pr_err("%s: unsupported mode in: %s\n",
				__func__, kcontrol->id.name);
			return -EINVAL;
		}

		if (strnstr(kcontrol->id.name, "RX_0",
		    sizeof(kcontrol->id.name)) ||
		    strnstr(kcontrol->id.name, "TX_0",
		    sizeof(kcontrol->id.name))) {
			port->channel = TDM_0;
		} else if (strnstr(kcontrol->id.name, "RX_1",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_1",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_1;
		} else if (strnstr(kcontrol->id.name, "RX_2",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_2",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_2;
		} else if (strnstr(kcontrol->id.name, "RX_3",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_3",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_3;
		} else if (strnstr(kcontrol->id.name, "RX_4",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_4",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_4;
		} else if (strnstr(kcontrol->id.name, "RX_5",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_5",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_5;
		} else if (strnstr(kcontrol->id.name, "RX_6",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_6",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_6;
		} else if (strnstr(kcontrol->id.name, "RX_7",
			   sizeof(kcontrol->id.name)) ||
			   strnstr(kcontrol->id.name, "TX_7",
			   sizeof(kcontrol->id.name))) {
			port->channel = TDM_7;
		} else {
			pr_err("%s: unsupported channel in: %s\n",
				__func__, kcontrol->id.name);
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}
	return 0;
}

static int tdm_rx_sample_rate_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct tdm_port port;
	int ret = tdm_get_port_idx(kcontrol, &port);

	if (ret) {
		pr_err("%s: unsupported control: %s\n",
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
		pr_err("%s: unsupported control: %s\n",
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
		pr_err("%s: unsupported control: %s\n",
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
		pr_err("%s: unsupported control: %s\n",
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
		pr_err("%s: unsupported control: %s\n",
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
		pr_err("%s: unsupported control: %s\n",
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
		pr_err("%s: unsupported control: %s\n",
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
		pr_err("%s: unsupported control: %s\n",
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
		pr_err("%s: unsupported control: %s\n",
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
		pr_err("%s: unsupported control: %s\n",
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
		pr_err("%s: unsupported control: %s\n",
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
		pr_err("%s: unsupported control: %s\n",
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

static int aux_pcm_get_port_idx(struct snd_kcontrol *kcontrol)
{
	int idx;

	if (strnstr(kcontrol->id.name, "PRIM_AUX_PCM",
		    sizeof("PRIM_AUX_PCM"))) {
		idx = PRIM_AUX_PCM;
	} else if (strnstr(kcontrol->id.name, "SEC_AUX_PCM",
			 sizeof("SEC_AUX_PCM"))) {
		idx = SEC_AUX_PCM;
	} else if (strnstr(kcontrol->id.name, "TERT_AUX_PCM",
			 sizeof("TERT_AUX_PCM"))) {
		idx = TERT_AUX_PCM;
	} else if (strnstr(kcontrol->id.name, "QUAT_AUX_PCM",
			 sizeof("QUAT_AUX_PCM"))) {
		idx = QUAT_AUX_PCM;
	} else if (strnstr(kcontrol->id.name, "QUIN_AUX_PCM",
			 sizeof("QUIN_AUX_PCM"))) {
		idx = QUIN_AUX_PCM;
	} else {
		pr_err("%s: unsupported port: %s\n",
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
	    sizeof("PRIM_MI2S_RX"))) {
		idx = PRIM_MI2S;
	} else if (strnstr(kcontrol->id.name, "SEC_MI2S_RX",
		 sizeof("SEC_MI2S_RX"))) {
		idx = SEC_MI2S;
	} else if (strnstr(kcontrol->id.name, "TERT_MI2S_RX",
		 sizeof("TERT_MI2S_RX"))) {
		idx = TERT_MI2S;
	} else if (strnstr(kcontrol->id.name, "QUAT_MI2S_RX",
		 sizeof("QUAT_MI2S_RX"))) {
		idx = QUAT_MI2S;
	} else if (strnstr(kcontrol->id.name, "QUIN_MI2S_RX",
		 sizeof("QUIN_MI2S_RX"))) {
		idx = QUIN_MI2S;
	} else if (strnstr(kcontrol->id.name, "PRIM_MI2S_TX",
		 sizeof("PRIM_MI2S_TX"))) {
		idx = PRIM_MI2S;
	} else if (strnstr(kcontrol->id.name, "SEC_MI2S_TX",
		 sizeof("SEC_MI2S_TX"))) {
		idx = SEC_MI2S;
	} else if (strnstr(kcontrol->id.name, "TERT_MI2S_TX",
		 sizeof("TERT_MI2S_TX"))) {
		idx = TERT_MI2S;
	} else if (strnstr(kcontrol->id.name, "QUAT_MI2S_TX",
		 sizeof("QUAT_MI2S_TX"))) {
		idx = QUAT_MI2S;
	} else if (strnstr(kcontrol->id.name, "QUIN_MI2S_TX",
		 sizeof("QUIN_MI2S_TX"))) {
		idx = QUIN_MI2S;
	} else {
		pr_err("%s: unsupported channel: %s\n",
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
	case SAMPLING_RATE_11P025KHZ:
		sample_rate_val = 1;
		break;
	case SAMPLING_RATE_16KHZ:
		sample_rate_val = 2;
		break;
	case SAMPLING_RATE_22P05KHZ:
		sample_rate_val = 3;
		break;
	case SAMPLING_RATE_32KHZ:
		sample_rate_val = 4;
		break;
	case SAMPLING_RATE_44P1KHZ:
		sample_rate_val = 5;
		break;
	case SAMPLING_RATE_48KHZ:
		sample_rate_val = 6;
		break;
	case SAMPLING_RATE_96KHZ:
		sample_rate_val = 7;
		break;
	case SAMPLING_RATE_192KHZ:
		sample_rate_val = 8;
		break;
	default:
		sample_rate_val = 6;
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
		sample_rate = SAMPLING_RATE_11P025KHZ;
		break;
	case 2:
		sample_rate = SAMPLING_RATE_16KHZ;
		break;
	case 3:
		sample_rate = SAMPLING_RATE_22P05KHZ;
		break;
	case 4:
		sample_rate = SAMPLING_RATE_32KHZ;
		break;
	case 5:
		sample_rate = SAMPLING_RATE_44P1KHZ;
		break;
	case 6:
		sample_rate = SAMPLING_RATE_48KHZ;
		break;
	case 7:
		sample_rate = SAMPLING_RATE_96KHZ;
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

static int mi2s_auxpcm_get_format(int value)
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

static int mi2s_auxpcm_get_format_value(int format)
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
		mi2s_auxpcm_get_format_value(mi2s_rx_cfg[idx].bit_format);

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
		mi2s_auxpcm_get_format(ucontrol->value.enumerated.item[0]);

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
		mi2s_auxpcm_get_format_value(mi2s_tx_cfg[idx].bit_format);

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
		mi2s_auxpcm_get_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_tx_format = %d, item = %d\n", __func__,
		  idx, mi2s_tx_cfg[idx].bit_format,
		  ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_aux_pcm_rx_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		mi2s_auxpcm_get_format_value(aux_pcm_rx_cfg[idx].bit_format);

	pr_debug("%s: idx[%d]_rx_format = %d, item = %d\n", __func__,
		idx, aux_pcm_rx_cfg[idx].bit_format,
		ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_aux_pcm_rx_format_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	aux_pcm_rx_cfg[idx].bit_format =
		mi2s_auxpcm_get_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_rx_format = %d, item = %d\n", __func__,
		  idx, aux_pcm_rx_cfg[idx].bit_format,
		  ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_aux_pcm_tx_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	ucontrol->value.enumerated.item[0] =
		mi2s_auxpcm_get_format_value(aux_pcm_tx_cfg[idx].bit_format);

	pr_debug("%s: idx[%d]_tx_format = %d, item = %d\n", __func__,
		idx, aux_pcm_tx_cfg[idx].bit_format,
		ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_aux_pcm_tx_format_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int idx = aux_pcm_get_port_idx(kcontrol);

	if (idx < 0)
		return idx;

	aux_pcm_tx_cfg[idx].bit_format =
		mi2s_auxpcm_get_format(ucontrol->value.enumerated.item[0]);

	pr_debug("%s: idx[%d]_tx_format = %d, item = %d\n", __func__,
		  idx, aux_pcm_tx_cfg[idx].bit_format,
		  ucontrol->value.enumerated.item[0]);

	return 0;
}

static int msm_hifi_ctrl(struct snd_soc_component *component)
{
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(component);
	struct snd_soc_card *card = component->card;
	struct msm_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);

	dev_dbg(component->dev, "%s: msm_hifi_control = %d\n", __func__,
		msm_hifi_control);

	if (!pdata || !pdata->hph_en1_gpio_p) {
		dev_err(component->dev, "%s: hph_en1_gpio is invalid\n",
			__func__);
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
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);

	dev_dbg(component->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	msm_hifi_control = ucontrol->value.integer.value[0];
	msm_hifi_ctrl(component);

	return 0;
}

static const struct snd_kcontrol_new msm_int_snd_controls[] = {
	SOC_ENUM_EXT("WSA_CDC_DMA_RX_0 Channels", wsa_cdc_dma_rx_0_chs,
			cdc_dma_rx_ch_get, cdc_dma_rx_ch_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_RX_1 Channels", wsa_cdc_dma_rx_1_chs,
			cdc_dma_rx_ch_get, cdc_dma_rx_ch_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_0 Channels", rx_cdc_dma_rx_0_chs,
			cdc_dma_rx_ch_get, cdc_dma_rx_ch_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_1 Channels", rx_cdc_dma_rx_1_chs,
			cdc_dma_rx_ch_get, cdc_dma_rx_ch_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_2 Channels", rx_cdc_dma_rx_2_chs,
			cdc_dma_rx_ch_get, cdc_dma_rx_ch_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_3 Channels", rx_cdc_dma_rx_3_chs,
			cdc_dma_rx_ch_get, cdc_dma_rx_ch_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_5 Channels", rx_cdc_dma_rx_5_chs,
			cdc_dma_rx_ch_get, cdc_dma_rx_ch_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_TX_0 Channels", wsa_cdc_dma_tx_0_chs,
			cdc_dma_tx_ch_get, cdc_dma_tx_ch_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_TX_1 Channels", wsa_cdc_dma_tx_1_chs,
			cdc_dma_tx_ch_get, cdc_dma_tx_ch_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_TX_2 Channels", wsa_cdc_dma_tx_2_chs,
			cdc_dma_tx_ch_get, cdc_dma_tx_ch_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_0 Channels", tx_cdc_dma_tx_0_chs,
			cdc_dma_tx_ch_get, cdc_dma_tx_ch_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_3 Channels", tx_cdc_dma_tx_3_chs,
			cdc_dma_tx_ch_get, cdc_dma_tx_ch_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_4 Channels", tx_cdc_dma_tx_4_chs,
			cdc_dma_tx_ch_get, cdc_dma_tx_ch_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_RX_0 Format", wsa_cdc_dma_rx_0_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_RX_1 Format", wsa_cdc_dma_rx_1_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_0 Format", rx_cdc_dma_rx_0_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_1 Format", rx_cdc_dma_rx_1_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_2 Format", rx_cdc_dma_rx_2_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_3 Format", rx_cdc_dma_rx_3_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_5 Format", rx_cdc_dma_rx_5_format,
			cdc_dma_rx_format_get, cdc_dma_rx_format_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_TX_1 Format", wsa_cdc_dma_tx_1_format,
			cdc_dma_tx_format_get, cdc_dma_tx_format_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_TX_2 Format", wsa_cdc_dma_tx_2_format,
			cdc_dma_tx_format_get, cdc_dma_tx_format_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_0 Format", tx_cdc_dma_tx_0_format,
			cdc_dma_tx_format_get, cdc_dma_tx_format_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_3 Format", tx_cdc_dma_tx_3_format,
			cdc_dma_tx_format_get, cdc_dma_tx_format_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_4 Format", tx_cdc_dma_tx_4_format,
			cdc_dma_tx_format_get, cdc_dma_tx_format_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_RX_0 SampleRate",
			wsa_cdc_dma_rx_0_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_RX_1 SampleRate",
			wsa_cdc_dma_rx_1_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_0 SampleRate",
			rx_cdc_dma_rx_0_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_1 SampleRate",
			rx_cdc_dma_rx_1_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_2 SampleRate",
			rx_cdc_dma_rx_2_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_3 SampleRate",
			rx_cdc_dma_rx_3_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("RX_CDC_DMA_RX_5 SampleRate",
			rx_cdc_dma_rx_5_sample_rate,
			cdc_dma_rx_sample_rate_get,
			cdc_dma_rx_sample_rate_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_TX_0 SampleRate",
			wsa_cdc_dma_tx_0_sample_rate,
			cdc_dma_tx_sample_rate_get,
			cdc_dma_tx_sample_rate_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_TX_1 SampleRate",
			wsa_cdc_dma_tx_1_sample_rate,
			cdc_dma_tx_sample_rate_get,
			cdc_dma_tx_sample_rate_put),
	SOC_ENUM_EXT("WSA_CDC_DMA_TX_2 SampleRate",
			wsa_cdc_dma_tx_2_sample_rate,
			cdc_dma_tx_sample_rate_get,
			cdc_dma_tx_sample_rate_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_0 SampleRate",
			tx_cdc_dma_tx_0_sample_rate,
			cdc_dma_tx_sample_rate_get,
			cdc_dma_tx_sample_rate_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_3 SampleRate",
			tx_cdc_dma_tx_3_sample_rate,
			cdc_dma_tx_sample_rate_get,
			cdc_dma_tx_sample_rate_put),
	SOC_ENUM_EXT("TX_CDC_DMA_TX_4 SampleRate",
			tx_cdc_dma_tx_4_sample_rate,
			cdc_dma_tx_sample_rate_get,
			cdc_dma_tx_sample_rate_put),
};

static const struct snd_kcontrol_new msm_ext_snd_controls[] = {
	SOC_ENUM_EXT("SLIM_0_RX Channels", slim_0_rx_chs,
			slim_rx_ch_get, slim_rx_ch_put),
	SOC_ENUM_EXT("SLIM_2_RX Channels", slim_2_rx_chs,
			slim_rx_ch_get, slim_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_TX Channels", slim_0_tx_chs,
			slim_tx_ch_get, slim_tx_ch_put),
	SOC_ENUM_EXT("SLIM_1_TX Channels", slim_1_tx_chs,
			slim_tx_ch_get, slim_tx_ch_put),
	SOC_ENUM_EXT("SLIM_5_RX Channels", slim_5_rx_chs,
			slim_rx_ch_get, slim_rx_ch_put),
	SOC_ENUM_EXT("SLIM_6_RX Channels", slim_6_rx_chs,
			slim_rx_ch_get, slim_rx_ch_put),
	SOC_ENUM_EXT("SLIM_0_RX Format", slim_0_rx_format,
			slim_rx_bit_format_get, slim_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_5_RX Format", slim_5_rx_format,
			slim_rx_bit_format_get, slim_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_6_RX Format", slim_6_rx_format,
			slim_rx_bit_format_get, slim_rx_bit_format_put),
	SOC_ENUM_EXT("SLIM_0_TX Format", slim_0_tx_format,
			slim_tx_bit_format_get, slim_tx_bit_format_put),
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
};

static const struct snd_kcontrol_new msm_common_snd_controls[] = {
	SOC_ENUM_EXT("USB_AUDIO_RX Channels", usb_rx_chs,
			usb_audio_rx_ch_get, usb_audio_rx_ch_put),
	SOC_ENUM_EXT("USB_AUDIO_TX Channels", usb_tx_chs,
			usb_audio_tx_ch_get, usb_audio_tx_ch_put),
	SOC_ENUM_EXT("Display Port RX Channels", ext_disp_rx_chs,
			ext_disp_rx_ch_get, ext_disp_rx_ch_put),
	SOC_ENUM_EXT("PROXY_RX Channels", proxy_rx_chs,
			proxy_rx_ch_get, proxy_rx_ch_put),
	SOC_ENUM_EXT("USB_AUDIO_RX Format", usb_rx_format,
			usb_audio_rx_format_get, usb_audio_rx_format_put),
	SOC_ENUM_EXT("USB_AUDIO_TX Format", usb_tx_format,
			usb_audio_tx_format_get, usb_audio_tx_format_put),
	SOC_ENUM_EXT("Display Port RX Bit Format", ext_disp_rx_format,
			ext_disp_rx_format_get, ext_disp_rx_format_put),
	SOC_ENUM_EXT("USB_AUDIO_RX SampleRate", usb_rx_sample_rate,
			usb_audio_rx_sample_rate_get,
			usb_audio_rx_sample_rate_put),
	SOC_ENUM_EXT("USB_AUDIO_TX SampleRate", usb_tx_sample_rate,
			usb_audio_tx_sample_rate_get,
			usb_audio_tx_sample_rate_put),
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
	SOC_ENUM_EXT("QUIN_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
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
	SOC_ENUM_EXT("QUIN_AUX_PCM_RX SampleRate", quin_aux_pcm_rx_sample_rate,
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
	SOC_ENUM_EXT("QUIN_AUX_PCM_TX SampleRate", quin_aux_pcm_tx_sample_rate,
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
	SOC_ENUM_EXT("QUIN_MI2S_RX SampleRate", quin_mi2s_rx_sample_rate,
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
	SOC_ENUM_EXT("QUIN_MI2S_TX SampleRate", quin_mi2s_tx_sample_rate,
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
	SOC_ENUM_EXT("QUIN_MI2S_RX Channels", quin_mi2s_rx_chs,
			msm_mi2s_rx_ch_get, msm_mi2s_rx_ch_put),
	SOC_ENUM_EXT("QUIN_MI2S_TX Channels", quin_mi2s_tx_chs,
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
	SOC_ENUM_EXT("QUIN_MI2S_RX Format", mi2s_rx_format,
			msm_mi2s_rx_format_get, msm_mi2s_rx_format_put),
	SOC_ENUM_EXT("QUIN_MI2S_TX Format", mi2s_tx_format,
			msm_mi2s_tx_format_get, msm_mi2s_tx_format_put),
	SOC_ENUM_EXT("PRIM_AUX_PCM_RX Format", aux_pcm_rx_format,
			msm_aux_pcm_rx_format_get, msm_aux_pcm_rx_format_put),
	SOC_ENUM_EXT("PRIM_AUX_PCM_TX Format", aux_pcm_tx_format,
			msm_aux_pcm_tx_format_get, msm_aux_pcm_tx_format_put),
	SOC_ENUM_EXT("SEC_AUX_PCM_RX Format", aux_pcm_rx_format,
			msm_aux_pcm_rx_format_get, msm_aux_pcm_rx_format_put),
	SOC_ENUM_EXT("SEC_AUX_PCM_TX Format", aux_pcm_tx_format,
			msm_aux_pcm_tx_format_get, msm_aux_pcm_tx_format_put),
	SOC_ENUM_EXT("TERT_AUX_PCM_RX Format", aux_pcm_rx_format,
			msm_aux_pcm_rx_format_get, msm_aux_pcm_rx_format_put),
	SOC_ENUM_EXT("TERT_AUX_PCM_TX Format", aux_pcm_tx_format,
			msm_aux_pcm_tx_format_get, msm_aux_pcm_tx_format_put),
	SOC_ENUM_EXT("QUAT_AUX_PCM_RX Format", aux_pcm_rx_format,
			msm_aux_pcm_rx_format_get, msm_aux_pcm_rx_format_put),
	SOC_ENUM_EXT("QUAT_AUX_PCM_TX Format", aux_pcm_tx_format,
			msm_aux_pcm_tx_format_get, msm_aux_pcm_tx_format_put),
	SOC_ENUM_EXT("QUIN_AUX_PCM_RX Format", aux_pcm_rx_format,
			msm_aux_pcm_rx_format_get, msm_aux_pcm_rx_format_put),
	SOC_ENUM_EXT("QUIN_AUX_PCM_TX Format", aux_pcm_tx_format,
			msm_aux_pcm_tx_format_get, msm_aux_pcm_tx_format_put),
	SOC_ENUM_EXT("HiFi Function", hifi_function, msm_hifi_get,
			msm_hifi_put),
	SOC_ENUM_EXT("BT SampleRate", bt_sample_rate,
			msm_bt_sample_rate_get,
			msm_bt_sample_rate_put),
	SOC_ENUM_EXT("BT SampleRate RX", bt_sample_rate_rx,
			msm_bt_sample_rate_rx_get,
			msm_bt_sample_rate_rx_put),
	SOC_ENUM_EXT("BT SampleRate TX", bt_sample_rate_tx,
			msm_bt_sample_rate_tx_get,
			msm_bt_sample_rate_tx_put),
	SOC_ENUM_EXT("VI_FEED_TX Channels", vi_feed_tx_chs,
			msm_vi_feed_tx_ch_get, msm_vi_feed_tx_ch_put),
};

static int msm_snd_enable_codec_ext_clk(struct snd_soc_component *component,
					int enable, bool dapm)
{
	int ret = 0;

	if (!strcmp(component->name, "tavil_codec")) {
		ret = tavil_cdc_mclk_enable(component, enable);
	} else if (!strcmp(dev_name(component->dev), "tasha_codec")) {
		ret = tasha_cdc_mclk_enable(component, enable, dapm);
	} else {
		dev_err(component->dev, "%s: unknown codec to enable ext clk\n",
			__func__);
		ret = -EINVAL;
	}
	return ret;
}

static int msm_snd_enable_codec_ext_tx_clk(struct snd_soc_component *component,
					   int enable, bool dapm)
{
	int ret = 0;

	if (!strcmp(component->name, "tavil_codec")) {
		ret = tavil_cdc_mclk_tx_enable(component, enable);
	} else if (!strcmp(dev_name(component->dev), "tasha_codec")) {
		ret = tasha_cdc_mclk_tx_enable(component, enable, dapm);
	} else {
		dev_err(component->dev, "%s: unknown codec to enable TX ext clk\n",
			__func__);
		ret = -EINVAL;
	}

	return ret;
}

static int msm_mclk_tx_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);

	pr_debug("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return msm_snd_enable_codec_ext_tx_clk(component, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return msm_snd_enable_codec_ext_tx_clk(component, 0, true);
	}
	return 0;
}

static int msm_mclk_event(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);

	pr_debug("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		return msm_snd_enable_codec_ext_clk(component, 1, true);
	case SND_SOC_DAPM_POST_PMD:
		return msm_snd_enable_codec_ext_clk(component, 0, true);
	}
	return 0;
}

static int msm_hifi_ctrl_event(struct snd_soc_dapm_widget *w,
			       struct snd_kcontrol *k, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct snd_soc_card *card = component->card;
	struct msm_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);

	dev_dbg(component->dev, "%s: msm_hifi_control = %d\n",
		__func__, msm_hifi_control);

	if (!pdata || !pdata->hph_en0_gpio_p) {
		dev_err(component->dev, "%s: hph_en0_gpio is invalid\n",
			__func__);
		return -EINVAL;
	}

	if (msm_hifi_control != MSM_HIFI_ON) {
		dev_dbg(component->dev, "%s: HiFi mixer control is not set\n",
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

static const struct snd_soc_dapm_widget msm_ext_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("MCLK",  SND_SOC_NOPM, 0, 0,
			    msm_mclk_event,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("MCLK TX",  SND_SOC_NOPM, 0, 0,
	msm_mclk_tx_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SPK("Lineout_1 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_2 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_3 amp", NULL),
	SND_SOC_DAPM_SPK("Lineout_4 amp", NULL),
	SND_SOC_DAPM_SPK("hifi amp", msm_hifi_ctrl_event),
	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Secondary Mic", NULL),
	SND_SOC_DAPM_MIC("ANCRight Headset Mic", NULL),
	SND_SOC_DAPM_MIC("ANCLeft Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Analog Mic4", NULL),
	SND_SOC_DAPM_MIC("Analog Mic5", NULL),
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

static int msm_dmic_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct msm_asoc_mach_data *pdata = NULL;
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	int ret = 0;
	u32 dmic_idx;
	int *dmic_gpio_cnt;
	struct device_node *dmic_gpio;
	char  *wname;

	wname = strpbrk(w->name, "0123");
	if (!wname) {
		dev_err(component->dev, "%s: widget not found\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(wname, 10, &dmic_idx);
	if (ret < 0) {
		dev_err(component->dev, "%s: Invalid DMIC line on the codec\n",
			__func__);
		return -EINVAL;
	}

	pdata = snd_soc_card_get_drvdata(component->card);

	switch (dmic_idx) {
	case 0:
	case 1:
		dmic_gpio_cnt = &dmic_0_1_gpio_cnt;
		dmic_gpio = pdata->dmic01_gpio_p;
		break;
	case 2:
	case 3:
		dmic_gpio_cnt = &dmic_2_3_gpio_cnt;
		dmic_gpio = pdata->dmic23_gpio_p;
		break;
	default:
		dev_err(component->dev, "%s: Invalid DMIC Selection\n",
			__func__);
		return -EINVAL;
	}

	dev_dbg(component->dev, "%s: event %d DMIC%d dmic_gpio_cnt %d\n",
			__func__, event, dmic_idx, *dmic_gpio_cnt);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		(*dmic_gpio_cnt)++;
		if (*dmic_gpio_cnt == 1) {
			ret = msm_cdc_pinctrl_select_active_state(
						dmic_gpio);
			if (ret < 0) {
				pr_err("%s: gpio set cannot be activated %sd",
					__func__, "dmic_gpio");
				return ret;
			}
		}

		break;
	case SND_SOC_DAPM_POST_PMD:
		(*dmic_gpio_cnt)--;
		if (*dmic_gpio_cnt == 0) {
			ret = msm_cdc_pinctrl_select_sleep_state(
					dmic_gpio);
			if (ret < 0) {
				pr_err("%s: gpio set cannot be de-activated %sd",
					__func__, "dmic_gpio");
				return ret;
			}
		}
		break;
	default:
		pr_err("%s: invalid DAPM event %d\n", __func__, event);
		return -EINVAL;
	}
	return 0;
}

static const struct snd_soc_dapm_widget msm_int_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Analog Mic1", NULL),
	SND_SOC_DAPM_MIC("Analog Mic2", NULL),
	SND_SOC_DAPM_MIC("Analog Mic3", NULL),
	SND_SOC_DAPM_MIC("Analog Mic4", NULL),
	SND_SOC_DAPM_MIC("Digital Mic0", msm_dmic_event),
	SND_SOC_DAPM_MIC("Digital Mic1", msm_dmic_event),
	SND_SOC_DAPM_MIC("Digital Mic2", msm_dmic_event),
	SND_SOC_DAPM_MIC("Digital Mic3", msm_dmic_event),
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

static void param_set_mask(struct snd_pcm_hw_params *p, int n,
			   unsigned int bit)
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

static int msm_cdc_dma_get_idx_from_beid(int32_t be_id)
{
	int idx = 0;

	switch (be_id) {
	case MSM_BACKEND_DAI_WSA_CDC_DMA_RX_0:
		idx = WSA_CDC_DMA_RX_0;
		break;
	case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_0:
		idx = WSA_CDC_DMA_TX_0;
		break;
	case MSM_BACKEND_DAI_WSA_CDC_DMA_RX_1:
		idx = WSA_CDC_DMA_RX_1;
		break;
	case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_1:
		idx = WSA_CDC_DMA_TX_1;
		break;
	case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_2:
		idx = WSA_CDC_DMA_TX_2;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_0:
		idx = RX_CDC_DMA_RX_0;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_1:
		idx = RX_CDC_DMA_RX_1;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_2:
		idx = RX_CDC_DMA_RX_2;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_3:
		idx = RX_CDC_DMA_RX_3;
		break;
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_5:
		idx = RX_CDC_DMA_RX_5;
		break;
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_0:
		idx = TX_CDC_DMA_TX_0;
		break;
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_3:
		idx = TX_CDC_DMA_TX_3;
		break;
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_4:
		idx = TX_CDC_DMA_TX_4;
		break;
	default:
		idx = RX_CDC_DMA_RX_0;
		break;
	}

	return idx;
}

static int msm_ext_disp_get_idx_from_beid(int32_t be_id)
{
	int idx = -EINVAL;

	switch (be_id) {
	case MSM_BACKEND_DAI_DISPLAY_PORT_RX:
		idx = DP_RX_IDX;
		break;
	default:
		pr_err("%s: Incorrect ext_disp BE id %d\n", __func__, be_id);
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
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	int rc = 0;
	int idx;
	void *config = NULL;
	struct snd_soc_component *component = NULL;

	pr_debug("%s: format = %d, rate = %d\n",
		  __func__, params_format(params), params_rate(params));

	switch (dai_link->id) {
	case MSM_BACKEND_DAI_SLIMBUS_0_RX:
	case MSM_BACKEND_DAI_SLIMBUS_1_RX:
	case MSM_BACKEND_DAI_SLIMBUS_2_RX:
	case MSM_BACKEND_DAI_SLIMBUS_3_RX:
	case MSM_BACKEND_DAI_SLIMBUS_4_RX:
	case MSM_BACKEND_DAI_SLIMBUS_6_RX:
		idx = msm_slim_get_ch_from_beid(dai_link->id);
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				slim_rx_cfg[idx].bit_format);
		rate->min = rate->max = slim_rx_cfg[idx].sample_rate;
		channels->min = channels->max = slim_rx_cfg[idx].channels;
		break;

	case MSM_BACKEND_DAI_SLIMBUS_0_TX:
	case MSM_BACKEND_DAI_SLIMBUS_3_TX:
		idx = msm_slim_get_ch_from_beid(dai_link->id);
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
		component = snd_soc_rtdcom_lookup(rtd, "tavil_codec");
		if (!component) {
			pr_err("%s: component is NULL\n", __func__);
			rc = -EINVAL;
			goto done;
		}

		rate->min = rate->max = SAMPLING_RATE_16KHZ;
		channels->min = channels->max = 1;

		config = msm_codec_fn.get_afe_config_fn(component,
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

	case MSM_BACKEND_DAI_DISPLAY_PORT_RX:
		idx = msm_ext_disp_get_idx_from_beid(dai_link->id);
		if (idx < 0) {
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

	case MSM_BACKEND_DAI_QUIN_TDM_RX_0:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUIN][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_rx_cfg[TDM_QUIN][TDM_0].bit_format);
		rate->min = rate->max = tdm_rx_cfg[TDM_QUIN][TDM_0].sample_rate;
		break;

	case MSM_BACKEND_DAI_QUIN_TDM_TX_0:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUIN][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       tdm_tx_cfg[TDM_QUIN][TDM_0].bit_format);
		rate->min = rate->max = tdm_tx_cfg[TDM_QUIN][TDM_0].sample_rate;
		break;


	case MSM_BACKEND_DAI_AUXPCM_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_rx_cfg[PRIM_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_rx_cfg[PRIM_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[PRIM_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_AUXPCM_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_tx_cfg[PRIM_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_tx_cfg[PRIM_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[PRIM_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_SEC_AUXPCM_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_rx_cfg[SEC_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_rx_cfg[SEC_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[SEC_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_SEC_AUXPCM_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_tx_cfg[SEC_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_tx_cfg[SEC_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[SEC_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_TERT_AUXPCM_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_rx_cfg[TERT_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_rx_cfg[TERT_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[TERT_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_TERT_AUXPCM_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_tx_cfg[TERT_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_tx_cfg[TERT_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[TERT_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_QUAT_AUXPCM_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_rx_cfg[QUAT_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_rx_cfg[QUAT_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[QUAT_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_QUAT_AUXPCM_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_tx_cfg[QUAT_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_tx_cfg[QUAT_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[QUAT_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_QUIN_AUXPCM_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_rx_cfg[QUIN_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_rx_cfg[QUIN_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_rx_cfg[QUIN_AUX_PCM].channels;
		break;

	case MSM_BACKEND_DAI_QUIN_AUXPCM_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			aux_pcm_tx_cfg[QUIN_AUX_PCM].bit_format);
		rate->min = rate->max =
			aux_pcm_tx_cfg[QUIN_AUX_PCM].sample_rate;
		channels->min = channels->max =
			aux_pcm_tx_cfg[QUIN_AUX_PCM].channels;
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

	case MSM_BACKEND_DAI_QUINARY_MI2S_RX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_rx_cfg[QUIN_MI2S].bit_format);
		rate->min = rate->max = mi2s_rx_cfg[QUIN_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_rx_cfg[QUIN_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_QUINARY_MI2S_TX:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			mi2s_tx_cfg[QUIN_MI2S].bit_format);
		rate->min = rate->max = mi2s_tx_cfg[QUIN_MI2S].sample_rate;
		channels->min = channels->max =
			mi2s_tx_cfg[QUIN_MI2S].channels;
		break;

	case MSM_BACKEND_DAI_WSA_CDC_DMA_RX_0:
	case MSM_BACKEND_DAI_WSA_CDC_DMA_RX_1:
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_0:
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_1:
	case MSM_BACKEND_DAI_RX_CDC_DMA_RX_2:
		idx = msm_cdc_dma_get_idx_from_beid(dai_link->id);
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				cdc_dma_rx_cfg[idx].bit_format);
		rate->min = rate->max = cdc_dma_rx_cfg[idx].sample_rate;
		channels->min = channels->max = cdc_dma_rx_cfg[idx].channels;
		break;

	case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_1:
	case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_2:
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_0:
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_3:
	case MSM_BACKEND_DAI_TX_CDC_DMA_TX_4:
		idx = msm_cdc_dma_get_idx_from_beid(dai_link->id);
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				cdc_dma_tx_cfg[idx].bit_format);
		rate->min = rate->max = cdc_dma_tx_cfg[idx].sample_rate;
		channels->min = channels->max = cdc_dma_tx_cfg[idx].channels;
		break;

	case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_0:
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
			       SNDRV_PCM_FORMAT_S32_LE);
		rate->min = rate->max = SAMPLING_RATE_8KHZ;
		channels->min = channels->max = msm_vi_feed_tx_ch;
		break;

	default:
		rate->min = rate->max = SAMPLING_RATE_48KHZ;
		break;
	}

done:
	return rc;
}

static bool msm_usbc_swap_gnd_mic(struct snd_soc_component *component,
		bool active)
{
	struct snd_soc_card *card = component->card;
	struct msm_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(card);

	if (!pdata->fsa_handle)
		return false;

	return fsa4480_switch_event(pdata->fsa_handle, FSA_MIC_GND_SWAP);
}

static bool msm_swap_gnd_mic(struct snd_soc_component *component, bool active)
{
	int value = 0;
	bool ret = false;
	struct snd_soc_card *card;
	struct msm_asoc_mach_data *pdata;

	if (!component) {
		pr_err("%s component is NULL\n", __func__);
		return false;
	}
	card = component->card;
	pdata = snd_soc_card_get_drvdata(card);

	if (!pdata)
		return false;

	if (wcd_mbhc_cfg.enable_usbc_analog)
		return msm_usbc_swap_gnd_mic(component, active);

	/* if usbc is not defined, swap using us_euro_gpio_p */
	if (pdata->us_euro_gpio_p) {
		value = msm_cdc_pinctrl_get_state(
				pdata->us_euro_gpio_p);
		if (value)
			msm_cdc_pinctrl_select_sleep_state(
					pdata->us_euro_gpio_p);
		else
			msm_cdc_pinctrl_select_active_state(
					pdata->us_euro_gpio_p);
		dev_dbg(component->dev, "%s: swap select switch %d to %d\n",
			__func__, value, !value);
		ret = true;
	}
	return ret;
}

static int msm_afe_set_config(struct snd_soc_component *component)
{
	int ret = 0;
	void *config_data = NULL;

	if (!msm_codec_fn.get_afe_config_fn) {
		dev_err(component->dev, "%s: codec get afe config not init'ed\n",
			__func__);
		return -EINVAL;
	}

	config_data = msm_codec_fn.get_afe_config_fn(component,
			AFE_CDC_REGISTERS_CONFIG);
	if (config_data) {
		ret = afe_set_config(AFE_CDC_REGISTERS_CONFIG, config_data, 0);
		if (ret) {
			dev_err(component->dev,
				"%s: Failed to set codec registers config %d\n",
				__func__, ret);
			return ret;
		}
	}

	config_data = msm_codec_fn.get_afe_config_fn(component,
			AFE_CDC_REGISTER_PAGE_CONFIG);
	if (config_data) {
		ret = afe_set_config(AFE_CDC_REGISTER_PAGE_CONFIG, config_data,
				    0);
		if (ret)
			dev_err(component->dev,
				"%s: Failed to set cdc register page config\n",
				__func__);
	}

	config_data = msm_codec_fn.get_afe_config_fn(component,
			AFE_SLIMBUS_SLAVE_CONFIG);
	if (config_data) {
		ret = afe_set_config(AFE_SLIMBUS_SLAVE_CONFIG, config_data, 0);
		if (ret) {
			dev_err(component->dev,
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

static int msm_audrx_tavil_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	void *config_data;
	struct snd_soc_component *component = NULL;
	struct snd_soc_dapm_context *dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_component *aux_comp;
	struct snd_card *card = rtd->card->snd_card;
	struct snd_info_entry *entry;
	struct msm_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(rtd->card);

	/*
	 * Codec SLIMBUS configuration
	 * RX1, RX2, RX3, RX4, RX5, RX6, RX7, RX8
	 * TX1, TX2, TX3, TX4, TX5, TX6, TX7, TX8, TX9, TX10, TX11, TX12, TX13
	 * TX14, TX15, TX16
	 */
	unsigned int rx_ch[WCD934X_RX_MAX] = {144, 145, 146, 147, 148, 149,
					      150, 151};
	unsigned int tx_ch[WCD934X_TX_MAX] = {128, 129, 130, 131, 132, 133,
					      134, 135, 136, 137, 138, 139,
					      140, 141, 142, 143};

	pr_info("%s: dev_name:%s\n", __func__, dev_name(cpu_dai->dev));

	rtd->pmdown_time = 0;

	component = snd_soc_rtdcom_lookup(rtd, "tavil_codec");
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}
	dapm = snd_soc_component_get_dapm(component);

	ret = snd_soc_add_component_controls(component, msm_ext_snd_controls,
					 ARRAY_SIZE(msm_ext_snd_controls));
	if (ret < 0) {
		pr_err("%s: add_codec_controls failed, err %d\n",
			__func__, ret);
		return ret;
	}

	ret = snd_soc_add_component_controls(component, msm_common_snd_controls,
					 ARRAY_SIZE(msm_common_snd_controls));
	if (ret < 0) {
		pr_err("%s: add_codec_controls failed, err %d\n",
			__func__, ret);
		return ret;
	}

	snd_soc_dapm_new_controls(dapm, msm_ext_dapm_widgets,
				ARRAY_SIZE(msm_ext_dapm_widgets));

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

	snd_soc_dapm_sync(dapm);

	snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch),
				    tx_ch, ARRAY_SIZE(rx_ch), rx_ch);

	msm_codec_fn.get_afe_config_fn = tavil_get_afe_config;

	ret = msm_afe_set_config(component);
	if (ret) {
		pr_err("%s: Failed to set AFE config %d\n", __func__, ret);
		goto err;
	}
	pdata->is_afe_config_done = true;

	config_data = msm_codec_fn.get_afe_config_fn(component,
						     AFE_AANC_VERSION);
	if (config_data) {
		ret = afe_set_config(AFE_AANC_VERSION, config_data, 0);
		if (ret) {
			pr_err("%s: Failed to set aanc version %d\n",
				__func__, ret);
			goto err;
		}
	}

	/*
	 * Send speaker configuration only for WSA8810.
	 * Default configuration is for WSA8815.
	 */
	pr_debug("%s: Number of aux devices: %d\n",
		__func__, rtd->card->num_aux_devs);
	if (rtd->card->num_aux_devs &&
	    !list_empty(&rtd->card->aux_comp_list)) {
		aux_comp = list_first_entry(&rtd->card->aux_comp_list,
				struct snd_soc_component, card_aux_list);
		if (!strcmp(aux_comp->name, WSA8810_NAME_1) ||
		    !strcmp(aux_comp->name, WSA8810_NAME_2)) {
			tavil_set_spkr_mode(component, WCD934X_SPKR_MODE_1);
			tavil_set_spkr_gain_offset(component,
					WCD934X_RX_GAIN_OFFSET_M1P5_DB);
		}
	}

	card = rtd->card->snd_card;
	if (!pdata->codec_root) {
		entry = snd_info_create_subdir(card->module, "codecs",
						 card->proc_root);
		if (!entry) {
			pr_debug("%s: Cannot create codecs module entry\n",
				 __func__);
			ret = 0;
			goto err;
		}
		pdata->codec_root = entry;
	}
	tavil_codec_info_create_codec_entry(pdata->codec_root, component);

	codec_reg_done = true;
	return 0;
err:
	return ret;
}

static int msm_audrx_tasha_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	void *config_data;
	struct snd_soc_component *component = NULL;
	struct snd_soc_dapm_context *dapm;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_component *aux_comp;
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
	unsigned int tx_ch[TASHA_TX_MAX]  = {128, 129, 130, 131, 132, 133,
					     134, 135, 136, 137, 138, 139,
					     140, 141, 142, 143};

	pr_info("%s: dev_name:%s\n", __func__, dev_name(cpu_dai->dev));

	rtd->pmdown_time = 0;

	component = snd_soc_rtdcom_lookup(rtd, "tasha_codec");
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	dapm = snd_soc_component_get_dapm(component);

	ret = snd_soc_add_component_controls(component, msm_ext_snd_controls,
					 ARRAY_SIZE(msm_ext_snd_controls));
	if (ret < 0) {
		pr_err("%s: add_component_controls failed, err %d\n",
			__func__, ret);
		return ret;
	}

	ret = snd_soc_add_component_controls(component, msm_common_snd_controls,
					 ARRAY_SIZE(msm_common_snd_controls));
	if (ret < 0) {
		pr_err("%s: add_component_controls failed, err %d\n",
			__func__, ret);
		return ret;
	}

	snd_soc_dapm_new_controls(dapm, msm_ext_dapm_widgets,
				ARRAY_SIZE(msm_ext_dapm_widgets));

	snd_soc_dapm_add_routes(dapm, wcd_audio_paths,
				ARRAY_SIZE(wcd_audio_paths));

	snd_soc_dapm_enable_pin(dapm, "Lineout_1 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_2 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_3 amp");
	snd_soc_dapm_enable_pin(dapm, "Lineout_4 amp");

	snd_soc_dapm_ignore_suspend(dapm, "MADINPUT");
	snd_soc_dapm_ignore_suspend(dapm, "MAD_CPE_INPUT");
	snd_soc_dapm_ignore_suspend(dapm, "Handset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Secondary Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_1 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_2 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_3 amp");
	snd_soc_dapm_ignore_suspend(dapm, "Lineout_4 amp");
	snd_soc_dapm_ignore_suspend(dapm, "ANCRight Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "ANCLeft Headset Mic");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic0");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic1");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic2");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic3");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic4");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic5");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic4");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic6");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic7");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic8");

	snd_soc_dapm_ignore_suspend(dapm, "EAR");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT1");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT2");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC4");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC5");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC0");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC3");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC4");
	snd_soc_dapm_ignore_suspend(dapm, "DMIC5");
	snd_soc_dapm_ignore_suspend(dapm, "ANC EAR");
	snd_soc_dapm_ignore_suspend(dapm, "SPK1 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "SPK2 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "HPHL");
	snd_soc_dapm_ignore_suspend(dapm, "HPHR");
	snd_soc_dapm_ignore_suspend(dapm, "AIF4 VI");
	snd_soc_dapm_ignore_suspend(dapm, "VIINPUT");

	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT3");
	snd_soc_dapm_ignore_suspend(dapm, "LINEOUT4");
	snd_soc_dapm_ignore_suspend(dapm, "ANC HPHL");
	snd_soc_dapm_ignore_suspend(dapm, "ANC HPHR");
	snd_soc_dapm_ignore_suspend(dapm, "ANC LINEOUT1");
	snd_soc_dapm_ignore_suspend(dapm, "ANC LINEOUT2");

	snd_soc_dapm_sync(dapm);

	snd_soc_dai_set_channel_map(codec_dai, ARRAY_SIZE(tx_ch),
				    tx_ch, ARRAY_SIZE(rx_ch), rx_ch);

	msm_codec_fn.get_afe_config_fn = tasha_get_afe_config;

	ret = msm_afe_set_config(component);
	if (ret) {
		pr_err("%s: Failed to set AFE config %d\n", __func__, ret);
		goto err;
	}
	pdata->is_afe_config_done = true;

	config_data = msm_codec_fn.get_afe_config_fn(component,
						     AFE_AANC_VERSION);
	if (config_data) {
		ret = afe_set_config(AFE_AANC_VERSION, config_data, 0);
		if (ret) {
			pr_err("%s: Failed to set aanc version %d\n",
				__func__, ret);
			goto err;
		}
	}

	/*
	 * Send speaker configuration only for WSA8810.
	 * Default configuration is for WSA8815.
	 */
	pr_debug("%s: Number of aux devices: %d\n",
		__func__, rtd->card->num_aux_devs);
	if (rtd->card->num_aux_devs &&
	    !list_empty(&rtd->card->aux_comp_list)) {
		aux_comp = list_first_entry(&rtd->card->aux_comp_list,
				struct snd_soc_component, card_aux_list);
		if (!strcmp(aux_comp->name, WSA8810_NAME_1) ||
		    !strcmp(aux_comp->name, WSA8810_NAME_2)) {
			tasha_set_spkr_mode(component, SPKR_MODE_1);
			tasha_set_spkr_gain_offset(component,
					RX_GAIN_OFFSET_M1P5_DB);
		}
	}

	card = rtd->card->snd_card;
	if (!pdata->codec_root) {
		entry = snd_info_create_subdir(card->module, "codecs",
						 card->proc_root);
		if (!entry) {
			pr_debug("%s: Cannot create codecs module entry\n",
				 __func__);
			ret = 0;
			goto err;
		}
		pdata->codec_root = entry;
	}
	tasha_codec_info_create_codec_entry(pdata->codec_root, component);

	codec_reg_done = true;
	return 0;
err:
	return ret;
}

static int msm_int_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_soc_component *component;
	struct snd_soc_dapm_context *dapm;
	struct snd_card *card;
	struct snd_info_entry *entry;
	struct snd_soc_component *aux_comp;
	struct msm_asoc_mach_data *pdata =
				snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	component = snd_soc_rtdcom_lookup(rtd, "bolero_codec");
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}
	dapm = snd_soc_component_get_dapm(component);

	ret = snd_soc_add_component_controls(component, msm_int_snd_controls,
				ARRAY_SIZE(msm_int_snd_controls));
	if (ret < 0) {
		pr_err("%s: add_component_controls failed: %d\n",
			__func__, ret);
		return ret;
	}
	ret = snd_soc_add_component_controls(component, msm_common_snd_controls,
				ARRAY_SIZE(msm_common_snd_controls));
	if (ret < 0) {
		pr_err("%s: add common snd controls failed: %d\n",
			__func__, ret);
		return ret;
	}

	snd_soc_dapm_new_controls(dapm, msm_int_dapm_widgets,
				ARRAY_SIZE(msm_int_dapm_widgets));

	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic0");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic1");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic2");
	snd_soc_dapm_ignore_suspend(dapm, "Digital Mic3");

	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic1");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic2");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic3");
	snd_soc_dapm_ignore_suspend(dapm, "Analog Mic4");

	snd_soc_dapm_ignore_suspend(dapm, "WSA_SPK1 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "WSA_SPK2 OUT");
	snd_soc_dapm_ignore_suspend(dapm, "WSA AIF VI");
	snd_soc_dapm_ignore_suspend(dapm, "VIINPUT_WSA");

	snd_soc_dapm_sync(dapm);

	/*
	 * Send speaker configuration only for WSA8810.
	 * Default configuration is for WSA8815.
	 */
	dev_dbg(component->dev, "%s: Number of aux devices: %d\n",
		__func__, rtd->card->num_aux_devs);
	if (rtd->card->num_aux_devs &&
	    !list_empty(&rtd->card->aux_comp_list)) {
		list_for_each_entry(aux_comp, &rtd->card->aux_comp_list,
				card_aux_list) {
			if (!strcmp(aux_comp->name, WSA8810_NAME_1) ||
			    !strcmp(aux_comp->name, WSA8810_NAME_2)) {
				wsa_macro_set_spkr_mode(component,
							WSA_MACRO_SPKR_MODE_1);
				wsa_macro_set_spkr_gain_offset(component,
						WSA_MACRO_GAIN_OFFSET_M1P5_DB);
				break;
			}
		}
	}
	card = rtd->card->snd_card;
	if (!pdata->codec_root) {
		entry = snd_info_create_subdir(card->module, "codecs",
						 card->proc_root);
		if (!entry) {
			pr_debug("%s: Cannot create codecs module entry\n",
				 __func__);
			ret = 0;
			goto err;
		}
		pdata->codec_root = entry;
	}
	bolero_info_create_codec_entry(pdata->codec_root, codec);
	/*
	 * SM6150 MSM 1.0 doesn't have hardware wake up interrupt line
	 * from AOSS to APSS. So, it uses SW workaround and listens to
	 * interrupt from AFE over IPC.
	 * Check for MSM version and MSM ID and register wake irq
	 * accordingly to provide compatibility to all chipsets.
	 */
	if (socinfo_get_id() == SM6150_SOC_MSM_ID &&
	    socinfo_get_version() == SM6150_SOC_VERSION_1_0)
		bolero_register_wake_irq(component, true);
	else
		bolero_register_wake_irq(component, false);

	codec_reg_done = true;
	return 0;
err:
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

static void *def_wcd_mbhc_cal(void)
{
	void *wcd_mbhc_cal;
	struct wcd_mbhc_btn_detect_cfg *btn_cfg;
	u16 *btn_high;

	wcd_mbhc_cal = kzalloc(WCD_MBHC_CAL_SIZE(WCD_MBHC_DEF_BUTTONS,
				WCD9XXX_MBHC_DEF_RLOADS), GFP_KERNEL);
	if (!wcd_mbhc_cal)
		return NULL;

#define S(X, Y) ((WCD_MBHC_CAL_PLUG_TYPE_PTR(wcd_mbhc_cal)->X) = (Y))
	S(v_hs_max, 1600);
#undef S
#define S(X, Y) ((WCD_MBHC_CAL_BTN_DET_PTR(wcd_mbhc_cal)->X) = (Y))
	S(num_btn, WCD_MBHC_DEF_BUTTONS);
#undef S

	btn_cfg = WCD_MBHC_CAL_BTN_DET_PTR(wcd_mbhc_cal);
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

	return wcd_mbhc_cal;
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
			goto err;
		}
		if (dai_link->id == MSM_BACKEND_DAI_SLIMBUS_5_RX) {
			pr_debug("%s: rx_5_ch=%d\n", __func__,
				  slim_rx_cfg[5].channels);
			rx_ch_count = slim_rx_cfg[5].channels;
		} else if (dai_link->id == MSM_BACKEND_DAI_SLIMBUS_2_RX) {
			pr_debug("%s: rx_2_ch=%d\n", __func__,
				 slim_rx_cfg[2].channels);
			rx_ch_count = slim_rx_cfg[2].channels;
		} else if (dai_link->id == MSM_BACKEND_DAI_SLIMBUS_6_RX) {
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
			goto err;
		}
	} else {

		pr_debug("%s: %s_tx_dai_id_%d_ch=%d\n", __func__,
			 codec_dai->name, codec_dai->id, user_set_tx_ch);
		ret = snd_soc_dai_get_channel_map(codec_dai,
					 &tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get tx codec chan map, err:%d\n",
				__func__, ret);
			goto err;
		}
		/* For <codec>_tx1 case */
		if (dai_link->id == MSM_BACKEND_DAI_SLIMBUS_0_TX)
			user_set_tx_ch = slim_tx_cfg[0].channels;
		/* For <codec>_tx3 case */
		else if (dai_link->id == MSM_BACKEND_DAI_SLIMBUS_1_TX)
			user_set_tx_ch = slim_tx_cfg[1].channels;
		else if (dai_link->id == MSM_BACKEND_DAI_SLIMBUS_4_TX)
			user_set_tx_ch = msm_vi_feed_tx_ch;
		else
			user_set_tx_ch = tx_ch_cnt;

		pr_debug("%s: msm_slim_0_tx_ch(%d) user_set_tx_ch(%d) tx_ch_cnt(%d), BE id (%d)\n",
			 __func__,  slim_tx_cfg[0].channels, user_set_tx_ch,
			 tx_ch_cnt, dai_link->id);

		ret = snd_soc_dai_set_channel_map(cpu_dai,
						  user_set_tx_ch, tx_ch, 0, 0);
		if (ret < 0)
			pr_err("%s: failed to set tx cpu chan map, err:%d\n",
				__func__, ret);
	}

err:
	return ret;
}


static int msm_snd_cdc_dma_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;

	int ret = 0;
	u32 rx_ch_cdc_dma, tx_ch_cdc_dma;
	u32 rx_ch_cnt = 0, tx_ch_cnt = 0;
	u32 user_set_tx_ch = 0;
	u32 user_set_rx_ch = 0;
	u32 ch_id;

	ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, &tx_ch_cdc_dma, &rx_ch_cnt,
				&rx_ch_cdc_dma);
	if (ret < 0) {
		pr_err("%s: failed to get codec chan map, err:%d\n",
			__func__, ret);
		goto err;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (dai_link->id) {
		case MSM_BACKEND_DAI_WSA_CDC_DMA_RX_0:
		case MSM_BACKEND_DAI_WSA_CDC_DMA_RX_1:
		case MSM_BACKEND_DAI_RX_CDC_DMA_RX_0:
		case MSM_BACKEND_DAI_RX_CDC_DMA_RX_1:
		case MSM_BACKEND_DAI_RX_CDC_DMA_RX_2:
		case MSM_BACKEND_DAI_RX_CDC_DMA_RX_3:
		case MSM_BACKEND_DAI_RX_CDC_DMA_RX_4:
		case MSM_BACKEND_DAI_RX_CDC_DMA_RX_5:
		{
			ch_id = msm_cdc_dma_get_idx_from_beid(dai_link->id);
			pr_debug("%s: id %d rx_ch=%d\n", __func__,
				ch_id, cdc_dma_rx_cfg[ch_id].channels);
			user_set_rx_ch = cdc_dma_rx_cfg[ch_id].channels;
			ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
					  user_set_rx_ch, &rx_ch_cdc_dma);
			if (ret < 0) {
				pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
				goto err;
			}

		}
		break;
		}
	} else {
		switch (dai_link->id) {
		case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_0:
		{
			user_set_tx_ch = msm_vi_feed_tx_ch;
		}
		break;
		case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_1:
		case MSM_BACKEND_DAI_WSA_CDC_DMA_TX_2:
		case MSM_BACKEND_DAI_TX_CDC_DMA_TX_0:
		case MSM_BACKEND_DAI_TX_CDC_DMA_TX_3:
		case MSM_BACKEND_DAI_TX_CDC_DMA_TX_4:
		{
			ch_id = msm_cdc_dma_get_idx_from_beid(dai_link->id);
			pr_debug("%s: id %d tx_ch=%d\n", __func__,
				ch_id, cdc_dma_tx_cfg[ch_id].channels);
			user_set_tx_ch = cdc_dma_tx_cfg[ch_id].channels;
		}
		break;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai, user_set_tx_ch,
					&tx_ch_cdc_dma, 0, 0);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map, err:%d\n",
			__func__, ret);
			goto err;
		}
	}

err:
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
			goto err;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
				num_rx_ch, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to set cpu chan map, err:%d\n",
				__func__, ret);
			goto err;
		}
	} else {
		num_tx_ch =  params_channels(params);
		pr_debug("%s: %s  tx_dai_id = %d  num_ch = %d\n", __func__,
			codec_dai->name, codec_dai->id, num_tx_ch);
		ret = snd_soc_dai_get_channel_map(codec_dai,
				&tx_ch_cnt, tx_ch, &rx_ch_cnt, rx_ch);
		if (ret < 0) {
			pr_err("%s: failed to get tx codec chan map, err:%d\n",
				__func__, ret);
			goto err;
		}
		ret = snd_soc_dai_set_channel_map(cpu_dai,
				num_tx_ch, tx_ch, 0, 0);
		if (ret < 0) {
			pr_err("%s: failed to set tx cpu chan map, err:%d\n",
				__func__, ret);
			goto err;
		}
	}

err:
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
		goto err;
	}

	dev_dbg(rtd->dev, "%s: tx_ch_cnt(%d) BE id %d\n",
		__func__, tx_ch_cnt, dai_link->id);

	ret = snd_soc_dai_set_channel_map(cpu_dai,
					  tx_ch_cnt, tx_ch, rx_ch_cnt, rx_ch);
	if (ret)
		dev_err(rtd->dev, "%s: failed to set cpu chan map, err:%d\n",
			__func__, ret);

err:
	return ret;
}

int msm_snd_cpe_hw_params(struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	int ret = 0;
	u32 tx_ch[SLIM_MAX_TX_PORTS];
	u32 tx_ch_cnt = 0;

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

	pr_debug("%s: tx_ch_cnt(%d) id %d\n",
		 __func__, tx_ch_cnt, dai_link->id);

	ret = snd_soc_dai_set_channel_map(cpu_dai,
					  tx_ch_cnt, tx_ch, 0, 0);
	if (ret < 0) {
		pr_err("%s: failed to set cpu chan map, err:%d\n",
			__func__, ret);
		goto end;
	}
end:
	return ret;
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
	case MSM_BACKEND_DAI_QUINARY_MI2S_RX:
		afe_port_id = AFE_PORT_ID_QUINARY_MI2S_RX;
		break;
	case MSM_BACKEND_DAI_QUINARY_MI2S_TX:
		afe_port_id = AFE_PORT_ID_QUINARY_MI2S_TX;
		break;
	default:
		pr_err("%s: Invalid BE id: %d\n", __func__, be_id);
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

	port_id = msm_get_port_id(rtd->dai_link->id);
	if (port_id < 0) {
		dev_err(rtd->card->dev, "%s: Invalid port_id\n", __func__);
		ret = port_id;
		goto err;
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
		goto err;
	}

err:
	return ret;
}

static int sm6150_tdm_snd_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	int slot_width = 32;
	int channels, slots;
	unsigned int slot_mask, rate, clk_freq;
	unsigned int slot_offset[8] = {0, 4, 8, 12, 16, 20, 24, 28};

	pr_debug("%s: dai id = 0x%x\n", __func__, cpu_dai->id);

	/* currently only supporting TDM_RX_0 and TDM_TX_0 */
	switch (cpu_dai->id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
		slots = tdm_rx_cfg[TDM_PRI][TDM_0].channels;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
		slots = tdm_rx_cfg[TDM_SEC][TDM_0].channels;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_RX:
		slots = tdm_rx_cfg[TDM_TERT][TDM_0].channels;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
		slots = tdm_rx_cfg[TDM_QUAT][TDM_0].channels;
		break;
	case AFE_PORT_ID_QUINARY_TDM_RX:
		slots = tdm_rx_cfg[TDM_QUIN][TDM_0].channels;
		break;
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		slots = tdm_tx_cfg[TDM_PRI][TDM_0].channels;
		break;
	case AFE_PORT_ID_SECONDARY_TDM_TX:
		slots = tdm_tx_cfg[TDM_SEC][TDM_0].channels;
		break;
	case AFE_PORT_ID_TERTIARY_TDM_TX:
		slots = tdm_tx_cfg[TDM_TERT][TDM_0].channels;
		break;
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
		slots = tdm_tx_cfg[TDM_QUAT][TDM_0].channels;
		break;
	case AFE_PORT_ID_QUINARY_TDM_TX:
		slots = tdm_tx_cfg[TDM_QUIN][TDM_0].channels;
		break;

	default:
		pr_err("%s: dai id 0x%x not supported\n",
			__func__, cpu_dai->id);
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/*2 slot config - bits 0 and 1 set for the first two slots */
		slot_mask = 0x0000FFFF >> (16-slots);
		channels = slots;

		pr_debug("%s: tdm rx slot_width %d slots %d\n",
			__func__, slot_width, slots);

		ret = snd_soc_dai_set_tdm_slot(cpu_dai, 0, slot_mask,
			slots, slot_width);
		if (ret < 0) {
			pr_err("%s: failed to set tdm rx slot, err:%d\n",
				__func__, ret);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai,
			0, NULL, channels, slot_offset);
		if (ret < 0) {
			pr_err("%s: failed to set tdm rx channel map, err:%d\n",
				__func__, ret);
			goto end;
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		/*2 slot config - bits 0 and 1 set for the first two slots */
		slot_mask = 0x0000FFFF >> (16-slots);
		channels = slots;

		pr_debug("%s: tdm tx slot_width %d slots %d\n",
			__func__, slot_width, slots);

		ret = snd_soc_dai_set_tdm_slot(cpu_dai, slot_mask, 0,
			slots, slot_width);
		if (ret < 0) {
			pr_err("%s: failed to set tdm tx slot, err:%d\n",
				__func__, ret);
			goto end;
		}

		ret = snd_soc_dai_set_channel_map(cpu_dai,
			channels, slot_offset, 0, NULL);
		if (ret < 0) {
			pr_err("%s: failed to set tdm tx channel map, err:%d\n",
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
	if (ret < 0)
		pr_err("%s: failed to set tdm clk, err:%d\n",
			__func__, ret);

end:
	return ret;
}

static int msm_get_tdm_mode(u32 port_id)
{
	int tdm_mode;

	switch (port_id) {
	case AFE_PORT_ID_PRIMARY_TDM_RX:
	case AFE_PORT_ID_PRIMARY_TDM_TX:
		tdm_mode = TDM_PRI;
	break;
	case AFE_PORT_ID_SECONDARY_TDM_RX:
	case AFE_PORT_ID_SECONDARY_TDM_TX:
		tdm_mode = TDM_SEC;
	break;
	case AFE_PORT_ID_TERTIARY_TDM_RX:
	case AFE_PORT_ID_TERTIARY_TDM_TX:
		tdm_mode = TDM_TERT;
	break;
	case AFE_PORT_ID_QUATERNARY_TDM_RX:
	case AFE_PORT_ID_QUATERNARY_TDM_TX:
		tdm_mode = TDM_QUAT;
	break;
	case AFE_PORT_ID_QUINARY_TDM_RX:
	case AFE_PORT_ID_QUINARY_TDM_TX:
		tdm_mode = TDM_QUIN;
	break;
	default:
		pr_err("%s: Invalid port id: %d\n", __func__, port_id);
		tdm_mode = -EINVAL;
	}
	return tdm_mode;
}

static int sm6150_tdm_snd_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int tdm_mode = msm_get_tdm_mode(cpu_dai->id);

	if (tdm_mode < 0) {
		dev_err(rtd->card->dev, "%s: Invalid tdm_mode\n", __func__);
		return tdm_mode;
	}

	/* currently only supporting TDM_RX_0 and TDM_TX_0 */
	if (pdata->mi2s_gpio_p[tdm_mode])
		ret = msm_cdc_pinctrl_select_active_state(
				pdata->mi2s_gpio_p[tdm_mode]);

	return ret;
}

static void sm6150_tdm_snd_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	int tdm_mode = msm_get_tdm_mode(cpu_dai->id);

	if (tdm_mode < 0) {
		dev_err(rtd->card->dev, "%s: Invalid tdm_mode\n", __func__);
		return;
	}

	/* currently only supporting TDM_RX_0 and TDM_TX_0 */
	if (pdata->mi2s_gpio_p[tdm_mode])
		msm_cdc_pinctrl_select_sleep_state(
				pdata->mi2s_gpio_p[tdm_mode]);
}

static struct snd_soc_ops sm6150_tdm_be_ops = {
	.hw_params = sm6150_tdm_snd_hw_params,
	.startup = sm6150_tdm_snd_startup,
	.shutdown = sm6150_tdm_snd_shutdown
};

static int msm_fe_qos_prepare(struct snd_pcm_substream *substream)
{
	cpumask_t mask;

	if (pm_qos_request_active(&substream->latency_pm_qos_req))
		pm_qos_remove_request(&substream->latency_pm_qos_req);

	cpumask_clear(&mask);
	cpumask_set_cpu(1, &mask); /* affine to core 1 */
	cpumask_set_cpu(2, &mask); /* affine to core 2 */
	cpumask_copy(&substream->latency_pm_qos_req.cpus_affine, &mask);

	substream->latency_pm_qos_req.type = PM_QOS_REQ_AFFINE_CORES;

	pm_qos_add_request(&substream->latency_pm_qos_req,
			  PM_QOS_CPU_DMA_LATENCY,
			  MSM_LL_QOS_VALUE);
	return 0;
}

static struct snd_soc_ops msm_fe_qos_ops = {
	.prepare = msm_fe_qos_prepare,
};

static int msm_mi2s_snd_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int index = cpu_dai->id;
	int port_id = msm_get_port_id(rtd->dai_link->id);
	unsigned int fmt = SND_SOC_DAIFMT_CBS_CFS;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	dev_dbg(rtd->card->dev,
		"%s: substream = %s  stream = %d, dai name %s, dai ID %d\n",
		__func__, substream->name, substream->stream,
		cpu_dai->name, cpu_dai->id);

	if (port_id < 0) {
		dev_err(rtd->card->dev, "%s: Invalid port_id\n", __func__);
		ret = port_id;
		goto err;
	}

	if (index < PRIM_MI2S || index >= MI2S_MAX) {
		ret = -EINVAL;
		dev_err(rtd->card->dev,
			"%s: CPU DAI id (%d) out of range\n",
			__func__, cpu_dai->id);
		goto err;
	}
	/*
	 * Mutex protection in case the same MI2S
	 * interface using for both TX and RX so
	 * that the same clock won't be enable twice.
	 */
	mutex_lock(&mi2s_intf_conf[index].lock);
	if (++mi2s_intf_conf[index].ref_cnt == 1) {
		/* Check if msm needs to provide the clock to the interface */
		if (!mi2s_intf_conf[index].msm_is_mi2s_master) {
			mi2s_clk[index].clk_id = mi2s_ebit_clk[index];
			fmt = SND_SOC_DAIFMT_CBM_CFM;
		}
		ret = msm_mi2s_set_sclk(substream, true);
		if (ret < 0) {
			dev_err(rtd->card->dev,
				"%s: afe lpass clock failed to enable MI2S clock, err:%d\n",
				__func__, ret);
			goto clean_up;
		}

		ret = snd_soc_dai_set_fmt(cpu_dai, fmt);
		if (ret < 0) {
			pr_err("%s: set fmt cpu dai failed for MI2S (%d), err:%d\n",
				__func__, index, ret);
			goto clk_off;
		}
		if (mi2s_intf_conf[index].msm_is_ext_mclk) {
			pr_debug("%s: Enabling mclk, clk_freq_in_hz = %u\n",
				__func__, mi2s_mclk[index].clk_freq_in_hz);
			mi2s_mclk[index].enable = 1;
			ret = afe_set_lpass_clock_v2(port_id,
						     &mi2s_mclk[index]);
			if (ret < 0) {
				pr_err("%s: afe lpass mclk failed, err:%d\n",
					__func__, ret);
				goto clk_off;
			}
		}
		if (pdata->mi2s_gpio_p[index])
			msm_cdc_pinctrl_select_active_state(
					pdata->mi2s_gpio_p[index]);
	}
clk_off:
	if (ret < 0)
		msm_mi2s_set_sclk(substream, false);
clean_up:
	if (ret < 0)
		mi2s_intf_conf[index].ref_cnt--;
	mutex_unlock(&mi2s_intf_conf[index].lock);
err:
	return ret;
}

static void msm_mi2s_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int index = rtd->cpu_dai->id;
	int port_id = msm_get_port_id(rtd->dai_link->id);
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	if (port_id < 0) {
		dev_err(rtd->card->dev, "%s: Invalid port_id\n", __func__);
		return;
	}

	if (index < PRIM_MI2S || index >= MI2S_MAX) {
		pr_err("%s:invalid MI2S DAI(%d)\n", __func__, index);
		return;
	}

	mutex_lock(&mi2s_intf_conf[index].lock);
	if (--mi2s_intf_conf[index].ref_cnt == 0) {
		if (pdata->mi2s_gpio_p[index])
			msm_cdc_pinctrl_select_sleep_state(
					pdata->mi2s_gpio_p[index]);

		ret = msm_mi2s_set_sclk(substream, false);
		if (ret < 0)
			pr_err("%s:clock disable failed for MI2S (%d); ret=%d\n",
				__func__, index, ret);

		if (mi2s_intf_conf[index].msm_is_ext_mclk) {
			pr_debug("%s: Disabling mclk, clk_freq_in_hz = %u\n",
				 __func__, mi2s_mclk[index].clk_freq_in_hz);
			mi2s_mclk[index].enable = 0;
			ret = afe_set_lpass_clock_v2(port_id,
						     &mi2s_mclk[index]);
			if (ret < 0)
				pr_err("%s: mclk disable failed for MCLK (%d); ret=%d\n",
					__func__, index, ret);
		}
	}
	mutex_unlock(&mi2s_intf_conf[index].lock);
}

static struct snd_soc_ops msm_mi2s_be_ops = {
	.startup = msm_mi2s_snd_startup,
	.shutdown = msm_mi2s_snd_shutdown,
};

static struct snd_soc_ops msm_cdc_dma_be_ops = {
	.hw_params = msm_snd_cdc_dma_hw_params,
};

static struct snd_soc_ops msm_be_ops = {
	.hw_params = msm_snd_hw_params,
};

static struct snd_soc_ops msm_slimbus_2_be_ops = {
	.hw_params = msm_slimbus_2_hw_params,
};

static struct snd_soc_ops msm_wcn_ops = {
	.hw_params = msm_wcn_hw_params,
};

static struct snd_soc_ops msm_ext_cpe_ops = {
	.hw_params = msm_snd_cpe_hw_params,
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm_common_dai_links[] = {
	/* FrontEnd DAI Links */
	{/* hw:x,0 */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{/* hw:x,1 */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	{/* hw:x,2 */
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
		.id = MSM_FRONTEND_DAI_VOICEMMODE1,
	},
	{/* hw:x,3 */
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
		.id = MSM_FRONTEND_DAI_VOIP,
	},
	{/* hw:x,4 */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA3,
	},
	/* Hostless PCM purpose */
	{/* hw:x,5 */
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
	{/* hw:x,6 */
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
	{/* hw:x,7 */
		.name = "MSM AFE-PCM TX",
		.stream_name = "AFE-PROXY TX",
		.cpu_dai_name = "msm-dai-q6-dev.240",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.platform_name  = "msm-pcm-afe",
		.dpcm_capture = 1,
		.ignore_suspend = 1,
	},
	{/* hw:x,8 */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA4,
	},
	{/* hw:x,9 */
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
	{/* hw:x,10 */
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
	{/* hw:x,11 */
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
	{/* hw:x,12 */
		.name = "SLIMBUS_7 Hostless",
		.stream_name = "SLIMBUS_7 Hostless",
		.cpu_dai_name = "SLIMBUS7_HOSTLESS",
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
	{/* hw:x,13 */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA5,
		.ops = &msm_fe_qos_ops,
	},
	{/* hw:x,14 */
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_LSM1,
	},
	/* Multiple Tunnel instances */
	{/* hw:x,15 */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA7,
	},
	{/* hw:x,16 */
		.name = MSM_DAILINK_NAME(MultiMedia10),
		.stream_name = "MultiMedia10",
		.cpu_dai_name = "MultiMedia10",
		.platform_name = "msm-pcm-dsp.1",
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA10,
	},
	{/* hw:x,17 */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA8,
		.ops = &msm_fe_qos_ops,
	},
	/* HDMI Hostless */
	{/* hw:x,18 */
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
	{/* hw:x,19 */
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
		.id = MSM_FRONTEND_DAI_VOICEMMODE2,
	},
	/* LSM FE */
	{/* hw:x,20 */
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_LSM2,
	},
	{/* hw:x,21 */
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_LSM3,
	},
	{/* hw:x,22 */
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_LSM4,
	},
	{/* hw:x,23 */
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_LSM5,
	},
	{/* hw:x,24 */
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_LSM6,
	},
	{/* hw:x,25 */
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_LSM7,
	},
	{/* hw:x,26 */
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_LSM8,
	},
	{/* hw:x,27 */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA9,
	},
	{/* hw:x,28 */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA11,
	},
	{/* hw:x,29 */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA12,
	},
	{/* hw:x,30 */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA13,
	},
	{/* hw:x,31 */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA14,
	},
	{/* hw:x,32 */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA15,
	},
	{/* hw:x,33 */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA16,
	},
	{/* hw:x,34 */
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
	{/* hw:x,35 */
		.name = "CDC_DMA Hostless",
		.stream_name = "CDC_DMA Hostless",
		.cpu_dai_name = "CDC_DMA_HOSTLESS",
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
	{/* hw:x,36 */
		.name = "TX3_CDC_DMA Hostless",
		.stream_name = "TX3_CDC_DMA Hostless",
		.cpu_dai_name = "TX3_CDC_DMA_HOSTLESS",
		.platform_name = "msm-pcm-hostless",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
	},
};

static struct snd_soc_dai_link msm_tavil_fe_dai_links[] = {
	{/* hw:x,37 */
		.name = LPASS_BE_SLIMBUS_4_TX,
		.stream_name = "Slimbus4 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16393",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_vifeedback",
		.id = MSM_BACKEND_DAI_SLIMBUS_4_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
	},
	/* Ultrasound RX DAI Link */
	{/* hw:x,38 */
		.name = "SLIMBUS_2 Hostless Playback",
		.stream_name = "SLIMBUS_2 Hostless Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16388",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "tavil_codec",
		.codec_dai_name = "tavil_rx2",
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ops = &msm_slimbus_2_be_ops,
	},
	/* Ultrasound TX DAI Link */
	{/* hw:x,39 */
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

static struct snd_soc_dai_link msm_int_compress_capture_dai[] = {
	{
		.name = "Compress9",
		.stream_name = "Compress9",
		.cpu_dai_name = "MultiMedia17",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA17,
	},
	{
		.name = "Compress10",
		.stream_name = "Compress10",
		.cpu_dai_name = "MultiMedia18",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA18,
	},
	{
		.name = "Compress11",
		.stream_name = "Compress11",
		.cpu_dai_name = "MultiMedia19",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA19,
	},
	{
		.name = "Compress12",
		.stream_name = "Compress12",
		.cpu_dai_name = "MultiMedia28",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA28,
	},
	{
		.name = "Compress13",
		.stream_name = "Compress13",
		.cpu_dai_name = "MultiMedia29",
		.platform_name = "msm-compress-dsp",
		.dynamic = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA29,
	},
};

static struct snd_soc_dai_link msm_bolero_fe_dai_links[] = {
	{/* hw:x,37 */
		.name = LPASS_BE_WSA_CDC_DMA_TX_0,
		.stream_name = "WSA CDC DMA0 Capture",
		.cpu_dai_name = "msm-dai-cdc-dma-dev.45057",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "bolero_codec",
		.codec_dai_name = "wsa_macro_vifeedback",
		.id = MSM_BACKEND_DAI_WSA_CDC_DMA_TX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ops = &msm_cdc_dma_be_ops,
	},
};

static struct snd_soc_dai_link msm_tasha_fe_dai_links[] = {
	/* tasha_vifeedback for speaker protection */
	{
		.name = LPASS_BE_SLIMBUS_4_TX,
		.stream_name = "Slimbus4 Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16393",
		.platform_name = "msm-pcm-hostless",
		.codec_name = "tasha_codec",
		.codec_dai_name = "tasha_vifeedback",
		.id = MSM_BACKEND_DAI_SLIMBUS_4_TX,
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
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.ignore_pmdown_time = 1,
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
		.dpcm_capture = 1,
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
		.dpcm_capture = 1,
		.codec_dai_name = "tasha_mad1",
		.codec_name = "tasha_codec",
		.ops = &msm_ext_cpe_ops,
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA6,
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
		.id = MSM_BACKEND_DAI_AFE_PCM_RX,
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
		.id = MSM_BACKEND_DAI_AFE_PCM_TX,
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
		.id = MSM_BACKEND_DAI_INCALL_RECORD_TX,
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
		.id = MSM_BACKEND_DAI_INCALL_RECORD_RX,
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
		.id = MSM_BACKEND_DAI_VOICE_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
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
		.id = MSM_BACKEND_DAI_VOICE2_PLAYBACK_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
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
		.id = MSM_BACKEND_DAI_USB_RX,
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
		.id = MSM_BACKEND_DAI_USB_TX,
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
		.id = MSM_BACKEND_DAI_PRI_TDM_RX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &sm6150_tdm_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
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
		.id = MSM_BACKEND_DAI_PRI_TDM_TX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &sm6150_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_SEC_TDM_RX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &sm6150_tdm_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
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
		.id = MSM_BACKEND_DAI_SEC_TDM_TX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &sm6150_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_TERT_TDM_RX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &sm6150_tdm_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
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
		.id = MSM_BACKEND_DAI_TERT_TDM_TX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &sm6150_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_QUAT_TDM_RX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &sm6150_tdm_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
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
		.id = MSM_BACKEND_DAI_QUAT_TDM_TX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &sm6150_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUIN_TDM_RX_0,
		.stream_name = "Quinary TDM0 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36928",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_QUIN_TDM_RX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &sm6150_tdm_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_QUIN_TDM_TX_0,
		.stream_name = "Quinary TDM0 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36929",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_QUIN_TDM_TX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &sm6150_tdm_be_ops,
		.ignore_suspend = 1,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_0_RX,
		.init = &msm_audrx_tavil_init,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_0_TX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_1_RX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_1_TX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_2_RX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_3_RX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_3_TX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_4_RX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_5_RX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_5_TX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_6_RX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_4_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
		.ignore_suspend = 1,
		.no_pcm = 1,
		.dpcm_capture = 1,
	},
};

static struct snd_soc_dai_link msm_tasha_be_dai_links[] = {
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
		.id = MSM_BACKEND_DAI_SLIMBUS_0_RX,
		.init = &msm_audrx_tasha_init,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_0_TX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_1_RX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_1_TX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_3_RX,
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
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_SLIMBUS_3_TX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_4_RX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_5_RX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_5_TX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_6_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_be_ops,
		/* dai link has playback support */
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_7_RX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_7_TX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_8_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.init = &msm_wcn_init,
		.ops = &msm_wcn_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link ext_disp_be_dai_link[] = {
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
		.id = MSM_BACKEND_DAI_DISPLAY_PORT_RX,
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
		.id = MSM_BACKEND_DAI_PRI_MI2S_RX,
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
		.id = MSM_BACKEND_DAI_PRI_MI2S_TX,
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
		.id = MSM_BACKEND_DAI_SECONDARY_MI2S_RX,
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
		.id = MSM_BACKEND_DAI_SECONDARY_MI2S_TX,
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
		.id = MSM_BACKEND_DAI_TERTIARY_MI2S_RX,
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
		.id = MSM_BACKEND_DAI_TERTIARY_MI2S_TX,
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
		.id = MSM_BACKEND_DAI_QUATERNARY_MI2S_RX,
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
		.id = MSM_BACKEND_DAI_QUATERNARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUIN_MI2S_RX,
		.stream_name = "Quinary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.4",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_QUINARY_MI2S_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm_mi2s_be_ops,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
	},
	{
		.name = LPASS_BE_QUIN_MI2S_TX,
		.stream_name = "Quinary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.4",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_QUINARY_MI2S_TX,
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
		.id = MSM_BACKEND_DAI_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
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
		.id = MSM_BACKEND_DAI_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
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
		.id = MSM_BACKEND_DAI_SEC_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
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
		.id = MSM_BACKEND_DAI_SEC_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
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
		.id = MSM_BACKEND_DAI_TERT_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
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
		.id = MSM_BACKEND_DAI_TERT_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
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
		.id = MSM_BACKEND_DAI_QUAT_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
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
		.id = MSM_BACKEND_DAI_QUAT_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
	/* Quinary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_QUIN_AUXPCM_RX,
		.stream_name = "Quin AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.5",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_QUIN_AUXPCM_RX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_QUIN_AUXPCM_TX,
		.stream_name = "Quin AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.5",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_QUIN_AUXPCM_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm_wsa_cdc_dma_be_dai_links[] = {
	/* WSA CDC DMA Backend DAI Links */
	{
		.name = LPASS_BE_WSA_CDC_DMA_RX_0,
		.stream_name = "WSA CDC DMA0 Playback",
		.cpu_dai_name = "msm-dai-cdc-dma-dev.45056",
		.platform_name = "msm-pcm-routing",
		.codec_name = "bolero_codec",
		.codec_dai_name = "wsa_macro_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.init = &msm_int_audrx_init,
		.id = MSM_BACKEND_DAI_WSA_CDC_DMA_RX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
	},
	{
		.name = LPASS_BE_WSA_CDC_DMA_RX_1,
		.stream_name = "WSA CDC DMA1 Playback",
		.cpu_dai_name = "msm-dai-cdc-dma-dev.45058",
		.platform_name = "msm-pcm-routing",
		.codec_name = "bolero_codec",
		.codec_dai_name = "wsa_macro_rx_mix",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_WSA_CDC_DMA_RX_1,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
	},
	{
		.name = LPASS_BE_WSA_CDC_DMA_TX_1,
		.stream_name = "WSA CDC DMA1 Capture",
		.cpu_dai_name = "msm-dai-cdc-dma-dev.45059",
		.platform_name = "msm-pcm-routing",
		.codec_name = "bolero_codec",
		.codec_dai_name = "wsa_macro_echo",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_WSA_CDC_DMA_TX_1,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
	},
};

static struct snd_soc_dai_link msm_rx_tx_cdc_dma_be_dai_links[] = {
	/* RX CDC DMA Backend DAI Links */
	{
		.name = LPASS_BE_RX_CDC_DMA_RX_0,
		.stream_name = "RX CDC DMA0 Playback",
		.cpu_dai_name = "msm-dai-cdc-dma-dev.45104",
		.platform_name = "msm-pcm-routing",
		.codec_name = "bolero_codec",
		.codec_dai_name = "rx_macro_rx1",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_RX_CDC_DMA_RX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
	},
	{
		.name = LPASS_BE_RX_CDC_DMA_RX_1,
		.stream_name = "RX CDC DMA1 Playback",
		.cpu_dai_name = "msm-dai-cdc-dma-dev.45106",
		.platform_name = "msm-pcm-routing",
		.codec_name = "bolero_codec",
		.codec_dai_name = "rx_macro_rx2",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_RX_CDC_DMA_RX_1,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
	},
	{
		.name = LPASS_BE_RX_CDC_DMA_RX_2,
		.stream_name = "RX CDC DMA2 Playback",
		.cpu_dai_name = "msm-dai-cdc-dma-dev.45108",
		.platform_name = "msm-pcm-routing",
		.codec_name = "bolero_codec",
		.codec_dai_name = "rx_macro_rx3",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_RX_CDC_DMA_RX_2,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
	},
	{
		.name = LPASS_BE_RX_CDC_DMA_RX_3,
		.stream_name = "RX CDC DMA3 Playback",
		.cpu_dai_name = "msm-dai-cdc-dma-dev.45110",
		.platform_name = "msm-pcm-routing",
		.codec_name = "bolero_codec",
		.codec_dai_name = "rx_macro_rx4",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_RX_CDC_DMA_RX_3,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
	},
	/* TX CDC DMA Backend DAI Links */
	{
		.name = LPASS_BE_TX_CDC_DMA_TX_0,
		.stream_name = "TX CDC DMA0 Capture",
		.cpu_dai_name = "msm-dai-cdc-dma-dev.45105",
		.platform_name = "msm-pcm-routing",
		.codec_name = "bolero_codec",
		.codec_dai_name = "rx_macro_echo",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_TX_CDC_DMA_TX_0,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
	},
	{
		.name = LPASS_BE_TX_CDC_DMA_TX_3,
		.stream_name = "TX CDC DMA3 Capture",
		.cpu_dai_name = "msm-dai-cdc-dma-dev.45111",
		.platform_name = "msm-pcm-routing",
		.codec_name = "bolero_codec",
		.codec_dai_name = "tx_macro_tx1",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_TX_CDC_DMA_TX_3,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
	},
	{
		.name = LPASS_BE_TX_CDC_DMA_TX_4,
		.stream_name = "TX CDC DMA4 Capture",
		.cpu_dai_name = "msm-dai-cdc-dma-dev.45113",
		.platform_name = "msm-pcm-routing",
		.codec_name = "bolero_codec",
		.codec_dai_name = "tx_macro_tx2",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_TX_CDC_DMA_TX_4,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm_cdc_dma_be_ops,
	},
};

static struct snd_soc_dai_link msm_sm6150_dai_links[
			 ARRAY_SIZE(msm_common_dai_links) +
			 ARRAY_SIZE(msm_tavil_fe_dai_links) +
			 ARRAY_SIZE(msm_bolero_fe_dai_links) +
			 ARRAY_SIZE(msm_tasha_fe_dai_links) +
			 ARRAY_SIZE(msm_common_misc_fe_dai_links) +
			 ARRAY_SIZE(msm_int_compress_capture_dai) +
			 ARRAY_SIZE(msm_common_be_dai_links) +
			 ARRAY_SIZE(msm_tavil_be_dai_links) +
			 ARRAY_SIZE(msm_tasha_be_dai_links) +
			 ARRAY_SIZE(msm_wcn_be_dai_links) +
			 ARRAY_SIZE(ext_disp_be_dai_link) +
			 ARRAY_SIZE(msm_mi2s_be_dai_links) +
			 ARRAY_SIZE(msm_auxpcm_be_dai_links) +
			 ARRAY_SIZE(msm_wsa_cdc_dma_be_dai_links) +
			 ARRAY_SIZE(msm_rx_tx_cdc_dma_be_dai_links)];

static int msm_snd_card_tavil_late_probe(struct snd_soc_card *card)
{
	const char *be_dl_name = LPASS_BE_SLIMBUS_0_RX;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_component *component;
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

	component = snd_soc_rtdcom_lookup(rtd, "tavil_codec");
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		ret = -EINVAL;
		goto err_pcm_runtime;
	}
	mbhc_calibration = def_wcd_mbhc_cal();
	if (!mbhc_calibration) {
		ret = -ENOMEM;
		goto err_mbhc_cal;
	}
	wcd_mbhc_cfg.calibration = mbhc_calibration;
	ret = tavil_mbhc_hs_detect(component, &wcd_mbhc_cfg);
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

static int msm_snd_card_tasha_late_probe(struct snd_soc_card *card)
{
	const char *be_dl_name = LPASS_BE_SLIMBUS_0_RX;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_component *component;
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

	component = snd_soc_rtdcom_lookup(rtd, "tasha_codec");
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		ret = -EINVAL;
		goto err_pcm_runtime;
	}

	mbhc_calibration = def_wcd_mbhc_cal();
	if (!mbhc_calibration) {
		ret = -ENOMEM;
		goto err_mbhc_cal;
	}
	wcd_mbhc_cfg.calibration = mbhc_calibration;
	ret = tasha_mbhc_hs_detect(component, &wcd_mbhc_cfg);
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

static int msm_audrx_stub_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_soc_component *component =
			snd_soc_rtdcom_lookup(rtd, "msm-stub-codec");

	ret = snd_soc_add_component_controls(component, msm_ext_snd_controls,
					 ARRAY_SIZE(msm_ext_snd_controls));
	if (ret < 0) {
		dev_err(component->dev,
			"%s: add_codec_controls failed, err = %d\n",
			__func__, ret);
		return ret;
	}

	return 0;
}

static int msm_snd_stub_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;

	int ret = 0;
	unsigned int rx_ch[] = {144, 145, 146, 147, 148, 149, 150,
				151};
	unsigned int tx_ch[] = {128, 129, 130, 131, 132, 133,
				134, 135, 136, 137, 138, 139,
				140, 141, 142, 143};

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
					slim_rx_cfg[SLIM_RX_0].channels,
					rx_ch);
		if (ret < 0)
			pr_err("%s: RX failed to set cpu chan map error %d\n",
				__func__, ret);
	} else {
		ret = snd_soc_dai_set_channel_map(cpu_dai,
					slim_tx_cfg[SLIM_TX_0].channels,
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA1
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
		.id = MSM_BACKEND_DAI_SLIMBUS_0_RX,
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
		.id = MSM_BACKEND_DAI_SLIMBUS_0_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msm_stub_be_ops,
	},
};

static struct snd_soc_dai_link msm_stub_dai_links[
			 ARRAY_SIZE(msm_stub_fe_dai_links) +
			 ARRAY_SIZE(msm_stub_be_dai_links)];

struct snd_soc_card snd_soc_card_stub_msm = {
	.name		= "sm6150-stub-snd-card",
};

static const struct of_device_id sm6150_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,sm6150-asoc-snd",
	  .data = "codec"},
	{ .compatible = "qcom,sm6150-asoc-snd-stub",
	  .data = "stub_codec"},
	{},
};

static struct snd_soc_card *populate_snd_card_dailinks(struct device *dev)
{
	struct snd_soc_card *card = NULL;
	struct snd_soc_dai_link *dailink;
	int total_links = 0, rc = 0;
	u32 tavil_codec = 0, auxpcm_audio_intf = 0;
	u32 mi2s_audio_intf = 0, ext_disp_audio_intf = 0;
	u32 wcn_btfm_intf = 0;
	const struct of_device_id *match;
	u32 tasha_codec = 0;

	match = of_match_node(sm6150_asoc_machine_of_match, dev->of_node);
	if (!match) {
		dev_err(dev, "%s: No DT match found for sound card\n",
			__func__);
		return NULL;
	}

	if (!strcmp(match->data, "codec")) {
		card = &snd_soc_card_sm6150_msm;
		memcpy(msm_sm6150_dai_links + total_links,
		       msm_common_dai_links,
		       sizeof(msm_common_dai_links));

		total_links += ARRAY_SIZE(msm_common_dai_links);

		memcpy(msm_sm6150_dai_links + total_links,
		       msm_common_misc_fe_dai_links,
		       sizeof(msm_common_misc_fe_dai_links));

		total_links += ARRAY_SIZE(msm_common_misc_fe_dai_links);

		rc = of_property_read_u32(dev->of_node, "qcom,tavil_codec",
						&tavil_codec);
		if (rc)
			dev_dbg(dev, "%s: No DT match for tavil codec\n",
				__func__);

		rc = of_property_read_u32(dev->of_node, "qcom,tasha_codec",
						&tasha_codec);
		if (rc)
			dev_dbg(dev, "%s: No DT match for tasha codec\n",
				__func__);

		if (tavil_codec) {
			card->late_probe =
				msm_snd_card_tavil_late_probe;
			memcpy(msm_sm6150_dai_links + total_links,
				msm_tavil_fe_dai_links,
				sizeof(msm_tavil_fe_dai_links));
			total_links +=
				ARRAY_SIZE(msm_tavil_fe_dai_links);
		} else if (tasha_codec) {
			card->late_probe =
				msm_snd_card_tasha_late_probe;
			memcpy(msm_sm6150_dai_links + total_links,
				msm_tasha_fe_dai_links,
				sizeof(msm_tasha_fe_dai_links));
			total_links +=
				ARRAY_SIZE(msm_tasha_fe_dai_links);
		} else {
			memcpy(msm_sm6150_dai_links + total_links,
				msm_bolero_fe_dai_links,
				sizeof(msm_bolero_fe_dai_links));
			total_links +=
				ARRAY_SIZE(msm_bolero_fe_dai_links);
		}

		memcpy(msm_sm6150_dai_links + total_links,
		       msm_int_compress_capture_dai,
		       sizeof(msm_int_compress_capture_dai));

		total_links += ARRAY_SIZE(msm_int_compress_capture_dai);

		memcpy(msm_sm6150_dai_links + total_links,
		       msm_common_be_dai_links,
		       sizeof(msm_common_be_dai_links));

		total_links += ARRAY_SIZE(msm_common_be_dai_links);

		if (tavil_codec) {
			memcpy(msm_sm6150_dai_links + total_links,
					msm_tavil_be_dai_links,
					sizeof(msm_tavil_be_dai_links));
			total_links += ARRAY_SIZE(msm_tavil_be_dai_links);
		} else if (tasha_codec) {
			memcpy(msm_sm6150_dai_links + total_links,
					msm_tasha_be_dai_links,
					sizeof(msm_tasha_be_dai_links));
			total_links += ARRAY_SIZE(msm_tasha_be_dai_links);
		} else {
			memcpy(msm_sm6150_dai_links + total_links,
			       msm_wsa_cdc_dma_be_dai_links,
			       sizeof(msm_wsa_cdc_dma_be_dai_links));
			total_links +=
				ARRAY_SIZE(msm_wsa_cdc_dma_be_dai_links);

			memcpy(msm_sm6150_dai_links + total_links,
			       msm_rx_tx_cdc_dma_be_dai_links,
			       sizeof(msm_rx_tx_cdc_dma_be_dai_links));
			total_links +=
				ARRAY_SIZE(msm_rx_tx_cdc_dma_be_dai_links);
		}

		rc = of_property_read_u32(dev->of_node,
					  "qcom,ext-disp-audio-rx",
					  &ext_disp_audio_intf);
		if (rc) {
			dev_dbg(dev, "%s: No DT match Ext Disp interface\n",
				__func__);
		} else {
			if (ext_disp_audio_intf) {
				memcpy(msm_sm6150_dai_links + total_links,
					ext_disp_be_dai_link,
					sizeof(ext_disp_be_dai_link));
				total_links +=
					ARRAY_SIZE(ext_disp_be_dai_link);
			}
		}

		rc = of_property_read_u32(dev->of_node, "qcom,mi2s-audio-intf",
					  &mi2s_audio_intf);
		if (rc) {
			dev_dbg(dev, "%s: No DT match MI2S audio interface\n",
				__func__);
		} else {
			if (mi2s_audio_intf) {
				memcpy(msm_sm6150_dai_links + total_links,
					msm_mi2s_be_dai_links,
					sizeof(msm_mi2s_be_dai_links));
				total_links +=
					ARRAY_SIZE(msm_mi2s_be_dai_links);
			}
		}


		rc = of_property_read_u32(dev->of_node, "qcom,wcn-btfm",
					  &wcn_btfm_intf);
		if (rc) {
			dev_dbg(dev, "%s: No DT match wcn btfm interface\n",
				__func__);
		} else {
			if (wcn_btfm_intf) {
				memcpy(msm_sm6150_dai_links + total_links,
					msm_wcn_be_dai_links,
					sizeof(msm_wcn_be_dai_links));
				total_links +=
					ARRAY_SIZE(msm_wcn_be_dai_links);
			}
		}

		rc = of_property_read_u32(dev->of_node,
					  "qcom,auxpcm-audio-intf",
					  &auxpcm_audio_intf);
		if (rc) {
			dev_dbg(dev, "%s: No DT match Aux PCM interface\n",
				__func__);
		} else {
			if (auxpcm_audio_intf) {
				memcpy(msm_sm6150_dai_links + total_links,
					msm_auxpcm_be_dai_links,
					sizeof(msm_auxpcm_be_dai_links));
				total_links +=
					ARRAY_SIZE(msm_auxpcm_be_dai_links);
			}
		}

		dailink = msm_sm6150_dai_links;
	} else if (!strcmp(match->data, "stub_codec")) {
		card = &snd_soc_card_stub_msm;

		memcpy(msm_stub_dai_links + total_links,
		       msm_stub_fe_dai_links,
		       sizeof(msm_stub_fe_dai_links));
		total_links += ARRAY_SIZE(msm_stub_fe_dai_links);

		memcpy(msm_stub_dai_links + total_links,
		       msm_stub_be_dai_links,
		       sizeof(msm_stub_be_dai_links));
		total_links += ARRAY_SIZE(msm_stub_be_dai_links);

		dailink = msm_stub_dai_links;
	}

	if (card) {
		card->dai_link = dailink;
		card->num_links = total_links;
	}

	return card;
}

static int msm_wsa881x_init(struct snd_soc_component *component)
{
	u8 spkleft_ports[WSA881X_MAX_SWR_PORTS] = {0, 1, 2, 3};
	u8 spkright_ports[WSA881X_MAX_SWR_PORTS] = {0, 1, 2, 3};
	u8 spkleft_port_types[WSA881X_MAX_SWR_PORTS] = {SPKR_L, SPKR_L_COMP,
						SPKR_L_BOOST, SPKR_L_VI};
	u8 spkright_port_types[WSA881X_MAX_SWR_PORTS] = {SPKR_R, SPKR_R_COMP,
						SPKR_R_BOOST, SPKR_R_VI};
	unsigned int ch_rate[WSA881X_MAX_SWR_PORTS] = {2400, 600, 300, 1200};
	unsigned int ch_mask[WSA881X_MAX_SWR_PORTS] = {0x1, 0xF, 0x3, 0x3};
	struct msm_asoc_mach_data *pdata;
	struct snd_soc_dapm_context *dapm;
	struct snd_card *card = component->card->snd_card;
	struct snd_info_entry *entry;
	int ret = 0;

	if (!component) {
		pr_err("%s codec is NULL\n", __func__);
		return -EINVAL;
	}

	dapm = snd_soc_component_get_dapm(component);

	if (!strcmp(component->name_prefix, "SpkrLeft")) {
		dev_dbg(component->dev, "%s: setting left ch map to codec %s\n",
			__func__, component->name);
		wsa881x_set_channel_map(component, &spkleft_ports[0],
				WSA881X_MAX_SWR_PORTS, &ch_mask[0],
				&ch_rate[0], &spkleft_port_types[0]);
		if (dapm->component) {
			snd_soc_dapm_ignore_suspend(dapm, "SpkrLeft IN");
			snd_soc_dapm_ignore_suspend(dapm, "SpkrLeft SPKR");
		}
	} else if (!strcmp(component->name_prefix, "SpkrRight")) {
		dev_dbg(component->dev, "%s: setting right ch map to codec %s\n",
			__func__, component->name);
		wsa881x_set_channel_map(component, &spkright_ports[0],
				WSA881X_MAX_SWR_PORTS, &ch_mask[0],
				&ch_rate[0], &spkright_port_types[0]);
		if (dapm->component) {
			snd_soc_dapm_ignore_suspend(dapm, "SpkrRight IN");
			snd_soc_dapm_ignore_suspend(dapm, "SpkrRight SPKR");
		}
	} else {
		dev_err(component->dev, "%s: wrong codec name %s\n", __func__,
			component->name);
		ret = -EINVAL;
		goto err;
	}
	pdata = snd_soc_card_get_drvdata(component->card);
	if (!pdata->codec_root) {
		entry = snd_info_create_subdir(card->module, "codecs",
						 card->proc_root);
		if (!entry) {
			pr_err("%s: Cannot create codecs module entry\n",
				 __func__);
			ret = 0;
			goto err;
		}
		pdata->codec_root = entry;
	}
	wsa881x_codec_info_create_codec_entry(pdata->codec_root,
					      component);
err:
	return ret;
}

static int msm_aux_codec_init(struct snd_soc_component *component)
{
	struct snd_soc_dapm_context *dapm = snd_soc_component_get_dapm(component);
	int ret = 0;
	void *mbhc_calibration;
	struct snd_info_entry *entry;
	struct snd_card *card = component->card->snd_card;
	struct msm_asoc_mach_data *pdata;

	snd_soc_dapm_ignore_suspend(dapm, "EAR");
	snd_soc_dapm_ignore_suspend(dapm, "AUX");
	snd_soc_dapm_ignore_suspend(dapm, "HPHL");
	snd_soc_dapm_ignore_suspend(dapm, "HPHR");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC1");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC2");
	snd_soc_dapm_ignore_suspend(dapm, "AMIC3");
	snd_soc_dapm_sync(dapm);

	pdata = snd_soc_card_get_drvdata(component->card);
	if (!pdata->codec_root) {
		entry = snd_info_create_subdir(card->module, "codecs",
						 card->proc_root);
		if (!entry) {
			pr_err("%s: Cannot create codecs module entry\n",
				 __func__);
			ret = 0;
			goto codec_root_err;
		}
		pdata->codec_root = entry;
	}
	wcd937x_info_create_codec_entry(pdata->codec_root, component);
codec_root_err:
	mbhc_calibration = def_wcd_mbhc_cal();
	if (!mbhc_calibration) {
		return -ENOMEM;
	}
	wcd_mbhc_cfg.calibration = mbhc_calibration;
	ret = wcd937x_mbhc_hs_detect(component, &wcd_mbhc_cfg);

	return ret;
}

static int msm_init_aux_dev(struct platform_device *pdev,
				struct snd_soc_card *card)
{
	struct device_node *wsa_of_node;
	struct device_node *aux_codec_of_node;
	u32 wsa_max_devs;
	u32 wsa_dev_cnt;
	u32 codec_max_aux_devs = 0;
	u32 codec_aux_dev_cnt = 0;
	int i;
	struct msm_wsa881x_dev_info *wsa881x_dev_info = NULL;
	struct aux_codec_dev_info *aux_cdc_dev_info = NULL;
	const char *auxdev_name_prefix[1];
	char *dev_name_str = NULL;
	int found = 0;
	int codecs_found = 0;
	int ret = 0;

	/* Get maximum WSA device count for this platform */
	ret = of_property_read_u32(pdev->dev.of_node,
				   "qcom,wsa-max-devs", &wsa_max_devs);
	if (ret) {
		dev_err(&pdev->dev,
			 "%s: wsa-max-devs property missing in DT %s, ret = %d\n",
			 __func__, pdev->dev.of_node->full_name, ret);
		wsa_max_devs = 0;
		goto codec_aux_dev;
	}
	if (wsa_max_devs == 0) {
		dev_dbg(&pdev->dev,
			 "%s: Max WSA devices is 0 for this target?\n",
			 __func__);
		goto codec_aux_dev;
	}

	/* Get count of WSA device phandles for this platform */
	wsa_dev_cnt = of_count_phandle_with_args(pdev->dev.of_node,
						 "qcom,wsa-devs", NULL);
	if (wsa_dev_cnt == -ENOENT) {
		dev_warn(&pdev->dev, "%s: No wsa device defined in DT.\n",
			 __func__);
		goto err;
	} else if (wsa_dev_cnt <= 0) {
		dev_err(&pdev->dev,
			"%s: Error reading wsa device from DT. wsa_dev_cnt = %d\n",
			__func__, wsa_dev_cnt);
		ret = -EINVAL;
		goto err;
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
		goto err;
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
		goto err;
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
			goto err;
		}
		if (soc_find_component_locked(wsa_of_node, NULL)) {
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

codec_aux_dev:
	if (!strnstr(card->name, "tavil", strlen(card->name)) &&
	    !strnstr(card->name, "tasha", strlen(card->name))) {
		/* Get maximum aux codec device count for this platform */
		ret = of_property_read_u32(pdev->dev.of_node,
					   "qcom,codec-max-aux-devs",
					   &codec_max_aux_devs);
		if (ret) {
			dev_err(&pdev->dev,
				 "%s: codec-max-aux-devs property missing in DT %s, ret = %d\n",
				 __func__, pdev->dev.of_node->full_name, ret);
			codec_max_aux_devs = 0;
			goto aux_dev_register;
		}
		if (codec_max_aux_devs == 0) {
			dev_dbg(&pdev->dev,
				 "%s: Max aux codec devices is 0 for this target?\n",
				 __func__);
			goto aux_dev_register;
		}

		/* Get count of aux codec device phandles for this platform */
		codec_aux_dev_cnt = of_count_phandle_with_args(
					pdev->dev.of_node,
					"qcom,codec-aux-devs", NULL);
		if (codec_aux_dev_cnt == -ENOENT) {
			dev_warn(&pdev->dev, "%s: No aux codec defined in DT.\n",
				 __func__);
			goto err;
		} else if (codec_aux_dev_cnt <= 0) {
			dev_err(&pdev->dev,
				"%s: Error reading aux codec device from DT, dev_cnt=%d\n",
				__func__, codec_aux_dev_cnt);
			ret = -EINVAL;
			goto err;
		}

		/*
		 * Expect total phandles count to be NOT less than maximum possible
		 * AUX device count. However, if it is less, then assign same value to
		 * max count as well.
		 */
		if (codec_aux_dev_cnt < codec_max_aux_devs) {
			dev_dbg(&pdev->dev,
				"%s: codec_max_aux_devs = %d cannot exceed codec_aux_dev_cnt = %d\n",
				__func__, codec_max_aux_devs,
				codec_aux_dev_cnt);
			codec_max_aux_devs = codec_aux_dev_cnt;
		}

		/*
		 * Alloc mem to store phandle and index info of aux codec
		 * if already registered with ALSA core
		 */
		aux_cdc_dev_info = devm_kcalloc(&pdev->dev, codec_max_aux_devs,
					sizeof(struct aux_codec_dev_info),
					GFP_KERNEL);
		if (!aux_cdc_dev_info) {
			ret = -ENOMEM;
			goto err;
		}

		/*
		 * search and check whether all aux codecs are already
		 * registered with ALSA core or not. If found a node, store
		 * the node and the index in a local array of struct for later
		 * use.
		 */
		for (i = 0; i < codec_aux_dev_cnt; i++) {
			aux_codec_of_node = of_parse_phandle(pdev->dev.of_node,
						    "qcom,codec-aux-devs", i);
			if (unlikely(!aux_codec_of_node)) {
				/* we should not be here */
				dev_err(&pdev->dev,
					"%s: aux codec dev node is not present\n",
					__func__);
				ret = -EINVAL;
				goto err;
			}
			if (soc_find_component_locked(aux_codec_of_node, NULL)) {
				/* AUX codec registered with ALSA core */
				aux_cdc_dev_info[codecs_found].of_node =
							aux_codec_of_node;
				aux_cdc_dev_info[codecs_found].index = i;
				codecs_found++;
			}
		}

		if (codecs_found < codec_max_aux_devs) {
			dev_dbg(&pdev->dev,
				"%s: failed to find %d components. Found only %d\n",
				__func__, codec_max_aux_devs, codecs_found);
			return -EPROBE_DEFER;
		}
		dev_info(&pdev->dev,
			"%s: found %d AUX codecs registered with ALSA core\n",
			__func__, codecs_found);

	}

aux_dev_register:
	card->num_aux_devs = wsa_max_devs + codec_max_aux_devs;
	card->num_configs = wsa_max_devs + codec_max_aux_devs;

	/* Alloc array of AUX devs struct */
	msm_aux_dev = devm_kcalloc(&pdev->dev, card->num_aux_devs,
				       sizeof(struct snd_soc_aux_dev),
				       GFP_KERNEL);
	if (!msm_aux_dev) {
		ret = -ENOMEM;
		goto err;
	}

	/* Alloc array of codec conf struct */
	msm_codec_conf = devm_kcalloc(&pdev->dev, card->num_configs,
					  sizeof(struct snd_soc_codec_conf),
					  GFP_KERNEL);
	if (!msm_codec_conf) {
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < wsa_max_devs; i++) {
		dev_name_str = devm_kzalloc(&pdev->dev, DEV_NAME_STR_LEN,
					    GFP_KERNEL);
		if (!dev_name_str) {
			ret = -ENOMEM;
			goto err;
		}

		ret = of_property_read_string_index(pdev->dev.of_node,
						    "qcom,wsa-aux-dev-prefix",
						    wsa881x_dev_info[i].index,
						    auxdev_name_prefix);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: failed to read wsa aux dev prefix, ret = %d\n",
				__func__, ret);
			ret = -EINVAL;
			goto err;
		}

		snprintf(dev_name_str, strlen("wsa881x.%d"), "wsa881x.%d", i);
		msm_aux_dev[i].name = dev_name_str;
		msm_aux_dev[i].codec_name = NULL;
		msm_aux_dev[i].codec_of_node =
					wsa881x_dev_info[i].of_node;
		msm_aux_dev[i].init = msm_wsa881x_init;
		msm_codec_conf[i].dev_name = NULL;
		msm_codec_conf[i].name_prefix = auxdev_name_prefix[0];
		msm_codec_conf[i].of_node =
				wsa881x_dev_info[i].of_node;
	}

	for (i = 0; i < codec_aux_dev_cnt; i++) {
		msm_aux_dev[wsa_max_devs + i].name = "aux_codec";
		msm_aux_dev[wsa_max_devs + i].codec_name = NULL;
		msm_aux_dev[wsa_max_devs + i].codec_of_node =
					aux_cdc_dev_info[i].of_node;
		msm_aux_dev[wsa_max_devs + i].init =  msm_aux_codec_init;
		msm_codec_conf[wsa_max_devs + i].dev_name = NULL;
		msm_codec_conf[wsa_max_devs + i].name_prefix =
						NULL;
		msm_codec_conf[wsa_max_devs + i].of_node =
				aux_cdc_dev_info[i].of_node;
	}

	card->codec_conf = msm_codec_conf;
	card->aux_dev = msm_aux_dev;
err:
	return ret;
}

static void msm_i2s_auxpcm_init(struct platform_device *pdev)
{
	int count;
	u32 mi2s_master_slave[MI2S_MAX];
	u32 mi2s_ext_mclk[MI2S_MAX];
	int ret;

	for (count = 0; count < MI2S_MAX; count++) {
		mutex_init(&mi2s_intf_conf[count].lock);
		mi2s_intf_conf[count].ref_cnt = 0;
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

	ret = of_property_read_u32_array(pdev->dev.of_node,
					 "qcom,msm-mi2s-ext-mclk",
					 mi2s_ext_mclk, MI2S_MAX);
	if (ret) {
		dev_dbg(&pdev->dev, "%s: no qcom,msm-mi2s-ext-mclk in DT node\n",
			__func__);
	} else {
		for (count = 0; count < MI2S_MAX; count++)
			mi2s_intf_conf[count].msm_is_ext_mclk =
				mi2s_ext_mclk[count];
	}
}

static void msm_i2s_auxpcm_deinit(void)
{
	int count;

	for (count = 0; count < MI2S_MAX; count++) {
		mutex_destroy(&mi2s_intf_conf[count].lock);
		mi2s_intf_conf[count].ref_cnt = 0;
		mi2s_intf_conf[count].msm_is_mi2s_master = 0;
		mi2s_intf_conf[count].msm_is_ext_mclk = 0;
	}
}

static int sm6150_ssr_enable(struct device *dev, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm_asoc_mach_data *pdata = NULL;
	struct snd_soc_component *component = NULL;
	int ret = 0;

	if (!card) {
		dev_err(dev, "%s: card is NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	if (strnstr(card->name, "tavil", strlen(card->name)) ||
	    strnstr(card->name, "tasha", strlen(card->name))) {
		pdata = snd_soc_card_get_drvdata(card);
		if (!pdata->is_afe_config_done) {
			const char *be_dl_name = LPASS_BE_SLIMBUS_0_RX;
			struct snd_soc_pcm_runtime *rtd;

			rtd = snd_soc_get_pcm_runtime(card, be_dl_name);
			if (!rtd) {
				dev_err(dev,
					"%s: snd_soc_get_pcm_runtime for %s failed!\n",
					__func__, be_dl_name);
				ret = -EINVAL;
				goto err;
			}
			component = snd_soc_rtdcom_lookup(rtd, "tavil_codec");
			if (!component) {
				dev_err(dev, "%s: component is NULL\n",
					__func__);
				ret = -EINVAL;
				goto err;
			}
			ret = msm_afe_set_config(component);
			if (ret)
				dev_err(dev, "%s: Failed to set AFE config. err %d\n",
					__func__, ret);
			else
				pdata->is_afe_config_done = true;
		}
	}
	snd_soc_card_change_online_state(card, 1);
	dev_dbg(dev, "%s: setting snd_card to ONLINE\n", __func__);

err:
	return ret;
}

static void sm6150_ssr_disable(struct device *dev, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct msm_asoc_mach_data *pdata;

	if (!card) {
		dev_err(dev, "%s: card is NULL\n", __func__);
		return;
	}

	dev_dbg(dev, "%s: setting snd_card to OFFLINE\n", __func__);
	snd_soc_card_change_online_state(card, 0);

	if (strnstr(card->name, "tavil", strlen(card->name)) ||
	    strnstr(card->name, "tasha", strlen(card->name))) {
		pdata = snd_soc_card_get_drvdata(card);
		msm_afe_clear_config();
		pdata->is_afe_config_done = false;
	}
}

static int msm_ext_prepare_hifi(struct msm_asoc_mach_data *pdata)
{
	int ret = 0;

	if (gpio_is_valid(pdata->hph_en1_gpio)) {
		pr_debug("%s: hph_en1_gpio request %d\n", __func__,
			pdata->hph_en1_gpio);
		ret = gpio_request(pdata->hph_en1_gpio, "hph_en1_gpio");
		if (ret) {
			pr_err("%s: hph_en1_gpio request failed, ret:%d\n",
				__func__, ret);
			goto err;
		}
	}
	if (gpio_is_valid(pdata->hph_en0_gpio)) {
		pr_debug("%s: hph_en0_gpio request %d\n", __func__,
			pdata->hph_en0_gpio);
		ret = gpio_request(pdata->hph_en0_gpio, "hph_en0_gpio");
		if (ret)
			pr_err("%s: hph_en0_gpio request failed, ret:%d\n",
				__func__, ret);
	}

err:
	return ret;
}

static const struct snd_event_ops sm6150_ssr_ops = {
	.enable = sm6150_ssr_enable,
	.disable = sm6150_ssr_disable,
};

static int msm_audio_ssr_compare(struct device *dev, void *data)
{
	struct device_node *node = data;

	dev_dbg(dev, "%s: dev->of_node = 0x%p, node = 0x%p\n",
		__func__, dev->of_node, node);
	return (dev->of_node && dev->of_node == node);
}

static int msm_audio_ssr_register(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct snd_event_clients *ssr_clients = NULL;
	struct device_node *node;
	int ret;
	int i;

	for (i = 0; ; i++) {
		node = of_parse_phandle(np, "qcom,msm_audio_ssr_devs", i);
		if (!node)
			break;
		snd_event_mstr_add_client(&ssr_clients,
					msm_audio_ssr_compare, node);
	}

	ret = snd_event_master_register(dev, &sm6150_ssr_ops,
					ssr_clients, NULL);
	if (!ret)
		snd_event_notify(dev, SND_EVENT_UP);

	return ret;
}

static int msm_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct msm_asoc_mach_data *pdata;
	const char *mbhc_audio_jack_type = NULL;
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

	ret = msm_populate_dai_link_component_of_node(card);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}

	ret = msm_init_aux_dev(pdev, card);
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

	pdata->hph_en1_gpio = of_get_named_gpio(pdev->dev.of_node,
						"qcom,hph-en1-gpio", 0);
	if (!gpio_is_valid(pdata->hph_en1_gpio))
		pdata->hph_en1_gpio_p = of_parse_phandle(pdev->dev.of_node,
					"qcom,hph-en1-gpio", 0);
	if (!gpio_is_valid(pdata->hph_en1_gpio) && (!pdata->hph_en1_gpio_p)) {
		dev_dbg(&pdev->dev, "property %s not detected in node %s",
			"qcom,hph-en1-gpio", pdev->dev.of_node->full_name);
	}

	pdata->hph_en0_gpio = of_get_named_gpio(pdev->dev.of_node,
						"qcom,hph-en0-gpio", 0);
	if (!gpio_is_valid(pdata->hph_en0_gpio))
		pdata->hph_en0_gpio_p = of_parse_phandle(pdev->dev.of_node,
					"qcom,hph-en0-gpio", 0);
	if (!gpio_is_valid(pdata->hph_en0_gpio) && (!pdata->hph_en0_gpio_p)) {
		dev_dbg(&pdev->dev, "property %s not detected in node %s",
			"qcom,hph-en0-gpio", pdev->dev.of_node->full_name);
	}

	ret = msm_ext_prepare_hifi(pdata);
	if (ret) {
		dev_dbg(&pdev->dev, "msm_ext_prepare_hifi failed (%d)\n",
			ret);
		ret = 0;
	}

	ret = of_property_read_string(pdev->dev.of_node,
		"qcom,mbhc-audio-jack-type", &mbhc_audio_jack_type);
	if (ret) {
		dev_dbg(&pdev->dev, "Looking up %s property in node %s failed\n",
			"qcom,mbhc-audio-jack-type",
			pdev->dev.of_node->full_name);
		dev_dbg(&pdev->dev, "Jack type properties set to default\n");
		ret = 0;
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
			dev_dbg(&pdev->dev, "Unknown value, set to default\n");
		}
	}

	pdata->mi2s_gpio_p[PRIM_MI2S] = of_parse_phandle(pdev->dev.of_node,
					"qcom,pri-mi2s-gpios", 0);
	pdata->mi2s_gpio_p[SEC_MI2S] = of_parse_phandle(pdev->dev.of_node,
					"qcom,sec-mi2s-gpios", 0);
	pdata->mi2s_gpio_p[TERT_MI2S] = of_parse_phandle(pdev->dev.of_node,
					"qcom,tert-mi2s-gpios", 0);
	pdata->mi2s_gpio_p[QUAT_MI2S] = of_parse_phandle(pdev->dev.of_node,
					"qcom,quat-mi2s-gpios", 0);
	pdata->mi2s_gpio_p[QUIN_MI2S] = of_parse_phandle(pdev->dev.of_node,
					"qcom,quin-mi2s-gpios", 0);

	/*
	 * Parse US-Euro gpio info from DT. Report no error if us-euro
	 * entry is not found in DT file as some targets do not support
	 * US-Euro detection
	 */
	pdata->us_euro_gpio_p = of_parse_phandle(pdev->dev.of_node,
					"qcom,us-euro-gpios", 0);
	if (!pdata->us_euro_gpio_p) {
		dev_dbg(&pdev->dev, "property %s not detected in node %s",
			"qcom,us-euro-gpios", pdev->dev.of_node->full_name);
	} else {
		dev_dbg(&pdev->dev, "%s detected\n",
			"qcom,us-euro-gpios");
		wcd_mbhc_cfg.swap_gnd_mic = msm_swap_gnd_mic;
	}

	if (wcd_mbhc_cfg.enable_usbc_analog) {
		wcd_mbhc_cfg.swap_gnd_mic = msm_usbc_swap_gnd_mic;

		pdata->fsa_handle = of_parse_phandle(pdev->dev.of_node,
						"fsa4480-i2c-handle", 0);
		if (!pdata->fsa_handle)
			dev_err(&pdev->dev,
				"property %s not detected in node %s\n",
				"fsa4480-i2c-handle",
				pdev->dev.of_node->full_name);
	}

	msm_i2s_auxpcm_init(pdev);
	if (!strnstr(card->name, "tavil", strlen(card->name)) &&
	    !strnstr(card->name, "tasha", strlen(card->name))) {
		pdata->dmic01_gpio_p = of_parse_phandle(pdev->dev.of_node,
						      "qcom,cdc-dmic01-gpios",
						       0);
		pdata->dmic23_gpio_p = of_parse_phandle(pdev->dev.of_node,
						      "qcom,cdc-dmic23-gpios",
						       0);
	}

	ret = msm_audio_ssr_register(&pdev->dev);
	if (ret)
		pr_err("%s: Registration with SND event FWK failed ret = %d\n",
			__func__, ret);

err:
	return ret;
}

static int msm_asoc_machine_remove(struct platform_device *pdev)
{
	snd_event_master_deregister(&pdev->dev);
	msm_i2s_auxpcm_deinit();

	return 0;
}

static struct platform_driver sm6150_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = sm6150_asoc_machine_of_match,
		.suppress_bind_attrs = true,
	},
	.probe = msm_asoc_machine_probe,
	.remove = msm_asoc_machine_remove,
};

int __init sm6150_init(void)
{
	pr_debug("%s\n", __func__);
	return platform_driver_register(&sm6150_asoc_machine_driver);
}

void sm6150_exit(void)
{
	pr_debug("%s\n", __func__);
	platform_driver_unregister(&sm6150_asoc_machine_driver);
}

MODULE_DESCRIPTION("ALSA SoC SM6150 Machine driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, sm6150_asoc_machine_of_match);
