// SPDX-License-Identifier: GPL-2.0-only
/*
 * IOMMU API for ARM architected SMMU implementations.
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
#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-iommu.h>
#include <linux/dma-mapping.h>
#include <linux/dma-mapping-fast.h>
#include <linux/dma-noncoherent.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/interconnect.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_iommu.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/of_platform.h>
#include <linux/irq.h>
#include <linux/wait.h>

#include <linux/amba/bus.h>
#include <linux/fsl/mc.h>

#include "arm-smmu.h"
#include "iommu-logger.h"

#define CREATE_TRACE_POINTS
#include "arm-smmu-trace.h"

/*
 * Apparently, some Qualcomm arm64 platforms which appear to expose their SMMU
 * global register space are still, in fact, using a hypervisor to mediate it
 * by trapping and emulating register accesses. Sadly, some deployed versions
 * of said trapping code have bugs wherein they go horribly wrong for stores
 * using r31 (i.e. XZR/WZR) as the source register.
 */
#define QCOM_DUMMY_VAL -1

#define ARM_MMU500_ACR_CACHE_LOCK	(1 << 26)

#define TLB_LOOP_TIMEOUT		500000	/* 500ms */
#define TLB_LOOP_INC_MAX		1000      /*1ms*/

#define MSI_IOVA_BASE			0x8000000
#define MSI_IOVA_LENGTH			0x100000

#define ARM_SMMU_ICC_AVG_BW		0
#define ARM_SMMU_ICC_PEAK_BW_HIGH	1000
#define ARM_SMMU_ICC_PEAK_BW_LOW	0
#define ARM_SMMU_ICC_ACTIVE_ONLY_TAG	0x3

static int force_stage;
module_param(force_stage, int, S_IRUGO);
MODULE_PARM_DESC(force_stage,
	"Force SMMU mappings to be installed at a particular stage of translation. A value of '1' or '2' forces the corresponding stage. All other values are ignored (i.e. no stage is forced). Note that selecting a specific stage will disable support for nested translation.");
static bool disable_bypass =
	IS_ENABLED(CONFIG_ARM_SMMU_DISABLE_BYPASS_BY_DEFAULT);
module_param(disable_bypass, bool, S_IRUGO);
MODULE_PARM_DESC(disable_bypass,
	"Disable bypass streams such that incoming transactions from devices that are not attached to an iommu domain will report an abort back to the device and will not be allowed to pass through the SMMU.");

#define s2cr_init_val (struct arm_smmu_s2cr){				\
	.type = disable_bypass ? S2CR_TYPE_FAULT : S2CR_TYPE_BYPASS,	\
	.cb_handoff = false,						\
}

struct arm_smmu_cb {
	u64				ttbr[2];
	u32				tcr[2];
	u32				mair[2];
	struct arm_smmu_cfg		*cfg;
};

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

#define TCU_TESTBUS_SEL_ALL 0x3
#define TBU_TESTBUS_SEL_ALL 0xf

int tbu_testbus_sel = TBU_TESTBUS_SEL_ALL;
int tcu_testbus_sel = TCU_TESTBUS_SEL_ALL;

module_param_named(tcu_testbus_sel, tcu_testbus_sel, int, 0644);
module_param_named(tbu_testbus_sel, tbu_testbus_sel, int, 0644);

struct arm_smmu_pte_info {
	void *virt_addr;
	size_t size;
	struct list_head entry;
};

static bool using_legacy_binding, using_generic_binding;

struct arm_smmu_option_prop {
	u32 opt;
	const char *prop;
};

static struct arm_smmu_option_prop arm_smmu_options[] = {
	{ ARM_SMMU_OPT_FATAL_ASF, "qcom,fatal-asf" },
	{ ARM_SMMU_OPT_SKIP_INIT, "qcom,skip-init" },
	{ ARM_SMMU_OPT_3LVL_TABLES, "qcom,use-3-lvl-tables" },
	{ ARM_SMMU_OPT_NO_ASID_RETENTION, "qcom,no-asid-retention" },
	{ ARM_SMMU_OPT_DISABLE_ATOS, "qcom,disable-atos" },
	{ ARM_SMMU_OPT_SPLIT_TABLES, "qcom,split-tables" },
	{ 0, NULL},
};

static phys_addr_t arm_smmu_iova_to_phys(struct iommu_domain *domain,
					dma_addr_t iova);
static phys_addr_t arm_smmu_iova_to_phys_hard(struct iommu_domain *domain,
				    dma_addr_t iova, unsigned long trans_flags);
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

static int arm_smmu_setup_default_domain(struct device *dev,
				struct iommu_domain *domain);
static int __arm_smmu_domain_set_attr(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data);
static int arm_smmu_domain_get_attr(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data);

static inline int arm_smmu_rpm_get(struct arm_smmu_device *smmu)
{
	if (pm_runtime_enabled(smmu->dev))
		return pm_runtime_resume_and_get(smmu->dev);

	return 0;
}

static inline void arm_smmu_rpm_put(struct arm_smmu_device *smmu)
{
	if (pm_runtime_enabled(smmu->dev))
		pm_runtime_put(smmu->dev);
}

static struct arm_smmu_domain *to_smmu_domain(struct iommu_domain *dom)
{
	struct msm_iommu_domain *msm_domain = to_msm_iommu_domain(dom);

	return container_of(msm_domain, struct arm_smmu_domain, domain);
}

static struct arm_smmu_domain *cb_cfg_to_smmu_domain(struct arm_smmu_cfg *cfg)
{
	return container_of(cfg, struct arm_smmu_domain, cfg);
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

	return test_bit(DOMAIN_ATTR_DYNAMIC, smmu_domain->attributes);
}

static bool is_iommu_pt_coherent(struct arm_smmu_domain *smmu_domain)
{
	if (test_bit(DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT,
		     smmu_domain->attributes))
		return true;
	else if (smmu_domain->smmu && smmu_domain->smmu->dev)
		return dev_is_dma_coherent(smmu_domain->smmu->dev);
	else
		return false;
}

static bool arm_smmu_has_secure_vmid(struct arm_smmu_domain *smmu_domain)
{
	return (smmu_domain->secure_vmid != VMID_INVAL);
}

static void arm_smmu_secure_domain_lock(struct arm_smmu_domain *smmu_domain)
{
	if (arm_smmu_has_secure_vmid(smmu_domain))
		mutex_lock(&smmu_domain->assign_lock);
}

static void arm_smmu_secure_domain_unlock(struct arm_smmu_domain *smmu_domain)
{
	if (arm_smmu_has_secure_vmid(smmu_domain))
		mutex_unlock(&smmu_domain->assign_lock);
}

#ifdef CONFIG_ARM_SMMU_SELFTEST

static int selftest;
module_param_named(selftest, selftest, int, 0644);
static int irq_count;

struct arm_smmu_cf_selftest_data {
	struct arm_smmu_device *smmu;
	int cbndx;
};

static DECLARE_WAIT_QUEUE_HEAD(wait_int);
static irqreturn_t arm_smmu_cf_selftest(int irq, void *data)
{
	u32 fsr;
	struct irq_data *irq_data = irq_get_irq_data(irq);
	struct arm_smmu_cf_selftest_data *cb_data = data;
	struct arm_smmu_device *smmu = cb_data->smmu;
	int idx = cb_data->cbndx;
	unsigned long hwirq = ULONG_MAX;

	fsr = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSR);

	irq_count++;
	if (irq_data)
		hwirq = irq_data->hwirq;
	pr_info("Interrupt (irq:%d hwirq:%ld) received, fsr:0x%x\n",
				irq, hwirq, fsr);

	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_FSR, fsr);

	wake_up(&wait_int);
	return IRQ_HANDLED;
}

static void arm_smmu_interrupt_selftest(struct arm_smmu_device *smmu)
{
	int cb;
	int cb_count = 0;
	struct arm_smmu_cf_selftest_data *cb_data;

	if (!selftest)
		return;

	cb = smmu->num_s2_context_banks;

	if (smmu->version < ARM_SMMU_V2)
		return;

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data)
		return;
	cb_data->smmu = smmu;

	for_each_clear_bit_from(cb, smmu->context_map,
				smmu->num_context_banks) {
		int irq;
		int ret;
		u32 reg;
		u32 reg_orig;
		int irq_cnt;

		irq = smmu->irqs[smmu->num_global_irqs + cb];
		cb_data->cbndx = cb;

		ret = devm_request_threaded_irq(smmu->dev, irq, NULL,
				arm_smmu_cf_selftest,
				IRQF_ONESHOT | IRQF_SHARED,
				"arm-smmu-context-fault", cb_data);
		if (ret < 0) {
			dev_err(smmu->dev,
				"Failed to request cntx IRQ %d (%u)\n",
				cb, irq);
			continue;
		}

		cb_count++;
		irq_cnt = irq_count;

		reg_orig = arm_smmu_cb_read(smmu, cb, ARM_SMMU_CB_SCTLR);
		reg = reg_orig | SCTLR_CFIE | SCTLR_CFRE;

		arm_smmu_cb_write(smmu, cb, ARM_SMMU_CB_SCTLR, reg);
		dev_info(smmu->dev, "Testing cntx %d irq %d\n", cb, irq);

		/* Make sure ARM_SMMU_CB_SCTLR is configured */
		wmb();
		arm_smmu_cb_write(smmu, cb, ARM_SMMU_CB_FSRRESTORE, FSR_TF);

		wait_event_timeout(wait_int, (irq_count > irq_cnt),
			msecs_to_jiffies(1000));

		/* Make sure ARM_SMMU_CB_FSRRESTORE is written to */
		wmb();
		arm_smmu_cb_write(smmu, cb, ARM_SMMU_CB_SCTLR, reg_orig);
		devm_free_irq(smmu->dev, irq, cb_data);
	}

	kfree(cb_data);
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

static void arm_smmu_arch_write_sync(struct arm_smmu_device *smmu)
{
	u32 id;

	if (!smmu)
		return;

	/* Read to complete prior write transcations */
	id = arm_smmu_readl(smmu, ARM_SMMU_IMPL_DEF0, 0);


	/* Wait for read to complete before off */
	rmb();
}

static struct platform_driver arm_smmu_driver;
static struct msm_iommu_ops arm_smmu_ops;

#ifdef CONFIG_ARM_SMMU_LEGACY_DT_BINDINGS
static int arm_smmu_bus_init(struct iommu_ops *ops);

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
			    "#stream-id-cells", -1)
		if (it->node == np) {
			*(void **)data = dev;
			return 1;
		}
	it->node = np;
	return err == -ENOENT ? 0 : err;
}

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
				&arm_smmu_ops.iommu_ops);
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

/*
 * With the legacy DT binding in play, we have no guarantees about
 * probe order, but then we're also not doing default domains, so we can
 * delay setting bus ops until we're sure every possible SMMU is ready,
 * and that way ensure that no add_device() calls get missed.
 */
static int arm_smmu_legacy_bus_init(void)
{
	if (using_legacy_binding)
		return arm_smmu_bus_init(&arm_smmu_ops);
	return 0;
}
device_initcall_sync(arm_smmu_legacy_bus_init);
#else
static int arm_smmu_register_legacy_master(struct device *dev,
					   struct arm_smmu_device **smmu)
{
	return -ENODEV;
}
#endif /* CONFIG_ARM_SMMU_LEGACY_DT_BINDINGS */

static int __arm_smmu_alloc_cb(struct arm_smmu_device *smmu, int start,
			       struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	unsigned long *map = smmu->context_map;
	int end = smmu->num_context_banks;
	int idx;
	int i;

	for_each_cfg_sme(fwspec, i, idx) {
		if (smmu->s2crs[idx].pinned)
			return smmu->s2crs[idx].cbndx;
	}

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

static int arm_smmu_raise_interconnect_bw(struct arm_smmu_power_resources *pwr)
{
	if (!pwr->icc_path)
		return 0;
	return icc_set_bw(pwr->icc_path, ARM_SMMU_ICC_AVG_BW,
			  ARM_SMMU_ICC_PEAK_BW_HIGH);
}

static void arm_smmu_lower_interconnect_bw(struct arm_smmu_power_resources *pwr)
{
	if (!pwr->icc_path)
		return;
	WARN_ON(icc_set_bw(pwr->icc_path, ARM_SMMU_ICC_AVG_BW,
			   ARM_SMMU_ICC_PEAK_BW_LOW));
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
static void arm_smmu_power_off_atomic(struct arm_smmu_device *smmu,
				      struct arm_smmu_power_resources *pwr)
{
	unsigned long flags;

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

	arm_smmu_arch_write_sync(smmu);
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

	ret = arm_smmu_raise_interconnect_bw(pwr);
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
	arm_smmu_lower_interconnect_bw(pwr);
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
	arm_smmu_lower_interconnect_bw(pwr);
	pwr->power_count = 0;
	mutex_unlock(&pwr->power_lock);
}

int arm_smmu_power_on(struct arm_smmu_power_resources *pwr)
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

void arm_smmu_power_off(struct arm_smmu_device *smmu,
			struct arm_smmu_power_resources *pwr)
{
	arm_smmu_power_off_atomic(smmu, pwr);
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
	bool atomic_domain = test_bit(DOMAIN_ATTR_ATOMIC,
				      smmu_domain->attributes);

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
	bool atomic_domain = test_bit(DOMAIN_ATTR_ATOMIC,
				      smmu_domain->attributes);

	if (atomic_domain) {
		arm_smmu_power_off_atomic(smmu, smmu->pwr);
		return;
	}

	arm_smmu_power_off(smmu, smmu->pwr);
}

/* Wait for any pending TLB invalidations to complete */
static int __arm_smmu_tlb_sync(struct arm_smmu_device *smmu, int page,
				int sync, int status)
{
	unsigned int inc, delay;
	u32 reg;

	/*
	 * Allowing an unbounded number of sync requests to be submitted when a
	 * TBU is not processing sync requests can cause a TBU's command queue
	 * to fill up. Once the queue is full, subsequent sync requests can
	 * stall the CPU indefinitely. Avoid this by gating subsequent sync
	 * requests after the first sync timeout on an SMMU.
	 */
	if (IS_ENABLED(CONFIG_IOMMU_TLBSYNC_DEBUG) &&
	    test_bit(0, &smmu->sync_timed_out))
		return -EINVAL;

	arm_smmu_writel(smmu, page, sync, QCOM_DUMMY_VAL);
	for (delay = 1, inc = 1; delay < TLB_LOOP_TIMEOUT; delay += inc) {
		reg = arm_smmu_readl(smmu, page, status);
		if (!(reg & sTLBGSTATUS_GSACTIVE))
			return 0;

		cpu_relax();
		udelay(inc);
		if (inc < TLB_LOOP_INC_MAX)
			inc *= 2;
	}

	if (IS_ENABLED(CONFIG_IOMMU_TLBSYNC_DEBUG) &&
	    test_and_set_bit_lock(0, &smmu->sync_timed_out))
		goto out;

	trace_tlbsync_timeout(smmu->dev, 0);
	if (smmu->impl && smmu->impl->tlb_sync_timeout)
		smmu->impl->tlb_sync_timeout(smmu);
out:
	return -EINVAL;
}

static void arm_smmu_tlb_sync_global(struct arm_smmu_device *smmu)
{
	unsigned long flags;

	spin_lock_irqsave(&smmu->global_sync_lock, flags);
	if (__arm_smmu_tlb_sync(smmu, ARM_SMMU_GR0, ARM_SMMU_GR0_sTLBGSYNC,
				ARM_SMMU_GR0_sTLBGSTATUS))
		dev_err_ratelimited(smmu->dev,
				    "TLB global sync failed!\n");
	spin_unlock_irqrestore(&smmu->global_sync_lock, flags);
}

static void arm_smmu_tlb_sync_context(void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	int idx = smmu_domain->cfg.cbndx;
	unsigned long flags;

	spin_lock_irqsave(&smmu_domain->sync_lock, flags);
	if (__arm_smmu_tlb_sync(smmu, ARM_SMMU_CB(smmu, idx),
				ARM_SMMU_CB_TLBSYNC, ARM_SMMU_CB_TLBSTATUS))
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
	int idx = smmu_domain->cfg.cbndx;
	bool use_tlbiall = smmu->options & ARM_SMMU_OPT_NO_ASID_RETENTION;
	ktime_t cur = ktime_get();

	trace_tlbi_start(dev, 0);

	if (!use_tlbiall) {
		wmb();
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_S1_TLBIASID,
				  cfg->asid);
	} else {
		wmb();
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_S1_TLBIALL, 0);
	}

	arm_smmu_tlb_sync_context(cookie);
	trace_tlbi_end(dev, ktime_us_delta(ktime_get(), cur));
}

static void arm_smmu_tlb_inv_context_s2(void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_device *smmu = smmu_domain->smmu;

	/* See above */
	wmb();
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_TLBIVMID, smmu_domain->cfg.vmid);
	arm_smmu_tlb_sync_global(smmu);
}

static void arm_smmu_tlb_inv_range_s1(unsigned long iova, size_t size,
				      size_t granule, bool leaf, void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	int reg, idx = cfg->cbndx;
	bool use_tlbiall = smmu->options & ARM_SMMU_OPT_NO_ASID_RETENTION;

	if (smmu->features & ARM_SMMU_FEAT_COHERENT_WALK)
		wmb();

	if (!use_tlbiall) {
		reg = leaf ? ARM_SMMU_CB_S1_TLBIVAL : ARM_SMMU_CB_S1_TLBIVA;

		if (cfg->fmt != ARM_SMMU_CTX_FMT_AARCH64) {
			iova = (iova >> 12) << 12;
			iova |= cfg->asid;
			do {
				arm_smmu_cb_write(smmu, idx, reg, iova);
				iova += granule;
			} while (size -= granule);
		} else {
			iova >>= 12;
			iova |= (u64)cfg->asid << 48;
			do {
				arm_smmu_cb_writeq(smmu, idx, reg, iova);
				iova += granule >> 12;
			} while (size -= granule);
		}
	} else {
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_S1_TLBIALL, 0);
	}
}

static void arm_smmu_tlb_inv_range_s2(unsigned long iova, size_t size,
				      size_t granule, bool leaf, void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	int reg, idx = smmu_domain->cfg.cbndx;

	if (smmu->features & ARM_SMMU_FEAT_COHERENT_WALK)
		wmb();

	reg = leaf ? ARM_SMMU_CB_S2_TLBIIPAS2L : ARM_SMMU_CB_S2_TLBIIPAS2;
	iova >>= 12;
	do {
		if (smmu_domain->cfg.fmt == ARM_SMMU_CTX_FMT_AARCH64)
			arm_smmu_cb_writeq(smmu, idx, reg, iova);
		else
			arm_smmu_cb_write(smmu, idx, reg, iova);
		iova += granule >> 12;
	} while (size -= granule);
}

static void arm_smmu_tlb_inv_walk(unsigned long iova, size_t size,
				  size_t granule, void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	const struct arm_smmu_flush_ops *ops = smmu_domain->flush_ops;

	if (!IS_ENABLED(CONFIG_QCOM_IOMMU_TLBI_QUIRKS)) {
		smmu_domain->defer_flush = true;
		return;
	}

	ops->tlb_inv_range(iova, size, granule, false, cookie);
	ops->tlb_sync(cookie);
}

static void arm_smmu_tlb_inv_leaf(unsigned long iova, size_t size,
				  size_t granule, void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	const struct arm_smmu_flush_ops *ops = smmu_domain->flush_ops;

	if (!IS_ENABLED(CONFIG_QCOM_IOMMU_TLBI_QUIRKS)) {
		smmu_domain->defer_flush = true;
		return;
	}

	ops->tlb_inv_range(iova, size, granule, true, cookie);
	ops->tlb_sync(cookie);
}

static void arm_smmu_tlb_add_page(struct iommu_iotlb_gather *gather,
				  unsigned long iova, size_t granule,
				  void *cookie)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	const struct arm_smmu_flush_ops *ops = smmu_domain->flush_ops;

	if (!IS_ENABLED(CONFIG_QCOM_IOMMU_TLBI_QUIRKS)) {
		smmu_domain->defer_flush = true;
		return;
	}

	ops->tlb_inv_range(iova, granule, granule, true, cookie);
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
	struct arm_smmu_device *smmu = smmu_domain->smmu;

	if (smmu->features & ARM_SMMU_FEAT_COHERENT_WALK)
		wmb();

	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_TLBIVMID, smmu_domain->cfg.vmid);
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

static void *arm_smmu_alloc_pgtable(void *cookie, int order, gfp_t gfp_mask)
{
	int ret;
	struct page *page;
	void *page_addr;
	size_t size = (1UL << order) * PAGE_SIZE;
	struct arm_smmu_domain *smmu_domain = cookie;

	if (!arm_smmu_has_secure_vmid(smmu_domain)) {
		/* size is expected to be 4K with current configuration */
		if (size == PAGE_SIZE) {
			page = list_first_entry_or_null(
				&smmu_domain->nonsecure_pool, struct page, lru);
			if (page) {
				list_del_init(&page->lru);
				return page_address(page);
			}
		}

		page = alloc_pages(gfp_mask, order);
		if (!page)
			return NULL;

		return page_address(page);
	}

	page_addr = arm_smmu_secure_pool_remove(smmu_domain, size);
	if (page_addr)
		return page_addr;

	page = alloc_pages(gfp_mask, order);
	if (!page)
		return NULL;

	page_addr = page_address(page);
	ret = arm_smmu_prepare_pgtable(page_addr, cookie);
	if (ret) {
		free_pages((unsigned long)page_addr, order);
		return NULL;
	}

	return page_addr;
}

static void arm_smmu_free_pgtable(void *cookie, void *virt, int order)
{
	struct arm_smmu_domain *smmu_domain = cookie;
	size_t size = (1UL << order) * PAGE_SIZE;

	if (!arm_smmu_has_secure_vmid(smmu_domain)) {
		free_pages((unsigned long)virt, order);
		return;
	}

	if (arm_smmu_secure_pool_add(smmu_domain, virt, size))
		arm_smmu_unprepare_pgtable(smmu_domain, virt, size);
}

static const struct iommu_pgtable_ops arm_smmu_pgtable_ops = {
	.alloc_pgtable = arm_smmu_alloc_pgtable,
	.free_pgtable  = arm_smmu_free_pgtable,
};

static const struct arm_smmu_flush_ops arm_smmu_s1_tlb_ops = {
	.tlb = {
		.tlb_flush_all  = arm_smmu_tlb_inv_context_s1,
		.tlb_flush_walk = arm_smmu_tlb_inv_walk,
		.tlb_flush_leaf = arm_smmu_tlb_inv_leaf,
		.tlb_add_page   = arm_smmu_tlb_add_page,
	},
	.tlb_inv_range		= arm_smmu_tlb_inv_range_s1,
	.tlb_sync		= arm_smmu_tlb_sync_context,
};

static const struct arm_smmu_flush_ops arm_smmu_s2_tlb_ops_v2 = {
	.tlb = {
		.tlb_flush_all  = arm_smmu_tlb_inv_context_s2,
		.tlb_flush_walk = arm_smmu_tlb_inv_walk,
		.tlb_flush_leaf = arm_smmu_tlb_inv_leaf,
		.tlb_add_page   = arm_smmu_tlb_add_page,
	},
	.tlb_inv_range		= arm_smmu_tlb_inv_range_s2,
	.tlb_sync		= arm_smmu_tlb_sync_context,
};

static const struct arm_smmu_flush_ops arm_smmu_s2_tlb_ops_v1 = {
	.tlb = {
		.tlb_flush_all  = arm_smmu_tlb_inv_context_s2,
		.tlb_flush_walk = arm_smmu_tlb_inv_walk,
		.tlb_flush_leaf = arm_smmu_tlb_inv_leaf,
		.tlb_add_page   = arm_smmu_tlb_add_page,
	},
	.tlb_inv_range		= arm_smmu_tlb_inv_vmid_nosync,
	.tlb_sync		= arm_smmu_tlb_sync_vmid,
};

static void arm_smmu_deferred_flush(struct arm_smmu_domain *smmu_domain)
{
	/*
	 * This checks for deferred invalidations, and perform flush all.
	 * Deferred invalidations helps replace multiple invalidations with
	 * single flush
	 */
	if (smmu_domain->defer_flush) {
		smmu_domain->flush_ops->tlb.tlb_flush_all(smmu_domain);
		smmu_domain->defer_flush = false;
	}
}

static void print_ctx_regs(struct arm_smmu_device *smmu, struct arm_smmu_cfg
			   *cfg, unsigned int fsr)
{
	u32 fsynr0;
	int idx = cfg->cbndx;
	bool stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;

	fsynr0 = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSYNR0);

	dev_err(smmu->dev, "FAR    = 0x%016llx\n",
		arm_smmu_cb_readq(smmu, idx, ARM_SMMU_CB_FAR));
	dev_err(smmu->dev, "PAR    = 0x%pK\n",
		(void *) arm_smmu_cb_readq(smmu, idx, ARM_SMMU_CB_PAR));

	dev_err(smmu->dev,
		"FSR    = 0x%08x [%s%s%s%s%s%s%s%s%s%s]\n",
		fsr,
		(fsr & FSR_TF) ?  (fsynr0 & FSYNR0_WNR ?
				 "TF W " : "TF R ") : "",
		(fsr & FSR_AFF) ? "AFF " : "",
		(fsr & FSR_PF) ? (fsynr0 & FSYNR0_WNR ?
				"PF W " : "PF R ") : "",
		(fsr & FSR_EF) ? "EF " : "",
		(fsr & FSR_TLBMCF) ? "TLBMCF " : "",
		(fsr & FSR_TLBLKF) ? "TLBLKF " : "",
		(fsr & FSR_ASF) ? "ASF " : "",
		(fsr & FSR_UUT) ? "UUT " : "",
		(fsr & FSR_SS) ? "SS " : "",
		(fsr & FSR_MULTI) ? "MULTI " : "");

	if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
		dev_err(smmu->dev, "TTBR0  = 0x%pK\n",
			(void *) (unsigned long)
			arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_TTBR0));
		dev_err(smmu->dev, "TTBR1  = 0x%pK\n",
			(void *) (unsigned long)
			arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_TTBR1));
	} else {
		dev_err(smmu->dev, "TTBR0  = 0x%pK\n",
			(void *) arm_smmu_cb_readq(smmu, idx,
						   ARM_SMMU_CB_TTBR0));
		if (stage1)
			dev_err(smmu->dev, "TTBR1  = 0x%pK\n",
				(void *) arm_smmu_cb_readq(smmu, idx,
							   ARM_SMMU_CB_TTBR1));
	}


	dev_err(smmu->dev, "SCTLR  = 0x%08x ACTLR  = 0x%08x\n",
	       arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_SCTLR),
	       arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_ACTLR));
	dev_err(smmu->dev, "CBAR  = 0x%08x\n",
		arm_smmu_gr1_read(smmu, ARM_SMMU_GR1_CBAR(cfg->cbndx)));
	dev_err(smmu->dev, "MAIR0   = 0x%08x MAIR1   = 0x%08x\n",
	       arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_S1_MAIR0),
	       arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_S1_MAIR1));

}

static phys_addr_t arm_smmu_verify_fault(struct iommu_domain *domain,
					 dma_addr_t iova, u32 fsr, u32 fsynr0)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct msm_io_pgtable_info *pgtbl_info = &smmu_domain->pgtbl_info[0];
	phys_addr_t phys_hard_priv = 0;
	phys_addr_t phys_stimu, phys_stimu_post_tlbiall;
	unsigned long flags = 0;
	unsigned int ias = smmu_domain->pgtbl_info[0].pgtbl_cfg.ias;

	/*
	 * The address in the CB's FAR is not sign-extended, so lets perform the
	 * sign extension here, as arm_smmu_iova_to_phys_hard() expects the
	 * IOVA to be sign extended.
	 */
	if ((iova & BIT_ULL(ias)) &&
	    (test_bit(DOMAIN_ATTR_SPLIT_TABLES, smmu_domain->attributes)))
		iova |= GENMASK_ULL(63, ias + 1);

	/* Get the transaction type */
	if (fsynr0 & FSYNR0_WNR)
		flags |= IOMMU_TRANS_WRITE;
	if (fsynr0 & FSYNR0_PNU)
		flags |= IOMMU_TRANS_PRIV;
	if (fsynr0 & FSYNR0_IND)
		flags |= IOMMU_TRANS_INST;

	/* Now replicate the faulty transaction */
	phys_stimu = arm_smmu_iova_to_phys_hard(domain, iova, flags);

	/*
	 * If the replicated transaction fails, it could be due to legitimate
	 * unmapped access (translation fault) or stale TLB with insufficient
	 * privileges (permission fault). Try ATOS operation with full access
	 * privileges to rule out stale entry with insufficient privileges case.
	 */
	if (!phys_stimu)
		phys_hard_priv = arm_smmu_iova_to_phys_hard(domain, iova,
						       IOMMU_TRANS_DEFAULT |
						       IOMMU_TRANS_PRIV);

	/* Now replicate the faulty transaction post tlbiall */
	pgtbl_info->pgtbl_cfg.tlb->tlb_flush_all(smmu_domain);
	phys_stimu_post_tlbiall = arm_smmu_iova_to_phys_hard(domain, iova,
							     flags);

	if (!phys_stimu && phys_hard_priv) {
		dev_err(smmu->dev,
			"ATOS results differed across access privileges...\n"
			"Before: %pa After: %pa\n",
			&phys_stimu, &phys_hard_priv);
	}

	if (phys_stimu != phys_stimu_post_tlbiall) {
		dev_err(smmu->dev,
			"ATOS results differed across TLBIALL...\n"
			"Before: %pa After: %pa\n", &phys_stimu,
						&phys_stimu_post_tlbiall);
	}

	return (phys_stimu == 0 ? phys_stimu_post_tlbiall : phys_stimu);
}

int iommu_get_fault_ids(struct iommu_domain *domain,
			struct iommu_fault_ids *f_ids)
{
	struct arm_smmu_domain *smmu_domain;
	struct arm_smmu_device *smmu;
	u32 fsr, fsynr1;
	int idx, ret;

	if (!domain || !f_ids)
		return -EINVAL;

	smmu_domain = to_smmu_domain(domain);
	smmu = smmu_domain->smmu;
	idx = smmu_domain->cfg.cbndx;

	ret = arm_smmu_power_on(smmu->pwr);
	if (ret)
		return ret;

	fsr = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSR);

	if (!(fsr & FSR_FAULT)) {
		arm_smmu_power_off(smmu, smmu->pwr);
		return -EINVAL;
	}

	fsynr1 = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSYNR1);
	arm_smmu_power_off(smmu, smmu->pwr);

	f_ids->bid = FIELD_GET(FSYNR1_BID, fsynr1);
	f_ids->pid = FIELD_GET(FSYNR1_PID, fsynr1);
	f_ids->mid = FIELD_GET(FSYNR1_MID, fsynr1);

	return 0;
}
EXPORT_SYMBOL(iommu_get_fault_ids);

static irqreturn_t arm_smmu_context_fault(int irq, void *dev)
{
	int flags, ret, tmp;
	u32 fsr, fsynr0, fsynr1, frsynra, resume;
	unsigned long iova;
	struct iommu_domain *domain = dev;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	bool fatal_asf = smmu->options & ARM_SMMU_OPT_FATAL_ASF;
	phys_addr_t phys_soft;
	uint64_t pte;
	unsigned int ias = smmu_domain->pgtbl_info[0].pgtbl_cfg.ias;
	bool non_fatal_fault = test_bit(DOMAIN_ATTR_NON_FATAL_FAULTS,
					smmu_domain->attributes);

	static DEFINE_RATELIMIT_STATE(_rs,
				      DEFAULT_RATELIMIT_INTERVAL,
				      DEFAULT_RATELIMIT_BURST);
	int idx = smmu_domain->cfg.cbndx;

	ret = arm_smmu_power_on(smmu->pwr);
	if (ret)
		return IRQ_NONE;

	fsr = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSR);

	if (!(fsr & FSR_FAULT)) {
		ret = IRQ_NONE;
		goto out_power_off;
	}

	if (fatal_asf && (fsr & FSR_ASF)) {
		dev_err(smmu->dev,
			"Took an address size fault.  Refusing to recover.\n");
		BUG();
	}

	fsynr0 = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSYNR0);
	fsynr1 = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_FSYNR1);
	flags = fsynr0 & FSYNR0_WNR ? IOMMU_FAULT_WRITE : IOMMU_FAULT_READ;
	if (fsr & FSR_TF)
		flags |= IOMMU_FAULT_TRANSLATION;
	if (fsr & FSR_PF)
		flags |= IOMMU_FAULT_PERMISSION;
	if (fsr & FSR_EF)
		flags |= IOMMU_FAULT_EXTERNAL;
	if (fsr & FSR_SS)
		flags |= IOMMU_FAULT_TRANSACTION_STALLED;

	iova = arm_smmu_cb_readq(smmu, idx, ARM_SMMU_CB_FAR);

	/*
	 * The address in the CB's FAR is not sign-extended, so lets perform the
	 * sign extension here, as arm_smmu_iova_to_phys() expects the
	 * IOVA to be sign extended.
	 */
	if ((iova & BIT_ULL(ias)) &&
	    (test_bit(DOMAIN_ATTR_SPLIT_TABLES, smmu_domain->attributes)))
		iova |= GENMASK_ULL(63, ias + 1);

	phys_soft = arm_smmu_iova_to_phys(domain, iova);
	frsynra = arm_smmu_gr1_read(smmu, ARM_SMMU_GR1_CBFRSYNRA(cfg->cbndx));
	tmp = report_iommu_fault(domain, smmu->dev, iova, flags);
	if (!tmp || (tmp == -EBUSY)) {
		dev_dbg(smmu->dev,
			"Context fault handled by client: iova=0x%08lx, cb=%d, fsr=0x%x, fsynr0=0x%x, fsynr1=0x%x\n",
			iova, cfg->cbndx, fsr, fsynr0, fsynr1);
		dev_dbg(smmu->dev,
			"Client info: BID=0x%lx, PID=0x%lx, MID=0x%lx\n",
			FIELD_GET(FSYNR1_BID, fsynr1),
			FIELD_GET(FSYNR1_PID, fsynr1),
			FIELD_GET(FSYNR1_MID, fsynr1));
		dev_dbg(smmu->dev,
			"soft iova-to-phys=%pa\n", &phys_soft);
		ret = IRQ_HANDLED;
		resume = RESUME_TERMINATE;
	} else {
		if (__ratelimit(&_rs)) {
			phys_addr_t phys_atos;

			print_ctx_regs(smmu, cfg, fsr);
			phys_atos = arm_smmu_verify_fault(domain, iova, fsr,
							  fsynr0);
			dev_err(smmu->dev,
				"Unhandled context fault: iova=0x%08lx, cb=%d, fsr=0x%x, fsynr0=0x%x, fsynr1=0x%x\n",
				iova, cfg->cbndx, fsr, fsynr0, fsynr1);

			dev_err(smmu->dev, "SSD=0x%x SID=0x%x\n",
				FIELD_GET(CBFRSYNRA_SSD, frsynra),
				FIELD_GET(CBFRSYNRA_SID, frsynra));

			dev_err(smmu->dev,
				"Client info: BID=0x%lx, PID=0x%lx, MID=0x%lx\n",
				FIELD_GET(FSYNR1_BID, fsynr1),
				FIELD_GET(FSYNR1_PID, fsynr1),
				FIELD_GET(FSYNR1_MID, fsynr1));

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
		}
		ret = IRQ_HANDLED;
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
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_FSR, fsr);

		/*
		 * Barrier required to ensure that the FSR is cleared
		 * before resuming SMMU operation
		 */
		wmb();

		/* Retry or terminate any stalled transactions */
		if (fsr & FSR_SS)
			arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_RESUME,
					  resume);
	}

out_power_off:
	arm_smmu_power_off(smmu, smmu->pwr);

	return ret;
}

static irqreturn_t arm_smmu_global_fault(int irq, void *dev)
{
	u32 gfsr, gfsynr0, gfsynr1, gfsynr2;
	struct arm_smmu_device *smmu = dev;

	if (arm_smmu_power_on(smmu->pwr))
		return IRQ_NONE;

	gfsr = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_sGFSR);
	gfsynr0 = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_sGFSYNR0);
	gfsynr1 = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_sGFSYNR1);
	gfsynr2 = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_sGFSYNR2);

	if (!gfsr) {
		arm_smmu_power_off(smmu, smmu->pwr);
		return IRQ_NONE;
	}

	dev_err_ratelimited(smmu->dev,
		"Unexpected global fault, this could be serious\n");
	dev_err_ratelimited(smmu->dev,
		"\tGFSR 0x%08x, GFSYNR0 0x%08x, GFSYNR1 0x%08x, GFSYNR2 0x%08x\n",
		gfsr, gfsynr0, gfsynr1, gfsynr2);

	wmb();
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_sGFSR, gfsr);
	arm_smmu_power_off(smmu, smmu->pwr);
	return IRQ_HANDLED;
}

static u32 arm_smmu_tcr(u64 tcr, bool split_tables)
{
	u32 tcr_out, tcr0, tcr1, tg0;

	if (split_tables) {
		tcr0 = FIELD_GET(TCR_TCR0, tcr);
		/*
		 * The TCR configuration for TTBR1 is identical
		 * to the TCR configuration for TTBR0, except
		 * for the TG field, so translate the TCR0
		 * settings to TCR1 settings by shifting them
		 */
		tcr1 = FIELD_PREP(TCR_TCR1, tcr0);
		tcr1 &= ~TCR1_TG1;

		/* Map TG0 -> TG1 */
		tg0 = FIELD_GET(TCR0_TG0, tcr0);
		if (tg0 == TCR0_TG0_4K)
			tcr1 |= FIELD_PREP(TCR1_TG1, TCR1_TG1_4K);
		else if (tg0 == TCR0_TG0_64K)
			tcr1 |= FIELD_PREP(TCR1_TG1, TCR1_TG1_64K);
		else if (tg0 == TCR0_TG0_16K)
			tcr1 |= FIELD_PREP(TCR1_TG1, TCR1_TG1_16K);

		tcr_out = tcr1 | tcr0;
	} else {
		tcr_out = lower_32_bits(tcr);
	}

	return tcr_out;
}

static void arm_smmu_init_context_bank(struct arm_smmu_domain *smmu_domain,
				       struct msm_io_pgtable_info *pgtbl_info)
{
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct arm_smmu_cb *cb = &smmu_domain->smmu->cbs[cfg->cbndx];
	struct io_pgtable_cfg *pgtbl_cfg = &pgtbl_info[0].pgtbl_cfg;
	struct io_pgtable_cfg *ttbr1_pgtbl_cfg = &pgtbl_info[1].pgtbl_cfg;
	bool stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;
	bool split_tables = test_bit(DOMAIN_ATTR_SPLIT_TABLES,
				     smmu_domain->attributes);

	cb->cfg = cfg;

	/* TCR */
	if (stage1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
			cb->tcr[0] = pgtbl_cfg->arm_v7s_cfg.tcr;
		} else {
			cb->tcr[0] =
				arm_smmu_tcr(pgtbl_cfg->arm_lpae_s1_cfg.tcr,
					     split_tables);
			cb->tcr[1] = pgtbl_cfg->arm_lpae_s1_cfg.tcr >> 32;
			cb->tcr[1] |= FIELD_PREP(TCR2_SEP, TCR2_SEP_UPSTREAM);
			if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64)
				cb->tcr[1] |= TCR2_AS;
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
			cb->ttbr[0] |= FIELD_PREP(TTBRn_ASID, cfg->asid);
			if (split_tables) {
				cb->ttbr[1] =
				ttbr1_pgtbl_cfg->arm_lpae_s1_cfg.ttbr[0];
			} else {
				cb->ttbr[1] =
					pgtbl_cfg->arm_lpae_s1_cfg.ttbr[1];
				cb->ttbr[1] |=
					FIELD_PREP(TTBRn_ASID, cfg->asid);
			}
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
					unsigned long *attributes)
{
	u32 reg;
	bool stage1;
	struct arm_smmu_cb *cb = &smmu->cbs[idx];
	struct arm_smmu_cfg *cfg = cb->cfg;
	struct arm_smmu_domain *smmu_domain = NULL;

	/* Unassigned context banks only need disabling */
	if (!cfg) {
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_SCTLR, 0);
		return;
	}

	stage1 = cfg->cbar != CBAR_TYPE_S2_TRANS;

	/* CBA2R */
	if (smmu->version > ARM_SMMU_V1) {
		if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64)
			reg = CBA2R_VA64;
		else
			reg = 0;
		/* 16-bit VMIDs live in CBA2R */
		if (smmu->features & ARM_SMMU_FEAT_VMID16)
			reg |= FIELD_PREP(CBA2R_VMID16, cfg->vmid);

		arm_smmu_gr1_write(smmu, ARM_SMMU_GR1_CBA2R(idx), reg);
	}

	/* CBAR */
	reg = FIELD_PREP(CBAR_TYPE, cfg->cbar);
	if (smmu->version < ARM_SMMU_V2)
		reg |= FIELD_PREP(CBAR_IRPTNDX, cfg->irptndx);

	/*
	 * Use the weakest shareability/memory types, so they are
	 * overridden by the ttbcr/pte.
	 */
	if (stage1) {
		reg |= FIELD_PREP(CBAR_S1_BPSHCFG, CBAR_S1_BPSHCFG_NSH) |
			FIELD_PREP(CBAR_S1_MEMATTR, CBAR_S1_MEMATTR_WB);
	} else if (!(smmu->features & ARM_SMMU_FEAT_VMID16)) {
		/* 8-bit VMIDs live in CBAR */
		reg |= FIELD_PREP(CBAR_VMID, cfg->vmid);
	}
	arm_smmu_gr1_write(smmu, ARM_SMMU_GR1_CBAR(idx), reg);

	/*
	 * TCR
	 * We must write this before the TTBRs, since it determines the
	 * access behaviour of some fields (in particular, ASID[15:8]).
	 */
	if (stage1 && smmu->version > ARM_SMMU_V1)
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_TCR2, cb->tcr[1]);
	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_TCR, cb->tcr[0]);

	/* TTBRs */
	if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH32_S) {
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_CONTEXTIDR, cfg->asid);
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_TTBR0, cb->ttbr[0]);
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_TTBR1, cb->ttbr[1]);
	} else {
		arm_smmu_cb_writeq(smmu, idx, ARM_SMMU_CB_TTBR0, cb->ttbr[0]);
		if (stage1)
			arm_smmu_cb_writeq(smmu, idx, ARM_SMMU_CB_TTBR1,
					   cb->ttbr[1]);
	}

	/* MAIRs (stage-1 only) */
	if (stage1) {
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_S1_MAIR0, cb->mair[0]);
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_S1_MAIR1, cb->mair[1]);
	}

	/* SCTLR */
	reg = SCTLR_CFCFG | SCTLR_CFIE | SCTLR_CFRE | SCTLR_AFE | SCTLR_TRE;

	/*
	 * Ensure bypass transactions are Non-shareable only for clients
	 * who are not io-coherent.
	 */
	smmu_domain = cb_cfg_to_smmu_domain(cfg);

	/*
	 * Override cacheability, shareability, r/w allocation for
	 * clients who are io-coherent
	 */
	if (of_dma_is_coherent(smmu_domain->dev->of_node)) {

		reg |= FIELD_PREP(SCTLR_WACFG, SCTLR_WACFG_WA) |
		       FIELD_PREP(SCTLR_RACFG, SCTLR_RACFG_RA) |
		       FIELD_PREP(SCTLR_SHCFG, SCTLR_SHCFG_OSH) |
		       SCTLR_MTCFG |
		       FIELD_PREP(SCTLR_MEM_ATTR, SCTLR_MEM_ATTR_OISH_WB_CACHE);
	} else {
		reg |= FIELD_PREP(SCTLR_SHCFG, SCTLR_SHCFG_NSH);
	}

	if (attributes && test_bit(DOMAIN_ATTR_FAULT_MODEL_NO_CFRE, attributes))
		reg &= ~SCTLR_CFRE;

	if (attributes && test_bit(DOMAIN_ATTR_FAULT_MODEL_NO_STALL,
				   attributes))
		reg &= ~SCTLR_CFCFG;

	if (attributes && test_bit(DOMAIN_ATTR_FAULT_MODEL_HUPCF, attributes))
		reg |= SCTLR_HUPCF;

	if (!attributes || (!test_bit(DOMAIN_ATTR_S1_BYPASS, attributes) &&
	     !test_bit(DOMAIN_ATTR_EARLY_MAP, attributes)) || !stage1)
		reg |= SCTLR_M;
	if (stage1)
		reg |= SCTLR_S1_ASIDPNE;
	if (IS_ENABLED(CONFIG_CPU_BIG_ENDIAN))
		reg |= SCTLR_E;

	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_SCTLR, reg);
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

static int get_range_prop(struct device *dev, const char *prop,
			  dma_addr_t *ret_base, dma_addr_t *ret_end)
{
	struct device_node *np;
	int naddr, nsize, len;
	u64 base, end, size;
	const __be32 *ranges;

	if (!dev->of_node)
		return -ENOENT;

	np = of_parse_phandle(dev->of_node, "qcom,iommu-group", 0);
	if (!np)
		np = dev->of_node;

	ranges = of_get_property(np, prop, &len);

	if (!ranges)
		return -ENOENT;

	len /= sizeof(u32);
	naddr = of_n_addr_cells(np);
	nsize = of_n_size_cells(np);
	if (len < naddr + nsize) {
		dev_err(dev, "Invalid length for %s, expected %d cells\n",
			prop, naddr + nsize);
		return -EINVAL;
	}
	if (naddr == 0 || nsize == 0) {
		dev_err(dev, "Invalid #address-cells %d or #size-cells %d for %s\n",
			prop, naddr, nsize);
		return -EINVAL;
	}

	base = of_read_number(ranges, naddr);
	size = of_read_number(ranges + naddr, nsize);
	end = base + size - 1;

	*ret_base = base;
	*ret_end = end;
	return 0;
}

static int arm_smmu_get_domain_dma_range(struct device *dev,
					 struct iommu_domain *domain,
					 dma_addr_t hw_base,
					 dma_addr_t hw_end,
					 dma_addr_t *ret_base,
					 dma_addr_t *ret_end)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	dma_addr_t dma_base, dma_end;
	bool is_fast = test_bit(DOMAIN_ATTR_FAST, smmu_domain->attributes);
	int ret;

	ret = get_range_prop(dev, "qcom,iommu-dma-addr-pool", &dma_base,
			     &dma_end);
	if (ret == -ENOENT) {
		if (is_fast) {
			/*
			 * This domain uses fastmap, but doesn't have any domain
			 * geometry limitations, as implied by the absence of
			 * the qcom,iommu-dma-addr-pool property, so impose the
			 * default fastmap geometry requirement.
			 */
			dma_base = 0;
			dma_end = SZ_4G - 1;
		} else {
			dma_base = hw_base;
			dma_end = hw_end;
		}
	} else if (ret) {
		return ret;
	}

	if (!((hw_base <= dma_base) && (dma_end <= hw_end)))
		return -EINVAL;

	*ret_base = dma_base;
	*ret_end = dma_end;
	return 0;
}

/*
 * Get the supported IOVA range for the domain, this can be larger than the
 * configured DMA layer IOVA range.
 */
static int arm_smmu_get_domain_iova_range(struct device *dev,
					   struct iommu_domain *domain,
					   unsigned long ias,
					   dma_addr_t *ret_base,
					   dma_addr_t *ret_end)
{
	dma_addr_t iova_base, iova_end;
	dma_addr_t dma_base, dma_end, geometry_start, geometry_end;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	dma_addr_t hw_base = 0;
	dma_addr_t hw_end = (1UL << ias) - 1;
	bool is_fast = test_bit(DOMAIN_ATTR_FAST, smmu_domain->attributes);
	int ret;

	if (!is_fast) {
		iova_base = hw_base;
		iova_end = hw_end;
		goto end;
	}

	ret = arm_smmu_get_domain_dma_range(dev, domain, hw_base, hw_end,
					    &dma_base, &dma_end);
	if (ret)
		return ret;

	ret = get_range_prop(dev, "qcom,iommu-geometry", &geometry_start,
			     &geometry_end);
	if (!ret) {
		if (geometry_start >= SZ_1G * 4ULL ||
		    geometry_end >= SZ_1G * 4ULL) {
			pr_err("fastmap geometry does not support IOVAs >= 4GB\n");
			return -EINVAL;
		}

		if (geometry_start < dma_base)
			iova_base = geometry_start;
		else
			iova_base = dma_base;

		if (geometry_end > dma_end)
			iova_end = geometry_end;
		else
			iova_end = dma_end;
	} else if (ret == -ENOENT) {
		iova_base = 0;
		iova_end = SZ_4G - 1;
	} else {
		return ret;
	}

	if (!((hw_base <= iova_base) && (iova_end <= hw_end)))
		return -EINVAL;

end:
	*ret_base = iova_base;
	*ret_end = iova_end;
	return 0;
}

/*
 * Checks for "qcom,iommu-dma-addr-pool" property to specify the DMA layer IOVA
 * range for the domain. If not present, and the domain doesn't use fastmap,
 * the domain geometry is unmodified.
 */
static int arm_smmu_adjust_domain_geometry(struct device *dev,
					   struct iommu_domain *domain)
{
	dma_addr_t dma_base, dma_end;
	int ret;

	ret = arm_smmu_get_domain_dma_range(dev, domain,
					    domain->geometry.aperture_start,
					    domain->geometry.aperture_end,
					    &dma_base, &dma_end);
	if (ret)
		return ret;

	domain->geometry.aperture_start = dma_base;
	domain->geometry.aperture_end = dma_end;
	return 0;
}

/* This function assumes that the domain's init mutex is held */
static int arm_smmu_get_dma_cookie(struct device *dev,
				    struct arm_smmu_domain *smmu_domain)
{
	bool is_fast = test_bit(DOMAIN_ATTR_FAST, smmu_domain->attributes);
	bool s1_bypass = test_bit(DOMAIN_ATTR_S1_BYPASS,
				 smmu_domain->attributes);
	struct iommu_domain *domain = &smmu_domain->domain.iommu_domain;
	struct io_pgtable_ops *pgtbl_ops = smmu_domain->pgtbl_ops[0];

	if (s1_bypass)
		return 0;

	else if (is_fast)
		return fast_smmu_init_mapping(dev, domain, pgtbl_ops);

	return iommu_get_dma_cookie(domain);
}

static void arm_smmu_put_dma_cookie(struct iommu_domain *domain)
{
	int s1_bypass = 0, is_fast = 0;

	iommu_domain_get_attr(domain, DOMAIN_ATTR_S1_BYPASS,
					&s1_bypass);
	iommu_domain_get_attr(domain, DOMAIN_ATTR_FAST, &is_fast);

	if (is_fast && IS_ENABLED(CONFIG_IOMMU_IO_PGTABLE_FAST))
		fast_smmu_put_dma_cookie(domain);
	else if (!s1_bypass)
		iommu_put_dma_cookie(domain);
}

static void arm_smmu_domain_get_qcom_quirks(struct arm_smmu_domain *smmu_domain,
					    struct arm_smmu_device *smmu,
					    unsigned long *quirks)
{
	if (test_bit(DOMAIN_ATTR_USE_UPSTREAM_HINT, smmu_domain->attributes))
		*quirks |= IO_PGTABLE_QUIRK_QCOM_USE_UPSTREAM_HINT;
	if (test_bit(DOMAIN_ATTR_USE_LLC_NWA, smmu_domain->attributes))
		*quirks |= IO_PGTABLE_QUIRK_QCOM_USE_LLC_NWA;
}

static int arm_smmu_setup_context_bank(struct arm_smmu_domain *smmu_domain,
				       struct arm_smmu_device *smmu,
				       struct device *dev)
{
	struct iommu_domain *domain = &smmu_domain->domain.iommu_domain;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	bool dynamic = is_dynamic_domain(domain);
	int irq, ret = 0;

	if (!dynamic) {
		/* Initialise the context bank with our page table cfg */
		arm_smmu_init_context_bank(smmu_domain,
					   smmu_domain->pgtbl_info);
		arm_smmu_write_context_bank(smmu, cfg->cbndx,
					    smmu_domain->attributes);

		if (smmu->impl && smmu->impl->init_context_bank)
			smmu->impl->init_context_bank(smmu_domain, dev);

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
		}
	} else {
		cfg->irptndx = INVALID_IRPTNDX;
	}

	return ret;
}

static int arm_smmu_init_domain_context(struct iommu_domain *domain,
					struct arm_smmu_device *smmu,
					struct device *dev)
{
	int start, ret = 0;
	unsigned long ias, oas;
	enum io_pgtable_fmt fmt;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct msm_io_pgtable_info *ttbr0_pgtbl_info =
		&smmu_domain->pgtbl_info[0];
	struct msm_io_pgtable_info *ttbr1_pgtbl_info =
		&smmu_domain->pgtbl_info[1];
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	unsigned long quirks = 0;
	struct io_pgtable *iop;
	bool split_tables = false;

	mutex_lock(&smmu_domain->init_mutex);
	if (smmu_domain->smmu)
		goto out_unlock;

	if (domain->type == IOMMU_DOMAIN_DMA) {
		ret = arm_smmu_setup_default_domain(dev, domain);
		if (ret) {
			dev_err(dev, "%s: default domain setup failed\n",
				__func__);
			goto out_unlock;
		}
	}

	if (domain->type == IOMMU_DOMAIN_IDENTITY) {
		smmu_domain->stage = ARM_SMMU_DOMAIN_BYPASS;
		smmu_domain->smmu = smmu;
		smmu_domain->cfg.irptndx = INVALID_IRPTNDX;
		smmu_domain->cfg.asid = INVALID_ASID;
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
			       ARM_SMMU_FEAT_FMT_AARCH64_4K))) {
		cfg->fmt = ARM_SMMU_CTX_FMT_AARCH64;
		if (smmu_domain->stage == ARM_SMMU_DOMAIN_S1 &&
		    smmu->options & ARM_SMMU_OPT_SPLIT_TABLES)
			split_tables = test_bit(DOMAIN_ATTR_SPLIT_TABLES,
				smmu_domain->attributes);
	}
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
		smmu_domain->flush_ops = &arm_smmu_s1_tlb_ops;
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
			smmu_domain->flush_ops = &arm_smmu_s2_tlb_ops_v2;
		else
			smmu_domain->flush_ops = &arm_smmu_s2_tlb_ops_v1;
		break;
	default:
		ret = -EINVAL;
		goto out_unlock;
	}

#ifdef CONFIG_IOMMU_IO_PGTABLE_FAST
	if (test_bit(DOMAIN_ATTR_FAST, smmu_domain->attributes))
		fmt = ARM_V8L_FAST;
#endif

	if (smmu_domain->non_strict)
		quirks |= IO_PGTABLE_QUIRK_NON_STRICT;
	arm_smmu_domain_get_qcom_quirks(smmu_domain, smmu, &quirks);

	ret = arm_smmu_alloc_cb(domain, smmu, dev);
	if (ret < 0)
		goto out_unlock;

	cfg->cbndx = ret;

	smmu_domain->smmu = smmu;
	if (smmu->impl && smmu->impl->init_context) {
		ret = smmu->impl->init_context(smmu_domain);
		if (ret)
			goto out_clear_smmu;
	}

	ret = arm_smmu_get_domain_iova_range(dev, domain, ias,
					     &ttbr0_pgtbl_info->iova_base,
					     &ttbr0_pgtbl_info->iova_end);
	if (ret) {
		dev_err(dev, "Failed to get domain IOVA range\n");
		goto out_clear_smmu;
	}

	ttbr0_pgtbl_info->pgtbl_cfg = (struct io_pgtable_cfg) {
		.quirks		= quirks,
		.pgsize_bitmap	= smmu->pgsize_bitmap,
		.ias		= ias,
		.oas		= oas,
		.coherent_walk	= is_iommu_pt_coherent(smmu_domain),
		.tlb		= &smmu_domain->flush_ops->tlb,
		.iommu_pgtable_ops = &arm_smmu_pgtable_ops,
		.iommu_dev	= smmu->dev,
	};

	smmu_domain->dev = dev;
	smmu_domain->pgtbl_ops[0] = alloc_io_pgtable_ops(fmt,
						&ttbr0_pgtbl_info->pgtbl_cfg,
						smmu_domain);
	if (!smmu_domain->pgtbl_ops[0]) {
		ret = -ENOMEM;
		goto out_clear_smmu;
	}
	if (split_tables) {
		ttbr1_pgtbl_info->iova_base = ttbr0_pgtbl_info->iova_base;
		ttbr1_pgtbl_info->iova_end = ttbr0_pgtbl_info->iova_end;
		ttbr1_pgtbl_info->pgtbl_cfg = ttbr0_pgtbl_info->pgtbl_cfg;
		smmu_domain->pgtbl_ops[1] = alloc_io_pgtable_ops(fmt,
						&ttbr1_pgtbl_info->pgtbl_cfg,
						smmu_domain);
		if (!smmu_domain->pgtbl_ops[1]) {
			ret = -ENOMEM;
			goto out_clear_smmu;
		}
	}

	iop = container_of(smmu_domain->pgtbl_ops[0], struct io_pgtable, ops);
	ret = iommu_logger_register(&smmu_domain->logger, domain, dev, iop);
	if (ret)
		goto out_clear_smmu;

	/*
	 * Clear the attribute if we didn't actually set up the split tables
	 * so that domain can query itself later
	 */
	if (!split_tables)
		clear_bit(DOMAIN_ATTR_SPLIT_TABLES, smmu_domain->attributes);
	/*
	 * assign any page table memory that might have been allocated
	 * during alloc_io_pgtable_ops
	 */
	arm_smmu_secure_domain_lock(smmu_domain);
	ret = arm_smmu_assign_table(smmu_domain);
	if (ret == -EPROBE_DEFER) {
		arm_smmu_secure_domain_unlock(smmu_domain);
		goto out_clear_smmu;
	}
	arm_smmu_secure_domain_unlock(smmu_domain);

	/* Update the domain's page sizes to reflect the page table format */
	domain->pgsize_bitmap = ttbr0_pgtbl_info->pgtbl_cfg.pgsize_bitmap;
	domain->geometry.aperture_end = (1UL << ias) - 1;
	ret = arm_smmu_adjust_domain_geometry(dev, domain);
	if (ret)
		goto out_logger;
	domain->geometry.force_aperture = true;

	if (domain->type == IOMMU_DOMAIN_DMA) {
		ret = arm_smmu_get_dma_cookie(dev, smmu_domain);
		if (ret)
			goto out_logger;
	}

	/* Assign an asid */
	ret = arm_smmu_init_asid(domain, smmu);
	if (ret)
		goto out_logger;

	ret = arm_smmu_setup_context_bank(smmu_domain, smmu, dev);
	if (ret)
		goto out_logger;

	strlcpy(smmu_domain->domain.name, dev_name(dev),
		sizeof(smmu_domain->domain.name));
	mutex_unlock(&smmu_domain->init_mutex);

	return 0;

out_logger:
	iommu_logger_unregister(smmu_domain->logger);
	smmu_domain->logger = NULL;
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
	int idx = cfg->cbndx;
	int irq;
	bool dynamic;
	int ret;

	if (!smmu || domain->type == IOMMU_DOMAIN_IDENTITY)
		return;

	ret = arm_smmu_rpm_get(smmu);
	if (ret < 0)
		return;

	ret = arm_smmu_power_on(smmu->pwr);
	if (ret) {
		WARN_ONCE(ret, "Woops, powering on smmu %pK failed. Leaking context bank\n",
				smmu);
		arm_smmu_rpm_put(smmu);
		return;
	}

	dynamic = is_dynamic_domain(domain);
	if (dynamic) {
		arm_smmu_free_asid(domain);
		free_io_pgtable_ops(smmu_domain->pgtbl_ops[1]);
		free_io_pgtable_ops(smmu_domain->pgtbl_ops[0]);
		arm_smmu_power_off(smmu, smmu->pwr);
		arm_smmu_rpm_put(smmu);
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
	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_SCTLR, 0);

	if (cfg->irptndx != INVALID_IRPTNDX) {
		irq = smmu->irqs[smmu->num_global_irqs + cfg->irptndx];
		devm_free_irq(smmu->dev, irq, domain);
	}

	free_io_pgtable_ops(smmu_domain->pgtbl_ops[1]);
	free_io_pgtable_ops(smmu_domain->pgtbl_ops[0]);
	arm_smmu_secure_domain_lock(smmu_domain);
	arm_smmu_secure_pool_destroy(smmu_domain);
	arm_smmu_unassign_table(smmu_domain);
	arm_smmu_secure_domain_unlock(smmu_domain);
	__arm_smmu_free_bitmap(smmu->context_map, cfg->cbndx);
	arm_smmu_power_off(smmu, smmu->pwr);
	arm_smmu_rpm_put(smmu);
	arm_smmu_domain_reinit(smmu_domain);
}

static struct iommu_domain *arm_smmu_domain_alloc(unsigned type)
{
	struct arm_smmu_domain *smmu_domain;

	if (type != IOMMU_DOMAIN_UNMANAGED &&
	    type != IOMMU_DOMAIN_IDENTITY &&
	    type != IOMMU_DOMAIN_DMA)
		return NULL;
	/*
	 * Allocate the domain and initialise some of its data structures.
	 * We can't really do anything meaningful until we've added a
	 * master.
	 */
	smmu_domain = kzalloc(sizeof(*smmu_domain), GFP_KERNEL);
	if (!smmu_domain)
		return NULL;

	mutex_init(&smmu_domain->init_mutex);
	spin_lock_init(&smmu_domain->cb_lock);
	spin_lock_init(&smmu_domain->sync_lock);
	INIT_LIST_HEAD(&smmu_domain->pte_info_list);
	INIT_LIST_HEAD(&smmu_domain->unassign_list);
	mutex_init(&smmu_domain->assign_lock);
	INIT_LIST_HEAD(&smmu_domain->secure_pool_list);
	INIT_LIST_HEAD(&smmu_domain->nonsecure_pool);
	arm_smmu_domain_reinit(smmu_domain);

	return &smmu_domain->domain.iommu_domain;
}

static void arm_smmu_domain_free(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);

	/*
	 * Free the domain resources. We assume that all devices have
	 * already been detached.
	 */
	arm_smmu_put_dma_cookie(domain);
	arm_smmu_destroy_domain_context(domain);
	iommu_logger_unregister(smmu_domain->logger);
	kfree(smmu_domain);
}

static int arm_smmu_write_smr(struct arm_smmu_device *smmu, int idx)
{
	struct arm_smmu_smr *smr = smmu->smrs + idx;
	u32 reg = FIELD_PREP(SMR_ID, smr->id) | FIELD_PREP(SMR_MASK, smr->mask);
	u32 val;

	if (!(smmu->features & ARM_SMMU_FEAT_EXIDS) && smr->valid)
		reg |= SMR_VALID;
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_SMR(idx), reg);

	/*
	 * Check if the write went properly. If failed, we would have to fail
	 * the attach sequence to avoid any USF faults being generated in the
	 * future due to device transactions, since this SID entry would not
	 * be present in the stream mapping table.
	 */
	val = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_SMR(idx));
	if (val != reg) {
		dev_err(smmu->dev, "SMR[%d] write err write:0x%lx, read:0x%lx\n",
				idx, reg, val);
		return -EINVAL;
	}

	return 0;
}

static void arm_smmu_write_s2cr(struct arm_smmu_device *smmu, int idx)
{
	struct arm_smmu_s2cr *s2cr = smmu->s2crs + idx;
	u32 reg = FIELD_PREP(S2CR_TYPE, s2cr->type) |
		  FIELD_PREP(S2CR_CBNDX, s2cr->cbndx) |
		  FIELD_PREP(S2CR_PRIVCFG, s2cr->privcfg) |
		  FIELD_PREP(S2CR_SHCFG, S2CR_SHCFG_NSH);

	if (smmu->features & ARM_SMMU_FEAT_EXIDS && smmu->smrs &&
	    smmu->smrs[idx].valid)
		reg |= S2CR_EXIDVALID;
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_S2CR(idx), reg);
}

static int arm_smmu_write_sme(struct arm_smmu_device *smmu, int idx)
{
	arm_smmu_write_s2cr(smmu, idx);
	if (smmu->smrs)
		if (arm_smmu_write_smr(smmu, idx))
			return -EINVAL;

	return 0;
}

/*
 * The width of SMR's mask field depends on sCR0_EXIDENABLE, so this function
 * should be called after sCR0 is written.
 */
static void arm_smmu_test_smr_masks(struct arm_smmu_device *smmu)
{
	unsigned long size;
	u32 id;
	u32 s2cr;
	u32 smr;
	int idx;

	/* Check if Stream Match Register support is included */
	if (!smmu->smrs)
		return;

	/* ID0 */
	id = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_ID0);
	size = FIELD_GET(ID0_NUMSMRG, id);

	/*
	 * Few SMR registers may be inuse before the smmu driver
	 * probes(say by the bootloader). Find a SMR register
	 * which is not inuse.
	 */
	for (idx = 0; idx < size; idx++) {
		if (smmu->features & ARM_SMMU_FEAT_EXIDS) {
			s2cr = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_S2CR(idx));
			if (!FIELD_GET(S2CR_EXIDVALID, s2cr))
				break;
		} else {
			smr = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_SMR(idx));
			if (!FIELD_GET(SMR_VALID, smr))
				break;
		}
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
	smr = FIELD_PREP(SMR_ID, smmu->streamid_mask);
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_SMR(idx), smr);
	smr = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_SMR(idx));
	smmu->streamid_mask = FIELD_GET(SMR_ID, smr);

	smr = FIELD_PREP(SMR_MASK, smmu->streamid_mask);
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_SMR(idx), smr);
	smr = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_SMR(idx));
	smmu->smr_mask_mask = FIELD_GET(SMR_MASK, smr);
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
	bool pinned = smmu->s2crs[idx].pinned;
	u8 cbndx = smmu->s2crs[idx].cbndx;;

	if (--smmu->s2crs[idx].count)
		return false;

	smmu->s2crs[idx] = s2cr_init_val;
	if (pinned) {
		smmu->s2crs[idx].pinned = true;
		smmu->s2crs[idx].cbndx = cbndx;
	} else if (smmu->smrs) {
		smmu->smrs[idx].valid = false;
	}

	return true;
}

static int arm_smmu_master_alloc_smes(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct arm_smmu_master_cfg *cfg = fwspec->iommu_priv;
	struct arm_smmu_device *smmu = cfg->smmu;
	struct arm_smmu_smr *smrs = smmu->smrs;
	struct iommu_group *group;
	int i, idx, ret;

	mutex_lock(&smmu->iommu_group_mutex);
	mutex_lock(&smmu->stream_map_mutex);
	/* Figure out a viable stream map entry allocation */
	for_each_cfg_sme(fwspec, i, idx) {
		u16 sid = FIELD_GET(SMR_ID, fwspec->ids[i]);
		u16 mask = FIELD_GET(SMR_MASK, fwspec->ids[i]);

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
	const struct iommu_flush_ops *tlb;

	tlb = smmu_domain->pgtbl_info[0].pgtbl_cfg.tlb;

	mutex_lock(&smmu->stream_map_mutex);
	for_each_cfg_sme(fwspec, i, idx) {
		if (WARN_ON(s2cr[idx].attach_count == 0)) {
			mutex_unlock(&smmu->stream_map_mutex);
			return;
		}
		s2cr[idx].attach_count -= 1;

		if (s2cr[idx].attach_count > 0)
			continue;

		arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_SMR(idx), 0);
		arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_S2CR(idx), 0);
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
		if (arm_smmu_write_sme(smmu, idx)) {
			mutex_unlock(&smmu->stream_map_mutex);
			return -EINVAL;
		}
	}
	mutex_unlock(&smmu->stream_map_mutex);

	return 0;
}

static void arm_smmu_detach_dev(struct iommu_domain *domain,
				struct device *dev)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	bool dynamic = is_dynamic_domain(domain);
	bool atomic_domain = test_bit(DOMAIN_ATTR_ATOMIC,
				      smmu_domain->attributes);

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
	arm_smmu_power_off(smmu, smmu->pwr);
}

static int arm_smmu_assign_table(struct arm_smmu_domain *smmu_domain)
{
	int ret = 0;
	int dest_vmids[2] = {VMID_HLOS, smmu_domain->secure_vmid};
	int dest_perms[2] = {PERM_READ | PERM_WRITE, PERM_READ};
	int source_vmid = VMID_HLOS;
	struct arm_smmu_pte_info *pte_info, *temp;

	if (!arm_smmu_has_secure_vmid(smmu_domain))
		return ret;

	list_for_each_entry(pte_info, &smmu_domain->pte_info_list, entry) {
		ret = hyp_assign_phys(virt_to_phys(pte_info->virt_addr),
				      PAGE_SIZE, &source_vmid, 1,
				      dest_vmids, dest_perms, 2);

		if (ret == -EPROBE_DEFER)
			return ret;

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

	if (!arm_smmu_has_secure_vmid(smmu_domain))
		return;

	list_for_each_entry(pte_info, &smmu_domain->unassign_list, entry) {
		ret = hyp_assign_phys(virt_to_phys(pte_info->virt_addr),
				      PAGE_SIZE, source_vmlist, 2,
				      &dest_vmids, &dest_perms, 1);

		if (ret == -EPROBE_DEFER)
			return;

		if (WARN_ON(ret))
			break;
		free_pages((unsigned long)pte_info->virt_addr,
			   get_order(pte_info->size));
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

	if (!arm_smmu_has_secure_vmid(smmu_domain)) {
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

	if (!arm_smmu_has_secure_vmid(smmu_domain)) {
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

	if (test_bit(DOMAIN_ATTR_ATOMIC, smmu_domain->attributes) ||
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

static void arm_smmu_release_prealloc_memory(
		struct arm_smmu_domain *smmu_domain, struct list_head *list)
{
	struct page *page, *tmp;

	list_for_each_entry_safe(page, tmp, list, lru) {
		list_del(&page->lru);
		__free_pages(page, 0);
	}
}

static struct device_node *arm_iommu_get_of_node(struct device *dev)
{
	struct device_node *np;

	if (!dev->of_node)
		return NULL;

	np = of_parse_phandle(dev->of_node, "qcom,iommu-group", 0);
	return np ? np : dev->of_node;
}

static int arm_smmu_setup_default_domain(struct device *dev,
					 struct iommu_domain *domain)
{
	struct device_node *np;
	int ret;
	const char *str;
	int attr = 1;
	u32 val;

	np = arm_iommu_get_of_node(dev);
	if (!np)
		return 0;

	ret = of_property_read_string(np, "qcom,iommu-dma", &str);
	if (ret)
		str = "default";

	if (!strcmp(str, "bypass")) {
		__arm_smmu_domain_set_attr(
			domain, DOMAIN_ATTR_S1_BYPASS, &attr);
	} else if (!strcmp(str, "fastmap")) {
		/* Ensure DOMAIN_ATTR_ATOMIC is set for GKI */
		__arm_smmu_domain_set_attr(
			domain, DOMAIN_ATTR_ATOMIC, &attr);
		__arm_smmu_domain_set_attr(
			domain, DOMAIN_ATTR_FAST, &attr);
	} else if (!strcmp(str, "atomic")) {
		__arm_smmu_domain_set_attr(
			domain, DOMAIN_ATTR_ATOMIC, &attr);
	} else if (!strcmp(str, "disabled")) {
		/*
		 * Don't touch hw, and don't allocate irqs or other resources.
		 * Ensure the context bank is set to a valid value per dynamic
		 * attr requirement.
		 */
		set_bit(DOMAIN_ATTR_DYNAMIC,
			to_smmu_domain(domain)->attributes);
		val = 0;
		__arm_smmu_domain_set_attr(
			domain, DOMAIN_ATTR_CONTEXT_BANK, &val);
	}

	/*
	 * default value:
	 * Stall-on-fault
	 * faults trigger kernel panic
	 * return abort
	 */
	if (of_property_match_string(np, "qcom,iommu-faults",
				     "stall-disable") >= 0)
		__arm_smmu_domain_set_attr(domain,
			DOMAIN_ATTR_FAULT_MODEL_NO_STALL, &attr);

	if (of_property_match_string(np, "qcom,iommu-faults", "no-CFRE") >= 0)
		__arm_smmu_domain_set_attr(
			domain, DOMAIN_ATTR_FAULT_MODEL_NO_CFRE, &attr);

	if (of_property_match_string(np, "qcom,iommu-faults", "HUPCF") >= 0)
		__arm_smmu_domain_set_attr(
			domain, DOMAIN_ATTR_FAULT_MODEL_HUPCF, &attr);

	if (of_property_match_string(np, "qcom,iommu-faults", "non-fatal") >= 0)
		__arm_smmu_domain_set_attr(domain,
			DOMAIN_ATTR_NON_FATAL_FAULTS, &attr);

	/* Default value: disabled */
	ret = of_property_read_u32(np, "qcom,iommu-vmid", &val);
	if (!ret) {
		__arm_smmu_domain_set_attr(
			domain, DOMAIN_ATTR_SECURE_VMID, &val);
	}

	/* Default value: disabled */
	ret = of_property_read_string(np, "qcom,iommu-pagetable", &str);
	if (ret)
		str = "disabled";
	if (!strcmp(str, "coherent"))
		__arm_smmu_domain_set_attr(domain,
			DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT, &attr);
	else if (!strcmp(str, "LLC"))
		__arm_smmu_domain_set_attr(domain,
			DOMAIN_ATTR_USE_UPSTREAM_HINT, &attr);
	else if (!strcmp(str, "LLC_NWA"))
		__arm_smmu_domain_set_attr(domain,
			DOMAIN_ATTR_USE_LLC_NWA, &attr);


	/* Default value: disabled */
	if (of_property_read_bool(np, "qcom,iommu-earlymap"))
		__arm_smmu_domain_set_attr(domain,
			DOMAIN_ATTR_EARLY_MAP, &attr);
	return 0;
}

struct lookup_iommu_group_data {
	struct device_node *np;
	struct iommu_group *group;
};

/* This isn't a "fast lookup" since its N^2, but probably good enough */
static int __bus_lookup_iommu_group(struct device *dev, void *priv)
{
	struct lookup_iommu_group_data *data = priv;
	struct device_node *np;
	struct iommu_group *group;

	group = iommu_group_get(dev);
	if (!group)
		return 0;

	np = of_parse_phandle(dev->of_node, "qcom,iommu-group", 0);
	if (np != data->np) {
		iommu_group_put(group);
		return 0;
	}

	data->group = group;
	return 1;
}

static struct iommu_group *of_get_device_group(struct device *dev)
{
	struct lookup_iommu_group_data data = {
		.np = NULL,
		.group = NULL,
	};
	struct iommu_group *group;
	int ret;

	data.np = of_parse_phandle(dev->of_node, "qcom,iommu-group", 0);
	if (!data.np)
		return NULL;

	ret = bus_for_each_dev(&platform_bus_type, NULL, &data,
				__bus_lookup_iommu_group);
	if (ret > 0)
		return data.group;

#ifdef CONFIG_PCI
	ret = bus_for_each_dev(&pci_bus_type, NULL, &data,
				__bus_lookup_iommu_group);
	if (ret > 0)
		return data.group;
#endif

	group = generic_device_group(dev);
	if (IS_ERR(group))
		return NULL;
	return group;
}

static int arm_smmu_attach_dev(struct iommu_domain *domain, struct device *dev)
{
	int ret;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct arm_smmu_device *smmu;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	int s1_bypass = 0;

	if (!fwspec || fwspec->ops != &arm_smmu_ops.iommu_ops) {
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

	ret = arm_smmu_rpm_get(smmu);
	if (ret < 0)
		return ret;

	/* Enable Clocks and Power */
	ret = arm_smmu_power_on(smmu->pwr);
	if (ret) {
		arm_smmu_rpm_put(smmu);
		return ret;
	}

	/* Ensure that the domain is finalised */
	ret = arm_smmu_init_domain_context(domain, smmu, dev);
	if (ret < 0)
		goto out_power_off;

	ret = arm_smmu_domain_get_attr(domain, DOMAIN_ATTR_S1_BYPASS,
					&s1_bypass);
	if (s1_bypass)
		domain->type = IOMMU_DOMAIN_UNMANAGED;

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
	if (!ret && test_bit(DOMAIN_ATTR_ATOMIC, smmu_domain->attributes)) {
		WARN_ON(arm_smmu_power_on(smmu->pwr));
		arm_smmu_power_off_atomic(smmu, smmu->pwr);
	}

	arm_smmu_power_off(smmu, smmu->pwr);
	arm_smmu_rpm_put(smmu);

	return ret;
}

static struct io_pgtable_ops *arm_smmu_get_pgtable_ops(
					struct arm_smmu_domain *smmu_domain,
					unsigned long iova)
{
	struct io_pgtable_cfg *cfg = &smmu_domain->pgtbl_info[0].pgtbl_cfg;
	long iova_ext_bits = (s64)iova >> cfg->ias;
	unsigned int idx = 0;
	bool split_tables_domain = test_bit(DOMAIN_ATTR_SPLIT_TABLES,
					    smmu_domain->attributes);

	if (!split_tables_domain)
		return smmu_domain->pgtbl_ops[0];

	if (iova_ext_bits) {
		iova_ext_bits = ~iova_ext_bits;
		idx = 1;
	}

	if (WARN_ON(iova_ext_bits))
		return ERR_PTR(-ERANGE);

	return smmu_domain->pgtbl_ops[idx];
}

/*
 * The ARM IO-Page-table code assumes that all mappings are for TTBR0. For
 * devices that use the upper portion of the IOVA space, this is a problem
 * as the upper portion of the address space is beyond the TTBR0 space, so the
 * IOVA code will forbid the mapping. To circumvent this, unconditionally mask
 * the sign extended bits. This should be okay, as those are the only bits
 * that are relevant anyway for indexing into the page tables.
 */
static unsigned long arm_smmu_mask_iova(struct arm_smmu_domain *smmu_domain,
					unsigned long iova)
{
	unsigned int ias = smmu_domain->pgtbl_info[0].pgtbl_cfg.ias;
	unsigned long mask = (1UL << ias) - 1;

	if (!test_bit(DOMAIN_ATTR_SPLIT_TABLES, smmu_domain->attributes))
		return iova;

	return iova & mask;
}

static int arm_smmu_map(struct iommu_domain *domain, unsigned long iova,
			phys_addr_t paddr, size_t size, int prot)
{
	int ret;
	unsigned long flags;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops;
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	LIST_HEAD(nonsecure_pool);

	ops = arm_smmu_get_pgtable_ops(smmu_domain, iova);
	if (IS_ERR(ops))
		return PTR_ERR(ops);
	else if (!ops)
		return -EINVAL;

	if (!IS_ENABLED(CONFIG_ARM_SMMU_SKIP_MAP_POWER_ON)) {
		ret = arm_smmu_domain_power_on(domain, smmu_domain->smmu);
		if (ret)
			return ret;
	}

	iova = arm_smmu_mask_iova(smmu_domain, iova);
	arm_smmu_secure_domain_lock(smmu_domain);
	arm_smmu_rpm_get(smmu);
	spin_lock_irqsave(&smmu_domain->cb_lock, flags);
	ret = ops->map(ops, iova, paddr, size, prot);

	arm_smmu_deferred_flush(smmu_domain);

	spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);
	arm_smmu_rpm_put(smmu);

	/* if the map call failed due to insufficient memory,
	 * then retry again with preallocated memory to see
	 * if the map call succeeds.
	 */
	if (ret == -ENOMEM) {
		arm_smmu_prealloc_memory(smmu_domain, size, &nonsecure_pool);
		arm_smmu_rpm_get(smmu);
		spin_lock_irqsave(&smmu_domain->cb_lock, flags);
		list_splice_init(&nonsecure_pool, &smmu_domain->nonsecure_pool);
		ret = ops->map(ops, iova, paddr, size, prot);
		list_splice_init(&smmu_domain->nonsecure_pool, &nonsecure_pool);
		arm_smmu_deferred_flush(smmu_domain);
		spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);
		arm_smmu_rpm_put(smmu);
		arm_smmu_release_prealloc_memory(smmu_domain, &nonsecure_pool);

	}

	if (!IS_ENABLED(CONFIG_ARM_SMMU_SKIP_MAP_POWER_ON))
		arm_smmu_domain_power_off(domain, smmu_domain->smmu);

	arm_smmu_assign_table(smmu_domain);
	arm_smmu_secure_domain_unlock(smmu_domain);

	return ret;
}

static uint64_t arm_smmu_iova_to_pte(struct iommu_domain *domain,
	      dma_addr_t iova)
{
	uint64_t ret;
	unsigned long flags;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct msm_io_pgtable_info *pgtbl_info = &smmu_domain->pgtbl_info[0];
	struct io_pgtable_ops *ops;

	if (!pgtbl_info->iova_to_pte)
		return 0;

	ops = arm_smmu_get_pgtable_ops(smmu_domain, iova);
	if (IS_ERR_OR_NULL(ops))
		return 0;
	iova = arm_smmu_mask_iova(smmu_domain, iova);
	spin_lock_irqsave(&smmu_domain->cb_lock, flags);
	ret = pgtbl_info->iova_to_pte(ops, iova);
	spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);
	return ret;
}

static size_t arm_smmu_unmap(struct iommu_domain *domain, unsigned long iova,
			     size_t size, struct iommu_iotlb_gather *gather)
{
	size_t ret;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct io_pgtable_ops *ops;
	unsigned long flags;

	ops = arm_smmu_get_pgtable_ops(smmu_domain, iova);
	if (IS_ERR_OR_NULL(ops))
		return 0;
	iova = arm_smmu_mask_iova(smmu_domain, iova);
	ret = arm_smmu_domain_power_on(domain, smmu_domain->smmu);
	if (ret)
		return ret;

	arm_smmu_secure_domain_lock(smmu_domain);

	arm_smmu_rpm_get(smmu);
	spin_lock_irqsave(&smmu_domain->cb_lock, flags);
	ret = ops->unmap(ops, iova, size, gather);
	spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);
	arm_smmu_rpm_put(smmu);

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

static void arm_smmu_flush_iotlb_all(struct iommu_domain *domain)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;

	if (smmu_domain->flush_ops) {
		arm_smmu_rpm_get(smmu);
		if (arm_smmu_domain_power_on(domain, smmu)) {
			WARN_ON(1);
			arm_smmu_rpm_put(smmu);
			return;
		}
		smmu_domain->flush_ops->tlb.tlb_flush_all(smmu_domain);
		arm_smmu_domain_power_off(domain, smmu);
		arm_smmu_rpm_put(smmu);
	}
}

static void arm_smmu_iotlb_sync(struct iommu_domain *domain,
				struct iommu_iotlb_gather *gather)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;

	if (smmu_domain->flush_ops) {
		arm_smmu_rpm_get(smmu);
		if (arm_smmu_domain_power_on(domain, smmu)) {
			WARN_ON(1);
			arm_smmu_rpm_put(smmu);
			return;
		}
		smmu_domain->flush_ops->tlb_sync(smmu_domain);
		arm_smmu_domain_power_off(domain, smmu);
		arm_smmu_rpm_put(smmu);
	}
}

#define MAX_MAP_SG_BATCH_SIZE (SZ_4M)
static size_t arm_smmu_map_sg(struct iommu_domain *domain, unsigned long iova,
			   struct scatterlist *sg, unsigned int nents, int prot)
{
	int ret;
	size_t size, batch_size, size_to_unmap = 0;
	unsigned long flags;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops;
	struct msm_io_pgtable_info *pgtbl_info = &smmu_domain->pgtbl_info[0];
	unsigned int idx_start, idx_end;
	struct scatterlist *sg_start, *sg_end;
	unsigned long __saved_iova_start;
	LIST_HEAD(nonsecure_pool);

	if (!pgtbl_info->map_sg)
		return 0;

	ops = arm_smmu_get_pgtable_ops(smmu_domain, iova);
	if (IS_ERR_OR_NULL(ops))
		return 0;
	iova = arm_smmu_mask_iova(smmu_domain, iova);

	if (!IS_ENABLED(CONFIG_ARM_SMMU_SKIP_MAP_POWER_ON)) {
		ret = arm_smmu_domain_power_on(domain, smmu_domain->smmu);
		if (ret)
			return ret;
	}

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
		ret = pgtbl_info->map_sg(ops, iova, sg_start,
					 idx_end - idx_start, prot, &size);
		arm_smmu_deferred_flush(smmu_domain);
		spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);

		if (ret == -ENOMEM) {
			/* unmap any partially mapped iova */
			if (size) {
				arm_smmu_secure_domain_unlock(smmu_domain);
				arm_smmu_unmap(domain, iova, size, NULL);
				arm_smmu_secure_domain_lock(smmu_domain);
			}
			arm_smmu_prealloc_memory(smmu_domain,
						 batch_size, &nonsecure_pool);
			spin_lock_irqsave(&smmu_domain->cb_lock, flags);
			list_splice_init(&nonsecure_pool,
					 &smmu_domain->nonsecure_pool);
			ret = pgtbl_info->map_sg(ops, iova, sg_start,
						 idx_end - idx_start, prot,
						 &size);
			list_splice_init(&smmu_domain->nonsecure_pool,
					 &nonsecure_pool);
			arm_smmu_deferred_flush(smmu_domain);
			spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);
			arm_smmu_release_prealloc_memory(smmu_domain,
							 &nonsecure_pool);
		}

		/* Returns -ve val on error */
		if (ret < 0) {
			size_to_unmap = iova + size - __saved_iova_start;
			goto out;
		}

		iova += batch_size;
		idx_start = idx_end;
		sg_start = sg_end;
		size = 0;
	}

out:
	if (!IS_ENABLED(CONFIG_ARM_SMMU_SKIP_MAP_POWER_ON))
		arm_smmu_domain_power_off(domain, smmu_domain->smmu);

	arm_smmu_assign_table(smmu_domain);
	arm_smmu_secure_domain_unlock(smmu_domain);

	if (size_to_unmap) {
		arm_smmu_unmap(domain, __saved_iova_start, size_to_unmap, NULL);
		iova = __saved_iova_start;
	}
	return iova - __saved_iova_start;
}

static phys_addr_t __arm_smmu_iova_to_phys_hard(struct iommu_domain *domain,
					      dma_addr_t iova)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;
	struct arm_smmu_cfg *cfg = &smmu_domain->cfg;
	struct io_pgtable_ops *ops;
	struct device *dev = smmu->dev;
	void __iomem *reg;
	u32 tmp;
	u64 phys;
	unsigned long va;
	int idx = cfg->cbndx;

	ops = arm_smmu_get_pgtable_ops(smmu_domain, iova);
	if (IS_ERR_OR_NULL(ops))
		return 0;

	va = iova & ~0xfffUL;
	if (cfg->fmt == ARM_SMMU_CTX_FMT_AARCH64)
		arm_smmu_cb_writeq(smmu, idx, ARM_SMMU_CB_ATS1PR, va);
	else
		arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_ATS1PR, va);

	reg = arm_smmu_page(smmu, ARM_SMMU_CB(smmu, idx)) + ARM_SMMU_CB_ATSR;
	if (readl_poll_timeout_atomic(reg, tmp, !(tmp & ATSR_ACTIVE), 5, 50)) {
		phys = ops->iova_to_phys(ops, iova);
		dev_err(dev,
			"iova to phys timed out on %pad. software table walk result=%pa.\n",
			&iova, &phys);
		phys = 0;
		return phys;
	}

	phys = arm_smmu_cb_readq(smmu, idx, ARM_SMMU_CB_PAR);
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
	struct io_pgtable_ops *ops;

	ops = arm_smmu_get_pgtable_ops(smmu_domain, iova);
	iova = arm_smmu_mask_iova(smmu_domain, iova);
	if (domain->type == IOMMU_DOMAIN_IDENTITY)
		return iova;

	if (IS_ERR_OR_NULL(ops))
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
				    dma_addr_t iova, unsigned long trans_flags)
{
	phys_addr_t ret = 0;
	unsigned long flags;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct arm_smmu_device *smmu = smmu_domain->smmu;

	if (smmu->options & ARM_SMMU_OPT_DISABLE_ATOS)
		return 0;

	if (arm_smmu_power_on(smmu_domain->smmu->pwr))
		return 0;

	if (smmu->impl && smmu->impl->iova_to_phys_hard) {
		ret = smmu->impl->iova_to_phys_hard(smmu_domain, iova,
						    trans_flags);
		goto out;
	}

	spin_lock_irqsave(&smmu_domain->cb_lock, flags);
	if (smmu_domain->smmu->features & ARM_SMMU_FEAT_TRANS_OPS &&
			smmu_domain->stage == ARM_SMMU_DOMAIN_S1)
		ret = __arm_smmu_iova_to_phys_hard(domain, iova);

	spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);

out:
	arm_smmu_power_off(smmu, smmu_domain->smmu->pwr);

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

static
struct arm_smmu_device *arm_smmu_get_by_fwnode(struct fwnode_handle *fwnode)
{
	struct device *dev = driver_find_device_by_fwnode(&arm_smmu_driver.driver,
							  fwnode);
	put_device(dev);
	return dev ? dev_get_drvdata(dev) : NULL;
}

static int arm_smmu_add_device(struct device *dev)
{
	struct arm_smmu_device *smmu;
	struct arm_smmu_master_cfg *cfg;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct device_link *link;
	int i, ret;

	if (using_legacy_binding) {
		ret = arm_smmu_register_legacy_master(dev, &smmu);

		/*
		 * If dev->iommu_fwspec is initally NULL, arm_smmu_register_legacy_master()
		 * will allocate/initialise a new one. Thus we need to update fwspec for
		 * later use.
		 */
		fwspec = dev_iommu_fwspec_get(dev);
		if (ret)
			goto out_free;
	} else if (fwspec && fwspec->ops == &arm_smmu_ops.iommu_ops) {
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
		u16 sid = FIELD_GET(SMR_ID, fwspec->ids[i]);
		u16 mask = FIELD_GET(SMR_MASK, fwspec->ids[i]);

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

	link = device_link_add(dev, smmu->dev, DL_FLAG_STATELESS);
	if (!link) {
		dev_err(dev, "error in device link creation between %s & %s\n",
				dev_name(smmu->dev), dev_name(dev));
		ret = -ENODEV;
		goto out_cfg_free;
	}

	ret = arm_smmu_master_alloc_smes(dev);
	if (ret)
		goto out_dev_link_free;
	arm_smmu_power_off(smmu, smmu->pwr);
	return 0;

out_dev_link_free:
	device_link_del(link);
out_cfg_free:
	kfree(cfg);
out_pwr_off:
	arm_smmu_power_off(smmu, smmu->pwr);
out_free:
	iommu_fwspec_free(dev);
	return ret;
}

static void arm_smmu_remove_device(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct arm_smmu_device *smmu;
	struct device_link *link;
	int ret;

	if (!fwspec || fwspec->ops != &arm_smmu_ops.iommu_ops)
		return;

	smmu = fwspec_smmu(fwspec);

	ret = arm_smmu_rpm_get(smmu);
	if (ret < 0)
		return;

	if (arm_smmu_power_on(smmu->pwr)) {
		WARN_ON(1);
		arm_smmu_rpm_put(smmu);
		return;
	}

	/* Remove the device link between dev and the smmu if any */
	list_for_each_entry(link, &smmu->dev->links.consumers, s_node) {
		if (link->consumer == dev)
			device_link_del(link);
	}

	arm_smmu_master_free_smes(fwspec);
	iommu_group_remove_device(dev);
	kfree(fwspec->iommu_priv);
	iommu_fwspec_free(dev);
	arm_smmu_power_off(smmu, smmu->pwr);
	arm_smmu_rpm_put(smmu);
}

static struct iommu_group *arm_smmu_device_group(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct arm_smmu_device *smmu = fwspec_smmu(fwspec);
	struct iommu_group *group = NULL;
	int i, idx;

	group = of_get_device_group(dev);
	if (group)
		goto finish;

	for_each_cfg_sme(fwspec, i, idx) {
		if (group && smmu->s2crs[idx].group &&
		    group != smmu->s2crs[idx].group) {
			dev_err(dev, "ID:%x IDX:%x is already in a group!\n",
				fwspec->ids[i], idx);
			return ERR_PTR(-EINVAL);
		}

		if (!group)
			group = smmu->s2crs[idx].group;
	}

	if (group)
		iommu_group_ref_get(group);
	else {
		if (dev_is_pci(dev))
			group = pci_device_group(dev);
		else if (dev_is_fsl_mc(dev))
			group = fsl_mc_device_group(dev);
		else
			group = generic_device_group(dev);

		if (IS_ERR(group))
			return NULL;
	}

finish:
	if (smmu->impl && smmu->impl->device_group &&
	    smmu->impl->device_group(dev, group)) {
		iommu_group_put(group);
		return ERR_PTR(-EINVAL);
	}

	return group;
}

static int arm_smmu_domain_get_attr(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_cfg *pgtbl_cfg =
		&smmu_domain->pgtbl_info[0].pgtbl_cfg;
	int ret = 0;
	unsigned long iommu_attr = (unsigned long)attr;

	mutex_lock(&smmu_domain->init_mutex);
	switch (iommu_attr) {
	case DOMAIN_ATTR_NESTING:
		*(int *)data = (smmu_domain->stage == ARM_SMMU_DOMAIN_NESTED);
		ret = 0;
		break;
	case DOMAIN_ATTR_DMA_USE_FLUSH_QUEUE:
		*(int *)data = smmu_domain->non_strict;
		ret = 0;
		break;
	case DOMAIN_ATTR_PT_BASE_ADDR:
		*((phys_addr_t *)data) = pgtbl_cfg->arm_lpae_s1_cfg.ttbr[0];
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
		val = pgtbl_cfg->arm_lpae_s1_cfg.ttbr[0];
		if (smmu_domain->cfg.cbar != CBAR_TYPE_S2_TRANS)
			val |= FIELD_PREP(TTBRn_ASID, ARM_SMMU_CB_ASID(smmu,
							&smmu_domain->cfg));
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
		*((int *)data) = test_bit(DOMAIN_ATTR_DYNAMIC,
					  smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_NON_FATAL_FAULTS:
		*((int *)data) = test_bit(DOMAIN_ATTR_NON_FATAL_FAULTS,
					  smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_S1_BYPASS:
		*((int *)data) = test_bit(DOMAIN_ATTR_S1_BYPASS,
					  smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_SECURE_VMID:
		*((int *)data) = smmu_domain->secure_vmid;
		ret = 0;
		break;
	case DOMAIN_ATTR_PGTBL_INFO: {
		struct iommu_pgtbl_info *info = data;

		if (!test_bit(DOMAIN_ATTR_FAST, smmu_domain->attributes)) {
			ret = -ENODEV;
			break;
		}
		info->ops = smmu_domain->pgtbl_ops[0];
		ret = 0;
		break;
	}
	case DOMAIN_ATTR_FAST:
		*((int *)data) = test_bit(DOMAIN_ATTR_FAST,
					  smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_USE_UPSTREAM_HINT:
		*((int *)data) = test_bit(DOMAIN_ATTR_USE_UPSTREAM_HINT,
					  smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_USE_LLC_NWA:
		*((int *)data) = test_bit(DOMAIN_ATTR_USE_LLC_NWA,
					  smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_EARLY_MAP:
		*((int *)data) = test_bit(DOMAIN_ATTR_EARLY_MAP,
					  smmu_domain->attributes);
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
		*((int *)data) = test_bit(DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT,
					  smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_FAULT_MODEL_NO_CFRE:
	case DOMAIN_ATTR_FAULT_MODEL_NO_STALL:
	case DOMAIN_ATTR_FAULT_MODEL_HUPCF:
		*((int *)data) = test_bit(attr, smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_SPLIT_TABLES:
		*((int *)data) = test_bit(DOMAIN_ATTR_SPLIT_TABLES,
					  smmu_domain->attributes);
		ret = 0;
		break;
	default:
		ret = -ENODEV;
		break;
	}
	mutex_unlock(&smmu_domain->init_mutex);
	return ret;
}

static int __arm_smmu_domain_set_attr2(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data);
static int __arm_smmu_domain_set_attr(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data)
{
	int ret = 0;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	unsigned long iommu_attr = (unsigned long)attr;

	switch (iommu_attr) {
	case DOMAIN_ATTR_NESTING:
		if (smmu_domain->smmu) {
			ret = -EPERM;
			goto out;
		}

		if (*(int *)data)
			smmu_domain->stage = ARM_SMMU_DOMAIN_NESTED;
		else
			smmu_domain->stage = ARM_SMMU_DOMAIN_S1;

		break;
	case DOMAIN_ATTR_DMA_USE_FLUSH_QUEUE:
		smmu_domain->non_strict = *(int *)data;
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

		if (IS_ENABLED(CONFIG_IOMMU_DYNAMIC_DOMAINS)) {
			if (smmu_domain->smmu != NULL) {
				dev_err(smmu_domain->smmu->dev,
				  "cannot change dynamic attribute while attached\n");
				ret = -EBUSY;
				break;
			}

			if (dynamic)
				set_bit(DOMAIN_ATTR_DYNAMIC,
					smmu_domain->attributes);
			else
				clear_bit(DOMAIN_ATTR_DYNAMIC,
					  smmu_domain->attributes);
			ret = 0;
		} else {
			ret = -ENOTSUPP;
		}
		break;
	}
	case DOMAIN_ATTR_CONTEXT_BANK:
		/* context bank can't be set while attached */
		if (smmu_domain->smmu != NULL) {
			ret = -EBUSY;
			break;
		}
		/* ... and it can only be set for dynamic contexts. */
		if (!test_bit(DOMAIN_ATTR_DYNAMIC, smmu_domain->attributes)) {
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
			set_bit(DOMAIN_ATTR_NON_FATAL_FAULTS,
				smmu_domain->attributes);
		else
			clear_bit(DOMAIN_ATTR_NON_FATAL_FAULTS,
				  smmu_domain->attributes);
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
			set_bit(DOMAIN_ATTR_S1_BYPASS, smmu_domain->attributes);
		else
			clear_bit(DOMAIN_ATTR_S1_BYPASS,
				  smmu_domain->attributes);

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
			set_bit(DOMAIN_ATTR_ATOMIC, smmu_domain->attributes);
		else
			clear_bit(DOMAIN_ATTR_ATOMIC, smmu_domain->attributes);
		break;
	}
	case DOMAIN_ATTR_SECURE_VMID:
		/* can't be changed while attached */
		if (smmu_domain->smmu != NULL) {
			ret = -EBUSY;
			break;
		}

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
		/* can't be changed while attached */
		if (smmu_domain->smmu != NULL) {
			ret = -EBUSY;
			break;
		}

		if (*((int *)data)) {
			if (IS_ENABLED(CONFIG_IOMMU_IO_PGTABLE_FAST)) {
				set_bit(DOMAIN_ATTR_FAST,
					smmu_domain->attributes);
				set_bit(DOMAIN_ATTR_ATOMIC,
					smmu_domain->attributes);
				ret = 0;
			} else {
				ret = -ENOTSUPP;
			}
		}
		break;
	default:
		ret = __arm_smmu_domain_set_attr2(domain, attr, data);
	}
out:
	return ret;
}

/* yeee-haw */
static int __arm_smmu_domain_set_attr2(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	int ret = 0;
	unsigned long iommu_attr = (unsigned long)attr;

	switch (iommu_attr) {
	case DOMAIN_ATTR_USE_UPSTREAM_HINT:
	case DOMAIN_ATTR_USE_LLC_NWA:
		if (IS_ENABLED(CONFIG_QCOM_IOMMU_IO_PGTABLE_QUIRKS)) {

			/* can't be changed while attached */
			if (smmu_domain->smmu != NULL) {
				ret = -EBUSY;
			} else if (*((int *)data)) {
				set_bit(attr, smmu_domain->attributes);
				ret = 0;
			}
		} else {
			ret = -ENOTSUPP;
		}
		break;
	case DOMAIN_ATTR_EARLY_MAP: {
		int early_map = *((int *)data);

		ret = 0;
		if (early_map) {
			set_bit(DOMAIN_ATTR_EARLY_MAP, smmu_domain->attributes);
		} else {
			if (smmu_domain->smmu)
				ret = arm_smmu_enable_s1_translations(
								smmu_domain);

			if (!ret)
				clear_bit(DOMAIN_ATTR_EARLY_MAP,
					  smmu_domain->attributes);
		}
		break;
	}
	case DOMAIN_ATTR_FAULT_MODEL_NO_CFRE:
	case DOMAIN_ATTR_FAULT_MODEL_NO_STALL:
	case DOMAIN_ATTR_FAULT_MODEL_HUPCF:
		if (*((int *)data))
			set_bit(attr, smmu_domain->attributes);
		ret = 0;
		break;
	case DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT: {
		int force_coherent = *((int *)data);

		if (smmu_domain->smmu != NULL) {
			dev_err(smmu_domain->smmu->dev,
			  "cannot change force coherent attribute while attached\n");
			ret = -EBUSY;
		} else if (force_coherent) {
			set_bit(DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT,
				smmu_domain->attributes);
			ret = 0;
		} else {
			clear_bit(DOMAIN_ATTR_PAGE_TABLE_FORCE_COHERENT,
				  smmu_domain->attributes);
			ret = 0;
		}
		break;
	}
	case DOMAIN_ATTR_SPLIT_TABLES: {
		int split_tables = *((int *)data);
		/* can't be changed while attached */
		if (smmu_domain->smmu != NULL) {
			ret = -EBUSY;
		} else if (split_tables) {
			set_bit(DOMAIN_ATTR_SPLIT_TABLES,
				smmu_domain->attributes);
			ret = 0;
		} else {
			clear_bit(DOMAIN_ATTR_SPLIT_TABLES,
				  smmu_domain->attributes);
			ret = 0;
		}
		break;
	}
	default:
		ret = -ENODEV;
	}

	return ret;
}

static int arm_smmu_domain_set_attr(struct iommu_domain *domain,
				    enum iommu_attr attr, void *data)
{
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	int ret;

	mutex_lock(&smmu_domain->init_mutex);
	ret = __arm_smmu_domain_set_attr(domain, attr, data);
	mutex_unlock(&smmu_domain->init_mutex);

	return ret;
}
static int arm_smmu_of_xlate(struct device *dev, struct of_phandle_args *args)
{
	u32 mask, fwid = 0;

	if (args->args_count > 0)
		fwid |= FIELD_PREP(SMR_ID, args->args[0]);

	if (args->args_count > 1)
		fwid |= FIELD_PREP(SMR_MASK, args->args[1]);
	else if (!of_property_read_u32(args->np, "stream-match-mask", &mask))
		fwid |= FIELD_PREP(SMR_MASK, mask);

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
	int idx = cfg->cbndx;
	u32 reg;
	int ret;

	ret = arm_smmu_power_on(smmu->pwr);
	if (ret)
		return ret;

	reg = arm_smmu_cb_read(smmu, idx, ARM_SMMU_CB_SCTLR);
	reg |= SCTLR_M;

	arm_smmu_cb_write(smmu, idx, ARM_SMMU_CB_SCTLR, reg);
	arm_smmu_power_off(smmu, smmu->pwr);
	return ret;
}

static bool arm_smmu_is_iova_coherent(struct iommu_domain *domain,
					 dma_addr_t iova)
{
	bool ret;
	unsigned long flags;
	struct arm_smmu_domain *smmu_domain = to_smmu_domain(domain);
	struct io_pgtable_ops *ops;
	struct msm_io_pgtable_info *pgtbl_info = &smmu_domain->pgtbl_info[0];

	if (!pgtbl_info->is_iova_coherent)
		return false;

	ops = arm_smmu_get_pgtable_ops(smmu_domain, iova);
	if (IS_ERR_OR_NULL(ops))
		return false;

	iova = arm_smmu_mask_iova(smmu_domain, iova);
	spin_lock_irqsave(&smmu_domain->cb_lock, flags);
	ret = pgtbl_info->is_iova_coherent(ops, iova);
	spin_unlock_irqrestore(&smmu_domain->cb_lock, flags);
	return ret;
}

static void arm_smmu_tlbi_domain(struct iommu_domain *domain)
{
	arm_smmu_tlb_inv_context_s1(to_smmu_domain(domain));
}

static struct msm_iommu_ops arm_smmu_ops = {
	.map_sg			= arm_smmu_map_sg,
	.iova_to_phys_hard	= arm_smmu_iova_to_phys_hard,
	.is_iova_coherent	= arm_smmu_is_iova_coherent,
	.tlbi_domain		= arm_smmu_tlbi_domain,
	.iova_to_pte		= arm_smmu_iova_to_pte,
	.iommu_ops = {

		.capable		= arm_smmu_capable,
		.domain_alloc		= arm_smmu_domain_alloc,
		.domain_free		= arm_smmu_domain_free,
		.attach_dev		= arm_smmu_attach_dev,
		.detach_dev		= arm_smmu_detach_dev,
		.map			= arm_smmu_map,
		.unmap			= arm_smmu_unmap,
		.flush_iotlb_all	= arm_smmu_flush_iotlb_all,
		.iotlb_sync		= arm_smmu_iotlb_sync,
		.iova_to_phys		= arm_smmu_iova_to_phys,
		.add_device		= arm_smmu_add_device,
		.remove_device		= arm_smmu_remove_device,
		.device_group		= arm_smmu_device_group,
		.domain_get_attr	= arm_smmu_domain_get_attr,
		.domain_set_attr	= arm_smmu_domain_set_attr,
		.of_xlate		= arm_smmu_of_xlate,
		.get_resv_regions	= arm_smmu_get_resv_regions,
		.put_resv_regions	= arm_smmu_put_resv_regions,
		/* Restricted during device attach */
		.pgsize_bitmap		= -1UL,
	}
};

static void arm_smmu_context_bank_reset(struct arm_smmu_device *smmu)
{
	int i;

	/* Make sure all context banks are disabled and clear CB_FSR  */
	for (i = 0; i < smmu->num_context_banks; ++i) {
		arm_smmu_write_context_bank(smmu, i, 0);
		arm_smmu_cb_write(smmu, i, ARM_SMMU_CB_FSR, FSR_FAULT);
	}
}

static void arm_smmu_device_reset(struct arm_smmu_device *smmu)
{
	int i;
	u32 reg;

	/* clear global FSR */
	reg = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_sGFSR);
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_sGFSR, reg);

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
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_TLBIALLH, QCOM_DUMMY_VAL);
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_TLBIALLNSNH, QCOM_DUMMY_VAL);

	reg = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_sCR0);

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
	reg &= ~(sCR0_BSU);

	if (smmu->features & ARM_SMMU_FEAT_VMID16)
		reg |= sCR0_VMID16EN;

	if (smmu->features & ARM_SMMU_FEAT_EXIDS)
		reg |= sCR0_EXIDENABLE;

	/* Force bypass transaction to be Non-Shareable & not io-coherent */
	reg &= ~sCR0_SHCFG;
	reg |= FIELD_PREP(sCR0_SHCFG, sCR0_SHCFG_NSH);

	if (smmu->impl && smmu->impl->reset)
		smmu->impl->reset(smmu);

	/* Push the button */
	arm_smmu_tlb_sync_global(smmu);
	wmb();
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_sCR0, reg);
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
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
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

	if (cb < 0) {
		mutex_unlock(&smmu->stream_map_mutex);
		return __arm_smmu_alloc_cb(smmu, smmu->num_s2_context_banks,
					   dev);
	}

	for (i = 0; i < smmu->num_mapping_groups; i++) {
		if (smmu->s2crs[i].cb_handoff && smmu->s2crs[i].cbndx == cb) {
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
		raw_smr = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_SMR(i));
		if (!(raw_smr & SMR_VALID))
			continue;

		smr.mask = FIELD_GET(SMR_MASK, raw_smr & ~SMR_VALID);
		smr.id = FIELD_GET(SMR_ID, raw_smr);
		smr.valid = true;

		raw_s2cr = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_S2CR(i));
		memset(&s2cr, 0, sizeof(s2cr));
		s2cr.group = NULL;
		s2cr.count = 1;
		s2cr.type = FIELD_GET(S2CR_TYPE, raw_s2cr);
		s2cr.privcfg = FIELD_GET(S2CR_PRIVCFG, raw_s2cr);
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
			dev_err(dev, "Couldn't get clock: %s\n",
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

static int arm_smmu_init_interconnect(struct arm_smmu_power_resources *pwr)
{
	struct device *dev = pwr->dev;

	/* We don't want the interconnect APIs to print an error message */
	if (!of_find_property(dev->of_node, "interconnects", NULL)) {
		dev_dbg(dev, "No interconnect info\n");
		return 0;
	}

	pwr->icc_path = of_icc_get(dev, NULL);
	if (IS_ERR_OR_NULL(pwr->icc_path)) {
		if (PTR_ERR(pwr->icc_path) != -EPROBE_DEFER)
			dev_err(dev, "Unable to read interconnect path from devicetree rc: %ld\n",
				PTR_ERR(pwr->icc_path));
		return pwr->icc_path ? PTR_ERR(pwr->icc_path) : -EINVAL;
	}

	if (of_property_read_bool(dev->of_node, "qcom,active-only"))
		icc_set_tag(pwr->icc_path, ARM_SMMU_ICC_ACTIVE_ONLY_TAG);

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

	ret = arm_smmu_init_interconnect(pwr);
	if (ret)
		return ERR_PTR(ret);

	return pwr;
}

static void arm_smmu_exit_power_resources(struct arm_smmu_power_resources *pwr)
{
	icc_put(pwr->icc_path);
}

static int arm_smmu_device_cfg_probe(struct arm_smmu_device *smmu)
{
	unsigned int size;
	u32 id;
	bool cttw_reg, cttw_fw = smmu->features & ARM_SMMU_FEAT_COHERENT_WALK;
	int i;

	dev_dbg(smmu->dev, "probing hardware configuration...\n");
	dev_dbg(smmu->dev, "SMMUv%d with:\n",
			smmu->version == ARM_SMMU_V2 ? 2 : 1);

	/* ID0 */
	id = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_ID0);

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
		size = 1 << FIELD_GET(ID0_NUMSIDB, id);
	}
	smmu->streamid_mask = size - 1;
	if (id & ID0_SMS) {
		smmu->features |= ARM_SMMU_FEAT_STREAM_MATCH;
		size = FIELD_GET(ID0_NUMSMRG, id);
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
			   "\tstream matching with %u register groups", size);
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
	id = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_ID1);
	smmu->pgshift = (id & ID1_PAGESIZE) ? 16 : 12;

	/* Check for size mismatch of SMMU address space from mapped region */
	size = 1 << (FIELD_GET(ID1_NUMPAGENDXB, id) + 1);
	if (smmu->numpage != 2 * size << smmu->pgshift)
		dev_warn(smmu->dev,
			"SMMU address space size (0x%x) differs from mapped region size (0x%x)!\n",
			2 * size << smmu->pgshift, smmu->numpage);
	/* Now properly encode NUMPAGE to subsequently derive SMMU_CB_BASE */
	smmu->numpage = size;

	smmu->num_s2_context_banks = FIELD_GET(ID1_NUMS2CB, id);
	smmu->num_context_banks = FIELD_GET(ID1_NUMCB, id);
	if (smmu->num_s2_context_banks > smmu->num_context_banks) {
		dev_err(smmu->dev, "impossible number of S2 context banks!\n");
		return -ENODEV;
	}
	dev_dbg(smmu->dev, "\t%u context banks (%u stage-2 only)\n",
		   smmu->num_context_banks, smmu->num_s2_context_banks);
	smmu->cbs = devm_kcalloc(smmu->dev, smmu->num_context_banks,
				 sizeof(*smmu->cbs), GFP_KERNEL);
	if (!smmu->cbs)
		return -ENOMEM;

	/* ID2 */
	id = arm_smmu_gr0_read(smmu, ARM_SMMU_GR0_ID2);
	size = arm_smmu_id_size_to_bits(FIELD_GET(ID2_IAS, id));
	smmu->ipa_size = size;

	/* The output mask is also applied for bypass */
	size = arm_smmu_id_size_to_bits(FIELD_GET(ID2_OAS, id));
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
		size = FIELD_GET(ID2_UBS, id);
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

	if (arm_smmu_ops.iommu_ops.pgsize_bitmap == -1UL)
		arm_smmu_ops.iommu_ops.pgsize_bitmap = smmu->pgsize_bitmap;
	else
		arm_smmu_ops.iommu_ops.pgsize_bitmap |= smmu->pgsize_bitmap;
	dev_dbg(smmu->dev, "\tSupported page sizes: 0x%08lx\n",
		   smmu->pgsize_bitmap);


	if (smmu->features & ARM_SMMU_FEAT_TRANS_S1)
		dev_dbg(smmu->dev, "\tStage-1: %lu-bit VA -> %lu-bit IPA\n",
			smmu->va_size, smmu->ipa_size);

	if (smmu->features & ARM_SMMU_FEAT_TRANS_S2)
		dev_dbg(smmu->dev, "\tStage-2: %lu-bit IPA -> %lu-bit PA\n",
			smmu->ipa_size, smmu->pa_size);

	if (smmu->impl && smmu->impl->cfg_probe)
		return smmu->impl->cfg_probe(smmu);

	return 0;
}

struct arm_smmu_match_data {
	enum arm_smmu_arch_version version;
	enum arm_smmu_implementation model;
};

#define ARM_SMMU_MATCH_DATA(name, ver, imp)	\
static const struct arm_smmu_match_data name = { .version = ver, .model = imp }

ARM_SMMU_MATCH_DATA(smmu_generic_v1, ARM_SMMU_V1, GENERIC_SMMU);
ARM_SMMU_MATCH_DATA(smmu_generic_v2, ARM_SMMU_V2, GENERIC_SMMU);
ARM_SMMU_MATCH_DATA(arm_mmu401, ARM_SMMU_V1_64K, GENERIC_SMMU);
ARM_SMMU_MATCH_DATA(arm_mmu500, ARM_SMMU_V2, ARM_MMU500);
ARM_SMMU_MATCH_DATA(cavium_smmuv2, ARM_SMMU_V2, CAVIUM_SMMUV2);
ARM_SMMU_MATCH_DATA(qcom_smmuv500, ARM_SMMU_V2, QCOM_SMMUV500);
ARM_SMMU_MATCH_DATA(qcom_smmuv2, ARM_SMMU_V2, QCOM_SMMUV2);

static const struct of_device_id arm_smmu_of_match[] = {
	{ .compatible = "arm,smmu-v1", .data = &smmu_generic_v1 },
	{ .compatible = "arm,smmu-v2", .data = &smmu_generic_v2 },
	{ .compatible = "arm,mmu-400", .data = &smmu_generic_v1 },
	{ .compatible = "arm,mmu-401", .data = &arm_mmu401 },
	{ .compatible = "arm,mmu-500", .data = &arm_mmu500 },
	{ .compatible = "cavium,smmu-v2", .data = &cavium_smmuv2 },
	{ .compatible = "qcom,qsmmu-v500", .data = &qcom_smmuv500 },
	{ .compatible = "qcom,smmu-v2", .data = &qcom_smmuv2 },
	{ },
};
MODULE_DEVICE_TABLE(of, arm_smmu_of_match);

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

static int arm_smmu_bus_init(struct iommu_ops *ops)
{
	int err;

	/* Oh, for a proper bus abstraction */
	if (!iommu_present(&platform_bus_type)) {
		err = bus_set_iommu(&platform_bus_type, ops);
		if (err)
			return err;
	}
#ifdef CONFIG_ARM_AMBA
	if (!iommu_present(&amba_bustype)) {
		err = bus_set_iommu(&amba_bustype, ops);
		if (err)
			goto err_reset_platform_ops;
	}
#endif
#ifdef CONFIG_PCI
	if (!iommu_present(&pci_bus_type)) {
		err = bus_set_iommu(&pci_bus_type, ops);
		if (err)
			goto err_reset_amba_ops;
	}
#endif
#ifdef CONFIG_FSL_MC_BUS
	if (!iommu_present(&fsl_mc_bus_type)) {
		err = bus_set_iommu(&fsl_mc_bus_type, ops);
		if (err)
			goto err_reset_pci_ops;
	}
#endif
	return 0;

err_reset_pci_ops: __maybe_unused;
#ifdef CONFIG_PCI
	bus_set_iommu(&pci_bus_type, NULL);
#endif
err_reset_amba_ops: __maybe_unused;
#ifdef CONFIG_ARM_AMBA
	bus_set_iommu(&amba_bustype, NULL);
#endif
err_reset_platform_ops: __maybe_unused;
	bus_set_iommu(&platform_bus_type, NULL);
	return err;
}

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
		if (!using_legacy_binding) {
			pr_notice("deprecated \"mmu-masters\" DT property in use; %s support unavailable\n",
				  IS_ENABLED(CONFIG_ARM_SMMU_LEGACY_DT_BINDINGS) ? "DMA API" : "SMMU");
		}
		using_legacy_binding = true;
	} else if (!legacy_binding && !using_legacy_binding) {
		using_generic_binding = true;
	} else {
		dev_err(dev, "not probing due to mismatched DT properties\n");
		return -ENODEV;
	}

	smmu = devm_kzalloc(dev, sizeof(*smmu), GFP_KERNEL);
	if (!smmu)
		return -ENOMEM;

	smmu->dev = dev;

	data = of_device_get_match_data(dev);
	smmu->version = data->version;
	smmu->model = data->model;

	if (of_dma_is_coherent(dev->of_node))
		smmu->features |= ARM_SMMU_FEAT_COHERENT_WALK;

	smmu = arm_smmu_impl_init(smmu);
	if (IS_ERR(smmu))
		return PTR_ERR(smmu);

	idr_init(&smmu->asid_idr);
	mutex_init(&smmu->idr_mutex);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "no MEM resource info\n");
		return -EINVAL;
	}

	smmu->phys_addr = res->start;
	smmu->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(smmu->base))
		return PTR_ERR(smmu->base);
	/*
	 * The resource size should effectively match the value of SMMU_TOP;
	 * stash that temporarily until we know PAGESIZE to validate it with.
	 */
	smmu->numpage = resource_size(res);

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

	smmu->irqs = devm_kcalloc(dev, num_irqs, sizeof(*smmu->irqs),
				  GFP_KERNEL);
	if (!smmu->irqs)
		return -ENOMEM;

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

	err = arm_smmu_device_cfg_probe(smmu);
	if (err)
		goto out_power_off;

	err = arm_smmu_handoff_cbs(smmu);
	if (err)
		goto out_power_off;

	if (smmu->version == ARM_SMMU_V2) {
		if (smmu->num_context_banks > smmu->num_context_irqs) {
			dev_err(dev,
				"found %d context interrupt(s) but have %d context banks. assuming %d context interrupts.\n",
				smmu->num_context_irqs, smmu->num_context_banks,
				smmu->num_context_banks);
			return -ENODEV;
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

	iommu_device_set_ops(&smmu->iommu, &arm_smmu_ops.iommu_ops);
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
	arm_smmu_power_off(smmu, smmu->pwr);

	/*
	 * On GKI, we use the upstream implementation of the IOMMU page table
	 * management code, which lacks all of the optimizations that we have
	 * downstream to speed up calls into the SMMU driver to unmap memory.
	 *
	 * When the GPU goes into slumber, it relinquishes its votes for
	 * the regulators and clocks that the SMMU driver votes for. This
	 * means that when the SMMU driver adds/removes votes for the
	 * power resources required to access the GPU SMMU registers for
	 * TLB invalidations while unmapping memory, the SMMU driver has to
	 * wait for the resources to actually turn on/off, which incurs a
	 * considerable amount of delay.
	 *
	 * This delay, coupled with the use of the unoptimized IOMMU page table
	 * management code in GKI results in slow unmap calls. To alleviate
	 * that, we can remove the latency incurred by enabling/disabling the
	 * power resources, by always keeping them on.
	 *
	 */
	if (IS_ENABLED(CONFIG_ARM_SMMU_POWER_ALWAYS_ON))
		arm_smmu_power_on(smmu->pwr);

	/*
	 * We want to avoid touching dev->power.lock in fastpaths unless
	 * it's really going to do something useful - pm_runtime_enabled()
	 * can serve as an ideal proxy for that decision. So, conditionally
	 * enable pm_runtime.
	 */
	if (dev->pm_domain) {
		pm_runtime_set_active(dev);
		pm_runtime_enable(dev);
	}

	/*
	 * For ACPI and generic DT bindings, an SMMU will be probed before
	 * any device which might need it, so we want the bus ops in place
	 * ready to handle default domain setup as soon as any SMMU exists.
	 */
	if (!using_legacy_binding)
		return arm_smmu_bus_init(&arm_smmu_ops.iommu_ops);

	return 0;

out_power_off:
	arm_smmu_power_off(smmu, smmu->pwr);

out_exit_power_resources:
	arm_smmu_exit_power_resources(smmu->pwr);

	return err;
}

static int arm_smmu_device_remove(struct platform_device *pdev)
{
	struct arm_smmu_device *smmu = platform_get_drvdata(pdev);

	if (!smmu)
		return -ENODEV;

	if (arm_smmu_power_on(smmu->pwr))
		return -EINVAL;

	if (!bitmap_empty(smmu->context_map, ARM_SMMU_MAX_CBS))
		dev_err(&pdev->dev, "removing device with active domains!\n");

	arm_smmu_bus_init(NULL);
	iommu_device_unregister(&smmu->iommu);

	if (smmu->impl && smmu->impl->device_remove)
		smmu->impl->device_remove(smmu);

	idr_destroy(&smmu->asid_idr);

	/* Turn the thing off */
	arm_smmu_gr0_write(smmu, ARM_SMMU_GR0_sCR0, sCR0_CLIENTPD);
	arm_smmu_power_off(smmu, smmu->pwr);

	/* Remove the extra reference that was taken in the probe function */
	if (IS_ENABLED(CONFIG_ARM_SMMU_POWER_ALWAYS_ON))
		arm_smmu_power_off(smmu, smmu->pwr);

	arm_smmu_exit_power_resources(smmu->pwr);

	return 0;
}

static int __maybe_unused arm_smmu_runtime_resume(struct device *dev)
{
	struct arm_smmu_device *smmu = dev_get_drvdata(dev);
	int ret;

	ret = clk_bulk_enable(smmu->num_clks, smmu->clks);
	if (ret)
		return ret;

	ret = arm_smmu_power_on(smmu->pwr);
	if (ret)
		return ret;

	arm_smmu_device_reset(smmu);
	arm_smmu_power_off(smmu, smmu->pwr);

	return 0;
}

static int __maybe_unused arm_smmu_runtime_suspend(struct device *dev)
{
	struct arm_smmu_device *smmu = dev_get_drvdata(dev);

	clk_bulk_disable(smmu->num_clks, smmu->clks);

	return 0;
}

static int __maybe_unused arm_smmu_pm_resume(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;

	return arm_smmu_runtime_resume(dev);
}

static int __maybe_unused arm_smmu_pm_suspend(struct device *dev)
{
	if (pm_runtime_suspended(dev))
		return 0;

	return arm_smmu_runtime_suspend(dev);
}

static const struct dev_pm_ops arm_smmu_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(arm_smmu_pm_suspend, arm_smmu_pm_resume)
	SET_RUNTIME_PM_OPS(arm_smmu_runtime_suspend,
			   arm_smmu_runtime_resume, NULL)
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

static int qsmmuv500_tbu_remove(struct platform_device *pdev)
{
	struct qsmmuv500_tbu_device *tbu = dev_get_drvdata(&pdev->dev);

	arm_smmu_exit_power_resources(tbu->pwr);
	return 0;
}

static struct platform_driver qsmmuv500_tbu_driver = {
	.driver	= {
		.name		= "qsmmuv500-tbu",
		.of_match_table	= of_match_ptr(qsmmuv500_tbu_of_match),
	},
	.probe	= qsmmuv500_tbu_probe,
	.remove = qsmmuv500_tbu_remove,
};

static struct platform_driver arm_smmu_driver = {
	.driver	= {
		.name			= "arm-smmu",
		.of_match_table		= of_match_ptr(arm_smmu_of_match),
		.pm			= &arm_smmu_pm_ops,
		.suppress_bind_attrs    = true,
	},
	.probe	= arm_smmu_device_dt_probe,
	.remove	= arm_smmu_device_remove,
};

static int __init arm_smmu_init(void)
{
	int ret;
	ktime_t cur;

	cur = ktime_get();
	ret = platform_driver_register(&qsmmuv500_tbu_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&arm_smmu_driver);
	if (ret) {
		platform_driver_unregister(&qsmmuv500_tbu_driver);
		return ret;
	}

	trace_smmu_init(ktime_us_delta(ktime_get(), cur));
	return ret;
}
subsys_initcall(arm_smmu_init);

static void __exit arm_smmu_exit(void)
{
	platform_driver_unregister(&arm_smmu_driver);
	platform_driver_unregister(&qsmmuv500_tbu_driver);
}
module_exit(arm_smmu_exit);

MODULE_DESCRIPTION("IOMMU API for ARM architected SMMU implementations");
MODULE_AUTHOR("Will Deacon <will@kernel.org>");
MODULE_LICENSE("GPL v2");
