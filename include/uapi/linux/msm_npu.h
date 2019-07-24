/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _UAPI_MSM_NPU_H_
#define _UAPI_MSM_NPU_H_

/* -------------------------------------------------------------------------
 * Includes
 * -------------------------------------------------------------------------
 */
#include <linux/types.h>

/* -------------------------------------------------------------------------
 * Defines
 * -------------------------------------------------------------------------
 */
#define MSM_NPU_IOCTL_MAGIC 'n'

/* get npu info */
#define MSM_NPU_GET_INFO \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 1, struct msm_npu_get_info_ioctl)

/* map buf */
#define MSM_NPU_MAP_BUF \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 2, struct msm_npu_map_buf_ioctl)

/* map buf */
#define MSM_NPU_UNMAP_BUF \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 3, struct msm_npu_unmap_buf_ioctl)

/* load network */
#define MSM_NPU_LOAD_NETWORK \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 4, struct msm_npu_load_network_ioctl)

/* unload network */
#define MSM_NPU_UNLOAD_NETWORK \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 5, struct msm_npu_unload_network_ioctl)

/* exec network */
#define MSM_NPU_EXEC_NETWORK \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 6, struct msm_npu_exec_network_ioctl)

/* load network v2 */
#define MSM_NPU_LOAD_NETWORK_V2 \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 7, struct msm_npu_load_network_ioctl_v2)

/* exec network v2 */
#define MSM_NPU_EXEC_NETWORK_V2 \
	_IOWR(MSM_NPU_IOCTL_MAGIC, 8, struct msm_npu_exec_network_ioctl_v2)

/* receive event */
#define MSM_NPU_RECEIVE_EVENT \
	_IOR(MSM_NPU_IOCTL_MAGIC, 9, struct msm_npu_event)

/* set property */
#define MSM_NPU_SET_PROP \
	_IOW(MSM_NPU_IOCTL_MAGIC, 10, struct msm_npu_property)

/* get property */
#define MSM_NPU_GET_PROP \
	_IOW(MSM_NPU_IOCTL_MAGIC, 11, struct msm_npu_property)

#define MSM_NPU_EVENT_TYPE_START 0x10000000
#define MSM_NPU_EVENT_TYPE_EXEC_DONE (MSM_NPU_EVENT_TYPE_START + 1)
#define MSM_NPU_EVENT_TYPE_EXEC_V2_DONE (MSM_NPU_EVENT_TYPE_START + 2)
#define MSM_NPU_EVENT_TYPE_SSR (MSM_NPU_EVENT_TYPE_START + 3)

#define MSM_NPU_MAX_INPUT_LAYER_NUM 8
#define MSM_NPU_MAX_OUTPUT_LAYER_NUM 4
#define MSM_NPU_MAX_PATCH_LAYER_NUM (MSM_NPU_MAX_INPUT_LAYER_NUM +\
	MSM_NPU_MAX_OUTPUT_LAYER_NUM)

#define MSM_NPU_PROP_ID_START 0x100
#define MSM_NPU_PROP_ID_FW_STATE (MSM_NPU_PROP_ID_START + 0)
#define MSM_NPU_PROP_ID_PERF_MODE (MSM_NPU_PROP_ID_START + 1)
#define MSM_NPU_PROP_ID_PERF_MODE_MAX (MSM_NPU_PROP_ID_START + 2)
#define MSM_NPU_PROP_ID_DRV_VERSION (MSM_NPU_PROP_ID_START + 3)
#define MSM_NPU_PROP_ID_HARDWARE_VERSION (MSM_NPU_PROP_ID_START + 4)
#define MSM_NPU_PROP_ID_IPC_QUEUE_INFO (MSM_NPU_PROP_ID_START + 5)
#define MSM_NPU_PROP_ID_DRV_FEATURE (MSM_NPU_PROP_ID_START + 6)

#define MSM_NPU_FW_PROP_ID_START 0x1000
#define MSM_NPU_PROP_ID_DCVS_MODE (MSM_NPU_FW_PROP_ID_START + 0)
#define MSM_NPU_PROP_ID_DCVS_MODE_MAX (MSM_NPU_FW_PROP_ID_START + 1)
#define MSM_NPU_PROP_ID_CLK_GATING_MODE (MSM_NPU_FW_PROP_ID_START + 2)
#define MSM_NPU_PROP_ID_HW_VERSION (MSM_NPU_FW_PROP_ID_START + 3)
#define MSM_NPU_PROP_ID_FW_VERSION (MSM_NPU_FW_PROP_ID_START + 4)

/* features supported by driver */
#define MSM_NPU_FEATURE_MULTI_EXECUTE  0x1
#define MSM_NPU_FEATURE_ASYNC_EXECUTE  0x2

#define PROP_PARAM_MAX_SIZE 8

/* -------------------------------------------------------------------------
 * Data Structures
 * -------------------------------------------------------------------------
 */
struct msm_npu_patch_info {
	/* chunk id */
	uint32_t chunk_id;
	/* instruction size in bytes */
	uint16_t instruction_size_in_bytes;
	/* variable size in bits */
	uint16_t variable_size_in_bits;
	/* shift value in bits */
	uint16_t shift_value_in_bits;
	/* location offset */
	uint32_t loc_offset;
};

struct msm_npu_layer {
	/* layer id */
	uint32_t layer_id;
	/* patch information*/
	struct msm_npu_patch_info patch_info;
	/* buffer handle */
	int32_t buf_hdl;
	/* buffer size */
	uint32_t buf_size;
	/* physical address */
	uint64_t buf_phys_addr;
};

struct msm_npu_patch_info_v2 {
	/* patch value */
	uint32_t value;
	/* chunk id */
	uint32_t chunk_id;
	/* instruction size in bytes */
	uint32_t instruction_size_in_bytes;
	/* variable size in bits */
	uint32_t variable_size_in_bits;
	/* shift value in bits */
	uint32_t shift_value_in_bits;
	/* location offset */
	uint32_t loc_offset;
};

struct msm_npu_patch_buf_info {
	/* physical address to be patched */
	uint64_t buf_phys_addr;
	/* buffer id */
	uint32_t buf_id;
};

/* -------------------------------------------------------------------------
 * Data Structures - IOCTLs
 * -------------------------------------------------------------------------
 */
struct msm_npu_map_buf_ioctl {
	/* buffer ion handle */
	int32_t buf_ion_hdl;
	/* buffer size */
	uint32_t size;
	/* iommu mapped physical address */
	uint64_t npu_phys_addr;
};

struct msm_npu_unmap_buf_ioctl {
	/* buffer ion handle */
	int32_t buf_ion_hdl;
	/* iommu mapped physical address */
	uint64_t npu_phys_addr;
};

struct msm_npu_get_info_ioctl {
	/* firmware version */
	uint32_t firmware_version;
	/* reserved */
	uint32_t flags;
};

struct msm_npu_load_network_ioctl {
	/* buffer ion handle */
	int32_t buf_ion_hdl;
	/* physical address */
	uint64_t buf_phys_addr;
	/* buffer size */
	uint32_t buf_size;
	/* first block size */
	uint32_t first_block_size;
	/* reserved */
	uint32_t flags;
	/* network handle */
	uint32_t network_hdl;
	/* priority */
	uint32_t priority;
	/* perf mode */
	uint32_t perf_mode;
};

struct msm_npu_load_network_ioctl_v2 {
	/* physical address */
	uint64_t buf_phys_addr;
	/* patch info(v2) for all input/output layers */
	uint64_t patch_info;
	/* buffer ion handle */
	int32_t buf_ion_hdl;
	/* buffer size */
	uint32_t buf_size;
	/* first block size */
	uint32_t first_block_size;
	/* load flags */
	uint32_t flags;
	/* network handle */
	uint32_t network_hdl;
	/* priority */
	uint32_t priority;
	/* perf mode */
	uint32_t perf_mode;
	/* number of layers in the network */
	uint32_t num_layers;
	/* number of layers to be patched */
	uint32_t patch_info_num;
	/* reserved */
	uint32_t reserved;
};

struct msm_npu_unload_network_ioctl {
	/* network handle */
	uint32_t network_hdl;
};

struct msm_npu_exec_network_ioctl {
	/* network handle */
	uint32_t network_hdl;
	/* input layer number */
	uint32_t input_layer_num;
	/* input layer info */
	struct msm_npu_layer input_layers[MSM_NPU_MAX_INPUT_LAYER_NUM];
	/* output layer number */
	uint32_t output_layer_num;
	/* output layer info */
	struct msm_npu_layer output_layers[MSM_NPU_MAX_OUTPUT_LAYER_NUM];
	/* patching is required */
	uint32_t patching_required;
	/* asynchronous execution */
	uint32_t async;
	/* reserved */
	uint32_t flags;
};

struct msm_npu_exec_network_ioctl_v2 {
	/* stats buffer to be filled with execution stats */
	uint64_t stats_buf_addr;
	/* patch buf info for both input and output layers */
	uint64_t patch_buf_info;
	/* network handle */
	uint32_t network_hdl;
	/* asynchronous execution */
	uint32_t async;
	/* execution flags */
	uint32_t flags;
	/* stats buf size allocated */
	uint32_t stats_buf_size;
	/* number of layers to be patched */
	uint32_t patch_buf_info_num;
	/* reserved */
	uint32_t reserved;
};

struct msm_npu_event_execute_done {
	uint32_t network_hdl;
	int32_t exec_result;
};

struct msm_npu_event_execute_v2_done {
	uint32_t network_hdl;
	int32_t exec_result;
	/* stats buf size filled */
	uint32_t stats_buf_size;
};

struct msm_npu_event_ssr {
	uint32_t network_hdl;
};

struct msm_npu_event {
	uint32_t type;
	union {
		struct msm_npu_event_execute_done exec_done;
		struct msm_npu_event_execute_v2_done exec_v2_done;
		struct msm_npu_event_ssr ssr;
		uint8_t data[128];
	} u;
	uint32_t reserved[4];
};

struct msm_npu_property {
	uint32_t prop_id;
	uint32_t num_of_params;
	uint32_t network_hdl;
	uint32_t prop_param[PROP_PARAM_MAX_SIZE];
};

#endif /*_UAPI_MSM_NPU_H_*/
