/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "../apu.h"
#include "../apu_config.h"
#include "../apu_hw.h"

struct mtk_apu;

int mt6893_rproc_init(struct mtk_apu *apu);
int mt6893_rproc_exit(struct mtk_apu *apu);
int mt6893_rproc_start(struct mtk_apu *apu);
int mt6893_rproc_stop(struct mtk_apu *apu);
int mt6893_apu_memmap_init(struct mtk_apu *apu);
void mt6893_apu_memmap_remove(struct mtk_apu *apu);
void mt6893_rv_cg_gating(struct mtk_apu *apu);
void mt6893_rv_cg_ungating(struct mtk_apu *apu);
void mt6893_rv_cachedump(struct mtk_apu *apu);
