/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_OPE_HW_MGR_H
#define CAM_OPE_HW_MGR_H

#include <linux/types.h>
#include <linux/completion.h>
#include <media/cam_ope.h>
#include "ope_hw.h"
#include "cam_hw_mgr_intf.h"
#include "cam_hw_intf.h"
#include "cam_req_mgr_workq.h"
#include "cam_mem_mgr.h"
#include "cam_smmu_api.h"
#include "cam_soc_util.h"
#include "cam_req_mgr_timer.h"
#include "cam_context.h"
#include "ope_hw.h"
#include "cam_cdm_intf_api.h"
#include "cam_req_mgr_timer.h"

#define OPE_CTX_MAX               32
#define CAM_FRAME_CMD_MAX         20


#define OPE_WORKQ_NUM_TASK        100
#define OPE_WORKQ_TASK_CMD_TYPE   1
#define OPE_WORKQ_TASK_MSG_TYPE   2

#define OPE_PACKET_SIZE           0
#define OPE_PACKET_TYPE           1
#define OPE_PACKET_OPCODE         2

#define OPE_PACKET_MAX_CMD_BUFS   4

#define OPE_MAX_OUTPUT_SUPPORTED  8
#define OPE_MAX_INPUT_SUPPORTED   3

#define OPE_FRAME_PROCESS_SUCCESS 0
#define OPE_FRAME_PROCESS_FAILURE 1

#define OPE_CTX_STATE_FREE        0
#define OPE_CTX_STATE_IN_USE      1
#define OPE_CTX_STATE_ACQUIRED    2
#define OPE_CTX_STATE_RELEASE     3

#define OPE_CMDS                  OPE_MAX_CMD_BUFS
#define CAM_MAX_IN_RES            8

#define OPE_MAX_CDM_BLS           24

#define CAM_OPE_MAX_PER_PATH_VOTES 6
#define CAM_OPE_BW_CONFIG_UNKNOWN  0
#define CAM_OPE_BW_CONFIG_V2       2

#define CLK_HW_OPE                 0x0
#define CLK_HW_MAX                 0x1

#define OPE_DEVICE_IDLE_TIMEOUT    400
#define OPE_REQUEST_TIMEOUT        200

/**
 * struct cam_ope_clk_bw_request_v2
 * @budget_ns: Time required to process frame
 * @frame_cycles: Frame cycles needed to process the frame
 * @rt_flag: Flag to indicate real time stream
 * @reserved: Reserved for future use
 * @num_paths: Number of paths for per path bw vote
 * @axi_path: Per path vote info for OPE
 */
struct cam_ope_clk_bw_req_internal_v2 {
	uint64_t budget_ns;
	uint32_t frame_cycles;
	uint32_t rt_flag;
	uint32_t reserved;
	uint32_t num_paths;
	struct cam_axi_per_path_bw_vote axi_path[CAM_OPE_MAX_PER_PATH_VOTES];
};

/**
 * struct cam_ope_clk_bw_request
 * @budget_ns: Time required to process frame
 * @frame_cycles: Frame cycles needed to process the frame
 * @rt_flag: Flag to indicate real time stream
 * @uncompressed_bw: Bandwidth required to process frame
 * @compressed_bw: Compressed bandwidth to process frame
 */
struct cam_ope_clk_bw_request {
	uint64_t budget_ns;
	uint32_t frame_cycles;
	uint32_t rt_flag;
	uint64_t uncompressed_bw;
	uint64_t compressed_bw;
};

/**
 * struct cam_ctx_clk_info
 * @curr_fc: Context latest request frame cycles
 * @rt_flag: Flag to indicate real time request
 * @base_clk: Base clock to process the request
 * @reserved: Reserved field
 * @uncompressed_bw: Current bandwidth voting
 * @compressed_bw: Current compressed bandwidth voting
 * @clk_rate: Supported clock rates for the context
 * @num_paths: Number of valid AXI paths
 * @axi_path: ctx based per path bw vote
 */
struct cam_ctx_clk_info {
	uint32_t curr_fc;
	uint32_t rt_flag;
	uint32_t base_clk;
	uint32_t reserved;
	uint64_t uncompressed_bw;
	uint64_t compressed_bw;
	int32_t clk_rate[CAM_MAX_VOTE];
	uint32_t num_paths;
	struct cam_axi_per_path_bw_vote axi_path[CAM_OPE_MAX_PER_PATH_VOTES];
};

/**
 * struct ope_cmd_generic_blob
 * @ctx: Current context info
 * @req_info_idx: Index used for request
 * @io_buf_addr: pointer to io buffer address
 */
struct ope_cmd_generic_blob {
	struct cam_ope_ctx *ctx;
	uint32_t req_idx;
	uint64_t *io_buf_addr;
};

/**
 * struct cam_ope_clk_info
 * @base_clk: Base clock to process request
 * @curr_clk: Current clock of hadrware
 * @threshold: Threshold for overclk count
 * @over_clked: Over clock count
 * @uncompressed_bw: Current bandwidth voting
 * @compressed_bw: Current compressed bandwidth voting
 * @num_paths: Number of AXI vote paths
 * @axi_path: Current per path bw vote info
 * @hw_type: IPE/BPS device type
 * @watch_dog: watchdog timer handle
 * @watch_dog_reset_counter: Counter for watch dog reset
 */
struct cam_ope_clk_info {
	uint32_t base_clk;
	uint32_t curr_clk;
	uint32_t threshold;
	uint32_t over_clked;
	uint64_t uncompressed_bw;
	uint64_t compressed_bw;
	uint32_t num_paths;
	struct cam_axi_per_path_bw_vote axi_path[CAM_OPE_MAX_PER_PATH_VOTES];
	uint32_t hw_type;
	struct cam_req_mgr_timer *watch_dog;
	uint32_t watch_dog_reset_counter;
};

/**
 * struct ope_cmd_work_data
 *
 * @type:       Type of work data
 * @data:       Private data
 * @req_id:     Request Id
 */
struct ope_cmd_work_data {
	uint32_t type;
	void *data;
	int64_t req_id;
};

/**
 * struct ope_msg_work_data
 *
 * @type:       Type of work data
 * @data:       Private data
 * @irq_status: IRQ status
 */
struct ope_msg_work_data {
	uint32_t type;
	void *data;
	uint32_t irq_status;
};

/**
 * struct ope_clk_work_data
 *
 * @type: Type of work data
 * @data: Private data
 */
struct ope_clk_work_data {
	uint32_t type;
	void *data;
};

/**
 * struct cdm_dmi_cmd
 *
 * @length:   Number of bytes in LUT
 * @reserved: reserved bits
 * @cmd:      Command ID (CDMCmd)
 * @addr:     Address of the LUT in memory
 * @DMIAddr:  Address of the target DMI config register
 * @DMISel:   DMI identifier
 */
struct cdm_dmi_cmd {
	unsigned int length   : 16;
	unsigned int reserved : 8;
	unsigned int cmd      : 8;
	unsigned int addr;
	unsigned int DMIAddr  : 24;
	unsigned int DMISel   : 8;
} __attribute__((__packed__));

/**
 * struct ope_debug_buffer
 *
 * @cpu_addr:         CPU address
 * @iova_addr:        IOVA address
 * @len:              Buffer length
 * @size:             Buffer Size
 * @offset:	      buffer offset
 */
struct ope_debug_buffer {
	uintptr_t cpu_addr;
	dma_addr_t iova_addr;
	size_t len;
	uint32_t size;
	uint32_t offset;
};

/**
 * struct ope_kmd_buffer
 *
 * @mem_handle:       Memory handle
 * @cpu_addr:         CPU address
 * @iova_addr:        IOVA address
 * @iova_cdm_addr:    CDM IOVA address
 * @offset:           Offset of buffer
 * @len:              Buffer length
 * @size:             Buffer Size
 */
struct ope_kmd_buffer {
	uint32_t mem_handle;
	uintptr_t cpu_addr;
	dma_addr_t iova_addr;
	dma_addr_t iova_cdm_addr;
	uint32_t offset;
	size_t len;
	uint32_t size;
};

struct ope_stripe_settings {
	uintptr_t cpu_addr;
	dma_addr_t iova_addr;
	size_t len;
	uint32_t size;
	uint32_t buf_type;
	uint32_t type_buffered;
};

/**
 * struct ope_pass_settings
 *
 * @cpu_addr:         CPU address
 * @iova_addr:        IOVA address
 * @len:              Buffer length
 * @size:             Buffer Size
 * @idx:              Pass Index
 * @buf_type:         Direct/Indirect type
 * @type_buffered:    SB/DB types
 */
struct ope_pass_settings {
	uintptr_t cpu_addr;
	dma_addr_t iova_addr;
	size_t len;
	uint32_t size;
	uint32_t idx;
	uint32_t buf_type;
	uint32_t type_buffered;
};

/**
 * struct ope_frame_settings
 *
 * @cpu_addr:         CPU address
 * @iova_addr:        IOVA address
 * @offset:           offset
 * @len:              Buffer length
 * @size:             Buffer Size
 * @buf_type:         Direct/Indirect type
 * @type_buffered:    SB/DB types
 * @prefecth_disable: Disable prefetch
 */
struct ope_frame_settings {
	uintptr_t cpu_addr;
	dma_addr_t iova_addr;
	uint32_t offset;
	size_t len;
	uint32_t size;
	uint32_t buf_type;
	uint32_t type_buffered;
	uint32_t prefecth_disable;
};

/**
 * struct ope_stripe_io
 *
 * @format:            Stripe format
 * @s_location:        Stripe location
 * @cpu_addr:          Stripe CPU address
 * @iova_addr:         Stripe IOVA address
 * @width:             Stripe width
 * @height:            Stripe height
 * @stride:            Stripe stride
 * @unpack_format:     Unpack format
 * @pack_format:       Packing format
 * @alignment:         Stripe alignment
 * @offset:            Stripe offset
 * @x_init:            X_init
 * @subsample_period:  Subsample period
 * @subsample_pattern: Subsample pattern
 * @len:               Stripe buffer length
 * @disable_bus:       disable bus for the stripe
 */
struct ope_stripe_io {
	uint32_t format;
	uint32_t s_location;
	uintptr_t cpu_addr;
	dma_addr_t iova_addr;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t unpack_format;
	uint32_t pack_format;
	uint32_t alignment;
	uint32_t offset;
	uint32_t x_init;
	uint32_t subsample_period;
	uint32_t subsample_pattern;
	size_t len;
	uint32_t disable_bus;
};

/**
 * struct ope_io_buf
 *
 * @direction:     Direction of a buffer
 * @resource_type: Resource type of IO Buffer
 * @format:        Format
 * @fence:         Fence
 * @num_planes:    Number of planes
 * @num_stripes:   Number of stripes
 * @s_io:          Stripe info
 */
struct ope_io_buf {
	uint32_t direction;
	uint32_t resource_type;
	uint32_t format;
	uint32_t fence;
	uint32_t num_planes;
	uint32_t num_stripes[OPE_MAX_PLANES];
	struct ope_stripe_io s_io[OPE_MAX_PLANES][OPE_MAX_STRIPES];
};

/**
 * struct cam_ope_request
 *
 * @request_id:          Request Id
 * @req_idx:             Index in request list
 * @state:               Request state
 * @num_batch:           Number of batches
 * @num_cmd_bufs:        Number of command buffers
 * @num_frame_bufs:      Number of frame buffers
 * @num_pass_bufs:       Number of pass Buffers
 * @num_stripes:         Number of Stripes
 * @num_io_bufs:         Number of IO Buffers
 * @in_resource:         Input resource
 * @num_stripe_cmd_bufs: Command buffers per stripe
 * @ope_kmd_buf:         KMD buffer for OPE programming
 * @ope_debug_buf:       Debug buffer
 * @io_buf:              IO config info of a request
 * @cdm_cmd:             CDM command for OPE CDM
 * @clk_info:            Clock Info V1
 * @clk_info_v2:         Clock Info V2
 * @hang_data:           Debug data for HW error
 * @submit_timestamp:    Submit timestamp to hw
 */
struct cam_ope_request {
	uint64_t request_id;
	uint32_t req_idx;
	uint32_t state;
	uint32_t num_batch;
	uint32_t num_cmd_bufs;
	uint32_t num_frame_bufs;
	uint32_t num_pass_bufs;
	uint32_t num_stripes[OPE_MAX_BATCH_SIZE];
	uint32_t num_io_bufs[OPE_MAX_BATCH_SIZE];
	uint32_t in_resource;
	uint8_t num_stripe_cmd_bufs[OPE_MAX_BATCH_SIZE][OPE_MAX_STRIPES];
	struct ope_kmd_buffer ope_kmd_buf;
	struct ope_debug_buffer ope_debug_buf;
	struct ope_io_buf *io_buf[OPE_MAX_BATCH_SIZE][OPE_MAX_IO_BUFS];
	struct cam_cdm_bl_request *cdm_cmd;
	struct cam_ope_clk_bw_request clk_info;
	struct cam_ope_clk_bw_req_internal_v2 clk_info_v2;
	struct cam_hw_mgr_dump_pf_data hang_data;
	ktime_t submit_timestamp;
};

/**
 * struct cam_ope_cdm
 *
 * @cdm_handle: OPE CDM Handle
 * @cdm_ops:    OPE CDM Operations
 */
struct cam_ope_cdm {
	uint32_t cdm_handle;
	struct cam_cdm_utils_ops *cdm_ops;
};

/**
 * struct cam_ope_ctx
 *
 * @context_priv:    Private data of context
 * @bitmap:          Context bit map
 * @bitmap_size:     Context bit map size
 * @bits:            Context bit map bits
 * @ctx_id:          Context ID
 * @ctx_state:       State of a context
 * @req_cnt:         Requests count
 * @ctx_mutex:       Mutex for context
 * @acquire_dev_cmd: Cam acquire command
 * @ope_acquire:     OPE acquire command
 * @ctxt_event_cb:   Callback of a context
 * @req_list:        Request List
 * @ope_cdm:         OPE CDM info
 * @last_req_time:   Timestamp of last request
 * @req_watch_dog:   Watchdog for requests
 * @req_watch_dog_reset_counter: Request reset counter
 * @clk_info:        OPE Ctx clock info
 * @clk_watch_dog:   Clock watchdog
 * @clk_watch_dog_reset_counter: Reset counter
 * @last_flush_req: last flush req for this ctx
 */
struct cam_ope_ctx {
	void *context_priv;
	size_t bitmap_size;
	void *bitmap;
	size_t bits;
	uint32_t ctx_id;
	uint32_t ctx_state;
	uint32_t req_cnt;
	struct mutex ctx_mutex;
	struct cam_acquire_dev_cmd acquire_dev_cmd;
	struct ope_acquire_dev_info ope_acquire;
	cam_hw_event_cb_func ctxt_event_cb;
	struct cam_ope_request *req_list[CAM_CTX_REQ_MAX];
	struct cam_ope_cdm ope_cdm;
	uint64_t last_req_time;
	struct cam_req_mgr_timer *req_watch_dog;
	uint32_t req_watch_dog_reset_counter;
	struct cam_ctx_clk_info clk_info;
	struct cam_req_mgr_timer *clk_watch_dog;
	uint32_t clk_watch_dog_reset_counter;
	uint64_t last_flush_req;
};

/**
 * struct cam_ope_hw_mgr
 *
 * @open_cnt:          OPE device open count
 * @ope_ctx_cnt:       Open context count
 * @hw_mgr_mutex:      Mutex for HW manager
 * @hw_mgr_lock:       Spinlock for HW manager
 * @hfi_en:            Flag for HFI
 * @iommu_hdl:         OPE Handle
 * @iommu_sec_hdl:     OPE Handle for secure
 * @iommu_cdm_hdl:     CDM Handle
 * @iommu_sec_cdm_hdl: CDM Handle for secure
 * @num_ope:           Number of OPE
 * @secure_mode:       Mode of OPE operation
 * @ctx_bitmap:        Context bit map
 * @ctx_bitmap_size:   Context bit map size
 * @ctx_bits:          Context bit map bits
 * @ctx:               OPE context
 * @devices:           OPE devices
 * @ope_caps:          OPE capabilities
 * @cmd_work:          Command work
 * @msg_work:          Message work
 * @timer_work:        Timer work
 * @cmd_work_data:     Command work data
 * @msg_work_data:     Message work data
 * @timer_work_data:   Timer work data
 * @ope_dev_intf:      OPE device interface
 * @cdm_reg_map:       OPE CDM register map
 * @clk_info:          OPE clock Info for HW manager
 * @dentry:            Pointer to OPE debugfs directory
 * @frame_dump_enable: OPE frame setting dump enablement
 * @dump_req_data_enable: OPE hang dump enablement
 */
struct cam_ope_hw_mgr {
	int32_t             open_cnt;
	uint32_t            ope_ctx_cnt;
	struct mutex        hw_mgr_mutex;
	spinlock_t          hw_mgr_lock;
	bool                hfi_en;
	int32_t             iommu_hdl;
	int32_t             iommu_sec_hdl;
	int32_t             iommu_cdm_hdl;
	int32_t             iommu_sec_cdm_hdl;
	uint32_t            num_ope;
	bool                secure_mode;
	void *ctx_bitmap;
	size_t ctx_bitmap_size;
	size_t ctx_bits;
	struct cam_ope_ctx  ctx[OPE_CTX_MAX];
	struct cam_hw_intf  **devices[OPE_DEV_MAX];
	struct ope_query_cap_cmd ope_caps;
	uint64_t last_callback_time;

	struct cam_req_mgr_core_workq *cmd_work;
	struct cam_req_mgr_core_workq *msg_work;
	struct cam_req_mgr_core_workq *timer_work;
	struct ope_cmd_work_data *cmd_work_data;
	struct ope_msg_work_data *msg_work_data;
	struct ope_clk_work_data *timer_work_data;
	struct cam_hw_intf *ope_dev_intf[OPE_DEV_MAX];
	struct cam_soc_reg_map *cdm_reg_map[OPE_DEV_MAX][OPE_BASE_MAX];
	struct cam_ope_clk_info clk_info;
	struct dentry *dentry;
	bool   frame_dump_enable;
	bool   dump_req_data_enable;
};

/**
 * struct cam_ope_buf_entry
 *
 * @fd:                FD of cmd buffer
 * @memhdl:           Memhandle of cmd buffer
 * @iova:              IOVA address of cmd buffer
 * @offset:            Offset of cmd buffer
 * @len:               Length of cmd buffer
 * @size:              Size of cmd buffer
 */
struct cam_ope_buf_entry {
	uint32_t          fd;
	uint64_t          memhdl;
	uint64_t          iova;
	uint64_t          offset;
	uint64_t          len;
	uint64_t          size;
};

/**
 * struct cam_ope_bl_entry
 *
 * @base:              Base IOVA address of BL
 * @len:               Length of BL
 * @arbitration:       Arbitration bit
 */
struct cam_ope_bl_entry {
	uint32_t         base;
	uint32_t         len;
	uint32_t         arbitration;
};

/**
 * struct cam_ope_output_info
 *
 * @iova:              IOVA address of output buffer
 * @offset:            Offset of buffer
 * @len:               Length of buffer
 */
struct cam_ope_output_info {
	uint64_t    iova;
	uint64_t    offset;
	uint64_t    len;
};

/**
 * struct cam_ope_hang_dump
 *
 * @num_bls:           count of BLs for request
 * @num_bufs:          Count of buffer related to request
 * @num_outputs:       Count of output beffers
 * @entries:           Buffers info
 * @bl_entries:        BLs info
 * @outputs:           Output info
 */
struct cam_ope_hang_dump {
	uint32_t num_bls;
	uint32_t num_bufs;
	uint64_t num_outputs;
	struct cam_ope_buf_entry entries[OPE_MAX_BATCH_SIZE * OPE_MAX_CMD_BUFS];
	struct cam_ope_bl_entry bl_entries[OPE_MAX_CDM_BLS];
	struct cam_ope_output_info outputs
		[OPE_MAX_BATCH_SIZE * OPE_OUT_RES_MAX];
};
#endif /* CAM_OPE_HW_MGR_H */
