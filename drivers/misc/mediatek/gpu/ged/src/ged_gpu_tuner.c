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
#include "ged_debugFS.h"

#define DEBUG_ON	1
#define DEBUG_OFF	0

const char *global_packagename = "-1";
static struct mutex gsGPUTunerLock;
static struct GED_GPU_TUNER_ITEM gpu_tuner_status;
static struct GED_GPU_TUNER_HINT gpu_tuner_last_custom_hint;
static struct list_head gItemList;
static struct dentry *gsGEDGPUTunerDir;
static struct dentry *gsGPUTunerDumpStatusEntry;
static struct dentry *gpsCustomHintSetEntry;
static struct dentry *gpsDebugEntry;
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

static void *_ged_gpu_tuner_dump_status_seq_start(
		struct seq_file *psSeqFile,
		loff_t *puiPosition)
{
	return (*puiPosition == 0) ? SEQ_START_TOKEN : NULL;
}

static void _ged_gpu_tuner_dump_status_seq_stop(
		struct seq_file *psSeqFile,
		void *pvData)
{
}

static void *_ged_gpu_tuner_dump_status_seq_next(
		struct seq_file *psSeqFile,
		void *pvData,
		loff_t *puiPosition)
{
	return NULL;
}

static int _ged_gpu_tuner_dump_status_seq_show(
		struct seq_file *psSeqFile,
		void *pvData)
{
		struct list_head *listentry;
		struct GED_GPU_TUNER_ITEM *item;
		char buf[BUF_LEN];
		int cnt = 0;

		seq_puts(psSeqFile, "========================================\n");
		seq_puts(psSeqFile, "[Global Status]\n");
		snprintf(buf, sizeof(buf), "feature(%08x)\n",
			gpu_tuner_status.status.feature);
		seq_puts(psSeqFile, buf);
		seq_puts(psSeqFile, "========================================\n");
		list_for_each(listentry, &gItemList) {
			item =
			list_entry(listentry, struct GED_GPU_TUNER_ITEM, List);
			if (item) {
				snprintf(buf, sizeof(buf), " [%d]\n", cnt++);
				seq_puts(psSeqFile, buf);
				snprintf(buf, sizeof(buf),
				" pkgname(%s) cmd(%s) feature(%08x)\n",
				item->status.packagename,
				(*item->status.cmd) ? item->status.cmd : "N/A",
				item->status.feature);
				seq_puts(psSeqFile, buf);
				seq_puts(psSeqFile, "========================================\n");
			}
		}

	return 0;
}

const struct seq_operations gsGPUTunerDumpStatusReadOps = {
	.start = _ged_gpu_tuner_dump_status_seq_start,
	.stop = _ged_gpu_tuner_dump_status_seq_stop,
	.next = _ged_gpu_tuner_dump_status_seq_next,
	.show = _ged_gpu_tuner_dump_status_seq_show,
};

static ssize_t _ged_custom_hint_set_write_entry(
		const char __user *pszBuffer,
		size_t uiCount,
		loff_t uiPosition,
		void *pvData)
{
#define NUM_TOKEN 3

	/*
	 *  This proc node accept only: [PACKAGE NAME][CMD][VALUE]
	 *  for ex: "packagename anisotropic_disable 1"
	 *
	 */

	char acBuffer[BUF_LEN];
	int index[NUM_TOKEN], i;
	char *packagename, *cmd, *val;
	int value, feature, len;
	GED_ERROR ret;

	if (!((uiCount > 0) && (uiCount < BUF_LEN - 1)))
		return GED_ERROR_INVALID_PARAMS;

	memset(acBuffer, 0, BUF_LEN);
	if (ged_copy_from_user(acBuffer, pszBuffer, uiCount))
		return GED_ERROR_FAIL;

	acBuffer[uiCount] = '\0';
	ret = _tokenizer(acBuffer, uiCount, index, NUM_TOKEN);

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

	return uiCount;
}

static void *_ged_gpu_tuner_custom_hint_set_seq_start(
		struct seq_file *psSeqFile,
		loff_t *puiPosition)
{
	return (*puiPosition == 0) ? SEQ_START_TOKEN : NULL;
}

static void _ged_gpu_tuner_custom_hint_set_seq_stop(
		struct seq_file *psSeqFile,
		void *pvData)
{
}

static void *_ged_gpu_tuner_custom_hint_set_seq_next(
		struct seq_file *psSeqFile,
		void *pvData,
		loff_t *puiPosition)
{
	return NULL;
}

static int _ged_gpu_tuner_custom_hint_set_seq_show(
		struct seq_file *psSeqFile,
		void *pvData)
{

	char buf[BUF_LEN];

	seq_puts(psSeqFile, "support cmd list\n");
	seq_puts(psSeqFile, "anisotropic_disable => MTK_GPU_TUNER_ANISOTROPIC_DISABLE\n");
	seq_puts(psSeqFile, "trilinear_disable => MTK_GPU_TUNER_TRILINEAR_DISABLE\n");
	seq_puts(psSeqFile, "========================================\n");
	if (gpu_tuner_last_custom_hint.packagename[0]) {

		snprintf(buf, sizeof(buf),
		" name(%s) cmd(%s) feature(%d) value(%d)\n",
				gpu_tuner_last_custom_hint.packagename,
				gpu_tuner_last_custom_hint.cmd,
				gpu_tuner_last_custom_hint.feature,
				gpu_tuner_last_custom_hint.value);
		seq_puts(psSeqFile, buf);
	} else
		seq_puts(psSeqFile, "no custom hint is set\n");

	return 0;
}

const struct seq_operations gsGPUTunerCustomHintSetReadOps = {
	.start = _ged_gpu_tuner_custom_hint_set_seq_start,
	.stop = _ged_gpu_tuner_custom_hint_set_seq_stop,
	.next = _ged_gpu_tuner_custom_hint_set_seq_next,
	.show = _ged_gpu_tuner_custom_hint_set_seq_show,
};

static ssize_t _ged_debug_write_entry(
		const char __user *pszBuffer,
		size_t uiCount,
		loff_t uiPosition,
		void *pvData)
{

#undef NUM_TOKEN
#define NUM_TOKEN 1
	char acBuffer[BUF_LEN];
	char *val;
	int index[NUM_TOKEN], value;
	GED_ERROR ret;

	if (!((uiCount > 0) && (uiCount < BUF_LEN - 1)))
		return GED_ERROR_INVALID_PARAMS;

	if (ged_copy_from_user(acBuffer, pszBuffer, uiCount))
		return GED_ERROR_FAIL;

	acBuffer[uiCount] = '\0';
	ret = _tokenizer(acBuffer, uiCount, index, NUM_TOKEN);
	if (ret == NUM_TOKEN) {
		val = acBuffer + index[0];
		ret = kstrtoint(val, 0, &value);
		GPU_TUNER_INFO("debug(%d)\n", value);
		debug = value;
	} else {
		GPU_TUNER_ERROR("[%s] invalid input\n", __func__);
		return GED_ERROR_FAIL;
	}

	return uiCount;
}

static void *_ged_gpu_tuner_debug_seq_start(
		struct seq_file *psSeqFile,
		loff_t *puiPosition)
{
	return (*puiPosition == 0) ? SEQ_START_TOKEN : NULL;
}

static void _ged_gpu_tuner_debug_seq_stop(
		struct seq_file *psSeqFile,
		void *pvData)
{
}

static void *_ged_gpu_tuner_debug_seq_next(
		struct seq_file *psSeqFile,
		void *pvData,
		loff_t *puiPosition)
{
	return NULL;
}

static int _ged_gpu_tuner_debug_seq_show(
		struct seq_file *psSeqFile,
		void *pvData)
{
	char buf[BUF_LEN];

	snprintf(buf, sizeof(buf), "debug(%d)\n", debug);
	seq_puts(psSeqFile, buf);

	return 0;
}

const struct seq_operations gsGPUDebugReadOps = {
	.start = _ged_gpu_tuner_debug_seq_start,
	.stop = _ged_gpu_tuner_debug_seq_stop,
	.next = _ged_gpu_tuner_debug_seq_next,
	.show = _ged_gpu_tuner_debug_seq_show,
};

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

	err = ged_debugFS_create_entry_dir(
			"gpu_tuner",
			NULL,
			&gsGEDGPUTunerDir);

	err = ged_debugFS_create_entry(
			"dump_status",
			gsGEDGPUTunerDir,
			&gsGPUTunerDumpStatusReadOps,
			NULL,
			NULL,
			&gsGPUTunerDumpStatusEntry);
	if (unlikely(err != GED_OK)) {
		GPU_TUNER_ERROR("[%s] failed to create dump_status entry!\n",
		__func__);
		goto ERROR;
	}

	err = ged_debugFS_create_entry(
			"custom_hint_set",
			gsGEDGPUTunerDir,
			&gsGPUTunerCustomHintSetReadOps,
			_ged_custom_hint_set_write_entry,
			NULL,
			&gpsCustomHintSetEntry);
	if (unlikely(err != GED_OK)) {
		GPU_TUNER_ERROR(
		"[%s] failed to create custom hint set entry!\n",
		__func__);

		goto ERROR;
	}

	err = ged_debugFS_create_entry(
			"debug",
			gsGEDGPUTunerDir,
			&gsGPUDebugReadOps,
			_ged_debug_write_entry,
			NULL,
			&gpsDebugEntry);
	if (unlikely(err != GED_OK)) {
		GPU_TUNER_ERROR(
		"[%s] failed to create custom hint set entry!\n",
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
