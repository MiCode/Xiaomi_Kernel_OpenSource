/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/msm_dma_iommu_mapping.h>

#include <asm/dma-iommu.h>
#include <soc/qcom/secure_buffer.h>

#include "msm_drv.h"
#include "msm_mmu.h"

#ifndef SZ_4G
#define SZ_4G	(((size_t) SZ_1G) * 4)
#endif

struct msm_smmu_client {
	struct device *dev;
	struct dma_iommu_mapping *mmu_mapping;
	bool domain_attached;
	const struct msm_smmu_domain *domain;
};

struct msm_smmu {
	struct msm_mmu base;
	struct device *client_dev;
	struct msm_smmu_client *client;
	struct list_head list;
	atomic_long_t mapped;
	uint64_t fault_addr;
	struct drm_mm va_mm;
	struct list_head iova;
};

struct msm_smmu_domain {
	const char *label;
	size_t va_start;
	size_t va_size;
	bool secure;
};

#define to_msm_smmu(x) container_of(x, struct msm_smmu, base)
#define msm_smmu_to_client(smmu) (smmu->client)


static int msm_smmu_fault_handler(struct iommu_domain *iommu,
	 struct device *dev, unsigned long iova, int flags, void *arg)
{

	dev_info(dev, "%s: iova=0x%08lx, flags=0x%x, iommu=%p\n", __func__,
			iova, flags, iommu);
	return 0;
}


static int _msm_smmu_create_mapping(struct msm_smmu_client *client,
	const struct msm_smmu_domain *domain);

static int msm_smmu_attach(struct msm_mmu *mmu, const char **names, int cnt)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct msm_smmu_client *client = msm_smmu_to_client(smmu);
	int rc = 0;

	if (!client) {
		pr_err("undefined smmu client\n");
		return -EINVAL;
	}

	/* domain attach only once */
	if (client->domain_attached)
		return 0;

	rc = arm_iommu_attach_device(client->dev,
			client->mmu_mapping);
	if (rc) {
		dev_err(client->dev, "iommu attach dev failed (%d)\n",
				rc);
		return rc;
	}

	client->domain_attached = true;

	dev_dbg(client->dev, "iommu domain attached\n");

	return 0;
}

static void msm_smmu_detach(struct msm_mmu *mmu, const char **names, int cnt)
{
	DBG("detaching");
}

static int msm_smmu_map(struct msm_mmu *mmu, dma_addr_t iova,
		struct sg_table *sgt, int prot)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct msm_smmu_client *client = msm_smmu_to_client(smmu);
	struct iommu_domain *domain;
	struct scatterlist *sg;
	dma_addr_t da = iova;
	unsigned int i, j;
	int ret;
	size_t bytes;

	if (!client)
		return -ENODEV;

	domain = client->mmu_mapping->domain;
	if (!domain || !sgt)
		return -EINVAL;

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		dma_addr_t pa = sg_phys(sg) - sg->offset;
		size_t bytes = sg->length + sg->offset;

		ret = iommu_map(domain, da, pa, bytes, prot);
		if (ret)
			goto fail;

		da += bytes;
	}

	return 0;

fail:
	da = iova;

	for_each_sg(sgt->sgl, sg, i, j) {
		bytes = sg->length + sg->offset;
		iommu_unmap(domain, da, bytes);
		da += bytes;
	}
	return ret;
}

static int msm_smmu_map_sg(struct msm_mmu *mmu, struct sg_table *sgt,
		enum dma_data_direction dir)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct msm_smmu_client *client = msm_smmu_to_client(smmu);
	int ret;

	ret = dma_map_sg(client->dev, sgt->sgl, sgt->nents, dir);
	if (ret != sgt->nents)
		return -ENOMEM;

	return 0;
}

static void msm_smmu_unmap_sg(struct msm_mmu *mmu, struct sg_table *sgt,
		enum dma_data_direction dir)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct msm_smmu_client *client = msm_smmu_to_client(smmu);

	dma_unmap_sg(client->dev, sgt->sgl, sgt->nents, dir);
}

static int msm_smmu_unmap(struct msm_mmu *mmu, dma_addr_t iova,
		struct sg_table *sgt)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct msm_smmu_client *client = msm_smmu_to_client(smmu);
	struct iommu_domain *domain;
	struct scatterlist *sg;
	dma_addr_t da = iova;
	int i;

	if (!client)
		return -ENODEV;

	domain = client->mmu_mapping->domain;
	if (!domain || !sgt)
		return -EINVAL;

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		size_t bytes = sg->length + sg->offset;
		size_t unmapped;

		unmapped = iommu_unmap(domain, da, bytes);
		if (unmapped < bytes)
			return unmapped;

		WARN_ON(!PAGE_ALIGNED(bytes));

		da += bytes;
	}

	return 0;
}

static void msm_smmu_destroy(struct msm_mmu *mmu)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct platform_device *pdev = to_platform_device(smmu->client_dev);

	platform_device_unregister(pdev);
	kfree(smmu);
}

static int msm_smmu_map_dma_buf(struct msm_mmu *mmu, struct sg_table *sgt,
			struct dma_buf *dma_buf, int dir)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct msm_smmu_client *client = msm_smmu_to_client(smmu);
	int ret;

	ret = msm_dma_map_sg_lazy(client->dev, sgt->sgl, sgt->nents, dir,
			dma_buf);
	if (ret != sgt->nents) {
		DRM_ERROR("dma map sg failed\n");
		return -ENOMEM;
	}

	return 0;
}

static void msm_smmu_unmap_dma_buf(struct msm_mmu *mmu, struct sg_table *sgt,
			struct dma_buf *dma_buf, int dir)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct msm_smmu_client *client = msm_smmu_to_client(smmu);

	msm_dma_unmap_sg(client->dev, sgt->sgl, sgt->nents, dir, dma_buf);
}

static const struct msm_mmu_funcs funcs = {
	.attach = msm_smmu_attach,
	.detach = msm_smmu_detach,
	.map = msm_smmu_map,
	.map_sg = msm_smmu_map_sg,
	.unmap_sg = msm_smmu_unmap_sg,
	.unmap = msm_smmu_unmap,
	.map_dma_buf = msm_smmu_map_dma_buf,
	.unmap_dma_buf = msm_smmu_unmap_dma_buf,
	.destroy = msm_smmu_destroy,
};

static struct msm_smmu_domain msm_smmu_domains[MSM_SMMU_DOMAIN_MAX] = {
	[MSM_SMMU_DOMAIN_UNSECURE] = {
		.label = "mdp_ns",
		.va_start = SZ_128K,
		.va_size = SZ_4G - SZ_128K,
		.secure = false,
	},
	[MSM_SMMU_DOMAIN_SECURE] = {
		.label = "mdp_s",
		.va_start = 0,
		.va_size = SZ_4G,
		.secure = true,
	},
	[MSM_SMMU_DOMAIN_NRT_UNSECURE] = {
		.label = "rot_ns",
		.va_start = SZ_128K,
		.va_size = SZ_4G - SZ_128K,
		.secure = false,
	},
	[MSM_SMMU_DOMAIN_NRT_SECURE] = {
		.label = "rot_s",
		.va_start = 0,
		.va_size = SZ_4G,
		.secure = true,
	},
	[MSM_SMMU_DOMAIN_GPU_UNSECURE] = {
		.label = "gpu",
		.va_start = SZ_16M,
		.va_size = 0xFFFFFFFF - SZ_16M,
	},
};

static const struct of_device_id msm_smmu_dt_match[] = {
	{ .compatible = "qcom,smmu_kms_unsec",
		.data = &msm_smmu_domains[MSM_SMMU_DOMAIN_UNSECURE] },
	{ .compatible = "qcom,smmu_kms_sec",
		.data = &msm_smmu_domains[MSM_SMMU_DOMAIN_SECURE] },
	{ .compatible = "qcom,smmu_nrt_unsec",
		.data = &msm_smmu_domains[MSM_SMMU_DOMAIN_NRT_UNSECURE] },
	{ .compatible = "qcom,smmu_nrt_sec",
		.data = &msm_smmu_domains[MSM_SMMU_DOMAIN_NRT_SECURE] },
	{ .compatible = "qcom,kgsl-smmu-v2",
		.data = &msm_smmu_domains[MSM_SMMU_DOMAIN_GPU_UNSECURE] },
	{}
};
MODULE_DEVICE_TABLE(of, msm_smmu_dt_match);

static struct device *msm_smmu_device_create(struct device *dev,
		enum msm_mmu_domain_type domain,
		struct msm_smmu *smmu)
{
	struct device_node *child;
	struct platform_device *pdev;
	int i;
	const char *compat = NULL;
	int rc;

	for (i = 0; i < ARRAY_SIZE(msm_smmu_dt_match); i++) {
		if (msm_smmu_dt_match[i].data == &msm_smmu_domains[domain]) {
			compat = msm_smmu_dt_match[i].compatible;
			break;
		}
	}

	if (!compat) {
		DRM_ERROR("unable to find matching domain for %d\n", domain);
		return ERR_PTR(-ENOENT);
	}
	DRM_INFO("found domain %d compat: %s\n", domain, compat);

	if ((domain == MSM_SMMU_DOMAIN_UNSECURE) ||
		(domain == MSM_SMMU_DOMAIN_GPU_UNSECURE)) {

		smmu->client = devm_kzalloc(dev,
				sizeof(struct msm_smmu_client), GFP_KERNEL);
		if (!smmu->client)
			return ERR_PTR(-ENOMEM);

		smmu->client->dev = dev;

		rc = _msm_smmu_create_mapping(msm_smmu_to_client(smmu),
			msm_smmu_dt_match[i].data);
		if (rc) {
			devm_kfree(dev, smmu->client);
			smmu->client = NULL;
			return ERR_PTR(rc);
		}

		return NULL;
	}

	child = of_find_compatible_node(dev->of_node, NULL, compat);
	if (!child) {
		DRM_ERROR("unable to find compatible node for %s\n", compat);
		return ERR_PTR(-ENODEV);
	}

	pdev = of_platform_device_create(child, NULL, dev);
	if (!pdev) {
		DRM_ERROR("unable to create smmu platform dev for domain %d\n",
				domain);
		return ERR_PTR(-ENODEV);
	}

	smmu->client = platform_get_drvdata(pdev);

	return &pdev->dev;
}

struct msm_mmu *msm_smmu_new(struct drm_device *drm_dev,
	struct device *dev, enum msm_mmu_domain_type domain)
{
	struct msm_smmu *smmu;
	struct device *client_dev;
	struct msm_smmu_client *client;
	struct msm_drm_private *dpriv = drm_dev->dev_private;
	static uint32_t name[MSM_SMMU_DOMAIN_MAX] = {
		MSM_MMU_UNSECURE,
		MSM_MMU_NRT_UNSECURE,
		MSM_MMU_SECURE,
		MSM_MMU_NRT_SECURE,
		MSM_MMU_GPU_UNSECURE,
		MSM_MMU_GPU_SECURE
	};

	smmu = kzalloc(sizeof(*smmu), GFP_KERNEL);
	if (!smmu)
		return ERR_PTR(-ENOMEM);

	client_dev = msm_smmu_device_create(dev, domain, smmu);
	if (IS_ERR(client_dev)) {
		kfree(smmu);
		return (void *)client_dev ? : ERR_PTR(-ENODEV);
	}

	smmu->client_dev = client_dev;

	smmu->fault_addr = ~0ULL;

	INIT_LIST_HEAD(&smmu->iova);
	atomic_long_set(&smmu->mapped, 0);

	msm_mmu_init(&smmu->base, drm_dev, dev, name[domain], &funcs);
	client = msm_smmu_to_client(smmu);
	if (client) {
		iommu_set_fault_handler(client->mmu_mapping->domain,
				msm_smmu_fault_handler, dev);

		drm_mm_init(&smmu->va_mm, client->domain->va_start,
				client->domain->va_size);
	}

	dpriv->mmus[domain] = &smmu->base;

	return &smmu->base;
}

static int _msm_smmu_create_mapping(struct msm_smmu_client *client,
	const struct msm_smmu_domain *domain)
{
	int disable_htw = 1;
	int rc;

	client->mmu_mapping = arm_iommu_create_mapping(&platform_bus_type,
			domain->va_start, domain->va_size);
	if (IS_ERR(client->mmu_mapping)) {
		dev_err(client->dev,
			"iommu create mapping failed for domain=%s\n",
			domain->label);
		return PTR_ERR(client->mmu_mapping);
	}

	rc = iommu_domain_set_attr(client->mmu_mapping->domain,
			DOMAIN_ATTR_COHERENT_HTW_DISABLE, &disable_htw);
	if (rc) {
		dev_err(client->dev, "couldn't disable coherent HTW\n");
		goto error;
	}

	if (domain->secure) {
		int secure_vmid = VMID_CP_PIXEL;

		rc = iommu_domain_set_attr(client->mmu_mapping->domain,
				DOMAIN_ATTR_SECURE_VMID, &secure_vmid);
		if (rc) {
			dev_err(client->dev, "couldn't set secure pix vmid\n");
			goto error;
		}
	}
	client->domain = domain;

	return 0;

error:
	arm_iommu_release_mapping(client->mmu_mapping);
	return rc;
}

/**
 * msm_smmu_probe()
 * @pdev: platform device
 *
 * Each smmu context acts as a separate device and the context banks are
 * configured with a VA range.
 * Registers the clks as each context bank has its own clks, for which voting
 * has to be done everytime before using that context bank.
 */
static int msm_smmu_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct msm_smmu_client *client;
	const struct msm_smmu_domain *domain;
	int rc;

	match = of_match_device(msm_smmu_dt_match, &pdev->dev);
	if (!match || !match->data) {
		dev_err(&pdev->dev, "probe failed as match data is invalid\n");
		return -EINVAL;
	}

	domain = match->data;
	if (!domain) {
		dev_err(&pdev->dev, "no matching device found\n");
		return -EINVAL;
	}

	DRM_INFO("probing device %s\n", match->compatible);

	client = devm_kzalloc(&pdev->dev, sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	client->dev = &pdev->dev;

	rc = _msm_smmu_create_mapping(client, domain);
	platform_set_drvdata(pdev, client);

	return rc;
}

static int msm_smmu_remove(struct platform_device *pdev)
{
	struct msm_smmu_client *client;

	client = platform_get_drvdata(pdev);
	if (client->domain_attached) {
		arm_iommu_detach_device(client->dev);
		client->domain_attached = false;
	}
	arm_iommu_release_mapping(client->mmu_mapping);

	return 0;
}

static struct platform_driver msm_smmu_driver = {
	.probe = msm_smmu_probe,
	.remove = msm_smmu_remove,
	.driver = {
		.name = "msmdrm_smmu",
		.of_match_table = msm_smmu_dt_match,
	},
};

static int __init msm_smmu_driver_init(void)
{
	int ret;

	ret = platform_driver_register(&msm_smmu_driver);
	if (ret)
		pr_err("mdss_smmu_register_driver() failed!\n");

	return ret;
}
module_init(msm_smmu_driver_init);

static void __exit msm_smmu_driver_cleanup(void)
{
	platform_driver_unregister(&msm_smmu_driver);
}
module_exit(msm_smmu_driver_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM SMMU driver");
