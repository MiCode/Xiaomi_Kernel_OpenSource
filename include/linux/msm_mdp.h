/* include/linux/msm_mdp.h
 *
 * Copyright (C) 2007 Google Incorporated
 * Copyright (c) 2012 Code Aurora Forum. All rights reserved.
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
#ifndef _MSM_MDP_H_
#define _MSM_MDP_H_

#include <linux/types.h>
#include <linux/fb.h>

#define MSMFB_IOCTL_MAGIC 'm'
#define MSMFB_GRP_DISP          _IOW(MSMFB_IOCTL_MAGIC, 1, unsigned int)
#define MSMFB_BLIT              _IOW(MSMFB_IOCTL_MAGIC, 2, unsigned int)
#define MSMFB_SUSPEND_SW_REFRESHER _IOW(MSMFB_IOCTL_MAGIC, 128, unsigned int)
#define MSMFB_RESUME_SW_REFRESHER _IOW(MSMFB_IOCTL_MAGIC, 129, unsigned int)
#define MSMFB_CURSOR _IOW(MSMFB_IOCTL_MAGIC, 130, struct fb_cursor)
#define MSMFB_SET_LUT _IOW(MSMFB_IOCTL_MAGIC, 131, struct fb_cmap)
#define MSMFB_HISTOGRAM _IOWR(MSMFB_IOCTL_MAGIC, 132, struct mdp_histogram)
/* new ioctls's for set/get ccs matrix */
#define MSMFB_GET_CCS_MATRIX  _IOWR(MSMFB_IOCTL_MAGIC, 133, struct mdp_ccs)
#define MSMFB_SET_CCS_MATRIX  _IOW(MSMFB_IOCTL_MAGIC, 134, struct mdp_ccs)
#define MSMFB_OVERLAY_SET       _IOWR(MSMFB_IOCTL_MAGIC, 135, \
						struct mdp_overlay)
#define MSMFB_OVERLAY_UNSET     _IOW(MSMFB_IOCTL_MAGIC, 136, unsigned int)
#define MSMFB_OVERLAY_PLAY      _IOW(MSMFB_IOCTL_MAGIC, 137, \
						struct msmfb_overlay_data)
#define MSMFB_GET_PAGE_PROTECTION _IOR(MSMFB_IOCTL_MAGIC, 138, \
					struct mdp_page_protection)
#define MSMFB_SET_PAGE_PROTECTION _IOW(MSMFB_IOCTL_MAGIC, 139, \
					struct mdp_page_protection)
#define MSMFB_OVERLAY_GET      _IOR(MSMFB_IOCTL_MAGIC, 140, \
						struct mdp_overlay)
#define MSMFB_OVERLAY_PLAY_ENABLE     _IOW(MSMFB_IOCTL_MAGIC, 141, unsigned int)
#define MSMFB_OVERLAY_BLT       _IOWR(MSMFB_IOCTL_MAGIC, 142, \
						struct msmfb_overlay_blt)
#define MSMFB_OVERLAY_BLT_OFFSET     _IOW(MSMFB_IOCTL_MAGIC, 143, unsigned int)
#define MSMFB_HISTOGRAM_START	_IO(MSMFB_IOCTL_MAGIC, 144)
#define MSMFB_HISTOGRAM_STOP	_IO(MSMFB_IOCTL_MAGIC, 145)
#define MSMFB_NOTIFY_UPDATE	_IOW(MSMFB_IOCTL_MAGIC, 146, unsigned int)

#define MSMFB_OVERLAY_3D       _IOWR(MSMFB_IOCTL_MAGIC, 147, \
						struct msmfb_overlay_3d)

#define MSMFB_MIXER_INFO       _IOWR(MSMFB_IOCTL_MAGIC, 148, \
						struct msmfb_mixer_info_req)
#define MSMFB_OVERLAY_PLAY_WAIT _IOWR(MSMFB_IOCTL_MAGIC, 149, \
						struct msmfb_overlay_data)
#define MSMFB_WRITEBACK_INIT _IO(MSMFB_IOCTL_MAGIC, 150)
#define MSMFB_WRITEBACK_START _IO(MSMFB_IOCTL_MAGIC, 151)
#define MSMFB_WRITEBACK_STOP _IO(MSMFB_IOCTL_MAGIC, 152)
#define MSMFB_WRITEBACK_QUEUE_BUFFER _IOW(MSMFB_IOCTL_MAGIC, 153, \
						struct msmfb_data)
#define MSMFB_WRITEBACK_DEQUEUE_BUFFER _IOW(MSMFB_IOCTL_MAGIC, 154, \
						struct msmfb_data)
#define MSMFB_WRITEBACK_TERMINATE _IO(MSMFB_IOCTL_MAGIC, 155)
#define MSMFB_MDP_PP _IOWR(MSMFB_IOCTL_MAGIC, 156, struct msmfb_mdp_pp)

#define FB_TYPE_3D_PANEL 0x10101010
#define MDP_IMGTYPE2_START 0x10000
#define MSMFB_DRIVER_VERSION	0xF9E8D701

enum {
	NOTIFY_UPDATE_START,
	NOTIFY_UPDATE_STOP,
};

enum {
	MDP_RGB_565,      /* RGB 565 planer */
	MDP_XRGB_8888,    /* RGB 888 padded */
	MDP_Y_CBCR_H2V2,  /* Y and CbCr, pseudo planer w/ Cb is in MSB */
	MDP_Y_CBCR_H2V2_ADRENO,
	MDP_ARGB_8888,    /* ARGB 888 */
	MDP_RGB_888,      /* RGB 888 planer */
	MDP_Y_CRCB_H2V2,  /* Y and CrCb, pseudo planer w/ Cr is in MSB */
	MDP_YCRYCB_H2V1,  /* YCrYCb interleave */
	MDP_Y_CRCB_H2V1,  /* Y and CrCb, pseduo planer w/ Cr is in MSB */
	MDP_Y_CBCR_H2V1,   /* Y and CrCb, pseduo planer w/ Cr is in MSB */
	MDP_RGBA_8888,    /* ARGB 888 */
	MDP_BGRA_8888,	  /* ABGR 888 */
	MDP_RGBX_8888,	  /* RGBX 888 */
	MDP_Y_CRCB_H2V2_TILE,  /* Y and CrCb, pseudo planer tile */
	MDP_Y_CBCR_H2V2_TILE,  /* Y and CbCr, pseudo planer tile */
	MDP_Y_CR_CB_H2V2,  /* Y, Cr and Cb, planar */
	MDP_Y_CR_CB_GH2V2,  /* Y, Cr and Cb, planar aligned to Android YV12 */
	MDP_Y_CB_CR_H2V2,  /* Y, Cb and Cr, planar */
	MDP_Y_CRCB_H1V1,  /* Y and CrCb, pseduo planer w/ Cr is in MSB */
	MDP_Y_CBCR_H1V1,  /* Y and CbCr, pseduo planer w/ Cb is in MSB */
	MDP_IMGTYPE_LIMIT,
	MDP_BGR_565 = MDP_IMGTYPE2_START,      /* BGR 565 planer */
	MDP_FB_FORMAT,    /* framebuffer format */
	MDP_IMGTYPE_LIMIT2 /* Non valid image type after this enum */
};

enum {
	PMEM_IMG,
	FB_IMG,
};

enum {
	HSIC_HUE = 0,
	HSIC_SAT,
	HSIC_INT,
	HSIC_CON,
	NUM_HSIC_PARAM,
};

/* mdp_blit_req flag values */
#define MDP_ROT_NOP 0
#define MDP_FLIP_LR 0x1
#define MDP_FLIP_UD 0x2
#define MDP_ROT_90 0x4
#define MDP_ROT_180 (MDP_FLIP_UD|MDP_FLIP_LR)
#define MDP_ROT_270 (MDP_ROT_90|MDP_FLIP_UD|MDP_FLIP_LR)
#define MDP_DITHER 0x8
#define MDP_BLUR 0x10
#define MDP_BLEND_FG_PREMULT 0x20000
#define MDP_DEINTERLACE 0x80000000
#define MDP_SHARPENING  0x40000000
#define MDP_NO_DMA_BARRIER_START	0x20000000
#define MDP_NO_DMA_BARRIER_END		0x10000000
#define MDP_NO_BLIT			0x08000000
#define MDP_BLIT_WITH_DMA_BARRIERS	0x000
#define MDP_BLIT_WITH_NO_DMA_BARRIERS    \
	(MDP_NO_DMA_BARRIER_START | MDP_NO_DMA_BARRIER_END)
#define MDP_BLIT_SRC_GEM                0x04000000
#define MDP_BLIT_DST_GEM                0x02000000
#define MDP_BLIT_NON_CACHED		0x01000000
#define MDP_OV_PIPE_SHARE		0x00800000
#define MDP_DEINTERLACE_ODD		0x00400000
#define MDP_OV_PLAY_NOWAIT		0x00200000
#define MDP_SOURCE_ROTATED_90		0x00100000
#define MDP_DPP_HSIC			0x00080000
#define MDP_BORDERFILL_SUPPORTED	0x00010000
#define MDP_SECURE_OVERLAY_SESSION      0x00008000
#define MDP_MEMORY_ID_TYPE_FB		0x00001000

#define MDP_TRANSP_NOP 0xffffffff
#define MDP_ALPHA_NOP 0xff

#define MDP_FB_PAGE_PROTECTION_NONCACHED         (0)
#define MDP_FB_PAGE_PROTECTION_WRITECOMBINE      (1)
#define MDP_FB_PAGE_PROTECTION_WRITETHROUGHCACHE (2)
#define MDP_FB_PAGE_PROTECTION_WRITEBACKCACHE    (3)
#define MDP_FB_PAGE_PROTECTION_WRITEBACKWACACHE  (4)
/* Sentinel: Don't use! */
#define MDP_FB_PAGE_PROTECTION_INVALID           (5)
/* Count of the number of MDP_FB_PAGE_PROTECTION_... values. */
#define MDP_NUM_FB_PAGE_PROTECTION_VALUES        (5)

struct mdp_rect {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
};

struct mdp_img {
	uint32_t width;
	uint32_t height;
	uint32_t format;
	uint32_t offset;
	int memory_id;		/* the file descriptor */
	uint32_t priv;
};

/*
 * {3x3} + {3} ccs matrix
 */

#define MDP_CCS_RGB2YUV 	0
#define MDP_CCS_YUV2RGB 	1

#define MDP_CCS_SIZE	9
#define MDP_BV_SIZE	3

struct mdp_ccs {
	int direction;			/* MDP_CCS_RGB2YUV or YUV2RGB */
	uint16_t ccs[MDP_CCS_SIZE];	/* 3x3 color coefficients */
	uint16_t bv[MDP_BV_SIZE];	/* 1x3 bias vector */
};

struct mdp_csc {
	int id;
	uint32_t csc_mv[9];
	uint32_t csc_pre_bv[3];
	uint32_t csc_post_bv[3];
	uint32_t csc_pre_lv[6];
	uint32_t csc_post_lv[6];
};

/* The version of the mdp_blit_req structure so that
 * user applications can selectively decide which functionality
 * to include
 */

#define MDP_BLIT_REQ_VERSION 2

struct mdp_blit_req {
	struct mdp_img src;
	struct mdp_img dst;
	struct mdp_rect src_rect;
	struct mdp_rect dst_rect;
	uint32_t alpha;
	uint32_t transp_mask;
	uint32_t flags;
	int sharpening_strength;  /* -127 <--> 127, default 64 */
};

struct mdp_blit_req_list {
	uint32_t count;
	struct mdp_blit_req req[];
};

#define MSMFB_DATA_VERSION 2

struct msmfb_data {
	uint32_t offset;
	int memory_id;
	int id;
	uint32_t flags;
	uint32_t priv;
	uint32_t iova;
};

#define MSMFB_NEW_REQUEST -1

struct msmfb_overlay_data {
	uint32_t id;
	struct msmfb_data data;
	uint32_t version_key;
	struct msmfb_data plane1_data;
	struct msmfb_data plane2_data;
};

struct msmfb_img {
	uint32_t width;
	uint32_t height;
	uint32_t format;
};

#define MSMFB_WRITEBACK_DEQUEUE_BLOCKING 0x1
struct msmfb_writeback_data {
	struct msmfb_data buf_info;
	struct msmfb_img img;
};

struct dpp_ctrl {
	/*
	 *'sharp_strength' has inputs = -128 <-> 127
	 *  Increasingly positive values correlate with increasingly sharper
	 *  picture. Increasingly negative values correlate with increasingly
	 *  smoothed picture.
	 */
	int8_t sharp_strength;
	int8_t hsic_params[NUM_HSIC_PARAM];
};

struct mdp_overlay {
	struct msmfb_img src;
	struct mdp_rect src_rect;
	struct mdp_rect dst_rect;
	uint32_t z_order;	/* stage number */
	uint32_t is_fg;		/* control alpha & transp */
	uint32_t alpha;
	uint32_t transp_mask;
	uint32_t flags;
	uint32_t id;
	uint32_t user_data[8];
	struct dpp_ctrl dpp;
};

struct msmfb_overlay_3d {
	uint32_t is_3d;
	uint32_t width;
	uint32_t height;
};


struct msmfb_overlay_blt {
	uint32_t enable;
	uint32_t offset;
	uint32_t width;
	uint32_t height;
	uint32_t bpp;
};

struct mdp_histogram {
	uint32_t frame_cnt;
	uint32_t bin_cnt;
	uint32_t *r;
	uint32_t *g;
	uint32_t *b;
};


/*

	mdp_block_type defines the identifiers for each of pipes in MDP 4.3

	MDP_BLOCK_RESERVED is provided for backward compatibility and is
	deprecated. It corresponds to DMA_P. So MDP_BLOCK_DMA_P should be used
	instead.

*/

enum {
	MDP_BLOCK_RESERVED = 0,
	MDP_BLOCK_OVERLAY_0,
	MDP_BLOCK_OVERLAY_1,
	MDP_BLOCK_VG_1,
	MDP_BLOCK_VG_2,
	MDP_BLOCK_RGB_1,
	MDP_BLOCK_RGB_2,
	MDP_BLOCK_DMA_P,
	MDP_BLOCK_DMA_S,
	MDP_BLOCK_DMA_E,
	MDP_BLOCK_MAX,
};

struct mdp_pcc_coeff {
	uint32_t c, r, g, b, rr, gg, bb, rg, gb, rb, rgb_0, rgb_1;
};

struct mdp_pcc_cfg_data {
	uint32_t block;
	uint32_t ops;
	struct mdp_pcc_coeff r, g, b;
};

#define MDP_CSC_FLAG_ENABLE	0x1
#define MDP_CSC_FLAG_YUV_IN	0x2
#define MDP_CSC_FLAG_YUV_OUT	0x4

struct mdp_csc_cfg {
	/* flags for enable CSC, toggling RGB,YUV input/output */
	uint32_t flags;
	uint32_t csc_mv[9];
	uint32_t csc_pre_bv[3];
	uint32_t csc_post_bv[3];
	uint32_t csc_pre_lv[6];
	uint32_t csc_post_lv[6];
};

struct mdp_csc_cfg_data {
	uint32_t block;
	struct mdp_csc_cfg csc_data;
};

enum {
	mdp_lut_igc,
	mdp_lut_pgc,
	mdp_lut_hist,
	mdp_lut_max,
};


struct mdp_igc_lut_data {
	uint32_t block;
	uint32_t len, ops;
	uint32_t *c0_c1_data;
	uint32_t *c2_data;
};

struct mdp_ar_gc_lut_data {
	uint32_t x_start;
	uint32_t slope;
	uint32_t offset;
};

struct mdp_pgc_lut_data {
	uint32_t block;
	uint32_t flags;
	uint8_t num_r_stages;
	uint8_t num_g_stages;
	uint8_t num_b_stages;
	struct mdp_ar_gc_lut_data *r_data;
	struct mdp_ar_gc_lut_data *g_data;
	struct mdp_ar_gc_lut_data *b_data;
};


struct mdp_hist_lut_data {
	uint32_t block;
	uint32_t ops;
	uint32_t len;
	uint32_t *data;
};


struct mdp_lut_cfg_data {
	uint32_t lut_type;
	union {
		struct mdp_igc_lut_data igc_lut_data;
		struct mdp_pgc_lut_data pgc_lut_data;
		struct mdp_hist_lut_data hist_lut_data;
	} data;
};

enum {
	mdp_op_pcc_cfg,
	mdp_op_csc_cfg,
	mdp_op_lut_cfg,
	mdp_op_max,
};

struct msmfb_mdp_pp {
	uint32_t op;
	union {
		struct mdp_pcc_cfg_data pcc_cfg_data;
		struct mdp_csc_cfg_data csc_cfg_data;
		struct mdp_lut_cfg_data lut_cfg_data;
	} data;
};


struct mdp_page_protection {
	uint32_t page_protection;
};


struct mdp_mixer_info {
	int pndx;
	int pnum;
	int ptype;
	int mixer_num;
	int z_order;
};

#define MAX_PIPE_PER_MIXER  4

struct msmfb_mixer_info_req {
	int mixer_num;
	int cnt;
	struct mdp_mixer_info info[MAX_PIPE_PER_MIXER];
};


#ifdef __KERNEL__

/* get the framebuffer physical address information */
int get_fb_phys_info(unsigned long *start, unsigned long *len, int fb_num);
struct fb_info *msm_fb_get_writeback_fb(void);
int msm_fb_writeback_init(struct fb_info *info);
int msm_fb_writeback_start(struct fb_info *info);
int msm_fb_writeback_queue_buffer(struct fb_info *info,
		struct msmfb_data *data);
int msm_fb_writeback_dequeue_buffer(struct fb_info *info,
		struct msmfb_data *data);
int msm_fb_writeback_stop(struct fb_info *info);
int msm_fb_writeback_terminate(struct fb_info *info);
#endif

#endif /*_MSM_MDP_H_*/
