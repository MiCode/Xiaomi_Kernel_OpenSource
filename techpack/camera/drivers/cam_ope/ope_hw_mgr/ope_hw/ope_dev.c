// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of_device.h>
#include <linux/timer.h>
#include "ope_core.h"
#include "ope_soc.h"
#include "cam_hw.h"
#include "ope_hw.h"
#include "cam_hw_intf.h"
#include "cam_io_util.h"
#include "cam_ope_hw_mgr_intf.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"
#include "ope_hw_100.h"
#include "ope_dev_intf.h"
#include "camera_main.h"

static struct cam_ope_hw_intf_data cam_ope_dev_list[OPE_DEV_MAX];
static struct cam_ope_device_hw_info ope_hw_info;
static struct cam_ope_soc_private ope_soc_info;
EXPORT_SYMBOL(ope_soc_info);

static struct hw_version_reg ope_hw_version_reg = {
	.hw_ver = 0x0,
};

static char ope_dev_name[8];

static int cam_ope_init_hw_version(struct cam_hw_soc_info *soc_info,
	struct cam_ope_device_core_info *core_info)
{
	int rc = 0;

	CAM_DBG(CAM_OPE, "soc_info = %x core_info = %x",
		soc_info, core_info);
	CAM_DBG(CAM_OPE, "CDM:%x TOP: %x QOS: %x PP: %x RD: %x WR: %x",
		soc_info->reg_map[OPE_CDM_BASE].mem_base,
		soc_info->reg_map[OPE_TOP_BASE].mem_base,
		soc_info->reg_map[OPE_QOS_BASE].mem_base,
		soc_info->reg_map[OPE_PP_BASE].mem_base,
		soc_info->reg_map[OPE_BUS_RD].mem_base,
		soc_info->reg_map[OPE_BUS_WR].mem_base);
	CAM_DBG(CAM_OPE, "core: %x",
		core_info->ope_hw_info->ope_cdm_base);

	core_info->ope_hw_info->ope_cdm_base =
		soc_info->reg_map[OPE_CDM_BASE].mem_base;
	core_info->ope_hw_info->ope_top_base =
		soc_info->reg_map[OPE_TOP_BASE].mem_base;
	core_info->ope_hw_info->ope_qos_base =
		soc_info->reg_map[OPE_QOS_BASE].mem_base;
	core_info->ope_hw_info->ope_pp_base =
		soc_info->reg_map[OPE_PP_BASE].mem_base;
	core_info->ope_hw_info->ope_bus_rd_base =
		soc_info->reg_map[OPE_BUS_RD].mem_base;
	core_info->ope_hw_info->ope_bus_wr_base =
		soc_info->reg_map[OPE_BUS_WR].mem_base;

	core_info->hw_version = cam_io_r_mb(
			core_info->ope_hw_info->ope_top_base +
			ope_hw_version_reg.hw_ver);

	switch (core_info->hw_version) {
	case OPE_HW_VER_1_0_0:
	case OPE_HW_VER_1_1_0:
		core_info->ope_hw_info->ope_hw = &ope_hw_100;
		break;
	default:
		CAM_ERR(CAM_OPE, "Unsupported version : %u",
			core_info->hw_version);
		rc = -EINVAL;
		break;
	}

	ope_hw_100.top_reg->base = core_info->ope_hw_info->ope_top_base;
	ope_hw_100.bus_rd_reg->base = core_info->ope_hw_info->ope_bus_rd_base;
	ope_hw_100.bus_wr_reg->base = core_info->ope_hw_info->ope_bus_wr_base;
	ope_hw_100.pp_reg->base = core_info->ope_hw_info->ope_pp_base;

	return rc;
}

int cam_ope_register_cpas(struct cam_hw_soc_info *soc_info,
	struct cam_ope_device_core_info *core_info,
	uint32_t hw_idx)
{
	struct cam_cpas_register_params cpas_register_params;
	int rc;

	cpas_register_params.dev = &soc_info->pdev->dev;
	memcpy(cpas_register_params.identifier, "ope", sizeof("ope"));
	cpas_register_params.cam_cpas_client_cb = NULL;
	cpas_register_params.cell_index = hw_idx;
	cpas_register_params.userdata = NULL;

	rc = cam_cpas_register_client(&cpas_register_params);
	if (rc < 0) {
		CAM_ERR(CAM_OPE, "failed: %d", rc);
		return rc;
	}
	core_info->cpas_handle = cpas_register_params.client_handle;

	return rc;
}

static int cam_ope_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_intf                *ope_dev_intf = NULL;
	struct cam_hw_info                *ope_dev = NULL;
	const struct of_device_id         *match_dev = NULL;
	struct cam_ope_device_core_info   *core_info = NULL;
	struct cam_ope_dev_probe           ope_probe;
	struct cam_ope_cpas_vote           cpas_vote;
	struct cam_ope_soc_private        *soc_private;
	int i;
	uint32_t hw_idx;
	int rc = 0;

	struct platform_device *pdev = to_platform_device(dev);

	of_property_read_u32(pdev->dev.of_node,
		"cell-index", &hw_idx);

	ope_dev_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!ope_dev_intf)
		return -ENOMEM;

	ope_dev_intf->hw_idx = hw_idx;
	ope_dev_intf->hw_type = OPE_DEV_OPE;
	ope_dev = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!ope_dev) {
		rc = -ENOMEM;
		goto ope_dev_alloc_failed;
	}

	memset(ope_dev_name, 0, sizeof(ope_dev_name));
	snprintf(ope_dev_name, sizeof(ope_dev_name),
		"ope%1u", ope_dev_intf->hw_idx);

	ope_dev->soc_info.pdev = pdev;
	ope_dev->soc_info.dev = &pdev->dev;
	ope_dev->soc_info.dev_name = ope_dev_name;
	ope_dev_intf->hw_priv = ope_dev;
	ope_dev_intf->hw_ops.init = cam_ope_init_hw;
	ope_dev_intf->hw_ops.deinit = cam_ope_deinit_hw;
	ope_dev_intf->hw_ops.get_hw_caps = cam_ope_get_hw_caps;
	ope_dev_intf->hw_ops.start = cam_ope_start;
	ope_dev_intf->hw_ops.stop = cam_ope_stop;
	ope_dev_intf->hw_ops.flush = cam_ope_flush;
	ope_dev_intf->hw_ops.process_cmd = cam_ope_process_cmd;

	CAM_DBG(CAM_OPE, "type %d index %d",
		ope_dev_intf->hw_type,
		ope_dev_intf->hw_idx);

	if (ope_dev_intf->hw_idx < OPE_DEV_MAX)
		cam_ope_dev_list[ope_dev_intf->hw_idx].hw_intf =
			ope_dev_intf;

	platform_set_drvdata(pdev, ope_dev_intf);


	ope_dev->core_info = kzalloc(sizeof(struct cam_ope_device_core_info),
		GFP_KERNEL);
	if (!ope_dev->core_info) {
		rc = -ENOMEM;
		goto ope_core_alloc_failed;
	}
	core_info = (struct cam_ope_device_core_info *)ope_dev->core_info;
	core_info->ope_hw_info = &ope_hw_info;
	ope_dev->soc_info.soc_private = &ope_soc_info;

	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev) {
		rc = -EINVAL;
		CAM_DBG(CAM_OPE, "No ope hardware info");
		goto ope_match_dev_failed;
	}

	rc = cam_ope_init_soc_resources(&ope_dev->soc_info, cam_ope_irq,
		ope_dev);
	if (rc < 0) {
		CAM_ERR(CAM_OPE, "failed to init_soc");
		goto init_soc_failed;
	}
	core_info->hw_type = OPE_DEV_OPE;
	core_info->hw_idx = hw_idx;
	rc = cam_ope_register_cpas(&ope_dev->soc_info,
		core_info, ope_dev_intf->hw_idx);
	if (rc < 0)
		goto register_cpas_failed;

	rc = cam_ope_enable_soc_resources(&ope_dev->soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_OPE, "enable soc resorce failed: %d", rc);
		goto enable_soc_failed;
	}
	cpas_vote.ahb_vote.type = CAM_VOTE_ABSOLUTE;
	cpas_vote.ahb_vote.vote.level = CAM_SVS_VOTE;
	cpas_vote.axi_vote.num_paths = 1;
	cpas_vote.axi_vote.axi_path[0].path_data_type =
		CAM_AXI_PATH_DATA_OPE_WR_VID;
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

	rc = cam_ope_init_hw_version(&ope_dev->soc_info, ope_dev->core_info);
	if (rc)
		goto init_hw_failure;

	cam_ope_disable_soc_resources(&ope_dev->soc_info, true);
	cam_cpas_stop(core_info->cpas_handle);
	ope_dev->hw_state = CAM_HW_STATE_POWER_DOWN;

	ope_probe.hfi_en = ope_soc_info.hfi_en;
	cam_ope_process_cmd(ope_dev, OPE_HW_PROBE,
		&ope_probe, sizeof(ope_probe));
	mutex_init(&ope_dev->hw_mutex);
	spin_lock_init(&ope_dev->hw_lock);
	init_completion(&ope_dev->hw_complete);

	CAM_DBG(CAM_OPE, "OPE:%d component bound successfully",
		ope_dev_intf->hw_idx);
	soc_private = ope_dev->soc_info.soc_private;
	cam_ope_dev_list[ope_dev_intf->hw_idx].num_hw_pid =
		soc_private->num_pid;

	for (i = 0; i < soc_private->num_pid; i++)
		cam_ope_dev_list[ope_dev_intf->hw_idx].hw_pid[i] =
			soc_private->pid[i];

	return rc;

init_hw_failure:
enable_soc_failed:
register_cpas_failed:
init_soc_failed:
ope_match_dev_failed:
	kfree(ope_dev->core_info);
ope_core_alloc_failed:
	kfree(ope_dev);
ope_dev_alloc_failed:
	kfree(ope_dev_intf);
	return rc;
}

static void cam_ope_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);

	CAM_DBG(CAM_OPE, "Unbinding component: %s", pdev->name);
}

int cam_ope_hw_init(struct cam_ope_hw_intf_data **ope_hw_intf_data,
		uint32_t hw_idx)
{
	int rc = 0;

	if (cam_ope_dev_list[hw_idx].hw_intf) {
		*ope_hw_intf_data = &cam_ope_dev_list[hw_idx];
		rc = 0;
	} else {
		CAM_ERR(CAM_OPE, "inval param");
		*ope_hw_intf_data = NULL;
		rc = -ENODEV;
	}
	return rc;
}

const static struct component_ops cam_ope_component_ops = {
	.bind = cam_ope_component_bind,
	.unbind = cam_ope_component_unbind,
};

int cam_ope_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_OPE, "Adding OPE component");
	rc = component_add(&pdev->dev, &cam_ope_component_ops);
	if (rc)
		CAM_ERR(CAM_OPE, "failed to add component rc: %d", rc);

	return rc;
}

static const struct of_device_id cam_ope_dt_match[] = {
	{
		.compatible = "qcom,ope",
		.data = &ope_hw_version_reg,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cam_ope_dt_match);

struct platform_driver cam_ope_driver = {
	.probe = cam_ope_probe,
	.driver = {
		.name = "ope",
		.of_match_table = cam_ope_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_ope_init_module(void)
{
	return platform_driver_register(&cam_ope_driver);
}

void cam_ope_exit_module(void)
{
	platform_driver_unregister(&cam_ope_driver);
}

MODULE_DESCRIPTION("CAM OPE driver");
MODULE_LICENSE("GPL v2");
