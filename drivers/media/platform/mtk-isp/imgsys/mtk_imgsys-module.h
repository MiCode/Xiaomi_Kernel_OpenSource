/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_MODULES_H_
#define _MTK_IMGSYS_MODULES_H_

struct mtk_imgsys_dev;

struct module_ops {
	int module_id;
	void (*init)(struct mtk_imgsys_dev *imgsys_dev);
	void (*set)(struct mtk_imgsys_dev *imgsys_dev);
	void (*dump)(struct mtk_imgsys_dev *imgsys_dev, unsigned int engine);
	void (*uninit)(struct mtk_imgsys_dev *imgsys_dev);
};

#endif /* _MTK_IMGSYS_MODULES_H_ */
