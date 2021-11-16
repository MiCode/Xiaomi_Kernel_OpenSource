/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CDM_H_
#define _CAM_CDM_H_

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/random.h>
#include <linux/spinlock_types.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/bug.h>

#include "cam_cdm_intf_api.h"
#include "cam_soc_util.h"
#include "cam_cpas_api.h"
#include "cam_hw_intf.h"
#include "cam_hw.h"
#include "cam_debug_util.h"

#define CAM_MAX_SW_CDM_VERSION_SUPPORTED  1
#define CAM_SW_CDM_INDEX                  0
#define CAM_CDM_INFLIGHT_WORKS            5
#define CAM_CDM_HW_RESET_TIMEOUT          300

/*
 * Macros to get prepare and get information
 * from client CDM handles.
 */

#define CAM_CDM_HW_ID_MASK      0xF
#define CAM_CDM_HW_ID_SHIFT     0x10

#define CAM_CDM_CLIENTS_ID_MASK 0xFF

#define CAM_CDM_BL_FIFO_ID_MASK 0xF
#define CAM_CDM_BL_FIFO_ID_SHIFT 0x8

#define CAM_CDM_GET_HW_IDX(x) (((x) >> CAM_CDM_HW_ID_SHIFT) & \
	CAM_CDM_HW_ID_MASK)

#define CAM_CDM_GET_BLFIFO_IDX(x) (((x) >> CAM_CDM_BL_FIFO_ID_SHIFT) & \
	CAM_CDM_BL_FIFO_ID_MASK)

#define CAM_CDM_CREATE_CLIENT_HANDLE(hw_idx, priority, client_idx) \
	((((hw_idx) & CAM_CDM_HW_ID_MASK) << CAM_CDM_HW_ID_SHIFT) | \
	(((priority) & CAM_CDM_BL_FIFO_ID_MASK) << CAM_CDM_BL_FIFO_ID_SHIFT)| \
	 ((client_idx) & CAM_CDM_CLIENTS_ID_MASK))
#define CAM_CDM_GET_CLIENT_IDX(x) ((x) & CAM_CDM_CLIENTS_ID_MASK)
#define CAM_PER_CDM_MAX_REGISTERED_CLIENTS (CAM_CDM_CLIENTS_ID_MASK + 1)
#define CAM_CDM_INTF_MGR_MAX_SUPPORTED_CDM (CAM_CDM_HW_ID_MASK + 1)

/* Number of FIFO supported on CDM */
#define CAM_CDM_NUM_BL_FIFO 0x4

/* Max number of register set for different CDM */
#define CAM_CDM_BL_FIFO_REG_NUM 0x4
#define CAM_CDM_BL_FIFO_IRQ_REG_NUM 0x4
#define CAM_CDM_BL_FIFO_PENDING_REQ_REG_NUM 0x2
#define CAM_CDM_SCRATCH_REG_NUM 0xc
#define CAM_CDM_COMP_WAIT_STATUS_REG_NUM 0x2
#define CAM_CDM_PERF_MON_REG_NUM 0x2

/* BL_FIFO configurations*/
#define CAM_CDM_BL_FIFO_LENGTH_MAX_DEFAULT 0x40
#define CAM_CDM_BL_FIFO_LENGTH_CFG_SHIFT 0x10
#define CAM_CDM_BL_FIFO_FLUSH_SHIFT 0x3

#define CAM_CDM_BL_FIFO_REQ_SIZE_MAX 0x00
#define CAM_CDM_BL_FIFO_REQ_SIZE_MAX_DIV2 0x01
#define CAM_CDM_BL_FIFO_REQ_SIZE_MAX_DIV4 0x10
#define CAM_CDM_BL_FIFO_REQ_SIZE_MAX_DIV8 0x11

/* CDM core status bitmap */
#define CAM_CDM_HW_INIT_STATUS 0x0
#define CAM_CDM_FIFO_0_BLDONE_STATUS 0x0
#define CAM_CDM_FIFO_1_BLDONE_STATUS 0x1
#define CAM_CDM_FIFO_2_BLDONE_STATUS 0x2
#define CAM_CDM_FIFO_3_BLDONE_STATUS 0x3
#define CAM_CDM_RESET_HW_STATUS 0x4
#define CAM_CDM_ERROR_HW_STATUS 0x5
#define CAM_CDM_FLUSH_HW_STATUS 0x6

/* Curent BL command masks and shifts */
#define CAM_CDM_CURRENT_BL_LEN   0xFFFFF
#define CAM_CDM_CURRENT_BL_ARB   0x100000
#define CAM_CDM_CURRENT_BL_FIFO  0xC00000
#define CAM_CDM_CURRENT_BL_TAG   0xFF000000

#define CAM_CDM_CURRENT_BL_ARB_SHIFT   0x14
#define CAM_CDM_CURRENT_BL_FIFO_SHIFT  0x16
#define CAM_CDM_CURRENT_BL_TAG_SHIFT   0x18

/* IRQ bit-masks */
#define CAM_CDM_IRQ_STATUS_RST_DONE_MASK 0x1
#define CAM_CDM_IRQ_STATUS_INLINE_IRQ_MASK 0x2
#define CAM_CDM_IRQ_STATUS_BL_DONE_MASK 0x4
#define CAM_CDM_IRQ_STATUS_ERROR_INV_CMD_MASK 0x10000
#define CAM_CDM_IRQ_STATUS_ERROR_OVER_FLOW_MASK 0x20000
#define CAM_CDM_IRQ_STATUS_ERROR_AHB_BUS_MASK 0x40000
#define CAM_CDM_IRQ_STATUS_USR_DATA_MASK 0xFF

#define CAM_CDM_IRQ_STATUS_ERRORS \
	(CAM_CDM_IRQ_STATUS_ERROR_INV_CMD_MASK | \
	 CAM_CDM_IRQ_STATUS_ERROR_OVER_FLOW_MASK | \
	 CAM_CDM_IRQ_STATUS_ERROR_AHB_BUS_MASK)

/* Structure to store hw version info */
struct cam_version_reg {
	uint32_t hw_version;
};

/**
 * struct cam_cdm_irq_regs - CDM IRQ registers
 *
 * @irq_mask:        register offset for irq_mask
 * @irq_clear:       register offset for irq_clear
 * @irq_clear_cmd:   register offset to initiate irq clear
 * @irq_set:         register offset to set irq
 * @irq_set_cmd:     register offset to issue set_irq from irq_set
 * @irq_status:      register offset to look which irq is received
 */
struct cam_cdm_irq_regs {
	uint32_t irq_mask;
	uint32_t irq_clear;
	uint32_t irq_clear_cmd;
	uint32_t irq_set;
	uint32_t irq_set_cmd;
	uint32_t irq_status;
};

/**
 * struct cam_cdm_bl_fifo_regs - BL_FIFO registers
 *
 * @bl_fifo_base:    register offset to write bl_cmd base address
 * @bl_fifo_len:     register offset to write bl_cmd length
 * @bl_fifo_store:   register offset to commit the BL cmd
 * @bl_fifo_cfg:     register offset to config BL_FIFO depth, etc.
 */
struct cam_cdm_bl_fifo_regs {
	uint32_t bl_fifo_base;
	uint32_t bl_fifo_len;
	uint32_t bl_fifo_store;
	uint32_t bl_fifo_cfg;
};

/**
 * struct cam_cdm_bl_pending_req_reg_params - BL_FIFO pending registers
 *
 * @rb_offset:          register offset pending bl request in BL_FIFO
 * @rb_mask:            mask to get number of pending BLs in BL_FIFO
 * @rb_num_fifo:        number of BL_FIFO's information in the register
 * @rb_next_fifo_shift: shift to get next fifo's pending BLs.
 */
struct cam_cdm_bl_pending_req_reg_params {
	uint32_t rb_offset;
	uint32_t rb_mask;
	uint32_t rb_num_fifo;
	uint32_t rb_next_fifo_shift;
};

/**
 * struct cam_cdm_scratch_reg - scratch register
 *
 * @scratch_reg:        offset of scratch register
 */
struct cam_cdm_scratch_reg {
	uint32_t scratch_reg;
};

/* struct cam_cdm_perf_mon_regs - perf_mon registers */
struct cam_cdm_perf_mon_regs {
	uint32_t perf_mon_ctrl;
	uint32_t perf_mon_0;
	uint32_t perf_mon_1;
	uint32_t perf_mon_2;
};

/**
 * struct cam_cdm_perf_mon_regs - perf mon counter's registers
 *
 * @count_cfg_0:         register offset to configure perf measures
 * @always_count_val:    register offset for always count value
 * @busy_count_val:      register offset to get busy count
 * @stall_axi_count_val: register offset to get axi stall counts
 * @count_status:        register offset to know if count status finished
 *                       for stall, busy and always.
 */
struct cam_cdm_perf_regs {
	uint32_t count_cfg_0;
	uint32_t always_count_val;
	uint32_t busy_count_val;
	uint32_t stall_axi_count_val;
	uint32_t count_status;
};

/**
 * struct cam_cdm_icl_data_regs - CDM icl data registers
 *
 * @icl_last_data_0:     register offset to log last known good command
 * @icl_last_data_1:     register offset to log last known good command 1
 * @icl_last_data_2:     register offset to log last known good command 2
 * @icl_inv_data:        register offset to log CDM cmd that triggered
 *                       invalid command.
 */
struct cam_cdm_icl_data_regs {
	uint32_t icl_last_data_0;
	uint32_t icl_last_data_1;
	uint32_t icl_last_data_2;
	uint32_t icl_inv_data;
};

/**
 * struct cam_cdm_icl_misc_regs - CDM icl misc registers
 *
 * @icl_inv_bl_addr:     register offset to give address of bl_cmd that
 *                       gave invalid command
 * @icl_status:          register offset for context that gave good BL
 *                       command and invalid command.
 */
struct cam_cdm_icl_misc_regs {
	uint32_t icl_inv_bl_addr;
	uint32_t icl_status;
};

/**
 * struct cam_cdm_icl_regs - CDM icl registers
 *
 * @data_regs:           structure with registers of all cdm good and invalid
 *                       BL command information.
 * @misc_regs:           structure with registers for invalid command address
 *                       and context
 */
struct cam_cdm_icl_regs {
	struct cam_cdm_icl_data_regs *data_regs;
	struct cam_cdm_icl_misc_regs *misc_regs;
};

/**
 * struct cam_cdm_comp_wait_status - BL_FIFO comp_event status register
 *
 * @comp_wait_status:    register offset to give information on whether the
 *                       CDM is waiting for an event from another module
 */
struct cam_cdm_comp_wait_status {
	uint32_t comp_wait_status;
};

/**
 * struct cam_cdm_common_reg_data - structure for register data
 *
 * @num_bl_fifo:            number of FIFO are there in CDM
 * @num_bl_fifo_irq:        number of FIFO irqs in CDM
 * @num_bl_pending_req_reg: number of pending_requests register in CDM
 * @num_scratch_reg:        number of scratch registers in CDM
 */
struct cam_cdm_common_reg_data {
	uint32_t num_bl_fifo;
	uint32_t num_bl_fifo_irq;
	uint32_t num_bl_pending_req_reg;
	uint32_t num_scratch_reg;
};

/**
 * struct cam_cdm_common_regs - common structure to get common registers
 *                       of CDM
 *
 * @cdm_hw_version:      offset to read cdm_hw_version
 * @cam_version:         offset to read the camera Titan architecture version
 * @rst_cmd:             offset to reset the CDM
 * @cgc_cfg:             offset to configure CDM CGC logic
 * @core_cfg:            offset to configure CDM core with ARB_SEL, implicit
 *                       wait, etc.
 * @core_en:             offset to pause/enable CDM
 * @fe_cfg:              offset to configure CDM fetch engine
 * @irq_context_status   offset to read back irq context status
 * @bl_fifo_rb:          offset to set BL_FIFO read back
 * @bl_fifo_base_rb:     offset to read back base address on offset set by
 *                       bl_fifo_rb
 * @bl_fifo_len_rb:      offset to read back base len and tag on offset set by
 *                       bl_fifo_rb
 * @usr_data:            offset to read user data from GEN_IRQ commands
 * @wait_status:         offset to read status for last WAIT command
 * @last_ahb_addr:       offset to read back last AHB address generated by CDM
 * @last_ahb_data:       offset to read back last AHB data generated by CDM
 * @core_debug:          offset to configure CDM debug bus and debug features
 * @last_ahb_err_addr:   offset to read back last AHB Error address generated
 *                       by CDM
 * @last_ahb_err_data:   offset to read back last AHB Error data generated
 *                       by CDM
 * @current_bl_base:     offset to read back current command buffer BASE address
 *                       value out of BL_FIFO
 * @current_bl_len:      offset to read back current command buffer len, TAG,
 *                       context ID ARB value out of BL_FIFO
 * @current_used_ahb_base: offset to read back current base address used by
 *                       CDM to access camera register
 * @debug_status:        offset to read back current CDM status
 * @bus_misr_cfg0:       offset to enable bus MISR and configure sampling mode
 * @bus_misr_cfg1:       offset to select from one of the six MISR's for reading
 *                       signature value
 * @bus_misr_rd_val:     offset to read MISR signature
 * @pending_req:         registers to read pending request in FIFO
 * @comp_wait:           registers to read comp_event CDM is waiting for
 * @perf_mon:            registers to read perf_mon information
 * @scratch:             registers to read scratch register value
 * @perf_reg:            registers to read performance counters value
 * @icl_reg:             registers to read information related to good
 *                       and invalid commands in FIFO
 * @spare:               spare register
 * @priority_group_bit_offset offset of priority group bits
 *
 */
struct cam_cdm_common_regs {
	uint32_t cdm_hw_version;
	const struct cam_version_reg *cam_version;
	uint32_t rst_cmd;
	uint32_t cgc_cfg;
	uint32_t core_cfg;
	uint32_t core_en;
	uint32_t fe_cfg;
	uint32_t irq_context_status;
	uint32_t bl_fifo_rb;
	uint32_t bl_fifo_base_rb;
	uint32_t bl_fifo_len_rb;
	uint32_t usr_data;
	uint32_t wait_status;
	uint32_t last_ahb_addr;
	uint32_t last_ahb_data;
	uint32_t core_debug;
	uint32_t last_ahb_err_addr;
	uint32_t last_ahb_err_data;
	uint32_t current_bl_base;
	uint32_t current_bl_len;
	uint32_t current_used_ahb_base;
	uint32_t debug_status;
	uint32_t bus_misr_cfg0;
	uint32_t bus_misr_cfg1;
	uint32_t bus_misr_rd_val;
	const struct cam_cdm_bl_pending_req_reg_params
		*pending_req[CAM_CDM_BL_FIFO_PENDING_REQ_REG_NUM];
	const struct cam_cdm_comp_wait_status
		*comp_wait[CAM_CDM_COMP_WAIT_STATUS_REG_NUM];
	const struct cam_cdm_perf_mon_regs
		*perf_mon[CAM_CDM_PERF_MON_REG_NUM];
	const struct cam_cdm_scratch_reg
		*scratch[CAM_CDM_SCRATCH_REG_NUM];
	const struct cam_cdm_perf_regs *perf_reg;
	const struct cam_cdm_icl_regs *icl_reg;
	uint32_t spare;
	uint32_t priority_group_bit_offset;
};

/**
 * struct cam_cdm_hw_reg_offset - BL_FIFO comp_event status register
 *
 * @cmn_reg:             pointer to structure to get common registers of a CDM
 * @bl_fifo_reg:         pointer to structure to get BL_FIFO registers of a CDM
 * @irq_reg:             pointer to structure to get IRQ registers of a CDM
 * @reg_data:            pointer to structure to reg_data related to CDM
 *                       registers
 */
struct cam_cdm_hw_reg_offset {
	const struct cam_cdm_common_regs *cmn_reg;
	const struct cam_cdm_bl_fifo_regs *bl_fifo_reg[CAM_CDM_BL_FIFO_REG_NUM];
	const struct cam_cdm_irq_regs *irq_reg[CAM_CDM_BL_FIFO_IRQ_REG_NUM];
	const struct cam_cdm_common_reg_data *reg_data;
};

/* enum cam_cdm_hw_process_intf_cmd - interface commands.*/
enum cam_cdm_hw_process_intf_cmd {
	CAM_CDM_HW_INTF_CMD_ACQUIRE,
	CAM_CDM_HW_INTF_CMD_RELEASE,
	CAM_CDM_HW_INTF_CMD_SUBMIT_BL,
	CAM_CDM_HW_INTF_CMD_RESET_HW,
	CAM_CDM_HW_INTF_CMD_FLUSH_HW,
	CAM_CDM_HW_INTF_CMD_HANDLE_ERROR,
	CAM_CDM_HW_INTF_CMD_HANG_DETECT,
	CAM_CDM_HW_INTF_DUMP_DBG_REGS,
	CAM_CDM_HW_INTF_CMD_INVALID,
};

/* enum cam_cdm_flags - Bit fields for CDM flags used */
enum cam_cdm_flags {
	CAM_CDM_FLAG_SHARED_CDM,
	CAM_CDM_FLAG_PRIVATE_CDM,
};

/* enum cam_cdm_type - Enum for possible CAM CDM types */
enum cam_cdm_type {
	CAM_VIRTUAL_CDM,
	CAM_HW_CDM,
};

/* enum cam_cdm_mem_base_index - Enum for possible CAM CDM types */
enum cam_cdm_mem_base_index {
	CAM_HW_CDM_BASE_INDEX,
	CAM_HW_CDM_MAX_INDEX = CAM_SOC_MAX_BLOCK,
};

/* enum cam_cdm_bl_cb_type - Enum for possible CAM CDM cb request types */
enum cam_cdm_bl_cb_type {
	CAM_HW_CDM_BL_CB_CLIENT = 1,
	CAM_HW_CDM_BL_CB_INTERNAL,
};

/* enum cam_cdm_arbitration - Enum type of arbitration */
enum cam_cdm_arbitration {
	CAM_CDM_ARBITRATION_NONE,
	CAM_CDM_ARBITRATION_ROUND_ROBIN,
	CAM_CDM_ARBITRATION_PRIORITY_BASED,
	CAM_CDM_ARBITRATION_MAX,
};

enum cam_cdm_hw_version {
	CAM_CDM_VERSION = 0,
	CAM_CDM_VERSION_1_0 = 0x10000000,
	CAM_CDM_VERSION_1_1 = 0x10010000,
	CAM_CDM_VERSION_1_2 = 0x10020000,
	CAM_CDM_VERSION_2_0 = 0x20000000,
	CAM_CDM_VERSION_2_1 = 0x20010000,
	CAM_CDM_VERSION_MAX,
};

/* struct cam_cdm_client - struct for cdm clients data.*/
struct cam_cdm_client {
	struct cam_cdm_acquire_data data;
	void __iomem  *changebase_addr;
	uint32_t stream_on;
	uint32_t refcount;
	struct mutex lock;
	uint32_t handle;
};

/* struct cam_cdm_work_payload - struct for cdm work payload data.*/
struct cam_cdm_work_payload {
	struct cam_hw_info *hw;
	uint32_t irq_status;
	uint32_t irq_data;
	int fifo_idx;
	ktime_t workq_scheduled_ts;
	struct work_struct work;
};

/* struct cam_cdm_bl_cb_request_entry - callback entry for work to process.*/
struct cam_cdm_bl_cb_request_entry {
	uint8_t bl_tag;
	enum cam_cdm_bl_cb_type request_type;
	uint32_t client_hdl;
	void *userdata;
	uint32_t cookie;
	struct list_head entry;
};

/* struct cam_cdm_hw_intf_cmd_submit_bl - cdm interface submit command.*/
struct cam_cdm_hw_intf_cmd_submit_bl {
	uint32_t handle;
	struct cam_cdm_bl_request *data;
};

/* struct cam_cdm_hw_mem - CDM hw memory struct */
struct cam_cdm_hw_mem {
	int32_t handle;
	uint32_t vaddr;
	uintptr_t kmdvaddr;
	size_t size;
};

/* struct cam_cdm_bl_fifo - CDM hw memory struct */
struct cam_cdm_bl_fifo {
	struct completion bl_complete;
	struct workqueue_struct *work_queue;
	struct list_head bl_request_list;
	struct mutex fifo_lock;
	uint8_t bl_tag;
	uint32_t bl_depth;
	uint8_t last_bl_tag_done;
	atomic_t work_record;
};

/**
 * struct cam_cdm - CDM hw device struct
 *
 * @index:               index of CDM hardware
 * @name:                cdm_name
 * @id:                  enum for possible CDM hardwares
 * @flags:               enum to tell if CDM is private of shared
 * @reset_complete:      completion event to make CDM wait for reset
 * @work_queue:          workqueue to schedule work for virtual CDM
 * @bl_request_list:     bl_request list for submitted commands in
 *                       virtual CDM
 * @version:             CDM version with major, minor, incr and reserved
 * @hw_version:          CDM version as read from the cdm_version register
 * @hw_family_version:   version of hw family the CDM belongs to
 * @iommu_hdl:           CDM iommu handle
 * @offsets:             pointer to structure of CDM registers
 * @ops:                 CDM ops for generating cdm commands
 * @clients:             CDM clients array currently active on CDM
 * @bl_fifo:             structure with per fifo related attributes
 * @cdm_status:          bitfield with bits assigned for different cdm status
 * @bl_tag:              slot value at which the next bl cmd will be written
 *                       in case of virtual CDM
 * @gen_irq:             memory region in which gen_irq command will be written
 * @cpas_handle:         handle for cpas driver
 * @arbitration:         type of arbitration to be used for the CDM
 */
struct cam_cdm {
	uint32_t index;
	char name[128];
	enum cam_cdm_id id;
	enum cam_cdm_flags flags;
	struct completion reset_complete;
	struct workqueue_struct *work_queue;
	struct list_head bl_request_list;
	struct cam_hw_version version;
	uint32_t hw_version;
	uint32_t hw_family_version;
	struct cam_iommu_handle iommu_hdl;
	struct cam_cdm_hw_reg_offset *offsets;
	struct cam_cdm_utils_ops *ops;
	struct cam_cdm_client *clients[CAM_PER_CDM_MAX_REGISTERED_CLIENTS];
	struct cam_cdm_bl_fifo bl_fifo[CAM_CDM_BL_FIFO_MAX];
	unsigned long cdm_status;
	uint8_t bl_tag;
	struct cam_cdm_hw_mem gen_irq[CAM_CDM_BL_FIFO_MAX];
	uint32_t cpas_handle;
	enum cam_cdm_arbitration arbitration;
};

/* struct cam_cdm_private_dt_data - CDM hw custom dt data */
struct cam_cdm_private_dt_data {
	bool dt_cdm_shared;
	bool config_fifo;
	bool is_single_ctx_cdm;
	uint8_t priority_group;
	uint32_t fifo_depth[CAM_CDM_BL_FIFO_MAX];
	uint32_t dt_num_supported_clients;
	const char *dt_cdm_client_name[CAM_PER_CDM_MAX_REGISTERED_CLIENTS];
};

/* struct cam_cdm_intf_devices - CDM mgr interface devices */
struct cam_cdm_intf_devices {
	struct mutex lock;
	uint32_t refcount;
	struct cam_hw_intf *device;
	struct cam_cdm_private_dt_data *data;
};

/* struct cam_cdm_intf_mgr - CDM mgr interface device struct */
struct cam_cdm_intf_mgr {
	bool probe_done;
	uint32_t cdm_count;
	uint32_t dt_supported_hw_cdm;
	int32_t refcount;
	struct cam_cdm_intf_devices nodes[CAM_CDM_INTF_MGR_MAX_SUPPORTED_CDM];
};

int cam_cdm_intf_register_hw_cdm(struct cam_hw_intf *hw,
	struct cam_cdm_private_dt_data *data, enum cam_cdm_type type,
	uint32_t *index);
int cam_cdm_intf_deregister_hw_cdm(struct cam_hw_intf *hw,
	struct cam_cdm_private_dt_data *data, enum cam_cdm_type type,
	uint32_t index);

#endif /* _CAM_CDM_H_ */
