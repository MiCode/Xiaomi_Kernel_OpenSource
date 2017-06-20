/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

enum codecs {
	TOMTOM_CODEC,
	TASHA_CODEC,
	MAX_CODECS,
};

struct ext_intf_cfg {
	atomic_t quat_mi2s_clk_ref;
	atomic_t quin_mi2s_clk_ref;
	atomic_t auxpcm_mi2s_clk_ref;
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
