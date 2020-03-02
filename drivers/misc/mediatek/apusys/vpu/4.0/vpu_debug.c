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

#include "vpu_cmn.h"
#include "vpu_algo.h"
#include "vpu_debug.h"
#include "vpu_hw.h"

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

static void vpu_debug_algo_entry(struct seq_file *s,
	struct vpu_device *dev, struct __vpu_algo *alg)
{
	int i;
	int ret;
	struct vpu_port *p;

	seq_printf(s, "[%s: addr: 0x%llx, len: 0x%x]\n",
		alg->a.name, alg->a.mva, alg->a.len);

	vpu_alg_debug("%s: [%s: addr: 0x%llx, len: 0x%x]\n",
		__func__, alg->a.name, alg->a.mva, alg->a.len);

	ret = vpu_alg_load(dev, NULL, alg);

	if (ret)
		return;

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
}

static int vpu_debug_algo(struct seq_file *s)
{
	struct vpu_device *dev;
	struct list_head *ptr, *tmp;
	struct __vpu_algo *alg;
	int ret;

	if (!s)
		return -ENOENT;

	dev = (struct vpu_device *) s->private;

	mutex_lock(&dev->cmd_lock);

	/* Bootup VPU */
	ret = vpu_dev_boot(dev);
	if (ret)
		goto out;

	list_for_each_safe(ptr, tmp, &dev->algo) {
		alg = list_entry(ptr, struct __vpu_algo, list);
		vpu_debug_algo_entry(s, dev, alg);
	}

out:
	mutex_unlock(&dev->cmd_lock);

	return 0;
}

static int vpu_debug_mesg(struct seq_file *s)
{
	struct vpu_device *dev;
	char *head, *body, *tail;

	if (!s)
		return -ENOENT;

	dev = (struct vpu_device *) s->private;

	head = (char *)(dev->work_buf->va + VPU_OFFSET_LOG);
	body = head + VPU_SIZE_LOG_HEADER;
	tail = head + VPU_SIZE_LOG_BUF - 1;

	*tail = '\0';

	seq_printf(s, "=== VPU_%d Log Buffer ===\n", dev->id);
	seq_printf(s, "%s\n", body);

	return 0;
}

static int vpu_debug_reg(struct seq_file *s)
{
	struct vpu_device *dev;
	int i;

	if (!s)
		return -ENOENT;

	dev = (struct vpu_device *) s->private;

	seq_printf(s, "vpu%d registers\n", dev->id);
	seq_puts(s, "name, offset, value\n");

	// TODO: read registers only when power is enabled
	for (i = 0; i < VPU_NUM_REGS; i++) {
		struct vpu_reg_desc *reg;
		uint32_t reg_val;

		reg = &g_vpu_reg_descs[i];
		reg_val = vpu_read_reg32(
			(unsigned long)dev->reg_base, reg->offset);

		seq_printf(s, "%s, %08x, %08x\n",
			  reg->name,
			  reg->offset,
			  reg_val);
	}

	return 0;
}

#define VPU_DEBUGFS_DEF(name) \
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
static const struct file_operations vpu_debug_ ## name ## _fops = { \
	.open = vpu_debug_ ## name ## _open, \
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = seq_release, \
}

VPU_DEBUGFS_DEF(algo);
VPU_DEBUGFS_DEF(mesg);
VPU_DEBUGFS_DEF(reg);

#undef VPU_DEBUGFS_DEF

#define VPU_DEBUGFS_CREATE(name) \
{ \
	vpu_d##name = debugfs_create_file(#name, 0644, \
		droot,         \
			NULL, &vpu_debug_ ## name ## _fops); \
	if (IS_ERR_OR_NULL(vpu_d##name)) { \
		ret = PTR_ERR(vpu_d##name); \
		pr_info("%s: vpu%d: " #name "): %d\n", \
			__func__, dev->id, ret); \
		goto out; \
	} \
	vpu_d##name->d_inode->i_private = dev; \
}

int vpu_init_dev_debug(struct platform_device *pdev, struct vpu_device *dev)
{
	int ret = 0;
	struct dentry *droot;

	if (!vpu_drv->droot)
		return -ENODEV;

	droot = debugfs_create_dir(dev->name, vpu_drv->droot);

	if (IS_ERR_OR_NULL(droot)) {
		ret = PTR_ERR(droot);
		pr_info("%s: failed to create debugfs node: vpu/%s: %d\n",
			__func__, dev->name, ret);
		goto out;
	}

	VPU_DEBUGFS_CREATE(algo);
	VPU_DEBUGFS_CREATE(mesg);
	VPU_DEBUGFS_CREATE(reg);

out:
	return ret;
}

#undef VPU_DEBUGFS_CREATE

void vpu_exit_dev_debug(struct platform_device *pdev, struct vpu_device *dev)
{
	if (!vpu_drv || !vpu_drv->droot || !dev || !dev->droot)
		return;

	debugfs_remove_recursive(dev->droot);
	dev->droot = NULL;
}

int vpu_init_debug(void)
{
	int ret = 0;
	struct dentry *droot;

	droot = debugfs_create_dir("vpu", NULL);

	if (IS_ERR_OR_NULL(droot)) {
		ret = PTR_ERR(droot);
		pr_info("%s: failed to create debugfs node: %d\n",
			__func__, ret);
		goto out;
	}

	vpu_drv->droot = droot;
	debugfs_create_u32("klog", 0660, droot, &vpu_klog);
	vpu_klog = VPU_DBG_DRV;

out:
	return ret;
}


void vpu_exit_debug(void)
{
	if (!vpu_drv || !vpu_drv->droot)
		return;

	debugfs_remove_recursive(vpu_drv->droot);
	vpu_drv->droot = NULL;
}

