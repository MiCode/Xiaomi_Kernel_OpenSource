/*
 * Copyright (C) 2011 Unisoc Co., Ltd.
 * Jinfeng.Lin <Jinfeng.Lin1@unisoc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/power/sprd_vote.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>

static struct sprd_vote *g_vote;
static void *private_data;
static bool vote_disabled;

static void sprd_vote_lock(struct sprd_vote *vote_gov)
{
	mutex_lock(&vote_gov->lock);
}

static void sprd_vote_unlock(struct sprd_vote *vote_gov)
{
	mutex_unlock(&vote_gov->lock);
}

static void sprd_vote_reset_last_vote_dynamic(struct sprd_vote *vote_gov)
{
	int i;

	for (i = 0; i < SPRD_VOTE_TYPE_MAX; i++) {
		vote_gov->last_vote_dynamic[i].value = 0;
		vote_gov->last_vote_dynamic[i].vote_type = 0;
		vote_gov->last_vote_dynamic[i].vote_cmd = 0;
	}
}

static void sprd_vote_set_enable_prop(struct sprd_vote_client *vote_client, int len, bool enable)
{
	int i;

	for (i = 0; i < len; i++)
		vote_client[i].enable = enable;
}

static int sprd_vote_all(struct sprd_vote *vote_gov, int vote_type_id, int vote_cmd,
			 int value, int *target_value, bool enable)
{
	sprd_vote_set_enable_prop(vote_gov->ibat_client, SPRD_VOTE_TYPE_IBAT_ID_MAX, enable);
	sprd_vote_set_enable_prop(vote_gov->ibus_client, SPRD_VOTE_TYPE_IBUS_ID_MAX, enable);
	sprd_vote_set_enable_prop(vote_gov->cccv_client, SPRD_VOTE_TYPE_CCCV_ID_MAX, enable);

	if (!enable)
		sprd_vote_reset_last_vote_dynamic(vote_gov);

	return true;
}

static int sprd_vote_select(struct sprd_vote_client *vote_client, int vote_type,
			    int id, int vote_type_id_len, int vote_cmd,
			    int value, bool enable, int *target_value)
{
	int i, min = 0x7fffffff, max = -1;
	int ret = 0;

	if (vote_client[id].value == value && vote_client[id].enable == enable) {
		pr_info("vote_gov:%s vote the same vote_client[%d].value = %d,"
			" value = %d; vote_client[%d].enable = %d, enable = %d\n",
			vote_type_names[vote_type], id, vote_client[id].value,
			value, id, vote_client[id].enable, enable);
		ret = -EINVAL;
		return ret;
	}
	vote_client[id].value = value;
	vote_client[id].enable = enable;

	for (i = 0; i < vote_type_id_len; i++) {
		if (!vote_client[i].enable)
			continue;
		if (min > vote_client[i].value)
			min = vote_client[i].value;
		if (max < vote_client[i].value)
			max = vote_client[i].value;
	}

	switch (vote_cmd) {
	case SPRD_VOTE_CMD_MIN:
		*target_value = min;
		break;
	case SPRD_VOTE_CMD_MAX:
		*target_value = max;
		break;
	default:
		ret = -EINVAL;
		pr_err("vote_gov: vote_cmd[%d] error!!!\n", vote_cmd);
		break;
	}

	return ret;
}

static int sprd_vote_func(struct sprd_vote *vote_gov, bool enable, int vote_type,
			  int vote_type_id, int vote_cmd, int value, void *data)
{
	int target_value = 0, len;
	struct sprd_vote_client *vote_client = NULL;
	int ret = 0;

	sprd_vote_lock(vote_gov);

	switch (vote_type) {
	case SPRD_VOTE_TYPE_IBAT:
		len = SPRD_VOTE_TYPE_IBAT_ID_MAX;
		vote_client = vote_gov->ibat_client;
		break;
	case SPRD_VOTE_TYPE_IBUS:
		len = SPRD_VOTE_TYPE_IBUS_ID_MAX;
		vote_client = vote_gov->ibus_client;
		break;
	case SPRD_VOTE_TYPE_CCCV:
		len = SPRD_VOTE_TYPE_CCCV_ID_MAX;
		vote_client = vote_gov->cccv_client;
		break;
	case SPRD_VOTE_TYPE_ALL:
		pr_info("vote_gov: vote_all[%s]\n", enable ? "enable" : "disable");
		ret = sprd_vote_all(vote_gov, vote_type_id, vote_cmd,
				    value, &target_value, enable);
		break;
	default:
		pr_err("vote_gov: vote_type[%d] error!!!\n", vote_type);
		ret = -EINVAL;
		break;
	}

	if (ret)
		goto vote_unlock;

	if (vote_type_id >= len) {
		pr_err("vote_gov: vote_type[%d]: vote_type_id[%d] out of range",
		       vote_type, vote_type_id);
		ret = -EINVAL;
		goto vote_unlock;
	}

	ret = sprd_vote_select(vote_client, vote_type, vote_type_id, len,
			       vote_cmd, value, enable, &target_value);
	if (ret)
		goto vote_unlock;

	vote_gov->last_vote_dynamic[vote_type].vote_type = vote_type;
	vote_gov->last_vote_dynamic[vote_type].vote_cmd = vote_cmd;
	vote_gov->last_vote_dynamic[vote_type].value = target_value;

	if (vote_disabled) {
		pr_err("vote function disabled!!\n");
		goto vote_unlock;
	}

	vote_gov->cb(vote_gov, vote_type, target_value, data);

vote_unlock:
	sprd_vote_unlock(vote_gov);
	return ret;
}

static void sprd_vote_force_callback(struct sprd_vote *vote_gov)
{
	int i;

	for (i = 0; i < SPRD_VOTE_TYPE_MAX; i++) {
		if (vote_gov->last_vote_dynamic[i].value)
			vote_gov->cb(vote_gov,
				     vote_gov->last_vote_dynamic[i].vote_type,
				     vote_gov->last_vote_dynamic[i].value,
				     private_data);
	}
}

static void sprd_vote_reset_last_vote_fixied(struct sprd_vote *vote_gov)
{
	int i;

	for (i = 0; i < SPRD_VOTE_TYPE_MAX; i++) {
		vote_gov->last_vote_fixied[i].value = 0;
		vote_gov->last_vote_fixied[i].vote_type = 0;
	}
}

static void sprd_vote_save_last_vote_fixied(struct sprd_vote *vote_gov, int vote_type, int value)
{
	vote_gov->last_vote_fixied[vote_type].value = value;
	vote_gov->last_vote_fixied[vote_type].vote_type = vote_type;
}

static ssize_t vote_gov_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct sprd_vote *vote = g_vote;

	if (!vote) {
		dev_err(dev, "%s vote is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "vote is null\n");
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", g_vote->name);
}

static ssize_t vote_info_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct sprd_vote *vote = g_vote;
	int i, len, idx = 0;
	char bufinfo[1024];

	if (!vote) {
		dev_err(dev, "%s vote is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "vote is null\n");
	}

	memset(bufinfo, '\0', sizeof(bufinfo));

	len = snprintf(bufinfo + idx, sizeof(bufinfo) - idx, "[SPRD_VOTE_FUNC][%s]\n",
		       vote_disabled ? "disabled" : "enabled");
	idx += len;

	for (i = 0; i < SPRD_VOTE_TYPE_IBAT_ID_MAX; i++) {
		len = snprintf(bufinfo + idx, sizeof(bufinfo) - idx, "[%s][%d][%s]\n",
			       vote_type_names[SPRD_VOTE_TYPE_IBAT],
			       vote->ibat_client[i].value,
			       vote->ibat_client[i].enable ? "enabled" : "disabled");
		idx += len;
	}

	for (i = 0; i < SPRD_VOTE_TYPE_IBUS_ID_MAX; i++) {
		len = snprintf(bufinfo + idx, sizeof(bufinfo) - idx, "[%s][%d][%s]\n",
			       vote_type_names[SPRD_VOTE_TYPE_IBUS],
			       vote->ibus_client[i].value,
			       vote->ibus_client[i].enable ? "enabled" : "disabled");
		idx += len;
	}

	for (i = 0; i < SPRD_VOTE_TYPE_CCCV_ID_MAX; i++) {
		len = snprintf(bufinfo + idx, sizeof(bufinfo) - idx, "[%s][%d][%s]\n",
			       vote_type_names[SPRD_VOTE_TYPE_CCCV],
			       vote->cccv_client[i].value,
			       vote->cccv_client[i].enable ? "enabled" : "disabled");
		idx += len;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", bufinfo);
}

static ssize_t vote_disabled_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct sprd_vote *vote = g_vote;
	int ret;
	bool value;

	if (!vote) {
		dev_err(dev, "%s vote is null\n", __func__);
		return count;
	}

	ret = kstrtobool(buf, &value);
	if (ret) {
		dev_err(dev, "fail to store vote_disabled, ret = %d\n", ret);
		return count;
	}

	sprd_vote_lock(vote);
	vote_disabled = value;

	if (!vote_disabled) {
		sprd_vote_force_callback(vote);
		sprd_vote_reset_last_vote_fixied(vote);
	}
	sprd_vote_unlock(vote);

	return count;
}

static ssize_t vote_disabled_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct sprd_vote *vote = g_vote;

	if (!vote) {
		dev_err(dev, "%s vote is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "vote is null\n");
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", vote_disabled ? "Disabled" : "Enabled");
}

static ssize_t vote_fixed_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_vote *vote = g_vote;
	int ret;
	u32 value;

	if (!vote) {
		dev_err(dev, "%s g_vote is null\n", __func__);
		return count;
	}

	ret =  kstrtouint(buf, 10, &value);
	if (ret) {
		dev_err(dev, "fail to store fixied value, ret = %d\n", ret);
		return count;
	}

	if (vote->current_vote_type >= SPRD_VOTE_TYPE_MAX) {
		dev_err(dev, "current_vote_type overflow\n");
		return count;
	}

	sprd_vote_save_last_vote_fixied(vote, vote->current_vote_type, value);
	vote->cb(vote, vote->current_vote_type, value, private_data);
	dev_info(dev, "%s , vote_type = %d, value = %d\n",
		 __func__, vote->current_vote_type, value);

	return count;
}

static ssize_t voted_type_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sprd_vote *vote = g_vote;

	if (!vote) {
		dev_err(dev, "%s vote is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "vote is null\n");
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", vote_type_names[vote->current_vote_type]);
}

static ssize_t voted_type_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct sprd_vote *vote = g_vote;
	int ret;
	u32 type;

	if (!vote) {
		dev_err(dev, "%s vote is null\n", __func__);
		return count;
	}

	ret =  kstrtouint(buf, 10, &type);
	if (ret) {
		dev_err(dev, "fail to store vote_type, ret = %d\n", ret);
		return count;
	}

	if (type >= SPRD_VOTE_TYPE_MAX) {
		dev_err(dev, "type overflow, ret = %d\n", ret);
		return count;

	}

	vote->current_vote_type = type;
	dev_info(dev, "%s [%s][%d]\n", __func__, vote_type_names[type], type);

	return count;
}

static ssize_t voted_value_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct sprd_vote *vote = g_vote;
	sprd_vote_last_vote *last_vote;
	int i, len, idx = 0;
	char bufinfo[1024];

	if (!vote) {
		dev_err(dev, "%s vote is null\n", __func__);
		return snprintf(buf, PAGE_SIZE, "vote is null\n");
	}

	memset(bufinfo, '\0', sizeof(bufinfo));
	len = snprintf(bufinfo + idx, sizeof(bufinfo) - idx, "[SPRD_VOTE_FUNC][%s]\n",
		       vote_disabled ? "disabled" : "enabled");
	idx += len;

	if (vote_disabled)
		last_vote = vote->last_vote_fixied;
	else
		last_vote = vote->last_vote_dynamic;

	for (i = 0; i < SPRD_VOTE_TYPE_MAX; i++) {
		len = snprintf(bufinfo + idx, sizeof(bufinfo) - idx, "[%s][%s][%s][%d]\n",
			       vote_disabled ? "Fixied" : "Dynamic",
			       vote_type_names[last_vote[i].vote_type],
			       vote_cmd_names[last_vote[i].vote_cmd],
			       last_vote[i].value);
		idx += len;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", bufinfo);
}

static int sprd_vote_register_sysfs(struct device *dev)
{
	struct sprd_vote_sysfs *sysfs;
	int ret = 0;

	sysfs = devm_kzalloc(dev, sizeof(*sysfs), GFP_KERNEL);
	if (!sysfs)
		return -ENOMEM;

	sysfs->name = "sprd_vote";
	sysfs->attrs[0] = &sysfs->attr_vote_gov.attr;
	sysfs->attrs[1] = &sysfs->attr_vote_info.attr;
	sysfs->attrs[2] = &sysfs->attr_vote_disabled.attr;
	sysfs->attrs[3] = &sysfs->attr_vote_fixed.attr;
	sysfs->attrs[4] = &sysfs->attr_voted_value.attr;
	sysfs->attrs[5] = &sysfs->attr_voted_type.attr;
	sysfs->attrs[6] = NULL;
	sysfs->attr_g.name = "sprd_vote";
	sysfs->attr_g.attrs = sysfs->attrs;

	sysfs_attr_init(&sysfs->attr_vote_gov.attr);
	sysfs->attr_vote_gov.attr.name = "vote_gov";
	sysfs->attr_vote_gov.attr.mode = 0444;
	sysfs->attr_vote_gov.show = vote_gov_show;

	sysfs_attr_init(&sysfs->attr_vote_info.attr);
	sysfs->attr_vote_info.attr.name = "vote_info";
	sysfs->attr_vote_info.attr.mode = 0444;
	sysfs->attr_vote_info.show = vote_info_show;

	sysfs_attr_init(&sysfs->attr_vote_disabled.attr);
	sysfs->attr_vote_disabled.attr.name = "vote_disabled";
	sysfs->attr_vote_disabled.attr.mode = 0644;
	sysfs->attr_vote_disabled.show = vote_disabled_show;
	sysfs->attr_vote_disabled.store = vote_disabled_store;

	sysfs_attr_init(&sysfs->attr_vote_fixed.attr);
	sysfs->attr_vote_fixed.attr.name = "vote_fixed";
	sysfs->attr_vote_fixed.attr.mode = 0200;
	sysfs->attr_vote_fixed.store = vote_fixed_store;

	sysfs_attr_init(&sysfs->attr_voted_value.attr);
	sysfs->attr_voted_value.attr.name = "voted_value";
	sysfs->attr_voted_value.attr.mode = 0444;
	sysfs->attr_voted_value.show = voted_value_show;

	sysfs_attr_init(&sysfs->attr_voted_type.attr);
	sysfs->attr_voted_type.attr.name = "voted_type";
	sysfs->attr_voted_type.attr.mode = 0644;
	sysfs->attr_voted_type.show = voted_type_show;
	sysfs->attr_voted_type.store = voted_type_store;

	ret = sysfs_create_group(&dev->kobj, &sysfs->attr_g);
	if (ret < 0)
		dev_err(dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

struct sprd_vote *sprd_charge_vote_register(char *name,
					    void (*cb)(struct sprd_vote *vote_gov,
						       int vote_type, int value,
						       void *data),
					    void *data,
					    struct device *dev)
{
	struct sprd_vote *vote_gov = NULL;
	int ret;

	vote_gov = devm_kzalloc(dev, sizeof(struct sprd_vote), GFP_KERNEL);
	if (!vote_gov)
		return ERR_PTR(-ENOMEM);

	vote_gov->name = devm_kstrdup(dev, name, GFP_KERNEL);
	if (!vote_gov->name) {
		pr_err("vote_gov: fail to dum name %s\n", name);
		return ERR_PTR(-ENOMEM);
	}

	vote_gov->cb = cb;
	vote_gov->data = data;
	vote_gov->vote = sprd_vote_func;
	g_vote = vote_gov;
	private_data = data;
	mutex_init(&vote_gov->lock);

	if (!dev)
		return vote_gov;

	ret = sprd_vote_register_sysfs(dev);
	if (ret)
		dev_err(dev, "register sysfs fail, ret = %d\n", ret);

	return vote_gov;
}
EXPORT_SYMBOL(sprd_charge_vote_register);
MODULE_LICENSE("GPL");
