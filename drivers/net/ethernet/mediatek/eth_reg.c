/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include "eth_reg.h"
#include <linux/regmap.h>

int phy_ethtool_ioctl(struct phy_device *phydev, void *useraddr)
{
	return 1;
}

void rt_sysc_w32(u32 val, unsigned reg)
{
	regmap_write(ethsys_map, reg, val);
}

u32 rt_sysc_r32(unsigned reg)
{
	u32 val;

	regmap_read(ethsys_map, reg, &val);
	return val;
}

void rt_sysc_m32(u32 clr, u32 set, unsigned reg)
{
	u32 val = rt_sysc_r32(reg) & ~clr;

	regmap_write(ethsys_map, reg, val | set);
}

int mt7620_get_eco(void)
{
	/* rt_sysc_r32(SYSC_REG_CHIP_REV) & CHIP_REV_ECO_MASK; */
	return 0;
}

int of_get_mac_address_mtd(struct device_node *np, void *mac)
{
	struct device_node *mtd_np = NULL;
	size_t retlen;
	int size, ret;
	struct mtd_info *mtd;
	const char *part;
	const __be32 *list;
	u32 phandle;

/* ra_mtd_read_nm("Factory", GMAC0_OFFSET, 6, addr.sa_data); */
/* #define GMAC0_OFFSET    0x28 */

	list = of_get_property(np, "mtd-mac-address", &size);
	if (!list || (size != (2 * sizeof(*list))))
		return -ENOENT;
	/* printk("list %d size =%d\n", be32_to_cpup(list) ,size); */
	phandle = be32_to_cpup(list++);
	if (phandle)
		mtd_np = of_find_node_by_phandle(phandle);

	if (!mtd_np)
		return -ENOENT;

	part = of_get_property(mtd_np, "label", NULL);
	if (!part)
		part = mtd_np->name;
	/* printk("mtd name= %s\n", part); */

	mtd = get_mtd_device_nm(part);

	if (IS_ERR(mtd)) {
		/* printk("can't get mtd device\n"); */
		return PTR_ERR(mtd);
	}

	ret = mtd_read(mtd, be32_to_cpup(list), 6, &retlen, (u_char *)mac);
	put_mtd_device(mtd);

	return ret;
}
