/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */
#ifndef __MSM_CVP_PRIVATE_H__
#define __MSM_CVP_PRIVATE_H__

#include <linux/videodev2.h>

#define MAX_DFS_HFI_PARAMS 20
#define HFI_MAX_PLANES 4

/* VIDIOC private cvp command */
#define VIDIOC_CVP_CMD \
		_IOWR('V', BASE_VIDIOC_PRIVATE_CVP, struct cvp_kmd_arg)

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
 * CVP_KMD_REQUEST_POWER - this argument type is used to
 *          set the power required to driver. it passes
 *          struct cvp_kmd_request_power {}
 */
#define CVP_KMD_REQUEST_POWER		(CVP_KMD_CMD_START + 2)

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

#define CVP_KMD_HFI_SEND_CMD        (CVP_KMD_CMD_START + 5)

#define CVP_KMD_HFI_DFS_CONFIG_CMD  (CVP_KMD_CMD_START + 6)

#define CVP_KMD_HFI_DFS_FRAME_CMD  (CVP_KMD_CMD_START + 7)

#define CVP_KMD_HFI_DFS_FRAME_CMD_RESPONSE  (CVP_KMD_CMD_START + 8)

#define CVP_KMD_HFI_DME_CONFIG_CMD  (CVP_KMD_CMD_START + 9)

#define CVP_KMD_HFI_DME_FRAME_CMD  (CVP_KMD_CMD_START + 10)

#define CVP_KMD_HFI_DME_FRAME_CMD_RESPONSE  (CVP_KMD_CMD_START + 11)

#define CVP_KMD_HFI_PERSIST_CMD  (CVP_KMD_CMD_START + 12)

#define CVP_KMD_HFI_PERSIST_CMD_RESPONSE  (CVP_KMD_CMD_START + 13)

#define CVP_KMD_HFI_DME_FRAME_FENCE_CMD  (CVP_KMD_CMD_START + 14)

#define CVP_KMD_HFI_ICA_FRAME_CMD  (CVP_KMD_CMD_START + 15)

#define CVP_KMD_HFI_FD_FRAME_CMD  (CVP_KMD_CMD_START + 16)

#define CVP_KMD_UPDATE_POWER	(CVP_KMD_CMD_START + 17)

#define CVP_KMD_SEND_CMD_PKT	(CVP_KMD_CMD_START + 64)

#define CVP_KMD_RECEIVE_MSG_PKT	 (CVP_KMD_CMD_START + 65)

#define CVP_KMD_SET_SYS_PROPERTY	(CVP_KMD_CMD_START + 66)

#define CVP_KMD_GET_SYS_PROPERTY	(CVP_KMD_CMD_START + 67)

#define CVP_KMD_SESSION_CONTROL		(CVP_KMD_CMD_START + 68)

#define CVP_KMD_SEND_FENCE_CMD_PKT   (0x10001000 + 69)

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
	unsigned int session_id;
	unsigned int reserved[10];
};

/**
 * struct cvp_kmd_request_power - power / clock data information
 * @clock_cycles_a:  clock cycles per second required for hardware_a
 * @clock_cycles_b:  clock cycles per second required for hardware_b
 * @ddr_bw:        bandwidth required for ddr in bps
 * @sys_cache_bw:  bandwidth required for system cache in bps
 */
struct cvp_kmd_request_power {
	unsigned int clock_cycles_a;
	unsigned int clock_cycles_b;
	unsigned int ddr_bw;
	unsigned int sys_cache_bw;
	unsigned int reserved[8];
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
	unsigned int index;
	unsigned int type;
	unsigned int fd;
	unsigned int size;
	unsigned int offset;
	unsigned int pixelformat;
	unsigned int flags;
	unsigned int reserved[5];
};

/**
 * struct cvp_kmd_send_cmd - sending generic HFI command
 * @cmd_address_fd:   file descriptor of cmd_address
 * @cmd_size:         allocated size of buffer
 */
struct cvp_kmd_send_cmd {
	unsigned int cmd_address_fd;
	unsigned int cmd_size;
	unsigned int reserved[10];
};

/**
 * struct cvp_kmd_color_plane_info - color plane info
 * @stride:      stride of plane
 * @buf_size:    size of plane
 */
struct cvp_kmd_color_plane_info {
	int stride[HFI_MAX_PLANES];
	unsigned int buf_size[HFI_MAX_PLANES];
};

/**
 * struct cvp_kmd_client_data - store generic client
 *                              data
 * @transactionid:  transaction id
 * @client_data1:   client data to be used during callback
 * @client_data2:   client data to be used during callback
 */
struct cvp_kmd_client_data {
	unsigned int transactionid;
	unsigned int client_data1;
	unsigned int client_data2;
};

#define CVP_COLOR_PLANE_INFO_SIZE \
	sizeof(struct cvp_kmd_color_plane_info)
#define CVP_CLIENT_DATA_SIZE	sizeof(struct cvp_kmd_client_data)
#define CVP_DFS_CONFIG_CMD_SIZE   38
#define CVP_DFS_FRAME_CMD_SIZE 16
#define CVP_DFS_FRAME_BUFFERS_OFFSET 8

#define CVP_DME_CONFIG_CMD_SIZE   194
#define CVP_DME_FRAME_CMD_SIZE 28
#define CVP_DME_FRAME_BUFFERS_OFFSET 12
#define CVP_DME_BUF_NUM	8

#define CVP_PERSIST_CMD_SIZE 11
#define CVP_PERSIST_BUFFERS_OFFSET 7
#define CVP_PERSIST_BUF_NUM	2

struct cvp_kmd_dfs_config {
	unsigned int cvp_dfs_config[CVP_DFS_CONFIG_CMD_SIZE];
};

struct cvp_kmd_dfs_frame {
	unsigned int frame_data[CVP_DFS_FRAME_CMD_SIZE];
};

struct cvp_kmd_dme_config {
	unsigned int cvp_dme_config[CVP_DME_CONFIG_CMD_SIZE];
};

struct cvp_kmd_dme_frame {
	unsigned int frame_data[CVP_DME_FRAME_CMD_SIZE];
};

struct cvp_kmd_persist_buf {
	unsigned int persist_data[CVP_PERSIST_CMD_SIZE];
};

#define	MAX_HFI_PKT_SIZE	470

struct cvp_kmd_hfi_packet {
	unsigned int pkt_data[MAX_HFI_PKT_SIZE];
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

#define MAX_KMD_PROP_NUM	(CVP_KMD_PROP_PWR_SYSCACHE_OP + 1)

struct cvp_kmd_sys_property {
	unsigned int prop_type;
	unsigned int data;
};

struct cvp_kmd_sys_properties {
	unsigned int prop_num;
	struct cvp_kmd_sys_property prop_data;
};

#define SESSION_CREATE	1
#define SESSION_DELETE	2
#define SESSION_START	3
#define SESSION_STOP	4
#define SESSION_INFO	5

struct cvp_kmd_session_control {
	unsigned int ctrl_type;
	unsigned int ctrl_data[8];
};

#define MAX_HFI_FENCE_SIZE        16
#define	MAX_HFI_FENCE_OFFSET	(MAX_HFI_PKT_SIZE-MAX_HFI_FENCE_SIZE)
struct cvp_kmd_hfi_fence_packet {
	unsigned int pkt_data[MAX_HFI_FENCE_OFFSET];
	unsigned int fence_data[MAX_HFI_FENCE_SIZE];
};


/**
 * struct cvp_kmd_arg - argument passed with VIDIOC_CVP_CMD
 * To be deprecated
 * @type:          command type
 * @buf_offset:    offset to buffer list in the command
 * @buf_num:       number of buffers in the command
 * @session:       session information
 * @req_power:     power information
 * @regbuf:        buffer to be registered
 * @unregbuf:      buffer to be unregistered
 * @send_cmd:      sending generic HFI command
 * @dfs_config:    sending DFS config command
 * @dfs_frame:     sending DFS frame command
 * @hfi_pkt:       HFI packet created by user library
 * @sys_properties System properties read or set by user library
 * @hfi_fence_pkt: HFI fence packet created by user library
 */
struct cvp_kmd_arg {
	unsigned int type;
	unsigned int buf_offset;
	unsigned int buf_num;
	union cvp_data_t {
		struct cvp_kmd_session_info session;
		struct cvp_kmd_request_power req_power;
		struct cvp_kmd_buffer regbuf;
		struct cvp_kmd_buffer unregbuf;
		struct cvp_kmd_send_cmd send_cmd;
		struct cvp_kmd_dfs_config dfs_config;
		struct cvp_kmd_dfs_frame dfs_frame;
		struct cvp_kmd_dme_config dme_config;
		struct cvp_kmd_dme_frame dme_frame;
		struct cvp_kmd_persist_buf pbuf_cmd;
		struct cvp_kmd_hfi_packet hfi_pkt;
		struct cvp_kmd_sys_properties sys_properties;
		struct cvp_kmd_hfi_fence_packet hfi_fence_pkt;
		struct cvp_kmd_session_control session_ctrl;
	} data;
};
#endif
