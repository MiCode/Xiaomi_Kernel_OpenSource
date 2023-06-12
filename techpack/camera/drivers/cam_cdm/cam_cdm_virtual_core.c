// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021,  The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/kernel.h>

#include "cam_soc_util.h"
#include "cam_smmu_api.h"
#include "cam_cdm_intf_api.h"
#include "cam_cdm.h"
#include "cam_cdm_util.h"
#include "cam_cdm_virtual.h"
#include "cam_cdm_core_common.h"
#include "cam_cdm_soc.h"
#include "cam_io_util.h"
#include "cam_req_mgr_workq.h"
#include "cam_common_util.h"

#define CAM_CDM_VIRTUAL_NAME "qcom,cam_virtual_cdm"

static void cam_virtual_cdm_work(struct work_struct *work)
{
	struct cam_cdm_work_payload *payload;
	struct cam_hw_info *cdm_hw;
	struct cam_cdm *core;

	payload = container_of(work, struct cam_cdm_work_payload, work);
	if (payload) {
		cdm_hw = payload->hw;
		core = (struct cam_cdm *)cdm_hw->core_info;

		cam_common_util_thread_switch_delay_detect(
			"Virtual CDM workq schedule",
			payload->workq_scheduled_ts,
			CAM_WORKQ_SCHEDULE_TIME_THRESHOLD);

		if (payload->irq_status & 0x2) {
			struct cam_cdm_bl_cb_request_entry *node;

			CAM_DBG(CAM_CDM, "CDM HW Gen/inline IRQ with data=%x",
				payload->irq_data);
			mutex_lock(&cdm_hw->hw_mutex);
			node = cam_cdm_find_request_by_bl_tag(
				payload->irq_data,
				&core->bl_request_list);
			if (node) {
				if (node->request_type ==
					CAM_HW_CDM_BL_CB_CLIENT) {
					cam_cdm_notify_clients(cdm_hw,
						CAM_CDM_CB_STATUS_BL_SUCCESS,
						(void *)node);
				} else if (node->request_type ==
					CAM_HW_CDM_BL_CB_INTERNAL) {
					CAM_ERR(CAM_CDM, "Invalid node=%pK %d",
						node, node->request_type);
				}
				list_del_init(&node->entry);
				kfree(node);
			} else {
				CAM_ERR(CAM_CDM, "Invalid node for inline irq");
			}
			mutex_unlock(&cdm_hw->hw_mutex);
		}
		if (payload->irq_status & 0x1) {
			CAM_DBG(CAM_CDM, "CDM HW reset done IRQ");
			complete(&core->reset_complete);
		}
		kfree(payload);
	}

}

int cam_virtual_cdm_submit_bl(struct cam_hw_info *cdm_hw,
	struct cam_cdm_hw_intf_cmd_submit_bl *req,
	struct cam_cdm_client *client)
{
	int i, rc = -EINVAL;
	struct cam_cdm_bl_request *cdm_cmd = req->data;
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;

	mutex_lock(&client->lock);
	for (i = 0; i < req->data->cmd_arrary_count ; i++) {
		uintptr_t vaddr_ptr = 0;
		size_t len = 0;

		if ((!cdm_cmd->cmd[i].len) &&
			(cdm_cmd->cmd[i].len > 0x100000)) {
			CAM_ERR(CAM_CDM,
				"len(%d) is invalid count=%d total cnt=%d",
				cdm_cmd->cmd[i].len, i,
				req->data->cmd_arrary_count);
			rc = -EINVAL;
			break;
		}
		if (req->data->type == CAM_CDM_BL_CMD_TYPE_MEM_HANDLE) {
			rc = cam_mem_get_cpu_buf(
				cdm_cmd->cmd[i].bl_addr.mem_handle, &vaddr_ptr,
				&len);
		} else if (req->data->type ==
			CAM_CDM_BL_CMD_TYPE_KERNEL_IOVA) {
			rc = 0;
			vaddr_ptr = cdm_cmd->cmd[i].bl_addr.kernel_iova;
			len = cdm_cmd->cmd[i].offset + cdm_cmd->cmd[i].len;
		} else {
			CAM_ERR(CAM_CDM,
				"Only mem hdl/Kernel va type is supported %d",
				req->data->type);
			rc = -EINVAL;
			break;
		}

		if ((!rc) && (vaddr_ptr) && (len) &&
			(len >= cdm_cmd->cmd[i].offset)) {


			if ((len - cdm_cmd->cmd[i].offset) <
				cdm_cmd->cmd[i].len) {
				CAM_ERR(CAM_CDM, "Not enough buffer");
				rc = -EINVAL;
				break;
			}
			CAM_DBG(CAM_CDM,
				"hdl=%x vaddr=%pK offset=%d cmdlen=%d:%zu",
				cdm_cmd->cmd[i].bl_addr.mem_handle,
				(void *)vaddr_ptr, cdm_cmd->cmd[i].offset,
				cdm_cmd->cmd[i].len, len);
			rc = cam_cdm_util_cmd_buf_write(
				&client->changebase_addr,
				((uint32_t *)vaddr_ptr +
					((cdm_cmd->cmd[i].offset)/4)),
				cdm_cmd->cmd[i].len, client->data.base_array,
				client->data.base_array_cnt, core->bl_tag);
			if (rc) {
				CAM_ERR(CAM_CDM,
					"write failed for cnt=%d:%d len %u",
					i, req->data->cmd_arrary_count,
					cdm_cmd->cmd[i].len);
				break;
			}
		} else {
			CAM_ERR(CAM_CDM,
				"Sanity check failed for hdl=%x len=%zu:%d",
				cdm_cmd->cmd[i].bl_addr.mem_handle, len,
				cdm_cmd->cmd[i].offset);
			CAM_ERR(CAM_CDM,
				"Sanity check failed for cmd_count=%d cnt=%d",
				i, req->data->cmd_arrary_count);
			rc = -EINVAL;
			break;
		}
		if (!rc) {
			struct cam_cdm_work_payload *payload;

			CAM_DBG(CAM_CDM,
				"write BL success for cnt=%d with tag=%d",
				i, core->bl_tag);
			if ((true == req->data->flag) &&
				(i == req->data->cmd_arrary_count)) {
				struct cam_cdm_bl_cb_request_entry *node;

				node = kzalloc(sizeof(
					struct cam_cdm_bl_cb_request_entry),
					GFP_KERNEL);
				if (!node) {
					rc = -ENOMEM;
					break;
				}
				node->request_type = CAM_HW_CDM_BL_CB_CLIENT;
				node->client_hdl = req->handle;
				node->cookie = req->data->cookie;
				node->bl_tag = core->bl_tag;
				node->userdata = req->data->userdata;
				mutex_lock(&cdm_hw->hw_mutex);
				list_add_tail(&node->entry,
					&core->bl_request_list);
				mutex_unlock(&cdm_hw->hw_mutex);

				payload = kzalloc(sizeof(
					struct cam_cdm_work_payload),
					GFP_ATOMIC);
				if (payload) {
					payload->irq_status = 0x2;
					payload->irq_data = core->bl_tag;
					payload->hw = cdm_hw;
					INIT_WORK((struct work_struct *)
						&payload->work,
						cam_virtual_cdm_work);
					payload->workq_scheduled_ts =
						ktime_get();
					queue_work(core->work_queue,
						&payload->work);
				}
			}
			core->bl_tag++;
			CAM_DBG(CAM_CDM,
				"Now commit the BL nothing for virtual");
			if (!rc && (core->bl_tag == 63))
				core->bl_tag = 0;
		}
	}
	mutex_unlock(&client->lock);
	return rc;
}

int cam_virtual_cdm_probe(struct platform_device *pdev)
{
	struct cam_hw_info *cdm_hw = NULL;
	struct cam_hw_intf *cdm_hw_intf = NULL;
	struct cam_cdm *cdm_core = NULL;
	struct cam_cdm_private_dt_data *soc_private = NULL;
	int rc;
	struct cam_cpas_register_params cpas_parms;

	cdm_hw_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!cdm_hw_intf)
		return -ENOMEM;

	cdm_hw = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!cdm_hw) {
		kfree(cdm_hw_intf);
		return -ENOMEM;
	}

	cdm_hw->core_info = kzalloc(sizeof(struct cam_cdm), GFP_KERNEL);
	if (!cdm_hw->core_info) {
		kfree(cdm_hw);
		kfree(cdm_hw_intf);
		return -ENOMEM;
	}
	cdm_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	cdm_hw->soc_info.pdev = pdev;
	cdm_hw_intf->hw_type = CAM_VIRTUAL_CDM;
	cdm_hw->soc_info.soc_private = kzalloc(
			sizeof(struct cam_cdm_private_dt_data), GFP_KERNEL);
	if (!cdm_hw->soc_info.soc_private) {
		rc = -ENOMEM;
		goto soc_load_failed;
	}

	rc = cam_cdm_soc_load_dt_private(pdev, cdm_hw->soc_info.soc_private);
	if (rc) {
		CAM_ERR(CAM_CDM, "Failed to load CDM dt private data");
		kfree(cdm_hw->soc_info.soc_private);
		cdm_hw->soc_info.soc_private = NULL;
		goto soc_load_failed;
	}

	cdm_core = (struct cam_cdm *)cdm_hw->core_info;
	soc_private = (struct cam_cdm_private_dt_data *)
					cdm_hw->soc_info.soc_private;
	if (soc_private->dt_cdm_shared == true)
		cdm_core->flags = CAM_CDM_FLAG_SHARED_CDM;
	else
		cdm_core->flags = CAM_CDM_FLAG_PRIVATE_CDM;

	cdm_core->bl_tag = 0;
	INIT_LIST_HEAD(&cdm_core->bl_request_list);
	init_completion(&cdm_core->reset_complete);
	cdm_hw_intf->hw_priv = cdm_hw;
	cdm_hw_intf->hw_ops.get_hw_caps = cam_cdm_get_caps;
	cdm_hw_intf->hw_ops.init = NULL;
	cdm_hw_intf->hw_ops.deinit = NULL;
	cdm_hw_intf->hw_ops.start = cam_cdm_stream_start;
	cdm_hw_intf->hw_ops.stop = cam_cdm_stream_stop;
	cdm_hw_intf->hw_ops.read = NULL;
	cdm_hw_intf->hw_ops.write = NULL;
	cdm_hw_intf->hw_ops.process_cmd = cam_cdm_process_cmd;

	CAM_DBG(CAM_CDM, "type %d index %d", cdm_hw_intf->hw_type,
		cdm_hw_intf->hw_idx);

	platform_set_drvdata(pdev, cdm_hw_intf);

	cdm_hw->open_count = 0;
	cdm_core->iommu_hdl.non_secure = -1;
	cdm_core->iommu_hdl.secure = -1;
	mutex_init(&cdm_hw->hw_mutex);
	spin_lock_init(&cdm_hw->hw_lock);
	init_completion(&cdm_hw->hw_complete);
	mutex_lock(&cdm_hw->hw_mutex);
	cdm_core->id = CAM_CDM_VIRTUAL;
	memcpy(cdm_core->name, CAM_CDM_VIRTUAL_NAME,
		sizeof(CAM_CDM_VIRTUAL_NAME));
	cdm_core->work_queue = alloc_workqueue(cdm_core->name,
		WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_SYSFS,
		CAM_CDM_INFLIGHT_WORKS);
	cdm_core->ops = NULL;

	cpas_parms.cam_cpas_client_cb = cam_cdm_cpas_cb;
	cpas_parms.cell_index = cdm_hw->soc_info.index;
	cpas_parms.dev = &pdev->dev;
	cpas_parms.userdata = cdm_hw_intf;
	strlcpy(cpas_parms.identifier, "cam-cdm-intf",
		CAM_HW_IDENTIFIER_LENGTH);
	rc = cam_cpas_register_client(&cpas_parms);
	if (rc) {
		CAM_ERR(CAM_CDM, "Virtual CDM CPAS registration failed");
		goto cpas_registration_failed;
	}
	CAM_DBG(CAM_CDM, "CPAS registration successful handle=%d",
		cpas_parms.client_handle);
	cdm_core->cpas_handle = cpas_parms.client_handle;

	CAM_DBG(CAM_CDM, "CDM%d probe successful", cdm_hw_intf->hw_idx);

	rc = cam_cdm_intf_register_hw_cdm(cdm_hw_intf,
			soc_private, CAM_VIRTUAL_CDM, &cdm_core->index);
	if (rc) {
		CAM_ERR(CAM_CDM, "Virtual CDM Interface registration failed");
		goto intf_registration_failed;
	}
	CAM_DBG(CAM_CDM, "CDM%d registered to intf successful",
		cdm_hw_intf->hw_idx);
	mutex_unlock(&cdm_hw->hw_mutex);

	return 0;
intf_registration_failed:
	cam_cpas_unregister_client(cdm_core->cpas_handle);
cpas_registration_failed:
	kfree(cdm_hw->soc_info.soc_private);
	flush_workqueue(cdm_core->work_queue);
	destroy_workqueue(cdm_core->work_queue);
	mutex_unlock(&cdm_hw->hw_mutex);
	mutex_destroy(&cdm_hw->hw_mutex);
soc_load_failed:
	kfree(cdm_hw->core_info);
	kfree(cdm_hw);
	kfree(cdm_hw_intf);
	return rc;
}

int cam_virtual_cdm_remove(struct platform_device *pdev)
{
	struct cam_hw_info *cdm_hw = NULL;
	struct cam_hw_intf *cdm_hw_intf = NULL;
	struct cam_cdm *cdm_core = NULL;
	int rc = -EBUSY;

	cdm_hw_intf = platform_get_drvdata(pdev);
	if (!cdm_hw_intf) {
		CAM_ERR(CAM_CDM, "Failed to get dev private data");
		return rc;
	}

	cdm_hw = cdm_hw_intf->hw_priv;
	if (!cdm_hw) {
		CAM_ERR(CAM_CDM,
			"Failed to get virtual private data for type=%d idx=%d",
			cdm_hw_intf->hw_type, cdm_hw_intf->hw_idx);
		return rc;
	}

	cdm_core = cdm_hw->core_info;
	if (!cdm_core) {
		CAM_ERR(CAM_CDM,
			"Failed to get virtual core data for type=%d idx=%d",
			cdm_hw_intf->hw_type, cdm_hw_intf->hw_idx);
		return rc;
	}

	rc = cam_cpas_unregister_client(cdm_core->cpas_handle);
	if (rc) {
		CAM_ERR(CAM_CDM, "CPAS unregister failed");
		return rc;
	}

	rc = cam_cdm_intf_deregister_hw_cdm(cdm_hw_intf,
			cdm_hw->soc_info.soc_private, CAM_VIRTUAL_CDM,
			cdm_core->index);
	if (rc) {
		CAM_ERR(CAM_CDM,
			"Virtual CDM Interface de-registration failed");
		return rc;
	}

	flush_workqueue(cdm_core->work_queue);
	destroy_workqueue(cdm_core->work_queue);
	mutex_destroy(&cdm_hw->hw_mutex);
	kfree(cdm_hw->soc_info.soc_private);
	kfree(cdm_hw->core_info);
	kfree(cdm_hw);
	kfree(cdm_hw_intf);
	rc = 0;

	return rc;
}
