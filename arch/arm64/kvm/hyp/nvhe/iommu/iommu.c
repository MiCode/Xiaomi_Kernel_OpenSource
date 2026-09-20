// SPDX-License-Identifier: GPL-2.0
/*
 * IOMMU operations for pKVM
 *
 * Copyright (C) 2022 Linaro Ltd.
 */

#include <asm/kvm_hyp.h>
#include <asm/kvm_hypevents.h>

#include <hyp/adjust_pc.h>

#include <kvm/iommu.h>
#include <nvhe/alloc_mgt.h>
#include <nvhe/iommu.h>
#include <nvhe/mem_protect.h>
#include <nvhe/mm.h>

static DEFINE_PER_CPU(struct kvm_iommu_paddr_cache, kvm_iommu_unmap_cache);

void **kvm_hyp_iommu_domains;

phys_addr_t cma_base;
size_t cma_size;

#define MAX_BLOCK_POOLS 16

static struct hyp_pool iommu_system_pool;
static struct hyp_pool iommu_block_pools[MAX_BLOCK_POOLS];
static struct hyp_pool iommu_atomic_pool;

/*
 * hyp_pool->lock is dropped multiple times during a block_pool reclaim. We then
 * need another global lock to serialize that operation with an allocation.
 */
static DEFINE_HYP_SPINLOCK(__block_pools_lock);
bool __block_pools_available;

static const u8 pmd_order = PMD_SHIFT - PAGE_SHIFT;

DECLARE_PER_CPU(struct kvm_hyp_req, host_hyp_reqs);

static atomic_t kvm_iommu_idmap_initialized;

/*
 * All domain operations are lockless thanks to atomics, except for
 * alloc/free domain where:
 * alloc_domain: Wants to initialize the domain and only after set the refcount
 *   so it can be used when fully initialized.
 * free_domain: Wants to clear the domain refcount, then clear the domain, so
 *   no other call can use it while being freed.
 * This creates a race when a domain refcount is 0 and still in the free domain
 * and another alloc_domain is called for the same ID.
 * This should never happen with well behaved host.
 */
static DEFINE_HYP_SPINLOCK(kvm_iommu_domain_lock);

static inline void kvm_iommu_idmap_init_done(void)
{
	atomic_set_release(&kvm_iommu_idmap_initialized, 1);
}

static inline bool kvm_iommu_is_ready(void)
{
	return atomic_read_acquire(&kvm_iommu_idmap_initialized) == 1;
}

void *kvm_iommu_donate_pages(u8 order, bool request)
{
	struct kvm_hyp_req *req = this_cpu_ptr(&host_hyp_reqs);
	static int last_block_pool;
	void *p;
	int i;

	if (!READ_ONCE(__block_pools_available))
		goto from_system_pool;

	hyp_spin_lock(&__block_pools_lock);

	i = last_block_pool;
	do {
		p = hyp_alloc_pages(&iommu_block_pools[i], order);
		if (p) {
			last_block_pool = i;
			hyp_spin_unlock(&__block_pools_lock);
			return p;
		}

		if (++i >= MAX_BLOCK_POOLS)
			i = 0;
	} while (i != last_block_pool);

	WRITE_ONCE(__block_pools_available, 0);

	hyp_spin_unlock(&__block_pools_lock);

from_system_pool:
	p = hyp_alloc_pages(&iommu_system_pool, order);
	if (p)
		return p;

	if (request) {
		req->type = KVM_HYP_REQ_TYPE_MEM;
		req->mem.dest = REQ_MEM_DEST_HYP_IOMMU;
		req->mem.sz_alloc = (1 << order) * PAGE_SIZE;
		req->mem.nr_pages = 1;
	}
	return NULL;
}

void __kvm_iommu_reclaim_pages(struct hyp_pool *pool, void *p, u8 order)
{
	/*
	 * Order MUST be same allocated page, however the buddy allocator
	 * is allowed to give higher order pages.
	 */
	BUG_ON(order > hyp_virt_to_page(p)->order);

	hyp_put_page(pool, p);
}

void kvm_iommu_reclaim_pages(void *p, u8 order)
{
	phys_addr_t phys = hyp_virt_to_phys(p);
	int i;

	if (phys < cma_base || phys >= (cma_base + cma_size)) {
		__kvm_iommu_reclaim_pages(&iommu_system_pool, p, order);
		return;
	}

	hyp_spin_lock(&__block_pools_lock);

	for (i = 0; i < MAX_BLOCK_POOLS; i++) {
		struct hyp_pool *pool = &iommu_block_pools[i];

		if (!pool->max_order)
			continue;

		if (phys >= pool->range_start && phys < pool->range_end) {
			__kvm_iommu_reclaim_pages(pool, p, order);
			hyp_spin_unlock(&__block_pools_lock);
			return;
		}
	}

	hyp_spin_lock(&__block_pools_lock);

	WARN_ON(1);
}

void *kvm_iommu_donate_pages_atomic(u8 order)
{
	return hyp_alloc_pages(&iommu_atomic_pool, order);
}

void kvm_iommu_reclaim_pages_atomic(void *p, u8 order)
{
	__kvm_iommu_reclaim_pages(&iommu_atomic_pool, p, order);
}

/* Request to hypervisor. */
int kvm_iommu_request(struct kvm_hyp_req *req)
{
	struct kvm_hyp_req *cur_req = this_cpu_ptr(&host_hyp_reqs);

	if (cur_req->type != KVM_HYP_LAST_REQ)
		return -EBUSY;

	memcpy(cur_req, req, sizeof(struct kvm_hyp_req));

	return 0;
}

bool kvm_iommu_donate_from_cma(phys_addr_t phys, unsigned long order)
{
	phys_addr_t end = phys + PAGE_SIZE * (1 << order);

	if (end <= phys)
		return false;

	if (order != pmd_order)
		return false;

	if (!IS_ALIGNED(phys, PMD_SIZE))
		return false;

	if (phys < cma_base || end > cma_base + cma_size)
		return false;

	return true;
}

struct hyp_pool *__get_empty_block_pool(phys_addr_t phys)
{
	int p;

	for (p = 0; p < MAX_BLOCK_POOLS; p++) {
		struct hyp_pool *pool = &iommu_block_pools[p];

		if (pool->max_order)
			continue;

		if (hyp_pool_init(pool, hyp_phys_to_pfn(phys), 1 << pmd_order, 0))
			return NULL;

		WRITE_ONCE(__block_pools_available, 1);

		return pool;
	}

	return NULL;
}

void __repudiate_host_page(void *addr, unsigned long order,
			   struct kvm_hyp_memcache *host_mc)
{
	push_hyp_memcache(host_mc, addr, hyp_virt_to_phys, order);
	WARN_ON(__pkvm_hyp_donate_host(hyp_virt_to_pfn(addr), 1 << order));
}

int kvm_iommu_refill(struct kvm_hyp_memcache *host_mc)
{
	struct kvm_hyp_memcache tmp = *host_mc;

	if (!kvm_iommu_ops)
		return -EINVAL;

	/* Paired with smp_wmb() in kvm_iommu_init() */
	smp_rmb();

	while (tmp.nr_pages) {
		unsigned long order = FIELD_GET(~PAGE_MASK, tmp.head);
		phys_addr_t phys = tmp.head & PAGE_MASK;
		struct hyp_pool *pool = &iommu_system_pool;
		u64 nr_pages;
		void *addr;

		if (check_shl_overflow(1UL, order, &nr_pages) ||
		    !IS_ALIGNED(phys, PAGE_SIZE << order))
			return -EINVAL;

		addr = admit_host_page(&tmp, order);
		if (!addr)
			return -EINVAL;
		*host_mc = tmp;

		if (kvm_iommu_donate_from_cma(phys, order)) {
			hyp_spin_lock(&__block_pools_lock);
			pool = __get_empty_block_pool(phys);
			hyp_spin_unlock(&__block_pools_lock);
			if (!pool) {
				__repudiate_host_page(addr, order, &tmp);
				*host_mc = tmp;
				return -EBUSY;
			}
		} else {
			hyp_virt_to_page(addr)->order = order;
			hyp_set_page_refcounted(hyp_virt_to_page(addr));
			hyp_put_page(pool, addr);
		}
	}

	return 0;
}

void kvm_iommu_reclaim(struct kvm_hyp_memcache *host_mc, int target)
{
	unsigned long prev_nr_pages = host_mc->nr_pages;
	unsigned long block_pages = 1 << pmd_order;
	int p = 0;

	if (!kvm_iommu_ops)
		return;

	smp_rmb();

	reclaim_hyp_pool(&iommu_system_pool, host_mc, target);

	target -= host_mc->nr_pages - prev_nr_pages;

	while (target > block_pages && p < MAX_BLOCK_POOLS) {
		struct hyp_pool *pool = &iommu_block_pools[p];

		hyp_spin_lock(&__block_pools_lock);

		if (hyp_pool_free_pages(pool) == block_pages) {
			reclaim_hyp_pool(pool, host_mc, block_pages);
			hyp_pool_init_empty(pool, 1);
			target -= block_pages;
		}

		hyp_spin_unlock(&__block_pools_lock);
		p++;
	}
}

int kvm_iommu_reclaimable(void)
{
	unsigned long reclaimable = 0;
	int p;

	if (!kvm_iommu_ops)
		return 0;

	smp_rmb();

	reclaimable += hyp_pool_free_pages(&iommu_system_pool);

	/*
	 * This also accounts for blocks, allocated from the CMA region. This is
	 * not exactly what the shrinker wants... but we need to have a way to
	 * report this memory to the host.
	 */

	for (p = 0; p < MAX_BLOCK_POOLS; p++) {
		unsigned long __free_pages = hyp_pool_free_pages(&iommu_block_pools[p]);

		if (__free_pages == 1 << pmd_order)
			reclaimable += __free_pages;
	}

	return reclaimable;
}

struct hyp_mgt_allocator_ops kvm_iommu_allocator_ops = {
	.refill = kvm_iommu_refill,
	.reclaim = kvm_iommu_reclaim,
	.reclaimable = kvm_iommu_reclaimable,
};

static struct kvm_hyp_iommu_domain *
__handle_to_domain(pkvm_handle_t domain_id, bool alloc)
{
	int idx;
	struct kvm_hyp_iommu_domain *domains;

	if (domain_id >= KVM_IOMMU_MAX_DOMAINS)
		return NULL;
	domain_id = array_index_nospec(domain_id, KVM_IOMMU_MAX_DOMAINS);

	idx = domain_id / KVM_IOMMU_DOMAINS_PER_PAGE;
	domains = (struct kvm_hyp_iommu_domain *)READ_ONCE(kvm_hyp_iommu_domains[idx]);
	if (!domains) {
		if (!alloc)
			return NULL;
		domains = kvm_iommu_donate_page();
		if (!domains)
			return NULL;
		/*
		 * handle_to_domain() does not have to be called under a lock,
		 * but even though we allocate a leaf in all cases, it's only
		 * really a valid thing to do under alloc_domain(), which uses a
		 * lock. Races are therefore a host bug and we don't need to be
		 * delicate about it.
		 */
		if (WARN_ON(cmpxchg64_relaxed(&kvm_hyp_iommu_domains[idx], 0,
					      (void *)domains) != 0)) {
			kvm_iommu_reclaim_page(domains);
			return NULL;
		}
	}
	return &domains[domain_id % KVM_IOMMU_DOMAINS_PER_PAGE];
}

static struct kvm_hyp_iommu_domain *
handle_to_domain(pkvm_handle_t domain_id)
{
	return __handle_to_domain(domain_id, true);
}

static int domain_get(struct kvm_hyp_iommu_domain *domain)
{
	int old = atomic_fetch_inc_acquire(&domain->refs);

	BUG_ON(!old || (old + 1 < 0));
	return 0;
}

static void domain_put(struct kvm_hyp_iommu_domain *domain)
{
	BUG_ON(!atomic_dec_return_release(&domain->refs));
}

int kvm_iommu_alloc_domain(pkvm_handle_t domain_id, u32 type)
{
	int ret = -EINVAL;
	struct kvm_hyp_iommu_domain *domain;

	domain = handle_to_domain(domain_id);
	if (!domain)
		return -ENOMEM;

	hyp_spin_lock(&kvm_iommu_domain_lock);
	if (atomic_read(&domain->refs))
		goto out_unlock;

	domain->domain_id = domain_id;
	ret = kvm_iommu_ops->alloc_domain(domain, type);
	if (ret)
		goto out_unlock;

	atomic_set_release(&domain->refs, 1);
out_unlock:
	hyp_spin_unlock(&kvm_iommu_domain_lock);
	return ret;
}

int kvm_iommu_free_domain(pkvm_handle_t domain_id)
{
	int ret = 0;
	struct kvm_hyp_iommu_domain *domain;

	domain = handle_to_domain(domain_id);
	if (!domain)
		return -EINVAL;

	hyp_spin_lock(&kvm_iommu_domain_lock);
	if (WARN_ON(atomic_cmpxchg_acquire(&domain->refs, 1, 0) != 1)) {
		ret = -EINVAL;
		goto out_unlock;
	}

	kvm_iommu_ops->free_domain(domain);

	/* Set domain->refs to 0 and mark it as unused. */
	memset(domain, 0, sizeof(*domain));

out_unlock:
	hyp_spin_unlock(&kvm_iommu_domain_lock);

	return ret;
}

int kvm_iommu_attach_dev(pkvm_handle_t iommu_id, pkvm_handle_t domain_id,
			 u32 endpoint_id, u32 pasid, u32 pasid_bits)
{
	int ret;
	struct kvm_hyp_iommu *iommu;
	struct kvm_hyp_iommu_domain *domain;

	iommu = kvm_iommu_ops->get_iommu_by_id(iommu_id);
	if (!iommu)
		return -EINVAL;

	domain = handle_to_domain(domain_id);
	if (!domain || domain_get(domain))
		return -EINVAL;

	ret = kvm_iommu_ops->attach_dev(iommu, domain, endpoint_id, pasid, pasid_bits);
	if (ret)
		domain_put(domain);
	return ret;
}

int kvm_iommu_detach_dev(pkvm_handle_t iommu_id, pkvm_handle_t domain_id,
			 u32 endpoint_id, u32 pasid)
{
	int ret;
	struct kvm_hyp_iommu *iommu;
	struct kvm_hyp_iommu_domain *domain;

	iommu = kvm_iommu_ops->get_iommu_by_id(iommu_id);
	if (!iommu)
		return -EINVAL;

	domain = handle_to_domain(domain_id);
	if (!domain || atomic_read(&domain->refs) <= 1)
		return -EINVAL;

	ret = kvm_iommu_ops->detach_dev(iommu, domain, endpoint_id, pasid);
	if (ret)
		return ret;
	domain_put(domain);
	return ret;
}

#define IOMMU_PROT_MASK (IOMMU_READ | IOMMU_WRITE | IOMMU_CACHE |\
			 IOMMU_NOEXEC | IOMMU_MMIO | IOMMU_PRIV)

int kvm_iommu_map_pages(pkvm_handle_t domain_id, unsigned long iova,
			phys_addr_t paddr, size_t pgsize,
			size_t pgcount, int prot, unsigned long *mapped)
{
	size_t size;
	int ret;
	size_t total_mapped = 0;
	struct kvm_hyp_iommu_domain *domain;

	*mapped = 0;
	if (!kvm_iommu_ops || !kvm_iommu_ops->map_pages)
		return -ENODEV;

	if (prot & ~IOMMU_PROT_MASK)
		return -EOPNOTSUPP;

	if (__builtin_mul_overflow(pgsize, pgcount, &size) ||
	    iova + size < iova || paddr + size < paddr)
		return -E2BIG;

	if (domain_id == KVM_IOMMU_DOMAIN_IDMAP_ID)
		return -EINVAL;

	domain = handle_to_domain(domain_id);
	if (!domain || domain_get(domain))
		return -ENOENT;

	ret = __pkvm_host_use_dma(paddr, size);
	if (ret)
		goto out_put_domain;

	ret = kvm_iommu_ops->map_pages(domain, iova, paddr, pgsize, pgcount,
				       prot, &total_mapped);

	pgcount -= total_mapped / pgsize;
	/*
	 * unuse the bits that haven't been mapped yet. The host calls back
	 * either to continue mapping, or to unmap and unuse what's been done
	 * so far.
	 */
	if (pgcount)
		__pkvm_host_unuse_dma(paddr + total_mapped, pgcount * pgsize);

	*mapped = total_mapped;

out_put_domain:
	domain_put(domain);
	/* Mask -ENOMEM, as it's passed as a request. */
	return ret == -ENOMEM ? 0 : ret;
}

/* Based on  the kernel iommu_iotlb* but with some tweak, this can be unified later. */
static inline void kvm_iommu_iotlb_sync(struct kvm_hyp_iommu_domain *domain,
					struct iommu_iotlb_gather *iotlb_gather)
{
	if (kvm_iommu_ops->iotlb_sync)
		kvm_iommu_ops->iotlb_sync(domain, iotlb_gather);

	iommu_iotlb_gather_init(iotlb_gather);
}

static bool kvm_iommu_iotlb_gather_is_disjoint(struct iommu_iotlb_gather *gather,
					       unsigned long iova, size_t size)
{
	unsigned long start = iova, end = start + size - 1;

	return gather->end != 0 &&
		(end + 1 < gather->start || start > gather->end + 1);
}

static inline void kvm_iommu_iotlb_gather_add_range(struct iommu_iotlb_gather *gather,
						    unsigned long iova, size_t size)
{
	unsigned long end = iova + size - 1;

	if (gather->start > iova)
		gather->start = iova;
	if (gather->end < end)
		gather->end = end;
}

void kvm_iommu_iotlb_gather_add_page(struct kvm_hyp_iommu_domain *domain,
				     struct iommu_iotlb_gather *gather,
				     unsigned long iova,
				     size_t size)
{
	if ((gather->pgsize && gather->pgsize != size) ||
	    kvm_iommu_iotlb_gather_is_disjoint(gather, iova, size))
		kvm_iommu_iotlb_sync(domain, gather);

	gather->pgsize = size;
	kvm_iommu_iotlb_gather_add_range(gather, iova, size);
}

void kvm_iommu_flush_unmap_cache(struct kvm_iommu_paddr_cache *cache)
{
	while (cache->ptr) {
		cache->ptr--;
		WARN_ON(__pkvm_host_unuse_dma(cache->paddr[cache->ptr],
					      cache->pgsize[cache->ptr]));
	}
}

size_t kvm_iommu_unmap_pages(pkvm_handle_t domain_id,
			     unsigned long iova, size_t pgsize, size_t pgcount)
{
	size_t size;
	size_t unmapped;
	size_t total_unmapped = 0;
	struct kvm_hyp_iommu_domain *domain;
	size_t max_pgcount;
	struct iommu_iotlb_gather iotlb_gather;
	struct kvm_iommu_paddr_cache *cache = this_cpu_ptr(&kvm_iommu_unmap_cache);

	if (!kvm_iommu_ops || !kvm_iommu_ops->unmap_pages)
		return 0;

	if (!pgsize || !pgcount)
		return 0;

	if (__builtin_mul_overflow(pgsize, pgcount, &size) ||
	    iova + size < iova)
		return 0;

	if (domain_id == KVM_IOMMU_DOMAIN_IDMAP_ID)
		return 0;

	domain = handle_to_domain(domain_id);
	if (!domain || domain_get(domain))
		return 0;

	iommu_iotlb_gather_init(&iotlb_gather);

	while (total_unmapped < size) {
		max_pgcount = min_t(size_t, pgcount, KVM_IOMMU_PADDR_CACHE_MAX);
		unmapped = kvm_iommu_ops->unmap_pages(domain, iova, pgsize,
						      max_pgcount, &iotlb_gather, cache);
		if (!unmapped)
			break;
		kvm_iommu_iotlb_sync(domain, &iotlb_gather);
		kvm_iommu_flush_unmap_cache(cache);
		iova += unmapped;
		total_unmapped += unmapped;
		pgcount -= unmapped / pgsize;
	}

	domain_put(domain);
	return total_unmapped;
}

phys_addr_t kvm_iommu_iova_to_phys(pkvm_handle_t domain_id, unsigned long iova)
{
	phys_addr_t phys = 0;
	struct kvm_hyp_iommu_domain *domain;

	if (!kvm_iommu_ops || !kvm_iommu_ops->iova_to_phys)
		return 0;

	if (domain_id == KVM_IOMMU_DOMAIN_IDMAP_ID)
		return iova;

	domain = handle_to_domain( domain_id);

	if (!domain || domain_get(domain))
		return 0;

	phys = kvm_iommu_ops->iova_to_phys(domain, iova);
	domain_put(domain);
	return phys;
}

bool kvm_iommu_host_dabt_handler(struct kvm_cpu_context *host_ctxt, u64 esr, u64 addr)
{
	bool ret = false;

	if (kvm_iommu_ops && kvm_iommu_ops->dabt_handler)
		ret = kvm_iommu_ops->dabt_handler(host_ctxt, esr, addr);

	if (ret)
		kvm_skip_host_instr();

	return ret;
}

static int iommu_power_on(struct kvm_power_domain *pd)
{
	struct kvm_hyp_iommu *iommu = container_of(pd, struct kvm_hyp_iommu,
						   power_domain);
	bool prev;
	int ret;

	kvm_iommu_lock(iommu);
	prev = iommu->power_is_off;
	iommu->power_is_off = false;
	ret = kvm_iommu_ops->resume ? kvm_iommu_ops->resume(iommu) : 0;
	if (ret)
		iommu->power_is_off = prev;
	kvm_iommu_unlock(iommu);
	return ret;
}

static int iommu_power_off(struct kvm_power_domain *pd)
{
	struct kvm_hyp_iommu *iommu = container_of(pd, struct kvm_hyp_iommu,
						   power_domain);
	bool prev;
	int ret;

	kvm_iommu_lock(iommu);
	prev = iommu->power_is_off;
	iommu->power_is_off = true;
	ret = kvm_iommu_ops->suspend ? kvm_iommu_ops->suspend(iommu) : 0;
	if (ret)
		iommu->power_is_off = prev;
	kvm_iommu_unlock(iommu);
	return ret;
}

static const struct kvm_power_domain_ops iommu_power_ops = {
	.power_on	= iommu_power_on,
	.power_off	= iommu_power_off,
};

int kvm_iommu_init_device(struct kvm_hyp_iommu *iommu)
{
	kvm_iommu_lock_init(iommu);

	return pkvm_init_power_domain(&iommu->power_domain, &iommu_power_ops);
}

static int kvm_iommu_init_idmap(struct kvm_hyp_memcache *atomic_mc)
{
	int ret;

	/* atomic_mc is optional. */
	if (!atomic_mc->head)
		return 0;
	ret = hyp_pool_init_empty(&iommu_atomic_pool, 1024 /* order = 10 */);
	if (ret)
		return ret;

	return refill_hyp_pool(&iommu_atomic_pool, atomic_mc);
}

int kvm_iommu_init(struct kvm_iommu_ops *ops, struct kvm_hyp_memcache *atomic_mc,
		   unsigned long init_arg)
{
	int i, ret;

	if (WARN_ON(!ops->get_iommu_by_id ||
		    !ops->alloc_domain ||
		    !ops->free_domain ||
		    !ops->attach_dev ||
		    !ops->detach_dev))
		return -ENODEV;

	ret = __pkvm_host_donate_hyp(__hyp_pa(kvm_hyp_iommu_domains) >> PAGE_SHIFT,
				     1 << get_order(KVM_IOMMU_DOMAINS_ROOT_SIZE));
	if (ret)
		return ret;

	ret = hyp_pool_init_empty(&iommu_system_pool, 64 /* order = 6*/);
	if (ret)
		return ret;

	for (i = 0; i < MAX_BLOCK_POOLS; i++) {
		ret = hyp_pool_init_empty(&iommu_block_pools[i], 1);
		if (ret)
			return ret;
	}

	/* Ensure iommu_system_pool is ready _before_ iommu_ops is set */
	smp_wmb();
	kvm_iommu_ops = ops;

	ret = kvm_iommu_init_idmap(atomic_mc);
	if (ret)
		return ret;

	return ops->init ? ops->init(init_arg) : 0;
}

static inline int pkvm_to_iommu_prot(int prot)
{
	switch (prot) {
	case PKVM_HOST_MEM_PROT:
		return IOMMU_READ | IOMMU_WRITE;
	case PKVM_HOST_MMIO_PROT:
		return IOMMU_READ | IOMMU_WRITE | IOMMU_MMIO;
	case 0:
		return 0;
	default:
		/* We don't understand that, it might cause corruption, so panic. */
		BUG();
	}

	return 0;
}
void kvm_iommu_host_stage2_idmap(phys_addr_t start, phys_addr_t end,
				 enum kvm_pgtable_prot prot)
{
	struct kvm_hyp_iommu_domain *domain;

	if (!kvm_iommu_is_ready())
		return;

	trace_iommu_idmap(start, end, prot);

	domain = __handle_to_domain(KVM_IOMMU_DOMAIN_IDMAP_ID, false);

	kvm_iommu_ops->host_stage2_idmap(domain, start, end, pkvm_to_iommu_prot(prot));
}

static int __snapshot_host_stage2(const struct kvm_pgtable_visit_ctx *ctx,
				  enum kvm_pgtable_walk_flags visit)
{
	u64 start = ctx->addr;
	kvm_pte_t pte = *ctx->ptep;
	u32 level = ctx->level;
	struct kvm_hyp_iommu_domain *domain = ctx->arg;
	u64 end = start + kvm_granule_size(level);
	int prot = IOMMU_READ | IOMMU_WRITE;

	if (!addr_is_memory(start))
		prot |= IOMMU_MMIO;

	if (!pte || kvm_pte_valid(pte))
		kvm_iommu_ops->host_stage2_idmap(domain, start, end, prot);

	return 0;
}

int kvm_iommu_snapshot_host_stage2(struct kvm_hyp_iommu_domain *domain)
{
	int ret;
	struct kvm_pgtable_walker walker = {
		.cb	= __snapshot_host_stage2,
		.flags	= KVM_PGTABLE_WALK_LEAF,
		.arg = domain,
	};
	struct kvm_pgtable *pgt = &host_mmu.pgt;

	hyp_spin_lock(&host_mmu.lock);
	ret = kvm_pgtable_walk(pgt, 0, BIT(pgt->ia_bits), &walker);
	/* Start receiving calls to host_stage2_idmap. */
	if (!ret)
		kvm_iommu_idmap_init_done();
	hyp_spin_unlock(&host_mmu.lock);

	return ret;
}

int kvm_iommu_iotlb_sync_map(pkvm_handle_t domain_id,
			     unsigned long iova, size_t size)
{
	struct kvm_hyp_iommu_domain *domain;
	int ret;

	if (!kvm_iommu_ops || !kvm_iommu_ops->iotlb_sync_map)
		return -ENODEV;

	if (!size || (iova + size < iova))
		return -EINVAL;

	if (domain_id == KVM_IOMMU_DOMAIN_IDMAP_ID)
		return -EINVAL;

	domain = handle_to_domain(domain_id);

	if (!domain || domain_get(domain))
		return -EINVAL;

	ret = kvm_iommu_ops->iotlb_sync_map(domain, iova, size);
	domain_put(domain);
	return ret;
}
