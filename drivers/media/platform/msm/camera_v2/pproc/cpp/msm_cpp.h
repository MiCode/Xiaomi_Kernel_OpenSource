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

#ifndef __MSM_CPP_H__
#define __MSM_CPP_H__

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <media/v4l2-subdev.h>
#include "msm_sd.h"

/* hw version info:
  31:28  Major version
  27:16  Minor version
  15:0   Revision bits
**/
#define CPP_HW_VERSION_1_1_0  0x10010000
#define CPP_HW_VERSION_1_1_1  0x10010001
#define CPP_HW_VERSION_2_0_0  0x20000000

#define MAX_ACTIVE_CPP_INSTANCE 8
#define MAX_CPP_PROCESSING_FRAME 2
#define MAX_CPP_V4l2_EVENTS 30

#define MSM_CPP_MICRO_BASE          0x4000
#define MSM_CPP_MICRO_HW_VERSION    0x0000
#define MSM_CPP_MICRO_IRQGEN_STAT   0x0004
#define MSM_CPP_MICRO_IRQGEN_CLR    0x0008
#define MSM_CPP_MICRO_IRQGEN_MASK   0x000C
#define MSM_CPP_MICRO_FIFO_TX_DATA  0x0010
#define MSM_CPP_MICRO_FIFO_TX_STAT  0x0014
#define MSM_CPP_MICRO_FIFO_RX_DATA  0x0018
#define MSM_CPP_MICRO_FIFO_RX_STAT  0x001C
#define MSM_CPP_MICRO_BOOT_START    0x0020
#define MSM_CPP_MICRO_BOOT_LDORG    0x0024
#define MSM_CPP_MICRO_CLKEN_CTL     0x0030

#define MSM_CPP_CMD_GET_BOOTLOADER_VER	0x1
#define MSM_CPP_CMD_FW_LOAD				0x2
#define MSM_CPP_CMD_EXEC_JUMP			0x3
#define MSM_CPP_CMD_RESET_HW			0x5
#define MSM_CPP_CMD_PROCESS_FRAME		0x6
#define MSM_CPP_CMD_FLUSH_STREAM		0x7
#define MSM_CPP_CMD_CFG_MEM_PARAM		0x8
#define MSM_CPP_CMD_ERROR_REQUEST		0x9
#define MSM_CPP_CMD_GET_STATUS			0xA
#define MSM_CPP_CMD_GET_FW_VER			0xB

#define MSM_CPP_MSG_ID_CMD          0x3E646D63
#define MSM_CPP_MSG_ID_OK           0x0A0A4B4F
#define MSM_CPP_MSG_ID_TRAILER      0xABCDEFAA

#define MSM_CPP_MSG_ID_JUMP_ACK     0x00000001
#define MSM_CPP_MSG_ID_FRAME_ACK    0x00000002
#define MSM_CPP_MSG_ID_FRAME_NACK   0x00000003
#define MSM_CPP_MSG_ID_FLUSH_ACK    0x00000004
#define MSM_CPP_MSG_ID_FLUSH_NACK   0x00000005
#define MSM_CPP_MSG_ID_CFG_MEM_ACK  0x00000006
#define MSM_CPP_MSG_ID_CFG_MEM_INV  0x00000007
#define MSM_CPP_MSG_ID_ERROR_STATUS 0x00000008
#define MSM_CPP_MSG_ID_INVALID_CMD  0x00000009
#define MSM_CPP_MSG_ID_GEN_STATUS   0x0000000A
#define MSM_CPP_MSG_ID_FLUSHED      0x0000000B
#define MSM_CPP_MSG_ID_FW_VER       0x0000000C

#define MSM_CPP_JUMP_ADDRESS		0x20
#define MSM_CPP_START_ADDRESS		0x0
#define MSM_CPP_END_ADDRESS			0x3F00

#define MSM_CPP_POLL_RETRIES		20
#define MSM_CPP_TASKLETQ_SIZE		16
#define MSM_CPP_TX_FIFO_LEVEL		16

struct cpp_subscribe_info {
	struct v4l2_fh *vfh;
	uint32_t active;
};

enum cpp_state {
	CPP_STATE_BOOT,
	CPP_STATE_IDLE,
	CPP_STATE_ACTIVE,
	CPP_STATE_OFF,
};

enum msm_queue {
	MSM_CAM_Q_CTRL,     /* control command or control command status */
	MSM_CAM_Q_VFE_EVT,  /* adsp event */
	MSM_CAM_Q_VFE_MSG,  /* adsp message */
	MSM_CAM_Q_V4L2_REQ, /* v4l2 request */
	MSM_CAM_Q_VPE_MSG,  /* vpe message */
	MSM_CAM_Q_PP_MSG,  /* pp message */
};

struct msm_queue_cmd {
	struct list_head list_config;
	struct list_head list_control;
	struct list_head list_frame;
	struct list_head list_pict;
	struct list_head list_vpe_frame;
	struct list_head list_eventdata;
	enum msm_queue type;
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

struct msm_cpp_tasklet_queue_cmd {
	struct list_head list;
	uint32_t irq_status;
	uint32_t tx_fifo[MSM_CPP_TX_FIFO_LEVEL];
	uint32_t tx_level;
	uint8_t cmd_used;
};

struct msm_cpp_buffer_map_info_t {
	unsigned long len;
	unsigned long phy_addr;
	struct ion_handle *ion_handle;
	struct msm_cpp_buffer_info_t buff_info;
};

struct msm_cpp_buffer_map_list_t {
	struct msm_cpp_buffer_map_info_t map_info;
	struct list_head entry;
};

struct msm_cpp_buff_queue_info_t {
	uint32_t used;
	uint16_t session_id;
	uint16_t stream_id;
	struct list_head vb2_buff_head;
	struct list_head native_buff_head;
};

struct msm_cpp_work_t {
	struct work_struct my_work;
	struct cpp_device *cpp_dev;
};

struct cpp_device {
	struct platform_device *pdev;
	struct msm_sd_subdev msm_sd;
	struct v4l2_subdev subdev;
	struct resource *mem;
	struct resource *irq;
	struct resource *io;
	struct resource	*vbif_mem;
	struct resource *vbif_io;
	struct resource	*cpp_hw_mem;
	void __iomem *vbif_base;
	void __iomem *base;
	void __iomem *cpp_hw_base;
	struct clk **cpp_clk;
	struct regulator *fs_cpp;
	struct mutex mutex;
	enum cpp_state state;
	uint8_t is_firmware_loaded;
	char *fw_name_bin;
	struct workqueue_struct *timer_wq;
	struct msm_cpp_work_t *work;
	uint32_t fw_version;
	uint8_t stream_cnt;
	uint8_t timeout_trial_cnt;

	int domain_num;
	struct iommu_domain *domain;
	struct device *iommu_ctx;
	struct ion_client *client;
	struct kref refcount;

	/* Reusing proven tasklet from msm isp */
	atomic_t irq_cnt;
	uint8_t taskletq_idx;
	spinlock_t  tasklet_lock;
	struct list_head tasklet_q;
	struct tasklet_struct cpp_tasklet;
	struct msm_cpp_tasklet_queue_cmd
		tasklet_queue_cmd[MSM_CPP_TASKLETQ_SIZE];

	struct cpp_subscribe_info cpp_subscribe_list[MAX_ACTIVE_CPP_INSTANCE];
	uint32_t cpp_open_cnt;
	struct cpp_hw_info hw_info;

	struct msm_device_queue eventData_q; /* V4L2 Event Payload Queue */

	/* Processing Queue
	 * store frame info for frames sent to microcontroller
	 */
	struct msm_device_queue processing_q;

	struct msm_cpp_buff_queue_info_t *buff_queue;
	uint32_t num_buffq;
	struct v4l2_subdev *buf_mgr_subdev;
};
#endif /* __MSM_CPP_H__ */
