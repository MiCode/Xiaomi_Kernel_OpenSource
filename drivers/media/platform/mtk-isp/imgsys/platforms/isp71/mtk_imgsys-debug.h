/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Christopher Chen <christopher.chen@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_DEBUG_H_
#define _MTK_IMGSYS_DEBUG_H_

#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-module.h"
#include "mtk_imgsys-engine.h"
/**
 * enum IMGSYS_DL_PATH_E
 *
 * Definition about supported direct link path
 */
enum IMGSYS_DL_PATH_E {
	IMGSYS_DL_WPEE_TRAW = 0,
	IMGSYS_DL_WPEE_DIP,
	IMGSYS_DL_WPET_TRAW,
	IMGSYS_DL_WPET_DIP,
	IMGSYS_DL_WPEL_TRAW,
	IMGSYS_DL_WPE_PQDIP,
	IMGSYS_DL_TRAW_DIP,
	IMGSYS_DL_DIP_PQDIP,
};

struct imgsys_dbg_engine_t {
	enum mtk_imgsys_engine eng_e;
	char eng_name[8];
};

void imgsys_dl_debug_dump(struct mtk_imgsys_dev *imgsys_dev, unsigned int hw_comb);
void imgsys_debug_dump_routine(struct mtk_imgsys_dev *imgsys_dev,
	const struct module_ops *imgsys_modules, int imgsys_module_num,
	unsigned int hw_comb);

#endif /* _MTK_IMGSYS_DEBUG_H_ */
