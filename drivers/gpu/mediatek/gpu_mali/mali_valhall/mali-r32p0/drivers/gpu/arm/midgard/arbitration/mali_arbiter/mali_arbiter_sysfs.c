// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT

/*
 * (C) COPYRIGHT 2020-2021 Arm Limited or its affiliates. All rights reserved.
 */

/*
 * SysFS handling for the Mali reference arbiter
 */
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include "mali_arbiter.h"
#include "mali_gpu_assign.h"
#include "mali_gpu_resource_group.h"
#include "mali_gpu_partition_config.h"

/* Maximum dynamic entry name size */
#define MALI_ARB_NAMESIZE 16

#define index_of(ptr, container) \
	(size_t)((ptr)-(container))

struct mali_arb_sysfs_slice_entry {
	struct mali_arb_sysfs *root;
	struct kobject kobj;
	struct device *dev;
};

struct mali_arb_sysfs_partition_entry {
	struct mali_arb_sysfs *root;
	struct kobject kobj;
	struct device *config_dev;
	struct kobject *slices_kobj;
};

struct mali_arb_sysfs {
	struct mali_arb *arb;
	struct device *dev;

	int (*get_slice_assignment)(struct mali_arb *arb,
		uint8_t partition_index, uint32_t *buf);
	int (*set_slice_assignment)(struct mali_arb *arb,
		uint8_t partition_index, uint32_t slices, uint32_t *old_slices);
	int (*get_aw_assignment)(struct mali_arb *arb,
		uint8_t partition_index, uint32_t *buf);
	int (*set_aw_assignment)(struct mali_arb *arb,
		uint8_t partition_index, uint32_t access_windows,
		uint32_t *old_access_windows);

	char name[MALI_ARB_NAMESIZE];
	struct kobject *kobj;
	struct kobject *slices_kobj;
	struct kobject *partitions_kobj;

	struct mali_arb_sysfs_slice_entry slices[MALI_PTM_SLICES_COUNT];
	struct mali_arb_sysfs_partition_entry
		partitions[MALI_PTM_PARTITION_COUNT];
};

struct mali_arb_sysfs_entry_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject*, char*);
	ssize_t (*store)(struct kobject*, const char*, size_t);
};

static int get_slice_partition(struct mali_arb_sysfs_slice_entry *entry)
{
	int i;
	uint32_t slices;
	const size_t slice = index_of(entry, entry->root->slices);

	for (i = 0; i < MALI_PTM_PARTITION_COUNT; ++i) {
		if (entry->root->get_slice_assignment(entry->root->arb, i,
			&slices))
			return -1;

		if (slices & (1 << slice))
			break;
	}

	/* It is valid for a slice to not be assigned to any partition */
	if (i == MALI_PTM_PARTITION_COUNT)
		i = -1;

	return i;
}

static ssize_t mali_arb_sysfs_entry_attr_show(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	int ret = -EIO;
	struct mali_arb_sysfs_entry_attr *eattr = container_of(attr,
		struct mali_arb_sysfs_entry_attr, attr);

	if (eattr->show)
		ret = eattr->show(kobj, buf);

	return ret;
}

static ssize_t mali_arb_sysfs_entry_attr_store(struct kobject *kobj,
	struct attribute *attr, const char *buf, size_t size)
{
	int ret = -EIO;
	struct mali_arb_sysfs_entry_attr *eattr = container_of(attr,
		struct mali_arb_sysfs_entry_attr, attr);

	if (eattr->store)
		ret = eattr->store(kobj, buf, size);

	return ret;
}

static const struct sysfs_ops mali_arb_sysfs_entry_ops = {
	.show = mali_arb_sysfs_entry_attr_show,
	.store = mali_arb_sysfs_entry_attr_store,
};

void mali_arb_sysfs_slice_entry_release(struct kobject *kobj)
{
	struct mali_arb_sysfs_slice_entry *entry = container_of(kobj,
		struct mali_arb_sysfs_slice_entry, kobj);
	entry->root = NULL;
}

void mali_arb_sysfs_partition_entry_release(struct kobject *kobj)
{
	struct mali_arb_sysfs_partition_entry *entry = container_of(kobj,
		struct mali_arb_sysfs_partition_entry, kobj);
	entry->root = NULL;
	entry->config_dev = NULL;
	entry->slices_kobj = NULL;
}

/* Slice entries */
static ssize_t slice_tiler_type_show(struct kobject *kobj, char *buf)
{
	enum mali_gpu_slice_tiler_type type;
	int err;
	bool is_large;
	struct mali_ptm_rg_ops *rg_ops;
	struct mali_arb_sysfs_slice_entry *entry = container_of(kobj,
		struct mali_arb_sysfs_slice_entry, kobj);

	rg_ops = dev_get_drvdata(entry->dev);

	err = rg_ops->get_slice_tiler_type(
		entry->dev,
		index_of(entry, entry->root->slices),
		&type);
	if (err)
		return err;

	is_large = type == MALI_GPU_TILER_HIGH_PERFORMANCE;
	return sprintf(buf, "%s\n", (is_large ? "Large" : "Small"));
}

static ssize_t slice_num_cores_show(struct kobject *kobj, char *buf)
{
	uint8_t cores;
	uint64_t cores_mask = 0;
	uint8_t mask_stride = 0;
	int slice_ind = 0;
	int err;
	struct mali_ptm_rg_ops *rg_ops;
	struct mali_arb_sysfs_slice_entry *entry = container_of(kobj,
		struct mali_arb_sysfs_slice_entry, kobj);

	rg_ops = dev_get_drvdata(entry->dev);

	err = rg_ops->get_slices_core_mask(entry->dev, &cores_mask,
		&mask_stride);
	if (err)
		return err;

	slice_ind = index_of(entry, entry->root->slices);

	cores = hweight64(cores_mask >> (mask_stride * slice_ind) &
		((0x1 << mask_stride) - 1));

	return sprintf(buf, "%u\n", cores);
}

static ssize_t slice_partition_show(struct kobject *kobj, char *buf)
{
	struct mali_arb_sysfs_slice_entry *entry = container_of(kobj,
		struct mali_arb_sysfs_slice_entry, kobj);
	return sprintf(buf, "%d\n", get_slice_partition(entry));
}

static ssize_t slice_enabled_show(struct kobject *kobj, char *buf)
{
	struct mali_arb_sysfs_slice_entry *entry = container_of(kobj,
		struct mali_arb_sysfs_slice_entry, kobj);
	struct mali_ptm_rg_ops *rg_ops;
	uint32_t mask;
	int err = 0;
	int slice_ind;

	rg_ops = dev_get_drvdata(entry->dev);
	if (!rg_ops || !rg_ops->get_enabled_slices_mask)
		return -ENODEV;

	err = rg_ops->get_enabled_slices_mask(entry->dev, &mask);
	if (err)
		return err;

	slice_ind = index_of(entry, entry->root->slices);
	mask = (mask & (0x1 << slice_ind)) >> slice_ind;
	return sprintf(buf, "%d\n", mask);
}

static ssize_t slice_powered_show(struct kobject *kobj, char *buf)
{
	struct mali_arb_sysfs_slice_entry *entry = container_of(kobj,
		struct mali_arb_sysfs_slice_entry, kobj);
	struct mali_ptm_rg_ops *rg_ops;
	uint32_t mask;
	int err = 0;
	int slice_ind;

	rg_ops = dev_get_drvdata(entry->dev);
	if (!rg_ops || !rg_ops->get_powered_slices_mask)
		return -ENODEV;

	err = rg_ops->get_powered_slices_mask(entry->dev, &mask);
	if (err)
		return err;

	slice_ind = index_of(entry, entry->root->slices);
	mask = (mask & (0x1 << slice_ind)) >> slice_ind;
	return sprintf(buf, "%d\n", mask);
}

static struct mali_arb_sysfs_entry_attr slice_tiler_type_attr =
	__ATTR(tiler_type, 0444, slice_tiler_type_show, NULL);
static struct mali_arb_sysfs_entry_attr slice_num_cores_attr =
	__ATTR(num_cores, 0444, slice_num_cores_show, NULL);
static struct mali_arb_sysfs_entry_attr slice_partition_attr =
	__ATTR(partition, 0444, slice_partition_show, NULL);
static struct mali_arb_sysfs_entry_attr slice_enabled_attr =
	__ATTR(enabled, 0444, slice_enabled_show, NULL);
static struct mali_arb_sysfs_entry_attr slice_powered_attr =
	__ATTR(powered, 0444, slice_powered_show, NULL);

static struct attribute *mali_arb_sysfs_slice_attributes[] = {
	&slice_tiler_type_attr.attr,
	&slice_num_cores_attr.attr,
	&slice_partition_attr.attr,
	&slice_enabled_attr.attr,
	&slice_powered_attr.attr,
	NULL,
};

static struct kobj_type mali_arb_slice_ktype = {
	.sysfs_ops = &mali_arb_sysfs_entry_ops,
	.default_attrs = mali_arb_sysfs_slice_attributes,
	.release = mali_arb_sysfs_slice_entry_release,
};

/* Partition entries */
static ssize_t partition_active_slices_show(struct kobject *kobj, char *buf)
{
	uint32_t slices;
	struct mali_arb_sysfs_partition_entry *entry = container_of(kobj,
		struct mali_arb_sysfs_partition_entry, kobj);
	const size_t p = index_of(entry, entry->root->partitions);
	const int err = entry->root->get_slice_assignment(entry->root->arb, p,
		&slices);

	if (err)
		return err;

	return sprintf(buf, "0x%X\n", slices);
}

static ssize_t partition_active_slices_store(struct kobject *kobj,
	const char *buf, size_t size)
{
	struct mali_arb_sysfs_partition_entry *entry = container_of(kobj,
		struct mali_arb_sysfs_partition_entry, kobj);
	const size_t p = index_of(entry, entry->root->partitions);
	uint32_t old_slices, slices;
	int err, i;

	/* Auto-detect numeric base */
	err = kstrtouint(buf, 0, &slices);
	if (err) {
		dev_dbg(entry->root->dev, "Failed to parse input\n");
		return err;
	}

	err = entry->root->set_slice_assignment(entry->root->arb, p, slices,
		&old_slices);
	if (err)
		return err;

	/* Update the sysfs symlinks as necessary */
	for (i = 0; i < MALI_PTM_SLICES_COUNT; ++i) {
		if ((old_slices & (1 << i)) && !(slices & (1 << i))) {
			/* Remove unused */
			sysfs_remove_link(entry->slices_kobj,
				entry->root->slices[i].kobj.name);
		} else if (!(old_slices & (1 << i)) && (slices & (1 << i))) {
			/* Add new */
			if (sysfs_create_link(entry->slices_kobj,
				&(entry->root->slices[i].kobj),
				entry->root->slices[i].kobj.name))
				dev_dbg(entry->root->dev,
					"Cannot create slice%d SysFS link\n",
					i);
		}
	}

	return size;
}

static ssize_t partition_assigned_aws_show(struct kobject *kobj, char *buf)
{
	struct mali_arb_sysfs_partition_entry *entry = container_of(kobj,
		struct mali_arb_sysfs_partition_entry, kobj);
	const size_t p = index_of(entry, entry->root->partitions);
	uint32_t aws;

	if (entry->root->get_aw_assignment(entry->root->arb, p, &aws))
		return sprintf(buf, "0x0\n");

	return sprintf(buf, "0x%X\n", aws);
}

static ssize_t partition_assigned_aws_store(struct kobject *kobj,
	const char *buf, size_t size)
{
	struct mali_arb_sysfs_partition_entry *entry = container_of(kobj,
		struct mali_arb_sysfs_partition_entry, kobj);
	const size_t p = index_of(entry, entry->root->partitions);
	uint32_t old_aws, aws;
	int err;

	/* Auto-detect numeric base */
	err = kstrtouint(buf, 0, &aws);
	if (err) {
		dev_dbg(entry->root->dev, "Failed to parse input\n");
		return err;
	}

	err = entry->root->set_aw_assignment(entry->root->arb, p, aws,
		&old_aws);
	if (err)
		return err;

	return size;
}

static struct mali_arb_sysfs_entry_attr partition_active_slices_attr =
	__ATTR(active_slices, 0664, partition_active_slices_show,
		partition_active_slices_store);

static struct mali_arb_sysfs_entry_attr partition_assigned_aws_attr =
	__ATTR(assigned_access_windows, 0664, partition_assigned_aws_show,
		partition_assigned_aws_store);

static struct attribute *mali_arb_sysfs_partition_attributes[] = {
	&partition_active_slices_attr.attr,
	&partition_assigned_aws_attr.attr,
	NULL,
};

static struct kobj_type mali_arb_partition_ktype = {
	.sysfs_ops = &mali_arb_sysfs_entry_ops,
	.default_attrs = mali_arb_sysfs_partition_attributes,
	.release = mali_arb_sysfs_partition_entry_release,
};

struct mali_arb_sysfs *mali_arb_sysfs_create_root(struct mali_arb *arb,
	struct device *dev,
	int (*get_slice_assignment)(struct mali_arb*, uint8_t, uint32_t*),
	int (*set_slice_assignment)(struct mali_arb*, uint8_t, uint32_t,
		uint32_t*),
	int (*get_aw_assignment)(struct mali_arb*, uint8_t, uint32_t*),
	int (*set_aw_assignment)(struct mali_arb*, uint8_t, uint32_t,
		uint32_t*))
{
	struct mali_arb_sysfs *root;

	root = kzalloc(sizeof(struct mali_arb_sysfs), GFP_KERNEL);
	if (!root) {
		dev_dbg(dev, "Failed to allocate mali_arb_sysfs\n");
		return NULL;
	}

	root->arb = arb;
	root->dev = dev;
	root->get_slice_assignment = get_slice_assignment;
	root->set_slice_assignment = set_slice_assignment;
	root->get_aw_assignment = get_aw_assignment;
	root->set_aw_assignment = set_aw_assignment;

	/* Create the root */
	if (snprintf(root->name, MALI_ARB_NAMESIZE, "arbiter") < 0)
		return NULL;

	root->kobj = kobject_create_and_add(root->name, &dev->kobj);
	if (!root->kobj) {
		dev_dbg(dev, "Failed to allocate sysfs root\n");
		goto clean_up_root;
	}

	/* Create the subfolders */
	root->slices_kobj = kobject_create_and_add("slices", root->kobj);
	if (!root->slices_kobj) {
		dev_dbg(dev, "Failed to allocate sysfs slice root\n");
		goto clean_up_kobj;
	}
	root->partitions_kobj = kobject_create_and_add("partitions",
		root->kobj);
	if (!root->partitions_kobj) {
		dev_dbg(dev, "Failed to allocate sysfs partitions root\n");
		goto clean_up_slices;
	}

	return root;

clean_up_slices:
	kobject_put(root->slices_kobj);
clean_up_kobj:
	kobject_put(root->kobj);
clean_up_root:
	kfree(root);
	return NULL;
}

void mali_arb_sysfs_destroy_root(struct mali_arb_sysfs *root)
{
	if (!root)
		return;

	if (root->partitions_kobj)
		kobject_put(root->partitions_kobj);
	if (root->slices_kobj)
		kobject_put(root->slices_kobj);
	if (root->kobj)
		kobject_put(root->kobj);

	kfree(root);
}

int mali_arb_sysfs_add_slice(struct mali_arb_sysfs *data, uint8_t index,
	struct device *dev)
{
	struct mali_arb_sysfs_slice_entry *entry;
	int err;

	if (!data || (index >= MALI_PTM_SLICES_COUNT) || !dev ||
		data->slices[index].dev)
		return -EINVAL;

	/* Create the slice group */
	entry = &(data->slices[index]);
	entry->root = data;
	entry->dev = dev;
	err = kobject_init_and_add(&entry->kobj, &mali_arb_slice_ktype,
		data->slices_kobj, "slice%u", index);
	if (err) {
		dev_dbg(data->dev, "Failed to init slice%u sysfs entry\n",
			index);
		goto clean_up;
	}

	return 0;

clean_up:
	kobject_put(&entry->kobj);
	return err;
}

int mali_arb_sysfs_add_partition(struct mali_arb_sysfs *data, uint8_t index,
	struct device *config_dev)
{
	struct mali_arb_sysfs_partition_entry *entry;
	int err;

	if (!data || (index >= MALI_PTM_PARTITION_COUNT) || !config_dev ||
		data->partitions[index].config_dev)
		return -EINVAL;

	/* Create the partition group */
	entry = &(data->partitions[index]);
	entry->root = data;
	entry->config_dev = config_dev;
	err = kobject_init_and_add(&entry->kobj, &mali_arb_partition_ktype,
		data->partitions_kobj, "partition%u", index);
	if (err) {
		dev_dbg(data->dev,
			"Failed to init partition%u sysfs entry\n", index);
		goto clean_up;
	}

	entry->slices_kobj = kobject_create_and_add("slices", &entry->kobj);
	if (!entry->slices_kobj) {
		dev_dbg(data->dev,
			"Failed to init partition%u/slices sysfs entry\n",
			index);
		err = -ENOMEM;
		goto clean_up;
	}

	return 0;

clean_up:
	kobject_put(&entry->kobj);
	kobject_put(entry->slices_kobj);
	return err;
}

void mali_arb_sysfs_free(struct mali_arb_sysfs *data)
{
	int i;

	if (WARN_ON(!data))
		return;

	for (i = 0; i < MALI_PTM_SLICES_COUNT; ++i) {
		if (data->slices[i].root)
			kobject_put(&(data->slices[i].kobj));
	}

	for (i = 0; i < MALI_PTM_PARTITION_COUNT; ++i) {
		if (!data->partitions[i].root)
			continue;

		kobject_put(data->partitions[i].slices_kobj);
		kobject_put(&(data->partitions[i].kobj));
	}

	mali_arb_sysfs_destroy_root(data);
}
