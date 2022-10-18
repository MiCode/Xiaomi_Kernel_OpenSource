/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_CRE_HW_MGR_H
#define CAM_CRE_HW_MGR_H

#include <linux/types.h>
#include <linux/completion.h>
#include <media/cam_cre.h>

#include "cam_cre_hw_intf.h"
#include "cam_hw_mgr_intf.h"
#include "cam_hw_intf.h"
#include "cam_req_mgr_workq.h"
#include "cam_mem_mgr.h"
#include "cam_context.h"
#include "cre_top.h"

#define CRE_CTX_MAX                  32

#define CRE_WORKQ_NUM_TASK           64
#define CRE_WORKQ_TASK_CMD_TYPE      1
#define CRE_WORKQ_TASK_MSG_TYPE      2

#define CRE_PACKET_MAX_CMD_BUFS      1

#define CRE_CTX_STATE_FREE           0
#define CRE_CTX_STATE_IN_USE         1
#define CRE_CTX_STATE_ACQUIRED       2
#define CRE_CTX_STATE_RELEASE        3

#define CRE_MAX_IN_RES               2
#define CRE_MAX_OUT_RES              2
#define CRE_MAX_IO_BUFS              3

#define CAM_CRE_BW_CONFIG_UNKNOWN    0
#define CAM_CRE_BW_CONFIG_V2         2

#define CRE_DEVICE_IDLE_TIMEOUT      400
#define CRE_REQUEST_TIMEOUT          200

#define CAM_CRE_MAX_PER_PATH_VOTES   2
#define CAM_CRE_MAX_REG_SET          32

#define CAM_CRE_MAX_ACTIVE           8
/*
 * Response time threshold in ms beyond which a request is not expected
 * to be with CRE hw
 */
#define CAM_CRE_RESPONSE_TIME_THRESHOLD   300

/*
 * struct cam_cre_irq_data
 *
 * @error:          IRQ error
 * @top_irq_status: CRE top irq status
 * @wr_buf_done:    write engine buf done
 */
struct cam_cre_irq_data {
	uint32_t error;
	uint32_t top_irq_status;
	uint32_t wr_buf_done;
};


/**
 * struct cam_cre_hw_intf_data - CRE hw intf data
 *
 * @Brief:        cre hw intf pointer and pid list data
 *
 * @devices:      cre hw intf pointer
 * @num_devices:  Number of CRE devices
 * @num_hw_pid:   Number of pids for this hw
 * @hw_pid:       cre hw pid values
 *
 */
struct cam_cre_hw_intf_data {
	struct cam_hw_intf  *hw_intf;
	uint32_t             num_hw_pid;
	uint32_t             hw_pid[CRE_DEV_MAX];
};

/**
 * struct cam_cre_ctx_clk_info
 * @curr_fc: Context latest request frame cycles
 * @rt_flag: Flag to indicate real time request
 * @base_clk: Base clock to process the request
 * @reserved: Reserved field
 * @clk_rate: Supported clock rates for the context
 * @num_paths: Number of valid AXI paths
 * @axi_path: ctx based per path bw vote
 */
struct cam_cre_ctx_clk_info {
	uint32_t curr_fc;
	uint32_t rt_flag;
	uint32_t base_clk;
	uint32_t reserved;
	int32_t clk_rate[CAM_MAX_VOTE];
	uint32_t num_paths;
	struct cam_axi_per_path_bw_vote axi_path[CAM_CRE_MAX_PER_PATH_VOTES];
};

/**
 * struct cre_cmd_generic_blob
 * @ctx: Current context info
 * @req_info_idx: Index used for request
 * @io_buf_addr: pointer to io buffer address
 */
struct cre_cmd_generic_blob {
	struct cam_cre_ctx *ctx;
	uint32_t req_idx;
	uint64_t *io_buf_addr;
};

/**
 * struct cam_cre_clk_info
 * @base_clk: Base clock to process request
 * @curr_clk: Current clock of hadrware
 * @threshold: Threshold for overclk count
 * @over_clked: Over clock count
 * @num_paths: Number of AXI vote paths
 * @axi_path: Current per path bw vote info
 * @hw_type: IPE/BPS device type
 * @watch_dog: watchdog timer handle
 * @watch_dog_reset_counter: Counter for watch dog reset
 * @uncompressed_bw: uncompressed BW
 * @compressed_bw: compressed BW
 */
struct cam_cre_clk_info {
	uint32_t base_clk;
	uint32_t curr_clk;
	uint32_t threshold;
	uint32_t over_clked;
	uint32_t num_paths;
	struct cam_axi_per_path_bw_vote axi_path[CAM_CRE_MAX_PER_PATH_VOTES];
	uint32_t hw_type;
	struct cam_req_mgr_timer *watch_dog;
	uint32_t watch_dog_reset_counter;
	uint64_t uncompressed_bw;
	uint64_t compressed_bw;
};

/**
 * struct cre_cmd_work_data
 *
 * @type:       Type of work data
 * @data:       Private data
 * @req_id:     Request Idx
 */
struct cre_cmd_work_data {
	uint32_t type;
	void *data;
	int64_t req_idx;
};

/**
 * struct cre_msg_work_data
 *
 * @type:       Type of work data
 * @data:       Private data
 * @irq_status: IRQ status
 */
struct cre_msg_work_data {
	uint32_t type;
	void *data;
	struct cam_cre_irq_data irq_data;
};

/**
 * struct cre_clk_work_data
 *
 * @type: Type of work data
 * @data: Private data
 */
struct cre_clk_work_data {
	uint32_t type;
	void *data;
};

struct plane_info {
	uintptr_t  cpu_addr;
	dma_addr_t iova_addr;
	uint32_t   width;
	uint32_t   height;
	uint32_t   stride;
	uint32_t   format;
	uint32_t   alignment;
	uint32_t   offset;
	size_t     len;
};

/**
 * struct cre_io_buf
 *
 * @direction:     Direction of a buffer
 * @resource_type: Resource type of IO Buffer
 * @format:     Format
 * @fence:         Fence
 * @num_planes:    Number of planes
 * p_info:         per plane info
 */
struct cre_io_buf {
	uint32_t direction;
	uint32_t resource_type;
	uint32_t format;
	uint32_t fence;
	uint32_t num_planes;
	struct   plane_info p_info[CAM_CRE_MAX_PLANES];
};

struct cre_reg_set {
	uint32_t offset;
	uint32_t value;
};

struct cre_reg_buffer {
	uint32_t num_rd_reg_set;
	uint32_t num_wr_reg_set;
	struct cre_reg_set rd_reg_set[CAM_CRE_MAX_REG_SET];
	struct cre_reg_set wr_reg_set[CAM_CRE_MAX_REG_SET];
};

/**
 * struct cam_cre_clk_bw_request
 * @budget_ns: Time required to process frame
 * @frame_cycles: Frame cycles needed to process the frame
 * @rt_flag: Flag to indicate real time stream
 * @uncompressed_bw: Bandwidth required to process frame
 * @compressed_bw: Compressed bandwidth to process frame
 */
struct cam_cre_clk_bw_request {
	uint64_t budget_ns;
	uint32_t frame_cycles;
	uint32_t rt_flag;
	uint64_t uncompressed_bw;
	uint64_t compressed_bw;
};

/**
 * struct cam_cre_clk_bw_req_internal_v2
 * @budget_ns: Time required to process frame
 * @frame_cycles: Frame cycles needed to process the frame
 * @rt_flag: Flag to indicate real time stream
 * @reserved: Reserved for future use
 * @num_paths: Number of paths for per path bw vote
 * @axi_path: Per path vote info for CRE
 */
struct cam_cre_clk_bw_req_internal_v2 {
	uint64_t budget_ns;
	uint32_t frame_cycles;
	uint32_t rt_flag;
	uint32_t reserved;
	uint32_t num_paths;
	struct cam_axi_per_path_bw_vote axi_path[CAM_CRE_MAX_PER_PATH_VOTES];
};

/**
 * struct cam_cre_request
 *
 * @request_id:          Request Id
 * @req_idx:             Index in request list
 * @state:               Request state
 * @num_batch:           Number of batches
 * @num_frame_bufs:      Number of frame buffers
 * @num_pass_bufs:       Number of pass Buffers
 * @num_io_bufs:         Number of IO Buffers
 * @in_resource:         Input resource
 * @cre_debug_buf:       Debug buffer
 * @io_buf:              IO config info of a request
 * @clk_info:            Clock Info V1
 * @clk_info_v2:         Clock Info V2
 * @hang_data:           Debug data for HW error
 * @submit_timestamp:    Submit timestamp to hw
 */
struct cam_cre_request {
	uint64_t  request_id;
	uint32_t  req_idx;
	uint32_t  state;
	uint32_t  num_batch;
	uint32_t  frames_done;
	uint32_t  num_frame_bufs;
	uint32_t  num_pass_bufs;
	uint32_t  num_io_bufs[CRE_MAX_BATCH_SIZE];
	uint32_t  in_resource;
	struct    cre_reg_buffer cre_reg_buf[CRE_MAX_BATCH_SIZE];
	struct    cre_io_buf *io_buf[CRE_MAX_BATCH_SIZE][CRE_MAX_IO_BUFS];
	struct    cam_cre_clk_bw_request clk_info;
	struct    cam_cre_clk_bw_req_internal_v2 clk_info_v2;
	struct    cam_hw_mgr_dump_pf_data hang_data;
	ktime_t   submit_timestamp;
};

/**
 * struct cam_cre_ctx
 *
 * @ctx_id:            Context ID
 * @ctx_state:         State of a context
 * @req_cnt:           Requests count
 * @last_flush_req:    last flush req for this ctx
 * @last_req_time:     Timestamp of last request
 * @last_req_idx:      Last submitted req index
 * @last_done_req_idx: Last done req index
 * @bitmap:            Context bit map
 * @bitmap_size:       Context bit map size
 * @bits:              Context bit map bits
 * @context_priv:      Private data of context
 * @iommu_hdl:         smmu handle
 * @ctx_mutex:         Mutex for context
 * @acquire_dev_cmd:   Cam acquire command
 * @cre_acquire:       CRE acquire command
 * @clk_info:          CRE Ctx clock info
 * @packet:            Current packet to process
 * @cre_top:           Pointer to CRE top data structure
 * @req_list:          Request List
 * @ctxt_event_cb:     Callback of a context
 */
struct cam_cre_ctx {
	uint32_t ctx_id;
	uint32_t ctx_state;
	uint32_t req_cnt;
	uint64_t last_flush_req;
	uint64_t last_req_time;
	uint64_t last_req_idx;
	uint64_t last_done_req_idx;
	void    *bitmap;
	size_t   bitmap_size;
	size_t   bits;
	void    *context_priv;
	int      iommu_hdl;

	struct mutex                     ctx_mutex;
	struct cam_acquire_dev_cmd       acquire_dev_cmd;
	struct cam_cre_acquire_dev_info  cre_acquire;
	struct cam_cre_ctx_clk_info      clk_info;
	struct cre_top                  *cre_top;
	struct cam_packet               *packet;
	struct cam_cre_request          *req_list[CAM_CTX_REQ_MAX];
	cam_hw_event_cb_func ctxt_event_cb;
};

/**
 * struct cam_cre_hw_mgr
 *
 * @cre_ctx_cnt:       Open context count
 * @hw_mgr_mutex:      Mutex for HW manager
 * @hw_mgr_lock:       Spinlock for HW manager
 * @iommu_hdl:         CRE Handle
 * @iommu_sec_hdl:     CRE Handle for secure
 * @num_cre:           Number of CRE
 * @secure_mode:       Mode of CRE creration
 * @ctx_bitmap:        Context bit map
 * @ctx_bitmap_size:   Context bit map size
 * @ctx_bits:          Context bit map bits
 * @ctx:               CRE context
 * @devices:           CRE devices
 * @cre_caps:          CRE capabilities
 * @cmd_work:          Command work
 * @msg_work:          Message work
 * @timer_work:        Timer work
 * @cmd_work_data:     Command work data
 * @msg_work_data:     Message work data
 * @timer_work_data:   Timer work data
 * @cre_dev_intf:      CRE device interface
 * @clk_info:          CRE clock Info for HW manager
 * @dentry:            Pointer to CRE debugfs directory
 * @dump_req_data_enable: CRE hang dump enablement
 */
struct cam_cre_hw_mgr {
	uint32_t      cre_ctx_cnt;
	struct mutex  hw_mgr_mutex;
	spinlock_t    hw_mgr_lock;
	int32_t       iommu_hdl;
	int32_t       iommu_sec_hdl;
	uint32_t      num_cre;
	uint64_t      cre_debug_clk;
	bool          secure_mode;
	void    *ctx_bitmap;
	size_t   ctx_bitmap_size;
	size_t   ctx_bits;
	struct   cam_cre_ctx  ctx[CRE_CTX_MAX];
	struct   cam_hw_intf  **devices[CRE_DEV_MAX];
	struct   cam_cre_query_cap_cmd cre_caps;

	struct cam_req_mgr_core_workq *cmd_work;
	struct cam_req_mgr_core_workq *msg_work;
	struct cam_req_mgr_core_workq *timer_work;
	struct cre_cmd_work_data *cmd_work_data;
	struct cre_msg_work_data *msg_work_data;
	struct cre_clk_work_data *timer_work_data;
	struct cam_hw_intf *cre_dev_intf[CRE_DEV_MAX];
	struct cam_soc_reg_map *reg_map[CRE_DEV_MAX][CRE_BASE_MAX];
	struct cam_cre_clk_info clk_info;
	struct dentry *dentry;
	bool   dump_req_data_enable;
};

/**
 * struct cam_cre_hw_ctx_data
 *
 * @context_priv: Context private data, cam_context from
 *     acquire.
 * @ctx_mutex: Mutex for context
 * @cre_dev_acquire_info: Acquire device info
 * @ctxt_event_cb: Context callback function
 * @in_use: Flag for context usage
 * @wait_complete: Completion info
 * @last_flush_req: req id which was flushed last.
 */
struct cam_cre_hw_ctx_data {
	void *context_priv;
	struct mutex ctx_mutex;
	struct cam_cre_acquire_dev_info cre_dev_acquire_info;
	cam_hw_event_cb_func ctxt_event_cb;
	bool in_use;
	struct completion wait_complete;
	uint64_t last_flush_req;
};
#endif /* CAM_CRE_HW_MGR_H */
