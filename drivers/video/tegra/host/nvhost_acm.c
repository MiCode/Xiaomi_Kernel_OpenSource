/*
 * drivers/video/tegra/host/nvhost_acm.c
 *
 * Tegra Graphics Host Automatic Clock Management
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <trace/events/nvhost.h>

#include <mach/powergate.h>
#include <mach/clk.h>
#include <mach/hardware.h>
#include <mach/mc.h>

#include "nvhost_acm.h"
#include "dev.h"

#define ACM_SUSPEND_WAIT_FOR_IDLE_TIMEOUT	(2 * HZ)
#define POWERGATE_DELAY 			10
#define MAX_DEVID_LENGTH			16

DEFINE_MUTEX(client_list_lock);

struct nvhost_module_client {
	struct list_head node;
	unsigned long rate[NVHOST_MODULE_MAX_CLOCKS];
	void *priv;
};

static void do_powergate_locked(int id)
{
	if (id != -1 && tegra_powergate_is_powered(id))
		tegra_powergate_partition(id);
}

static void do_unpowergate_locked(int id)
{
	int ret = 0;
	if (id != -1) {
		ret = tegra_unpowergate_partition(id);
		if (ret)
			pr_err("%s: unpowergate failed: id = %d\n",
					__func__, id);
	}
}

static void do_module_reset_locked(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	/* assert module and mc client reset */
	if (pdata->powergate_ids[0] != -1)
		tegra_powergate_mc_flush(pdata->powergate_ids[0]);
	if (pdata->powergate_ids[0] != -1)
		tegra_powergate_mc_disable(pdata->powergate_ids[0]);
	if (pdata->clocks[0].reset)
		tegra_periph_reset_assert(pdata->clk[0]);

	if (pdata->powergate_ids[1] != -1)
		tegra_powergate_mc_flush(pdata->powergate_ids[1]);
	if (pdata->powergate_ids[1] != -1)
		tegra_powergate_mc_disable(pdata->powergate_ids[1]);
	if (pdata->clocks[1].reset)
		tegra_periph_reset_assert(pdata->clk[1]);

	udelay(POWERGATE_DELAY);

	/* deassert reset */
	if (pdata->clocks[0].reset)
		tegra_periph_reset_deassert(pdata->clk[0]);
	if (pdata->powergate_ids[0] != -1)
		tegra_powergate_mc_enable(pdata->powergate_ids[0]);
	if (pdata->powergate_ids[0] != -1)
		tegra_powergate_mc_flush_done(pdata->powergate_ids[0]);

	if (pdata->clocks[1].reset)
		tegra_periph_reset_deassert(pdata->clk[1]);
	if (pdata->powergate_ids[1] != -1)
		tegra_powergate_mc_enable(pdata->powergate_ids[1]);
	if (pdata->powergate_ids[1] != -1)
		tegra_powergate_mc_flush_done(pdata->powergate_ids[1]);
}

void nvhost_module_reset(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	dev_dbg(&dev->dev,
		"%s: asserting %s module reset (id %d, id2 %d)\n",
		__func__, dev_name(&dev->dev),
		pdata->powergate_ids[0], pdata->powergate_ids[1]);

	mutex_lock(&pdata->lock);
	do_module_reset_locked(dev);
	mutex_unlock(&pdata->lock);

	if (pdata->finalize_poweron)
		pdata->finalize_poweron(dev);

	dev_dbg(&dev->dev, "%s: module %s out of reset\n",
		__func__, dev_name(&dev->dev));
}

static void to_state_clockgated_locked(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	if (pdata->powerstate == NVHOST_POWER_STATE_RUNNING) {
		int i, err;
		if (pdata->prepare_clockoff) {
			err = pdata->prepare_clockoff(dev);
			if (err) {
				dev_err(&dev->dev, "error clock gating");
				return;
			}
		}

		for (i = 0; i < pdata->num_clks; i++)
			clk_disable_unprepare(pdata->clk[i]);

		if (nvhost_get_parent(dev))
			nvhost_module_idle(to_platform_device(dev->dev.parent));

		if (!pdata->can_powergate) {
			pm_runtime_mark_last_busy(&dev->dev);
			pm_runtime_put_autosuspend(&dev->dev);
		}
	} else if (pdata->powerstate == NVHOST_POWER_STATE_POWERGATED
			&& pdata->can_powergate) {
		do_unpowergate_locked(pdata->powergate_ids[0]);
		do_unpowergate_locked(pdata->powergate_ids[1]);

		if (pdata->powerup_reset)
			do_module_reset_locked(dev);
	}
	pdata->powerstate = NVHOST_POWER_STATE_CLOCKGATED;
}

static void to_state_running_locked(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	int prev_state = pdata->powerstate;

	if (pdata->powerstate == NVHOST_POWER_STATE_POWERGATED) {
		pm_runtime_get_sync(&dev->dev);
		to_state_clockgated_locked(dev);
	}

	if (pdata->powerstate == NVHOST_POWER_STATE_CLOCKGATED) {
		int i;

		if (!pdata->can_powergate)
			pm_runtime_get_sync(&dev->dev);

		if (nvhost_get_parent(dev))
			nvhost_module_busy(to_platform_device(dev->dev.parent));

		for (i = 0; i < pdata->num_clks; i++) {
			int err = clk_prepare_enable(pdata->clk[i]);
			if (err) {
				dev_err(&dev->dev, "Cannot turn on clock %s",
					pdata->clocks[i].name);
				return;
			}
		}

		if (pdata->finalize_clockon)
			pdata->finalize_clockon(dev);

		/* Invoke callback after power un-gating. This is used for
		 * restoring context. */
		if (prev_state == NVHOST_POWER_STATE_POWERGATED
				&& pdata->finalize_poweron)
			pdata->finalize_poweron(dev);
	}
	pdata->powerstate = NVHOST_POWER_STATE_RUNNING;
}

/* This gets called from powergate_handler() and from module suspend.
 * Module suspend is done for all modules, runtime power gating only
 * for modules with can_powergate set.
 */
static int to_state_powergated_locked(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	int err = 0;

	if (pdata->prepare_poweroff &&
		pdata->powerstate != NVHOST_POWER_STATE_POWERGATED) {
		/* Clock needs to be on in prepare_poweroff */
		to_state_running_locked(dev);
		err = pdata->prepare_poweroff(dev);
		if (err)
			return err;
	}

	if (pdata->powerstate == NVHOST_POWER_STATE_RUNNING)
		to_state_clockgated_locked(dev);

	if (pdata->can_powergate) {
		do_powergate_locked(pdata->powergate_ids[0]);
		do_powergate_locked(pdata->powergate_ids[1]);
	}

	pdata->powerstate = NVHOST_POWER_STATE_POWERGATED;
	pm_runtime_mark_last_busy(&dev->dev);
	pm_runtime_put_autosuspend(&dev->dev);
	return 0;
}

static void schedule_powergating_locked(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	if (pdata->can_powergate)
		schedule_delayed_work(&pdata->powerstate_down,
				msecs_to_jiffies(pdata->powergate_delay));
}

static void schedule_clockgating_locked(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	schedule_delayed_work(&pdata->powerstate_down,
			msecs_to_jiffies(pdata->clockgate_delay));
}

void nvhost_module_busy(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	if (pdata->busy)
		pdata->busy(dev);

	mutex_lock(&pdata->lock);
	cancel_delayed_work(&pdata->powerstate_down);

	pdata->refcount++;
	if (pdata->refcount > 0 && !nvhost_module_powered(dev))
		to_state_running_locked(dev);
	mutex_unlock(&pdata->lock);
}

static void powerstate_down_handler(struct work_struct *work)
{
	struct platform_device *dev;
	struct nvhost_device_data *pdata;

	pdata = container_of(to_delayed_work(work),
			struct nvhost_device_data,
			powerstate_down);

	dev = pdata->pdev;

	mutex_lock(&pdata->lock);
	if (pdata->refcount == 0) {
		switch (pdata->powerstate) {
		case NVHOST_POWER_STATE_RUNNING:
			to_state_clockgated_locked(dev);
			schedule_powergating_locked(dev);
			break;
		case NVHOST_POWER_STATE_CLOCKGATED:
			if (to_state_powergated_locked(dev))
				schedule_powergating_locked(dev);
			break;
		default:
			break;
		}
	}
	mutex_unlock(&pdata->lock);
}

void nvhost_module_idle_mult(struct platform_device *dev, int refs)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	bool kick = false;

	mutex_lock(&pdata->lock);

	pdata->refcount -= refs;

	/* submits on the fly -> exit */
	if (pdata->refcount)
		goto out;

	if (pdata->idle) {
		/* give a reference for idle(). otherwise the dev can be
		 * clockgated */
		pdata->refcount++;
		mutex_unlock(&pdata->lock);
		pdata->idle(dev);
		mutex_lock(&pdata->lock);
		pdata->refcount--;
	}

	/* check that we don't have any new submits on the channel */
	if (pdata->refcount)
		goto out;

	/* no new submits. just schedule clock gating */
	kick = true;
	if (nvhost_module_powered(dev))
		schedule_clockgating_locked(dev);

out:
	mutex_unlock(&pdata->lock);

	/* wake up a waiting thread if we actually went to idle state */
	if (kick)
		wake_up(&pdata->idle_wq);

}

int nvhost_module_get_rate(struct platform_device *dev, unsigned long *rate,
		int index)
{
	struct clk *c;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	c = pdata->clk[index];
	if (IS_ERR_OR_NULL(c))
		return -EINVAL;

	/* Need to enable client to get correct rate */
	nvhost_module_busy(dev);
	*rate = clk_get_rate(c);
	nvhost_module_idle(dev);
	return 0;

}

static int nvhost_module_update_rate(struct platform_device *dev, int index)
{
	unsigned long rate = 0;
	struct nvhost_module_client *m;
	unsigned long devfreq_rate, default_rate;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	int ret;

	if (!pdata->clk[index])
		return -EINVAL;

	/* If devfreq is on, use that clock rate, otherwise default */
	devfreq_rate = pdata->clocks[index].devfreq_rate;
	default_rate = devfreq_rate ?
		devfreq_rate : pdata->clocks[index].default_rate;
	default_rate = clk_round_rate(pdata->clk[index], default_rate);

	list_for_each_entry(m, &pdata->client_list, node) {
		unsigned long r = m->rate[index];
		if (!r)
			r = default_rate;
		rate = max(r, rate);
	}
	if (!rate)
		rate = default_rate;

	trace_nvhost_module_update_rate(dev->name,
			pdata->clocks[index].name, rate);

	ret = clk_set_rate(pdata->clk[index], rate);

	if (pdata->update_clk)
		pdata->update_clk(dev);

	return ret;

}

int nvhost_module_set_rate(struct platform_device *dev, void *priv,
		unsigned long rate, int index, int bBW)
{
	struct nvhost_module_client *m;
	int ret = 0;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	mutex_lock(&client_list_lock);
	list_for_each_entry(m, &pdata->client_list, node) {
		if (m->priv == priv) {
			if (bBW) {
				/*
				 * If client sets BW, then we need to
				 * convert it to freq.
				 * rate is Bps and input param of
				 * tegra_emc_bw_to_freq_req is KBps.
				 */
				unsigned int freq_khz =
				tegra_emc_bw_to_freq_req
					((unsigned long)(rate >> 10));

				m->rate[index] =
					clk_round_rate(pdata->clk[index],
					(unsigned long)(freq_khz << 10));
			} else
				m->rate[index] =
					clk_round_rate(pdata->clk[index], rate);
		}
	}

	ret = nvhost_module_update_rate(dev, index);
	mutex_unlock(&client_list_lock);
	return ret;

}

int nvhost_module_add_client(struct platform_device *dev, void *priv)
{
	struct nvhost_module_client *client;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	INIT_LIST_HEAD(&client->node);
	client->priv = priv;

	mutex_lock(&client_list_lock);
	list_add_tail(&client->node, &pdata->client_list);
	mutex_unlock(&client_list_lock);

	return 0;
}

void nvhost_module_remove_client(struct platform_device *dev, void *priv)
{
	int i;
	struct nvhost_module_client *m;
	int found = 0;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	mutex_lock(&client_list_lock);
	list_for_each_entry(m, &pdata->client_list, node) {
		if (priv == m->priv) {
			list_del(&m->node);
			found = 1;
			break;
		}
	}
	if (found) {
		kfree(m);
		for (i = 0; i < pdata->num_clks; i++)
			nvhost_module_update_rate(dev, i);
	}
	mutex_unlock(&client_list_lock);
}

static ssize_t refcount_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int ret;
	struct nvhost_device_power_attr *power_attribute =
		container_of(attr, struct nvhost_device_power_attr, \
			power_attr[NVHOST_POWER_SYSFS_ATTRIB_REFCOUNT]);
	struct platform_device *dev = power_attribute->ndev;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	mutex_lock(&pdata->lock);
	ret = sprintf(buf, "%d\n", pdata->refcount);
	mutex_unlock(&pdata->lock);

	return ret;
}

static ssize_t powergate_delay_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int powergate_delay = 0, ret = 0;
	struct nvhost_device_power_attr *power_attribute =
		container_of(attr, struct nvhost_device_power_attr, \
			power_attr[NVHOST_POWER_SYSFS_ATTRIB_POWERGATE_DELAY]);
	struct platform_device *dev = power_attribute->ndev;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	if (!pdata->can_powergate) {
		dev_info(&dev->dev, "does not support power-gating\n");
		return count;
	}

	mutex_lock(&pdata->lock);
	ret = sscanf(buf, "%d", &powergate_delay);
	if (ret == 1 && powergate_delay >= 0)
		pdata->powergate_delay = powergate_delay;
	else
		dev_err(&dev->dev, "Invalid powergate delay\n");
	mutex_unlock(&pdata->lock);

	return count;
}

static ssize_t powergate_delay_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int ret;
	struct nvhost_device_power_attr *power_attribute =
		container_of(attr, struct nvhost_device_power_attr, \
			power_attr[NVHOST_POWER_SYSFS_ATTRIB_POWERGATE_DELAY]);
	struct platform_device *dev = power_attribute->ndev;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	mutex_lock(&pdata->lock);
	ret = sprintf(buf, "%d\n", pdata->powergate_delay);
	mutex_unlock(&pdata->lock);

	return ret;
}

static ssize_t clockgate_delay_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int clockgate_delay = 0, ret = 0;
	struct nvhost_device_power_attr *power_attribute =
		container_of(attr, struct nvhost_device_power_attr, \
			power_attr[NVHOST_POWER_SYSFS_ATTRIB_CLOCKGATE_DELAY]);
	struct platform_device *dev = power_attribute->ndev;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	mutex_lock(&pdata->lock);
	ret = sscanf(buf, "%d", &clockgate_delay);
	if (ret == 1 && clockgate_delay >= 0)
		pdata->clockgate_delay = clockgate_delay;
	else
		dev_err(&dev->dev, "Invalid clockgate delay\n");
	mutex_unlock(&pdata->lock);

	return count;
}

static ssize_t clockgate_delay_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int ret;
	struct nvhost_device_power_attr *power_attribute =
		container_of(attr, struct nvhost_device_power_attr, \
			power_attr[NVHOST_POWER_SYSFS_ATTRIB_CLOCKGATE_DELAY]);
	struct platform_device *dev = power_attribute->ndev;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	mutex_lock(&pdata->lock);
	ret = sprintf(buf, "%d\n", pdata->clockgate_delay);
	mutex_unlock(&pdata->lock);

	return ret;
}

int nvhost_module_set_devfreq_rate(struct platform_device *dev, int index,
		unsigned long rate)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	rate = clk_round_rate(pdata->clk[index], rate);
	pdata->clocks[index].devfreq_rate = rate;

	trace_nvhost_module_set_devfreq_rate(dev->name,
			pdata->clocks[index].name, rate);

	return nvhost_module_update_rate(dev, index);
}

int nvhost_module_init(struct platform_device *dev)
{
	int i = 0, err = 0;
	struct kobj_attribute *attr = NULL;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	/* initialize clocks to known state */
	INIT_LIST_HEAD(&pdata->client_list);
	while (pdata->clocks[i].name && i < NVHOST_MODULE_MAX_CLOCKS) {
		char devname[MAX_DEVID_LENGTH];
		long rate = pdata->clocks[i].default_rate;
		struct clk *c;

		snprintf(devname, MAX_DEVID_LENGTH, "tegra_%s",
			dev_name(&dev->dev));
		c = clk_get_sys(devname, pdata->clocks[i].name);
		if (IS_ERR_OR_NULL(c)) {
			dev_err(&dev->dev, "Cannot get clock %s\n",
					pdata->clocks[i].name);
			continue;
		}

		rate = clk_round_rate(c, rate);
		clk_prepare_enable(c);
		clk_set_rate(c, rate);
		clk_disable_unprepare(c);
		pdata->clk[i] = c;
		i++;
	}
	pdata->num_clks = i;

	mutex_init(&pdata->lock);
	init_waitqueue_head(&pdata->idle_wq);
	INIT_DELAYED_WORK(&pdata->powerstate_down, powerstate_down_handler);

	/* reset the module */
	do_module_reset_locked(dev);

	/* power gate units that we can power gate */
	if (pdata->can_powergate) {
		do_powergate_locked(pdata->powergate_ids[0]);
		do_powergate_locked(pdata->powergate_ids[1]);
		pdata->powerstate = NVHOST_POWER_STATE_POWERGATED;
	} else {
		do_unpowergate_locked(pdata->powergate_ids[0]);
		do_unpowergate_locked(pdata->powergate_ids[1]);
		pdata->powerstate = NVHOST_POWER_STATE_CLOCKGATED;
	}

	/* Init the power sysfs attributes for this device */
	pdata->power_attrib = kzalloc(sizeof(struct nvhost_device_power_attr),
		GFP_KERNEL);
	if (!pdata->power_attrib) {
		dev_err(&dev->dev, "Unable to allocate sysfs attributes\n");
		return -ENOMEM;
	}
	pdata->power_attrib->ndev = dev;

	pdata->power_kobj = kobject_create_and_add("acm", &dev->dev.kobj);
	if (!pdata->power_kobj) {
		dev_err(&dev->dev, "Could not add dir 'power'\n");
		err = -EIO;
		goto fail_attrib_alloc;
	}

	attr = &pdata->power_attrib->power_attr[NVHOST_POWER_SYSFS_ATTRIB_CLOCKGATE_DELAY];
	attr->attr.name = "clockgate_delay";
	attr->attr.mode = S_IWUSR | S_IRUGO;
	attr->show = clockgate_delay_show;
	attr->store = clockgate_delay_store;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	sysfs_attr_init(&attr->attr);
#endif
	if (sysfs_create_file(pdata->power_kobj, &attr->attr)) {
		dev_err(&dev->dev, "Could not create sysfs attribute clockgate_delay\n");
		err = -EIO;
		goto fail_clockdelay;
	}

	attr = &pdata->power_attrib->power_attr[NVHOST_POWER_SYSFS_ATTRIB_POWERGATE_DELAY];
	attr->attr.name = "powergate_delay";
	attr->attr.mode = S_IWUSR | S_IRUGO;
	attr->show = powergate_delay_show;
	attr->store = powergate_delay_store;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	sysfs_attr_init(&attr->attr);
#endif
	if (sysfs_create_file(pdata->power_kobj, &attr->attr)) {
		dev_err(&dev->dev, "Could not create sysfs attribute powergate_delay\n");
		err = -EIO;
		goto fail_powergatedelay;
	}

	attr = &pdata->power_attrib->power_attr[NVHOST_POWER_SYSFS_ATTRIB_REFCOUNT];
	attr->attr.name = "refcount";
	attr->attr.mode = S_IRUGO;
	attr->show = refcount_show;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	sysfs_attr_init(&attr->attr);
#endif
	if (sysfs_create_file(pdata->power_kobj, &attr->attr)) {
		dev_err(&dev->dev, "Could not create sysfs attribute refcount\n");
		err = -EIO;
		goto fail_refcount;
	}

	return 0;

fail_refcount:
	attr = &pdata->power_attrib->power_attr[NVHOST_POWER_SYSFS_ATTRIB_POWERGATE_DELAY];
	sysfs_remove_file(pdata->power_kobj, &attr->attr);

fail_powergatedelay:
	attr = &pdata->power_attrib->power_attr[NVHOST_POWER_SYSFS_ATTRIB_CLOCKGATE_DELAY];
	sysfs_remove_file(pdata->power_kobj, &attr->attr);

fail_clockdelay:
	kobject_put(pdata->power_kobj);

fail_attrib_alloc:
	kfree(pdata->power_attrib);

	return err;
}

static int is_module_idle(struct platform_device *dev)
{
	int count;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	mutex_lock(&pdata->lock);
	count = pdata->refcount;
	mutex_unlock(&pdata->lock);

	return (count == 0);
}

int nvhost_module_suspend(struct platform_device *dev)
{
	int ret;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	ret = wait_event_timeout(pdata->idle_wq, is_module_idle(dev),
			ACM_SUSPEND_WAIT_FOR_IDLE_TIMEOUT);
	if (ret == 0) {
		dev_info(&dev->dev, "%s prevented suspend\n",
				dev_name(&dev->dev));
		return -EBUSY;
	}

	mutex_lock(&pdata->lock);
	cancel_delayed_work(&pdata->powerstate_down);
	to_state_powergated_locked(dev);
	mutex_unlock(&pdata->lock);

	if (pdata->suspend_ndev)
		pdata->suspend_ndev(dev);

	return 0;
}

void nvhost_module_deinit(struct platform_device *dev)
{
	int i;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	nvhost_module_suspend(dev);
	for (i = 0; i < pdata->num_clks; i++)
		clk_put(pdata->clk[i]);
	pdata->powerstate = NVHOST_POWER_STATE_DEINIT;
}

/* public host1x power management APIs */
bool nvhost_module_powered_ext(struct platform_device *dev)
{
	struct platform_device *pdev;

	BUG_ON(!nvhost_get_parent(dev));

	/* get the parent */
	pdev = to_platform_device(dev->dev.parent);

	return nvhost_module_powered(pdev);
}

void nvhost_module_busy_ext(struct platform_device *dev)
{
	struct platform_device *pdev;

	BUG_ON(!nvhost_get_parent(dev));

	/* get the parent */
	pdev = to_platform_device(dev->dev.parent);

	nvhost_module_busy(pdev);
}

void nvhost_module_idle_ext(struct platform_device *dev)
{
	struct platform_device *pdev;

	BUG_ON(!nvhost_get_parent(dev));

	/* get the parent */
	pdev = to_platform_device(dev->dev.parent);

	nvhost_module_idle(pdev);
}
