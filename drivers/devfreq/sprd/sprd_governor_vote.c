// SPDX-License-Identifier: GPL-2.0
//
// Unisoc ddr dvfs driver
//
// Copyright (C) 2022 Unisoc, Inc.
// Author: Mingmin Ling <mingmin.ling@unisoc.com>

#include <linux/ctype.h>
#include <linux/devfreq.h>
#include <linux/errno.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include "sprd_ddr_dvfs.h"

static unsigned int force_freq;
static int backdoor_status;
static struct devfreq *gov_devfreq;
static struct mutex dfs_step_mutex;

static ssize_t scaling_request_ddr_freq_show(struct device *dev,
					     struct device_attribute *attr, char *buf)
{
	ssize_t count;
	unsigned int data;
	int err;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;

	err = gov_callback->get_request_freq(&data);
	if (err < 0)
		data = 0;
	count = sprintf(buf, "%u\n", data);

	if (err)
		return err;

	return count;
}

static ssize_t scaling_request_ddr_freq_store(struct device *dev, struct device_attribute *attr,
					      const char *buf, size_t count)
{
	unsigned int request_freq;
	int err;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	ktime_t call_time = ktime_to_ms(ktime_get());

	err = sscanf(buf, "%u\n", &request_freq);

	if (err < 1) {
		dev_warn(dev, "request freq para err: %d\n", err);
		return count;
	}
	err = gov_callback->send_freq_request(request_freq);
	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(SEND_FREQ_REQUEST_T, err, NULL, request_freq,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);
	if (err) {
		dev_err(dev, "request freq fail: %d\n", err);
		return err;
	}

	return count;
}
static DEVICE_ATTR_RW(scaling_request_ddr_freq);

static ssize_t scaling_force_ddr_freq_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	ssize_t count;
	unsigned int data, freq;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;
	ktime_t call_time = ktime_to_ms(ktime_get());

	err = gov_callback->get_dvfs_auto_status(&data);
	if (err < 0) {
		data = 0;
		dev_err(dev->parent, "get ddr auto dfs status fail: %d\n", err);
	}

	if (data == 1) {
		freq = 0;
	} else {
		err = gov_callback->get_force_freq(&freq);
		if (err < 0)
			freq = 0;
	}

	count = sprintf(buf, "%u\n", freq);

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(GET_DVFS_FORCE_FREQ_T, err, NULL, data,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);

	return count;
}

static ssize_t scaling_force_ddr_freq_store(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t count)
{
	int err;
	unsigned int i, freq_num = 0, status;
	unsigned long data = 0;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	ktime_t call_time = ktime_to_ms(ktime_get());

	err = gov_callback->get_freq_num(&freq_num);
	if (err < 0) {
		dev_warn(dev->parent, "get ddr freq num err: %d\n", err);
		return count;
	}
	err = sscanf(buf, "%u\n", &force_freq);

	if (err < 1) {
		dev_warn(dev->parent, "get scaling force ddr freq err: %d\n", err);
		return count;
	}
	for (i = 0; i < freq_num; i++) {
		err = gov_callback->get_freq_table(&data, i);
		if (!err && (data > 0))
			if (force_freq == data) {
				mutex_lock(&devfreq->lock);
				err = update_devfreq(devfreq);
				mutex_unlock(&devfreq->lock);
				break;
			}
	}

	if ((i == freq_num) || err) {
		dev_err(dev->parent, "force freq %u fail: %d, pid: %d, comm: %s\n",
			force_freq, err, task_pid_nr(current), current->comm);
		status = 1;
	} else {
		dev_info(dev->parent, "force ddr freq %u, pid: %d, comm: %s\n",
			 force_freq, task_pid_nr(current), current->comm);
		status = 0;
	}

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(SCALING_FORCE_DDR_FREQ, status, NULL, force_freq,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);

	if (err)
		return err;

	return count;
}
static DEVICE_ATTR_RW(scaling_force_ddr_freq);

static ssize_t scaling_overflow_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int i, freq_num, data;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;
	ktime_t call_time = ktime_to_ms(ktime_get());

	err = gov_callback->get_freq_num(&freq_num);
	if (err < 0)
		freq_num = 0;
	for (i = 0; i < freq_num; i++) {
		err = gov_callback->get_overflow(&data, i);
		if (err < 0) {
			data = 0;
			dev_err(dev->parent, "get sel[%u] overflow fail: %d\n", i, err);
		}
		count += sprintf(&buf[count], "%u ", data);
	}
	count += sprintf(&buf[count], "\n");

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(GET_OVERFLOW_T, 0, NULL, freq_num,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);

	if (err)
		return err;

	if (err)
		return err;

	return count;
}

static ssize_t scaling_overflow_store(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int sel, overflow, name_len;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	char *arg;
	int err;
	ktime_t call_time = ktime_to_ms(ktime_get());

	arg = (char *)buf;
	while (*arg && !isspace(*arg))
		arg++;
	name_len = arg - buf;
	if (!name_len)
		return -EINVAL;
	arg = kzalloc(name_len * sizeof(char) + 1, GFP_KERNEL);
	if (arg == NULL)
		return -EINVAL;

	memcpy(arg, buf, name_len);

	err = sscanf(buf, "%u %u\n", &sel, &overflow);
	if (err < 2) {
		dev_warn(dev->parent, "overflow para err: %d\n", err);
		kfree(arg);
		return count;
	}
	err = gov_callback->set_overflow(overflow, sel);
	if (err)
		dev_err(dev->parent, "set sel[%u] overflow %u fail: %d\n", sel, overflow, err);

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(SET_OVERFLOW_T, err, arg, name_len,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);
	kfree(arg);
	arg = NULL;

	if (err)
		return err;

	return count;
}
static DEVICE_ATTR_RW(scaling_overflow);

static ssize_t scaling_underflow_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int i, freq_num, data;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;
	ktime_t call_time = ktime_to_ms(ktime_get());

	err = gov_callback->get_freq_num(&freq_num);
	if (err < 0)
		freq_num = 0;
	for (i = 0; i < freq_num; i++) {
		err = gov_callback->get_underflow(&data, i);
		if (err < 0) {
			data = 0;
			dev_err(dev->parent, "get sel[%u] underflow fail: %d\n", i, err);
		}
		count += sprintf(&buf[count], "%u ", data);
	}
	count += sprintf(&buf[count], "\n");

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(GET_UNDERFLOW_T, 0, NULL, freq_num,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);

	if (err)
		return err;

	return count;
}

static ssize_t scaling_underflow_store(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{
	unsigned int sel, underflow, name_len;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	char *arg;
	int err;
	ktime_t call_time = ktime_to_ms(ktime_get());

	arg = (char *)buf;
	while (*arg && !isspace(*arg))
		arg++;
	name_len = arg - buf;
	if (!name_len)
		return -EINVAL;
	arg = kzalloc(name_len * sizeof(char) + 1, GFP_KERNEL);
	if (arg == NULL)
		return -EINVAL;

	memcpy(arg, buf, name_len);

	err = sscanf(buf, "%u %u\n", &sel, &underflow);
	if (err < 2) {
		dev_warn(dev->parent, "underflow para err: %d\n", err);
		kfree(arg);
		return count;
	}
	err = gov_callback->set_underflow(underflow, sel);
	if (err)
		dev_err(dev->parent, "set sel[%u] underflow %u fail: %d\n", sel, underflow, err);

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(SET_UNDERFLOW_T, err, arg, name_len,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);
	kfree(arg);
	arg = NULL;

	if (err)
		return err;

	return count;
}
static DEVICE_ATTR_RW(scaling_underflow);

static ssize_t dfs_on_off_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	ssize_t count;
	unsigned int data;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;
	ktime_t call_time = ktime_to_ms(ktime_get());

	err = gov_callback->get_dvfs_status(&data);
	if (err < 0) {
		data = 0;
		dev_err(dev->parent, "get ddr dfs status fail: %d\n", err);
	}
	count = sprintf(buf, "%u\n", data);

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(GET_DVFS_STATUS_T, err, NULL, data,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);

	if (err)
		return err;

	return count;
}

static ssize_t dfs_on_off_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int enable;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;
	ktime_t call_time = ktime_to_ms(ktime_get());

	err = sscanf(buf, "%u\n", &enable);
	if (err < 1) {
		dev_warn(dev->parent, "enable para err: %d\n", err);
		return count;
	}
	if (enable == 1)
		err = gov_callback->dvfs_enable();
	else if (enable == 0)
		err = gov_callback->dvfs_disable();
	else
		err = -EINVAL;

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(DFS_ON_OFF, err, NULL, enable,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);

	if (err) {
		dev_err(dev->parent, "ddr dfs enable[%u] fail: %d", enable, err);
		return err;
	}

	return count;
}
static DEVICE_ATTR_RW(dfs_on_off);

static ssize_t auto_dfs_on_off_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	ssize_t count;
	unsigned int data;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;
	ktime_t call_time = ktime_to_ms(ktime_get());

	err = gov_callback->get_dvfs_auto_status(&data);
	if (err < 0) {
		data = 0;
		dev_err(dev->parent, "get ddr auto dfs status fail: %d\n", err);
	}
	count = sprintf(buf, "%u\n", data);

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(GET_DVFS_AUTO_STATUS_T, err, NULL, data,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);

	if (err)
		return err;

	return count;
}

static ssize_t auto_dfs_on_off_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned int enable;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;
	ktime_t call_time = ktime_to_ms(ktime_get());

	err = sscanf(buf, "%u\n", &enable);
	if (err < 1) {
		dev_warn(dev->parent, "get auto dfs enable para err: %d\n", err);
		return count;
	}
	if (enable == 1)
		err = gov_callback->dvfs_auto_enable();
	else if (enable == 0)
		err = gov_callback->dvfs_auto_disable();
	else
		err = -EINVAL;

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(AUTO_DFS_ON_OFF, err, NULL, enable,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);

	if (err) {
		dev_err(dev->parent,
			"ddr auto dfs enable[%u] fail: %d, pid: %d, comm: %s\n",
			enable, err, task_pid_nr(current), current->comm);
		return err;
	}
	dev_info(dev->parent,
		 "ddr auto dfs enable[%u], pid: %d, comm: %s\n",
		 enable, task_pid_nr(current), current->comm);

	return count;
}
static DEVICE_ATTR_RW(auto_dfs_on_off);

static ssize_t ddrinfo_cur_freq_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t count;
	unsigned int data;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;
	ktime_t call_time = ktime_to_ms(ktime_get());

	err = gov_callback->get_cur_freq(&data);
	if (err < 0) {
		data = 0;
		dev_err(dev->parent, "get ddr cur freq fail: %d\n", err);
	}
	count = sprintf(buf, "%u\n", data);

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(GET_CUR_FREQ_T, err, NULL, data,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);

	if (err)
		return err;

	return count;
}
static DEVICE_ATTR_RO(ddrinfo_cur_freq);

static ssize_t ddrinfo_freq_table_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int i, freq_num = 0;
	unsigned long data;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;
	ktime_t call_time = ktime_to_ms(ktime_get());

	err = gov_callback->get_freq_num(&freq_num);
	if (err < 0)
		freq_num = 0;
	for (i = 0; i < freq_num; i++) {
		err = gov_callback->get_freq_table(&data, i);
		if (!err && (data > 0) && (data != 0xff))
			count += sprintf(&buf[count], "%lu ", data);
	}
	count += sprintf(&buf[count], "\n");

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(GET_FREQ_TABLE_T, 0, NULL, freq_num,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);

	if (err)
		return err;

	return count;
}
static DEVICE_ATTR_RO(ddrinfo_freq_table);

static ssize_t scenario_dfs_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int err;
	unsigned int name_len;
	char *arg;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	ktime_t call_time = ktime_to_ms(ktime_get());

	arg = (char *)buf;
	while (*arg && !isspace(*arg))
		arg++;
	name_len = arg - buf;
	if (!name_len)
		return -EINVAL;
	arg = kzalloc(name_len * sizeof(char) + 1, GFP_KERNEL);
	if (arg == NULL)
		return -EINVAL;

	memcpy(arg, buf, name_len);
	err = gov_callback->governor_vote(arg);
	if (err)
		dev_err(dev->parent, "scene %s enter fail: %d, pid: %d, comm: %s\n",
			arg, err, task_pid_nr(current), current->comm);

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(SCENARIO_DFS_ENTER, err, arg, name_len,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);
	kfree(arg);
	arg = NULL;

	if (err)
		return err;

	return count;
}
static DEVICE_ATTR_WO(scenario_dfs);

static ssize_t exit_scene_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int err;
	unsigned int name_len;
	char *arg;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	ktime_t call_time = ktime_to_ms(ktime_get());

	arg = (char *)buf;
	while (*arg && !isspace(*arg))
		arg++;
	name_len = arg - buf;
	if (!name_len)
		return -EINVAL;
	arg = kzalloc(name_len * sizeof(char) + 1, GFP_KERNEL);
	if (arg == NULL)
		return -EINVAL;

	memcpy(arg, buf, name_len);
	err = gov_callback->governor_unvote(arg);
	if (err)
		dev_err(dev->parent, "scene %s exit fail: %d, pid: %d, comm: %s\n",
			arg, err, task_pid_nr(current), current->comm);

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(EXIT_SCENE, err, arg, name_len,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);
	kfree(arg);
	arg = NULL;

	if (err)
		return err;

	return count;
}
static DEVICE_ATTR_WO(exit_scene);

static ssize_t scene_freq_set_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int err;
	unsigned int name_len, freq;
	char *arg;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	ktime_t call_time = ktime_to_ms(ktime_get());

	arg = (char *)buf;
	while (*arg && !isspace(*arg))
		arg++;
	name_len = arg - buf;
	if (!name_len)
		return -EINVAL;
	arg = kzalloc(name_len * sizeof(char) + 1, GFP_KERNEL);
	if (arg == NULL)
		return -EINVAL;

	memcpy(arg, buf, name_len);

	err = sscanf(&buf[name_len], "%u\n", &freq);
	if (err < 1) {
		dev_warn(dev->parent, "get set freq fail: %d\n", err);
		kfree(arg);
		return count;
	}

	err = gov_callback->governor_change_point(arg, freq);
	if (err)
		dev_err(dev->parent, "scene %s change freq %u fail: %d\n", arg, freq, err);

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(SCENE_FREQ_SET, err, arg, name_len,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);
	kfree(arg);
	arg = NULL;

	if (err)
		return err;

	return count;
}
static DEVICE_ATTR_WO(scene_freq_set);

static ssize_t scene_boost_dfs_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned int i, enable, freq, freq_num = 0;
	unsigned long data = 0;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	int err;
	ktime_t call_time = ktime_to_ms(ktime_get());

	err = sscanf(buf, "%u %u\n", &enable, &freq);
	if (err < 2) {
		dev_warn(dev->parent, "get boost para err: %d\n", err);
		return count;
	}

	if (enable == 1) {
		err = gov_callback->get_freq_num(&freq_num);
		if (err < 0) {
			dev_err(dev->parent, "boost flow get ddr freq num err: %d\n", err);
			goto out;
		}

		for (i = 0; i < freq_num; i++) {
			err = gov_callback->get_freq_table(&data, i);
			if (!err) {
				if ((data > 0) && (data != 0xff) && (freq == data)) {
					err = gov_callback->governor_change_point("boost", freq);
					if (err < 0) {
						dev_err(dev->parent, "change boost freq %u fail %d\n",
							freq, err);
						goto out;
					}
					break;
				}
			} else {
				dev_err(dev->parent, "boost get freq table fn%u fail %d\n", i, err);
				goto out;
			}
		}
		if (i < freq_num)
			err = gov_callback->governor_vote("boost");
		else
			err = -EFAULT;
	} else if (enable == 0) {
		err = gov_callback->governor_unvote("boost");
	} else {
		err = -EINVAL;
	}
out:
	mutex_lock(&dfs_step_mutex);
	if (enable == 1)
		gov_callback->ddr_dfs_step_add(SCENE_BOOST_ENTER, err, NULL, freq,
					       task_pid_nr(current), current->comm, call_time);
	else
		gov_callback->ddr_dfs_step_add(SCENE_BOOST_ENTER, err, NULL, enable,
					       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);

	if (err) {
		dev_err(dev->parent, "scene boost freq %u enter[%u] fail: %d\n",
			freq, enable, err);
		return err;
	}

	return count;
}
static DEVICE_ATTR_WO(scene_boost_dfs);

static ssize_t backdoor_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", backdoor_status);
}

static ssize_t backdoor_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int err, backdoor;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;
	ktime_t call_time = ktime_to_ms(ktime_get());

	err = sscanf(buf, "%d\n", &backdoor);
	if (err < 1) {
		dev_warn(dev->parent, "set backdoor err: %d\n", err);
		return count;
	}

	if (backdoor_status == backdoor)
		return count;

	err = -EINVAL;
	if (backdoor == 1)
		err = gov_callback->governor_vote("top");
	else if (backdoor == 0)
		err = gov_callback->governor_unvote("top");

	if (err) {
		dev_err(dev->parent, "set backdoor %d fail: %d, pid: %d, comm: %s\n",
			backdoor, err, task_pid_nr(current), current->comm);
		return err;
	}

	backdoor_status  = backdoor;
	gov_callback->dvfs_perf_mode_enable(backdoor);

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(SET_BACKDOOR, err, NULL, backdoor,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);
	dev_info(dev->parent, "set backdoor %d, pid: %d, comm: %s\n",
		 backdoor, task_pid_nr(current), current->comm);

	return count;
}
static DEVICE_ATTR_RW(backdoor);

static ssize_t scene_dfs_list_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	char *name;
	unsigned int freq, flag;
	int i = 0, err;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;

	do {
		err =  gov_callback->get_point_info(&name, &freq, &flag, i);
		if (err == 0)
			count += sprintf(&buf[count], "%s freq %u  flag %u\n",
					 name, freq, flag);
		i++;
	} while (!err);

	return count;
}
static DEVICE_ATTR_RO(scene_dfs_list);

static ssize_t ddrinfo_dfs_step_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned int i = 0;
	char *arg = "NONE_STEP", *step_status = "NONE_STATUS";
	char *scene = NULL, *comm = NULL;
	int err, pid = -1, buff = 0;
	ktime_t time = 0;
	struct devfreq *devfreq = to_devfreq(dev);
	struct governor_callback *gov_callback =
		(struct governor_callback *)devfreq->last_status.private_data;

	do {
		err = gov_callback->ddrinfo_dfs_step_parse(&arg, &step_status,
							   &scene, &buff, &pid, &comm, &time, i);

		if (scene == NULL)
			count += snprintf(&buf[count], INFO_LEN_MAX,
					  "dfs_dbg_log: DDR_DFS_STEP: %s %s, buff: %u, pid: %d, comm: %s, time: %lld ms\n",
					  arg, step_status, buff, pid, comm, time);
		else
			count += snprintf(&buf[count], INFO_LEN_MAX,
					  "dfs_dbg_log: DDR_DFS_STEP: %s %s, scene: %s, pid: %d, comm: %s, time: %lld ms\n",
					  arg, step_status, scene, pid, comm, time);
		i++;

		if (i >= PAGE_SIZE / INFO_LEN_MAX) // make sure count <= 4096
			break;
	} while (!err);

	return count;
}
static DEVICE_ATTR_RO(ddrinfo_dfs_step);

static struct attribute *dev_entries[] = {
	&dev_attr_scaling_request_ddr_freq.attr,
	&dev_attr_scaling_force_ddr_freq.attr,
	&dev_attr_scaling_overflow.attr,
	&dev_attr_scaling_underflow.attr,
	&dev_attr_dfs_on_off.attr,
	&dev_attr_auto_dfs_on_off.attr,
	&dev_attr_ddrinfo_cur_freq.attr,
	&dev_attr_ddrinfo_freq_table.attr,
	&dev_attr_scenario_dfs.attr,
	&dev_attr_exit_scene.attr,
	&dev_attr_scene_freq_set.attr,
	&dev_attr_scene_boost_dfs.attr,
	&dev_attr_scene_dfs_list.attr,
	&dev_attr_backdoor.attr,
	&dev_attr_ddrinfo_dfs_step.attr,
	NULL,
};

static struct attribute_group gov_vote_attrs = {
	.name   = "sprd-governor",
	.attrs  = dev_entries,
};

int scene_dfs_request(char *scenario)
{
	int err;
	struct governor_callback *gov_callback;
	struct devfreq *devfreq = gov_devfreq;

	if (!devfreq)
		return -ENODEV;

	gov_callback = (struct governor_callback *)devfreq->last_status.private_data;

	err = gov_callback->governor_vote(scenario);
	if (err)
		dev_err(devfreq->dev.parent, "scene %s enter fail: %d\n", scenario, err);

	return err;
}
EXPORT_SYMBOL(scene_dfs_request);

int scene_exit(char *scenario)
{
	int err;
	struct governor_callback *gov_callback;
	struct devfreq *devfreq = gov_devfreq;

	if (!devfreq)
		return -ENODEV;

	gov_callback = (struct governor_callback *)devfreq->last_status.private_data;

	err = gov_callback->governor_unvote(scenario);
	if (err)
		dev_err(devfreq->dev.parent, "scene %s exit fail: %d\n", scenario, err);

	return err;
}
EXPORT_SYMBOL(scene_exit);

int change_scene_freq(char *scenario, unsigned int freq)
{
	int err;
	char *arg;
	struct governor_callback *gov_callback;
	struct devfreq *devfreq = gov_devfreq;
	unsigned int name_len;
	ktime_t call_time = ktime_to_ms(ktime_get());

	if (!devfreq)
		return -ENODEV;

	arg = (char *)scenario;
	while (*arg && !isspace(*arg))
		arg++;
	name_len = arg - scenario;
	if (!name_len)
		return -EINVAL;
	arg = kzalloc(name_len * sizeof(char) + 1, GFP_KERNEL);
	if (arg == NULL)
		return -EINVAL;

	memcpy(arg, scenario, name_len);

	gov_callback = (struct governor_callback *)devfreq->last_status.private_data;

	err = gov_callback->governor_change_point(scenario, freq);
	if (err)
		dev_err(devfreq->dev.parent, "scene %s change freq %u fail: %d\n",
			scenario, freq, err);

	mutex_lock(&dfs_step_mutex);
	gov_callback->ddr_dfs_step_add(CHANGE_POINT, err, arg, name_len,
				       task_pid_nr(current), current->comm, call_time);
	mutex_unlock(&dfs_step_mutex);
	kfree(arg);

	return err;
}
EXPORT_SYMBOL(change_scene_freq);

static int gov_vote_start(struct devfreq *devfreq)
{
	int err;

	mutex_init(&dfs_step_mutex);
	err = sysfs_create_group(&devfreq->dev.kobj, &gov_vote_attrs);
	if (err) {
		dev_err(devfreq->dev.parent, "dvfs sysfs create fail: %d\n", err);
		return err;
	}

	err = devfreq_update_stats(devfreq);
	if (err) {
		dev_err(devfreq->dev.parent, "dvfs update states fail: %d\n", err);
		return err;
	}
	gov_devfreq = devfreq;

	return err;
}

static int gov_vote_stop(struct devfreq *devfreq)
{
	return 0;
}


static int gov_vote_get_target_freq(struct devfreq *devfreq,
	unsigned long *freq)
{
	*freq = force_freq;

	return 0;
}


static int gov_vote_handler(struct devfreq *devfreq,
	unsigned int event, void *data)
{
	switch (event) {
	case DEVFREQ_GOV_START:
		return gov_vote_start(devfreq);
	case DEVFREQ_GOV_STOP:
		return gov_vote_stop(devfreq);
	case DEVFREQ_GOV_UPDATE_INTERVAL:
	case DEVFREQ_GOV_SUSPEND:
	case DEVFREQ_GOV_RESUME:
	default:
		return 0;
	}
}

struct devfreq_governor sprd_vote = {
	.name = "sprd-governor",
	.get_target_freq = gov_vote_get_target_freq,
	.event_handler = gov_vote_handler,
};
