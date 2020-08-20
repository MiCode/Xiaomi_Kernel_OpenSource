/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */
#ifndef __MSM_CVP_PRIVATE_H__
#define __MSM_CVP_PRIVATE_H__

#include <linux/types.h>

/* Commands type */
#define CVP_KMD_CMD_BASE		0x10000000
#define CVP_KMD_CMD_START		(CVP_KMD_CMD_BASE + 0x1000)

/*
 * userspace clients pass one of the below arguments type
 * in struct cvp_kmd_arg (@type field).
 */

/*
 * CVP_KMD_GET_SESSION_INFO - this argument type is used to
 *          get the session information from driver. it passes
 *          struct cvp_kmd_session_info {}
 */
#define CVP_KMD_GET_SESSION_INFO	(CVP_KMD_CMD_START + 1)

/*
 * CVP_KMD_REGISTER_BUFFER - this argument type is used to
 *          register the buffer to driver. it passes
 *          struct cvp_kmd_buffer {}
 */
#define CVP_KMD_REGISTER_BUFFER		(CVP_KMD_CMD_START + 3)

/*
 * CVP_KMD_REGISTER_BUFFER - this argument type is used to
 *          unregister the buffer to driver. it passes
 *          struct cvp_kmd_buffer {}
 */
#define CVP_KMD_UNREGISTER_BUFFER	(CVP_KMD_CMD_START + 4)

#define CVP_KMD_UPDATE_POWER	(CVP_KMD_CMD_START + 17)

#define CVP_KMD_SEND_CMD_PKT	(CVP_KMD_CMD_START + 64)

#define CVP_KMD_RECEIVE_MSG_PKT	 (CVP_KMD_CMD_START + 65)

#define CVP_KMD_SET_SYS_PROPERTY	(CVP_KMD_CMD_START + 66)

#define CVP_KMD_GET_SYS_PROPERTY	(CVP_KMD_CMD_START + 67)

#define CVP_KMD_SESSION_CONTROL		(CVP_KMD_CMD_START + 68)

#define CVP_KMD_SEND_FENCE_CMD_PKT	(CVP_KMD_CMD_START + 69)

#define CVP_KMD_FLUSH_ALL	(CVP_KMD_CMD_START + 70)

#define CVP_KMD_FLUSH_FRAME	(CVP_KMD_CMD_START + 71)

/* flags */
#define CVP_KMD_FLAG_UNSECURE			0x00000000
#define CVP_KMD_FLAG_SECURE			0x00000001

/* buffer type */
#define CVP_KMD_BUFTYPE_INPUT			0x00000001
#define CVP_KMD_BUFTYPE_OUTPUT			0x00000002
#define CVP_KMD_BUFTYPE_INTERNAL_1		0x00000003
#define CVP_KMD_BUFTYPE_INTERNAL_2		0x00000004


/**
 * struct cvp_kmd_session_info - session information
 * @session_id:    current session id
 */
struct cvp_kmd_session_info {
	__u32 session_id;
	__u32 reserved[10];
};

/**
 * struct cvp_kmd_buffer - buffer information to be registered
 * @index:         index of buffer
 * @type:          buffer type
 * @fd:            file descriptor of buffer
 * @size:          allocated size of buffer
 * @offset:        offset in fd from where usable data starts
 * @pixelformat:   fourcc format
 * @flags:         buffer flags
 */
struct cvp_kmd_buffer {
	__u32 index;
	__u32 type;
	__u32 fd;
	__u32 size;
	__u32 offset;
	__u32 pixelformat;
	__u32 flags;
	__u32 reserved[5];
};

/**
 * struct cvp_kmd_send_cmd - sending generic HFI command
 * @cmd_address_fd:   file descriptor of cmd_address
 * @cmd_size:         allocated size of buffer
 */
struct cvp_kmd_send_cmd {
	__u32 cmd_address_fd;
	__u32 cmd_size;
	__u32 reserved[10];
};

/**
 * struct cvp_kmd_client_data - store generic client
 *                              data
 * @transactionid:  transaction id
 * @client_data1:   client data to be used during callback
 * @client_data2:   client data to be used during callback
 */
struct cvp_kmd_client_data {
	__u32 transactionid;
	__u32 client_data1;
	__u32 client_data2;
};


#define	MAX_HFI_PKT_SIZE	470

struct cvp_kmd_hfi_packet {
	__u32 pkt_data[MAX_HFI_PKT_SIZE];
};

#define CVP_KMD_PROP_HFI_VERSION	1
#define CVP_KMD_PROP_SESSION_TYPE	2
#define CVP_KMD_PROP_SESSION_KERNELMASK	3
#define CVP_KMD_PROP_SESSION_PRIORITY	4
#define CVP_KMD_PROP_SESSION_SECURITY	5
#define CVP_KMD_PROP_SESSION_DSPMASK	6

#define CVP_KMD_PROP_PWR_FDU	0x10
#define CVP_KMD_PROP_PWR_ICA	0x11
#define CVP_KMD_PROP_PWR_OD	0x12
#define CVP_KMD_PROP_PWR_MPU	0x13
#define CVP_KMD_PROP_PWR_FW	0x14
#define CVP_KMD_PROP_PWR_DDR	0x15
#define CVP_KMD_PROP_PWR_SYSCACHE	0x16
#define CVP_KMD_PROP_PWR_FDU_OP	0x17
#define CVP_KMD_PROP_PWR_ICA_OP	0x18
#define CVP_KMD_PROP_PWR_OD_OP	0x19
#define CVP_KMD_PROP_PWR_MPU_OP	0x1A
#define CVP_KMD_PROP_PWR_FW_OP	0x1B
#define CVP_KMD_PROP_PWR_DDR_OP	0x1C
#define CVP_KMD_PROP_PWR_SYSCACHE_OP	0x1D
#define CVP_KMD_PROP_PWR_FPS_FDU	0x1E
#define CVP_KMD_PROP_PWR_FPS_MPU	0x1F
#define CVP_KMD_PROP_PWR_FPS_OD	0x20
#define CVP_KMD_PROP_PWR_FPS_ICA	0x21

#define MAX_KMD_PROP_NUM_PER_PACKET		8
#define MAX_KMD_PROP_TYPE	(CVP_KMD_PROP_PWR_FPS_ICA + 1)

struct cvp_kmd_sys_property {
	__u32 prop_type;
	__u32 data;
};

struct cvp_kmd_sys_properties {
	__u32 prop_num;
	struct cvp_kmd_sys_property prop_data[MAX_KMD_PROP_NUM_PER_PACKET];
};

#define SESSION_CREATE	1
#define SESSION_DELETE	2
#define SESSION_START	3
#define SESSION_STOP	4
#define SESSION_INFO	5

struct cvp_kmd_session_control {
	__u32 ctrl_type;
	__u32 ctrl_data[8];
};

#define MAX_HFI_FENCE_SIZE	16
#define MAX_HFI_FENCE_OFFSET	(MAX_HFI_PKT_SIZE-MAX_HFI_FENCE_SIZE)
struct cvp_kmd_hfi_fence_packet {
	__u32 pkt_data[MAX_HFI_FENCE_OFFSET];
	__u32 fence_data[MAX_HFI_FENCE_SIZE];
	__u64 frame_id;
};

struct cvp_kmd_fence {
	__s32 h_synx;
	__u32 secure_key;
};

struct cvp_kmd_fence_ctrl {
	__u32 magic;
	__u32 reserved;
	__u64 frame_id;
	__u32 num_fences;
	__u32 output_index;
	struct cvp_kmd_fence fences[MAX_HFI_FENCE_SIZE/2];
};

#define MAX_FENCE_DATA_SIZE	(MAX_HFI_FENCE_SIZE + 6)

struct cvp_kmd_hfi_synx_packet {
	__u32 pkt_data[MAX_HFI_PKT_SIZE];
	union {
		__u32 fence_data[MAX_FENCE_DATA_SIZE];
		struct cvp_kmd_fence_ctrl fc;
	};
};

/**
 * struct cvp_kmd_arg
 *
 * @type:          command type
 * @buf_offset:    offset to buffer list in the command
 * @buf_num:       number of buffers in the command
 * @session:       session information
 * @req_power:     power information
 * @regbuf:        buffer to be registered
 * @unregbuf:      buffer to be unregistered
 * @send_cmd:      sending generic HFI command

 * @hfi_pkt:       HFI packet created by user library
 * @sys_properties System properties read or set by user library
 * @hfi_fence_pkt: HFI fence packet created by user library
 */
struct cvp_kmd_arg {
	__u32 type;
	__u32 buf_offset;
	__u32 buf_num;
	union cvp_data_t {
		struct cvp_kmd_session_info session;
		struct cvp_kmd_buffer regbuf;
		struct cvp_kmd_buffer unregbuf;
		struct cvp_kmd_send_cmd send_cmd;
		struct cvp_kmd_hfi_packet hfi_pkt;
		struct cvp_kmd_sys_properties sys_properties;
		struct cvp_kmd_hfi_fence_packet hfi_fence_pkt;
		struct cvp_kmd_hfi_synx_packet hfi_synx_pkt;
		struct cvp_kmd_session_control session_ctrl;
		__u64 frame_id;
	} data;
};

struct cvp_kmd_request_power {
	__u32 deprecated;
};
#endif
