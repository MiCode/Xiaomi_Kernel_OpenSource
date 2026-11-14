// SPDX-License-Identifier: GPL-2.0
/*
 * pKVM hyp driver for the Arm SMMUv3
 *
 * Copyright (C) 2022 Linaro Ltd.
 */
#include <asm/arm-smmu-v3-common.h>
#include <asm/kvm_hyp.h>
#include <linux/io-pgtable-arm.h>
#include <nvhe/alloc.h>
#include <nvhe/iommu.h>
#include <nvhe/mem_protect.h>
#include <nvhe/mm.h>
#include <nvhe/pkvm.h>
#include <nvhe/rwlock.h>
#include <nvhe/trap_handler.h>

#include "arm_smmu_v3.h"
#include "arm-smmu-v3-module.h"

#ifdef MODULE
void *memset(void *dst, int c, size_t count)
{
	return CALL_FROM_OPS(memset, dst, c, count);
}

#ifdef CONFIG_LIST_HARDENED
bool __list_add_valid_or_report(struct list_head *new,
				struct list_head *prev,
				struct list_head *next)
{
	return CALL_FROM_OPS(list_add_valid_or_report, new, prev, next);
}

bool __list_del_entry_valid_or_report(struct list_head *entry)
{
	return CALL_FROM_OPS(list_del_entry_valid_or_report, entry);
}
#endif

const struct pkvm_module_ops		*mod_ops;
#endif

#define ARM_SMMU_POLL_TIMEOUT_US	100000 /* 100ms arbitrary timeout */

size_t __ro_after_init kvm_hyp_arm_smmu_v3_count;
struct hyp_arm_smmu_v3_device *kvm_hyp_arm_smmu_v3_smmus;

#define for_each_smmu(smmu) \
	for ((smmu) = kvm_hyp_arm_smmu_v3_smmus; \
	     (smmu) != &kvm_hyp_arm_smmu_v3_smmus[kvm_hyp_arm_smmu_v3_count]; \
	     (smmu)++)

/*
 * Wait until @cond is true.
 * Return 0 on success, or -ETIMEDOUT
 */
#define smmu_wait(_cond)					\
({								\
	int __i = 0;						\
	int __ret = 0;						\
								\
	while (!(_cond)) {					\
		if (++__i > ARM_SMMU_POLL_TIMEOUT_US) {		\
			__ret = -ETIMEDOUT;			\
			break;					\
		}						\
		pkvm_udelay(1);					\
	}							\
	__ret;							\
})

#define smmu_wait_event(_smmu, _cond)				\
({								\
	if ((_smmu)->features & ARM_SMMU_FEAT_SEV) {		\
		while (!(_cond))				\
			wfe();					\
	}							\
	smmu_wait(_cond);					\
})

struct domain_iommu_node {
	struct kvm_hyp_iommu		*iommu;
	struct list_head		list;
	unsigned long			ref;
};

/*
 * SMMUv3 domain:
 * @domain: Pointer to the IOMMU domain.
 * @iommu_list: List of SMMU instances for this domain
 * @list_lock: Protects iommu_list
 * @type: Type of domain (S1, S2)
 * @pgt_lock: Lock for page table
 * @pgtable: io_pgtable instance for this domain
 */
struct hyp_arm_smmu_v3_domain {
	struct kvm_hyp_iommu_domain     *domain;
	struct list_head		iommu_list;
	hyp_rwlock_t			list_lock;
	u32				type;
	hyp_spinlock_t			pgt_lock;
	struct io_pgtable		*pgtable;
};

static struct hyp_arm_smmu_v3_device *to_smmu(struct kvm_hyp_iommu *iommu)
{
	return container_of(iommu, struct hyp_arm_smmu_v3_device, iommu);
}

static int smmu_write_cr0(struct hyp_arm_smmu_v3_device *smmu, u32 val)
{
	writel_relaxed(val, smmu->base + ARM_SMMU_CR0);
	return smmu_wait(readl_relaxed(smmu->base + ARM_SMMU_CR0ACK) == val);
}

/* Transfer ownership of structures from host to hyp */
static int smmu_take_pages(u64 phys, size_t size)
{
	WARN_ON(!PAGE_ALIGNED(phys) || !PAGE_ALIGNED(size));
	return __pkvm_host_donate_hyp(phys >> PAGE_SHIFT, size >> PAGE_SHIFT);
}

static void smmu_reclaim_pages(u64 phys, size_t size)
{
	WARN_ON(!PAGE_ALIGNED(phys) || !PAGE_ALIGNED(size));
	WARN_ON(__pkvm_hyp_donate_host(phys >> PAGE_SHIFT, size >> PAGE_SHIFT));
}

#define Q_WRAP(smmu, reg)	((reg) & (1 << (smmu)->cmdq_log2size))
#define Q_IDX(smmu, reg)	((reg) & ((1 << (smmu)->cmdq_log2size) - 1))

static bool smmu_cmdq_has_space(struct hyp_arm_smmu_v3_device *smmu, u32 n)
{
	u64 smmu_cons = readl_relaxed(smmu->base + ARM_SMMU_CMDQ_CONS);
	u32 space, prod, cons;

	prod = Q_IDX(smmu, smmu->cmdq_prod);
	cons = Q_IDX(smmu, smmu_cons);

	if (Q_WRAP(smmu, smmu->cmdq_prod) == Q_WRAP(smmu, smmu_cons))
		space = (1 << smmu->cmdq_log2size) - (prod - cons);
	else
		space = cons - prod;

	return space >= n;
}

static bool smmu_cmdq_empty(struct hyp_arm_smmu_v3_device *smmu)
{
	u64 cons = readl_relaxed(smmu->base + ARM_SMMU_CMDQ_CONS);

	return Q_IDX(smmu, smmu->cmdq_prod) == Q_IDX(smmu, cons) &&
	       Q_WRAP(smmu, smmu->cmdq_prod) == Q_WRAP(smmu, cons);
}

static int smmu_build_cmd(u64 *cmd, struct arm_smmu_cmdq_ent *ent)
{
	cmd[0] |= FIELD_PREP(CMDQ_0_OP, ent->opcode);

	switch (ent->opcode) {
	case CMDQ_OP_CFGI_ALL:
		cmd[1] |= FIELD_PREP(CMDQ_CFGI_1_RANGE, 31);
		break;
	case CMDQ_OP_CFGI_CD:
		cmd[0] |= FIELD_PREP(CMDQ_CFGI_0_SSID, ent->cfgi.ssid);
		fallthrough;
	case CMDQ_OP_CFGI_STE:
		cmd[0] |= FIELD_PREP(CMDQ_CFGI_0_SID, ent->cfgi.sid);
		cmd[1] |= FIELD_PREP(CMDQ_CFGI_1_LEAF, ent->cfgi.leaf);
		break;
	case CMDQ_OP_TLBI_NH_VA:
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_VMID, ent->tlbi.vmid);
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_NUM, ent->tlbi.num);
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_SCALE, ent->tlbi.scale);
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_ASID, ent->tlbi.asid);
		cmd[1] |= FIELD_PREP(CMDQ_TLBI_1_LEAF, ent->tlbi.leaf);
		cmd[1] |= FIELD_PREP(CMDQ_TLBI_1_TTL, ent->tlbi.ttl);
		cmd[1] |= FIELD_PREP(CMDQ_TLBI_1_TG, ent->tlbi.tg);
		cmd[1] |= ent->tlbi.addr & CMDQ_TLBI_1_VA_MASK;
		break;
	case CMDQ_OP_TLBI_NSNH_ALL:
		break;
	case CMDQ_OP_TLBI_NH_ASID:
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_ASID, ent->tlbi.asid);
		fallthrough;
	case CMDQ_OP_TLBI_S12_VMALL:
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_VMID, ent->tlbi.vmid);
		break;
	case CMDQ_OP_TLBI_S2_IPA:
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_NUM, ent->tlbi.num);
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_SCALE, ent->tlbi.scale);
		cmd[0] |= FIELD_PREP(CMDQ_TLBI_0_VMID, ent->tlbi.vmid);
		cmd[1] |= FIELD_PREP(CMDQ_TLBI_1_LEAF, ent->tlbi.leaf);
		cmd[1] |= FIELD_PREP(CMDQ_TLBI_1_TTL, ent->tlbi.ttl);
		cmd[1] |= FIELD_PREP(CMDQ_TLBI_1_TG, ent->tlbi.tg);
		cmd[1] |= ent->tlbi.addr & CMDQ_TLBI_1_IPA_MASK;
		break;
	case CMDQ_OP_CMD_SYNC:
		cmd[0] |= FIELD_PREP(CMDQ_SYNC_0_CS, CMDQ_SYNC_0_CS_SEV);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int smmu_issue_cmds(struct hyp_arm_smmu_v3_device *smmu,
			   u64 *cmds, int n)
{
	int i;
	int ret;
	u32 prod;

	ret = smmu_wait_event(smmu, smmu_cmdq_has_space(smmu, n));
	if (ret)
		return ret;

	for (i = 0; i < n; i++) {
		int j;
		int idx = Q_IDX(smmu, smmu->cmdq_prod + i);
		u64 *slot = smmu->cmdq_base + idx * CMDQ_ENT_DWORDS;

		for (j = 0; j < CMDQ_ENT_DWORDS; j++)
			slot[j] = cpu_to_le64(cmds[i * CMDQ_ENT_DWORDS + j]);
	}

	prod = (Q_WRAP(smmu, smmu->cmdq_prod) | Q_IDX(smmu, smmu->cmdq_prod)) + n;
	smmu->cmdq_prod = Q_OVF(smmu->cmdq_prod) | Q_WRAP(smmu, prod) | Q_IDX(smmu, prod);

	writel(Q_IDX(smmu, smmu->cmdq_prod) | Q_WRAP(smmu, smmu->cmdq_prod),
	       smmu->base + ARM_SMMU_CMDQ_PROD);
	return 0;
}

static int smmu_add_cmd(struct hyp_arm_smmu_v3_device *smmu,
			struct arm_smmu_cmdq_ent *ent)
{
	u64 cmd[CMDQ_ENT_DWORDS] = {};
	int ret;

	ret = smmu_build_cmd(cmd, ent);
	if (ret)
		return ret;

	return smmu_issue_cmds(smmu, cmd, 1);
}

static int smmu_sync_cmd(struct hyp_arm_smmu_v3_device *smmu)
{
	int ret;
	struct arm_smmu_cmdq_ent cmd = {
		.opcode = CMDQ_OP_CMD_SYNC,
	};

	ret = smmu_add_cmd(smmu, &cmd);
	if (ret)
		return ret;

	return smmu_wait_event(smmu, smmu_cmdq_empty(smmu));
}

static int smmu_send_cmd(struct hyp_arm_smmu_v3_device *smmu,
			 struct arm_smmu_cmdq_ent *cmd)
{
	int ret;

	if (smmu->iommu.power_is_off)
		return 0;

	ret = smmu_add_cmd(smmu, cmd);
	if (ret)
		return ret;

	return smmu_sync_cmd(smmu);
}

static int smmu_sync_ste(struct hyp_arm_smmu_v3_device *smmu, u32 sid)
{
	struct arm_smmu_cmdq_ent cmd = {
		.opcode = CMDQ_OP_CFGI_STE,
		.cfgi.sid = sid,
		.cfgi.leaf = true,
	};

	return smmu_send_cmd(smmu, &cmd);
}

static int smmu_sync_cd(struct hyp_arm_smmu_v3_device *smmu, u32 sid, u32 ssid)
{
	struct arm_smmu_cmdq_ent cmd = {
		.opcode = CMDQ_OP_CFGI_CD,
		.cfgi.sid	= sid,
		.cfgi.ssid	= ssid,
		.cfgi.leaf = true,
	};

	return smmu_send_cmd(smmu, &cmd);
}

static int smmu_alloc_l2_strtab(struct hyp_arm_smmu_v3_device *smmu, u32 sid)
{
	struct arm_smmu_strtab_cfg *cfg = &smmu->strtab_cfg;
	struct arm_smmu_strtab_l1 *l1_desc;
	dma_addr_t l2ptr_dma;
	struct arm_smmu_strtab_l2 *l2table;
	size_t l2_order = get_order(sizeof(struct arm_smmu_strtab_l2));
	int flags = 0;

	l1_desc = &cfg->l2.l1tab[arm_smmu_strtab_l1_idx(sid)];
	if (l1_desc->l2ptr)
		return 0;

	if (!(smmu->features & ARM_SMMU_FEAT_COHERENCY))
		flags |= IOMMU_PAGE_NOCACHE;

	l2table = kvm_iommu_donate_pages(l2_order, flags);
	if (!l2table)
		return -ENOMEM;

	l2ptr_dma = hyp_virt_to_phys(l2table);

	if (l2ptr_dma & (~STRTAB_L1_DESC_L2PTR_MASK | ~PAGE_MASK)) {
		kvm_iommu_reclaim_pages(l2table, l2_order);
		return -EINVAL;
	}

	/* Ensure the empty stream table is visible before the descriptor write */
	wmb();

	arm_smmu_write_strtab_l1_desc(l1_desc, l2ptr_dma);
	return 0;
}

static struct arm_smmu_ste *
smmu_get_ste_ptr(struct hyp_arm_smmu_v3_device *smmu, u32 sid)
{
	struct arm_smmu_strtab_cfg *cfg = &smmu->strtab_cfg;

	if (smmu->features & ARM_SMMU_FEAT_2_LVL_STRTAB) {
		struct arm_smmu_strtab_l1 *l1_desc =
					&cfg->l2.l1tab[arm_smmu_strtab_l1_idx(sid)];
		struct arm_smmu_strtab_l2 *l2ptr;

		if (arm_smmu_strtab_l1_idx(sid) > cfg->l2.num_l1_ents)
			return NULL;
		/* L2 should be allocated before calling this. */
		if (WARN_ON(!l1_desc->l2ptr))
			return NULL;

		l2ptr = hyp_phys_to_virt(l1_desc->l2ptr & STRTAB_L1_DESC_L2PTR_MASK);
		/* Two-level walk */
		return &l2ptr->stes[arm_smmu_strtab_l2_idx(sid)];
	}

	if (sid > cfg->linear.num_ents)
		return NULL;
	/* Simple linear lookup */
	return &cfg->linear.table[sid];
}

static struct arm_smmu_ste *
smmu_get_alloc_ste_ptr(struct hyp_arm_smmu_v3_device *smmu, u32 sid)
{
	if (smmu->features & ARM_SMMU_FEAT_2_LVL_STRTAB) {
		int ret = smmu_alloc_l2_strtab(smmu, sid);

		if (ret) {
			WARN_ON(ret != -ENOMEM);
			return NULL;
		}
	}
	return smmu_get_ste_ptr(smmu, sid);
}

static u64 *smmu_get_cd_ptr(u64 *cdtab, u32 ssid)
{
	/* Only linear supported for now. */
	return cdtab + ssid * CTXDESC_CD_DWORDS;
}

static u64 *smmu_alloc_cd(struct hyp_arm_smmu_v3_device *smmu, u32 pasid_bits)
{
	u64 *cd_table;
	int flags = 0;
	u32 requested_order = get_order((1 << pasid_bits) *
					(CTXDESC_CD_DWORDS << 3));

	/*
	 * We support max of 64K linear tables only, this should be enough
	 * for 128 pasids
	 */
	if (WARN_ON(requested_order > 4))
		return NULL;

	if (!(smmu->features & ARM_SMMU_FEAT_COHERENCY))
		flags |= IOMMU_PAGE_NOCACHE;

	cd_table = kvm_iommu_donate_pages(requested_order, flags);
	if (!cd_table)
		return NULL;
	return (u64 *)hyp_virt_to_phys(cd_table);
}

static void smmu_free_cd(u64 *cd_table, u32 pasid_bits)
{
	u32 order = get_order((1 << pasid_bits) *
			      (CTXDESC_CD_DWORDS << 3));

	kvm_iommu_reclaim_pages(cd_table, order);
}

static int smmu_init_registers(struct hyp_arm_smmu_v3_device *smmu)
{
	u64 val, old;
	int ret;

	if (!(readl_relaxed(smmu->base + ARM_SMMU_GBPA) & GBPA_ABORT))
		return -EINVAL;

	/* Initialize all RW registers that will be read by the SMMU */
	ret = smmu_write_cr0(smmu, 0);
	if (ret)
		return ret;

	val = FIELD_PREP(CR1_TABLE_SH, ARM_SMMU_SH_ISH) |
	      FIELD_PREP(CR1_TABLE_OC, CR1_CACHE_WB) |
	      FIELD_PREP(CR1_TABLE_IC, CR1_CACHE_WB) |
	      FIELD_PREP(CR1_QUEUE_SH, ARM_SMMU_SH_ISH) |
	      FIELD_PREP(CR1_QUEUE_OC, CR1_CACHE_WB) |
	      FIELD_PREP(CR1_QUEUE_IC, CR1_CACHE_WB);
	writel_relaxed(val, smmu->base + ARM_SMMU_CR1);
	writel_relaxed(CR2_PTM, smmu->base + ARM_SMMU_CR2);

	val = readl_relaxed(smmu->base + ARM_SMMU_GERROR);
	old = readl_relaxed(smmu->base + ARM_SMMU_GERRORN);
	/* Service Failure Mode is fatal */
	if ((val ^ old) & GERROR_SFM_ERR)
		return -EIO;
	/* Clear pending errors */
	writel_relaxed(val, smmu->base + ARM_SMMU_GERRORN);

	return 0;
}

static int smmu_init_cmdq(struct hyp_arm_smmu_v3_device *smmu)
{
	u64 cmdq_base;
	size_t cmdq_nr_entries, cmdq_size;
	int ret;
	enum kvm_pgtable_prot prot = PAGE_HYP;

	cmdq_base = readq_relaxed(smmu->base + ARM_SMMU_CMDQ_BASE);
	if (cmdq_base & ~(Q_BASE_RWA | Q_BASE_ADDR_MASK | Q_BASE_LOG2SIZE))
		return -EINVAL;

	smmu->cmdq_log2size = cmdq_base & Q_BASE_LOG2SIZE;
	cmdq_nr_entries = 1 << smmu->cmdq_log2size;
	cmdq_size = cmdq_nr_entries * CMDQ_ENT_DWORDS * 8;

	cmdq_base &= Q_BASE_ADDR_MASK;

	if (!(smmu->features & ARM_SMMU_FEAT_COHERENCY))
		prot |= KVM_PGTABLE_PROT_NORMAL_NC;

	ret = ___pkvm_host_donate_hyp_prot(cmdq_base >> PAGE_SHIFT,
					   PAGE_ALIGN(cmdq_size) >> PAGE_SHIFT,
					   false, prot);
	if (ret)
		return ret;

	smmu->cmdq_base = hyp_phys_to_virt(cmdq_base);

	memset(smmu->cmdq_base, 0, cmdq_size);
	writel_relaxed(0, smmu->base + ARM_SMMU_CMDQ_PROD);
	writel_relaxed(0, smmu->base + ARM_SMMU_CMDQ_CONS);

	return 0;
}

/*
 * Event q support is optional and managed by the kernel,
 * However, it must set in a shared state so it can't be donated
 * to the hypervisor later.
 * This relies on the ARM_SMMU_EVTQ_BASE can't be changed after
 * de-privilege.
 */
static int smmu_init_evtq(struct hyp_arm_smmu_v3_device *smmu)
{
	u64 evtq_base, evtq_pfn;
	size_t evtq_nr_entries, evtq_size, evtq_nr_pages;
	void *evtq_va, *evtq_end;
	size_t i;
	int ret;

	evtq_base = readq_relaxed(smmu->base + ARM_SMMU_EVTQ_BASE);
	if (!evtq_base)
		return 0;

	if (evtq_base & ~(Q_BASE_RWA | Q_BASE_ADDR_MASK | Q_BASE_LOG2SIZE))
		return -EINVAL;

	evtq_nr_entries = 1 << (evtq_base & Q_BASE_LOG2SIZE);
	evtq_size = evtq_nr_entries * EVTQ_ENT_DWORDS * 8;
	evtq_nr_pages = PAGE_ALIGN(evtq_size) >> PAGE_SHIFT;

	evtq_pfn = PAGE_ALIGN(evtq_base & Q_BASE_ADDR_MASK) >> PAGE_SHIFT;

	for (i = 0 ; i < evtq_nr_pages ; ++i) {
		ret = __pkvm_host_share_hyp(evtq_pfn + i);
		if (ret)
			return ret;
	}

	evtq_va = hyp_phys_to_virt(evtq_pfn << PAGE_SHIFT);
	evtq_end = hyp_phys_to_virt((evtq_pfn + evtq_nr_pages) << PAGE_SHIFT);

	return hyp_pin_shared_mem(evtq_va, evtq_end);
}

static int smmu_init_strtab(struct hyp_arm_smmu_v3_device *smmu)
{
	int ret;
	u64 strtab_base;
	size_t strtab_size;
	u32 strtab_cfg, fmt;
	int split, log2size;
	struct arm_smmu_strtab_cfg *cfg = &smmu->strtab_cfg;
	enum kvm_pgtable_prot prot = PAGE_HYP;

	if (!(smmu->features & ARM_SMMU_FEAT_COHERENCY))
		prot |= KVM_PGTABLE_PROT_NORMAL_NC;

	strtab_base = readq_relaxed(smmu->base + ARM_SMMU_STRTAB_BASE);
	if (strtab_base & ~(STRTAB_BASE_ADDR_MASK | STRTAB_BASE_RA))
		return -EINVAL;

	strtab_cfg = readl_relaxed(smmu->base + ARM_SMMU_STRTAB_BASE_CFG);
	if (strtab_cfg & ~(STRTAB_BASE_CFG_FMT | STRTAB_BASE_CFG_SPLIT |
			   STRTAB_BASE_CFG_LOG2SIZE))
		return -EINVAL;

	fmt = FIELD_GET(STRTAB_BASE_CFG_FMT, strtab_cfg);
	split = FIELD_GET(STRTAB_BASE_CFG_SPLIT, strtab_cfg);
	log2size = FIELD_GET(STRTAB_BASE_CFG_LOG2SIZE, strtab_cfg);
	strtab_base &= STRTAB_BASE_ADDR_MASK;

	switch (fmt) {
	case STRTAB_BASE_CFG_FMT_LINEAR:
		if (split)
			return -EINVAL;
		cfg->linear.num_ents = 1 << log2size;
		strtab_size = cfg->linear.num_ents * sizeof(struct arm_smmu_ste);
		cfg->linear.ste_dma = strtab_base;
		ret = ___pkvm_host_donate_hyp_prot(strtab_base >> PAGE_SHIFT,
						   PAGE_ALIGN(strtab_size) >> PAGE_SHIFT,
						   false, prot);
		if (ret)
			return -EINVAL;
		cfg->linear.table = hyp_phys_to_virt(strtab_base);
		/* Disable all STEs */
		memset(cfg->linear.table, 0, strtab_size);
		break;
	case STRTAB_BASE_CFG_FMT_2LVL:
		if (split != STRTAB_SPLIT)
			return -EINVAL;
		cfg->l2.num_l1_ents = 1 << max(0, log2size - split);
		strtab_size = cfg->l2.num_l1_ents * sizeof(struct arm_smmu_strtab_l1);
		cfg->l2.l1_dma = strtab_base;
		ret = ___pkvm_host_donate_hyp_prot(strtab_base >> PAGE_SHIFT,
						   PAGE_ALIGN(strtab_size) >> PAGE_SHIFT,
						   false, prot);
		if (ret)
			return -EINVAL;
		cfg->l2.l1tab = hyp_phys_to_virt(strtab_base);
		/* Disable all STEs */
		memset(cfg->l2.l1tab, 0, strtab_size);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int smmu_reset_device(struct hyp_arm_smmu_v3_device *smmu)
{
	int ret;
	struct arm_smmu_cmdq_ent cfgi_cmd = {
		.opcode = CMDQ_OP_CFGI_ALL,
	};
	struct arm_smmu_cmdq_ent tlbi_cmd = {
		.opcode = CMDQ_OP_TLBI_NSNH_ALL,
	};

	/* Invalidate all cached configs and TLBs */
	ret = smmu_write_cr0(smmu, CR0_CMDQEN);
	if (ret)
		return ret;

	ret = smmu_add_cmd(smmu, &cfgi_cmd);
	if (ret)
		goto err_disable_cmdq;

	ret = smmu_add_cmd(smmu, &tlbi_cmd);
	if (ret)
		goto err_disable_cmdq;

	ret = smmu_sync_cmd(smmu);
	if (ret)
		goto err_disable_cmdq;

	/* Enable translation */
	return smmu_write_cr0(smmu, CR0_SMMUEN | CR0_CMDQEN | CR0_ATSCHK | CR0_EVTQEN);

err_disable_cmdq:
	return smmu_write_cr0(smmu, 0);
}

static int smmu_init_device(struct hyp_arm_smmu_v3_device *smmu)
{
	int ret;

	if (!PAGE_ALIGNED(smmu->mmio_addr | smmu->mmio_size))
		return -EINVAL;

	ret = ___pkvm_host_donate_hyp(smmu->mmio_addr >> PAGE_SHIFT,
				      smmu->mmio_size >> PAGE_SHIFT,
				      /* accept_mmio */ true);
	if (ret)
		return ret;

	smmu->base = hyp_phys_to_virt(smmu->mmio_addr);

	ret = smmu_init_registers(smmu);
	if (ret)
		return ret;

	ret = smmu_init_cmdq(smmu);
	if (ret)
		return ret;

	ret = smmu_init_evtq(smmu);
	if (ret)
		return ret;

	ret = smmu_init_strtab(smmu);
	if (ret)
		return ret;

	ret = smmu_reset_device(smmu);
	if (ret)
		return ret;

	return kvm_iommu_init_device(&smmu->iommu);
}

static int smmu_init(void)
{
	int ret;
	struct hyp_arm_smmu_v3_device *smmu;
	size_t smmu_arr_size = PAGE_ALIGN(sizeof(*kvm_hyp_arm_smmu_v3_smmus) *
					  kvm_hyp_arm_smmu_v3_count);
	phys_addr_t smmu_arr_phys;

	kvm_hyp_arm_smmu_v3_smmus = kern_hyp_va(kvm_hyp_arm_smmu_v3_smmus);

	smmu_arr_phys = hyp_virt_to_phys(kvm_hyp_arm_smmu_v3_smmus);

	ret = smmu_take_pages(smmu_arr_phys, smmu_arr_size);
	if (ret)
		return ret;

	for_each_smmu(smmu) {
		ret = smmu_init_device(smmu);
		if (ret)
			goto out_reclaim_smmu;
	}

	return 0;
out_reclaim_smmu:
	smmu_reclaim_pages(smmu_arr_phys, smmu_arr_size);
	return ret;
}

static struct kvm_hyp_iommu *smmu_id_to_iommu(pkvm_handle_t smmu_id)
{
	if (smmu_id >= kvm_hyp_arm_smmu_v3_count)
		return NULL;
	smmu_id = array_index_nospec(smmu_id, kvm_hyp_arm_smmu_v3_count);

	return &kvm_hyp_arm_smmu_v3_smmus[smmu_id].iommu;
}

static int smmu_alloc_domain(struct kvm_hyp_iommu_domain *domain, int type)
{
	struct hyp_arm_smmu_v3_domain *smmu_domain;

	if (type >= KVM_ARM_SMMU_DOMAIN_MAX)
		return -EINVAL;

	smmu_domain = hyp_alloc(sizeof(*smmu_domain));
	if (!smmu_domain)
		return -ENOMEM;

	INIT_LIST_HEAD(&smmu_domain->iommu_list);
	hyp_rwlock_init(&smmu_domain->list_lock);
	/*
	 * Can't do much without knowing the SMMUv3.
	 * Page table will be allocated at attach_dev, but can be
	 * freed from free domain.
	 */
	smmu_domain->domain = domain;
	smmu_domain->type = type;
	hyp_spin_lock_init(&smmu_domain->pgt_lock);
	domain->priv = (void *)smmu_domain;

	return 0;
}

static void smmu_free_domain(struct kvm_hyp_iommu_domain *domain)
{
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;

	if (smmu_domain->pgtable)
		kvm_arm_io_pgtable_free(smmu_domain->pgtable);

	hyp_free(smmu_domain);
}

static void smmu_inv_domain(struct hyp_arm_smmu_v3_device *smmu,
			    struct hyp_arm_smmu_v3_domain *smmu_domain)
{
	struct kvm_hyp_iommu_domain *domain = smmu_domain->domain;
	struct arm_smmu_cmdq_ent cmd = {};

	if (smmu_domain->pgtable->cfg.fmt == ARM_64_LPAE_S2) {
		cmd.opcode = CMDQ_OP_TLBI_S12_VMALL;
		cmd.tlbi.vmid = domain->domain_id;
	} else {
		cmd.opcode = CMDQ_OP_TLBI_NH_ASID;
		cmd.tlbi.asid = domain->domain_id;
	}

	if (smmu->iommu.power_is_off)
		return;

	WARN_ON(smmu_send_cmd(smmu, &cmd));
}

static void smmu_tlb_flush_all(void *cookie)
{
	struct kvm_hyp_iommu_domain *domain = cookie;
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;
	struct hyp_arm_smmu_v3_device *smmu;
	struct domain_iommu_node *iommu_node;

	hyp_read_lock(&smmu_domain->list_lock);
	list_for_each_entry(iommu_node, &smmu_domain->iommu_list, list) {
		smmu = to_smmu(iommu_node->iommu);
		kvm_iommu_lock(&smmu->iommu);
		smmu_inv_domain(smmu, smmu_domain);
		kvm_iommu_unlock(&smmu->iommu);
	}
	hyp_read_unlock(&smmu_domain->list_lock);
}

static void smmu_cmdq_batch_add(struct hyp_arm_smmu_v3_device *smmu,
				struct arm_smmu_cmdq_batch *cmds,
				struct arm_smmu_cmdq_ent *cmd)
{
	int index;

	if (cmds->num == CMDQ_BATCH_ENTRIES) {
		smmu_issue_cmds(smmu, cmds->cmds, cmds->num);
		cmds->num = 0;
	}

	index = cmds->num * CMDQ_ENT_DWORDS;
	smmu_build_cmd(&cmds->cmds[index], cmd);

	cmds->num++;
}

static int smmu_tlb_inv_range_smmu(struct hyp_arm_smmu_v3_device *smmu,
				   struct kvm_hyp_iommu_domain *domain,
				   struct arm_smmu_cmdq_ent *cmd,
				   unsigned long iova, size_t size, size_t granule)
{
	int ret = 0;
	unsigned long end = iova + size, num_pages = 0, tg = 0;
	size_t inv_range = granule;
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;
	struct arm_smmu_cmdq_batch cmds;

	kvm_iommu_lock(&smmu->iommu);
	if (smmu->iommu.power_is_off)
		goto out_ret;

	/* Almost copy-paste from the kernel dirver. */
	if (smmu->features & ARM_SMMU_FEAT_RANGE_INV) {
		/* Get the leaf page size */
		tg = __ffs(smmu_domain->pgtable->cfg.pgsize_bitmap);

		num_pages = size >> tg;

		/* Convert page size of 12,14,16 (log2) to 1,2,3 */
		cmd->tlbi.tg = (tg - 10) / 2;

		/*
		 * Determine what level the granule is at. For non-leaf, both
		 * io-pgtable and SVA pass a nominal last-level granule because
		 * they don't know what level(s) actually apply, so ignore that
		 * and leave TTL=0. However for various errata reasons we still
		 * want to use a range command, so avoid the SVA corner case
		 * where both scale and num could be 0 as well.
		 */
		if (cmd->tlbi.leaf)
			cmd->tlbi.ttl = 4 - ((ilog2(granule) - 3) / (tg - 3));
		else if ((num_pages & CMDQ_TLBI_RANGE_NUM_MAX) == 1)
			num_pages++;
	}

	cmds.num = 0;

	while (iova < end) {
		if (smmu->features & ARM_SMMU_FEAT_RANGE_INV) {
			/*
			 * On each iteration of the loop, the range is 5 bits
			 * worth of the aligned size remaining.
			 * The range in pages is:
			 *
			 * range = (num_pages & (0x1f << __ffs(num_pages)))
			 */
			unsigned long scale, num;

			/* Determine the power of 2 multiple number of pages */
			scale = __ffs(num_pages);
			cmd->tlbi.scale = scale;

			/* Determine how many chunks of 2^scale size we have */
			num = (num_pages >> scale) & CMDQ_TLBI_RANGE_NUM_MAX;
			cmd->tlbi.num = num - 1;

			/* range is num * 2^scale * pgsize */
			inv_range = num << (scale + tg);

			/* Clear out the lower order bits for the next iteration */
			num_pages -= num << scale;
		}
		cmd->tlbi.addr = iova;
		smmu_cmdq_batch_add(smmu, &cmds, cmd);
		BUG_ON(iova + inv_range < iova);
		iova += inv_range;
	}

	WARN_ON(smmu_issue_cmds(smmu, cmds.cmds, cmds.num));
	ret = smmu_sync_cmd(smmu);
out_ret:
	kvm_iommu_unlock(&smmu->iommu);
	return ret;
}

static void smmu_tlb_inv_range(struct kvm_hyp_iommu_domain *domain,
			       unsigned long iova, size_t size, size_t granule,
			       bool leaf)
{
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;
	struct hyp_arm_smmu_v3_device *smmu;
	struct domain_iommu_node *iommu_node;
	unsigned long end = iova + size;
	struct arm_smmu_cmdq_ent cmd;

	cmd.tlbi.leaf = leaf;
	if (smmu_domain->pgtable->cfg.fmt == ARM_64_LPAE_S2) {
		cmd.opcode = CMDQ_OP_TLBI_S2_IPA;
		cmd.tlbi.vmid = domain->domain_id;
	} else {
		cmd.opcode = CMDQ_OP_TLBI_NH_VA;
		cmd.tlbi.asid = domain->domain_id;
		cmd.tlbi.vmid = 0;
	}
	/*
	 * There are no mappings at high addresses since we don't use TTB1, so
	 * no overflow possible.
	 */
	BUG_ON(end < iova);
	hyp_read_lock(&smmu_domain->list_lock);
	list_for_each_entry(iommu_node, &smmu_domain->iommu_list, list) {
		smmu = to_smmu(iommu_node->iommu);
		WARN_ON(smmu_tlb_inv_range_smmu(smmu, domain,
						&cmd, iova, size, granule));
	}
	hyp_read_unlock(&smmu_domain->list_lock);
}

static void smmu_unmap_visit_leaf(phys_addr_t addr, size_t size,
				  struct io_pgtable_walk_common *data,
				  void *wd)
{
	u64 *ptep = wd;
	u64 pte = *ptep;

	/* Might be a cleared table. */
	if (!pte)
		return;
	WARN_ON(iommu_pkvm_unuse_dma(addr, size));
	*ptep = 0;
}

static void smmu_tlb_flush_walk(unsigned long iova, size_t size,
				size_t granule, void *cookie)
{
	struct kvm_hyp_iommu_domain *domain = cookie;

	smmu_tlb_inv_range(domain, iova, size, granule, false);
}

static void smmu_tlb_add_page(struct iommu_iotlb_gather *gather,
			      unsigned long iova, size_t granule,
			      void *cookie)
{
	if (gather)
		kvm_iommu_iotlb_gather_add_page(cookie, gather, iova, granule);
	else
		smmu_tlb_inv_range(cookie, iova, granule, granule, true);
}

static void smmu_free_leaf(unsigned long phys, size_t granule, void *cookie)
{
	WARN_ON(iommu_pkvm_unuse_dma(phys, granule));
}

static const struct iommu_flush_ops smmu_tlb_ops = {
	.tlb_flush_all	= smmu_tlb_flush_all,
	.tlb_flush_walk = smmu_tlb_flush_walk,
	.tlb_add_page	= smmu_tlb_add_page,
	.free_leaf	= smmu_free_leaf,
};

static void smmu_iotlb_sync(struct kvm_hyp_iommu_domain *domain,
			    struct iommu_iotlb_gather *gather)
{
	size_t size;
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;
	struct io_pgtable *pgtable = smmu_domain->pgtable;
	struct arm_lpae_io_pgtable *data = io_pgtable_to_data(pgtable);
	struct arm_lpae_io_pgtable_walk_data wd = {
		.cookie = data,
	};
	struct io_pgtable_walk_common walk_data = {
		.visit_leaf = smmu_unmap_visit_leaf,
		.data = &wd,
	};

	if (!gather->pgsize)
		return;
	size = gather->end - gather->start + 1;
	smmu_tlb_inv_range(domain, gather->start, size,  gather->pgsize, true);

	/*
	 * Now decrement the refcount of unmapped pages thanks to
	 * IO_PGTABLE_QUIRK_UNMAP_INVAL
	 */
	pgtable->ops.pgtable_walk(&pgtable->ops, gather->start, size, &walk_data);
}

static int smmu_domain_config_s2(struct kvm_hyp_iommu_domain *domain,
				 struct arm_smmu_ste *ste)
{
	struct io_pgtable_cfg *cfg;
	u64 ts, sl, ic, oc, sh, tg, ps;
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;

	cfg = &smmu_domain->pgtable->cfg;
	ps = cfg->arm_lpae_s2_cfg.vtcr.ps;
	tg = cfg->arm_lpae_s2_cfg.vtcr.tg;
	sh = cfg->arm_lpae_s2_cfg.vtcr.sh;
	oc = cfg->arm_lpae_s2_cfg.vtcr.orgn;
	ic = cfg->arm_lpae_s2_cfg.vtcr.irgn;
	sl = cfg->arm_lpae_s2_cfg.vtcr.sl;
	ts = cfg->arm_lpae_s2_cfg.vtcr.tsz;

	ste->data[0] = STRTAB_STE_0_V |
		FIELD_PREP(STRTAB_STE_0_CFG, STRTAB_STE_0_CFG_S2_TRANS);
	ste->data[1] = FIELD_PREP(STRTAB_STE_1_SHCFG, STRTAB_STE_1_SHCFG_INCOMING);
	ste->data[2] = FIELD_PREP(STRTAB_STE_2_VTCR,
			FIELD_PREP(STRTAB_STE_2_VTCR_S2PS, ps) |
			FIELD_PREP(STRTAB_STE_2_VTCR_S2TG, tg) |
			FIELD_PREP(STRTAB_STE_2_VTCR_S2SH0, sh) |
			FIELD_PREP(STRTAB_STE_2_VTCR_S2OR0, oc) |
			FIELD_PREP(STRTAB_STE_2_VTCR_S2IR0, ic) |
			FIELD_PREP(STRTAB_STE_2_VTCR_S2SL0, sl) |
			FIELD_PREP(STRTAB_STE_2_VTCR_S2T0SZ, ts)) |
		 FIELD_PREP(STRTAB_STE_2_S2VMID, domain->domain_id) |
		 STRTAB_STE_2_S2AA64 | STRTAB_STE_2_S2R;
	ste->data[3] = cfg->arm_lpae_s2_cfg.vttbr & STRTAB_STE_3_S2TTB_MASK;

	return 0;
}

static u64 *smmu_domain_config_s1_ste(struct hyp_arm_smmu_v3_device *smmu,
				      u32 pasid_bits, struct arm_smmu_ste *ste)
{
	u64 *cd_table;

	cd_table = smmu_alloc_cd(smmu, pasid_bits);
	if (!cd_table)
		return NULL;

	ste->data[1] = FIELD_PREP(STRTAB_STE_1_S1DSS, STRTAB_STE_1_S1DSS_SSID0) |
		FIELD_PREP(STRTAB_STE_1_S1CIR, STRTAB_STE_1_S1C_CACHE_WBRA) |
		FIELD_PREP(STRTAB_STE_1_S1COR, STRTAB_STE_1_S1C_CACHE_WBRA) |
		FIELD_PREP(STRTAB_STE_1_S1CSH, ARM_SMMU_SH_ISH);
	ste->data[0] = ((u64)cd_table & STRTAB_STE_0_S1CTXPTR_MASK) |
		FIELD_PREP(STRTAB_STE_0_CFG, STRTAB_STE_0_CFG_S1_TRANS) |
		FIELD_PREP(STRTAB_STE_0_S1CDMAX, pasid_bits) |
		FIELD_PREP(STRTAB_STE_0_S1FMT, STRTAB_STE_0_S1FMT_LINEAR) |
		STRTAB_STE_0_V;

	return cd_table;
}

/*
 * This function handles configuration for pasid and non-pasid domains
 * with the following assumptions:
 * - pasid 0 always attached first, this should be the typicall flow
 *   for the kernel where attach_dev is always called before set_dev_pasid.
 *   In that case only pasid 0 is allowed to allocate memory for the CD,
 *   and other pasids would expect to find the tabel.
 * - pasid 0 is detached last, also guaranteed from the kernel.
 */
static int smmu_domain_config_s1(struct hyp_arm_smmu_v3_device *smmu,
				 struct kvm_hyp_iommu_domain *domain,
				 u32 sid, u32 pasid, u32 pasid_bits,
				 struct arm_smmu_ste *ste)
{
	struct arm_smmu_ste *dst;
	u64 val;
	u64 *cd_entry, *cd_table;
	struct io_pgtable_cfg *cfg;
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;

	cfg = &smmu_domain->pgtable->cfg;
	dst = smmu_get_ste_ptr(smmu, sid);
	val = dst->data[0];

	if (FIELD_GET(STRTAB_STE_0_CFG, val) == STRTAB_STE_0_CFG_S2_TRANS)
		return -EBUSY;

	if (pasid == 0) {
		cd_table = smmu_domain_config_s1_ste(smmu, pasid_bits, ste);
		if (!cd_table)
			return -ENOMEM;
	} else {
		u32 nr_entries;

		cd_table = (u64 *)(FIELD_GET(STRTAB_STE_0_S1CTXPTR_MASK, val) << 6);
		if (!cd_table)
			return -EINVAL;
		nr_entries = 1 << FIELD_GET(STRTAB_STE_0_S1CDMAX, val);
		if (pasid >= nr_entries)
			return -E2BIG;
	}

	/* Write CD. */
	cd_entry = smmu_get_cd_ptr(hyp_phys_to_virt((u64)cd_table), pasid);

	/* CD already used by another device. */
	if (cd_entry[0])
		return -EBUSY;

	cd_entry[1] = cpu_to_le64(cfg->arm_lpae_s1_cfg.ttbr & CTXDESC_CD_1_TTB0_MASK);
	cd_entry[2] = 0;
	cd_entry[3] = cpu_to_le64(cfg->arm_lpae_s1_cfg.mair);

	/* STE is live. */
	if (pasid)
		smmu_sync_cd(smmu, sid, pasid);
	val =  FIELD_PREP(CTXDESC_CD_0_TCR_T0SZ, cfg->arm_lpae_s1_cfg.tcr.tsz) |
	       FIELD_PREP(CTXDESC_CD_0_TCR_TG0, cfg->arm_lpae_s1_cfg.tcr.tg) |
	       FIELD_PREP(CTXDESC_CD_0_TCR_IRGN0, cfg->arm_lpae_s1_cfg.tcr.irgn) |
	       FIELD_PREP(CTXDESC_CD_0_TCR_ORGN0, cfg->arm_lpae_s1_cfg.tcr.orgn) |
	       FIELD_PREP(CTXDESC_CD_0_TCR_SH0, cfg->arm_lpae_s1_cfg.tcr.sh) |
	       FIELD_PREP(CTXDESC_CD_0_TCR_IPS, cfg->arm_lpae_s1_cfg.tcr.ips) |
	       CTXDESC_CD_0_TCR_EPD1 | CTXDESC_CD_0_AA64 |
	       CTXDESC_CD_0_R | CTXDESC_CD_0_A |
	       CTXDESC_CD_0_ASET |
	       FIELD_PREP(CTXDESC_CD_0_ASID, domain->domain_id) |
	       CTXDESC_CD_0_V;
	WRITE_ONCE(cd_entry[0], cpu_to_le64(val));
	/* STE is live. */
	if (pasid)
		smmu_sync_cd(smmu, sid, pasid);
	return 0;
}

static int smmu_domain_finalise(struct hyp_arm_smmu_v3_device *smmu,
				struct kvm_hyp_iommu_domain *domain)
{
	int ret;
	struct io_pgtable_cfg cfg;
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;
	struct arm_lpae_io_pgtable *data;
	bool idmapped = domain->domain_id == KVM_IOMMU_DOMAIN_IDMAP_ID;
	unsigned long quirks = idmapped ? 0 : IO_PGTABLE_QUIRK_UNMAP_INVAL;

	if (smmu_domain->type == KVM_ARM_SMMU_DOMAIN_S1) {
		size_t ias = (smmu->features & ARM_SMMU_FEAT_VAX) ? 52 : 48;

		cfg = (struct io_pgtable_cfg) {
			.fmt = ARM_64_LPAE_S1,
			.pgsize_bitmap = smmu->pgsize_bitmap,
			.ias = min_t(unsigned long, ias, VA_BITS),
			.oas = smmu->ias,
			.coherent_walk = smmu->features & ARM_SMMU_FEAT_COHERENCY,
			.tlb = &smmu_tlb_ops,
			.quirks = quirks,
		};
	} else {
		cfg = (struct io_pgtable_cfg) {
			.fmt = ARM_64_LPAE_S2,
			.pgsize_bitmap = smmu->pgsize_bitmap,
			.ias = smmu->ias,
			.oas = smmu->oas,
			.coherent_walk = smmu->features & ARM_SMMU_FEAT_COHERENCY,
			.tlb = &smmu_tlb_ops,
			.quirks = quirks,
		};
	}

	hyp_spin_lock(&smmu_domain->pgt_lock);
	smmu_domain->pgtable = kvm_arm_io_pgtable_alloc(&cfg, domain, &ret);
	hyp_spin_unlock(&smmu_domain->pgt_lock);
	if (ret)
		return ret;

	data = io_pgtable_to_data(smmu_domain->pgtable);
	data->idmapped = idmapped;
	return ret;
}

static bool smmu_domain_compat(struct hyp_arm_smmu_v3_device *smmu,
			       struct hyp_arm_smmu_v3_domain *smmu_domain)
{
	struct io_pgtable_cfg *cfg;

	/* Domain is empty. */
	if (!smmu_domain->pgtable)
		return true;

	if (smmu_domain->type == KVM_ARM_SMMU_DOMAIN_S2) {
		if (!(smmu->features & ARM_SMMU_FEAT_TRANS_S2))
			return false;
	} else {
		if (!(smmu->features & ARM_SMMU_FEAT_TRANS_S1))
			return false;
	}

	cfg = &smmu_domain->pgtable->cfg;

	/* Best effort. */
	return  ((smmu->pgsize_bitmap | cfg->pgsize_bitmap) == smmu->pgsize_bitmap);
}

static bool smmu_existing_in_domain(struct hyp_arm_smmu_v3_device *smmu,
				    struct hyp_arm_smmu_v3_domain *smmu_domain)
{
	struct domain_iommu_node *iommu_node;
	struct hyp_arm_smmu_v3_device *other;

	hyp_assert_write_lock_held(&smmu_domain->list_lock);

	list_for_each_entry(iommu_node, &smmu_domain->iommu_list, list) {
		other = to_smmu(iommu_node->iommu);
		if (other == smmu)
			return true;
	}

	return false;
}

static void smmu_get_ref_domain(struct hyp_arm_smmu_v3_device *smmu,
				struct hyp_arm_smmu_v3_domain *smmu_domain)
{
	struct domain_iommu_node *iommu_node;
	struct hyp_arm_smmu_v3_device *other;

	hyp_assert_write_lock_held(&smmu_domain->list_lock);

	list_for_each_entry(iommu_node, &smmu_domain->iommu_list, list) {
		other = to_smmu(iommu_node->iommu);
		if (other == smmu) {
			iommu_node->ref++;
			return;
		}
	}
}

static void smmu_put_ref_domain(struct hyp_arm_smmu_v3_device *smmu,
				struct hyp_arm_smmu_v3_domain *smmu_domain)
{
	struct domain_iommu_node *iommu_node, *temp;
	struct hyp_arm_smmu_v3_device *other;

	hyp_assert_write_lock_held(&smmu_domain->list_lock);

	list_for_each_entry_safe(iommu_node, temp, &smmu_domain->iommu_list, list) {
		other = to_smmu(iommu_node->iommu);
		if (other == smmu) {
			iommu_node->ref--;
			if (iommu_node->ref == 0) {
				/*
				 * Ensure no stale tlb entries when domain_id
				 * is re-used for this SMMU.
				 */
				smmu_inv_domain(smmu, smmu_domain);

				list_del(&iommu_node->list);
				hyp_free(iommu_node);
			}
			return;
		}
	}
}

static int smmu_fix_up_domains(struct hyp_arm_smmu_v3_device *smmu,
			       struct hyp_arm_smmu_v3_domain *smmu_domain)
{
	/*
	 * BYPASS domains only supported on stage-2 instances, that is over restrictive
	 * but for now as stage-1 is limited to VA_BITS to match the kernel, it might
	 * not cover the ia bits, we don't support it.
	 */
	if (smmu_domain->type == KVM_ARM_SMMU_DOMAIN_BYPASS) {
		if (smmu->features & ARM_SMMU_FEAT_TRANS_S2)
			smmu_domain->type = KVM_ARM_SMMU_DOMAIN_S2;
		else
			return -EINVAL;
	} else if (smmu_domain->type == KVM_ARM_SMMU_DOMAIN_ANY) {
		/* Any domain defaults to S1 as we don't know if the guest needs pasid. */
		if (smmu->features & ARM_SMMU_FEAT_TRANS_S1) {
			smmu_domain->type = KVM_ARM_SMMU_DOMAIN_S1;
		} else {
			smmu_domain->type = KVM_ARM_SMMU_DOMAIN_S2;
		}
	}

	return 0;
}

static int smmu_attach_dev(struct kvm_hyp_iommu *iommu, struct kvm_hyp_iommu_domain *domain,
			   u32 sid, u32 pasid, u32 pasid_bits, unsigned long flags)
{
	int i;
	int ret;
	struct arm_smmu_ste *dst;
	struct arm_smmu_ste ste = {};
	struct hyp_arm_smmu_v3_device *smmu = to_smmu(iommu);
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;
	struct domain_iommu_node *iommu_node = NULL;
	bool init_idmap = false;

	hyp_write_lock(&smmu_domain->list_lock);
	kvm_iommu_lock(iommu);
	dst = smmu_get_alloc_ste_ptr(smmu, sid);
	if (!dst) {
		ret = -ENOMEM;
		goto out_unlock;
	}

	/* Map domain type to an SMMUv3 stage. */
	ret = smmu_fix_up_domains(smmu, smmu_domain);
	if (ret)
		goto out_unlock;

	if (!smmu_existing_in_domain(smmu, smmu_domain)) {
		if (!smmu_domain_compat(smmu, smmu_domain)) {
			ret = -EBUSY;
			goto out_unlock;
		}
		iommu_node = hyp_alloc(sizeof(struct domain_iommu_node));
		if (!iommu_node) {
			ret = -ENOMEM;
			goto out_unlock;
		}
		iommu_node->iommu = iommu;
		iommu_node->ref = 1;
	} else {
		smmu_get_ref_domain(smmu, smmu_domain);
	}

	if (!smmu_domain->pgtable) {
		ret = smmu_domain_finalise(smmu, domain);
		if (ret)
			goto out_unlock;
		if (domain->domain_id == KVM_IOMMU_DOMAIN_IDMAP_ID)
			init_idmap = true;
	}

	if (smmu_domain->type == KVM_ARM_SMMU_DOMAIN_S2) {
		/* Device already attached or pasid for s2. */
		if (dst->data[0] || pasid) {
			ret = -EBUSY;
			goto out_unlock;
		}
		ret = smmu_domain_config_s2(domain, &ste);
	} else {
		/*
		 * Allocate and config CD, and update CD if possible.
		 */
		pasid_bits = min(pasid_bits, smmu->ssid_bits);
		ret = smmu_domain_config_s1(smmu, domain, sid, pasid,
					    pasid_bits, &ste);
	}
	/* We don't update STEs for pasid domains. */
	if (ret || pasid)
		goto out_unlock;

	/*
	 * The SMMU may cache a disabled STE.
	 * Initialize all fields, sync, then enable it.
	 */
	for (i = 1; i < STRTAB_STE_DWORDS; i++)
		dst->data[i] = ste.data[i];

	ret = smmu_sync_ste(smmu, sid);
	if (ret)
		goto out_unlock;

	WRITE_ONCE(dst->data[0], ste.data[0]);
	ret = smmu_sync_ste(smmu, sid);
	WARN_ON(ret);
out_unlock:
	if (iommu_node && ret)
		hyp_free(iommu_node);
	else if (iommu_node)
		list_add_tail(&iommu_node->list, &smmu_domain->iommu_list);

	kvm_iommu_unlock(iommu);
	hyp_write_unlock(&smmu_domain->list_lock);

	if (init_idmap)
		ret = kvm_iommu_snapshot_host_stage2(domain);

	return ret;
}

static int smmu_detach_dev(struct kvm_hyp_iommu *iommu, struct kvm_hyp_iommu_domain *domain,
			   u32 sid, u32 pasid)
{
	struct arm_smmu_ste *dst;
	int i, ret;
	struct hyp_arm_smmu_v3_device *smmu = to_smmu(iommu);
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;
	u32 pasid_bits = 0;
	u64 *cd_table, *cd;

	hyp_write_lock(&smmu_domain->list_lock);
	kvm_iommu_lock(iommu);
	dst = smmu_get_ste_ptr(smmu, sid);
	if (!dst) {
		ret = -ENODEV;
		goto out_unlock;
	}

	/*
	 * For stage-1:
	 * - The kernel has to detach pasid = 0 the last.
	 * - This will free the CD.
	 */
	if (smmu_domain->type == KVM_ARM_SMMU_DOMAIN_S1) {
		pasid_bits = FIELD_GET(STRTAB_STE_0_S1CDMAX, dst->data[0]);
		if (pasid >= (1 << pasid_bits)) {
			ret = -E2BIG;
			goto out_unlock;
		}
		cd_table = (u64 *)(dst->data[0] & STRTAB_STE_0_S1CTXPTR_MASK);
		if (WARN_ON(!cd_table)) {
			ret = -ENODEV;
			goto out_unlock;
		}

		cd_table = hyp_phys_to_virt((phys_addr_t)cd_table);
		if (pasid == 0) {
			int j;

			/* Ensure other pasids are detached. */
			for (j = 1 ; j < (1 << pasid_bits) ; ++j) {
				cd = smmu_get_cd_ptr(cd_table, j);
				if (cd[0] & CTXDESC_CD_0_V) {
					ret = -EINVAL;
					goto out_unlock;
				}
			}

			smmu_free_cd(cd_table, pasid_bits);
		} else {
			cd = smmu_get_cd_ptr(cd_table, pasid);
			cd[0] = 0;
			smmu_sync_cd(smmu, sid, pasid);
			cd[1] = 0;
			cd[2] = 0;
			cd[3] = 0;
			ret = smmu_sync_cd(smmu, sid, pasid);
			goto out_unlock;
		}
	}
	/* For stage-2 and pasid = 0 */
	dst->data[0] = 0;
	ret = smmu_sync_ste(smmu, sid);
	if (ret)
		goto out_unlock;
	for (i = 1; i < STRTAB_STE_DWORDS; i++)
		dst->data[i] = 0;

	ret = smmu_sync_ste(smmu, sid);

	smmu_put_ref_domain(smmu, smmu_domain);
out_unlock:
	kvm_iommu_unlock(iommu);
	hyp_write_unlock(&smmu_domain->list_lock);
	return ret;
}

static int smmu_map_pages(struct kvm_hyp_iommu_domain *domain, unsigned long iova,
			  phys_addr_t paddr, size_t pgsize,
			  size_t pgcount, int prot, size_t *total_mapped)
{
	size_t mapped;
	size_t granule;
	int ret = 0;
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;
	struct io_pgtable *pgtable = smmu_domain->pgtable;

	if (!pgtable)
		return -EINVAL;

	granule = 1UL << __ffs(smmu_domain->pgtable->cfg.pgsize_bitmap);
	if (!IS_ALIGNED(iova | paddr | pgsize, granule))
		return -EINVAL;

	hyp_spin_lock(&smmu_domain->pgt_lock);
	while (pgcount) {
		mapped = 0;
		ret = pgtable->ops.map_pages(&pgtable->ops, iova, paddr,
					     pgsize, pgcount, prot, 0, &mapped);
		if (ret)
			break;
		WARN_ON(!IS_ALIGNED(mapped, pgsize));
		WARN_ON(mapped > pgcount * pgsize);

		pgcount -= mapped / pgsize;
		*total_mapped += mapped;
		iova += mapped;
		paddr += mapped;
	}
	hyp_spin_unlock(&smmu_domain->pgt_lock);

	return ret;
}

static size_t smmu_unmap_pages(struct kvm_hyp_iommu_domain *domain, unsigned long iova,
			       size_t pgsize, size_t pgcount, struct iommu_iotlb_gather *gather)
{
	size_t granule, unmapped, total_unmapped = 0;
	size_t size = pgsize * pgcount;
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;
	struct io_pgtable *pgtable = smmu_domain->pgtable;

	if (!pgtable)
		return -EINVAL;

	granule = 1UL << __ffs(smmu_domain->pgtable->cfg.pgsize_bitmap);
	if (!IS_ALIGNED(iova | pgsize, granule))
		return 0;

	hyp_spin_lock(&smmu_domain->pgt_lock);
	while (total_unmapped < size) {
		unmapped = pgtable->ops.unmap_pages(&pgtable->ops, iova, pgsize,
						    pgcount, gather);
		if (!unmapped)
			break;
		iova += unmapped;
		total_unmapped += unmapped;
		pgcount -= unmapped / pgsize;
	}
	hyp_spin_unlock(&smmu_domain->pgt_lock);
	return total_unmapped;
}

static phys_addr_t smmu_iova_to_phys(struct kvm_hyp_iommu_domain *domain,
				     unsigned long iova)
{
	phys_addr_t paddr;
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;
	struct io_pgtable *pgtable = smmu_domain->pgtable;

	if (!pgtable)
		return -EINVAL;

	hyp_spin_lock(&smmu_domain->pgt_lock);
	paddr = pgtable->ops.iova_to_phys(&pgtable->ops, iova);
	hyp_spin_unlock(&smmu_domain->pgt_lock);

	return paddr;
}

static bool smmu_dabt_device(struct hyp_arm_smmu_v3_device *smmu,
			     struct user_pt_regs *regs,
			     u64 esr, u32 off)
{
	bool is_write = esr & ESR_ELx_WNR;
	unsigned int len = BIT((esr & ESR_ELx_SAS) >> ESR_ELx_SAS_SHIFT);
	int rd = (esr & ESR_ELx_SRT_MASK) >> ESR_ELx_SRT_SHIFT;
	const u32 no_access  = 0;
	const u32 read_write = (u32)(-1);
	const u32 read_only = is_write ? no_access : read_write;
	u32 mask = no_access;

	/*
	 * Only handle MMIO access with u32 size and alignment.
	 * We don't need to change 64-bit registers for now.
	 */
	if ((len != sizeof(u32)) || (off & (sizeof(u32) - 1)))
		return false;

	switch (off) {
	case ARM_SMMU_EVTQ_PROD + SZ_64K:
		mask = read_write;
		break;
	case ARM_SMMU_EVTQ_CONS + SZ_64K:
		mask = read_write;
		break;
	case ARM_SMMU_GERROR:
		mask = read_only;
		break;
	case ARM_SMMU_GERRORN:
		mask = read_write;
		break;
	};

	if (!mask)
		return false;
	if (is_write)
		writel_relaxed(regs->regs[rd] & mask, smmu->base + off);
	else
		regs->regs[rd] = readl_relaxed(smmu->base + off);

	return true;
}

static int smmu_id_to_token(pkvm_handle_t smmu_id, u64 *out_token)
{
	if (smmu_id >= kvm_hyp_arm_smmu_v3_count)
		return -EINVAL;

	smmu_id = array_index_nospec(smmu_id, kvm_hyp_arm_smmu_v3_count);
	*out_token = kvm_hyp_arm_smmu_v3_smmus[smmu_id].mmio_addr;
	return 0;
}

static int smmu_dev_block_dma(struct kvm_hyp_iommu *iommu, u32 sid, bool is_host2guest)
{
	struct hyp_arm_smmu_v3_device *smmu = to_smmu(iommu);
	static struct arm_smmu_ste *dst;
	int ret = 0;

	kvm_iommu_lock(iommu);
	dst = smmu_get_ste_ptr(smmu, sid);

	/*
	 * VFIO will attach the device to a blocking domain, this will make the
	 * kernel driver detach the device which should be have zeroed STE.
	 * So, if this is not the current state of the device, something
	 * went wrong.
	 * For guests, we need to do more as guests might not exit cleanly
	 * and the device might be translating, so we have to actually block
	 * the device and clean the STE/CD.
	 */
	if (dst->data[0]) {
		if (is_host2guest) {
			ret = -EINVAL;
		} else {
			int i = 0;
			u32 cfg = FIELD_GET(STRTAB_STE_0_CFG, dst->data[0]);

			if (cfg == STRTAB_STE_0_CFG_S1_TRANS) {
				size_t nr_entries, cd_sz;
				u64 cd_table;

				cd_table = (dst->data[0] & STRTAB_STE_0_S1CTXPTR_MASK);
				nr_entries = 1 << FIELD_GET(STRTAB_STE_0_S1CDMAX, dst->data[0]);
				cd_sz = (1 << nr_entries) * (CTXDESC_CD_DWORDS << 3);
				kvm_iommu_reclaim_pages(hyp_phys_to_virt(cd_table), get_order(cd_sz));
			}
			/* zap zippity zop. */
			for (i = 0; i < STRTAB_STE_DWORDS; i++)
				dst->data[i] = 0;
			ret = smmu_sync_ste(smmu, sid);
		}
	}

	kvm_iommu_unlock(iommu);
	return ret;
}

static bool smmu_dabt_handler(struct user_pt_regs *regs, u64 esr, u64 addr)
{
	struct hyp_arm_smmu_v3_device *smmu;

	for_each_smmu(smmu) {
		if (addr < smmu->mmio_addr || addr >= smmu->mmio_addr + smmu->mmio_size)
			continue;
		return smmu_dabt_device(smmu, regs, esr, addr - smmu->mmio_addr);
	}
	return false;
}

int smmu_suspend(struct kvm_hyp_iommu *iommu)
{
	struct hyp_arm_smmu_v3_device *smmu = to_smmu(iommu);

	/*
	 * Disable translation, GBPA is validated at probe to be set, so all translation
	 * would be aborted when SMMU is disabled.
	 */
	if (iommu->power_domain.type == KVM_POWER_DOMAIN_HOST_HVC)
		return smmu_write_cr0(smmu, 0);
	return 0;
}

int smmu_resume(struct kvm_hyp_iommu *iommu)
{
	struct hyp_arm_smmu_v3_device *smmu = to_smmu(iommu);

	/*
	 * Re-enable and clean all caches.
	 */
	if (iommu->power_domain.type == KVM_POWER_DOMAIN_HOST_HVC)
		return smmu_reset_device(smmu);
	return 0;
}

/*
 * Although SMMU can support multiple granules, it must at least support PAGE_SIZE
 * as the CPU, and for the IDMAP domains, we only use this granule.
 * As we optimize for memory usage and performance, we try to use block mappings
 * when possible.
 */
static size_t smmu_pgsize_idmap(size_t size, u64 paddr)
{
	size_t pgsizes;
	size_t pgsize_bitmask = 0;

	if (PAGE_SIZE == SZ_4K) {
		pgsize_bitmask = SZ_4K | SZ_2M | SZ_1G;
	} else if (PAGE_SIZE == SZ_16K) {
		pgsize_bitmask = SZ_16K | SZ_32M;
	} else if (PAGE_SIZE == SZ_64K){
		pgsize_bitmask = SZ_64K | SZ_512M;
	}

	/* All page sizes that fit the size */
	pgsizes = pgsize_bitmask & GENMASK_ULL(__fls(size), 0);

	/* Address must be aligned to page size */
	if (likely(paddr))
		pgsizes &= GENMASK_ULL(__ffs(paddr), 0);

	WARN_ON(!pgsizes);

	return BIT(__fls(pgsizes));
}

static void smmu_host_stage2_idmap(struct kvm_hyp_iommu_domain *domain,
				   phys_addr_t start, phys_addr_t end, int prot)
{
	size_t size = end - start;
	size_t pgsize, pgcount;
	size_t mapped, unmapped;
	int ret;
	struct hyp_arm_smmu_v3_domain *smmu_domain = domain->priv;
	struct io_pgtable *pgtable = smmu_domain->pgtable;

	end = min(end, BIT(pgtable->cfg.oas));
	if (start >= end)
		return;

	if (prot) {
		if (!(prot & IOMMU_MMIO))
			prot |= IOMMU_CACHE;

		while (size) {
			mapped = 0;
			pgsize = smmu_pgsize_idmap(size, start);
			pgcount = size / pgsize;
			ret = pgtable->ops.map_pages(&pgtable->ops, start, start,
						     pgsize, pgcount, prot, 0, &mapped);
			size -= mapped;
			start += mapped;
			if (!mapped || ret)
				return;
		}
	} else {
		while (size) {
			pgsize = smmu_pgsize_idmap(size, start);
			pgcount = size / pgsize;
			unmapped = pgtable->ops.unmap_pages(&pgtable->ops, start,
							    pgsize, pgcount, NULL);
			size -= unmapped;
			start += unmapped;
			if (!unmapped)
				return;
		}
	}
}

#ifdef MODULE
int smmu_init_hyp_module(const struct pkvm_module_ops *ops)
{
	if (!ops)
		return -EINVAL;

	mod_ops = ops;
	return 0;
}
#endif

/* Shared with the kernel driver in EL1 */
struct kvm_iommu_ops smmu_ops = {
	.init				= smmu_init,
	.get_iommu_by_id		= smmu_id_to_iommu,
	.alloc_domain			= smmu_alloc_domain,
	.free_domain			= smmu_free_domain,
	.iotlb_sync			= smmu_iotlb_sync,
	.attach_dev			= smmu_attach_dev,
	.detach_dev			= smmu_detach_dev,
	.map_pages			= smmu_map_pages,
	.unmap_pages			= smmu_unmap_pages,
	.iova_to_phys			= smmu_iova_to_phys,
	.dabt_handler			= smmu_dabt_handler,
	.suspend			= smmu_suspend,
	.resume				= smmu_resume,
	.host_stage2_idmap		= smmu_host_stage2_idmap,
	.dev_block_dma			= smmu_dev_block_dma,
	.get_iommu_token_by_id		= smmu_id_to_token,
};
