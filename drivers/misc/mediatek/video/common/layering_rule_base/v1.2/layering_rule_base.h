/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef __LAYERING_RULE_BASE__
#define __LAYERING_RULE_BASE__

#include "disp_session.h"
#include "disp_lcm.h"
#include "disp_drv_log.h"
#include "primary_display.h"
#include "disp_drv_platform.h"
#include "display_recorder.h"

#define PRIMARY_OVL_LAYER_NUM PRIMARY_SESSION_INPUT_LAYER_COUNT
#define SECONDARY_OVL_LAYER_NUM EXTERNAL_SESSION_INPUT_LAYER_COUNT

/* #define HRT_DEBUG_LEVEL1 */
/* #define HRT_DEBUG_LEVEL2 */
#define HRT_UT_DEBUG

#define PATH_FMT_RSZ_SHIFT 9
#define PATH_FMT_PIPE_SHIFT 7
#define PATH_FMT_DISP_SHIFT 5
#define PATH_FMT_ID_SHIFT 0

#define HRT_UINT_BOUND_BPP 4
#define HRT_UINT_WEIGHT 100
/* bpp x uint weight = 2 x 100 */
#define HRT_AEE_WEIGHT 200
#define HRT_ROUND_CORNER_WEIGHT 200

#define MAKE_UNIFIED_HRT_PATH_FMT(rsz_type, pipe_type, disp_type, id) \
	( \
	((rsz_type)		<< PATH_FMT_RSZ_SHIFT)	| \
	((pipe_type)		<< PATH_FMT_PIPE_SHIFT)	| \
	((disp_type)		<< PATH_FMT_DISP_SHIFT)	| \
	((id)			<< PATH_FMT_ID_SHIFT))

enum HRT_SCALE_SCENARIO {
	HRT_SCALE_NONE = 0,
	HRT_SCALE_133,
	HRT_SCALE_150,
	HRT_SCALE_200,
	HRT_SCALE_266,
	HRT_SCALE_UNKNOWN,
	HRT_SCALE_NUM = HRT_SCALE_UNKNOWN,
};

enum HRT_PATH_RSZ_TYPE {
	HRT_PATH_RSZ_NONE = 0,
	HRT_PATH_RSZ_GENERAL,
	HRT_PATH_RSZ_PARTIAL,
	HRT_PATH_RSZ_PARTIAL_PMA,
};

enum HRT_PATH_PIPE_TYPE {
	HRT_PATH_PIPE_SINGLE = 0,
	HRT_PATH_PIPE_DUAL,
};

enum HRT_PATH_DISP_TYPE {
	HRT_PATH_DISP_SINGLE = 0,
	HRT_PATH_DISP_DUAL_MIRROR,
	HRT_PATH_DISP_DUAL_EXT,
};

enum HRT_DISP_TYPE {
	HRT_PRIMARY = 0,
	HRT_SECONDARY,
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
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	LYE_OPT_ROUND_CORNER,
#endif
	LYE_OPT_NUM
};

enum ADJUST_LAYOUT_PURPOSE {
	ADJUST_LAYOUT_EXT_GROUPING,
	ADJUST_LAYOUT_OVERLAP_CAL,
};

struct hrt_sort_entry {
	struct hrt_sort_entry *head, *tail;
	struct layer_config *layer_info;
	int key;
	int overlap_w;
};

struct layering_rule_info_t {
	int layer_tb_idx;
	int bound_tb_idx;
	int disp_path;
	int scale_rate;
	int dal_enable;
	int primary_fps;
	int hrt_sys_state;
	int wrot_sram;
	unsigned int hrt_idx;
};

struct layering_rule_ops {
	bool (*resizing_rule)(struct disp_layer_info *disp_info);
	void (*scenario_decision)(struct disp_layer_info *disp_info);
	int *(*get_bound_table)(enum DISP_HW_MAPPING_TB_TYPE tb_type);
	int (*get_mapping_table)(enum DISP_HW_MAPPING_TB_TYPE tb_type,
				 int param);
	/* should be removed */
	int (*get_hrt_bound)(int is_larb, int hrt_level);

	void (*copy_hrt_bound_table)(int is_larb, int *hrt_table);
	void (*rsz_by_gpu_info_change)(void);
	bool (*rollback_to_gpu_by_hw_limitation)(
					struct disp_layer_info *disp_info);
	bool (*unset_disp_rsz_attr)(struct disp_layer_info *disp_info,
				    int disp_idx);
	bool (*adaptive_dc_enabled)(void);
	bool (*rollback_all_to_GPU_for_idle)(void);
	bool (*frame_has_wcg)(struct disp_layer_info *disp_info,
			      enum HRT_DISP_TYPE di);
	/* for fbdc */
	void (*fbdc_pre_calculate)(struct disp_layer_info *disp_info);
	void (*fbdc_adjust_layout)(struct disp_layer_info *disp_info,
		enum ADJUST_LAYOUT_PURPOSE p);
	void (*fbdc_restore_layout)(struct disp_layer_info *dst_info,
		enum ADJUST_LAYOUT_PURPOSE p);
	void (*fbdc_rule)(struct disp_layer_info *disp_info);
};

#define HRT_GET_DVFS_LEVEL(hrt_num) (hrt_num & 0xF)
#define HRT_SET_DVFS_LEVEL(hrt_num, value) \
	(hrt_num = ((hrt_num & ~(0xF)) | (value & 0xF)))
#define HRT_GET_SCALE_SCENARIO(hrt_num) ((hrt_num & 0xF0) >> 4)
#define HRT_SET_SCALE_SCENARIO(hrt_num, value) \
	(hrt_num = ((hrt_num & ~(0xF0)) | ((value & 0xF) << 4)))
#define HRT_GET_AEE_FLAG(hrt_num) ((hrt_num & 0x100) >> 8)
#define HRT_SET_AEE_FLAG(hrt_num, value) \
	(hrt_num = ((hrt_num & ~(0x100)) | ((value & 0x1) << 8)))
#define HRT_GET_WROT_SRAM_FLAG(hrt_num) ((hrt_num & 0x600) >> 9)
#define HRT_SET_WROT_SRAM_FLAG(hrt_num, value) \
	(hrt_num = ((hrt_num & ~(0x600)) | ((value & 0x3) << 9)))
#define HRT_GET_DC_FLAG(hrt_num) ((hrt_num & 0x800) >> 11)
#define HRT_SET_DC_FLAG(hrt_num, value) \
	(hrt_num = ((hrt_num & ~(0x800)) | (((value) & 0x1) << 11)))
#define HRT_GET_PATH_SCENARIO(hrt_num) ((hrt_num & 0xFFFF0000) >> 16)
#define HRT_SET_PATH_SCENARIO(hrt_num, value) \
	(hrt_num = ((hrt_num & ~(0xFFFF0000)) | ((value & 0xFFFF) << 16)))
#define HRT_GET_PATH_RSZ_TYPE(hrt_path) \
	((hrt_path >> PATH_FMT_RSZ_SHIFT) & 0x3)
#define HRT_GET_PATH_DISP_TYPE(hrt_path) \
	((hrt_path >> PATH_FMT_DISP_SHIFT) & 0x3)
#define HRT_GET_PATH_PIPE_TYPE(hrt_path) \
	((hrt_path >> PATH_FMT_PIPE_SHIFT) & 0x3)
#define HRT_GET_PATH_ID(hrt_path) (hrt_path & 0x1F)

int layering_rule_start(struct disp_layer_info *disp_info, int debug_mode);
extern int hdmi_get_dev_info(int is_sf, void *info);
int gen_hrt_pattern(void);
int set_hrt_state(enum HRT_SYS_STATE sys_state, int en);
void register_layering_rule_ops(struct layering_rule_ops *ops,
				struct layering_rule_info_t *info);
int get_phy_layer_limit(int layer_map_tb, int disp_idx);
bool is_ext_path(struct disp_layer_info *disp_info);
int get_phy_ovl_layer_cnt(struct disp_layer_info *disp_info, int disp_idx);
bool is_max_lcm_resolution(void);
bool is_decouple_path(struct disp_layer_info *disp_info);
int rollback_resize_layer_to_GPU_range(struct disp_layer_info *disp_info,
				int disp_idx, int start_idx, int end_idx);
int rollback_all_resize_layer_to_GPU(struct disp_layer_info *disp_info,
				     int disp_idx);
bool is_yuv(enum DISP_FORMAT format);
bool is_argb_fmt(enum DISP_FORMAT format);
bool is_gles_layer(struct disp_layer_info *disp_info,
		   int disp_idx, int layer_idx);
bool has_layer_cap(struct layer_config *layer_info, enum LAYERING_CAPS l_caps);
void set_layering_opt(enum LYE_HELPER_OPT opt, int value);
int get_layering_opt(enum LYE_HELPER_OPT opt);
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
void set_round_corner_opt(enum LYE_HELPER_OPT opt, int value);
int get_round_corner_opt(enum LYE_HELPER_OPT opt);
#endif
void rollback_layer_to_GPU(struct disp_layer_info *disp_info, int disp_idx,
	int i);

/* rollback and set NO_FBDC flag */
void rollback_compress_layer_to_GPU(struct disp_layer_info *disp_info,
	int disp_idx, int i);
bool is_layer_id_valid(struct disp_layer_info *disp_info,
	int disp_idx, int i);
#endif
