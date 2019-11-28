/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include "vpu_cmn.h"
#include "vpu_algo.h"
#include "vpu_debug.h"
#include "vpu_hw.h"
#include "vpu_power.h"

enum message_level {
	VPU_DBG_MSG_LEVEL_NONE,
	VPU_DBG_MSG_LEVEL_CTRL,
	VPU_DBG_MSG_LEVEL_DATA,
	VPU_DBG_MSG_LEVEL_DEBUG,
	VPU_DBG_MSG_LEVEL_TOTAL,
};

struct vpu_message_ctrl {
	unsigned int mutex;
	int head;
	int tail;
	int buf_size;
	unsigned int level_mask;
	unsigned int data;
};

u32 vpu_klog;

const char *g_vpu_prop_type_names[VPU_NUM_PROP_TYPES] = {
	[VPU_PROP_TYPE_CHAR]     = "char",
	[VPU_PROP_TYPE_INT32]    = "int32",
	[VPU_PROP_TYPE_FLOAT]    = "float",
	[VPU_PROP_TYPE_INT64]    = "int64",
	[VPU_PROP_TYPE_DOUBLE]   = "double"
};

const char *g_vpu_port_usage_names[VPU_NUM_PORT_USAGES] = {
	[VPU_PORT_USAGE_IMAGE]     = "image",
	[VPU_PORT_USAGE_DATA]      = "data",
};

const char *g_vpu_port_dir_names[VPU_NUM_PORT_DIRS] = {
	[VPU_PORT_DIR_IN]       = "in",
	[VPU_PORT_DIR_OUT]      = "out",
	[VPU_PORT_DIR_IN_OUT]   = "in-out",
};

static void vpu_debug_algo_info(struct seq_file *s,
	struct vpu_prop *p, struct vpu_prop_desc *d)
{
#if 0
	int i;
	unsigned long ptr = p->ptr + d->offset;

	for (i = 0; i < d->count; i++) {
		seq_printf(s, "%s[%d] = ", d->name, i);
		switch (d->type) {
		case VPU_PROP_TYPE_CHAR:
			seq_printf(s, "%c\n", *((char *)ptr));
			break;
		case VPU_PROP_TYPE_INT32:
			seq_printf(s, "%d\n", *((int32_t *)ptr));
			break;
		case VPU_PROP_TYPE_FLOAT:
			seq_printf(s, "%lf\n", *((float *)ptr));
			break;
		case VPU_PROP_TYPE_INT64:
			seq_printf(s, "%lld\n", *((int64_t *)ptr));
			break;
		case VPU_PROP_TYPE_DOUBLE:
			seq_printf(s, "%llf\n", *((double *)ptr));
			break;
		default:
			break;
		}
		ptr += g_vpu_prop_type_size[d->type];
	}
#endif
}

static void vpu_debug_algo_prop(struct seq_file *s,
	struct vpu_prop *p, const char *prefix)
{
	struct vpu_prop_desc *d;
	int i;

	if (!p->desc_cnt)
		return;

	seq_printf(s, "	|%-5s|%-15s|%-7s|%-7s|%-30s|\n",
		prefix, "Name", "Type", "Count", "Value");

	for (i = 0; i < p->desc_cnt; i++) {
		d = &p->desc[i];

		seq_printf(s, "	|%-5d|%-15s|%-7s|%-7d|%04XH ",
			  d->id,
			  d->name,
			  g_vpu_prop_type_names[d->type],
			  d->count,
			  0);

		vpu_debug_algo_info(s, p, d);
	}


}

static int vpu_debug_algo_entry(struct seq_file *s,
	struct vpu_device *vd, struct __vpu_algo *alg)
{
	int i;
	int ret = 0;
	struct vpu_port *p;

	seq_printf(s, "[%s: addr: 0x%llx, len: 0x%x ref: %d builtin: %d]\n",
		   alg->a.name, alg->a.mva,
		   alg->a.len, kref_read(&alg->ref), alg->builtin);

	vpu_alg_debug("%s: [%s: addr: 0x%llx, len: 0x%x ref: %d builtin: %d]\n",
		      __func__, alg->a.name, alg->a.mva,
		      alg->a.len, kref_read(&alg->ref), alg->builtin);

	ret = vpu_alg_load(vd, NULL, alg);
	if (ret)
		return ret;

	vpu_alg_get(vd, NULL, alg);
	if (!alg->a.port_count)
		goto out;

	seq_printf(s, "	|%-5s|%-15s|%-7s|%-7s|\n",
			"Port", "Name", "Dir", "Usage");

	for (i = 0; i < alg->a.port_count; i++) {
		p = &alg->a.ports[i];
		seq_printf(s, "	|%-5d|%-15s|%-7s|%-7s|\n",
				  p->id, p->name,
				  g_vpu_port_dir_names[p->dir],
				  g_vpu_port_usage_names[p->usage]);
	}
	vpu_debug_algo_prop(s, &alg->a.sett, "Sett.");
	vpu_debug_algo_prop(s, &alg->a.info, "Info.");
out:
	vpu_alg_put(alg);
	return ret;
}

static int vpu_debug_algo(struct seq_file *s)
{
	struct vpu_device *vd;
	struct list_head *ptr, *tmp;
	struct __vpu_algo *alg;
	int ret;

	if (!s)
		return -ENOENT;

	vd = (struct vpu_device *) s->private;

	mutex_lock(&vd->cmd_lock);

	ret = vpu_pwr_get_locked_nb(vd);
	if (ret)
		goto err_pwr;

	ret = vpu_dev_boot(vd);
	if (ret == -ETIMEDOUT)
		goto err_pwr;
	if (ret)
		goto err_boot;

	list_for_each_safe(ptr, tmp, &vd->algo) {
		alg = list_entry(ptr, struct __vpu_algo, list);
		ret = vpu_debug_algo_entry(s, vd, alg);
		if (ret == -ETIMEDOUT)
			goto err_pwr;
		if (ret)
			goto err_boot;
	}

err_boot:
	vpu_pwr_put_locked(vd);
err_pwr:
	mutex_unlock(&vd->cmd_lock);

	return 0;
}

static void *vpu_mesg_pa_to_va(struct vpu_mem *work_buf, unsigned int phys_addr)
{
	unsigned long ret = 0;
	int offset = 0;

	if (!phys_addr)
		return NULL;

	offset = phys_addr - work_buf->pa;
	ret = work_buf->va + offset;

	return (void *)(ret);
}

static void vpu_mesg_init(struct vpu_device *vd)
{
	u64 log_buf = 0;

	struct vpu_message_ctrl *msg = NULL;

	if (!vd->iova_work.m.va)
		return;

	log_buf = vd->iova_work.m.va + VPU_OFFSET_LOG + VPU_SIZE_LOG_HEADER;
	msg = (struct vpu_message_ctrl *)log_buf;
	memset(msg, 0, VPU_SIZE_LOG_DATA);
	msg->level_mask = (1 << VPU_DBG_MSG_LEVEL_CTRL);
}

static void vpu_mesg_clr(struct vpu_device *vd)
{
	char *data = NULL;
	u64 log_buf = 0;
	struct vpu_message_ctrl *msg = NULL;

	if (!vd->iova_work.m.va)
		return;

	log_buf = vd->iova_work.m.va + VPU_OFFSET_LOG + VPU_SIZE_LOG_HEADER;
	msg = (struct vpu_message_ctrl *)log_buf;
	data = (char *)vpu_mesg_pa_to_va(&vd->iova_work.m, msg->data);

	msg->head = 0;
	msg->tail = 0;

	if (data)
		memset(data, 0,
		       VPU_SIZE_LOG_DATA - sizeof(struct vpu_message_ctrl));
}

static int vpu_mesg_level_set(void *data, u64 val)
{
	u64 log_buf = 0;
	struct vpu_message_ctrl *msg = NULL;
	struct vpu_device *vd = data;
	int level = (int)val;

	if (!vd)
		return -ENOENT;

	if (!level) {
		vpu_mesg_clr(vd);
		return 0;
	}

	if (level < -1 || level >= VPU_DBG_MSG_LEVEL_TOTAL) {
		pr_info("val: %d\n", level);
		return -ENOENT;
	}

	if (!vd->iova_work.m.va)
		return -EFAULT;

	log_buf = vd->iova_work.m.va + VPU_OFFSET_LOG + VPU_SIZE_LOG_HEADER;
	msg = (struct vpu_message_ctrl *)log_buf;

	if (level != -1)
		msg->level_mask ^= (1 << level);
	else
		msg->level_mask = 0;
	return 0;
}

static int vpu_mesg_level_get(void *data, u64 *val)
{
	u64 log_buf = 0;
	struct vpu_message_ctrl *msg = NULL;
	struct vpu_device *vd = data;

	if (!vd)
		return -ENOENT;

	if (!vd->iova_work.m.va)
		return -EFAULT;

	log_buf = vd->iova_work.m.va + VPU_OFFSET_LOG + VPU_SIZE_LOG_HEADER;
	msg = (struct vpu_message_ctrl *)log_buf;
	*val = msg->level_mask;
	return 0;
}

int vpu_mesg_seq(struct seq_file *s, struct vpu_device *vd)
{
	int i, wrap = false;
	char *data = NULL;
	u64 log_buf = 0;
	struct vpu_message_ctrl *msg = NULL;

	if (!s)
		return -ENOENT;

	if (!vd->iova_work.m.va)
		return -ENOENT;

	log_buf = vd->iova_work.m.va + VPU_OFFSET_LOG + VPU_SIZE_LOG_HEADER;
	msg = (struct vpu_message_ctrl *)log_buf;
	data = (char *)vpu_mesg_pa_to_va(&vd->iova_work.m, msg->data);
	i = msg->head;
	do {
		if (msg->head == msg->tail || i == msg->tail)
			seq_printf(s, "%s", "<empty log>\n");
		while (i != msg->tail && data) {
			if (i > msg->tail && wrap)
				break;

			seq_printf(s, "%s", data + i);
			i += strlen(data + i) + 1;

			if (i >= msg->buf_size) {
				i = 0;
				wrap = true;
			}
		}
	} while (0);

	return 0;

}

static int vpu_debug_mesg(struct seq_file *s)
{
	if (!s)
		return -ENOENT;

	return vpu_mesg_seq(s, (struct vpu_device *)s->private);
}

static int vpu_debug_reg(struct seq_file *s)
{
	struct vpu_device *vd;
	int ret;

	if (!s)
		return -ENOENT;

	vd = (struct vpu_device *) s->private;

	mutex_lock(&vd->lock);
	ret = vpu_pwr_get_locked_nb(vd);
	if (ret)
		return ret;

	seq_printf(s, "vpu%d registers\n", vd->id);
	seq_puts(s, "name\toffset\tvalue\n");

#define seq_vpu_reg(r) \
	seq_printf(s, "%s\t%4xh\t%08xh\n", #r, r, vpu_reg_read(vd, r))

	seq_vpu_reg(CG_CON);
	seq_vpu_reg(SW_RST);
	seq_vpu_reg(DONE_ST);
	seq_vpu_reg(CTRL);
	seq_vpu_reg(XTENSA_INT);
	seq_vpu_reg(CTL_XTENSA_INT);
	seq_vpu_reg(DEFAULT0);
	seq_vpu_reg(DEFAULT1);
	seq_vpu_reg(XTENSA_INFO00);
	seq_vpu_reg(XTENSA_INFO01);
	seq_vpu_reg(XTENSA_INFO02);
	seq_vpu_reg(XTENSA_INFO03);
	seq_vpu_reg(XTENSA_INFO04);
	seq_vpu_reg(XTENSA_INFO05);
	seq_vpu_reg(XTENSA_INFO06);
	seq_vpu_reg(XTENSA_INFO07);
	seq_vpu_reg(XTENSA_INFO08);
	seq_vpu_reg(XTENSA_INFO09);
	seq_vpu_reg(XTENSA_INFO10);
	seq_vpu_reg(XTENSA_INFO11);
	seq_vpu_reg(XTENSA_INFO12);
	seq_vpu_reg(XTENSA_INFO13);
	seq_vpu_reg(XTENSA_INFO14);
	seq_vpu_reg(XTENSA_INFO15);
	seq_vpu_reg(XTENSA_INFO16);
	seq_vpu_reg(XTENSA_INFO17);
	seq_vpu_reg(XTENSA_INFO18);
	seq_vpu_reg(XTENSA_INFO19);
	seq_vpu_reg(XTENSA_INFO20);
	seq_vpu_reg(XTENSA_INFO21);
	seq_vpu_reg(XTENSA_INFO22);
	seq_vpu_reg(XTENSA_INFO23);
	seq_vpu_reg(XTENSA_INFO24);
	seq_vpu_reg(XTENSA_INFO25);
	seq_vpu_reg(XTENSA_INFO26);
	seq_vpu_reg(XTENSA_INFO27);
	seq_vpu_reg(XTENSA_INFO28);
	seq_vpu_reg(XTENSA_INFO29);
	seq_vpu_reg(XTENSA_INFO30);
	seq_vpu_reg(XTENSA_INFO31);
	seq_vpu_reg(DEBUG_INFO00);
	seq_vpu_reg(DEBUG_INFO01);
	seq_vpu_reg(DEBUG_INFO02);
	seq_vpu_reg(DEBUG_INFO03);
	seq_vpu_reg(DEBUG_INFO04);
	seq_vpu_reg(DEBUG_INFO05);
	seq_vpu_reg(DEBUG_INFO06);
	seq_vpu_reg(DEBUG_INFO07);
	seq_vpu_reg(XTENSA_ALTRESETVEC);
#undef seq_vpu_reg

	vpu_pwr_put_locked(vd);
	mutex_unlock(&vd->lock);

	return 0;
}

/**
 * vpu_debug_jtag_set - enable/disable jtag
 *
 * @data: vpu device
 * @val: user write value
 *
 * return 0: enable/disable success
 */
static int vpu_debug_jtag_set(void *data, u64 val)
{
	struct vpu_device *vd = data;
	int ret = 0;

	if (!vd)
		return -ENOENT;

	if (val) {
		/* enable jtag */
		vd->jtag_enabled = 1;
	} else {
		/* disable jtag */
		vd->jtag_enabled = 0;
	}

	return ret;
}

/**
 * vpu_debug_jtag_get - show jtag status of vpu
 * @data: vpu device
 * @val: return value
 *
 * return 0: operation success
 */
static int vpu_debug_jtag_get(void *data, u64 *val)
{
	struct vpu_device *vd = data;

	if (!vd)
		return -ENOENT;

	*val = vd->jtag_enabled;
	return 0;
}

static char *vpu_debug_simple_write(const char __user *buffer, size_t count)
{
	char *buf;
	int ret;

	buf = kzalloc(count + 1, GFP_KERNEL);
	if (!buf)
		goto out;

	ret = copy_from_user(buf, buffer, count);
	if (ret) {
		pr_info("%s: copy_from_user: ret=%d\n", __func__, ret);
		kfree(buf);
		buf = NULL;
		goto out;
	}

	buf[count] = '\0';
out:
	return buf;
}

static int vpu_debug_vpu_memory(struct seq_file *s)
{
	vpu_dmp_seq(s);
	return 0;
}

static ssize_t vpu_debug_vpu_memory_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *f_pos)
{
	char *buf, *cmd, *cur;

	pr_info("%s:\n", __func__);

	buf = vpu_debug_simple_write(buffer, count);

	if (!buf)
		goto out;

	cur = buf;
	cmd = strsep(&cur, " \t\n");
	if (!strcmp(cmd, "free"))
		vpu_dmp_free_all();

	kfree(buf);
out:
	return count;
}

static int vpu_debug_dump(struct seq_file *s)
{
	struct vpu_device *vd;

	if (!s)
		return -ENOENT;

	vd = (struct vpu_device *) s->private;
	vpu_dmp_seq_core(s, vd);

	return 0;
}

static ssize_t vpu_debug_dump_write(struct file *filp,
	const char __user *buffer, size_t count, loff_t *f_pos)
{
	char *buf, *cmd, *cur;
	struct vpu_device *vd;

	if (!filp || !filp->f_inode)
		goto out;

	vd = (struct vpu_device *) filp->f_inode->i_private;

	if (!vd)
		goto out;

	buf = vpu_debug_simple_write(buffer, count);

	if (!buf)
		goto out;

	cur = buf;
	cmd = strsep(&cur, " \t\n");
	if (!strcmp(cmd, "free"))
		vpu_dmp_free(vd);
	else if (!strcmp(cmd, "dump"))
		vpu_dmp_create(vd, NULL, "Dump trigger by user");

	kfree(buf);
out:
	return count;
}

#define VPU_DEBUGFS_FOP_DEF(name) \
static struct dentry *vpu_d##name; \
static int vpu_debug_## name ##_show(struct seq_file *s, void *unused) \
{ \
	vpu_debug_## name(s); \
	return 0; \
} \
static int vpu_debug_## name ##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, vpu_debug_ ## name ## _show, \
		inode->i_private); \
} \

#define VPU_DEBUGFS_DEF(name) \
	VPU_DEBUGFS_FOP_DEF(name) \
static const struct file_operations vpu_debug_ ## name ## _fops = { \
	.open = vpu_debug_ ## name ## _open, \
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

#define VPU_DEBUGFS_RW_DEF(name) \
	VPU_DEBUGFS_FOP_DEF(name) \
static const struct file_operations vpu_debug_ ## name ## _fops = { \
	.open = vpu_debug_ ## name ## _open, \
	.read = seq_read, \
	.write = vpu_debug_ ## name ## _write, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

VPU_DEBUGFS_DEF(algo);
VPU_DEBUGFS_RW_DEF(dump);
VPU_DEBUGFS_DEF(mesg);
VPU_DEBUGFS_DEF(reg);
VPU_DEBUGFS_RW_DEF(vpu_memory);

#undef VPU_DEBUGFS_DEF

static struct dentry *vpu_djtag;
DEFINE_SIMPLE_ATTRIBUTE(vpu_debug_jtag_fops, vpu_debug_jtag_get,
			vpu_debug_jtag_set, "%lld\n");

static struct dentry *vpu_dmesg_level;
DEFINE_SIMPLE_ATTRIBUTE(vpu_debug_mesg_level_fops, vpu_mesg_level_get,
			vpu_mesg_level_set, "%lld\n");

#define VPU_DEBUGFS_CREATE(name) \
{ \
	vpu_d##name = debugfs_create_file(#name, 0644, \
		droot,         \
			NULL, &vpu_debug_ ## name ## _fops); \
	if (IS_ERR_OR_NULL(vpu_d##name)) { \
		ret = PTR_ERR(vpu_d##name); \
		pr_info("%s: vpu%d: " #name "): %d\n", \
			__func__, (vd) ? (vd->id) : 0, ret); \
		goto out; \
	} \
	vpu_d##name->d_inode->i_private = vd; \
}

int vpu_init_dev_debug(struct platform_device *pdev, struct vpu_device *vd)
{
	int ret = 0;
	struct dentry *droot;

	if (!vpu_drv->droot)
		return -ENODEV;

	droot = debugfs_create_dir(vd->name, vpu_drv->droot);

	if (IS_ERR_OR_NULL(droot)) {
		ret = PTR_ERR(droot);
		pr_info("%s: failed to create debugfs node: vpu/%s: %d\n",
			__func__, vd->name, ret);
		goto out;
	}

	debugfs_create_u64("pw_off_latency", 0660, droot,
		&vd->pw_off_latency);
	debugfs_create_u64("cmd_timeout", 0660, droot,
		&vd->cmd_timeout);
	debugfs_create_u32("state", 0440, droot,
		&vd->state);

	vpu_dmp_init(vd);
	vpu_mesg_init(vd);

	VPU_DEBUGFS_CREATE(algo);
	VPU_DEBUGFS_CREATE(dump);
	VPU_DEBUGFS_CREATE(mesg);
	VPU_DEBUGFS_CREATE(reg);
	VPU_DEBUGFS_CREATE(jtag);
	VPU_DEBUGFS_CREATE(mesg_level);
out:
	return ret;
}

void vpu_exit_dev_debug(struct platform_device *pdev, struct vpu_device *vd)
{
	if (!vpu_drv || !vpu_drv->droot || !vd || !vd->droot)
		return;

	vpu_dmp_exit(vd);

	debugfs_remove_recursive(vd->droot);
	vd->droot = NULL;
}

int vpu_init_debug(void)
{
	int ret = 0;
	struct dentry *droot;
	struct vpu_device *vd = NULL;

	droot = debugfs_create_dir("vpu", NULL);

	if (IS_ERR_OR_NULL(droot)) {
		ret = PTR_ERR(droot);
		pr_info("%s: failed to create debugfs node: %d\n",
			__func__, ret);
		goto out;
	}

	vpu_drv->droot = droot;
	vpu_klog = VPU_DBG_DRV;
	debugfs_create_u32("klog", 0660, droot, &vpu_klog);
	vpu_drv->ilog = 0;
	debugfs_create_u32("ilog", 0660, droot, &vpu_drv->ilog);
	vpu_drv->met = 0;
	debugfs_create_u32("met", 0660, droot, &vpu_drv->met);

	VPU_DEBUGFS_CREATE(vpu_memory);

out:
	return ret;
}

#undef VPU_DEBUGFS_CREATE

void vpu_exit_debug(void)
{
	if (!vpu_drv || !vpu_drv->droot)
		return;

	debugfs_remove_recursive(vpu_drv->droot);
	vpu_drv->droot = NULL;
}

