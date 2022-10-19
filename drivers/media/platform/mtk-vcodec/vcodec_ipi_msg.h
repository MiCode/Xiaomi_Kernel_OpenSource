/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 */

#ifndef _VCODEC_IPI_MSG_H_
#define _VCODEC_IPI_MSG_H_

#include <linux/videodev2.h>
#include <linux/v4l2-controls.h>

#ifdef VIDEO_MAX_FRAME
#undef VIDEO_MAX_FRAME
#define VIDEO_MAX_FRAME 64
#endif

#define AP_IPIMSG_VDEC_SEND_BASE 0xA000
#define VCU_IPIMSG_VDEC_ACK_BASE 0xB000
#define VCU_IPIMSG_VDEC_SEND_BASE 0xC000
#define AP_IPIMSG_VDEC_ACK_BASE 0xD000

#define AP_IPIMSG_VENC_SEND_BASE 0x1000
#define VCU_IPIMSG_VENC_ACK_BASE 0x2000
#define VCU_IPIMSG_VENC_SEND_BASE 0x3000
#define AP_IPIMSG_VENC_ACK_BASE 0x4000

enum mtk_venc_hw_id {
	MTK_VENC_CORE_0 = 0,
	MTK_VENC_CORE_1 = 1,
	MTK_VENC_CORE_2 = 2,
	MTK_VENC_HW_NUM = 3,
};

enum mtk_vdec_hw_id {
	MTK_VDEC_CORE = 0,
	MTK_VDEC_LAT = 1,
	MTK_VDEC_CORE1 = 2,
	MTK_VDEC_LAT1 = 3,
	MTK_VDEC_LINE_COUNT = 4,
	MTK_VDEC_HW_NUM = 5,
};

enum mtk_fmt_type {
	MTK_FMT_DEC = 0,
	MTK_FMT_ENC = 1,
	MTK_FMT_FRAME = 2,
};

enum mtk_frame_type {
	MTK_FRAME_NONE = 0,
	MTK_FRAME_I = 1,
	MTK_FRAME_P = 2,
	MTK_FRAME_B = 3,
};

enum v4l2_vdec_trick_mode {
	/* decode all frame */
	V4L2_VDEC_TRICK_MODE_ALL = 0,
	/* decode all except of non-reference frame */
	V4L2_VDEC_TRICK_MODE_IP,
	/* only decode I frame */
	V4L2_VDEC_TRICK_MODE_I
};

/**
 * struct mtk_video_fmt - Structure used to store information about pixelformats
 */
struct mtk_video_fmt {
	__u32	fourcc;
	__u32	type;   /* enum mtk_fmt_type */
	__u32	num_planes;
	__u32	reserved;
};

/**
 * struct mtk_codec_framesizes - Structure used to store information about
 *							framesizes
 */
struct mtk_codec_framesizes {
	__u32	fourcc;
	__u32	profile;
	__u32	level;
	__u32	reserved;
	struct	v4l2_frmsize_stepwise	stepwise;
};

/**
 * struct mtk_video_frame_frameintervals - Structure used to store information about
 *							frameintervals
 * fourcc/width/height are input parameters
 * stepwise is output parameter
 */
struct mtk_video_frame_frameintervals {
	__u32   fourcc;
	__u32   width;
	__u32   height;
	struct v4l2_frmival_stepwise stepwise;
};

struct mtk_color_desc {
	__u32	color_primaries;
	__u32	transform_character;
	__u32	matrix_coeffs;
	__u32	display_primaries_x[3];
	__u32	display_primaries_y[3];
	__u32	white_point_x;
	__u32	white_point_y;
	__u32	max_display_mastering_luminance;
	__u32	min_display_mastering_luminance;
	__u32	max_content_light_level;
	__u32	max_pic_light_level;
	__u32	is_hdr;
	__u32	full_range;
	__u32	reserved;
};

struct v4l2_vdec_hdr10_info {
	__u8 matrix_coefficients;
	__u8 bits_per_channel;
	__u8 chroma_subsampling_horz;
	__u8 chroma_subsampling_vert;
	__u8 cb_subsampling_horz;
	__u8 cb_subsampling_vert;
	__u8 chroma_siting_horz;
	__u8 chroma_siting_vert;
	__u8 color_range;
	__u8 transfer_characteristics;
	__u8 colour_primaries;
	__u16 max_CLL;  // CLL: Content Light Level
	__u16 max_FALL; // FALL: Frame Average Light Level
	__u16 primaries[3][2];
	__u16 white_point[2];
	__u32 max_luminance;
	__u32 min_luminance;
};

struct v4l2_vdec_hdr10plus_data {
	__u64 addr; // user pointer
	__u32 size;
};

struct mtk_venc_multi_ref {
	__u32	multi_ref_en;
	__u32	intra_period;
	__u32	superp_period;
	__u32	superp_ref_type;
	__u32	ref0_distance;
	__u32	ref1_dsitance;
	__u32	max_distance;
	__u32	reserved;
};

struct mtk_venc_vui_info {
	__u32	aspect_ratio_idc;
	__u32	sar_width;
	__u32	sar_height;
	__u32	reserved;
};

struct mtk_hdr_dynamic_info {
	__u32    max_sc_lR;
		// u(17); Max R Nits *10; in the range of 0x00000-0x186A0
	__u32    max_sc_lG;
		// u(17); Max G Nits *10; in the range of 0x00000-0x186A0
	__u32    max_sc_lB;
		// u(17); Max B Nits *10; in the range of 0x00000-0x186A0
	__u32    avg_max_rgb;
		// u(17); Average maxRGB Nits *10; in 0x00000-0x186A0
	__u32    distribution_values[9];
		/* u(17)
		 * 0=1% percentile maxRGB Nits *10
		 * 1=Maximum Nits of 99YF *10
		 * 2=Average Nits of DPY100F
		 * 3=25% percentile maxRGB Nits *10
		 * 4=50% percentile maxRGB Nits *10
		 * 5=75% percentile maxRGB Nits *10
		 * 6=90% percentile maxRGB Nits *10
		 * 7=95% percentile maxRGB Nits *10
		 * 8=99.95% percentile maxRGB Nits *10
		 */
	__u32   reserved;
};

struct dynamicinfo_change_flag {
	__u32 fgBitrate;
	__u32 fgFrameQp;
	__u32 fgSliceHeaderSpacing;
	__u32 fgForceI;
	__u32 fgBaseLayerPid;
	__u32 fgMarkLTR;
	__u32 fgUseLTR;
	__u32 fgTemporalLayerCount;
};

struct temporal_layer_count {
	__u32 nPLayerCountActual;
	__u32 nBLayerCountActual;
};

struct inputqueue_dynamic_info {
	struct dynamicinfo_change_flag changed;
	__u64 nTimeStamp;
	__u32 nBitrate;
	__u32 nFrameQp;
	__u32 bSliceHeaderSpacing;
	__u32 bForceI;
	__u32 nBaseLayerPid;
	__u32 nMarkLTR;
	__u32 nUseLTR;
	__u32 reserved;
	struct temporal_layer_count sTemporalLayerCount;
};


/**
 * struct vdec_pic_info  - picture size information
 * @pic_w: picture width
 * @pic_h: picture height
 * @buf_w   : picture buffer width (codec aligned up from pic_w)
 * @buf_h   : picture buffer heiht (codec aligned up from pic_h)
 * @fb_sz: frame buffer size
 * @bitdepth: Sequence bitdepth
 * @layout_mode: mediatek frame layout mode
 * @fourcc: frame buffer color format
 * @field: enum v4l2_field, field type of this sequence
 * E.g. suppose picture size is 176x144,
 *      buffer size will be aligned to 176x160.
 */
struct vdec_pic_info {
	__u32 pic_w;
	__u32 pic_h;
	__u32 buf_w;
	__u32 buf_h;
	__u32 fb_sz[VIDEO_MAX_PLANES];
	__u32 bitdepth;
	__u32 layout_mode;
	__u32 fourcc;
	__u32 field;
};

/**
 * struct vdec_dec_info - decode information
 * @dpb_sz		: decoding picture buffer size
 * @vdec_changed_info  : some changed flags
 * @bs_dma		: Input bit-stream buffer dma address
 * @bs_fd               : Input bit-stream buffer dmabuf fd
 * @fb_dma		: Y frame buffer dma address
 * @fb_fd             : Y frame buffer dmabuf fd
 * @vdec_bs_va		: VDEC bitstream buffer struct virtual address
 * @vdec_fb_va		: VDEC frame buffer struct virtual address
 * @fb_num_planes	: frame buffer plane count
 * @reserved		: reserved variable for 64bit align
 */
struct vdec_dec_info {
	__u32 dpb_sz;
	__u32 vdec_changed_info;
	__u64 bs_dma;
	__u64 bs_fd;
	__u64 fb_dma[VIDEO_MAX_PLANES];
	__u64 fb_fd[VIDEO_MAX_PLANES];
	__u64 vdec_bs_va;
	__u64 vdec_fb_va;
	__u32 fb_num_planes;
	__u32 index;
	__u32 wait_key_frame;
	__u32 error_map;
	__u64 timestamp;
	__u32 queued_frame_buf_count;
	__u32 vpeek;
};

#define HDR10_PLUS_MAX_SIZE              (128)

struct hdr10plus_info {
	__u8 data[HDR10_PLUS_MAX_SIZE];
	__u32 size;
};

enum vcodec_mem_type {
	MEM_TYPE_FOR_SW = 0,                    /* /< External memory for SW */
	MEM_TYPE_FOR_HW,                        /* /< External memory for HW  */
	MEM_TYPE_FOR_UBE_HW,                    /* /< External memory for UBE reserved memory */
	MEM_TYPE_FOR_SEC_SW,                    /* /< External memory for secure SW */
	MEM_TYPE_FOR_SEC_HW,                    /* /< External memory for secure HW */
	MEM_TYPE_FOR_SEC_UBE_HW,                /* /< External memory for secure UBE */
	MEM_TYPE_FOR_SHM,                       /* /< External memory for share memory */
	MEM_TYPE_FOR_SEC_WFD_HW,                /* /< External memory for secure WFD */
	MEM_TYPE_MAX = 0xFFFFFFFF               /* /< Max memory type */
};

/**
 * struct mem_obj - memory buffer allocated in kernel
 *
 * @flag:	flag of buffer
 * @iova:	iova of buffer
 * @len:	buffer length
 * @pa:	physical address
 * @va: kernel virtual address
 */
struct vcodec_mem_obj {
	__u32 type;
	__u32 len;
	__u64 iova;
	__u64 pa;
	__u64 va;
};

#endif
