// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/of.h>
#include <linux/pm_qos.h>
#include <linux/platform_device.h>
#include <linux/qcom_scm.h>
#include "msm_cvp_debug.h"
#include "cvp_comm_def.h"
#include "cvp_core_hfi.h"
#include "cvp_hfi.h"
#ifdef CVP_MDT_ENABLED
#include <linux/of_address.h>
#include <linux/firmware.h>
#include <linux/soc/qcom/mdt_loader.h>
#else
#include <soc/qcom/subsystem_restart.h>
#endif

#define MAX_FIRMWARE_NAME_SIZE 128

#ifdef CVP_MDT_ENABLED
static int __load_fw_to_memory(struct platform_device *pdev,
		const char *fw_name)
{
	int rc = 0;
	const struct firmware *firmware = NULL;
	char firmware_name[MAX_FIRMWARE_NAME_SIZE] = {0};
	struct device_node *node = NULL;
	struct resource res = {0};
	phys_addr_t phys = 0;
	size_t res_size = 0;
	ssize_t fw_size = 0;
	void *virt = NULL;
	int pas_id = 0;

	if (!fw_name || !(*fw_name) || !pdev) {
		dprintk(CVP_ERR, "%s: Invalid inputs\n", __func__);
		return -EINVAL;
	}
	if (strlen(fw_name) >= MAX_FIRMWARE_NAME_SIZE - 4) {
		dprintk(CVP_ERR, "%s: Invalid fw name\n", __func__);
		return -EINVAL;
	}
	scnprintf(firmware_name, ARRAY_SIZE(firmware_name), "%s.mdt", fw_name);

	rc = of_property_read_u32(pdev->dev.of_node, "pas-id", &pas_id);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: error %d while reading DT for \"pas-id\"\n",
				__func__, rc);
		goto exit;
	}

	node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!node) {
		dprintk(CVP_ERR,
			"%s: DT error getting \"memory-region\" property\n",
				__func__);
		return -EINVAL;
	}

	rc = of_address_to_resource(node, 0, &res);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: error %d getting \"memory-region\" resource\n",
				__func__, rc);
		goto exit;
	}
	phys = res.start;
	res_size = (size_t)resource_size(&res);

	rc = request_firmware(&firmware, firmware_name, &pdev->dev);
	if (rc) {
		dprintk(CVP_ERR, "%s: error %d requesting \"%s\"\n",
				__func__, rc, firmware_name);
		goto exit;
	}

	fw_size = qcom_mdt_get_size(firmware);
	if (fw_size < 0 || res_size < (size_t)fw_size) {
		rc = -EINVAL;
		dprintk(CVP_ERR,
			"%s: Corrupted fw image. Alloc size: %lu, fw size: %ld",
				__func__, res_size, fw_size);
		goto exit;
	}

	virt = memremap(phys, res_size, MEMREMAP_WC);
	if (!virt) {
		rc = -ENOMEM;
		dprintk(CVP_ERR, "%s: unable to remap firmware memory\n",
				__func__);
		goto exit;
	}

	rc = qcom_mdt_load(&pdev->dev, firmware, firmware_name,
			pas_id, virt, phys, res_size, NULL);
	if (rc) {
		dprintk(CVP_ERR, "%s: error %d loading \"%s\"\n",
				__func__, rc, firmware_name);
		goto exit;
	}
	rc = qcom_scm_pas_auth_and_reset(pas_id);
	if (rc) {
		dprintk(CVP_ERR, "%s: error %d authenticating \"%s\"\n",
				__func__, rc, firmware_name);
		goto exit;
	}

	memunmap(virt);
	release_firmware(firmware);
	dprintk(CVP_CORE, "%s: firmware \"%s\" loaded successfully\n",
			__func__, firmware_name);
	return pas_id;

exit:
	if (virt)
		memunmap(virt);
	if (firmware)
		release_firmware(firmware);
	return rc;
}
#endif

int load_cvp_fw_impl(struct iris_hfi_device *device)
{
	int rc = 0;

	if (!device->resources.fw.cookie) {
#ifdef CVP_MDT_ENABLED
		device->resources.fw.cookie =
			__load_fw_to_memory(device->res->pdev,
			device->res->fw_name);
		if (device->resources.fw.cookie <= 0) {
			dprintk(CVP_ERR, "Failed to download firmware\n");
			device->resources.fw.cookie = 0;
			rc = -ENOMEM;
		}
#else
		device->resources.fw.cookie =
			subsystem_get_with_fwname("evass",
					device->res->fw_name);
		if (IS_ERR_OR_NULL(device->resources.fw.cookie)) {
			dprintk(CVP_ERR, "Failed to download firmware\n");
			device->resources.fw.cookie = NULL;
			rc = -ENOMEM;
		}
#endif
	}
	return rc;
}

int unload_cvp_fw_impl(struct iris_hfi_device *device)
{
#ifdef CVP_MDT_ENABLED
	qcom_scm_pas_shutdown(device->resources.fw.cookie);
	device->resources.fw.cookie = 0;
#else
	subsystem_put(device->resources.fw.cookie);
	device->resources.fw.cookie = NULL;
#endif
	return 0;
}
