// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/nvmem-consumer.h>
#include <linux/slab.h>

#include "apusys_power_ctl.h"
#include "hal_config_power.h"
#include "apu_log.h"
struct device *apu_dev;

uint32_t efuse[APUSYS_EFUSE_NUM];
static const char *efuse_field[APUSYS_EFUSE_NUM] = {
	"efuse_segment", "efuse_pod19", "efuse_pod26"};


int init_platform_resource(struct platform_device *pdev,
struct hal_param_init_power *init_power_data)
{
	int err = 0, i = 0;
	struct nvmem_cell *cell[APUSYS_EFUSE_NUM];
	uint32_t *buf[APUSYS_EFUSE_NUM];
	size_t len[APUSYS_EFUSE_NUM];

	// necessary platform resources
	struct device *apusys_dev = &pdev->dev;
	struct resource *apusys_rpc_res = NULL;
	struct resource *apusys_pcu_res = NULL;
	struct resource *apusys_vcore_res = NULL;
	struct device_node *infra_ao_node = NULL;
	struct device_node *infra_bcrm_node = NULL;
	struct device_node *spm_node = NULL;
	struct device_node *apmixed_node = NULL;

	// debug related resources
	struct device_node *apusys_conn_node = NULL;
	struct device_node *apusys_vpu0_node = NULL;
	struct device_node *apusys_vpu1_node = NULL;
	struct device_node *apusys_mdla0_node = NULL;

	LOG_INF("%s pdev id = %d name = %s, name = %s\n", __func__,
						pdev->id, pdev->name,
						pdev->dev.of_node->name);
	init_power_data->dev = apusys_dev;
#ifdef APUSYS_POWER_BRINGUP
	apu_dev = &pdev->dev;
#endif

	for (i = 0; i < APUSYS_EFUSE_NUM; i++) {
		cell[i] = nvmem_cell_get(apusys_dev, efuse_field[i]);
		if (IS_ERR(cell[i])) {
			if (PTR_ERR(cell[i]) == -EPROBE_DEFER)
				return PTR_ERR(cell[i]);
			return -1;
		}

		buf[i] = (uint32_t *)nvmem_cell_read(cell[i], &len[i]);
		nvmem_cell_put(cell[i]);

		if (IS_ERR(buf[i]))
			return PTR_ERR(buf[i]);

		LOG_INF("efuse cell id: %d %x\n", i, *buf[i]);
		efuse[i] = *buf[i];

		kfree(buf[i]);
	}

	// spm
	spm_node = of_find_compatible_node(NULL, NULL,
		"mediatek,mt6873-scpsys");
	if (spm_node) {
		init_power_data->spm_base_addr = of_iomap(spm_node, 0);

		if (IS_ERR((void *)init_power_data->spm_base_addr)) {
			LOG_ERR("Unable to iomap spm_base_addr\n");
			goto err_exit;
		}
	}

	// infra ao
	infra_ao_node = of_find_compatible_node(NULL, NULL,
						"mediatek,mt8192-infracfg");
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

	/* APUPLL is from APMIXED and its parent clock is from XTAL(26MHz); */
	apmixed_node = of_find_compatible_node(NULL, NULL,
						"mediatek,mt8192-apmixedsys");
	if (apmixed_node) {
		init_power_data->apmixed_base_addr = of_iomap(
							apmixed_node, 0);

		if (IS_ERR((void *)init_power_data->apmixed_base_addr)) {
			LOG_ERR("Unable to iomap apmixed\n");
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
	apusys_conn_node = of_find_compatible_node(NULL, NULL,
							"mediatek,mt8192-apu_conn");
	if (apusys_conn_node) {
		init_power_data->conn_base_addr = of_iomap(
							apusys_conn_node, 0);

		if (IS_ERR((void *)init_power_data->conn_base_addr)) {
			LOG_ERR("Unable to iomap conn_base_addr\n");
			goto err_exit;
		}
	}

	// apusys vpu0
	apusys_vpu0_node = of_find_compatible_node(NULL, NULL,
							"mediatek,mt8192-apu0");
	if (apusys_vpu0_node) {
		init_power_data->vpu0_base_addr = of_iomap(
							apusys_vpu0_node, 0);

		if (IS_ERR((void *)init_power_data->vpu0_base_addr)) {
			LOG_ERR("Unable to iomap vpu0_base_addr\n");
			goto err_exit;
		}
	}

	// apusys vpu1
	apusys_vpu1_node = of_find_compatible_node(NULL, NULL,
							"mediatek,mt8192-apu1");
	if (apusys_vpu1_node) {
		init_power_data->vpu1_base_addr = of_iomap(
							apusys_vpu1_node, 0);

		if (IS_ERR((void *)init_power_data->vpu1_base_addr)) {
			LOG_ERR("Unable to iomap vpu1_base_addr\n");
			goto err_exit;
		}
	}

	// apusys mdla0
	apusys_mdla0_node = of_find_compatible_node(NULL, NULL,
							"mediatek,mt8192-apu_mdla0");
	if (apusys_mdla0_node) {
		init_power_data->mdla0_base_addr = of_iomap(
							apusys_mdla0_node, 0);

		if (IS_ERR((void *)init_power_data->mdla0_base_addr)) {
			LOG_ERR("Unable to iomap mdla0_base_addr\n");
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
	init_power_data->mdla0_base_addr = NULL;

	return err;
}
