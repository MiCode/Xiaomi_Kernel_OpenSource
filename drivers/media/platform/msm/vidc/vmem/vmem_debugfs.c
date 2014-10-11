/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include "vmem.h"

struct vmem_debugfs_cookie {
	phys_addr_t addr;
	size_t size;
};

static int __vmem_alloc_get(void *priv, u64 *val)
{
	struct vmem_debugfs_cookie *cookie = priv;

	*val = cookie->size;
	return 0;
}

static int __vmem_alloc_set(void *priv, u64 val)
{
	struct vmem_debugfs_cookie *cookie = priv;
	int rc = 0;

	switch (val) {
	case 0: /* free */
		vmem_free(cookie->addr);
		cookie->size = 0;
		break;
	default:
		rc = vmem_allocate(val, &cookie->addr);
		cookie->size = val;
		break;
	}

	return rc;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_vmem_alloc, __vmem_alloc_get,
		__vmem_alloc_set, "%llu");

struct dentry *vmem_debugfs_init(struct platform_device *pdev)
{
	struct vmem_debugfs_cookie *alloc_cookie = NULL;
	struct dentry *debugfs_root = NULL;

	alloc_cookie = devm_kzalloc(&pdev->dev, sizeof(*alloc_cookie),
			GFP_KERNEL);
	if (!alloc_cookie)
		goto exit;

	debugfs_root = debugfs_create_dir("vmem", NULL);
	if (IS_ERR_OR_NULL(debugfs_root)) {
		pr_warn("Failed to create '<debugfs>/vmem'\n");
		debugfs_root = NULL;
		goto exit;
	}

	debugfs_create_file("alloc", S_IRUSR | S_IWUSR, debugfs_root,
			alloc_cookie, &fops_vmem_alloc);

exit:
	return debugfs_root;
}

void vmem_debugfs_deinit(struct dentry *debugfs_root)
{
	debugfs_remove_recursive(debugfs_root);
}

