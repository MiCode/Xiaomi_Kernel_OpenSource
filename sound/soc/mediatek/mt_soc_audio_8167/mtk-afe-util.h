/*
 * mtk-afe-util.h  --  Mediatek audio utility
 *
 * Copyright (c) 2016 MediaTek Inc.
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

#ifndef _MTK_AFE_UTILITY_H_
#define _MTK_AFE_UTILITY_H_

struct mtk_afe;

int mtk_afe_enable_top_cg(struct mtk_afe *afe, unsigned int cg_type);

int mtk_afe_disable_top_cg(struct mtk_afe *afe, unsigned int cg_type);

int mtk_afe_enable_main_clk(struct mtk_afe *afe);

int mtk_afe_disable_main_clk(struct mtk_afe *afe);

int mtk_afe_emi_clk_on(struct mtk_afe *afe);

int mtk_afe_emi_clk_off(struct mtk_afe *afe);

int mtk_afe_enable_afe_on(struct mtk_afe *afe);

int mtk_afe_disable_afe_on(struct mtk_afe *afe);

#endif
