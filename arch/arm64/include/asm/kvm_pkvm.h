// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 - Google LLC
 * Author: Quentin Perret <qperret@google.com>
 * Author: Fuad Tabba <tabba@google.com>
 */
#ifndef __ARM64_KVM_PKVM_H__
#define __ARM64_KVM_PKVM_H__

#include <linux/arm_ffa.h>
#include <linux/memblock.h>
#include <linux/scatterlist.h>
#include <asm/kvm_pgtable.h>
#include <asm/sysreg.h>

/* Maximum number of VMs that can co-exist under pKVM. */
#define KVM_MAX_PVMS 255

#define HYP_MEMBLOCK_REGIONS 128
#define PVMFW_INVALID_LOAD_ADDR	(-1)

int pkvm_vm_ioctl_enable_cap(struct kvm *kvm,struct kvm_enable_cap *cap);
int pkvm_init_host_vm(struct kvm *kvm, unsigned long type);
int pkvm_create_hyp_vm(struct kvm *kvm);
void pkvm_destroy_hyp_vm(struct kvm *kvm);
bool pkvm_is_hyp_created(struct kvm *kvm);
int pkvm_create_hyp_vcpu(struct kvm_vcpu *vcpu);
void pkvm_host_reclaim_page(struct kvm *host_kvm, phys_addr_t ipa);
int pvkm_enable_smc_forwarding(struct file *kvm_file);
/*
 * This functions as an allow-list of protected VM capabilities.
 * Features not explicitly allowed by this function are denied.
 */
static inline bool kvm_pvm_ext_allowed(long ext)
{
	switch (ext) {
	case KVM_CAP_IRQCHIP:
	case KVM_CAP_ONE_REG:
	case KVM_CAP_ARM_PSCI:
	case KVM_CAP_ARM_PSCI_0_2:
	case KVM_CAP_NR_VCPUS:
	case KVM_CAP_MAX_VCPUS:
	case KVM_CAP_MAX_VCPU_ID:
	case KVM_CAP_MSI_DEVID:
	case KVM_CAP_ARM_VM_IPA_SIZE:
	case KVM_CAP_ARM_PMU_V3:
	case KVM_CAP_ARM_SVE:
	case KVM_CAP_ARM_PTRAUTH_ADDRESS:
	case KVM_CAP_ARM_PTRAUTH_GENERIC:
	case KVM_CAP_ARM_PROTECTED_VM:
		return true;
	default:
		return false;
	}
}

static inline unsigned long pvm_supported_vcpu_features(void)
{
	unsigned long features = 0;

	set_bit(KVM_ARM_VCPU_POWER_OFF, &features);

	if (kvm_pvm_ext_allowed(KVM_CAP_ARM_EL1_32BIT))
		set_bit(KVM_ARM_VCPU_EL1_32BIT, &features);

	if (kvm_pvm_ext_allowed(KVM_CAP_ARM_PSCI_0_2))
		set_bit(KVM_ARM_VCPU_PSCI_0_2, &features);

	if (kvm_pvm_ext_allowed(KVM_CAP_ARM_PMU_V3))
		set_bit(KVM_ARM_VCPU_PMU_V3, &features);

	if (kvm_pvm_ext_allowed(KVM_CAP_ARM_SVE))
		set_bit(KVM_ARM_VCPU_SVE, &features);

	if (kvm_pvm_ext_allowed(KVM_CAP_ARM_PTRAUTH_ADDRESS) &&
	    kvm_pvm_ext_allowed(KVM_CAP_ARM_PTRAUTH_GENERIC)) {
		set_bit(KVM_ARM_VCPU_PTRAUTH_ADDRESS, &features);
		set_bit(KVM_ARM_VCPU_PTRAUTH_GENERIC, &features);
	}

	return features;
}

/* All HAFGRTR_EL2 bits are AMU */
#define HAFGRTR_AMU	__HAFGRTR_EL2_MASK

#define PVM_HAFGRTR_EL2_SET \
	(kvm_has_feat(kvm, ID_AA64PFR0_EL1, AMU, IMP) ? 0ULL : HAFGRTR_AMU)

#define PVM_HAFGRTR_EL2_CLR (0ULL)

/* No support for debug, trace, of PMU for protected VMs */
#define PVM_HDFGRTR_EL2_SET __HDFGRTR_EL2_MASK
#define PVM_HDFGRTR_EL2_CLR __HDFGRTR_EL2_nMASK

#define PVM_HDFGWTR_EL2_SET __HDFGWTR_EL2_MASK
#define PVM_HDFGWTR_EL2_CLR __HDFGWTR_EL2_nMASK

#define HFGxTR_RAS_IMP 	(\
			HFGxTR_EL2_ERXADDR_EL1 | \
			HFGxTR_EL2_ERXPFGF_EL1 | \
			HFGxTR_EL2_ERXMISCn_EL1 | \
			HFGxTR_EL2_ERXSTATUS_EL1 | \
			HFGxTR_EL2_ERXCTLR_EL1 | \
			HFGxTR_EL2_ERXFR_EL1 | \
			HFGxTR_EL2_ERRSELR_EL1 | \
			HFGxTR_EL2_ERRIDR_EL1 \
			)
#define HFGxTR_RAS_V1P1 (\
			HFGxTR_EL2_ERXPFGCDN_EL1 | \
			HFGxTR_EL2_ERXPFGCTL_EL1 \
			)
#define HFGxTR_GIC	HFGxTR_EL2_ICC_IGRPENn_EL1
#define HFGxTR_CSV2	(\
			HFGxTR_EL2_SCXTNUM_EL0 | \
			HFGxTR_EL2_SCXTNUM_EL1 \
			)
#define HFGxTR_LOR 	(\
			HFGxTR_EL2_LORSA_EL1 | \
			HFGxTR_EL2_LORN_EL1 | \
			HFGxTR_EL2_LORID_EL1 | \
			HFGxTR_EL2_LOREA_EL1 | \
			HFGxTR_EL2_LORC_EL1 \
			)
#define HFGxTR_PAUTH	(\
			HFGxTR_EL2_APIBKey | \
			HFGxTR_EL2_APIAKey | \
			HFGxTR_EL2_APGAKey | \
			HFGxTR_EL2_APDBKey | \
			HFGxTR_EL2_APDAKey \
			)
#define HFGxTR_nAIE	(\
			HFGxTR_EL2_nAMAIR2_EL1 | \
			HFGxTR_EL2_nMAIR2_EL1 \
			)
#define HFGxTR_nS2POE	HFGxTR_EL2_nS2POR_EL1
#define HFGxTR_nS1POE 	(\
			HFGxTR_EL2_nPOR_EL1 | \
			HFGxTR_EL2_nPOR_EL0 \
			)
#define HFGxTR_nS1PIE 	(\
			HFGxTR_EL2_nPIR_EL1 | \
			HFGxTR_EL2_nPIRE0_EL1 \
			)
#define HFGxTR_nTHE 	HFGxTR_EL2_nRCWMASK_EL1
#define HFGxTR_nSME 	(\
			HFGxTR_EL2_nTPIDR2_EL0 | \
			HFGxTR_EL2_nSMPRI_EL1 \
			)
#define HFGxTR_nGCS 	(\
			HFGxTR_EL2_nGCS_EL1 | \
			HFGxTR_EL2_nGCS_EL0 \
			)
#define HFGxTR_nLS64 	HFGxTR_EL2_nACCDATA_EL1

#define PVM_HFGXTR_EL2_SET \
	(kvm_has_feat(kvm, ID_AA64PFR0_EL1, RAS, IMP) ? 0ULL : HFGxTR_RAS_IMP) | \
	(kvm_has_feat(kvm, ID_AA64PFR0_EL1, RAS, V1P1) ? 0ULL : HFGxTR_RAS_V1P1) | \
	(kvm_has_feat(kvm, ID_AA64PFR0_EL1, GIC, IMP) ? 0ULL : HFGxTR_GIC) | \
	(kvm_has_feat(kvm, ID_AA64PFR0_EL1, CSV2, IMP) ? 0ULL : HFGxTR_CSV2) | \
	(kvm_has_feat(kvm, ID_AA64MMFR1_EL1, LO, IMP) ? 0ULL : HFGxTR_LOR) | \
	(vcpu_has_ptrauth(vcpu) ? 0ULL : HFGxTR_PAUTH) | \
	(vcpu_has_ptrauth(vcpu) ? 0ULL : HFGxTR_PAUTH) | \
	0

#define PVM_HFGXTR_EL2_CLR \
	(kvm_has_feat(kvm, ID_AA64MMFR3_EL1, AIE, IMP) ? 0ULL : HFGxTR_nAIE) | \
	(kvm_has_feat(kvm, ID_AA64MMFR3_EL1, S2POE, IMP) ? 0ULL : HFGxTR_nS2POE) | \
	(kvm_has_feat(kvm, ID_AA64MMFR3_EL1, S1POE, IMP) ? 0ULL : HFGxTR_nS1POE) | \
	(kvm_has_feat(kvm, ID_AA64MMFR3_EL1, S1PIE, IMP) ? 0ULL : HFGxTR_nS1PIE) | \
	(kvm_has_feat(kvm, ID_AA64PFR1_EL1, THE, IMP) ? 0ULL : HFGxTR_nTHE) | \
	(kvm_has_feat(kvm, ID_AA64PFR1_EL1, SME, IMP) ? 0ULL : HFGxTR_nSME) | \
	(kvm_has_feat(kvm, ID_AA64PFR1_EL1, GCS, IMP) ? 0ULL : HFGxTR_nGCS) | \
	(kvm_has_feat(kvm, ID_AA64ISAR1_EL1, LS64, LS64) ? 0ULL : HFGxTR_nLS64) | \
	0

#define PVM_HFGRTR_EL2_SET	PVM_HFGXTR_EL2_SET
#define PVM_HFGWTR_EL2_SET	PVM_HFGXTR_EL2_SET
#define PVM_HFGRTR_EL2_CLR	PVM_HFGXTR_EL2_CLR
#define PVM_HFGWTR_EL2_CLR	PVM_HFGXTR_EL2_CLR

#define HFGITR_SPECRES	(\
			HFGITR_EL2_CPPRCTX | \
			HFGITR_EL2_DVPRCTX | \
			HFGITR_EL2_CFPRCTX \
			)
#define HFGITR_TLBIOS	(\
			HFGITR_EL2_TLBIVAALE1OS | \
			HFGITR_EL2_TLBIVALE1OS | \
			HFGITR_EL2_TLBIVAAE1OS | \
			HFGITR_EL2_TLBIASIDE1OS | \
			HFGITR_EL2_TLBIVAE1OS | \
			HFGITR_EL2_TLBIVMALLE1OS \
			)
#define HFGITR_TLBIRANGE \
			(\
			HFGITR_TLBIOS | \
			HFGITR_EL2_TLBIRVAALE1 | \
			HFGITR_EL2_TLBIRVALE1 | \
			HFGITR_EL2_TLBIRVAAE1 | \
			HFGITR_EL2_TLBIRVAE1 | \
			HFGITR_EL2_TLBIRVAE1 | \
			HFGITR_EL2_TLBIRVAALE1IS | \
			HFGITR_EL2_TLBIRVALE1IS | \
			HFGITR_EL2_TLBIRVAAE1IS | \
			HFGITR_EL2_TLBIRVAE1IS | \
			HFGITR_EL2_TLBIVAALE1IS | \
			HFGITR_EL2_TLBIVALE1IS | \
			HFGITR_EL2_TLBIVAAE1IS | \
			HFGITR_EL2_TLBIASIDE1IS | \
			HFGITR_EL2_TLBIVAE1IS | \
			HFGITR_EL2_TLBIVMALLE1IS | \
			HFGITR_EL2_TLBIRVAALE1OS | \
			HFGITR_EL2_TLBIRVALE1OS | \
			HFGITR_EL2_TLBIRVAAE1OS | \
			HFGITR_EL2_TLBIRVAE1OS \
			)
#define HFGITR_TLB	HFGITR_TLBIRANGE
#define HFGITR_PAN2	(\
			HFGITR_EL2_ATS1E1WP | \
			HFGITR_EL2_ATS1E1RP | \
			HFGITR_EL2_ATS1E0W | \
			HFGITR_EL2_ATS1E0R | \
			HFGITR_EL2_ATS1E1W | \
			HFGITR_EL2_ATS1E1R \
			)
#define HFGITR_PAN	HFGITR_PAN2
#define HFGITR_DPB2	HFGITR_EL2_DCCVADP
#define HFGITR_DPB_IMP	HFGITR_EL2_DCCVAP
#define HFGITR_DPB	(HFGITR_DPB_IMP | HFGITR_DPB2)
#define HFGITR_nGCS	(\
			HFGITR_EL2_nGCSEPP | \
			HFGITR_EL2_nGCSSTR_EL1 | \
			HFGITR_EL2_nGCSPUSHM_EL1 \
			)
#define HFGITR_nBRBE	(\
			HFGITR_EL2_nBRBIALL | \
			HFGITR_EL2_nBRBINJ \
			)

#define PVM_HFGITR_EL2_SET \
	(kvm_has_feat(kvm, ID_AA64ISAR2_EL1, ATS1A, IMP) ? 0ULL : HFGITR_EL2_ATS1E1A) | \
	(kvm_has_feat(kvm, ID_AA64ISAR1_EL1, SPECRES, IMP) ? 0ULL : HFGITR_SPECRES) | \
	(kvm_has_feat(kvm, ID_AA64ISAR0_EL1, TLB, OS) ? 0ULL : HFGITR_TLB) | \
	(kvm_has_feat(kvm, ID_AA64MMFR1_EL1, PAN, IMP) ? 0ULL : HFGITR_PAN) | \
	(kvm_has_feat(kvm, ID_AA64ISAR1_EL1, DPB, IMP) ? 0ULL : HFGITR_DPB) | \
	0

#define PVM_HFGITR_EL2_CLR \
	(kvm_has_feat(kvm, ID_AA64PFR1_EL1, GCS, IMP) ? 0ULL : HFGITR_nGCS) | \
	(kvm_has_feat(kvm, ID_AA64DFR0_EL1, BRBE, IMP) ? 0ULL : HFGITR_nBRBE) | \
	0

#define HCRX_NMI		HCRX_EL2_TALLINT

#define HCRX_nPAuth_LR		HCRX_EL2_PACMEn
#define HCRX_nFPMR		HCRX_EL2_EnFPM
#define HCRX_nGCS		HCRX_EL2_GCSEn
#define HCRX_nSYSREG128		HCRX_EL2_EnIDCP128
#define HCRX_nADERR		HCRX_EL2_EnSDERR
#define HCRX_nDoubleFault2	HCRX_EL2_TMEA
#define HCRX_nANERR		HCRX_EL2_EnSNERR
#define HCRX_nD128		HCRX_EL2_D128En
#define HCRX_nTHE		HCRX_EL2_PTTWI
#define HCRX_nSCTLR2		HCRX_EL2_SCTLR2En
#define HCRX_nTCR2		HCRX_EL2_TCR2En
#define HCRX_nMOPS		(HCRX_EL2_MSCEn | HCRX_EL2_MCE2)
#define HCRX_nCMOW		HCRX_EL2_CMOW
#define HCRX_nNMI		(HCRX_EL2_VFNMI | HCRX_EL2_VINMI)
#define HCRX_SME		HCRX_EL2_SMPME
#define HCRX_nXS		(HCRX_EL2_FGTnXS | HCRX_EL2_FnXS)
#define HCRX_nLS64		(HCRX_EL2_EnASR| HCRX_EL2_EnALS | HCRX_EL2_EnAS0)

#define PVM_HCRX_EL2_SET \
	(kvm_has_feat(kvm, ID_AA64PFR1_EL1, NMI, IMP) ? 0ULL : HCRX_NMI) | \
	(kvm_has_feat(kvm, ID_AA64PFR1_EL1, SME, IMP) ? 0ULL : HCRX_SME) | \
	0

#define PVM_HCRX_EL2_CLR \
	(vcpu_has_ptrauth(vcpu) ? HCRX_nPAuth_LR : 0ULL) | \
	(kvm_has_feat(kvm, ID_AA64PFR1_EL1, GCS, IMP) ? 0ULL : HCRX_nGCS) | \
	(kvm_has_feat(kvm, ID_AA64ISAR2_EL1, SYSREG_128, IMP) ? 0ULL : HCRX_nSYSREG128) | \
	(kvm_has_feat(kvm, ID_AA64MMFR3_EL1, ADERR, FEAT_ADERR) ? 0ULL : HCRX_nADERR) | \
	(kvm_has_feat(kvm, ID_AA64PFR1_EL1, DF2, IMP) ? 0ULL : HCRX_nDoubleFault2) | \
	(kvm_has_feat(kvm, ID_AA64MMFR3_EL1, ANERR, FEAT_ANERR) ? 0ULL : HCRX_nANERR) | \
	(true /* trap unless ID_AA64MMFR0_EL1 PARANGE == 0b111 */ ? 0ULL : HCRX_nD128) | \
	(kvm_has_feat(kvm, ID_AA64PFR1_EL1, THE, IMP) ? 0ULL : HCRX_nTHE) | \
	(kvm_has_feat(kvm, ID_AA64MMFR3_EL1, SCTLRX, IMP) ? 0ULL : HCRX_nSCTLR2) | \
	(kvm_has_feat(kvm, ID_AA64MMFR3_EL1, TCRX, IMP) ? 0ULL : HCRX_nTCR2) | \
	(kvm_has_feat(kvm, ID_AA64ISAR2_EL1, MOPS, IMP) ? 0ULL : HCRX_nMOPS) | \
	(kvm_has_feat(kvm, ID_AA64MMFR1_EL1, CMOW, IMP) ? 0ULL : HCRX_nCMOW) | \
	(kvm_has_feat(kvm, ID_AA64PFR1_EL1, NMI, IMP) ? 0ULL : HCRX_nNMI) | \
	(kvm_has_feat(kvm, ID_AA64ISAR1_EL1, XS, IMP) ? 0ULL : HCRX_nXS) | \
	(kvm_has_feat(kvm, ID_AA64ISAR1_EL1, LS64, LS64) ? 0ULL : HCRX_nLS64) | \
	0

enum pkvm_moveable_reg_type {
	PKVM_MREG_MEMORY,
	PKVM_MREG_PROTECTED_RANGE,
	PKVM_MREG_ASSIGN_MMIO,
};

struct pkvm_moveable_reg {
	phys_addr_t start;
	u64 size;
	enum pkvm_moveable_reg_type type;
};

#define PKVM_NR_MOVEABLE_REGS 512
extern struct pkvm_moveable_reg kvm_nvhe_sym(pkvm_moveable_regs)[];
extern unsigned int kvm_nvhe_sym(pkvm_moveable_regs_nr);

extern struct memblock_region kvm_nvhe_sym(hyp_memory)[];
extern unsigned int kvm_nvhe_sym(hyp_memblock_nr);

extern phys_addr_t kvm_nvhe_sym(pvmfw_base);
extern phys_addr_t kvm_nvhe_sym(pvmfw_size);

static inline unsigned long
hyp_vmemmap_memblock_size(struct memblock_region *reg, size_t vmemmap_entry_size)
{
	unsigned long nr_pages = reg->size >> PAGE_SHIFT;
	unsigned long start, end;

	start = (reg->base >> PAGE_SHIFT) * vmemmap_entry_size;
	end = start + nr_pages * vmemmap_entry_size;
	start = ALIGN_DOWN(start, PAGE_SIZE);
	end = ALIGN(end, PAGE_SIZE);

	return end - start;
}

static inline unsigned long hyp_vmemmap_pages(size_t vmemmap_entry_size)
{
	unsigned long res = 0, i;

	for (i = 0; i < kvm_nvhe_sym(hyp_memblock_nr); i++) {
		res += hyp_vmemmap_memblock_size(&kvm_nvhe_sym(hyp_memory)[i],
						 vmemmap_entry_size);
	}

	return res >> PAGE_SHIFT;
}

static inline unsigned long hyp_vm_table_pages(void)
{
	return PAGE_ALIGN(KVM_MAX_PVMS * sizeof(void *)) >> PAGE_SHIFT;
}

static inline unsigned long __hyp_pgtable_max_pages(unsigned long nr_pages)
{
	unsigned long total = 0;
	int i;

	/* Provision the worst case scenario */
	for (i = KVM_PGTABLE_FIRST_LEVEL; i <= KVM_PGTABLE_LAST_LEVEL; i++) {
		nr_pages = DIV_ROUND_UP(nr_pages, PTRS_PER_PTE);
		total += nr_pages;
	}

	return total;
}

static inline unsigned long __hyp_pgtable_moveable_regs_pages(void)
{
	unsigned long res = 0, i;

	/* Cover all of moveable regions with page-granularity */
	for (i = 0; i < kvm_nvhe_sym(pkvm_moveable_regs_nr); i++) {
		struct pkvm_moveable_reg *reg = &kvm_nvhe_sym(pkvm_moveable_regs)[i];
		res += __hyp_pgtable_max_pages(reg->size >> PAGE_SHIFT);
	}

	return res;
}

extern u64 kvm_nvhe_sym(hyp_lm_size_mb);

static inline unsigned long hyp_s1_pgtable_pages(void)
{
	unsigned long res;

	if (!kvm_nvhe_sym(hyp_lm_size_mb))
		res = __hyp_pgtable_moveable_regs_pages();
	else
		res = __hyp_pgtable_max_pages(kvm_nvhe_sym(hyp_lm_size_mb) * SZ_1M / PAGE_SIZE);

	/* Allow 1 GiB for private mappings */
	res += __hyp_pgtable_max_pages(SZ_1G >> PAGE_SHIFT);

	return res;
}

static inline unsigned long host_s2_pgtable_pages(void)
{
	unsigned long res;

	/*
	 * Include an extra 16 pages to safely upper-bound the worst case of
	 * concatenated pgds.
	 */
	res = __hyp_pgtable_moveable_regs_pages() + 16;

	/* Allow 1 GiB for non-moveable regions */
	res += __hyp_pgtable_max_pages(SZ_1G >> PAGE_SHIFT);

	return res;
}

#ifdef CONFIG_PKVM_SELFTESTS
static inline unsigned long pkvm_selftest_pages(void) { return 32; }
#else
static inline unsigned long pkvm_selftest_pages(void) { return 0; }
#endif

#define KVM_FFA_MBOX_NR_PAGES		1
#define KVM_FFA_SPM_HANDLE_NR_PAGES	2

/*
 * Maximum number of consitutents allowed in a descriptor. This number is
 * arbitrary, see comment below on SG_MAX_SEGMENTS in hyp_ffa_proxy_pages().
 */
#define KVM_FFA_MAX_NR_CONSTITUENTS	4096

static inline unsigned long hyp_ffa_proxy_pages(void)
{
	size_t desc_max;
	unsigned long num_pages;

	/*
	 * SG_MAX_SEGMENTS is supposed to bound the number of elements in an
	 * sglist, which should match the number of consituents in the
	 * corresponding FFA descriptor. As such, the EL2 buffer needs to be
	 * large enough to hold a descriptor with SG_MAX_SEGMENTS consituents
	 * at least. But the kernel's DMA code doesn't enforce the limit, and
	 * it is sometimes abused, so let's allow larger descriptors and hope
	 * for the best.
	 */
	BUILD_BUG_ON(KVM_FFA_MAX_NR_CONSTITUENTS < SG_MAX_SEGMENTS);

	/*
	 * The hypervisor FFA proxy needs enough memory to buffer a fragmented
	 * descriptor returned from EL3 in response to a RETRIEVE_REQ call.
	 */
	desc_max = sizeof(struct ffa_mem_region) +
		   sizeof(struct ffa_mem_region_attributes) +
		   sizeof(struct ffa_composite_mem_region) +
		   KVM_FFA_MAX_NR_CONSTITUENTS * sizeof(struct ffa_mem_region_addr_range);

	/* Plus a page each for the hypervisor's RX and TX mailboxes. */
	num_pages = (2 * KVM_FFA_MBOX_NR_PAGES) + DIV_ROUND_UP(desc_max, PAGE_SIZE);

	return num_pages;
}

static inline size_t pkvm_host_sve_state_size(void)
{
	if (!system_supports_sve())
		return 0;

	return size_add(sizeof(struct cpu_sve_state),
			SVE_SIG_REGS_SIZE(sve_vq_from_vl(kvm_host_sve_max_vl)));
}

int __pkvm_topup_hyp_alloc(unsigned long nr_pages);

#define kvm_call_refill_hyp_nvhe(f, ...)				\
({									\
	struct arm_smccc_res res;					\
	int __ret;							\
	do {								\
		__ret = -1;						\
		arm_smccc_1_1_hvc(KVM_HOST_SMCCC_FUNC(f),		\
				  ##__VA_ARGS__, &res);			\
		if (WARN_ON(res.a0 != SMCCC_RET_SUCCESS))		\
			break;						\
									\
		__ret = res.a1;						\
		if (__ret == -ENOMEM && res.a3) {			\
			__ret = __pkvm_topup_hyp_alloc(res.a3);		\
		} else {						\
			break;						\
		}							\
	} while (!__ret);						\
	__ret;								\
})

enum pkvm_ptdump_ops {
	PKVM_PTDUMP_GET_LEVEL,
	PKVM_PTDUMP_GET_RANGE,
	PKVM_PTDUMP_WALK_RANGE,
};

struct pkvm_ptdump_log {
	/* VA_BIT - PAGE_SHIFT + 1 (INVALID_PTDUMP_PFN) */
	u64	pfn: 41;
	bool	valid: 1;
	bool	r: 1;
	bool	w: 1;
	char	xn: 2;
	bool	table: 1;
	u16	page_state: 2;
	u16	level: 8;
} __packed;

#define INVALID_PTDUMP_PFN	(BIT(41) - 1)

struct pkvm_ptdump_log_hdr {
	/* The next page */
	u64	pfn_next: 48;
	/* The write index in the log page */
	u64	w_index: 16;
};

int pkvm_call_hyp_nvhe_ppage(struct kvm_pinned_page *ppage,
			     int (*call_hyp_nvhe)(u64, u64, u8, void*),
			     void *args, bool unmap);

struct pkvm_mapping {
	struct rb_node node;
	u64 gfn;
	u64 pfn;
	u64 nr_pages;
	u64 __subtree_last;	/* Internal member for interval tree */
};

int pkvm_pgtable_stage2_init(struct kvm_pgtable *pgt, struct kvm_s2_mmu *mmu,
			     struct kvm_pgtable_mm_ops *mm_ops, struct kvm_pgtable_pte_ops *pte_ops);
void pkvm_pgtable_stage2_destroy(struct kvm_pgtable *pgt);
int pkvm_pgtable_stage2_map(struct kvm_pgtable *pgt, u64 addr, u64 size, u64 phys,
			    enum kvm_pgtable_prot prot, void *mc,
			    enum kvm_pgtable_walk_flags flags);
int pkvm_pgtable_stage2_unmap(struct kvm_pgtable *pgt, u64 addr, u64 size);
int pkvm_pgtable_stage2_wrprotect(struct kvm_pgtable *pgt, u64 addr, u64 size);
int pkvm_pgtable_stage2_flush(struct kvm_pgtable *pgt, u64 addr, u64 size);
bool pkvm_pgtable_stage2_test_clear_young(struct kvm_pgtable *pgt, u64 addr, u64 size, bool mkold);
int pkvm_pgtable_stage2_relax_perms(struct kvm_pgtable *pgt, u64 addr, enum kvm_pgtable_prot prot,
				    enum kvm_pgtable_walk_flags flags);
kvm_pte_t pkvm_pgtable_stage2_mkyoung(struct kvm_pgtable *pgt, u64 addr,
				      enum kvm_pgtable_walk_flags flags);
int pkvm_pgtable_stage2_split(struct kvm_pgtable *pgt, u64 addr, u64 size, void *mc);
void pkvm_pgtable_stage2_free_unlinked(struct kvm_pgtable_mm_ops *mm_ops,
				       struct kvm_pgtable_pte_ops *pte_ops,
				       void *pgtable, s8 level);
kvm_pte_t *pkvm_pgtable_stage2_create_unlinked(struct kvm_pgtable *pgt, u64 phys, s8 level,
					       enum kvm_pgtable_prot prot, void *mc,
					       bool force_pte);
#endif	/* __ARM64_KVM_PKVM_H__ */
