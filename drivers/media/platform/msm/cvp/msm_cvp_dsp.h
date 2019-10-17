/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef MSM_CVP_DSP_H
#define MSM_CVP_DSP_H

#include <linux/types.h>
#include "msm_cvp_debug.h"
#include "cvp_core_hfi.h"

#define CVP_APPS_DSP_GLINK_GUID "cvp-glink-apps-dsp"
#define CVP_APPS_DSP_SMD_GUID "cvp-smd-apps-dsp"

/*
 * API for CVP driver to send physical address to dsp driver
 * @param phys_addr
 * Physical address of command message queue
 * that needs to be mapped to CDSP.
 * It should be allocated from CMA adsp_mem region.
 *
 * @param size_in_bytes
 * Size in bytes of command message queue
 */
int cvp_dsp_send_cmd_hfi_queue(phys_addr_t *phys_addr,
	uint32_t size_in_bytes, struct iris_hfi_device *device);

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
 * API for CVP driver to set CVP status during
 * cvp subsystem error.
 *
 */
void cvp_dsp_set_cvp_ssr(void);

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

