/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef __MSM_VIDC_PRIVATE_H__
#define __MSM_VIDC_PRIVATE_H__

#include <linux/videodev2.h>

/* VIDIOC private video command */
#define VIDIOC_VIDEO_CMD \
		_IOWR('V', BASE_VIDIOC_PRIVATE_VIDEO, struct msm_vidc_arg)

/* Commands type */
#define MSM_VIDC_CMD_START			0x10000000
#define MSM_CVP_START				(MSM_VIDC_CMD_START + 0x1000)

/*
 * userspace clients pass one of the below arguments type
 * in struct msm_vidc_arg (@type field).
 */

/*
 * MSM_CVP_GET_SESSION_INFO - this argument type is used to
 *          get the session information from driver. it passes
 *          struct msm_cvp_session_info {}
 */
#define MSM_CVP_GET_SESSION_INFO	(MSM_CVP_START + 1)

/*
 * MSM_CVP_REQUEST_POWER - this argument type is used to
 *          set the power required to driver. it passes
 *          struct msm_cvp_request_power {}
 */
#define MSM_CVP_REQUEST_POWER		(MSM_CVP_START + 2)

/*
 * MSM_CVP_REGISTER_BUFFER - this argument type is used to
 *          register the buffer to driver. it passes
 *          struct msm_cvp_buffer {}
 */
#define MSM_CVP_REGISTER_BUFFER		(MSM_CVP_START + 3)

/*
 * MSM_CVP_REGISTER_BUFFER - this argument type is used to
 *          unregister the buffer to driver. it passes
 *          struct msm_cvp_buffer {}
 */
#define MSM_CVP_UNREGISTER_BUFFER	(MSM_CVP_START + 4)

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
 * struct msm_vidc_arg - argument passed with VIDIOC_VIDEO_CMD
 * @type:          command type
 * @session:       session information
 * @req_power:     power information
 * @regbuf:        buffer to be registered
 * @unregbuf:      buffer to be unregistered
 */
struct msm_vidc_arg {
	unsigned int type;
	union data_t {
		struct msm_cvp_session_info session;
		struct msm_cvp_request_power req_power;
		struct msm_cvp_buffer regbuf;
		struct msm_cvp_buffer unregbuf;
	} data;
	unsigned int reserved[12];
};

#endif
