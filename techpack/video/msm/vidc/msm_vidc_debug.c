// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#define CREATE_TRACE_POINTS
#define MAX_SSR_STRING_LEN 10
#define MAX_DEBUG_LEVEL_STRING_LEN 15
#include "msm_vidc_debug.h"
#include "vidc_hfi_api.h"
#include <linux/of_fdt.h>

int msm_vidc_debug = VIDC_ERR | VIDC_PRINTK |
	FW_ERROR | FW_FATAL | FW_FTRACE;
EXPORT_SYMBOL(msm_vidc_debug);

bool msm_vidc_lossless_encode = !true;
EXPORT_SYMBOL(msm_vidc_lossless_encode);

int msm_vidc_fw_debug_mode = HFI_DEBUG_MODE_QUEUE;
bool msm_vidc_fw_coverage = !true;
bool msm_vidc_thermal_mitigation_disabled = !true;
int msm_vidc_clock_voting = !1;
bool msm_vidc_syscache_disable = !true;
bool msm_vidc_cvp_usage = true;
int msm_vidc_err_recovery_disable = !1;

#define MAX_DBG_BUF_SIZE 4096

#define DYNAMIC_BUF_OWNER(__binfo) ({ \
	atomic_read(&__binfo->ref_count) >= 2 ? "video driver" : "firmware";\
})

static struct log_cookie ctxt[MAX_SUPPORTED_INSTANCES];

struct core_inst_pair {
	struct msm_vidc_core *core;
	struct msm_vidc_inst *inst;
};

static u32 write_str(char *buffer,
		size_t size, const char *fmt, ...)
{
	va_list args;
	u32 len;

	va_start(args, fmt);
	len = vscnprintf(buffer, size, fmt, args);
	va_end(args);
	return len;
}

static ssize_t core_info_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct msm_vidc_core *core = file->private_data;
	struct hfi_device *hdev;
	struct hal_fw_info fw_info = { {0} };
	char *dbuf, *cur, *end;
	int i = 0, rc = 0;
	ssize_t len = 0;

	if (!core || !core->device) {
		d_vpr_e("%s: invalid params %pK\n", __func__, core);
		return 0;
	}

	dbuf = kzalloc(MAX_DBG_BUF_SIZE, GFP_KERNEL);
	if (!dbuf) {
		d_vpr_e("%s: Allocation failed!\n", __func__);
		return -ENOMEM;
	}
	cur = dbuf;
	end = cur + MAX_DBG_BUF_SIZE;
	hdev = core->device;

	cur += write_str(cur, end - cur, "===============================\n");
	cur += write_str(cur, end - cur, "CORE %d: %pK\n", core->id, core);
	cur += write_str(cur, end - cur, "===============================\n");
	cur += write_str(cur, end - cur, "Core state: %d\n", core->state);
	rc = call_hfi_op(hdev, get_fw_info, hdev->hfi_device_data, &fw_info);
	if (rc) {
		d_vpr_e("Failed to read FW info\n");
		goto err_fw_info;
	}

	cur += write_str(cur, end - cur,
		"FW version : %s\n", &fw_info.version);
	cur += write_str(cur, end - cur,
		"base addr: 0x%x\n", fw_info.base_addr);
	cur += write_str(cur, end - cur,
		"register_base: 0x%x\n", fw_info.register_base);
	cur += write_str(cur, end - cur,
		"register_size: %u\n", fw_info.register_size);
	cur += write_str(cur, end - cur, "irq: %u\n", fw_info.irq);
	cur += write_str(cur, end - cur,
		"ddr_type: %d\n", of_fdt_get_ddrtype());

err_fw_info:
	for (i = SYS_MSG_START; i < SYS_MSG_END; i++) {
		cur += write_str(cur, end - cur, "completions[%d]: %s\n", i,
			completion_done(&core->completions[SYS_MSG_INDEX(i)]) ?
			"pending" : "done");
	}
	len = simple_read_from_buffer(buf, count, ppos,
			dbuf, cur - dbuf);

	kfree(dbuf);
	return len;
}

static const struct file_operations core_info_fops = {
	.open = simple_open,
	.read = core_info_read,
};

static ssize_t trigger_ssr_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	unsigned long ssr_trigger_val = 0;
	int rc = 0;
	struct msm_vidc_core *core = filp->private_data;
	size_t size = MAX_SSR_STRING_LEN;
	char kbuf[MAX_SSR_STRING_LEN + 1] = {0};

	if (!buf)
		return -EINVAL;

	if (!count)
		goto exit;

	if (count < size)
		size = count;

	if (copy_from_user(kbuf, buf, size)) {
		d_vpr_e("%s: User memory fault\n", __func__);
		rc = -EFAULT;
		goto exit;
	}

	rc = kstrtoul(kbuf, 0, &ssr_trigger_val);
	if (rc) {
		d_vpr_e("returning error err %d\n", rc);
		rc = -EINVAL;
	} else {
		msm_vidc_trigger_ssr(core, ssr_trigger_val);
		rc = count;
	}
exit:
	return rc;
}

static const struct file_operations ssr_fops = {
	.open = simple_open,
	.write = trigger_ssr_write,
};

static ssize_t debug_level_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *ppos)
{
	int rc = 0;
	struct msm_vidc_core *core = filp->private_data;
	char kbuf[MAX_DEBUG_LEVEL_STRING_LEN] = {0};

	/* filter partial writes and invalid commands */
	if (*ppos != 0 || count >= sizeof(kbuf) || count == 0) {
		d_vpr_e("returning error - pos %d, count %d\n", *ppos, count);
		rc = -EINVAL;
	}

	rc = simple_write_to_buffer(kbuf, sizeof(kbuf) - 1, ppos, buf, count);
	if (rc < 0) {
		d_vpr_e("%s: User memory fault\n", __func__);
		rc = -EFAULT;
		goto exit;
	}

	rc = kstrtoint(kbuf, 0, &msm_vidc_debug);
	if (rc) {
		d_vpr_e("returning error err %d\n", rc);
		rc = -EINVAL;
		goto exit;
	}
	core->resources.msm_vidc_hw_rsp_timeout =
	((msm_vidc_debug & 0xFF) > (VIDC_ERR | VIDC_HIGH)) ? 1500 : 1000;
	rc = count;
	d_vpr_h("debug timeout updated to - %d\n",
		core->resources.msm_vidc_hw_rsp_timeout);

exit:
	return rc;
}

static ssize_t debug_level_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	size_t len;
	char kbuf[MAX_DEBUG_LEVEL_STRING_LEN];

	len = scnprintf(kbuf, sizeof(kbuf), "0x%08x\n", msm_vidc_debug);
	return simple_read_from_buffer(buf, count, ppos, kbuf, len);
}

static const struct file_operations debug_level_fops = {
	.open = simple_open,
	.write = debug_level_write,
	.read = debug_level_read,
};

struct dentry *msm_vidc_debugfs_init_drv(void)
{
	bool ok = false;
	struct dentry *dir = NULL;

	dir = debugfs_create_dir("msm_vidc", NULL);
	if (IS_ERR_OR_NULL(dir)) {
		dir = NULL;
		goto failed_create_dir;
	}

#define __debugfs_create(__type, __name, __value) ({                          \
	struct dentry *f = debugfs_create_##__type(__name, 0644,	\
		dir, __value);                                                \
	if (IS_ERR_OR_NULL(f)) {                                              \
		d_vpr_e("Failed creating debugfs file '%pd/%s'\n",  \
			dir, __name);                                         \
		f = NULL;                                                     \
	}                                                                     \
	f;                                                                    \
})

	ok =
	__debugfs_create(u32, "fw_debug_mode", &msm_vidc_fw_debug_mode) &&
	__debugfs_create(bool, "fw_coverage", &msm_vidc_fw_coverage) &&
	__debugfs_create(bool, "disable_thermal_mitigation",
			&msm_vidc_thermal_mitigation_disabled) &&
	__debugfs_create(u32, "core_clock_voting",
			&msm_vidc_clock_voting) &&
	__debugfs_create(bool, "disable_video_syscache",
			&msm_vidc_syscache_disable) &&
	__debugfs_create(bool, "cvp_usage", &msm_vidc_cvp_usage) &&
	__debugfs_create(bool, "lossless_encoding",
			&msm_vidc_lossless_encode) &&
	__debugfs_create(u32, "disable_err_recovery",
			&msm_vidc_err_recovery_disable);

#undef __debugfs_create

	if (!ok)
		goto failed_create_dir;

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
		d_vpr_e("%s: invalid params\n", __func__);
		goto failed_create_dir;
	}

	snprintf(debugfs_name, MAX_DEBUGFS_NAME, "core%d", core->id);
	dir = debugfs_create_dir(debugfs_name, parent);
	if (!dir) {
		d_vpr_e("Failed to create debugfs for msm_vidc\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_file("info", 0444, dir, core, &core_info_fops)) {
		d_vpr_e("debugfs_create_file: fail\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_file("trigger_ssr", 0200,
			dir, core, &ssr_fops)) {
		d_vpr_e("debugfs_create_file: fail\n");
		goto failed_create_dir;
	}
	if (!debugfs_create_file("debug_level", 0644,
			parent, core, &debug_level_fops)) {
		d_vpr_e("debugfs_create_file: fail\n");
		goto failed_create_dir;
	}
failed_create_dir:
	return dir;
}

static int inst_info_open(struct inode *inode, struct file *file)
{
	d_vpr_l("Open inode ptr: %pK\n", inode->i_private);
	file->private_data = inode->i_private;
	return 0;
}

static int publish_unreleased_reference(struct msm_vidc_inst *inst,
		char **dbuf, char *end)
{
	struct msm_vidc_buffer *temp = NULL;
	char *cur = *dbuf;

	if (!inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (inst->buffer_mode_set[OUTPUT_PORT] == HAL_BUFFER_MODE_DYNAMIC) {
		cur += write_str(cur, end - cur, "Pending buffer references\n");

		mutex_lock(&inst->registeredbufs.lock);
		list_for_each_entry(temp, &inst->registeredbufs.list, list) {
			struct vb2_buffer *vb2 = &temp->vvb.vb2_buf;

			if (vb2->type == OUTPUT_MPLANE) {
				cur += write_str(cur, end - cur,
				"\tbuffer: %#x fd[0] = %d size %d refcount = %d\n",
				temp->smem[0].device_addr,
				vb2->planes[0].m.fd,
				vb2->planes[0].length,
				temp->smem[0].refcount);
			}
		}
		mutex_unlock(&inst->registeredbufs.lock);
	}

	*dbuf = cur;
	return 0;
}

static void put_inst_helper(struct kref *kref)
{
	struct msm_vidc_inst *inst = container_of(kref,
			struct msm_vidc_inst, kref);

	msm_vidc_destroy(inst);
}

static ssize_t inst_info_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	struct core_inst_pair *idata = file->private_data;
	struct msm_vidc_core *core;
	struct msm_vidc_inst *inst, *temp = NULL;
	char *dbuf, *cur, *end;
	int i, j;
	ssize_t len = 0;
	struct v4l2_format *f;

	if (!idata || !idata->core || !idata->inst) {
		d_vpr_e("%s: invalid params %pK\n", __func__, idata);
		return 0;
	}

	core = idata->core;
	inst = idata->inst;

	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		if (temp == inst)
			break;
	}
	inst = ((temp == inst) && kref_get_unless_zero(&inst->kref)) ?
		inst : NULL;
	mutex_unlock(&core->lock);

	if (!inst) {
		d_vpr_e("%s: Instance has become obsolete", __func__);
		return 0;
	}

	dbuf = kzalloc(MAX_DBG_BUF_SIZE, GFP_KERNEL);
	if (!dbuf) {
		s_vpr_e(inst->sid, "%s: Allocation failed!\n", __func__);
		len = -ENOMEM;
		goto failed_alloc;
	}
	cur = dbuf;
	end = cur + MAX_DBG_BUF_SIZE;

	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	cur += write_str(cur, end - cur, "==============================\n");
	cur += write_str(cur, end - cur, "INSTANCE: %pK (%s)\n", inst,
		inst->session_type == MSM_VIDC_ENCODER ? "Encoder" : "Decoder");
	cur += write_str(cur, end - cur, "==============================\n");
	cur += write_str(cur, end - cur, "core: %pK\n", inst->core);
	cur += write_str(cur, end - cur, "height: %d\n", f->fmt.pix_mp.height);
	cur += write_str(cur, end - cur, "width: %d\n", f->fmt.pix_mp.width);
	cur += write_str(cur, end - cur, "fps: %d\n",
			inst->clk_data.frame_rate >> 16);
	cur += write_str(cur, end - cur, "state: %d\n", inst->state);
	cur += write_str(cur, end - cur, "secure: %d\n",
		!!(inst->flags & VIDC_SECURE));
	cur += write_str(cur, end - cur, "-----------Formats-------------\n");
	for (i = 0; i < MAX_PORT_NUM; i++) {
		f = &inst->fmts[i].v4l2_fmt;
		cur += write_str(cur, end - cur, "capability: %s\n",
			i == INPUT_PORT ? "Output" : "Capture");
		cur += write_str(cur, end - cur, "name : %s\n",
			inst->fmts[i].name);
		cur += write_str(cur, end - cur, "planes : %d\n",
			f->fmt.pix_mp.num_planes);
		cur += write_str(cur, end - cur,
			"type: %s\n", i == INPUT_PORT ?
			"Output" : "Capture");
		switch (inst->buffer_mode_set[i]) {
		case HAL_BUFFER_MODE_STATIC:
			cur += write_str(cur, end - cur,
				"buffer mode : %s\n", "static");
			break;
		case HAL_BUFFER_MODE_DYNAMIC:
			cur += write_str(cur, end - cur,
				"buffer mode : %s\n", "dynamic");
			break;
		default:
			cur += write_str(cur, end - cur,
				"buffer mode : unsupported\n");
		}

		cur += write_str(cur, end - cur, "count: %u\n",
				inst->bufq[i].vb2_bufq.num_buffers);

		for (j = 0; j < f->fmt.pix_mp.num_planes; j++)
			cur += write_str(cur, end - cur,
				"size for plane %d: %u\n",
				j, f->fmt.pix_mp.plane_fmt[j].sizeimage);

		if (i < MAX_PORT_NUM - 1)
			cur += write_str(cur, end - cur, "\n");
	}
	cur += write_str(cur, end - cur, "-------------------------------\n");
	for (i = SESSION_MSG_START; i < SESSION_MSG_END; i++) {
		cur += write_str(cur, end - cur, "completions[%d]: %s\n", i,
		completion_done(&inst->completions[SESSION_MSG_INDEX(i)]) ?
		"pending" : "done");
	}
	cur += write_str(cur, end - cur, "ETB Count: %d\n", inst->count.etb);
	cur += write_str(cur, end - cur, "EBD Count: %d\n", inst->count.ebd);
	cur += write_str(cur, end - cur, "FTB Count: %d\n", inst->count.ftb);
	cur += write_str(cur, end - cur, "FBD Count: %d\n", inst->count.fbd);

	publish_unreleased_reference(inst, &cur, end);
	len = simple_read_from_buffer(buf, count, ppos,
		dbuf, cur - dbuf);

	kfree(dbuf);
failed_alloc:
	kref_put(&inst->kref, put_inst_helper);
	return len;
}

static int inst_info_release(struct inode *inode, struct file *file)
{
	d_vpr_l("Release inode ptr: %pK\n", inode->i_private);
	file->private_data = NULL;
	return 0;
}

static const struct file_operations inst_info_fops = {
	.open = inst_info_open,
	.read = inst_info_read,
	.release = inst_info_release,
};

struct dentry *msm_vidc_debugfs_init_inst(struct msm_vidc_inst *inst,
		struct dentry *parent)
{
	struct dentry *dir = NULL, *info = NULL;
	char debugfs_name[MAX_DEBUGFS_NAME];
	struct core_inst_pair *idata = NULL;

	if (!inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		goto exit;
	}
	snprintf(debugfs_name, MAX_DEBUGFS_NAME, "inst_%p", inst);

	idata = kzalloc(sizeof(struct core_inst_pair), GFP_KERNEL);
	if (!idata) {
		s_vpr_e(inst->sid, "%s: Allocation failed!\n", __func__);
		goto exit;
	}

	idata->core = inst->core;
	idata->inst = inst;

	dir = debugfs_create_dir(debugfs_name, parent);
	if (!dir) {
		s_vpr_e(inst->sid, "Failed to create debugfs for msm_vidc\n");
		goto failed_create_dir;
	}

	info = debugfs_create_file("info", 0444, dir,
			idata, &inst_info_fops);
	if (!info) {
		s_vpr_e(inst->sid, "debugfs_create_file: fail\n");
		goto failed_create_file;
	}

	dir->d_inode->i_private = info->d_inode->i_private;
	inst->debug.pdata[FRAME_PROCESSING].sampling = true;
	return dir;

failed_create_file:
	debugfs_remove_recursive(dir);
	dir = NULL;
failed_create_dir:
	kfree(idata);
exit:
	return dir;
}

void msm_vidc_debugfs_deinit_inst(struct msm_vidc_inst *inst)
{
	struct dentry *dentry = NULL;

	if (!inst || !inst->debugfs_root)
		return;

	dentry = inst->debugfs_root;
	if (dentry->d_inode) {
		s_vpr_l(inst->sid, "Destroy %pK\n", dentry->d_inode->i_private);
		kfree(dentry->d_inode->i_private);
		dentry->d_inode->i_private = NULL;
	}
	debugfs_remove_recursive(dentry);
	inst->debugfs_root = NULL;
}

void msm_vidc_debugfs_update(struct msm_vidc_inst *inst,
	enum msm_vidc_debugfs_event e)
{
	struct msm_vidc_debug *d = &inst->debug;
	char a[64] = "Frame processing";

	switch (e) {
	case MSM_VIDC_DEBUGFS_EVENT_ETB:
		inst->count.etb++;
		trace_msm_v4l2_vidc_buffer_counter("ETB",
				inst->count.etb, inst->count.ebd,
				inst->count.ftb, inst->count.fbd);
		if (inst->count.ebd && inst->count.ftb > inst->count.fbd) {
			d->pdata[FRAME_PROCESSING].name[0] = '\0';
			tic(inst, FRAME_PROCESSING, a);
		}
	break;
	case MSM_VIDC_DEBUGFS_EVENT_EBD:
		inst->count.ebd++;
		trace_msm_v4l2_vidc_buffer_counter("EBD",
				inst->count.etb, inst->count.ebd,
				inst->count.ftb, inst->count.fbd);
		if (inst->count.ebd && inst->count.ebd == inst->count.etb) {
			toc(inst, FRAME_PROCESSING);
			s_vpr_p(inst->sid, "EBD: FW needs input buffers\n");
		}
		if (inst->count.ftb == inst->count.fbd)
			s_vpr_p(inst->sid, "EBD: FW needs output buffers\n");
	break;
	case MSM_VIDC_DEBUGFS_EVENT_FTB: {
		inst->count.ftb++;
		trace_msm_v4l2_vidc_buffer_counter("FTB",
				inst->count.etb, inst->count.ebd,
				inst->count.ftb, inst->count.fbd);
		if (inst->count.ebd && inst->count.etb > inst->count.ebd) {
			d->pdata[FRAME_PROCESSING].name[0] = '\0';
			tic(inst, FRAME_PROCESSING, a);
		}
	}
	break;
	case MSM_VIDC_DEBUGFS_EVENT_FBD:
		inst->count.fbd++;
		inst->debug.samples++;
		trace_msm_v4l2_vidc_buffer_counter("FBD",
				inst->count.etb, inst->count.ebd,
				inst->count.ftb, inst->count.fbd);
		if (inst->count.fbd &&
			inst->count.fbd == inst->count.ftb) {
			toc(inst, FRAME_PROCESSING);
			s_vpr_p(inst->sid, "FBD: FW needs output buffers\n");
		}
		if (inst->count.etb == inst->count.ebd)
			s_vpr_p(inst->sid, "FBD: FW needs input buffers\n");
		break;
	default:
		s_vpr_e(inst->sid, "Invalid state in debugfs: %d\n", e);
		break;
	}
}

int msm_vidc_check_ratelimit(void)
{
	static DEFINE_RATELIMIT_STATE(_rs,
				VIDC_DBG_SESSION_RATELIMIT_INTERVAL,
				VIDC_DBG_SESSION_RATELIMIT_BURST);
	return __ratelimit(&_rs);
}

/**
 * get_sid() must be called under "&core->lock"
 * to avoid race condition at occupying empty slot.
 */
int get_sid(u32 *sid, u32 session_type)
{
	int i;

	for (i = 0; i < MAX_SUPPORTED_INSTANCES; i++) {
		if (!ctxt[i].used) {
			ctxt[i].used = 1;
			*sid = i+1;
			update_log_ctxt(*sid, session_type, 0);
			break;
		}
	}

	return (i == MAX_SUPPORTED_INSTANCES);
}

void put_sid(u32 sid)
{
	if (!sid || sid > MAX_SUPPORTED_INSTANCES) {
		d_vpr_e("%s: invalid sid %#x\n",
			__func__, sid);
		return;
	}
	if (ctxt[sid-1].used)
		ctxt[sid-1].used = 0;
}

inline void update_log_ctxt(u32 sid, u32 session_type, u32 fourcc)
{
	const char *codec;
	char type;
	u32 s_type = 0;

	if (!sid || sid > MAX_SUPPORTED_INSTANCES) {
		d_vpr_e("%s: invalid sid %#x\n",
			__func__, sid);
	}

	switch (fourcc) {
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_H264_NO_SC:
		codec = "h264";
		break;
	case V4L2_PIX_FMT_H264_MVC:
		codec = " mvc";
		break;
	case V4L2_PIX_FMT_MPEG1:
		codec = "mpg1";
		break;
	case V4L2_PIX_FMT_MPEG2:
		codec = "mpg2";
		break;
	case V4L2_PIX_FMT_VP8:
		codec = " vp8";
		break;
	case V4L2_PIX_FMT_VP9:
		codec = " vp9";
		break;
	case V4L2_PIX_FMT_HEVC:
		codec = "h265";
		break;
	case V4L2_PIX_FMT_TME:
		codec = " tme";
		break;
	case V4L2_PIX_FMT_CVP:
		codec = " cvp";
		break;
	default:
		codec = "....";
		break;
	}

	switch (session_type) {
	case MSM_VIDC_ENCODER:
		type = 'e';
		s_type = VIDC_ENCODER;
		break;
	case MSM_VIDC_DECODER:
		type = 'd';
		s_type = VIDC_DECODER;
		break;
	case MSM_VIDC_CVP:
		type = 'c';
		s_type = VIDC_CVP;
	default:
		type = '.';
		break;
	}

	ctxt[sid-1].session_type = s_type;
	ctxt[sid-1].codec_type = fourcc;
	memcpy(&ctxt[sid-1].name, codec, 4);
	ctxt[sid-1].name[4] = type;
	ctxt[sid-1].name[5] = '\0';
}

inline char *get_codec_name(u32 sid)
{
	if (!sid || sid > MAX_SUPPORTED_INSTANCES)
		return ".....";

	return ctxt[sid-1].name;
}

/**
 * 0xx -> allow prints for all sessions
 * 1xx -> allow only encoder prints
 * 2xx -> allow only decoder prints
 * 4xx -> allow only cvp prints
 */
inline bool is_print_allowed(u32 sid, u32 level)
{
	if (!(msm_vidc_debug & level))
		return false;

	if (!((msm_vidc_debug >> 8) & 0xF))
		return true;

	if (!sid || sid > MAX_SUPPORTED_INSTANCES)
		return true;

	if (ctxt[sid-1].session_type & msm_vidc_debug)
		return true;

	return false;
}
