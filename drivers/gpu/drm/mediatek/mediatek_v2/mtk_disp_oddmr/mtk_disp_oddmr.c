// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/ratelimit.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/uaccess.h>
#include <drm/mediatek_drm.h>

#include "../mtk_drm_crtc.h"
#include "../mtk_drm_ddp_comp.h"
#include "../mtk_drm_drv.h"
#include "../mtk_drm_lowpower.h"
#include "../mtk_log.h"
#include "../mtk_dump.h"
#include "../mtk_drm_mmp.h"
#include "../mtk_drm_gem.h"
#include "../mtk_drm_fb.h"
#include "mtk_disp_oddmr.h"
#include "mtk_disp_oddmr_parse_data.h"
#include "mtk_disp_oddmr_tuning.h"

/* ODDMR TOP */
#define DISP_ODDMR_TOP_CTR_2 0x0008
#define REG_VS_RE_GEN REG_FLD_MSB_LSB(1, 1)
#define DISP_ODDMR_TOP_CTR_3 0x000C
#define REG_ODDMR_TOP_CLK_FORCE_EN REG_FLD_MSB_LSB(15, 15)
#define REG_FORCE_COMMIT REG_FLD_MSB_LSB(4, 4)
#define REG_BYPASS_SHADOW REG_FLD_MSB_LSB(5, 5)
#define DISP_ODDMR_TOP_CTR_4 0x001C//rst
#define DISP_ODDMR_IRQ_FORCE 0x0020
#define DISP_ODDMR_IRQ_CLEAR 0x0024
#define DISP_ODDMR_IRQ_MASK 0x0028
#define DISP_ODDMR_IRQ_STATUS 0x002C
#define DISP_ODDMR_IRQ_FRAME_DONE BIT(0)
#define DISP_ODDMR_IRQ_ODW_DONE BIT(1)
#define DISP_ODDMR_IRQ_I_FRM_DNE BIT(2)
#define DISP_ODDMR_IRQ_O_FRM_DNE BIT(3)
#define DISP_ODDMR_IRQ_SOF BIT(5)
#define DISP_ODDMR_IRQ_RAW_STATUS 0x0030
#define DISP_ODDMR_DITHER_CTR_1 0x0044//11bit
#define DISP_ODDMR_DITHER_CTR_2 0x0048//10bit
#define DISP_ODDMR_DMR_UDMA_CTR_4 0x00D0
#define REG_DMR_BASE_ADDR_L REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_DMR_UDMA_CTR_5 0x00D4
#define REG_DMR_BASE_ADDR_H REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_UDMA_CTR_0 0x0100
#define REG_OD_CLK_EN REG_FLD_MSB_LSB(11, 11)
#define DISP_ODDMR_OD_HSK_0 0x0180
#define REG_HS2DE_VDE_H_SIZE REG_FLD_MSB_LSB(13, 0)
#define DISP_ODDMR_OD_HSK_1 0x0184
#define REG_HS2DE_VDE_V_SIZE REG_FLD_MSB_LSB(12, 0)
#define DISP_ODDMR_OD_HSK_2 0x0188
#define DISP_ODDMR_OD_HSK_3 0x018C
#define DISP_ODDMR_OD_HSK_4 0x0190
#define DISP_ODDMR_CRP_CTR_0 0x0194
#define REG_CROP_VDE_H_SIZE REG_FLD_MSB_LSB(13, 0)
#define DISP_ODDMR_CRP_CTR_1 0x0198
#define REG_CROP_VDE_V_SIZE REG_FLD_MSB_LSB(12, 0)
#define DISP_ODDMR_CRP_CTR_2 0x019C
#define REG_WINDOW_LR REG_FLD_MSB_LSB(0, 0)
#define REG_GUARD_BAND_PIXEL REG_FLD_MSB_LSB(14, 8)
#define DISP_ODDMR_TOP_OD_BYASS 0x01C0
#define DISP_ODDMR_TOP_DMR_BYASS 0x01C4
#define DISP_ODDMR_TOP_S2R_BYPASS 0x01C8
#define DISP_ODDMR_TOP_HRT0_BYPASS 0x01CC
#define DISP_ODDMR_TOP_HRT1_BYPASS 0x01D0
#define DISP_ODDMR_TOP_DITHER_BYPASS 0x01D4
#define DISP_ODDMR_TOP_CRP_BYPSS 0x01D8
#define DISP_ODDMR_TOP_CLK_GATING 0x01E0
#define DISP_ODDMR_DMR_UDMA_ENABLE 0x01E4
#define DISP_ODDMR_DITHER_12TO11_BYPASS 0x01E8
#define DISP_ODDMR_DITHER_12TO10_BYPASS 0x01EC
#define DISP_ODDMR_DMR_SECURITY 0x01FC

/* VGS */
#define DISP_ODDMR_REG_VGS_BASE 0x200

/* DMR */
#define DISP_ODDMR_REG_DMR_BASE 0x400
#define DISP_ODDMR_REG_DMR_EN (0x0004 + DISP_ODDMR_REG_DMR_BASE)
#define DISP_ODDMR_REG_DMR_LAYER_MODE_EN (0x0008 + DISP_ODDMR_REG_DMR_BASE)
#define DISP_ODDMR_REG_DMR_OFFSET_SCALER_EN (0x000C + DISP_ODDMR_REG_DMR_BASE)
#define DISP_ODDMR_REG_DMR_OFFSET_GAIN (0x0010 + DISP_ODDMR_REG_DMR_BASE)
#define REG_DMR_OFFSET_GAIN REG_FLD_MSB_LSB(9, 0)
#define DISP_ODDMR_REG_DMR_GLOBAL_MODE_EN (0x0164 + DISP_ODDMR_REG_DMR_BASE)
#define DISP_ODDMR_REG_DMR_LOCAL_MODE_EN (0x0168 + DISP_ODDMR_REG_DMR_BASE)
#define DISP_ODDMR_REG_DMR_ZERO_MODE (0x016C + DISP_ODDMR_REG_DMR_BASE)
#define DISP_ODDMR_REG_DMR_FRAME_SCALER_EN (0x0174 + DISP_ODDMR_REG_DMR_BASE)
#define DISP_ODDMR_REG_DMR_DITHER_EN (0x0178 + DISP_ODDMR_REG_DMR_BASE)
#define DISP_ODDMR_REG_DMR_FRAME_SCALER_FOR_BETA (0x017C + DISP_ODDMR_REG_DMR_BASE)
#define REG_DMR_FRAME_SCALER_FOR_BETA REG_FLD_MSB_LSB(7, 0)
#define DISP_ODDMR_REG_DMR_FRAME_SCALER_FOR_GAIN (0x0180 + DISP_ODDMR_REG_DMR_BASE)
#define REG_DMR_FRAME_SCALER_FOR_GAIN REG_FLD_MSB_LSB(7, 0)
#define DISP_ODDMR_REG_DMR_FRAME_SCALER_FOR_OFFSET (0x0184 + DISP_ODDMR_REG_DMR_BASE)
#define REG_DMR_FRAME_SCALER_FOR_OFFSET REG_FLD_MSB_LSB(7, 0)
#define DISP_ODDMR_REG_DMR_BLOCK_SIZE_MODE (0x0188 + DISP_ODDMR_REG_DMR_BASE)
#define REG_DMR_BLOCK_SIZE_MODE REG_FLD_MSB_LSB(1, 0)
#define DISP_ODDMR_REG_DMR_LAYER_MODE (0x018C + DISP_ODDMR_REG_DMR_BASE)
#define REG_DMR_LAYER_MODE REG_FLD_MSB_LSB(1, 0)
#define DISP_ODDMR_REG_DMR_RGB_MODE (0x0190 + DISP_ODDMR_REG_DMR_BASE)
#define REG_DMR_RGB_MODE REG_FLD_MSB_LSB(1, 0)
#define DISP_ODDMR_REG_DMR_PANEL_WIDTH (0x0194 + DISP_ODDMR_REG_DMR_BASE)
#define REG_DMR_PANEL_WIDTH REG_FLD_MSB_LSB(13, 0)
#define DISP_ODDMR_REG_DMR_HVSP_6BLOCK_EN (0x01A8 + DISP_ODDMR_REG_DMR_BASE)

/* UDMA_DMR */
#define DISP_ODDMR_REG_UDMA_DMR_BASE 0x600


/* OD */
#define DISP_ODDMR_REG_OD_BASE 0x1000
#define DISP_ODDMR_OD_SRAM_CTRL_0 (0x0004 + DISP_ODDMR_REG_OD_BASE)
#define REG_W_OD_SRAM_IO_EN REG_FLD_MSB_LSB(0, 0)
#define REG_B_OD_SRAM_IO_EN REG_FLD_MSB_LSB(1, 1)
#define REG_G_OD_SRAM_IO_EN REG_FLD_MSB_LSB(2, 2)
#define REG_R_OD_SRAM_IO_EN REG_FLD_MSB_LSB(3, 3)
#define REG_WBGR_OD_SRAM_IO_EN REG_FLD_MSB_LSB(3, 0)
#define REG_OD_SRAM_WRITE_SEL REG_FLD_MSB_LSB(4, 4)
#define REG_OD_SRAM_READ_SEL REG_FLD_MSB_LSB(5, 5)
#define REG_AUTO_SRAM_ADR_INC_EN REG_FLD_MSB_LSB(11, 11)
//#define REG_SRAM_TABLE_EN REG_FLD_MSB_LSB(15, 12)

#define DISP_ODDMR_OD_SRAM_CTRL_1 (0x0008 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_SRAM1_IOADDR REG_FLD_MSB_LSB(8, 0)
#define REG_OD_SRAM1_IOWE_READ_BACK REG_FLD_MSB_LSB(15, 15)
#define DISP_ODDMR_OD_SRAM_CTRL_2 (0x000C + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_SRAM1_IOWDATA REG_FLD_MSB_LSB(7, 0)
#define DISP_ODDMR_OD_SRAM_CTRL_3 (0x0010 + DISP_ODDMR_REG_OD_BASE)

#define DISP_ODDMR_OD_SRAM_CTRL_4 (0x0014 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_SRAM2_IOADDR REG_FLD_MSB_LSB(8, 0)
#define REG_OD_SRAM2_IOWE_READ_BACK REG_FLD_MSB_LSB(15, 15)
#define DISP_ODDMR_OD_SRAM_CTRL_5 (0x0018 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_SRAM2_IOWDATA REG_FLD_MSB_LSB(7, 0)
#define DISP_ODDMR_OD_SRAM_CTRL_6 (0x001C + DISP_ODDMR_REG_OD_BASE)

#define DISP_ODDMR_OD_SRAM_CTRL_7 (0x0020 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_SRAM3_IOADDR REG_FLD_MSB_LSB(8, 0)
#define REG_OD_SRAM3_IOWE_READ_BACK REG_FLD_MSB_LSB(15, 15)
#define DISP_ODDMR_OD_SRAM_CTRL_8 (0x0024 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_SRAM3_IOWDATA REG_FLD_MSB_LSB(7, 0)
#define DISP_ODDMR_OD_SRAM_CTRL_9 (0x0028 + DISP_ODDMR_REG_OD_BASE)

#define DISP_ODDMR_OD_SRAM_CTRL_10 (0x002C + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_SRAM4_IOADDR REG_FLD_MSB_LSB(8, 0)
#define REG_OD_SRAM4_IOWE_READ_BACK REG_FLD_MSB_LSB(15, 15)
#define DISP_ODDMR_OD_SRAM_CTRL_11 (0x0030 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_SRAM4_IOWDATA REG_FLD_MSB_LSB(7, 0)
#define DISP_ODDMR_OD_SRAM_CTRL_12 (0x0034 + DISP_ODDMR_REG_OD_BASE)

#define DISP_ODDMR_OD_SRAM_CTRL_13 (0x0038 + DISP_ODDMR_REG_OD_BASE)
#define DISP_ODDMR_OD_CTRL_EN (0x0040 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_EN REG_FLD_MSB_LSB(0, 0)
#define REG_OD_MODE REG_FLD_MSB_LSB(3, 1)
#define DISP_ODDMR_OD_PQ_0 (0x0044 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_USER_WEIGHT REG_FLD_MSB_LSB(7, 0)
#define DISP_ODDMR_OD_PQ_1 (0x0048 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_ACT_THRD REG_FLD_MSB_LSB(7, 0)
#define DISP_ODDMR_OD_BASE_ADDR_R_LSB (0x0054 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_BASE_ADDR_R_LSB REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_BASE_ADDR_R_MSB (0x0058 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_BASE_ADDR_R_MSB REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_BASE_ADDR_G_LSB (0x005C + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_BASE_ADDR_G_LSB REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_BASE_ADDR_G_MSB (0x0060 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_BASE_ADDR_G_MSB REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_BASE_ADDR_B_LSB (0x0064 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_BASE_ADDR_B_LSB REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_BASE_ADDR_B_MSB (0x0068 + DISP_ODDMR_REG_OD_BASE)
#define REG_OD_BASE_ADDR_B_MSB REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_SW_RESET (0x0094 + DISP_ODDMR_REG_OD_BASE)
#define DISP_ODDMR_OD_UMDA_CTRL_0 (0x00B0 + DISP_ODDMR_REG_OD_BASE)//all 0
#define DISP_ODDMR_OD_UMDA_CTRL_1 (0x00B4 + DISP_ODDMR_REG_OD_BASE)
#define REG_LN_OFFSET REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_UMDA_CTRL_2 (0x00B8 + DISP_ODDMR_REG_OD_BASE)
#define REG_HSIZE REG_FLD_MSB_LSB(15, 0)
#define DISP_ODDMR_OD_UMDA_CTRL_3 (0x00BC + DISP_ODDMR_REG_OD_BASE)
#define REG_VSIZE REG_FLD_MSB_LSB(15, 0)

/* SPR2RGB */
#define DISP_ODDMR_REG_SPR2RGB_BASE 0x800
#define DISP_ODDMR_REG_SPR_COMP_EN (0x0004 + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_MASK_0 (0x0008 + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_MASK_1 (0x000C + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_MASK_2 (0x0010 + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_MASK_3 (0x0014 + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_MASK_4 (0x0018 + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_MASK_5 (0x001C + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_MASK_2X2 (0x0020 + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_PANEL_WIDTH (0x0024 + DISP_ODDMR_REG_SPR2RGB_BASE)
#define DISP_ODDMR_REG_SPR_X_INIT (0x0028 + DISP_ODDMR_REG_SPR2RGB_BASE)

/* SMI SB */
#define DISP_ODDMR_REG_SMI_BASE 0xA00
#define DISP_ODDMR_HRT_CTR (0x003C + DISP_ODDMR_REG_SMI_BASE)
#define REG_HR0_RE_ULTRA_MODE REG_FLD_MSB_LSB(3, 0)
#define REG_HR0_POACH_CFG_OFF REG_FLD_MSB_LSB(4, 4)
#define REG_HR0_PREULTRA_RE_ULTRA_MASK REG_FLD_MSB_LSB(6, 6)
#define REG_HR0_ULTRA_RE_MASK REG_FLD_MSB_LSB(7, 7)
#define REG_HR1_RE_ULTRA_MODE REG_FLD_MSB_LSB(11, 8)
#define REG_HR1_POACH_CFG_OFF REG_FLD_MSB_LSB(12, 12)
#define REG_HR1_PREULTRA_RE_ULTRA_MASK REG_FLD_MSB_LSB(14, 14)
#define REG_HR1_ULTRA_RE_MASK REG_FLD_MSB_LSB(15, 15)
#define DISP_ODDMR_REG_HR0_PREULTRA_RE_IN_THR_0 (0x0040 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR0_PREULTRA_RE_IN_THR_1 (0x0044 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR0_PREULTRA_RE_OUT_THR_0 (0x0048 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR0_PREULTRA_RE_OUT_THR_1 (0x004C + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR0_ULTRA_RE_IN_THR_0 (0x0050 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR0_ULTRA_RE_IN_THR_1 (0x0054 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR0_ULTRA_RE_OUT_THR_0 (0x0058 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR0_ULTRA_RE_OUT_THR_1 (0x005C + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR1_PREULTRA_RE_IN_THR_0 (0x0060 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR1_PREULTRA_RE_IN_THR_1 (0x0064 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR1_PREULTRA_RE_OUT_THR_0 (0x0068 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR1_PREULTRA_RE_OUT_THR_1 (0x006C + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR1_ULTRA_RE_IN_THR_0 (0x0070 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR1_ULTRA_RE_IN_THR_1 (0x0074 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR1_ULTRA_RE_OUT_THR_0 (0x0078 + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_HR1_ULTRA_RE_OUT_THR_1 (0x007C + DISP_ODDMR_REG_SMI_BASE)//12 0
#define DISP_ODDMR_REG_ODR_PREULTRA_RE_IN_THR_0 (0x0080 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_ODR_PREULTRA_RE_IN_THR_1 (0x0084 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_ODR_PREULTRA_RE_OUT_THR_0 (0x0088 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_ODR_PREULTRA_RE_OUT_THR_1 (0x008C + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_ODR_ULTRA_RE_IN_THR_0 (0x0090 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_ODR_ULTRA_RE_IN_THR_1 (0x0094 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_ODR_ULTRA_RE_OUT_THR_0 (0x0098 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_ODR_ULTRA_RE_OUT_THR_1 (0x009C + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODR_8 (0x00A0 + DISP_ODDMR_REG_SMI_BASE)
#define REG_ODR_POACH_CFG_OFF REG_FLD_MSB_LSB(1, 1)
#define REG_ODR_RE_ULTRA_MODE REG_FLD_MSB_LSB(7, 4)
#define DISP_ODDMR_REG_ODW_PREULTRA_WR_IN_THR_0  (0x00C0 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_ODW_PREULTRA_WR_IN_THR_1  (0x00C4 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_ODW_PREULTRA_WR_OUT_THR_0  (0x00C8 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_ODW_PREULTRA_WR_OUT_THR_1  (0x00CC + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_ODW_ULTRA_WR_IN_THR_0  (0x00D0 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_ODW_ULTRA_WR_IN_THR_1  (0x00D4 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_ODW_ULTRA_WR_OUT_THR_0  (0x00D8 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_ODW_ULTRA_WR_OUT_THR_1  (0x00DC + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_ODW_8 (0x00E0 + DISP_ODDMR_REG_SMI_BASE)
#define REG_ODW_POACH_CFG_OFF REG_FLD_MSB_LSB(1, 1)
#define REG_ODW_WR_ULTRA_MODE REG_FLD_MSB_LSB(7, 4)
#define DISP_ODDMR_REG_DMR_PREULTRA_RE_IN_THR_0  (0x0100 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_DMR_PREULTRA_RE_IN_THR_1  (0x0104 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_DMR_PREULTRA_RE_OUT_THR_0  (0x0108 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_DMR_PREULTRA_RE_OUT_THR_1  (0x010C + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_DMR_ULTRA_RE_IN_THR_0  (0x0110 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_DMR_ULTRA_RE_IN_THR_1  (0x0114 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_DMR_ULTRA_RE_OUT_THR_0  (0x0118 + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_REG_DMR_ULTRA_RE_OUT_THR_1  (0x011C + DISP_ODDMR_REG_SMI_BASE)//15 0
#define DISP_ODDMR_SMI_SB_FLG_DMR_8 (0x0120 + DISP_ODDMR_REG_SMI_BASE)
#define REG_DMR_POACH_CFG_OFF REG_FLD_MSB_LSB(1, 1)
#define REG_DMR_RE_ULTRA_MODE REG_FLD_MSB_LSB(7, 4)
/* OD UDMA R*/
#define DISP_ODDMR_REG_UDMA_R_BASE 0x1600
#define DISP_ODDMR_UDMA_R_CTRL84_0	(0x0160 + DISP_ODDMR_REG_UDMA_R_BASE)//R
#define DISP_ODDMR_UDMA_R_CTRL84_1	(0x0164 + DISP_ODDMR_REG_UDMA_R_BASE)//R
#define DISP_ODDMR_UDMA_R_CTRL84_2	(0x0168 + DISP_ODDMR_REG_UDMA_R_BASE)//R
#define DISP_ODDMR_UDMA_R_CTRL86_0	(0x01A0 + DISP_ODDMR_REG_UDMA_R_BASE)//R
#define DISP_ODDMR_UDMA_R_CTRL86_1	(0x01A4 + DISP_ODDMR_REG_UDMA_R_BASE)//R
#define DISP_ODDMR_UDMA_R_CTRL86_2	(0x01A8 + DISP_ODDMR_REG_UDMA_R_BASE)//R
#define DISP_ODDMR_UDMA_R_CTRL88	(0x01C0 + DISP_ODDMR_REG_UDMA_R_BASE)//W 1
#define DISP_ODDMR_UDMA_R_CTRL_104	(0x01C8 + DISP_ODDMR_REG_UDMA_R_BASE)//R

/* OD UDMA W*/
#define DISP_ODDMR_REG_UDMA_W_BASE 0x1800
#define DISP_ODDMR_UDMA_W_CTR_58	(0x0160 + DISP_ODDMR_REG_UDMA_W_BASE)//R
#define DISP_ODDMR_UDMA_W_CTR_59	(0x0164 + DISP_ODDMR_REG_UDMA_W_BASE)//R
#define DISP_ODDMR_UDMA_W_CTR_60	(0x0168 + DISP_ODDMR_REG_UDMA_W_BASE)//R
#define DISP_ODDMR_UDMA_W_CTR_74	(0x01A0 + DISP_ODDMR_REG_UDMA_W_BASE)//R
#define DISP_ODDMR_UDMA_W_CTR_75	(0x01A4 + DISP_ODDMR_REG_UDMA_W_BASE)//R
#define DISP_ODDMR_UDMA_W_CTR_76	(0x01A8 + DISP_ODDMR_REG_UDMA_W_BASE)//R
#define DISP_ODDMR_UDMA_W_CTR_82	(0x01C0 + DISP_ODDMR_REG_UDMA_W_BASE)//W 1
#define DISP_ODDMR_UDMA_W_CTR_84	(0x01C8 + DISP_ODDMR_REG_UDMA_W_BASE)//R

#define OD_H_ALIGN_BITS 128
#define ODDMR_READ_IN_PRE_ULTRA     (2 / 3)
#define ODDMR_READ_IN_ULTRA         (1 / 3)
#define ODDMR_READ_OUT_PRE_ULTRA    (3 / 4)
#define ODDMR_READ_OUT_ULTRA        (2 / 4)
#define ODDMR_WRITE_IN_PRE_ULTRA    (1 / 3)
#define ODDMR_WRITE_IN_ULTRA        (2 / 3)
#define ODDMR_WRITE_OUT_PRE_ULTRA   (1 / 4)
#define ODDMR_WRITE_OUT_ULTRA       (2 / 4)

static bool debug_flow_log = true;
#define ODDMRFLOW_LOG(fmt, arg...) do { \
	if (debug_flow_log) \
		DDPINFO("[FLOW]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_low_log;
#define ODDMRLOW_LOG(fmt, arg...) do { \
	if (debug_low_log) \
		DDPINFO("[LOW]%s:" fmt, __func__, ##arg); \
	} while (0)

static bool debug_api_log;
#define ODDMRAPI_LOG(fmt, arg...) do { \
	if (debug_api_log) \
		DDPINFO("[API]%s:" fmt, __func__, ##arg); \
	} while (0)

//TODO split dmr table in sw for dynamic overhead
static bool is_oddmr_od_support;
static bool is_oddmr_dmr_support;
static bool g_oddmr_ddren = true;
static bool g_oddmr_hrt_en = true;
static bool g_oddmr_dump_en;
#define ODDMR_ENABLE_IRQ
#define ODDMR_IRQ_MASK_VAL 0x2F
/*
 * od_weight_trigger is used to trigger od set pq
 * is used in resume, res switch flow frame 2
 * frame 1: od on weight = 0 (weight_trigger == 1)
 * frame 2: od on weight != 0 (weight_trigger == 0)
 */
static atomic_t g_oddmr_od_weight_trigger = ATOMIC_INIT(0);
static atomic_t g_oddmr_frame_dirty = ATOMIC_INIT(0);
static atomic_t g_oddmr_pq_dirty = ATOMIC_INIT(0);
static atomic_t g_oddmr_sof_irq_available = ATOMIC_INIT(0);
/* 2: need oddmr hrt, 1: oddmr hrt done, 0:nothing todo */
static atomic_t g_oddmr_dmr_hrt_done = ATOMIC_INIT(0);
static atomic_t g_oddmr_od_hrt_done = ATOMIC_INIT(0);
static atomic_t g_od_check_trigger = ATOMIC_INIT(1);

// It's a work around for no comp assigned in functions.
struct mtk_ddp_comp *default_comp;
struct mtk_ddp_comp *oddmr1_default_comp;
struct mtk_disp_oddmr *g_oddmr_priv;
struct mtk_disp_oddmr *g_oddmr1_priv;
struct mtk_oddmr_timing g_oddmr_current_timing;
static struct mtk_oddmr_panelid g_panelid;

static struct task_struct *oddmr_sof_irq_event_task;
static DECLARE_WAIT_QUEUE_HEAD(g_oddmr_sof_irq_wq);
static DECLARE_WAIT_QUEUE_HEAD(g_oddmr_hrt_wq);
static DEFINE_SPINLOCK(g_oddmr_clock_lock);
static DEFINE_SPINLOCK(g_oddmr_od_sram_lock);
static DEFINE_SPINLOCK(g_oddmr_timing_lock);

static void mtk_oddmr_od_hsk_force_clk(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg);
static void mtk_oddmr_od_smi(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg);
static void mtk_oddmr_od_set_dram(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg);
static void mtk_oddmr_od_set_dram(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg);
static void mtk_oddmr_set_od_enable(struct mtk_ddp_comp *comp, uint32_t enable,
		struct cmdq_pkt *handle);
static void mtk_oddmr_set_od_enable_dual(struct mtk_ddp_comp *comp, uint32_t enable,
		struct cmdq_pkt *handle);
static uint32_t mtk_oddmr_od_get_dram_size(uint32_t width, uint32_t height,
		uint32_t scaling_mode, uint32_t od_mode, uint32_t channel);


static inline unsigned int mtk_oddmr_read(struct mtk_ddp_comp *comp,
		unsigned int offset)
{
		if (offset >= 0x2000 || (offset % 4) != 0) {
			DDPPR_ERR("%s: invalid addr 0x%x\n",
				__func__, offset);
		return 0;
	}

	return readl(comp->regs + offset);
}

static inline void mtk_oddmr_write_mask_cpu(struct mtk_ddp_comp *comp,
		unsigned int value, unsigned int offset, unsigned int mask)
{
	unsigned int tmp;

	if (offset >= 0x2000 || (offset % 4) != 0) {
		DDPPR_ERR("%s: invalid addr 0x%x\n",
				__func__, offset);
		return;
	}

	tmp = readl(comp->regs + offset);
	tmp = (tmp & ~mask) | (value & mask);
	writel(tmp, comp->regs + offset);
}

static inline void mtk_oddmr_write_cpu(struct mtk_ddp_comp *comp,
		unsigned int value, unsigned int offset)
{
	if (offset >= 0x2000 || (offset % 4) != 0) {
		DDPPR_ERR("%s: invalid addr 0x%x\n",
				__func__, offset);
		return;
	}
	writel(value, comp->regs + offset);
}

static inline void mtk_oddmr_write(struct mtk_ddp_comp *comp, unsigned int value,
		unsigned int offset, void *handle)
{
	if (offset >= 0x2000 || (offset % 4) != 0) {
		DDPPR_ERR("%s: invalid addr 0x%x\n",
				__func__, offset);
		return;
	}
	if (handle != NULL) {
		cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
				comp->regs_pa + offset, value, ~0);
	} else {
		writel(value, comp->regs + offset);
	}
}

static inline void mtk_oddmr_write_mask(struct mtk_ddp_comp *comp, unsigned int value,
		unsigned int offset, unsigned int mask, void *handle)
{
	if (offset >= 0x2000 || (offset % 4) != 0) {
		DDPPR_ERR("%s: invalid addr 0x%x\n",
				__func__, offset);
		return;
	}
	if (handle != NULL) {
		cmdq_pkt_write((struct cmdq_pkt *)handle, comp->cmdq_base,
				comp->regs_pa + offset, value, mask);
	} else {
		mtk_oddmr_write_mask_cpu(comp, value, offset, mask);
	}
}

static inline struct mtk_disp_oddmr *comp_to_oddmr(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_oddmr, ddp_comp);
}

/* must check after get current table idx*/
static inline bool mtk_oddmr_is_table_valid(int table_idx,
		uint32_t valid_table, const char *name, int line, int log)
{
	if (table_idx < 0) {
		if (log)
			DDPPR_ERR("%s[%d] invalid tableidx %d 0x%x\n",
					name, line, table_idx, valid_table);
		return false;
	}
	if (((1 << table_idx) & valid_table) > 0)
		return true;
	if (log)
		DDPPR_ERR("%s[%d] invalid tableidx %d 0x%x\n",
				name, line, table_idx, valid_table);
	return false;
}
#define IS_TABLE_VALID(idx, valid) mtk_oddmr_is_table_valid(idx, valid, __func__, __LINE__, 1)
#define IS_TABLE_VALID_LOW(idx, valid) mtk_oddmr_is_table_valid(idx, valid, __func__, __LINE__, 0)
static bool mtk_oddmr_is_svp_on_mtee(void)
{
	struct device_node *dt_node;

	dt_node = of_find_node_by_name(NULL, "MTEE");
	if (!dt_node)
		return false;

	return true;
}
/*
 * @addr init data from addr if needed, set to NULL when no need
 * @secu alloc svp iommu if needed
 */
	static inline struct mtk_drm_gem_obj *
mtk_oddmr_load_buffer(struct drm_crtc *crtc,
		size_t size, void *addr, bool secu)
{
	struct mtk_drm_gem_obj *gem;

	ODDMRAPI_LOG("+\n");

	if (!size) {
		DDPMSG("%s invalid size\n",
				__func__);
		return NULL;
	}
	if (secu) {
		//mtk_svp_page-uncached,mtk_mm-uncached
		gem = mtk_drm_gem_create_from_heap(crtc->dev,
				"mtk_svp_page-uncached", size);
	} else {
		gem = mtk_drm_gem_create(
				crtc->dev, size, true);
	}

	if (!gem) {
		DDPMSG("%s gem create fail\n", __func__);
		return NULL;
	}
	DDPMSG("%s gem create %p iommu %llx size %u\n", __func__, gem->kvaddr, gem->dma_addr, size);
	if ((addr != NULL) && (!secu))
		memcpy(gem->kvaddr, addr, size);

	return gem;
}

static int mtk_oddmr_create_gce_pkt(struct drm_crtc *crtc,
		struct cmdq_pkt **pkt)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int index = 0;

	if (!mtk_crtc) {
		DDPMSG("%s:%d, invalid crtc:0x%p\n",
				__func__, __LINE__, crtc);
		return -1;
	}

	index = drm_crtc_index(crtc);
	if (index) {
		DDPMSG("%s:%d, invalid crtc:0x%p, index:%d\n",
				__func__, __LINE__, crtc, index);
		return -1;
	}

	if (*pkt != NULL)
		return 0;

	if (mtk_crtc->gce_obj.client[CLIENT_PQ])
		*pkt = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_PQ]);
	else
		*pkt = cmdq_pkt_create(mtk_crtc->gce_obj.client[CLIENT_CFG]);

	return 0;
}

static int mtk_oddmr_acquire_clock(void)
{
	unsigned long flags;

	ODDMRAPI_LOG("%d+\n",
			atomic_read(&g_oddmr_priv->oddmr_clock_ref));

	spin_lock_irqsave(&g_oddmr_clock_lock, flags);
	if (atomic_read(&g_oddmr_priv->oddmr_clock_ref) == 0) {
		ODDMRFLOW_LOG("top clock is off\n");
		spin_unlock_irqrestore(&g_oddmr_clock_lock, flags);
		return -EAGAIN;
	}
	if (default_comp->mtk_crtc->is_dual_pipe) {
		if (atomic_read(&g_oddmr1_priv->oddmr_clock_ref) == 0) {
			ODDMRFLOW_LOG("top clock is off\n");
			spin_unlock_irqrestore(&g_oddmr_clock_lock, flags);
			return -EAGAIN;
		}
		atomic_inc(&g_oddmr1_priv->oddmr_clock_ref);
	}
	atomic_inc(&g_oddmr_priv->oddmr_clock_ref);
	spin_unlock_irqrestore(&g_oddmr_clock_lock, flags);
	ODDMRAPI_LOG("%d-\n",
			atomic_read(&g_oddmr_priv->oddmr_clock_ref));
	return 0;
}

static int mtk_oddmr_release_clock(void)
{
	unsigned long flags;

	ODDMRAPI_LOG("%d+\n",
			atomic_read(&g_oddmr_priv->oddmr_clock_ref));

	spin_lock_irqsave(&g_oddmr_clock_lock, flags);
	if (atomic_read(&g_oddmr_priv->oddmr_clock_ref) == 0) {
		ODDMRFLOW_LOG("top clock is off\n");
		spin_unlock_irqrestore(&g_oddmr_clock_lock, flags);
		return -EAGAIN;
	}
	if (default_comp->mtk_crtc->is_dual_pipe) {
		if (atomic_read(&g_oddmr1_priv->oddmr_clock_ref) == 0) {
			ODDMRFLOW_LOG("top clock is off\n");
			spin_unlock_irqrestore(&g_oddmr_clock_lock, flags);
			return -EAGAIN;
		}
		atomic_dec(&g_oddmr1_priv->oddmr_clock_ref);
	}
	atomic_dec(&g_oddmr_priv->oddmr_clock_ref);
	spin_unlock_irqrestore(&g_oddmr_clock_lock, flags);
	ODDMRAPI_LOG("%d-\n",
			atomic_read(&g_oddmr_priv->oddmr_clock_ref));
	return 0;
}

static void mtk_oddmr_set_pq(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, struct mtk_oddmr_pq_param *pq)
{
	struct mtk_oddmr_pq_pair *param;
	uint32_t cnt;
	int i;

	param = pq->param;
	cnt = pq->counts;
	if (0 == cnt || NULL == param) {
		ODDMRFLOW_LOG("pq is NULL\n");
	} else {
		for (i = 0; i < cnt; i++) {
			ODDMRLOW_LOG("i %d, 0x%x 0x%x\n", i, param[i].addr, param[i].value);
			mtk_oddmr_write(comp, param[i].value, param[i].addr, pkg);
		}
	}
}

static void mtk_oddmr_set_crop(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	uint32_t tile_overhead, comp_width, height;
	uint32_t reg_value, reg_mask, win_lr = 0;

	reg_value = 0;
	reg_mask = 0;
	tile_overhead = oddmr_priv->cfg.comp_overhead;
	comp_width = oddmr_priv->cfg.comp_in_width;
	height = oddmr_priv->cfg.height;
	if (oddmr_priv->is_right_pipe)
		win_lr = 1;
	ODDMRAPI_LOG("comp_width %u tile_overhead %u win_lr %u+\n",
		comp_width, tile_overhead, win_lr);
	SET_VAL_MASK(reg_value, reg_mask, win_lr, REG_WINDOW_LR);
	SET_VAL_MASK(reg_value, reg_mask, tile_overhead, REG_GUARD_BAND_PIXEL);
	mtk_oddmr_write(comp, reg_value, DISP_ODDMR_CRP_CTR_2, pkg);
	//actual width
	mtk_oddmr_write(comp, comp_width - tile_overhead, DISP_ODDMR_CRP_CTR_0, pkg);
	mtk_oddmr_write(comp, height, DISP_ODDMR_CRP_CTR_1, pkg);
	mtk_oddmr_write(comp, 0, DISP_ODDMR_TOP_CRP_BYPSS, pkg);
}

static void mtk_oddmr_set_crop_dual(struct cmdq_pkt *pkg)
{
	ODDMRAPI_LOG("+\n");
	if (default_comp->mtk_crtc->is_dual_pipe) {
		mtk_oddmr_set_crop(default_comp, pkg);
		mtk_oddmr_set_crop(oddmr1_default_comp, pkg);
	} else
		mtk_oddmr_write(default_comp, 1, DISP_ODDMR_TOP_CRP_BYPSS, pkg);
}

static void mtk_oddmr_od_udma_init(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	uint32_t od_mode = g_od_param.od_basic_info.basic_param.od_mode;

	switch (od_mode) {
	case OD_MODE_TYPE_COMPRESS_18:
		break;
	case OD_MODE_TYPE_COMPRESS_12:
		mtk_oddmr_write(comp, 0x00000000, 0x183c, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1800, pkg);
		mtk_oddmr_write(comp, 0x00000009, 0x1804, pkg);
		mtk_oddmr_write(comp, 0x00007fff, 0x1808, pkg);
		mtk_oddmr_write(comp, 0x00008008, 0x180c, pkg);
		mtk_oddmr_write(comp, 0x00008008, 0x1810, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1814, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1818, pkg);
		mtk_oddmr_write(comp, 0x00000008, 0x181c, pkg);
		mtk_oddmr_write(comp, 0x00000010, 0x1820, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1824, pkg);
		mtk_oddmr_write(comp, 0x00000020, 0x1828, pkg);
		mtk_oddmr_write(comp, 0x00000001, 0x183c, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1800, pkg);
		mtk_oddmr_write(comp, 0x00000009, 0x1804, pkg);
		mtk_oddmr_write(comp, 0x00007fff, 0x1808, pkg);
		mtk_oddmr_write(comp, 0x00008008, 0x180c, pkg);
		mtk_oddmr_write(comp, 0x00008008, 0x1810, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1814, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1818, pkg);
		mtk_oddmr_write(comp, 0x00000008, 0x181c, pkg);
		mtk_oddmr_write(comp, 0x00000010, 0x1820, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1824, pkg);
		mtk_oddmr_write(comp, 0x00000020, 0x1828, pkg);
		mtk_oddmr_write(comp, 0x00000002, 0x183c, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1800, pkg);
		mtk_oddmr_write(comp, 0x00000009, 0x1804, pkg);
		mtk_oddmr_write(comp, 0x00007fff, 0x1808, pkg);
		mtk_oddmr_write(comp, 0x00008008, 0x180c, pkg);
		mtk_oddmr_write(comp, 0x00008008, 0x1810, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1814, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1818, pkg);
		mtk_oddmr_write(comp, 0x00000008, 0x181c, pkg);
		mtk_oddmr_write(comp, 0x00000010, 0x1820, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1824, pkg);
		mtk_oddmr_write(comp, 0x00000020, 0x1828, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1600, pkg);
		mtk_oddmr_write(comp, 0x000000ff, 0x1604, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1608, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x160c, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1610, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1614, pkg);
		mtk_oddmr_write(comp, 0x000000ff, 0x1618, pkg);
		mtk_oddmr_write(comp, 0x000000ff, 0x161c, pkg);
		mtk_oddmr_write(comp, 0x00000007, 0x1620, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1624, pkg);
		mtk_oddmr_write(comp, 0x00000000, 0x1628, pkg);
		mtk_oddmr_write(comp, 0x000000f8, 0x162c, pkg);
		mtk_oddmr_write(comp, 0x000000f8, 0x1630, pkg);
		mtk_oddmr_write(comp, 0x000000f8, 0x1634, pkg);
		mtk_oddmr_write(comp, 0x0000ffea, 0x1638, pkg);
		break;
	default:
		break;
	}
}

/* top,dither,common pq, udma */
static void mtk_oddmr_od_common_init(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	uint32_t value = 0, mask = 0;

	ODDMRAPI_LOG("+\n");
	/* top */
	SET_VAL_MASK(value, mask, 0, REG_OD_EN);
	SET_VAL_MASK(value, mask, g_od_param.od_basic_info.basic_param.od_mode, REG_OD_MODE);
	mtk_oddmr_write(comp, value, DISP_ODDMR_OD_CTRL_EN, pkg);
	if (g_od_param.od_basic_info.basic_param.dither_sel == 1) {
		mtk_oddmr_write(comp, g_od_param.od_basic_info.basic_param.dither_ctl,
				DISP_ODDMR_DITHER_CTR_1, pkg);
		mtk_oddmr_write(comp, 0, DISP_ODDMR_DITHER_12TO11_BYPASS, pkg);
		mtk_oddmr_write(comp, 1, DISP_ODDMR_DITHER_12TO10_BYPASS, pkg);
	} else if (g_od_param.od_basic_info.basic_param.dither_sel == 2) {
		mtk_oddmr_write(comp, g_od_param.od_basic_info.basic_param.dither_ctl,
				DISP_ODDMR_DITHER_CTR_2, pkg);
		mtk_oddmr_write(comp, 1, DISP_ODDMR_DITHER_12TO11_BYPASS, pkg);
		mtk_oddmr_write(comp, 0, DISP_ODDMR_DITHER_12TO10_BYPASS, pkg);
	} else if (g_od_param.od_basic_info.basic_param.dither_sel == 0) {
		mtk_oddmr_write(comp, 1, DISP_ODDMR_DITHER_12TO11_BYPASS, pkg);
		mtk_oddmr_write(comp, 1, DISP_ODDMR_DITHER_12TO10_BYPASS, pkg);
	}
	/* od basic pq */
	mtk_oddmr_set_pq(comp, pkg, &g_od_param.od_basic_info.basic_pq);
}

static void mtk_oddmr_od_hsk(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	uint32_t height, comp_width, tile_overhead = 0;
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);

	if (oddmr_priv == NULL)
		return;
	if (oddmr_priv->data != NULL && comp->mtk_crtc->is_dual_pipe)
		tile_overhead = oddmr_priv->cfg.comp_overhead;
	comp_width = oddmr_priv->cfg.comp_in_width;
	height = oddmr_priv->cfg.height;

	mtk_oddmr_write(comp, comp_width, DISP_ODDMR_OD_HSK_0, pkg);
	mtk_oddmr_write(comp, height, DISP_ODDMR_OD_HSK_1, pkg);
	mtk_oddmr_write(comp, g_od_param.od_basic_info.basic_param.od_hsk_2,
			DISP_ODDMR_OD_HSK_2, pkg);
	mtk_oddmr_write(comp, g_od_param.od_basic_info.basic_param.od_hsk_3,
			DISP_ODDMR_OD_HSK_3, pkg);
	mtk_oddmr_write(comp, g_od_param.od_basic_info.basic_param.od_hsk_4,
			DISP_ODDMR_OD_HSK_4, pkg);
}

static void mtk_oddmr_od_set_res(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	uint32_t tile_overhead, scaling_mode, hscaling, h_2x4x_sel, comp_width, width, height;
	uint32_t line_offset = 0;
	tile_overhead = 0;
	if (oddmr_priv == NULL)
		return;
	if (oddmr_priv->data != NULL && comp->mtk_crtc->is_dual_pipe)
		tile_overhead = oddmr_priv->cfg.comp_overhead;
	scaling_mode = g_od_param.od_basic_info.basic_param.scaling_mode;
	hscaling = (scaling_mode & BIT(0)) > 0 ? 2 : 1;
	h_2x4x_sel = (scaling_mode & BIT(2)) > 0 ? 2 : 1;
	hscaling = hscaling * h_2x4x_sel;
	comp_width = oddmr_priv->cfg.comp_in_width;
	width = oddmr_priv->cfg.width;
	height = oddmr_priv->cfg.height;
	line_offset = DIV_ROUND_UP(comp_width, hscaling);
	ODDMRAPI_LOG("w %u, h %u, tileoverhead %u hscaling %u comp_w %u line %u\n",
		width, height, tile_overhead, hscaling, comp_width, line_offset);
	mtk_oddmr_write(comp, 0, DISP_ODDMR_OD_UMDA_CTRL_0, pkg);
	mtk_oddmr_write(comp, line_offset, DISP_ODDMR_OD_UMDA_CTRL_1, pkg);
	mtk_oddmr_write(comp, comp_width, DISP_ODDMR_OD_UMDA_CTRL_2, pkg);
	mtk_oddmr_write(comp, height, DISP_ODDMR_OD_UMDA_CTRL_3, pkg);
}

static void mtk_oddmr_od_init_end(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_drm_private *priv;

	priv = mtk_crtc->base.dev->dev_private;
	if (mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_ODDMR_UNDERRUN_AEE)) {
		mtk_oddmr_write_mask(comp, ODDMR_IRQ_MASK_VAL,
			DISP_ODDMR_IRQ_MASK, ODDMR_IRQ_MASK_VAL, handle);
	}
	//od reset
	mtk_oddmr_write(comp, 0x200, DISP_ODDMR_OD_SW_RESET, handle);
	mtk_oddmr_write(comp, 0, DISP_ODDMR_OD_SW_RESET, handle);
	mtk_oddmr_od_udma_init(comp, handle);
	//force clk off
	mtk_oddmr_od_hsk(comp, handle);
	//bypass off
	mtk_oddmr_write(comp, 0,
			DISP_ODDMR_TOP_OD_BYASS, handle);
	mtk_oddmr_write(comp, 1,
			DISP_ODDMR_TOP_HRT0_BYPASS, handle);
}

static void mtk_oddmr_fill_cfg(struct mtk_ddp_comp *comp, struct mtk_ddp_config *cfg)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);

	oddmr_priv->cfg.width = cfg->w;
	oddmr_priv->cfg.height = cfg->h;
	//if (!cfg->tile_overhead.is_support) {
	if (mtk_crtc->is_dual_pipe) {
		oddmr_priv->cfg.comp_in_width = cfg->w / 2;
		oddmr_priv->cfg.comp_overhead = 0;
		oddmr_priv->cfg.total_overhead = 0;
	} else {
		oddmr_priv->cfg.comp_in_width = cfg->w;
		oddmr_priv->cfg.comp_overhead = 0;
		oddmr_priv->cfg.total_overhead = 0;
	}
	//}
}
static bool mtk_oddmr_drm_mode_to_oddmr_timg(struct mtk_drm_crtc *mtk_crtc,
		struct mtk_oddmr_timing *oddmr_mode)
{
	bool ret = false;

	if ((mtk_crtc != NULL) &&
			(mtk_crtc->base.state != NULL) &&
			(oddmr_mode != NULL)) {
		oddmr_mode->hdisplay = mtk_crtc->base.state->adjusted_mode.hdisplay;
		oddmr_mode->vdisplay = mtk_crtc->base.state->adjusted_mode.vdisplay;
		oddmr_mode->vrefresh = drm_mode_vrefresh(&mtk_crtc->base.state->adjusted_mode);
		ret = true;
	} else
		ret =  false;
	return ret;
}

static void mtk_oddmr_copy_oddmr_timg(struct mtk_oddmr_timing *dst,
		struct mtk_oddmr_timing *src)
{
	ODDMRAPI_LOG("+\n");
	dst->hdisplay = src->hdisplay;
	dst->vdisplay = src->vdisplay;
	dst->vrefresh = src->vrefresh;
	dst->mode_chg_index = src->mode_chg_index;
	dst->bl_level = src->bl_level;
}

static bool mtk_oddmr_match_panelid(struct mtk_oddmr_panelid *read,
		struct mtk_oddmr_panelid *expected)
{
	int i;
	bool flag = true;

	/* always return true if panelid in bin is zero*/
	if (expected->len == 0)
		return true;
	if (expected->len > 16)
		expected->len = 16;

	for (i = 0; i < expected->len; i++) {
		if (expected->data[i] != 0) {
			flag = false;
			break;
		}
	}
	if (flag == true)
		return flag;
	/* compare */
	flag = true;
	if (read->len > 16)
		read->len = 16;

	for (i = 0; i < read->len; i++) {
		if (expected->data[i] != read->data[i]) {
			flag = false;
			DDPPR_ERR("%s, idx %d, expected %u but %u\n",
					__func__, i, expected->data[i], read->data[i]);
			break;
		}
	}
	return flag;
}

/* return 100 x byte per pixel */
static int mtk_oddmr_dmr_bpp(int mode)
{
	int bits, ret = 100;

	switch (mode) {
	case DMR_MODE_TYPE_RGB8X8L8:
		bits = 3;
		break;
	case DMR_MODE_TYPE_RGB4X4L4:
	case DMR_MODE_TYPE_RGB4X4Q:
		bits = 6;
		break;
	case DMR_MODE_TYPE_RGB4X4L8:
		bits = 12;
		break;
	case DMR_MODE_TYPE_W2X2L4:
	case DMR_MODE_TYPE_W2X2Q:
		bits = 8;
		break;
	default:
		bits = 12;
		break;
	}
	ret = ret * bits / 8;
	return ret;
}

/* return 100 x byte per pixel */
static int mtk_oddmr_od_bpp(int mode)
{
	int bits, ret = 100;
	uint32_t scaling_mode =
		g_od_param.od_basic_info.basic_param.scaling_mode;
	int hscaling, h_2x4x_sel, vscaling;

	hscaling = (scaling_mode & BIT(0)) > 0 ? 2 : 1;
	vscaling = (scaling_mode & BIT(1)) > 0 ? 2 : 1;
	h_2x4x_sel = (scaling_mode & BIT(2)) > 0 ? 2 : 1;
	hscaling = hscaling * h_2x4x_sel;
	switch (mode) {
	case OD_MODE_TYPE_RGB444:
	case OD_MODE_TYPE_COMPRESS_12:
		bits = 12;
		break;
	case OD_MODE_TYPE_RGB565:
		bits = 16;
		break;
	case OD_MODE_TYPE_COMPRESS_18:
		bits = 18;
		break;
	case OD_MODE_TYPE_RGB555:
		bits = 15;
		break;
	case OD_MODE_TYPE_RGB888:
		bits = 24;
		break;
	default:
		bits = 24;
		break;
	}
	/* double for R & W */
	ret = 2 * ret;
	ret = ret * bits / (hscaling * vscaling * 8);
	return ret;
}

static void mtk_oddmr_od_srt_cal(struct mtk_ddp_comp *comp, int en)
{
	struct mtk_disp_oddmr *oddmr_priv;
	uint32_t srt = 0, scaling_mode, od_mode, vrefresh;

	if (comp == NULL)
		return;
	oddmr_priv = comp_to_oddmr(comp);
	if (en) {
		scaling_mode = g_od_param.od_basic_info.basic_param.scaling_mode;
		od_mode = g_od_param.od_basic_info.basic_param.od_mode;
		srt += mtk_oddmr_od_get_dram_size(oddmr_priv->cfg.comp_in_width,
			oddmr_priv->cfg.height, scaling_mode, od_mode, 0);
		srt += mtk_oddmr_od_get_dram_size(oddmr_priv->cfg.comp_in_width,
			oddmr_priv->cfg.height, scaling_mode, od_mode, 1);
		srt += mtk_oddmr_od_get_dram_size(oddmr_priv->cfg.comp_in_width,
			oddmr_priv->cfg.height, scaling_mode, od_mode, 2);
		vrefresh = g_oddmr_current_timing.vrefresh;
		//blanking ratio
		do_div(srt, 1000);
		srt *= 125;
		do_div(srt, 100);
		srt = srt * vrefresh;
		do_div(srt, 1000);
		oddmr_priv->qos_srt_odr = srt;
		oddmr_priv->qos_srt_odw = srt;
	} else {
		oddmr_priv->qos_srt_odr = 0;
		oddmr_priv->qos_srt_odw = 0;
	}
	ODDMRAPI_LOG("srt %u -\n", srt);
}

static void mtk_oddmr_set_spr2rgb(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	uint32_t mask_2x2 = 0, spr_mask[6] = {0}, x_init = 0, spr_format = 0;
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	bool is_right_pipe = false;
	uint32_t overhead, comp_width, width;

	if (oddmr_priv->spr_enable == 0 || oddmr_priv->spr_relay == 1)
		return;
	ODDMRAPI_LOG("+\n");
	is_right_pipe = oddmr_priv->is_right_pipe;
	spr_format = oddmr_priv->spr_format;
	overhead = oddmr_priv->cfg.comp_overhead;
	comp_width = oddmr_priv->cfg.comp_in_width;
	width = oddmr_priv->cfg.width;
	switch (spr_format) {
	case MTK_PANEL_RGBG_BGRG_TYPE:
		mask_2x2 = 1;
		spr_mask[0] = 1;
		spr_mask[1] = 3;
		spr_mask[2] = 3;
		spr_mask[3] = 1;
		spr_mask[4] = 0;
		spr_mask[5] = 0;
		break;
	case MTK_PANEL_BGRG_RGBG_TYPE:
		mask_2x2 = 1;
		spr_mask[0] = 3;
		spr_mask[1] = 1;
		spr_mask[2] = 1;
		spr_mask[3] = 3;
		spr_mask[4] = 0;
		spr_mask[5] = 0;
		break;
	case MTK_PANEL_RGBRGB_BGRBGR_TYPE:
		mask_2x2 = 0;
		spr_mask[0] = 1;
		spr_mask[1] = 2;
		spr_mask[2] = 3;
		spr_mask[3] = 3;
		spr_mask[4] = 2;
		spr_mask[5] = 1;
		break;
	case MTK_PANEL_BGRBGR_RGBRGB_TYPE:
		mask_2x2 = 0;
		spr_mask[0] = 3;
		spr_mask[1] = 2;
		spr_mask[2] = 1;
		spr_mask[3] = 1;
		spr_mask[4] = 2;
		spr_mask[5] = 3;
		break;
	case MTK_PANEL_RGBRGB_BRGBRG_TYPE:
		mask_2x2 = 0;
		spr_mask[0] = 1;
		spr_mask[1] = 2;
		spr_mask[2] = 3;
		spr_mask[3] = 2;
		spr_mask[4] = 3;
		spr_mask[5] = 1;
		break;
	case MTK_PANEL_BRGBRG_RGBRGB_TYPE:
		mask_2x2 = 0;
		spr_mask[0] = 2;
		spr_mask[1] = 3;
		spr_mask[2] = 1;
		spr_mask[3] = 1;
		spr_mask[4] = 2;
		spr_mask[5] = 3;
		break;
	default:
		break;
	}
	mtk_oddmr_write(comp, mask_2x2, DISP_ODDMR_REG_SPR_MASK_2X2, pkg);
	mtk_oddmr_write(comp, spr_mask[0], DISP_ODDMR_REG_SPR_MASK_0, pkg);
	mtk_oddmr_write(comp, spr_mask[1], DISP_ODDMR_REG_SPR_MASK_1, pkg);
	mtk_oddmr_write(comp, spr_mask[2], DISP_ODDMR_REG_SPR_MASK_2, pkg);
	mtk_oddmr_write(comp, spr_mask[3], DISP_ODDMR_REG_SPR_MASK_3, pkg);
	mtk_oddmr_write(comp, spr_mask[4], DISP_ODDMR_REG_SPR_MASK_4, pkg);
	mtk_oddmr_write(comp, spr_mask[5], DISP_ODDMR_REG_SPR_MASK_5, pkg);
	if (is_right_pipe) {
		//start idx of right pipe
		x_init = width - comp_width;
		x_init = mask_2x2 ? x_init % 2 : x_init % 3;
	}
	mtk_oddmr_write(comp, x_init, DISP_ODDMR_REG_SPR_X_INIT, pkg);
	if (comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_write(comp, comp_width, DISP_ODDMR_REG_SPR_PANEL_WIDTH, pkg);
	else
		mtk_oddmr_write(comp, width, DISP_ODDMR_REG_SPR_PANEL_WIDTH, pkg);
	mtk_oddmr_write(comp, 1, DISP_ODDMR_REG_SPR_COMP_EN, pkg);
	mtk_oddmr_write(comp, 0, DISP_ODDMR_TOP_S2R_BYPASS, pkg);
}

static void mtk_oddmr_set_spr2rgb_dual(struct cmdq_pkt *pkg)
{
	if (g_oddmr_priv->spr_enable == 0 || g_oddmr_priv->spr_relay == 1)
		return;
	mtk_oddmr_set_spr2rgb(default_comp, pkg);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_set_spr2rgb(oddmr1_default_comp, pkg);
}

static void mtk_oddmr_start(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	ODDMRLOW_LOG("%s oddmr_start\n", mtk_dump_comp_str(comp));
}

static void mtk_oddmr_stop(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);

	ODDMRFLOW_LOG("%s oddmr_stop\n", mtk_dump_comp_str(comp));
	oddmr_priv->qos_srt_dmrr = 0;
	oddmr_priv->qos_srt_odr = 0;
	oddmr_priv->qos_srt_odw = 0;
}

static void mtk_oddmr_od_clk_en(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	uint32_t value = 0, mask = 0;

	value = 0x0880; mask = 0x0880;
	SET_VAL_MASK(value, mask, 1, REG_OD_CLK_EN);
	mtk_oddmr_write_mask(comp, value,
			DISP_ODDMR_OD_UDMA_CTR_0, mask, NULL);
}
static void mtk_oddmr_od_prepare(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);

	ODDMRFLOW_LOG("%s+\n", mtk_dump_comp_str(comp));
	if (oddmr_priv->data->is_od_need_force_clk)
		mtk_oddmr_od_hsk_force_clk(comp, NULL);
	if (oddmr_priv->data->is_od_need_crop_garbage == true) {
		mtk_oddmr_write_cpu(comp, 0, DISP_ODDMR_CRP_CTR_0);
		mtk_oddmr_write_cpu(comp, 0, DISP_ODDMR_CRP_CTR_1);
		mtk_oddmr_write_cpu(comp, 0, DISP_ODDMR_TOP_CRP_BYPSS);
		udelay(1);
	}
	mtk_oddmr_od_clk_en(comp, NULL);
	//crop off, add for first boot
	if (comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_set_crop(comp, NULL);
	else
		mtk_oddmr_write(comp, 1, DISP_ODDMR_TOP_CRP_BYPSS, NULL);
}

static void mtk_oddmr_dmr_prepare(struct mtk_ddp_comp *comp)
{
	mtk_oddmr_write_cpu(comp, 1,
			DISP_ODDMR_REG_DMR_EN);
}
static void mtk_oddmr_spr2rgb_prepare(struct mtk_ddp_comp *comp)
{
	mtk_oddmr_write_cpu(comp, 1,
			DISP_ODDMR_REG_SPR_COMP_EN);
}

static void mtk_oddmr_top_prepare(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	uint32_t value = 0, mask = 0;

	ODDMRAPI_LOG("+\n");
	SET_VAL_MASK(value, mask, 1, REG_ODDMR_TOP_CLK_FORCE_EN);
	SET_VAL_MASK(value, mask, 1, REG_FORCE_COMMIT);
	SET_VAL_MASK(value, mask, 1, REG_BYPASS_SHADOW);
	mtk_oddmr_write_mask(comp, value,
			DISP_ODDMR_TOP_CTR_3, mask, handle);

}

static void mtk_oddmr_prepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);

	ODDMRAPI_LOG("+\n");
	mtk_ddp_comp_clk_prepare(comp);
	if (is_oddmr_od_support || is_oddmr_dmr_support)
		mtk_oddmr_top_prepare(comp, NULL);
	if (is_oddmr_od_support)
		mtk_oddmr_od_prepare(comp, NULL);
	if (is_oddmr_dmr_support)
		mtk_oddmr_dmr_prepare(comp);

	if ((comp->mtk_crtc->panel_ext->params->spr_params.enable == 1) &&
			(comp->mtk_crtc->panel_ext->params->spr_params.relay == 0) &&
			(is_oddmr_od_support || is_oddmr_dmr_support))
		mtk_oddmr_spr2rgb_prepare(comp);
	atomic_set(&oddmr_priv->oddmr_clock_ref, 1);
}

static void mtk_oddmr_unprepare(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	unsigned long flags;

	ODDMRAPI_LOG("+\n");
	spin_lock_irqsave(&g_oddmr_clock_lock, flags);
	atomic_dec(&oddmr_priv->oddmr_clock_ref);
	while (atomic_read(&oddmr_priv->oddmr_clock_ref) > 0) {
		spin_unlock_irqrestore(&g_oddmr_clock_lock, flags);
		ODDMRFLOW_LOG("waiting for oddmr_lock, %d\n",
				atomic_read(&oddmr_priv->oddmr_clock_ref));
		usleep_range(50, 100);
		spin_lock_irqsave(&g_oddmr_clock_lock, flags);
	}
	spin_unlock_irqrestore(&g_oddmr_clock_lock, flags);
	mtk_ddp_comp_clk_unprepare(comp);
}

#ifdef IF_ZERO
static void mtk_disp_oddmr_config_overhead(struct mtk_ddp_comp *comp, struct mtk_ddp_config *cfg)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	uint32_t comp_overhead;

	if (cfg->tile_overhead.is_support) {
		//TODO: dmr blocksize
		if (mtk_crtc->is_dual_pipe)
			comp_overhead = oddmr_priv->data->tile_overhead;
		else
			comp_overhead = 0;
		if (oddmr_priv->is_right_pipe) {
			cfg->tile_overhead.right_in_width += comp_overhead;
			cfg->tile_overhead.right_overhead += comp_overhead;
			oddmr_priv->cfg.comp_overhead = comp_overhead;
			oddmr_priv->cfg.comp_in_width = cfg->tile_overhead.right_in_width;
			oddmr_priv->cfg.total_overhead = cfg->tile_overhead.right_overhead;
		} else {
			cfg->tile_overhead.left_in_width += comp_overhead;
			cfg->tile_overhead.left_overhead += comp_overhead;
			oddmr_priv->cfg.comp_overhead = comp_overhead;
			oddmr_priv->cfg.comp_in_width = cfg->tile_overhead.left_in_width;
			oddmr_priv->cfg.total_overhead = cfg->tile_overhead.left_overhead;
		}
	}
}
#endif

static void mtk_oddmr_first_cfg(struct mtk_ddp_comp *comp,
		struct mtk_ddp_config *cfg, struct cmdq_pkt *handle)
{
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	struct mtk_ddp_comp *out_comp = NULL;
	unsigned long flags;
	int crtc_idx;

	DDPMSG("%s+\n", __func__);
	is_oddmr_dmr_support = comp->mtk_crtc->panel_ext->params->is_support_dmr;
	is_oddmr_od_support = comp->mtk_crtc->panel_ext->params->is_support_od;
	/* get spr status */
	oddmr_priv->spr_enable = comp->mtk_crtc->panel_ext->params->spr_params.enable;
	oddmr_priv->spr_relay = comp->mtk_crtc->panel_ext->params->spr_params.relay;
	oddmr_priv->spr_format = comp->mtk_crtc->panel_ext->params->spr_params.spr_format_type;
	/* read panelid */
	crtc_idx = drm_crtc_index(&mtk_crtc->base);
	if (crtc_idx == 0)
		out_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (out_comp && (is_oddmr_dmr_support || is_oddmr_od_support))
		out_comp->funcs->io_cmd(out_comp, NULL, DSI_READ_PANELID, &g_panelid);
	spin_lock_irqsave(&g_oddmr_timing_lock, flags);
	mtk_oddmr_drm_mode_to_oddmr_timg(mtk_crtc, &g_oddmr_current_timing);
	spin_unlock_irqrestore(&g_oddmr_timing_lock, flags);
	mtk_oddmr_fill_cfg(comp, cfg);
	if (is_oddmr_dmr_support || is_oddmr_od_support)
		mtk_oddmr_set_spr2rgb(comp, handle);

	DDPMSG("%s dmr %d od %d-\n", __func__, is_oddmr_dmr_support, is_oddmr_od_support);
}

static void mtk_oddmr_config(struct mtk_ddp_comp *comp,
		struct mtk_ddp_config *cfg,
		struct cmdq_pkt *handle)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
	struct mtk_drm_crtc *mtk_crtc = comp->mtk_crtc;
	struct cmdq_pkt *cmdq_handle0 = NULL;
	struct cmdq_pkt *cmdq_handle1 = NULL;
	struct cmdq_client *client = NULL;

	if (!comp->mtk_crtc || !comp->mtk_crtc->panel_ext || g_oddmr_priv == NULL) {
		ODDMRFLOW_LOG("comp is invalid-\n");
		return;
	}
	ODDMRFLOW_LOG("%s+\n", mtk_dump_comp_str(comp));
	if (mtk_crtc->gce_obj.client[CLIENT_PQ])
		client = mtk_crtc->gce_obj.client[CLIENT_PQ];
	else
		client = mtk_crtc->gce_obj.client[CLIENT_CFG];

	mtk_oddmr_fill_cfg(comp, cfg);

	if (is_oddmr_od_support == true && g_oddmr_priv->od_state == ODDMR_INIT_DONE) {
		//crop off
		if (comp->mtk_crtc->is_dual_pipe)
			mtk_oddmr_set_crop(comp, NULL);
		else
			mtk_oddmr_write(comp, 1, DISP_ODDMR_TOP_CRP_BYPSS, NULL);
		//write sram
		cmdq_mbox_enable(client->chan);
		cmdq_handle0 = oddmr_priv->od_data.od_sram_pkgs[0];
		cmdq_handle1 = oddmr_priv->od_data.od_sram_pkgs[1];
		if (cmdq_handle0) {
			CRTC_MMP_MARK(0, oddmr_ctl, comp->id, 0);
			cmdq_pkt_refinalize(cmdq_handle0);
			CRTC_MMP_MARK(0, oddmr_ctl, comp->id, 1);
			cmdq_pkt_flush(cmdq_handle0);
			CRTC_MMP_MARK(0, oddmr_ctl, comp->id, 2);
		}
		if (cmdq_handle1) {
			CRTC_MMP_MARK(0, oddmr_ctl, comp->id, 3);
			cmdq_pkt_refinalize(cmdq_handle1);
			CRTC_MMP_MARK(0, oddmr_ctl, comp->id, 4);
			cmdq_pkt_flush(cmdq_handle1);
			CRTC_MMP_MARK(0, oddmr_ctl, comp->id, 5);
		}
		cmdq_mbox_disable(client->chan);
		if (oddmr_priv->od_data.od_sram_read_sel == 1)
			mtk_oddmr_write(comp, 0x20, DISP_ODDMR_OD_SRAM_CTRL_0, NULL);
		else
			mtk_oddmr_write(comp, 0x10, DISP_ODDMR_OD_SRAM_CTRL_0, NULL);
		if (!(g_oddmr_priv->spr_enable == 0 || g_oddmr_priv->spr_relay == 1))
			mtk_oddmr_set_spr2rgb(comp, NULL);
		mtk_oddmr_od_smi(comp, NULL);
		mtk_oddmr_od_set_dram(comp, NULL);
		mtk_oddmr_od_common_init(comp, NULL);
		mtk_oddmr_od_set_res(comp, NULL);
		mtk_oddmr_od_init_end(comp, NULL);
		mtk_oddmr_set_od_enable(comp, oddmr_priv->od_enable, handle);
		//sw bypass first frame od pq
		if ((oddmr_priv->data->is_od_support_hw_skip_first_frame == false) &&
				(is_oddmr_od_support == true) &&
				(oddmr_priv->od_enable == 1)) {
			mtk_oddmr_write_mask(comp, 0, DISP_ODDMR_OD_PQ_0,
					REG_FLD_MASK(REG_OD_USER_WEIGHT), handle);
			atomic_set(&g_oddmr_od_weight_trigger, 1);
		}
	}
}

static void mtk_oddmr_dump_od_table(int table_idx)
{
	struct mtk_oddmr_od_table *table = g_od_param.od_tables[table_idx];
	struct mtk_oddmr_od_table_basic_info *info = &table->table_basic_info;
	int i;

	DDPDUMP("OD Table%d\n", table_idx);
	DDPDUMP("%u x %u fps: %u(%d-%d) dbv 0x%x(0x%x-0x%x)\n",
			info->width, info->height,
			info->fps, info->min_fps, info->max_fps,
			info->dbv, info->min_dbv, info->max_dbv);
	DDPDUMP("fps cnt %d\n", table->fps_cnt);
	for (i = 0; i < table->fps_cnt && i < OD_GAIN_MAX; i++) {
		DDPDUMP("(%d, %d) ",
				table->fps_table[i].item, table->fps_table[i].value);
	}
	DDPDUMP("\n");
	DDPDUMP("dbv cnt %d\n", table->bl_cnt);
	for (i = 0; i < table->bl_cnt && i < OD_GAIN_MAX; i++) {
		DDPDUMP("(%d, %d) ",
				table->bl_table[i].item, table->bl_table[i].value);
	}
	DDPDUMP("\n");
}

static void mtk_oddmr_dump_od_param(void)
{
	struct mtk_oddmr_od_basic_param *basic_param =
		&g_od_param.od_basic_info.basic_param;

	DDPDUMP("OD Basic info:\n");
	DDPDUMP("valid_table 0x%x, valid_table_cnt %d\n",
			g_od_param.valid_table, g_od_param.valid_table_cnt);
	DDPDUMP("res_switch_mode %d, %u x %u, table cnts %d, od_mode %d\n",
			basic_param->resolution_switch_mode,
			basic_param->panel_width, basic_param->panel_height,
			basic_param->table_cnt, basic_param->od_mode);
	DDPDUMP("dither_sel %d scaling_mode 0x%x\n",
			basic_param->dither_sel, basic_param->scaling_mode);
	if (IS_TABLE_VALID_LOW(0, g_od_param.valid_table))
		mtk_oddmr_dump_od_table(0);
	if (IS_TABLE_VALID_LOW(1, g_od_param.valid_table))
		mtk_oddmr_dump_od_table(1);
}

static void mtk_oddmr_dump_dmr_table(int table_idx)
{
	struct mtk_oddmr_dmr_table *table = g_dmr_param.dmr_tables[table_idx];
	struct mtk_oddmr_dmr_table_basic_info *info = &table->table_basic_info;
	int i;

	DDPDUMP("DMR Table%d\n", table_idx);
	DDPDUMP("%u x %u fps: %u(%d-%d) dbv 0x%x(0x%x-0x%x)\n",
			info->width, info->height,
			info->fps, info->min_fps, info->max_fps,
			info->dbv, info->min_dbv, info->max_dbv);
	DDPDUMP("fps cnt %d\n", table->fps_cnt);
	for (i = 0; i < table->fps_cnt && i < DMR_GAIN_MAX; i++) {
		DDPDUMP("(%d, 0x%x 0x%x 0x%x) ",
				table->fps_table[i].fps, table->fps_table[i].beta,
				table->fps_table[i].gain, table->fps_table[i].offset);
	}
	DDPDUMP("\n");
	DDPDUMP("dbv cnt %d\n", table->bl_cnt);
	for (i = 0; i < table->bl_cnt && i < DMR_GAIN_MAX; i++) {
		DDPDUMP("(%d, %d) ",
				table->bl_table[i].item, table->bl_table[i].value);
	}
	DDPDUMP("\n");
}

static void mtk_oddmr_dump_dmr_param(void)
{
	struct mtk_oddmr_dmr_basic_param *basic_param =
		&g_dmr_param.dmr_basic_info.basic_param;

	DDPDUMP("DMR Basic info:\n");
	DDPDUMP("valid_table 0x%x, valid_table_cnt %d\n",
			g_dmr_param.valid_table, g_dmr_param.valid_table_cnt);
	DDPDUMP("res_switch_mode %d, %u x %u, table cnts %d, table mode %d\n",
			basic_param->resolution_switch_mode,
			basic_param->panel_width, basic_param->panel_height,
			basic_param->table_cnt, basic_param->dmr_table_mode);
	DDPDUMP("dither_sel %d is_second_dmr 0x%x\n",
			basic_param->dither_sel, basic_param->is_second_dmr);
	if (IS_TABLE_VALID_LOW(0, g_dmr_param.valid_table))
		mtk_oddmr_dump_dmr_table(0);
	if (IS_TABLE_VALID_LOW(1, g_dmr_param.valid_table))
		mtk_oddmr_dump_dmr_table(1);
}

int mtk_oddmr_analysis(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);

	DDPDUMP("== %s ANALYSIS:0x%x ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	DDPDUMP("od_support %d dmr_support %d\n", is_oddmr_od_support, is_oddmr_dmr_support);
	DDPDUMP("od_en %d, dmr_en %d, od_state %d, dmr_state %d\n",
			oddmr_priv->od_enable,
			oddmr_priv->dmr_enable,
			oddmr_priv->od_state,
			oddmr_priv->dmr_state);
	DDPDUMP("oddmr current bl %u, fps %u %u x %u\n",
			g_oddmr_current_timing.bl_level,
			g_oddmr_current_timing.vrefresh,
			g_oddmr_current_timing.hdisplay,
			g_oddmr_current_timing.vdisplay);
	DDPDUMP("OD: r_sel %d, sram0 %d, sram1 %d\n",
			oddmr_priv->od_data.od_sram_read_sel,
			oddmr_priv->od_data.od_sram_table_idx[0],
			oddmr_priv->od_data.od_sram_table_idx[1]);
	mtk_oddmr_dump_od_param();
	DDPDUMP("DMR: cur_table %d\n",
			atomic_read(&oddmr_priv->dmr_data.cur_table_idx));
	mtk_oddmr_dump_dmr_param();
	return 0;
}

void mtk_oddmr_dump(struct mtk_ddp_comp *comp)
{
	void __iomem *baddr = comp->regs;
	void __iomem *mbaddr;
	int i;

	if (g_oddmr_dump_en == false)
		return;
	DDPDUMP("== %s REGS:0x%x ==\n", mtk_dump_comp_str(comp), comp->regs_pa);
	DDPDUMP("-- Start dump oddmr registers --\n");
	mbaddr = baddr;
	DDPDUMP("ODDMR_TOP: 0x%p\n", mbaddr);
	for (i = 0; i < 0x200; i += 16) {
		DDPDUMP("ODDMR_TOP+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
				readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
				readl(mbaddr + i + 0xc));
	}

	mbaddr = baddr + DISP_ODDMR_REG_VGS_BASE;
	DDPDUMP("ODDMR_VGS: 0x%p\n", mbaddr);
	for (i = 0; i < 0x1FC; i += 16) {
		DDPDUMP("ODDMR_VGS+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
				readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
				readl(mbaddr + i + 0xc));
	}

	mbaddr = baddr + DISP_ODDMR_REG_DMR_BASE;
	DDPDUMP("ODDMR_DMR: 0x%p\n", mbaddr);
	for (i = 0; i < 0x1B0; i += 16) {
		DDPDUMP("ODDMR_DMR+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
				readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
				readl(mbaddr + i + 0xc));
	}

	mbaddr = baddr + DISP_ODDMR_REG_UDMA_DMR_BASE;
	DDPDUMP("ODDMR_UDMA_DMR: 0x%p\n", mbaddr);
	for (i = 0; i < 0x200; i += 16) {
		DDPDUMP("ODDMR_UDMA_DMR+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
				readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
				readl(mbaddr + i + 0xc));
	}

	mbaddr = baddr + DISP_ODDMR_REG_OD_BASE;
	DDPDUMP("ODDMR_OD: 0x%p\n", mbaddr);
	for (i = 0; i < 0x1FC; i += 16) {
		DDPDUMP("ODDMR_OD+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
				readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
				readl(mbaddr + i + 0xc));
	}

	mbaddr = baddr + DISP_ODDMR_REG_SMI_BASE;
	DDPDUMP("ODDMR_SMI: 0x%p\n", mbaddr);
	for (i = 0; i < 0x1E0; i += 16) {
		DDPDUMP("ODDMR_SMI+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
				readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
				readl(mbaddr + i + 0xc));
	}

	mbaddr = baddr + DISP_ODDMR_REG_SPR2RGB_BASE;
	DDPDUMP("ODDMR_SPR2RGB: 0x%p\n", mbaddr);
	for (i = 0; i < 0x2C; i += 16) {
		DDPDUMP("ODDMR_SPR2RGB+%x: 0x%x 0x%x 0x%x 0x%x\n", i, readl(mbaddr + i),
				readl(mbaddr + i + 0x4), readl(mbaddr + i + 0x8),
				readl(mbaddr + i + 0xc));
	}
}

void mtk_disp_oddmr_debug(const char *opt)
{
	ODDMRFLOW_LOG(":%s\n", opt);
	if (strncmp(opt, "flow_log:", 9) == 0) {
		debug_flow_log = strncmp(opt + 9, "1", 1) == 0;
		ODDMRFLOW_LOG("debug_flow_log = %d\n", debug_flow_log);
	} else if (strncmp(opt, "api_log:", 8) == 0) {
		debug_api_log = strncmp(opt + 8, "1", 1) == 0;
		ODDMRFLOW_LOG("debug_api_log = %d\n", debug_api_log);
	} else if (strncmp(opt, "low_log:", 8) == 0) {
		debug_low_log = strncmp(opt + 8, "1", 1) == 0;
		ODDMRFLOW_LOG("debug_low_log = %d\n", debug_low_log);
	} else if (strncmp(opt, "dump_en:", 8) == 0) {
		g_oddmr_dump_en = strncmp(opt + 8, "1", 1) == 0;
		ODDMRFLOW_LOG("g_oddmr_dump_en = %d\n", g_oddmr_dump_en);
	} else if (strncmp(opt, "ddren:", 6) == 0) {
		g_oddmr_ddren = strncmp(opt + 6, "1", 1) == 0;
		ODDMRFLOW_LOG("g_oddmr_ddren = %d\n", g_oddmr_ddren);
	} else if (strncmp(opt, "hrt:", 4) == 0) {
		g_oddmr_hrt_en = strncmp(opt + 4, "1", 1) == 0;
		ODDMRFLOW_LOG("g_oddmr_hrt_en = %d\n", g_oddmr_hrt_en);
	} else if (strncmp(opt, "sof_stop", 8) == 0) {
		kthread_stop(oddmr_sof_irq_event_task);
		ODDMRFLOW_LOG("sof_stop\n");
	} else if (strncmp(opt, "check_trigger:", 14) == 0) {
		unsigned int on, ret;

		ret = sscanf(opt, "check_trigger:%u\n", &on);
		if (ret != 1) {
			ODDMRFLOW_LOG("error to parse cmd %s\n", opt);
			return;
		}
		ODDMRFLOW_LOG("check_trigger = %u\n", on);
		atomic_set(&g_od_check_trigger, on);
	} else if (strncmp(opt, "debugdump:", 10) == 0) {
		ODDMRFLOW_LOG("debug_flow_log = %d\n", debug_flow_log);
		ODDMRFLOW_LOG("debug_api_log = %d\n", debug_api_log);
		ODDMRFLOW_LOG("debug_low_log = %d\n", debug_low_log);
	}
}

static inline int mtk_oddmr_gain_interpolation(int left_item,
		int tmp_item, int right_item, int left_value, int right_value)
{
	int result;

	result = (100 * (tmp_item - left_item) / (right_item - left_item) *
			(right_value - left_value) + 100 * left_value)/100;
	return result;
}

static void mtk_oddmr_od_free_buffer(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_oddmr *priv = comp_to_oddmr(comp);

	ODDMRAPI_LOG("+\n");
	if (priv->od_data.r_channel != NULL) {
		mtk_drm_gem_free_object(&priv->od_data.r_channel->base);
		priv->od_data.r_channel = NULL;
	}
	if (priv->od_data.g_channel != NULL) {
		mtk_drm_gem_free_object(&priv->od_data.r_channel->base);
		priv->od_data.r_channel = NULL;
	}
	if (priv->od_data.r_channel != NULL) {
		mtk_drm_gem_free_object(&priv->od_data.r_channel->base);
		priv->od_data.b_channel = NULL;
	}
}
/* channel: 0:B 1:G 2:R */
static int mtk_oddmr_od_get_bpc(uint32_t od_mode, uint32_t channel)
{
	int bpc = 0;

	ODDMRAPI_LOG("+\n");
	switch (od_mode) {
	case OD_MODE_TYPE_RGB444:
	case OD_MODE_TYPE_COMPRESS_12:
		bpc = 4;
		break;
	case OD_MODE_TYPE_RGB565:
		if (channel == 1)
			bpc = 6;
		else
			bpc = 5;
		break;
	case OD_MODE_TYPE_COMPRESS_18:
		bpc = 6;
		break;
	case OD_MODE_TYPE_RGB555:
		bpc = 5;
		break;
	case OD_MODE_TYPE_RGB888:
		bpc = 8;
		break;
	default:
		bpc = 4;
		break;
	}
	return bpc;
}

/*
 * hscaling : 1,2,4; vscaling: 1,2
 * Width = align_up(Width, 8)
 * channel size = align_up(Width / hscaling x bpc, 128) x Height / vscaling / 8
 */
static uint32_t mtk_oddmr_od_get_dram_size(uint32_t width, uint32_t height,
		uint32_t scaling_mode, uint32_t od_mode, uint32_t channel)
{
	uint32_t hscaling, h_2x4x_sel, vscaling, bpc, size, width_up;

	ODDMRAPI_LOG("+\n");
	bpc = mtk_oddmr_od_get_bpc(od_mode, channel);
	hscaling = (scaling_mode & BIT(0)) > 0 ? 2 : 1;
	vscaling = (scaling_mode & BIT(1)) > 0 ? 2 : 1;
	h_2x4x_sel = (scaling_mode & BIT(2)) > 0 ? 2 : 1;
	hscaling = hscaling * h_2x4x_sel;
	width_up = DIV_ROUND_UP(width, 8) * 8;
	size = width / hscaling * bpc;
	size = DIV_ROUND_UP(size, OD_H_ALIGN_BITS);
	size = size * OD_H_ALIGN_BITS * height / vscaling / 8;

	ODDMRFLOW_LOG("width %u width_up %u hscaling %u vscaling %u bpc %u\n",
		width, width_up, hscaling, vscaling, bpc);
	ODDMRFLOW_LOG("table mode %u channel %u dram size %u\n", od_mode, channel, size);
	return size;
}

static void mtk_oddmr_od_alloc_dram_dual(void)
{

	uint32_t scaling_mode, size_b, size_g, size_r, width = 0, height = 0, od_mode;
	int tile_overhead = 0, ret;
	bool secu;

	ODDMRAPI_LOG("+\n");
	if (default_comp == NULL || g_oddmr_priv == NULL) {
		DDPPR_ERR("%s comp is invalid\n", __func__);
		return;
	}
	ret = mtk_drm_crtc_get_panel_original_size(&default_comp->mtk_crtc->base,
				&width, &height);
	if (ret < 0 || width == 0 || height == 0) {
		ODDMRFLOW_LOG("panel original size error(%ux%u).\n", width, height);
		width = g_od_param.od_basic_info.basic_param.panel_width;
		height = g_od_param.od_basic_info.basic_param.panel_height;
	}
	scaling_mode = g_od_param.od_basic_info.basic_param.scaling_mode;
	od_mode = g_od_param.od_basic_info.basic_param.od_mode;
	tile_overhead = g_oddmr_priv->cfg.comp_overhead;
	ODDMRFLOW_LOG("%ux%u tile_overhead %d scaling_mode 0x%x od_mode %u\n",
		width, height, tile_overhead, scaling_mode, od_mode);

	secu = mtk_oddmr_is_svp_on_mtee();
	//od do not support secu for short of secu mem
	if (g_oddmr_priv->data != NULL && !g_oddmr_priv->data->is_od_support_sec)
		secu = false;
	//TODO check size, should not be too big
	if (default_comp->mtk_crtc->is_dual_pipe) {
		size_b = mtk_oddmr_od_get_dram_size((width/2 + tile_overhead),
				height, scaling_mode, od_mode, 0);
		size_g = mtk_oddmr_od_get_dram_size((width/2 + tile_overhead),
				height, scaling_mode, od_mode, 1);
		size_r = mtk_oddmr_od_get_dram_size((width/2 + tile_overhead),
				height, scaling_mode, od_mode, 2);
		/* non secure */
		mtk_oddmr_od_free_buffer(default_comp);
		mtk_oddmr_od_free_buffer(oddmr1_default_comp);
		g_oddmr_priv->od_data.b_channel =
			mtk_oddmr_load_buffer(&default_comp->mtk_crtc->base, size_b, NULL, secu);
		g_oddmr_priv->od_data.g_channel =
			mtk_oddmr_load_buffer(&default_comp->mtk_crtc->base, size_g, NULL, secu);
		g_oddmr_priv->od_data.r_channel =
			mtk_oddmr_load_buffer(&default_comp->mtk_crtc->base, size_r, NULL, secu);
		g_oddmr1_priv->od_data.b_channel =
			mtk_oddmr_load_buffer(&default_comp->mtk_crtc->base, size_b, NULL, secu);
		g_oddmr1_priv->od_data.g_channel =
			mtk_oddmr_load_buffer(&default_comp->mtk_crtc->base, size_g, NULL, secu);
		g_oddmr1_priv->od_data.r_channel =
			mtk_oddmr_load_buffer(&default_comp->mtk_crtc->base, size_r, NULL, secu);
	} else {
		size_b = mtk_oddmr_od_get_dram_size(width, height, scaling_mode, od_mode, 0);
		size_g = mtk_oddmr_od_get_dram_size(width, height, scaling_mode, od_mode, 1);
		size_r = mtk_oddmr_od_get_dram_size(width, height, scaling_mode, od_mode, 2);
		mtk_oddmr_od_free_buffer(default_comp);
		g_oddmr_priv->od_data.b_channel =
			mtk_oddmr_load_buffer(&default_comp->mtk_crtc->base, size_b, NULL, secu);
		g_oddmr_priv->od_data.g_channel =
			mtk_oddmr_load_buffer(&default_comp->mtk_crtc->base, size_g, NULL, secu);
		g_oddmr_priv->od_data.r_channel =
			mtk_oddmr_load_buffer(&default_comp->mtk_crtc->base, size_r, NULL, secu);
	}
}

static void mtk_oddmr_od_set_dram(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	dma_addr_t addr = 0;
	struct mtk_disp_oddmr *priv = comp_to_oddmr(comp);

	ODDMRAPI_LOG("+\n");
	if ((priv->od_data.r_channel != NULL) &&
			(priv->od_data.g_channel != NULL) &&
			(priv->od_data.b_channel != NULL)) {
		addr = priv->od_data.r_channel->dma_addr;
		mtk_oddmr_write_mask(comp, addr >> 4, DISP_ODDMR_OD_BASE_ADDR_R_LSB,
				REG_FLD_MASK(REG_OD_BASE_ADDR_R_LSB), pkg);
		mtk_oddmr_write_mask(comp, addr >> 20, DISP_ODDMR_OD_BASE_ADDR_R_MSB,
				REG_FLD_MASK(REG_OD_BASE_ADDR_R_MSB), pkg);
		addr = priv->od_data.g_channel->dma_addr;
		mtk_oddmr_write_mask(comp, addr >> 4, DISP_ODDMR_OD_BASE_ADDR_G_LSB,
				REG_FLD_MASK(REG_OD_BASE_ADDR_G_LSB), pkg);
		mtk_oddmr_write_mask(comp, addr >> 20, DISP_ODDMR_OD_BASE_ADDR_G_MSB,
				REG_FLD_MASK(REG_OD_BASE_ADDR_G_MSB), pkg);
		addr = priv->od_data.b_channel->dma_addr;
		mtk_oddmr_write_mask(comp, addr >> 4, DISP_ODDMR_OD_BASE_ADDR_B_LSB,
				REG_FLD_MASK(REG_OD_BASE_ADDR_B_LSB), pkg);
		mtk_oddmr_write_mask(comp, addr >> 20, DISP_ODDMR_OD_BASE_ADDR_B_MSB,
				REG_FLD_MASK(REG_OD_BASE_ADDR_B_MSB), pkg);
	} else {
		DDPPR_ERR("%s buffer is invalid\n", __func__);
	}
}

static void mtk_oddmr_od_set_dram_dual(struct cmdq_pkt *pkg)
{
	ODDMRAPI_LOG("+\n");
	mtk_oddmr_od_set_dram(default_comp, pkg);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_od_set_dram(oddmr1_default_comp, pkg);
}

static int mtk_oddmr_od_init_sram(struct mtk_ddp_comp *comp,
		struct cmdq_pkt *pkg, int table_idx, int sram_idx)
{
	struct mtk_oddmr_od_table *table;
	struct mtk_oddmr_pq_pair *param_pq;
	uint8_t *raw_table, tmp_data;
	int channel, srams, cols, rows, raw_idx, i;
	uint32_t value, mask, tmp_r_sel, tmp_w_sel, table_size;

	ODDMRAPI_LOG("+\n");
	if (!IS_TABLE_VALID(table_idx, g_od_param.valid_table)) {
		DDPPR_ERR("%s table %d is invalid\n", __func__, table_idx);
		return -EFAULT;
	}
	table = g_od_param.od_tables[table_idx];
	param_pq = table->pq_od.param;
	raw_table = table->raw_table.value;
	table_size = table->raw_table.size;
	raw_idx = 0;
	if (table_size < 33 * 33 * 3) {
		DDPPR_ERR("%s table%d size %u is too small\n", __func__, table_idx, table_size);
		return -EFAULT;
	}
	for (i = 0; i < table->pq_od.counts; i++)
		mtk_oddmr_write(comp, param_pq[i].value, param_pq[i].addr, pkg);

	/* B:0-bit1, G:1-bit2, R:2-bit3*/
	for (channel = 0; channel < 3; channel++) {
		mtk_oddmr_write(comp, 0, DISP_ODDMR_OD_SRAM_CTRL_0, pkg);
		/* 1: 17x17 2:16x17 3:17x16 4:16x16*/
		for (srams = 1; srams < 5; srams++) {
			value = 0;
			mask = 0;
			tmp_w_sel = sram_idx;
			tmp_r_sel = !sram_idx;
			SET_VAL_MASK(value, mask, 1 << (channel + 1), REG_WBGR_OD_SRAM_IO_EN);
			SET_VAL_MASK(value, mask, 0, REG_AUTO_SRAM_ADR_INC_EN);
			SET_VAL_MASK(value, mask, tmp_w_sel, REG_OD_SRAM_WRITE_SEL);
			SET_VAL_MASK(value, mask, tmp_r_sel, REG_OD_SRAM_READ_SEL);
			mtk_oddmr_write_mask(comp, value, DISP_ODDMR_OD_SRAM_CTRL_0, mask, pkg);
			rows = (srams < 3) ? 17 : 16;
			cols = (srams % 2 == 1) ? 17 : 16;
			ODDMRFLOW_LOG("channel%d sram%d size %dx%d\n", channel, srams, rows, cols);
			for (i = 0; i < rows * cols; i++) {
				tmp_data = raw_table[raw_idx];
				raw_idx++;
				mtk_oddmr_write(comp, tmp_data,
					(DISP_ODDMR_OD_SRAM_CTRL_2 + 12 * (srams - 1)), pkg);
				mtk_oddmr_write(comp, 0x8000 | (i & 0x1FF),
					(DISP_ODDMR_OD_SRAM_CTRL_1 + 12 * (srams - 1)), pkg);
			}
			value = 0;
			mask = 0;
			SET_VAL_MASK(value, mask, 0, REG_WBGR_OD_SRAM_IO_EN);
			SET_VAL_MASK(value, mask, tmp_w_sel, REG_OD_SRAM_WRITE_SEL);
			SET_VAL_MASK(value, mask, tmp_r_sel, REG_OD_SRAM_READ_SEL);
			SET_VAL_MASK(value, mask, 0, REG_AUTO_SRAM_ADR_INC_EN);
			mtk_oddmr_write_mask(comp, value, DISP_ODDMR_OD_SRAM_CTRL_0, mask, pkg);
		}
		//SET_VAL_MASK(value, mask, 0, REG_FLD(1, channel + 1));
		//mtk_oddmr_write_mask(comp, value, DISP_ODDMR_OD_SRAM_CTRL_0, mask, pkg);
		mtk_oddmr_write(comp, 0, DISP_ODDMR_OD_SRAM_CTRL_0, pkg);
	}
	return 0;
}

int mtk_oddmr_od_init_sram_dual(struct cmdq_pkt *pkg, int table_idx, int sram_idx)
{
	int ret;

	ret = mtk_oddmr_od_init_sram(default_comp, pkg, table_idx, sram_idx);
	if (ret >= 0)
		g_oddmr_priv->od_data.od_sram_table_idx[sram_idx] = table_idx;
	if (default_comp->mtk_crtc->is_dual_pipe) {
		ret = mtk_oddmr_od_init_sram(oddmr1_default_comp, pkg, table_idx, sram_idx);
		if (ret >= 0)
			g_oddmr1_priv->od_data.od_sram_table_idx[sram_idx] = table_idx;
	}
	ODDMRAPI_LOG("sram_idx %d, table_idx %d\n",
			sram_idx, g_oddmr_priv->od_data.od_sram_table_idx[sram_idx]);
	return ret;
}

static int _mtk_oddmr_od_table_lookup(struct mtk_oddmr_timing *cur_timing)
{
	int idx;
	unsigned int fps = cur_timing->vrefresh;
	uint32_t cnts = g_od_param.od_basic_info.basic_param.table_cnt;

	for (idx = 0; idx < cnts; idx++) {
		if ((IS_TABLE_VALID(idx, g_od_param.valid_table)) &&
				(fps >= g_od_param.od_tables[idx]->table_basic_info.min_fps) &&
				(fps <= g_od_param.od_tables[idx]->table_basic_info.max_fps))
			break;
	}
	if ((idx == cnts) ||
			!IS_TABLE_VALID(idx, g_od_param.valid_table))
		idx = -1;
	ODDMRFLOW_LOG("table_idx %d\n", idx);
	return idx;
}
/*
 *table_idx: output table_idx found
 *return 0: state cur sram, 1: flip sram, 2: update sram table
 */
static int mtk_oddmr_od_table_lookup(struct mtk_disp_oddmr *priv,
		struct mtk_oddmr_timing *cur_timing, int *table_idx)
{
	int tmp_table_idx, tmp1_table_idx, tmp_sram_idx, ret;

	ODDMRAPI_LOG("fps %u\n", cur_timing->vrefresh);
	if (priv->od_data.od_sram_read_sel < 0 ||
			priv->od_data.od_sram_table_idx[0] < 0) {
		DDPPR_ERR("%s od is not init properly\n", __func__);
		return -EFAULT;
	}
	tmp_sram_idx = priv->od_data.od_sram_read_sel;
	tmp_table_idx = priv->od_data.od_sram_table_idx[!!tmp_sram_idx];
	tmp1_table_idx = priv->od_data.od_sram_table_idx[!tmp_sram_idx];
	/* best stay at current table */
	if ((IS_TABLE_VALID(tmp_table_idx, g_od_param.valid_table)) &&
	(cur_timing->vrefresh >= g_od_param.od_tables[tmp_table_idx]->table_basic_info.min_fps) &&
	(cur_timing->vrefresh <= g_od_param.od_tables[tmp_table_idx]->table_basic_info.max_fps)) {
		*table_idx = tmp_table_idx;
		return 0;
	}
	/* second best just flip sram */
	if ((IS_TABLE_VALID(tmp1_table_idx, g_od_param.valid_table)) &&
	(cur_timing->vrefresh >= g_od_param.od_tables[tmp1_table_idx]->table_basic_info.min_fps) &&
	(cur_timing->vrefresh <= g_od_param.od_tables[tmp1_table_idx]->table_basic_info.max_fps)) {
		*table_idx = tmp1_table_idx;
		return 1;
	}
	/* worst case should update table */
	ret = _mtk_oddmr_od_table_lookup(cur_timing);
	if (ret >= 0) {
		*table_idx = ret;
		ODDMRFLOW_LOG("update table fps %u, table_idx %d sram(%d %d)\n",
				cur_timing->vrefresh, ret, priv->od_data.od_sram_table_idx[0],
				priv->od_data.od_sram_table_idx[1]);
		return 2;
	}
	ODDMRFLOW_LOG("fps %u out of range\n", cur_timing->vrefresh);
	return -EFAULT;
}

/*
 * return int for further calculate
 */
static int mtk_oddmr_common_gain_lookup(int item, void *table, uint32_t cnt)
{
	int i, left_value, right_value, result, left_item, right_item, tmp_item;
	struct mtk_oddmr_table_gain *gain_table;

	ODDMRAPI_LOG("cnt %u\n", cnt);
	gain_table = (struct mtk_oddmr_table_gain *)table;
	tmp_item = item;
	if (tmp_item <= gain_table[0].item) {
		if (tmp_item != 0)
			ODDMRFLOW_LOG("item %d outof range (%u, %u)\n", tmp_item,
					gain_table[0].item, gain_table[cnt - 1].item);
		result = 0;
		return result;
	}
	for (i = 1; i < cnt; i++) {
		if (tmp_item <= gain_table[i].item)
			break;
	}
	if (i >= cnt) {
		result = 0;
		ODDMRFLOW_LOG("item %u outof range (%u, %u)\n", tmp_item,
				gain_table[0].item, gain_table[cnt - 1].item);
	} else {
		//to cover negative value in calculate
		left_value = (int)gain_table[i - 1].value;
		right_value = (int)gain_table[i].value;
		left_item = (int)gain_table[i - 1].item;
		right_item = (int)gain_table[i].item;
		result = mtk_oddmr_gain_interpolation(left_item, tmp_item, right_item,
				left_value, right_value);
		ODDMRFLOW_LOG("idx %d L:(%d,%d),R:(%d,%d) V:(%d,%d)\n", i,
				left_item, left_value, right_item, right_value, tmp_item, result);
	}
	return result;
}

static int mtk_oddmr_od_gain_lookup(uint32_t fps, uint32_t dbv, int table_idx, uint32_t *weight)
{
	int result_fps, result_dbv, tmp_item;
	uint32_t cnt;
	struct mtk_oddmr_table_gain *bl_gain_table;
	struct mtk_oddmr_table_gain *fps_gain_table;

	ODDMRAPI_LOG("fps %u, dbv %u, table%d\n", fps, dbv, table_idx);
	if (!IS_TABLE_VALID(table_idx, g_od_param.valid_table)) {
		*weight = 0;
		DDPPR_ERR("%s table%d is invalid\n", __func__, table_idx);
		return -EFAULT;
	}
	/*fps*/
	cnt = g_od_param.od_tables[table_idx]->fps_cnt;
	fps_gain_table = g_od_param.od_tables[table_idx]->fps_table;
	tmp_item = (int)fps;
	result_fps = mtk_oddmr_common_gain_lookup(tmp_item, fps_gain_table, cnt);
	/* dbv */
	cnt = g_od_param.od_tables[table_idx]->bl_cnt;
	bl_gain_table = g_od_param.od_tables[table_idx]->bl_table;
	tmp_item = (int)dbv;
	result_dbv = mtk_oddmr_common_gain_lookup(tmp_item, bl_gain_table, cnt);
	*weight = ((uint32_t)result_dbv * (uint32_t)result_fps + 32) / 64;
	return 0;
}

static void mtk_oddmr_od_smi(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	uint32_t value, mask, buf_size;
	struct mtk_disp_oddmr *oddmr = comp_to_oddmr(comp);
	struct mtk_drm_private *priv = comp->mtk_crtc->base.dev->dev_private;

	value = 0;
	mask = 0;
	/* hrt0 */
	SET_VAL_MASK(value, mask, 4, REG_HR0_RE_ULTRA_MODE);
	SET_VAL_MASK(value, mask, 0, REG_HR0_POACH_CFG_OFF);
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_HRT_CTR, mask, pkg);
	if (priv->data->mmsys_id == MMSYS_MT6985) {
		//read in pre-ultra
		mtk_oddmr_write(comp, 0xFFF, DISP_ODDMR_REG_HR0_PREULTRA_RE_IN_THR_0, pkg);
		mtk_oddmr_write(comp, 0, DISP_ODDMR_REG_HR0_PREULTRA_RE_IN_THR_1, pkg);
		//read out pre-ultra
		mtk_oddmr_write(comp, 0xFFF, DISP_ODDMR_REG_HR0_PREULTRA_RE_OUT_THR_0, pkg);
		mtk_oddmr_write(comp, 0, DISP_ODDMR_REG_HR0_PREULTRA_RE_OUT_THR_1, pkg);
		//read in ultra
		mtk_oddmr_write(comp, 0, DISP_ODDMR_REG_HR0_ULTRA_RE_IN_THR_0, pkg);
		mtk_oddmr_write(comp, 0, DISP_ODDMR_REG_HR0_ULTRA_RE_IN_THR_1, pkg);
		//read out ultra
		mtk_oddmr_write(comp, 0, DISP_ODDMR_REG_HR0_ULTRA_RE_OUT_THR_0, pkg);
		mtk_oddmr_write(comp, 0, DISP_ODDMR_REG_HR0_ULTRA_RE_OUT_THR_1, pkg);
		value = 0;
		mask = 0;
		SET_VAL_MASK(value, mask, 4, REG_HR0_RE_ULTRA_MODE);
		SET_VAL_MASK(value, mask, 0, REG_HR0_POACH_CFG_OFF);
		SET_VAL_MASK(value, mask, 0, REG_HR0_PREULTRA_RE_ULTRA_MASK);
		SET_VAL_MASK(value, mask, 1, REG_HR0_ULTRA_RE_MASK);
		mtk_oddmr_write_mask(comp, value, DISP_ODDMR_HRT_CTR, mask, pkg);
	}
	value = 0;
	mask = 0;
	/* odr*/
	SET_VAL_MASK(value, mask, 4, REG_ODR_RE_ULTRA_MODE);
	SET_VAL_MASK(value, mask, 0, REG_ODR_POACH_CFG_OFF);
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_SMI_SB_FLG_ODR_8, mask, pkg);
	buf_size = oddmr->data->odr_buffer_size;
	value = buf_size * ODDMR_READ_IN_PRE_ULTRA;//read in pre-ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_REG_ODR_PREULTRA_RE_IN_THR_0, 0xFFFF, pkg);
	mtk_oddmr_write_mask(comp, value >> 16,
			DISP_ODDMR_REG_ODR_PREULTRA_RE_IN_THR_1, 0xFFFF, pkg);
	value = buf_size * ODDMR_READ_IN_ULTRA;//read in ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_REG_ODR_ULTRA_RE_IN_THR_0, 0xFFFF, pkg);
	mtk_oddmr_write_mask(comp, value >> 16, DISP_ODDMR_REG_ODR_ULTRA_RE_IN_THR_1, 0xFFFF, pkg);
	value = buf_size * ODDMR_READ_OUT_PRE_ULTRA;//read out pre-ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_REG_ODR_PREULTRA_RE_OUT_THR_0, 0xFFFF, pkg);
	mtk_oddmr_write_mask(comp, value >> 16,
			DISP_ODDMR_REG_ODR_PREULTRA_RE_OUT_THR_1, 0xFFFF, pkg);
	value = buf_size * ODDMR_READ_OUT_ULTRA;//read out ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_REG_ODR_ULTRA_RE_OUT_THR_0, 0xFFFF, pkg);
	mtk_oddmr_write_mask(comp, value >> 16, DISP_ODDMR_REG_ODR_ULTRA_RE_OUT_THR_1, 0xFFFF, pkg);
	value = 0;
	mask = 0;
	/* odw*/
	SET_VAL_MASK(value, mask, 4, REG_ODW_WR_ULTRA_MODE);
	SET_VAL_MASK(value, mask, 0, REG_ODW_POACH_CFG_OFF);
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_SMI_SB_FLG_ODW_8, mask, pkg);
	buf_size = oddmr->data->odw_buffer_size;
	value = buf_size * ODDMR_WRITE_IN_PRE_ULTRA;//write in pre-ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_REG_ODW_PREULTRA_WR_IN_THR_0, 0xFFFF, pkg);
	mtk_oddmr_write_mask(comp, value >> 16,
			DISP_ODDMR_REG_ODW_PREULTRA_WR_IN_THR_1, 0xFFFF, pkg);
	value = buf_size * ODDMR_WRITE_IN_ULTRA;//write in ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_REG_ODW_ULTRA_WR_IN_THR_0, 0xFFFF, pkg);
	mtk_oddmr_write_mask(comp, value >> 16, DISP_ODDMR_REG_ODW_ULTRA_WR_IN_THR_1, 0xFFFF, pkg);
	value = buf_size * ODDMR_WRITE_OUT_PRE_ULTRA;//write out pre-ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_REG_ODW_PREULTRA_WR_OUT_THR_0, 0xFFFF, pkg);
	mtk_oddmr_write_mask(comp, value >> 16,
			DISP_ODDMR_REG_ODW_PREULTRA_WR_OUT_THR_1, 0xFFFF, pkg);
	value = buf_size * ODDMR_WRITE_OUT_ULTRA;//write out ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_REG_ODW_ULTRA_WR_OUT_THR_0, 0xFFFF, pkg);
	mtk_oddmr_write_mask(comp, value >> 16, DISP_ODDMR_REG_ODW_ULTRA_WR_OUT_THR_1, 0xFFFF, pkg);
}

static void mtk_oddmr_dmr_smi(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	uint32_t value, mask, buf_size;
	struct mtk_disp_oddmr *oddmr = comp_to_oddmr(comp);

	/* hrt1 */
	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 4, REG_HR1_RE_ULTRA_MODE);
	SET_VAL_MASK(value, mask, 0, REG_HR1_POACH_CFG_OFF);
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_HRT_CTR, mask, pkg);
	/* dmr */
	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 4, REG_DMR_RE_ULTRA_MODE);
	SET_VAL_MASK(value, mask, 0, REG_DMR_POACH_CFG_OFF);
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_SMI_SB_FLG_DMR_8, mask, pkg);
	buf_size = oddmr->data->dmr_buffer_size;
	value = buf_size * ODDMR_READ_IN_PRE_ULTRA;//read in pre-ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_REG_DMR_PREULTRA_RE_IN_THR_0, 0xFFFF, pkg);
	mtk_oddmr_write_mask(comp, value >> 16,
			DISP_ODDMR_REG_DMR_PREULTRA_RE_IN_THR_1, 0xFFFF, pkg);
	value = buf_size * ODDMR_READ_IN_ULTRA;//read in ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_REG_DMR_ULTRA_RE_IN_THR_0, 0xFFFF, pkg);
	mtk_oddmr_write_mask(comp, value >> 16, DISP_ODDMR_REG_DMR_ULTRA_RE_IN_THR_1, 0xFFFF, pkg);
	value = buf_size * ODDMR_READ_OUT_PRE_ULTRA;//read out pre-ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_REG_DMR_PREULTRA_RE_OUT_THR_0, 0xFFFF, pkg);
	mtk_oddmr_write_mask(comp, value >> 16,
			DISP_ODDMR_REG_DMR_PREULTRA_RE_OUT_THR_1, 0xFFFF, pkg);
	value = buf_size * ODDMR_READ_OUT_ULTRA;//read out ultra
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_REG_DMR_ULTRA_RE_OUT_THR_0, 0xFFFF, pkg);
	mtk_oddmr_write_mask(comp, value >> 16, DISP_ODDMR_REG_DMR_ULTRA_RE_OUT_THR_1, 0xFFFF, pkg);
}

static void mtk_oddmr_od_smi_dual(struct cmdq_pkt *pkg)
{
	mtk_oddmr_od_smi(default_comp, pkg);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_od_smi(oddmr1_default_comp, pkg);
}

static void mtk_oddmr_dmr_smi_dual(struct cmdq_pkt *pkg)
{
	mtk_oddmr_dmr_smi(default_comp, pkg);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_dmr_smi(oddmr1_default_comp, pkg);
}

static void mtk_oddmr_od_set_res_dual(struct cmdq_pkt *pkg)
{
	mtk_oddmr_od_set_res(default_comp, pkg);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_od_set_res(oddmr1_default_comp, pkg);
}

static void mtk_oddmr_dmr_free_table(struct mtk_ddp_comp *comp)
{
	struct mtk_disp_oddmr *priv = comp_to_oddmr(comp);

	ODDMRAPI_LOG("+\n");
	if (priv->dmr_data.mura_table != NULL) {
		mtk_drm_gem_free_object(&priv->dmr_data.mura_table->base);
		priv->dmr_data.mura_table = NULL;
	}
}

static int mtk_oddmr_dmr_alloc_table_dual(int table_idx)
{
	uint32_t size_l, size_r;
	void *addr_l, *addr_r;
	int ret = -EFAULT;

	ODDMRAPI_LOG("+\n");
	if (IS_TABLE_VALID(table_idx, g_dmr_param.valid_table)) {
		if (default_comp->mtk_crtc->is_dual_pipe) {
			/* non secure */
			size_l = g_dmr_param.dmr_tables[table_idx]->raw_table_left.size;
			size_r = g_dmr_param.dmr_tables[table_idx]->raw_table_right.size;
			addr_l = g_dmr_param.dmr_tables[table_idx]->raw_table_left.value;
			addr_r = g_dmr_param.dmr_tables[table_idx]->raw_table_right.value;
			mtk_oddmr_dmr_free_table(default_comp);
			mtk_oddmr_dmr_free_table(oddmr1_default_comp);
			g_oddmr_priv->dmr_data.mura_table = mtk_oddmr_load_buffer(
					&default_comp->mtk_crtc->base, size_l, addr_l, false);
			g_oddmr1_priv->dmr_data.mura_table = mtk_oddmr_load_buffer(
					&default_comp->mtk_crtc->base, size_r, addr_r, false);
		} else {
			/* non secure */
			size_l = g_dmr_param.dmr_tables[table_idx]->raw_table_single.size;
			addr_l = g_dmr_param.dmr_tables[table_idx]->raw_table_single.value;
			mtk_oddmr_dmr_free_table(default_comp);
			g_oddmr_priv->dmr_data.mura_table = mtk_oddmr_load_buffer(
					&default_comp->mtk_crtc->base, size_l, addr_l, false);
		}
		ret = 0;
	} else {
		ret = -EFAULT;
		DDPPR_ERR("%s table%d is invalid\n", __func__, table_idx);
	}
	return ret;
}

/* crop,vgs,udma */
static int mtk_oddmr_dmr_set_table_dual(struct cmdq_pkt *pkg, int table_idx)
{
	struct mtk_oddmr_pq_pair *param;
	uint32_t cnt, width, height, tile_overhead = 0;
	dma_addr_t addr = 0;

	ODDMRAPI_LOG("+\n");
	if (g_oddmr_priv == NULL) {
		DDPPR_ERR("%s comp is invalid\n", __func__);
		return -EFAULT;
	}
	if (g_oddmr_priv->data != NULL)
		tile_overhead = g_oddmr_priv->data->tile_overhead;

	if (!IS_TABLE_VALID(table_idx, g_dmr_param.valid_table)) {
		DDPPR_ERR("%s dmr table%d is invalid\n", __func__, table_idx);
		return -EFAULT;
	}
	width = g_dmr_param.dmr_tables[table_idx]->table_basic_info.width;
	height = g_dmr_param.dmr_tables[table_idx]->table_basic_info.height;
	param = g_dmr_param.dmr_tables[table_idx]->pq_common.param;
	cnt = g_dmr_param.dmr_tables[table_idx]->pq_common.counts;
	mtk_oddmr_set_pq(default_comp, pkg,
			&g_dmr_param.dmr_tables[table_idx]->pq_common);
	addr = g_oddmr_priv->dmr_data.mura_table->dma_addr;
	mtk_oddmr_write(default_comp, addr >> 4, DISP_ODDMR_DMR_UDMA_CTR_4, pkg);
	mtk_oddmr_write(default_comp, addr >> 20, DISP_ODDMR_DMR_UDMA_CTR_5, pkg);
	mtk_oddmr_write(default_comp, width, DISP_ODDMR_REG_DMR_PANEL_WIDTH, pkg);
	atomic_set(&g_oddmr_priv->dmr_data.cur_table_idx, table_idx);
	if (default_comp->mtk_crtc->is_dual_pipe) {
		mtk_oddmr_set_pq(oddmr1_default_comp, pkg,
				&g_dmr_param.dmr_tables[table_idx]->pq_common);
		addr = g_oddmr1_priv->dmr_data.mura_table->dma_addr;
		mtk_oddmr_write(oddmr1_default_comp, addr >> 4, DISP_ODDMR_DMR_UDMA_CTR_4, pkg);
		mtk_oddmr_write(oddmr1_default_comp, addr >> 20, DISP_ODDMR_DMR_UDMA_CTR_5, pkg);
		mtk_oddmr_write(oddmr1_default_comp, width / 2 + tile_overhead,
			DISP_ODDMR_REG_DMR_PANEL_WIDTH, pkg);
		mtk_oddmr_write(default_comp, width / 2 + tile_overhead,
			DISP_ODDMR_REG_DMR_PANEL_WIDTH, pkg);
		mtk_oddmr_set_pq(default_comp, pkg,
				&g_dmr_param.dmr_tables[table_idx]->pq_left_pipe);
		mtk_oddmr_set_pq(oddmr1_default_comp, pkg,
				&g_dmr_param.dmr_tables[table_idx]->pq_right_pipe);
		mtk_oddmr_set_crop_dual(pkg);
		atomic_set(&g_oddmr1_priv->dmr_data.cur_table_idx, table_idx);
	} else {
		mtk_oddmr_set_pq(default_comp, pkg,
				&g_dmr_param.dmr_tables[table_idx]->pq_single_pipe);
		mtk_oddmr_write(default_comp, 1, DISP_ODDMR_TOP_CRP_BYPSS, pkg);
	}
	return 0;
}

static void mtk_oddmr_dmr_set_bl_gian_dual(uint32_t offset_gain, struct cmdq_pkt *handle)
{
	ODDMRAPI_LOG("+\n");
	mtk_oddmr_write(default_comp, offset_gain, DISP_ODDMR_REG_DMR_OFFSET_GAIN, handle);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_write(oddmr1_default_comp,
				offset_gain, DISP_ODDMR_REG_DMR_OFFSET_GAIN, handle);
}

static void
mtk_oddmr_dmr_set_fps_gian_dual(struct mtk_oddmr_dmr_fps_gain *fps_weight, struct cmdq_pkt *handle)
{
	ODDMRAPI_LOG("+\n");
	mtk_oddmr_write(default_comp, fps_weight->beta,
			DISP_ODDMR_REG_DMR_FRAME_SCALER_FOR_BETA, handle);
	mtk_oddmr_write(default_comp, fps_weight->gain,
			DISP_ODDMR_REG_DMR_FRAME_SCALER_FOR_GAIN, handle);
	mtk_oddmr_write(default_comp, fps_weight->offset,
			DISP_ODDMR_REG_DMR_FRAME_SCALER_FOR_OFFSET, handle);
	if (default_comp->mtk_crtc->is_dual_pipe) {
		mtk_oddmr_write(oddmr1_default_comp, fps_weight->beta,
				DISP_ODDMR_REG_DMR_FRAME_SCALER_FOR_BETA, handle);
		mtk_oddmr_write(oddmr1_default_comp, fps_weight->gain,
				DISP_ODDMR_REG_DMR_FRAME_SCALER_FOR_GAIN, handle);
		mtk_oddmr_write(oddmr1_default_comp, fps_weight->offset,
				DISP_ODDMR_REG_DMR_FRAME_SCALER_FOR_OFFSET, handle);
	}
}

static int mtk_oddmr_dmr_fps_gain_lookup(int table_idx, struct mtk_oddmr_dmr_fps_gain *weight)
{
	int i, left_value, right_value, result, left_item, right_item, tmp_item;
	uint32_t cnt;
	struct mtk_oddmr_dmr_fps_gain *gain_table;

	ODDMRAPI_LOG("+\n");
	if (!IS_TABLE_VALID(table_idx, g_dmr_param.valid_table))
		return -1;
	cnt = g_dmr_param.dmr_tables[table_idx]->fps_cnt;
	if (cnt > DMR_GAIN_MAX)
		cnt = DMR_GAIN_MAX;
	gain_table = g_dmr_param.dmr_tables[table_idx]->fps_table;
	tmp_item = (int)weight->fps;
	if (tmp_item <= gain_table[0].fps) {
		DDPPR_ERR("%s fps %u outof range (%u, %u)\n", __func__, tmp_item,
				gain_table[0].fps, gain_table[cnt - 1].fps);
		//TODO out of range set 0 or edge
		weight->beta = 0;
		weight->gain = 0;
		weight->offset = 0;
		return 0;
	}
	for (i = 1; i < cnt; i++) {
		if (tmp_item <= gain_table[i].fps)
			break;
	}
	if (i >= cnt) {
		DDPPR_ERR("%s fps %d outof range (%u, %u)\n", __func__, tmp_item,
				gain_table[0].fps, gain_table[cnt - 1].fps);
		weight->beta = gain_table[cnt - 1].beta;
		weight->gain = gain_table[cnt - 1].gain;
		weight->offset = gain_table[cnt - 1].offset;
		return 0;
	}
	left_item = (int)gain_table[i - 1].fps;
	right_item = (int)gain_table[i].fps;
	left_value = (int)gain_table[i - 1].beta;
	right_value = (int)gain_table[i].beta;
	result = mtk_oddmr_gain_interpolation(left_item, tmp_item, right_item,
			left_value, right_value);
	weight->beta = (uint32_t)result;

	left_value = (int)gain_table[i - 1].gain;
	right_value = (int)gain_table[i].gain;
	result = mtk_oddmr_gain_interpolation(left_item, tmp_item, right_item,
			left_value, right_value);
	weight->gain = (uint32_t)result;

	left_value = (int)gain_table[i - 1].offset;
	right_value = (int)gain_table[i].offset;
	result = mtk_oddmr_gain_interpolation(left_item, tmp_item, right_item,
			left_value, right_value);
	weight->offset = (uint32_t)result;
	return 0;
}

static int mtk_oddmr_dmr_bl_gain_lookup(uint32_t dbv, int table_idx, uint32_t *weight)
{
	int result, tmp_item;
	uint32_t cnt;
	struct mtk_oddmr_table_gain *bl_gain_table;

	if (!IS_TABLE_VALID(table_idx, g_dmr_param.valid_table)) {
		*weight = 0;
		return -EFAULT;
	}
	tmp_item = (int)dbv;
	bl_gain_table = g_dmr_param.dmr_tables[table_idx]->bl_table;
	cnt = g_dmr_param.dmr_tables[table_idx]->bl_cnt;
	result = mtk_oddmr_common_gain_lookup(tmp_item, bl_gain_table, cnt);
	*weight = (uint32_t)result;
	return 0;
}

static int mtk_oddmr_dmr_table_lookup(struct mtk_disp_oddmr *priv,
		struct mtk_oddmr_timing *new_timing)
{
	int table_idx;
	uint32_t resolution_switch_mode, width, height;

	ODDMRAPI_LOG("+\n");
	table_idx = atomic_read(&priv->dmr_data.cur_table_idx);
	resolution_switch_mode = g_dmr_param.dmr_basic_info.basic_param.resolution_switch_mode;

	/* best stay at current table */
	if ((IS_TABLE_VALID(table_idx, g_dmr_param.valid_table)) &&
	(((new_timing->mode_chg_index & MODE_DSI_RES) == 0) ||
	(resolution_switch_mode == 0)) &&
	(new_timing->vrefresh >= g_dmr_param.dmr_tables[table_idx]->table_basic_info.min_fps) &&
	(new_timing->vrefresh <= g_dmr_param.dmr_tables[table_idx]->table_basic_info.max_fps)) {
		return table_idx;
	}
	for (table_idx = 0;
			table_idx < g_dmr_param.dmr_basic_info.basic_param.table_cnt; table_idx++) {
		if (!IS_TABLE_VALID(table_idx, g_dmr_param.valid_table))
			continue;
		/* ddic res switch */
		if (resolution_switch_mode) {
			width = g_dmr_param.dmr_tables[table_idx]->table_basic_info.width;
			height = g_dmr_param.dmr_tables[table_idx]->table_basic_info.height;
			if ((new_timing->hdisplay == width) &&
				(new_timing->vdisplay == height) &&
				(new_timing->vrefresh >=
				g_dmr_param.dmr_tables[table_idx]->table_basic_info.min_fps) &&
				(new_timing->vrefresh <=
				g_dmr_param.dmr_tables[table_idx]->table_basic_info.max_fps))
				return table_idx;
		} else {
			/* AP res switch */
			if ((new_timing->vrefresh >=
				g_dmr_param.dmr_tables[table_idx]->table_basic_info.min_fps) &&
				(new_timing->vrefresh <=
				g_dmr_param.dmr_tables[table_idx]->table_basic_info.max_fps))
				return table_idx;
		}
	}
	DDPPR_ERR("%s table not found, %u,%u,%u Hz\n",
			new_timing->hdisplay, new_timing->vdisplay, new_timing->vrefresh);
	return -EFAULT;
}

static void mtk_oddmr_dmr_timing_chg_dual(struct mtk_oddmr_timing *timing, struct cmdq_pkt *handle)
{
	int prev_table_idx, table_idx;
	uint32_t dbv, dbv_gain;
	struct mtk_oddmr_dmr_fps_gain fps_weight;

	ODDMRAPI_LOG("+\n");
	if (g_oddmr_priv->dmr_state >= ODDMR_INIT_DONE) {
		table_idx = mtk_oddmr_dmr_table_lookup(g_oddmr_priv, timing);
		if (table_idx < 0)
			return;
		prev_table_idx = atomic_read(&g_oddmr_priv->dmr_data.cur_table_idx);
		if (table_idx != prev_table_idx) {
			/* switch table */
			mtk_oddmr_dmr_alloc_table_dual(table_idx);
			mtk_oddmr_dmr_set_table_dual(handle, table_idx);
			dbv = g_oddmr_current_timing.bl_level;
			mtk_oddmr_dmr_bl_gain_lookup(dbv, table_idx, &dbv_gain);
			mtk_oddmr_dmr_set_bl_gian_dual(dbv_gain, handle);
		}
		fps_weight.fps = timing->vrefresh;
		mtk_oddmr_dmr_fps_gain_lookup(table_idx, &fps_weight);
		mtk_oddmr_dmr_set_fps_gian_dual(&fps_weight, handle);
		ODDMRFLOW_LOG("table_idx new:%d,old:%d\n", table_idx, prev_table_idx);
	}
}

static void mtk_oddmr_od_flip(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	struct mtk_disp_oddmr *priv = comp_to_oddmr(comp);
	int table_idx;
	uint32_t value, mask, r_sel, w_sel;

	ODDMRAPI_LOG("+\n");
	//flip
	priv->od_data.od_sram_read_sel = !priv->od_data.od_sram_read_sel;
	table_idx = priv->od_data.od_sram_table_idx[!!priv->od_data.od_sram_read_sel];
	r_sel = priv->od_data.od_sram_read_sel;
	w_sel = !r_sel;
	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, w_sel, REG_OD_SRAM_WRITE_SEL);
	SET_VAL_MASK(value, mask, r_sel, REG_OD_SRAM_READ_SEL);
	ODDMRFLOW_LOG("value w_sel %d r_sel %d 0x%x\n", w_sel, r_sel, value);
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_OD_SRAM_CTRL_0, mask, handle);
	/* od pq */
	if (IS_TABLE_VALID(table_idx, g_od_param.valid_table))
		mtk_oddmr_set_pq(comp, handle, &g_od_param.od_tables[table_idx]->pq_od);
}

static void mtk_oddmr_set_od_weight(struct mtk_ddp_comp *comp, uint32_t weight,
		struct cmdq_pkt *handle)
{
	mtk_oddmr_write(comp, weight, DISP_ODDMR_OD_PQ_0,
			handle);
}

static void mtk_oddmr_set_od_weight_dual(struct mtk_ddp_comp *comp, uint32_t weight,
		struct cmdq_pkt *handle)
{
	ODDMRAPI_LOG("oddmr %u+\n", weight);
	mtk_oddmr_set_od_weight(default_comp, weight, handle);
	if (comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_set_od_weight(oddmr1_default_comp, weight, handle);
}

static void mtk_oddmr_od_timing_chg_dual(struct mtk_oddmr_timing *timing, struct cmdq_pkt *handle)
{
	uint32_t weight;
	unsigned long flags;
	int ret, table_idx;

	ODDMRAPI_LOG("+\n");
	if (g_oddmr_priv->od_state >= ODDMR_INIT_DONE) {
		spin_lock_irqsave(&g_oddmr_od_sram_lock, flags);
		ret = mtk_oddmr_od_table_lookup(g_oddmr_priv, timing, &table_idx);
		if (ret >= 0) {
			if (ret == 1) {
				//flip
				mtk_oddmr_od_flip(default_comp, handle);
				if (default_comp->mtk_crtc->is_dual_pipe)
					mtk_oddmr_od_flip(oddmr1_default_comp, handle);
			}
			if (ret == 2) {
				//TODO:update table work queue(table pq sram) mt6985 no need
				//g_oddmr_priv->od_state = ODDMR_TABLE_UPDATING;
			}
			if ((timing->mode_chg_index & MODE_DSI_RES) &&
			(g_od_param.od_basic_info.basic_param.resolution_switch_mode == 1)) {
				/* res switch in ddic */
				atomic_set(&g_oddmr_od_weight_trigger, 1);
				weight = 0;
				mtk_oddmr_od_set_res_dual(handle);
			} else {
				mtk_oddmr_od_gain_lookup(timing->vrefresh,
						timing->bl_level, table_idx, &weight);
			}
			mtk_oddmr_set_od_weight_dual(default_comp, weight, handle);
		}
		spin_unlock_irqrestore(&g_oddmr_od_sram_lock, flags);
	}
}

/* call from drm atomic flow */
void mtk_oddmr_timing_chg(struct mtk_oddmr_timing *timing, struct cmdq_pkt *handle)
{
	unsigned long flags;
	struct mtk_oddmr_timing timing_working_copy;

	if (is_oddmr_dmr_support || is_oddmr_od_support) {
		ODDMRAPI_LOG("+\n");
		mtk_oddmr_copy_oddmr_timg(&timing_working_copy, timing);
		spin_lock_irqsave(&g_oddmr_timing_lock, flags);
		timing_working_copy.bl_level = g_oddmr_current_timing.bl_level;
		mtk_oddmr_copy_oddmr_timg(&g_oddmr_current_timing, timing);
		spin_unlock_irqrestore(&g_oddmr_timing_lock, flags);
		ODDMRFLOW_LOG("w %u, h %u, fps %u, bl %u\n",
				timing_working_copy.hdisplay,
				timing_working_copy.vdisplay,
				timing_working_copy.vrefresh,
				timing_working_copy.bl_level);
		if (timing->mode_chg_index & MODE_DSI_RES)
			mtk_oddmr_set_spr2rgb_dual(handle);
	}
	if (is_oddmr_dmr_support && handle != NULL)
		mtk_oddmr_dmr_timing_chg_dual(&timing_working_copy, handle);
	if (is_oddmr_od_support && handle != NULL)
		mtk_oddmr_od_timing_chg_dual(&timing_working_copy, handle);
}

static void mtk_oddmr_dmr_bl_chg(uint32_t bl_level, struct cmdq_pkt *handle)
{
	uint32_t offset_gain;
	int table_idx;

	if (g_oddmr_priv->dmr_state >= ODDMR_INIT_DONE) {
		ODDMRAPI_LOG("+\n");
		table_idx = atomic_read(&g_oddmr_priv->dmr_data.cur_table_idx);
		if (!IS_TABLE_VALID(table_idx, g_dmr_param.valid_table)) {
			DDPPR_ERR("%s table invalid %d\n", __func__, table_idx);
			return;
		}
		mtk_oddmr_dmr_bl_gain_lookup(bl_level, table_idx, &offset_gain);
		mtk_oddmr_dmr_set_bl_gian_dual(offset_gain, handle);
	}
}

static void mtk_oddmr_od_bl_chg(uint32_t bl_level, struct cmdq_pkt *handle)
{
	uint32_t weight = 0;
	int sram_idx, table_idx;

	if (g_oddmr_priv->od_state >= ODDMR_INIT_DONE) {
		ODDMRAPI_LOG("+\n");
		sram_idx = g_oddmr_priv->od_data.od_sram_read_sel;
		table_idx = g_oddmr_priv->od_data.od_sram_table_idx[sram_idx];
		if (!IS_TABLE_VALID(table_idx, g_od_param.valid_table)) {
			DDPPR_ERR("%s table invalid %d\n", __func__, table_idx);
			return;
		}
		mtk_oddmr_od_gain_lookup(g_oddmr_current_timing.vrefresh,
				bl_level, table_idx, &weight);
		mtk_oddmr_set_od_weight_dual(default_comp, weight, handle);
	}
}
void mtk_oddmr_bl_chg(uint32_t bl_level, struct cmdq_pkt *handle)
{
	unsigned long flags;

	ODDMRLOW_LOG("bl_level %u\n", bl_level);
	if (is_oddmr_dmr_support && handle != NULL)
		mtk_oddmr_dmr_bl_chg(bl_level, handle);

	if (is_oddmr_od_support && handle != NULL)
		mtk_oddmr_od_bl_chg(bl_level, handle);

	/* keep track of chg anytime */
	spin_lock_irqsave(&g_oddmr_timing_lock, flags);
	g_oddmr_current_timing.bl_level = bl_level;
	spin_unlock_irqrestore(&g_oddmr_timing_lock, flags);
}
void mtk_oddmr_set_pq_dirty(void)
{
	atomic_set(&g_oddmr_pq_dirty, 1);
}

int mtk_oddmr_hrt_cal_notify(int *oddmr_hrt)
{
	int sum = 0;

	if (is_oddmr_od_support || is_oddmr_dmr_support) {
		if (atomic_read(&g_oddmr_od_hrt_done) == 2)
			atomic_set(&g_oddmr_od_hrt_done, 1);
		if (atomic_read(&g_oddmr_dmr_hrt_done) == 2)
			atomic_set(&g_oddmr_dmr_hrt_done, 1);
		if (g_oddmr_priv->od_enable)
			sum += mtk_oddmr_od_bpp(g_od_param.od_basic_info.basic_param.od_mode);
		if (g_oddmr_priv->dmr_enable)
			sum += mtk_oddmr_dmr_bpp(
					g_dmr_param.dmr_basic_info.basic_param.dmr_table_mode);
		wake_up_all(&g_oddmr_hrt_wq);
		ODDMRLOW_LOG("od %d dmr %d sum %d\n",
				g_oddmr_priv->od_enable, g_oddmr_priv->dmr_enable, sum);
	} else {
		return 0;
	}
	if (g_oddmr_hrt_en == false)
		return 0;
	*oddmr_hrt += sum;
	return sum;
}

void mtk_oddmr_od_sec_bypass(uint32_t sec_on, struct cmdq_pkt *handle)
{
	uint32_t enable;
	static uint32_t prev;

	if (is_oddmr_od_support) {
		enable = g_oddmr_priv->od_enable && (!sec_on);
		mtk_oddmr_set_od_enable_dual(NULL, enable, handle);
		if (enable && sec_on != prev &&
			(g_oddmr_priv->data->is_od_support_hw_skip_first_frame == false)) {
			mtk_oddmr_set_od_weight_dual(default_comp, 0, handle);
			atomic_set(&g_oddmr_od_weight_trigger, 1);
		}
		prev = sec_on;
	}
}
void mtk_oddmr_ddren(struct cmdq_pkt *cmdq_handle,
	struct drm_crtc *crtc, int en)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned long crtc_id = (unsigned long)drm_crtc_index(crtc);
	const u16 reg_jump = CMDQ_THR_SPR_IDX1;
	const u16 var1 = CMDQ_THR_SPR_IDX2;
	struct cmdq_operand lop, rop;
	u32 inst_condi_jump;
	u64 *inst, jump_pa;
	resource_size_t pa;
	unsigned int mmsys_sodi_req_mask, sodi_req_sel_val_sw;

	if (priv->data->mmsys_id != MMSYS_MT6985 || g_oddmr_ddren == false)
		return;
	if (crtc_id != 0)
		return;
	mmsys_sodi_req_mask = 0xF4;
	sodi_req_sel_val_sw = 0x4100;
	pa = 0x14013000;
	if (default_comp)
		pa = default_comp->regs_pa;
	if (en) {
		/* 1. get od status */
		lop.reg = true;
		lop.idx = CMDQ_THR_SPR_IDX2;
		rop.reg = false;
		rop.value = 1;

		cmdq_pkt_read(cmdq_handle, NULL,
					pa + DISP_ODDMR_OD_CTRL_EN, var1);
		cmdq_pkt_logic_command(cmdq_handle, CMDQ_LOGIC_AND, var1, &lop, &rop);
		/* 2. set ddren if od is enabled*/
		lop.reg = true;
		lop.idx = CMDQ_THR_SPR_IDX2;
		rop.reg = false;
		rop.value = 1;

		/*mark condition jump */
		inst_condi_jump = cmdq_handle->cmd_buf_size;
		cmdq_pkt_assign_command(cmdq_handle, reg_jump, 0);

		cmdq_pkt_cond_jump_abs(cmdq_handle, reg_jump, &lop, &rop,
			CMDQ_NOT_EQUAL);

		/* if condition false, will jump here */
		{
			cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
				mtk_crtc->config_regs_pa + mmsys_sodi_req_mask,
				sodi_req_sel_val_sw, sodi_req_sel_val_sw);
		}

		/* if condition true, will jump curreent postzion */
		inst = cmdq_pkt_get_va_by_offset(cmdq_handle,  inst_condi_jump);
		jump_pa = cmdq_pkt_get_pa_by_offset(cmdq_handle,
					cmdq_handle->cmd_buf_size);
		*inst = *inst | CMDQ_REG_SHIFT_ADDR(jump_pa);

	} else
		cmdq_pkt_write(cmdq_handle, mtk_crtc->gce_obj.base,
			mtk_crtc->config_regs_pa + mmsys_sodi_req_mask,
			0x4000, sodi_req_sel_val_sw);
}

int mtk_oddmr_io_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		enum mtk_ddp_io_cmd cmd, void *params)
{
	ODDMRLOW_LOG("cmd %d+\n", cmd);

	switch (cmd) {
	case FRAME_DIRTY:
		if (comp->id == DDP_COMPONENT_ODDMR1)
			break;
		atomic_set(&g_oddmr_frame_dirty, 1);
		break;
	case ODDMR_TRIG_CTL:
	{
		uint32_t on = *(uint32_t *)params;

		if (comp->id == DDP_COMPONENT_ODDMR1)
			break;
		atomic_set(&g_od_check_trigger, on);
	}
		break;
	case PQ_DIRTY:
		if (comp->id == DDP_COMPONENT_ODDMR1)
			break;
		mtk_oddmr_set_pq_dirty();
		break;
	case ODDMR_BL_CHG:
	{
		uint32_t bl_level = *(uint32_t *)params;

		if (comp->id == DDP_COMPONENT_ODDMR1)
			break;
		mtk_oddmr_bl_chg(bl_level, handle);
	}
		break;
	case ODDMR_TIMING_CHG:
	{
		struct mtk_oddmr_timing *timing = (struct mtk_oddmr_timing *)params;

		if (comp->id == DDP_COMPONENT_ODDMR1)
			break;
		mtk_oddmr_timing_chg(timing, handle);
	}
		break;
	case COMP_SEC_CFG:
	{
		bool sec_on = *(bool *)params;

		if (comp->id == DDP_COMPONENT_ODDMR1)
			break;
		mtk_oddmr_od_sec_bypass(sec_on, handle);
	}
		break;
	case PMQOS_UPDATE_BW:
	{
		int od_enable, sec_on;
		struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
		struct mtk_drm_private *priv =
			comp->mtk_crtc->base.dev->dev_private;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;
		sec_on = comp->mtk_crtc->sec_on;
		od_enable = oddmr_priv->od_enable && (!sec_on);
		mtk_oddmr_od_srt_cal(comp, od_enable);
		ODDMRLOW_LOG("srt odr(%u,%u),odw(%u,%u),dmrr(%u,%u)\n",
			oddmr_priv->last_qos_srt_odr, oddmr_priv->qos_srt_odr,
			oddmr_priv->last_qos_srt_odw, oddmr_priv->qos_srt_odw,
			oddmr_priv->last_qos_srt_dmrr, oddmr_priv->qos_srt_dmrr);
		/* process normal */
		if (oddmr_priv->last_qos_srt_odr != oddmr_priv->qos_srt_odr) {
			__mtk_disp_set_module_bw(oddmr_priv->qos_req_odr, comp->id,
				oddmr_priv->qos_srt_odr, DISP_BW_NORMAL_MODE);
			oddmr_priv->last_qos_srt_odr = oddmr_priv->qos_srt_odr;
		}
		if (oddmr_priv->last_qos_srt_odw != oddmr_priv->qos_srt_odw) {
			__mtk_disp_set_module_bw(oddmr_priv->qos_req_odw, comp->id,
				oddmr_priv->qos_srt_odw, DISP_BW_NORMAL_MODE);
			oddmr_priv->last_qos_srt_odw = oddmr_priv->qos_srt_odw;
		}
		if (oddmr_priv->last_qos_srt_dmrr != oddmr_priv->qos_srt_dmrr) {
			__mtk_disp_set_module_bw(oddmr_priv->qos_req_dmrr, comp->id,
				oddmr_priv->qos_srt_dmrr, DISP_BW_NORMAL_MODE);
			oddmr_priv->last_qos_srt_dmrr = oddmr_priv->qos_srt_dmrr;
		}
	}
		break;
	case PMQOS_SET_HRT_BW:
	{
		int od_enable, dmr_enable, sec_on;
		u32 bw_val = *(unsigned int *)params;
		struct mtk_disp_oddmr *oddmr_priv = comp_to_oddmr(comp);
		struct mtk_drm_private *priv =
			comp->mtk_crtc->base.dev->dev_private;

		if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_MMQOS_SUPPORT))
			break;
		sec_on = comp->mtk_crtc->sec_on;
		od_enable = oddmr_priv->od_enable && (!sec_on);
		dmr_enable = oddmr_priv->dmr_enable;
		od_enable = !!bw_val && od_enable;
		dmr_enable = !!bw_val && dmr_enable;
		/* set to max if need hrt */
		__mtk_disp_set_module_hrt(oddmr_priv->qos_req_dmrr_hrt, dmr_enable);
		__mtk_disp_set_module_hrt(oddmr_priv->qos_req_odr_hrt, od_enable);
		__mtk_disp_set_module_hrt(oddmr_priv->qos_req_odw_hrt, od_enable);
		ODDMRLOW_LOG("hrt od %d dmr %d\n", od_enable, dmr_enable);
	}
		break;
	default:
		break;
	}
	return 0;
}

static void mtk_oddmr_set_od_enable(struct mtk_ddp_comp *comp, uint32_t enable,
		struct cmdq_pkt *handle)
{
	bool sec_on;

	sec_on = comp->mtk_crtc->sec_on;
	ODDMRLOW_LOG("en %d, sec %d+\n", enable, sec_on);
	if (enable && !sec_on)
		mtk_oddmr_write_mask(comp, 1, DISP_ODDMR_OD_CTRL_EN, 0x01, handle);
	else
		mtk_oddmr_write_mask(comp, 0, DISP_ODDMR_OD_CTRL_EN, 0x01, handle);
}

static void mtk_oddmr_set_od_enable_dual(struct mtk_ddp_comp *comp, uint32_t enable,
		struct cmdq_pkt *handle)
{
	mtk_oddmr_set_od_enable(default_comp, enable, handle);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_set_od_enable(oddmr1_default_comp, enable, handle);
}
static void mtk_oddmr_set_dmr_enable(struct mtk_ddp_comp *comp, uint32_t enable,
		struct cmdq_pkt *handle)
{
	ODDMRAPI_LOG("+\n");
	if (enable) {
		mtk_oddmr_top_prepare(comp, handle);
		mtk_oddmr_write(comp, 1, DISP_ODDMR_REG_DMR_EN, handle);
		mtk_oddmr_write(comp, 0,
				DISP_ODDMR_TOP_DMR_BYASS, handle);
		mtk_oddmr_write(comp, 0,
				DISP_ODDMR_TOP_HRT1_BYPASS, handle);
		mtk_oddmr_write(comp, 1,
				DISP_ODDMR_DMR_UDMA_ENABLE, handle);
	} else {
		mtk_oddmr_write(comp, 1,
				DISP_ODDMR_TOP_DMR_BYASS, handle);
		mtk_oddmr_write(comp, 1,
				DISP_ODDMR_TOP_HRT1_BYPASS, handle);
		mtk_oddmr_write(comp, 0,
				DISP_ODDMR_DMR_UDMA_ENABLE, handle);
	}
}

static void mtk_oddmr_set_dmr_enable_dual(struct mtk_ddp_comp *comp, uint32_t enable,
		struct cmdq_pkt *handle)
{
	ODDMRFLOW_LOG("+\n");
	mtk_oddmr_set_dmr_enable(default_comp, enable, handle);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_set_dmr_enable(oddmr1_default_comp, enable, handle);
}

static void mtk_oddmr_od_init_end_dual(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle)
{
	ODDMRFLOW_LOG("+\n");
	mtk_oddmr_od_init_end(default_comp, handle);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_od_init_end(oddmr1_default_comp, handle);
}

static void mtk_oddmr_od_tuning_write_sram(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct mtk_oddmr_od_tuning_sram *tuning_data)
{
	uint32_t channel, sram, idx, val, ctl;
	uint32_t value = 0, mask = 0, tmp_r_sel = 0, tmp_w_sel = 0;

	channel = tuning_data->channel;
	sram = tuning_data->sram;
	idx = tuning_data->idx;
	val = tuning_data->value;

	ctl = mtk_oddmr_read(comp, DISP_ODDMR_OD_SRAM_CTRL_0);

	tmp_r_sel = (ctl & 0x20) >> 5;
	tmp_w_sel = tmp_r_sel;
	value = 0;
	mask = 0;
	SET_VAL_MASK(value, mask, 1 << (channel + 1), REG_WBGR_OD_SRAM_IO_EN);
	SET_VAL_MASK(value, mask, 0, REG_AUTO_SRAM_ADR_INC_EN);
	SET_VAL_MASK(value, mask, tmp_w_sel, REG_OD_SRAM_WRITE_SEL);
	SET_VAL_MASK(value, mask, tmp_r_sel, REG_OD_SRAM_READ_SEL);
	mtk_oddmr_write_mask(comp, value, DISP_ODDMR_OD_SRAM_CTRL_0, mask, handle);
	mtk_oddmr_write(comp, val,
		(DISP_ODDMR_OD_SRAM_CTRL_2 + 12 * (sram - 1)), handle);
	mtk_oddmr_write(comp, 0x8000 | (idx & 0x1FF),
		(DISP_ODDMR_OD_SRAM_CTRL_1 + 12 * (sram - 1)), handle);
	mtk_oddmr_write_mask(comp, ctl, DISP_ODDMR_OD_SRAM_CTRL_0, 0xFFFFFFFF, handle);
}

static void mtk_oddmr_od_tuning_write_sram_dual(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, struct mtk_oddmr_od_tuning_sram *tuning_data)
{
	mtk_oddmr_od_tuning_write_sram(default_comp, handle, tuning_data);
	if (comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_od_tuning_write_sram(oddmr1_default_comp, handle, tuning_data);
}
/* all oddmr user cmd use handle dualpipe itself because it is not drm atomic */
static int mtk_oddmr_user_cmd(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		uint32_t cmd, void *data)
{
	ODDMRFLOW_LOG("+ cmd: %d\n", cmd);
	/* only handle comp0 as main oddmr comp */
	if (comp->id == DDP_COMPONENT_ODDMR1)
		return 0;

	switch (cmd) {
	case ODDMR_CMD_OD_SET_WEIGHT:
	{
		uint32_t value = *(uint32_t *)data;

		mtk_oddmr_set_od_weight_dual(comp, value, handle);
		if (atomic_read(&g_oddmr_od_weight_trigger) > 0)
			atomic_dec(&g_oddmr_od_weight_trigger);
		break;
	}
	case ODDMR_CMD_OD_ENABLE:
	{
		uint32_t value = *(uint32_t *)data;
		//sw bypass first frame od pq
		//TODO: g_oddmr_od_weight_trigger = 2 ???
		if ((g_oddmr_priv->data->is_od_support_hw_skip_first_frame == false) &&
				(is_oddmr_od_support == true) &&
				(value == 1)) {
			mtk_oddmr_set_od_weight_dual(comp, 0, handle);
			atomic_set(&g_oddmr_od_weight_trigger, 1);
		}
		mtk_oddmr_set_od_enable_dual(comp, value, handle);
		break;
	}
	case ODDMR_CMD_DMR_ENABLE:
	{
		uint32_t value = *(uint32_t *)data;

		mtk_oddmr_set_dmr_enable_dual(comp, value, handle);
		break;
	}
	case ODDMR_CMD_OD_INIT_END:
	{
		mtk_oddmr_od_init_end_dual(comp, handle);
		break;
	}
	case ODDMR_CMD_EOF_CHECK_TRIGGER:
	{
		cmdq_pkt_set_event(handle, comp->mtk_crtc->gce_obj.event[EVENT_STREAM_DIRTY]);
		break;
	}
	case ODDMR_CMD_OD_TUNING_WRITE_SRAM:
	{
		struct mtk_oddmr_od_tuning_sram *tuning_data = data;

		if (tuning_data == NULL) {
			ODDMRFLOW_LOG("tuning data is NULL\n", cmd);
			return -EFAULT;
		}
		mtk_oddmr_od_tuning_write_sram_dual(comp, handle, tuning_data);
		break;
	}
	default:
	ODDMRFLOW_LOG("error cmd: %d\n", cmd);
	return -EINVAL;
	}
	return 0;
}

void disp_oddmr_on_start_of_frame(void)
{
	if (!default_comp)
		return;
	if (is_oddmr_od_support == false ||
			g_oddmr_priv->od_state < ODDMR_INIT_DONE ||
			g_oddmr_priv->od_enable == 0)
		return;
	if (!atomic_read(&g_oddmr_sof_irq_available)) {
		atomic_set(&g_oddmr_sof_irq_available, 1);
		wake_up_interruptible(&g_oddmr_sof_irq_wq);
	}
}

static void disp_oddmr_wait_sof_irq(void)
{
	uint32_t weight;
	int ret = 0, sel = 0;
	bool frame_req_trig, pq_req_trig;

	if (atomic_read(&g_oddmr_sof_irq_available) == 0) {
		ODDMRLOW_LOG("wait_event_interruptible\n");
		ret = wait_event_interruptible(g_oddmr_sof_irq_wq,
				atomic_read(&g_oddmr_sof_irq_available) == 1);
		ODDMRLOW_LOG("sof_irq_available = 1, waken up, ret = %d\n", ret);
		if (ret < 0)
			return;
	} else {
		ODDMRFLOW_LOG("sof_irq_available = 0\n");
		return;
	}
	CRTC_MMP_EVENT_START(0, oddmr_sof_thread, 0, 0);
	CRTC_MMP_MARK(0, oddmr_sof_thread, atomic_read(&g_oddmr_od_weight_trigger), 0);
	if (g_oddmr_priv->od_enable) {
		/* 1. restore user weight */
		if (atomic_read(&g_oddmr_od_weight_trigger) == 1) {
			sel = g_oddmr_priv->od_data.od_sram_read_sel;
			mtk_oddmr_od_gain_lookup(g_oddmr_current_timing.vrefresh,
					g_oddmr_current_timing.bl_level,
					g_oddmr_priv->od_data.od_sram_table_idx[sel], &weight);
			ODDMRFLOW_LOG("weight restore %u\n", weight);
			mtk_crtc_user_cmd(&default_comp->mtk_crtc->base,
				default_comp, ODDMR_CMD_OD_SET_WEIGHT, &weight);
			CRTC_MMP_MARK(0, oddmr_sof_thread, weight, 1);
		}
		/* 2. wait until near next frame te */
		frame_req_trig = (atomic_read(&g_oddmr_frame_dirty) == 1);
		pq_req_trig = (atomic_read(&g_oddmr_pq_dirty) == 1);
		atomic_set(&g_oddmr_frame_dirty, 0);
		atomic_set(&g_oddmr_pq_dirty, 0);
		{
			uint32_t second = 1000000, eof;
			u16 fps = g_oddmr_current_timing.vrefresh;

			if (fps <= 0)
				fps = 10;
			eof = 1 * second / fps;

			ODDMRLOW_LOG("fps: %u eof %u\n", fps, eof);
			if (eof >= 2000)
				usleep_range(eof - 2000, eof - 1500);
		}
		CRTC_MMP_MARK(0, oddmr_sof_thread, 0, 2);
		/* 3. check & trigger */
		if (frame_req_trig || pq_req_trig ||
				atomic_read(&g_oddmr_od_weight_trigger)) {
			if (atomic_read(&g_oddmr_frame_dirty) == 0 &&
				atomic_read(&g_oddmr_pq_dirty) == 0 &&
				mtk_crtc_is_frame_trigger_mode(&default_comp->mtk_crtc->base) &&
				atomic_read(&g_od_check_trigger)) {
				ODDMRLOW_LOG("check trigger\n");
				mtk_crtc_check_trigger(default_comp->mtk_crtc, true, true);
				//mtk_crtc_user_cmd(&default_comp->mtk_crtc->base,
						//default_comp, ODDMR_CMD_EOF_CHECK_TRIGGER, NULL);
				CRTC_MMP_MARK(0, oddmr_sof_thread, 0, 3);
			}
		}
	}
	CRTC_MMP_EVENT_END(0, oddmr_sof_thread, 0, 0);
}
static int mtk_oddmr_sof_irq_trigger(void *data)
{
	while (1) {
		disp_oddmr_wait_sof_irq();
		atomic_set(&g_oddmr_sof_irq_available, 0);
		if (kthread_should_stop()) {
			DDPPR_ERR("%s stopped\n", __func__);
			break;
		}
	}
	return 0;
}

/* top,dither,common pq, udma */
static void mtk_oddmr_od_common_init_dual(struct cmdq_pkt *pkg)
{
	mtk_oddmr_od_common_init(default_comp, pkg);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_od_common_init(oddmr1_default_comp, pkg);
}

static void mtk_oddmr_od_hsk_force_clk(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	mtk_oddmr_write(comp, 1, DISP_ODDMR_OD_HSK_0, pkg);
	mtk_oddmr_write(comp, 1, DISP_ODDMR_OD_HSK_1, pkg);
	mtk_oddmr_write(comp, 0xFFF, DISP_ODDMR_OD_HSK_2, pkg);
	mtk_oddmr_write(comp, 0xFFF, DISP_ODDMR_OD_HSK_3, pkg);
	mtk_oddmr_write(comp, 0xFFF, DISP_ODDMR_OD_HSK_4, pkg);
}

static void mtk_oddmr_od_hsk_force_clk_dual(struct cmdq_pkt *pkg)
{
	mtk_oddmr_od_hsk_force_clk(default_comp, pkg);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_od_hsk_force_clk(oddmr1_default_comp, pkg);
}

static int mtk_oddmr_od_init(void)
{
	struct mtk_drm_crtc *mtk_crtc;
	int ret, table_idx;
	struct cmdq_client *client = NULL;

	ODDMRAPI_LOG("+\n");
	if (default_comp == NULL || g_oddmr_priv == NULL || default_comp->mtk_crtc == NULL) {
		ODDMRFLOW_LOG("oddmr comp is not available\n");
		return -1;
	}
	if (!is_oddmr_od_support) {
		ODDMRFLOW_LOG("od is not support\n");
		return -1;
	}
	if (g_oddmr_priv->od_state != ODDMR_LOAD_PARTS) {
		ODDMRFLOW_LOG("od can not init, state %d\n", g_oddmr_priv->od_state);
		return -1;
	}
	if (g_oddmr_priv->od_enable > 0) {
		ODDMRFLOW_LOG("od can not init when running\n");
		return -1;
	}
	if (g_od_param.od_basic_info.basic_param.table_cnt != g_od_param.valid_table_cnt) {
		g_oddmr_priv->od_state = ODDMR_INVALID;
		ODDMRFLOW_LOG("tables are not fully loaded,cnts %d, valid %d\n",
			g_od_param.od_basic_info.basic_param.table_cnt, g_od_param.valid_table_cnt);
		return -1;
	}
	if (!mtk_oddmr_match_panelid(&g_panelid, &g_od_param.od_basic_info.basic_param.panelid)) {
		ODDMRFLOW_LOG("panelid does not match\n");
		return -1;
	}
	mtk_crtc = default_comp->mtk_crtc;
	mtk_drm_set_idlemgr(&mtk_crtc->base, 0, 1);
	mtk_crtc_check_trigger(default_comp->mtk_crtc, true, true);
	ret = mtk_oddmr_acquire_clock();
	if (ret == 0) {
		g_oddmr_priv->od_state = ODDMR_LOAD_DONE;
		mtk_oddmr_od_common_init_dual(NULL);
		mtk_oddmr_set_spr2rgb_dual(NULL);
		mtk_oddmr_od_smi_dual(NULL);
		mtk_oddmr_od_alloc_dram_dual();
		mtk_oddmr_od_set_dram_dual(NULL);
		table_idx = _mtk_oddmr_od_table_lookup(&g_oddmr_current_timing);

		//for 6985 force en clk, need to restore od_hsk later
		mtk_oddmr_od_hsk_force_clk_dual(NULL);
		//init srams
		if (mtk_crtc->gce_obj.client[CLIENT_PQ])
			client = mtk_crtc->gce_obj.client[CLIENT_PQ];
		else
			client = mtk_crtc->gce_obj.client[CLIENT_CFG];
		cmdq_mbox_enable(client->chan);

		if (IS_TABLE_VALID(0, g_od_param.valid_table)) {
			CRTC_MMP_MARK(0, oddmr_ctl, 0, 0);
			mtk_oddmr_create_gce_pkt(&mtk_crtc->base,
				&g_oddmr_priv->od_data.od_sram_pkgs[0]);
			mtk_oddmr_od_init_sram(default_comp,
				g_oddmr_priv->od_data.od_sram_pkgs[0], 0, 0);
			g_oddmr_priv->od_data.od_sram_table_idx[0] = 0;
			CRTC_MMP_MARK(0, oddmr_ctl, 0, 1);
			cmdq_pkt_flush(g_oddmr_priv->od_data.od_sram_pkgs[0]);
			CRTC_MMP_MARK(0, oddmr_ctl, 0, 2);
		} else {
			ODDMRFLOW_LOG("table0 must be valid\n");
			g_oddmr_priv->od_state = ODDMR_LOAD_PARTS;
			mtk_oddmr_release_clock();
			mtk_drm_set_idlemgr(&mtk_crtc->base, 1, 1);
			return -1;
		}
		if (IS_TABLE_VALID(1, g_od_param.valid_table)) {
			CRTC_MMP_MARK(0, oddmr_ctl, 0, 3);
			mtk_oddmr_create_gce_pkt(&mtk_crtc->base,
				&g_oddmr_priv->od_data.od_sram_pkgs[1]);
			mtk_oddmr_od_init_sram(default_comp,
				g_oddmr_priv->od_data.od_sram_pkgs[1], 1, 1);
			g_oddmr_priv->od_data.od_sram_table_idx[1] = 1;
			CRTC_MMP_MARK(0, oddmr_ctl, 0, 4);
			cmdq_pkt_flush(g_oddmr_priv->od_data.od_sram_pkgs[1]);
			CRTC_MMP_MARK(0, oddmr_ctl, 0, 5);
		}
		if (table_idx == 1) {
			mtk_oddmr_write(default_comp, 0x20, DISP_ODDMR_OD_SRAM_CTRL_0, NULL);
			g_oddmr_priv->od_data.od_sram_read_sel = 1;
		} else {
			mtk_oddmr_write(default_comp, 0x10, DISP_ODDMR_OD_SRAM_CTRL_0, NULL);
			g_oddmr_priv->od_data.od_sram_read_sel = 0;
		}
		if (default_comp->mtk_crtc->is_dual_pipe) {
			CRTC_MMP_MARK(0, oddmr_ctl, 1, 0);
			mtk_oddmr_create_gce_pkt(&mtk_crtc->base,
				&g_oddmr1_priv->od_data.od_sram_pkgs[0]);
			mtk_oddmr_od_init_sram(oddmr1_default_comp,
				g_oddmr1_priv->od_data.od_sram_pkgs[0], 0, 0);
			g_oddmr1_priv->od_data.od_sram_table_idx[0] = 0;
			CRTC_MMP_MARK(0, oddmr_ctl, 1, 1);
			cmdq_pkt_flush(g_oddmr1_priv->od_data.od_sram_pkgs[0]);
			CRTC_MMP_MARK(0, oddmr_ctl, 1, 2);
			if (IS_TABLE_VALID(1, g_od_param.valid_table)) {
				CRTC_MMP_MARK(0, oddmr_ctl, 1, 3);
				mtk_oddmr_create_gce_pkt(&mtk_crtc->base,
					&g_oddmr1_priv->od_data.od_sram_pkgs[1]);
				mtk_oddmr_od_init_sram(oddmr1_default_comp,
					g_oddmr1_priv->od_data.od_sram_pkgs[1], 1, 1);
				g_oddmr1_priv->od_data.od_sram_table_idx[1] = 1;
				CRTC_MMP_MARK(0, oddmr_ctl, 1, 4);
				cmdq_pkt_flush(g_oddmr1_priv->od_data.od_sram_pkgs[1]);
				CRTC_MMP_MARK(0, oddmr_ctl, 1, 5);
			}
			if (table_idx == 1) {
				mtk_oddmr_write(oddmr1_default_comp,
					0x20, DISP_ODDMR_OD_SRAM_CTRL_0, NULL);
				g_oddmr1_priv->od_data.od_sram_read_sel = 1;
			} else {
				mtk_oddmr_write(oddmr1_default_comp,
					0x10, DISP_ODDMR_OD_SRAM_CTRL_0, NULL);
				g_oddmr1_priv->od_data.od_sram_read_sel = 0;
			}
		}
		cmdq_mbox_disable(client->chan);

		mtk_oddmr_od_set_res_dual(NULL);
		mtk_oddmr_set_crop_dual(NULL);
		g_oddmr_priv->od_state = ODDMR_INIT_DONE;
		mtk_oddmr_release_clock();
		ret = mtk_crtc_user_cmd(&default_comp->mtk_crtc->base,
				default_comp, ODDMR_CMD_OD_INIT_END, NULL);
	}
	mtk_drm_set_idlemgr(&mtk_crtc->base, 1, 1);
	return ret;
}

static int mtk_oddmr_od_enable(struct drm_device *dev, int en)
{
	int ret, enable = en;

	ODDMRAPI_LOG("%d\n", enable);
	if (default_comp == NULL || g_oddmr_priv == NULL) {
		ODDMRFLOW_LOG("od comp is NULL\n");
		return -1;
	}
	if (g_oddmr_priv->od_state < ODDMR_INIT_DONE) {
		ODDMRFLOW_LOG("od can not enable, state %d\n", g_oddmr_priv->od_state);
		return -EFAULT;
	}
	mtk_drm_idlemgr_kick(__func__,
			&default_comp->mtk_crtc->base, 1);
	ret = mtk_oddmr_acquire_clock();
	if (ret == 0)
		mtk_oddmr_release_clock();
	else {
		ODDMRFLOW_LOG("clock not on %d\n", ret);
		return ret;
	}

	if (enable) {
		int tmp = g_oddmr_priv->od_enable;

		g_oddmr_priv->od_enable = enable;
		if (default_comp->mtk_crtc->is_dual_pipe)
			g_oddmr1_priv->od_enable = enable;
		atomic_set(&g_oddmr_od_hrt_done, 2);
		drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, dev);
		//mtk_crtc_check_trigger(default_comp->mtk_crtc, true, true);
		ret = wait_event_interruptible_timeout(g_oddmr_hrt_wq,
				atomic_read(&g_oddmr_od_hrt_done) == 1, msecs_to_jiffies(200));
		if (ret <= 0) {
			atomic_set(&g_oddmr_od_hrt_done, 0);
			ODDMRFLOW_LOG("enable %d repaint timeout %d\n", enable, ret);
			g_oddmr_priv->od_enable = tmp;
			if (default_comp->mtk_crtc->is_dual_pipe)
				g_oddmr1_priv->od_enable = tmp;
			return -EAGAIN;
		}
		ret = mtk_crtc_user_cmd(&default_comp->mtk_crtc->base,
				default_comp, ODDMR_CMD_OD_ENABLE, &enable);
		if (ret != 0) {
			ODDMRFLOW_LOG("enable %d fail %d\n", enable, ret);
			g_oddmr_priv->od_enable = tmp;
			if (default_comp->mtk_crtc->is_dual_pipe)
				g_oddmr1_priv->od_enable = tmp;
			return -EAGAIN;
		}
	} else {
		ret = mtk_crtc_user_cmd(&default_comp->mtk_crtc->base,
				default_comp, ODDMR_CMD_OD_ENABLE, &enable);
		if (ret != 0) {
			ODDMRFLOW_LOG("enable %d fail %d\n", enable, ret);
			return -EAGAIN;
		}
		g_oddmr_priv->od_enable = enable;
		if (default_comp->mtk_crtc->is_dual_pipe)
			g_oddmr1_priv->od_enable = enable;
		atomic_set(&g_oddmr_od_hrt_done, 2);
		drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, dev);
		//mtk_crtc_check_trigger(default_comp->mtk_crtc, true, true);
		ret = wait_event_interruptible_timeout(g_oddmr_hrt_wq,
				atomic_read(&g_oddmr_od_hrt_done) == 1, msecs_to_jiffies(200));
		if (ret <= 0)
			ODDMRFLOW_LOG("enable %d repaint timeout %d\n", enable, ret);
	}
	return ret;
}

static void mtk_oddmr_dmr_set_table_mode(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	uint32_t dmr_table_mode, layer_mode_en, block_size_mode, layer_mode, rgb_mode;

	ODDMRAPI_LOG("+\n");
	dmr_table_mode = g_dmr_param.dmr_basic_info.basic_param.dmr_table_mode;
	switch (dmr_table_mode) {
	case DMR_MODE_TYPE_RGB8X8L4:
		layer_mode_en = 1;
		block_size_mode = 2;
		layer_mode = 0;
		rgb_mode = 0;
		break;
	case DMR_MODE_TYPE_RGB8X8L8:
		layer_mode_en = 1;
		block_size_mode = 2;
		layer_mode = 1;
		rgb_mode = 0;
		break;
	case DMR_MODE_TYPE_RGB4X4L4:
		layer_mode_en = 1;
		block_size_mode = 1;
		layer_mode = 0;
		rgb_mode = 0;
		break;
	case DMR_MODE_TYPE_RGB4X4L8:
		layer_mode_en = 1;
		block_size_mode = 1;
		layer_mode = 1;
		rgb_mode = 0;
		break;
	case DMR_MODE_TYPE_W2X2L4:
		layer_mode_en = 1;
		block_size_mode = 0;
		layer_mode = 0;
		rgb_mode = 0;
		break;
	case DMR_MODE_TYPE_W2X2Q:
		layer_mode_en = 0;
		block_size_mode = 0;
		layer_mode = 0;
		rgb_mode = 0;
		break;
	case DMR_MODE_TYPE_RGB4X4Q:
		layer_mode_en = 0;
		block_size_mode = 1;
		layer_mode = 0;
		rgb_mode = 1;
		break;
	case DMR_MODE_TYPE_W4X4Q:
		layer_mode_en = 0;
		block_size_mode = 1;
		layer_mode = 0;
		rgb_mode = 0;
		break;
	case DMR_MODE_TYPE_RGB7X8Q:
		layer_mode_en = 0;
		block_size_mode = 2;
		layer_mode = 0;
		rgb_mode = 0;
		break;
	default:
		layer_mode_en = 1;
		block_size_mode = 1;
		layer_mode = 1;
		rgb_mode = 0;
		break;
	}
	mtk_oddmr_write(comp, layer_mode_en, DISP_ODDMR_REG_DMR_LAYER_MODE_EN, pkg);
	mtk_oddmr_write(comp, block_size_mode, DISP_ODDMR_REG_DMR_BLOCK_SIZE_MODE, pkg);
	mtk_oddmr_write(comp, layer_mode, DISP_ODDMR_REG_DMR_LAYER_MODE, pkg);
	mtk_oddmr_write(comp, rgb_mode, DISP_ODDMR_REG_DMR_RGB_MODE, pkg);
}
/* top, dither,common pq*/
static void mtk_oddmr_dmr_common_init(struct mtk_ddp_comp *comp, struct cmdq_pkt *pkg)
{
	ODDMRAPI_LOG("+\n");
	/* top */
	//mtk_oddmr_write(comp, 1, DISP_ODDMR_TOP_CTR_2, pkg);
	mtk_oddmr_write(comp, 1, DISP_ODDMR_TOP_DMR_BYASS, pkg);
	mtk_oddmr_write(comp, 1, DISP_ODDMR_REG_DMR_EN, pkg);
	mtk_oddmr_write(comp, 1, DISP_ODDMR_REG_DMR_HVSP_6BLOCK_EN, pkg);
	mtk_oddmr_write(comp, 1, DISP_ODDMR_TOP_CLK_GATING, pkg);
	mtk_oddmr_dmr_set_table_mode(comp, pkg);
	if (g_dmr_param.dmr_basic_info.basic_param.dither_sel == 1) {
		mtk_oddmr_write(comp, g_dmr_param.dmr_basic_info.basic_param.dither_ctl,
				DISP_ODDMR_DITHER_CTR_1, pkg);
		mtk_oddmr_write(comp, 0, DISP_ODDMR_DITHER_12TO11_BYPASS, pkg);
		mtk_oddmr_write(comp, 1, DISP_ODDMR_DITHER_12TO10_BYPASS, pkg);
	} else if (g_dmr_param.dmr_basic_info.basic_param.dither_sel == 2) {
		mtk_oddmr_write(comp, g_dmr_param.dmr_basic_info.basic_param.dither_ctl,
				DISP_ODDMR_DITHER_CTR_2, pkg);
		mtk_oddmr_write(comp, 1, DISP_ODDMR_DITHER_12TO11_BYPASS, pkg);
		mtk_oddmr_write(comp, 0, DISP_ODDMR_DITHER_12TO10_BYPASS, pkg);
	} else if (g_dmr_param.dmr_basic_info.basic_param.dither_sel == 0) {
		mtk_oddmr_write(comp, 1, DISP_ODDMR_DITHER_12TO11_BYPASS, pkg);
		mtk_oddmr_write(comp, 1, DISP_ODDMR_DITHER_12TO10_BYPASS, pkg);
	}
	/* dmr basic */
	mtk_oddmr_set_pq(comp, pkg, &g_dmr_param.dmr_basic_info.basic_pq);
}

static void mtk_oddmr_dmr_common_init_dual(struct cmdq_pkt *pkg)
{
	ODDMRAPI_LOG("+\n");
	mtk_oddmr_dmr_common_init(default_comp, pkg);
	if (default_comp->mtk_crtc->is_dual_pipe)
		mtk_oddmr_dmr_common_init(oddmr1_default_comp, pkg);
}
static int mtk_oddmr_dmr_init(void)
{
	struct mtk_drm_crtc *mtk_crtc;
	int ret, table_idx = 0;

	ODDMRAPI_LOG("+\n");
	if (default_comp == NULL || g_oddmr_priv == NULL || default_comp->mtk_crtc == NULL) {
		ODDMRFLOW_LOG("oddmr comp is NULL\n");
		return -1;
	}
	if (!is_oddmr_dmr_support) {
		ODDMRFLOW_LOG("dmr is not support\n");
		return -1;
	}
	if (g_oddmr_priv->dmr_state != ODDMR_LOAD_PARTS) {
		ODDMRFLOW_LOG("dmr can not init, state %d\n", g_oddmr_priv->dmr_state);
		return -1;
	}
	if (g_oddmr_priv->dmr_enable > 0) {
		ODDMRFLOW_LOG("dmr can not init when running\n");
		return -1;
	}
	if (g_dmr_param.dmr_basic_info.basic_param.table_cnt != g_dmr_param.valid_table_cnt) {
		g_oddmr_priv->dmr_state = ODDMR_INVALID;
		ODDMRFLOW_LOG("tables are not fully loaded,cnts %d, valid %d\n",
				g_dmr_param.dmr_basic_info.basic_param.table_cnt,
				g_dmr_param.valid_table_cnt);
		return -1;
	}
	if (!mtk_oddmr_match_panelid(&g_panelid, &g_dmr_param.dmr_basic_info.basic_param.panelid)) {
		ODDMRFLOW_LOG("panelid does not match\n");
		return -1;
	}
	mtk_drm_idlemgr_kick(__func__,
			&default_comp->mtk_crtc->base, 1);
	//mtk_crtc_check_trigger(default_comp->mtk_crtc, false, true);
	ret = mtk_oddmr_acquire_clock();
	if (ret == 0) {
		mtk_crtc = default_comp->mtk_crtc;
		g_oddmr_priv->dmr_state = ODDMR_LOAD_DONE;
		mtk_oddmr_dmr_common_init_dual(NULL);
		mtk_oddmr_set_spr2rgb_dual(NULL);
		mtk_oddmr_dmr_smi_dual(NULL);
		table_idx = mtk_oddmr_dmr_table_lookup(g_oddmr_priv, &g_oddmr_current_timing);
		if ((table_idx < 0) ||
				!IS_TABLE_VALID(table_idx, g_dmr_param.valid_table))
			table_idx = 0;
		ret = mtk_oddmr_dmr_alloc_table_dual(table_idx);
		if (ret < 0) {
			mtk_oddmr_release_clock();
			g_oddmr_priv->dmr_state = ODDMR_LOAD_PARTS;
			return ret;
		}
		ret = mtk_oddmr_dmr_set_table_dual(NULL, table_idx);
		if (ret < 0) {
			mtk_oddmr_release_clock();
			g_oddmr_priv->dmr_state = ODDMR_LOAD_PARTS;
			return ret;
		}
		g_oddmr_priv->dmr_state = ODDMR_INIT_DONE;
		mtk_oddmr_release_clock();
	}
	return ret;
}

static int mtk_oddmr_dmr_enable(struct drm_device *dev, bool en)
{
	int ret, enable = en;

	ODDMRAPI_LOG("%d\n", enable);
	if (default_comp == NULL || g_oddmr_priv == NULL) {
		ODDMRFLOW_LOG("od comp is NULL\n");
		return -1;
	}
	if (g_oddmr_priv->dmr_state < ODDMR_INIT_DONE) {
		ODDMRFLOW_LOG("can not enable, state %d\n", g_oddmr_priv->dmr_state);
		return -EFAULT;
	}
	mtk_drm_idlemgr_kick(__func__,
			&default_comp->mtk_crtc->base, 1);
	ret = mtk_oddmr_acquire_clock();
	if (ret == 0)
		mtk_oddmr_release_clock();
	else {
		ODDMRFLOW_LOG("clock not on %d\n", ret);
		return ret;
	}

	if (enable) {
		int tmp = g_oddmr_priv->dmr_enable;

		g_oddmr_priv->dmr_enable = enable;
		if (default_comp->mtk_crtc->is_dual_pipe)
			g_oddmr1_priv->dmr_enable = enable;
		atomic_set(&g_oddmr_dmr_hrt_done, 2);
		drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, dev);
		ret = wait_event_interruptible_timeout(g_oddmr_hrt_wq,
				atomic_read(&g_oddmr_dmr_hrt_done) == 1, msecs_to_jiffies(200));
		if (ret <= 0) {
			atomic_set(&g_oddmr_dmr_hrt_done, 0);
			ODDMRFLOW_LOG("enable %d repaint timeout %d\n", enable, ret);
			g_oddmr_priv->dmr_enable = tmp;
			if (default_comp->mtk_crtc->is_dual_pipe)
				g_oddmr1_priv->dmr_enable = tmp;
			return -EAGAIN;
		}
		ret = mtk_crtc_user_cmd(&default_comp->mtk_crtc->base,
				default_comp, ODDMR_CMD_DMR_ENABLE, &enable);
		if (ret != 0) {
			ODDMRFLOW_LOG("enable %d fail %d\n", enable, ret);
			g_oddmr_priv->dmr_enable = tmp;
			if (default_comp->mtk_crtc->is_dual_pipe)
				g_oddmr1_priv->dmr_enable = tmp;
			return -EAGAIN;
		}
	} else {
		ret = mtk_crtc_user_cmd(&default_comp->mtk_crtc->base,
				default_comp, ODDMR_CMD_DMR_ENABLE, &enable);
		if (ret != 0) {
			ODDMRFLOW_LOG("enable %d fail %d\n", enable, ret);
			return -EAGAIN;
		}
		g_oddmr_priv->dmr_enable = enable;
		if (default_comp->mtk_crtc->is_dual_pipe)
			g_oddmr1_priv->dmr_enable = enable;
		atomic_set(&g_oddmr_dmr_hrt_done, 2);
		drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, dev);
		ret = wait_event_interruptible_timeout(g_oddmr_hrt_wq,
				atomic_read(&g_oddmr_dmr_hrt_done) == 1, msecs_to_jiffies(200));
		if (ret <= 0)
			ODDMRFLOW_LOG("enable %d repaint timeout %d\n", enable, ret);

	}
	return ret;
}

static int mtk_oddmr_od_sram_read(struct mtk_ddp_comp *comp, int table_idx,
			struct mtk_oddmr_sw_reg *sw_reg, struct mtk_oddmr_od_param *pparam)
{
	struct mtk_oddmr_od_table *table;
	uint8_t *raw_table;
	uint32_t table_size, map_id, *val;
	int idx;

	if (!IS_TABLE_VALID(table_idx, pparam->valid_table)) {
		ODDMRFLOW_LOG("table %d is invalid\n", table_idx);
		return -EFAULT;
	}
	table = pparam->od_tables[table_idx];
	raw_table = table->raw_table.value;
	table_size = table->raw_table.size;

	map_id = (sw_reg->reg & 0xFFFF) / 4;
	val = &sw_reg->val;
	idx = map_id - OD_TABLE_B_SRAM1_START;
	if (idx >= table_size) {
		ODDMRFLOW_LOG("table%d idx %d outof size %d\n", table_idx, idx, table_size);
		return -EFAULT;
	}
	*val = raw_table[idx];
	ODDMRAPI_LOG("map_id 0x%x table_idx %d, idx %d val %u\n", map_id, table_idx, idx, *val);
	return 0;
}

static int mtk_oddmr_od_sram_write(struct mtk_ddp_comp *comp, int table_idx,
			struct mtk_oddmr_sw_reg *sw_reg, struct mtk_oddmr_od_param *pparam)
{
	struct mtk_oddmr_od_table *table;
	uint8_t *raw_table;
	uint32_t table_size, map_id, val;
	int idx, ret = 0;
	struct mtk_oddmr_od_tuning_sram tuning_data = {0};

	if (!IS_TABLE_VALID(table_idx, pparam->valid_table)) {
		ODDMRFLOW_LOG("table %d is invalid\n", table_idx);
		return -EFAULT;
	}
	table = pparam->od_tables[table_idx];
	raw_table = table->raw_table.value;
	table_size = table->raw_table.size;

	map_id = (sw_reg->reg & 0xFFFF) / 4;
	val = sw_reg->val;
	idx = map_id - OD_TABLE_B_SRAM1_START;
	if (idx >= table_size) {
		ODDMRFLOW_LOG("table%d idx %d outof size %d\n", table_idx, idx, table_size);
		return -EFAULT;
	}
	//raw_table[idx] = val;
	/* B:0-bit1, G:1-bit2, R:2-bit3*/
	if (map_id >= OD_TABLE_B_SRAM1_START && map_id <= OD_TABLE_B_SRAM1_END) {
		tuning_data.channel = 0;
		tuning_data.sram = 1;
		tuning_data.idx = map_id - OD_TABLE_B_SRAM1_START;
	} else if (map_id >= OD_TABLE_B_SRAM2_START && map_id <= OD_TABLE_B_SRAM2_END) {
		tuning_data.channel = 0;
		tuning_data.sram = 2;
		tuning_data.idx = map_id - OD_TABLE_B_SRAM2_START;
	} else if (map_id >= OD_TABLE_B_SRAM3_START && map_id <= OD_TABLE_B_SRAM3_END) {
		tuning_data.channel = 0;
		tuning_data.sram = 3;
		tuning_data.idx = map_id - OD_TABLE_B_SRAM3_START;
	} else if (map_id >= OD_TABLE_B_SRAM4_START && map_id <= OD_TABLE_B_SRAM4_END) {
		tuning_data.channel = 0;
		tuning_data.sram = 4;
		tuning_data.idx = map_id - OD_TABLE_B_SRAM4_START;
	} else if (map_id >= OD_TABLE_G_SRAM1_START && map_id <= OD_TABLE_G_SRAM1_END) {
		tuning_data.channel = 1;
		tuning_data.sram = 1;
		tuning_data.idx = map_id - OD_TABLE_G_SRAM1_START;
	} else if (map_id >= OD_TABLE_G_SRAM2_START && map_id <= OD_TABLE_G_SRAM2_END) {
		tuning_data.channel = 1;
		tuning_data.sram = 2;
		tuning_data.idx = map_id - OD_TABLE_G_SRAM2_START;
	} else if (map_id >= OD_TABLE_G_SRAM3_START && map_id <= OD_TABLE_G_SRAM3_END) {
		tuning_data.channel = 1;
		tuning_data.sram = 3;
		tuning_data.idx = map_id - OD_TABLE_G_SRAM3_START;
	} else if (map_id >= OD_TABLE_G_SRAM4_START && map_id <= OD_TABLE_G_SRAM4_END) {
		tuning_data.channel = 1;
		tuning_data.sram = 4;
		tuning_data.idx = map_id - OD_TABLE_G_SRAM4_START;
	} else if (map_id >= OD_TABLE_R_SRAM1_START && map_id <= OD_TABLE_R_SRAM1_END) {
		tuning_data.channel = 2;
		tuning_data.sram = 1;
		tuning_data.idx = map_id - OD_TABLE_R_SRAM1_START;
	} else if (map_id >= OD_TABLE_R_SRAM2_START && map_id <= OD_TABLE_R_SRAM2_END) {
		tuning_data.channel = 2;
		tuning_data.sram = 2;
		tuning_data.idx = map_id - OD_TABLE_R_SRAM2_START;
	} else if (map_id >= OD_TABLE_R_SRAM3_START && map_id <= OD_TABLE_R_SRAM3_END) {
		tuning_data.channel = 2;
		tuning_data.sram = 3;
		tuning_data.idx = map_id - OD_TABLE_R_SRAM3_START;
	} else if (map_id >= OD_TABLE_R_SRAM4_START && map_id <= OD_TABLE_R_SRAM4_END) {
		tuning_data.channel = 2;
		tuning_data.sram = 4;
		tuning_data.idx = map_id - OD_TABLE_R_SRAM4_START;
	} else {
		ODDMRFLOW_LOG("map_id 0x%x outof bound\n", map_id);
		return -EFAULT;
	}
	tuning_data.value = val;

	ret = mtk_crtc_user_cmd(&comp->mtk_crtc->base, comp,
		ODDMR_CMD_OD_TUNING_WRITE_SRAM, &tuning_data);
	if (ret == 1)
		ret = -EFAULT;
	ODDMRAPI_LOG("ret %d map_id:0x%x(%u,%u,%u,%u)\n", ret, map_id,
		tuning_data.channel, tuning_data.sram, tuning_data.idx, tuning_data.value);
	return ret;
}

static int mtk_oddmr_od_status_read(struct mtk_ddp_comp *comp, int table_idx,
			struct mtk_oddmr_sw_reg *sw_reg, struct mtk_oddmr_timing *timing)
{
	uint32_t map_id, *val;
	int ret = 0;

	map_id = (sw_reg->reg & 0xFFFF) / 4;
	val = &sw_reg->val;
	switch (map_id) {
	case OD_CURRENT_TABLE:
		*val = table_idx;
		break;
	case OD_CURRENT_BL:
		*val = timing->bl_level;
		break;
	case OD_CURRENT_FPS:
		*val = timing->vrefresh;
		break;
	case OD_CURRENT_HDISPLAY:
		*val = timing->hdisplay;
		break;
	case OD_CURRENT_VDISPLAY:
		*val = timing->vdisplay;
		break;
	default:
		ret = -EFAULT;
		break;
	}
	ODDMRAPI_LOG("map_id 0x%x, %d\n", map_id, *val);
	return ret;
}

static int mtk_oddmr_od_read_sw_reg(struct mtk_ddp_comp *comp, struct mtk_drm_oddmr_ctl *ctl_data,
			struct mtk_oddmr_od_param *pparam)
{
	int ret, table_idx;
	struct mtk_oddmr_sw_reg sw_reg;
	uint32_t map_id;
	struct mtk_disp_oddmr *priv;

	if (ctl_data == NULL ||
		ctl_data->data == NULL ||
		ctl_data->size != sizeof(struct mtk_oddmr_sw_reg)) {
		ODDMRFLOW_LOG("ctl_data is invalid\n");
		return -EFAULT;
	}
	if (comp == NULL || pparam == NULL) {
		ODDMRFLOW_LOG("comp,pparam is NULL\n");
		return -EFAULT;
	}
	if (copy_from_user(&sw_reg, ctl_data->data, ctl_data->size)) {
		ODDMRFLOW_LOG("copy_from_user fail\n");
		ret = -EFAULT;
		goto out;
	}
	priv = comp_to_oddmr(comp);
	table_idx = priv->od_data.od_sram_table_idx[!!priv->od_data.od_sram_read_sel];
	if (!IS_TABLE_VALID(table_idx, pparam->valid_table)) {
		ODDMRFLOW_LOG("table %d is invalid\n", table_idx);
		return -EFAULT;
	}
	map_id = (sw_reg.reg & 0xFFFF) / 4;
	if (map_id >= OD_TABLE_B_SRAM1_START && map_id <= OD_TABLE_R_SRAM4_END)
		ret = mtk_oddmr_od_sram_read(comp, table_idx, &sw_reg, pparam);
	else if (map_id <= OD_CURRENT_VDISPLAY)
		ret = mtk_oddmr_od_status_read(comp, table_idx, &sw_reg, &g_oddmr_current_timing);
	else
		ret = mtk_oddmr_od_tuning_read(comp, table_idx, &sw_reg, pparam);
	if (copy_to_user(ctl_data->data, &sw_reg, ctl_data->size)) {
		ODDMRFLOW_LOG("copy_to_user fail\n");
		ret = -EFAULT;
		goto out;
	}
	ODDMRFLOW_LOG("reg 0x%x 0x%x 0x%x+\n", sw_reg.reg, sw_reg.val, ctl_data->size);
out:
	return ret;
}

static int mtk_oddmr_od_write_sw_reg(struct mtk_ddp_comp *comp, struct mtk_drm_oddmr_ctl *ctl_data,
			struct mtk_oddmr_od_param *pparam)
{
	int ret, table_idx;
	struct mtk_oddmr_sw_reg sw_reg;
	uint32_t map_id;
	struct mtk_disp_oddmr *priv;

	if (ctl_data == NULL ||
		ctl_data->data == NULL ||
		ctl_data->size != sizeof(struct mtk_oddmr_sw_reg)) {
		ODDMRFLOW_LOG("ctl_data is invalid\n");
		return -EFAULT;
	}
	if (comp == NULL || pparam == NULL) {
		ODDMRFLOW_LOG("comp,pparam is NULL\n");
		return -EFAULT;
	}
	if (copy_from_user(&sw_reg, ctl_data->data, ctl_data->size)) {
		ODDMRFLOW_LOG("copy_from_user fail\n");
		ret = -EFAULT;
		goto out;
	}
	ODDMRFLOW_LOG("reg 0x%x val 0x%x+\n", sw_reg.reg, sw_reg.val);
	priv = comp_to_oddmr(comp);
	table_idx = priv->od_data.od_sram_table_idx[!!priv->od_data.od_sram_read_sel];
	if (!IS_TABLE_VALID(table_idx, pparam->valid_table)) {
		ODDMRFLOW_LOG("table %d is invalid\n", table_idx);
		return -EFAULT;
	}
	map_id = (sw_reg.reg & 0xFFFF) / 4;
	if (map_id >= OD_TABLE_B_SRAM1_START && map_id <= OD_TABLE_R_SRAM4_END)
		ret = mtk_oddmr_od_sram_write(comp, table_idx, &sw_reg, pparam);
	else
		ret = mtk_oddmr_od_tuning_write(comp, table_idx, &sw_reg, pparam);
out:
	return ret;
}

/* all oddmr ioctl use handle dualpipe itself because it is not int drm atomic flow */
int mtk_drm_ioctl_oddmr_ctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	int ret;
	struct mtk_drm_oddmr_ctl *param = data;

	ODDMRAPI_LOG("+\n");
	if (param == NULL) {
		ODDMRFLOW_LOG("param is null\n");
		return -EFAULT;
	}
	switch (param->cmd) {
	case MTK_DRM_ODDMR_OD_INIT:
		ret = mtk_oddmr_od_init();
		break;
	case MTK_DRM_ODDMR_OD_ENABLE:
		ret = mtk_oddmr_od_enable(dev, 1);
		break;
	case MTK_DRM_ODDMR_OD_DISABLE:
		ret = mtk_oddmr_od_enable(dev, 0);
		break;
	case MTK_DRM_ODDMR_DMR_INIT:
		ret = mtk_oddmr_dmr_init();
		break;
	case MTK_DRM_ODDMR_DMR_ENABLE:
		ret = mtk_oddmr_dmr_enable(dev, 1);
		break;
	case MTK_DRM_ODDMR_DMR_DISABLE:
		ret = mtk_oddmr_dmr_enable(dev, 0);
		break;
	case MTK_DRM_ODDMR_OD_READ_SW_REG:
		ret = mtk_oddmr_od_read_sw_reg(default_comp, param, &g_od_param);
		break;
	case MTK_DRM_ODDMR_OD_WRITE_SW_REG:
		ret = mtk_oddmr_od_write_sw_reg(default_comp, param, &g_od_param);
		break;
	default:
		ODDMRFLOW_LOG("cmd %d is invalid\n", param->cmd);
		ret = -EINVAL;
		break;
	}
	ODDMRFLOW_LOG("cmd %d ret %d\n", param->cmd, ret);
	return ret;
}

int mtk_drm_ioctl_oddmr_load_param(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	int ret;
	struct mtk_drm_oddmr_param *param = data;

	ODDMRAPI_LOG("+\n");
	if (default_comp == NULL || g_oddmr_priv == NULL) {
		ODDMRFLOW_LOG("oddmr comp is NULL\n");
		return -1;
	}
	if (param == NULL) {
		ODDMRFLOW_LOG("param is NULL\n");
		return -EFAULT;
	}
	ret = 0;
	switch (param->head_id >> 24) {
	case ODDMR_DMR_BASIC_INFO:
	case ODDMR_DMR_TABLE:
		if (!is_oddmr_dmr_support) {
			ODDMRFLOW_LOG("dmr not support\n");
			ret = -EFAULT;
		}
		break;
	case ODDMR_OD_BASIC_INFO:
	case ODDMR_OD_TABLE:
		if (!is_oddmr_od_support) {
			ODDMRFLOW_LOG("od not support\n");
			ret = -EFAULT;
		}
		break;
	default:
		ret = -EFAULT;
		break;
	}
	if (ret < 0)
		return ret;
	/*
	 * If any section is loaded, set all to loading,
	 * set loading done in init to support partial loading.
	 */
	ret = mtk_oddmr_load_param(g_oddmr_priv, data);
	return ret;
}

static const struct mtk_ddp_comp_funcs mtk_disp_oddmr_funcs = {
	.config = mtk_oddmr_config,
	.start = mtk_oddmr_start,
	.stop = mtk_oddmr_stop,
	.prepare = mtk_oddmr_prepare,
	.unprepare = mtk_oddmr_unprepare,
	.io_cmd = mtk_oddmr_io_cmd,
	.user_cmd = mtk_oddmr_user_cmd,
	.first_cfg = mtk_oddmr_first_cfg,
	//.config_overhead = mtk_disp_oddmr_config_overhead,
};

static int mtk_disp_oddmr_bind(struct device *dev, struct device *master,
		void *data)
{
	struct mtk_disp_oddmr *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct mtk_drm_private *private = drm_dev->dev_private;
	struct mtk_disp_oddmr *oddmr_priv;
	int ret;
	char buf[50];

	pr_notice("%s+\n", __func__);
	ret = mtk_ddp_comp_register(drm_dev, &priv->ddp_comp);
	if (ret < 0) {
		dev_err(dev, "Failed to register component %s: %d\n",
				dev->of_node->full_name, ret);
		return ret;
	}
	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT)) {
		oddmr_priv = comp_to_oddmr(&priv->ddp_comp);
		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "DMRR");
		oddmr_priv->qos_req_dmrr = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "ODR");
		oddmr_priv->qos_req_odr = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "ODW");
		oddmr_priv->qos_req_odw = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "DMRR_HRT");
		oddmr_priv->qos_req_dmrr_hrt = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "ODR_HRT");
		oddmr_priv->qos_req_odr_hrt = of_mtk_icc_get(dev, buf);

		mtk_disp_pmqos_get_icc_path_name(buf, sizeof(buf),
						&priv->ddp_comp, "ODW_HRT");
		oddmr_priv->qos_req_odw_hrt = of_mtk_icc_get(dev, buf);

	}
	pr_notice("%s-\n", __func__);
	return 0;
}

static void mtk_disp_oddmr_unbind(struct device *dev, struct device *master,
		void *data)
{
	struct mtk_disp_oddmr *priv = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;

	pr_notice("%s+\n", __func__);
	mtk_ddp_comp_unregister(drm_dev, &priv->ddp_comp);
}

static const struct component_ops mtk_disp_oddmr_component_ops = {
	.bind = mtk_disp_oddmr_bind,
	.unbind = mtk_disp_oddmr_unbind,
};

static unsigned int oddmr_err_trigger;
unsigned int check_oddmr_err_event(void)
{
	return oddmr_err_trigger;
}

void clear_oddmr_err_event(void)
{
	DDPMSG("%s, do clear underrun event\n", __func__);
	oddmr_err_trigger = 0;
}

#ifdef ODDMR_ENABLE_IRQ
/*
 * Check frame done status at sof.
 * It is a err detection with delay,
 * should be replaced when better ways come along.
 */
static irqreturn_t mtk_oddmr_check_framedone(int irq, void *dev_id)
{
	struct mtk_disp_oddmr *oddmr_priv = dev_id;
	struct mtk_ddp_comp *comp;
	struct mtk_drm_private *priv = NULL;
	int opt = 0;
	unsigned int ret = 0;
	uint32_t status_raw, status, od_enable;

	if (IS_ERR_OR_NULL(oddmr_priv))
		return IRQ_NONE;

	if (mtk_drm_top_clk_isr_get("oddmr_irq") == false) {
		DDPIRQ("%s, top clk off\n", __func__);
		return IRQ_NONE;
	}
	comp = &oddmr_priv->ddp_comp;
	status_raw = mtk_oddmr_read(comp, DISP_ODDMR_IRQ_RAW_STATUS);
	status = mtk_oddmr_read(comp, DISP_ODDMR_IRQ_STATUS);
	mtk_oddmr_write(comp, status, DISP_ODDMR_IRQ_CLEAR, NULL);
	mtk_oddmr_write(comp, 0, DISP_ODDMR_IRQ_CLEAR, NULL);
	CRTC_MMP_MARK(0, oddmr_ctl, status_raw, status);
	//DDP_IRQ
	DDPIRQ("%s %s irq, val:0x%x,0x%x\n", __func__, mtk_dump_comp_str(comp),
		status_raw, status);
	if (status &
		(DISP_ODDMR_IRQ_FRAME_DONE |
		DISP_ODDMR_IRQ_ODW_DONE |
		DISP_ODDMR_IRQ_I_FRM_DNE |
		DISP_ODDMR_IRQ_O_FRM_DNE)) {
		//only care eof follows sof
		if (oddmr_priv->irq_status & DISP_ODDMR_IRQ_SOF)
			oddmr_priv->irq_status |= status;
		DDPIRQ("%s %s eof status:0x%x,0x%x\n", __func__, mtk_dump_comp_str(comp),
			status, oddmr_priv->irq_status);
	}
	if (status & DISP_ODDMR_IRQ_SOF) {
		status = oddmr_priv->irq_status | status;

		/* reset irq status */
		od_enable = mtk_oddmr_read(comp, DISP_ODDMR_OD_CTRL_EN) & 0x01;
		oddmr_priv->irq_status = status & DISP_ODDMR_IRQ_SOF;
		/* do not care eof status when disable */
		if (!od_enable)
			oddmr_priv->irq_status = 0;
		DDPIRQ("%s %s sof status:0x%x,0x%x\n", __func__, mtk_dump_comp_str(comp),
			status, oddmr_priv->irq_status);
		if ((status & DISP_ODDMR_IRQ_I_FRM_DNE) &&
			(status & DISP_ODDMR_IRQ_O_FRM_DNE) &&
			!(status & DISP_ODDMR_IRQ_FRAME_DONE) &&
			!(status & DISP_ODDMR_IRQ_ODW_DONE)) {
			if (od_enable) {
				bool dump_en_tmp;

				priv = comp->mtk_crtc->base.dev->dev_private;
				opt = mtk_drm_helper_get_opt(priv->helper_opt,
					MTK_DRM_OPT_ODDMR_UNDERRUN_AEE);
				oddmr_err_trigger = 1;
				DDPPR_ERR("%s %s err status 0x%x\n", __func__,
					mtk_dump_comp_str(comp), status);
				dump_en_tmp = g_oddmr_dump_en;
				g_oddmr_dump_en = true;
				mtk_drm_crtc_analysis(&(comp->mtk_crtc->base));
				mtk_drm_crtc_dump(&(comp->mtk_crtc->base));
				g_oddmr_dump_en = dump_en_tmp;
				mtk_smi_dbg_hang_detect("oddmr-underrun");
				if (opt) {
					DDPAEE("ODDMR:buffer underrun. TS: 0x%08llx\n",
						arch_timer_read_counter());
				}
			}
		}
	}
	ret = IRQ_HANDLED;
	mtk_drm_top_clk_isr_put("oddmr_irq");

	return ret;
}
#endif

static int mtk_disp_oddmr_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_disp_oddmr *priv;
	enum mtk_ddp_comp_id comp_id;
	int ret;
	struct sched_param param = {.sched_priority = 85 };
	struct cpumask mask;

	DDPMSG("%s+\n", __func__);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	comp_id = mtk_ddp_comp_get_id(dev->of_node, MTK_DISP_ODDMR);
	if ((int)comp_id < 0) {
		dev_err(dev, "Failed to identify by alias: %d\n", comp_id);
		ret = comp_id;
		goto err;
	}

	ret = mtk_ddp_comp_init(dev, dev->of_node, &priv->ddp_comp, comp_id,
			&mtk_disp_oddmr_funcs);
	if (ret) {
		dev_err(dev, "Failed to initialize component: %d\n", ret);
		goto err;
	}

	priv->data = of_device_get_match_data(dev);
	platform_set_drvdata(pdev, priv);

	if (comp_id == DDP_COMPONENT_ODDMR0) {
		priv->is_right_pipe = false;
		g_oddmr_priv = priv;
	}
	if (comp_id == DDP_COMPONENT_ODDMR1) {
		priv->is_right_pipe = true;
		g_oddmr1_priv = priv;
	}
	if (!default_comp && comp_id == DDP_COMPONENT_ODDMR0)
		default_comp = &priv->ddp_comp;
	if (!oddmr1_default_comp && comp_id == DDP_COMPONENT_ODDMR1)
		oddmr1_default_comp = &priv->ddp_comp;

#ifdef ODDMR_ENABLE_IRQ
	if (comp_id == DDP_COMPONENT_ODDMR0 ||
		comp_id == DDP_COMPONENT_ODDMR1) {
		int irq_num;

		irq_num = platform_get_irq(pdev, 0);
		if (irq_num < 0) {
			dev_err(&pdev->dev, "failed to request oddmr irq resource\n");
			ret = -EPROBE_DEFER;
			goto err;
		}
		irq_set_status_flags(irq_num, IRQ_TYPE_LEVEL_HIGH);
		ret = devm_request_irq(
			&pdev->dev, irq_num, priv->data->irq_handler,
			IRQF_TRIGGER_NONE | IRQF_SHARED, dev_name(&pdev->dev), priv);
		if (ret) {
			DDPAEE("%s:%d, failed to request irq:%d ret:%d\n",
					__func__, __LINE__,
					irq_num, ret);
			ret = -EPROBE_DEFER;
			goto err;
		}
	}
#endif

	mtk_ddp_comp_pm_enable(&priv->ddp_comp);

	ret = component_add(dev, &mtk_disp_oddmr_component_ops);
	if (ret != 0) {
		dev_err(dev, "Failed to add component: %d\n", ret);
		mtk_ddp_comp_pm_disable(&priv->ddp_comp);
		goto err;
	}

	/* probe only once */
	if (comp_id == DDP_COMPONENT_ODDMR0) {
		oddmr_sof_irq_event_task =
			kthread_create(mtk_oddmr_sof_irq_trigger,
					NULL, "oddmr_sof");
		cpumask_setall(&mask);
		cpumask_clear_cpu(0, &mask);
		set_cpus_allowed_ptr(oddmr_sof_irq_event_task, &mask);
		if (sched_setscheduler(oddmr_sof_irq_event_task, SCHED_RR, &param))
			DDPMSG("oddmr_sof_irq_event_task setschedule fail");
		wake_up_process(oddmr_sof_irq_event_task);
	}

	priv->od_data.od_sram_read_sel = -1;
	priv->od_data.od_sram_table_idx[0] = -1;
	priv->od_data.od_sram_table_idx[1] = -1;
	atomic_set(&priv->dmr_data.cur_table_idx, -1);

	DDPMSG("%s id %d, type %d\n", __func__, comp_id, mtk_ddp_comp_get_type(comp_id));
	return ret;
err:
	return ret;
}

static int mtk_disp_oddmr_remove(struct platform_device *pdev)
{
	struct mtk_disp_oddmr *priv = dev_get_drvdata(&pdev->dev);

	component_del(&pdev->dev, &mtk_disp_oddmr_component_ops);
	mtk_ddp_comp_pm_disable(&priv->ddp_comp);

	return 0;
}
static const struct mtk_disp_oddmr_data mt6985_oddmr_driver_data = {
	.is_od_support_table_update = false,
	.is_support_rtff = false,
	.is_od_support_hw_skip_first_frame = false,
	.is_od_need_crop_garbage = true,
	.is_od_need_force_clk = true,
	.is_od_support_sec = false,
	.tile_overhead = 0,
	.dmr_buffer_size = 458,
	.odr_buffer_size = 264,
	.odw_buffer_size = 264,
	.irq_handler = mtk_oddmr_check_framedone,
};

static const struct of_device_id mtk_disp_oddmr_driver_dt_match[] = {
	{ .compatible = "mediatek,mt6985-disp-oddmr",
		.data = &mt6985_oddmr_driver_data},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_disp_oddmr_driver_dt_match);

struct platform_driver mtk_disp_oddmr_driver = {
	.probe = mtk_disp_oddmr_probe,
	.remove = mtk_disp_oddmr_remove,
	.driver = {
		.name = "mediatek-disp-oddmr",
		.owner = THIS_MODULE,
		.of_match_table = mtk_disp_oddmr_driver_dt_match,
	},
};
