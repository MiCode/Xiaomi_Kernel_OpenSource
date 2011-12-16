/*
 * Copyright (C) 2007-2008 Advanced Micro Devices, Inc.
 * Author: Joerg Roedel <joerg.roedel@amd.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/bug.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/iommu.h>
#include <linux/scatterlist.h>

static struct iommu_ops *iommu_ops;

void register_iommu(struct iommu_ops *ops)
{
	if (iommu_ops)
		BUG();

	iommu_ops = ops;
}

bool iommu_found(void)
{
	return iommu_ops != NULL;
}
EXPORT_SYMBOL_GPL(iommu_found);

/**
 * iommu_set_fault_handler() - set a fault handler for an iommu domain
 * @domain: iommu domain
 * @handler: fault handler
 *
 * This function should be used by IOMMU users which want to be notified
 * whenever an IOMMU fault happens.
 *
 * The fault handler itself should return 0 on success, and an appropriate
 * error code otherwise.
 */
void iommu_set_fault_handler(struct iommu_domain *domain,
					iommu_fault_handler_t handler)
{
	BUG_ON(!domain);

	domain->handler = handler;
}
EXPORT_SYMBOL_GPL(iommu_set_fault_handler);

struct iommu_domain *iommu_domain_alloc(int flags)
{
	struct iommu_domain *domain;
	int ret;

	if (!iommu_found())
		return NULL;

	domain = kzalloc(sizeof(*domain), GFP_KERNEL);
	if (!domain)
		return NULL;

	ret = iommu_ops->domain_init(domain, flags);
	if (ret)
		goto out_free;

	return domain;

out_free:
	kfree(domain);

	return NULL;
}
EXPORT_SYMBOL_GPL(iommu_domain_alloc);

void iommu_domain_free(struct iommu_domain *domain)
{
	if (!iommu_found())
		return;

	iommu_ops->domain_destroy(domain);
	kfree(domain);
}
EXPORT_SYMBOL_GPL(iommu_domain_free);

int iommu_attach_device(struct iommu_domain *domain, struct device *dev)
{
	if (!iommu_found())
		return -ENODEV;

	return iommu_ops->attach_dev(domain, dev);
}
EXPORT_SYMBOL_GPL(iommu_attach_device);

void iommu_detach_device(struct iommu_domain *domain, struct device *dev)
{
	if (!iommu_found())
		return;

	iommu_ops->detach_dev(domain, dev);
}
EXPORT_SYMBOL_GPL(iommu_detach_device);

phys_addr_t iommu_iova_to_phys(struct iommu_domain *domain,
			       unsigned long iova)
{
	if (!iommu_found())
		return 0;

	return iommu_ops->iova_to_phys(domain, iova);
}
EXPORT_SYMBOL_GPL(iommu_iova_to_phys);

int iommu_domain_has_cap(struct iommu_domain *domain,
			 unsigned long cap)
{
	if (!iommu_found())
		return -ENODEV;

	return iommu_ops->domain_has_cap(domain, cap);
}
EXPORT_SYMBOL_GPL(iommu_domain_has_cap);

int iommu_map(struct iommu_domain *domain, unsigned long iova,
	      phys_addr_t paddr, int gfp_order, int prot)
{
	unsigned long invalid_mask;
	size_t size;

	if (!iommu_found())
		return -ENODEV;

	size         = 0x1000UL << gfp_order;
	invalid_mask = size - 1;

	BUG_ON((iova | paddr) & invalid_mask);

	return iommu_ops->map(domain, iova, paddr, gfp_order, prot);
}
EXPORT_SYMBOL_GPL(iommu_map);

int iommu_unmap(struct iommu_domain *domain, unsigned long iova, int gfp_order)
{
	unsigned long invalid_mask;
	size_t size;

	if (!iommu_found())
		return -ENODEV;

	size         = 0x1000UL << gfp_order;
	invalid_mask = size - 1;

	BUG_ON(iova & invalid_mask);

	return iommu_ops->unmap(domain, iova, gfp_order);
}
EXPORT_SYMBOL_GPL(iommu_unmap);

int iommu_map_range(struct iommu_domain *domain, unsigned int iova,
		    struct scatterlist *sg, unsigned int len, int prot)
{
	if (!iommu_found())
		return -ENODEV;

	BUG_ON(iova & (~PAGE_MASK));

	return iommu_ops->map_range(domain, iova, sg, len, prot);
}
EXPORT_SYMBOL_GPL(iommu_map_range);

int iommu_unmap_range(struct iommu_domain *domain, unsigned int iova,
		      unsigned int len)
{
	if (!iommu_found())
		return -ENODEV;

	BUG_ON(iova & (~PAGE_MASK));

	return iommu_ops->unmap_range(domain, iova, len);
}
EXPORT_SYMBOL_GPL(iommu_unmap_range);

phys_addr_t iommu_get_pt_base_addr(struct iommu_domain *domain)
{
	if (!iommu_found())
		return 0;

	return iommu_ops->get_pt_base_addr(domain);
}
EXPORT_SYMBOL_GPL(iommu_get_pt_base_addr);
