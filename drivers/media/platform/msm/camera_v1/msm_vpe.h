/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MSM_VPE_H_
#define _MSM_VPE_H_

#include <mach/camera.h>

/***********  start of register offset *********************/
#define VPE_INTR_ENABLE_OFFSET                0x0020
#define VPE_INTR_STATUS_OFFSET                0x0024
#define VPE_INTR_CLEAR_OFFSET                 0x0028
#define VPE_DL0_START_OFFSET                  0x0030
#define VPE_HW_VERSION_OFFSET                 0x0070
#define VPE_SW_RESET_OFFSET                   0x0074
#define VPE_AXI_RD_ARB_CONFIG_OFFSET          0x0078
#define VPE_SEL_CLK_OR_HCLK_TEST_BUS_OFFSET   0x007C
#define VPE_CGC_EN_OFFSET                     0x0100
#define VPE_CMD_STATUS_OFFSET                 0x10008
#define VPE_PROFILE_EN_OFFSET                 0x10010
#define VPE_PROFILE_COUNT_OFFSET              0x10014
#define VPE_CMD_MODE_OFFSET                   0x10060
#define VPE_SRC_SIZE_OFFSET                   0x10108
#define VPE_SRCP0_ADDR_OFFSET                 0x1010C
#define VPE_SRCP1_ADDR_OFFSET                 0x10110
#define VPE_SRC_YSTRIDE1_OFFSET               0x1011C
#define VPE_SRC_FORMAT_OFFSET                 0x10124
#define VPE_SRC_UNPACK_PATTERN1_OFFSET        0x10128
#define VPE_OP_MODE_OFFSET                    0x10138
#define VPE_SCALE_PHASEX_INIT_OFFSET          0x1013C
#define VPE_SCALE_PHASEY_INIT_OFFSET          0x10140
#define VPE_SCALE_PHASEX_STEP_OFFSET          0x10144
#define VPE_SCALE_PHASEY_STEP_OFFSET          0x10148
#define VPE_OUT_FORMAT_OFFSET                 0x10150
#define VPE_OUT_PACK_PATTERN1_OFFSET          0x10154
#define VPE_OUT_SIZE_OFFSET                   0x10164
#define VPE_OUTP0_ADDR_OFFSET                 0x10168
#define VPE_OUTP1_ADDR_OFFSET                 0x1016C
#define VPE_OUT_YSTRIDE1_OFFSET               0x10178
#define VPE_OUT_XY_OFFSET                     0x1019C
#define VPE_SRC_XY_OFFSET                     0x10200
#define VPE_SRC_IMAGE_SIZE_OFFSET             0x10208
#define VPE_SCALE_CONFIG_OFFSET               0x10230
#define VPE_DEINT_STATUS_OFFSET               0x30000
#define VPE_DEINT_DECISION_OFFSET             0x30004
#define VPE_DEINT_COEFF0_OFFSET               0x30010
#define VPE_SCALE_STATUS_OFFSET               0x50000
#define VPE_SCALE_SVI_PARAM_OFFSET            0x50010
#define VPE_SCALE_SHARPEN_CFG_OFFSET          0x50020
#define VPE_SCALE_COEFF_LSP_0_OFFSET          0x50400
#define VPE_SCALE_COEFF_MSP_0_OFFSET          0x50404

#define VPE_AXI_ARB_1_OFFSET                  0x00408
#define VPE_AXI_ARB_2_OFFSET                  0x0040C

#define VPE_SCALE_COEFF_LSBn(n)	(0x50400 + 8 * (n))
#define VPE_SCALE_COEFF_MSBn(n)	(0x50404 + 8 * (n))
#define VPE_SCALE_COEFF_NUM			32

/*********** end of register offset ********************/


#define VPE_HARDWARE_VERSION          0x00080308
#define VPE_SW_RESET_VALUE            0x00000010  /* bit 4 for PPP*/
#define VPE_AXI_RD_ARB_CONFIG_VALUE   0x124924
#define VPE_CMD_MODE_VALUE            0x1
#define VPE_DEFAULT_OP_MODE_VALUE     0x40FC0004
#define VPE_CGC_ENABLE_VALUE          0xffff
#define VPE_DEFAULT_SCALE_CONFIG      0x3c

#define VPE_NORMAL_MODE_CLOCK_RATE   150000000
#define VPE_TURBO_MODE_CLOCK_RATE   200000000


/**************************************************/
/*********** End of command id ********************/
/**************************************************/

enum vpe_state {
	VPE_STATE_IDLE,
	VPE_STATE_INIT,
	VPE_STATE_ACTIVE,
};

struct vpe_ctrl_type {
	spinlock_t        lock;
	uint32_t          irq_status;
	void              *syncdata;
	uint16_t          op_mode;
	void              *extdata;
	uint32_t          extlen;
	struct msm_vpe_callback *resp;
	uint32_t          out_h;  /* this is BEFORE rotation. */
	uint32_t          out_w;  /* this is BEFORE rotation. */
	struct timespec   ts;
	int               output_type;
	int               frame_pack;
	uint8_t           pad_2k_bool;
	enum vpe_state    state;
	unsigned long     out_y_addr;
	unsigned long     out_cbcr_addr;
	struct v4l2_subdev subdev;
	struct platform_device *pdev;
	struct resource   *vpeirq;
	void __iomem      *vpebase;
	struct resource	  *vpemem;
	struct resource   *vpeio;
	void        *device_extdata;
	struct regulator *fs_vpe;
	struct clk	*vpe_clk[2];
	struct msm_mctl_pp_frame_info *pp_frame_info;
};

/*
* vpe_input_update
*
* Define the parameters for output plane
*/
/* this is the dimension of ROI.  width / height. */
struct vpe_src_size_packed {
	uint32_t        src_w;
	uint32_t        src_h;
};

struct vpe_src_xy_packed {
	uint32_t        src_x;
	uint32_t        src_y;
};

struct vpe_input_plane_update_type {
	struct vpe_src_size_packed             src_roi_size;
	/* crop updates this set. */
	struct vpe_src_xy_packed               src_roi_offset;
	/* input address*/
	uint8_t                         *src_p0_addr;
	uint8_t                         *src_p1_addr;
};

struct vpe_msg_stats {
	uint32_t    buffer;
	uint32_t    frameCounter;
};

struct vpe_msg_output {
	uint8_t   output_id;
	uint32_t  yBuffer;
	uint32_t  cbcrBuffer;
	uint32_t  frameCounter;
};

struct vpe_message {
	uint8_t  _d;
	union {
		struct vpe_msg_output              msgOut;
		struct vpe_msg_stats               msgStats;
	} _u;
};

#define SCALER_PHASE_BITS 29
#define HAL_MDP_PHASE_STEP_2P50    0x50000000
#define HAL_MDP_PHASE_STEP_1P66    0x35555555
#define HAL_MDP_PHASE_STEP_1P25    0x28000000

struct phase_val_t {
	int32_t phase_init_x;
	int32_t phase_init_y;
	int32_t phase_step_x;
	int32_t phase_step_y;
};

#define VIDIOC_MSM_VPE_INIT \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 15, struct msm_cam_media_controller *)

#define VIDIOC_MSM_VPE_RELEASE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 16, struct msm_cam_media_controller *)

#define VIDIOC_MSM_VPE_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 17, struct msm_mctl_pp_params *)

#endif /*_MSM_VPE_H_*/

