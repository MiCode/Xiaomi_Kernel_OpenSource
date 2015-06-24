/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/iommu.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/qcom_iommu.h>
#include "msm_iommu_perfmon.h"

static LIST_HEAD(iommu_list);
static struct dentry *msm_iommu_root_debugfs_dir;
static const char *NO_EVENT_CLASS_NAME = "none";
static const unsigned int MAX_EVEN_CLASS_NAME_LEN = 36;

struct event_class {
	unsigned int event_number;
	const char *desc;
};

static struct event_class pmu_event_classes[] = {
	{ 0x00, "cycle_count"      },
	{ 0x01, "cycle_count64"    },
	{ 0x08, "tlb_refill"       },
	{ 0x09, "tlb_refill_read"  },
	{ 0x0A, "tlb_refill_write" },
	{ 0x10, "access"           },
	{ 0x11, "access_read"      },
	{ 0x12, "access_write"     },
	{ 0x80, "full_misses"      },
	{ 0x81, "partial_miss_1lbfb_hit" },
	{ 0x82, "partial_miss_2lbfb_hit" },
	{ 0x83, "full_hit" },
	{ 0x90, "pred_req_full_miss" },
	{ 0x91, "pred_req_partial_miss_1lbfb_hit" },
	{ 0x92, "pred_req_partial_miss_2lbfb_hit" },
	{ 0xb0, "tot_num_miss_axi_htw_read_req" },
	{ 0xb1, "tot_num_pred_axi_htw_read_req" },
};

static unsigned int iommu_pm_create_sup_cls_str(char **buf,
						struct iommu_pmon *pmon)
{
	unsigned long buf_size = ARRAY_SIZE(pmu_event_classes) *
				 MAX_EVEN_CLASS_NAME_LEN;
	unsigned int pos = 0;
	unsigned int nevent_cls = pmon->nevent_cls_supported;

	*buf = kzalloc(buf_size, GFP_KERNEL);
	if (*buf) {
		unsigned int j;
		int i;
		struct event_class *ptr;
		size_t array_len = ARRAY_SIZE(pmu_event_classes);

		ptr = pmu_event_classes;

		for (j = 0; j < nevent_cls; ++j) {
			for (i = 0; i < array_len; ++i) {

				if (ptr[i].event_number !=
						pmon->event_cls_supported[j])
					continue;

				if (pos < buf_size) {
					pos += snprintf(&(*buf)[pos],
							buf_size-pos,
							"[%u] %s\n",
							ptr[i].event_number,
							ptr[i].desc);
				}
				break;
			}
		}
	}
	return pos;
}

static int iommu_pm_event_class_supported(struct iommu_pmon *pmon,
					  int event_class)
{
	unsigned int nevent_cls = pmon->nevent_cls_supported;
	unsigned int i;

	for (i = 0; i < nevent_cls; ++i) {
		if (event_class == pmon->event_cls_supported[i])
			return event_class;
	}
	return MSM_IOMMU_PMU_NO_EVENT_CLASS;
}

static const char *iommu_pm_find_event_class_name(int event_class)
{
	size_t array_len;
	struct event_class *ptr;
	int i;
	const char *event_class_name = NO_EVENT_CLASS_NAME;

	if (event_class < 0)
		goto out;

	array_len = ARRAY_SIZE(pmu_event_classes);
	ptr = pmu_event_classes;

	for (i = 0; i < array_len; ++i) {
		if (ptr[i].event_number == event_class) {
			event_class_name =  ptr[i].desc;
			break;
		}
	}

out:
	return event_class_name;
}

static int iommu_pm_find_event_class(struct iommu_pmon *pmon,
				     const char *event_class_name)
{
	size_t array_len;
	struct event_class *ptr;
	int i;
	int event_class = MSM_IOMMU_PMU_NO_EVENT_CLASS;

	if (strcmp(event_class_name, NO_EVENT_CLASS_NAME) == 0)
		goto out;

	array_len = ARRAY_SIZE(pmu_event_classes);
	ptr = pmu_event_classes;

	for (i = 0; i < array_len; ++i) {
		if (strcmp(ptr[i].desc, event_class_name) == 0) {
			event_class =  ptr[i].event_number;
			goto out;
		}
	}

out:
	event_class = iommu_pm_event_class_supported(pmon, event_class);
	return event_class;
}

static inline void iommu_pm_add_to_iommu_list(struct iommu_pmon *iommu_pmon)
{
	list_add(&iommu_pmon->iommu_list, &iommu_list);
}

static inline void iommu_pm_del_from_iommu_list(struct iommu_pmon *iommu_pmon)
{
	list_del(&iommu_pmon->iommu_list);
}

static struct iommu_pmon *iommu_pm_get_pm_by_dev(struct device *dev)
{
	struct iommu_pmon *pmon;
	struct iommu_info *info;
	struct list_head *ent;

	list_for_each(ent, &iommu_list) {
		pmon = list_entry(ent, struct iommu_pmon, iommu_list);
		info = &pmon->iommu;
		if (dev == info->iommu_dev)
			return pmon;
	}
	return NULL;
}

static void iommu_pm_set_event_type(struct iommu_pmon *pmon,
				    struct iommu_pmon_counter *counter)
{
	int event_class;
	unsigned int count_no;
	struct iommu_info *iommu = &pmon->iommu;

	event_class = counter->current_event_class;
	count_no = counter->absolute_counter_no;

	if (event_class == MSM_IOMMU_PMU_NO_EVENT_CLASS) {
		if (iommu->hw_ops->is_hw_access_OK(pmon)) {
			iommu->ops->iommu_lock_acquire(1);
			iommu->hw_ops->counter_disable(iommu, counter);
			iommu->hw_ops->ovfl_int_disable(iommu, counter);
			iommu->hw_ops->set_event_class(pmon, count_no, 0);
			iommu->ops->iommu_lock_release(1);
		}
		counter->overflow_count = 0;
		counter->value = 0;
	} else {
		counter->overflow_count = 0;
		counter->value = 0;
		if (iommu->hw_ops->is_hw_access_OK(pmon)) {
			iommu->ops->iommu_lock_acquire(1);
			iommu->hw_ops->set_event_class(pmon, count_no,
					event_class);
			iommu->hw_ops->ovfl_int_enable(iommu, counter);
			iommu->hw_ops->counter_enable(iommu, counter);
			iommu->ops->iommu_lock_release(1);
		}
	}
}

static void iommu_pm_reset_counts(struct iommu_pmon *pmon)
{
	unsigned int i;
	unsigned int j;

	for (i = 0; i < pmon->num_groups; ++i) {
		struct iommu_pmon_cnt_group *cnt_grp = &pmon->cnt_grp[i];

		for (j = 0; j < cnt_grp->num_counters; ++j) {
			cnt_grp->counters[j].value = 0;
			cnt_grp->counters[j].overflow_count = 0;
		}
	}
}

static void iommu_pm_set_all_counters(struct iommu_pmon *pmon)
{
	unsigned int i;
	unsigned int j;

	for (i = 0; i < pmon->num_groups; ++i) {
		struct iommu_pmon_cnt_group *cnt_grp = &pmon->cnt_grp[i];

		for (j = 0; j < cnt_grp->num_counters; ++j)
			iommu_pm_set_event_type(pmon, &cnt_grp->counters[j]);
	}
}

static void iommu_pm_read_all_counters(struct iommu_pmon *pmon)
{
	unsigned int i;
	unsigned int j;
	struct iommu_info *iommu = &pmon->iommu;

	for (i = 0; i < pmon->num_groups; ++i) {
		struct iommu_pmon_cnt_group *cnt_grp = &pmon->cnt_grp[i];

		for (j = 0; j < cnt_grp->num_counters; ++j) {
			struct iommu_pmon_counter *counter;

			counter = &cnt_grp->counters[j];
			counter->value = iommu->hw_ops->read_counter(counter);
		}
	}
}

static void iommu_pm_on(struct iommu_pmon *pmon)
{
	unsigned int i;
	struct iommu_info *iommu = &pmon->iommu;
	struct msm_iommu_drvdata *iommu_drvdata =
					dev_get_drvdata(iommu->iommu_dev);

	iommu->ops->iommu_power_on(iommu_drvdata);
	iommu->ops->iommu_bus_vote(iommu_drvdata, 1);
	iommu->ops->iommu_clk_on(iommu_drvdata);

	/* Reset counters in HW */
	iommu->ops->iommu_lock_acquire(1);
	iommu->hw_ops->reset_counters(&pmon->iommu);
	iommu->ops->iommu_lock_release(1);

	/* Reset SW counters */
	iommu_pm_reset_counts(pmon);

	pmon->enabled = 1;

	iommu_pm_set_all_counters(pmon);

	iommu->ops->iommu_lock_acquire(1);

	/* enable all counter group */
	for (i = 0; i < pmon->num_groups; ++i)
		iommu->hw_ops->grp_enable(iommu, i);

	/* enable global counters */
	iommu->hw_ops->enable_pm(iommu);
	iommu->ops->iommu_lock_release(1);

	pr_info("%s: TLB performance monitoring turned ON\n",
		pmon->iommu.iommu_name);
}

static void iommu_pm_off(struct iommu_pmon *pmon)
{
	unsigned int i;
	struct iommu_info *iommu = &pmon->iommu;
	struct msm_iommu_drvdata *iommu_drvdata =
					dev_get_drvdata(iommu->iommu_dev);

	pmon->enabled = 0;

	iommu->ops->iommu_lock_acquire(1);

	/* disable global counters */
	iommu->hw_ops->disable_pm(iommu);

	/* Check if we overflowed just before turning off pmon */
	iommu->hw_ops->check_for_overflow(pmon);

	/* disable all counter group */
	for (i = 0; i < pmon->num_groups; ++i)
		iommu->hw_ops->grp_disable(iommu, i);

	/* Update cached copy of counters before turning off power */
	iommu_pm_read_all_counters(pmon);

	iommu->ops->iommu_lock_release(1);
	iommu->ops->iommu_clk_off(iommu_drvdata);
	iommu->ops->iommu_bus_vote(iommu_drvdata, 0);
	iommu->ops->iommu_power_off(iommu_drvdata);

	pr_info("%s: TLB performance monitoring turned OFF\n",
		pmon->iommu.iommu_name);
}

static int iommu_pm_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t iommu_pm_count_value_read(struct file *fp,
					 char __user *user_buff,
					 size_t count, loff_t *pos)
{
	size_t rd_cnt;
	unsigned long long full_count;

	struct iommu_pmon_counter *counter = fp->private_data;
	struct iommu_pmon *pmon = counter->cnt_group->pmon;
	struct iommu_info *iommu = &pmon->iommu;
	char buf[50];
	size_t len;

	mutex_lock(&pmon->lock);

	if (iommu->hw_ops->is_hw_access_OK(pmon)) {
		iommu->ops->iommu_lock_acquire(1);
		counter->value = iommu->hw_ops->read_counter(counter);
		iommu->ops->iommu_lock_release(1);
	}
	full_count = (unsigned long long) counter->value +
		     ((unsigned long long)counter->overflow_count *
			0x100000000ULL);

	len = snprintf(buf, 50, "%llu\n", full_count);
	rd_cnt = simple_read_from_buffer(user_buff, count, pos, buf, len);
	mutex_unlock(&pmon->lock);

	return rd_cnt;
}

static const struct file_operations cnt_value_file_ops = {
	.open = iommu_pm_debug_open,
	.read = iommu_pm_count_value_read,
};

static ssize_t iommu_pm_event_class_read(struct file *fp,
					 char __user *user_buff,
					 size_t count, loff_t *pos)
{
	size_t rd_cnt;
	struct iommu_pmon_counter *counter = fp->private_data;
	struct iommu_pmon *pmon = counter->cnt_group->pmon;
	char buf[50];
	const char *event_class_name;
	size_t len;

	mutex_lock(&pmon->lock);
	event_class_name = iommu_pm_find_event_class_name(
						counter->current_event_class);
	len = snprintf(buf, 50, "%s\n", event_class_name);

	rd_cnt = simple_read_from_buffer(user_buff, count, pos, buf, len);
	mutex_unlock(&pmon->lock);
	return rd_cnt;
}

static ssize_t iommu_pm_event_class_write(struct file *fp,
					  const char __user *user_buff,
					  size_t count, loff_t *pos)
{
	size_t wr_cnt;
	char buf[50];
	size_t buf_size = sizeof(buf);
	struct iommu_pmon_counter *counter = fp->private_data;
	struct iommu_pmon *pmon = counter->cnt_group->pmon;
	int current_event_class;

	if ((count + *pos) >= buf_size)
		return -EINVAL;

	mutex_lock(&pmon->lock);
	current_event_class = counter->current_event_class;
	wr_cnt = simple_write_to_buffer(buf, buf_size, pos, user_buff, count);
	if (wr_cnt >= 1) {
		int rv;
		long value;

		buf[wr_cnt-1] = '\0';
		rv = kstrtol(buf, 10, &value);
		if (!rv) {
			counter->current_event_class =
				iommu_pm_find_event_class(pmon,
					iommu_pm_find_event_class_name(value));
		} else {
			counter->current_event_class =
					iommu_pm_find_event_class(pmon, buf);
	}	}

	if (current_event_class != counter->current_event_class)
		iommu_pm_set_event_type(pmon, counter);

	mutex_unlock(&pmon->lock);
	return wr_cnt;
}

static const struct file_operations event_class_file_ops = {
	.open = iommu_pm_debug_open,
	.read = iommu_pm_event_class_read,
	.write = iommu_pm_event_class_write,
};

static ssize_t iommu_reset_counters_write(struct file *fp,
				    const char __user *user_buff,
				    size_t count, loff_t *pos)
{
	size_t wr_cnt;
	char buf[10];
	size_t buf_size = sizeof(buf);
	struct iommu_pmon *pmon = fp->private_data;
	struct iommu_info *iommu = &pmon->iommu;

	if ((count + *pos) >= buf_size)
		return -EINVAL;

	mutex_lock(&pmon->lock);
	wr_cnt = simple_write_to_buffer(buf, buf_size, pos, user_buff, count);
	if (wr_cnt >= 1) {
		unsigned long cmd = 0;
		int rv;

		buf[wr_cnt-1] = '\0';
		rv = kstrtoul(buf, 10, &cmd);
		if (!rv && (cmd == 1)) {
			if (iommu->hw_ops->is_hw_access_OK(pmon)) {
				iommu->ops->iommu_lock_acquire(1);
				iommu->hw_ops->reset_counters(&pmon->iommu);
				iommu->ops->iommu_lock_release(1);
			}
			iommu_pm_reset_counts(pmon);
			pr_info("TLB performance counters reset\n");
		} else {
			pr_err("Unknown performance monitor command: %lu\n",
				cmd);
		}
	}
	mutex_unlock(&pmon->lock);
	return wr_cnt;
}

static const struct file_operations reset_file_ops = {
	.open = iommu_pm_debug_open,
	.write = iommu_reset_counters_write,
};

static ssize_t iommu_pm_enable_counters_read(struct file *fp,
					     char __user *user_buff,
					     size_t count, loff_t *pos)
{
	size_t rd_cnt;
	char buf[5];
	size_t len;
	struct iommu_pmon *pmon = fp->private_data;

	mutex_lock(&pmon->lock);
	len = snprintf(buf, 5, "%u\n", pmon->enabled);
	rd_cnt = simple_read_from_buffer(user_buff, count, pos, buf, len);
	mutex_unlock(&pmon->lock);
	return rd_cnt;
}

static ssize_t iommu_pm_enable_counters_write(struct file *fp,
				     const char __user *user_buff,
				     size_t count, loff_t *pos)
{
	size_t wr_cnt;
	char buf[10];
	size_t buf_size = sizeof(buf);
	struct iommu_pmon *pmon = fp->private_data;

	if ((count + *pos) >= buf_size)
		return -EINVAL;

	mutex_lock(&pmon->lock);
	wr_cnt = simple_write_to_buffer(buf, buf_size, pos, user_buff, count);
	if (wr_cnt >= 1) {
		unsigned long cmd;
		int rv;

		buf[wr_cnt-1] = '\0';
		rv = kstrtoul(buf, 10, &cmd);
		if (!rv && (cmd < 2)) {
			if (pmon->enabled == 1 && cmd == 0) {
				if (pmon->iommu.always_on ||
				    pmon->iommu_attach_count > 0)
					iommu_pm_off(pmon);
			} else if (pmon->enabled == 0 && cmd == 1) {
				/* We can only turn on perf. monitoring if
				 * iommu is attached (if not always on).
				 * Delay turning on perf. monitoring until
				 * we are attached.
				 */
				if (pmon->iommu.always_on ||
				    pmon->iommu_attach_count > 0)
					iommu_pm_on(pmon);
				else
					pmon->enabled = 1;
			}
		} else {
			pr_err("Unknown performance monitor command: %lu\n",
				cmd);
		}
	}
	mutex_unlock(&pmon->lock);
	return wr_cnt;
}

static const struct file_operations event_enable_file_ops = {
	.open = iommu_pm_debug_open,
	.read = iommu_pm_enable_counters_read,
	.write = iommu_pm_enable_counters_write,
};

static ssize_t iommu_pm_avail_event_cls_read(struct file *fp,
					     char __user *user_buff,
					     size_t count, loff_t *pos)
{
	size_t rd_cnt = 0;
	struct iommu_pmon *pmon = fp->private_data;
	char *buf;
	size_t len;

	mutex_lock(&pmon->lock);

	len = iommu_pm_create_sup_cls_str(&buf, pmon);
	if (buf) {
		rd_cnt = simple_read_from_buffer(user_buff, count, pos,
						 buf, len);
		kfree(buf);
	}
	mutex_unlock(&pmon->lock);
	return rd_cnt;
}

static const struct file_operations available_event_cls_file_ops = {
	.open = iommu_pm_debug_open,
	.read = iommu_pm_avail_event_cls_read,
};



static int iommu_pm_create_grp_debugfs_counters_hierarchy(
					struct iommu_pmon_cnt_group *cnt_grp,
					unsigned int *abs_counter_no)
{
	int ret = 0;
	int j;
	char name[20];

	for (j = 0; j < cnt_grp->num_counters; ++j) {
		struct dentry *grp_dir = cnt_grp->group_dir;
		struct dentry *counter_dir;

		cnt_grp->counters[j].cnt_group = cnt_grp;
		cnt_grp->counters[j].counter_no = j;
		cnt_grp->counters[j].absolute_counter_no = *abs_counter_no;
		(*abs_counter_no)++;
		cnt_grp->counters[j].value = 0;
		cnt_grp->counters[j].overflow_count = 0;
		cnt_grp->counters[j].current_event_class =
						MSM_IOMMU_PMU_NO_EVENT_CLASS;

		snprintf(name, 20, "counter%u", j);

		counter_dir = debugfs_create_dir(name, grp_dir);

		if (IS_ERR_OR_NULL(counter_dir)) {
			pr_err("unable to create counter debugfs dir %s\n",
				name);
			ret = -ENOMEM;
			goto out;
		}

		cnt_grp->counters[j].counter_dir = counter_dir;

		if (!debugfs_create_file("value", 0644, counter_dir,
					 &cnt_grp->counters[j],
					 &cnt_value_file_ops)) {
			ret = -EIO;
			goto out;
		}

		if (!debugfs_create_file("current_event_class", 0644,
				counter_dir, &cnt_grp->counters[j],
				&event_class_file_ops)) {
			ret = -EIO;
			goto out;
		}
	}
out:
	return ret;
}

static int iommu_pm_create_group_debugfs_hierarchy(struct iommu_info *iommu,
				   struct iommu_pmon *pmon_entry)
{
	int i;
	int ret = 0;
	char name[20];
	unsigned int abs_counter_no = 0;

	for (i = 0; i < pmon_entry->num_groups; ++i) {
		pmon_entry->cnt_grp[i].pmon = pmon_entry;
		pmon_entry->cnt_grp[i].grp_no = i;
		pmon_entry->cnt_grp[i].num_counters = pmon_entry->num_counters;
		pmon_entry->cnt_grp[i].counters =
			kzalloc(sizeof(*pmon_entry->cnt_grp[i].counters)
			* pmon_entry->cnt_grp[i].num_counters, GFP_KERNEL);

		if (!pmon_entry->cnt_grp[i].counters) {
			pr_err("Unable to allocate memory for counters\n");
			ret = -ENOMEM;
			goto out;
		}
		snprintf(name, 20, "group%u", i);
		pmon_entry->cnt_grp[i].group_dir = debugfs_create_dir(name,
							pmon_entry->iommu_dir);
		if (IS_ERR_OR_NULL(pmon_entry->cnt_grp[i].group_dir)) {
			pr_err("unable to create group debugfs dir %s\n", name);
			ret = -ENOMEM;
			goto out;
		}

		ret = iommu_pm_create_grp_debugfs_counters_hierarchy(
						&pmon_entry->cnt_grp[i],
						&abs_counter_no);
		if (ret)
			goto out;
	}
out:
	return ret;
}

int msm_iommu_pm_iommu_register(struct iommu_pmon *pmon_entry)
{
	int ret = 0;
	struct iommu_info *iommu = &pmon_entry->iommu;
	int i;

	if (!iommu->ops || !iommu->iommu_name || !iommu->base
					|| !iommu->iommu_dev) {
		ret = -EINVAL;
		goto out;
	}

	if (!msm_iommu_root_debugfs_dir) {
		msm_iommu_root_debugfs_dir = debugfs_create_dir("iommu", NULL);
		if (IS_ERR_OR_NULL(msm_iommu_root_debugfs_dir)) {
			pr_err("Failed creating iommu debugfs dir \"iommu\"\n");
			ret = -EIO;
			goto out;
		}
	}

	pmon_entry->cnt_grp = kzalloc(sizeof(*pmon_entry->cnt_grp)
				      * pmon_entry->num_groups, GFP_KERNEL);
	if (!pmon_entry->cnt_grp) {
		ret = -ENOMEM;
		goto file_err;
	}
	pmon_entry->iommu_dir = debugfs_create_dir(iommu->iommu_name,
						   msm_iommu_root_debugfs_dir);
	if (IS_ERR_OR_NULL(pmon_entry->iommu_dir)) {
		pr_err("unable to create iommu debugfs dir %s\n",
							iommu->iommu_name);
		ret = -ENOMEM;
		goto free_mem;
	}

	if (!debugfs_create_file("reset_counters", 0644,
			pmon_entry->iommu_dir, pmon_entry, &reset_file_ops)) {
		ret = -EIO;
		goto free_mem;
	}

	if (!debugfs_create_file("enable_counters", 0644,
		pmon_entry->iommu_dir, pmon_entry, &event_enable_file_ops)) {
		ret = -EIO;
		goto free_mem;
	}

	if (!debugfs_create_file("available_event_classes", 0644,
			pmon_entry->iommu_dir, pmon_entry,
			&available_event_cls_file_ops)) {
		ret = -EIO;
		goto free_mem;
	}

	ret = iommu_pm_create_group_debugfs_hierarchy(iommu, pmon_entry);
	if (ret)
		goto free_mem;

	iommu->hw_ops->initialize_hw(pmon_entry);

	if (iommu->evt_irq > 0) {
		ret = request_threaded_irq(iommu->evt_irq, NULL,
				iommu->hw_ops->evt_ovfl_int_handler,
				IRQF_ONESHOT | IRQF_SHARED,
				"msm_iommu_pmon_nonsecure_irq", pmon_entry);
		if (ret) {
			pr_err("Request IRQ %d failed with ret=%d\n",
								iommu->evt_irq,
								ret);
			goto free_mem;
		}
	} else {
		pr_info("%s: Overflow interrupt not available\n", __func__);
	}

	dev_dbg(iommu->iommu_dev, "%s iommu registered\n", iommu->iommu_name);

	goto out;
free_mem:
	if (pmon_entry->cnt_grp) {
		for (i = 0; i < pmon_entry->num_groups; ++i) {
			kfree(pmon_entry->cnt_grp[i].counters);
			pmon_entry->cnt_grp[i].counters = 0;
		}
	}
	kfree(pmon_entry->cnt_grp);
	pmon_entry->cnt_grp = 0;
file_err:
	debugfs_remove_recursive(msm_iommu_root_debugfs_dir);
out:
	return ret;
}
EXPORT_SYMBOL(msm_iommu_pm_iommu_register);

void msm_iommu_pm_iommu_unregister(struct device *dev)
{
	int i;
	struct iommu_pmon *pmon_entry = iommu_pm_get_pm_by_dev(dev);

	if (!pmon_entry)
		return;

	free_irq(pmon_entry->iommu.evt_irq, pmon_entry->iommu.iommu_dev);

	if (!pmon_entry)
		goto remove_debugfs;

	if (pmon_entry->cnt_grp) {
		for (i = 0; i < pmon_entry->num_groups; ++i)
			kfree(pmon_entry->cnt_grp[i].counters);
	}

	kfree(pmon_entry->cnt_grp);

remove_debugfs:
	debugfs_remove_recursive(msm_iommu_root_debugfs_dir);
}
EXPORT_SYMBOL(msm_iommu_pm_iommu_unregister);

struct iommu_pmon *msm_iommu_pm_alloc(struct device *dev)
{
	struct iommu_pmon *pmon_entry;
	struct iommu_info *info;

	pmon_entry = devm_kzalloc(dev, sizeof(*pmon_entry), GFP_KERNEL);
	if (!pmon_entry)
		return NULL;
	info = &pmon_entry->iommu;
	info->iommu_dev = dev;
	mutex_init(&pmon_entry->lock);
	iommu_pm_add_to_iommu_list(pmon_entry);
	return pmon_entry;
}
EXPORT_SYMBOL(msm_iommu_pm_alloc);

void msm_iommu_pm_free(struct device *dev)
{
	struct iommu_pmon *pmon = iommu_pm_get_pm_by_dev(dev);

	if (pmon)
		iommu_pm_del_from_iommu_list(pmon);
}
EXPORT_SYMBOL(msm_iommu_pm_free);

void msm_iommu_attached(struct device *dev)
{
	struct iommu_pmon *pmon = iommu_pm_get_pm_by_dev(dev);

	if (pmon) {
		mutex_lock(&pmon->lock);
		++pmon->iommu_attach_count;
		if (pmon->iommu_attach_count == 1) {
			/* If perf. mon was enabled before we attached we do
			 * the actual enabling after we attach.
			 */
			if (pmon->enabled && !pmon->iommu.always_on)
				iommu_pm_on(pmon);
		}
		mutex_unlock(&pmon->lock);
	}
}
EXPORT_SYMBOL(msm_iommu_attached);

void msm_iommu_detached(struct device *dev)
{
	struct iommu_pmon *pmon = iommu_pm_get_pm_by_dev(dev);

	if (pmon) {
		mutex_lock(&pmon->lock);
		if (pmon->iommu_attach_count == 1) {
			/* If perf. mon is still enabled we have to disable
			 * before we do the detach if iommu is not always on.
			 */
			if (pmon->enabled && !pmon->iommu.always_on)
				iommu_pm_off(pmon);
		}
		BUG_ON(pmon->iommu_attach_count == 0);
		--pmon->iommu_attach_count;
		mutex_unlock(&pmon->lock);
	}
}
EXPORT_SYMBOL(msm_iommu_detached);
