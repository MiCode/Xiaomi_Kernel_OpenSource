// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "ged_ge.h"

#include <ged_bridge.h>

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>
#include <linux/anon_inodes.h>

#include <linux/kmemleak.h>

struct GEEntry {
	uint64_t unique_id;

	struct file *file;
	int alloc_fd;

	int region_num;
	void *data;
	int *region_sizes; /* sub data */
	uint32_t **region_data; /* sub data */

	struct list_head ge_entry_list;
};

#define GED_PDEBUG(fmt, ...)\
	pr_debug("[GRALLOC_EXTRA,%s:%d]" fmt, __FILE__, __LINE__, ##__VA_ARGS__)

static struct kmem_cache *gPoolCache;
static LIST_HEAD(ge_entry_list_head);
static DEFINE_SPINLOCK(ge_entry_list_lock);

/* region alloc and free lock */
static DEFINE_SPINLOCK(ge_raf_lock);

static uint64_t gen_unique_id(void)
{
	/* A collision may occur after 2^44 sec, aka 557,474 years,
	 * if we do allocate and free in 2^20 times per second.
	 */
	static uint64_t serial;

	unsigned long flags;
	uint64_t ret;

	spin_lock_irqsave(&ge_raf_lock, flags);
	ret = serial++;
	spin_unlock_irqrestore(&ge_raf_lock, flags);

	return ret;
}

static int ge_entry_release(struct inode *inode, struct file *file)
{
	unsigned long flags;
	int i;
	struct GEEntry *entry = file->private_data;

	spin_lock_irqsave(&ge_entry_list_lock, flags);
	list_del(&entry->ge_entry_list);
	spin_unlock_irqrestore(&ge_entry_list_lock, flags);

	/* remove object */
	for (i = 0; i < entry->region_num; ++i)
		kfree(entry->region_data[i]);
	kfree(entry->region_sizes);
	kmem_cache_free(gPoolCache, entry);
	return 0;
}

static const struct file_operations GEEntry_fops = {
	.release = ge_entry_release,
};

GED_ERROR ged_ge_init(void)
{
	int flags = 0;
	GED_ERROR err = GED_OK;

	gPoolCache = kmem_cache_create("gralloc_extra",
		sizeof(struct GEEntry), 0, flags, NULL);

	if (!gPoolCache) {
		GED_PDEBUG("kmem_cache_create fail\n");
		err = GED_ERROR_FAIL;
	}

	return err;
}

int ged_ge_exit(void)
{
	/* TODO : free all memory */
	kmem_cache_destroy(gPoolCache);

	return 0;
}

int ged_ge_alloc(int region_num, uint32_t *region_sizes)
{
	unsigned long flags;
	int i;
	struct GEEntry *entry =
		(struct GEEntry *)kmem_cache_zalloc(gPoolCache, GFP_KERNEL);

	if (!entry) {
		GED_PDEBUG("alloc entry fail, size:%zu\n",
			sizeof(struct GEEntry));
		goto err_entry;
	}

	entry->alloc_fd = get_unused_fd_flags(O_CLOEXEC);

	if (entry->alloc_fd < 0) {
		GED_PDEBUG("get_unused_fd_flags() return %d\n",
			entry->alloc_fd);
		goto err_fd;
	}

	entry->file = anon_inode_getfile("gralloc_extra",
		&GEEntry_fops, entry, 0);

	if (IS_ERR(entry->file)) {
		GED_PDEBUG("anon_inode_getfile() fail\n");
		goto err_entry_file;
	}

	/* Avoid using the unpredictable value for Fuzzing */
	region_num = GE_ALLOC_STRUCT_NUM;

	entry->region_num = region_num;
	entry->data =
		kzalloc((sizeof(uint32_t) + sizeof(uint32_t *)) * region_num,
			GFP_KERNEL);
	if (!entry->data) {
		GED_PDEBUG("alloc data fail, size:%zu\n",
			sizeof(void *) * region_num);
		goto err_kmalloc;
	}

	entry->region_sizes = (uint32_t *)entry->data;
	entry->region_data = (uint32_t **)(entry->region_sizes + region_num);
	for (i = 0; i < region_num; ++i)
		entry->region_sizes[i] = region_sizes[i];

	entry->unique_id = gen_unique_id();

	spin_lock_irqsave(&ge_entry_list_lock, flags);
	list_add_tail(&entry->ge_entry_list, &ge_entry_list_head);
	spin_unlock_irqrestore(&ge_entry_list_lock, flags);

	fd_install(entry->alloc_fd, entry->file);

	return entry->alloc_fd;

err_kmalloc:
err_entry_file:
	put_unused_fd(entry->alloc_fd);
err_fd:
	kmem_cache_free(gPoolCache, entry);
err_entry:
	return -1;
}

static void dump_ge_regions(struct GEEntry *entry)
{
	int i;

	for (i = 0; i < entry->region_num; ++i) {
		GED_PDEBUG("ged_ge dump, %d/%d, s %d\n",
		i, entry->region_num, entry->region_sizes[i]);
	}
}

static int valid_parameters(struct GEEntry *entry, int region_id,
	int u32_offset, int u32_size)
{
	if (region_id < 0 || region_id >= entry->region_num ||
	u32_offset < 0 || u32_size < 0 ||
	u32_offset * sizeof(uint32_t) > entry->region_sizes[region_id] ||
	(u32_offset + u32_size) * sizeof(uint32_t) >
		entry->region_sizes[region_id]
	) {

		GED_PDEBUG("fail, invalid r_id %d, o %d, s %d\n",
				region_id, u32_offset, u32_size);

		dump_ge_regions(entry);

		return -EFAULT;
	}

	return 0;
}

int ged_ge_get(int ge_fd, int region_id, int u32_offset,
	int u32_size, uint32_t *output_data)
{
	unsigned long flags;
	int err = 0;
	uint32_t i;
	struct GEEntry *entry = NULL;
	uint32_t *pregion_data = NULL;
	struct file *file = fget(ge_fd);

	if (file == NULL || file->f_op != &GEEntry_fops) {
		GED_PDEBUG("fail, invalid ge_fd %d\n", ge_fd);
		if (file)
			fput(file);
		return -EFAULT;
	}

	entry = file->private_data;

	if (valid_parameters(entry, region_id, u32_offset, u32_size)) {
		err = -EFAULT;
		goto err_parameter;
	}

	spin_lock_irqsave(&ge_raf_lock, flags);
	pregion_data = entry->region_data[region_id];
	spin_unlock_irqrestore(&ge_raf_lock, flags);

	if (pregion_data) {
		for (i = 0; i < u32_size; ++i)
			output_data[i] = pregion_data[u32_offset + i];
	} else {
		for (i = 0; i < u32_size; ++i)
			output_data[i] = 0;
	}

err_parameter:
	fput(file);

	return err;
}

int ged_ge_set(int ge_fd, int region_id, int u32_offset,
	int u32_size, uint32_t *input_data)
{
	unsigned long flags;
	int err = 0;
	uint32_t i;
	struct GEEntry *entry = NULL;
	uint32_t *pregion_data = NULL;
	struct file *file = fget(ge_fd);

	if (file == NULL || file->f_op != &GEEntry_fops) {
		GED_PDEBUG("fail, invalid ge_fd %d\n", ge_fd);
		if (file)
			fput(file);
		return -EFAULT;
	}

	entry = file->private_data;

	if (valid_parameters(entry, region_id, u32_offset, u32_size)) {
		err = -EFAULT;
		goto err_parameter;
	}

	spin_lock_irqsave(&ge_raf_lock, flags);
	while (!entry->region_data[region_id]) {
		void *data = NULL;

		spin_unlock_irqrestore(&ge_raf_lock, flags);
		data = kzalloc(entry->region_sizes[region_id], GFP_KERNEL);
		if (!data) {
			GED_PDEBUG("kmalloc fail, size: %d\n",
			entry->region_sizes[region_id]);
			err = -EFAULT;
			goto err_parameter;
		}
		spin_lock_irqsave(&ge_raf_lock, flags);

		/* Check again.
		 * Assign data to entry->region_data[region_id]
		 * if it still is NULL.
		 */
		if (entry->region_data[region_id]) {
			spin_unlock_irqrestore(&ge_raf_lock, flags);
			kfree(data);
			spin_lock_irqsave(&ge_raf_lock, flags);
			break;
		}

		entry->region_data[region_id] = data;

		/* Avoid kmemleak scan false positive */
		kmemleak_ignore(data);
	}

	pregion_data = entry->region_data[region_id];
	spin_unlock_irqrestore(&ge_raf_lock, flags);

	for (i = 0; i < u32_size; ++i)
		pregion_data[u32_offset + i] = input_data[i];

err_parameter:
	fput(file);

	return err;
}

int ged_bridge_ge_alloc(
	struct GED_BRIDGE_IN_GE_ALLOC *psALLOC_IN,
	struct GED_BRIDGE_OUT_GE_ALLOC *psALLOC_OUT)
{
	psALLOC_OUT->ge_fd =
		ged_ge_alloc(psALLOC_IN->region_num, psALLOC_IN->region_sizes);
	psALLOC_OUT->eError = !!(psALLOC_OUT->ge_fd) ? GED_OK : GED_ERROR_OOM;
	return 0;
}

int ged_bridge_ge_get(
	struct GED_BRIDGE_IN_GE_GET *psGET_IN,
	struct GED_BRIDGE_OUT_GE_GET *psGET_OUT,
	int output_package_size)
{
	/* in gpu_ext/ged/lib/ged_ge.cpp. ged_ge_get()
	 * iOutSize will show the header size and data size.
	 */
	int header_size = sizeof(struct GED_BRIDGE_OUT_GE_GET);

	if (psGET_IN->uint32_offset < 0 ||
			psGET_IN->uint32_offset >= 0x20000000ULL ||
			psGET_IN->uint32_size < 0 ||
			psGET_IN->uint32_size >= 0x20000000ULL) {
		pr_info("[%s] invalid offset(%d) or size(%d)",
				__func__,
				psGET_IN->uint32_offset,
				psGET_IN->uint32_size);
		return -EFAULT;
	}
	if ((output_package_size - header_size) !=
		psGET_IN->uint32_size * sizeof(uint32_t)) {
		pr_info("[%s] data (%d byte) != u32_size (%d byte)",
			__func__,
			(int)(output_package_size - header_size),
			(int)(psGET_IN->uint32_size * sizeof(uint32_t)));
		return -EFAULT;
	}

	psGET_OUT->eError = ged_ge_get(
			psGET_IN->ge_fd,
			psGET_IN->region_id,
			psGET_IN->uint32_offset,
			psGET_IN->uint32_size,
			psGET_OUT->data);
	return 0;
}

int ged_bridge_ge_set(
	struct GED_BRIDGE_IN_GE_SET *psSET_IN,
	struct GED_BRIDGE_OUT_GE_SET *psSET_OUT,
	int input_package_size)
{

	int header_size = sizeof(struct GED_BRIDGE_IN_GE_SET);

	if (psSET_IN->uint32_offset < 0 ||
			psSET_IN->uint32_offset >= 0x20000000ULL ||
			psSET_IN->uint32_size < 0 ||
			psSET_IN->uint32_size >= 0x20000000ULL) {
		pr_info("[%s] invalid offset(%d) or size(%d)",
				__func__,
				psSET_IN->uint32_offset,
				psSET_IN->uint32_size);
		return -EFAULT;
	}
	if ((input_package_size - header_size) !=
		psSET_IN->uint32_size * sizeof(uint32_t)) {
		pr_info("[%s] data (%d byte) != u32_size (%d byte)",
			__func__,
			(int)(input_package_size - header_size),
			(int)(psSET_IN->uint32_size * sizeof(uint32_t)));
		return -EFAULT;
	}

	psSET_OUT->eError = ged_ge_set(
			psSET_IN->ge_fd,
			psSET_IN->region_id,
			psSET_IN->uint32_offset,
			psSET_IN->uint32_size,
			psSET_IN->data);
	return 0;
}

int ged_bridge_ge_info(
	struct GED_BRIDGE_IN_GE_INFO *psINFO_IN,
	struct GED_BRIDGE_OUT_GE_INFO *psINFO_OUT)
{
	struct file *file = fget(psINFO_IN->ge_fd);

	if (file == NULL || file->f_op != &GEEntry_fops) {
		GED_PDEBUG("ged_ge fail, invalid ge_fd %d\n", psINFO_IN->ge_fd);
		if (file)
			fput(file);
		return -EFAULT;
	}

	psINFO_OUT->unique_id =
		((struct GEEntry *)(file->private_data))->unique_id;
	psINFO_OUT->eError = GED_OK;

	fput(file);

	return 0;
}
