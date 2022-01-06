/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef MTK_DRM_CRTC_H
#define MTK_DRM_CRTC_H

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include <drm/drm_crtc.h>
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_plane.h"
#include <drm/drm_writeback.h>
#include "mtk_debug.h"
#include "mtk_log.h"
#include "mtk_panel_ext.h"
#include "mtk_drm_lowpower.h"
#include "mtk_disp_recovery.h"
#include "mtk_drm_ddp_addon.h"
#include <linux/pm_wakeup.h>
#include "mtk_disp_pmqos.h"


#define MAX_CRTC 3
#define OVL_LAYER_NR 12L
#define OVL_PHY_LAYER_NR 4L
#define RDMA_LAYER_NR 1UL
#define EXTERNAL_INPUT_LAYER_NR 2UL
#define MEMORY_INPUT_LAYER_NR 2UL
#define MAX_PLANE_NR                                                           \
	((OVL_LAYER_NR) + (EXTERNAL_INPUT_LAYER_NR) + (MEMORY_INPUT_LAYER_NR))
#define MTK_PLANE_INPUT_LAYER_COUNT (OVL_LAYER_NR)
#define MTK_LUT_SIZE 512
#define MTK_MAX_BPC 10
#define MTK_MIN_BPC 3
#define BW_MODULE 17
#define COLOR_MATRIX_PARAMS 17

#define PRIMARY_OVL_PHY_LAYER_NR 6L

#define PRIMARY_OVL_EXT_LAYER_NR 6L

#define MTK_HDR10P_PROPERTY_FLAG 2

#define pgc	_get_context()

/* TODO: BW report module should not hardcode */
enum DISP_PMQOS_SLOT {
	DISP_PMQOS_OVL0_BW = 0,
	DISP_PMQOS_OVL0_FBDC_BW,
	DISP_PMQOS_OVL1_BW,
	DISP_PMQOS_OVL1_FBDC_BW,
	DISP_PMQOS_OVL0_2L_BW,
	DISP_PMQOS_OVL0_2L_FBDC_BW,
	DISP_PMQOS_OVL1_2L_BW,
	DISP_PMQOS_OVL1_2L_FBDC_BW,
	DISP_PMQOS_OVL2_2L_BW,
	DISP_PMQOS_OVL2_2L_FBDC_BW,
	DISP_PMQOS_OVL3_2L_BW,
	DISP_PMQOS_OVL3_2L_FBDC_BW,
	DISP_PMQOS_RDMA0_BW,
	DISP_PMQOS_RDMA1_BW,
	DISP_PMQOS_RDMA2_BW,
	DISP_PMQOS_WDMA0_BW,
	DISP_PMQOS_WDMA1_BW
};

#define IGNORE_MODULE_IRQ

#define DISP_SLOT_CUR_CONFIG_FENCE_BASE 0x0000
#define DISP_SLOT_CUR_CONFIG_FENCE(n)                                          \
	(DISP_SLOT_CUR_CONFIG_FENCE_BASE + (0x4 * (n)))
#define DISP_SLOT_OVL_DSI_SEQ(n)                                          \
	(DISP_SLOT_CUR_CONFIG_FENCE(OVL_LAYER_NR) + (0x4 * (n)))
#define DISP_SLOT_PRESENT_FENCE(n)                                          \
	(DISP_SLOT_OVL_DSI_SEQ(MAX_CRTC) + (0x4 * (n)))
#define DISP_SLOT_SF_PRESENT_FENCE(n)                                          \
	(DISP_SLOT_PRESENT_FENCE(MAX_CRTC) + (0x4 * (n)))
#define DISP_SLOT_SUBTRACTOR_WHEN_FREE_BASE                                    \
	(DISP_SLOT_SF_PRESENT_FENCE(MAX_CRTC) + 0x4)
#define DISP_SLOT_SUBTRACTOR_WHEN_FREE(n)                                      \
	(DISP_SLOT_SUBTRACTOR_WHEN_FREE_BASE + (0x4 * (n)))
#define DISP_SLOT_ESD_READ_BASE DISP_SLOT_SUBTRACTOR_WHEN_FREE(OVL_LAYER_NR)
#define DISP_SLOT_PMQOS_BW_BASE                                                \
	(DISP_SLOT_ESD_READ_BASE + (ESD_CHECK_NUM * 2 * 0x4))
#define DISP_SLOT_PMQOS_BW(n) (DISP_SLOT_PMQOS_BW_BASE + ((n)*0x4))
#define DISP_SLOT_RDMA_FB_IDX_BASE (DISP_SLOT_PMQOS_BW(BW_MODULE))
#define DISP_SLOT_RDMA_FB_IDX (DISP_SLOT_RDMA_FB_IDX_BASE + 0x4)
#define DISP_SLOT_RDMA_FB_ID (DISP_SLOT_RDMA_FB_IDX + 0x4)
#define DISP_SLOT_CUR_HRT_IDX (DISP_SLOT_RDMA_FB_ID + 0x4)
#define DISP_SLOT_CUR_HRT_LEVEL (DISP_SLOT_CUR_HRT_IDX + 0x4)
#define DISP_SLOT_CUR_OUTPUT_FENCE (DISP_SLOT_CUR_HRT_LEVEL + 0x4)
#define DISP_SLOT_CUR_INTERFACE_FENCE (DISP_SLOT_CUR_OUTPUT_FENCE + 0x4)
#define DISP_SLOT_COLOR_MATRIX_PARAMS(n)                                      \
	((DISP_SLOT_CUR_INTERFACE_FENCE + 0x4) + (n) * 0x4)
#define DISP_SLOT_OVL_STATUS						       \
	(DISP_SLOT_COLOR_MATRIX_PARAMS(COLOR_MATRIX_PARAMS))
#define DISP_SLOT_LAYER_REC_BASE (DISP_SLOT_OVL_STATUS + 0x4)
#define DISP_SLOT_LAYER_REC_OVL0_2L (DISP_SLOT_LAYER_REC_BASE)
#define DISP_SLOT_LAYER_REC_OVL0 (DISP_SLOT_LAYER_REC_OVL0_2L + 0x4 * 14)
#define DISP_SLOT_LAYER_REC_END (DISP_SLOT_LAYER_REC_OVL0 + 0x4 * 18)
#define DISP_SLOT_TRIG_CNT (DISP_SLOT_LAYER_REC_END)
#define DISP_SLOT_READ_DDIC_BASE (DISP_SLOT_TRIG_CNT + 0x4)
#define DISP_SLOT_READ_DDIC_BASE_END		\
	(DISP_SLOT_READ_DDIC_BASE + READ_DDIC_SLOT_NUM * 0x4)
#define DISP_SLOT_CUR_USER_CMD_IDX (DISP_SLOT_READ_DDIC_BASE_END + 0x4)
#define DISP_SLOT_CUR_BL_IDX (DISP_SLOT_CUR_USER_CMD_IDX + 0x4)

/* For Dynamic OVL feature */
#define DISP_OVL_ROI_SIZE 0x20
#define DISP_OVL_DATAPATH_CON 0x24

/* TODO: figure out Display pipe which need report PMQOS BW */
#define DISP_SLOT_SIZE (DISP_SLOT_CUR_BL_IDX + 0x4)
#if DISP_SLOT_SIZE > CMDQ_BUF_ALLOC_SIZE
#error "DISP_SLOT_SIZE exceed CMDQ_BUF_ALLOC_SIZE"
#endif

#define to_mtk_crtc(x) container_of(x, struct mtk_drm_crtc, base)

#define to_mtk_crtc_state(x) container_of(x, struct mtk_crtc_state, base)

#define _MTK_CRTC_COLOR_FMT_ID_SHIFT 0
#define _MTK_CRTC_COLOR_FMT_ID_WIDTH 8
#define _MTK_CRTC_COLOR_FMT_RGBSWAP_SHIFT                                      \
	(_MTK_CRTC_COLOR_FMT_ID_SHIFT + _MTK_CRTC_COLOR_FMT_ID_WIDTH)
#define _MTK_CRTC_COLOR_FMT_RGBSWAP_WIDTH 1
#define _MTK_CRTC_COLOR_FMT_BYTESWAP_SHIFT                                     \
	(_MTK_CRTC_COLOR_FMT_RGBSWAP_SHIFT + _MTK_CRTC_COLOR_FMT_RGBSWAP_WIDTH)
#define _MTK_CRTC_COLOR_FMT_BYTESWAP_WIDTH 1
#define _MTK_CRTC_COLOR_FMT_FORMAT_SHIFT                                       \
	(_MTK_CRTC_COLOR_FMT_BYTESWAP_SHIFT +                                  \
	 _MTK_CRTC_COLOR_FMT_BYTESWAP_WIDTH)
#define _MTK_CRTC_COLOR_FMT_FORMAT_WIDTH 5
#define _MTK_CRTC_COLOR_FMT_VDO_SHIFT                                          \
	(_MTK_CRTC_COLOR_FMT_FORMAT_SHIFT + _MTK_CRTC_COLOR_FMT_FORMAT_WIDTH)
#define _MTK_CRTC_COLOR_FMT_VDO_WIDTH 1
#define _MTK_CRTC_COLOR_FMT_BLOCK_SHIT                                         \
	(_MTK_CRTC_COLOR_FMT_VDO_SHIFT + _MTK_CRTC_COLOR_FMT_VDO_WIDTH)
#define _MTK_CRTC_COLOR_FMT_BLOCK_WIDTH 1
#define _MTK_CRTC_COLOR_FMT_bpp_SHIFT                                          \
	(_MTK_CRTC_COLOR_FMT_BLOCK_SHIT + _MTK_CRTC_COLOR_FMT_BLOCK_WIDTH)
#define _MTK_CRTC_COLOR_FMT_bpp_WIDTH 6
#define _MTK_CRTC_COLOR_FMT_RGB_SHIFT                                          \
	(_MTK_CRTC_COLOR_FMT_bpp_SHIFT + _MTK_CRTC_COLOR_FMT_bpp_WIDTH)
#define _MTK_CRTC_COLOR_FMT_RGB_WIDTH 1

#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833)
#define GCE_BASE_ADDR 0x10228000
#define GCE_GCTL_VALUE 0x48
#define GCE_DEBUG_START_ADDR 0x1104
#define GCE_DDR_EN BIT(16)
#endif

#define _MASK_SHIFT(val, width, shift)                                         \
	(((val) >> (shift)) & ((1 << (width)) - 1))

#define REG_FLD(width, shift)                                                  \
	((unsigned int)((((width)&0xFF) << 16) | ((shift)&0xFF)))

#define REG_FLD_MSB_LSB(msb, lsb) REG_FLD((msb) - (lsb) + 1, (lsb))

#define REG_FLD_WIDTH(field) ((unsigned int)(((field) >> 16) & 0xFF))

#define REG_FLD_SHIFT(field) ((unsigned int)((field)&0xFF))

#define REG_FLD_MASK(field)                                                    \
	((unsigned int)((1ULL << REG_FLD_WIDTH(field)) - 1)                    \
	 << REG_FLD_SHIFT(field))

#define REG_FLD_VAL(field, val)                                                \
	(((val) << REG_FLD_SHIFT(field)) & REG_FLD_MASK(field))

#define REG_FLD_VAL_GET(field, regval)                                         \
	(((regval)&REG_FLD_MASK(field)) >> REG_FLD_SHIFT(field))

#define DISP_REG_GET_FIELD(field, reg32)                                       \
	REG_FLD_VAL_GET(field, __raw_readl((unsigned long *)(reg32)))

#define SET_VAL_MASK(o_val, o_mask, i_val, i_fld)                              \
	do {                                                                   \
		o_val |= (i_val << REG_FLD_SHIFT(i_fld));                      \
		o_mask |= (REG_FLD_MASK(i_fld));                               \
	} while (0)

#define MAKE_CRTC_COLOR_FMT(rgb, bpp, block, vdo, format, byteswap, rgbswap,   \
			    id)                                                \
	(((rgb) << _MTK_CRTC_COLOR_FMT_RGB_SHIFT) |                            \
	 ((bpp) << _MTK_CRTC_COLOR_FMT_bpp_SHIFT) |                            \
	 ((block) << _MTK_CRTC_COLOR_FMT_BLOCK_SHIT) |                         \
	 ((vdo) << _MTK_CRTC_COLOR_FMT_VDO_SHIFT) |                            \
	 ((format) << _MTK_CRTC_COLOR_FMT_FORMAT_SHIFT) |                      \
	 ((byteswap) << _MTK_CRTC_COLOR_FMT_BYTESWAP_SHIFT) |                  \
	 ((rgbswap) << _MTK_CRTC_COLOR_FMT_RGBSWAP_SHIFT) |                    \
	 ((id) << _MTK_CRTC_COLOR_FMT_ID_SHIFT))

#define MTK_CRTC_COLOR_FMT_GET_RGB(fmt)                                        \
	_MASK_SHIFT(fmt, _MTK_CRTC_COLOR_FMT_RGB_WIDTH,                        \
		    _MTK_CRTC_COLOR_FMT_RGB_SHIFT)
#define MTK_CRTC_COLOR_FMT_GET_bpp(fmt)                                        \
	_MASK_SHIFT(fmt, _MTK_CRTC_COLOR_FMT_bpp_WIDTH,                        \
		    _MTK_CRTC_COLOR_FMT_bpp_SHIFT)
#define MTK_CRTC_COLOR_FMT_GET_BLOCK(fmt)                                      \
	_MASK_SHIFT(fmt, _MTK_CRTC_COLOR_FMT_BLOCK_WIDTH,                      \
		    _MTK_CRTC_COLOR_FMT_BLOCK_SHIT)
#define MTK_CRTC_COLOR_FMT_GET_VDO(fmt)                                        \
	_MASK_SHIFT(fmt, _MTK_CRTC_COLOR_FMT_VDO_WIDTH,                        \
		    _MTK_CRTC_COLOR_FMT_VDO_SHIFT)
#define MTK_CRTC_COLOR_FMT_GET_FORMAT(fmt)                                     \
	_MASK_SHIFT(fmt, _MTK_CRTC_COLOR_FMT_FORMAT_WIDTH,                     \
		    _MTK_CRTC_COLOR_FMT_FORMAT_SHIFT)
#define MTK_CRTC_COLOR_FMT_GET_BYTESWAP(fmt)                                   \
	_MASK_SHIFT(fmt, _MTK_CRTC_COLOR_FMT_BYTESWAP_WIDTH,                   \
		    _MTK_CRTC_COLOR_FMT_BYTESWAP_SHIFT)
#define MTK_CRTC_COLOR_FMT_GET_RGBSWAP(fmt)                                    \
	_MASK_SHIFT(fmt, _MTK_CRTC_COLOR_FMT_RGBSWAP_WIDTH,                    \
		    _MTK_CRTC_COLOR_FMT_RGBSWAP_SHIFT)
#define MTK_CRTC_COLOR_FMT_GET_ID(fmt)                                         \
	_MASK_SHIFT(fmt, _MTK_CRTC_COLOR_FMT_ID_WIDTH,                         \
		    _MTK_CRTC_COLOR_FMT_ID_SHIFT)
#define MTK_CRTC_COLOR_FMT_GET_Bpp(fmt) (MTK_CRTC_COLOR_FMT_GET_bpp(fmt) / 8)

#define MAX_CRTC_DC_FB 3

#define for_each_comp_in_target_ddp_mode_bound(comp, mtk_crtc, __i, __j,       \
					       ddp_mode, offset)               \
	for ((__i) = 0; (__i) < DDP_PATH_NR; (__i)++)                          \
		for ((__j) = 0;                         \
			(offset) <                          \
			(mtk_crtc)->ddp_ctx[ddp_mode].ddp_comp_nr[__i] &&  \
			(__j) <                             \
			(mtk_crtc)->ddp_ctx[ddp_mode].ddp_comp_nr[__i] -  \
			offset &&                           \
			((comp) = (mtk_crtc)->ddp_ctx[ddp_mode].ddp_comp[__i]  \
			[__j],                              \
			1);                                 \
			(__j)++)                            \
			for_each_if(comp)

#define for_each_comp_in_target_ddp_mode(comp, mtk_crtc, __i, __j, ddp_mode)   \
	for_each_comp_in_target_ddp_mode_bound(comp, mtk_crtc, __i, __j,       \
			ddp_mode, 0)

#define for_each_comp_in_crtc_path_bound(comp, mtk_crtc, __i, __j, offset)     \
	for_each_comp_in_target_ddp_mode_bound(comp, mtk_crtc, __i, __j,       \
			mtk_crtc->ddp_mode, offset)

#define for_each_comp_in_cur_crtc_path(comp, mtk_crtc, __i, __j)               \
	for_each_comp_in_crtc_path_bound(comp, mtk_crtc, __i, __j, 0)

#define for_each_comp_in_crtc_target_path(comp, mtk_crtc, __i, ddp_path)       \
	for ((__i) = 0;                           \
		(__i) <                               \
		(mtk_crtc)->ddp_ctx[mtk_crtc->ddp_mode]  \
		.ddp_comp_nr[(ddp_path)] &&           \
		((comp) = (mtk_crtc)                  \
		->ddp_ctx[mtk_crtc->ddp_mode]         \
		.ddp_comp[(ddp_path)][__i],           \
		1);                                   \
		(__i)++)                              \
		for_each_if(comp)

#define for_each_comp_in_crtc_target_mode(comp, mtk_crtc, __i, __j, ddp_mode)  \
	for ((__i) = 0; (__i) < DDP_PATH_NR; (__i)++)                          \
		for ((__j) = 0;                       \
			(__j) <                           \
			(mtk_crtc)->ddp_ctx[ddp_mode].ddp_comp_nr[__i] &&  \
			((comp) = (mtk_crtc)->ddp_ctx[ddp_mode].ddp_comp[__i]  \
			[__j],                            \
			1);                               \
			(__j)++)                          \
			for_each_if(comp)

#define for_each_comp_in_crtc_path_reverse(comp, mtk_crtc, __i, __j)           \
	for ((__i) = DDP_PATH_NR - 1; (__i) >= 0; (__i)--)                     \
		for ((__j) =                          \
			(mtk_crtc)->ddp_ctx[mtk_crtc->ddp_mode]   \
			.ddp_comp_nr[__i] -               \
			1;                                \
			(__j) >= 0 &&                     \
			((comp) = (mtk_crtc)              \
			->ddp_ctx[mtk_crtc->ddp_mode]     \
			.ddp_comp[__i][__j],              \
			1);                               \
			(__j)--)                          \
			for_each_if(comp)

#define for_each_comp_in_all_crtc_mode(comp, mtk_crtc, __i, __j, p_mode)       \
	for ((p_mode) = 0; (p_mode) < DDP_MODE_NR; (p_mode)++)                 \
		for ((__i) = 0; (__i) < DDP_PATH_NR; (__i)++)                  \
			for ((__j) = 0; (__j) <           \
				(mtk_crtc)->ddp_ctx[p_mode]   \
				.ddp_comp_nr[__i] &&          \
				((comp) = (mtk_crtc)          \
				->ddp_ctx[p_mode]             \
				.ddp_comp[__i][__j],          \
				1);                           \
				(__j)++)                      \
				for_each_if(comp)

#define for_each_comp_id_in_path_data(comp_id, path_data, __i, __j, p_mode)    \
	for ((p_mode) = 0; (p_mode) < DDP_MODE_NR; (p_mode)++)        \
		for ((__i) = 0; (__i) < DDP_PATH_NR; (__i)++)             \
			for ((__j) = 0;                   \
				(__j) <                       \
				(path_data)->path_len[p_mode][__i] &&  \
				((comp_id) = (path_data)      \
				->path[p_mode][__i][__j],     \
				1);                           \
				(__j)++)

#define for_each_comp_id_in_dual_pipe(comp_id, path_data, __i, __j)    \
	for ((__i) = 0; (__i) < DDP_SECOND_PATH; (__i)++) \
		for ((__j) = 0;				  \
			(__j) <					  \
			(path_data)->dual_path_len[__i] &&  \
			((comp_id) = (path_data)	  \
			->dual_path[__i][__j],	  \
			1);						  \
			(__j)++)

#define for_each_comp_in_dual_pipe(comp, mtk_crtc, __i, __j)       \
	for ((__i) = 0; (__i) < DDP_SECOND_PATH; (__i)++)		   \
		for ((__j) = 0; (__j) <		  \
			(mtk_crtc)->dual_pipe_ddp_ctx   \
			.ddp_comp_nr[__i] &&		  \
			((comp) = (mtk_crtc)		  \
			->dual_pipe_ddp_ctx			  \
			.ddp_comp[__i][__j],		  \
			1);						  \
			(__j)++)					  \
			for_each_if(comp)

#define for_each_wb_comp_id_in_path_data(comp_id, path_data, __i, p_mode)      \
	for ((p_mode) = 0; (p_mode) < DDP_MODE_NR; (p_mode)++)        \
		for ((__i) = 0;                       \
			(__i) < (path_data)->wb_path_len[p_mode] &&  \
			((comp_id) = (path_data)          \
			->wb_path[p_mode][__i], 1);       \
			(__i)++)

enum MTK_CRTC_PROP {
	CRTC_PROP_OVERLAP_LAYER_NUM,
	CRTC_PROP_LYE_IDX,
	CRTC_PROP_PRES_FENCE_IDX,
	CRTC_PROP_SF_PRES_FENCE_IDX,
	CRTC_PROP_DOZE_ACTIVE,
	CRTC_PROP_OUTPUT_ENABLE,
	CRTC_PROP_OUTPUT_FENCE_IDX,
	CRTC_PROP_OUTPUT_X,
	CRTC_PROP_OUTPUT_Y,
	CRTC_PROP_OUTPUT_WIDTH,
	CRTC_PROP_OUTPUT_HEIGHT,
	CRTC_PROP_OUTPUT_FB_ID,
	CRTC_PROP_INTF_FENCE_IDX,
	CRTC_PROP_DISP_MODE_IDX,
	CRTC_PROP_HBM_ENABLE,
	CRTC_PROP_COLOR_TRANSFORM,
	CRTC_PROP_USER_SCEN,
	CRTC_PROP_HDR_ENABLE,
	CRTC_PROP_OVL_DSI_SEQ,
	CRTC_PROP_MAX,
};

#define USER_SCEN_BLANK (BIT(0))

enum MTK_CRTC_COLOR_FMT {
	CRTC_COLOR_FMT_UNKNOWN = 0,
	CRTC_COLOR_FMT_Y8 = MAKE_CRTC_COLOR_FMT(0, 8, 0, 0, 7, 0, 0, 1),
	CRTC_COLOR_FMT_RGBA4444 = MAKE_CRTC_COLOR_FMT(1, 16, 0, 0, 4, 0, 0, 2),
	CRTC_COLOR_FMT_RGBA5551 = MAKE_CRTC_COLOR_FMT(1, 16, 0, 0, 0, 0, 0, 3),
	CRTC_COLOR_FMT_RGB565 = MAKE_CRTC_COLOR_FMT(1, 16, 0, 0, 0, 0, 0, 4),
	CRTC_COLOR_FMT_BGR565 = MAKE_CRTC_COLOR_FMT(1, 16, 0, 0, 0, 1, 0, 5),
	CRTC_COLOR_FMT_RGB888 = MAKE_CRTC_COLOR_FMT(1, 24, 0, 0, 1, 1, 0, 6),
	CRTC_COLOR_FMT_BGR888 = MAKE_CRTC_COLOR_FMT(1, 24, 0, 0, 1, 0, 0, 7),
	CRTC_COLOR_FMT_RGBA8888 = MAKE_CRTC_COLOR_FMT(1, 32, 0, 0, 2, 1, 0, 8),
	CRTC_COLOR_FMT_BGRA8888 = MAKE_CRTC_COLOR_FMT(1, 32, 0, 0, 2, 0, 0, 9),
	CRTC_COLOR_FMT_ARGB8888 = MAKE_CRTC_COLOR_FMT(1, 32, 0, 0, 3, 1, 0, 10),
	CRTC_COLOR_FMT_ABGR8888 = MAKE_CRTC_COLOR_FMT(1, 32, 0, 0, 3, 0, 0, 11),
	CRTC_COLOR_FMT_RGBX8888 = MAKE_CRTC_COLOR_FMT(1, 32, 0, 0, 0, 0, 0, 12),
	CRTC_COLOR_FMT_BGRX8888 = MAKE_CRTC_COLOR_FMT(1, 32, 0, 0, 0, 0, 0, 13),
	CRTC_COLOR_FMT_XRGB8888 = MAKE_CRTC_COLOR_FMT(1, 32, 0, 0, 0, 0, 0, 14),
	CRTC_COLOR_FMT_XBGR8888 = MAKE_CRTC_COLOR_FMT(1, 32, 0, 0, 0, 0, 0, 15),
	CRTC_COLOR_FMT_AYUV = MAKE_CRTC_COLOR_FMT(0, 0, 0, 0, 0, 0, 0, 16),
	CRTC_COLOR_FMT_YUV = MAKE_CRTC_COLOR_FMT(0, 0, 0, 0, 0, 0, 0, 17),
	CRTC_COLOR_FMT_UYVY = MAKE_CRTC_COLOR_FMT(0, 16, 0, 0, 4, 0, 0, 18),
	CRTC_COLOR_FMT_VYUY = MAKE_CRTC_COLOR_FMT(0, 16, 0, 0, 4, 1, 0, 19),
	CRTC_COLOR_FMT_YUYV = MAKE_CRTC_COLOR_FMT(0, 16, 0, 0, 5, 0, 0, 20),
	CRTC_COLOR_FMT_YVYU = MAKE_CRTC_COLOR_FMT(0, 16, 0, 0, 5, 1, 0, 21),
	CRTC_COLOR_FMT_UYVY_BLK = MAKE_CRTC_COLOR_FMT(0, 16, 1, 0, 4, 0, 0, 22),
	CRTC_COLOR_FMT_VYUY_BLK = MAKE_CRTC_COLOR_FMT(0, 16, 1, 0, 4, 1, 0, 23),
	CRTC_COLOR_FMT_YUY2_BLK = MAKE_CRTC_COLOR_FMT(0, 16, 1, 0, 5, 0, 0, 24),
	CRTC_COLOR_FMT_YVYU_BLK = MAKE_CRTC_COLOR_FMT(0, 16, 1, 0, 5, 1, 0, 25),
	CRTC_COLOR_FMT_YV12 = MAKE_CRTC_COLOR_FMT(0, 8, 0, 0, 8, 1, 0, 26),
	CRTC_COLOR_FMT_I420 = MAKE_CRTC_COLOR_FMT(0, 8, 0, 0, 8, 0, 0, 27),
	CRTC_COLOR_FMT_YV16 = MAKE_CRTC_COLOR_FMT(0, 8, 0, 0, 9, 1, 0, 28),
	CRTC_COLOR_FMT_I422 = MAKE_CRTC_COLOR_FMT(0, 8, 0, 0, 9, 0, 0, 29),
	CRTC_COLOR_FMT_YV24 = MAKE_CRTC_COLOR_FMT(0, 8, 0, 0, 10, 1, 0, 30),
	CRTC_COLOR_FMT_I444 = MAKE_CRTC_COLOR_FMT(0, 8, 0, 0, 10, 0, 0, 31),
	CRTC_COLOR_FMT_NV12 = MAKE_CRTC_COLOR_FMT(0, 8, 0, 0, 12, 0, 0, 32),
	CRTC_COLOR_FMT_NV21 = MAKE_CRTC_COLOR_FMT(0, 8, 0, 0, 12, 1, 0, 33),
	CRTC_COLOR_FMT_NV12_BLK = MAKE_CRTC_COLOR_FMT(0, 8, 1, 0, 12, 0, 0, 34),
	CRTC_COLOR_FMT_NV21_BLK = MAKE_CRTC_COLOR_FMT(0, 8, 1, 0, 12, 1, 0, 35),
	CRTC_COLOR_FMT_NV12_BLK_FLD =
		MAKE_CRTC_COLOR_FMT(0, 8, 1, 1, 12, 0, 0, 36),
	CRTC_COLOR_FMT_NV21_BLK_FLD =
		MAKE_CRTC_COLOR_FMT(0, 8, 1, 1, 12, 1, 0, 37),
	CRTC_COLOR_FMT_NV16 = MAKE_CRTC_COLOR_FMT(0, 8, 0, 0, 13, 0, 0, 38),
	CRTC_COLOR_FMT_NV61 = MAKE_CRTC_COLOR_FMT(0, 8, 0, 0, 13, 1, 0, 39),
	CRTC_COLOR_FMT_NV24 = MAKE_CRTC_COLOR_FMT(0, 8, 0, 0, 14, 0, 0, 40),
	CRTC_COLOR_FMT_NV42 = MAKE_CRTC_COLOR_FMT(0, 8, 0, 0, 14, 1, 0, 41),
	CRTC_COLOR_FMT_PARGB8888 =
		MAKE_CRTC_COLOR_FMT(1, 32, 0, 0, 3, 1, 0, 42),
	CRTC_COLOR_FMT_PABGR8888 =
		MAKE_CRTC_COLOR_FMT(1, 32, 0, 0, 3, 1, 1, 43),
	CRTC_COLOR_FMT_PRGBA8888 =
		MAKE_CRTC_COLOR_FMT(1, 32, 0, 0, 3, 0, 1, 44),
	CRTC_COLOR_FMT_PBGRA8888 =
		MAKE_CRTC_COLOR_FMT(1, 32, 0, 0, 3, 0, 0, 45),
};

/* CLIENT_SODI_LOOP for sw workaround to fix gce hw bug */
#define DECLARE_GCE_CLIENT(EXPR)                                               \
	EXPR(CLIENT_CFG)                                                       \
	EXPR(CLIENT_TRIG_LOOP)                                                 \
	EXPR(CLIENT_SODI_LOOP)                                                 \
	EXPR(CLIENT_SUB_CFG)                                                   \
	EXPR(CLIENT_DSI_CFG)                                                   \
	EXPR(CLIENT_SEC_CFG)                                                   \
	EXPR(CLIENT_TYPE_MAX)

enum CRTC_GCE_CLIENT_TYPE { DECLARE_GCE_CLIENT(DECLARE_NUM) };

enum CRTC_GCE_EVENT_TYPE {
	EVENT_CMD_EOF,
	EVENT_VDO_EOF,
	EVENT_STREAM_EOF,
	EVENT_STREAM_DIRTY,
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833)
	EVENT_SYNC_TOKEN_SODI,
#endif
	EVENT_TE,
	EVENT_ESD_EOF,
	EVENT_RDMA0_EOF,
	EVENT_WDMA0_EOF,
	EVENT_WDMA1_EOF,
	EVENT_STREAM_BLOCK,
	EVENT_CABC_EOF,
	EVENT_DSI0_SOF,
	EVENT_TYPE_MAX,
};

enum CRTC_DDP_MODE {
	DDP_MAJOR,
	DDP_MINOR,
	DDP_MODE_NR,
	DDP_NO_USE,
};

enum CRTC_DDP_PATH {
	DDP_FIRST_PATH,
	DDP_SECOND_PATH,
	DDP_PATH_NR,
};

/**
 * enum CWB_BUFFER_TYPE - user want to use buffer type
 * @IMAGE_ONLY: u8 *image
 * @CARRY_METADATA: struct user_cwb_buffer
 */
enum CWB_BUFFER_TYPE {
	IMAGE_ONLY,
	CARRY_METADATA,
	BUFFER_TYPE_NR,
};

struct mtk_crtc_path_data {
	const enum mtk_ddp_comp_id *path[DDP_MODE_NR][DDP_PATH_NR];
	unsigned int path_len[DDP_MODE_NR][DDP_PATH_NR];
	bool path_req_hrt[DDP_MODE_NR][DDP_PATH_NR];
	const enum mtk_ddp_comp_id *wb_path[DDP_MODE_NR];
	unsigned int wb_path_len[DDP_MODE_NR];
	const struct mtk_addon_scenario_data *addon_data;
	//for dual path
	const enum mtk_ddp_comp_id *dual_path[DDP_PATH_NR];
	unsigned int dual_path_len[DDP_PATH_NR];
	const struct mtk_addon_scenario_data *addon_data_dual;
};

struct mtk_crtc_gce_obj {
	struct cmdq_client *client[CLIENT_TYPE_MAX];
	struct cmdq_pkt_buffer buf;
	struct cmdq_base *base;
	int event[EVENT_TYPE_MAX];
};

/**
 * struct mtk_crtc_ddp_ctx - MediaTek specific ddp structure for crtc path
 * control.
 * @mutex: handle to one of the ten disp_mutex streams
 * @ddp_comp_nr: number of components in ddp_comp
 * @ddp_comp: array of pointers the mtk_ddp_comp structures used by this crtc
 * @wb_comp_nr: number of components in 1to2 path
 * @wb_comp: array of pointers the mtk_ddp_comp structures used for 1to2 path
 * @wb_fb: temp wdma output buffer in 1to2 path
 * @dc_fb: frame buffer for decouple mode
 * @dc_fb_idx: the index of latest used fb
 */
struct mtk_crtc_ddp_ctx {
	struct mtk_disp_mutex *mutex;
	unsigned int ddp_comp_nr[DDP_PATH_NR];
	struct mtk_ddp_comp **ddp_comp[DDP_PATH_NR];
	bool req_hrt[DDP_PATH_NR];
	unsigned int wb_comp_nr;
	struct mtk_ddp_comp **wb_comp;
	struct drm_framebuffer *wb_fb;
	struct drm_framebuffer *dc_fb;
	unsigned int dc_fb_idx;
};

struct mtk_drm_fake_vsync {
	struct task_struct *fvsync_task;
	wait_queue_head_t fvsync_wq;
	atomic_t fvsync_active;
};

struct mtk_drm_fake_layer {
	unsigned int fake_layer_mask;
	struct drm_framebuffer *fake_layer_buf[PRIMARY_OVL_PHY_LAYER_NR];
	bool init;
	bool first_dis;
};


struct disp_ccorr_config {
	int mode;
	int color_matrix[16];
	bool featureFlag;
};

struct user_cwb_image {
	u8 *image;
	int width, height;
};

struct user_cwb_metadata {
	unsigned long long timestamp;
	unsigned int frameIndex;
};

struct user_cwb_buffer {
	struct user_cwb_image data;
	struct user_cwb_metadata meta;
};

struct mtk_cwb_buffer_info {
	struct mtk_rect dst_roi;
	u32 addr_mva;
	u64 addr_va;
	struct drm_framebuffer *fb;
	unsigned long long timestamp;
};

struct mtk_cwb_funcs {
	/**
	 * @get_buffer:
	 *
	 * This function is optional.
	 *
	 * If user hooks this callback, driver will use this first when
	 * wdma irq is arrived. (capture done)
	 * User need fill buffer address to *buffer.
	 *
	 * If user not hooks this callback driver will confirm whether
	 * mtk_wdma_capture_info->user_buffer is NULL or not.
	 * User can use setUserBuffer() assigned this param.
	 */
	void (*get_buffer)(void **buffer);

	/**
	 * @copy_done:
	 *
	 * When Buffer copy done will be use this callback to notify user.
	 */
	void (*copy_done)(void *buffer, enum CWB_BUFFER_TYPE type);
};

struct mtk_cwb_info {
	unsigned int enable;

	struct mtk_rect src_roi;
	unsigned int count;
	bool is_sec;

	unsigned int buf_idx;
	struct mtk_cwb_buffer_info buffer[2];
	unsigned int copy_w;
	unsigned int copy_h;

	enum addon_scenario scn;
	struct mtk_ddp_comp *comp;

	void *user_buffer;
	enum CWB_BUFFER_TYPE type;
	const struct mtk_cwb_funcs *funcs;
};

/**
 * struct mtk_drm_crtc - MediaTek specific crtc structure.
 * @base: crtc object.
 * @enabled: records whether crtc_enable succeeded
 * @bpc: Maximum bits per color channel.
 * @lock: Mutex lock for critical section in crtc
 * @gce_obj: the elements for controlling GCE engine.
 * @planes: array of 4 drm_plane structures, one for each overlay plane
 * @pending_planes: whether any plane has pending changes to be applied
 * @config_regs: memory mapped mmsys configuration register space
 * @ddp_ctx: contain path components and mutex
 * @mutex: handle to one of the ten disp_mutex streams
 * @ddp_mode: the currently selected ddp path
 * @panel_ext: contain extended panel extended information and callback function
 * @esd_ctx: ESD check task context
 * @qos_ctx: BW Qos task context
 */
struct mtk_drm_crtc {
	struct drm_crtc base;
	bool enabled;
	unsigned int bpc;
	bool pending_needs_vblank;
	struct mutex lock;
	struct drm_pending_vblank_event *event;
	struct mtk_crtc_gce_obj gce_obj;
	struct cmdq_pkt *trig_loop_cmdq_handle;
#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833)
	struct cmdq_pkt *sodi_loop_cmdq_handle;
#endif
	struct mtk_drm_plane *planes;
	unsigned int layer_nr;
	bool pending_planes;

	void __iomem *config_regs;
	resource_size_t config_regs_pa;
	const struct mtk_mmsys_reg_data *mmsys_reg_data;
	struct mtk_crtc_ddp_ctx ddp_ctx[DDP_MODE_NR];
	struct mtk_disp_mutex *mutex[DDP_PATH_NR];
	unsigned int ddp_mode;
	unsigned int cur_config_fence[OVL_LAYER_NR];

	struct drm_writeback_connector wb_connector;
	bool wb_enable;
	bool wb_hw_enable;

	const struct mtk_crtc_path_data *path_data;
	struct mtk_crtc_ddp_ctx dual_pipe_ddp_ctx;
	bool is_dual_pipe;

	struct mtk_drm_idlemgr *idlemgr;
	wait_queue_head_t crtc_status_wq;
	struct mtk_panel_ext *panel_ext;
	struct mtk_drm_esd_ctx *esd_ctx;
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	struct mtk_drm_gem_obj *round_corner_gem;
	struct mtk_drm_gem_obj *round_corner_gem_l;
	struct mtk_drm_gem_obj *round_corner_gem_r;
#endif
	struct mtk_drm_qos_ctx *qos_ctx;
	bool sec_on;
	struct task_struct *vblank_enable_task;
	wait_queue_head_t vblank_enable_wq;
	atomic_t vblank_enable_task_active;

	char *wk_lock_name;
	struct wakeup_source *wk_lock;

	struct mtk_drm_fake_vsync *fake_vsync;
	struct mtk_drm_fake_layer fake_layer;

	/* DC mode - RDMA config thread*/
	struct task_struct *dc_main_path_commit_task;
	wait_queue_head_t dc_main_path_commit_wq;
	atomic_t dc_main_path_commit_event;
	struct task_struct *trigger_event_task;
	struct task_struct *trigger_delay_task;
	struct task_struct *trig_cmdq_task;
	atomic_t trig_event_act;
	atomic_t trig_delay_act;
	atomic_t delayed_trig;
	atomic_t cmdq_trig;
	wait_queue_head_t trigger_delay;
	wait_queue_head_t trigger_event;
	wait_queue_head_t trigger_cmdq;

	unsigned int avail_modes_num;
	struct drm_display_mode *avail_modes;
	struct timeval vblank_time;
	unsigned int max_fps;

	bool mipi_hopping_sta;
	bool panel_osc_hopping_sta;
	bool vblank_en;

	atomic_t already_config;

	bool layer_rec_en;
	unsigned int fps_change_index;

	wait_queue_head_t state_wait_queue;
	bool crtc_blank;
	struct mutex blank_lock;

	wait_queue_head_t present_fence_wq;
	struct task_struct *pf_release_thread;
	atomic_t pf_event;

	wait_queue_head_t sf_present_fence_wq;
	struct task_struct *sf_pf_release_thread;
	atomic_t sf_pf_event;

	/*capture write back ctx*/
	struct mutex cwb_lock;
	struct mtk_cwb_info *cwb_info;
	struct task_struct *cwb_task;
	wait_queue_head_t cwb_wq;
	atomic_t cwb_task_active;

	ktime_t eof_time;
	struct task_struct *signal_present_fece_task;
	struct cmdq_cb_data cb_data;
	atomic_t cmdq_done;
	wait_queue_head_t signal_fence_task_wq;
};

struct mtk_crtc_state {
	struct drm_crtc_state base;
	struct cmdq_pkt *cmdq_handle;

	bool pending_config;
	unsigned int pending_width;
	unsigned int pending_height;
	unsigned int pending_vrefresh;

	struct mtk_lye_ddp_state lye_state;
	struct mtk_rect rsz_src_roi;
	struct mtk_rect rsz_dst_roi;
	struct mtk_rsz_param rsz_param[2];
	atomic_t plane_enabled_num;

	/* property */
	unsigned int prop_val[CRTC_PROP_MAX];
	bool doze_changed;
};

struct mtk_cmdq_cb_data {
	struct drm_crtc_state		*state;
	struct cmdq_pkt			*cmdq_handle;
	struct drm_crtc			*crtc;
	unsigned int misc;
};

extern unsigned int te_cnt;

int mtk_drm_crtc_enable_vblank(struct drm_device *drm, unsigned int pipe);
void mtk_drm_crtc_disable_vblank(struct drm_device *drm, unsigned int pipe);
bool mtk_crtc_get_vblank_timestamp(struct drm_device *dev, unsigned int pipe,
				 int *max_error,
				 ktime_t *vblank_time,
				 bool in_vblank_irq);
void mtk_drm_crtc_commit(struct drm_crtc *crtc);
void mtk_crtc_ddp_irq(struct drm_crtc *crtc, struct mtk_ddp_comp *comp);
void mtk_crtc_vblank_irq(struct drm_crtc *crtc);
int mtk_drm_crtc_create(struct drm_device *drm_dev,
			const struct mtk_crtc_path_data *path_data);
void mtk_drm_crtc_plane_update(struct drm_crtc *crtc, struct drm_plane *plane,
			       struct mtk_plane_state *state);

void mtk_drm_crtc_dump(struct drm_crtc *crtc);
void mtk_drm_crtc_analysis(struct drm_crtc *crtc);
bool mtk_crtc_is_frame_trigger_mode(struct drm_crtc *crtc);
void mtk_crtc_wait_frame_done(struct mtk_drm_crtc *mtk_crtc,
			      struct cmdq_pkt *cmdq_handle,
			      enum CRTC_DDP_PATH ddp_path,
			      int clear_event);

struct mtk_ddp_comp *mtk_ddp_comp_request_output(struct mtk_drm_crtc *mtk_crtc);

/* get fence */
int mtk_drm_crtc_getfence_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv);
int mtk_drm_crtc_get_sf_fence_ioctl(struct drm_device *dev, void *data,
				    struct drm_file *file_priv);

long mtk_crtc_wait_status(struct drm_crtc *crtc, bool status, long timeout);
void mtk_crtc_cwb_path_disconnect(struct drm_crtc *crtc);
int mtk_crtc_path_switch(struct drm_crtc *crtc, unsigned int path_sel,
			 int need_lock);
void mtk_need_vds_path_switch(struct drm_crtc *crtc);

void mtk_drm_crtc_first_enable(struct drm_crtc *crtc);
void mtk_drm_crtc_enable(struct drm_crtc *crtc);
void mtk_drm_crtc_disable(struct drm_crtc *crtc, bool need_wait);
bool mtk_crtc_with_sub_path(struct drm_crtc *crtc, unsigned int ddp_mode);

void mtk_crtc_ddp_prepare(struct mtk_drm_crtc *mtk_crtc);
void mtk_crtc_ddp_unprepare(struct mtk_drm_crtc *mtk_crtc);
void mtk_crtc_stop(struct mtk_drm_crtc *mtk_crtc, bool need_wait);
void mtk_crtc_connect_default_path(struct mtk_drm_crtc *mtk_crtc);
void mtk_crtc_disconnect_default_path(struct mtk_drm_crtc *mtk_crtc);
void mtk_crtc_config_default_path(struct mtk_drm_crtc *mtk_crtc);
void mtk_crtc_restore_plane_setting(struct mtk_drm_crtc *mtk_crtc);
bool mtk_crtc_set_status(struct drm_crtc *crtc, bool status);
int mtk_crtc_attach_addon_path_comp(struct drm_crtc *crtc,
	const struct mtk_addon_module_data *module_data, bool is_attach);
void mtk_crtc_connect_addon_module(struct drm_crtc *crtc);
void mtk_crtc_disconnect_addon_module(struct drm_crtc *crtc);
int mtk_crtc_gce_flush(struct drm_crtc *crtc, void *gce_cb, void *cb_data,
			struct cmdq_pkt *cmdq_handle);
struct cmdq_pkt *mtk_crtc_gce_commit_begin(struct drm_crtc *crtc);
void mtk_crtc_pkt_create(struct cmdq_pkt **cmdq_handle,
	struct drm_crtc *crtc, struct cmdq_client *cl);
int mtk_crtc_get_mutex_id(struct drm_crtc *crtc, unsigned int ddp_mode,
			  enum mtk_ddp_comp_id find_comp);
void mtk_crtc_disconnect_path_between_component(struct drm_crtc *crtc,
						unsigned int ddp_mode,
						enum mtk_ddp_comp_id prev,
						enum mtk_ddp_comp_id next,
						struct cmdq_pkt *cmdq_handle);
void mtk_crtc_connect_path_between_component(struct drm_crtc *crtc,
					     unsigned int ddp_mode,
					     enum mtk_ddp_comp_id prev,
					     enum mtk_ddp_comp_id next,
					     struct cmdq_pkt *cmdq_handle);
int mtk_crtc_find_comp(struct drm_crtc *crtc, unsigned int ddp_mode,
		       enum mtk_ddp_comp_id comp_id);
int mtk_crtc_find_next_comp(struct drm_crtc *crtc, unsigned int ddp_mode,
			    enum mtk_ddp_comp_id comp_id);
int mtk_crtc_find_prev_comp(struct drm_crtc *crtc, unsigned int ddp_mode,
		enum mtk_ddp_comp_id comp_id);
void mtk_drm_fake_vsync_switch(struct drm_crtc *crtc, bool enable);
void mtk_crtc_check_trigger(struct mtk_drm_crtc *mtk_crtc, bool delay,
		bool need_lock);

bool mtk_crtc_is_dc_mode(struct drm_crtc *crtc);
void mtk_crtc_clear_wait_event(struct drm_crtc *crtc);
void mtk_crtc_hw_block_ready(struct drm_crtc *crtc);
int mtk_crtc_lcm_ATA(struct drm_crtc *crtc);
int mtk_crtc_mipi_freq_switch(struct drm_crtc *crtc, unsigned int en,
			unsigned int userdata);
int mtk_crtc_osc_freq_switch(struct drm_crtc *crtc, unsigned int en,
			unsigned int userdata);
int mtk_crtc_enter_tui(struct drm_crtc *crtc);
int mtk_crtc_exit_tui(struct drm_crtc *crtc);


struct drm_display_mode *mtk_drm_crtc_avail_disp_mode(struct drm_crtc *crtc,
	unsigned int idx);
unsigned int mtk_drm_primary_frame_bw(struct drm_crtc *crtc);

unsigned int mtk_drm_primary_display_get_debug_state(
	struct mtk_drm_private *priv, char *stringbuf, int buf_len);

bool mtk_crtc_with_trigger_loop(struct drm_crtc *crtc);
void mtk_crtc_stop_trig_loop(struct drm_crtc *crtc);
void mtk_crtc_start_trig_loop(struct drm_crtc *crtc);

#if defined(CONFIG_MACH_MT6873) || defined(CONFIG_MACH_MT6853) \
	|| defined(CONFIG_MACH_MT6833)
bool mtk_crtc_with_sodi_loop(struct drm_crtc *crtc);
void mtk_crtc_stop_sodi_loop(struct drm_crtc *crtc);
void mtk_crtc_start_sodi_loop(struct drm_crtc *crtc);
#endif

void mtk_crtc_change_output_mode(struct drm_crtc *crtc, int aod_en);
int mtk_crtc_user_cmd(struct drm_crtc *crtc, struct mtk_ddp_comp *comp,
		unsigned int cmd, void *params);
unsigned int mtk_drm_dump_wk_lock(struct mtk_drm_private *priv,
	char *stringbuf, int buf_len);
char *mtk_crtc_index_spy(int crtc_index);
bool mtk_drm_get_hdr_property(void);
int mtk_drm_aod_setbacklight(struct drm_crtc *crtc, unsigned int level);

int mtk_drm_crtc_wait_blank(struct mtk_drm_crtc *mtk_crtc);
void mtk_drm_crtc_init_para(struct drm_crtc *crtc);
void mtk_drm_layer_dispatch_to_dual_pipe(
	struct mtk_plane_state *plane_state,
	struct mtk_plane_state *plane_state_l,
	struct mtk_plane_state *plane_state_r,
	unsigned int w);
void mtk_crtc_dual_layer_config(struct mtk_drm_crtc *mtk_crtc,
		struct mtk_ddp_comp *comp, unsigned int idx,
		struct mtk_plane_state *plane_state, struct cmdq_pkt *cmdq_handle);
unsigned int dual_pipe_comp_mapping(unsigned int comp_id);
int mtk_drm_crtc_set_panel_hbm(struct drm_crtc *crtc, bool en);
int mtk_drm_crtc_hbm_wait(struct drm_crtc *crtc, bool en);
/* ********************* Legacy DISP API *************************** */
unsigned int DISP_GetScreenWidth(void);
unsigned int DISP_GetScreenHeight(void);

void mtk_crtc_disable_secure_state(struct drm_crtc *crtc);
int mtk_crtc_check_out_sec(struct drm_crtc *crtc);
struct golden_setting_context *
	__get_golden_setting_context(struct mtk_drm_crtc *mtk_crtc);
/***********************  PanelMaster  ********************************/
void mtk_crtc_start_for_pm(struct drm_crtc *crtc);
void mtk_crtc_stop_for_pm(struct mtk_drm_crtc *mtk_crtc, bool need_wait);
bool mtk_crtc_frame_buffer_existed(void);
int m4u_sec_init(void);

#endif /* MTK_DRM_CRTC_H */
