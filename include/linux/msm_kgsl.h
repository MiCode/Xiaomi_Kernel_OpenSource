#ifndef _MSM_KGSL_H
#define _MSM_KGSL_H

#include <uapi/linux/msm_kgsl.h>

#define KGSL_3D0_REG_MEMORY	"kgsl_3d0_reg_memory"
#define KGSL_3D0_IRQ		"kgsl_3d0_irq"
#define KGSL_2D0_REG_MEMORY	"kgsl_2d0_reg_memory"
#define KGSL_2D0_IRQ		"kgsl_2d0_irq"
#define KGSL_2D1_REG_MEMORY	"kgsl_2d1_reg_memory"
#define KGSL_2D1_IRQ		"kgsl_2d1_irq"

enum kgsl_iommu_context_id {
	KGSL_IOMMU_CONTEXT_USER = 0,
	KGSL_IOMMU_CONTEXT_PRIV = 1,
};

struct kgsl_iommu_ctx {
	const char *iommu_ctx_name;
	enum kgsl_iommu_context_id ctx_id;
};

struct kgsl_device_iommu_data {
	const struct kgsl_iommu_ctx *iommu_ctxs;
	int iommu_ctx_count;
	unsigned int physstart;
	unsigned int physend;
};

struct kgsl_device_platform_data {
	struct kgsl_pwrlevel pwrlevel[KGSL_MAX_PWRLEVELS];
	int init_level;
	int num_levels;
	int (*set_grp_async)(void);
	unsigned int idle_timeout;
	bool strtstp_sleepwake;
	unsigned int nap_allowed;
	unsigned int clk_map;
	unsigned int idle_needed;
	struct msm_bus_scale_pdata *bus_scale_table;
	struct kgsl_device_iommu_data *iommu_data;
	int iommu_count;
	struct msm_dcvs_core_info *core_info;
};

#ifdef CONFIG_MSM_KGSL_DRM
int kgsl_gem_obj_addr(int drm_fd, int handle, unsigned long *start,
			unsigned long *len);
#else
#define kgsl_gem_obj_addr(...) 0
#endif
#endif /* _MSM_KGSL_H */
