// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>

#include <linux/mfd/mt6360.h>

#define MT6360_DBG_TYPESTR	"I2C,MT6360"

struct dbg_internal {
	struct dentry *rt_root;
	struct dentry *ic_root;
	bool rt_dir_create;
	struct mutex io_lock;
	u16 reg;
	u16 size;
	u16 data_buffer_size;
	void *data_buffer;
};

struct dbg_info {
	const char *dirname;
	const char *devname;
	const char *typestr;
	void *io_drvdata;
	int (*io_read)(void *drvdata, u16 reg, void *val, u16 size);
	int (*io_write)(void *drvdata, u16 reg, const void *val, u16 size);
	struct dbg_internal internal;
};

struct mt6360_dbg_info {
	struct device *dev;
	struct dbg_info dbg_info[MT6360_SLAVE_MAX];
};

#ifdef CONFIG_DEBUG_FS
/* reg/size/data/bustype */
#define PREALLOC_RBUFFER_SIZE	(32)
#define PREALLOC_WBUFFER_SIZE	(1000)

static int data_debug_show(struct seq_file *s, void *data)
{
	struct dbg_info *di = s->private;
	struct dbg_internal *d = &di->internal;
	void *buffer;
	u8 *pdata;
	int i, ret;

	if (d->data_buffer_size < d->size) {
		buffer = kzalloc(d->size, GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
		kfree(d->data_buffer);
		d->data_buffer = buffer;
		d->data_buffer_size = d->size;
	}
	/* read transfer */
	if (!di->io_read)
		return -EPERM;
	ret = di->io_read(di->io_drvdata, d->reg, d->data_buffer, d->size);
	if (ret < 0)
		return ret;
	pdata = d->data_buffer;
	seq_puts(s, "0x");
	for (i = 0; i < d->size; i++)
		seq_printf(s, "%02x,", *(pdata + i));
	seq_puts(s, "\n");
	return 0;
}

static int data_debug_open(struct inode *inode, struct file *file)
{
	if (file->f_mode & FMODE_READ)
		return single_open(file, data_debug_show, inode->i_private);
	return simple_open(inode, file);
}

static ssize_t data_debug_write(struct file *file,
				const char __user *user_buf,
				size_t cnt, loff_t *loff)
{
	struct dbg_info *di = file->private_data;
	struct dbg_internal *d = &di->internal;
	void *buffer;
	u8 *pdata;
	char buf[PREALLOC_WBUFFER_SIZE + 1], *token, *cur;
	int val_cnt = 0, ret;

	if (cnt > PREALLOC_WBUFFER_SIZE)
		return -ENOMEM;
	if (copy_from_user(buf, user_buf, cnt))
		return -EFAULT;
	/* buffer size check */
	if (d->data_buffer_size < d->size) {
		buffer = kzalloc(d->size, GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
		kfree(d->data_buffer);
		d->data_buffer = buffer;
		d->data_buffer_size = d->size;
	}
	/* data parsing */
	cur = buf;
	pdata = d->data_buffer;
	while ((token = strsep(&cur, ",\n")) != NULL) {
		if (!*token)
			break;
		if (val_cnt++ >= d->size)
			break;
		if (kstrtou8(token, 16, pdata++))
			return -EINVAL;
	}
	if (val_cnt != d->size)
		return -EINVAL;
	/* write transfer */
	if (!di->io_write)
		return -EPERM;
	ret = di->io_write(di->io_drvdata, d->reg, d->data_buffer, d->size);
	return (ret < 0) ? ret : cnt;
}

static int data_debug_release(struct inode *inode, struct file *file)
{
	if (file->f_mode & FMODE_READ)
		return single_release(inode, file);
	return 0;
}

static const struct file_operations data_debug_fops = {
	.open = data_debug_open,
	.read = seq_read,
	.write = data_debug_write,
	.llseek = seq_lseek,
	.release = data_debug_release,
};

static int type_debug_show(struct seq_file *s, void *data)
{
	struct dbg_info *di = s->private;

	seq_printf(s, "%s,%s\n", di->typestr, di->devname);
	return 0;
}

static int type_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, type_debug_show, inode->i_private);
}

static const struct file_operations type_debug_fops = {
	.open = type_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static ssize_t lock_debug_read(struct file *file,
			       char __user *user_buf, size_t cnt, loff_t *loff)
{
	struct dbg_info *di = file->private_data;
	struct dbg_internal *d = &di->internal;
	char buf[10];

	snprintf(buf, sizeof(buf), "%d\n", mutex_is_locked(&d->io_lock));
	return simple_read_from_buffer(user_buf, cnt, loff, buf, strlen(buf));
}

static ssize_t lock_debug_write(struct file *file,
				const char __user *user_buf,
				size_t cnt, loff_t *loff)
{
	struct dbg_info *di = file->private_data;
	struct dbg_internal *d = &di->internal;
	u32 lock;
	int ret;

	ret = kstrtou32_from_user(user_buf, cnt, 0, &lock);
	if (ret < 0)
		return ret;
	lock ? mutex_lock(&d->io_lock) : mutex_unlock(&d->io_lock);
	return cnt;
}

static const struct file_operations lock_debug_fops = {
	.open = simple_open,
	.read = lock_debug_read,
	.write = lock_debug_write,
};

static int generic_debugfs_init(struct dbg_info *di)
{
	struct dbg_internal *d = &di->internal;

	/* valid check */
	if (!di->dirname || !di->devname || !di->typestr)
		return -EINVAL;
	d->data_buffer_size = PREALLOC_RBUFFER_SIZE;
	d->data_buffer = kzalloc(PREALLOC_RBUFFER_SIZE, GFP_KERNEL);
	if (!d->data_buffer)
		return -ENOMEM;
	/* create debugfs */
	d->rt_root = debugfs_lookup("ext_dev_io", NULL);
	if (!d->rt_root) {
		d->rt_root = debugfs_create_dir("ext_dev_io", NULL);
		if (!d->rt_root)
			return -ENODEV;
		d->rt_dir_create = true;
	}
	d->ic_root = debugfs_create_dir(di->dirname, d->rt_root);
	if (!d->ic_root)
		goto err_cleanup_rt;
	if (!debugfs_create_u16("reg", 0644, d->ic_root, &d->reg))
		goto err_cleanup_ic;
	if (!debugfs_create_u16("size", 0644, d->ic_root, &d->size))
		goto err_cleanup_ic;
	if (!debugfs_create_file("data", 0644,
				 d->ic_root, di, &data_debug_fops))
		goto err_cleanup_ic;
	if (!debugfs_create_file("type", 0444,
				 d->ic_root, di, &type_debug_fops))
		goto err_cleanup_ic;
	if (!debugfs_create_file("lock", 0644,
				 d->ic_root, di, &lock_debug_fops))
		goto err_cleanup_ic;
	mutex_init(&d->io_lock);
	return 0;
err_cleanup_ic:
	debugfs_remove_recursive(d->ic_root);
err_cleanup_rt:
	if (d->rt_dir_create)
		debugfs_remove_recursive(d->rt_root);
	kfree(d->data_buffer);
	return -ENODEV;
}

static void generic_debugfs_exit(struct dbg_info *di)
{
	struct dbg_internal *d = &di->internal;

	mutex_destroy(&d->io_lock);
	debugfs_remove_recursive(d->ic_root);
	if (d->rt_dir_create)
		debugfs_remove_recursive(d->rt_root);
	kfree(d->data_buffer);
}
#else
static inline int generic_debugfs_init(struct dbg_info *di)
{
	return 0;
}

static inline void generic_debugfs_exit(struct dbg_info *di) {}
#endif

static int mt6360_dbg_io_read(void *drvdata, u16 reg, void *val, u16 size)
{
	return regmap_bulk_read((struct regmap *)drvdata, reg, val, size);
}

static int mt6360_dbg_io_write(void *drvdata,
			       u16 reg, const void *val, u16 size)
{
	return regmap_bulk_write((struct regmap *)drvdata, reg, val, size);
}

static int mt6360_dbg_slave_register(struct device *parent,
				     struct i2c_client *i2c,
				     struct dbg_info *di)
{
	struct device *dev = &i2c->dev;
	struct regmap *regmap = dev_get_regmap(dev, NULL);

	if (IS_ERR_OR_NULL(regmap))
		return -EINVAL;
	di->dirname = devm_kasprintf(parent, GFP_KERNEL,
				     "MT6360.%s", dev_name(dev));
	di->devname = dev_name(dev);
	di->typestr = MT6360_DBG_TYPESTR;
	di->io_drvdata = regmap;
	di->io_read = mt6360_dbg_io_read;
	di->io_write = mt6360_dbg_io_write;
	return generic_debugfs_init(di);
}

static int mt6360_dbg_probe(struct platform_device *pdev)
{
	struct mt6360_pmu_info *mpi = dev_get_drvdata(pdev->dev.parent);
	struct mt6360_dbg_info *mdi;
	int i, ret;

	mdi = devm_kzalloc(&pdev->dev, sizeof(*mdi), GFP_KERNEL);
	if (!mdi)
		return -ENOMEM;
	mdi->dev = &pdev->dev;
	platform_set_drvdata(pdev, mdi);

	for (i = 0; i < MT6360_SLAVE_MAX; i++) {
		ret = mt6360_dbg_slave_register(&pdev->dev,
						mpi->i2c[i], &mdi->dbg_info[i]);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to register [%d] dbg\n", i);
			return ret;
		}
	}
	dev_info(&pdev->dev, "%s successfully\n", __func__);
	return 0;
}

static int mt6360_dbg_remove(struct platform_device *pdev)
{
	struct mt6360_dbg_info *mdi = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < MT6360_SLAVE_MAX; i++)
		generic_debugfs_exit(&mdi->dbg_info[i]);
	return 0;
}

static const struct platform_device_id mt6360_dbg_id[] = {
	{ "mt6360_dbg", 0 },
	{},
};
MODULE_DEVICE_TABLE(platform, mt6360_dbg_id);

static struct platform_driver mt6360_dbg_driver = {
	.driver = {
		.name = "mt6360_dbg",
	},
	.probe = mt6360_dbg_probe,
	.remove = mt6360_dbg_remove,
	.id_table = mt6360_dbg_id,
};
module_platform_driver(mt6360_dbg_driver);

MODULE_AUTHOR("CY Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6360 Dbg Driver");
MODULE_LICENSE("GPL");
