/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6768-afe-common.h  --  Mediatek 6768 audio driver definitions
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

#ifndef _MT_6768_AFE_COMMON_H_
#define _MT_6768_AFE_COMMON_H_
#include <sound/soc.h>
#include <linux/list.h>
#include <linux/regmap.h>
#include "mt6768-reg.h"
#include "../common/mtk-base-afe.h"
#include "../common/mtk-sp-common.h"

enum {
	MT6768_MEMIF_DL1,
	MT6768_MEMIF_DL12,
	MT6768_MEMIF_DL2,
	MT6768_MEMIF_DL3,
	MT6768_MEMIF_MOD_DAI,
	MT6768_MEMIF_VUL,
	MT6768_MEMIF_VUL12,
	MT6768_MEMIF_VUL2,
	MT6768_MEMIF_AWB,
	MT6768_MEMIF_AWB2,
	MT6768_MEMIF_NUM,
	MT6768_DAI_ADDA = MT6768_MEMIF_NUM,
	MT6768_DAI_VOW,
	MT6768_DAI_CONNSYS_I2S,
	MT6768_DAI_I2S_0,
	MT6768_DAI_I2S_1,
	MT6768_DAI_I2S_2,
	MT6768_DAI_I2S_3,
	MT6768_DAI_HW_GAIN_1,
	MT6768_DAI_HW_GAIN_2,
	MT6768_DAI_PCM_2,
	MT6768_DAI_HOSTLESS_LPBK,
	MT6768_DAI_HOSTLESS_FM,
	MT6768_DAI_HOSTLESS_SPEECH,
	MT6768_DAI_HOSTLESS_SPH_ECHO_REF,
	MT6768_DAI_HOSTLESS_SPK_INIT,
	MT6768_DAI_HOSTLESS_IMPEDANCE,
	MT6768_DAI_HOSTLESS_UL1,
	MT6768_DAI_HOSTLESS_UL2,
	MT6768_DAI_HOSTLESS_UL3,
	MT6768_DAI_HOSTLESS_UL4,
	MT6768_DAI_HOSTLESS_DSP_DL,
	MT6768_DAI_NUM,
};

#define MT6768_RECORD_MEMIF MT6768_MEMIF_VUL12
#define MT6768_ECHO_REF_MEMIF MT6768_MEMIF_AWB2
#define MT6768_PRIMARY_MEMIF MT6768_MEMIF_DL1
#define MT6768_FAST_MEMIF MT6768_MEMIF_DL1
#define MT6768_DEEP_MEMIF MT6768_MEMIF_DL2
#define MT6768_VOIP_MEMIF MT6768_MEMIF_DL12
#define MT6768_MMAP_DL_MEMIF MT6768_MEMIF_DL3
#define MT6768_MMAP_UL_MEMIF MT6768_MEMIF_VUL
#define MT6768_BARGEIN_MEMIF MT6768_MEMIF_AWB

#if defined(CONFIG_SND_SOC_MTK_AUDIO_DSP)
#define MT6768_DSP_PRIMARY_MEMIF MT6768_MEMIF_DL1
#define MT6768_DSP_DEEPBUFFER_MEMIF MT6768_MEMIF_DL3
#define MT6768_DSP_VOIP_MEMIF MT6768_MEMIF_DL3
#define MT6768_DSP_OFFLOAD_MEMIF MT6768_MEMIF_DL12
#define MT6768_DSP_CAPTURE_UL1_MEMIF MT6768_MEMIF_VUL12
#define MT6768_DSP_REF_MEMIF MT6768_MEMIF_AWB2
#define MT6768_DSP_PLAYBACKDL_MEMIF MT6768_MEMIF_DL4
#define MT6768_DSP_PLAYBACKUL_MEMIF MT6768_MEMIF_VUL4
#endif

enum {
	MT6768_IRQ_0,
	MT6768_IRQ_1,
	MT6768_IRQ_2,
	MT6768_IRQ_3,
	MT6768_IRQ_4,
	MT6768_IRQ_5,
	MT6768_IRQ_6,
	MT6768_IRQ_7,
	MT6768_IRQ_11,
	MT6768_IRQ_12,
	MT6768_IRQ_NUM,
};

enum {
	MTKAIF_PROTOCOL_1 = 0,
	MTKAIF_PROTOCOL_2,
	MTKAIF_PROTOCOL_2_CLK_P2,
};

enum {
	MTK_AFE_ADDA_DL_GAIN_MUTE = 0,
	/* SA suggest apply -0.3db to audio/speech path */
	MTK_AFE_ADDA_DL_GAIN_NORMAL = 0xf74f,
};

/* MCLK */
enum {
	MT6768_I2S0_MCK = 0,
	MT6768_I2S1_MCK,
	MT6768_I2S2_MCK,
	MT6768_I2S3_MCK,
	MT6768_MCK_NUM,
};

struct snd_pcm_substream;
struct mtk_base_irq_data;
struct clk;

struct mt6768_afe_private {
	struct clk **clk;
	struct regmap *apmixed;
	struct regmap *topckgen;
	int irq_cnt[MT6768_MEMIF_NUM];
	int stf_positive_gain_db;
	int dram_resource_counter;
	int sgen_mode;
	int sgen_rate;
	int sgen_amplitude;
	/* usb call */
	int usb_call_echo_ref_enable;
	int usb_call_echo_ref_size;
	bool usb_call_echo_ref_reallocate;
	/* deep buffer playback */
	int deep_playback_state;
	/* fast playback */
	int fast_playback_state;
	/* mmap playback */
	int mmap_playback_state;
	/* mmap record */
	int mmap_record_state;
	/* primary playback */
	int primary_playback_state;
	/* voip rx */
	int voip_rx_state;
	/* xrun assert */
	int xrun_assert[MT6768_MEMIF_NUM];

	/* dai */
	bool dai_on[MT6768_DAI_NUM];
	void *dai_priv[MT6768_DAI_NUM];

	/* adda */
	int mtkaif_protocol;
	bool mtkaif_calibration_ok;
	int mtkaif_chosen_phase[4];
	int mtkaif_phase_cycle[4];
	int mtkaif_calibration_num_phase;
	int mtkaif_dmic;

	/* mck */
	int mck_rate[MT6768_MCK_NUM];

	/* speech mixctrl instead property usage */
	int speech_a2m_msg_id;
	int speech_md_status;
	int speech_adsp_status;
	int speech_mic_mute;
	int speech_dl_mute;
	int speech_ul_mute;
	int speech_phone1_md_idx;
	int speech_phone2_md_idx;
	int speech_phone_id;
	int speech_md_epof;
	int speech_bt_sco_wb;
	int speech_shm_init;
	int speech_shm_usip;
	int speech_shm_widx;
};

int mt6768_dai_adda_register(struct mtk_base_afe *afe);
int mt6768_dai_i2s_register(struct mtk_base_afe *afe);
int mt6768_dai_hw_gain_register(struct mtk_base_afe *afe);
int mt6768_dai_pcm_register(struct mtk_base_afe *afe);
int mt6768_dai_tdm_register(struct mtk_base_afe *afe);

int mt6768_dai_hostless_register(struct mtk_base_afe *afe);

int mt6768_add_misc_control(struct snd_soc_component *platform);

int mt6768_set_local_afe(struct mtk_base_afe *afe);

unsigned int mt6768_general_rate_transform(struct device *dev,
					   unsigned int rate);
unsigned int mt6768_rate_transform(struct device *dev,
				   unsigned int rate, int aud_blk);
int mt6768_enable_dc_compensation(bool enable);
int mt6768_set_lch_dc_compensation(int value);
int mt6768_set_rch_dc_compensation(int value);
int mt6768_adda_dl_gain_control(bool mute);

int mt6768_print_register(struct mtk_base_afe *afe);
#endif
