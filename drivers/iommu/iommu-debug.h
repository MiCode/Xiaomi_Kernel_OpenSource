#ifndef IOMMU_DEBUG_H
#define IOMMU_DEBUG_H

#ifdef CONFIG_IOMMU_DEBUG_TRACKING

void iommu_debug_attach_device(struct iommu_domain *domain, struct device *dev);
void iommu_debug_detach_device(struct iommu_domain *domain, struct device *dev);
void iommu_debug_domain_add(struct iommu_domain *domain);
void iommu_debug_domain_remove(struct iommu_domain *domain);

#else  /* !CONFIG_IOMMU_DEBUG_TRACKING */

static inline void iommu_debug_attach_device(struct iommu_domain *domain,
					     struct device *dev)
{
}

static inline void iommu_debug_detach_device(struct iommu_domain *domain,
					     struct device *dev)
{
}

static inline void iommu_debug_domain_add(struct iommu_domain *domain)
{
}

static inline void iommu_debug_domain_remove(struct iommu_domain *domain)
{
}

#endif  /* CONFIG_IOMMU_DEBUG_TRACKING */

#endif /* IOMMU_DEBUG_H */
