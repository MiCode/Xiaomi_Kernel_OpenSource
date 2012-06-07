/* drivers/android/pmem.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2012, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/export.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fmem.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/debugfs.h>
#include <linux/android_pmem.h>
#include <linux/mempolicy.h>
#include <linux/sched.h>
#include <linux/kobject.h>
#include <linux/pm_runtime.h>
#include <linux/memory_alloc.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/mm_types.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/sizes.h>
#include <asm/mach/map.h>
#include <asm/page.h>

#define PMEM_MAX_DEVICES (10)

#define PMEM_MAX_ORDER (128)
#define PMEM_MIN_ALLOC PAGE_SIZE

#define PMEM_INITIAL_NUM_BITMAP_ALLOCATIONS (64)

#define PMEM_32BIT_WORD_ORDER (5)
#define PMEM_BITS_PER_WORD_MASK (BITS_PER_LONG - 1)

#ifdef CONFIG_ANDROID_PMEM_DEBUG
#define PMEM_DEBUG 1
#else
#define PMEM_DEBUG 0
#endif

#define SYSTEM_ALLOC_RETRY 10

/* indicates that a refernce to this file has been taken via get_pmem_file,
 * the file should not be released until put_pmem_file is called */
#define PMEM_FLAGS_BUSY 0x1
/* indicates that this is a suballocation of a larger master range */
#define PMEM_FLAGS_CONNECTED 0x1 << 1
/* indicates this is a master and not a sub allocation and that it is mmaped */
#define PMEM_FLAGS_MASTERMAP 0x1 << 2
/* submap and unsubmap flags indicate:
 * 00: subregion has never been mmaped
 * 10: subregion has been mmaped, reference to the mm was taken
 * 11: subretion has ben released, refernece to the mm still held
 * 01: subretion has been released, reference to the mm has been released
 */
#define PMEM_FLAGS_SUBMAP 0x1 << 3
#define PMEM_FLAGS_UNSUBMAP 0x1 << 4

struct pmem_data {
	/* in alloc mode: an index into the bitmap
	 * in no_alloc mode: the size of the allocation */
	int index;
	/* see flags above for descriptions */
	unsigned int flags;
	/* protects this data field, if the mm_mmap sem will be held at the
	 * same time as this sem, the mm sem must be taken first (as this is
	 * the order for vma_open and vma_close ops */
	struct rw_semaphore sem;
	/* info about the mmaping process */
	struct vm_area_struct *vma;
	/* task struct of the mapping process */
	struct task_struct *task;
	/* process id of teh mapping process */
	pid_t pid;
	/* file descriptor of the master */
	int master_fd;
	/* file struct of the master */
	struct file *master_file;
	/* a list of currently available regions if this is a suballocation */
	struct list_head region_list;
	/* a linked list of data so we can access them for debugging */
	struct list_head list;
#if PMEM_DEBUG
	int ref;
#endif
};

struct pmem_bits {
	unsigned allocated:1;		/* 1 if allocated, 0 if free */
	unsigned order:7;		/* size of the region in pmem space */
};

struct pmem_region_node {
	struct pmem_region region;
	struct list_head list;
};

#define PMEM_DEBUG_MSGS 0
#if PMEM_DEBUG_MSGS
#define DLOG(fmt,args...) \
	do { pr_debug("[%s:%s:%d] "fmt, __FILE__, __func__, __LINE__, \
		    ##args); } \
	while (0)
#else
#define DLOG(x...) do {} while (0)
#endif

enum pmem_align {
	PMEM_ALIGN_4K,
	PMEM_ALIGN_1M,
};

#define PMEM_NAME_SIZE 16

struct alloc_list {
	void *addr;                  /* physical addr of allocation */
	void *aaddr;                 /* aligned physical addr       */
	unsigned int size;           /* total size of allocation    */
	unsigned char __iomem *vaddr; /* Virtual addr                */
	struct list_head allocs;
};

struct pmem_info {
	struct miscdevice dev;
	/* physical start address of the remaped pmem space */
	unsigned long base;
	/* vitual start address of the remaped pmem space */
	unsigned char __iomem *vbase;
	/* total size of the pmem space */
	unsigned long size;
	/* number of entries in the pmem space */
	unsigned long num_entries;
	/* pfn of the garbage page in memory */
	unsigned long garbage_pfn;
	/* which memory type (i.e. SMI, EBI1) this PMEM device is backed by */
	unsigned memory_type;

	char name[PMEM_NAME_SIZE];

	/* index of the garbage page in the pmem space */
	int garbage_index;
	/* reserved virtual address range */
	struct vm_struct *area;

	enum pmem_allocator_type allocator_type;

	int (*allocate)(const int,
			const unsigned long,
			const unsigned int);
	int (*free)(int, int);
	int (*free_space)(int, struct pmem_freespace *);
	unsigned long (*len)(int, struct pmem_data *);
	unsigned long (*start_addr)(int, struct pmem_data *);

	/* actual size of memory element, e.g.: (4 << 10) is 4K */
	unsigned int quantum;

	/* indicates maps of this region should be cached, if a mix of
	 * cached and uncached is desired, set this and open the device with
	 * O_SYNC to get an uncached region */
	unsigned cached;
	unsigned buffered;
	union {
		struct {
			/* in all_or_nothing allocator mode the first mapper
			 * gets the whole space and sets this flag */
			unsigned allocated;
		} all_or_nothing;

		struct {
			/* the buddy allocator bitmap for the region
			 * indicating which entries are allocated and which
			 * are free.
			 */

			struct pmem_bits *buddy_bitmap;
		} buddy_bestfit;

		struct {
			unsigned int bitmap_free; /* # of zero bits/quanta */
			uint32_t *bitmap;
			int32_t bitmap_allocs;
			struct {
				short bit;
				unsigned short quanta;
			} *bitm_alloc;
		} bitmap;

		struct {
			unsigned long used;      /* Bytes currently allocated */
			struct list_head alist;  /* List of allocations       */
		} system_mem;
	} allocator;

	int id;
	struct kobject kobj;

	/* for debugging, creates a list of pmem file structs, the
	 * data_list_mutex should be taken before pmem_data->sem if both are
	 * needed */
	struct mutex data_list_mutex;
	struct list_head data_list;
	/* arena_mutex protects the global allocation arena
	 *
	 * IF YOU TAKE BOTH LOCKS TAKE THEM IN THIS ORDER:
	 * down(pmem_data->sem) => mutex_lock(arena_mutex)
	 */
	struct mutex arena_mutex;

	long (*ioctl)(struct file *, unsigned int, unsigned long);
	int (*release)(struct inode *, struct file *);
	/* reference count of allocations */
	atomic_t allocation_cnt;
	/*
	 * request function for a region when the allocation count goes
	 * from 0 -> 1
	 */
	int (*mem_request)(void *);
	/*
	 * release function for a region when the allocation count goes
	 * from 1 -> 0
	 */
	int (*mem_release)(void *);
	/*
	 * private data for the request/release callback
	 */
	void *region_data;
	/*
	 * map and unmap as needed
	 */
	int map_on_demand;
	/*
	 * memory will be reused through fmem
	 */
	int reusable;
};
#define to_pmem_info_id(a) (container_of(a, struct pmem_info, kobj)->id)

static void ioremap_pmem(int id);
static void pmem_put_region(int id);
static int pmem_get_region(int id);

static struct pmem_info pmem[PMEM_MAX_DEVICES];
static int id_count;

#define PMEM_SYSFS_DIR_NAME "pmem_regions" /* under /sys/kernel/ */
static struct kset *pmem_kset;

#define PMEM_IS_FREE_BUDDY(id, index) \
	(!(pmem[id].allocator.buddy_bestfit.buddy_bitmap[index].allocated))
#define PMEM_BUDDY_ORDER(id, index) \
	(pmem[id].allocator.buddy_bestfit.buddy_bitmap[index].order)
#define PMEM_BUDDY_INDEX(id, index) \
	(index ^ (1 << PMEM_BUDDY_ORDER(id, index)))
#define PMEM_BUDDY_NEXT_INDEX(id, index) \
	(index + (1 << PMEM_BUDDY_ORDER(id, index)))
#define PMEM_OFFSET(index) (index * pmem[id].quantum)
#define PMEM_START_ADDR(id, index) \
	(PMEM_OFFSET(index) + pmem[id].base)
#define PMEM_BUDDY_LEN(id, index) \
	((1 << PMEM_BUDDY_ORDER(id, index)) * pmem[id].quantum)
#define PMEM_END_ADDR(id, index) \
	(PMEM_START_ADDR(id, index) + PMEM_LEN(id, index))
#define PMEM_START_VADDR(id, index) \
	(PMEM_OFFSET(id, index) + pmem[id].vbase)
#define PMEM_END_VADDR(id, index) \
	(PMEM_START_VADDR(id, index) + PMEM_LEN(id, index))
#define PMEM_REVOKED(data) (data->flags & PMEM_FLAGS_REVOKED)
#define PMEM_IS_PAGE_ALIGNED(addr) (!((addr) & (~PAGE_MASK)))
#define PMEM_IS_SUBMAP(data) \
	((data->flags & PMEM_FLAGS_SUBMAP) && \
	(!(data->flags & PMEM_FLAGS_UNSUBMAP)))

static int pmem_release(struct inode *, struct file *);
static int pmem_mmap(struct file *, struct vm_area_struct *);
static int pmem_open(struct inode *, struct file *);
static long pmem_ioctl(struct file *, unsigned int, unsigned long);

struct file_operations pmem_fops = {
	.release = pmem_release,
	.mmap = pmem_mmap,
	.open = pmem_open,
	.unlocked_ioctl = pmem_ioctl,
};

#define PMEM_ATTR(_name, _mode, _show, _store) {            \
	.attr = {.name = __stringify(_name), .mode = _mode }, \
	.show = _show,                                        \
	.store = _store,                                      \
}

struct pmem_attr {
	struct attribute attr;
	ssize_t(*show) (const int id, char * const);
	ssize_t(*store) (const int id, const char * const, const size_t count);
};
#define to_pmem_attr(a) container_of(a, struct pmem_attr, attr)

#define RW_PMEM_ATTR(name)  \
static struct pmem_attr pmem_attr_## name = \
 PMEM_ATTR(name, S_IRUGO | S_IWUSR, show_pmem_## name, store_pmem_## name)

#define RO_PMEM_ATTR(name)  \
static struct pmem_attr pmem_attr_## name = \
 PMEM_ATTR(name, S_IRUGO, show_pmem_## name, NULL)

#define WO_PMEM_ATTR(name)  \
static struct pmem_attr pmem_attr_## name = \
 PMEM_ATTR(name, S_IWUSR, NULL, store_pmem_## name)

static ssize_t show_pmem(struct kobject *kobj,
			struct attribute *attr,
			char *buf)
{
	struct pmem_attr *a = to_pmem_attr(attr);
	return a->show ? a->show(to_pmem_info_id(kobj), buf) : -EIO;
}

static ssize_t store_pmem(struct kobject *kobj, struct attribute *attr,
		     const char *buf, size_t count)
{
	struct pmem_attr *a = to_pmem_attr(attr);
	return a->store ? a->store(to_pmem_info_id(kobj), buf, count) : -EIO;
}

static struct sysfs_ops pmem_ops = {
	.show = show_pmem,
	.store = store_pmem,
};

static ssize_t show_pmem_base(int id, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu(%#lx)\n",
		pmem[id].base, pmem[id].base);
}
RO_PMEM_ATTR(base);

static ssize_t show_pmem_size(int id, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu(%#lx)\n",
		pmem[id].size, pmem[id].size);
}
RO_PMEM_ATTR(size);

static ssize_t show_pmem_allocator_type(int id, char *buf)
{
	switch (pmem[id].allocator_type) {
	case  PMEM_ALLOCATORTYPE_ALLORNOTHING:
		return scnprintf(buf, PAGE_SIZE, "%s\n", "All or Nothing");
	case  PMEM_ALLOCATORTYPE_BUDDYBESTFIT:
		return scnprintf(buf, PAGE_SIZE, "%s\n", "Buddy Bestfit");
	case  PMEM_ALLOCATORTYPE_BITMAP:
		return scnprintf(buf, PAGE_SIZE, "%s\n", "Bitmap");
	case PMEM_ALLOCATORTYPE_SYSTEM:
		return scnprintf(buf, PAGE_SIZE, "%s\n", "System heap");
	default:
		return scnprintf(buf, PAGE_SIZE,
			"??? Invalid allocator type (%d) for this region! "
			"Something isn't right.\n",
			pmem[id].allocator_type);
	}
}
RO_PMEM_ATTR(allocator_type);

static ssize_t show_pmem_mapped_regions(int id, char *buf)
{
	struct list_head *elt;
	int ret;

	ret = scnprintf(buf, PAGE_SIZE,
		      "pid #: mapped regions (offset, len) (offset,len)...\n");

	mutex_lock(&pmem[id].data_list_mutex);
	list_for_each(elt, &pmem[id].data_list) {
		struct pmem_data *data =
			list_entry(elt, struct pmem_data, list);
		struct list_head *elt2;

		down_read(&data->sem);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "pid %u:",
				data->pid);
		list_for_each(elt2, &data->region_list) {
			struct pmem_region_node *region_node = list_entry(elt2,
					struct pmem_region_node,
					list);
			ret += scnprintf(buf + ret, PAGE_SIZE - ret,
					"(%lx,%lx) ",
					region_node->region.offset,
					region_node->region.len);
		}
		up_read(&data->sem);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}
	mutex_unlock(&pmem[id].data_list_mutex);
	return ret;
}
RO_PMEM_ATTR(mapped_regions);

#define PMEM_COMMON_SYSFS_ATTRS \
	&pmem_attr_base.attr, \
	&pmem_attr_size.attr, \
	&pmem_attr_allocator_type.attr, \
	&pmem_attr_mapped_regions.attr


static ssize_t show_pmem_allocated(int id, char *buf)
{
	ssize_t ret;

	mutex_lock(&pmem[id].arena_mutex);
	ret = scnprintf(buf, PAGE_SIZE, "%s\n",
		pmem[id].allocator.all_or_nothing.allocated ?
		"is allocated" : "is NOT allocated");
	mutex_unlock(&pmem[id].arena_mutex);
	return ret;
}
RO_PMEM_ATTR(allocated);

static struct attribute *pmem_allornothing_attrs[] = {
	PMEM_COMMON_SYSFS_ATTRS,

	&pmem_attr_allocated.attr,

	NULL
};

static struct kobj_type pmem_allornothing_ktype = {
	.sysfs_ops = &pmem_ops,
	.default_attrs = pmem_allornothing_attrs,
};

static ssize_t show_pmem_total_entries(int id, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%lu\n", pmem[id].num_entries);
}
RO_PMEM_ATTR(total_entries);

static ssize_t show_pmem_quantum_size(int id, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u (%#x)\n",
		pmem[id].quantum, pmem[id].quantum);
}
RO_PMEM_ATTR(quantum_size);

static ssize_t show_pmem_buddy_bitmap_dump(int id, char *buf)
{
	int ret, i;

	mutex_lock(&pmem[id].data_list_mutex);
	ret = scnprintf(buf, PAGE_SIZE, "index\torder\tlength\tallocated\n");

	for (i = 0; i < pmem[id].num_entries && (PAGE_SIZE - ret);
			i = PMEM_BUDDY_NEXT_INDEX(id, i))
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%d\t%d\t%d\t%d\n",
			i, PMEM_BUDDY_ORDER(id, i),
			PMEM_BUDDY_LEN(id, i),
			!PMEM_IS_FREE_BUDDY(id, i));

	mutex_unlock(&pmem[id].data_list_mutex);
	return ret;
}
RO_PMEM_ATTR(buddy_bitmap_dump);

#define PMEM_BITMAP_BUDDY_BESTFIT_COMMON_SYSFS_ATTRS \
	&pmem_attr_quantum_size.attr, \
	&pmem_attr_total_entries.attr

static struct attribute *pmem_buddy_bestfit_attrs[] = {
	PMEM_COMMON_SYSFS_ATTRS,

	PMEM_BITMAP_BUDDY_BESTFIT_COMMON_SYSFS_ATTRS,

	&pmem_attr_buddy_bitmap_dump.attr,

	NULL
};

static struct kobj_type pmem_buddy_bestfit_ktype = {
	.sysfs_ops = &pmem_ops,
	.default_attrs = pmem_buddy_bestfit_attrs,
};

static ssize_t show_pmem_free_quanta(int id, char *buf)
{
	ssize_t ret;

	mutex_lock(&pmem[id].arena_mutex);
	ret = scnprintf(buf, PAGE_SIZE, "%u\n",
		pmem[id].allocator.bitmap.bitmap_free);
	mutex_unlock(&pmem[id].arena_mutex);
	return ret;
}
RO_PMEM_ATTR(free_quanta);

static ssize_t show_pmem_bits_allocated(int id, char *buf)
{
	ssize_t ret;
	unsigned int i;

	mutex_lock(&pmem[id].arena_mutex);

	ret = scnprintf(buf, PAGE_SIZE,
		"id: %d\nbitnum\tindex\tquanta allocated\n", id);

	for (i = 0; i < pmem[id].allocator.bitmap.bitmap_allocs; i++)
		if (pmem[id].allocator.bitmap.bitm_alloc[i].bit != -1)
			ret += scnprintf(buf + ret, PAGE_SIZE - ret,
				"%u\t%u\t%u\n",
				i,
				pmem[id].allocator.bitmap.bitm_alloc[i].bit,
				pmem[id].allocator.bitmap.bitm_alloc[i].quanta
				);

	mutex_unlock(&pmem[id].arena_mutex);
	return ret;
}
RO_PMEM_ATTR(bits_allocated);

static struct attribute *pmem_bitmap_attrs[] = {
	PMEM_COMMON_SYSFS_ATTRS,

	PMEM_BITMAP_BUDDY_BESTFIT_COMMON_SYSFS_ATTRS,

	&pmem_attr_free_quanta.attr,
	&pmem_attr_bits_allocated.attr,

	NULL
};

static struct attribute *pmem_system_attrs[] = {
	PMEM_COMMON_SYSFS_ATTRS,

	NULL
};

static struct kobj_type pmem_bitmap_ktype = {
	.sysfs_ops = &pmem_ops,
	.default_attrs = pmem_bitmap_attrs,
};

static struct kobj_type pmem_system_ktype = {
	.sysfs_ops = &pmem_ops,
	.default_attrs = pmem_system_attrs,
};

static int pmem_allocate_from_id(const int id, const unsigned long size,
						const unsigned int align)
{
	int ret;
	ret = pmem_get_region(id);

	if (ret)
		return -1;

	ret = pmem[id].allocate(id, size, align);

	if (ret < 0)
		pmem_put_region(id);

	return ret;
}

static int pmem_free_from_id(const int id, const int index)
{
	pmem_put_region(id);
	return pmem[id].free(id, index);
}

static int pmem_get_region(int id)
{
	/* Must be called with arena mutex locked */
	atomic_inc(&pmem[id].allocation_cnt);
	if (!pmem[id].vbase) {
		DLOG("PMEMDEBUG: mapping for %s", pmem[id].name);
		if (pmem[id].mem_request) {
			int ret = pmem[id].mem_request(pmem[id].region_data);
			if (ret) {
				atomic_dec(&pmem[id].allocation_cnt);
				return 1;
			}
		}
		ioremap_pmem(id);
	}

	if (pmem[id].vbase) {
		return 0;
	} else {
		if (pmem[id].mem_release)
			pmem[id].mem_release(pmem[id].region_data);
		atomic_dec(&pmem[id].allocation_cnt);
		return 1;
	}
}

static void pmem_put_region(int id)
{
	/* Must be called with arena mutex locked */
	if (atomic_dec_and_test(&pmem[id].allocation_cnt)) {
		DLOG("PMEMDEBUG: unmapping for %s", pmem[id].name);
		BUG_ON(!pmem[id].vbase);
		if (pmem[id].map_on_demand) {
			/* unmap_kernel_range() flushes the caches
			 * and removes the page table entries
			 */
			unmap_kernel_range((unsigned long)pmem[id].vbase,
				 pmem[id].size);
			pmem[id].vbase = NULL;
			if (pmem[id].mem_release) {
				int ret = pmem[id].mem_release(
						pmem[id].region_data);
				WARN(ret, "mem_release failed");
			}

		}
	}
}

static int get_id(struct file *file)
{
	return MINOR(file->f_dentry->d_inode->i_rdev);
}

static char *get_name(struct file *file)
{
	int id = get_id(file);
	return pmem[id].name;
}

static int is_pmem_file(struct file *file)
{
	int id;

	if (unlikely(!file || !file->f_dentry || !file->f_dentry->d_inode))
		return 0;

	id = get_id(file);
	return (unlikely(id >= PMEM_MAX_DEVICES ||
		file->f_dentry->d_inode->i_rdev !=
		     MKDEV(MISC_MAJOR, pmem[id].dev.minor))) ? 0 : 1;
}

static int has_allocation(struct file *file)
{
	/* must be called with at least read lock held on
	 * ((struct pmem_data *)(file->private_data))->sem which
	 * means that file is guaranteed not to be NULL upon entry!!
	 * check is_pmem_file first if not accessed via pmem_file_ops */
	struct pmem_data *pdata = file->private_data;
	return pdata && pdata->index != -1;
}

static int is_master_owner(struct file *file)
{
	struct file *master_file;
	struct pmem_data *data = file->private_data;
	int put_needed, ret = 0;

	if (!has_allocation(file))
		return 0;
	if (PMEM_FLAGS_MASTERMAP & data->flags)
		return 1;
	master_file = fget_light(data->master_fd, &put_needed);
	if (master_file && data->master_file == master_file)
		ret = 1;
	if (master_file)
		fput_light(master_file, put_needed);
	return ret;
}

static int pmem_free_all_or_nothing(int id, int index)
{
	/* caller should hold the lock on arena_mutex! */
	DLOG("index %d\n", index);

	pmem[id].allocator.all_or_nothing.allocated =  0;
	return 0;
}

static int pmem_free_space_all_or_nothing(int id,
		struct pmem_freespace *fs)
{
	/* caller should hold the lock on arena_mutex! */
	fs->total = (unsigned long)
		pmem[id].allocator.all_or_nothing.allocated == 0 ?
		pmem[id].size : 0;

	fs->largest = fs->total;
	return 0;
}


static int pmem_free_buddy_bestfit(int id, int index)
{
	/* caller should hold the lock on arena_mutex! */
	int curr = index;
	DLOG("index %d\n", index);


	/* clean up the bitmap, merging any buddies */
	pmem[id].allocator.buddy_bestfit.buddy_bitmap[curr].allocated = 0;
	/* find a slots buddy Buddy# = Slot# ^ (1 << order)
	 * if the buddy is also free merge them
	 * repeat until the buddy is not free or end of the bitmap is reached
	 */
	do {
		int buddy = PMEM_BUDDY_INDEX(id, curr);
		if (buddy < pmem[id].num_entries &&
		    PMEM_IS_FREE_BUDDY(id, buddy) &&
		    PMEM_BUDDY_ORDER(id, buddy) ==
				PMEM_BUDDY_ORDER(id, curr)) {
			PMEM_BUDDY_ORDER(id, buddy)++;
			PMEM_BUDDY_ORDER(id, curr)++;
			curr = min(buddy, curr);
		} else {
			break;
		}
	} while (curr < pmem[id].num_entries);

	return 0;
}


static int pmem_free_space_buddy_bestfit(int id,
		struct pmem_freespace *fs)
{
	/* caller should hold the lock on arena_mutex! */
	int curr;
	unsigned long size;
	fs->total = 0;
	fs->largest = 0;

	for (curr = 0; curr < pmem[id].num_entries;
	     curr = PMEM_BUDDY_NEXT_INDEX(id, curr)) {
		if (PMEM_IS_FREE_BUDDY(id, curr)) {
			size = PMEM_BUDDY_LEN(id, curr);
			if (size > fs->largest)
				fs->largest = size;
			fs->total += size;
		}
	}
	return 0;
}


static inline uint32_t start_mask(int bit_start)
{
	return (uint32_t)(~0) << (bit_start & PMEM_BITS_PER_WORD_MASK);
}

static inline uint32_t end_mask(int bit_end)
{
	return (uint32_t)(~0) >>
		((BITS_PER_LONG - bit_end) & PMEM_BITS_PER_WORD_MASK);
}

static inline int compute_total_words(int bit_end, int word_index)
{
	return ((bit_end + BITS_PER_LONG - 1) >>
			PMEM_32BIT_WORD_ORDER) - word_index;
}

static void bitmap_bits_clear_all(uint32_t *bitp, int bit_start, int bit_end)
{
	int word_index = bit_start >> PMEM_32BIT_WORD_ORDER, total_words;

	total_words = compute_total_words(bit_end, word_index);
	if (total_words > 0) {
		if (total_words == 1) {
			bitp[word_index] &=
				~(start_mask(bit_start) & end_mask(bit_end));
		} else {
			bitp[word_index++] &= ~start_mask(bit_start);
			if (total_words > 2) {
				int total_bytes;

				total_words -= 2;
				total_bytes = total_words << 2;

				memset(&bitp[word_index], 0, total_bytes);
				word_index += total_words;
			}
			bitp[word_index] &= ~end_mask(bit_end);
		}
	}
}

static int pmem_free_bitmap(int id, int bitnum)
{
	/* caller should hold the lock on arena_mutex! */
	int i;
	char currtask_name[FIELD_SIZEOF(struct task_struct, comm) + 1];

	DLOG("bitnum %d\n", bitnum);

	for (i = 0; i < pmem[id].allocator.bitmap.bitmap_allocs; i++) {
		const int curr_bit =
			pmem[id].allocator.bitmap.bitm_alloc[i].bit;

		if (curr_bit == bitnum) {
			const int curr_quanta =
				pmem[id].allocator.bitmap.bitm_alloc[i].quanta;

			bitmap_bits_clear_all(pmem[id].allocator.bitmap.bitmap,
				curr_bit, curr_bit + curr_quanta);
			pmem[id].allocator.bitmap.bitmap_free += curr_quanta;
			pmem[id].allocator.bitmap.bitm_alloc[i].bit = -1;
			pmem[id].allocator.bitmap.bitm_alloc[i].quanta = 0;
			return 0;
		}
	}
	printk(KERN_ALERT "pmem: %s: Attempt to free unallocated index %d, id"
		" %d, pid %d(%s)\n", __func__, bitnum, id,  current->pid,
		get_task_comm(currtask_name, current));

	return -1;
}

static int pmem_free_system(int id, int index)
{
	/* caller should hold the lock on arena_mutex! */
	struct alloc_list *item;

	DLOG("index %d\n", index);
	if (index != 0)
		item = (struct alloc_list *)index;
	else
		return 0;

	if (item->vaddr != NULL) {
		iounmap(item->vaddr);
		kfree(__va(item->addr));
		list_del(&item->allocs);
		kfree(item);
	}

	return 0;
}

static int pmem_free_space_bitmap(int id, struct pmem_freespace *fs)
{
	int i, j;
	int max_allocs = pmem[id].allocator.bitmap.bitmap_allocs;
	int alloc_start = 0;
	int next_alloc;
	unsigned long size = 0;

	fs->total = 0;
	fs->largest = 0;

	for (i = 0; i < max_allocs; i++) {

		int alloc_quanta = 0;
		int alloc_idx = 0;
		next_alloc = pmem[id].num_entries;

		/* Look for the lowest bit where next allocation starts */
		for (j = 0; j < max_allocs; j++) {
			const int curr_alloc = pmem[id].allocator.
						bitmap.bitm_alloc[j].bit;
			if (curr_alloc != -1) {
				if (alloc_start == curr_alloc)
					alloc_idx = j;
				if (alloc_start >= curr_alloc)
					continue;
				if (curr_alloc < next_alloc)
					next_alloc = curr_alloc;
			}
		}
		alloc_quanta = pmem[id].allocator.bitmap.
				bitm_alloc[alloc_idx].quanta;
		size = (next_alloc - (alloc_start + alloc_quanta)) *
				pmem[id].quantum;

		if (size > fs->largest)
			fs->largest = size;
		fs->total += size;

		if (next_alloc == pmem[id].num_entries)
			break;
		else
			alloc_start = next_alloc;
	}

	return 0;
}

static int pmem_free_space_system(int id, struct pmem_freespace *fs)
{
	fs->total = pmem[id].size;
	fs->largest = pmem[id].size;

	return 0;
}

static void pmem_revoke(struct file *file, struct pmem_data *data);

static int pmem_release(struct inode *inode, struct file *file)
{
	struct pmem_data *data = file->private_data;
	struct pmem_region_node *region_node;
	struct list_head *elt, *elt2;
	int id = get_id(file), ret = 0;

#if PMEM_DEBUG_MSGS
	char currtask_name[FIELD_SIZEOF(struct task_struct, comm) + 1];
#endif
	DLOG("releasing memory pid %u(%s) file %p(%ld) dev %s(id: %d)\n",
		current->pid, get_task_comm(currtask_name, current),
		file, file_count(file), get_name(file), id);
	mutex_lock(&pmem[id].data_list_mutex);
	/* if this file is a master, revoke all the memory in the connected
	 *  files */
	if (PMEM_FLAGS_MASTERMAP & data->flags) {
		list_for_each(elt, &pmem[id].data_list) {
			struct pmem_data *sub_data =
				list_entry(elt, struct pmem_data, list);
			int is_master;

			down_read(&sub_data->sem);
			is_master = (PMEM_IS_SUBMAP(sub_data) &&
				file == sub_data->master_file);
			up_read(&sub_data->sem);

			if (is_master)
				pmem_revoke(file, sub_data);
		}
	}
	list_del(&data->list);
	mutex_unlock(&pmem[id].data_list_mutex);

	down_write(&data->sem);

	/* if it is not a connected file and it has an allocation, free it */
	if (!(PMEM_FLAGS_CONNECTED & data->flags) && has_allocation(file)) {
		mutex_lock(&pmem[id].arena_mutex);
		ret = pmem_free_from_id(id, data->index);
		mutex_unlock(&pmem[id].arena_mutex);
	}

	/* if this file is a submap (mapped, connected file), downref the
	 * task struct */
	if (PMEM_FLAGS_SUBMAP & data->flags)
		if (data->task) {
			put_task_struct(data->task);
			data->task = NULL;
		}

	file->private_data = NULL;

	list_for_each_safe(elt, elt2, &data->region_list) {
		region_node = list_entry(elt, struct pmem_region_node, list);
		list_del(elt);
		kfree(region_node);
	}
	BUG_ON(!list_empty(&data->region_list));

	up_write(&data->sem);
	kfree(data);
	if (pmem[id].release)
		ret = pmem[id].release(inode, file);

	return ret;
}

static int pmem_open(struct inode *inode, struct file *file)
{
	struct pmem_data *data;
	int id = get_id(file);
	int ret = 0;
#if PMEM_DEBUG_MSGS
	char currtask_name[FIELD_SIZEOF(struct task_struct, comm) + 1];
#endif

	DLOG("pid %u(%s) file %p(%ld) dev %s(id: %d)\n",
		current->pid, get_task_comm(currtask_name, current),
		file, file_count(file), get_name(file), id);
	data = kmalloc(sizeof(struct pmem_data), GFP_KERNEL);
	if (!data) {
		printk(KERN_ALERT "pmem: %s: unable to allocate memory for "
				"pmem metadata.", __func__);
		return -1;
	}
	data->flags = 0;
	data->index = -1;
	data->task = NULL;
	data->vma = NULL;
	data->pid = 0;
	data->master_file = NULL;
#if PMEM_DEBUG
	data->ref = 0;
#endif
	INIT_LIST_HEAD(&data->region_list);
	init_rwsem(&data->sem);

	file->private_data = data;
	INIT_LIST_HEAD(&data->list);

	mutex_lock(&pmem[id].data_list_mutex);
	list_add(&data->list, &pmem[id].data_list);
	mutex_unlock(&pmem[id].data_list_mutex);
	return ret;
}

static unsigned long pmem_order(unsigned long len, int id)
{
	int i;

	len = (len + pmem[id].quantum - 1)/pmem[id].quantum;
	len--;
	for (i = 0; i < sizeof(len)*8; i++)
		if (len >> i == 0)
			break;
	return i;
}

static int pmem_allocator_all_or_nothing(const int id,
		const unsigned long len,
		const unsigned int align)
{
	/* caller should hold the lock on arena_mutex! */
	DLOG("all or nothing\n");
	if ((len > pmem[id].size) ||
		pmem[id].allocator.all_or_nothing.allocated)
		return -1;
	pmem[id].allocator.all_or_nothing.allocated = 1;
	return len;
}

static int pmem_allocator_buddy_bestfit(const int id,
		const unsigned long len,
		unsigned int align)
{
	/* caller should hold the lock on arena_mutex! */
	int curr;
	int best_fit = -1;
	unsigned long order;

	DLOG("buddy bestfit\n");
	order = pmem_order(len, id);
	if (order > PMEM_MAX_ORDER)
		goto out;

	DLOG("order %lx\n", order);

	/* Look through the bitmap.
	 * 	If a free slot of the correct order is found, use it.
	 * 	Otherwise, use the best fit (smallest with size > order) slot.
	 */
	for (curr = 0;
	     curr < pmem[id].num_entries;
	     curr = PMEM_BUDDY_NEXT_INDEX(id, curr))
		if (PMEM_IS_FREE_BUDDY(id, curr)) {
			if (PMEM_BUDDY_ORDER(id, curr) ==
					(unsigned char)order) {
				/* set the not free bit and clear others */
				best_fit = curr;
				break;
			}
			if (PMEM_BUDDY_ORDER(id, curr) >
					(unsigned char)order &&
			    (best_fit < 0 ||
			     PMEM_BUDDY_ORDER(id, curr) <
					PMEM_BUDDY_ORDER(id, best_fit)))
				best_fit = curr;
		}

	/* if best_fit < 0, there are no suitable slots; return an error */
	if (best_fit < 0) {
#if PMEM_DEBUG
		printk(KERN_ALERT "pmem: %s: no space left to allocate!\n",
			__func__);
#endif
		goto out;
	}

	/* now partition the best fit:
	 * 	split the slot into 2 buddies of order - 1
	 * 	repeat until the slot is of the correct order
	 */
	while (PMEM_BUDDY_ORDER(id, best_fit) > (unsigned char)order) {
		int buddy;
		PMEM_BUDDY_ORDER(id, best_fit) -= 1;
		buddy = PMEM_BUDDY_INDEX(id, best_fit);
		PMEM_BUDDY_ORDER(id, buddy) = PMEM_BUDDY_ORDER(id, best_fit);
	}
	pmem[id].allocator.buddy_bestfit.buddy_bitmap[best_fit].allocated = 1;
out:
	return best_fit;
}


static inline unsigned long paddr_from_bit(const int id, const int bitnum)
{
	return pmem[id].base + pmem[id].quantum * bitnum;
}

static inline unsigned long bit_from_paddr(const int id,
		const unsigned long paddr)
{
	return (paddr - pmem[id].base) / pmem[id].quantum;
}

static void bitmap_bits_set_all(uint32_t *bitp, int bit_start, int bit_end)
{
	int word_index = bit_start >> PMEM_32BIT_WORD_ORDER, total_words;

	total_words = compute_total_words(bit_end, word_index);
	if (total_words > 0) {
		if (total_words == 1) {
			bitp[word_index] |=
				(start_mask(bit_start) & end_mask(bit_end));
		} else {
			bitp[word_index++] |= start_mask(bit_start);
			if (total_words > 2) {
				int total_bytes;

				total_words -= 2;
				total_bytes = total_words << 2;

				memset(&bitp[word_index], ~0, total_bytes);
				word_index += total_words;
			}
			bitp[word_index] |= end_mask(bit_end);
		}
	}
}

static int
bitmap_allocate_contiguous(uint32_t *bitp, int num_bits_to_alloc,
		int total_bits, int spacing, int start_bit)
{
	int bit_start, last_bit, word_index;

	if (num_bits_to_alloc <= 0)
		return -1;

	for (bit_start = start_bit; ;
		bit_start = ((last_bit +
			(word_index << PMEM_32BIT_WORD_ORDER) + spacing - 1)
			& ~(spacing - 1)) + start_bit) {
		int bit_end = bit_start + num_bits_to_alloc, total_words;

		if (bit_end > total_bits)
			return -1; /* out of contiguous memory */

		word_index = bit_start >> PMEM_32BIT_WORD_ORDER;
		total_words = compute_total_words(bit_end, word_index);

		if (total_words <= 0)
			return -1;

		if (total_words == 1) {
			last_bit = fls(bitp[word_index] &
					(start_mask(bit_start) &
						end_mask(bit_end)));
			if (last_bit)
				continue;
		} else {
			int end_word = word_index + (total_words - 1);
			last_bit =
				fls(bitp[word_index] & start_mask(bit_start));
			if (last_bit)
				continue;

			for (word_index++;
					word_index < end_word;
					word_index++) {
				last_bit = fls(bitp[word_index]);
				if (last_bit)
					break;
			}
			if (last_bit)
				continue;

			last_bit = fls(bitp[word_index] & end_mask(bit_end));
			if (last_bit)
				continue;
		}
		bitmap_bits_set_all(bitp, bit_start, bit_end);
		return bit_start;
	}
	return -1;
}

static int reserve_quanta(const unsigned int quanta_needed,
		const int id,
		unsigned int align)
{
	/* alignment should be a valid power of 2 */
	int ret = -1, start_bit = 0, spacing = 1;

	/* Sanity check */
	if (quanta_needed > pmem[id].allocator.bitmap.bitmap_free) {
#if PMEM_DEBUG
		printk(KERN_ALERT "pmem: %s: request (%d) too big for"
			" available free (%d)\n", __func__, quanta_needed,
			pmem[id].allocator.bitmap.bitmap_free);
#endif
		return -1;
	}

	start_bit = bit_from_paddr(id,
		(pmem[id].base + align - 1) & ~(align - 1));
	if (start_bit <= -1) {
#if PMEM_DEBUG
		printk(KERN_ALERT
			"pmem: %s: bit_from_paddr fails for"
			" %u alignment.\n", __func__, align);
#endif
		return -1;
	}
	spacing = align / pmem[id].quantum;
	spacing = spacing > 1 ? spacing : 1;

	ret = bitmap_allocate_contiguous(pmem[id].allocator.bitmap.bitmap,
		quanta_needed,
		(pmem[id].size + pmem[id].quantum - 1) / pmem[id].quantum,
		spacing,
		start_bit);

#if PMEM_DEBUG
	if (ret < 0)
		printk(KERN_ALERT "pmem: %s: not enough contiguous bits free "
			"in bitmap! Region memory is either too fragmented or"
			" request is too large for available memory.\n",
			__func__);
#endif

	return ret;
}

static int pmem_allocator_bitmap(const int id,
		const unsigned long len,
		const unsigned int align)
{
	/* caller should hold the lock on arena_mutex! */
	int bitnum, i;
	unsigned int quanta_needed;

	DLOG("bitmap id %d, len %ld, align %u\n", id, len, align);
	if (!pmem[id].allocator.bitmap.bitm_alloc) {
#if PMEM_DEBUG
		printk(KERN_ALERT "pmem: bitm_alloc not present! id: %d\n",
			id);
#endif
		return -1;
	}

	quanta_needed = (len + pmem[id].quantum - 1) / pmem[id].quantum;
	DLOG("quantum size %u quanta needed %u free %u id %d\n",
		pmem[id].quantum, quanta_needed,
		pmem[id].allocator.bitmap.bitmap_free, id);

	if (pmem[id].allocator.bitmap.bitmap_free < quanta_needed) {
#if PMEM_DEBUG
		printk(KERN_ALERT "pmem: memory allocation failure. "
			"PMEM memory region exhausted, id %d."
			" Unable to comply with allocation request.\n", id);
#endif
		return -1;
	}

	bitnum = reserve_quanta(quanta_needed, id, align);
	if (bitnum == -1)
		goto leave;

	for (i = 0;
		i < pmem[id].allocator.bitmap.bitmap_allocs &&
			pmem[id].allocator.bitmap.bitm_alloc[i].bit != -1;
		i++)
		;

	if (i >= pmem[id].allocator.bitmap.bitmap_allocs) {
		void *temp;
		int32_t new_bitmap_allocs =
			pmem[id].allocator.bitmap.bitmap_allocs << 1;
		int j;

		if (!new_bitmap_allocs) { /* failed sanity check!! */
#if PMEM_DEBUG
			pr_alert("pmem: bitmap_allocs number"
				" wrapped around to zero! Something "
				"is VERY wrong.\n");
#endif
			return -1;
		}

		if (new_bitmap_allocs > pmem[id].num_entries) {
			/* failed sanity check!! */
#if PMEM_DEBUG
			pr_alert("pmem: required bitmap_allocs"
				" number exceeds maximum entries possible"
				" for current quanta\n");
#endif
			return -1;
		}

		temp = krealloc(pmem[id].allocator.bitmap.bitm_alloc,
				new_bitmap_allocs *
				sizeof(*pmem[id].allocator.bitmap.bitm_alloc),
				GFP_KERNEL);
		if (!temp) {
#if PMEM_DEBUG
			pr_alert("pmem: can't realloc bitmap_allocs,"
				"id %d, current num bitmap allocs %d\n",
				id, pmem[id].allocator.bitmap.bitmap_allocs);
#endif
			return -1;
		}
		pmem[id].allocator.bitmap.bitmap_allocs = new_bitmap_allocs;
		pmem[id].allocator.bitmap.bitm_alloc = temp;

		for (j = i; j < new_bitmap_allocs; j++) {
			pmem[id].allocator.bitmap.bitm_alloc[j].bit = -1;
			pmem[id].allocator.bitmap.bitm_alloc[i].quanta = 0;
		}

		DLOG("increased # of allocated regions to %d for id %d\n",
			pmem[id].allocator.bitmap.bitmap_allocs, id);
	}

	DLOG("bitnum %d, bitm_alloc index %d\n", bitnum, i);

	pmem[id].allocator.bitmap.bitmap_free -= quanta_needed;
	pmem[id].allocator.bitmap.bitm_alloc[i].bit = bitnum;
	pmem[id].allocator.bitmap.bitm_alloc[i].quanta = quanta_needed;
leave:
	return bitnum;
}

static int pmem_allocator_system(const int id,
		const unsigned long len,
		const unsigned int align)
{
	/* caller should hold the lock on arena_mutex! */
	struct alloc_list *list;
	unsigned long aligned_len;
	int count = SYSTEM_ALLOC_RETRY;
	void *buf;

	DLOG("system id %d, len %ld, align %u\n", id, len, align);

	if ((pmem[id].allocator.system_mem.used + len) > pmem[id].size) {
		DLOG("requested size would be larger than quota\n");
		return -1;
	}

	/* Handle alignment */
	aligned_len = len + align;

	/* Attempt allocation */
	list = kmalloc(sizeof(struct alloc_list), GFP_KERNEL);
	if (list == NULL) {
		printk(KERN_ERR "pmem: failed to allocate system metadata\n");
		return -1;
	}
	list->vaddr = NULL;

	buf = NULL;
	while ((buf == NULL) && count--) {
		buf = kmalloc((aligned_len), GFP_KERNEL);
		if (buf == NULL) {
			DLOG("pmem: kmalloc %d temporarily failed len= %ld\n",
				count, aligned_len);
		}
	}
	if (!buf) {
		printk(KERN_CRIT "pmem: kmalloc failed for id= %d len= %ld\n",
			id, aligned_len);
		kfree(list);
		return -1;
	}
	list->size = aligned_len;
	list->addr = (void *)__pa(buf);
	list->aaddr = (void *)(((unsigned int)(list->addr) + (align - 1)) &
			~(align - 1));

	if (!pmem[id].cached)
		list->vaddr = ioremap(__pa(buf), aligned_len);
	else
		list->vaddr = ioremap_cached(__pa(buf), aligned_len);

	INIT_LIST_HEAD(&list->allocs);
	list_add(&list->allocs, &pmem[id].allocator.system_mem.alist);

	return (int)list;
}

static pgprot_t pmem_phys_mem_access_prot(struct file *file, pgprot_t vma_prot)
{
	int id = get_id(file);
#ifdef pgprot_writecombine
	if (pmem[id].cached == 0 || file->f_flags & O_SYNC)
		/* on ARMv6 and ARMv7 this expands to Normal Noncached */
		return pgprot_writecombine(vma_prot);
#endif
#ifdef pgprot_ext_buffered
	else if (pmem[id].buffered)
		return pgprot_ext_buffered(vma_prot);
#endif
	return vma_prot;
}

static unsigned long pmem_start_addr_all_or_nothing(int id,
		struct pmem_data *data)
{
	return PMEM_START_ADDR(id, 0);
}

static unsigned long pmem_start_addr_buddy_bestfit(int id,
		struct pmem_data *data)
{
	return PMEM_START_ADDR(id, data->index);
}

static unsigned long pmem_start_addr_bitmap(int id, struct pmem_data *data)
{
	return data->index * pmem[id].quantum + pmem[id].base;
}

static unsigned long pmem_start_addr_system(int id, struct pmem_data *data)
{
	return (unsigned long)(((struct alloc_list *)(data->index))->aaddr);
}

static void *pmem_start_vaddr(int id, struct pmem_data *data)
{
	if (pmem[id].allocator_type == PMEM_ALLOCATORTYPE_SYSTEM)
		return ((struct alloc_list *)(data->index))->vaddr;
	else
	return pmem[id].start_addr(id, data) - pmem[id].base + pmem[id].vbase;
}

static unsigned long pmem_len_all_or_nothing(int id, struct pmem_data *data)
{
	return data->index;
}

static unsigned long pmem_len_buddy_bestfit(int id, struct pmem_data *data)
{
	return PMEM_BUDDY_LEN(id, data->index);
}

static unsigned long pmem_len_bitmap(int id, struct pmem_data *data)
{
	int i;
	unsigned long ret = 0;

	mutex_lock(&pmem[id].arena_mutex);

	for (i = 0; i < pmem[id].allocator.bitmap.bitmap_allocs; i++)
		if (pmem[id].allocator.bitmap.bitm_alloc[i].bit ==
				data->index) {
			ret = pmem[id].allocator.bitmap.bitm_alloc[i].quanta *
				pmem[id].quantum;
			break;
		}

	mutex_unlock(&pmem[id].arena_mutex);
#if PMEM_DEBUG
	if (i >= pmem[id].allocator.bitmap.bitmap_allocs)
		pr_alert("pmem: %s: can't find bitnum %d in "
			"alloc'd array!\n", __func__, data->index);
#endif
	return ret;
}

static unsigned long pmem_len_system(int id, struct pmem_data *data)
{
	unsigned long ret = 0;

	mutex_lock(&pmem[id].arena_mutex);

	ret = ((struct alloc_list *)data->index)->size;
	mutex_unlock(&pmem[id].arena_mutex);

	return ret;
}

static int pmem_map_garbage(int id, struct vm_area_struct *vma,
			    struct pmem_data *data, unsigned long offset,
			    unsigned long len)
{
	int i, garbage_pages = len >> PAGE_SHIFT;

	vma->vm_flags |= VM_IO | VM_RESERVED | VM_PFNMAP | VM_SHARED | VM_WRITE;
	for (i = 0; i < garbage_pages; i++) {
		if (vm_insert_pfn(vma, vma->vm_start + offset + (i * PAGE_SIZE),
		    pmem[id].garbage_pfn))
			return -EAGAIN;
	}
	return 0;
}

static int pmem_unmap_pfn_range(int id, struct vm_area_struct *vma,
				struct pmem_data *data, unsigned long offset,
				unsigned long len)
{
	int garbage_pages;
	DLOG("unmap offset %lx len %lx\n", offset, len);

	BUG_ON(!PMEM_IS_PAGE_ALIGNED(len));

	garbage_pages = len >> PAGE_SHIFT;
	zap_page_range(vma, vma->vm_start + offset, len, NULL);
	pmem_map_garbage(id, vma, data, offset, len);
	return 0;
}

static int pmem_map_pfn_range(int id, struct vm_area_struct *vma,
			      struct pmem_data *data, unsigned long offset,
			      unsigned long len)
{
	int ret;
	DLOG("map offset %lx len %lx\n", offset, len);
	BUG_ON(!PMEM_IS_PAGE_ALIGNED(vma->vm_start));
	BUG_ON(!PMEM_IS_PAGE_ALIGNED(vma->vm_end));
	BUG_ON(!PMEM_IS_PAGE_ALIGNED(len));
	BUG_ON(!PMEM_IS_PAGE_ALIGNED(offset));

	ret = io_remap_pfn_range(vma, vma->vm_start + offset,
		(pmem[id].start_addr(id, data) + offset) >> PAGE_SHIFT,
		len, vma->vm_page_prot);
	if (ret) {
#if PMEM_DEBUG
		pr_alert("pmem: %s: io_remap_pfn_range fails with "
			"return value: %d!\n",	__func__, ret);
#endif

		ret = -EAGAIN;
	}
	return ret;
}

static int pmem_remap_pfn_range(int id, struct vm_area_struct *vma,
			      struct pmem_data *data, unsigned long offset,
			      unsigned long len)
{
	/* hold the mm semp for the vma you are modifying when you call this */
	BUG_ON(!vma);
	zap_page_range(vma, vma->vm_start + offset, len, NULL);
	return pmem_map_pfn_range(id, vma, data, offset, len);
}

static void pmem_vma_open(struct vm_area_struct *vma)
{
	struct file *file = vma->vm_file;
	struct pmem_data *data = file->private_data;
	int id = get_id(file);

#if PMEM_DEBUG_MSGS
	char currtask_name[FIELD_SIZEOF(struct task_struct, comm) + 1];
#endif
	DLOG("Dev %s(id: %d) pid %u(%s) ppid %u file %p count %ld\n",
		get_name(file), id, current->pid,
		get_task_comm(currtask_name, current),
		current->parent->pid, file, file_count(file));
	/* this should never be called as we don't support copying pmem
	 * ranges via fork */
	down_read(&data->sem);
	BUG_ON(!has_allocation(file));
	/* remap the garbage pages, forkers don't get access to the data */
	pmem_unmap_pfn_range(id, vma, data, 0, vma->vm_start - vma->vm_end);
	up_read(&data->sem);
}

static void pmem_vma_close(struct vm_area_struct *vma)
{
	struct file *file = vma->vm_file;
	struct pmem_data *data = file->private_data;

#if PMEM_DEBUG_MSGS
	char currtask_name[FIELD_SIZEOF(struct task_struct, comm) + 1];
#endif
	DLOG("Dev %s(id: %d) pid %u(%s) ppid %u file %p count %ld\n",
		get_name(file), get_id(file), current->pid,
		get_task_comm(currtask_name, current),
		current->parent->pid, file, file_count(file));

	if (unlikely(!is_pmem_file(file))) {
		pr_warning("pmem: something is very wrong, you are "
		       "closing a vm backing an allocation that doesn't "
		       "exist!\n");
		return;
	}

	down_write(&data->sem);
	if (unlikely(!has_allocation(file))) {
		up_write(&data->sem);
		pr_warning("pmem: something is very wrong, you are "
		       "closing a vm backing an allocation that doesn't "
		       "exist!\n");
		return;
	}
	if (data->vma == vma) {
		data->vma = NULL;
		if ((data->flags & PMEM_FLAGS_CONNECTED) &&
		    (data->flags & PMEM_FLAGS_SUBMAP))
			data->flags |= PMEM_FLAGS_UNSUBMAP;
	}
	/* the kernel is going to free this vma now anyway */
	up_write(&data->sem);
}

static struct vm_operations_struct vm_ops = {
	.open = pmem_vma_open,
	.close = pmem_vma_close,
};

static int pmem_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct pmem_data *data = file->private_data;
	int index = -1;
	unsigned long vma_size =  vma->vm_end - vma->vm_start;
	int ret = 0, id = get_id(file);
#if PMEM_DEBUG_MSGS
	char currtask_name[FIELD_SIZEOF(struct task_struct, comm) + 1];
#endif

	if (!data) {
		pr_err("pmem: Invalid file descriptor, no private data\n");
		return -EINVAL;
	}
	DLOG("pid %u(%s) mmap vma_size %lu on dev %s(id: %d)\n", current->pid,
		get_task_comm(currtask_name, current), vma_size,
		get_name(file), id);
	if (vma->vm_pgoff || !PMEM_IS_PAGE_ALIGNED(vma_size)) {
#if PMEM_DEBUG
		pr_err("pmem: mmaps must be at offset zero, aligned"
				" and a multiple of pages_size.\n");
#endif
		return -EINVAL;
	}

	down_write(&data->sem);
	/* check this file isn't already mmaped, for submaps check this file
	 * has never been mmaped */
	if ((data->flags & PMEM_FLAGS_SUBMAP) ||
	    (data->flags & PMEM_FLAGS_UNSUBMAP)) {
#if PMEM_DEBUG
		pr_err("pmem: you can only mmap a pmem file once, "
		       "this file is already mmaped. %x\n", data->flags);
#endif
		ret = -EINVAL;
		goto error;
	}
	/* if file->private_data == unalloced, alloc*/
	if (data->index == -1) {
		mutex_lock(&pmem[id].arena_mutex);
		index = pmem_allocate_from_id(id,
				vma->vm_end - vma->vm_start,
				SZ_4K);
		mutex_unlock(&pmem[id].arena_mutex);
		/* either no space was available or an error occured */
		if (index == -1) {
			pr_err("pmem: mmap unable to allocate memory"
				"on %s\n", get_name(file));
			ret = -ENOMEM;
			goto error;
		}
		/* store the index of a successful allocation */
		data->index = index;
	}

	if (pmem[id].len(id, data) < vma_size) {
#if PMEM_DEBUG
		pr_err("pmem: mmap size [%lu] does not match"
		       " size of backing region [%lu].\n", vma_size,
		       pmem[id].len(id, data));
#endif
		ret = -EINVAL;
		goto error;
	}

	vma->vm_pgoff = pmem[id].start_addr(id, data) >> PAGE_SHIFT;

	vma->vm_page_prot = pmem_phys_mem_access_prot(file, vma->vm_page_prot);

	if (data->flags & PMEM_FLAGS_CONNECTED) {
		struct pmem_region_node *region_node;
		struct list_head *elt;
		if (pmem_map_garbage(id, vma, data, 0, vma_size)) {
			pr_alert("pmem: mmap failed in kernel!\n");
			ret = -EAGAIN;
			goto error;
		}
		list_for_each(elt, &data->region_list) {
			region_node = list_entry(elt, struct pmem_region_node,
						 list);
			DLOG("remapping file: %p %lx %lx\n", file,
				region_node->region.offset,
				region_node->region.len);
			if (pmem_remap_pfn_range(id, vma, data,
						 region_node->region.offset,
						 region_node->region.len)) {
				ret = -EAGAIN;
				goto error;
			}
		}
		data->flags |= PMEM_FLAGS_SUBMAP;
		get_task_struct(current->group_leader);
		data->task = current->group_leader;
		data->vma = vma;
#if PMEM_DEBUG
		data->pid = current->pid;
#endif
		DLOG("submmapped file %p vma %p pid %u\n", file, vma,
		     current->pid);
	} else {
		if (pmem_map_pfn_range(id, vma, data, 0, vma_size)) {
			pr_err("pmem: mmap failed in kernel!\n");
			ret = -EAGAIN;
			goto error;
		}
		data->flags |= PMEM_FLAGS_MASTERMAP;
		data->pid = current->pid;
	}
	vma->vm_ops = &vm_ops;
error:
	up_write(&data->sem);
	return ret;
}

/* the following are the api for accessing pmem regions by other drivers
 * from inside the kernel */
int get_pmem_user_addr(struct file *file, unsigned long *start,
		   unsigned long *len)
{
	int ret = -1;

	if (is_pmem_file(file)) {
		struct pmem_data *data = file->private_data;

		down_read(&data->sem);
		if (has_allocation(file)) {
			if (data->vma) {
				*start = data->vma->vm_start;
				*len = data->vma->vm_end - data->vma->vm_start;
			} else {
				*start = *len = 0;
#if PMEM_DEBUG
				pr_err("pmem: %s: no vma present.\n",
					__func__);
#endif
			}
			ret = 0;
		}
		up_read(&data->sem);
	}

#if PMEM_DEBUG
	if (ret)
		pr_err("pmem: %s: requested pmem data from invalid"
			"file.\n", __func__);
#endif
	return ret;
}

int get_pmem_addr(struct file *file, unsigned long *start,
		  unsigned long *vstart, unsigned long *len)
{
	int ret = -1;

	if (is_pmem_file(file)) {
		struct pmem_data *data = file->private_data;

		down_read(&data->sem);
		if (has_allocation(file)) {
			int id = get_id(file);

			*start = pmem[id].start_addr(id, data);
			*len = pmem[id].len(id, data);
			*vstart = (unsigned long)
				pmem_start_vaddr(id, data);
			up_read(&data->sem);
#if PMEM_DEBUG
			down_write(&data->sem);
			data->ref++;
			up_write(&data->sem);
#endif
			DLOG("returning start %#lx len %lu "
				"vstart %#lx\n",
				*start, *len, *vstart);
			ret = 0;
		} else {
			up_read(&data->sem);
		}
	}
	return ret;
}

int get_pmem_file(unsigned int fd, unsigned long *start, unsigned long *vstart,
		  unsigned long *len, struct file **filp)
{
	int ret = -1;
	struct file *file = fget(fd);

	if (unlikely(file == NULL)) {
		pr_err("pmem: %s: requested data from file "
			"descriptor that doesn't exist.\n", __func__);
	} else {
#if PMEM_DEBUG_MSGS
		char currtask_name[FIELD_SIZEOF(struct task_struct, comm) + 1];
#endif
		DLOG("filp %p rdev %d pid %u(%s) file %p(%ld)"
			" dev %s(id: %d)\n", filp,
			file->f_dentry->d_inode->i_rdev,
			current->pid, get_task_comm(currtask_name, current),
			file, file_count(file), get_name(file), get_id(file));

		if (!get_pmem_addr(file, start, vstart, len)) {
			if (filp)
				*filp = file;
			ret = 0;
		} else {
			fput(file);
		}
	}
	return ret;
}
EXPORT_SYMBOL(get_pmem_file);

int get_pmem_fd(int fd, unsigned long *start, unsigned long *len)
{
	unsigned long vstart;
	return get_pmem_file(fd, start, &vstart, len, NULL);
}
EXPORT_SYMBOL(get_pmem_fd);

void put_pmem_file(struct file *file)
{
#if PMEM_DEBUG_MSGS
	char currtask_name[FIELD_SIZEOF(struct task_struct, comm) + 1];
#endif
	DLOG("rdev %d pid %u(%s) file %p(%ld)" " dev %s(id: %d)\n",
		file->f_dentry->d_inode->i_rdev, current->pid,
		get_task_comm(currtask_name, current), file,
		file_count(file), get_name(file), get_id(file));
	if (is_pmem_file(file)) {
#if PMEM_DEBUG
		struct pmem_data *data = file->private_data;

		down_write(&data->sem);
		if (!data->ref--) {
			data->ref++;
			pr_alert("pmem: pmem_put > pmem_get %s "
				"(pid %d)\n",
			       pmem[get_id(file)].dev.name, data->pid);
			BUG();
		}
		up_write(&data->sem);
#endif
		fput(file);
	}
}
EXPORT_SYMBOL(put_pmem_file);

void put_pmem_fd(int fd)
{
	int put_needed;
	struct file *file = fget_light(fd, &put_needed);

	if (file) {
		put_pmem_file(file);
		fput_light(file, put_needed);
	}
}

void flush_pmem_fd(int fd, unsigned long offset, unsigned long len)
{
	int fput_needed;
	struct file *file = fget_light(fd, &fput_needed);

	if (file) {
		flush_pmem_file(file, offset, len);
		fput_light(file, fput_needed);
	}
}

void flush_pmem_file(struct file *file, unsigned long offset, unsigned long len)
{
	struct pmem_data *data;
	int id;
	void *vaddr;
	struct pmem_region_node *region_node;
	struct list_head *elt;
	void *flush_start, *flush_end;
#ifdef CONFIG_OUTER_CACHE
	unsigned long phy_start, phy_end;
#endif
	if (!is_pmem_file(file))
		return;

	id = get_id(file);
	if (!pmem[id].cached)
		return;

	/* is_pmem_file fails if !file */
	data = file->private_data;

	down_read(&data->sem);
	if (!has_allocation(file))
		goto end;

	vaddr = pmem_start_vaddr(id, data);

	if (pmem[id].allocator_type == PMEM_ALLOCATORTYPE_SYSTEM) {
		dmac_flush_range(vaddr,
			(void *)((unsigned long)vaddr +
				 ((struct alloc_list *)(data->index))->size));
#ifdef CONFIG_OUTER_CACHE
		phy_start = pmem_start_addr_system(id, data);

		phy_end = phy_start +
			((struct alloc_list *)(data->index))->size;

		outer_flush_range(phy_start, phy_end);
#endif
		goto end;
	}
	/* if this isn't a submmapped file, flush the whole thing */
	if (unlikely(!(data->flags & PMEM_FLAGS_CONNECTED))) {
		dmac_flush_range(vaddr, vaddr + pmem[id].len(id, data));
#ifdef CONFIG_OUTER_CACHE
		phy_start = (unsigned long)vaddr -
				(unsigned long)pmem[id].vbase + pmem[id].base;

		phy_end  =  phy_start + pmem[id].len(id, data);

		outer_flush_range(phy_start, phy_end);
#endif
		goto end;
	}
	/* otherwise, flush the region of the file we are drawing */
	list_for_each(elt, &data->region_list) {
		region_node = list_entry(elt, struct pmem_region_node, list);
		if ((offset >= region_node->region.offset) &&
		    ((offset + len) <= (region_node->region.offset +
			region_node->region.len))) {
			flush_start = vaddr + region_node->region.offset;
			flush_end = flush_start + region_node->region.len;
			dmac_flush_range(flush_start, flush_end);
#ifdef CONFIG_OUTER_CACHE

			phy_start = (unsigned long)flush_start -
				(unsigned long)pmem[id].vbase + pmem[id].base;

			phy_end  =  phy_start + region_node->region.len;

			outer_flush_range(phy_start, phy_end);
#endif
			break;
		}
	}
end:
	up_read(&data->sem);
}

int pmem_cache_maint(struct file *file, unsigned int cmd,
		struct pmem_addr *pmem_addr)
{
	struct pmem_data *data;
	int id;
	unsigned long vaddr, paddr, length, offset,
		      pmem_len, pmem_start_addr;

	/* Called from kernel-space so file may be NULL */
	if (!file)
		return -EBADF;

	/*
	 * check that the vaddr passed for flushing is valid
	 * so that you don't crash the kernel
	 */
	if (!pmem_addr->vaddr)
		return -EINVAL;

	data = file->private_data;
	id = get_id(file);

	if (!pmem[id].cached)
		return 0;

	offset = pmem_addr->offset;
	length = pmem_addr->length;

	down_read(&data->sem);
	if (!has_allocation(file)) {
		up_read(&data->sem);
		return -EINVAL;
	}
	pmem_len = pmem[id].len(id, data);
	pmem_start_addr = pmem[id].start_addr(id, data);
	up_read(&data->sem);

	if (offset + length > pmem_len)
		return -EINVAL;

	vaddr = pmem_addr->vaddr;
	paddr = pmem_start_addr + offset;

	DLOG("pmem cache maint on dev %s(id: %d)"
		"(vaddr %lx paddr %lx len %lu bytes)\n",
		get_name(file), id, vaddr, paddr, length);
	if (cmd == PMEM_CLEAN_INV_CACHES)
		clean_and_invalidate_caches(vaddr,
				length, paddr);
	else if (cmd == PMEM_CLEAN_CACHES)
		clean_caches(vaddr, length, paddr);
	else if (cmd == PMEM_INV_CACHES)
		invalidate_caches(vaddr, length, paddr);

	return 0;
}
EXPORT_SYMBOL(pmem_cache_maint);

static int pmem_connect(unsigned long connect, struct file *file)
{
	int ret = 0, put_needed;
	struct file *src_file;

	if (!file) {
		pr_err("pmem: %s: NULL file pointer passed in, "
			"bailing out!\n", __func__);
		ret = -EINVAL;
		goto leave;
	}

	src_file = fget_light(connect, &put_needed);

	if (!src_file) {
		pr_err("pmem: %s: src file not found!\n", __func__);
		ret = -EBADF;
		goto leave;
	}

	if (src_file == file) { /* degenerative case, operator error */
		pr_err("pmem: %s: src_file and passed in file are "
			"the same; refusing to connect to self!\n", __func__);
		ret = -EINVAL;
		goto put_src_file;
	}

	if (unlikely(!is_pmem_file(src_file))) {
		pr_err("pmem: %s: src file is not a pmem file!\n",
			__func__);
		ret = -EINVAL;
		goto put_src_file;
	} else {
		struct pmem_data *src_data = src_file->private_data;

		if (!src_data) {
			pr_err("pmem: %s: src file pointer has no"
				"private data, bailing out!\n", __func__);
			ret = -EINVAL;
			goto put_src_file;
		}

		down_read(&src_data->sem);

		if (unlikely(!has_allocation(src_file))) {
			up_read(&src_data->sem);
			pr_err("pmem: %s: src file has no allocation!\n",
				__func__);
			ret = -EINVAL;
		} else {
			struct pmem_data *data;
			int src_index = src_data->index;

			up_read(&src_data->sem);

			data = file->private_data;
			if (!data) {
				pr_err("pmem: %s: passed in file "
					"pointer has no private data, bailing"
					" out!\n", __func__);
				ret = -EINVAL;
				goto put_src_file;
			}

			down_write(&data->sem);
			if (has_allocation(file) &&
					(data->index != src_index)) {
				up_write(&data->sem);

				pr_err("pmem: %s: file is already "
					"mapped but doesn't match this "
					"src_file!\n", __func__);
				ret = -EINVAL;
			} else {
				data->index = src_index;
				data->flags |= PMEM_FLAGS_CONNECTED;
				data->master_fd = connect;
				data->master_file = src_file;

				up_write(&data->sem);

				DLOG("connect %p to %p\n", file, src_file);
			}
		}
	}
put_src_file:
	fput_light(src_file, put_needed);
leave:
	return ret;
}

static void pmem_unlock_data_and_mm(struct pmem_data *data,
				    struct mm_struct *mm)
{
	up_write(&data->sem);
	if (mm != NULL) {
		up_write(&mm->mmap_sem);
		mmput(mm);
	}
}

static int pmem_lock_data_and_mm(struct file *file, struct pmem_data *data,
				 struct mm_struct **locked_mm)
{
	int ret = 0;
	struct mm_struct *mm = NULL;
#if PMEM_DEBUG_MSGS
	char currtask_name[FIELD_SIZEOF(struct task_struct, comm) + 1];
#endif
	DLOG("pid %u(%s) file %p(%ld)\n",
		current->pid, get_task_comm(currtask_name, current),
		file, file_count(file));

	*locked_mm = NULL;
lock_mm:
	down_read(&data->sem);
	if (PMEM_IS_SUBMAP(data)) {
		mm = get_task_mm(data->task);
		if (!mm) {
			up_read(&data->sem);
#if PMEM_DEBUG
			pr_alert("pmem: can't remap - task is gone!\n");
#endif
			return -1;
		}
	}
	up_read(&data->sem);

	if (mm)
		down_write(&mm->mmap_sem);

	down_write(&data->sem);
	/* check that the file didn't get mmaped before we could take the
	 * data sem, this should be safe b/c you can only submap each file
	 * once */
	if (PMEM_IS_SUBMAP(data) && !mm) {
		pmem_unlock_data_and_mm(data, mm);
		DLOG("mapping contention, repeating mmap op\n");
		goto lock_mm;
	}
	/* now check that vma.mm is still there, it could have been
	 * deleted by vma_close before we could get the data->sem */
	if ((data->flags & PMEM_FLAGS_UNSUBMAP) && (mm != NULL)) {
		/* might as well release this */
		if (data->flags & PMEM_FLAGS_SUBMAP) {
			put_task_struct(data->task);
			data->task = NULL;
			/* lower the submap flag to show the mm is gone */
			data->flags &= ~(PMEM_FLAGS_SUBMAP);
		}
		pmem_unlock_data_and_mm(data, mm);
#if PMEM_DEBUG
		pr_alert("pmem: vma.mm went away!\n");
#endif
		return -1;
	}
	*locked_mm = mm;
	return ret;
}

int pmem_remap(struct pmem_region *region, struct file *file,
		      unsigned operation)
{
	int ret;
	struct pmem_region_node *region_node;
	struct mm_struct *mm = NULL;
	struct list_head *elt, *elt2;
	int id = get_id(file);
	struct pmem_data *data;

	DLOG("operation %#x, region offset %ld, region len %ld\n",
		operation, region->offset, region->len);

	if (!is_pmem_file(file)) {
#if PMEM_DEBUG
		pr_err("pmem: remap request for non-pmem file descriptor\n");
#endif
		return -EINVAL;
	}

	/* is_pmem_file fails if !file */
	data = file->private_data;

	/* pmem region must be aligned on a page boundry */
	if (unlikely(!PMEM_IS_PAGE_ALIGNED(region->offset) ||
		 !PMEM_IS_PAGE_ALIGNED(region->len))) {
#if PMEM_DEBUG
		pr_err("pmem: request for unaligned pmem"
			"suballocation %lx %lx\n",
			region->offset, region->len);
#endif
		return -EINVAL;
	}

	/* if userspace requests a region of len 0, there's nothing to do */
	if (region->len == 0)
		return 0;

	/* lock the mm and data */
	ret = pmem_lock_data_and_mm(file, data, &mm);
	if (ret)
		return 0;

	/* only the owner of the master file can remap the client fds
	 * that back in it */
	if (!is_master_owner(file)) {
#if PMEM_DEBUG
		pr_err("pmem: remap requested from non-master process\n");
#endif
		ret = -EINVAL;
		goto err;
	}

	/* check that the requested range is within the src allocation */
	if (unlikely((region->offset > pmem[id].len(id, data)) ||
		     (region->len > pmem[id].len(id, data)) ||
		     (region->offset + region->len > pmem[id].len(id, data)))) {
#if PMEM_DEBUG
		pr_err("pmem: suballoc doesn't fit in src_file!\n");
#endif
		ret = -EINVAL;
		goto err;
	}

	if (operation == PMEM_MAP) {
		region_node = kmalloc(sizeof(struct pmem_region_node),
			      GFP_KERNEL);
		if (!region_node) {
			ret = -ENOMEM;
#if PMEM_DEBUG
			pr_alert("pmem: No space to allocate remap metadata!");
#endif
			goto err;
		}
		region_node->region = *region;
		list_add(&region_node->list, &data->region_list);
	} else if (operation == PMEM_UNMAP) {
		int found = 0;
		list_for_each_safe(elt, elt2, &data->region_list) {
			region_node = list_entry(elt, struct pmem_region_node,
				      list);
			if (region->len == 0 ||
			    (region_node->region.offset == region->offset &&
			    region_node->region.len == region->len)) {
				list_del(elt);
				kfree(region_node);
				found = 1;
			}
		}
		if (!found) {
#if PMEM_DEBUG
			pr_err("pmem: Unmap region does not map any"
				" mapped region!");
#endif
			ret = -EINVAL;
			goto err;
		}
	}

	if (data->vma && PMEM_IS_SUBMAP(data)) {
		if (operation == PMEM_MAP)
			ret = pmem_remap_pfn_range(id, data->vma, data,
				   region->offset, region->len);
		else if (operation == PMEM_UNMAP)
			ret = pmem_unmap_pfn_range(id, data->vma, data,
				   region->offset, region->len);
	}

err:
	pmem_unlock_data_and_mm(data, mm);
	return ret;
}

static void pmem_revoke(struct file *file, struct pmem_data *data)
{
	struct pmem_region_node *region_node;
	struct list_head *elt, *elt2;
	struct mm_struct *mm = NULL;
	int id = get_id(file);
	int ret = 0;

	data->master_file = NULL;
	ret = pmem_lock_data_and_mm(file, data, &mm);
	/* if lock_data_and_mm fails either the task that mapped the fd, or
	 * the vma that mapped it have already gone away, nothing more
	 * needs to be done */
	if (ret)
		return;
	/* unmap everything */
	/* delete the regions and region list nothing is mapped any more */
	if (data->vma)
		list_for_each_safe(elt, elt2, &data->region_list) {
			region_node = list_entry(elt, struct pmem_region_node,
						 list);
			pmem_unmap_pfn_range(id, data->vma, data,
					     region_node->region.offset,
					     region_node->region.len);
			list_del(elt);
			kfree(region_node);
	}
	/* delete the master file */
	pmem_unlock_data_and_mm(data, mm);
}

static void pmem_get_size(struct pmem_region *region, struct file *file)
{
	/* called via ioctl file op, so file guaranteed to be not NULL */
	struct pmem_data *data = file->private_data;
	int id = get_id(file);

	down_read(&data->sem);
	if (!has_allocation(file)) {
		region->offset = 0;
		region->len = 0;
	} else {
		region->offset = pmem[id].start_addr(id, data);
		region->len = pmem[id].len(id, data);
	}
	up_read(&data->sem);
	DLOG("offset 0x%lx len 0x%lx\n", region->offset, region->len);
}


static long pmem_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	/* called from user space as file op, so file guaranteed to be not
	 * NULL
	 */
	struct pmem_data *data = file->private_data;
	int id = get_id(file);
#if PMEM_DEBUG_MSGS
	char currtask_name[
		FIELD_SIZEOF(struct task_struct, comm) + 1];
#endif

	DLOG("pid %u(%s) file %p(%ld) cmd %#x, dev %s(id: %d)\n",
		current->pid, get_task_comm(currtask_name, current),
		file, file_count(file), cmd, get_name(file), id);

	switch (cmd) {
	case PMEM_GET_PHYS:
		{
			struct pmem_region region;

			DLOG("get_phys\n");
			down_read(&data->sem);
			if (!has_allocation(file)) {
				region.offset = 0;
				region.len = 0;
			} else {
				region.offset = pmem[id].start_addr(id, data);
				region.len = pmem[id].len(id, data);
			}
			up_read(&data->sem);

			if (copy_to_user((void __user *)arg, &region,
						sizeof(struct pmem_region)))
				return -EFAULT;

			DLOG("pmem: successful request for "
				"physical address of pmem region id %d, "
				"offset 0x%lx, len 0x%lx\n",
				id, region.offset, region.len);

			break;
		}
	case PMEM_MAP:
		{
			struct pmem_region region;
			DLOG("map\n");
			if (copy_from_user(&region, (void __user *)arg,
						sizeof(struct pmem_region)))
				return -EFAULT;
			return pmem_remap(&region, file, PMEM_MAP);
		}
		break;
	case PMEM_UNMAP:
		{
			struct pmem_region region;
			DLOG("unmap\n");
			if (copy_from_user(&region, (void __user *)arg,
						sizeof(struct pmem_region)))
				return -EFAULT;
			return pmem_remap(&region, file, PMEM_UNMAP);
			break;
		}
	case PMEM_GET_SIZE:
		{
			struct pmem_region region;
			DLOG("get_size\n");
			pmem_get_size(&region, file);
			if (copy_to_user((void __user *)arg, &region,
						sizeof(struct pmem_region)))
				return -EFAULT;
			break;
		}
	case PMEM_GET_TOTAL_SIZE:
		{
			struct pmem_region region;
			DLOG("get total size\n");
			region.offset = 0;
			get_id(file);
			region.len = pmem[id].size;
			if (copy_to_user((void __user *)arg, &region,
						sizeof(struct pmem_region)))
				return -EFAULT;
			break;
		}
	case PMEM_GET_FREE_SPACE:
		{
			struct pmem_freespace fs;
			DLOG("get freespace on %s(id: %d)\n",
				get_name(file), id);

			mutex_lock(&pmem[id].arena_mutex);
			pmem[id].free_space(id, &fs);
			mutex_unlock(&pmem[id].arena_mutex);

			DLOG("%s(id: %d) total free %lu, largest %lu\n",
				get_name(file), id, fs.total, fs.largest);

			if (copy_to_user((void __user *)arg, &fs,
				sizeof(struct pmem_freespace)))
				return -EFAULT;
			break;
	}

	case PMEM_ALLOCATE:
		{
			int ret = 0;
			DLOG("allocate, id %d\n", id);
			down_write(&data->sem);
			if (has_allocation(file)) {
				pr_err("pmem: Existing allocation found on "
					"this file descrpitor\n");
				up_write(&data->sem);
				return -EINVAL;
			}

			mutex_lock(&pmem[id].arena_mutex);
			data->index = pmem_allocate_from_id(id,
					arg,
					SZ_4K);
			mutex_unlock(&pmem[id].arena_mutex);
			ret = data->index == -1 ? -ENOMEM :
				data->index;
			up_write(&data->sem);
			return ret;
		}
	case PMEM_ALLOCATE_ALIGNED:
		{
			struct pmem_allocation alloc;
			int ret = 0;

			if (copy_from_user(&alloc, (void __user *)arg,
						sizeof(struct pmem_allocation)))
				return -EFAULT;
			DLOG("allocate id align %d %u\n", id, alloc.align);
			down_write(&data->sem);
			if (has_allocation(file)) {
				pr_err("pmem: Existing allocation found on "
					"this file descrpitor\n");
				up_write(&data->sem);
				return -EINVAL;
			}

			if (alloc.align & (alloc.align - 1)) {
				pr_err("pmem: Alignment is not a power of 2\n");
				return -EINVAL;
			}

			if (alloc.align != SZ_4K &&
					(pmem[id].allocator_type !=
						PMEM_ALLOCATORTYPE_BITMAP)) {
				pr_err("pmem: Non 4k alignment requires bitmap"
					" allocator on %s\n", pmem[id].name);
				return -EINVAL;
			}

			if (alloc.align > SZ_1M ||
				alloc.align < SZ_4K) {
				pr_err("pmem: Invalid Alignment (%u) "
					"specified\n", alloc.align);
				return -EINVAL;
			}

			mutex_lock(&pmem[id].arena_mutex);
			data->index = pmem_allocate_from_id(id,
				alloc.size,
				alloc.align);
			mutex_unlock(&pmem[id].arena_mutex);
			ret = data->index == -1 ? -ENOMEM :
				data->index;
			up_write(&data->sem);
			return ret;
		}
	case PMEM_CONNECT:
		DLOG("connect\n");
		return pmem_connect(arg, file);
	case PMEM_CLEAN_INV_CACHES:
	case PMEM_CLEAN_CACHES:
	case PMEM_INV_CACHES:
		{
			struct pmem_addr pmem_addr;

			if (copy_from_user(&pmem_addr, (void __user *)arg,
						sizeof(struct pmem_addr)))
				return -EFAULT;

			return pmem_cache_maint(file, cmd, &pmem_addr);
		}
	default:
		if (pmem[id].ioctl)
			return pmem[id].ioctl(file, cmd, arg);

		DLOG("ioctl invalid (%#x)\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static void ioremap_pmem(int id)
{
	unsigned long addr;
	const struct mem_type *type;

	DLOG("PMEMDEBUG: ioremaping for %s\n", pmem[id].name);
	if (pmem[id].map_on_demand) {
		addr = (unsigned long)pmem[id].area->addr;
		if (pmem[id].cached)
			type = get_mem_type(MT_DEVICE_CACHED);
		else
			type = get_mem_type(MT_DEVICE);
		DLOG("PMEMDEBUG: Remap phys %lx to virt %lx on %s\n",
			pmem[id].base, addr, pmem[id].name);
		if (ioremap_pages(addr, pmem[id].base,  pmem[id].size, type)) {
				pr_err("pmem: Failed to map pages\n");
				BUG();
		}
		pmem[id].vbase = pmem[id].area->addr;
		/* Flush the cache after installing page table entries to avoid
		 * aliasing when these pages are remapped to user space.
		 */
		flush_cache_vmap(addr, addr + pmem[id].size);
	} else {
		if (pmem[id].cached)
			pmem[id].vbase = ioremap_cached(pmem[id].base,
						pmem[id].size);
	#ifdef ioremap_ext_buffered
		else if (pmem[id].buffered)
			pmem[id].vbase = ioremap_ext_buffered(pmem[id].base,
						pmem[id].size);
	#endif
		else
			pmem[id].vbase = ioremap(pmem[id].base, pmem[id].size);
	}
}

int pmem_setup(struct android_pmem_platform_data *pdata,
	       long (*ioctl)(struct file *, unsigned int, unsigned long),
	       int (*release)(struct inode *, struct file *))
{
	int i, index = 0, id;
	struct vm_struct *pmem_vma = NULL;
	struct page *page;

	if (id_count >= PMEM_MAX_DEVICES) {
		pr_alert("pmem: %s: unable to register driver(%s) - no more "
			"devices available!\n", __func__, pdata->name);
		goto err_no_mem;
	}

	if (!pdata->size) {
		pr_alert("pmem: %s: unable to register pmem driver(%s) - zero "
			"size passed in!\n", __func__, pdata->name);
		goto err_no_mem;
	}

	id = id_count++;

	pmem[id].id = id;

	if (pmem[id].allocate) {
		pr_alert("pmem: %s: unable to register pmem driver - "
			"duplicate registration of %s!\n",
			__func__, pdata->name);
		goto err_no_mem;
	}

	pmem[id].allocator_type = pdata->allocator_type;

	/* 'quantum' is a "hidden" variable that defaults to 0 in the board
	 * files */
	pmem[id].quantum = pdata->quantum ?: PMEM_MIN_ALLOC;
	if (pmem[id].quantum < PMEM_MIN_ALLOC ||
		!is_power_of_2(pmem[id].quantum)) {
		pr_alert("pmem: %s: unable to register pmem driver %s - "
			"invalid quantum value (%#x)!\n",
			__func__, pdata->name, pmem[id].quantum);
		goto err_reset_pmem_info;
	}

	if (pdata->size % pmem[id].quantum) {
		/* bad alignment for size! */
		pr_alert("pmem: %s: Unable to register driver %s - "
			"memory region size (%#lx) is not a multiple of "
			"quantum size(%#x)!\n", __func__, pdata->name,
			pdata->size, pmem[id].quantum);
		goto err_reset_pmem_info;
	}

	pmem[id].cached = pdata->cached;
	pmem[id].buffered = pdata->buffered;
	pmem[id].size = pdata->size;
	pmem[id].memory_type = pdata->memory_type;
	strlcpy(pmem[id].name, pdata->name, PMEM_NAME_SIZE);

	pmem[id].num_entries = pmem[id].size / pmem[id].quantum;

	memset(&pmem[id].kobj, 0, sizeof(pmem[0].kobj));
	pmem[id].kobj.kset = pmem_kset;

	switch (pmem[id].allocator_type) {
	case PMEM_ALLOCATORTYPE_ALLORNOTHING:
		pmem[id].allocate = pmem_allocator_all_or_nothing;
		pmem[id].free = pmem_free_all_or_nothing;
		pmem[id].free_space = pmem_free_space_all_or_nothing;
		pmem[id].len = pmem_len_all_or_nothing;
		pmem[id].start_addr = pmem_start_addr_all_or_nothing;
		pmem[id].num_entries = 1;
		pmem[id].quantum = pmem[id].size;
		pmem[id].allocator.all_or_nothing.allocated = 0;

		if (kobject_init_and_add(&pmem[id].kobj,
				&pmem_allornothing_ktype, NULL,
				"%s", pdata->name))
			goto out_put_kobj;

		break;

	case PMEM_ALLOCATORTYPE_BUDDYBESTFIT:
		pmem[id].allocator.buddy_bestfit.buddy_bitmap = kmalloc(
			pmem[id].num_entries * sizeof(struct pmem_bits),
			GFP_KERNEL);
		if (!pmem[id].allocator.buddy_bestfit.buddy_bitmap)
			goto err_reset_pmem_info;

		memset(pmem[id].allocator.buddy_bestfit.buddy_bitmap, 0,
			sizeof(struct pmem_bits) * pmem[id].num_entries);

		for (i = sizeof(pmem[id].num_entries) * 8 - 1; i >= 0; i--)
			if ((pmem[id].num_entries) &  1<<i) {
				PMEM_BUDDY_ORDER(id, index) = i;
				index = PMEM_BUDDY_NEXT_INDEX(id, index);
			}
		pmem[id].allocate = pmem_allocator_buddy_bestfit;
		pmem[id].free = pmem_free_buddy_bestfit;
		pmem[id].free_space = pmem_free_space_buddy_bestfit;
		pmem[id].len = pmem_len_buddy_bestfit;
		pmem[id].start_addr = pmem_start_addr_buddy_bestfit;
		if (kobject_init_and_add(&pmem[id].kobj,
				&pmem_buddy_bestfit_ktype, NULL,
				"%s", pdata->name))
			goto out_put_kobj;

		break;

	case PMEM_ALLOCATORTYPE_BITMAP: /* 0, default if not explicit */
		pmem[id].allocator.bitmap.bitm_alloc = kmalloc(
			PMEM_INITIAL_NUM_BITMAP_ALLOCATIONS *
				sizeof(*pmem[id].allocator.bitmap.bitm_alloc),
			GFP_KERNEL);
		if (!pmem[id].allocator.bitmap.bitm_alloc) {
			pr_alert("pmem: %s: Unable to register pmem "
					"driver %s - can't allocate "
					"bitm_alloc!\n",
					__func__, pdata->name);
			goto err_reset_pmem_info;
		}

		if (kobject_init_and_add(&pmem[id].kobj,
				&pmem_bitmap_ktype, NULL,
				"%s", pdata->name))
			goto out_put_kobj;

		for (i = 0; i < PMEM_INITIAL_NUM_BITMAP_ALLOCATIONS; i++) {
			pmem[id].allocator.bitmap.bitm_alloc[i].bit = -1;
			pmem[id].allocator.bitmap.bitm_alloc[i].quanta = 0;
		}

		pmem[id].allocator.bitmap.bitmap_allocs =
			PMEM_INITIAL_NUM_BITMAP_ALLOCATIONS;

		pmem[id].allocator.bitmap.bitmap =
			kcalloc((pmem[id].num_entries + 31) / 32,
				sizeof(unsigned int), GFP_KERNEL);
		if (!pmem[id].allocator.bitmap.bitmap) {
			pr_alert("pmem: %s: Unable to register pmem "
				"driver - can't allocate bitmap!\n",
				__func__);
			goto err_cant_register_device;
		}
		pmem[id].allocator.bitmap.bitmap_free = pmem[id].num_entries;

		pmem[id].allocate = pmem_allocator_bitmap;
		pmem[id].free = pmem_free_bitmap;
		pmem[id].free_space = pmem_free_space_bitmap;
		pmem[id].len = pmem_len_bitmap;
		pmem[id].start_addr = pmem_start_addr_bitmap;

		DLOG("bitmap allocator id %d (%s), num_entries %u, raw size "
			"%lu, quanta size %u\n",
			id, pdata->name, pmem[id].allocator.bitmap.bitmap_free,
			pmem[id].size, pmem[id].quantum);
		break;

	case PMEM_ALLOCATORTYPE_SYSTEM:

		INIT_LIST_HEAD(&pmem[id].allocator.system_mem.alist);

		pmem[id].allocator.system_mem.used = 0;
		pmem[id].vbase = NULL;

		if (kobject_init_and_add(&pmem[id].kobj,
				&pmem_system_ktype, NULL,
				"%s", pdata->name))
			goto out_put_kobj;

		pmem[id].allocate = pmem_allocator_system;
		pmem[id].free = pmem_free_system;
		pmem[id].free_space = pmem_free_space_system;
		pmem[id].len = pmem_len_system;
		pmem[id].start_addr = pmem_start_addr_system;
		pmem[id].num_entries = 0;
		pmem[id].quantum = PAGE_SIZE;

		DLOG("system allocator id %d (%s), raw size %lu\n",
			id, pdata->name, pmem[id].size);
		break;

	default:
		pr_alert("Invalid allocator type (%d) for pmem driver\n",
			pdata->allocator_type);
		goto err_reset_pmem_info;
	}

	pmem[id].ioctl = ioctl;
	pmem[id].release = release;
	mutex_init(&pmem[id].arena_mutex);
	mutex_init(&pmem[id].data_list_mutex);
	INIT_LIST_HEAD(&pmem[id].data_list);

	pmem[id].dev.name = pdata->name;
	pmem[id].dev.minor = id;
	pmem[id].dev.fops = &pmem_fops;
	pmem[id].reusable = pdata->reusable;
	pr_info("pmem: Initializing %s as %s\n",
		pdata->name, pdata->cached ? "cached" : "non-cached");

	if (misc_register(&pmem[id].dev)) {
		pr_alert("Unable to register pmem driver!\n");
		goto err_cant_register_device;
	}

	if (!pmem[id].reusable) {
		pmem[id].base = allocate_contiguous_memory_nomap(pmem[id].size,
			pmem[id].memory_type, PAGE_SIZE);
		if (!pmem[id].base) {
			pr_err("pmem: Cannot allocate from reserved memory for %s\n",
				pdata->name);
			goto err_misc_deregister;
		}
	}

	/* reusable pmem requires map on demand */
	pmem[id].map_on_demand = pdata->map_on_demand || pdata->reusable;
	if (pmem[id].map_on_demand) {
		if (pmem[id].reusable) {
			const struct fmem_data *fmem_info = fmem_get_info();
			pmem[id].area = fmem_info->area;
			pmem[id].base = fmem_info->phys;
		} else {
			pmem_vma = get_vm_area(pmem[id].size, VM_IOREMAP);
			if (!pmem_vma) {
				pr_err("pmem: Failed to allocate virtual space for "
					"%s\n", pdata->name);
				goto err_free;
			}
			pr_err("pmem: Reserving virtual address range %lx - %lx for"
				" %s\n", (unsigned long) pmem_vma->addr,
				(unsigned long) pmem_vma->addr + pmem[id].size,
				pdata->name);
			pmem[id].area = pmem_vma;
		}
	} else
		pmem[id].area = NULL;

	page = alloc_page(GFP_KERNEL);
	if (!page) {
		pr_err("pmem: Failed to allocate page for %s\n", pdata->name);
		goto cleanup_vm;
	}
	pmem[id].garbage_pfn = page_to_pfn(page);
	atomic_set(&pmem[id].allocation_cnt, 0);

	if (pdata->setup_region)
		pmem[id].region_data = pdata->setup_region();

	if (pdata->request_region)
		pmem[id].mem_request = pdata->request_region;

	if (pdata->release_region)
		pmem[id].mem_release = pdata->release_region;

	pr_info("allocating %lu bytes at %lx physical for %s\n",
		pmem[id].size, pmem[id].base, pmem[id].name);

	return 0;

cleanup_vm:
	if (!pmem[id].reusable)
		remove_vm_area(pmem_vma);
err_free:
	if (!pmem[id].reusable)
		free_contiguous_memory_by_paddr(pmem[id].base);
err_misc_deregister:
	misc_deregister(&pmem[id].dev);
err_cant_register_device:
out_put_kobj:
	kobject_put(&pmem[id].kobj);
	if (pmem[id].allocator_type == PMEM_ALLOCATORTYPE_BUDDYBESTFIT)
		kfree(pmem[id].allocator.buddy_bestfit.buddy_bitmap);
	else if (pmem[id].allocator_type == PMEM_ALLOCATORTYPE_BITMAP) {
		kfree(pmem[id].allocator.bitmap.bitmap);
		kfree(pmem[id].allocator.bitmap.bitm_alloc);
	}
err_reset_pmem_info:
	pmem[id].allocate = 0;
	pmem[id].dev.minor = -1;
err_no_mem:
	return -1;
}

static int pmem_probe(struct platform_device *pdev)
{
	struct android_pmem_platform_data *pdata;

	if (!pdev || !pdev->dev.platform_data) {
		pr_alert("Unable to probe pmem!\n");
		return -1;
	}
	pdata = pdev->dev.platform_data;

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return pmem_setup(pdata, NULL, NULL);
}

static int pmem_remove(struct platform_device *pdev)
{
	int id = pdev->id;
	__free_page(pfn_to_page(pmem[id].garbage_pfn));
	pm_runtime_disable(&pdev->dev);
	if (pmem[id].vbase)
		iounmap(pmem[id].vbase);
	if (pmem[id].map_on_demand && !pmem[id].reusable && pmem[id].area)
		free_vm_area(pmem[id].area);
	if (pmem[id].base)
		free_contiguous_memory_by_paddr(pmem[id].base);
	kobject_put(&pmem[id].kobj);
	if (pmem[id].allocator_type == PMEM_ALLOCATORTYPE_BUDDYBESTFIT)
		kfree(pmem[id].allocator.buddy_bestfit.buddy_bitmap);
	else if (pmem[id].allocator_type == PMEM_ALLOCATORTYPE_BITMAP) {
		kfree(pmem[id].allocator.bitmap.bitmap);
		kfree(pmem[id].allocator.bitmap.bitm_alloc);
	}
	misc_deregister(&pmem[id].dev);
	return 0;
}

static int pmem_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: suspending...\n");
	return 0;
}

static int pmem_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "pm_runtime: resuming...\n");
	return 0;
}

static const struct dev_pm_ops pmem_dev_pm_ops = {
	.runtime_suspend = pmem_runtime_suspend,
	.runtime_resume = pmem_runtime_resume,
};

static struct platform_driver pmem_driver = {
	.probe = pmem_probe,
	.remove = pmem_remove,
	.driver = { .name = "android_pmem",
		    .pm = &pmem_dev_pm_ops,
  }
};


static int __init pmem_init(void)
{
	/* create /sys/kernel/<PMEM_SYSFS_DIR_NAME> directory */
	pmem_kset = kset_create_and_add(PMEM_SYSFS_DIR_NAME,
		NULL, kernel_kobj);
	if (!pmem_kset) {
		pr_err("pmem(%s):kset_create_and_add fail\n", __func__);
		return -ENOMEM;
	}

	return platform_driver_register(&pmem_driver);
}

static void __exit pmem_exit(void)
{
	platform_driver_unregister(&pmem_driver);
}

module_init(pmem_init);
module_exit(pmem_exit);

