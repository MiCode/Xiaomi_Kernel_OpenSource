// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec_mux.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "bus.h"
#include "mux_switch.h"

#define PROC_TYPEC "mtk_typec"
#define PROC_MUX_SWITCH PROC_TYPEC "/mux_switch"
#define PROC_MUX "mux"
#define PROC_SWITCH "switch"

/* struct typec_mux_switch */
struct typec_mux_switch {
	struct device *dev;
	struct typec_switch *sw;
	struct typec_mux *mux;
	int orientation;
	struct typec_mux_state state;
	struct proc_dir_entry *root;
};

/* struct mtk_typec_switch */
struct mtk_typec_switch {
	struct device *dev;
	struct typec_switch *sw;
	struct list_head list;
};

/* struct mtk_typec_mux */
struct mtk_typec_mux {
	struct device *dev;
	struct typec_mux *mux;
	struct list_head list;
};

static LIST_HEAD(mux_list);
static LIST_HEAD(switch_list);
static DEFINE_MUTEX(mux_lock);
static DEFINE_MUTEX(switch_lock);

/* MUX */
struct typec_mux *mtk_typec_mux_register(struct device *dev,
			const struct typec_mux_desc *desc)
{
	struct mtk_typec_mux *typec_mux;
	struct typec_mux *mux;

	mutex_lock(&mux_lock);
	list_for_each_entry(typec_mux, &mux_list, list) {
		if (typec_mux->dev == dev) {
			mux = ERR_PTR(-EEXIST);
			goto out;
		}
	}

	typec_mux = kzalloc(sizeof(*typec_mux), GFP_KERNEL);
	if (!typec_mux) {
		mux = ERR_PTR(-ENOMEM);
		goto out;
	}

	mux = typec_mux_register(dev, desc);
	if (IS_ERR(mux)) {
		kfree(typec_mux);
		mux = ERR_PTR(-EINVAL);
		goto out;
	}
	typec_mux->mux = mux;
	list_add_tail(&typec_mux->list, &mux_list);
out:
	mutex_unlock(&mux_lock);
	return mux;
}
EXPORT_SYMBOL_GPL(mtk_typec_mux_register);

void mtk_typec_mux_unregister(struct typec_mux *mux)
{
	struct mtk_typec_mux *typec_mux;

	mutex_lock(&mux_lock);
	list_for_each_entry(typec_mux, &mux_list, list) {
		if (typec_mux->mux == mux)
			break;
	}

	list_del(&typec_mux->list);
	kfree(typec_mux);
	mutex_unlock(&mux_lock);

	typec_mux_unregister(mux);
}
EXPORT_SYMBOL_GPL(mtk_typec_mux_unregister);

static int mtk_typec_mux_set(struct typec_mux *mux, struct typec_mux_state *state)
{
	struct typec_mux_switch *mux_sw = typec_mux_get_drvdata(mux);
	struct mtk_typec_mux *typec_mux;
	int ret = 0;

	dev_info(mux_sw->dev, "%s %d %d\n", __func__,
		 mux_sw->state.mode, state->mode);

	mutex_lock(&mux_lock);

	list_for_each_entry(typec_mux, &mux_list, list) {
		if (!IS_ERR_OR_NULL(typec_mux->mux))
			typec_mux->mux->set(typec_mux->mux, state);
	}

	mux_sw->state.alt = state->alt;
	mux_sw->state.mode = state->mode;
	mux_sw->state.data = state->data;

	mutex_unlock(&mux_lock);

	return ret;
}

static int proc_mux_show(struct seq_file *s, void *unused)
{
	struct typec_mux_switch *mux_sw = s->private;

	seq_printf(s, "%lu\n", mux_sw->state.mode);
	return 0;
}

static int proc_mux_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_mux_show, PDE_DATA(inode));
}

static ssize_t proc_mux_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct typec_mux_switch *mux_sw = s->private;
	struct typec_mux_state state = {};
	char buf[20];
	u32 tmp;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 0, &tmp))
		return -EINVAL;

	if (tmp > 2) {
		dev_info(mux_sw->dev, "%s %d, INVALID: %d\n", __func__,
			 mux_sw->state.mode, tmp);
		return count;
	}

	state.mode = tmp;
	mtk_typec_mux_set(mux_sw->mux, &state);

	return count;
}

static const struct proc_ops proc_mux_fops = {
	.proc_open = proc_mux_open,
	.proc_write = proc_mux_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/* SWITCH */
struct typec_switch *mtk_typec_switch_register(struct device *dev,
			const struct typec_switch_desc *desc)
{
	struct mtk_typec_switch *typec_sw;
	struct typec_switch *sw;

	mutex_lock(&switch_lock);
	list_for_each_entry(typec_sw, &switch_list, list) {
		if (typec_sw->dev == dev) {
			sw = ERR_PTR(-EEXIST);
			goto out;
		}
	}

	typec_sw = kzalloc(sizeof(*typec_sw), GFP_KERNEL);
	if (!typec_sw) {
		sw = ERR_PTR(-ENOMEM);
		goto out;
	}

	sw = typec_switch_register(dev, desc);
	if (IS_ERR(sw)) {
		kfree(typec_sw);
		sw = ERR_PTR(-EINVAL);
		goto out;
	}
	typec_sw->sw = sw;
	list_add_tail(&typec_sw->list, &switch_list);
out:
	mutex_unlock(&switch_lock);
	return sw;
}
EXPORT_SYMBOL_GPL(mtk_typec_switch_register);

void mtk_typec_switch_unregister(struct typec_switch *sw)
{
	struct mtk_typec_switch *typec_sw;

	mutex_lock(&switch_lock);
	list_for_each_entry(typec_sw, &switch_list, list) {
		if (typec_sw->sw == sw)
			break;
	}
	list_del(&typec_sw->list);
	kfree(typec_sw);
	mutex_unlock(&switch_lock);

	typec_switch_unregister(sw);
}
EXPORT_SYMBOL_GPL(mtk_typec_switch_unregister);

static int mtk_typec_switch_set(struct typec_switch *sw,
			      enum typec_orientation orientation)
{
	struct typec_mux_switch *mux_sw = typec_switch_get_drvdata(sw);
	struct mtk_typec_switch *typec_sw;
	int ret = 0;

	dev_info(mux_sw->dev, "%s %d %d\n", __func__,
		 mux_sw->orientation, orientation);

	if (mux_sw->orientation == orientation)
		return ret;

	mutex_lock(&switch_lock);

	list_for_each_entry(typec_sw, &switch_list, list) {
		if (!IS_ERR_OR_NULL(typec_sw->sw))
			typec_sw->sw->set(typec_sw->sw, orientation);
	}

	mux_sw->orientation = orientation;

	mutex_unlock(&switch_lock);

	return ret;
}

static int proc_switch_show(struct seq_file *s, void *unused)
{
	struct typec_mux_switch *mux_sw = s->private;

	switch (mux_sw->orientation) {
	case TYPEC_ORIENTATION_NONE:
		seq_printf(s, "NONE\n");
		break;
	case TYPEC_ORIENTATION_NORMAL:
		seq_printf(s, "NORMAL\n");
		break;
	case TYPEC_ORIENTATION_REVERSE:
		seq_printf(s, "REVERSE\n");
		break;
	default:
		seq_printf(s, "INVALID\n");
	}

	seq_printf(s, "%d\n", mux_sw->orientation);
	return 0;
}

static int proc_switch_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_switch_show, PDE_DATA(inode));
}

static ssize_t proc_switch_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct typec_mux_switch *mux_sw = s->private;
	char buf[20];
	u32 tmp;

	memset(buf, 0x00, sizeof(buf));
	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (kstrtouint(buf, 0, &tmp))
		return -EINVAL;

	if (tmp > 2) {
		dev_info(mux_sw->dev, "%s %d, INVALID: %d\n", __func__,
			 mux_sw->orientation, tmp);
		return count;
	}

	mtk_typec_switch_set(mux_sw->sw, tmp);

	return count;
}

static const struct proc_ops proc_switch_fops = {
	.proc_open = proc_switch_open,
	.proc_write = proc_switch_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int typec_mux_switch_procfs_init(struct typec_mux_switch *mux_sw)
{
	struct device *dev = mux_sw->dev;
	struct proc_dir_entry *root;
	struct proc_dir_entry *file;
	int ret = 0;

	proc_mkdir(PROC_TYPEC, NULL);

	root = proc_mkdir(PROC_MUX_SWITCH, NULL);
	if (!root) {
		dev_info(dev, "failed to creat %d dir\n", PROC_MUX_SWITCH);
		return -ENOMEM;
	}

	file = proc_create_data(PROC_SWITCH, 0644,
			root, &proc_switch_fops, mux_sw);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", PROC_SWITCH);
		ret = -ENOMEM;
		goto err;
	}

	file = proc_create_data(PROC_MUX, 0644,
			root, &proc_mux_fops, mux_sw);
	if (!file) {
		dev_info(dev, "failed to creat proc file: %s\n", PROC_MUX);
		ret = -ENOMEM;
		goto err;
	}

	mux_sw->root = root;
	return 0;

err:
	proc_remove(root);
	return ret;
}

static int typec_mux_switch_procfs_exit(struct typec_mux_switch *mux_sw)
{
	proc_remove(mux_sw->root);
	return 0;
}

static int typec_mux_switch_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct typec_mux_switch *mux_sw;
	struct typec_switch_desc sw_desc = { };
	struct typec_mux_desc mux_desc = { };
	int ret = 0;

	dev_info(dev, "%s\n", __func__);

	mux_sw = kzalloc(sizeof(*mux_sw), GFP_KERNEL);
	if (!mux_sw)
		return -ENOMEM;

	mux_sw->dev = dev;

	sw_desc.drvdata = mux_sw;
	sw_desc.fwnode = dev->fwnode;
	sw_desc.set = mtk_typec_switch_set;

	mux_sw->sw = typec_switch_register(dev, &sw_desc);
	if (IS_ERR(mux_sw->sw)) {
		dev_info(dev, "error registering typec switch: %ld\n",
			PTR_ERR(mux_sw->sw));
		return PTR_ERR(mux_sw->sw);
	}

	mux_desc.drvdata = mux_sw;
	mux_desc.fwnode = dev->fwnode;
	mux_desc.set = mtk_typec_mux_set;

	mux_sw->mux = typec_mux_register(dev, &mux_desc);
	if (IS_ERR(mux_sw->mux)) {
		typec_switch_unregister(mux_sw->sw);
		dev_info(dev, "error registering typec mux: %ld\n",
			PTR_ERR(mux_sw->mux));
		return PTR_ERR(mux_sw->mux);
	}

	platform_set_drvdata(pdev, mux_sw);

	/* create procfs for half-automation switch */
	typec_mux_switch_procfs_init(mux_sw);

	dev_info(dev, "%s done\n", __func__);
	return ret;
}

static int typec_mux_switch_remove(struct platform_device *pdev)
{
	struct typec_mux_switch *mux_sw = dev_get_drvdata(&pdev->dev);

	typec_mux_switch_procfs_exit(mux_sw);
	return 0;
}

static const struct of_device_id typec_mux_switch_ids[] = {
	{.compatible = "mediatek,typec_mux_switch",},
	{},
};

static struct platform_driver typec_mux_switch_driver = {
	.probe = typec_mux_switch_probe,
	.remove = typec_mux_switch_remove,
	.driver = {
		.name = "mtk-typec-mux-switch",
		.of_match_table = typec_mux_switch_ids,
	},
};

module_platform_driver(typec_mux_switch_driver);

MODULE_DESCRIPTION("Mediatek Type-C mux switch driver");
MODULE_LICENSE("GPL");
