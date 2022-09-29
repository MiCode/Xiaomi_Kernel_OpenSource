/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_DISP_ODDMR_H__
#define __MTK_DISP_ODDMR_H__
#include <linux/uaccess.h>
#include <drm/mediatek_drm.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include "../mtk_drm_crtc.h"
#include "../mtk_drm_ddp_comp.h"
#include "../mtk_dump.h"
#include "../mtk_drm_mmp.h"
#include "../mtk_drm_gem.h"
#include "../mtk_drm_fb.h"

#define OD_TABLE_MAX 2
#define DMR_TABLE_MAX 2
#define DMR_GAIN_MAX 15
#define OD_GAIN_MAX 15

enum ODDMR_STATE {
	ODDMR_INVALID = 0,
	ODDMR_LOAD_PARTS,
	ODDMR_LOAD_DONE,
	ODDMR_TABLE_UPDATING,
	ODDMR_INIT_DONE,
	ODDMR_MODE_DONE,
};
enum ODDMR_USER_CMD {
	ODDMR_CMD_OD_SET_WEIGHT,
	ODDMR_CMD_OD_ENABLE,
	ODDMR_CMD_DMR_ENABLE,
	ODDMR_CMD_OD_INIT_END,
	ODDMR_CMD_EOF_CHECK_TRIGGER,
	ODDMR_CMD_OD_TUNING_WRITE_SRAM,
	ODDMR_CMD_ODDMR_DDREN_OFF,
};
enum MTK_ODDMR_MODE_CHANGE_INDEX {
	MODE_ODDMR_NOT_DEFINED = 0,
	MODE_ODDMR_DSI_VFP = BIT(0),
	MODE_ODDMR_DSI_HFP = BIT(1),
	MODE_ODDMR_DSI_CLK = BIT(2),
	MODE_ODDMR_DSI_RES = BIT(3),
	MODE_ODDMR_DDIC = BIT(4),
	MODE_ODDMR_MSYNC20 = BIT(5),
	MODE_ODDMR_DUMMY_PKG = BIT(6),
	MODE_ODDMR_LTPO = BIT(7),
	MODE_ODDMR_MAX,
};

enum MTK_ODDMR_OD_MODE_TYPE {
	OD_MODE_TYPE_RGB444 = 0,
	OD_MODE_TYPE_RGB565 = 1,
	OD_MODE_TYPE_COMPRESS_18 = 2,
	OD_MODE_TYPE_RGB666 = 4,
	OD_MODE_TYPE_COMPRESS_12 = 5,
	OD_MODE_TYPE_RGB555 = 6,
	OD_MODE_TYPE_RGB888 = 7,
};

enum MTK_ODDMR_DMR_MODE_TYPE {
	DMR_MODE_TYPE_RGB8X8L4 = 0,
	DMR_MODE_TYPE_RGB8X8L8 = 1,
	DMR_MODE_TYPE_RGB4X4L4 = 2,
	DMR_MODE_TYPE_RGB4X4L8 = 3,
	DMR_MODE_TYPE_W2X2L4 = 4,
	DMR_MODE_TYPE_W2X2Q = 5,
	DMR_MODE_TYPE_RGB4X4Q = 6,
	DMR_MODE_TYPE_W4X4Q = 7,
	DMR_MODE_TYPE_RGB7X8Q = 8,
};

struct mtk_disp_oddmr_data {
	/* dujac not support update od table */
	bool is_od_support_table_update;
	bool is_support_rtff;
	bool is_od_support_hw_skip_first_frame;
	bool is_od_need_crop_garbage;
	bool is_od_need_force_clk;
	bool is_od_support_sec;
	int tile_overhead;
	uint32_t dmr_buffer_size;
	uint32_t odr_buffer_size;
	uint32_t odw_buffer_size;
};

struct mtk_disp_oddmr_od_data {
	int od_sram_read_sel;
	int od_sram_table_idx[2];
	/* TODO: sram 0,1 fixed pkg, need support sram1 update */
	struct cmdq_pkt *od_sram_pkgs[2];
	struct mtk_drm_gem_obj *r_channel;
	struct mtk_drm_gem_obj *g_channel;
	struct mtk_drm_gem_obj *b_channel;
};

struct mtk_disp_oddmr_dmr_data {
	atomic_t cur_table_idx;
	struct mtk_drm_gem_obj *mura_table;
};

struct mtk_disp_oddmr_cfg {
	uint32_t width;
	uint32_t height;
	uint32_t comp_in_width;
	uint32_t comp_overhead;
	uint32_t total_overhead;
};
/**
 * struct mtk_disp_oddmr - DISP_oddmr driver structure
 * @ddp_comp - structure containing type enum and hardware resources
 */
struct mtk_disp_oddmr {
	struct mtk_ddp_comp	 ddp_comp;
	const struct mtk_disp_oddmr_data *data;
	bool is_right_pipe;
	struct mtk_disp_oddmr_od_data od_data;
	struct mtk_disp_oddmr_dmr_data dmr_data;
	struct mtk_disp_oddmr_cfg cfg;
	atomic_t oddmr_clock_ref;
	int od_enable;
	int dmr_enable;
	unsigned int spr_enable;
	unsigned int spr_relay;
	unsigned int spr_format;
	/* only use in pipe0 */
	enum ODDMR_STATE od_state;
	enum ODDMR_STATE dmr_state;
};

int mtk_drm_ioctl_oddmr_load_param(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
int mtk_drm_ioctl_oddmr_ctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
void disp_oddmr_on_start_of_frame(void);
void mtk_oddmr_timing_chg(struct mtk_oddmr_timing *timing, struct cmdq_pkt *handle);
void mtk_oddmr_bl_chg(uint32_t bl_level, struct cmdq_pkt *handle);
int mtk_oddmr_hrt_cal_notify(int *oddmr_hrt);
void mtk_disp_oddmr_debug(const char *opt);
void mtk_oddmr_ddren(struct cmdq_pkt *cmdq_handle,
	struct drm_crtc *crtc, int en);
#endif
