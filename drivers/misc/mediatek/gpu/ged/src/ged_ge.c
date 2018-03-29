/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "ged_ge.h"

#include <ged_bridge.h>

#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/seq_file.h>

#include <ged_debugFS.h>

typedef struct {
	int ver;
	int ref;
	int region_num;

	void *data;
	int *region_sizes; /* sub data */
	uint32_t **region_data; /* sub data */
} GEEntry;

#define GE_PERR(fmt, ...)   pr_err("[GRALLOC_EXTRA,%s:%d]" fmt, __FILE__, __LINE__, ##__VA_ARGS__)

typedef struct {
	GED_FILE_PRIVATE_BASE base;
	int16_t ref_table[GE_POOL_ENTRY_SIZE];
} GED_GE_FILE;

static struct kmem_cache *gPoolCache;
static uint16_t gver = 1;
static GEEntry *gPoolEntry[GE_POOL_ENTRY_SIZE];
static DEFINE_MUTEX(gPoolMutex);

static struct dentry *gDFSEntry;

static void *_get_debugfs_seq_nextx(loff_t *pos)
{
	loff_t n = *pos - 1;

	return (n >= GE_POOL_ENTRY_SIZE) ? NULL : &gPoolEntry[n];
}

static void *_ge_debugfs_seq_start(struct seq_file *m, loff_t *pos)
{
	if (*pos == 0)
		return SEQ_START_TOKEN;

	return _get_debugfs_seq_nextx(pos);
}
void _ge_debugfs_seq_stop(struct seq_file *m, void *v)
{
	/* do nothing */
}
void *_ge_debugfs_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
	*pos += 1;
	return _get_debugfs_seq_nextx(pos);
}
int _ge_debugfs_seq_show(struct seq_file *m, void *v)
{
	if (v == SEQ_START_TOKEN) {
		seq_printf(m, "max: %d\n", GE_POOL_ENTRY_SIZE);
	} else {
		GEEntry *entry;

		mutex_lock(&gPoolMutex);
		entry = *((GEEntry **)v);
		if (entry) {
			int i;
			int memory_size = 0;
			int memory_ksize = 0;
			int regions = 0;
			int idx = ((char *)v - (char *)&gPoolEntry[0]) / sizeof(GEEntry *);

			memory_size += (sizeof(uint32_t) + sizeof(uint32_t *)) * entry->region_num;
			memory_ksize += ksize(entry->data);
			for (i = 0; i < entry->region_num; ++i) {
				if (entry->region_data[i]) {
					regions |= (1 << i);
					memory_size += entry->region_sizes[i];
					memory_ksize += ksize(entry->region_data[i]);
				}
			}

			seq_printf(m, "idx: %03x, ref: %d, region:%x data_size: (%d)%d bytes\n",
					idx, entry->ref, regions, memory_size, memory_ksize);
		}
		mutex_unlock(&gPoolMutex);
	}

	return 0;
}
static const struct seq_operations gDEFEntryOps = {
	.start = _ge_debugfs_seq_start,
	.stop = _ge_debugfs_seq_stop,
	.next = _ge_debugfs_seq_next,
	.show = _ge_debugfs_seq_show,
};
static ssize_t _ge_debugfs_write_entry(const char __user *pszBuffer, size_t uiCount, loff_t uiPosition, void *pvData)
{
	return uiCount;
}

int ged_ge_init(void)
{
	int flags = 0;

	gPoolCache = kmem_cache_create("gralloc_extra", sizeof(GEEntry), 0, flags, NULL);

	ged_debugFS_create_entry(
			"ge",
			NULL,
			&gDEFEntryOps,
			_ge_debugfs_write_entry,
			NULL,
			&gDFSEntry);

	return 0;
}

int ged_ge_exit(void)
{
	ged_debugFS_remove_entry(gDFSEntry);

	/* TODO : free all memory */
	kmem_cache_destroy(gPoolCache);

	return 0;
}

void ged_ge_context_ref(GED_FILE_PRIVATE_BASE *base, uint32_t ge_hnd)
{
	GED_GE_FILE *file = container_of(base, GED_GE_FILE, base);
	int idx = GE_GEHND2IDX(ge_hnd);

	if (file) {
		mutex_lock(&gPoolMutex);
		file->ref_table[idx] += 1;
		mutex_unlock(&gPoolMutex);
	}
}
void ged_ge_context_deref(GED_FILE_PRIVATE_BASE *base, uint32_t ge_hnd)
{
	GED_GE_FILE *file = container_of(base, GED_GE_FILE, base);
	int idx = GE_GEHND2IDX(ge_hnd);

	if (file) {
		mutex_lock(&gPoolMutex);
		file->ref_table[idx] -= 1;
		mutex_unlock(&gPoolMutex);
	}
}

static void _ged_ge_free_entry(GEEntry *entry)
{
	int i;
	/* remove object */
	for (i = 0; i < entry->region_num; ++i)
		kfree(entry->region_data[i]);
	kfree(entry->region_sizes);
	kmem_cache_free(gPoolCache, entry);
}

static void _ged_ge_deinit_context(void *base)
{
	int i;
	GED_GE_FILE *file = container_of(base, GED_GE_FILE, base);

	mutex_lock(&gPoolMutex);

	for (i = 0; i < GE_POOL_ENTRY_SIZE; ++i) {
		int deref = file->ref_table[i];
		GEEntry *entry = gPoolEntry[i];

		if (deref > 0 && entry) {
			entry->ref -= deref;

			if (entry->ref == 0) {
				gPoolEntry[i] = NULL;
				_ged_ge_free_entry(entry);
			}
		}
	}

	mutex_unlock(&gPoolMutex);

	kfree(file);
}

int ged_ge_init_context(void **pbase)
{
	GED_FILE_PRIVATE_BASE *base = (GED_FILE_PRIVATE_BASE *)*pbase;

	if (!base) {
		GED_GE_FILE *file = kmalloc(sizeof(GED_GE_FILE), GFP_KERNEL);

		if (!file) {
			GE_PERR("fail to allocate GE_FILE: size:%zu\n", sizeof(GED_GE_FILE));
			goto err_pfile;
		}

		memset(file, 0, sizeof(GED_GE_FILE));

		file->base.free_func = _ged_ge_deinit_context;

		*(GED_FILE_PRIVATE_BASE **)pbase = &file->base;
	} else {
		GED_GE_FILE *file = container_of(base, GED_GE_FILE, base);

		if (file->base.free_func != _ged_ge_deinit_context) {
			GE_PERR("private is not used by GE, please check!!");
			BUG();
		}
	}

	return 0;

err_pfile:
	return 1;
}

static int _get_unused_idx(void)
{
	int i;

	for (i = 0; i < GE_POOL_ENTRY_SIZE; ++i) {
		if (gPoolEntry[i] == NULL)
			return i;
	}

	return -1;
}

GEEntry *_gehnd2entry(uint32_t ge_hnd)
{
	GEEntry *entry = NULL;
	uint32_t idx = -1;

	/* verify ge_hnd */
	idx = GE_GEHND2IDX(ge_hnd);
	if (idx < GE_POOL_ENTRY_SIZE) {
		entry = gPoolEntry[idx];

		/* check ver */
		if (entry && entry->ver != GE_GEHND2VER(ge_hnd))
			entry = NULL;
	}

	return entry;
}

uint32_t ged_ge_alloc(int region_num, uint32_t *region_sizes)
{
	int i;
	GEEntry *entry = (GEEntry *)kmem_cache_zalloc(gPoolCache, GFP_KERNEL);
	int idx = -1;
	uint32_t ge_hnd = GE_INVALID_GEHND;

	if (!entry) {
		GE_PERR("alloc entry fail, size:%zu\n", sizeof(GEEntry));
		goto err_entry;
	}

	entry->region_num = region_num;
	entry->data = kmalloc((sizeof(uint32_t) + sizeof(uint32_t *)) * region_num, GFP_KERNEL);
	if (!entry->data) {
		GE_PERR("alloc data fail, size:%zu\n", sizeof(void *) * region_num);
		goto err_kmalloc;
	}
	memset(entry->data, 0, (sizeof(uint32_t) + sizeof(uint32_t *)) * region_num);

	entry->region_sizes = (uint32_t *)entry->data;
	entry->region_data = (uint32_t **)(entry->data + sizeof(uint32_t) * region_num);
	for (i = 0; i < region_num; ++i)
		entry->region_sizes[i] = region_sizes[i];

	mutex_lock(&gPoolMutex);

	idx = _get_unused_idx();
	if (idx < 0) {
		GE_PERR("pool full!!, PoolLimit: %d\n", GE_POOL_ENTRY_SIZE);
		mutex_unlock(&gPoolMutex);
		goto err_unused_idx;
	}

	entry->ref = 1;

	gPoolEntry[idx] = entry;

	/* gen ver for check, find a non-zero value */
	do { entry->ver = gver++; } while (entry->ver == 0);

	mutex_unlock(&gPoolMutex);

	/* encode a user_hnd */
	ge_hnd = (entry->ver << GE_POOL_ENTRY_SHIFT) | idx;

	return ge_hnd;

err_unused_idx:
	kfree(entry->data);
err_kmalloc:
	kmem_cache_free(gPoolCache, entry);
err_entry:
	return ge_hnd;
}

int32_t ged_ge_retain(uint32_t ge_hnd)
{
	int old_ref = -1;
	GEEntry *entry = NULL;

	mutex_lock(&gPoolMutex);

	entry = _gehnd2entry(ge_hnd);
	if (!entry) {
		GE_PERR("invalid ge_hnd: 0x%x\n", ge_hnd);
		goto unlock_exit;
	}

	old_ref = entry->ref;
	entry->ref += 1;

	mutex_unlock(&gPoolMutex);

	return old_ref;

unlock_exit:
	mutex_unlock(&gPoolMutex);
	return -1;
}

int32_t ged_ge_release(uint32_t ge_hnd)
{
	int old_ref = -1;
	GEEntry *entry = NULL;

	mutex_lock(&gPoolMutex);

	entry = _gehnd2entry(ge_hnd);
	if (!entry) {
		GE_PERR("invalid ge_hnd: 0x%x\n", ge_hnd);
		goto unlock_exit;
	}

	old_ref = entry->ref;
	entry->ref -= 1;
	if (old_ref == 1) {
		/* remove from pool first */
		gPoolEntry[GE_GEHND2IDX(ge_hnd)] = NULL;
	}

	mutex_unlock(&gPoolMutex);

	if (old_ref == 1)
		_ged_ge_free_entry(entry);

	return old_ref;

unlock_exit:
	mutex_unlock(&gPoolMutex);
	return -1;
}

int ged_ge_get(uint32_t ge_hnd, int region_id, int u32_offset, int u32_size, uint32_t *output_data)
{
	uint32_t i;
	GEEntry *entry = NULL;
	uint32_t *pregion_data = NULL;

	mutex_lock(&gPoolMutex);
	entry = _gehnd2entry(ge_hnd);
	mutex_unlock(&gPoolMutex);

	if (!entry) {
		GE_PERR("ge_hnd invalid 0x%x\n", ge_hnd);
		return -1;
	}

	pregion_data = entry->region_data[region_id];
	if (pregion_data) {
		for (i = 0; i < u32_size; ++i)
			output_data[i] = pregion_data[u32_offset + i];
	} else {
		for (i = 0; i < u32_size; ++i)
			output_data[i] = 0;
	}

	return 0;
}

int ged_ge_set(uint32_t ge_hnd, int region_id, int u32_offset, int u32_size, uint32_t *input_data)
{
	uint32_t i;
	GEEntry *entry = NULL;
	uint32_t *pregion_data = NULL;

	mutex_lock(&gPoolMutex);
	entry = _gehnd2entry(ge_hnd);
	mutex_unlock(&gPoolMutex);

	if (!entry) {
		GE_PERR("ge_hnd invalid 0x%x\n", ge_hnd);
		return -1;
	}

	if (!entry->region_data[region_id]) {
		/* lazy allocate */
		entry->region_data[region_id] = kmalloc(entry->region_sizes[region_id], GFP_KERNEL);
		memset(entry->region_data[region_id], 0, entry->region_sizes[region_id]);
	}

	pregion_data = entry->region_data[region_id];
	for (i = 0; i < u32_size; ++i)
		pregion_data[u32_offset + i] = input_data[i];

	return 0;
}

int ged_bridge_ge_alloc(GED_BRIDGE_IN_GE_ALLOC *psALLOC_IN,	GED_BRIDGE_OUT_GE_ALLOC *psALLOC_OUT)
{
	psALLOC_OUT->ge_hnd = ged_ge_alloc(psALLOC_IN->region_num, psALLOC_IN->region_sizes);
	psALLOC_OUT->eError = !!(psALLOC_OUT->ge_hnd) ? GED_OK : GED_ERROR_OOM;
	return 0;
}

int ged_bridge_ge_retain(GED_BRIDGE_IN_GE_RETAIN *psRETAIN_IN, GED_BRIDGE_OUT_GE_RETAIN *psRETAIN_OUT)
{
	psRETAIN_OUT->ref = ged_ge_retain(psRETAIN_IN->ge_hnd);
	psRETAIN_OUT->eError = (psRETAIN_OUT->ref) ? GED_OK : GED_ERROR_INVALID_PARAMS;
	return 0;
}

int ged_bridge_ge_release(GED_BRIDGE_IN_GE_RELEASE *psRELEASE_IN, GED_BRIDGE_OUT_GE_RELEASE *psRELEASE_OUT)
{
	psRELEASE_OUT->ref = ged_ge_release(psRELEASE_IN->ge_hnd);
	psRELEASE_OUT->eError = (psRELEASE_OUT->ref) ? GED_OK : GED_ERROR_INVALID_PARAMS;
	return 0;
}

int ged_bridge_ge_get(GED_BRIDGE_IN_GE_GET *psGET_IN, GED_BRIDGE_OUT_GE_GET *psGET_OUT)
{
	psGET_OUT->eError = ged_ge_get(
			psGET_IN->ge_hnd,
			psGET_IN->region_id,
			psGET_IN->uint32_offset,
			psGET_IN->uint32_size,
			psGET_OUT->data);
	return 0;
}

int ged_bridge_ge_set(GED_BRIDGE_IN_GE_SET *psSET_IN, GED_BRIDGE_OUT_GE_SET *psSET_OUT)
{
	psSET_OUT->eError = ged_ge_set(
			psSET_IN->ge_hnd,
			psSET_IN->region_id,
			psSET_IN->uint32_offset,
			psSET_IN->uint32_size,
			psSET_IN->data);
	return 0;
}

