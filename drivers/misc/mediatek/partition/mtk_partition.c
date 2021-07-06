/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include <linux/types.h>
#include <linux/genhd.h>
#include <mt-plat/mtk_boot.h>

struct hd_struct *get_part(const char *name)
{
	dev_t devt;
	int partno;
	struct disk_part_iter piter;
	struct gendisk *disk;
	struct hd_struct *part = NULL;
	int boot_type;

	if (!name)
		return part;

	boot_type = get_boot_type();
	if (boot_type == BOOTDEV_UFS)
		devt = blk_lookup_devt("sdc", 0);
	else
		devt = blk_lookup_devt("mmcblk0", 0);

	disk = get_gendisk(devt, &partno);

	if (!disk || get_capacity(disk) == 0)
		return 0;

	disk_part_iter_init(&piter, disk, 0);
	while ((part = disk_part_iter_next(&piter))) {
		if (part->info && !strcmp(part->info->volname, name)) {
			get_device(part_to_dev(part));
			break;
		}
	}
	disk_part_iter_exit(&piter);

	return part;
}
EXPORT_SYMBOL(get_part);

void put_part(struct hd_struct *part)
{
	disk_put_part(part);
}
EXPORT_SYMBOL(put_part);
