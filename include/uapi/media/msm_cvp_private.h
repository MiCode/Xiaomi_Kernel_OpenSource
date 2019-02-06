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
		_IOWR('V', BASE_VIDIOC_PRIVATE_CVP, struct msm_cvp_arg)

/* Commands type */
#define MSM_VIDC_CMD_START		0x10000000
#define MSM_CVP_CMD_START		(MSM_VIDC_CMD_START + 0x1000)

/*
 * userspace clients pass one of the below arguments type
 * in struct msm_cvp_arg (@type field).
 */

/*
 * MSM_CVP_GET_SESSION_INFO - this argument type is used to
 *          get the session information from driver. it passes
 *          struct msm_cvp_session_info {}
 */
#define MSM_CVP_GET_SESSION_INFO	(MSM_CVP_CMD_START + 1)

/*
 * MSM_CVP_REQUEST_POWER - this argument type is used to
 *          set the power required to driver. it passes
 *          struct msm_cvp_request_power {}
 */
#define MSM_CVP_REQUEST_POWER		(MSM_CVP_CMD_START + 2)

/*
 * MSM_CVP_REGISTER_BUFFER - this argument type is used to
 *          register the buffer to driver. it passes
 *          struct msm_cvp_buffer {}
 */
#define MSM_CVP_REGISTER_BUFFER		(MSM_CVP_CMD_START + 3)

/*
 * MSM_CVP_REGISTER_BUFFER - this argument type is used to
 *          unregister the buffer to driver. it passes
 *          struct msm_cvp_buffer {}
 */
#define MSM_CVP_UNREGISTER_BUFFER	(MSM_CVP_CMD_START + 4)

#define MSM_CVP_HFI_SEND_CMD        (MSM_CVP_CMD_START + 5)

#define MSM_CVP_HFI_DFS_CONFIG_CMD  (MSM_CVP_CMD_START + 6)

#define MSM_CVP_HFI_DFS_FRAME_CMD  (MSM_CVP_CMD_START + 7)

#define MSM_CVP_HFI_DFS_FRAME_CMD_RESPONSE  (MSM_CVP_CMD_START + 8)

#define MSM_CVP_HFI_DME_CONFIG_CMD  (MSM_CVP_CMD_START + 9)

#define MSM_CVP_HFI_DME_FRAME_CMD  (MSM_CVP_CMD_START + 10)

#define MSM_CVP_HFI_DME_FRAME_CMD_RESPONSE  (MSM_CVP_CMD_START + 11)

#define MSM_CVP_HFI_PERSIST_CMD  (MSM_CVP_CMD_START + 12)

#define MSM_CVP_HFI_PERSIST_CMD_RESPONSE  (MSM_CVP_CMD_START + 13)

/* flags */
#define MSM_CVP_FLAG_UNSECURE			0x00000000
#define MSM_CVP_FLAG_SECURE			0x00000001

/* buffer type */
#define MSM_CVP_BUFTYPE_INPUT			0x00000001
#define MSM_CVP_BUFTYPE_OUTPUT			0x00000002
#define MSM_CVP_BUFTYPE_INTERNAL_1		0x00000003
#define MSM_CVP_BUFTYPE_INTERNAL_2		0x00000004


/**
 * struct msm_cvp_session_info - session information
 * @session_id:    current session id
 */
struct msm_cvp_session_info {
	unsigned int session_id;
	unsigned int reserved[10];
};

/**
 * struct msm_cvp_request_power - power / clock data information
 * @clock_cycles_a:  clock cycles per second required for hardware_a
 * @clock_cycles_b:  clock cycles per second required for hardware_b
 * @ddr_bw:        bandwidth required for ddr in bps
 * @sys_cache_bw:  bandwidth required for system cache in bps
 */
struct msm_cvp_request_power {
	unsigned int clock_cycles_a;
	unsigned int clock_cycles_b;
	unsigned int ddr_bw;
	unsigned int sys_cache_bw;
	unsigned int reserved[8];
};

/**
 * struct msm_cvp_buffer - buffer information to be registered
 * @index:         index of buffer
 * @type:          buffer type
 * @fd:            file descriptor of buffer
 * @size:          allocated size of buffer
 * @offset:        offset in fd from where usable data starts
 * @pixelformat:   fourcc format
 * @flags:         buffer flags
 */
struct msm_cvp_buffer {
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
 * struct msm_cvp_send_cmd - sending generic HFI command
 * @cmd_address_fd:   file descriptor of cmd_address
 * @cmd_size:         allocated size of buffer
 */
struct msm_cvp_send_cmd {
	unsigned int cmd_address_fd;
	unsigned int cmd_size;
	unsigned int reserved[10];
};

/**
 * struct msm_cvp_color_plane_info - color plane info
 * @stride:      stride of plane
 * @buf_size:    size of plane
 */
struct msm_cvp_color_plane_info {
	int stride[HFI_MAX_PLANES];
	unsigned int buf_size[HFI_MAX_PLANES];
};

/**
 * struct msm_cvp_client_data - store generic client
 *                              data
 * @transactionid:  transaction id
 * @client_data1:   client data to be used during callback
 * @client_data2:   client data to be used during callback
 */
struct msm_cvp_client_data {
	unsigned int transactionid;
	unsigned int client_data1;
	unsigned int client_data2;
};

#define CVP_COLOR_PLANE_INFO_SIZE \
	sizeof(struct msm_cvp_color_plane_info)
#define CVP_CLIENT_DATA_SIZE	sizeof(struct msm_cvp_client_data)
#define CVP_DFS_CONFIG_CMD_SIZE   38
#define CVP_DFS_FRAME_CMD_SIZE 16
#define CVP_DFS_FRAME_BUFFERS_OFFSET 8

#define CVP_DME_CONFIG_CMD_SIZE   181
#define CVP_DME_FRAME_CMD_SIZE 28
#define CVP_DME_FRAME_BUFFERS_OFFSET 12
#define CVP_DME_BUF_NUM	8

#define CVP_PERSIST_CMD_SIZE 11
#define CVP_PERSIST_BUFFERS_OFFSET 7
#define CVP_PSRSIST_BUF_NUM	2

struct msm_cvp_dfs_config {
	unsigned int cvp_dfs_config[CVP_DFS_CONFIG_CMD_SIZE];
};

struct msm_cvp_dfs_frame {
	unsigned int frame_data[CVP_DFS_FRAME_CMD_SIZE];
};

struct msm_cvp_dme_config {
	unsigned int cvp_dme_config[CVP_DME_CONFIG_CMD_SIZE];
};

struct msm_cvp_dme_frame {
	unsigned int frame_data[CVP_DME_FRAME_CMD_SIZE];
};

struct msm_cvp_persist_buf {
	unsigned int persist_data[CVP_PERSIST_CMD_SIZE];
};

/**
 * struct msm_cvp_arg - argument passed with VIDIOC_CVP_CMD
 * @type:          command type
 * @session:       session information
 * @req_power:     power information
 * @regbuf:        buffer to be registered
 * @unregbuf:      buffer to be unregistered
 * @send_cmd:      sending generic HFI command
 * @dfs_config:    sending DFS config command
 * @dfs_frame:     sending DFS frame command
 */
struct msm_cvp_arg {
	unsigned int type;
	union data_t {
		struct msm_cvp_session_info session;
		struct msm_cvp_request_power req_power;
		struct msm_cvp_buffer regbuf;
		struct msm_cvp_buffer unregbuf;
		struct msm_cvp_send_cmd send_cmd;
		struct msm_cvp_dfs_config dfs_config;
		struct msm_cvp_dfs_frame dfs_frame;
		struct msm_cvp_dme_config dme_config;
		struct msm_cvp_dme_frame dme_frame;
		struct msm_cvp_persist_buf pbuf_cmd;
	} data;
	unsigned int reserved[12];
};

#endif
