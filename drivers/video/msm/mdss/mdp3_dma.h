/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef MDP3_DMA_H
#define MDP3_DMA_H

#include <linux/notifier.h>
#include <linux/sched.h>

#define MDP_HISTOGRAM_BL_SCALE_MAX 1024
#define MDP_HISTOGRAM_BL_LEVEL_MAX 255
#define MDP_HISTOGRAM_FRAME_COUNT_MAX 0x20
#define MDP_HISTOGRAM_BIT_MASK_MAX 0x4
#define MDP_HISTOGRAM_CSC_MATRIX_MAX 0x2000
#define MDP_HISTOGRAM_CSC_VECTOR_MAX 0x200
#define MDP_HISTOGRAM_BIN_NUM	32
#define MDP_LUT_SIZE 256

enum {
	MDP3_DMA_P,
	MDP3_DMA_S,
	MDP3_DMA_E,
	MDP3_DMA_MAX
};

enum {
	MDP3_DMA_CAP_CURSOR = 0x1,
	MDP3_DMA_CAP_COLOR_CORRECTION = 0x2,
	MDP3_DMA_CAP_HISTOGRAM = 0x4,
	MDP3_DMA_CAP_GAMMA_CORRECTION = 0x8,
	MDP3_DMA_CAP_DITHER = 0x10,
	MDP3_DMA_CAP_ALL = 0x1F
};

enum {
	MDP3_DMA_OUTPUT_SEL_AHB,
	MDP3_DMA_OUTPUT_SEL_DSI_CMD,
	MDP3_DMA_OUTPUT_SEL_LCDC,
	MDP3_DMA_OUTPUT_SEL_DSI_VIDEO,
	MDP3_DMA_OUTPUT_SEL_MAX
};

enum {
	MDP3_DMA_IBUF_FORMAT_RGB888,
	MDP3_DMA_IBUF_FORMAT_RGB565,
	MDP3_DMA_IBUF_FORMAT_XRGB8888,
	MDP3_DMA_IBUF_FORMAT_UNDEFINED
};

enum {
	MDP3_DMA_OUTPUT_PACK_PATTERN_RGB = 0x21,
	MDP3_DMA_OUTPUT_PACK_PATTERN_RBG = 0x24,
	MDP3_DMA_OUTPUT_PACK_PATTERN_BGR = 0x12,
	MDP3_DMA_OUTPUT_PACK_PATTERN_BRG = 0x18,
	MDP3_DMA_OUTPUT_PACK_PATTERN_GBR = 0x06,
	MDP3_DMA_OUTPUT_PACK_PATTERN_GRB = 0x09,
};

enum {
	MDP3_DMA_OUTPUT_PACK_ALIGN_LSB,
	MDP3_DMA_OUTPUT_PACK_ALIGN_MSB
};

enum {
	MDP3_DMA_OUTPUT_COMP_BITS_4, /*4 bits per color component*/
	MDP3_DMA_OUTPUT_COMP_BITS_5,
	MDP3_DMA_OUTPUT_COMP_BITS_6,
	MDP3_DMA_OUTPUT_COMP_BITS_8,
};

enum {
	MDP3_DMA_CURSOR_FORMAT_ARGB888,
};

enum {
	MDP3_DMA_COLOR_CORRECT_SET_1,
	MDP3_DMA_COLOR_CORRECT_SET_2
};

enum {
	MDP3_DMA_LUT_POSITION_PRE,
	MDP3_DMA_LUT_POSITION_POST
};

enum {
	MDP3_DMA_LUT_DISABLE = 0x0,
	MDP3_DMA_LUT_ENABLE_C0 = 0x01,
	MDP3_DMA_LUT_ENABLE_C1 = 0x02,
	MDP3_DMA_LUT_ENABLE_C2 = 0x04,
	MDP3_DMA_LUT_ENABLE_ALL = 0x07,
};

enum {
	MDP3_DMA_HISTOGRAM_BIT_MASK_NONE = 0X0,
	MDP3_DMA_HISTOGRAM_BIT_MASK_ONE_MSB = 0x1,
	MDP3_DMA_HISTOGRAM_BIT_MASK_TWO_MSB = 0x2,
	MDP3_DMA_HISTOGRAM_BIT_MASK_THREE_MSB = 0x3
};

enum {
	MDP3_DMA_COLOR_FLIP_NONE,
	MDP3_DMA_COLOR_FLIP_COMP1 = 0x1,
	MDP3_DMA_COLOR_FLIP_COMP2 = 0x2,
	MDP3_DMA_COLOR_FLIP_COMP3 = 0x4,
};

enum {
	MDP3_DMA_CURSOR_BLEND_NONE = 0x0,
	MDP3_DMA_CURSOR_BLEND_PER_PIXEL_ALPHA =  0x3,
	MDP3_DMA_CURSOR_BLEND_CONSTANT_ALPHA = 0x5,
	MDP3_DMA_CURSOR_BLEND_COLOR_KEYING = 0x9
};

enum {
	MDP3_DMA_HISTO_OP_START,
	MDP3_DMA_HISTO_OP_STOP,
	MDP3_DMA_HISTO_OP_CANCEL,
	MDP3_DMA_HISTO_OP_RESET
};

enum {
	MDP3_DMA_HISTO_STATE_UNKNOWN,
	MDP3_DMA_HISTO_STATE_IDLE,
	MDP3_DMA_HISTO_STATE_RESET,
	MDP3_DMA_HISTO_STATE_START,
	MDP3_DMA_HISTO_STATE_READY,
};

enum {
	MDP3_DMA_CALLBACK_TYPE_VSYNC = 0x01,
	MDP3_DMA_CALLBACK_TYPE_DMA_DONE = 0x02,
	MDP3_DMA_CALLBACK_TYPE_HIST_RESET_DONE = 0x04,
	MDP3_DMA_CALLBACK_TYPE_HIST_DONE = 0x08,
};

struct mdp3_dma_source {
	u32 format;
	int width;
	int height;
	int x;
	int y;
	void *buf;
	int stride;
	int vsync_count;
	int vporch;
};

struct mdp3_dma_output_config {
	int dither_en;
	u32 out_sel;
	u32 bit_mask_polarity;
	u32 color_components_flip;
	u32 pack_pattern;
	u32 pack_align;
	u32 color_comp_out_bits;
};

struct mdp3_dma_cursor_blend_config {
	u32 mode;
	u32 transparent_color; /*color keying*/
	u32 transparency_mask;
	u32 constant_alpha;
};

struct mdp3_dma_cursor {
	int enable; /* enable cursor or not*/
	u32 format;
	int width;
	int height;
	int x;
	int y;
	void *buf;
	struct mdp3_dma_cursor_blend_config blend_config;
};

struct mdp3_dma_ccs {
	u32 *mv; /*set1 matrix vector, 3x3 */
	u32 *pre_bv; /*pre-bias vector for set1, 1x3*/
	u32 *post_bv; /*post-bias vecotr for set1,  */
	u32 *pre_lv; /*pre-limit vector for set 1, 1x6*/
	u32 *post_lv;
};

struct mdp3_dma_lut {
	u16 *color0_lut;
	u16 *color1_lut;
	u16 *color2_lut;
};

struct mdp3_dma_lut_config {
	int lut_enable;
	u32 lut_sel;
	u32 lut_position;
	bool lut_dirty;
};

struct mdp3_dma_color_correct_config {
	int ccs_enable;
	u32 post_limit_sel;
	u32 pre_limit_sel;
	u32 post_bias_sel;
	u32 pre_bias_sel;
	u32 ccs_sel;
	bool ccs_dirty;
};

struct mdp3_dma_histogram_config {
	int frame_count;
	u32 bit_mask_polarity;
	u32 bit_mask;
	int auto_clear_en;
};

struct mdp3_dma_histogram_data {
	u32 r_data[MDP_HISTOGRAM_BIN_NUM];
	u32 g_data[MDP_HISTOGRAM_BIN_NUM];
	u32 b_data[MDP_HISTOGRAM_BIN_NUM];
	u32 extra[2];
};

struct mdp3_notification {
	void (*handler)(void *arg);
	void *arg;
};

struct mdp3_tear_check {
	int frame_rate;
	bool hw_vsync_mode;
	u32 tear_check_en;
	u32 sync_cfg_height;
	u32 vsync_init_val;
	u32 sync_threshold_start;
	u32 sync_threshold_continue;
	u32 start_pos;
	u32 rd_ptr_irq;
	u32 refx100;
};

struct mdp3_intf;

struct mdp3_dma {
	u32 dma_sel;
	u32 capability;
	int in_use;
	int available;

	spinlock_t dma_lock;
	spinlock_t histo_lock;
	struct completion vsync_comp;
	struct completion dma_comp;
	struct completion histo_comp;
	struct mdp3_notification vsync_client;
	struct mdp3_notification dma_notifier_client;

	struct mdp3_dma_output_config output_config;
	struct mdp3_dma_source source_config;

	struct mdp3_dma_cursor cursor;
	struct mdp3_dma_color_correct_config ccs_config;
	struct mdp3_dma_lut_config lut_config;
	struct mdp3_dma_histogram_config histogram_config;
	int histo_state;
	struct mdp3_dma_histogram_data histo_data;
	unsigned int vsync_status;
	bool update_src_cfg;

	int (*dma_config)(struct mdp3_dma *dma,
			struct mdp3_dma_source *source_config,
			struct mdp3_dma_output_config *output_config);

	int (*dma_sync_config)(struct mdp3_dma *dma, struct mdp3_dma_source
				*source_config, struct mdp3_tear_check *te);

	void (*dma_config_source)(struct mdp3_dma *dma);

	int (*start)(struct mdp3_dma *dma, struct mdp3_intf *intf);

	int (*stop)(struct mdp3_dma *dma, struct mdp3_intf *intf);

	int (*config_cursor)(struct mdp3_dma *dma,
				struct mdp3_dma_cursor *cursor);

	int (*config_ccs)(struct mdp3_dma *dma,
			struct mdp3_dma_color_correct_config *config,
			struct mdp3_dma_ccs *ccs);

	int (*config_lut)(struct mdp3_dma *dma,
			struct mdp3_dma_lut_config *config,
			struct mdp3_dma_lut *lut);

	int (*update)(struct mdp3_dma *dma, void *buf, struct mdp3_intf *intf);

	int (*update_cursor)(struct mdp3_dma *dma, int x, int y);

	int (*get_histo)(struct mdp3_dma *dma);

	int (*config_histo)(struct mdp3_dma *dma,
				struct mdp3_dma_histogram_config *histo_config);

	int (*histo_op)(struct mdp3_dma *dma, u32 op);

	void (*vsync_enable)(struct mdp3_dma *dma,
			struct mdp3_notification *vsync_client);

	void (*dma_done_notifier)(struct mdp3_dma *dma,
			struct mdp3_notification *dma_client);
};

struct mdp3_video_intf_cfg {
	int hsync_period;
	int hsync_pulse_width;
	int vsync_period;
	int vsync_pulse_width;
	int display_start_x;
	int display_end_x;
	int display_start_y;
	int display_end_y;
	int active_start_x;
	int active_end_x;
	int active_h_enable;
	int active_start_y;
	int active_end_y;
	int active_v_enable;
	int hsync_skew;
	int hsync_polarity;
	int vsync_polarity;
	int de_polarity;
	int underflow_color;
};

struct mdp3_dsi_cmd_intf_cfg {
	int primary_dsi_cmd_id;
	int secondary_dsi_cmd_id;
	int dsi_cmd_tg_intf_sel;
};

struct mdp3_intf_cfg {
	u32 type;
	struct mdp3_video_intf_cfg video;
	struct mdp3_dsi_cmd_intf_cfg dsi_cmd;
};

struct mdp3_intf {
	struct mdp3_intf_cfg cfg;
	int active;
	int available;
	int in_use;
	int (*config)(struct mdp3_intf *intf, struct mdp3_intf_cfg *cfg);
	int (*start)(struct mdp3_intf *intf);
	int (*stop)(struct mdp3_intf *intf);
};

int mdp3_dma_init(struct mdp3_dma *dma);

int mdp3_intf_init(struct mdp3_intf *intf);

void mdp3_dma_callback_enable(struct mdp3_dma *dma, int type);

void mdp3_dma_callback_disable(struct mdp3_dma *dma, int type);

#endif /* MDP3_DMA_H */
