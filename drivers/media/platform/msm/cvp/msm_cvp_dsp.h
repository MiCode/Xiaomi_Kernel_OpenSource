/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef MSM_CVP_DSP_H
#define MSM_CVP_DSP_H

#include <linux/types.h>

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
	uint32_t size_in_bytes);

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

#endif // MSM_CVP_DSP_H

