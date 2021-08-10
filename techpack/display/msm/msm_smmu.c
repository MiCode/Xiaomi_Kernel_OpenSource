/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/msm_dma_iommu_mapping.h>
#include <linux/dma-mapping.h>

#include <soc/qcom/secure_buffer.h>

#include "msm_drv.h"
#include "msm_gem.h"
#include "msm_mmu.h"
#include "sde_dbg.h"

struct msm_smmu_client {
	struct device *dev;
	const char *compat;
	struct iommu_domain *domain;
	const struct dma_map_ops *dma_ops;
	bool domain_attached;
	bool secure;
	struct list_head smmu_list;
};

struct msm_smmu {
	struct msm_mmu base;
	struct device *client_dev;
	struct msm_smmu_client *client;
};

struct msm_smmu_domain {
	const char *label;
	bool secure;
};

#define to_msm_smmu(x) container_of(x, struct msm_smmu, base)
#define msm_smmu_to_client(smmu) (smmu->client)

/* Serialization lock for smmu_list */
static DEFINE_MUTEX(smmu_list_lock);

/* List of all smmu devices installed */
static LIST_HEAD(sde_smmu_list);

static int msm_smmu_attach(struct msm_mmu *mmu, const char * const *names,
		int cnt)
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

	if (client->dma_ops) {
		set_dma_ops(client->dev, client->dma_ops);
		client->dma_ops = NULL;
		dev_dbg(client->dev, "iommu domain ops restored\n");
	}

	rc = iommu_attach_device(client->domain, client->dev);
	if (rc) {
		dev_err(client->dev, "iommu attach dev failed (%d)\n", rc);
		return rc;
	}

	client->domain_attached = true;

	dev_dbg(client->dev, "iommu domain attached\n");

	return 0;
}

static void msm_smmu_detach(struct msm_mmu *mmu, const char * const *names,
		int cnt)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct msm_smmu_client *client = msm_smmu_to_client(smmu);

	if (!client) {
		pr_err("undefined smmu client\n");
		return;
	}

	if (!client->domain_attached)
		return;

	pm_runtime_get_sync(mmu->dev);
	msm_dma_unmap_all_for_dev(client->dev);
	iommu_detach_device(client->domain, client->dev);

	client->dma_ops = get_dma_ops(client->dev);
	if (client->dma_ops) {
		set_dma_ops(client->dev, NULL);
		dev_dbg(client->dev, "iommu domain ops removed\n");
	}

	pm_runtime_put_sync(mmu->dev);

	client->domain_attached = false;
	dev_dbg(client->dev, "iommu domain detached\n");
}

static int msm_smmu_set_attribute(struct msm_mmu *mmu,
		enum iommu_attr attr, void *data)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct msm_smmu_client *client = msm_smmu_to_client(smmu);
	int ret = 0;

	if (!client || !client->domain)
		return -ENODEV;

	ret = iommu_domain_set_attr(client->domain, attr, data);
	if (ret)
		DRM_ERROR("set domain attribute failed:%d\n", ret);

	return ret;
}

static int msm_smmu_one_to_one_unmap(struct msm_mmu *mmu,
				uint32_t dest_address, uint32_t size)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct msm_smmu_client *client = msm_smmu_to_client(smmu);
	int ret = 0;

	if (!client || !client->domain)
		return -ENODEV;

	ret = iommu_unmap(client->domain, dest_address, size);
	if (ret != size)
		pr_err("smmu unmap failed\n");

	return 0;
}

static int msm_smmu_one_to_one_map(struct msm_mmu *mmu, uint32_t iova,
		uint32_t dest_address, uint32_t size, int prot)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct msm_smmu_client *client = msm_smmu_to_client(smmu);
	int ret = 0;

	if (!client || !client->domain)
		return -ENODEV;

	ret = iommu_map(client->domain, dest_address, dest_address,
			size, prot);
	if (ret)
		pr_err("smmu map failed\n");

	return ret;
}

static int msm_smmu_map(struct msm_mmu *mmu, uint64_t iova,
		struct sg_table *sgt, unsigned int len, int prot)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct msm_smmu_client *client = msm_smmu_to_client(smmu);
	size_t ret = 0;

	if (sgt && sgt->sgl) {
		ret = iommu_map_sg(client->domain, iova, sgt->sgl,
				sgt->nents, prot);
		WARN_ON((int)ret < 0);
		DRM_DEBUG("%pad/0x%x/0x%x/\n", &sgt->sgl->dma_address,
				sgt->sgl->dma_length, prot);
		SDE_EVT32(sgt->sgl->dma_address, sgt->sgl->dma_length, prot);
	}
	return (ret == len) ? 0 : -EINVAL;
}

static int msm_smmu_unmap(struct msm_mmu *mmu, uint64_t iova,
		struct sg_table *sgt, unsigned int len)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct msm_smmu_client *client = msm_smmu_to_client(smmu);

	pm_runtime_get_sync(mmu->dev);
	iommu_unmap(client->domain, iova, len);
	pm_runtime_put_sync(mmu->dev);

	return 0;
}

static void msm_smmu_destroy(struct msm_mmu *mmu)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct platform_device *pdev = to_platform_device(smmu->client_dev);

	if (smmu->client_dev)
		platform_device_unregister(pdev);
	kfree(smmu);
}

struct device *msm_smmu_get_dev(struct msm_mmu *mmu)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);

	return smmu->client_dev;
}

static int msm_smmu_map_dma_buf(struct msm_mmu *mmu, struct sg_table *sgt,
		int dir, u32 flags)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct msm_smmu_client *client = msm_smmu_to_client(smmu);
	unsigned long attrs = 0x0;
	int ret;

	if (!sgt || !client) {
		DRM_ERROR("sg table is invalid\n");
		return -ENOMEM;
	}

	if (flags & MSM_BO_KEEPATTRS)
		attrs |= DMA_ATTR_IOMMU_USE_LLC_NWA;

	/*
	 * For import buffer type, dma_map_sg_attrs is called during
	 * dma_buf_map_attachment and is not required to call again
	 */
	if (!(flags & MSM_BO_EXTBUF)) {
		ret = dma_map_sg_attrs(client->dev, sgt->sgl, sgt->nents, dir,
				attrs);
		if (!ret) {
			DRM_ERROR("dma map sg failed\n");
			return -ENOMEM;
		}
	}

	if (sgt && sgt->sgl) {
		DRM_DEBUG("%pad/0x%x/0x%x/0x%lx\n",
				&sgt->sgl->dma_address, sgt->sgl->dma_length,
				dir, attrs);
		SDE_EVT32(sgt->sgl->dma_address, sgt->sgl->dma_length,
				dir, attrs, client->secure, flags);
	}

	return 0;
}


static void msm_smmu_unmap_dma_buf(struct msm_mmu *mmu, struct sg_table *sgt,
		int dir, u32 flags)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct msm_smmu_client *client = msm_smmu_to_client(smmu);

	if (!sgt || !client) {
		DRM_ERROR("sg table is invalid\n");
		return;
	}

	if (sgt->sgl) {
		DRM_DEBUG("%pad/0x%x/0x%x\n",
				&sgt->sgl->dma_address, sgt->sgl->dma_length,
				dir);
		SDE_EVT32(sgt->sgl->dma_address, sgt->sgl->dma_length,
				dir, client->secure, flags);
	}

	if (!(flags & MSM_BO_EXTBUF))
		dma_unmap_sg(client->dev, sgt->sgl, sgt->nents, dir);
}

static bool msm_smmu_is_domain_secure(struct msm_mmu *mmu)
{
	struct msm_smmu *smmu = to_msm_smmu(mmu);
	struct msm_smmu_client *client = msm_smmu_to_client(smmu);

	return client->secure;
}

static const struct msm_mmu_funcs funcs = {
	.attach = msm_smmu_attach,
	.detach = msm_smmu_detach,
	.map = msm_smmu_map,
	.unmap = msm_smmu_unmap,
	.map_dma_buf = msm_smmu_map_dma_buf,
	.unmap_dma_buf = msm_smmu_unmap_dma_buf,
	.destroy = msm_smmu_destroy,
	.is_domain_secure = msm_smmu_is_domain_secure,
	.set_attribute = msm_smmu_set_attribute,
	.one_to_one_map = msm_smmu_one_to_one_map,
	.one_to_one_unmap = msm_smmu_one_to_one_unmap,
	.get_dev = msm_smmu_get_dev,
};

static struct msm_smmu_domain msm_smmu_domains[MSM_SMMU_DOMAIN_MAX] = {
	[MSM_SMMU_DOMAIN_UNSECURE] = {
		.label = "mdp_ns",
		.secure = false,
	},
	[MSM_SMMU_DOMAIN_SECURE] = {
		.label = "mdp_s",
		.secure = true,
	},
	[MSM_SMMU_DOMAIN_NRT_UNSECURE] = {
		.label = "rot_ns",
		.secure = false,
	},
	[MSM_SMMU_DOMAIN_NRT_SECURE] = {
		.label = "rot_s",
		.secure = true,
	},
};

static const struct of_device_id msm_smmu_dt_match[] = {
	{ .compatible = "qcom,smmu_sde_unsec",
		.data = &msm_smmu_domains[MSM_SMMU_DOMAIN_UNSECURE] },
	{ .compatible = "qcom,smmu_sde_sec",
		.data = &msm_smmu_domains[MSM_SMMU_DOMAIN_SECURE] },
	{ .compatible = "qcom,smmu_sde_nrt_unsec",
		.data = &msm_smmu_domains[MSM_SMMU_DOMAIN_NRT_UNSECURE] },
	{ .compatible = "qcom,smmu_sde_nrt_sec",
		.data = &msm_smmu_domains[MSM_SMMU_DOMAIN_NRT_SECURE] },
	{}
};
MODULE_DEVICE_TABLE(of, msm_smmu_dt_match);

static struct msm_smmu_client *msm_smmu_get_smmu(const char *compat)
{
	struct msm_smmu_client *curr = NULL;
	bool found = false;

	if (!compat) {
		pr_err("invalid param\n");
		return ERR_PTR(-EINVAL);
	}

	mutex_lock(&smmu_list_lock);
	list_for_each_entry(curr, &sde_smmu_list, smmu_list) {
		if (of_compat_cmp(compat, curr->compat, strlen(compat)) == 0) {
			DRM_DEBUG("found msm_smmu_client for %s\n", compat);
			found = true;
			break;
		}
	}
	mutex_unlock(&smmu_list_lock);

	if (!found)
		return ERR_PTR(-ENODEV);

	return curr;
}

static struct device *msm_smmu_device_add(struct device *dev,
		enum msm_mmu_domain_type domain,
		struct msm_smmu *smmu)
{
	int i;
	const char *compat = NULL;

	for (i = 0; i < ARRAY_SIZE(msm_smmu_dt_match); i++) {
		if (msm_smmu_dt_match[i].data == &msm_smmu_domains[domain]) {
			compat = msm_smmu_dt_match[i].compatible;
			break;
		}
	}

	if (!compat) {
		DRM_DEBUG("unable to find matching domain for %d\n", domain);
		return ERR_PTR(-ENOENT);
	}
	DRM_DEBUG("found domain %d compat: %s\n", domain, compat);

	smmu->client = msm_smmu_get_smmu(compat);
	if (IS_ERR_OR_NULL(smmu->client)) {
		DRM_ERROR("unable to find domain %d compat: %s\n", domain,
				compat);
		return ERR_PTR(-ENODEV);
	}

	return smmu->client->dev;
}

struct msm_mmu *msm_smmu_new(struct device *dev,
		enum msm_mmu_domain_type domain)
{
	struct msm_smmu *smmu;
	struct device *client_dev;

	smmu = kzalloc(sizeof(*smmu), GFP_KERNEL);
	if (!smmu)
		return ERR_PTR(-ENOMEM);

	client_dev = msm_smmu_device_add(dev, domain, smmu);
	if (IS_ERR_OR_NULL(client_dev)) {
		kfree(smmu);
		return (void *)client_dev ? : ERR_PTR(-ENODEV);
	}

	smmu->client_dev = client_dev;
	msm_mmu_init(&smmu->base, dev, &funcs);

	return &smmu->base;
}

static int msm_smmu_fault_handler(struct iommu_domain *domain,
		struct device *dev, unsigned long iova,
		int flags, void *token)
{
	struct msm_smmu_client *client;
	int rc = -EINVAL;

	if (!token) {
		DRM_ERROR("Error: token is NULL\n");
		return -EINVAL;
	}

	client = (struct msm_smmu_client *)token;

	/* see iommu.h for fault flags definition */
	SDE_EVT32(iova, flags);
	DRM_ERROR("trigger dump, iova=0x%08lx, flags=0x%x\n", iova, flags);
	DRM_ERROR("SMMU device:%s", client->dev ? client->dev->kobj.name : "");

	/*
	 * return -ENOSYS to allow smmu driver to dump out useful
	 * debug info.
	 */
	return rc;
}

/**
 * msm_smmu_bind - bind smmu device with controlling device
 * @dev:        Pointer to base of platform device
 * @master:     Pointer to container of drm device
 * @data:       Pointer to private data
 * Returns:     Zero on success
 */
static int msm_smmu_bind(struct device *dev, struct device *master, void *data)
{
	return 0;
}

/**
 * msm_smmu_unbind - unbind msm_smmu from controlling device
 * @dev:        Pointer to base of platform device
 * @master:     Pointer to container of drm device
 * @data:       Pointer to private data
 */
static void msm_smmu_unbind(struct device *dev,
		struct device *master, void *data)
{
}

static const struct component_ops msm_smmu_comp_ops = {
	.bind = msm_smmu_bind,
	.unbind = msm_smmu_unbind,
};

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
	int ret;

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
	client->domain = iommu_get_domain_for_dev(client->dev);
	if (!client->domain) {
		dev_err(&pdev->dev, "iommu get domain for dev failed\n");
		return -EINVAL;
	}
	client->compat = match->compatible;
	client->secure = domain->secure;
	client->domain_attached = true;

	if (!client->dev->dma_parms)
		client->dev->dma_parms = devm_kzalloc(client->dev,
				sizeof(*client->dev->dma_parms), GFP_KERNEL);
	dma_set_max_seg_size(client->dev, DMA_BIT_MASK(32));
	dma_set_seg_boundary(client->dev, (unsigned long)DMA_BIT_MASK(64));

	iommu_set_fault_handler(client->domain,
			msm_smmu_fault_handler, (void *)client);

	DRM_INFO("Created domain %s, secure=%d\n",
			domain->label, domain->secure);

	platform_set_drvdata(pdev, client);

	mutex_lock(&smmu_list_lock);
	list_add(&client->smmu_list, &sde_smmu_list);
	mutex_unlock(&smmu_list_lock);

	ret = component_add(&pdev->dev, &msm_smmu_comp_ops);
	if (ret)
		pr_err("component add failed\n");

	return ret;
}

static int msm_smmu_remove(struct platform_device *pdev)
{
	struct msm_smmu_client *client;
	struct msm_smmu_client *curr, *next;

	client = platform_get_drvdata(pdev);
	client->domain_attached = false;

	mutex_lock(&smmu_list_lock);
	list_for_each_entry_safe(curr, next, &sde_smmu_list, smmu_list) {
		if (curr == client) {
			list_del(&client->smmu_list);
			break;
		}
	}
	mutex_unlock(&smmu_list_lock);

	return 0;
}

static struct platform_driver msm_smmu_driver = {
	.probe = msm_smmu_probe,
	.remove = msm_smmu_remove,
	.driver = {
		.name = "msmdrm_smmu",
		.of_match_table = msm_smmu_dt_match,
		.suppress_bind_attrs = true,
	},
};

int __init msm_smmu_driver_init(void)
{
	int ret;

	ret = platform_driver_register(&msm_smmu_driver);
	if (ret)
		pr_err("mdss_smmu_register_driver() failed!\n");

	return ret;
}

void __exit msm_smmu_driver_cleanup(void)
{
	platform_driver_unregister(&msm_smmu_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM SMMU driver");
