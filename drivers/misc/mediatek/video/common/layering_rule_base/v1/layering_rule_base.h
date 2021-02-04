/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __LAYERING_RULE_BASE__
#define __LAYERING_RULE_BASE__

#include "ddp_mmp.h"
#include "disp_drv_log.h"
#include "disp_drv_platform.h"
#include "disp_lcm.h"
#include "disp_session.h"
#include "display_recorder.h"
#include "primary_display.h"

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

#define MAKE_UNIFIED_HRT_PATH_FMT(rsz_type, pipe_type, disp_type, id)          \
	(((rsz_type) << PATH_FMT_RSZ_SHIFT) |                                  \
	 ((pipe_type) << PATH_FMT_PIPE_SHIFT) |                                \
	 ((disp_type) << PATH_FMT_DISP_SHIFT) | ((id) << PATH_FMT_ID_SHIFT))

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
};

struct layering_rule_ops {
	bool (*resizing_rule)(struct disp_layer_info *disp_info);
	void (*scenario_decision)(struct disp_layer_info *disp_info);
	int *(*get_bound_table)(enum DISP_HW_MAPPING_TB_TYPE tb_type);
	int (*get_mapping_table)(enum DISP_HW_MAPPING_TB_TYPE tb_type,
				 int param);
	int (*get_hrt_bound)(int is_larb, int hrt_level);
	void (*rsz_by_gpu_info_change)(void);
	bool (*rollback_to_gpu_by_hw_limitation)(
	    struct disp_layer_info *disp_info);
	bool (*adaptive_dc_enabled)(void);
	bool (*has_hrt_limit)(struct disp_layer_info *disp_info, int disp_idx);
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
#define HRT_GET_DC_FLAG(hrt_num) ((hrt_num & 0x200) >> 9)
#define HRT_SET_DC_FLAG(hrt_num, value)                                        \
	(hrt_num = ((hrt_num & ~(0x200)) | (((value)&0x1) << 9)))
#define HRT_GET_PATH_SCENARIO(hrt_num) ((hrt_num & 0xFFFF0000) >> 16)
#define HRT_SET_PATH_SCENARIO(hrt_num, value)                                  \
	(hrt_num = ((hrt_num & ~(0xFFFF0000)) | ((value & 0xFFFF) << 16)))
#define HRT_GET_PATH_RSZ_TYPE(hrt_path) ((hrt_path >> PATH_FMT_RSZ_SHIFT) & 0x3)
#define HRT_GET_PATH_DISP_TYPE(hrt_path)                                       \
	((hrt_path >> PATH_FMT_DISP_SHIFT) & 0x3)
#define HRT_GET_PATH_PIPE_TYPE(hrt_path)                                       \
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
int rollback_all_resize_layer_to_GPU(struct disp_layer_info *disp_info,
				     int disp_idx);
bool is_yuv(enum DISP_FORMAT format);
bool is_argb_fmt(enum DISP_FORMAT format);
bool is_gles_layer(struct disp_layer_info *disp_info, int disp_idx,
		   int layer_idx);
bool has_layer_cap(struct layer_config *layer_info, enum LAYERING_CAPS l_caps);

#endif
