// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/iommu.h>
#include <linux/slab.h>

static DEFINE_MUTEX(iommu_debug_attachments_lock);
static LIST_HEAD(iommu_debug_attachments);

/*
 * Each group may have more than one domain; but each domain may
 * only have one group.
 * Used by debug tools to display the name of the device(s) associated
 * with a particular domain.
 */
struct iommu_debug_attachment {
	struct iommu_domain *domain;
	struct iommu_group *group;
	struct list_head list;
};

static struct iommu_debug_attachment *iommu_logger_init(
						struct iommu_domain *domain,
						struct iommu_group *group)
{
	struct iommu_debug_attachment *logger;

	logger = kzalloc(sizeof(*logger), GFP_KERNEL);
	if (!logger)
		return NULL;

	INIT_LIST_HEAD(&logger->list);
	logger->domain = domain;
	logger->group = group;

	return logger;
}

int iommu_logger_register(struct iommu_debug_attachment **logger_out,
			  struct iommu_domain *domain,
			  struct iommu_group *group)
{
	struct iommu_debug_attachment *logger;

	if (!logger_out)
		return -EINVAL;

	logger = iommu_logger_init(domain, group);
	if (!logger)
		return -ENOMEM;

	mutex_lock(&iommu_debug_attachments_lock);
	list_add(&logger->list, &iommu_debug_attachments);
	mutex_unlock(&iommu_debug_attachments_lock);

	*logger_out = logger;
	return 0;
}
EXPORT_SYMBOL(iommu_logger_register);

void iommu_logger_unregister(struct iommu_debug_attachment *logger)
{
	if (!logger)
		return;

	mutex_lock(&iommu_debug_attachments_lock);
	list_del(&logger->list);
	mutex_unlock(&iommu_debug_attachments_lock);
	kfree(logger);
}
EXPORT_SYMBOL(iommu_logger_unregister);

MODULE_DESCRIPTION("QTI IOMMU SUPPORT");
MODULE_LICENSE("GPL v2");
