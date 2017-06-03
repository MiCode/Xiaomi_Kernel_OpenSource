/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* MSM EMAC PHY Controller driver.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_mdio.h>
#include <linux/phy.h>
#include <linux/iopoll.h>
#include <linux/acpi.h>
#include <linux/phy.h>
#include <linux/pm_runtime.h>
#include "emac_hw.h"
#include "emac_defines.h"
#include "emac_regs.h"
#include "emac_phy.h"
#include "emac_rgmii.h"
#include "emac_sgmii.h"

/**
 * emac_phy_mdio_autopoll_disable() - disable mdio autopoll
 * @hw: the emac hardware
 *
 * The autopoll feature takes over the MDIO bus.  In order for
 * the PHY driver to be able to talk to the PHY over the MDIO
 * bus, we need to temporarily disable the autopoll feature.
 */
static int emac_phy_mdio_autopoll_disable(struct emac_hw *hw)
{
	u32 val;

	emac_reg_update32(hw, EMAC, EMAC_MDIO_CTRL, MDIO_AP_EN, 0);
	wmb(); /* ensure mdio autopoll disable is requested */

	/* wait for any mdio polling to complete */
	if (!readl_poll_timeout(hw->reg_addr[EMAC] + EMAC_MDIO_CTRL, val,
				!(val & MDIO_BUSY), 100, MDIO_WAIT_TIMES * 100))
		return 0;

	/* failed to disable; ensure it is enabled before returning */
	emac_reg_update32(hw, EMAC, EMAC_MDIO_CTRL, 0, MDIO_AP_EN);
	wmb(); /* ensure mdio autopoll is enabled */
	return -EBUSY;
}

/**
 * emac_phy_mdio_autopoll_enable() - disable mdio autopoll
 * @adpt: the emac adapter
 *
 * The EMAC has the ability to poll the external PHY on the MDIO
 * bus for link state changes.  This eliminates the need for the
 * driver to poll the phy.  If if the link state does change,
 * the EMAC issues an interrupt on behalf of the PHY.
 */
static void emac_phy_mdio_autopoll_enable(struct emac_hw *hw)
{
	emac_reg_update32(hw, EMAC, EMAC_MDIO_CTRL, 0, MDIO_AP_EN);
	wmb(); /* ensure mdio autopoll is enabled */
}

static int emac_mdio_read(struct mii_bus *bus, int addr, int regnum)
{
	struct emac_adapter *adpt = bus->priv;
	struct emac_hw  *hw  = &adpt->hw;
	struct emac_phy *phy = &adpt->phy;
	u32 reg = 0;
	int ret = 0;

	if (pm_runtime_enabled(adpt->netdev->dev.parent) &&
	    pm_runtime_status_suspended(adpt->netdev->dev.parent)) {
		emac_dbg(adpt, hw, "EMAC in suspended state\n");
		return ret;
	}

	if (phy->external) {
		ret = emac_phy_mdio_autopoll_disable(hw);
		if (ret) {
			emac_err(adpt, "error in %s (phy addr %d, err %d)\n",
				 __func__, addr, ret);
			return ret;
		}
	}

	emac_reg_update32(hw, EMAC, EMAC_PHY_STS, PHY_ADDR_BMSK,
			  (addr << PHY_ADDR_SHFT));
	wmb(); /* ensure PHY address is set before we proceed */

	reg = reg & ~(MDIO_REG_ADDR_BMSK | MDIO_CLK_SEL_BMSK |
			MDIO_MODE | MDIO_PR);
	reg = SUP_PREAMBLE |
	      ((MDIO_CLK_25_4 << MDIO_CLK_SEL_SHFT) & MDIO_CLK_SEL_BMSK) |
	      ((regnum << MDIO_REG_ADDR_SHFT) & MDIO_REG_ADDR_BMSK) |
	      MDIO_START | MDIO_RD_NWR;

	emac_reg_w32(hw, EMAC, EMAC_MDIO_CTRL, reg);
	mb(); /* ensure hw starts the operation before we check for result */

	if (readl_poll_timeout(hw->reg_addr[EMAC] + EMAC_MDIO_CTRL, reg,
			       !(reg & (MDIO_START | MDIO_BUSY)),
			       100, MDIO_WAIT_TIMES * 100)) {
		emac_err(adpt, "error reading phy addr %d phy reg 0x%02x\n",
			 addr, regnum);
		ret = -EIO;
	} else {
		ret = (reg >> MDIO_DATA_SHFT) & MDIO_DATA_BMSK;

		emac_dbg(adpt, hw, "EMAC PHY ADDR %d PHY RD 0x%02x -> 0x%04x\n",
			 addr, regnum, ret);
	}

	if (phy->external)
		emac_phy_mdio_autopoll_enable(hw);

	return ret;
}

static int emac_mdio_write(struct mii_bus *bus, int addr, int regnum, u16 val)
{
	struct emac_adapter *adpt = bus->priv;
	struct emac_hw  *hw  = &adpt->hw;
	struct emac_phy *phy = &adpt->phy;
	u32 reg = 0;
	int ret = 0;

	if (pm_runtime_enabled(adpt->netdev->dev.parent) &&
	    pm_runtime_status_suspended(adpt->netdev->dev.parent)) {
		emac_dbg(adpt, hw, "EMAC in suspended state\n");
		return ret;
	}

	if (phy->external) {
		ret = emac_phy_mdio_autopoll_disable(hw);
		if (ret) {
			emac_err(adpt, "error in %s (phy addr %d, err %d)\n",
				 __func__, addr, ret);
			return ret;
		}
	}

	emac_reg_update32(hw, EMAC, EMAC_PHY_STS, PHY_ADDR_BMSK,
			  (addr << PHY_ADDR_SHFT));
	wmb(); /* ensure PHY address is set before we proceed */

	reg = reg & ~(MDIO_REG_ADDR_BMSK | MDIO_CLK_SEL_BMSK |
		MDIO_DATA_BMSK | MDIO_MODE | MDIO_PR);
	reg = SUP_PREAMBLE |
	((MDIO_CLK_25_4 << MDIO_CLK_SEL_SHFT) & MDIO_CLK_SEL_BMSK) |
	((regnum << MDIO_REG_ADDR_SHFT) & MDIO_REG_ADDR_BMSK) |
	((val << MDIO_DATA_SHFT) & MDIO_DATA_BMSK) |
	MDIO_START;

	emac_reg_w32(hw, EMAC, EMAC_MDIO_CTRL, reg);
	mb(); /* ensure hw starts the operation before we check for result */

	if (readl_poll_timeout(hw->reg_addr[EMAC] + EMAC_MDIO_CTRL, reg,
			       !(reg & (MDIO_START | MDIO_BUSY)), 100,
			       MDIO_WAIT_TIMES * 100)) {
		emac_err(adpt, "error writing phy addr %d phy reg 0x%02x data 0x%02x\n",
			 addr, regnum, val);
		ret = -EIO;
	} else
		emac_dbg(adpt, hw, "EMAC PHY Addr %d PHY WR 0x%02x <- 0x%04x\n",
			 addr, regnum, val);

	if (phy->external)
		emac_phy_mdio_autopoll_enable(hw);

	return ret;
}

int emac_phy_config_fc(struct emac_adapter *adpt)
{
	struct emac_phy *phy = &adpt->phy;
	struct emac_hw  *hw  = &adpt->hw;
	u32 mac;

	if (phy->disable_fc_autoneg || !phy->external)
		phy->cur_fc_mode = phy->req_fc_mode;

	mac = emac_reg_r32(hw, EMAC, EMAC_MAC_CTRL);

	switch (phy->cur_fc_mode) {
	case EMAC_FC_NONE:
		mac &= ~(RXFC | TXFC);
		break;
	case EMAC_FC_RX_PAUSE:
		mac &= ~TXFC;
		mac |= RXFC;
		break;
	case EMAC_FC_TX_PAUSE:
		mac |= TXFC;
		mac &= ~RXFC;
		break;
	case EMAC_FC_FULL:
	case EMAC_FC_DEFAULT:
		mac |= (TXFC | RXFC);
		break;
	default:
		emac_err(adpt, "flow control param set incorrectly\n");
		return -EINVAL;
	}

	emac_reg_w32(hw, EMAC, EMAC_MAC_CTRL, mac);
	/* ensure flow control config is slushed to hw */
	wmb();
	return 0;
}

/* Configure the MDIO bus and connect the external PHY */
int emac_phy_config_external(struct platform_device *pdev,
			     struct emac_adapter *adpt)
{
	struct device_node *np = pdev->dev.of_node;
	struct mii_bus *mii_bus;
	int ret;

	/* Create the mii_bus object for talking to the MDIO bus */
	mii_bus = devm_mdiobus_alloc(&pdev->dev);
	adpt->mii_bus = mii_bus;

	if (!mii_bus)
		return -ENOMEM;

	mii_bus->name = "emac-mdio";
	snprintf(mii_bus->id, MII_BUS_ID_SIZE, "%s", pdev->name);
	mii_bus->read = emac_mdio_read;
	mii_bus->write = emac_mdio_write;
	mii_bus->parent = &pdev->dev;
	mii_bus->priv = adpt;

	if (ACPI_COMPANION(&pdev->dev)) {
		u32 phy_addr;

		ret = mdiobus_register(mii_bus);
		if (ret) {
			emac_err(adpt, "could not register mdio bus\n");
			return ret;
		}
		ret = device_property_read_u32(&pdev->dev, "phy-channel",
					       &phy_addr);
		if (ret)
			/* If we can't read a valid phy address, then assume
			 * that there is only one phy on this mdio bus.
			 */
			adpt->phydev = phy_find_first(mii_bus);
		else
			adpt->phydev = mii_bus->phy_map[phy_addr];
	} else {
		struct device_node *phy_np;

		ret = of_mdiobus_register(mii_bus, np);
		if (ret) {
			emac_err(adpt, "could not register mdio bus\n");
			return ret;
		}

		phy_np = of_parse_phandle(np, "phy-handle", 0);
		adpt->phydev = of_phy_find_device(phy_np);
		of_node_put(phy_np);
	}

	if (!adpt->phydev) {
		emac_err(adpt, "could not find external phy\n");
		mdiobus_unregister(mii_bus);
		return -ENODEV;
	}

	if (!adpt->phydev->phy_id) {
		emac_err(adpt, "External phy is not up\n");
		mdiobus_unregister(mii_bus);
		return -EPROBE_DEFER;
	}

	if (adpt->phydev->drv) {
		emac_dbg(adpt, probe, "attached PHY driver [%s] ",
			 adpt->phydev->drv->name);
		emac_dbg(adpt, probe, "(mii_bus:phy_addr=%s, irq=%d)\n",
			 dev_name(&adpt->phydev->dev), adpt->phydev->irq);
	}

	/* Set initial link status to false */
	adpt->phydev->link = 0;
	return 0;
}

int emac_phy_config_internal(struct platform_device *pdev,
			     struct emac_adapter *adpt)
{
	struct emac_phy *phy = &adpt->phy;
	struct device_node *dt = pdev->dev.of_node;
	int ret;

	phy->external = !of_property_read_bool(dt, "qcom,no-external-phy");

	/* Get the link mode */
	ret = of_get_phy_mode(dt);
	if (ret < 0) {
		emac_err(adpt, "unknown phy mode: %s\n", phy_modes(ret));
		return ret;
	}

	phy->phy_interface = ret;

	switch (phy->phy_interface) {
	case PHY_INTERFACE_MODE_RGMII:
		phy->ops = emac_rgmii_ops;
		break;
	case PHY_INTERFACE_MODE_SGMII:
		phy->ops = emac_sgmii_ops;
		break;
	default:
		emac_err(adpt, "unsupported phy mode: %s\n", phy_modes(ret));
		return -EINVAL;
	}

	ret = phy->ops.config(pdev, adpt);
	if (ret)
		return ret;

	return 0;
}
