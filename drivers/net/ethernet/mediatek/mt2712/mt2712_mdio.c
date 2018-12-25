/* Copyright (C) 2017 MediaTek Inc.
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

#include "mt2712_yheader.h"

/* \brief read MII PHY register, function called by the driver alone
 *
 * \details Read MII registers through the API read_phy_reg where the
 * related MAC registers can be configured.
 *
 * \param[in] pdata - pointer to driver private data structure.
 * \param[in] phyaddr - the phy address to read
 * \param[in] phyreg - the phy regiester id to read
 * \param[out] phydata - pointer to the value that is read from the phy registers
 *
 * \return int
 *
 * \retval  0 - successfully read data from register
 * \retval -1 - error occurred
 * \retval  1 - if the feature is not defined.
 */

int mdio_read_direct(struct prv_data *pdata, int phyaddr, int phyreg, int *phydata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int phy_reg_read_status;

	if (hw_if->read_phy_regs) {
		phy_reg_read_status =
		    hw_if->read_phy_regs(phyaddr, phyreg, phydata);
	} else {
		phy_reg_read_status = 1;
		pr_err("%s: hw_if->read_phy_regs not defined", DEV_NAME);
	}

	return phy_reg_read_status;
}

/* \brief write MII PHY register, function called by the driver alone
 *
 * \details Writes MII registers through the API write_phy_reg where the
 * related MAC registers can be configured.
 *
 * \param[in] pdata - pointer to driver private data structure.
 * \param[in] phyaddr - the phy address to write
 * \param[in] phyreg - the phy regiester id to write
 * \param[out] phydata - actual data to be written into the phy registers
 *
 * \return void
 *
 * \retval  0 - successfully read data from register
 * \retval -1 - error occurred
 * \retval  1 - if the feature is not defined.
 */

int mdio_write_direct(struct prv_data *pdata, int phyaddr, int phyreg, int phydata)
{
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int phy_reg_write_status;

	if (hw_if->write_phy_regs) {
		phy_reg_write_status =
		    hw_if->write_phy_regs(phyaddr, phyreg, phydata);
	} else {
		phy_reg_write_status = 1;
		pr_err("%s: hw_if->write_phy_regs not defined", DEV_NAME);
	}

	return phy_reg_write_status;
}

/* \brief read MII PHY register.
 *
 * \details Read MII registers through the API read_phy_reg where the
 * related MAC registers can be configured.
 *
 * \param[in] bus - points to the mii_bus structure
 * \param[in] phyaddr - the phy address to write
 * \param[in] phyreg - the phy register offset to write
 *
 * \return int
 *
 * \retval  - value read from given phy register
 */

static int mdio_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
	struct net_device *dev = bus->priv;
	struct prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int phydata;

	if (hw_if->read_phy_regs)
		hw_if->read_phy_regs(phyaddr, phyreg, &phydata);
	else
		pr_err("%s: hw_if->read_phy_regs not defined", DEV_NAME);

	return phydata;
}

/* \brief API to write MII PHY register
 *
 * \details This API is expected to write MII registers with the value being
 * passed as the last argument which is done in write_phy_regs API
 * called by this function.
 *
 * \param[in] bus - points to the mii_bus structure
 * \param[in] phyaddr - the phy address to write
 * \param[in] phyreg - the phy register offset to write
 * \param[in] phydata - the register value to write with
 *
 * \return 0 on success and -ve number on failure.
 */

static int mdio_write(struct mii_bus *bus, int phyaddr, int phyreg, u16 phydata)
{
	struct net_device *dev = bus->priv;
	struct prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int ret = Y_SUCCESS;

	if (hw_if->write_phy_regs) {
		hw_if->write_phy_regs(phyaddr, phyreg, phydata);
	} else {
		ret = -1;
		pr_err("%s: hw_if->write_phy_regs not defined", DEV_NAME);
	}

	return ret;
}

/* \brief API to reset PHY
 *
 * \details This API is issue soft reset to PHY core and waits
 * until soft reset completes.
 *
 * \param[in] bus - points to the mii_bus structure
 *
 * \return 0 on success and -ve number on failure.
 */

static int mdio_reset(struct mii_bus *bus)
{
	struct net_device *dev = bus->priv;
	struct prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	int phydata;

	hw_if->read_phy_regs(pdata->phyaddr, MII_BMCR, &phydata);

	if (phydata < 0)
		return 0;

	/* issue soft reset to PHY */
	phydata |= BMCR_RESET;
	hw_if->write_phy_regs(pdata->phyaddr, MII_BMCR, phydata);

	/* wait until software reset completes */
	do {
		hw_if->read_phy_regs(pdata->phyaddr, MII_BMCR, &phydata);
	} while ((phydata >= 0) && (phydata & BMCR_RESET));

	return 0;
}

/* \details This function is invoked by other functions to get the PHY register
 * dump. This function is used during development phase for debug purpose.
 *
 * \param[in] pdata – pointer to private data structure.
 *
 * \return 0
 */

void dump_phy_registers(struct prv_data *pdata)
{
	int phydata = 0;

	pr_err("\n************* PHY Reg dump *************************\n");

	mdio_read_direct(pdata, pdata->phyaddr, MII_BMCR, &phydata);
	pr_err("Phy Control Reg(Basic Mode Control Reg) (%#x) = %#x\n", MII_BMCR, phydata);

	mdio_read_direct(pdata, pdata->phyaddr, MII_BMSR, &phydata);
	pr_err("Phy Status Reg(Basic Mode Status Reg) (%#x) = %#x\n", MII_BMSR, phydata);

	mdio_read_direct(pdata, pdata->phyaddr, MII_PHYSID1, &phydata);
	pr_err("Phy Id (PHYS ID 1) (%#x)= %#x\n", MII_PHYSID1, phydata);

	mdio_read_direct(pdata, pdata->phyaddr, MII_PHYSID2, &phydata);
	pr_err("Phy Id (PHYS ID 2) (%#x)= %#x\n", MII_PHYSID2, phydata);

	mdio_read_direct(pdata, pdata->phyaddr, MII_ADVERTISE, &phydata);
	pr_err("Auto-nego Adv (Advertisement Control Reg) (%#x) = %#x\n", MII_ADVERTISE, phydata);

	/* read Phy Control Reg */
	mdio_read_direct(pdata, pdata->phyaddr, MII_LPA, &phydata);
	pr_err("Auto-nego Lap (Link Partner Ability Reg) (%#x)= %#x\n", MII_LPA, phydata);

	mdio_read_direct(pdata, pdata->phyaddr, MII_EXPANSION, &phydata);
	pr_err("Auto-nego Exp (Extension Reg) (%#x) = %#x\n", MII_EXPANSION, phydata);

	mdio_read_direct(pdata, pdata->phyaddr, AUTO_NEGO_NP, &phydata);
	pr_err("Auto-nego Np (%#x) = %#x\n", AUTO_NEGO_NP, phydata);

	mdio_read_direct(pdata, pdata->phyaddr, MII_ESTATUS, &phydata);
	pr_err("Extended Status Reg (%#x) = %#x\n", MII_ESTATUS, phydata);

	mdio_read_direct(pdata, pdata->phyaddr, MII_CTRL1000, &phydata);
	pr_err("1000 Ctl Reg (1000BASE-T Control Reg) (%#x) = %#x\n", MII_CTRL1000, phydata);

	mdio_read_direct(pdata, pdata->phyaddr, MII_STAT1000, &phydata);
	pr_err("1000 Sts Reg (1000BASE-T Status)(%#x) = %#x\n", MII_STAT1000, phydata);

	mdio_read_direct(pdata, pdata->phyaddr, PHY_CTL, &phydata);
	pr_err("PHY Ctl Reg (%#x) = %#x\n", PHY_CTL, phydata);

	mdio_read_direct(pdata, pdata->phyaddr, PHY_STS, &phydata);
	pr_err("PHY Sts Reg (%#x) = %#x\n", PHY_STS, phydata);

	mdio_read_direct(pdata, pdata->phyaddr, 0x11, &phydata);
	pr_err("PHY 0x11 Reg = %#x\n", phydata);

	mdio_read_direct(pdata, pdata->phyaddr, 0x14, &phydata);
	pr_err("PHY 0x14 Reg = %#x\n", phydata);

	pr_err("\n****************************************************\n");
}

/* \brief API to adjust link parameters.
 *
 * \details This function will be called by PAL to inform the driver
 * about various link parameters like duplex and speed. This function
 * will configure the MAC based on link parameters.
 *
 * \param[in] dev - pointer to net_device structure
 *
 * \return void
 */

static void adjust_link(struct net_device *dev)
{
	struct prv_data *pdata = netdev_priv(dev);
	struct hw_if_struct *hw_if = &pdata->hw_if;
	struct phy_device *phydev = pdata->phydev;
	unsigned long flags;
	int new_state = 0;

	if (!phydev)
		return;

	spin_lock_irqsave(&pdata->lock, flags);

	if (phydev->link) {
		/* Now we make sure that we can be in full duplex mode.
		 * If not, we operate in half-duplex mode
		 */
		if (phydev->duplex != pdata->oldduplex) {
			new_state = 1;
			if (phydev->duplex)
				hw_if->set_full_duplex();
			else
				hw_if->set_half_duplex();

			pdata->oldduplex = phydev->duplex;
		}

		/* FLOW ctrl operation */
		if (phydev->pause || phydev->asym_pause) {
			if (pdata->flow_ctrl != pdata->oldflow_ctrl)
				configure_flow_ctrl(pdata);
		}

		if (phydev->speed != pdata->speed) {
			new_state = 1;
			switch (phydev->speed) {
			case SPEED_1000:
				hw_if->set_gmii_speed();
				break;
			case SPEED_100:
				hw_if->set_mii_speed_100();
				break;
			case SPEED_10:
				hw_if->set_mii_speed_10();
				break;
			}
			pdata->speed = phydev->speed;
		}

		if (!pdata->oldlink) {
			new_state = 1;
			pdata->oldlink = 1;
		}
	} else if (pdata->oldlink) {
		new_state = 1;
		pdata->oldlink = 0;
		pdata->speed = 0;
		pdata->oldduplex = -1;
	}

	if (new_state)
		phy_print_status(phydev);

	spin_unlock_irqrestore(&pdata->lock, flags);
}

/* \brief API to initialize PHY.
 *
 * \details This function will initializes the driver's PHY state and attaches
 * the PHY to the MAC driver.
 *
 * \param[in] dev - pointer to net_device structure
 *
 * \return integer
 *
 * \retval 0 on success & negative number on failure.
 */

static int init_phy(struct net_device *dev)
{
	struct prv_data *pdata = netdev_priv(dev);
	struct phy_device *phydev = NULL;
	char phy_id_fmt[MII_BUS_ID_SIZE + 3];
	char bus_id[MII_BUS_ID_SIZE];

	snprintf(bus_id, MII_BUS_ID_SIZE, "dwc_phy-%x", pdata->bus_id);

	snprintf(phy_id_fmt, MII_BUS_ID_SIZE + 3, PHY_ID_FMT, bus_id, pdata->phyaddr);

	phydev = phy_connect(dev, phy_id_fmt, &adjust_link, pdata->interface);
	if (IS_ERR(phydev)) {
		pr_err("%s: Could not attach to PHY\n", dev->name);
		return PTR_ERR(phydev);
	}

	if (phydev->phy_id == 0) {
		phy_disconnect(phydev);
		return -ENODEV;
	}

	if (pdata->interface == PHY_INTERFACE_MODE_GMII) {
		phydev->supported = PHY_GBIT_FEATURES;
	} else if ((pdata->interface == PHY_INTERFACE_MODE_MII) ||
		(pdata->interface == PHY_INTERFACE_MODE_RMII)) {
		phydev->supported = PHY_BASIC_FEATURES;
	}

	phydev->advertising = phydev->supported;

	pdata->phydev = phydev;
	phy_start(pdata->phydev);

	return 0;
}

/* \brief API to register mdio.
 *
 * \details This function will allocate mdio bus and register it
 * phy layer.
 *
 * \param[in] dev - pointer to net_device structure
 *
 * \return 0 on success and -ve on failure.
 */

int mdio_register(struct net_device *dev)
{
	struct prv_data *pdata = netdev_priv(dev);
	struct mii_bus *new_bus = NULL;
	int phyaddr = 0;
	unsigned short phy_detected = 0;
	int ret = Y_SUCCESS;

	/* find the phy ID or phy address which is connected to our MAC */
	for (phyaddr = 0; phyaddr < 32; phyaddr++) {
		int phy_reg_read_status, mii_status;

		phy_reg_read_status =
		    mdio_read_direct(pdata, phyaddr, MII_BMSR, &mii_status);
		if (phy_reg_read_status == 0) {
			if (mii_status != 0x0000 && mii_status != 0xffff) {
				pr_err("%s: Phy detected at ID/ADDR %d\n", DEV_NAME, phyaddr);
				phy_detected = 1;
				break;
			}
		} else if (phy_reg_read_status < 0) {
			pr_err("%s: Error reading the phy register MII_BMSR for phy ID/ADDR %d\n", DEV_NAME, phyaddr);
		}
	}
	if (!phy_detected) {
		pr_err("%s: No phy could be detected\n", DEV_NAME);
		return -ENOLINK;
	}
	pdata->phyaddr = phyaddr;
	pdata->bus_id = 0x1;

	dump_phy_registers(pdata);

	new_bus = mdiobus_alloc();
	if (!new_bus) {
		pr_err("Unable to allocate mdio bus\n");
		return -ENOMEM;
	}

	new_bus->name = "dwc_phy";
	new_bus->read = mdio_read;
	new_bus->write = mdio_write;
	new_bus->reset = mdio_reset;
	snprintf(new_bus->id, MII_BUS_ID_SIZE, "%s-%x", new_bus->name,
		 pdata->bus_id);
	new_bus->priv = dev;
	new_bus->phy_mask = 0;
	new_bus->parent = &pdata->pdev->dev;
	ret = mdiobus_register(new_bus);
	if (ret != 0) {
		pr_err("%s: Cannot register as MDIO bus\n", new_bus->name);
		mdiobus_free(new_bus);
		return ret;
	}
	pdata->mii = new_bus;

	ret = init_phy(dev);
	if (unlikely(ret)) {
		pr_err("Cannot attach to PHY (error: %d)\n", ret);
		goto err_out_phy_connect;
	}

	return ret;

 err_out_phy_connect:
	mdio_unregister(dev);
	return ret;
}

/* \brief API to unregister mdio.
 *
 * \details This function will unregister mdio bus and free's the memory
 * allocated to it.
 *
 * \param[in] dev - pointer to net_device structure
 *
 * \return void
 */

void mdio_unregister(struct net_device *dev)
{
	struct prv_data *pdata = netdev_priv(dev);

	if (pdata->phydev) {
		phy_stop(pdata->phydev);
		phy_disconnect(pdata->phydev);
		pdata->phydev = NULL;
	}

	mdiobus_unregister(pdata->mii);
	pdata->mii->priv = NULL;
	mdiobus_free(pdata->mii);
	pdata->mii = NULL;
}
