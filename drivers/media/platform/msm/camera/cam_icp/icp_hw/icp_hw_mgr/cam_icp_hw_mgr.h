/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef CAM_ICP_HW_MGR_H
#define CAM_ICP_HW_MGR_H

#include <linux/types.h>
#include <linux/completion.h>
#include <media/cam_icp.h>
#include "cam_icp_hw_intf.h"
#include "cam_hw_mgr_intf.h"
#include "cam_hw_intf.h"
#include "cam_a5_hw_intf.h"
#include "hfi_session_defs.h"
#include "cam_req_mgr_workq.h"
#include "cam_mem_mgr.h"
#include "cam_smmu_api.h"


#define CAM_ICP_ROLE_PARENT     1
#define CAM_ICP_ROLE_CHILD      2

#define CAM_FRAME_CMD_MAX       20

#define CAM_MAX_OUT_RES         6

#define ICP_WORKQ_NUM_TASK      30
#define ICP_WORKQ_TASK_CMD_TYPE 1
#define ICP_WORKQ_TASK_MSG_TYPE 2

#define ICP_PACKET_SIZE         0
#define ICP_PACKET_TYPE         1
#define ICP_PACKET_OPCODE       2
#define ICP_MAX_OUTPUT_SUPPORTED 6

#define ICP_FRAME_PROCESS_SUCCESS 0
#define ICP_FRAME_PROCESS_FAILURE 1

/**
 * struct icp_hfi_mem_info
 * @qtbl: Memory info of queue table
 * @cmd_q: Memory info of command queue
 * @msg_q: Memory info of message queue
 * @dbg_q: Memory info of debug queue
 * @sec_heap: Memory info of secondary heap
 * @fw_buf: Memory info of firmware
 */
struct icp_hfi_mem_info {
	struct cam_mem_mgr_memory_desc qtbl;
	struct cam_mem_mgr_memory_desc cmd_q;
	struct cam_mem_mgr_memory_desc msg_q;
	struct cam_mem_mgr_memory_desc dbg_q;
	struct cam_mem_mgr_memory_desc sec_heap;
	struct cam_mem_mgr_memory_desc fw_buf;
	struct cam_smmu_region_info shmem;
};

/**
 * struct hfi_cmd_work_data
 * @type: Task type
 * @data: Pointer to command data
 * @request_id: Request id
 */
struct hfi_cmd_work_data {
	uint32_t type;
	void *data;
	int32_t request_id;
};

/**
 * struct hfi_msg_work_data
 * @type: Task type
 * @data: Pointer to message data
 * @irq_status: IRQ status
 */
struct hfi_msg_work_data {
	uint32_t type;
	void *data;
	uint32_t irq_status;
};

/**
 * struct hfi_frame_process_info
 * @hfi_frame_cmd: Frame process command info
 * @bitmap: Bitmap for hfi_frame_cmd
 * @bits: Used in hfi_frame_cmd bitmap
 * @lock: Lock for hfi_frame_cmd
 * @request_id: Request id list
 * @num_out_resources: Number of out syncs
 * @out_resource: Out sync info
 */
struct hfi_frame_process_info {
	struct hfi_cmd_ipebps_async hfi_frame_cmd[CAM_FRAME_CMD_MAX];
	void *bitmap;
	size_t bits;
	struct mutex lock;
	uint64_t request_id[CAM_FRAME_CMD_MAX];
	uint32_t num_out_resources[CAM_FRAME_CMD_MAX];
	uint32_t out_resource[CAM_FRAME_CMD_MAX][CAM_MAX_OUT_RES];
};

/**
 * struct cam_icp_hw_ctx_data
 * @context_priv: Context private data
 * @ctx_mutex: Mutex for context
 * @fw_handle: Firmware handle
 * @scratch_mem_size: Scratch memory size
 * @acquire_dev_cmd: Acquire command
 * @icp_dev_acquire_info: Acquire device info
 * @ctxt_event_cb: Context callback function
 * @in_use: Flag for context usage
 * @role: Role of a context in case of chaining
 * @chain_ctx: Peer context
 * @hfi_frame_process: Frame process command
 * @wait_complete: Completion info
 * @ctx_id: Context Id
 * @temp_payload: Payload for destroy handle data
 */
struct cam_icp_hw_ctx_data {
	void *context_priv;
	struct mutex ctx_mutex;
	uint32_t fw_handle;
	uint32_t scratch_mem_size;
	struct cam_acquire_dev_cmd acquire_dev_cmd;
	struct cam_icp_acquire_dev_info *icp_dev_acquire_info;
	cam_hw_event_cb_func ctxt_event_cb;
	bool in_use;
	uint32_t role;
	struct cam_icp_hw_ctx_data *chain_ctx;
	struct hfi_frame_process_info hfi_frame_process;
	struct completion wait_complete;
	struct ipe_bps_destroy temp_payload;
	uint32_t ctx_id;
};

/**
 * struct cam_icp_hw_mgr
 * @hw_mgr_mutex: Mutex for ICP hardware manager
 * @hw_mgr_lock: Spinlock for ICP hardware manager
 * @devices: Devices of ICP hardware manager
 * @ctx_data: Context data
 * @icp_caps: ICP capabilities
 * @fw_download: Firmware download state
 * @iommu_hdl: Non secure IOMMU handle
 * @iommu_sec_hdl: Secure IOMMU handle
 * @hfi_mem: Memory for hfi
 * @cmd_work: Work queue for hfi commands
 * @msg_work: Work queue for hfi messages
 * @msg_buf: Buffer for message data from firmware
 * @dbg_buf: Buffer for debug data from firmware
 * @a5_complete: Completion info
 * @cmd_work_data: Pointer to command work queue task
 * @msg_work_data: Pointer to message work queue task
 * @ctxt_cnt: Active context count
 * @ipe_ctxt_cnt: IPE Active context count
 * @bps_ctxt_cnt: BPS Active context count
 * @dentry: Debugfs entry
 * @a5_debug: A5 debug flag
 * @icp_pc_flag: Flag to enable/disable power collapse
 */
struct cam_icp_hw_mgr {
	struct mutex hw_mgr_mutex;
	spinlock_t hw_mgr_lock;

	struct cam_hw_intf **devices[CAM_ICP_DEV_MAX];
	struct cam_icp_hw_ctx_data ctx_data[CAM_ICP_CTX_MAX];
	struct cam_icp_query_cap_cmd icp_caps;

	bool fw_download;
	int32_t iommu_hdl;
	int32_t iommu_sec_hdl;
	struct icp_hfi_mem_info hfi_mem;
	struct cam_req_mgr_core_workq *cmd_work;
	struct cam_req_mgr_core_workq *msg_work;
	uint32_t msg_buf[256];
	uint32_t dbg_buf[256];
	struct completion a5_complete;
	struct hfi_cmd_work_data *cmd_work_data;
	struct hfi_msg_work_data *msg_work_data;
	uint32_t ctxt_cnt;
	uint32_t ipe_ctxt_cnt;
	uint32_t bps_ctxt_cnt;
	struct dentry *dentry;
	bool a5_debug;
	bool icp_pc_flag;
};

static int cam_icp_mgr_hw_close(void *hw_priv, void *hw_close_args);
static int cam_icp_mgr_download_fw(void *hw_mgr_priv, void *download_fw_args);
#endif /* CAM_ICP_HW_MGR_H */
