// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/of.h>

#define PHY_ID_88Q2110			0x002b0980

#define Q2110_PMA_PMD_CTRL		(MII_ADDR_C45 | 0x10000)
/* 1 = PMA/PMD reset, 0 = Normal */
#define Q2110_PMA_PMD_RST		BIT(15)

#define Q2110_T1_PMA_PMD_CTRL		(MII_ADDR_C45 | 0x10834)
/* 1 = PHY as master, 0 = PHY as slave */
#define Q2110_T1_MASTER_SLAVE_CFG	BIT(14)
/* 0 = 100BASE-T1, 1 = 1000BASE-T1 */
#define Q2110_T1_LINK_TYPE		BIT(0)

#define Q2110_RST_CTRL			(MII_ADDR_C45 | 0x038000)
/* software reset of T-unit */
#define Q2110_RGMII_SW_RESET		BIT(15)

#define Q2110_T1_AN_STATUS		(MII_ADDR_C45 | 0x070201)
/* 1 = Link Up, 0 = Link Down */
#define Q2110_T1_LINK_STATUS		BIT(2)

#define Q2110_COM_PORT_CTRL		(MII_ADDR_C45 | 0x1f8001)
/* Add delay on TX_CLK */
#define Q2110_RGMII_TX_TIMING_CTRL	BIT(15)
/* Add delay on RX_CLK */
#define Q2110_RGMII_RX_TIMING_CTRL	BIT(14)

/* Set and/or override some configuration registers based on the
 * marvell,88q2110 property stored in the of_node for the phydev.
 * marvell,88q2110 = <speed master>,...;
 * speed: 1000Mbps or 100Mbps.
 * master: 1-master, 0-slave.
 */
static int q2110_dts_init(struct phy_device *phydev)
{
	const __be32 *paddr;
	u32 speed;
	u32 master;
	int val, len;

	if (!phydev->mdio.dev.of_node)
		return 0;

	paddr = of_get_property(phydev->mdio.dev.of_node,
				"marvell,88q2110", &len);
	if (!paddr)
		return 0;

	speed = be32_to_cpup(paddr);
	master = be32_to_cpup(paddr + 1);

	val = phy_read(phydev, Q2110_T1_PMA_PMD_CTRL);
	if (val < 0)
		return val;
	val &= ~(Q2110_T1_MASTER_SLAVE_CFG | Q2110_T1_LINK_TYPE);
	if (speed == SPEED_1000)
		val |= Q2110_T1_LINK_TYPE;
	if (master)
		val |= Q2110_T1_MASTER_SLAVE_CFG;
	val = phy_write(phydev, Q2110_T1_PMA_PMD_CTRL, val);
	if (val < 0)
		return val;

	/* Software Reset PHY */
	val = phy_read(phydev, Q2110_PMA_PMD_CTRL);
	if (val < 0)
		return val;
	val |= Q2110_PMA_PMD_RST;
	val = phy_write(phydev, Q2110_PMA_PMD_CTRL, val);
	if (val < 0)
		return val;

	do {
		val = phy_read(phydev, Q2110_PMA_PMD_CTRL);
		if (val < 0)
			return val;
	} while (val & Q2110_PMA_PMD_RST);

	return 0;
}

static int q2110_timing_init(struct phy_device *phydev)
{
	int val;

	if (phy_interface_is_rgmii(phydev)) {
		val = phy_read(phydev, Q2110_COM_PORT_CTRL);
		if (val < 0)
			return val;

		val &= ~(Q2110_RGMII_TX_TIMING_CTRL |
			 Q2110_RGMII_RX_TIMING_CTRL);

		if (phydev->interface == PHY_INTERFACE_MODE_RGMII_ID)
			val |= (Q2110_RGMII_TX_TIMING_CTRL |
				Q2110_RGMII_RX_TIMING_CTRL);
		else if (phydev->interface == PHY_INTERFACE_MODE_RGMII_RXID)
			val |= Q2110_RGMII_RX_TIMING_CTRL;
		else if (phydev->interface == PHY_INTERFACE_MODE_RGMII_TXID)
			val |= Q2110_RGMII_TX_TIMING_CTRL;

		val = phy_write(phydev, Q2110_COM_PORT_CTRL, val);
		if (val < 0)
			return val;
	}

	/* Software Reset of T-unit */
	val = phy_read(phydev, Q2110_RST_CTRL);
	if (val < 0)
		return val;
	val |= Q2110_RGMII_SW_RESET;
	val = phy_write(phydev, Q2110_RST_CTRL, val);
	if (val < 0)
		return val;

	/* not self-clearing */
	val = phy_read(phydev, Q2110_RST_CTRL);
	if (val < 0)
		return val;
	val &= ~Q2110_RGMII_SW_RESET;
	val = phy_write(phydev, Q2110_RST_CTRL, val);
	if (val < 0)
		return val;

	return 0;
}

static int q2110_config_init(struct phy_device *phydev)
{
	phydev->supported =
		SUPPORTED_1000baseT_Full | SUPPORTED_100baseT_Full;
	phydev->advertising =
		SUPPORTED_1000baseT_Full | SUPPORTED_100baseT_Full;
	phydev->state = PHY_NOLINK;
	phydev->autoneg = AUTONEG_DISABLE;

	q2110_dts_init(phydev);
	q2110_timing_init(phydev);

	return 0;
}

static int q2110_read_status(struct phy_device *phydev)
{
	int val;

	phydev->duplex = 1;
	phydev->pause = 0;

	val = phy_read(phydev, Q2110_T1_AN_STATUS);
	if (val < 0)
		return val;

	if (val & Q2110_T1_LINK_STATUS)
		phydev->link = 1;
	else
		phydev->link = 0;

	val = phy_read(phydev, Q2110_T1_PMA_PMD_CTRL);
	if (val < 0)
		return val;

	if (val & Q2110_T1_LINK_TYPE)
		phydev->speed = SPEED_1000;
	else
		phydev->speed = SPEED_100;

	return 0;
}

static int q2110_match_phy_device(struct phy_device *phydev)
{
	return (phydev->c45_ids.device_ids[1] & 0xfffffff0) == PHY_ID_88Q2110;
}

static struct phy_driver marvell_88q_driver[] = {
	{
		.phy_id		= PHY_ID_88Q2110,
		.phy_id_mask	= 0xfffffff0,
		.name		= "Marvell 88Q2110",
		.config_init	= q2110_config_init,
		.match_phy_device = q2110_match_phy_device,
		.read_status	= q2110_read_status,
	}
};

module_phy_driver(marvell_88q_driver);
