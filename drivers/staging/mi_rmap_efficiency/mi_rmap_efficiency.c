
#include <linux/module.h>
#include <linux/types.h>
#include <linux/swap.h>
#include <linux/proc_fs.h>
#include <linux/gfp.h>
#include <linux/printk.h>
#include <linux/mmzone.h>
#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/pagemap.h>
#include <linux/page-flags.h>
#include <linux/debugfs.h>
#include <linux/memcontrol.h>
#include <linux/spinlock.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/vmscan.h>
#define MI_RETRY_GET_MAPCOUNT (3)
static unsigned int mi_mapcount_protect_thres __read_mostly = 20;
static unsigned int mi_thres_control __read_mostly = 10;
static unsigned long mi_memavail_noprotect_thres = 0;
static bool mi_mapped_protect_setup = false;
static struct kobject *kobj;
spinlock_t traversal_lock;
/*
 * [0]: sum of anon mapped pages
 * [1]: sum of file mapped pages
 *
 * */
static atomic_long_t nr_mapped_multiple[2] = {
	ATOMIC_INIT(0)
};
static atomic_long_t nr_mapped_multiple_debug[2] = {
	ATOMIC_INIT(0)
};
long max_nr_mapped_multiple[2] = {0};
unsigned long mi_mapped_protect_thres[2] = {0};
extern void do_traversal_all_lruvec(void);
static bool mapped_protect_is_full(int type)
{
	if (atomic_long_read(&nr_mapped_multiple[type]) > mi_mapped_protect_thres[type])
		return true;
	else
		return false;
}
static bool mem_available_is_low(void)
{
	long available = si_mem_available();
	if (available < mi_memavail_noprotect_thres)
		return true;
	return false;
}
static void page_should_be_protect(void *data, struct page* page, bool *should_protect)
{
	int type;
	if (unlikely(!mi_mapped_protect_setup)) {
		*should_protect = false;
		return;
	}
	if (likely(page_mapcount(page) < mi_mapcount_protect_thres)) {
		*should_protect = false;
		return;
	}
	if (unlikely(PageUnevictable(page))) {
		*should_protect = false;
		return;
	}
	type = page_is_file_lru(page);
	if (unlikely(mapped_protect_is_full(type))) {
		*should_protect = false;
		return;
	}
	if (unlikely(mem_available_is_low())) {
		*should_protect = false;
		return;
	}
	*should_protect = true;
}
static void update_page_mapcount(void *data, struct page *page, bool inc_size, bool compound, bool *ret, bool *success)
{
	unsigned long nr_mapped_multi_pages;
	int type, mapcount;
	*success = true;
	if (inc_size) {
		mapcount = atomic_inc_return(&page->_mapcount);
		if (ret)
			*ret = ((mapcount == 0) ? true : false);
	} else {
		mapcount = atomic_add_return(-1, &page->_mapcount);
		if (ret)
			*ret = ((mapcount < 0) ? true : false);
	}
	if (likely((mapcount + (inc_size ? 1 : 2)) != mi_mapcount_protect_thres))
		return;
	if (unlikely(!mi_mapped_protect_setup))
		return;
	if (!PageLRU(page) || PageUnevictable(page))
		return;
	type = page_is_file_lru(page);
	if (inc_size) {
		atomic_long_add(1, &nr_mapped_multiple[type]);
		nr_mapped_multi_pages = atomic_long_read(&nr_mapped_multiple[type]);
		if (max_nr_mapped_multiple[type] < nr_mapped_multi_pages)
			max_nr_mapped_multiple[type] = nr_mapped_multi_pages;
	} else {
		atomic_long_sub(1, &nr_mapped_multiple[type]);
	}
	return;
}
static void mi_folio_check_bypass(void *data, struct  page * page,unsigned long nr_to_scan, int lru, bool *bypass)
{
	if (page_mapcount( page) >= 20) {
		*bypass = true;
	}
}
/* count mapped pages when add to LRU */
static void add_page_to_lrulist(void *data, struct page *page, bool compound, enum lru_list lru)
{
	int type;
	unsigned long nr_mapped_multi_pages;
	if (unlikely(!mi_mapped_protect_setup))
		return;
	if (likely(page_mapcount(page) < mi_mapcount_protect_thres))
		return;
	if (lru == LRU_UNEVICTABLE)
		return;
	/* type - should the page be on a file LRU or anon LRU */
	type = page_is_file_lru(page);
	atomic_long_add(1, &nr_mapped_multiple[type]);
	nr_mapped_multi_pages = atomic_long_read(&nr_mapped_multiple[type]);
	if (max_nr_mapped_multiple[type] < nr_mapped_multi_pages) {
		max_nr_mapped_multiple[type] = nr_mapped_multi_pages;
	}
}
/* count mapped pages when del from LRU */
static void del_page_from_lrulist(void *data, struct page *page, bool compound, enum lru_list lru)
{
	int type;
	if (unlikely(!mi_mapped_protect_setup))
		return;
	if (likely(page_mapcount(page) < mi_mapcount_protect_thres))
		return;
	if (lru == LRU_UNEVICTABLE)
		return;
	/* type - should the page be on a file LRU or anon LRU */
	type = page_is_file_lru(page);
	atomic_long_sub(1, &nr_mapped_multiple[type]);
}
/* pages scanned and get the statistics of mapped pages during the traversal */
static void do_traversal_lruvec(void *data, struct lruvec *lruvec)
{
	struct page *page;
	int lru, i;
	spin_lock_irq(&traversal_lock);
	for_each_evictable_lru(lru) {
		int type = is_file_lru(lru);
		list_for_each_entry(page, &lruvec->lists[lru], lru) {
			if (!page)
				continue;
			/* the head page of huge page */
			if (PageHead(page)) {
				for (i = 0; i < compound_nr(page); i++) {
					if (page_mapcount(&page[i]) >= mi_mapcount_protect_thres)
						atomic_long_add(1, &nr_mapped_multiple_debug[type]);
				}
				continue;
			}
			/* normal page */
			if (page_mapcount(page) >= mi_mapcount_protect_thres)
				atomic_long_add(thp_nr_pages(page), &nr_mapped_multiple_debug[type]);
		}
	}
	spin_unlock_irq(&traversal_lock);
}
/* present related statistical data in proc */
static int mapped_protect_show(struct seq_file *m, void *arg) {
	atomic_long_set(&nr_mapped_multiple_debug[0], 0);
	atomic_long_set(&nr_mapped_multiple_debug[1], 0);
	do_traversal_all_lruvec();
	seq_printf(m,
		   "mi_mapped_protect_setup:     %s\n"
		   "mi_cur_anon_mapped_pages:     %ld\n"
		   "mi_cur_file_mapped_pages:     %ld\n"
		   "mi_sum_anon_mapped_pages:     %ld\n"
		   "mi_sum_file_mapped_pages:     %ld\n"
		   "mi_max_sum_anon_mapped_pages:     %ld\n"
		   "mi_max_sum_file_mapped_pages:     %ld\n"
		   "mi_anon_mapped_pages_thres:     %lu\n"
		   "mi_file_mapped_pages_thres:     %lu\n"
		   "mi_memavail_noprotect_thres:     %lu\n"
		   "mi_mapcount_protect_thres:     %u\n",
		   mi_mapped_protect_setup ? "enable" : "disable",
		   nr_mapped_multiple_debug[0],
		   nr_mapped_multiple_debug[1],
		   nr_mapped_multiple[0],
		   nr_mapped_multiple[1],
		   max_nr_mapped_multiple[0],
		   max_nr_mapped_multiple[1],
		   mi_mapped_protect_thres[0],
		   mi_mapped_protect_thres[1],
		   mi_memavail_noprotect_thres,
		   mi_thres_control);
	seq_putc(m, '\n');
	return 0;
}
static ssize_t show_mi_mapped_protect_enable(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%s\n", mi_mapped_protect_setup ? "yes" : "no");
}
static ssize_t store_mi_mapped_protect_enable(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t len)
{
	ssize_t ret;
	ret = kstrtobool(buf, &mi_mapped_protect_setup);
	/* convert failed */
	if (ret) {
		return ret;
	}
	return len;
}
static struct kobj_attribute mi_mapped_protect_check_enable_attr = __ATTR(
	enabled, 0644, show_mi_mapped_protect_enable, store_mi_mapped_protect_enable
);
static ssize_t show_mi_mapcount_protect_thres(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", mi_mapcount_protect_thres);
}
static ssize_t store_mi_mapcount_protect_thres(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t len)
{
	if (kstrtouint(buf, 0, &mi_mapcount_protect_thres))
		return -EINVAL;
	return len;
}
static struct kobj_attribute mi_mapcount_protect_thres_attr = __ATTR(
	mapcount_protect_thres, 0644, show_mi_mapcount_protect_thres, store_mi_mapcount_protect_thres
);
static ssize_t show_mi_thres_control(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", mi_thres_control);
}
static ssize_t store_mi_thres_control(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t len)
{
	if (kstrtouint(buf, 0, &mi_thres_control))
		return -EINVAL;
	mi_memavail_noprotect_thres = totalram_pages() / mi_thres_control;
	mi_mapped_protect_thres[0] = mi_memavail_noprotect_thres >> 1;
	mi_mapped_protect_thres[1] = mi_memavail_noprotect_thres >> 1;
	return len;
}
static struct kobj_attribute mi_thres_control_attr = __ATTR(
	noprotect_thres_control, 0644, show_mi_thres_control, store_mi_thres_control
);
static struct attribute *mi_mapped_protect_check_attr[] = {
	&mi_mapped_protect_check_enable_attr.attr,
	&mi_mapcount_protect_thres_attr.attr,
	&mi_thres_control_attr.attr,
	NULL
};
static struct attribute_group mi_mapped_protect_check_attr_group = {
	.name = "mi_mapped_protect",
	.attrs = mi_mapped_protect_check_attr,
};
static int init_mapped_protect_sysfs(void)
{
	kobj = kobject_create_and_add("mapped_protect", &THIS_MODULE->mkobj.kobj);
	if (kobj) {
		if (sysfs_create_group(kobj, &mi_mapped_protect_check_attr_group))
			pr_err("mapped_protect: failed to create related sysfs\n");
	}
	return 0;
}
static void destroy_mapped_control_sysfs(void)
{
	if (kobj) {
		sysfs_remove_group(kobj, &mi_mapped_protect_check_attr_group);
		kobject_put(kobj);
	}
}
static void register_mapped_protect_vendor_hooks(void)
{
	register_trace_android_vh_update_page_mapcount(update_page_mapcount, NULL);
	register_trace_android_vh_add_page_to_lrulist(add_page_to_lrulist, NULL);
	register_trace_android_vh_del_page_from_lrulist(del_page_from_lrulist, NULL);
	register_trace_android_vh_page_should_be_protected(page_should_be_protect, NULL);
	register_trace_android_vh_do_traversal_lruvec(do_traversal_lruvec, NULL);
  	register_trace_android_vh_page_referenced_check_bypass(mi_folio_check_bypass, NULL);
	return;
}
static void unregister_mapped_protect_vendor_hooks(void)
{
	unregister_trace_android_vh_update_page_mapcount(update_page_mapcount, NULL);
	unregister_trace_android_vh_add_page_to_lrulist(add_page_to_lrulist, NULL);
	unregister_trace_android_vh_del_page_from_lrulist(del_page_from_lrulist, NULL);
	unregister_trace_android_vh_page_should_be_protected(page_should_be_protect, NULL);
	unregister_trace_android_vh_do_traversal_lruvec(do_traversal_lruvec, NULL);
  	unregister_trace_android_vh_page_referenced_check_bypass(mi_folio_check_bypass, NULL);
	return;
}
static int __init mapped_protect_init(void)
{
	int retry = 0;
	spin_lock_init(&traversal_lock);
	init_mapped_protect_sysfs();
	register_mapped_protect_vendor_hooks();
	mi_memavail_noprotect_thres = totalram_pages() / mi_thres_control;
	mi_mapped_protect_thres[0] = mi_memavail_noprotect_thres >> 1;
	mi_mapped_protect_thres[1] = mi_memavail_noprotect_thres >> 1;
	do_traversal_all_lruvec();
	atomic_long_set(&nr_mapped_multiple[0], atomic_long_read(&nr_mapped_multiple_debug[0]));
	atomic_long_set(&nr_mapped_multiple[1], atomic_long_read(&nr_mapped_multiple_debug[1]));
retry_get_mapped_pages:
	if (retry++ < MI_RETRY_GET_MAPCOUNT) {
		atomic_long_set(&nr_mapped_multiple_debug[0], 0);
		atomic_long_set(&nr_mapped_multiple_debug[1], 0);
		do_traversal_all_lruvec();
		if (atomic_long_read(&nr_mapped_multiple[0]) !=
			atomic_long_read(&nr_mapped_multiple_debug[0]) ||
			atomic_long_read(&nr_mapped_multiple[1]) !=
			atomic_long_read(&nr_mapped_multiple_debug[1])) {
			atomic_long_set(&nr_mapped_multiple[0], atomic_long_read(&nr_mapped_multiple_debug[0]));
			atomic_long_set(&nr_mapped_multiple[1], atomic_long_read(&nr_mapped_multiple_debug[1]));
			goto retry_get_mapped_pages;
		}
	}
	mi_mapped_protect_setup = true;
	proc_create_single("mi_mapped_protect_show", 0, NULL, mapped_protect_show);
	printk(KERN_EMERG "initialize mapped_protect voom voom!!!!\n");
	return 0;
}
static void __exit mapped_protect_exit(void)
{
	destroy_mapped_control_sysfs();
	unregister_mapped_protect_vendor_hooks();
	printk(KERN_EMERG "mapped_protect exit successful!!!!\n");
	return;
}
module_init(mapped_protect_init);
module_exit(mapped_protect_exit);
MODULE_AUTHOR("maminghui5 <maminghui5@xiaomi.com>");
MODULE_LICENSE("GPL");

