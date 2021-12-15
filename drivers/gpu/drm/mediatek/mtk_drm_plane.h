/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MTK_DRM_PLANE_H_
#define _MTK_DRM_PLANE_H_

#include <drm/drm_crtc.h>
#include <linux/types.h>
#include <drm/mediatek_drm.h>

#define MAKE_DISP_FORMAT_ID(id, bpp) (((id) << 8) | (bpp))

#define MTK_PLANE_OVL_TIMELINE_ID(x) (x)

/* /============================
 */
/* structure declarations */
/* /=========================== */

enum DISP_ORIENTATION {
	DISP_ORIENTATION_0 = 0,
	DISP_ORIENTATION_90 = 1,
	DISP_ORIENTATION_180 = 2,
	DISP_ORIENTATION_270 = 3,
};

enum DISP_FORMAT {
	DISP_FORMAT_UNKNOWN = 0,

	DISP_FORMAT_RGB565 = MAKE_DISP_FORMAT_ID(1, 2),
	DISP_FORMAT_RGB888 = MAKE_DISP_FORMAT_ID(2, 3),
	DISP_FORMAT_BGR888 = MAKE_DISP_FORMAT_ID(3, 3),
	DISP_FORMAT_ARGB8888 = MAKE_DISP_FORMAT_ID(4, 4),
	DISP_FORMAT_ABGR8888 = MAKE_DISP_FORMAT_ID(5, 4),
	DISP_FORMAT_RGBA8888 = MAKE_DISP_FORMAT_ID(6, 4),
	DISP_FORMAT_BGRA8888 = MAKE_DISP_FORMAT_ID(7, 4),
	DISP_FORMAT_YUV422 = MAKE_DISP_FORMAT_ID(8, 2),
	DISP_FORMAT_XRGB8888 = MAKE_DISP_FORMAT_ID(9, 4),
	DISP_FORMAT_XBGR8888 = MAKE_DISP_FORMAT_ID(10, 4),
	DISP_FORMAT_RGBX8888 = MAKE_DISP_FORMAT_ID(11, 4),
	DISP_FORMAT_BGRX8888 = MAKE_DISP_FORMAT_ID(12, 4),
	DISP_FORMAT_UYVY = MAKE_DISP_FORMAT_ID(13, 2),
	DISP_FORMAT_YUV420_P = MAKE_DISP_FORMAT_ID(14, 2),
	DISP_FORMAT_YV12 = MAKE_DISP_FORMAT_ID(16, 1), /* BPP = 1.5 */
	DISP_FORMAT_PARGB8888 = MAKE_DISP_FORMAT_ID(17, 4),
	DISP_FORMAT_PABGR8888 = MAKE_DISP_FORMAT_ID(18, 4),
	DISP_FORMAT_PRGBA8888 = MAKE_DISP_FORMAT_ID(19, 4),
	DISP_FORMAT_PBGRA8888 = MAKE_DISP_FORMAT_ID(20, 4),
	DISP_FORMAT_DIM = MAKE_DISP_FORMAT_ID(21, 0),
	DISP_FORMAT_BPP_MASK = 0xFF,
};

enum DISP_LAYER_TYPE {
	DISP_LAYER_2D = 0,
	DISP_LAYER_3D_SBS_0 = 0x1,
	DISP_LAYER_3D_SBS_90 = 0x2,
	DISP_LAYER_3D_SBS_180 = 0x3,
	DISP_LAYER_3D_SBS_270 = 0x4,
	DISP_LAYER_3D_TAB_0 = 0x10,
	DISP_LAYER_3D_TAB_90 = 0x20,
	DISP_LAYER_3D_TAB_180 = 0x30,
	DISP_LAYER_3D_TAB_270 = 0x40,
};

enum DISP_BUFFER_TYPE {
	/* normal memory */
	DISP_NORMAL_BUFFER = 0,
	/* normal memory but should not be dumpped within screenshot */
	DISP_PROTECT_BUFFER = 1,
	/* secure memory */
	DISP_SECURE_BUFFER = 2,
	DISP_SECURE_BUFFER_SHIFT = 0x10002
};

enum DISP_BUFFER_SOURCE {
	/* ion buffer */
	DISP_BUFFER_ION = 0,
	/* dim layer, const alpha */
	DISP_BUFFER_ALPHA = 1,
	/* mva buffer */
	DISP_BUFFER_MVA = 2,
};

enum DISP_ALPHA_TYPE {
	DISP_ALPHA_ONE = 0,
	DISP_ALPHA_SRC = 1,
	DISP_ALPHA_SRC_INVERT = 2,
	DISP_ALPHA_INVALID = 3,
};

enum DISP_YUV_RANGE_ENUM {
	DISP_YUV_BT601_FULL = 0,
	DISP_YUV_BT601 = 1,
	DISP_YUV_BT709 = 2
};

enum MTK_FMT_MODIFIER {
	MTK_FMT_NONE = 0,
	MTK_FMT_PREMULTIPLIED = 1,
};

enum MTK_PLANE_PROP {
	PLANE_PROP_NEXT_BUFF_IDX,
	PLANE_PROP_LYE_BLOB_IDX,
	PLANE_PROP_ALPHA_CON,
	PLANE_PROP_PLANE_ALPHA,
	PLANE_PROP_DATASPACE,
	PLANE_PROP_VPITCH,
	PLANE_PROP_COMPRESS,
	PLANE_PROP_DIM_COLOR,
	PLANE_PROP_IS_MML,
	PLANE_PROP_MML_SUBMIT,
	PLANE_PROP_MAX,
};

struct mtk_drm_plane {
	struct drm_plane base;
	struct drm_property *plane_property[PLANE_PROP_MAX];
};

#define to_mtk_plane(x) container_of(x, struct mtk_drm_plane, base)

struct mtk_plane_pending_state {
	bool config;
	bool enable;
	dma_addr_t addr;
	size_t size;
	unsigned int pitch;
	unsigned int format;
	uint64_t modifier;
	unsigned int src_x;
	unsigned int src_y;
	unsigned int dst_x;
	unsigned int dst_y;
	unsigned int width;
	unsigned int height;
	bool dirty;
	bool is_sec;
	int sec_id;
	unsigned int prop_val[PLANE_PROP_MAX];
};

struct mtk_plane_input_config {
	void *src_base_addr;
	void *src_phy_addr;
	enum DISP_BUFFER_SOURCE buffer_source;
	enum DISP_BUFFER_TYPE security;
	enum DISP_FORMAT src_fmt;
	enum DISP_ALPHA_TYPE src_alpha;
	enum DISP_ALPHA_TYPE dst_alpha;
	enum DISP_YUV_RANGE_ENUM yuv_range;

	enum DISP_ORIENTATION layer_rotation;
	enum DISP_LAYER_TYPE layer_type;
	enum DISP_ORIENTATION video_rotation;

	__u32 next_buff_idx;
	/* fence to be waited before using this buffer. -1 if invalid */
	int src_fence_fd;
	/* fence struct of src_fence_fd, used in kernel */
	void *src_fence_struct;

	__u32 src_color_key;
	__u32 frm_sequence;

	void *dirty_roi_addr;
	__u16 dirty_roi_num;

	__u16 src_pitch;
	__u16 src_offset_x, src_offset_y;
	__u16 src_width, src_height;
	__u16 tgt_offset_x, tgt_offset_y;
	__u16 tgt_width, tgt_height;

	__u8 alpha_enable;
	__u8 alpha;
	__u8 sur_aen;
	__u8 src_use_color_key;
	__u8 layer_id;
	__u8 layer_enable;
	__u8 src_direct_link;

	__u8 isTdshp;
	__u8 identity;
	__u8 connected_type;
	__s8 ext_sel_layer;
};

struct mtk_plane_comp_state {
	uint32_t comp_id;
	uint32_t lye_id;
	int32_t ext_lye_id;
	uint32_t layer_caps;
};

struct mtk_plane_state {
	struct drm_plane_state base;
	struct mtk_plane_pending_state pending;
	struct mtk_plane_comp_state comp_state;
	struct drm_crtc *crtc;

	/* property */
	unsigned int prop_val[PLANE_PROP_MAX];
};

#define to_mtk_plane_state(x) container_of(x, struct mtk_plane_state, base)

int mtk_plane_init(struct drm_device *dev, struct mtk_drm_plane *plane,
		   unsigned int zpos, unsigned long possible_crtcs,
		   enum drm_plane_type type);

int mtk_get_format_bpp(uint32_t format);
char *mtk_get_format_name(uint32_t format);

void mtk_plane_get_comp_state(struct drm_plane *plane,
			      struct mtk_plane_comp_state *comp_state,
			      struct drm_crtc *crtc, int lock);
unsigned int to_crtc_plane_index(unsigned int plane_index);

#endif
