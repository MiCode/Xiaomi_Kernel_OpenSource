/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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

#ifndef __MSM8952_SLIMBUS_AUDIO
#define __MSM8952_SLIMBUS_AUDIO

#include <dsp/apr_audio-v2.h>

struct ext_intf_cfg {
	atomic_t quat_mi2s_clk_ref;
	atomic_t quin_mi2s_clk_ref;
	atomic_t auxpcm_mi2s_clk_ref;
};

enum {
	PRIM_MI2S = 0,
	SEC_MI2S,
	TERT_MI2S,
	QUAT_MI2S,
	QUIN_MI2S,
	MI2S_MAX,
};

struct msm8952_codec {
	void* (*get_afe_config_fn)(struct snd_soc_codec *codec,
			enum afe_config_type config_type);
};

struct msm8952_asoc_mach_data {
	int ext_pa;
	int us_euro_gpio;
	struct delayed_work hs_detect_dwork;
	struct snd_soc_codec *codec;
	struct msm8952_codec msm8952_codec_fn;
	struct ext_intf_cfg clk_ref;
	struct snd_info_entry *codec_root;
	void __iomem *vaddr_gpio_mux_spkr_ctl;
	void __iomem *vaddr_gpio_mux_mic_ctl;
	void __iomem *vaddr_gpio_mux_pcm_ctl;
	void __iomem *vaddr_gpio_mux_quin_ctl;
	void __iomem *vaddr_gpio_mux_qui_pcm_sec_mode_ctl;
	void __iomem *vaddr_gpio_mux_mic_ext_clk_ctl;
	void __iomem *vaddr_gpio_mux_sec_tlmm_ctl;
	struct device_node *us_euro_gpio_p;
	struct device_node *mi2s_gpio_p[MI2S_MAX];
};

int msm_slim_0_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params);
int msm_slim_0_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params);
int msm_slim_1_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params);
int msm_slim_2_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params);
int msm_slim_4_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params);
int msm_slim_4_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params);
int msm_slim_5_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params);
int msm_slim_6_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params);
int msm_slim_5_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params);
int msm_snd_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params);
int msm8952_slimbus_2_hw_params(struct snd_pcm_substream *substream,
					  struct snd_pcm_hw_params *params);
int msm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params);
int msm_quin_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params);
int msm_btsco_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params);
int msm_proxy_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params);
int msm_proxy_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params);
int msm_tdm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			       struct snd_pcm_hw_params *params);
int msm_audrx_init(struct snd_soc_pcm_runtime *rtd);
int msm_mi2s_snd_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params);
int msm_snd_cpe_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params);
int msm_tdm_snd_hw_params(struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params);
int msm_quat_mi2s_snd_startup(struct snd_pcm_substream *substream);
void msm_quat_mi2s_snd_shutdown(struct snd_pcm_substream *substream);

int msm_quin_mi2s_snd_startup(struct snd_pcm_substream *substream);
void msm_quin_mi2s_snd_shutdown(struct snd_pcm_substream *substream);

int msm_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params);
int msm_prim_auxpcm_startup(struct snd_pcm_substream *substream);
void msm_prim_auxpcm_shutdown(struct snd_pcm_substream *substream);

int msm_tdm_startup(struct snd_pcm_substream *substream);
void msm_tdm_shutdown(struct snd_pcm_substream *substream);

struct snd_soc_card *populate_snd_card_dailinks(struct device *dev);
int msm_slim_4_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params);
int msm895x_wsa881x_init(struct snd_soc_component *component);
int msm8952_init_wsa_dev(struct platform_device *pdev,
		struct snd_soc_card *card);
void msm895x_free_auxdev_mem(struct platform_device *pdev);
#endif
