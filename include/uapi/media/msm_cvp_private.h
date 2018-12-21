/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */
#ifndef __MSM_CVP_PRIVATE_H__
#define __MSM_CVP_PRIVATE_H__

#include <linux/videodev2.h>

#define MAX_DFS_HFI_PARAMS 20

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
 * enum HFI_COLOR_PLANE_TYPE - define the type of plane
 */
enum HFI_COLOR_PLANE_TYPE {
	HFI_COLOR_PLANE_METADATA,
	HFI_COLOR_PLANE_PICDATA,
	HFI_MAX_PLANES
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

/**
 * struct msm_cvp_dfsconfig - dfs config packet
 * @cmd_size:               command size in bytes
 * @cmd_address:            command address
 * @size:                   packet size in bytes
 * @packet_type:            HFI_CMD_SESSION_CVP_DFS
 * @session_id:             id value associated with a session
 * @srcbuffer_format:       buffer format of source imagesize
 * @left_plane_info:        left view buffer plane info
 * @right_plane_info:       right view buffer plane info
 * @width:                  image width
 * @height:                 image height
 * @occlusionmask_enable:   0: disable, 1: enable
 * @occlusioncost:          occlusion cost threshold
 * @occlusionbound:         occlusion bound
 * @occlusionshift:         occlusion shift
 * @maxdisparity:           max disparitymap in integer precision
 * @disparityoffset:        disparity offset
 * @medianfilter_enable:    enable median filter on disparity map
 * @occlusionfilling_enable:0: disable, 1: enable
 * @occlusionmaskdump:      0: disable, 1: enable
 * @clientdata:             client data for mapping command
 *                          and message pairs
 */
struct msm_cvp_dfsconfig {
	unsigned int cmd_size;
	unsigned int cmd_address;
	unsigned int size;
	unsigned int packet_type;
	unsigned int session_id;
	unsigned int srcbuffer_format;
	struct msm_cvp_color_plane_info left_plane_info;
	struct msm_cvp_color_plane_info right_plane_info;
	unsigned int width;
	unsigned int height;
	unsigned int occlusionmask_enable;
	unsigned int occlusioncost;
	unsigned int occlusionbound;
	unsigned int occlusionshift;
	unsigned int maxdisparity;
	unsigned int disparityoffset;
	unsigned int medianfilter_enable;
	unsigned int occlusionfilling_enable;
	unsigned int occlusionmaskdump;
	struct msm_cvp_client_data clientdata;
	unsigned int reserved[MAX_DFS_HFI_PARAMS];
};

/**
 * struct msm_cvp_dfsframe - dfs frame packet
 * @cmd_size:                command size in bytes
 * @cmd_address:             command address
 * @size:                    packet size in bytes
 * @packet_type:             HFI_CMD_SESSION_CVP_DFS
 * @session_id:              id value associated with a session
 * @left_buffer_index:       left buffer index
 * @right_buffer_index:      right buffer index
 * @disparitymap_buffer_idx: disparity map buffer index
 * @occlusionmask_buffer_idx:occlusion mask buffer index
 */
struct msm_cvp_dfsframe {
	unsigned int cmd_size;
	unsigned int cmd_address;
	unsigned int size;
	unsigned int packet_type;
	unsigned int session_id;
	unsigned int left_buffer_index;
	unsigned int right_buffer_index;
	unsigned int disparitymap_buffer_idx;
	unsigned int occlusionmask_buffer_idx;
	struct msm_cvp_client_data clientdata;
};

/**
 * struct msm_cvp_arg - argument passed with VIDIOC_CVP_CMD
 * @type:          command type
 * @session:       session information
 * @req_power:     power information
 * @regbuf:        buffer to be registered
 * @unregbuf:      buffer to be unregistered
 * @send_cmd:      sending generic HFI command
 * @dfsconfig:     sending DFS config command
 * @dfsframe:      sending DFS frame command
 */
struct msm_cvp_arg {
	unsigned int type;
	union data_t {
		struct msm_cvp_session_info session;
		struct msm_cvp_request_power req_power;
		struct msm_cvp_buffer regbuf;
		struct msm_cvp_buffer unregbuf;
		struct msm_cvp_send_cmd send_cmd;
		struct msm_cvp_dfsconfig dfsconfig;
		struct msm_cvp_dfsframe dfsframe;
	} data;
	unsigned int reserved[12];
};

#endif
