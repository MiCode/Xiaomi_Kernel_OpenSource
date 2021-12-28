// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/timer.h>

#include "cam_node.h"
#include "cre_core.h"
#include "cre_soc.h"
#include "cam_hw.h"
#include "cre_hw.h"
#include "cam_hw_intf.h"
#include "cam_io_util.h"
#include "cam_cre_hw_mgr_intf.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"
#include "cre_hw_100.h"
#include "cre_dev_intf.h"
#include "cam_smmu_api.h"
#include "camera_main.h"

static struct cam_cre_hw_intf_data cam_cre_dev_list[CRE_DEV_MAX];
static struct cam_cre_device_hw_info cre_hw_info;
static struct cam_cre_soc_private cre_soc_info;

static char cre_dev_name[8];

static struct cre_hw_version_reg cre_hw_version_reg = {
	.hw_ver = 0x0,
};

static int cam_cre_init_hw_version(struct cam_hw_soc_info *soc_info,
	struct cam_cre_device_core_info *core_info)
{
	int rc = 0;

	CAM_DBG(CAM_CRE, "soc_info = %x core_info = %x",
		soc_info, core_info);
	CAM_DBG(CAM_CRE, "TOP: %x RD: %x WR: %x",
		soc_info->reg_map[CRE_TOP_BASE].mem_base,
		soc_info->reg_map[CRE_BUS_RD].mem_base,
		soc_info->reg_map[CRE_BUS_WR].mem_base);

	core_info->cre_hw_info->cre_top_base =
		soc_info->reg_map[CRE_TOP_BASE].mem_base;
	core_info->cre_hw_info->cre_bus_rd_base =
		soc_info->reg_map[CRE_BUS_RD].mem_base;
	core_info->cre_hw_info->cre_bus_wr_base =
		soc_info->reg_map[CRE_BUS_WR].mem_base;

	core_info->hw_version = cam_io_r_mb(
			core_info->cre_hw_info->cre_top_base +
			cre_hw_version_reg.hw_ver);

	switch (core_info->hw_version) {
	case CRE_HW_VER_1_0_0:
		core_info->cre_hw_info->cre_hw = &cre_hw_100;
		break;
	default:
		CAM_ERR(CAM_CRE, "Unsupported version : %u",
			core_info->hw_version);
		rc = -EINVAL;
		break;
	}

	cre_hw_100.top_reg_offset->base = core_info->cre_hw_info->cre_top_base;
	cre_hw_100.bus_rd_reg_offset->base = core_info->cre_hw_info->cre_bus_rd_base;
	cre_hw_100.bus_wr_reg_offset->base = core_info->cre_hw_info->cre_bus_wr_base;

	return rc;
}

int cam_cre_register_cpas(struct cam_hw_soc_info *soc_info,
	struct cam_cre_device_core_info *core_info,
	uint32_t hw_idx)
{
	struct cam_cpas_register_params cpas_register_params;
	int rc;

	cpas_register_params.dev = &soc_info->pdev->dev;
	memcpy(cpas_register_params.identifier, "cre", sizeof("cre"));
	cpas_register_params.cam_cpas_client_cb = NULL;
	cpas_register_params.cell_index = hw_idx;
	cpas_register_params.userdata = NULL;

	rc = cam_cpas_register_client(&cpas_register_params);
	if (rc < 0) {
		CAM_ERR(CAM_CRE, "failed: %d", rc);
		return rc;
	}
	core_info->cpas_handle = cpas_register_params.client_handle;

	return rc;
}

static int cam_cre_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_intf                *cre_dev_intf = NULL;
	struct cam_hw_info                *cre_dev = NULL;
	const struct of_device_id         *match_dev = NULL;
	struct cam_cre_device_core_info   *core_info = NULL;
	struct cam_cre_dev_probe           cre_probe;
	struct cam_cre_cpas_vote           cpas_vote;
	struct cam_cre_soc_private        *soc_private;
	int i;
	uint32_t hw_idx;
	int rc = 0;

	struct platform_device *pdev = to_platform_device(dev);

	of_property_read_u32(pdev->dev.of_node,
		"cell-index", &hw_idx);

	cre_dev_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!cre_dev_intf)
		return -ENOMEM;

	cre_dev_intf->hw_idx = hw_idx;
	cre_dev_intf->hw_type = CRE_DEV_CRE;
	cre_dev = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!cre_dev) {
		rc = -ENOMEM;
		goto cre_dev_alloc_failed;
	}

	memset(cre_dev_name, 0, sizeof(cre_dev_name));
	snprintf(cre_dev_name, sizeof(cre_dev_name),
		"cre%1u", cre_dev_intf->hw_idx);

	cre_dev->soc_info.pdev = pdev;
	cre_dev->soc_info.dev = &pdev->dev;
	cre_dev->soc_info.dev_name = cre_dev_name;
	cre_dev_intf->hw_priv = cre_dev;
	cre_dev_intf->hw_ops.init = cam_cre_init_hw;
	cre_dev_intf->hw_ops.deinit = cam_cre_deinit_hw;
	cre_dev_intf->hw_ops.get_hw_caps = cam_cre_get_hw_caps;
	cre_dev_intf->hw_ops.process_cmd = cam_cre_process_cmd;

	CAM_DBG(CAM_CRE, "type %d index %d",
		cre_dev_intf->hw_type,
		cre_dev_intf->hw_idx);

	if (cre_dev_intf->hw_idx < CRE_DEV_MAX)
		cam_cre_dev_list[cre_dev_intf->hw_idx].hw_intf =
			cre_dev_intf;

	platform_set_drvdata(pdev, cre_dev_intf);


	cre_dev->core_info = kzalloc(sizeof(struct cam_cre_device_core_info),
		GFP_KERNEL);
	if (!cre_dev->core_info) {
		rc = -ENOMEM;
		goto cre_core_alloc_failed;
	}
	core_info = (struct cam_cre_device_core_info *)cre_dev->core_info;
	core_info->cre_hw_info = &cre_hw_info;
	cre_dev->soc_info.soc_private = &cre_soc_info;

	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev) {
		rc = -EINVAL;
		CAM_DBG(CAM_CRE, "No cre hardware info");
		goto cre_match_dev_failed;
	}

	rc = cam_cre_init_soc_resources(&cre_dev->soc_info, cam_cre_irq,
		cre_dev);
	if (rc < 0) {
		CAM_ERR(CAM_CRE, "failed to init_soc");
		goto init_soc_failed;
	}
	core_info->hw_type = CRE_DEV_CRE;
	core_info->hw_idx = hw_idx;
	rc = cam_cre_register_cpas(&cre_dev->soc_info,
		core_info, cre_dev_intf->hw_idx);
	if (rc < 0)
		goto register_cpas_failed;

	rc = cam_cre_enable_soc_resources(&cre_dev->soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_CRE, "enable soc resorce failed: %d", rc);
		goto enable_soc_failed;
	}
	cpas_vote.ahb_vote.type = CAM_VOTE_ABSOLUTE;
	cpas_vote.ahb_vote.vote.level = CAM_SVS_VOTE;
	cpas_vote.axi_vote.num_paths = 1;
	cpas_vote.axi_vote.axi_path[0].path_data_type =
		CAM_AXI_PATH_DATA_CRE_WR_OUT;
	cpas_vote.axi_vote.axi_path[0].transac_type =
		CAM_AXI_TRANSACTION_WRITE;
	cpas_vote.axi_vote.axi_path[0].camnoc_bw =
		CAM_CPAS_DEFAULT_AXI_BW;
	cpas_vote.axi_vote.axi_path[0].mnoc_ab_bw =
		CAM_CPAS_DEFAULT_AXI_BW;
	cpas_vote.axi_vote.axi_path[0].mnoc_ib_bw =
		CAM_CPAS_DEFAULT_AXI_BW;
	cpas_vote.axi_vote.axi_path[0].ddr_ab_bw =
		CAM_CPAS_DEFAULT_AXI_BW;
	cpas_vote.axi_vote.axi_path[0].ddr_ib_bw =
		CAM_CPAS_DEFAULT_AXI_BW;

	rc = cam_cpas_start(core_info->cpas_handle,
		&cpas_vote.ahb_vote, &cpas_vote.axi_vote);

	rc = cam_cre_init_hw_version(&cre_dev->soc_info, cre_dev->core_info);
	if (rc)
		goto init_hw_failure;

	cam_cre_disable_soc_resources(&cre_dev->soc_info);
	cam_cpas_stop(core_info->cpas_handle);
	cre_dev->hw_state = CAM_HW_STATE_POWER_DOWN;

	cam_cre_process_cmd(cre_dev, CRE_HW_PROBE,
		&cre_probe, sizeof(cre_probe));
	mutex_init(&cre_dev->hw_mutex);
	spin_lock_init(&cre_dev->hw_lock);
	init_completion(&cre_dev->hw_complete);

	CAM_DBG(CAM_CRE, "CRE:%d component bound successfully",
		cre_dev_intf->hw_idx);
	soc_private = cre_dev->soc_info.soc_private;
	cam_cre_dev_list[cre_dev_intf->hw_idx].num_hw_pid =
		soc_private->num_pid;

	for (i = 0; i < soc_private->num_pid; i++)
		cam_cre_dev_list[cre_dev_intf->hw_idx].hw_pid[i] =
			soc_private->pid[i];

	return rc;

init_hw_failure:
enable_soc_failed:
register_cpas_failed:
init_soc_failed:
cre_match_dev_failed:
	kfree(cre_dev->core_info);
cre_core_alloc_failed:
	kfree(cre_dev);
cre_dev_alloc_failed:
	kfree(cre_dev_intf);
	return rc;
}

static void cam_cre_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);

	CAM_DBG(CAM_CRE, "Unbinding component: %s", pdev->name);
}

int cam_cre_hw_init(struct cam_cre_hw_intf_data **cre_hw_intf_data,
		uint32_t hw_idx)
{
	int rc = 0;

	if (cam_cre_dev_list[hw_idx].hw_intf) {
		*cre_hw_intf_data = &cam_cre_dev_list[hw_idx];
		rc = 0;
	} else {
		CAM_ERR(CAM_CRE, "inval param");
		*cre_hw_intf_data = NULL;
		rc = -ENODEV;
	}
	return rc;
}

const static struct component_ops cam_cre_component_ops = {
	.bind = cam_cre_component_bind,
	.unbind = cam_cre_component_unbind,
};

int cam_cre_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_CRE, "Adding CRE component");
	rc = component_add(&pdev->dev, &cam_cre_component_ops);
	if (rc)
		CAM_ERR(CAM_CRE, "failed to add component rc: %d", rc);

	return rc;
}

static const struct of_device_id cam_cre_dt_match[] = {
	{
		.compatible = "qcom,cre",
		.data = &cre_hw_version_reg,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cam_cre_dt_match);

struct platform_driver cam_cre_driver = {
	.probe = cam_cre_probe,
	.driver = {
		.name = "cre",
		.of_match_table = cam_cre_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_cre_init_module(void)
{
	return platform_driver_register(&cam_cre_driver);
}

void cam_cre_exit_module(void)
{
	platform_driver_unregister(&cam_cre_driver);
}

MODULE_DESCRIPTION("CAM CRE driver");
MODULE_LICENSE("GPL v2");
