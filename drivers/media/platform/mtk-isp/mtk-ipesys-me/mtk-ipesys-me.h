/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_ME_H_
#define _MTK_IMGSYS_ME_H_

#include "mtk_imgsys-dev.h"

struct ipesys_me_clocks {
	struct clk_bulk_data *clks;
	unsigned int clk_num;
};

struct ipesys_me_device {
	void __iomem *regs;
	struct device *dev;
	struct ipesys_me_clocks me_clk;
};

#define ME_CTL_OFFSET      0x0000
#define ME_CTL_RANGE       0xA10
#define ME_CTL_RANGE_TF    0x120


void ipesys_me_set_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void ipesys_me_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine);
void ipesys_me_uninit(struct mtk_imgsys_dev *imgsys_dev);
struct device *ipesys_me_getdev(void);


#endif /* _MTK_IMGSYS_ME_H_ */
