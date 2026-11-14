// SPDX-License-Identifier: GPL-2.0
/*
 * pKVM host driver for the Arm SMMUv3
 *
 * Copyright (C) 2022 Linaro Ltd.
 */
#include <asm/kvm_pkvm.h>
#include <asm/kvm_mmu.h>

#include <linux/moduleparam.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include "arm-smmu-v3.h"
#include "pkvm/arm_smmu_v3.h"

extern struct kvm_iommu_ops kvm_nvhe_sym(smmu_ops);

struct host_arm_smmu_device {
	struct arm_smmu_device		smmu;
	pkvm_handle_t			id;
	u32				boot_gbpa;
	struct kvm_power_domain		power_domain;
};

#define smmu_to_host(_smmu) \
	container_of(_smmu, struct host_arm_smmu_device, smmu);

struct kvm_arm_smmu_master {
	struct arm_smmu_device		*smmu;
	struct device			*dev;
	struct xarray			domains;
	u32				ssid_bits;
	bool				idmapped; /* Stage-2 is transparently identity mapped*/
};

struct kvm_arm_smmu_domain {
	struct iommu_domain		domain;
	struct arm_smmu_device		*smmu;
	struct mutex			init_mutex;
	pkvm_handle_t			id;
};

#define to_kvm_smmu_domain(_domain) \
	container_of(_domain, struct kvm_arm_smmu_domain, domain)

#ifdef MODULE
static unsigned long                   pkvm_module_token;

#define ksym_ref_addr_nvhe(x) \
	((typeof(kvm_nvhe_sym(x)) *)(pkvm_el2_mod_va(&kvm_nvhe_sym(x), pkvm_module_token)))

int kvm_nvhe_sym(smmu_init_hyp_module)(const struct pkvm_module_ops *ops);
#else
#define ksym_ref_addr_nvhe(x) \
	((typeof(kvm_nvhe_sym(x)) *)(kern_hyp_va(lm_alias(&kvm_nvhe_sym(x)))))
#endif

static size_t				kvm_arm_smmu_cur;
static size_t				kvm_arm_smmu_count;
static struct hyp_arm_smmu_v3_device	*kvm_arm_smmu_array;
static DEFINE_IDA(kvm_arm_smmu_domain_ida);
/*
 * Pre allocated pages that can be used from the EL2 part of the driver from atomic
 * context, ideally used for page table pages for identity domains.
 */
static int atomic_pages;
module_param(atomic_pages, int, 0);

static struct platform_driver kvm_arm_smmu_driver;

static struct arm_smmu_device *
kvm_arm_smmu_get_by_fwnode(struct fwnode_handle *fwnode)
{
	struct device *dev;

	dev = driver_find_device_by_fwnode(&kvm_arm_smmu_driver.driver, fwnode);
	put_device(dev);
	return dev ? dev_get_drvdata(dev) : NULL;
}

static struct iommu_ops kvm_arm_smmu_ops;

static struct iommu_device *kvm_arm_smmu_probe_device(struct device *dev)
{
	struct arm_smmu_device *smmu;
	struct kvm_arm_smmu_master *master;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);

	if (WARN_ON_ONCE(dev_iommu_priv_get(dev)))
		return ERR_PTR(-EBUSY);

	smmu = kvm_arm_smmu_get_by_fwnode(fwspec->iommu_fwnode);
	if (!smmu)
		return ERR_PTR(-ENODEV);

	master = kzalloc(sizeof(*master), GFP_KERNEL);
	if (!master)
		return ERR_PTR(-ENOMEM);

	master->dev = dev;
	master->smmu = smmu;

	device_property_read_u32(dev, "pasid-num-bits", &master->ssid_bits);
	master->ssid_bits = min(smmu->ssid_bits, master->ssid_bits);
	xa_init(&master->domains);
	master->idmapped = device_property_read_bool(dev, "iommu-idmapped");
	dev_iommu_priv_set(dev, master);
	if (!device_link_add(dev, smmu->dev,
			     DL_FLAG_PM_RUNTIME |
			     DL_FLAG_AUTOREMOVE_SUPPLIER)) {
		kfree(master);
		return ERR_PTR(-ENOLINK);
	}

	return &smmu->iommu;
}

static struct iommu_domain *kvm_arm_smmu_domain_alloc(unsigned type)
{
	struct kvm_arm_smmu_domain *kvm_smmu_domain;

	/*
	 * We don't support
	 * - IOMMU_DOMAIN_DMA_FQ because lazy unmap would clash with memory
	 *   donation to guests.
	 */
	if (type != IOMMU_DOMAIN_DMA &&
	    type != IOMMU_DOMAIN_UNMANAGED &&
	    type != IOMMU_DOMAIN_IDENTITY &&
		type != IOMMU_DOMAIN_BLOCKED)
		return ERR_PTR(-EOPNOTSUPP);

	kvm_smmu_domain = kzalloc(sizeof(*kvm_smmu_domain), GFP_KERNEL);
	if (!kvm_smmu_domain)
		return ERR_PTR(-ENOMEM);

	mutex_init(&kvm_smmu_domain->init_mutex);

	return &kvm_smmu_domain->domain;
}

static int kvm_arm_smmu_domain_finalize(struct kvm_arm_smmu_domain *kvm_smmu_domain,
					struct kvm_arm_smmu_master *master)
{
	int ret = 0;
	struct arm_smmu_device *smmu = master->smmu;
	unsigned int max_domains;
	enum kvm_arm_smmu_domain_type type;
	struct io_pgtable_cfg cfg;
	unsigned long ias;

	if (kvm_smmu_domain->smmu && (kvm_smmu_domain->smmu != smmu))
		return -EINVAL;

	if (kvm_smmu_domain->smmu)
		return 0;

	if (kvm_smmu_domain->domain.type == IOMMU_DOMAIN_IDENTITY) {
		kvm_smmu_domain->id = KVM_IOMMU_DOMAIN_IDMAP_ID;
		/*
		 * Identity domains doesn't use the DMA API, so no need to
		 * set the  domain aperture.
		 */
		goto out;
	}

	/* Default to stage-1. */
	if (smmu->features & ARM_SMMU_FEAT_TRANS_S1) {
		ias = (smmu->features & ARM_SMMU_FEAT_VAX) ? 52 : 48;
		cfg = (struct io_pgtable_cfg) {
			.fmt = ARM_64_LPAE_S1,
			.pgsize_bitmap = smmu->pgsize_bitmap,
			.ias = min_t(unsigned long, ias, VA_BITS),
			.oas = smmu->ias,
			.coherent_walk = smmu->features & ARM_SMMU_FEAT_COHERENCY,
		};
		ret = io_pgtable_configure(&cfg);
		if (ret)
			return ret;

		type = KVM_ARM_SMMU_DOMAIN_S1;
		kvm_smmu_domain->domain.pgsize_bitmap = cfg.pgsize_bitmap;
		kvm_smmu_domain->domain.geometry.aperture_end = (1UL << cfg.ias) - 1;
		max_domains = 1 << smmu->asid_bits;
	} else {
		cfg = (struct io_pgtable_cfg) {
			.fmt = ARM_64_LPAE_S2,
			.pgsize_bitmap = smmu->pgsize_bitmap,
			.ias = smmu->ias,
			.oas = smmu->oas,
			.coherent_walk = smmu->features & ARM_SMMU_FEAT_COHERENCY,
		};
		ret = io_pgtable_configure(&cfg);
		if (ret)
			return ret;

		type = KVM_ARM_SMMU_DOMAIN_S2;
		kvm_smmu_domain->domain.pgsize_bitmap = cfg.pgsize_bitmap;
		kvm_smmu_domain->domain.geometry.aperture_end = (1UL << cfg.ias) - 1;
		max_domains = 1 << smmu->vmid_bits;
	}
	kvm_smmu_domain->domain.geometry.force_aperture = true;

	/*
	 * The hypervisor uses the domain_id for asid/vmid so it has to be
	 * unique, and it has to be in range of this smmu, which can be
	 * either 8 or 16 bits.
	 */
	ret = ida_alloc_range(&kvm_arm_smmu_domain_ida, KVM_IOMMU_DOMAIN_NR_START,
			      min(KVM_IOMMU_MAX_DOMAINS, max_domains), GFP_KERNEL);
	if (ret < 0)
		return ret;

	kvm_smmu_domain->id = ret;

	ret = kvm_iommu_alloc_domain(kvm_smmu_domain->id, type);
	if (ret) {
		ida_free(&kvm_arm_smmu_domain_ida, kvm_smmu_domain->id);
		return ret;
	}

out:
	kvm_smmu_domain->smmu = smmu;
	return ret;
}

static void kvm_arm_smmu_domain_free(struct iommu_domain *domain)
{
	int ret;
	struct kvm_arm_smmu_domain *kvm_smmu_domain = to_kvm_smmu_domain(domain);
	struct arm_smmu_device *smmu = kvm_smmu_domain->smmu;

	if (smmu && (kvm_smmu_domain->domain.type != IOMMU_DOMAIN_IDENTITY)) {
		ret = kvm_iommu_free_domain(kvm_smmu_domain->id);
		ida_free(&kvm_arm_smmu_domain_ida, kvm_smmu_domain->id);
	}
	kfree(kvm_smmu_domain);
}

static int kvm_arm_smmu_detach_dev_pasid(struct host_arm_smmu_device *host_smmu,
					 struct kvm_arm_smmu_master *master,
					 ioasid_t pasid)
{
	int i, ret;
	struct arm_smmu_device *smmu = &host_smmu->smmu;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(master->dev);
	struct kvm_arm_smmu_domain *domain = xa_load(&master->domains, pasid);

	if (!domain)
		return 0;

	for (i = 0; i < fwspec->num_ids; i++) {
		int sid = fwspec->ids[i];

		ret = kvm_iommu_detach_dev(host_smmu->id, domain->id, sid, pasid);
		if (ret) {
			dev_err(smmu->dev, "cannot detach device %s (0x%x): %d\n",
				dev_name(master->dev), sid, ret);
			break;
		}
	}

	xa_erase(&master->domains, pasid);

	return ret;
}

static int kvm_arm_smmu_detach_dev(struct host_arm_smmu_device *host_smmu,
				   struct kvm_arm_smmu_master *master)
{
	return kvm_arm_smmu_detach_dev_pasid(host_smmu, master, 0);
}

static void kvm_arm_smmu_remove_dev_pasid(struct device *dev, ioasid_t pasid,
					  struct iommu_domain *domain)
{
	struct kvm_arm_smmu_master *master = dev_iommu_priv_get(dev);
	struct host_arm_smmu_device *host_smmu = smmu_to_host(master->smmu);

	kvm_arm_smmu_detach_dev_pasid(host_smmu, master, pasid);
}

static void kvm_arm_smmu_release_device(struct device *dev)
{
	struct kvm_arm_smmu_master *master = dev_iommu_priv_get(dev);
	struct host_arm_smmu_device *host_smmu = smmu_to_host(master->smmu);

	kvm_arm_smmu_detach_dev(host_smmu, master);
	xa_destroy(&master->domains);
	kfree(master);
	iommu_fwspec_free(dev);
}

static int kvm_arm_smmu_set_dev_pasid(struct iommu_domain *domain,
				      struct device *dev, ioasid_t pasid)
{
	int i, ret;
	struct arm_smmu_device *smmu;
	struct host_arm_smmu_device *host_smmu;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct kvm_arm_smmu_master *master = dev_iommu_priv_get(dev);
	struct kvm_arm_smmu_domain *kvm_smmu_domain = to_kvm_smmu_domain(domain);

	if (!master)
		return -ENODEV;

	smmu = master->smmu;
	host_smmu = smmu_to_host(smmu);

	ret = kvm_arm_smmu_detach_dev_pasid(host_smmu, master, pasid);
	if (ret || (domain->type == IOMMU_DOMAIN_BLOCKED))
		return ret;

	mutex_lock(&kvm_smmu_domain->init_mutex);
	ret = kvm_arm_smmu_domain_finalize(kvm_smmu_domain, master);
	mutex_unlock(&kvm_smmu_domain->init_mutex);
	if (ret)
		return ret;

	for (i = 0; i < fwspec->num_ids; i++) {
		int sid = fwspec->ids[i];

		ret = kvm_iommu_attach_dev(host_smmu->id, kvm_smmu_domain->id,
					   sid, pasid, master->ssid_bits, 0);
		if (ret) {
			dev_err(smmu->dev, "cannot attach device %s (0x%x): %d\n",
				dev_name(dev), sid, ret);
			goto out_ret;
		}
	}
	ret = xa_insert(&master->domains, pasid, kvm_smmu_domain, GFP_KERNEL);

out_ret:
	if (ret)
		kvm_arm_smmu_detach_dev(host_smmu, master);
	return ret;
}

static int kvm_arm_smmu_attach_dev(struct iommu_domain *domain,
				   struct device *dev)
{
	struct kvm_arm_smmu_master *master = dev_iommu_priv_get(dev);
	unsigned long pasid = 0;

	/* All pasids must be removed first. */
	if (xa_find_after(&master->domains, &pasid, ULONG_MAX, XA_PRESENT))
		return -EBUSY;

	return kvm_arm_smmu_set_dev_pasid(domain, dev, 0);
}

static int kvm_arm_smmu_def_domain_type(struct device *dev)
{
	struct kvm_arm_smmu_master *master = dev_iommu_priv_get(dev);

	if (master->idmapped && atomic_pages)
		return IOMMU_DOMAIN_IDENTITY;
	return 0;
}

static bool kvm_arm_smmu_capable(struct device *dev, enum iommu_cap cap)
{
	struct kvm_arm_smmu_master *master = dev_iommu_priv_get(dev);

	switch (cap) {
	case IOMMU_CAP_CACHE_COHERENCY:
		return master->smmu->features & ARM_SMMU_FEAT_COHERENCY;
	case IOMMU_CAP_NOEXEC:
	default:
		return false;
	}
}

static int kvm_arm_smmu_map_pages(struct iommu_domain *domain,
				  unsigned long iova, phys_addr_t paddr,
				  size_t pgsize, size_t pgcount, int prot,
				  gfp_t gfp, size_t *total_mapped)
{
	struct kvm_arm_smmu_domain *kvm_smmu_domain = to_kvm_smmu_domain(domain);

	return kvm_iommu_map_pages(kvm_smmu_domain->id, iova, paddr, pgsize,
				   pgcount, prot, gfp, total_mapped);
}

static size_t kvm_arm_smmu_unmap_pages(struct iommu_domain *domain,
				       unsigned long iova, size_t pgsize,
				       size_t pgcount,
				       struct iommu_iotlb_gather *iotlb_gather)
{
	struct kvm_arm_smmu_domain *kvm_smmu_domain = to_kvm_smmu_domain(domain);

	return kvm_iommu_unmap_pages(kvm_smmu_domain->id, iova, pgsize, pgcount);
}

static phys_addr_t kvm_arm_smmu_iova_to_phys(struct iommu_domain *domain,
					     dma_addr_t iova)
{
	struct kvm_arm_smmu_domain *kvm_smmu_domain = to_kvm_smmu_domain(domain);

	return kvm_iommu_iova_to_phys(kvm_smmu_domain->id, iova);
}

struct kvm_arm_smmu_map_sg {
	struct iommu_map_cookie_sg cookie;
	struct kvm_iommu_sg *sg;
	unsigned int ptr;
	unsigned long iova;
	int prot;
	gfp_t gfp;
	unsigned int nents;
};

static struct iommu_map_cookie_sg *kvm_arm_smmu_alloc_cookie_sg(unsigned long iova,
								int prot,
								unsigned int nents,
								gfp_t gfp)
{
	int ret;
	struct kvm_arm_smmu_map_sg *map_sg = kzalloc(sizeof(*map_sg), gfp);

	if (!map_sg)
		return NULL;

	map_sg->sg = kvm_iommu_sg_alloc(nents, gfp);
	if (!map_sg->sg)
		return NULL;
	map_sg->iova = iova;
	map_sg->prot = prot;
	map_sg->gfp = gfp;
	map_sg->nents = nents;
	ret = kvm_iommu_share_hyp_sg(map_sg->sg, nents);
	if (ret) {
		kvm_iommu_sg_free(map_sg->sg, nents);
		kfree(map_sg);
		return NULL;
	}

	return &map_sg->cookie;
}

static int kvm_arm_smmu_add_deferred_map_sg(struct iommu_map_cookie_sg *cookie,
					    phys_addr_t paddr, size_t pgsize, size_t pgcount)
{
	struct kvm_arm_smmu_map_sg *map_sg = container_of(cookie, struct kvm_arm_smmu_map_sg,
							  cookie);
	struct kvm_iommu_sg *sg = map_sg->sg;

	sg[map_sg->ptr].phys = paddr;
	sg[map_sg->ptr].pgsize = pgsize;
	sg[map_sg->ptr].pgcount = pgcount;
	map_sg->ptr++;
	return 0;
}

static size_t kvm_arm_smmu_consume_deferred_map_sg(struct iommu_map_cookie_sg *cookie)
{
	struct kvm_arm_smmu_map_sg *map_sg = container_of(cookie, struct kvm_arm_smmu_map_sg,
							  cookie);
	struct kvm_iommu_sg *sg = map_sg->sg;
	struct kvm_arm_smmu_domain *kvm_smmu_domain = to_kvm_smmu_domain(map_sg->cookie.domain);
	size_t total_mapped;

	total_mapped = kvm_iommu_map_sg(kvm_smmu_domain->id, sg, map_sg->iova, map_sg->ptr,
					map_sg->prot, map_sg->gfp);

	kvm_iommu_unshare_hyp_sg(sg, map_sg->nents);
	kvm_iommu_sg_free(sg, map_sg->nents);
	kfree(map_sg);
	return total_mapped;
}

static struct iommu_ops kvm_arm_smmu_ops = {
	.capable		= kvm_arm_smmu_capable,
	.device_group		= arm_smmu_device_group,
	.of_xlate		= arm_smmu_of_xlate,
	.get_resv_regions	= arm_smmu_get_resv_regions,
	.probe_device		= kvm_arm_smmu_probe_device,
	.release_device		= kvm_arm_smmu_release_device,
	.domain_alloc		= kvm_arm_smmu_domain_alloc,
	.pgsize_bitmap		= -1UL,
	.remove_dev_pasid	= kvm_arm_smmu_remove_dev_pasid,
	.owner			= THIS_MODULE,
	.def_domain_type	= kvm_arm_smmu_def_domain_type,
	.default_domain_ops = &(const struct iommu_domain_ops) {
		.attach_dev	= kvm_arm_smmu_attach_dev,
		.free		= kvm_arm_smmu_domain_free,
		.map_pages	= kvm_arm_smmu_map_pages,
		.unmap_pages	= kvm_arm_smmu_unmap_pages,
		.iova_to_phys	= kvm_arm_smmu_iova_to_phys,
		.set_dev_pasid	= kvm_arm_smmu_set_dev_pasid,
		.alloc_cookie_sg = kvm_arm_smmu_alloc_cookie_sg,
		.add_deferred_map_sg = kvm_arm_smmu_add_deferred_map_sg,
		.consume_deferred_map_sg = kvm_arm_smmu_consume_deferred_map_sg,
	}
};

static bool kvm_arm_smmu_validate_features(struct arm_smmu_device *smmu)
{
	unsigned int required_features =
		ARM_SMMU_FEAT_TT_LE;
	unsigned int forbidden_features =
		ARM_SMMU_FEAT_STALL_FORCE;
	unsigned int keep_features =
		ARM_SMMU_FEAT_2_LVL_STRTAB	|
		ARM_SMMU_FEAT_2_LVL_CDTAB	|
		ARM_SMMU_FEAT_TT_LE		|
		ARM_SMMU_FEAT_SEV		|
		ARM_SMMU_FEAT_COHERENCY		|
		ARM_SMMU_FEAT_TRANS_S1		|
		ARM_SMMU_FEAT_TRANS_S2		|
		ARM_SMMU_FEAT_VAX		|
		ARM_SMMU_FEAT_RANGE_INV;

	if (smmu->options & ARM_SMMU_OPT_PAGE0_REGS_ONLY) {
		dev_err(smmu->dev, "unsupported layout\n");
		return false;
	}

	if ((smmu->features & required_features) != required_features) {
		dev_err(smmu->dev, "missing features 0x%x\n",
			required_features & ~smmu->features);
		return false;
	}

	if (smmu->features & forbidden_features) {
		dev_err(smmu->dev, "features 0x%x forbidden\n",
			smmu->features & forbidden_features);
		return false;
	}

	smmu->features &= keep_features;

	return true;
}

static irqreturn_t kvm_arm_smmu_evt_handler(int irq, void *dev)
{
	int i;
	struct arm_smmu_device *smmu = dev;
	struct arm_smmu_queue *q = &smmu->evtq.q;
	struct arm_smmu_ll_queue *llq = &q->llq;
	static DEFINE_RATELIMIT_STATE(rs, DEFAULT_RATELIMIT_INTERVAL,
				      DEFAULT_RATELIMIT_BURST);
	u64 evt[EVTQ_ENT_DWORDS];

	if (pm_runtime_get_if_active(smmu->dev) == 0) {
		dev_err(smmu->dev, "Unable to handle event interrupt because device not runtime active\n");
		return IRQ_NONE;
	}

	do {
		while (!queue_remove_raw(q, evt)) {
			u8 id = FIELD_GET(EVTQ_0_ID, evt[0]);

			if (!__ratelimit(&rs))
				continue;

			dev_info(smmu->dev, "event 0x%02x received:\n", id);
			for (i = 0; i < ARRAY_SIZE(evt); ++i)
				dev_info(smmu->dev, "\t0x%016llx\n",
					 (unsigned long long)evt[i]);

			cond_resched();
		}

		/*
		 * Not much we can do on overflow, so scream and pretend we're
		 * trying harder.
		 */
		if (queue_sync_prod_in(q) == -EOVERFLOW)
			dev_err(smmu->dev, "EVTQ overflow detected -- events lost\n");
	} while (!queue_empty(llq));

	/* Sync our overflow flag, as we believe we're up to speed */
	queue_sync_cons_ovf(q);
	pm_runtime_put(smmu->dev);
	return IRQ_HANDLED;
}

static irqreturn_t kvm_arm_smmu_gerror_handler(int irq, void *dev)
{
	u32 gerror, gerrorn, active;
	struct arm_smmu_device *smmu = dev;

	if (pm_runtime_get_if_active(smmu->dev) == 0) {
		dev_err(smmu->dev, "Unable to handle global error interrupt because device not runtime active\n");
		return IRQ_NONE;
	}

	gerror = readl_relaxed(smmu->base + ARM_SMMU_GERROR);
	gerrorn = readl_relaxed(smmu->base + ARM_SMMU_GERRORN);

	active = gerror ^ gerrorn;
	if (!(active & GERROR_ERR_MASK)) {
		pm_runtime_put(smmu->dev);
		return IRQ_NONE; /* No errors pending */
	}

	dev_warn(smmu->dev,
		 "unexpected global error reported (0x%08x), this could be serious\n",
		 active);

	/* There is no API to reconfigure the device at the moment.*/
	if (active & GERROR_SFM_ERR)
		dev_err(smmu->dev, "device has entered Service Failure Mode!\n");

	if (active & GERROR_MSI_GERROR_ABT_ERR)
		dev_warn(smmu->dev, "GERROR MSI write aborted\n");

	if (active & GERROR_MSI_PRIQ_ABT_ERR)
		dev_warn(smmu->dev, "PRIQ MSI write aborted\n");

	if (active & GERROR_MSI_EVTQ_ABT_ERR)
		dev_warn(smmu->dev, "EVTQ MSI write aborted\n");

	if (active & GERROR_MSI_CMDQ_ABT_ERR)
		dev_warn(smmu->dev, "CMDQ MSI write aborted\n");

	if (active & GERROR_PRIQ_ABT_ERR)
		dev_err(smmu->dev, "PRIQ write aborted -- events may have been lost\n");

	if (active & GERROR_EVTQ_ABT_ERR)
		dev_err(smmu->dev, "EVTQ write aborted -- events may have been lost\n");

	if (active & GERROR_CMDQ_ERR) {
		dev_err(smmu->dev, "CMDQ ERR -- Hypervisor cmdq corrupted?\n");
		BUG();
	}

	writel(gerror, smmu->base + ARM_SMMU_GERRORN);
	pm_runtime_put(smmu->dev);
	return IRQ_HANDLED;
}

static irqreturn_t kvm_arm_smmu_pri_handler(int irq, void *dev)
{
	struct arm_smmu_device *smmu = dev;

	dev_err(smmu->dev, "PRI not supported in KVM driver!\n");

	return IRQ_HANDLED;
}

static int kvm_arm_smmu_device_reset(struct host_arm_smmu_device *host_smmu)
{
	int ret;
	u32 reg;
	struct arm_smmu_device *smmu = &host_smmu->smmu;
	u32 irqen_flags = IRQ_CTRL_EVTQ_IRQEN | IRQ_CTRL_GERROR_IRQEN;

	reg = readl_relaxed(smmu->base + ARM_SMMU_CR0);
	if (reg & CR0_SMMUEN)
		dev_warn(smmu->dev, "SMMU currently enabled! Resetting...\n");

	/* Disable bypass */
	host_smmu->boot_gbpa = readl_relaxed(smmu->base + ARM_SMMU_GBPA);
	ret = arm_smmu_update_gbpa(smmu, GBPA_ABORT, 0);
	if (ret)
		return ret;

	ret = arm_smmu_device_disable(smmu);
	if (ret)
		return ret;

	/* Stream table */
	arm_smmu_write_strtab(smmu);

	/* Command queue */
	writeq_relaxed(smmu->cmdq.q.q_base, smmu->base + ARM_SMMU_CMDQ_BASE);

	/* Event queue */
	writeq_relaxed(smmu->evtq.q.q_base, smmu->base + ARM_SMMU_EVTQ_BASE);
	writel_relaxed(smmu->evtq.q.llq.prod, smmu->base + SZ_64K + ARM_SMMU_EVTQ_PROD);
	writel_relaxed(smmu->evtq.q.llq.cons, smmu->base + SZ_64K + ARM_SMMU_EVTQ_CONS);

	/* Disable IRQs first */
	ret = arm_smmu_write_reg_sync(smmu, 0, ARM_SMMU_IRQ_CTRL,
				      ARM_SMMU_IRQ_CTRLACK);
	if (ret) {
		dev_err(smmu->dev, "failed to disable irqs\n");
		return ret;
	}

	/*
	 * We don't support combined irqs for now, no specific reason, they are uncommon
	 * so we just try to avoid bloating the code.
	 */
	if (smmu->combined_irq)
		dev_err(smmu->dev, "Combined irqs not supported by this driver\n");
	else
		arm_smmu_setup_unique_irqs(smmu, kvm_arm_smmu_evt_handler,
					   kvm_arm_smmu_gerror_handler,
					   kvm_arm_smmu_pri_handler);

	if (smmu->features & ARM_SMMU_FEAT_PRI)
		irqen_flags |= IRQ_CTRL_PRIQ_IRQEN;

	/* Enable interrupt generation on the SMMU */
	ret = arm_smmu_write_reg_sync(smmu, irqen_flags,
				      ARM_SMMU_IRQ_CTRL, ARM_SMMU_IRQ_CTRLACK);
	if (ret)
		dev_warn(smmu->dev, "failed to enable irqs\n");

	return 0;
}

static int kvm_arm_probe_scmi_pd(struct device_node *scmi_node,
				 struct kvm_power_domain *pd)
{
	int ret;
	struct resource res;
	struct of_phandle_args args;

	pd->type = KVM_POWER_DOMAIN_ARM_SCMI;

	ret = of_parse_phandle_with_args(scmi_node, "shmem", NULL, 0, &args);
	if (ret)
		return ret;

	ret = of_address_to_resource(args.np, 0, &res);
	if (ret)
		goto out_put_nodes;

	ret = of_property_read_u32(scmi_node, "arm,smc-id",
				   &pd->arm_scmi.smc_id);
	if (ret)
		goto out_put_nodes;

	/*
	 * The shared buffer is unmapped from the host while a request is in
	 * flight, so it has to be on its own page.
	 */
	if (!IS_ALIGNED(res.start, SZ_64K) || resource_size(&res) < SZ_64K) {
		ret = -EINVAL;
		goto out_put_nodes;
	}

	pd->arm_scmi.shmem_base = res.start;
	pd->arm_scmi.shmem_size = resource_size(&res);

out_put_nodes:
	of_node_put(args.np);
	return ret;
}

/* TODO: Move this. None of it is specific to SMMU */
static int kvm_arm_probe_power_domain(struct device *dev,
				      struct kvm_power_domain *pd)
{
	int ret;
	struct device_node *parent;
	struct of_phandle_args args;

	if (!of_get_property(dev->of_node, "power-domains", NULL))
		return 0;

	ret = of_parse_phandle_with_args(dev->of_node, "power-domains",
					 "#power-domain-cells", 0, &args);
	if (ret)
		return ret;

	parent = of_get_parent(args.np);
	if (parent && of_device_is_compatible(parent, "arm,scmi-smc") &&
	    args.args_count > 0) {
		pd->arm_scmi.domain_id = args.args[0];
		ret = kvm_arm_probe_scmi_pd(parent, pd);
	} else {
		dev_warn(dev, "Unknown PM method for %pOF, using HVC\n",
			 args.np);
		pd->type = KVM_POWER_DOMAIN_HOST_HVC;
		pd->device_id = kvm_arm_smmu_cur;
	}
	of_node_put(parent);
	of_node_put(args.np);
	return ret;
}

static int kvm_arm_smmu_probe(struct platform_device *pdev)
{
	int ret;
	size_t size;
	phys_addr_t ioaddr;
	struct resource *res;
	struct arm_smmu_device *smmu;
	struct device *dev = &pdev->dev;
	struct host_arm_smmu_device *host_smmu;
	struct hyp_arm_smmu_v3_device *hyp_smmu;

	if (kvm_arm_smmu_cur >= kvm_arm_smmu_count)
		return -ENOSPC;

	hyp_smmu = &kvm_arm_smmu_array[kvm_arm_smmu_cur];

	host_smmu = devm_kzalloc(dev, sizeof(*host_smmu), GFP_KERNEL);
	if (!host_smmu)
		return -ENOMEM;

	smmu = &host_smmu->smmu;
	smmu->dev = dev;

	ret = arm_smmu_fw_probe(pdev, smmu);
	if (ret)
		return ret;

	ret = kvm_arm_probe_power_domain(dev, &host_smmu->power_domain);
	if (ret)
		return ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	size = resource_size(res);
	if (size < SZ_128K) {
		dev_err(dev, "unsupported MMIO region size (%pr)\n", res);
		return -EINVAL;
	}
	ioaddr = res->start;
	host_smmu->id = kvm_arm_smmu_cur;

	smmu->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(smmu->base))
		return PTR_ERR(smmu->base);

	arm_smmu_probe_irq(pdev, smmu);

	ret = arm_smmu_device_hw_probe(smmu);
	if (ret)
		return ret;

	if (!kvm_arm_smmu_validate_features(smmu))
		return -ENODEV;

	if (kvm_arm_smmu_ops.pgsize_bitmap == -1UL)
		kvm_arm_smmu_ops.pgsize_bitmap = smmu->pgsize_bitmap;
	else
		kvm_arm_smmu_ops.pgsize_bitmap |= smmu->pgsize_bitmap;

	ret = arm_smmu_init_one_queue(smmu, &smmu->cmdq.q, smmu->base,
				      ARM_SMMU_CMDQ_PROD, ARM_SMMU_CMDQ_CONS,
				      CMDQ_ENT_DWORDS, "cmdq");
	if (ret)
		return ret;

	/* evtq */
	ret = arm_smmu_init_one_queue(smmu, &smmu->evtq.q, smmu->base + SZ_64K,
				      ARM_SMMU_EVTQ_PROD, ARM_SMMU_EVTQ_CONS,
				      EVTQ_ENT_DWORDS, "evtq");
	if (ret)
		return ret;

	ret = arm_smmu_init_strtab(smmu);
	if (ret)
		return ret;

	ret = kvm_arm_smmu_device_reset(host_smmu);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, smmu);

	/* Hypervisor parameters */
	hyp_smmu->pgsize_bitmap = smmu->pgsize_bitmap;
	hyp_smmu->oas = smmu->oas;
	hyp_smmu->ias = smmu->ias;
	hyp_smmu->mmio_addr = ioaddr;
	hyp_smmu->mmio_size = size;
	hyp_smmu->features = smmu->features;
	hyp_smmu->iommu.power_domain = host_smmu->power_domain;
	hyp_smmu->ssid_bits = smmu->ssid_bits;

	kvm_arm_smmu_cur++;

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	/*
	 * Take a reference to keep the SMMU powered on while the hypervisor
	 * initializes it.
	 */
	pm_runtime_resume_and_get(dev);

	return arm_smmu_register_iommu(smmu, &kvm_arm_smmu_ops, ioaddr);
}

static void kvm_arm_smmu_remove(struct platform_device *pdev)
{
	struct arm_smmu_device *smmu = platform_get_drvdata(pdev);
	struct host_arm_smmu_device *host_smmu = smmu_to_host(smmu);

	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	/*
	 * There was an error during hypervisor setup. The hyp driver may
	 * have already enabled the device, so disable it.
	 */
	arm_smmu_device_disable(smmu);
	arm_smmu_update_gbpa(smmu, host_smmu->boot_gbpa, GBPA_ABORT);
	arm_smmu_unregister_iommu(smmu);
}

static int kvm_arm_smmu_suspend(struct device *dev)
{
	struct arm_smmu_device *smmu = dev_get_drvdata(dev);
	struct host_arm_smmu_device *host_smmu = smmu_to_host(smmu);

	if (host_smmu->power_domain.type == KVM_POWER_DOMAIN_HOST_HVC)
		return pkvm_iommu_suspend(dev);
	return 0;
}

static int kvm_arm_smmu_resume(struct device *dev)
{
	struct arm_smmu_device *smmu = dev_get_drvdata(dev);
	struct host_arm_smmu_device *host_smmu = smmu_to_host(smmu);

	if (host_smmu->power_domain.type == KVM_POWER_DOMAIN_HOST_HVC)
		return pkvm_iommu_resume(dev);
	return 0;
}

static const struct dev_pm_ops kvm_arm_smmu_pm_ops = {
	SET_RUNTIME_PM_OPS(kvm_arm_smmu_suspend, kvm_arm_smmu_resume, NULL)
};

static const struct of_device_id arm_smmu_of_match[] = {
	{ .compatible = "arm,smmu-v3", },
	{ },
};

static struct platform_driver kvm_arm_smmu_driver = {
	.driver = {
		.name = "kvm-arm-smmu-v3",
		.of_match_table = arm_smmu_of_match,
		.pm = &kvm_arm_smmu_pm_ops,
	},
	.remove = kvm_arm_smmu_remove,
};

static int kvm_arm_smmu_array_alloc(void)
{
	int smmu_order;
	struct device_node *np;

	kvm_arm_smmu_count = 0;
	for_each_compatible_node(np, NULL, "arm,smmu-v3")
		kvm_arm_smmu_count++;

	if (!kvm_arm_smmu_count)
		return 0;

	/* Allocate the parameter list shared with the hypervisor */
	smmu_order = get_order(kvm_arm_smmu_count * sizeof(*kvm_arm_smmu_array));
	kvm_arm_smmu_array = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO,
						      smmu_order);
	if (!kvm_arm_smmu_array)
		return -ENOMEM;

	return 0;
}

static void kvm_arm_smmu_array_free(void)
{
	int order;

	order = get_order(kvm_arm_smmu_count * sizeof(*kvm_arm_smmu_array));
	free_pages((unsigned long)kvm_arm_smmu_array, order);
}

static int smmu_put_device(struct device *dev, void *data)
{
	pm_runtime_put(dev);
	return 0;
}

static int smmu_alloc_atomic_mc(struct kvm_hyp_memcache *atomic_mc)
{
	int ret;
#ifndef MODULE
	u64 i;
	phys_addr_t start, end;

	/*
	 * Allocate pages to cover mapping with PAGE_SIZE for all memory
	 * Then allocate extra for 1GB of MMIO.
	 * Add 10 extra pages as we map the rest with first level blocks
	 * for PAGE_SIZE = 4KB, that should cover 5TB of address space.
	 */
	for_each_mem_range(i, &start, &end) {
		atomic_pages += __hyp_pgtable_max_pages((end - start) >> PAGE_SHIFT);
	}

	atomic_pages += __hyp_pgtable_max_pages(SZ_1G >> PAGE_SHIFT) + 10;
#endif

	/* Module didn't set that parameter. */
	if (!atomic_pages)
		return 0;

	/* For PGD*/
	ret = topup_hyp_memcache(atomic_mc, 1, 3);
	if (ret)
		return ret;
	ret = topup_hyp_memcache(atomic_mc, atomic_pages, 0);
	if (ret)
		return ret;
	pr_info("smmuv3: Allocated %d MiB for atomic usage\n",
		(atomic_pages << PAGE_SHIFT) / SZ_1M);
	/* Topup hyp alloc so IOMMU driver can allocate domains. */
	__pkvm_topup_hyp_alloc(1);

	return ret;
}

/*
 * Drop the PM references of the SMMU taken at probe
 * after it's guaranteed the hypervisor as initialized the SMMUs.
 */
static int kvm_arm_smmu_v3_post_init(void)
{
	if (!kvm_arm_smmu_count)
		return 0;

	WARN_ON(driver_for_each_device(&kvm_arm_smmu_driver.driver, NULL,
				       NULL, smmu_put_device));
	return 0;
}

static int kvm_arm_smmu_v3_init_drv(void)
{
	int ret;
	struct kvm_hyp_memcache atomic_mc;

	/*
	 * Check whether any device owned by the host is behind an SMMU.
	 */
	ret = kvm_arm_smmu_array_alloc();
	if (ret || !kvm_arm_smmu_count)
		return ret;

	ret = platform_driver_probe(&kvm_arm_smmu_driver, kvm_arm_smmu_probe);
	if (ret)
		goto err_free;

	if (kvm_arm_smmu_cur != kvm_arm_smmu_count) {
		/* A device exists but failed to probe */
		ret = -EUNATCH;
		goto err_free;
	}

#ifdef MODULE
	ret = pkvm_load_el2_module(kvm_nvhe_sym(smmu_init_hyp_module),
				   &pkvm_module_token);

	if (ret) {
		pr_err("Failed to load SMMUv3 IOMMU EL2 module: %d\n", ret);
		return ret;
	}
#endif
	/*
	 * These variables are stored in the nVHE image, and won't be accessible
	 * after KVM initialization. Ownership of kvm_arm_smmu_array will be
	 * transferred to the hypervisor as well.
	 */
	kvm_hyp_arm_smmu_v3_smmus = kvm_arm_smmu_array;
	kvm_hyp_arm_smmu_v3_count = kvm_arm_smmu_count;

	ret = smmu_alloc_atomic_mc(&atomic_mc);
	if (ret)
		goto err_free;

	ret = kvm_iommu_init_hyp(ksym_ref_addr_nvhe(smmu_ops), &atomic_mc);
	if (ret)
		return ret;

	/* Preemptively allocate the identity domain. */
	if (atomic_pages) {
		ret = kvm_iommu_alloc_domain(KVM_IOMMU_DOMAIN_IDMAP_ID,
					     KVM_IOMMU_DOMAIN_IDMAP_TYPE);
		if (ret)
			return ret;
	}
	return kvm_arm_smmu_v3_post_init();

err_free:
	kvm_arm_smmu_array_free();
	return ret;
}

static void kvm_arm_smmu_v3_remove_drv(void)
{
	platform_driver_unregister(&kvm_arm_smmu_driver);
}

static pkvm_handle_t kvm_arm_smmu_v3_id(struct device *dev)
{
	struct arm_smmu_device *smmu = dev_get_drvdata(dev);
	struct host_arm_smmu_device *host_smmu = smmu_to_host(smmu);

	return host_smmu->id;
}

static pkvm_handle_t kvm_arm_v3_id_by_of(struct device_node *np)
{
	struct device *dev;

	dev = driver_find_device_by_of_node(&kvm_arm_smmu_driver.driver, np);
	if (!dev)
		return 0;

	put_device(dev);

	return kvm_arm_smmu_v3_id(dev);
}

static int kvm_arm_smmu_v3_num_ids(struct device *dev)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);

	if (!fwspec)
		return -ENODEV;

	return fwspec->num_ids;
}

static int kvm_arm_smmu_v3_device_id(struct device *dev, u32 idx,
				     pkvm_handle_t *out_iommu, u32 *out_sid)
{
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);
	struct kvm_arm_smmu_master *master = dev_iommu_priv_get(dev);

	if (!fwspec || !master)
		return -ENODEV;
	if (idx >= fwspec->num_ids)
		return -ENOENT;
	*out_sid = fwspec->ids[idx];
	*out_iommu = kvm_arm_smmu_v3_id(master->smmu->dev);

	return 0;
}

struct kvm_iommu_driver kvm_smmu_v3_ops = {
	.init_driver = kvm_arm_smmu_v3_init_drv,
	.remove_driver = kvm_arm_smmu_v3_remove_drv,
	.get_iommu_id_by_of = kvm_arm_v3_id_by_of,
	.get_device_iommu_num_ids = kvm_arm_smmu_v3_num_ids,
	.get_device_iommu_id = kvm_arm_smmu_v3_device_id,
};

static int kvm_arm_smmu_v3_register(void)
{
	if (!is_protected_kvm_enabled())
		return 0;

	return kvm_iommu_register_driver(&kvm_smmu_v3_ops);
};

/*
 * Register must be run before de-privliage before kvm_iommu_init_driver
 * for module case, it should be loaded using pKVM early loading which
 * loads it before this point.
 * For builtin drivers we use core_initcall
 */
#ifdef MODULE
module_init(kvm_arm_smmu_v3_register);
#else
core_initcall(kvm_arm_smmu_v3_register);
#endif
MODULE_LICENSE("GPL v2");
