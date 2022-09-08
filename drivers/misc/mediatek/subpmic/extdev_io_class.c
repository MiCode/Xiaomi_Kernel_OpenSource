// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/extdev_io_class.h>

#define PREALLOC_RBUFFER_SIZE	(32)
#define PREALLOC_WBUFFER_SIZE	(512)

static struct class *extdev_io_class;

static ssize_t extdev_io_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf);
static ssize_t extdev_io_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count);

#define EXTDEV_IO_DEVICE_ATTR(_name, _mode) 		\
{							\
	.attr = { .name = #_name, .mode = _mode },	\
	.show = extdev_io_show,				\
	.store = extdev_io_store,			\
}

static struct device_attribute extdev_io_device_attributes[] = {
	EXTDEV_IO_DEVICE_ATTR(reg, 0644),
	EXTDEV_IO_DEVICE_ATTR(size, 0644),
	EXTDEV_IO_DEVICE_ATTR(data, 0644),
	EXTDEV_IO_DEVICE_ATTR(type, 0444),
	EXTDEV_IO_DEVICE_ATTR(lock, 0644),
};

enum {
	EXTDEV_IO_DESC_REG,
	EXTDEV_IO_DESC_SIZE,
	EXTDEV_IO_DESC_DATA,
	EXTDEV_IO_DESC_TYPE,
	EXTDEV_IO_DESC_LOCK,
};

static int extdev_io_read(struct extdev_io_device *extdev, char *buf)
{
	int cnt = 0, i, ret;
	void *buffer;
	u8 *data;
	struct extdev_desc *desc = extdev->desc;

	if (extdev->data_buffer_size < extdev->size) {
		buffer = kzalloc(extdev->size, GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
		kfree(extdev->data_buffer);
		extdev->data_buffer = buffer;
		extdev->data_buffer_size = extdev->size;
	}
	/* read transfer */
	if (!desc->io_read)
		return -EPERM;
	ret = desc->io_read(desc->rmap, extdev->reg, extdev->data_buffer,
			    extdev->size);
	if (ret < 0)
		return ret;
	data = extdev->data_buffer;
	cnt = snprintf(buf + cnt, 256, "0x");
	if (cnt >= 256)
		goto err;
	for (i = 0; i < extdev->size; i++) {
		cnt += snprintf(buf + cnt, 256, "%02x,", *(data + i));
		if (cnt >= 256)
			goto err;
	}
	cnt += snprintf(buf + cnt, 256, "\n");
	if (cnt >= 256)
		goto err;
	return ret;
err:
	pr_notice("%s: the string is been truncated\n", __func__);
	return -EINVAL;
}

static int extdev_io_write(struct extdev_io_device *extdev, const char *buf_internal, size_t cnt)
{
	void *buffer;
	u8 *pdata;
	char buf[PREALLOC_WBUFFER_SIZE + 1], *token, *cur;
	int val_cnt = 0, ret;
	struct extdev_desc *desc = extdev->desc;

	if (cnt > PREALLOC_WBUFFER_SIZE)
		return -ENOMEM;
	memcpy(buf, buf_internal, cnt);
	buf[cnt] = 0;
	/* buffer size check */
	if (extdev->data_buffer_size < extdev->size) {
		buffer = kzalloc(extdev->size, GFP_KERNEL);
		if (!buffer)
			return -ENOMEM;
		kfree(extdev->data_buffer);
		extdev->data_buffer = buffer;
		extdev->data_buffer_size = extdev->size;
	}
	/* data parsing */
	cur = buf;
	pdata = extdev->data_buffer;
	while ((token = strsep(&cur, ",\n")) != NULL) {
		if (!*token)
			break;
		if (val_cnt++ >= extdev->size)
			break;
		if (kstrtou8(token, 16, pdata++))
			return -EINVAL;
	}
	if (val_cnt != extdev->size)
		return -EINVAL;
	/* write transfer */
	if (!desc->io_write)
		return -EPERM;
	ret = desc->io_write(desc->rmap, extdev->reg, extdev->data_buffer, extdev->size);
	return (ret < 0) ? ret : cnt;
}

static ssize_t extdev_io_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct extdev_io_device *extdev = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - extdev_io_device_attributes;
	int ret = 0;

	mutex_lock(&extdev->io_lock);
	switch (offset) {
	case EXTDEV_IO_DESC_REG:
		ret = snprintf(buf, 256, "0x%04x\n", extdev->reg);
		if (ret >= 256)
			pr_notice("%s: the string is been truncated\n", __func__);
		break;
	case EXTDEV_IO_DESC_SIZE:
		ret = snprintf(buf, 256, "%d\n", extdev->size);
		if (ret >= 256)
			pr_notice("%s: the string is been truncated\n", __func__);
		break;
	case EXTDEV_IO_DESC_DATA:
		ret = extdev_io_read(extdev, buf);
		break;
	case EXTDEV_IO_DESC_TYPE:
		ret = snprintf(buf, 256, "%s\n", extdev->desc->typestr);
		if (ret >= 256)
			pr_notice("%s: the string is been truncated\n", __func__);
		break;
	case EXTDEV_IO_DESC_LOCK:
		ret = snprintf(buf, 256, "%d\n", extdev->access_lock);
		if (ret >= 256)
			pr_notice("%s: the string is been truncated\n", __func__);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&extdev->io_lock);
	return ret < 0 ? ret : strlen(buf);
}

static int get_parameters(char *buf, long int *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtoul(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
			}
		else
			return -EINVAL;
	}
	return 0;
}

static ssize_t extdev_io_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct extdev_io_device *extdev = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - extdev_io_device_attributes;
	long val = 0;
	int ret = 0;

	mutex_lock(&extdev->io_lock);
	switch (offset) {
	case EXTDEV_IO_DESC_REG:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0)
			break;
		extdev->reg = val;
		break;
	case EXTDEV_IO_DESC_SIZE:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0)
			break;
		extdev->size = val;
		break;
	case EXTDEV_IO_DESC_DATA:
		ret = extdev_io_write(extdev, buf, count);
		break;
	case EXTDEV_IO_DESC_LOCK:
		ret = get_parameters((char *)buf, &val, 1);
		if (ret < 0)
			break;
		if (!!val == extdev->access_lock)
			ret = -EFAULT;
		else
			extdev->access_lock = !!val;
		break;
	default:
		ret = -EINVAL;
	}
	mutex_unlock(&extdev->io_lock);
	return ret < 0 ? ret : count;
}

struct extdev_io_device * extdev_io_device_register(struct device *parent,
						    struct extdev_desc *desc)
{
	struct extdev_io_device *extdev;

	if (!parent) {
		pr_err("%s: Expected proper parent device\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	if (!desc || !desc->devname || !desc->typestr || !desc->rmap)
		return ERR_PTR(-EINVAL);

	extdev = devm_kzalloc(parent, sizeof(*extdev), GFP_KERNEL);
	if (!extdev) {
		pr_err("%s: failed to alloc extdev\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	extdev->desc = desc;
	mutex_init(&extdev->io_lock);

	/* for MTK engineer setting */
	extdev->size = 1;

	extdev->data_buffer_size = PREALLOC_RBUFFER_SIZE;
	extdev->data_buffer = kzalloc(PREALLOC_RBUFFER_SIZE, GFP_KERNEL);
	if (!extdev->data_buffer) {
		kfree(extdev);
		return ERR_PTR(-ENOMEM);
	}

	extdev->dev = device_create_with_groups(extdev_io_class, parent, 0, extdev, NULL, "%s",
						desc->dirname);
	if (IS_ERR(extdev->dev)) {
		kfree(extdev->data_buffer);
		return ERR_CAST(extdev->dev);
	}

	return extdev;
}
EXPORT_SYMBOL_GPL(extdev_io_device_register);

void extdev_io_device_unregister(struct extdev_io_device *extdev)
{
	device_unregister(extdev->dev);
	kfree(extdev->data_buffer);
	mutex_destroy(&extdev->io_lock);
}
EXPORT_SYMBOL_GPL(extdev_io_device_unregister);

static void devm_extdev_io_device_release(struct device *dev, void *res)
{
	struct extdev_io_device **extdev = res;

	extdev_io_device_unregister(*extdev);
}

struct extdev_io_device * devm_extdev_io_device_register(struct device *parent,
							 struct extdev_desc *desc)
{
	struct extdev_io_device **ptr, *extdev;

	ptr = devres_alloc(devm_extdev_io_device_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	extdev = extdev_io_device_register(parent, desc);
	if (IS_ERR(extdev)) {
		devres_free(ptr);
	} else {
		*ptr = extdev;
		devres_add(parent, ptr);
	}

	return extdev;
}
EXPORT_SYMBOL_GPL(devm_extdev_io_device_register);

static struct attribute *extdev_io_class_attrs[] = {
	&extdev_io_device_attributes[0].attr,
	&extdev_io_device_attributes[1].attr,
	&extdev_io_device_attributes[2].attr,
	&extdev_io_device_attributes[3].attr,
	&extdev_io_device_attributes[4].attr,
	NULL,
};

static const struct attribute_group extdev_io_attr_group = {
	.attrs = extdev_io_class_attrs,
};

static const struct attribute_group *extdev_io_attr_groups[] = {
	&extdev_io_attr_group,
	NULL
};

static int __init extdev_io_class_init(void)
{
	pr_info("%s\n", __func__);
	extdev_io_class = class_create(THIS_MODULE, "extdev_io");
	if (IS_ERR(extdev_io_class)) {
		pr_err("Unable to create extdev_io class(%d)\n", PTR_ERR(extdev_io_class));
		return PTR_ERR(extdev_io_class);
	}

	extdev_io_class->dev_groups = extdev_io_attr_groups;
	pr_info("extdev_io class init OK\n");
	return 0;
}

static void __exit extdev_io_class_exit(void)
{
	class_destroy(extdev_io_class);
	pr_info("extdev_io class deinit OK\n");
}

subsys_initcall(extdev_io_class_init);
module_exit(extdev_io_class_exit);

MODULE_DESCRIPTION("Extdev io class");
MODULE_AUTHOR("Gene Chen <gene_chen@richtek.com>");
MODULE_LICENSE("GPL");
