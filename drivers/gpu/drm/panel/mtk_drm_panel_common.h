/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_DRM_PANEL_COMMON_H_
#define _MTK_DRM_PANEL_COMMON_H_

#include "dt-bindings/lcm/mtk_lcm_settings.h"

#if IS_ENABLED(CONFIG_DRM_MEDIATEK_PANEL)
/* function: set backlight
 * input: func: DBI, DPI, DSI, brightness level
 * output: 0 for success; !0 for failed
 */
extern int mtk_drm_gateic_set_backlight(unsigned int level, char func);

/* function: enable backlight
 * input: func: DBI, DPI, DSI,
 *		enable: 1 for enable, 0 for disable
 *		pwm_enable: set backlight by pwm or not
 * output: 0 for success; !0 for failed
 */
extern int mtk_drm_gateic_enable_backlight(char func);

/* function: panel power on
 * input: func: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 */
extern int mtk_drm_gateic_power_on(char func);

/* function: panel power off
 * input: func: DBI, DPI, DSI
 * output: 0 for success; !0 for failed
 */
extern int mtk_drm_gateic_power_off(char func);

#else
__attribute__ ((weak)) int mtk_drm_gateic_set_backlight(unsigned int level, char func)
{
	return 0;
};

__attribute__ ((weak)) int mtk_drm_gateic_enable_backlight(char func)
{
	return 0;
};
__attribute__ ((weak)) int mtk_drm_gateic_power_on(char func)
{
	return 0;
};

__attribute__ ((weak)) int mtk_drm_gateic_power_off(char func)
{
	return 0;
};
#endif

#endif
