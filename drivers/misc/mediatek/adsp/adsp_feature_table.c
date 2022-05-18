// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include "adsp_feature_define.h"
#include "adsp_platform.h"
#include "adsp_core.h"

struct adsp_feature_tb {
	const char *name;
	int counter[ADSP_CORE_TOTAL];
};

static struct adsp_feature_control feature_ctrl[ADSP_CORE_TOTAL];
/*adsp feature list*/
static struct adsp_feature_tb feature_table[ADSP_NUM_FEATURE_ID] = {
	[SYSTEM_FEATURE_ID]           = {.name = "system"},
	[ADSP_LOGGER_FEATURE_ID]      = {.name = "logger"},
	[AURISYS_FEATURE_ID]          = {.name = "aurisys"},
	[AUDIO_CONTROLLER_FEATURE_ID] = {.name = "audio_controller"},
	[PRIMARY_FEATURE_ID]          = {.name = "primary"},
	[DEEPBUF_FEATURE_ID]          = {.name = "deepbuf"},
	[OFFLOAD_FEATURE_ID]          = {.name = "offload"},
	[AUDIO_PLAYBACK_FEATURE_ID]   = {.name = "audplayback"},
	[AUDIO_MUSIC_FEATURE_ID]      = {.name = "music"},
	[A2DP_PLAYBACK_FEATURE_ID]    = {.name = "a2dp_playback"},
	[BLEDL_FEATURE_ID]            = {.name = "bledl"},
	[BLEUL_FEATURE_ID]            = {.name = "bleul"},
	[BLEDEC_FEATURE_ID]           = {.name = "bledec"},
	[BLEENC_FEATURE_ID]           = {.name = "bleenc"},
	[AUDIO_DATAPROVIDER_FEATURE_ID] = {.name = "dataprovider"},
	[SPK_PROTECT_FEATURE_ID]      = {.name = "spk_protect"},
	[VOICE_CALL_FEATURE_ID]       = {.name = "voice_call"},
	[VOIP_FEATURE_ID]             = {.name = "voip"},
	[CAPTURE_UL1_FEATURE_ID]      = {.name = "capture_ul1"},
	[CALL_FINAL_FEATURE_ID]       = {.name = "call_final"},
	[FAST_FEATURE_ID]             = {.name = "fast"},
	[KTV_FEATURE_ID]              = {.name = "ktv"},
	[CAPTURE_RAW_FEATURE_ID]      = {.name = "capture_raw"},
	[FM_ADSP_FEATURE_ID]          = {.name = "fm_adsp"},
	[VOICE_CALL_SUB_FEATURE_ID]   = {.name = "voice_call_sub"},
	[BLE_CALL_DL_FEATURE_ID]      = {.name = "blecalldl"},
	[BLE_CALL_UL_FEATURE_ID]      = {.name = "blecallul"},
};

int adsp_get_feature_index(const char *str)
{
	int i = 0;
	size_t len;
	struct adsp_feature_tb *unit;

	if (!str)
		return -EINVAL;

	len = strlen(str);

	for (i = 0; i < ADSP_NUM_FEATURE_ID; i++) {
		unit = &feature_table[i];
		if (!unit->name || strlen(unit->name) != len)
			continue;
		if (strncmp(unit->name, str, len) == 0)
			break;
	}

	return i == ADSP_NUM_FEATURE_ID ? -EINVAL : i;
}

ssize_t adsp_dump_feature_state(u32 cid, char *buffer, int size)
{
	int n = 0;
	uint32_t i = 0;
	struct adsp_feature_tb *unit;
	struct adsp_feature_control *ctrl = &feature_ctrl[cid];

	n += scnprintf(buffer + n, size - n, "%-20s %-8s %-8s\n",
		       "Feature_name", "DTS_Set", "Counter");
	for (i = 0; i < ADSP_NUM_FEATURE_ID; i++) {
		unit = &feature_table[i];
		if (!unit->name)
			continue;
		n += scnprintf(buffer + n, size - n, "%-20s %-8d %-3d\n",
			unit->name,
			(ctrl->feature_set >> i) & 0x1,
			unit->counter[cid]);
	}

	n += scnprintf(buffer + n, size - n, "Count_total: %d\n", ctrl->total);
	return n;
}

bool adsp_feature_is_active(u32 cid)
{
	if (cid >= get_adsp_core_total())
		return false;

	return feature_ctrl[cid].total;
}

bool is_adsp_feature_in_active(void)
{
	// ADSP_A is the master.
	return adsp_feature_is_active(ADSP_A_ID);
}
EXPORT_SYMBOL(is_adsp_feature_in_active);

int _adsp_register_feature(u32 cid, u32 fid, u32 opt)
{
	int ret = 0;
	struct adsp_feature_control *ctrl;
	struct adsp_feature_tb *item;

	if (fid >= ADSP_NUM_FEATURE_ID || cid >= get_adsp_core_total())
		return -EINVAL;

	ctrl = &feature_ctrl[cid];
	item = &feature_table[fid];

	if (!item->name)
		return -EINVAL;

	if (cid != ADSP_A_ID)
		_adsp_register_feature(ADSP_A_ID, SYSTEM_FEATURE_ID, 0);

	mutex_lock(&ctrl->lock);

	if (ctrl->total == 0 && ctrl->resume) {
		cancel_delayed_work(&ctrl->suspend_work);
		ret = ctrl->resume();
	}
	if (ret == 0) {
		item->counter[cid] += 1;
		ctrl->total += 1;
	}

	mutex_unlock(&ctrl->lock);
	return ret;
}

int _adsp_deregister_feature(u32 cid, u32 fid, u32 opt)
{
	struct adsp_feature_control *ctrl;
	struct adsp_feature_tb *item;
	unsigned long delay;

	if (fid >= ADSP_NUM_FEATURE_ID || cid >= get_adsp_core_total())
		return -EINVAL;

	ctrl = &feature_ctrl[cid];
	item = &feature_table[fid];

	if (!item->name)
		return -EINVAL;

	mutex_lock(&ctrl->lock);

	if (item->counter[cid] == 0) {
		pr_err("[%s] error to deregister id=%d\n", __func__, fid);
		mutex_unlock(&ctrl->lock);
		return -EINVAL;
	}
	item->counter[cid] -= 1;
	ctrl->total -= 1;

	if (ctrl->total == 0) {
		delay = (opt & DEREGI_FLAG_NODELAY) ?
			0 : msecs_to_jiffies(ctrl->delay_ms);
		queue_delayed_work(ctrl->wq, &ctrl->suspend_work, delay);
		pr_debug("%s, send suspend work cid(%u), fid(%u), delay(%lu)",
			 __func__, cid, fid, delay);
	}

	mutex_unlock(&ctrl->lock);

	if (cid != ADSP_A_ID)
		_adsp_deregister_feature(ADSP_A_ID, SYSTEM_FEATURE_ID, 0);

	return 0;
}

bool is_feature_in_set(u32 cid, u32 fid)
{
	if (fid >= ADSP_NUM_FEATURE_ID || cid >= get_adsp_core_total())
		return false;

	return (feature_ctrl[cid].feature_set >> fid) & 0x1;
}

int adsp_feature_in_which_core(enum adsp_feature_id fid)
{
	u32 cid = 0;

	if (fid >= ADSP_NUM_FEATURE_ID)
		return -1;

	for (cid = 0; cid < get_adsp_core_total(); cid++) {
		if (is_feature_in_set(cid, fid))
			return cid;
	}

	return -1;
}

int adsp_register_feature(enum adsp_feature_id fid)
{
	int ret = -1, cid;
	bool flag = false;

	if (fid >= ADSP_NUM_FEATURE_ID)
		return -EINVAL;

	for (cid = 0; cid < get_adsp_core_total(); cid++) {
		if (!is_feature_in_set(cid, fid))
			continue;

		flag = true;
		ret = _adsp_register_feature(cid, fid, 0);

		if (ret)
			pr_info("%s, failed core:%d, fid:%d, ret:%d",
				__func__, cid, fid, ret);
	}

	if (!flag)
		pr_info("%s, feature '%s' not in any core_set",
			__func__, feature_table[fid].name);

	return ret;
}
EXPORT_SYMBOL(adsp_register_feature);

int adsp_deregister_feature(enum adsp_feature_id fid)
{
	int ret = -1, cid;
	bool flag = false;

	if (fid >= ADSP_NUM_FEATURE_ID)
		return -EINVAL;

	for (cid = 0; cid < get_adsp_core_total(); cid++) {
		if (!is_feature_in_set(cid, fid))
			continue;

		flag = true;
		ret = _adsp_deregister_feature(cid, fid, 0);

		if (ret)
			pr_info("%s, failed core:%d, fid:%d, ret:%d",
				__func__, cid, fid, ret);
	}

	if (!flag)
		pr_info("%s, feature '%s' not in any core_set",
			__func__, feature_table[fid].name);

	return ret;
}
EXPORT_SYMBOL(adsp_deregister_feature);

static void suspend_work_ws(struct work_struct *work)
{
	struct adsp_feature_control *ctrl = container_of(work,
				struct adsp_feature_control, suspend_work.work);

	mutex_lock(&ctrl->lock);
	if (ctrl->total == 0 && ctrl->suspend)
		ctrl->suspend();
	mutex_unlock(&ctrl->lock);
}

bool flush_suspend_work(u32 cid)
{
	struct adsp_feature_control *ctrl = &feature_ctrl[cid];

	if (ctrl->total == 0)
		return flush_delayed_work(&ctrl->suspend_work);

	return false;
}

int init_adsp_feature_control(u32 cid, u32 feature_set, int delay_ms,
			struct workqueue_struct *wq,
			int (*_suspend)(void),
			int (*_resume)(void))
{
	struct adsp_feature_control *ctrl;
	int i = 0;

	if (cid >= get_adsp_core_total() || !wq)
		return -EINVAL;

	ctrl = &feature_ctrl[cid];
	ctrl->wq = wq;
	ctrl->suspend = _suspend;
	ctrl->resume = _resume;
	ctrl->feature_set = feature_set;
	ctrl->delay_ms = delay_ms;
	mutex_init(&ctrl->lock);
	INIT_DELAYED_WORK(&ctrl->suspend_work, suspend_work_ws);

	ctrl->total = 0;
	for (i = 0; i < ADSP_NUM_FEATURE_ID; i++)
		ctrl->total += feature_table[i].counter[cid];

	return 0;
}
