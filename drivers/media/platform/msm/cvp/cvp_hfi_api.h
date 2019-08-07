/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __CVP_HFI_API_H__
#define __CVP_HFI_API_H__

#include <linux/log2.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/hash.h>
#include "msm_cvp_core.h"
#include "msm_cvp_resources.h"
#include "cvp_hfi_helper.h"

#define CONTAINS(__a, __sz, __t) (\
	(__t >= __a) && \
	(__t < __a + __sz) \
)

#define OVERLAPS(__t, __tsz, __a, __asz) (\
	(__t <= __a) && \
	(__t + __tsz >= __a + __asz) \
)

#define CVP_VERSION_LENGTH 128

/* 16 encoder and 16 decoder sessions */
#define CVP_MAX_SESSIONS	32

#define HFI_DFS_CONFIG_CMD_SIZE	38
#define HFI_DFS_FRAME_CMD_SIZE	16
#define HFI_DFS_FRAME_BUFFERS_OFFSET 8
#define HFI_DFS_BUF_NUM 4

#define HFI_DME_CONFIG_CMD_SIZE	194
#define HFI_DME_BASIC_CONFIG_CMD_SIZE	51
#define HFI_DME_FRAME_CMD_SIZE	28
#define HFI_DME_FRAME_BUFFERS_OFFSET 12
#define HFI_DME_BUF_NUM 8

#define HFI_PERSIST_CMD_SIZE	11
#define HFI_PERSIST_BUFFERS_OFFSET 7
#define HFI_PERSIST_BUF_NUM     2

#define HFI_DS_CMD_SIZE	50
#define HFI_DS_BUFFERS_OFFSET	44
#define HFI_DS_BUF_NUM	3

#define HFI_OF_CONFIG_CMD_SIZE 34
#define HFI_OF_FRAME_CMD_SIZE 24
#define HFI_OF_BUFFERS_OFFSET 8
#define HFI_OF_BUF_NUM 8

#define HFI_ODT_CONFIG_CMD_SIZE 23
#define HFI_ODT_FRAME_CMD_SIZE 33
#define HFI_ODT_BUFFERS_OFFSET 11
#define HFI_ODT_BUF_NUM 11

#define HFI_OD_CONFIG_CMD_SIZE 24
#define HFI_OD_FRAME_CMD_SIZE 12
#define HFI_OD_BUFFERS_OFFSET 6
#define HFI_OD_BUF_NUM 3

#define HFI_NCC_CONFIG_CMD_SIZE 47
#define HFI_NCC_FRAME_CMD_SIZE 22
#define HFI_NCC_BUFFERS_OFFSET 8
#define HFI_NCC_BUF_NUM 7

#define HFI_ICA_CONFIG_CMD_SIZE 127
#define HFI_ICA_FRAME_CMD_SIZE 14
#define HFI_ICA_BUFFERS_OFFSET 6
#define HFI_ICA_BUF_NUM 4

#define HFI_HCD_CONFIG_CMD_SIZE 46
#define HFI_HCD_FRAME_CMD_SIZE 18
#define HFI_HCD_BUFFERS_OFFSET 12
#define HFI_HCD_BUF_NUM 3

#define HFI_DCM_CONFIG_CMD_SIZE 20
#define HFI_DCM_FRAME_CMD_SIZE 19
#define HFI_DCM_BUFFERS_OFFSET 9
#define HFI_DCM_BUF_NUM 5

#define HFI_PYS_HCD_CONFIG_CMD_SIZE 461
#define HFI_PYS_HCD_FRAME_CMD_SIZE 66
#define HFI_PYS_HCD_BUFFERS_OFFSET 14
#define HFI_PYS_HCD_BUF_NUM 26

#define HFI_FD_CONFIG_CMD_SIZE 28
#define HFI_FD_FRAME_CMD_SIZE  10
#define HFI_FD_BUFFERS_OFFSET  6
#define HFI_FD_BUF_NUM 2

#define HFI_MODEL_CMD_SIZE 9
#define HFI_MODEL_BUFFERS_OFFSET 7
#define HFI_MODEL_BUF_NUM 1

#define DFS_BIT_OFFSET (CVP_KMD_HFI_DFS_FRAME_CMD - CVP_KMD_CMD_START)
#define DME_BIT_OFFSET (CVP_KMD_HFI_DME_FRAME_CMD - CVP_KMD_CMD_START)
#define PERSIST_BIT_OFFSET (CVP_KMD_HFI_PERSIST_CMD - CVP_KMD_CMD_START)
#define ICA_BIT_OFFSET (CVP_KMD_HFI_ICA_FRAME_CMD - CVP_KMD_CMD_START)
#define FD_BIT_OFFSET (CVP_KMD_HFI_FD_FRAME_CMD - CVP_KMD_CMD_START)

#define HFI_VERSION_MAJOR_MASK 0xFF000000
#define HFI_VERSION_MAJOR_SHFIT 24
#define HFI_VERSION_MINOR_MASK 0x00FFFFE0
#define HFI_VERSION_MINOR_SHIFT 5
#define HFI_VERSION_BRANCH_MASK 0x0000001F
#define HFI_VERSION_BRANCH_SHIFT 0

enum cvp_status {
	CVP_ERR_NONE = 0x0,
	CVP_ERR_FAIL = 0x80000000,
	CVP_ERR_ALLOC_FAIL,
	CVP_ERR_ILLEGAL_OP,
	CVP_ERR_BAD_PARAM,
	CVP_ERR_BAD_HANDLE,
	CVP_ERR_NOT_SUPPORTED,
	CVP_ERR_BAD_STATE,
	CVP_ERR_MAX_CLIENTS,
	CVP_ERR_IFRAME_EXPECTED,
	CVP_ERR_HW_FATAL,
	CVP_ERR_BITSTREAM_ERR,
	CVP_ERR_INDEX_NOMORE,
	CVP_ERR_SEQHDR_PARSE_FAIL,
	CVP_ERR_INSUFFICIENT_BUFFER,
	CVP_ERR_BAD_POWER_STATE,
	CVP_ERR_NO_VALID_SESSION,
	CVP_ERR_TIMEOUT,
	CVP_ERR_CMDQFULL,
	CVP_ERR_START_CODE_NOT_FOUND,
	CVP_ERR_NOC_ERROR,
	CVP_ERR_CLIENT_PRESENT = 0x90000001,
	CVP_ERR_CLIENT_FATAL,
	CVP_ERR_CMD_QUEUE_FULL,
	CVP_ERR_UNUSED = 0x10000000
};

enum hal_property {
	HAL_UNUSED_PROPERTY = 0xFFFFFFFF,
};

enum hal_ssr_trigger_type {
	SSR_ERR_FATAL = 1,
	SSR_SW_DIV_BY_ZERO,
	SSR_HW_WDOG_IRQ,
	SSR_SESSION_ABORT,
};

enum hal_intra_refresh_mode {
	HAL_INTRA_REFRESH_NONE,
	HAL_INTRA_REFRESH_CYCLIC,
	HAL_INTRA_REFRESH_RANDOM,
	HAL_UNUSED_INTRA = 0x10000000,
};

enum cvp_resource_id {
	CVP_RESOURCE_NONE,
	CVP_RESOURCE_SYSCACHE,
	CVP_UNUSED_RESOURCE = 0x10000000,
};

struct cvp_resource_hdr {
	enum cvp_resource_id resource_id;
	void *resource_handle;
};

struct cvp_buffer_addr_info {
	enum hal_buffer buffer_type;
	u32 buffer_size;
	u32 num_buffers;
	u32 align_device_addr;
	u32 extradata_addr;
	u32 extradata_size;
	u32 response_required;
};

/* Needs to be exactly the same as hfi_buffer_info */
struct cvp_hal_buffer_info {
	u32 buffer_addr;
	u32 extra_data_addr;
};

struct cvp_hal_fw_info {
	char version[CVP_VERSION_LENGTH];
	phys_addr_t base_addr;
	int register_base;
	int register_size;
	int irq;
};

enum hal_flush {
	HAL_FLUSH_INPUT,
	HAL_FLUSH_OUTPUT,
	HAL_FLUSH_ALL,
	HAL_UNUSED_FLUSH = 0x10000000,
};

enum hal_event_type {
	HAL_EVENT_SEQ_CHANGED_SUFFICIENT_RESOURCES,
	HAL_EVENT_SEQ_CHANGED_INSUFFICIENT_RESOURCES,
	HAL_EVENT_RELEASE_BUFFER_REFERENCE,
	HAL_UNUSED_SEQCHG = 0x10000000,
};

/* HAL Response */
#define IS_HAL_SYS_CMD(cmd) ((cmd) >= HAL_SYS_INIT_DONE && \
		(cmd) <= HAL_SYS_ERROR)
#define IS_HAL_SESSION_CMD(cmd) ((cmd) >= HAL_SESSION_EVENT_CHANGE && \
		(cmd) <= HAL_SESSION_ERROR)
enum hal_command_response {
	HAL_NO_RESP,
	HAL_SYS_INIT_DONE,
	HAL_SYS_SET_RESOURCE_DONE,
	HAL_SYS_RELEASE_RESOURCE_DONE,
	HAL_SYS_PING_ACK_DONE,
	HAL_SYS_PC_PREP_DONE,
	HAL_SYS_IDLE,
	HAL_SYS_DEBUG,
	HAL_SYS_WATCHDOG_TIMEOUT,
	HAL_SYS_ERROR,
	/* SESSION COMMANDS_DONE */
	HAL_SESSION_EVENT_CHANGE,
	HAL_SESSION_INIT_DONE,
	HAL_SESSION_END_DONE,
	HAL_SESSION_SET_BUFFER_DONE,
	HAL_SESSION_ABORT_DONE,
	HAL_SESSION_STOP_DONE,
	HAL_SESSION_CVP_OPERATION_CONFIG,
	HAL_SESSION_FLUSH_DONE,
	HAL_SESSION_SUSPEND_DONE,
	HAL_SESSION_RESUME_DONE,
	HAL_SESSION_SET_PROP_DONE,
	HAL_SESSION_GET_PROP_DONE,
	HAL_SESSION_RELEASE_BUFFER_DONE,
	HAL_SESSION_REGISTER_BUFFER_DONE,
	HAL_SESSION_UNREGISTER_BUFFER_DONE,
	HAL_SESSION_RELEASE_RESOURCE_DONE,
	HAL_SESSION_DFS_CONFIG_CMD_DONE,
	HAL_SESSION_DFS_FRAME_CMD_DONE,
	HAL_SESSION_DME_CONFIG_CMD_DONE,
	HAL_SESSION_DME_BASIC_CONFIG_CMD_DONE,
	HAL_SESSION_DME_FRAME_CMD_DONE,
	HAL_SESSION_TME_CONFIG_CMD_DONE,
	HAL_SESSION_ODT_CONFIG_CMD_DONE,
	HAL_SESSION_OD_CONFIG_CMD_DONE,
	HAL_SESSION_NCC_CONFIG_CMD_DONE,
	HAL_SESSION_ICA_CONFIG_CMD_DONE,
	HAL_SESSION_HCD_CONFIG_CMD_DONE,
	HAL_SESSION_DC_CONFIG_CMD_DONE,
	HAL_SESSION_DCM_CONFIG_CMD_DONE,
	HAL_SESSION_PYS_HCD_CONFIG_CMD_DONE,
	HAL_SESSION_FD_CONFIG_CMD_DONE,
	HAL_SESSION_PERSIST_CMD_DONE,
	HAL_SESSION_MODEL_BUF_CMD_DONE,
	HAL_SESSION_ICA_FRAME_CMD_DONE,
	HAL_SESSION_FD_FRAME_CMD_DONE,
	HAL_SESSION_PROPERTY_INFO,
	HAL_SESSION_ERROR,
	HAL_RESPONSE_UNUSED = 0x10000000,
};

struct msm_cvp_capability {
	u32 reserved[183];
};

struct cvp_hal_sys_init_done {
	u32 dec_codec_supported;
	u32 enc_codec_supported;
	u32 codec_count;
	struct msm_cvp_capability *capabilities;
	u32 max_sessions_supported;
};

struct cvp_hal_session_init_done {
	struct msm_cvp_capability capability;
};

struct msm_cvp_cb_cmd_done {
	u32 device_id;
	void *session_id;
	enum cvp_status status;
	u32 size;
	union {
		struct cvp_hfi_msg_session_hdr msg_hdr;
		struct cvp_resource_hdr resource_hdr;
		struct cvp_buffer_addr_info buffer_addr_info;
		struct cvp_hal_sys_init_done sys_init_done;
		struct cvp_hal_session_init_done session_init_done;
		struct cvp_hal_buffer_info buffer_info;
		enum hal_flush flush_type;
	} data;
};

struct cvp_hal_index_extradata_input_crop_payload {
	u32 size;
	u32 version;
	u32 port_index;
	u32 left;
	u32 top;
	u32 width;
	u32 height;
};

struct msm_cvp_cb_event {
	u32 device_id;
	void *session_id;
	enum cvp_status status;
	u32 height;
	u32 width;
	int bit_depth;
	u32 hal_event_type;
	u32 packet_buffer;
	u32 extra_data_buffer;
	u32 pic_struct;
	u32 colour_space;
	u32 profile;
	u32 level;
	u32 entropy_mode;
	u32 capture_buf_count;
	struct cvp_hal_index_extradata_input_crop_payload crop_data;
};

struct msm_cvp_cb_data_done {
	u32 device_id;
	void *session_id;
	enum cvp_status status;
	u32 size;
	u32 clnt_data;
};

struct msm_cvp_cb_info {
	enum hal_command_response response_type;
	union {
		struct msm_cvp_cb_cmd_done cmd;
		struct msm_cvp_cb_event event;
		struct msm_cvp_cb_data_done data;
	} response;
};

enum msm_cvp_hfi_type {
	CVP_HFI_IRIS,
};

enum msm_cvp_thermal_level {
	CVP_THERMAL_NORMAL = 0,
	CVP_THERMAL_LOW,
	CVP_THERMAL_HIGH,
	CVP_THERMAL_CRITICAL
};

struct msm_cvp_gov_data {
	struct cvp_bus_vote_data *data;
	u32 data_count;
};

enum msm_cvp_power_mode {
	CVP_POWER_NORMAL = 0,
	CVP_POWER_LOW,
	CVP_POWER_TURBO
};

struct cvp_bus_vote_data {
	u32 domain;
	u32 ddr_bw;
	u32 sys_cache_bw;
	enum msm_cvp_power_mode power_mode;
	bool use_sys_cache;
};

struct cvp_hal_cmd_sys_get_property_packet {
	u32 size;
	u32 packet_type;
	u32 num_properties;
	u32 rg_property_data[1];
};

#define call_hfi_op(q, op, args...)			\
	(((q) && (q)->op) ? ((q)->op(args)) : 0)

struct msm_cvp_hfi_defs {
	unsigned int size;
	unsigned int type;
	unsigned int buf_offset;
	unsigned int buf_num;
	enum hal_command_response resp;
};

struct cvp_hfi_device {
	void *hfi_device_data;
	/*Add function pointers for all the hfi functions below*/
	int (*core_init)(void *device);
	int (*core_release)(void *device);
	int (*core_trigger_ssr)(void *device, enum hal_ssr_trigger_type);
	int (*session_init)(void *device, void *session_id, void **new_session);
	int (*session_end)(void *session);
	int (*session_abort)(void *session);
	int (*session_set_buffers)(void *sess,
				struct cvp_buffer_addr_info *buffer_info);
	int (*session_release_buffers)(void *sess,
				struct cvp_buffer_addr_info *buffer_info);
	int (*session_send)(void *sess,
		struct cvp_kmd_hfi_packet *in_pkt);
	int (*scale_clocks)(void *dev, u32 freq);
	int (*vote_bus)(void *dev, struct cvp_bus_vote_data *data,
			int num_data);
	int (*get_fw_info)(void *dev, struct cvp_hal_fw_info *fw_info);
	int (*session_clean)(void *sess);
	int (*get_core_capabilities)(void *dev);
	int (*suspend)(void *dev);
	int (*flush_debug_queue)(void *dev);
	int (*noc_error_info)(void *dev);
	int (*validate_session)(void *sess, const char *func);
};

typedef void (*hfi_cmd_response_callback) (enum hal_command_response cmd,
			void *data);
typedef void (*msm_cvp_callback) (u32 response, void *callback);

struct cvp_hfi_device *cvp_hfi_initialize(enum msm_cvp_hfi_type hfi_type,
		u32 device_id, struct msm_cvp_platform_resources *res,
		hfi_cmd_response_callback callback);
void cvp_hfi_deinitialize(enum msm_cvp_hfi_type hfi_type,
			struct cvp_hfi_device *hdev);

int get_pkt_index(struct cvp_hal_session_cmd_pkt *hdr);
int get_signal_from_pkt_type(unsigned int type);
int set_feature_bitmask(int pkt_index, unsigned long *bitmask);
int get_hfi_version(void);
unsigned int get_msg_size(void);
unsigned int get_msg_session_id(void *msg);
unsigned int get_msg_errorcode(void *msg);
int get_msg_opconfigs(void *msg, unsigned int *session_id,
		unsigned int *error_type, unsigned int *config_id);
extern const struct msm_cvp_hfi_defs cvp_hfi_defs[];

#endif /*__CVP_HFI_API_H__ */
