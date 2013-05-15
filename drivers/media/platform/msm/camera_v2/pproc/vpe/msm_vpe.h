/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_VPE_H__
#define __MSM_VPE_H__

#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-subdev.h>
#include "msm_sd.h"

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
#define VPE_TURBO_MODE_CLOCK_RATE    200000000
#define VPE_SUBDEV_MAX_EVENTS        30

/**************************************************/
/*********** End of command id ********************/
/**************************************************/

#define SCALER_PHASE_BITS 29
#define HAL_MDP_PHASE_STEP_2P50    0x50000000
#define HAL_MDP_PHASE_STEP_1P66    0x35555555
#define HAL_MDP_PHASE_STEP_1P25    0x28000000


#define MAX_ACTIVE_VPE_INSTANCE 8
#define MAX_VPE_PROCESSING_FRAME 2
#define MAX_VPE_V4l2_EVENTS 30

#define MSM_VPE_TASKLETQ_SIZE		16

/**
 * The format of the msm_vpe_transaction_setup_cfg is as follows:
 *
 * - vpe_update_scale_coef (65*4 uint32_t's)
 *   - Each table is 65 uint32_t's long
 *   - 1st uint32_t in each table indicates offset
 *   - Following 64 uint32_t's are the data
 *
 * - vpe_input_plane_config (6 uint32_t's)
 *   - VPE_SRC_FORMAT_OFFSET
 *   - VPE_SRC_UNPACK_PATTERN1_OFFSET
 *   - VPE_SRC_IMAGE_SIZE_OFFSET
 *   - VPE_SRC_YSTRIDE1_OFFSET
 *   - VPE_SRC_SIZE_OFFSET
 *   - VPE_SRC_XY_OFFSET
 *
 * - vpe_output_plane_config (5 uint32_t's)
 *   - VPE_OUT_FORMAT_OFFSET
 *   - VPE_OUT_PACK_PATTERN1_OFFSET
 *   - VPE_OUT_YSTRIDE1_OFFSET
 *   - VPE_OUT_SIZE_OFFSET
 *   - VPE_OUT_XY_OFFSET
 *
 * - vpe_operation_config (1 uint32_t)
 *   - VPE_OP_MODE_OFFSET
 *
 */

#define VPE_SCALER_CONFIG_LEN           260
#define VPE_INPUT_PLANE_CFG_LEN         24
#define VPE_OUTPUT_PLANE_CFG_LEN        20
#define VPE_OPERATION_MODE_CFG_LEN      4
#define VPE_NUM_SCALER_TABLES		4

#define VPE_TRANSACTION_SETUP_CONFIG_LEN (			\
		(VPE_SCALER_CONFIG_LEN * VPE_NUM_SCALER_TABLES)	\
		+ VPE_INPUT_PLANE_CFG_LEN			\
		+ VPE_OUTPUT_PLANE_CFG_LEN			\
		+ VPE_OPERATION_MODE_CFG_LEN)
/* VPE_TRANSACTION_SETUP_CONFIG_LEN = 1088 */

struct msm_vpe_transaction_setup_cfg {
	uint8_t scaler_cfg[VPE_TRANSACTION_SETUP_CONFIG_LEN];
};

struct vpe_subscribe_info {
	struct v4l2_fh *vfh;
	uint32_t active;
};

enum vpe_state {
	VPE_STATE_BOOT,
	VPE_STATE_IDLE,
	VPE_STATE_ACTIVE,
	VPE_STATE_OFF,
};

struct msm_queue_cmd {
	struct list_head list_config;
	struct list_head list_control;
	struct list_head list_frame;
	struct list_head list_pict;
	struct list_head list_vpe_frame;
	struct list_head list_eventdata;
	void *command;
	atomic_t on_heap;
	struct timespec ts;
	uint32_t error_code;
	uint32_t trans_code;
};

struct msm_device_queue {
	struct list_head list;
	spinlock_t lock;
	wait_queue_head_t wait;
	int max;
	int len;
	const char *name;
};

struct msm_vpe_tasklet_queue_cmd {
	struct list_head list;
	uint32_t irq_status;
	uint8_t cmd_used;
};

struct msm_vpe_buffer_map_info_t {
	unsigned long len;
	unsigned long phy_addr;
	struct ion_handle *ion_handle;
	struct msm_vpe_buffer_info_t buff_info;
};

struct msm_vpe_buffer_map_list_t {
	struct msm_vpe_buffer_map_info_t map_info;
	struct list_head entry;
};

struct msm_vpe_buff_queue_info_t {
	uint32_t used;
	uint16_t session_id;
	uint16_t stream_id;
	struct list_head vb2_buff_head;
	struct list_head native_buff_head;
};

struct vpe_device {
	struct platform_device *pdev;
	struct msm_sd_subdev msm_sd;
	struct v4l2_subdev subdev;
	struct resource *mem;
	struct resource *irq;
	void __iomem *base;
	struct clk **vpe_clk;
	struct regulator *fs_vpe;
	struct mutex mutex;
	enum vpe_state state;

	int domain_num;
	struct iommu_domain *domain;
	struct device *iommu_ctx_src;
	struct device *iommu_ctx_dst;
	struct ion_client *client;
	struct kref refcount;

	/* Reusing proven tasklet from msm isp */
	atomic_t irq_cnt;
	uint8_t taskletq_idx;
	spinlock_t  tasklet_lock;
	struct list_head tasklet_q;
	struct tasklet_struct vpe_tasklet;
	struct msm_vpe_tasklet_queue_cmd
	tasklet_queue_cmd[MSM_VPE_TASKLETQ_SIZE];

	struct vpe_subscribe_info vpe_subscribe_list[MAX_ACTIVE_VPE_INSTANCE];
	uint32_t vpe_open_cnt;

	struct msm_device_queue eventData_q; /* V4L2 Event Payload Queue */

	/*
	 * Processing Queue: store frame info for frames sent to
	 * microcontroller
	 */
	struct msm_device_queue processing_q;

	struct msm_vpe_buff_queue_info_t *buff_queue;
	uint32_t num_buffq;
	struct v4l2_subdev *buf_mgr_subdev;
};

#endif /* __MSM_VPE_H__ */
