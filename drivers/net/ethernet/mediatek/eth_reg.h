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

#ifndef ETH_REG_H
#define ETH_REG_H

#include <linux/io.h>
#include <linux/of.h>
#include <linux/mtd/mtd.h>
#include <linux/phy.h>

extern struct regmap *ethsys_map;

int phy_ethtool_ioctl(struct phy_device *phydev, void *useraddr);

void rt_sysc_w32(u32 val, unsigned reg);

u32 rt_sysc_r32(unsigned reg);

void rt_sysc_m32(u32 clr, u32 set, unsigned reg);

int mt7620_get_eco(void);

int of_get_mac_address_mtd(struct device_node *np, void *mac);

#endif
