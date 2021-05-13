/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */


#ifndef __MML_DRIVER_H__
#define __MML_DRIVER_H__

#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

struct mml_dev {
	struct platform_device *pdev;
	/* struct mml_comp *comp[MML_ENGINE_TOTAL]; */
	struct cmdq_base *cmdq_base;
	struct cmdq_client *cmdq_clt;
};

#endif	/* __MML_DRIVER_H__ */
