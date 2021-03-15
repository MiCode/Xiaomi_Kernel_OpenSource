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

#define CAM_CDM_HW_ID_MASK      0xF
#define CAM_CDM_HW_ID_SHIFT     0x5
#define CAM_CDM_CLIENTS_ID_MASK 0x1F

#define CAM_CDM_GET_HW_IDX(x) (((x) >> CAM_CDM_HW_ID_SHIFT) & \
	CAM_CDM_HW_ID_MASK)
#define CAM_CDM_CREATE_CLIENT_HANDLE(hw_idx, client_idx) \
	((((hw_idx) & CAM_CDM_HW_ID_MASK) << CAM_CDM_HW_ID_SHIFT) | \
	 ((client_idx) & CAM_CDM_CLIENTS_ID_MASK))
#define CAM_CDM_GET_CLIENT_IDX(x) ((x) & CAM_CDM_CLIENTS_ID_MASK)
#define CAM_PER_CDM_MAX_REGISTERED_CLIENTS (CAM_CDM_CLIENTS_ID_MASK + 1)
#define CAM_CDM_INTF_MGR_MAX_SUPPORTED_CDM (CAM_CDM_HW_ID_MASK + 1)

/* enum cam_cdm_reg_attr - read, write, read and write permissions.*/
enum cam_cdm_reg_attr {
	CAM_REG_ATTR_READ,
	CAM_REG_ATTR_WRITE,
	CAM_REG_ATTR_READ_WRITE,
};

/* enum cam_cdm_hw_process_intf_cmd - interface commands.*/
enum cam_cdm_hw_process_intf_cmd {
	CAM_CDM_HW_INTF_CMD_ACQUIRE,
	CAM_CDM_HW_INTF_CMD_RELEASE,
	CAM_CDM_HW_INTF_CMD_SUBMIT_BL,
	CAM_CDM_HW_INTF_CMD_RESET_HW,
	CAM_CDM_HW_INTF_CMD_HANG_DETECT,
	CAM_CDM_HW_INTF_CMD_INVALID,
};

/* enum cam_cdm_regs - CDM driver offset enums.*/
enum cam_cdm_regs {
	/*cfg_offsets 0*/
	CDM_CFG_HW_VERSION,
	CDM_CFG_TITAN_VERSION,
	CDM_CFG_RST_CMD,
	CDM_CFG_CGC_CFG,
	CDM_CFG_CORE_CFG,
	CDM_CFG_CORE_EN,
	CDM_CFG_FE_CFG,
	/*irq_offsets 7*/
	CDM_IRQ_MASK,
	CDM_IRQ_CLEAR,
	CDM_IRQ_CLEAR_CMD,
	CDM_IRQ_SET,
	CDM_IRQ_SET_CMD,
	CDM_IRQ_STATUS,
	CDM_IRQ_USR_DATA,
	/*BL FIFO Registers 14*/
	CDM_BL_FIFO_BASE_REG,
	CDM_BL_FIFO_LEN_REG,
	CDM_BL_FIFO_STORE_REG,
	CDM_BL_FIFO_CFG,
	CDM_BL_FIFO_RB,
	CDM_BL_FIFO_BASE_RB,
	CDM_BL_FIFO_LEN_RB,
	CDM_BL_FIFO_PENDING_REQ_RB,
	/*CDM System Debug Registers 22*/
	CDM_DBG_WAIT_STATUS,
	CDM_DBG_SCRATCH_0_REG,
	CDM_DBG_SCRATCH_1_REG,
	CDM_DBG_SCRATCH_2_REG,
	CDM_DBG_SCRATCH_3_REG,
	CDM_DBG_SCRATCH_4_REG,
	CDM_DBG_SCRATCH_5_REG,
	CDM_DBG_SCRATCH_6_REG,
	CDM_DBG_SCRATCH_7_REG,
	CDM_DBG_LAST_AHB_ADDR,
	CDM_DBG_LAST_AHB_DATA,
	CDM_DBG_CORE_DBUG,
	CDM_DBG_LAST_AHB_ERR_ADDR,
	CDM_DBG_LAST_AHB_ERR_DATA,
	CDM_DBG_CURRENT_BL_BASE,
	CDM_DBG_CURRENT_BL_LEN,
	CDM_DBG_CURRENT_USED_AHB_BASE,
	CDM_DBG_DEBUG_STATUS,
	/*FE Bus Miser Registers 40*/
	CDM_BUS_MISR_CFG_0,
	CDM_BUS_MISR_CFG_1,
	CDM_BUS_MISR_RD_VAL,
	/*Performance Counter registers 43*/
	CDM_PERF_MON_CTRL,
	CDM_PERF_MON_0,
	CDM_PERF_MON_1,
	CDM_PERF_MON_2,
	/*Spare registers 47*/
	CDM_SPARE,
};

/* struct cam_cdm_reg_offset - struct for offset with attribute.*/
struct cam_cdm_reg_offset {
	uint32_t offset;
	enum cam_cdm_reg_attr attribute;
};

/* struct cam_cdm_reg_offset_table - struct for whole offset table.*/
struct cam_cdm_reg_offset_table {
	uint32_t first_offset;
	uint32_t last_offset;
	uint32_t reg_count;
	const struct cam_cdm_reg_offset *offsets;
	uint32_t offset_max_size;
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
	struct work_struct work;
};

/* enum cam_cdm_bl_cb_type - Enum for possible CAM CDM cb request types */
enum cam_cdm_bl_cb_type {
	CAM_HW_CDM_BL_CB_CLIENT = 1,
	CAM_HW_CDM_BL_CB_INTERNAL,
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

/* struct cam_cdm - CDM hw device struct */
struct cam_cdm {
	uint32_t index;
	char name[128];
	enum cam_cdm_id id;
	enum cam_cdm_flags flags;
	struct completion reset_complete;
	struct completion bl_complete;
	struct workqueue_struct *work_queue;
	struct list_head bl_request_list;
	struct cam_hw_version version;
	uint32_t hw_version;
	uint32_t hw_family_version;
	struct cam_iommu_handle iommu_hdl;
	struct cam_cdm_reg_offset_table *offset_tbl;
	struct cam_cdm_utils_ops *ops;
	struct cam_cdm_client *clients[CAM_PER_CDM_MAX_REGISTERED_CLIENTS];
	uint8_t bl_tag;
	atomic_t error;
	atomic_t bl_done;
	struct cam_cdm_hw_mem gen_irq;
	uint32_t cpas_handle;
	atomic_t work_record;
};

/* struct cam_cdm_private_dt_data - CDM hw custom dt data */
struct cam_cdm_private_dt_data {
	bool dt_cdm_shared;
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
	struct cam_cdm_intf_devices nodes[CAM_CDM_INTF_MGR_MAX_SUPPORTED_CDM];
	uint32_t cdm_count;
	uint32_t dt_supported_hw_cdm;
	int32_t refcount;
};

int cam_cdm_intf_register_hw_cdm(struct cam_hw_intf *hw,
	struct cam_cdm_private_dt_data *data, enum cam_cdm_type type,
	uint32_t *index);
int cam_cdm_intf_deregister_hw_cdm(struct cam_hw_intf *hw,
	struct cam_cdm_private_dt_data *data, enum cam_cdm_type type,
	uint32_t index);

#endif /* _CAM_CDM_H_ */
