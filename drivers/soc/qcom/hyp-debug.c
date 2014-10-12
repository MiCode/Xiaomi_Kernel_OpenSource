/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/hvc.h>

#include "peripheral-loader.h"

#define hyp_dbg_err(fmt, ...)						\
	pr_err("%s: " fmt, "hyp-debug", ##__VA_ARGS__)
#define hyp_dbg_info(fmt, ...)						\
	pr_info("%s: " fmt, "hyp-debug", ##__VA_ARGS__)

#define HVC_FN_DBG_MAP_RANGE			HVC_FN_SIP(1)
#define HVC_FN_DBG_UNMAP_RANGE			HVC_FN_SIP(2)

#define MEM_PERM_EXECUTE			BIT(0)
#define MEM_PERM_WRITE				BIT(1)
#define MEM_PERM_READ				BIT(2)

#define MEM_CACHE_nGnRnE			0x0
#define MEM_CACHE_nGnRE				0x1
#define MEM_CACHE_nGRE				0x2
#define MEM_CACHE_GRE				0x3
#define MEM_CACHE_ONC_INC			0x5
#define MEM_CACHE_ONC_IWT			0x6
#define MEM_CACHE_ONC_IWB			0x7
#define MEM_CACHE_OWT_INC			0x9
#define MEM_CACHE_OWT_IWT			0xA
#define MEM_CACHE_OWT_IWB			0xB
#define MEM_CACHE_OWB_INC			0xD
#define MEM_CACHE_OWB_IWT			0xE
#define MEM_CACHE_OWB_IWB			0xF

#define MEM_SHARE_NS				0x0
#define MEM_SHARE_OS				0x2
#define MEM_SHARE_IS				0x3

static u64 mem_addr, mem_size, mem_perm_attr, mem_cache_attr, mem_share_attr;

static int hyp_debug_mem_addr_get(void *data, u64 *val)
{
	*val = mem_addr;
	return 0;
}

static int hyp_debug_mem_addr_set(void *data, u64 val)
{
	mem_addr = val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(hyp_debug_mem_addr_fops, hyp_debug_mem_addr_get,
			hyp_debug_mem_addr_set, "%llu\n");

static int hyp_debug_mem_size_get(void *data, u64 *val)
{
	*val = mem_size;
	return 0;
}

static int hyp_debug_mem_size_set(void *data, u64 val)
{
	mem_size = val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(hyp_debug_mem_size_fops, hyp_debug_mem_size_get,
			hyp_debug_mem_size_set, "%llu\n");

static int hyp_debug_mem_perm_attr_get(void *data, u64 *val)
{
	*val = mem_perm_attr;
	return 0;
}

static int hyp_debug_mem_perm_attr_set(void *data, u64 val)
{
	mem_perm_attr = val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(hyp_debug_mem_perm_attr_fops,
			hyp_debug_mem_perm_attr_get,
			hyp_debug_mem_perm_attr_set, "%llu\n");

static int hyp_debug_mem_cache_attr_get(void *data, u64 *val)
{
	*val = mem_cache_attr;
	return 0;
}

static int hyp_debug_mem_cache_attr_set(void *data, u64 val)
{
	mem_cache_attr = val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(hyp_debug_mem_cache_attr_fops,
			hyp_debug_mem_cache_attr_get,
			hyp_debug_mem_cache_attr_set, "%llu\n");

static int hyp_debug_mem_share_attr_get(void *data, u64 *val)
{
	*val = mem_share_attr;
	return 0;
}

static int hyp_debug_mem_share_attr_set(void *data, u64 val)
{
	mem_share_attr = val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(hyp_debug_mem_share_attr_fops,
			hyp_debug_mem_share_attr_get,
			hyp_debug_mem_share_attr_set, "%llu\n");

static int hyp_debug_mem_map_set(void *data, u64 val)
{
	struct hvc_desc desc = { {0}, {0} };
	int ret;

	desc.arg[0] = mem_addr;
	desc.arg[1] = mem_size;
	desc.arg[2] = mem_perm_attr;
	desc.arg[3] = mem_cache_attr;
	desc.arg[4] = mem_share_attr;
	ret = hvc(HVC_FN_DBG_MAP_RANGE, &desc);
	if (ret)
		hyp_dbg_err("user specified hvc map range failed: %d\n", ret);

	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(hyp_debug_mem_map_fops, NULL, hyp_debug_mem_map_set,
			"%llu\n");

static int hyp_debug_mem_unmap_set(void *data, u64 val)
{
	struct hvc_desc desc = { {0}, {0} };
	int ret;

	desc.arg[0] = mem_addr;
	desc.arg[1] = mem_size;
	ret = hvc(HVC_FN_DBG_UNMAP_RANGE, &desc);
	if (ret)
		hyp_dbg_err("user specified hvc unmap range failed: %d\n", ret);

	return ret;
}
DEFINE_SIMPLE_ATTRIBUTE(hyp_debug_mem_unmap_fops, NULL, hyp_debug_mem_unmap_set,
			"%llu\n");

struct restart_notifier_block {
	struct pil_image_info __iomem *pil_info;
	struct notifier_block nb;
};

static struct restart_notifier_block *restart_nbs;

static int restart_notifier_cb(struct notifier_block *this, unsigned long code,
			       void *data)
{
	struct restart_notifier_block *restart_nb;
	struct hvc_desc desc = { {0}, {0} };
	u64 addr;
	u32 size;
	int ret;

	restart_nb = container_of(this, struct restart_notifier_block, nb);

	switch (code) {
	case SUBSYS_AFTER_POWERUP:
		memcpy_fromio(&addr, &restart_nb->pil_info->start, sizeof(u64));
		size = readl_relaxed(&restart_nb->pil_info->size);
		desc.arg[0] = addr;
		desc.arg[1] = size;
		ret = hvc(HVC_FN_DBG_UNMAP_RANGE, &desc);
		if (ret)
			hyp_dbg_err("subsys hvc unmap range failed: %lu %d\n",
				    code, ret);
		break;
	case SUBSYS_BEFORE_SHUTDOWN:
		memcpy_fromio(&addr, &restart_nb->pil_info->start, sizeof(u64));
		size = readl_relaxed(&restart_nb->pil_info->size);
		desc.arg[0] = addr;
		desc.arg[1] = size;
		desc.arg[2] = MEM_PERM_EXECUTE | MEM_PERM_WRITE | MEM_PERM_READ;
		desc.arg[3] = MEM_CACHE_OWB_IWB;
		desc.arg[4] = MEM_SHARE_NS;
		ret = hvc(HVC_FN_DBG_MAP_RANGE, &desc);
		if (ret)
			hyp_dbg_err("subsys hvc map range failed: %lu %d\n",
				    code, ret);
		break;
	}

	return NOTIFY_DONE;
}

static int __init hyp_debug_init(void)
{
	struct device_node *np;
	struct resource res;
	struct dentry *debugfs_dir, *debugfs_file;
	char subsys_name[FIELD_SIZEOF(struct pil_image_info, name)];
	void __iomem *pil_info_base, __iomem *addr;
	void *handle;
	u32 nr_restart_nb;
	int i, ret;

	np = of_find_compatible_node(NULL, NULL, "qcom,msm-imem-pil");
	if (!np) {
		hyp_dbg_err("pil imem DT node does not exist\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(np, 0, &res);
	if (ret)
		return ret;

	pil_info_base = ioremap(res.start, resource_size(&res));
	if (!pil_info_base) {
		hyp_dbg_err("pil info imem base offset mapping failed\n");
		return -ENOMEM;
	}

	nr_restart_nb = resource_size(&res) / sizeof(struct pil_image_info);
	restart_nbs = kzalloc(nr_restart_nb *
			     sizeof(struct restart_notifier_block), GFP_KERNEL);
	if (!restart_nbs) {
		ret = -ENOMEM;
		hyp_dbg_err("restart notifiers allocation failed\n");
		goto err0;
	}

	for (i = 0; i < nr_restart_nb; i++) {
		addr = pil_info_base + sizeof(struct pil_image_info) * i;
		restart_nbs[i].pil_info = (struct pil_image_info __iomem *)addr;

		memcpy_fromio(subsys_name, restart_nbs[i].pil_info->name,
			      sizeof(restart_nbs[i].pil_info->name));
		if (subsys_name[0] == '\0')
			break;

		restart_nbs[i].nb.notifier_call = restart_notifier_cb;
		handle = subsys_notif_register_notifier(subsys_name,
							&restart_nbs[i].nb);
		if (IS_ERR_OR_NULL(handle)) {
			ret = PTR_ERR(handle);
			hyp_dbg_err("subsys notif register %d failed: %d\n", i,
				    ret);
			goto err1;
		}
	}

	mem_perm_attr = MEM_PERM_EXECUTE | MEM_PERM_WRITE | MEM_PERM_READ;
	mem_cache_attr = MEM_CACHE_OWB_IWB;
	mem_share_attr = MEM_SHARE_NS;

	debugfs_dir = debugfs_create_dir("hyp_debug", NULL);
	if (IS_ERR_OR_NULL(debugfs_dir)) {
		ret = PTR_ERR(debugfs_dir);
		goto err1;
	}

	debugfs_file = debugfs_create_file("mem_addr", S_IRUGO, debugfs_dir,
					   NULL, &hyp_debug_mem_addr_fops);
	if (IS_ERR_OR_NULL(debugfs_file)) {
		ret = PTR_ERR(debugfs_file);
		goto err2;
	}

	debugfs_file = debugfs_create_file("mem_size", S_IRUGO, debugfs_dir,
					   NULL, &hyp_debug_mem_size_fops);
	if (IS_ERR_OR_NULL(debugfs_file)) {
		ret = PTR_ERR(debugfs_file);
		goto err2;
	}

	debugfs_file = debugfs_create_file("mem_perm_attr", S_IRUGO,
					   debugfs_dir, NULL,
					   &hyp_debug_mem_perm_attr_fops);
	if (IS_ERR_OR_NULL(debugfs_file)) {
		ret = PTR_ERR(debugfs_file);
		goto err2;
	}

	debugfs_file = debugfs_create_file("mem_cache_attr", S_IRUGO,
					   debugfs_dir, NULL,
					   &hyp_debug_mem_cache_attr_fops);
	if (IS_ERR_OR_NULL(debugfs_file)) {
		ret = PTR_ERR(debugfs_file);
		goto err2;
	}

	debugfs_file = debugfs_create_file("mem_share_attr", S_IRUGO,
					   debugfs_dir, NULL,
					   &hyp_debug_mem_share_attr_fops);
	if (IS_ERR_OR_NULL(debugfs_file)) {
		ret = PTR_ERR(debugfs_file);
		goto err2;
	}

	debugfs_file = debugfs_create_file("mem_map", S_IRUGO, debugfs_dir,
					   NULL, &hyp_debug_mem_map_fops);
	if (IS_ERR_OR_NULL(debugfs_file)) {
		ret = PTR_ERR(debugfs_file);
		goto err2;
	}

	debugfs_file = debugfs_create_file("mem_unmap", S_IRUGO, debugfs_dir,
					   NULL, &hyp_debug_mem_unmap_fops);
	if (IS_ERR_OR_NULL(debugfs_file)) {
		ret = PTR_ERR(debugfs_file);
		goto err2;
	}

	hyp_dbg_info("MSM Hyp Debug initialized\n");
	return 0;
err2:
	debugfs_remove_recursive(debugfs_dir);
err1:
	for (i--; i >= 0; i--)
		subsys_notif_unregister_notifier(handle, &restart_nbs[i].nb);
	kfree(restart_nbs);
err0:
	iounmap(pil_info_base);
	return ret;
}
late_initcall(hyp_debug_init);
