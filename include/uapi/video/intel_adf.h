/*
 * INTEL CONFIDENTIAL
 *
 * Copyright 2014
 * Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and treaty
 * provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or disclosed
 * in any way without Intels prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel
 * or otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */

#ifndef _UAPI_VIDEO_INTEL_ADF_H_
#define _UAPI_VIDEO_INTEL_ADF_H_

#include <linux/types.h>

/*
 * This identifier should be increased every time the contents of the this file
 * change in any manner that could require detection from the client. This
 * enables a kernel to reject any clients that are too old. Note that a failure
 * to match compatible versions should result in the user mode falling back to
 * the baseline simple post.
 */
#define INTEL_ADF_VERSION 3

struct intel_adf_rect {
	__s16 x;
	__s16 y;
	__u16 w;
	__u16 h;
};

struct intel_adf_rect_fixedpoint {
	__s32 x;
	__s32 y;
	__u32 w;
	__u32 h;
};

enum intel_adf_compression_mode {
	INTEL_ADF_UNCOMPRESSED,
	INTEL_ADF_COMPRESSED,
};

enum intel_adf_blending_type {
	INTEL_ADF_BLENDING_NONE,
	INTEL_ADF_BLENDING_PREMULT,
	INTEL_ADF_BLENDING_COVERAGE,
};

enum intel_adf_transform_mode {
	INTEL_ADF_TRANSFORM_NONE	= 0,
	INTEL_ADF_TRANSFORM_FLIPH	= 1,
	INTEL_ADF_TRANSFORM_FLIPV	= 2,
	INTEL_ADF_TRANSFORM_ROT90	= 4,

	/* INTEL_ADF_TRANSFORM_FLIPH | INTEL_ADF_TRANSFORM_FLIPV; */
	INTEL_ADF_TRANSFORM_ROT180	= 3,

	/*
	 * INTEL_ADF_TRANSFORM_FLIPH | INTEL_ADF_TRANSFORM_FLIPV |
	 * INTEL_ADF_TRANSFORM_ROT90;
	 */
	INTEL_ADF_TRANSFORM_ROT270	= 7,
};

enum intel_adf_colorspace {
	INTEL_ADF_COLORSPACE_UNSPECIFIED,
	INTEL_ADF_COLORSPACE_JFIF,
	INTEL_ADF_COLORSPACE_BT601_625,
	INTEL_ADF_COLORSPACE_BT601_525,
	INTEL_ADF_COLORSPACE_BT709,
	INTEL_ADF_COLORSPACE_BT2020,
};

enum intel_adf_plane_flags {
	INTEL_ADF_PLANE_DISABLE		= 0x00000000,
	INTEL_ADF_PLANE_ENABLE		= 0x00000001,

	/* User hint that this plane is unchanged since the last post */
	INTEL_ADF_PLANE_HINT_UNCHANGED	= 0x00000002,

	/* Flags reserved for any future hardware usage */
	INTEL_ADF_PLANE_HW_PRIVATE_1	= 0x10000000,
	INTEL_ADF_PLANE_HW_PRIVATE_2	= 0x20000000,
	INTEL_ADF_PLANE_HW_PRIVATE_3	= 0x40000000,
	INTEL_ADF_PLANE_HW_PRIVATE_4	= 0x80000000,
};

/* Configuration for describing an overlay plane displaying a buffer */
struct intel_adf_plane {
	/* Combination of flags from the intel_adf_plane_flags enum */
	__u32 flags;

	/* index of the overlay engine for this config */
	__u8 overlay_id;

	/*
	 * Index of the buffer to put on this plane
	 * (within adf_post_config.bufs array)
	 */
	__u8 buffer_id;

	/* Constant Alpha value */
	__u16 alpha;

	struct intel_adf_rect dst;
	struct intel_adf_rect_fixedpoint src;
	enum intel_adf_blending_type blending:8;
	enum intel_adf_transform_mode transform:8;
	enum intel_adf_compression_mode compression:8;
	enum intel_adf_colorspace colorspace:8;
};

/* Configuration for describing a constant color plane */
struct intel_adf_color {
	/* Placeholder for future flags. Set to 0 */
	__u32 flags;

	/* device specific color format */
	__u32 color;

	struct intel_adf_rect dst;
};

enum intel_adf_panelfitter_flags {
	/* Panel fitter fill modes */
	INTEL_ADF_PANELFITTER_DISABLE		= 0x00000000,
	INTEL_ADF_PANELFITTER_AUTO		= 0x00000001,
	INTEL_ADF_PANELFITTER_LETTERBOX		= 0x00000002,
	INTEL_ADF_PANELFITTER_PILLARBOX		= 0x00000003,

	/* Hint that the panel fitter state is unchanged since the last post */
	INTEL_ADF_PANELFITTER_HINT_UNCHANGED	= 0x08000000,

	/* Flags reserved for any future hardware usage */
	INTEL_ADF_PANELFITTER_HW_PRIVATE_1	= 0x10000000,
	INTEL_ADF_PANELFITTER_HW_PRIVATE_2	= 0x20000000,
	INTEL_ADF_PANELFITTER_HW_PRIVATE_3	= 0x40000000,
	INTEL_ADF_PANELFITTER_HW_PRIVATE_4	= 0x80000000,
};

/* Configuration for describing a panel fitter */
struct intel_adf_panelfitter {
	/* Combination of flags from the intel_adf_panelfitter_flags enum */
	__u32 flags;

	struct intel_adf_rect dst;
	struct intel_adf_rect src;
};

enum intel_adf_config_type {
	INTEL_ADF_CONFIG_PLANE,
	INTEL_ADF_CONFIG_COLOR,
	INTEL_ADF_CONFIG_PANELFITTER,
};

struct intel_adf_config {
	/* Type of this config entry */
	enum intel_adf_config_type type:8;

	/* index of the interface being updated by this config */
	__u8 interface_id;

	/* Reserved for future. Initialise to 0 */
	__u16 pad;

	union {
		struct intel_adf_color color;
		struct intel_adf_plane plane;
		struct intel_adf_panelfitter panelfitter;
	};
};

enum intel_adf_post_flags {
	INTEL_ADF_POST_NO_FLAGS		= 0x00000000,

	/* Call the custom page flip handler when this flip is completed. */
	INTEL_ADF_POST_FLIPCALLBACK	= 0x00000001,

	/* Apply this immediately, without waiting for vsync */
	INTEL_ADF_POST_IMMEDIATE	= 0x00000002,

	/* Discard anything currently queued and make this flip on next vsync */
	INTEL_ADF_POST_DISCARDQUEUE	= 0x00000004,

	/*
	 * If set, this is a partial update specifying only a subset of planes,
	 * the unspecified planes should remain unchanged.
	 * If not set, any plane not specified should be turned off.
	 */
	INTEL_ADF_POST_PARTIAL		= 0x00000008,

	/* Flags reserved for any future hardware usage */
	INTEL_ADF_POST_HW_PRIVATE_1	= 0x10000000,
	INTEL_ADF_POST_HW_PRIVATE_2	= 0x20000000,
	INTEL_ADF_POST_HW_PRIVATE_3	= 0x40000000,
	INTEL_ADF_POST_HW_PRIVATE_4	= 0x80000000,
};

struct intel_adf_post_custom_data {
	/* INTEL_ADF_VERSION for backwards compatibility support. */
	__u32 version;

	/* Combination of flags from the intel_adf_post_flags enum */
	__u32 flags;

	/* Device specific number that describe how the layers are ordered. */
	__u32 zorder;

	/* Number of configuration entries */
	__u32 n_configs;

	/* configuration entries will follow this structure in memory */
	struct intel_adf_config configs[0];
};

/*
 * Color manager custom command
 * This is required to parse color properties
 * More details in intel_color_manager.*
 */
struct __attribute__((packed, aligned(16))) color_cmd {
	u8 property;
	u8 action;
	u16 size;
	u64 data_ptr;
};

/* Color correction IOCTL */
#define INTEL_ADF_COLOR_MANAGER_SET	\
	_IOW(ADF_IOCTL_TYPE, ADF_IOCTL_NR_CUSTOM + 1, \
		struct color_cmd)
#define INTEL_ADF_COLOR_MANAGER_GET	\
	_IOR(ADF_IOCTL_TYPE, ADF_IOCTL_NR_CUSTOM + 2, \
		struct color_cmd)

#endif /* _UAPI_VIDEO_INTEL_ADF_H_ */
