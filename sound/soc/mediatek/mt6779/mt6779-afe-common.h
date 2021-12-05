/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Eason Yen <eason.yen@mediatek.com>
 */

#ifndef _MT_6779_AFE_COMMON_H_
#define _MT_6779_AFE_COMMON_H_
#include <sound/soc.h>
#include <linux/list.h>
#include <linux/regmap.h>
#include "mt6779-reg.h"
#include "../common/mtk-base-afe.h"
#include "../common/mtk-sp-common.h"

enum {
	MT6779_MEMIF_DL1,
	MT6779_MEMIF_DL12,
	MT6779_MEMIF_DL2,
	MT6779_MEMIF_DL3,
	MT6779_MEMIF_DL4,
	MT6779_MEMIF_DL5,
	MT6779_MEMIF_DL6,
	MT6779_MEMIF_DL7,
	MT6779_MEMIF_DL8,
	MT6779_MEMIF_DAI,
	MT6779_MEMIF_DAI2,
	MT6779_MEMIF_MOD_DAI,
	MT6779_MEMIF_VUL12,
	MT6779_MEMIF_VUL2,
	MT6779_MEMIF_VUL3,
	MT6779_MEMIF_VUL4,
	MT6779_MEMIF_VUL5,
	MT6779_MEMIF_VUL6,
	MT6779_MEMIF_AWB,
	MT6779_MEMIF_AWB2,
	MT6779_MEMIF_HDMI,
	MT6779_MEMIF_NUM,
	MT6779_DAI_ADDA = MT6779_MEMIF_NUM,
	MT6779_DAI_ADDA_CH34,
	MT6779_DAI_AP_DMIC,
	MT6779_DAI_AP_DMIC_CH34,
	MT6779_DAI_VOW,
	MT6779_DAI_CONNSYS_I2S,
	MT6779_DAI_I2S_0,
	MT6779_DAI_I2S_1,
	MT6779_DAI_I2S_2,
	MT6779_DAI_I2S_3,
	MT6779_DAI_I2S_5,
	MT6779_DAI_HW_GAIN_1,
	MT6779_DAI_HW_GAIN_2,
	MT6779_DAI_SRC_1,
	MT6779_DAI_SRC_2,
	MT6779_DAI_PCM_1,
	MT6779_DAI_PCM_2,
	MT6779_DAI_TDM,
	MT6779_DAI_HOSTLESS_LPBK,
	MT6779_DAI_HOSTLESS_FM,
	MT6779_DAI_HOSTLESS_SPEECH,
	MT6779_DAI_HOSTLESS_SPH_ECHO_REF,
	MT6779_DAI_HOSTLESS_SPK_INIT,
	MT6779_DAI_HOSTLESS_IMPEDANCE,
	MT6779_DAI_HOSTLESS_SRC_1,	/* just an exmpale */
	MT6779_DAI_HOSTLESS_SRC_BARGEIN,
	MT6779_DAI_HOSTLESS_UL1,
	MT6779_DAI_HOSTLESS_UL2,
	MT6779_DAI_HOSTLESS_UL3,
	MT6779_DAI_HOSTLESS_UL6,
	MT6779_DAI_HOSTLESS_DSP_DL,
	MT6779_DAI_NUM,
};

#define MT6779_DAI_I2S_MAX_NUM 5 //depends each platform's max i2s num
#define MT6779_RECORD_MEMIF MT6779_MEMIF_VUL12
#define MT6779_ECHO_REF_MEMIF MT6779_MEMIF_AWB
#define MT6779_PRIMARY_MEMIF MT6779_MEMIF_DL1
#define MT6779_FAST_MEMIF MT6779_MEMIF_DL2
#define MT6779_DEEP_MEMIF MT6779_MEMIF_DL3
#define MT6779_VOIP_MEMIF MT6779_MEMIF_DL12
#define MT6779_BARGEIN_MEMIF MT6779_MEMIF_AWB

#if defined(CONFIG_SND_SOC_MTK_AUDIO_DSP)
#define MT6779_DSP_PRIMARY_MEMIF MT6779_MEMIF_DL1
#define MT6779_DSP_DEEPBUFFER_MEMIF MT6779_MEMIF_DL3
#define MT6779_DSP_VOIP_MEMIF MT6779_MEMIF_DL12
#define MT6779_DSP_PLAYBACKDL_MEMIF MT6779_MEMIF_DL4
#define MT6779_DSP_PLAYBACKUL_MEMIF MT6779_MEMIF_VUL4
#endif

enum {
	MT6779_IRQ_0,
	MT6779_IRQ_1,
	MT6779_IRQ_2,
	MT6779_IRQ_3,
	MT6779_IRQ_4,
	MT6779_IRQ_5,
	MT6779_IRQ_6,
	MT6779_IRQ_7,
	MT6779_IRQ_8,
	MT6779_IRQ_9,
	MT6779_IRQ_10,
	MT6779_IRQ_11,
	MT6779_IRQ_12,
	MT6779_IRQ_13,
	MT6779_IRQ_14,
	MT6779_IRQ_15,
	MT6779_IRQ_16,
	MT6779_IRQ_17,
	MT6779_IRQ_18,
	MT6779_IRQ_19,
	MT6779_IRQ_20,
	MT6779_IRQ_21,
	MT6779_IRQ_22,
	MT6779_IRQ_23,
	MT6779_IRQ_24,
	MT6779_IRQ_25,
	MT6779_IRQ_26,
	MT6779_IRQ_31,	/* used only for TDM */
	MT6779_IRQ_NUM,
};

enum {
	MTKAIF_PROTOCOL_1 = 0,
	MTKAIF_PROTOCOL_2,
	MTKAIF_PROTOCOL_2_CLK_P2,
};

enum {
	MTK_AFE_ADDA_DL_GAIN_MUTE = 0,
	MTK_AFE_ADDA_DL_GAIN_NORMAL = 0xf74f,
	/* SA suggest apply -0.3db to audio/speech path */
};

/* MCLK */
enum {
	MT6779_I2S0_MCK = 0,
	MT6779_I2S1_MCK,
	MT6779_I2S2_MCK,
	MT6779_I2S3_MCK,
	MT6779_I2S4_MCK,
	MT6779_I2S4_BCK,
	MT6779_I2S5_MCK,
	MT6779_MCK_NUM,
};

struct snd_pcm_substream;
struct mtk_base_irq_data;
struct clk;

struct mt6779_afe_private {
	struct clk **clk;
	struct regmap *topckgen;
	int irq_cnt[MT6779_MEMIF_NUM];
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
	/* primary playback */
	int primary_playback_state;
	/* voip rx */
	int voip_rx_state;
	/* xrun assert */
	int xrun_assert[MT6779_MEMIF_NUM];

	/* dai */
	bool dai_on[MT6779_DAI_NUM];
	void *dai_priv[MT6779_DAI_NUM];

	/* adda */
	int mtkaif_protocol;
	bool mtkaif_calibration_ok;
	int mtkaif_chosen_phase[4];
	int mtkaif_phase_cycle[4];
	int mtkaif_calibration_num_phase;
	int mtkaif_dmic;
	int mtkaif_dmic_ch34;

	/* mck */
	int mck_rate[MT6779_MCK_NUM];

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
	int speech_md_headversion;
	int speech_md_version;
};

int mt6779_dai_adda_register(struct mtk_base_afe *afe);
int mt6779_dai_i2s_register(struct mtk_base_afe *afe);
int mt6779_dai_hw_gain_register(struct mtk_base_afe *afe);
int mt6779_dai_src_register(struct mtk_base_afe *afe);
int mt6779_dai_pcm_register(struct mtk_base_afe *afe);
int mt6779_dai_tdm_register(struct mtk_base_afe *afe);

int mt6779_dai_hostless_register(struct mtk_base_afe *afe);

int mt6779_add_misc_control(struct snd_soc_component *component);

int mt6779_set_local_afe(struct mtk_base_afe *afe);

unsigned int mt6779_general_rate_transform(struct device *dev,
					   unsigned int rate);
unsigned int mt6779_rate_transform(struct device *dev,
				   unsigned int rate, int aud_blk);
int mt6779_enable_dc_compensation(bool enable);
int mt6779_set_lch_dc_compensation(int value);
int mt6779_set_rch_dc_compensation(int value);
int mt6779_adda_dl_gain_control(bool mute);

int mt6779_dai_set_priv(struct mtk_base_afe *afe, unsigned int id,
			int priv_size, const void *priv_data);
#endif
