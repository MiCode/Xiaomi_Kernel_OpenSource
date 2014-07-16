/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/qdsp6v2/apr.h>
#include <soc/qcom/smd.h>
#include <soc/qcom/ocmem.h>
#include <sound/q6core.h>

#define TIMEOUT_MS 1000

#define MAX_META_INFO_SIZE 1024

struct meta_info_t {
	uint32_t nKeyValue;
	uint32_t nBufferLength;
	uint8_t *nBuffer;
};

/*
 * AVS bring up in the modem is optimitized for the new
 * Sub System Restart design and 100 milliseconds timeout
 * is sufficient to make sure the Q6 will be ready.
 */
#define Q6_READY_TIMEOUT_MS 100

struct q6core_str {
	struct apr_svc *core_handle_q;
	wait_queue_head_t bus_bw_req_wait;
	wait_queue_head_t cmd_req_wait;
	u32 bus_bw_resp_received;
	enum cmd_flags {
		FLAG_NONE,
		FLAG_CMDRSP_LICENSE_RESULT
	} cmd_resp_received_flag;
	struct mutex cmd_lock;
	union {
		struct avcs_cmdrsp_get_license_validation_result
						cmdrsp_license_result;
	} cmd_resp_payload;
	struct avcs_cmd_rsp_get_low_power_segments_info_t lp_ocm_payload;
	u32 param;
};

static struct q6core_str q6core_lcl;

static int32_t aprv2_core_fn_q(struct apr_client_data *data, void *priv)
{
	uint32_t *payload1;
	uint32_t nseg;
	int i, j;

	if (data == NULL) {
		pr_err("%s: data argument is null\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: core msg: payload len = %u, apr resp opcode = 0x%x\n",
		__func__,
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
			pr_err("%s: Invalid cmd rsp[0x%x][0x%x] opcode %d\n",
					__func__,
					payload1[0], payload1[1], data->opcode);
			break;
		}
		break;
	}

	case AVCS_CMDRSP_GET_LOW_POWER_SEGMENTS_INFO:
		payload1 = data->payload;
		pr_info("%s: cmd = AVCS_CMDRSP_GET_LOW_POWER_SEGMENTS_INFO num_segments = 0x%x\n",
					__func__, payload1[0]);
		nseg = payload1[0];
		q6core_lcl.lp_ocm_payload.num_segments = nseg;
		q6core_lcl.lp_ocm_payload.bandwidth = payload1[1];
		for (i = 0, j = 2; i < nseg; i++) {
			q6core_lcl.lp_ocm_payload.mem_segment[i].type =
					(payload1[j] & 0xffff);
			q6core_lcl.lp_ocm_payload.mem_segment[i].category =
					((payload1[j++] >> 16) & 0xffff);
			q6core_lcl.lp_ocm_payload.mem_segment[i].size =
					payload1[j++];
			q6core_lcl.lp_ocm_payload.
				mem_segment[i].start_address_lsw =
				payload1[j++];
			q6core_lcl.lp_ocm_payload.
				mem_segment[i].start_address_msw =
				payload1[j++];
		}

		q6core_lcl.bus_bw_resp_received = 1;
		wake_up(&q6core_lcl.bus_bw_req_wait);
		break;

	case RESET_EVENTS:{
		pr_debug("%s: Reset event received in Core service\n",
			__func__);
		apr_reset(q6core_lcl.core_handle_q);
		q6core_lcl.core_handle_q = NULL;
		break;
	}

	case AVCS_CMDRSP_ADSP_EVENT_GET_STATE:
		payload1 = data->payload;
		q6core_lcl.param = payload1[0];
		pr_debug("%s: Received ADSP get state response 0x%x\n",
			 __func__, q6core_lcl.param);
		/* ensure .param is updated prior to .bus_bw_resp_received */
		wmb();
		q6core_lcl.bus_bw_resp_received = 1;
		wake_up(&q6core_lcl.bus_bw_req_wait);
		break;
	case AVCS_CMDRSP_GET_LICENSE_VALIDATION_RESULT:
		payload1 = data->payload;
		pr_debug("%s: cmd = LICENSE_VALIDATION_RESULT, result = 0x%x\n",
				__func__, payload1[0]);
		q6core_lcl.cmd_resp_payload.cmdrsp_license_result.result
								= payload1[0];
		q6core_lcl.cmd_resp_received_flag = FLAG_CMDRSP_LICENSE_RESULT;
		wake_up(&q6core_lcl.cmd_req_wait);
		break;
	default:
		pr_err("%s: Message id from adsp core svc: 0x%x\n",
			__func__, data->opcode);
		break;
	}

	return 0;
}


void ocm_core_open(void)
{
	if (q6core_lcl.core_handle_q == NULL)
		q6core_lcl.core_handle_q = apr_register("ADSP", "CORE",
					aprv2_core_fn_q, 0xFFFFFFFF, NULL);
	pr_debug("%s: Open_q %p\n", __func__, q6core_lcl.core_handle_q);
	if (q6core_lcl.core_handle_q == NULL)
		pr_err("%s: Unable to register CORE\n", __func__);
}

int32_t core_set_license(uint32_t key, uint32_t module_id)
{
	struct avcs_cmd_set_license *cmd_setl = NULL;
	struct meta_info_t metainfo;
	int rc = 0, paycket_size = 0;

	pr_debug("%s: key:0x%x, id:0x%x\n", __func__, key, module_id);

	mutex_lock(&(q6core_lcl.cmd_lock));

	metainfo.nKeyValue = key;
	metainfo.nBuffer = NULL;
	metainfo.nBufferLength = 0;
	rc = 0;
	if (rc != 0 || metainfo.nBufferLength <= 0 ||
		metainfo.nBufferLength > MAX_META_INFO_SIZE) {
		pr_err("%s: error getting metainfo size, err:0x%x, size:%d\n",
					__func__, rc, metainfo.nBufferLength);
		goto fail_cmd1;
	}

	metainfo.nBuffer = kzalloc(metainfo.nBufferLength, GFP_KERNEL);
	if (metainfo.nBuffer == NULL) {
		pr_err("%s: kzalloc for metainfo failed\n", __func__);
		rc  = -ENOMEM;
		goto fail_cmd1;
	}
	rc = 1;
	if (rc) {
		pr_err("%s: error getting metainfo err:%d\n", __func__, rc);
		goto fail_cmd2;
	}

	paycket_size = sizeof(struct avcs_cmd_set_license) +
						metainfo.nBufferLength;
	/*round up total paycket_size to next 4 byte boundary*/
	paycket_size = ((paycket_size + 0x3)>>2)<<2;

	cmd_setl = kzalloc(paycket_size, GFP_KERNEL);
	if (cmd_setl == NULL) {
		pr_err("%s: kzalloc for cmd_set_license failed\n", __func__);
		rc  = -ENOMEM;
		goto fail_cmd2;
	}

	ocm_core_open();
	if (q6core_lcl.core_handle_q == NULL) {
		pr_err("%s: apr registration for CORE failed\n", __func__);
		rc  = -ENODEV;
		goto fail_cmd;
	}

	cmd_setl->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_EVENT,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cmd_setl->hdr.pkt_size = paycket_size;
	cmd_setl->hdr.src_port = 0;
	cmd_setl->hdr.dest_port = 0;
	cmd_setl->hdr.token = 0;
	cmd_setl->hdr.opcode = AVCS_CMD_SET_LICENSE;
	cmd_setl->id = module_id;
	cmd_setl->overwrite = 1;
	cmd_setl->size = metainfo.nBufferLength;
	memcpy((uint8_t *)cmd_setl + sizeof(struct avcs_cmd_set_license),
				metainfo.nBuffer, metainfo.nBufferLength);
	pr_info("%s: Set license opcode=0x%x ,key=0x%x, id =0x%x, size = %d\n",
			__func__, cmd_setl->hdr.opcode,
			metainfo.nKeyValue, cmd_setl->id, cmd_setl->size);
	rc = apr_send_pkt(q6core_lcl.core_handle_q, (uint32_t *)cmd_setl);
	if (rc < 0)
		pr_err("%s: SET_LICENSE failed op[0x%x]rc[%d]\n",
					__func__, cmd_setl->hdr.opcode, rc);

fail_cmd:
	kfree(cmd_setl);
fail_cmd2:
	kfree(metainfo.nBuffer);
fail_cmd1:
	mutex_unlock(&(q6core_lcl.cmd_lock));

	return rc;
}

int32_t core_get_license_status(uint32_t module_id)
{
	struct avcs_cmd_get_license_validation_result get_lvr_cmd;
	int ret = 0;

	pr_info("%s: module_id 0x%x", __func__, module_id);

	mutex_lock(&(q6core_lcl.cmd_lock));
	ocm_core_open();
	if (q6core_lcl.core_handle_q == NULL) {
		pr_err("%s: apr registration for CORE failed\n", __func__);
		ret  = -ENODEV;
		goto fail_cmd;
	}

	get_lvr_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	get_lvr_cmd.hdr.pkt_size =
		sizeof(struct avcs_cmd_get_license_validation_result);

	get_lvr_cmd.hdr.src_port = 0;
	get_lvr_cmd.hdr.dest_port = 0;
	get_lvr_cmd.hdr.token = 0;
	get_lvr_cmd.hdr.opcode = AVCS_CMD_GET_LICENSE_VALIDATION_RESULT;
	get_lvr_cmd.id = module_id;


	ret = apr_send_pkt(q6core_lcl.core_handle_q, (uint32_t *) &get_lvr_cmd);
	if (ret < 0) {
		pr_err("%s: license_validation request failed, err %d\n",
							__func__, ret);
		ret = -EREMOTE;
		goto fail_cmd;
	}

	q6core_lcl.cmd_resp_received_flag &= ~(FLAG_CMDRSP_LICENSE_RESULT);
	mutex_unlock(&(q6core_lcl.cmd_lock));
	ret = wait_event_timeout(q6core_lcl.cmd_req_wait,
			(q6core_lcl.cmd_resp_received_flag ==
				FLAG_CMDRSP_LICENSE_RESULT),
				msecs_to_jiffies(TIMEOUT_MS));
	mutex_lock(&(q6core_lcl.cmd_lock));
	if (!ret) {
		pr_err("%s: wait_event timeout for CMDRSP_LICENSE_RESULT\n",
				__func__);
		ret = -ETIME;
		goto fail_cmd;
	}
	q6core_lcl.cmd_resp_received_flag &= ~(FLAG_CMDRSP_LICENSE_RESULT);
	ret = q6core_lcl.cmd_resp_payload.cmdrsp_license_result.result;

fail_cmd:
	mutex_unlock(&(q6core_lcl.cmd_lock));
	pr_info("%s: cmdrsp_license_result.result = 0x%x for module 0x%x\n",
				__func__, ret, module_id);
	return ret;
}

uint32_t core_set_dolby_manufacturer_id(int manufacturer_id)
{
	struct adsp_dolby_manufacturer_id payload;
	int rc = 0;
	pr_info("%s: manufacturer_id :%d\n", __func__, manufacturer_id);
	mutex_lock(&(q6core_lcl.cmd_lock));
	ocm_core_open();
	if (q6core_lcl.core_handle_q) {
		payload.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_EVENT,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		payload.hdr.pkt_size =
			sizeof(struct adsp_dolby_manufacturer_id);
		payload.hdr.src_port = 0;
		payload.hdr.dest_port = 0;
		payload.hdr.token = 0;
		payload.hdr.opcode = ADSP_CMD_SET_DOLBY_MANUFACTURER_ID;
		payload.manufacturer_id = manufacturer_id;
		pr_debug("%s: Send Dolby security opcode=0x%x manufacturer ID = %d\n",
			__func__,
			payload.hdr.opcode, payload.manufacturer_id);
		rc = apr_send_pkt(q6core_lcl.core_handle_q,
						(uint32_t *)&payload);
		if (rc < 0)
			pr_err("%s: SET_DOLBY_MANUFACTURER_ID failed op[0x%x]rc[%d]\n",
				__func__, payload.hdr.opcode, rc);
	}
	mutex_unlock(&(q6core_lcl.cmd_lock));
	return rc;
}

int core_get_low_power_segments(
		struct avcs_cmd_rsp_get_low_power_segments_info_t **lp_memseg)
{
	struct avcs_cmd_get_low_power_segments_info lp_ocm_cmd;
	int ret = 0;

	pr_debug("%s:", __func__);

	ocm_core_open();
	if (q6core_lcl.core_handle_q == NULL) {
		pr_info("%s: apr registration for CORE failed\n", __func__);
		return -ENODEV;
	}


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
		pr_err("%s: CORE low power segment request failed %d\n",
			__func__, ret);
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

	*lp_memseg = &q6core_lcl.lp_ocm_payload;
	return 0;

fail_cmd:
	return ret;
}

bool q6core_is_adsp_ready(void)
{
	int rc;
	bool ret = false;
	struct apr_hdr hdr;

	pr_debug("%s: enter\n", __func__);
	memset(&hdr, 0, sizeof(hdr));
	hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				      APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE, 0);
	hdr.opcode = AVCS_CMD_ADSP_EVENT_GET_STATE;

	ocm_core_open();
	q6core_lcl.bus_bw_resp_received = 0;
	rc = apr_send_pkt(q6core_lcl.core_handle_q, (uint32_t *)&hdr);
	if (rc < 0) {
		pr_err("%s: Get ADSP state APR packet send event %d\n",
			__func__, rc);
		goto bail;
	}

	rc = wait_event_timeout(q6core_lcl.bus_bw_req_wait,
				(q6core_lcl.bus_bw_resp_received == 1),
				msecs_to_jiffies(Q6_READY_TIMEOUT_MS));
	if (rc > 0 && q6core_lcl.bus_bw_resp_received) {
		/* ensure to read updated param by callback thread */
		rmb();
		ret = !!q6core_lcl.param;
	}
bail:
	pr_debug("%s: leave, rc %d, adsp ready %d\n", __func__, rc, ret);
	return ret;
}

static int __init core_init(void)
{
	init_waitqueue_head(&q6core_lcl.bus_bw_req_wait);
	q6core_lcl.bus_bw_resp_received = 0;

	q6core_lcl.core_handle_q = NULL;

	init_waitqueue_head(&q6core_lcl.cmd_req_wait);
	q6core_lcl.cmd_resp_received_flag = FLAG_NONE;
	mutex_init(&q6core_lcl.cmd_lock);

	return 0;
}
module_init(core_init);

static void __exit core_exit(void)
{
	mutex_destroy(&q6core_lcl.cmd_lock);
}
module_exit(core_exit);
MODULE_DESCRIPTION("ADSP core driver");
MODULE_LICENSE("GPL v2");

