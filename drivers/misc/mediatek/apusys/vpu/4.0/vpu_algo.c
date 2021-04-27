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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "vpu_cmd.h"
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

static struct __vpu_algo *vpu_alg_get(struct vpu_algo_list *al,
	const char *name, struct __vpu_algo *alg)
{
	if (alg) {
		kref_get(&alg->ref);
		goto out;
	}

	if (!name)
		goto out;

	/* search from tail, so that existing algorithm can be
	 * overidden by dynamic loaded ones.
	 **/
	spin_lock(&al->lock);
	list_for_each_entry_reverse(alg, &al->a, list) {
		if (!strcmp(alg->a.name, name)) {
			/* found, reference count++ */
			kref_get(&alg->ref);
			goto unlock;
		}
	}
	alg = NULL;
unlock:
	spin_unlock(&al->lock);
out:
	if (alg)
		vpu_alg_debug("%s: vpu%d: %s: %s: ref: %d builtin: %d\n",
			__func__, al->vd->id, al->name, alg->a.name,
			kref_read(&alg->ref), alg->builtin);
	else
		vpu_alg_debug("%s: vpu%d: %s: %s was not found\n",
			__func__, al->vd->id, al->name, name);

	return alg;
}

static void vpu_alg_release(struct kref *ref)
{
	struct __vpu_algo *alg
		= container_of(ref, struct __vpu_algo, ref);
	struct vpu_algo_list *al = alg->al;

	spin_lock(&al->lock);
	list_del(&alg->list);
	al->cnt--;
	spin_unlock(&al->lock);

	vpu_alg_debug("%s: vpu%d: %s: %s, algo_cnt: %d builtin: %d\n",
		__func__, al->vd->id, al->name, alg->a.name,
		al->cnt, alg->builtin);

	/* free __vpu_algo memory */
	vpu_alg_free(container_of(ref, struct __vpu_algo, ref));
}

static void vpu_alg_put(struct __vpu_algo *alg)
{
	vpu_alg_debug("%s: vpu%d: %s: %s: ref: %d builtin: %d\n",
		__func__, alg->al->vd->id, alg->al->name, alg->a.name,
		kref_read(&alg->ref), alg->builtin);
	kref_put(&alg->ref, alg->al->ops->release);
}

/**
 * vpu_alg_unload() - unload currently loaded algorithm of
 *                    command priority "prio" from vpu
 * @vd: vpu device
 * @prio: command priority
 *
 * vpu_cmd_lock() must be locked before calling this function
 */
static void vpu_alg_unload(struct vpu_algo_list *al, int prio)
{
	struct __vpu_algo *alg;

	if (!al)
		return;

	alg = vpu_cmd_alg(al->vd, prio);
	if (!alg)
		return;

	al->ops->put(alg);
	vpu_cmd_alg_clr(al->vd, prio);
}

/**
 * vpu_alg_load() - load an algortihm for normal priority(0)
 *                  d2d execution
 * @vd: vpu device
 *
 * Automatically unload currently loaded algortihm, and
 * load given one.
 * vpu_cmd_lock() must be locked before calling this function
 */
static int vpu_alg_load(struct vpu_algo_list *al, const char *name,
	struct __vpu_algo *alg, int prio)
{
	int ret = 0;

	alg = al->ops->get(al, name, alg);
	if (!alg) {
		vpu_alg_debug("%s: vpu%d: %s: \"%s\" was not found\n",
			__func__, al->vd->id, al->name, name);
		return -ENOENT;
	}

	al->ops->unload(al, prio);

	if (al->ops->hw_init) {
		ret = al->ops->hw_init(al, alg);
		if (ret) {
			pr_info("%s: vpu%d: %s: hw_init: %d\n",
				__func__, al->vd->id, al->name, ret);
			al->ops->put(alg);
			goto out;
		}
	}

	vpu_cmd_alg_set(al->vd, prio, alg);
out:
	return ret;
}

struct __vpu_algo *vpu_alg_alloc(struct vpu_algo_list *al)
{
	struct __vpu_algo *algo;

	algo = kzalloc(sizeof(struct __vpu_algo) +
				prop_info_data_length, GFP_KERNEL);
	if (!algo)
		return NULL;

	algo->a.info.ptr = (uintptr_t) algo + sizeof(struct __vpu_algo);
	algo->a.info.length = prop_info_data_length;
	algo->builtin = false;
	algo->al = al;

	INIT_LIST_HEAD(&algo->list);
	kref_init(&algo->ref);  /* init count = 1 */

	return algo;
}

void vpu_alg_free(struct __vpu_algo *alg)
{
	struct device *dev = alg->al->vd->dev;

	vpu_iova_free(dev, &alg->prog);
	vpu_iova_free(dev, &alg->iram);
	kfree(alg);
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

struct vpu_algo_ops vpu_normal_aops = {
	.load = vpu_alg_load,
	.unload = vpu_alg_unload,
	.get = vpu_alg_get,
	.put = vpu_alg_put,
	.release = vpu_alg_release,
	.hw_init = vpu_hw_alg_init,
	.add = NULL,
	.del = NULL,
};

struct vpu_algo_ops vpu_prelaod_aops = {
	.load = vpu_alg_load,
	.unload = vpu_alg_unload,
	.get = vpu_alg_get,
	.put = vpu_alg_put,
	.release = vpu_alg_release,
	.hw_init = NULL,
	.add = NULL,
	.del = NULL,
};


