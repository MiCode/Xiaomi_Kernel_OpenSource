/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#include <linux/iommu.h>
#include <asm/dma-iommu.h>
#include <linux/platform_device.h>

#include "nvme-qcom.h"

static const struct of_device_id nvme_matches[] = {
	{ .compatible = "qcom,nvme" },
};

static struct dma_iommu_mapping *mapping;

static int nvme_qcom_parse_smmu_attr(struct device *dev,
				    struct iommu_domain *domain,
				    const char *key,
				    enum iommu_attr attr)
{
	int rc = 0;
	unsigned int data = 1;

	if (of_find_property(dev->of_node, key, NULL)) {
		rc = iommu_domain_set_attr(domain, attr, &data);
		if (!rc)
			dev_dbg(dev, "enabled SMMU attribute %u\n", attr);
		else
			dev_err(dev, "failed to set SMMU attribute %u\n", attr);
	}

	return rc;
}

static int nvme_qcom_parse_smmu_attrs(struct device *dev,
				     struct iommu_domain *domain)
{
	int rc = 0;

	rc |= nvme_qcom_parse_smmu_attr(dev, domain,
		"qcom,smmu-attr-s1-bypass", DOMAIN_ATTR_S1_BYPASS);

	rc |= nvme_qcom_parse_smmu_attr(dev, domain,
		"qcom,smmu-attr-fastmap", DOMAIN_ATTR_FAST);

	rc |= nvme_qcom_parse_smmu_attr(dev, domain,
		"qcom,smmu-attr-atomic", DOMAIN_ATTR_ATOMIC);

	rc |= nvme_qcom_parse_smmu_attr(dev, domain,
		"qcom,smmu-attr-pt-coherent",
		DOMAIN_ATTR_PAGE_TABLE_IS_COHERENT);

	rc |= nvme_qcom_parse_smmu_attr(dev, domain,
		"qcom,smmu-attr-pt-coherent-force",
		DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT);

	return rc;
}

static int nvme_qcom_parse_smmu(struct device *dev)
{
	int rc;
	const char *key;
	u64 val64;
	dma_addr_t iova_base = 0;
	u64 iova_size = 0;

	key = "qcom,smmu-iova-base";
	rc = of_property_read_u64(dev->of_node, key, &val64);
	if (rc) {
		dev_err(dev, "error parsing DT prop %s: %d\n", key, rc);
		return rc;
	}
	iova_base = (dma_addr_t)val64;

	key = "qcom,smmu-iova-size";
	rc = of_property_read_u64(dev->of_node, key, &val64);
	if (rc) {
		dev_err(dev, "error parsing DT prop %s: %d\n", key, rc);
		return rc;
	}
	iova_size = val64;
	dev_dbg(dev, "creating SMMU mapping with base=0x%llx, size=0x%llx\n",
		iova_base, iova_size);

	mapping = arm_iommu_create_mapping(dev->bus, iova_base, iova_size);
	if (IS_ERR(mapping)) {
		dev_err(dev, "failed to create SMMU mapping\n");
		return PTR_ERR(mapping);
	}

	rc = nvme_qcom_parse_smmu_attrs(dev, mapping->domain);
	if (rc) {
		dev_err(dev, "error parsing SMMU DT attributes\n");
		goto err_release_mapping;
	}

	rc = arm_iommu_attach_device(dev, mapping);
	if (rc) {
		dev_err(dev, "failed to attach device to smmu\n");
		goto err_release_mapping;
	}

	return 0;

err_release_mapping:
	arm_iommu_release_mapping(mapping);
	return rc;
}

int nvme_qcom_parse_dt(struct device *dev)
{
	int rc = 0;

	if (!dev->of_node) {
		dev_notice(dev, "device tree node is not present\n");
		return -EINVAL;
	}

	if (!of_match_node(nvme_matches, dev->of_node)) {
		dev_notice(dev, "device tree node is not compatible\n");
		return -EINVAL;
	}

	if (of_find_property(dev->of_node, "qcom,smmu", NULL))
		rc = nvme_qcom_parse_smmu(dev);
	else
		dev_dbg(dev, "SMMU config not present in DT\n");

	return rc;
}

void nvme_qcom_release_mapping(struct device *dev)
{
	arm_iommu_detach_device(dev);
	arm_iommu_release_mapping(mapping);
}
