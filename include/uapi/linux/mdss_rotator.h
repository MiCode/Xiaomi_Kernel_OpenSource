/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _UAPI_MDSS_ROTATOR_H_
#define _UAPI_MDSS_ROTATOR_H_

#include <linux/msm_mdp_ext.h>

#define MDSS_ROTATOR_IOCTL_MAGIC 'w'

/* open a rotation session */
#define MDSS_ROTATION_OPEN \
	_IOWR(MDSS_ROTATOR_IOCTL_MAGIC, 1, struct mdp_rotation_config *)

/* change the rotation session configuration */
#define MDSS_ROTATION_CONFIG \
	_IOWR(MDSS_ROTATOR_IOCTL_MAGIC, 2, struct mdp_rotation_config *)

/* queue the rotation request */
#define MDSS_ROTATION_REQUEST \
	_IOWR(MDSS_ROTATOR_IOCTL_MAGIC, 3, struct mdp_rotation_request *)

/* close a rotation session with the specified rotation session ID */
#define MDSS_ROTATION_CLOSE	_IOW(MDSS_ROTATOR_IOCTL_MAGIC, 4, unsigned int)

/*
 * Rotation request flag
 */
/* no rotation flag, i.e. color space conversion */
#define MDP_ROTATION_NOP	0x01

/* left/right flip */
#define MDP_ROTATION_FLIP_LR	0x02

/* up/down flip */
#define MDP_ROTATION_FLIP_UD	0x04

/* rotate 90 degree */
#define MDP_ROTATION_90		0x08

/* rotate 180 degre */
#define MDP_ROTATION_180	(MDP_ROTATION_FLIP_LR | MDP_ROTATION_FLIP_UD)

/* rotate 270 degree */
#define MDP_ROTATION_270	(MDP_ROTATION_90 | MDP_ROTATION_180)

/* format is interlaced */
#define MDP_ROTATION_DEINTERLACE 0x10

/* enable bwc */
#define MDP_ROTATION_BWC_EN	0x40

/* secure data */
#define MDP_ROTATION_SECURE	0x80

/*
 * Rotation commit flag
 */
/* Flag indicates to validate the rotation request */
#define MDSS_ROTATION_REQUEST_VALIDATE	0x01

#define MDP_ROTATION_REQUEST_VERSION_1_0	0x00010000

/*
 * Client can let driver to allocate the hardware resources with
 * this particular hw resource id.
 */
#define MDSS_ROTATION_HW_ANY	0xFFFFFFFF

/*
 * Configuration Structures
 */
struct mdp_rotation_buf_info {
	uint32_t width;
	uint32_t height;
	uint32_t format;
	struct mult_factor comp_ratio;
};

struct mdp_rotation_config {
	uint32_t	version;
	uint32_t	session_id;
	struct mdp_rotation_buf_info	input;
	struct mdp_rotation_buf_info	output;
	uint32_t	frame_rate;
	uint32_t	flags;
	uint32_t	reserved[6];
};

struct mdp_rotation_item {
	/* rotation request flag */
	uint32_t	flags;

	/* Source crop rectangle */
	struct mdp_rect	src_rect;

	/* Destination rectangle */
	struct mdp_rect	dst_rect;

	/* Input buffer for the request */
	struct mdp_layer_buffer	input;

	/* The output buffer for the request */
	struct mdp_layer_buffer	output;

	/*
	 * DMA pipe selection for this request by client:
	 * 0: DMA pipe 0
	 * 1: DMA pipe 1
	 * or MDSS_ROTATION_HW_ANY if client wants
	 * driver to allocate any that is available
	 */
	uint32_t	pipe_idx;

	/*
	 * Write-back block selection for this request by client:
	 * 0: Write-back block 0
	 * 1: Write-back block 1
	 * or MDSS_ROTATION_HW_ANY if client wants
	 * driver to allocate any that is available
	 */
	uint32_t	wb_idx;

	/* Which session ID is this request scheduled on */
	uint32_t	session_id;

	/* 32bits reserved value for future usage */
	uint32_t	reserved[6];
};

struct mdp_rotation_request {
	/* 32bit version indicates the request structure */
	uint32_t	version;

	uint32_t	flags;

	/* Number of rotation request items in the list */
	uint32_t	count;

	/* Pointer to a list of rotation request items */
	struct mdp_rotation_item __user	*list;

	/* 32bits reserved value for future usage*/
	uint32_t	reserved[6];
};

#endif /*_UAPI_MDSS_ROTATOR_H_*/
