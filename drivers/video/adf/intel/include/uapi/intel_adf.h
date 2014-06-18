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
 */

#ifndef _VIDEO_INTEL_ADF_H_
#define _VIDEO_INTEL_ADF_H_

/*
 * This identifier should be increased every time the contents of the this file
 * change in any manner that could require detection from the client. This
 * enables a kernel to reject any clients that are too old. Note that a failure
 * to match compatible versions should result in the user mode falling back to
 * the baseline simple post.
 */
#define INTEL_ADF_VERSION 1

enum intel_adf_compression {
	INTEL_ADF_UNCOMPRESSED,
	INTEL_ADF_COMPRESSED,
};

enum intel_adf_blending {
	INTEL_ADF_BLENDING_NONE,
	INTEL_ADF_BLENDING_PREMULT,
	INTEL_ADF_BLENDING_COVERAGE,
};

enum intel_adf_transform {
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

/**
 * intel_adf_color
 * overlay_id:	index of the overlay engine
 * zorder:	depth of the layer ion screen, lower is further back
 * reserved:	set reserved bit
 * color:	color
 */
struct intel_adf_color {
	__u8	overlay_id;
	__u8	zorder;
	__u16	reserved;
	__u32	color;
};

enum intel_adf_plane_flags {
	INTEL_ADF_PLANE_DISABLE		= 0x0001,
	/* User indication that this plane is unchanged since the last post */
	INTEL_ADF_PLANE_UNCHANGED	= 0x0002,
	/* Flags for any undefined HW specific usage */
	INTEL_ADF_PLANE_HWSPECIFIC1	= 0x1000,
	INTEL_ADF_PLANE_HWSPECIFIC2	= 0x2000,
	INTEL_ADF_PLANE_HWSPECIFIC3	= 0x4000,
	INTEL_ADF_PLANE_HWSPECIFIC4	= 0x8000,
};

/**
 * struct intel_adf_plane - intel plane structure for adf
 * overlay_id:	index of the overlay engine for this plane
 * interface_id:index of interface
 * buffer_id:	Index of the buffer to put on this plane
 *		(within adf_post_config.bufs array)
 * flags:	flags
 * dst_x:	destination left
 * dst_y:	destination top
 * dst_w:	destination width
 * dst_h:	destination_height
 * src_x:	16.16 fixed point source left
 * src_y:	16.16 fixed point source top
 * src_w:	16.16 fixed point source width
 * src_h:	16.16 fixed point source height
 * alpha:	constant alpha value
 * compression:	compiression mode
 * blending:	blending mode
 * transform:	transform mode
 * pad:		struct padding, value is always zero.
 */
struct intel_adf_plane {
	/*
	 * NOTE: this field might be useless, already have
	 * overlay engine assigned to adf_buffer
	 **/
	__u8	overlay_id;
	__u8	inteface_id;
	__u8	buffer_id;
	__u16	flags;
	__s16	dst_x;
	__s16	dst_y;
	__u16	dst_w;
	__u16	dst_h;
	__s32	src_x;
	__s32	src_y;
	__u32	src_w;
	__u32	src_h;
	__u8	alpha;
	enum intel_adf_compression	compression:4;
	enum intel_adf_blending		blending:4;
	enum intel_adf_transform	transform:4;
	__u8	pad:4;
};

enum intel_adf_pfitter_flags {
	INTEL_ADF_PFIT_DISABLE		= 0x0001,
	INTEL_ADF_PFIT_AUTO		= 0x0002,
	INTEL_ADF_PFIT_LETTERBOX	= 0x0004,
	INTEL_ADF_PFIT_PILLARBOX	= 0x0008,
	/* Flags for any undefined HW specific usage */
	INTEL_ADF_PFIT_HWSPECIFIC1	= 0x1000,
	INTEL_ADF_PFIT_HWSPECIFIC2	= 0x2000,
	INTEL_ADF_PFIT_HWSPECIFIC3	= 0x4000,
	INTEL_ADF_PFIT_HWSPECIFIC4	= 0x8000,
};

/**
 * struct intel_adf_panelfitter
 * overlay_id:	index of the overlay engine for panel fitter
 * pad:		for structure padding, should be zero
 * flags:	flags
 * dst_x:	destination left
 * dst_y:	destination top
 * dst_w:	destination width
 * dst_h:	destination height
 * src_x:	source left
 * src_y:	source top
 * src_w:	source width
 * src_h:	source height
 */
struct intel_adf_panelfitter {
	__u8	verlay_id;
	__u8	pad;
	__u16	flags;
	__s16	dst_x;
	__s16	dst_y;
	__u16	dst_w;
	__u16	dst_h;
	__s16	src_x;
	__s16	src_y;
	__u16	src_w;
	__u16	src_h;
};

/*
 * All the overlay structures begin with a _u8 overlay_id element.
 * The type of the specified overlay_engine will determine the type
 * of this union
 */
struct intel_adf_overlay {
	union {
		__u8	overlay_id;
		struct intel_adf_color		color;
		struct intel_adf_plane		plane;
		struct intel_adf_panelfitter	panelfitter;
	};
};

enum intel_adf_post_flags {
	/* Call the custom page flip handler when this flip has completed */
	INTEL_ADF_POST_FLIPCALLBACK	= 0x0001,
	/* Apply this immediately, without waiting for vsyn c*/
	INTEL_ADF_POST_IMMEDIATE	= 0x0002,
	/*
	 * Discard anything currently queued and make this flip happen on
	 * the next vsync
	 */
	INTEL_ADF_POST_DISCARDQUEUE	= 0x0004,
	/* Flags for any undefined HW specific usage */
	INTEL_ADF_POST_HWSPECIFIC	= 0xF000,
};

/**
 * intel_adf_post_custom_data
 * version:	INTEL_ADF_VERSION for backward compatibility support
 * flags:	adf post flags
 * num_overlays:number of overlays
 * overlays:	overlay entried will follow this structure in memory
 */
struct intel_adf_post_custom_data {
	__u32				version;
	enum intel_adf_post_flags	flags;
	__u32				num_overlays;

	/* Z order (on hardware that supports it) is defined by the order of
	 * these planes entry [0] is backmost, entry [num_overlays-1] is
	 * frontmost
	 */
	struct intel_adf_overlay	overlays[0];
};

#endif /* _VIDEO_INTEL_ADF_H_ */
