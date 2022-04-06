/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_6789_H
#define MTK_DRM_6789_H

#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#ifndef DRM_CMDQ_DISABLE
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#else
#include "../mtk-cmdq-ext.h"
#endif

#include "../mtk_drm_ddp.h"
#include "../mtk_drm_crtc.h"
#include "../mtk_drm_drv.h"
#include "../mtk_drm_ddp_comp.h"
#include "../mtk_dump.h"
#include "../mtk_disp_ovl.h"
#include "../mtk_disp_wdma.h"
#include "../mtk_mipi_tx.h"
#include "../mtk_disp_dsc.h"
#include "../mtk_disp_postmask.h"
#include "../mtk_disp_rdma.h"
#include "../mtk_disp_rsz.h"
#include "../mtk_disp_aal.h"
#include "../mtk_disp_ccorr.h"
#include "../mtk_disp_color.h"
#include "../mtk_disp_dither.h"
#include "../mtk_dsi.h"

// mtk_drm_ddp
#define MT6789_DISP_OVL0_MOUT_EN 0xf04

#define MT6789_DISP_RDMA2_RSZ0_RSZ1_SOUT_SEL  0xF08
	#define MT6789_RSZ0_SOUT_TO_DISP_OVL0_2L     (0)
	#define MT6789_RSZ0_SOUT_TO_DISP_OVL0        (1)
#define MT6789_DISP_REG_CONFIG_DISP_RDMA0_RSZ0_SOUT_SEL 0xF0C
	#define MT6789_SOUT_TO_DISP_DSI0_SEL     (0)
	#define MT6789_RDMA0_SOUT_TO_DISP_COLOR0     (1)
#define MT6789_DISP_REG_CONFIG_DISP_TOVL0_OUT0_MOUT_EN 0xF14
	#define MT6789_OVL0_2L_MOUT_TO_DISP_RDMA0_SEL     BIT(0)
	#define MT6789_OVL0_2L_MOUT_TO_DISP_RSZ0_SEL     BIT(1)
	#define MT6789_OVL0_2L_MOUT_TO_DISP_WDMA0_SEL     BIT(2)
#define MT6789_DISP_REG_CONFIG_DISP_TOVL0_OUT1_MOUT_EN    0xF18
	#define MT6789_OVL0_MOUT_TO_DISP_RDMA0_SEL    BIT(0)
	#define MT6789_OVL0_MOUT_TO_DISP_RSZ0_SEL    BIT(1)
	#define MT6789_OVL0_MOUT_TO_DISP_WDMA0_SEL    BIT(2)
#define MT6789_DISP_REG_CONFIG_DISP_RSZ0_MOUT_EN     0xF1C
	#define MT6789_RSZ0_MOUT_TO_DISP_RDMA0_SEL              BIT(0)
	#define MT6789_RSZ0_MOUT_TO_DISP_WDMA0_SEL              BIT(1)
	#define MT6789_RSZ0_MOUT_TO_DISP_RDMA2_RSZ0_RSZ1_SOUT  BIT(2)
#define MT6789_DISP_REG_CONFIG_DISP_DITHER0_MOUT_EN 0xF20
	#define MT6789_DITHER0_MOUT_TO_DISP_DSI0_SEL     BIT(0)
	#define MT6789_DITHER0_MOUT_TO_DISP_DISP_WDMA0       BIT(1)
	#define MT6789_DITHER0_MOUT_TO_DISP_DISP_DSC_WRAP0       BIT(2)
#define MT6789_DISP_REG_CONFIG_DISP_DSC_WRAP0_MOUT_EN	0xF38
	#define MT6789_DISP_DSC_WRAP0_TO_DISP_WDMA0_SEL	BIT(0)
	#define MT6789_DISP_DSC_WRAP0_TO_DISP_DSI0_SEL	BIT(1)

#define MT6789_DISP_REG_CONFIG_DISP_RSZ0_SEL_IN 0xF24
	#define MT6789_RSZ0_FROM_DISP_OVL0_2L     (0)
	#define MT6789_RSZ0_FROM_DISP_OVL0        (1)
#define MT6789_DISP_REG_CONFIG_DISP_RDMA0_SEL_IN 0xF28
	#define MT6789_SEL_IN_RDMA0_FROM_DISP_OVL0     0
	#define MT6789_SEL_IN_RDMA0_FROM_DISP_RSZ0     1
	#define MT6789_SEL_IN_RDMA0_FROM_DISP_OVL0_2L       2
//#define MT6789_DISP_REG_CONFIG_DISP_BYPASS_SPR0_SEL_IN 0xF2C
//	#define MT6789_SEL_IN_FROM_DISP_DITHER0_MOUT     (0)
//	#define MT6789_SEL_IN_FROM_DISP_SPR0     (1)
#define MT6789_DISP_REG_CONFIG_DSI0_SEL_IN 0xF30
	#define MT6789_SEL_IN_FROM_DISP_RDMA0_RSZ0_SOUT   (0)
	#define MT6789_SEL_IN_FROM_DISP_DITHERR0       (1)
	#define MT6789_SEL_IN_FROM_DISP_DSC_WRAP0       (2)
#define MT6789_DISP_REG_CONFIG_DISP_WDMA0_SEL_IN 0xF34
	#define MT6789_WDMA0_SEL_IN_FROM_DISP_DITHER0_MOUT          (0)
	#define MT6789_SEL_IN_FROM_DISP_RSZ0		(1)
	#define MT6789_WDMA0_SEL_IN_FROM_DISP_OVL0_2L	(2)
	#define MT6789_WDMA0_SEL_IN_FROM_DISP_OVL0	(3)
	#define MT6789_WDMA0_SEL_IN_FROM_DISP_DSC_MOUT	(4)

#define MT6789_DISP_MUTEX0_MOD0 0x30
#define MT6789_DISP_MUTEX0_SOF 0x2C

#define MT6789_MUTEX_MOD_DISP_OVL0 BIT(0)
#define MT6789_MUTEX_MOD_DISP_OVL0_2L BIT(1)
#define MT6789_MUTEX_MOD_DISP_RDMA0 BIT(2)
#define MT6789_MUTEX_MOD_DISP_RSZ0 BIT(3)
#define MT6789_MUTEX_MOD_DISP_COLOR0 BIT(4)
#define MT6789_MUTEX_MOD_DISP_CCORR0 BIT(5)
#define MT6789_MUTEX_MOD_DISP_AAL0 BIT(7)
#define MT6789_MUTEX_MOD_DISP_GAMMA0 BIT(8)
#define MT6789_MUTEX_MOD_DISP_POSTMASK0 BIT(9)
#define MT6789_MUTEX_MOD_DISP_DITHER0 BIT(10)
#define MT6789_MUTEX_MOD_DISP_DSC_WRAP0_CORE0 BIT(13)
#define MT6789_MUTEX_MOD_DISP_DSI0 BIT(14)
#define MT6789_MUTEX_MOD_DISP_WDMA0 BIT(15)
#define MT6789_MUTEX_MOD_DISP_PWM0 BIT(16)

#define MT6789_MUTEX_SOF_SINGLE_MODE 0
#define MT6789_MUTEX_SOF_DSI0 1
#define MT6789_MUTEX_EOF_DSI0 (MT6789_MUTEX_SOF_DSI0 << 6)

// overlay
extern const struct mtk_disp_ovl_data mt6789_ovl_driver_data;
// wdma
extern const struct mtk_disp_wdma_data mt6789_wdma_driver_data;
// rdma
extern const struct mtk_disp_rdma_data mt6789_rdma_driver_data;
// dsc
extern const struct mtk_disp_dsc_data mt6789_dsc_driver_data;
// rsz
extern const struct mtk_disp_rsz_data mt6789_rsz_driver_data;
// postmask
extern const struct mtk_disp_postmask_data mt6789_postmask_driver_data;
// aal
extern const struct mtk_disp_aal_data mt6789_aal_driver_data;
// ccorr
extern const struct mtk_disp_ccorr_data mt6789_ccorr_driver_data;
// color
extern const struct mtk_disp_color_data mt6789_color_driver_data;
// dither
extern const struct mtk_disp_dither_data mt6789_dither_driver_data;
// dsi
extern const struct mtk_dsi_driver_data mt6789_dsi_driver_data;
// drv
extern const struct mtk_mmsys_driver_data mt6789_mmsys_driver_data;
// ddp
extern const struct mtk_disp_ddp_data mt6789_ddp_driver_data;
extern const struct mtk_mmsys_reg_data mt6789_mmsys_reg_data;
// mipi_tx
extern const struct mtk_mipitx_data mt6789_mipitx_data;
extern const struct mtk_mipitx_data mt6789_mipitx_cphy_data;

// ddp
int mtk_ddp_mout_en_MT6789(const struct mtk_mmsys_reg_data *data,
			   enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			   unsigned int *addr);
int mtk_ddp_sel_in_MT6789(const struct mtk_mmsys_reg_data *data,
			  enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			  unsigned int *addr);
int mtk_ddp_sout_sel_MT6789(const struct mtk_mmsys_reg_data *data,
			    enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			    unsigned int *addr);
int mtk_ddp_ovl_bg_blend_en_MT6789(const struct mtk_mmsys_reg_data *data,
			   enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			   unsigned int *addr);
void mtk_ddp_insert_dsc_prim_MT6789(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mtk_ddp_remove_dsc_prim_MT6789(struct mtk_drm_crtc *mtk_crtc,
	struct cmdq_pkt *handle);
void mutex_dump_analysis_mt6789(struct mtk_disp_mutex *mutex);
void mmsys_config_dump_analysis_mt6789(void __iomem *config_regs);
#endif /* MTK_DRM_6789_H */
