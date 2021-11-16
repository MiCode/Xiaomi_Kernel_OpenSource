/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
 */
#ifndef __WCD_MBHC_LEGACY_H__
#define __WCD_MBHC_LEGACY_H__

#include <asoc/wcdcal-hwdep.h>
#include <asoc/wcd-mbhc-v2.h>

#if IS_ENABLED(CONFIG_SND_SOC_WCD_MBHC_LEGACY)
void wcd_mbhc_legacy_init(struct wcd_mbhc *mbhc);
#else
static inline void wcd_mbhc_legacy_init(struct wcd_mbhc *mbhc)
{
}
#endif

#endif /* __WCD_MBHC_LEGACY_H__ */
