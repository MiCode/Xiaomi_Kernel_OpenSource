/*
 * Copyright (C) 2011 MediaTek, Inc.
 *
 * Author: Holmes Chiou <holmes.chiou@mediatek.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MT_FREQHOPPING_H__
#define __MT_FREQHOPPING_H__

#define FHTAG "[FH] "

#define FH_MSG_ERROR(fmt, args...)	pr_err(FHTAG fmt, ##args)
#define FH_MSG_NOTICE(fmt, args...)	pr_notice(FHTAG fmt, ##args)
#define FH_MSG_INFO(fmt, args...)	/* pr_info(FHTAG fmt, ##args) */

#define FH_MIN_PLLID		0u	/* FH_MIN_PLLID = FH_ARMCA7_PLLID is for current design */
#define FH_ARMCA7_PLLID		0u
#define FH_ARMCA15_PLLID	1u
#define FH_MAIN_PLLID		2u	/* hf_faxi_ck = mainpll/4 */
#define FH_MEM_PLLID		3u	/* ?? */
#define FH_MSDC_PLLID		4u	/* hf_fmsdc30_1_ck = MSDCPLL/4 */
#define FH_MM_PLLID		5u	/* hf_fmfg_ck = MMPLL(455MHz) */
#define FH_VENC_PLLID		6u	/* hf_fcci400_ck = VENCPLL */
#define FH_TVD_PLLID		7u	/* hf_dpi0_ck = TVDPLL/2 = 297 */
#define FH_VCODEC_PLLID		8u	/* hf_fvdec_ck = VCODECPLL */
#define FH_LVDS_PLLID		9u
#define FH_MSDC2_PLLID		10u
#define FH_LVDS2_PLLID		11u
#define FH_MAX_PLLID		11u	/* FH_MAX_PLLID = FH_LVDS2_PLLID is for current design */
#define FH_PLL_NUM		12u

struct freqhopping_ssc {
	unsigned int freq;
	unsigned int dt;
	unsigned int df;
	unsigned int upbnd;
	unsigned int lowbnd;
	unsigned int dds;
};

struct mt_fh_hal_driver *mt_get_fh_hal_drv(void);
extern int mtk_fhctl_enable_ssc_by_id(unsigned int fh_pll_id);
extern int mtk_fhctl_disable_ssc_by_id(unsigned int fh_pll_id);
extern int mtk_fhctl_hopping_by_id(unsigned int fh_pll_id, unsigned int target_vco_frequency);

#endif				/* !__MT_FREQHOPPING_H__ */
