/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CDM_API_H_
#define _CAM_CDM_API_H_

#include <media/cam_defs.h>
#include "cam_cdm_util.h"
#include "cam_soc_util.h"

#define CAM_CDM_BL_CMD_MAX  25

/* enum cam_cdm_id - Enum for possible CAM CDM hardwares */
enum cam_cdm_id {
	CAM_CDM_VIRTUAL,
	CAM_CDM_HW_ANY,
	CAM_CDM_CPAS,
	CAM_CDM_IFE,
	CAM_CDM_TFE,
	CAM_CDM_OPE,
	CAM_CDM_IPE0,
	CAM_CDM_IPE1,
	CAM_CDM_BPS,
	CAM_CDM_VFE,
	CAM_CDM_RT,
	CAM_CDM_MAX
};

/* enum cam_cdm_cb_status - Enum for possible CAM CDM callback */
enum cam_cdm_cb_status {
	CAM_CDM_CB_STATUS_BL_SUCCESS,
	CAM_CDM_CB_STATUS_INVALID_BL_CMD,
	CAM_CDM_CB_STATUS_PAGEFAULT,
	CAM_CDM_CB_STATUS_HW_RESET_ONGOING,
	CAM_CDM_CB_STATUS_HW_RESET_DONE,
	CAM_CDM_CB_STATUS_HW_FLUSH,
	CAM_CDM_CB_STATUS_HW_RESUBMIT,
	CAM_CDM_CB_STATUS_HW_ERROR,
	CAM_CDM_CB_STATUS_UNKNOWN_ERROR,
};

/* enum cam_cdm_bl_cmd_addr_type - Enum for possible CDM bl cmd addr types */
enum cam_cdm_bl_cmd_addr_type {
	CAM_CDM_BL_CMD_TYPE_MEM_HANDLE,
	CAM_CDM_BL_CMD_TYPE_HW_IOVA,
	CAM_CDM_BL_CMD_TYPE_KERNEL_IOVA,
};

/* enum cam_cdm_bl_fifo - interface commands.*/
enum cam_cdm_bl_fifo_queue {
	CAM_CDM_BL_FIFO_0,
	CAM_CDM_BL_FIFO_1,
	CAM_CDM_BL_FIFO_2,
	CAM_CDM_BL_FIFO_3,
	CAM_CDM_BL_FIFO_MAX,
};

/**
 * struct cam_cdm_acquire_data - Cam CDM acquire data structure
 *
 * @identifier : Input identifier string which is the device label from dt
 *                     like vfe, ife, jpeg etc
 * @cell_index : Input integer identifier pointing to the cell index from dt
 *                     of the device. This can be used to form a unique string
 *                     with @identifier like vfe0, ife1, jpeg0 etc
 * @id : ID of a specific or any CDM HW which needs to be acquired.
 * @userdata : Input private data which will be returned as part
 *                     of callback.
 * @cam_cdm_callback : Input callback pointer for triggering the
 *                     callbacks from CDM driver
 *                     @handle : CDM Client handle
 *                     @userdata : Private data given at the time of acquire
 *                     @status : Callback status
 *                     @cookie : Cookie if the callback is gen irq status
 * @base_array_cnt : Input number of ioremapped address pair pointing
 *                     in base_array, needed only if selected cdm is a virtual.
 * @base_array : Input pointer to ioremapped address pair arrary
 *                     needed only if selected cdm is a virtual.
 * @priority : Priority of the client.
 * @cdm_version : CDM version is output while acquiring HW cdm and
 *                     it is Input while acquiring virtual cdm.
 *                     Currently fixing it to one version below
 *                     acquire API.
 * @ops : Output pointer updated by cdm driver to the CDM
 *                     util ops for this HW version of CDM acquired.
 * @handle  : Output Unique handle generated for this acquire
 *
 */
struct cam_cdm_acquire_data {
	char identifier[128];
	uint32_t cell_index;
	enum cam_cdm_id id;
	void *userdata;
	void (*cam_cdm_callback)(uint32_t handle, void *userdata,
		enum cam_cdm_cb_status status, uint64_t cookie);
	uint32_t base_array_cnt;
	struct cam_soc_reg_map *base_array[CAM_SOC_MAX_BLOCK];
	enum cam_cdm_bl_fifo_queue priority;
	struct cam_hw_version cdm_version;
	struct cam_cdm_utils_ops *ops;
	uint32_t handle;
};

/**
 * struct cam_cdm_bl_cmd - Cam CDM HW bl command
 *
 * @bl_addr : Union of all three type for CDM BL commands
 * @mem_handle : Input mem handle of bl cmd
 * @offset : Input offset of the actual bl cmd in the memory pointed
 *           by mem_handle
 * @len : Input length of the BL command, Cannot be more than 1MB and
 *           this is will be validated with offset+size of the memory pointed
 *           by mem_handle
 * @enable_debug_gen_irq : bool flag to submit extra gen_irq afteR bl_command
 * @arbitrate : bool flag to arbitrate on submitted BL boundary
 */
struct cam_cdm_bl_cmd {
	union {
		int32_t mem_handle;
		uint32_t *hw_iova;
		uintptr_t kernel_iova;
	} bl_addr;
	uint32_t offset;
	uint32_t len;
	bool enable_debug_gen_irq;
	bool arbitrate;
};

/**
 * struct cam_cdm_bl_request - Cam CDM HW base & length (BL) request
 *
 * @flag : 1 for callback needed and 0 for no callback when this BL
 *            request is done
 * @gen_irq_arb : enum for setting arbitration in gen_irq
 * @userdata :Input private data which will be returned as part
 *             of callback if request for this bl request in flags.
 * @type : type of the submitted bl cmd address.
 * @cmd_arrary_count : Input number of BL commands to be submitted to CDM
 * @cookie : Cookie if the callback is gen irq status
 * @bl_cmd_array     : Input payload holding the BL cmd's arrary
 *                     to be sumbitted.
 *
 */
struct cam_cdm_bl_request {
	bool flag;
	bool gen_irq_arb;
	void *userdata;
	enum cam_cdm_bl_cmd_addr_type type;
	uint32_t cmd_arrary_count;
	uint64_t cookie;
	struct cam_cdm_bl_cmd cmd[1];
};

/**
 * struct cam_cdm_bl_data - last submiited CDM BL data
 *
 * @mem_handle : Input mem handle of bl cmd
 * @hw_addr    : Hw address of submitted Bl command
 * @offset     : Input offset of the actual bl cmd in the memory pointed
 *               by mem_handle
 * @len        : length of submitted Bl command to CDM.
 * @input_len  : Input length of the BL command, Cannot be more than 1MB and
 *           this is will be validated with offset+size of the memory pointed
 *           by mem_handle
 * @type       :  CDM bl cmd addr types.
 */
struct cam_cdm_bl_data {
	int32_t mem_handle;
	dma_addr_t hw_addr;
	uint32_t offset;
	size_t len;
	uint32_t  input_len;
	enum cam_cdm_bl_cmd_addr_type type;
};

/**
 * struct cam_cdm_bl_info
 *
 * @bl_count   : No. of Bl commands submiited to CDM.
 * @cmd        : payload holding the BL cmd's arrary
 *               that is sumbitted.
 *
 */
struct cam_cdm_bl_info {
	int32_t bl_count;
	struct cam_cdm_bl_data cmd[CAM_CDM_BL_CMD_MAX];
};

/**
 * struct cam_cdm_bl_info
 *
 * @handle    : handle for the bl fifo client
 * @module_id : module information of the hw.
 *
 */
struct cam_cdm_handle_info {
	uint32_t handle;
	uint32_t module_id;
};

/**
 * @brief : API to get the CDM capabilities for a camera device type
 *
 * @identifier : Input pointer to a string which is the device label from dt
 *                   like vfe, ife, jpeg etc, We do not need cell index
 *                   assuming all devices of a single type maps to one SMMU
 *                   client
 * @cdm_handles : Input iommu handle memory pointer to update handles
 *
 * @return 0 on success
 */
int cam_cdm_get_iommu_handle(char *identifier,
	struct cam_iommu_handle *cdm_handles);

/**
 * @brief : API to acquire a CDM
 *
 * @data : Input data for the CDM to be acquired
 *
 * @return 0 on success
 */
int cam_cdm_acquire(struct cam_cdm_acquire_data *data);

/**
 * @brief : API to release a previously acquired CDM
 *
 * @handle : Input handle for the CDM to be released
 *
 * @return 0 on success
 */
int cam_cdm_release(uint32_t handle);

/**
 * @brief : API to submit the base & length (BL's) for acquired CDM
 *
 * @handle : Input cdm handle to which the BL's needs to be sumbitted.
 * @data   : Input pointer to the BL's to be sumbitted
 *
 * @return 0 on success
 */
int cam_cdm_submit_bls(uint32_t handle, struct cam_cdm_bl_request *data);

/**
 * @brief : API to stream ON a previously acquired CDM,
 *          during this we turn on/off clocks/power based on active clients.
 *
 * @handle : Input handle for the CDM to be released
 *
 * @return 0 on success
 */
int cam_cdm_stream_on(uint32_t handle);

/**
 * @brief : API to stream OFF a previously acquired CDM,
 *          during this we turn on/off clocks/power based on active clients.
 *
 * @handle : Input handle for the CDM to be released
 *
 * @return 0 on success
 */
int cam_cdm_stream_off(uint32_t handle);

/**
 * @brief : API to reset previously acquired CDM,
 *          this should be only performed only if the CDM is private.
 *
 * @handle : Input handle of the CDM to reset
 *
 * @return 0 on success
 */
int cam_cdm_reset_hw(uint32_t handle);

/**
 * @brief : API to publish CDM ops to HW blocks like IFE
 *
 * @return : CDM operations
 *
 */
struct cam_cdm_utils_ops *cam_cdm_publish_ops(void);

/**
 * @brief : API to register CDM hw to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int cam_hw_cdm_init_module(void);

/**
 * @brief : API to register CDM interface to platform framework.
 * @return struct platform_device pointer on on success, or ERR_PTR() on error.
 */
int cam_cdm_intf_init_module(void);

/**
 * @brief : API to remove CDM interface from platform framework.
 */
void cam_cdm_intf_exit_module(void);

/**
 * @brief : API to remove CDM hw from platform framework.
 */
void cam_hw_cdm_exit_module(void);

/**
 * @brief : API to flush previously acquired CDM,
 *          this should be only performed only if the CDM is private.
 *
 * @handle : Input handle of the CDM to reset
 *
 * @return 0 on success
 */
int cam_cdm_flush_hw(uint32_t handle);

/**
 * @brief : API to detect culprit bl_tag in previously acquired CDM,
 *          this should be only performed only if the CDM is private.
 *
 * @handle : Input handle of the CDM to reset
 *
 * @return 0 on success
 */
int cam_cdm_handle_error(uint32_t handle);

/**
 * @brief : API get CDM ops
 *
 * @return : CDM operations
 *
 */
struct cam_cdm_utils_ops *cam_cdm_publish_ops(void);

/**
 * @brief : API to detect hang in previously acquired CDM,
 *          this should be only performed only if the CDM is private.
 *
 * @handle : Input handle of the CDM to detect hang
 * @module_id : Module id of the HW
 *
 * @return 0 on success
 */
int cam_cdm_detect_hang_error(uint32_t handle, uint32_t module_id);

/**
 * @brief : API to dump the CDM Debug registers
 *
 * @handle : Input handle of the CDM to dump the registers
 *
 * @return 0 on success
 */
int cam_cdm_dump_debug_registers(uint32_t handle);
#endif /* _CAM_CDM_API_H_ */
