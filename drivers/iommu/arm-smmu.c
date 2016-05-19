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
 */

#define pr_fmt(fmt) "arm-smmu: " fmt

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>

#include <linux/amba/bus.h>
#include <soc/qcom/msm_tz_smmu.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/msm_pcie.h>
#include <asm/cacheflush.h>
#include <linux/msm-bus.h>
#include <dt-bindings/msm/msm-bus-ids.h>

#include "io-pgtable.h"

/* Maximum number of stream IDs assigned to a single device */
#define MAX_MASTER_STREAMIDS		45

/* Maximum number of context banks per SMMU */
#define ARM_SMMU_MAX_CBS		128

/* Maximum number of mapping groups per SMMU */
#define ARM_SMMU_MAX_SMRS		128

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

/* Configuration registers */
#define ARM_SMMU_GR0_sCR0		0x0
#define sCR0_CLIENTPD			(1 << 0)
#define sCR0_GFRE			(1 << 1)
#define sCR0_GFIE			(1 << 2)
#define sCR0_GCFGFRE			(1 << 4)
#define sCR0_GCFGFIE			(1 << 5)
#define sCR0_USFCFG			(1 << 10)
#define sCR0_VMIDPNE			(1 << 11)
#define sCR0_PTM			(1 << 12)
#define sCR0_FB				(1 << 13)
#define sCR0_BSU_SHIFT			14
#define sCR0_BSU_MASK			0x3

/* Identification registers */
#define ARM_SMMU_GR0_ID0		0x20
#define ARM_SMMU_GR0_ID1		0x24
#define ARM_SMMU_GR0_ID2		0x28
#define ARM_SMMU_GR0_ID3		0x2c
#define ARM_SMMU_GR0_ID4		0x30
#define ARM_SMMU_GR0_ID5		0x34
#define ARM_SMMU_GR0_ID6		0x38
#define ARM_SMMU_GR0_ID7		0x3c
#define ARM_SMMU_GR0_sGFSR		0x48
#define ARM_SMMU_GR0_sGFSYNR0		0x50
#define ARM_SMMU_GR0_sGFSYNR1		0x54
#define ARM_SMMU_GR0_sGFSYNR2		0x58

#define ID0_S1TS			(1 << 30)
#define ID0_S2TS			(1 << 29)
#define ID0_NTS				(1 << 28)
#define ID0_SMS				(1 << 27)
#define ID0_ATOSNS			(1 << 26)
#define ID0_CTTW			(1 << 14)
#define ID0_NUMIRPT_SHIFT		16
#define ID0_NUMIRPT_MASK		0xff
#define ID0_NUMSIDB_SHIFT		9
#define ID0_NUMSIDB_MASK		0xf
#define ID0_NUMSMRG_SHIFT		0
#define ID0_NUMSMRG_MASK		0xff

#define ID1_PAGESIZE			(1 << 31)
#define ID1_NUMPAGENDXB_SHIFT		28
#define ID1_NUMPAGENDXB_MASK		7
#define ID1_NUMS2CB_SHIFT		16
#define ID1_NUMS2CB_MASK		0xff
#define ID1_NUMCB_SHIFT			0
#define ID1_NUMCB_MASK			0xff

#define ID2_OAS_SHIFT			4
#define ID2_OAS_MASK			0xf
#define ID2_IAS_SHIFT			0
#define ID2_IAS_MASK			0xf
#define ID2_UBS_SHIFT			8
#define ID2_UBS_MASK			0xf
#define ID2_PTFS_4K			(1 << 12)
#define ID2_PTFS_16K			(1 << 13)
#define ID2_PTFS_64K			(1 << 14)

/* Global TLB invalidation */
#define ARM_SMMU_GR0_TLBIVMID		0x64
#define ARM_SMMU_GR0_TLBIALLNSNH	0x68
#define ARM_SMMU_GR0_TLBIALLH		0x6c
#define ARM_SMMU_GR0_sTLBGSYNC		0x70
#define ARM_SMMU_GR0_sTLBGSTATUS	0x74
#define sTLBGSTATUS_GSACTIVE		(1 << 0)
#define TLB_LOOP_TIMEOUT		500000	/* 500ms */

/* Stream mapping registers */
#define ARM_SMMU_GR0_SMR(n)		(0x800 + ((n) << 2))
#define SMR_VALID			(1 << 31)
#define SMR_MASK_SHIFT			16
#define SMR_MASK_MASK			0x7fff
#define SMR_ID_SHIFT			0
#define SMR_ID_MASK			0x7fff

#define ARM_SMMU_GR0_S2CR(n)		(0xc00 + ((n) << 2))
#define S2CR_CBNDX_SHIFT		0
#define S2CR_CBNDX_MASK			0xff
#define S2CR_TYPE_SHIFT			16
#define S2CR_TYPE_MASK			0x3
#define S2CR_TYPE_TRANS			(0 << S2CR_TYPE_SHIFT)
#define S2CR_TYPE_BYPASS		(1 << S2CR_TYPE_SHIFT)
#define S2CR_TYPE_FAULT			(2 << S2CR_TYPE_SHIFT)

/* Context bank attribute registers */
#define ARM_SMMU_GR1_CBAR(n)		(0x0 + ((n) << 2))
#define CBAR_VMID_SHIFT			0
#define CBAR_VMID_MASK			0xff
#define CBAR_S1_BPSHCFG_SHIFT		8
#define CBAR_S1_BPSHCFG_MASK		3
#define CBAR_S1_BPSHCFG_NSH		3
#define CBAR_S1_MEMATTR_SHIFT		12
#define CBAR_S1_MEMATTR_MASK		0xf
#define CBAR_S1_MEMATTR_WB		0xf
#define CBAR_TYPE_SHIFT			16
#define CBAR_TYPE_MASK			0x3
#define CBAR_TYPE_S2_TRANS		(0 << CBAR_TYPE_SHIFT)
#define CBAR_TYPE_S1_TRANS_S2_BYPASS	(1 << CBAR_TYPE_SHIFT)
#define CBAR_TYPE_S1_TRANS_S2_FAULT	(2 << CBAR_TYPE_SHIFT)
#define CBAR_TYPE_S1_TRANS_S2_TRANS	(3 << CBAR_TYPE_SHIFT)
#define CBAR_IRPTNDX_SHIFT		24
#define CBAR_IRPTNDX_MASK		0xff

#define ARM_SMMU_GR1_CBA2R(n)		(0x800 + ((n) << 2))
#define CBA2R_RW64_32BIT		(0 << 0)
#define CBA2R_RW64_64BIT		(1 << 0)

/* Translation context bank */
#define ARM_SMMU_CB_BASE(smmu)		((smmu)->base + ((smmu)->size >> 1))
#define ARM_SMMU_CB(smmu, n)		((n) * (1 << (smmu)->pgshift))

#define ARM_SMMU_CB_SCTLR		0x0
#define ARM_SMMU_CB_ACTLR		0x4
#define ARM_SMMU_CB_RESUME		0x8
#define ARM_SMMU_CB_TTBCR2		0x10
#define ARM_SMMU_CB_TTBR0_LO		0x20
#define ARM_SMMU_CB_TTBR0_HI		0x24
#define ARM_SMMU_CB_TTBR1_LO		0x28
#define ARM_SMMU_CB_TTBR1_HI		0x2c
#define ARM_SMMU_CB_TTBCR		0x30
#define ARM_SMMU_CB_CONTEXTIDR		0x34
#define ARM_SMMU_CB_S1_MAIR0		0x38
#define ARM_SMMU_CB_S1_MAIR1		0x3c
#define ARM_SMMU_CB_PAR_LO		0x50
#define ARM_SMMU_CB_PAR_HI		0x54
#define ARM_SMMU_CB_FSR			0x58
#define ARM_SMMU_CB_FSRRESTORE		0x5c
#define ARM_SMMU_CB_FAR_LO		0x60
#define ARM_SMMU_CB_FAR_HI		0x64
#define ARM_SMMU_CB_FSYNR0		0x68
#define ARM_SMMU_CB_S1_TLBIVA		0x600
#define ARM_SMMU_CB_S1_TLBIASID		0x610
#define ARM_SMMU_CB_S1_TLBIALL		0x618
#define ARM_SMMU_CB_S1_TLBIVAL		0x620
#define ARM_SMMU_CB_S2_TLBIIPAS2	0x630
#define ARM_SMMU_CB_S2_TLBIIPAS2L	0x638
#define ARM_SMMU_CB_TLBSYNC		0x7f0
#define ARM_SMMU_CB_TLBSTATUS		0x7f4
#define TLBSTATUS_SACTIVE		(1 << 0)
#define ARM_SMMU_CB_ATS1PR_LO		0x800
#define ARM_SMMU_CB_ATS1PR_HI		0x804
#define ARM_SMMU_CB_ATSR		0x8f0
#define ARM_SMMU_GR1_CBFRSYNRA(n)	(0x400 + ((n) << 2))

#define SCTLR_S1_ASIDPNE		(1 << 12)
#define SCTLR_CFCFG			(1 << 7)
#define SCTLR_CFIE			(1 << 6)
#define SCTLR_CFRE			(1 << 5)
#define SCTLR_E				(1 << 4)
#define SCTLR_AFE			(1 << 2)
#define SCTLR_TRE			(1 << 1)
#define SCTLR_M				(1 << 0)
#define SCTLR_EAE_SBOP			(SCTLR_AFE | SCTLR_TRE)

#define CB_PAR_F			(1 << 0)

#define ATSR_ACTIVE			(1 << 0)

#define RESUME_RETRY			(0 << 0)
#define RESUME_TERMINATE		(1 << 0)

#define TTBCR2_SEP_SHIFT		15
#define TTBCR2_SEP_MASK			0x7

#define TTBCR2_ADDR_32			0
#define TTBCR2_ADDR_36			1
#define TTBCR2_ADDR_40			2
#define TTBCR2_ADDR_42			3
#define TTBCR2_ADDR_44			4
#define TTBCR2_ADDR_48			5

#define TTBCR2_SEP_31			0
#define TTBCR2_SEP_35			1
#define TTBCR2_SEP_39			2
#define TTBCR2_SEP_41			3
#define TTBCR2_SEP_43			4
#define TTBCR2_SEP_47			5
#define TTBCR2_SEP_NOSIGN		7

#define TTBRn_HI_ASID_SHIFT            16

#define FSR_MULTI			(1 << 31)
#define FSR_SS				(1 << 30)
#define FSR_UUT				(1 << 8)
#define FSR_ASF				(1 << 7)
#define FSR_TLBLKF			(1 << 6)
#define FSR_TLBMCF			(1 << 5)
#define FSR_EF				(1 << 4)
#define FSR_PF				(1 << 3)
#define FSR_AFF				(1 << 2)
#define FSR_TF				(1 << 1)

/* Definitions for implementation-defined registers */
#define ACTLR_QCOM_OSH_SHIFT		28
#define ACTLR_QCOM_OSH			1

#define ACTLR_QCOM_ISH_SHIFT		29
#define ACTLR_QCOM_ISH			1

#define ACTLR_QCOM_NSH_SHIFT		30
#define ACTLR_QCOM_NSH			1

#define ARM_SMMU_IMPL_DEF0(smmu) \
	((smmu)->base + (2 * (1 << (smmu)->pgshift)))
#define ARM_SMMU_IMPL_DEF1(smmu) \
	((smmu)->base + (6 * (1 << (smmu)->pgshift)))
#define IMPL_DEF1_MICRO_MMU_CTRL	0
#define MICRO_MMU_CTRL_LOCAL_HALT_REQ	(1 << 2)
#define MICRO_MMU_CTRL_IDLE		(1 << 3)

#define FSR_IGN				(FSR_AFF | FSR_ASF | \
					 FSR_TLBMCF | FSR_TLBLKF)
#define FSR_FAULT			(FSR_MULTI | FSR_SS | FSR_UUT | \
					 FSR_EF | FSR_PF | FSR_TF | FSR_IGN)

#define FSYNR0_WNR			(1 << 4)

static int force_stage;
module_param_named(force_stage, force_stage, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(force_stage,
	"Force SMMU mappings to be installed at a particular stage of translation. A value of '1' or '2' forces the corresponding stage. All other values are ignored (i.e. no stage is forced). Note that selecting a specific stage will disable support for nested translation.");

enum arm_smmu_arch_version {
	ARM_SMMU_V1 = 1,
	ARM_SMMU_V2,
};

struct arm_smmu_smr {
	u8				idx;
	u16				mask;
	u16				id;
};

struct arm_smmu_master_cfg {
	int				num_streamids;
	u16				streamids[MAX_MASTER_STREAMIDS];
	struct arm_smmu_smr		*smrs;
};

struct arm_smmu_master {
	struct device_node		*of_node;
	struct rb_node			node;
	struct arm_smmu_master_cfg	cfg;
};

enum smmu_model_id {
	SMMU_MODEL_DEFAULT,
	SMMU_MODEL_QCOM_V2,
};

struct arm_smmu_impl_def_reg {
	u32 offset;
	u32 value;
};

struct arm_smmu_device {
	struct device			*dev;

	enum smmu_model_id		model;

	void __iomem			*base;
	unsigned long			size;
	phys_addr_t			phys_addr;
	unsigned long			pgshift;

#define ARM_SMMU_FEAT_COHERENT_WALK	(1 << 0)
#define ARM_SMMU_FEAT_STREAM_MATCH	(1 << 1)
#define ARM_SMMU_FEAT_TRANS_S1		(1 << 2)
#define ARM_SMMU_FEAT_TRANS_S2		(1 << 3)
#define ARM_SMMU_FEAT_TRANS_NESTED	(1 << 4)
#define ARM_SMMU_FEAT_TRANS_OPS		(1 << 5)
	u32				features;

#define ARM_SMMU_OPT_SECURE_CFG_ACCESS (1 << 0)
#define ARM_SMMU_OPT_INVALIDATE_ON_MAP (1 << 1)
#define ARM_SMMU_OPT_HALT_AND_TLB_ON_ATOS  (1 << 2)
#define ARM_SMMU_OPT_REGISTER_SAVE	(1 << 3)
#define ARM_SMMU_OPT_SKIP_INIT		(1 << 4)
#define ARM_SMMU_OPT_ERRATA_CTX_FAULT_HANG (1 << 5)
#define ARM_SMMU_OPT_FATAL_ASF		(1 << 6)
#define ARM_SMMU_OPT_ERRATA_TZ_ATOS	(1 << 7)
#define ARM_SMMU_OPT_NO_SMR_CHECK	(1 << 9)
#define ARM_SMMU_OPT_DYNAMIC		(1 << 10)
#define ARM_SMMU_OPT_HALT		(1 << 11)
#define ARM_SMMU_OPT_STATIC_CB		(1 << 12)
	u32				options;
	enum arm_smmu_arch_version	version;

	u32				num_context_banks;
	u32				num_s2_context_banks;
	DECLARE_BITMAP(context_map, ARM_SMMU_MAX_CBS);
	atomic_t			irptndx;

	u32				num_mapping_groups;
	DECLARE_BITMAP(smr_map, ARM_SMMU_MAX_SMRS);

	unsigned long			va_size;
	unsigned long			ipa_size;
	unsigned long			pa_size;

	u32				num_global_irqs;
	u32				num_context_irqs;
	unsigned int			*irqs;

	struct list_head		list;
	struct rb_root			masters;

	int				num_clocks;
	struct clk			**clocks;

	struct regulator		*gdsc;
	struct notifier_block		regulator_nb;

	/* Protects against domains attaching to the same SMMU concurrently */
	struct mutex			attach_lock;
	unsigned int			attach_count;
	struct idr			asid_idr;

	struct arm_smmu_impl_def_reg	*impl_def_attach_registers;
	unsigned int			num_impl_def_attach_registers;

	spinlock_t			atos_lock;
	unsigned int			clock_refs_count;
	spinlock_t			clock_refs_lock;

	struct msm_bus_client_handle	*bus_client;
	char				*bus_client_name;

	enum tz_smmu_device_id		sec_id;
};

struct arm_smmu_cfg {
	u8				cbndx;
	u8				irptndx;
	u32				cbar;
	u32				procid;
	u16				asid;
	u8				vmid;
};
#define INVALID_IRPTNDX			0xff
#define INVALID_CBNDX			0xff
#define INVALID_ASID			0xffff
#define INVALID_VMID			0xff
/*
 * In V7L and V8L with TTBCR2.AS == 0, ASID is 8 bits.
 * V8L 16 with TTBCR2.AS == 1 (16 bit ASID) isn't supported yet.
 */
#define MAX_ASID			0xff

#define ARM_SMMU_CB_ASID(cfg)		((cfg)->asid)
#define ARM_SMMU_CB_VMID(cfg)		((cfg)->vmid)

enum arm_smmu_domain_stage {
	ARM_SMMU_DOMAIN_S1 = 0,
	ARM_SMMU_DOMAIN_S2,
	ARM_SMMU_DOMAIN_NESTED,
};

struct arm_smmu_pte_info {
	void *virt_addr;
	size_t size;
	struct list_head entry;
};

struct arm_smmu_domain {
	struct arm_smmu_device		*smmu;
	struct io_pgtable_ops		*pgtbl_ops;
	struct io_pgtable_cfg		pgtbl_cfg;
	spinlock_t			pgtbl_spin_lock;
	struct mutex			pgtbl_mutex_lock;
	struct arm_smmu_cfg		cfg;
	enum arm_smmu_domain_stage	stage;
	struct mutex			init_mutex; /* Protects smmu pointer */
	u32				attributes;
	bool				slave_side_secure;
	u32				secure_vmid;
	struct list_head		pte_info_list;
	struct list_head		unassign_list;
	struct mutex			assign_lock;
	struct list_head		secure_pool_list;
	bool				non_fatal_faults;
};

static struct iommu_ops arm_smmu_ops;

static DEFINE_SPINLOCK(arm_smmu_devices_lock);
static LIST_HEAD(arm_smmu_devices);

struct arm_smmu_option_prop {
	u32 opt;
	const char *prop;
};

static struct arm_smmu_option_prop arm_smmu_options[] = {
	{ ARM_SMMU_OPT_SECURE_CFG_ACCESS, "calxeda,smmu-secure-config-access" },
	{ ARM_SMMU_OPT_INVALIDATE_ON_MAP, "qcom,smmu-invalidate-on-map" },
	{ ARM_SMMU_OPT_HALT_AND_TLB_ON_ATOS, "qcom,halt-and-tlb-on-atos" },
	{ ARM_SMMU_OPT_REGISTER_SAVE, "qcom,register-save" },
	{ ARM_SMMU_OPT_SKIP_INIT, "qcom,skip-init" },
	{ ARM_SMMU_OPT_ERRATA_CTX_FAULT_HANG, "qcom,errata-ctx-fault-hang" },
	{ ARM_SMMU_OPT_FATAL_ASF, "qcom,fatal-asf" },
	{ ARM_SMMU_OPT_ERRATA_TZ_ATOS, "qcom,errata-tz-atos" },
	{ ARM_SMMU_OPT_NO_SMR_CHECK, "qcom,no-smr-check" },
	{ ARM_SMMU_OPT_DYNAMIC, "qcom,dynamic" },
	{ ARM_SMMU_OPT_HALT, "qcom,enable-smmu-halt"},
	{ ARM_SMMU_OPT_STATIC_CB, "qcom,enable-static-cb"},
	{ 0, NULL},
};

static int arm_smmu_enable_clocks_atomic(struct arm_smmu_device *smmu);
static void arm_smmu_disable_clocks_atomic(struct arm_smmu_device *smmu);
static void arm_smmu_prepare_pgtable(void *addr, void *cookie);
static void arm_smmu_unprepare_pgtable(void *cookie, void *addr, size_t size);
static phys_addr_t arm_smmu_iova_to_phys_hard(struct iommu_domain *domain,
					      dma_addr_t iova);
static phys_addr_t arm_smmu_iova_to_phys_hard_no_halt(
	struct iommu_domain *domain, dma_addr_t iova);
static int arm_smmu_wait_for_halt(struct arm_smmu_device *smmu);
static int arm_smmu_halt_nowait(struct arm_smmu_device *smmu);
static void arm_smmu_resume(struct arm_smmu_device *smmu);
static phys_addr_t arm_smmu_iova_to_phys(struct iommu_domain *domain,
					dma_addr_t iova);
static int arm_smmu_assign_table(struct arm_smmu_domain *smmu_domain);
static void arm_smmu_unassign_table(struct arm_smmu_domain *smmu_domain);
static int arm_smmu_halt(struct arm_smmu_device *smmu);
static void arm_smmu_device_reset(struct arm_smmu_device *smmu);
static size_t arm_smmu_unmap(struct iommu_domain *domain, unsigned long iova,
			     size_t size);
static bool arm_smmu_is_master_side_secure(struct arm_smmu_domain *smmu_domain);
static bool arm_smmu_is_static_cb(struct arm_smmu_device *smmu);
static bool arm_smmu_is_slave_side_secure(struct arm_smmu_domain *smmu_domain);
static bool arm_smmu_has_secure_vmid(struct arm_smmu_domain *smmu_domain);

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

static struct device_node *dev_get_dev_node(struct device *dev)
{
	if (dev_is_pci(dev)) {
		struct pci_bus *bus = to_pci_dev(dev)->bus;

		while (!pci_is_root_bus(bus))
			bus = bus->parent;
		return bus->bridge->parent->of_node;
	}

	return dev->of_node;
}

static struct arm_smmu_master *find_smmu_master(struct arm_smmu_device *smmu,
						struct device_node *dev_node)
{
	struct rb_node *node = smmu->masters.rb_node;

	while (node) {
		struct arm_smmu_master *master;

		master = container_of(node, struct arm_smmu_master, node);

		if (dev_node < master->of_node)
			node = node->rb_left;
		else if (dev_node > master->of_node)
			node = node->rb_right;
		else
			return master;
	}

	return NULL;
}

static struct arm_smmu_master_cfg *
find_smmu_master_cfg(struct device *dev)
{
	struct arm_smmu_master_cfg *cfg = NULL;
	struct iommu_group *group = iommu_group_get(dev);

	if (group) {
		cfg = iommu_group_get_iommudata(group);
		iommu_group_put(group);
	}

	return cfg;
}

static int insert_smmu_master(struct arm_smmu_device *smmu,
			      struct arm_smmu_master *master)
{
	struct rb_node **new, *parent;

	new = &smmu->masters.rb_node;
	parent = NULL;
	while (*new) {
		struct arm_smmu_master *this
			= container_of(*new, struct arm_smmu_master, node);

		parent = *new;
		if (master->of_node < this->of_node)
			new = &((*new)->rb_left);
		else if (master->of_node > this->of_node)
			new = &((*new)->rb_right);
		else
			return -EEXIST;
	}

	rb_link_node(&master->node, parent, new);
	rb_insert_color(&master->node, &smmu->masters);
	return 0;
}

struct iommus_entry {
	struct list_head list;
	struct device_node *node;
	u16 streamids[MAX_MASTER_STREAMIDS];
	int num_sids;
};

static int register_smmu_master(struct arm_smmu_device *smmu,
				struct iommus_entry *entry)
{
	int i;
	struct arm_smmu_master *master;
	struct device *dev = smmu->dev;

	master = find_smmu_master(smmu, entry->node);
	if (master) {
		dev_err(dev,
			"rejecting multiple registrations for master device %s\n",
			entry->node->name);
		return -EBUSY;
	}

	if (entry->num_sids > MAX_MASTER_STREAMIDS) {
		dev_err(dev,
			"reached maximum number (%d) of stream IDs for master device %s\n",
			MAX_MASTER_STREAMIDS, entry->node->name);
		return -ENOSPC;
	}

	master = devm_kzalloc(dev, sizeof(*master), GFP_KERNEL);
	if (!master)
		return -ENOMEM;

	master->of_node			= entry->node;
	master->cfg.num_streamids	= entry->num_sids;

	for (i = 0; i < master->cfg.num_streamids; ++i)
		master->cfg.streamids[i] = entry->streamids[i];

	return insert_smmu_master(smmu, master);
}

static int arm_smmu_parse_iommus_properties(struct arm_smmu_device *smmu,
					int *num_masters)
{
	struct of_phandle_args iommuspec;
	struct device_node *master;

	*num_masters = 0;

	for_each_node_with_property(master, "iommus") {
		int arg_ind = 0;
		struct iommus_entry *entry, *n;
		LIST_HEAD(iommus);

		while (!of_parse_phandle_with_args(
				master, "iommus", "#iommu-cells",
				arg_ind, &iommuspec)) {
			if (iommuspec.np != smmu->dev->of_node) {
				arg_ind++;
				continue;
			}

			list_for_each_entry(entry, &iommus, list)
				if (entry->node == master)
					break;
			if (&entry->list == &iommus) {
				entry = devm_kzalloc(smmu->dev, sizeof(*entry),
						GFP_KERNEL);
				if (!entry)
					return -ENOMEM;
				entry->node = master;
				list_add(&entry->list, &iommus);
			}
			switch (iommuspec.args_count) {
			case 0:
				/*
				 * For pci-e devices the SIDs are provided
				 * at device attach time.
				 */
				break;
			case 1:
				entry->num_sids++;
				entry->streamids[entry->num_sids - 1]
					= iommuspec.args[0];
				break;
			default:
				BUG();
			}
			arg_ind++;
		}

		list_for_each_entry_safe(entry, n, &iommus, list) {
			int rc = register_smmu_master(smmu, entry);
			if (rc) {
				dev_err(smmu->dev, "Couldn't register %s\n",
					entry->node->name);
			} else {
				(*num_masters)++;
			}
			list_del(&entry->list);
			devm_kfree(smmu->dev, entry);
		}
	}

	return 0;
}

static struct arm_smmu_device *find_smmu_for_device(struct device *dev)
{
	struct arm_smmu_device *smmu;
	struct arm_smmu_master *master = NULL;
	struct device_node *dev_node = dev_get_dev_node(dev);

	spin_lock(&arm_smmu_devices_lock);
	list_for_each_entry(smmu, &arm_smmu_devices, list) {
		master = find_smmu_master(smmu, dev_node);
		if (master)
			break;
	}
	spin_unlock(&arm_smmu_devices_lock);

	return master ? smmu : NULL;
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

static void arm_smmu_unprepare_clocks(struct arm_smmu_device *smmu)
{
	int i;

	for (i = 0; i < smmu->num_clocks; ++i)
		clk_unprepare(smmu->clocks[i]);
}

static int arm_smmu_prepare_clocks(struct arm_smmu_device *smmu)
{
	int i, ret = 0;

	for (i = 0; i < smmu->num_clocks; ++i) {
		ret = clk_prepare(smmu->clocks[i]);
		if (ret) {
			dev_err(smmu->dev, "Couldn't prepare clock #%d\n", i);
			while (i--)
				clk_unprepare(smmu->clocks[i]);
			break;
		}
	}
	return ret;
}

static int arm_smmu_request_bus(struct arm_smmu_device *smmu)
{
	if (!smmu->bus_client)
		return 0;
	return msm_bus_scale_update_bw(smmu->bus_client, 0, 1000);
}

static int arm_smmu_unrequest_bus(struct arm_smmu_device *smmu)
{
	if (!smmu->bus_client)
		return 0;
	return msm_bus_scale_update_bw(smmu->bus_client, 0, 0);
}

static int arm_smmu_disable_regulators(struct arm_smmu_device *smmu)
{
	arm_smmu_unprepare_clocks(smmu);
	arm_smmu_unrequest_bus(smmu);
	if (!smmu->gdsc)
		return 0;
	return regulator_disable(smmu->gdsc);
}

static int arm_smmu_enable_regulators(struct arm_smmu_device *smmu)
{
	int ret;

	if (smmu->gdsc) {
		ret = regulator_enable(smmu->gdsc);
		if (WARN_ON_ONCE(ret))
			goto out;
	}

	ret = arm_smmu_request_bus(smmu);
	if (WARN_ON_ONCE(ret))
		goto out_reg;

	ret = arm_smmu_prepare_clocks(smmu);
	if (WARN_ON_ONCE(ret))
		goto out_bus;

	return ret;

out_bus:
	arm_smmu_unrequest_bus(smmu);
out_reg:
	if (smmu->gdsc)
		regulator_disable(smmu->gdsc);
out:
	return ret;
}

static int arm_smmu_enable_clocks(struct arm_smmu_device *smmu)
{
	int ret = 0;

	ret = arm_smmu_enable_regulators(smmu);
	if (unlikely(ret))
		return ret;
	ret = arm_smmu_enable_clocks_atomic(smmu);
	if (unlikely(ret))
		arm_smmu_disable_regulators(smmu);

	return ret;
}

static void arm_smmu_disable_clocks(struct arm_smmu_device *smmu)
{
	arm_smmu_disable_clocks_atomic(smmu);
	arm_smmu_disable_regulators(smmu);
}

/* Clocks must be prepared before this (arm_smmu_prepare_clocks) */
static int arm_smmu_enable_clocks_atomic(struct arm_smmu_device *smmu)
{
	int i, ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&smmu->clock_refs_lock, flags);
	if (smmu->clock_refs_count++ > 0) {
		spin_unlock_irqrestore(&smmu->clock_refs_lock, flags);
		return 0;
	}

	for (i = 0; i < smmu->num_clocks; ++i) {
		ret = clk_enable(smmu->clocks[i]);
		if (WARN_ON_ONCE(ret)) {
			dev_err(smmu->dev, "Couldn't enable clock #%d\n", i);
			while (i--)
				clk_disable(smmu->clocks[i]);
			smmu->clock_refs_count--;
			break;
		}
	}
	spin_unlock_irqrestore(&smmu->clock_refs_lock, flags);
	return ret;
}

/* Clocks should be unprepared after this (arm_smmu_unprepare_clocks) */
static void arm_smmu_disable_clocks_atomic(struct arm_smmu_device *smmu)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&smmu->clock_refs_lock, flags);
	if (smmu->clock_refs_count-- > 1) {
		spin_unlock_irqrestore(&smmu->clock_refs_lock, flags);
		return;
	}

	for (i = 0; i < smmu->num_clocks; ++i)
		clk_disable(smmu->clocks[i]);
	spin_unlock_irqrestore(&smmu->clock_refs_lock, flags);
}

/* Wait for any pending TLB invalidations to complete */
static void __arm_smmu_tlb_sync(struct arm_smmu_device *smmu)
{
	int count = 0;
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);

	writel_relaxed(0, gr0_base + ARM_SMMU_GR0_sTLBGSYNC);
	while (readl_relaxed(gr0_base + ARM_SMMU_GR0_sTLBGSTATUS)
	       & sTLBGSTATUS_GSACTIVE) {
		cpu_relax();
		if (++count == TLB_LOOP_TIMEOUT) {
			dev_err_ratelimited(smmu->dev,
			"TLB sync timed out -- SMMU may be deadlocked\n");
			return;
		}
		udelay(1);
	}
}

static void arm_smmu_tlb_sync_cb(struct arm_smmu_device *smmu,
				int cbndx)
{
	void __iomem *base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cbndx);
	u32 val;

	writel_relaxed(0, base + ARM_SMMU_CB_TLBSYNC);
	if (readl_poll_timeout_atomic(base + ARM_SMMU_CB_TLBSTATUS, val,
				      !(val & TLBSTATUS_SACTIVE),
				      0, TLB_LOOP_TIMEOUT))
		dev_err(smmu->dev, "TLBSYNC timeout!\n");
}

static void arm_smmu_tlb_sync(void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;

	if (smmu_domain->smmu == NULL)
		return;

	arm_smmu_tlb_sync_cb(smmu_domain->smmu, smmu_domain->cfg.cbndx);
}

/* Must be called with clocks/regulators enabled */
static void arm_smmu_tlb_inv_context(void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	bool stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;
	void __iomem *base;

	if (!smmu)
		return;

	if (stage1) {
		base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);
		writel_relaxed(ARM_SMMU_CB_ASID(cfg),
			       base + ARM_SMMU_CB_S1_TLBIASID);
		arm_smmu_tlb_sync_cb(smmu, cfg->cbndx);
	} else {
		base = ARM_SMMU_GR0(smmu);
		writel_relaxed(ARM_SMMU_CB_VMID(cfg),
			       base + ARM_SMMU_GR0_TLBIVMID);
		__arm_smmu_tlb_sync(smmu);
	}
}

/* Must be called with clocks/regulators enabled */
static void arm_smmu_tlb_inv_range_nosync(unsigned long iova, size_t size,
					  bool leaf, void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	bool stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;
	void __iomem *reg;
	int atomic_ctx = smmu_domain->attributes & (1 << DOMAIN_ATTR_ATOMIC);

	BUG_ON(atomic_ctx && !smmu);
	if (!smmu)
		return;

	if (stage1) {
		reg = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);
		reg += leaf ? ARM_SMMU_CB_S1_TLBIVAL : ARM_SMMU_CB_S1_TLBIVA;

		if (!IS_ENABLED(CONFIG_64BIT) || smmu->version == ARM_SMMU_V1) {
			iova &= ~12UL;
			iova |= ARM_SMMU_CB_ASID(cfg);
			writel_relaxed(iova, reg);
#ifdef CONFIG_64BIT
		} else {
			iova >>= 12;
			iova |= (u64)ARM_SMMU_CB_ASID(cfg) << 48;
			writeq_relaxed(iova, reg);
#endif
		}
#ifdef CONFIG_64BIT
	} else if (smmu->version == ARM_SMMU_V2) {
		reg = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);
		reg += leaf ? ARM_SMMU_CB_S2_TLBIIPAS2L :
			      ARM_SMMU_CB_S2_TLBIIPAS2;
		writeq_relaxed(iova >> 12, reg);
#endif
	} else {
		reg = ARM_SMMU_GR0(smmu) + ARM_SMMU_GR0_TLBIVMID;
		writel_relaxed(ARM_SMMU_CB_VMID(cfg), reg);
	}
}

static void arm_smmu_flush_pgtable(void *addr, size_t size, void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	int coherent_htw_disable = smmu_domain->attributes &
		(1 << DOMAIN_ATTR_COHERENT_HTW_DISABLE);


	/* Ensure new page tables are visible to the hardware walker */
	if (!coherent_htw_disable) {
		dsb(ishst);
	} else {
		/*
		 * If the SMMU can't walk tables in the CPU caches, treat them
		 * like non-coherent DMA since we need to flush the new entries
		 * all the way out to memory. There's no possibility of
		 * recursion here as the SMMU table walker will not be wired
		 * through another SMMU.
		 */
		dmac_clean_range(addr, addr + size);
	}
}

static void arm_smmu_tlbi_domain(struct iommu_domain *domain)
{
	arm_smmu_tlb_inv_context(domain->priv);
}

static int arm_smmu_enable_config_clocks(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;

	return arm_smmu_enable_clocks(smmu_domain->smmu);
}

static void arm_smmu_disable_config_clocks(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;

	arm_smmu_disable_clocks(smmu_domain->smmu);
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
		kfree(it);
	}
}

static void *arm_smmu_alloc_pages_exact(void *cookie,
					size_t size, gfp_t gfp_mask)
{
	void *ret;
	struct arm_smmu_domain *smmu_domain = cookie;

	if (!arm_smmu_is_master_side_secure(smmu_domain))
		return alloc_pages_exact(size, gfp_mask);

	ret = arm_smmu_secure_pool_remove(smmu_domain, size);
	if (ret)
		return ret;

	ret = alloc_pages_exact(size, gfp_mask);
	if (ret)
		arm_smmu_prepare_pgtable(ret, cookie);

	return ret;
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

static struct iommu_gather_ops arm_smmu_gather_ops = {
	.tlb_flush_all	= arm_smmu_tlb_inv_context,
	.tlb_add_flush	= arm_smmu_tlb_inv_range_nosync,
	.tlb_sync	= arm_smmu_tlb_sync,
	.flush_pgtable	= arm_smmu_flush_pgtable,
	.alloc_pages_exact = arm_smmu_alloc_pages_exact,
	.free_pages_exact = arm_smmu_free_pages_exact,
};

static phys_addr_t arm_smmu_verify_fault(struct iommu_domain *domain,
					 dma_addr_t iova, u32 fsr)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu;
	void __iomem *cb_base;
	u64 sctlr, sctlr_orig;
	phys_addr_t phys;

	smmu = smmu_domain->smmu;
	cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);

	arm_smmu_halt_nowait(smmu);

	writel_relaxed(RESUME_TERMINATE, cb_base + ARM_SMMU_CB_RESUME);

	arm_smmu_wait_for_halt(smmu);

	/* clear FSR to allow ATOS to log any faults */
	writel_relaxed(fsr, cb_base + ARM_SMMU_CB_FSR);

	/* disable stall mode momentarily */
	sctlr_orig = readl_relaxed(cb_base + ARM_SMMU_CB_SCTLR);
	sctlr = sctlr_orig & ~SCTLR_CFCFG;
	writel_relaxed(sctlr, cb_base + ARM_SMMU_CB_SCTLR);

	phys = arm_smmu_iova_to_phys_hard_no_halt(domain, iova);

	if (!phys) {
		dev_err(smmu->dev,
			"ATOS failed. Will issue a TLBIALL and try again...\n");
		arm_smmu_tlb_inv_context(smmu_domain);
		phys = arm_smmu_iova_to_phys_hard_no_halt(domain, iova);
		if (phys)
			dev_err(smmu->dev,
				"ATOS succeeded this time. Maybe we missed a TLB invalidation while messing with page tables earlier??\n");
		else
			dev_err(smmu->dev,
				"ATOS still failed. If the page tables look good (check the software table walk) then hardware might be misbehaving.\n");
	}

	/* restore SCTLR */
	writel_relaxed(sctlr_orig, cb_base + ARM_SMMU_CB_SCTLR);

	arm_smmu_resume(smmu);

	return phys;
}

static irqreturn_t arm_smmu_context_fault(int irq, void *dev)
{
	int flags, ret, tmp;
	u32 fsr, fsynr, resume;
	unsigned long iova, far;
	struct iommu_domain *domain = dev;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu;
	void __iomem *cb_base;
	bool ctx_hang_errata;
	bool fatal_asf;
	void __iomem *gr1_base;
	phys_addr_t phys_soft;
	u32 frsynra;
	bool non_fatal_fault = smmu_domain->non_fatal_faults;

	static DEFINE_RATELIMIT_STATE(_rs,
				      DEFAULT_RATELIMIT_INTERVAL,
				      DEFAULT_RATELIMIT_BURST);

	mutex_lock(&smmu_domain->init_mutex);
	smmu = smmu_domain->smmu;
	if (!smmu) {
		ret = IRQ_HANDLED;
		pr_err("took a fault on a detached domain (%p)\n", domain);
		goto out_unlock;
	}
	ctx_hang_errata = smmu->options & ARM_SMMU_OPT_ERRATA_CTX_FAULT_HANG;
	fatal_asf = smmu->options & ARM_SMMU_OPT_FATAL_ASF;

	if (arm_smmu_enable_clocks(smmu)) {
		ret = IRQ_NONE;
		goto out_unlock;
	}

	gr1_base = ARM_SMMU_GR1(smmu);
	cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);
	fsr = readl_relaxed(cb_base + ARM_SMMU_CB_FSR);

	if (!(fsr & FSR_FAULT)) {
		arm_smmu_disable_clocks(smmu);
		ret = IRQ_NONE;
		goto out_unlock;
	}

	if (fatal_asf && (fsr & FSR_ASF)) {
		dev_err(smmu->dev,
			"Took an address size fault.  Refusing to recover.\n");
		BUG();
	}

	fsynr = readl_relaxed(cb_base + ARM_SMMU_CB_FSYNR0);
	flags = fsynr & FSYNR0_WNR ? IOMMU_FAULT_WRITE : IOMMU_FAULT_READ;
	if (fsr & FSR_TF)
		flags |= IOMMU_FAULT_TRANSLATION;
	if (fsr & FSR_PF)
		flags |= IOMMU_FAULT_PERMISSION;
	if (fsr & FSR_EF)
		flags |= IOMMU_FAULT_EXTERNAL;
	if (fsr & FSR_SS)
		flags |= IOMMU_FAULT_TRANSACTION_STALLED;

	far = readl_relaxed(cb_base + ARM_SMMU_CB_FAR_LO);
#ifdef CONFIG_64BIT
	far |= ((u64)readl_relaxed(cb_base + ARM_SMMU_CB_FAR_HI)) << 32;
#endif
	iova = far;

	phys_soft = arm_smmu_iova_to_phys(domain, iova);
	frsynra = readl_relaxed(gr1_base + ARM_SMMU_GR1_CBFRSYNRA(cfg->cbndx));
	tmp = report_iommu_fault(domain, smmu->dev, iova, flags);
	if (!tmp || (tmp == -EBUSY)) {
		dev_dbg(smmu->dev,
			"Context fault handled by client: iova=0x%08lx, fsr=0x%x, fsynr=0x%x, cb=%d\n",
			iova, fsr, fsynr, cfg->cbndx);
		dev_dbg(smmu->dev,
			"soft iova-to-phys=%pa\n", &phys_soft);
		ret = IRQ_HANDLED;
		resume = RESUME_TERMINATE;
	} else {
		phys_addr_t phys_atos = arm_smmu_verify_fault(domain, iova,
							      fsr);

		if (__ratelimit(&_rs)) {
			dev_err(smmu->dev,
				"Unhandled context fault: iova=0x%08lx, fsr=0x%x, fsynr=0x%x, cb=%d\n",
				iova, fsr, fsynr, cfg->cbndx);
			dev_err(smmu->dev, "FAR    = %016lx\n",
				(unsigned long)far);
			dev_err(smmu->dev,
				"FSR    = %08x [%s%s%s%s%s%s%s%s%s]\n",
				fsr,
				(fsr & 0x02) ? "TF " : "",
				(fsr & 0x04) ? "AFF " : "",
				(fsr & 0x08) ? "PF " : "",
				(fsr & 0x10) ? "EF " : "",
				(fsr & 0x20) ? "TLBMCF " : "",
				(fsr & 0x40) ? "TLBLKF " : "",
				(fsr & 0x80) ? "MHF " : "",
				(fsr & 0x40000000) ? "SS " : "",
				(fsr & 0x80000000) ? "MULTI " : "");
			dev_err(smmu->dev,
				"soft iova-to-phys=%pa\n", &phys_soft);
			if (!phys_soft)
				dev_err(smmu->dev,
					"SOFTWARE TABLE WALK FAILED! Looks like %s accessed an unmapped address!\n",
					dev_name(smmu->dev));
			dev_err(smmu->dev,
				"hard iova-to-phys (ATOS)=%pa\n", &phys_atos);
			dev_err(smmu->dev, "SID=0x%x\n", frsynra & 0xffff);
		}
		ret = IRQ_NONE;
		resume = RESUME_TERMINATE;
		if (!non_fatal_fault) {
			dev_err(smmu->dev,
				"Unhandled context faults are fatal on this domain. Going down now...\n");
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
		if (fsr & FSR_SS) {
			if (ctx_hang_errata)
				arm_smmu_tlb_sync_cb(smmu, cfg->cbndx);
			writel_relaxed(resume, cb_base + ARM_SMMU_CB_RESUME);
		}
	}

	arm_smmu_disable_clocks(smmu);
out_unlock:
	mutex_unlock(&smmu_domain->init_mutex);

	return ret;
}

static irqreturn_t arm_smmu_global_fault(int irq, void *dev)
{
	u32 gfsr, gfsynr0, gfsynr1, gfsynr2;
	struct arm_smmu_device *smmu = dev;
	void __iomem *gr0_base = ARM_SMMU_GR0_NS(smmu);

	if (arm_smmu_enable_clocks(smmu))
		return IRQ_NONE;

	gfsr = readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSR);
	gfsynr0 = readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSYNR0);
	gfsynr1 = readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSYNR1);
	gfsynr2 = readl_relaxed(gr0_base + ARM_SMMU_GR0_sGFSYNR2);

	if (!gfsr) {
		arm_smmu_disable_clocks(smmu);
		return IRQ_NONE;
	}

	dev_err_ratelimited(smmu->dev,
		"Unexpected global fault, this could be serious\n");
	dev_err_ratelimited(smmu->dev,
		"\tGFSR 0x%08x, GFSYNR0 0x%08x, GFSYNR1 0x%08x, GFSYNR2 0x%08x\n",
		gfsr, gfsynr0, gfsynr1, gfsynr2);

	writel(gfsr, gr0_base + ARM_SMMU_GR0_sGFSR);
	arm_smmu_disable_clocks(smmu);
	return IRQ_HANDLED;
}

static void arm_smmu_trigger_fault(struct iommu_domain *domain,
				   unsigned long flags)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu;
	void __iomem *cb_base;

	if (!smmu_domain->smmu) {
		pr_err("Can't trigger faults on non-attached domains\n");
		return;
	}

	smmu = smmu_domain->smmu;

	cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);
	if (arm_smmu_enable_clocks(smmu))
		return;
	dev_err(smmu->dev, "Writing 0x%lx to FSRRESTORE on cb %d\n",
		flags, cfg->cbndx);
	writel_relaxed(flags, cb_base + ARM_SMMU_CB_FSRRESTORE);
	/* give the interrupt time to fire... */
	msleep(1000);
	arm_smmu_disable_clocks(smmu);
}

static void arm_smmu_init_context_bank(struct arm_smmu_domain *smmu_domain,
				       struct io_pgtable_cfg *pgtbl_cfg)
{
	u32 reg;
	bool stage1;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	void __iomem *cb_base, *gr0_base, *gr1_base;

	gr0_base = ARM_SMMU_GR0(smmu);
	gr1_base = ARM_SMMU_GR1(smmu);
	stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;
	cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);

	/* CBAR */
	reg = cfg->cbar;
	if (smmu->version == ARM_SMMU_V1)
		reg |= cfg->irptndx << CBAR_IRPTNDX_SHIFT;

	/*
	 * Use the weakest shareability/memory types, so they are
	 * overridden by the ttbcr/pte.
	 */
	if (stage1) {
		reg |= (CBAR_S1_BPSHCFG_NSH << CBAR_S1_BPSHCFG_SHIFT) |
			(CBAR_S1_MEMATTR_WB << CBAR_S1_MEMATTR_SHIFT);
	}
	reg |= ARM_SMMU_CB_VMID(cfg) << CBAR_VMID_SHIFT;
	writel_relaxed(reg, gr1_base + ARM_SMMU_GR1_CBAR(cfg->cbndx));

	if (smmu->version > ARM_SMMU_V1) {
		/* CBA2R */
#ifdef CONFIG_64BIT
		reg = CBA2R_RW64_64BIT;
		if (!arm_smmu_has_secure_vmid(smmu_domain) &&
			arm_smmu_is_static_cb(smmu))
			msm_tz_set_cb_format(smmu->sec_id, cfg->cbndx);
#else
		reg = CBA2R_RW64_32BIT;
#endif
		writel_relaxed(reg, gr1_base + ARM_SMMU_GR1_CBA2R(cfg->cbndx));
	}

	/* TTBRs */
	if (stage1) {
		reg = pgtbl_cfg->arm_lpae_s1_cfg.ttbr[0];
		writel_relaxed(reg, cb_base + ARM_SMMU_CB_TTBR0_LO);
		reg = pgtbl_cfg->arm_lpae_s1_cfg.ttbr[0] >> 32;
		reg |= ARM_SMMU_CB_ASID(cfg) << TTBRn_HI_ASID_SHIFT;
		writel_relaxed(reg, cb_base + ARM_SMMU_CB_TTBR0_HI);

		reg = pgtbl_cfg->arm_lpae_s1_cfg.ttbr[1];
		writel_relaxed(reg, cb_base + ARM_SMMU_CB_TTBR1_LO);
		reg = pgtbl_cfg->arm_lpae_s1_cfg.ttbr[1] >> 32;
		reg |= ARM_SMMU_CB_ASID(cfg) << TTBRn_HI_ASID_SHIFT;
		writel_relaxed(reg, cb_base + ARM_SMMU_CB_TTBR1_HI);
	} else {
		reg = pgtbl_cfg->arm_lpae_s2_cfg.vttbr;
		writel_relaxed(reg, cb_base + ARM_SMMU_CB_TTBR0_LO);
		reg = pgtbl_cfg->arm_lpae_s2_cfg.vttbr >> 32;
		writel_relaxed(reg, cb_base + ARM_SMMU_CB_TTBR0_HI);
	}

	/* TTBCR */
	if (stage1) {
		reg = pgtbl_cfg->arm_lpae_s1_cfg.tcr;
		writel_relaxed(reg, cb_base + ARM_SMMU_CB_TTBCR);
		if (smmu->version > ARM_SMMU_V1) {
			reg = pgtbl_cfg->arm_lpae_s1_cfg.tcr >> 32;
			reg |= (TTBCR2_SEP_NOSIGN << TTBCR2_SEP_SHIFT);
			writel_relaxed(reg, cb_base + ARM_SMMU_CB_TTBCR2);
		}
	} else {
		reg = pgtbl_cfg->arm_lpae_s2_cfg.vtcr;
		writel_relaxed(reg, cb_base + ARM_SMMU_CB_TTBCR);
	}

	/* MAIRs (stage-1 only) */
	if (stage1) {
		reg = pgtbl_cfg->arm_lpae_s1_cfg.mair[0];
		writel_relaxed(reg, cb_base + ARM_SMMU_CB_S1_MAIR0);
		reg = pgtbl_cfg->arm_lpae_s1_cfg.mair[1];
		writel_relaxed(reg, cb_base + ARM_SMMU_CB_S1_MAIR1);
	}

	if (smmu->model == SMMU_MODEL_QCOM_V2) {
		reg = ACTLR_QCOM_ISH << ACTLR_QCOM_ISH_SHIFT |
			ACTLR_QCOM_OSH << ACTLR_QCOM_OSH_SHIFT |
			ACTLR_QCOM_NSH << ACTLR_QCOM_NSH_SHIFT;
		writel_relaxed(reg, cb_base + ARM_SMMU_CB_ACTLR);
	}

	/* SCTLR */
	reg = SCTLR_CFCFG | SCTLR_CFIE | SCTLR_CFRE | SCTLR_EAE_SBOP;

	if (!(smmu_domain->attributes & (1 << DOMAIN_ATTR_S1_BYPASS)) ||
								!stage1)
		reg |= SCTLR_M;
	if (stage1)
		reg |= SCTLR_S1_ASIDPNE;
#ifdef __BIG_ENDIAN
	reg |= SCTLR_E;
#endif
	writel_relaxed(reg, cb_base + ARM_SMMU_CB_SCTLR);
}

static bool arm_smmu_is_static_cb(struct arm_smmu_device *smmu)
{
	return smmu->options & ARM_SMMU_OPT_STATIC_CB;
}

static bool arm_smmu_has_secure_vmid(struct arm_smmu_domain *smmu_domain)
{
	return smmu_domain->secure_vmid != VMID_INVAL;
}

static bool arm_smmu_is_slave_side_secure(struct arm_smmu_domain *smmu_domain)
{
	return arm_smmu_has_secure_vmid(smmu_domain)
		&& smmu_domain->slave_side_secure;
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

static unsigned long arm_smmu_pgtbl_lock(struct arm_smmu_domain *smmu_domain)
{
	unsigned long flags = 0;

	if (arm_smmu_is_slave_side_secure(smmu_domain))
		mutex_lock(&smmu_domain->pgtbl_mutex_lock);
	else
		spin_lock_irqsave(&smmu_domain->pgtbl_spin_lock, flags);

	return flags;
}

static void arm_smmu_pgtbl_unlock(struct arm_smmu_domain *smmu_domain,
					unsigned long flags)
{
	if (arm_smmu_is_slave_side_secure(smmu_domain))
		mutex_unlock(&smmu_domain->pgtbl_mutex_lock);
	else
		spin_unlock_irqrestore(&smmu_domain->pgtbl_spin_lock, flags);
}

static int arm_smmu_restore_sec_cfg(struct arm_smmu_device *smmu)
{
	int ret, scm_ret;

	if (!arm_smmu_is_static_cb(smmu))
		return 0;

	ret = scm_restore_sec_cfg(smmu->sec_id, 0x0, &scm_ret);
	if (ret || scm_ret) {
		pr_err("scm call IOMMU_SECURE_CFG failed\n");
		return -EINVAL;
	}

	return 0;
}

static int arm_smmu_init_domain_context(struct iommu_domain *domain,
					struct arm_smmu_device *smmu)
{
	int irq, start, ret = 0;
	unsigned long ias, oas;
	struct io_pgtable_ops *pgtbl_ops;
	enum io_pgtable_fmt fmt;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	bool is_fast = smmu_domain->attributes & (1 << DOMAIN_ATTR_FAST);

	if (smmu_domain->smmu)
		goto out;

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

	switch (smmu_domain->stage) {
	case ARM_SMMU_DOMAIN_S1:
		cfg->cbar = CBAR_TYPE_S1_TRANS_S2_BYPASS;
		start = smmu->num_s2_context_banks;
		ias = smmu->va_size;
		oas = smmu->ipa_size;
		if (IS_ENABLED(CONFIG_64BIT))
			fmt = ARM_64_LPAE_S1;
		else
			fmt = ARM_32_LPAE_S1;
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
		if (IS_ENABLED(CONFIG_64BIT))
			fmt = ARM_64_LPAE_S2;
		else
			fmt = ARM_32_LPAE_S2;
		break;
	default:
		ret = -EINVAL;
		goto out;
	}

	if (cfg->cbndx == INVALID_CBNDX) {
		ret = __arm_smmu_alloc_bitmap(smmu->context_map, start,
				smmu->num_context_banks);
		if (IS_ERR_VALUE(ret))
			goto out;
		cfg->cbndx = ret;
	} else {
		if (test_and_set_bit(cfg->cbndx, smmu->context_map))
			goto out;
	}

	if (smmu->version == ARM_SMMU_V1) {
		cfg->irptndx = atomic_inc_return(&smmu->irptndx);
		cfg->irptndx %= smmu->num_context_irqs;
	} else {
		cfg->irptndx = cfg->cbndx;
	}

	smmu_domain->smmu = smmu;

	if (arm_smmu_is_slave_side_secure(smmu_domain)) {
		smmu_domain->pgtbl_cfg = (struct io_pgtable_cfg) {
			.pgsize_bitmap	= arm_smmu_ops.pgsize_bitmap,
			.arm_msm_secure_cfg = {
				.sec_id = smmu->sec_id,
				.cbndx = cfg->cbndx,
			},
		};
		fmt = ARM_MSM_SECURE;
	} else {
		smmu_domain->pgtbl_cfg = (struct io_pgtable_cfg) {
			.pgsize_bitmap	= arm_smmu_ops.pgsize_bitmap,
			.ias		= ias,
			.oas		= oas,
			.tlb		= &arm_smmu_gather_ops,
		};
	}

	if (is_fast)
		fmt = ARM_V8L_FAST;

	cfg->asid = cfg->cbndx + 1;
	cfg->vmid = cfg->cbndx + 2;
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
	if (arm_smmu_is_master_side_secure(smmu_domain)) {
		arm_smmu_secure_domain_lock(smmu_domain);
		arm_smmu_assign_table(smmu_domain);
		arm_smmu_secure_domain_unlock(smmu_domain);
	}

	/* Initialise the context bank with our page table cfg */
	arm_smmu_init_context_bank(smmu_domain, &smmu_domain->pgtbl_cfg);

	/*
	 * Request context fault interrupt. Do this last to avoid the
	 * handler seeing a half-initialised domain state.
	 */
	irq = smmu->irqs[smmu->num_global_irqs + cfg->irptndx];
	ret = request_threaded_irq(irq, NULL, arm_smmu_context_fault,
				IRQF_ONESHOT | IRQF_SHARED,
				"arm-smmu-context-fault", domain);
	if (IS_ERR_VALUE(ret)) {
		dev_err(smmu->dev, "failed to request context IRQ %d (%u)\n",
			cfg->irptndx, irq);
		cfg->irptndx = INVALID_IRPTNDX;
	}

	/* Publish page table ops for map/unmap */
	smmu_domain->pgtbl_ops = pgtbl_ops;
	return 0;

out_clear_smmu:
	smmu_domain->smmu = NULL;
out:
	return ret;
}

static void arm_smmu_destroy_domain_context(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	void __iomem *cb_base;
	int irq;

	if (arm_smmu_enable_clocks(smmu_domain->smmu))
		goto free_irqs;
	/*
	 * Disable the context bank and free the page tables before freeing
	 * it.
	 */
	cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);
	writel_relaxed(0, cb_base + ARM_SMMU_CB_SCTLR);

	arm_smmu_tlb_inv_context(smmu_domain);

	arm_smmu_disable_clocks(smmu_domain->smmu);

free_irqs:
	if (cfg->irptndx != INVALID_IRPTNDX) {
		irq = smmu->irqs[smmu->num_global_irqs + cfg->irptndx];
		free_irq(irq, domain);
	}

	__arm_smmu_free_bitmap(smmu->context_map, cfg->cbndx);
	smmu_domain->smmu = NULL;
}

static int arm_smmu_domain_init(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain;

	/*
	 * Allocate the domain and initialise some of its data structures.
	 * We can't really do anything meaningful until we've added a
	 * master.
	 */
	smmu_domain = kzalloc(sizeof(*smmu_domain), GFP_KERNEL);
	if (!smmu_domain)
		return -ENOMEM;

	smmu_domain->secure_vmid = VMID_INVAL;
	/* disable coherent htw by default */
	smmu_domain->attributes = (1 << DOMAIN_ATTR_COHERENT_HTW_DISABLE);
	INIT_LIST_HEAD(&smmu_domain->pte_info_list);
	INIT_LIST_HEAD(&smmu_domain->unassign_list);
	INIT_LIST_HEAD(&smmu_domain->secure_pool_list);
	smmu_domain->cfg.cbndx = INVALID_CBNDX;
	smmu_domain->cfg.irptndx = INVALID_IRPTNDX;
	smmu_domain->cfg.asid = INVALID_ASID;
	smmu_domain->cfg.vmid = INVALID_VMID;

	mutex_init(&smmu_domain->init_mutex);
	spin_lock_init(&smmu_domain->pgtbl_spin_lock);
	mutex_init(&smmu_domain->assign_lock);
	mutex_init(&smmu_domain->pgtbl_mutex_lock);
	domain->priv = smmu_domain;
	return 0;
}

static void arm_smmu_domain_destroy(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;

	/*
	 * Free the domain resources. We assume that all devices have
	 * already been detached.
	 */
	if (smmu_domain->pgtbl_ops) {
		free_io_pgtable_ops(smmu_domain->pgtbl_ops);
		/* unassign any freed page table memory */
		if (arm_smmu_is_master_side_secure(smmu_domain)) {
			arm_smmu_secure_domain_lock(smmu_domain);
			arm_smmu_secure_pool_destroy(smmu_domain);
			arm_smmu_unassign_table(smmu_domain);
			arm_smmu_secure_domain_unlock(smmu_domain);
		}
		smmu_domain->pgtbl_ops = NULL;
	}

	kfree(smmu_domain);
}

static int arm_smmu_master_configure_smrs(struct arm_smmu_device *smmu,
					  struct arm_smmu_master_cfg *cfg)
{
	int i;
	struct arm_smmu_smr *smrs;
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);

	if (!(smmu->features & ARM_SMMU_FEAT_STREAM_MATCH))
		return 0;

	if (cfg->smrs)
		return -EEXIST;

	smrs = kmalloc_array(cfg->num_streamids, sizeof(*smrs), GFP_KERNEL);
	if (!smrs) {
		dev_err(smmu->dev, "failed to allocate %d SMRs\n",
			cfg->num_streamids);
		return -ENOMEM;
	}

	/* Allocate the SMRs on the SMMU */
	for (i = 0; i < cfg->num_streamids; ++i) {
		int idx = __arm_smmu_alloc_bitmap(smmu->smr_map, 0,
						  smmu->num_mapping_groups);
		if (IS_ERR_VALUE(idx)) {
			dev_err(smmu->dev, "failed to allocate free SMR\n");
			goto err_free_smrs;
		}

		smrs[i] = (struct arm_smmu_smr) {
			.idx	= idx,
			.mask	= 0, /* We don't currently share SMRs */
			.id	= cfg->streamids[i],
		};
	}

	/* It worked! Now, poke the actual hardware */
	for (i = 0; i < cfg->num_streamids; ++i) {
		u32 reg = SMR_VALID | smrs[i].id << SMR_ID_SHIFT |
			  smrs[i].mask << SMR_MASK_SHIFT;
		writel_relaxed(reg, gr0_base + ARM_SMMU_GR0_SMR(smrs[i].idx));
	}

	cfg->smrs = smrs;
	return 0;

err_free_smrs:
	while (--i >= 0)
		__arm_smmu_free_bitmap(smmu->smr_map, smrs[i].idx);
	kfree(smrs);
	return -ENOSPC;
}

static void arm_smmu_master_free_smrs(struct arm_smmu_device *smmu,
				      struct arm_smmu_master_cfg *cfg)
{
	int i;
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);
	struct arm_smmu_smr *smrs = cfg->smrs;

	if (!smrs)
		return;

	/* Invalidate the SMRs before freeing back to the allocator */
	for (i = 0; i < cfg->num_streamids; ++i) {
		u8 idx = smrs[i].idx;

		writel_relaxed(~SMR_VALID, gr0_base + ARM_SMMU_GR0_SMR(idx));
		__arm_smmu_free_bitmap(smmu->smr_map, idx);
	}

	cfg->smrs = NULL;
	kfree(smrs);
}

static int arm_smmu_domain_add_master(struct arm_smmu_domain *smmu_domain,
				      struct arm_smmu_master_cfg *cfg)
{
	int i, ret;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);

	/* Devices in an IOMMU group may already be configured */
	ret = arm_smmu_master_configure_smrs(smmu, cfg);
	if (ret)
		return ret == -EEXIST ? 0 : ret;

	for (i = 0; i < cfg->num_streamids; ++i) {
		u32 idx, s2cr;

		idx = cfg->smrs ? cfg->smrs[i].idx : cfg->streamids[i];
		s2cr = S2CR_TYPE_TRANS |
		       (smmu_domain->cfg.cbndx << S2CR_CBNDX_SHIFT);
		writel_relaxed(s2cr, gr0_base + ARM_SMMU_GR0_S2CR(idx));
	}

	return 0;
}

static void arm_smmu_domain_remove_master(struct arm_smmu_domain *smmu_domain,
					  struct arm_smmu_master_cfg *cfg)
{
	int i;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);

	/* An IOMMU group is torn down by the first device to be removed */
	if ((smmu->features & ARM_SMMU_FEAT_STREAM_MATCH) && !cfg->smrs)
		return;

	/*
	 * We *must* clear the S2CR first, because freeing the SMR means
	 * that it can be re-allocated immediately.
	 */
	if (arm_smmu_enable_clocks(smmu))
		return;
	for (i = 0; i < cfg->num_streamids; ++i) {
		u32 idx = cfg->smrs ? cfg->smrs[i].idx : cfg->streamids[i];

		writel_relaxed(S2CR_TYPE_BYPASS,
			       gr0_base + ARM_SMMU_GR0_S2CR(idx));
	}

	arm_smmu_master_free_smrs(smmu, cfg);
	arm_smmu_disable_clocks(smmu);
}

static void arm_smmu_impl_def_programming(struct arm_smmu_device *smmu)
{
	int i;
	struct arm_smmu_impl_def_reg *regs = smmu->impl_def_attach_registers;

	arm_smmu_halt(smmu);
	for (i = 0; i < smmu->num_impl_def_attach_registers; ++i)
		writel_relaxed(regs[i].value,
			ARM_SMMU_GR0(smmu) + regs[i].offset);
	arm_smmu_resume(smmu);
}

static int arm_smmu_attach_dynamic(struct iommu_domain *domain,
					struct arm_smmu_device *smmu)
{
	int ret;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	enum io_pgtable_fmt fmt;
	struct io_pgtable_ops *pgtbl_ops = NULL;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;

	if (!(smmu->options & ARM_SMMU_OPT_DYNAMIC)) {
		dev_err(smmu->dev, "dynamic domains not supported\n");
		return -EPERM;
	}

	if (smmu_domain->smmu != NULL) {
		dev_err(smmu->dev, "domain is already attached\n");
		return -EBUSY;
	}

	if (smmu_domain->cfg.cbndx >= smmu->num_context_banks) {
		dev_err(smmu->dev, "invalid context bank\n");
		return -ENODEV;
	}

	if (smmu->features & ARM_SMMU_FEAT_TRANS_NESTED) {
		smmu_domain->cfg.cbar = CBAR_TYPE_S1_TRANS_S2_BYPASS;
	} else if (smmu->features & ARM_SMMU_FEAT_TRANS_S1) {
		smmu_domain->cfg.cbar = CBAR_TYPE_S1_TRANS_S2_BYPASS;
	} else {
		/* dynamic only makes sense for S1. */
		return -EINVAL;
	}

	smmu_domain->pgtbl_cfg = (struct io_pgtable_cfg) {
		.pgsize_bitmap	= arm_smmu_ops.pgsize_bitmap,
		.ias		= smmu->va_size,
		.oas		= smmu->ipa_size,
		.tlb		= &arm_smmu_gather_ops,
	};

	fmt = IS_ENABLED(CONFIG_64BIT) ? ARM_64_LPAE_S1 : ARM_32_LPAE_S1;

	pgtbl_ops = alloc_io_pgtable_ops(fmt, &smmu_domain->pgtbl_cfg,
					 smmu_domain);
	if (!pgtbl_ops)
		return -ENOMEM;

	/*
	 * assign any page table memory that might have been allocated
	 * during alloc_io_pgtable_ops
	 */
	if (arm_smmu_is_master_side_secure(smmu_domain)) {
		arm_smmu_secure_domain_lock(smmu_domain);
		arm_smmu_assign_table(smmu_domain);
		arm_smmu_secure_domain_unlock(smmu_domain);
	}

	cfg->vmid = cfg->cbndx + 2;
	smmu_domain->smmu = smmu;

	mutex_lock(&smmu->attach_lock);
	/* try to avoid reusing an old ASID right away */
	ret = idr_alloc_cyclic(&smmu->asid_idr, domain,
				smmu->num_context_banks + 2,
				MAX_ASID + 1, GFP_KERNEL);
	if (ret < 0) {
		dev_err_ratelimited(smmu->dev,
			"dynamic ASID allocation failed: %d\n", ret);
		goto out;
	}

	smmu_domain->cfg.asid = ret;
	smmu_domain->smmu = smmu;
	smmu_domain->pgtbl_ops = pgtbl_ops;
	ret = 0;
out:
	if (ret)
		free_io_pgtable_ops(pgtbl_ops);
	mutex_unlock(&smmu->attach_lock);

	return ret;
}

static int arm_smmu_populate_cb(struct arm_smmu_device *smmu,
		struct arm_smmu_domain *smmu_domain, struct device *dev)
{
	void __iomem *gr0_base;
	struct arm_smmu_master_cfg *cfg;
	struct arm_smmu_cfg *smmu_cfg = &smmu_domain->cfg;
	int i;
	u32 sid;

	gr0_base = ARM_SMMU_GR0(smmu);
	cfg = find_smmu_master_cfg(dev);

	if (!cfg)
		return -ENODEV;

	sid = cfg->streamids[0];

	for (i = 0; i < smmu->num_mapping_groups; i++) {
		u32 smr, s2cr;
		u8 cbndx;

		smr = readl_relaxed(gr0_base + ARM_SMMU_GR0_SMR(i));

		if (sid == ((smr >> SMR_ID_SHIFT) & SMR_ID_MASK)) {
			s2cr = readl_relaxed(gr0_base + ARM_SMMU_GR0_S2CR(i));
			cbndx = (s2cr >> S2CR_CBNDX_SHIFT) & S2CR_CBNDX_MASK;
			smmu_cfg->cbndx = cbndx;
			return 0;
		}
	}

	return -EINVAL;
}

static int arm_smmu_attach_dev(struct iommu_domain *domain, struct device *dev)
{
	int ret;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_device *smmu;
	struct arm_smmu_master_cfg *cfg;
	int atomic_ctx = smmu_domain->attributes & (1 << DOMAIN_ATTR_ATOMIC);

	mutex_lock(&smmu_domain->init_mutex);
	smmu = find_smmu_for_device(dev);
	if (!smmu) {
		dev_err(dev, "cannot attach to SMMU, is it on the same bus?\n");
		mutex_unlock(&smmu_domain->init_mutex);
		return -ENXIO;
	}

	if (smmu_domain->attributes & (1 << DOMAIN_ATTR_DYNAMIC)) {
		ret = arm_smmu_attach_dynamic(domain, smmu);
		mutex_unlock(&smmu_domain->init_mutex);
		return ret;
	}

	mutex_lock(&smmu->attach_lock);

	if (dev->archdata.iommu) {
		dev_err(dev, "already attached to IOMMU domain\n");
		ret = -EEXIST;
		goto err_unlock;
	}

	if (!smmu->attach_count) {
		/*
		 * We need an extra power vote if we can't retain register
		 * settings across a power collapse, or if this is an
		 * atomic domain (since atomic domains can't sleep during
		 * unmap, so regulators already need to be on to enable tlb
		 * invalidation).  The result (due to regulator
		 * refcounting) is that we never disable regulators while a
		 * client is attached in these cases.
		 */
		if (!(smmu->options & ARM_SMMU_OPT_REGISTER_SAVE) ||
		    atomic_ctx) {
			ret = arm_smmu_enable_regulators(smmu);
			if (ret)
				goto err_unlock;
		}
		ret = arm_smmu_enable_clocks(smmu);
		if (ret)
			goto err_disable_regulators;
		arm_smmu_device_reset(smmu);
		arm_smmu_impl_def_programming(smmu);
	} else {
		ret = arm_smmu_enable_clocks(smmu);
		if (ret)
			goto err_unlock;
	}
	smmu->attach_count++;

	if (arm_smmu_is_static_cb(smmu)) {
		ret = arm_smmu_populate_cb(smmu, smmu_domain, dev);

		if (ret) {
			dev_err(dev, "Failed to get valid context bank\n");
			goto err_disable_clocks;
		}
		smmu_domain->slave_side_secure = true;
	}

	/* Ensure that the domain is finalised */
	ret = arm_smmu_init_domain_context(domain, smmu);
	if (IS_ERR_VALUE(ret))
		goto err_disable_clocks;

	/*
	 * Sanity check the domain. We don't support domains across
	 * different SMMUs.
	 */
	if (smmu_domain->smmu != smmu) {
		dev_err(dev,
			"cannot attach to SMMU %s whilst already attached to domain on SMMU %s\n",
			dev_name(smmu_domain->smmu->dev), dev_name(smmu->dev));
		ret = -EINVAL;
		goto err_destroy_domain_context;
	}

	if (!(smmu_domain->attributes & (1 << DOMAIN_ATTR_COHERENT_HTW_DISABLE))
		&& !(smmu->features & ARM_SMMU_FEAT_COHERENT_WALK)) {
		dev_err(dev,
			"Can't attach: this domain wants coherent htw but %s doesn't support it\n",
			dev_name(smmu_domain->smmu->dev));
		ret = -EINVAL;
		goto err_destroy_domain_context;
	}

	/* Looks ok, so add the device to the domain */
	cfg = find_smmu_master_cfg(dev);
	if (!cfg) {
		ret = -ENODEV;
		goto err_destroy_domain_context;
	}

	ret = arm_smmu_domain_add_master(smmu_domain, cfg);
	if (ret)
		goto err_destroy_domain_context;
	dev->archdata.iommu = domain;
	arm_smmu_disable_clocks(smmu);
	mutex_unlock(&smmu->attach_lock);
	mutex_unlock(&smmu_domain->init_mutex);
	return ret;

err_destroy_domain_context:
	arm_smmu_destroy_domain_context(domain);
err_disable_clocks:
	arm_smmu_disable_clocks(smmu);
	--smmu->attach_count;
err_disable_regulators:
	if (!smmu->attach_count &&
	    (!(smmu->options & ARM_SMMU_OPT_REGISTER_SAVE) || atomic_ctx))
		arm_smmu_disable_regulators(smmu);
err_unlock:
	mutex_unlock(&smmu->attach_lock);
	mutex_unlock(&smmu_domain->init_mutex);
	return ret;
}

static void arm_smmu_power_off(struct arm_smmu_device *smmu,
			       bool force_regulator_disable)
{
	/* Turn the thing off */
	if (arm_smmu_enable_clocks(smmu))
		return;
	writel_relaxed(sCR0_CLIENTPD,
		ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sCR0);
	arm_smmu_disable_clocks(smmu);
	if (!(smmu->options & ARM_SMMU_OPT_REGISTER_SAVE)
	    || force_regulator_disable)
		arm_smmu_disable_regulators(smmu);
}

static void arm_smmu_detach_dynamic(struct iommu_domain *domain,
					struct arm_smmu_device *smmu)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;

	mutex_lock(&smmu->attach_lock);
	if (smmu->attach_count > 0) {
		if (arm_smmu_enable_clocks(smmu_domain->smmu))
			goto idr_remove;
		arm_smmu_tlb_inv_context(smmu_domain);
		arm_smmu_disable_clocks(smmu_domain->smmu);
	}
idr_remove:
	idr_remove(&smmu->asid_idr, smmu_domain->cfg.asid);
	smmu_domain->cfg.asid = INVALID_ASID;
	smmu_domain->smmu = NULL;
	mutex_unlock(&smmu->attach_lock);
}

static void arm_smmu_detach_dev(struct iommu_domain *domain, struct device *dev)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_master_cfg *cfg;
	struct arm_smmu_device *smmu;
	int atomic_ctx = smmu_domain->attributes & (1 << DOMAIN_ATTR_ATOMIC);

	mutex_lock(&smmu_domain->init_mutex);
	smmu = smmu_domain->smmu;
	if (!smmu) {
		dev_err(dev, "Domain already detached!\n");
		mutex_unlock(&smmu_domain->init_mutex);
		return;
	}


	if (smmu_domain->attributes & (1 << DOMAIN_ATTR_DYNAMIC)) {
		arm_smmu_detach_dynamic(domain, smmu);
		mutex_unlock(&smmu_domain->init_mutex);
		return;
	}

	mutex_lock(&smmu->attach_lock);

	cfg = find_smmu_master_cfg(dev);
	if (!cfg)
		goto unlock;

	dev->archdata.iommu = NULL;
	arm_smmu_domain_remove_master(smmu_domain, cfg);
	arm_smmu_destroy_domain_context(domain);
	if (!--smmu->attach_count)
		arm_smmu_power_off(smmu, atomic_ctx);
unlock:
	mutex_unlock(&smmu->attach_lock);
	mutex_unlock(&smmu_domain->init_mutex);
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
	return;
}

static void arm_smmu_unprepare_pgtable(void *cookie, void *addr, size_t size)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_pte_info *pte_info;

	BUG_ON(!arm_smmu_is_master_side_secure(smmu_domain));

	pte_info = kzalloc(sizeof(struct arm_smmu_pte_info), GFP_ATOMIC);
	if (!pte_info)
		return;

	pte_info->virt_addr = addr;
	pte_info->size = size;
	list_add_tail(&pte_info->entry, &smmu_domain->unassign_list);
}

static void arm_smmu_prepare_pgtable(void *addr, void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_pte_info *pte_info;

	BUG_ON(!arm_smmu_is_master_side_secure(smmu_domain));

	pte_info = kzalloc(sizeof(struct arm_smmu_pte_info), GFP_ATOMIC);
	if (!pte_info)
		return;
	pte_info->virt_addr = addr;
	list_add_tail(&pte_info->entry, &smmu_domain->pte_info_list);
}

static int arm_smmu_map(struct iommu_domain *domain, unsigned long iova,
			phys_addr_t paddr, size_t size, int prot)
{
	int ret;
	unsigned long flags;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct io_pgtable_ops *ops= smmu_domain->pgtbl_ops;

	if (!ops)
		return -ENODEV;

	arm_smmu_secure_domain_lock(smmu_domain);

	flags = arm_smmu_pgtbl_lock(smmu_domain);
	ret = ops->map(ops, iova, paddr, size, prot);
	arm_smmu_pgtbl_unlock(smmu_domain, flags);

	if (!ret)
		ret = arm_smmu_assign_table(smmu_domain);

	arm_smmu_secure_domain_unlock(smmu_domain);

	return ret;
}

static size_t arm_smmu_map_sg(struct iommu_domain *domain, unsigned long iova,
			   struct scatterlist *sg, unsigned int nents, int prot)
{
	int ret;
	size_t size;
	unsigned long flags;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct io_pgtable_ops *ops = smmu_domain->pgtbl_ops;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	int atomic_ctx = smmu_domain->attributes & (1 << DOMAIN_ATTR_ATOMIC);

	if (!ops)
		return -ENODEV;

	if (arm_smmu_is_slave_side_secure(smmu_domain) && atomic_ctx) {
		dev_err(smmu->dev, "Slave side atomic context not supported\n");
		return 0;
	}

	if (arm_smmu_is_slave_side_secure(smmu_domain)) {
		mutex_lock(&smmu_domain->init_mutex);

		if (arm_smmu_enable_clocks(smmu)) {
			mutex_unlock(&smmu_domain->init_mutex);
			return 0;
		}
	}

	arm_smmu_secure_domain_lock(smmu_domain);

	flags = arm_smmu_pgtbl_lock(smmu_domain);
	ret = ops->map_sg(ops, iova, sg, nents, prot, &size);
	arm_smmu_pgtbl_unlock(smmu_domain, flags);

	if (ret) {
		if (arm_smmu_assign_table(smmu_domain)) {
			ret = 0;
			goto out;
		}
	} else {
		arm_smmu_secure_domain_unlock(smmu_domain);
		arm_smmu_unmap(domain, iova, size);
	}

out:
	arm_smmu_secure_domain_unlock(smmu_domain);
	if (arm_smmu_is_slave_side_secure(smmu_domain)) {
		arm_smmu_disable_clocks(smmu_domain->smmu);
		mutex_unlock(&smmu_domain->init_mutex);
	}
	return ret;
}

static size_t arm_smmu_unmap(struct iommu_domain *domain, unsigned long iova,
			     size_t size)
{
	size_t ret;
	unsigned long flags;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct io_pgtable_ops *ops= smmu_domain->pgtbl_ops;
	int atomic_ctx = smmu_domain->attributes & (1 << DOMAIN_ATTR_ATOMIC);

	if (!ops)
		return 0;

	if (arm_smmu_is_slave_side_secure(smmu_domain) && atomic_ctx) {
		dev_err(smmu_domain->smmu->dev,
				"Slave side atomic context not supported\n");
		return 0;
	}

	/*
	 * The contract here is that if you set DOMAIN_ATTR_ATOMIC your
	 * domain *must* must be attached an SMMU during unmap.  This
	 * function calls other functions that try to use smmu_domain->smmu
	 * if it's not NULL (like the tlb invalidation routines).  So if
	 * the client sets DOMAIN_ATTR_ATOMIC and detaches in the middle of
	 * the unmap the smmu instance could go away and we could
	 * dereference NULL.  This little BUG_ON should catch most gross
	 * offenders but if atomic clients violate this contract then this
	 * code is racy.
	 */
	BUG_ON(atomic_ctx && !smmu_domain->smmu);

	if (atomic_ctx) {
		if (arm_smmu_enable_clocks_atomic(smmu_domain->smmu))
			return 0;
	} else {
		mutex_lock(&smmu_domain->init_mutex);
		arm_smmu_secure_domain_lock(smmu_domain);
		if (smmu_domain->smmu &&
		    arm_smmu_enable_clocks(smmu_domain->smmu)) {
			arm_smmu_secure_domain_unlock(smmu_domain);
			mutex_unlock(&smmu_domain->init_mutex);
			return 0;
		}
	}

	flags = arm_smmu_pgtbl_lock(smmu_domain);
	ret = ops->unmap(ops, iova, size);
	arm_smmu_pgtbl_unlock(smmu_domain, flags);

	/*
	 * While splitting up block mappings, we might allocate page table
	 * memory during unmap, so the vmids needs to be assigned to the
	 * memory here as well.
	 */
	if (arm_smmu_assign_table(smmu_domain)) {
		arm_smmu_unassign_table(smmu_domain);
		arm_smmu_secure_domain_unlock(smmu_domain);
		mutex_unlock(&smmu_domain->init_mutex);
		return 0;
	}

	/* Also unassign any pages that were free'd during unmap */
	arm_smmu_unassign_table(smmu_domain);

	if (atomic_ctx) {
		arm_smmu_disable_clocks_atomic(smmu_domain->smmu);
	} else {
		if (smmu_domain->smmu)
			arm_smmu_disable_clocks(smmu_domain->smmu);
		arm_smmu_secure_domain_unlock(smmu_domain);
		mutex_unlock(&smmu_domain->init_mutex);
	}

	return ret;
}

static phys_addr_t arm_smmu_iova_to_phys(struct iommu_domain *domain,
					 dma_addr_t iova)
{
	phys_addr_t ret;
	unsigned long flags;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct io_pgtable_ops *ops= smmu_domain->pgtbl_ops;

	if (!ops)
		return 0;

	flags = arm_smmu_pgtbl_lock(smmu_domain);
	ret = ops->iova_to_phys(ops, iova);
	arm_smmu_pgtbl_unlock(smmu_domain, flags);
	return ret;
}

static int arm_smmu_wait_for_halt(struct arm_smmu_device *smmu)
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

static int __arm_smmu_halt(struct arm_smmu_device *smmu, bool wait)
{
	u32 reg;
	void __iomem *impl_def1_base = ARM_SMMU_IMPL_DEF1(smmu);

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

	return wait ? arm_smmu_wait_for_halt(smmu) : 0;
}

static int arm_smmu_halt(struct arm_smmu_device *smmu)
{
	return __arm_smmu_halt(smmu, true);
}

static int arm_smmu_halt_nowait(struct arm_smmu_device *smmu)
{
	return __arm_smmu_halt(smmu, false);
}

static void arm_smmu_resume(struct arm_smmu_device *smmu)
{
	void __iomem *impl_def1_base = ARM_SMMU_IMPL_DEF1(smmu);
	u32 reg;

	if (arm_smmu_restore_sec_cfg(smmu))
		return;

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

static phys_addr_t __arm_smmu_iova_to_phys_hard(struct iommu_domain *domain,
						dma_addr_t iova, bool do_halt)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct device *dev = smmu->dev;
	void __iomem *cb_base;
	u32 tmp;
	u64 phys;
	unsigned long flags;

	if (arm_smmu_enable_clocks(smmu))
		return 0;

	cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);

	spin_lock_irqsave(&smmu->atos_lock, flags);

	if (do_halt && arm_smmu_halt(smmu))
		goto err_unlock;

	if (smmu->version == 1)
		writel_relaxed(iova & ~0xfff, cb_base + ARM_SMMU_CB_ATS1PR_LO);
	else
		writeq_relaxed(iova & ~0xfffULL,
			       cb_base + ARM_SMMU_CB_ATS1PR_LO);

	if (readl_poll_timeout_atomic(cb_base + ARM_SMMU_CB_ATSR, tmp,
				      !(tmp & ATSR_ACTIVE), 5, 50)) {
		dev_err(dev, "iova to phys timed out\n");
		goto err_resume;
	}

	phys = readl_relaxed(cb_base + ARM_SMMU_CB_PAR_LO);
	phys |= ((u64) readl_relaxed(cb_base + ARM_SMMU_CB_PAR_HI)) << 32;

	if (do_halt)
		arm_smmu_resume(smmu);
	spin_unlock_irqrestore(&smmu->atos_lock, flags);

	if (phys & CB_PAR_F) {
		dev_err(dev, "translation fault on %s!\n", dev_name(dev));
		dev_err(dev, "PAR = 0x%llx\n", phys);
		phys = 0;
	} else {
		phys = (phys & (PHYS_MASK & ~0xfffULL)) | (iova & 0xfff);
	}

	arm_smmu_disable_clocks(smmu);
	return phys;

err_resume:
	if (do_halt)
		arm_smmu_resume(smmu);
err_unlock:
	spin_unlock_irqrestore(&smmu->atos_lock, flags);
	arm_smmu_disable_clocks(smmu);
	phys = arm_smmu_iova_to_phys(domain, iova);
	dev_err(dev,
		"iova to phys failed 0x%pa. software table walk result=%pa.\n",
		&iova, &phys);
	return 0;
}

static phys_addr_t arm_smmu_iova_to_phys_hard(struct iommu_domain *domain,
					      dma_addr_t iova)
{
	return __arm_smmu_iova_to_phys_hard(domain, iova, true);
}

static phys_addr_t arm_smmu_iova_to_phys_hard_no_halt(
	struct iommu_domain *domain, dma_addr_t iova)
{
	return __arm_smmu_iova_to_phys_hard(domain, iova, false);
}

static unsigned long arm_smmu_reg_read(struct iommu_domain *domain,
				       unsigned long offset)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_device *smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	void __iomem *cb_base;
	unsigned long val;

	if (offset >= SZ_4K) {
		pr_err("Invalid offset: 0x%lx\n", offset);
		return 0;
	}

	mutex_lock(&smmu_domain->init_mutex);
	smmu = smmu_domain->smmu;
	if (!smmu) {
		WARN(1, "Can't read registers of a detached domain\n");
		val = 0;
		goto unlock;
	}

	cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);
	if (arm_smmu_enable_clocks(smmu)) {
		val = 0;
		goto unlock;
	}
	val = readl_relaxed(cb_base + offset);
	arm_smmu_disable_clocks(smmu);

unlock:
	mutex_unlock(&smmu_domain->init_mutex);
	return val;
}

static void arm_smmu_reg_write(struct iommu_domain *domain,
			       unsigned long offset, unsigned long val)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;
	struct arm_smmu_device *smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	void __iomem *cb_base;

	if (offset >= SZ_4K) {
		pr_err("Invalid offset: 0x%lx\n", offset);
		return;
	}

	mutex_lock(&smmu_domain->init_mutex);
	smmu = smmu_domain->smmu;
	if (!smmu) {
		WARN(1, "Can't read registers of a detached domain\n");
		goto unlock;
	}

	cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, cfg->cbndx);
	if (arm_smmu_enable_clocks(smmu))
		goto unlock;
	writel_relaxed(val, cb_base + offset);
	arm_smmu_disable_clocks(smmu);
unlock:
	mutex_unlock(&smmu_domain->init_mutex);
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
	case IOMMU_CAP_INTR_REMAP:
		return true; /* MSIs are just memory writes */
	default:
		return false;
	}
}

static void __arm_smmu_release_pci_iommudata(void *data)
{
	kfree(data);
}

static int arm_smmu_add_device(struct device *dev)
{
	struct arm_smmu_device *smmu;
	struct arm_smmu_master_cfg *cfg;
	struct iommu_group *group;
	void (*releasefn)(void *) = NULL;
	int ret;

	smmu = find_smmu_for_device(dev);
	if (!smmu)
		return -ENODEV;

	group = iommu_group_alloc();
	if (IS_ERR(group)) {
		dev_err(dev, "Failed to allocate IOMMU group\n");
		return PTR_ERR(group);
	}

	if (dev_is_pci(dev)) {
		u32 sid;
		int tmp;

		cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
		if (!cfg) {
			ret = -ENOMEM;
			goto out_put_group;
		}

		cfg->num_streamids = 1;
		ret = msm_pcie_configure_sid(dev, &sid, &tmp);
		if (ret) {
			dev_err(dev,
				"Couldn't configure SID through PCI-e driver: %d\n",
				ret);
			kfree(cfg);
			goto out_put_group;
		}
		cfg->streamids[0] = sid;
		releasefn = __arm_smmu_release_pci_iommudata;
	} else {
		struct arm_smmu_master *master;

		master = find_smmu_master(smmu, dev->of_node);
		if (!master) {
			ret = -ENODEV;
			goto out_put_group;
		}

		cfg = &master->cfg;
	}

	iommu_group_set_iommudata(group, cfg, releasefn);
	ret = iommu_group_add_device(group, dev);

out_put_group:
	iommu_group_put(group);
	return ret;
}

static void arm_smmu_remove_device(struct device *dev)
{
	iommu_group_remove_device(dev);
}

static int arm_smmu_domain_get_attr(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data)
{
	int ret;
	struct arm_smmu_domain *smmu_domain = domain->priv;

	mutex_lock(&smmu_domain->init_mutex);
	switch (attr) {
	case DOMAIN_ATTR_NESTING:
		*(int *)data = (smmu_domain->stage == ARM_SMMU_DOMAIN_NESTED);
		ret = 0;
		break;
	case DOMAIN_ATTR_COHERENT_HTW_DISABLE:
		*((int *)data) = !!(smmu_domain->attributes &
				(1 << DOMAIN_ATTR_COHERENT_HTW_DISABLE));
		ret = 0;
		break;
	case DOMAIN_ATTR_SECURE_VMID:
		*((int *)data) = smmu_domain->secure_vmid;
		ret = 0;
		break;
	case DOMAIN_ATTR_PT_BASE_ADDR:
		*((phys_addr_t *)data) =
			smmu_domain->pgtbl_cfg.arm_lpae_s1_cfg.ttbr[0];
		ret = 0;
		break;
	case DOMAIN_ATTR_CONTEXT_BANK:
		/* context bank index isn't valid until we are attached */
		if (smmu_domain->smmu == NULL)
			return -ENODEV;

		*((unsigned int *) data) = smmu_domain->cfg.cbndx;
		ret = 0;
		break;
	case DOMAIN_ATTR_TTBR0: {
		u64 val;
		/* not valid until we are attached */
		if (smmu_domain->smmu == NULL)
			return -ENODEV;

		val = smmu_domain->pgtbl_cfg.arm_lpae_s1_cfg.ttbr[0];
		if (smmu_domain->cfg.cbar != CBAR_TYPE_S2_TRANS)
			val |= (u64)ARM_SMMU_CB_ASID(&smmu_domain->cfg)
					<< (32ULL + TTBRn_HI_ASID_SHIFT);
		*((u64 *)data) = val;
		ret = 0;
		break;
	}
	case DOMAIN_ATTR_CONTEXTIDR:
		/* not valid until attached */
		if (smmu_domain->smmu == NULL)
			return -ENODEV;
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
	case DOMAIN_ATTR_FAST:
		*((int *)data) = !!(smmu_domain->attributes
					& (1 << DOMAIN_ATTR_FAST));
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
	struct arm_smmu_domain *smmu_domain = domain->priv;

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
	case DOMAIN_ATTR_COHERENT_HTW_DISABLE:
	{
		struct arm_smmu_device *smmu;
		int htw_disable = *((int *)data);

		smmu = smmu_domain->smmu;

		if (smmu && !(smmu->features & ARM_SMMU_FEAT_COHERENT_WALK)
			&& !htw_disable) {
			dev_err(smmu->dev,
				"Can't enable coherent htw on this domain: this SMMU doesn't support it\n");
			ret = -EINVAL;
			goto out_unlock;
		}

		if (htw_disable)
			smmu_domain->attributes |=
				(1 << DOMAIN_ATTR_COHERENT_HTW_DISABLE);
		else
			smmu_domain->attributes &=
				~(1 << DOMAIN_ATTR_COHERENT_HTW_DISABLE);
		break;
	}
	case DOMAIN_ATTR_SECURE_VMID:
		BUG_ON(smmu_domain->secure_vmid != VMID_INVAL);
		smmu_domain->secure_vmid = *((int *)data);
		break;
	case DOMAIN_ATTR_ATOMIC:
	{
		int atomic_ctx = *((int *)data);
		if (atomic_ctx)
			smmu_domain->attributes |= (1 << DOMAIN_ATTR_ATOMIC);
		else
			smmu_domain->attributes &= ~(1 << DOMAIN_ATTR_ATOMIC);
		break;
	}
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
	case DOMAIN_ATTR_NON_FATAL_FAULTS:
		smmu_domain->non_fatal_faults = *((int *)data);
		ret = 0;
		break;
	case DOMAIN_ATTR_S1_BYPASS: {
		int bypass = *((int *)data);

		if (bypass)
			smmu_domain->attributes |= 1 << DOMAIN_ATTR_S1_BYPASS;
		else
			smmu_domain->attributes &=
					~(1 << DOMAIN_ATTR_S1_BYPASS);

		ret = 0;
		break;
	}
	case DOMAIN_ATTR_FAST:
		if (*((int *)data))
			smmu_domain->attributes |= 1 << DOMAIN_ATTR_FAST;
		ret = 0;
		break;
	default:
		ret = -ENODEV;
		break;
	}

out_unlock:
	mutex_unlock(&smmu_domain->init_mutex);
	return ret;
}

static int arm_smmu_dma_supported(struct iommu_domain *domain,
				  struct device *dev, u64 mask)
{
	struct arm_smmu_device *smmu;
	struct arm_smmu_domain *smmu_domain = domain->priv;
	int ret;

	mutex_lock(&smmu_domain->init_mutex);
	smmu = smmu_domain->smmu;
	if (!smmu) {
		dev_err(dev,
			"Can't call dma_supported on an unattached domain\n");
		mutex_unlock(&smmu_domain->init_mutex);
		return 0;
	}

	ret = ((1ULL << smmu->va_size) - 1) <= mask ? 0 : 1;
	mutex_unlock(&smmu_domain->init_mutex);
	return ret;
}

static unsigned long arm_smmu_get_pgsize_bitmap(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = domain->priv;

	/*
	 * if someone is calling map before attach just return the
	 * supported page sizes for the hardware itself.
	 */
	if (!smmu_domain->pgtbl_cfg.pgsize_bitmap)
		return arm_smmu_ops.pgsize_bitmap;
	/*
	 * otherwise return the page sizes supported by this specific page
	 * table configuration
	 */
	return smmu_domain->pgtbl_cfg.pgsize_bitmap;
}

static struct iommu_ops arm_smmu_ops = {
	.capable		= arm_smmu_capable,
	.domain_init		= arm_smmu_domain_init,
	.domain_destroy		= arm_smmu_domain_destroy,
	.attach_dev		= arm_smmu_attach_dev,
	.detach_dev		= arm_smmu_detach_dev,
	.map			= arm_smmu_map,
	.unmap			= arm_smmu_unmap,
	.map_sg			= arm_smmu_map_sg,
	.iova_to_phys		= arm_smmu_iova_to_phys,
	.iova_to_phys_hard	= arm_smmu_iova_to_phys_hard,
	.add_device		= arm_smmu_add_device,
	.remove_device		= arm_smmu_remove_device,
	.domain_get_attr	= arm_smmu_domain_get_attr,
	.domain_set_attr	= arm_smmu_domain_set_attr,
	.pgsize_bitmap		= -1UL, /* Restricted during device attach */
	.get_pgsize_bitmap	= arm_smmu_get_pgsize_bitmap,
	.dma_supported		= arm_smmu_dma_supported,
	.trigger_fault		= arm_smmu_trigger_fault,
	.reg_read		= arm_smmu_reg_read,
	.reg_write		= arm_smmu_reg_write,
	.tlbi_domain		= arm_smmu_tlbi_domain,
	.enable_config_clocks	= arm_smmu_enable_config_clocks,
	.disable_config_clocks	= arm_smmu_disable_config_clocks,
};

static void arm_smmu_device_reset(struct arm_smmu_device *smmu)
{
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);
	void __iomem *cb_base;
	int i = 0;
	u32 reg;

	/* clear global FSR */
	reg = readl_relaxed(ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sGFSR);
	writel(reg, ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sGFSR);

	if (!(smmu->options & ARM_SMMU_OPT_SKIP_INIT)) {
		/* Mark all SMRn as invalid and all S2CRn as bypass */
		for (i = 0; i < smmu->num_mapping_groups; ++i) {
			writel_relaxed(0,
				gr0_base + ARM_SMMU_GR0_SMR(i));
			writel_relaxed(S2CR_TYPE_BYPASS,
				gr0_base + ARM_SMMU_GR0_S2CR(i));
		}

		/* Make sure all context banks are disabled and clear CB_FSR  */
		for (i = 0; i < smmu->num_context_banks; ++i) {
			cb_base = ARM_SMMU_CB_BASE(smmu) + ARM_SMMU_CB(smmu, i);
			writel_relaxed(0, cb_base + ARM_SMMU_CB_SCTLR);
			writel_relaxed(FSR_FAULT, cb_base + ARM_SMMU_CB_FSR);
		}
	}

	/* Invalidate the TLB, just in case */
	writel_relaxed(0, gr0_base + ARM_SMMU_GR0_TLBIALLH);
	writel_relaxed(0, gr0_base + ARM_SMMU_GR0_TLBIALLNSNH);

	reg = readl_relaxed(ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sCR0);

	/* Enable fault reporting */
	reg |= (sCR0_GFRE | sCR0_GFIE | sCR0_GCFGFRE | sCR0_GCFGFIE);

	/* Disable TLB broadcasting. */
	reg |= (sCR0_VMIDPNE | sCR0_PTM);

	/* Enable client access */
	reg &= ~sCR0_CLIENTPD;

	/* Raise an unidentified stream fault on unmapped access */
	reg |= sCR0_USFCFG;

	/* Disable forced broadcasting */
	reg &= ~sCR0_FB;

	/* Don't upgrade barriers */
	reg &= ~(sCR0_BSU_MASK << sCR0_BSU_SHIFT);

	/* Push the button */
	__arm_smmu_tlb_sync(smmu);
	writel(reg, ARM_SMMU_GR0_NS(smmu) + ARM_SMMU_GR0_sCR0);
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

static int regulator_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	int ret = 0;
	struct arm_smmu_device *smmu = container_of(nb,
					struct arm_smmu_device, regulator_nb);

	/* Ignore EVENT DISABLE as no clocks could be turned on
	 * at this notification.
	*/
	if (event != REGULATOR_EVENT_PRE_DISABLE &&
				event != REGULATOR_EVENT_ENABLE)
		return NOTIFY_OK;

	ret = arm_smmu_prepare_clocks(smmu);
	if (ret)
		goto out;

	ret = arm_smmu_enable_clocks_atomic(smmu);
	if (ret)
		goto unprepare_clock;

	if (event == REGULATOR_EVENT_PRE_DISABLE)
		arm_smmu_halt(smmu);
	else if (event == REGULATOR_EVENT_ENABLE)
		arm_smmu_resume(smmu);

	arm_smmu_disable_clocks_atomic(smmu);
unprepare_clock:
	arm_smmu_unprepare_clocks(smmu);
out:
	return NOTIFY_OK;
}

static int register_regulator_notifier(struct arm_smmu_device *smmu)
{
	struct device *dev = smmu->dev;
	int ret = 0;

	if (smmu->options & ARM_SMMU_OPT_HALT) {
		smmu->regulator_nb.notifier_call = regulator_notifier;
		ret = regulator_register_notifier(smmu->gdsc,
						&smmu->regulator_nb);

		if (ret)
			dev_err(dev, "Regulator notifier request failed\n");
	}
	return ret;
}

static int arm_smmu_init_regulators(struct arm_smmu_device *smmu)
{
	struct device *dev = smmu->dev;

	if (!of_get_property(dev->of_node, "vdd-supply", NULL))
		return 0;

	smmu->gdsc = devm_regulator_get(dev, "vdd");
	if (IS_ERR(smmu->gdsc))
		return PTR_ERR(smmu->gdsc);

	return 0;
}

static int arm_smmu_init_clocks(struct arm_smmu_device *smmu)
{
	const char *cname;
	struct property *prop;
	int i;
	struct device *dev = smmu->dev;

	smmu->num_clocks =
		of_property_count_strings(dev->of_node, "clock-names");

	if (smmu->num_clocks < 1)
		return 0;

	smmu->clocks = devm_kzalloc(
		dev, sizeof(*smmu->clocks) * smmu->num_clocks,
		GFP_KERNEL);

	if (!smmu->clocks) {
		dev_err(dev,
			"Failed to allocate memory for clocks\n");
		return -ENODEV;
	}

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

		smmu->clocks[i] = c;

		++i;
	}
	return 0;
}

static int arm_smmu_init_bus_scaling(struct platform_device *pdev,
				     struct arm_smmu_device *smmu)
{
	u32 master_id;

	if (of_property_read_u32(pdev->dev.of_node, "qcom,bus-master-id",
				 &master_id)) {
		dev_dbg(smmu->dev, "No bus scaling info\n");
		return 0;
	}

	smmu->bus_client_name = devm_kasprintf(
		smmu->dev, GFP_KERNEL, "smmu-bus-client-%s",
		dev_name(smmu->dev));

	if (!smmu->bus_client_name)
		return -ENOMEM;

	smmu->bus_client = msm_bus_scale_register(
		master_id, MSM_BUS_SLAVE_EBI_CH0, smmu->bus_client_name, true);
	if (IS_ERR(&smmu->bus_client)) {
		int ret = PTR_ERR(smmu->bus_client);

		if (ret != -EPROBE_DEFER)
			dev_err(smmu->dev, "Bus client registration failed\n");
		return ret;
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

static int arm_smmu_device_cfg_probe(struct arm_smmu_device *smmu)
{
	unsigned long size;
	void __iomem *gr0_base = ARM_SMMU_GR0(smmu);
	u32 id;

	if (arm_smmu_restore_sec_cfg(smmu))
		return -ENODEV;

	dev_dbg(smmu->dev, "probing hardware configuration...\n");
	dev_dbg(smmu->dev, "SMMUv%d with:\n", smmu->version);

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
		dev_err(smmu->dev, "\tno translation support (id0=%x)!\n", id);
		return -ENODEV;
	}

	if (smmu->version == 1 || (!(id & ID0_ATOSNS) && (id & ID0_S1TS))) {
		smmu->features |= ARM_SMMU_FEAT_TRANS_OPS;
		dev_dbg(smmu->dev, "\taddress translation ops\n");
	}

	if (id & ID0_CTTW) {
		smmu->features |= ARM_SMMU_FEAT_COHERENT_WALK;
		dev_dbg(smmu->dev, "\tcoherent table walk\n");
	}

	if (id & ID0_SMS) {
		u32 smr, sid, mask;

		smmu->features |= ARM_SMMU_FEAT_STREAM_MATCH;
		smmu->num_mapping_groups = (id >> ID0_NUMSMRG_SHIFT) &
					   ID0_NUMSMRG_MASK;
		if (smmu->num_mapping_groups == 0) {
			dev_err(smmu->dev,
				"stream-matching supported, but no SMRs present!\n");
			return -ENODEV;
		}

		if (!(smmu->options & ARM_SMMU_OPT_NO_SMR_CHECK)) {
			smr = SMR_MASK_MASK << SMR_MASK_SHIFT;
			smr |= (SMR_ID_MASK << SMR_ID_SHIFT);
			writel_relaxed(smr, gr0_base + ARM_SMMU_GR0_SMR(0));
			smr = readl_relaxed(gr0_base + ARM_SMMU_GR0_SMR(0));

			mask = (smr >> SMR_MASK_SHIFT) & SMR_MASK_MASK;
			sid = (smr >> SMR_ID_SHIFT) & SMR_ID_MASK;
			if ((mask & sid) != sid) {
				dev_err(smmu->dev,
					"SMR mask bits (0x%x) insufficient for ID field (0x%x)\n",
					mask, sid);
				return -ENODEV;
			}
		}

		dev_dbg(smmu->dev,
			"\tstream matching with %u register groups, mask 0x%x",
			smmu->num_mapping_groups, mask);
	} else {
		smmu->num_mapping_groups = (id >> ID0_NUMSIDB_SHIFT) &
					   ID0_NUMSIDB_MASK;
	}

	/* ID1 */
	id = readl_relaxed(gr0_base + ARM_SMMU_GR0_ID1);
	smmu->pgshift = (id & ID1_PAGESIZE) ? 16 : 12;

	/* Check for size mismatch of SMMU address space from mapped region */
	size = 1 << (((id >> ID1_NUMPAGENDXB_SHIFT) & ID1_NUMPAGENDXB_MASK) + 1);
	size *= 2 << smmu->pgshift;
	if (smmu->size != size)
		dev_warn(smmu->dev,
			"SMMU address space size (0x%lx) differs from mapped region size (0x%lx)!\n",
			size, smmu->size);

	smmu->num_s2_context_banks = (id >> ID1_NUMS2CB_SHIFT) & ID1_NUMS2CB_MASK;
	smmu->num_context_banks = (id >> ID1_NUMCB_SHIFT) & ID1_NUMCB_MASK;
	if (smmu->num_s2_context_banks > smmu->num_context_banks) {
		dev_err(smmu->dev, "impossible number of S2 context banks!\n");
		return -ENODEV;
	}
	dev_dbg(smmu->dev, "\t%u context banks (%u stage-2 only)\n",
		smmu->num_context_banks, smmu->num_s2_context_banks);

	/* ID2 */
	id = readl_relaxed(gr0_base + ARM_SMMU_GR0_ID2);
	size = arm_smmu_id_size_to_bits((id >> ID2_IAS_SHIFT) & ID2_IAS_MASK);
	smmu->ipa_size = size;

	/* The output mask is also applied for bypass */
	size = arm_smmu_id_size_to_bits((id >> ID2_OAS_SHIFT) & ID2_OAS_MASK);
	smmu->pa_size = size;

	/*
	 * What the page table walker can address actually depends on which
	 * descriptor format is in use, but since a) we don't know that yet,
	 * and b) it can vary per context bank, this will have to do...
	 */
	dma_set_mask_and_coherent(smmu->dev, DMA_BIT_MASK(size));

	if (smmu->version == ARM_SMMU_V1) {
		smmu->va_size = smmu->ipa_size;
		size = SZ_4K | SZ_2M | SZ_1G;
	} else {
		size = (id >> ID2_UBS_SHIFT) & ID2_UBS_MASK;
		smmu->va_size = arm_smmu_id_size_to_bits(size);
#ifndef CONFIG_64BIT
		smmu->va_size = min(32UL, smmu->va_size);
#endif
		smmu->va_size = min(36UL, smmu->va_size);
		size = 0;
		if (id & ID2_PTFS_4K)
			size |= SZ_4K | SZ_2M | SZ_1G;
		if (id & ID2_PTFS_16K)
			size |= SZ_16K | SZ_32M;
		if (id & ID2_PTFS_64K)
			size |= SZ_64K | SZ_512M;
	}

	arm_smmu_ops.pgsize_bitmap &= size;
	dev_dbg(smmu->dev, "\tSupported page sizes: 0x%08lx\n", size);

	if (smmu->features & ARM_SMMU_FEAT_TRANS_S1)
		dev_dbg(smmu->dev, "\tStage-1: %lu-bit VA -> %lu-bit IPA\n",
			smmu->va_size, smmu->ipa_size);

	if (smmu->features & ARM_SMMU_FEAT_TRANS_S2)
		dev_dbg(smmu->dev, "\tStage-2: %lu-bit IPA -> %lu-bit PA\n",
			smmu->ipa_size, smmu->pa_size);

	return 0;
}

static const struct of_device_id arm_smmu_of_match[] = {
	{ .compatible = "arm,smmu-v1", .data = (void *)ARM_SMMU_V1 },
	{ .compatible = "arm,smmu-v2", .data = (void *)ARM_SMMU_V2 },
	{ .compatible = "arm,mmu-400", .data = (void *)ARM_SMMU_V1 },
	{ .compatible = "arm,mmu-401", .data = (void *)ARM_SMMU_V1 },
	{ .compatible = "arm,mmu-500", .data = (void *)ARM_SMMU_V2 },
	{ .compatible = "qcom,smmu-v2", .data = (void *)ARM_SMMU_V2 },
	{ },
};
MODULE_DEVICE_TABLE(of, arm_smmu_of_match);

static int arm_smmu_device_dt_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id;
	struct resource *res;
	struct arm_smmu_device *smmu;
	struct device *dev = &pdev->dev;
	struct rb_node *node;
	int num_irqs, i, err, num_masters;

	smmu = devm_kzalloc(dev, sizeof(*smmu), GFP_KERNEL);
	if (!smmu) {
		dev_err(dev, "failed to allocate arm_smmu_device\n");
		return -ENOMEM;
	}
	smmu->dev = dev;
	mutex_init(&smmu->attach_lock);
	spin_lock_init(&smmu->atos_lock);
	spin_lock_init(&smmu->clock_refs_lock);

	of_id = of_match_node(arm_smmu_of_match, dev->of_node);
	if (!of_id)
		return -ENODEV;
	smmu->version = (enum arm_smmu_arch_version)of_id->data;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	smmu->phys_addr = res->start;
	smmu->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(smmu->base))
		return PTR_ERR(smmu->base);
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

	i = 0;
	smmu->masters = RB_ROOT;
	err = arm_smmu_parse_iommus_properties(smmu, &num_masters);
	if (err)
		goto out_put_masters;

	dev_dbg(dev, "registered %d master devices\n", num_masters);

	err = arm_smmu_parse_impl_def_registers(smmu);
	if (err)
		goto out_put_masters;

	err = arm_smmu_init_regulators(smmu);
	if (err)
		goto out_put_masters;

	err = arm_smmu_init_clocks(smmu);
	if (err)
		goto out_put_masters;

	err = arm_smmu_init_bus_scaling(pdev, smmu);
	if (err)
		goto out_put_masters;

	parse_driver_options(smmu);

	err = arm_smmu_enable_clocks(smmu);
	if (err)
		goto out_put_masters;

	smmu->sec_id = msm_dev_to_device_id(dev);
	err = arm_smmu_device_cfg_probe(smmu);
	arm_smmu_disable_clocks(smmu);
	if (err)
		goto out_put_masters;

	if (of_device_is_compatible(dev->of_node, "qcom,smmu-v2"))
		smmu->model = SMMU_MODEL_QCOM_V2;

	if (smmu->version > ARM_SMMU_V1 &&
	    smmu->num_context_banks != smmu->num_context_irqs) {
		dev_err(dev,
			"found %d context interrupt(s) but have %d context banks. assuming %d context interrupts.\n",
			smmu->num_context_irqs, smmu->num_context_banks,
			smmu->num_context_banks);
		smmu->num_context_irqs = smmu->num_context_banks;
	}

	for (i = 0; i < smmu->num_global_irqs; ++i) {
		err = request_threaded_irq(smmu->irqs[i],
					NULL, arm_smmu_global_fault,
					IRQF_ONESHOT | IRQF_SHARED,
					"arm-smmu global fault", smmu);
		if (err) {
			dev_err(dev, "failed to request global IRQ %d (%u)\n",
				i, smmu->irqs[i]);
			goto out_free_irqs;
		}
	}

	idr_init(&smmu->asid_idr);

	err = register_regulator_notifier(smmu);
	if (err)
		goto out_free_irqs;

	INIT_LIST_HEAD(&smmu->list);
	spin_lock(&arm_smmu_devices_lock);
	list_add(&smmu->list, &arm_smmu_devices);
	spin_unlock(&arm_smmu_devices_lock);

	return 0;

out_free_irqs:
	while (i--)
		free_irq(smmu->irqs[i], smmu);

out_put_masters:
	for (node = rb_first(&smmu->masters); node; node = rb_next(node)) {
		struct arm_smmu_master *master
			= container_of(node, struct arm_smmu_master, node);
		of_node_put(master->of_node);
	}

	return err;
}

static int arm_smmu_device_remove(struct platform_device *pdev)
{
	int i;
	struct device *dev = &pdev->dev;
	struct arm_smmu_device *curr, *smmu = NULL;
	struct rb_node *node;

	spin_lock(&arm_smmu_devices_lock);
	list_for_each_entry(curr, &arm_smmu_devices, list) {
		if (curr->dev == dev) {
			smmu = curr;
			list_del(&smmu->list);
			break;
		}
	}
	spin_unlock(&arm_smmu_devices_lock);

	if (!smmu)
		return -ENODEV;

	for (node = rb_first(&smmu->masters); node; node = rb_next(node)) {
		struct arm_smmu_master *master
			= container_of(node, struct arm_smmu_master, node);
		of_node_put(master->of_node);
	}

	if (!bitmap_empty(smmu->context_map, ARM_SMMU_MAX_CBS))
		dev_err(dev, "removing device with active domains!\n");

	for (i = 0; i < smmu->num_global_irqs; ++i)
		free_irq(smmu->irqs[i], smmu);

	mutex_lock(&smmu->attach_lock);
	idr_destroy(&smmu->asid_idr);
	/*
	 * If all devices weren't detached for some reason, we're
	 * still powered on. Power off now.
	 */
	if (smmu->attach_count)
		arm_smmu_power_off(smmu, false);
	mutex_unlock(&smmu->attach_lock);

	msm_bus_scale_unregister(smmu->bus_client);

	return 0;
}

static struct platform_driver arm_smmu_driver = {
	.driver	= {
		.owner		= THIS_MODULE,
		.name		= "arm-smmu",
		.of_match_table	= of_match_ptr(arm_smmu_of_match),
	},
	.probe	= arm_smmu_device_dt_probe,
	.remove	= arm_smmu_device_remove,
};

static int __init arm_smmu_init(void)
{
	struct device_node *np;
	int ret;

	/*
	 * Play nice with systems that don't have an ARM SMMU by checking that
	 * an ARM SMMU exists in the system before proceeding with the driver
	 * and IOMMU bus operation registration.
	 */
	np = of_find_matching_node(NULL, arm_smmu_of_match);
	if (!np)
		return 0;

	of_node_put(np);

	ret = platform_driver_register(&arm_smmu_driver);
	if (ret)
		return ret;

	/* Oh, for a proper bus abstraction */
	if (!iommu_present(&platform_bus_type))
		bus_set_iommu(&platform_bus_type, &arm_smmu_ops);

#ifdef CONFIG_ARM_AMBA
	if (!iommu_present(&amba_bustype))
		bus_set_iommu(&amba_bustype, &arm_smmu_ops);
#endif

#ifdef CONFIG_PCI
	if (!iommu_present(&pci_bus_type))
		bus_set_iommu(&pci_bus_type, &arm_smmu_ops);
#endif

	return 0;
}

static void __exit arm_smmu_exit(void)
{
	return platform_driver_unregister(&arm_smmu_driver);
}

subsys_initcall(arm_smmu_init);
module_exit(arm_smmu_exit);

MODULE_DESCRIPTION("IOMMU API for ARM architected SMMU implementations");
MODULE_AUTHOR("Will Deacon <will.deacon@arm.com>");
MODULE_LICENSE("GPL v2");
