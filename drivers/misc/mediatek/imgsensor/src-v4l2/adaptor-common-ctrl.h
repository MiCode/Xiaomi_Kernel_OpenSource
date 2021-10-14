/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2021 MediaTek Inc. */

#ifndef __ADAPTOR_COMMON_CTRL_H__
#define __ADAPTOR_COMMON_CTRL_H__

int g_stagger_info(struct adaptor_ctx *ctx,
				   int scenario,
				   struct mtk_stagger_info *info);

int g_stagger_scenario(struct adaptor_ctx *ctx,
					   int scenario,
					   struct mtk_stagger_target_scenario *info);

int g_max_exposure(struct adaptor_ctx *ctx,
					   int scenario,
					   struct mtk_stagger_max_exp_time *info);

int g_max_exposure_line(struct adaptor_ctx *ctx,
					   int scenario,
					   struct mtk_max_exp_line *info);

#endif
