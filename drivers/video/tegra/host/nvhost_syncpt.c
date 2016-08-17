/*
 * drivers/video/tegra/host/nvhost_syncpt.c
 *
 * Tegra Graphics Host Syncpoints
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION, All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/nvhost_ioctl.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <trace/events/nvhost.h>
#include "nvhost_syncpt.h"
#include "nvhost_acm.h"
#include "dev.h"
#include "chip_support.h"

#define MAX_SYNCPT_LENGTH	5

/* Name of sysfs node for min and max value */
static const char *min_name = "min";
static const char *max_name = "max";

/**
 * Resets syncpoint and waitbase values to sw shadows
 */
void nvhost_syncpt_reset(struct nvhost_syncpt *sp)
{
	u32 i;
	BUG_ON(!(syncpt_op().reset && syncpt_op().reset_wait_base));

	for (i = 0; i < nvhost_syncpt_nb_pts(sp); i++)
		syncpt_op().reset(sp, i);
	for (i = 0; i < nvhost_syncpt_nb_bases(sp); i++)
		syncpt_op().reset_wait_base(sp, i);
	wmb();
}

/**
 * Resets syncpoint and waitbase values of a
 * single client to sw shadows
 */
void nvhost_syncpt_reset_client(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_master *nvhost_master = nvhost_get_host(pdev);
	u32 id;

	BUG_ON(!(syncpt_op().reset && syncpt_op().reset_wait_base));

	for_each_set_bit(id, (unsigned long *)&pdata->syncpts, BITS_PER_LONG)
		syncpt_op().reset(&nvhost_master->syncpt, id);
	for_each_set_bit(id, (unsigned long *)&pdata->waitbases, BITS_PER_LONG)
		syncpt_op().reset_wait_base(&nvhost_master->syncpt, id);
	wmb();
}


/**
 * Updates sw shadow state for client managed registers
 */
void nvhost_syncpt_save(struct nvhost_syncpt *sp)
{
	u32 i;
	BUG_ON(!(syncpt_op().update_min && syncpt_op().read_wait_base));

	for (i = 0; i < nvhost_syncpt_nb_pts(sp); i++) {
		if (nvhost_syncpt_client_managed(sp, i))
			syncpt_op().update_min(sp, i);
		else
			BUG_ON(!nvhost_syncpt_min_eq_max(sp, i));
	}

	for (i = 0; i < nvhost_syncpt_nb_bases(sp); i++)
		syncpt_op().read_wait_base(sp, i);
}

/**
 * Updates the last value read from hardware.
 */
u32 nvhost_syncpt_update_min(struct nvhost_syncpt *sp, u32 id)
{
	u32 val;

	BUG_ON(!syncpt_op().update_min);

	val = syncpt_op().update_min(sp, id);
	trace_nvhost_syncpt_update_min(id, val);

	return val;
}

/**
 * Get the current syncpoint value
 */
u32 nvhost_syncpt_read(struct nvhost_syncpt *sp, u32 id)
{
	u32 val;
	BUG_ON(!syncpt_op().update_min);
	nvhost_module_busy(syncpt_to_dev(sp)->dev);
	val = syncpt_op().update_min(sp, id);
	nvhost_module_idle(syncpt_to_dev(sp)->dev);
	return val;
}

/**
 * Get the current syncpoint base
 */
u32 nvhost_syncpt_read_wait_base(struct nvhost_syncpt *sp, u32 id)
{
	u32 val;
	BUG_ON(!syncpt_op().read_wait_base);
	nvhost_module_busy(syncpt_to_dev(sp)->dev);
	syncpt_op().read_wait_base(sp, id);
	val = sp->base_val[id];
	nvhost_module_idle(syncpt_to_dev(sp)->dev);
	return val;
}

/**
 * Write a cpu syncpoint increment to the hardware, without touching
 * the cache. Caller is responsible for host being powered.
 */
void nvhost_syncpt_cpu_incr(struct nvhost_syncpt *sp, u32 id)
{
	BUG_ON(!syncpt_op().cpu_incr);
	syncpt_op().cpu_incr(sp, id);
}

/**
 * Increment syncpoint value from cpu, updating cache
 */
void nvhost_syncpt_incr(struct nvhost_syncpt *sp, u32 id)
{
	if (nvhost_syncpt_client_managed(sp, id))
		nvhost_syncpt_incr_max(sp, id, 1);
	nvhost_module_busy(syncpt_to_dev(sp)->dev);
	nvhost_syncpt_cpu_incr(sp, id);
	nvhost_module_idle(syncpt_to_dev(sp)->dev);
}

/**
 * Updated sync point form hardware, and returns true if syncpoint is expired,
 * false if we may need to wait
 */
static bool syncpt_update_min_is_expired(
	struct nvhost_syncpt *sp,
	u32 id,
	u32 thresh)
{
	syncpt_op().update_min(sp, id);
	return nvhost_syncpt_is_expired(sp, id, thresh);
}

/**
 * Main entrypoint for syncpoint value waits.
 */
int nvhost_syncpt_wait_timeout(struct nvhost_syncpt *sp, u32 id,
			u32 thresh, u32 timeout, u32 *value)
{
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(wq);
	void *ref;
	void *waiter;
	int err = 0, check_count = 0, low_timeout = 0;
	u32 val;

	if (value)
		*value = 0;

	/* first check cache */
	if (nvhost_syncpt_is_expired(sp, id, thresh)) {
		if (value)
			*value = nvhost_syncpt_read_min(sp, id);
		return 0;
	}

	/* keep host alive */
	nvhost_module_busy(syncpt_to_dev(sp)->dev);

	/* try to read from register */
	val = syncpt_op().update_min(sp, id);
	if (nvhost_syncpt_is_expired(sp, id, thresh)) {
		if (value)
			*value = val;
		goto done;
	}

	if (!timeout) {
		err = -EAGAIN;
		goto done;
	}

	/* schedule a wakeup when the syncpoint value is reached */
	waiter = nvhost_intr_alloc_waiter();
	if (!waiter) {
		err = -ENOMEM;
		goto done;
	}

	err = nvhost_intr_add_action(&(syncpt_to_dev(sp)->intr), id, thresh,
				NVHOST_INTR_ACTION_WAKEUP_INTERRUPTIBLE, &wq,
				waiter,
				&ref);
	if (err)
		goto done;

	err = -EAGAIN;
	/* Caller-specified timeout may be impractically low */
	if (timeout < SYNCPT_CHECK_PERIOD)
		low_timeout = timeout;

	/* wait for the syncpoint, or timeout, or signal */
	while (timeout) {
		u32 check = min_t(u32, SYNCPT_CHECK_PERIOD, timeout);
		int remain = wait_event_interruptible_timeout(wq,
				syncpt_update_min_is_expired(sp, id, thresh),
				check);
		if (remain > 0 || nvhost_syncpt_is_expired(sp, id, thresh)) {
			if (value)
				*value = nvhost_syncpt_read_min(sp, id);
			err = 0;
			break;
		}
		if (remain < 0) {
			err = remain;
			break;
		}
		if (timeout != NVHOST_NO_TIMEOUT)
			timeout -= check;
		if (timeout && check_count <= MAX_STUCK_CHECK_COUNT) {
			dev_warn(&syncpt_to_dev(sp)->dev->dev,
				"%s: syncpoint id %d (%s) stuck waiting %d, timeout=%d\n",
				 current->comm, id, syncpt_op().name(sp, id),
				 thresh, timeout);
			syncpt_op().debug(sp);
			if (check_count == MAX_STUCK_CHECK_COUNT) {
				if (low_timeout) {
					dev_warn(&syncpt_to_dev(sp)->dev->dev,
						"is timeout %d too low?\n",
						low_timeout);
				}
				nvhost_debug_dump(syncpt_to_dev(sp));
			}
			check_count++;
		}
	}
	nvhost_intr_put_ref(&(syncpt_to_dev(sp)->intr), id, ref);

done:
	nvhost_module_idle(syncpt_to_dev(sp)->dev);
	return err;
}

/**
 * Returns true if syncpoint is expired, false if we may need to wait
 */
bool nvhost_syncpt_is_expired(
	struct nvhost_syncpt *sp,
	u32 id,
	u32 thresh)
{
	u32 current_val;
	u32 future_val;
	smp_rmb();
	current_val = (u32)atomic_read(&sp->min_val[id]);
	future_val = (u32)atomic_read(&sp->max_val[id]);

	/* Note the use of unsigned arithmetic here (mod 1<<32).
	 *
	 * c = current_val = min_val	= the current value of the syncpoint.
	 * t = thresh			= the value we are checking
	 * f = future_val  = max_val	= the value c will reach when all
	 *			   	  outstanding increments have completed.
	 *
	 * Note that c always chases f until it reaches f.
	 *
	 * Dtf = (f - t)
	 * Dtc = (c - t)
	 *
	 *  Consider all cases:
	 *
	 *	A) .....c..t..f.....	Dtf < Dtc	need to wait
	 *	B) .....c.....f..t..	Dtf > Dtc	expired
	 *	C) ..t..c.....f.....	Dtf > Dtc	expired	   (Dct very large)
	 *
	 *  Any case where f==c: always expired (for any t).  	Dtf == Dcf
	 *  Any case where t==c: always expired (for any f).  	Dtf >= Dtc (because Dtc==0)
	 *  Any case where t==f!=c: always wait.	 	Dtf <  Dtc (because Dtf==0,
	 *							Dtc!=0)
	 *
	 *  Other cases:
	 *
	 *	A) .....t..f..c.....	Dtf < Dtc	need to wait
	 *	A) .....f..c..t.....	Dtf < Dtc	need to wait
	 *	A) .....f..t..c.....	Dtf > Dtc	expired
	 *
	 *   So:
	 *	   Dtf >= Dtc implies EXPIRED	(return true)
	 *	   Dtf <  Dtc implies WAIT	(return false)
	 *
	 * Note: If t is expired then we *cannot* wait on it. We would wait
	 * forever (hang the system).
	 *
	 * Note: do NOT get clever and remove the -thresh from both sides. It
	 * is NOT the same.
	 *
	 * If future valueis zero, we have a client managed sync point. In that
	 * case we do a direct comparison.
	 */
	if (!nvhost_syncpt_client_managed(sp, id))
		return future_val - thresh >= current_val - thresh;
	else
		return (s32)(current_val - thresh) >= 0;
}

void nvhost_syncpt_debug(struct nvhost_syncpt *sp)
{
	syncpt_op().debug(sp);
}

int nvhost_mutex_try_lock(struct nvhost_syncpt *sp, int idx)
{
	struct nvhost_master *host = syncpt_to_dev(sp);
	u32 reg;

	nvhost_module_busy(host->dev);
	reg = syncpt_op().mutex_try_lock(sp, idx);
	if (reg) {
		nvhost_module_idle(host->dev);
		return -EBUSY;
	}
	atomic_inc(&sp->lock_counts[idx]);
	return 0;
}

void nvhost_mutex_unlock(struct nvhost_syncpt *sp, int idx)
{
	syncpt_op().mutex_unlock(sp, idx);
	nvhost_module_idle(syncpt_to_dev(sp)->dev);
	atomic_dec(&sp->lock_counts[idx]);
}

/* remove a wait pointed to by patch_addr */
int nvhost_syncpt_patch_wait(struct nvhost_syncpt *sp, void *patch_addr)
{
	return syncpt_op().patch_wait(sp, patch_addr);
}

/* Displays the current value of the sync point via sysfs */
static ssize_t syncpt_min_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct nvhost_syncpt_attr *syncpt_attr =
		container_of(attr, struct nvhost_syncpt_attr, attr);

	return snprintf(buf, PAGE_SIZE, "%u",
			nvhost_syncpt_read(&syncpt_attr->host->syncpt,
				syncpt_attr->id));
}

static ssize_t syncpt_max_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct nvhost_syncpt_attr *syncpt_attr =
		container_of(attr, struct nvhost_syncpt_attr, attr);

	return snprintf(buf, PAGE_SIZE, "%u",
			nvhost_syncpt_read_max(&syncpt_attr->host->syncpt,
				syncpt_attr->id));
}

int nvhost_syncpt_init(struct platform_device *dev,
		struct nvhost_syncpt *sp)
{
	int i;
	struct nvhost_master *host = syncpt_to_dev(sp);
	int err = 0;

	/* Allocate structs for min, max and base values */
	sp->min_val = kzalloc(sizeof(atomic_t) * nvhost_syncpt_nb_pts(sp),
			GFP_KERNEL);
	sp->max_val = kzalloc(sizeof(atomic_t) * nvhost_syncpt_nb_pts(sp),
			GFP_KERNEL);
	sp->base_val = kzalloc(sizeof(u32) * nvhost_syncpt_nb_bases(sp),
			GFP_KERNEL);
	sp->lock_counts =
		kzalloc(sizeof(atomic_t) * nvhost_syncpt_nb_mlocks(sp),
			GFP_KERNEL);

	if (!(sp->min_val && sp->max_val && sp->base_val && sp->lock_counts)) {
		/* frees happen in the deinit */
		err = -ENOMEM;
		goto fail;
	}

	sp->kobj = kobject_create_and_add("syncpt", &dev->dev.kobj);
	if (!sp->kobj) {
		err = -EIO;
		goto fail;
	}

	/* Allocate two attributes for each sync point: min and max */
	sp->syncpt_attrs = kzalloc(sizeof(*sp->syncpt_attrs)
			* nvhost_syncpt_nb_pts(sp) * 2, GFP_KERNEL);
	if (!sp->syncpt_attrs) {
		err = -ENOMEM;
		goto fail;
	}

	/* Fill in the attributes */
	for (i = 0; i < nvhost_syncpt_nb_pts(sp); i++) {
		char name[MAX_SYNCPT_LENGTH];
		struct kobject *kobj;
		struct nvhost_syncpt_attr *min = &sp->syncpt_attrs[i*2];
		struct nvhost_syncpt_attr *max = &sp->syncpt_attrs[i*2+1];

		/* Create one directory per sync point */
		snprintf(name, sizeof(name), "%d", i);
		kobj = kobject_create_and_add(name, sp->kobj);
		if (!kobj) {
			err = -EIO;
			goto fail;
		}

		min->id = i;
		min->host = host;
		min->attr.attr.name = min_name;
		min->attr.attr.mode = S_IRUGO;
		min->attr.show = syncpt_min_show;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
		sysfs_attr_init(&min->attr.attr);
		sysfs_attr_init(&max->attr.attr);
#endif
		if (sysfs_create_file(kobj, &min->attr.attr)) {
			err = -EIO;
			goto fail;
		}

		max->id = i;
		max->host = host;
		max->attr.attr.name = max_name;
		max->attr.attr.mode = S_IRUGO;
		max->attr.show = syncpt_max_show;
		if (sysfs_create_file(kobj, &max->attr.attr)) {
			err = -EIO;
			goto fail;
		}
	}

	return err;

fail:
	nvhost_syncpt_deinit(sp);
	return err;
}

void nvhost_syncpt_deinit(struct nvhost_syncpt *sp)
{
	kobject_put(sp->kobj);

	kfree(sp->min_val);
	sp->min_val = NULL;

	kfree(sp->max_val);
	sp->max_val = NULL;

	kfree(sp->base_val);
	sp->base_val = NULL;

	kfree(sp->lock_counts);
	sp->lock_counts = 0;

	kfree(sp->syncpt_attrs);
	sp->syncpt_attrs = NULL;
}

int nvhost_syncpt_client_managed(struct nvhost_syncpt *sp, u32 id)
{
	return BIT(id) & syncpt_to_dev(sp)->info.client_managed;
}

int nvhost_syncpt_nb_pts(struct nvhost_syncpt *sp)
{
	return syncpt_to_dev(sp)->info.nb_pts;
}

int nvhost_syncpt_nb_bases(struct nvhost_syncpt *sp)
{
	return syncpt_to_dev(sp)->info.nb_bases;
}

int nvhost_syncpt_nb_mlocks(struct nvhost_syncpt *sp)
{
	return syncpt_to_dev(sp)->info.nb_mlocks;
}

/* public sync point API */
u32 nvhost_syncpt_incr_max_ext(struct platform_device *dev, u32 id, u32 incrs)
{
	struct platform_device *pdev;
	struct nvhost_syncpt *sp;

	BUG_ON(!nvhost_get_parent(dev));

	/* get the parent */
	pdev = to_platform_device(dev->dev.parent);
	sp = &(nvhost_get_host(pdev)->syncpt);

	return nvhost_syncpt_incr_max(sp, id, incrs);
}

void nvhost_syncpt_cpu_incr_ext(struct platform_device *dev, u32 id)
{
	struct platform_device *pdev;
	struct nvhost_syncpt *sp;

	BUG_ON(!nvhost_get_parent(dev));

	/* get the parent */
	pdev = to_platform_device(dev->dev.parent);
	sp = &(nvhost_get_host(pdev)->syncpt);

	nvhost_syncpt_cpu_incr(sp, id);
}

u32 nvhost_syncpt_read_ext(struct platform_device *dev, u32 id)
{
	struct platform_device *pdev;
	struct nvhost_syncpt *sp;

	BUG_ON(!nvhost_get_parent(dev));

	/* get the parent */
	pdev = to_platform_device(dev->dev.parent);
	sp = &(nvhost_get_host(pdev)->syncpt);

	return nvhost_syncpt_read(sp, id);
}

int nvhost_syncpt_wait_timeout_ext(struct platform_device *dev, u32 id,
	u32 thresh, u32 timeout, u32 *value)
{
	struct platform_device *pdev;
	struct nvhost_syncpt *sp;

	BUG_ON(!nvhost_get_parent(dev));

	/* get the parent */
	pdev = to_platform_device(dev->dev.parent);
	sp = &(nvhost_get_host(pdev)->syncpt);

	return nvhost_syncpt_wait_timeout(sp, id, thresh, timeout, value);
}
