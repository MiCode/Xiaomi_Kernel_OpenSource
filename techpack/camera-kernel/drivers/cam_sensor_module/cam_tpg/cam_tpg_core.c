// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include "cam_tpg_core.h"
#include "cam_packet_util.h"
#include <cam_mem_mgr.h>
#include "tpg_hw/tpg_hw.h"

int cam_tpg_shutdown(struct cam_tpg_device *tpg_dev)
{
	if (tpg_dev != NULL) {
		CAM_INFO(CAM_TPG, "TPG[%d] shutdown cleanup.",
				tpg_dev->soc_info.index);
		tpg_hw_reset(&tpg_dev->tpg_hw);
		tpg_dev->state = CAM_TPG_STATE_INIT;
	}
	return 0;
}

int cam_tpg_publish_dev_info(
	struct cam_req_mgr_device_info *info)
{
	int rc = 0;
	struct cam_tpg_device *tpg_dev = NULL;

	if (!info)
		return -EINVAL;

	tpg_dev = (struct cam_tpg_device *)
		cam_get_device_priv(info->dev_hdl);

	if (!tpg_dev) {
		CAM_ERR(CAM_TPG, "Device data is NULL");
		return -EINVAL;
	}

	info->dev_id = CAM_REQ_MGR_DEVICE_TPG;
	strlcpy(info->name, CAM_TPG_NAME, sizeof(info->name));
	/* Hard code for now */
	info->p_delay = 2;
	info->trigger = CAM_TRIGGER_POINT_SOF;

	return rc;
}

int cam_tpg_setup_link(
	struct cam_req_mgr_core_dev_link_setup *link)
{
	struct cam_tpg_device *tpg_dev = NULL;

	if (!link)
		return -EINVAL;

	tpg_dev = (struct cam_tpg_device *)
		cam_get_device_priv(link->dev_hdl);
	if (!tpg_dev) {
		CAM_ERR(CAM_TPG, "Device data is NULL");
		return -EINVAL;
	}

	mutex_lock(&tpg_dev->mutex);
	if (link->link_enable) {
		tpg_dev->crm_intf.link_hdl = link->link_hdl;
		tpg_dev->crm_intf.crm_cb = link->crm_cb;
		CAM_DBG(CAM_TPG, "TPG[%d] CRM enable link done", tpg_dev->soc_info.index);
	} else {
		tpg_dev->crm_intf.link_hdl = -1;
		tpg_dev->crm_intf.crm_cb = NULL;
		CAM_DBG(CAM_TPG, "TPG[%d] CRM disable link done", tpg_dev->soc_info.index);
	}
	mutex_unlock(&tpg_dev->mutex);
	return 0;
}

static int cam_tpg_notify_frame_skip(
	struct cam_req_mgr_apply_request *apply)
{
	CAM_DBG(CAM_TPG, "Got Skip frame from crm");
	return 0;
}

static int cam_tpg_apply_req(
	struct cam_req_mgr_apply_request *apply)
{
	if (!apply) {
		CAM_ERR(CAM_TPG, "invalid parameters");
		return -EINVAL;
	}
	CAM_DBG(CAM_TPG, "Got Apply request from crm %lld", apply->request_id);
	return 0;
}

static int cam_tpg_flush_req(
	struct cam_req_mgr_flush_request *flush)
{
	CAM_DBG(CAM_TPG, "Got Flush request from crm");
	return 0;
}

static int cam_tpg_process_crm_evt(
	struct cam_req_mgr_link_evt_data *event)
{

	struct cam_tpg_device *tpg_dev = NULL;
	if (!event) {
		CAM_ERR(CAM_TPG, "Invalid argument");
		return -EINVAL;
	}

	tpg_dev = cam_get_device_priv(event->dev_hdl);
	if (!tpg_dev) {
		CAM_ERR(CAM_TPG, "Invalid dev_hdl");
		return -EINVAL;
	}

	switch(event->evt_type) {
	case CAM_REQ_MGR_LINK_EVT_SOF_FREEZE:
		tpg_hw_dump_status(&tpg_dev->tpg_hw);
		break;
	default:
		CAM_DBG(CAM_TPG, "Got crm event notification: %d", event->evt_type);
		break;
	}
	return 0;
}

static int cam_tpg_dump_req(
	struct cam_req_mgr_dump_info *dump_info)
{
	CAM_DBG(CAM_TPG, "Got dump request from CRM");
	return 0;
}

int tpg_crm_intf_init(
	struct cam_tpg_device *tpg_dev)
{
	if (tpg_dev == NULL)
		return -EINVAL;

	tpg_dev->crm_intf.device_hdl = -1;
	tpg_dev->crm_intf.link_hdl = -1;
	tpg_dev->crm_intf.ops.get_dev_info = cam_tpg_publish_dev_info;
	tpg_dev->crm_intf.ops.link_setup = cam_tpg_setup_link;
	tpg_dev->crm_intf.ops.apply_req = cam_tpg_apply_req;
	tpg_dev->crm_intf.ops.notify_frame_skip =
		cam_tpg_notify_frame_skip;
	tpg_dev->crm_intf.ops.flush_req = cam_tpg_flush_req;
	tpg_dev->crm_intf.ops.process_evt = cam_tpg_process_crm_evt;
	tpg_dev->crm_intf.ops.dump_req = cam_tpg_dump_req;

	return 0;
}

static int __cam_tpg_handle_query_cap(
	struct cam_tpg_device *tpg_dev,
	struct cam_tpg_query_cap *query)
{
	struct cam_hw_soc_info *soc_info = NULL;

	if (!tpg_dev || !query) {
		CAM_ERR(CAM_TPG, "invalid argument");
		return -EINVAL;
	}

	soc_info = &tpg_dev->soc_info;
	CAM_DBG(CAM_TPG, "Handling tpg query capability for %d slot: %d phy:%d",
			soc_info->index, tpg_dev->slot_id, tpg_dev->phy_id);
	query->slot_info = soc_info->index;
	query->csiphy_slot_id = tpg_dev->phy_id;
	query->version = 0x0;
	if (tpg_dev->tpg_hw.hw_info) {
		query->version   = tpg_dev->tpg_hw.hw_info->version;
	} else {
		CAM_ERR(CAM_TPG, "Invalid hw info");
		return -EINVAL;
	}

	return 0;
}

static int __cam_tpg_handle_acquire_dev(
	struct cam_tpg_device *tpg_dev,
	struct cam_tpg_acquire_dev *acquire)
{
	int rc = 0;
	struct cam_create_dev_hdl crm_intf_params;

	if (!tpg_dev || !acquire) {
		CAM_ERR(CAM_TPG, "invalid input ");
		rc = -EINVAL;
		goto exit;
	}

	if (tpg_dev->state != CAM_TPG_STATE_INIT) {
		CAM_ERR(CAM_TPG, "TPG[%d] not in right state[%d] to acquire",
				tpg_dev->soc_info.index, tpg_dev->state);
		rc = -EINVAL;
		goto exit;
	}

	crm_intf_params.session_hdl = acquire->session_handle;
	crm_intf_params.ops = &tpg_dev->crm_intf.ops;
	crm_intf_params.v4l2_sub_dev_flag = 0;
	crm_intf_params.media_entity_flag = 0;
	crm_intf_params.priv = tpg_dev;
	crm_intf_params.dev_id = CAM_TPG;

	acquire->device_handle =
		cam_create_device_hdl(&crm_intf_params);
	tpg_dev->crm_intf.device_hdl = acquire->device_handle;
	tpg_dev->crm_intf.session_hdl = acquire->session_handle;
	rc = tpg_hw_acquire(&tpg_dev->tpg_hw, (struct tpg_hw_acquire_args *)NULL);
	if (rc) {
		CAM_ERR(CAM_TPG, "TPG[%d] hw acquire failed", tpg_dev->soc_info.index);
		cam_destroy_device_hdl(tpg_dev->crm_intf.device_hdl);
		tpg_dev->crm_intf.device_hdl  = -1;
		tpg_dev->crm_intf.session_hdl = -1;
	} else {
		tpg_dev->state = CAM_TPG_STATE_ACQUIRE;
		CAM_INFO(CAM_TPG, "TPG[%d] Acquire Device done", tpg_dev->soc_info.index);
	}
exit:
	return rc;
}

static int __cam_tpg_handle_release_dev(
	struct cam_tpg_device *tpg_dev,
	struct cam_release_dev_cmd *release)
{
	int rc = 0;

	if (!release || !tpg_dev) {
		CAM_ERR(CAM_TPG, "Invalid params");
		return -EINVAL;
	}

	if (release->dev_handle <= 0) {
		CAM_ERR(CAM_TPG, "Invalid device handle for context");
		return -EINVAL;
	}

	if (release->session_handle <= 0) {
		CAM_ERR(CAM_TPG, "Invalid session handle for context");
		return -EINVAL;
	}
	if (tpg_dev->state == CAM_TPG_STATE_INIT) {
		CAM_WARN(CAM_TPG, "TPG[%d] not in right state[%d] to release",
				tpg_dev->soc_info.index, tpg_dev->state);
		return 0;
	}

	if (tpg_dev->state == CAM_TPG_STATE_START) {
		CAM_DBG(CAM_TPG, "TPG[%d] release from start state",
						tpg_dev->soc_info.index);
		rc = tpg_hw_stop(&tpg_dev->tpg_hw);
		if (rc < 0) {
			CAM_ERR(CAM_TPG, "TPG[%d] unable to stop tpg",
						tpg_dev->soc_info.index);
			return rc;
		}
	}
	rc = tpg_hw_release(&tpg_dev->tpg_hw);
	if (rc) {
		CAM_ERR(CAM_TPG, "TPG[%d] hw release failed",
						tpg_dev->soc_info.index);
	} else {
		cam_destroy_device_hdl(tpg_dev->crm_intf.device_hdl);
		tpg_dev->crm_intf.device_hdl  = -1;
		tpg_dev->crm_intf.session_hdl = -1;
		CAM_INFO(CAM_TPG, "TPG[%d] Release Done.", tpg_dev->soc_info.index);
		tpg_dev->state = CAM_TPG_STATE_INIT;
	}

	return rc;
}

static int __cam_tpg_handle_start_dev(
	struct cam_tpg_device *tpg_dev,
	struct cam_start_stop_dev_cmd *start)
{
	int rc = 0;
	if (!start || !tpg_dev)
		return -EINVAL;

	if (start->dev_handle <= 0) {
		CAM_ERR(CAM_TPG, "Invalid device handle for context");
		return -EINVAL;
	}

	if (start->session_handle <= 0) {
		CAM_ERR(CAM_TPG, "Invalid session handle for context");
		return -EINVAL;
	}
	if (tpg_dev->state != CAM_TPG_STATE_ACQUIRE) {
		CAM_ERR(CAM_TPG, "TPG[%d] not in right state[%d] to start",
				tpg_dev->soc_info.index, tpg_dev->state);
		return -EINVAL;
	}
	rc = tpg_hw_start(&tpg_dev->tpg_hw);
	if (rc) {
		CAM_ERR(CAM_TPG, "TPG[%d] START_DEV failed", tpg_dev->soc_info.index);
	} else {
		tpg_dev->state = CAM_TPG_STATE_START;
		CAM_INFO(CAM_TPG, "TPG[%d] START_DEV done.", tpg_dev->soc_info.index);
	}

	return rc;
}

static int __cam_tpg_handle_stop_dev(
	struct cam_tpg_device *tpg_dev,
	struct cam_start_stop_dev_cmd *stop)
{
	int rc = 0;
	if (!stop || !tpg_dev)
		return -EINVAL;

	if (stop->dev_handle <= 0) {
		CAM_ERR(CAM_TPG, "Invalid device handle for context");
		return -EINVAL;
	}

	if (stop->session_handle <= 0) {
		CAM_ERR(CAM_TPG, "Invalid session handle for context");
		return -EINVAL;
	}
	if (tpg_dev->state != CAM_TPG_STATE_START) {
		CAM_WARN(CAM_TPG, "TPG[%d] not in right state[%d] to stop",
				tpg_dev->soc_info.index, tpg_dev->state);
	}
	rc = tpg_hw_stop(&tpg_dev->tpg_hw);
	if (rc) {
		CAM_ERR(CAM_TPG, "TPG[%d] STOP_DEV failed", tpg_dev->soc_info.index);
	} else {
		/* Free all allocated streams during stop dev */
		tpg_hw_free_streams(&tpg_dev->tpg_hw);
		tpg_dev->state = CAM_TPG_STATE_ACQUIRE;
		CAM_INFO(CAM_TPG, "TPG[%d] STOP_DEV done.", tpg_dev->soc_info.index);
	}

	return rc;
}

static int cam_tpg_validate_cmd_descriptor(
	struct cam_cmd_buf_desc *cmd_desc,
	uint32_t *cmd_type, uintptr_t *cmd_addr)
{
	int rc = 0;
	uintptr_t generic_ptr;
	size_t len_of_buff = 0;
	uint32_t                *cmd_buf = NULL;
	struct tpg_command_header_t *cmd_header = NULL;

	if (!cmd_desc || !cmd_type || !cmd_addr)
		return -EINVAL;

	rc = cam_mem_get_cpu_buf(cmd_desc->mem_handle,
		&generic_ptr, &len_of_buff);
	if (rc < 0) {
		CAM_ERR(CAM_TPG,
			"Failed to get cmd buf Mem address : %d", rc);
		return rc;
	}

	cmd_buf = (uint32_t *)generic_ptr;
	cmd_buf += cmd_desc->offset / 4;
	cmd_header = (struct tpg_command_header_t *)cmd_buf;

	if (len_of_buff < sizeof(struct tpg_command_header_t)) {
		CAM_ERR(CAM_TPG, "Got invalid command descriptor of invalid cmd buffer size");
		rc = -EINVAL;
		goto end;
	}

	switch (cmd_header->cmd_type) {
	case TPG_CMD_TYPE_GLOBAL_CONFIG: {
		if (cmd_header->size != sizeof(struct tpg_global_config_t)) {
			CAM_ERR(CAM_TPG, "Got invalid global config command recv: %d exp: %d",
					cmd_header->size,
					sizeof(struct tpg_global_config_t));
			rc = -EINVAL;
			goto end;
		}
		CAM_INFO(CAM_TPG, "Got global config cmd");
		*cmd_type = TPG_CMD_TYPE_GLOBAL_CONFIG;
		break;
	}
	case TPG_CMD_TYPE_STREAM_CONFIG: {
		if (cmd_header->size != sizeof(struct tpg_stream_config_t)) {
			CAM_ERR(CAM_TPG, "Got invalid stream config command recv: %d exp: %d",
					cmd_header->size,
					sizeof(struct tpg_stream_config_t));

			rc = -EINVAL;
			goto end;
		}
		CAM_INFO(CAM_TPG, "Got stream config cmd");
		*cmd_type = TPG_CMD_TYPE_STREAM_CONFIG;
		break;
	}
	case TPG_CMD_TYPE_ILLUMINATION_CONFIG: {
		if (cmd_header->size != sizeof(struct tpg_illumination_control)) {
			CAM_ERR(CAM_TPG, "Got invalid illumination config command");
			rc = -EINVAL;
			goto end;
		}
		*cmd_type = TPG_CMD_TYPE_ILLUMINATION_CONFIG;
		break;
	}
	default:
		rc = -EINVAL;
		CAM_ERR(CAM_TPG, "invalid config command");
		goto end;
	}
	if ((ssize_t)cmd_desc->offset > (len_of_buff - cmd_header->size)) {
		CAM_ERR(CAM_TPG, "cmd header offset mismatch");
		rc = -EINVAL;
	}

	*cmd_addr = (uintptr_t)cmd_header;
end:
	return rc;
}

static int cam_tpg_cmd_buf_parse(
	struct cam_tpg_device *tpg_dev,
	struct cam_packet *packet)
{
	int rc = 0, i = 0;
	struct cam_cmd_buf_desc *cmd_desc = NULL;

	if (!tpg_dev || !packet)
		return -EINVAL;

	for (i = 0; i < packet->num_cmd_buf; i++) {
		uint32_t cmd_type = TPG_CMD_TYPE_INVALID;
		uintptr_t cmd_addr;

		cmd_desc = (struct cam_cmd_buf_desc *)
			((uint32_t *)&packet->payload +
			(packet->cmd_buf_offset / 4) +
			(i * (sizeof(struct cam_cmd_buf_desc)/4)));

		rc = cam_tpg_validate_cmd_descriptor(cmd_desc,
				&cmd_type, &cmd_addr);
		if (rc < 0)
			goto end;

		switch (cmd_type) {
		case TPG_CMD_TYPE_GLOBAL_CONFIG:
			rc = tpg_hw_copy_global_config(&tpg_dev->tpg_hw,
				(struct tpg_global_config_t *)cmd_addr);
			break;
		case TPG_CMD_TYPE_STREAM_CONFIG: {
			rc = tpg_hw_add_stream(&tpg_dev->tpg_hw,
				(struct tpg_stream_config_t *)cmd_addr);
			break;
		}
		case TPG_CMD_TYPE_ILLUMINATION_CONFIG:
			CAM_ERR(CAM_TPG, "TPG[%d] ILLUMINATION CONFIG not supported now ",
								tpg_dev->soc_info.index);
			break;
		default:
			CAM_ERR(CAM_TPG, "TPG[%d] invalid command %d",
				tpg_dev->soc_info.index,
				cmd_type);
			rc = -EINVAL;
			break;
		}
	}
end:
	return rc;
}

static int cam_tpg_packet_parse(
	struct cam_tpg_device *tpg_dev,
	struct cam_config_dev_cmd *config)
{
	int rc = 0;
	uintptr_t generic_ptr;
	size_t len_of_buff = 0, remain_len = 0;
	struct cam_packet *csl_packet = NULL;

	rc = cam_mem_get_cpu_buf(config->packet_handle,
		&generic_ptr, &len_of_buff);
	if (rc < 0) {
		CAM_ERR(CAM_TPG, "Failed in getting the packet: %d", rc);
		return rc;
	}

	if ((sizeof(struct cam_packet) > len_of_buff) ||
		((size_t)config->offset >= len_of_buff -
		sizeof(struct cam_packet))) {
		CAM_ERR(CAM_TPG,
			"Inval cam_packet struct size: %zu, len_of_buff: %zu",
			 sizeof(struct cam_packet), len_of_buff);
		rc = -EINVAL;
		goto end;
	}
	remain_len = len_of_buff;
	remain_len -= (size_t)config->offset;
	csl_packet = (struct cam_packet *)(generic_ptr +
		(uint32_t)config->offset);

	if (cam_packet_util_validate_packet(csl_packet,
		remain_len)) {
		CAM_ERR(CAM_TPG, "Invalid packet params");
		rc = -EINVAL;
		goto end;
	}

	CAM_DBG(CAM_TPG, "TPG[%d] "
			"CONFIG_DEV, Packet opcode = %d num_cmds: %d num_ios: %d num_patch: %d",
			tpg_dev->soc_info.index,
			(csl_packet->header.op_code & 0xFF),
			csl_packet->num_cmd_buf,
			csl_packet->num_io_configs,
			csl_packet->num_patches);
	switch ((csl_packet->header.op_code & 0xFF)) {
	case CAM_TPG_PACKET_OPCODE_INITIAL_CONFIG: {
		if (csl_packet->num_cmd_buf <= 0) {
			CAM_ERR(CAM_TPG, "Expecting atleast one command in Init packet");
			rc = -EINVAL;
			goto end;
		}
		rc = cam_tpg_cmd_buf_parse(tpg_dev, csl_packet);
		if (rc < 0) {
			CAM_ERR(CAM_TPG, "CMD buffer parse failed");
			goto end;
		}
		tpg_hw_config(&tpg_dev->tpg_hw, TPG_HW_CMD_INIT_CONFIG, NULL);
		break;
	}
	case CAM_TPG_PACKET_OPCODE_NOP: {
		struct cam_req_mgr_add_request add_req = {0};

		CAM_DBG(CAM_TPG, "TPG[%d] NOP packet request id : %llu",
				tpg_dev->soc_info.index,
				csl_packet->header.request_id);
		if ((tpg_dev->crm_intf.link_hdl != -1) &&
			(tpg_dev->crm_intf.device_hdl != -1) &&
			(tpg_dev->crm_intf.crm_cb != NULL)) {
			add_req.link_hdl = tpg_dev->crm_intf.link_hdl;
			add_req.dev_hdl  = tpg_dev->crm_intf.device_hdl;
			add_req.req_id = csl_packet->header.request_id;
			tpg_dev->crm_intf.crm_cb->add_req(&add_req);
		} else {
			CAM_ERR(CAM_TPG, "TPG[%d] invalid link req: %llu",
					tpg_dev->soc_info.index,
					csl_packet->header.request_id);
		}
		break;
	}
	default:
		CAM_ERR(CAM_TPG, "TPG[%d] Invalid packet %x",
					tpg_dev->soc_info.index,
					(csl_packet->header.op_code & 0xFF));
		rc = -EINVAL;
		break;
	}
end:
	return rc;
}

static int __cam_tpg_handle_config_dev(
	struct cam_tpg_device *tpg_dev,
	struct cam_config_dev_cmd *config)
{
	int rc = 0;

	if (!config || !tpg_dev)
		return -EINVAL;

	if (config->dev_handle <= 0) {
		CAM_ERR(CAM_TPG, "TPG[%d] Invalid device handle",
				tpg_dev->soc_info.index);
		return -EINVAL;
	}

	if (config->session_handle <= 0) {
		CAM_ERR(CAM_TPG, "TPG[%d] Invalid session handle",
				tpg_dev->soc_info.index);
		return -EINVAL;
	}

	if (tpg_dev->state < CAM_TPG_STATE_ACQUIRE) {
		CAM_ERR(CAM_TPG, "TPG[%d] not in right state[%d] to configure",
				tpg_dev->soc_info.index, tpg_dev->state);
	}
	// Handle Config Dev
	rc = cam_tpg_packet_parse(tpg_dev, config);
	return rc;
}

static int validate_ioctl_params(
	struct cam_tpg_device *tpg_dev,
	struct cam_control *cmd)
{
	int rc = 0;

	if (!tpg_dev || !cmd) {
		CAM_ERR(CAM_TPG, "Invalid input args");
		rc = -EINVAL;
		goto exit;
	}

	if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_TPG, "TPG[%d] Invalid handle type: %d",
			tpg_dev->soc_info.index,
			cmd->handle_type);
		rc = -EINVAL;
	}
	CAM_DBG(CAM_TPG, "TPG[%d] Opcode: %d", tpg_dev->soc_info.index, cmd->op_code);
exit:
	return rc;
}

int cam_tpg_core_cfg(
	struct cam_tpg_device *tpg_dev,
	void *arg)
{
	int rc = 0;
	struct cam_control   *cmd = (struct cam_control *)arg;

	rc = validate_ioctl_params(tpg_dev, cmd);
	if (rc < 0)
		return rc;

	mutex_lock(&tpg_dev->mutex);
	switch (cmd->op_code) {
	case CAM_QUERY_CAP: {
		struct cam_tpg_query_cap query = {0};

		if (copy_from_user(&query, u64_to_user_ptr(cmd->handle),
			sizeof(query))) {
			rc = -EFAULT;
			break;
		}

		rc = __cam_tpg_handle_query_cap(tpg_dev, &query);
		if (rc) {
			CAM_ERR(CAM_TPG, "TPG[%d] querycap is failed(rc = %d)",
				tpg_dev->soc_info.index,
				rc);
			break;
		}

		if (copy_to_user(u64_to_user_ptr(cmd->handle), &query,
			sizeof(query)))
			rc = -EFAULT;

		break;
	}
	case CAM_ACQUIRE_DEV: {
		struct cam_tpg_acquire_dev acquire = {0};

		if (copy_from_user(&acquire, u64_to_user_ptr(cmd->handle),
			sizeof(acquire))) {
			rc = -EFAULT;
			break;
		}
		rc = __cam_tpg_handle_acquire_dev(tpg_dev, &acquire);
		if (rc) {
			CAM_ERR(CAM_TPG, "TPG[%d] acquire device failed(rc = %d)",
				tpg_dev->soc_info.index,
				rc);
			break;
		}
		if (copy_to_user(u64_to_user_ptr(cmd->handle), &acquire,
			sizeof(acquire)))
			rc = -EFAULT;
		break;
	}
	case CAM_RELEASE_DEV: {
		struct cam_release_dev_cmd release;

		if (copy_from_user(&release, u64_to_user_ptr(cmd->handle),
			sizeof(release)))
			rc = -EFAULT;
		else {
			rc = __cam_tpg_handle_release_dev(tpg_dev, &release);
			if (rc)
				CAM_ERR(CAM_TPG,
					"TPG[%d] release device failed(rc = %d)",
					tpg_dev->soc_info.index,
					rc);
		}
		break;
	}
	case CAM_START_DEV: {
		struct cam_start_stop_dev_cmd start;

		if (copy_from_user(&start, u64_to_user_ptr(cmd->handle),
			sizeof(start)))
			rc = -EFAULT;
		else {
			rc = __cam_tpg_handle_start_dev(tpg_dev, &start);
			if (rc)
				CAM_ERR(CAM_TPG,
					"TPG[%d] start device failed(rc = %d)",
					tpg_dev->soc_info.index,
					rc);
		}
		break;
	}
	case CAM_STOP_DEV: {
		struct cam_start_stop_dev_cmd stop;

		if (copy_from_user(&stop, u64_to_user_ptr(cmd->handle),
			sizeof(stop)))
			rc = -EFAULT;
		else {
			rc = __cam_tpg_handle_stop_dev(tpg_dev, &stop);
			if (rc)
				CAM_ERR(CAM_TPG,
					"TPG[%d] stop device failed(rc = %d)",
					tpg_dev->soc_info.index,
					rc);
		}
		break;

	}
	case CAM_CONFIG_DEV: {
		struct cam_config_dev_cmd config;

		if (copy_from_user(&config, u64_to_user_ptr(cmd->handle),
			sizeof(config)))
			rc = -EFAULT;
		else {
			rc = __cam_tpg_handle_config_dev(tpg_dev, &config);
			if (rc)
				CAM_ERR(CAM_TPG,
					"TPG[%d] config device failed(rc = %d)",
					tpg_dev->soc_info.index,
					rc);
		}
		break;
	}
	default:
		CAM_ERR(CAM_TPG, "Invalid ioctl : %d", cmd->op_code);
		rc = -EINVAL;
		break;
	}
	mutex_unlock(&tpg_dev->mutex);
	return rc;
}
