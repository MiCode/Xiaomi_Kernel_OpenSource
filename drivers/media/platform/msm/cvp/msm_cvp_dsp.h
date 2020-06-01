/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#ifndef MSM_CVP_DSP_H
#define MSM_CVP_DSP_H

#include <linux/types.h>
#include "msm_cvp_debug.h"
#include "cvp_core_hfi.h"

#define CVP_APPS_DSP_GLINK_GUID "cvp-glink-apps-dsp"
#define CVP_APPS_DSP_SMD_GUID "cvp-smd-apps-dsp"

#define VMID_CDSP_Q6 (30)
#define HLOS_VM_NUM 1
#define DSP_VM_NUM 2
#define CVP_DSP_MAX_RESERVED 5
#define CVP_DSP_RESPONSE_TIMEOUT 1000

int cvp_dsp_device_init(void);
void cvp_dsp_device_exit(void);
void cvp_dsp_send_hfi_queue(void);

enum DSP_COMMAND {
	CVP_DSP_SEND_HFI_QUEUE = 0,
	CVP_DSP_SUSPEND = 1,
	CVP_DSP_RESUME = 2,
	CVP_DSP_SHUTDOWN = 3,
	CVP_DSP_REGISTER_BUFFER = 4,
	CVP_DSP_DEREGISTER_BUFFER = 5,
	CVP_DSP_MAX_CMD
};

struct cvp_dsp_cmd_msg {
	uint32_t type;
	int32_t ret;
	uint64_t msg_ptr;
	uint32_t msg_ptr_len;
	uint32_t buff_fd_iova;
	uint32_t buff_index;
	uint32_t buff_size;
	uint32_t session_id;
	int32_t ddr_type;
	uint32_t buff_fd;
	uint32_t buff_offset;
	uint32_t buff_fd_size;
	uint32_t reserved1;
	uint32_t reserved2;
};

struct cvp_dsp_rsp_msg {
	uint32_t type;
	int32_t ret;
	uint32_t reserved[CVP_DSP_MAX_RESERVED];
};

/*
 * API for CVP driver to suspend CVP session during
 * power collapse
 *
 * @param session_flag
 * Flag to share details of session.
 */
int cvp_dsp_suspend(uint32_t session_flag);

/*
 * API for CVP driver to resume CVP session during
 * power collapse
 *
 * @param session_flag
 * Flag to share details of session.
 */
int cvp_dsp_resume(uint32_t session_flag);

/*
 * API for CVP driver to shutdown CVP session during
 * cvp subsystem error.
 *
 * @param session_flag
 * Flag to share details of session.
 */
int cvp_dsp_shutdown(uint32_t session_flag);

/*
 * API to register iova buffer address with CDSP
 *
 * @session_id:     cvp session id
 * @buff_fd:        buffer fd
 * @buff_fd_size:   total size of fd in bytes
 * @buff_size:      size in bytes of cvp buffer
 * @buff_offset:    buffer offset
 * @buff_index:     buffer index
 * @iova_buff_addr: IOVA buffer address
 */
int cvp_dsp_register_buffer(uint32_t session_id, uint32_t buff_fd,
			uint32_t buff_fd_size, uint32_t buff_size,
			uint32_t buff_offset, uint32_t buff_index,
			uint32_t buff_fd_iova);

/*
 * API to de-register iova buffer address from CDSP
 *
 * @session_id:     cvp session id
 * @buff_fd:        buffer fd
 * @buff_fd_size:   total size of fd in bytes
 * @buff_size:      size in bytes of cvp buffer
 * @buff_offset:    buffer offset
 * @buff_index:     buffer index
 * @iova_buff_addr: IOVA buffer address
 */
int cvp_dsp_deregister_buffer(uint32_t session_id, uint32_t buff_fd,
			uint32_t buff_fd_size, uint32_t buff_size,
			uint32_t buff_offset, uint32_t buff_index,
			uint32_t buff_fd_iova);

#endif // MSM_CVP_DSP_H

