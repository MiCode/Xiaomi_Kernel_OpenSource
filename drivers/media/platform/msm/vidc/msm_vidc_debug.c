/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#define CREATE_TRACE_POINTS
#include "msm_vidc_debug.h"
#include "vidc_hfi_api.h"

#define MAX_DBG_BUF_SIZE 4096
int msm_vidc_debug = VIDC_ERR | VIDC_WARN;
int msm_vidc_debug_out = VIDC_OUT_PRINTK;
int msm_fw_debug = 0x18;
int msm_fw_debug_mode = 0x1;
int msm_fw_low_power_mode = 0x1;
int msm_vidc_hw_rsp_timeout = 1000;
u32 msm_fw_coverage = 0x0;
int msm_vidc_vpe_csc_601_to_709 = 0x0;
int msm_vidc_dcvs_mode = 0x1;
int msm_vidc_sys_idle_indicator = 0x0;
u32 msm_vidc_firmware_unload_delay = 15000;

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

#define DYNAMIC_BUF_OWNER(__binfo) ({ \
	atomic_read(&__binfo->ref_count) == 2 ? "video driver" : "firmware";\
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
	struct hfi_device *hdev;
	int i = 0;
	if (!core || !core->device) {
		dprintk(VIDC_ERR, "Invalid params, core: %p\n", core);
		return 0;
	}
	hdev = core->device;
	INIT_DBG_BUF(dbg_buf);
	write_str(&dbg_buf, "===============================\n");
	write_str(&dbg_buf, "CORE %d: 0x%p\n", core->id, core);
	write_str(&dbg_buf, "===============================\n");
	write_str(&dbg_buf, "state: %d\n", core->state);
	write_str(&dbg_buf, "base addr: 0x%x\n",
		call_hfi_op(hdev, get_fw_info, hdev->hfi_device_data,
					FW_BASE_ADDRESS));
	write_str(&dbg_buf, "register_base: 0x%x\n",
		call_hfi_op(hdev, get_fw_info, hdev->hfi_device_data,
					FW_REGISTER_BASE));
	write_str(&dbg_buf, "register_size: %u\n",
		call_hfi_op(hdev, get_fw_info, hdev->hfi_device_data,
					FW_REGISTER_SIZE));
	write_str(&dbg_buf, "irq: %u\n",
		call_hfi_op(hdev, get_fw_info, hdev->hfi_device_data,
					FW_IRQ));
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

static int trigger_ssr_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t trigger_ssr_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos) {
	u32 ssr_trigger_val;
	int rc;
	struct msm_vidc_core *core = filp->private_data;
	rc = sscanf(buf, "%d", &ssr_trigger_val);
	if (rc < 0) {
		dprintk(VIDC_WARN, "returning error err %d\n", rc);
		rc = -EINVAL;
	} else {
		msm_vidc_trigger_ssr(core, ssr_trigger_val);
		rc = count;
	}
	return rc;
}

static const struct file_operations ssr_fops = {
	.open = trigger_ssr_open,
	.write = trigger_ssr_write,
};

struct dentry *msm_vidc_debugfs_init_drv(void)
{
	struct dentry *dir = debugfs_create_dir("msm_vidc", NULL);
	if (IS_ERR_OR_NULL(dir)) {
		dir = NULL;
		goto failed_create_dir;
	}

	if (!debugfs_create_u32("debug_level", S_IRUGO | S_IWUSR,
			dir, &msm_vidc_debug)) {
		dprintk(VIDC_ERR, "debugfs_create_file: fail\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_u32("fw_level", S_IRUGO | S_IWUSR,
			dir, &msm_fw_debug)) {
		dprintk(VIDC_ERR, "debugfs_create_file: fail\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_u32("fw_debug_mode", S_IRUGO | S_IWUSR,
			dir, &msm_fw_debug_mode)) {
		dprintk(VIDC_ERR, "debugfs_create_file: fail\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_u32("fw_coverage", S_IRUGO | S_IWUSR,
			dir, &msm_fw_coverage)) {
		dprintk(VIDC_WARN, "debugfs_create_file fw_coverage: fail\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_u32("dcvs_mode", S_IRUGO | S_IWUSR,
			dir, &msm_vidc_dcvs_mode)) {
		dprintk(VIDC_WARN, "debugfs_create_file dcvs_mode: fail\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_u32("fw_low_power_mode", S_IRUGO | S_IWUSR,
			dir, &msm_fw_low_power_mode)) {
		dprintk(VIDC_ERR, "debugfs_create_file: fail\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_u32("debug_output", S_IRUGO | S_IWUSR,
			dir, &msm_vidc_debug_out)) {
		dprintk(VIDC_ERR, "debugfs_create_file: fail\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_u32("hw_rsp_timeout", S_IRUGO | S_IWUSR,
			dir, &msm_vidc_hw_rsp_timeout)) {
		dprintk(VIDC_ERR, "debugfs_create_file: fail\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_bool("enable_vpe_csc_601_709", S_IRUGO | S_IWUSR,
			dir, &msm_vidc_vpe_csc_601_to_709)) {
		dprintk(VIDC_ERR, "debugfs_create_file: fail\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_bool("sys_idle_indicator", S_IRUGO | S_IWUSR,
			dir, &msm_vidc_sys_idle_indicator)) {
		dprintk(VIDC_ERR,
			"debugfs_create_file: sys_idle_indicator fail\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_u32("firmware_unload_delay", S_IRUGO | S_IWUSR,
			dir, &msm_vidc_firmware_unload_delay)) {
		dprintk(VIDC_ERR,
			"debugfs_create_file: firmware_unload_delay fail\n");
		goto failed_create_dir;
	}
	return dir;

failed_create_dir:
	if (dir)
		debugfs_remove_recursive(vidc_driver->debugfs_root);

	return NULL;
}

struct dentry *msm_vidc_debugfs_init_core(struct msm_vidc_core *core,
		struct dentry *parent)
{
	struct dentry *dir = NULL;
	char debugfs_name[MAX_DEBUGFS_NAME];
	if (!core) {
		dprintk(VIDC_ERR, "Invalid params, core: %p\n", core);
		goto failed_create_dir;
	}

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
	if (!debugfs_create_file("trigger_ssr", S_IWUSR,
			dir, core, &ssr_fops)) {
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

static int publish_unreleased_reference(struct msm_vidc_inst *inst)
{
	struct buffer_info *temp = NULL;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid param\n", __func__);
		return -EINVAL;
	}

	if (inst->buffer_mode_set[CAPTURE_PORT] == HAL_BUFFER_MODE_DYNAMIC) {
		write_str(&dbg_buf, "Pending buffer references:\n");

		mutex_lock(&inst->registeredbufs.lock);
		list_for_each_entry(temp, &inst->registeredbufs.list, list) {
			if (temp->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
			!temp->inactive && atomic_read(&temp->ref_count)) {
				write_str(&dbg_buf,
				"\tpending buffer: 0x%lx fd[0] = %d ref_count = %d held by: %s\n",
				temp->device_addr[0],
				temp->fd[0],
				atomic_read(&temp->ref_count),
				DYNAMIC_BUF_OWNER(temp));
			}
		}
		mutex_unlock(&inst->registeredbufs.lock);
	}
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
	write_str(&dbg_buf, "height: %d\n", inst->prop.height[CAPTURE_PORT]);
	write_str(&dbg_buf, "width: %d\n", inst->prop.width[CAPTURE_PORT]);
	write_str(&dbg_buf, "fps: %d\n", inst->prop.fps);
	write_str(&dbg_buf, "state: %d\n", inst->state);
	write_str(&dbg_buf, "secure: %d\n", !!(inst->flags & VIDC_SECURE));
	write_str(&dbg_buf, "-----------Formats-------------\n");
	for (i = 0; i < MAX_PORT_NUM; i++) {
		write_str(&dbg_buf, "capability: %s\n", i == OUTPUT_PORT ?
			"Output" : "Capture");
		write_str(&dbg_buf, "name : %s\n", inst->fmts[i]->name);
		write_str(&dbg_buf, "planes : %d\n", inst->fmts[i]->num_planes);
		write_str(
		&dbg_buf, "type: %s\n", inst->fmts[i]->type == OUTPUT_PORT ?
		"Output" : "Capture");
		switch (inst->buffer_mode_set[i]) {
		case HAL_BUFFER_MODE_STATIC:
			write_str(&dbg_buf, "buffer mode : %s\n", "static");
			break;
		case HAL_BUFFER_MODE_RING:
			write_str(&dbg_buf, "buffer mode : %s\n", "ring");
			break;
		case HAL_BUFFER_MODE_DYNAMIC:
			write_str(&dbg_buf, "buffer mode : %s\n", "dynamic");
			break;
		default:
			write_str(&dbg_buf, "buffer mode : unsupported\n");
		}

		write_str(&dbg_buf, "count: %u\n",
				inst->bufq[i].vb2_bufq.num_buffers);

		for (j = 0; j < inst->fmts[i]->num_planes; j++)
			write_str(&dbg_buf, "size for plane %d: %u\n", j,
			inst->bufq[i].vb2_bufq.plane_sizes[j]);

		if (i < MAX_PORT_NUM - 1)
			write_str(&dbg_buf, "\n");
	}
	write_str(&dbg_buf, "-------------------------------\n");
	for (i = SESSION_MSG_START; i < SESSION_MSG_END; i++) {
		write_str(&dbg_buf, "completions[%d]: %s\n", i,
		completion_done(&inst->completions[SESSION_MSG_INDEX(i)]) ?
		"pending" : "done");
	}
	write_str(&dbg_buf, "ETB Count: %d\n", inst->count.etb);
	write_str(&dbg_buf, "EBD Count: %d\n", inst->count.ebd);
	write_str(&dbg_buf, "FTB Count: %d\n", inst->count.ftb);
	write_str(&dbg_buf, "FBD Count: %d\n", inst->count.fbd);

	publish_unreleased_reference(inst);

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
	inst->debug.pdata[FRAME_PROCESSING].sampling = true;
failed_create_dir:
	return dir;
}

void msm_vidc_debugfs_update(struct msm_vidc_inst *inst,
	enum msm_vidc_debugfs_event e)
{
	struct msm_vidc_debug *d = &inst->debug;
	char a[64] = "Frame processing";
	switch (e) {
	case MSM_VIDC_DEBUGFS_EVENT_ETB:
		inst->count.etb++;
		if (inst->count.ebd && inst->count.ftb > inst->count.fbd) {
			d->pdata[FRAME_PROCESSING].name[0] = '\0';
			tic(inst, FRAME_PROCESSING, a);
		}
	break;
	case MSM_VIDC_DEBUGFS_EVENT_EBD:
		inst->count.ebd++;
		if (inst->count.ebd && inst->count.ebd == inst->count.etb) {
			toc(inst, FRAME_PROCESSING);
			dprintk(VIDC_PROF, "EBD: FW needs input buffers\n");
		}
		if (inst->count.ftb == inst->count.fbd)
			dprintk(VIDC_PROF, "EBD: FW needs output buffers\n");
	break;
	case MSM_VIDC_DEBUGFS_EVENT_FTB: {
		inst->count.ftb++;
		if (inst->count.ebd && inst->count.etb > inst->count.ebd) {
			d->pdata[FRAME_PROCESSING].name[0] = '\0';
			tic(inst, FRAME_PROCESSING, a);
		}
	}
	break;
	case MSM_VIDC_DEBUGFS_EVENT_FBD:
		inst->debug.samples++;
		if (inst->count.ebd && inst->count.fbd == inst->count.ftb) {
			toc(inst, FRAME_PROCESSING);
			dprintk(VIDC_PROF, "FBD: FW needs output buffers\n");
		}
		if (inst->count.etb == inst->count.ebd)
			dprintk(VIDC_PROF, "FBD: FW needs input buffers\n");
		break;
	default:
		dprintk(VIDC_ERR, "Invalid state in debugfs: %d\n", e);
		break;
	}
}

