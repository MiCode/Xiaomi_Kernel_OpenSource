/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _UAPI_VIDEO_GSP_CFG_H
#define _UAPI_VIDEO_GSP_CFG_H

#include <linux/types.h>
#include <asm/ioctl.h>
#include <stdbool.h>
#include "drm.h"

/* define ioctl code for gsp */
#define GSP_IO_MAGIC  ('G')

#define GSP_IO_SHIFT  (5)

#define GSP_GET_CAPABILITY_SHIFT  (6)
#define GSP_TRIGGER_SHIFT  (5)
#define GSP_ASYNC_SHIFT  (4)
#define GSP_SPLIT_SHIFT  (3)
#define GSP_CNT_SHIFT  (0)

/*
 * _IO_NR() has 8 bits
 * bit7: MAX_NR indicate max validate code
 * bit6: indicate GSP_GET_CAPABILITY code
 * bit5: indicate GSP_TRIGGER code
 * bit4: indicate whether kcfgs are async
 * bit3: indicate whether kcfgs are split which
 *	 respond to split-case
 * bit2-bit0: indicate how many kcfgs there are
 */
#define GSP_GET_CAPABILITY  (0x1 << GSP_GET_CAPABILITY_SHIFT)
#define GSP_TRIGGER  (0x1 << GSP_TRIGGER_SHIFT)
#define GSP_IO_MASK  (0x7 << GSP_IO_SHIFT)
#define GSP_ASYNC_MASK (0x1 << GSP_ASYNC_SHIFT)
#define GSP_SPLIT_MASK (0x1 << GSP_SPLIT_SHIFT)
#define GSP_CNT_MASK (0x7 << GSP_CNT_SHIFT)

#define GSP_IO_GET_CAPABILITY(size)  \
_IOWR(GSP_IO_MAGIC, GSP_GET_CAPABILITY, size)

#define GSP_IO_TRIGGER(async, cnt, split, size)  \
{\
_IOWR(GSP_IO_MAGIC,\
GSP_TRIGGER | (async) << GSP_ASYNC_SHIFT |\
(split) << GSP_SPLIT_SHIFT | (cnt) << GSP_CNT_SHIFT,\
size)\
}

#define GSP_CAPABILITY_MAGIC  0xDEEFBEEF

enum gsp_layer_type {
	GSP_IMG_LAYER,
	GSP_OSD_LAYER,
	GSP_DES_LAYER,
	GSP_INVAL_LAYER
};

/*the address type of gsp can process*/
enum gsp_addr_type {
	GSP_ADDR_TYPE_INVALUE,
	GSP_ADDR_TYPE_PHYSICAL,
	GSP_ADDR_TYPE_IOVIRTUAL,
	GSP_ADDR_TYPE_MAX,
};

enum gsp_irq_mod {
	GSP_IRQ_MODE_PULSE = 0x00,
	GSP_IRQ_MODE_LEVEL,
	GSP_IRQ_MODE_LEVEL_INVALID,
};

enum gsp_irq_type {
	GSP_IRQ_TYPE_DISABLE = 0x00,
	GSP_IRQ_TYPE_ENABLE,
	GSP_IRQ_TYPE_INVALID,
};

enum gsp_rot_angle {
	GSP_ROT_ANGLE_0 = 0x00,
	GSP_ROT_ANGLE_90,
	GSP_ROT_ANGLE_180,
	GSP_ROT_ANGLE_270,
	GSP_ROT_ANGLE_0_M,
	GSP_ROT_ANGLE_90_M,
	GSP_ROT_ANGLE_180_M,
	GSP_ROT_ANGLE_270_M,
	GSP_ROT_ANGLE_MAX_NUM,
};

struct gsp_rgb {
	__u8 b_val;
	__u8 g_val;
	__u8 r_val;
	__u8 a_val;
};

struct gsp_pos {
	__u16 pt_x;
	__u16 pt_y;
};

struct gsp_rect {
	__u16 st_x;
	__u16 st_y;
	__u16 rect_w;
	__u16 rect_h;
};

struct gsp_addr_data {
	__u32 addr_y;
	__u32 addr_uv;
	__u32 addr_va;
};

struct gsp_offset {
	__u32 uv_offset;
	__u32 v_offset;
};

struct gsp_yuv_adjust_para {
	__u32 y_brightness;
	__u32 y_contrast;
	__u32 u_offset;
	__u32 u_saturation;
	__u32 v_offset;
	__u32 v_saturation;
};

struct gsp_background_para {
	__u32 bk_enable;
	__u32 bk_blend_mod;
	struct gsp_rgb		 background_rgb;
};

struct gsp_scale_para {
	__u32 scale_en;
	__u32 htap_mod;
	__u32 vtap_mod;
	struct gsp_rect		scale_rect_in;
	struct gsp_rect		scale_rect_out;
};
/*
 * to distinguish struct from uapi gsp cfg header file
 * and no uapi gsp cfg header file. structure at uapi
 * header file has suffix "_user"
 */

struct gsp_layer_user {
	__u32 type;
	__u32 enable;
	__s32 share_fd;
	__s32 wait_fd;
	__s32 sig_fd;
	struct gsp_addr_data src_addr;
	struct gsp_offset offset;
};


#define CAPABILITY_MAGIC_NUMBER 0xDEEFBEEF
struct gsp_capability {
	/*used to indicate struct is initialized*/
	__u32 magic;
	char version[32];

	__u32 capa_size;
	__u32 io_cnt;
	__u32 core_cnt;

	__u32 max_layer;
	__u32 max_img_layer;

	struct gsp_rect crop_max;
	struct gsp_rect crop_min;
	struct gsp_rect out_max;
	struct gsp_rect out_min;

	/* GSP_ADDR_TYPE_PHYSICAL:phy addr
	 * GSP_ADDR_TYPE_IOVIRTUAL:iova addr
	 */
	__u32 buf_type;
};

#endif
