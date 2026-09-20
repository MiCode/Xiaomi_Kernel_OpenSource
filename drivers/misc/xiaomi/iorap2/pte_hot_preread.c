#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/rbtree.h>
#include <linux/wait.h>
#include <linux/rwlock.h>
#include <linux/atomic.h>
#include <linux/cred.h>
#include <linux/fadvise.h>
#include <linux/module.h>
#include <linux/mman.h>
#include <linux/pagewalk.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/swapfile.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/sched.h>
#include <trace/events/task.h>
#include "../../../../mm/swap.h"
#include <linux/file.h>
#include <linux/pgtable.h>

#include "mi_lb_def.h"
#include "pte_hot_preread.h"
#include "launch_boost_debug.h"

#define bitmap_for_each_set_region(bitmap, rs, re, start, end)		     \
	for ((rs) = (start),						     \
	     bitmap_next_set_region((bitmap), &(rs), &(re), (end));	     \
	     (rs) < (re);						     \
	     (rs) = (re) + 1,						     \
	     bitmap_next_set_region((bitmap), &(rs), &(re), (end)))

struct lb_hot_mps g_hot_mps;

static inline void mps_list_lock(struct lb_hot_mps *mps)
{
	spin_lock(&mps->list_lock);
}

static inline void mps_list_unlock(struct lb_hot_mps *mps)
{
	spin_unlock(&mps->list_lock);
}

static inline struct pte_segment *mps_alloc_seg(struct lb_hot_mps *mps, gfp_t gfp)
{
	struct pte_segment *seg;

	if (atomic_read(&mps->nr_alloc_segs) >= g_max_pte_segment_count)
		return NULL;
	seg = kmem_cache_zalloc(mps->seg_cachep, gfp);
	if (seg)
		atomic_inc(&mps->nr_alloc_segs);
	return seg;
}

static inline void mps_free_seg(struct lb_hot_mps *mps, struct pte_segment *seg)
{
	atomic_dec(&mps->nr_alloc_segs);
	kmem_cache_free(mps->seg_cachep, seg);
}

static struct pte_segment *mps_rec_load_seg(struct pte_record *mps_rec,
					    ulong seg_index, bool create, int class)
{
	struct pte_segment *seg;
	int ret;

	seg = xa_load(&mps_rec->rec_segs[class], seg_index);
	if (unlikely(!seg) && create) {
		struct lb_hot_mps *mps = mps_rec->rec_mps;
		seg = mps_alloc_seg(mps, GFP_NOWAIT);
		if (!seg) {
			MI_LB_ERR("alloc seg failed");
			return NULL;
		}
		ret = xa_insert(&mps_rec->rec_segs[class], seg_index, seg, GFP_NOWAIT);
		if (ret) {
			mps_free_seg(mps, seg);
			MI_LB_ERR("insert seg failed");
			return NULL;
		}
		mps_rec->nr_segments++;
	}

	mps_rec->last_seg = seg;
	mps_rec->last_seg_index = seg_index;
	mps_rec->last_seg_class = class;
	return seg;
}

static inline int mps_rec_set_hot(struct pte_record *mps_rec, ulong page_index, int class)
{
	ulong seg_index = MPS_SEG_INDEX(page_index);
	ulong seg_offset = MPS_SEG_OFFSET(page_index);
	struct pte_segment *seg = mps_rec->last_seg;

	if (unlikely(seg_index != mps_rec->last_seg_index || class != mps_rec->last_seg_class || !seg)) {
		seg = mps_rec_load_seg(mps_rec, seg_index, true, class);
		if (!seg)
			return -ENOMEM;
	}

	bitmap_set(seg->pagemap, seg_offset, 1);
	return 0;
}

static int mps_rec_free(struct pte_record *mps_rec)
{
	struct lb_hot_mps *mps = mps_rec->rec_mps;
	unsigned long idx;
	struct pte_segment *seg;
	int i = PTE_ANON_PAGE;

	mmdrop(mps_rec->rec_mm);

	for (; i < PTE_PAGE_COUNT; i++) {
		xa_for_each(&mps_rec->rec_segs[i], idx, seg) {
			mps_free_seg(mps, seg);

			xa_erase(&mps_rec->rec_segs[i], idx);
		}
	}

	kfree(mps_rec);
	atomic_dec(&mps->nr_alloc_mps_recs);

	return 0;
}


static struct pte_record *mps_get_record(struct lb_hot_mps *mps,
					 struct mm_struct *mm)
{
	struct pte_record *mps_rec;
	long key = (long)mm;

	mps_list_lock(mps);
	mps_rec = xa_load(&mps->rec_tree, key);
	if (mps_rec && (mps_rec->rec_mm != mm || !atomic_inc_not_zero(&mps_rec->rec_users)))
		mps_rec = NULL;

	mps_list_unlock(mps);
	return mps_rec;
}

static inline void mps_rec_get(struct pte_record *mps_rec)
{
	atomic_inc(&mps_rec->rec_users);
}

static void mps_delayed_free_rec(struct rcu_head *rhp)
{
	struct pte_record *mps_rec;

	mps_rec = container_of(rhp, struct pte_record, rec_rcu);
	mps_rec_free(mps_rec);
}

static void mps_rec_do_put(struct pte_record *mps_rec)
{
	call_rcu(&mps_rec->rec_rcu, mps_delayed_free_rec);
}

static inline bool mps_rec_put(struct pte_record *mps_rec)
{

	if (atomic_dec_and_test(&mps_rec->rec_users)) {
		mps_rec_do_put(mps_rec);
		return true;
	}
	return false;
}

static void mps_unlink_rec_locked(struct lb_hot_mps *mps, struct mm_struct *mm)
{
	struct pte_record *mps_rec;
	long key = (long)mm;

	mps_rec = xa_load(&mps->rec_tree, key);
	if (!mps_rec || mps_rec->rec_mm != mm) {
		/* This may print too much log because frequently mmput.
		 * MI_LB_ERR("not found pte_record or found another pte_record: %d", !mps_rec); */
		return;
	}
	if (1 != atomic_read(&mps_rec->rec_users))
		return;

	mps->total_pre_anon += mps_rec->nr_pre_anon;
	mps->total_pre_file += mps_rec->nr_pre_file;
	mps->nr_snapshot--;
	list_del(&mps_rec->rec_node);
	xa_erase(&mps->rec_tree, key);
	mps_rec_put(mps_rec); /* rec_users - 1 == 0 and free mps_rec */
}

static void mps_unlink_rec(struct lb_hot_mps *mps, struct mm_struct *mm)
{
	mps_list_lock(mps);
	mps_unlink_rec_locked(mps, mm);
	mps_list_unlock(mps);
}

static bool mps_link_rec(struct lb_hot_mps *mps, struct pte_record *mps_rec)
{
	int ret;
	bool suc = false;
	long key = (long)mps_rec->rec_mm;

	mps_list_lock(mps_rec->rec_mps);
	/* free apps more than g_pte_max_active_apps */
	if (mps->nr_snapshot > g_pte_max_active_apps - 1) {
		struct pte_record *to_free = list_entry(mps->rec_list.prev, struct pte_record, rec_node);

		MI_LB_DBG("shrink app, uid: %u, pid: %u", to_free->uid, to_free->pid);
		mps_unlink_rec_locked(mps, to_free->rec_mm);
	}
	ret = xa_insert(&mps->rec_tree, key, mps_rec, GFP_NOWAIT);
	if (ret) {
		MI_LB_ERR("insert mps_rec encounter: %d", ret);
		goto out;
	}
	mps_rec_get(mps_rec); //rec_users +1 = 2
	list_add(&mps_rec->rec_node, &mps->rec_list);
	mps->nr_snapshot++;
	suc = true;

out:
	mps_list_unlock(mps_rec->rec_mps);
	return suc;
}

static struct pte_record *mps_alloc_record(struct lb_hot_mps *mps, uint32_t pid,
					   uint32_t uid, struct mm_struct *mm)
{
	struct pte_record *mps_rec;

	mps_rec = kzalloc(sizeof(*mps_rec), GFP_NOWAIT);
	if (!mps_rec) {
		MI_LB_ERR("alloc mps_rec %u failed", pid);
		return NULL;
	}
	atomic_inc(&mps->nr_alloc_mps_recs);

	mps_rec->pid = pid;
	mps_rec->uid = uid;
	xa_init(&mps_rec->rec_segs[PTE_FILE_PAGE]);
	xa_init(&mps_rec->rec_segs[PTE_ANON_PAGE]);
	mutex_init(&mps_rec->rec_lock);
	atomic_set(&mps_rec->rec_users, 1); //rec_users =1
	mps_rec->rec_mps = mps;
	mps_rec->rec_jiffies = jiffies;

	mps_rec->rec_mm = mm;
	if (!mps_link_rec(mps, mps_rec)) {
		MI_LB_ERR("mps_rec %u can not link", pid);
		kfree(mps_rec);
		atomic_dec(&mps->nr_alloc_mps_recs);
		return NULL;
	}

	mmgrab(mm); //mm_struct  + 1

	return mps_rec;
}

static struct pte_record *mps_find_check_record(struct lb_hot_mps *mps,
						struct task_struct *tsk,
						struct mm_struct *mm,
						bool create)
{
	pid_t tgid = task_tgid_nr(tsk);
	uint32_t uid = task_uid(tsk).val;
	struct pte_record *mps_rec;

	mps_rec = mps_get_record(mps, mm);
	if (create) {
		if (mps_rec) {
			mps_rec_put(mps_rec);
			return NULL;
		}
		mps_rec = mps_alloc_record(mps, tgid, uid, mm);
		if (!mps_rec) {
			return NULL;
		}
	} else {
		if (!mps_rec) {
			return NULL;
		}
		if (mps_rec->pid != tgid || mps_rec->uid != uid) {
			MI_LB_ERR("mps_rec %u %d not equal task %u %u",
				  mps_rec->pid, mps_rec->uid, tgid, uid);
			mps_rec_put(mps_rec);
			return NULL;
		}
	}

	return mps_rec;
}


static struct pte_record *mps_find_get_record(struct lb_hot_mps *mps, uint32_t pid,
					      bool create)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	struct pte_record *mps_rec = NULL;

	tsk = get_pid_task(find_vpid(pid), PIDTYPE_PID);
	if (!tsk) {
		MI_LB_ERR("get_pid_task %u failed", pid);
		return NULL;
	}

	mm = get_task_mm(tsk);
	if (mm) {
		mps_rec = mps_find_check_record(mps, tsk, mm, create);
		mmput(mm);
	}

	put_task_struct(tsk);

	return mps_rec;
}

static inline bool mps_skip_vma(struct vm_area_struct *vma, uint32_t types)
{
	if (is_vm_hugetlb_page(vma))
		return true;
	if (vma->vm_flags & (VM_PFNMAP | VM_MIXEDMAP))
		return true;
	if (!vma->vm_file && !(types & MPS_PAGE_TYPE_ANON))
		return true;
	if (vma->vm_file && !(types & MPS_PAGE_TYPE_FILE))
		return true;
	return false;
}

static inline bool mps_page_is_hot(struct page *page, pte_t ptent,
				   struct pte_record *mps_rec)
{
	if (PageActive(page)) {
		mps_rec->nr_active++;
		return true;
	} else {
		bool refer = PageReferenced(page);
		bool young = pte_young(ptent);

		if (young)
			mps_rec->nr_young++;
		if (refer)
			mps_rec->nr_referenced++;
		if (refer && young) {
			mps_rec->nr_refer_young++;
			return true;
		}
	}
	return false;
}

static int mps_hot_pte_range(pmd_t *pmd, unsigned long addr, unsigned long end,
			     struct mm_walk *walk)
{
	struct pte_record *mps_rec = walk->private;
	struct vm_area_struct *vma = walk->vma;
	int ret = 0;
	pte_t *pte = NULL, *entry;
	pte_t ptent;
	spinlock_t *ptl = NULL;
	struct page *page = NULL;
	uint32_t nr_pages = 0;
	int class = (vma->vm_file ? PTE_FILE_PAGE : PTE_ANON_PAGE);

	if (unlikely(pmd_trans_huge(*pmd) || pmd_devmap(*pmd))) {
		return 0;
	}

	if (!g_pte_anon_swapin_enable && (class == PTE_ANON_PAGE))
		return 0;
	if (!g_pte_fadvise_enable && (class == PTE_FILE_PAGE))
		return 0;
	if (!g_pte_mark_file_anon_enable)
		class = PTE_NON_PAGE;
	pte = entry = pte_offset_map_lock(vma->vm_mm, pmd, addr, &ptl);
	if (NULL == pte)
		return 0;
	for (; addr != end; pte++, addr += PAGE_SIZE) {
		ptent = *pte;
		if (!pte_present(ptent))
			continue;
		page = vm_normal_page(vma, addr, ptent);
		if (!page)
			continue;

		if (!mps_page_is_hot(page, ptent, mps_rec))
			continue;
		ret = mps_rec_set_hot(mps_rec, MPS_PAGE_INDEX(addr), class);
		if (ret)
			break;
		nr_pages++;
	}
	/* mps_rec_set_hot may return -ENOMEM when pte == entry. */
	pte_unmap_unlock(entry, ptl);
	if (!vma->vm_file)
		mps_rec->nr_anon += nr_pages;
	else
		mps_rec->nr_file += nr_pages;
	return ret;
}

static int mps_rec_snapshot(struct pte_record *mps_rec, uint32_t flags)
{
	struct mm_struct *mm = mps_rec->rec_mm;
	struct vm_area_struct *vma;
	uint32_t types = MPS_ACTION_SNAPSHOT_TYPE(flags);
	int ret = 0;
	const struct mm_walk_ops snapshot_ops = {
		.pmd_entry = mps_hot_pte_range,
	};
	VMA_ITERATOR(vmi, mm, 0);

	mmap_read_lock(mm);
	for_each_vma(vmi, vma){
		if (!vma->vm_file && vma->vm_end < MPS_ANON_BEGIN_ADDR)
			continue;
		if (mps_skip_vma(vma, types))
			continue;

		ret = walk_page_range(mm, vma->vm_start, vma->vm_end, &snapshot_ops, mps_rec);
		if (ret)
			break;
	}
	mmap_read_unlock(mm);
	if (flags & MPS_ACTION_DEBUG_VERBOSE) {
		unsigned long idx;
		struct pte_segment *seg;
		int i = 0;
		for (; i < PTE_PAGE_COUNT; i++) {
			xa_for_each(&mps_rec->rec_segs[i], idx, seg) {
				MI_LB_INFO("seg[0x%04x] %4u: %*pb", (uint32_t)idx,
					  bitmap_weight(seg->pagemap, MPS_SEGMENT_NBIT),
					  MPS_SEGMENT_NBIT, seg->pagemap);
			}
		}
	}
	return ret;
}

static int mps_rec_prepage_file(struct pte_record *mps_rec, struct file *file,
				ulong offset, ulong len)
{
	int ret;

	ret = vfs_fadvise(file, offset, len, POSIX_FADV_WILLNEED);
	if (!ret)
		mps_rec->nr_pre_file += len / PAGE_SIZE;

	return ret;
}

struct mps_swapin_walk_data {
	struct pte_record *mps_rec;
	struct vm_area_struct *vma;
};

static int mps_swapin_walk_pmd_entry(pmd_t *pmd, unsigned long start,
		unsigned long end, struct mm_walk *walk)
{
	struct mps_swapin_walk_data *data = (struct mps_swapin_walk_data *)walk->private;
	struct pte_record *mps_rec = data->mps_rec;
	struct vm_area_struct *vma = data->vma;
	pte_t *ptep = NULL;
	spinlock_t *ptl;
	unsigned long addr;

	for (addr = start; addr < end; addr += PAGE_SIZE) {
		pte_t pte;
		swp_entry_t entry;
		struct page *page;

		if (!ptep++) {
			ptep = pte_offset_map_lock(vma->vm_mm, pmd, addr, &ptl);
			if (!ptep)
				break;
		}

		pte = READ_ONCE(*ptep);
		barrier();
		if (!is_swap_pte(pte))
			continue;
		entry = pte_to_swp_entry(pte);
		if (unlikely(non_swap_entry(entry)))
			continue;

		pte_unmap_unlock(ptep, ptl);
		ptep = NULL;

		page = read_swap_cache_async(entry, GFP_HIGHUSER_MOVABLE,
					     vma, addr, NULL);
		if (page) {
			put_page(page);
			mps_rec->nr_pre_anon++;
		}
	}

	if (ptep)
		pte_unmap_unlock(ptep, ptl);
	cond_resched();

	return 0;
}

static int mps_rec_swapin_range(struct pte_record *mps_rec,
				struct vm_area_struct *vma, ulong addr,
				ulong end)
{
	int ret;
	const struct mm_walk_ops mps_swapin_walk_ops = {
		.pmd_entry		= mps_swapin_walk_pmd_entry,
		.walk_lock		= PGWALK_RDLOCK,
	};
	struct mps_swapin_walk_data data = {
		.mps_rec		= mps_rec,
		.vma			= vma,
	};

	ret = walk_page_range(mps_rec->rec_mm, addr,
			min_t(ulong, end, vma->vm_end), &mps_swapin_walk_ops, &data);

	return ret < 0 ? ret : 0;
}

static void mps_rec_mmput_hook(void *data1, struct mm_struct *mm)
{
	struct lb_hot_mps *mps = &g_hot_mps;

	if (!mm)
		return;

	mps_unlink_rec(mps, mm);
}

static int mps_rec_prepage_seg_range(struct pte_record *mps_rec, ulong addr,
				     ulong end, uint32_t flags)
{
	struct vm_area_struct *vma;
	int ret = 0;
	uint32_t types = MPS_ACTION_PREPAGE_TYPE(flags);
	struct file *file = NULL;
	ulong len = end - addr;
	loff_t offset;
	s64 dur;
	bool skip_swapin = false;
	struct lb_hot_mps *mps = mps_rec->rec_mps;

	MPS_STAT_WRITE_TIME(&mps_rec->stat, last_rlock_acquire_ts);
	mmap_read_lock(mps_rec->rec_mm);
	dur = MPS_STAT_DUR_TIME(&mps_rec->stat, last_rlock_acquire_ts);
	if (g_rlock_expire_interruput_enable && dur > g_rlock_acquire_threshold) {
		skip_swapin = true;
		mps_set_interrupt(mps_rec);
		atomic64_inc(&mps->stat.interrupt_count[MPS_INTERRUPT_BY_RLOCK]);
	}
	MPS_STAT_WRITE_SWAPIN_ACC_TIME(&mps_rec->stat, total_rlock_acquire_time, dur);

	vma = find_vma(mps_rec->rec_mm, addr);
	if (vma && !mps_skip_vma(vma, types)) {
		end = min(end, vma->vm_end);
		len = end - addr;
		if (vma->vm_file && get_file_rcu(vma->vm_file)) {
			file = vma->vm_file;
			offset = (loff_t)(addr - vma->vm_start) +
				 	((loff_t)vma->vm_pgoff << PAGE_SHIFT);
		} else if (!skip_swapin) {
			MPS_STAT_WRITE_TIME(&mps_rec->stat, last_swapin_trigger_ts);
			ret = mps_rec_swapin_range(mps_rec, vma, addr, end);
			dur = MPS_STAT_DUR_TIME(&mps_rec->stat, last_swapin_trigger_ts);
			MPS_STAT_WRITE_SWAPIN_ACC_TIME(&mps_rec->stat, total_swapin_trigger_time, dur);
		}
	}
	mmap_read_unlock(mps_rec->rec_mm);

	if (file) {
		if(g_atrace_enable) {
			char *f_path;
			f_path =file_path(file, mps->file_path, 256);
			MI_LB_DBG("pre_hot_file:%s [0x%lld 0x%lld]", f_path, offset, offset + len);
		}
		MPS_STAT_WRITE_TIME(&mps_rec->stat, last_fadvise_trigger_ts);
		ret = mps_rec_prepage_file(mps_rec, file, offset, len);
		fput(file);
		dur = MPS_STAT_DUR_TIME(&mps_rec->stat, last_fadvise_trigger_ts);
		MPS_STAT_WRITE_SWAPIN_ACC_TIME(&mps_rec->stat, total_fadvise_trigger_time, dur);
	}

	if (flags & MPS_ACTION_DEBUG_VERBOSE)
		MI_LB_INFO("prepage_%s %lu [0x%lx 0x%lx]", file ? "file" : "anon",
			len / PAGE_SIZE, MPS_PAGE_INDEX(addr),
			MPS_PAGE_INDEX(end));

	return ret;
}

static int mps_rec_prepage_seg(struct pte_record *mps_rec,
			       struct pte_segment *seg, ulong seg_index,
			       uint32_t flags)
{
	int ret = 0;
	uint32_t rs, re;

	bitmap_for_each_set_region(seg->pagemap, rs, re, 0, MPS_SEGMENT_NBIT)
	{
		ulong addr = MPS_SEG_TO_PAGE_INDEX(seg_index, rs) * PAGE_SIZE;
		ulong len = (re - rs) * PAGE_SIZE;

		if (mps_get_interrupt(mps_rec))
			break;

		ret = mps_rec_prepage_seg_range(mps_rec, addr,
							addr + len, flags);
		if (ret) {
			break;
		}
	}
	return ret;
}

static int mps_rec_prepage(struct pte_record *mps_rec, uint32_t flags,
			   uint32_t nr_anon_pre, uint32_t nr_file_pre)
{
	unsigned long idx;
	struct pte_segment *seg;
	int ret = 0, class = PTE_ANON_PAGE;
	s64 dur;
	struct lb_hot_mps *mps = mps_rec->rec_mps;

	MPS_STAT_WRITE_TIME(&mps_rec->stat, last_preread_start_ts);
	for (; class < PTE_PAGE_COUNT; class++) {
		xa_for_each(&mps_rec->rec_segs[class], idx, seg) {
			if (mps_get_interrupt(mps_rec))
				goto out;
			ret = mps_rec_prepage_seg(mps_rec, seg, idx, flags);
			if (mps_rec->nr_pre_anon >= nr_anon_pre)
				flags &= ~MPS_ACTION_PREPAGE_ANON;
			if (mps_rec->nr_pre_file >= nr_file_pre)
				flags &= ~MPS_ACTION_PREPAGE_FILE;
			if ((flags &
			     (MPS_ACTION_PREPAGE_ANON | MPS_ACTION_PREPAGE_FILE)) == 0)
				break;
			if (ret == -ENOMEM)
				break;
			dur = MPS_STAT_DUR_TIME(&mps_rec->stat, last_preread_start_ts);
			if (g_swapin_expire_interruput_enable && dur > g_swapin_total_time_threshold) {
				mps_set_interrupt(mps_rec);
				atomic64_inc(&mps->stat.interrupt_count[MPS_INTERRUPT_BY_SWAPIN]);
				goto out;
			}
		}
		/* don't need to swapin another class when disable this feature. */
		if (!g_pte_mark_file_anon_enable || !g_pte_fadvise_enable)
			break;
	}
out:
	dur = MPS_STAT_DUR_TIME(&mps_rec->stat, last_preread_start_ts);
	MPS_STAT_WRITE_SWAPIN_ACC_TIME(&mps_rec->stat, total_preread_time, dur);
	MI_LB_DBG("mps_rec %u ret %d prepage %u %u seg %u, rlock: %lld, swapin: %lld, fadvise: %lld, total: %lld, interrupt: %d",
			mps_rec->pid, ret,
			mps_rec->nr_pre_anon, mps_rec->nr_pre_file,
			mps_rec->nr_segments,
			MPS_STAT_READ_TIME(&mps_rec->stat, total_rlock_acquire_time),
			MPS_STAT_READ_TIME(&mps_rec->stat, total_swapin_trigger_time),
			MPS_STAT_READ_TIME(&mps_rec->stat, total_fadvise_trigger_time),
			MPS_STAT_READ_TIME(&mps_rec->stat, total_preread_time),
			mps_get_interrupt(mps_rec));
	return ret;
}

int mi_lb_dealwith_pte_snapshot_cmd(struct lb_owner *owner)
{
	int ret = 0;
	struct pte_record *mps_rec;
	struct lb_hot_mps *mps = &g_hot_mps;

	MI_LB_DBG("uid: %d, pid: %d, packagename: %s",
	   				owner->uid, owner->pid, owner->package_name);

	mps_rec = mps_find_get_record(mps, owner->pid, true);
	if (!mps_rec) {
		MI_LB_ERR("mps_rec %u maybe has snapshot", owner->pid);
		return ret;
	}
	mutex_lock(&mps_rec->rec_lock);
	ret = mps_rec_snapshot(mps_rec,
				MPS_PAGE_TYPE_ANON | MPS_PAGE_TYPE_FILE);

	mps_list_lock(mps);
	mps->total_anon += mps_rec->nr_anon;
	mps->total_file += mps_rec->nr_file;
	mps_list_unlock(mps);

	mutex_unlock(&mps_rec->rec_lock);
	mps_rec_put(mps_rec);

	if (ret)
		mps->nr_error++;

	return ret;
}

static void mps_stat_record_time(struct pte_record *mps_rec, struct lb_owner *owner)
{
	struct lb_hot_mps *mps = &g_hot_mps;

	if (MPS_STAT_READ_TIME(&mps_rec->stat, total_preread_time) < MPS_STAT_READ_TIME(&mps->stat, min_swapin_time)) {
		MPS_STAT_ASSIGN_TIME(&mps->stat, min_swapin_time, MPS_STAT_READ_TIME(&mps_rec->stat, total_preread_time));
		memcpy(mps->stat.shortest_app, owner->package_name, PACKAGE_NAME_MAX);
	}
	if (MPS_STAT_READ_TIME(&mps_rec->stat, total_preread_time) > MPS_STAT_READ_TIME(&mps->stat, max_swapin_time)) {
		MPS_STAT_ASSIGN_TIME(&mps->stat, max_swapin_time, MPS_STAT_READ_TIME(&mps_rec->stat, total_preread_time));
		memcpy(mps->stat.longest_app, owner->package_name, PACKAGE_NAME_MAX);
	}
	atomic64_add(MPS_STAT_READ_TIME(&mps_rec->stat, total_rlock_acquire_time), &mps->stat.total_rlock_time);
	atomic64_add(MPS_STAT_READ_TIME(&mps_rec->stat, total_swapin_trigger_time), &mps->stat.total_swapin_time);
	atomic64_add(MPS_STAT_READ_TIME(&mps_rec->stat, total_fadvise_trigger_time), &mps->stat.total_fadvise_time);
	atomic64_add(MPS_STAT_READ_TIME(&mps_rec->stat, total_preread_time), &mps->stat.total_preread_time);
	atomic64_inc(&mps->stat.total_swapin_count);
}

int mi_lb_dealwith_pte_preread_cmd(struct lb_owner *owner)
{
	int ret = 0;
	struct pte_record *mps_rec;
	struct lb_hot_mps *mps = &g_hot_mps;
	struct mm_struct *mm;

	MI_LB_DBG("uid: %d, pid: %d, packagename: %s",
	   				owner->uid, owner->pid, owner->package_name);

	mps_rec = mps_find_get_record(mps, owner->pid, false);
	if (!mps_rec) {
		MI_LB_ERR("mps_rec %u maybe no snapshot", owner->pid);
		return ret;
	}
	mutex_lock(&mps_rec->rec_lock);
	mps_rec->nr_pre_anon = 0;
	mps_rec->nr_pre_file = 0;
	ret = mps_rec_prepage(mps_rec, MPS_ACTION_PREPAGE_ANON | MPS_ACTION_PREPAGE_FILE, mps_rec->nr_anon, mps_rec->nr_file);

	mps_list_lock(mps);
	mps->nr_prepage++;
	mps_list_unlock(mps);

	mutex_unlock(&mps_rec->rec_lock);

	mps_stat_record_time(mps_rec, owner);

	mm = mps_rec->rec_mm;
	mps_rec_put(mps_rec);
	mps_unlink_rec(mps, mm);

	if (ret)
		mps->nr_error++;
	return ret;
}

int mi_lb_dealwith_pte_interrupt_cmd(struct lb_owner *owner)
{
	struct pte_record *mps_rec;
	struct lb_hot_mps *mps = &g_hot_mps;
	struct mm_struct *mm;

	MI_LB_DBG("uid: %d, pid: %d, packagename: %s",
					owner->uid, owner->pid, owner->package_name);

	mps_rec = mps_find_get_record(mps, owner->pid, false);
	if (!mps_rec) {
		MI_LB_ERR("mps_rec %u maybe no snapshot", owner->pid);
		return -ENOENT;
	}
	MI_LB_DBG("uid: %d, pid: %d, packagename: %s, interrupt preread",
					owner->uid, owner->pid, owner->package_name);
	mps_set_interrupt(mps_rec);
	atomic64_inc(&mps->stat.interrupt_count[MPS_INTERRUPT_BY_IOCTL]);
	mm = mps_rec->rec_mm;
	mps_rec_put(mps_rec);
	mps_unlink_rec(mps, mm);

	return 0;
}

int mi_lb_dealwith_pte_clean_cmd(struct lb_owner *owner)
{
	struct pte_record *mps_rec;
	struct pte_record *rec_tmp;
	struct lb_hot_mps *mps = &g_hot_mps;

	mps_list_lock(mps);
	list_for_each_entry_safe(mps_rec, rec_tmp, &mps->rec_list, rec_node) {
		if (mps_rec->pid == owner->pid && mps_rec->uid == owner->uid) {
			mps_unlink_rec_locked(mps, mps_rec->rec_mm);
			break;
		}
	}
	mps_list_unlock(mps);

	return 0;
}


static int mps_init(struct lb_hot_mps *mps)
{

	memset(mps, 0, sizeof(*mps));
	spin_lock_init(&mps->list_lock);
	mps->seg_cachep = KMEM_CACHE(pte_segment, SLAB_ACCOUNT);
	if (!mps->seg_cachep)
		return -ENOMEM;

	INIT_LIST_HEAD(&mps->rec_list);
	xa_init(&mps->rec_tree);
	atomic_set(&mps->nr_alloc_segs, 0);
	atomic_set(&mps->nr_alloc_mps_recs, 0);

	mps->stat.min_swapin_time = LONG_MAX;

	register_trace_android_vh_mmput_mm(mps_rec_mmput_hook, NULL);
	return 0;
}

static void mps_free_records(struct lb_hot_mps *mps)
{
	struct pte_record *mps_rec;
	struct pte_record *rec_tmp;

	unregister_trace_android_vh_mmput_mm(mps_rec_mmput_hook, NULL);

	mps_list_lock(mps);

	list_for_each_entry_safe(mps_rec, rec_tmp, &mps->rec_list, rec_node) {
		mps_unlink_rec_locked(mps, mps_rec->rec_mm);
	}
	mps_list_unlock(mps);
}

static void mps_exit(struct lb_hot_mps *mps)
{
	mps_free_records(mps);
	rcu_barrier();

	kmem_cache_destroy(mps->seg_cachep);
}

int lb_hot_init(void)
{
	int ret;

	MI_LB_INFO("init");
	ret = mps_init(&g_hot_mps);
	if (ret)
		return ret;
	return 0;
}

void lb_hot_exit(void)
{
	MI_LB_INFO("exit");
	mps_exit(&g_hot_mps);
	return;
}
