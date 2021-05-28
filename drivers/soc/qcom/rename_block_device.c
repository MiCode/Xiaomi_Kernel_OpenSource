// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/genhd.h>
#include <linux/device.h>
#include <linux/of.h>
#define PATH_SIZE	32
#define SLOT_STR_LENGTH 3
#define MAX_STR_SIZE 255

static char active_slot[SLOT_STR_LENGTH];
static char backup_slot[SLOT_STR_LENGTH];
static int dp_enabled;
static char final_name[MAX_STR_SIZE];
static int __init set_slot_suffix(char *str)
{
	if (str) {
		strlcpy(active_slot, str, SLOT_STR_LENGTH);
		strcmp(active_slot, "_a") ?
		strlcpy(backup_slot, "_a", SLOT_STR_LENGTH) :
		strlcpy(backup_slot, "_b", SLOT_STR_LENGTH);
		dp_enabled = 1;
	}
	return 1;
}
__setup("androidboot.slot_suffix=", set_slot_suffix);

static void get_slot_updated_name(char *name)
{
	int length = strlen(name);

	memset(final_name, '\0', MAX_STR_SIZE);
	strlcpy(final_name, name, MAX_STR_SIZE);
	if (dp_enabled && (final_name[length-2] == '_')) {
		if (final_name[length-1] == 'a')
			final_name[length-1] = active_slot[1];
		else if (final_name[length-1] == 'b')
			final_name[length-1] = backup_slot[1];
	}
}

static int rename_blk_dev_init(void)
{
	dev_t dev;
	int index = 0, partno;
	struct gendisk *disk;
	struct device_node *node;
	char dev_path[PATH_SIZE];
	const char *actual_name;
	char *modified_name;

	node = of_find_compatible_node(NULL, NULL, "qcom,blkdev-rename");
	if (!node) {
		pr_err("qcom,blkdev-rename is missing\n");
		goto out;
	}
	while (!of_property_read_string_index(node, "actual-dev", index,
						&actual_name)) {
		memset(dev_path, '\0', PATH_SIZE);
		snprintf(dev_path, PATH_SIZE, "/dev/%s", actual_name);
		dev = name_to_dev_t(dev_path);
		if (!dev) {
			pr_err("No device path : %s\n", dev_path);
			goto out;
		}
		disk = get_gendisk(dev, &partno);
		if (!disk) {
			pr_err("No device with dev path : %s\n", dev_path);
			goto out;
		}
		if (!of_property_read_string_index(node, dp_enabled ?
					"rename-dev-ab" : "rename-dev",
				 index,	(const char **)&modified_name)) {
			get_slot_updated_name(modified_name);
			device_rename(disk_to_dev(disk), final_name);
		} else {
			pr_err("rename-dev for actual-dev = %s is missing\n",
								 actual_name);
			goto out;
		}
		index++;
	}
out:
	return  0;
}

late_initcall(rename_blk_dev_init);
MODULE_DESCRIPTION("Rename block devices");
MODULE_LICENSE("GPL v2");
