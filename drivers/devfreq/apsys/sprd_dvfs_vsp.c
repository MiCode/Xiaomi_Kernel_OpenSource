// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include "sprd_dvfs_vsp.h"

LIST_HEAD(vsp_dvfs_head);

struct sprd_vsp_dvfs_data {
	const char *ver;
	u32 max_freq_level;
	const struct ip_dvfs_ops *dvfs_ops;
};

static const struct sprd_vsp_dvfs_data sharkl5_vsp_data = {
	.ver = "sharkl5",
	.max_freq_level = 3,
	.dvfs_ops = &sharkl5_vsp_dvfs_ops,
};

static const struct sprd_vsp_dvfs_data roc1_vsp_data = {
	.ver = "roc1",
	.max_freq_level = 4,
	.dvfs_ops = &roc1_vsp_dvfs_ops,
};

static const struct sprd_vsp_dvfs_data sharkl5pro_vsp_data = {
	.ver = "sharkl5pro",
	.max_freq_level = 3,
	.dvfs_ops = &sharkl5pro_vsp_dvfs_ops,
};

static const struct sprd_vsp_dvfs_data qogirl6_vsp_data = {
	.ver = "qogirl6",
	.max_freq_level = 3,
	.dvfs_ops = &qogirl6_vsp_dvfs_ops,
};

static const struct sprd_vsp_dvfs_data qogirn6pro_vsp_data = {
	.ver = "qogirn6pro",
	.max_freq_level = 5,
	.dvfs_ops = &qogirn6pro_vpudec_vsp_dvfs_ops,
};

static const struct sprd_vsp_dvfs_data qogirn6pro_vpuenc_data = {
	.ver = "vpuenc",
	.max_freq_level = 4,
	.dvfs_ops = &qogirn6pro_vpuenc_vsp_dvfs_ops,
};

static const struct sprd_vsp_dvfs_data qogirn6lite_vsp_data = {
	.ver = "qogirn6lite",
	.max_freq_level = 5,
	.dvfs_ops = &qogirn6lite_vpudec_vsp_dvfs_ops,
};

static const struct sprd_vsp_dvfs_data qogirn6lite_vpuenc_data = {
	.ver = "vpuenc",
	.max_freq_level = 5,
	.dvfs_ops = &qogirn6lite_vpuenc_vsp_dvfs_ops,
};

static const struct of_device_id vsp_dvfs_of_match[] = {
	{ .compatible = "sprd,hwdvfs-vsp-sharkl5",
	  .data = &sharkl5_vsp_data },
	{ .compatible = "sprd,hwdvfs-vsp-roc1",
	  .data = &roc1_vsp_data },
	{ .compatible = "sprd,hwdvfs-vsp-sharkl5pro",
	  .data = &sharkl5pro_vsp_data },
	{ .compatible = "sprd,hwdvfs-vsp-qogirl6",
	  .data = &qogirl6_vsp_data },
	{ .compatible = "sprd,hwdvfs-vsp-qogirn6pro",
	  .data = &qogirn6pro_vsp_data },
	{ .compatible = "sprd,hwdvfs-vpuenc-qogirn6pro",
	  .data = &qogirn6pro_vpuenc_data },
	{ .compatible = "sprd,hwdvfs-vsp-qogirn6lite",
	  .data = &qogirn6lite_vsp_data },
	{ .compatible = "sprd,hwdvfs-vpuenc-qogirn6lite",
	  .data = &qogirn6lite_vpuenc_data },
	{ },
};

MODULE_DEVICE_TABLE(of, vsp_dvfs_of_match);

BLOCKING_NOTIFIER_HEAD(vsp_dvfs_chain);
BLOCKING_NOTIFIER_HEAD(vpuenc_dvfs_chain);

int vsp_dvfs_notifier_call_chain(void *data, bool is_enc)
{
	if (is_enc) {
		pr_debug("notifier_call_chain: enc\n");
		return blocking_notifier_call_chain(&vpuenc_dvfs_chain, 0, data);
	} else {
		pr_debug("notifier_call_chain: dec\n");
		return blocking_notifier_call_chain(&vsp_dvfs_chain, 0, data);
	}
}
EXPORT_SYMBOL_GPL(vsp_dvfs_notifier_call_chain);

static ssize_t get_dvfs_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vsp_dvfs *vsp = dev_get_drvdata(devfreq->dev.parent);
	int ret = 0;

	if (!get_display_power_status()) {
		pr_err("dpu is stopped, reject get vpu dvfs status\n");
		return -EINVAL;
	}
	ret = sprintf(buf, "%d\n", vsp->ip_coeff.hw_dfs_en);

	return ret;
}

static ssize_t set_dvfs_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vsp_dvfs *vsp = dev_get_drvdata(devfreq->dev.parent);
	u32 user_en;
	int ret;

	if (!get_display_power_status()) {
		pr_err("dpu is stopped, reject set vpu dvfs status\n");
		return -EINVAL;
	}

	ret = sscanf(buf, "%u\n", &user_en);
	if (ret != 1)
		return -EINVAL;

	if (vsp->dvfs_ops && vsp->dvfs_ops->hw_dvfs_en) {
		vsp->dvfs_ops->hw_dvfs_en(vsp, user_en);
		vsp->ip_coeff.hw_dfs_en = user_en;
	}
	else
		pr_info("%s: ip ops null\n", __func__);
	ret = count;

	return ret;
}

static ssize_t get_work_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vsp_dvfs *vsp = dev_get_drvdata(devfreq->dev.parent);
	u32 work_freq;
	int ret = 0;

	if (!get_display_power_status()) {
		pr_err("dpu is stopped, reject get vpu dvfs status\n");
		return -EINVAL;
	}

	if (vsp->dvfs_ops && vsp->dvfs_ops->get_work_freq) {
		work_freq = vsp->dvfs_ops->get_work_freq(vsp);
		ret = sprintf(buf, "%u\n", work_freq);
	}
	else
		ret = sprintf(buf, "undefined\n");

	return ret;
}

static ssize_t set_work_freq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vsp_dvfs *vsp = dev_get_drvdata(devfreq->dev.parent);
	u32 user_freq;
	int ret;

	if (!get_display_power_status()) {
		pr_err("dpu is stopped, reject set vpu dvfs status\n");
		return -EINVAL;
	}

	mutex_lock(&devfreq->lock);
	ret = sscanf(buf, "%u\n", &user_freq);
	if (ret != 1) {
		mutex_unlock(&devfreq->lock);
		return -EINVAL;
	}
	pr_info("%s: dvfs freq %u", __func__, user_freq);
	vsp->work_freq = user_freq;
	vsp->freq_type = DVFS_WORK;

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
	struct vsp_dvfs *vsp = dev_get_drvdata(devfreq->dev.parent);
	int ret = 0;
	u32 idle_freq;

	if (!get_display_power_status()) {
		pr_err("dpu is stopped, reject get vpu dvfs status\n");
		return -EINVAL;
	}

	if (vsp->dvfs_ops && vsp->dvfs_ops->get_idle_freq) {
		idle_freq = vsp->dvfs_ops->get_idle_freq(vsp);
		ret = sprintf(buf, "%d\n", idle_freq);
	}
	else
		ret = sprintf(buf, "undefined\n");

	return ret;

}

static ssize_t set_idle_freq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vsp_dvfs *vsp = dev_get_drvdata(devfreq->dev.parent);
	u32 idle_freq;
	int ret;

	if (!get_display_power_status()) {
		pr_err("dpu is stopped, reject set vpu dvfs status\n");
		return -EINVAL;
	}

	mutex_lock(&devfreq->lock);
	ret = sscanf(buf, "%u\n", &idle_freq);
	if (ret != 1) {
		mutex_unlock(&devfreq->lock);
		return -EINVAL;
	}
	vsp->idle_freq = idle_freq;
	vsp->freq_type = DVFS_IDLE;
	ret = update_devfreq(devfreq);
	if (ret == 0)
		ret = count;
	mutex_unlock(&devfreq->lock);

	return ret;

}

static ssize_t get_work_index_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vsp_dvfs *vsp = dev_get_drvdata(devfreq->dev.parent);
	int ret = 0, work_index;

	if (!get_display_power_status()) {
		pr_err("dpu is stopped, reject get vpu dvfs status\n");
		return -EINVAL;
	}

	if (vsp->dvfs_ops && vsp->dvfs_ops->get_work_index) {
		work_index = vsp->dvfs_ops->get_work_index(vsp);
		ret = sprintf(buf, "%d\n", work_index);
	}
	else
		ret = sprintf(buf, "undefined\n");

	return ret;
}

static ssize_t set_work_index_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vsp_dvfs *vsp = dev_get_drvdata(devfreq->dev.parent);
	u32 work_index;
	int ret;

	if (!get_display_power_status()) {
		pr_err("dpu is stopped, reject set vpu dvfs status\n");
		return -EINVAL;
	}

	ret = sscanf(buf, "%u\n", &work_index);
	if (ret != 1)
		return -EINVAL;

	if (vsp->dvfs_ops && vsp->dvfs_ops->set_work_index)
		vsp->dvfs_ops->set_work_index(vsp, work_index);
	else
		pr_info("%s: ip ops null\n", __func__);

	return count;
}

static ssize_t get_idle_index_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vsp_dvfs *vsp = dev_get_drvdata(devfreq->dev.parent);
	int ret = 0, idle_index;

	if (!get_display_power_status()) {
		pr_err("dpu is stopped, reject get vpu dvfs status\n");
		return -EINVAL;
	}

	if (vsp->dvfs_ops && vsp->dvfs_ops->get_idle_index) {
		idle_index = vsp->dvfs_ops->get_idle_index(vsp);
		ret = sprintf(buf, "%d\n", idle_index);
	}
	else
		ret = sprintf(buf, "undefined\n");

	return ret;
}

static ssize_t set_idle_index_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vsp_dvfs *vsp = dev_get_drvdata(devfreq->dev.parent);
	u32 idle_index;
	int ret;

	if (!get_display_power_status()) {
		pr_err("dpu is stopped, reject set vpu dvfs status\n");
		return -EINVAL;
	}

	ret = sscanf(buf, "%u\n", &idle_index);
	if (ret != 1)
		return -EINVAL;

	if (vsp->dvfs_ops && vsp->dvfs_ops->set_idle_index)
		vsp->dvfs_ops->set_idle_index(vsp, idle_index);
	else
		pr_info("%s: ip ops null\n", __func__);

	return count;
}

static ssize_t get_dvfs_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vsp_dvfs *vsp = dev_get_drvdata(devfreq->dev.parent);
	struct ip_dvfs_status ip_status = {0};
	ssize_t len = 0;

	if (!get_display_power_status()) {
		pr_err("dpu is stopped, reject get vpu dvfs status\n");
		return -EINVAL;
	}

	if (vsp->dvfs_ops && vsp->dvfs_ops->get_dvfs_status)
		vsp->dvfs_ops->get_dvfs_status(vsp, &ip_status);
	else {
		len = sprintf(buf, "undefined\n");
		return len;
	}

	len = sprintf(buf, "apsys_cur_volt\tvsp_vote_volt\t"
			"vpuenc_vote_volt\tdpu_vote_volt\tvdsp_vote_volt\n");

	len += sprintf(buf + len, "%s\t\t%s\t\t%s\t\t%s\t\t%s\n",
			ip_status.apsys_cur_volt, ip_status.vsp_vote_volt,
			ip_status.vpuenc_vote_volt,
			ip_status.dpu_vote_volt, ip_status.vdsp_vote_volt);

	len += sprintf(buf + len, "\t\tvsp_cur_freq\tvpuenc_cur_freq\t"
			"dpu_cur_freq\tvdsp_cur_freq\n");

	len += sprintf(buf + len, "\t\t%s\t\t%s\t\t%s\t\t%s\n",
			ip_status.vsp_cur_freq, ip_status.vpuenc_cur_freq,
			ip_status.dpu_cur_freq, ip_status.vdsp_cur_freq);

	return len;
}


static ssize_t get_dvfs_table_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *devfreq = to_devfreq(dev);
	struct vsp_dvfs *vsp = dev_get_drvdata(devfreq->dev.parent);
	struct ip_dvfs_map_cfg dvfs_table[MAX_FREQ_LEVEL];
	ssize_t len = 0;
	int i;

	if (!get_display_power_status()) {
		pr_err("dpu is stopped, reject get vpu dvfs status\n");
		return -EINVAL;
	}

	if (vsp->dvfs_ops && vsp->dvfs_ops->get_dvfs_table)
		vsp->dvfs_ops->get_dvfs_table(dvfs_table);
	else {
		pr_info("%s: ip ops null\n", __func__);
		return len;
	}

	len = sprintf(buf, "map_index\tvolt_level\tclk_level\tclk_rate\n");
	for (i = 0; i < vsp->max_freq_level; i++) {
		len += sprintf(buf+len, "%d\t\t%d (%s)\t%d\t\t%d\t\t\n",
				dvfs_table[i].map_index,
				dvfs_table[i].volt_level,
				dvfs_table[i].volt_val,
				dvfs_table[i].clk_level,
				dvfs_table[i].clk_rate);
	}

	return len;
}

/*sys for gov_entries*/
static DEVICE_ATTR(dvfs_enable, 0644, get_dvfs_enable_show,
				   set_dvfs_enable_store);
static DEVICE_ATTR(work_freq, 0644, get_work_freq_show,
				   set_work_freq_store);
static DEVICE_ATTR(idle_freq, 0644, get_idle_freq_show,
				   set_idle_freq_store);
static DEVICE_ATTR(work_index, 0644, get_work_index_show,
				   set_work_index_store);
static DEVICE_ATTR(idle_index, 0644, get_idle_index_show,
				   set_idle_index_store);
static DEVICE_ATTR(dvfs_status, 0644, get_dvfs_status_show, NULL);
static DEVICE_ATTR(dvfs_table, 0644, get_dvfs_table_info_show, NULL);

static struct attribute *dev_entries[] = {
	&dev_attr_dvfs_enable.attr,
	&dev_attr_work_freq.attr,
	&dev_attr_idle_freq.attr,
	&dev_attr_work_index.attr,
	&dev_attr_idle_index.attr,
	&dev_attr_dvfs_status.attr,
	&dev_attr_dvfs_table.attr,
	NULL,
};

static struct attribute_group dev_attr_group = {
	.name    = "vsp_governor",
	.attrs    = dev_entries,
};

static void userspace_exit(struct devfreq *devfreq)
{
	/*
	 * Remove the sysfs entry, unless this is being called after
	 * device_del(), which should have done this already via kobject_del().
	 */
	if (devfreq->dev.kobj.sd)
		sysfs_remove_group(&devfreq->dev.kobj, &dev_attr_group);
}

static int userspace_init(struct devfreq *devfreq)
{
	int ret = 0;

	ret = sysfs_create_group(&devfreq->dev.kobj, &dev_attr_group);

	return ret;
}

static int vsp_dvfs_gov_get_target(struct devfreq *devfreq,
		unsigned long *freq)
{
	struct vsp_dvfs *vsp = dev_get_drvdata(devfreq->dev.parent);
	unsigned long adjusted_freq = 0;

	pr_debug("devfreq_governor-->get_target_freq\n");

	if (vsp->freq_type == DVFS_WORK)
		adjusted_freq = vsp->work_freq;
	else
		adjusted_freq = vsp->idle_freq;

	if (devfreq->scaling_max_freq && adjusted_freq > devfreq->scaling_max_freq)
		adjusted_freq = devfreq->scaling_max_freq;

	else if (devfreq->scaling_min_freq && adjusted_freq < devfreq->scaling_min_freq)
		adjusted_freq = devfreq->scaling_min_freq;

	*freq = adjusted_freq;

	pr_debug("dvfs *freq %lu", *freq);
	return 0;
}

static int vsp_dvfs_gov_event_handler(struct devfreq *devfreq,
		unsigned int event, void *data)
{
	int ret = 0;

	pr_info("devfreq_governor-->event_handler(%d)\n", event);
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

struct devfreq_governor vsp_devfreq_gov = {
	.name = "vsp_dvfs",
	.get_target_freq = vsp_dvfs_gov_get_target,
	.event_handler = vsp_dvfs_gov_event_handler,
};

static int vsp_dvfs_notify_callback(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct vsp_dvfs *vsp = container_of(nb, struct vsp_dvfs, vsp_dvfs_nb);
	u32 dvfs_freq = *(int *)data;

	mutex_lock(&vsp->devfreq->lock);

	if (!vsp->ip_coeff.hw_dfs_en) {
		pr_info("vsp dvfs is disabled, nothing to do");
		mutex_unlock(&vsp->devfreq->lock);
		return NOTIFY_DONE;
	}

	vsp->work_freq = dvfs_freq;
	vsp->freq_type = DVFS_WORK;

	if (update_devfreq(vsp->devfreq)) {
		pr_err("update devfreq fail\n");
		mutex_unlock(&vsp->devfreq->lock);
		return NOTIFY_DONE;
	}
	mutex_unlock(&vsp->devfreq->lock);

	return NOTIFY_OK;
}

static int vsp_dvfs_target(struct device *dev, unsigned long *freq,
		u32 flags)
{
	struct vsp_dvfs *vsp = dev_get_drvdata(dev);
	struct dev_pm_opp *opp;
	unsigned long target_freq;

	pr_debug("devfreq_dev_profile-->target, freq=%lu\n", *freq);
	opp = devfreq_recommended_opp(dev, freq, flags);

	if (IS_ERR(opp)) {
		dev_err(dev, "Failed to find opp for %lu KHz\n", *freq);
		return PTR_ERR(opp);
	}

	target_freq = dev_pm_opp_get_freq(opp);
	pr_debug("target freq from opp %lu\n", target_freq);
	dev_pm_opp_put(opp);

	if (vsp->freq_type == DVFS_WORK)
		vsp->work_freq = target_freq;
	else
		vsp->idle_freq = target_freq;
	vsp->dvfs_ops->updata_target_freq(vsp, target_freq, vsp->freq_type);

	return 0;
}

int vsp_dvfs_get_dev_status(struct device *dev,
		struct devfreq_dev_status *stat)
{
	/*
	struct vsp_dvfs *vsp = dev_get_drvdata(dev);
	struct devfreq_event_data edata;
	int ret = 0;

	pr_info("devfreq_dev_profile-->get_dev_status\n");
	ret = devfreq_event_get_event(vsp->edev, &edata);

	if (ret < 0)
		return ret;

	stat->current_frequency = vsp->freq;
	stat->busy_time = edata.load_count;
	stat->total_time = edata.total_count;

	return ret;
	*/
	return 0;
}

static int vsp_dvfs_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct vsp_dvfs *vsp = dev_get_drvdata(dev);

	if (vsp->freq_type == DVFS_WORK)
		*freq = vsp->work_freq;
	else
		*freq = vsp->idle_freq;
	pr_debug("devfreq_dev_profile-->get_cur_freq, *freq=%lu\n", *freq);

	return 0;
}
static struct devfreq_dev_profile vsp_dvfs_profile = {
	.target             = vsp_dvfs_target,
	.get_dev_status     = vsp_dvfs_get_dev_status,
	.get_cur_freq       = vsp_dvfs_get_cur_freq,
};

static int vsp_dvfs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct apsys_dev *apsys;
	struct vsp_dvfs *vsp;
	int ret;
	const struct sprd_vsp_dvfs_data *data = NULL;

	vsp = devm_kzalloc(dev, sizeof(*vsp), GFP_KERNEL);
	if (!vsp)
		return -ENOMEM;

	apsys = find_apsys_device_by_name("apsys-dvfs");
	if (!apsys)
		return -ENOMEM;

	vsp->apsys = apsys;

	data = (struct sprd_vsp_dvfs_data *)of_device_get_match_data(dev);
	if (data) {
		vsp->dvfs_ops = data->dvfs_ops;
		vsp->max_freq_level = data->max_freq_level;
	} else {
		pr_err("No matching driver data found\n");
		return -EINVAL;
	}

	if (!vsp->dvfs_ops) {
		pr_err("attach dvfs ops %s failed\n", data->ver);
		return -EINVAL;
	}

	ret = of_property_read_u32(np, "sprd,dvfs-work-freq",
			&vsp->work_freq);
	ret |= of_property_read_u32(np, "sprd,dvfs-idle-freq",
			&vsp->idle_freq);
	ret |= of_property_read_u32(np, "sprd,dvfs-enable-flag",
			&vsp->ip_coeff.hw_dfs_en);
	if (ret) {
		pr_err("parse sprd_vsp_dvfs dts failed\n");
		return -EINVAL;
	}
	pr_info("work freq %d, idle freq %d, enable flag %d\n",
			vsp->work_freq,
			vsp->idle_freq,
			vsp->ip_coeff.hw_dfs_en);

	if (dev_pm_opp_of_add_table(dev)) {
		dev_err(dev, "Invalid operating-points in device tree.\n");
		return -EINVAL;
	}
	vsp->vsp_dvfs_nb.notifier_call = vsp_dvfs_notify_callback;
	if (!strcmp("vpuenc", data->ver)) {
		pr_debug("chain_register: vpuenc \n");
		ret = blocking_notifier_chain_register(&vpuenc_dvfs_chain,
			&vsp->vsp_dvfs_nb);
	} else {
		pr_debug("chain_register: vpudec \n");
		ret = blocking_notifier_chain_register(&vsp_dvfs_chain,
			&vsp->vsp_dvfs_nb);
	}

	platform_set_drvdata(pdev, vsp);
	vsp->devfreq = devm_devfreq_add_device(dev,
					&vsp_dvfs_profile,
					"vsp_dvfs",
					NULL);
	if (IS_ERR(vsp->devfreq)) {
		dev_err(dev,
		"failed to add devfreq dev with vsp-dvfs governor\n");
		ret = PTR_ERR(vsp->devfreq);
		goto ret;
	}
	//device_rename(&vsp->devfreq->dev, "vsp");

	if (vsp->dvfs_ops && vsp->dvfs_ops->parse_dt)
		vsp->dvfs_ops->parse_dt(vsp, np);
	if (vsp->dvfs_ops && vsp->dvfs_ops->dvfs_init)
		vsp->dvfs_ops->dvfs_init(vsp);

	pr_info("Succeeded to register a vsp dvfs device\n");

	return 0;

ret:
	dev_pm_opp_of_remove_table(dev);
	if (!strcmp("vpuenc", data->ver))
		blocking_notifier_chain_unregister(&vpuenc_dvfs_chain, &vsp->vsp_dvfs_nb);
	else
		blocking_notifier_chain_unregister(&vsp_dvfs_chain, &vsp->vsp_dvfs_nb);

	return ret;
}

static int vsp_dvfs_remove(struct platform_device *pdev)
{
	return 0;
}

static __maybe_unused int vsp_dvfs_suspend(struct device *dev)
{
	pr_info("%s()\n", __func__);

	return 0;
}

static __maybe_unused int vsp_dvfs_resume(struct device *dev)
{
	struct vsp_dvfs *vsp = dev_get_drvdata(dev);

	pr_info("%s()\n", __func__);

	if (vsp->dvfs_ops && vsp->dvfs_ops->dvfs_init)
		vsp->dvfs_ops->dvfs_init(vsp);

	return 0;
}

static SIMPLE_DEV_PM_OPS(vsp_dvfs_pm, vsp_dvfs_suspend,
			 vsp_dvfs_resume);

struct platform_driver vsp_dvfs_driver = {
	.probe    = vsp_dvfs_probe,
	.remove     = vsp_dvfs_remove,
	.driver = {
		.name = "vsp-dvfs",
		.pm = &vsp_dvfs_pm,
		.of_match_table = vsp_dvfs_of_match,
	},
};

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Sprd vsp devfreq driver");
MODULE_AUTHOR("Chunlei Guo <chunlei.guo@unisoc.com>");

