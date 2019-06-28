/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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
#ifndef __WCD937X_MBHC_H__
#define __WCD937X_MBHC_H__
#include "../wcd-mbhc-v2.h"

struct wcd937x_mbhc {
	struct wcd_mbhc wcd_mbhc;
	struct blocking_notifier_head notifier;
	struct fw_info *fw_data;
};

#if IS_ENABLED(CONFIG_SND_SOC_WCD937X)
extern int wcd937x_mbhc_init(struct wcd937x_mbhc **mbhc,
			   struct snd_soc_codec *codec,
			   struct fw_info *fw_data);
extern void wcd937x_mbhc_hs_detect_exit(struct snd_soc_codec *codec);
extern int wcd937x_mbhc_hs_detect(struct snd_soc_codec *codec,
				struct wcd_mbhc_config *mbhc_cfg);
extern void wcd937x_mbhc_deinit(struct snd_soc_codec *codec);
extern int wcd937x_mbhc_post_ssr_init(struct wcd937x_mbhc *mbhc,
				    struct snd_soc_codec *codec);
extern void wcd937x_mbhc_ssr_down(struct wcd937x_mbhc *mbhc,
				    struct snd_soc_codec *codec);
extern int wcd937x_mbhc_get_impedance(struct wcd937x_mbhc *wcd937x_mbhc,
				    uint32_t *zl, uint32_t *zr);
#else
static inline int wcd937x_mbhc_init(struct wcd937x_mbhc **mbhc,
				  struct snd_soc_codec *codec,
				  struct fw_info *fw_data)
{
	return 0;
}
static inline void wcd937x_mbhc_hs_detect_exit(struct snd_soc_codec *codec)
{
}
static inline int wcd937x_mbhc_hs_detect(struct snd_soc_codec *codec,
				       struct wcd_mbhc_config *mbhc_cfg)
{
		return 0;
}
static inline void wcd937x_mbhc_deinit(struct snd_soc_codec *codec)
{
}
static inline int wcd937x_mbhc_post_ssr_init(struct wcd937x_mbhc *mbhc,
					   struct snd_soc_codec *codec)
{
	return 0;
}
static inline void wcd937x_mbhc_ssr_down(struct wcd937x_mbhc *mbhc,
					   struct snd_soc_codec *codec)
{
}
static inline int wcd937x_mbhc_get_impedance(struct wcd937x_mbhc *wcd937x_mbhc,
					   uint32_t *zl, uint32_t *zr)
{
	if (zl)
		*zl = 0;
	if (zr)
		*zr = 0;
	return -EINVAL;
}
#endif

#endif /* __WCD937X_MBHC_H__ */
