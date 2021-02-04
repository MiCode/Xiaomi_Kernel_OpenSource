/*
 * mt2701-afe-clock-ctrl.h  --  Mediatek 2701 afe clock ctrl definition
 *
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Garlic Tseng <garlic.tseng@mediatek.com>
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

#ifndef _MT2701_AFE_CLOCK_CTRL_H_
#define _MT2701_AFE_CLOCK_CTRL_H_

struct mtk_base_afe;

int mt2701_init_clock(struct mtk_base_afe *afe);
int mt2701_afe_enable_clock(struct mtk_base_afe *afe);
void mt2701_afe_disable_clock(struct mtk_base_afe *afe);

int mt2701_turn_on_a1sys_clock(struct mtk_base_afe *afe);
void mt2701_turn_off_a1sys_clock(struct mtk_base_afe *afe);

int mt2701_turn_on_a2sys_clock(struct mtk_base_afe *afe);
void mt2701_turn_off_a2sys_clock(struct mtk_base_afe *afe);

int mt2701_turn_on_afe_clock(struct mtk_base_afe *afe);
void mt2701_turn_off_afe_clock(struct mtk_base_afe *afe);

void mt2701_mclk_configuration(struct mtk_base_afe *afe, int id, int domain,
			       unsigned int mclk);
int mt2701_turn_on_mclk(struct mtk_base_afe *afe, int id);
void mt2701_turn_off_mclk(struct mtk_base_afe *afe, int id);

int mt2712_init_clock(struct mtk_base_afe *afe);
int mt2712_afe_enable_clock(struct mtk_base_afe *afe);
void mt2712_afe_disable_clock(struct mtk_base_afe *afe);

int mt2712_turn_on_a1sys_clock(struct mtk_base_afe *afe);
void mt2712_turn_off_a1sys_clock(struct mtk_base_afe *afe);

int mt2712_turn_on_a2sys_clock(struct mtk_base_afe *afe);
void mt2712_turn_off_a2sys_clock(struct mtk_base_afe *afe);

int mt2712_turn_on_asrc_clock(struct mtk_base_afe *afe);
void mt2712_turn_off_asrc_clock(struct mtk_base_afe *afe);

int mt2712_turn_on_afe_clock(struct mtk_base_afe *afe);
void mt2712_turn_off_afe_clock(struct mtk_base_afe *afe);

void mt2712_mclk_configuration(struct mtk_base_afe *afe, int id, int domain,
			       unsigned int mclk);
int mt2712_turn_on_mclk(struct mtk_base_afe *afe, int id);
void mt2712_turn_off_mclk(struct mtk_base_afe *afe, int id);
int mt2712_turn_on_tdm_clock(struct mtk_base_afe *afe);
void mt2712_turn_off_tdm_clock(struct mtk_base_afe *afe);

int mt2701_tdm_clk_configuration(struct mtk_base_afe *afe, int tdm_id,
				  struct snd_pcm_hw_params *params, bool coclk);

#endif
