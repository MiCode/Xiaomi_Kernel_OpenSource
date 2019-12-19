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

struct __vpu_algo *vpu_alg_get(struct vpu_device *vd, const char *name,
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
	spin_lock(&vd->algo_lock);
	list_for_each_entry_reverse(alg, &vd->algo, list) {
		if (!strcmp(alg->a.name, name)) {
			/* found, reference count++ */
			kref_get(&alg->ref);
			goto unlock;
		}
	}

not_found:
	alg = NULL;
unlock:
	spin_unlock(&vd->algo_lock);
out:
	if (alg)
		vpu_alg_debug("%s: vpu%d: %s: ref: %d builtin: %d\n",
			      __func__, vd->id, alg->a.name,
			      kref_read(&alg->ref), alg->builtin);
	else
		vpu_alg_debug("%s: vpu%d: %s not found\n",
			      __func__, vd->id, name);

	return alg;
}

void vpu_alg_release(struct kref *ref)
{
	struct __vpu_algo *alg
		= container_of(ref, struct __vpu_algo, ref);

	/* delete __vpu_algo itself from vd->algo list */
	spin_lock(&alg->vd->algo_lock);
	list_del(&alg->list);
	alg->vd->algo_cnt--;
	spin_unlock(&alg->vd->algo_lock);

	vpu_alg_debug("%s: vpu%d: %s, algo_cnt: %d builtin: %d\n",
		      __func__, alg->vd->id, alg->a.name,
		      alg->vd->algo_cnt, alg->builtin);

	if (!alg->builtin && (alg->iova.bin == VPU_MEM_ALLOC))
		vpu_iova_free(alg->vd->dev, &alg->iova);

	/* free __vpu_algo memory */
	vpu_alg_free(container_of(ref, struct __vpu_algo, ref));
}

void vpu_alg_put(struct __vpu_algo *alg)
{
	vpu_alg_debug("%s: vpu%d: %s: ref: %d builtin: %d\n",
		      __func__, alg->vd->id, alg->a.name,
		      kref_read(&alg->ref), alg->builtin);
	kref_put(&alg->ref, vpu_alg_release);
}

// vd->cmd_lock, should be acquired before calling this function
static int vpu_alg_load_info(struct vpu_device *vd, struct __vpu_algo *alg)
{
	int ret;

	ret = vpu_hw_alg_info(vd, alg);  // vpu_hw_get_algo_info
	if (ret) {
		pr_info("%s: vpu_hw_alg_info: %d\n", __func__, ret);
		goto err;
	}
	vpu_calc_prop_offset(alg->a.info.desc,
		alg->a.info.desc_cnt, &alg->a.info.length);
	vpu_alg_debug("%s: vpu%d: sett.len: 0x%x\n",
		__func__, vd->id, alg->a.info.length);
	vpu_calc_prop_offset(alg->a.sett.desc,
		alg->a.sett.desc_cnt, &alg->a.sett.length);
	vpu_alg_debug("%s: vpu%d: sett.len: 0x%x\n",
		__func__, vd->id, alg->a.sett.length);

	alg->info_valid = true;

err:
	return ret;
}

/**
 * vpu_alg_unload() - unload currently loaded algortihm from vpu
 * @vd: vpu device
 *
 * vd->cmd_lock, should be locked before calling this function
 */
void vpu_alg_unload(struct vpu_device *vd)
{
	if (!vd || !vd->algo_curr)
		return;

	vpu_alg_put(vd->algo_curr);
	vd->algo_curr = NULL;
}

/**
 * vpu_alg_load() - load an algortihm for d2d execution
 * @vd: vpu device
 *
 * Automatically unload currently loaded algortihm, and
 * load given one.
 * vd->cmd_lock, should be locked before calling this function.
 */
int vpu_alg_load(struct vpu_device *vd, const char *name,
	struct __vpu_algo *alg)
{
	int ret = 0;

	alg = vpu_alg_get(vd, name, alg);
	if (!alg) {
		pr_info("%s: \"%s\" was not found\n", __func__, name);
		return -ENOENT;
	}

	vpu_alg_unload(vd);

	ret = vpu_hw_alg_init(vd, alg);  // vpu_hw_load_algo
	if (ret) {
		pr_info("%s: vpu_hw_alg_init: %d\n", __func__, ret);
		vpu_alg_put(alg);
		goto out;
	}

	vd->algo_curr = alg;

	// get algo info when
	// info is invalid and not from vpu_execute()
	if (!name && !alg->info_valid) {
		ret = vpu_alg_load_info(vd, alg);
		if (ret)
			vpu_alg_unload(vd);
	}

out:
	return ret;
}

struct __vpu_algo *vpu_alg_alloc(struct vpu_device *vd)
{
	struct __vpu_algo *algo;

	algo = kzalloc(sizeof(struct __vpu_algo) +
				prop_info_data_length, GFP_KERNEL);
	if (!algo)
		return NULL;

	algo->a.info.ptr = (uintptr_t) algo + sizeof(struct __vpu_algo);
	algo->a.info.length = prop_info_data_length;
	algo->info_valid = false;
	algo->builtin = false;
	algo->vd = vd;

	INIT_LIST_HEAD(&algo->list);
	kref_init(&algo->ref);  /* init count = 1 */

	return algo;
}

/*
 * vpu_alg_add - add fw to apusys
 * @vd: vpu device.
 * @fw: firmware pass to apusys
 */
int vpu_alg_add(struct vpu_device *vd, struct apusys_firmware_hnd *fw)
{
	struct __vpu_algo *alg = NULL;
	struct __vpu_algo *tmp = NULL;

	int ret = 0;

	if (fw->magic != VPU_FW_MAGIC)
		return -EINVAL;

	alg = vpu_alg_get(vd, fw->name, NULL);
	if (alg) {
		/* found only built-in => create dynamic one */
		if (alg->builtin)
			vpu_alg_put(alg);
		/* simply increase reference count and return,
		 * if the algorithm is already exist in dynamic
		 * loaded list (builtin = false)
		 */
		else
			goto out;
	}

	alg = vpu_alg_alloc(vd);
	if (!alg)
		return -ENOMEM;

	alg->iova.bin = VPU_MEM_ALLOC;
	alg->iova.size = fw->size;
	alg->a.mva = vpu_iova_alloc(to_platform_device(vd->dev), &alg->iova);
	if (!alg->a.mva) {
		ret = -ENOMEM;
		goto algo_free;
	}

	/* copy apusys algo content to vpu iova and sync to vpu device*/
	memcpy((void *)alg->iova.m.va, (void *)fw->kva, fw->size);
	vpu_iova_sync_for_device(vd->dev, &alg->iova);
	alg->a.len = alg->iova.size;

	/* make sure alg->a.name will full-filled null byte first */
	memset(alg->a.name, 0, sizeof(alg->a.name));
	strncpy(alg->a.name, fw->name,
		min(sizeof(alg->a.name), sizeof(fw->name)) - 1);

	spin_lock(&vd->algo_lock);
	list_for_each_entry_reverse(tmp, &vd->algo, list) {
		if (!strcmp(tmp->a.name, fw->name) && !tmp->builtin) {
			ret = -EEXIST;
			vpu_alg_get(vd, NULL, tmp);
			goto unlock;
		}
	}

	list_add_tail(&alg->list, &vd->algo);
	vd->algo_cnt++;
unlock:
	spin_unlock(&vd->algo_lock);

	if (!ret) {
		vpu_alg_debug("%s: name %s, len %d, mva 0x%lx alg_cnt: %d builtin: %d\n",
			      __func__, alg->a.name, alg->a.len,
			      (unsigned long)alg->a.mva, vd->algo_cnt,
			      alg->builtin);
	} else if (ret == -EEXIST) {
		vpu_alg_debug("%s: name %s already exist\n",
			      __func__, fw->name);
		vpu_iova_free(vd->dev, &alg->iova);
algo_free:
		vpu_alg_free(alg);
		ret = 0;
	}
out:
	return ret;
}

/*
 * vpu_alg_del - remove fw from apusys
 * @vd: vpu device.
 * @fw: firmware pass to apusys
 */
int vpu_alg_del(struct vpu_device *vd, struct apusys_firmware_hnd *fw)
{
	struct __vpu_algo *alg;

	if (fw->magic != VPU_FW_MAGIC)
		return -EINVAL;

	/* search from tail, so that existing algorithm can be
	 * overidden by dynamic loaded ones.
	 */
	list_for_each_entry_reverse(alg, &vd->algo, list) {
		if (!strcmp(alg->a.name, fw->name)) {
			vpu_alg_debug("%s: name %s len %d mva 0x%lx\n",
				      __func__, alg->a.name,
				      alg->a.len, (unsigned long)alg->a.mva);
			vpu_alg_put(alg);
			return 0;
		}
	}
	/* not found in vd->algo list */
	return -ENOENT;
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

int vpu_firmware(struct vpu_device *vd, struct apusys_firmware_hnd *fw)
{
	if (fw->op == APUSYS_FIRMWARE_LOAD)
		return vpu_alg_add(vd, fw);
	else if (fw->op == APUSYS_FIRMWARE_UNLOAD)
		return vpu_alg_del(vd, fw);

	vpu_cmd_debug("%s: unknown op: %d\n", __func__, fw->op);
	return -EINVAL;
}
