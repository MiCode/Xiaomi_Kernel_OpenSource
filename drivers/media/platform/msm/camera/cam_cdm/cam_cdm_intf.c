/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/ion.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/kernel.h>

#include "cam_cdm_intf_api.h"
#include "cam_cdm.h"
#include "cam_cdm_virtual.h"
#include "cam_soc_util.h"
#include "cam_cdm_soc.h"

static struct cam_cdm_intf_mgr cdm_mgr;
static DEFINE_MUTEX(cam_cdm_mgr_lock);

static const struct of_device_id msm_cam_cdm_intf_dt_match[] = {
	{ .compatible = "qcom,cam-cdm-intf", },
	{}
};

static int get_cdm_mgr_refcount(void)
{
	int rc = 0;

	mutex_lock(&cam_cdm_mgr_lock);
	if (cdm_mgr.probe_done == false) {
		CAM_ERR(CAM_CDM, "CDM intf mgr not probed yet");
		rc = -EPERM;
	} else {
		CAM_DBG(CAM_CDM, "CDM intf mgr get refcount=%d",
			cdm_mgr.refcount);
		cdm_mgr.refcount++;
	}
	mutex_unlock(&cam_cdm_mgr_lock);
	return rc;
}

static void put_cdm_mgr_refcount(void)
{
	mutex_lock(&cam_cdm_mgr_lock);
	if (cdm_mgr.probe_done == false) {
		CAM_ERR(CAM_CDM, "CDM intf mgr not probed yet");
	} else {
		CAM_DBG(CAM_CDM, "CDM intf mgr put refcount=%d",
			cdm_mgr.refcount);
		if (cdm_mgr.refcount > 0) {
			cdm_mgr.refcount--;
		} else {
			CAM_ERR(CAM_CDM, "Refcount put when zero");
			WARN_ON(1);
		}
	}
	mutex_unlock(&cam_cdm_mgr_lock);
}

static int get_cdm_iommu_handle(struct cam_iommu_handle *cdm_handles,
	uint32_t hw_idx)
{
	int rc = -EPERM;
	struct cam_hw_intf *hw = cdm_mgr.nodes[hw_idx].device;

	if (hw->hw_ops.get_hw_caps) {
		rc = hw->hw_ops.get_hw_caps(hw->hw_priv, cdm_handles,
			sizeof(struct cam_iommu_handle));
	}

	return rc;
}

static int get_cdm_index_by_id(char *identifier,
	uint32_t cell_index, uint32_t *hw_index)
{
	int rc = -EPERM, i, j;
	char client_name[128];

	CAM_DBG(CAM_CDM, "Looking for HW id of =%s and index=%d",
		identifier, cell_index);
	snprintf(client_name, sizeof(client_name), "%s", identifier);
	CAM_DBG(CAM_CDM, "Looking for HW id of %s count:%d", client_name,
		cdm_mgr.cdm_count);
	mutex_lock(&cam_cdm_mgr_lock);
	for (i = 0; i < cdm_mgr.cdm_count; i++) {
		mutex_lock(&cdm_mgr.nodes[i].lock);
		CAM_DBG(CAM_CDM, "dt_num_supported_clients=%d",
			cdm_mgr.nodes[i].data->dt_num_supported_clients);

		for (j = 0; j <
			cdm_mgr.nodes[i].data->dt_num_supported_clients; j++) {
			CAM_DBG(CAM_CDM, "client name:%s",
				cdm_mgr.nodes[i].data->dt_cdm_client_name[j]);
			if (!strcmp(
				cdm_mgr.nodes[i].data->dt_cdm_client_name[j],
				client_name)) {
				rc = 0;
				*hw_index = i;
				break;
			}
		}
		mutex_unlock(&cdm_mgr.nodes[i].lock);
		if (rc == 0)
			break;
	}
	mutex_unlock(&cam_cdm_mgr_lock);

	return rc;
}

int cam_cdm_get_iommu_handle(char *identifier,
	struct cam_iommu_handle *cdm_handles)
{
	int i, j, rc = -EPERM;

	if ((!identifier) || (!cdm_handles))
		return -EINVAL;

	if (get_cdm_mgr_refcount()) {
		CAM_ERR(CAM_CDM, "CDM intf mgr get refcount failed");
		return rc;
	}
	CAM_DBG(CAM_CDM, "Looking for Iommu handle of %s", identifier);

	for (i = 0; i < cdm_mgr.cdm_count; i++) {
		mutex_lock(&cdm_mgr.nodes[i].lock);
		if (!cdm_mgr.nodes[i].data) {
			mutex_unlock(&cdm_mgr.nodes[i].lock);
			continue;
		}
		for (j = 0; j <
			 cdm_mgr.nodes[i].data->dt_num_supported_clients;
			j++) {
			if (!strcmp(
				cdm_mgr.nodes[i].data->dt_cdm_client_name[j],
				identifier)) {
				rc = get_cdm_iommu_handle(cdm_handles, i);
				break;
			}
		}
		mutex_unlock(&cdm_mgr.nodes[i].lock);
		if (rc == 0)
			break;
	}
	put_cdm_mgr_refcount();

	return rc;
}
EXPORT_SYMBOL(cam_cdm_get_iommu_handle);

int cam_cdm_acquire(struct cam_cdm_acquire_data *data)
{
	int rc = -EPERM;
	struct cam_hw_intf *hw;
	uint32_t hw_index = 0;

	if ((!data) || (!data->identifier) || (!data->base_array) ||
		(!data->base_array_cnt))
		return -EINVAL;

	if (get_cdm_mgr_refcount()) {
		CAM_ERR(CAM_CDM, "CDM intf mgr get refcount failed");
		return rc;
	}

	if (data->id > CAM_CDM_HW_ANY) {
		CAM_ERR(CAM_CDM,
			"only CAM_CDM_VIRTUAL/CAM_CDM_HW_ANY is supported");
		rc = -EPERM;
		goto end;
	}
	rc = get_cdm_index_by_id(data->identifier, data->cell_index,
		&hw_index);
	if ((rc < 0) && (hw_index < CAM_CDM_INTF_MGR_MAX_SUPPORTED_CDM)) {
		CAM_ERR(CAM_CDM, "Failed to identify associated hw id");
		goto end;
	} else {
		CAM_DBG(CAM_CDM, "hw_index:%d", hw_index);
		hw = cdm_mgr.nodes[hw_index].device;
		if (hw && hw->hw_ops.process_cmd) {
			rc = hw->hw_ops.process_cmd(hw->hw_priv,
					CAM_CDM_HW_INTF_CMD_ACQUIRE, data,
					sizeof(struct cam_cdm_acquire_data));
			if (rc < 0) {
				CAM_ERR(CAM_CDM, "CDM hw acquire failed");
				goto end;
			}
		} else {
			CAM_ERR(CAM_CDM, "idx %d doesn't have acquire ops",
				hw_index);
			rc = -EPERM;
		}
	}
end:
	if (rc < 0) {
		CAM_ERR(CAM_CDM, "CDM acquire failed for id=%d name=%s, idx=%d",
			data->id, data->identifier, data->cell_index);
		put_cdm_mgr_refcount();
	}
	return rc;
}
EXPORT_SYMBOL(cam_cdm_acquire);

int cam_cdm_release(uint32_t handle)
{
	uint32_t hw_index;
	int rc = -EPERM;
	struct cam_hw_intf *hw;

	if (get_cdm_mgr_refcount()) {
		CAM_ERR(CAM_CDM, "CDM intf mgr get refcount failed");
		return rc;
	}

	hw_index = CAM_CDM_GET_HW_IDX(handle);
	if (hw_index < CAM_CDM_INTF_MGR_MAX_SUPPORTED_CDM) {
		hw = cdm_mgr.nodes[hw_index].device;
		if (hw && hw->hw_ops.process_cmd) {
			rc = hw->hw_ops.process_cmd(hw->hw_priv,
					CAM_CDM_HW_INTF_CMD_RELEASE, &handle,
					sizeof(handle));
			if (rc < 0)
				CAM_ERR(CAM_CDM,
					"hw release failed for handle=%x",
					handle);
		} else
			CAM_ERR(CAM_CDM, "hw idx %d doesn't have release ops",
				hw_index);
	}
	put_cdm_mgr_refcount();
	if (rc == 0)
		put_cdm_mgr_refcount();

	return rc;
}
EXPORT_SYMBOL(cam_cdm_release);


int cam_cdm_submit_bls(uint32_t handle, struct cam_cdm_bl_request *data)
{
	uint32_t hw_index;
	int rc = -EINVAL;
	struct cam_hw_intf *hw;

	if (!data)
		return rc;

	if (get_cdm_mgr_refcount()) {
		CAM_ERR(CAM_CDM, "CDM intf mgr get refcount failed");
		rc = -EPERM;
		return rc;
	}

	hw_index = CAM_CDM_GET_HW_IDX(handle);
	if (hw_index < CAM_CDM_INTF_MGR_MAX_SUPPORTED_CDM) {
		struct cam_cdm_hw_intf_cmd_submit_bl req;

		hw = cdm_mgr.nodes[hw_index].device;
		if (hw && hw->hw_ops.process_cmd) {
			req.data = data;
			req.handle = handle;
			rc = hw->hw_ops.process_cmd(hw->hw_priv,
				CAM_CDM_HW_INTF_CMD_SUBMIT_BL, &req,
				sizeof(struct cam_cdm_hw_intf_cmd_submit_bl));
			if (rc < 0)
				CAM_ERR(CAM_CDM,
					"hw submit bl failed for handle=%x",
					handle);
		} else {
			CAM_ERR(CAM_CDM, "hw idx %d doesn't have submit ops",
				hw_index);
		}
	}
	put_cdm_mgr_refcount();

	return rc;
}
EXPORT_SYMBOL(cam_cdm_submit_bls);

int cam_cdm_stream_on(uint32_t handle)
{
	uint32_t hw_index;
	int rc = -EINVAL;
	struct cam_hw_intf *hw;

	if (get_cdm_mgr_refcount()) {
		CAM_ERR(CAM_CDM, "CDM intf mgr get refcount failed");
		rc = -EPERM;
		return rc;
	}

	hw_index = CAM_CDM_GET_HW_IDX(handle);
	if (hw_index < CAM_CDM_INTF_MGR_MAX_SUPPORTED_CDM) {
		hw = cdm_mgr.nodes[hw_index].device;
			if (hw && hw->hw_ops.start) {
				rc = hw->hw_ops.start(hw->hw_priv, &handle,
						sizeof(uint32_t));
				if (rc < 0)
					CAM_ERR(CAM_CDM,
						"hw start failed handle=%x",
						handle);
			} else {
				CAM_ERR(CAM_CDM,
					"hw idx %d doesn't have start ops",
					hw_index);
			}
	}
	put_cdm_mgr_refcount();

	return rc;
}
EXPORT_SYMBOL(cam_cdm_stream_on);

int cam_cdm_stream_off(uint32_t handle)
{
	uint32_t hw_index;
	int rc = -EINVAL;
	struct cam_hw_intf *hw;

	if (get_cdm_mgr_refcount()) {
		CAM_ERR(CAM_CDM, "CDM intf mgr get refcount failed");
		rc = -EPERM;
		return rc;
	}

	hw_index = CAM_CDM_GET_HW_IDX(handle);
	if (hw_index < CAM_CDM_INTF_MGR_MAX_SUPPORTED_CDM) {
		hw = cdm_mgr.nodes[hw_index].device;
		if (hw && hw->hw_ops.stop) {
			rc = hw->hw_ops.stop(hw->hw_priv, &handle,
					sizeof(uint32_t));
			if (rc < 0)
				CAM_ERR(CAM_CDM, "hw stop failed handle=%x",
					handle);
		} else {
			CAM_ERR(CAM_CDM, "hw idx %d doesn't have stop ops",
				hw_index);
		}
	}
	put_cdm_mgr_refcount();

	return rc;
}
EXPORT_SYMBOL(cam_cdm_stream_off);

int cam_cdm_reset_hw(uint32_t handle)
{
	uint32_t hw_index;
	int rc = -EINVAL;
	struct cam_hw_intf *hw;

	if (get_cdm_mgr_refcount()) {
		CAM_ERR(CAM_CDM, "CDM intf mgr get refcount failed");
		rc = -EPERM;
		return rc;
	}

	hw_index = CAM_CDM_GET_HW_IDX(handle);
	if (hw_index < CAM_CDM_INTF_MGR_MAX_SUPPORTED_CDM) {
		hw = cdm_mgr.nodes[hw_index].device;
		if (hw && hw->hw_ops.process_cmd) {
			rc = hw->hw_ops.process_cmd(hw->hw_priv,
					CAM_CDM_HW_INTF_CMD_RESET_HW, &handle,
					sizeof(handle));
			if (rc < 0)
				CAM_ERR(CAM_CDM,
					"CDM hw release failed for handle=%x",
					handle);
		} else {
			CAM_ERR(CAM_CDM, "hw idx %d doesn't have release ops",
				hw_index);
		}
	}
	put_cdm_mgr_refcount();

	return rc;
}
EXPORT_SYMBOL(cam_cdm_reset_hw);

int cam_cdm_intf_register_hw_cdm(struct cam_hw_intf *hw,
	struct cam_cdm_private_dt_data *data, enum cam_cdm_type type,
	uint32_t *index)
{
	int rc = -EINVAL;

	if ((!hw) || (!data) || (!index))
		return rc;

	if (get_cdm_mgr_refcount()) {
		CAM_ERR(CAM_CDM, "CDM intf mgr get refcount failed");
		return rc;
	}

	mutex_lock(&cam_cdm_mgr_lock);
	if ((type == CAM_VIRTUAL_CDM) &&
		(!cdm_mgr.nodes[CAM_SW_CDM_INDEX].device)) {
		mutex_lock(&cdm_mgr.nodes[CAM_SW_CDM_INDEX].lock);
		cdm_mgr.nodes[CAM_SW_CDM_INDEX].device = hw;
		cdm_mgr.nodes[CAM_SW_CDM_INDEX].data = data;
		*index = cdm_mgr.cdm_count;
		mutex_unlock(&cdm_mgr.nodes[CAM_SW_CDM_INDEX].lock);
		cdm_mgr.cdm_count++;
		rc = 0;
	} else if ((type == CAM_HW_CDM) && (cdm_mgr.cdm_count > 0)) {
		mutex_lock(&cdm_mgr.nodes[cdm_mgr.cdm_count].lock);
		cdm_mgr.nodes[cdm_mgr.cdm_count].device = hw;
		cdm_mgr.nodes[cdm_mgr.cdm_count].data = data;
		*index = cdm_mgr.cdm_count;
		mutex_unlock(&cdm_mgr.nodes[cdm_mgr.cdm_count].lock);
		cdm_mgr.cdm_count++;
		rc = 0;
	} else {
		CAM_ERR(CAM_CDM, "CDM registration failed type=%d count=%d",
			type, cdm_mgr.cdm_count);
	}
	mutex_unlock(&cam_cdm_mgr_lock);
	put_cdm_mgr_refcount();

	return rc;
}

int cam_cdm_intf_deregister_hw_cdm(struct cam_hw_intf *hw,
	struct cam_cdm_private_dt_data *data, enum cam_cdm_type type,
	uint32_t index)
{
	int rc = -EINVAL;

	if ((!hw) || (!data))
		return rc;

	if (get_cdm_mgr_refcount()) {
		CAM_ERR(CAM_CDM, "CDM intf mgr get refcount failed");
		rc = -EPERM;
		return rc;
	}

	mutex_lock(&cam_cdm_mgr_lock);
	if ((type == CAM_VIRTUAL_CDM) &&
		(hw == cdm_mgr.nodes[CAM_SW_CDM_INDEX].device) &&
		(index == CAM_SW_CDM_INDEX)) {
		mutex_lock(&cdm_mgr.nodes[cdm_mgr.cdm_count].lock);
		cdm_mgr.nodes[CAM_SW_CDM_INDEX].device = NULL;
		cdm_mgr.nodes[CAM_SW_CDM_INDEX].data = NULL;
		mutex_unlock(&cdm_mgr.nodes[cdm_mgr.cdm_count].lock);
		rc = 0;
	} else if ((type == CAM_HW_CDM) &&
		(hw == cdm_mgr.nodes[index].device)) {
		mutex_lock(&cdm_mgr.nodes[index].lock);
		cdm_mgr.nodes[index].device = NULL;
		cdm_mgr.nodes[index].data = NULL;
		mutex_unlock(&cdm_mgr.nodes[index].lock);
		cdm_mgr.cdm_count--;
		rc = 0;
	} else {
		CAM_ERR(CAM_CDM, "CDM Deregistration failed type=%d index=%d",
			type, index);
	}
	mutex_unlock(&cam_cdm_mgr_lock);
	put_cdm_mgr_refcount();

	return rc;
}

static int cam_cdm_intf_probe(struct platform_device *pdev)
{
	int i, rc;

	rc = cam_cdm_intf_mgr_soc_get_dt_properties(pdev, &cdm_mgr);
	if (rc) {
		CAM_ERR(CAM_CDM, "Failed to get dt properties");
		return rc;
	}
	mutex_lock(&cam_cdm_mgr_lock);
	for (i = 0 ; i < CAM_CDM_INTF_MGR_MAX_SUPPORTED_CDM; i++) {
		mutex_init(&cdm_mgr.nodes[i].lock);
		cdm_mgr.nodes[i].device = NULL;
		cdm_mgr.nodes[i].data = NULL;
		cdm_mgr.nodes[i].refcount = 0;
	}
	cdm_mgr.probe_done = true;
	cdm_mgr.refcount = 0;
	mutex_unlock(&cam_cdm_mgr_lock);
	rc = cam_virtual_cdm_probe(pdev);
	if (rc) {
		mutex_lock(&cam_cdm_mgr_lock);
		cdm_mgr.probe_done = false;
		for (i = 0 ; i < CAM_CDM_INTF_MGR_MAX_SUPPORTED_CDM; i++) {
			if (cdm_mgr.nodes[i].device || cdm_mgr.nodes[i].data ||
				(cdm_mgr.nodes[i].refcount != 0))
				CAM_ERR(CAM_CDM,
					"Valid node present in index=%d", i);
			mutex_destroy(&cdm_mgr.nodes[i].lock);
			cdm_mgr.nodes[i].device = NULL;
			cdm_mgr.nodes[i].data = NULL;
			cdm_mgr.nodes[i].refcount = 0;
		}
		mutex_unlock(&cam_cdm_mgr_lock);
	}

	return rc;
}

static int cam_cdm_intf_remove(struct platform_device *pdev)
{
	int i, rc = -EBUSY;

	if (get_cdm_mgr_refcount()) {
		CAM_ERR(CAM_CDM, "CDM intf mgr get refcount failed");
		return rc;
	}

	if (cam_virtual_cdm_remove(pdev)) {
		CAM_ERR(CAM_CDM, "Virtual CDM remove failed");
		goto end;
	}
	put_cdm_mgr_refcount();

	mutex_lock(&cam_cdm_mgr_lock);
	if (cdm_mgr.refcount != 0) {
		CAM_ERR(CAM_CDM, "cdm manger refcount not zero %d",
			cdm_mgr.refcount);
		goto end;
	}

	for (i = 0 ; i < CAM_CDM_INTF_MGR_MAX_SUPPORTED_CDM; i++) {
		if (cdm_mgr.nodes[i].device || cdm_mgr.nodes[i].data ||
			(cdm_mgr.nodes[i].refcount != 0)) {
			CAM_ERR(CAM_CDM, "Valid node present in index=%d", i);
			mutex_unlock(&cam_cdm_mgr_lock);
			goto end;
		}
		mutex_destroy(&cdm_mgr.nodes[i].lock);
		cdm_mgr.nodes[i].device = NULL;
		cdm_mgr.nodes[i].data = NULL;
		cdm_mgr.nodes[i].refcount = 0;
	}
	cdm_mgr.probe_done = false;
	rc = 0;

end:
	mutex_unlock(&cam_cdm_mgr_lock);
	return rc;
}

static struct platform_driver cam_cdm_intf_driver = {
	.probe = cam_cdm_intf_probe,
	.remove = cam_cdm_intf_remove,
	.driver = {
	.name = "msm_cam_cdm_intf",
	.owner = THIS_MODULE,
	.of_match_table = msm_cam_cdm_intf_dt_match,
	.suppress_bind_attrs = true,
	},
};

static int __init cam_cdm_intf_init_module(void)
{
	return platform_driver_register(&cam_cdm_intf_driver);
}

static void __exit cam_cdm_intf_exit_module(void)
{
	platform_driver_unregister(&cam_cdm_intf_driver);
}

module_init(cam_cdm_intf_init_module);
module_exit(cam_cdm_intf_exit_module);
MODULE_DESCRIPTION("MSM Camera CDM Intf driver");
MODULE_LICENSE("GPL v2");
