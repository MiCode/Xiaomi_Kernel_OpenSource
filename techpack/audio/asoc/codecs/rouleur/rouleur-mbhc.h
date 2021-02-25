/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */
#ifndef __ROULEUR_MBHC_H__
#define __ROULEUR_MBHC_H__
#include <asoc/wcd-mbhc-v2.h>

struct rouleur_mbhc {
	struct wcd_mbhc wcd_mbhc;
	struct blocking_notifier_head notifier;
	struct fw_info *fw_data;
};

#if IS_ENABLED(CONFIG_SND_SOC_ROULEUR)
extern int rouleur_mbhc_init(struct rouleur_mbhc **mbhc,
			   struct snd_soc_component *component,
			   struct fw_info *fw_data);
extern void rouleur_mbhc_hs_detect_exit(struct snd_soc_component *component);
extern int rouleur_mbhc_hs_detect(struct snd_soc_component *component,
				struct wcd_mbhc_config *mbhc_cfg);
extern void rouleur_mbhc_deinit(struct snd_soc_component *component);
extern int rouleur_mbhc_post_ssr_init(struct rouleur_mbhc *mbhc,
				    struct snd_soc_component *component);
extern void rouleur_mbhc_ssr_down(struct rouleur_mbhc *mbhc,
				    struct snd_soc_component *component);
extern int rouleur_mbhc_get_impedance(struct rouleur_mbhc *rouleur_mbhc,
				    uint32_t *zl, uint32_t *zr);
#else
static inline int rouleur_mbhc_init(struct rouleur_mbhc **mbhc,
				  struct snd_soc_component *component,
				  struct fw_info *fw_data)
{
	return 0;
}
static inline void rouleur_mbhc_hs_detect_exit(
			struct snd_soc_component *component)
{
}
static inline int rouleur_mbhc_hs_detect(struct snd_soc_component *component,
				       struct wcd_mbhc_config *mbhc_cfg)
{
		return 0;
}
static inline void rouleur_mbhc_deinit(struct snd_soc_component *component)
{
}
static inline int rouleur_mbhc_post_ssr_init(struct rouleur_mbhc *mbhc,
					   struct snd_soc_component *component)
{
	return 0;
}
static inline void rouleur_mbhc_ssr_down(struct rouleur_mbhc *mbhc,
					   struct snd_soc_component *component)
{
}
static inline int rouleur_mbhc_get_impedance(struct rouleur_mbhc *rouleur_mbhc,
					   uint32_t *zl, uint32_t *zr)
{
	if (zl)
		*zl = 0;
	if (zr)
		*zr = 0;
	return -EINVAL;
}
#endif

#endif /* __ROULEUR_MBHC_H__ */
