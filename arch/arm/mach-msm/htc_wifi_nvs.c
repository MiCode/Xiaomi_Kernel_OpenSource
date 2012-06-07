/* arch/arm/mach-msm/htc_wifi_nvs.c
 *
 * Code to extract WiFi calibration information from ATAG set up 
 * by the bootloader.
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Dmitry Shmidt <dimitrysh@google.com>
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

/* configuration tags specific to msm */
#define ATAG_MSM_WIFI	0x57494649 /* MSM WiFi */

#define MAX_NVS_SIZE	0x800U
static unsigned char wifi_nvs_ram[MAX_NVS_SIZE];

unsigned char *get_wifi_nvs_ram( void )
{
	return( wifi_nvs_ram );
}
EXPORT_SYMBOL(get_wifi_nvs_ram);

static int __init parse_tag_msm_wifi(const struct tag *tag)
{
	unsigned char *dptr = (unsigned char *)(&tag->u);
	unsigned size;
	
	size = min((tag->hdr.size - 2) * sizeof(__u32), MAX_NVS_SIZE);
#ifdef ATAG_MSM_WIFI_DEBUG	
	unsigned i;
	
	printk("WiFi Data size = %d , 0x%x\n", tag->hdr.size, tag->hdr.tag);
	for (i = 0; i < size; i++)
		printk("%02x ", *dptr++);
#endif	
	memcpy( (void *)wifi_nvs_ram, (void *)dptr, size );
	return 0;
}

__tagtable(ATAG_MSM_WIFI, parse_tag_msm_wifi);
