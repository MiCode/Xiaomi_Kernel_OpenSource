/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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
#include <linux/pm_runtime.h>
#include <linux/phy.h>

#include "emac.h"
#include "emac_hw.h"
#include "emac_defines.h"
#include "emac_regs.h"
#include "emac_phy.h"
#include "emac_rgmii.h"
#include "emac_sgmii_v1.h"

static int emac_disable_mdio_autopoll(struct emac_hw *hw)
{
	u32 i, val;

	emac_reg_update32(hw, EMAC, EMAC_MDIO_CTRL, MDIO_AP_EN, 0);
	wmb(); /* ensure mdio autopoll disable is requested */

	/* wait for any mdio polling to complete */
	for (i = 0; i < MDIO_WAIT_TIMES; i++) {
		val = emac_reg_r32(hw, EMAC, EMAC_MDIO_CTRL);
		if (!(val & MDIO_BUSY))
			return 0;

		udelay(10); /* atomic context */
	}

	/* failed to disable; ensure it is enabled before returning */
	emac_reg_update32(hw, EMAC, EMAC_MDIO_CTRL, 0, MDIO_AP_EN);
	wmb(); /* ensure mdio autopoll is enabled */
	return -EBUSY;
}

static void emac_enable_mdio_autopoll(struct emac_hw *hw)
{
	emac_reg_update32(hw, EMAC, EMAC_MDIO_CTRL, 0, MDIO_AP_EN);
	wmb(); /* ensure mdio autopoll is enabled */
}

int emac_hw_read_phy_reg(struct emac_adapter *adpt, bool ext, u8 dev, bool fast,
			 u16 reg_addr, u16 *phy_data)
{
	struct emac_phy *phy = &adpt->phy;
	struct emac_hw  *hw  = &adpt->hw;
	u32 i, clk_sel, val = 0;
	int retval = 0;

	*phy_data = 0;
	clk_sel = fast ? MDIO_CLK_25_4 : MDIO_CLK_25_28;

	if (phy->external) {
		retval = emac_disable_mdio_autopoll(hw);
		if (retval)
			return retval;
	}

	emac_reg_update32(hw, EMAC, EMAC_PHY_STS, PHY_ADDR_BMSK,
			  (dev << PHY_ADDR_SHFT));
	wmb(); /* ensure PHY address is set before we proceed */

	if (ext) {
		val = ((dev << DEVAD_SHFT) & DEVAD_BMSK) |
		      ((reg_addr << EX_REG_ADDR_SHFT) & EX_REG_ADDR_BMSK);
		emac_reg_w32(hw, EMAC, EMAC_MDIO_EX_CTRL, val);
		wmb(); /* ensure proper address is set before proceeding */

		val = SUP_PREAMBLE |
		      ((clk_sel << MDIO_CLK_SEL_SHFT) & MDIO_CLK_SEL_BMSK) |
		      MDIO_START | MDIO_MODE | MDIO_RD_NWR;
	} else {
		val = val & ~(MDIO_REG_ADDR_BMSK | MDIO_CLK_SEL_BMSK |
				MDIO_MODE | MDIO_PR);
		val = SUP_PREAMBLE |
		      ((clk_sel << MDIO_CLK_SEL_SHFT) & MDIO_CLK_SEL_BMSK) |
		      ((reg_addr << MDIO_REG_ADDR_SHFT) & MDIO_REG_ADDR_BMSK) |
		      MDIO_START | MDIO_RD_NWR;
	}

	emac_reg_w32(hw, EMAC, EMAC_MDIO_CTRL, val);
	mb(); /* ensure hw starts the operation before we check for result */

	for (i = 0; i < MDIO_WAIT_TIMES; i++) {
		val = emac_reg_r32(hw, EMAC, EMAC_MDIO_CTRL);
		if (!(val & (MDIO_START | MDIO_BUSY))) {
			*phy_data = (u16)((val >> MDIO_DATA_SHFT) &
					MDIO_DATA_BMSK);
			break;
		}
		udelay(10); /* atomic context */
	}

	if (i == MDIO_WAIT_TIMES)
		retval = -EIO;

	if (phy->external)
		emac_enable_mdio_autopoll(hw);

	return retval;
}

int emac_hw_write_phy_reg(struct emac_adapter *adpt, bool ext, u8 dev,
			  bool fast, u16 reg_addr, u16 phy_data)
{
	struct emac_phy *phy = &adpt->phy;
	struct emac_hw  *hw  = &adpt->hw;
	u32 i, clk_sel, val = 0;
	int retval = 0;

	clk_sel = fast ? MDIO_CLK_25_4 : MDIO_CLK_25_28;

	if (phy->external) {
		retval = emac_disable_mdio_autopoll(hw);
		if (retval)
			return retval;
	}

	emac_reg_update32(hw, EMAC, EMAC_PHY_STS, PHY_ADDR_BMSK,
			  (dev << PHY_ADDR_SHFT));
	wmb(); /* ensure PHY address is set before we proceed */

	if (ext) {
		val = ((dev << DEVAD_SHFT) & DEVAD_BMSK) |
		      ((reg_addr << EX_REG_ADDR_SHFT) & EX_REG_ADDR_BMSK);
		emac_reg_w32(hw, EMAC, EMAC_MDIO_EX_CTRL, val);
		wmb(); /* ensure proper address is set before proceeding */

		val = SUP_PREAMBLE |
			((clk_sel << MDIO_CLK_SEL_SHFT) & MDIO_CLK_SEL_BMSK) |
			((phy_data << MDIO_DATA_SHFT) & MDIO_DATA_BMSK) |
			MDIO_START | MDIO_MODE;
	} else {
		val = val & ~(MDIO_REG_ADDR_BMSK | MDIO_CLK_SEL_BMSK |
			MDIO_DATA_BMSK | MDIO_MODE | MDIO_PR);
		val = SUP_PREAMBLE |
		((clk_sel << MDIO_CLK_SEL_SHFT) & MDIO_CLK_SEL_BMSK) |
		((reg_addr << MDIO_REG_ADDR_SHFT) & MDIO_REG_ADDR_BMSK) |
		((phy_data << MDIO_DATA_SHFT) & MDIO_DATA_BMSK) |
		MDIO_START;
	}

	emac_reg_w32(hw, EMAC, EMAC_MDIO_CTRL, val);
	mb(); /* ensure hw starts the operation before we check for result */

	for (i = 0; i < MDIO_WAIT_TIMES; i++) {
		val = emac_reg_r32(hw, EMAC, EMAC_MDIO_CTRL);
		if (!(val & (MDIO_START | MDIO_BUSY)))
			break;
		udelay(10); /* atomic context */
	}

	if (i == MDIO_WAIT_TIMES)
		retval = -EIO;

	if (phy->external)
		emac_enable_mdio_autopoll(hw);

	return retval;
}

int emac_phy_read(struct emac_adapter *adpt, u16 phy_addr, u16 reg_addr,
		  u16 *phy_data)
{
	struct emac_phy *phy = &adpt->phy;
	unsigned long  flags;
	int  retval;

	spin_lock_irqsave(&phy->lock, flags);
	retval = emac_hw_read_phy_reg(adpt, false, phy_addr, true, reg_addr,
				      phy_data);
	spin_unlock_irqrestore(&phy->lock, flags);

	if (retval)
		emac_err(adpt, "error reading phy reg 0x%02x\n", reg_addr);
	else
		emac_dbg(adpt, hw, "EMAC PHY RD: 0x%02x -> 0x%04x\n", reg_addr,
			 *phy_data);

	return retval;
}

int emac_phy_write(struct emac_adapter *adpt, u16 phy_addr, u16 reg_addr,
		   u16 phy_data)
{
	struct emac_phy *phy = &adpt->phy;
	unsigned long  flags;
	int  retval;

	spin_lock_irqsave(&phy->lock, flags);
	retval = emac_hw_write_phy_reg(adpt, false, phy_addr, true, reg_addr,
				       phy_data);
	spin_unlock_irqrestore(&phy->lock, flags);

	if (retval)
		emac_err(adpt, "error writing phy reg 0x%02x\n", reg_addr);
	else
		emac_dbg(adpt, hw, "EMAC PHY WR: 0x%02x <- 0x%04x\n", reg_addr,
			 phy_data);

	return retval;
}

/* reset external phy */
void emac_phy_reset_external(struct emac_adapter *adpt)
{
	/* Trigger ephy reset by pulling line low */
	adpt->gpio_off(adpt, false, true);
	/* need delay to complete ephy reset */
	usleep_range(10000, 20000);
	/* Complete ephy reset by pulling line back up */
	adpt->gpio_on(adpt, false, true);
	/* need delay to complete ephy reset */
	usleep_range(10000, 20000);
}

/* initialize external phy */
int emac_phy_init_external(struct emac_adapter *adpt)
{
	struct emac_phy *phy = &adpt->phy;
	struct emac_hw  *hw  = &adpt->hw;
	u16 phy_id[2];
	int retval = 0;

	if (phy->external) {
		emac_phy_reset_external(adpt);

		retval = emac_phy_read(adpt, phy->addr, MII_PHYSID1,
				       &phy_id[0]);
		if (retval)
			return retval;
		retval = emac_phy_read(adpt, phy->addr, MII_PHYSID2,
				       &phy_id[1]);
		if (retval)
			return retval;

		phy->id[0] = phy_id[0];
		phy->id[1] = phy_id[1];
	} else {
		emac_disable_mdio_autopoll(hw);
	}

	return phy->ops.init_ephy(adpt);
}

static int emac_hw_setup_phy_link(struct emac_adapter *adpt,
				  enum emac_flow_ctrl req_fc_mode, u32 speed,
				  bool autoneg, bool fc)
{
	struct emac_phy *phy = &adpt->phy;
	u16 adv, bmcr, ctrl1000 = 0;
	int retval = 0;

	if (autoneg) {
		switch (req_fc_mode) {
		case EMAC_FC_FULL:
		case EMAC_FC_RX_PAUSE:
			adv = ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM;
			break;
		case EMAC_FC_TX_PAUSE:
			adv = ADVERTISE_PAUSE_ASYM;
			break;
		default:
			adv = 0;
			break;
		}
		if (!fc)
			adv &= ~(ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM);

		if (speed & EMAC_LINK_SPEED_10_HALF)
			adv |= ADVERTISE_10HALF;

		if (speed & EMAC_LINK_SPEED_10_FULL)
			adv |= ADVERTISE_10HALF | ADVERTISE_10FULL;

		if (speed & EMAC_LINK_SPEED_100_HALF)
			adv |= ADVERTISE_100HALF;

		if (speed & EMAC_LINK_SPEED_100_FULL)
			adv |= ADVERTISE_100HALF | ADVERTISE_100FULL;

		if (speed & EMAC_LINK_SPEED_1GB_FULL)
			ctrl1000 |= ADVERTISE_1000FULL;

		retval |= emac_phy_write(adpt, phy->addr, MII_ADVERTISE, adv);
		retval |= emac_phy_write(adpt, phy->addr, MII_CTRL1000,
					 ctrl1000);

		bmcr = BMCR_RESET | BMCR_ANENABLE | BMCR_ANRESTART;
		retval |= emac_phy_write(adpt, phy->addr, MII_BMCR, bmcr);
	} else {
		bmcr = BMCR_RESET;
		switch (speed) {
		case EMAC_LINK_SPEED_10_HALF:
			bmcr |= BMCR_SPEED10;
			break;
		case EMAC_LINK_SPEED_10_FULL:
			bmcr |= BMCR_SPEED10 | BMCR_FULLDPLX;
			break;
		case EMAC_LINK_SPEED_100_HALF:
			bmcr |= BMCR_SPEED100;
			break;
		case EMAC_LINK_SPEED_100_FULL:
			bmcr |= BMCR_SPEED100 | BMCR_FULLDPLX;
			break;
		case EMAC_LINK_SPEED_1GB_FULL:
			bmcr |= BMCR_SPEED1000 | BMCR_FULLDPLX;
			break;
		default:
			return -EINVAL;
		}

		retval |= emac_phy_write(adpt, phy->addr, MII_BMCR, bmcr);
	}

	return retval;
}

int emac_phy_setup_link(struct emac_adapter *adpt, u32 speed, bool autoneg,
			bool fc)
{
	struct emac_phy *phy = &adpt->phy;
	int retval = 0;

	if (!phy->external)
		return phy->ops.link_setup_no_ephy(adpt, speed, autoneg);

	if (emac_hw_setup_phy_link(adpt, phy->req_fc_mode, speed, autoneg,
				   fc)) {
		emac_err(adpt,
			 "error on setup_phy(speed:%d autoneg:%d fc:%d)\n",
			 speed, autoneg, fc);
		retval = -EINVAL;
	} else {
		phy->autoneg = autoneg;
	}

	return retval;
}

int emac_phy_setup_link_speed(struct emac_adapter *adpt, u32 speed,
			      bool autoneg, bool fc)
{
	struct emac_phy *phy = &adpt->phy;

	/* update speed based on input link speed */
	phy->autoneg_advertised = speed & EMAC_LINK_SPEED_DEFAULT;
	return emac_phy_setup_link(adpt, phy->autoneg_advertised, autoneg, fc);
}

int emac_phy_check_link(struct emac_adapter *adpt, u32 *speed, bool *link_up)
{
	struct emac_phy *phy = &adpt->phy;
	u16 bmsr, pssr;
	int retval;

	if (!phy->external)
		return phy->ops.link_check_no_ephy(adpt, speed, link_up);

	retval = emac_phy_read(adpt, phy->addr, MII_BMSR, &bmsr);
	if (retval)
		return retval;

	if (!(bmsr & BMSR_LSTATUS)) {
		*link_up = false;
		*speed = EMAC_LINK_SPEED_UNKNOWN;
		return 0;
	}
	*link_up = true;
	retval = emac_phy_read(adpt, phy->addr, MII_PSSR, &pssr);
	if (retval)
		return retval;

	if (!(pssr & PSSR_SPD_DPLX_RESOLVED)) {
		emac_err(adpt, "error for speed duplex resolved\n");
		return -EINVAL;
	}

	switch (pssr & PSSR_SPEED) {
	case PSSR_1000MBS:
		if (pssr & PSSR_DPLX)
			*speed = EMAC_LINK_SPEED_1GB_FULL;
		else
			emac_err(adpt, "1000M half duplex is invalid");
		break;
	case PSSR_100MBS:
		if (pssr & PSSR_DPLX)
			*speed = EMAC_LINK_SPEED_100_FULL;
		else
			*speed = EMAC_LINK_SPEED_100_HALF;
		break;
	case PSSR_10MBS:
		if (pssr & PSSR_DPLX)
			*speed = EMAC_LINK_SPEED_10_FULL;
		else
			*speed = EMAC_LINK_SPEED_10_HALF;
		break;
	default:
		*speed = EMAC_LINK_SPEED_UNKNOWN;
		retval = -EINVAL;
		break;
	}

	return retval;
}

int emac_phy_get_lpa_speed(struct emac_adapter *adpt, u32 *speed)
{
	struct emac_phy *phy = &adpt->phy;
	int retval;
	u16 lpa, stat1000;
	bool link;

	if (!phy->external)
		return phy->ops.link_check_no_ephy(adpt, speed, &link);

	retval = emac_phy_read(adpt, phy->addr, MII_LPA, &lpa);
	retval |= emac_phy_read(adpt, phy->addr, MII_STAT1000, &stat1000);
	if (retval)
		return retval;

	*speed = EMAC_LINK_SPEED_10_HALF;
	if (lpa & LPA_10FULL)
		*speed = EMAC_LINK_SPEED_10_FULL;
	else if (lpa & LPA_10HALF)
		*speed = EMAC_LINK_SPEED_10_HALF;
	else if (lpa & LPA_100FULL)
		*speed = EMAC_LINK_SPEED_100_FULL;
	else if (lpa & LPA_100HALF)
		*speed = EMAC_LINK_SPEED_100_HALF;
	else if (stat1000 & LPA_1000FULL)
		*speed = EMAC_LINK_SPEED_1GB_FULL;

	return 0;
}

/* read phy configuration and initialize it */
int emac_phy_config(struct platform_device *pdev, struct emac_adapter *adpt)
{
	struct emac_phy *phy = &adpt->phy;
	struct device_node *dt = pdev->dev.of_node;
	int ret;

	phy->external = !of_property_read_bool(dt, "qcom,no-external-phy");

	/* get phy address on MDIO bus */
	if (phy->external) {
		ret = of_property_read_u32(dt, "phy-addr", &phy->addr);
		if (ret)
			return ret;
	}

	/* get phy mode */
	ret = of_get_phy_mode(dt);
	if (ret < 0)
		return ret;

	phy->phy_mode = ret;
	/* sgmii v2 is always using ACPI */
	phy->ops = (phy->phy_mode == PHY_INTERFACE_MODE_RGMII) ?
		    emac_rgmii_ops : emac_sgmii_v1_ops;

	if (!phy->external)
		phy->uses_gpios = false;

	ret = phy->ops.config(pdev, adpt);
	if (ret)
		return ret;

	spin_lock_init(&phy->lock);

	phy->autoneg = true;
	phy->autoneg_advertised = EMAC_LINK_SPEED_DEFAULT;

	return phy->ops.init(adpt);
}

/* Flow Control (fc)  */
static int emac_get_fc_mode(struct emac_adapter *adpt,
			    enum emac_flow_ctrl *mode)
{
	struct emac_phy *phy = &adpt->phy;
	u16 i, bmsr = 0, pssr = 0;
	int retval = 0;

	for (i = 0; i < EMAC_MAX_SETUP_LNK_CYCLE; i++) {
		retval = emac_phy_read(adpt, phy->addr, MII_BMSR, &bmsr);
		if (retval)
			return retval;

		if (bmsr & BMSR_LSTATUS) {
			retval = emac_phy_read(adpt, phy->addr, MII_PSSR,
					       &pssr);
			if (retval)
				return retval;

			if (!(pssr & PSSR_SPD_DPLX_RESOLVED)) {
				emac_err(adpt,
					 "error for speed duplex resolved\n");
				return -EINVAL;
			}

			if ((pssr & PSSR_FC_TXEN) &&
			    (pssr & PSSR_FC_RXEN)) {
				*mode = (phy->req_fc_mode == EMAC_FC_FULL) ?
					EMAC_FC_FULL : EMAC_FC_RX_PAUSE;
			} else if (pssr & PSSR_FC_TXEN) {
				*mode = EMAC_FC_TX_PAUSE;
			} else if (pssr & PSSR_FC_RXEN) {
				*mode = EMAC_FC_RX_PAUSE;
			} else {
				*mode = EMAC_FC_NONE;
			}
			break;
		}
		msleep(100); /* link can take upto few seconds to come up */
	}

	if (i == EMAC_MAX_SETUP_LNK_CYCLE) {
		emac_err(adpt, "error when get flow control mode\n");
		retval = -EINVAL;
	}

	return retval;
}

int emac_phy_config_fc(struct emac_adapter *adpt)
{
	struct emac_phy *phy = &adpt->phy;
	struct emac_hw  *hw  = &adpt->hw;
	u32 mac;
	int retval;

	if (phy->disable_fc_autoneg || !phy->external) {
		phy->cur_fc_mode = phy->req_fc_mode;
	} else {
		retval = emac_get_fc_mode(adpt, &phy->cur_fc_mode);
		if (retval)
			return retval;
	}

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

void emac_reg_write_all(void __iomem *base, const struct emac_reg_write *itr)
{
	for (; itr->offset != END_MARKER; ++itr)
		writel_relaxed(itr->val, base + itr->offset);
}
