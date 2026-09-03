// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/devfreq-event.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include "../governor.h"

#include "sprd_dvfs_apsys.h"
#include "sprd_dvfs_gsp.h"

static int GSP_DVFS_ENABLE = 1;

LIST_HEAD(gsp_dvfs_head);
ATOMIC_NOTIFIER_HEAD(gsp_dvfs_chain);

int gsp_dvfs_notifier_call_chain(void *data)
{
	return atomic_notifier_call_chain(&gsp_dvfs_chain, 0, data);
}
EXPORT_SYMBOL_GPL(gsp_dvfs_notifier_call_chain);

static ssize_t gsp_dvfs_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct gsp_dvfs *gsp = dev_get_drvdata(devfreq->dev.parent);
	int ret;

	ret = sprintf(buf, "%d\n", gsp->dvfs_enable);

	return ret;
}

static ssize_t gsp_dvfs_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct gsp_dvfs *gsp = dev_get_drvdata(devfreq->dev.parent);
	int ret, user_en;

	if (!get_display_power_status()) {
		pr_err("gsp is stopped, reject get gsp dvfs status\n");
		return -EINVAL;
	}

	ret = sscanf(buf, "%d\n", &user_en);
	if (ret == 0)
		return -EINVAL;

	gsp->dvfs_enable = user_en;

	if (gsp->dvfs_ops && gsp->dvfs_ops->hw_dfs_en) {
		gsp->dvfs_ops->hw_dfs_en(gsp, user_en);
		gsp->dvfs_coffe.hw_dfs_en = user_en;
	} else
		pr_info("%s: ip ops null\n", __func__);

	return count;
}

static ssize_t get_hw_dfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct gsp_dvfs *gsp = dev_get_drvdata(devfreq->dev.parent);
	int ret;

	ret = sprintf(buf, "%d\n", gsp->dvfs_coffe.hw_dfs_en);

	return ret;
}

static ssize_t set_hw_dfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct gsp_dvfs *gsp = dev_get_drvdata(devfreq->dev.parent);
	u32 dfs_en;
	int ret;

	if (!get_display_power_status()) {
		pr_err("gsp is stopped, reject get gsp dvfs status\n");
		return -EINVAL;
	}

	ret = sscanf(buf, "%d\n", &dfs_en);
	if (ret == 0)
		return -EINVAL;

	if (gsp->dvfs_ops && gsp->dvfs_ops->hw_dfs_en) {
		gsp->dvfs_ops->hw_dfs_en(gsp, dfs_en);
		gsp->dvfs_coffe.hw_dfs_en = dfs_en;
	} else
		pr_info("%s: ip ops null\n", __func__);

	return count;
}

static ssize_t get_work_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct gsp_dvfs *gsp = dev_get_drvdata(devfreq->dev.parent);
	u32 work_freq;
	int ret;

	if (!get_display_power_status()) {
		pr_err("gsp is stopped, reject get gsp dvfs status\n");
		return -EINVAL;
	}

	if (gsp->dvfs_ops && gsp->dvfs_ops->get_work_freq) {
		work_freq = gsp->dvfs_ops->get_work_freq(gsp);
		ret = sprintf(buf, "%d\n", work_freq);
	} else
		ret = sprintf(buf, "undefined\n");

	return ret;
}

static ssize_t set_work_freq_store(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct gsp_dvfs *gsp = dev_get_drvdata(devfreq->dev.parent);
	u32 user_freq;
	int ret;

	if (!get_display_power_status()) {
		pr_err("gsp is stopped, reject get gsp dvfs status\n");
		return -EINVAL;
	}

	mutex_lock(&devfreq->lock);

	ret = sscanf(buf, "%d\n", &user_freq);
	if (ret == 0) {
		mutex_unlock(&devfreq->lock);
		return -EINVAL;
	}

	gsp->work_freq = user_freq;
	gsp->freq_type = DVFS_WORK;
	ret = update_devfreq(devfreq);
	if (ret == 0)
		ret = count;

	mutex_unlock(&devfreq->lock);

	return ret;
}

static ssize_t get_idle_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct gsp_dvfs *gsp = dev_get_drvdata(devfreq->dev.parent);
	u32 idle_freq;
	int ret;

	if (!get_display_power_status()) {
		pr_err("gsp is stopped, reject get gsp dvfs status\n");
		return -EINVAL;
	}

	if (gsp->dvfs_ops && gsp->dvfs_ops->get_idle_freq) {
		idle_freq = gsp->dvfs_ops->get_idle_freq(gsp);
		ret = sprintf(buf, "%d\n", idle_freq);
	} else
		ret = sprintf(buf, "undefined\n");

	return ret;
}

static ssize_t set_idle_freq_store(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct gsp_dvfs *gsp = dev_get_drvdata(devfreq->dev.parent);
	u32 user_freq;
	int ret;

	if (!get_display_power_status()) {
		pr_err("gsp is stopped, reject get gsp dvfs status\n");
		return -EINVAL;
	}

	mutex_lock(&devfreq->lock);

	ret = sscanf(buf, "%d\n", &user_freq);
	if (ret == 0) {
		mutex_unlock(&devfreq->lock);
		return -EINVAL;
	}

	gsp->idle_freq = user_freq;
	gsp->freq_type = DVFS_IDLE;
	ret = update_devfreq(devfreq);
	if (ret == 0)
		ret = count;

	mutex_unlock(&devfreq->lock);

	return ret;
}

static ssize_t get_work_index_show(struct device *dev,
		struct device_attribute *attr,  char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct gsp_dvfs *gsp = dev_get_drvdata(devfreq->dev.parent);
	int work_index, ret;

	if (!get_display_power_status()) {
		pr_err("gsp is stopped, reject get gsp dvfs status\n");
		return -EINVAL;
	}

	if (gsp->dvfs_ops && gsp->dvfs_ops->get_work_index) {
		work_index = gsp->dvfs_ops->get_work_index(gsp);
		ret = sprintf(buf, "%d\n", work_index);
	} else
		ret = sprintf(buf, "undefined\n");

	return ret;
}

static ssize_t set_work_index_store(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct gsp_dvfs *gsp = dev_get_drvdata(devfreq->dev.parent);
	int work_index, ret;

	if (!get_display_power_status()) {
		pr_err("gsp is stopped, reject get gsp dvfs status\n");
		return -EINVAL;
	}

	ret = sscanf(buf, "%d\n", &work_index);
	if (ret == 0)
		return -EINVAL;

	if (gsp->dvfs_ops && gsp->dvfs_ops->set_work_index)
		gsp->dvfs_ops->set_work_index(gsp, work_index);
	else
		pr_info("%s: ip ops null\n", __func__);

	return count;
}

static ssize_t get_idle_index_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct gsp_dvfs *gsp = dev_get_drvdata(devfreq->dev.parent);
	int idle_index, ret;

	if (!get_display_power_status()) {
		pr_err("gsp is stopped, reject get gsp dvfs status\n");
		return -EINVAL;
	}

	if (gsp->dvfs_ops && gsp->dvfs_ops->get_idle_index) {
		idle_index = gsp->dvfs_ops->get_idle_index(gsp);
		ret = sprintf(buf, "%d\n", idle_index);
	} else
		ret = sprintf(buf, "undefined\n");

	return ret;
}

static ssize_t set_idle_index_store(struct device *dev,
		struct device_attribute *attr, const char *buf,
		size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct gsp_dvfs *gsp = dev_get_drvdata(devfreq->dev.parent);
	int idle_index, ret;

	if (!get_display_power_status()) {
		pr_err("gsp is stopped, reject get gsp dvfs status\n");
		return -EINVAL;
	}

	ret = sscanf(buf, "%d\n", &idle_index);
	if (ret == 0)
		return -EINVAL;

	if (gsp->dvfs_ops && gsp->dvfs_ops->set_idle_index)
		gsp->dvfs_ops->set_idle_index(gsp, idle_index);
	else
		pr_info("%s: ip ops null\n", __func__);

	return count;
}

static ssize_t get_dvfs_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct gsp_dvfs *gsp = dev_get_drvdata(devfreq->dev.parent);
	struct ip_dvfs_status dvfs_status;
	ssize_t len = 0;

	if (!get_display_power_status()) {
		pr_err("gsp is stopped, reject get gsp dvfs status\n");
		return -EINVAL;
	}

	if (gsp->dvfs_ops && gsp->dvfs_ops->get_dvfs_status)
		gsp->dvfs_ops->get_dvfs_status(gsp, &dvfs_status);
	else {
		len = sprintf(buf, "undefined\n");
		return len;
	}

	len = sprintf(buf, "apsys_cur_volt\tvsp_vote_volt\t"
			"gsp_vote_volt\tvdsp_vote_volt\n");

	len += sprintf(buf + len, "%s\t\t%s\t\t%s\t\t%s\n",
			dvfs_status.apsys_cur_volt, dvfs_status.vsp_vote_volt,
			dvfs_status.gsp0_vote_volt, dvfs_status.vdsp_vote_volt);

	len += sprintf(buf + len, "\t\tvsp_cur_freq\tgsp_cur_freq\t"
			"vdsp_cur_freq\n");

	len += sprintf(buf + len, "\t\t%s\t\t%s\t\t%s\n",
			dvfs_status.vsp_cur_freq, dvfs_status.gsp0_cur_freq,
			dvfs_status.vdsp_cur_freq);

	return len;
}

static ssize_t get_dvfs_table_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct gsp_dvfs *gsp = dev_get_drvdata(devfreq->dev.parent);
	struct ip_dvfs_map_cfg dvfs_table[8];
	ssize_t len = 0;
	int i;
	int table_num = 0;

	if (gsp->dvfs_ops && gsp->dvfs_ops->get_dvfs_table)
		table_num = gsp->dvfs_ops->get_dvfs_table(dvfs_table);
	else
		pr_info("%s: ip ops null\n", __func__);

	len = sprintf(buf, "map_index\tvolt_level\tclk_level\tclk_rate\n");
	for (i = 0; i < table_num; i++) {
		len += sprintf(buf+len, "%d\t\t%d\t\t%d\t\t%d\t\t\n",
				dvfs_table[i].map_index,
				dvfs_table[i].volt_level,
				dvfs_table[i].clk_level,
				dvfs_table[i].clk_rate);
	}

	return len;
}

static DEVICE_ATTR(dvfs_enable, 0644, gsp_dvfs_enable_show,
				   gsp_dvfs_enable_store);
static DEVICE_ATTR(hw_dfs_en, 0644, get_hw_dfs_show,
				   set_hw_dfs_store);
static DEVICE_ATTR(work_freq, 0644, get_work_freq_show,
				   set_work_freq_store);
static DEVICE_ATTR(idle_freq, 0644, get_idle_freq_show,
				   set_idle_freq_store);
static DEVICE_ATTR(work_index, 0644, get_work_index_show,
				   set_work_index_store);
static DEVICE_ATTR(idle_index, 0644, get_idle_index_show,
				   set_idle_index_store);
static DEVICE_ATTR(dvfs_status, 0444, get_dvfs_status_show, NULL);
static DEVICE_ATTR(dvfs_table, 0444, get_dvfs_table_show, NULL);

static struct attribute *dev_entries[] = {
	&dev_attr_dvfs_enable.attr,
	&dev_attr_hw_dfs_en.attr,
	&dev_attr_work_freq.attr,
	&dev_attr_idle_freq.attr,
	&dev_attr_work_index.attr,
	&dev_attr_idle_index.attr,
	&dev_attr_dvfs_status.attr,
	&dev_attr_dvfs_table.attr,
	NULL,
};

static struct attribute_group dev_attr_group = {
	.name	= "gsp_governor",
	.attrs	= dev_entries,
};

static int gsp_dvfs_notify_callback(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct gsp_dvfs *gsp = container_of(nb, struct gsp_dvfs, gsp_dvfs_nb);
	u32 freq = *(int *)data;

	if (dpu_vsp_dvfs_check_clkeb()) {
		pr_info("%s(), dpu_vsp eb is not on\n", __func__);
		return NOTIFY_DONE;
	}

	if (!gsp->dvfs_enable)
		return NOTIFY_DONE;

	if (gsp->dvfs_ops && gsp->dvfs_ops->set_work_freq) {
		gsp->dvfs_ops->set_work_freq(gsp, freq);
		pr_debug("set work freq = %u\n", freq);
	}

	if (gsp->devfreq->profile->freq_table)
		devfreq_update_status(gsp->devfreq, freq);

	gsp->work_freq = freq;
	gsp->freq_type = DVFS_WORK;
	gsp->devfreq->previous_freq = freq;

	return NOTIFY_OK;
}

static int gsp_dvfs_target(struct device *dev, unsigned long *freq,
				u32 flags)
{
	struct gsp_dvfs *gsp = dev_get_drvdata(dev);
	struct dev_pm_opp *opp;
	u32 target_freq;

	pr_debug("devfreq_dev_profile-->target\n");

	opp = devfreq_recommended_opp(dev, freq, flags);
	if (IS_ERR(opp)) {
		dev_err(dev, "failed to find opp for %lu KHz\n", *freq);
		return PTR_ERR(opp);
	}
	target_freq = dev_pm_opp_get_freq(opp);
	dev_pm_opp_put(opp);

	if (gsp->freq_type == DVFS_WORK) {
		if (gsp->dvfs_ops && gsp->dvfs_ops->set_work_freq) {
			gsp->dvfs_ops->set_work_freq(gsp, target_freq);
			pr_debug("set work freq = %u\n", target_freq);
		}
	} else {
		if (gsp->dvfs_ops && gsp->dvfs_ops->set_idle_freq) {
			gsp->dvfs_ops->set_idle_freq(gsp, target_freq);
			pr_debug("set idle freq = %u\n", target_freq);
		}
	}

	*freq = target_freq;

	return 0;
}

static int gsp_dvfs_get_dev_status(struct device *dev,
					 struct devfreq_dev_status *stat)
{
	/*
	struct gsp_dvfs *gsp = dev_get_drvdata(dev);
	struct devfreq_event_data edata;
	int ret = 0;

	pr_debug("devfreq_dev_profile-->get_dev_status\n");

	ret = devfreq_event_get_event(gsp->edev, &edata);
	if (ret < 0)
		return ret;

	stat->current_frequency = gsp->work_freq;
	stat->busy_time = edata.load_count;
	stat->total_time = edata.total_count;

	return ret;
	*/
	return 0;
}

static int gsp_dvfs_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct gsp_dvfs *gsp = dev_get_drvdata(dev);

	pr_debug("devfreq_dev_profile-->get_cur_freq\n");

	if (gsp->freq_type == DVFS_WORK)
		*freq = gsp->work_freq;
	else
		*freq = gsp->idle_freq;

	return 0;
}

static struct devfreq_dev_profile gsp_dvfs_profile = {
	.polling_ms	= 0,
	.target             = gsp_dvfs_target,
	.get_dev_status     = gsp_dvfs_get_dev_status,
	.get_cur_freq       = gsp_dvfs_get_cur_freq,
};

static __maybe_unused int gsp_dvfs_suspend(struct device *dev)
{
	pr_info("%s()\n", __func__);

	return 0;
}

static __maybe_unused int gsp_dvfs_resume(struct device *dev)
{
	struct gsp_dvfs *gsp = dev_get_drvdata(dev);

	pr_info("%s()\n", __func__);

	if (gsp->dvfs_ops && gsp->dvfs_ops->dvfs_init)
		gsp->dvfs_ops->dvfs_init(gsp);

	return 0;
}

static SIMPLE_DEV_PM_OPS(gsp_dvfs_pm, gsp_dvfs_suspend,
			 gsp_dvfs_resume);

static int userspace_init(struct devfreq *devfreq)
{
	int ret = 0;

	ret = sysfs_create_group(&devfreq->dev.kobj, &dev_attr_group);

	return ret;
}

static void userspace_exit(struct devfreq *devfreq)
{
	/*
	 * Remove the sysfs entry, unless this is being called after
	 * device_del(), which should have done this already via kobject_del().
	 */
	if (devfreq->dev.kobj.sd)
		sysfs_remove_group(&devfreq->dev.kobj, &dev_attr_group);
}

static int gsp_gov_get_target(struct devfreq *devfreq,
				     unsigned long *freq)
{
	struct gsp_dvfs *gsp = dev_get_drvdata(devfreq->dev.parent);
	u32 adjusted_freq = 0;

	pr_debug("devfreq_governor-->get_target_freq\n");

	if (gsp->freq_type == DVFS_WORK)
		adjusted_freq = gsp->work_freq;
	else
		adjusted_freq = gsp->idle_freq;

	if (devfreq->scaling_max_freq && adjusted_freq > devfreq->scaling_max_freq)
		adjusted_freq = devfreq->scaling_max_freq;
	else if (devfreq->scaling_min_freq && adjusted_freq < devfreq->scaling_min_freq)
		adjusted_freq = devfreq->scaling_min_freq;

	*freq = adjusted_freq;

	return 0;
}

static int gsp_gov_event_handler(struct devfreq *devfreq,
					unsigned int event, void *data)
{
	int ret = 0;

	switch (event) {
	case DEVFREQ_GOV_START:
		ret = userspace_init(devfreq);
		break;
	case DEVFREQ_GOV_STOP:
		userspace_exit(devfreq);
		break;
	default:
		break;
	}

	return ret;
}

struct devfreq_governor gsp_devfreq_gov = {
	.name = "gsp_dvfs",
	.get_target_freq = gsp_gov_get_target,
	.event_handler = gsp_gov_event_handler,
};

static int gsp_dvfs_parse_dt(struct gsp_dvfs *gsp,
			      struct device_node *np)
{
	int ret;

	pr_info("%s()\n", __func__);

	ret = of_property_read_u32(np, "sprd,hw-dfs-en",
			&gsp->dvfs_coffe.hw_dfs_en);
	ret |= of_property_read_u32(np, "sprd,work-index-def",
			&gsp->dvfs_coffe.work_index_def);
	ret |= of_property_read_u32(np, "sprd,idle-index-def",
			&gsp->dvfs_coffe.idle_index_def);

	return ret;
}

static int gsp_dvfs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	const struct sprd_gsp_dvfs_ops *pdata;
	struct apsys_dev *apsys;
	struct gsp_dvfs *gsp;
	int ret;

	gsp = devm_kzalloc(dev, sizeof(*gsp), GFP_KERNEL);
	if (!gsp)
		return -ENOMEM;

	apsys = find_apsys_device_by_name("apsys-dvfs");
	if (!apsys)
		return -ENOMEM;

	gsp->apsys = apsys;

	pdata = of_device_get_match_data(&pdev->dev);
	if (pdata) {
		gsp->dvfs_ops = pdata->dvfs_ops;
	} else {
		pr_err("No matching driver data found\n");
		return -EINVAL;
	}

	ret = gsp_dvfs_parse_dt(gsp, np);
	if (ret) {
		pr_err("parse gsp dvfs dt failed\n");
		return ret;
	}

	ret = dev_pm_opp_of_add_table(dev);
	if (ret) {
		dev_err(dev, "invalid operating-points in device tree.\n");
		return ret;
	}

	gsp->gsp_dvfs_nb.notifier_call = gsp_dvfs_notify_callback;
	ret = atomic_notifier_chain_register(&gsp_dvfs_chain, &gsp->gsp_dvfs_nb);
	if (ret) {
		dev_err(&pdev->dev,
			"failed to register gsp layer change notifier\n");
		goto err;
	}

	platform_set_drvdata(pdev, gsp);
	gsp->devfreq = devm_devfreq_add_device(dev,
						 &gsp_dvfs_profile,
						 "gsp_dvfs",
						 NULL);
	if (IS_ERR(gsp->devfreq)) {
		dev_err(dev,
			"failed to add devfreq dev with gsp-dvfs governor\n");
		ret = PTR_ERR(gsp->devfreq);
		goto err;
	}

	device_rename(&gsp->devfreq->dev, "gsp");

	gsp->dvfs_enable = GSP_DVFS_ENABLE;

	if (gsp->dvfs_ops && gsp->dvfs_ops->parse_dt)
		gsp->dvfs_ops->parse_dt(gsp, np);

	if (gsp->dvfs_ops && gsp->dvfs_ops->dvfs_init)
		gsp->dvfs_ops->dvfs_init(gsp);

	pr_info("gsp dvfs module registered\n");

	return 0;
err:
	dev_pm_opp_of_remove_table(dev);
	atomic_notifier_chain_unregister(&gsp_dvfs_chain, &gsp->gsp_dvfs_nb);

	return ret;
}

static int gsp_dvfs_remove(struct platform_device *pdev)
{
	pr_info("gsp_dvfs_remove\n");
	return 0;
}

static const struct sprd_gsp_dvfs_ops qogirn6pro_gsp_dvfs = {
	.dvfs_ops = &qogirn6pro_gsp_dvfs_ops,
};

static const struct sprd_gsp_dvfs_ops qogirn6lite_gsp_dvfs = {
	.dvfs_ops = &qogirn6lite_gsp_dvfs_ops,
};

static const struct of_device_id gsp_dvfs_of_match[] = {
	{ .compatible = "sprd,hwdvfs-gsp-qogirn6pro",
	  .data = &qogirn6pro_gsp_dvfs },
	{ .compatible = "sprd,hwdvfs-gsp-qogirn6lite",
	  .data = &qogirn6lite_gsp_dvfs },
	{ }
};

MODULE_DEVICE_TABLE(of, gsp_dvfs_of_match);

struct platform_driver gsp_dvfs_driver = {
	.probe	= gsp_dvfs_probe,
	.remove	= gsp_dvfs_remove,
	.driver = {
		.name = "gsp-dvfs",
		.pm	= &gsp_dvfs_pm,
		.of_match_table = gsp_dvfs_of_match,
	},
};

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("sprd gsp devfreq driver");
MODULE_AUTHOR("Kevin Tang <kevin.tang@unisoc.com>");
