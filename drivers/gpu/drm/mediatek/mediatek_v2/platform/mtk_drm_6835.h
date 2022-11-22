/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_6835_H
#define MTK_DRM_6835_H

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


#define MT6835_DISP_OVL0_UFOD_SEL                                0xF0C
	#define MT6835_DISP_OVL0_UFOD_FROM_DISP_RSZ0_MAIN_OVL_SOUT   (0)
	#define MT6835_DISP_OVL0_UFOD_FROM_DISP_DMDP_MAIN_OVL_SOUT   (1)

#define MT6835_DISP_RSZ0_SEL                                0xF24
	#define MT6835_DISP_RSZ0_FROM_DISP_OVL1_2L_BLEND_MOUT   (1)
	#define MT6835_DISP_RSZ0_FROM_DISP_OVL0_BLEND_MOUT      (2)
	#define MT6835_DISP_RSZ0_FROM_DISP_OVL1_2L              (4)
	#define MT6835_DISP_RSZ0_FROM_DISP_OVL0                 (5)

#define MT6835_DISP_MAIN_OVL_DISP_WDMA_SEL                                0xF2C
	#define MT6835_DISP_MAIN_OVL_DISP_WDMA_FROM_DISP_RSZ0_MOUT            (0)
	#define MT6835_DISP_MAIN_OVL_DISP_WDMA_FROM_DISP_OVL1_2L_BLEND_MOUT   (2)
	#define MT6835_DISP_MAIN_OVL_DISP_WDMA_FROM_DISP_OVL0_BLEND_MOUT      (3)

#define MT6835_DISP_MAIN_OVL_DISP_UFBC_WDMA_SEL                                0xF30
	#define MT6835_DISP_MAIN_OVL_DISP_UFBC_WDMA_FROM_DISP_RSZ0_MOUT            (0)
	#define MT6835_DISP_MAIN_OVL_DISP_UFBC_WDMA_FROM_DISP_OVL1_2L_BLEND_MOUT   (2)
	#define MT6835_DISP_MAIN_OVL_DISP_UFBC_WDMA_FROM_DISP_OVL0_BLEND_MOUT      (3)

#define MT6835_DISP_MAIN_OVL_DISP_PQ0_SEL                                0xF34
	#define MT6835_DISP_MAIN_OVL_DISP_PQ0_FROM_DISP_RSZ0_MOUT            (0)
	#define MT6835_DISP_MAIN_OVL_DISP_PQ0_FROM_DISP_OVL1_2L_BLEND_MOUT   (2)
	#define MT6835_DISP_MAIN_OVL_DISP_PQ0_FROM_DISP_OVL0_BLEND_MOUT      (3)

#define MT6835_DISP_PQ0_SEL                                   0xF38
	#define MT6835_DISP_PQ0_FROM_DISP_MAIN_OVL_DISP_PQ0_SEL   (1)

#define MT6835_DISP_RDMA0_SEL                            0xF3C
	#define MT6835_DISP_RDMA0_FROM_DISP_DSI0_SEL         (0)
	#define MT6835_DISP_RDMA0_FROM_DISP_RDMA0_POS_MOUT   (1)

#define MT6835_DISP_RDMA0_POS_SEL                                   0xF40
	#define MT6835_DISP_RDMA0_POS_FROM_DISP_RDMA0_POS_MOUT   (0)
	#define MT6835_DISP_RDMA0_POS_FROM_DISP_RDMA0_SOUT       (1)

#define MT6835_DISP_COLOR0_SEL                         0xF48
	#define MT6835_DISP_COLOR0_FROM_DISP_TDSHP0_SOUT   (1)

#define MT6835_DISP_MDP_AAL0_SEL                         0xF4C
	#define MT6835_DISP_MDP_AAL0_FROM_DISP_CCORR0_SOUT   (0)

#define MT6835_DISP_DSC_WRAP0_L_SEL                      0xF68
	#define MT6835_DISP_DSC_WRAP0_L_FROM_DISP_PQ0_SOUT   (0)

#define MT6835_DISP_WDMA0_SEL                                     0xF70
	#define MT6835_DISP_WDMA0_FROM_DISP_MAIN_OVL_DISP_WDMA_SEL    (1)
	#define MT6835_DISP_WDMA0_FROM_DISP_SPR0_VIRTUAL              (2)

#define MT6835_DISP_UFBC_WDMA0_SEL                                         0xF74
	#define MT6835_DISP_UFBC_WDMA0_FROM_DISP_MAIN_OVL_DISP_UFBC_WDMA_SEL   (1)
	#define MT6835_DISP_UFBC_WDMA0_FROM_DISP_SPR0_VIRTUAL                  (2)

#define MT6835_DISP_MAIN0_SEL                          0xF78
	#define MT6835_DISP_MAIN0_FROM_DISP_PQ0_SOUT                   (1)
	#define MT6835_DISP_MAIN0_FROM_DISP_DSC_WRAP0_MOUT             (3)

#define MT6835_DISP_DSI0_SEL                           0xF84
	#define MT6835_DISP_DSI0_FROM_DISP_RDMA0_OUT_RELAY             (0)
	#define MT6835_DISP_DSI0_FROM_DISP_MAIN0_SOUT                  (1)

#define MT6835_DISP_RSZ0_MAIN_OVL_SOUT_SEL                            0xF8C
	#define MT6835_DISP_RSZ0_MAIN_OVL_SOUT_TO_DISP_OVL1_2L_UFOD_SEL   (1)
	#define MT6835_DISP_RSZ0_MAIN_OVL_SOUT_TO_DISP_OVL0_UFOD_SEL      (2)

#define MT6835_DISP_RDMA0_SOUT_SEL                           0xF98
	#define MT6835_DISP_RDMA0_SOUT_TO_DISP_DSI0_SEL          (0)
	#define MT6835_DISP_RDMA0_SOUT_TO_DISP_TDSHP0            (1)

#define MT6835_DISP_TDSHP0_SOUT_SEL                      0xF9C
	#define MT6835_DISP_TDSHP0_SOUT_TO_DISP_COLOR0_SEL   (1)

#define MT6835_DISP_CCORR0_SOUT_SEL                        0xFA4
	#define MT6835_DISP_CCORR0_SOUT_TO_DISP_MDP_AAL0_SEL   (0)

#define MT6835_DISP_PQ0_SOUT_SEL                           0xFAC
	#define MT6835_DISP_PQ0_SOUT_TO_DISP_MAIN0_SEL         (1)

#define MT6835_DISP_MAIN0_SOUT_SEL                          0xFBC
	#define MT6835_DISP_MAIN0_SOUT_TO_DISP_RDMA0_IN_RELAY   (0)
	#define MT6835_DISP_MAIN0_SOUT_TO_DISP_DSI0_SEL         (1)

#define MT6835_DISP_MAIN0_TX_SOUT_SEL                       0xFF8
	#define MT6835_DISP_MAIN0_TX_SOUT_TO_DISP_DSI0          (0)
	#define MT6835_DISP_MAIN0_TX_TO_DISP_DBPI0              (1)

#define MT6835_DISP_OVL0_BLEND_MOUT_EN                                  0xFCC
	#define MT6835_DISP_OVL0_MOUT_TO_DISP_RSZ0_SEL                      BIT(0)
	#define MT6835_DISP_OVL0_MOUT_TO_DISP_MAIN_OVL_DISP_WDMA_SEL        BIT(2)
	#define MT6835_DISP_OVL0_MOUT_TO_DISP_MAIN_OVL_DISP_UFBC_WDMA_SEL   BIT(3)
	#define MT6835_DISP_OVL0_MOUT_TO_DISP_MAIN_OVL_DISP_PQ0_SEL         BIT(4)

#define MT6835_DISP_OVL1_2L_BLEND_MOUT_EN                     0xFD4
	#define MT6835_DISP_OVL1_2L_MOUT_TO_DISP_RSZ0_SEL         BIT(0)
	#define MT6835_DISP_OVL1_2L_MOUT_TO_DISP_WDMA0_SEL        BIT(2)
	#define MT6835_DISP_OVL1_2L_MOUT_TO_DISP_UFBC_WDMA0_SEL   BIT(3)
	#define MT6835_DISP_OVL1_2L_MOUT_TO_DISP_PQ0_SEL          BIT(4)

#define MT6835_DISP_RSZ0_MOUT_EN                           0xFD8
	#define MT6835_DISP_RSZ0_MOUT_TO_DISP_OVL_SOUT         BIT(0)
	#define MT6835_DISP_RSZ0_MOUT_TO_DISP_WDMA0_SEL        BIT(2)
	#define MT6835_DISP_RSZ0_MOUT_TO_DISP_UFBC_WDMA0_SEL   BIT(3)
	#define MT6835_DISP_RSZ0_MOUT_TO_DISP_PQ0_SEL          BIT(4)

#define MT6835_DISP_RDMA0_POS_MOUT_EN                          0xFDC
	#define MT6835_DISP_RDMA0_POS_MOUT_TO_DISP_RDMA0_POS_SEL   BIT(0)
	#define MT6835_DISP_RDMA0_POS_MOUT_TO_DISP_RDMA0_SEL       BIT(1)

#define MT6835_DISP_POSTMASK0_MOUT_EN                        0xFE0
	#define MT6835_DISP_POSTMASK0_MOUT_TO_DISP_DITHER0_SEL   BIT(0)

#define MT6835_DISP_DITHER0_MOUT_EN                       0xFE4
	#define MT6835_DISP_DITHER0_MOUT_TO_DISP_SPR0_VIRTUAL          BIT(0)

#define MT6835_DISP_SPR0_MOUT_EN                              0xFE8
	#define MT6835_DISP_SPR0_MOUT_TO_DISP_WDMA0_SEL        BIT(0)
	#define MT6835_DISP_SPR0_MOUT_TO_DISP_UFBC_WDMA0_SEL   BIT(1)
	#define MT6835_DISP_SPR0_MOUT_TO_DISP_PQ0_SOUT         BIT(2)

#define MT6835_DISP_DSC_WRAP0_MOUT_EN                      0xFF4
	#define MT6835_DISP_DSC_WRAP0_MOUT_TO_DISP_MAIN0_SEL   BIT(0)

#define MT6835_DISP_MUTEX0_MOD0   0x30
#define MT6835_DISP_MUTEX0_SOF    0x2C

#define MT6835_MUTEX_MOD_DISP_OVL0            BIT(0)
#define MT6835_MUTEX_MOD_DISP_OVL1_2L         BIT(2)
#define MT6835_MUTEX_MOD_DISP_RSZ0            BIT(3)
#define MT6835_MUTEX_MOD_DISP_RDMA0           BIT(4)
#define MT6835_MUTEX_MOD_DISP_COLOR0          BIT(7)
#define MT6835_MUTEX_MOD_DISP_CCORR0          BIT(8)
#define MT6835_MUTEX_MOD_DISP_AAL0            BIT(11)
#define MT6835_MUTEX_MOD_DISP_GAMMA0          BIT(12)
#define MT6835_MUTEX_MOD_DISP_POSTMASK0       BIT(13)
#define MT6835_MUTEX_MOD_DISP_DITHER0         BIT(14)
#define MT6835_MUTEX_MOD_DISP_DSI0            BIT(22)
#define MT6835_MUTEX_MOD_DISP_WDMA0           BIT(23)
#define MT6835_MUTEX_MOD_DISP_PWM0           (BIT(9)|BIT(31))

#define MT6835_MUTEX_SOF_SINGLE_MODE 0
#define MT6835_MUTEX_SOF_DSI0 1
#define MT6835_MUTEX_EOF_DSI0 (MT6835_MUTEX_SOF_DSI0 << 6)

// overlay
extern const struct mtk_disp_ovl_data mt6835_ovl_driver_data;
// wdma
extern const struct mtk_disp_wdma_data mt6835_wdma_driver_data;
// rdma
extern const struct mtk_disp_rdma_data mt6835_rdma_driver_data;
// rsz
extern const struct mtk_disp_rsz_data mt6835_rsz_driver_data;
// postmask
extern const struct mtk_disp_postmask_data mt6835_postmask_driver_data;
// aal
extern const struct mtk_disp_aal_data mt6835_aal_driver_data;
// ccorr
extern const struct mtk_disp_ccorr_data mt6835_ccorr_driver_data;
// color
extern const struct mtk_disp_color_data mt6835_color_driver_data;
// dither
extern const struct mtk_disp_dither_data mt6835_dither_driver_data;
// dsi
extern const struct mtk_dsi_driver_data mt6835_dsi_driver_data;
// drv
extern const struct mtk_mmsys_driver_data mt6835_mmsys_driver_data;
// ddp
extern const struct mtk_disp_ddp_data mt6835_ddp_driver_data;
extern const struct mtk_mmsys_reg_data mt6835_mmsys_reg_data;
// mipi_tx
extern const struct mtk_mipitx_data mt6835_mipitx_data;
extern const struct mtk_mipitx_data mt6835_mipitx_cphy_data;

// ddp
int mtk_ddp_mout_en_MT6835(const struct mtk_mmsys_reg_data *data,
			   enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			   unsigned int *addr);
int mtk_ddp_sel_in_MT6835(const struct mtk_mmsys_reg_data *data,
			  enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			  unsigned int *addr);
int mtk_ddp_sout_sel_MT6835(const struct mtk_mmsys_reg_data *data,
			    enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			    unsigned int *addr);
int mtk_ddp_ovl_bg_blend_en_MT6835(const struct mtk_mmsys_reg_data *data,
			   enum mtk_ddp_comp_id cur, enum mtk_ddp_comp_id next,
			   unsigned int *addr);
void mutex_dump_analysis_mt6835(struct mtk_disp_mutex *mutex);
void mmsys_config_dump_analysis_mt6835(void __iomem *config_regs);
#endif /* MTK_DRM_6835_H */
