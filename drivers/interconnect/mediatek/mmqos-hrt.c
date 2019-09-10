// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#include <linux/module.h>
#include "mmqos-mtk.h"

#define MULTIPLY_W_DRAM_WEIGHT(value) ((value)*6/5)

struct mmqos_hrt *mmqos_hrt;

s32 mtk_mmqos_get_avail_hrt_bw(enum hrt_type type)
{
	u32 i, used_bw = 0;

	for (i = 0; i < HRT_TYPE_NUM; i++) {
		if (mmqos_hrt->hrt_bw[i] != type)
			used_bw += mmqos_hrt->hrt_bw[i];
	}

	if (mmqos_hrt->cam_max_bw)
		used_bw = used_bw - mmqos_hrt->hrt_bw[HRT_CAM]
				+ mmqos_hrt->cam_max_bw;

	return (mmqos_hrt->hrt_total_bw - used_bw);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_get_avail_hrt_bw);


s32 mtk_mmqos_register_bw_throttle_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(
				&mmqos_hrt->hrt_bw_throttle_notifier,
				nb);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_register_bw_throttle_notifier);

s32 mtk_mmqos_unregister_bw_throttle_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(
				&mmqos_hrt->hrt_bw_throttle_notifier,
				nb);
}
EXPORT_SYMBOL_GPL(mtk_mmqos_unregister_bw_throttle_notifier);

void mtk_mmqos_wait_throttle_done(void)
{
	u32 wait_result;

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

	mmqos_hrt->hrt_bw[type] = bw;
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_mmqos_set_hrt_bw);

static void notify_bw_throttle(u32 bw)
{
	u64 start_jiffies = jiffies;

	blocking_notifier_call_chain(&mmqos_hrt->hrt_bw_throttle_notifier,
		(bw > 0)?BW_THROTTLE_START:BW_THROTTLE_END, NULL);

	pr_notice("%s: notify_time=%u\n", __func__,
		jiffies_to_msecs(jiffies-start_jiffies));
}

static void set_camera_max_bw(u32 bw)
{
	mmqos_hrt->cam_max_bw = bw;
	pr_notice("%s: %d\n", __func__, bw);

	if (mmqos_hrt->blocking) {
		atomic_inc(&mmqos_hrt->lock_count);
		pr_notice("%s: increase lock_count=%d\n", __func__,
			atomic_read(&mmqos_hrt->lock_count));
	}
	notify_bw_throttle(bw);

	if (mmqos_hrt->blocking) {
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

	cancel_delayed_work_sync(&mmqos_hrt->work);
	mmqos_hrt->cam_occu_bw = MULTIPLY_W_DRAM_WEIGHT(bw);
	mutex_lock(&mmqos_hrt->blocking_lock);
	if (mmqos_hrt->cam_occu_bw < mmqos_hrt->cam_max_bw) {
		mmqos_hrt->blocking = false;
		schedule_delayed_work(&mmqos_hrt->work, 2 * HZ);
	} else {
		mmqos_hrt->blocking = true;
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
}
EXPORT_SYMBOL_GPL(mtk_mmqos_init_hrt);

static struct attribute *mmqos_hrt_sysfs_attrs[] = {
	&dev_attr_camera_max_bw.attr,
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


MODULE_LICENSE("GPL v2");
