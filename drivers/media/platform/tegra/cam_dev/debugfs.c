/*
 * debugfs.c
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define CAMERA_DEVICE_INTERNAL

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include <media/nvc.h>
#include <media/camera.h>

static struct camera_platform_info *cam_desc;
static const char *device_type[] = {
	"sensor",
	"focuser",
	"flash",
	"rom",
	"other1",
	"other2",
	"other3",
	"other4",
	"unsupported type"
};

static int camera_debugfs_layout(struct seq_file *s)
{
	struct cam_device_layout *layout = cam_desc->layout;
	int num = cam_desc->size_layout / sizeof(*layout);
	const char *pt;

	if (!layout)
		return -EEXIST;

	if (unlikely(num * sizeof(*layout) != cam_desc->size_layout)) {
		seq_printf(s, "WHAT? layout size mismatch: %zx vs %d\n",
			cam_desc->size_layout, num);
		return -EFAULT;
	}

	while (num--) {
		if (layout->type < ARRAY_SIZE(device_type))
			pt = device_type[layout->type];
		else
			pt = device_type[ARRAY_SIZE(device_type) - 1];
		seq_printf(s,
			"%016llx %.20s %.20s %1x %1x %.2x %1x %.8x %x %.20s\n",
			layout->guid, pt, layout->alt_name, layout->pos,
			layout->bus, layout->addr, layout->addr_byte,
			layout->dev_id, layout->index, layout->name);
		layout++;
	}

	return 0;
}

static int camera_debugfs_platform_data(struct seq_file *s)
{
	struct camera_platform_data *pd = cam_desc->pdata;

	seq_printf(s, "platform data: cfg = %x\n", pd->cfg);
	return 0;
}

static int camera_debugfs_chips(struct seq_file *s)
{
	struct camera_chip *ccp;
	const struct regmap_config *rcfg;

	seq_printf(s, "\n%s: camera devices supported:\n", cam_desc->dname);
	mutex_lock(cam_desc->c_mutex);
	list_for_each_entry(ccp, cam_desc->chip_list, list) {
		rcfg = &ccp->regmap_cfg;
		seq_printf(s, "    %.16s: type %d, ref_cnt %d. ",
			ccp->name,
			ccp->type,
			atomic_read(&ccp->ref_cnt)
			);
		seq_printf(s, "%02d bit addr, %02d bit data, stride %d, pad %d",
			rcfg->reg_bits,
			rcfg->val_bits,
			rcfg->reg_stride,
			rcfg->pad_bits
			);
		seq_printf(s, " bits. cache type: %d, flags r %02x / w %02x\n",
			rcfg->cache_type,
			rcfg->read_flag_mask,
			rcfg->write_flag_mask
			);
	}
	mutex_unlock(cam_desc->c_mutex);

	return 0;
}

static int camera_debugfs_devices(struct seq_file *s)
{
	struct camera_device *cdev;

	if (list_empty(cam_desc->dev_list)) {
		seq_printf(s, "\n%s: No device installed.\n", cam_desc->dname);
		return 0;
	}

	seq_printf(s, "\n%s: activated devices:\n", cam_desc->dname);
	mutex_lock(cam_desc->d_mutex);
	list_for_each_entry(cdev, cam_desc->dev_list, list) {
		seq_printf(s, "    %s: on %s, %s power %s\n",
			cdev->name,
			cdev->chip->name,
			atomic_read(&cdev->in_use) ? "occupied" : "free",
			cdev->is_power_on ? "on" : "off"
			);
	}
	mutex_unlock(cam_desc->d_mutex);

	return 0;
}

static int camera_debugfs_apps(struct seq_file *s)
{
	struct camera_info *user;
	int num = 0;

	if (list_empty(cam_desc->app_list)) {
		seq_printf(s, "\n%s: No App running.\n", cam_desc->dname);
		return 0;
	}

	mutex_lock(cam_desc->u_mutex);
	list_for_each_entry(user, cam_desc->app_list, list) {
		struct camera_device *cdev = user->cdev;
		struct camera_chip *ccp = NULL;

		if (cdev)
			ccp = cdev->chip;
		seq_printf(s, "app #%02d: on %s chip type: %s\n", num,
			cdev ? (char *)cdev->name : "NULL",
			ccp ? (char *)ccp->name : "NULL");
		num++;
	}
	mutex_unlock(cam_desc->u_mutex);
	seq_printf(s, "\n%s: There are %d apps running.\n",
		cam_desc->dname, num);

	return 0;
}

static int camera_status_show(struct seq_file *s, void *data)
{
	pr_info("%s %s\n", __func__, cam_desc->dname);

	if (list_empty(cam_desc->chip_list)) {
		seq_printf(s, "%s: No devices supported.\n", cam_desc->dname);
		return 0;
	}

	camera_debugfs_layout(s);
	camera_debugfs_platform_data(s);
	camera_debugfs_chips(s);
	camera_debugfs_devices(s);
	camera_debugfs_apps(s);

	return 0;
}

static ssize_t camera_attr_set(struct file *s,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[24];
	int buf_size;
	u32 val = 0;

	pr_info("%s\n", __func__);

	if (!user_buf || count <= 1)
		return -EFAULT;

	memset(buf, 0, sizeof(buf));
	buf_size = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (sscanf(buf + 1, "0x%x", &val) == 1)
		goto set_attr;
	if (sscanf(buf + 1, "0X%x", &val) == 1)
		goto set_attr;
	if (sscanf(buf + 1, "%d", &val) == 1)
		goto set_attr;

	pr_info("SYNTAX ERROR: %s\n", buf);
	return -EFAULT;

set_attr:
	pr_info("new data = %x\n", val);
	switch (buf[0]) {
	case 'd':
		cam_desc->pdata->cfg = val;
		break;
	}

	return count;
}

static int camera_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, camera_status_show, inode->i_private);
}

static const struct file_operations camera_debugfs_fops = {
	.open = camera_debugfs_open,
	.read = seq_read,
	.write = camera_attr_set,
	.llseek = seq_lseek,
	.release = single_release,
};

int camera_debugfs_init(struct camera_platform_info *info)
{
	struct dentry *d;

	dev_dbg(info->dev, "%s %s\n", __func__, info->dname);
	info->d_entry = debugfs_create_dir(
		info->miscdev.this_device->kobj.name, NULL);
	if (info->d_entry == NULL) {
		dev_err(info->dev, "%s: create dir failed\n", __func__);
		return -ENOMEM;
	}

	d = debugfs_create_file("d", S_IRUGO|S_IWUSR, info->d_entry,
		(void *)info, &camera_debugfs_fops);
	if (!d) {
		dev_err(info->dev, "%s: create file failed\n", __func__);
		debugfs_remove_recursive(info->d_entry);
		info->d_entry = NULL;
	}

	cam_desc = info;
	return -EFAULT;
}

int camera_debugfs_remove(void)
{
	if (cam_desc->d_entry)
		debugfs_remove_recursive(cam_desc->d_entry);
	cam_desc->d_entry = NULL;
	return 0;
}
