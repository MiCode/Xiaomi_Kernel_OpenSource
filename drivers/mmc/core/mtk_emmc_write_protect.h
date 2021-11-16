/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _MTK_EMMC_WRITE_PROTECT_H_
#define _MTK_EMMC_WRITE_PROTECT_H_


#define EXT_CSD_USR_WP                  171     /* R/W */
#define EXT_CSD_CMD_SET_NORMAL          (1<<0)
#define EXT_CSD_USR_WP_EN_PERM_WP       (1<<2)
#define EXT_CSD_USR_WP_EN_PWR_WP        (1)


int set_power_on_write_protect(struct mmc_card *card);
int emmc_set_wp_by_partitions(struct mmc_card *card,
	char *partition_nametab[], int partition_num, unsigned char wp_type);

#endif
