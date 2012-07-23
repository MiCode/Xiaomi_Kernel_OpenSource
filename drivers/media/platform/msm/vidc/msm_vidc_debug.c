/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_vidc_debug.h"

#define MAX_DBG_BUF_SIZE 4096
int msm_vidc_debug;

struct debug_buffer {
	char ptr[MAX_DBG_BUF_SIZE];
	char *curr;
	u32 filled_size;
};

static struct debug_buffer dbg_buf;

#define INIT_DBG_BUF(__buf) ({ \
	__buf.curr = __buf.ptr;\
	__buf.filled_size = 0; \
})

static int core_info_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static u32 write_str(struct debug_buffer *buffer, const char *fmt, ...)
{
	va_list args;
	u32 size;
	va_start(args, fmt);
	size = vscnprintf(buffer->curr, MAX_DBG_BUF_SIZE - 1, fmt, args);
	va_end(args);
	buffer->curr += size;
	buffer->filled_size += size;
	return size;
}

static ssize_t core_info_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct msm_vidc_core *core = file->private_data;
	int i = 0;
	if (!core) {
		dprintk(VIDC_ERR, "Invalid params, core: %p\n", core);
		return 0;
	}
	INIT_DBG_BUF(dbg_buf);
	write_str(&dbg_buf, "===============================\n");
	write_str(&dbg_buf, "CORE %d: 0x%p\n", core->id, core);
	write_str(&dbg_buf, "===============================\n");
	write_str(&dbg_buf, "state: %d\n", core->state);
	write_str(&dbg_buf, "base addr: 0x%x\n", core->base_addr);
	write_str(&dbg_buf, "register_base: 0x%x\n", core->register_base);
	write_str(&dbg_buf, "register_size: %u\n", core->register_size);
	write_str(&dbg_buf, "irq: %u\n", core->irq);
	for (i = SYS_MSG_START; i < SYS_MSG_END; i++) {
		write_str(&dbg_buf, "completions[%d]: %s\n", i,
			completion_done(&core->completions[SYS_MSG_INDEX(i)]) ?
			"pending" : "done");
	}
	return simple_read_from_buffer(buf, count, ppos,
			dbg_buf.ptr, dbg_buf.filled_size);
}

static const struct file_operations core_info_fops = {
	.open = core_info_open,
	.read = core_info_read,
};

struct dentry *msm_vidc_debugfs_init_core(struct msm_vidc_core *core,
		struct dentry *parent)
{
	struct dentry *dir = NULL;
	char debugfs_name[MAX_DEBUGFS_NAME];
	if (!core) {
		dprintk(VIDC_ERR, "Invalid params, core: %p\n", core);
		goto failed_create_dir;
	}
	msm_vidc_debug = 0;
	snprintf(debugfs_name, MAX_DEBUGFS_NAME, "core%d", core->id);
	dir = debugfs_create_dir(debugfs_name, parent);
	if (!dir) {
		dprintk(VIDC_ERR, "Failed to create debugfs for msm_vidc\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_file("info", S_IRUGO, dir, core, &core_info_fops)) {
		dprintk(VIDC_ERR, "debugfs_create_file: fail\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_u32("debug_level", S_IRUGO | S_IWUSR,
			parent,	&msm_vidc_debug)) {
		dprintk(VIDC_ERR, "debugfs_create_file: fail\n");
		goto failed_create_dir;
	}
failed_create_dir:
	return dir;
}

static int inst_info_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t inst_info_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct msm_vidc_inst *inst = file->private_data;
	int i, j;
	if (!inst) {
		dprintk(VIDC_ERR, "Invalid params, core: %p\n", inst);
		return 0;
	}
	INIT_DBG_BUF(dbg_buf);
	write_str(&dbg_buf, "===============================\n");
	write_str(&dbg_buf, "INSTANCE: 0x%p (%s)\n", inst,
		inst->session_type == MSM_VIDC_ENCODER ? "Encoder" : "Decoder");
	write_str(&dbg_buf, "===============================\n");
	write_str(&dbg_buf, "core: 0x%p\n", inst->core);
	write_str(&dbg_buf, "height: %d\n", inst->prop.height);
	write_str(&dbg_buf, "width: %d\n", inst->prop.width);
	write_str(&dbg_buf, "state: %d\n", inst->state);
	write_str(&dbg_buf, "-----------Formats-------------\n");
	for (i = 0; i < MAX_PORT_NUM; i++) {
		write_str(&dbg_buf, "capability: %s\n", i == OUTPUT_PORT ?
			"Output" : "Capture");
		write_str(&dbg_buf, "name : %s\n", inst->fmts[i]->name);
		write_str(&dbg_buf, "planes : %d\n", inst->fmts[i]->num_planes);
		write_str(
		&dbg_buf, "type: %s\n", inst->fmts[i]->type == OUTPUT_PORT ?
		"Output" : "Capture");
		for (j = 0; j < inst->fmts[i]->num_planes; j++)
			write_str(&dbg_buf, "size for plane %d: %u\n", j,
			inst->fmts[i]->get_frame_size(j,
			inst->prop.height, inst->prop.width));
	}
	write_str(&dbg_buf, "-------------------------------\n");
	for (i = SESSION_MSG_START; i < SESSION_MSG_END; i++) {
		write_str(&dbg_buf, "completions[%d]: %s\n", i,
		completion_done(&inst->completions[SESSION_MSG_INDEX(i)]) ?
		"pending" : "done");
	}
	return simple_read_from_buffer(buf, count, ppos,
		dbg_buf.ptr, dbg_buf.filled_size);
}

static const struct file_operations inst_info_fops = {
	.open = inst_info_open,
	.read = inst_info_read,
};

struct dentry *msm_vidc_debugfs_init_inst(struct msm_vidc_inst *inst,
		struct dentry *parent)
{
	struct dentry *dir = NULL;
	char debugfs_name[MAX_DEBUGFS_NAME];
	if (!inst) {
		dprintk(VIDC_ERR, "Invalid params, inst: %p\n", inst);
		goto failed_create_dir;
	}
	snprintf(debugfs_name, MAX_DEBUGFS_NAME, "inst_%p", inst);
	dir = debugfs_create_dir(debugfs_name, parent);
	if (!dir) {
		dprintk(VIDC_ERR, "Failed to create debugfs for msm_vidc\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_file("info", S_IRUGO, dir, inst, &inst_info_fops)) {
		dprintk(VIDC_ERR, "debugfs_create_file: fail\n");
		goto failed_create_dir;
	}

failed_create_dir:
	return dir;
}
