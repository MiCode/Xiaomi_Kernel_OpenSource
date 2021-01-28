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

#include "ged_gpu_tuner.h"
#include "ged_base.h"
#include "ged_log.h"
#include "ged_sysfs.h"

#define DEBUG_ON	1
#define DEBUG_OFF	0

const char *global_packagename = "-1";
static struct mutex gsGPUTunerLock;
static struct GED_GPU_TUNER_ITEM gpu_tuner_status;
static struct GED_GPU_TUNER_HINT gpu_tuner_last_custom_hint;
static struct list_head gItemList;
static struct kobject *gpu_tuner_kobj;
static int debug = DEBUG_OFF;

#define GPU_TUNER_TAG "[GT]"
#define GPU_TUNER_DEBUG(fmt, args...) \
		do { \
			if (debug == DEBUG_ON) \
				pr_debug(GPU_TUNER_TAG fmt, ##args); \
		} while (0)
#define GPU_TUNER_INFO(fmt, args...) pr_info(GPU_TUNER_TAG fmt, ##args)
#define GPU_TUNER_ERROR(fmt, args...) pr_info(GPU_TUNER_TAG fmt, ##args)

static void _tolower_s(
		char *p)
{
	const int tolower = 'a' - 'A';

	for ( ; *p; ++p) {
		if (*p >= 'A' && *p <= 'Z')
			*p += tolower;
	}
}

static GED_ERROR _translateCmdToFeature(
		char *cmd,
		int *feature)
{
	GED_ERROR ret = GED_OK;

	if (!cmd || !feature)
		return GED_ERROR_INVALID_PARAMS;

	if (!strncmp("anisotropic_disable", cmd, strlen(cmd)))
		*feature = MTK_GPU_TUNER_ANISOTROPIC_DISABLE;
	else if (!strncmp("trilinear_disable", cmd, strlen(cmd)))
		*feature = MTK_GPU_TUNER_TRILINEAR_DISABLE;
	else
		ret = GED_ERROR_FAIL;

	return ret;
}

int _tokenizer(
		char *src,
		int len,
		int *index,
		int indexNum)
{
	int i, j, head;

	if (!src || !index)
		return GED_ERROR_INVALID_PARAMS;

	i = j = 0;
	head = -1;
	for ( ; i < len; i++) {
		if (src[i] != ' ') {
			if (head == -1)
				head = i;
		} else {
			if (head != -1) {
				index[j] = head;
				j++;
				if (j == indexNum)
					return j;
				head = -1;
			}
			src[i] = 0;
		}
	}

	if (head != -1) {
		index[j] = head;
		j++;
		return j;
	}

	return -1;
}

static void _update_all_with_global_status(void)
{
	struct list_head *listentry;
	struct GED_GPU_TUNER_ITEM *item;

	list_for_each(listentry, &gItemList) {
		item = list_entry(listentry, struct GED_GPU_TUNER_ITEM, List);
		if (item) {
			mutex_lock(&gsGPUTunerLock);
			item->status.feature &= gpu_tuner_status.status.feature;
			mutex_unlock(&gsGPUTunerLock);
			GPU_TUNER_DEBUG(
			"[%s] item feature(%08x) global feature(%8x)\n",
			__func__, item->status.feature,
			gpu_tuner_status.status.feature);
		}
	}
}

static struct GED_GPU_TUNER_ITEM *_ged_gpu_tuner_find_item_by_package_name(
		char *packagename)
{
		struct list_head *listentry;
		struct GED_GPU_TUNER_ITEM *item;
		char *p = packagename;

		if (!packagename)
			return NULL;

		_tolower_s(p);
		list_for_each(listentry, &gItemList) {

			item = list_entry(listentry,
				struct GED_GPU_TUNER_ITEM, List);
			if (item &&
			!(strncmp(item->status.packagename, packagename,
			strlen(packagename)))) {
				GPU_TUNER_DEBUG(
				"[%s] package_name(%s) feature(%08x) value(%d)\n",
				__func__, item->status.packagename,
				item->status.feature, item->status.value);
				return item;
			}
		}

		return NULL;
}

//-----------------------------------------------------------------------------
static ssize_t dump_status_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct list_head *listentry;
	struct GED_GPU_TUNER_ITEM *item;
	int cnt = 0;
	int pos = 0;
	int length;

	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"========================================\n"
			"[Global Status]\n"
			"feature(%08x)\n"
			"========================================\n",
			gpu_tuner_status.status.feature);
	pos += length;
	list_for_each(listentry, &gItemList) {
		item = list_entry(listentry, struct GED_GPU_TUNER_ITEM, List);
		if (item) {
			length = scnprintf(buf + pos,
					PAGE_SIZE - pos,
					" [%d]\n"
					" pkgname(%s) cmd(%s) feature(%08x)\n"
					"========================================\n",
					cnt++,
					item->status.packagename,
					(*item->status.cmd) ?
					item->status.cmd : "N/A",
					item->status.feature);
			pos += length;
		}
	}

	return pos;
}

static KOBJ_ATTR_RO(dump_status);
//-----------------------------------------------------------------------------
static ssize_t custom_hint_set_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
#define NUM_TOKEN 3
	/*
	 *  This proc node accept only: [PACKAGE NAME][CMD][VALUE]
	 *  for ex: "packagename anisotropic_disable 1"
	 *
	 */

	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE] = {0};
	int index[NUM_TOKEN] = {0};
	int i;
	char *packagename, *cmd, *val;
	int value = 0;
	int feature, len;
	GED_ERROR ret;

	if (!((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)))
		return GED_ERROR_INVALID_PARAMS;

	if (!scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf))
		return GED_ERROR_FAIL;

	ret = _tokenizer(acBuffer, count, index, NUM_TOKEN);

	GPU_TUNER_DEBUG("retOfTokenizer(%d) acBuffer(%s)n", ret, acBuffer);
	if (ret == NUM_TOKEN) {
		for (i = 0; i < ret; i++) {
			GPU_TUNER_DEBUG("index[%d] = %s\n",
				i, acBuffer + index[i]);
		}
		packagename = acBuffer + index[0];
		cmd = acBuffer + index[1];
		val = acBuffer + index[2];

		ret = _translateCmdToFeature(cmd, &feature);
		if (ret != GED_OK) {
			GPU_TUNER_ERROR("[%s] No recognize cmd %s\n",
			__func__, cmd);
			return GED_ERROR_FAIL;
		}

		ret = kstrtoint(val, 0, &value);
		if (value != 0)
			ret = ged_gpu_tuner_hint_set(packagename, feature);
		else
			ret = ged_gpu_tuner_hint_restore(packagename, feature);

		len = strlen(packagename);
		strncpy(gpu_tuner_last_custom_hint.packagename,
			packagename, len);
		strncpy(gpu_tuner_last_custom_hint.cmd, cmd, strlen(cmd));

		gpu_tuner_last_custom_hint.feature = feature;
		gpu_tuner_last_custom_hint.value = value;

		GPU_TUNER_DEBUG(
		"[last_hint] name(%s) cmd(%s) feature(%08x) value(%d)\n",
		gpu_tuner_last_custom_hint.packagename,
		gpu_tuner_last_custom_hint.cmd,
		gpu_tuner_last_custom_hint.feature,
		gpu_tuner_last_custom_hint.value);
	} else {
		GPU_TUNER_ERROR("[%s]invalid input\n", __func__);
		return GED_ERROR_FAIL;
	}

	return count;
#undef NUM_TOKEN
}

static ssize_t custom_hint_set_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	int pos = 0;
	int length;

	length = scnprintf(buf + pos, PAGE_SIZE - pos,
			"support cmd list\n"
			"anisotropic_disable => MTK_GPU_TUNER_ANISOTROPIC_DISABLE\n"
			"trilinear_disable => MTK_GPU_TUNER_TRILINEAR_DISABLE\n"
			"========================================\n");
	pos += length;
	if (gpu_tuner_last_custom_hint.packagename[0]) {
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				" name(%s) cmd(%s) feature(%d) value(%d)\n",
				gpu_tuner_last_custom_hint.packagename,
				gpu_tuner_last_custom_hint.cmd,
				gpu_tuner_last_custom_hint.feature,
				gpu_tuner_last_custom_hint.value);
		pos += length;
	} else {
		length = scnprintf(buf + pos, PAGE_SIZE - pos,
				"no custom hint is set\n");
		pos += length;
	}

	return pos;
}

static KOBJ_ATTR_RW(custom_hint_set);
//-----------------------------------------------------------------------------
static ssize_t debug_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t count)
{
#define NUM_TOKEN 1
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE] = {0};
	char *val;
	int index[NUM_TOKEN] = {0};
	int value = 0;
	GED_ERROR ret;

	if (!((count > 0) && (count < GED_SYSFS_MAX_BUFF_SIZE)))
		return GED_ERROR_INVALID_PARAMS;

	if (!scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf))
		return GED_ERROR_FAIL;

	ret = _tokenizer(acBuffer, count, index, NUM_TOKEN);
	if (ret == NUM_TOKEN) {
		val = acBuffer + index[0];
		ret = kstrtoint(val, 0, &value);
		GPU_TUNER_INFO("debug(%d)\n", value);
		debug = value;
	} else {
		GPU_TUNER_ERROR("[%s] invalid input\n", __func__);
		return GED_ERROR_FAIL;
	}

	return count;
#undef NUM_TOKEN
}

static ssize_t debug_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "debug(%d)\n", debug);
}

static KOBJ_ATTR_RW(debug);
//-----------------------------------------------------------------------------

GED_ERROR ged_gpu_get_stauts_by_packagename(
		char *packagename,
		struct GED_GPU_TUNER_ITEM *status)
{
	struct GED_GPU_TUNER_ITEM *item = NULL;
	char *p = packagename;

	if (!status || !packagename) {
		GPU_TUNER_ERROR("[%s] invalid parameter\n", __func__);
		return GED_ERROR_INVALID_PARAMS;
	}

	GPU_TUNER_DEBUG("[%s][IN] name(%s)\n", __func__, packagename);

	_tolower_s(p);
	if (!strncmp(packagename, global_packagename,
		strlen(global_packagename)))
		status->status.feature |= gpu_tuner_status.status.feature;
	else {
		item = _ged_gpu_tuner_find_item_by_package_name(packagename);
		if (item) {
			item->status.feature |= gpu_tuner_status.status.feature;
			status->status.feature = item->status.feature;

			GPU_TUNER_DEBUG("[%s] in(%08x) out(%08x)\n",
			__func__, item->status.feature, status->status.feature);
		}
	}

	GPU_TUNER_DEBUG("[%s][OUT] found(%d)\n", __func__, (item) ? 1 : 0);

	return (item) ? GED_OK : GED_ERROR_FAIL;
}

GED_ERROR ged_gpu_tuner_hint_set(
		char *packagename,
		enum GPU_TUNER_FEATURE eFeature)
{

	GED_ERROR err = GED_OK;
	struct GED_GPU_TUNER_ITEM *item = NULL;
	int len = 0;
	bool find = false;
	char *p = packagename;

	if (!packagename || (strlen(packagename) > (BUF_LEN - 1))) {
		GPU_TUNER_ERROR("[%s] invalid parameter\n", __func__);
		return GED_ERROR_INVALID_PARAMS;
	}

	GPU_TUNER_DEBUG("[%s][IN] name(%s) feature(%08x)\n",
	__func__, packagename, eFeature);

	_tolower_s(p);
	if (!strncmp(packagename, global_packagename,
		strlen(global_packagename))) {
		mutex_lock(&gsGPUTunerLock);
		gpu_tuner_status.status.feature |= eFeature;
		mutex_unlock(&gsGPUTunerLock);
		_update_all_with_global_status();
	} else {
		item = _ged_gpu_tuner_find_item_by_package_name(packagename);
		if (item) {
			find = true;
			GPU_TUNER_DEBUG("[%s] item(%p)\n", __func__, item);
		} else {
			item =
			(struct GED_GPU_TUNER_ITEM *)
			ged_alloc_atomic(sizeof(struct GED_GPU_TUNER_ITEM));

			if (unlikely(item == NULL)) {
				GPU_TUNER_ERROR("[%s] Fail to create item\n",
					__func__);
				err = GED_ERROR_FAIL;
				goto ERROR;
			}

			memset(item->status.packagename, 0, BUF_LEN);
				len = strlen(packagename);

			strncpy(item->status.packagename, packagename, len);
			item->status.feature = item->status.value = 0;
			memset(item->status.cmd, 0, BUF_LEN);
		}

		mutex_lock(&gsGPUTunerLock);

		item->status.feature |= eFeature;
		item->status.feature |= gpu_tuner_status.status.feature;
		item->status.value |= eFeature;
		item->status.value |= gpu_tuner_status.status.feature;


		if (find == false) {
			INIT_LIST_HEAD(&item->List);
			list_add_tail(&item->List, &gItemList);
		}

		mutex_unlock(&gsGPUTunerLock);
	}
	if (item)
		GPU_TUNER_DEBUG("[%s]name(%s) feature(%d)\n",
			__func__, item->status.packagename,
			item->status.feature);
	GPU_TUNER_DEBUG(
		"[%s]input feature(%08x) global status(%08x) err(%d)\n",
		__func__, eFeature, gpu_tuner_status.status.feature, err);

ERROR:
	GPU_TUNER_DEBUG("[%s][OUT] err(%d)\n", __func__, err);
	return err;
}

GED_ERROR ged_gpu_tuner_hint_restore(
		char *packagename,
		enum GPU_TUNER_FEATURE eFeature)
{
	GED_ERROR err = GED_OK;
	struct GED_GPU_TUNER_ITEM *item = NULL;
	char *p = packagename;

	if (!packagename) {
		GPU_TUNER_ERROR("[%s] invalid parameter\n", __func__);
		return GED_ERROR_INVALID_PARAMS;
	}

	GPU_TUNER_DEBUG("[%s][IN] name(%s) feature(%08x)\n",
	__func__, packagename, eFeature);

	_tolower_s(p);
	if (!strncmp(packagename, global_packagename,
	strlen(global_packagename))) {
		mutex_lock(&gsGPUTunerLock);
		gpu_tuner_status.status.feature &= ~eFeature;
		mutex_unlock(&gsGPUTunerLock);
		_update_all_with_global_status();
	} else {
		item = _ged_gpu_tuner_find_item_by_package_name(packagename);
		if (!item) {
			GPU_TUNER_ERROR("[%s]Can't find matched item\n",
				__func__);
			goto ERROR;
		}

		mutex_lock(&gsGPUTunerLock);

		item->status.feature &= ~eFeature;
		item->status.feature |= gpu_tuner_status.status.feature;

		mutex_unlock(&gsGPUTunerLock);
	if (item)
		GPU_TUNER_DEBUG("[%s]name(%s) feature(%d) item(%p)\n",
			__func__, item->status.packagename,
			item->status.feature, item);
	GPU_TUNER_DEBUG("[%s]input feature(%08x) global status(%08x) err(%d)\n",
		__func__, eFeature, gpu_tuner_status.status.feature, err);
	}

ERROR:

	GPU_TUNER_DEBUG("[%s][OUT] err(%d)\n", __func__, err);

	return err;
}

GED_ERROR ged_gpu_tuner_init(void)
{
	GED_ERROR err = GED_OK;

	GPU_TUNER_DEBUG("[%s] In\n", __func__);

	INIT_LIST_HEAD(&gItemList);
	mutex_init(&gsGPUTunerLock);

	gpu_tuner_status.status.feature = gpu_tuner_status.status.value = 0;
	debug = false;

	err = ged_sysfs_create_dir(NULL, "gpu_tuner", &gpu_tuner_kobj);
	if (unlikely(err != GED_OK)) {
		GPU_TUNER_ERROR("[%s] failed to create gpu_tuner dir!\n",
				__func__);
		goto ERROR;
	}

	err = ged_sysfs_create_file(gpu_tuner_kobj, &kobj_attr_dump_status);
	if (unlikely(err != GED_OK)) {
		GPU_TUNER_ERROR("[%s] failed to create dump_status entry!\n",
				__func__);
		goto ERROR;
	}

	err = ged_sysfs_create_file(gpu_tuner_kobj, &kobj_attr_custom_hint_set);
	if (unlikely(err != GED_OK)) {
		GPU_TUNER_ERROR(
			"[%s] failed to create custom_hint_set entry!\n",
			__func__);
		goto ERROR;
	}

	err = ged_sysfs_create_file(gpu_tuner_kobj, &kobj_attr_debug);
	if (unlikely(err != GED_OK)) {
		GPU_TUNER_ERROR("[%s] failed to create debug entry!\n",
				__func__);
		goto ERROR;
	}

ERROR:

	GPU_TUNER_DEBUG("[%s] Out ret(%d)\n", __func__, err);

	return err;
}

GED_ERROR ged_gpu_tuner_exit(void)
{
	struct list_head *listentry, *listentryTmp;
	struct GED_GPU_TUNER_ITEM *item;

	GPU_TUNER_DEBUG("[%s] In\n", __func__);

	mutex_destroy(&gsGPUTunerLock);

	list_for_each_safe(listentry, listentryTmp, &gItemList) {
		item = list_entry(listentry, struct GED_GPU_TUNER_ITEM, List);
		if (item) {
			list_del(&item->List);
			ged_free(item, sizeof(struct GED_GPU_TUNER_ITEM));
		}
	}

	ged_sysfs_remove_file(gpu_tuner_kobj, &kobj_attr_debug);
	ged_sysfs_remove_file(gpu_tuner_kobj, &kobj_attr_custom_hint_set);
	ged_sysfs_remove_file(gpu_tuner_kobj, &kobj_attr_dump_status);
	ged_sysfs_remove_dir(&gpu_tuner_kobj);

	GPU_TUNER_DEBUG("[%s] Out\n", __func__);

	return GED_OK;
}

int ged_bridge_gpu_tuner_status(
		struct GED_BRIDGE_IN_GPU_TUNER_STATUS *in,
		struct GED_BRIDGE_OUT_GPU_TUNER_STATUS *out)
{
	struct GED_GPU_TUNER_ITEM item;
	GED_ERROR err = GED_ERROR_FAIL;

	if (!in || !out) {
		GPU_TUNER_ERROR("[%s] invalid parameter\n", __func__);
		return GED_ERROR_INVALID_PARAMS;
	}

	GPU_TUNER_DEBUG("[%s][IN] name(%s)\n", __func__, in->name);

	item.status.feature = 0;
	in->name[sizeof(in->name)-1] = '\0';
	err = ged_gpu_get_stauts_by_packagename(in->name, &item);
	if (err == GED_OK) {
		out->feature = item.status.feature;
		GPU_TUNER_DEBUG("[%s] out_feature(%08x) in_feature(%08x)\n",
		__func__, out->feature, item.status.feature);
	}
	out->feature |= gpu_tuner_status.status.feature;

	GPU_TUNER_DEBUG("[%s][OUT] ret(%d) out(%08x)\n",
	__func__, err, (out) ? out->feature : -1);

	return GED_OK;
}
