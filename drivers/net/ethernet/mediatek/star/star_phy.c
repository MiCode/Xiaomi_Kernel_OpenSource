/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Zhiyong Tao <zhiyong.tao@mediatek.com>
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

#include "star.h"

static void ex_phy_reset(star_dev *sdev, u32 id)
{
	u16 val = 0;

	val = star_mdc_mdio_read(sdev, id, 0);
	val |= BMCR_RESET;
	star_mdc_mdio_write(sdev, id, 0, val);
}

static void ex_phy_re_an(star_dev *dev, u32 id)
{
	u16 val = 0;

	val = star_mdc_mdio_read(dev, id, 0);
	/* enable AN, and restart AN for SMSC PHY */
	val |= BMCR_ANENABLE | BMCR_ANRESTART;
	star_mdc_mdio_write(dev, id, 0, val);
}

static void ex_phy_dis_gpsi(star_dev *dev, u32 id)
{
	u16 val = 0;

	val = star_mdc_mdio_read(dev, id, 18);
	val &= ~0x400;
	star_mdc_mdio_write(dev, id, 18, val);
}

static void smsc8710a_phy_init(star_dev *sdev)
{
	u32 phy_id = sdev->phy_ops->addr;

	/* E2 ECO fixup the problem which 10M can't rx packet on E1 IC */
	star_set_bit(star_dummy(sdev->base), STAR_DUMMY_E2_ECO);
	/* for smsc8710a, after soft reset,
	 * AN is disabled, so enable it again
	 */
	ex_phy_reset(sdev, phy_id);
	ex_phy_re_an(sdev, phy_id);
}

static struct eth_phy_ops smsc8710a_phy_ops = {
	.phy_id = PHYID2_SMSC8710A,
	.init = smsc8710a_phy_init,
};

static void dm9162_phy_init(star_dev *sdev)
{
	u32 phy_id = sdev->phy_ops->addr;

	ex_phy_reset(sdev, phy_id);
	ex_phy_dis_gpsi(sdev, phy_id);
}

static struct eth_phy_ops dm9162_phy_ops = {
	.phy_id = PHYID2_DM9162_XMII,
	.init = dm9162_phy_init,
};

static void ksz8081mnx_phy_init(star_dev *sdev)
{
	u32 data;
	star_private *star_prv = sdev->star_prv;

	/*set davicom phy register0 bit10 is 0 in MII for mt8160*/
	data = star_mdc_mdio_read(sdev, star_prv->phy_addr, 0x0) & (~(1 << 10));
	star_mdc_mdio_write(sdev, star_prv->phy_addr, 0x0, data);
	data = star_mdc_mdio_read(sdev, star_prv->phy_addr, 0x0);
}

static struct eth_phy_ops ksz8081mnx_phy_ops = {
	.phy_id = PHYID2_KSZ8081MNX,
	.init = ksz8081mnx_phy_init,
};

static void default_phy_init(star_dev *sdev)
{
	u32 phy_id = sdev->phy_ops->addr;

	/* E2 ECO fixup the problem which 10M can't rx packet on E1 IC */
	star_set_bit(star_dummy(sdev->base), STAR_DUMMY_E2_ECO);
	ex_phy_reset(sdev, phy_id);
}

static struct eth_phy_ops default_phy_ops = {
	.phy_id = 0,
	.init = default_phy_init,
};

static void ip101g_az_disable(star_dev *dev, u32 id)
{
	star_mdc_mdio_write(dev, id, 0x0d, 0x0007);
	star_mdc_mdio_write(dev, id, 0x0e, 0x003c);
	star_mdc_mdio_write(dev, id, 0x0d, 0x4007);
	star_mdc_mdio_write(dev, id, 0x0e, 0x0000);
	star_mdc_mdio_read(dev, id, 0x0e);
}

static void ip101g_anar_init(star_dev *sdev, u32 id)
{
	u16 val = 0;

	val = star_mdc_mdio_read(sdev, id, 4);
	val &= ~(ADVERTISE_NPAGE | ADVERTISE_RFAULT | ADVERTISE_PAUSE_ASYM);
	star_mdc_mdio_write(sdev, id, 4, val);
}

static void ip101g_phy_init(star_dev *sdev)
{
	u32 phy_id = sdev->phy_ops->addr;

	/* E2 ECO fixup the problem which 10M can't rx packet on E1 IC */
	star_set_bit(star_dummy(sdev->base), STAR_DUMMY_E2_ECO);
	ex_phy_reset(sdev, phy_id);
	ip101g_az_disable(sdev, phy_id);
	ip101g_anar_init(sdev, phy_id);
}

static struct eth_phy_ops ip101g_phy_ops = {
	.phy_id = PHYID2_IP101G,
	.init = ip101g_phy_init,
};

static void rtl8201fr_phy_init(star_dev *sdev)
{
	u16 save_page;
	u32 temp;
	star_private *star_prv = sdev->star_prv;

	/* save page */
	save_page = star_mdc_mdio_read(sdev, star_prv->phy_addr, 31);
	/* set to page0 */
	star_mdc_mdio_write(sdev, star_prv->phy_addr, 31, 0x0000);
	/* set register 0[15]=1 */
	temp = star_mdc_mdio_read(sdev, star_prv->phy_addr, 0);
	star_mdc_mdio_write(sdev, star_prv->phy_addr, 0, temp | (1 << 15));
	/* set register 0[12]=0 */
	temp = star_mdc_mdio_read(sdev, star_prv->phy_addr, 0);
	star_mdc_mdio_write(sdev, star_prv->phy_addr, 0, temp & (~(1 << 12)));
	/* set register 4[11]=0 */
	temp = star_mdc_mdio_read(sdev, star_prv->phy_addr, 4);
	star_mdc_mdio_write(sdev, star_prv->phy_addr, 4, temp & (~(1 << 11)));
	/* set register 0[12]=1 */
	temp = star_mdc_mdio_read(sdev, star_prv->phy_addr, 0);
	star_mdc_mdio_write(sdev, star_prv->phy_addr, 0, temp | (1 << 12));
	/* set page from save_page */
	star_mdc_mdio_write(sdev, star_prv->phy_addr, 31, save_page);

	save_page = star_mdc_mdio_read(sdev, star_prv->phy_addr, 31);
	/* set page 4 */
	star_mdc_mdio_write(sdev, star_prv->phy_addr, 31, 0x0004);
	/* EEE_nway_disable */
	star_mdc_mdio_write(sdev, star_prv->phy_addr, 16, 0x4077);
	/* set to page0 */
	star_mdc_mdio_write(sdev, star_prv->phy_addr, 31, 0x0000);
	/* Set Address mode and MMD Device = 7 */
	star_mdc_mdio_write(sdev, star_prv->phy_addr, 13, 0x0007);
	/* Set Address Value */
	star_mdc_mdio_write(sdev, star_prv->phy_addr, 14, 0x003C);
	/* Set Data mode and MMD Device = 7 */
	star_mdc_mdio_write(sdev, star_prv->phy_addr, 13, 0x4007);
	/* turn off 100BASE-TX EEE capability */
	star_mdc_mdio_write(sdev, star_prv->phy_addr, 14, 0x0000);
	/* Restart Auto-Negotiation */
	star_mdc_mdio_write(sdev, star_prv->phy_addr, 0, 0x1200);
	star_mdc_mdio_write(sdev, star_prv->phy_addr, 31, save_page);

	/* init tx for realtek RMII timing issue */
	temp = star_get_reg(star_test0(sdev->base));
	temp &= ~(0x1 << 31);
	/* select tx clock inverse */
	temp |= (0x1 << 31);
	star_set_reg(star_test0(sdev->base), temp);
	STAR_PR_INFO("0x58(0x%x).\n",
		     star_get_reg(star_test0(sdev->base)));
}

void rtl8201fr_wol_enable(struct net_device *netdev)
{
	star_private *star_prv = NULL;
	star_dev *dev = NULL;
	struct sockaddr sa;
	char *mac_addr = sa.sa_data;
	u32 val = 0;

	star_prv = netdev_priv(netdev);
	dev = &star_prv->star_dev;

	STAR_PR_INFO("enter %s\n", __func__);

	memcpy(sa.sa_data, netdev->dev_addr, netdev->addr_len);
	STAR_PR_INFO("device mac address:%x %x %x %x %x %x.\n",
		     netdev->dev_addr[0], netdev->dev_addr[1],
		     netdev->dev_addr[2], netdev->dev_addr[3],
		     netdev->dev_addr[4], netdev->dev_addr[5]);

	/* enable phy wol */
	star_mdc_mdio_write(dev, star_prv->phy_addr, 4, 0x61);
	star_mdc_mdio_write(dev, star_prv->phy_addr, 0, 0x3200);

	/* set mac address */
	star_mdc_mdio_write(dev, star_prv->phy_addr, 31, 0x12);
	star_mdc_mdio_write(dev, star_prv->phy_addr, 16,
			    (mac_addr[1] << 8) | (mac_addr[0] << 0));
	star_mdc_mdio_write(dev, star_prv->phy_addr, 17,
			    (mac_addr[3] << 8) | (mac_addr[2] << 0));
	star_mdc_mdio_write(dev, star_prv->phy_addr, 18,
			    (mac_addr[5] << 8) | (mac_addr[4] << 0));
	STAR_PR_INFO("mac address:%x %x %x %x %x %x.\n",
		     mac_addr[0], mac_addr[1], mac_addr[2],
		 mac_addr[3], mac_addr[4], mac_addr[5]);

	/* set max length */
	star_mdc_mdio_write(dev, star_prv->phy_addr, 31, 0x11);
	star_mdc_mdio_write(dev, star_prv->phy_addr, 17, 0x1FFF);

	/* enable magic packet event */
	star_mdc_mdio_write(dev, star_prv->phy_addr, 16, 0x1000);

	/* set tx isolate */
	star_mdc_mdio_write(dev, star_prv->phy_addr, 31, 0x7);
	val = star_mdc_mdio_read(dev, star_prv->phy_addr, 20);
	star_mdc_mdio_write(dev, star_prv->phy_addr, 20, val | (1 << 15));

	/* set rx isolate */
	star_mdc_mdio_write(dev, star_prv->phy_addr, 31, 0x17);
	val = star_mdc_mdio_read(dev, star_prv->phy_addr, 19);
	star_mdc_mdio_write(dev, star_prv->phy_addr, 19, val | (1 << 15));

	/* return page 0 */
	star_mdc_mdio_write(dev, star_prv->phy_addr, 31, 0);
}

void rtl8201fr_wol_disable(struct net_device *netdev)
{
	u32 val = 0;
	star_private *star_prv = netdev_priv(netdev);
	star_dev *dev = &star_prv->star_dev;

	STAR_PR_INFO("enter %s\n", __func__);

	/* unset rx isolate */
	star_mdc_mdio_write(dev, star_prv->phy_addr, 31, 0x17);
	val = star_mdc_mdio_read(dev, star_prv->phy_addr, 19);
	star_mdc_mdio_write(dev, star_prv->phy_addr, 19, val & (~(1 << 15)));

	/* unset tx isolate */
	star_mdc_mdio_write(dev, star_prv->phy_addr, 31, 0x7);
	val = star_mdc_mdio_read(dev, star_prv->phy_addr, 20);
	star_mdc_mdio_write(dev, star_prv->phy_addr, 20, val & (~(1 << 15)));

	star_mdc_mdio_write(dev, star_prv->phy_addr, 31, 0x11);
	/* disable magic packet event */
	star_mdc_mdio_write(dev, star_prv->phy_addr, 16, 0x0);

	/* unset max length and reset PMEB pin as high */
	star_mdc_mdio_write(dev, star_prv->phy_addr, 17, 0x9FFF);

	/* return page 0 */
	star_mdc_mdio_write(dev, star_prv->phy_addr, 31, 0);
}

static struct eth_phy_ops rtl8201fr_phy_ops = {
	.phy_id = PHYID2_RTL8201FR,
	.init = rtl8201fr_phy_init,
	.wol_enable = rtl8201fr_wol_enable,
	.wol_disable = rtl8201fr_wol_disable,
};

int star_detect_phyid(star_dev *dev)
{
	int addr;
	u16 reg2;

	for (addr = 0; addr < 32; addr++) {
		reg2 = star_mdc_mdio_read(dev, addr, PHY_REG_IDENTFIR2);
		STAR_PR_INFO("%s(%d) id=%d, vendor=0x%x\n",
			     __func__, __LINE__, addr, reg2);

		if (reg2 == PHYID2_SMSC8710A) {
			STAR_PR_INFO("Ethernet: SMSC8710A PHY\n\r");
			dev->phy_ops = &smsc8710a_phy_ops;
			dev->phy_ops->addr = addr;
			break;
		} else if (reg2 == PHYID2_DM9162_XMII) {
			STAR_PR_INFO("Ethernet: DM9162 PHY\n\r");
			dev->phy_ops = &dm9162_phy_ops;
			dev->phy_ops->addr = addr;
			break;
		} else if (reg2 == PHYID2_KSZ8081MNX) {
			STAR_PR_INFO("Ethernet: KSZ8081 PHY\n\r");
			dev->phy_ops = &ksz8081mnx_phy_ops;
			dev->phy_ops->addr = addr;
			break;
		} else if (reg2 == PHYID2_IP101G) {
			STAR_PR_INFO("Ethernet: IP101G PHY\n\r");
			dev->phy_ops = &ip101g_phy_ops;
			dev->phy_ops->addr = addr;
			break;
		} else if (reg2 == PHYID2_RTL8201FR) {
			STAR_PR_INFO("Ethernet: RTL8201FR PHY\n\r");
			dev->phy_ops = &rtl8201fr_phy_ops;
			dev->phy_ops->addr = addr;
			break;
		}
	}

	if (addr == 32) {
		for (addr = 0; addr < 32; addr++) {
			reg2 = star_mdc_mdio_read(dev, addr, PHY_REG_IDENTFIR2);
			STAR_PR_DEBUG(
				 "%s id=%d, vendor=0x%x\n", __func__,
				 addr, reg2);

			if (reg2 != 0xFFFF) {
				STAR_PR_ERR(
					 "Don't support current PHY\n");
				dev->phy_ops = &default_phy_ops;
				dev->phy_ops->phy_id = reg2;
				dev->phy_ops->addr = addr;
				break;
			}
		}
	}

	return addr;
}

