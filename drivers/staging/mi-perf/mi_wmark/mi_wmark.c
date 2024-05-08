#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/page-flags.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/swap.h>
#include <linux/slab.h>

#include "mi_wmark.h"

static struct kobject *mi_wmark_kobj;
static int extra_free_kbytes;
static unsigned long pages_low;
static int min_free_kbytes_ = 1024;
static int watermark_scale_factor_ = 10;
static unsigned long vm_total_pages_;


static __always_inline struct zoneref *next_zones_zonelist_(struct zoneref *z,
					enum zone_type highest_zoneidx)
{
	while (zonelist_zone_idx(z) > highest_zoneidx)
		z++;
	return z;
}

#define for_each_zone_zonelist_(zone, z, zlist, highidx) \
	for (z = next_zones_zonelist_(zonelist->_zonerefs, highidx), zone = zonelist_zone(z);	\
		zone;							\
		z = next_zones_zonelist_(++z, highidx),	\
			zone = zonelist_zone(z))

static enum zone_type gfp_zone_(gfp_t flags)
{
	enum zone_type z;
	gfp_t local_flags = flags;
	int bit;

	bit = (__force int) ((local_flags) & GFP_ZONEMASK);

	z = (GFP_ZONE_TABLE >> (bit * GFP_ZONES_SHIFT)) &
					 ((1 << GFP_ZONES_SHIFT) - 1);
	return z;
}

static struct zone *next_zone_(struct zone *zone)
{
	pg_data_t *pgdat = zone->zone_pgdat;

	if (zone < pgdat->node_zones + MAX_NR_ZONES - 1)
		zone++;
	else
		zone = NULL;
	return zone;
}

#define for_each_zone_(zone)			        \
	for (zone = (NODE_DATA(numa_node_id()))->node_zones; \
	     zone;					\
	     zone = next_zone_(zone))

static unsigned long nr_free_zone_pages(int offset)
{
	struct zoneref *z;
	struct zone *zone;
	/* Just pick one node, since fallback list is circular */
	unsigned long sum = 0;
	struct zonelist *zonelist = node_zonelist(numa_node_id(), GFP_KERNEL);

	for_each_zone_zonelist_(zone, z, zonelist, offset) {
		unsigned long size =  zone_managed_pages(zone);
		unsigned long high = high_wmark_pages(zone);

		if (size > high)
			sum += size - high;
	}

	return sum;
}

static void setup_per_zone_wmarks_extra(void)
{
	unsigned long pages_min = min_free_kbytes_ >> (PAGE_SHIFT - 10);
	unsigned long lowmem_pages = 0;
	// unsigned long vm_total_pages_ = nr_free_zone_pages(gfp_zone_(GFP_HIGHUSER_MOVABLE));
	struct zone *zone;
	unsigned long flags;

	/* Calculate total number of !ZONE_HIGHMEM pages */
	for_each_zone_(zone) {
		if (!is_highmem(zone))
			lowmem_pages += zone_managed_pages(zone);
	}

	for_each_zone_(zone) {
		u64 tmp, low;

		spin_lock_irqsave(&zone->lock, flags);
		tmp = (u64)pages_min * zone_managed_pages(zone);
		do_div(tmp, lowmem_pages);
		low = (u64)pages_low * zone_managed_pages(zone);
		do_div(low, vm_total_pages_);
		if (is_highmem(zone)) {
			/*
			 * __GFP_HIGH and PF_MEMALLOC allocations usually don't
			 * need highmem pages, so cap pages_min to a small
			 * value here.
			 *
			 * The WMARK_HIGH-WMARK_LOW and (WMARK_LOW-WMARK_MIN)
			 * deltas control async page reclaim, and so should
			 * not be capped for highmem.
			 */
			unsigned long min_pages;

			min_pages = zone_managed_pages(zone) / 1024;
			min_pages = clamp(min_pages, SWAP_CLUSTER_MAX, 128UL);
			zone->_watermark[WMARK_MIN] = min_pages;
		} else {
			/*
			 * If it's a lowmem zone, reserve a number of pages
			 * proportionate to the zone's size.
			 */
			zone->_watermark[WMARK_MIN] = tmp;
		}

		/*
		 * Set the kswapd watermarks distance according to the
		 * scale factor in proportion to available memory, but
		 * ensure a minimum size on small systems.
		 */
		tmp = max_t(u64, tmp >> 2,
			    mult_frac(zone_managed_pages(zone),
				      watermark_scale_factor_, 10000));

		zone->watermark_boost = 0;
		zone->_watermark[WMARK_LOW]  = min_wmark_pages(zone) + low + tmp;
		zone->_watermark[WMARK_HIGH] = low_wmark_pages(zone) + tmp;
		zone->_watermark[WMARK_PROMO] = high_wmark_pages(zone) + tmp;


		spin_unlock_irqrestore(&zone->lock, flags);
	}
}

ssize_t extra_free_kbytes_show(struct kobject *kobj, struct kobj_attribute *attr,
								char *buf)
{
	return sprintf(buf, "min_free_kbytes: %d  watermark_scale_factor: %d"
		" extra_free_kbytes: %d\n", min_free_kbytes_, watermark_scale_factor_,
		extra_free_kbytes);
}

/*
 * Example in cmdline:
 * step1(note: following 2 lines are a long command.):
 *   echo `cat /proc/sys/vm/min_free_kbytes` " " `cat /proc/sys/vm/watermark_scale_factor` " "
 *   `cat /proc/sys/vm/extra_free_kbytes` > /sys/kernel/mi_wmark/extra_free_kbytes
 * step2:
 *   cat /proc/sys/vm/lowmem_reserve_ratio
 */
ssize_t extra_free_kbytes_store(struct kobject *kobj, struct kobj_attribute *attr,
							const char *buf, size_t count)
{
	int extra_free_kbytes_ = -1, min_free_kbytes__ = -1, watermark_scale_factor__ = -1;
	static DEFINE_SPINLOCK(lock);

	spin_lock(&lock);
	if (sscanf(buf, "%d %d %d", &min_free_kbytes__, &watermark_scale_factor__,
		&extra_free_kbytes_) != 3) {
		min_free_kbytes__ = -1;
		watermark_scale_factor__ = -1;
		if (sscanf(buf, "%d", &extra_free_kbytes_) != 1) {
			spin_unlock(&lock);
			return -EINVAL;
		}
	}
	if (extra_free_kbytes_ != -1) {
		extra_free_kbytes = extra_free_kbytes_;
		pages_low =  extra_free_kbytes >> (PAGE_SHIFT - 10);
	}
	if (min_free_kbytes__ != -1)
		min_free_kbytes_ = min_free_kbytes__;
	if (watermark_scale_factor__ != -1)
		watermark_scale_factor_ = watermark_scale_factor__;
	setup_per_zone_wmarks_extra();
	spin_unlock(&lock);
	pr_info("extra_free_kbytes_store pid=%d, comm=%s", current->pid, current->comm);
	return count;
}

static MI_PERF_ATTR_RW(extra_free_kbytes);

static struct attribute *attrs[] = {&kobj_attr_extra_free_kbytes.attr, NULL};

static int mi_wmark_sysfs_create(void)
{
	int err;

	mi_wmark_kobj = kobject_create_and_add("mi_wmark", kernel_kobj);
	if (!mi_wmark_kobj) {
		pr_err("failed to create mi_wmark node.\n");
		return -1;
	}
	err = sysfs_create_files(mi_wmark_kobj, (const struct attribute **)attrs);
	if (err) {
		pr_err("failed to create mi_wmark attrs.\n");
		kobject_put(mi_wmark_kobj);
		return -1;
	}
	return 0;
}

static void mi_wmark_sysfs_destory(void)
{
	if (mi_wmark_kobj != NULL) {
		kobject_put(mi_wmark_kobj);
		mi_wmark_kobj = NULL;
	}
}



int __init mi_wmark_init(void)
{

	if (mi_wmark_sysfs_create())
		return -1;
	vm_total_pages_ = nr_free_zone_pages(gfp_zone_(GFP_HIGHUSER_MOVABLE));
	pr_info("mi_wmark init ok.\n");
	return 0;
}

void __exit mi_wmark_exit(void)
{
	mi_wmark_sysfs_destory();
}

module_init(mi_wmark_init);
module_exit(mi_wmark_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhouwenhao");
MODULE_DESCRIPTION("A moudle to adjust kswapd watermark.");
