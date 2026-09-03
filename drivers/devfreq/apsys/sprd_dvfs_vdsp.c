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

#include "sprd_dvfs_vdsp.h"

static int VDSP_DVFS_ENABLE = 1;

LIST_HEAD(vdsp_dvfs_head);
BLOCKING_NOTIFIER_HEAD(vdsp_dvfs_chain);


int vdsp_dvfs_notifier_call_chain(void *data)
{
	return blocking_notifier_call_chain(&vdsp_dvfs_chain, 0, data);
}
EXPORT_SYMBOL_GPL(vdsp_dvfs_notifier_call_chain);

static ssize_t vdsp_dvfs_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vdsp_dvfs *vdsp = dev_get_drvdata(devfreq->dev.parent);
	int ret;

	ret = sprintf(buf, "%d\n", vdsp->dvfs_enable);

	return ret;
}

static ssize_t vdsp_dvfs_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vdsp_dvfs *vdsp = dev_get_drvdata(devfreq->dev.parent);
	int ret, user_en;

	ret = sscanf(buf, "%d\n", &user_en);
	if (ret == 0)
		return -EINVAL;

	/* disable vdsp dvfs */
	vdsp->dvfs_enable = user_en;

	return count;
}

static ssize_t get_hw_dfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vdsp_dvfs *vdsp = dev_get_drvdata(devfreq->dev.parent);
	int ret;

	ret = sprintf(buf, "%d\n", vdsp->dvfs_coffe.hw_dfs_en);

	return ret;
}

static ssize_t set_hw_dfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vdsp_dvfs *vdsp = dev_get_drvdata(devfreq->dev.parent);
	u32 dfs_en;
	int ret;

	ret = sscanf(buf, "%d\n", &dfs_en);
	if (ret == 0)
		return -EINVAL;

	if (vdsp->dvfs_ops && vdsp->dvfs_ops->hw_dfs_en) {
		vdsp->dvfs_ops->hw_dfs_en(vdsp, dfs_en);
		vdsp->dvfs_coffe.hw_dfs_en = dfs_en;
	} else
		pr_info("%s: ip ops null\n", __func__);

	return count;
}

static ssize_t get_work_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vdsp_dvfs *vdsp = dev_get_drvdata(devfreq->dev.parent);
	u32 work_freq;
	int ret;

	if (vdsp->dvfs_ops && vdsp->dvfs_ops->get_work_freq) {
		work_freq = vdsp->dvfs_ops->get_work_freq(vdsp);
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
	struct vdsp_dvfs *vdsp = dev_get_drvdata(devfreq->dev.parent);
	u32 user_freq;
	int ret;

	mutex_lock(&devfreq->lock);

	ret = sscanf(buf, "%d\n", &user_freq);
	if (ret == 0) {
		mutex_unlock(&devfreq->lock);
		return -EINVAL;
	}

	vdsp->work_freq = user_freq;
	vdsp->freq_type = DVFS_WORK;
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
	struct vdsp_dvfs *vdsp = dev_get_drvdata(devfreq->dev.parent);
	u32 idle_freq;
	int ret;

	if (vdsp->dvfs_ops && vdsp->dvfs_ops->get_idle_freq) {
		idle_freq = vdsp->dvfs_ops->get_idle_freq(vdsp);
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
	struct vdsp_dvfs *vdsp = dev_get_drvdata(devfreq->dev.parent);
	u32 user_freq;
	int ret;

	mutex_lock(&devfreq->lock);

	ret = sscanf(buf, "%d\n", &user_freq);
	if (ret == 0) {
		mutex_unlock(&devfreq->lock);
		return -EINVAL;
	}

	vdsp->idle_freq = user_freq;
	vdsp->freq_type = DVFS_IDLE;
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
	struct vdsp_dvfs *vdsp = dev_get_drvdata(devfreq->dev.parent);
	int work_index, ret;

	if (vdsp->dvfs_ops && vdsp->dvfs_ops->get_work_index) {
		work_index = vdsp->dvfs_ops->get_work_index(vdsp);
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
	struct vdsp_dvfs *vdsp = dev_get_drvdata(devfreq->dev.parent);
	int work_index, ret;

	ret = sscanf(buf, "%d\n", &work_index);
	if (ret == 0)
		return -EINVAL;

	if (vdsp->dvfs_ops && vdsp->dvfs_ops->set_work_index) {
		ret = vdsp->dvfs_ops->set_work_index(vdsp, work_index);
		if (ret) {
			pr_err("%s , set_work_index err ret:%d , index:%d\n",
				__func__, ret, work_index);
			return -EFAULT;
		}
	}
	else
		pr_info("%s: ip ops null\n", __func__);

	return count;
}

static ssize_t get_idle_index_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vdsp_dvfs *vdsp = dev_get_drvdata(devfreq->dev.parent);
	int idle_index, ret;

	if (vdsp->dvfs_ops && vdsp->dvfs_ops->get_idle_index) {
		idle_index = vdsp->dvfs_ops->get_idle_index(vdsp);
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
	struct vdsp_dvfs *vdsp = dev_get_drvdata(devfreq->dev.parent);
	int idle_index, ret;

	ret = sscanf(buf, "%d\n", &idle_index);
	if (ret == 0)
		return -EINVAL;

	if (vdsp->dvfs_ops && vdsp->dvfs_ops->set_idle_index)
		vdsp->dvfs_ops->set_idle_index(vdsp, idle_index);
	else
		pr_info("%s: ip ops null\n", __func__);

	return count;
}

static ssize_t get_dvfs_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vdsp_dvfs *vdsp = dev_get_drvdata(devfreq->dev.parent);
	struct ip_dvfs_status dvfs_status;
	ssize_t len = 0;

	if (vdsp->dvfs_ops && vdsp->dvfs_ops->get_dvfs_status)
		vdsp->dvfs_ops->get_dvfs_status(vdsp, &dvfs_status);
	else {
		len = sprintf(buf, "undefined\n");
		return len;
	}

	len = sprintf(buf, "apsys_cur_volt\tvsp_vote_volt\t"
			"dpu_vote_volt\tvdsp_vote_volt\n");

	len += sprintf(buf + len, "%s\t\t%s\t\t%s\t\t%s\n",
			dvfs_status.apsys_cur_volt, dvfs_status.vsp_vote_volt,
			dvfs_status.dpu_vote_volt, dvfs_status.vdsp_vote_volt);

	len += sprintf(buf + len, "\t\tvsp_cur_freq\tdpu_cur_freq\t"
			"vdsp_cur_freq\n");

	len += sprintf(buf + len, "\t\t%s\t\t%s\t\t%s\n",
			dvfs_status.vsp_cur_freq, dvfs_status.dpu_cur_freq,
			dvfs_status.vdsp_cur_freq);

	return len;
}

static ssize_t get_dvfs_table_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vdsp_dvfs *vdsp = dev_get_drvdata(devfreq->dev.parent);
	struct ip_dvfs_map_cfg dvfs_table[8];
	ssize_t len = 0;
	int i;
	int table_num = 0;

	if (vdsp->dvfs_ops && vdsp->dvfs_ops->get_dvfs_table)
		table_num = vdsp->dvfs_ops->get_dvfs_table(dvfs_table);
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

static DEVICE_ATTR(dvfs_enable, 0644, vdsp_dvfs_enable_show,
				   vdsp_dvfs_enable_store);
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
	.name	= "vdsp_governor",
	.attrs	= dev_entries,
};

static int vdsp_dvfs_notify_callback(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct vdsp_dvfs *vdsp = container_of(nb, struct vdsp_dvfs, vdsp_dvfs_nb);
	u32 dvfs_freq = *(int *)data;
	int ret;
	mutex_lock(&vdsp->devfreq->lock);

	pr_info("%s enter", __func__);

	if (!vdsp->dvfs_enable) {
		pr_err("vdsp dvfs is disabled, nothing to do\n");
		mutex_unlock(&vdsp->devfreq->lock);
		return NOTIFY_DONE;
	}


	vdsp->work_freq = dvfs_freq;
	vdsp->freq_type = DVFS_WORK;
	ret = update_devfreq(vdsp->devfreq);
	if (ret < 0)
		pr_err("update_devfreq is failed");
	pr_info("%s exit\n", __func__);
	mutex_unlock(&vdsp->devfreq->lock);

	return NOTIFY_OK;
}

static int vdsp_dvfs_target(struct device *dev, unsigned long *freq,
				u32 flags)
{
	struct vdsp_dvfs *vdsp = dev_get_drvdata(dev);
	struct dev_pm_opp *opp;
	u32 target_freq;
	int ret = 0;

	pr_info("devfreq_dev_profile-->target\n");

	opp = devfreq_recommended_opp(dev, freq, flags);
	if (IS_ERR(opp)) {
		dev_err(dev, "failed to find opp for %lu KHz\n", *freq);
		return PTR_ERR(opp);
	}
	target_freq = dev_pm_opp_get_freq(opp);
	dev_pm_opp_put(opp);

	if (vdsp->freq_type == DVFS_WORK) {
		if (vdsp->dvfs_ops && vdsp->dvfs_ops->set_work_freq) {
			vdsp->work_freq = target_freq;
			ret = vdsp->dvfs_ops->set_work_freq(vdsp, target_freq);
			pr_info("set work freq = %u, ret:%d\n", target_freq, ret);
		}
	} else {
		if (vdsp->dvfs_ops && vdsp->dvfs_ops->set_idle_freq) {
			vdsp->idle_freq = target_freq;
			vdsp->dvfs_ops->set_idle_freq(vdsp, target_freq);
			pr_info("set idle freq = %u\n", target_freq);
		}
	}
	if (ret == 0)
		*freq = target_freq;

	return ret;
}

static int vdsp_dvfs_get_dev_status(struct device *dev,
					 struct devfreq_dev_status *stat)
{
	return 0;
}

static int vdsp_dvfs_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct vdsp_dvfs *vdsp = dev_get_drvdata(dev);

	pr_info("devfreq_dev_profile-->get_cur_freq\n");

	if (vdsp->freq_type == DVFS_WORK)
		*freq = vdsp->work_freq;
	else
		*freq = vdsp->idle_freq;

	return 0;
}

static struct devfreq_dev_profile vdsp_dvfs_profile = {
	.polling_ms	= 0,
	.target             = vdsp_dvfs_target,
	.get_dev_status     = vdsp_dvfs_get_dev_status,
	.get_cur_freq       = vdsp_dvfs_get_cur_freq,
};

static __maybe_unused int vdsp_dvfs_suspend(struct device *dev)
{
	pr_info("%s()\n", __func__);

	return 0;
}

static __maybe_unused int vdsp_dvfs_resume(struct device *dev)
{
	struct vdsp_dvfs *vdsp = dev_get_drvdata(dev);

	pr_info("%s()\n", __func__);

	if (vdsp->dvfs_ops && vdsp->dvfs_ops->dvfs_init)
		vdsp->dvfs_ops->dvfs_init(vdsp);

	return 0;
}

static SIMPLE_DEV_PM_OPS(vdsp_dvfs_pm, vdsp_dvfs_suspend,
			 vdsp_dvfs_resume);

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

static int vdsp_gov_get_target(struct devfreq *devfreq,
				     unsigned long *freq)
{
	struct vdsp_dvfs *vdsp = dev_get_drvdata(devfreq->dev.parent);
	u32 adjusted_freq = 0;

	pr_info("get_target_freq\n");

	if (vdsp->freq_type == DVFS_WORK)
		adjusted_freq = vdsp->work_freq;
	else
		adjusted_freq = vdsp->idle_freq;

	if (devfreq->scaling_max_freq && adjusted_freq > devfreq->scaling_max_freq)
		adjusted_freq = devfreq->scaling_max_freq;
	else if (devfreq->scaling_min_freq && adjusted_freq < devfreq->scaling_min_freq)
		adjusted_freq = devfreq->scaling_min_freq;

	*freq = adjusted_freq;

	return 0;
}

static int vdsp_gov_event_handler(struct devfreq *devfreq,
					unsigned int event, void *data)
{
	int ret = 0;

	pr_info("event_handler(%d)\n", event);
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

struct devfreq_governor vdsp_devfreq_gov = {
	.name = "vdsp_dvfs",
	.get_target_freq = vdsp_gov_get_target,
	.event_handler = vdsp_gov_event_handler,
};

static int vdsp_dvfs_parse_dt(struct vdsp_dvfs *vdsp,
			      struct device_node *np)
{
	int ret;

	pr_info("%s()\n", __func__);

	ret = of_property_read_u32(np, "sprd,hw-dfs-en",
			&vdsp->dvfs_coffe.hw_dfs_en);
	ret |= of_property_read_u32(np, "sprd,work-index-def",
			&vdsp->dvfs_coffe.work_index_def);
	ret |= of_property_read_u32(np, "sprd,idle-index-def",
			&vdsp->dvfs_coffe.idle_index_def);

	return ret;
}

static int vdsp_dvfs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	const struct sprd_vdsp_dvfs_ops *pdata;
	struct apsys_dev *apsys;
	struct vdsp_dvfs *vdsp;
	int ret;

	vdsp = devm_kzalloc(dev, sizeof(*vdsp), GFP_KERNEL);
	if (!vdsp)
		return -ENOMEM;

	apsys = find_apsys_device_by_name("apsys-dvfs");
	if (!apsys)
		return -ENOMEM;

	vdsp->apsys = apsys;

	pdata = of_device_get_match_data(&pdev->dev);
	if (pdata) {
		vdsp->dvfs_ops = pdata->dvfs_ops;
	} else {
		pr_err("No matching driver data found\n");
		return -EINVAL;
	}

	ret = vdsp_dvfs_parse_dt(vdsp, np);
	if (ret) {
		pr_err("parse vdsp dvfs dt failed\n");
		return ret;
	}

	ret = dev_pm_opp_of_add_table(dev);
	if (ret) {
		dev_err(dev, "invalid operating-points in device tree.\n");
		return ret;
	}

	vdsp->vdsp_dvfs_nb.notifier_call = vdsp_dvfs_notify_callback;
	ret = blocking_notifier_chain_register(&vdsp_dvfs_chain, &vdsp->vdsp_dvfs_nb);
	if (ret) {
		pr_err("failed to register vdsp layer change notifier\n");
		goto err;
	}

	platform_set_drvdata(pdev, vdsp);
	vdsp->devfreq = devm_devfreq_add_device(dev,
						 &vdsp_dvfs_profile,
						 "vdsp_dvfs",
						 NULL);
	if (IS_ERR(vdsp->devfreq)) {
		pr_err("failed to add devfreq dev with vdsp-dvfs governor\n");
		ret = PTR_ERR(vdsp->devfreq);
		goto err;
	}

	device_rename(&vdsp->devfreq->dev, "vdsp");

	vdsp->dvfs_enable = VDSP_DVFS_ENABLE;

	if (vdsp->dvfs_ops && vdsp->dvfs_ops->parse_dt)
		vdsp->dvfs_ops->parse_dt(vdsp, np);
	if (vdsp->dvfs_ops && vdsp->dvfs_ops->parse_pll)
		vdsp->dvfs_ops->parse_pll(vdsp, dev);
	if (vdsp->dvfs_ops && vdsp->dvfs_ops->dvfs_init)
		vdsp->dvfs_ops->dvfs_init(vdsp);

	pr_info("vdsp dvfs module registered\n");

	return 0;
err:
	dev_pm_opp_of_remove_table(dev);
	blocking_notifier_chain_unregister(&vdsp_dvfs_chain, &vdsp->vdsp_dvfs_nb);

	return ret;
}

static int vdsp_dvfs_remove(struct platform_device *pdev)
{
	pr_info("vdsp_dvfs_remove\n");
	return 0;
}

static const struct sprd_vdsp_dvfs_ops sharkl5pro_dvfs_ops = {
	.dvfs_ops = &sharkl5pro_vdsp_dvfs_ops,
};

static const struct of_device_id vdsp_dvfs_of_match[] = {
	{ .compatible = "sprd,hwdvfs-vdsp-sharkl5pro",
	  .data = &sharkl5pro_dvfs_ops },
	{ },
};

MODULE_DEVICE_TABLE(of, vdsp_dvfs_of_match);

struct platform_driver vdsp_dvfs_driver = {
	.probe	= vdsp_dvfs_probe,
	.remove	= vdsp_dvfs_remove,
	.driver = {
		.name = "vdsp-dvfs",
		.pm	= &vdsp_dvfs_pm,
		.of_match_table = vdsp_dvfs_of_match,
	},
};

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("sprd vdsp devfreq driver");
MODULE_AUTHOR("Sam Liu <sam.liu@unisoc.com>");
