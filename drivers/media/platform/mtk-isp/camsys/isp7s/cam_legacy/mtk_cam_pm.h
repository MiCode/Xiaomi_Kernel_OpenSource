/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 *
 */

#ifndef _MTK_CAM_PM_H_
#define _MTK_CAM_PM_H_

enum CAMSYS_LARB_IDX {
	CAMSYS_LARB_13 = 0,
	CAMSYS_LARB_14,
	CAMSYS_LARB_25,
	CAMSYS_LARB_26,
	CAMSYS_LARB_NUM
};

struct mtk_larb {
	struct device *cam_dev;
	struct device *devs[CAMSYS_LARB_NUM];
};

extern struct platform_driver mtk_cam_larb_driver;

static inline struct device *find_larb(struct mtk_larb *larb, int larb_id)
{
	switch (larb_id) {
	case 13:
		return larb->devs[CAMSYS_LARB_13];
	case 14:
		return larb->devs[CAMSYS_LARB_14];
	case 25:
		return larb->devs[CAMSYS_LARB_25];
	case 26:
		return larb->devs[CAMSYS_LARB_26];
	}

	return NULL;
}

#endif /* _MTK_CAM_PM_H_ */
