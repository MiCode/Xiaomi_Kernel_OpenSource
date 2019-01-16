#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/aee.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/setup.h>
#include <mach/mtk_memcfg.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/mod_devicetable.h>
#include <asm/io.h>
#include <linux/memblock.h>

#define MTK_MEMCFG_SIMPLE_BUFFER_LEN 16
#define MTK_MEMCFG_LARGE_BUFFER_LEN (2048)	/* it should not be larger than 1 page */

#if defined(CONFIG_MTK_FB)
extern unsigned int DISP_GetVRamSizeBoot(char *cmdline);
#ifdef CONFIG_OF
extern phys_addr_t mtkfb_get_fb_base(void);
#endif
#endif

struct mtk_memcfg_info_buf {
	unsigned long max_len;
	unsigned long curr_pos;
	char buf[MTK_MEMCFG_LARGE_BUFFER_LEN];
};

static struct mtk_memcfg_info_buf mtk_memcfg_layout_buf = {
	.buf = {[0 ... (MTK_MEMCFG_LARGE_BUFFER_LEN - 1)] = 0,},
	.max_len = MTK_MEMCFG_LARGE_BUFFER_LEN,
	.curr_pos = 0,
};

static unsigned long mtk_memcfg_late_warning_flag;

void mtk_memcfg_write_memory_layout_buf(char *fmt, ...)
{
	va_list ap;
	struct mtk_memcfg_info_buf *layout_buf = &mtk_memcfg_layout_buf;
	if (layout_buf->curr_pos <= layout_buf->max_len) {
		va_start(ap, fmt);
		layout_buf->curr_pos +=
		    vsnprintf((layout_buf->buf + layout_buf->curr_pos),
			      (layout_buf->max_len - layout_buf->curr_pos), fmt,
			      ap);
		va_end(ap);
	}
}

void mtk_memcfg_late_warning(unsigned long flag)
{
	mtk_memcfg_late_warning_flag |= flag;
}

/* kenerl memory information */

static int mtk_memcfg_memory_layout_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s", mtk_memcfg_layout_buf.buf);
	seq_printf(m, "buffer usage: %lu/%lu\n",
		   (mtk_memcfg_layout_buf.curr_pos <=
		    mtk_memcfg_layout_buf.max_len ?
		    mtk_memcfg_layout_buf.curr_pos :
		    mtk_memcfg_layout_buf.max_len),
		   mtk_memcfg_layout_buf.max_len);

	return 0;
}

static int mtk_memcfg_memory_layout_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_memcfg_memory_layout_show, NULL);
}

/* end of kenerl memory information */

/* kenerl memory fragmentation trigger */

static LIST_HEAD(frag_page_list);
static DEFINE_SPINLOCK(frag_page_list_lock);
static unsigned long mtk_memcfg_frag_round;
static struct kmem_cache *frag_page_cache;

struct frag_page {
	struct list_head list;
	struct page *page;
};

static int mtk_memcfg_frag_show(struct seq_file *m, void *v)
{
	int cnt = 0;
	struct frag_page *frag_page, *n_frag_page;
	spin_lock(&frag_page_list_lock);
	list_for_each_entry_safe(frag_page, n_frag_page, &frag_page_list, list) {
		cnt++;
	}
	spin_unlock(&frag_page_list_lock);
	seq_printf(m, "round: %lu, fragmentation-trigger held %d pages, %d MB\n",
		   mtk_memcfg_frag_round,
		   cnt, (cnt << PAGE_SHIFT) >> 20);

	return 0;
}

static int mtk_memcfg_frag_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_memcfg_frag_show, NULL);
}

void split_page(struct page *page, unsigned int order);

static int do_fragmentation(void *n)
{
	struct frag_page *frag_page, *n_frag_page;
	struct page *page;
	gfp_t gfp_mask = GFP_ATOMIC;
	unsigned int max_order = 2;
	int cnt = 0, i;

	/* trigger fragmentation */
	/*
	 * Allocate an order-2-page, split it into 4 order-0-pages,
	 * and free 3 of them, repeatedly.
	 * In this way, we split all high order pages to
	 * order-0-pages and order-1-pages to create a
	 * fragmentation scenario.
	 *
	 * In current stage, we only trigger fragmentation in
	 * normal zone.
	 */
	while (1) {
#if 1
		if (cnt >= 10000) {
			/*
			 * release all memory and restart the fragmentation
			 * Allocating too much frag_page consumes too mush order-0 pages
			 */
			spin_lock(&frag_page_list_lock);
			list_for_each_entry_safe(frag_page, n_frag_page,
						 &frag_page_list, list) {
				list_del(&frag_page->list);
				__free_page(frag_page->page);
				kmem_cache_free(frag_page_cache, frag_page);
				cnt--;
			}
			spin_unlock(&frag_page_list_lock);
			pr_alert("round: %lu, fragmentation-trigger free pages %d left\n",
				 mtk_memcfg_frag_round, cnt);
		}
#endif
		while (1) {
			frag_page = kmem_cache_alloc(frag_page_cache, gfp_mask);
			if (!frag_page)
				break;
			page = alloc_pages(gfp_mask, max_order);
			if (!page) {
				kfree(frag_page);
				break;
			}
			split_page(page, 0);
			INIT_LIST_HEAD(&frag_page->list);
			frag_page->page = page;
			spin_lock(&frag_page_list_lock);
			list_add(&frag_page->list, &frag_page_list);
			spin_unlock(&frag_page_list_lock);
			for (i = 1; i < (1 << max_order); i++)
				__free_page(page + i);
			cnt++;
		}
		mtk_memcfg_frag_round++;
		pr_alert("round: %lu, fragmentation-trigger allocate %d pages %d MB\n",
			 mtk_memcfg_frag_round, cnt, (cnt << PAGE_SHIFT) >> 20);
		msleep(500);
	}

	return 0;
}

static ssize_t
mtk_memcfg_frag_write(struct file *file, const char __user *buffer,
		      size_t count, loff_t *pos)
{
	static char state;
	static struct task_struct *p;
	if (count > 0) {
		if (get_user(state, buffer))
			return -EFAULT;
		state -= '0';
		pr_alert("%s state = %d\n", __func__, state);
		if (state) {
			pr_alert("activate do_fragmentation kthread\n");
			p = kthread_create(do_fragmentation, NULL,
					   "fragmentationd");
			if (!IS_ERR(p))
				wake_up_process(p);
		}
	}
	return count;
}

/* end of kenerl memory fragmentation trigger */

static int __init mtk_memcfg_init(void)
{
	return 0;
}

static void __exit mtk_memcfg_exit(void)
{
}

static const struct file_operations mtk_memcfg_memory_layout_operations = {
	.open = mtk_memcfg_memory_layout_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations mtk_memcfg_frag_operations = {
	.open = mtk_memcfg_frag_open,
	.write = mtk_memcfg_frag_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#ifdef CONFIG_SLUB_DEBUG

extern int slabtrace_open(struct inode *inode, struct file *file);
static const struct file_operations proc_slabtrace_operations = {
	.open = slabtrace_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif
static int __init mtk_memcfg_late_init(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtk_memcfg_dir = NULL;

	pr_info("[%s] start\n", __func__);

	mtk_memcfg_dir = proc_mkdir("mtk_memcfg", NULL);

	if (!mtk_memcfg_dir) {
		pr_err("[%s]: mkdir /proc/mtk_memcfg failed\n", __func__);
	} else {
		/* display kernel memory layout */
		entry = proc_create("memory_layout",
				    S_IRUGO | S_IWUSR, mtk_memcfg_dir,
				    &mtk_memcfg_memory_layout_operations);

		if (!entry)
			pr_err("create memory_layout proc entry failed\n");

		/* display kernel memory layout */
		entry = proc_create("frag-trigger",
				    S_IRUGO | S_IWUSR, mtk_memcfg_dir,
				    &mtk_memcfg_frag_operations);

		if (!entry)
			pr_err("create memory_layout proc entry failed\n");

		frag_page_cache = kmem_cache_create("frag_page_cache",
						    sizeof(struct frag_page),
						    0, SLAB_PANIC, NULL);

		if (!frag_page_cache)
			pr_err("create frag_page_cache failed\n");

#ifdef CONFIG_SLUB_DEBUG
		/* slabtrace - full slub object backtrace */
		entry = proc_create("slabtrace",
				    S_IRUSR, mtk_memcfg_dir,
				    &proc_slabtrace_operations);

		if (!entry)
			pr_err("create slabtrace proc entry failed\n");
#endif
	}
	return 0;
}

module_init(mtk_memcfg_init);
module_exit(mtk_memcfg_exit);

#ifdef CONFIG_HIGHMEM
extern unsigned long totalhigh_pages;
#endif /* end of CONFIG_HIGHMEM */
static int __init mtk_memcfg_late_sanity_test(void)
{
	/* trigger kernel warning if warning flag is set */
	if (mtk_memcfg_late_warning_flag & WARN_MEMBLOCK_CONFLICT) {
		aee_kernel_warning("[memory layout conflict]",
				   mtk_memcfg_layout_buf.buf);
	}

	if (mtk_memcfg_late_warning_flag & WARN_MEMSIZE_CONFLICT) {
		aee_kernel_warning("[memory size conflict]",
				   mtk_memcfg_layout_buf.buf);
	}

	if (mtk_memcfg_late_warning_flag & WARN_API_NOT_INIT) {
		aee_kernel_warning("[API is not initialized]",
				   mtk_memcfg_layout_buf.buf);
	}

#ifdef CONFIG_HIGHMEM
	/* check highmem zone size */
	if (unlikely
		(totalhigh_pages && (totalhigh_pages << PAGE_SHIFT) < SZ_8M)) {
		aee_kernel_warning("[high zone lt 8MB]", __func__);
	}
#endif /* end of CONFIG_HIGHMEM */

	return 0;
}

/* scan memory layout */
#ifdef CONFIG_OF
static int dt_scan_memory(unsigned long node, const char *uname, int depth, void *data)
{
	char *type = of_get_flat_dt_prop(node, "device_type", NULL);
	int i;
	unsigned long l;
	u64 kernel_mem_sz = 0;
	u64 phone_dram_sz = 0x0;	/* original phone DRAM size */
	u64 dram_sz = 0;	/* total DRAM size of all modules */
	dram_info_t *dram_info;
	mem_desc_t *mem_desc;
	mblock_info_t *mblock_info;
	__be32 *reg, *endp;
	u64 fb_base = 0x12345678, fb_size = 0;

	/* We are scanning "memory" nodes only */
	if (type == NULL) {
		/*
		 * The longtrail doesn't have a device_type on the
		 * /memory node, so look for the node called /memory@0.
		 */
		if (depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
	} else if (strcmp(type, "memory") != 0) {
		return 0;
	}

		reg = of_get_flat_dt_prop(node, "reg", &l);
	if (reg == NULL)
		return 0;

	endp = reg + (l / sizeof(__be32));

	while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
		u64 base, size;

		base = dt_mem_next_cell(dt_root_addr_cells, &reg);
		size = dt_mem_next_cell(dt_root_size_cells, &reg);

		if (size == 0)
			continue;

		MTK_MEMCFG_LOG_AND_PRINTK(KERN_ALERT
			"[PHY layout]DRAM size (dt) :  0x%llx - 0x%llx  (0x%llx)\n",
				(unsigned long long)base,
				(unsigned long long)base + (unsigned long long)size - 1,
				(unsigned long long)size);
		kernel_mem_sz += size;
	}

	/* orig_dram_info */
	dram_info = (dram_info_t *)of_get_flat_dt_prop(node,
			"orig_dram_info", NULL);
	if (dram_info) {
		for (i = 0; i < dram_info->rank_num; i++) {
			MTK_MEMCFG_LOG_AND_PRINTK(KERN_ALERT
				"[PHY layout]orig_dram rank[%d]   :   "
				"0x%08llx - 0x%08llx (0x%llx)\n",
				i,
				dram_info->rank_info[i].start,
				dram_info->rank_info[i].start +
				dram_info->rank_info[i].size - 1,
				dram_info->rank_info[i].size
				);
			phone_dram_sz += dram_info->rank_info[i].size;
		}
	}

	/* mblock_info */
	mblock_info = (mblock_info_t *)of_get_flat_dt_prop(node,
			"mblock_info", NULL);
	if (mblock_info) {
		for (i = 0; i < mblock_info->mblock_num; i++) {
			MTK_MEMCFG_LOG_AND_PRINTK(KERN_ALERT
				"[PHY layout]mblock[%d][r%d]  :   "
				"0x%08llx - 0x%08llx (0x%llx)\n",
				i,
				mblock_info->mblock[i].rank,
				mblock_info->mblock[i].start,
				mblock_info->mblock[i].start +
				mblock_info->mblock[i].size - 1,
				mblock_info->mblock[i].size
				);
			dram_sz += mblock_info->mblock[i].size;
		}
	}

	/* lca reserved memory */
	mem_desc = (mem_desc_t *)of_get_flat_dt_prop(node,
			"lca_reserved_mem", NULL);
	if (mem_desc && mem_desc->size) {
		MTK_MEMCFG_LOG_AND_PRINTK(KERN_ALERT
			"[PHY layout]lca_reserved_mem   :   "
			"0x%08llx - 0x%08llx (0x%llx)\n",
			mem_desc->start,
			mem_desc->start +
			mem_desc->size - 1,
			mem_desc->size
			);
		dram_sz += mem_desc->size;
	}

	/* tee reserved memory */
	mem_desc = (mem_desc_t *)of_get_flat_dt_prop(node,
			"tee_reserved_mem", NULL);
	if (mem_desc && mem_desc->size) {
		MTK_MEMCFG_LOG_AND_PRINTK(KERN_ALERT
			"[PHY layout]tee_reserved_mem   :   "
			"0x%08llx - 0x%08llx (0x%llx)\n",
			mem_desc->start,
			mem_desc->start +
			mem_desc->size - 1,
			mem_desc->size
			);
		dram_sz += mem_desc->size;
	}

	/* frame buffer */
	fb_size = (u64)DISP_GetVRamSizeBoot(NULL);
	fb_base = (u64)mtkfb_get_fb_base();
	dram_sz += fb_size;

	/* verify memory size */
#if 0
	if (dram_sz != phone_dram_sz) {
		MTK_MEMCFG_LOG_AND_PRINTK(KERN_ALERT
			"memory size not matched: dram_sz: 0x%llx, "
				"phone_dram_sz: 0x%llx\n",
				(unsigned long long)dram_sz,
				(unsigned long long)phone_dram_sz);
		mtk_memcfg_late_warning(WARN_MEMSIZE_CONFLICT);
	}
#endif

	/* print memory information */
	MTK_MEMCFG_LOG_AND_PRINTK(KERN_ALERT
		"[PHY layout]available DRAM size = 0x%llx\n"
		"[PHY layout]FB (dt) :  0x%llx - 0x%llx  (0x%llx)\n",
			(unsigned long long)kernel_mem_sz,
			(unsigned long long)fb_base,
			(unsigned long long)fb_base + fb_size - 1,
			(unsigned long long)fb_size);

	pr_alert("fb base: 0x%llx, size: 0x%llx\n",
		 (unsigned long long)DISP_GetVRamSizeBoot(NULL),
		 (unsigned long long)mtkfb_get_fb_base());

	return node;
}

static int __init display_early_memory_info(void)
{
	int node;
	/* system memory */
	node = of_scan_flat_dt(dt_scan_memory, NULL);
	return 0;
}


#endif /* end of CONFIG_OF */

late_initcall(mtk_memcfg_late_init);
late_initcall(mtk_memcfg_late_sanity_test);
#ifdef CONFIG_OF
pure_initcall(display_early_memory_info);
#endif /* end of CONFIG_OF */


#if 0	/* test code of of_reserve */
/* test memory-reservd code */
phys_addr_t test_base = 0;
phys_addr_t test_size = 0;
reservedmem_of_init_fn reserve_memory_test_fn(struct reserved_mem *rmem,
				      unsigned long node, const char *uname)
{
	pr_alert("%s, name: %s, uname: %s, base: 0x%llx, size: 0x%llx\n",
		 __func__,  rmem->name, uname,
		 (unsigned long long)rmem->base,
		 (unsigned long long)rmem->size);
	/* memblock_free(rmem->base, rmem->size); */
	test_base = rmem->base;
	test_size = rmem->size;

	return 0;
}

static int __init init_test_reserve_memory(void)
{
	void *p = 0;
	p = ioremap(test_base, (size_t)test_size);
	if (p) {
		pr_alert("%s:%d ioremap ok: %p\n", __func__, __LINE__,
			 p);
	} else {
		pr_alert("%s:%d ioremap failed\n", __func__, __LINE__);
	}
	return 0;
}
late_initcall(init_test_reserve_memory);

reservedmem_of_init_fn mrdump_reserve_initfn(struct reserved_mem *rmem,
				      unsigned long node, const char *uname)
{
	pr_alert("%s, name: %s, uname: %s, base: 0x%llx, size: 0x%llx\n",
		 __func__,  rmem->name, uname,
		 (unsigned long long)rmem->base,
		 (unsigned long long)rmem->size);

	return 0;
}

RESERVEDMEM_OF_DECLARE(reserve_memory_test1, "reserve-memory-test",
		       reserve_memory_test_fn);
RESERVEDMEM_OF_DECLARE(mrdump_reserved_memory, "mrdump-reserved-memory",
		       mrdump_reserve_initfn);
#endif /* end of test code of of_reserve */
