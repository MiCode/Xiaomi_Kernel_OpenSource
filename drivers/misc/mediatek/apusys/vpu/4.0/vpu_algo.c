/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/slab.h>
#include <linux/string.h>
#include "vpu_cmn.h"
#include "vpu_algo.h"
#include "vpu_debug.h"

static uint32_t prop_info_data_length;

const size_t g_vpu_prop_type_size[VPU_NUM_PROP_TYPES] = {
	[VPU_PROP_TYPE_CHAR]     = sizeof(char),
	[VPU_PROP_TYPE_INT32]    = sizeof(int32_t),
	[VPU_PROP_TYPE_FLOAT]    = sizeof(int32_t),
	[VPU_PROP_TYPE_INT64]    = sizeof(int64_t),
	[VPU_PROP_TYPE_DOUBLE]   = sizeof(int64_t)
};

struct vpu_prop_desc g_vpu_prop_descs[VPU_NUM_PROPS] = {
#define INS_PROP(id, name, type, count, access) \
	{ VPU_PROP_ ## id, VPU_PROP_TYPE_ ## type, \
	  VPU_PROP_ACCESS_ ## access, 0, count, name }

	INS_PROP(RESERVED, "reserved", INT32, 256, RDONLY),
#undef INS_PROP
};

/* called by vpu_init(), calculating prop_info_data_length */
int vpu_init_algo(void)
{
	int i = 0;
	unsigned int offset = 0;
	unsigned int prop_data_length;
	struct vpu_prop_desc *prop_desc;

	for (i = 0; i < VPU_NUM_PROPS; i++) {
		prop_desc = &g_vpu_prop_descs[i];
		prop_desc->offset = offset;
		prop_data_length = prop_desc->count *
				g_vpu_prop_type_size[prop_desc->type];
		offset += prop_data_length;
	}
	/* the total length = last offset + last data length */
	prop_info_data_length = offset;

	vpu_alg_debug("%s: prop_info_data_length: %x\n",
		__func__, prop_info_data_length);

	return 0;
}

static int vpu_calc_prop_offset(struct vpu_prop_desc *descs,
	uint32_t count, uint32_t *length)
{

	struct vpu_prop_desc *prop_desc;
	uint32_t offset = 0;
	uint32_t alignment = 1;
	uint32_t i, tmp;
	size_t type_size;

	/* get the alignment of struct packing */
	for (i = 0; i < count; i++) {
		prop_desc = &descs[i];
		type_size = g_vpu_prop_type_size[prop_desc->type];

		if (alignment < type_size)
			alignment = type_size;
	}

	/* calculate every prop's offset  */
	for (i = 0; i < count; i++) {
		prop_desc = &descs[i];
		type_size = g_vpu_prop_type_size[prop_desc->type];

		/* align offset with next data type */
		tmp = offset % type_size;
		if (tmp)
			offset += type_size - tmp;

		/* padding if the remainder is not enough */
		tmp = alignment - offset % alignment;
		if (tmp < type_size)
			offset += tmp;

		prop_desc->offset = offset;
		offset += prop_desc->count * type_size;
	}
	*length = offset;

	return 0;
}

struct __vpu_algo *vpu_alg_get(struct vpu_device *dev, const char *name,
	struct __vpu_algo *alg)
{
	if (alg) {
		kref_get(&alg->ref);
		goto out;
	}

	if (!name)
		goto not_found;

	/* search from tail, so that existing algorithm can be
	 * overidden by dynamic loaded ones.
	 **/
	list_for_each_entry_reverse(alg, &dev->algo, list) {
		if (!strcmp(alg->a.name, name)) {
			/* found, reference count++ */
			kref_get(&alg->ref);
			goto out;
		}
	}

not_found:
	alg = NULL;
out:
	return alg;
}

void vpu_alg_release(struct kref *ref)
{
	kfree(container_of(ref, struct __vpu_algo, ref));
}

void vpu_alg_put(struct __vpu_algo *alg)
{
	kref_put(&alg->ref, vpu_alg_release);
}

// dev->cmd_lock, should be acquired before calling this function
static int vpu_alg_load_info(struct vpu_device *dev, struct __vpu_algo *alg)
{
	int ret;

	ret = vpu_hw_alg_info(dev, alg);  // vpu_hw_get_algo_info
	if (ret) {
		pr_info("%s: vpu_hw_alg_info: %d\n", __func__, ret);
		goto err;
	}
	vpu_calc_prop_offset(alg->a.info.desc,
		alg->a.info.desc_cnt, &alg->a.info.length);
	vpu_alg_debug("%s: vpu%d: sett.len: 0x%x\n",
		__func__, dev->id, alg->a.info.length);
	vpu_calc_prop_offset(alg->a.sett.desc,
		alg->a.sett.desc_cnt, &alg->a.sett.length);
	vpu_alg_debug("%s: vpu%d: sett.len: 0x%x\n",
		__func__, dev->id, alg->a.sett.length);

	alg->info_valid = true;

err:
	return ret;
}

// dev->cmd_lock, should be acquired before calling this function
int vpu_alg_load(struct vpu_device *dev, const char *name,
	struct __vpu_algo *alg)
{
	int ret = 0;

	alg = vpu_alg_get(dev, name, alg);
	if (!alg) {
		pr_info("%s: \"%s\" was not found\n", __func__, name);
		return -ENOENT;
	}

	if (dev->algo_curr) {
		vpu_alg_put(dev->algo_curr);
		dev->algo_curr = NULL;
	}

	ret = vpu_hw_alg_init(dev, alg);  // vpu_hw_load_algo
	if (ret) {
		pr_info("%s: vpu_hw_alg_init: %d\n", __func__, ret);
		goto err;
	}

	dev->algo_curr = alg;

	// get algo info when
	// info is invalid and not from vpu_execute()
	if (!name && !alg->info_valid) {
		ret = vpu_alg_load_info(dev, alg);
		if (ret)
			goto err;
	}

	goto out;

err:
	vpu_alg_put(alg);
out:
	vpu_alg_debug("%s: %d\n", __func__, ret);  // debug
	return ret;
}

struct __vpu_algo *vpu_alg_alloc(void)
{
	struct __vpu_algo *algo;

	algo = kzalloc(sizeof(struct __vpu_algo) +
				prop_info_data_length, GFP_KERNEL);
	if (!algo)
		return NULL;

	algo->a.info.ptr = (uintptr_t) algo + sizeof(struct __vpu_algo);
	algo->a.info.length = prop_info_data_length;
	algo->info_valid = false;

	INIT_LIST_HEAD(&algo->list);
	kref_init(&algo->ref);  /* init count = 1 */

	return algo;
}

int vpu_alloc_request(struct vpu_request **rreq)
{
	struct vpu_request *req;

	req = kzalloc(sizeof(struct vpu_request), GFP_KERNEL);

	if (!req)
		return -ENOMEM;

	*rreq = req;
	return 0;
}

int vpu_free_request(struct vpu_request *req)
{
	if (req != NULL)
		kfree(req);
	return 0;
}

