// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/hash.h>
#include <linux/soc/qcom/smem.h>
#include <soc/qcom/socinfo.h>
#include "cvp_hfi_helper.h"
#include "cvp_hfi_io.h"
#include "msm_cvp_debug.h"
#include "cvp_hfi.h"
#include "msm_cvp_common.h"

extern struct msm_cvp_drv *cvp_driver;

static int _deprecated_hfi_msg_process(u32 device_id,
	struct cvp_hfi_msg_session_hdr *pkt,
	struct msm_cvp_cb_info *info,
	struct msm_cvp_inst *inst);

static enum cvp_status hfi_map_err_status(u32 hfi_err)
{
	enum cvp_status cvp_err;

	switch (hfi_err) {
	case HFI_ERR_NONE:
		cvp_err = CVP_ERR_NONE;
		break;
	case HFI_ERR_SYS_FATAL:
		cvp_err = CVP_ERR_HW_FATAL;
		break;
	case HFI_ERR_SYS_NOC_ERROR:
		cvp_err = CVP_ERR_NOC_ERROR;
		break;
	case HFI_ERR_SYS_VERSION_MISMATCH:
	case HFI_ERR_SYS_INVALID_PARAMETER:
	case HFI_ERR_SYS_SESSION_ID_OUT_OF_RANGE:
	case HFI_ERR_SESSION_INVALID_PARAMETER:
	case HFI_ERR_SESSION_INVALID_SESSION_ID:
	case HFI_ERR_SESSION_INVALID_STREAM_ID:
		cvp_err = CVP_ERR_BAD_PARAM;
		break;
	case HFI_ERR_SYS_INSUFFICIENT_RESOURCES:
	case HFI_ERR_SYS_UNSUPPORTED_DOMAIN:
	case HFI_ERR_SYS_UNSUPPORTED_CODEC:
	case HFI_ERR_SESSION_UNSUPPORTED_PROPERTY:
	case HFI_ERR_SESSION_UNSUPPORTED_SETTING:
	case HFI_ERR_SESSION_INSUFFICIENT_RESOURCES:
	case HFI_ERR_SESSION_UNSUPPORTED_STREAM:
		cvp_err = CVP_ERR_NOT_SUPPORTED;
		break;
	case HFI_ERR_SYS_MAX_SESSIONS_REACHED:
		cvp_err = CVP_ERR_MAX_CLIENTS;
		break;
	case HFI_ERR_SYS_SESSION_IN_USE:
		cvp_err = CVP_ERR_CLIENT_PRESENT;
		break;
	case HFI_ERR_SESSION_FATAL:
		cvp_err = CVP_ERR_CLIENT_FATAL;
		break;
	case HFI_ERR_SESSION_BAD_POINTER:
		cvp_err = CVP_ERR_BAD_PARAM;
		break;
	case HFI_ERR_SESSION_INCORRECT_STATE_OPERATION:
		cvp_err = CVP_ERR_BAD_STATE;
		break;
	default:
		cvp_err = CVP_ERR_FAIL;
		break;
	}
	return cvp_err;
}

static int hfi_process_sys_error(u32 device_id,
	struct cvp_hfi_msg_event_notify_packet *pkt,
	struct msm_cvp_cb_info *info)
{
	struct msm_cvp_cb_cmd_done cmd_done = {0};

	cmd_done.device_id = device_id;
	cmd_done.status = hfi_map_err_status(pkt->event_data1);

	info->response_type = HAL_SYS_ERROR;
	info->response.cmd = cmd_done;

	return 0;
}

static int hfi_process_session_error(u32 device_id,
		struct cvp_hfi_msg_event_notify_packet *pkt,
		struct msm_cvp_cb_info *info)
{
	struct msm_cvp_cb_cmd_done cmd_done = {0};

	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
	cmd_done.status = hfi_map_err_status(pkt->event_data1);
	info->response.cmd = cmd_done;
	dprintk(CVP_INFO, "Received: SESSION_ERROR with event id : %#x %#x\n",
		pkt->event_data1, pkt->event_data2);
	switch (pkt->event_data1) {
	/* Ignore below errors */
	case HFI_ERR_SESSION_INVALID_SCALE_FACTOR:
	case HFI_ERR_SESSION_UPSCALE_NOT_SUPPORTED:
		dprintk(CVP_INFO, "Non Fatal: HFI_EVENT_SESSION_ERROR\n");
		info->response_type = HAL_RESPONSE_UNUSED;
		break;
	default:
		dprintk(CVP_ERR,
			"%s: session %x data1 %#x, data2 %#x\n", __func__,
			pkt->session_id, pkt->event_data1, pkt->event_data2);
		info->response_type = HAL_RESPONSE_UNUSED;
		break;
	}

	return 0;
}

static int hfi_process_event_notify(u32 device_id,
		struct cvp_hfi_msg_event_notify_packet *pkt,
		struct msm_cvp_cb_info *info)
{
	dprintk(CVP_DBG, "Received: EVENT_NOTIFY\n");

	if (pkt->size < sizeof(struct cvp_hfi_msg_event_notify_packet)) {
		dprintk(CVP_ERR, "Invalid Params\n");
		return -E2BIG;
	}

	switch (pkt->event_id) {
	case HFI_EVENT_SYS_ERROR:
		dprintk(CVP_ERR, "HFI_EVENT_SYS_ERROR: %d, %#x\n",
			pkt->event_data1, pkt->event_data2);
		return hfi_process_sys_error(device_id, pkt, info);

	case HFI_EVENT_SESSION_ERROR:
		dprintk(CVP_INFO, "HFI_EVENT_SESSION_ERROR[%#x]\n",
				pkt->session_id);
		return hfi_process_session_error(device_id, pkt, info);

	default:
		*info = (struct msm_cvp_cb_info) {
			.response_type =  HAL_RESPONSE_UNUSED,
		};

		return 0;
	}
}

static int hfi_process_sys_init_done(u32 device_id,
		struct cvp_hfi_msg_sys_init_done_packet *pkt,
		struct msm_cvp_cb_info *info)
{
	struct msm_cvp_cb_cmd_done cmd_done = {0};
	enum cvp_status status = CVP_ERR_NONE;

	dprintk(CVP_DBG, "RECEIVED: SYS_INIT_DONE\n");
	if (sizeof(struct cvp_hfi_msg_sys_init_done_packet) > pkt->size) {
		dprintk(CVP_ERR, "%s: bad_pkt_size: %d\n", __func__,
				pkt->size);
		return -E2BIG;
	}
	if (!pkt->num_properties) {
		dprintk(CVP_DBG,
				"hal_process_sys_init_done: no_properties\n");
		goto err_no_prop;
	}

	status = hfi_map_err_status(pkt->error_type);
	if (status) {
		dprintk(CVP_ERR, "%s: status %#x\n",
			__func__, status);
		goto err_no_prop;
	}

err_no_prop:
	cmd_done.device_id = device_id;
	cmd_done.session_id = NULL;
	cmd_done.status = (u32)status;
	cmd_done.size = sizeof(struct cvp_hal_sys_init_done);

	info->response_type = HAL_SYS_INIT_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

enum cvp_status cvp_hfi_process_sys_init_done_prop_read(
	struct cvp_hfi_msg_sys_init_done_packet *pkt,
	struct cvp_hal_sys_init_done *sys_init_done)
{
	enum cvp_status status = CVP_ERR_NONE;
	u32 rem_bytes, num_properties;
	u8 *data_ptr;

	if (!pkt || !sys_init_done) {
		dprintk(CVP_ERR,
			"hfi_msg_sys_init_done: Invalid input\n");
		return CVP_ERR_FAIL;
	}

	rem_bytes = pkt->size - sizeof(struct
			cvp_hfi_msg_sys_init_done_packet) + sizeof(u32);

	if (!rem_bytes) {
		dprintk(CVP_ERR,
			"hfi_msg_sys_init_done: missing_prop_info\n");
		return CVP_ERR_FAIL;
	}

	status = hfi_map_err_status(pkt->error_type);
	if (status) {
		dprintk(CVP_ERR, "%s: status %#x\n", __func__, status);
		return status;
	}

	data_ptr = (u8 *) &pkt->rg_property_data[0];
	num_properties = pkt->num_properties;
	dprintk(CVP_DBG,
		"%s: data_start %pK, num_properties %#x\n",
		__func__, data_ptr, num_properties);

	sys_init_done->capabilities = NULL;
	return status;
}

static int hfi_process_session_init_done(u32 device_id,
		struct cvp_hfi_msg_sys_session_init_done_packet *pkt,
		struct msm_cvp_cb_info *info)
{
	struct msm_cvp_cb_cmd_done cmd_done = {0};
	struct cvp_hal_session_init_done session_init_done = { {0} };

	dprintk(CVP_DBG, "RECEIVED: SESSION_INIT_DONE[%x]\n", pkt->session_id);

	if (sizeof(struct cvp_hfi_msg_sys_session_init_done_packet)
			> pkt->size) {
		dprintk(CVP_ERR,
			"hal_process_session_init_done: bad_pkt_size\n");
		return -E2BIG;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.data.session_init_done = session_init_done;
	cmd_done.size = sizeof(struct cvp_hal_session_init_done);

	info->response_type = HAL_SESSION_INIT_DONE;
	info->response.cmd = cmd_done;

	return 0;
}
static int hfi_process_session_end_done(u32 device_id,
		struct cvp_hfi_msg_sys_session_end_done_packet *pkt,
		struct msm_cvp_cb_info *info)
{
	struct msm_cvp_cb_cmd_done cmd_done = {0};

	dprintk(CVP_DBG, "RECEIVED: SESSION_END_DONE[%#x]\n", pkt->session_id);

	if (!pkt || pkt->size !=
		sizeof(struct cvp_hfi_msg_sys_session_end_done_packet)) {
		dprintk(CVP_ERR, "%s: bad packet/packet size\n", __func__);
		return -E2BIG;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.size = 0;

	info->response_type = HAL_SESSION_END_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

static int hfi_process_session_abort_done(u32 device_id,
	struct cvp_hfi_msg_sys_session_abort_done_packet *pkt,
	struct msm_cvp_cb_info *info)
{
	struct msm_cvp_cb_cmd_done cmd_done = {0};

	dprintk(CVP_DBG, "RECEIVED: SESSION_ABORT_DONE[%#x]\n",
			pkt->session_id);

	if (!pkt || pkt->size !=
		sizeof(struct cvp_hfi_msg_sys_session_abort_done_packet)) {
		dprintk(CVP_ERR, "%s: bad packet/packet size: %d\n",
				__func__, pkt ? pkt->size : 0);
		return -E2BIG;
	}
	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)pkt->session_id;
	cmd_done.status = hfi_map_err_status(pkt->error_type);
	cmd_done.size = 0;

	info->response_type = HAL_SESSION_ABORT_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

static int hfi_process_session_set_buf_done(u32 device_id,
		struct cvp_hfi_msg_session_hdr *pkt,
		struct msm_cvp_cb_info *info)
{
	struct msm_cvp_cb_cmd_done cmd_done = {0};
	unsigned int pkt_size = get_msg_size();

	if (!pkt || pkt->size < pkt_size) {
		dprintk(CVP_ERR, "bad packet/packet size %d\n",
				pkt ? pkt->size : 0);
		return -E2BIG;
	}
	dprintk(CVP_DBG, "RECEIVED:CVP_SET_BUFFER_DONE[%#x]\n",
			pkt->session_id);

	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)get_msg_session_id(pkt);
	cmd_done.status = hfi_map_err_status(get_msg_errorcode(pkt));
	cmd_done.size = 0;

	info->response_type = HAL_SESSION_SET_BUFFER_DONE;
	info->response.cmd = cmd_done;

	return 0;
}


static int hfi_process_session_rel_buf_done(u32 device_id,
		struct cvp_hfi_msg_session_hdr *pkt,
		struct msm_cvp_cb_info *info)
{
	struct msm_cvp_cb_cmd_done cmd_done = {0};
	unsigned int pkt_size = get_msg_size();

	if (!pkt || pkt->size < pkt_size) {
		dprintk(CVP_ERR, "bad packet/packet size %d\n",
				pkt ? pkt->size : 0);
		return -E2BIG;
	}
	dprintk(CVP_DBG, "RECEIVED:CVP_RELEASE_BUFFER_DONE[%#x]\n",
			pkt->session_id);

	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)get_msg_session_id(pkt);
	cmd_done.status = hfi_map_err_status(get_msg_errorcode(pkt));
	cmd_done.size = 0;

	info->response_type = HAL_SESSION_RELEASE_BUFFER_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

static int hfi_process_session_cvp_operation_config(u32 device_id,
	struct cvp_hfi_msg_session_op_cfg_packet *pkt,
	struct msm_cvp_cb_info *info)
{
	struct msm_cvp_cb_cmd_done cmd_done = {0};
	int signal;
	unsigned int conf_id, session_id, error_type;

	if (!pkt) {
		dprintk(CVP_ERR, "%s: invalid param\n", __func__);
		return -EINVAL;
	} else if (pkt->size < get_msg_size()) {
		dprintk(CVP_ERR,
				"%s: bad_pkt_size\n", __func__);
		return -E2BIG;
	}

	get_msg_opconfigs(pkt, &session_id, &error_type, &conf_id);
	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)session_id;
	cmd_done.status = hfi_map_err_status(error_type);
	cmd_done.size = 0;

	dprintk(CVP_DBG,
		"%s: device_id=%d status=%d, sessionid=%pK config=%x\n",
		__func__, device_id, cmd_done.status,
		cmd_done.session_id, pkt->op_conf_id);

	if (pkt->packet_type == HFI_MSG_SESSION_CVP_SET_PERSIST_BUFFERS)
		signal = get_signal_from_pkt_type(
				HFI_CMD_SESSION_CVP_SET_PERSIST_BUFFERS);
	else if (pkt->packet_type == HFI_MSG_SESSION_CVP_SET_MODEL_BUFFERS)
		signal = get_signal_from_pkt_type(
				HFI_CMD_SESSION_CVP_SET_MODEL_BUFFERS);
	else
		signal = get_signal_from_pkt_type(conf_id);

	if (signal < 0) {
		dprintk(CVP_ERR, "%s Invalid op config id\n", __func__);
		return -EINVAL;
	}

	info->response_type = signal;
	info->response.cmd = cmd_done;
	return 0;
}

static struct msm_cvp_inst *cvp_get_inst_from_id(struct msm_cvp_core *core,
	unsigned int session_id)
{
	struct msm_cvp_inst *inst = NULL;
	bool match = false;
	int count = 0;

	if (!core || !session_id)
		return NULL;

retry:
	if (mutex_trylock(&core->lock)) {
		list_for_each_entry(inst, &core->instances, list) {
			if (hash32_ptr(inst->session) == session_id) {
				match = true;
				break;
			}
		}

		inst = match ? inst : NULL;
		mutex_unlock(&core->lock);
	} else {
		if (core->state == CVP_CORE_UNINIT)
			return NULL;
		usleep_range(100, 200);
		count++;
		if (count < 1000)
			goto retry;
		else
			dprintk(CVP_ERR, "timeout locking core mutex\n");
	}

	return inst;

}

static int __dme_output_cache_operation(struct cvp_hfi_msg_session_hdr *pkt)
{
	struct cvp_hfi_msg_dme_pkt *dme_pkt;
	int rc;

	if (!pkt) {
		dprintk(CVP_ERR, "%s: invalid param\n", __func__);
		return -EINVAL;
	} else if (pkt->size < get_msg_size()) {
		dprintk(CVP_ERR, "%s: bad_pkt_size %d\n", __func__, pkt->size);
		return -E2BIG;
	}

	dme_pkt = (struct cvp_hfi_msg_dme_pkt *)pkt;
	rc = dma_buf_begin_cpu_access_partial(dme_pkt->statsbuffer.dbuf,
						DMA_TO_DEVICE, 0,
						dme_pkt->statsbuffer.size);
	if (rc) {
		dprintk(CVP_ERR, "%s: begin_cpu_access failed\n", __func__);
		return rc;
	}
	rc = dma_buf_end_cpu_access_partial(dme_pkt->statsbuffer.dbuf,
						DMA_FROM_DEVICE, 0,
						dme_pkt->statsbuffer.size);
	if (rc)
		dprintk(CVP_ERR, "%s: end_cpu_access failed\n", __func__);

	return rc;
}

static int hfi_process_session_cvp_msg(u32 device_id,
	struct cvp_hfi_msg_session_hdr *pkt,
	struct msm_cvp_cb_info *info)
{
	struct cvp_session_msg *sess_msg;
	struct msm_cvp_inst *inst = NULL;
	struct msm_cvp_core *core;
	void *session_id;

	if (!pkt) {
		dprintk(CVP_ERR, "%s: invalid param\n", __func__);
		return -EINVAL;
	} else if (pkt->size > MAX_HFI_PKT_SIZE * sizeof(unsigned int)) {
		dprintk(CVP_ERR, "%s: bad_pkt_size %d\n", __func__, pkt->size);
		return -E2BIG;
	}
	session_id = (void *)(uintptr_t)get_msg_session_id(pkt);
	core = list_first_entry(&cvp_driver->cores, struct msm_cvp_core, list);
	inst = cvp_get_inst_from_id(core, (unsigned int)session_id);

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid session\n", __func__);
		return -EINVAL;
	}

	if (inst->deprecate_bitmask) {
		if (pkt->packet_type == HFI_MSG_SESSION_CVP_DME
			|| pkt->packet_type == HFI_MSG_SESSION_CVP_ICA
			|| pkt->packet_type == HFI_MSG_SESSION_CVP_FD) {
			u64 ktid;
			u32 kdata1, kdata2;

			kdata1 = pkt->client_data.kdata1;
			kdata2 = pkt->client_data.kdata2;
			ktid = ((u64)kdata2 << 32) | kdata1;


			if (pkt->packet_type == HFI_MSG_SESSION_CVP_DME)
				__dme_output_cache_operation(pkt);

			msm_cvp_unmap_buf_cpu(inst, ktid);

			return _deprecated_hfi_msg_process(device_id,
				pkt, info, inst);
		}
		dprintk(CVP_ERR, "Invalid deprecate_bitmask %#x\n",
					inst->deprecate_bitmask);
	}

	sess_msg = kmem_cache_alloc(cvp_driver->msg_cache, GFP_KERNEL);
	if (sess_msg == NULL) {
		dprintk(CVP_ERR, "%s runs out msg cache memory\n", __func__);
		return -ENOMEM;
	}

	memcpy(&sess_msg->pkt, pkt, get_msg_size());

	dprintk(CVP_DBG,
		"%s: Received msg %x cmd_done.status=%d sessionid=%x\n",
		__func__, pkt->packet_type,
		hfi_map_err_status(get_msg_errorcode(pkt)), session_id);

	spin_lock(&inst->session_queue.lock);
	if (inst->session_queue.msg_count >= MAX_NUM_MSGS_PER_SESSION) {
		dprintk(CVP_ERR, "Reached session queue size limit\n");
		goto error_handle_msg;
	}
	list_add_tail(&sess_msg->node, &inst->session_queue.msgs);
	inst->session_queue.msg_count++;
	spin_unlock(&inst->session_queue.lock);

	wake_up_all(&inst->session_queue.wq);

	info->response_type = HAL_NO_RESP;

	return 0;

error_handle_msg:
	spin_unlock(&inst->session_queue.lock);
	kmem_cache_free(cvp_driver->msg_cache, sess_msg);
	return -ENOMEM;
}

static int hfi_process_session_cvp_dme(u32 device_id,
	struct cvp_hfi_msg_session_hdr *pkt,
	struct msm_cvp_cb_info *info)
{
	struct msm_cvp_cb_cmd_done cmd_done = {0};

	if (!pkt) {
		dprintk(CVP_ERR, "%s: invalid param\n", __func__);
		return -EINVAL;
	} else if (pkt->size < get_msg_size()) {
		dprintk(CVP_ERR, "%s: bad_pkt_size %d\n", __func__, pkt->size);
		return -E2BIG;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)get_msg_session_id(pkt);
	cmd_done.status = hfi_map_err_status(get_msg_errorcode(pkt));
	cmd_done.size = 0;

	dprintk(CVP_DBG,
		"%s: device_id=%d cmd_done.status=%d sessionid=%#x\n",
		__func__, device_id, cmd_done.status, cmd_done.session_id);
	info->response_type = HAL_SESSION_DME_FRAME_CMD_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

static int hfi_process_session_cvp_ica(u32 device_id,
	struct cvp_hfi_msg_session_hdr *pkt,
	struct msm_cvp_cb_info *info)
{
	struct msm_cvp_cb_cmd_done cmd_done = {0};

	if (!pkt) {
		dprintk(CVP_ERR, "%s: invalid param\n", __func__);
		return -EINVAL;
	} else if (pkt->size < get_msg_size()) {
		dprintk(CVP_ERR, "%s: bad_pkt_size %d\n", __func__, pkt->size);
		return -E2BIG;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)get_msg_session_id(pkt);
	cmd_done.status = hfi_map_err_status(get_msg_errorcode(pkt));
	cmd_done.size = 0;

	dprintk(CVP_DBG,
		"%s: device_id=%d cmd_done.status=%d sessionid=%#x\n",
		__func__, device_id, cmd_done.status, cmd_done.session_id);
	info->response_type = HAL_SESSION_ICA_FRAME_CMD_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

static int hfi_process_session_cvp_fd(u32 device_id,
	struct cvp_hfi_msg_session_hdr *pkt,
	struct msm_cvp_cb_info *info)
{
	struct msm_cvp_cb_cmd_done cmd_done = {0};

	if (!pkt) {
		dprintk(CVP_ERR, "%s: invalid param\n", __func__);
		return -EINVAL;
	} else if (pkt->size < get_msg_size()) {
		dprintk(CVP_ERR, "%s: bad_pkt_size %d\n", __func__, pkt->size);
		return -E2BIG;
	}

	cmd_done.device_id = device_id;
	cmd_done.session_id = (void *)(uintptr_t)get_msg_session_id(pkt);
	cmd_done.status = hfi_map_err_status(get_msg_errorcode(pkt));
	cmd_done.size = 0;

	dprintk(CVP_DBG,
		"%s: device_id=%d cmd_done.status=%d sessionid=%#x\n",
		__func__, device_id, cmd_done.status, cmd_done.session_id);
	info->response_type = HAL_SESSION_FD_FRAME_CMD_DONE;
	info->response.cmd = cmd_done;

	return 0;
}

static int _deprecated_hfi_msg_process(u32 device_id,
	struct cvp_hfi_msg_session_hdr *pkt,
	struct msm_cvp_cb_info *info,
	struct msm_cvp_inst *inst)
{
	if (pkt->packet_type == HFI_MSG_SESSION_CVP_DME)
		if (test_bit(DME_BIT_OFFSET,
				&inst->deprecate_bitmask))
			return hfi_process_session_cvp_dme(
					device_id, (void *)pkt, info);

	if (pkt->packet_type == HFI_MSG_SESSION_CVP_ICA)
		if (test_bit(ICA_BIT_OFFSET,
				&inst->deprecate_bitmask))
			return hfi_process_session_cvp_ica(
				device_id, (void *)pkt, info);

	if (pkt->packet_type == HFI_MSG_SESSION_CVP_FD)
		if (test_bit(FD_BIT_OFFSET,
				&inst->deprecate_bitmask))
			return hfi_process_session_cvp_fd(
				device_id, (void *)pkt, info);

	dprintk(CVP_ERR, "Deprecatd MSG doesn't match bitmask %x %lx\n",
			pkt->packet_type, inst->deprecate_bitmask);
	return -EINVAL;
}

static void hfi_process_sys_get_prop_image_version(
		struct cvp_hfi_msg_sys_property_info_packet *pkt)
{
	int i = 0;
	size_t smem_block_size = 0;
	u8 *smem_table_ptr;
	char version[256];
	const u32 version_string_size = 128;
	const u32 smem_image_index = 14 * 128;
	u8 *str_image_version;
	int req_bytes;

	req_bytes = pkt->size - sizeof(*pkt);
	if (req_bytes < version_string_size ||
			!pkt->rg_property_data[1] ||
			pkt->num_properties > 1) {
		dprintk(CVP_ERR, "%s: bad_pkt: %d\n", __func__, req_bytes);
		return;
	}
	str_image_version = (u8 *)&pkt->rg_property_data[1];
	/*
	 * The version string returned by firmware includes null
	 * characters at the start and in between. Replace the null
	 * characters with space, to print the version info.
	 */
	for (i = 0; i < version_string_size; i++) {
		if (str_image_version[i] != '\0')
			version[i] = str_image_version[i];
		else
			version[i] = ' ';
	}
	version[i] = '\0';
	dprintk(CVP_DBG, "F/W version: %s\n", version);

	smem_table_ptr = qcom_smem_get(QCOM_SMEM_HOST_ANY,
			SMEM_IMAGE_VERSION_TABLE, &smem_block_size);
	if ((smem_image_index + version_string_size) <= smem_block_size &&
			smem_table_ptr)
		memcpy(smem_table_ptr + smem_image_index,
				str_image_version, version_string_size);
}

static int hfi_process_sys_property_info(u32 device_id,
		struct cvp_hfi_msg_sys_property_info_packet *pkt,
		struct msm_cvp_cb_info *info)
{
	if (!pkt) {
		dprintk(CVP_ERR, "%s: invalid param\n", __func__);
		return -EINVAL;
	} else if (pkt->size > sizeof(*pkt)) {
		dprintk(CVP_ERR,
				"%s: bad_pkt_size %d\n", __func__, pkt->size);
		return -E2BIG;
	} else if (!pkt->num_properties) {
		dprintk(CVP_WARN,
				"%s: no_properties\n", __func__);
		return -EINVAL;
	}

	switch (pkt->rg_property_data[0]) {
	case HFI_PROPERTY_SYS_IMAGE_VERSION:
		hfi_process_sys_get_prop_image_version(pkt);

		*info = (struct msm_cvp_cb_info) {
			.response_type =  HAL_RESPONSE_UNUSED,
		};
		return 0;
	default:
		dprintk(CVP_DBG,
				"%s: unknown_prop_id: %x\n",
				__func__, pkt->rg_property_data[0]);
		return -ENOTSUPP;
	}

}

int cvp_hfi_process_msg_packet(u32 device_id,
		struct cvp_hal_msg_pkt_hdr *msg_hdr,
		struct msm_cvp_cb_info *info)
{
	typedef int (*pkt_func_def)(u32, void *, struct msm_cvp_cb_info *info);
	pkt_func_def pkt_func = NULL;

	if (!info || !msg_hdr || msg_hdr->size < CVP_IFACEQ_MIN_PKT_SIZE) {
		dprintk(CVP_ERR, "%s: bad packet/packet size\n",
			__func__);
		return -EINVAL;
	}

	dprintk(CVP_DBG, "Received HFI MSG with type %#x\n", msg_hdr->packet);
	switch (msg_hdr->packet) {
	case HFI_MSG_EVENT_NOTIFY:
		pkt_func = (pkt_func_def)hfi_process_event_notify;
		break;
	case  HFI_MSG_SYS_INIT_DONE:
		pkt_func = (pkt_func_def)hfi_process_sys_init_done;
		break;
	case HFI_MSG_SYS_SESSION_INIT_DONE:
		pkt_func = (pkt_func_def)hfi_process_session_init_done;
		break;
	case HFI_MSG_SYS_PROPERTY_INFO:
		pkt_func = (pkt_func_def)hfi_process_sys_property_info;
		break;
	case HFI_MSG_SYS_SESSION_END_DONE:
		pkt_func = (pkt_func_def)hfi_process_session_end_done;
		break;
	case HFI_MSG_SESSION_CVP_SET_BUFFERS:
		pkt_func = (pkt_func_def) hfi_process_session_set_buf_done;
		break;
	case HFI_MSG_SESSION_CVP_RELEASE_BUFFERS:
		pkt_func = (pkt_func_def)hfi_process_session_rel_buf_done;
		break;
	case HFI_MSG_SYS_SESSION_ABORT_DONE:
		pkt_func = (pkt_func_def)hfi_process_session_abort_done;
		break;
	case HFI_MSG_SESSION_CVP_OPERATION_CONFIG:
	case HFI_MSG_SESSION_CVP_SET_PERSIST_BUFFERS:
	case HFI_MSG_SESSION_CVP_SET_MODEL_BUFFERS:
		pkt_func =
			(pkt_func_def)hfi_process_session_cvp_operation_config;
		break;
	case HFI_MSG_SESSION_CVP_DS:
	case HFI_MSG_SESSION_CVP_DFS:
	case HFI_MSG_SESSION_CVP_DME:
	case HFI_MSG_SESSION_CVP_FD:
		pkt_func = (pkt_func_def)hfi_process_session_cvp_msg;
		break;
	default:
		dprintk(CVP_DBG, "Use default msg handler: %#x\n",
				msg_hdr->packet);
		pkt_func = (pkt_func_def)hfi_process_session_cvp_msg;
		break;
	}

	return pkt_func ?
		pkt_func(device_id, (void *)msg_hdr, info) : -ENOTSUPP;
}
