/* Copyright (c) 2007, 2013-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2007 Google Incorporated
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MDP3_PPP_H
#define MDP3_PPP_H
#include "mdp3.h"
#include "mdss_fb.h"

#define PPP_WRITEL(val, off) MDP3_REG_WRITE(off, val)

#define MAX_BLIT_REQ 16
#define PPP_UPSCALE_MAX 64
#define PPP_BLUR_SCALE_MAX 128
#define PPP_LUT_MAX 256

#define MDPOP_SMART_BLIT        BIT(31) /* blit optimization flag */

/* MDP PPP Operations */
#define MDPOP_NOP               0
#define MDPOP_LR                BIT(0)	/* left to right flip */
#define MDPOP_UD                BIT(1)	/* up and down flip */
#define MDPOP_ROT90             BIT(2)	/* rotate image to 90 degree */
#define MDPOP_ROT180            (MDPOP_UD|MDPOP_LR)
#define MDPOP_ROT270            (MDPOP_ROT90|MDPOP_UD|MDPOP_LR)
#define MDPOP_ASCALE            BIT(7)
#define MDPOP_ALPHAB            BIT(8)	/* enable alpha blending */
#define MDPOP_TRANSP            BIT(9)	/* enable transparency */
#define MDPOP_DITHER            BIT(10)	/* enable dither */
#define MDPOP_SHARPENING		BIT(11) /* enable sharpening */
#define MDPOP_BLUR				BIT(12) /* enable blur */
#define MDPOP_FG_PM_ALPHA       BIT(13)
#define MDPOP_LAYER_IS_FG       BIT(14)

#define MDPOP_ROTATION (MDPOP_ROT90|MDPOP_LR|MDPOP_UD)

#define PPP_OP_CONVERT_YCBCR2RGB BIT(2)
#define PPP_OP_CONVERT_ON		BIT(3)
#define PPP_OP_SCALE_X_ON		BIT(0)
#define PPP_OP_SCALE_Y_ON		BIT(1)
#define PPP_OP_ROT_ON			BIT(8)
#define PPP_OP_ROT_90			BIT(9)
#define PPP_OP_FLIP_LR			BIT(10)
#define PPP_OP_FLIP_UD			BIT(11)
#define PPP_OP_BLEND_ON			BIT(12)
#define PPP_OP_BLEND_CONSTANT_ALPHA BIT(14)
#define PPP_OP_DITHER_EN		BIT(16)
#define PPP_BLEND_CALPHA_TRNASP BIT(24)

#define PPP_OP_BLEND_SRCPIXEL_ALPHA 0
#define PPP_OP_BLEND_ALPHA_BLEND_NORMAL 0
#define PPP_OP_BLEND_ALPHA_BLEND_REVERSE BIT(15)

#define PPP_BLEND_BG_USE_ALPHA_SEL      (1 << 0)
#define PPP_BLEND_BG_ALPHA_REVERSE      (1 << 3)
#define PPP_BLEND_BG_SRCPIXEL_ALPHA     (0 << 1)
#define PPP_BLEND_BG_DSTPIXEL_ALPHA     (1 << 1)
#define PPP_BLEND_BG_CONSTANT_ALPHA     (2 << 1)
#define PPP_BLEND_BG_CONST_ALPHA_VAL(x) ((x) << 24)
#define PPP_OP_BG_CHROMA_H2V1 BIT(25)

#define CLR_G 0x0
#define CLR_B 0x1
#define CLR_R 0x2
#define CLR_ALPHA 0x3

#define CLR_Y  CLR_G
#define CLR_CB CLR_B
#define CLR_CR CLR_R

/* from lsb to msb */
#define PPP_GET_PACK_PATTERN(a, x, y, z, bit) \
	(((a)<<(bit*3))|((x)<<(bit*2))|((y)<<bit)|(z))

/* Frame unpacking */
#define PPP_C0G_8BITS (BIT(1)|BIT(0))
#define PPP_C1B_8BITS (BIT(3)|BIT(2))
#define PPP_C2R_8BITS (BIT(5)|BIT(4))
#define PPP_C3A_8BITS (BIT(7)|BIT(6))

#define PPP_C0G_6BITS BIT(1)
#define PPP_C1B_6BITS BIT(3)
#define PPP_C2R_6BITS BIT(5)

#define PPP_C0G_5BITS BIT(0)
#define PPP_C1B_5BITS BIT(2)
#define PPP_C2R_5BITS BIT(4)

#define PPP_SRC_C3_ALPHA_EN BIT(8)

#define PPP_SRC_BPP_INTERLVD_1BYTES 0
#define PPP_SRC_BPP_INTERLVD_2BYTES BIT(9)
#define PPP_SRC_BPP_INTERLVD_3BYTES BIT(10)
#define PPP_SRC_BPP_INTERLVD_4BYTES (BIT(10)|BIT(9))

#define PPP_SRC_BPP_ROI_ODD_X BIT(11)
#define PPP_SRC_BPP_ROI_ODD_Y BIT(12)
#define PPP_SRC_INTERLVD_2COMPONENTS BIT(13)
#define PPP_SRC_INTERLVD_3COMPONENTS BIT(14)
#define PPP_SRC_INTERLVD_4COMPONENTS (BIT(14)|BIT(13))

#define PPP_SRC_UNPACK_TIGHT BIT(17)
#define PPP_SRC_UNPACK_LOOSE 0
#define PPP_SRC_UNPACK_ALIGN_LSB 0
#define PPP_SRC_UNPACK_ALIGN_MSB BIT(18)

#define PPP_SRC_FETCH_PLANES_INTERLVD 0
#define PPP_SRC_FETCH_PLANES_PSEUDOPLNR BIT(20)

#define PPP_OP_SRC_CHROMA_H2V1 BIT(18)
#define PPP_OP_SRC_CHROMA_H1V2 BIT(19)
#define PPP_OP_SRC_CHROMA_420 (BIT(18)|BIT(19))
#define PPP_OP_SRC_CHROMA_OFFSITE BIT(20)

#define PPP_DST_PACKET_CNT_INTERLVD_2ELEM BIT(9)
#define PPP_DST_PACKET_CNT_INTERLVD_3ELEM BIT(10)
#define PPP_DST_PACKET_CNT_INTERLVD_4ELEM (BIT(10)|BIT(9))
#define PPP_DST_PACKET_CNT_INTERLVD_6ELEM (BIT(11)|BIT(9))

#define PPP_DST_C3A_8BIT (BIT(7)|BIT(6))
#define PPP_DST_C3ALPHA_EN BIT(8)

#define PPP_DST_PACK_LOOSE 0
#define PPP_DST_PACK_TIGHT BIT(13)
#define PPP_DST_PACK_ALIGN_LSB 0
#define PPP_DST_PACK_ALIGN_MSB BIT(14)

#define PPP_DST_OUT_SEL_AXI 0
#define PPP_DST_OUT_SEL_MDDI BIT(15)

#define PPP_DST_BPP_2BYTES BIT(16)
#define PPP_DST_BPP_3BYTES BIT(17)
#define PPP_DST_BPP_4BYTES (BIT(17)|BIT(16))

#define PPP_DST_PLANE_INTERLVD 0
#define PPP_DST_PLANE_PLANAR BIT(18)
#define PPP_DST_PLANE_PSEUDOPLN BIT(19)

#define PPP_OP_DST_CHROMA_H2V1 BIT(21)
#define PPP_OP_DST_CHROMA_420 (BIT(21)|BIT(22))
#define PPP_OP_COLOR_SPACE_YCBCR BIT(17)

#define MDP_SCALE_Q_FACTOR 512
#define MDP_MAX_X_SCALE_FACTOR (MDP_SCALE_Q_FACTOR*4)
#define MDP_MIN_X_SCALE_FACTOR (MDP_SCALE_Q_FACTOR/4)
#define MDP_MAX_Y_SCALE_FACTOR (MDP_SCALE_Q_FACTOR*4)
#define MDP_MIN_Y_SCALE_FACTOR (MDP_SCALE_Q_FACTOR/4)

#define MDP_TOP_LUMA       16
#define MDP_TOP_CHROMA     0
#define MDP_BOTTOM_LUMA    19
#define MDP_BOTTOM_CHROMA  3
#define MDP_LEFT_LUMA      22
#define MDP_LEFT_CHROMA    6
#define MDP_RIGHT_LUMA     25
#define MDP_RIGHT_CHROMA   9

#define MDP_RGB_565_SRC_REG (PPP_C2R_5BITS | PPP_C0G_6BITS | \
	PPP_C1B_5BITS | PPP_SRC_BPP_INTERLVD_2BYTES | \
	PPP_SRC_INTERLVD_3COMPONENTS | PPP_SRC_UNPACK_TIGHT | \
	PPP_SRC_UNPACK_ALIGN_LSB | \
	PPP_SRC_FETCH_PLANES_INTERLVD)

#define MDP_RGB_888_SRC_REG (PPP_C2R_8BITS | PPP_C0G_8BITS | \
	PPP_C1B_8BITS | PPP_SRC_BPP_INTERLVD_3BYTES | \
	PPP_SRC_INTERLVD_3COMPONENTS | PPP_SRC_UNPACK_TIGHT | \
	PPP_SRC_UNPACK_ALIGN_LSB | PPP_SRC_FETCH_PLANES_INTERLVD)

#define MDP_RGBX_8888_SRC_REG (PPP_C2R_8BITS | PPP_C0G_8BITS | \
	PPP_C1B_8BITS | PPP_C3A_8BITS | \
	PPP_SRC_C3_ALPHA_EN | PPP_SRC_BPP_INTERLVD_4BYTES | \
	PPP_SRC_INTERLVD_4COMPONENTS | PPP_SRC_UNPACK_TIGHT | \
	PPP_SRC_UNPACK_ALIGN_LSB | \
	PPP_SRC_FETCH_PLANES_INTERLVD)

#define MDP_Y_CBCR_H2V2_SRC_REG (PPP_C2R_8BITS | PPP_C0G_8BITS | \
	PPP_C1B_8BITS | PPP_SRC_BPP_INTERLVD_2BYTES | \
	PPP_SRC_INTERLVD_2COMPONENTS | PPP_SRC_UNPACK_TIGHT | \
	PPP_SRC_UNPACK_ALIGN_LSB | \
	PPP_SRC_FETCH_PLANES_PSEUDOPLNR)

#define MDP_YCRYCB_H2V1_SRC_REG (PPP_C2R_8BITS | \
	PPP_C0G_8BITS | PPP_C1B_8BITS | \
	PPP_C3A_8BITS | PPP_SRC_BPP_INTERLVD_2BYTES | \
	PPP_SRC_INTERLVD_4COMPONENTS | \
	PPP_SRC_UNPACK_TIGHT | PPP_SRC_UNPACK_ALIGN_LSB)

#define MDP_Y_CRCB_H2V1_SRC_REG (PPP_C2R_8BITS | \
	PPP_C0G_8BITS | PPP_C1B_8BITS | \
	PPP_C3A_8BITS | PPP_SRC_BPP_INTERLVD_2BYTES | \
	PPP_SRC_INTERLVD_2COMPONENTS | PPP_SRC_UNPACK_TIGHT | \
	PPP_SRC_UNPACK_ALIGN_LSB | PPP_SRC_FETCH_PLANES_PSEUDOPLNR)

#define MDP_RGB_565_DST_REG (PPP_C0G_6BITS | \
	PPP_C1B_5BITS | PPP_C2R_5BITS | \
	PPP_DST_PACKET_CNT_INTERLVD_3ELEM | \
	PPP_DST_PACK_TIGHT | PPP_DST_PACK_ALIGN_LSB | \
	PPP_DST_OUT_SEL_AXI | PPP_DST_BPP_2BYTES | \
	PPP_DST_PLANE_INTERLVD)

#define MDP_RGB_888_DST_REG (PPP_C0G_8BITS | \
	PPP_C1B_8BITS | PPP_C2R_8BITS | \
	PPP_DST_PACKET_CNT_INTERLVD_3ELEM | PPP_DST_PACK_TIGHT | \
	PPP_DST_PACK_ALIGN_LSB | PPP_DST_OUT_SEL_AXI | \
	PPP_DST_BPP_3BYTES | PPP_DST_PLANE_INTERLVD)

#define MDP_RGBX_8888_DST_REG (PPP_C0G_8BITS | \
	PPP_C1B_8BITS | PPP_C2R_8BITS | PPP_C3A_8BITS | \
	PPP_DST_C3ALPHA_EN | PPP_DST_PACKET_CNT_INTERLVD_4ELEM | \
	PPP_DST_PACK_TIGHT | PPP_DST_PACK_ALIGN_LSB | \
	PPP_DST_OUT_SEL_AXI | PPP_DST_BPP_4BYTES | \
	PPP_DST_PLANE_INTERLVD)

#define MDP_Y_CBCR_H2V2_DST_REG (PPP_C2R_8BITS | \
	PPP_C0G_8BITS | PPP_C1B_8BITS | PPP_C3A_8BITS | \
	PPP_DST_PACKET_CNT_INTERLVD_2ELEM | \
	PPP_DST_PACK_TIGHT | PPP_DST_PACK_ALIGN_LSB | \
	PPP_DST_OUT_SEL_AXI | PPP_DST_BPP_2BYTES)

#define MDP_YCRYCB_H2V1_DST_REG (PPP_C2R_8BITS | PPP_C0G_8BITS | \
	PPP_C1B_8BITS | PPP_C3A_8BITS | PPP_DST_PACKET_CNT_INTERLVD_4ELEM | \
	PPP_DST_PACK_TIGHT | PPP_DST_PACK_ALIGN_LSB | \
	PPP_DST_OUT_SEL_AXI | PPP_DST_BPP_2BYTES | \
	PPP_DST_PLANE_INTERLVD)

#define MDP_Y_CRCB_H2V1_DST_REG (PPP_C2R_8BITS | \
	PPP_C0G_8BITS | PPP_C1B_8BITS | PPP_C3A_8BITS | \
	PPP_DST_PACKET_CNT_INTERLVD_2ELEM | PPP_DST_PACK_TIGHT | \
	PPP_DST_PACK_ALIGN_LSB | PPP_DST_OUT_SEL_AXI | \
	PPP_DST_BPP_2BYTES)

/* LUT */
#define MDP_LUT_C0_EN BIT(5)
#define MDP_LUT_C1_EN BIT(6)
#define MDP_LUT_C2_EN BIT(7)

/* Dither */
#define MDP_OP_DITHER_EN BIT(16)

/* Rotator */
#define MDP_OP_ROT_ON BIT(8)
#define MDP_OP_ROT_90 BIT(9)
#define MDP_OP_FLIP_LR BIT(10)
#define MDP_OP_FLIP_UD BIT(11)

/* Blend */
#define MDP_OP_BLEND_EN BIT(12)
#define MDP_OP_BLEND_EQ_SEL BIT(15)
#define MDP_OP_BLEND_TRANSP_EN BIT(24)
#define MDP_BLEND_MASK (MDP_OP_BLEND_EN | MDP_OP_BLEND_EQ_SEL | \
	MDP_OP_BLEND_TRANSP_EN | BIT(14) | BIT(13))

#define MDP_BLEND_ALPHA_SEL 13
#define MDP_BLEND_ALPHA_MASK 0x3
#define MDP_BLEND_CONST_ALPHA 24
#define MDP_BLEND_TRASP_COL_MASK 0xFFFFFF

/* CSC Matrix */
#define MDP_CSC_RGB2YUV		0
#define MDP_CSC_YUV2RGB		1

#define MDP_CSC_SIZE	9
#define MDP_BV_SIZE		3
#define MDP_LV_SIZE		4

enum ppp_lut_type {
	LUT_PRE_TABLE = 0,
	LUT_POST_TABLE,
};

enum ppp_csc_matrix {
	CSC_PRIMARY_MATRIX = 0,
	CSC_SECONDARY_MATRIX,
};

/* scale tables */
enum {
	PPP_DOWNSCALE_PT2TOPT4,
	PPP_DOWNSCALE_PT4TOPT6,
	PPP_DOWNSCALE_PT6TOPT8,
	PPP_DOWNSCALE_PT8TOPT1,
	PPP_DOWNSCALE_MAX,
};

struct ppp_table {
	uint32_t reg;
	uint32_t val;
};

struct ppp_resource {
	u64 next_ab;
	u64 next_ib;
	u64 clk_rate;
};

struct ppp_csc_table {
	int direction;			/* MDP_CCS_RGB2YUV or YUV2RGB */
	uint16_t fwd_matrix[MDP_CCS_SIZE];	/* 3x3 color coefficients */
	uint16_t rev_matrix[MDP_CCS_SIZE];	/* 3x3 color coefficients */
	uint16_t bv[MDP_BV_SIZE];	/* 1x3 bias vector */
	uint16_t lv[MDP_LV_SIZE];	/* 1x3 limit vector */
};

struct ppp_blend {
	int const_alpha;
	int trans_color; /*color keying*/
};

struct ppp_img_prop {
	int32_t x;
	int32_t y;
	uint32_t width;
	uint32_t height;
};

struct ppp_img_desc {
	struct ppp_img_prop prop;
	struct ppp_img_prop roi;
	int color_fmt;
	void *p0;  /* plane 0 */
	void *p1;
	void *p3;
	int stride0;
	int stride1;
	int stride2;
};

struct ppp_blit_op {
	struct ppp_img_desc src;
	struct ppp_img_desc dst;
	struct ppp_img_desc bg;
	struct ppp_blend blend;
	uint32_t mdp_op; /* Operations */
	uint32_t solid_fill_color;
	bool solid_fill;
};

struct ppp_edge_rep {
	uint32_t dst_roi_width;
	uint32_t dst_roi_height;
	uint32_t is_scale_enabled;

	/*
	 * positions of the luma pixel(relative to the image ) required for
	 * scaling the ROI
	 */
	int32_t luma_interp_point_left;
	int32_t luma_interp_point_right;
	int32_t luma_interp_point_top;
	int32_t luma_interp_point_bottom;

	/*
	 * positions of the chroma pixel(relative to the image ) required for
	 * interpolating a chroma value at all required luma positions
	 */
	int32_t chroma_interp_point_left;
	int32_t chroma_interp_point_right;
	int32_t chroma_interp_point_top;
	int32_t chroma_interp_point_bottom;

	/*
	 * a rectangular region within the chroma plane of the "image".
	 * Chroma pixels falling inside of this rectangle belongs to the ROI
	 */
	int32_t chroma_bound_left;
	int32_t chroma_bound_right;
	int32_t chroma_bound_top;
	int32_t chroma_bound_bottom;

	/*
	 * number of chroma pixels to replicate on the left, right,
	 * top and bottom edge of the ROI.
	 */
	int32_t chroma_repeat_left;
	int32_t chroma_repeat_right;
	int32_t chroma_repeat_top;
	int32_t chroma_repeat_bottom;

	/*
	 * number of luma pixels to replicate on the left, right,
	 * top and bottom edge of the ROI.
	 */
	int32_t luma_repeat_left;
	int32_t luma_repeat_right;
	int32_t luma_repeat_top;
	int32_t luma_repeat_bottom;
};

bool check_if_rgb(int color);

/* func for ppp register values */
uint32_t ppp_bpp(uint32_t type);
uint32_t ppp_src_config(uint32_t type);
uint32_t ppp_out_config(uint32_t type);
uint32_t ppp_pack_pattern(uint32_t type, uint32_t yuv2rgb);
uint32_t ppp_dst_op_reg(uint32_t type);
uint32_t ppp_src_op_reg(uint32_t type);
bool ppp_per_p_alpha(uint32_t type);
bool ppp_multi_plane(uint32_t type);
uint32_t *ppp_default_pre_lut(void);
uint32_t *ppp_default_post_lut(void);
struct ppp_csc_table *ppp_csc_rgb2yuv(void);
struct ppp_csc_table *ppp_csc_table2(void);
void ppp_load_up_lut(void);
void ppp_load_gaussian_lut(void);
void ppp_load_x_scale_table(int idx);
void ppp_load_y_scale_table(int idx);

int mdp3_ppp_res_init(struct msm_fb_data_type *mfd);
int mdp3_ppp_init(void);
int config_ppp_op_mode(struct ppp_blit_op *blit_op);
void ppp_enable(void);
int mdp3_ppp_parse_req(void __user *p,
	struct mdp_async_blit_req_list *req_list_header,
	int async);

#endif
