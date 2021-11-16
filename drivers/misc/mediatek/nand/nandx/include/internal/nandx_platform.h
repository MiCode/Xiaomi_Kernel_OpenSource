/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#ifndef __NANDX_PLATFORM_H__
#define __NANDX_PLATFORM_H__

#include "nandx_util.h"
#include "nandx_info.h"

#ifdef MT8167
#define NANDX_PLATFORM	NANDX_MT8167
#else
#define NANDX_PLATFORM	NANDX_NONE
#endif

static inline enum IC_VER nandx_get_chip_version(void)
{
	if (NANDX_PLATFORM == NANDX_NONE)
		NANDX_ASSERT(0);

	return NANDX_PLATFORM;
}

static inline struct nfc_resource *nandx_get_nfc_resource(enum IC_VER ver)
{
	extern struct nfc_resource nandx_resource[];

	if (ver == NANDX_MT8167)
		return &nandx_resource[NANDX_MT8167];

	NANDX_ASSERT(0);

	return NULL;
}

void nandx_platform_enable_clock(struct platform_data *data,
				 bool high_speed_en, bool ecc_clk_en);
void nandx_platform_disable_clock(struct platform_data *data,
				  bool high_speed_en, bool ecc_clk_en);
void nandx_platform_prepare_clock(struct platform_data *data,
				  bool high_speed_en, bool ecc_clk_en);
void nandx_platform_unprepare_clock(struct platform_data *data,
				    bool high_speed_en, bool ecc_clk_en);
int nandx_platform_init(struct platform_data *pdata);
int nandx_platform_power_on(struct platform_data *pdata);
int nandx_platform_power_down(struct platform_data *pdata);

#endif				/* __NANDX_PLATFORM_H__ */
