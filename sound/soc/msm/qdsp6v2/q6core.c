/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <mach/msm_smd.h>
#include <mach/qdsp6v2/apr.h>
#include "q6core.h"
#include <mach/ocmem.h>

#define TIMEOUT_MS 1000

struct q6core_str {
	struct apr_svc *core_handle_q;
	wait_queue_head_t bus_bw_req_wait;
	u32 bus_bw_resp_received;
	struct avcs_cmd_rsp_get_low_power_segments_info_t *lp_ocm_payload;
};

struct q6core_str q6core_lcl;

static int32_t aprv2_core_fn_q(struct apr_client_data *data, void *priv)
{
	uint32_t *payload1;
	uint32_t nseg;
	int i, j;

	pr_info("core msg: payload len = %u, apr resp opcode = 0x%X\n",
		data->payload_size, data->opcode);

	switch (data->opcode) {

	case APR_BASIC_RSP_RESULT:{

		if (data->payload_size == 0) {
			pr_err("%s: APR_BASIC_RSP_RESULT No Payload ",
					__func__);
			return 0;
		}

		payload1 = data->payload;

		switch (payload1[0]) {

		case AVCS_CMD_GET_LOW_POWER_SEGMENTS_INFO:
			pr_info("%s: Cmd = AVCS_CMD_GET_LOW_POWER_SEGMENTS_INFO status[0x%x]\n",
				__func__, payload1[1]);
			break;
		default:
			pr_err("Invalid cmd rsp[0x%x][0x%x]\n",
					payload1[0], payload1[1]);
			break;
		}
		break;
	}

	case AVCS_CMDRSP_GET_LOW_POWER_SEGMENTS_INFO:
		payload1 = data->payload;
		pr_info("%s: cmd = AVCS_CMDRSP_GET_LOW_POWER_SEGMENTS_INFO num_segments = 0x%x\n",
					__func__, payload1[0]);
		nseg = payload1[0];
		q6core_lcl.lp_ocm_payload->num_segments = nseg;
		q6core_lcl.lp_ocm_payload->bandwidth = payload1[1];
		for (i = 0, j = 2; i < nseg; i++) {
			q6core_lcl.lp_ocm_payload->mem_segment[i].type =
					(payload1[j] & 0xffff);
			q6core_lcl.lp_ocm_payload->mem_segment[i].category =
					((payload1[j++] >> 16) & 0xffff);
			q6core_lcl.lp_ocm_payload->mem_segment[i].size =
					payload1[j++];
			q6core_lcl.lp_ocm_payload->
				mem_segment[i].start_address_lsw =
				payload1[j++];
			q6core_lcl.lp_ocm_payload->
				mem_segment[i].start_address_msw =
				payload1[j++];
		}

		q6core_lcl.bus_bw_resp_received = 1;
		wake_up(&q6core_lcl.bus_bw_req_wait);
		break;

	case RESET_EVENTS:{
		pr_debug("Reset event received in Core service");
		apr_reset(q6core_lcl.core_handle_q);
		q6core_lcl.core_handle_q = NULL;
		break;
	}

	default:
		pr_err("Message id from adsp core svc: %d\n", data->opcode);
		break;
	}

	return 0;
}


void ocm_core_open(void)
{
	if (q6core_lcl.core_handle_q == NULL)
		q6core_lcl.core_handle_q = apr_register("ADSP", "CORE",
					aprv2_core_fn_q, 0xFFFFFFFF, NULL);
	pr_debug("Open_q %p\n", q6core_lcl.core_handle_q);
	if (q6core_lcl.core_handle_q == NULL)
		pr_err("%s: Unable to register CORE\n", __func__);
}

int core_get_low_power_segments(
		struct avcs_cmd_rsp_get_low_power_segments_info_t **lp_memseg)
{
	struct avcs_cmd_get_low_power_segments_info lp_ocm_cmd;
	u8 *cptr = NULL;
	int ret = 0;

	pr_debug("%s: ", __func__);

	ocm_core_open();
	if (q6core_lcl.core_handle_q == NULL) {
		pr_info("%s: apr registration for CORE failed\n", __func__);
		return -ENODEV;
	}

	cptr = kzalloc(
		sizeof(struct avcs_cmd_rsp_get_low_power_segments_info_t),
		GFP_KERNEL);
	if (!cptr) {
		pr_err("%s: Failed to allocate memory for low power segment struct\n",
				__func__);
		return -ENOMEM;
	}
	q6core_lcl.lp_ocm_payload =
		(struct avcs_cmd_rsp_get_low_power_segments_info_t *) cptr;

	lp_ocm_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	lp_ocm_cmd.hdr.pkt_size =
		sizeof(struct avcs_cmd_get_low_power_segments_info);

	lp_ocm_cmd.hdr.src_port = 0;
	lp_ocm_cmd.hdr.dest_port = 0;
	lp_ocm_cmd.hdr.token = 0;
	lp_ocm_cmd.hdr.opcode = AVCS_CMD_GET_LOW_POWER_SEGMENTS_INFO;


	ret = apr_send_pkt(q6core_lcl.core_handle_q, (uint32_t *) &lp_ocm_cmd);
	if (ret < 0) {
		pr_err("%s: CORE low power segment request failed\n", __func__);
		goto fail_cmd;
	}

	ret = wait_event_timeout(q6core_lcl.bus_bw_req_wait,
				(q6core_lcl.bus_bw_resp_received == 1),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout for GET_LOW_POWER_SEGMENTS\n",
				__func__);
		ret = -ETIME;
		goto fail_cmd;
	}

	*lp_memseg = q6core_lcl.lp_ocm_payload;
	return 0;

fail_cmd:
	return ret;
}


static int __init core_init(void)
{
	init_waitqueue_head(&q6core_lcl.bus_bw_req_wait);
	q6core_lcl.bus_bw_resp_received = 0;

	q6core_lcl.core_handle_q = NULL;
	q6core_lcl.lp_ocm_payload = kzalloc(
	sizeof(struct avcs_cmd_rsp_get_low_power_segments_info_t), GFP_KERNEL);

	if (!q6core_lcl.lp_ocm_payload) {
		pr_err("%s: Failed to allocate memory for low power segment struct\n",
				__func__);
		return -ENOMEM;
	}

	return 0;
}
module_init(core_init);

static void __exit core_exit(void)
{
	kfree(q6core_lcl.lp_ocm_payload);
}
module_exit(core_exit);
MODULE_DESCRIPTION("ADSP core driver");
MODULE_LICENSE("GPL v2");

