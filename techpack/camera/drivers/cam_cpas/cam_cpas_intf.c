// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/of.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/cam_cpas.h>
#include <media/cam_req_mgr.h>
#include <dt-bindings/msm/msm-camera.h>

#include "cam_subdev.h"
#include "cam_cpas_hw_intf.h"
#include "cam_cpas_soc.h"
#include "camera_main.h"
#include "cam_cpas_api.h"

#define CAM_CPAS_DEV_NAME    "cam-cpas"
#define CAM_CPAS_INTF_INITIALIZED() (g_cpas_intf && g_cpas_intf->probe_done)

/**
 * struct cam_cpas_intf : CPAS interface
 *
 * @pdev: Platform device
 * @subdev: Subdev info
 * @hw_intf: CPAS HW interface
 * @hw_caps: CPAS HW capabilities
 * @intf_lock: CPAS interface mutex
 * @open_cnt: CPAS subdev open count
 * @probe_done: Whether CPAS prove completed
 *
 */
struct cam_cpas_intf {
	struct platform_device *pdev;
	struct cam_subdev subdev;
	struct cam_hw_intf *hw_intf;
	struct cam_cpas_hw_caps hw_caps;
	struct mutex intf_lock;
	uint32_t open_cnt;
	bool probe_done;
};

static struct cam_cpas_intf *g_cpas_intf;

const char *cam_cpas_axi_util_path_type_to_string(
	uint32_t path_data_type)
{
	switch (path_data_type) {
	/* IFE Paths */
	case CAM_AXI_PATH_DATA_IFE_LINEAR:
		return "IFE_LINEAR";
	case CAM_AXI_PATH_DATA_IFE_VID:
		return "IFE_VID";
	case CAM_AXI_PATH_DATA_IFE_DISP:
		return "IFE_DISP";
	case CAM_AXI_PATH_DATA_IFE_STATS:
		return "IFE_STATS";
	case CAM_AXI_PATH_DATA_IFE_RDI0:
		return "IFE_RDI0";
	case CAM_AXI_PATH_DATA_IFE_RDI1:
		return "IFE_RDI1";
	case CAM_AXI_PATH_DATA_IFE_RDI2:
		return "IFE_RDI2";
	case CAM_AXI_PATH_DATA_IFE_RDI3:
		return "IFE_RDI3";
	case CAM_AXI_PATH_DATA_IFE_PDAF:
		return "IFE_PDAF";
	case CAM_AXI_PATH_DATA_IFE_PIXEL_RAW:
		return "IFE_PIXEL_RAW";

	/* IPE Paths */
	case CAM_AXI_PATH_DATA_IPE_RD_IN:
		return "IPE_RD_IN";
	case CAM_AXI_PATH_DATA_IPE_RD_REF:
		return "IPE_RD_REF";
	case CAM_AXI_PATH_DATA_IPE_WR_VID:
		return "IPE_WR_VID";
	case CAM_AXI_PATH_DATA_IPE_WR_DISP:
		return "IPE_WR_DISP";
	case CAM_AXI_PATH_DATA_IPE_WR_REF:
		return "IPE_WR_REF";

	/* OPE Paths */
	case CAM_AXI_PATH_DATA_OPE_RD_IN:
		return "OPE_RD_IN";
	case CAM_AXI_PATH_DATA_OPE_RD_REF:
		return "OPE_RD_REF";
	case CAM_AXI_PATH_DATA_OPE_WR_VID:
		return "OPE_WR_VID";
	case CAM_AXI_PATH_DATA_OPE_WR_DISP:
		return "OPE_WR_DISP";
	case CAM_AXI_PATH_DATA_OPE_WR_REF:
		return "OPE_WR_REF";

	/* SFE Paths */
	case CAM_AXI_PATH_DATA_SFE_NRDI:
		return "SFE_NRDI";
	case CAM_AXI_PATH_DATA_SFE_RDI0:
		return "IFE_RDI0";
	case CAM_AXI_PATH_DATA_SFE_RDI1:
		return "IFE_RDI1";
	case CAM_AXI_PATH_DATA_SFE_RDI2:
		return "IFE_RDI2";
	case CAM_AXI_PATH_DATA_SFE_RDI3:
		return "IFE_RDI3";
	case CAM_AXI_PATH_DATA_SFE_RDI4:
		return "IFE_RDI4";
	case CAM_AXI_PATH_DATA_SFE_STATS:
		return "SFE_STATS";

	/* Common Paths */
	case CAM_AXI_PATH_DATA_ALL:
		return "DATA_ALL";
	default:
		return "IFE_PATH_INVALID";
	}
}
EXPORT_SYMBOL(cam_cpas_axi_util_path_type_to_string);

const char *cam_cpas_axi_util_trans_type_to_string(
	uint32_t transac_type)
{
	switch (transac_type) {
	case CAM_AXI_TRANSACTION_READ:
		return "TRANSAC_READ";
	case CAM_AXI_TRANSACTION_WRITE:
		return "TRANSAC_WRITE";
	default:
		return "TRANSAC_INVALID";
	}
}
EXPORT_SYMBOL(cam_cpas_axi_util_trans_type_to_string);

bool cam_cpas_is_feature_supported(uint32_t flag, uint32_t hw_map,
	uint32_t *fuse_val)
{
	struct cam_hw_info *cpas_hw = NULL;
	struct cam_cpas_private_soc *soc_private = NULL;
	bool supported = true;
	int32_t i;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return false;
	}

	cpas_hw = (struct cam_hw_info *) g_cpas_intf->hw_intf->hw_priv;
	soc_private =
		(struct cam_cpas_private_soc *)cpas_hw->soc_info.soc_private;

	if (flag >= CAM_CPAS_FUSE_FEATURE_MAX) {
		CAM_ERR(CAM_CPAS, "Unknown feature flag %x", flag);
		return false;
	}

	for (i = 0; i < soc_private->num_feature_info; i++)
		if (soc_private->feature_info[i].feature == flag)
			break;

	if (i == soc_private->num_feature_info)
		goto end;

	if (soc_private->feature_info[i].type == CAM_CPAS_FEATURE_TYPE_DISABLE
		|| (soc_private->feature_info[i].type ==
		CAM_CPAS_FEATURE_TYPE_ENABLE)) {
		if ((soc_private->feature_info[i].hw_map & hw_map) == hw_map)
			supported = soc_private->feature_info[i].enable;
	} else {
		if (!fuse_val) {
			CAM_ERR(CAM_CPAS,
				"Invalid arg fuse_val");
		} else {
			*fuse_val = soc_private->feature_info[i].value;
		}
	}

end:
	return supported;
}
EXPORT_SYMBOL(cam_cpas_is_feature_supported);

int cam_cpas_get_cpas_hw_version(uint32_t *hw_version)
{
	struct cam_hw_info *cpas_hw = NULL;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (!hw_version) {
		CAM_ERR(CAM_CPAS, "invalid input %pK", hw_version);
		return -EINVAL;
	}

	cpas_hw = (struct cam_hw_info  *) g_cpas_intf->hw_intf->hw_priv;

	*hw_version = cpas_hw->soc_info.hw_version;

	if (*hw_version == CAM_CPAS_TITAN_NONE) {
		CAM_DBG(CAM_CPAS, "Didn't find a valid HW Version %d",
			*hw_version);
	}

	return 0;
}

int cam_cpas_get_camnoc_fifo_fill_level_info(
	uint32_t cpas_version,
	uint32_t client_handle)
{
	int rc = 0;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	rc = cam_cpas_hw_get_camnoc_fill_level_info(cpas_version,
		client_handle);
	if (rc)
		CAM_ERR(CAM_CPAS, "Failed to dump fifo reg rc %d", rc);

	return rc;
}

int cam_cpas_get_hw_info(uint32_t *camera_family,
	struct cam_hw_version *camera_version,
	struct cam_hw_version *cpas_version,
	uint32_t *cam_caps,
	struct cam_cpas_fuse_info *cam_fuse_info)
{
	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (!camera_family || !camera_version || !cpas_version || !cam_caps) {
		CAM_ERR(CAM_CPAS, "invalid input %pK %pK %pK %pK",
			camera_family, camera_version, cpas_version, cam_caps);
		return -EINVAL;
	}

	*camera_family  = g_cpas_intf->hw_caps.camera_family;
	*camera_version = g_cpas_intf->hw_caps.camera_version;
	*cpas_version   = g_cpas_intf->hw_caps.cpas_version;
	*cam_caps       = g_cpas_intf->hw_caps.camera_capability;
	if (cam_fuse_info)
		*cam_fuse_info  = g_cpas_intf->hw_caps.fuse_info;

	CAM_DBG(CAM_CPAS, "Family %d, version %d.%d cam_caps %d",
		*camera_family, camera_version->major,
		camera_version->minor, *cam_caps);

	return 0;
}
EXPORT_SYMBOL(cam_cpas_get_hw_info);

int cam_cpas_reg_write(uint32_t client_handle,
	enum cam_cpas_reg_base reg_base, uint32_t offset, bool mb,
	uint32_t value)
{
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (g_cpas_intf->hw_intf->hw_ops.process_cmd) {
		struct cam_cpas_hw_cmd_reg_read_write cmd_reg_write;

		cmd_reg_write.client_handle = client_handle;
		cmd_reg_write.reg_base = reg_base;
		cmd_reg_write.offset = offset;
		cmd_reg_write.value = value;
		cmd_reg_write.mb = mb;

		rc = g_cpas_intf->hw_intf->hw_ops.process_cmd(
			g_cpas_intf->hw_intf->hw_priv,
			CAM_CPAS_HW_CMD_REG_WRITE, &cmd_reg_write,
			sizeof(struct cam_cpas_hw_cmd_reg_read_write));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in process_cmd, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid process_cmd ops");
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_reg_write);

int cam_cpas_reg_read(uint32_t client_handle,
	enum cam_cpas_reg_base reg_base, uint32_t offset, bool mb,
	uint32_t *value)
{
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (!value) {
		CAM_ERR(CAM_CPAS, "Invalid arg value");
		return -EINVAL;
	}

	if (g_cpas_intf->hw_intf->hw_ops.process_cmd) {
		struct cam_cpas_hw_cmd_reg_read_write cmd_reg_read;

		cmd_reg_read.client_handle = client_handle;
		cmd_reg_read.reg_base = reg_base;
		cmd_reg_read.offset = offset;
		cmd_reg_read.mb = mb;
		cmd_reg_read.value = 0;

		rc = g_cpas_intf->hw_intf->hw_ops.process_cmd(
			g_cpas_intf->hw_intf->hw_priv,
			CAM_CPAS_HW_CMD_REG_READ, &cmd_reg_read,
			sizeof(struct cam_cpas_hw_cmd_reg_read_write));
		if (rc) {
			CAM_ERR(CAM_CPAS, "Failed in process_cmd, rc=%d", rc);
			return rc;
		}

		*value = cmd_reg_read.value;
	} else {
		CAM_ERR(CAM_CPAS, "Invalid process_cmd ops");
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_reg_read);

int cam_cpas_update_axi_vote(uint32_t client_handle,
	struct cam_axi_vote *axi_vote)
{
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (!axi_vote) {
		CAM_ERR(CAM_CPAS, "NULL axi vote");
		return -EINVAL;
	}

	if (g_cpas_intf->hw_intf->hw_ops.process_cmd) {
		struct cam_cpas_hw_cmd_axi_vote cmd_axi_vote;

		cmd_axi_vote.client_handle = client_handle;
		cmd_axi_vote.axi_vote = axi_vote;

		rc = g_cpas_intf->hw_intf->hw_ops.process_cmd(
			g_cpas_intf->hw_intf->hw_priv,
			CAM_CPAS_HW_CMD_AXI_VOTE, &cmd_axi_vote,
			sizeof(struct cam_cpas_hw_cmd_axi_vote));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in process_cmd, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid process_cmd ops");
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_update_axi_vote);

int cam_cpas_update_ahb_vote(uint32_t client_handle,
	struct cam_ahb_vote *ahb_vote)
{
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (g_cpas_intf->hw_intf->hw_ops.process_cmd) {
		struct cam_cpas_hw_cmd_ahb_vote cmd_ahb_vote;

		cmd_ahb_vote.client_handle = client_handle;
		cmd_ahb_vote.ahb_vote = ahb_vote;

		rc = g_cpas_intf->hw_intf->hw_ops.process_cmd(
			g_cpas_intf->hw_intf->hw_priv,
			CAM_CPAS_HW_CMD_AHB_VOTE, &cmd_ahb_vote,
			sizeof(struct cam_cpas_hw_cmd_ahb_vote));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in process_cmd, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid process_cmd ops");
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_update_ahb_vote);

int cam_cpas_stop(uint32_t client_handle)
{
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (g_cpas_intf->hw_intf->hw_ops.stop) {
		struct cam_cpas_hw_cmd_stop cmd_hw_stop;

		cmd_hw_stop.client_handle = client_handle;

		rc = g_cpas_intf->hw_intf->hw_ops.stop(
			g_cpas_intf->hw_intf->hw_priv, &cmd_hw_stop,
			sizeof(struct cam_cpas_hw_cmd_stop));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in stop, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid stop ops");
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_stop);

int cam_cpas_start(uint32_t client_handle,
	struct cam_ahb_vote *ahb_vote, struct cam_axi_vote *axi_vote)
{
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (!axi_vote) {
		CAM_ERR(CAM_CPAS, "NULL axi vote");
		return -EINVAL;
	}

	if (g_cpas_intf->hw_intf->hw_ops.start) {
		struct cam_cpas_hw_cmd_start cmd_hw_start;

		cmd_hw_start.client_handle = client_handle;
		cmd_hw_start.ahb_vote = ahb_vote;
		cmd_hw_start.axi_vote = axi_vote;

		rc = g_cpas_intf->hw_intf->hw_ops.start(
			g_cpas_intf->hw_intf->hw_priv, &cmd_hw_start,
			sizeof(struct cam_cpas_hw_cmd_start));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in start, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid start ops");
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_start);

void cam_cpas_log_votes(void)
{
	uint32_t dummy_args;
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return;
	}

	if (g_cpas_intf->hw_intf->hw_ops.process_cmd) {
		rc = g_cpas_intf->hw_intf->hw_ops.process_cmd(
			g_cpas_intf->hw_intf->hw_priv,
			CAM_CPAS_HW_CMD_LOG_VOTE, &dummy_args,
			sizeof(dummy_args));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in process_cmd, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid process_cmd ops");
	}

}
EXPORT_SYMBOL(cam_cpas_log_votes);

int cam_cpas_select_qos_settings(uint32_t selection_mask)
{
	int rc = 0;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -EBADR;
	}

	if (g_cpas_intf->hw_intf->hw_ops.process_cmd) {
		rc = g_cpas_intf->hw_intf->hw_ops.process_cmd(
			g_cpas_intf->hw_intf->hw_priv,
			CAM_CPAS_HW_CMD_SELECT_QOS, &selection_mask,
			sizeof(selection_mask));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in process_cmd, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid process_cmd ops");
		rc = -EBADR;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_select_qos_settings);

int cam_cpas_notify_event(const char *identifier_string,
	int32_t identifier_value)
{
	int rc = 0;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -EBADR;
	}

	if (g_cpas_intf->hw_intf->hw_ops.process_cmd) {
		struct cam_cpas_hw_cmd_notify_event event = { 0 };

		event.identifier_string = identifier_string;
		event.identifier_value = identifier_value;

		rc = g_cpas_intf->hw_intf->hw_ops.process_cmd(
			g_cpas_intf->hw_intf->hw_priv,
			CAM_CPAS_HW_CMD_LOG_EVENT, &event,
			sizeof(event));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in process_cmd, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid process_cmd ops");
		rc = -EBADR;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_notify_event);

int cam_cpas_unregister_client(uint32_t client_handle)
{
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (g_cpas_intf->hw_intf->hw_ops.process_cmd) {
		rc = g_cpas_intf->hw_intf->hw_ops.process_cmd(
			g_cpas_intf->hw_intf->hw_priv,
			CAM_CPAS_HW_CMD_UNREGISTER_CLIENT,
			&client_handle, sizeof(uint32_t));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in process_cmd, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid process_cmd ops");
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_unregister_client);

int cam_cpas_register_client(
	struct cam_cpas_register_params *register_params)
{
	int rc;

	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return -ENODEV;
	}

	if (g_cpas_intf->hw_intf->hw_ops.process_cmd) {
		rc = g_cpas_intf->hw_intf->hw_ops.process_cmd(
			g_cpas_intf->hw_intf->hw_priv,
			CAM_CPAS_HW_CMD_REGISTER_CLIENT, register_params,
			sizeof(struct cam_cpas_register_params));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in process_cmd, rc=%d", rc);
	} else {
		CAM_ERR(CAM_CPAS, "Invalid process_cmd ops");
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(cam_cpas_register_client);

int cam_cpas_subdev_cmd(struct cam_cpas_intf *cpas_intf,
	struct cam_control *cmd)
{
	int rc = 0;

	if (!cmd) {
		CAM_ERR(CAM_CPAS, "Invalid input cmd");
		return -EINVAL;
	}

	switch (cmd->op_code) {
	case CAM_QUERY_CAP: {
		struct cam_cpas_query_cap query;

		rc = copy_from_user(&query, u64_to_user_ptr(cmd->handle),
			sizeof(query));
		if (rc) {
			CAM_ERR(CAM_CPAS, "Failed in copy from user, rc=%d",
				rc);
			break;
		}

		rc = cam_cpas_get_hw_info(&query.camera_family,
			&query.camera_version, &query.cpas_version,
			&query.reserved, NULL);
		if (rc)
			break;

		rc = copy_to_user(u64_to_user_ptr(cmd->handle), &query,
			sizeof(query));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in copy to user, rc=%d", rc);

		break;
	}
	case CAM_QUERY_CAP_V2: {
		struct cam_cpas_query_cap_v2 query;

		rc = copy_from_user(&query, u64_to_user_ptr(cmd->handle),
			sizeof(query));
		if (rc) {
			CAM_ERR(CAM_CPAS, "Failed in copy from user, rc=%d",
				rc);
			break;
		}

		rc = cam_cpas_get_hw_info(&query.camera_family,
			&query.camera_version, &query.cpas_version,
			&query.reserved,
			&query.fuse_info);
		if (rc)
			break;

		rc = copy_to_user(u64_to_user_ptr(cmd->handle), &query,
			sizeof(query));
		if (rc)
			CAM_ERR(CAM_CPAS, "Failed in copy to user, rc=%d", rc);

		break;
	}
	case CAM_SD_SHUTDOWN:
		break;
	default:
		CAM_ERR(CAM_CPAS, "Unknown op code %d for CPAS", cmd->op_code);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int cam_cpas_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_cpas_intf *cpas_intf = v4l2_get_subdevdata(sd);

	if (!cpas_intf || !cpas_intf->probe_done) {
		CAM_ERR(CAM_CPAS, "CPAS not initialized");
		return -ENODEV;
	}

	mutex_lock(&cpas_intf->intf_lock);
	cpas_intf->open_cnt++;
	CAM_DBG(CAM_CPAS, "CPAS Subdev open count %d", cpas_intf->open_cnt);
	mutex_unlock(&cpas_intf->intf_lock);

	return 0;
}

static int __cam_cpas_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_cpas_intf *cpas_intf = v4l2_get_subdevdata(sd);

	if (!cpas_intf || !cpas_intf->probe_done) {
		CAM_ERR(CAM_CPAS, "CPAS not initialized");
		return -ENODEV;
	}

	mutex_lock(&cpas_intf->intf_lock);
	if (cpas_intf->open_cnt <= 0) {
		CAM_WARN(CAM_CPAS, "device already closed, open_cnt: %d", cpas_intf->open_cnt);
		mutex_unlock(&cpas_intf->intf_lock);
		return 0;
	}
	cpas_intf->open_cnt--;
	CAM_DBG(CAM_CPAS, "CPAS Subdev close count %d", cpas_intf->open_cnt);
	mutex_unlock(&cpas_intf->intf_lock);

	return 0;
}

static int cam_cpas_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	bool crm_active = cam_req_mgr_is_open(CAM_CPAS);

	if (crm_active) {
		CAM_DBG(CAM_CPAS, "CRM is ACTIVE, close should be from CRM");
		return 0;
	}

	return __cam_cpas_subdev_close(sd, fh);
}

static long cam_cpas_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int32_t rc;
	struct cam_cpas_intf *cpas_intf = v4l2_get_subdevdata(sd);

	if (!cpas_intf || !cpas_intf->probe_done) {
		CAM_ERR(CAM_CPAS, "CPAS not initialized");
		return -ENODEV;
	}

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_cpas_subdev_cmd(cpas_intf, (struct cam_control *) arg);
		break;
	case CAM_SD_SHUTDOWN:
		rc = __cam_cpas_subdev_close(sd, NULL);
		break;
	default:
		CAM_ERR(CAM_CPAS, "Invalid command %d for CPAS!", cmd);
		rc = -EINVAL;
		break;
	}

	return rc;
}

#ifdef CONFIG_COMPAT
static long cam_cpas_subdev_compat_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	struct cam_control cmd_data;
	int32_t rc;
	struct cam_cpas_intf *cpas_intf = v4l2_get_subdevdata(sd);

	if (!cpas_intf || !cpas_intf->probe_done) {
		CAM_ERR(CAM_CPAS, "CPAS not initialized");
		return -ENODEV;
	}

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_ERR(CAM_CPAS, "Failed to copy from user_ptr=%pK size=%zu",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_cpas_subdev_cmd(cpas_intf, &cmd_data);
		break;
	default:
		CAM_ERR(CAM_CPAS, "Invalid command %d for CPAS!", cmd);
		rc = -EINVAL;
		break;
	}

	if (!rc) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			CAM_ERR(CAM_CPAS,
				"Failed to copy to user_ptr=%pK size=%zu",
				(void __user *)arg, sizeof(cmd_data));
			rc = -EFAULT;
		}
	}

	return rc;
}
#endif

static struct v4l2_subdev_core_ops cpas_subdev_core_ops = {
	.ioctl = cam_cpas_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_cpas_subdev_compat_ioctl,
#endif
};

static const struct v4l2_subdev_ops cpas_subdev_ops = {
	.core = &cpas_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops cpas_subdev_intern_ops = {
	.open = cam_cpas_subdev_open,
	.close = cam_cpas_subdev_close,
};

static int cam_cpas_subdev_register(struct platform_device *pdev)
{
	int rc;
	struct cam_subdev *subdev;

	if (!g_cpas_intf)
		return -EINVAL;

	subdev = &g_cpas_intf->subdev;

	subdev->name = CAM_CPAS_DEV_NAME;
	subdev->pdev = pdev;
	subdev->ops = &cpas_subdev_ops;
	subdev->internal_ops = &cpas_subdev_intern_ops;
	subdev->token = g_cpas_intf;
	subdev->sd_flags =
		V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;
	subdev->ent_function = CAM_CPAS_DEVICE_TYPE;
	subdev->close_seq_prior = CAM_SD_CLOSE_LOW_PRIORITY;

	rc = cam_register_subdev(subdev);
	if (rc) {
		CAM_ERR(CAM_CPAS, "failed register subdev: %s!",
			CAM_CPAS_DEV_NAME);
		return rc;
	}

	platform_set_drvdata(g_cpas_intf->pdev, g_cpas_intf);
	return rc;
}

static int cam_cpas_dev_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_cpas_hw_caps *hw_caps;
	struct cam_hw_intf *hw_intf;
	int rc;
	struct platform_device *pdev = to_platform_device(dev);

	if (g_cpas_intf) {
		CAM_ERR(CAM_CPAS, "cpas component already binded");
		return -EALREADY;
	}

	g_cpas_intf = kzalloc(sizeof(*g_cpas_intf), GFP_KERNEL);
	if (!g_cpas_intf)
		return -ENOMEM;

	mutex_init(&g_cpas_intf->intf_lock);
	g_cpas_intf->pdev = pdev;

	rc = cam_cpas_hw_probe(pdev, &g_cpas_intf->hw_intf);
	if (rc || (g_cpas_intf->hw_intf == NULL)) {
		CAM_ERR(CAM_CPAS, "Failed in hw probe, rc=%d", rc);
		goto error_destroy_mem;
	}

	hw_intf = g_cpas_intf->hw_intf;
	hw_caps = &g_cpas_intf->hw_caps;

	if (hw_intf->hw_ops.get_hw_caps) {
		rc = hw_intf->hw_ops.get_hw_caps(hw_intf->hw_priv,
			hw_caps, sizeof(struct cam_cpas_hw_caps));
		if (rc) {
			CAM_ERR(CAM_CPAS, "Failed in get_hw_caps, rc=%d", rc);
			goto error_hw_remove;
		}
	} else {
		CAM_ERR(CAM_CPAS, "Invalid get_hw_caps ops");
		goto error_hw_remove;
	}

	rc = cam_cpas_subdev_register(pdev);
	if (rc)
		goto error_hw_remove;

	g_cpas_intf->probe_done = true;
	CAM_DBG(CAM_CPAS,
		"Component bound successfully %d, %d.%d.%d, %d.%d.%d, 0x%x",
		hw_caps->camera_family, hw_caps->camera_version.major,
		hw_caps->camera_version.minor, hw_caps->camera_version.incr,
		hw_caps->cpas_version.major, hw_caps->cpas_version.minor,
		hw_caps->cpas_version.incr, hw_caps->camera_capability);

	return rc;

error_hw_remove:
	cam_cpas_hw_remove(g_cpas_intf->hw_intf);
error_destroy_mem:
	mutex_destroy(&g_cpas_intf->intf_lock);
	kfree(g_cpas_intf);
	g_cpas_intf = NULL;
	CAM_ERR(CAM_CPAS, "CPAS component bind failed");
	return rc;
}

static void cam_cpas_dev_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	if (!CAM_CPAS_INTF_INITIALIZED()) {
		CAM_ERR(CAM_CPAS, "cpas intf not initialized");
		return;
	}

	mutex_lock(&g_cpas_intf->intf_lock);
	g_cpas_intf->probe_done = false;
	cam_unregister_subdev(&g_cpas_intf->subdev);
	cam_cpas_hw_remove(g_cpas_intf->hw_intf);
	mutex_unlock(&g_cpas_intf->intf_lock);
	mutex_destroy(&g_cpas_intf->intf_lock);
	kfree(g_cpas_intf);
	g_cpas_intf = NULL;
}

const static struct component_ops cam_cpas_dev_component_ops = {
	.bind = cam_cpas_dev_component_bind,
	.unbind = cam_cpas_dev_component_unbind,
};

static int cam_cpas_dev_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_CPAS, "Adding CPAS INTF component");
	rc = component_add(&pdev->dev, &cam_cpas_dev_component_ops);
	if (rc)
		CAM_ERR(CAM_CPAS, "failed to add component rc: %d", rc);

	return rc;
}

static int cam_cpas_dev_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_cpas_dev_component_ops);
	return 0;
}

static const struct of_device_id cam_cpas_dt_match[] = {
	{.compatible = "qcom,cam-cpas"},
	{}
};

struct platform_driver cam_cpas_driver = {
	.probe = cam_cpas_dev_probe,
	.remove = cam_cpas_dev_remove,
	.driver = {
		.name = CAM_CPAS_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = cam_cpas_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_cpas_dev_init_module(void)
{
	return platform_driver_register(&cam_cpas_driver);
}

void cam_cpas_dev_exit_module(void)
{
	platform_driver_unregister(&cam_cpas_driver);
}

MODULE_DESCRIPTION("MSM CPAS driver");
MODULE_LICENSE("GPL v2");
