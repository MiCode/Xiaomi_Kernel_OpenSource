/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include "plat_sram_flag.h"

static struct plat_sram_flag *plat;

static inline unsigned int extract_n2mbits(unsigned int input,
		unsigned int n, unsigned int m)
{
	/*
	 * 1. ~0 = 1111 1111 1111 1111 1111 1111 1111 1111
	 * 2. ~0 << (m - n + 1) = 1111 1111 1111 1111 1100 0000 0000 0000
	 * assuming we are extracting 14 bits,
	 * the +1 is added for inclusive selection
	 * 3. ~(~0 << (m - n + 1)) = 0000 0000 0000 0000 0011 1111 1111 1111
	 */
	int mask;

	if (n > m) {
		n = n + m;
		m = n - m;
		n = n - m;
	}
	mask = ~(~0 << (m - n + 1));
	return (input >> n) & mask;
}

static int check_sram_base(void)
{
	if (plat)
		return 0;

	pr_notice("%s:%d: sram_base == 0x0\n", __func__, __LINE__);
	return -1;
}

/* return negative integer if fails */
int set_sram_flag_lastpc_valid(void)
{
	if (check_sram_base() < 0)
		return -1;

	plat->plat_sram_flag0 =
		(plat->plat_sram_flag0 | (1 << OFFSET_LASTPC_VALID));
	return 0;
}
EXPORT_SYMBOL(set_sram_flag_lastpc_valid);

/* return negative integer if fails */
int set_sram_flag_etb_user(unsigned int etb_id, unsigned int user_id)
{
	if (check_sram_base() < 0)
		return -1;

	if (etb_id >= MAX_ETB_NUM) {
		pr_notice("%s:%d: etb_id > MAX_ETB_NUM\n",
				__func__, __LINE__);
		return -1;
	}

	if (user_id >= MAX_ETB_USER_NUM) {
		pr_notice("%s:%d: user_id > MAX_ETB_USER_NUM\n",
				__func__, __LINE__);
		return -1;
	}

	plat->plat_sram_flag0 =
		(plat->plat_sram_flag0 & ~(0x7 << (OFFSET_ETB_0 + etb_id*3)))
		| ((user_id & 0x7) << (OFFSET_ETB_0 + etb_id*3));

	return 0;
}
EXPORT_SYMBOL(set_sram_flag_etb_user);

/* return negative integer if fails */
int set_sram_flag_dfd_valid(void)
{
	if (check_sram_base() < 0)
		return -1;

	plat->plat_sram_flag1 =
		(plat->plat_sram_flag1 | (1 << OFFSET_DFD_VALID));

	return 0;
}
EXPORT_SYMBOL(set_sram_flag_dfd_valid);

static struct platform_driver plat_sram_flag_drv = {
	.driver = {
		.name = "plat_sram_flag",
		.bus = &platform_bus_type,
		.owner = THIS_MODULE,
	},
};

static ssize_t plat_sram_flag_dump_show(struct device_driver *driver,
		char *buf)
{
	unsigned int i;
	char *wp = buf;

	if (!plat) {
		pr_notice("%s:%d: sram_base == 0x0\n", __func__, __LINE__);
		return snprintf(buf, PAGE_SIZE, "sram_base == 0x0\n");
	}

	wp += snprintf(wp, PAGE_SIZE,
			"plat_sram_flag0 = 0x%x\n", plat->plat_sram_flag0);
	wp += snprintf(wp, PAGE_SIZE,
			"plat_sram_flag1 = 0x%x\nplat_sram_flag2 = 0x%x\n",
			plat->plat_sram_flag1, plat->plat_sram_flag2);

	wp += snprintf(wp, PAGE_SIZE, "\n-------------\n");

	wp += snprintf(wp, PAGE_SIZE, "lastpc_valid = 0x%x\n",
			extract_n2mbits(plat->plat_sram_flag0,
				OFFSET_LASTPC_VALID, OFFSET_LASTPC_VALID));
	wp += snprintf(wp, PAGE_SIZE, "lastpc_valid_before_reboot = 0x%x\n",
			extract_n2mbits(plat->plat_sram_flag0,
				OFFSET_LASTPC_VALID_BEFORE_REBOOT,
				OFFSET_LASTPC_VALID_BEFORE_REBOOT));

	for (i = 0; i <= MAX_ETB_NUM-1; ++i)
		wp += snprintf(wp, PAGE_SIZE,
			"user_id_of_multi_user_etb_%d = 0x%03x\n",
			i, extract_n2mbits(plat->plat_sram_flag0,
				OFFSET_ETB_0 + i*3, OFFSET_ETB_0 + i*3 + 2));

	wp += snprintf(wp, PAGE_SIZE, "dfd_valid = 0x%x\n",
			extract_n2mbits(plat->plat_sram_flag1,
				OFFSET_DFD_VALID, OFFSET_DFD_VALID));
	wp += snprintf(wp, PAGE_SIZE, "dfd_valid_before_reboot = 0x%x\n",
			extract_n2mbits(plat->plat_sram_flag1,
				OFFSET_DFD_VALID_BEFORE_REBOOT,
				OFFSET_DFD_VALID_BEFORE_REBOOT));

	return strlen(buf);
}

static DRIVER_ATTR_RO(plat_sram_flag_dump);

static int __init plat_sram_flag_init(void)
{
	int ret = 0;
	unsigned int size;

	plat = (struct plat_sram_flag *)get_dbg_info_base(PLAT_SRAM_FLAG_KEY);
	if (!plat)
		return -EINVAL;

	size = get_dbg_info_size(PLAT_SRAM_FLAG_KEY);
	if (size != sizeof(struct plat_sram_flag)) {
		pr_debug("[SRAM FLAG] Can't match plat_sram_flag size\n");
		return -EINVAL;
	}

	ret = platform_driver_register(&plat_sram_flag_drv);
	if (ret)
		return -ENODEV;

	ret = driver_create_file(&plat_sram_flag_drv.driver,
			&driver_attr_plat_sram_flag_dump);
	if (ret)
		pr_notice("%s:%d: driver_create_file failed.\n",
				__func__, __LINE__);


	return 0;
}

core_initcall(plat_sram_flag_init);
