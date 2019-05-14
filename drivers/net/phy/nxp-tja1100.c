// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/of.h>

#define PHY_ID_TJA1100	0x0180dc48
#define TJA1100_LINKUP	BIT(15)

static int tja1100_config_init(struct phy_device *phydev)
{
	phydev->supported = SUPPORTED_100baseT_Full;
	phydev->advertising = SUPPORTED_100baseT_Full;
	phydev->state = PHY_NOLINK;
	phydev->autoneg = AUTONEG_DISABLE;

	return 0;
}

static int tja1100_read_status(struct phy_device *phydev)
{
	int val;

	phydev->duplex = 1;
	phydev->pause = 0;
	phydev->speed = SPEED_100;

	val = phy_read(phydev, MII_RESV1);
	if (val < 0)
		return val;

	if (val & TJA1100_LINKUP)
		phydev->link = 1;
	else
		phydev->link = 0;

	return 0;
}

static struct phy_driver nxp_tja1100_driver[] = {
	{
		.phy_id		= PHY_ID_TJA1100,
		.phy_id_mask	= 0xffffffff,
		.name		= "NXP TJA1100",
		.config_init	= tja1100_config_init,
		.read_status	= tja1100_read_status,
	}
};

module_phy_driver(nxp_tja1100_driver);

static struct mdio_device_id __maybe_unused nxp_tbl[] = {
	{ PHY_ID_TJA1100, 0xffffffff },
	{ }
};

MODULE_DEVICE_TABLE(mdio, nxp_tbl);
