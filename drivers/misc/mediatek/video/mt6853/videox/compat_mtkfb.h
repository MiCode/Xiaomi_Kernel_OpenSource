// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef _COMPAT_MTKFB_H_
#define _COMPAT_MTKFB_H_
#include <linux/compat.h>

#ifdef CONFIG_COMPAT

struct compat_fb_overlay_layer {
	compat_uint_t layer_id;
	compat_uint_t layer_enable;

	compat_uptr_t src_base_addr;
	compat_uptr_t src_phy_addr;
	compat_uint_t src_direct_link;
	compat_int_t src_fmt;
	compat_uint_t src_use_color_key;
	compat_uint_t src_color_key;
	compat_uint_t src_pitch;
	compat_uint_t src_offset_x, src_offset_y;
	compat_uint_t src_width, src_height;

	compat_uint_t tgt_offset_x, tgt_offset_y;
	compat_uint_t tgt_width, tgt_height;
	compat_int_t layer_rotation;
	compat_int_t layer_type;
	compat_int_t video_rotation;
	/* set to 1, will go through tdshp first,
	 * then layer blending, then to color
	 */
	compat_uint_t isTdshp;

	compat_int_t next_buff_idx;
	compat_int_t identity;
	compat_int_t connected_type;
	compat_uint_t security;
	compat_uint_t alpha_enable;
	compat_uint_t alpha;
	compat_int_t fence_fd;
	compat_int_t ion_fd;
};

struct compat_mtk_dispif_info {
	compat_uint_t display_id;
	compat_uint_t isHwVsyncAvailable;
	compat_uint_t displayType;
	compat_uint_t displayWidth;
	compat_uint_t displayHeight;
	compat_uint_t displayFormat;
	compat_uint_t displayMode;
	compat_uint_t vsyncFPS;
	compat_uint_t physicalWidth;
	compat_uint_t physicalHeight;
	compat_uint_t isConnected;
	compat_uint_t lcmOriginalWidth;
	compat_uint_t lcmOriginalHeight;
};

#define COMPAT_MTKFB_SET_OVERLAY_LAYER \
	MTK_IOW(0, struct compat_fb_overlay_layer)
#define COMPAT_MTKFB_TRIG_OVERLAY_OUT \
	MTK_IO(1)
#define COMPAT_MTKFB_SET_VIDEO_LAYERS \
	MTK_IOW(2, struct compat_fb_overlay_layer)
#define COMPAT_MTKFB_CAPTURE_FRAMEBUFFER \
	MTK_IOW(3, compat_ulong_t)
#define COMPAT_MTKFB_CONFIG_IMMEDIATE_UPDATE \
	MTK_IOW(4, compat_ulong_t)
#define COMPAT_MTKFB_GET_FRAMEBUFFER_MVA \
	MTK_IOR(26, compat_uint_t)
#define COMPAT_MTKFB_GET_DISPLAY_IF_INFORMATION \
	MTK_IOR(22, struct compat_mtk_dispif_info)
#define COMPAT_MTKFB_GET_POWERSTATE \
	MTK_IOR(21, compat_ulong_t)
#define COMPAT_MTKFB_META_RESTORE_SCREEN \
	MTK_IOW(101, compat_ulong_t)
#define COMPAT_MTKFB_POWERON \
	MTK_IO(12)
#define COMPAT_MTKFB_POWEROFF \
	MTK_IO(13)
#define COMPAT_MTKFB_AEE_LAYER_EXIST \
	MTK_IOR(23, compat_ulong_t)
#define COMPAT_MTKFB_FACTORY_AUTO_TEST \
	MTK_IOR(25, compat_ulong_t)
#define COMPAT_MTKFB_META_SHOW_BOOTLOGO \
	MTK_IO(105)


#endif

#endif /*_COMPAT_MTKFB_H_*/
