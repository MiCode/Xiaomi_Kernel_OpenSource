/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 */

#ifndef _HEADER_DESC_
#define _HEADER_DESC_

/* TBD */
#include <linux/videodev2.h>
/**/

enum imgsysrotation {
	imgsysrotation_0      = 0, //
	imgsysrotation_90,         //90 CW
	imgsysrotation_180,
	imgsysrotation_270
};

enum imgsysflip {
	imgsysflip_off     = 0, //
	imgsysflip_on      = 1, //
};

enum img_resize_ratio {
	img_resize_anyratio,
	img_resize_down4,
	img_resize_down2,
	img_resize_down42,
	img_resiz_max
};
#define COMPACT_USE
struct v4l2_ext_plane {
#ifndef COMPACT_USE
	__u32			bytesused;
	__u32			length;
#endif
	union {
#ifndef COMPACT_USE
		__u32		mem_offset;
		__u64		userptr;
#endif
		struct {
			__s32		fd;
			__u32		offset;
		} dma_buf;
	} m;
#ifndef COMPACT_USE
	__u32			data_offset;
	__u32			reserved[11];
#else
	__u64			reserved[2];
#endif
};
#define IMGBUF_MAX_PLANES (3)
struct v4l2_ext_buffer {
#ifndef COMPACT_USE
	__u32			index;
	__u32			type;
	__u32			flags;
	__u32			field;
	__u64			timestamp;
	__u32			sequence;
	__u32			memory;
#endif
	struct v4l2_ext_plane	planes[IMGBUF_MAX_PLANES];
	__u32			num_planes;
#ifndef COMPACT_USE
	__u32			reserved[11];
#else
	__u64			reserved[2];
#endif
};

struct mtk_imgsys_crop {
	struct v4l2_rect	c;
	struct v4l2_fract	left_subpix;
	struct v4l2_fract	top_subpix;
	struct v4l2_fract	width_subpix;
	struct v4l2_fract	height_subpix;
};

struct plane_pix_format {
	__u32		sizeimage;
	__u32		bytesperline;
} __packed;


struct pix_format_mplane {
	__u32				width;
	__u32				height;
	__u32				pixelformat;
	struct plane_pix_format	plane_fmt[IMGBUF_MAX_PLANES];
} __packed;


struct buf_format {
	union {
		struct pix_format_mplane	pix_mp;  /* V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE */
	} fmt;
};

struct buf_info {
	struct v4l2_ext_buffer buf;
	struct buf_format fmt;
	struct mtk_imgsys_crop crop;
	/* struct v4l2_rect compose; */
	__u32 rotation;
	__u32 hflip;
	__u32 vflip;
	__u8  resizeratio;
	__u32  secu;
};

#define FRAME_BUF_MAX (1)
struct frameparams {
	struct buf_info bufs[FRAME_BUF_MAX];
};

#define SCALE_MAX (1)
#define TIME_MAX (192)

struct header_desc {
	__u32 fparams_tnum;
	struct frameparams fparams[TIME_MAX][SCALE_MAX];
};

#define TMAX (16)
struct header_desc_norm {
	__u32 fparams_tnum;
	struct frameparams fparams[TMAX][SCALE_MAX];
};

#define IMG_MAX_HW_INPUTS	3

#define IMG_MAX_HW_OUTPUTS	4
/* TODO */
#define IMG_MAX_HW_DMAS		72
struct singlenode_desc {
	__u8 dmas_enable[IMG_MAX_HW_DMAS][TIME_MAX];
	struct header_desc	dmas[IMG_MAX_HW_DMAS];
	struct header_desc	tuning_meta;
	struct header_desc	ctrl_meta;
};

struct singlenode_desc_norm {
	__u8 dmas_enable[IMG_MAX_HW_DMAS][TMAX];
	struct header_desc_norm	dmas[IMG_MAX_HW_DMAS];
	struct header_desc_norm	tuning_meta;
	struct header_desc_norm	ctrl_meta;
};

#define V4L2_META_FMT_MTISP_DESC   v4l2_fourcc('M', 'T', 'f', 'd')
	/* ISP description fmt*/
#define V4L2_META_FMT_MTISP_SD   v4l2_fourcc('M', 'T', 'f', 's')
	/* ISP singledevice fmt*/
#define V4L2_META_FMT_MTISP_DESC_NORM   v4l2_fourcc('M', 'T', 'f', 'r')
	/* ISP SMVR DESC fmt*/
#define V4L2_META_FMT_MTISP_SDNORM   v4l2_fourcc('M', 'T', 's', 'r')
	/* ISP SMVR SD fmt*/
#define V4L2_PIX_FMT_WARP2P      v4l2_fourcc('M', 'W', '2', 'P')
	/* Mediatek warp map 32-bit, 2 plane */
#define V4L2_PIX_FMT_YUV422      v4l2_fourcc('Y', 'U', '1', '6')
	/* YUV-8bit packed 4:2:2 3plane */
#define V4L2_PIX_FMT_YUYV_Y210P    v4l2_fourcc('Y', 'U', '1', 'A')
	/* YUV-10bit packed 4:2:2 1plane, YUYV  */
#define V4L2_PIX_FMT_YVYU_Y210P    v4l2_fourcc('Y', 'V', '1', 'A')
	/* YUV-10bit packed 4:2:2 1plane, YVYU  */
#define V4L2_PIX_FMT_UYVY_Y210P    v4l2_fourcc('U', 'Y', '1', 'A')
	/* YUV-10bit packed 4:2:2 1plane, UYVY  */
#define V4L2_PIX_FMT_VYUY_Y210P    v4l2_fourcc('V', 'Y', '1', 'A')
	/* YUV-10bit packed 4:2:2 1plane, VYUY  */
#define V4L2_PIX_FMT_YUV_2P210P    v4l2_fourcc('U', '2', '2', 'A')
	/* YUV-10bit packed 4:2:2 2plane, (Y)(UV)  */
#define V4L2_PIX_FMT_YVU_2P210P    v4l2_fourcc('V', '2', '2', 'A')
	/* YUV-10bit packed 4:2:2 2plane, (Y)(VU)  */
#define V4L2_PIX_FMT_YUV_3P210P    v4l2_fourcc('Y', '2', '3', 'A')
	/* YUV-10bit packed 4:2:2 3plane */
#define V4L2_PIX_FMT_YUV_2P010P    v4l2_fourcc('U', '0', '2', 'A')
	/* YUV-10bit packed 4:2:0 2plane, (Y)(UV)  */
#define V4L2_PIX_FMT_YVU_2P010P    v4l2_fourcc('V', '0', '2', 'A')
	/* YUV-10bit packed 4:2:0 2plane, (Y)(VU)  */
#define V4L2_PIX_FMT_YUV_3P010P    v4l2_fourcc('Y', '0', '3', 'A')
	/* YUV-10bit packed 4:2:0 3plane */
#define V4L2_PIX_FMT_YUYV_Y210    v4l2_fourcc('Y', 'U', '1', 'a')
	/* YUV-10bit unpacked 4:2:2 1plane, YUYV  */
#define V4L2_PIX_FMT_YVYU_Y210    v4l2_fourcc('Y', 'V', '1', 'a')
	/* YUV-10bit unpacked 4:2:2 1plane, YVYU  */
#define V4L2_PIX_FMT_UYVY_Y210    v4l2_fourcc('U', 'Y', '1', 'a')
	/* YUV-10bit unpacked 4:2:2 1plane, UYVY  */
#define V4L2_PIX_FMT_VYUY_Y210    v4l2_fourcc('V', 'Y', '1', 'a')
	/* YUV-10bit unpacked 4:2:2 1plane, VYUY  */
#define V4L2_PIX_FMT_YUV_2P210    v4l2_fourcc('U', '2', '2', 'a')
	/* YUV-10bit unpacked 4:2:2 2plane, (Y)(UV)  */
#define V4L2_PIX_FMT_YVU_2P210    v4l2_fourcc('V', '2', '2', 'a')
	/* YUV-10bit unpacked 4:2:2 2plane, (Y)(VU)  */
#define V4L2_PIX_FMT_YUV_3P210    v4l2_fourcc('Y', '2', '3', 'a')
	/* YUV-10bit unpacked 4:2:2 3plane */
#define V4L2_PIX_FMT_YUV_2P010    v4l2_fourcc('U', '0', '2', 'a')
	/* YUV-10bit unpacked 4:2:0 2plane, (Y)(UV)  */
#define V4L2_PIX_FMT_YVU_2P010    v4l2_fourcc('V', '0', '2', 'a')
	/* YUV-10bit unpacked 4:2:0 2plane, (Y)(VU)  */
#define V4L2_PIX_FMT_YUV_3P010    v4l2_fourcc('Y', '0', '3', 'a')
	/* YUV-10bit unpacked 4:2:0 3plane */
#define V4L2_PIX_FMT_YUV_2P012P    v4l2_fourcc('U', '0', '2', 'C')
	/* YUV-12bit packed 4:2:0 2plane, (Y)(UV)  */
#define V4L2_PIX_FMT_YVU_2P012P    v4l2_fourcc('V', '0', '2', 'C')
	/* YUV-12bit packed 4:2:0 2plane, (Y)(VU)  */
#define V4L2_PIX_FMT_YUV_2P012    v4l2_fourcc('U', '0', '2', 'c')
	/* YUV-12bit unpacked 4:2:0 2plane, (Y)(UV)  */
#define V4L2_PIX_FMT_YVU_2P012    v4l2_fourcc('V', '0', '2', 'c')
	/* YUV-12bit unpacked 4:2:0 2plane, (Y)(VU)  */
#define V4L2_PIX_FMT_MTISP_RGB3PP8  v4l2_fourcc('M', 'r', '3', '8')
	/* RGB-8bit-3P-Packed, (R)(G)(B) */
#define V4L2_PIX_FMT_MTISP_RGB3PP10 v4l2_fourcc('M', 'r', '3', 'a')
	/* RGB-10bit-3P-Packed, (R)(G)(B) */
#define V4L2_PIX_FMT_MTISP_RGB3PP12 v4l2_fourcc('M', 'r', '3', 'c')
	/* RGB-12bit-3P-Packed, (R)(G)(B) */
#define V4L2_PIX_FMT_MTISP_RGB3PU8 v4l2_fourcc('M', 'R', '3', '8')
	/* RGB-8bit-3P-Unpacked, (R)(G)(B) */
#define V4L2_PIX_FMT_MTISP_RGB3PU10 v4l2_fourcc('M', 'R', '3', 'A')
	/* RGB-10bit-3P-Unpacked, (R)(G)(B) */
#define V4L2_PIX_FMT_MTISP_RGB3PU12 v4l2_fourcc('M', 'R', '3', 'C')
	/* RGB-12bit-3P-Unpacked, (R)(G)(B) */
#define V4L2_PIX_FMT_MTISP_FGRBP8 v4l2_fourcc('M', 'F', 'g', '8')
	/* FG-8bit-2P-Packed */
#define V4L2_PIX_FMT_MTISP_FGRBP10 v4l2_fourcc('M', 'F', 'g', 'a')
	/* FG-10bit-2P-Packed */
#define V4L2_PIX_FMT_MTISP_FGRBP12 v4l2_fourcc('M', 'F', 'g', 'c')
	/* FG-12bit-2P-Packed */
#define V4L2_PIX_FMT_MTISP_FGRBU8 v4l2_fourcc('M', 'F', 'G', '8')
	/* FG-8bit-2P-Unpacked */
#define V4L2_PIX_FMT_MTISP_FGRBU10 v4l2_fourcc('M', 'F', 'G', 'A')
      /* FG-10bit-2P-Unpacked */
#define V4L2_PIX_FMT_MTISP_FGRBU12 v4l2_fourcc('M', 'F', 'G', 'C')
	/* FG-12bit-2P-Unpacked */
#define V4L2_PIX_FMT_MTISP_FGRB3P8 v4l2_fourcc('M', 'f', '3', '8')
	/* FG-8bit-3P-Packed */
#define V4L2_PIX_FMT_MTISP_FGRB3P10 v4l2_fourcc('M', 'f', '3', 'a')
	/* FG-10bit-3P-Packed */
#define V4L2_PIX_FMT_MTISP_FGRB3P12 v4l2_fourcc('M', 'f', '3', 'c')
	/* FG-12bit-3P-Packed */
#define V4L2_PIX_FMT_MTISP_FGRB3U8 v4l2_fourcc('M', 'F', '3', '8')
	/* FG-8bit-3P-Unpacked */
#define V4L2_PIX_FMT_MTISP_FGRB3U10 v4l2_fourcc('M', 'F', '3', 'A')
	/* FG-10bit-3P-Unpacked */
#define V4L2_PIX_FMT_MTISP_FGRB3U12 v4l2_fourcc('M', 'F', '3', 'C')
	/* FG-12bit-3P-Unpacked */
#define V4L2_PIX_FMT_MTISP_RGB48 v4l2_fourcc('M', 'R', '1', '6')
	/* RGB-48bit */
#define V4L2_PIX_FMT_MTISP_Y32   v4l2_fourcc('M', 'T', '3', '2')
	/* Y-32bit */
#define V4L2_PIX_FMT_MTISP_Y16   v4l2_fourcc('M', 'T', '1', '6')
	/* Y-16bit */
#define V4L2_PIX_FMT_MTISP_Y8   v4l2_fourcc('M', 'T', '0', '8')
	/* Y-8bit */
#define V4L2_PIX_FMT_MTISP_SBGGRU10  v4l2_fourcc('M', 'b', 'B', 'A')
	/* Bayer-10bit-Unpacked, (B)(G)(G)(R) */
#define V4L2_PIX_FMT_MTISP_SGBRGU10  v4l2_fourcc('M', 'b', 'G', 'A')
	/* Bayer-10bit-Unpacked, (G)(B)(R)(G) */
#define V4L2_PIX_FMT_MTISP_SGRBGU10  v4l2_fourcc('M', 'b', 'g', 'A')
	/* Bayer-10bit-Unpacked, (G)(R)(B)(G) */
#define V4L2_PIX_FMT_MTISP_SRGGBU10  v4l2_fourcc('M', 'b', 'R', 'A')
	/* Bayer-10bit-Unpacked, (R)(G)(G)(B) */
#define V4L2_PIX_FMT_MTISP_SBGGRU12  v4l2_fourcc('M', 'b', 'B', 'C')
	/* Bayer-12bit-Unpacked, (B)(G)(G)(R) */
#define V4L2_PIX_FMT_MTISP_SGBRGU12  v4l2_fourcc('M', 'b', 'G', 'C')
	/* Bayer-12bit-Unpacked, (G)(B)(R)(G) */
#define V4L2_PIX_FMT_MTISP_SGRBGU12  v4l2_fourcc('M', 'b', 'g', 'C')
	/* Bayer-12bit-Unpacked, (G)(R)(B)(G) */
#define V4L2_PIX_FMT_MTISP_SRGGBU12  v4l2_fourcc('M', 'b', 'R', 'C')
	/* Bayer-12bit-Unpacked, (R)(G)(G)(B) */

/* Vendor specific - Mediatek ISP bayer formats: from videodev2.h */
#define V4L2_PIX_FMT_MTISP_SBGGR10  v4l2_fourcc('M', 'B', 'B', 'A')
/*  Packed 10-bit  */
#define V4L2_PIX_FMT_MTISP_SGBRG10  v4l2_fourcc('M', 'B', 'G', 'A')
/*  Packed 10-bit  */
#define V4L2_PIX_FMT_MTISP_SGRBG10  v4l2_fourcc('M', 'B', 'g', 'A')
/*  Packed 10-bit  */
#define V4L2_PIX_FMT_MTISP_SRGGB10  v4l2_fourcc('M', 'B', 'R', 'A')
/*  Packed 10-bit  */
#define V4L2_PIX_FMT_MTISP_SBGGR12  v4l2_fourcc('M', 'B', 'B', 'C')
/*  Packed 12-bit  */
#define V4L2_PIX_FMT_MTISP_SGBRG12  v4l2_fourcc('M', 'B', 'G', 'C')
/*  Packed 12-bit  */
#define V4L2_PIX_FMT_MTISP_SGRBG12  v4l2_fourcc('M', 'B', 'g', 'C')
/*  Packed 12-bit  */
#define V4L2_PIX_FMT_MTISP_SRGGB12  v4l2_fourcc('M', 'B', 'R', 'C')
/*  Packed 12-bit  */
#define V4L2_PIX_FMT_MTISP_SBGGR14  v4l2_fourcc('M', 'B', 'B', 'E')
/*  Packed 14-bit  */
#define V4L2_PIX_FMT_MTISP_SGBRG14  v4l2_fourcc('M', 'B', 'G', 'E')
/*  Packed 14-bit  */
#define V4L2_PIX_FMT_MTISP_SGRBG14  v4l2_fourcc('M', 'B', 'g', 'E')
/*  Packed 14-bit  */
#define V4L2_PIX_FMT_MTISP_SRGGB14  v4l2_fourcc('M', 'B', 'R', 'E')
/*  Packed 14-bit  */
#define V4L2_PIX_FMT_MTISP_SBGGR8F   v4l2_fourcc('M', 'F', 'B', '8')
/*  Full-G  8-bit  */
#define V4L2_PIX_FMT_MTISP_SGBRG8F   v4l2_fourcc('M', 'F', 'G', '8')
/*  Full-G  8-bit  */
#define V4L2_PIX_FMT_MTISP_SGRBG8F   v4l2_fourcc('M', 'F', 'g', '8')
/*  Full-G  8-bit  */
#define V4L2_PIX_FMT_MTISP_SRGGB8F   v4l2_fourcc('M', 'F', 'R', '8')
/*  Full-G  8-bit  */
#define V4L2_PIX_FMT_MTISP_SBGGR10F  v4l2_fourcc('M', 'F', 'B', 'A')
/*  Full-G 10-bit  */
#define V4L2_PIX_FMT_MTISP_SGBRG10F  v4l2_fourcc('M', 'F', 'G', 'A')
/*  Full-G 10-bit  */
#define V4L2_PIX_FMT_MTISP_SGRBG10F  v4l2_fourcc('M', 'F', 'g', 'A')
/*  Full-G 10-bit  */
#define V4L2_PIX_FMT_MTISP_SRGGB10F  v4l2_fourcc('M', 'F', 'R', 'A')
/*  Full-G 10-bit  */
#define V4L2_PIX_FMT_MTISP_SBGGR12F  v4l2_fourcc('M', 'F', 'B', 'C')
/*  Full-G 12-bit  */
#define V4L2_PIX_FMT_MTISP_SGBRG12F  v4l2_fourcc('M', 'F', 'G', 'C')
/*  Full-G 12-bit  */
#define V4L2_PIX_FMT_MTISP_SGRBG12F  v4l2_fourcc('M', 'F', 'g', 'C')
/*  Full-G 12-bit  */
#define V4L2_PIX_FMT_MTISP_SRGGB12F  v4l2_fourcc('M', 'F', 'R', 'C')
/*  Full-G 12-bit  */
#define V4L2_PIX_FMT_MTISP_SBGGR14F  v4l2_fourcc('M', 'F', 'B', 'E')
/*  Full-G 14-bit  */
#define V4L2_PIX_FMT_MTISP_SGBRG14F  v4l2_fourcc('M', 'F', 'G', 'E')
/*  Full-G 14-bit  */
#define V4L2_PIX_FMT_MTISP_SGRBG14F  v4l2_fourcc('M', 'F', 'g', 'E')
/*  Full-G 14-bit  */
#define V4L2_PIX_FMT_MTISP_SRGGB14F  v4l2_fourcc('M', 'F', 'R', 'E')
/*  Full-G 14-bit  */
/* Vendor specific - Mediatek Luminance+Chrominance formats */
#define V4L2_PIX_FMT_MTISP_YUYV10P v4l2_fourcc('Y', 'U', 'A', 'P')
/* 16  YUV 4:2:2 10-bit packed */
#define V4L2_PIX_FMT_MTISP_YVYU10P v4l2_fourcc('Y', 'V', 'A', 'P')
/* 16  YUV 4:2:2 10-bit packed */
#define V4L2_PIX_FMT_MTISP_UYVY10P v4l2_fourcc('U', 'Y', 'A', 'P')
/* 16  YUV 4:2:2 10-bit packed */
#define V4L2_PIX_FMT_MTISP_VYUY10P v4l2_fourcc('V', 'Y', 'A', 'P')
/* 16  YUV 4:2:2 10-bit packed */
#define V4L2_PIX_FMT_MTISP_NV12_10P v4l2_fourcc('1', '2', 'A', 'P')
/* 12  Y/CbCr 4:2:0 10 bits packed */
#define V4L2_PIX_FMT_MTISP_NV21_10P v4l2_fourcc('2', '1', 'A', 'P')
/* 12  Y/CrCb 4:2:0 10 bits packed */
#define V4L2_PIX_FMT_MTISP_NV16_10P v4l2_fourcc('1', '6', 'A', 'P')
/* 16  Y/CbCr 4:2:2 10 bits packed */
#define V4L2_PIX_FMT_MTISP_NV61_10P v4l2_fourcc('6', '1', 'A', 'P')
/* 16  Y/CrCb 4:2:2 10 bits packed */
#define V4L2_PIX_FMT_MTISP_SBGGRU14  v4l2_fourcc('M', 'b', 'B', 'E')
	/* Bayer-14bit-Unpacked, (B)(G)(G)(R) */
#define V4L2_PIX_FMT_MTISP_SGBRGU14  v4l2_fourcc('M', 'b', 'G', 'E')
	/* Bayer-14bit-Unpacked, (G)(B)(R)(G) */
#define V4L2_PIX_FMT_MTISP_SGRBGU14  v4l2_fourcc('M', 'b', 'g', 'E')
	/* Bayer-14bit-Unpacked, (G)(R)(B)(G) */
#define V4L2_PIX_FMT_MTISP_SRGGBU14  v4l2_fourcc('M', 'b', 'R', 'E')
	/* Bayer-14bit-Unpacked, (R)(G)(G)(B) */
#define V4L2_PIX_FMT_MTISP_SBGGRU15  v4l2_fourcc('M', 'b', 'B', 'F')
	/* Bayer-15bit-Unpacked, (B)(G)(G)(R) */
#define V4L2_PIX_FMT_MTISP_SGBRGU15  v4l2_fourcc('M', 'b', 'G', 'F')
	/* Bayer-15bit-Unpacked, (G)(B)(R)(G) */
#define V4L2_PIX_FMT_MTISP_SGRBGU15  v4l2_fourcc('M', 'b', 'g', 'F')
	/* Bayer-15bit-Unpacked, (G)(R)(B)(G) */
#define V4L2_PIX_FMT_MTISP_SRGGBU15  v4l2_fourcc('M', 'b', 'R', 'F')
	/* Bayer-15bit-Unpacked, (R)(G)(G)(B) */
#define V4L2_PIX_FMT_MTISP_SBGGR16  v4l2_fourcc('M', 'B', 'B', 'G')
	/* Bayer-16bit-Packed, (B)(G)(G)(R) */
#define V4L2_PIX_FMT_MTISP_SGBRG16  v4l2_fourcc('M', 'B', 'G', 'G')
	/* Bayer-16bit-Packed, (G)(B)(R)(G) */
#define V4L2_PIX_FMT_MTISP_SGRBG16  v4l2_fourcc('M', 'B', 'g', 'G')
	/* Bayer-16bit-Packed, (G)(R)(B)(G) */
#define V4L2_PIX_FMT_MTISP_SRGGB16  v4l2_fourcc('M', 'B', 'R', 'G')
	/* Bayer-16bit-Packed, (R)(G)(G)(B) */
#define V4L2_PIX_FMT_MTISP_SBGGR22  v4l2_fourcc('M', 'B', 'B', 'M')
	/* Bayer-22bit-Packed, (B)(G)(G)(R) */
#define V4L2_PIX_FMT_MTISP_SGBRG22  v4l2_fourcc('M', 'B', 'G', 'M')
	/* Bayer-22bit-Packed, (G)(B)(R)(G) */
#define V4L2_PIX_FMT_MTISP_SGRBG22  v4l2_fourcc('M', 'B', 'g', 'M')
	/* Bayer-22bit-Packed, (G)(R)(B)(G) */
#define V4L2_PIX_FMT_MTISP_SRGGB22  v4l2_fourcc('M', 'B', 'R', 'M')
	/* Bayer-22bit-Packed, (R)(G)(G)(B) */
#define V4L2_PIX_FMT_UFBC_NV12    v4l2_fourcc('U', 'F', '2', '8')
	/* YUV-8bit UFBC packed 4:2:0 2plane, (Y)(UV)  */
#define V4L2_PIX_FMT_UFBC_NV21    v4l2_fourcc('V', 'F', '2', '8')
	/* YUV-8bit UFBC packed 4:2:0 2plane, (Y)(VU)  */
#define V4L2_PIX_FMT_UFBC_YUV_2P010P    v4l2_fourcc('U', 'F', '2', 'A')
	/* YUV-10bit UFBC packed 4:2:0 2plane, (Y)(UV)  */
#define V4L2_PIX_FMT_UFBC_YVU_2P010P    v4l2_fourcc('V', 'F', '2', 'A')
	/* YUV-10bit UFBC packed 4:2:0 2plane, (Y)(VU)  */
#define V4L2_PIX_FMT_UFBC_YUV_2P012P    v4l2_fourcc('U', 'F', '2', 'C')
	/* YUV-12bit UFBC packed 4:2:0 2plane, (Y)(UV)  */
#define V4L2_PIX_FMT_UFBC_YVU_2P012P    v4l2_fourcc('V', 'F', '2', 'C')
	/* YUV-12bit UFBC packed 4:2:0 2plane, (Y)(VU)  */
#define V4L2_PIX_FMT_MTISP_UFBC_SBGGR8 v4l2_fourcc('U', 'B', 'B', '8')
	/* UFBC Bayer format, 8 bits, 1 plane, may be (BGGR) */
#define V4L2_PIX_FMT_MTISP_UFBC_SGBRG8 v4l2_fourcc('U', 'B', 'G', '8')
	/* UFBC Bayer format, 8 bits, 1 plane, may be (GBRG) */
#define V4L2_PIX_FMT_MTISP_UFBC_SGRBG8 v4l2_fourcc('U', 'B', 'g', '8')
	/* UFBC Bayer format, 8 bits, 1 plane, may be (GRBG) */
#define V4L2_PIX_FMT_MTISP_UFBC_SRGGB8 v4l2_fourcc('U', 'B', 'R', '8')
	/* UFBC Bayer format, 8 bits, 1 plane, may be (RGGB) */
#define V4L2_PIX_FMT_MTISP_UFBC_SBGGR10 v4l2_fourcc('U', 'B', 'B', 'A')
	/* UFBC Bayer format, 10 bits, 1 plane, may be (BGGR) */
#define V4L2_PIX_FMT_MTISP_UFBC_SGBRG10 v4l2_fourcc('U', 'B', 'G', 'A')
	/* UFBC Bayer format, 10 bits, 1 plane, may be (GBRG) */
#define V4L2_PIX_FMT_MTISP_UFBC_SGRBG10 v4l2_fourcc('U', 'B', 'g', 'A')
	/* UFBC Bayer format, 10 bits, 1 plane, may be (GRBG) */
#define V4L2_PIX_FMT_MTISP_UFBC_SRGGB10 v4l2_fourcc('U', 'B', 'R', 'A')
	/* UFBC Bayer format, 10 bits, 1 plane, may be (RGGB) */
#define V4L2_PIX_FMT_MTISP_UFBC_SBGGR12 v4l2_fourcc('U', 'B', 'B', 'C')
	/* UFBC Bayer format, 12 bits, 1 plane, may be (BGGR) */
#define V4L2_PIX_FMT_MTISP_UFBC_SGBRG12 v4l2_fourcc('U', 'B', 'G', 'C')
	/* UFBC Bayer format, 12 bits, 1 plane, may be (GBRG) */
#define V4L2_PIX_FMT_MTISP_UFBC_SGRBG12 v4l2_fourcc('U', 'B', 'g', 'C')
	/* UFBC Bayer format, 12 bits, 1 plane, may be (GRBG) */
#define V4L2_PIX_FMT_MTISP_UFBC_SRGGB12 v4l2_fourcc('U', 'B', 'R', 'C')
	/* UFBC Bayer format, 12 bits, 1 plane, may be (RGGB) */
#define V4L2_PIX_FMT_MTISP_UFBC_SBGGR14 v4l2_fourcc('U', 'B', 'B', 'E')
	/* UFBC Bayer format, 14 bits, 1 plane, may be (BGGR) */
#define V4L2_PIX_FMT_MTISP_UFBC_SGBRG14 v4l2_fourcc('U', 'B', 'G', 'E')
	/* UFBC Bayer format, 14 bits, 1 plane, may be (GBRG) */
#define V4L2_PIX_FMT_MTISP_UFBC_SGRBG14 v4l2_fourcc('U', 'B', 'g', 'E')
	/* UFBC Bayer format, 14 bits, 1 plane, may be (GRBG) */
#define V4L2_PIX_FMT_MTISP_UFBC_SRGGB14 v4l2_fourcc('U', 'B', 'R', 'E')
	/* UFBC Bayer format, 14 bits, 1 plane, may be (RGGB) */
#define V4L2_PIX_FMT_AFBC_RGBA32 v4l2_fourcc('A', 'F', 'R', '8')
	/* RGB-8bit AFBC packed, (R)(G)(B)(A) */
#define V4L2_PIX_FMT_AFBC_BGRA32 v4l2_fourcc('A', 'F', 'B', '8')
	/* RGB-8bit AFBC packed, (B)(G)(R)(A) */
#define V4L2_PIX_FMT_AFBC_NV12 v4l2_fourcc('A', 'F', 'U', '8')
	/* YUV-8bit AFBC packed 4:2:0 2plane, (Y)(UV) */
#define V4L2_PIX_FMT_AFBC_NV21 v4l2_fourcc('A', 'F', 'V', '8')
	/* YUV-8bit AFBC packed 4:2:0 2plane, (Y)(VU) */
#define V4L2_PIX_FMT_AFBC_YUV_2P010P v4l2_fourcc('A', 'F', 'U', 'A')
	/* YUV-10bit AFBC packed 4:2:0 2plane, (Y)(UV) */
#define V4L2_PIX_FMT_AFBC_YVU_2P010P v4l2_fourcc('A', 'F', 'V', 'A')
	/* YUV-10bit AFBC packed 4:2:0 2plane, (Y)(VU) */
#define V4L2_PIX_FMT_MTISP_SBGGRM10 v4l2_fourcc('M', 'M', 'B', 'A')
	/* MIPI-10bit-Packed, (B)(G)(G)(R) */
#define V4L2_PIX_FMT_MTISP_SGBRGM10 v4l2_fourcc('M', 'M', 'G', 'A')
	/* MIPI-10bit-Packed, (G)(B)(R)(G) */
#define V4L2_PIX_FMT_MTISP_SGRBGM10 v4l2_fourcc('M', 'M', 'g', 'A')
	/* MIPI-10bit-Packed, (G)(R)(B)(G) */
#define V4L2_PIX_FMT_MTISP_SRGGBM10 v4l2_fourcc('M', 'M', 'R', 'A')
	/* MIPI-10bit-Packed, (R)(G)(G)(B) */

/* Vendor specific - Mediatek ISP parameters for firmware */
#define V4L2_META_FMT_MTISP_PARAMS v4l2_fourcc('M', 'T', 'f', 'p')

#endif
