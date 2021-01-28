/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __DBGTOP_H__
#define __DBGTOP_H__

#define MTK_DBGTOP_TEST			0
#define DBGTOP_MAX_CMD_LEN		4

#define MTK_DBGTOP_MODE			(DBGTOP_BASE+0x0000)
#define MTK_DBGTOP_DEBUG_CTL		(DBGTOP_BASE+0x0030)
#define MTK_DBGTOP_DEBUG_CTL2		(DBGTOP_BASE+0x0034)
#define MTK_DBGTOP_LATCH_CTL		(DBGTOP_BASE+0x0040)
#define MTK_DBGTOP_LATCH_CTL2           (DBGTOP_BASE+LATCH_CTL2_OFFSET)
#define MTK_DBGTOP_MFG_REG		(DBGTOP_BASE+0x0060)

/* DBGTOP_MODE */
#define MTK_DBGTOP_MODE_KEY		(0x22000000)
#define MTK_DBGTOP_MODE_DDR_RESERVE	(0x00000001)

/* DBGTOP_DEBUG_CTL */
#define MTK_DBGTOP_DEBUG_CTL_KEY	(0x59000000)
#define MTK_DBGTOP_DVFSRC_PAUSE_PULSE	(0x00080000)
#define MTK_DBGTOP_DVFSRC_SUCECESS_ACK	(0x00800000)

/* DBGTOP_DEBUG_CTL2 */
#define MTK_DBGTOP_DEBUG_CTL2_KEY	(0x55000000)
#define MTK_DBGTOP_DVFSRC_EN		(0x00000200)

/* DBGTOP_LATCH_CTL */
#define MTK_DBGTOP_LATCH_CTL_KEY	(0x95000000)
#define MTK_DBGTOP_DVFSRC_LATCH_EN	(0x00002000)

/* DBGTOP_LATCH_CTL2 */
#define MTK_DBGTOP_LATCH_CTL2_KEY	(0x95000000)
#define MTK_DBGTOP_DFD_EN		(0x00020000) /* (1<<17) */
#define MTK_DBGTOP_DFD_THERM1_DIS	(0x00040000) /* (1<<18) */
#define MTK_DBGTOP_DFD_THERM2_DIS	(0x00080000) /* (1<<19) */

#define MTK_DBGTOP_DFD_TIMEOUT_SHIFT	(0)          /* bit(16:0) */
#define MTK_DBGTOP_DFD_TIMEOUT_MASK \
	(0x1FFFF << MTK_DBGTOP_DFD_TIMEOUT_SHIFT)

/* DBGTOP_MFG_REG */
#define MTK_DBGTOP_MFG_REG_KEY		(0x77000000)
#define MTK_DBGTOP_MFG_PWR_ON		(0x00000001)
#define MTK_DBGTOP_MFG_PWR_EN		(0x00000002)

int mtk_dbgtop_dram_reserved(int enable);
int mtk_dbgtop_cfg_dvfsrc(int enable);
int mtk_dbgtop_pause_dvfsrc(int enable);
int mtk_dbgtop_dfd_count_en(int value);
int mtk_dbgtop_dfd_therm1_dis(int value);
int mtk_dbgtop_dfd_therm2_dis(int value);
int mtk_dbgtop_dfd_timeout(int value);
int mtk_dbgtop_mfg_pwr_on(int value);
int mtk_dbgtop_mfg_pwr_en(int value);
void get_dfd_base(void __iomem *dfd_base, unsigned int latch_offset);

#endif
