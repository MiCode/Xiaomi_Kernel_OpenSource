/*
 * Copyright (c) 2015 Pengutronix, Sascha Hauer <kernel@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/regmap.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/soc/mediatek/infracfg.h>
#include <asm/processor.h>

#define INFRA_TOPAXI_PROTECTEN		0x0220
#define INFRA_TOPAXI_PROTECTSTA1	0x0228
#define INFRA_TOPAXI_PROTECTEN1		0x0250
#define INFRA_TOPAXI_PROTECTSTA3	0x0258

static int set_bits_wait(struct regmap *reg, u32 mask, u32 en_ofs, u32 sta_ofs)
{
	unsigned long expired;
	u32 val;
	int ret;

	regmap_update_bits(reg, en_ofs, mask, mask);

	expired = jiffies + HZ;

	while (1) {
		ret = regmap_read(reg, sta_ofs, &val);
		if (ret)
			return ret;

		if ((val & mask) == mask)
			break;

		cpu_relax();
		if (time_after(jiffies, expired))
			return -EIO;
	}

	return 0;
}

static int clr_bits_wait(struct regmap *reg, u32 mask, u32 en_ofs, u32 sta_ofs)
{
	unsigned long expired;
	u32 val;
	int ret;

	regmap_update_bits(reg, en_ofs, mask, 0);

	expired = jiffies + HZ;

	while (1) {
		ret = regmap_read(reg, sta_ofs, &val);
		if (ret)
			return ret;

		if (!(val & mask))
			break;

		cpu_relax();
		if (time_after(jiffies, expired))
			return -EIO;
	}

	return 0;
}

/**
 * mtk_infracfg_set_bus_protection - enable bus protection
 * @regmap: The infracfg regmap
 * @mask: The mask containing the protection bits to be enabled.
 *
 * This function enables the bus protection bits for disabled power
 * domains so that the system does not hanf when some unit accesses the
 * bus while in power down.
 */
int mtk_infracfg_set_bus_protection(struct regmap *infracfg, u32 mask)
{
	return set_bits_wait(infracfg, mask,
			INFRA_TOPAXI_PROTECTEN, INFRA_TOPAXI_PROTECTSTA1);
}

/**
 * mtk_infracfg_clear_bus_protection - disable bus protection
 * @regmap: The infracfg regmap
 * @mask: The mask containing the protection bits to be disabled.
 *
 * This function disables the bus protection bits previously enabled with
 * mtk_infracfg_set_bus_protection.
 */
int mtk_infracfg_clear_bus_protection(struct regmap *infracfg, u32 mask)
{
	return clr_bits_wait(infracfg, mask,
			INFRA_TOPAXI_PROTECTEN, INFRA_TOPAXI_PROTECTSTA1);
}

/**
 * mtk_infracfg_set_bus_protection1 - enable bus protection
 * @regmap: The infracfg regmap
 * @mask: The mask containing the protection bits to be enabled.
 *
 * This function enables the bus protection bits for disabled power
 * domains so that the system does not hanf when some unit accesses the
 * bus while in power down.
 */
int mtk_infracfg_set_bus_protection1(struct regmap *infracfg, u32 mask)
{
	return set_bits_wait(infracfg, mask,
			INFRA_TOPAXI_PROTECTEN1, INFRA_TOPAXI_PROTECTSTA3);
}

/**
 * mtk_infracfg_clear_bus_protection1 - disable bus protection
 * @regmap: The infracfg regmap
 * @mask: The mask containing the protection bits to be disabled.
 *
 * This function disables the bus protection bits previously enabled with
 * mtk_infracfg_set_bus_protection.
 */
int mtk_infracfg_clear_bus_protection1(struct regmap *infracfg, u32 mask)
{
	return clr_bits_wait(infracfg, mask,
			INFRA_TOPAXI_PROTECTEN1, INFRA_TOPAXI_PROTECTSTA3);
}


MODULE_LICENSE("GPL v2");
