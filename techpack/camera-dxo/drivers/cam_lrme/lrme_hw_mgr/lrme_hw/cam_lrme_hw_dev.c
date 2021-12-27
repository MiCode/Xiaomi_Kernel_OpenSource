// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <media/cam_req_mgr.h>

#include "cam_subdev.h"
#include "cam_lrme_hw_intf.h"
#include "cam_lrme_hw_core.h"
#include "cam_lrme_hw_soc.h"
#include "cam_lrme_hw_reg.h"
#include "cam_req_mgr_workq.h"
#include "cam_lrme_hw_mgr.h"
#include "cam_mem_mgr_api.h"
#include "cam_smmu_api.h"

static int cam_lrme_hw_dev_util_cdm_acquire(struct cam_lrme_core *lrme_core,
	struct cam_hw_info *lrme_hw)
{
	int rc, i;
	struct cam_cdm_bl_request *cdm_cmd;
	struct cam_cdm_acquire_data cdm_acquire;
	struct cam_lrme_cdm_info *hw_cdm_info;

	hw_cdm_info = kzalloc(sizeof(struct cam_lrme_cdm_info),
		GFP_KERNEL);
	if (!hw_cdm_info) {
		CAM_ERR(CAM_LRME, "No memory for hw_cdm_info");
		return -ENOMEM;
	}

	cdm_cmd = kzalloc((sizeof(struct cam_cdm_bl_request) +
		((CAM_LRME_MAX_HW_ENTRIES - 1) *
		sizeof(struct cam_cdm_bl_cmd))), GFP_KERNEL);
	if (!cdm_cmd) {
		CAM_ERR(CAM_LRME, "No memory for cdm_cmd");
		kfree(hw_cdm_info);
		return -ENOMEM;
	}

	memset(&cdm_acquire, 0, sizeof(cdm_acquire));
	strlcpy(cdm_acquire.identifier, "lrmecdm", sizeof("lrmecdm"));
	cdm_acquire.cell_index = lrme_hw->soc_info.index;
	cdm_acquire.handle = 0;
	cdm_acquire.userdata = hw_cdm_info;
	cdm_acquire.cam_cdm_callback = NULL;
	cdm_acquire.id = CAM_CDM_VIRTUAL;
	cdm_acquire.base_array_cnt = lrme_hw->soc_info.num_reg_map;
	for (i = 0; i < lrme_hw->soc_info.num_reg_map; i++)
		cdm_acquire.base_array[i] = &lrme_hw->soc_info.reg_map[i];

	rc = cam_cdm_acquire(&cdm_acquire);
	if (rc) {
		CAM_ERR(CAM_LRME, "Can't acquire cdm");
		goto error;
	}

	hw_cdm_info->cdm_cmd = cdm_cmd;
	hw_cdm_info->cdm_ops = cdm_acquire.ops;
	hw_cdm_info->cdm_handle = cdm_acquire.handle;

	lrme_core->hw_cdm_info = hw_cdm_info;
	CAM_DBG(CAM_LRME, "cdm acquire done");

	return 0;
error:
	kfree(cdm_cmd);
	kfree(hw_cdm_info);
	return rc;
}

static int cam_lrme_hw_dev_probe(struct platform_device *pdev)
{
	struct cam_hw_info *lrme_hw;
	struct cam_hw_intf lrme_hw_intf;
	struct cam_lrme_core *lrme_core;
	const struct of_device_id *match_dev = NULL;
	struct cam_lrme_hw_info *hw_info;
	int rc, i;

	lrme_hw = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!lrme_hw) {
		CAM_ERR(CAM_LRME, "No memory to create lrme_hw");
		return -ENOMEM;
	}

	lrme_core = kzalloc(sizeof(struct cam_lrme_core), GFP_KERNEL);
	if (!lrme_core) {
		CAM_ERR(CAM_LRME, "No memory to create lrme_core");
		kfree(lrme_hw);
		return -ENOMEM;
	}

	lrme_hw->core_info = lrme_core;
	lrme_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	lrme_hw->soc_info.pdev = pdev;
	lrme_hw->soc_info.dev = &pdev->dev;
	lrme_hw->soc_info.dev_name = pdev->name;
	lrme_hw->open_count = 0;
	lrme_core->state = CAM_LRME_CORE_STATE_INIT;

	mutex_init(&lrme_hw->hw_mutex);
	spin_lock_init(&lrme_hw->hw_lock);
	init_completion(&lrme_hw->hw_complete);
	init_completion(&lrme_core->reset_complete);

	rc = cam_req_mgr_workq_create("cam_lrme_hw_worker",
		CAM_LRME_HW_WORKQ_NUM_TASK,
		&lrme_core->work, CRM_WORKQ_USAGE_IRQ, 0);
	if (rc) {
		CAM_ERR(CAM_LRME, "Unable to create a workq, rc=%d", rc);
		goto free_memory;
	}

	for (i = 0; i < CAM_LRME_HW_WORKQ_NUM_TASK; i++)
		lrme_core->work->task.pool[i].payload =
			&lrme_core->work_data[i];

	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev || !match_dev->data) {
		CAM_ERR(CAM_LRME, "No Of_match data, %pK", match_dev);
		rc = -EINVAL;
		goto destroy_workqueue;
	}
	hw_info = (struct cam_lrme_hw_info *)match_dev->data;
	lrme_core->hw_info = hw_info;

	rc = cam_lrme_soc_init_resources(&lrme_hw->soc_info,
		cam_lrme_hw_irq, lrme_hw);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to init soc, rc=%d", rc);
		goto destroy_workqueue;
	}

	rc = cam_lrme_hw_dev_util_cdm_acquire(lrme_core, lrme_hw);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to acquire cdm");
		goto deinit_platform_res;
	}

	rc = cam_smmu_get_handle("lrme", &lrme_core->device_iommu.non_secure);
	if (rc) {
		CAM_ERR(CAM_LRME, "Get iommu handle failed");
		goto release_cdm;
	}

	rc = cam_lrme_hw_start(lrme_hw, NULL, 0);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to hw init, rc=%d", rc);
		goto detach_smmu;
	}

	rc = cam_lrme_hw_util_get_caps(lrme_hw, &lrme_core->hw_caps);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to get hw caps, rc=%d", rc);
		if (cam_lrme_hw_stop(lrme_hw, NULL, 0))
			CAM_ERR(CAM_LRME, "Failed in hw deinit");
		goto detach_smmu;
	}

	rc = cam_lrme_hw_stop(lrme_hw, NULL, 0);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to deinit hw, rc=%d", rc);
		goto detach_smmu;
	}

	lrme_core->hw_idx = lrme_hw->soc_info.index;
	lrme_hw_intf.hw_priv = lrme_hw;
	lrme_hw_intf.hw_idx = lrme_hw->soc_info.index;
	lrme_hw_intf.hw_ops.get_hw_caps = cam_lrme_hw_get_caps;
	lrme_hw_intf.hw_ops.init = NULL;
	lrme_hw_intf.hw_ops.deinit = NULL;
	lrme_hw_intf.hw_ops.reset = cam_lrme_hw_reset;
	lrme_hw_intf.hw_ops.reserve = NULL;
	lrme_hw_intf.hw_ops.release = NULL;
	lrme_hw_intf.hw_ops.start = cam_lrme_hw_start;
	lrme_hw_intf.hw_ops.stop = cam_lrme_hw_stop;
	lrme_hw_intf.hw_ops.read = NULL;
	lrme_hw_intf.hw_ops.write = NULL;
	lrme_hw_intf.hw_ops.process_cmd = cam_lrme_hw_process_cmd;
	lrme_hw_intf.hw_ops.flush = cam_lrme_hw_flush;
	lrme_hw_intf.hw_type = CAM_HW_LRME;

	rc = cam_cdm_get_iommu_handle("lrmecdm", &lrme_core->cdm_iommu);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to acquire the CDM iommu handles");
		goto detach_smmu;
	}

	rc = cam_lrme_mgr_register_device(&lrme_hw_intf,
		&lrme_core->device_iommu,
		&lrme_core->cdm_iommu);
	if (rc) {
		CAM_ERR(CAM_LRME, "Failed to register device");
		goto detach_smmu;
	}

	platform_set_drvdata(pdev, lrme_hw);
	CAM_DBG(CAM_LRME, "LRME-%d probe successful", lrme_hw_intf.hw_idx);

	return rc;

detach_smmu:
	cam_smmu_destroy_handle(lrme_core->device_iommu.non_secure);
release_cdm:
	cam_cdm_release(lrme_core->hw_cdm_info->cdm_handle);
	kfree(lrme_core->hw_cdm_info->cdm_cmd);
	kfree(lrme_core->hw_cdm_info);
deinit_platform_res:
	if (cam_lrme_soc_deinit_resources(&lrme_hw->soc_info))
		CAM_ERR(CAM_LRME, "Failed in soc deinit");
	mutex_destroy(&lrme_hw->hw_mutex);
destroy_workqueue:
	cam_req_mgr_workq_destroy(&lrme_core->work);
free_memory:
	mutex_destroy(&lrme_hw->hw_mutex);
	kfree(lrme_hw);
	kfree(lrme_core);

	return rc;
}

static int cam_lrme_hw_dev_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct cam_hw_info *lrme_hw;
	struct cam_lrme_core *lrme_core;

	lrme_hw = platform_get_drvdata(pdev);
	if (!lrme_hw) {
		CAM_ERR(CAM_LRME, "Invalid lrme_hw from fd_hw_intf");
		return -ENODEV;
	}

	lrme_core = (struct cam_lrme_core *)lrme_hw->core_info;
	if (!lrme_core) {
		CAM_ERR(CAM_LRME, "Invalid lrme_core from fd_hw");
		rc = -EINVAL;
		goto deinit_platform_res;
	}

	cam_smmu_destroy_handle(lrme_core->device_iommu.non_secure);
	cam_cdm_release(lrme_core->hw_cdm_info->cdm_handle);
	cam_lrme_mgr_deregister_device(lrme_core->hw_idx);

	kfree(lrme_core->hw_cdm_info->cdm_cmd);
	kfree(lrme_core->hw_cdm_info);
	kfree(lrme_core);

deinit_platform_res:
	rc = cam_lrme_soc_deinit_resources(&lrme_hw->soc_info);
	if (rc)
		CAM_ERR(CAM_LRME, "Error in LRME soc deinit, rc=%d", rc);

	mutex_destroy(&lrme_hw->hw_mutex);
	kfree(lrme_hw);

	return rc;
}

static const struct of_device_id cam_lrme_hw_dt_match[] = {
	{
		.compatible = "qcom,lrme",
		.data = &cam_lrme10_hw_info,
	},
	{}
};

MODULE_DEVICE_TABLE(of, cam_lrme_hw_dt_match);

static struct platform_driver cam_lrme_hw_driver = {
	.probe = cam_lrme_hw_dev_probe,
	.remove = cam_lrme_hw_dev_remove,
	.driver = {
		.name = "cam_lrme_hw",
		.owner = THIS_MODULE,
		.of_match_table = cam_lrme_hw_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_lrme_hw_init_module(void)
{
	return platform_driver_register(&cam_lrme_hw_driver);
}

static void __exit cam_lrme_hw_exit_module(void)
{
	platform_driver_unregister(&cam_lrme_hw_driver);
}

module_init(cam_lrme_hw_init_module);
module_exit(cam_lrme_hw_exit_module);
MODULE_DESCRIPTION("CAM LRME HW driver");
MODULE_LICENSE("GPL v2");
