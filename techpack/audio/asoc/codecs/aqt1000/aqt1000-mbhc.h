/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 */
#ifndef __AQT1000_MBHC_H__
#define __AQT1000_MBHC_H__
#include <asoc/wcd-mbhc-v2.h>

struct aqt1000_mbhc {
	struct wcd_mbhc wcd_mbhc;
	struct blocking_notifier_head notifier;
	struct aqt1000 *aqt;
	struct fw_info *fw_data;
	bool mbhc_started;
};

#if IS_ENABLED(CONFIG_SND_SOC_AQT1000)
extern int aqt_mbhc_init(struct aqt1000_mbhc **mbhc,
			   struct snd_soc_component *component,
			   struct fw_info *fw_data);
extern void aqt_mbhc_hs_detect_exit(struct snd_soc_component *component);
extern int aqt_mbhc_hs_detect(struct snd_soc_component *component,
				struct wcd_mbhc_config *mbhc_cfg);
extern void aqt_mbhc_deinit(struct snd_soc_component *component);
extern int aqt_mbhc_post_ssr_init(struct aqt1000_mbhc *mbhc,
				    struct snd_soc_component *component);
extern int aqt_mbhc_get_impedance(struct aqt1000_mbhc *aqt_mbhc,
				    uint32_t *zl, uint32_t *zr);
#else
static inline int aqt_mbhc_init(struct aqt1000_mbhc **mbhc,
				  struct snd_soc_component *component,
				  struct fw_info *fw_data)
{
	return 0;
}
static inline void aqt_mbhc_hs_detect_exit(struct snd_soc_component *component)
{
}
static inline int aqt_mbhc_hs_detect(struct snd_soc_component *component,
				       struct wcd_mbhc_config *mbhc_cfg)
{
		return 0;
}
static inline void aqt_mbhc_deinit(struct snd_soc_component *component)
{
}
static inline int aqt_mbhc_post_ssr_init(struct aqt1000_mbhc *mbhc,
					   struct snd_soc_component *component)
{
	return 0;
}

static inline int aqt_mbhc_get_impedance(struct aqt1000_mbhc *aqt_mbhc,
					   uint32_t *zl, uint32_t *zr)
{
	if (zl)
		*zl = 0;
	if (zr)
		*zr = 0;
	return -EINVAL;
}
#endif

#endif /* __AQT1000_MBHC_H__ */
