/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ARM64_KVM_NVHE_IOMMU_H__
#define __ARM64_KVM_NVHE_IOMMU_H__

#include <linux/types.h>
#include <asm/kvm_host.h>

#include <nvhe/mem_protect.h>

struct kvm_iommu_ops {
	int (*init)(void);
	bool (*host_smc_handler)(struct kvm_cpu_context *host_ctxt);
	bool (*host_mmio_dabt_handler)(struct kvm_cpu_context *host_ctxt,
				       phys_addr_t fault_pa, unsigned int len,
				       bool is_write, int rd);
	void (*host_stage2_set_owner)(phys_addr_t addr, size_t size, pkvm_id owner_id);
	int (*host_stage2_adjust_mmio_range)(phys_addr_t addr, phys_addr_t *start,
					     phys_addr_t *end);
};

extern struct kvm_iommu_ops kvm_iommu_ops;
extern const struct kvm_iommu_ops kvm_s2mpu_ops;

#endif	/* __ARM64_KVM_NVHE_IOMMU_H__ */
