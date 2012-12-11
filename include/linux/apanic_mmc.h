/*
 * include/linux/apanic_mmc.h
 *
 * Copyright 2012 Motorola Mobility LLC .
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_APANIC_MMC_H__
#define __LINUX_APANIC_MMC_H__

#include <linux/genhd.h>

#ifdef __KERNEL__

struct raw_mmc_panic_ops {
	int type;      /* MMC, SD, SDIO on the fly */
	int (*panic_probe)(struct hd_struct *hd, int type);
	int (*panic_write)(struct hd_struct *hd, char *buf,
			unsigned int offset, unsigned int len);
	int (*panic_erase)(struct hd_struct *hd, unsigned int offset,
			unsigned int len);
};

#ifdef CONFIG_APANIC_MMC
int is_apanic_threads_dump(void);
int is_emergency_dump(void);
void emergency_dump(void);
void apanic_mmc_partition_add(struct hd_struct *part);
void apanic_mmc_partition_remove(struct hd_struct *part);
int apanic_mmc_annotate(const char *annotation);

int __init apanic_mmc_init(struct raw_mmc_panic_ops *panic_ops);
#else
static inline int is_apanic_threads_dump(void) { return 0; }
static inline int is_emergency_dump(void) { return 0; }
static inline void emergency_dump(void) {}
static inline void apanic_mmc_partition_add(struct hd_struct *part) {}
static inline void apanic_mmc_partition_remove(struct hd_struct *part) {}
static inline int apanic_mmc_annotate(const char *annotation) { return 0; }

static inline int apanic_mmc_init(struct raw_mmc_panic_ops *panic_ops)
{
	return 0;
}
#endif /* CONFIG_APANIC_MMC */

#endif /* __KERNEL__ */

#endif /* __LINUX_APANIC_MMC_H__ */
