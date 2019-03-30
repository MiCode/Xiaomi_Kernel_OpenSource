/*
 * IOMMU API for ARM architected SMMU implementations.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) 2013 ARM Limited
 *
 * Author: Will Deacon <will.deacon@arm.com>
 *
 * This driver currently supports:
 *	- SMMUv1 and v2 implementations
 *	- Stream-matching and stream-indexing
 *	- v7/v8 long-descriptor format
 *	- Non-secure access to the SMMU
 *	- Context fault reporting
 *	- Extended Stream ID (16 bit)
 */

#define pr_fmt(fmt) "arm-smmu: " fmt

#include <linux/acpi.h>
#include <linux/acpi_iort.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/dma-iommu.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/io-64-nonatomic-hi-lo.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_iommu.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/of_platform.h>
#include <linux/msm-bus.h>
#include <trace/events/iommu.h>
#include <dt-bindings/msm/msm-bus-ids.h>
#include <linux/irq.h>
#include <linux/wait.h>

#include <linux/amba/bus.h>

#include <soc/qcom/msm_tz_smmu.h>
#include <soc/qcom/scm.h>
#include "io-pgtable.h"
#include "arm-smmu-regs.h"

#define ARM_MMU500_ACTLR_CPRE		(1 << 1)

#define ARM_MMU500_ACR_CACHE_LOCK	(1 << 26)
#define ARM_MMU500_ACR_SMTNMB_TLBEN	(1 << 8)

#define TLB_LOOP_TIMEOUT		500000	/* 500ms */
#define TLB_SPIN_COUNT			10

#define ARM_SMMU_IMPL_DEF0(smmu) \
	((smmu)->base + (2 * (1 << (smmu)->pgshift)))
#define ARM_SMMU_IMPL_DEF1(smmu) \
	((smmu)->base + (6 * (1 << (smmu)->pgshift)))

/* Maximum number of context banks per SMMU */
#define ARM_SMMU_MAX_CBS		128

/* SMMU global address space */
#define ARM_SMMU_GR0(smmu)		((smmu)->base)
#define ARM_SMMU_GR1(smmu)		((smmu)->base + (1 << (smmu)->pgshift))

/*
 * SMMU global address space with conditional offset to access secure
 * aliases of non-secure registers (e.g. nsCR0: 0x400, nsGFSR: 0x448,
 * nsGFSYNR0: 0x450)
 */
#define ARM_SMMU_GR0_NS(smmu)						\
	((smmu)->base +							\
		((smmu->options & ARM_SMMU_OPT_SECURE_CFG_ACCESS)	\
			? 0x400 : 0))

/*
 * Some 64-bit registers only make sense to write atomically, but in such
 * cases all the data relevant to AArch32 formats lies within the lower word,
 * therefore this actually makes more sense than it might first appear.
 */
#ifdef CONFIG_64BIT
#define smmu_write_atomic_lq		writeq_relaxed
#else
#define smmu_write_atomic_lq		writel_relaxed
#endif

/* Translation context bank */
#define ARM_SMMU_CB(smmu, n)	((smmu)->cb_base + ((n) << (smmu)->pgshift))

#define MSI_IOVA_BASE			0x8000000
#define MSI_IOVA_LENGTH			0x100000

static int force_stage;
module_param(force_stage, int, S_IRUGO);
MODULE_PARM_DESC(force_stage,
	"Force SMMU mappings to be installed at a particular stage of translation. A value of '1' or '2' forces the corresponding stage. All other values are ignored (i.e. no stage is forced). Note that selecting a specific stage will disable support for nested translation.");
static bool disable_bypass = true;
module_param(disable_bypass, bool, S_IRUGO);
MODULE_PARM_DESC(disable_bypass,
	"Disable bypass streams such that incoming transactions from devices that are not attached to an iommu domain will report an abort back to the device and will not be allowed to pass through the SMMU.");

enum arm_smmu_arch_version {
	ARM_SMMU_V1,
	ARM_SMMU_V1_64K,
	ARM_SMMU_V2,
};

enum arm_smmu_implementation {
	GENERIC_SMMU,
	ARM_MMU500,
	CAVIUM_SMMUV2,
	QCOM_SMMUV2,
	QCOM_SMMUV500,
};

struct arm_smmu_impl_def_reg {
	u32 offset;
	u32 value;
};

/* Until ACPICA headers cover IORT rev. C */
#ifndef ACPI_IORT_SMMU_CORELINK_MMU401
#define ACPI_IORT_SMMU_CORELINK_MMU401	0x4
#endif
#ifndef ACPI_IORT_SMMU_CAVIUM_THUNDERX
#define ACPI_IORT_SMMU_CAVIUM_THUNDERX	0x5
#endif

/*
 * attach_count
 *	The SMR and S2CR registers are only programmed when the number of
 *	devices attached to the iommu using these registers is > 0. This
 *	is required for the "SID switch" use case for secure display.
 *	Protected by stream_map_mutex.
 */
struct arm_smmu_s2cr {
	struct iommu_group		*group;
	int				count;
	int				attach_count;
	enum arm_smmu_s2cr_type		type;
	enum arm_smmu_s2cr_privcfg	privcfg;
	u8				cbndx;
	bool				cb_handoff;
};

#define s2cr_init_val (struct arm_smmu_s2cr){				\
	.type = disable_bypass ? S2CR_TYPE_FAULT : S2CR_TYPE_BYPASS,	\
	.cb_handoff = false,						\
}

struct arm_smmu_smr {
	u16				mask;
	u16				id;
	bool				valid;
};

struct arm_smmu_cb {
	u64				ttbr[2];
	u32				tcr[2];
	u32				mair[2];
	struct arm_smmu_cfg		*cfg;
	u32				actlr;
};

struct arm_smmu_master_cfg {
	struct arm_smmu_device		*smmu;
	s16				smendx[];
};
#define INVALID_SMENDX			-1
#define __fwspec_cfg(fw) ((struct arm_smmu_master_cfg *)fw->iommu_priv)
#define fwspec_smmu(fw)  (__fwspec_cfg(fw)->smmu)
#define fwspec_smendx(fw, i) \
	(i >= fw->num_ids ? INVALID_SMENDX : __fwspec_cfg(fw)->smendx[i])
#define for_each_cfg_sme(fw, i, idx) \
	for (i = 0; idx = fwspec_smendx(fw, i), i < fw->num_ids; ++i)

/*
 * Describes resources required for on/off power operation.
 * Separate reference count is provided for atomic/nonatomic
 * operations.
 */
struct arm_smmu_power_resources {
	struct platform_device		*pdev;
	struct device			*dev;

	struct clk			**clocks;
	int				num_clocks;

	struct regulator_bulk_data	*gdscs;
	int				num_gdscs;

	uint32_t			bus_client;
	struct msm_bus_scale_pdata	*bus_dt_data;

	/* Protects power_count */
	struct mutex			power_lock;
	int				power_count;

	/* Protects clock_refs_count */
	spinlock_t			clock_refs_lock;
	int				clock_refs_count;
	int				regulator_defer;
};

struct arm_smmu_arch_ops;
struct arm_smmu_device {
	struct device			*dev;

	void __iomem			*base;
	void __iomem			*cb_base;
	unsigned long			size;
	phys_addr_t			phys_addr;
	unsigned long			pgshift;

#define ARM_SMMU_FEAT_COHERENT_WALK	(1 << 0)
#define ARM_SMMU_FEAT_STREAM_MATCH	(1 << 1)
#define ARM_SMMU_FEAT_TRANS_S1		(1 << 2)
#define ARM_SMMU_FEAT_TRANS_S2		(1 << 3)
#define ARM_SMMU_FEAT_TRANS_NESTED	(1 << 4)
#define ARM_SMMU_FEAT_TRANS_OPS		(1 << 5)
#define ARM_SMMU_FEAT_VMID16		(1 << 6)
#define ARM_SMMU_FEAT_FMT_AARCH64_4K	(1 << 7)
#define ARM_SMMU_FEAT_FMT_AARCH64_16K	(1 << 8)
#define ARM_SMMU_FEAT_FMT_AARCH64_64K	(1 << 9)
#define ARM_SMMU_FEAT_FMT_AARCH32_L	(1 << 10)
#define ARM_SMMU_FEAT_FMT_AARCH32_S	(1 << 11)
#define ARM_SMMU_FEAT_EXIDS		(1 << 12)
	u32				features;

#define ARM_SMMU_OPT_SECURE_CFG_ACCESS (1 << 0)
#define ARM_SMMU_OPT_FATAL_ASF		(1 << 1)
#define ARM_SMMU_OPT_SKIP_INIT		(1 << 2)
#define ARM_SMMU_OPT_DYNAMIC		(1 << 3)
#define ARM_SMMU_OPT_3LVL_TABLES	(1 << 4)
#define ARM_SMMU_OPT_NO_ASID_RETENTION	(1 << 5)
#define ARM_SMMU_OPT_STATIC_CB		(1 << 6)
#define ARM_SMMU_OPT_DISABLE_ATOS	(1 << 7)
#define ARM_SMMU_OPT_MIN_IOVA_ALIGN	(1 << 8)
	u32				options;
	enum arm_smmu_arch_version	version;
	enum arm_smmu_implementation	model;

	u32				num_context_banks;
	u32				num_s2_context_banks;
	DECLARE_BITMAP(context_map, ARM_SMMU_MAX_CBS);
	struct arm_smmu_cb		*cbs;
	atomic_t			irptndx;

	u32				num_mapping_groups;
	u16				streamid_mask;
	u16				smr_mask_mask;
	struct arm_smmu_smr		*smrs;
	struct arm_smmu_s2cr		*s2crs;
	struct mutex			stream_map_mutex;
	struct mutex			iommu_group_mutex;
	unsigned long			va_size;
	unsigned long			ipa_size;
	unsigned long			pa_size;
	unsigned long			pgsize_bitmap;

	u32				num_global_irqs;
	u32				num_context_irqs;
	unsigned int			*irqs;

	struct list_head		list;

	u32				cavium_id_base; /* Specific to Cavium */

	spinlock_t			global_sync_lock;

	/* IOMMU core code handle */
	struct iommu_device		iommu;

	/* Specific to QCOM */
	struct arm_smmu_impl_def_reg	*impl_def_attach_registers;
	unsigned int			num_impl_def_attach_registers;

	struct arm_smmu_power_resources *pwr;

	spinlock_t			atos_lock;

	/* protects idr */
	struct mutex			idr_mutex;
	struct idr			asid_idr;

	struct arm_smmu_arch_ops	*arch_ops;
	void				*archdata;

	enum tz_smmu_device_id		sec_id;
};

enum arm_smmu_context_fmt {
	ARM_SMMU_CTX_FMT_NONE,
	ARM_SMMU_CTX_FMT_AARCH64,
	ARM_SMMU_CTX_FMT_AARCH32_L,
	ARM_SMMU_CTX_FMT_AARCH32_S,
};

struct arm_smmu_cfg {
	u8				cbndx;
	u8				irptndx;
	union {
		u16			asid;
		u16			vmid;
	};
	u32				cbar;
	u32				procid;
	enum arm_smmu_context_fmt	fmt;
};
#define INVALID_IRPTNDX			0xff
#define INVALID_CBNDX			0xff
#define INVALID_ASID			0xffff
/*
 * In V7L and V8L with TTBCR2.AS == 0, ASID is 8 bits.
 * V8L 16 with TTBCR2.AS == 1 (16 bit ASID) isn't supported yet.
 */
#define MAX_ASID			0xff

#define ARM_SMMU_CB_ASID(smmu, cfg)		((cfg)->asid)
#define ARM_SMMU_CB_VMID(smmu, cfg) ((u16)(smmu)->cavium_id_base + \
							(cfg)->cbndx + 1)

enum arm_smmu_domain_stage {
	ARM_SMMU_DOMAIN_S1 = 0,
	ARM_SMMU_DOMAIN_S2,
	ARM_SMMU_DOMAIN_NESTED,
	ARM_SMMU_DOMAIN_BYPASS,
};

struct arm_smmu_pte_info {
	void *virt_addr;
	size_t size;
	struct list_head entry;
};

struct arm_smmu_domain {
	struct arm_smmu_device		*smmu;
	struct device			*dev;
	struct io_pgtable_ops		*pgtbl_ops;

	struct arm_smmu_cfg		cfg;
	enum arm_smmu_domain_stage	stage;
	struct mutex			init_mutex; /* Protects smmu pointer */
	spinlock_t			cb_lock; /* Serialises ATS1* ops */
	spinlock_t			sync_lock; /* Serialises TLB syncs */
	struct io_pgtable_cfg		pgtbl_cfg;
	u32 attributes;
	bool				slave_side_secure;
	u32				secure_vmid;
	struct list_head		pte_info_list;
	struct list_head		unassign_list;
	struct mutex			assign_lock;
	struct list_head		secure_pool_list;
	/* nonsecure pool protected by pgtbl_lock */
	struct list_head		nonsecure_pool;
	struct iommu_domain		domain;
	bool				qsmmuv500_errata1_min_iova_align;
};

struct arm_smmu_option_prop {
	u32 opt;
	const char *prop;
};

static atomic_t cavium_smmu_context_count = ATOMIC_INIT(0);

static bool using_legacy_binding, using_generic_binding;

static struct arm_smmu_option_prop arm_smmu_options[] = {
	{ ARM_SMMU_OPT_SECURE_CFG_ACCESS, "calxeda,smmu-secure-config-access" },
	{ ARM_SMMU_OPT_FATAL_ASF, "qcom,fatal-asf" },
	{ ARM_SMMU_OPT_SKIP_INIT, "qcom,skip-init" },
	{ ARM_SMMU_OPT_DYNAMIC, "qcom,dynamic" },
	{ ARM_SMMU_OPT_3LVL_TABLES, "qcom,use-3-lvl-tables" },
	{ ARM_SMMU_OPT_NO_ASID_RETENTION, "qcom,no-asid-retention" },
	{ ARM_SMMU_OPT_STATIC_CB, "qcom,enable-static-cb"},
	{ ARM_SMMU_OPT_DISABLE_ATOS, "qcom,disable-atos" },
	{ ARM_SMMU_OPT_MIN_IOVA_ALIGN, "qcom,min-iova-align" },
	{ 0, NULL},
};

static phys_addr_t arm_smmu_iova_to_phys(struct iommu_domain *domain,
					dma_addr_t iova);
static phys_addr_t arm_smmu_iova_to_phys_hard(struct iommu_domain *domain,
					      dma_addr_t iova);
static void arm_smmu_destroy_domain_context(struct iommu_domain *domain);

static int arm_smmu_prepare_pgtable(void *addr, void *cookie);
static void arm_smmu_unprepare_pgtable(void *cookie, void *addr, size_t size);
static int arm_smmu_assign_table(struct arm_smmu_domain *smmu_domain);
static void arm_smmu_unassign_table(struct arm_smmu_domain *smmu_domain);

static uint64_t arm_smmu_iova_to_pte(struct iommu_domain *domain,
				    dma_addr_t iova);

static int arm_smmu_enable_s1_translations(struct arm_smmu_domain *smmu_domain);

static int arm_smmu_alloc_cb(struct iommu_domain *domain,
				struct arm_smmu_device *smmu,
				struct device *dev);

static bool arm_smmu_is_static_cb(struct arm_smmu_device *smmu);
static bool arm_smmu_is_master_side_secure(struct arm_smmu_domain *smmu_domain);
static bool arm_smmu_is_slave_side_secure(struct arm_smmu_domain *smmu_domain);

static int msm_secure_smmu_map(struct iommu_domain *domain, unsigned long iova,
			       phys_addr_t paddr, size_t size, int prot);
static size_t msm_secure_smmu_unmap(struct iommu_domain *domain,
				    unsigned long iova,
				    size_t size);
static size_t msm_secure_smmu_map_sg(struct iommu_domain *domain,
				     unsigned long iova,
				     struct scatterlist *sg,
				     unsigned int nents, int prot);

static struct arm_smmu_domain *to_smmu_domain(struct iommu_domain *dom)
{
	return container_of(dom, struct arm_smmu_domain, domain);
}

static void parse_driver_options(struct arm_smmu_device *smmu)
{
	int i = 0;

	do {
		if (of_property_read_bool(smmu->dev->of_node,
						arm_smmu_options[i].prop)) {
			smmu->options |= arm_smmu_options[i].opt;
			dev_dbg(smmu->dev, "option %s\n",
				arm_smmu_options[i].prop);
		}
	} while (arm_smmu_options[++i].opt);
}

static bool is_dynamic_domain(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);

	return !!(smmu_domain->attributes & (1 << DOMAIN_ATTR_DYNAMIC));
}

static int arm_smmu_restore_sec_cfg(struct arm_smmu_device *smmu, u32 cb)
{
	int ret;
	int scm_ret = 0;

	if (!arm_smmu_is_static_cb(smmu))
		return 0;

	ret = scm_restore_sec_cfg(smmu->sec_id, cb, &scm_ret);
	if (ret || scm_ret) {
		pr_err("scm call IOMMU_SECURE_CFG failed\n");
		return -EINVAL;
	}

	return 0;
}
static bool is_iommu_pt_coherent(struct arm_smmu_domain *smmu_domain)
{
	if (smmu_domain->attributes &
			(1 << DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT))
		return true;
	else if (smmu_domain->smmu && smmu_domain->smmu->dev)
		return smmu_domain->smmu->dev->archdata.dma_coherent;
	else
		return false;
}

static bool arm_smmu_is_static_cb(struct arm_smmu_device *smmu)
{
	return smmu->options & ARM_SMMU_OPT_STATIC_CB;
}

static bool arm_smmu_has_secure_vmid(struct arm_smmu_domain *smmu_domain)
{
	return (smmu_domain->secure_vmid != VMID_INVAL);
}

static bool arm_smmu_is_slave_side_secure(struct arm_smmu_domain *smmu_domain)
{
	return arm_smmu_has_secure_vmid(smmu_domain) &&
			smmu_domain->slave_side_secure;
}

static bool arm_smmu_is_master_side_secure(struct arm_smmu_domain *smmu_domain)
{
	return arm_smmu_has_secure_vmid(smmu_domain)
			&& !smmu_domain->slave_side_secure;
}

static void arm_smmu_secure_domain_lock(struct arm_smmu_domain *smmu_domain)
{
	if (arm_smmu_is_master_side_secure(smmu_domain))
		mutex_lock(&smmu_domain->assign_lock);
}

static void arm_smmu_secure_domain_unlock(struct arm_smmu_domain *smmu_domain)
{
	if (arm_smmu_is_master_side_secure(smmu_domain))
		mutex_unlock(&smmu_domain->assign_lock);
}

static bool arm_smmu_opt_hibernation(struct arm_smmu_device *smmu)
{
	return IS_ENABLED(CONFIG_HIBERNATION);
}

#ifdef CONFIG_ARM_SMMU_SELFTEST

static int selftest;
module_param_named(selftest, selftest, int, 0644);
static int irq_count;

static DECLARE_WAIT_QUEUE_HEAD(wait_int);
static irqreturn_t arm_smmu_cf_selftest(int irq, void *cb_base)
{
	u32 fsr;
	struct irq_data *irq_data = irq_get_irq_data(irq);
	unsigned long hwirq = ULONG_MAX;

	fsr = readl_relaxed(cb_base + ARM_SMMU_CB_FSR);

	irq_count++;
	if (irq_data)
		hwirq = irq_data->hwirq;
	pr_info("Interrupt (irq:%d hwirq:%ld) received, fsr:0x%x\n",
				irq, hwirq, fsr);

	writel_relaxed(fsr, cb_base + ARM_SMMU_CB_FSR);

	wake_up(&wait_int);
	return IRQ_HANDLED;
}

static void arm_smmu_interrupt_selftest(struct arm_smmu_device *smmu)
{
	int cb;
	int cb_count = 0;

	if (!selftest)
		return;

	if (arm_smmu_is_static_cb(smmu))
		return;

	cb = smmu->num_s2_context_banks;

	if (smmu->version < ARM_SMMU_V2)
		return;

	for_each_clear_bit_from(cb, smmu->context_map,
				smmu->num_context_banks) {
		int irq;
		int ret;
		void *cb_base;
		u32 reg;
		u32 reg_orig;
		int irq_cnt;

		irq = smmu->irqs[smmu->num_global_irqs + cb];
		cb_base = ARM_SMMU_CB(smmu, cb);

		ret = devm_request_threaded_irq(smmu->dev, irq, NULL,
				arm_smmu_cf_selftest,
				IRQF_ONESHOT | IRQF_SHARED,
				"arm-smmu-context-fault", cb_base);
		if (ret < 0) {
			dev_err(smmu->dev,
				"Failed to request cntx IRQ %d (%u)\n",
				cb, irq);
			continue;
		}

		cb_count++;
		irq_cnt = irq_count;

		reg_orig = readl_relaxed(cb_base + ARM_SMMU_CB_SCTLR);
		reg = reg_orig | SCTLR_CFIE | SCTLR_CFRE;

		writel_relaxed(reg, cb_base + ARM_SMMU_CB_SCTLR);
		dev_info(smmu->dev, "Testing cntx %d irq %d\n", cb, irq);

		/* Make sure ARM_SMMU_CB_SCTLR is configured */
		wmb();
		writel_relaxed(FSR_TF, cb_base + ARM_SMMU_CB_FSRRESTORE);

		wait_event_timeout(wait_int, (irq_count > irq_cnt),
			msecs_to_jiffies(1000));

		/* Make sure ARM_SMMU_CB_FSRRESTORE is written to */
		wmb();
		writel_relaxed(reg_orig, cb_base + ARM_SMMU_CB_SCTLR);
		devm_free_irq(smmu->dev, irq, cb_base);
	}

	dev_info(smmu->dev,
			"Interrupt selftest completed...\n");
	dev_info(smmu->dev,
			"Tested %d contexts, received %d interrupts\n",
			cb_count, irq_count);
	WARN_ON(cb_count != irq_count);
	irq_count = 0;
}
#else
static void arm_smmu_interrupt_selftest(struct arm_smmu_device *smmu)
{
}
#endif

/*
 * init()
 * Hook for additional device tree parsing at probe time.
 *
 * device_reset()
 * Hook for one-time architecture-specific register settings.
 *
 * iova_to_phys_hard()
 * Provides debug information. May be called from the context fault irq handler.
 *
 * init_context_bank()
 * Hook for architecture-specific settings which require knowledge of the
 * dynamically allocated context bank number.
 *
 * device_group()
 * Hook for checking whether a device is compatible with a said group.
 */
struct arm_smmu_arch_ops {
	int (*init)(struct arm_smmu_device *smmu);
	void (*device_reset)(struct arm_smmu_device *smmu);
	phys_addr_t (*iova_to_phys_hard)(struct iommu_domain *domain,
					 dma_addr_t iova);
	void (*init_context_bank)(struct arm_smmu_domain *smmu_domain,
					struct device *dev);
	int (*device_group)(struct device *dev, struct iommu_group *group);
};

static int arm_smmu_arch_init(struct arm_smmu_device *smmu)
{
	if (!smmu->arch_ops)
		return 0;
	if (!smmu->arch_ops->init)
		return 0;
	return smmu->arch_ops->init(smmu);
}

static void arm_smmu_arch_device_reset(struct arm_smmu_device *smmu)
{
	if (!smmu->arch_ops)
		return;
	if (!smmu->arch_ops->device_reset)
		return;
	return smmu->arch_ops->device_reset(smmu);
}

static void arm_smmu_arch_init_context_bank(
		struct arm_smmu_domain *smmu_domain, struct device *dev)
{
	struct arm_smmu_device *smmu = smmu_domain->smmu;

	if (!smmu->arch_ops)
		return;
	if (!smmu->arch_ops->init_context_bank)
		return;
	return smmu->arch_ops->init_context_bank(smmu_domain, dev);
}

static int arm_smmu_arch_device_group(struct device *dev,
					struct iommu_group *group)
{
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
	struct arm_smmu_device *smmu = fwspec_smmu(fwspec);

	if (!smmu->arch_ops)
		return 0;
	if (!smmu->arch_ops->device_group)
		return 0;
	return smmu->arch_ops->device_group(dev, group);
}

static void arm_smmu_arch_write_sync(struct arm_smmu_device *smmu)
{
	u32 id;

	if (!smmu)
		return;

	/* Read to complete prior write transcations */
	id = readl_relaxed(ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_ID0);

	/* Wait for read to complete before off */
	rmb();
}

static struct device_node *dev_get_dev_node(struct device *dev)
{
	if (dev_is_pci(dev)) {
		struct pci_bus *bus = to_pci_dev(dev)->bus;

		while (!pci_is_root_bus(bus))
			bus = bus->parent;
		return of_node_get(bus->bridge->parent->of_node);
	}

	return of_node_get(dev->of_node);
}

static int __arm_smmu_get_pci_sid(struct pci_dev *pdev, u16 alias, void *data)
{
	*((__be32 *)data) = cpu_to_be32(alias);
	return 0; /* Continue walking */
}

static int __find_legacy_master_phandle(struct device *dev, void *data)
{
	struct of_phandle_iterator *it = *(void **)data;
	struct device_node *np = it->node;
	int err;

	of_for_each_phandle(it, err, dev->of_node, "mmu-masters",
			    "#stream-id-cells", 0)
		if (it->node == np) {
			*(void **)data = dev;
			return 1;
		}
	it->node = np;
	return err == -ENOENT ? 0 : err;
}

static struct platform_driver arm_smmu_driver;
static struct iommu_ops arm_smmu_ops;

static int arm_smmu_register_legacy_master(struct device *dev,
					   struct arm_smmu_device **smmu)
{
	struct device *smmu_dev;
	struct device_node *np;
	struct of_phandle_iterator it;
	void *data = &it;
	u32 *sids;
	__be32 pci_sid;
	int err = 0;

	memset(&it, 0, sizeof(it));
	np = dev_get_dev_node(dev);
	if (!np || !of_find_property(np, "#stream-id-cells", NULL)) {
		of_node_put(np);
		return -ENODEV;
	}

	it.node = np;
	err = driver_for_each_device(&arm_smmu_driver.driver, NULL, &data,
				     __find_legacy_master_phandle);
	smmu_dev = data;
	of_node_put(np);
	if (err == 0)
		return -ENODEV;
	if (err < 0)
		return err;

	if (dev_is_pci(dev)) {
		/* "mmu-masters" assumes Stream ID == Requester ID */
		pci_for_each_dma_alias(to_pci_dev(dev), __arm_smmu_get_pci_sid,
				       &pci_sid);
		it.cur = &pci_sid;
		it.cur_count = 1;
	}

	err = iommu_fwspec_init(dev, &smmu_dev->of_node->fwnode,
				&arm_smmu_ops);
	if (err)
		return err;

	sids = kcalloc(it.cur_count, sizeof(*sids), GFP_KERNEL);
	if (!sids)
		return -ENOMEM;

	*smmu = dev_get_drvdata(smmu_dev);
	of_phandle_iterator_args(&it, sids, it.cur_count);
	err = iommu_fwspec_add_ids(dev, sids, it.cur_count);
	kfree(sids);
	return err;
}

static int __arm_smmu_alloc_bitmap(unsigned long *map, int start, int end)
{
	int idx;

	do {
		idx = find_next_zero_bit(map, end, start);
		if (idx == end)
			return -ENOSPC;
	} while (test_and_set_bit(idx, map));

	return idx;
}

static void __arm_smmu_free_bitmap(unsigned long *map, int idx)
{
	clear_bit(idx, map);
}

static int arm_smmu_prepare_clocks(struct arm_smmu_power_resources *pwr)
{
	int i, ret = 0;

	for (i = 0; i < pwr->num_clocks; ++i) {
		ret = clk_prepare(pwr->clocks[i]);
		if (ret) {
			dev_err(pwr->dev, "Couldn't prepare clock #%d\n", i);
			while (i--)
				clk_unprepare(pwr->clocks[i]);
			break;
		}
	}
	return ret;
}

static void arm_smmu_unprepare_clocks(struct arm_smmu_power_resources *pwr)
{
	int i;

	for (i = pwr->num_clocks; i; --i)
		clk_unprepare(pwr->clocks[i - 1]);
}

static int arm_smmu_enable_clocks(struct arm_smmu_power_resources *pwr)
{
	int i, ret = 0;

	for (i = 0; i < pwr->num_clocks; ++i) {
		ret = clk_enable(pwr->clocks[i]);
		if (ret) {
			dev_err(pwr->dev, "Couldn't enable clock #%d\n", i);
			while (i--)
				clk_disable(pwr->clocks[i]);
			break;
		}
	}

	return ret;
}

static void arm_smmu_disable_clocks(struct arm_smmu_power_resources *pwr)
{
	int i;

	for (i = pwr->num_clocks; i; --i)
		clk_disable(pwr->clocks[i - 1]);
}

static int arm_smmu_request_bus(struct arm_smmu_power_resources *pwr)
{
	if (!pwr->bus_client)
		return 0;
	return msm_bus_scale_client_update_request(pwr->bus_client, 1);
}

static void arm_smmu_unrequest_bus(struct arm_smmu_power_resources *pwr)
{
	if (!pwr->bus_client)
		return;
	WARN_ON(msm_bus_scale_client_update_request(pwr->bus_client, 0));
}

static int arm_smmu_enable_regulators(struct arm_smmu_power_resources *pwr)
{
	struct regulator_bulk_data *consumers;
	int num_consumers, ret;
	int i;

	num_consumers = pwr->num_gdscs;
	consumers = pwr->gdscs;
	for (i = 0; i < num_consumers; i++) {
		ret = regulator_enable(consumers[i].consumer);
		if (ret)
			goto out;
	}
	return 0;

out:
	i -= 1;
	for (; i >= 0; i--)
		regulator_disable(consumers[i].consumer);
	return ret;
}

static int arm_smmu_disable_regulators(struct arm_smmu_power_resources *pwr)
{
	struct regulator_bulk_data *consumers;
	int i;
	int num_consumers, ret, r;

	num_consumers = pwr->num_gdscs;
	consumers = pwr->gdscs;
	for (i = num_consumers - 1; i >= 0; --i) {
		ret = regulator_disable_deferred(consumers[i].consumer,
						 pwr->regulator_defer);
		if (ret != 0)
			goto err;
	}

	return 0;

err:
	pr_err("Failed to disable %s: %d\n", consumers[i].supply, ret);
	for (++i; i < num_consumers; ++i) {
		r = regulator_enable(consumers[i].consumer);
		if (r != 0)
			pr_err("Failed to rename %s: %d\n",
			       consumers[i].supply, r);
	}

	return ret;
}

/* Clocks must be prepared before this (arm_smmu_prepare_clocks) */
static int arm_smmu_power_on_atomic(struct arm_smmu_power_resources *pwr)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&pwr->clock_refs_lock, flags);
	if (pwr->clock_refs_count > 0) {
		pwr->clock_refs_count++;
		spin_unlock_irqrestore(&pwr->clock_refs_lock, flags);
		return 0;
	}

	ret = arm_smmu_enable_clocks(pwr);
	if (!ret)
		pwr->clock_refs_count = 1;

	spin_unlock_irqrestore(&pwr->clock_refs_lock, flags);
	return ret;
}

/* Clocks should be unprepared after this (arm_smmu_unprepare_clocks) */
static void arm_smmu_power_off_atomic(struct arm_smmu_power_resources *pwr)
{
	unsigned long flags;
	struct arm_smmu_device *smmu = pwr->dev->driver_data;

	arm_smmu_arch_write_sync(smmu);

	spin_lock_irqsave(&pwr->clock_refs_lock, flags);
	if (pwr->clock_refs_count == 0) {
		WARN(1, "%s: bad clock_ref_count\n", dev_name(pwr->dev));
		spin_unlock_irqrestore(&pwr->clock_refs_lock, flags);
		return;

	} else if (pwr->clock_refs_count > 1) {
		pwr->clock_refs_count--;
		spin_unlock_irqrestore(&pwr->clock_refs_lock, flags);
		return;
	}

	arm_smmu_disable_clocks(pwr);

	pwr->clock_refs_count = 0;
	spin_unlock_irqrestore(&pwr->clock_refs_lock, flags);
}

static int arm_smmu_power_on_slow(struct arm_smmu_power_resources *pwr)
{
	int ret;

	mutex_lock(&pwr->power_lock);
	if (pwr->power_count > 0) {
		pwr->power_count += 1;
		mutex_unlock(&pwr->power_lock);
		return 0;
	}

	ret = arm_smmu_request_bus(pwr);
	if (ret)
		goto out_unlock;

	ret = arm_smmu_enable_regulators(pwr);
	if (ret)
		goto out_disable_bus;

	ret = arm_smmu_prepare_clocks(pwr);
	if (ret)
		goto out_disable_regulators;

	pwr->power_count = 1;
	mutex_unlock(&pwr->power_lock);
	return 0;

out_disable_regulators:
	regulator_bulk_disable(pwr->num_gdscs, pwr->gdscs);
out_disable_bus:
	arm_smmu_unrequest_bus(pwr);
out_unlock:
	mutex_unlock(&pwr->power_lock);
	return ret;
}

static void arm_smmu_power_off_slow(struct arm_smmu_power_resources *pwr)
{
	mutex_lock(&pwr->power_lock);
	if (pwr->power_count == 0) {
		WARN(1, "%s: Bad power count\n", dev_name(pwr->dev));
		mutex_unlock(&pwr->power_lock);
		return;

	} else if (pwr->power_count > 1) {
		pwr->power_count--;
		mutex_unlock(&pwr->power_lock);
		return;
	}

	arm_smmu_unprepare_clocks(pwr);
	arm_smmu_disable_regulators(pwr);
	arm_smmu_unrequest_bus(pwr);
	pwr->power_count = 0;
	mutex_unlock(&pwr->power_lock);
}

static int arm_smmu_power_on(struct arm_smmu_power_resources *pwr)
{
	int ret;

	ret = arm_smmu_power_on_slow(pwr);
	if (ret)
		return ret;

	ret = arm_smmu_power_on_atomic(pwr);
	if (ret)
		goto out_disable;

	return 0;

out_disable:
	arm_smmu_power_off_slow(pwr);
	return ret;
}

static void arm_smmu_power_off(struct arm_smmu_power_resources *pwr)
{
	arm_smmu_power_off_atomic(pwr);
	arm_smmu_power_off_slow(pwr);
}

/*
 * Must be used instead of arm_smmu_power_on if it may be called from
 * atomic context
 */
static int arm_smmu_domain_power_on(struct iommu_domain *domain,
				struct arm_smmu_device *smmu)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	int atomic_domain = smmu_domain->attributes & (1 << DOMAIN_ATTR_ATOMIC);

	if (atomic_domain)
		return arm_smmu_power_on_atomic(smmu->pwr);

	return arm_smmu_power_on(smmu->pwr);
}

/*
 * Must be used instead of arm_smmu_power_on if it may be called from
 * atomic context
 */
static void arm_smmu_domain_power_off(struct iommu_domain *domain,
				struct arm_smmu_device *smmu)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	int atomic_domain = smmu_domain->attributes & (1 << DOMAIN_ATTR_ATOMIC);

	if (atomic_domain) {
		arm_smmu_power_off_atomic(smmu->pwr);
		return;
	}

	arm_smmu_power_off(smmu->pwr);
}

/* Wait for any pending TLB invalidations to complete */
static int __arm_smmu_tlb_sync(struct arm_smmu_device *smmu,
				void __iomem *sync, void __iomem *status)
{
	unsigned int spin_cnt, delay;
	u32 sync_inv_ack, tbu_pwr_status;

	writel_relaxed(0, sync);
	for (delay = 1; delay < TLB_LOOP_TIMEOUT; delay *= 2) {
		for (spin_cnt = TLB_SPIN_COUNT; spin_cnt > 0; spin_cnt--) {
			if (!(readl_relaxed(status) & sTLBGSTATUS_GSACTIVE))
				return 0;
			cpu_relax();
		}
		udelay(delay);
	}
	sync_inv_ack = scm_io_read((unsigned long)(smmu->phys_addr +
				     ARM_SMMU_STATS_SYNC_INV_TBU_ACK));
	tbu_pwr_status = scm_io_read((unsigned long)(smmu->phys_addr +
				     ARM_SMMU_TBU_PWR_STATUS));
	trace_tlbsync_timeout(smmu->dev, 0);
	dev_err_ratelimited(smmu->dev,
			    "TLB sync timed out -- SMMU may be deadlocked ack 0x%x pwr 0x%x\n",
			    sync_inv_ack, tbu_pwr_status);
	return -EINVAL;
}

static void arm_smmu_tlb_sync_global(struct arm_smmu_device *smmu)
{
	void __iomem *base = ARM_SMMU_GR0(smmu);
	unsigned long flags;

	spin_lock_irqsave(&smmu->global_sync_lock, flags);
	if (__arm_smmu_tlb_sync(smmu, base + ARM_SMMU_GR0_sTLBGSYNC,
					base + ARM_SMMU_GR0_sTLBGSTATUS))
		dev_err_ratelimited(smmu->dev,
				    "TLB global sync failed!\n");
	spin_unlock_irqrestore(&smmu->global_sync_lock, flags);
}

static void arm_smmu_tlb_sync_context(void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	void __iomem *base = ARM_SMMU_CB(smmu, smmu_domain->cfg.cbndx);
	unsigned long flags;

	spin_lock_irqsave(&smmu_domain->sync_lock, flags);
	if (__arm_smmu_tlb_sync(smmu, base + ARM_SMMU_CB_TLBSYNC,
					base + ARM_SMMU_CB_TLBSTATUS))
		dev_err_ratelimited(smmu->dev,
				"TLB sync on cb%d failed for device %s\n",
				smmu_domain->cfg.cbndx,
				dev_name(smmu_domain->dev));
	spin_unlock_irqrestore(&smmu_domain->sync_lock, flags);
}

static void arm_smmu_tlb_sync_vmid(void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;

	arm_smmu_tlb_sync_global(smmu_domain->smmu);
}

static void arm_smmu_tlb_inv_context_s1(void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct device *dev = smmu_domain->dev;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	void __iomem *base = ARM_SMMU_CB(smmu_domain->smmu, cfg->cbndx);
	bool use_tlbiall = smmu->options & ARM_SMMU_OPT_NO_ASID_RETENTION;
	ktime_t cur = ktime_get();

	trace_tlbi_start(dev, 0);

	if (!use_tlbiall)
		writel_relaxed(cfg->asid, base + ARM_SMMU_CB_S1_TLBIASID);
	else
		writel_relaxed(0, base + ARM_SMMU_CB_S1_TLBIALL);

	arm_smmu_tlb_sync_context(cookie);
	trace_tlbi_end(dev, ktime_us_delta(ktime_get(), cur));
}

static void arm_smmu_tlb_inv_context_s2(void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	void __iomem *base = ARM_SMMU_GR0(smmu);

	writel_relaxed(smmu_domain->cfg.vmid, base + ARM_SMMU_GR0_TLBIVMID);
	arm_smmu_tlb_sync_global(smmu);
}

static void arm_smmu_tlb_inv_range_nosync(unsigned long iova, size_t size,
					  size_t granule, bool leaf, void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	bool stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;
	void __iomem *reg = ARM_SMMU_CB(smmu_domain->smmu, cfg->cbndx);
	bool use_tlbiall = smmu->options & ARM_SMMU_OPT_NO_ASID_RETENTION;

	if (smmu_domain->smmu->features & ARM_SMMU_FEAT_COHERENT_WALK)
		wmb();

	if (stage1 && !use_tlbiall) {
		reg += leaf ? ARM_SMMU_CB_S1_TLBIVAL : ARM_SMMU_CB_S1_TLBIVA;

		if (cfg->fmt != ARM_SMMU_CTX_FMT_AARCH64) {
			iova &= ~12UL;
			iova |= cfg->asid;
			do {
				writel_relaxed(iova, reg);
				iova += granule;
			} while (size -= granule);
		} else {
			iova >>= 12;
			iova |= (u64)cfg->asid << 48;
			do {
				writeq_relaxed(iova, reg);
				iova += granule >> 12;
			} while (size -= granule);
		}
	} else if (stage1 && use_tlbiall) {
		reg += ARM_SMMU_CB_S1_TLBIALL;
		writel_relaxed(0, reg);
	} else {
		reg += leaf ? ARM_SMMU_CB_S2_TLBIIPAS2L :
			      ARM_SMMU_CB_S2_TLBIIPAS2;
		iova >>= 12;
		do {
			smmu_write_atomic_lq(iova, reg);
			iova += granule >> 12;
		} while (size -= granule);
	}
}

/*
 * On MMU-401 at least, the cost of firing off multiple TLBIVMIDs appears
 * almost negligible, but the benefit of getting the first one in as far ahead
 * of the sync as possible is significant, hence we don't just make this a
 * no-op and set .tlb_sync to arm_smmu_inv_context_s2() as you might think.
 */
static void arm_smmu_tlb_inv_vmid_nosync(unsigned long iova, size_t size,
					 size_t granule, bool leaf, void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	void __iomem *base = ARM_SMMU_GR0(smmu_domain->smmu);

	if (smmu_domain->smmu->features & ARM_SMMU_FEAT_COHERENT_WALK)
		wmb();

	writel_relaxed(smmu_domain->cfg.vmid, base + ARM_SMMU_GR0_TLBIVMID);
}

struct arm_smmu_secure_pool_chunk {
	void *addr;
	size_t size;
	struct list_head list;
};

static void *arm_smmu_secure_pool_remove(struct arm_smmu_domain *smmu_domain,
					size_t size)
{
	struct arm_smmu_secure_pool_chunk *it;

	list_for_each_entry(it, &smmu_domain->secure_pool_list, list) {
		if (it->size == size) {
			void *addr = it->addr;

			list_del(&it->list);
			kfree(it);
			return addr;
		}
	}

	return NULL;
}

static int arm_smmu_secure_pool_add(struct arm_smmu_domain *smmu_domain,
				     void *addr, size_t size)
{
	struct arm_smmu_secure_pool_chunk *chunk;

	chunk = kmalloc(sizeof(*chunk), GFP_ATOMIC);
	if (!chunk)
		return -ENOMEM;

	chunk->addr = addr;
	chunk->size = size;
	memset(addr, 0, size);
	list_add(&chunk->list, &smmu_domain->secure_pool_list);

	return 0;
}

static void arm_smmu_secure_pool_destroy(struct arm_smmu_domain *smmu_domain)
{
	struct arm_smmu_secure_pool_chunk *it, *i;

	list_for_each_entry_safe(it, i, &smmu_domain->secure_pool_list, list) {
		arm_smmu_unprepare_pgtable(smmu_domain, it->addr, it->size);
		/* pages will be freed later (after being unassigned) */
		list_del(&it->list);
		kfree(it);
	}
}

static void *arm_smmu_alloc_pages_exact(void *cookie,
					size_t size, gfp_t gfp_mask)
{
	int ret;
	void *page;
	struct arm_smmu_domain *smmu_domain = cookie;

	if (!arm_smmu_is_master_side_secure(smmu_domain)) {
		struct page *pg;
		/* size is expected to be 4K with current configuration */
		if (size == PAGE_SIZE) {
			pg = list_first_entry_or_null(
				&smmu_domain->nonsecure_pool, struct page, lru);
			if (pg) {
				list_del_init(&pg->lru);
				return page_address(pg);
			}
		}
		return alloc_pages_exact(size, gfp_mask);
	}

	page = arm_smmu_secure_pool_remove(smmu_domain, size);
	if (page)
		return page;

	page = alloc_pages_exact(size, gfp_mask);
	if (page) {
		ret = arm_smmu_prepare_pgtable(page, cookie);
		if (ret) {
			free_pages_exact(page, size);
			return NULL;
		}
	}

	return page;
}

static void arm_smmu_free_pages_exact(void *cookie, void *virt, size_t size)
{
	struct arm_smmu_domain *smmu_domain = cookie;

	if (!arm_smmu_is_master_side_secure(smmu_domain)) {
		free_pages_exact(virt, size);
		return;
	}

	if (arm_smmu_secure_pool_add(smmu_domain, virt, size))
		arm_smmu_unprepare_pgtable(smmu_domain, virt, size);
}

static const struct iommu_gather_ops arm_smmu_s1_tlb_ops = {
	.tlb_flush_all	= arm_smmu_tlb_inv_context_s1,
	.tlb_add_flush	= arm_smmu_tlb_inv_range_nosync,
	.tlb_sync	= arm_smmu_tlb_sync_context,
	.alloc_pages_exact = arm_smmu_alloc_pages_exact,
	.free_pages_exact = arm_smmu_free_pages_exact,
};

static const struct iommu_gather_ops arm_smmu_s2_tlb_ops_v2 = {
	.tlb_flush_all	= arm_smmu_tlb_inv_context_s2,
	.tlb_add_flush	= arm_smmu_tlb_inv_range_nosync,
	.tlb_sync	= arm_smmu_tlb_sync_context,
	.alloc_pages_exact = arm_smmu_alloc_pages_exact,
	.free_pages_exact = arm_smmu_free_pages_exact,
};

static const struct iommu_gather_ops arm_smmu_s2_tlb_ops_v1 = {
	.tlb_flush_all	= arm_smmu_tlb_inv_context_s2,
	.tlb_add_flush	= arm_smmu_tlb_inv_vmid_nosync,
	.tlb_sync	= arm_smmu_tlb_sync_vmid,
	.alloc_pages_exact = arm_smmu_alloc_pages_exact,
	.free_pages_exact = arm_smmu_free_pages_exact,
};

static void msm_smmu_tlb_inv_context(void *cookie)
{
}

static void msm_smmu_tlb_inv_range_nosync(unsigned long iova, size_t size,
					  size_t granule, bool leaf,
					  void *cookie)
{
}

static void msm_smmu_tlb_sync(void *cookie)
{
}

static struct iommu_gather_ops msm_smmu_gather_ops = {
	.tlb_flush_all	= msm_smmu_tlb_inv_context,
	.tlb_add_flush	= msm_smmu_tlb_inv_range_nosync,
	.tlb_sync	= msm_smmu_tlb_sync,
	.alloc_pages_exact = arm_smmu_alloc_pages_exact,
	.free_pages_exact = arm_smmu_free_pages_exact,
};

static void print_ctx_regs(struct arm_smmu_device *smmu, struct arm_smmu_cfg
			   *cfg, unsigned int fsr)
{
	u32 fsynr0;
	void __iomem *cb_base = ARM_SMMU_CB(smmu, cfg->cbndx);
	void __iomem *gr1_base = ARM_SMMU_GR1(smmu);
	bool stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;

	fsynr0 = readl_relaxed(cb_base + ARM_SMMU_CB_FSYNR0);

	dev_err(smmu->dev, "FAR    = 0x%016llx\n",
		readq_relaxed(cb_base + ARM_SMMU_CB_FAR));
	dev_err(smmu->dev, "PAR    = 0x%pK\n",
		readq_relaxed(cb_base + ARM_SMMU_CB_PAR));

	dev_err(smmu->dev,
		"FSR    = 0x%08x [%s%s%s%s%s%s%s%s%s%s]\n",
		fsr,
		(fsr & 0x02) ?  (fsynr0 & 0x10 ?
				 "TF W " : "TF R ") : "",
		(fsr & 0x04) ? "AFF " : "",
		(fsr & 0x08) ? (fsynr0 & 0x10 ?
				"PF W " : "PF R ") : "",
		(fsr & 0x10) ? "EF " : "",
		(fsr & 0x20) ? "TLBMCF " : "",
		(fsr & 0x40) ? "TLBLKF " : "",
		(fsr & 0x80) ? "MHF " : "",
		(fsr & 0x100) ? "UUT " : "",
		(fsr & 0x40000000) ? "SS " : "",
		(fsr & 0x80000000) ? "MULTI " : "");

	if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
		dev_err(smmu->dev, "TTBR0  = 0x%pK\n",
			readl_relaxed(cb_base + ARM_SMMU_CB_TTBR0));
		dev_err(smmu->dev, "TTBR1  = 0x%pK\n",
			readl_relaxed(cb_base + ARM_SMMU_CB_TTBR1));
	} else {
		dev_err(smmu->dev, "TTBR0  = 0x%pK\n",
			readq_relaxed(cb_base + ARM_SMMU_CB_TTBR0));
		if (stage1)
			dev_err(smmu->dev, "TTBR1  = 0x%pK\n",
				readq_relaxed(cb_base + ARM_SMMU_CB_TTBR1));
	}


	dev_err(smmu->dev, "SCTLR  = 0x%08x ACTLR  = 0x%08x\n",
	       readl_relaxed(cb_base + ARM_SMMU_CB_SCTLR),
	       readl_relaxed(cb_base + ARM_SMMU_CB_ACTLR));
	dev_err(smmu->dev, "CBAR  = 0x%08x\n",
	       readl_relaxed(gr1_base + ARM_SMMU_GR1_CBAR(cfg->cbndx)));
	dev_err(smmu->dev, "MAIR0   = 0x%08x MAIR1   = 0x%08x\n",
	       readl_relaxed(cb_base + ARM_SMMU_CB_S1_MAIR0),
	       readl_relaxed(cb_base + ARM_SMMU_CB_S1_MAIR1));

}

static phys_addr_t arm_smmu_verify_fault(struct iommu_domain *domain,
					 dma_addr_t iova, u32 fsr)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	phys_addr_t phys;
	phys_addr_t phys_post_tlbiall;

	phys = arm_smmu_iova_to_phys_hard(domain, iova);
	smmu_domain->pgtbl_cfg.tlb->tlb_flush_all(smmu_domain);
	phys_post_tlbiall = arm_smmu_iova_to_phys_hard(domain, iova);

	if (phys != phys_post_tlbiall) {
		dev_err(smmu->dev,
			"ATOS results differed across TLBIALL...\n"
			"Before: %pa After: %pa\n", &phys, &phys_post_tlbiall);
	}

	return (phys == 0 ? phys_post_tlbiall : phys);
}

static irqreturn_t arm_smmu_context_fault(int irq, void *dev)
{
	int flags, ret, tmp;
	u32 fsr, fsynr0, fsynr1, frsynra, resume;
	unsigned long iova;
	struct iommu_domain *domain = dev;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	void __iomem *cb_base;
	void __iomem *gr1_base;
	bool fatal_asf = smmu->options & ARM_SMMU_OPT_FATAL_ASF;
	phys_addr_t phys_soft;
	uint64_t pte;
	bool non_fatal_fault = !!(smmu_domain->attributes &
					(1 << DOMAIN_ATTR_NON_FATAL_FAULTS));

	static DEFINE_RATELIMIT_STATE(_rs,
				      DEFAULT_RATELIMIT_INTERVAL,
				      DEFAULT_RATELIMIT_BURST);

	ret = arm_smmu_power_on(smmu->pwr);
	if (ret)
		return IRQ_NONE;

	gr1_base = ARM_SMMU_GR1(smmu);
	cb_base = ARM_SMMU_CB(smmu, cfg->cbndx);
	fsr = readl_relaxed(cb_base + ARM_SMMU_CB_FSR);

	if (!(fsr & FSR_FAULT)) {
		ret = IRQ_NONE;
		goto out_power_off;
	}

	if (fatal_asf && (fsr & FSR_ASF)) {
		dev_err(smmu->dev,
			"Took an address size fault.  Refusing to recover.\n");
		BUG();
	}

	fsynr0 = readl_relaxed(cb_base + ARM_SMMU_CB_FSYNR0);
	fsynr1 = readl_relaxed(cb_base + ARM_SMMU_CB_FSYNR1);
	flags = fsynr0 & FSYNR0_WNR ? IOMMU_FAULT_WRITE : IOMMU_FAULT_READ;
	if (fsr & FSR_TF)
		flags |= IOMMU_FAULT_TRANSLATION;
	if (fsr & FSR_PF)
		flags |= IOMMU_FAULT_PERMISSION;
	if (fsr & FSR_EF)
		flags |= IOMMU_FAULT_EXTERNAL;
	if (fsr & FSR_SS)
		flags |= IOMMU_FAULT_TRANSACTION_STALLED;

	iova = readq_relaxed(cb_base + ARM_SMMU_CB_FAR);
	phys_soft = arm_smmu_iova_to_phys(domain, iova);
	frsynra = readl_relaxed(gr1_base + ARM_SMMU_GR1_CBFRSYNRA(cfg->cbndx));
	frsynra &= CBFRSYNRA_SID_MASK;
	tmp = report_iommu_fault(domain, smmu->dev, iova, flags);
	if (!tmp || (tmp == -EBUSY)) {
		dev_dbg(smmu->dev,
			"Context fault handled by client: iova=0x%08lx, cb=%d, fsr=0x%x, fsynr0=0x%x, fsynr1=0x%x\n",
			iova, cfg->cbndx, fsr, fsynr0, fsynr1);
		dev_dbg(smmu->dev,
			"soft iova-to-phys=%pa\n", &phys_soft);
		ret = IRQ_HANDLED;
		resume = RESUME_TERMINATE;
	} else {
		if (__ratelimit(&_rs)) {
			phys_addr_t phys_atos;

			print_ctx_regs(smmu, cfg, fsr);
			phys_atos = arm_smmu_verify_fault(domain, iova, fsr);
			dev_err(smmu->dev,
				"Unhandled context fault: iova=0x%08lx, cb=%d, fsr=0x%x, fsynr0=0x%x, fsynr1=0x%x\n",
				iova, cfg->cbndx, fsr, fsynr0, fsynr1);


			dev_err(smmu->dev,
				"soft iova-to-phys=%pa\n", &phys_soft);
			if (!phys_soft)
				dev_err(smmu->dev,
					"SOFTWARE TABLE WALK FAILED! Looks like %s accessed an unmapped address!\n",
					dev_name(smmu->dev));
			else {
				pte = arm_smmu_iova_to_pte(domain, iova);
				dev_err(smmu->dev, "PTE = %016llx\n", pte);
			}
			if (phys_atos)
				dev_err(smmu->dev, "hard iova-to-phys (ATOS)=%pa\n",
					&phys_atos);
			else
				dev_err(smmu->dev, "hard iova-to-phys (ATOS) failed\n");
			dev_err(smmu->dev, "SID=0x%x\n", frsynra);
		}
		ret = IRQ_NONE;
		resume = RESUME_TERMINATE;
		if (!non_fatal_fault) {
			dev_err(smmu->dev,
				"Unhandled arm-smmu context fault!\n");
			BUG();
		}
	}

	/*
	 * If the client returns -EBUSY, do not clear FSR and do not RESUME
	 * if stalled. This is required to keep the IOMMU client stalled on
	 * the outstanding fault. This gives the client a chance to take any
	 * debug action and then terminate the stalled transaction.
	 * So, the sequence in case of stall on fault should be:
	 * 1) Do not clear FSR or write to RESUME here
	 * 2) Client takes any debug action
	 * 3) Client terminates the stalled transaction and resumes the IOMMU
	 * 4) Client clears FSR. The FSR should only be cleared after 3) and
	 *    not before so that the fault remains outstanding. This ensures
	 *    SCTLR.HUPCF has the desired effect if subsequent transactions also
	 *    need to be terminated.
	 */
	if (tmp != -EBUSY) {
		/* Clear the faulting FSR */
		writel_relaxed(fsr, cb_base + ARM_SMMU_CB_FSR);

		/*
		 * Barrier required to ensure that the FSR is cleared
		 * before resuming SMMU operation
		 */
		wmb();

		/* Retry or terminate any stalled transactions */
		if (fsr & FSR_SS)
			writel_relaxed(resume, cb_base + ARM_SMMU_CB_RESUME);
	}

out_power_off:
	arm_smmu_power_off(smmu->pwr);

	return ret;
}

static irqreturn_t arm_smmu_global_fault(int irq, void *dev)
{
	u32 gfsr, gfsynr0, gfsynr1, gfsynr2;
	struct arm_smmu_device *smmu = dev;
	void __iomem *gr0_base = ARM_SMMU_GR0_NS(smmu);

	if (arm_smmu_power_on(smmu->pwr))
		return IRQ_NONE;

	gfsr = readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSR);
	gfsynr0 = readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSYNR0);
	gfsynr1 = readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSYNR1);
	gfsynr2 = readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSYNR2);

	if (!gfsr) {
		arm_smmu_power_off(smmu->pwr);
		return IRQ_NONE;
	}

	dev_err_ratelimited(smmu->dev,
		"Unexpected global fault, this could be serious\n");
	dev_err_ratelimited(smmu->dev,
		"\tGFSR 0x%08x, GFSYNR0 0x%08x, GFSYNR1 0x%08x, GFSYNR2 0x%08x\n",
		gfsr, gfsynr0, gfsynr1, gfsynr2);

	writel(gfsr, gr0_base + ARM_SMMU_GR0_sGFSR);
	arm_smmu_power_off(smmu->pwr);
	return IRQ_HANDLED;
}

static bool arm_smmu_master_attached(struct arm_smmu_device *smmu,
				     struct iommu_fwspec *fwspec)
{
	int i, idx;

	for_each_cfg_sme(fwspec, i, idx) {
		if (smmu->s2crs[idx].attach_count)
			return true;
	}

	return false;
}

static int arm_smmu_set_pt_format(struct arm_smmu_domain *smmu_domain,
				  struct io_pgtable_cfg *pgtbl_cfg)
{
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	int ret = 0;

	if ((smmu->version > ARM_SMMU_V1) &&
	    (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64) &&
	    !arm_smmu_has_secure_vmid(smmu_domain) &&
	    arm_smmu_is_static_cb(smmu)) {
		ret = msm_tz_set_cb_format(smmu->sec_id, cfg->cbndx);
	}
	return ret;
}

static void arm_smmu_init_context_bank(struct arm_smmu_domain *smmu_domain,
				       struct io_pgtable_cfg *pgtbl_cfg)
{
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_cb *cb = &smmu_domain->smmu->cbs[cfg->cbndx];
	bool stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;

	cb->cfg = cfg;

	/* TTBCR */
	if (stage1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
			cb->tcr[0] = pgtbl_cfg->arm_v7s_cfg.tcr;
		} else {
			cb->tcr[0] = pgtbl_cfg->arm_lpae_s1_cfg.tcr;
			cb->tcr[1] = pgtbl_cfg->arm_lpae_s1_cfg.tcr >> 32;
			cb->tcr[1] |= TTBCR2_SEP_UPSTREAM;
			if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64)
				cb->tcr[1] |= TTBCR2_AS;
		}
	} else {
		cb->tcr[0] = pgtbl_cfg->arm_lpae_s2_cfg.vtcr;
	}

	/* TTBRs */
	if (stage1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
			cb->ttbr[0] = pgtbl_cfg->arm_v7s_cfg.ttbr[0];
			cb->ttbr[1] = pgtbl_cfg->arm_v7s_cfg.ttbr[1];
		} else {
			cb->ttbr[0] = pgtbl_cfg->arm_lpae_s1_cfg.ttbr[0];
			cb->ttbr[0] |= (u64)cfg->asid << TTBRn_ASID_SHIFT;
			cb->ttbr[1] = pgtbl_cfg->arm_lpae_s1_cfg.ttbr[1];
			cb->ttbr[1] |= (u64)cfg->asid << TTBRn_ASID_SHIFT;
		}
	} else {
		cb->ttbr[0] = pgtbl_cfg->arm_lpae_s2_cfg.vttbr;
	}

	/* MAIRs (stage-1 only) */
	if (stage1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
			cb->mair[0] = pgtbl_cfg->arm_v7s_cfg.prrr;
			cb->mair[1] = pgtbl_cfg->arm_v7s_cfg.nmrr;
		} else {
			cb->mair[0] = pgtbl_cfg->arm_lpae_s1_cfg.mair[0];
			cb->mair[1] = pgtbl_cfg->arm_lpae_s1_cfg.mair[1];
		}
	}
}

static void arm_smmu_write_context_bank(struct arm_smmu_device *smmu, int idx,
					u32 attributes)
{
	u32 reg;
	bool stage1;
	struct arm_smmu_cb *cb = &smmu->cbs[idx];
	struct arm_smmu_cfg *cfg = cb->cfg;
	void __iomem *cb_base, *gr1_base;

	cb_base = ARM_SMMU_CB(smmu, idx);

	/* Unassigned context banks only need disabling */
	if (!cfg) {
		writel_relaxed(0, cb_base + ARM_SMMU_CB_SCTLR);
		return;
	}

	gr1_base = ARM_SMMU_GR1(smmu);
	stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;

	/* CBA2R */
	if (smmu->version > ARM_SMMU_V1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64)
			reg = CBA2R_RW64_64BIT;
		else
			reg = CBA2R_RW64_32BIT;
		/* 16-bit VMIDs live in CBA2R */
		if (smmu->features & ARM_SMMU_FEAT_VMID16)
			reg |= cfg->vmid << CBA2R_VMID_SHIFT;

		writel_relaxed(reg, gr1_base + ARM_SMMU_GR1_CBA2R(idx));
	}

	/* CBAR */
	reg = cfg->cbar;
	if (smmu->version < ARM_SMMU_V2)
		reg |= cfg->irptndx << CBAR_IRPTNDX_SHIFT;

	/*
	 * Use the weakest shareability/memory types, so they are
	 * overridden by the ttbcr/pte.
	 */
	if (stage1) {
		reg |= (CBAR_S1_BPSHCFG_NSH << CBAR_S1_BPSHCFG_SHIFT) |
			(CBAR_S1_MEMATTR_WB << CBAR_S1_MEMATTR_SHIFT);
	} else if (!(smmu->features & ARM_SMMU_FEAT_VMID16)) {
		/* 8-bit VMIDs live in CBAR */
		reg |= cfg->vmid << CBAR_VMID_SHIFT;
	}
	writel_relaxed(reg, gr1_base + ARM_SMMU_GR1_CBAR(idx));

	/*
	 * TTBCR
	 * We must write this before the TTBRs, since it determines the
	 * access behaviour of some fields (in particular, ASID[15:8]).
	 */
	if (stage1 && smmu->version > ARM_SMMU_V1)
		writel_relaxed(cb->tcr[1], cb_base + ARM_SMMU_CB_TTBCR2);
	writel_relaxed(cb->tcr[0], cb_base + ARM_SMMU_CB_TTBCR);

	/* TTBRs */
	if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
		writel_relaxed(cfg->asid, cb_base + ARM_SMMU_CB_CONTEXTIDR);
		writel_relaxed(cb->ttbr[0], cb_base + ARM_SMMU_CB_TTBR0);
		writel_relaxed(cb->ttbr[1], cb_base + ARM_SMMU_CB_TTBR1);
	} else {
		writeq_relaxed(cb->ttbr[0], cb_base + ARM_SMMU_CB_TTBR0);
		if (stage1)
			writeq_relaxed(cb->ttbr[1], cb_base + ARM_SMMU_CB_TTBR1);
	}

	/* MAIRs (stage-1 only) */
	if (stage1) {
		writel_relaxed(cb->mair[0], cb_base + ARM_SMMU_CB_S1_MAIR0);
		writel_relaxed(cb->mair[1], cb_base + ARM_SMMU_CB_S1_MAIR1);
	}

	/* ACTLR (implementation defined) */
	writel_relaxed(cb->actlr, cb_base + ARM_SMMU_CB_ACTLR);

	/* SCTLR */
	reg = SCTLR_CFCFG | SCTLR_CFIE | SCTLR_CFRE | SCTLR_AFE | SCTLR_TRE;

	/* Ensure bypass transactions are Non-shareable */
	reg |= SCTLR_SHCFG_NSH << SCTLR_SHCFG_SHIFT;

	if (attributes & (1 << DOMAIN_ATTR_CB_STALL_DISABLE)) {
		reg &= ~SCTLR_CFCFG;
		reg |= SCTLR_HUPCF;
	}

	if (attributes & (1 << DOMAIN_ATTR_NO_CFRE))
		reg &= ~SCTLR_CFRE;

	if ((!(attributes & (1 << DOMAIN_ATTR_S1_BYPASS)) &&
	     !(attributes & (1 << DOMAIN_ATTR_EARLY_MAP))) || !stage1)
		reg |= SCTLR_M;
	if (stage1)
		reg |= SCTLR_S1_ASIDPNE;
	if (IS_ENABLED(CONFIG_CPU_BIG_ENDIAN))
		reg |= SCTLR_E;

	writel_relaxed(reg, cb_base + ARM_SMMU_CB_SCTLR);
}

static int arm_smmu_init_asid(struct iommu_domain *domain,
				struct arm_smmu_device *smmu)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	bool dynamic = is_dynamic_domain(domain);
	int ret;

	if (!dynamic) {
		cfg->asid = cfg->cbndx + 1;
	} else {
		mutex_lock(&smmu->idr_mutex);
		ret = idr_alloc_cyclic(&smmu->asid_idr, domain,
				smmu->num_context_banks + 2,
				MAX_ASID + 1, GFP_KERNEL);

		mutex_unlock(&smmu->idr_mutex);
		if (ret < 0) {
			dev_err(smmu->dev, "dynamic ASID allocation failed: %d\n",
				ret);
			return ret;
		}
		cfg->asid = ret;
	}
	return 0;
}

static void arm_smmu_free_asid(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	bool dynamic = is_dynamic_domain(domain);

	if (cfg->asid == INVALID_ASID || !dynamic)
		return;

	mutex_lock(&smmu->idr_mutex);
	idr_remove(&smmu->asid_idr, cfg->asid);
	mutex_unlock(&smmu->idr_mutex);
}

static int arm_smmu_init_domain_context(struct iommu_domain *domain,
					struct arm_smmu_device *smmu,
					struct device *dev)
{
	int irq, start, ret = 0;
	unsigned long ias, oas;
	struct io_pgtable_ops *pgtbl_ops;
	enum io_pgtable_fmt fmt;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	const struct iommu_gather_ops *tlb_ops;
	bool is_fast = smmu_domain->attributes & (1 << DOMAIN_ATTR_FAST);
	unsigned long quirks = 0;
	bool dynamic;

	mutex_lock(&smmu_domain->init_mutex);
	if (smmu_domain->smmu)
		goto out_unlock;

	if (domain->type == IOMMU_DOMAIN_IDENTITY) {
		smmu_domain->stage = ARM_SMMU_DOMAIN_BYPASS;
		smmu_domain->smmu = smmu;
		smmu_domain->cfg.irptndx = INVALID_IRPTNDX;
		smmu_domain->cfg.asid = INVALID_ASID;
	}

	dynamic = is_dynamic_domain(domain);
	if (dynamic && !(smmu->options & ARM_SMMU_OPT_DYNAMIC)) {
		dev_err(smmu->dev, "dynamic domains not supported\n");
		ret = -EPERM;

		goto out_unlock;
	}

	if (arm_smmu_has_secure_vmid(smmu_domain) &&
	    arm_smmu_opt_hibernation(smmu)) {
		dev_err(smmu->dev,
			"Secure usecases not supported with hibernation\n");
		ret = -EPERM;
		goto out_unlock;
	}

	/*
	 * Mapping the requested stage onto what we support is surprisingly
	 * complicated, mainly because the spec allows S1+S2 SMMUs without
	 * support for nested translation. That means we end up with the
	 * following table:
	 *
	 * Requested        Supported        Actual
	 *     S1               N              S1
	 *     S1             S1+S2            S1
	 *     S1               S2             S2
	 *     S1               S1             S1
	 *     N                N              N
	 *     N              S1+S2            S2
	 *     N                S2             S2
	 *     N                S1             S1
	 *
	 * Note that you can't actually request stage-2 mappings.
	 */
	if (!(smmu->features & ARM_SMMU_FEAT_TRANS_S1))
		smmu_domain->stage = ARM_SMMU_DOMAIN_S2;
	if (!(smmu->features & ARM_SMMU_FEAT_TRANS_S2))
		smmu_domain->stage = ARM_SMMU_DOMAIN_S1;

	/*
	 * Choosing a suitable context format is even more fiddly. Until we
	 * grow some way for the caller to express a preference, and/or move
	 * the decision into the io-pgtable code where it arguably belongs,
	 * just aim for the closest thing to the rest of the system, and hope
	 * that the hardware isn't esoteric enough that we can't assume AArch64
	 * support to be a superset of AArch32 support...
	 */
	if (smmu->features & ARM_SMMU_FEAT_FMT_AARCH32_L)
		cfg->fmt = ARM_SMMU_CTX_FMT_AARCH32_L;
	if (IS_ENABLED(CONFIG_IOMMU_IO_PGTABLE_ARMV7S) &&
	    !IS_ENABLED(CONFIG_64BIT) && !IS_ENABLED(CONFIG_ARM_LPAE) &&
	    (smmu->features & ARM_SMMU_FEAT_FMT_AARCH32_S) &&
	    (smmu_domain->stage == ARM_SMMU_DOMAIN_S1))
		cfg->fmt = ARM_SMMU_CTX_FMT_AARCH32_S;
	if ((IS_ENABLED(CONFIG_64BIT) || cfg->fmt == ARM_SMMU_CTX_FMT_NONE) &&
	    (smmu->features & (ARM_SMMU_FEAT_FMT_AARCH64_64K |
			       ARM_SMMU_FEAT_FMT_AARCH64_16K |
			       ARM_SMMU_FEAT_FMT_AARCH64_4K)))
		cfg->fmt = ARM_SMMU_CTX_FMT_AARCH64;

	if (cfg->fmt == ARM_SMMU_CTX_FMT_NONE) {
		ret = -EINVAL;
		goto out_unlock;
	}

	switch (smmu_domain->stage) {
	case ARM_SMMU_DOMAIN_S1:
		cfg->cbar = CBAR_TYPE_S1_TRANS_S2_BYPASS;
		start = smmu->num_s2_context_banks;
		ias = smmu->va_size;
		oas = smmu->ipa_size;
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64) {
			fmt = ARM_64_LPAE_S1;
			if (smmu->options & ARM_SMMU_OPT_3LVL_TABLES)
				ias = min(ias, 39UL);
		} else if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_L) {
			fmt = ARM_32_LPAE_S1;
			ias = min(ias, 32UL);
			oas = min(oas, 40UL);
		} else {
			fmt = ARM_V7S;
			ias = min(ias, 32UL);
			oas = min(oas, 32UL);
		}
		tlb_ops = &arm_smmu_s1_tlb_ops;
		break;
	case ARM_SMMU_DOMAIN_NESTED:
		/*
		 * We will likely want to change this if/when KVM gets
		 * involved.
		 */
	case ARM_SMMU_DOMAIN_S2:
		cfg->cbar = CBAR_TYPE_S2_TRANS;
		start = 0;
		ias = smmu->ipa_size;
		oas = smmu->pa_size;
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64) {
			fmt = ARM_64_LPAE_S2;
		} else {
			fmt = ARM_32_LPAE_S2;
			ias = min(ias, 40UL);
			oas = min(oas, 40UL);
		}
		if (smmu->version == ARM_SMMU_V2)
			tlb_ops = &arm_smmu_s2_tlb_ops_v2;
		else
			tlb_ops = &arm_smmu_s2_tlb_ops_v1;
		break;
	default:
		ret = -EINVAL;
		goto out_unlock;
	}

	if (is_fast)
		fmt = ARM_V8L_FAST;

	if (smmu_domain->attributes & (1 << DOMAIN_ATTR_USE_UPSTREAM_HINT))
		quirks |= IO_PGTABLE_QUIRK_QCOM_USE_UPSTREAM_HINT;
	if (is_iommu_pt_coherent(smmu_domain))
		quirks |= IO_PGTABLE_QUIRK_NO_DMA;
	if (smmu_domain->attributes & (1 << DOMAIN_ATTR_USE_LLC_NWA))
		quirks |= IO_PGTABLE_QUIRK_QCOM_USE_LLC_NWA;
	if (((quirks & IO_PGTABLE_QUIRK_QCOM_USE_UPSTREAM_HINT) ||
	     (quirks & IO_PGTABLE_QUIRK_QCOM_USE_LLC_NWA)) &&
		(smmu->model == QCOM_SMMUV500))
		quirks |= IO_PGTABLE_QUIRK_QSMMUV500_NON_SHAREABLE;

	if (arm_smmu_is_slave_side_secure(smmu_domain))
		tlb_ops = &msm_smmu_gather_ops;

	ret = arm_smmu_alloc_cb(domain, smmu, dev);
	if (ret < 0)
		goto out_unlock;

	cfg->cbndx = ret;

	if (arm_smmu_is_slave_side_secure(smmu_domain)) {
		smmu_domain->pgtbl_cfg = (struct io_pgtable_cfg) {
			.quirks         = quirks,
			.pgsize_bitmap  = smmu->pgsize_bitmap,
			.arm_msm_secure_cfg = {
				.sec_id = smmu->sec_id,
				.cbndx = cfg->cbndx,
			},
			.tlb		= tlb_ops,
			.iommu_dev      = smmu->dev,
		};
		fmt = ARM_MSM_SECURE;
	} else  {
		smmu_domain->pgtbl_cfg = (struct io_pgtable_cfg) {
			.quirks		= quirks,
			.pgsize_bitmap	= smmu->pgsize_bitmap,
			.ias		= ias,
			.oas		= oas,
			.tlb		= tlb_ops,
			.iommu_dev	= smmu->dev,
		};
	}

	smmu_domain->smmu = smmu;
	smmu_domain->dev = dev;
	pgtbl_ops = alloc_io_pgtable_ops(fmt, &smmu_domain->pgtbl_cfg,
					smmu_domain);
	if (!pgtbl_ops) {
		ret = -ENOMEM;
		goto out_clear_smmu;
	}

	/*
	 * assign any page table memory that might have been allocated
	 * during alloc_io_pgtable_ops
	 */
	arm_smmu_secure_domain_lock(smmu_domain);
	arm_smmu_assign_table(smmu_domain);
	arm_smmu_secure_domain_unlock(smmu_domain);

	/* Update the domain's page sizes to reflect the page table format */
	domain->pgsize_bitmap = smmu_domain->pgtbl_cfg.pgsize_bitmap;
	domain->geometry.aperture_end = (1UL << ias) - 1;
	domain->geometry.force_aperture = true;

	/* Assign an asid */
	ret = arm_smmu_init_asid(domain, smmu);
	if (ret)
		goto out_clear_smmu;

	if (!dynamic) {
		/* Initialise the context bank with our page table cfg */
		arm_smmu_init_context_bank(smmu_domain,
						&smmu_domain->pgtbl_cfg);
		arm_smmu_arch_init_context_bank(smmu_domain, dev);
		arm_smmu_write_context_bank(smmu, cfg->cbndx,
						smmu_domain->attributes );
		/* for slave side secure, we may have to force the pagetable
		 * format to V8L.
		 */
		ret = arm_smmu_set_pt_format(smmu_domain,
					     &smmu_domain->pgtbl_cfg);
		if (ret)
			goto out_clear_smmu;

		if (smmu->version < ARM_SMMU_V2) {
			cfg->irptndx = atomic_inc_return(&smmu->irptndx);
			cfg->irptndx %= smmu->num_context_irqs;
		} else {
			cfg->irptndx = cfg->cbndx;
		}

		/*
		 * Request context fault interrupt. Do this last to avoid the
		 * handler seeing a half-initialised domain state.
		 */
		irq = smmu->irqs[smmu->num_global_irqs + cfg->irptndx];
		ret = devm_request_threaded_irq(smmu->dev, irq, NULL,
			arm_smmu_context_fault, IRQF_ONESHOT | IRQF_SHARED,
			"arm-smmu-context-fault", domain);
		if (ret < 0) {
			dev_err(smmu->dev, "failed to request context IRQ %d (%u)\n",
				cfg->irptndx, irq);
			cfg->irptndx = INVALID_IRPTNDX;
			goto out_clear_smmu;
		}
	} else {
		cfg->irptndx = INVALID_IRPTNDX;
	}
	mutex_unlock(&smmu_domain->init_mutex);

	/* Publish page table ops for map/unmap */
	smmu_domain->pgtbl_ops = pgtbl_ops;
	if (arm_smmu_is_slave_side_secure(smmu_domain) &&
			!arm_smmu_master_attached(smmu, dev->iommu_fwspec))
		arm_smmu_restore_sec_cfg(smmu, cfg->cbndx);

	return 0;

out_clear_smmu:
	arm_smmu_destroy_domain_context(domain);
	smmu_domain->smmu = NULL;
out_unlock:
	mutex_unlock(&smmu_domain->init_mutex);
	return ret;
}

static void arm_smmu_domain_reinit(struct arm_smmu_domain *smmu_domain)
{
	smmu_domain->cfg.irptndx = INVALID_IRPTNDX;
	smmu_domain->cfg.cbndx = INVALID_CBNDX;
	smmu_domain->secure_vmid = VMID_INVAL;
}

static void arm_smmu_destroy_domain_context(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	void __iomem *cb_base;
	int irq;
	bool dynamic;
	int ret;

	if (!smmu || domain->type == IOMMU_DOMAIN_IDENTITY)
		return;

	ret = arm_smmu_power_on(smmu->pwr);
	if (ret) {
		WARN_ONCE(ret, "Woops, powering on smmu %p failed. Leaking context bank\n",
				smmu);
		return;
	}

	dynamic = is_dynamic_domain(domain);
	if (dynamic) {
		arm_smmu_free_asid(domain);
		free_io_pgtable_ops(smmu_domain->pgtbl_ops);
		arm_smmu_power_off(smmu->pwr);
		arm_smmu_secure_domain_lock(smmu_domain);
		arm_smmu_secure_pool_destroy(smmu_domain);
		arm_smmu_unassign_table(smmu_domain);
		arm_smmu_secure_domain_unlock(smmu_domain);
		arm_smmu_domain_reinit(smmu_domain);
		return;
	}

	/*
	 * Disable the context bank and free the page tables before freeing
	 * it.
	 */
	smmu->cbs[cfg->cbndx].cfg = NULL;
	cb_base = ARM_SMMU_CB(smmu, cfg->cbndx);
	writel_relaxed(0, cb_base + ARM_SMMU_CB_SCTLR);

	if (cfg->irptndx != INVALID_IRPTNDX) {
		irq = smmu->irqs[smmu->num_global_irqs + cfg->irptndx];
		devm_free_irq(smmu->dev, irq, domain);
	}

	free_io_pgtable_ops(smmu_domain->pgtbl_ops);
	arm_smmu_secure_domain_lock(smmu_domain);
	arm_smmu_secure_pool_destroy(smmu_domain);
	arm_smmu_unassign_table(smmu_domain);
	arm_smmu_secure_domain_unlock(smmu_domain);
	__arm_smmu_free_bitmap(smmu->context_map, cfg->cbndx);

	arm_smmu_power_off(smmu->pwr);
	arm_smmu_domain_reinit(smmu_domain);
}

static struct iommu_domain *arm_smmu_domain_alloc(unsigned type)
{
	struct arm_smmu_domain *smmu_domain;

	/* Do not support DOMAIN_DMA for now */
	if (type != IOMMU_DOMAIN_UNMANAGED &&
	    type != IOMMU_DOMAIN_IDENTITY)
		return NULL;
	/*
	 * Allocate the domain and initialise some of its data structures.
	 * We can't really do anything meaningful until we've added a
	 * master.
	 */
	smmu_domain = kzalloc(sizeof(*smmu_domain), GFP_KERNEL);
	if (!smmu_domain)
		return NULL;

	if (type == IOMMU_DOMAIN_DMA && (using_legacy_binding ||
	    iommu_get_dma_cookie(&smmu_domain->domain))) {
		kfree(smmu_domain);
		return NULL;
	}

	mutex_init(&smmu_domain->init_mutex);
	spin_lock_init(&smmu_domain->cb_lock);
	spin_lock_init(&smmu_domain->sync_lock);
	INIT_LIST_HEAD(&smmu_domain->pte_info_list);
	INIT_LIST_HEAD(&smmu_domain->unassign_list);
	mutex_init(&smmu_domain->assign_lock);
	INIT_LIST_HEAD(&smmu_domain->secure_pool_list);
	INIT_LIST_HEAD(&smmu_domain->nonsecure_pool);
	arm_smmu_domain_reinit(smmu_domain);

	return &smmu_domain->domain;
}

static void arm_smmu_domain_free(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);

	/*
	 * Free the domain resources. We assume that all devices have
	 * already been detached.
	 */
	iommu_put_dma_cookie(domain);
	arm_smmu_destroy_domain_context(domain);
	kfree(smmu_domain);
}

static void arm_smmu_write_smr(struct arm_smmu_device *smmu, int idx)
{
	struct arm_smmu_smr *smr = smmu->smrs + idx;
	u32 reg = smr->id << SMR_ID_SHIFT | smr->mask << SMR_MASK_SHIFT;

	if (!(smmu->features & ARM_SMMU_FEAT_EXIDS) && smr->valid)
		reg |= SMR_VALID;
	writel_relaxed(reg, ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_SMR(idx));
}

static void arm_smmu_write_s2cr(struct arm_smmu_device *smmu, int idx)
{
	struct arm_smmu_s2cr *s2cr = smmu->s2crs + idx;
	u32 reg = (s2cr->type & S2CR_TYPE_MASK) << S2CR_TYPE_SHIFT |
		  (s2cr->cbndx & S2CR_CBNDX_MASK) << S2CR_CBNDX_SHIFT |
		  (s2cr->privcfg & S2CR_PRIVCFG_MASK) << S2CR_PRIVCFG_SHIFT |
		  S2CR_SHCFG_NSH << S2CR_SHCFG_SHIFT;

	if (smmu->features & ARM_SMMU_FEAT_EXIDS && smmu->smrs &&
	    smmu->smrs[idx].valid)
		reg |= S2CR_EXIDVALID;
	writel_relaxed(reg, ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_S2CR(idx));
}

static void arm_smmu_write_sme(struct arm_smmu_device *smmu, int idx)
{
	arm_smmu_write_s2cr(smmu, idx);
	if (smmu->smrs)
		arm_smmu_write_smr(smmu, idx);
}

/*
 * The width of SMR's mask field depends on sCR0_EXIDENABLE, so this function
 * should be called after sCR0 is written.
 */
static void arm_smmu_test_smr_masks(struct arm_smmu_device *smmu)
{
	unsigned long size;
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);
	u32 smr, id;
	int idx;

	/* Check if Stream Match Register support is included */
	if (!smmu->smrs)
		return;

	/* For slave side secure targets, as we can't write to the
	 * global space, set the sme mask values to default.
	 */
	if (arm_smmu_is_static_cb(smmu)) {
		smmu->streamid_mask = SID_MASK;
		smmu->smr_mask_mask = SMR_MASK_MASK;
		return;
	}

	/* ID0 */
	id = readl_relaxed(gr0_base + ARM_SMMU_GR0_ID0);
	size = (id >> ID0_NUMSMRG_SHIFT) & ID0_NUMSMRG_MASK;

	/*
	 * Few SMR registers may be inuse before the smmu driver
	 * probes(say by the bootloader). Find a SMR register
	 * which is not inuse.
	 */
	for (idx = 0; idx < size; idx++) {
		smr = readl_relaxed(gr0_base + ARM_SMMU_GR0_SMR(idx));
		if (!(smr & SMR_VALID))
			break;
	}
	if (idx == size) {
		dev_err(smmu->dev,
				"Unable to compute streamid_masks\n");
		return;
	}

	/*
	 * SMR.ID bits may not be preserved if the corresponding MASK
	 * bits are set, so check each one separately. We can reject
	 * masters later if they try to claim IDs outside these masks.
	 */
	smr = smmu->streamid_mask << SMR_ID_SHIFT;
	writel_relaxed(smr, gr0_base + ARM_SMMU_GR0_SMR(idx));
	smr = readl_relaxed(gr0_base + ARM_SMMU_GR0_SMR(idx));
	smmu->streamid_mask = smr >> SMR_ID_SHIFT;

	smr = smmu->streamid_mask << SMR_MASK_SHIFT;
	writel_relaxed(smr, gr0_base + ARM_SMMU_GR0_SMR(idx));
	smr = readl_relaxed(gr0_base + ARM_SMMU_GR0_SMR(idx));
	smmu->smr_mask_mask = smr >> SMR_MASK_SHIFT;
}

static int arm_smmu_find_sme(struct arm_smmu_device *smmu, u16 id, u16 mask)
{
	struct arm_smmu_smr *smrs = smmu->smrs;
	int i, free_idx = -ENOSPC;

	/* Stream indexing is blissfully easy */
	if (!smrs)
		return id;

	/* Validating SMRs is... less so */
	for (i = 0; i < smmu->num_mapping_groups; ++i) {
		if (!smrs[i].valid) {
			/*
			 * Note the first free entry we come across, which
			 * we'll claim in the end if nothing else matches.
			 */
			if (free_idx < 0)
				free_idx = i;
			continue;
		}
		/*
		 * If the new entry is _entirely_ matched by an existing entry,
		 * then reuse that, with the guarantee that there also cannot
		 * be any subsequent conflicting entries. In normal use we'd
		 * expect simply identical entries for this case, but there's
		 * no harm in accommodating the generalisation.
		 */
		if ((mask & smrs[i].mask) == mask &&
		    !((id ^ smrs[i].id) & ~smrs[i].mask))
			return i;
		/*
		 * If the new entry has any other overlap with an existing one,
		 * though, then there always exists at least one stream ID
		 * which would cause a conflict, and we can't allow that risk.
		 */
		if (!((id ^ smrs[i].id) & ~(smrs[i].mask | mask)))
			return -EINVAL;
	}

	return free_idx;
}

static bool arm_smmu_free_sme(struct arm_smmu_device *smmu, int idx)
{
	if (--smmu->s2crs[idx].count)
		return false;

	smmu->s2crs[idx] = s2cr_init_val;
	if (smmu->smrs)
		smmu->smrs[idx].valid = false;

	return true;
}

static int arm_smmu_master_alloc_smes(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
	struct arm_smmu_master_cfg *cfg = fwspec->iommu_priv;
	struct arm_smmu_device *smmu = cfg->smmu;
	struct arm_smmu_smr *smrs = smmu->smrs;
	struct iommu_group *group;
	int i, idx, ret;

	mutex_lock(&smmu->iommu_group_mutex);
	mutex_lock(&smmu->stream_map_mutex);
	/* Figure out a viable stream map entry allocation */
	for_each_cfg_sme(fwspec, i, idx) {
		u16 sid = fwspec->ids[i];
		u16 mask = fwspec->ids[i] >> SMR_MASK_SHIFT;

		if (idx != INVALID_SMENDX) {
			ret = -EEXIST;
			goto sme_err;
		}

		ret = arm_smmu_find_sme(smmu, sid, mask);
		if (ret < 0)
			goto sme_err;

		idx = ret;
		if (smrs && smmu->s2crs[idx].count == 0) {
			smrs[idx].id = sid;
			smrs[idx].mask = mask;
			smrs[idx].valid = true;
		}
		smmu->s2crs[idx].count++;
		cfg->smendx[i] = (s16)idx;
	}
	mutex_unlock(&smmu->stream_map_mutex);

	group = iommu_group_get_for_dev(dev);
	if (!group)
		group = ERR_PTR(-ENOMEM);
	if (IS_ERR(group)) {
		ret = PTR_ERR(group);
		goto iommu_group_err;
	}
	iommu_group_put(group);

	/* It worked! Don't poke the actual hardware until we've attached */
	for_each_cfg_sme(fwspec, i, idx)
		smmu->s2crs[idx].group = group;

	mutex_unlock(&smmu->iommu_group_mutex);
	return 0;

iommu_group_err:
	mutex_lock(&smmu->stream_map_mutex);

sme_err:
	while (i--) {
		arm_smmu_free_sme(smmu, cfg->smendx[i]);
		cfg->smendx[i] = INVALID_SMENDX;
	}
	mutex_unlock(&smmu->stream_map_mutex);
	mutex_unlock(&smmu->iommu_group_mutex);
	return ret;
}

static void arm_smmu_master_free_smes(struct iommu_fwspec *fwspec)
{
	struct arm_smmu_device *smmu = fwspec_smmu(fwspec);
	struct arm_smmu_master_cfg *cfg = fwspec->iommu_priv;
	int i, idx;

	mutex_lock(&smmu->stream_map_mutex);
	for_each_cfg_sme(fwspec, i, idx) {
		if (arm_smmu_free_sme(smmu, idx))
			arm_smmu_write_sme(smmu, idx);
		cfg->smendx[i] = INVALID_SMENDX;
	}
	mutex_unlock(&smmu->stream_map_mutex);
}

static void arm_smmu_domain_remove_master(struct arm_smmu_domain *smmu_domain,
					  struct iommu_fwspec *fwspec)
{
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_s2cr *s2cr = smmu->s2crs;
	int i, idx;
	const struct iommu_gather_ops *tlb;

	tlb = smmu_domain->pgtbl_cfg.tlb;

	mutex_lock(&smmu->stream_map_mutex);
	for_each_cfg_sme(fwspec, i, idx) {
		if (WARN_ON(s2cr[idx].attach_count == 0)) {
			mutex_unlock(&smmu->stream_map_mutex);
			return;
		}
		s2cr[idx].attach_count -= 1;

		if (s2cr[idx].attach_count > 0)
			continue;

		writel_relaxed(0, ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_SMR(idx));
		writel_relaxed(0, ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_S2CR(idx));
	}
	mutex_unlock(&smmu->stream_map_mutex);

	/* Ensure there are no stale mappings for this context bank */
	tlb->tlb_flush_all(smmu_domain);
}

static int arm_smmu_domain_add_master(struct arm_smmu_domain *smmu_domain,
				      struct iommu_fwspec *fwspec)
{
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_s2cr *s2cr = smmu->s2crs;
	u8 cbndx = smmu_domain->cfg.cbndx;
	enum arm_smmu_s2cr_type type;
	int i, idx;

	if (smmu_domain->stage == ARM_SMMU_DOMAIN_BYPASS)
		type = S2CR_TYPE_BYPASS;
	else
		type = S2CR_TYPE_TRANS;

	mutex_lock(&smmu->stream_map_mutex);
	for_each_cfg_sme(fwspec, i, idx) {
		if (s2cr[idx].attach_count++ > 0)
			continue;

		s2cr[idx].type = type;
		s2cr[idx].privcfg = S2CR_PRIVCFG_DEFAULT;
		s2cr[idx].cbndx = cbndx;
		arm_smmu_write_sme(smmu, idx);
	}
	mutex_unlock(&smmu->stream_map_mutex);

	return 0;
}

static void arm_smmu_detach_dev(struct iommu_domain *domain,
				struct device *dev)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
	int dynamic = smmu_domain->attributes & (1 << DOMAIN_ATTR_DYNAMIC);
	int atomic_domain = smmu_domain->attributes & (1 << DOMAIN_ATTR_ATOMIC);

	if (dynamic)
		return;

	if (!smmu) {
		dev_err(dev, "Domain not attached; cannot detach!\n");
		return;
	}

	if (atomic_domain)
		arm_smmu_power_on_atomic(smmu->pwr);
	else
		arm_smmu_power_on(smmu->pwr);

	arm_smmu_domain_remove_master(smmu_domain, fwspec);
	arm_smmu_power_off(smmu->pwr);
}

static int arm_smmu_assign_table(struct arm_smmu_domain *smmu_domain)
{
	int ret = 0;
	int dest_vmids[2] = {VMID_HLOS, smmu_domain->secure_vmid};
	int dest_perms[2] = {PERM_READ | PERM_WRITE, PERM_READ};
	int source_vmid = VMID_HLOS;
	struct arm_smmu_pte_info *pte_info, *temp;

	if (!arm_smmu_is_master_side_secure(smmu_domain))
		return ret;

	list_for_each_entry(pte_info, &smmu_domain->pte_info_list, entry) {
		ret = hyp_assign_phys(virt_to_phys(pte_info->virt_addr),
				      PAGE_SIZE, &source_vmid, 1,
				      dest_vmids, dest_perms, 2);
		if (WARN_ON(ret))
			break;
	}

	list_for_each_entry_safe(pte_info, temp, &smmu_domain->pte_info_list,
								entry) {
		list_del(&pte_info->entry);
		kfree(pte_info);
	}
	return ret;
}

static void arm_smmu_unassign_table(struct arm_smmu_domain *smmu_domain)
{
	int ret;
	int dest_vmids = VMID_HLOS;
	int dest_perms = PERM_READ | PERM_WRITE | PERM_EXEC;
	int source_vmlist[2] = {VMID_HLOS, smmu_domain->secure_vmid};
	struct arm_smmu_pte_info *pte_info, *temp;

	if (!arm_smmu_is_master_side_secure(smmu_domain))
		return;

	list_for_each_entry(pte_info, &smmu_domain->unassign_list, entry) {
		ret = hyp_assign_phys(virt_to_phys(pte_info->virt_addr),
				      PAGE_SIZE, source_vmlist, 2,
				      &dest_vmids, &dest_perms, 1);
		if (WARN_ON(ret))
			break;
		free_pages_exact(pte_info->virt_addr, pte_info->size);
	}

	list_for_each_entry_safe(pte_info, temp, &smmu_domain->unassign_list,
				 entry) {
		list_del(&pte_info->entry);
		kfree(pte_info);
	}
}

static void arm_smmu_unprepare_pgtable(void *cookie, void *addr, size_t size)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_pte_info *pte_info;

	if (smmu_domain->slave_side_secure ||
	    !arm_smmu_has_secure_vmid(smmu_domain)) {
		if (smmu_domain->slave_side_secure)
			WARN(1, "slave side secure is enforced\n");
		else
			WARN(1, "Invalid VMID is set !!\n");
		return;
	}

	pte_info = kzalloc(sizeof(struct arm_smmu_pte_info), GFP_ATOMIC);
	if (!pte_info)
		return;

	pte_info->virt_addr = addr;
	pte_info->size = size;
	list_add_tail(&pte_info->entry, &smmu_domain->unassign_list);
}

static int arm_smmu_prepare_pgtable(void *addr, void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_pte_info *pte_info;

	if (smmu_domain->slave_side_secure ||
	    !arm_smmu_has_secure_vmid(smmu_domain)) {
		if (smmu_domain->slave_side_secure)
			WARN(1, "slave side secure is enforced\n");
		else
			WARN(1, "Invalid VMID is set !!\n");
		return -EINVAL;
	}

	pte_info = kzalloc(sizeof(struct arm_smmu_pte_info), GFP_ATOMIC);
	if (!pte_info)
		return -ENOMEM;
	pte_info->virt_addr = addr;
	list_add_tail(&pte_info->entry, &smmu_domain->pte_info_list);
	return 0;
}

static void arm_smmu_prealloc_memory(struct arm_smmu_domain *smmu_domain,
					size_t size, struct list_head *pool)
{
	int i;
	u32 nr = 0;
	struct page *page;

	if ((smmu_domain->attributes & (1 << DOMAIN_ATTR_ATOMIC)) ||
			arm_smmu_has_secure_vmid(smmu_domain))
		return;

	/* number of 2nd level pagetable entries */
	nr += round_up(size, SZ_1G) >> 30;
	/* number of 3rd level pagetabel entries */
	nr += round_up(size, SZ_2M) >> 21;

	/* Retry later with atomic allocation on error */
	for (i = 0; i < nr; i++) {
		page = alloc_pages(GFP_KERNEL | __GFP_ZERO, 0);
		if (!page)
			break;
		list_add(&page->lru, pool);
	}
}

static void arm_smmu_prealloc_memory_sg(struct arm_smmu_domain *smmu_domain,
					struct scatterlist *sgl, int nents,
					struct list_head *pool)
{
	int i;
	size_t size = 0;
	struct scatterlist *sg;

	if ((smmu_domain->attributes & (1 << DOMAIN_ATTR_ATOMIC)) ||
			arm_smmu_has_secure_vmid(smmu_domain))
		return;

	for_each_sg(sgl, sg, nents, i)
		size += sg->length;

	arm_smmu_prealloc_memory(smmu_domain, size, pool);
}

static void arm_smmu_release_prealloc_memory(
		struct arm_smmu_domain *smmu_domain, struct list_head *list)
{
	struct page *page, *tmp;

	list_for_each_entry_safe(page, tmp, list, lru) {
		list_del(&page->lru);
		__free_pages(page, 0);
	}
}

static int arm_smmu_attach_dev(struct iommu_domain *domain, struct device *dev)
{
	int ret;
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
	struct arm_smmu_device *smmu;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	int atomic_domain = smmu_domain->attributes & (1 << DOMAIN_ATTR_ATOMIC);

	if (!fwspec || fwspec->ops != &arm_smmu_ops) {
		dev_err(dev, "cannot attach to SMMU, is it on the same bus?\n");
		return -ENXIO;
	}

	/*
	 * FIXME: The arch/arm DMA API code tries to attach devices to its own
	 * domains between of_xlate() and add_device() - we have no way to cope
	 * with that, so until ARM gets converted to rely on groups and default
	 * domains, just say no (but more politely than by dereferencing NULL).
	 * This should be at least a WARN_ON once that's sorted.
	 */
	if (!fwspec->iommu_priv)
		return -ENODEV;

	smmu = fwspec_smmu(fwspec);

	/* Enable Clocks and Power */
	ret = arm_smmu_power_on(smmu->pwr);
	if (ret)
		return ret;

	/* Ensure that the domain is finalised */
	ret = arm_smmu_init_domain_context(domain, smmu, dev);
	if (ret < 0)
		goto out_power_off;

	/* Do not modify the SIDs, HW is still running */
	if (is_dynamic_domain(domain)) {
		ret = 0;
		goto out_power_off;
	}

	/*
	 * Sanity check the domain. We don't support domains across
	 * different SMMUs.
	 */
	if (smmu_domain->smmu != smmu) {
		dev_err(dev,
			"cannot attach to SMMU %s whilst already attached to domain on SMMU %s\n",
			dev_name(smmu_domain->smmu->dev), dev_name(smmu->dev));
		ret = -EINVAL;
		goto out_power_off;
	}

	/* Looks ok, so add the device to the domain */
	ret = arm_smmu_domain_add_master(smmu_domain, fwspec);

out_power_off:
	/*
	 * Keep an additional vote for non-atomic power until domain is
	 * detached
	 */
	if (!ret && atomic_domain) {
		WARN_ON(arm_smmu_power_on(smmu->pwr));
		arm_smmu_power_off_atomic(smmu->pwr);
	}

	arm_smmu_power_off(smmu->pwr);

	return ret;
}

static int arm_smmu_map(struct iommu_domain *domain, unsigned long iova,
			phys_addr_t paddr, size_t size, int prot)
{
	int ret;
	unsigned long flags;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops = to_smmu_domain(domain)->pgtbl_ops;
	LIST_HEAD(nonsecure_pool);

	if (!ops)
		return -ENODEV;

	if (arm_smmu_is_slave_side_secure(smmu_domain))
		return msm_secure_smmu_map(domain, iova, paddr, size, prot);

	arm_smmu_prealloc_memory(smmu_domain, size, &nonsecure_pool);
	arm_smmu_secure_domain_lock(smmu_domain);

	spin_lock_irqsave(&smmu_domain->cb_lock, flags);
	list_splice_init(&nonsecure_pool, &smmu_domain->nonsecure_pool);
	ret = ops->map(ops, iova, paddr, size, prot);
	list_splice_init(&smmu_domain->nonsecure_pool, &nonsecure_pool);
	spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);

	arm_smmu_assign_table(smmu_domain);
	arm_smmu_secure_domain_unlock(smmu_domain);

	arm_smmu_release_prealloc_memory(smmu_domain, &nonsecure_pool);
	return ret;
}

static uint64_t arm_smmu_iova_to_pte(struct iommu_domain *domain,
	      dma_addr_t iova)
{
	uint64_t ret;
	unsigned long flags;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops = smmu_domain->pgtbl_ops;

	if (!ops || !ops->iova_to_pte)
		return 0;

	spin_lock_irqsave(&smmu_domain->cb_lock, flags);
	ret = ops->iova_to_pte(ops, iova);
	spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);
	return ret;
}

static size_t arm_smmu_unmap(struct iommu_domain *domain, unsigned long iova,
			     size_t size)
{
	size_t ret;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops = smmu_domain->pgtbl_ops;
	unsigned long flags;

	if (!ops)
		return 0;

	if (arm_smmu_is_slave_side_secure(smmu_domain))
		return msm_secure_smmu_unmap(domain, iova, size);

	ret = arm_smmu_domain_power_on(domain, smmu_domain->smmu);
	if (ret)
		return ret;

	arm_smmu_secure_domain_lock(smmu_domain);

	spin_lock_irqsave(&smmu_domain->cb_lock, flags);
	ret = ops->unmap(ops, iova, size);
	spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);

	arm_smmu_domain_power_off(domain, smmu_domain->smmu);
	/*
	 * While splitting up block mappings, we might allocate page table
	 * memory during unmap, so the vmids needs to be assigned to the
	 * memory here as well.
	 */
	arm_smmu_assign_table(smmu_domain);
	/* Also unassign any pages that were free'd during unmap */
	arm_smmu_unassign_table(smmu_domain);
	arm_smmu_secure_domain_unlock(smmu_domain);
	return ret;
}

#define MAX_MAP_SG_BATCH_SIZE (SZ_4M)
static size_t arm_smmu_map_sg(struct iommu_domain *domain, unsigned long iova,
			   struct scatterlist *sg, unsigned int nents, int prot)
{
	int ret;
	size_t size, batch_size, size_to_unmap = 0;
	unsigned long flags;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops = smmu_domain->pgtbl_ops;
	unsigned int idx_start, idx_end;
	struct scatterlist *sg_start, *sg_end;
	unsigned long __saved_iova_start;
	LIST_HEAD(nonsecure_pool);

	if (!ops)
		return -ENODEV;

	if (arm_smmu_is_slave_side_secure(smmu_domain))
		return msm_secure_smmu_map_sg(domain, iova, sg, nents, prot);

	arm_smmu_prealloc_memory_sg(smmu_domain, sg, nents, &nonsecure_pool);
	arm_smmu_secure_domain_lock(smmu_domain);

	__saved_iova_start = iova;
	idx_start = idx_end = 0;
	sg_start = sg_end = sg;
	while (idx_end < nents) {
		batch_size = sg_end->length;
		sg_end = sg_next(sg_end);
		idx_end++;
		while ((idx_end < nents) &&
		       (batch_size + sg_end->length < MAX_MAP_SG_BATCH_SIZE)) {

			batch_size += sg_end->length;
			sg_end = sg_next(sg_end);
			idx_end++;
		}

		spin_lock_irqsave(&smmu_domain->cb_lock, flags);
		list_splice_init(&nonsecure_pool, &smmu_domain->nonsecure_pool);
		ret = ops->map_sg(ops, iova, sg_start, idx_end - idx_start,
				  prot, &size);
		list_splice_init(&smmu_domain->nonsecure_pool, &nonsecure_pool);
		spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);

		/* Returns 0 on error */
		if (!ret) {
			size_to_unmap = iova + size - __saved_iova_start;
			goto out;
		}

		iova += batch_size;
		idx_start = idx_end;
		sg_start = sg_end;
	}

out:
	arm_smmu_assign_table(smmu_domain);

	if (size_to_unmap) {
		arm_smmu_unmap(domain, __saved_iova_start, size_to_unmap);
		iova = __saved_iova_start;
	}
	arm_smmu_secure_domain_unlock(smmu_domain);
	arm_smmu_release_prealloc_memory(smmu_domain, &nonsecure_pool);
	return iova - __saved_iova_start;
}

static phys_addr_t __arm_smmu_iova_to_phys_hard(struct iommu_domain *domain,
					      dma_addr_t iova)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct io_pgtable_ops *ops= smmu_domain->pgtbl_ops;
	struct device *dev = smmu->dev;
	void __iomem *cb_base;
	u32 tmp;
	u64 phys;
	unsigned long va;

	cb_base = ARM_SMMU_CB(smmu, cfg->cbndx);

	/* ATS1 registers can only be written atomically */
	va = iova & ~0xfffUL;
	if (smmu->version == ARM_SMMU_V2)
		smmu_write_atomic_lq(va, cb_base + ARM_SMMU_CB_ATS1PR);
	else /* Register is only 32-bit in v1 */
		writel_relaxed(va, cb_base + ARM_SMMU_CB_ATS1PR);

	if (readl_poll_timeout_atomic(cb_base + ARM_SMMU_CB_ATSR, tmp,
				      !(tmp & ATSR_ACTIVE), 5, 50)) {
		phys = ops->iova_to_phys(ops, iova);
		dev_err(dev,
			"iova to phys timed out on %pad. software table walk result=%pa.\n",
			&iova, &phys);
		phys = 0;
		return phys;
	}

	phys = readq_relaxed(cb_base + ARM_SMMU_CB_PAR);
	if (phys & CB_PAR_F) {
		dev_err(dev, "translation fault!\n");
		dev_err(dev, "PAR = 0x%llx\n", phys);
		phys = 0;
	} else {
		phys = (phys & (PHYS_MASK & ~0xfffULL)) | (iova & 0xfff);
	}

	return phys;
}

static phys_addr_t arm_smmu_iova_to_phys(struct iommu_domain *domain,
					dma_addr_t iova)
{
	phys_addr_t ret;
	unsigned long flags;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops = smmu_domain->pgtbl_ops;

	if (domain->type == IOMMU_DOMAIN_IDENTITY)
		return iova;

	if (!ops)
		return 0;

	spin_lock_irqsave(&smmu_domain->cb_lock, flags);
	ret = ops->iova_to_phys(ops, iova);
	spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);

	return ret;
}

/*
 * This function can sleep, and cannot be called from atomic context. Will
 * power on register block if required. This restriction does not apply to the
 * original iova_to_phys() op.
 */
static phys_addr_t arm_smmu_iova_to_phys_hard(struct iommu_domain *domain,
					dma_addr_t iova)
{
	phys_addr_t ret = 0;
	unsigned long flags;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;

	if (smmu->options & ARM_SMMU_OPT_DISABLE_ATOS)
		return 0;

	if (arm_smmu_power_on(smmu_domain->smmu->pwr))
		return 0;

	if (smmu_domain->smmu->arch_ops &&
	    smmu_domain->smmu->arch_ops->iova_to_phys_hard) {
		ret = smmu_domain->smmu->arch_ops->iova_to_phys_hard(
						domain, iova);
		goto out;
	}

	spin_lock_irqsave(&smmu_domain->cb_lock, flags);
	if (smmu_domain->smmu->features & ARM_SMMU_FEAT_TRANS_OPS &&
			smmu_domain->stage == ARM_SMMU_DOMAIN_S1)
		ret = __arm_smmu_iova_to_phys_hard(domain, iova);

	spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);

out:
	arm_smmu_power_off(smmu_domain->smmu->pwr);

	return ret;
}

static bool arm_smmu_capable(enum iommu_cap cap)
{
	switch (cap) {
	case IOMMU_CAP_CACHE_COHERENCY:
		/*
		 * Return true here as the SMMU can always send out coherent
		 * requests.
		 */
		return true;
	case IOMMU_CAP_NOEXEC:
		return true;
	default:
		return false;
	}
}

static int arm_smmu_match_node(struct device *dev, void *data)
{
	return dev->fwnode == data;
}

static
struct arm_smmu_device *arm_smmu_get_by_fwnode(struct fwnode_handle *fwnode)
{
	struct device *dev = driver_find_device(&arm_smmu_driver.driver, NULL,
						fwnode, arm_smmu_match_node);
	put_device(dev);
	return dev ? dev_get_drvdata(dev) : NULL;
}

#ifdef CONFIG_MSM_TZ_SMMU
static int msm_secure_smmu_map(struct iommu_domain *domain, unsigned long iova,
			       phys_addr_t paddr, size_t size, int prot)
{
	size_t ret;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops = smmu_domain->pgtbl_ops;

	ret = ops->map(ops, iova, paddr, size, prot);

	return ret;
}

static size_t msm_secure_smmu_unmap(struct iommu_domain *domain,
				    unsigned long iova,
				    size_t size)
{
	size_t ret;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops = smmu_domain->pgtbl_ops;

	ret = arm_smmu_domain_power_on(domain, smmu_domain->smmu);
	if (ret)
		return ret;

	ret = ops->unmap(ops, iova, size);

	arm_smmu_domain_power_off(domain, smmu_domain->smmu);

	return ret;
}

static size_t msm_secure_smmu_map_sg(struct iommu_domain *domain,
				     unsigned long iova,
				     struct scatterlist *sg,
				     unsigned int nents, int prot)
{
	int ret;
	size_t size;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops = smmu_domain->pgtbl_ops;

	ret = ops->map_sg(ops, iova, sg, nents, prot, &size);

	if (!ret)
		msm_secure_smmu_unmap(domain, iova, size);

	return ret;
}

#endif

static int arm_smmu_add_device(struct device *dev)
{
	struct arm_smmu_device *smmu;
	struct arm_smmu_master_cfg *cfg;
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
	int i, ret;

	if (using_legacy_binding) {
		ret = arm_smmu_register_legacy_master(dev, &smmu);

		/*
		 * If dev->iommu_fwspec is initally NULL, arm_smmu_register_legacy_master()
		 * will allocate/initialise a new one. Thus we need to update fwspec for
		 * later use.
		 */
		fwspec = dev->iommu_fwspec;
		if (ret)
			goto out_free;
	} else if (fwspec && fwspec->ops == &arm_smmu_ops) {
		smmu = arm_smmu_get_by_fwnode(fwspec->iommu_fwnode);
		if (!smmu)
			return -ENODEV;
	} else {
		return -ENODEV;
	}

	ret = arm_smmu_power_on(smmu->pwr);
	if (ret)
		goto out_free;

	ret = -EINVAL;
	for (i = 0; i < fwspec->num_ids; i++) {
		u16 sid = fwspec->ids[i];
		u16 mask = fwspec->ids[i] >> SMR_MASK_SHIFT;

		if (sid & ~smmu->streamid_mask) {
			dev_err(dev, "stream ID 0x%x out of range for SMMU (0x%x)\n",
				sid, smmu->streamid_mask);
			goto out_pwr_off;
		}
		if (mask & ~smmu->smr_mask_mask) {
			dev_err(dev, "SMR mask 0x%x out of range for SMMU (0x%x)\n",
				mask, smmu->smr_mask_mask);
			goto out_pwr_off;
		}
	}

	ret = -ENOMEM;
	cfg = kzalloc(offsetof(struct arm_smmu_master_cfg, smendx[i]),
		      GFP_KERNEL);
	if (!cfg)
		goto out_pwr_off;

	cfg->smmu = smmu;
	fwspec->iommu_priv = cfg;
	while (i--)
		cfg->smendx[i] = INVALID_SMENDX;

	ret = arm_smmu_master_alloc_smes(dev);
	if (ret)
		goto out_cfg_free;

	arm_smmu_power_off(smmu->pwr);
	return 0;

out_cfg_free:
	kfree(cfg);
out_pwr_off:
	arm_smmu_power_off(smmu->pwr);
out_free:
	iommu_fwspec_free(dev);
	return ret;
}

static void arm_smmu_remove_device(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
	struct arm_smmu_device *smmu;

	if (!fwspec || fwspec->ops != &arm_smmu_ops)
		return;

	smmu = fwspec_smmu(fwspec);
	if (arm_smmu_power_on(smmu->pwr)) {
		WARN_ON(1);
		return;
	}

	arm_smmu_master_free_smes(fwspec);
	iommu_group_remove_device(dev);
	kfree(fwspec->iommu_priv);
	iommu_fwspec_free(dev);
	arm_smmu_power_off(smmu->pwr);
}

static struct iommu_group *arm_smmu_device_group(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
	struct arm_smmu_device *smmu = fwspec_smmu(fwspec);
	struct iommu_group *group = NULL;
	int i, idx;

	for_each_cfg_sme(fwspec, i, idx) {
		if (group && smmu->s2crs[idx].group &&
		    group != smmu->s2crs[idx].group)
			return ERR_PTR(-EINVAL);

		group = smmu->s2crs[idx].group;
	}

	if (group)
		iommu_group_ref_get(group);
	else {
		if (dev_is_pci(dev))
			group = pci_device_group(dev);
		else
			group = generic_device_group(dev);

		if (IS_ERR(group))
			return NULL;
	}

	if (arm_smmu_arch_device_group(dev, group)) {
		iommu_group_put(group);
		return ERR_PTR(-EINVAL);
	}

	return group;
}

static int arm_smmu_domain_get_attr(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	int ret = 0;

	if (domain->type != IOMMU_DOMAIN_UNMANAGED)
		return -EINVAL;

	mutex_lock(&smmu_domain->init_mutex);
	switch (attr) {
	case DOMAIN_ATTR_NESTING:
		*(int *)data = (smmu_domain->stage == ARM_SMMU_DOMAIN_NESTED);
		ret = 0;
		break;
	case DOMAIN_ATTR_PT_BASE_ADDR:
		*((phys_addr_t *)data) =
			smmu_domain->pgtbl_cfg.arm_lpae_s1_cfg.ttbr[0];
		ret = 0;
		break;
	case DOMAIN_ATTR_CONTEXT_BANK:
		/* context bank index isn't valid until we are attached */
		if (smmu_domain->smmu == NULL) {
			ret = -ENODEV;
			break;
		}
		*((unsigned int *) data) = smmu_domain->cfg.cbndx;
		ret = 0;
		break;
	case DOMAIN_ATTR_TTBR0: {
		u64 val;
		struct arm_smmu_device *smmu = smmu_domain->smmu;
		/* not valid until we are attached */
		if (smmu == NULL) {
			ret = -ENODEV;
			break;
		}
		val = smmu_domain->pgtbl_cfg.arm_lpae_s1_cfg.ttbr[0];
		if (smmu_domain->cfg.cbar != CBAR_TYPE_S2_TRANS)
			val |= (u64)ARM_SMMU_CB_ASID(smmu, &smmu_domain->cfg)
					<< (TTBRn_ASID_SHIFT);
		*((u64 *)data) = val;
		ret = 0;
		break;
	}
	case DOMAIN_ATTR_CONTEXTIDR:
		/* not valid until attached */
		if (smmu_domain->smmu == NULL) {
			ret = -ENODEV;
			break;
		}
		*((u32 *)data) = smmu_domain->cfg.procid;
		ret = 0;
		break;
	case DOMAIN_ATTR_PROCID:
		*((u32 *)data) = smmu_domain->cfg.procid;
		ret = 0;
		break;
	case DOMAIN_ATTR_DYNAMIC:
		*((int *)data) = !!(smmu_domain->attributes
					& (1 << DOMAIN_ATTR_DYNAMIC));
		ret = 0;
		break;
	case DOMAIN_ATTR_NON_FATAL_FAULTS:
		*((int *)data) = !!(smmu_domain->attributes
				    & (1 << DOMAIN_ATTR_NON_FATAL_FAULTS));
		ret = 0;
		break;
	case DOMAIN_ATTR_S1_BYPASS:
		*((int *)data) = !!(smmu_domain->attributes
				    & (1 << DOMAIN_ATTR_S1_BYPASS));
		ret = 0;
		break;
	case DOMAIN_ATTR_SECURE_VMID:
		*((int *)data) = smmu_domain->secure_vmid;
		ret = 0;
		break;
	case DOMAIN_ATTR_PGTBL_INFO: {
		struct iommu_pgtbl_info *info = data;

		if (!(smmu_domain->attributes & (1 << DOMAIN_ATTR_FAST))) {
			ret = -ENODEV;
			break;
		}
		info->pmds = smmu_domain->pgtbl_cfg.av8l_fast_cfg.pmds;
		ret = 0;
		break;
	}
	case DOMAIN_ATTR_FAST:
		*((int *)data) = !!(smmu_domain->attributes
					& (1 << DOMAIN_ATTR_FAST));
		ret = 0;
		break;
	case DOMAIN_ATTR_USE_UPSTREAM_HINT:
		*((int *)data) = !!(smmu_domain->attributes &
				   (1 << DOMAIN_ATTR_USE_UPSTREAM_HINT));
		ret = 0;
		break;
	case DOMAIN_ATTR_USE_LLC_NWA:
		*((int *)data) = !!(smmu_domain->attributes &
				   (1 << DOMAIN_ATTR_USE_LLC_NWA));
		ret = 0;
		break;
	case DOMAIN_ATTR_EARLY_MAP:
		*((int *)data) = !!(smmu_domain->attributes
				    & (1 << DOMAIN_ATTR_EARLY_MAP));
		ret = 0;
		break;
	case DOMAIN_ATTR_BITMAP_IOVA_ALLOCATOR:
		*((int *)data) = !!(smmu_domain->attributes
				& (1 << DOMAIN_ATTR_BITMAP_IOVA_ALLOCATOR));
		ret = 0;
		break;
	case DOMAIN_ATTR_PAGE_TABLE_IS_COHERENT:
		if (!smmu_domain->smmu) {
			ret = -ENODEV;
			break;
		}
		*((int *)data) = is_iommu_pt_coherent(smmu_domain);
		ret = 0;
		break;
	case DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT:
		*((int *)data) = !!(smmu_domain->attributes
			& (1 << DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT));
		ret = 0;
		break;
	case DOMAIN_ATTR_CB_STALL_DISABLE:
		*((int *)data) = !!(smmu_domain->attributes
			& (1 << DOMAIN_ATTR_CB_STALL_DISABLE));
		ret = 0;
		break;
	case DOMAIN_ATTR_NO_CFRE:
		*((int *)data) = !!(smmu_domain->attributes
			& (1 << DOMAIN_ATTR_NO_CFRE));
		ret = 0;
		break;
	case DOMAIN_ATTR_QCOM_MMU500_ERRATA_MIN_IOVA_ALIGN:
		*((int *)data) = smmu_domain->qsmmuv500_errata1_min_iova_align;
		ret = 0;
		break;
	default:
		ret = -ENODEV;
		break;
	}
	mutex_unlock(&smmu_domain->init_mutex);
	return ret;
}

static int arm_smmu_domain_set_attr(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data)
{
	int ret = 0;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);

	if (domain->type != IOMMU_DOMAIN_UNMANAGED)
		return -EINVAL;

	mutex_lock(&smmu_domain->init_mutex);

	switch (attr) {
	case DOMAIN_ATTR_NESTING:
		if (smmu_domain->smmu) {
			ret = -EPERM;
			goto out_unlock;
		}

		if (*(int *)data)
			smmu_domain->stage = ARM_SMMU_DOMAIN_NESTED;
		else
			smmu_domain->stage = ARM_SMMU_DOMAIN_S1;

		break;
	case DOMAIN_ATTR_PROCID:
		if (smmu_domain->smmu != NULL) {
			dev_err(smmu_domain->smmu->dev,
			  "cannot change procid attribute while attached\n");
			ret = -EBUSY;
			break;
		}
		smmu_domain->cfg.procid = *((u32 *)data);
		ret = 0;
		break;
	case DOMAIN_ATTR_DYNAMIC: {
		int dynamic = *((int *)data);

		if (smmu_domain->smmu != NULL) {
			dev_err(smmu_domain->smmu->dev,
			  "cannot change dynamic attribute while attached\n");
			ret = -EBUSY;
			break;
		}

		if (dynamic)
			smmu_domain->attributes |= 1 << DOMAIN_ATTR_DYNAMIC;
		else
			smmu_domain->attributes &= ~(1 << DOMAIN_ATTR_DYNAMIC);
		ret = 0;
		break;
	}
	case DOMAIN_ATTR_CONTEXT_BANK:
		/* context bank can't be set while attached */
		if (smmu_domain->smmu != NULL) {
			ret = -EBUSY;
			break;
		}
		/* ... and it can only be set for dynamic contexts. */
		if (!(smmu_domain->attributes & (1 << DOMAIN_ATTR_DYNAMIC))) {
			ret = -EINVAL;
			break;
		}

		/* this will be validated during attach */
		smmu_domain->cfg.cbndx = *((unsigned int *)data);
		ret = 0;
		break;
	case DOMAIN_ATTR_NON_FATAL_FAULTS: {
		u32 non_fatal_faults = *((int *)data);

		if (non_fatal_faults)
			smmu_domain->attributes |=
					1 << DOMAIN_ATTR_NON_FATAL_FAULTS;
		else
			smmu_domain->attributes &=
					~(1 << DOMAIN_ATTR_NON_FATAL_FAULTS);
		ret = 0;
		break;
	}
	case DOMAIN_ATTR_S1_BYPASS: {
		int bypass = *((int *)data);

		/* bypass can't be changed while attached */
		if (smmu_domain->smmu != NULL) {
			ret = -EBUSY;
			break;
		}
		if (bypass)
			smmu_domain->attributes |= 1 << DOMAIN_ATTR_S1_BYPASS;
		else
			smmu_domain->attributes &=
					~(1 << DOMAIN_ATTR_S1_BYPASS);

		ret = 0;
		break;
	}
	case DOMAIN_ATTR_ATOMIC:
	{
		int atomic_ctx = *((int *)data);

		/* can't be changed while attached */
		if (smmu_domain->smmu != NULL) {
			ret = -EBUSY;
			break;
		}
		if (atomic_ctx)
			smmu_domain->attributes |= (1 << DOMAIN_ATTR_ATOMIC);
		else
			smmu_domain->attributes &= ~(1 << DOMAIN_ATTR_ATOMIC);
		break;
	}
	case DOMAIN_ATTR_SECURE_VMID:
		if (smmu_domain->secure_vmid != VMID_INVAL) {
			ret = -ENODEV;
			WARN(1, "secure vmid already set!");
			break;
		}
		smmu_domain->secure_vmid = *((int *)data);
		break;
		/*
		 * fast_smmu_unmap_page() and fast_smmu_alloc_iova() both
		 * expect that the bus/clock/regulator are already on. Thus also
		 * force DOMAIN_ATTR_ATOMIC to bet set.
		 */
	case DOMAIN_ATTR_FAST:
		if (*((int *)data)) {
			smmu_domain->attributes |= 1 << DOMAIN_ATTR_FAST;
			smmu_domain->attributes |= 1 << DOMAIN_ATTR_ATOMIC;
		}
		ret = 0;
		break;
	case DOMAIN_ATTR_USE_UPSTREAM_HINT:
		/* can't be changed while attached */
		if (smmu_domain->smmu != NULL) {
			ret = -EBUSY;
			break;
		}
		if (*((int *)data))
			smmu_domain->attributes |=
				1 << DOMAIN_ATTR_USE_UPSTREAM_HINT;
		ret = 0;
		break;
	case DOMAIN_ATTR_USE_LLC_NWA:
		/* can't be changed while attached */
		if (smmu_domain->smmu != NULL) {
			ret = -EBUSY;
			break;
		}
		if (*((int *)data))
			smmu_domain->attributes |=
				1 << DOMAIN_ATTR_USE_LLC_NWA;
		ret = 0;
		break;
	case DOMAIN_ATTR_EARLY_MAP: {
		int early_map = *((int *)data);

		ret = 0;
		if (early_map) {
			smmu_domain->attributes |=
						1 << DOMAIN_ATTR_EARLY_MAP;
		} else {
			if (smmu_domain->smmu)
				ret = arm_smmu_enable_s1_translations(
								smmu_domain);

			if (!ret)
				smmu_domain->attributes &=
					~(1 << DOMAIN_ATTR_EARLY_MAP);
		}
		break;
	}
	case DOMAIN_ATTR_BITMAP_IOVA_ALLOCATOR:
		if (*((int *)data))
			smmu_domain->attributes |=
				1 << DOMAIN_ATTR_BITMAP_IOVA_ALLOCATOR;
		ret = 0;
		break;
	case DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT: {
		int force_coherent = *((int *)data);

		if (smmu_domain->smmu != NULL) {
			dev_err(smmu_domain->smmu->dev,
			  "cannot change force coherent attribute while attached\n");
			ret = -EBUSY;
			break;
		}

		if (force_coherent)
			smmu_domain->attributes |=
			    1 << DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT;
		else
			smmu_domain->attributes &=
			    ~(1 << DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT);

		ret = 0;
		break;
	}

	case DOMAIN_ATTR_CB_STALL_DISABLE:
		if (*((int *)data))
			smmu_domain->attributes |=
				1 << DOMAIN_ATTR_CB_STALL_DISABLE;
		ret = 0;
		break;
	case DOMAIN_ATTR_NO_CFRE:
		if (*((int *)data))
			smmu_domain->attributes |=
				1 << DOMAIN_ATTR_NO_CFRE;
		ret = 0;
		break;
	default:
		ret = -ENODEV;
	}

out_unlock:
	mutex_unlock(&smmu_domain->init_mutex);
	return ret;
}

static int arm_smmu_of_xlate(struct device *dev, struct of_phandle_args *args)
{
	u32 mask, fwid = 0;

	if (args->args_count > 0)
		fwid |= (u16)args->args[0];

	if (args->args_count > 1)
		fwid |= (u16)args->args[1] << SMR_MASK_SHIFT;
	else if (!of_property_read_u32(args->np, "stream-match-mask", &mask))
		fwid |= (u16)mask << SMR_MASK_SHIFT;

	return iommu_fwspec_add_ids(dev, &fwid, 1);
}

static void arm_smmu_get_resv_regions(struct device *dev,
				      struct list_head *head)
{
	struct iommu_resv_region *region;
	int prot = IOMMU_WRITE | IOMMU_NOEXEC | IOMMU_MMIO;

	region = iommu_alloc_resv_region(MSI_IOVA_BASE, MSI_IOVA_LENGTH,
					 prot, IOMMU_RESV_SW_MSI);
	if (!region)
		return;

	list_add_tail(&region->list, head);

	iommu_dma_get_resv_regions(dev, head);
}

static void arm_smmu_put_resv_regions(struct device *dev,
				      struct list_head *head)
{
	struct iommu_resv_region *entry, *next;

	list_for_each_entry_safe(entry, next, head, list)
		kfree(entry);
}
static int arm_smmu_enable_s1_translations(struct arm_smmu_domain *smmu_domain)
{
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	void __iomem *cb_base;
	u32 reg;
	int ret;

	cb_base = ARM_SMMU_CB(smmu, cfg->cbndx);
	ret = arm_smmu_power_on(smmu->pwr);
	if (ret)
		return ret;

	reg = readl_relaxed(cb_base + ARM_SMMU_CB_SCTLR);
	reg |= SCTLR_M;

	writel_relaxed(reg, cb_base + ARM_SMMU_CB_SCTLR);
	arm_smmu_power_off(smmu->pwr);
	return ret;
}

static bool arm_smmu_is_iova_coherent(struct iommu_domain *domain,
					 dma_addr_t iova)
{
	bool ret;
	unsigned long flags;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops = smmu_domain->pgtbl_ops;

	if (!ops)
		return false;

	spin_lock_irqsave(&smmu_domain->cb_lock, flags);
	ret = ops->is_iova_coherent(ops, iova);
	spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);
	return ret;
}

static void arm_smmu_trigger_fault(struct iommu_domain *domain,
					unsigned long flags)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu;
	void __iomem *cb_base;

	if (!smmu_domain->smmu) {
		pr_err("Can't trigger faults on non-attached domains\n");
		return;
	}

	smmu = smmu_domain->smmu;
	if (arm_smmu_power_on(smmu->pwr))
		return;

	cb_base = ARM_SMMU_CB(smmu, cfg->cbndx);
	dev_err(smmu->dev, "Writing 0x%lx to FSRRESTORE on cb %d\n",
		flags, cfg->cbndx);
	writel_relaxed(flags, cb_base + ARM_SMMU_CB_FSRRESTORE);
	/* give the interrupt time to fire... */
	msleep(1000);

	arm_smmu_power_off(smmu->pwr);
}

static unsigned long arm_smmu_reg_read(struct iommu_domain *domain,
				       unsigned long offset)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	void __iomem *cb_base;
	unsigned long val;

	if (offset >= SZ_4K) {
		pr_err("Invalid offset: 0x%lx\n", offset);
		return 0;
	}

	smmu = smmu_domain->smmu;
	if (!smmu) {
		WARN(1, "Can't read registers of a detached domain\n");
		val = 0;
		return val;
	}

	if (arm_smmu_power_on(smmu->pwr))
		return 0;

	cb_base = ARM_SMMU_CB(smmu, cfg->cbndx);
	val = readl_relaxed(cb_base + offset);

	arm_smmu_power_off(smmu->pwr);
	return val;
}

static void arm_smmu_reg_write(struct iommu_domain *domain,
			       unsigned long offset, unsigned long val)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	void __iomem *cb_base;

	if (offset >= SZ_4K) {
		pr_err("Invalid offset: 0x%lx\n", offset);
		return;
	}

	smmu = smmu_domain->smmu;
	if (!smmu) {
		WARN(1, "Can't read registers of a detached domain\n");
		return;
	}

	if (arm_smmu_power_on(smmu->pwr))
		return;

	cb_base = ARM_SMMU_CB(smmu, cfg->cbndx);
	writel_relaxed(val, cb_base + offset);

	arm_smmu_power_off(smmu->pwr);
}

static void arm_smmu_tlbi_domain(struct iommu_domain *domain)
{
	arm_smmu_tlb_inv_context_s1(to_smmu_domain(domain));
}

static int arm_smmu_enable_config_clocks(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);

	return arm_smmu_power_on(smmu_domain->smmu->pwr);
}

static void arm_smmu_disable_config_clocks(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);

	arm_smmu_power_off(smmu_domain->smmu->pwr);
}

static struct iommu_ops arm_smmu_ops = {
	.capable		= arm_smmu_capable,
	.domain_alloc		= arm_smmu_domain_alloc,
	.domain_free		= arm_smmu_domain_free,
	.attach_dev		= arm_smmu_attach_dev,
	.detach_dev		= arm_smmu_detach_dev,
	.map			= arm_smmu_map,
	.unmap			= arm_smmu_unmap,
	.map_sg			= arm_smmu_map_sg,
	.iova_to_phys		= arm_smmu_iova_to_phys,
	.iova_to_phys_hard	= arm_smmu_iova_to_phys_hard,
	.add_device		= arm_smmu_add_device,
	.remove_device		= arm_smmu_remove_device,
	.device_group		= arm_smmu_device_group,
	.domain_get_attr	= arm_smmu_domain_get_attr,
	.domain_set_attr	= arm_smmu_domain_set_attr,
	.of_xlate		= arm_smmu_of_xlate,
	.get_resv_regions	= arm_smmu_get_resv_regions,
	.put_resv_regions	= arm_smmu_put_resv_regions,
	.pgsize_bitmap		= -1UL, /* Restricted during device attach */
	.trigger_fault		= arm_smmu_trigger_fault,
	.reg_read		= arm_smmu_reg_read,
	.reg_write		= arm_smmu_reg_write,
	.tlbi_domain		= arm_smmu_tlbi_domain,
	.enable_config_clocks	= arm_smmu_enable_config_clocks,
	.disable_config_clocks	= arm_smmu_disable_config_clocks,
	.is_iova_coherent	= arm_smmu_is_iova_coherent,
	.iova_to_pte = arm_smmu_iova_to_pte,
};

#define IMPL_DEF1_MICRO_MMU_CTRL	0
#define MICRO_MMU_CTRL_LOCAL_HALT_REQ	(1 << 2)
#define MICRO_MMU_CTRL_IDLE		(1 << 3)

/* Definitions for implementation-defined registers */
#define ACTLR_QCOM_OSH_SHIFT		28
#define ACTLR_QCOM_OSH			1

#define ACTLR_QCOM_ISH_SHIFT		29
#define ACTLR_QCOM_ISH			1

#define ACTLR_QCOM_NSH_SHIFT		30
#define ACTLR_QCOM_NSH			1

static int qsmmuv2_wait_for_halt(struct arm_smmu_device *smmu)
{
	void __iomem *impl_def1_base = ARM_SMMU_IMPL_DEF1(smmu);
	u32 tmp;

	if (readl_poll_timeout_atomic(impl_def1_base + IMPL_DEF1_MICRO_MMU_CTRL,
					tmp, (tmp & MICRO_MMU_CTRL_IDLE),
					0, 30000)) {
		dev_err(smmu->dev, "Couldn't halt SMMU!\n");
		return -EBUSY;
	}

	return 0;
}

static int __qsmmuv2_halt(struct arm_smmu_device *smmu, bool wait)
{
	void __iomem *impl_def1_base = ARM_SMMU_IMPL_DEF1(smmu);
	u32 reg;

	reg = readl_relaxed(impl_def1_base + IMPL_DEF1_MICRO_MMU_CTRL);
	reg |= MICRO_MMU_CTRL_LOCAL_HALT_REQ;

	if (arm_smmu_is_static_cb(smmu)) {
		phys_addr_t impl_def1_base_phys = impl_def1_base - smmu->base +
							smmu->phys_addr;

		if (scm_io_write(impl_def1_base_phys +
					IMPL_DEF1_MICRO_MMU_CTRL, reg)) {
			dev_err(smmu->dev,
				"scm_io_write fail. SMMU might not be halted");
			return -EINVAL;
		}
	} else {
		writel_relaxed(reg, impl_def1_base + IMPL_DEF1_MICRO_MMU_CTRL);
	}

	return wait ? qsmmuv2_wait_for_halt(smmu) : 0;
}

static int qsmmuv2_halt(struct arm_smmu_device *smmu)
{
	return __qsmmuv2_halt(smmu, true);
}

static int qsmmuv2_halt_nowait(struct arm_smmu_device *smmu)
{
	return __qsmmuv2_halt(smmu, false);
}

static void qsmmuv2_resume(struct arm_smmu_device *smmu)
{
	void __iomem *impl_def1_base = ARM_SMMU_IMPL_DEF1(smmu);
	u32 reg;

	reg = readl_relaxed(impl_def1_base + IMPL_DEF1_MICRO_MMU_CTRL);
	reg &= ~MICRO_MMU_CTRL_LOCAL_HALT_REQ;

	if (arm_smmu_is_static_cb(smmu)) {
		phys_addr_t impl_def1_base_phys = impl_def1_base - smmu->base +
							smmu->phys_addr;

		if (scm_io_write(impl_def1_base_phys +
				IMPL_DEF1_MICRO_MMU_CTRL, reg))
			dev_err(smmu->dev,
				"scm_io_write fail. SMMU might not be resumed");
	} else {
		writel_relaxed(reg, impl_def1_base + IMPL_DEF1_MICRO_MMU_CTRL);
	}
}

static void qsmmuv2_device_reset(struct arm_smmu_device *smmu)
{
	int i;
	u32 val;
	struct arm_smmu_impl_def_reg *regs = smmu->impl_def_attach_registers;
	/*
	 * SCTLR.M must be disabled here per ARM SMMUv2 spec
	 * to prevent table walks with an inconsistent state.
	 */
	for (i = 0; i < smmu->num_context_banks; ++i) {
		struct arm_smmu_cb *cb = &smmu->cbs[i];

		val = ACTLR_QCOM_ISH << ACTLR_QCOM_ISH_SHIFT |
		ACTLR_QCOM_OSH << ACTLR_QCOM_OSH_SHIFT |
		ACTLR_QCOM_NSH << ACTLR_QCOM_NSH_SHIFT;
		cb->actlr = val;
	}

	/* Program implementation defined registers */
	qsmmuv2_halt(smmu);
	for (i = 0; i < smmu->num_impl_def_attach_registers; ++i)
		writel_relaxed(regs[i].value,
			ARM_SMMU_GR0(smmu) + regs[i].offset);
	qsmmuv2_resume(smmu);
}

static phys_addr_t qsmmuv2_iova_to_phys_hard(struct iommu_domain *domain,
				dma_addr_t iova)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	int ret;
	phys_addr_t phys = 0;
	unsigned long flags;
	u32 sctlr, sctlr_orig, fsr;
	void __iomem *cb_base;

	ret = arm_smmu_power_on(smmu_domain->smmu->pwr);
	if (ret)
		return ret;

	spin_lock_irqsave(&smmu->atos_lock, flags);
	cb_base = ARM_SMMU_CB(smmu, smmu_domain->cfg.cbndx);

	qsmmuv2_halt_nowait(smmu);
	writel_relaxed(RESUME_TERMINATE, cb_base + ARM_SMMU_CB_RESUME);
	qsmmuv2_wait_for_halt(smmu);

	/* clear FSR to allow ATOS to log any faults */
	fsr = readl_relaxed(cb_base + ARM_SMMU_CB_FSR);
	writel_relaxed(fsr, cb_base + ARM_SMMU_CB_FSR);

	/* disable stall mode momentarily */
	sctlr_orig = readl_relaxed(cb_base + ARM_SMMU_CB_SCTLR);
	sctlr = sctlr_orig & ~SCTLR_CFCFG;
	writel_relaxed(sctlr, cb_base + ARM_SMMU_CB_SCTLR);

	phys = __arm_smmu_iova_to_phys_hard(domain, iova);

	/* restore SCTLR */
	writel_relaxed(sctlr_orig, cb_base + ARM_SMMU_CB_SCTLR);

	qsmmuv2_resume(smmu);
	spin_unlock_irqrestore(&smmu->atos_lock, flags);

	arm_smmu_power_off(smmu_domain->smmu->pwr);
	return phys;
}

struct arm_smmu_arch_ops qsmmuv2_arch_ops = {
	.device_reset = qsmmuv2_device_reset,
	.iova_to_phys_hard = qsmmuv2_iova_to_phys_hard,
};

static void arm_smmu_context_bank_reset(struct arm_smmu_device *smmu)
{
	int i;
	u32 reg, major;
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);
	void __iomem *cb_base;

	if (smmu->model == ARM_MMU500) {
		/*
		 * Before clearing ARM_MMU500_ACTLR_CPRE, need to
		 * clear CACHE_LOCK bit of ACR first. And, CACHE_LOCK
		 * bit is only present in MMU-500r2 onwards.
		 */
		reg = readl_relaxed(gr0_base + ARM_SMMU_GR0_ID7);
		major = (reg >> ID7_MAJOR_SHIFT) & ID7_MAJOR_MASK;
		reg = readl_relaxed(gr0_base + ARM_SMMU_GR0_sACR);
		if (major >= 2)
			reg &= ~ARM_MMU500_ACR_CACHE_LOCK;
		/*
		 * Allow unmatched Stream IDs to allocate bypass
		 * TLB entries for reduced latency.
		 */
		reg |= ARM_MMU500_ACR_SMTNMB_TLBEN;
		writel_relaxed(reg, gr0_base + ARM_SMMU_GR0_sACR);
	}

	/* Make sure all context banks are disabled and clear CB_FSR  */
	for (i = 0; i < smmu->num_context_banks; ++i) {
		cb_base = ARM_SMMU_CB(smmu, i);

		arm_smmu_write_context_bank(smmu, i, 0);
		writel_relaxed(FSR_FAULT, cb_base + ARM_SMMU_CB_FSR);
		/*
		 * Disable MMU-500's not-particularly-beneficial next-page
		 * prefetcher for the sake of errata #841119 and #826419.
		 */
		if (smmu->model == ARM_MMU500) {
			reg = readl_relaxed(cb_base + ARM_SMMU_CB_ACTLR);
			reg &= ~ARM_MMU500_ACTLR_CPRE;
			writel_relaxed(reg, cb_base + ARM_SMMU_CB_ACTLR);
		}
	}
}

static void arm_smmu_device_reset(struct arm_smmu_device *smmu)
{
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);
	int i;
	u32 reg;

	/* clear global FSR */
	reg = readl_relaxed(ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sGFSR);
	writel_relaxed(reg, ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sGFSR);

	/*
	 * Reset stream mapping groups: Initial values mark all SMRn as
	 * invalid and all S2CRn as bypass unless overridden.
	 */
	if (!(smmu->options & ARM_SMMU_OPT_SKIP_INIT)) {
		for (i = 0; i < smmu->num_mapping_groups; ++i)
			arm_smmu_write_sme(smmu, i);

		arm_smmu_context_bank_reset(smmu);
	}

	/* Invalidate the TLB, just in case */
	writel_relaxed(0, gr0_base + ARM_SMMU_GR0_TLBIALLH);
	writel_relaxed(0, gr0_base + ARM_SMMU_GR0_TLBIALLNSNH);

	reg = readl_relaxed(ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sCR0);

	/* Enable fault reporting */
	reg |= (sCR0_GFRE | sCR0_GFIE | sCR0_GCFGFRE | sCR0_GCFGFIE);

	/* Disable TLB broadcasting. */
	reg |= (sCR0_VMIDPNE | sCR0_PTM);

	/* Enable client access, handling unmatched streams as appropriate */
	reg &= ~sCR0_CLIENTPD;
	if (disable_bypass)
		reg |= sCR0_USFCFG;
	else
		reg &= ~sCR0_USFCFG;

	/* Disable forced broadcasting */
	reg &= ~sCR0_FB;

	/* Don't upgrade barriers */
	reg &= ~(sCR0_BSU_MASK << sCR0_BSU_SHIFT);

	if (smmu->features & ARM_SMMU_FEAT_VMID16)
		reg |= sCR0_VMID16EN;

	if (smmu->features & ARM_SMMU_FEAT_EXIDS)
		reg |= sCR0_EXIDENABLE;

	/* Force bypass transaction to be Non-Shareable & not io-coherent */
	reg &= ~(sCR0_SHCFG_MASK << sCR0_SHCFG_SHIFT);
	reg |= sCR0_SHCFG_NSH << sCR0_SHCFG_SHIFT;

	/* Push the button */
	arm_smmu_tlb_sync_global(smmu);
	writel(reg, ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sCR0);

	/* Manage any implementation defined features */
	arm_smmu_arch_device_reset(smmu);
}

static int arm_smmu_id_size_to_bits(int size)
{
	switch (size) {
	case 0:
		return 32;
	case 1:
		return 36;
	case 2:
		return 40;
	case 3:
		return 42;
	case 4:
		return 44;
	case 5:
	default:
		return 48;
	}
}


/*
 * Some context banks needs to be transferred from bootloader to HLOS in a way
 * that allows ongoing traffic. The current expectation is that these context
 * banks operate in bypass mode.
 * Additionally, there must be exactly one device in devicetree with stream-ids
 * overlapping those used by the bootloader.
 */
static int arm_smmu_alloc_cb(struct iommu_domain *domain,
				struct arm_smmu_device *smmu,
				struct device *dev)
{
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	u32 i, idx;
	int cb = -EINVAL;
	bool dynamic;

	/*
	 * Dynamic domains have already set cbndx through domain attribute.
	 * Verify that they picked a valid value.
	 */
	dynamic = is_dynamic_domain(domain);
	if (dynamic) {
		cb = smmu_domain->cfg.cbndx;
		if (cb < smmu->num_context_banks)
			return cb;
		else
			return -EINVAL;
	}

	mutex_lock(&smmu->stream_map_mutex);
	for_each_cfg_sme(fwspec, i, idx) {
		if (smmu->s2crs[idx].cb_handoff)
			cb = smmu->s2crs[idx].cbndx;
	}

	if (cb >= 0 && arm_smmu_is_static_cb(smmu))
		smmu_domain->slave_side_secure = true;

	if (cb < 0 && !arm_smmu_is_static_cb(smmu)) {
		mutex_unlock(&smmu->stream_map_mutex);
		return __arm_smmu_alloc_bitmap(smmu->context_map,
						smmu->num_s2_context_banks,
						smmu->num_context_banks);
	}

	for (i = 0; i < smmu->num_mapping_groups; i++) {
		if (smmu->s2crs[i].cb_handoff && smmu->s2crs[i].cbndx == cb) {
			if (!arm_smmu_is_static_cb(smmu))
				smmu->s2crs[i].cb_handoff = false;
			smmu->s2crs[i].count -= 1;
		}
	}
	mutex_unlock(&smmu->stream_map_mutex);

	return cb;
}

static int arm_smmu_handoff_cbs(struct arm_smmu_device *smmu)
{
	u32 i, raw_smr, raw_s2cr;
	struct arm_smmu_smr smr;
	struct arm_smmu_s2cr s2cr;

	for (i = 0; i < smmu->num_mapping_groups; i++) {
		raw_smr = readl_relaxed(ARM_SMMU_GR0(smmu) +
					ARM_SMMU_GR0_SMR(i));
		if (!(raw_smr & SMR_VALID))
			continue;

		smr.mask = (raw_smr >> SMR_MASK_SHIFT) & SMR_MASK_MASK;
		smr.id = (u16)raw_smr;
		smr.valid = true;

		raw_s2cr = readl_relaxed(ARM_SMMU_GR0(smmu) +
					ARM_SMMU_GR0_S2CR(i));
		memset(&s2cr, 0, sizeof(s2cr));
		s2cr.group = NULL;
		s2cr.count = 1;
		s2cr.type = (raw_s2cr >> S2CR_TYPE_SHIFT) & S2CR_TYPE_MASK;
		s2cr.privcfg = (raw_s2cr >> S2CR_PRIVCFG_SHIFT) &
				S2CR_PRIVCFG_MASK;
		s2cr.cbndx = (u8)raw_s2cr;
		s2cr.cb_handoff = true;

		if (s2cr.type != S2CR_TYPE_TRANS)
			continue;

		smmu->smrs[i] = smr;
		smmu->s2crs[i] = s2cr;
		bitmap_set(smmu->context_map, s2cr.cbndx, 1);
		dev_dbg(smmu->dev, "Handoff smr: %x s2cr: %x cb: %d\n",
			raw_smr, raw_s2cr, s2cr.cbndx);
	}

	return 0;
}

static int arm_smmu_parse_impl_def_registers(struct arm_smmu_device *smmu)
{
	struct device *dev = smmu->dev;
	int i, ntuples, ret;
	u32 *tuples;
	struct arm_smmu_impl_def_reg *regs, *regit;

	if (!of_find_property(dev->of_node, "attach-impl-defs", &ntuples))
		return 0;

	ntuples /= sizeof(u32);
	if (ntuples % 2) {
		dev_err(dev,
			"Invalid number of attach-impl-defs registers: %d\n",
			ntuples);
		return -EINVAL;
	}

	regs = devm_kmalloc(
		dev, sizeof(*smmu->impl_def_attach_registers) * ntuples,
		GFP_KERNEL);
	if (!regs)
		return -ENOMEM;

	tuples = devm_kmalloc(dev, sizeof(u32) * ntuples * 2, GFP_KERNEL);
	if (!tuples)
		return -ENOMEM;

	ret = of_property_read_u32_array(dev->of_node, "attach-impl-defs",
					tuples, ntuples);
	if (ret)
		return ret;

	for (i = 0, regit = regs; i < ntuples; i += 2, ++regit) {
		regit->offset = tuples[i];
		regit->value = tuples[i + 1];
	}

	devm_kfree(dev, tuples);

	smmu->impl_def_attach_registers = regs;
	smmu->num_impl_def_attach_registers = ntuples / 2;

	return 0;
}


static int arm_smmu_init_clocks(struct arm_smmu_power_resources *pwr)
{
	const char *cname;
	struct property *prop;
	int i;
	struct device *dev = pwr->dev;

	pwr->num_clocks =
		of_property_count_strings(dev->of_node, "clock-names");

	if (pwr->num_clocks < 1) {
		pwr->num_clocks = 0;
		return 0;
	}

	pwr->clocks = devm_kzalloc(
		dev, sizeof(*pwr->clocks) * pwr->num_clocks,
		GFP_KERNEL);

	if (!pwr->clocks)
		return -ENOMEM;

	i = 0;
	of_property_for_each_string(dev->of_node, "clock-names",
				prop, cname) {
		struct clk *c = devm_clk_get(dev, cname);

		if (IS_ERR(c)) {
			dev_err(dev, "Couldn't get clock: %s",
				cname);
			return PTR_ERR(c);
		}

		if (clk_get_rate(c) == 0) {
			long rate = clk_round_rate(c, 1000);

			clk_set_rate(c, rate);
		}

		pwr->clocks[i] = c;

		++i;
	}
	return 0;
}

static int arm_smmu_init_regulators(struct arm_smmu_power_resources *pwr)
{
	const char *cname;
	struct property *prop;
	int i, ret = 0;
	struct device *dev = pwr->dev;

	pwr->num_gdscs =
		of_property_count_strings(dev->of_node, "qcom,regulator-names");

	if (pwr->num_gdscs < 1) {
		pwr->num_gdscs = 0;
		return 0;
	}

	pwr->gdscs = devm_kzalloc(
			dev, sizeof(*pwr->gdscs) * pwr->num_gdscs, GFP_KERNEL);

	if (!pwr->gdscs)
		return -ENOMEM;

	if (!of_property_read_u32(dev->of_node,
				  "qcom,deferred-regulator-disable-delay",
				  &(pwr->regulator_defer)))
		dev_info(dev, "regulator defer delay %d\n",
			pwr->regulator_defer);

	i = 0;
	of_property_for_each_string(dev->of_node, "qcom,regulator-names",
				prop, cname)
		pwr->gdscs[i++].supply = cname;

	ret = devm_regulator_bulk_get(dev, pwr->num_gdscs, pwr->gdscs);
	return ret;
}

static int arm_smmu_init_bus_scaling(struct arm_smmu_power_resources *pwr)
{
	struct device *dev = pwr->dev;

	/* We don't want the bus APIs to print an error message */
	if (!of_find_property(dev->of_node, "qcom,msm-bus,name", NULL)) {
		dev_dbg(dev, "No bus scaling info\n");
		return 0;
	}

	pwr->bus_dt_data = msm_bus_cl_get_pdata(pwr->pdev);
	if (!pwr->bus_dt_data) {
		dev_err(dev, "Unable to read bus-scaling from devicetree\n");
		return -EINVAL;
	}

	pwr->bus_client = msm_bus_scale_register_client(pwr->bus_dt_data);
	if (!pwr->bus_client) {
		dev_err(dev, "Bus client registration failed\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * Cleanup done by devm. Any non-devm resources must clean up themselves.
 */
static struct arm_smmu_power_resources *arm_smmu_init_power_resources(
						struct platform_device *pdev)
{
	struct arm_smmu_power_resources *pwr;
	int ret;

	pwr = devm_kzalloc(&pdev->dev, sizeof(*pwr), GFP_KERNEL);
	if (!pwr)
		return ERR_PTR(-ENOMEM);

	pwr->dev = &pdev->dev;
	pwr->pdev = pdev;
	mutex_init(&pwr->power_lock);
	spin_lock_init(&pwr->clock_refs_lock);

	ret = arm_smmu_init_clocks(pwr);
	if (ret)
		return ERR_PTR(ret);

	ret = arm_smmu_init_regulators(pwr);
	if (ret)
		return ERR_PTR(ret);

	ret = arm_smmu_init_bus_scaling(pwr);
	if (ret)
		return ERR_PTR(ret);

	return pwr;
}

/*
 * Bus APIs are devm-safe.
 */
static void arm_smmu_exit_power_resources(struct arm_smmu_power_resources *pwr)
{
	msm_bus_scale_unregister_client(pwr->bus_client);
}

static int arm_smmu_device_cfg_probe(struct arm_smmu_device *smmu)
{
	unsigned long size;
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);
	u32 id;
	bool cttw_reg, cttw_fw = smmu->features & ARM_SMMU_FEAT_COHERENT_WALK;
	int i;

	if (arm_smmu_restore_sec_cfg(smmu, 0))
		return -ENODEV;

	dev_dbg(smmu->dev, "probing hardware configuration...\n");
	dev_dbg(smmu->dev, "SMMUv%d with:\n",
			smmu->version == ARM_SMMU_V2 ? 2 : 1);

	/* ID0 */
	id = readl_relaxed(gr0_base + ARM_SMMU_GR0_ID0);

	/* Restrict available stages based on module parameter */
	if (force_stage == 1)
		id &= ~(ID0_S2TS | ID0_NTS);
	else if (force_stage == 2)
		id &= ~(ID0_S1TS | ID0_NTS);

	if (id & ID0_S1TS) {
		smmu->features |= ARM_SMMU_FEAT_TRANS_S1;
		dev_dbg(smmu->dev, "\tstage 1 translation\n");
	}

	if (id & ID0_S2TS) {
		smmu->features |= ARM_SMMU_FEAT_TRANS_S2;
		dev_dbg(smmu->dev, "\tstage 2 translation\n");
	}

	if (id & ID0_NTS) {
		smmu->features |= ARM_SMMU_FEAT_TRANS_NESTED;
		dev_dbg(smmu->dev, "\tnested translation\n");
	}

	if (!(smmu->features &
		(ARM_SMMU_FEAT_TRANS_S1 | ARM_SMMU_FEAT_TRANS_S2))) {
		dev_err(smmu->dev, "\tno translation support!\n");
		return -ENODEV;
	}

	if ((id & ID0_S1TS) &&
		((smmu->version < ARM_SMMU_V2) || !(id & ID0_ATOSNS))) {
		smmu->features |= ARM_SMMU_FEAT_TRANS_OPS;
		dev_dbg(smmu->dev, "\taddress translation ops\n");
	}

	/*
	 * In order for DMA API calls to work properly, we must defer to what
	 * the FW says about coherency, regardless of what the hardware claims.
	 * Fortunately, this also opens up a workaround for systems where the
	 * ID register value has ended up configured incorrectly.
	 */
	cttw_reg = !!(id & ID0_CTTW);
	if (cttw_fw || cttw_reg)
		dev_notice(smmu->dev, "\t%scoherent table walk\n",
			   cttw_fw ? "" : "non-");
	if (cttw_fw != cttw_reg)
		dev_notice(smmu->dev,
			   "\t(IDR0.CTTW overridden by FW configuration)\n");

	/* Max. number of entries we have for stream matching/indexing */
	if (smmu->version == ARM_SMMU_V2 && id & ID0_EXIDS) {
		smmu->features |= ARM_SMMU_FEAT_EXIDS;
		size = 1 << 16;
	} else {
		size = 1 << ((id >> ID0_NUMSIDB_SHIFT) & ID0_NUMSIDB_MASK);
	}
	smmu->streamid_mask = size - 1;
	if (id & ID0_SMS) {

		smmu->features |= ARM_SMMU_FEAT_STREAM_MATCH;
		size = (id >> ID0_NUMSMRG_SHIFT) & ID0_NUMSMRG_MASK;
		if (size == 0) {
			dev_err(smmu->dev,
				"stream-matching supported, but no SMRs present!\n");
			return -ENODEV;
		}

		/* Zero-initialised to mark as invalid */
		smmu->smrs = devm_kcalloc(smmu->dev, size, sizeof(*smmu->smrs),
					  GFP_KERNEL);
		if (!smmu->smrs)
			return -ENOMEM;

		dev_notice(smmu->dev,
			   "\tstream matching with %lu register groups", size);
	}
	/* s2cr->type == 0 means translation, so initialise explicitly */
	smmu->s2crs = devm_kmalloc_array(smmu->dev, size, sizeof(*smmu->s2crs),
					 GFP_KERNEL);
	if (!smmu->s2crs)
		return -ENOMEM;
	for (i = 0; i < size; i++)
		smmu->s2crs[i] = s2cr_init_val;

	smmu->num_mapping_groups = size;
	mutex_init(&smmu->stream_map_mutex);
	mutex_init(&smmu->iommu_group_mutex);
	spin_lock_init(&smmu->global_sync_lock);

	if (smmu->version < ARM_SMMU_V2 || !(id & ID0_PTFS_NO_AARCH32)) {
		smmu->features |= ARM_SMMU_FEAT_FMT_AARCH32_L;
		if (!(id & ID0_PTFS_NO_AARCH32S))
			smmu->features |= ARM_SMMU_FEAT_FMT_AARCH32_S;
	}

	/* ID1 */
	id = readl_relaxed(gr0_base + ARM_SMMU_GR0_ID1);
	smmu->pgshift = (id & ID1_PAGESIZE) ? 16 : 12;

	/* Check for size mismatch of SMMU address space from mapped region */
	size = 1 << (((id >> ID1_NUMPAGENDXB_SHIFT) & ID1_NUMPAGENDXB_MASK) + 1);
	size <<= smmu->pgshift;
	if (smmu->cb_base != gr0_base + size)
		dev_warn(smmu->dev,
			"SMMU address space size (0x%lx) differs from mapped region size (0x%tx)!\n",
			size * 2, (smmu->cb_base - gr0_base) * 2);

	smmu->num_s2_context_banks = (id >> ID1_NUMS2CB_SHIFT) & ID1_NUMS2CB_MASK;
	smmu->num_context_banks = (id >> ID1_NUMCB_SHIFT) & ID1_NUMCB_MASK;
	if (smmu->num_s2_context_banks > smmu->num_context_banks) {
		dev_err(smmu->dev, "impossible number of S2 context banks!\n");
		return -ENODEV;
	}
	dev_dbg(smmu->dev, "\t%u context banks (%u stage-2 only)\n",
		   smmu->num_context_banks, smmu->num_s2_context_banks);
	/*
	 * Cavium CN88xx erratum #27704.
	 * Ensure ASID and VMID allocation is unique across all SMMUs in
	 * the system.
	 */
	if (smmu->model == CAVIUM_SMMUV2) {
		smmu->cavium_id_base =
			atomic_add_return(smmu->num_context_banks,
					  &cavium_smmu_context_count);
		smmu->cavium_id_base -= smmu->num_context_banks;
		dev_notice(smmu->dev, "\tenabling workaround for Cavium erratum 27704\n");
	}
	smmu->cbs = devm_kcalloc(smmu->dev, smmu->num_context_banks,
				 sizeof(*smmu->cbs), GFP_KERNEL);
	if (!smmu->cbs)
		return -ENOMEM;
	for (i = 0; i < smmu->num_context_banks; i++) {
		void __iomem *cb_base;

		cb_base = ARM_SMMU_CB(smmu, i);
		smmu->cbs[i].actlr = readl_relaxed(cb_base + ARM_SMMU_CB_ACTLR);
	}

	/* ID2 */
	id = readl_relaxed(gr0_base + ARM_SMMU_GR0_ID2);
	size = arm_smmu_id_size_to_bits((id >> ID2_IAS_SHIFT) & ID2_IAS_MASK);
	smmu->ipa_size = size;

	/* The output mask is also applied for bypass */
	size = arm_smmu_id_size_to_bits((id >> ID2_OAS_SHIFT) & ID2_OAS_MASK);
	smmu->pa_size = size;

	if (id & ID2_VMID16)
		smmu->features |= ARM_SMMU_FEAT_VMID16;

	/*
	 * What the page table walker can address actually depends on which
	 * descriptor format is in use, but since a) we don't know that yet,
	 * and b) it can vary per context bank, this will have to do...
	 */
	if (dma_set_mask_and_coherent(smmu->dev, DMA_BIT_MASK(size)))
		dev_warn(smmu->dev,
			 "failed to set DMA mask for table walker\n");

	if (smmu->version < ARM_SMMU_V2) {
		smmu->va_size = smmu->ipa_size;
		if (smmu->version == ARM_SMMU_V1_64K)
			smmu->features |= ARM_SMMU_FEAT_FMT_AARCH64_64K;
	} else {
		size = (id >> ID2_UBS_SHIFT) & ID2_UBS_MASK;
		smmu->va_size = arm_smmu_id_size_to_bits(size);
		if (id & ID2_PTFS_4K)
			smmu->features |= ARM_SMMU_FEAT_FMT_AARCH64_4K;
		if (id & ID2_PTFS_16K)
			smmu->features |= ARM_SMMU_FEAT_FMT_AARCH64_16K;
		if (id & ID2_PTFS_64K)
			smmu->features |= ARM_SMMU_FEAT_FMT_AARCH64_64K;
	}

	/* Now we've corralled the various formats, what'll it do? */
	if (smmu->features & ARM_SMMU_FEAT_FMT_AARCH32_S)
		smmu->pgsize_bitmap |= SZ_4K | SZ_64K | SZ_1M | SZ_16M;
	if (smmu->features &
	    (ARM_SMMU_FEAT_FMT_AARCH32_L | ARM_SMMU_FEAT_FMT_AARCH64_4K))
		smmu->pgsize_bitmap |= SZ_4K | SZ_2M | SZ_1G;
	if (smmu->features & ARM_SMMU_FEAT_FMT_AARCH64_16K)
		smmu->pgsize_bitmap |= SZ_16K | SZ_32M;
	if (smmu->features & ARM_SMMU_FEAT_FMT_AARCH64_64K)
		smmu->pgsize_bitmap |= SZ_64K | SZ_512M;

	if (arm_smmu_ops.pgsize_bitmap == -1UL)
		arm_smmu_ops.pgsize_bitmap = smmu->pgsize_bitmap;
	else
		arm_smmu_ops.pgsize_bitmap |= smmu->pgsize_bitmap;
	dev_dbg(smmu->dev, "\tSupported page sizes: 0x%08lx\n",
		   smmu->pgsize_bitmap);


	if (smmu->features & ARM_SMMU_FEAT_TRANS_S1)
		dev_dbg(smmu->dev, "\tStage-1: %lu-bit VA -> %lu-bit IPA\n",
			smmu->va_size, smmu->ipa_size);

	if (smmu->features & ARM_SMMU_FEAT_TRANS_S2)
		dev_dbg(smmu->dev, "\tStage-2: %lu-bit IPA -> %lu-bit PA\n",
			smmu->ipa_size, smmu->pa_size);

	return 0;
}

struct arm_smmu_match_data {
	enum arm_smmu_arch_version version;
	enum arm_smmu_implementation model;
	struct arm_smmu_arch_ops *arch_ops;
};

#define ARM_SMMU_MATCH_DATA(name, ver, imp, ops)	\
static struct arm_smmu_match_data name = {		\
.version = ver,						\
.model = imp,						\
.arch_ops = ops,					\
}							\

struct arm_smmu_arch_ops qsmmuv500_arch_ops;

ARM_SMMU_MATCH_DATA(smmu_generic_v1, ARM_SMMU_V1, GENERIC_SMMU, NULL);
ARM_SMMU_MATCH_DATA(smmu_generic_v2, ARM_SMMU_V2, GENERIC_SMMU, NULL);
ARM_SMMU_MATCH_DATA(arm_mmu401, ARM_SMMU_V1_64K, GENERIC_SMMU, NULL);
ARM_SMMU_MATCH_DATA(arm_mmu500, ARM_SMMU_V2, ARM_MMU500, NULL);
ARM_SMMU_MATCH_DATA(cavium_smmuv2, ARM_SMMU_V2, CAVIUM_SMMUV2, NULL);
ARM_SMMU_MATCH_DATA(qcom_smmuv2, ARM_SMMU_V2, QCOM_SMMUV2, &qsmmuv2_arch_ops);
ARM_SMMU_MATCH_DATA(qcom_smmuv500, ARM_SMMU_V2, QCOM_SMMUV500,
		    &qsmmuv500_arch_ops);

static const struct of_device_id arm_smmu_of_match[] = {
	{ .compatible = "arm,smmu-v1", .data = &smmu_generic_v1 },
	{ .compatible = "arm,smmu-v2", .data = &smmu_generic_v2 },
	{ .compatible = "arm,mmu-400", .data = &smmu_generic_v1 },
	{ .compatible = "arm,mmu-401", .data = &arm_mmu401 },
	{ .compatible = "arm,mmu-500", .data = &arm_mmu500 },
	{ .compatible = "cavium,smmu-v2", .data = &cavium_smmuv2 },
	{ .compatible = "qcom,smmu-v2", .data = &qcom_smmuv2 },
	{ .compatible = "qcom,qsmmu-v500", .data = &qcom_smmuv500 },
	{ },
};
MODULE_DEVICE_TABLE(of, arm_smmu_of_match);

#ifdef CONFIG_MSM_TZ_SMMU
int register_iommu_sec_ptbl(void)
{
	struct device_node *np;

	for_each_matching_node(np, arm_smmu_of_match)
		if (of_find_property(np, "qcom,tz-device-id", NULL) &&
				of_device_is_available(np))
			break;
	if (!np)
		return -ENODEV;

	of_node_put(np);

	return msm_iommu_sec_pgtbl_init();
}
#endif

#ifdef CONFIG_ACPI
static int acpi_smmu_get_data(u32 model, struct arm_smmu_device *smmu)
{
	int ret = 0;

	switch (model) {
	case ACPI_IORT_SMMU_V1:
	case ACPI_IORT_SMMU_CORELINK_MMU400:
		smmu->version = ARM_SMMU_V1;
		smmu->model = GENERIC_SMMU;
		break;
	case ACPI_IORT_SMMU_CORELINK_MMU401:
		smmu->version = ARM_SMMU_V1_64K;
		smmu->model = GENERIC_SMMU;
		break;
	case ACPI_IORT_SMMU_V2:
		smmu->version = ARM_SMMU_V2;
		smmu->model = GENERIC_SMMU;
		break;
	case ACPI_IORT_SMMU_CORELINK_MMU500:
		smmu->version = ARM_SMMU_V2;
		smmu->model = ARM_MMU500;
		break;
	case ACPI_IORT_SMMU_CAVIUM_THUNDERX:
		smmu->version = ARM_SMMU_V2;
		smmu->model = CAVIUM_SMMUV2;
		break;
	default:
		ret = -ENODEV;
	}

	return ret;
}

static int arm_smmu_device_acpi_probe(struct platform_device *pdev,
				      struct arm_smmu_device *smmu)
{
	struct device *dev = smmu->dev;
	struct acpi_iort_node *node =
		*(struct acpi_iort_node **)dev_get_platdata(dev);
	struct acpi_iort_smmu *iort_smmu;
	int ret;

	/* Retrieve SMMU1/2 specific data */
	iort_smmu = (struct acpi_iort_smmu *)node->node_data;

	ret = acpi_smmu_get_data(iort_smmu->model, smmu);
	if (ret < 0)
		return ret;

	/* Ignore the configuration access interrupt */
	smmu->num_global_irqs = 1;

	if (iort_smmu->flags & ACPI_IORT_SMMU_COHERENT_WALK)
		smmu->features |= ARM_SMMU_FEAT_COHERENT_WALK;

	return 0;
}
#else
static inline int arm_smmu_device_acpi_probe(struct platform_device *pdev,
					     struct arm_smmu_device *smmu)
{
	return -ENODEV;
}
#endif

static void arm_smmu_bus_init(void)
{
	/* Oh, for a proper bus abstraction */
	if (!iommu_present(&platform_bus_type))
		bus_set_iommu(&platform_bus_type, &arm_smmu_ops);
#ifdef CONFIG_ARM_AMBA
	if (!iommu_present(&amba_bustype))
		bus_set_iommu(&amba_bustype, &arm_smmu_ops);
#endif
#ifdef CONFIG_PCI
	if (!iommu_present(&pci_bus_type)) {
		pci_request_acs();
		bus_set_iommu(&pci_bus_type, &arm_smmu_ops);
	}
#endif
}

static int qsmmuv500_tbu_register(struct device *dev, void *data);
static int arm_smmu_device_dt_probe(struct platform_device *pdev)
{
	const struct arm_smmu_match_data *data;
	struct resource *res;
	struct arm_smmu_device *smmu;
	struct device *dev = &pdev->dev;
	int num_irqs, i, err;
	bool legacy_binding;

	legacy_binding = of_find_property(dev->of_node, "mmu-masters", NULL);
	if (legacy_binding && !using_generic_binding) {
		if (!using_legacy_binding)
			pr_notice("deprecated \"mmu-masters\" DT property in use; DMA API support unavailable\n");
		using_legacy_binding = true;
	} else if (!legacy_binding && !using_legacy_binding) {
		using_generic_binding = true;
	} else {
		dev_err(dev, "not probing due to mismatched DT properties\n");
		return -ENODEV;
	}

	smmu = devm_kzalloc(dev, sizeof(*smmu), GFP_KERNEL);
	if (!smmu) {
		dev_err(dev, "failed to allocate arm_smmu_device\n");
		return -ENOMEM;
	}
	smmu->dev = dev;
	spin_lock_init(&smmu->atos_lock);
	idr_init(&smmu->asid_idr);
	mutex_init(&smmu->idr_mutex);

	data = of_device_get_match_data(dev);
	smmu->version = data->version;
	smmu->model = data->model;
	smmu->arch_ops = data->arch_ops;

	if (of_dma_is_coherent(dev->of_node))
		smmu->features |= ARM_SMMU_FEAT_COHERENT_WALK;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "no MEM resource info\n");
		return -EINVAL;
	}

	smmu->phys_addr = res->start;
	smmu->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(smmu->base))
		return PTR_ERR(smmu->base);
	smmu->cb_base = smmu->base + resource_size(res) / 2;
	smmu->size = resource_size(res);

	if (of_property_read_u32(dev->of_node, "#global-interrupts",
				 &smmu->num_global_irqs)) {
		dev_err(dev, "missing #global-interrupts property\n");
		return -ENODEV;
	}

	num_irqs = 0;
	while ((res = platform_get_resource(pdev, IORESOURCE_IRQ, num_irqs))) {
		num_irqs++;
		if (num_irqs > smmu->num_global_irqs)
			smmu->num_context_irqs++;
	}

	if (!smmu->num_context_irqs) {
		dev_err(dev, "found %d interrupts but expected at least %d\n",
			num_irqs, smmu->num_global_irqs + 1);
		return -ENODEV;
	}

	smmu->irqs = devm_kzalloc(dev, sizeof(*smmu->irqs) * num_irqs,
				  GFP_KERNEL);
	if (!smmu->irqs) {
		dev_err(dev, "failed to allocate %d irqs\n", num_irqs);
		return -ENOMEM;
	}

	for (i = 0; i < num_irqs; ++i) {
		int irq = platform_get_irq(pdev, i);

		if (irq < 0) {
			dev_err(dev, "failed to get irq index %d\n", i);
			return -ENODEV;
		}
		smmu->irqs[i] = irq;
	}

	parse_driver_options(smmu);

	smmu->pwr = arm_smmu_init_power_resources(pdev);
	if (IS_ERR(smmu->pwr))
		return PTR_ERR(smmu->pwr);

	err = arm_smmu_power_on(smmu->pwr);
	if (err)
		goto out_exit_power_resources;

	smmu->sec_id = msm_dev_to_device_id(dev);
	err = arm_smmu_device_cfg_probe(smmu);
	if (err)
		goto out_power_off;

	err = arm_smmu_handoff_cbs(smmu);
	if (err)
		goto out_power_off;

	err = arm_smmu_parse_impl_def_registers(smmu);
	if (err)
		goto out_power_off;

	if (smmu->version == ARM_SMMU_V2) {
		if (smmu->num_context_banks > smmu->num_context_irqs) {
			dev_err(dev,
				"found %d context irq(s) but have %d context banks. assuming %d context interrupts.\n",
				smmu->num_context_irqs, smmu->num_context_banks,
				smmu->num_context_banks);
		}

		/* Ignore superfluous interrupts */
		smmu->num_context_irqs = smmu->num_context_banks;
	}

	for (i = 0; i < smmu->num_global_irqs; ++i) {
		err = devm_request_threaded_irq(smmu->dev, smmu->irqs[i],
					NULL, arm_smmu_global_fault,
					IRQF_ONESHOT | IRQF_SHARED,
					"arm-smmu global fault", smmu);
		if (err) {
			dev_err(dev, "failed to request global IRQ %d (%u)\n",
				i, smmu->irqs[i]);
			goto out_power_off;
		}
	}

	err = arm_smmu_arch_init(smmu);
	if (err)
		goto out_power_off;

	iommu_device_set_ops(&smmu->iommu, &arm_smmu_ops);
	iommu_device_set_fwnode(&smmu->iommu, dev->fwnode);

	err = iommu_device_register(&smmu->iommu);

	if (err) {
		dev_err(dev, "Failed to register iommu\n");
		return err;
	}
	platform_set_drvdata(pdev, smmu);
	arm_smmu_device_reset(smmu);
	arm_smmu_test_smr_masks(smmu);
	arm_smmu_interrupt_selftest(smmu);
	arm_smmu_power_off(smmu->pwr);

	/*
	 * For ACPI and generic DT bindings, an SMMU will be probed before
	 * any device which might need it, so we want the bus ops in place
	 * ready to handle default domain setup as soon as any SMMU exists.
	 */
	if (!using_legacy_binding)
		arm_smmu_bus_init();

	return 0;

out_power_off:
	arm_smmu_power_off(smmu->pwr);

out_exit_power_resources:
	arm_smmu_exit_power_resources(smmu->pwr);

	return err;
}

/*
 * With the legacy DT binding in play, though, we have no guarantees about
 * probe order, but then we're also not doing default domains, so we can
 * delay setting bus ops until we're sure every possible SMMU is ready,
 * and that way ensure that no add_device() calls get missed.
 */
static int arm_smmu_legacy_bus_init(void)
{
	if (using_legacy_binding)
		arm_smmu_bus_init();
	return 0;
}
device_initcall_sync(arm_smmu_legacy_bus_init);

static int arm_smmu_device_remove(struct platform_device *pdev)
{
	struct arm_smmu_device *smmu = platform_get_drvdata(pdev);

	if (!smmu)
		return -ENODEV;

	if (arm_smmu_power_on(smmu->pwr))
		return -EINVAL;

	if (!bitmap_empty(smmu->context_map, ARM_SMMU_MAX_CBS))
		dev_err(&pdev->dev, "removing device with active domains!\n");

	idr_destroy(&smmu->asid_idr);

	/* Turn the thing off */
	writel(sCR0_CLIENTPD, ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sCR0);
	arm_smmu_power_off(smmu->pwr);

	arm_smmu_exit_power_resources(smmu->pwr);

	return 0;
}

static int __maybe_unused arm_smmu_pm_resume(struct device *dev)
{
	struct arm_smmu_device *smmu = dev_get_drvdata(dev);
	int ret;

	ret = arm_smmu_power_on(smmu->pwr);
	if (ret)
		return ret;

	arm_smmu_device_reset(smmu);
	arm_smmu_power_off(smmu->pwr);

	return 0;
}

static SIMPLE_DEV_PM_OPS(arm_smmu_pm_ops, NULL, arm_smmu_pm_resume);

static struct platform_driver arm_smmu_driver = {
	.driver	= {
		.name		= "arm-smmu",
		.of_match_table	= of_match_ptr(arm_smmu_of_match),
		.pm		= &arm_smmu_pm_ops,
		.suppress_bind_attrs = true,
	},
	.probe	= arm_smmu_device_dt_probe,
	.remove	= arm_smmu_device_remove,
};

static struct platform_driver qsmmuv500_tbu_driver;
static int __init arm_smmu_init(void)
{
	static bool registered;
	int ret = 0;
	ktime_t cur;

	if (registered)
		return 0;

	cur = ktime_get();
	ret = platform_driver_register(&qsmmuv500_tbu_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&arm_smmu_driver);
#ifdef CONFIG_MSM_TZ_SMMU
	ret = register_iommu_sec_ptbl();
#endif
	registered = !ret;
	trace_smmu_init(ktime_us_delta(ktime_get(), cur));

	return ret;
}

static void __exit arm_smmu_exit(void)
{
	return platform_driver_unregister(&arm_smmu_driver);
}

subsys_initcall(arm_smmu_init);
module_exit(arm_smmu_exit);

static int __init arm_smmu_of_init(struct device_node *np)
{
	int ret = arm_smmu_init();

	if (ret)
		return ret;

	if (!of_platform_device_create(np, NULL, platform_bus_type.dev_root))
		return -ENODEV;

	return 0;
}
IOMMU_OF_DECLARE(arm_smmuv1, "arm,smmu-v1", arm_smmu_of_init);
IOMMU_OF_DECLARE(arm_smmuv2, "arm,smmu-v2", arm_smmu_of_init);
IOMMU_OF_DECLARE(arm_mmu400, "arm,mmu-400", arm_smmu_of_init);
IOMMU_OF_DECLARE(arm_mmu401, "arm,mmu-401", arm_smmu_of_init);
IOMMU_OF_DECLARE(arm_mmu500, "arm,mmu-500", arm_smmu_of_init);
IOMMU_OF_DECLARE(cavium_smmuv2, "cavium,smmu-v2", arm_smmu_of_init);

#define TCU_HW_VERSION_HLOS1		(0x18)

#define DEBUG_SID_HALT_REG		0x0
#define DEBUG_SID_HALT_VAL		(0x1 << 16)
#define DEBUG_SID_HALT_SID_MASK		0x3ff

#define DEBUG_VA_ADDR_REG		0x8

#define DEBUG_TXN_TRIGG_REG		0x18
#define DEBUG_TXN_AXPROT_SHIFT		6
#define DEBUG_TXN_AXCACHE_SHIFT		2
#define DEBUG_TRX_WRITE			(0x1 << 1)
#define DEBUG_TXN_READ			(0x0 << 1)
#define DEBUG_TXN_TRIGGER		0x1

#define DEBUG_SR_HALT_ACK_REG		0x20
#define DEBUG_SR_HALT_ACK_VAL		(0x1 << 1)
#define DEBUG_SR_ECATS_RUNNING_VAL	(0x1 << 0)

#define DEBUG_PAR_REG			0x28
#define DEBUG_PAR_PA_MASK		((0x1ULL << 36) - 1)
#define DEBUG_PAR_PA_SHIFT		12
#define DEBUG_PAR_FAULT_VAL		0x1

#define DEBUG_AXUSER_REG		0x30
#define DEBUG_AXUSER_CDMID_MASK         0xff
#define DEBUG_AXUSER_CDMID_SHIFT        36
#define DEBUG_AXUSER_CDMID_VAL          255

#define TBU_DBG_TIMEOUT_US		100

#define QSMMUV500_ACTLR_DEEP_PREFETCH_MASK	0x3
#define QSMMUV500_ACTLR_DEEP_PREFETCH_SHIFT	0x8


struct actlr_setting {
	struct arm_smmu_smr smr;
	u32 actlr;
};

struct qsmmuv500_archdata {
	struct list_head		tbus;
	void __iomem			*tcu_base;
	u32				version;
	struct actlr_setting		*actlrs;
	u32				actlr_tbl_size;
};
#define get_qsmmuv500_archdata(smmu)				\
	((struct qsmmuv500_archdata *)(smmu->archdata))

struct qsmmuv500_tbu_device {
	struct list_head		list;
	struct device			*dev;
	struct arm_smmu_device		*smmu;
	void __iomem			*base;
	void __iomem			*status_reg;

	struct arm_smmu_power_resources *pwr;
	u32				sid_start;
	u32				num_sids;

	/* Protects halt count */
	spinlock_t			halt_lock;
	u32				halt_count;
};

struct qsmmuv500_group_iommudata {
	bool has_actlr;
	u32 actlr;
};
#define to_qsmmuv500_group_iommudata(group)				\
	((struct qsmmuv500_group_iommudata *)				\
		(iommu_group_get_iommudata(group)))


static bool arm_smmu_fwspec_match_smr(struct iommu_fwspec *fwspec,
				      struct arm_smmu_smr *smr)
{
	struct arm_smmu_smr *smr2;
	struct arm_smmu_device *smmu = fwspec_smmu(fwspec);
	int i, idx;

	for_each_cfg_sme(fwspec, i, idx) {
		smr2 = &smmu->smrs[idx];
		/* Continue if table entry does not match */
		if ((smr->id ^ smr2->id) & ~(smr->mask | smr2->mask))
			continue;
		return true;
	}
	return false;
}

static int qsmmuv500_tbu_halt(struct qsmmuv500_tbu_device *tbu,
				struct arm_smmu_domain *smmu_domain)
{
	unsigned long flags;
	u32 halt, fsr, sctlr_orig, sctlr, status;
	void __iomem *base, *cb_base;

	if (of_property_read_bool(tbu->dev->of_node,
						"qcom,opt-out-tbu-halting")) {
		dev_notice(tbu->dev, "TBU opted-out for halting!\n");
		return -EBUSY;
	}

	spin_lock_irqsave(&tbu->halt_lock, flags);
	if (tbu->halt_count) {
		tbu->halt_count++;
		spin_unlock_irqrestore(&tbu->halt_lock, flags);
		return 0;
	}

	cb_base = ARM_SMMU_CB(smmu_domain->smmu, smmu_domain->cfg.cbndx);
	base = tbu->base;
	halt = readl_relaxed(base + DEBUG_SID_HALT_REG);
	halt |= DEBUG_SID_HALT_VAL;
	writel_relaxed(halt, base + DEBUG_SID_HALT_REG);

	if (!readl_poll_timeout_atomic(base + DEBUG_SR_HALT_ACK_REG, status,
					(status & DEBUG_SR_HALT_ACK_VAL),
					0, TBU_DBG_TIMEOUT_US))
		goto out;

	fsr = readl_relaxed(cb_base + ARM_SMMU_CB_FSR);
	if (!(fsr & FSR_FAULT)) {
		dev_err(tbu->dev, "Couldn't halt TBU!\n");
		spin_unlock_irqrestore(&tbu->halt_lock, flags);
		return -ETIMEDOUT;
	}

	/*
	 * We are in a fault; Our request to halt the bus will not complete
	 * until transactions in front of us (such as the fault itself) have
	 * completed. Disable iommu faults and terminate any existing
	 * transactions.
	 */
	sctlr_orig = readl_relaxed(cb_base + ARM_SMMU_CB_SCTLR);
	sctlr = sctlr_orig & ~(SCTLR_CFCFG | SCTLR_CFIE);
	writel_relaxed(sctlr, cb_base + ARM_SMMU_CB_SCTLR);

	writel_relaxed(fsr, cb_base + ARM_SMMU_CB_FSR);
	writel_relaxed(RESUME_TERMINATE, cb_base + ARM_SMMU_CB_RESUME);

	if (readl_poll_timeout_atomic(base + DEBUG_SR_HALT_ACK_REG, status,
					(status & DEBUG_SR_HALT_ACK_VAL),
					0, TBU_DBG_TIMEOUT_US)) {
		dev_err(tbu->dev, "Couldn't halt TBU from fault context!\n");
		writel_relaxed(sctlr_orig, cb_base + ARM_SMMU_CB_SCTLR);
		spin_unlock_irqrestore(&tbu->halt_lock, flags);
		return -ETIMEDOUT;
	}

	writel_relaxed(sctlr_orig, cb_base + ARM_SMMU_CB_SCTLR);
out:
	tbu->halt_count = 1;
	spin_unlock_irqrestore(&tbu->halt_lock, flags);
	return 0;
}

static void qsmmuv500_tbu_resume(struct qsmmuv500_tbu_device *tbu)
{
	unsigned long flags;
	u32 val;
	void __iomem *base;

	spin_lock_irqsave(&tbu->halt_lock, flags);
	if (!tbu->halt_count) {
		WARN(1, "%s: bad tbu->halt_count", dev_name(tbu->dev));
		spin_unlock_irqrestore(&tbu->halt_lock, flags);
		return;

	} else if (tbu->halt_count > 1) {
		tbu->halt_count--;
		spin_unlock_irqrestore(&tbu->halt_lock, flags);
		return;
	}

	base = tbu->base;
	val = readl_relaxed(base + DEBUG_SID_HALT_REG);
	val &= ~DEBUG_SID_HALT_VAL;
	writel_relaxed(val, base + DEBUG_SID_HALT_REG);

	tbu->halt_count = 0;
	spin_unlock_irqrestore(&tbu->halt_lock, flags);
}

static struct qsmmuv500_tbu_device *qsmmuv500_find_tbu(
	struct arm_smmu_device *smmu, u32 sid)
{
	struct qsmmuv500_tbu_device *tbu = NULL;
	struct qsmmuv500_archdata *data = get_qsmmuv500_archdata(smmu);

	list_for_each_entry(tbu, &data->tbus, list) {
		if (tbu->sid_start <= sid &&
		    sid < tbu->sid_start + tbu->num_sids)
			return tbu;
	}
	return NULL;
}

static int qsmmuv500_ecats_lock(struct arm_smmu_domain *smmu_domain,
				struct qsmmuv500_tbu_device *tbu,
				unsigned long *flags)
{
	struct arm_smmu_device *smmu = tbu->smmu;
	struct qsmmuv500_archdata *data = get_qsmmuv500_archdata(smmu);
	u32 val;

	spin_lock_irqsave(&smmu->atos_lock, *flags);
	/* The status register is not accessible on version 1.0 */
	if (data->version == 0x01000000)
		return 0;

	if (readl_poll_timeout_atomic(tbu->status_reg,
					val, (val == 0x1), 0,
					TBU_DBG_TIMEOUT_US)) {
		dev_err(tbu->dev, "ECATS hw busy!\n");
		spin_unlock_irqrestore(&smmu->atos_lock, *flags);
		return  -ETIMEDOUT;
	}

	return 0;
}

static void qsmmuv500_ecats_unlock(struct arm_smmu_domain *smmu_domain,
					struct qsmmuv500_tbu_device *tbu,
					unsigned long *flags)
{
	struct arm_smmu_device *smmu = tbu->smmu;
	struct qsmmuv500_archdata *data = get_qsmmuv500_archdata(smmu);

	/* The status register is not accessible on version 1.0 */
	if (data->version != 0x01000000)
		writel_relaxed(0, tbu->status_reg);
	spin_unlock_irqrestore(&smmu->atos_lock, *flags);
}

/*
 * Zero means failure.
 */
static phys_addr_t qsmmuv500_iova_to_phys(
		struct iommu_domain *domain, dma_addr_t iova, u32 sid)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct qsmmuv500_tbu_device *tbu;
	int ret;
	phys_addr_t phys = 0;
	u64 val, fsr;
	unsigned long flags;
	void __iomem *cb_base;
	u32 sctlr_orig, sctlr;
	int needs_redo = 0;
	ktime_t timeout;

	/* only 36 bit iova is supported */
	if (iova >= (1ULL << 36)) {
		dev_err_ratelimited(smmu->dev, "ECATS: address too large: %pad\n",
					&iova);
		return 0;
	}

	cb_base = ARM_SMMU_CB(smmu, cfg->cbndx);
	tbu = qsmmuv500_find_tbu(smmu, sid);
	if (!tbu)
		return 0;

	ret = arm_smmu_power_on(tbu->pwr);
	if (ret)
		return 0;

	ret = qsmmuv500_tbu_halt(tbu, smmu_domain);
	if (ret)
		goto out_power_off;

	/*
	 * ECATS can trigger the fault interrupt, so disable it temporarily
	 * and check for an interrupt manually.
	 */
	sctlr_orig = readl_relaxed(cb_base + ARM_SMMU_CB_SCTLR);
	sctlr = sctlr_orig & ~(SCTLR_CFCFG | SCTLR_CFIE);
	writel_relaxed(sctlr, cb_base + ARM_SMMU_CB_SCTLR);

	/* Only one concurrent atos operation */
	ret = qsmmuv500_ecats_lock(smmu_domain, tbu, &flags);
	if (ret)
		goto out_resume;

redo:
	/* Set address and stream-id */
	val = readq_relaxed(tbu->base + DEBUG_SID_HALT_REG);
	val &= ~DEBUG_SID_HALT_SID_MASK;
	val |= sid & DEBUG_SID_HALT_SID_MASK;
	writeq_relaxed(val, tbu->base + DEBUG_SID_HALT_REG);
	writeq_relaxed(iova, tbu->base + DEBUG_VA_ADDR_REG);
	val = (u64)(DEBUG_AXUSER_CDMID_VAL & DEBUG_AXUSER_CDMID_MASK) <<
		DEBUG_AXUSER_CDMID_SHIFT;
	writeq_relaxed(val, tbu->base + DEBUG_AXUSER_REG);

	/*
	 * Write-back Read and Write-Allocate
	 * Priviledged, nonsecure, data transaction
	 * Read operation.
	 */
	val = 0xF << DEBUG_TXN_AXCACHE_SHIFT;
	val |= 0x3 << DEBUG_TXN_AXPROT_SHIFT;
	val |= DEBUG_TXN_TRIGGER;
	writeq_relaxed(val, tbu->base + DEBUG_TXN_TRIGG_REG);

	ret = 0;
	timeout = ktime_add_us(ktime_get(), TBU_DBG_TIMEOUT_US);
	for (;;) {
		val = readl_relaxed(tbu->base + DEBUG_SR_HALT_ACK_REG);
		if (!(val & DEBUG_SR_ECATS_RUNNING_VAL))
			break;
		val = readl_relaxed(cb_base + ARM_SMMU_CB_FSR);
		if (val & FSR_FAULT)
			break;
		if (ktime_compare(ktime_get(), timeout) > 0) {
			dev_err(tbu->dev, "ECATS translation timed out!\n");
			ret = -ETIMEDOUT;
			break;
		}
	}

	val = readq_relaxed(tbu->base + DEBUG_PAR_REG);
	fsr = readl_relaxed(cb_base + ARM_SMMU_CB_FSR);
	if (fsr & FSR_FAULT) {
		dev_err(tbu->dev, "ECATS generated a fault interrupt! FSR = %llx, SID=0x%x\n",
				fsr, sid);

		/* Clear pending interrupts */
		writel_relaxed(fsr, cb_base + ARM_SMMU_CB_FSR);
		/*
		 * Barrier required to ensure that the FSR is cleared
		 * before resuming SMMU operation.
		 */
		wmb();
		writel_relaxed(RESUME_TERMINATE, cb_base + ARM_SMMU_CB_RESUME);

		/* Check if ECATS translation failed */
		if (val & DEBUG_PAR_FAULT_VAL)
			dev_err(tbu->dev, "ECATS translation failed! PAR = %llx\n",
					val);
		ret = -EINVAL;
	}

	phys = (val >> DEBUG_PAR_PA_SHIFT) & DEBUG_PAR_PA_MASK;
	if (ret < 0)
		phys = 0;

	/* Reset hardware */
	writeq_relaxed(0, tbu->base + DEBUG_TXN_TRIGG_REG);
	writeq_relaxed(0, tbu->base + DEBUG_VA_ADDR_REG);
	val = readl_relaxed(tbu->base + DEBUG_SID_HALT_REG);
	val &= ~DEBUG_SID_HALT_SID_MASK;
	writel_relaxed(val, tbu->base + DEBUG_SID_HALT_REG);

	/*
	 * After a failed translation, the next successful translation will
	 * incorrectly be reported as a failure.
	 */
	if (!phys && needs_redo++ < 2)
		goto redo;

	writel_relaxed(sctlr_orig, cb_base + ARM_SMMU_CB_SCTLR);
	qsmmuv500_ecats_unlock(smmu_domain, tbu, &flags);

out_resume:
	qsmmuv500_tbu_resume(tbu);

out_power_off:
	/* Read to complete prior write transcations */
	val = readl_relaxed(tbu->base + DEBUG_SR_HALT_ACK_REG);

	/* Wait for read to complete before off */
	rmb();

	arm_smmu_power_off(tbu->pwr);

	return phys;
}

static phys_addr_t qsmmuv500_iova_to_phys_hard(
		struct iommu_domain *domain, dma_addr_t iova)
{
	u16 sid;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct iommu_fwspec *fwspec;
	void __iomem *gr1_base;
	u32 frsynra;


	/* Check to see if the domain is associated with the test
	 * device. If the domain belongs to the test device, then
	 * pick the SID from fwspec.
	 */
	if (domain->is_debug_domain) {
		fwspec = smmu_domain->dev->iommu_fwspec;
		sid    = (u16)fwspec->ids[0];
	} else {

		/* If the domain belongs to an actual device, read
		 * SID from the corresponding frsynra register
		 */
		gr1_base = ARM_SMMU_GR1(smmu);
		frsynra  = readl_relaxed(gr1_base +
				ARM_SMMU_GR1_CBFRSYNRA(cfg->cbndx));
		frsynra &= CBFRSYNRA_SID_MASK;
		sid      = frsynra;
	}
	return qsmmuv500_iova_to_phys(domain, iova, sid);
}

static void qsmmuv500_release_group_iommudata(void *data)
{
	kfree(data);
}

/* If a device has a valid actlr, it must match */
static int qsmmuv500_device_group(struct device *dev,
				struct iommu_group *group)
{
	struct iommu_fwspec *fwspec = dev->iommu_fwspec;
	struct arm_smmu_device *smmu = fwspec_smmu(fwspec);
	struct qsmmuv500_archdata *data = get_qsmmuv500_archdata(smmu);
	struct qsmmuv500_group_iommudata *iommudata;
	u32 actlr, i;
	struct arm_smmu_smr *smr;

	iommudata = to_qsmmuv500_group_iommudata(group);
	if (!iommudata) {
		iommudata = kzalloc(sizeof(*iommudata), GFP_KERNEL);
		if (!iommudata)
			return -ENOMEM;

		iommu_group_set_iommudata(group, iommudata,
				qsmmuv500_release_group_iommudata);
	}

	for (i = 0; i < data->actlr_tbl_size; i++) {
		smr = &data->actlrs[i].smr;
		actlr = data->actlrs[i].actlr;

		if (!arm_smmu_fwspec_match_smr(fwspec, smr))
			continue;

		if (!iommudata->has_actlr) {
			iommudata->actlr = actlr;
			iommudata->has_actlr = true;
		} else if (iommudata->actlr != actlr) {
			return -EINVAL;
		}
	}

	return 0;
}

static void qsmmuv500_init_cb(struct arm_smmu_domain *smmu_domain,
				struct device *dev)
{
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cb *cb = &smmu->cbs[smmu_domain->cfg.cbndx];
	struct qsmmuv500_group_iommudata *iommudata =
		to_qsmmuv500_group_iommudata(dev->iommu_group);

	if (!iommudata->has_actlr)
		return;

	cb->actlr = iommudata->actlr;
	/*
	 * Prefetch only works properly if the start and end of all
	 * buffers in the page table are aligned to ARM_SMMU_MIN_IOVA_ALIGN.
	 */
	if (((iommudata->actlr >> QSMMUV500_ACTLR_DEEP_PREFETCH_SHIFT) &
			QSMMUV500_ACTLR_DEEP_PREFETCH_MASK) &&
				  (smmu->options & ARM_SMMU_OPT_MIN_IOVA_ALIGN))
		smmu_domain->qsmmuv500_errata1_min_iova_align = true;
}

static int qsmmuv500_tbu_register(struct device *dev, void *cookie)
{
	struct arm_smmu_device *smmu = cookie;
	struct qsmmuv500_tbu_device *tbu;
	struct qsmmuv500_archdata *data = get_qsmmuv500_archdata(smmu);

	if (!dev->driver) {
		dev_err(dev, "TBU failed probe, QSMMUV500 cannot continue!\n");
		return -EINVAL;
	}

	tbu = dev_get_drvdata(dev);

	INIT_LIST_HEAD(&tbu->list);
	tbu->smmu = smmu;
	list_add(&tbu->list, &data->tbus);
	return 0;
}

static int qsmmuv500_read_actlr_tbl(struct arm_smmu_device *smmu)
{
	int len, i;
	struct device *dev = smmu->dev;
	struct qsmmuv500_archdata *data = get_qsmmuv500_archdata(smmu);
	struct actlr_setting *actlrs;
	const __be32 *cell;

	cell = of_get_property(dev->of_node, "qcom,actlr", NULL);
	if (!cell)
		return 0;

	len = of_property_count_elems_of_size(dev->of_node, "qcom,actlr",
						sizeof(u32) * 3);
	if (len < 0)
		return 0;

	actlrs = devm_kzalloc(dev, sizeof(*actlrs) * len, GFP_KERNEL);
	if (!actlrs)
		return -ENOMEM;

	for (i = 0; i < len; i++) {
		actlrs[i].smr.id = of_read_number(cell++, 1);
		actlrs[i].smr.mask = of_read_number(cell++, 1);
		actlrs[i].actlr = of_read_number(cell++, 1);
	}

	data->actlrs = actlrs;
	data->actlr_tbl_size = len;
	return 0;
}

static int qsmmuv500_arch_init(struct arm_smmu_device *smmu)
{
	struct resource *res;
	struct device *dev = smmu->dev;
	struct qsmmuv500_archdata *data;
	struct platform_device *pdev;
	int ret;
	u32 val;
	void __iomem *reg;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	INIT_LIST_HEAD(&data->tbus);

	pdev = container_of(dev, struct platform_device, dev);
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "tcu-base");
	if (!res) {
		dev_err(dev, "Unable to get the tcu-base\n");
		return -EINVAL;
	}
	data->tcu_base = devm_ioremap(dev, res->start, resource_size(res));
	if (IS_ERR(data->tcu_base))
		return PTR_ERR(data->tcu_base);

	data->version = readl_relaxed(data->tcu_base + TCU_HW_VERSION_HLOS1);
	smmu->archdata = data;

	if (arm_smmu_is_static_cb(smmu))
		return 0;

	ret = qsmmuv500_read_actlr_tbl(smmu);
	if (ret)
		return ret;

	reg = ARM_SMMU_GR0(smmu);
	val = readl_relaxed(reg + ARM_SMMU_GR0_sACR);
	val &= ~ARM_MMU500_ACR_CACHE_LOCK;
	writel_relaxed(val, reg + ARM_SMMU_GR0_sACR);
	val = readl_relaxed(reg + ARM_SMMU_GR0_sACR);
	/*
	 * Modifiying the nonsecure copy of the sACR register is only
	 * allowed if permission is given in the secure sACR register.
	 * Attempt to detect if we were able to update the value.
	 */
	WARN_ON(val & ARM_MMU500_ACR_CACHE_LOCK);

	ret = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (ret)
		return ret;

	/* Attempt to register child devices */
	ret = device_for_each_child(dev, smmu, qsmmuv500_tbu_register);
	if (ret)
		return -EPROBE_DEFER;

	return 0;
}

struct arm_smmu_arch_ops qsmmuv500_arch_ops = {
	.init = qsmmuv500_arch_init,
	.iova_to_phys_hard = qsmmuv500_iova_to_phys_hard,
	.init_context_bank = qsmmuv500_init_cb,
	.device_group = qsmmuv500_device_group,
};

static const struct of_device_id qsmmuv500_tbu_of_match[] = {
	{.compatible = "qcom,qsmmuv500-tbu"},
	{}
};

static int qsmmuv500_tbu_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct qsmmuv500_tbu_device *tbu;
	const __be32 *cell;
	int len;

	tbu = devm_kzalloc(dev, sizeof(*tbu), GFP_KERNEL);
	if (!tbu)
		return -ENOMEM;

	INIT_LIST_HEAD(&tbu->list);
	tbu->dev = dev;
	spin_lock_init(&tbu->halt_lock);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	tbu->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(tbu->base))
		return PTR_ERR(tbu->base);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "status-reg");
	tbu->status_reg = devm_ioremap_resource(dev, res);
	if (IS_ERR(tbu->status_reg))
		return PTR_ERR(tbu->status_reg);

	cell = of_get_property(dev->of_node, "qcom,stream-id-range", &len);
	if (!cell || len < 8)
		return -EINVAL;

	tbu->sid_start = of_read_number(cell, 1);
	tbu->num_sids = of_read_number(cell + 1, 1);

	tbu->pwr = arm_smmu_init_power_resources(pdev);
	if (IS_ERR(tbu->pwr))
		return PTR_ERR(tbu->pwr);

	dev_set_drvdata(dev, tbu);
	return 0;
}

static struct platform_driver qsmmuv500_tbu_driver = {
	.driver	= {
		.name		= "qsmmuv500-tbu",
		.of_match_table	= of_match_ptr(qsmmuv500_tbu_of_match),
	},
	.probe	= qsmmuv500_tbu_probe,
};

MODULE_DESCRIPTION("IOMMU API for ARM architected SMMU implementations");
MODULE_AUTHOR("Will Deacon <will.deacon@arm.com>");
MODULE_LICENSE("GPL v2");
