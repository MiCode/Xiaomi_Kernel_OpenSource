/* Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
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
/*
 * Copyright 2011, The Android Open Source Project

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
     * Neither the name of The Android Open Source Project nor the names of
       its contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED BY The Android Open Source Project ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL The Android Open Source Project BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
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
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/info.h>
#include <dsp/audio_notifier.h>
#include <dsp/q6afe-v2.h>
#include <dsp/q6core.h>
#include "device_event.h"
#include "msm-pcm-routing-v2.h"

#define DRV_NAME "sa8155-asoc-snd"

#define __CHIPSET__ "SA8155 "
#define MSM_DAILINK_NAME(name) (__CHIPSET__#name)

#define DEV_NAME_STR_LEN            32

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

#define ADSP_STATE_READY_TIMEOUT_MS 3000
#define MSM_LL_QOS_VALUE 300 /* time in us to ensure LPM doesn't go in C3/C4 */

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

struct mi2s_conf {
	struct mutex lock;
	u32 ref_cnt;
	u32 msm_is_mi2s_master;
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
	struct msm_pinctrl_info pinctrl_info;
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
	TDM_QUIN,
	TDM_INTERFACE_MAX,
};

struct tdm_port {
	u32 mode;
	u32 channel;
};

/* TDM default config */
static struct dev_config tdm_rx_cfg[TDM_INTERFACE_MAX][TDM_PORT_MAX] = {
	{ /* PRI TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	},
	{ /* SEC TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	},
	{ /* TERT TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 6}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	},
	{ /* QUAT TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 8}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	},
	{ /* QUIN TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* RX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* RX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* RX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* RX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* RX_7 */
	}
};

/* TDM default config */
static struct dev_config tdm_tx_cfg[TDM_INTERFACE_MAX][TDM_PORT_MAX] = {
	{ /* PRI TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	},
	{ /* SEC TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 6}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	},
	{ /* TERT TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 4}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 2}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	},
	{ /* QUAT TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 8}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	},
	{ /* QUIN TDM */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 6}, /* TX_0 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_1 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_2 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_3 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_4 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_5 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_6 */
		{SAMPLING_RATE_48KHZ, SNDRV_PCM_FORMAT_S16_LE, 1}, /* TX_7 */
	}
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
	{32, 16},
	/* QUIN TDM */
	{32, 16}
};

/*****************************************************************************
* TO BE UPDATED: Codec/Platform specific tdm slot table
*****************************************************************************/
static struct tdm_slot_cfg tdm_slot_custom[TDM_INTERFACE_MAX] = {
	/* PRI TDM */
	{16, 16},
	/* SEC TDM */
	{16, 16},
	/* TERT TDM */
	{16, 16},
	/* QUAT TDM */
	{16, 16},
	/* QUIN TDM */
	{16, 16}
};


/* TDM default slot offset config */
#define TDM_SLOT_OFFSET_MAX   32

static unsigned int tdm_rx_slot_offset
	[TDM_INTERFACE_MAX][TDM_PORT_MAX][TDM_SLOT_OFFSET_MAX] = {
	{/* PRI TDM */
		{0, 4, 0xFFFF},
		{8, 12, 0xFFFF},
		{16, 20, 0xFFFF},
		{24, 28, 0xFFFF},
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* SEC TDM */
		{0, 4, 0xFFFF},
		{8, 12, 0xFFFF},
		{16, 20, 0xFFFF},
		{24, 28, 0xFFFF},
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{28, 0xFFFF},
	},
	{/* TERT TDM */
		{0, 4, 8, 12, 16, 20, 0xFFFF},
		{24, 0xFFFF},
		{28, 0xFFFF},
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* QUAT TDM */
		{0, 4, 8, 12, 16, 20, 24, 28, 0xFFFF},/*AMP OUT*/
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* QUIN TDM */
		{0, 4, 0xFFFF},/*STEREO SPKR1*/
		{8, 12, 0xFFFF},/*STEREO SPKR2*/
		{16, 20, 0xFFFF},/*STEREO SPKR3*/
		{24, 28, 0xFFFF},/*STEREO SPKR4*/
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	}
};

static unsigned int tdm_tx_slot_offset
	[TDM_INTERFACE_MAX][TDM_PORT_MAX][TDM_SLOT_OFFSET_MAX] = {
	{/* PRI TDM */
		{0, 4, 0xFFFF},
		{8, 12, 0xFFFF},
		{16, 20, 0xFFFF},
		{24, 28, 0xFFFF},
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* SEC TDM */
		{0, 4, 8, 12, 16, 20, 0xFFFF},
		{24, 0xFFFF},
		{28, 0xFFFF},
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* TERT TDM */
		{0, 4, 8, 12, 0xFFFF},
		{16, 20, 0xFFFF},
		{24, 28, 0xFFFF},
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{28, 0xFFFF},
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
	},
	{/* QUIN TDM */
		{0, 4, 8, 12, 16, 20, 0xFFFF},/*EC/ANC REF*/
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	}
};

/*****************************************************************************
* TO BE UPDATED: Codec/Platform specific tdm slot offset table
* NOTE:
*     Each entry represents the slot offset array of one backend tdm device
*     valid offset represents the starting offset in byte for the channel
*     use 0xFFFF for end or unused slot offset entry.
*****************************************************************************/
static unsigned int tdm_rx_slot_offset_custom
	[TDM_INTERFACE_MAX][TDM_PORT_MAX][TDM_SLOT_OFFSET_MAX] = {
	{/* PRI TDM */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* SEC TDM */
		{0, 2, 0xFFFF},
		{4, 0xFFFF},
		{6, 0xFFFF},
		{8, 0xFFFF},
		{10, 0xFFFF},
		{12, 14, 16, 18, 20, 22, 24, 26, 0xFFFF},
		{28, 30, 0xFFFF},
		{30, 0xFFFF},
	},
	{/* TERT TDM */
		{0, 2, 0xFFFF},
		{4, 6, 8, 10, 12, 14, 16, 18, 0xFFFF},
		{20, 22, 24, 26, 28, 30, 0xFFFF},
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* QUAT TDM */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* QUIN TDM */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	}
};

static unsigned int tdm_tx_slot_offset_custom
	[TDM_INTERFACE_MAX][TDM_PORT_MAX][TDM_SLOT_OFFSET_MAX] = {
	{/* PRI TDM */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* SEC TDM */
		{0, 2, 0xFFFF},
		{4, 6, 8, 10, 12, 14, 16, 18, 0xFFFF},
		{20, 22, 24, 26, 28, 30, 0xFFFF},
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* TERT TDM */
		{0, 2, 4, 6, 8, 10, 12, 0xFFFF},
		{14, 16, 0xFFFF},
		{18, 20, 22, 24, 26, 28, 30, 0xFFFF},
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{30, 0xFFFF},
	},
	{/* QUAT TDM */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	},
	{/* QUIN TDM */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
		{0xFFFF}, /* not used */
	}
};


static char const *bit_format_text[] = {"S16_LE", "S24_LE", "S24_3LE",
					  "S32_LE"};
static char const *ext_disp_bit_format_text[] = {"S16_LE", "S24_LE",
						 "S24_3LE"};
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
static char const *tdm_ch_text[] = {
	"One", "Two", "Three", "Four",
	"Five", "Six", "Seven", "Eight",
	"Nine", "Ten", "Eleven", "Twelve",
	"Thirteen", "Fourteen", "Fifteen", "Sixteen",
	"Seventeen", "Eighteen", "Nineteen", "Twenty",
	"TwentyOne", "TwentyTwo", "TwentyThree", "TwentyFour",
	"TwentyFive", "TwentySix", "TwentySeven", "TwentyEight",
	"TwentyNine", "Thirty", "ThirtyOne", "ThirtyTwo"};
static char const *tdm_bit_format_text[] = {"S16_LE", "S24_LE", "S32_LE"};
static char const *tdm_sample_rate_text[] = {"KHZ_8", "KHZ_16", "KHZ_32",
					     "KHZ_48", "KHZ_176P4",
					     "KHZ_352P8"};
static const char *const tdm_slot_num_text[] = {"One", "Two", "Four",
	"Eight", "Sixteen", "ThirtyTwo"};
static const char *const tdm_slot_width_text[] = {"16", "24", "32"};
static const char *const auxpcm_rate_text[] = {"KHZ_8", "KHZ_16"};
static char const *mi2s_rate_text[] = {"KHZ_8", "KHZ_11P025", "KHZ_16",
				      "KHZ_22P05", "KHZ_32", "KHZ_44P1",
				      "KHZ_48", "KHZ_96", "KHZ_192"};
static const char *const mi2s_ch_text[] = {"One", "Two", "Three", "Four",
					   "Five", "Six", "Seven",
					   "Eight"};
static const char *const qos_text[] = {"Disable", "Enable"};

static SOC_ENUM_SINGLE_EXT_DECL(usb_rx_chs, usb_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_tx_chs, usb_ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(ext_disp_rx_chs, ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(proxy_rx_chs, ch_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_rx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(usb_tx_format, bit_format_text);
static SOC_ENUM_SINGLE_EXT_DECL(ext_disp_rx_format, ext_disp_bit_format_text);
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

static bool is_initial_boot = true;

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

static struct mi2s_conf mi2s_intf_conf[MI2S_MAX];

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
	} else if (strnstr(kcontrol->id.name, "QUIN",
	    sizeof(kcontrol->id.name))) {
		mode = TDM_QUIN;
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
		slot_offset = tdm_rx_slot_offset[port.mode][port.channel];
		pr_debug("%s: mode = %d, channel = %d\n",
				__func__, port.mode, port.channel);
		for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
			ucontrol->value.integer.value[i] = slot_offset[i];
			pr_debug("%s: offset %d, value %d\n",
					__func__, i, slot_offset[i]);
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
		slot_offset = tdm_rx_slot_offset[port.mode][port.channel];
		pr_debug("%s: mode = %d, channel = %d\n",
				__func__, port.mode, port.channel);
		for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
			slot_offset[i] = ucontrol->value.integer.value[i];
			pr_debug("%s: offset %d, value %d\n",
					__func__, i, slot_offset[i]);
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
		slot_offset = tdm_tx_slot_offset[port.mode][port.channel];
		pr_debug("%s: mode = %d, channel = %d\n",
				__func__, port.mode, port.channel);
		for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
			ucontrol->value.integer.value[i] = slot_offset[i];
			pr_debug("%s: offset %d, value %d\n",
					__func__, i, slot_offset[i]);
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
		slot_offset = tdm_tx_slot_offset[port.mode][port.channel];
		pr_debug("%s: mode = %d, channel = %d\n",
				__func__, port.mode, port.channel);
		for (i = 0; i < TDM_SLOT_OFFSET_MAX; i++) {
			slot_offset[i] = ucontrol->value.integer.value[i];
			pr_debug("%s: offset %d, value %d\n",
					__func__, i, slot_offset[i]);
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
	else if (strnstr(kcontrol->id.name, "QUIN_AUX_PCM",
			 sizeof("QUIN_AUX_PCM")))
		idx = QUIN_AUX_PCM;
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
	else if (strnstr(kcontrol->id.name, "QUIN_MI2S_RX",
		 sizeof("QUIN_MI2S_RX")))
		idx = QUIN_MI2S;
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
	else if (strnstr(kcontrol->id.name, "QUIN_MI2S_TX",
		 sizeof("QUIN_MI2S_TX")))
		idx = QUIN_MI2S;
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

static const struct snd_kcontrol_new msm_snd_controls[] = {
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
	SOC_ENUM_EXT("PRI_TDM_RX_1 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_RX_2 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_RX_3 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_TX_1 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_TX_2 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_TX_3 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_1 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_2 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_3 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_1 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_2 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("PRI_TDM_TX_3 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("PRI_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("PRI_TDM_RX_1 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("PRI_TDM_RX_2 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("PRI_TDM_RX_3 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("PRI_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("PRI_TDM_TX_1 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("PRI_TDM_TX_2 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("PRI_TDM_TX_3 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_RX_1 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_RX_2 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_RX_3 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_TX_1 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_TX_2 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_TX_3 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_1 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_2 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_3 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_1 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_2 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("SEC_TDM_TX_3 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("SEC_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("SEC_TDM_RX_1 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("SEC_TDM_RX_2 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("SEC_TDM_RX_3 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("SEC_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("SEC_TDM_TX_1 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("SEC_TDM_TX_2 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("SEC_TDM_TX_3 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("TERT_TDM_RX_1 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("TERT_TDM_RX_2 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("TERT_TDM_RX_3 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("TERT_TDM_RX_4 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("TERT_TDM_TX_1 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("TERT_TDM_TX_2 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("TERT_TDM_TX_3 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_1 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_2 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_3 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_4 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_1 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_2 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("TERT_TDM_TX_3 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("TERT_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("TERT_TDM_RX_1 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("TERT_TDM_RX_2 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("TERT_TDM_RX_3 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("TERT_TDM_RX_4 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("TERT_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("TERT_TDM_TX_1 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("TERT_TDM_TX_2 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("TERT_TDM_TX_3 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_1 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_2 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_3 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_1 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_2 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_3 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_1 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_2 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_3 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_1 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_2 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_3 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_1 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_2 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_RX_3 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_1 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_2 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("QUAT_TDM_TX_3 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_0 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_1 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_2 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_3 SampleRate", tdm_rx_sample_rate,
			tdm_rx_sample_rate_get,
			tdm_rx_sample_rate_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_0 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_1 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_2 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_3 SampleRate", tdm_tx_sample_rate,
			tdm_tx_sample_rate_get,
			tdm_tx_sample_rate_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_0 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_1 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_2 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_3 Format", tdm_rx_format,
			tdm_rx_format_get,
			tdm_rx_format_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_0 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_1 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_2 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_3 Format", tdm_tx_format,
			tdm_tx_format_get,
			tdm_tx_format_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_0 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_1 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_2 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("QUIN_TDM_RX_3 Channels", tdm_rx_chs,
			tdm_rx_ch_get,
			tdm_rx_ch_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_0 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_1 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_2 Channels", tdm_tx_chs,
			tdm_tx_ch_get,
			tdm_tx_ch_put),
	SOC_ENUM_EXT("QUIN_TDM_TX_3 Channels", tdm_tx_chs,
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
	SOC_ENUM_EXT("QUIN_TDM SlotNumber", tdm_slot_num,
			tdm_slot_num_get, tdm_slot_num_put),
	SOC_ENUM_EXT("QUIN_TDM SlotWidth", tdm_slot_width,
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
	SOC_SINGLE_MULTI_EXT("QUIN_TDM_RX_0 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUIN_TDM_RX_1 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUIN_TDM_RX_2 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUIN_TDM_RX_3 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_rx_slot_mapping_get, tdm_rx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUIN_TDM_TX_0 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUIN_TDM_TX_1 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUIN_TDM_TX_2 SlotMapping",
		SND_SOC_NOPM, 0, 0xFFFF, 0, TDM_SLOT_OFFSET_MAX,
		tdm_tx_slot_mapping_get, tdm_tx_slot_mapping_put),
	SOC_SINGLE_MULTI_EXT("QUIN_TDM_TX_3 SlotMapping",
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

static int msm_ext_disp_get_idx_from_beid(int32_t be_id)
{
	int idx;

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
	int rc = 0;
	int idx;

	pr_debug("%s: format = %d, rate = %d\n",
		  __func__, params_format(params), params_rate(params));

	switch (dai_link->id) {
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

	default:
		rate->min = rate->max = SAMPLING_RATE_48KHZ;
		break;
	}

done:
	return rc;
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

#ifdef ENABLE_PINCTRL
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
						"quat_mi2s_disable");
	if (IS_ERR(pinctrl_info->mi2s_disable)) {
		pr_err("%s: could not get mi2s_disable pinstate\n", __func__);
		goto err;
	}
	pinctrl_info->mi2s_active = pinctrl_lookup_state(pinctrl,
						"quat_mi2s_enable");
	if (IS_ERR(pinctrl_info->mi2s_active)) {
		pr_err("%s: could not get mi2s_active pinstate\n", __func__);
		goto err;
	}
	pinctrl_info->tdm_disable = pinctrl_lookup_state(pinctrl,
						"quat_tdm_disable");
	if (IS_ERR(pinctrl_info->tdm_disable)) {
		pr_err("%s: could not get tdm_disable pinstate\n", __func__);
		goto err;
	}
	pinctrl_info->tdm_active = pinctrl_lookup_state(pinctrl,
						"quat_tdm_enable");
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
#else
static int msm_set_pinctrl(struct msm_pinctrl_info *pinctrl_info,
				enum pinctrl_pin_state new_state)
{
	return 0;
}

static void msm_release_pinctrl(struct platform_device *pdev)
{
	return;
}

static int msm_get_pinctrl(struct platform_device *pdev)
{
	return 0;
}
#endif

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
	case AFE_PORT_ID_QUINARY_TDM_RX:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUIN][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_QUIN][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_QUIN][TDM_0].sample_rate;
		break;
	case AFE_PORT_ID_QUINARY_TDM_RX_1:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUIN][TDM_1].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_QUIN][TDM_1].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_QUIN][TDM_1].sample_rate;
		break;
	case AFE_PORT_ID_QUINARY_TDM_RX_2:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUIN][TDM_2].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_QUIN][TDM_2].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_QUIN][TDM_2].sample_rate;
		break;
	case AFE_PORT_ID_QUINARY_TDM_RX_3:
		channels->min = channels->max =
				tdm_rx_cfg[TDM_QUIN][TDM_3].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_rx_cfg[TDM_QUIN][TDM_3].bit_format);
		rate->min = rate->max =
				tdm_rx_cfg[TDM_QUIN][TDM_3].sample_rate;
		break;
	case AFE_PORT_ID_QUINARY_TDM_TX:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUIN][TDM_0].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_QUIN][TDM_0].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_QUIN][TDM_0].sample_rate;
		break;
	case AFE_PORT_ID_QUINARY_TDM_TX_1:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUIN][TDM_1].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_QUIN][TDM_1].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_QUIN][TDM_1].sample_rate;
		break;
	case AFE_PORT_ID_QUINARY_TDM_TX_2:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUIN][TDM_2].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_QUIN][TDM_2].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_QUIN][TDM_2].sample_rate;
		break;
	case AFE_PORT_ID_QUINARY_TDM_TX_3:
		channels->min = channels->max =
				tdm_tx_cfg[TDM_QUIN][TDM_3].channels;
		param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				tdm_tx_cfg[TDM_QUIN][TDM_3].bit_format);
		rate->min = rate->max =
				tdm_tx_cfg[TDM_QUIN][TDM_3].sample_rate;
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

static int sa8155_tdm_snd_hw_params(struct snd_pcm_substream *substream,
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
	case AFE_PORT_ID_QUINARY_TDM_RX:
		slots = tdm_slot[TDM_QUIN].num;
		slot_width = tdm_slot[TDM_QUIN].width;
		slot_offset = tdm_rx_slot_offset[TDM_QUIN][TDM_0];
		break;
	case AFE_PORT_ID_QUINARY_TDM_RX_1:
		slots = tdm_slot[TDM_QUIN].num;
		slot_width = tdm_slot[TDM_QUIN].width;
		slot_offset = tdm_rx_slot_offset[TDM_QUIN][TDM_1];
		break;
	case AFE_PORT_ID_QUINARY_TDM_RX_2:
		slots = tdm_slot[TDM_QUIN].num;
		slot_width = tdm_slot[TDM_QUIN].width;
		slot_offset = tdm_rx_slot_offset[TDM_QUIN][TDM_2];
		break;
	case AFE_PORT_ID_QUINARY_TDM_RX_3:
		slots = tdm_slot[TDM_QUIN].num;
		slot_width = tdm_slot[TDM_QUIN].width;
		slot_offset = tdm_rx_slot_offset[TDM_QUIN][TDM_3];
		break;
	case AFE_PORT_ID_QUINARY_TDM_TX:
		slots = tdm_slot[TDM_QUIN].num;
		slot_width = tdm_slot[TDM_QUIN].width;
		slot_offset = tdm_tx_slot_offset[TDM_QUIN][TDM_0];
		break;
	case AFE_PORT_ID_QUINARY_TDM_TX_1:
		slots = tdm_slot[TDM_QUIN].num;
		slot_width = tdm_slot[TDM_QUIN].width;
		slot_offset = tdm_tx_slot_offset[TDM_QUIN][TDM_1];
		break;
	case AFE_PORT_ID_QUINARY_TDM_TX_2:
		slots = tdm_slot[TDM_QUIN].num;
		slot_width = tdm_slot[TDM_QUIN].width;
		slot_offset = tdm_tx_slot_offset[TDM_QUIN][TDM_2];
		break;
	case AFE_PORT_ID_QUINARY_TDM_TX_3:
		slots = tdm_slot[TDM_QUIN].num;
		slot_width = tdm_slot[TDM_QUIN].width;
		slot_offset = tdm_tx_slot_offset[TDM_QUIN][TDM_3];
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

static int sa8155_tdm_snd_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = &pdata->pinctrl_info;

	/* currently only supporting TDM_RX_0 and TDM_TX_0 */
	if ((cpu_dai->id == AFE_PORT_ID_QUATERNARY_TDM_RX) ||
		(cpu_dai->id == AFE_PORT_ID_QUATERNARY_TDM_TX)) {
		ret = msm_set_pinctrl(pinctrl_info, STATE_TDM_ACTIVE);
		if (ret)
			pr_err("%s: TDM TLMM pinctrl set failed with %d\n",
				__func__, ret);
	}

	return ret;
}

static void sa8155_tdm_snd_shutdown(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = &pdata->pinctrl_info;

	/* currently only supporting TDM_RX_0 and TDM_TX_0 */
	if ((cpu_dai->id == AFE_PORT_ID_QUATERNARY_TDM_RX) ||
		(cpu_dai->id == AFE_PORT_ID_QUATERNARY_TDM_TX)) {
		ret = msm_set_pinctrl(pinctrl_info, STATE_DISABLE);
		if (ret)
			pr_err("%s: TDM TLMM pinctrl set failed with %d\n",
				__func__, ret);
	}
}

static struct snd_soc_ops sa8155_tdm_be_ops = {
	.hw_params = sa8155_tdm_snd_hw_params,
	.startup = sa8155_tdm_snd_startup,
	.shutdown = sa8155_tdm_snd_shutdown
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
	unsigned int fmt = SND_SOC_DAIFMT_CBS_CFS;
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = &pdata->pinctrl_info;
	int ret_pinctrl = 0;

	dev_dbg(rtd->card->dev,
		"%s: substream = %s  stream = %d, dai name %s, dai ID %d\n",
		__func__, substream->name, substream->stream,
		cpu_dai->name, cpu_dai->id);

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
		if (index == QUAT_MI2S) {
			ret_pinctrl = msm_set_pinctrl(pinctrl_info,
						      STATE_MI2S_ACTIVE);
			if (ret_pinctrl)
				pr_err("%s: MI2S TLMM pinctrl set failed with %d\n",
					__func__, ret_pinctrl);
		}
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
	struct snd_soc_card *card = rtd->card;
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);
	struct msm_pinctrl_info *pinctrl_info = &pdata->pinctrl_info;
	int ret_pinctrl = 0;

	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	if (index < PRIM_MI2S || index >= MI2S_MAX) {
		pr_err("%s:invalid MI2S DAI(%d)\n", __func__, index);
		return;
	}

	mutex_lock(&mi2s_intf_conf[index].lock);
	if (--mi2s_intf_conf[index].ref_cnt == 0) {
		ret = msm_mi2s_set_sclk(substream, false);
		if (ret < 0)
			pr_err("%s:clock disable failed for MI2S (%d); ret=%d\n",
				__func__, index, ret);
		if (index == QUAT_MI2S) {
			ret_pinctrl = msm_set_pinctrl(pinctrl_info,
						      STATE_DISABLE);
			if (ret_pinctrl)
				pr_err("%s: MI2S TLMM pinctrl set failed with %d\n",
					__func__, ret_pinctrl);
		}
	}
	mutex_unlock(&mi2s_intf_conf[index].lock);
}

static struct snd_soc_ops msm_mi2s_be_ops = {
	.startup = msm_mi2s_snd_startup,
	.shutdown = msm_mi2s_snd_shutdown,
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA1
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA2,
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
		.id = MSM_FRONTEND_DAI_VOICEMMODE1,
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
		.id = MSM_FRONTEND_DAI_VOIP,
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA3,
	},
	/* - SLIMBUS_0 Hostless */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA4,
	},
	/* Hostless PCM purpose */
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
	/* - SLIMBUS_1 Hostless */
	/* - SLIMBUS_3 Hostless */
	/* - SLIMBUS_4 Hostless */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA5,
		.ops = &msm_fe_qos_ops,
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_LSM1,
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA7,
	},
	{
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA8,
		.ops = &msm_fe_qos_ops,
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
		.id = MSM_FRONTEND_DAI_VOICEMMODE2,
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_LSM2,
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_LSM3,
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_LSM4,
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_LSM5,
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_LSM6,
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_LSM7,
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
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_LSM8,
	},
	/* - Multimedia9 */
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA11,
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA12,
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA13,
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA14,
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA15,
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA16,
	},
	/* - SLIMBUS_8 Hostless */
	/* - Slimbus4 Capture */
	/* - SLIMBUS_2 Hostless Playback */
	/* - SLIMBUS_2 Hostless Capture */
	/* HFP TX */
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
	/* - SLIMBUS_7 Hostless */
	{
		.name = "Compress Capture",
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
		.ignore_pmdown_time = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA17,
	},
};

static struct snd_soc_dai_link msm_auto_fe_dai_links[] = {
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
	/* Low latency ASM loopback for ICC */
	{
		.name = MSM_DAILINK_NAME(LowLatency Loopback),
		.stream_name = "MultiMedia9",
		.cpu_dai_name = "MultiMedia9",
		.platform_name = "msm-pcm-loopback.1",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
				SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA9,
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
		.name = MSM_DAILINK_NAME(Media20),
		.stream_name = "MultiMedia20",
		.cpu_dai_name = "MultiMedia20",
		.platform_name = "msm-pcm-loopback",
		.dynamic = 1,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.id = MSM_FRONTEND_DAI_MULTIMEDIA20,
	},
	{
		.name = MSM_DAILINK_NAME(HFP RX),
		.stream_name = "MultiMedia21",
		.cpu_dai_name = "MultiMedia21",
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA21,
	},
	/* TDM Hostless */
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
		.name = "Quaternary MI2S_RX Hostless Playback",
		.stream_name = "Quaternary MI2S_RX Hostless Playback",
		.cpu_dai_name = "QUAT_MI2S_RX_HOSTLESS",
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
		.name = "Secondary MI2S_TX Hostless Capture",
		.stream_name = "Secondary MI2S_TX Hostless Capture",
		.cpu_dai_name = "SEC_MI2S_TX_HOSTLESS",
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
		.name = "DTMF RX Hostless",
		.stream_name = "DTMF RX Hostless",
		.cpu_dai_name = "DTMF_RX_HOSTLESS",
		.platform_name = "msm-pcm-dtmf",
		.dynamic = 1,
		.dpcm_playback = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			    SND_SOC_DPCM_TRIGGER_POST},
		.no_host_mode = SND_SOC_DAI_LINK_NO_HOST,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.id = MSM_FRONTEND_DAI_DTMF_RX,
	},
	{
		.name = "Secondary TDM RX 7 Hostless",
		.stream_name = "Secondary TDM RX 7 Hostless",
		.cpu_dai_name = "SEC_TDM_RX_7_HOSTLESS",
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
		.name = "Tertiary TDM TX 7 Hostless",
		.stream_name = "Tertiary TDM TX 7 Hostless",
		.cpu_dai_name = "TERT_TDM_TX_7_HOSTLESS",
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

static struct snd_soc_dai_link msm_custom_fe_dai_links[] = {
	/* FrontEnd DAI Links */
	{
		.name = MSM_DAILINK_NAME(Media1),
		.stream_name = "MultiMedia1",
		.cpu_dai_name = "MultiMedia1",
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA1,
		.ops = &msm_fe_qos_ops,
	},
	{
		.name = MSM_DAILINK_NAME(Media2),
		.stream_name = "MultiMedia2",
		.cpu_dai_name = "MultiMedia2",
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA2,
		.ops = &msm_fe_qos_ops,
	},
	{
		.name = MSM_DAILINK_NAME(Media3),
		.stream_name = "MultiMedia3",
		.cpu_dai_name = "MultiMedia3",
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA3,
		.ops = &msm_fe_qos_ops,
	},
	{
		.name = MSM_DAILINK_NAME(Media5),
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
	{
		.name = MSM_DAILINK_NAME(Media6),
		.stream_name = "MultiMedia6",
		.cpu_dai_name = "MultiMedia6",
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA6,
		.ops = &msm_fe_qos_ops,
	},
	{
		.name = MSM_DAILINK_NAME(Media8),
		.stream_name = "MultiMedia8",
		.cpu_dai_name = "MultiMedia8",
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA8,
		.ops = &msm_fe_qos_ops,
	},
	{
		.name = MSM_DAILINK_NAME(Media9),
		.stream_name = "MultiMedia9",
		.cpu_dai_name = "MultiMedia9",
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA9,
		.ops = &msm_fe_qos_ops,
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
		.name = MSM_DAILINK_NAME(Media20),
		.stream_name = "MultiMedia20",
		.cpu_dai_name = "MultiMedia20",
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
		.id = MSM_FRONTEND_DAI_MULTIMEDIA20,
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
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
		.ignore_suspend = 1,
	},
};

static struct snd_soc_dai_link msm_auto_be_dai_links[] = {
	/* Backend DAI Links */
	{
		.name = LPASS_BE_PRI_TDM_RX_1,
		.stream_name = "Primary TDM1 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36866",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_PRI_TDM_RX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_PRI_TDM_RX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_PRI_TDM_RX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_PRI_TDM_TX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_PRI_TDM_TX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_PRI_TDM_TX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_SEC_TDM_RX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_SEC_TDM_RX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_SEC_TDM_RX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_SEC_TDM_RX_7,
		.stream_name = "Secondary TDM7 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36894",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_SEC_TDM_RX_7,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_SEC_TDM_TX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_SEC_TDM_TX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_SEC_TDM_TX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_TERT_TDM_RX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_TERT_TDM_RX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_TERT_TDM_RX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_RX_4,
		.stream_name = "Tertiary TDM4 Playback",
		.cpu_dai_name = "msm-dai-q6-tdm.36904",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_playback = 1,
		.id = MSM_BACKEND_DAI_TERT_TDM_RX_4,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_TERT_TDM_TX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_TERT_TDM_TX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_TERT_TDM_TX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
		.ignore_suspend = 1,
	},
	{
		.name = LPASS_BE_TERT_TDM_TX_7,
		.stream_name = "Tertiary TDM7 Capture",
		.cpu_dai_name = "msm-dai-q6-tdm.36911",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.dpcm_capture = 1,
		.id = MSM_BACKEND_DAI_TERT_TDM_TX_7,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_QUAT_TDM_RX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_QUAT_TDM_RX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_QUAT_TDM_RX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_QUAT_TDM_TX_1,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_QUAT_TDM_TX_2,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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
		.id = MSM_BACKEND_DAI_QUAT_TDM_TX_3,
		.be_hw_params_fixup = msm_tdm_be_hw_params_fixup,
		.ops = &sa8155_tdm_be_ops,
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

static struct snd_soc_dai_link msm_auto_dai_links[
			 ARRAY_SIZE(msm_common_dai_links) +
			 ARRAY_SIZE(msm_auto_fe_dai_links) +
			 ARRAY_SIZE(msm_common_be_dai_links) +
			 ARRAY_SIZE(msm_auto_be_dai_links) +
			 ARRAY_SIZE(ext_disp_be_dai_link) +
			 ARRAY_SIZE(msm_mi2s_be_dai_links) +
			 ARRAY_SIZE(msm_auxpcm_be_dai_links)];

static struct snd_soc_dai_link msm_auto_custom_dai_links[
			 ARRAY_SIZE(msm_custom_fe_dai_links) +
			 ARRAY_SIZE(msm_auto_fe_dai_links) +
			 ARRAY_SIZE(msm_common_be_dai_links) +
			 ARRAY_SIZE(msm_auto_be_dai_links) +
			 ARRAY_SIZE(ext_disp_be_dai_link) +
			 ARRAY_SIZE(msm_mi2s_be_dai_links) +
			 ARRAY_SIZE(msm_auxpcm_be_dai_links)];

struct snd_soc_card snd_soc_card_auto_msm = {
	.name = "sa8155-adp-star-snd-card",
};

struct snd_soc_card snd_soc_card_auto_custom_msm = {
	.name = "sa8155-custom-snd-card",
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

static const struct of_device_id sa8155_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,sa8155-asoc-snd-adp-star",
	  .data = "adp_star_codec"},
	{ .compatible = "qcom,sa8155-asoc-snd-custom",
	  .data = "custom_codec"},
	{},
};

static struct snd_soc_card *populate_snd_card_dailinks(struct device *dev)
{
	struct snd_soc_card *card = NULL;
	struct snd_soc_dai_link *dailink;
	int len_1, len_2, len_3;
	int total_links;
	const struct of_device_id *match;

	match = of_match_node(sa8155_asoc_machine_of_match, dev->of_node);
	if (!match) {
		dev_err(dev, "%s: No DT match found for sound card\n",
			__func__);
		return NULL;
	}

	if (!strcmp(match->data, "adp_star_codec")) {
		card = &snd_soc_card_auto_msm;
		len_1 = ARRAY_SIZE(msm_common_dai_links);
		len_2 = len_1 + ARRAY_SIZE(msm_auto_fe_dai_links);
		len_3 = len_2 + ARRAY_SIZE(msm_common_be_dai_links);
		total_links = len_3 + ARRAY_SIZE(msm_auto_be_dai_links);
		memcpy(msm_auto_dai_links,
			   msm_common_dai_links,
			   sizeof(msm_common_dai_links));
		memcpy(msm_auto_dai_links + len_1,
			   msm_auto_fe_dai_links,
			   sizeof(msm_auto_fe_dai_links));
		memcpy(msm_auto_dai_links + len_2,
			   msm_common_be_dai_links,
			   sizeof(msm_common_be_dai_links));
		memcpy(msm_auto_dai_links + len_3,
			   msm_auto_be_dai_links,
			   sizeof(msm_auto_be_dai_links));

		if (of_property_read_bool(dev->of_node,
					  "qcom,ext-disp-audio-rx")) {
			dev_dbg(dev, "%s(): ext disp audio support present\n",
				__func__);
			memcpy(msm_auto_dai_links + total_links,
			       ext_disp_be_dai_link,
			       sizeof(ext_disp_be_dai_link));
			total_links += ARRAY_SIZE(ext_disp_be_dai_link);
		}
		if (of_property_read_bool(dev->of_node,
					  "qcom,mi2s-audio-intf")) {
			memcpy(msm_auto_dai_links + total_links,
			       msm_mi2s_be_dai_links,
			       sizeof(msm_mi2s_be_dai_links));
			total_links += ARRAY_SIZE(msm_mi2s_be_dai_links);
		}
		if (of_property_read_bool(dev->of_node,
					  "qcom,auxpcm-audio-intf")) {
			memcpy(msm_auto_dai_links + total_links,
			msm_auxpcm_be_dai_links,
			sizeof(msm_auxpcm_be_dai_links));
			total_links += ARRAY_SIZE(msm_auxpcm_be_dai_links);
		}

		dailink = msm_auto_dai_links;
	}  else if (!strcmp(match->data, "custom_codec")) {
		card = &snd_soc_card_auto_custom_msm;
		len_1 = ARRAY_SIZE(msm_custom_fe_dai_links);
		len_2 = len_1 + ARRAY_SIZE(msm_auto_fe_dai_links);
		len_3 = len_2 + ARRAY_SIZE(msm_common_be_dai_links);
		total_links = len_3 + ARRAY_SIZE(msm_auto_be_dai_links);
		memcpy(msm_auto_custom_dai_links,
		       msm_custom_fe_dai_links,
		       sizeof(msm_custom_fe_dai_links));
		memcpy(msm_auto_custom_dai_links + len_1,
		       msm_auto_fe_dai_links,
		       sizeof(msm_auto_fe_dai_links));
		memcpy(msm_auto_custom_dai_links + len_2,
		       msm_common_be_dai_links,
		       sizeof(msm_common_be_dai_links));
		memcpy(msm_auto_custom_dai_links + len_3,
		       msm_auto_be_dai_links,
		       sizeof(msm_auto_be_dai_links));

		if (of_property_read_bool(dev->of_node,
					  "qcom,ext-disp-audio-rx")) {
			dev_dbg(dev, "%s(): ext disp audio support present\n",
				__func__);
			memcpy(msm_auto_custom_dai_links + total_links,
			       ext_disp_be_dai_link,
			       sizeof(ext_disp_be_dai_link));
			total_links += ARRAY_SIZE(ext_disp_be_dai_link);
		}
		if (of_property_read_bool(dev->of_node,
					  "qcom,mi2s-audio-intf")) {
			memcpy(msm_auto_custom_dai_links + total_links,
			       msm_mi2s_be_dai_links,
			       sizeof(msm_mi2s_be_dai_links));
			total_links += ARRAY_SIZE(msm_mi2s_be_dai_links);
		}
		if (of_property_read_bool(dev->of_node,
					  "qcom,auxpcm-audio-intf")) {
			memcpy(msm_auto_custom_dai_links + total_links,
			msm_auxpcm_be_dai_links,
			sizeof(msm_auxpcm_be_dai_links));
			total_links += ARRAY_SIZE(msm_auxpcm_be_dai_links);
		}
		dailink = msm_auto_custom_dai_links;
	} else {
		dev_err(dev, "%s: Codec not supported\n",
			__func__);
		return NULL;
	}

	if (card) {
		card->dai_link = dailink;
		card->num_links = total_links;
	}

	return card;
}

/*****************************************************************************
* TO BE UPDATED: Codec/Platform specific tdm slot and offset table selection
*****************************************************************************/
static int msm_tdm_init(struct device *dev)
{
	const struct of_device_id *match;

	match = of_match_node(sa8155_asoc_machine_of_match, dev->of_node);
	if (!match) {
		dev_err(dev, "%s: No DT match found for sound card\n",
			__func__);
		return -EINVAL;
	}

	if (!strcmp(match->data, "custom_codec")) {
		dev_dbg(dev, "%s: custom tdm configuration\n", __func__);

		memcpy(tdm_rx_slot_offset,
			tdm_rx_slot_offset_custom,
			sizeof(tdm_rx_slot_offset_custom));
		memcpy(tdm_tx_slot_offset,
			tdm_tx_slot_offset_custom,
			sizeof(tdm_tx_slot_offset_custom));
		memcpy(tdm_slot,
			tdm_slot_custom,
			sizeof(tdm_slot_custom));
	} else {
		dev_dbg(dev, "%s: default tdm configuration\n", __func__);
	}

	return 0;
}

static void msm_i2s_auxpcm_init(struct platform_device *pdev)
{
	int count;
	u32 mi2s_master_slave[MI2S_MAX];
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
}

static void msm_i2s_auxpcm_deinit(void)
{
	int count;

	for (count = 0; count < MI2S_MAX; count++) {
		mutex_destroy(&mi2s_intf_conf[count].lock);
		mi2s_intf_conf[count].ref_cnt = 0;
		mi2s_intf_conf[count].msm_is_mi2s_master = 0;
	}
}
static int msm_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct msm_asoc_mach_data *pdata;
	int ret;
	enum apr_subsys_state q6_state;

	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}

	q6_state = apr_get_q6_state();
	if (q6_state == APR_SUBSYS_DOWN) {
		dev_dbg(&pdev->dev, "deferring %s, adsp_state %d\n",
			__func__, q6_state);
		return -EPROBE_DEFER;
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

	ret = msm_populate_dai_link_component_of_node(card);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}

	/* Populate controls of snd card */
	card->controls = msm_snd_controls;
	card->num_controls = ARRAY_SIZE(msm_snd_controls);

	ret = msm_tdm_init(&pdev->dev);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}

	ret = devm_snd_soc_register_card(&pdev->dev, card);
	if (ret == -EPROBE_DEFER) {
		goto err;
	} else if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}
	dev_info(&pdev->dev, "Sound card %s registered\n", card->name);

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

	msm_i2s_auxpcm_init(pdev);

	return 0;
err:
	msm_release_pinctrl(pdev);
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int msm_asoc_machine_remove(struct platform_device *pdev)
{
	msm_i2s_auxpcm_deinit();

	msm_release_pinctrl(pdev);
	return 0;
}

static struct platform_driver sa8155_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = sa8155_asoc_machine_of_match,
	},
	.probe = msm_asoc_machine_probe,
	.remove = msm_asoc_machine_remove,
};

static int dummy_asoc_machine_probe(struct platform_device *pdev)
{
	return 0;
}

static int dummy_asoc_machine_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_device sa8155_dummy_asoc_machine_device = {
	.name = "sa8155-asoc-snd-dummy",
};

static struct platform_driver sa8155_dummy_asoc_machine_driver = {
	.driver = {
		.name = "sa8155-asoc-snd-dummy",
		.owner = THIS_MODULE,
	},
	.probe = dummy_asoc_machine_probe,
	.remove = dummy_asoc_machine_remove,
};

static int sa8155_notifier_service_cb(struct notifier_block *this,
					 unsigned long opcode, void *ptr)
{
	pr_debug("%s: Service opcode 0x%lx\n", __func__, opcode);

	switch (opcode) {
	case AUDIO_NOTIFIER_SERVICE_DOWN:
		break;
	case AUDIO_NOTIFIER_SERVICE_UP:
		if (is_initial_boot) {
			platform_driver_register(&sa8155_dummy_asoc_machine_driver);
			platform_device_register(&sa8155_dummy_asoc_machine_device);
			is_initial_boot = false;
		}
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block service_nb = {
	.notifier_call  = sa8155_notifier_service_cb,
	.priority = -INT_MAX,
};

int __init sa8155_init(void)
{
	pr_debug("%s\n", __func__);
	audio_notifier_register("sa8155", AUDIO_NOTIFIER_ADSP_DOMAIN,
				      &service_nb);
	return platform_driver_register(&sa8155_asoc_machine_driver);
}

void sa8155_exit(void)
{
	pr_debug("%s\n", __func__);
	platform_driver_unregister(&sa8155_asoc_machine_driver);
	audio_notifier_deregister("sa8155");
}

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, sa8155_asoc_machine_of_match);
