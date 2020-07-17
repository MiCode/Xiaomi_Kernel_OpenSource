// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of_device.h>
#include <linux/timer.h>
#include "a5_core.h"
#include "a5_soc.h"
#include "cam_io_util.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cam_a5_hw_intf.h"
#include "cam_icp_hw_mgr_intf.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"

struct a5_soc_info cam_a5_soc_info;
EXPORT_SYMBOL(cam_a5_soc_info);

struct cam_a5_device_hw_info cam_a5_hw_info = {
	.hw_ver = 0x0,
	.nsec_reset = 0x4,
	.a5_control = 0x8,
	.a5_host_int_en = 0x10,
	.a5_host_int = 0x14,
	.a5_host_int_clr = 0x18,
	.a5_host_int_status = 0x1c,
	.a5_host_int_set = 0x20,
	.host_a5_int = 0x30,
	.fw_version = 0x44,
	.init_req = 0x48,
	.init_response = 0x4c,
	.shared_mem_ptr = 0x50,
	.shared_mem_size = 0x54,
	.qtbl_ptr = 0x58,
	.uncached_heap_ptr = 0x5c,
	.uncached_heap_size = 0x60,
	.a5_status = 0x200,
};
EXPORT_SYMBOL(cam_a5_hw_info);

static bool cam_a5_cpas_cb(uint32_t client_handle, void *userdata,
	struct cam_cpas_irq_data *irq_data)
{
	bool error_handled = false;

	if (!irq_data)
		return error_handled;

	switch (irq_data->irq_type) {
	case CAM_CAMNOC_IRQ_IPE_BPS_UBWC_DECODE_ERROR:
		CAM_ERR_RATE_LIMIT(CAM_ICP,
			"IPE/BPS UBWC Decode error type=%d status=%x thr_err=%d, fcl_err=%d, len_md_err=%d, format_err=%d",
			irq_data->irq_type,
			irq_data->u.dec_err.decerr_status.value,
			irq_data->u.dec_err.decerr_status.thr_err,
			irq_data->u.dec_err.decerr_status.fcl_err,
			irq_data->u.dec_err.decerr_status.len_md_err,
			irq_data->u.dec_err.decerr_status.format_err);
		error_handled = true;
		break;
	case CAM_CAMNOC_IRQ_IPE_BPS_UBWC_ENCODE_ERROR:
		CAM_ERR_RATE_LIMIT(CAM_ICP,
			"IPE/BPS UBWC Encode error type=%d status=%x",
			irq_data->irq_type,
			irq_data->u.enc_err.encerr_status.value);
		error_handled = true;
		break;
	default:
		break;
	}

	return error_handled;
}

int cam_a5_register_cpas(struct cam_hw_soc_info *soc_info,
			struct cam_a5_device_core_info *core_info,
			uint32_t hw_idx)
{
	struct cam_cpas_register_params cpas_register_params;
	int rc;

	cpas_register_params.dev = &soc_info->pdev->dev;
	memcpy(cpas_register_params.identifier, "icp", sizeof("icp"));
	cpas_register_params.cam_cpas_client_cb = cam_a5_cpas_cb;
	cpas_register_params.cell_index = hw_idx;
	cpas_register_params.userdata = NULL;

	rc = cam_cpas_register_client(&cpas_register_params);
	if (rc < 0) {
		CAM_ERR(CAM_ICP, "failed: %d", rc);
		return rc;
	}

	core_info->cpas_handle = cpas_register_params.client_handle;
	return rc;
}

int cam_a5_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct cam_hw_info *a5_dev = NULL;
	struct cam_hw_intf *a5_dev_intf = NULL;
	const struct of_device_id *match_dev = NULL;
	struct cam_a5_device_core_info *core_info = NULL;
	struct cam_a5_device_hw_info *hw_info = NULL;

	a5_dev_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!a5_dev_intf)
		return -ENOMEM;

	of_property_read_u32(pdev->dev.of_node,
		"cell-index", &a5_dev_intf->hw_idx);

	a5_dev = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!a5_dev) {
		rc = -ENOMEM;
		goto a5_dev_alloc_failure;
	}

	a5_dev->soc_info.pdev = pdev;
	a5_dev->soc_info.dev = &pdev->dev;
	a5_dev->soc_info.dev_name = pdev->name;
	a5_dev_intf->hw_priv = a5_dev;
	a5_dev_intf->hw_ops.init = cam_a5_init_hw;
	a5_dev_intf->hw_ops.deinit = cam_a5_deinit_hw;
	a5_dev_intf->hw_ops.process_cmd = cam_a5_process_cmd;
	a5_dev_intf->hw_type = CAM_ICP_DEV_A5;

	CAM_DBG(CAM_ICP, "type %d index %d",
		a5_dev_intf->hw_type,
		a5_dev_intf->hw_idx);

	platform_set_drvdata(pdev, a5_dev_intf);

	a5_dev->core_info = kzalloc(sizeof(struct cam_a5_device_core_info),
					GFP_KERNEL);
	if (!a5_dev->core_info) {
		rc = -ENOMEM;
		goto core_info_alloc_failure;
	}
	core_info = (struct cam_a5_device_core_info *)a5_dev->core_info;

	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev) {
		CAM_ERR(CAM_ICP, "No a5 hardware info");
		rc = -EINVAL;
		goto match_err;
	}
	hw_info = (struct cam_a5_device_hw_info *)match_dev->data;
	core_info->a5_hw_info = hw_info;

	a5_dev->soc_info.soc_private = &cam_a5_soc_info;

	rc = cam_a5_init_soc_resources(&a5_dev->soc_info, cam_a5_irq,
		a5_dev);
	if (rc < 0) {
		CAM_ERR(CAM_ICP, "failed to init_soc");
		goto init_soc_failure;
	}

	CAM_DBG(CAM_ICP, "soc info : %pK",
				(void *)&a5_dev->soc_info);
	rc = cam_a5_register_cpas(&a5_dev->soc_info,
			core_info, a5_dev_intf->hw_idx);
	if (rc < 0) {
		CAM_ERR(CAM_ICP, "a5 cpas registration failed");
		goto cpas_reg_failed;
	}
	a5_dev->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&a5_dev->hw_mutex);
	spin_lock_init(&a5_dev->hw_lock);
	init_completion(&a5_dev->hw_complete);

	CAM_DBG(CAM_ICP, "A5%d probe successful",
		a5_dev_intf->hw_idx);
	return 0;

cpas_reg_failed:
init_soc_failure:
match_err:
	kfree(a5_dev->core_info);
core_info_alloc_failure:
	kfree(a5_dev);
a5_dev_alloc_failure:
	kfree(a5_dev_intf);

	return rc;
}

static const struct of_device_id cam_a5_dt_match[] = {
	{
		.compatible = "qcom,cam-a5",
		.data = &cam_a5_hw_info,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cam_a5_dt_match);

static struct platform_driver cam_a5_driver = {
	.probe = cam_a5_probe,
	.driver = {
		.name = "cam-a5",
		.owner = THIS_MODULE,
		.of_match_table = cam_a5_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_a5_init_module(void)
{
	return platform_driver_register(&cam_a5_driver);
}

static void __exit cam_a5_exit_module(void)
{
	platform_driver_unregister(&cam_a5_driver);
}

module_init(cam_a5_init_module);
module_exit(cam_a5_exit_module);
MODULE_DESCRIPTION("CAM A5 driver");
MODULE_LICENSE("GPL v2");
