/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *       Tiffany Lin <tiffany.lin@mediatek.com>
 */
#ifndef _VCODEC_PM_CODEC_H_
#define _VCODEC_PM_CODEC_H_

#include "mtk_vcodec_drv.h"

int mtk_vcodec_init_pm(struct mtk_vcodec_dev *dev);
void mtk_vcodec_clock_on(enum mtk_instance_type type, struct mtk_vcodec_dev *dev);
void mtk_vcodec_clock_off(enum mtk_instance_type type, struct mtk_vcodec_dev *dev);
#endif
