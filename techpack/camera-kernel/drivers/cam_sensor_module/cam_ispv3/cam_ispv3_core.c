// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <cam_sensor_cmn_header.h>
#include "cam_ispv3_core.h"
#include "cam_sensor_util.h"
#include "cam_soc_util.h"
#include "cam_trace.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"

static void cam_ispv3_update_req_mgr(struct cam_ispv3_ctrl_t *s_ctrl,
				     struct cam_packet *csl_packet)
{
	struct cam_req_mgr_add_request add_req;
	uint32_t reg_val;
	int      ret = 0;

	add_req.link_hdl = s_ctrl->bridge_intf.link_hdl;
	add_req.req_id = csl_packet->header.request_id;
	CAM_DBG(CAM_ISPV3, " Rxed Req Id: %lld",
		csl_packet->header.request_id);

	add_req.dev_hdl = s_ctrl->bridge_intf.device_hdl;
	add_req.ispv3_key_reg_bad_state = FALSE;

	ret = cam_ispv3_reg_read(s_ctrl->priv, 0xffed2300, &reg_val);

	if (!ret &&
		(reg_val & 0x4)) {
		CAM_ERR(CAM_ISPV3, "mipi tx stop streaming, trigger recovery now , reg_val %d",reg_val);
		add_req.ispv3_key_reg_bad_state = TRUE;
	} else if (!ret){
		CAM_DBG(CAM_ISPV3, "mipi tx is normally streaming, reg_val %d",reg_val);
	} else {
		CAM_ERR(CAM_ISPV3, "read reg val failed!!! ret %d",ret);}

	if (s_ctrl->bridge_intf.crm_cb &&
		s_ctrl->bridge_intf.crm_cb->add_req) {
		s_ctrl->bridge_intf.crm_cb->add_req(&add_req);
		CAM_INFO(CAM_ISPV3, "ISPV3 Request Id: %lld added to CRM",
			 add_req.req_id);
	} else {
		CAM_ERR(CAM_ISPV3, "ISPV3 Can't add Request ID: %lld to CRM",
			csl_packet->header.request_id);
	}
}

void cam_ispv3_shutdown(struct cam_ispv3_ctrl_t *s_ctrl)
{
	int ret = 0;

	if (s_ctrl->bridge_intf.device_hdl != -1) {
		ret = cam_destroy_device_hdl(s_ctrl->bridge_intf.device_hdl);
		if (ret < 0)
			CAM_ERR(CAM_ISPV3,
				"dhdl already destroyed: ret = %d", ret);
	}

	s_ctrl->bridge_intf.device_hdl = -1;
	s_ctrl->bridge_intf.link_hdl = -1;
	s_ctrl->bridge_intf.session_hdl = -1;
	s_ctrl->ispv3_state = CAM_ISPV3_INIT;
}

int32_t cam_ispv3_process_config(struct cam_ispv3_ctrl_t *s_ctrl, void *arg)
{
	int ret = 0;
	uintptr_t generic_pkt_addr, cmd_buf_ptr;
	size_t pkt_len;
	size_t remain_len = 0, len_of_buffer;
	struct cam_control *ioctl_ctrl = NULL;
	struct cam_config_dev_cmd dev_config;
	struct cam_packet *csl_packet = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	uint32_t *cmd_buf =  NULL;
	uint32_t *offset = NULL;
	int frame_off = 0;
	struct ispv3_cmd_set *cmd_set = NULL;

	ioctl_ctrl = (struct cam_control *)arg;
	if (copy_from_user(&dev_config,
		u64_to_user_ptr(ioctl_ctrl->handle),
		sizeof(dev_config)))
		return -EFAULT;
	ret = cam_mem_get_cpu_buf(dev_config.packet_handle,
		&generic_pkt_addr, &pkt_len);
	if (ret) {
		CAM_ERR(CAM_ISPV3,
			"ISPV3 error in converting command Handle Error: %d", ret);
		return ret;
	}
	remain_len = pkt_len;
	if ((sizeof(struct cam_packet) > pkt_len) ||
		((size_t)dev_config.offset >= pkt_len -
		sizeof(struct cam_packet))) {
		CAM_ERR(CAM_ISPV3,
			"Inval cam_packet strut size: %zu, len_of_buff: %zu",
			 sizeof(struct cam_packet), pkt_len);
		return -EINVAL;
	}

	remain_len -= (size_t)dev_config.offset;
	csl_packet = (struct cam_packet *)
		(generic_pkt_addr + (uint32_t)dev_config.offset);

	if (cam_packet_util_validate_packet(csl_packet,
		remain_len)) {
		CAM_ERR(CAM_ISPV3, "Invalid packet params");
		ret = -EINVAL;
		return ret;
	}

	offset = (uint32_t *)&csl_packet->payload;
	offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
	cmd_desc = (struct cam_cmd_buf_desc *)(offset);

	ret = cam_mem_get_cpu_buf(cmd_desc->mem_handle,
		&cmd_buf_ptr, &len_of_buffer);
	if (ret) {
		CAM_ERR(CAM_ISPV3, "Fail in get buffer: %d", ret);
		return ret;
	}
	if ((len_of_buffer < sizeof(struct ispv3_cmd_set)) ||
		(cmd_desc->offset >
		(len_of_buffer - sizeof(struct ispv3_cmd_set)))) {
		CAM_ERR(CAM_ISPV3, "Not enough buffer");
		ret = -EINVAL;
		return ret;
	}
	remain_len = len_of_buffer - cmd_desc->offset;
	cmd_buf = (uint32_t *)cmd_buf_ptr + cmd_desc->offset;
	cmd_set = (struct ispv3_cmd_set *)cmd_buf;
	frame_off = csl_packet->header.request_id % MAX_PER_FRAME_ARRAY;
	memcpy(&(s_ctrl->per_frame[frame_off]), cmd_set, sizeof(struct ispv3_cmd_set));

	cam_ispv3_update_req_mgr(s_ctrl, csl_packet);

	return ret;
}

int32_t cam_ispv3_driver_cmd(struct cam_ispv3_ctrl_t *s_ctrl,
			     void *arg)
{
	struct cam_control *cmd = (struct cam_control *)arg;
	struct ispv3_image_device *priv;
	struct ispv3_data *data;
	int ret = 0;

	if (!s_ctrl || !arg) {
		CAM_ERR(CAM_ISPV3, "s_ctrl is NULL");
		return -EINVAL;
	}

	priv = s_ctrl->priv;
	if (!priv) {
		CAM_ERR(CAM_ISPV3, "The ispv3 data struct is NULL!");
		return -EINVAL;
	}

	data = priv->pdata;
	if (!data) {
		CAM_ERR(CAM_ISPV3, "The ispv3 data struct is NULL!");
		return -EINVAL;
	}

	mutex_lock(&(s_ctrl->cam_ispv3_mutex));
	switch (cmd->op_code) {
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev sensor_acq_dev;
		struct cam_create_dev_hdl bridge_params;

		if (s_ctrl->bridge_intf.device_hdl != -1) {
			CAM_ERR(CAM_ISPV3, "Device is already acquired");
			ret = -EINVAL;
			goto release_mutex;
		}
		ret = copy_from_user(&sensor_acq_dev,
			u64_to_user_ptr(cmd->handle),
			sizeof(sensor_acq_dev));
		if (ret < 0) {
			CAM_ERR(CAM_ISPV3, "Failed Copying from user");
			goto release_mutex;
		}

		if (sensor_acq_dev.reserved)
			s_ctrl->trigger_source = CAM_REQ_MGR_TRIG_SRC_EXTERNAL;
		else
			s_ctrl->trigger_source = CAM_REQ_MGR_TRIG_SRC_INTERNAL;

		bridge_params.session_hdl = sensor_acq_dev.session_handle;
		bridge_params.ops = &s_ctrl->bridge_intf.ops;
		bridge_params.v4l2_sub_dev_flag = 0;
		bridge_params.media_entity_flag = 0;
		bridge_params.priv = s_ctrl;
		bridge_params.dev_id = CAM_ISPV3;

		sensor_acq_dev.device_handle =
			cam_create_device_hdl(&bridge_params);
		s_ctrl->bridge_intf.device_hdl = sensor_acq_dev.device_handle;
		s_ctrl->bridge_intf.session_hdl = sensor_acq_dev.session_handle;

		CAM_DBG(CAM_ISPV3, "Device Handle: %d trigger_source: %s",
			sensor_acq_dev.device_handle,
			(s_ctrl->trigger_source == CAM_REQ_MGR_TRIG_SRC_INTERNAL) ?
			"internal" : "external");

		if (copy_to_user(u64_to_user_ptr(cmd->handle),
			&sensor_acq_dev,
			sizeof(struct cam_sensor_acquire_dev))) {
			CAM_ERR(CAM_ISPV3, "Failed Copy to User");
			ret = -EFAULT;
			goto release_mutex;
		}

		s_ctrl->ispv3_state = CAM_ISPV3_ACQUIRE;
		CAM_INFO(CAM_ISPV3,
			"CAM_ACQUIRE_DEV Success");
	}
		break;
	case CAM_RELEASE_DEV: {
		if ((s_ctrl->ispv3_state == CAM_ISPV3_INIT) ||
			(s_ctrl->ispv3_state == CAM_ISPV3_START)) {
			ret = -EINVAL;
			CAM_WARN(CAM_ISPV3,
			"Not in right state to release : %d",
			s_ctrl->ispv3_state);
			goto release_mutex;
		}

		if (s_ctrl->bridge_intf.link_hdl != -1) {
			CAM_ERR(CAM_ISPV3,
				"Device [%d] still active on link 0x%x",
				s_ctrl->ispv3_state,
				s_ctrl->bridge_intf.link_hdl);
			ret = -EAGAIN;
			goto release_mutex;
		}

		ret = cam_destroy_device_hdl(s_ctrl->bridge_intf.device_hdl);
		if (ret < 0)
			CAM_ERR(CAM_ISPV3,
				"failed in destroying the device hdl");
		s_ctrl->bridge_intf.device_hdl = -1;
		s_ctrl->bridge_intf.link_hdl = -1;
		s_ctrl->bridge_intf.session_hdl = -1;

		s_ctrl->ispv3_state = CAM_ISPV3_INIT;
		CAM_INFO(CAM_ISPV3, "CAM_RELEASE_DEV Success");
	}
		break;

	case CAM_START_DEV: {
		CAM_ERR(CAM_ISPV3, "ISPV3 STARTDEV state %d", s_ctrl->ispv3_state);
	}
		break;
	case CAM_STOP_DEV: {
		CAM_ERR(CAM_ISPV3, "ISPV3 CAM_STOP_DEV ");
	}
		break;
	case CAM_CONFIG_DEV: {
		cam_ispv3_process_config(s_ctrl, arg);
	}
		break;

	case ISP_OPCODE_DISABLE_PCIE_LINK:
		atomic_set(&data->pci_link_state, ISPV3_PCI_LINK_DOWN);
		mutex_unlock(&(s_ctrl->cam_ispv3_mutex));
		ret = ispv3_suspend_pci_link(data);
		if (ret)
			CAM_ERR(CAM_ISPV3, "cannot resume the ispv3 device");
		break;

	case ISP_OPCODE_PWR_ON:
		if (FRENQUENCY_1066MHZ == cmd->handle)
			s_ctrl->is4K = true;
		else
			s_ctrl->is4K = false;
		ret = cam_ispv3_turn_on(s_ctrl, priv, cmd->handle);
		break;

	case ISP_OPCODE_PWR_OFF:
		ret = cam_ispv3_turn_off(priv);
		break;

	case ISP_OPCODE_CHANGE_SPI_SPEED:
		ret = cam_ispv3_spi_change_speed(priv, cmd);
		break;

	case ISP_OPCODE_READ:
		ret = cam_ispv3_read(priv, cmd);
		break;

	case ISP_OPCODE_WRITE:
		ret = cam_ispv3_write(priv, cmd);
		break;

	case ISP_OPCODE_MISN_CONFIG:
		ret = cam_ispv3_reg_write(priv, ISPV3_SW_DATA_CPU_ADDR,
					  ISPV3_SW_DATA_CPU_MISN_CFG);
		if (ret)
			break;
		ret = cam_ispv3_reg_write(priv, ISPV3_SW_TRIGGER_CPU_ADDR,
					  ISPV3_SW_TRIGGER_CPU_VAL);
		if (ret)
			break;
		if (!wait_for_completion_timeout(&priv->comp_setup,
						 msecs_to_jiffies(1000))) {
			CAM_ERR(CAM_ISPV3, "wait for completion timeout!");
			ret = -ETIMEDOUT;
		}
		break;

	case ISP_OPCODE_POLL_EXIT:
		cam_ispv3_notify_message(s_ctrl,
					 V4L_EVENT_CAM_REQ_MGR_POLL_EXIT,
					 V4L_EVENT_CAM_REQ_MGR_EVENT);
		break;

	default:
		CAM_ERR(CAM_ISPV3, "Invalid Opcode: %d", cmd->op_code);
		ret = -EINVAL;
		goto release_mutex;
	}

release_mutex:
	if (cmd->op_code != ISP_OPCODE_DISABLE_PCIE_LINK)
		mutex_unlock(&(s_ctrl->cam_ispv3_mutex));

	return ret;
}

int cam_ispv3_publish_dev_info(struct cam_req_mgr_device_info *info)
{
	int ret = 0;
	struct cam_ispv3_ctrl_t *s_ctrl = NULL;

	if (!info)
		return -EINVAL;

	s_ctrl = (struct cam_ispv3_ctrl_t *)
		 cam_get_device_priv(info->dev_hdl);

	if (!s_ctrl) {
		CAM_ERR(CAM_ISPV3, "Device data is NULL");
		return -EINVAL;
	}

	info->dev_id = CAM_REQ_MGR_DEVICE_ISPV3;
	strlcpy(info->name, CAM_ISPV3_NAME, sizeof(info->name));
	info->p_delay = 1;
	info->trigger = CAM_TRIGGER_POINT_SOF;
	info->trigger_source = s_ctrl->trigger_source;
	info->latest_frame_id = -1;
	CAM_DBG(CAM_ISPV3, "ISPV3 trigger source:%d", info->trigger_source);
	return ret;
}

int cam_ispv3_establish_link(struct cam_req_mgr_core_dev_link_setup *link)
{
	struct cam_ispv3_ctrl_t *s_ctrl = NULL;

	if (!link)
		return -EINVAL;

	s_ctrl = (struct cam_ispv3_ctrl_t *)
		cam_get_device_priv(link->dev_hdl);
	if (!s_ctrl) {
		CAM_ERR(CAM_ISPV3, "Device data is NULL");
		return -EINVAL;
	}

	mutex_lock(&s_ctrl->cam_ispv3_mutex);
	if (link->link_enable) {
		s_ctrl->bridge_intf.link_hdl = link->link_hdl;
		s_ctrl->bridge_intf.crm_cb = link->crm_cb;
	} else {
		s_ctrl->bridge_intf.link_hdl = -1;
		s_ctrl->bridge_intf.crm_cb = NULL;
	}
	mutex_unlock(&s_ctrl->cam_ispv3_mutex);

	return 0;
}

int cam_ispv3_power(struct v4l2_subdev *sd, int on)
{
	struct cam_ispv3_ctrl_t *s_ctrl = v4l2_get_subdevdata(sd);

	mutex_lock(&(s_ctrl->cam_ispv3_mutex));
	if (!on && s_ctrl->ispv3_state == CAM_ISPV3_START)
		s_ctrl->ispv3_state = CAM_ISPV3_ACQUIRE;
	mutex_unlock(&(s_ctrl->cam_ispv3_mutex));

	return 0;
}

int32_t cam_ispv3_apply_request(struct cam_req_mgr_apply_request *apply)
{
	int32_t ret = 0;
	struct cam_ispv3_ctrl_t *s_ctrl = NULL;
	int frame_off = 0;

	if (!apply)
		return -EINVAL;
	s_ctrl = (struct cam_ispv3_ctrl_t *) cam_get_device_priv(apply->dev_hdl);
	frame_off = apply->request_id % MAX_PER_FRAME_ARRAY;

	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_PR_PARA_LONG_ISO,
				 s_ctrl->per_frame[frame_off].aec_iso[1]);
	if (ret != 0)
		CAM_ERR(CAM_ISPV3, "write LONG_GAIN Fail");
	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_PR_PARA_LONG_ET,
				 s_ctrl->per_frame[frame_off].aec_exposure_time[1]);
	if (ret != 0)
		CAM_ERR(CAM_ISPV3, "write LONG_ET Fail");

#ifdef ISPV3_WHOLE_SESSION
	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_PR_PARA_LONG_BGAIN,
				 s_ctrl->per_frame[frame_off].awb_ctl_b_gain);
	if (ret != 0)
		CAM_ERR(CAM_ISPV3, "write LONG_BGAIN Fail");
	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_PR_PARA_LONG_RGAIN,
				 s_ctrl->per_frame[frame_off].awb_ctl_r_gain);
	if (ret != 0)
		CAM_ERR(CAM_ISPV3, "write LONG_RGAIN Fail");
	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_PR_PARA_LONG_G0GAIN,
				 s_ctrl->per_frame[frame_off].awb_ctl_g_gain);
	if (ret != 0)
		CAM_ERR(CAM_ISPV3, "write LONG_G0GAIN Fail");
	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_PR_PARA_LONG_G1GAIN,
				 s_ctrl->per_frame[frame_off].awb_ctl_g_gain);
	if (ret != 0)
		CAM_ERR(CAM_ISPV3, "write LONG_G1GAIN Fail");
	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_PR_PARA_SAFE_ISO,
				 s_ctrl->per_frame[frame_off].aec_iso[2]);
	if (ret != 0)
		CAM_ERR(CAM_ISPV3, "write SAFE_ISO Fail");
	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_PR_PARA_SAFE_ET,
				 s_ctrl->per_frame[frame_off].aec_exposure_time[2]);
	if (ret != 0)
		CAM_ERR(CAM_ISPV3, "write SAFE_ET Fail");
	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_PR_PARA_SAFE_BGAIN,
				 s_ctrl->per_frame[frame_off].awb_ctl_b_gain);
	if (ret != 0)
		CAM_ERR(CAM_ISPV3, "write SAFE_BGAIN Fail");
	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_PR_PARA_SAFE_RGAIN,
				 s_ctrl->per_frame[frame_off].awb_ctl_r_gain);
	if (ret != 0)
		CAM_ERR(CAM_ISPV3, "write SAFE_RGAIN Fail");
	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_PR_PARA_SAFE_G0GAIN,
				 s_ctrl->per_frame[frame_off].awb_ctl_g_gain);
	if (ret != 0)
		CAM_ERR(CAM_ISPV3, "write SAFE_G0GAIN Fail");
	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_PR_PARA_SAFE_G1GAIN,
				 s_ctrl->per_frame[frame_off].awb_ctl_g_gain);
	if (ret != 0)
		CAM_ERR(CAM_ISPV3, "write SAFE_G1GAIN Fail");

#endif
	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_PR_PARA_LONG_RBBLACKPOINT,
				 ((s_ctrl->per_frame[frame_off].aec_b_black_point[1]) << 16) +
				 s_ctrl->per_frame[frame_off].aec_r_black_point[1]);
	if(ret != 0)
		CAM_ERR(CAM_ISPV3, "write LONG_RBBLACKPOINT Fail");

	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_PR_PARA_LONG_G0G1BLACKPOINT,
				 ((s_ctrl->per_frame[frame_off].aec_g1_black_point[1]) << 16) +
				 s_ctrl->per_frame[frame_off].aec_g0_black_point[1]);
	if(ret != 0)
		CAM_ERR(CAM_ISPV3, "write LONG_G0G1BLACKPOINT Fail");

#ifdef ISPV3_WHOLE_SESSION
	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_PR_PARA_SAFE_RBBLACKPOINT,
				 ((s_ctrl->per_frame[frame_off].aec_b_black_point[2]) << 16) +
				 s_ctrl->per_frame[frame_off].aec_r_black_point[2]);
	if(ret != 0)
		CAM_ERR(CAM_ISPV3, "write SAFE_RBBLACKPOINT Fail");

	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_PR_PARA_SAFE_G0G1BLACKPOINT,
				 ((s_ctrl->per_frame[frame_off].aec_g1_black_point[2]) << 16) +
				 s_ctrl->per_frame[frame_off].aec_g0_black_point[2]);
	if(ret != 0)
		CAM_ERR(CAM_ISPV3, "write SAFE_G0G1BLACKPOINT Fail");

	CAM_DBG(CAM_ISPV3, "long AECRBlackPoint: %d, AECBBlackPoint: %d\n",
		s_ctrl->per_frame[frame_off].aec_r_black_point[1],
		s_ctrl->per_frame[frame_off].aec_b_black_point[1]);
	CAM_DBG(CAM_ISPV3, "long AECG0BlackPoint: %d, AECG1BlackPoint: %d\n",
		s_ctrl->per_frame[frame_off].aec_g0_black_point[1],
		s_ctrl->per_frame[frame_off].aec_g1_black_point[1]);
	CAM_DBG(CAM_ISPV3, "mid AECRBlackPoint: %d, AECBBlackPoint: %d\n",
		s_ctrl->per_frame[frame_off].aec_r_black_point[2],
		s_ctrl->per_frame[frame_off].aec_b_black_point[2]);
	CAM_DBG(CAM_ISPV3, "mid AECG0BlackPoint: %d, AECG1BlackPoint: %d\n",
		s_ctrl->per_frame[frame_off].aec_g0_black_point[2],
		s_ctrl->per_frame[frame_off].aec_g1_black_point[2]);
	CAM_DBG(CAM_ISPV3, "[XM_CC]:reqID: %d\n", apply->request_id);
#endif
	ret = cam_ispv3_reg_write(s_ctrl->priv, ISPV3_MISN_GROUP_EN, 0x1);
	if (ret != 0)
		CAM_ERR(CAM_ISPV3, "Group_EN Fail");

	return ret;
}

int32_t cam_ispv3_flush_request(struct cam_req_mgr_flush_request *flush_req)
{
	int32_t ret = 0;
	struct cam_ispv3_ctrl_t *s_ctrl = NULL;

	if (!flush_req)
		return -EINVAL;
	s_ctrl = (struct cam_ispv3_ctrl_t *) cam_get_device_priv(flush_req->dev_hdl);
	if (flush_req->type == CAM_REQ_MGR_FLUSH_TYPE_ALL) {
		s_ctrl->stop_notify_crm = true;
		CAM_INFO(CAM_ISPV3, "stop_notify_crm is %d", s_ctrl->stop_notify_crm);
	}
	CAM_ERR(CAM_ISPV3, "ISPV3 Flush");
	return ret;
}
