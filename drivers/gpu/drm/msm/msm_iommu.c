/*
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

#include <linux/of_platform.h>
#include <linux/of_address.h>
#include "msm_drv.h"
#include "msm_iommu.h"

static int msm_fault_handler(struct iommu_domain *iommu, struct device *dev,
		unsigned long iova, int flags, void *arg)
{
	pr_warn_ratelimited("*** fault: iova=%16llX, flags=%d\n", (u64) iova, flags);
	return 0;
}

/*
 * Get and enable the IOMMU clocks so that we can make
 * sure they stay on the entire duration so that we can
 * safely change the pagetable from the GPU
 */
static void _get_iommu_clocks(struct msm_mmu *mmu, struct platform_device *pdev)
{
	struct msm_iommu *iommu = to_msm_iommu(mmu);
	struct device *dev;
	struct property *prop;
	const char *name;
	int i = 0;

	if (WARN_ON(!pdev))
		return;

	dev = &pdev->dev;

	iommu->nr_clocks =
		of_property_count_strings(dev->of_node, "clock-names");

	if (iommu->nr_clocks < 0) {
		iommu->nr_clocks = 0;
		return;
	}

	if (WARN_ON(iommu->nr_clocks > ARRAY_SIZE(iommu->clocks)))
		iommu->nr_clocks = ARRAY_SIZE(iommu->clocks);

	of_property_for_each_string(dev->of_node, "clock-names", prop, name) {
		if (i == iommu->nr_clocks)
			break;

		iommu->clocks[i] =  clk_get(dev, name);
		if (iommu->clocks[i])
			clk_prepare_enable(iommu->clocks[i]);

		i++;
	}
}

static int _attach_iommu_device(struct msm_mmu *mmu,
		struct iommu_domain *domain, const char **names, int cnt)
{
	int i;

	/* See if there is a iommus member in the current device.  If not, look
	 * for the names and see if there is one in there.
	 */

	if (of_find_property(mmu->dev->of_node, "iommus", NULL))
		return iommu_attach_device(domain, mmu->dev);

	/* Look through the list of names for a target */
	for (i = 0; i < cnt; i++) {
		struct device_node *node =
			of_find_node_by_name(mmu->dev->of_node, names[i]);

		if (!node)
			continue;

		if (of_find_property(node, "iommus", NULL)) {
			struct platform_device *pdev;

			/* Get the platform device for the node */
			of_platform_populate(node->parent, NULL, NULL,
				mmu->dev);

			pdev = of_find_device_by_node(node);

			if (!pdev)
				continue;

			_get_iommu_clocks(mmu,
				of_find_device_by_node(node->parent));

			mmu->dev = &pdev->dev;

			return iommu_attach_device(domain, mmu->dev);
		}
	}

	dev_err(mmu->dev, "Couldn't find a IOMMU device\n");
	return -ENODEV;
}

static int msm_iommu_attach(struct msm_mmu *mmu, const char **names, int cnt)
{
	struct msm_iommu *iommu = to_msm_iommu(mmu);
	int val = 1, ret;

	/* Hope springs eternal */
	iommu->allow_dynamic = true;

	/* per-instance pagetables need TTBR1 support in the IOMMU driver */
	ret = iommu_domain_set_attr(iommu->domain,
		DOMAIN_ATTR_ENABLE_TTBR1, &val);
	if (ret)
		iommu->allow_dynamic = false;

	/* Mark the GPU as I/O coherent if it is supported */
	iommu->is_coherent = of_dma_is_coherent(mmu->dev->of_node);

	/* Attach the device to the domain */
	ret = _attach_iommu_device(mmu, iommu->domain, names, cnt);
	if (ret)
		return ret;

	/*
	 * Get the context bank for the base domain; this will be shared with
	 * the children.
	 */
	iommu->cb = -1;
	if (iommu_domain_get_attr(iommu->domain, DOMAIN_ATTR_CONTEXT_BANK,
		&iommu->cb))
		iommu->allow_dynamic = false;

	return 0;
}

static int msm_iommu_attach_dynamic(struct msm_mmu *mmu, const char **names,
		int cnt)
{
	static unsigned int procid;
	struct msm_iommu *iommu = to_msm_iommu(mmu);
	int ret;
	unsigned int id;

	/* Assign a unique procid for the domain to cut down on TLB churn */
	id = ++procid;

	iommu_domain_set_attr(iommu->domain, DOMAIN_ATTR_PROCID, &id);

	ret = iommu_attach_device(iommu->domain, mmu->dev);
	if (ret)
		return ret;

	/*
	 * Get the TTBR0 and the CONTEXTIDR - these will be used by the GPU to
	 * switch the pagetable on its own.
	 */
	iommu_domain_get_attr(iommu->domain, DOMAIN_ATTR_TTBR0,
		&iommu->ttbr0);
	iommu_domain_get_attr(iommu->domain, DOMAIN_ATTR_CONTEXTIDR,
		&iommu->contextidr);

	return 0;
}

static void msm_iommu_detach(struct msm_mmu *mmu)
{
	struct msm_iommu *iommu = to_msm_iommu(mmu);
	int i;

	iommu_detach_device(iommu->domain, mmu->dev);

	for (i = 0; i < iommu->nr_clocks; i++) {
		if (iommu->clocks[i])
			clk_disable(iommu->clocks[i]);
	}
}

static void msm_iommu_detach_dynamic(struct msm_mmu *mmu)
{
	struct msm_iommu *iommu = to_msm_iommu(mmu);
	iommu_detach_device(iommu->domain, mmu->dev);
}

static int msm_iommu_map(struct msm_mmu *mmu, uint64_t iova,
		struct sg_table *sgt, int prot)
{
	struct msm_iommu *iommu = to_msm_iommu(mmu);
	struct iommu_domain *domain = iommu->domain;
	struct scatterlist *sg;
	uint64_t da = iova;
	unsigned int i, j;
	int ret;

	if (!domain || !sgt)
		return -EINVAL;

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		phys_addr_t pa = sg_phys(sg) - sg->offset;
		size_t bytes = sg->length + sg->offset;

		VERB("map[%d]: %016llx %pa(%zx)", i, iova, &pa, bytes);

		ret = iommu_map(domain, da, pa, bytes, prot);
		if (ret)
			goto fail;

		da += bytes;
	}

	return 0;

fail:
	da = iova;

	for_each_sg(sgt->sgl, sg, i, j) {
		size_t bytes = sg->length + sg->offset;
		iommu_unmap(domain, da, bytes);
		da += bytes;
	}
	return ret;
}

static int msm_iommu_unmap(struct msm_mmu *mmu, uint64_t iova,
		struct sg_table *sgt)
{
	struct msm_iommu *iommu = to_msm_iommu(mmu);
	struct iommu_domain *domain = iommu->domain;
	struct scatterlist *sg;
	uint64_t da = iova;
	int i;

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		size_t bytes = sg->length + sg->offset;
		size_t unmapped;

		unmapped = iommu_unmap(domain, da, bytes);
		if (unmapped < bytes)
			return unmapped;

		VERB("unmap[%d]: %016llx(%zx)", i, iova, bytes);

		BUG_ON(!PAGE_ALIGNED(bytes));

		da += bytes;
	}

	return 0;
}

static void msm_iommu_destroy(struct msm_mmu *mmu)
{
	struct msm_iommu *iommu = to_msm_iommu(mmu);
	iommu_domain_free(iommu->domain);
	kfree(iommu);
}

static const struct msm_mmu_funcs funcs = {
		.attach = msm_iommu_attach,
		.detach = msm_iommu_detach,
		.map = msm_iommu_map,
		.unmap = msm_iommu_unmap,
		.destroy = msm_iommu_destroy,
};

static const struct msm_mmu_funcs dynamic_funcs = {
		.attach = msm_iommu_attach_dynamic,
		.detach = msm_iommu_detach_dynamic,
		.map = msm_iommu_map,
		.unmap = msm_iommu_unmap,
		.destroy = msm_iommu_destroy,
};

struct msm_mmu *_msm_iommu_new(struct device *dev, struct iommu_domain *domain,
		const struct msm_mmu_funcs *funcs)
{
	struct msm_iommu *iommu;

	iommu = kzalloc(sizeof(*iommu), GFP_KERNEL);
	if (!iommu)
		return ERR_PTR(-ENOMEM);

	iommu->domain = domain;
	msm_mmu_init(&iommu->base, dev, funcs);
	iommu_set_fault_handler(domain, msm_fault_handler, dev);

	return &iommu->base;
}
struct msm_mmu *msm_iommu_new(struct device *dev, struct iommu_domain *domain)
{
	return _msm_iommu_new(dev, domain, &funcs);
}

/*
 * Given a base domain that is attached to a IOMMU device try to create a
 * dynamic domain that is also attached to the same device but allocates a new
 * pagetable. This is used to allow multiple pagetables to be attached to the
 * same device.
 */
struct msm_mmu *msm_iommu_new_dynamic(struct msm_mmu *base)
{
	struct msm_iommu *base_iommu = to_msm_iommu(base);
	struct iommu_domain *domain;
	struct msm_mmu *mmu;
	int ret, val = 1;
	struct msm_iommu *child_iommu;

	/* Don't continue if the base domain didn't have the support we need */
	if (!base || base_iommu->allow_dynamic == false)
		return ERR_PTR(-EOPNOTSUPP);

	domain = iommu_domain_alloc(&platform_bus_type);
	if (!domain)
		return ERR_PTR(-ENODEV);

	mmu = _msm_iommu_new(base->dev, domain, &dynamic_funcs);

	if (IS_ERR(mmu)) {
		if (domain)
			iommu_domain_free(domain);
		return mmu;
	}

	ret = iommu_domain_set_attr(domain, DOMAIN_ATTR_DYNAMIC, &val);
	if (ret) {
		msm_iommu_destroy(mmu);
		return ERR_PTR(ret);
	}

	/* Set the context bank to match the base domain */
	iommu_domain_set_attr(domain, DOMAIN_ATTR_CONTEXT_BANK,
		&base_iommu->cb);

	/* Mark the dynamic domain as I/O coherent if the base domain is */
	child_iommu = to_msm_iommu(mmu);
	child_iommu->is_coherent = base_iommu->is_coherent;

	return mmu;
}
