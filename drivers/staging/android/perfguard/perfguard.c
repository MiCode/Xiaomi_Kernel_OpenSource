/*
* android/perfguard.c
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/swap.h>
#include <linux/uaccess.h>
#include <linux/rmap.h>
#include <linux/hugetlb.h>
#include <linux/shrinker.h>
#include <asm/tlbflush.h>

#define NUMBUF 13
static struct dentry *pg_root;

struct perfguard{
	unsigned long nr_shrink_memory;
	unsigned long nr_shrink_process;
	unsigned long nr_shrink_zram;
	unsigned long nr_shrink_kgsl;
	unsigned long (*pg_kgsl_shrink) (struct shrinker *,
		struct shrink_control *sc);
	unsigned long (*pg_zsmalloc_shrink) (struct shrinker *,
		struct shrink_control *sc);
};
static struct perfguard pg;

void pg_register_kgsl_shrinker(struct shrinker *shrinker)
{
	pg.pg_kgsl_shrink = shrinker->scan_objects;
}

void pg_register_zsmalloc_shrinker(struct shrinker *shrinker)
{
	pg.pg_zsmalloc_shrink = shrinker->scan_objects;
}

/* Shrink system LRU pages */
ssize_t pg_sm_write(struct file *file, const char __user *buf,
		size_t siz, loff_t *pos)
{
	int err;
	char buffer[NUMBUF];
	unsigned long nr_to_reclaim, nr_reclaimed;

	memset(buffer, 0, sizeof(buffer));
	if (siz > sizeof(buffer) - 1)
		siz = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, siz)) {
		err = -EFAULT;
		goto out;
	}
	err = kstrtoul(strstrip(buffer), 0, &nr_to_reclaim);
	if (err)
		goto out;
	nr_reclaimed = pg_shrink_memory(nr_to_reclaim);
	pg.nr_shrink_memory += nr_reclaimed;
out:
	return err < 0 ? err : siz;
}

static const struct file_operations pg_smops = {
	.write          = pg_sm_write,
	.llseek 	= noop_llseek,
};

struct pg_shrinkprocess {
	struct vm_area_struct *vma;
	unsigned long nr_reclaimed;
};

static int pg_shrink_pte(pmd_t *pmd, unsigned long addr,
		unsigned long end, struct mm_walk *walk)
{
	struct pg_shrinkprocess *sp = walk->private;
	struct vm_area_struct *vma = sp->vma;
	pte_t *pte, ptent;
	spinlock_t *ptl;
	struct page *page;
	LIST_HEAD(page_list);
	int isolated;
	int reclaimed;

	split_huge_page_pmd(vma, addr, pmd);
	if (pmd_trans_unstable(pmd))
		return 0;
cont:
	isolated = 0;
	pte = pte_offset_map_lock(vma->vm_mm, pmd, addr, &ptl);
	for (; addr != end; pte++, addr += PAGE_SIZE) {
		ptent = *pte;
		if (!pte_present(ptent))
			continue;

		page = vm_normal_page(vma, addr, ptent);
		if (!page)
			continue;

		if (isolate_lru_page(page))
			continue;

		list_add(&page->lru, &page_list);
		inc_zone_page_state(page, NR_ISOLATED_ANON +
				page_is_file_cache(page));
		isolated++;
		if ((isolated >= SWAP_CLUSTER_MAX))
			break;
	}
	pte_unmap_unlock(pte - 1, ptl);
	reclaimed = pg_shrink_pages_from_list(&page_list, vma);
	sp->nr_reclaimed += reclaimed;
	if (addr != end)
		goto cont;

	cond_resched();
	return 0;
}

static unsigned long pg_shrink_process(pid_t pid)
{
	struct task_struct *task;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	struct mm_walk shrink_walk = {};
	struct pg_shrinkprocess sp;
	int ret = 0;

	if (!pid)
		return ret;

	task = get_pid_task(find_get_pid(pid), PIDTYPE_PID);
	if (!task) {
		ret = 0;
		goto out;
	}

	mm = get_task_mm(task);
	if (!mm) {
		put_task_struct(task);
		ret = 0;
		goto out;
	}

	memset(&sp, 0, sizeof(sp));
	shrink_walk.mm = mm;
	shrink_walk.pmd_entry = pg_shrink_pte;
	shrink_walk.private = &sp;

	down_read(&mm->mmap_sem);
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (is_vm_hugetlb_page(vma))
			continue;
		if (vma->vm_flags & (VM_SHARED | VM_MAYSHARE))
			continue;
		if (!vma->vm_file)
			continue;
		sp.vma = vma;
		walk_page_range(vma->vm_start, vma->vm_end, &shrink_walk);
	}
	flush_tlb_mm(mm);
	up_read(&mm->mmap_sem);
	mmput(mm);

	ret = sp.nr_reclaimed;
	put_task_struct(task);
out:
	return ret;
}

/* Shrink process pages */
ssize_t pg_sp_write(struct file *file, const char __user *buf,
		size_t siz, loff_t *pos)
{
	int err;
	pid_t pid;
	char buffer[NUMBUF];
	unsigned long nr_reclaimed;

	memset(buffer, 0, sizeof(buffer));
	if (siz > sizeof(buffer) - 1)
		siz = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, siz)) {
		err = -EFAULT;
		goto out;
	}
	err = kstrtoint(strstrip(buffer), 0, &pid);
	if (err)
		goto out;
	nr_reclaimed = pg_shrink_process(pid);
	pg.nr_shrink_process += nr_reclaimed;
out:
	return err < 0 ? err : siz;

}

static const struct file_operations pg_spops = {
	.write          = pg_sp_write,
	.llseek 	= noop_llseek,
};

/* Shrink ZRAM pages */
ssize_t pg_sz_write(struct file *file, const char __user *buf,
		size_t siz, loff_t *pos)
{
	int err;
	char buffer[NUMBUF];
	struct shrinker shrinker;
	struct shrink_control sc;
	unsigned long nr_to_reclaim, nr_reclaimed;

	if (!pg.pg_zsmalloc_shrink) {
		pr_warn("ZRAM shrinker is not registered\n");
		err = -EFAULT;
		goto out;
	}

	memset(buffer, 0, sizeof(buffer));
	if (siz > sizeof(buffer) - 1)
		siz = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, siz)) {
		err = -EFAULT;
		goto out;
	}
	err = kstrtoul(strstrip(buffer), 0, &nr_to_reclaim);
	if (err)
		goto out;
	sc.nr_to_scan = nr_to_reclaim;
	nr_reclaimed = pg.pg_zsmalloc_shrink(&shrinker, &sc);
	pg.nr_shrink_zram += nr_reclaimed;
out:
	return err < 0 ? err : siz;
}


static const struct file_operations pg_szops = {
	.write          = pg_sz_write,
	.llseek 	= noop_llseek,
};

/* Shrink KGSL pages */
ssize_t pg_sk_write(struct file *file, const char __user *buf,
		size_t siz, loff_t *pos)
{
	int err;
	char buffer[NUMBUF];
	struct shrinker shrinker;
	struct shrink_control sc;
	unsigned long nr_to_reclaim, nr_reclaimed;

	if (!pg.pg_kgsl_shrink) {
		pr_warn("KGSL shrinker is not registered\n");
		err = -EFAULT;
		goto out;
	}

	memset(buffer, 0, sizeof(buffer));
	if (siz > sizeof(buffer) - 1)
		siz = sizeof(buffer) - 1;
	if (copy_from_user(buffer, buf, siz)) {
		err = -EFAULT;
		goto out;
	}
	err = kstrtoul(strstrip(buffer), 0, &nr_to_reclaim);
	if (err)
		goto out;

	sc.nr_to_scan = nr_to_reclaim;
	nr_reclaimed = pg.pg_kgsl_shrink(&shrinker, &sc);
	pg.nr_shrink_kgsl += nr_reclaimed;
out:
	return err < 0 ? err : siz;
}

static const struct file_operations pg_skops = {
	.write          = pg_sk_write,
	.llseek 	= noop_llseek,
};

static int pg_st_show(struct seq_file *s, void *v)
{
	seq_printf(s, "nr_shrink_memory : %lu\n", pg.nr_shrink_memory);
	seq_printf(s, "nr_shrink_process : %lu\n", pg.nr_shrink_process);
	seq_printf(s, "nr_shrink_zram : %lu\n", pg.nr_shrink_zram);
	seq_printf(s, "nr_shrink_kgsl : %lu\n", pg.nr_shrink_kgsl);
	return 0;
}

static int pg_st_open(struct inode *inode, struct file *file)
{
	return single_open(file, pg_st_show, inode->i_private);
}

static const struct file_operations pg_stops = {
	.open		= pg_st_open,
	.read		= seq_read,
	.llseek 	= seq_lseek,
};

void pg_debugfs_init(void)
{
	struct dentry *entry;

	if (!debugfs_initialized())
		return;
	pg_root = debugfs_create_dir("perfguard", NULL);
	if (!pg_root) {
		pr_warn("debugfs dir <%s> creation failed\n", "sio");
		return;
	}

	entry = debugfs_create_file("shrink_memory", S_IWUSR,
			pg_root, NULL, &pg_smops);
	if (!entry) {
		pr_warn("debugfs file entry <%s> creation failed\n", "shrink_memory");
		return;
	}

	entry = debugfs_create_file("shrink_process", S_IWUSR,
			pg_root, NULL, &pg_spops);
	if (!entry) {
		pr_warn("debugfs file entry <%s> creation failed\n", "shrink_process");
		return;
	}

	entry = debugfs_create_file("shrink_zram", S_IWUSR,
			pg_root, NULL, &pg_szops);
	if (!entry) {
		pr_warn("debugfs file entry <%s> creation failed\n", "shrink_zram");
		return;
	}

	entry = debugfs_create_file("shrink_kgsl", S_IWUSR,
			pg_root, NULL, &pg_skops);
	if (!entry) {
		pr_warn("debugfs file entry <%s> creation failed\n", "shrink_kgsl");
		return;
	}


	entry = debugfs_create_file("statistics", S_IRUSR,
			pg_root, NULL, &pg_stops);
	if (!entry) {
		pr_warn("debugfs file entry <%s> creation failed\n", "statistics");
		return;
	}

}

static int __init pg_init(void)
{
	pg_debugfs_init();
	return 0;
}

module_init(pg_init);
MODULE_LICENSE("GPL");
