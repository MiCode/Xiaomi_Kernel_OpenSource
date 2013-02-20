/* arch/arm/mach-msm/htc_akm_cal.c
 *
 * Code to extract compass calibration information from ATAG set up 
 * by the bootloader.
 *
 * Copyright (C) 2007-2008 HTC Corporation
 * Author: Farmer Tseng <farmer_tseng@htc.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>

#include <asm/setup.h>

/* configuration tags specific to AKM8976 */
#define ATAG_AKM8976	0x89768976 /* AKM8976 */

#define MAX_CALI_SIZE	0x1000U

static char akm_cal_ram[MAX_CALI_SIZE];

char *get_akm_cal_ram(void)
{
	return(akm_cal_ram);
}
EXPORT_SYMBOL(get_akm_cal_ram);

static int __init parse_tag_akm(const struct tag *tag)
{
	unsigned char *dptr = (unsigned char *)(&tag->u);
	unsigned size;

	size = min((tag->hdr.size - 2) * sizeof(__u32), MAX_CALI_SIZE);

	printk(KERN_INFO "AKM Data size = %d , 0x%x, size = %d\n",
			tag->hdr.size, tag->hdr.tag, size);

#ifdef ATAG_COMPASS_DEBUG
	unsigned i;
	unsigned char *ptr;

	ptr = dptr;
	printk(KERN_INFO
	       "AKM Data size = %d , 0x%x\n",
	       tag->hdr.size, tag->hdr.tag);
	for (i = 0; i < size; i++)
		printk(KERN_INFO "%02x ", *ptr++);
#endif
	memcpy((void *)akm_cal_ram, (void *)dptr, size);
	return 0;
}

__tagtable(ATAG_AKM8976, parse_tag_akm);
