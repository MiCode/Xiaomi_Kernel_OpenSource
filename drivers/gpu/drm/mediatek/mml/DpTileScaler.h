/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __DP_TILE_SCALER_H__
#define __DP_TILE_SCALER_H__

#include <linux/printk.h>
#include <linux/types.h>
#include <linux/bug.h>
#include "mtk-mml-color.h"
#include "mtk-mml-pq-core.h"

#define TILE_SCALER_SUBPIXEL_SHIFT  (20)

#ifndef ASSERT
#define ASSERT(expr) \
	do { \
		if (expr) \
			break; \
		pr_err("MML ASSERT FAILED %s, %d\n", __FILE__, __LINE__); \
		WARN_ON(1); \
	} while (0)
#endif

enum scaler_algo {
	/* Cannot modify these enum definition */
	SCALER_4_TAPS  = 0,
	SCALER_6_TAPS  = 0,
	SCALER_SRC_ACC = 1,	/* n tap */
	SCALER_CUB_ACC = 2,	/* 4n tap */
	SCALER_6N_CUB_ACC = 2,	/* 6n tap */
};

void backward_4_taps(s32 outTileStart,
		     s32 outTileEnd,
		     s32 outMaxEnd,
		     s32 coeffStep,
		     s32 precision,
		     s32 cropOffset,
		     s32 cropFraction,
		     s32 inMaxEnd,
		     s32 inAlignment,
		     s32 *inTileStart,
		     s32 *inTileEnd);

void forward_4_taps(s32 inTileStart,
		    s32 inTileEnd,
		    s32 inMaxEnd,
		    s32 coeffStep,
		    s32 precision,
		    s32 cropOffset,
		    s32 cropSubpixel,
		    s32 outMaxEnd,
		    s32 outAlignment,
		    s32 backOutStart,
		    s32 outCalOrder,
		    s32 *outTileStart,
		    s32 *outTileEnd,
		    s32 *lumaOffset,
		    s32 *lumaSubpixel,
		    s32 *chromaOffset,
		    s32 *chromaSubpixel);

void backward_6_taps(s32 outTileStart,
		     s32 outTileEnd,
		     s32 outMaxEnd,
		     s32 coeffStep,
		     s32 precision,
		     s32 cropOffset,
		     s32 cropFraction,
		     s32 inMaxEnd,
		     s32 inAlignment,
		     s32 *inTileStart,
		     s32 *inTileEnd);

void forward_6_taps(s32 inTileStart,
		    s32 inTileEnd,
		    s32 inMaxEnd,
		    s32 coeffStep,
		    s32 precision,
		    s32 cropOffset,
		    s32 cropSubpixel,
		    s32 outMaxEnd,
		    s32 outAlignment,
		    s32 backOutStart,
		    s32 outCalOrder,
		    s32 *outTileStart,
		    s32 *outTileEnd,
		    s32 *lumaOffset,
		    s32 *lumaSubpixel,
		    s32 *chromaOffset,
		    s32 *chromaSubpixel);

void backward_src_acc(s32 outTileStart,
		      s32 outTileEnd,
		      s32 outMaxEnd,
		      s32 coeffStep,
		      s32 precision,
		      s32 cropOffset,
		      s32 cropFraction,
		      s32 inMaxEnd,
		      s32 inAlignment,
		      s32 *inTileStart,
		      s32 *inTileEnd);

void forward_src_acc(s32 inTileStart,
		     s32 inTileEnd,
		     s32 inMaxEnd,
		     s32 coeffStep,
		     s32 precision,
		     s32 cropOffset,
		     s32 cropSubpixel,
		     s32 outMaxEnd,
		     s32 outAlignment,
		     s32 backOutStart,
		     s32 outCalOrder,
		     s32 *outTileStart,
		     s32 *outTileEnd,
		     s32 *lumaOffset,
		     s32 *lumaSubpixel,
		     s32 *chromaOffset,
		     s32 *chromaSubpixel);

void backward_cub_acc(s32 outTileStart,
		      s32 outTileEnd,
		      s32 outMaxEnd,
		      s32 coeffStep,
		      s32 precision,
		      s32 cropOffset,
		      s32 cropFraction,
		      s32 inMaxEnd,
		      s32 inAlignment,
		      s32 *inTileStart,
		      s32 *inTileEnd);

void forward_cub_acc(s32 inTileStart,
		     s32 inTileEnd,
		     s32 inMaxEnd,
		     s32 coeffStep,
		     s32 precision,
		     s32 cropOffset,
		     s32 cropSubpixel,
		     s32 outMaxEnd,
		     s32 outAlignment,
		     s32 backOutStart,
		     s32 outCalOrder,
		     s32 *outTileStart,
		     s32 *outTileEnd,
		     s32 *lumaOffset,
		     s32 *lumaSubpixel,
		     s32 *chromaOffset,
		     s32 *chromaSubpixel);

#endif  // __DP_TILE_SCALER_H__
