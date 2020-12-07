// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/kernel.h>

#include "cam_soc_util.h"
#include "cam_smmu_api.h"
#include "cam_io_util.h"
#include "cam_cdm_intf_api.h"
#include "cam_cdm.h"
#include "cam_cdm_soc.h"
#include "cam_cdm_core_common.h"

static void cam_cdm_get_client_refcount(struct cam_cdm_client *client)
{
	mutex_lock(&client->lock);
	CAM_DBG(CAM_CDM, "CDM client get refcount=%d",
		client->refcount);
	client->refcount++;
	mutex_unlock(&client->lock);
}

static void cam_cdm_put_client_refcount(struct cam_cdm_client *client)
{
	mutex_lock(&client->lock);
	CAM_DBG(CAM_CDM, "CDM client put refcount=%d",
		client->refcount);
	if (client->refcount > 0) {
		client->refcount--;
	} else {
		CAM_ERR(CAM_CDM, "Refcount put when zero");
		WARN_ON(1);
	}
	mutex_unlock(&client->lock);
}

bool cam_cdm_set_cam_hw_version(
	uint32_t ver, struct cam_hw_version *cam_version)
{
	switch (ver) {
	case CAM_CDM170_VERSION:
	case CAM_CDM175_VERSION:
	case CAM_CDM480_VERSION:
		cam_version->major    = (ver & 0xF0000000);
		cam_version->minor    = (ver & 0xFFF0000);
		cam_version->incr     = (ver & 0xFFFF);
		cam_version->reserved = 0;
		return true;
	default:
		CAM_ERR(CAM_CDM, "CDM Version=%x not supported in util", ver);
	break;
	}
	return false;
}

bool cam_cdm_cpas_cb(uint32_t client_handle, void *userdata,
	struct cam_cpas_irq_data *irq_data)
{
	if (!irq_data)
		return false;

	CAM_DBG(CAM_CDM, "CPAS error callback type=%d", irq_data->irq_type);

	return false;
}

struct cam_cdm_utils_ops *cam_cdm_get_ops(
	uint32_t ver, struct cam_hw_version *cam_version, bool by_cam_version)
{
	if (by_cam_version == false) {
		switch (ver) {
		case CAM_CDM170_VERSION:
		case CAM_CDM175_VERSION:
		case CAM_CDM480_VERSION:
			return &CDM170_ops;
		default:
			CAM_ERR(CAM_CDM, "CDM Version=%x not supported in util",
				ver);
		}
	} else if (cam_version) {
		if (((cam_version->major == 1) &&
			(cam_version->minor == 0) &&
			(cam_version->incr == 0)) ||
			((cam_version->major == 1) &&
			(cam_version->minor == 1) &&
			(cam_version->incr == 0)) ||
			((cam_version->major == 1) &&
			(cam_version->minor == 2) &&
			(cam_version->incr == 0))) {

			CAM_DBG(CAM_CDM,
				"cam_hw_version=%x:%x:%x supported",
				cam_version->major, cam_version->minor,
				cam_version->incr);
			return &CDM170_ops;
		}

		CAM_ERR(CAM_CDM, "cam_hw_version=%x:%x:%x not supported",
			cam_version->major, cam_version->minor,
			cam_version->incr);
	}

	return NULL;
}

struct cam_cdm_bl_cb_request_entry *cam_cdm_find_request_by_bl_tag(
	uint32_t tag, struct list_head *bl_list)
{
	struct cam_cdm_bl_cb_request_entry *node;

	list_for_each_entry(node, bl_list, entry) {
		if (node->bl_tag == tag)
			return node;
	}
	CAM_ERR(CAM_CDM, "Could not find the bl request for tag=%x", tag);

	return NULL;
}

int cam_cdm_get_caps(void *hw_priv,
	void *get_hw_cap_args, uint32_t arg_size)
{
	struct cam_hw_info *cdm_hw = hw_priv;
	struct cam_cdm *cdm_core;

	if ((cdm_hw) && (cdm_hw->core_info) && (get_hw_cap_args) &&
		(sizeof(struct cam_iommu_handle) == arg_size)) {
		cdm_core = (struct cam_cdm *)cdm_hw->core_info;
		*((struct cam_iommu_handle *)get_hw_cap_args) =
			cdm_core->iommu_hdl;
		return 0;
	}

	return -EINVAL;
}

int cam_cdm_find_free_client_slot(struct cam_cdm *hw)
{
	int i;

	for (i = 0; i < CAM_PER_CDM_MAX_REGISTERED_CLIENTS; i++) {
		if (hw->clients[i] == NULL) {
			CAM_DBG(CAM_CDM, "Found client slot %d", i);
			return i;
		}
	}
	CAM_ERR(CAM_CDM, "No more client slots");

	return -EBUSY;
}


void cam_cdm_notify_clients(struct cam_hw_info *cdm_hw,
	enum cam_cdm_cb_status status, void *data)
{
	int i;
	struct cam_cdm *core = NULL;
	struct cam_cdm_client *client = NULL;

	if (!cdm_hw) {
		CAM_ERR(CAM_CDM, "CDM Notify called with NULL hw info");
		return;
	}
	core = (struct cam_cdm *)cdm_hw->core_info;

	if (status == CAM_CDM_CB_STATUS_BL_SUCCESS) {
		int client_idx;
		struct cam_cdm_bl_cb_request_entry *node =
			(struct cam_cdm_bl_cb_request_entry *)data;

		client_idx = CAM_CDM_GET_CLIENT_IDX(node->client_hdl);
		client = core->clients[client_idx];
		if ((!client) || (client->handle != node->client_hdl)) {
			CAM_ERR(CAM_CDM, "Invalid client %pK hdl=%x", client,
				node->client_hdl);
			return;
		}
		cam_cdm_get_client_refcount(client);
		if (client->data.cam_cdm_callback) {
			CAM_DBG(CAM_CDM, "Calling client=%s cb cookie=%d",
				client->data.identifier, node->cookie);
			client->data.cam_cdm_callback(node->client_hdl,
				node->userdata, CAM_CDM_CB_STATUS_BL_SUCCESS,
				node->cookie);
			CAM_DBG(CAM_CDM, "Exit client cb cookie=%d",
				node->cookie);
		} else {
			CAM_ERR(CAM_CDM, "No cb registered for client hdl=%x",
				node->client_hdl);
		}
		cam_cdm_put_client_refcount(client);
		return;
	}

	for (i = 0; i < CAM_PER_CDM_MAX_REGISTERED_CLIENTS; i++) {
		if (core->clients[i] != NULL) {
			client = core->clients[i];
			mutex_lock(&client->lock);
			CAM_DBG(CAM_CDM, "Found client slot %d", i);
			if (client->data.cam_cdm_callback) {
				if (status == CAM_CDM_CB_STATUS_PAGEFAULT) {
					unsigned long iova =
						(unsigned long)data;

					client->data.cam_cdm_callback(
						client->handle,
						client->data.userdata,
						CAM_CDM_CB_STATUS_PAGEFAULT,
						(iova & 0xFFFFFFFF));
				}
			} else {
				CAM_ERR(CAM_CDM,
					"No cb registered for client hdl=%x",
					client->handle);
			}
			mutex_unlock(&client->lock);
		}
	}
}

int cam_cdm_stream_ops_internal(void *hw_priv,
	void *start_args, bool operation)
{
	struct cam_hw_info *cdm_hw = hw_priv;
	struct cam_cdm *core = NULL;
	int rc = -EPERM;
	int client_idx;
	struct cam_cdm_client *client;
	uint32_t *handle = start_args;

	if (!hw_priv)
		return -EINVAL;

	core = (struct cam_cdm *)cdm_hw->core_info;
	client_idx = CAM_CDM_GET_CLIENT_IDX(*handle);
	client = core->clients[client_idx];
	if (!client) {
		CAM_ERR(CAM_CDM, "Invalid client %pK hdl=%x", client, *handle);
		return -EINVAL;
	}
	cam_cdm_get_client_refcount(client);
	if (*handle != client->handle) {
		CAM_ERR(CAM_CDM, "client id given handle=%x invalid", *handle);
		cam_cdm_put_client_refcount(client);
		return -EINVAL;
	}
	if (operation == true) {
		if (true == client->stream_on) {
			CAM_ERR(CAM_CDM,
				"Invalid CDM client is already streamed ON");
			cam_cdm_put_client_refcount(client);
			return rc;
		}
	} else {
		if (client->stream_on == false) {
			CAM_ERR(CAM_CDM,
				"Invalid CDM client is already streamed Off");
			cam_cdm_put_client_refcount(client);
			return rc;
		}
	}

	mutex_lock(&cdm_hw->hw_mutex);
	if (operation == true) {
		if (!cdm_hw->open_count) {
			struct cam_ahb_vote ahb_vote;
			struct cam_axi_vote axi_vote = {0};

			ahb_vote.type = CAM_VOTE_ABSOLUTE;
			ahb_vote.vote.level = CAM_LOWSVS_VOTE;
			axi_vote.num_paths = 1;
			axi_vote.axi_path[0].path_data_type =
				CAM_AXI_PATH_DATA_ALL;
			axi_vote.axi_path[0].transac_type =
				CAM_AXI_TRANSACTION_READ;
			axi_vote.axi_path[0].camnoc_bw =
				CAM_CPAS_DEFAULT_AXI_BW;
			axi_vote.axi_path[0].mnoc_ab_bw =
				CAM_CPAS_DEFAULT_AXI_BW;
			axi_vote.axi_path[0].mnoc_ib_bw =
				CAM_CPAS_DEFAULT_AXI_BW;

			rc = cam_cpas_start(core->cpas_handle,
				&ahb_vote, &axi_vote);
			if (rc != 0) {
				CAM_ERR(CAM_CDM, "CPAS start failed");
				goto end;
			}
			CAM_DBG(CAM_CDM, "CDM init first time");
			if (core->id == CAM_CDM_VIRTUAL) {
				CAM_DBG(CAM_CDM,
					"Virtual CDM HW init first time");
				rc = 0;
			} else {
				CAM_DBG(CAM_CDM, "CDM HW init first time");
				rc = cam_hw_cdm_init(hw_priv, NULL, 0);
				if (rc == 0) {
					rc = cam_hw_cdm_alloc_genirq_mem(
						hw_priv);
					if (rc != 0) {
						CAM_ERR(CAM_CDM,
							"Genirqalloc failed");
						cam_hw_cdm_deinit(hw_priv,
							NULL, 0);
					}
				} else {
					CAM_ERR(CAM_CDM, "CDM HW init failed");
				}
			}
			if (rc == 0) {
				cdm_hw->open_count++;
				client->stream_on = true;
			} else {
				if (cam_cpas_stop(core->cpas_handle))
					CAM_ERR(CAM_CDM, "CPAS stop failed");
			}
		} else {
			cdm_hw->open_count++;
			CAM_DBG(CAM_CDM, "CDM HW already ON count=%d",
				cdm_hw->open_count);
			rc = 0;
			client->stream_on = true;
		}
	} else {
		if (cdm_hw->open_count) {
			cdm_hw->open_count--;
			CAM_DBG(CAM_CDM, "stream OFF CDM %d",
				cdm_hw->open_count);
			if (!cdm_hw->open_count) {
				CAM_DBG(CAM_CDM, "CDM Deinit now");
				if (core->id == CAM_CDM_VIRTUAL) {
					CAM_DBG(CAM_CDM,
						"Virtual CDM HW Deinit");
					rc = 0;
				} else {
					CAM_DBG(CAM_CDM, "CDM HW Deinit now");
					rc = cam_hw_cdm_deinit(
						hw_priv, NULL, 0);
					if (cam_hw_cdm_release_genirq_mem(
						hw_priv))
						CAM_ERR(CAM_CDM,
							"Genirq release fail");
				}
				if (rc) {
					CAM_ERR(CAM_CDM,
						"Deinit failed in streamoff");
				} else {
					client->stream_on = false;
					rc = cam_cpas_stop(core->cpas_handle);
					if (rc)
						CAM_ERR(CAM_CDM,
							"CPAS stop failed");
				}
			} else {
				client->stream_on = false;
				rc = 0;
				CAM_DBG(CAM_CDM,
					"Client stream off success =%d",
					cdm_hw->open_count);
			}
		} else {
			CAM_DBG(CAM_CDM, "stream OFF CDM Invalid %d",
				cdm_hw->open_count);
			rc = -ENXIO;
		}
	}
end:
	cam_cdm_put_client_refcount(client);
	mutex_unlock(&cdm_hw->hw_mutex);
	return rc;
}

int cam_cdm_stream_start(void *hw_priv,
	void *start_args, uint32_t size)
{
	int rc = 0;

	if (!hw_priv)
		return -EINVAL;

	rc = cam_cdm_stream_ops_internal(hw_priv, start_args, true);
	return rc;

}

int cam_cdm_stream_stop(void *hw_priv,
	void *start_args, uint32_t size)
{
	int rc = 0;

	if (!hw_priv)
		return -EINVAL;

	rc = cam_cdm_stream_ops_internal(hw_priv, start_args, false);
	return rc;

}

int cam_cdm_process_cmd(void *hw_priv,
	uint32_t cmd, void *cmd_args, uint32_t arg_size)
{
	struct cam_hw_info *cdm_hw = hw_priv;
	struct cam_hw_soc_info *soc_data = NULL;
	struct cam_cdm *core = NULL;
	int rc = -EINVAL;

	if ((!hw_priv) || (!cmd_args) ||
		(cmd >= CAM_CDM_HW_INTF_CMD_INVALID))
		return rc;

	soc_data = &cdm_hw->soc_info;
	core = (struct cam_cdm *)cdm_hw->core_info;
	switch (cmd) {
	case CAM_CDM_HW_INTF_CMD_SUBMIT_BL: {
		struct cam_cdm_hw_intf_cmd_submit_bl *req;
		int idx;
		struct cam_cdm_client *client;

		if (sizeof(struct cam_cdm_hw_intf_cmd_submit_bl) != arg_size) {
			CAM_ERR(CAM_CDM, "Invalid CDM cmd %d arg size=%x", cmd,
				arg_size);
			break;
		}
		req = (struct cam_cdm_hw_intf_cmd_submit_bl *)cmd_args;
		if ((req->data->type < 0) ||
			(req->data->type > CAM_CDM_BL_CMD_TYPE_KERNEL_IOVA)) {
			CAM_ERR(CAM_CDM, "Invalid req bl cmd addr type=%d",
				req->data->type);
			break;
		}
		idx = CAM_CDM_GET_CLIENT_IDX(req->handle);
		client = core->clients[idx];
		if ((!client) || (req->handle != client->handle)) {
			CAM_ERR(CAM_CDM, "Invalid client %pK hdl=%x", client,
				req->handle);
			break;
		}
		cam_cdm_get_client_refcount(client);
		if ((req->data->flag == true) &&
			(!client->data.cam_cdm_callback)) {
			CAM_ERR(CAM_CDM,
				"CDM request cb without registering cb");
			cam_cdm_put_client_refcount(client);
			break;
		}
		if (client->stream_on != true) {
			CAM_ERR(CAM_CDM,
				"Invalid CDM needs to be streamed ON first");
			cam_cdm_put_client_refcount(client);
			break;
		}
		if (core->id == CAM_CDM_VIRTUAL)
			rc = cam_virtual_cdm_submit_bl(cdm_hw, req, client);
		else
			rc = cam_hw_cdm_submit_bl(cdm_hw, req, client);

		cam_cdm_put_client_refcount(client);
		break;
	}
	case CAM_CDM_HW_INTF_CMD_ACQUIRE: {
		struct cam_cdm_acquire_data *data;
		int idx;
		struct cam_cdm_client *client;

		if (sizeof(struct cam_cdm_acquire_data) != arg_size) {
			CAM_ERR(CAM_CDM, "Invalid CDM cmd %d arg size=%x", cmd,
				arg_size);
			break;
		}

		mutex_lock(&cdm_hw->hw_mutex);
		data = (struct cam_cdm_acquire_data *)cmd_args;
		CAM_DBG(CAM_CDM, "Trying to acquire client=%s in hw idx=%d",
			data->identifier, core->index);
		idx = cam_cdm_find_free_client_slot(core);
		if ((idx < 0) || (core->clients[idx])) {
			mutex_unlock(&cdm_hw->hw_mutex);
			CAM_ERR(CAM_CDM,
				"Fail to client slots, client=%s in hw idx=%d",
			data->identifier, core->index);
			break;
		}
		core->clients[idx] = kzalloc(sizeof(struct cam_cdm_client),
			GFP_KERNEL);
		if (!core->clients[idx]) {
			mutex_unlock(&cdm_hw->hw_mutex);
			rc = -ENOMEM;
			break;
		}

		mutex_unlock(&cdm_hw->hw_mutex);
		client = core->clients[idx];
		mutex_init(&client->lock);
		data->ops = core->ops;
		if (core->id == CAM_CDM_VIRTUAL) {
			data->cdm_version.major = 1;
			data->cdm_version.minor = 0;
			data->cdm_version.incr = 0;
			data->cdm_version.reserved = 0;
			data->ops = cam_cdm_get_ops(0,
					&data->cdm_version, true);
			if (!data->ops) {
				mutex_destroy(&client->lock);
				mutex_lock(&cdm_hw->hw_mutex);
				kfree(core->clients[idx]);
				core->clients[idx] = NULL;
				mutex_unlock(
					&cdm_hw->hw_mutex);
				rc = -EPERM;
				CAM_ERR(CAM_CDM, "Invalid ops for virtual cdm");
				break;
			}
		} else {
			data->cdm_version = core->version;
		}

		cam_cdm_get_client_refcount(client);
		mutex_lock(&client->lock);
		memcpy(&client->data, data,
			sizeof(struct cam_cdm_acquire_data));
		client->handle = CAM_CDM_CREATE_CLIENT_HANDLE(
					core->index,
					idx);
		client->stream_on = false;
		data->handle = client->handle;
		CAM_DBG(CAM_CDM, "Acquired client=%s in hwidx=%d",
			data->identifier, core->index);
		mutex_unlock(&client->lock);
		rc = 0;
		break;
	}
	case CAM_CDM_HW_INTF_CMD_RELEASE: {
		uint32_t *handle = cmd_args;
		int idx;
		struct cam_cdm_client *client;

		if (sizeof(uint32_t) != arg_size) {
			CAM_ERR(CAM_CDM,
				"Invalid CDM cmd %d size=%x for handle=%x",
				cmd, arg_size, *handle);
			return -EINVAL;
		}
		idx = CAM_CDM_GET_CLIENT_IDX(*handle);
		mutex_lock(&cdm_hw->hw_mutex);
		client = core->clients[idx];
		if ((!client) || (*handle != client->handle)) {
			CAM_ERR(CAM_CDM, "Invalid client %pK hdl=%x",
				client, *handle);
			mutex_unlock(&cdm_hw->hw_mutex);
			break;
		}
		cam_cdm_put_client_refcount(client);
		mutex_lock(&client->lock);
		if (client->refcount != 0) {
			CAM_ERR(CAM_CDM, "CDM Client refcount not zero %d",
				client->refcount);
			rc = -EPERM;
			mutex_unlock(&client->lock);
			mutex_unlock(&cdm_hw->hw_mutex);
			break;
		}
		core->clients[idx] = NULL;
		mutex_unlock(&client->lock);
		mutex_destroy(&client->lock);
		kfree(client);
		mutex_unlock(&cdm_hw->hw_mutex);
		rc = 0;
		break;
	}
	case CAM_CDM_HW_INTF_CMD_RESET_HW: {
		CAM_ERR(CAM_CDM, "CDM HW reset not supported for handle =%x",
			*((uint32_t *)cmd_args));
		break;
	}
	case CAM_CDM_HW_INTF_CMD_HANG_DETECT: {
		uint32_t *handle = cmd_args;
		int idx;
		struct cam_cdm_client *client;

		if (sizeof(uint32_t) != arg_size) {
			CAM_ERR(CAM_CDM,
				"Invalid CDM cmd %d size=%x for handle=%x",
				cmd, arg_size, *handle);
				return -EINVAL;
		}

		idx = CAM_CDM_GET_CLIENT_IDX(*handle);
		mutex_lock(&cdm_hw->hw_mutex);
		client = core->clients[idx];
		if ((!client) || (*handle != client->handle)) {
			CAM_ERR(CAM_CDM, "Invalid client %pK hdl=%x",
				client, *handle);
			mutex_unlock(&cdm_hw->hw_mutex);
			break;
		}
		rc = cam_hw_cdm_hang_detect(cdm_hw, *handle);
		mutex_unlock(&cdm_hw->hw_mutex);
		break;
	}
	default:
		CAM_ERR(CAM_CDM, "CDM HW intf command not valid =%d", cmd);
		break;
	}
	return rc;
}
