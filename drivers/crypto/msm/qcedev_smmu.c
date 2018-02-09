/* Qti (or) Qualcomm Technologies Inc CE device driver.
 *
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
#include <asm/dma-iommu.h>
#include <linux/dma-mapping.h>
#include <linux/list.h>
#include <linux/qcedev.h>
#include "qcedevi.h"
#include "qcedev_smmu.h"
#include "soc/qcom/secure_buffer.h"

static int qcedev_setup_context_bank(struct context_bank_info *cb,
				struct device *dev)
{
	int rc = 0;
	int secure_vmid = VMID_INVAL;
	struct bus_type *bus;

	if (!dev || !cb) {
		pr_err("%s err: invalid input params\n", __func__);
		return -EINVAL;
	}
	cb->dev = dev;

	bus = cb->dev->bus;
	if (IS_ERR_OR_NULL(bus)) {
		pr_err("%s err: failed to get bus type\n", __func__);
		rc = PTR_ERR(bus) ?: -ENODEV;
		goto remove_cb;
	}

	cb->mapping = arm_iommu_create_mapping(bus, cb->start_addr, cb->size);
	if (IS_ERR_OR_NULL(cb->mapping)) {
		pr_err("%s err: failed to create mapping\n", __func__);
		rc = PTR_ERR(cb->mapping) ?: -ENODEV;
		goto remove_cb;
	}

	if (cb->is_secure) {
		/* Hardcoded since we only have this vmid.*/
		secure_vmid = VMID_CP_BITSTREAM;
		rc = iommu_domain_set_attr(cb->mapping->domain,
			DOMAIN_ATTR_SECURE_VMID, &secure_vmid);
		if (rc) {
			pr_err("%s err: programming secure vmid failed %s %d\n",
				__func__, dev_name(dev), rc);
			goto release_mapping;
		}
	}

	rc = arm_iommu_attach_device(cb->dev, cb->mapping);
	if (rc) {
		pr_err("%s err: Failed to attach %s - %d\n",
			__func__, dev_name(dev), rc);
		goto release_mapping;
	}

	pr_info("%s Attached %s and create mapping\n", __func__, dev_name(dev));
	pr_info("%s Context Bank name:%s, is_secure:%d, start_addr:%#x\n",
			__func__, cb->name, cb->is_secure, cb->start_addr);
	pr_info("%s size:%#x, dev:%pK, mapping:%pK\n", __func__, cb->size,
			cb->dev, cb->mapping);
	return rc;

release_mapping:
	arm_iommu_release_mapping(cb->mapping);
remove_cb:
	return rc;
}

int qcedev_parse_context_bank(struct platform_device *pdev)
{
	struct qcedev_control *podev;
	struct context_bank_info *cb = NULL;
	struct device_node *np = NULL;
	int rc = 0;

	if (!pdev) {
		pr_err("%s err: invalid platform devices\n", __func__);
		return -EINVAL;
	}
	if (!pdev->dev.parent) {
		pr_err("%s err: failed to find a parent for %s\n",
			__func__, dev_name(&pdev->dev));
		return -EINVAL;
	}

	podev = dev_get_drvdata(pdev->dev.parent);
	np = pdev->dev.of_node;
	cb = devm_kzalloc(&pdev->dev, sizeof(*cb), GFP_KERNEL);
	if (!cb) {
		pr_err("%s ERROR = Failed to allocate cb\n", __func__);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&cb->list);
	list_add_tail(&cb->list, &podev->context_banks);

	rc = of_property_read_string(np, "label", &cb->name);
	if (rc)
		pr_debug("%s ERROR = Unable to read label\n", __func__);

	rc = of_property_read_u32(np, "virtual-addr", &cb->start_addr);
	if (rc) {
		pr_err("%s err: cannot read virtual region addr %d\n",
			__func__, rc);
		goto err_setup_cb;
	}

	rc = of_property_read_u32(np, "virtual-size", &cb->size);
	if (rc) {
		pr_err("%s err: cannot read virtual region size %d\n",
			__func__, rc);
		goto err_setup_cb;
	}

	cb->is_secure = of_property_read_bool(np, "qcom,secure-context-bank");

	rc = qcedev_setup_context_bank(cb, &pdev->dev);
	if (rc) {
		pr_err("%s err: cannot setup context bank %d\n", __func__, rc);
		goto err_setup_cb;
	}

	return 0;

err_setup_cb:
	devm_kfree(&pdev->dev, cb);
	list_del(&cb->list);
	return rc;
}
