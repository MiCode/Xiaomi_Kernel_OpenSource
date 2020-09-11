/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/mm_types.h>
#include <mt-plat/mtk_boot.h>

#include "mtk_sd.h"
#include <mmc/core/queue.h>
#include <mmc/core/core.h>
#include "dbg.h"

u64 msdc_get_user_capacity(struct msdc_host *host)
{
	u64 capacity = 0;
	u32 legacy_capacity = 0;
	struct mmc_card *card;

	if (host && host->mmc && host->mmc->card)
		card = host->mmc->card;
	else
		return 0;

	card = host->mmc->card;
	if (mmc_card_mmc(card)) {
		if (card->csd.read_blkbits) {
			legacy_capacity =
				(2 << (card->csd.read_blkbits - 1))
				* card->csd.capacity;
		} else {
			legacy_capacity = card->csd.capacity;
			ERR_MSG("XXX read_blkbits = 0 XXX");
		}
		capacity =
			(u64)(card->ext_csd.sectors) * 512 > legacy_capacity ?
			(u64)(card->ext_csd.sectors) * 512 : legacy_capacity;
	} else if (mmc_card_sd(card)) {
		capacity = (u64) (card->csd.capacity)
			<< (card->csd.read_blkbits);
	}
	return capacity;
}

u32 msdc_get_other_capacity(struct msdc_host *host, char *name)
{
	u32 device_other_capacity = 0;
	int i;
	struct mmc_card *card;

	if (host && host->mmc && host->mmc->card)
		card = host->mmc->card;
	else
		return 0;

	for (i = 0; i < card->nr_parts; i++) {
		if (!name) {
			device_other_capacity += card->part[i].size;
		} else if (strcmp(name, card->part[i].name) == 0) {
			device_other_capacity = card->part[i].size;
			break;
		}
	}

	return device_other_capacity;
}

int msdc_get_part_info(unsigned char *name, struct hd_struct *part)
{
	struct disk_part_iter piter;
	struct hd_struct *l_part;
	struct gendisk *disk;
	int ret = 0, partno;
	dev_t devt;

	devt = blk_lookup_devt("mmcblk0", 0);
	disk = get_gendisk(devt, &partno);

	if (!disk)
		return 0;

	disk_part_iter_init(&piter, disk, 0);
	while ((l_part = disk_part_iter_next(&piter))) {
		if (!strncmp(l_part->info->volname, name, strlen(name))) {
			memcpy(part, l_part, sizeof(struct hd_struct));
			ret = 1;
			break;
		}
	}
	disk_part_iter_exit(&piter);

	return ret;
}

#ifdef MTK_MSDC_USE_CACHE
unsigned long long g_cache_part_start;
unsigned long long g_cache_part_end;
unsigned long long g_usrdata_part_start;
unsigned long long g_usrdata_part_end;
static struct delayed_work get_cache_info;

int msdc_can_apply_cache(unsigned long long start_addr,
	unsigned int size)
{
	if (!g_cache_part_start && !g_cache_part_end &&
		!g_usrdata_part_start && !g_usrdata_part_end)
		return 0;

	/* if cache, userdata partition are connected,
	 * so check it as an area, else do check them separately
	 */
	if (g_cache_part_end == g_usrdata_part_start) {
		if ((start_addr < g_cache_part_start) ||
		    (start_addr + size >= g_usrdata_part_end)) {
			return 0;
		}
	} else {
		if (((start_addr < g_cache_part_start) ||
		     (start_addr + size >= g_cache_part_end))
		 && ((start_addr < g_usrdata_part_start) ||
		     (start_addr + size >= g_usrdata_part_end))) {
			return 0;
		}
	}

	return 1;
}

void msdc_get_cache_region(struct work_struct *work)
{
	struct hd_struct part = {0};
	static int retry;

	if (msdc_get_part_info("cache", &part)) {
		g_cache_part_start = part.start_sect;
		g_cache_part_end = g_cache_part_start + part.nr_sects;
	}

	memset(&part, 0, sizeof(struct hd_struct));
	if (msdc_get_part_info("userdata", &part)) {
		g_usrdata_part_start = part.start_sect;
		g_usrdata_part_end = g_usrdata_part_start + part.nr_sects;
	}

	pr_info("cache(0x%llX~0x%llX, usrdata(0x%llX~0x%llX)\n",
		g_cache_part_start, g_cache_part_end,
		g_usrdata_part_start, g_usrdata_part_end);

	if ((!g_cache_part_start || !g_cache_part_end ||
		!g_usrdata_part_start || !g_usrdata_part_end) &&
		retry++ < 5) {
		/* re-schedule, if part info not ready */
		pr_info("part info not ready re-schedule(%d) %s\n",
			retry, __func__);
		schedule_delayed_work(&get_cache_info, msecs_to_jiffies(1000));
	}
}
EXPORT_SYMBOL(msdc_get_cache_region);

static int __init init_get_cache_work(void)
{
	int boot_type;

	boot_type = get_boot_type();
	if (boot_type != BOOTDEV_SDMMC)
		return 0;

	INIT_DELAYED_WORK(&get_cache_info, msdc_get_cache_region);
	schedule_delayed_work(&get_cache_info, msecs_to_jiffies(500));
	return 0;
}

late_initcall_sync(init_get_cache_work);
#endif
