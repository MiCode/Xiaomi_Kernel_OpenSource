#ifndef _UAPI_MSM_MDP_H_
#define _UAPI_MSM_MDP_H_

#include <linux/types.h>
#include <linux/fb.h>

#define MSMFB_IOCTL_MAGIC 'm'
#define MSMFB_GRP_DISP          _IOW(MSMFB_IOCTL_MAGIC, 1, unsigned int)
#define MSMFB_BLIT              _IOW(MSMFB_IOCTL_MAGIC, 2, unsigned int)
#define MSMFB_SUSPEND_SW_REFRESHER _IOW(MSMFB_IOCTL_MAGIC, 128, unsigned int)
#define MSMFB_RESUME_SW_REFRESHER _IOW(MSMFB_IOCTL_MAGIC, 129, unsigned int)
#define MSMFB_CURSOR _IOW(MSMFB_IOCTL_MAGIC, 130, struct fb_cursor)
#define MSMFB_SET_LUT _IOW(MSMFB_IOCTL_MAGIC, 131, struct fb_cmap)
#define MSMFB_HISTOGRAM _IOWR(MSMFB_IOCTL_MAGIC, 132, struct mdp_histogram_data)
/* new ioctls's for set/get ccs matrix */
#define MSMFB_GET_CCS_MATRIX  _IOWR(MSMFB_IOCTL_MAGIC, 133, struct mdp_ccs)
#define MSMFB_SET_CCS_MATRIX  _IOW(MSMFB_IOCTL_MAGIC, 134, struct mdp_ccs)
#define MSMFB_OVERLAY_SET       _IOWR(MSMFB_IOCTL_MAGIC, 135, \
						struct mdp_overlay)
#define MSMFB_OVERLAY_UNSET     _IOW(MSMFB_IOCTL_MAGIC, 136, unsigned int)

#define MSMFB_OVERLAY_PLAY      _IOW(MSMFB_IOCTL_MAGIC, 137, \
						struct msmfb_overlay_data)
#define MSMFB_OVERLAY_QUEUE	MSMFB_OVERLAY_PLAY

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
#define MSMFB_HISTOGRAM_START	_IOR(MSMFB_IOCTL_MAGIC, 144, \
						struct mdp_histogram_start_req)
#define MSMFB_HISTOGRAM_STOP	_IOR(MSMFB_IOCTL_MAGIC, 145, unsigned int)
#define MSMFB_NOTIFY_UPDATE	_IOWR(MSMFB_IOCTL_MAGIC, 146, unsigned int)

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
#define MSMFB_OVERLAY_VSYNC_CTRL _IOW(MSMFB_IOCTL_MAGIC, 160, unsigned int)
#define MSMFB_VSYNC_CTRL  _IOW(MSMFB_IOCTL_MAGIC, 161, unsigned int)
#define MSMFB_BUFFER_SYNC  _IOW(MSMFB_IOCTL_MAGIC, 162, struct mdp_buf_sync)
#define MSMFB_OVERLAY_COMMIT      _IO(MSMFB_IOCTL_MAGIC, 163)
#define MSMFB_DISPLAY_COMMIT      _IOW(MSMFB_IOCTL_MAGIC, 164, \
						struct mdp_display_commit)
#define MSMFB_METADATA_SET  _IOW(MSMFB_IOCTL_MAGIC, 165, struct msmfb_metadata)
#define MSMFB_METADATA_GET  _IOW(MSMFB_IOCTL_MAGIC, 166, struct msmfb_metadata)
#define MSMFB_WRITEBACK_SET_MIRRORING_HINT _IOW(MSMFB_IOCTL_MAGIC, 167, \
						unsigned int)
#define MSMFB_ASYNC_BLIT              _IOW(MSMFB_IOCTL_MAGIC, 168, unsigned int)
#define MSMFB_OVERLAY_PREPARE		_IOWR(MSMFB_IOCTL_MAGIC, 169, \
						struct mdp_overlay_list)
#define MSMFB_LPM_ENABLE	_IOWR(MSMFB_IOCTL_MAGIC, 170, unsigned int)

#define FB_TYPE_3D_PANEL 0x10101010
#define MDP_IMGTYPE2_START 0x10000
#define MSMFB_DRIVER_VERSION	0xF9E8D701

/* HW Revisions for different MDSS targets */
#define MDSS_GET_MAJOR(rev)		((rev) >> 28)
#define MDSS_GET_MINOR(rev)		(((rev) >> 16) & 0xFFF)
#define MDSS_GET_STEP(rev)		((rev) & 0xFFFF)
#define MDSS_GET_MAJOR_MINOR(rev)	((rev) >> 16)

#define IS_MDSS_MAJOR_MINOR_SAME(rev1, rev2)	\
	(MDSS_GET_MAJOR_MINOR((rev1)) == MDSS_GET_MAJOR_MINOR((rev2)))

#define MDSS_MDP_REV(major, minor, step)	\
	((((major) & 0x000F) << 28) |		\
	 (((minor) & 0x0FFF) << 16) |		\
	 ((step)   & 0xFFFF))

#define MDSS_MDP_HW_REV_100	MDSS_MDP_REV(1, 0, 0) /* 8974 v1.0 */
#define MDSS_MDP_HW_REV_101	MDSS_MDP_REV(1, 1, 0) /* 8x26 v1.0 */
#define MDSS_MDP_HW_REV_101_1	MDSS_MDP_REV(1, 1, 1) /* 8x26 v2.0, 8926 v1.0 */
#define MDSS_MDP_HW_REV_101_2	MDSS_MDP_REV(1, 1, 2) /* 8926 v2.0 */
#define MDSS_MDP_HW_REV_102	MDSS_MDP_REV(1, 2, 0) /* 8974 v2.0 */
#define MDSS_MDP_HW_REV_102_1	MDSS_MDP_REV(1, 2, 1) /* 8974 v3.0 (Pro) */
#define MDSS_MDP_HW_REV_103	MDSS_MDP_REV(1, 3, 0) /* 8084 v1.0 */
#define MDSS_MDP_HW_REV_103_1	MDSS_MDP_REV(1, 3, 1) /* 8084 v1.1 */
#define MDSS_MDP_HW_REV_105	MDSS_MDP_REV(1, 5, 0) /* 8994 v1.0 */
#define MDSS_MDP_HW_REV_106	MDSS_MDP_REV(1, 6, 0) /* 8916 v1.0 */
#define MDSS_MDP_HW_REV_107	MDSS_MDP_REV(1, 7, 0)
#define MDSS_MDP_HW_REV_108	MDSS_MDP_REV(1, 8, 0) /* 8939 v1.0 */
#define MDSS_MDP_HW_REV_109	MDSS_MDP_REV(1, 9, 0) /* 8994 v2.0 */
#define MDSS_MDP_HW_REV_110	MDSS_MDP_REV(1, 10, 0) /* 8992 v1.0 */
#define MDSS_MDP_HW_REV_200	MDSS_MDP_REV(2, 0, 0) /* 8092 v1.0 */

enum {
	NOTIFY_UPDATE_START,
	NOTIFY_UPDATE_STOP,
	NOTIFY_UPDATE_POWER_OFF,
};

enum {
	NOTIFY_TYPE_NO_UPDATE,
	NOTIFY_TYPE_SUSPEND,
	NOTIFY_TYPE_UPDATE,
	NOTIFY_TYPE_BL_UPDATE,
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
	MDP_CBYCRY_H2V1,  /* CbYCrY interleave */
	MDP_Y_CRCB_H2V1,  /* Y and CrCb, pseduo planer w/ Cr is in MSB */
	MDP_Y_CBCR_H2V1,   /* Y and CrCb, pseduo planer w/ Cr is in MSB */
	MDP_Y_CRCB_H1V2,
	MDP_Y_CBCR_H1V2,
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
	MDP_YCRCB_H1V1,   /* YCrCb interleave */
	MDP_YCBCR_H1V1,   /* YCbCr interleave */
	MDP_BGR_565,      /* BGR 565 planer */
	MDP_BGR_888,      /* BGR 888 */
	MDP_Y_CBCR_H2V2_VENUS,
	MDP_BGRX_8888,   /* BGRX 8888 */
	MDP_RGBA_8888_TILE,	  /* RGBA 8888 in tile format */
	MDP_ARGB_8888_TILE,	  /* ARGB 8888 in tile format */
	MDP_ABGR_8888_TILE,	  /* ABGR 8888 in tile format */
	MDP_BGRA_8888_TILE,	  /* BGRA 8888 in tile format */
	MDP_RGBX_8888_TILE,	  /* RGBX 8888 in tile format */
	MDP_XRGB_8888_TILE,	  /* XRGB 8888 in tile format */
	MDP_XBGR_8888_TILE,	  /* XBGR 8888 in tile format */
	MDP_BGRX_8888_TILE,	  /* BGRX 8888 in tile format */
	MDP_YCBYCR_H2V1,  /* YCbYCr interleave */
	MDP_RGB_565_TILE,	  /* RGB 565 in tile format */
	MDP_BGR_565_TILE,	  /* BGR 565 in tile format */
	MDP_ARGB_1555,	/*ARGB 1555*/
	MDP_RGBA_5551,	/*RGBA 5551*/
	MDP_ARGB_4444,	/*ARGB 4444*/
	MDP_RGBA_4444,	/*RGBA 4444*/
	MDP_RGB_565_UBWC,
	MDP_RGBA_8888_UBWC,
	MDP_Y_CBCR_H2V2_UBWC,
	MDP_IMGTYPE_LIMIT,
	MDP_RGB_BORDERFILL,	/* border fill pipe */
	MDP_FB_FORMAT = MDP_IMGTYPE2_START,    /* framebuffer format */
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

#define MDSS_MDP_ROT_ONLY		0x80
#define MDSS_MDP_RIGHT_MIXER		0x100
#define MDSS_MDP_DUAL_PIPE		0x200

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
#define MDP_IS_FG 0x40000
#define MDP_SOLID_FILL 0x00000020
#define MDP_VPU_PIPE 0x00000040
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
#define MDP_OVERLAY_PP_CFG_EN		0x00080000
#define MDP_BACKEND_COMPOSITION		0x00040000
#define MDP_BORDERFILL_SUPPORTED	0x00010000
#define MDP_SECURE_OVERLAY_SESSION      0x00008000
#define MDP_SECURE_DISPLAY_OVERLAY_SESSION	0x00002000
#define MDP_OV_PIPE_FORCE_DMA		0x00004000
#define MDP_MEMORY_ID_TYPE_FB		0x00001000
#define MDP_BWC_EN			0x00000400
#define MDP_DECIMATION_EN		0x00000800
#define MDP_SMP_FORCE_ALLOC		0x00200000
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

struct color {
	uint32_t r;
	uint32_t g;
	uint32_t b;
	uint32_t alpha;
};

struct mdp_blit_req {
	struct mdp_img src;
	struct mdp_img dst;
	struct mdp_rect src_rect;
	struct mdp_rect dst_rect;
	struct color const_color;
	uint32_t alpha;
	uint32_t transp_mask;
	uint32_t flags;
	int sharpening_strength;  /* -127 <--> 127, default 64 */
	uint8_t color_space;
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
	struct msmfb_data dst_data;
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

#define MDP_PP_OPS_ENABLE 0x1
#define MDP_PP_OPS_READ 0x2
#define MDP_PP_OPS_WRITE 0x4
#define MDP_PP_OPS_DISABLE 0x8
#define MDP_PP_IGC_FLAG_ROM0	0x10
#define MDP_PP_IGC_FLAG_ROM1	0x20

#define MDP_PP_PA_HUE_ENABLE		0x10
#define MDP_PP_PA_SAT_ENABLE		0x20
#define MDP_PP_PA_VAL_ENABLE		0x40
#define MDP_PP_PA_CONT_ENABLE		0x80
#define MDP_PP_PA_SIX_ZONE_ENABLE	0x100
#define MDP_PP_PA_SKIN_ENABLE		0x200
#define MDP_PP_PA_SKY_ENABLE		0x400
#define MDP_PP_PA_FOL_ENABLE		0x800
#define MDP_PP_PA_HUE_MASK		0x1000
#define MDP_PP_PA_SAT_MASK		0x2000
#define MDP_PP_PA_VAL_MASK		0x4000
#define MDP_PP_PA_CONT_MASK		0x8000
#define MDP_PP_PA_SIX_ZONE_HUE_MASK	0x10000
#define MDP_PP_PA_SIX_ZONE_SAT_MASK	0x20000
#define MDP_PP_PA_SIX_ZONE_VAL_MASK	0x40000
#define MDP_PP_PA_MEM_COL_SKIN_MASK	0x80000
#define MDP_PP_PA_MEM_COL_SKY_MASK	0x100000
#define MDP_PP_PA_MEM_COL_FOL_MASK	0x200000
#define MDP_PP_PA_MEM_PROTECT_EN	0x400000
#define MDP_PP_PA_SAT_ZERO_EXP_EN	0x800000

#define MDSS_PP_DSPP_CFG	0x000
#define MDSS_PP_SSPP_CFG	0x100
#define MDSS_PP_LM_CFG	0x200
#define MDSS_PP_WB_CFG	0x300

#define MDSS_PP_ARG_MASK	0x3C00
#define MDSS_PP_ARG_NUM		4
#define MDSS_PP_ARG_SHIFT	10
#define MDSS_PP_LOCATION_MASK	0x0300
#define MDSS_PP_LOGICAL_MASK	0x00FF

#define MDSS_PP_ADD_ARG(var, arg) ((var) | (0x1 << (MDSS_PP_ARG_SHIFT + (arg))))
#define PP_ARG(x, var) ((var) & (0x1 << (MDSS_PP_ARG_SHIFT + (x))))
#define PP_LOCAT(var) ((var) & MDSS_PP_LOCATION_MASK)
#define PP_BLOCK(var) ((var) & MDSS_PP_LOGICAL_MASK)


struct mdp_qseed_cfg {
	uint32_t table_num;
	uint32_t ops;
	uint32_t len;
	uint32_t *data;
};

struct mdp_sharp_cfg {
	uint32_t flags;
	uint32_t strength;
	uint32_t edge_thr;
	uint32_t smooth_thr;
	uint32_t noise_thr;
};

struct mdp_qseed_cfg_data {
	uint32_t block;
	struct mdp_qseed_cfg qseed_data;
};

#define MDP_OVERLAY_PP_CSC_CFG         0x1
#define MDP_OVERLAY_PP_QSEED_CFG       0x2
#define MDP_OVERLAY_PP_PA_CFG          0x4
#define MDP_OVERLAY_PP_IGC_CFG         0x8
#define MDP_OVERLAY_PP_SHARP_CFG       0x10
#define MDP_OVERLAY_PP_HIST_CFG        0x20
#define MDP_OVERLAY_PP_HIST_LUT_CFG    0x40
#define MDP_OVERLAY_PP_PA_V2_CFG       0x80

#define MDP_CSC_FLAG_ENABLE	0x1
#define MDP_CSC_FLAG_YUV_IN	0x2
#define MDP_CSC_FLAG_YUV_OUT	0x4

#define MDP_CSC_MATRIX_COEFF_SIZE	9
#define MDP_CSC_CLAMP_SIZE		6
#define MDP_CSC_BIAS_SIZE		3

struct mdp_csc_cfg {
	/* flags for enable CSC, toggling RGB,YUV input/output */
	uint32_t flags;
	uint32_t csc_mv[MDP_CSC_MATRIX_COEFF_SIZE];
	uint32_t csc_pre_bv[MDP_CSC_BIAS_SIZE];
	uint32_t csc_post_bv[MDP_CSC_BIAS_SIZE];
	uint32_t csc_pre_lv[MDP_CSC_CLAMP_SIZE];
	uint32_t csc_post_lv[MDP_CSC_CLAMP_SIZE];
};

struct mdp_csc_cfg_v1_7 {
	uint32_t csc_mv[MDP_CSC_MATRIX_COEFF_SIZE];
	uint32_t csc_pre_bv[MDP_CSC_BIAS_SIZE];
	uint32_t csc_post_bv[MDP_CSC_BIAS_SIZE];
	uint32_t csc_pre_lv[MDP_CSC_CLAMP_SIZE];
	uint32_t csc_post_lv[MDP_CSC_CLAMP_SIZE];
};

struct mdp_csc_cfg_data {
	uint32_t block;
	uint32_t version;
	uint32_t flags;
	struct mdp_csc_cfg csc_data;
	void *cfg_payload;
};

struct mdp_pa_cfg {
	uint32_t flags;
	uint32_t hue_adj;
	uint32_t sat_adj;
	uint32_t val_adj;
	uint32_t cont_adj;
};

struct mdp_pa_mem_col_cfg {
	uint32_t color_adjust_p0;
	uint32_t color_adjust_p1;
	uint32_t hue_region;
	uint32_t sat_region;
	uint32_t val_region;
};

struct mdp_pa_mem_col_cfg_v1_7 {
	uint32_t color_adjust_p0;
	uint32_t color_adjust_p1;
	uint32_t color_adjust_p2;
	uint32_t blend_gain;
	uint32_t hue_region;
	uint32_t sat_region;
	uint32_t val_region;
};

#define MDP_SIX_ZONE_LUT_SIZE		384

struct mdp_pa_v2_data {
	/* Mask bits for PA features */
	uint32_t flags;
	uint32_t global_hue_adj;
	uint32_t global_sat_adj;
	uint32_t global_val_adj;
	uint32_t global_cont_adj;
	struct mdp_pa_mem_col_cfg skin_cfg;
	struct mdp_pa_mem_col_cfg sky_cfg;
	struct mdp_pa_mem_col_cfg fol_cfg;
	uint32_t six_zone_len;
	uint32_t six_zone_thresh;
	uint32_t *six_zone_curve_p0;
	uint32_t *six_zone_curve_p1;
};

struct mdp_pa_data_v1_7 {
	uint32_t global_hue_adj;
	uint32_t global_sat_adj;
	uint32_t global_val_adj;
	uint32_t global_cont_adj;
	struct mdp_pa_mem_col_cfg_v1_7 skin_cfg;
	struct mdp_pa_mem_col_cfg_v1_7 sky_cfg;
	struct mdp_pa_mem_col_cfg_v1_7 fol_cfg;
	uint32_t six_zone_len;
	uint32_t six_zone_thresh;
	uint32_t six_zone_adj_p0;
	uint32_t six_zone_adj_p1;
	uint32_t *six_zone_curve_p0;
	uint32_t *six_zone_curve_p1;
};

struct mdp_pa_v2_cfg_data {
	uint32_t version;
	uint32_t block;
	uint32_t flags;
	uint32_t mode;
	struct mdp_pa_v2_data pa_v2_data;
	void *cfg_payload;
};

enum {
	mdp_igc_v1_7 = 1,
	mdp_igc_vmax,
};

enum {
	mdp_igc_rec601,
	mdp_igc_rec709,
	mdp_igc_custom,
	mdp_igc_mode_max,
};

struct mdp_igc_lut_data {
	uint32_t block;
	uint32_t version;
	uint32_t len, ops;
	uint32_t *c0_c1_data;
	uint32_t *c2_data;
	void *cfg_payload;
};

struct mdp_igc_lut_data_v1_7 {
	uint32_t table_fmt;
	uint32_t len;
	uint32_t *c0_c1_data;
	uint32_t *c2_data;
};

struct mdp_histogram_cfg {
	uint32_t ops;
	uint32_t block;
	uint8_t frame_cnt;
	uint8_t bit_mask;
	uint16_t num_bins;
};

struct mdp_hist_lut_data_v1_7 {
	uint32_t len;
	uint32_t *data;
};

struct mdp_hist_lut_data {
	uint32_t block;
	uint32_t version;
	uint32_t ops;
	uint32_t len;
	uint32_t *data;
	void *cfg_payload;
};

struct mdp_overlay_pp_params {
	uint32_t config_ops;
	struct mdp_csc_cfg csc_cfg;
	struct mdp_qseed_cfg qseed_cfg[2];
	struct mdp_pa_cfg pa_cfg;
	struct mdp_pa_v2_data pa_v2_cfg;
	struct mdp_igc_lut_data igc_cfg;
	struct mdp_sharp_cfg sharp_cfg;
	struct mdp_histogram_cfg hist_cfg;
	struct mdp_hist_lut_data hist_lut_cfg;
};

/**
 * enum mdss_mdp_blend_op - Different blend operations set by userspace
 *
 * @BLEND_OP_NOT_DEFINED:    No blend operation defined for the layer.
 * @BLEND_OP_OPAQUE:         Apply a constant blend operation. The layer
 *                           would appear opaque in case fg plane alpha is
 *                           0xff.
 * @BLEND_OP_PREMULTIPLIED:  Apply source over blend rule. Layer already has
 *                           alpha pre-multiplication done. If fg plane alpha
 *                           is less than 0xff, apply modulation as well. This
 *                           operation is intended on layers having alpha
 *                           channel.
 * @BLEND_OP_COVERAGE:       Apply source over blend rule. Layer is not alpha
 *                           pre-multiplied. Apply pre-multiplication. If fg
 *                           plane alpha is less than 0xff, apply modulation as
 *                           well.
 * @BLEND_OP_MAX:            Used to track maximum blend operation possible by
 *                           mdp.
 */
enum mdss_mdp_blend_op {
	BLEND_OP_NOT_DEFINED = 0,
	BLEND_OP_OPAQUE,
	BLEND_OP_PREMULTIPLIED,
	BLEND_OP_COVERAGE,
	BLEND_OP_MAX,
};

#define MAX_PLANES	4
struct mdp_scale_data {
	uint8_t enable_pxl_ext;

	int init_phase_x[MAX_PLANES];
	int phase_step_x[MAX_PLANES];
	int init_phase_y[MAX_PLANES];
	int phase_step_y[MAX_PLANES];

	int num_ext_pxls_left[MAX_PLANES];
	int num_ext_pxls_right[MAX_PLANES];
	int num_ext_pxls_top[MAX_PLANES];
	int num_ext_pxls_btm[MAX_PLANES];

	int left_ftch[MAX_PLANES];
	int left_rpt[MAX_PLANES];
	int right_ftch[MAX_PLANES];
	int right_rpt[MAX_PLANES];

	int top_rpt[MAX_PLANES];
	int btm_rpt[MAX_PLANES];
	int top_ftch[MAX_PLANES];
	int btm_ftch[MAX_PLANES];

	uint32_t roi_w[MAX_PLANES];
};

/**
 * enum mdp_overlay_pipe_type - Different pipe type set by userspace
 *
 * @PIPE_TYPE_AUTO:    Not specified, pipe will be selected according to flags.
 * @PIPE_TYPE_VIG:     VIG pipe.
 * @PIPE_TYPE_RGB:     RGB pipe.
 * @PIPE_TYPE_DMA:     DMA pipe.
 * @PIPE_TYPE_CURSOR:  CURSOR pipe.
 * @PIPE_TYPE_MAX:     Used to track maximum number of pipe type.
 */
enum mdp_overlay_pipe_type {
	PIPE_TYPE_AUTO = 0,
	PIPE_TYPE_VIG,
	PIPE_TYPE_RGB,
	PIPE_TYPE_DMA,
	PIPE_TYPE_CURSOR,
	PIPE_TYPE_MAX,
};

/**
 * struct mdp_overlay - overlay surface structure
 * @src:	Source image information (width, height, format).
 * @src_rect:	Source crop rectangle, portion of image that will be fetched.
 *		This should always be within boundaries of source image.
 * @dst_rect:	Destination rectangle, the position and size of image on screen.
 *		This should always be within panel boundaries.
 * @z_order:	Blending stage to occupy in display, if multiple layers are
 *		present, highest z_order usually means the top most visible
 *		layer. The range acceptable is from 0-3 to support blending
 *		up to 4 layers.
 * @is_fg:	This flag is used to disable blending of any layers with z_order
 *		less than this overlay. It means that any layers with z_order
 *		less than this layer will not be blended and will be replaced
 *		by the background border color.
 * @alpha:	Used to set plane opacity. The range can be from 0-255, where
 *		0 means completely transparent and 255 means fully opaque.
 * @transp_mask: Color used as color key for transparency. Any pixel in fetched
 *		image matching this color will be transparent when blending.
 *		The color should be in same format as the source image format.
 * @flags:	This is used to customize operation of overlay. See MDP flags
 *		for more information.
 * @pipe_type:  Used to specify the type of overlay pipe.
 * @user_data:	DEPRECATED* Used to store user application specific information.
 * @bg_color:	Solid color used to fill the overlay surface when no source
 *		buffer is provided.
 * @horz_deci:	Horizontal decimation value, this indicates the amount of pixels
 *		dropped for each pixel that is fetched from a line. The value
 *		given should be power of two of decimation amount.
 *		0: no decimation
 *		1: decimate by 2 (drop 1 pixel for each pixel fetched)
 *		2: decimate by 4 (drop 3 pixels for each pixel fetched)
 *		3: decimate by 8 (drop 7 pixels for each pixel fetched)
 *		4: decimate by 16 (drop 15 pixels for each pixel fetched)
 * @vert_deci:	Vertical decimation value, this indicates the amount of lines
 *		dropped for each line that is fetched from overlay. The value
 *		given should be power of two of decimation amount.
 *		0: no decimation
 *		1: decimation by 2 (drop 1 line for each line fetched)
 *		2: decimation by 4 (drop 3 lines for each line fetched)
 *		3: decimation by 8 (drop 7 lines for each line fetched)
 *		4: decimation by 16 (drop 15 lines for each line fetched)
 * @overlay_pp_cfg: Overlay post processing configuration, for more information
 *		see struct mdp_overlay_pp_params.
 * @priority:	Priority is returned by the driver when overlay is set for the
 *		first time. It indicates the priority of the underlying pipe
 *		serving the overlay. This priority can be used by user-space
 *		in source split when pipes are re-used and shuffled around to
 *		reduce fallbacks.
 */
struct mdp_overlay {
	struct msmfb_img src;
	struct mdp_rect src_rect;
	struct mdp_rect dst_rect;
	uint32_t z_order;	/* stage number */
	uint32_t is_fg;		/* control alpha & transp */
	uint32_t alpha;
	uint32_t blend_op;
	uint32_t transp_mask;
	uint32_t flags;
	uint32_t pipe_type;
	uint32_t id;
	uint8_t priority;
	uint32_t user_data[6];
	uint32_t bg_color;
	uint8_t horz_deci;
	uint8_t vert_deci;
	struct mdp_overlay_pp_params overlay_pp_cfg;
	struct mdp_scale_data scale;
	uint8_t color_space;
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

#define MISR_CRC_BATCH_SIZE 32
enum {
	DISPLAY_MISR_EDP,
	DISPLAY_MISR_DSI0,
	DISPLAY_MISR_DSI1,
	DISPLAY_MISR_HDMI,
	DISPLAY_MISR_LCDC,
	DISPLAY_MISR_MDP,
	DISPLAY_MISR_ATV,
	DISPLAY_MISR_DSI_CMD,
	DISPLAY_MISR_MAX
};

enum {
	MISR_OP_NONE,
	MISR_OP_SFM,
	MISR_OP_MFM,
	MISR_OP_BM,
	MISR_OP_MAX
};

struct mdp_misr {
	uint32_t block_id;
	uint32_t frame_count;
	uint32_t crc_op_mode;
	uint32_t crc_value[MISR_CRC_BATCH_SIZE];
};

/*

	mdp_block_type defines the identifiers for pipes in MDP 4.3 and up

	MDP_BLOCK_RESERVED is provided for backward compatibility and is
	deprecated. It corresponds to DMA_P. So MDP_BLOCK_DMA_P should be used
	instead.

	MDP_LOGICAL_BLOCK_DISP_0 identifies the display pipe which fb0 uses,
	same for others.

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
	MDP_BLOCK_OVERLAY_2,
	MDP_LOGICAL_BLOCK_DISP_0 = 0x10,
	MDP_LOGICAL_BLOCK_DISP_1,
	MDP_LOGICAL_BLOCK_DISP_2,
	MDP_BLOCK_MAX,
};

/*
 * mdp_histogram_start_req is used to provide the parameters for
 * histogram start request
 */

struct mdp_histogram_start_req {
	uint32_t block;
	uint8_t frame_cnt;
	uint8_t bit_mask;
	uint16_t num_bins;
};

/*
 * mdp_histogram_data is used to return the histogram data, once
 * the histogram is done/stopped/cance
 */

struct mdp_histogram_data {
	uint32_t block;
	uint32_t bin_cnt;
	uint32_t *c0;
	uint32_t *c1;
	uint32_t *c2;
	uint32_t *extra_info;
};

enum {
	mdp_pcc_v1_7 = 0x1,
	mdp_pcc_vmax,
};

struct mdp_pcc_coeff {
	uint32_t c, r, g, b, rr, gg, bb, rg, gb, rb, rgb_0, rgb_1;
};

struct mdp_pcc_coeff_v1_7 {
	uint32_t c, r, g, b, rg, gb, rb, rgb;
};

struct mdp_pcc_data_v1_7 {
	struct mdp_pcc_coeff_v1_7 r, g, b;
};

struct mdp_pcc_cfg_data {
	uint32_t version;
	uint32_t block;
	uint32_t ops;
	struct mdp_pcc_coeff r, g, b;
	void *cfg_payload;
};

enum {
	mdp_lut_igc,
	mdp_lut_pgc,
	mdp_lut_hist,
	mdp_lut_max,
};

#define GC_LUT_ENTRIES_V1_7	512

struct mdp_ar_gc_lut_data {
	uint32_t x_start;
	uint32_t slope;
	uint32_t offset;
};

struct mdp_pgc_lut_data {
	uint32_t version;
	uint32_t block;
	uint32_t flags;
	uint8_t num_r_stages;
	uint8_t num_g_stages;
	uint8_t num_b_stages;
	struct mdp_ar_gc_lut_data *r_data;
	struct mdp_ar_gc_lut_data *g_data;
	struct mdp_ar_gc_lut_data *b_data;
	void *cfg_payload;
};

struct mdp_pgc_lut_data_v1_7 {
	uint32_t  len;
	uint32_t  *c0_data;
	uint32_t  *c1_data;
	uint32_t  *c2_data;
};

struct mdp_lut_cfg_data {
	uint32_t lut_type;
	union {
		struct mdp_igc_lut_data igc_lut_data;
		struct mdp_pgc_lut_data pgc_lut_data;
		struct mdp_hist_lut_data hist_lut_data;
	} data;
};

struct mdp_bl_scale_data {
	uint32_t min_lvl;
	uint32_t scale;
};

struct mdp_pa_cfg_data {
	uint32_t block;
	struct mdp_pa_cfg pa_data;
};

struct mdp_dither_data_v1_7 {
	uint32_t g_y_depth;
	uint32_t r_cr_depth;
	uint32_t b_cb_depth;
};

struct mdp_dither_cfg_data {
	uint32_t version;
	uint32_t block;
	uint32_t flags;
	uint32_t mode;
	uint32_t g_y_depth;
	uint32_t r_cr_depth;
	uint32_t b_cb_depth;
	void *cfg_payload;
};

#define MDP_GAMUT_TABLE_NUM		8
#define MDP_GAMUT_TABLE_NUM_V1_7	4
#define MDP_GAMUT_SCALE_OFF_TABLE_NUM	3
#define MDP_GAMUT_TABLE_V1_7_SZ 1228
#define MDP_GAMUT_SCALE_OFF_SZ 16
#define MDP_GAMUT_TABLE_V1_7_COARSE_SZ 32

enum {
	mdp_gamut_v1_7 = 1,
	mdp_gamut_vmax,
};


struct mdp_gamut_cfg_data {
	uint32_t block;
	uint32_t flags;
	uint32_t version;
	/* v1 version specific params */
	uint32_t gamut_first;
	uint32_t tbl_size[MDP_GAMUT_TABLE_NUM];
	uint16_t *r_tbl[MDP_GAMUT_TABLE_NUM];
	uint16_t *g_tbl[MDP_GAMUT_TABLE_NUM];
	uint16_t *b_tbl[MDP_GAMUT_TABLE_NUM];
	/* params for newer versions of gamut */
	void *cfg_payload;
};

enum {
	mdp_gamut_fine_mode = 0x1,
	mdp_gamut_coarse_mode,
};

struct mdp_gamut_data_v1_7 {
	uint32_t mode;
	uint32_t tbl_size[MDP_GAMUT_TABLE_NUM_V1_7];
	uint32_t *c0_data[MDP_GAMUT_TABLE_NUM_V1_7];
	uint32_t *c1_c2_data[MDP_GAMUT_TABLE_NUM_V1_7];
	uint32_t  tbl_scale_off_sz[MDP_GAMUT_SCALE_OFF_TABLE_NUM];
	uint32_t  *scale_off_data[MDP_GAMUT_SCALE_OFF_TABLE_NUM];
};

struct mdp_calib_config_data {
	uint32_t ops;
	uint32_t addr;
	uint32_t data;
};

struct mdp_calib_config_buffer {
	uint32_t ops;
	uint32_t size;
	uint32_t *buffer;
};

struct mdp_calib_dcm_state {
	uint32_t ops;
	uint32_t dcm_state;
};

struct mdp_pp_init_data {
	uint32_t init_request;
};

enum {
	MDP_PP_DISABLE,
	MDP_PP_ENABLE,
};

enum {
	DCM_UNINIT,
	DCM_UNBLANK,
	DCM_ENTER,
	DCM_EXIT,
	DCM_BLANK,
	DTM_ENTER,
	DTM_EXIT,
};

#define MDSS_PP_SPLIT_LEFT_ONLY		0x10000000
#define MDSS_PP_SPLIT_RIGHT_ONLY	0x20000000
#define MDSS_PP_SPLIT_MASK		0x30000000

#define MDSS_MAX_BL_BRIGHTNESS 255
#define AD_BL_LIN_LEN 256
#define AD_BL_ATT_LUT_LEN 33

#define MDSS_AD_MODE_AUTO_BL	0x0
#define MDSS_AD_MODE_AUTO_STR	0x1
#define MDSS_AD_MODE_TARG_STR	0x3
#define MDSS_AD_MODE_MAN_STR	0x7
#define MDSS_AD_MODE_CALIB	0xF

#define MDP_PP_AD_INIT	0x10
#define MDP_PP_AD_CFG	0x20

struct mdss_ad_init {
	uint32_t asym_lut[33];
	uint32_t color_corr_lut[33];
	uint8_t i_control[2];
	uint16_t black_lvl;
	uint16_t white_lvl;
	uint8_t var;
	uint8_t limit_ampl;
	uint8_t i_dither;
	uint8_t slope_max;
	uint8_t slope_min;
	uint8_t dither_ctl;
	uint8_t format;
	uint8_t auto_size;
	uint16_t frame_w;
	uint16_t frame_h;
	uint8_t logo_v;
	uint8_t logo_h;
	uint32_t alpha;
	uint32_t alpha_base;
	uint32_t bl_lin_len;
	uint32_t bl_att_len;
	uint32_t *bl_lin;
	uint32_t *bl_lin_inv;
	uint32_t *bl_att_lut;
};

#define MDSS_AD_BL_CTRL_MODE_EN 1
#define MDSS_AD_BL_CTRL_MODE_DIS 0
struct mdss_ad_cfg {
	uint32_t mode;
	uint32_t al_calib_lut[33];
	uint16_t backlight_min;
	uint16_t backlight_max;
	uint16_t backlight_scale;
	uint16_t amb_light_min;
	uint16_t filter[2];
	uint16_t calib[4];
	uint8_t strength_limit;
	uint8_t t_filter_recursion;
	uint16_t stab_itr;
	uint32_t bl_ctrl_mode;
};

/* ops uses standard MDP_PP_* flags */
struct mdss_ad_init_cfg {
	uint32_t ops;
	union {
		struct mdss_ad_init init;
		struct mdss_ad_cfg cfg;
	} params;
};

/* mode uses MDSS_AD_MODE_* flags */
struct mdss_ad_input {
	uint32_t mode;
	union {
		uint32_t amb_light;
		uint32_t strength;
		uint32_t calib_bl;
	} in;
	uint32_t output;
};

#define MDSS_CALIB_MODE_BL	0x1
struct mdss_calib_cfg {
	uint32_t ops;
	uint32_t calib_mask;
};

enum {
	mdp_op_pcc_cfg,
	mdp_op_csc_cfg,
	mdp_op_lut_cfg,
	mdp_op_qseed_cfg,
	mdp_bl_scale_cfg,
	mdp_op_pa_cfg,
	mdp_op_pa_v2_cfg,
	mdp_op_dither_cfg,
	mdp_op_gamut_cfg,
	mdp_op_calib_cfg,
	mdp_op_ad_cfg,
	mdp_op_ad_input,
	mdp_op_calib_mode,
	mdp_op_calib_buffer,
	mdp_op_calib_dcm_state,
	mdp_op_max,
	mdp_op_pp_init_cfg,
};

enum {
	WB_FORMAT_NV12,
	WB_FORMAT_RGB_565,
	WB_FORMAT_RGB_888,
	WB_FORMAT_xRGB_8888,
	WB_FORMAT_ARGB_8888,
	WB_FORMAT_BGRA_8888,
	WB_FORMAT_BGRX_8888,
	WB_FORMAT_ARGB_8888_INPUT_ALPHA /* Need to support */
};

struct msmfb_mdp_pp {
	uint32_t op;
	union {
		struct mdp_pcc_cfg_data pcc_cfg_data;
		struct mdp_csc_cfg_data csc_cfg_data;
		struct mdp_lut_cfg_data lut_cfg_data;
		struct mdp_qseed_cfg_data qseed_cfg_data;
		struct mdp_bl_scale_data bl_scale_data;
		struct mdp_pa_cfg_data pa_cfg_data;
		struct mdp_pa_v2_cfg_data pa_v2_cfg_data;
		struct mdp_dither_cfg_data dither_cfg_data;
		struct mdp_gamut_cfg_data gamut_cfg_data;
		struct mdp_calib_config_data calib_cfg;
		struct mdss_ad_init_cfg ad_init_cfg;
		struct mdss_calib_cfg mdss_calib_cfg;
		struct mdss_ad_input ad_input;
		struct mdp_calib_config_buffer calib_buffer;
		struct mdp_calib_dcm_state calib_dcm;
		struct mdp_pp_init_data init_data;
	} data;
};

#define FB_METADATA_VIDEO_INFO_CODE_SUPPORT 1
enum {
	metadata_op_none,
	metadata_op_base_blend,
	metadata_op_frame_rate,
	metadata_op_vic,
	metadata_op_wb_format,
	metadata_op_wb_secure,
	metadata_op_get_caps,
	metadata_op_crc,
	metadata_op_get_ion_fd,
	metadata_op_max
};

struct mdp_blend_cfg {
	uint32_t is_premultiplied;
};

struct mdp_mixer_cfg {
	uint32_t writeback_format;
	uint32_t alpha;
};

struct mdss_hw_caps {
	uint32_t mdp_rev;
	uint8_t rgb_pipes;
	uint8_t vig_pipes;
	uint8_t dma_pipes;
	uint8_t max_smp_cnt;
	uint8_t smp_per_pipe;
	uint32_t features;
};

struct msmfb_metadata {
	uint32_t op;
	uint32_t flags;
	union {
		struct mdp_misr misr_request;
		struct mdp_blend_cfg blend_cfg;
		struct mdp_mixer_cfg mixer_cfg;
		uint32_t panel_frame_rate;
		uint32_t video_info_code;
		struct mdss_hw_caps caps;
		uint8_t secure_en;
		int fbmem_ionfd;
	} data;
};

#define MDP_MAX_FENCE_FD	32
#define MDP_BUF_SYNC_FLAG_WAIT	1
#define MDP_BUF_SYNC_FLAG_RETIRE_FENCE	0x10

struct mdp_buf_sync {
	uint32_t flags;
	uint32_t acq_fen_fd_cnt;
	uint32_t session_id;
	int *acq_fen_fd;
	int *rel_fen_fd;
	int *retire_fen_fd;
};

struct mdp_async_blit_req_list {
	struct mdp_buf_sync sync;
	uint32_t count;
	struct mdp_blit_req req[];
};

#define MDP_DISPLAY_COMMIT_OVERLAY	1

struct mdp_display_commit {
	uint32_t flags;
	uint32_t wait_for_finish;
	struct fb_var_screeninfo var;
	struct mdp_rect l_roi;
	struct mdp_rect r_roi;
};

/**
 * struct mdp_overlay_list - argument for ioctl MSMFB_OVERLAY_PREPARE
 * @num_overlays:	Number of overlay layers as part of the frame.
 * @overlay_list:	Pointer to a list of overlay structures identifying
 *			the layers as part of the frame
 * @flags:		Flags can be used to extend behavior.
 * @processed_overlays:	Output parameter indicating how many pipes were
 *			successful. If there are no errors this number should
 *			match num_overlays. Otherwise it will indicate the last
 *			successful index for overlay that couldn't be set.
 */
struct mdp_overlay_list {
	uint32_t num_overlays;
	struct mdp_overlay **overlay_list;
	uint32_t flags;
	uint32_t processed_overlays;
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

#define MAX_PIPE_PER_MIXER  7

struct msmfb_mixer_info_req {
	int mixer_num;
	int cnt;
	struct mdp_mixer_info info[MAX_PIPE_PER_MIXER];
};

enum {
	DISPLAY_SUBSYSTEM_ID,
	ROTATOR_SUBSYSTEM_ID,
};

enum {
	MDP_IOMMU_DOMAIN_CP,
	MDP_IOMMU_DOMAIN_NS,
};

enum {
	MDP_WRITEBACK_MIRROR_OFF,
	MDP_WRITEBACK_MIRROR_ON,
	MDP_WRITEBACK_MIRROR_PAUSE,
	MDP_WRITEBACK_MIRROR_RESUME,
};

enum {
	MDP_CSC_ITU_R_601,
	MDP_CSC_ITU_R_601_FR,
	MDP_CSC_ITU_R_709,
};
#endif /*_UAPI_MSM_MDP_H_*/
