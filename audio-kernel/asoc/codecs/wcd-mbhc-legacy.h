/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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
#ifndef __WCD_MBHC_LEGACY_H__
#define __WCD_MBHC_LEGACY_H__

#include "wcdcal-hwdep.h"
#include "wcd-mbhc-v2.h"

#if IS_ENABLED(CONFIG_SND_SOC_WCD_MBHC_LEGACY)
void wcd_mbhc_legacy_init(struct wcd_mbhc *mbhc);
#else
static inline void wcd_mbhc_legacy_init(struct wcd_mbhc *mbhc)
{
}
#endif

#endif /* __WCD_MBHC_LEGACY_H__ */
