/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LINUX_MM_INLINE_H
#define LINUX_MM_INLINE_H

#include <linux/huge_mm.h>
#include <linux/swap.h>
#include <linux/string.h>
#ifndef __GENKSYMS__
#define PROTECT_TRACE_INCLUDE_PATH
#include <trace/hooks/mm.h>
#endif

/**
 * page_is_file_lru - should the page be on a file LRU or anon LRU?
 * @page: the page to test
 *
 * Returns 1 if @page is a regular filesystem backed page cache page or a lazily
 * freed anonymous page (e.g. via MADV_FREE).  Returns 0 if @page is a normal
 * anonymous page, a tmpfs page or otherwise ram or swap backed page.  Used by
 * functions that manipulate the LRU lists, to sort a page onto the right LRU
 * list.
 *
 * We would like to get this info without a page flag, but the state
 * needs to survive until the page is last deleted from the LRU, which
 * could be as far down as __page_cache_release.
 */
static inline int page_is_file_lru(struct page *page)
{
	return !PageSwapBacked(page);
}

static __always_inline void __update_lru_size(struct lruvec *lruvec,
				enum lru_list lru, enum zone_type zid,
				long nr_pages)
{
	struct pglist_data *pgdat = lruvec_pgdat(lruvec);

	lockdep_assert_held(&lruvec->lru_lock);
	WARN_ON_ONCE(nr_pages != (int)nr_pages);

	__mod_lruvec_state(lruvec, NR_LRU_BASE + lru, nr_pages);
	__mod_zone_page_state(&pgdat->node_zones[zid],
				NR_ZONE_LRU_BASE + lru, nr_pages);
}

static __always_inline void update_lru_size(struct lruvec *lruvec,
				enum lru_list lru, enum zone_type zid,
				long nr_pages)
{
	__update_lru_size(lruvec, lru, zid, nr_pages);
#ifdef CONFIG_MEMCG
	mem_cgroup_update_lru_size(lruvec, lru, zid, nr_pages);
#endif
}

/**
 * __clear_page_lru_flags - clear page lru flags before releasing a page
 * @page: the page that was on lru and now has a zero reference
 */
static __always_inline void __clear_page_lru_flags(struct page *page)
{
	VM_BUG_ON_PAGE(!PageLRU(page), page);

	__ClearPageLRU(page);

	/* this shouldn't happen, so leave the flags to bad_page() */
	if (PageActive(page) && PageUnevictable(page))
		return;

	__ClearPageActive(page);
	__ClearPageUnevictable(page);
}

/**
 * page_lru - which LRU list should a page be on?
 * @page: the page to test
 *
 * Returns the LRU list a page should be on, as an index
 * into the array of LRU lists.
 */
static __always_inline enum lru_list page_lru(struct page *page)
{
	enum lru_list lru;

	VM_BUG_ON_PAGE(PageActive(page) && PageUnevictable(page), page);

	if (PageUnevictable(page))
		return LRU_UNEVICTABLE;

	lru = page_is_file_lru(page) ? LRU_INACTIVE_FILE : LRU_INACTIVE_ANON;
	if (PageActive(page))
		lru += LRU_ACTIVE;

	return lru;
}

#ifdef CONFIG_LRU_GEN

#ifdef CONFIG_LRU_GEN_ENABLED
static inline bool lru_gen_enabled(void)
{
	DECLARE_STATIC_KEY_TRUE(lru_gen_caps[NR_LRU_GEN_CAPS]);

	return static_branch_likely(&lru_gen_caps[LRU_GEN_CORE]);
}
#else
static inline bool lru_gen_enabled(void)
{
	DECLARE_STATIC_KEY_FALSE(lru_gen_caps[NR_LRU_GEN_CAPS]);

	return static_branch_unlikely(&lru_gen_caps[LRU_GEN_CORE]);
}
#endif

static inline bool lru_gen_in_fault(void)
{
	return current->in_lru_fault;
}

static inline int lru_gen_from_seq(unsigned long seq)
{
	return seq % MAX_NR_GENS;
}

static inline int lru_hist_from_seq(unsigned long seq)
{
	return seq % NR_HIST_GENS;
}

static inline int lru_tier_from_refs(int refs)
{
	VM_WARN_ON_ONCE(refs > BIT(LRU_REFS_WIDTH));

	/* see the comment in page_lru_refs() */
	return order_base_2(refs + 1);
}

static inline int page_lru_refs(struct page *page)
{
	unsigned long flags = READ_ONCE(page->flags);
	bool workingset = flags & BIT(PG_workingset);

	/*
	 * Return the number of accesses beyond PG_referenced, i.e., N-1 if the
	 * total number of accesses is N>1, since N=0,1 both map to the first
	 * tier. lru_tier_from_refs() will account for this off-by-one. Also see
	 * the comment on MAX_NR_TIERS.
	 */
	return ((flags & LRU_REFS_MASK) >> LRU_REFS_PGOFF) + workingset;
}

static inline int page_lru_gen(struct page *page)
{
	unsigned long flags = READ_ONCE(page->flags);

	return ((flags & LRU_GEN_MASK) >> LRU_GEN_PGOFF) - 1;
}

static inline bool lru_gen_is_active(struct lruvec *lruvec, int gen)
{
	unsigned long max_seq = lruvec->lrugen.max_seq;

	VM_WARN_ON_ONCE(gen >= MAX_NR_GENS);

	/* see the comment on MIN_NR_GENS */
	return gen == lru_gen_from_seq(max_seq) || gen == lru_gen_from_seq(max_seq - 1);
}

static inline void lru_gen_update_size(struct lruvec *lruvec, struct page *page,
				       int old_gen, int new_gen)
{
	int type = page_is_file_lru(page);
	int zone = page_zonenum(page);
	int delta = thp_nr_pages(page);
	enum lru_list lru = type * LRU_INACTIVE_FILE;
	struct lru_gen_struct *lrugen = &lruvec->lrugen;

	VM_WARN_ON_ONCE(old_gen != -1 && old_gen >= MAX_NR_GENS);
	VM_WARN_ON_ONCE(new_gen != -1 && new_gen >= MAX_NR_GENS);
	VM_WARN_ON_ONCE(old_gen == -1 && new_gen == -1);

	if (old_gen >= 0)
		WRITE_ONCE(lrugen->nr_pages[old_gen][type][zone],
			   lrugen->nr_pages[old_gen][type][zone] - delta);
	if (new_gen >= 0)
		WRITE_ONCE(lrugen->nr_pages[new_gen][type][zone],
			   lrugen->nr_pages[new_gen][type][zone] + delta);

	/* addition */
	if (old_gen < 0) {
		if (lru_gen_is_active(lruvec, new_gen))
			lru += LRU_ACTIVE;
		__update_lru_size(lruvec, lru, zone, delta);
		return;
	}

	/* deletion */
	if (new_gen < 0) {
		if (lru_gen_is_active(lruvec, old_gen))
			lru += LRU_ACTIVE;
		__update_lru_size(lruvec, lru, zone, -delta);
		return;
	}

	/* promotion */
	if (!lru_gen_is_active(lruvec, old_gen) && lru_gen_is_active(lruvec, new_gen)) {
		__update_lru_size(lruvec, lru, zone, -delta);
		__update_lru_size(lruvec, lru + LRU_ACTIVE, zone, delta);
	}

	/* demotion requires isolation, e.g., lru_deactivate_fn() */
	VM_WARN_ON_ONCE(lru_gen_is_active(lruvec, old_gen) && !lru_gen_is_active(lruvec, new_gen));
}

static inline bool lru_gen_add_page(struct lruvec *lruvec, struct page *page, bool reclaiming)
{
	unsigned long seq;
	unsigned long flags;
	int gen = page_lru_gen(page);
	int type = page_is_file_lru(page);
	int zone = page_zonenum(page);
	struct lru_gen_struct *lrugen = &lruvec->lrugen;

	VM_WARN_ON_ONCE_PAGE(gen != -1, page);

	if (PageUnevictable(page) || !lrugen->enabled)
		return false;
	/*
	 * There are three common cases for this page:
	 * 1. If it's hot, e.g., freshly faulted in or previously hot and
	 *    migrated, add it to the youngest generation.
	 * 2. If it's cold but can't be evicted immediately, i.e., an anon page
	 *    not in swapcache or a dirty page pending writeback, add it to the
	 *    second oldest generation.
	 * 3. Everything else (clean, cold) is added to the oldest generation.
	 */
	if (PageActive(page))
		seq = lrugen->max_seq;
	else if ((type == LRU_GEN_ANON && !PageSwapCache(page)) ||
		 (PageReclaim(page) &&
		  (PageDirty(page) || PageWriteback(page))))
		seq = lrugen->min_seq[type] + 1;
	else
		seq = lrugen->min_seq[type];

	gen = lru_gen_from_seq(seq);
	flags = (gen + 1UL) << LRU_GEN_PGOFF;
	/* see the comment on MIN_NR_GENS about PG_active */
	set_mask_bits(&page->flags, LRU_GEN_MASK | BIT(PG_active), flags);

	lru_gen_update_size(lruvec, page, -1, gen);
	/* for rotate_reclaimable_page() */
	if (reclaiming)
		list_add_tail(&page->lru, &lrugen->lists[gen][type][zone]);
	else
		list_add(&page->lru, &lrugen->lists[gen][type][zone]);

	return true;
}

static inline bool lru_gen_del_page(struct lruvec *lruvec, struct page *page, bool reclaiming)
{
	unsigned long flags;
	int gen = page_lru_gen(page);

	if (gen < 0)
		return false;

	VM_WARN_ON_ONCE_PAGE(PageActive(page), page);
	VM_WARN_ON_ONCE_PAGE(PageUnevictable(page), page);

	/* for migrate_page_states() */
	flags = !reclaiming && lru_gen_is_active(lruvec, gen) ? BIT(PG_active) : 0;
	flags = set_mask_bits(&page->flags, LRU_GEN_MASK, flags);
	gen = ((flags & LRU_GEN_MASK) >> LRU_GEN_PGOFF) - 1;

	lru_gen_update_size(lruvec, page, gen, -1);
	list_del(&page->lru);

	return true;
}

#else /* !CONFIG_LRU_GEN */

static inline bool lru_gen_enabled(void)
{
	return false;
}

static inline bool lru_gen_in_fault(void)
{
	return false;
}

static inline bool lru_gen_add_page(struct lruvec *lruvec, struct page *page, bool reclaiming)
{
	return false;
}

static inline bool lru_gen_del_page(struct lruvec *lruvec, struct page *page, bool reclaiming)
{
	return false;
}

#endif /* CONFIG_LRU_GEN */

static __always_inline void add_page_to_lru_list(struct page *page,
				struct lruvec *lruvec)
{
	enum lru_list lru = page_lru(page);

	if (lru_gen_add_page(lruvec, page, false))
		return;

	trace_android_vh_add_page_to_lrulist(page, false, lru);
	update_lru_size(lruvec, lru, page_zonenum(page), thp_nr_pages(page));
	list_add(&page->lru, &lruvec->lists[lru]);
}

static __always_inline void add_page_to_lru_list_tail(struct page *page,
				struct lruvec *lruvec)
{
	enum lru_list lru = page_lru(page);

	if (lru_gen_add_page(lruvec, page, true))
		return;

	trace_android_vh_add_page_to_lrulist(page, false, page_lru(page));
	update_lru_size(lruvec, lru, page_zonenum(page), thp_nr_pages(page));
	list_add_tail(&page->lru, &lruvec->lists[lru]);
}

static __always_inline void del_page_from_lru_list(struct page *page,
				struct lruvec *lruvec)
{
	if (lru_gen_del_page(lruvec, page, false))
		return;

	trace_android_vh_del_page_from_lrulist(page, false, page_lru(page));
	list_del(&page->lru);
	update_lru_size(lruvec, page_lru(page), page_zonenum(page),
			-thp_nr_pages(page));
}

#ifdef CONFIG_ANON_VMA_NAME
/*
 * mmap_lock should be read-locked when calling anon_vma_name(). Caller should
 * either keep holding the lock while using the returned pointer or it should
 * raise anon_vma_name refcount before releasing the lock.
 */
extern struct anon_vma_name *anon_vma_name(struct vm_area_struct *vma);
extern struct anon_vma_name *anon_vma_name_alloc(const char *name);
extern void anon_vma_name_free(struct kref *kref);

/* mmap_lock should be read-locked */
static inline void anon_vma_name_get(struct anon_vma_name *anon_name)
{
	if (anon_name)
		kref_get(&anon_name->kref);
}

static inline void anon_vma_name_put(struct anon_vma_name *anon_name)
{
	if (anon_name)
		kref_put(&anon_name->kref, anon_vma_name_free);
}

static inline
struct anon_vma_name *anon_vma_name_reuse(struct anon_vma_name *anon_name)
{
	/* Prevent anon_name refcount saturation early on */
	if (kref_read(&anon_name->kref) < REFCOUNT_MAX) {
		anon_vma_name_get(anon_name);
		return anon_name;

	}
	return anon_vma_name_alloc(anon_name->name);
}

static inline void dup_anon_vma_name(struct vm_area_struct *orig_vma,
				     struct vm_area_struct *new_vma)
{
	struct anon_vma_name *anon_name = anon_vma_name(orig_vma);

	if (anon_name)
		new_vma->anon_name = anon_vma_name_reuse(anon_name);
}

static inline void free_anon_vma_name(struct vm_area_struct *vma)
{
	/*
	 * Not using anon_vma_name because it generates a warning if mmap_lock
	 * is not held, which might be the case here.
	 */
	if (!vma->vm_file)
		anon_vma_name_put(vma->anon_name);
}

static inline bool anon_vma_name_eq(struct anon_vma_name *anon_name1,
				    struct anon_vma_name *anon_name2)
{
	if (anon_name1 == anon_name2)
		return true;

	return anon_name1 && anon_name2 &&
		!strcmp(anon_name1->name, anon_name2->name);
}
#else /* CONFIG_ANON_VMA_NAME */
static inline struct anon_vma_name *anon_vma_name(struct vm_area_struct *vma)
{
	return NULL;
}

static inline struct anon_vma_name *anon_vma_name_alloc(const char *name)
{
	return NULL;
}

static inline void anon_vma_name_get(struct anon_vma_name *anon_name) {}
static inline void anon_vma_name_put(struct anon_vma_name *anon_name) {}
static inline void dup_anon_vma_name(struct vm_area_struct *orig_vma,
				     struct vm_area_struct *new_vma) {}
static inline void free_anon_vma_name(struct vm_area_struct *vma) {}

static inline bool anon_vma_name_eq(struct anon_vma_name *anon_name1,
				    struct anon_vma_name *anon_name2)
{
	return true;
}

#endif  /* CONFIG_ANON_VMA_NAME */

#endif
