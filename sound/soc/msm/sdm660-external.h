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

#ifndef __SDM660_EXTERNAL
#define __SDM660_EXTERNAL

int msm_snd_hw_params(struct snd_pcm_substream *substream,
		      struct snd_pcm_hw_params *params);
int msm_ext_slimbus_2_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params);
int msm_btsco_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				 struct snd_pcm_hw_params *params);
int msm_proxy_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				    struct snd_pcm_hw_params *params);
int msm_proxy_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				    struct snd_pcm_hw_params *params);
int msm_audrx_init(struct snd_soc_pcm_runtime *rtd);
int msm_snd_cpe_hw_params(struct snd_pcm_substream *substream,
			  struct snd_pcm_hw_params *params);
struct snd_soc_card *populate_snd_card_dailinks(struct device *dev,
						int snd_card_val);
int msm_ext_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
			       struct snd_pcm_hw_params *params);
#ifdef CONFIG_SND_SOC_EXT_CODEC
int msm_ext_cdc_init(struct platform_device *, struct msm_asoc_mach_data *,
		     struct snd_soc_card **, struct wcd_mbhc_config *);
void msm_ext_register_audio_notifier(struct platform_device *pdev);
void msm_ext_cdc_deinit(void);
#else
inline int msm_ext_cdc_init(struct platform_device *pdev,
			    struct msm_asoc_mach_data *pdata,
			    struct snd_soc_card **card,
			    struct wcd_mbhc_config *wcd_mbhc_cfg_ptr1)
{
	return 0;
}

inline void msm_ext_register_audio_notifier(struct platform_device *pdev)
{
}
inline void msm_ext_cdc_deinit(void)
{
}
#endif
#endif
