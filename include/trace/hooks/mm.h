/* SPDX-License-Identifier: GPL-2.0 */
#ifdef PROTECT_TRACE_INCLUDE_PATH
#undef PROTECT_TRACE_INCLUDE_PATH

#include <trace/hooks/save_incpath.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/restore_incpath.h>

#else /* PROTECT_TRACE_INCLUDE_PATH */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mm

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_MM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_MM_H

#include <trace/hooks/vendor_hooks.h>

#ifdef __GENKSYMS__
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/rwsem.h>
#include <../mm/slab.h>
#endif

struct oom_control;
struct slabinfo;
struct track;
struct address_space;
struct page_vma_mapped_walk;
struct cma;
struct compact_control;

DECLARE_RESTRICTED_HOOK(android_rvh_set_skip_swapcache_flags,
			TP_PROTO(gfp_t *flags),
			TP_ARGS(flags), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_set_gfp_zone_flags,
			TP_PROTO(gfp_t *flags),
			TP_ARGS(flags), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_update_readahead_gfp_mask,
			TP_PROTO(struct address_space *mapping, gfp_t *flags),
			TP_ARGS(mapping, flags), 2);
DECLARE_RESTRICTED_HOOK(android_rvh_set_readahead_gfp_mask,
			TP_PROTO(gfp_t *flags),
			TP_ARGS(flags), 1);
DECLARE_RESTRICTED_HOOK(android_rvh_rmqueue_bulk,
			TP_PROTO(void *unused),
			TP_ARGS(unused), 1);
DECLARE_HOOK(android_vh_meminfo_proc_show,
	TP_PROTO(struct seq_file *m),
	TP_ARGS(m));
DECLARE_HOOK(android_vh_exit_mm,
	TP_PROTO(struct mm_struct *mm),
	TP_ARGS(mm));
DECLARE_HOOK(android_vh_show_mem,
	TP_PROTO(unsigned int filter, nodemask_t *nodemask),
	TP_ARGS(filter, nodemask));
DECLARE_HOOK(android_vh_alloc_pages_slowpath,
	TP_PROTO(gfp_t gfp_mask, unsigned int order, unsigned long delta),
	TP_ARGS(gfp_mask, order, delta));
DECLARE_HOOK(android_vh_print_slabinfo_header,
	TP_PROTO(struct seq_file *m),
	TP_ARGS(m));
DECLARE_HOOK(android_vh_cache_show,
	TP_PROTO(struct seq_file *m, struct slabinfo *sinfo, struct kmem_cache *s),
	TP_ARGS(m, sinfo, s));
DECLARE_HOOK(android_vh_oom_check_panic,
	TP_PROTO(struct oom_control *oc, int *ret),
	TP_ARGS(oc, ret));
DECLARE_HOOK(android_vh_drain_all_pages_bypass,
	TP_PROTO(gfp_t gfp_mask, unsigned int order, unsigned long alloc_flags,
		int migratetype, unsigned long did_some_progress,
		bool *bypass),
	TP_ARGS(gfp_mask, order, alloc_flags, migratetype, did_some_progress, bypass));
DECLARE_HOOK(android_vh_dm_bufio_shrink_scan_bypass,
	TP_PROTO(unsigned long dm_bufio_current_allocated, bool *bypass),
	TP_ARGS(dm_bufio_current_allocated, bypass));
DECLARE_HOOK(android_vh_cleanup_old_buffers_bypass,
	TP_PROTO(unsigned long dm_bufio_current_allocated,
		unsigned long *max_age_hz,
		bool *bypass),
	TP_ARGS(dm_bufio_current_allocated, max_age_hz, bypass));
DECLARE_HOOK(android_vh_cma_drain_all_pages_bypass,
	TP_PROTO(unsigned int migratetype, bool *bypass),
	TP_ARGS(migratetype, bypass));
DECLARE_HOOK(android_vh_pcplist_add_cma_pages_bypass,
	TP_PROTO(int migratetype, bool *bypass),
	TP_ARGS(migratetype, bypass));
DECLARE_HOOK(android_vh_slab_alloc_node,
	TP_PROTO(void *object, unsigned long addr, struct kmem_cache *s),
	TP_ARGS(object, addr, s));
DECLARE_HOOK(android_vh_slab_free,
	TP_PROTO(unsigned long addr, struct kmem_cache *s),
	TP_ARGS(addr, s));
DECLARE_HOOK(android_vh_free_unref_page_bypass,
	TP_PROTO(struct page *page, int order, int migratetype, bool *bypass),
	TP_ARGS(page, order, migratetype, bypass));
DECLARE_HOOK(android_vh_kvmalloc_node_use_vmalloc,
	TP_PROTO(size_t size, gfp_t *kmalloc_flags, bool *use_vmalloc),
	TP_ARGS(size, kmalloc_flags, use_vmalloc));
DECLARE_HOOK(android_vh_should_alloc_pages_retry,
	TP_PROTO(gfp_t gfp_mask, int order, int *alloc_flags,
	int migratetype, struct zone *preferred_zone, struct page **page, bool *should_alloc_retry),
	TP_ARGS(gfp_mask, order, alloc_flags,
		migratetype, preferred_zone, page, should_alloc_retry));
DECLARE_HOOK(android_vh_unreserve_highatomic_bypass,
	TP_PROTO(bool force, struct zone *zone, bool *skip_unreserve_highatomic),
	TP_ARGS(force, zone, skip_unreserve_highatomic));
DECLARE_HOOK(android_vh_rmqueue_bulk_bypass,
	TP_PROTO(unsigned int order, struct per_cpu_pages *pcp, int migratetype,
		struct list_head *list),
	TP_ARGS(order, pcp, migratetype, list));
DECLARE_HOOK(android_vh_ra_tuning_max_page,
	TP_PROTO(struct readahead_control *ractl, unsigned long *max_page),
	TP_ARGS(ractl, max_page));
DECLARE_HOOK(android_vh_tune_mmap_readaround,
	TP_PROTO(unsigned int ra_pages, pgoff_t pgoff,
		pgoff_t *start, unsigned int *size, unsigned int *async_size),
	TP_ARGS(ra_pages, pgoff, start, size, async_size));
DECLARE_HOOK(android_vh_mmap_region,
	TP_PROTO(struct vm_area_struct *vma, unsigned long addr),
	TP_ARGS(vma, addr));
DECLARE_HOOK(android_vh_try_to_unmap_one,
	TP_PROTO(struct vm_area_struct *vma, struct page *page, unsigned long addr, bool ret),
	TP_ARGS(vma, page, addr, ret));
DECLARE_HOOK(android_vh_cma_alloc_retry,
	TP_PROTO(char *name, int *retry),
	TP_ARGS(name, retry));
struct mem_cgroup;
DECLARE_HOOK(android_vh_mem_cgroup_alloc,
	TP_PROTO(struct mem_cgroup *memcg),
	TP_ARGS(memcg));
DECLARE_HOOK(android_vh_mem_cgroup_free,
	TP_PROTO(struct mem_cgroup *memcg),
	TP_ARGS(memcg));
DECLARE_HOOK(android_vh_mem_cgroup_id_remove,
	TP_PROTO(struct mem_cgroup *memcg),
	TP_ARGS(memcg));
struct cgroup_subsys_state;
DECLARE_HOOK(android_vh_mem_cgroup_css_online,
	TP_PROTO(struct cgroup_subsys_state *css, struct mem_cgroup *memcg),
	TP_ARGS(css, memcg));
DECLARE_HOOK(android_vh_mem_cgroup_css_offline,
	TP_PROTO(struct cgroup_subsys_state *css, struct mem_cgroup *memcg),
	TP_ARGS(css, memcg));
DECLARE_HOOK(android_vh_vmpressure,
	TP_PROTO(struct mem_cgroup *memcg, bool *bypass),
	TP_ARGS(memcg, bypass));
DECLARE_HOOK(android_vh_do_page_trylock,
	TP_PROTO(struct page *page, struct rw_semaphore *sem,
		bool *got_lock, bool *success),
	TP_ARGS(page, sem, got_lock, success));
DECLARE_HOOK(android_vh_update_page_mapcount,
	TP_PROTO(struct page *page, bool inc_size, bool compound,
			bool *first_mapping, bool *success),
	TP_ARGS(page, inc_size, compound, first_mapping, success));
DECLARE_HOOK(android_vh_add_page_to_lrulist,
	TP_PROTO(struct page *page, bool compound, enum lru_list lru),
	TP_ARGS(page, compound, lru));
DECLARE_HOOK(android_vh_del_page_from_lrulist,
	TP_PROTO(struct page *page, bool compound, enum lru_list lru),
	TP_ARGS(page, compound, lru));
DECLARE_HOOK(android_vh_show_mapcount_pages,
	TP_PROTO(void *unused),
	TP_ARGS(unused));
DECLARE_HOOK(android_vh_do_traversal_lruvec,
	TP_PROTO(struct lruvec *lruvec),
	TP_ARGS(lruvec));
DECLARE_HOOK(android_vh_page_should_be_protected,
	TP_PROTO(struct page *page, bool *should_protect),
	TP_ARGS(page, should_protect));
DECLARE_HOOK(android_vh_mark_page_accessed,
	TP_PROTO(struct page *page),
	TP_ARGS(page));
DECLARE_HOOK(android_vh_page_cache_forced_ra,
	TP_PROTO(struct readahead_control *ractl, unsigned long req_count, bool *do_forced_ra),
	TP_ARGS(ractl, req_count, do_forced_ra));
DECLARE_HOOK(android_vh_alloc_pages_reclaim_bypass,
	TP_PROTO(gfp_t gfp_mask, int order, int alloc_flags,
	int migratetype, struct page **page),
	TP_ARGS(gfp_mask, order, alloc_flags, migratetype, page));
DECLARE_HOOK(android_vh_alloc_pages_failure_bypass,
	TP_PROTO(gfp_t gfp_mask, int order, int alloc_flags,
	int migratetype, struct page **page),
	TP_ARGS(gfp_mask, order, alloc_flags, migratetype, page));
DECLARE_HOOK(android_vh_save_track_hash,
	TP_PROTO(bool alloc, struct track *p),
	TP_ARGS(alloc, p));
DECLARE_HOOK(android_vh_rmqueue,
	TP_PROTO(struct zone *preferred_zone, struct zone *zone,
		unsigned int order, gfp_t gfp_flags,
		unsigned int alloc_flags, int migratetype),
	TP_ARGS(preferred_zone, zone, order,
		gfp_flags, alloc_flags, migratetype));
DECLARE_HOOK(android_vh_kmalloc_slab,
	TP_PROTO(unsigned int index, gfp_t flags, struct kmem_cache **s),
	TP_ARGS(index, flags, s));
DECLARE_HOOK(android_vh_madvise_cold_or_pageout,
	TP_PROTO(struct vm_area_struct *vma, bool *allow_shared),
	TP_ARGS(vma, allow_shared));
DECLARE_RESTRICTED_HOOK(android_rvh_ctl_dirty_rate,
	TP_PROTO(void *unused),
	TP_ARGS(unused), 1);
DECLARE_HOOK(android_vh_rmqueue_smallest_bypass,
	TP_PROTO(struct page **page, struct zone *zone, int order, int migratetype),
	TP_ARGS(page, zone, order, migratetype));
DECLARE_HOOK(android_vh_free_one_page_bypass,
	TP_PROTO(struct page *page, struct zone *zone, int order, int migratetype,
		int fpi_flags, bool *bypass),
	TP_ARGS(page, zone, order, migratetype, fpi_flags, bypass));
DECLARE_HOOK(android_vh_use_cma_first_check,
	TP_PROTO(bool *use_cma_first_check),
	TP_ARGS(use_cma_first_check));
DECLARE_HOOK(android_vh_alloc_highpage_movable_gfp_adjust,
        TP_PROTO(gfp_t *gfp_mask),
        TP_ARGS(gfp_mask));
DECLARE_HOOK(android_vh_anon_gfp_adjust,
	TP_PROTO(gfp_t *gfp_mask),
	TP_ARGS(gfp_mask));
DECLARE_HOOK(android_vh_slab_page_alloced,
	TP_PROTO(struct page *page, size_t size, gfp_t flags),
	TP_ARGS(page, size, flags));
DECLARE_HOOK(android_vh_kmalloc_order_alloced,
        TP_PROTO(struct page *page, size_t size, gfp_t flags),
        TP_ARGS(page, size, flags));
DECLARE_HOOK(android_vh_compact_finished,
	TP_PROTO(bool *abort_compact),
	TP_ARGS(abort_compact));
DECLARE_HOOK(android_vh_madvise_cold_or_pageout_abort,
	TP_PROTO(struct vm_area_struct *vma, bool *abort_madvise),
	TP_ARGS(vma, abort_madvise));
DECLARE_HOOK(android_vh_alloc_flags_cma_adjust,
	TP_PROTO(gfp_t gfp_mask, unsigned int *alloc_flags),
	TP_ARGS(gfp_mask, alloc_flags));
DECLARE_HOOK(android_vh_rmqueue_cma_fallback,
	TP_PROTO(struct zone *zone, unsigned int order, struct page **page),
	TP_ARGS(zone, order, page));
DECLARE_HOOK(android_vh_test_clear_look_around_ref,
	TP_PROTO(struct page *page),
	TP_ARGS(page));
DECLARE_HOOK(android_vh_look_around_migrate_page,
	TP_PROTO(struct page *old_page, struct page *new_page),
	TP_ARGS(old_page, new_page));
DECLARE_HOOK(android_vh_look_around,
	TP_PROTO(struct page_vma_mapped_walk *pvmw, struct page *page,
		struct vm_area_struct *vma, int *referenced),
	TP_ARGS(pvmw, page, vma, referenced));
DECLARE_HOOK(android_vh_try_cma_fallback,
	TP_PROTO(struct zone *zone, unsigned int order, bool *try_cma),
	TP_ARGS(zone, order, try_cma));
DECLARE_HOOK(android_vh_set_page_migrating,
	TP_PROTO(struct page *page),
	TP_ARGS(page));
DECLARE_HOOK(android_vh_clear_page_migrating,
	TP_PROTO(struct page *page),
	TP_ARGS(page));
DECLARE_HOOK(android_vh_cma_alloc_bypass,
	TP_PROTO(struct cma *cma, unsigned long count, unsigned int align,
		bool no_warn, struct page **page, bool *bypass),
	TP_ARGS(cma, count, align, no_warn, page, bypass));
DECLARE_HOOK(android_vh_alloc_pages_entry,
	TP_PROTO(gfp_t *gfp, unsigned int order, int preferred_nid,
		nodemask_t *nodemask),
	TP_ARGS(gfp, order, preferred_nid, nodemask));
DECLARE_HOOK(android_vh_isolate_freepages,
	TP_PROTO(struct compact_control *cc, struct page *page, bool *bypass),
	TP_ARGS(cc, page, bypass));
DECLARE_HOOK(android_vh_should_fault_around,
	TP_PROTO(struct vm_fault *vmf, bool *should_around),
	TP_ARGS(vmf, should_around));
DECLARE_HOOK(android_vh_do_read_fault,
	TP_PROTO(struct vm_fault *vmf, unsigned long fault_around_bytes),
	TP_ARGS(vmf, fault_around_bytes));
DECLARE_HOOK(android_vh_filemap_read,
	TP_PROTO(struct file *file, loff_t pos, size_t size),
	TP_ARGS(file, pos, size));
DECLARE_HOOK(android_vh_filemap_map_pages,
	TP_PROTO(struct file *file, pgoff_t first_pgoff,
		pgoff_t last_pgoff, vm_fault_t ret),
	TP_ARGS(file, first_pgoff, last_pgoff, ret));
DECLARE_HOOK(android_vh_do_swap_page_spf,
	TP_PROTO(bool *allow_swap_spf),
	TP_ARGS(allow_swap_spf));
DECLARE_HOOK(android_vh_tune_fault_around_bytes,
	TP_PROTO(unsigned long *fault_around_bytes),
	TP_ARGS(fault_around_bytes));
DECLARE_HOOK(android_vh_swapmem_gather_init,
	TP_PROTO(struct mm_struct *mm),
	TP_ARGS(mm));
DECLARE_HOOK(android_vh_swapmem_gather_add_bypass,
	TP_PROTO(struct mm_struct *mm, swp_entry_t entry, bool *bypass),
	TP_ARGS(mm, entry, bypass));
DECLARE_HOOK(android_vh_swapmem_gather_finish,
	TP_PROTO(struct mm_struct *mm),
	TP_ARGS(mm));
DECLARE_HOOK(android_vh_oom_swapmem_gather_init,
	TP_PROTO(struct mm_struct *mm),
	TP_ARGS(mm));
DECLARE_HOOK(android_vh_oom_swapmem_gather_finish,
	TP_PROTO(struct mm_struct *mm),
	TP_ARGS(mm));
DECLARE_HOOK(android_vh_do_anonymous_page,
	TP_PROTO(struct vm_area_struct *vma, struct page *page),
	TP_ARGS(vma, page));
DECLARE_HOOK(android_vh_do_swap_page,
	TP_PROTO(struct page *page, pte_t *pte, struct vm_fault *vmf,
		swp_entry_t entry),
	TP_ARGS(page, pte, vmf, entry));
DECLARE_HOOK(android_vh_do_wp_page,
	TP_PROTO(struct page *page),
	TP_ARGS(page));
DECLARE_HOOK(android_vh_uprobes_replace_page,
	TP_PROTO(struct page *new_page, struct page *old_page),
	TP_ARGS(new_page, old_page));
DECLARE_HOOK(android_vh_shmem_swapin_page,
	TP_PROTO(struct page *page),
	TP_ARGS(page));
DECLARE_HOOK(android_vh_should_end_madvise,
	TP_PROTO(struct mm_struct *mm, bool *skip, bool *pageout),
	TP_ARGS(mm, skip, pageout));
DECLARE_HOOK(android_vh_io_statistics,
	TP_PROTO(struct address_space *mapping, unsigned int index,
			unsigned int nr_page, bool read, bool direct),
	TP_ARGS(mapping, index, nr_page, read, direct));
DECLARE_HOOK(android_vh_page_cache_miss,
	TP_PROTO(struct file *file,
		pgoff_t start, pgoff_t len,
		pgoff_t index, bool buffer),
	TP_ARGS(file, start, len, index, buffer));
#endif /* _TRACE_HOOK_MM_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

#endif /* PROTECT_TRACE_INCLUDE_PATH */
