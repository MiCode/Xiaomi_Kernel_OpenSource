// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <asm/cacheflush.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#include "m4u_priv.h"

struct m4u_pte_info_t {
	struct imu_pgd_t *pgd;
	struct imu_pte_t *pte;
	unsigned int mva;
	unsigned long pa;
	unsigned int size;
	int valid;
};

static inline void m4u_set_pgd_val(struct imu_pgd_t *pgd,
			unsigned int val)
{
	COM_WriteReg32((unsigned long)&(imu_pgd_val(*pgd)), val);
}

static inline void read_lock_domain(
	struct m4u_domain_t *domain)
{
	mutex_lock(&domain->pgtable_mutex);
}

static inline void read_unlock_domain(
	struct m4u_domain_t *domain)
{
	mutex_unlock(&domain->pgtable_mutex);
}

static inline void write_lock_domain(
	struct m4u_domain_t *domain)
{
	mutex_lock(&domain->pgtable_mutex);
}

static inline void write_unlock_domain(
	struct m4u_domain_t *domain)
{
	mutex_unlock(&domain->pgtable_mutex);
}

/* should not hold pg_lock when call this func. */
inline int m4u_get_pt_type(struct m4u_domain_t *domain,
			unsigned int mva)
{
	struct imu_pgd_t *pgd;
	struct imu_pte_t *pte;
	int ret;

	read_lock_domain(domain);

	pgd = imu_pgd_offset(domain, mva);

	if (F_PGD_TYPE_IS_PAGE(*pgd)) {
		pte = imu_pte_offset_map(pgd, mva);
		if (F_PTE_TYPE_GET(imu_pte_val(*pte)) == F_PTE_TYPE_LARGE) {
			imu_pte_unmap(pte);
			ret = MMU_PT_TYPE_LARGE_PAGE;
		} else if (F_PTE_TYPE_GET(imu_pte_val(*pte)) ==
				F_PTE_TYPE_SMALL) {
			imu_pte_unmap(pte);
			ret = MMU_PT_TYPE_SMALL_PAGE;
		} else {
			imu_pte_unmap(pte);
			ret = -1;
		}
	} else if (F_PGD_TYPE_IS_SECTION(*pgd)) {
		ret = MMU_PT_TYPE_SECTION;
	} else if (F_PGD_TYPE_IS_SUPERSECTION(*pgd)) {
		ret = MMU_PT_TYPE_SUPERSECTION;
	} else {
		ret = -1;
	}
	read_unlock_domain(domain);
	return ret;
}

static inline unsigned int m4u_get_pt_type_size(int type)
{
	if (type == MMU_PT_TYPE_SMALL_PAGE)
		return MMU_SMALL_PAGE_SIZE;
	else if (type == MMU_PT_TYPE_LARGE_PAGE)
		return MMU_LARGE_PAGE_SIZE;
	else if (type == MMU_PT_TYPE_SECTION)
		return MMU_SECTION_SIZE;
	else if (type == MMU_PT_TYPE_SUPERSECTION)
		return MMU_SUPERSECTION_SIZE;
	else
		return -1;
}

/***********************************************************/
/** print pte info to log or sequncial file
 *    if data is NULL, info is out put to kernel log by pr log
 *    if pte is valid, we will print like va->pgd->pte->pa
 *    if pte is invalid, we print as many info as we can.
 * @return NULL
 * @remark
 * @see
 * @author K Zhang      @date 2013/11/18
 */
/************************************************************/
void *__m4u_print_pte(struct m4u_pte_info_t *info, void *data)
{
	if (info->valid) {
		if (info->size == SZ_4K) {
			M4U_PRINT_SEQ(data,
				"mva(0x%x)-->pgd(0x%x)-->pte(0x%x)-->pa(0x%lx) small\n",
					info->mva, imu_pgd_val(*info->pgd),
					imu_pte_val(*info->pte), info->pa);
		} else if (info->size == SZ_64K) {
			M4U_PRINT_SEQ(data,
				"mva(0x%x)-->pgd(0x%x)-->pte(0x%x)-->pa(0x%lx) large\n",
					info->mva,
					imu_pgd_val(*info->pgd),
					imu_pte_val(*info->pte), info->pa);
		} else if (info->size == SZ_1M) {
			M4U_PRINT_SEQ(data,
					"mva(0x%x)-->pgd(0x%x)-->pa(0x%lx) section\n",
					     info->mva,
					     imu_pgd_val(*info->pgd), info->pa);
		} else if (info->size == SZ_16M) {
			M4U_PRINT_SEQ(data,
						"mva(0x%x)-->pgd(0x%x)-->pa(0x%lx) super\n",
					     info->mva,
					     imu_pgd_val(*info->pgd), info->pa);
		}
	} else {
		M4U_PRINT_SEQ(data, "va(0x%x)\n", info->mva);
		M4U_PRINT_SEQ(data, "-->pgd(0x%x)\n", imu_pgd_val(*info->pgd));
		if (info->pte)
			M4U_PRINT_SEQ(data, "-->pte(0x%x)\n",
				imu_pte_val(*info->pte));
		M4U_PRINT_SEQ(data, " invalid\n");
	}

	return NULL;
}

/* domain->pgtable_mutex should be held */
int m4u_get_pte_info(struct m4u_domain_t *domain,
	unsigned int mva, struct m4u_pte_info_t *pte_info)
{
	struct imu_pgd_t *pgd;
	struct imu_pte_t *pte = NULL;
	unsigned long long pa = 0;
	unsigned int size;
	int valid = 1;

	pgd = imu_pgd_offset(domain, mva);

	if (F_PGD_TYPE_IS_PAGE(*pgd)) {
		pte = imu_pte_offset_map(pgd, mva);
		if (F_PTE_TYPE_GET(imu_pte_val(*pte)) == F_PTE_TYPE_LARGE) {
			pa = imu_pte_val(*pte) & F_PTE_PA_LARGE_MSK;
			if (imu_pte_val(*pte) & F_PTE_BIT32_BIT)
				pa |= 0x100000000;
			if (imu_pte_val(*pte) & F_PTE_BIT33_BIT)
				pa |= 0x200000000;

			pa |= mva & (~F_PTE_PA_LARGE_MSK);
			size = MMU_LARGE_PAGE_SIZE;
		} else if (F_PTE_TYPE_GET(imu_pte_val(*pte)) ==
				F_PTE_TYPE_SMALL) {
			pa = imu_pte_val(*pte) & F_PTE_PA_SMALL_MSK;
			if (imu_pte_val(*pte) & F_PTE_BIT32_BIT)
				pa |= 0x100000000;
			if (imu_pte_val(*pte) & F_PTE_BIT33_BIT)
				pa |= 0x200000000;

			pa |= mva & (~F_PTE_PA_SMALL_MSK);
			size = MMU_SMALL_PAGE_SIZE;
		} else {
			valid = 0;
			size = MMU_SMALL_PAGE_SIZE;
		}
	} else {
		pte = NULL;
		if (F_PGD_TYPE_IS_SECTION(*pgd)) {
			pa = imu_pgd_val(*pgd) & F_PGD_PA_SECTION_MSK;
			if (imu_pgd_val(*pgd) & F_PGD_BIT32_BIT)
				pa |= 0x100000000;
			if (imu_pgd_val(*pgd) & F_PGD_BIT33_BIT)
				pa |= 0x200000000;

			pa |= mva & (~F_PGD_PA_SECTION_MSK);
			size = MMU_SECTION_SIZE;
		} else if (F_PGD_TYPE_IS_SUPERSECTION(*pgd)) {
			pa = imu_pgd_val(*pgd) & F_PGD_PA_SUPERSECTION_MSK;
			if (imu_pgd_val(*pgd) & F_PGD_BIT32_BIT)
				pa |= 0x100000000;
			if (imu_pgd_val(*pgd) & F_PGD_BIT33_BIT)
				pa |= 0x200000000;

			pa |= mva & (~F_PGD_PA_SUPERSECTION_MSK);
			size = MMU_SUPERSECTION_SIZE;
		} else {
			valid = 0;
			size = MMU_SECTION_SIZE;
		}
	}

	pte_info->pgd = pgd;
	pte_info->pte = pte;
	pte_info->mva = mva;
	pte_info->pa = pa;
	pte_info->size = size;
	pte_info->valid = valid;
	return valid;
}

typedef void *(m4u_pte_fn_t) (struct m4u_pte_info_t *pte_info, void *data);

/***********************************************************/
/** interate all pte, and call fn for each pte.
 * @param   domain
 * @param   fn       -- to be called for each pte
 * @param   data     -- private data for fn
 *
 * @return NULL of success, non-NULL if interrupted by fn.
 * @remark
 *	1. fn will only be called when pte is valid.
 *	2. if fn return non-NULL, the iteration will return imediately.
 * @see
 * @author K Zhang      @date 2013/11/18
 */
/************************************************************/
void *m4u_for_each_pte(struct m4u_domain_t *domain,
		m4u_pte_fn_t *fn, void *data)
{
	unsigned int mva = 0;
	void *ret;
	struct m4u_pte_info_t pte_info;

	read_lock_domain(domain);
	while (1) {
		m4u_get_pte_info(domain, mva, &pte_info);

		if (pte_info.valid) {
			ret = fn(&pte_info, data);
			if (ret) {
				read_unlock_domain(domain);
				return ret;
			}
		}

		if (mva + pte_info.size < mva)	/* over flow */
			break;
		mva += pte_info.size;
	}

	read_unlock_domain(domain);
	return NULL;
}

/* dump pte info for mva, no matter it's valid or not */
/* this function doesn't lock pgtable lock. */
int m4u_dump_pte_nolock(struct m4u_domain_t *domain,
		unsigned int mva)
{
	struct m4u_pte_info_t pte_info;
	int valid = 0;

	valid = m4u_get_pte_info(domain, mva, &pte_info);

	__m4u_print_pte(&pte_info, NULL);

	/* show pre mva */
	if (pte_info.valid) {
		M4UMSG("pre_mva:0x%x size:0x%x\n", mva, pte_info.size);
		mva -= pte_info.size;
		m4u_get_pte_info(domain, mva, &pte_info);
		__m4u_print_pte(&pte_info, NULL);
	}

	return valid;
}

void m4u_dump_pte(struct m4u_domain_t *domain, unsigned int mva)
{
	read_lock_domain(domain);
	m4u_dump_pte_nolock(domain, mva);
	read_unlock_domain(domain);
}

unsigned long m4u_get_pte(struct m4u_domain_t *domain, unsigned int mva)
{
	struct m4u_pte_info_t pte_info;

	read_lock_domain(domain);
	m4u_get_pte_info(domain, mva, &pte_info);
	read_unlock_domain(domain);

	return pte_info.pa;
}

int _m4u_get_pte(struct m4u_domain_t *domain, unsigned int mva)
{
	struct m4u_pte_info_t pte_info;

	read_lock_domain(domain);
	m4u_get_pte_info(domain, mva, &pte_info);
	read_unlock_domain(domain);

	return pte_info.valid;
}

/***********************************************************/
/** dump pagetable to sequncial file or kernel log.
 * @param   domain   -- domain to dump
 * @param   seq      -- seq file. if NULL, we will dump to kernel log
 *
 * @remark  this func will lock pgtable_lock, it may sleep.
 * @author K Zhang      @date 2013/11/18
 */
/************************************************************/
void m4u_dump_pgtable(struct m4u_domain_t *domain,
			struct seq_file *seq)
{
	M4U_PRINT_SEQ(seq, "m4u dump pgtable start ==============>\n");
	m4u_for_each_pte(domain, __m4u_print_pte, seq);
	M4U_PRINT_SEQ(seq, "m4u dump pgtable done ==============>\n");
}

/* M4U_PROT_CACHE indicates M4U_PROT_SHARE, which route transaction to CCI*/
static inline unsigned int m4u_prot_fixup(unsigned int prot)
{
	/* don't support read/write protect */

/*    if(unlikely(!(prot & (M4U_PROT_READ|M4U_PROT_WRITE))))*/
/*	prot |= M4U_PROT_READ|M4U_PROT_WRITE;*/
/*    if(unlikely((prot&M4U_PROT_WRITE) && !(prot&M4U_PROT_READ)))*/
/*	prot |= M4U_PROT_WRITE;*/


	if (prot & M4U_PROT_CACHE)
		prot |= M4U_PROT_SHARE;

	return prot;
}

/***********************************************************/
/** convert m4u_prot to hardware pgd/pte attribute
 * @param   prot   -- m4u_prot flags
 *
 * @return  pgd or pte attribute
 * @remark
 * @see
 * @author K Zhang      @date 2013/11/18
 */
/************************************************************/
static inline unsigned int __m4u_get_pgd_attr_16M(unsigned int prot)
{
	unsigned int pgprot;

	pgprot = F_PGD_TYPE_SUPERSECTION;
	pgprot |= (prot & M4U_PROT_SEC) ? 0 : F_PGD_NS_BIT_SECTION(1);
	pgprot |= (prot & M4U_PROT_SHARE) ? F_PGD_S_BIT : 0;
	pgprot |= (prot & M4U_PROT_CACHE) ? (F_PGD_C_BIT | F_PGD_B_BIT) : 0;
	if (gM4U_4G_DRAM_Mode)
		pgprot |= F_PGD_BIT32_BIT;
	return pgprot;
}

static inline unsigned int __m4u_get_pgd_attr_1M(unsigned int prot)
{
	unsigned int pgprot;

	pgprot = F_PGD_TYPE_SECTION;
	pgprot |= (prot & M4U_PROT_SEC) ? 0 : F_PGD_NS_BIT_SECTION(1);
	pgprot |= (prot & M4U_PROT_SHARE) ? F_PGD_S_BIT : 0;
	pgprot |= (prot & M4U_PROT_CACHE) ? (F_PGD_C_BIT | F_PGD_B_BIT) : 0;
	if (gM4U_4G_DRAM_Mode)
		pgprot |= F_PGD_BIT32_BIT;
	return pgprot;
}

static inline unsigned int __m4u_get_pgd_attr_page(unsigned int prot)
{
	unsigned int pgprot;

	pgprot = F_PGD_TYPE_PAGE;
	pgprot |= (prot & M4U_PROT_SEC) ? 0 : F_PGD_NS_BIT_PAGE(1);
	return pgprot;
}

static inline unsigned int __m4u_get_pte_attr_64K(unsigned int prot)
{
	unsigned int pgprot;

	pgprot = F_PTE_TYPE_LARGE;
	pgprot |= (prot & M4U_PROT_SHARE) ? F_PTE_S_BIT : 0;
	pgprot |= (prot & M4U_PROT_CACHE) ? (F_PGD_C_BIT | F_PGD_B_BIT) : 0;
	if (gM4U_4G_DRAM_Mode)
		pgprot |= F_PTE_BIT32_BIT;
	return pgprot;
}

static inline unsigned int __m4u_get_pte_attr_4K(unsigned int prot)
{
	unsigned int pgprot;

	pgprot = F_PTE_TYPE_SMALL;
	pgprot |= (prot & M4U_PROT_SHARE) ? F_PTE_S_BIT : 0;
	pgprot |= (prot & M4U_PROT_CACHE) ? (F_PGD_C_BIT | F_PGD_B_BIT) : 0;
	if (gM4U_4G_DRAM_Mode)
		pgprot |= F_PTE_BIT32_BIT;
	return pgprot;
}

/***********************************************************/
/** cache flush for modified pte.
 *   notes: because pte is allocated using slab, cache sync is needed.
 *
 * @author K Zhang      @date 2013/11/18
 */
/************************************************************/
int m4u_clean_pte(struct m4u_domain_t *domain,
		unsigned int mva, unsigned int size)
{
	struct imu_pgd_t *pgd;
	unsigned long long tmp_mva = (unsigned long long)mva;
	unsigned long long end_plus_1 = tmp_mva + (unsigned long long)size;
	unsigned long long sync_entry_nr_sum = 0;

	while (tmp_mva < end_plus_1) {
		pgd = imu_pgd_offset(domain, tmp_mva);

		if (F_PGD_TYPE_IS_PAGE(*pgd)) {
			struct imu_pte_t *pte, *pte_end;
			unsigned long long next_mva, sync_entry_nr;

			pte = imu_pte_offset_map(pgd, tmp_mva);
			if (!pte) {
				/* invalid pte: goto next pgd entry */
				M4UMSG(
					"not clean pte, mva:0x%x, tmp_mva:0x%llx, size:0x%x\n",
					mva, tmp_mva, size);
				tmp_mva = m4u_calc_next_mva(tmp_mva,
					end_plus_1, MMU_SECTION_SIZE);
				continue;
			}

			next_mva = m4u_calc_next_mva(tmp_mva,
				end_plus_1, MMU_SECTION_SIZE);
			/*(next_mva - tmp_mva) / MMU_SMALL_PAGE_SIZE*/
			sync_entry_nr = (next_mva - tmp_mva) >> 12;
			pte_end = pte + sync_entry_nr;
			/* do cache sync for [pte, pte_end) */
#ifdef CONFIG_ARM64
			sync_entry_nr_sum += sync_entry_nr;
			__dma_flush_area((void *)pte,
					 sync_entry_nr * sizeof(*pte));
#else
			dmac_flush_range((void *)pte, (void *)pte_end);
#endif
			imu_pte_unmap(pte);
			tmp_mva = next_mva;

		} else if (F_PGD_TYPE_IS_SUPERSECTION(*pgd)) {
			/* for superseciton: don't need to sync. */
			tmp_mva = m4u_calc_next_mva(tmp_mva,
				end_plus_1, MMU_SUPERSECTION_SIZE);
		} else {
			/* for section/invalid: don't need to sync */
			tmp_mva = m4u_calc_next_mva(tmp_mva,
				end_plus_1, MMU_SECTION_SIZE);
		}
	}

	if (size <= SZ_4K) {
		if (!sync_entry_nr_sum)
			M4UMSG(
			"warning, not cache sync pte, mva:0x%x, size:0x%x\n",
				mva, size);
	} else {
		unsigned int size_tmp;

		size_tmp = (size + (SZ_4K - 1)) >> 12;
		if (size_tmp != sync_entry_nr_sum)
			M4UMSG(
			"warning, not cache sync pte, mva:0x%x, size:0x%x, size_tmp:%u, nr:%llu\n",
				mva, size, size_tmp, sync_entry_nr_sum);
	}

	return 0;
}

struct kmem_cache *gM4u_pte_kmem;
int m4u_pte_allocator_init(void)
{
	gM4u_pte_kmem = kmem_cache_create("m4u_pte",
			IMU_BYTES_PER_PTE, IMU_BYTES_PER_PTE,
			SLAB_CACHE_DMA, NULL);
	M4UINFO("%s: gM4u_pte_kmem = 0x%p, IMU_BYTES_PER_PTE = %d\n",
		__func__, gM4u_pte_kmem,
		(unsigned int)IMU_BYTES_PER_PTE);

	if (IS_ERR_OR_NULL(gM4u_pte_kmem)) {
		M4UMSG("error in %s: ret = %p\n", __func__, gM4u_pte_kmem);
		return -1;
	}

	return 0;
}

/***********************************************************/
/** allocate a new pte
 * @param   domain
 * @param   pgd      -- pgd to allocate for
 * @param   pgprot
 *
 * @return   0 -- pte is allocated
 *	    1 -- pte is not allocated, because it's allocated by others
 *	    <0 -- error
 * @remark
 * @see
 * @author K Zhang      @date 2013/11/18
 */
/************************************************************/
int m4u_alloc_pte(struct m4u_domain_t *domain,
			struct imu_pgd_t *pgd, unsigned int pgprot)
{
	void *pte_new_va = NULL;
	phys_addr_t pte_new;
	unsigned int retry_cnt = 0;

	/* pte_new_va = (unsigned int)kzalloc(IMU_BYTES_PER_PTE, GFP_KERNEL); */
	/* pte_new_va = (unsigned int)get_zeroed_page(GFP_KERNEL); */
	write_unlock_domain(domain);
	for (retry_cnt = 0; retry_cnt < 5; retry_cnt++) {
		pte_new_va = kmem_cache_zalloc(gM4u_pte_kmem,
					       GFP_KERNEL | GFP_DMA);
		if (likely(pte_new_va))
			break;
	}
	write_lock_domain(domain);
	if (unlikely(!pte_new_va)) {
		m4u_aee_print("%s: fail, nomemory\n", __func__);
		return -ENOMEM;
	}
	pte_new = __pa(pte_new_va);

	if (pte_new > 0xffffffffL) {
		if (!!(pte_new & 0x100000000LL))
			(pgprot) = (pgprot) | F_PGD_BIT32_BIT;
		if (!!(pte_new & 0x200000000LL))
			(pgprot) = (pgprot) | F_PGD_BIT33_BIT;
	}
	/* check again in case someone else
	 * may have allocated for this pgd first
	 */
	if (likely(!imu_pgd_val(*pgd))) {
		m4u_set_pgd_val(pgd, (unsigned int)(pte_new) | (pgprot));
		M4ULOG_LOW(
			"%s: pgd: 0x%lx, pte_va:0x%lx, pte_pa: 0x%lx, value: 0x%x\n",
			   __func__, (unsigned long)pgd,
			   (unsigned long)pte_new_va, (unsigned long)pte_new,
			   (unsigned int)(pte_new) | (pgprot));
		return 0;

	} else {
		/* allocated by other thread */
		/* kfree(__va(pte_new)); */
		M4ULOG_LOW("m4u pte allocated by others: pgd=0x%p\n", pgd);
		kmem_cache_free(gM4u_pte_kmem, (void *)pte_new_va);
		return 1;
	}
}

int m4u_free_pte(struct m4u_domain_t *domain,
			struct imu_pgd_t *pgd)
{
	struct imu_pte_t *pte_old;

	pte_old = imu_pte_map(pgd);
	m4u_set_pgd_val(pgd, 0);

	/* kfree(pte_old); */
	/* free_page(pte_old); */
	kmem_cache_free(gM4u_pte_kmem, pte_old);

	return 0;
}

/***********************************************************/
/** m4u_map_XX functions.
 *    map mva<->pa
 * notes: these function doesn't clean pte and invalid tlb
 *	for performance concern.
 *       callers should clean pte + invalid tlb after mapping.
 *
 * @author K Zhang      @date 2013/11/19
 */
/************************************************************/
int m4u_map_16M(struct m4u_domain_t *m4u_domain,
	unsigned int mva, phys_addr_t pa, unsigned int prot)
{
	int i;
	struct imu_pgd_t *pgd;
	unsigned int pgprot;
	unsigned int padscpt;

	if ((mva & (~F_PGD_PA_SUPERSECTION_MSK)) !=
			(pa & (~F_PGD_PA_SUPERSECTION_MSK))) {
		m4u_aee_print
			("error to mk_pte: mva=0x%x, pa=0x%pa, type=%s\n",
				mva, &pa, "supersection");
		return -EINVAL;
	}

	mva &= F_PGD_PA_SUPERSECTION_MSK;
	padscpt = ((unsigned int)pa & F_PGD_PA_SUPERSECTION_MSK);
	if (pa > 0xffffffffL) {
		if (!!(pa & 0x100000000LL))
			padscpt = padscpt | F_PTE_BIT32_BIT;
		if (!!(pa & 0x200000000LL))
			padscpt = padscpt | F_PTE_BIT33_BIT;
	}

	pgprot = __m4u_get_pgd_attr_16M(prot);

	write_lock_domain(m4u_domain);

	pgd = imu_pgd_offset(m4u_domain, mva);

	M4ULOG_LOW("%s: mva: 0x%x, pgd: 0x%lx (0x%lx + 0x%x)\n",
		__func__, mva, (unsigned long)pgd,
		(unsigned long)((m4u_domain)->pgd),
		imu_pgd_index(mva));
	M4ULOG_LOW("pa: 0x%pa, value: 0x%x\n",
		&pa, padscpt | pgprot);

	for (i = 0; i < 16; i++) {
		if (unlikely(imu_pgd_val(*pgd))) {
			m4u_aee_print
				("%s: mva=0x%x, pgd=0x%x, i=%d\n",
					__func__, mva, imu_pgd_val(*pgd), i);
			goto err_out;
		}
		m4u_set_pgd_val(pgd, padscpt | pgprot);
		pgd++;
	}

	write_unlock_domain(m4u_domain);

	return 0;

err_out:
	for (pgd--; i > 0; i--) {
		m4u_set_pgd_val(pgd, 0);
		pgd--;
	}

	write_unlock_domain(m4u_domain);

	return -1;
}

int m4u_map_1M(struct m4u_domain_t *m4u_domain,
	unsigned int mva, phys_addr_t pa, unsigned int prot)
{
	struct imu_pgd_t *pgd;
	unsigned int pgprot;
	unsigned int padscpt;

	if ((mva & (~F_PGD_PA_SECTION_MSK)) != (pa & (~F_PGD_PA_SECTION_MSK))) {
		m4u_aee_print
			("error to mk_pte: mva=0x%x, pa=0x%pa, type=%s\n",
			mva, &pa, "section");
		return -EINVAL;
	}

	mva &= F_PGD_PA_SECTION_MSK;
	padscpt = (unsigned int)(pa & F_PGD_PA_SECTION_MSK);
	if (pa > 0xffffffffL) {
		if (!!(pa & 0x100000000LL))
			padscpt = padscpt | F_PTE_BIT32_BIT;
		if (!!(pa & 0x200000000LL))
			padscpt = padscpt | F_PTE_BIT33_BIT;
	}

	pgprot = __m4u_get_pgd_attr_1M(prot);

	write_lock_domain(m4u_domain);

	pgd = imu_pgd_offset(m4u_domain, mva);

	if (unlikely(imu_pgd_val(*pgd))) {
		write_unlock_domain(m4u_domain);
		m4u_aee_print
			("%s: mva=0x%x, pgd=0x%x\n", __func__,
			mva, imu_pgd_val(*pgd));
		return -1;
	}

	m4u_set_pgd_val(pgd, padscpt | pgprot);

	write_unlock_domain(m4u_domain);

	M4ULOG_LOW("%s: mva: 0x%x, pgd: 0x%lx (0x%lx + 0x%x)\n",
		__func__, mva, (unsigned long)pgd,
		(unsigned long)((m4u_domain)->pgd),
		imu_pgd_index(mva));
	M4ULOG_LOW("pa: 0x%pa, value: 0x%x\n",
		&pa, padscpt | pgprot);

	return 0;
}

int m4u_map_64K(struct m4u_domain_t *m4u_domain,
		unsigned int mva, phys_addr_t pa, unsigned int prot)
{
	int ret, i;
	struct imu_pgd_t *pgd;
	struct imu_pte_t *pte;
	unsigned int pte_new, pgprot;
	unsigned int padscpt;

	if ((mva & (~F_PTE_PA_LARGE_MSK)) != (pa & (~F_PTE_PA_LARGE_MSK))) {
		m4u_aee_print
			("error to mk_pte: mva=0x%x, pa=0x%pa, type=%s\n",
				mva, &pa, "large page");
		return -EINVAL;
	}

	mva &= F_PTE_PA_LARGE_MSK;
	padscpt = (unsigned int)pa & F_PTE_PA_LARGE_MSK;
	if (pa > 0xffffffffL) {
		if (!!(pa & 0x100000000LL))
			padscpt = padscpt | F_PTE_BIT32_BIT;
		if (!!(pa & 0x200000000LL))
			padscpt = padscpt | F_PTE_BIT33_BIT;
	}

	pgprot = __m4u_get_pgd_attr_page(prot);

	write_lock_domain(m4u_domain);

	pgd = imu_pgd_offset(m4u_domain, mva);
	if (!imu_pgd_val(*pgd)) {
		ret = m4u_alloc_pte(m4u_domain, pgd, pgprot);
		if (ret < 0) {
			write_unlock_domain(m4u_domain);
			return ret;
		} else if (ret > 0)
			pte_new = 0;
		else
			pte_new = 1;
	} else {
/*
 *		if (unlikely((imu_pgd_val(*pgd) &
 *				(~F_PGD_PA_PAGETABLE_MSK)) != pgprot)) {
 *			write_unlock_domain(m4u_domain);
 *			m4u_aee_print("%s: mva=0x%x, pgd=0x%x, pgprot=0x%x\n",
 *					__func__, mva,
 *					imu_pgd_val(*pgd), pgprot);
 *			return -1;
 *		}
 */
		pte_new = 0;
	}

	pgprot = __m4u_get_pte_attr_64K(prot);

	pte = imu_pte_offset_map(pgd, mva);

	M4ULOG_LOW(
			"%s: mva: 0x%x, pgd_va:0x%lx, pgd:0x%x, pte:(0x%lx + 0x%x), pa: 0x%pa, value: 0x%x\n",
		   __func__, mva, (unsigned long)pgd,
		   imu_pgd_val(*pgd),
		   (unsigned long)imu_pte_map(pgd),//pte_va
		   imu_pte_index(mva),
		   &pa, padscpt | pgprot);

	for (i = 0; i < 16; i++) {
		if (unlikely(imu_pte_val(pte[i]))) {
			m4u_aee_print("%s: pte=0x%x, i=%d\n",
				__func__, imu_pte_val(pte[i]), i);
			goto err_out;
		}
		imu_pte_val(pte[i]) = padscpt | pgprot;
	}
	imu_pte_unmap(pte);

	write_unlock_domain(m4u_domain);

	return 0;

err_out:
	for (i--; i >= 0; i--)
		imu_pte_val(pte[i]) = 0;
	imu_pte_unmap(pte);

	if (pte_new)
		m4u_free_pte(m4u_domain, pgd);

	write_unlock_domain(m4u_domain);

	return -1;
}

int m4u_map_4K(struct m4u_domain_t *m4u_domain,
		unsigned int mva, phys_addr_t pa, unsigned int prot)
{
	int ret, pte_new;
	struct imu_pgd_t *pgd;
	struct imu_pte_t *pte;
	unsigned int pgprot;
	unsigned int padscpt;

	if ((mva & (~F_PTE_PA_SMALL_MSK)) != (pa & (~F_PTE_PA_SMALL_MSK))) {
		m4u_aee_print
			("error to mk_pte: mva=0x%x, pa=0x%pa, type=%s\n",
				mva, &pa, "small page");
		return -EINVAL;
	}

	mva &= F_PTE_PA_SMALL_MSK;
	padscpt = (unsigned int)pa & F_PTE_PA_SMALL_MSK;
	if (pa > 0xffffffffL) {
		if (!!(pa & 0x100000000LL))
			padscpt = padscpt | F_PTE_BIT32_BIT;
		if (!!(pa & 0x200000000LL))
			padscpt = padscpt | F_PTE_BIT33_BIT;
	}
	pgprot = __m4u_get_pgd_attr_page(prot);

	write_lock_domain(m4u_domain);

	pgd = imu_pgd_offset(m4u_domain, mva);
	if (!imu_pgd_val(*pgd)) {
		ret = m4u_alloc_pte(m4u_domain, pgd, pgprot);
		if (ret < 0) {
			write_unlock_domain(m4u_domain);
			return ret;
		} else if (ret > 0)
			pte_new = 0;
		else
			pte_new = 1;
	} else {
/*
 *		if (unlikely((imu_pgd_val(*pgd) &
 *			(~F_PGD_PA_PAGETABLE_MSK)) != pgprot)) {
 *			write_unlock_domain(m4u_domain);
 *			m4u_aee_print
 *				("%s: mva=0x%x, pgd=0x%x, pgprot=0x%x\n",
 *					__func__, mva,
 *					imu_pgd_val(*pgd), pgprot);
 *			return -1;
 *		}
 */
		pte_new = 0;
	}

	pgprot = __m4u_get_pte_attr_4K(prot);

	pte = imu_pte_offset_map(pgd, mva);

	if (unlikely(imu_pte_val(*pte))) {
		m4u_aee_print("%s: pte=0x%x\n", __func__, imu_pte_val(*pte));
		goto err_out;
	}

	imu_pte_val(*pte) = padscpt | pgprot;

	M4ULOG_LOW(
			"%s: mva: 0x%x, pgd_va:0x%lx, pgd:0x%x, pte:(0x%lx + 0x%x), pa: 0x%pa, value: 0x%x\n",
		   __func__, mva, (unsigned long)pgd,
		   imu_pgd_val(*pgd),
		   (unsigned long)imu_pte_map(pgd),//pte_va
		   imu_pte_index(mva),
		   &pa, padscpt | pgprot);

	imu_pte_unmap(pte);

	write_unlock_domain(m4u_domain);

	return 0;

err_out:
	imu_pte_unmap(pte);
	if (pte_new)
		m4u_free_pte(m4u_domain, pgd);

	write_unlock_domain(m4u_domain);

	return -1;
}

/* notes: both iova & paddr should be aligned. */
static inline int m4u_map_phys_align(
			struct m4u_domain_t *m4u_domain, unsigned int iova,
			phys_addr_t paddr,
			unsigned int size, unsigned int prot)
{
	int ret;

	if (size == SZ_16M)
		ret = m4u_map_16M(m4u_domain, iova, paddr, prot);
	else if (size == SZ_1M)
		ret = m4u_map_1M(m4u_domain, iova, paddr, prot);
	else if (size == SZ_64K)
		ret = m4u_map_64K(m4u_domain, iova, paddr, prot);
	else if (size == SZ_4K)
		ret = m4u_map_4K(m4u_domain, iova, paddr, prot);
	else {
		m4u_aee_print("%s: fail size=0x%x\n", __func__, size);
		return -1;
	}

	return ret;
}


/***********************************************************/
/* map a physical continuous memory to iova (mva).
 * @param   m4u_domain   domain
 * @param   iova         -- iova (mva)
 * @param   paddr        -- physical address
 * @param   size         -- size
 * @param   prot         -- m4u_prot
 *
 * @return   0 on success, others on fail
 * @remark
 * @see     refer to kernel/drivers/iommu/iommu.c iommu_map()
 * @author K Zhang      @date 2013/11/19
 */
/************************************************************/
int m4u_map_phys_range(struct m4u_domain_t *m4u_domain,
		unsigned int iova, phys_addr_t paddr,
		unsigned int size, unsigned int prot)
{
	unsigned int min_pagesz;
	int ret = 0;

	/* find out the minimum page size supported */
	min_pagesz = 1 << __ffs(m4u_domain->pgsize_bitmap);

	/*
	 * both the virtual address and the physical one, as well as
	 * the size of the mapping, must be aligned (at least) to the
	 * size of the smallest page supported by the hardware
	 */
	if (!IS_ALIGNED(iova | paddr | size, min_pagesz)) {
		M4UMSG("unaligned: iova 0x%x pa 0x%pa\n",
				iova, &paddr);
		M4UMSG("size 0x%x min_pagesz 0x%x\n",
				size, min_pagesz);
		return -EINVAL;
	}

	while (size) {
		unsigned long pgsize, addr_merge = (unsigned long)iova | paddr;
		unsigned int pgsize_idx;

		/* Max page size that still fits into 'size' */
		pgsize_idx = __fls(size);

		/* need to consider alignment requirements ? */
		if (likely(addr_merge)) {
			/* Max page size allowed by both iova and paddr */
			unsigned int align_pgsize_idx = __ffs(addr_merge);

			pgsize_idx = min(pgsize_idx, align_pgsize_idx);
		}

		/* build a mask of acceptable page sizes */
		pgsize = (1UL << (pgsize_idx + 1)) - 1;

		/* throw away page sizes not supported by the hardware */
		pgsize &= m4u_domain->pgsize_bitmap;

		/* make sure we're still sane */
		if (!pgsize) {
			M4UERR("page size is NULL\n");
			return -1;
		}

		/* pick the biggest page */
		pgsize_idx = __fls(pgsize);
		pgsize = 1UL << pgsize_idx;

		M4ULOG_LOW("mapping: iova 0x%x pa 0x%pa pgsize %lu\n",
			iova, &paddr, pgsize);

#if (M4U_DVT == MMU_PT_TYPE_SMALL_PAGE)
		if (pgsize > SZ_4K)
			pgsize = SZ_4K;
#endif
#if (M4U_DVT == MMU_PT_TYPE_LARGE_PAGE)
		if (pgsize > SZ_64K)
			pgsize = SZ_64K;
#endif
#if (M4U_DVT == MMU_PT_TYPE_SECTION)
		if (pgsize > SZ_1M)
			pgsize = SZ_1M;
#endif
#if (M4U_DVT == MMU_PT_TYPE_SUPERSECTION)
		if (pgsize > SZ_16M)
			pgsize = SZ_16M;
#endif

		ret = m4u_map_phys_align(m4u_domain, iova, paddr, pgsize, prot);
		if (ret)
			break;

		iova += pgsize;
		paddr += pgsize;
		size -= pgsize;
	}

	/* unroll mapping in case something went wrong */
	if (ret)
		m4u_unmap(m4u_domain, iova, size);
	return ret;
}

int m4u_map_sgtable(struct m4u_domain_t *m4u_domain,
			unsigned int mva,
		    struct sg_table *sg_table,
		    unsigned int size, unsigned int prot)
{
	int i, ret;
	struct scatterlist *sg;
	unsigned long long map_mva = (unsigned long long)mva;
	unsigned long long map_end = map_mva + (unsigned long long)size;

	prot = m4u_prot_fixup(prot);

	/*write_lock_domain(m4u_domain);*/

	for_each_sg(sg_table->sgl, sg, sg_table->nents, i) {
		dma_addr_t pa;
		unsigned int len;

		pa = get_sg_phys(sg);
		len = sg_dma_len(sg);
#ifdef CONFIG_NEED_SG_DMA_LENGTH
		if (sg_dma_address(sg) == 0)
			len = sg->length;
#endif

		M4ULOG_LOW("%s: for_each_sg i: %d, len: %d, mva: %llu\n",
			__func__, i, len, map_mva);

		if (map_mva + len > map_end) {
			M4UMSG("%s: map_mva(%llu)+len(0x%x)>end(%llu)\n",
				__func__, map_mva, len, map_end);
			break;
		}
		if (len == SZ_4K) {	/* for most cases */
			ret = m4u_map_4K(m4u_domain, map_mva, pa, prot);
		} else {
			ret = m4u_map_phys_range(m4u_domain,
				map_mva, pa, len, prot);
		}

		if (ret) {
			M4UMSG("%s: ret: %d, i: %d, sg->dma: 0x%lx\n",
			       __func__, ret, i,
			       (unsigned long)sg_dma_address(sg));
			M4UMSG(" sg->phy: 0x%lx, sg->offset: 0x%x\n",
			       (unsigned long)sg_phys(sg), sg->offset);
			goto err_out;
		} else {
			map_mva += len;
		}
	}

	if (map_mva < map_end) {
		M4UMSG("%s: map_mva(%llu) < map_end(%llu)\n",
			__func__, map_mva, map_end);
		goto err_out;
	}

	m4u_clean_pte(m4u_domain, mva, size);

	m4u_invalid_tlb_by_range(m4u_domain, mva, mva + size - 1);

	/*write_unlock_domain(m4u_domain);*/

	return 0;

err_out:
	/*write_unlock_domain(m4u_domain);*/

	m4u_unmap(m4u_domain, mva, size);
	return -EINVAL;
}



int m4u_check_free_pte(struct m4u_domain_t *domain,
			struct imu_pgd_t *pgd)
{
	struct imu_pte_t *pte;
	int i;

	pte = imu_pte_map(pgd);
	for (i = 0; i < IMU_PTRS_PER_PTE; i++) {
		if (imu_pte_val(pte[i]) != 0)
			break;
	}
	if (i == IMU_PTRS_PER_PTE) {
		m4u_free_pte(domain, pgd);
		m4u_set_pgd_val(pgd, 0);
		return 0;
	} else {
		return 1;
	}
}

int m4u_unmap(struct m4u_domain_t *domain,
		unsigned int mva, unsigned int size)
{
	struct imu_pgd_t *pgd;
	int i, ret;
	unsigned int start = mva;
	unsigned long long tmp_mva = (unsigned long long) mva;
	unsigned long long end_plus_1 = tmp_mva + (unsigned long long)size;

	write_lock_domain(domain);
	while (tmp_mva < end_plus_1) {
		pgd = imu_pgd_offset(domain, tmp_mva);

		if (F_PGD_TYPE_IS_PAGE(*pgd)) {
			struct imu_pte_t *pte;
			unsigned long long pte_offset;
			unsigned long long num_to_clean;

			pte_offset = imu_pte_index(tmp_mva);
			num_to_clean =
			    min((unsigned long long)((end_plus_1 - mva) /
					PAGE_SIZE),
				(unsigned long long)(IMU_PTRS_PER_PTE -
					pte_offset));

			pte = imu_pte_offset_map(pgd, tmp_mva);

			memset(pte, 0, num_to_clean << 2);

			ret = m4u_check_free_pte(domain, pgd);
			/* pte is not freed, need to flush pte */
			if (ret == 1) {
				m4u_clean_pte(domain,
					mva, num_to_clean << PAGE_SHIFT);
			}

			tmp_mva += num_to_clean << PAGE_SHIFT;
		} else if (F_PGD_TYPE_IS_SECTION(*pgd)) {
			m4u_set_pgd_val(pgd, 0);
			tmp_mva += MMU_SECTION_SIZE;
		} else if (F_PGD_TYPE_IS_SUPERSECTION(*pgd)) {
			struct imu_pgd_t *start = imu_supersection_start(pgd);

			if (unlikely(start != pgd))
				m4u_aee_print
				    ("%s: not align, mva=0x%x, pgd=0x%x\n",
				     __func__, mva, imu_pgd_val(*pgd));

			for (i = 0; i < 16; i++)
				imu_pgd_val(start[i]) = 0;

			tmp_mva = (tmp_mva + MMU_SUPERSECTION_SIZE) &
				(~(MMU_SUPERSECTION_SIZE - 1));	/* must align */
		} else {
			tmp_mva += MMU_SECTION_SIZE;
		}
	}

	write_unlock_domain(domain);

	m4u_invalid_tlb_by_range(domain, start, end_plus_1 - 1);

	return 0;
}

int m4u_debug_pgtable_show(struct seq_file *s, void *unused)
{
	m4u_dump_pgtable(s->private, s);
	return 0;
}

int m4u_debug_pgtable_open(struct inode *inode, struct file *file)
{
	return single_open(file, m4u_debug_pgtable_show, inode->i_private);
}

const struct file_operations m4u_debug_pgtable_fops = {
	.open = m4u_debug_pgtable_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int m4u_pgtable_init(struct m4u_device *m4u_dev,
		struct m4u_domain_t *m4u_domain, int m4u_id)
{

	if (m4u_id < 0 || m4u_id >= TOTAL_M4U_NUM) {
		M4UMSG("%s: ERROR m4u_id:%d\n", __func__, m4u_id);
		return 1;
	}

	/* ======= alloc pagetable======================= */
	m4u_domain->pgd =
	    dma_alloc_coherent(m4u_dev->pDev[m4u_id],
			M4U_PGD_SIZE, &(m4u_domain->pgd_pa), GFP_KERNEL);

	if (!(m4u_domain->pgd)) {
		M4UMSG("dma_alloc_coherent err! dma memory not available\n");
		return -1;
	}
	if ((unsigned int)(m4u_domain->pgd_pa) & (M4U_PGD_SIZE - 1)) {
		M4UMSG("dma_alloc_coherent memory not align. 0x%lx\n",
		       (unsigned long)(m4u_domain->pgd_pa));
		return -1;
	}

	M4UINFO(
		"dma_alloc_coherent success! pagetable_va=0x%lx, pagetable_pa=0x%lx, domian:%d\n",
		(unsigned long)(m4u_domain->pgd),
		(unsigned long)(m4u_domain->pgd_pa),
		m4u_id);

	memset((void *)m4u_domain->pgd, 0, M4U_PGD_SIZE);
	/* ======= alloc pagetable done======================= */

	if (m4u_id == 0) {
		if (m4u_pte_allocator_init() != 0)
			return -1;
	}

	if (m4u_id == 0)
		debugfs_create_file("pgtable_mm", 0644,
			m4u_dev->debug_root, m4u_domain,
			&m4u_debug_pgtable_fops);
	else if (m4u_id == 1)
		debugfs_create_file("pgtable_vpu", 0644,
			m4u_dev->debug_root, m4u_domain,
			&m4u_debug_pgtable_fops);

	return 0;
}
