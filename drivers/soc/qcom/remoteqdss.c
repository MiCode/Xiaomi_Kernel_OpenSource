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
#include <linux/dma-mapping.h>

#define REMOTEQDSS_FLAG_QUIET (BIT(0))

static unsigned long remoteqdss_dbg_flags;
module_param_named(dbg_flags, remoteqdss_dbg_flags, ulong, 0644);

static struct dentry *remoteqdss_dir;

#define REMOTEQDSS_ERR(fmt, ...) \
	pr_debug("%s: " fmt, __func__, ## __VA_ARGS__)

#define REMOTEQDSS_ERR_CALLER(fmt, caller, ...) \
	pr_debug("%pf: " fmt, caller, ## __VA_ARGS__)

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
	uint32_t id;
	uint32_t sw_entity_group;
	uint32_t sw_event_group;
	struct dentry *dir;
};

static struct device dma_dev;

/* Allowed message formats */

enum remoteqdss_cmd_id {
	CMD_ID_QUERY_SWEVENT_TAG,
	CMD_ID_FILTER_SWTRACE_STATE,
	CMD_ID_QUERY_SWTRACE_STATE,
	CMD_ID_FILTER_SWEVENT,
	CMD_ID_QUERY_SWEVENT,
	CMD_ID_FILTER_SWENTITY,
	CMD_ID_QUERY_SWENTITY,
};

struct remoteqdss_header_fmt {
	uint32_t subsys_id;
	uint32_t cmd_id;
};

struct remoteqdss_filter_swtrace_state_fmt {
	struct remoteqdss_header_fmt h;
	uint32_t state;
};

struct remoteqdss_filter_swevent_fmt {
	struct remoteqdss_header_fmt h;
	uint32_t event_group;
	uint32_t event_mask;
};

struct remoteqdss_query_swevent_fmt {
	struct remoteqdss_header_fmt h;
	uint32_t event_group;
};

struct remoteqdss_filter_swentity_fmt {
	struct remoteqdss_header_fmt h;
	uint32_t entity_group;
	uint32_t entity_mask;
};

struct remoteqdss_query_swentity_fmt {
	struct remoteqdss_header_fmt h;
	uint32_t entity_group;
};

/* msgs is a null terminated array */
static void remoteqdss_err_translation(struct qdss_msg_translation *msgs,
					u64 err, const void *caller)
{
	static DEFINE_RATELIMIT_STATE(rl, 5 * HZ, 2);
	struct qdss_msg_translation *msg;

	if (!err)
		return;

	if (remoteqdss_dbg_flags & REMOTEQDSS_FLAG_QUIET)
		return;

	for (msg = msgs; msg->msg; msg++) {
		if (err == msg->val && __ratelimit(&rl)) {
			REMOTEQDSS_ERR_CALLER("0x%llx: %s\n", caller, err,
						msg->msg);
			return;
		}
	}

	REMOTEQDSS_ERR_CALLER("Error 0x%llx\n", caller, err);
}

/* Shared across all remoteqdss scm functions */
#define SCM_CMD_ID (0x1)

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

static int remoteqdss_do_scm_call(struct scm_desc *desc,
		dma_addr_t addr, size_t size, const void *caller)
{
	int ret;

	memset(desc, 0, sizeof(*desc));
	desc->args[0] = dma_to_phys(NULL, addr);
	desc->args[1] = size;
	desc->arginfo = SCM_ARGS(2, SCM_RO, SCM_VAL);

	ret = scm_call2(
		SCM_SIP_FNID(SCM_SVC_QDSS, SCM_CMD_ID),
		desc);
	if (ret)
		return ret;

	remoteqdss_err_translation(remoteqdss_scm_msgs, desc->ret[0], caller);
	ret = desc->ret[0] ? -EINVAL : 0;
	return ret;
}

static int remoteqdss_scm_query_swtrace(void *priv, u64 *val)
{
	struct remoteqdss_data *data = priv;
	int ret;
	struct scm_desc desc;
	struct remoteqdss_header_fmt *fmt;
	dma_addr_t addr;

	fmt = dma_alloc_coherent(&dma_dev, sizeof(*fmt), &addr, GFP_KERNEL);
	if (!fmt)
		return -ENOMEM;
	fmt->subsys_id = data->id;
	fmt->cmd_id = CMD_ID_QUERY_SWTRACE_STATE;

	ret = remoteqdss_do_scm_call(&desc, addr, sizeof(*fmt),
					__builtin_return_address(0));
	*val = desc.ret[1];

	dma_free_coherent(&dma_dev, sizeof(*fmt), fmt, addr);
	return ret;
}

static int remoteqdss_scm_filter_swtrace(void *priv, u64 val)
{
	struct remoteqdss_data *data = priv;
	int ret;
	struct scm_desc desc;
	struct remoteqdss_filter_swtrace_state_fmt *fmt;
	dma_addr_t addr;

	fmt = dma_alloc_coherent(&dma_dev, sizeof(*fmt), &addr, GFP_KERNEL);
	if (!fmt)
		return -ENOMEM;
	fmt->h.subsys_id = data->id;
	fmt->h.cmd_id = CMD_ID_FILTER_SWTRACE_STATE;
	fmt->state = (uint32_t)val;

	ret = remoteqdss_do_scm_call(&desc, addr, sizeof(*fmt),
					__builtin_return_address(0));

	dma_free_coherent(&dma_dev, sizeof(*fmt), fmt, addr);
	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_sw_trace_output,
			remoteqdss_scm_query_swtrace,
			remoteqdss_scm_filter_swtrace,
			"0x%llx\n");

static int remoteqdss_scm_query_tag(void *priv, u64 *val)
{
	struct remoteqdss_data *data = priv;
	int ret;
	struct scm_desc desc;
	struct remoteqdss_header_fmt *fmt;
	dma_addr_t addr;

	fmt = dma_alloc_coherent(&dma_dev, sizeof(*fmt), &addr, GFP_KERNEL);
	if (!fmt)
		return -ENOMEM;
	fmt->subsys_id = data->id;
	fmt->cmd_id = CMD_ID_QUERY_SWEVENT_TAG;

	ret = remoteqdss_do_scm_call(&desc, addr, sizeof(*fmt),
					__builtin_return_address(0));
	*val = desc.ret[1];

	dma_free_coherent(&dma_dev, sizeof(*fmt), fmt, addr);
	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_tag,
			remoteqdss_scm_query_tag,
			NULL,
			"0x%llx\n");

static int remoteqdss_scm_query_swevent(void *priv, u64 *val)
{
	struct remoteqdss_data *data = priv;
	int ret;
	struct scm_desc desc;
	struct remoteqdss_query_swevent_fmt *fmt;
	dma_addr_t addr;

	fmt = dma_alloc_coherent(&dma_dev, sizeof(*fmt), &addr, GFP_KERNEL);
	if (!fmt)
		return -ENOMEM;
	fmt->h.subsys_id = data->id;
	fmt->h.cmd_id = CMD_ID_QUERY_SWEVENT;
	fmt->event_group = data->sw_event_group;

	ret = remoteqdss_do_scm_call(&desc, addr, sizeof(*fmt),
					__builtin_return_address(0));
	*val = desc.ret[1];

	dma_free_coherent(&dma_dev, sizeof(*fmt), fmt, addr);
	return ret;
}

static int remoteqdss_scm_filter_swevent(void *priv, u64 val)
{
	struct remoteqdss_data *data = priv;
	int ret;
	struct scm_desc desc;
	struct remoteqdss_filter_swevent_fmt *fmt;
	dma_addr_t addr;

	fmt = dma_alloc_coherent(&dma_dev, sizeof(*fmt), &addr, GFP_KERNEL);
	if (!fmt)
		return -ENOMEM;
	fmt->h.subsys_id = data->id;
	fmt->h.cmd_id = CMD_ID_FILTER_SWEVENT;
	fmt->event_group = data->sw_event_group;
	fmt->event_mask = (uint32_t)val;

	ret = remoteqdss_do_scm_call(&desc, addr, sizeof(*fmt),
					__builtin_return_address(0));

	dma_free_coherent(&dma_dev, sizeof(*fmt), fmt, addr);
	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_swevent,
			remoteqdss_scm_query_swevent,
			remoteqdss_scm_filter_swevent,
			"0x%llx\n");

static int remoteqdss_scm_query_swentity(void *priv, u64 *val)
{
	struct remoteqdss_data *data = priv;
	int ret;
	struct scm_desc desc;
	struct remoteqdss_query_swentity_fmt *fmt;
	dma_addr_t addr;

	fmt = dma_alloc_coherent(&dma_dev, sizeof(*fmt), &addr, GFP_KERNEL);
	if (!fmt)
		return -ENOMEM;
	fmt->h.subsys_id = data->id;
	fmt->h.cmd_id = CMD_ID_QUERY_SWENTITY;
	fmt->entity_group = data->sw_entity_group;

	ret = remoteqdss_do_scm_call(&desc, addr, sizeof(*fmt),
					__builtin_return_address(0));
	*val = desc.ret[1];

	dma_free_coherent(&dma_dev, sizeof(*fmt), fmt, addr);
	return ret;
}

static int remoteqdss_scm_filter_swentity(void *priv, u64 val)
{
	struct remoteqdss_data *data = priv;
	int ret;
	struct scm_desc desc;
	struct remoteqdss_filter_swentity_fmt *fmt;
	dma_addr_t addr;

	fmt = dma_alloc_coherent(&dma_dev, sizeof(*fmt), &addr, GFP_KERNEL);
	if (!fmt)
		return -ENOMEM;
	fmt->h.subsys_id = data->id;
	fmt->h.cmd_id = CMD_ID_FILTER_SWENTITY;
	fmt->entity_group = data->sw_entity_group;
	fmt->entity_mask = (uint32_t)val;

	ret = remoteqdss_do_scm_call(&desc, addr, sizeof(*fmt),
					__builtin_return_address(0));

	dma_free_coherent(&dma_dev, sizeof(*fmt), fmt, addr);
	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_swentity,
			remoteqdss_scm_query_swentity,
			remoteqdss_scm_filter_swentity,
			"0x%llx\n");

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

	dentry = debugfs_create_u32("sw_entity_group", S_IRUGO | S_IWUSR,
			data->dir, &data->sw_entity_group);
	if (IS_ERR_OR_NULL(dentry))
		goto out;

	dentry = debugfs_create_u32("sw_event_group", S_IRUGO | S_IWUSR,
			data->dir, &data->sw_event_group);
	if (IS_ERR_OR_NULL(dentry))
		goto out;

	dentry = debugfs_create_file("tag", S_IRUGO,
			data->dir, data, &fops_tag);
	if (IS_ERR_OR_NULL(dentry))
		goto out;

	dentry = debugfs_create_file("swevent", S_IRUGO | S_IWUSR,
			data->dir, data, &fops_swevent);
	if (IS_ERR_OR_NULL(dentry))
		goto out;

	dentry = debugfs_create_file("swentity", S_IRUGO | S_IWUSR,
			data->dir, data, &fops_swentity);
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
	int ret;

	/* Set up DMA */
	arch_setup_dma_ops(&dma_dev, 0, U64_MAX, NULL, false);
	ret = dma_coerce_mask_and_coherent(&dma_dev, DMA_BIT_MASK(64));
	if (ret)
		return ret;

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
late_initcall(remoteqdss_init);
