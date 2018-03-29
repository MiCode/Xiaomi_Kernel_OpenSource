/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2 of the License
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   Copyright (C) 2009-2013 John Crispin <blogic@openwrt.org>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/of_net.h>
#include <linux/of_mdio.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <linux/ioport.h>
#include <linux/mii.h>

#include "ralink_soc_eth.h"
#include "gsw_mt7620a.h"
#include "mt7530.h"
#include "mdio.h"

#define GSW_REG_PHY_TIMEOUT	(5 * HZ)

#if defined(CONFIG_SOC_MT7621) || defined(CONFIG_MACH_MT2701) || \
defined(CONFIG_ARCH_MT7623)
#define MT7620A_GSW_REG_PIAC	0x0004
#else
#define MT7620A_GSW_REG_PIAC	0x7004
#endif

#define GSW_NUM_VLANS		16
#define GSW_NUM_VIDS		4096
#define GSW_NUM_PORTS		7
#define GSW_PORT6		6

#define GSW_MDIO_ACCESS		BIT(31)
#define GSW_MDIO_READ		BIT(19)
#define GSW_MDIO_WRITE		BIT(18)
#define GSW_MDIO_START		BIT(16)
#define GSW_MDIO_ADDR_SHIFT	20
#define GSW_MDIO_REG_SHIFT	25

#define GSW_REG_PORT_PMCR(x)	(0x3000 + (x * 0x100))
#define GSW_REG_PORT_STATUS(x)	(0x3008 + (x * 0x100))
#define GSW_REG_SMACCR0		0x3fE4
#define GSW_REG_SMACCR1		0x3fE8
#define GSW_REG_CKGCR		0x3ff0

#define GSW_REG_IMR		0x7008
#define GSW_REG_ISR		0x700c
#define GSW_REG_GPC1		0x7014

#define SYSC_REG_CHIP_REV_ID	0x0c
#define SYSC_REG_CFG1		0x14
#define RST_CTRL_MCM		BIT(2)
#define SYSC_PAD_RGMII2_MDIO	0x58
#define SYSC_GPIO_MODE		0x60

#define PORT_IRQ_ST_CHG		0x7f

#if defined(CONFIG_SOC_MT7621) || defined(CONFIG_MACH_MT2701) || \
defined(CONFIG_ARCH_MT7623)
#define ESW_PHY_POLLING		0x0000
#else
#define ESW_PHY_POLLING		0x7000
#endif

#define	PMCR_IPG		BIT(18)
#define	PMCR_MAC_MODE		BIT(16)
#define	PMCR_FORCE		BIT(15)
#define	PMCR_TX_EN		BIT(14)
#define	PMCR_RX_EN		BIT(13)
#define	PMCR_BACKOFF		BIT(9)
#define	PMCR_BACKPRES		BIT(8)
#define	PMCR_RX_FC		BIT(5)
#define	PMCR_TX_FC		BIT(4)
#define	PMCR_SPEED(_x)		(_x << 2)
#define	PMCR_DUPLEX		BIT(1)
#define	PMCR_LINK		BIT(0)

#define PHY_AN_EN		BIT(31)
#define PHY_PRE_EN		BIT(30)
#define PMY_MDC_CONF(_x)	((_x & 0x3f) << 24)

enum {
	/* Global attributes. */
	GSW_ATTR_ENABLE_VLAN,
	/* Port attributes. */
	GSW_ATTR_PORT_UNTAG,
};

enum {
	PORT4_EPHY = 0,
	PORT4_EXT,
};

struct mt7620_gsw {
	struct device *dev;
	void __iomem *base;
	int irq;
	int port4;
	unsigned long autopoll;
	u32 trgmii_force;
};

struct mt7620_gsw *__gsw;
struct fe_priv *__priv;
static void lan_wan_partition(struct mt7620_gsw *gsw);

static inline void gsw_w32(struct mt7620_gsw *gsw, u32 val, unsigned reg)
{
	iowrite32(val, gsw->base + reg);
}

static inline u32 gsw_r32(struct mt7620_gsw *gsw, unsigned reg)
{
	return ioread32(gsw->base + reg);
}

static int mt7620_mii_busy_wait(struct mt7620_gsw *gsw)
{
	unsigned long t_start = jiffies;

	while (1) {
		if (!(gsw_r32(gsw, MT7620A_GSW_REG_PIAC) & GSW_MDIO_ACCESS))
			return 0;
		if (time_after(jiffies, t_start + GSW_REG_PHY_TIMEOUT))
			break;
	}

	pr_err("mdio: MDIO timeout\n");
	return -1;
}

static u32 _mt7620_mii_write(struct mt7620_gsw *gsw, u32 phy_addr,
			     u32 phy_register, u32 write_data)
{
	if (mt7620_mii_busy_wait(gsw))
		return -1;

	write_data &= 0xffff;

	gsw_w32(gsw, GSW_MDIO_ACCESS | GSW_MDIO_START | GSW_MDIO_WRITE |
		(phy_register << GSW_MDIO_REG_SHIFT) |
		(phy_addr << GSW_MDIO_ADDR_SHIFT) | write_data,
		MT7620A_GSW_REG_PIAC);

	if (mt7620_mii_busy_wait(gsw))
		return -1;

	return 0;
}

static u32 _mt7620_mii_read(struct mt7620_gsw *gsw, int phy_addr, int phy_reg)
{
	u32 d;

	if (mt7620_mii_busy_wait(gsw))
		return 0xffff;

	gsw_w32(gsw, GSW_MDIO_ACCESS | GSW_MDIO_START | GSW_MDIO_READ |
		(phy_reg << GSW_MDIO_REG_SHIFT) |
		(phy_addr << GSW_MDIO_ADDR_SHIFT), MT7620A_GSW_REG_PIAC);

	if (mt7620_mii_busy_wait(gsw))
		return 0xffff;

	d = gsw_r32(gsw, MT7620A_GSW_REG_PIAC) & 0xffff;

	return d;
}

int mt7620_mdio_write(struct mii_bus *bus, int phy_addr, int phy_reg, u16 val)
{
	struct fe_priv *priv = bus->priv;
	struct mt7620_gsw *gsw = (struct mt7620_gsw *)priv->soc->swpriv;

	return _mt7620_mii_write(gsw, phy_addr, phy_reg, val);
}

int mt7620_mdio_read(struct mii_bus *bus, int phy_addr, int phy_reg)
{
	struct fe_priv *priv = bus->priv;
	struct mt7620_gsw *gsw = (struct mt7620_gsw *)priv->soc->swpriv;

	return _mt7620_mii_read(gsw, phy_addr, phy_reg);
}

static void mt7530_mdio_w32(struct mt7620_gsw *gsw, u32 reg, u32 val)
{
	_mt7620_mii_write(gsw, 0x1f, 0x1f, (reg >> 6) & 0x3ff);
	_mt7620_mii_write(gsw, 0x1f, (reg >> 2) & 0xf, val & 0xffff);
	_mt7620_mii_write(gsw, 0x1f, 0x10, val >> 16);
}

static u32 mt7530_mdio_r32(struct mt7620_gsw *gsw, u32 reg)
{
	u16 high, low;

	_mt7620_mii_write(gsw, 0x1f, 0x1f, (reg >> 6) & 0x3ff);
	low = _mt7620_mii_read(gsw, 0x1f, (reg >> 2) & 0xf);
	high = _mt7620_mii_read(gsw, 0x1f, 0x10);

	return (high << 16) | (low & 0xffff);
}

static unsigned char *fe_speed_str(int speed)
{
	switch (speed) {
	case 2:
	case SPEED_1000:
		return "1000";
	case 1:
	case SPEED_100:
		return "100";
	case 0:
	case SPEED_10:
		return "10";
	}

	return "? ";
}

int mt7620a_has_carrier(struct fe_priv *priv)
{
	struct mt7620_gsw *gsw = (struct mt7620_gsw *)priv->soc->swpriv;
	int i;

	for (i = 0; i < GSW_PORT6; i++)
		if (gsw_r32(gsw, GSW_REG_PORT_STATUS(i)) & 0x1)
			return 1;
	return 0;
}

int mt7623_has_carrier(struct fe_priv *priv)
{
	struct mt7620_gsw *gsw = (struct mt7620_gsw *)priv->soc->swpriv;
	unsigned int link;
	int i = 4;

	link = mt7530_mdio_r32(gsw, 0x3008 + (i * 0x100)) & 0x1;

	if (!link) {
		pr_err("port 4 is not linked\n");
	} else {
		pr_err("port 4 is linked\n");
		return 1;
	}
	return 0;
}

static void mt7620a_handle_carrier(struct fe_priv *priv)
{
	if (!priv->phy)
		return;

	if (mt7620a_has_carrier(priv))
		netif_carrier_on(priv->netdev);
	else
		netif_carrier_off(priv->netdev);
}

void mt7620_mdio_link_adjust(struct fe_priv *priv, int port)
{
	if (priv->link[port])
		netdev_info(priv->netdev,
			    "port %d link up (%sMbps/%s duplex)\n", port,
			    fe_speed_str(priv->phy->speed[port]),
			    (DUPLEX_FULL ==
			     priv->phy->duplex[port]) ? "Full" : "Half");
	else
		netdev_info(priv->netdev, "port %d link down\n", port);
	mt7620a_handle_carrier(priv);
}

static irqreturn_t gsw_interrupt_mt7620(int irq, void *_priv)
{
	struct fe_priv *priv = (struct fe_priv *)_priv;
	struct mt7620_gsw *gsw = (struct mt7620_gsw *)priv->soc->swpriv;
	u32 status;
	int i, max = (gsw->port4 == PORT4_EPHY) ? (4) : (3);

	status = gsw_r32(gsw, GSW_REG_ISR);
	if (status & PORT_IRQ_ST_CHG)
		for (i = 0; i <= max; i++) {
			u32 status = gsw_r32(gsw, GSW_REG_PORT_STATUS(i));
			int link = status & 0x1;

			if (link != priv->link[i]) {
				if (link)
					netdev_info(priv->netdev,
						    "port %d link up (%sMbps/%s duplex)\n",
						    i,
						    fe_speed_str((status >> 2) &
								 3),
						    (status & 0x2) ? "Full" :
						    "Half");
				else
					netdev_info(priv->netdev,
						    "port %d link down\n", i);
			}

			priv->link[i] = link;
		}
	mt7620a_handle_carrier(priv);

	gsw_w32(gsw, status, GSW_REG_ISR);

	return IRQ_HANDLED;
}

static irqreturn_t gsw_interrupt_mt7621(int irq, void *_priv)
{
	struct fe_priv *priv = (struct fe_priv *)_priv;
	struct mt7620_gsw *gsw = (struct mt7620_gsw *)priv->soc->swpriv;
	u32 reg, i;

	reg = mt7530_mdio_r32(gsw, 0x700c);

	for (i = 0; i < 5; i++)
		if (reg & BIT(i)) {
			unsigned int link =
			    mt7530_mdio_r32(gsw, 0x3008 + (i * 0x100)) & 0x1;

			if (link != priv->link[i]) {
				priv->link[i] = link;
				if (link)
					pr_err("port %d link up\n", i);
				else
					pr_err("port %d link down\n", i);
#ifdef CONFIG_ARCH_MT7623
				if (i == 4 && link)
					netif_carrier_on(priv->netdev);
				else if (i == 4)
					netif_carrier_off(priv->netdev);
#endif
			}
		}
	if (!IS_ENABLED(CONFIG_ARCH_MT7623))
		mt7620a_handle_carrier(priv);
	mt7530_mdio_w32(gsw, 0x700c, 0x1f);

	return IRQ_HANDLED;
}

static int mt7620_is_bga(void)
{
	u32 bga = rt_sysc_r32(0x0c);

	return (bga >> 16) & 1;
}

static void gsw_auto_poll(struct mt7620_gsw *gsw)
{
	int phy;
	int lsb = -1, msb = 0;

	for_each_set_bit(phy, &gsw->autopoll, 32) {
		if (lsb < 0)
			lsb = phy;
		msb = phy;
	}

	if (lsb == msb)
		lsb--;

	gsw_w32(gsw,
		PHY_AN_EN | PHY_PRE_EN | PMY_MDC_CONF(5) | (msb << 8) | lsb,
		ESW_PHY_POLLING);
}

void mt7620_port_init(struct fe_priv *priv, struct device_node *np)
{
	struct mt7620_gsw *gsw = (struct mt7620_gsw *)priv->soc->swpriv;
	const __be32 *_id = of_get_property(np, "reg", NULL);
	int phy_mode, size, id;
	int shift = 12;
	u32 val, mask = 0;
	int min = (gsw->port4 == PORT4_EPHY) ? (5) : (4);

	if (!_id || (be32_to_cpu(*_id) < min) || (be32_to_cpu(*_id) > 5)) {
		if (_id)
			pr_err("%s: invalid port id %d\n", np->name,
			       be32_to_cpu(*_id));
		else
			pr_err("%s: invalid port id\n", np->name);
		return;
	}

	id = be32_to_cpu(*_id);

	if (id == 4)
		shift = 14;

	priv->phy->phy_fixed[id] =
	    of_get_property(np, "ralink,fixed-link", &size);
	if (priv->phy->phy_fixed[id] &&
	    (size != (4 * sizeof(*priv->phy->phy_fixed[id])))) {
		pr_err("%s: invalid fixed link property\n", np->name);
		priv->phy->phy_fixed[id] = NULL;
		return;
	}

	phy_mode = of_get_phy_mode(np);
	switch (phy_mode) {
	case PHY_INTERFACE_MODE_RGMII:
		mask = 0;
		break;
	case PHY_INTERFACE_MODE_MII:
		mask = 1;
		break;
	case PHY_INTERFACE_MODE_RMII:
		mask = 2;
		break;
	default:
		dev_err(priv->device, "port %d - invalid phy mode\n", id);
		return;
	}

	priv->phy->phy_node[id] = of_parse_phandle(np, "phy-handle", 0);
	if (!priv->phy->phy_node[id] && !priv->phy->phy_fixed[id])
		return;

	val = rt_sysc_r32(SYSC_REG_CFG1);
	val &= ~(3 << shift);
	val |= mask << shift;
	rt_sysc_w32(val, SYSC_REG_CFG1);

	if (priv->phy->phy_fixed[id]) {
		const __be32 *link = priv->phy->phy_fixed[id];
		int tx_fc, rx_fc;
		u32 val = 0;

		priv->phy->speed[id] = be32_to_cpup(link++);
		tx_fc = be32_to_cpup(link++);
		rx_fc = be32_to_cpup(link++);
		priv->phy->duplex[id] = be32_to_cpup(link++);
		priv->link[id] = 1;

		switch (priv->phy->speed[id]) {
		case SPEED_10:
			val = 0;
			break;
		case SPEED_100:
			val = 1;
			break;
		case SPEED_1000:
			val = 2;
			break;
		default:
			dev_err(priv->device, "invalid link speed: %d\n",
				priv->phy->speed[id]);
			priv->phy->phy_fixed[id] = 0;
			return;
		}
		val = PMCR_SPEED(val);
		val |= PMCR_LINK | PMCR_BACKPRES | PMCR_BACKOFF | PMCR_RX_EN |
		    PMCR_TX_EN | PMCR_FORCE | PMCR_MAC_MODE | PMCR_IPG;
		if (tx_fc)
			val |= PMCR_TX_FC;
		if (rx_fc)
			val |= PMCR_RX_FC;
		if (priv->phy->duplex[id])
			val |= PMCR_DUPLEX;
		gsw_w32(gsw, val, GSW_REG_PORT_PMCR(id));
		dev_info(priv->device, "using fixed link parameters\n");
		return;
	}

	if (priv->phy->phy_node[id] && priv->mii_bus->phy_map[id]) {
		u32 val = PMCR_BACKPRES | PMCR_BACKOFF | PMCR_RX_EN |
		    PMCR_TX_EN | PMCR_MAC_MODE | PMCR_IPG;

		gsw_w32(gsw, val, GSW_REG_PORT_PMCR(id));
		fe_connect_phy_node(priv, priv->phy->phy_node[id]);
		gsw->autopoll |= BIT(id);
		gsw_auto_poll(gsw);
		return;
	}
}

static void gsw_hw_init_mt7620(struct mt7620_gsw *gsw, struct device_node *np)
{
	u32 is_BGA = mt7620_is_bga();

	rt_sysc_w32(rt_sysc_r32(SYSC_REG_CFG1) | BIT(8), SYSC_REG_CFG1);
	gsw_w32(gsw, gsw_r32(gsw, GSW_REG_CKGCR) & ~(0x3 << 4), GSW_REG_CKGCR);

	if (of_property_read_bool(np, "mediatek,mt7530")) {
		u32 val;

		/* turn off ephy and set phy base addr to 12 */
		gsw_w32(gsw,
			gsw_r32(gsw, GSW_REG_GPC1) | (0x1f << 24) | (0xc << 16),
			GSW_REG_GPC1);

		/* set MT7530 central align */
		val = mt7530_mdio_r32(gsw, 0x7830);
		val &= ~1;
		val |= 1 << 1;
		mt7530_mdio_w32(gsw, 0x7830, val);

		val = mt7530_mdio_r32(gsw, 0x7a40);
		val &= ~(1 << 30);
		mt7530_mdio_w32(gsw, 0x7a40, val);

		mt7530_mdio_w32(gsw, 0x7a78, 0x855);
	} else {
		/* EPHY1 fixup - only run if the ephy is enabled */

		/*correct  PHY  setting L3.0 BGA */
		_mt7620_mii_write(gsw, 1, 31, 0x4000);	/* global, page 4 */

		_mt7620_mii_write(gsw, 1, 17, 0x7444);
		if (is_BGA)
			_mt7620_mii_write(gsw, 1, 19, 0x0114);
		else
			_mt7620_mii_write(gsw, 1, 19, 0x0117);

		_mt7620_mii_write(gsw, 1, 22, 0x10cf);
		_mt7620_mii_write(gsw, 1, 25, 0x6212);
		_mt7620_mii_write(gsw, 1, 26, 0x0777);
		_mt7620_mii_write(gsw, 1, 29, 0x4000);
		_mt7620_mii_write(gsw, 1, 28, 0xc077);
		_mt7620_mii_write(gsw, 1, 24, 0x0000);

		_mt7620_mii_write(gsw, 1, 31, 0x3000);	/* global, page 3 */
		_mt7620_mii_write(gsw, 1, 17, 0x4838);

		_mt7620_mii_write(gsw, 1, 31, 0x2000);	/* global, page 2 */
		if (is_BGA) {
			_mt7620_mii_write(gsw, 1, 21, 0x0515);
			_mt7620_mii_write(gsw, 1, 22, 0x0053);
			_mt7620_mii_write(gsw, 1, 23, 0x00bf);
			_mt7620_mii_write(gsw, 1, 24, 0x0aaf);
			_mt7620_mii_write(gsw, 1, 25, 0x0fad);
			_mt7620_mii_write(gsw, 1, 26, 0x0fc1);
		} else {
			_mt7620_mii_write(gsw, 1, 21, 0x0517);
			_mt7620_mii_write(gsw, 1, 22, 0x0fd2);
			_mt7620_mii_write(gsw, 1, 23, 0x00bf);
			_mt7620_mii_write(gsw, 1, 24, 0x0aab);
			_mt7620_mii_write(gsw, 1, 25, 0x00ae);
			_mt7620_mii_write(gsw, 1, 26, 0x0fff);
		}
		_mt7620_mii_write(gsw, 1, 31, 0x1000);	/* global, page 1 */
		_mt7620_mii_write(gsw, 1, 17, 0xe7f8);
	}

	_mt7620_mii_write(gsw, 1, 31, 0x8000);	/* local, page 0 */
	_mt7620_mii_write(gsw, 0, 30, 0xa000);
	_mt7620_mii_write(gsw, 1, 30, 0xa000);
	_mt7620_mii_write(gsw, 2, 30, 0xa000);
	_mt7620_mii_write(gsw, 3, 30, 0xa000);

	_mt7620_mii_write(gsw, 0, 4, 0x05e1);
	_mt7620_mii_write(gsw, 1, 4, 0x05e1);
	_mt7620_mii_write(gsw, 2, 4, 0x05e1);
	_mt7620_mii_write(gsw, 3, 4, 0x05e1);

	_mt7620_mii_write(gsw, 1, 31, 0xa000);	/* local, page 2 */
	_mt7620_mii_write(gsw, 0, 16, 0x1111);
	_mt7620_mii_write(gsw, 1, 16, 0x1010);
	_mt7620_mii_write(gsw, 2, 16, 0x1515);
	_mt7620_mii_write(gsw, 3, 16, 0x0f0f);

	/* CPU Port6 Force Link 1G, FC ON */
	gsw_w32(gsw, 0x5e33b, GSW_REG_PORT_PMCR(6));
	/* Set Port6 CPU Port */
	gsw_w32(gsw, 0x7f7f7fe0, 0x0010);

	/* setup port 4 */
	if (gsw->port4 == PORT4_EPHY) {
		u32 val = rt_sysc_r32(SYSC_REG_CFG1);

		val |= 3 << 14;
		rt_sysc_w32(val, SYSC_REG_CFG1);
		_mt7620_mii_write(gsw, 4, 30, 0xa000);
		_mt7620_mii_write(gsw, 4, 4, 0x05e1);
		_mt7620_mii_write(gsw, 4, 16, 0x1313);
		pr_info("gsw: setting port4 to ephy mode\n");
	}
}

static void gsw_hw_init_mt7621(struct mt7620_gsw *gsw, struct device_node *np)
{
	u32 i;
	u32 val;

	/* Hardware reset Switch */
	fe_reset(RST_CTRL_MCM);
	mdelay(10);

	/* reduce RGMII2 PAD driving strength */
	rt_sysc_m32(3 << 4, 0, SYSC_PAD_RGMII2_MDIO);

	/* gpio mux - RGMII1=Normal mode */
	rt_sysc_m32(BIT(14), 0, SYSC_GPIO_MODE);

	/* GMAC1= RGMII mode */
	rt_sysc_m32(3 << 12, 0, SYSC_REG_CFG1);

	/* enable MDIO to control MT7530 */
	rt_sysc_m32(3 << 12, 0, SYSC_GPIO_MODE);

	/* turn off all PHYs */
	for (i = 0; i <= 4; i++) {
		val = _mt7620_mii_read(gsw, i, 0x0);
		val |= (0x1 << 11);
		_mt7620_mii_write(gsw, i, 0x0, val);
	}

	/* reset the switch */
	mt7530_mdio_w32(gsw, 0x7000, 0x3);
	usleep_range(10, 20);
	/* udelay(10); */

	if ((rt_sysc_r32(SYSC_REG_CHIP_REV_ID) & 0xFFFF) == 0x0101) {
		/* (GE1, Force 1000M/FD, FC ON) */
		gsw_w32(gsw, 0x2005e30b, 0x100);
		mt7530_mdio_w32(gsw, 0x3600, 0x5e30b);
	} else {
		/* (GE1, Force 1000M/FD, FC ON) */
		gsw_w32(gsw, 0x2005e33b, 0x100);
		mt7530_mdio_w32(gsw, 0x3600, 0x5e33b);
	}

	/* (GE2, Link down) */
	gsw_w32(gsw, 0x8000, 0x200);

	/* val = 0x117ccf; //Enable Port 6, P5 as GMAC5, P5 disable */
	val = mt7530_mdio_r32(gsw, 0x7804);
	val &= ~(1 << 8);	/* Enable Port 6 */
	val |= (1 << 6);	/* Disable Port 5 */
	val |= (1 << 13);	/* Port 5 as GMAC, no Internal PHY */

	val |= (1 << 16);	/* change HW-TRAP */
	pr_err("change HW-TRAP to 0x%x\n", val);
	mt7530_mdio_w32(gsw, 0x7804, val);

	val = rt_sysc_r32(0x10);
	val = (val >> 6) & 0x7;
	if (val >= 6) {
		/* 25Mhz Xtal - do nothing */
	} else if (val >= 3) {
		/* 40Mhz */

		/* disable MT7530 core clock */
		_mt7620_mii_write(gsw, 0, 13, 0x1f);
		_mt7620_mii_write(gsw, 0, 14, 0x410);
		_mt7620_mii_write(gsw, 0, 13, 0x401f);
		_mt7620_mii_write(gsw, 0, 14, 0x0);

		/* disable MT7530 PLL */
		_mt7620_mii_write(gsw, 0, 13, 0x1f);
		_mt7620_mii_write(gsw, 0, 14, 0x40d);
		_mt7620_mii_write(gsw, 0, 13, 0x401f);
		_mt7620_mii_write(gsw, 0, 14, 0x2020);

		/* for MT7530 core clock = 500Mhz */
		_mt7620_mii_write(gsw, 0, 13, 0x1f);
		_mt7620_mii_write(gsw, 0, 14, 0x40e);
		_mt7620_mii_write(gsw, 0, 13, 0x401f);
		_mt7620_mii_write(gsw, 0, 14, 0x119);

		/* enable MT7530 PLL */
		_mt7620_mii_write(gsw, 0, 13, 0x1f);
		_mt7620_mii_write(gsw, 0, 14, 0x40d);
		_mt7620_mii_write(gsw, 0, 13, 0x401f);
		_mt7620_mii_write(gsw, 0, 14, 0x2820);

		usleep_range(20, 30);

		/* enable MT7530 core clock */
		_mt7620_mii_write(gsw, 0, 13, 0x1f);
		_mt7620_mii_write(gsw, 0, 14, 0x410);
		_mt7620_mii_write(gsw, 0, 13, 0x401f);
	} else {
		/* 20Mhz Xtal - TODO */
	}

	/* RGMII */
	_mt7620_mii_write(gsw, 0, 14, 0x1);

	/* set MT7530 central align */
	val = mt7530_mdio_r32(gsw, 0x7830);
	val &= ~1;
	val |= 1 << 1;
	mt7530_mdio_w32(gsw, 0x7830, val);

	val = mt7530_mdio_r32(gsw, 0x7a40);
	val &= ~(1 << 30);
	mt7530_mdio_w32(gsw, 0x7a40, val);

	mt7530_mdio_w32(gsw, 0x7a78, 0x855);
	mt7530_mdio_w32(gsw, 0x7b00, 0x102);	/* delay setting for 10/1000M */
	mt7530_mdio_w32(gsw, 0x7b04, 0x14);	/* delay setting for 10/1000M */

	/*Tx Driving */
	mt7530_mdio_w32(gsw, 0x7a54, 0x44);	/* lower driving */
	mt7530_mdio_w32(gsw, 0x7a5c, 0x44);	/* lower driving */
	mt7530_mdio_w32(gsw, 0x7a64, 0x44);	/* lower driving */
	mt7530_mdio_w32(gsw, 0x7a6c, 0x44);	/* lower driving */
	mt7530_mdio_w32(gsw, 0x7a74, 0x44);	/* lower driving */
	mt7530_mdio_w32(gsw, 0x7a7c, 0x44);	/* lower driving */

	/* lan_wan_partition(); */

	/* turn on all PHYs */
	for (i = 0; i <= 4; i++) {
		val = _mt7620_mii_read(gsw, i, 0);
		val &= ~BIT(11);
		_mt7620_mii_write(gsw, i, 0, val);
	}

	/* enable irq */
	val = mt7530_mdio_r32(gsw, 0x7808);
	val |= 3 << 16;
	mt7530_mdio_w32(gsw, 0x7808, val);
}

static void wait_loop(struct mt7620_gsw *gsw)
{
	int i, j;
	int read_data;

	j = 0;
	while (j < 10) {
		for (i = 0; i < 32; i = i + 1)
			read_data = gsw_r32(gsw, 0x610);
		j++;
	}
}

void trgmii_calibration_7623(struct mt7620_gsw *gsw)
{
	/* minimum delay for all correct */
	unsigned int tap_a[5] = { 0, 0, 0, 0, 0 };
	/* maximum delay for all correct */
	unsigned int tap_b[5] = { 0, 0, 0, 0, 0 };
	unsigned int final_tap[5];
	unsigned int rxc_step_size;
	unsigned int rxd_step_size;
	unsigned int read_data;
	unsigned int tmp;
	unsigned int rd_wd;
	int i;
	unsigned int err_cnt[5];
	unsigned int init_toggle_data;
	unsigned int err_flag[5];
	unsigned int err_total_flag;
	unsigned int training_word;
	unsigned int rd_tap;
	unsigned int is_mt7623_e1 = 0;
	u32 val;

	u32 TRGMII_7623_base;
	u32 TRGMII_7623_RD_0;
	u32 TRGMII_RCK_CTRL;

	TRGMII_7623_base = 0x300;	/* 0xFB110300 */
	TRGMII_7623_RD_0 = TRGMII_7623_base + 0x10;
	TRGMII_RCK_CTRL = TRGMII_7623_base;
	rxd_step_size = 0x1;
	rxc_step_size = 0x4;
	init_toggle_data = 0x00000055;
	training_word = 0x000000AC;

	pr_err("Calibration begin ........");
	/* RX clock gating in MT7623 */
	/* Assert RX  reset in MT7623 */
	/* Set TX OE edge in  MT7623 */
	/* Disable RX clock gating in MT7623 */
	/* Release RX reset in MT7623 */
	val = gsw_r32(gsw, TRGMII_7623_base + 0x04) & 0x3fffffff;
	gsw_w32(gsw, val, TRGMII_7623_base + 0x04);
	val = gsw_r32(gsw, TRGMII_7623_base + 0x00) | 0x80000000;
	gsw_w32(gsw, val, TRGMII_7623_base + 0x00);
	val = gsw_r32(gsw, TRGMII_7623_base + 0x78) | 0x00002000;
	gsw_w32(gsw, val, TRGMII_7623_base + 0x78);
	val = gsw_r32(gsw, TRGMII_7623_base + 0x04) | 0xC0000000;
	gsw_w32(gsw, val, TRGMII_7623_base + 0x04);
	val = gsw_r32(gsw, TRGMII_7623_base) & 0x7fffffff;
	gsw_w32(gsw, val, TRGMII_7623_base);
	/* pr_err("Check Point 1 .....\n"); */
	for (i = 0; i < 5; i++) {
		val = gsw_r32(gsw, TRGMII_7623_RD_0 + i * 8) | 0x80000000;
		gsw_w32(gsw, val, TRGMII_7623_RD_0 + i * 8);
	}

	pr_err("Enable Training Mode in MT7530\n");
	read_data = mt7530_mdio_r32(gsw, 0x7A40);
	read_data |= 0xC0000000;
	/* Enable Training Mode in MT7530 */
	mt7530_mdio_w32(gsw, 0x7A40, read_data);
	err_total_flag = 0;
	/* pr_err("Adjust RXC delay in MT7623\n"); */
	read_data = 0x0;
	while (err_total_flag == 0 && read_data != 0x68) {
		/* pr_err("2nd Enable EDGE CHK in MT7623\n"); */
		/* Enable EDGE CHK in MT7623 */
		for (i = 0; i < 5; i++) {
			val =
			    gsw_r32(gsw, TRGMII_7623_RD_0 + i * 8) | 0x40000000;
			val &= 0x4fffffff;
			gsw_w32(gsw, val, TRGMII_7623_RD_0 + i * 8);
		}
		wait_loop(gsw);
		err_total_flag = 1;
		for (i = 0; i < 5; i++) {
			err_cnt[i] =
			    gsw_r32(gsw, TRGMII_7623_RD_0 + i * 8) >> 8;
			err_cnt[i] &= 0x0000000f;
			rd_wd = gsw_r32(gsw, TRGMII_7623_RD_0 + i * 8) >> 16;
			rd_wd &= 0x000000ff;
			val = gsw_r32(gsw, TRGMII_7623_RD_0 + i * 8);
			/* pr_err("ERR_CNT = %d, RD_WD =%x, TRGMII_7623_RD_0 */
			/* =%x\n",err_cnt[i], rd_wd, val); */
			if (err_cnt[i] != 0)
				err_flag[i] = 1;
			else if (rd_wd != 0x55)
				err_flag[i] = 1;
			else
				err_flag[i] = 0;

			err_total_flag = err_flag[i] & err_total_flag;
		}

		/* pr_err("2nd Disable EDGE CHK in MT7623\n"); */
		/* Disable EDGE CHK in MT7623 */
		for (i = 0; i < 5; i++) {
			val =
			    gsw_r32(gsw, TRGMII_7623_RD_0 + i * 8) | 0x40000000;
			val &= 0x4fffffff;
			gsw_w32(gsw, val, TRGMII_7623_RD_0 + i * 8);
		}
		wait_loop(gsw);
		/* pr_err("2nd Disable EDGE CHK in MT7623\n"); */
		/* Adjust RXC delay */
		/* RX clock gating in MT7623 */
		val = gsw_r32(gsw, TRGMII_7623_base + 0x04) & 0x3fffffff;
		gsw_w32(gsw, val, TRGMII_7623_base + 0x04);
		read_data = gsw_r32(gsw, TRGMII_7623_base);
		if (err_total_flag == 0) {
			tmp = (read_data & 0x0000007f) + rxc_step_size;
			pr_err(" RXC delay = %d\n", tmp);
			read_data >>= 8;
			read_data &= 0xffffff80;
			read_data |= tmp;
			read_data <<= 8;
			read_data &= 0xffffff80;
			read_data |= tmp;
			gsw_w32(gsw, read_data, TRGMII_7623_base);
		} else {
			tmp = (read_data & 0x0000007f) + 16;
			pr_err(" RXC delay = %d\n", tmp);
			read_data >>= 8;
			read_data &= 0xffffff80;
			read_data |= tmp;
			read_data <<= 8;
			read_data &= 0xffffff80;
			read_data |= tmp;
			gsw_w32(gsw, read_data, TRGMII_7623_base);
		}
		read_data &= 0x000000ff;

		/* Disable RX clock gating in MT7623 */
		val = gsw_r32(gsw, TRGMII_7623_base + 0x04) | 0xC0000000;
		gsw_w32(gsw, val, TRGMII_7623_base + 0x04);
		for (i = 0; i < 5; i++) {
			/* Set bslip_en = ~bit_slip_en */
			val =
			    gsw_r32(gsw, TRGMII_7623_RD_0 + i * 8) | 0x80000000;
			gsw_w32(gsw, val, TRGMII_7623_RD_0 + i * 8);
		}
	}
	/* pr_err("Finish RXC Adjustment while loop\n"); */
	/* pr_err("Read RD_WD MT7623\n"); */
	/* Read RD_WD MT7623 */
	for (i = 0; i < 5; i++) {
		rd_tap = 0;
		while (err_flag[i] != 0 && rd_tap != 128) {
			/* Enable EDGE CHK in MT7623 */
			val =
			    gsw_r32(gsw, TRGMII_7623_RD_0 + i * 8) | 0x40000000;
			val &= 0x4fffffff;
			gsw_w32(gsw, val, TRGMII_7623_RD_0 + i * 8);
			wait_loop(gsw);
			read_data = gsw_r32(gsw, TRGMII_7623_RD_0 + i * 8);
			/* Read MT7623 Errcnt */
			err_cnt[i] = (read_data >> 8) & 0x0000000f;
			rd_wd = (read_data >> 16) & 0x000000ff;
			if (err_cnt[i] != 0 || rd_wd != 0x55)
				err_flag[i] = 1;
			else
				err_flag[i] = 0;
			/* Disable EDGE CHK in MT7623 */
			val =
			    gsw_r32(gsw, TRGMII_7623_RD_0 + i * 8) & 0x4fffffff;
			val |= 0x40000000;
			val &= 0x4fffffff;
			gsw_w32(gsw, val, TRGMII_7623_RD_0 + i * 8);
			wait_loop(gsw);
			if (err_flag[i] != 0) {
				/* Add RXD delay in MT7623 */
				rd_tap =
				    (read_data & 0x0000007f) + rxd_step_size;
				read_data = (read_data & 0xffffff80) | rd_tap;
				gsw_w32(gsw, read_data,
					TRGMII_7623_RD_0 + i * 8);
				tap_a[i] = rd_tap;
			} else {
				rd_tap = (read_data & 0x0000007f) + 48;
				read_data = (read_data & 0xffffff80) | rd_tap;
				gsw_w32(gsw, read_data,
					TRGMII_7623_RD_0 + i * 8);
			}
		}
		pr_err("MT7623 %dth bit  Tap_a = %d\n", i, tap_a[i]);
	}
	/* pr_err("Last While Loop\n"); */
	for (i = 0; i < 5; i++) {
		while ((err_flag[i] == 0) && (rd_tap != 128)) {
			read_data = gsw_r32(gsw, TRGMII_7623_RD_0 + i * 8);
			/* Add RXD delay in MT7623 */
			rd_tap = (read_data & 0x0000007f) + rxd_step_size;
			read_data = (read_data & 0xffffff80) | rd_tap;
			gsw_w32(gsw, read_data, TRGMII_7623_RD_0 + i * 8);
			/* Enable EDGE CHK in MT7623 */
			val =
			    gsw_r32(gsw, TRGMII_7623_RD_0 + i * 8) | 0x40000000;
			val &= 0x4fffffff;
			gsw_w32(gsw, val, TRGMII_7623_RD_0 + i * 8);
			wait_loop(gsw);
			read_data = gsw_r32(gsw, TRGMII_7623_RD_0 + i * 8);
			/* Read MT7623 Errcnt */
			err_cnt[i] = (read_data >> 8) & 0x0000000f;
			rd_wd = (read_data >> 16) & 0x000000ff;
			if (err_cnt[i] != 0 || rd_wd != 0x55)
				err_flag[i] = 1;
			else
				err_flag[i] = 0;

			/* Disable EDGE CHK in MT7623 */
			val = gsw_r32(gsw, TRGMII_7623_RD_0 + i * 8);
			val |= 0x40000000;
			val &= 0x4fffffff;
			gsw_w32(gsw, val, TRGMII_7623_RD_0 + i * 8);
			wait_loop(gsw);
		}

		tap_b[i] = rd_tap;	/* -rxd_step_size; */
		pr_err("MT7623 %dth bit  Tap_b = %d\n", i, tap_b[i]);
		/* Calculate RXD delay = (TAP_A + TAP_B)/2 */
		final_tap[i] = (tap_a[i] + tap_b[i]) / 2;
		read_data = (read_data & 0xffffff80) | final_tap[i];
		gsw_w32(gsw, read_data, TRGMII_7623_RD_0 + i * 8);
	}

	read_data = mt7530_mdio_r32(gsw, 0x7A40);
	read_data &= 0x3fffffff;
	mt7530_mdio_w32(gsw, 0x7A40, read_data);
}

void trgmii_calibration_7530(struct mt7620_gsw *gsw)
{
	unsigned int tap_a[5] = { 0, 0, 0, 0, 0 };
	unsigned int tap_b[5] = { 0, 0, 0, 0, 0 };
	unsigned int final_tap[5];
	unsigned int rxc_step_size;
	unsigned int rxd_step_size;
	unsigned int read_data;
	unsigned int tmp;
	int i;
	unsigned int err_cnt[5];
	unsigned int rd_wd;
	unsigned int init_toggle_data;
	unsigned int err_flag[5];
	unsigned int err_total_flag;
	unsigned int training_word;
	unsigned int rd_tap;
	unsigned int is_mt7623_e1 = 0;

	u32 TRGMII_7623_base;
	u32 TRGMII_7530_RD_0;
	u32 TRGMII_RXCTL;
	u32 TRGMII_RCK_CTRL;
	u32 TRGMII_7530_base;
	u32 TRGMII_7530_TX_base;
	u32 val;

	TRGMII_7623_base = 0x300;
	TRGMII_7530_base = 0x7A00;
	TRGMII_7530_RD_0 = TRGMII_7530_base + 0x10;
	TRGMII_RCK_CTRL = TRGMII_7623_base;
	rxd_step_size = 0x1;
	rxc_step_size = 0x8;
	init_toggle_data = 0x00000055;
	training_word = 0x000000AC;

	TRGMII_7530_TX_base = TRGMII_7530_base + 0x50;

	/* pr_err("Calibration begin ........\n"); */
	val = gsw_r32(gsw, TRGMII_7623_base + 0x40) | 0x80000000;
	gsw_w32(gsw, val, TRGMII_7623_base + 0x40);
	read_data = mt7530_mdio_r32(gsw, 0x7a10);
	/* pr_err("TRGMII_7530_RD_0 is %x\n", read_data); */

	read_data = mt7530_mdio_r32(gsw, TRGMII_7530_base + 0x04);
	read_data &= 0x3fffffff;
	/* RX clock gating in MT7530 */
	mt7530_mdio_w32(gsw, TRGMII_7530_base + 0x04, read_data);

	read_data = mt7530_mdio_r32(gsw, TRGMII_7530_base + 0x78);
	read_data |= 0x00002000;
	/* Set TX OE edge in  MT7530 */
	mt7530_mdio_w32(gsw, TRGMII_7530_base + 0x78, read_data);

	read_data = mt7530_mdio_r32(gsw, TRGMII_7530_base);
	read_data |= 0x80000000;
	/* Assert RX  reset in MT7530 */
	mt7530_mdio_w32(gsw, TRGMII_7530_base, read_data);

	read_data = mt7530_mdio_r32(gsw, TRGMII_7530_base);
	read_data &= 0x7fffffff;
	/* Release RX reset in MT7530 */
	mt7530_mdio_w32(gsw, TRGMII_7530_base, read_data);

	read_data = mt7530_mdio_r32(gsw, TRGMII_7530_base + 0x04);
	read_data |= 0xC0000000;
	/* Disable RX clock gating in MT7530 */
	mt7530_mdio_w32(gsw, TRGMII_7530_base + 0x04, read_data);

	/* pr_err("Enable Training Mode in MT7623\n"); */
	/*Enable Training Mode in MT7623 */
	val = gsw_r32(gsw, TRGMII_7623_base + 0x40) | 0x80000000;
	gsw_w32(gsw, val, TRGMII_7623_base + 0x40);
	if (gsw->trgmii_force == 2000) {
		val = gsw_r32(gsw, TRGMII_7623_base + 0x40) | 0xC0000000;
		gsw_w32(gsw, val, TRGMII_7623_base + 0x40);
	} else {
		val = gsw_r32(gsw, TRGMII_7623_base + 0x40) | 0x80000000;
		gsw_w32(gsw, val, TRGMII_7623_base + 0x40);
	}
	val = gsw_r32(gsw, TRGMII_7623_base + 0x078) & 0xfffff0ff;
	gsw_w32(gsw, val, TRGMII_7623_base + 0x078);
	val = gsw_r32(gsw, TRGMII_7623_base + 0x50) & 0xfffff0ff;
	gsw_w32(gsw, val, TRGMII_7623_base + 0x50);
	val = gsw_r32(gsw, TRGMII_7623_base + 0x58) & 0xfffff0ff;
	gsw_w32(gsw, val, TRGMII_7623_base + 0x58);
	val = gsw_r32(gsw, TRGMII_7623_base + 0x60) & 0xfffff0ff;
	gsw_w32(gsw, val, TRGMII_7623_base + 0x60);
	val = gsw_r32(gsw, TRGMII_7623_base + 0x68) & 0xfffff0ff;
	gsw_w32(gsw, val, TRGMII_7623_base + 0x68);
	val = gsw_r32(gsw, TRGMII_7623_base + 0x70) & 0xfffff0ff;
	gsw_w32(gsw, val, TRGMII_7623_base + 0x70);
	val = gsw_r32(gsw, TRGMII_7623_base + 0x78) & 0x00000800;
	gsw_w32(gsw, val, TRGMII_7623_base + 0x78);
	err_total_flag = 0;
	/* pr_err("Adjust RXC delay in MT7530\n"); */
	read_data = 0x0;
	while (err_total_flag == 0 && (read_data != 0x68)) {
		/* pr_err("2nd Enable EDGE CHK in MT7530\n"); */
		/* Enable EDGE CHK in MT7530 */
		for (i = 0; i < 5; i++) {
			read_data =
			    mt7530_mdio_r32(gsw, TRGMII_7530_RD_0 + i * 8);
			read_data |= 0x40000000;
			read_data &= 0x4fffffff;
			mt7530_mdio_w32(gsw, TRGMII_7530_RD_0 + i * 8,
					read_data);
			wait_loop(gsw);
			/* pr_err("2nd Disable EDGE CHK in MT7530\n"); */
			err_cnt[i] =
			    mt7530_mdio_r32(gsw, TRGMII_7530_RD_0 + i * 8);
			/* pr_err("MT7530 %dbit ERR_CNT=%x\n",i, err_cnt[i]); */
			err_cnt[i] >>= 8;
			err_cnt[i] &= 0x0000ff0f;
			rd_wd = err_cnt[i] >> 8;
			rd_wd &= 0x000000ff;
			err_cnt[i] &= 0x0000000f;
			/* read_data=mt7530_mdio_r32(gsw,0x7a10,&read_data); */
			if (err_cnt[i] != 0)
				err_flag[i] = 1;
			else if (rd_wd != 0x55)
				err_flag[i] = 1;
			else
				err_flag[i] = 0;

			if (i == 0)
				err_total_flag = err_flag[i];
			else
				err_total_flag = err_flag[i] & err_total_flag;

			/* Disable EDGE CHK in MT7530 */
			read_data =
			    mt7530_mdio_r32(gsw, TRGMII_7530_RD_0 + i * 8);
			read_data |= 0x40000000;
			read_data &= 0x4fffffff;
			mt7530_mdio_w32(gsw, TRGMII_7530_RD_0 + i * 8,
					read_data);
			wait_loop(gsw);
		}
		/*Adjust RXC delay */
		if (err_total_flag == 0) {
			read_data = mt7530_mdio_r32(gsw, TRGMII_7530_base);
			read_data |= 0x80000000;
			/* Assert RX  reset in MT7530 */
			mt7530_mdio_w32(gsw, TRGMII_7530_base, read_data);

			read_data =
			    mt7530_mdio_r32(gsw, TRGMII_7530_base + 0x04);
			read_data &= 0x3fffffff;
			/* RX clock gating in MT7530 */
			mt7530_mdio_w32(gsw, TRGMII_7530_base + 0x04,
					read_data);

			read_data = mt7530_mdio_r32(gsw, TRGMII_7530_base);
			tmp = read_data;
			tmp &= 0x0000007f;
			tmp += rxc_step_size;
			/* pr_err("Current rxc delay = %d\n", tmp); */
			read_data &= 0xffffff80;
			read_data |= tmp;
			mt7530_mdio_w32(gsw, TRGMII_7530_base, read_data);
			read_data = mt7530_mdio_r32(gsw, TRGMII_7530_base);
			/* pr_err("Current RXC delay = %x\n", read_data); */

			read_data = mt7530_mdio_r32(gsw, TRGMII_7530_base);
			read_data &= 0x7fffffff;
			/* Release RX reset in MT7530 */
			mt7530_mdio_w32(gsw, TRGMII_7530_base, read_data);

			read_data =
				mt7530_mdio_r32(gsw, TRGMII_7530_base + 0x04);
			read_data |= 0xc0000000;
			/* Disable RX clock gating in MT7530 */
			mt7530_mdio_w32(gsw, TRGMII_7530_base + 0x04,
					read_data);
			/* pr_err("####### MT7530 RXC delay is %d\n", tmp); */
		}
		read_data = tmp;
	}
	/* pr_err("Finish RXC Adjustment while loop\n"); */

	/* pr_err("Read RD_WD MT7530\n"); */
	/* Read RD_WD MT7530 */
	for (i = 0; i < 5; i++) {
		rd_tap = 0;
		while (err_flag[i] != 0 && rd_tap != 128) {
			/* Enable EDGE CHK in MT7530 */
			read_data =
			    mt7530_mdio_r32(gsw, TRGMII_7530_RD_0 + i * 8);
			read_data |= 0x40000000;
			read_data &= 0x4fffffff;
			mt7530_mdio_w32(gsw, TRGMII_7530_RD_0 + i * 8,
					read_data);
			wait_loop(gsw);
			err_cnt[i] = (read_data >> 8) & 0x0000000f;
			rd_wd = (read_data >> 16) & 0x000000ff;
			if (err_cnt[i] != 0 || rd_wd != 0x55)
				err_flag[i] = 1;
			else
				err_flag[i] = 0;

			if (err_flag[i] != 0) {
				/* Add RXD delay in MT7530 */
				rd_tap =
				    (read_data & 0x0000007f) + rxd_step_size;
				read_data = (read_data & 0xffffff80) | rd_tap;
				mt7530_mdio_w32(gsw, TRGMII_7530_RD_0 + i * 8,
						read_data);
				tap_a[i] = rd_tap;
			} else {
				/* Record the min delay TAP_A */
				tap_a[i] = (read_data & 0x0000007f);
				rd_tap = tap_a[i] + 0x4;
				read_data = (read_data & 0xffffff80) | rd_tap;
				mt7530_mdio_w32(gsw, TRGMII_7530_RD_0 + i * 8,
						read_data);
			}

			/* Disable EDGE CHK in MT7530 */
			read_data =
			    mt7530_mdio_r32(gsw, TRGMII_7530_RD_0 + i * 8);
			read_data |= 0x40000000;
			read_data &= 0x4fffffff;
			mt7530_mdio_w32(gsw, TRGMII_7530_RD_0 + i * 8,
					read_data);
			wait_loop(gsw);
		}
		pr_err("MT7530 %dth bit  Tap_a = %d\n", i, tap_a[i]);
	}
	/* pr_err("Last While Loop\n"); */
	for (i = 0; i < 5; i++) {
		rd_tap = 0;
		while (err_flag[i] == 0 && (rd_tap != 128)) {
			/* Enable EDGE CHK in MT7530 */
			read_data =
			    mt7530_mdio_r32(gsw, TRGMII_7530_RD_0 + i * 8);
			read_data |= 0x40000000;
			read_data &= 0x4fffffff;
			mt7530_mdio_w32(gsw, TRGMII_7530_RD_0 + i * 8,
					read_data);
			wait_loop(gsw);
			err_cnt[i] = (read_data >> 8) & 0x0000000f;
			rd_wd = (read_data >> 16) & 0x000000ff;

			if (err_cnt[i] != 0 || rd_wd != 0x55)
				err_flag[i] = 1;
			else
				err_flag[i] = 0;

			if (err_flag[i] == 0 && (rd_tap != 128)) {
				/* Add RXD delay in MT7530 */
				rd_tap =
				    (read_data & 0x0000007f) + rxd_step_size;
				read_data = (read_data & 0xffffff80) | rd_tap;
				mt7530_mdio_w32(gsw, TRGMII_7530_RD_0 + i * 8,
						read_data);
			}
			/* Disable EDGE CHK in MT7530 */
			read_data =
			    mt7530_mdio_r32(gsw, TRGMII_7530_RD_0 + i * 8);
			read_data |= 0x40000000;
			read_data &= 0x4fffffff;
			mt7530_mdio_w32(gsw, TRGMII_7530_RD_0 + i * 8,
					read_data);
			wait_loop(gsw);
		}
		tap_b[i] = rd_tap;	/* - rxd_step_size; */
		pr_err("MT7530 %dth bit  Tap_b = %d\n", i, tap_b[i]);
		/* Calculate RXD delay = (TAP_A + TAP_B)/2 */
		final_tap[i] = (tap_a[i] + tap_b[i]) / 2;
		/* pr_err("MT7530 %dbit Final Tap = %d\n", i, final_tap[i]); */

		read_data = (read_data & 0xffffff80) | final_tap[i];
		mt7530_mdio_w32(gsw, TRGMII_7530_RD_0 + i * 8, read_data);
	}
	if (gsw->trgmii_force == 2000) {
		val = gsw_r32(gsw, TRGMII_7623_base + 0x40) & 0x7fffffff;
		gsw_w32(gsw, val, TRGMII_7623_base + 0x40);
	} else {
		val = gsw_r32(gsw, TRGMII_7623_base + 0x40) & 0x3fffffff;
		gsw_w32(gsw, val, TRGMII_7623_base + 0x40);
	}
}

void mt7530_trgmii_clock_setting(struct mt7620_gsw *gsw, u32 xtal_mode)
{
	u32 reg_val;
	u32 val;
	/* TRGMII Clock */
	_mt7620_mii_write(gsw, 0, 13, 0x1f);
	_mt7620_mii_write(gsw, 0, 14, 0x410);
	_mt7620_mii_write(gsw, 0, 13, 0x401f);
	_mt7620_mii_write(gsw, 0, 14, 0x1);
	_mt7620_mii_write(gsw, 0, 13, 0x1f);
	_mt7620_mii_write(gsw, 0, 14, 0x404);
	_mt7620_mii_write(gsw, 0, 13, 0x401f);
	if (xtal_mode == 1) {	/* 25MHz */
		if (gsw->trgmii_force == 2600)
			_mt7620_mii_write(gsw, 0, 14, 0x1a00);	/* 325MHz */
		if (gsw->trgmii_force == 2000)
			_mt7620_mii_write(gsw, 0, 14, 0x1400);	/* 250MHz */
	} else if (xtal_mode == 2) {	/* 40MHz */
		if (gsw->trgmii_force == 2600)
			_mt7620_mii_write(gsw, 0, 14, 0x1040);	/* 325MHz */
		if (gsw->trgmii_force == 2000)
			_mt7620_mii_write(gsw, 0, 14, 0x0c80);	/* 250MHz */
	}
	_mt7620_mii_write(gsw, 0, 13, 0x1f);
	_mt7620_mii_write(gsw, 0, 14, 0x405);
	_mt7620_mii_write(gsw, 0, 13, 0x401f);
	_mt7620_mii_write(gsw, 0, 14, 0x0);

	_mt7620_mii_write(gsw, 0, 13, 0x1f);
	_mt7620_mii_write(gsw, 0, 14, 0x409);
	_mt7620_mii_write(gsw, 0, 13, 0x401f);
	if (xtal_mode == 1)	/* 25MHz */
		_mt7620_mii_write(gsw, 0, 14, 0x0057);
	else
		_mt7620_mii_write(gsw, 0, 14, 0x0087);

	_mt7620_mii_write(gsw, 0, 13, 0x1f);
	_mt7620_mii_write(gsw, 0, 14, 0x40a);
	_mt7620_mii_write(gsw, 0, 13, 0x401f);
	if (xtal_mode == 1)	/* 25MHz */
		_mt7620_mii_write(gsw, 0, 14, 0x0057);
	else
		_mt7620_mii_write(gsw, 0, 14, 0x0087);

	_mt7620_mii_write(gsw, 0, 13, 0x1f);
	_mt7620_mii_write(gsw, 0, 14, 0x403);
	_mt7620_mii_write(gsw, 0, 13, 0x401f);
	_mt7620_mii_write(gsw, 0, 14, 0x1800);

	_mt7620_mii_write(gsw, 0, 13, 0x1f);
	_mt7620_mii_write(gsw, 0, 14, 0x403);
	_mt7620_mii_write(gsw, 0, 13, 0x401f);
	_mt7620_mii_write(gsw, 0, 14, 0x1c00);

	_mt7620_mii_write(gsw, 0, 13, 0x1f);
	_mt7620_mii_write(gsw, 0, 14, 0x401);
	_mt7620_mii_write(gsw, 0, 13, 0x401f);
	_mt7620_mii_write(gsw, 0, 14, 0xc020);

	_mt7620_mii_write(gsw, 0, 13, 0x1f);
	_mt7620_mii_write(gsw, 0, 14, 0x406);
	_mt7620_mii_write(gsw, 0, 13, 0x401f);
	_mt7620_mii_write(gsw, 0, 14, 0xa030);

	_mt7620_mii_write(gsw, 0, 13, 0x1f);
	_mt7620_mii_write(gsw, 0, 14, 0x406);
	_mt7620_mii_write(gsw, 0, 13, 0x401f);
	_mt7620_mii_write(gsw, 0, 14, 0xa038);

	usleep_range(120, 140);		/* for MT7623 bring up test */

	_mt7620_mii_write(gsw, 0, 13, 0x1f);
	_mt7620_mii_write(gsw, 0, 14, 0x410);
	_mt7620_mii_write(gsw, 0, 13, 0x401f);
	_mt7620_mii_write(gsw, 0, 14, 0x3);

	reg_val = mt7530_mdio_r32(gsw, 0x7830);
	reg_val &= 0xFFFFFFFC;
	reg_val |= 0x00000001;
	mt7530_mdio_w32(gsw, 0x7830, reg_val);

	reg_val = mt7530_mdio_r32(gsw, 0x7a40);
	reg_val &= ~(0x1 << 30);
	reg_val &= ~(0x1 << 28);
	mt7530_mdio_w32(gsw, 0x7a40, reg_val);

	mt7530_mdio_w32(gsw, 0x7a78, 0x55);
	usleep_range(100, 125);		/* for mt7623 bring up test */

	val = gsw_r32(gsw, 0x300) & 0x7fffffff;
	gsw_w32(gsw, val, 0x300);

	trgmii_calibration_7623(gsw);
	trgmii_calibration_7530(gsw);
	val = gsw_r32(gsw, 0x300) | 0x80000000;
	gsw_w32(gsw, val, 0x300);
	val = gsw_r32(gsw, 0x300) & 0x7fffffff;
	gsw_w32(gsw, val, 0x300);

	/*MT7530 RXC reset */
	reg_val = mt7530_mdio_r32(gsw, 0x7a00);
	reg_val |= (0x1 << 31);
	mt7530_mdio_w32(gsw, 0x7a00, reg_val);
	mdelay(1);
	reg_val &= ~(0x1 << 31);
	mt7530_mdio_w32(gsw, 0x7a00, reg_val);
	mdelay(100);
}

static void gsw_hw_init_mt7623(struct mt7620_gsw *gsw, struct device_node *np)
{
	u32 i;
	u32 val;
	u32 xtal_mode, reg_val;

	__gsw = gsw;
	pr_err("gsw_hw_init_mt7623\n");
	val = rt_sysc_r32(0x002c) | (1 << 11);
	rt_sysc_w32(val, 0x002c);
	val = gsw_r32(gsw, 0x390) | 0x00000002;
	gsw_w32(gsw, val, 0x390);
	/*pr_err("Assert MT7623 RXC reset\n"); */
	val = gsw_r32(gsw, 0x300) | 0x80000000;
	gsw_w32(gsw, val, 0x300);
	/* Hardware reset Switch */
	fe_reset(RST_CTRL_MCM);	/* BIT(2) */
	/* udelay(10000); */

	/* Wait for Switch Reset Completed */
	for (i = 0; i < 100; i++) {
		mdelay(10);
		reg_val = mt7530_mdio_r32(gsw, 0x7800);
		if (reg_val != 0) {
			pr_err("MT7530 Reset Completed!!\n");
			break;
		}
		if (i == 99)
			pr_err("MT7530 Reset Timeout!!\n");
	}

	/* turn off all PHYs */
	for (i = 0; i <= 4; i++) {
		val = _mt7620_mii_read(gsw, i, 0x0);
		/* pr_err("mt7620_mii_read value = %d\n", val); */
		val |= (0x1 << 11);
		_mt7620_mii_write(gsw, i, 0x0, val);
		/* pr_err("twice value=%d\n", _mt7620_mii_read(gsw, i, 0x0)); */
	}

	/* reset the switch */
	mt7530_mdio_w32(gsw, 0x7000, 0x3);
	usleep_range(100, 125);

	/* (GE1, Force 1000M/FD, FC ON) */
	gsw_w32(gsw, 0x2005e33b, 0x100);
	mt7530_mdio_w32(gsw, 0x3600, 0x5e33b);

	val = mt7530_mdio_r32(gsw, 0x3600);
	/* (GE2, Link down) */
	gsw_w32(gsw, 0x8000, 0x200);

	/* val = 0x117ccf; //Enable Port 6, P5 as GMAC5, P5 disable */
	val = mt7530_mdio_r32(gsw, 0x7804);
	pr_err("HW-TRAP is 0x%x\n", val);
	val &= ~(1 << 8);	/* Enable Port 6 */
	val |= (1 << 6);	/* Disable Port 5 */
	val |= (1 << 13);	/* Port 5 as GMAC, no Internal PHY */

	val &= ~(1 << 5);
	val |= (1 << 16);	/* change HW-TRAP */
	/* ////// */
	/* val = 0x1017edf; */
	/* ///// */
	pr_err("change HW-TRAP to 0x%x\n", val);
	mt7530_mdio_w32(gsw, 0x7804, val);

	val = mt7530_mdio_r32(gsw, 0x7800);
	val = (val >> 9) & 0x3;
	pr_err("!!%s: Mhz value= %d\n", __func__, val);
	if (val == 0x3) {
		xtal_mode = 1;
		/* 25Mhz Xtal - do nothing */
	} else if (val == 0x2) {
		/* 40Mhz */
		xtal_mode = 2;
		/* disable MT7530 core clock */
		_mt7620_mii_write(gsw, 0, 13, 0x1f);
		_mt7620_mii_write(gsw, 0, 14, 0x410);
		_mt7620_mii_write(gsw, 0, 13, 0x401f);
		_mt7620_mii_write(gsw, 0, 14, 0x0);

		/* disable MT7530 PLL */
		_mt7620_mii_write(gsw, 0, 13, 0x1f);
		_mt7620_mii_write(gsw, 0, 14, 0x40d);
		_mt7620_mii_write(gsw, 0, 13, 0x401f);
		_mt7620_mii_write(gsw, 0, 14, 0x2020);

		/* for MT7530 core clock = 500Mhz */
		_mt7620_mii_write(gsw, 0, 13, 0x1f);
		_mt7620_mii_write(gsw, 0, 14, 0x40e);
		_mt7620_mii_write(gsw, 0, 13, 0x401f);
		_mt7620_mii_write(gsw, 0, 14, 0x119);

		/* enable MT7530 PLL */
		_mt7620_mii_write(gsw, 0, 13, 0x1f);
		_mt7620_mii_write(gsw, 0, 14, 0x40d);
		_mt7620_mii_write(gsw, 0, 13, 0x401f);
		_mt7620_mii_write(gsw, 0, 14, 0x2820);

		usleep_range(20, 30);

		/* enable MT7530 core clock */
		_mt7620_mii_write(gsw, 0, 13, 0x1f);
		_mt7620_mii_write(gsw, 0, 14, 0x410);
		_mt7620_mii_write(gsw, 0, 13, 0x401f);
	} else {
		xtal_mode = 3;
		/* 20Mhz Xtal - TODO */
	}

	/* RGMII */
	_mt7620_mii_write(gsw, 0, 14, 0x1);

	/* set MT7530 central align */
	val = mt7530_mdio_r32(gsw, 0x7830);
	val &= ~1;
	val |= 1 << 1;
	mt7530_mdio_w32(gsw, 0x7830, val);

	val = mt7530_mdio_r32(gsw, 0x7a40);
	val &= ~(1 << 30);
	mt7530_mdio_w32(gsw, 0x7a40, val);

	mt7530_mdio_w32(gsw, 0x7a78, 0x855);
	mt7530_mdio_w32(gsw, 0x7b00, 0x104);	/* delay setting for 10/1000M */
	mt7530_mdio_w32(gsw, 0x7b04, 0x10);	/* delay setting for 10/1000M */

	/*Tx Driving */
	mt7530_mdio_w32(gsw, 0x7a54, 0x88);	/* lower driving */
	mt7530_mdio_w32(gsw, 0x7a5c, 0x88);	/* lower driving */
	mt7530_mdio_w32(gsw, 0x7a64, 0x88);	/* lower driving */
	mt7530_mdio_w32(gsw, 0x7a6c, 0x88);	/* lower driving */
	mt7530_mdio_w32(gsw, 0x7a74, 0x88);	/* lower driving */
	mt7530_mdio_w32(gsw, 0x7a7c, 0x88);	/* lower driving */
	mt7530_mdio_w32(gsw, 0x7810, 0x11);	/* lower GE2 driving */

	/*Set MT7623/MT7683 TX Driving */

	gsw_w32(gsw, 0x88, 0x354);
	gsw_w32(gsw, 0x88, 0x35c);
	gsw_w32(gsw, 0x88, 0x364);
	gsw_w32(gsw, 0x88, 0x36c);
	gsw_w32(gsw, 0x88, 0x374);
	gsw_w32(gsw, 0x88, 0x37c);

	mt7530_trgmii_clock_setting(gsw, xtal_mode);

	lan_wan_partition(gsw);

	/* disable EEE!!!!!//////////////////////////////////// */
	for (i = 0; i <= 4; i++) {
		_mt7620_mii_write(gsw, i, 13, 0x7);
		_mt7620_mii_write(gsw, i, 14, 0x3C);
		_mt7620_mii_write(gsw, i, 13, 0x4007);
		_mt7620_mii_write(gsw, i, 14, 0x0);

		/* Increase SlvDPSready time */
		_mt7620_mii_write(gsw, i, 31, 0x52b5);
		_mt7620_mii_write(gsw, i, 16, 0xafae);
		_mt7620_mii_write(gsw, i, 18, 0x2f);
		_mt7620_mii_write(gsw, i, 16, 0x8fae);
		/* Incease post_update_timer */
		_mt7620_mii_write(gsw, i, 31, 0x3);
		_mt7620_mii_write(gsw, i, 17, 0x4b);
		/* Adjust 100_mse_threshold */
		_mt7620_mii_write(gsw, i, 13, 0x1e);
		_mt7620_mii_write(gsw, i, 14, 0x123);
		_mt7620_mii_write(gsw, i, 13, 0x401e);
		_mt7620_mii_write(gsw, i, 14, 0xffff);
		/* Disable mcc */
		_mt7620_mii_write(gsw, i, 13, 0x1e);
		_mt7620_mii_write(gsw, i, 14, 0xa6);
		_mt7620_mii_write(gsw, i, 13, 0x401e);
		_mt7620_mii_write(gsw, i, 14, 0x300);
		/* Disable HW auto downshift */
		_mt7620_mii_write(gsw, i, 31, 0x1);
		val = _mt7620_mii_read(gsw, i, 0x14);
		val &= ~(1 << 4);
		_mt7620_mii_write(gsw, i, 0x14, val);
	}

	/* turn on all PHYs */
	for (i = 0; i <= 4; i++) {
		val = _mt7620_mii_read(gsw, i, 0);
		val &= ~BIT(11);
		_mt7620_mii_write(gsw, i, 0, val);
	}

	/* enable irq */
	val = mt7530_mdio_r32(gsw, 0x7808);
	val |= 3 << 16;
	mt7530_mdio_w32(gsw, 0x7808, val);
}

void mt7620_set_mac(struct fe_priv *priv, unsigned char *mac)
{
	struct mt7620_gsw *gsw = (struct mt7620_gsw *)priv->soc->swpriv;
	unsigned long flags;

	spin_lock_irqsave(&priv->page_lock, flags);
	gsw_w32(gsw, (mac[0] << 8) | mac[1], GSW_REG_SMACCR1);
	gsw_w32(gsw, (mac[2] << 24) | (mac[3] << 16) | (mac[4] << 8) | mac[5],
		GSW_REG_SMACCR0);
	spin_unlock_irqrestore(&priv->page_lock, flags);
}

const struct of_device_id gsw_match[] = {
	{.compatible = "ralink,mt7620a-gsw"},
	{.compatible = "mediatek,mt7623-gsw"},
	{}
};

int mt7620_gsw_config(struct fe_priv *priv)
{
	struct mt7620_gsw *gsw = (struct mt7620_gsw *)priv->soc->swpriv;

	/* is the mt7530 internal or external */
	if (priv->mii_bus && priv->mii_bus->phy_map[0x1f]) {
		mt7530_probe(priv->device, gsw->base, NULL, 0);
		mt7530_probe(priv->device, NULL, priv->mii_bus, 1);
	} else {
		mt7530_probe(priv->device, gsw->base, NULL, 1);
	}

	return 0;
}

int mt7621_gsw_config(struct fe_priv *priv)
{
	if (priv->mii_bus && priv->mii_bus->phy_map[0x1f])
		mt7530_probe(priv->device, NULL, priv->mii_bus, 1);

	return 0;
}

int mt7620_gsw_probe(struct fe_priv *priv)
{
	struct mt7620_gsw *gsw;
	struct device_node *np;
	const char *port4 = NULL;
	int err;
	int size;
	const __be32 *list;

	np = of_find_matching_node(NULL, gsw_match);
	if (!np) {
		dev_err(priv->device, "no gsw node found\n");
		return -EINVAL;
	}
	np = of_node_get(np);

	gsw = devm_kzalloc(priv->device, sizeof(struct mt7620_gsw), GFP_KERNEL);
	if (!gsw) {
		/* dev_err(priv->device, "no memory for private data\n"); */
		return -ENOMEM;
	}

	gsw->base = of_iomap(np, 0);
	if (!gsw->base) {
		dev_err(priv->device, "gsw ioremap failed\n");
		return -ENOMEM;
	}
	pr_err("gsw->base 0x%lx\n", gsw->base);

	list =
	    of_get_property(priv->device->of_node, "ge1_trgmii_force", &size);
	if (!list)
		gsw->trgmii_force = 0;
	else
		gsw->trgmii_force = be32_to_cpup(list);

	gsw->dev = priv->device;
	priv->soc->swpriv = gsw;

	of_property_read_string(np, "ralink,port4", &port4);
	if (port4 && !strcmp(port4, "ephy"))
		gsw->port4 = PORT4_EPHY;
	else if (port4 && !strcmp(port4, "gmac"))
		gsw->port4 = PORT4_EXT;
	else
		gsw->port4 = PORT4_EPHY;	/* =0 .7621 */

	if (IS_ENABLED(CONFIG_SOC_MT7620))
		gsw_hw_init_mt7620(gsw, np);
	else if (IS_ENABLED(CONFIG_MACH_MT2701) ||
		 IS_ENABLED(CONFIG_ARCH_MT7623))
		gsw_hw_init_mt7623(gsw, np);
	else
		gsw_hw_init_mt7621(gsw, np);

	gsw->irq = irq_of_parse_and_map(np, 0);
	pr_err("gsw_irq = %d\n", gsw->irq);
	if (gsw->irq) {
		if (IS_ENABLED(CONFIG_SOC_MT7620)) {
			request_irq(gsw->irq, gsw_interrupt_mt7620, 0, "gsw",
				    priv);
			gsw_w32(gsw, ~PORT_IRQ_ST_CHG, GSW_REG_IMR);
		} else if (IS_ENABLED(CONFIG_MACH_MT2701) ||
			   IS_ENABLED(CONFIG_ARCH_MT7623)) {
			/* err = request_threaded_irq */
			/* (gsw->irq, esw_interrupt, NULL, IRQF_TRIGGER_NONE, */
			err =
			    request_threaded_irq(gsw->irq, gsw_interrupt_mt7621,
						 NULL, IRQF_TRIGGER_NONE, "gsw",
						 priv);

			if (err) {
				pr_err("request failure! gsw_irq=%d, err=%d\n",
				       gsw->irq, err);
				return err;
			}
			mt7530_mdio_w32(gsw, 0x7008, 0x1f);
		} else {
			request_irq(gsw->irq, gsw_interrupt_mt7621, 0, "gsw",
				    priv);
			mt7530_mdio_w32(gsw, 0x7008, 0x1f);
		}
	}

	return 0;
}

void is_switch_vlan_table_busy(struct mt7620_gsw *gsw)
{
	int j = 0;
	unsigned int val = 0;

	for (j = 0; j < 20; j++) {
		val = mt7530_mdio_r32(gsw, 0x90);
		if ((val & 0x80000000) == 0) {	/* table busy */
			break;
		}
		mdelay(70);
	}
	if (j == 20)
		pr_err("set vlan timeout value=0x%x.\n", val);
}

static void lan_wan_partition(struct mt7620_gsw *gsw)
{
/*Set  MT7530 */
#ifdef CONFIG_WAN_AT_P0
	pr_err("set LAN/WAN WLLLL\n");
	/* WLLLL, wan at P0 */
	/* LAN/WAN ports as security mode */
	mt7530_mdio_w32(gsw, 0x2004, 0xff0003);	/* port0 */
	mt7530_mdio_w32(gsw, 0x2104, 0xff0003);	/* port1 */
	mt7530_mdio_w32(gsw, 0x2204, 0xff0003);	/* port2 */
	mt7530_mdio_w32(gsw, 0x2304, 0xff0003);	/* port3 */
	mt7530_mdio_w32(gsw, 0x2404, 0xff0003);	/* port4 */

	/* set PVID */
	mt7530_mdio_w32(gsw, 0x2014, 0x10002);	/* port0 */
	mt7530_mdio_w32(gsw, 0x2114, 0x10001);	/* port1 */
	mt7530_mdio_w32(gsw, 0x2214, 0x10001);	/* port2 */
	mt7530_mdio_w32(gsw, 0x2314, 0x10001);	/* port3 */
	mt7530_mdio_w32(gsw, 0x2414, 0x10001);	/* port4 */
	/*port6 */
	/* VLAN member */
	is_switch_vlan_table_busy(gsw);
	mt7530_mdio_w32(gsw, 0x94, 0x407e0001);	/* VAWD1 */
	mt7530_mdio_w32(gsw, 0x90, 0x80001001);	/* VTCR, VID=1 */
	is_switch_vlan_table_busy(gsw);

	mt7530_mdio_w32(gsw, 0x94, 0x40610001);	/* VAWD1 */
	mt7530_mdio_w32(gsw, 0x90, 0x80001002);	/* VTCR, VID=2 */
	is_switch_vlan_table_busy(gsw);
#endif
#ifdef CONFIG_WAN_AT_P4
	pr_err("set LAN/WAN LLLLW\n");
	/* LLLLW, wan at P4 */
	/* LAN/WAN ports as security mode */
	mt7530_mdio_w32(gsw, 0x2004, 0xff0003);	/* port0 */
	mt7530_mdio_w32(gsw, 0x2104, 0xff0003);	/* port1 */
	mt7530_mdio_w32(gsw, 0x2204, 0xff0003);	/* port2 */
	mt7530_mdio_w32(gsw, 0x2304, 0xff0003);	/* port3 */
	mt7530_mdio_w32(gsw, 0x2404, 0xff0003);	/* port4 */

	/* set PVID */
	mt7530_mdio_w32(gsw, 0x2014, 0x10001);	/* port0 */
	mt7530_mdio_w32(gsw, 0x2114, 0x10001);	/* port1 */
	mt7530_mdio_w32(gsw, 0x2214, 0x10001);	/* port2 */
	mt7530_mdio_w32(gsw, 0x2314, 0x10001);	/* port3 */
	mt7530_mdio_w32(gsw, 0x2414, 0x10002);	/* port4 */

	/* VLAN member */
	is_switch_vlan_table_busy(gsw);
	mt7530_mdio_w32(gsw, 0x94, 0x404f0001);	/* VAWD1 */
	mt7530_mdio_w32(gsw, 0x90, 0x80001001);	/* VTCR, VID=1 */
	is_switch_vlan_table_busy(gsw);
	mt7530_mdio_w32(gsw, 0x94, 0x40500001);	/* VAWD1 */
	mt7530_mdio_w32(gsw, 0x90, 0x80001002);	/* VTCR, VID=2 */
	is_switch_vlan_table_busy(gsw);
#endif
}

void mii_mgr_read_combine(struct fe_priv *priv, u32 phy_addr, u32 phy_register,
			  u32 *read_data)
{
	struct mt7620_gsw *gsw = (struct mt7620_gsw *)priv->soc->swpriv;

	if (phy_addr == 31)
		*read_data = mt7530_mdio_r32(gsw, phy_register);

	else
		*read_data = _mt7620_mii_read(gsw, phy_addr, phy_register);
}

void mii_mgr_write_combine(struct fe_priv *priv, u32 phy_addr, u32 phy_register,
			   u32 write_data)
{
	struct mt7620_gsw *gsw = (struct mt7620_gsw *)priv->soc->swpriv;

	if (phy_addr == 31)
		mt7530_mdio_w32(gsw, phy_register, write_data);

	else
		_mt7620_mii_write(gsw, phy_addr, phy_register, write_data);
}

u32 mii_mgr_cl45_set_address(struct mt7620_gsw *gsw, u32 port_num, u32 dev_addr,
			     u32 reg_addr)
{
	u32 rc = 0;

	unsigned long t_start = jiffies;
	u32 data = 0;

	while (1) {
		if (!(gsw_r32(gsw, MT7620A_GSW_REG_PIAC) & GSW_MDIO_ACCESS)) {
			break;
		} else if (time_after(jiffies, t_start + GSW_REG_PHY_TIMEOUT)) {
			pr_err("\n MDIO Read operation is ongoing !!\n");
			return rc;
		}
	}
	data =
	    (dev_addr << 25) | (port_num << 20) | (0x00 << 18) | (0x00 << 16) |
	    reg_addr;
	gsw_w32(gsw, data, MT7620A_GSW_REG_PIAC);
	data |= GSW_MDIO_ACCESS;
	gsw_w32(gsw, data, MT7620A_GSW_REG_PIAC);

	t_start = jiffies;
	while (1) {
		/* 0 : Read/write operation complete */
		if (!(gsw_r32(gsw, MT7620A_GSW_REG_PIAC) & GSW_MDIO_ACCESS)) {
			return 1;
		} else if (time_after(jiffies, t_start + GSW_REG_PHY_TIMEOUT)) {
			pr_err("\n MDIO Write operation Time Out\n");
			return 0;
		}
	}
}

void mii_mgr_read_cl45(struct fe_priv *priv, u32 port_num, u32 dev_addr,
		       u32 reg_addr, u32 *read_data)
{
	u32 status = 0;
	u32 rc = 0;
	unsigned long t_start = jiffies;
	u32 data = 0;
	struct mt7620_gsw *gsw = (struct mt7620_gsw *)priv->soc->swpriv;

	/* set address first */
	mii_mgr_cl45_set_address(gsw, port_num, dev_addr, reg_addr);

	while (1) {
		if (!(gsw_r32(gsw, MT7620A_GSW_REG_PIAC) & GSW_MDIO_ACCESS)) {
			break;
		} else if (time_after(jiffies, t_start + GSW_REG_PHY_TIMEOUT)) {
			pr_err("\n MDIO Read operation is ongoing !!\n");
			return rc;
		}
	}

	data =
	    (dev_addr << 25) | (port_num << 20) | (0x03 << 18) | (0x00 << 16) |
	    reg_addr;
	gsw_w32(gsw, data, MT7620A_GSW_REG_PIAC);
	data |= GSW_MDIO_ACCESS;
	gsw_w32(gsw, data, MT7620A_GSW_REG_PIAC);
	t_start = jiffies;

	while (1) {
		if (!(gsw_r32(gsw, MT7620A_GSW_REG_PIAC) & GSW_MDIO_ACCESS)) {
			*read_data =
			    (gsw_r32(gsw, MT7620A_GSW_REG_PIAC) & 0x0000FFFF);
			return 1;
		} else if (time_after(jiffies, t_start + GSW_REG_PHY_TIMEOUT)) {
			pr_err("\n MDIO Read operation is Time Out!!\n");
			return 0;
		}
		status = gsw_r32(gsw, MT7620A_GSW_REG_PIAC);
	}
}

u32 mii_mgr_write_cl45(struct fe_priv *priv, u32 port_num, u32 dev_addr,
		       u32 reg_addr, u32 write_data)
{
	u32 rc = 0;
	unsigned long t_start = jiffies;
	u32 data = 0;
	struct mt7620_gsw *gsw = (struct mt7620_gsw *)priv->soc->swpriv;

	/* set address first */
	mii_mgr_cl45_set_address(gsw, port_num, dev_addr, reg_addr);

	while (1) {
		if (!(gsw_r32(gsw, MT7620A_GSW_REG_PIAC) & GSW_MDIO_ACCESS)) {
			break;
		} else if (time_after(jiffies, t_start + GSW_REG_PHY_TIMEOUT)) {
			pr_err("\n MDIO Read operation is ongoing !!\n");
			return rc;
		}
	}

	data =
	    (dev_addr << 25) | (port_num << 20) | (0x01 << 18) | (0x00 << 16) |
	    write_data;
	gsw_w32(gsw, data, MT7620A_GSW_REG_PIAC);
	data |= GSW_MDIO_ACCESS;
	gsw_w32(gsw, data, MT7620A_GSW_REG_PIAC);
	t_start = jiffies;

	while (1) {
		if (!(gsw_r32(gsw, MT7620A_GSW_REG_PIAC) & GSW_MDIO_ACCESS)) {
			return 1;
		} else if (time_after(jiffies, t_start + GSW_REG_PHY_TIMEOUT)) {
			pr_err("\n MDIO Write operation Time Out\n");
			return 0;
		}
	}
}

u32 __gsw_r32(struct fe_priv *priv, unsigned reg)
{
	struct mt7620_gsw *gsw = (struct mt7620_gsw *)priv->soc->swpriv;

	return gsw_r32(gsw, reg);
}

void __gsw_w32(struct fe_priv *priv, u32 val, unsigned reg)
{
	struct mt7620_gsw *gsw = (struct mt7620_gsw *)priv->soc->swpriv;

	gsw_w32(gsw, val, reg);
}

int esw_cnt_read(struct seq_file *seq, void *v)
{
	unsigned int pkt_cnt = 0;
	int i = 0;

	seq_puts(seq, "\n		  <<CPU>>\n");
	seq_puts(seq, "		    |\n");
	seq_puts(seq, "+-----------------------------------------------+\n");
	seq_puts(seq, "|		  <<PSE>>		        |\n");
	seq_puts(seq, "+-----------------------------------------------+\n");
	seq_puts(seq, "		   |\n");
	seq_puts(seq, "+-----------------------------------------------+\n");
	seq_puts(seq, "|		  <<GDMA>>		        |\n");
	seq_printf(seq, "| GDMA1_RX_GBCNT  : %010u (Rx Good Bytes)	|\n",
		   fe_r32(0x2400));
	seq_printf(seq, "| GDMA1_RX_GPCNT  : %010u (Rx Good Pkts)	|\n",
		   fe_r32(0x2408));
	seq_printf(seq, "| GDMA1_RX_OERCNT : %010u (overflow error)	|\n",
		   fe_r32(0x2410));
	seq_printf(seq, "| GDMA1_RX_FERCNT : %010u (FCS error)	|\n",
		   fe_r32(0x2414));
	seq_printf(seq, "| GDMA1_RX_SERCNT : %010u (too short)	|\n",
		   fe_r32(0x2418));
	seq_printf(seq, "| GDMA1_RX_LERCNT : %010u (too long)	|\n",
		   fe_r32(0x241C));
	seq_printf(seq, "| GDMA1_RX_CERCNT : %010u (checksum error)	|\n",
		   fe_r32(0x2420));
	seq_printf(seq, "| GDMA1_RX_FCCNT  : %010u (flow control)	|\n",
		   fe_r32(0x2424));
	seq_printf(seq, "| GDMA1_TX_SKIPCNT: %010u (about count)	|\n",
		   fe_r32(0x2428));
	seq_printf(seq, "| GDMA1_TX_COLCNT : %010u (collision count)	|\n",
		   fe_r32(0x242C));
	seq_printf(seq, "| GDMA1_TX_GBCNT  : %010u (Tx Good Bytes)	|\n",
		   fe_r32(0x2430));
	seq_printf(seq, "| GDMA1_TX_GPCNT  : %010u (Tx Good Pkts)	|\n",
		   fe_r32(0x2438));
	seq_puts(seq, "|						|\n");
	seq_printf(seq, "| GDMA2_RX_GBCNT  : %010u (Rx Good Bytes)	|\n",
		   fe_r32(0x2440));
	seq_printf(seq, "| GDMA2_RX_GPCNT  : %010u (Rx Good Pkts)	|\n",
		   fe_r32(0x2448));
	seq_printf(seq, "| GDMA2_RX_OERCNT : %010u (overflow error)	|\n",
		   fe_r32(0x2450));
	seq_printf(seq, "| GDMA2_RX_FERCNT : %010u (FCS error)	|\n",
		   fe_r32(0x2454));
	seq_printf(seq, "| GDMA2_RX_SERCNT : %010u (too short)	|\n",
		   fe_r32(0x2458));
	seq_printf(seq, "| GDMA2_RX_LERCNT : %010u (too long)	|\n",
		   fe_r32(0x245C));
	seq_printf(seq, "| GDMA2_RX_CERCNT : %010u (checksum error)	|\n",
		   fe_r32(0x2460));
	seq_printf(seq, "| GDMA2_RX_FCCNT  : %010u (flow control)	|\n",
		   fe_r32(0x2464));
	seq_printf(seq, "| GDMA2_TX_SKIPCNT: %010u (skip)		|\n",
		   fe_r32(0x2468));
	seq_printf(seq, "| GDMA2_TX_COLCNT : %010u (collision)	|\n",
		   fe_r32(0x246C));
	seq_printf(seq, "| GDMA2_TX_GBCNT  : %010u (Tx Good Bytes)	|\n",
		   fe_r32(0x2470));
	seq_printf(seq, "| GDMA2_TX_GPCNT  : %010u (Tx Good Pkts)	|\n",
		   fe_r32(0x2478));
	seq_puts(seq, "+-----------------------------------------------+\n");

#define DUMP_EACH_PORT(base)					\
	do { \
		for (i = 0; i < 7; i++) {				\
			pkt_cnt = mt7530_mdio_r32(__gsw, (base) + (i*0x100));\
			seq_printf(seq, "%8u ", pkt_cnt);		\
		}							\
		seq_puts(seq, "\n"); \
	} while (0)

	seq_printf(seq, "===================== %8s %8s %8s %8s %8s %8s %8s\n",
		   "Port0", "Port1", "Port2", "Port3", "Port4", "Port5",
		   "Port6");
	seq_puts(seq, "Tx Drop Packet      :");
	DUMP_EACH_PORT(0x4000);
	seq_puts(seq, "Tx CRC Error        :");
	DUMP_EACH_PORT(0x4004);
	seq_puts(seq, "Tx Unicast Packet   :");
	DUMP_EACH_PORT(0x4008);
	seq_puts(seq, "Tx Multicast Packet :");
	DUMP_EACH_PORT(0x400C);
	seq_puts(seq, "Tx Broadcast Packet :");
	DUMP_EACH_PORT(0x4010);
	seq_puts(seq, "Tx Collision Event  :");
	DUMP_EACH_PORT(0x4014);
	seq_puts(seq, "Tx Pause Packet     :");
	DUMP_EACH_PORT(0x402C);
	seq_puts(seq, "Rx Drop Packet      :");
	DUMP_EACH_PORT(0x4060);
	seq_puts(seq, "Rx Filtering Packet :");
	DUMP_EACH_PORT(0x4064);
	seq_puts(seq, "Rx Unicast Packet   :");
	DUMP_EACH_PORT(0x4068);
	seq_puts(seq, "Rx Multicast Packet :");
	DUMP_EACH_PORT(0x406C);
	seq_puts(seq, "Rx Broadcast Packet :");
	DUMP_EACH_PORT(0x4070);
	seq_puts(seq, "Rx Alignment Error  :");
	DUMP_EACH_PORT(0x4074);
	seq_puts(seq, "Rx CRC Error	    :");
	DUMP_EACH_PORT(0x4078);
	seq_puts(seq, "Rx Undersize Error  :");
	DUMP_EACH_PORT(0x407C);
	seq_puts(seq, "Rx Fragment Error   :");
	DUMP_EACH_PORT(0x4080);
	seq_puts(seq, "Rx Oversize Error   :");
	DUMP_EACH_PORT(0x4084);
	seq_puts(seq, "Rx Jabber Error     :");
	DUMP_EACH_PORT(0x4088);
	seq_puts(seq, "Rx Pause Packet     :");
	DUMP_EACH_PORT(0x408C);
	mt7530_mdio_w32(__gsw, 0x4fe0, 0xf0);
	mt7530_mdio_w32(__gsw, 0x4fe0, 0x800000f0);

	seq_puts(seq, "\n");

	return 0;
}

static int switch_count_open(struct inode *inode, struct file *file)
{
	return single_open(file, esw_cnt_read, NULL);
}

static const struct file_operations switch_count_fops = {
	.owner = THIS_MODULE,
	.open = switch_count_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static struct proc_dir_entry *proc_tx_ring, *proc_rx_ring;

int tx_ring_read(struct seq_file *seq, void *v)
{
	struct net_device *netdev = __priv->netdev;
	struct fe_tx_ring *ring = &__priv->tx_ring;
	struct fe_tx_dma *tx_ring;
	int i = 0;

	tx_ring =
	    kmalloc(sizeof(struct fe_tx_dma) * ring->tx_ring_size, GFP_KERNEL);
	if (tx_ring == NULL) {
		seq_puts(seq, " allocate temp tx_ring fail.\n");
		return 0;
	}

	for (i = 0; i < ring->tx_ring_size; i++)
		tx_ring[i] = ring->tx_dma[i];

	for (i = 0; i < ring->tx_ring_size; i++) {
		seq_printf(seq, "%d: %08x %08x %08x %08x\n", i,
			   *(int *)&tx_ring[i].txd1, *(int *)&tx_ring[i].txd2,
			   *(int *)&tx_ring[i].txd3, *(int *)&tx_ring[i].txd4);
	}

	kfree(tx_ring);
	return 0;
}

static int tx_ring_open(struct inode *inode, struct file *file)
{
	return single_open(file, tx_ring_read, NULL);
}

static const struct file_operations tx_ring_fops = {
	.owner = THIS_MODULE,
	.open = tx_ring_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

int rx_ring_read(struct seq_file *seq, void *v)
{
	struct net_device *netdev = __priv->netdev;
	struct fe_rx_ring *ring = &__priv->rx_ring;
	struct fe_rx_dma *rx_ring;

	int i = 0;

	rx_ring =
	    kmalloc(sizeof(struct fe_rx_dma) * ring->rx_ring_size, GFP_KERNEL);
	if (rx_ring == NULL) {
		seq_puts(seq, " allocate temp rx_ring fail.\n");
		return 0;
	}

	for (i = 0; i < ring->rx_ring_size; i++)
		rx_ring[i] = ring->rx_dma[i];

	for (i = 0; i < ring->rx_ring_size; i++) {
		seq_printf(seq, "%d: %08x %08x %08x %08x\n", i,
			   *(int *)&rx_ring[i].rxd1, *(int *)&rx_ring[i].rxd2,
			   *(int *)&rx_ring[i].rxd3, *(int *)&rx_ring[i].rxd4);
	}

	kfree(rx_ring);
	return 0;
}

static int rx_ring_open(struct inode *inode, struct file *file)
{
	return single_open(file, rx_ring_read, NULL);
}

static const struct file_operations rx_ring_fops = {
	.owner = THIS_MODULE,
	.open = rx_ring_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

#define PROCREG_ESW_CNT         "esw_cnt"
#define PROCREG_TXRING          "tx_ring"
#define PROCREG_RXRING          "rx_ring"
#define PROCREG_DIR             "mt7623"

struct proc_dir_entry *proc_reg_dir;
static struct proc_dir_entry *proc_esw_cnt;

int debug_proc_init(struct fe_priv *priv)
{
	__priv = priv;
	/* pr_err("debug_proc_init addr = %x\n", priv); */
	if (proc_reg_dir == NULL)
		proc_reg_dir = proc_mkdir(PROCREG_DIR, NULL);

	proc_tx_ring =
	    proc_create(PROCREG_TXRING, 0, proc_reg_dir, &tx_ring_fops);
	if (!proc_tx_ring)
		pr_err("!! FAIL to create %s PROC !!\n", PROCREG_TXRING);

	proc_rx_ring =
	    proc_create(PROCREG_RXRING, 0, proc_reg_dir, &rx_ring_fops);
	if (!proc_rx_ring)
		pr_err("!! FAIL to create %s PROC !!\n", PROCREG_RXRING);

	proc_esw_cnt =
	    proc_create(PROCREG_ESW_CNT, 0, proc_reg_dir, &switch_count_fops);
	if (!proc_esw_cnt)
		pr_err("!! FAIL to create %s PROC !!\n", PROCREG_ESW_CNT);

	pr_err("PROC INIT OK!\n");
	return 0;
}

void debug_proc_exit(void)
{
	if (proc_tx_ring)
		remove_proc_entry(PROCREG_TXRING, proc_reg_dir);

	if (proc_rx_ring)
		remove_proc_entry(PROCREG_RXRING, proc_reg_dir);

	if (proc_esw_cnt)
		remove_proc_entry(PROCREG_ESW_CNT, proc_reg_dir);

	if (proc_reg_dir)
		remove_proc_entry(PROCREG_DIR, 0);

	pr_err("proc exit\n");
}
