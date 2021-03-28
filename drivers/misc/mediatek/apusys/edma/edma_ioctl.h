/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: JB Tsai <jb.tsai@mediatek.com>
 */

#ifndef __EDMA_IOCTL_H__
#define __EDMA_IOCTL_H__

#define	EDMA_EXT_MODE_SIZE		0x40

#define EDMA_MAGICNO			'v'
#define EDMA_IOCTL_ENQUE_NORMAL		_IOWR(EDMA_MAGICNO,  0,\
					struct edma_normal)
#define EDMA_IOCTL_ENQUE_FILL		_IOWR(EDMA_MAGICNO,  1,\
					struct edma_fill)
#define EDMA_IOCTL_ENQUE_NUMERICAL	_IOWR(EDMA_MAGICNO,  2,\
					struct edma_numerical)
#define EDMA_IOCTL_ENQUE_FORMAT		_IOWR(EDMA_MAGICNO,  3,\
					struct edma_format)
#define EDMA_IOCTL_ENQUE_COMPRESS	_IOWR(EDMA_MAGICNO,  4,\
					struct edma_compress)
#define EDMA_IOCTL_ENQUE_DECOMPRESS	_IOWR(EDMA_MAGICNO,  5,\
					struct edma_decompress)
#define EDMA_IOCTL_ENQUE_RAW		_IOWR(EDMA_MAGICNO,  6,\
					struct edma_raw)
#define EDMA_IOCTL_DEQUE		_IOWR(EDMA_MAGICNO,  7,\
					struct edma_cmd_deque)
#define EDMA_IOCTL_SYNC_NORMAL		_IOWR(EDMA_MAGICNO,  8,\
					struct edma_normal)
#define EDMA_IOCTL_ENQUE_EXT_MODE	_IOWR(EDMA_MAGICNO,  9,\
					struct edma_ext)
#define EDMA_IOCTL_SYNC_EXT_MODE	_IOWR(EDMA_MAGICNO,  10,\
					struct edma_ext)


enum edma_req_status {
	/** 0: EDMA_REQ_STATUS_ENQUEUE */
	EDMA_REQ_STATUS_ENQUEUE,
	/** 1: EDMA_REQ_STATUS_RUN */
	EDMA_REQ_STATUS_RUN,
	/** 2: EDMA_REQ_STATUS_DEQUEUE */
	EDMA_REQ_STATUS_DEQUEUE,
	/** 3: EDMA_REQ_STATUS_TIMEOUT */
	EDMA_REQ_STATUS_TIMEOUT,
	/** 4: EDMA_REQ_STATUS_INVALID */
	EDMA_REQ_STATUS_INVALID,
	/** 5: EDMA_REQ_STATUS_FLUSH */
	EDMA_REQ_STATUS_FLUSH,
};

enum edma_desp_format {
	EDMA_FORMAT_FP32 = 1,
	EDMA_FORMAT_FP16,
	EDMA_FORMAT_I8,
	EDMA_FORMAT_YUV420_2_PLANE_8B,
	EDMA_FORMAT_YUV420_2_PLANE_16B,
	EDMA_FORMAT_YUV420_2_PLANE_P010,
	EDMA_FORMAT_YUV420_2_PLANE_MTK420_PACK_10B,
	EDMA_FORMAT_RGB_8B,
	EDMA_FORMAT_ARGB2SET_8B,
	EDMA_FORMAT_ARGB2SET_16B,
	EDMA_FORMAT_ARGB2SET_2101010,
	EDMA_FORMAT_BITSTREAM_YUV420_8B,
	EDMA_FORMAT_BITSTREAM_YUV420_10B,
	EDMA_FORMAT_BITSTREAM_ARGB_8B,
	EDMA_FORMAT_BITSTREAM_ARGB_10B,
};

struct edma_normal {
	__u64 cmd_handle;
	__u32 tile_channel;
	__u32 tile_width;
	__u32 tile_height;
	__u32 src_channel_stride;
	__u32 src_width_stride;
	__u32 dst_channel_stride;
	__u32 dst_width_stride;
	__u32 src_addr;
	__u32 dst_addr;
	__u8  buf_iommu_en;
};

struct edma_fill {
	__u64 cmd_handle;
	__u32 tile_channel;
	__u32 tile_width;
	__u32 tile_height;
	__u32 dst_channel_stride;
	__u32 dst_width_stride;
	__u32 dst_addr;
	__u32 fill_value;
	__u8  buf_iommu_en;
};

struct edma_numerical {
	__u64 cmd_handle;
	__u32 tile_channel;
	__u32 tile_width;
	__u32 tile_height;
	__u32 src_channel_stride;
	__u32 src_width_stride;
	__u32 dst_channel_stride;
	__u32 dst_width_stride;
	__u32 src_addr;
	__u32 dst_addr;
	__u32 range_scale;
	__u32 min_fp32;
	__u8  in_format;
	__u8  out_format;
};

struct edma_format {
	__u64 cmd_handle;
	__u32 src_tile_channel;
	__u32 src_tile_width;
	__u32 src_tile_height;
	__u32 src_channel_stride;
	__u32 src_uv_channel_stride;
	__u32 src_width_stride;
	__u32 src_uv_width_stride;
	__u32 dst_tile_channel;
	__u32 dst_tile_width;
	__u32 dst_channel_stride;
	__u32 dst_uv_channel_stride;
	__u32 dst_width_stride;
	__u32 dst_uv_width_stride;
	__u32 src_addr;
	__u32 src_uv_addr;
	__u32 dst_addr;
	__u32 dst_uv_addr;
	__u32 param_a;
	__u8  in_format;
	__u8  out_format;
};

struct edma_compress {
	__u64 cmd_handle;
	__u32 src_tile_channel;
	__u32 src_tile_width;
	__u32 src_tile_height;
	__u32 src_channel_stride;
	__u32 src_uv_channel_stride;
	__u32 src_width_stride;
	__u32 src_uv_width_stride;
	__u32 dst_tile_channel;
	__u32 dst_tile_width;
	__u32 dst_channel_stride;
	__u32 dst_uv_channel_stride;
	__u32 dst_width_stride;
	__u32 dst_uv_width_stride;
	__u32 src_addr;
	__u32 src_uv_addr;
	__u32 dst_addr;
	__u32 dst_uv_addr;
	__u32 param_a;
	__u32 param_m;
	__u32 cmprs_src_pxl;
	__u32 cmprs_dst_pxl;
	__u32 src_c_stride_pxl;
	__u32 src_w_stride_pxl;
	__u32 src_c_offset_m1;
	__u32 src_w_offset_m1;
	__u32 dst_c_stride_pxl;
	__u32 dst_w_stride_pxl;
	__u32 dst_c_offset_m1;
	__u32 dst_w_offset_m1;
	__u8  in_format;
	__u8  out_format;
	__u8  yuv2rgb_mat_bypass;
	__u8  rgb2yuv_mat_bypass;
	__u8  yuv2rgb_mat_select;
	__u8  rgb2yuv_mat_select;
};

struct edma_decompress {
	__u64 cmd_handle;
	__u32 src_tile_channel;
	__u32 src_tile_width;
	__u32 src_tile_height;
	__u32 src_channel_stride;
	__u32 src_uv_channel_stride;
	__u32 src_width_stride;
	__u32 src_uv_width_stride;
	__u32 dst_tile_channel;
	__u32 dst_tile_width;
	__u32 dst_channel_stride;
	__u32 dst_uv_channel_stride;
	__u32 dst_width_stride;
	__u32 dst_uv_width_stride;
	__u32 src_addr;
	__u32 src_uv_addr;
	__u32 dst_addr;
	__u32 dst_uv_addr;
	__u32 param_a;
	__u32 param_m;
	__u32 cmprs_src_pxl;
	__u32 cmprs_dst_pxl;
	__u32 src_c_stride_pxl;
	__u32 src_w_stride_pxl;
	__u32 src_c_offset_m1;
	__u32 src_w_offset_m1;
	__u32 dst_c_stride_pxl;
	__u32 dst_w_stride_pxl;
	__u32 dst_c_offset_m1;
	__u32 dst_w_offset_m1;
	__u8  in_format;
	__u8  out_format;
	__u8  yuv2rgb_mat_bypass;
	__u8  rgb2yuv_mat_bypass;
	__u8  yuv2rgb_mat_select;
	__u8  rgb2yuv_mat_select;
};

struct edma_raw {
	__u64 cmd_handle;
	__u32 src_tile_channel;
	__u32 src_tile_width;
	__u32 src_tile_height;
	__u32 src_channel_stride;
	__u32 src_width_stride;
	__u32 dst_tile_channel;
	__u32 dst_tile_width;
	__u32 dst_channel_stride;
	__u32 dst_width_stride;
	__u32 src_addr;
	__u32 src_uv_addr;
	__u32 dst_addr;
	__u8  plane_num;
	__u8  unpack_shift;
	__u8  bit_num;
};

struct edma_ext {
	__u64 cmd_handle;
	__u32 count;
	__u32 reg_addr;
	__u32 fill_value;
	__u8  desp_iommu_en;
} __attribute__ ((__packed__));

struct edma_cmd_deque {
	__u64 cmd_handle;
	__s32 cmd_result;
	__u32 cmd_status;
};

#endif /* __EDMA_IOCTL_H__ */
