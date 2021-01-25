/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#pragma once

#include <linux/bits.h>

struct dbgtop_drm {
	void __iomem *base;

	unsigned int mode_offset;
};

/* DBGTOP_MFG_REG */
#define MTK_DBGTOP_MFG_REG			(0x00000060)
#define MTK_DBGTOP_MFG_REG_KEY			(0x77000000)
#define MTK_DBGTOP_MFG_PWR_ON			(0x00000001)
#define MTK_DBGTOP_MFG_PWR_EN			(0x00000002)
#define MTK_DBGTOP_DFD_TIMEOUT_SHIFT		(0)
#define MTK_DBGTOP_DFD_TIMEOUT_MASK		GENMASK(16, 0)

#define DRMDRM_MODE_OFFSET			(0x00000000)

/* DBGTOP_LATCH_CTL2 */
#define MTK_DBGTOP_LATCH_CTL2_KEY		(0x95000000)
#define MTK_DBGTOP_LATCH_CTL2			(0x00000044)

extern int mtk_dbgtop_dfd_timeout(int value_abnormal, int value_normal);
extern int mtk_dbgtop_mfg_pwr_on(int value);
extern int mtk_dbgtop_mfg_pwr_en(int value);
