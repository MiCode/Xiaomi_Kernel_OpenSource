// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "hal_config_power.h"
#include "apu_log.h"


int init_platform_resource(struct platform_device *pdev,
struct hal_param_init_power *init_power_data)
{
	int err = 0;
	// necessary platform resources
	struct device *apusys_dev = &pdev->dev;
	struct resource *apusys_rpc_res = NULL;
	struct resource *apusys_pcu_res = NULL;
	struct resource *apusys_vcore_res = NULL;
	struct device_node *infra_ao_node = NULL;
	struct device_node *infra_bcrm_node = NULL;
	struct device_node *spm_node = NULL;

	// debug related resources
	struct device_node *apusys_conn_node = NULL;
	struct device_node *apusys_vpu0_node = NULL;
	struct device_node *apusys_vpu1_node = NULL;
	struct device_node *apusys_vpu2_node = NULL;
	struct device_node *apusys_mdla0_node = NULL;
	struct device_node *apusys_mdla1_node = NULL;

	LOG_INF("%s pdev id = %d name = %s, name = %s\n", __func__,
						pdev->id, pdev->name,
						pdev->dev.of_node->name);
	init_power_data->dev = apusys_dev;

	// spm
	spm_node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	if (spm_node) {
		init_power_data->spm_base_addr = of_iomap(spm_node, 0);

		if (IS_ERR((void *)init_power_data->spm_base_addr)) {
			LOG_ERR("Unable to iomap spm_base_addr\n");
			goto err_exit;
		}
	}

	// infra ao
	infra_ao_node = of_find_compatible_node(NULL, NULL,
						"mediatek,infracfg_ao");
	if (infra_ao_node) {
		init_power_data->infracfg_ao_base_addr = of_iomap(
							infra_ao_node, 0);

		if (IS_ERR((void *)init_power_data->infracfg_ao_base_addr)) {
			LOG_ERR("Unable to iomap infracfg_ao_base_addr\n");
			goto err_exit;
		}
	}

	// infra bcrm
	infra_bcrm_node = of_find_compatible_node(NULL, NULL,
						"mediatek,infra_bcrm");
	if (infra_bcrm_node) {
		init_power_data->infra_bcrm_base_addr = of_iomap(
							infra_bcrm_node, 0);

		if (IS_ERR((void *)init_power_data->infra_bcrm_base_addr)) {
			LOG_ERR("Unable to iomap infra_bcrm_base_addr\n");
			goto err_exit;
		}
	}

	// apusys rpc
	apusys_rpc_res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "apusys_rpc");
	init_power_data->rpc_base_addr = devm_ioremap_resource(
						apusys_dev, apusys_rpc_res);

	if (IS_ERR((void *)init_power_data->rpc_base_addr)) {
		LOG_ERR("Unable to ioremap apusys_rpc\n");
		goto err_exit;
	}

	LOG_INF("%s apusys_rpc = 0x%p, size = %d\n", __func__,
				init_power_data->rpc_base_addr,
				(unsigned int)resource_size(apusys_rpc_res));

	// apusys pcu
	apusys_pcu_res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "apusys_pcu");
	init_power_data->pcu_base_addr = devm_ioremap_resource(
						apusys_dev, apusys_pcu_res);

	if (IS_ERR((void *)init_power_data->pcu_base_addr)) {
		LOG_ERR("Unable to ioremap apusys_pcu\n");
		goto err_exit;
	}

	LOG_INF("%s apusys_pcu = 0x%p, size = %d\n", __func__,
				init_power_data->pcu_base_addr,
				(unsigned int)resource_size(apusys_pcu_res));

	// apusys vcore
	apusys_vcore_res = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "apusys_vcore");
	init_power_data->vcore_base_addr = devm_ioremap_resource(
						apusys_dev, apusys_vcore_res);

	if (IS_ERR((void *)init_power_data->vcore_base_addr)) {
		LOG_ERR("Unable to ioremap apusys_vcore\n");
		goto err_exit;
	}

	LOG_INF("%s apusys_vcore = 0x%p, size = %d\n", __func__,
				init_power_data->vcore_base_addr,
				(unsigned int)resource_size(apusys_vcore_res));
	// apusys conn
#if defined(CONFIG_MACH_MT6893)
	apusys_conn_node = of_find_compatible_node(NULL, NULL,
						"mediatek,mt6893-apu_conn");
#else
	apusys_conn_node = of_find_compatible_node(NULL, NULL,
						"mediatek,apu_conn");
#endif
	if (apusys_conn_node) {
		init_power_data->conn_base_addr = of_iomap(
							apusys_conn_node, 0);

		if (IS_ERR((void *)init_power_data->conn_base_addr)) {
			LOG_ERR("Unable to iomap conn_base_addr\n");
			goto err_exit;
		}
	}

	// apusys vpu0
#if defined(CONFIG_MACH_MT6893)
	apusys_vpu0_node = of_find_compatible_node(NULL, NULL,
						"mediatek,mt6893-apu0");
#else
	apusys_vpu0_node = of_find_compatible_node(NULL, NULL,
						"mediatek,apu0");
#endif
	if (apusys_vpu0_node) {
		init_power_data->vpu0_base_addr = of_iomap(
							apusys_vpu0_node, 0);

		if (IS_ERR((void *)init_power_data->vpu0_base_addr)) {
			LOG_ERR("Unable to iomap vpu0_base_addr\n");
			goto err_exit;
		}
	}

	// apusys vpu1
#if defined(CONFIG_MACH_MT6893)
	apusys_vpu1_node = of_find_compatible_node(NULL, NULL,
						"mediatek,mt6893-apu1");
#else
	apusys_vpu1_node = of_find_compatible_node(NULL, NULL,
						"mediatek,apu1");
#endif
	if (apusys_vpu1_node) {
		init_power_data->vpu1_base_addr = of_iomap(
							apusys_vpu1_node, 0);

		if (IS_ERR((void *)init_power_data->vpu1_base_addr)) {
			LOG_ERR("Unable to iomap vpu1_base_addr\n");
			goto err_exit;
		}
	}

	// apusys vpu2
#if defined(CONFIG_MACH_MT6893)
	apusys_vpu2_node = of_find_compatible_node(NULL, NULL,
						"mediatek,mt6893-apu2");
#else
	apusys_vpu2_node = of_find_compatible_node(NULL, NULL,
						"mediatek,apu2");
#endif
	if (apusys_vpu2_node) {
		init_power_data->vpu2_base_addr = of_iomap(
							apusys_vpu2_node, 0);

		if (IS_ERR((void *)init_power_data->vpu2_base_addr)) {
			LOG_ERR("Unable to iomap vpu2_base_addr\n");
			goto err_exit;
		}
	}

	// apusys mdla0
#if defined(CONFIG_MACH_MT6893)
	apusys_mdla0_node = of_find_compatible_node(NULL, NULL,
						"mediatek,mt6893-apu_mdla0");
#else
	apusys_mdla0_node = of_find_compatible_node(NULL, NULL,
						"mediatek,apu_mdla0");
#endif
	if (apusys_mdla0_node) {
		init_power_data->mdla0_base_addr = of_iomap(
							apusys_mdla0_node, 0);

		if (IS_ERR((void *)init_power_data->mdla0_base_addr)) {
			LOG_ERR("Unable to iomap mdla0_base_addr\n");
			goto err_exit;
		}
	}

	// apusys mdla1
#if defined(CONFIG_MACH_MT6893)
	apusys_mdla1_node = of_find_compatible_node(NULL, NULL,
						"mediatek,mt6893-apu_mdla1");
#else
	apusys_mdla1_node = of_find_compatible_node(NULL, NULL,
						"mediatek,apu_mdla1");
#endif
	if (apusys_mdla1_node) {
		init_power_data->mdla1_base_addr = of_iomap(
							apusys_mdla1_node, 0);

		if (IS_ERR((void *)init_power_data->mdla1_base_addr)) {
			LOG_ERR("Unable to iomap mdla1_base_addr\n");
			goto err_exit;
		}
	}

	return 0;

err_exit:
	init_power_data->rpc_base_addr = NULL;
	init_power_data->pcu_base_addr = NULL;
	init_power_data->vcore_base_addr = NULL;
	init_power_data->infracfg_ao_base_addr = NULL;
	init_power_data->infra_bcrm_base_addr = NULL;

	init_power_data->conn_base_addr = NULL;
	init_power_data->vpu0_base_addr = NULL;
	init_power_data->vpu1_base_addr = NULL;
	init_power_data->vpu2_base_addr = NULL;
	init_power_data->mdla0_base_addr = NULL;
	init_power_data->mdla1_base_addr = NULL;

	return err;
}
