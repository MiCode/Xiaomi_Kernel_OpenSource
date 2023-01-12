/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
 */
#ifndef __WCD_MBHC_V2_API_H__
#define __WCD_MBHC_V2_API_H__

#include "wcd-mbhc-v2.h"

#if IS_ENABLED(CONFIG_SND_SOC_WCD_MBHC)
int wcd_mbhc_start(struct wcd_mbhc *mbhc,
		       struct wcd_mbhc_config *mbhc_cfg);
void wcd_mbhc_stop(struct wcd_mbhc *mbhc);
int wcd_mbhc_init(struct wcd_mbhc *mbhc, struct snd_soc_component *component,
		      const struct wcd_mbhc_cb *mbhc_cb,
		      const struct wcd_mbhc_intr *mbhc_cdc_intr_ids,
		      struct wcd_mbhc_register *wcd_mbhc_regs,
		      bool impedance_det_en);
int wcd_mbhc_get_impedance(struct wcd_mbhc *mbhc, uint32_t *zl,
			   uint32_t *zr);
void wcd_mbhc_deinit(struct wcd_mbhc *mbhc);

#else
static inline void wcd_mbhc_stop(struct wcd_mbhc *mbhc)
{
}
int wcd_mbhc_init(struct wcd_mbhc *mbhc, struct snd_soc_component *component,
		      const struct wcd_mbhc_cb *mbhc_cb,
		      const struct wcd_mbhc_intr *mbhc_cdc_intr_ids,
		      struct wcd_mbhc_register *wcd_mbhc_regs,
		      bool impedance_det_en)
{
	return 0;
}
static inline int wcd_mbhc_start(struct wcd_mbhc *mbhc,
				 struct wcd_mbhc_config *mbhc_cfg)
{
	return 0;
}
static inline int wcd_mbhc_get_impedance(struct wcd_mbhc *mbhc,
					 uint32_t *zl,
					 uint32_t *zr)
{
	*zl = 0;
	*zr = 0;
	return -EINVAL;
}
static inline void wcd_mbhc_deinit(struct wcd_mbhc *mbhc)
{
}
#endif

#endif /* __WCD_MBHC_V2_API_H__ */
