/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6873-afe-common.h  --  Mediatek 6873 audio driver definitions
 *
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Shane Chien <shane.chien@mediatek.com>
 */

#ifndef _MT_6873_AFE_COMMON_H_
#define _MT_6873_AFE_COMMON_H_
#include <sound/soc.h>
#include <linux/list.h>
#include <linux/regmap.h>
#include <mt-plat/aee.h>
#include "mt6873-reg.h"
#include "../common/mtk-base-afe.h"

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#define AUDIO_AEE(message) \
	(aee_kernel_exception_api(__FILE__, \
				  __LINE__, \
				  DB_OPT_FTRACE, message, \
				  "audio assert"))
#else
#define AUDIO_AEE(message) WARN_ON(true)
#endif

enum {
	MT6873_MEMIF_DL1,
	MT6873_MEMIF_DL12,
	MT6873_MEMIF_DL2,
	MT6873_MEMIF_DL3,
	MT6873_MEMIF_DL4,
	MT6873_MEMIF_DL5,
	MT6873_MEMIF_DL6,
	MT6873_MEMIF_DL7,
	MT6873_MEMIF_DL8,
	MT6873_MEMIF_DL9,
	MT6873_MEMIF_DAI,
	MT6873_MEMIF_DAI2,
	MT6873_MEMIF_MOD_DAI,
	MT6873_MEMIF_VUL12,
	MT6873_MEMIF_VUL2,
	MT6873_MEMIF_VUL3,
	MT6873_MEMIF_VUL4,
	MT6873_MEMIF_VUL5,
	MT6873_MEMIF_VUL6,
	MT6873_MEMIF_AWB,
	MT6873_MEMIF_AWB2,
	MT6873_MEMIF_HDMI,
	MT6873_MEMIF_NUM,
	MT6873_DAI_ADDA = MT6873_MEMIF_NUM,
	MT6873_DAI_ADDA_CH34,
	MT6873_DAI_AP_DMIC,
	MT6873_DAI_AP_DMIC_CH34,
	MT6873_DAI_VOW,
	MT6873_DAI_CONNSYS_I2S,
	MT6873_DAI_I2S_0,
	MT6873_DAI_I2S_1,
	MT6873_DAI_I2S_2,
	MT6873_DAI_I2S_3,
	MT6873_DAI_I2S_5,
	MT6873_DAI_I2S_6,
	MT6873_DAI_I2S_7,
	MT6873_DAI_I2S_8,
	MT6873_DAI_I2S_9,
	MT6873_DAI_HW_GAIN_1,
	MT6873_DAI_HW_GAIN_2,
	MT6873_DAI_SRC_1,
	MT6873_DAI_SRC_2,
	MT6873_DAI_PCM_1,
	MT6873_DAI_PCM_2,
	MT6873_DAI_TDM,
	MT6873_DAI_HOSTLESS_LPBK,
	MT6873_DAI_HOSTLESS_FM,
	MT6873_DAI_HOSTLESS_HW_GAIN_AAUDIO,
	MT6873_DAI_HOSTLESS_SRC_AAUDIO,
	MT6873_DAI_HOSTLESS_SPEECH,
	MT6873_DAI_HOSTLESS_SPH_ECHO_REF,
	MT6873_DAI_HOSTLESS_SPK_INIT,
	MT6873_DAI_HOSTLESS_IMPEDANCE,
	MT6873_DAI_HOSTLESS_SRC_1,	/* just an exmpale */
	MT6873_DAI_HOSTLESS_SRC_BARGEIN,
	MT6873_DAI_HOSTLESS_UL1,
	MT6873_DAI_HOSTLESS_UL2,
	MT6873_DAI_HOSTLESS_UL3,
	MT6873_DAI_HOSTLESS_UL6,
	MT6873_DAI_HOSTLESS_DSP_DL,
	MT6873_DAI_NUM,
};

#define MT6873_RECORD_MEMIF MT6873_MEMIF_VUL12
#define MT6873_ECHO_REF_MEMIF MT6873_MEMIF_AWB
#define MT6873_PRIMARY_MEMIF MT6873_MEMIF_DL1
#define MT6873_FAST_MEMIF MT6873_MEMIF_DL2
#define MT6873_DEEP_MEMIF MT6873_MEMIF_DL3
#define MT6873_VOIP_MEMIF MT6873_MEMIF_DL12
#define MT6873_MMAP_DL_MEMIF MT6873_MEMIF_DL5
#define MT6873_MMAP_UL_MEMIF MT6873_MEMIF_VUL5
#define MT6873_BARGE_IN_MEMIF MT6873_MEMIF_AWB

// adsp define
#define MT6873_DSP_PRIMARY_MEMIF MT6873_MEMIF_DL1
#define MT6873_DSP_DEEPBUFFER_MEMIF MT6873_MEMIF_DL3
#define MT6873_DSP_VOIP_MEMIF MT6873_MEMIF_DL12
#define MT6873_DSP_PLAYBACKDL_MEMIF MT6873_MEMIF_DL4
#define MT6873_DSP_PLAYBACKUL_MEMIF MT6873_MEMIF_VUL4

enum {
	MT6873_IRQ_0,
	MT6873_IRQ_1,
	MT6873_IRQ_2,
	MT6873_IRQ_3,
	MT6873_IRQ_4,
	MT6873_IRQ_5,
	MT6873_IRQ_6,
	MT6873_IRQ_7,
	MT6873_IRQ_8,
	MT6873_IRQ_9,
	MT6873_IRQ_10,
	MT6873_IRQ_11,
	MT6873_IRQ_12,
	MT6873_IRQ_13,
	MT6873_IRQ_14,
	MT6873_IRQ_15,
	MT6873_IRQ_16,
	MT6873_IRQ_17,
	MT6873_IRQ_18,
	MT6873_IRQ_19,
	MT6873_IRQ_20,
	MT6873_IRQ_21,
	MT6873_IRQ_22,
	MT6873_IRQ_23,
	MT6873_IRQ_24,
	MT6873_IRQ_25,
	MT6873_IRQ_26,
	MT6873_IRQ_31,	/* used only for TDM */
	MT6873_IRQ_NUM,
};

/* MCLK */
enum {
	MT6873_I2S0_MCK = 0,
	MT6873_I2S1_MCK,
	MT6873_I2S2_MCK,
	MT6873_I2S3_MCK,
	MT6873_I2S4_MCK,
	MT6873_I2S4_BCK,
	MT6873_I2S5_MCK,
	MT6873_I2S6_MCK,
	MT6873_I2S7_MCK,
	MT6873_I2S8_MCK,
	MT6873_I2S9_MCK,
	MT6873_MCK_NUM,
};

#define MTK_SPK_NOT_SMARTPA_STR "MTK_SPK_NOT_SMARTPA"
#define MTK_SPK_RICHTEK_RT5509_STR "MTK_SPK_RICHTEK_RT5509"
#define MTK_SPK_MEDIATEK_MT6660_STR "MTK_SPK_MEDIATEK_MT6660"
#define MTK_SPK_GOODIX_TFA9874_STR "MTK_SPK_GOODIX_TFA9874"

#define MTK_SPK_I2S_0_STR "MTK_SPK_I2S_0"
#define MTK_SPK_I2S_1_STR "MTK_SPK_I2S_1"
#define MTK_SPK_I2S_2_STR "MTK_SPK_I2S_2"
#define MTK_SPK_I2S_3_STR "MTK_SPK_I2S_3"
#define MTK_SPK_I2S_5_STR "MTK_SPK_I2S_5"
#define MTK_SPK_I2S_6_STR "MTK_SPK_I2S_6"
#define MTK_SPK_I2S_7_STR "MTK_SPK_I2S_7"
#define MTK_SPK_I2S_8_STR "MTK_SPK_I2S_8"
#define MTK_SPK_I2S_9_STR "MTK_SPK_I2S_9"
#define MTK_SPK_TINYCONN_I2S_0_STR "MTK_SPK_TINYCONN_I2S_0"

enum mtk_spk_type {
	MTK_SPK_NOT_SMARTPA = 0,
	MTK_SPK_RICHTEK_RT5509,
	MTK_SPK_MEDIATEK_MT6660,
	MTK_SPK_GOODIX_TFA9874,
	MTK_SPK_TYPE_NUM
};

enum mtk_spk_i2s_type {
	MTK_SPK_I2S_TYPE_INVALID = -1,
	MTK_SPK_I2S_0,
	MTK_SPK_I2S_1,
	MTK_SPK_I2S_2,
	MTK_SPK_I2S_3,
	MTK_SPK_I2S_5,
	MTK_SPK_I2S_TYPE_NUM
};

struct snd_pcm_substream;
struct mtk_base_irq_data;
struct clk;

struct mt6873_afe_private {
	struct clk **clk;
	struct regmap *topckgen;
	struct regmap *apmixed;
	struct regmap *infracfg;
	int irq_cnt[MT6873_MEMIF_NUM];
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
	int xrun_assert[MT6873_MEMIF_NUM];

	/* dai */
	bool dai_on[MT6873_DAI_NUM];
	void *dai_priv[MT6873_DAI_NUM];

	/* adda */
	int mtkaif_protocol;
	int mtkaif_chosen_phase[4];
	int mtkaif_phase_cycle[4];
	int mtkaif_calibration_num_phase;
	int mtkaif_dmic;
	int mtkaif_dmic_ch34;
	int mtkaif_adda6_only;

	/* mck */
	int mck_rate[MT6873_MCK_NUM];

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
	int speech_cust_param_init;
};

int mt6873_dai_adda_register(struct mtk_base_afe *afe);
int mt6873_dai_i2s_register(struct mtk_base_afe *afe);
int mt6873_dai_hw_gain_register(struct mtk_base_afe *afe);
int mt6873_dai_src_register(struct mtk_base_afe *afe);
int mt6873_dai_pcm_register(struct mtk_base_afe *afe);
int mt6873_dai_tdm_register(struct mtk_base_afe *afe);

int mt6873_dai_hostless_register(struct mtk_base_afe *afe);

int mt6873_add_misc_control(struct snd_soc_component *component);

int mt6873_set_local_afe(struct mtk_base_afe *afe);

unsigned int mt6873_general_rate_transform(struct device *dev,
					   unsigned int rate);
unsigned int mt6873_rate_transform(struct device *dev,
				   unsigned int rate, int aud_blk);
int mt6873_dai_set_priv(struct mtk_base_afe *afe, int id,
			int priv_size, const void *priv_data);

int mt6873_enable_dc_compensation(bool enable);
int mt6873_set_lch_dc_compensation(int value);
int mt6873_set_rch_dc_compensation(int value);
int mt6873_adda_dl_gain_control(bool mute);

#endif
