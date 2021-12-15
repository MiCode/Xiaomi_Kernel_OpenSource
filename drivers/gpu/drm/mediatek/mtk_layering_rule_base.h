/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_LAYERING_RULE_BASE__
#define __MTK_LAYERING_RULE_BASE__

#if 0
#include "disp_session.h"
#include "disp_lcm.h"
#include "disp_drv_log.h"
#include "primary_display.h"
#include "disp_drv_platform.h"
#include "display_recorder.h"
#endif
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem.h>
#include <drm/mediatek_drm.h>
#include <drm/drm_modes.h>

/* move to Platform dependent part? */
#define TOTAL_OVL_LAYER_NUM (4 + 3 + 2 + 3)

#define PRIMARY_SESSION_INPUT_LAYER_COUNT (12) /* phy(4+2) + ext(3+3) */
#define EXTERNAL_SESSION_INPUT_LAYER_COUNT                                     \
	(2 /*2+3*/) /* 2 is enough, no need ext layer */
/* ***************************************** */

#define PRIMARY_OVL_LAYER_NUM PRIMARY_SESSION_INPUT_LAYER_COUNT
#define SECONDARY_OVL_LAYER_NUM EXTERNAL_SESSION_INPUT_LAYER_COUNT

/* #define HRT_DEBUG_LEVEL1 */
/* #define HRT_DEBUG_LEVEL2 */
/* #define HRT_UT_DEBUG */

#define PATH_FMT_RSZ_SHIFT 9
#define PATH_FMT_PIPE_SHIFT 7
#define PATH_FMT_DISP_SHIFT 5
#define PATH_FMT_ID_SHIFT 0

#define HRT_UINT_BOUND_BPP 4
#define HRT_UINT_WEIGHT 100
/* bpp x uint weight = 2 x 100 */
#define HRT_AEE_WEIGHT 200
#define HRT_ROUND_CORNER_WEIGHT 200

#define HRT_GET_FIRST_SET_BIT(n) (((n) - ((n) & ((n) - 1))))

enum HRT_DISP_TYPE {
	HRT_PRIMARY = 0,
	HRT_SECONDARY,
	HRT_THIRD,
	HRT_TYPE_NUM,
};

enum HRT_DEBUG_LAYER_DATA {
	HRT_LAYER_DATA_ID = 0,
	HRT_LAYER_DATA_SRC_FMT,
	HRT_LAYER_DATA_DST_OFFSET_X,
	HRT_LAYER_DATA_DST_OFFSET_Y,
	HRT_LAYER_DATA_DST_WIDTH,
	/*5*/
	HRT_LAYER_DATA_DST_HEIGHT,
	HRT_LAYER_DATA_SRC_WIDTH,
	HRT_LAYER_DATA_SRC_HEIGHT,
	HRT_LAYER_DATA_SRC_OFFSET_X,
	HRT_LAYER_DATA_SRC_OFFSET_Y,
	HRT_LAYER_DATA_COMPRESS,
	HRT_LAYER_DATA_CAPS,
	HRT_LAYER_DATA_NUM,
};

enum HRT_TYPE {
	HRT_TYPE_LARB0 = 0,
	HRT_TYPE_LARB1,
	HRT_TYPE_EMI,
	HRT_TYPE_UNKNOWN,
};

enum HRT_SYS_STATE {
	DISP_HRT_MJC_ON = 0,
	DISP_HRT_FORCE_DUAL_OFF,
	DISP_HRT_MULTI_TUI_ON,
};

enum DISP_HW_MAPPING_TB_TYPE {
	DISP_HW_EMI_BOUND_TB,
	DISP_HW_LARB_BOUND_TB,
	DISP_HW_OVL_TB,
	DISP_HW_LARB_TB,
	DISP_HW_LAYER_TB,
};

enum LYE_HELPER_OPT {
	LYE_OPT_DUAL_PIPE,
	LYE_OPT_EXT_LAYER,
	LYE_OPT_RPO,
	LYE_OPT_CLEAR_LAYER,
	LYE_OPT_NUM
};

enum ADJUST_LAYOUT_PURPOSE {
	ADJUST_LAYOUT_EXT_GROUPING,
	ADJUST_LAYOUT_OVERLAP_CAL,
};

enum LYE_TYPE {
	LYE_NORMAL,
	LYE_EXT0,
	LYE_EXT1,
	LYE_EXT2,
};

struct hrt_sort_entry {
	struct hrt_sort_entry *head, *tail;
	struct drm_mtk_layer_config *layer_info;
	int key;
	int overlap_w;
};

struct layering_rule_info_t {
	int bound_tb_idx;
	int addon_scn[HRT_TYPE_NUM];
	int dal_enable;
	int primary_fps;
	int hrt_sys_state;
	int wrot_sram;
	unsigned int hrt_idx;
};

enum SCN_FACTOR {
	SCN_NEED_VP_PQ = 0x00000001,
	SCN_NEED_GAME_PQ = 0x00000002,
	SCN_TRIPLE_DISP =  0x00000004,
};

struct layering_rule_ops {
	void (*scenario_decision)(unsigned int scn_decision_flag,
				  unsigned int scale_num);
	int *(*get_bound_table)(enum DISP_HW_MAPPING_TB_TYPE tb_type);
	uint16_t (*get_mapping_table)(struct drm_device *dev, int disp_idx,
				      enum DISP_HW_MAPPING_TB_TYPE tb_type,
				      int param);
	/* should be removed */
	int (*get_hrt_bound)(int is_larb, int hrt_level);

	void (*copy_hrt_bound_table)(struct drm_mtk_layering_info *disp_info,
		int is_larb, int *hrt_table, struct drm_device *dev);
	bool (*rollback_to_gpu_by_hw_limitation)(
		struct drm_device *dev,
		struct drm_mtk_layering_info *disp_info);
	bool (*rollback_all_to_GPU_for_idle)(struct drm_device *dev);
	/* for fbdc */
	void (*fbdc_pre_calculate)(struct drm_mtk_layering_info *disp_info);
	void (*fbdc_adjust_layout)(struct drm_mtk_layering_info *disp_info,
				   enum ADJUST_LAYOUT_PURPOSE p);
	void (*fbdc_restore_layout)(struct drm_mtk_layering_info *dst_info,
				    enum ADJUST_LAYOUT_PURPOSE p);
	void (*fbdc_rule)(struct drm_mtk_layering_info *disp_info);
};

#define HRT_GET_DVFS_LEVEL(hrt_num) (hrt_num & 0xF)
#define HRT_SET_DVFS_LEVEL(hrt_num, value)                                     \
	(hrt_num = ((hrt_num & ~(0xF)) | (value & 0xF)))
#define HRT_GET_SCALE_SCENARIO(hrt_num) ((hrt_num & 0xF0) >> 4)
#define HRT_SET_SCALE_SCENARIO(hrt_num, value)                                 \
	(hrt_num = ((hrt_num & ~(0xF0)) | ((value & 0xF) << 4)))
#define HRT_GET_AEE_FLAG(hrt_num) ((hrt_num & 0x100) >> 8)
#define HRT_SET_AEE_FLAG(hrt_num, value)                                       \
	(hrt_num = ((hrt_num & ~(0x100)) | ((value & 0x1) << 8)))
#define HRT_GET_WROT_SRAM_FLAG(hrt_num) ((hrt_num & 0x600) >> 9)
#define HRT_SET_WROT_SRAM_FLAG(hrt_num, value)                                 \
	(hrt_num = ((hrt_num & ~(0x600)) | ((value & 0x3) << 9)))

#define HRT_GET_NO_COMPRESS_FLAG(hrt_num) ((hrt_num & 0x7800) >> 11)
#define HRT_SET_NO_COMPRESS_FLAG(hrt_num, value)                               \
	(hrt_num = ((hrt_num & ~(0x7800)) | ((value & 0xf) << 11)))


#define HRT_GET_PATH_ID(hrt_path) (hrt_path & 0x1F)

// int layering_rule_start(struct drm_mtk_layering_info *disp_info, int
// debug_mode);
extern int hdmi_get_dev_info(int is_sf, void *info);
// int gen_hrt_pattern(void);
// int set_hrt_state(enum HRT_SYS_STATE sys_state, int en);
void mtk_register_layering_rule_ops(struct layering_rule_ops *ops,
				    struct layering_rule_info_t *info);
int mtk_get_phy_layer_limit(uint16_t layer_map_tb);
// int get_phy_ovl_layer_cnt(struct drm_mtk_layering_info *disp_info, int
// disp_idx);
// bool is_decouple_path(struct drm_mtk_layering_info *disp_info);
int mtk_rollback_resize_layer_to_GPU_range(
	struct drm_mtk_layering_info *disp_info, int disp_idx, int start_idx,
	int end_idx);
int mtk_rollback_all_resize_layer_to_GPU(
	struct drm_mtk_layering_info *disp_info, int disp_idx);
bool mtk_is_yuv(uint32_t format);
// bool is_argb_fmt(uint32_t format);
bool mtk_is_gles_layer(struct drm_mtk_layering_info *disp_info, int disp_idx,
		       int layer_idx);
bool mtk_has_layer_cap(struct drm_mtk_layer_config *layer_info,
		       enum MTK_LAYERING_CAPS l_caps);
void mtk_set_layering_opt(enum LYE_HELPER_OPT opt, int value);
void mtk_rollback_layer_to_GPU(struct drm_mtk_layering_info *disp_info,
			       int disp_idx, int i);

/* rollback and set NO_FBDC flag */
void mtk_rollback_compress_layer_to_GPU(struct drm_mtk_layering_info *disp_info,
					int disp_idx, int i);
bool mtk_is_layer_id_valid(struct drm_mtk_layering_info *disp_info,
			   int disp_idx, int i);
int mtk_layering_rule_ioctl(struct drm_device *drm, void *data,
	struct drm_file *file_priv);
#if IS_ENABLED(CONFIG_COMPAT)
int mtk_layering_rule_ioctl_compat(struct file *file, unsigned int cmd,
	unsigned long arg);
#endif

bool is_triple_disp(struct drm_mtk_layering_info *disp_info);
#endif
