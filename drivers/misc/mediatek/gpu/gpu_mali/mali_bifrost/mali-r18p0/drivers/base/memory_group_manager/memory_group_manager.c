/*
 *
 * (C) COPYRIGHT 2019 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#include <linux/fs.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/module.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif
#include <linux/memory_group_manager.h>

/**
 * struct mgm_group - Structure to keep track of the number of allocated
 *                    pages per group
 *
 * @size:  The number of allocated small(4KB) pages
 * @lp_size:  The number of allocated large(2MB) pages
 *
 * This structure allows page allocation information to be displayed via
 * debugfs. Display is organized per group with small and large sized pages.
 */
struct mgm_group {
	size_t size;
	size_t lp_size;
};

/**
 * struct mgm_groups - Structure for groups of memory group manager
 *
 * @groups: To keep track of the number of allocated pages of all groups
 * @dev: device attached
 * @mgm_debugfs_root: debugfs root directory of memory group manager
 *
 * This structure allows page allocation information to be displayed via
 * debugfs. Display is organized per group with small and large sized pages.
 */
struct mgm_groups {
	struct mgm_group groups[MEMORY_GROUP_MANAGER_NR_GROUPS];
	struct device *dev;
#ifdef CONFIG_DEBUG_FS
	struct dentry *mgm_debugfs_root;
#endif
};

#ifdef CONFIG_DEBUG_FS

static int mgm_size_get(void *data, u64 *val)
{
	struct mgm_group *group = data;

	*val = group->size;

	return 0;
}

static int mgm_lp_size_get(void *data, u64 *val)
{
	struct mgm_group *group = data;

	*val = group->lp_size;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_mgm_size, mgm_size_get, NULL, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_mgm_lp_size, mgm_lp_size_get, NULL, "%llu\n");

static void mgm_term_debugfs(struct mgm_groups *data)
{
	debugfs_remove_recursive(data->mgm_debugfs_root);
}

#define MGM_DEBUGFS_GROUP_NAME_MAX 10
static int mgm_initialize_debugfs(struct mgm_groups *mgm_data)
{
	int i;
	struct dentry *e, *g;
	char debugfs_group_name[MGM_DEBUGFS_GROUP_NAME_MAX];

	/*
	 * Create root directory of memory-group-manager
	 */
	mgm_data->mgm_debugfs_root =
		debugfs_create_dir("physical-memory-group-manager", NULL);
	if (IS_ERR(mgm_data->mgm_debugfs_root)) {
		dev_err(mgm_data->dev, "fail to create debugfs root directory\n");
		return -ENODEV;
	}

	/*
	 * Create debugfs files per group
	 */
	for (i = 0; i < MEMORY_GROUP_MANAGER_NR_GROUPS; i++) {
		scnprintf(debugfs_group_name, MGM_DEBUGFS_GROUP_NAME_MAX,
				"group_%d", i);
		g = debugfs_create_dir(debugfs_group_name,
				mgm_data->mgm_debugfs_root);
		if (IS_ERR(g)) {
			dev_err(mgm_data->dev, "fail to create group[%d]\n", i);
			goto remove_debugfs;
		}

		e = debugfs_create_file("size", 0444, g, &mgm_data->groups[i],
				&fops_mgm_size);
		if (IS_ERR(e)) {
			dev_err(mgm_data->dev, "fail to create size[%d]\n", i);
			goto remove_debugfs;
		}

		e = debugfs_create_file("lp_size", 0444, g,
				&mgm_data->groups[i], &fops_mgm_lp_size);
		if (IS_ERR(e)) {
			dev_err(mgm_data->dev,
				"fail to create lp_size[%d]\n", i);
			goto remove_debugfs;
		}
	}

	return 0;

remove_debugfs:
	mgm_term_debugfs(mgm_data);
	return -ENODEV;
}

#else

static void mgm_term_debugfs(struct mgm_groups *data)
{
}

static int mgm_initialize_debugfs(struct mgm_groups *mgm_data)
{
	return 0;
}

#endif /* CONFIG_DEBUG_FS */

#define ORDER_SMALL_PAGE 0
#define ORDER_LARGE_PAGE 9
static void update_size(struct memory_group_manager_device *mgm_dev, int
		group_id, int order, bool alloc)
{
	struct mgm_groups *data = mgm_dev->data;

	switch (order) {
	case ORDER_SMALL_PAGE:
		if (alloc)
			data->groups[group_id].size++;
		else {
			WARN_ON(data->groups[group_id].size == 0);
			data->groups[group_id].size--;
		}
	break;

	case ORDER_LARGE_PAGE:
		if (alloc)
			data->groups[group_id].lp_size++;
		else {
			WARN_ON(data->groups[group_id].lp_size == 0);
			data->groups[group_id].lp_size--;
		}
	break;

	default:
		dev_err(data->dev, "Unknown order(%d)\n", order);
	break;
	}
}

static struct page *example_mgm_alloc_page(
	struct memory_group_manager_device *mgm_dev, int group_id,
	gfp_t gfp_mask, unsigned int order)
{
	struct page *p;

	WARN_ON(group_id < 0);
	WARN_ON(group_id >= MEMORY_GROUP_MANAGER_NR_GROUPS);

	p = alloc_pages(gfp_mask, order);

	if (p) {
		update_size(mgm_dev, group_id, order, true);
	} else {
		struct mgm_groups *data = mgm_dev->data;

		dev_err(data->dev, "alloc_pages failed\n");
	}

	return p;
}

static void example_mgm_free_page(
	struct memory_group_manager_device *mgm_dev, int group_id,
	struct page *page, unsigned int order)
{
	WARN_ON(group_id < 0);
	WARN_ON(group_id >= MEMORY_GROUP_MANAGER_NR_GROUPS);

	__free_pages(page, order);

	update_size(mgm_dev, group_id, order, false);
}

static int mgm_initialize_data(struct mgm_groups *mgm_data)
{
	int i;

	for (i = 0; i < MEMORY_GROUP_MANAGER_NR_GROUPS; i++) {
		mgm_data->groups[i].size = 0;
		mgm_data->groups[i].lp_size = 0;
	}

	return mgm_initialize_debugfs(mgm_data);
}

static void mgm_term_data(struct mgm_groups *data)
{
	int i;

	for (i = 0; i < MEMORY_GROUP_MANAGER_NR_GROUPS; i++) {
		if (data->groups[i].size != 0)
			dev_warn(data->dev,
				"%zu 0-order pages in group(%d) leaked\n",
				data->groups[i].size, i);
		if (data->groups[i].lp_size != 0)
			dev_warn(data->dev,
				"%zu 9 order pages in group(%d) leaked\n",
				data->groups[i].lp_size, i);
	}

	mgm_term_debugfs(data);
}

static int memory_group_manager_probe(struct platform_device *pdev)
{
	struct memory_group_manager_device *mgm_dev;
	struct mgm_groups *mgm_data;

	mgm_dev = kzalloc(sizeof(*mgm_dev), GFP_KERNEL);
	if (!mgm_dev)
		return -ENOMEM;

	mgm_dev->ops.mgm_alloc_page = example_mgm_alloc_page;
	mgm_dev->ops.mgm_free_page = example_mgm_free_page;

	mgm_data = kzalloc(sizeof(*mgm_data), GFP_KERNEL);
	if (!mgm_data) {
		kfree(mgm_dev);
		return -ENOMEM;
	}

	mgm_dev->data = mgm_data;
	mgm_data->dev = &pdev->dev;

	if (mgm_initialize_data(mgm_data)) {
		kfree(mgm_data);
		kfree(mgm_dev);
		return -ENOENT;
	}

	platform_set_drvdata(pdev, mgm_dev);
	dev_info(&pdev->dev, "Memory group manager probed successfully\n");

	return 0;
}

static int memory_group_manager_remove(struct platform_device *pdev)
{
	struct memory_group_manager_device *mgm_dev =
		platform_get_drvdata(pdev);
	struct mgm_groups *mgm_data = mgm_dev->data;

	mgm_term_data(mgm_data);
	kfree(mgm_data);

	kfree(mgm_dev);

	dev_info(&pdev->dev, "Memory group manager removed successfully\n");

	return 0;
}

static const struct of_device_id memory_group_manager_dt_ids[] = {
	{ .compatible = "arm,physical-memory-group-manager" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, memory_group_manager_dt_ids);

static struct platform_driver memory_group_manager_driver = {
	.probe = memory_group_manager_probe,
	.remove = memory_group_manager_remove,
	.driver = {
		.name = "physical-memory-group-manager",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(memory_group_manager_dt_ids),
	}
};

module_platform_driver(memory_group_manager_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ARM Ltd.");
MODULE_VERSION("1.0");
