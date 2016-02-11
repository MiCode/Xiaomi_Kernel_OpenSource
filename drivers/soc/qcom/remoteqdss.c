/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <soc/qcom/scm.h>
#include <linux/debugfs.h>
#include <linux/ratelimit.h>

#define REMOTEQDSS_FLAG_QUIET (BIT(0))

static unsigned long remoteqdss_dbg_flags;
module_param_named(dbg_flags, remoteqdss_dbg_flags, ulong, 0644);

static struct dentry *remoteqdss_dir;

#define REMOTEQDSS_ERR(fmt, ...) \
	pr_err("%s: " fmt, __func__, ## __VA_ARGS__)

#define REMOTEQDSS_ERR_CALLER(fmt, ...) \
	pr_err("%pf: " fmt, __builtin_return_address(0), ## __VA_ARGS__)

struct qdss_msg_translation {
	u64 val;
	char *msg;
};

/*
 * id			Unique identifier
 * sw_entity_group	Array index
 * sw_event_group	Array index
 * dir			Parent debugfs directory
 */
struct remoteqdss_data {
	u8 id;
	u64 sw_entity_group;
	u64 sw_event_group;
	struct dentry *dir;
};

/* msgs is a null terminated array */
static void remoteqdss_err_translation(struct qdss_msg_translation *msgs,
								u64 err)
{
	static DEFINE_RATELIMIT_STATE(rl, 5 * HZ, 2);
	struct qdss_msg_translation *msg;

	if (!err)
		return;

	if (remoteqdss_dbg_flags & REMOTEQDSS_FLAG_QUIET)
		return;

	for (msg = msgs; msg->msg; msg++) {
		if (err == msg->val && __ratelimit(&rl)) {
			REMOTEQDSS_ERR_CALLER("0x%llx: %s\n", err, msg->msg);
			return;
		}
	}

	REMOTEQDSS_ERR_CALLER("Error 0x%llx\n", err);
}

/* SCM based devices */
#define SCM_FILTER_SWTRACE_ID (0x1)
#define SCM_QUERY_SWTRACE_ID  (0x2)

/* Response Values */
#define SCM_CMD_FAIL		(0x80)
#define SCM_QDSS_UNAVAILABLE	(0x81)
#define SCM_UNINITIALIZED	(0x82)
#define SCM_BAD_ARG		(0x83)
#define SCM_BAD_SUBSYS		(0x85)

static struct qdss_msg_translation remoteqdss_scm_msgs[] = {
	{SCM_CMD_FAIL,
		"Command failed"},
	{SCM_QDSS_UNAVAILABLE,
		"QDSS not available or cannot turn QDSS (clock) on"},
	{SCM_UNINITIALIZED,
		"Tracer not initialized or unable to initialize"},
	{SCM_BAD_ARG,
		"Invalid parameter value"},
	{SCM_BAD_SUBSYS,
		"Incorrect subsys ID"},
	{}
};

static struct remoteqdss_data *create_remoteqdss_data(u32 id)
{
	struct remoteqdss_data *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return NULL;

	data->id = id;
	return data;
}

static void free_remoteqdss_data(struct remoteqdss_data *data)
{
	kfree(data);
}

static int remoteqdss_scm_query_swtrace(void *priv, u64 *val)
{
	struct remoteqdss_data *data = priv;
	int ret;
	struct scm_desc desc;

	memset(&desc, 0, sizeof(desc));
	desc.args[0] = data->id;
	desc.arginfo = SCM_ARGS(1, SCM_VAL);

	ret = scm_call2(
		SCM_SIP_FNID(SCM_SVC_QDSS, SCM_QUERY_SWTRACE_ID),
		&desc);
	if (ret)
		return ret;

	remoteqdss_err_translation(remoteqdss_scm_msgs, desc.ret[0]);
	ret = desc.ret[0] ? -EINVAL : 0;
	*val = desc.ret[1];
	return ret;
}

static int remoteqdss_scm_filter_swtrace(void *priv, u64 val)
{
	struct remoteqdss_data *data = priv;
	int ret;
	struct scm_desc desc;

	memset(&desc, 0, sizeof(desc));
	desc.args[0] = data->id;
	desc.args[1] = val;
	desc.arginfo = SCM_ARGS(2, SCM_VAL, SCM_VAL);

	ret = scm_call2(
		SCM_SIP_FNID(SCM_SVC_QDSS, SCM_FILTER_SWTRACE_ID),
		&desc);
	if (ret)
		return ret;

	remoteqdss_err_translation(remoteqdss_scm_msgs, desc.ret[0]);
	ret = desc.ret[0] ? -EINVAL : 0;
	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(fops_sw_trace_output,
			remoteqdss_scm_query_swtrace,
			remoteqdss_scm_filter_swtrace,
			"%llu\n");

static void __init enumerate_scm_devices(struct dentry *parent)
{
	u64 unused;
	int ret;
	struct remoteqdss_data *data;
	struct dentry *dentry;

	if (!is_scm_armv8())
		return;

	data = create_remoteqdss_data(0);
	if (!data)
		return;

	/* Assume failure means device not present */
	ret = remoteqdss_scm_query_swtrace(data, &unused);
	if (ret)
		goto out;

	data->dir = debugfs_create_dir("tz", parent);
	if (IS_ERR_OR_NULL(data->dir))
		goto out;

	dentry = debugfs_create_file("sw_trace_output", S_IRUGO | S_IWUSR,
			data->dir, data, &fops_sw_trace_output);
	if (IS_ERR_OR_NULL(dentry))
		goto out;

	dentry = debugfs_create_u64("sw_entity_group", S_IRUGO | S_IWUSR,
			data->dir, &data->sw_entity_group);
	if (IS_ERR_OR_NULL(dentry))
		goto out;

	dentry = debugfs_create_u64("sw_event_group", S_IRUGO | S_IWUSR,
			data->dir, &data->sw_event_group);
	if (IS_ERR_OR_NULL(dentry))
		goto out;

	return;

out:
	debugfs_remove_recursive(data->dir);
	free_remoteqdss_data(data);
}

static int __init remoteqdss_init(void)
{
	unsigned long old_flags = remoteqdss_dbg_flags;

	/*
	 * disable normal error messages while checking
	 * if support is present.
	 */
	remoteqdss_dbg_flags |= REMOTEQDSS_FLAG_QUIET;

	remoteqdss_dir = debugfs_create_dir("remoteqdss", NULL);
	if (!remoteqdss_dir)
		return 0;

	enumerate_scm_devices(remoteqdss_dir);

	remoteqdss_dbg_flags = old_flags;
	return 0;
}
module_init(remoteqdss_init);
