// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include "mmqos-mtk.h"
#include "mtk-mmdvfs-debug.h"

#define CREATE_TRACE_POINTS
#include "mmqos_hrt_events.h"

#define MULTIPLY_W_DRAM_WEIGHT(value) ((value)*6/5)
#define MULTIPLY_RATIO(value) ((value)*1000)
#define DIVIDE_RATIO(value) ((value)/1000)

struct mmqos_hrt *mmqos_hrt;
static bool disp_report_bw;

static u32 mmqos_log_hrt_level;
enum mmqos_log_hrt_level {
	log_hrt_bw = 0,
};

static int hrt_ftrace_ena;

static void record_hrt_bw(u32 avail_bw, u32 cam_max_bw, u32 cam_bw)
{
	u32 pre_idx, idx;
	struct hrt_record *rec;

	rec = &mmqos_hrt->hrt_rec;
	mutex_lock(&rec->lock);
	idx = rec->idx;
	pre_idx = (idx - 1) % RECORD_NUM;
	if (rec->avail_hrt[pre_idx] != avail_bw ||
		rec->cam_max_hrt[pre_idx] != cam_max_bw ||
		rec->cam_hrt[pre_idx] != cam_bw) {
		rec->time[idx] = sched_clock();
		rec->avail_hrt[idx] = avail_bw;
		rec->cam_max_hrt[idx] = cam_max_bw;
		rec->cam_hrt[idx] = cam_bw;
		rec->idx = (idx + 1) % RECORD_NUM;
	}
	mutex_unlock(&rec->lock);
}

static void record_cam_hrt(u32 cam_max_bw)
{
	u32 pre_idx, idx;
	struct cam_hrt_record *rec = &mmqos_hrt->cam_hrt_rec;

	idx = rec->idx;
	pre_idx = (idx - 1) % RECORD_NUM;
	if (rec->cam_max_hrt[pre_idx] != cam_max_bw) {
		rec->time[idx] = sched_clock();
		rec->cam_max_hrt[idx] = cam_max_bw;
		rec->idx = (idx + 1) % RECORD_NUM;
	}
}

s32 mtk_mmqos_get_avail_hrt_bw(enum hrt_type type)
{
	u32 i, used_bw = 0;
	u32 result = 0;

	if (!mmqos_hrt)
		return -ENOENT;
	for (i = 0; i < HRT_TYPE_NUM; i++) {
		if (i != type)
			used_bw += (MULTIPLY_RATIO(mmqos_hrt->hrt_bw[i])
				/ mmqos_hrt->hrt_ratio[i]);
		if (hrt_ftrace_ena) {
			trace_mmqos__used_hrt_bw(i, mmqos_hrt->hrt_bw[i]);
		}
	}
	if (type != HRT_CAM && mmqos_hrt->cam_max_bw)
		used_bw = used_bw - (MULTIPLY_RATIO(mmqos_hrt->hrt_bw[HRT_CAM])
					/ mmqos_hrt->hrt_ratio[HRT_CAM])
					+ (MULTIPLY_RATIO(mmqos_hrt->cam_max_bw)
					/ mmqos_hrt->hrt_ratio[HRT_CAM]);

	result = DIVIDE_RATIO(mmqos_hrt->hrt_total_bw
			* mmqos_hrt->emi_ratio)
			- used_bw;

	result = DIVIDE_RATIO(result * mmqos_hrt->hrt_ratio[type]);

	record_hrt_bw(result, mmqos_hrt->cam_max_bw, mmqos_hrt->hrt_bw[HRT_CAM]);
	if (hrt_ftrace_ena) {
		trace_mmqos__used_hrt_bw(HRT_NONE, mmqos_hrt->cam_max_bw);
		trace_mmqos__avail_hrt_bw(result);
	}

	return result;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_get_avail_hrt_bw);

s32 mtk_mmqos_get_cam_hrt(void)
{
	struct hrt_record *rec;
	u32 idx;
	u32 cam_max_hrt, cam_hrt;

	if (!mmqos_hrt)
		return -ENOENT;
	rec = &mmqos_hrt->hrt_rec;
	mutex_lock(&rec->lock);
	if (rec->idx == 0)
		idx = RECORD_NUM - 1;
	else
		idx = (rec->idx - 1) % RECORD_NUM;
	cam_max_hrt = rec->cam_max_hrt[idx];
	cam_hrt = rec->cam_hrt[idx];
	mutex_unlock(&rec->lock);
	return cam_max_hrt ? cam_max_hrt : cam_hrt;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_get_cam_hrt);

s32 mtk_mmqos_register_bw_throttle_notifier(struct notifier_block *nb)
{
	if (!nb || !mmqos_hrt)
		return -EINVAL;
	return blocking_notifier_chain_register(
				&mmqos_hrt->hrt_bw_throttle_notifier,
				nb);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_register_bw_throttle_notifier);

s32 mtk_mmqos_unregister_bw_throttle_notifier(struct notifier_block *nb)
{
	if (!nb || !mmqos_hrt)
		return -EINVAL;
	return blocking_notifier_chain_unregister(
				&mmqos_hrt->hrt_bw_throttle_notifier,
				nb);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_unregister_bw_throttle_notifier);

void mtk_mmqos_wait_throttle_done(void)
{
	u32 wait_result;

	if (!mmqos_hrt)
		return;
	if (atomic_read(&mmqos_hrt->lock_count) > 0) {
		pr_notice("begin to blocking for cam_max_bw=%d\n",
			mmqos_hrt->cam_max_bw);
		wait_result = wait_event_timeout(mmqos_hrt->hrt_wait,
			atomic_read(&mmqos_hrt->lock_count) == 0,
			msecs_to_jiffies(200));
		pr_notice("blocking wait_result=%d\n", wait_result);
	}
}
EXPORT_SYMBOL_GPL(mtk_mmqos_wait_throttle_done);

s32 mtk_mmqos_set_hrt_bw(enum hrt_type type, u32 bw)
{
	if (type >= HRT_TYPE_NUM) {
		pr_notice("%s: wrong type:%d\n", __func__, type);
		return -EINVAL;
	}
	if (!mmqos_hrt)
		return -ENOENT;
	if (mmqos_hrt->hrt_bw[type] != bw) {
		mmqos_hrt->hrt_bw[type] = bw;
		if (mmqos_log_hrt_level & 1 << log_hrt_bw)
			pr_notice("%s: type=%d bw=%d\n", __func__, type, bw);
		if (hrt_ftrace_ena)
			trace_mmqos__total_hrt_bw(type, bw);
	}
	if (unlikely(!disp_report_bw) && type == HRT_DISP) {
		disp_report_bw = true;
		mtk_mmdvfs_debug_release_step0();
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_set_hrt_bw);

s32 mtk_mmqos_get_hrt_ratio(enum hrt_type type)
{
	if (type >= HRT_TYPE_NUM)
		return 1000;

	return mmqos_hrt->hrt_ratio[type];
}
EXPORT_SYMBOL_GPL(mtk_mmqos_get_hrt_ratio);

static void notify_bw_throttle(enum hrt_scen scen, bool is_start)
{
	u64 start_jiffies = jiffies;

	blocking_notifier_call_chain(&mmqos_hrt->hrt_bw_throttle_notifier,
		is_start?BW_THROTTLE_START:BW_THROTTLE_END, NULL);
	pr_notice("%s: scen(%d) notify_time=%u\n", __func__,
		scen, jiffies_to_msecs(jiffies-start_jiffies));
}

static void set_md_hrt_by_scen(u8 md_scen, bool in_speech, u32 md_type)
{
	u32 md_index = 0;

	switch (md_scen) {
	case MD_SCEN_SUB6_EXT:
		if (md_type == 1) /* sub6 only */
			md_index = 2;
		break;
	case MD_SCEN_NONE:
	default:
		break;
	}
	pr_notice("mmqos_hrt %s: md_scen=%u in_speech=%u md_type=%u md_index=%u\n",
		__func__, md_scen, in_speech, md_type, md_index);

	mmqos_hrt->hrt_bw[HRT_MD] = in_speech ?
		mmqos_hrt->md_speech_bw[md_index] :
		mmqos_hrt->md_speech_bw[md_index + 1];
}

s32 mtk_mmqos_hrt_scen(enum hrt_scen scen, bool is_start)
{
	s32 ret = 0;

	if (!mmqos_hrt) {
		pr_notice("%s: mmqos_hrt not ready\n");
		return -ENOENT;
	}

	pr_notice("%s: scen=%d, is_start=%d\n", __func__, scen, is_start);
	switch (scen) {
	case CAM_SCEN_CHANGE:
		notify_bw_throttle(scen, is_start);
		break;
	case MD_SPEECH:
		mmqos_hrt->in_speech = is_start;
		set_md_hrt_by_scen(mmqos_hrt->md_scen, is_start, mmqos_hrt->md_type);
		notify_bw_throttle(scen, is_start);
		break;
	default:
		pr_notice("%s: wrong hrt_scen(%d)\n", __func__, scen);
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_hrt_scen);

void mtk_mmqos_set_md_type(u32 md_type)
{
	mmqos_hrt->md_type = md_type;
	mtk_mmqos_hrt_scen(MD_SPEECH, mmqos_hrt->in_speech);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_set_md_type);

static ssize_t mtk_mmqos_scen_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	s32 ret;
	u32 scen, is_on;

	ret = sscanf(buf, "%u %u", &scen, &is_on);
	if (ret != 2) {
		dev_notice(dev, "%s: invalid input=%s ret=%d\n", __func__, buf, ret);
		return ret;
	}
	ret = mtk_mmqos_hrt_scen(scen, is_on == 1);
	if (ret)
		return ret;

	return count;
}

static DEVICE_ATTR_WO(mtk_mmqos_scen);

static int enable_vcp_blocking(void *data)
{
	atomic_inc(&mmqos_hrt->lock_count);
	pr_notice("%s: increase lock_count=%d\n", __func__,
		atomic_read(&mmqos_hrt->lock_count));

	if (!mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_MMQOS))
		mtk_mmdvfs_camera_notify_from_mmqos(true);

	atomic_dec(&mmqos_hrt->lock_count);
	wake_up(&mmqos_hrt->hrt_wait);
	pr_notice("%s: decrease lock_count=%d\n", __func__,
		atomic_read(&mmqos_hrt->lock_count));

	return 0;
}


static void set_camera_max_bw(u32 bw)
{
	struct task_struct *thread_vcp;

	pr_notice("%s: %d\n", __func__, bw);

	if (mmqos_hrt->cam_max_bw == 0 && bw > 0) {
		thread_vcp = kthread_run(enable_vcp_blocking, NULL, "enable vcp");
	} else if (mmqos_hrt->cam_max_bw > 0 && bw == 0) {
		mtk_mmdvfs_camera_notify_from_mmqos(false);
		mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_MMQOS);
	}

	mmqos_hrt->cam_max_bw = bw;
	record_cam_hrt(bw);

	if (mmqos_hrt->blocking && mmqos_hrt->cam_bw_inc) {
		atomic_inc(&mmqos_hrt->lock_count);
		pr_notice("%s: increase lock_count=%d\n", __func__,
			atomic_read(&mmqos_hrt->lock_count));
	}
	mtk_mmqos_hrt_scen(CAM_SCEN_CHANGE, bw > 0);
	if (mmqos_hrt->blocking && mmqos_hrt->cam_bw_inc) {
		atomic_dec(&mmqos_hrt->lock_count);
		wake_up(&mmqos_hrt->hrt_wait);
		pr_notice("%s: decrease lock_count=%d\n", __func__,
			atomic_read(&mmqos_hrt->lock_count));
	}
}

static void delay_work_handler(struct work_struct *work)
{
	mutex_lock(&mmqos_hrt->blocking_lock);
	set_camera_max_bw(mmqos_hrt->cam_occu_bw);
	mutex_unlock(&mmqos_hrt->blocking_lock);
}

static ssize_t camera_max_bw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	s32 ret;
	u32 bw = 0;

	ret = kstrtoint(buf, 10, &bw);
	if (ret) {
		dev_notice(dev, "wrong camera max bw string:%d\n", ret);
		return ret;
	}

	if (!mmqos_hrt) {
		dev_notice(dev, "mmqos_hrt is not ready\n");
		return -ENOENT;
	}

	bw = MULTIPLY_W_DRAM_WEIGHT(bw);
	if (mmqos_hrt->cam_occu_bw == bw)
		return count;

	cancel_delayed_work_sync(&mmqos_hrt->work);
	mmqos_hrt->cam_occu_bw = bw;
	mutex_lock(&mmqos_hrt->blocking_lock);
	if (mmqos_hrt->cam_occu_bw < mmqos_hrt->cam_max_bw) {
		mmqos_hrt->cam_bw_inc = false;
		schedule_delayed_work(&mmqos_hrt->work, 2 * HZ);
	} else {
		mmqos_hrt->cam_bw_inc = true;
		schedule_delayed_work(&mmqos_hrt->work, 0);
	}
	mutex_unlock(&mmqos_hrt->blocking_lock);

	return count;
}

static DEVICE_ATTR_WO(camera_max_bw);

void mtk_mmqos_init_hrt(struct mmqos_hrt *hrt)
{
	if (!hrt)
		return;
	mmqos_hrt = hrt;
	atomic_set(&mmqos_hrt->lock_count, 0);
	INIT_DELAYED_WORK(&mmqos_hrt->work, delay_work_handler);
	BLOCKING_INIT_NOTIFIER_HEAD(&mmqos_hrt->hrt_bw_throttle_notifier);
	mutex_init(&mmqos_hrt->blocking_lock);
	init_waitqueue_head(&mmqos_hrt->hrt_wait);
	mutex_init(&mmqos_hrt->hrt_rec.lock);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_init_hrt);

static struct attribute *mmqos_hrt_sysfs_attrs[] = {
	&dev_attr_camera_max_bw.attr,
	&dev_attr_mtk_mmqos_scen.attr,
	NULL
};

static struct attribute_group mmqos_hrt_sysfs_attr_group = {
	.name = "mmqos_hrt",
	.attrs = mmqos_hrt_sysfs_attrs
};

int mtk_mmqos_register_hrt_sysfs(struct device *dev)
{
	return sysfs_create_group(&dev->kobj, &mmqos_hrt_sysfs_attr_group);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_register_hrt_sysfs);

void  mtk_mmqos_unregister_hrt_sysfs(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &mmqos_hrt_sysfs_attr_group);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_unregister_hrt_sysfs);

module_param(mmqos_log_hrt_level, uint, 0644);
MODULE_PARM_DESC(mmqos_log_hrt_level, "mmqos hrt log level");

module_param(hrt_ftrace_ena, uint, 0644);
MODULE_PARM_DESC(hrt_ftrace_ena, "hrt ftrace enable");

MODULE_LICENSE("GPL v2");
