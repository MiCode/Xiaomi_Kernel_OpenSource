/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include "vpu_cfg.h"
#include <linux/types.h>
#include <linux/printk.h>
#include "vpu_power.h"
#include "vpu_debug.h"
#include "vpu_algo.h"
#include "vpu_cmd.h"
#include "vpu_met.h"
#include "vpu_hw.h"
#include "apusys_power.h"

static void vpu_pwr_off_locked(struct vpu_device *vd, int suspend);
static void vpu_pwr_wake_lock(struct vpu_device *vd);
static void vpu_pwr_wake_unlock(struct vpu_device *vd);

/* Get APUSYS DVFS User ID from VPU ID */
static inline int adu(int id)
{
	return (VPU0 + id);
}

static void vpu_pwr_off_timer(struct vpu_device *vd, unsigned long t)
{
	mod_delayed_work(vpu_drv->wq,
		&vd->pw_off_work,
		msecs_to_jiffies(t));
}

static void vpu_pwr_release(struct kref *ref)
{
	struct vpu_device *vd
		= container_of(ref, struct vpu_device, pw_ref);
	unsigned long t = (unsigned long)vd->pw_off_latency;

	if (!t)  /* t = 0: always on, return */
		return;

	if (t == 1)  /* t = 1: instant power off */
		t = 0;

	vpu_pwr_debug("%s: vpu%d: vpu_pwr_off_timer: %ld ms\n",
		__func__, vd->id, t);
	vpu_pwr_wake_unlock(vd);
	vpu_pwr_off_timer(vd, t /* ms */);
}

static void vpu_pwr_param(struct vpu_device *vd, uint8_t boost)
{
	uint8_t opp;

	if (boost == VPU_PWR_NO_BOOST)
		return;

	opp = apusys_boost_value_to_opp(adu(vd->id), boost);

	vpu_pwr_debug("%s: vpu%d: boost: %d, opp: %d\n",
		__func__, vd->id, boost, opp);

	apu_device_set_opp(adu(vd->id), opp);
	atomic_set(&vd->pw_boost, boost);
}

/**
 * vpu_pwr_get_locked() - acquire vpu power and increase power reference
 * @vd: vpu device
 * @boost: boost value: 0~100, given from vpu_request
 *
 * 1. vd->lock and vpu_cmd_lock() must be locked before calling this function
 * 2. Must paired with vpu_pwr_put_locked()
 */
int vpu_pwr_get_locked(struct vpu_device *vd, uint8_t boost)
{
	int ret = 0;

	if (vd->state == VS_REMOVING) {
		pr_info("%s: vpu%d is going to be removed\n",
			__func__, vd->id);
		return -ENODEV;
	}

	cancel_delayed_work(&vd->pw_off_work);

	if (kref_get_unless_zero(&vd->pw_ref)) {
		vpu_pwr_debug("%s: vpu%d: ref: %d, boost: %d\n",
			__func__, vd->id, kref_read(&vd->pw_ref), boost);
		goto out;
	}

	if (vd->state == VS_DOWN) {
		vpu_pwr_debug("%s: vpu%d: apu_device_power_on\n",
			__func__, vd->id);
		ret = apu_device_power_on(adu(vd->id));
		if (ret) {
			vpu_pwr_debug("%s: vpu%d: apu_device_power_on: failed: %d\n",
				__func__, vd->id, ret);
			goto err;
		}
		vd->state = VS_UP;
	}

	ret = vpu_kbuf_alloc(vd);
	if (ret)
		goto err;

	kref_init(&vd->pw_ref);
	atomic_set(&vd->pw_boost, VPU_PWR_NO_BOOST);
	vpu_pwr_wake_lock(vd);
	vpu_pwr_debug("%s: vpu%d: ref: 1\n", __func__, vd->id);

out:
	vpu_pwr_param(vd, boost);
err:
	return ret;
}

/**
 * vpu_pwr_put_locked() - decrease power reference
 * @vd: vpu device
 * @boost: boost value to be restored
 *
 * 1. Must paired with vpu_pwr_get_locked()
 * 2. There's no need to acquire vd->lock or vpu_cmd_lock()
 *    since the power off is done asynchronously by
 *    vpu_pwr_off() when vd->pw_ref reaches zero,
 *    and kref_put() itself is atomic.
 */
void vpu_pwr_put_locked(struct vpu_device *vd, uint8_t boost)
{
	if (!kref_read(&vd->pw_ref)) {
		vpu_pwr_debug("%s: vpu%d: ref is already zero\n",
			__func__, vd->id);
		return;
	}
	vpu_pwr_debug("%s: vpu%d: ref: %d--\n",
		__func__, vd->id, vpu_pwr_cnt(vd));
	vpu_pwr_param(vd, boost);
	kref_put(&vd->pw_ref, vpu_pwr_release);
}

/**
 * vpu_pwr_up_locked() - unconditionally power up VPU
 * @vd: vpu device
 * @boost: boost value: 0~100, given from vpu_request
 * @off_timer: set power off timer (us), 0: always on
 *
 * vd->lock and vpu_cmd_lock() must be locked before calling this function
 */
int vpu_pwr_up_locked(struct vpu_device *vd, uint8_t boost, uint32_t off_timer)
{
	int ret;
	uint64_t pol;

	ret = vpu_pwr_get_locked(vd, boost);
	if (ret)
		return ret;

	if (off_timer) {
		pol = vd->pw_off_latency;
		vd->pw_off_latency = off_timer;
		/* No need to restore boost for unconditionally power up */
		vpu_pwr_put_locked_nb(vd);
		vd->pw_off_latency = pol;
	}

	vpu_pwr_debug("%s: powered up %d\n", __func__, ret);
	return 0;
}

/**
 * vpu_pwr_down_ext_locked() - unconditionally power down VPU
 * @vd: vpu device
 * @suspend: 0: normal power off flow, 1: suspend power off flow
 * vd->lock and vpu_cmd_lock() must be locked before calling this function
 */
static void vpu_pwr_down_ext_locked(struct vpu_device *vd, int suspend)
{
	if (kref_read(&vd->pw_ref))
		vpu_pwr_wake_unlock(vd);

	refcount_set(&vd->pw_ref.refcount, 0);
	cancel_delayed_work(&vd->pw_off_work);
	vpu_pwr_off_locked(vd, suspend);
	vpu_pwr_debug("%s: vpu%d: powered down(%d)\n",
		__func__, vd->id, suspend);
}

/**
 * vpu_pwr_down_locked() - unconditionally power down VPU
 * @vd: vpu device
 *
 * vd->lock and vpu_cmd_lock() must be locked before calling this function
 */
void vpu_pwr_down_locked(struct vpu_device *vd)
{
	vpu_pwr_down_ext_locked(vd, /* suspend */ 0);
}

/**
 * vpu_pwr_suspend_locked() - power down VPU when suspend
 * @vd: vpu device
 *
 * vd->lock and vpu_cmd_lock() must be locked before calling this function
 */
void vpu_pwr_suspend_locked(struct vpu_device *vd)
{
	vpu_pwr_down_ext_locked(vd, /* suspend */ 1);
}

/**
 * vpu_pwr_up() - unconditionally power up VPU
 * @vd: vpu device
 * @boost: boost value: 0~100, given from vpu_request
 * @off_timer: set power off timer (us), 0: always on
 *
 */
int vpu_pwr_up(struct vpu_device *vd, uint8_t boost, uint32_t off_timer)
{
	int ret;

	vpu_cmd_lock_all(vd);
	ret = vpu_pwr_up_locked(vd, boost, off_timer);
	vpu_cmd_unlock_all(vd);
	return ret;
}

/**
 * vpu_pwr_down() - unconditionally power down VPU
 * @vd: vpu device
 *
 */
void vpu_pwr_down(struct vpu_device *vd)
{
	vpu_cmd_lock_all(vd);
	vpu_pwr_down_locked(vd);
	vpu_cmd_unlock_all(vd);
}

static void vpu_pwr_off_locked(struct vpu_device *vd, int suspend)
{
	int ret;
	int adu_id = adu(vd->id);

	vpu_cmd_clear(vd);
	vpu_kbuf_free(vd);
	vpu_met_pm_put(vd);

	if (vd->state <= VS_DOWN) {
		vpu_pwr_debug("%s: vpu%d: already off: %d\n",
			__func__, vd->id, vd->state);
		return;
	}

	if (vpu_xos_wait_idle(vd))
		pr_info("%s: vpu%d: not in idle state\n", __func__, vd->id);

	vpu_pwr_debug("%s: vpu%d: apu_device_power_suspend(%d, %d)\n",
		__func__, vd->id, adu_id, suspend);

	ret = apu_device_power_suspend(adu_id, suspend);
	if (ret)
		vpu_pwr_debug("%s: vpu%d: apu_device_power_suspend(%d, %d): failed: %d\n",
			__func__, vd->id, adu_id, suspend, ret);

	vd->state = VS_DOWN;

	/* Unlock xos_lock, in case the the lock is still hold by us.
	 * Other xos_lock requesters will get -EAGAIN at VS_DOWN state.
	 */
	vpu_xos_unlock(vd);
}

/**
 * vpu_pwr_off() - worker function of delayed power off
 * @vd: vpu device
 *
 */
static void vpu_pwr_off(struct work_struct *work)
{
	struct vpu_device *vd
		= container_of(work, struct vpu_device, pw_off_work.work);

	vpu_pwr_down(vd);
	wake_up_interruptible(&vd->pw_wait);
}

static void vpu_pwr_wake_lock(struct vpu_device *vd)
{
#ifdef CONFIG_PM_SLEEP
	__pm_stay_awake(&vd->pw_wake_lock);
#endif
}

static void vpu_pwr_wake_unlock(struct vpu_device *vd)
{
#ifdef CONFIG_PM_SLEEP
	__pm_relax(&vd->pw_wake_lock);
#endif
}

static void vpu_pwr_wake_init(struct vpu_device *vd)
{
#ifdef CONFIG_PM_SLEEP
	wakeup_source_init(&vd->pw_wake_lock, vd->name);
#endif
}

int vpu_init_dev_pwr(struct platform_device *pdev, struct vpu_device *vd)
{
	int ret = 0;

	INIT_DELAYED_WORK(&vd->pw_off_work, vpu_pwr_off);
	init_waitqueue_head(&vd->pw_wait);
	vd->pw_off_latency = VPU_PWR_OFF_LATENCY;
	refcount_set(&vd->pw_ref.refcount, 0);

	vpu_drv_debug("%s: vpu%d: apu_power_device_register call\n",
		__func__, vd->id);

	ret = apu_power_device_register(adu(vd->id), pdev);

	if (ret)
		dev_info(&pdev->dev, "apu_power_device_register: %d\n",
			ret);

	vpu_pwr_wake_init(vd);

	return ret;
}

void vpu_exit_dev_pwr(struct platform_device *pdev, struct vpu_device *vd)
{
	cancel_delayed_work(&vd->pw_off_work);
	apu_device_power_off(adu(vd->id));
	apu_power_device_unregister(adu(vd->id));
}

