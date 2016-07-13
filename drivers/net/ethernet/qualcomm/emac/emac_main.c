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

/* Qualcomm Technologies, Inc. EMAC Ethernet Controller driver.
 */

#include <linux/gpio.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/of_gpio.h>
#include <linux/acpi.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/tcp.h>
#include <linux/regulator/consumer.h>
#if IS_ENABLED(CONFIG_ACPI)
#include <linux/gpio/consumer.h>
#include <linux/property.h>
#include <net/ip6_checksum.h>
#endif

#include "emac.h"
#include "emac_phy.h"
#include "emac_hw.h"
#include "emac_ptp.h"

#define DRV_VERSION "1.1.0.0"

char emac_drv_name[] = "qcom_emac";
const char emac_drv_description[] =
			     "Qualcomm Technologies, Inc. EMAC Ethernet Driver";
const char emac_drv_version[] = DRV_VERSION;
static struct of_device_id emac_dt_match[];

#define EMAC_MSG_DEFAULT (NETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_MSG_LINK |  \
		NETIF_MSG_TIMER | NETIF_MSG_IFDOWN | NETIF_MSG_IFUP |         \
		NETIF_MSG_RX_ERR | NETIF_MSG_TX_ERR | NETIF_MSG_TX_QUEUED |   \
		NETIF_MSG_INTR | NETIF_MSG_TX_DONE | NETIF_MSG_RX_STATUS |    \
		NETIF_MSG_PKTDATA | NETIF_MSG_HW | NETIF_MSG_WOL)

/* Error bits that will result in a received frame being discarded */
#define EMAC_RRDES_ERROR (EMAC_RRDES_IPF | EMAC_RRDES_CRC | EMAC_RRDES_FAE | \
			EMAC_RRDES_TRN | EMAC_RRDES_RNT | EMAC_RRDES_INC | \
			EMAC_RRDES_FOV | EMAC_RRDES_LEN)
#define EMAC_RRDES_STATS_DW_IDX 3

#define EMAC_RRDESC_SIZE      4
#define EMAC_TS_RRDESC_SIZE   6
#define EMAC_TPDESC_SIZE      4
#define EMAC_RFDESC_SIZE      2

#define EMAC_RSS_IDT_SIZE     256

#define EMAC_SKB_CB(skb) ((struct emac_skb_cb *)(skb)->cb)

#define EMAC_PINCTRL_STATE_MDIO_ACTIVE "emac_mdio_active"
#define EMAC_PINCTRL_STATE_MDIO_SLEEP  "emac_mdio_sleep"
#define EMAC_PINCTRL_STATE_EPHY_ACTIVE "emac_ephy_active"
#define EMAC_PINCTRL_STATE_EPHY_SLEEP  "emac_ephy_sleep"

struct emac_skb_cb {
	u32           tpd_idx;
	unsigned long jiffies;
};

#define EMAC_HWTXTSTAMP_CB(skb) ((struct emac_hwtxtstamp_cb *)(skb)->cb)

struct emac_hwtxtstamp_cb {
	u32 sec;
	u32 ns;
};

static int msm_emac_msglvl = -1;
module_param_named(msglvl, msm_emac_msglvl, int, S_IRUGO | S_IWUSR | S_IWGRP);

static int msm_emac_intr_ext;
module_param_named(intr_ext, msm_emac_intr_ext, int,
		   S_IRUGO | S_IWUSR | S_IWGRP);

static irqreturn_t emac_isr(int irq, void *data);
static irqreturn_t emac_wol_isr(int irq, void *data);

/* EMAC HW has an issue with interrupt assignment because of which receive queue
 * 1 is disabled and following receive rss queue to interrupt mapping is used:
 * rss-queue   intr
 *    0        core0
 *    1        core3 (disabled)
 *    2        core1
 *    3        core2
 */
const struct emac_irq_common emac_irq_cmn_tbl[EMAC_IRQ_CNT] = {
	{ "emac_core0_irq", emac_isr, EMAC_INT_STATUS,  EMAC_INT_MASK,
		RX_PKT_INT0,	0},
	{ "emac_core3_irq", emac_isr, EMAC_INT3_STATUS, EMAC_INT3_MASK,
		0,		0},
	{ "emac_core1_irq", emac_isr, EMAC_INT1_STATUS, EMAC_INT1_MASK,
		RX_PKT_INT2,	0},
	{ "emac_core2_irq", emac_isr, EMAC_INT2_STATUS, EMAC_INT2_MASK,
		RX_PKT_INT3,	0},
	{ "emac_wol_irq"  , emac_wol_isr,            0,              0,
		0,		0},
};

static const char * const emac_gpio_name[] = {
	"qcom,emac-gpio-mdc", "qcom,emac-gpio-mdio"
};

static const char * const emac_clk_name[] = {
	"axi_clk", "cfg_ahb_clk", "125m_clk", "25m_clk", "tx_clk", "rx_clk",
	"sys_clk"
};

static const char * const emac_regulator_name[] = {
	"emac_vreg1", "emac_vreg2", "emac_vreg3", "emac_vreg4", "emac_vreg5"
};

#if IS_ENABLED(CONFIG_ACPI)
static const struct acpi_device_id emac_acpi_match[] = {
	{ "QCOM8070", 0 },
	{ }
}
MODULE_DEVICE_TABLE(acpi, emac_acpi_match);

static int emac_acpi_clk_set_rate(struct emac_adapter *adpt,
				  enum emac_clk_id id, u64 rate)
{
	union acpi_object params[4], args;
	union acpi_object *obj;
	const char duuid[16];
	int ret = 0;

	params[0].type = ACPI_TYPE_INTEGER;
	params[0].integer.value = id;
	params[1].type = ACPI_TYPE_INTEGER;
	params[1].integer.value = rate;
	args.type = ACPI_TYPE_PACKAGE;
	args.package.count = 2;
	args.package.elements = &params[0];

	obj = acpi_evaluate_dsm(ACPI_HANDLE(adpt->netdev->dev.parent), duuid,
				1, 2, &args);
	if (!obj)
		return -EINVAL;

	if ((obj->type != ACPI_TYPE_INTEGER) || (obj->integer.value)) {
		ret = -EINVAL;
		emac_err(clk_info->adpt,
			 "set clock rate for %d failed\n", clk_info->index);
	}

	ACPI_FREE(obj);
	return ret;
}

#define EMAC_PHY_MODE_MAX_LEN		16
struct emac_phy_mode_lookup {
	char	name[EMAC_PHY_MODE_MAX_LEN];
	int	type;
};

static phy_interface_t emac_phy_interface_type(const char *phy_mode)
{
	static struct emac_phy_mode_lookup[] = {
		{"sgmii",	PHY_INTERFACE_MODE_SGMII},
		{"rgmii",	PHY_INTERFACE_MODE_RGMII},
		{"xgmii",	PHY_INTERFACE_MODE_XGMII},
		{"rgmii-id",	PHY_INTERFACE_MODE_RGMII_ID},
		{"rgmii-txid",	PHY_INTERFACE_MODE_RGMII_TXID},
		{"tbi",		PHY_INTERFACE_MODE_TBI},
		{"rmii",	PHY_INTERFACE_MODE_RMII},
		{"rtbi",	PHY_INTERFACE_MODE_RTBI},
		{NULL,		0},
	};

	struct emac_phy_mode_lookup *cur = emac_phy_mode_lookup;

	if (!phy_mode)
		return PHY_INTERFACE_MODE_NA;

	for (; cur->name; ++cur)
		if (!strcasecmp(phy_mode, cur->name))
			return cur->type;

	return PHY_INTERFACE_MODE_MII;
}

static int emac_device_property_read_string(struct device *dev,
					    const char *propname,
					    const char **val)
{
	return (ACPI_HANDLE(dev)) ?
		acpi_dev_prop_read(ACPI_COMPANION(dev), propname,
				   DEV_PROP_STRING, val, 1) :
		of_property_read_string(dev->of_node, propname, val);
}

static inline bool emac_device_property_read_bool(struct device *dev,
						  const char *propname)
{
	return (ACPI_HANDLE(dev)) ?
		!acpi_dev_prop_get(ACPI_COMPANION(dev), propname, NULL) :
		of_property_read_bool(dev->of_node, propname);
}

static inline int emac_device_property_read_u32(struct device *dev,
						const char *propname, u32 *val)
{
	return DEV_PROP_READ_ARRAY(dev, propname, u32, DEV_PROP_U32, val, 1);
	return (ACPI_HANDLE(dev) ?
		acpi_dev_prop_read((ACPI_COMPANION(dev)), propname,
				   DEV_PROP_U32, val, nval) :
		of_property_read_u32(dev->of_node, propname, val);
}

static int emac_device_property_read_u8_array(struct device *dev,
					      const char *propname,
					      u8 *val, size_t nval)
{
	return (ACPI_HANDLE(dev) ?
		acpi_dev_prop_read((ACPI_COMPANION(dev)), propname, DEV_PROP_U8,
				   val, nval) :
		of_property_read_u8_array(dev->of_node, propname, u8, val,
					  nval);
}

static int emac_acpi_get_properties(struct platform_device *pdev,
				    struct emac_adapter *adpt)
{
	struct device *dev = &pdev->dev;
	const char *phy_mode;
	u8 maddr[ETH_ALEN];
	int ret;

	ret = emac_device_property_read_string(dev, "phy-mode", &phy_mode);
	if (ret < 0)
		phy_mode = NULL;
	adpt->phy_mode = emac_phy_interface_type(phy_mode);

	ret = emac_device_property_read_u32(dev, "phy-channel",
					    &adpt->hw.phy_addr);
	if (ret < 0)
		adpt->hw.phy_addr = 0;

	ret = emac_device_property_read_u8_array(dev, "mac-address", maddr,
						 ETH_ALEN);
	if (ret < 0) {
		dev_err(&pdev->dev, "no MAC address found\n");
		return ret;
	}

	if (!is_valid_ether_addr(maddr)) {
		dev_err(&pdev->dev, "invalid MAC address %pM\n", maddr);
		return -EINVAL;
	}

	ret = emac_device_property_read_u32(dev, "phy-version",
					    &adpt->hw.phy_version);
	if (ret < 0)
		adpt->hw.phy_version = 0;

	adpt->phy.external = !emac_device_property_read_bool(dev, "no-ephy");
	adpt->tstamp_en = device_property_read_bool(dev, "tstamp-eble");
	ether_addr_copy(adpt->hw.mac_perm_addr, maddr);
	return 0;
}

static int emac_acpi_get_resources(struct platform_device *pdev,
				   struct emac_adapter *adpt)
{
	struct device *dev = &pdev->dev;
	struct net_device *netdev = adpt->netdev;
	struct emac_irq_info *irq_info;
	union acpi_object *obj;
	struct resource *res;
	void __iomem *regmap;
	int i, retval;
	const char duuid[16];
	u16 irq_map[EMAC_NUM_IRQ] = {EMAC_CORE0_IRQ, EMAC_CORE3_IRQ,
				     EMAC_CORE1_IRQ, EMAC_CORE2_IRQ,
				     EMAC_SGMII_PHY_IRQ, EMAC_WOL_IRQ};

	/* Get device specific properties */
	retval = emac_acpi_get_properties(pdev, adpt);
	if (retval < 0)
		return -ENODEV;

	/* Execute DSM function 1 to initialize the clocks */
	obj = acpi_evaluate_dsm(ACPI_HANDLE(dev), duuid, 0x1, 1, NULL);
	if (!obj)
		return -EINVAL;

	if ((obj->type != ACPI_TYPE_INTEGER) || (obj->integer.value)) {
		emac_err(adpt, "failed to excute _DSM method function 1\n");
		ACPI_FREE(obj);
		return -ENOENT;
	}
	ACPI_FREE(obj);

	dev_info(&pdev->dev, "MAC address %pM\n", adpt->hw.mac_perm_addr);

	/* set phy address to zero for internal phy */
	if (!adpt->phy.external)
		adpt->hw.phy_addr = 0;

	/* check phy mode */
	if (adpt->phy_mode == PHY_INTERFACE_MODE_NA) {
		emac_err(adpt, "PHY interface mode not valid\n");
		return -EINVAL;
	}

	/* Assume GPIOs required for MDC/MDIO are enabled in firmware */
	adpt->phy.uses_gpios = true;

	/* get irqs */
	for (i = 0; i < EMAC_NUM_IRQ; i++) {
		/* SGMII_PHY IRQ is only required if phy_mode is "sgmii" */
		if ((irq_map[i] == EMAC_SGMII_PHY_IRQ) &&
		    (adpt->phy_mode != PHY_INTERFACE_MODE_SGMII))
			continue;

		irq_info = &adpt->irq_info[irq_map[i]];

		retval = platform_get_irq(pdev, i);
		if (retval < 0) {
			/* If WOL IRQ is not specified, WOL is disabled */
			if (irq_map[i] == EMAC_WOL_IRQ)
				continue;
			emac_err(adpt, "irq %d not found\n", i);
			return retval;
		}
		irq_info->irq = retval;
	}

	/* get register addresses */
	for (i = 0; i < NUM_EMAC_REG_BASES; i++) {
		/* 1588 is required only if tstamp is enabled */
		if ((i == EMAC_1588) && !adpt->tstamp_en)
			continue;

		/* qserdes & sgmii_phy are required only for sgmii phy */
		if ((adpt->phy_mode != PHY_INTERFACE_MODE_SGMII) &&
		    ((i == EMAC_QSERDES) || (i == EMAC_SGMII_PHY)))
			continue;

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			emac_err(adpt, "can't get %d iomem resource\n", i);
			return -ENOMEM;
		}

		regmap = devm_ioremap(&pdev->dev, res->start,
				      resource_size(res));
		/**
		 * SGMII-v2 controller has two CSRs one per lane digital part
		 * and second one for per lane analog part. The PHY regmap is
		 * compatible to SGMII-v1 controller and will be used in PHY
		 * common code and sgmii_laned regmap referenced in SGMII-v2
		 * specific initialization code.
		 */
		if ((i == EMAC_SGMII_PHY) &&
		    (adpt->hw.phy_version == SGMII_PHY_VERSION_2)) {
			adpt->hw.sgmii_laned = regmap;
			regmap += SGMII_PHY_LN_OFFSET;
		}
		adpt->hw.reg_addr[i] = regmap;

		if (!adpt->hw.reg_addr[i]) {
			emac_err(adpt, "can't ioremap %pR\n", res);
			return -EFAULT;
		}
	}
	netdev->base_addr = (unsigned long)adpt->hw.reg_addr[EMAC];
	return 0;
}
#else
static int emac_acpi_clk_set_rate(struct emac_adapter *adpt,
				  enum emac_clk_id id, u64 rate)
{
	return -ENXIO;
}

static int emac_acpi_get_resources(struct platform_device *pdev,
				   struct emac_adapter *adpt)
{
	return -ENXIO;
}
#endif

static int emac_clk_prepare_enable(struct emac_adapter *adpt,
				   enum emac_clk_id id)
{
	int ret;

	if (ACPI_HANDLE(adpt->netdev->dev.parent))
		return 0;

	ret = clk_prepare_enable(adpt->clk[id].clk);
	if (ret)
		emac_err(adpt, "error:%d on clk_prepare_enable(%s)\n", ret,
			 emac_clk_name[id]);
	else
		adpt->clk[id].enabled = true;

	return ret;
}

int emac_clk_set_rate(struct emac_adapter *adpt, enum emac_clk_id id,
		      enum emac_clk_rate rate)
{
	int ret;

	if (ACPI_HANDLE(adpt->netdev->dev.parent))
		return emac_acpi_clk_set_rate(adpt, id, rate);

	ret = clk_set_rate(adpt->clk[id].clk, rate);
	if (ret)
		emac_err(adpt, "error:%d on clk_set_rate(%s, %d)\n", ret,
			 emac_clk_name[id], rate);

	return ret;
}

/* reinitialize */
void emac_reinit_locked(struct emac_adapter *adpt)
{
	struct net_device *netdev = adpt->netdev;

	WARN_ON(in_interrupt());

	while (TEST_N_SET_FLAG(adpt, ADPT_STATE_RESETTING))
		msleep(20); /* Reset might take few 10s of ms */

	if (TEST_FLAG(adpt, ADPT_STATE_DOWN)) {
		CLR_FLAG(adpt, ADPT_STATE_RESETTING);
		return;
	}

	pm_runtime_get_sync(netdev->dev.parent);
	emac_down(adpt, EMAC_HW_CTRL_RESET_MAC);

	adpt->phy.ops.reset(adpt);
	emac_up(adpt);
	pm_runtime_mark_last_busy(netdev->dev.parent);
	pm_runtime_put_autosuspend(netdev->dev.parent);

	CLR_FLAG(adpt, ADPT_STATE_RESETTING);
}

void emac_task_schedule(struct emac_adapter *adpt)
{
	if (!TEST_FLAG(adpt, ADPT_STATE_DOWN) &&
	    !TEST_FLAG(adpt, ADPT_STATE_WATCH_DOG)) {
		SET_FLAG(adpt, ADPT_STATE_WATCH_DOG);
		schedule_work(&adpt->emac_task);
	}
}

void emac_check_lsc(struct emac_adapter *adpt)
{
	SET_FLAG(adpt, ADPT_TASK_LSC_REQ);
	adpt->link_jiffies = jiffies + EMAC_TRY_LINK_TIMEOUT;

	if (!TEST_FLAG(adpt, ADPT_STATE_DOWN))
		emac_task_schedule(adpt);
}

/* Respond to a TX hang */
static void emac_tx_timeout(struct net_device *netdev)
{
	struct emac_adapter *adpt = netdev_priv(netdev);

	if (!TEST_FLAG(adpt, ADPT_STATE_DOWN)) {
		SET_FLAG(adpt, ADPT_TASK_REINIT_REQ);
		emac_task_schedule(adpt);
	}
}

/* Configure Multicast and Promiscuous modes */
static void emac_set_rx_mode(struct net_device *netdev)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;
	struct netdev_hw_addr *ha;

	if (pm_runtime_status_suspended(adpt->netdev->dev.parent))
		return;
	if (TEST_FLAG(adpt, ADPT_STATE_DOWN))
		return;

	/* Check for Promiscuous and All Multicast modes */
	if (netdev->flags & IFF_PROMISC) {
		SET_FLAG(hw, HW_PROMISC_EN);
	} else if (netdev->flags & IFF_ALLMULTI) {
		SET_FLAG(hw, HW_MULTIALL_EN);
		CLR_FLAG(hw, HW_PROMISC_EN);
	} else {
		CLR_FLAG(hw, HW_MULTIALL_EN);
		CLR_FLAG(hw, HW_PROMISC_EN);
	}
	emac_hw_config_mac_ctrl(hw);

	/* update multicast address filtering */
	emac_hw_clear_mc_addr(hw);
	netdev_for_each_mc_addr(ha, netdev)
		emac_hw_set_mc_addr(hw, ha->addr);
}

/* Change MAC address */
static int emac_set_mac_address(struct net_device *netdev, void *p)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	if (netif_running(netdev))
		return -EBUSY;

	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
	memcpy(hw->mac_addr, addr->sa_data, netdev->addr_len);

	pm_runtime_get_sync(netdev->dev.parent);
	emac_hw_set_mac_addr(hw, hw->mac_addr);
	pm_runtime_mark_last_busy(netdev->dev.parent);
	pm_runtime_put_autosuspend(netdev->dev.parent);
	return 0;
}

/* Push the received skb to upper layers */
static void emac_receive_skb(struct emac_rx_queue *rxque,
			     struct sk_buff *skb,
			     u16 vlan_tag, bool vlan_flag)
{
	if (vlan_flag) {
		u16 vlan;

		EMAC_TAG_TO_VLAN(vlan_tag, vlan);
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), vlan);
	}

	napi_gro_receive(&rxque->napi, skb);
}

/* Consume next received packet descriptor */
static bool emac_get_rrdesc(struct emac_rx_queue *rxque,
			    union emac_sw_rrdesc *srrd)
{
	struct emac_adapter *adpt = netdev_priv(rxque->netdev);
	u32 *hrrd = EMAC_RRD(rxque, adpt->rrdesc_size,
			     rxque->rrd.consume_idx);

	/* If time stamping is enabled, it will be added in the beginning of
	 * the hw rrd (hrrd). In sw rrd (srrd), dwords 4 & 5 are reserved for
	 * the time stamp; hence the conversion.
	 * Also, read the rrd word with update flag first; read rest of rrd
	 * only if update flag is set.
	 */
	if (adpt->tstamp_en)
		srrd->dfmt.dw[3] = *(hrrd + 5);
	else
		srrd->dfmt.dw[3] = *(hrrd + 3);
	rmb(); /* ensure hw receive returned descriptor timestamp is read */

	if (!srrd->genr.update)
		return false;

	if (adpt->tstamp_en) {
		srrd->dfmt.dw[4] = *(hrrd++);
		srrd->dfmt.dw[5] = *(hrrd++);
	} else {
		srrd->dfmt.dw[4] = 0;
		srrd->dfmt.dw[5] = 0;
	}

	srrd->dfmt.dw[0] = *(hrrd++);
	srrd->dfmt.dw[1] = *(hrrd++);
	srrd->dfmt.dw[2] = *(hrrd++);
	mb(); /* ensure descriptor is read */

	emac_dbg(adpt, rx_status, "RX[%d]:SRRD[%x]: %x:%x:%x:%x:%x:%x\n",
		 rxque->que_idx, rxque->rrd.consume_idx, srrd->dfmt.dw[0],
		 srrd->dfmt.dw[1], srrd->dfmt.dw[2], srrd->dfmt.dw[3],
		 srrd->dfmt.dw[4], srrd->dfmt.dw[5]);

	if (unlikely(srrd->genr.nor != 1)) {
		/* multiple rfd not supported */
		emac_err(adpt, "Multi rfd not support yet! nor = %d\n",
			 srrd->genr.nor);
	}

	/* mark rrd as processed */
	srrd->genr.update = 0;
	*hrrd = srrd->dfmt.dw[3];

	if (++rxque->rrd.consume_idx == rxque->rrd.count)
		rxque->rrd.consume_idx = 0;

	return true;
}

/* Produce new receive free descriptor */
static bool emac_set_rfdesc(struct emac_rx_queue *rxque,
			    union emac_sw_rfdesc *srfd)
{
	struct emac_adapter *adpt = netdev_priv(rxque->netdev);
	u32 *hrfd = EMAC_RFD(rxque, adpt->rfdesc_size,
			     rxque->rfd.produce_idx);

	*(hrfd++) = srfd->dfmt.dw[0];
	*hrfd = srfd->dfmt.dw[1];

	if (++rxque->rfd.produce_idx == rxque->rfd.count)
		rxque->rfd.produce_idx = 0;

	return true;
}

/* Produce new transmit descriptor */
static bool emac_set_tpdesc(struct emac_tx_queue *txque,
			    union emac_sw_tpdesc *stpd)
{
	struct emac_adapter *adpt = netdev_priv(txque->netdev);
	u32 *htpd;

	txque->tpd.last_produce_idx = txque->tpd.produce_idx;
	htpd = EMAC_TPD(txque, adpt->tpdesc_size, txque->tpd.produce_idx);

	if (++txque->tpd.produce_idx == txque->tpd.count)
		txque->tpd.produce_idx = 0;

	*(htpd++) = stpd->dfmt.dw[0];
	*(htpd++) = stpd->dfmt.dw[1];
	*(htpd++) = stpd->dfmt.dw[2];
	*htpd = stpd->dfmt.dw[3];

	emac_dbg(adpt, tx_done, "TX[%d]:STPD[%x]: %x:%x:%x:%x\n",
		 txque->que_idx, txque->tpd.last_produce_idx, stpd->dfmt.dw[0],
		 stpd->dfmt.dw[1], stpd->dfmt.dw[2], stpd->dfmt.dw[3]);

	return true;
}

/* Mark the last transmit descriptor as such (for the transmit packet) */
static void emac_set_tpdesc_lastfrag(struct emac_tx_queue *txque)
{
	struct emac_adapter *adpt = netdev_priv(txque->netdev);
	u32 tmp_tpd;
	u32 *htpd = EMAC_TPD(txque, adpt->tpdesc_size,
			     txque->tpd.last_produce_idx);

	tmp_tpd = *(htpd + 1);
	tmp_tpd |= EMAC_TPD_LAST_FRAGMENT;
	*(htpd + 1) = tmp_tpd;
}

void emac_set_tpdesc_tstamp_sav(struct emac_tx_queue *txque)
{
	struct emac_adapter *adpt = netdev_priv(txque->netdev);
	u32 tmp_tpd;
	u32 *htpd = EMAC_TPD(txque, adpt->tpdesc_size,
			     txque->tpd.last_produce_idx);

	tmp_tpd = *(htpd + 3);
	tmp_tpd |= EMAC_TPD_TSTAMP_SAVE;
	*(htpd + 3) = tmp_tpd;
}

/* Fill up receive queue's RFD with preallocated receive buffers */
static int emac_refresh_rx_buffer(struct emac_rx_queue *rxque)
{
	struct emac_adapter *adpt = netdev_priv(rxque->netdev);
	struct emac_hw *hw = &adpt->hw;
	struct emac_buffer *curr_rxbuf;
	struct emac_buffer *next_rxbuf;
	union emac_sw_rfdesc srfd;
	struct sk_buff *skb;
	void *skb_data = NULL;
	u32 count = 0;
	u32 next_produce_idx;

	next_produce_idx = rxque->rfd.produce_idx;
	if (++next_produce_idx == rxque->rfd.count)
		next_produce_idx = 0;
	curr_rxbuf = GET_RFD_BUFFER(rxque, rxque->rfd.produce_idx);
	next_rxbuf = GET_RFD_BUFFER(rxque, next_produce_idx);

	/* this always has a blank rx_buffer*/
	while (next_rxbuf->dma == 0) {
		skb = dev_alloc_skb(adpt->rxbuf_size + NET_IP_ALIGN);
		if (unlikely(!skb)) {
			emac_err(adpt, "alloc rx buffer failed\n");
			break;
		}

		/* Make buffer alignment 2 beyond a 16 byte boundary
		 * this will result in a 16 byte aligned IP header after
		 * the 14 byte MAC header is removed
		 */
		skb_reserve(skb, NET_IP_ALIGN);
		skb_data = skb->data;
		curr_rxbuf->skb = skb;
		curr_rxbuf->length = adpt->rxbuf_size;
		curr_rxbuf->dma = dma_map_single(rxque->dev, skb_data,
						 curr_rxbuf->length,
						 DMA_FROM_DEVICE);
		srfd.genr.addr = curr_rxbuf->dma;
		emac_set_rfdesc(rxque, &srfd);
		next_produce_idx = rxque->rfd.produce_idx;
		if (++next_produce_idx == rxque->rfd.count)
			next_produce_idx = 0;

		curr_rxbuf = GET_RFD_BUFFER(rxque, rxque->rfd.produce_idx);
		next_rxbuf = GET_RFD_BUFFER(rxque, next_produce_idx);
		count++;
	}

	if (count) {
		u32 prod_idx = (rxque->rfd.produce_idx << rxque->produce_shft) &
				rxque->produce_mask;
		wmb(); /* ensure that the descriptors are properly set */
		emac_reg_update32(hw, EMAC, rxque->produce_reg,
				  rxque->produce_mask, prod_idx);
		wmb(); /* ensure that the producer's index is flushed to HW */
		emac_dbg(adpt, rx_status, "RX[%d]: prod idx 0x%x\n",
			 rxque->que_idx, rxque->rfd.produce_idx);
	}

	return count;
}

static void emac_clean_rfdesc(struct emac_rx_queue *rxque,
			      union emac_sw_rrdesc *srrd)
{
	struct emac_buffer *rfbuf = rxque->rfd.rfbuff;
	u32 consume_idx = srrd->genr.si;
	u16 i;

	for (i = 0; i < srrd->genr.nor; i++) {
		rfbuf[consume_idx].skb = NULL;
		if (++consume_idx == rxque->rfd.count)
			consume_idx = 0;
	}

	rxque->rfd.consume_idx = consume_idx;
	rxque->rfd.process_idx = consume_idx;
}

static inline bool emac_skb_cb_expired(struct sk_buff *skb)
{
	if (time_is_after_jiffies(EMAC_SKB_CB(skb)->jiffies +
				  msecs_to_jiffies(100)))
		return false;
	return true;
}

/* proper lock must be acquired before polling */
static void emac_poll_hwtxtstamp(struct emac_adapter *adpt)
{
	struct sk_buff_head *pending_q = &adpt->hwtxtstamp_pending_queue;
	struct sk_buff_head *q = &adpt->hwtxtstamp_ready_queue;
	struct sk_buff *skb, *skb_tmp;
	struct emac_hwtxtstamp hwtxtstamp;

	while (emac_hw_read_tx_tstamp(&adpt->hw, &hwtxtstamp)) {
		bool found = false;

		adpt->hwtxtstamp_stats.rx++;

		skb_queue_walk_safe(pending_q, skb, skb_tmp) {
			if (EMAC_SKB_CB(skb)->tpd_idx == hwtxtstamp.ts_idx) {
				struct sk_buff *pskb;

				EMAC_HWTXTSTAMP_CB(skb)->sec = hwtxtstamp.sec;
				EMAC_HWTXTSTAMP_CB(skb)->ns = hwtxtstamp.ns;
				/* the tx timestamps for all the pending
				 * packets before this one are lost
				 */
				while ((pskb = __skb_dequeue(pending_q))
				       != skb) {
					if (!pskb)
						break;
					EMAC_HWTXTSTAMP_CB(pskb)->sec = 0;
					EMAC_HWTXTSTAMP_CB(pskb)->ns = 0;
					__skb_queue_tail(q, pskb);
					adpt->hwtxtstamp_stats.lost++;
				}
				__skb_queue_tail(q, skb);
				found = true;
				break;
			}
		}

		if (!found) {
			emac_dbg(adpt, tx_done,
				 "no entry(tpd=%d) found, drop tx timestamp\n",
				 hwtxtstamp.ts_idx);
			adpt->hwtxtstamp_stats.drop++;
		}
	}

	skb_queue_walk_safe(pending_q, skb, skb_tmp) {
		/* No packet after this one expires */
		if (!emac_skb_cb_expired(skb))
			break;
		adpt->hwtxtstamp_stats.timeout++;
		emac_dbg(adpt, tx_done,
			 "tx timestamp timeout: tpd_idx=%d\n",
			 EMAC_SKB_CB(skb)->tpd_idx);

		__skb_unlink(skb, pending_q);
		EMAC_HWTXTSTAMP_CB(skb)->sec = 0;
		EMAC_HWTXTSTAMP_CB(skb)->ns = 0;
		__skb_queue_tail(q, skb);
	}
}

static void emac_schedule_hwtxtstamp_task(struct emac_adapter *adpt)
{
	if (TEST_FLAG(adpt, ADPT_STATE_DOWN))
		return;

	if (schedule_work(&adpt->hwtxtstamp_task))
		adpt->hwtxtstamp_stats.sched++;
}

static void emac_hwtxtstamp_task_routine(struct work_struct *work)
{
	struct emac_adapter *adpt = container_of(work, struct emac_adapter,
						 hwtxtstamp_task);
	struct sk_buff *skb;
	struct sk_buff_head q;
	unsigned long flags;

	adpt->hwtxtstamp_stats.poll++;

	__skb_queue_head_init(&q);

	while (1) {
		spin_lock_irqsave(&adpt->hwtxtstamp_lock, flags);
		if (adpt->hwtxtstamp_pending_queue.qlen)
			emac_poll_hwtxtstamp(adpt);
		skb_queue_splice_tail_init(&adpt->hwtxtstamp_ready_queue, &q);
		spin_unlock_irqrestore(&adpt->hwtxtstamp_lock, flags);

		if (!q.qlen)
			break;

		while ((skb = __skb_dequeue(&q))) {
			struct emac_hwtxtstamp_cb *cb = EMAC_HWTXTSTAMP_CB(skb);

			if (cb->sec || cb->ns) {
				struct skb_shared_hwtstamps ts;

				ts.hwtstamp = ktime_set(cb->sec, cb->ns);
				skb_tstamp_tx(skb, &ts);
				adpt->hwtxtstamp_stats.deliver++;
			}
			dev_kfree_skb_any(skb);
		}
	}

	if (adpt->hwtxtstamp_pending_queue.qlen)
		emac_schedule_hwtxtstamp_task(adpt);
}

/* Process receive event */
static void emac_handle_rx(struct emac_adapter *adpt,
			   struct emac_rx_queue *rxque,
			   int *num_pkts, int max_pkts)
{
	struct emac_hw *hw = &adpt->hw;
	struct net_device *netdev  = adpt->netdev;

	union emac_sw_rrdesc srrd;
	struct emac_buffer *rfbuf;
	struct sk_buff *skb;

	u32 hw_consume_idx, num_consume_pkts;
	u32 count = 0;
	u32 proc_idx;

	hw_consume_idx = emac_reg_field_r32(hw, EMAC, rxque->consume_reg,
					    rxque->consume_mask,
					    rxque->consume_shft);
	num_consume_pkts = (hw_consume_idx >= rxque->rrd.consume_idx) ?
		(hw_consume_idx -  rxque->rrd.consume_idx) :
		(hw_consume_idx + rxque->rrd.count - rxque->rrd.consume_idx);

	while (1) {
		if (!num_consume_pkts)
			break;

		if (!emac_get_rrdesc(rxque, &srrd))
			break;

		if (likely(srrd.genr.nor == 1)) {
			/* good receive */
			rfbuf = GET_RFD_BUFFER(rxque, srrd.genr.si);
			dma_unmap_single(rxque->dev, rfbuf->dma, rfbuf->length,
					 DMA_FROM_DEVICE);
			rfbuf->dma = 0;
			skb = rfbuf->skb;
		} else {
			/* multi rfd not supported */
			emac_err(adpt, "Multi rfd not support yet!\n");
			break;
		}
		emac_clean_rfdesc(rxque, &srrd);
		num_consume_pkts--;
		count++;

		/* Due to a HW issue in L4 check sum detection (UDP/TCP frags
		 * with DF set are marked as error), drop packets based on the
		 * error mask rather than the summary bit (ignoring L4F errors)
		 */
		if (srrd.dfmt.dw[EMAC_RRDES_STATS_DW_IDX] & EMAC_RRDES_ERROR) {
			emac_dbg(adpt, rx_status,
				 "Drop error packet[RRD: 0x%x:0x%x:0x%x:0x%x]\n",
				 srrd.dfmt.dw[0], srrd.dfmt.dw[1],
				 srrd.dfmt.dw[2], srrd.dfmt.dw[3]);

			dev_kfree_skb(skb);
			continue;
		}

		skb_put(skb, srrd.genr.pkt_len - ETH_FCS_LEN);
		skb->dev = netdev;
		skb->protocol = eth_type_trans(skb, skb->dev);
		if (netdev->features & NETIF_F_RXCSUM)
			skb->ip_summed = ((srrd.genr.l4f) ?
					  CHECKSUM_NONE : CHECKSUM_UNNECESSARY);
		else
			skb_checksum_none_assert(skb);

		if (TEST_FLAG(hw, HW_TS_RX_EN)) {
			struct skb_shared_hwtstamps *hwts = skb_hwtstamps(skb);

			hwts->hwtstamp = ktime_set(srrd.genr.ts_high,
						   srrd.genr.ts_low);
		}

		emac_receive_skb(rxque, skb, (u16)srrd.genr.cvlan_tag,
				 (bool)srrd.genr.cvlan_flag);

		netdev->last_rx = jiffies;
		(*num_pkts)++;
		if (*num_pkts >= max_pkts)
			break;
	}

	if (count) {
		proc_idx = (rxque->rfd.process_idx << rxque->process_shft) &
				rxque->process_mask;
		wmb(); /* ensure that the descriptors are properly cleared */
		emac_reg_update32(hw, EMAC, rxque->process_reg,
				  rxque->process_mask, proc_idx);
		wmb(); /* ensure that RFD producer index is flushed to HW */
		emac_dbg(adpt, rx_status, "RX[%d]: proc idx 0x%x\n",
			 rxque->que_idx, rxque->rfd.process_idx);

		emac_refresh_rx_buffer(rxque);
	}
}

/* get the number of free transmit descriptors */
static u32 emac_get_num_free_tpdescs(struct emac_tx_queue *txque)
{
	u32 produce_idx = txque->tpd.produce_idx;
	u32 consume_idx = txque->tpd.consume_idx;

	return (consume_idx > produce_idx) ?
		(consume_idx - produce_idx - 1) :
		(txque->tpd.count + consume_idx - produce_idx - 1);
}

/* Process transmit event */
static void emac_handle_tx(struct emac_adapter *adpt,
			   struct emac_tx_queue *txque)
{
	struct emac_hw *hw = &adpt->hw;
	struct emac_buffer *tpbuf;
	u32 hw_consume_idx;
	u32 pkts_compl = 0, bytes_compl = 0;

	hw_consume_idx = emac_reg_field_r32(hw, EMAC, txque->consume_reg,
					    txque->consume_mask,
					    txque->consume_shft);
	emac_dbg(adpt, tx_done, "TX[%d]: cons idx 0x%x\n",
		 txque->que_idx, hw_consume_idx);

	while (txque->tpd.consume_idx != hw_consume_idx) {
		tpbuf = GET_TPD_BUFFER(txque, txque->tpd.consume_idx);
		if (tpbuf->dma) {
			dma_unmap_single(txque->dev, tpbuf->dma, tpbuf->length,
					 DMA_TO_DEVICE);
			tpbuf->dma = 0;
		}

		if (tpbuf->skb) {
			pkts_compl++;
			bytes_compl += tpbuf->skb->len;
			dev_kfree_skb_irq(tpbuf->skb);
			tpbuf->skb = NULL;
		}

		if (++txque->tpd.consume_idx == txque->tpd.count)
			txque->tpd.consume_idx = 0;
	}

	if (pkts_compl || bytes_compl)
		netdev_completed_queue(adpt->netdev, pkts_compl, bytes_compl);
}

/* NAPI */
static int emac_napi_rtx(struct napi_struct *napi, int budget)
{
	struct emac_rx_queue *rxque = container_of(napi, struct emac_rx_queue,
						   napi);
	struct emac_adapter *adpt = netdev_priv(rxque->netdev);
	struct emac_irq_per_dev *irq = rxque->irq;
	struct emac_hw *hw = &adpt->hw;
	int work_done = 0;

	/* Keep link state information with original netdev */
	if (!netif_carrier_ok(adpt->netdev))
		goto quit_polling;

	emac_handle_rx(adpt, rxque, &work_done, budget);

	if (work_done < budget) {
quit_polling:
		napi_complete(napi);

		irq->mask |= rxque->intr;
		emac_reg_w32(hw, EMAC, emac_irq_cmn_tbl[irq->idx].mask_reg,
			     irq->mask);
		wmb(); /* ensure that interrupt enable is flushed to HW */
	}

	return work_done;
}

/* Check if enough transmit descriptors are available */
static bool emac_check_num_tpdescs(struct emac_tx_queue *txque,
				   const struct sk_buff *skb)
{
	u32 num_required = 1;
	u16 i;
	u16 proto_hdr_len = 0;

	if (skb_is_gso(skb)) {
		proto_hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);
		if (proto_hdr_len < skb_headlen(skb))
			num_required++;
		if (skb_shinfo(skb)->gso_type & SKB_GSO_TCPV6)
			num_required++;
	}

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++)
		num_required++;

	return num_required < emac_get_num_free_tpdescs(txque);
}

/* Fill up transmit descriptors with TSO and Checksum offload information */
static int emac_tso_csum(struct emac_adapter *adpt,
			 struct emac_tx_queue *txque,
			 struct sk_buff *skb,
			 union emac_sw_tpdesc *stpd)
{
	u8  hdr_len;
	int retval;

	if (skb_is_gso(skb)) {
		if (skb_header_cloned(skb)) {
			retval = pskb_expand_head(skb, 0, 0, GFP_ATOMIC);
			if (unlikely(retval))
				return retval;
		}

		if (skb->protocol == htons(ETH_P_IP)) {
			u32 pkt_len =
				((unsigned char *)ip_hdr(skb) - skb->data) +
				ntohs(ip_hdr(skb)->tot_len);
			if (skb->len > pkt_len)
				pskb_trim(skb, pkt_len);
		}

		hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);
		if (unlikely(skb->len == hdr_len)) {
			/* we only need to do csum */
			emac_warn(adpt, tx_err,
				  "tso not needed for packet with 0 data\n");
			goto do_csum;
		}

		if (skb_shinfo(skb)->gso_type & SKB_GSO_TCPV4) {
			ip_hdr(skb)->check = 0;
			tcp_hdr(skb)->check = ~csum_tcpudp_magic(
						ip_hdr(skb)->saddr,
						ip_hdr(skb)->daddr,
						0, IPPROTO_TCP, 0);
			stpd->genr.ipv4 = 1;
		}

		if (skb_shinfo(skb)->gso_type & SKB_GSO_TCPV6) {
			/* ipv6 tso need an extra tpd */
			union emac_sw_tpdesc extra_tpd;

			memset(stpd, 0, sizeof(union emac_sw_tpdesc));
			memset(&extra_tpd, 0, sizeof(union emac_sw_tpdesc));

			ipv6_hdr(skb)->payload_len = 0;
			tcp_hdr(skb)->check = ~csum_ipv6_magic(
						&ipv6_hdr(skb)->saddr,
						&ipv6_hdr(skb)->daddr,
						0, IPPROTO_TCP, 0);
			extra_tpd.tso.pkt_len = skb->len;
			extra_tpd.tso.lso = 0x1;
			extra_tpd.tso.lso_v2 = 0x1;
			emac_set_tpdesc(txque, &extra_tpd);
			stpd->tso.lso_v2 = 0x1;
		}

		stpd->tso.lso = 0x1;
		stpd->tso.tcphdr_offset = skb_transport_offset(skb);
		stpd->tso.mss = skb_shinfo(skb)->gso_size;
		return 0;
	}

do_csum:
	if (likely(skb->ip_summed == CHECKSUM_PARTIAL)) {
		u8 css, cso;

		cso = skb_transport_offset(skb);
		if (unlikely(cso & 0x1)) {
			emac_err(adpt, "payload offset should be even\n");
			return -EINVAL;
		}
		css = cso + skb->csum_offset;

		stpd->csum.payld_offset = cso >> 1;
		stpd->csum.cxsum_offset = css >> 1;
		stpd->csum.c_csum = 0x1;
	}

	return 0;
}

/* Fill up transmit descriptors */
static void emac_tx_map(struct emac_adapter *adpt,
			struct emac_tx_queue *txque,
			struct sk_buff *skb,
			union emac_sw_tpdesc *stpd)
{
	struct emac_hw  *hw = &adpt->hw;
	struct emac_buffer *tpbuf = NULL;
	u16 nr_frags = skb_shinfo(skb)->nr_frags;
	u32 len = skb_headlen(skb);
	u16 map_len = 0;
	u16 mapped_len = 0;
	u16 hdr_len = 0;
	u16 i;
	u32 tso = stpd->tso.lso;

	if (tso) {
		hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);
		map_len = hdr_len;

		tpbuf = GET_TPD_BUFFER(txque, txque->tpd.produce_idx);
		tpbuf->length = map_len;
		tpbuf->dma = dma_map_single(txque->dev,	skb->data,
					    hdr_len, DMA_TO_DEVICE);
		mapped_len += map_len;
		stpd->genr.addr_lo = EMAC_DMA_ADDR_LO(tpbuf->dma);
		stpd->genr.addr_hi = EMAC_DMA_ADDR_HI(tpbuf->dma);
		stpd->genr.buffer_len = tpbuf->length;
		emac_set_tpdesc(txque, stpd);
	}

	if (mapped_len < len) {
		tpbuf = GET_TPD_BUFFER(txque, txque->tpd.produce_idx);
		tpbuf->length = len - mapped_len;
		tpbuf->dma = dma_map_single(txque->dev, skb->data + mapped_len,
					    tpbuf->length, DMA_TO_DEVICE);
		stpd->genr.addr_lo = EMAC_DMA_ADDR_LO(tpbuf->dma);
		stpd->genr.addr_hi = EMAC_DMA_ADDR_HI(tpbuf->dma);
		stpd->genr.buffer_len  = tpbuf->length;
		emac_set_tpdesc(txque, stpd);
	}

	for (i = 0; i < nr_frags; i++) {
		struct skb_frag_struct *frag;

		frag = &skb_shinfo(skb)->frags[i];

		tpbuf = GET_TPD_BUFFER(txque, txque->tpd.produce_idx);
		tpbuf->length = frag->size;
		tpbuf->dma = dma_map_page(txque->dev, frag->page.p,
					  frag->page_offset,
					  tpbuf->length,
					  DMA_TO_DEVICE);
		stpd->genr.addr_lo = EMAC_DMA_ADDR_LO(tpbuf->dma);
		stpd->genr.addr_hi = EMAC_DMA_ADDR_HI(tpbuf->dma);
		stpd->genr.buffer_len  = tpbuf->length;
		emac_set_tpdesc(txque, stpd);
	}

	/* The last tpd */
	emac_set_tpdesc_lastfrag(txque);

	if (TEST_FLAG(hw, HW_TS_TX_EN) &&
	    (skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP)) {
		struct sk_buff *skb_ts = skb_clone(skb, GFP_ATOMIC);

		if (likely(skb_ts)) {
			unsigned long flags;

			emac_set_tpdesc_tstamp_sav(txque);
			skb_ts->sk = skb->sk;
			EMAC_SKB_CB(skb_ts)->tpd_idx =
				txque->tpd.last_produce_idx;
			EMAC_SKB_CB(skb_ts)->jiffies = get_jiffies_64();
			skb_shinfo(skb_ts)->tx_flags |= SKBTX_IN_PROGRESS;
			spin_lock_irqsave(&adpt->hwtxtstamp_lock, flags);
			if (adpt->hwtxtstamp_pending_queue.qlen >=
			    EMAC_TX_POLL_HWTXTSTAMP_THRESHOLD) {
				emac_poll_hwtxtstamp(adpt);
				adpt->hwtxtstamp_stats.tx_poll++;
			}
			__skb_queue_tail(&adpt->hwtxtstamp_pending_queue,
					 skb_ts);
			spin_unlock_irqrestore(&adpt->hwtxtstamp_lock, flags);
			adpt->hwtxtstamp_stats.tx++;
			emac_schedule_hwtxtstamp_task(adpt);
		}
	}

	/* The last buffer info contain the skb address,
	 * so it will be freed after unmap
	 */
	if (tpbuf)
		tpbuf->skb = skb;
}

/* Transmit the packet using specified transmit queue */
static int emac_start_xmit_frame(struct emac_adapter *adpt,
				 struct emac_tx_queue *txque,
				 struct sk_buff *skb)
{
	struct emac_hw  *hw = &adpt->hw;
	union emac_sw_tpdesc stpd;
	u32 prod_idx;

	if (TEST_FLAG(adpt, ADPT_STATE_DOWN)) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if (!emac_check_num_tpdescs(txque, skb)) {
		/* not enough descriptors, just stop queue */
		netif_stop_queue(adpt->netdev);
		return NETDEV_TX_BUSY;
	}

	memset(&stpd, 0, sizeof(union emac_sw_tpdesc));

	if (emac_tso_csum(adpt, txque, skb, &stpd) != 0) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if (vlan_tx_tag_present(skb)) {
		u16 vlan = vlan_tx_tag_get(skb);
		u16 tag;

		EMAC_VLAN_TO_TAG(vlan, tag);
		stpd.genr.cvlan_tag = tag;
		stpd.genr.ins_cvtag = 0x1;
	}

	if (skb_network_offset(skb) != ETH_HLEN)
		stpd.genr.type = 0x1;

	emac_tx_map(adpt, txque, skb, &stpd);

	netdev_sent_queue(adpt->netdev, skb->len);

	/* update produce idx */
	prod_idx = (txque->tpd.produce_idx << txque->produce_shft) &
			txque->produce_mask;
	wmb(); /* ensure that the descriptors are properly set */
	emac_reg_update32(hw, EMAC, txque->produce_reg,
			  txque->produce_mask, prod_idx);
	wmb(); /* ensure that RFD producer index is flushed to HW */
	emac_dbg(adpt, tx_queued, "TX[%d]: prod idx 0x%x\n",
		 txque->que_idx, txque->tpd.produce_idx);

	return NETDEV_TX_OK;
}

/* Transmit the packet */
static int emac_start_xmit(struct sk_buff *skb,
			   struct net_device *netdev)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_tx_queue *txque;

	txque = &adpt->tx_queue[EMAC_ACTIVE_TXQ];
	return emac_start_xmit_frame(adpt, txque, skb);
}

/* This funciton aquire spin-lock so should not call from sleeping context */
static void emac_wol_gpio_irq(struct emac_adapter *adpt, bool enable)
{
	struct emac_irq_per_dev *wol_irq = &adpt->irq[EMAC_WOL_IRQ];
	unsigned long flags;

	spin_lock_irqsave(&adpt->wol_irq_lock, flags);
	if (enable && !adpt->is_wol_enabled)
		enable_irq(wol_irq->irq);
	else if (!enable && adpt->is_wol_enabled)
		disable_irq_nosync(wol_irq->irq);
	adpt->is_wol_enabled = enable;
	spin_unlock_irqrestore(&adpt->wol_irq_lock, flags);
}

/* ISR */
static irqreturn_t emac_wol_isr(int irq, void *data)
{
	struct emac_adapter *adpt = emac_irq_get_adpt(data);
	struct emac_phy *phy = &adpt->phy;
	struct net_device *netdev = adpt->netdev;
	int ret = 0;
	u16 val = 0;

	pm_runtime_get_sync(netdev->dev.parent);
	ret = emac_phy_read(adpt, phy->addr, MII_INT_STATUS, &val);
	pm_runtime_mark_last_busy(netdev->dev.parent);
	pm_runtime_put_autosuspend(netdev->dev.parent);

	if (!pm_runtime_status_suspended(adpt->netdev->dev.parent)) {
		if (!ret &&
		    ((val & LINK_SUCCESS_INTERRUPT) || (val & LINK_SUCCESS_BX)))
			emac_wol_gpio_irq(adpt, false);
	}
	return IRQ_HANDLED;
}

static irqreturn_t emac_isr(int _irq, void *data)
{
	struct emac_irq_per_dev *irq = data;
	const struct emac_irq_common *irq_cmn = &emac_irq_cmn_tbl[irq->idx];
	struct emac_adapter *adpt = emac_irq_get_adpt(data);
	struct emac_rx_queue *rxque = &adpt->rx_queue[irq->idx];
	struct emac_hw *hw = &adpt->hw;
	int max_ints = 1;
	u32 isr, status;

	emac_dbg(emac_irq_get_adpt(data), wol, "EMAC wol interrupt received\n");
	/* disable the interrupt */
	emac_reg_w32(hw, EMAC, irq_cmn->mask_reg, 0);
	wmb(); /* ensure that interrupt disable is flushed to HW */

	do {
		isr = emac_reg_r32(hw, EMAC, irq_cmn->status_reg);
		status = isr & irq->mask;

		if (status == 0)
			break;

		if (status & ISR_ERROR) {
			emac_warn(adpt, intr, "isr error status 0x%x\n",
				  status & ISR_ERROR);
			/* reset MAC */
			SET_FLAG(adpt, ADPT_TASK_REINIT_REQ);
			emac_task_schedule(adpt);
		}

		/* Schedule the napi for receive queue with interrupt
		 * status bit set
		 */
		if ((status & rxque->intr)) {
			if (napi_schedule_prep(&rxque->napi)) {
				irq->mask &= ~rxque->intr;
				__napi_schedule(&rxque->napi);
			}
		}

		if (status & ISR_TX_PKT) {
			if (status & TX_PKT_INT)
				emac_handle_tx(adpt, &adpt->tx_queue[0]);
			if (status & TX_PKT_INT1)
				emac_handle_tx(adpt, &adpt->tx_queue[1]);
			if (status & TX_PKT_INT2)
				emac_handle_tx(adpt, &adpt->tx_queue[2]);
			if (status & TX_PKT_INT3)
				emac_handle_tx(adpt, &adpt->tx_queue[3]);
		}

		if (status & ISR_OVER)
			emac_warn(adpt, intr, "TX/RX overflow status 0x%x\n",
				  status & ISR_OVER);

		/* link event */
		if (status & (ISR_GPHY_LINK | SW_MAN_INT)) {
			emac_check_lsc(adpt);
			break;
		}

		if (status & PTP_INT)
			emac_ptp_intr(hw);
	} while (--max_ints > 0);

	/* enable the interrupt */
	emac_reg_w32(hw, EMAC, irq_cmn->mask_reg, irq->mask);
	wmb(); /* ensure that interrupt enable is flushed to HW */
	return IRQ_HANDLED;
}

/* Enable interrupts */
static inline void emac_enable_intr(struct emac_adapter *adpt)
{
	struct emac_hw *hw = &adpt->hw;

	emac_hw_enable_intr(hw);
}

/* Disable interrupts */
static inline void emac_disable_intr(struct emac_adapter *adpt)
{
	struct emac_hw *hw = &adpt->hw;
	int i;

	emac_hw_disable_intr(hw);
	for (i = 0; i < EMAC_NUM_CORE_IRQ; i++)
		if (adpt->irq[i].irq)
			synchronize_irq(adpt->irq[i].irq);
}

/* Configure VLAN tag strip/insert feature */
static int emac_set_features(struct net_device *netdev,
			     netdev_features_t features)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;
	netdev_features_t changed = features ^ netdev->features;

	if (!(changed & (NETIF_F_HW_VLAN_CTAG_TX | NETIF_F_HW_VLAN_CTAG_RX)))
		return 0;

	netdev->features = features;
	if (netdev->features & NETIF_F_HW_VLAN_CTAG_RX)
		SET_FLAG(hw, HW_VLANSTRIP_EN);
	else
		CLR_FLAG(hw, HW_VLANSTRIP_EN);

	if (netif_running(netdev))
		emac_reinit_locked(adpt);

	return 0;
}

static void emac_napi_enable_all(struct emac_adapter *adpt)
{
	u8 i;

	for (i = 0; i < adpt->num_rxques; i++)
		napi_enable(&adpt->rx_queue[i].napi);
}

static void emac_napi_disable_all(struct emac_adapter *adpt)
{
	u8 i;

	for (i = 0; i < adpt->num_rxques; i++)
		napi_disable(&adpt->rx_queue[i].napi);
}

/* Free all descriptors of given transmit queue */
static void emac_clean_tx_queue(struct emac_tx_queue *txque)
{
	struct device *dev = txque->dev;
	unsigned long size;
	u32 i;

	/* ring already cleared, nothing to do */
	if (!txque->tpd.tpbuff)
		return;

	for (i = 0; i < txque->tpd.count; i++) {
		struct emac_buffer *tpbuf = GET_TPD_BUFFER(txque, i);

		if (tpbuf->dma) {
			dma_unmap_single(dev, tpbuf->dma, tpbuf->length,
					 DMA_TO_DEVICE);
			tpbuf->dma = 0;
		}
		if (tpbuf->skb) {
			dev_kfree_skb_any(tpbuf->skb);
			tpbuf->skb = NULL;
		}
	}

	size = sizeof(struct emac_buffer) * txque->tpd.count;
	memset(txque->tpd.tpbuff, 0, size);

	/* clear the descriptor ring */
	memset(txque->tpd.tpdesc, 0, txque->tpd.size);

	txque->tpd.consume_idx = 0;
	txque->tpd.produce_idx = 0;
}

static void emac_clean_all_tx_queues(struct emac_adapter *adpt)
{
	u8 i;

	for (i = 0; i < adpt->num_txques; i++)
		emac_clean_tx_queue(&adpt->tx_queue[i]);
	netdev_reset_queue(adpt->netdev);
}

/* Free all descriptors of given receive queue */
static void emac_clean_rx_queue(struct emac_rx_queue *rxque)
{
	struct device *dev = rxque->dev;
	unsigned long size;
	u32 i;

	/* ring already cleared, nothing to do */
	if (!rxque->rfd.rfbuff)
		return;

	for (i = 0; i < rxque->rfd.count; i++) {
		struct emac_buffer *rfbuf = GET_RFD_BUFFER(rxque, i);

		if (rfbuf->dma) {
			dma_unmap_single(dev, rfbuf->dma, rfbuf->length,
					 DMA_FROM_DEVICE);
			rfbuf->dma = 0;
		}
		if (rfbuf->skb) {
			dev_kfree_skb(rfbuf->skb);
			rfbuf->skb = NULL;
		}
	}

	size =  sizeof(struct emac_buffer) * rxque->rfd.count;
	memset(rxque->rfd.rfbuff, 0, size);

	/* clear the descriptor rings */
	memset(rxque->rrd.rrdesc, 0, rxque->rrd.size);
	rxque->rrd.produce_idx = 0;
	rxque->rrd.consume_idx = 0;

	memset(rxque->rfd.rfdesc, 0, rxque->rfd.size);
	rxque->rfd.produce_idx = 0;
	rxque->rfd.consume_idx = 0;
}

static void emac_clean_all_rx_queues(struct emac_adapter *adpt)
{
	u8 i;

	for (i = 0; i < adpt->num_rxques; i++)
		emac_clean_rx_queue(&adpt->rx_queue[i]);
}

/* Free all buffers associated with given transmit queue */
static void emac_free_tx_descriptor(struct emac_tx_queue *txque)
{
	emac_clean_tx_queue(txque);

	kfree(txque->tpd.tpbuff);
	txque->tpd.tpbuff = NULL;
	txque->tpd.tpdesc = NULL;
	txque->tpd.tpdma = 0;
	txque->tpd.size = 0;
}

static void emac_free_all_tx_descriptor(struct emac_adapter *adpt)
{
	u8 i;

	for (i = 0; i < adpt->num_txques; i++)
		emac_free_tx_descriptor(&adpt->tx_queue[i]);
}

/* Allocate TX descriptor ring for the given transmit queue */
static int emac_alloc_tx_descriptor(struct emac_adapter *adpt,
				    struct emac_tx_queue *txque)
{
	struct emac_ring_header *ring_header = &adpt->ring_header;
	unsigned long size;

	size = sizeof(struct emac_buffer) * txque->tpd.count;
	txque->tpd.tpbuff = kzalloc(size, GFP_KERNEL);
	if (!txque->tpd.tpbuff)
		goto err_alloc_tpq_buffer;

	txque->tpd.size = txque->tpd.count * (adpt->tpdesc_size * 4);
	txque->tpd.tpdma = ring_header->dma + ring_header->used;
	txque->tpd.tpdesc = ring_header->desc + ring_header->used;
	ring_header->used += ALIGN(txque->tpd.size, 8);
	txque->tpd.produce_idx = 0;
	txque->tpd.consume_idx = 0;
	return 0;

err_alloc_tpq_buffer:
	emac_err(adpt, "Unable to allocate memory for the Tx descriptor\n");
	return -ENOMEM;
}

static int emac_alloc_all_tx_descriptor(struct emac_adapter *adpt)
{
	int retval = 0;
	u8 i;

	for (i = 0; i < adpt->num_txques; i++) {
		retval = emac_alloc_tx_descriptor(adpt, &adpt->tx_queue[i]);
		if (retval)
			break;
	}

	if (retval) {
		emac_err(adpt, "Allocation for Tx Queue %u failed\n", i);
		for (i--; i > 0; i--)
			emac_free_tx_descriptor(&adpt->tx_queue[i]);
	}

	return retval;
}

/* Free all buffers associated with given transmit queue */
static void emac_free_rx_descriptor(struct emac_rx_queue *rxque)
{
	emac_clean_rx_queue(rxque);

	kfree(rxque->rfd.rfbuff);
	rxque->rfd.rfbuff = NULL;

	rxque->rfd.rfdesc = NULL;
	rxque->rfd.rfdma  = 0;
	rxque->rfd.size   = 0;

	rxque->rrd.rrdesc = NULL;
	rxque->rrd.rrdma  = 0;
	rxque->rrd.size   = 0;
}

static void emac_free_all_rx_descriptor(struct emac_adapter *adpt)
{
	u8 i;

	for (i = 0; i < adpt->num_rxques; i++)
		emac_free_rx_descriptor(&adpt->rx_queue[i]);
}

/* Allocate RX descriptor rings for the given receive queue */
static int emac_alloc_rx_descriptor(struct emac_adapter *adpt,
				    struct emac_rx_queue *rxque)
{
	struct emac_ring_header *ring_header = &adpt->ring_header;
	unsigned long size;

	size = sizeof(struct emac_buffer) * rxque->rfd.count;
	rxque->rfd.rfbuff = kzalloc(size, GFP_KERNEL);
	if (!rxque->rfd.rfbuff)
		goto err_alloc_rfq_buffer;

	rxque->rrd.size = rxque->rrd.count * (adpt->rrdesc_size * 4);
	rxque->rfd.size = rxque->rfd.count * (adpt->rfdesc_size * 4);

	rxque->rrd.rrdma = ring_header->dma + ring_header->used;
	rxque->rrd.rrdesc = ring_header->desc + ring_header->used;
	ring_header->used += ALIGN(rxque->rrd.size, 8);

	rxque->rfd.rfdma = ring_header->dma + ring_header->used;
	rxque->rfd.rfdesc = ring_header->desc + ring_header->used;
	ring_header->used += ALIGN(rxque->rfd.size, 8);

	rxque->rrd.produce_idx = 0;
	rxque->rrd.consume_idx = 0;

	rxque->rfd.produce_idx = 0;
	rxque->rfd.consume_idx = 0;

	return 0;

err_alloc_rfq_buffer:
	emac_err(adpt, "Unable to allocate memory for the Rx descriptor\n");
	return -ENOMEM;
}

static int emac_alloc_all_rx_descriptor(struct emac_adapter *adpt)
{
	int retval = 0;
	u8 i;

	for (i = 0; i < adpt->num_rxques; i++) {
		retval = emac_alloc_rx_descriptor(adpt, &adpt->rx_queue[i]);
		if (retval)
			break;
	}

	if (retval) {
		emac_err(adpt, "Allocation for Rx Queue %u failed\n", i);
		for (i--; i > 0; i--)
			emac_free_rx_descriptor(&adpt->rx_queue[i]);
	}

	return retval;
}

/* Allocate all TX and RX descriptor rings */
static int emac_alloc_all_rtx_descriptor(struct emac_adapter *adpt)
{
	struct emac_ring_header *ring_header = &adpt->ring_header;
	int num_tques = adpt->num_txques;
	int num_rques = adpt->num_rxques;
	unsigned int num_tx_descs = adpt->num_txdescs;
	unsigned int num_rx_descs = adpt->num_rxdescs;
	struct device *dev = adpt->rx_queue[0].dev;
	int retval, que_idx;

	for (que_idx = 0; que_idx < adpt->num_txques; que_idx++)
		adpt->tx_queue[que_idx].tpd.count = adpt->num_txdescs;

	for (que_idx = 0; que_idx < adpt->num_rxques; que_idx++) {
		adpt->rx_queue[que_idx].rrd.count = adpt->num_rxdescs;
		adpt->rx_queue[que_idx].rfd.count = adpt->num_rxdescs;
	}

	/* Ring DMA buffer. Each ring may need up to 8 bytes for alignment,
	 * hence the additional padding bytes are allocated.
	 */
	ring_header->size =
		num_tques * num_tx_descs * (adpt->tpdesc_size * 4) +
		num_rques * num_rx_descs * (adpt->rfdesc_size * 4) +
		num_rques * num_rx_descs * (adpt->rrdesc_size * 4) +
		num_tques * 8 + num_rques * 2 * 8;

	emac_info(adpt, ifup, "TX queues %d, TX descriptors %d\n",
		  num_tques, num_tx_descs);
	emac_info(adpt, ifup, "RX queues %d, Rx descriptors %d\n",
		  num_rques, num_rx_descs);

	ring_header->used = 0;
	ring_header->desc = dma_alloc_coherent(dev, ring_header->size,
					&ring_header->dma, GFP_KERNEL);
	if (!ring_header->desc) {
		emac_err(adpt, "dma_alloc_coherent failed\n");
		retval = -ENOMEM;
		goto err_alloc_dma;
	}
	memset(ring_header->desc, 0, ring_header->size);
	ring_header->used = ALIGN(ring_header->dma, 8) - ring_header->dma;

	retval = emac_alloc_all_tx_descriptor(adpt);
	if (retval)
		goto err_alloc_tx;

	retval = emac_alloc_all_rx_descriptor(adpt);
	if (retval)
		goto err_alloc_rx;

	return 0;

err_alloc_rx:
	emac_free_all_tx_descriptor(adpt);
err_alloc_tx:
	dma_free_coherent(dev, ring_header->size,
			  ring_header->desc, ring_header->dma);

	ring_header->desc = NULL;
	ring_header->dma  = 0;
	ring_header->size = 0;
	ring_header->used = 0;
err_alloc_dma:
	return retval;
}

/* Free all TX and RX descriptor rings */
static void emac_free_all_rtx_descriptor(struct emac_adapter *adpt)
{
	struct emac_ring_header *ring_header = &adpt->ring_header;
	struct device *dev = adpt->rx_queue[0].dev;

	emac_free_all_tx_descriptor(adpt);
	emac_free_all_rx_descriptor(adpt);

	dma_free_coherent(dev, ring_header->size,
			  ring_header->desc, ring_header->dma);

	ring_header->desc = NULL;
	ring_header->dma  = 0;
	ring_header->size = 0;
	ring_header->used = 0;
}

/* Initialize descriptor rings */
static void emac_init_ring_ptrs(struct emac_adapter *adpt)
{
	int i, j;

	for (i = 0; i < adpt->num_txques; i++) {
		struct emac_tx_queue *txque = &adpt->tx_queue[i];
		struct emac_buffer *tpbuf = txque->tpd.tpbuff;

		txque->tpd.produce_idx = 0;
		txque->tpd.consume_idx = 0;
		for (j = 0; j < txque->tpd.count; j++)
			tpbuf[j].dma = 0;
	}

	for (i = 0; i < adpt->num_rxques; i++) {
		struct emac_rx_queue *rxque = &adpt->rx_queue[i];
		struct emac_buffer *rfbuf = rxque->rfd.rfbuff;

		rxque->rrd.produce_idx = 0;
		rxque->rrd.consume_idx = 0;
		rxque->rfd.produce_idx = 0;
		rxque->rfd.consume_idx = 0;
		for (j = 0; j < rxque->rfd.count; j++)
			rfbuf[j].dma = 0;
	}
}

/* Configure Receive Side Scaling (RSS) */
static void emac_config_rss(struct emac_adapter *adpt)
{
	static const u8 key[40] = {
		0x6D, 0x5A, 0x56, 0xDA, 0x25, 0x5B, 0x0E, 0xC2,
		0x41, 0x67, 0x25, 0x3D, 0x43, 0xA3, 0x8F, 0xB0,
		0xD0, 0xCA, 0x2B, 0xCB, 0xAE, 0x7B, 0x30, 0xB4,
		0x77, 0xCB, 0x2D, 0xA3, 0x80, 0x30, 0xF2, 0x0C,
		0x6A, 0x42, 0xB7, 0x3B, 0xBE, 0xAC, 0x01, 0xFA};

	struct emac_hw *hw = &adpt->hw;
	u32 reta = 0;
	u16 i, j;

	if (adpt->num_rxques == 1)
		return;

	if (!hw->rss_initialized) {
		hw->rss_initialized = true;
		/* initialize rss hash type and idt table size */
		hw->rss_hstype = EMAC_RSS_HSTYP_ALL_EN;
		hw->rss_idt_size = EMAC_RSS_IDT_SIZE;

		/* Fill out RSS key */
		memcpy(hw->rss_key, key, sizeof(hw->rss_key));

		/* Fill out redirection table */
		memset(hw->rss_idt, 0x0, sizeof(hw->rss_idt));
		for (i = 0, j = 0; i < EMAC_RSS_IDT_SIZE; i++, j++) {
			if (j == adpt->num_rxques)
				j = 0;
			if (j > 1)
				reta |= (j << ((i & 7) * 4));
			if ((i & 7) == 7) {
				hw->rss_idt[i>>3] = reta;
				reta = 0;
			}
		}
	}

	emac_hw_config_rss(hw);
}

/* Change the Maximum Transfer Unit (MTU) */
static int emac_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	int old_mtu   = netdev->mtu;
	int max_frame = new_mtu + ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN;

	if ((max_frame < EMAC_MIN_ETH_FRAME_SIZE) ||
	    (max_frame > EMAC_MAX_ETH_FRAME_SIZE)) {
		emac_err(adpt, "invalid MTU setting\n");
		return -EINVAL;
	}

	if ((old_mtu != new_mtu) && netif_running(netdev)) {
		emac_info(adpt, hw, "changing MTU from %d to %d\n",
			  netdev->mtu, new_mtu);
		netdev->mtu = new_mtu;
		adpt->hw.mtu = new_mtu;
		adpt->rxbuf_size = new_mtu > EMAC_DEF_RX_BUF_SIZE ?
			ALIGN(max_frame, 8) : EMAC_DEF_RX_BUF_SIZE;
		emac_reinit_locked(adpt);
	}

	return 0;
}

static inline int msm_emac_request_gpio_on(struct emac_adapter *adpt,
					   bool mdio, bool ephy)
{
	int i = 0;
	int result = 0;
	struct emac_phy *phy = &adpt->phy;

	if (phy->external) {
		for (i = 0; phy->uses_gpios && i < EMAC_GPIO_CNT; i++) {
			result = gpio_request(adpt->gpio[i], emac_gpio_name[i]);
			if (result) {
				emac_err(adpt,
					 "error:%d on gpio_request(%d:%s)\n",
					 result, adpt->gpio[i],
					 emac_gpio_name[i]);

				while (--i >= 0)
					gpio_free(adpt->gpio[i]);
				goto error;
			}
		}
	}
	return 0;
error:
	return result;
}

static inline int msm_emac_request_gpio_off(struct emac_adapter *adpt,
					    bool mdio, bool ephy)
{
	int i = 0;
	struct emac_phy *phy = &adpt->phy;

	if (phy->external) {
		for (i = 0; phy->uses_gpios && i < EMAC_GPIO_CNT; i++)
			gpio_free(adpt->gpio[i]);
	}
	return 0;
}

static inline int msm_emac_request_pinctrl_on(struct emac_adapter *adpt,
					      bool mdio, bool ephy)
{
	int result = 0;
	int ret    = 0;
	struct emac_phy *phy = &adpt->phy;

	if (phy->external) {
		if (mdio) {
			result = pinctrl_select_state(adpt->pinctrl,
						      adpt->mdio_pins_active);
			if (result)
				emac_err(adpt,
					 "error:%d Can not switch on %s pins\n",
					 result,
					 EMAC_PINCTRL_STATE_MDIO_ACTIVE);
			ret = result;
		}

		if (ephy) {
			result = pinctrl_select_state(adpt->pinctrl,
						      adpt->ephy_pins_active);
			if (result)
				emac_err(adpt,
					 "error:%d Can not switch on %s pins\n",
					 result,
					 EMAC_PINCTRL_STATE_EPHY_ACTIVE);
			if (!ret)
				ret = result;
		}
	}
	return ret;
}

static inline int msm_emac_request_pinctrl_off(struct emac_adapter *adpt,
					       bool mdio, bool ephy)
{
	int result = 0;
	int ret    = 0;
	struct emac_phy *phy = &adpt->phy;

	if (phy->external) {
		if (mdio) {
			result = pinctrl_select_state(adpt->pinctrl,
						      adpt->mdio_pins_sleep);
			if (result)
				emac_err(adpt,
					 "error:%d Can not switch off %s pins\n",
					 result, EMAC_PINCTRL_STATE_MDIO_SLEEP);
			ret = result;
		}

		if (ephy) {
			result = pinctrl_select_state(adpt->pinctrl,
						      adpt->ephy_pins_sleep);
			if (result)
				emac_err(adpt,
					 "error:%d Can not switch off %s pins\n",
					 result, EMAC_PINCTRL_STATE_EPHY_SLEEP);
			if (!ret)
				ret = result;
		}
	}
	return ret;
}

/* Bringup the interface/HW */
int emac_up(struct emac_adapter *adpt)
{
	struct emac_phy *phy = &adpt->phy;
	struct emac_hw *hw = &adpt->hw;
	struct net_device *netdev = adpt->netdev;
	int retval = 0;
	int i;

	if (!TEST_FLAG(adpt, ADPT_STATE_DOWN))
		return 0;

	emac_init_ring_ptrs(adpt);
	emac_set_rx_mode(netdev);

	emac_hw_config_mac(hw);
	emac_config_rss(adpt);

	retval = phy->ops.up(adpt);
	if (retval)
		return retval;

	for (i = 0; i < EMAC_NUM_CORE_IRQ; i++) {
		struct emac_irq_per_dev *irq = &adpt->irq[i];
		const struct emac_irq_common *irq_cmn = &emac_irq_cmn_tbl[i];

		if (!irq->irq)
			continue;

		retval = request_irq(irq->irq, irq_cmn->handler,
				     irq_cmn->irqflags, irq_cmn->name, irq);
		if (retval) {
			emac_err(adpt,
				 "error:%d on request_irq(%d:%s flags:0x%lx)\n",
				 retval, irq->irq, irq_cmn->name,
				 irq_cmn->irqflags);
			while (--i >= 0)
				if (adpt->irq[i].irq)
					free_irq(adpt->irq[i].irq,
						 &adpt->irq[i]);
			goto err_request_irq;
		}
	}

	for (i = 0; i < adpt->num_rxques; i++)
		emac_refresh_rx_buffer(&adpt->rx_queue[i]);

	emac_napi_enable_all(adpt);
	emac_enable_intr(adpt);

	netif_start_queue(netdev);
	CLR_FLAG(adpt, ADPT_STATE_DOWN);

	/* check link status */
	SET_FLAG(adpt, ADPT_TASK_LSC_REQ);
	adpt->link_jiffies = jiffies + EMAC_TRY_LINK_TIMEOUT;
	mod_timer(&adpt->emac_timer, jiffies);
	return retval;

err_request_irq:
	adpt->phy.ops.down(adpt);
	return retval;
}

/* Bring down the interface/HW */
void emac_down(struct emac_adapter *adpt, u32 ctrl)
{
	struct net_device *netdev = adpt->netdev;
	struct emac_phy *phy = &adpt->phy;
	struct emac_hw *hw = &adpt->hw;
	unsigned long flags;
	int i;

	if (TEST_FLAG(adpt, ADPT_STATE_DOWN))
		return;

	SET_FLAG(adpt, ADPT_STATE_DOWN);
	netif_stop_queue(netdev);

	netif_carrier_off(netdev);

	emac_disable_intr(adpt);
	emac_napi_disable_all(adpt);

	phy->ops.down(adpt);

	for (i = 0; i < EMAC_NUM_CORE_IRQ; i++)
		if (adpt->irq[i].irq)
			free_irq(adpt->irq[i].irq, &adpt->irq[i]);

	CLR_FLAG(adpt, ADPT_TASK_LSC_REQ);
	CLR_FLAG(adpt, ADPT_TASK_REINIT_REQ);
	CLR_FLAG(adpt, ADPT_TASK_CHK_SGMII_REQ);
	del_timer_sync(&adpt->emac_timer);

	cancel_work_sync(&adpt->hwtxtstamp_task);
	spin_lock_irqsave(&adpt->hwtxtstamp_lock, flags);
	__skb_queue_purge(&adpt->hwtxtstamp_pending_queue);
	__skb_queue_purge(&adpt->hwtxtstamp_ready_queue);
	spin_unlock_irqrestore(&adpt->hwtxtstamp_lock, flags);

	if (ctrl & EMAC_HW_CTRL_RESET_MAC)
		emac_hw_reset_mac(hw);

	phy->link_speed = EMAC_LINK_SPEED_UNKNOWN;
	emac_clean_all_tx_queues(adpt);
	emac_clean_all_rx_queues(adpt);
}

/* Called when the network interface is made active */
static int emac_open(struct net_device *netdev)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_irq_per_dev *irq = &adpt->irq[EMAC_WOL_IRQ];
	const struct emac_irq_common *irq_cmn = &emac_irq_cmn_tbl[EMAC_WOL_IRQ];
	int retval;

	netif_carrier_off(netdev);

	/* allocate rx/tx dma buffer & descriptors */
	retval = emac_alloc_all_rtx_descriptor(adpt);
	if (retval) {
		emac_err(adpt, "error in emac_alloc_all_rtx_descriptor\n");
		goto err_alloc_rtx;
	}

	pm_runtime_get_sync(netdev->dev.parent);
	retval = emac_up(adpt);
	pm_runtime_mark_last_busy(netdev->dev.parent);
	pm_runtime_put_autosuspend(netdev->dev.parent);
	if (retval)
		goto err_up;

	/* Register for EMAC WOL ISR */
	retval = request_threaded_irq(irq->irq, NULL, irq_cmn->handler,
				      IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				      irq_cmn->name, irq);
	if (retval) {
		emac_err(adpt,
			 "error:%d on request_irq(%d:%s flags:0x%lx)\n",
			 retval, irq->irq, irq_cmn->name, irq_cmn->irqflags);
		goto err_up;
	} else {
		emac_wol_gpio_irq(adpt, false);
	}
	return retval;

err_up:
	emac_free_all_rtx_descriptor(adpt);
err_alloc_rtx:
	return retval;
}

/* Called when the network interface is disabled */
static int emac_close(struct net_device *netdev)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;

	/* ensure no task is running and no reset is in progress */
	while (TEST_N_SET_FLAG(adpt, ADPT_STATE_RESETTING))
		msleep(20); /* Reset might take few 10s of ms */

	pm_runtime_get_sync(netdev->dev.parent);
	if (adpt->irq[EMAC_WOL_IRQ].irq)
		free_irq(adpt->irq[EMAC_WOL_IRQ].irq, &adpt->irq[EMAC_WOL_IRQ]);

	if (!TEST_FLAG(adpt, ADPT_STATE_DOWN))
		emac_down(adpt, EMAC_HW_CTRL_RESET_MAC);
	else
		emac_hw_reset_mac(hw);

	if (TEST_FLAG(hw, HW_PTP_CAP))
		emac_ptp_stop(hw);

	pm_runtime_mark_last_busy(netdev->dev.parent);
	pm_runtime_put_autosuspend(netdev->dev.parent);

	emac_free_all_rtx_descriptor(adpt);

	CLR_FLAG(adpt, ADPT_STATE_RESETTING);
	return 0;
}

/* Resize the descriptor rings */
int emac_resize_rings(struct net_device *netdev)
{
	/* close and then re-open interface */
	emac_close(netdev);
	return emac_open(netdev);
}

/* PHY related IOCTLs */
static int emac_mii_ioctl(struct net_device *netdev,
			  struct ifreq *ifr, int cmd)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_phy *phy = &adpt->phy;
	struct mii_ioctl_data *data = if_mii(ifr);
	int retval = 0;

	switch (cmd) {
	case SIOCGMIIPHY:
		data->phy_id = phy->addr;
		break;

	case SIOCGMIIREG:
		if (!capable(CAP_NET_ADMIN)) {
			retval = -EPERM;
			break;
		}

		if (data->reg_num & ~(0x1F)) {
			retval = -EFAULT;
			break;
		}

		if (data->phy_id >= PHY_MAX_ADDR) {
			retval = -EFAULT;
			break;
		}

		if (phy->external && data->phy_id != phy->addr) {
			retval = -EFAULT;
			break;
		}

		retval = emac_phy_read(adpt, data->phy_id, data->reg_num,
				       &data->val_out);
		break;

	case SIOCSMIIREG:
		if (!capable(CAP_NET_ADMIN)) {
			retval = -EPERM;
			break;
		}

		if (data->reg_num & ~(0x1F)) {
			retval = -EFAULT;
			break;
		}

		if (data->phy_id >= PHY_MAX_ADDR) {
			retval = -EFAULT;
			break;
		}

		if (phy->external && data->phy_id != phy->addr) {
			retval = -EFAULT;
			break;
		}

		retval = emac_phy_write(adpt, data->phy_id, data->reg_num,
					data->val_in);

		break;
	}

	return retval;
}

/* IOCTL support for the interface */
static int emac_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;

	switch (cmd) {
	case SIOCGMIIPHY:
	case SIOCGMIIREG:
	case SIOCSMIIREG:
		return emac_mii_ioctl(netdev, ifr, cmd);
	case SIOCSHWTSTAMP:
		if (TEST_FLAG(hw, HW_PTP_CAP))
			return emac_tstamp_ioctl(netdev, ifr, cmd);
	default:
		return -EOPNOTSUPP;
	}
}

/* Read statistics information from the HW */
void emac_update_hw_stats(struct emac_adapter *adpt)
{
	u16 hw_reg_addr = 0;
	u64 *stats_item = NULL;
	u32 val;

	/* Prevent stats update while adapter is being reset, or if the
	 * connection is down.
	 */
	if (adpt->phy.link_speed == 0)
		return;

	if (TEST_FLAG(adpt, ADPT_STATE_DOWN) ||
	    TEST_FLAG(adpt, ADPT_STATE_RESETTING))
		return;

	/* update rx status */
	hw_reg_addr = REG_MAC_RX_STATUS_BIN;
	stats_item = &adpt->hw_stats.rx_ok;

	while (hw_reg_addr <= REG_MAC_RX_STATUS_END) {
		val = emac_reg_r32(&adpt->hw, EMAC, hw_reg_addr);
		*stats_item += val;
		stats_item++;
		hw_reg_addr += sizeof(u32);
	}

	/* additional rx status */
	val = emac_reg_r32(&adpt->hw, EMAC, EMAC_RXMAC_STATC_REG23);
	adpt->hw_stats.rx_crc_align += val;
	val = emac_reg_r32(&adpt->hw, EMAC, EMAC_RXMAC_STATC_REG24);
	adpt->hw_stats.rx_jubbers += val;

	/* update tx status */
	hw_reg_addr = REG_MAC_TX_STATUS_BIN;
	stats_item = &adpt->hw_stats.tx_ok;

	while (hw_reg_addr <= REG_MAC_TX_STATUS_END) {
		val = emac_reg_r32(&adpt->hw, EMAC, hw_reg_addr);
		*stats_item += val;
		stats_item++;
		hw_reg_addr += sizeof(u32);
	}

	/* additional tx status */
	val = emac_reg_r32(&adpt->hw, EMAC, EMAC_TXMAC_STATC_REG25);
	adpt->hw_stats.tx_col += val;
}

/* Provide network statistics info for the interface */
struct rtnl_link_stats64 *emac_get_stats64(struct net_device *netdev,
					   struct rtnl_link_stats64 *net_stats)
{
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw_stats *hw_stats = &adpt->hw_stats;

	memset(net_stats, 0, sizeof(struct rtnl_link_stats64));
	emac_update_hw_stats(adpt);
	net_stats->rx_packets = hw_stats->rx_ok;
	net_stats->tx_packets = hw_stats->tx_ok;
	net_stats->rx_bytes = hw_stats->rx_byte_cnt;
	net_stats->tx_bytes = hw_stats->tx_byte_cnt;
	net_stats->multicast = hw_stats->rx_mcast;
	net_stats->collisions = hw_stats->tx_1_col +
	    hw_stats->tx_2_col * 2 +
	    hw_stats->tx_late_col + hw_stats->tx_abort_col;

	net_stats->rx_errors = hw_stats->rx_frag + hw_stats->rx_fcs_err +
	    hw_stats->rx_len_err + hw_stats->rx_sz_ov +
	    hw_stats->rx_align_err;
	net_stats->rx_fifo_errors = hw_stats->rx_rxf_ov;
	net_stats->rx_length_errors = hw_stats->rx_len_err;
	net_stats->rx_crc_errors = hw_stats->rx_fcs_err;
	net_stats->rx_frame_errors = hw_stats->rx_align_err;
	net_stats->rx_over_errors = hw_stats->rx_rxf_ov;
	net_stats->rx_missed_errors = hw_stats->rx_rxf_ov;

	net_stats->tx_errors = hw_stats->tx_late_col + hw_stats->tx_abort_col +
	    hw_stats->tx_underrun + hw_stats->tx_trunc;
	net_stats->tx_fifo_errors = hw_stats->tx_underrun;
	net_stats->tx_aborted_errors = hw_stats->tx_abort_col;
	net_stats->tx_window_errors = hw_stats->tx_late_col;

	return net_stats;
}

static const struct net_device_ops emac_netdev_ops = {
	.ndo_open		= &emac_open,
	.ndo_stop		= &emac_close,
	.ndo_validate_addr	= &eth_validate_addr,
	.ndo_start_xmit		= &emac_start_xmit,
	.ndo_set_mac_address	= &emac_set_mac_address,
	.ndo_change_mtu		= &emac_change_mtu,
	.ndo_do_ioctl		= &emac_ioctl,
	.ndo_tx_timeout		= &emac_tx_timeout,
	.ndo_get_stats64	= &emac_get_stats64,
	.ndo_set_features       = emac_set_features,
	.ndo_set_rx_mode        = emac_set_rx_mode,
};

/* Reinitialize the interface/HW if required */
static void emac_reinit_task_routine(struct emac_adapter *adpt)
{
	if (!TEST_FLAG(adpt, ADPT_TASK_REINIT_REQ))
		return;
	CLR_FLAG(adpt, ADPT_TASK_REINIT_REQ);

	if (TEST_FLAG(adpt, ADPT_STATE_DOWN) ||
	    TEST_FLAG(adpt, ADPT_STATE_RESETTING))
		return;

	emac_reinit_locked(adpt);
}

static inline char *emac_get_link_speed_desc(u32 speed)
{
	switch (speed) {
	case EMAC_LINK_SPEED_1GB_FULL:
		return  "1 Gbps Duplex Full";
	case EMAC_LINK_SPEED_100_FULL:
		return "100 Mbps Duplex Full";
	case EMAC_LINK_SPEED_100_HALF:
		return "100 Mbps Duplex Half";
	case EMAC_LINK_SPEED_10_FULL:
		return "10 Mbps Duplex Full";
	case EMAC_LINK_SPEED_10_HALF:
		return "10 Mbps Duplex HALF";
	default:
		return "unknown speed";
	}
}

/* Check link status and handle link state changes */
static void emac_link_task_routine(struct emac_adapter *adpt)
{
	struct net_device *netdev = adpt->netdev;
	struct emac_phy *phy = &adpt->phy;
	struct emac_hw *hw = &adpt->hw;
	char *link_desc;

	if (!TEST_FLAG(adpt, ADPT_TASK_LSC_REQ))
		return;
	CLR_FLAG(adpt, ADPT_TASK_LSC_REQ);

	/* ensure that no reset is in progress while link task is running */
	while (TEST_N_SET_FLAG(adpt, ADPT_STATE_RESETTING))
		msleep(20); /* Reset might take few 10s of ms */

	if (TEST_FLAG(adpt, ADPT_STATE_DOWN))
		goto link_task_done;

	pm_runtime_get_sync(netdev->dev.parent);
	emac_phy_check_link(adpt, &phy->link_speed, &phy->link_up);
	link_desc = emac_get_link_speed_desc(phy->link_speed);
	pm_runtime_mark_last_busy(netdev->dev.parent);
	pm_runtime_put_autosuspend(netdev->dev.parent);

	if (phy->link_up) {
		if (netif_carrier_ok(netdev))
			goto link_task_done;

		if (!adpt->runtime_enable)
			pm_runtime_get_sync(netdev->dev.parent);
		/* Acquire wake lock if link is detected to avoid device going
		 * into suspend
		 */
		__pm_stay_awake(&adpt->link_wlock);
		emac_info(adpt, timer, "NIC Link is Up %s\n", link_desc);

		phy->ops.tx_clk_set_rate(adpt);

		emac_hw_start_mac(hw);
		netif_carrier_on(netdev);
		netif_wake_queue(netdev);
		adpt->runtime_enable = 1;
	} else {
		if (time_after(adpt->link_jiffies, jiffies))
			SET_FLAG(adpt, ADPT_TASK_LSC_REQ);

		/* only continue if link was up previously */
		if (!netif_carrier_ok(netdev))
			goto link_task_done;

		phy->link_speed = 0;
		emac_info(adpt, timer, "NIC Link is Down\n");
		netif_stop_queue(netdev);
		netif_carrier_off(netdev);

		emac_hw_stop_mac(hw);
		/* Release wake lock if link is disconnected */
		__pm_relax(&adpt->link_wlock);
		pm_runtime_mark_last_busy(netdev->dev.parent);
		pm_runtime_put_autosuspend(netdev->dev.parent);
		adpt->runtime_enable = 0;
	}

	/* link state transition, kick timer */
	mod_timer(&adpt->emac_timer, jiffies);

link_task_done:
	CLR_FLAG(adpt, ADPT_STATE_RESETTING);
}

/* Watchdog task routine */
static void emac_task_routine(struct work_struct *work)
{
	struct emac_adapter *adpt = container_of(work, struct emac_adapter,
						 emac_task);

	if (!TEST_FLAG(adpt, ADPT_STATE_WATCH_DOG))
		emac_warn(adpt, timer, "flag STATE_WATCH_DOG doesn't set\n");

	emac_reinit_task_routine(adpt);

	emac_link_task_routine(adpt);

	adpt->phy.ops.periodic_task(adpt);

	CLR_FLAG(adpt, ADPT_STATE_WATCH_DOG);
}

/* Timer routine */
static void emac_timer_routine(unsigned long data)
{
	struct emac_adapter *adpt = (struct emac_adapter *)data;
	unsigned long delay;

	if (pm_runtime_status_suspended(adpt->netdev->dev.parent))
		return;

	/* poll faster when waiting for link */
	if (TEST_FLAG(adpt, ADPT_TASK_LSC_REQ))
		delay = HZ / 10;
	else
		delay = 2 * HZ;

	/* Reset the timer */
	mod_timer(&adpt->emac_timer, delay + jiffies);

	emac_task_schedule(adpt);
}

/* Initialize all queue data structures */
static void emac_init_rtx_queues(struct platform_device *pdev,
				 struct emac_adapter *adpt)
{
	int que_idx;

	adpt->num_txques = EMAC_DEF_TX_QUEUES;
	adpt->num_rxques = EMAC_DEF_RX_QUEUES;

	for (que_idx = 0; que_idx < adpt->num_txques; que_idx++) {
		struct emac_tx_queue *txque = &adpt->tx_queue[que_idx];

		txque->que_idx = que_idx;
		txque->netdev  = adpt->netdev;
		txque->dev     = &pdev->dev;
	}

	for (que_idx = 0; que_idx < adpt->num_rxques; que_idx++) {
		struct emac_rx_queue *rxque = &adpt->rx_queue[que_idx];

		rxque->que_idx = que_idx;
		rxque->netdev  = adpt->netdev;
		rxque->dev     = &pdev->dev;
	}

	switch (adpt->num_rxques) {
	case 4:
		adpt->rx_queue[3].produce_reg = EMAC_MAILBOX_13;
		adpt->rx_queue[3].produce_mask = RFD3_PROD_IDX_BMSK;
		adpt->rx_queue[3].produce_shft = RFD3_PROD_IDX_SHFT;

		adpt->rx_queue[3].process_reg = EMAC_MAILBOX_13;
		adpt->rx_queue[3].process_mask = RFD3_PROC_IDX_BMSK;
		adpt->rx_queue[3].process_shft = RFD3_PROC_IDX_SHFT;

		adpt->rx_queue[3].consume_reg = EMAC_MAILBOX_8;
		adpt->rx_queue[3].consume_mask = RFD3_CONS_IDX_BMSK;
		adpt->rx_queue[3].consume_shft = RFD3_CONS_IDX_SHFT;

		adpt->rx_queue[3].irq = &adpt->irq[3];
		adpt->rx_queue[3].intr = adpt->irq[3].mask & ISR_RX_PKT;

		/* fall through */
	case 3:
		adpt->rx_queue[2].produce_reg = EMAC_MAILBOX_6;
		adpt->rx_queue[2].produce_mask = RFD2_PROD_IDX_BMSK;
		adpt->rx_queue[2].produce_shft = RFD2_PROD_IDX_SHFT;

		adpt->rx_queue[2].process_reg = EMAC_MAILBOX_6;
		adpt->rx_queue[2].process_mask = RFD2_PROC_IDX_BMSK;
		adpt->rx_queue[2].process_shft = RFD2_PROC_IDX_SHFT;

		adpt->rx_queue[2].consume_reg = EMAC_MAILBOX_7;
		adpt->rx_queue[2].consume_mask = RFD2_CONS_IDX_BMSK;
		adpt->rx_queue[2].consume_shft = RFD2_CONS_IDX_SHFT;

		adpt->rx_queue[2].irq = &adpt->irq[2];
		adpt->rx_queue[2].intr = adpt->irq[2].mask & ISR_RX_PKT;

		/* fall through */
	case 2:
		adpt->rx_queue[1].produce_reg = EMAC_MAILBOX_5;
		adpt->rx_queue[1].produce_mask = RFD1_PROD_IDX_BMSK;
		adpt->rx_queue[1].produce_shft = RFD1_PROD_IDX_SHFT;

		adpt->rx_queue[1].process_reg = EMAC_MAILBOX_5;
		adpt->rx_queue[1].process_mask = RFD1_PROC_IDX_BMSK;
		adpt->rx_queue[1].process_shft = RFD1_PROC_IDX_SHFT;

		adpt->rx_queue[1].consume_reg = EMAC_MAILBOX_7;
		adpt->rx_queue[1].consume_mask = RFD1_CONS_IDX_BMSK;
		adpt->rx_queue[1].consume_shft = RFD1_CONS_IDX_SHFT;

		adpt->rx_queue[1].irq = &adpt->irq[1];
		adpt->rx_queue[1].intr = adpt->irq[1].mask & ISR_RX_PKT;

		/* fall through */
	case 1:
		adpt->rx_queue[0].produce_reg = EMAC_MAILBOX_0;
		adpt->rx_queue[0].produce_mask = RFD0_PROD_IDX_BMSK;
		adpt->rx_queue[0].produce_shft = RFD0_PROD_IDX_SHFT;

		adpt->rx_queue[0].process_reg = EMAC_MAILBOX_0;
		adpt->rx_queue[0].process_mask = RFD0_PROC_IDX_BMSK;
		adpt->rx_queue[0].process_shft = RFD0_PROC_IDX_SHFT;

		adpt->rx_queue[0].consume_reg = EMAC_MAILBOX_3;
		adpt->rx_queue[0].consume_mask = RFD0_CONS_IDX_BMSK;
		adpt->rx_queue[0].consume_shft = RFD0_CONS_IDX_SHFT;

		adpt->rx_queue[0].irq = &adpt->irq[0];
		adpt->rx_queue[0].intr = adpt->irq[0].mask & ISR_RX_PKT;
		break;
	}

	switch (adpt->num_txques) {
	case 4:
		adpt->tx_queue[3].produce_reg = EMAC_MAILBOX_11;
		adpt->tx_queue[3].produce_mask = H3TPD_PROD_IDX_BMSK;
		adpt->tx_queue[3].produce_shft = H3TPD_PROD_IDX_SHFT;

		adpt->tx_queue[3].consume_reg = EMAC_MAILBOX_12;
		adpt->tx_queue[3].consume_mask = H3TPD_CONS_IDX_BMSK;
		adpt->tx_queue[3].consume_shft = H3TPD_CONS_IDX_SHFT;

		/* fall through */
	case 3:
		adpt->tx_queue[2].produce_reg = EMAC_MAILBOX_9;
		adpt->tx_queue[2].produce_mask = H2TPD_PROD_IDX_BMSK;
		adpt->tx_queue[2].produce_shft = H2TPD_PROD_IDX_SHFT;

		adpt->tx_queue[2].consume_reg = EMAC_MAILBOX_10;
		adpt->tx_queue[2].consume_mask = H2TPD_CONS_IDX_BMSK;
		adpt->tx_queue[2].consume_shft = H2TPD_CONS_IDX_SHFT;

		/* fall through */
	case 2:
		adpt->tx_queue[1].produce_reg = EMAC_MAILBOX_16;
		adpt->tx_queue[1].produce_mask = H1TPD_PROD_IDX_BMSK;
		adpt->tx_queue[1].produce_shft = H1TPD_PROD_IDX_SHFT;

		adpt->tx_queue[1].consume_reg = EMAC_MAILBOX_10;
		adpt->tx_queue[1].consume_mask = H1TPD_CONS_IDX_BMSK;
		adpt->tx_queue[1].consume_shft = H1TPD_CONS_IDX_SHFT;

		/* fall through */
	case 1:
		adpt->tx_queue[0].produce_reg = EMAC_MAILBOX_15;
		adpt->tx_queue[0].produce_mask = NTPD_PROD_IDX_BMSK;
		adpt->tx_queue[0].produce_shft = NTPD_PROD_IDX_SHFT;

		adpt->tx_queue[0].consume_reg = EMAC_MAILBOX_2;
		adpt->tx_queue[0].consume_mask = NTPD_CONS_IDX_BMSK;
		adpt->tx_queue[0].consume_shft = NTPD_CONS_IDX_SHFT;
		break;
	}
}

/* Initialize various data structures  */
static void emac_init_adapter(struct emac_adapter *adpt)
{
	struct emac_phy *phy = &adpt->phy;
	struct emac_hw *hw   = &adpt->hw;
	int max_frame;

	/* ids */
	hw->devid = (u16)emac_reg_field_r32(hw, EMAC, EMAC_DMA_MAS_CTRL,
					    DEV_ID_NUM_BMSK, DEV_ID_NUM_SHFT);
	hw->revid = (u16)emac_reg_field_r32(hw, EMAC, EMAC_DMA_MAS_CTRL,
					    DEV_REV_NUM_BMSK, DEV_REV_NUM_SHFT);

	/* descriptors */
	adpt->num_txdescs = EMAC_DEF_TX_DESCS;
	adpt->num_rxdescs = EMAC_DEF_RX_DESCS;

	/* mtu */
	adpt->netdev->mtu = ETH_DATA_LEN;
	hw->mtu = adpt->netdev->mtu;
	max_frame = adpt->netdev->mtu + ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN;
	adpt->rxbuf_size = adpt->netdev->mtu > EMAC_DEF_RX_BUF_SIZE ?
			   ALIGN(max_frame, 8) : EMAC_DEF_RX_BUF_SIZE;

	/* dma */
	hw->dma_order = emac_dma_ord_out;
	hw->dmar_block = emac_dma_req_4096;
	hw->dmaw_block = emac_dma_req_128;
	hw->dmar_dly_cnt = DMAR_DLY_CNT_DEF;
	hw->dmaw_dly_cnt = DMAW_DLY_CNT_DEF;
	hw->tpd_burst = TXQ0_NUM_TPD_PREF_DEF;
	hw->rfd_burst = RXQ0_NUM_RFD_PREF_DEF;

	/* link */
	phy->link_up = false;
	phy->link_speed = EMAC_LINK_SPEED_UNKNOWN;

	/* flow control */
	phy->req_fc_mode = EMAC_FC_FULL;
	phy->cur_fc_mode = EMAC_FC_FULL;
	phy->disable_fc_autoneg = false;

	/* rss */
	hw->rss_initialized = false;
	hw->rss_hstype = 0;
	hw->rss_idt_size = 0;
	hw->rss_base_cpu = 0;
	memset(hw->rss_idt, 0x0, sizeof(hw->rss_idt));
	memset(hw->rss_key, 0x0, sizeof(hw->rss_key));

	/* irq moderator */
	hw->irq_mod = ((EMAC_DEF_RX_IRQ_MOD / 2) << IRQ_MODERATOR2_INIT_SHFT) |
		      ((EMAC_DEF_TX_IRQ_MOD / 2) << IRQ_MODERATOR_INIT_SHFT);

	/* others */
	hw->preamble = EMAC_PREAMBLE_DEF;
	adpt->wol = EMAC_WOL_MAGIC | EMAC_WOL_PHY;
}

/* Get the clock */
static int emac_get_clk(struct platform_device *pdev,
			struct emac_adapter *adpt)
{
	struct clk *clk;
	u8 i;

	for (i = 0; i < EMAC_CLK_CNT; i++) {
		clk = clk_get(&pdev->dev, emac_clk_name[i]);

		if (IS_ERR(clk)) {
			emac_err(adpt, "error:%ld on clk_get(%s)\n",
				 PTR_ERR(clk), emac_clk_name[i]);

			while (--i >= 0)
				if (adpt->clk[i].clk)
					clk_put(adpt->clk[i].clk);
			return PTR_ERR(clk);
		}

		adpt->clk[i].clk = clk;
	}

	return 0;
}

/* Initialize clocks */
static int emac_init_clks(struct emac_adapter *adpt)
{
	int retval;

	retval = emac_clk_prepare_enable(adpt, EMAC_CLK_AXI);
	if (retval)
		return retval;

	retval = emac_clk_prepare_enable(adpt, EMAC_CLK_CFG_AHB);
	if (retval)
		return retval;

	retval = emac_clk_set_rate(adpt, EMAC_CLK_125M, EMC_CLK_RATE_19_2MHZ);
	if (retval)
		return retval;

	retval = emac_clk_prepare_enable(adpt, EMAC_CLK_125M);

	return retval;
}

/* Enable clocks; needs emac_init_clks to be called before */
static int emac_enable_clks(struct emac_adapter *adpt)
{
	int retval;

	retval = emac_clk_set_rate(adpt, EMAC_CLK_TX, EMC_CLK_RATE_125MHZ);
	if (retval)
		return retval;

	retval = emac_clk_prepare_enable(adpt, EMAC_CLK_TX);
	if (retval)
		return retval;

	retval = emac_clk_set_rate(adpt, EMAC_CLK_125M, EMC_CLK_RATE_125MHZ);
	if (retval)
		return retval;

	retval = emac_clk_set_rate(adpt, EMAC_CLK_SYS_25M, EMC_CLK_RATE_25MHZ);
	if (retval)
		return retval;

	retval = emac_clk_prepare_enable(adpt, EMAC_CLK_SYS_25M);
	if (retval)
		return retval;

	retval = emac_clk_prepare_enable(adpt, EMAC_CLK_RX);
	if (retval)
		return retval;

	retval = emac_clk_prepare_enable(adpt, EMAC_CLK_SYS);

	return retval;
}

/* Disable clocks */
static void emac_disable_clks(struct emac_adapter *adpt)
{
	u8 i;

	for (i = 0; i < EMAC_CLK_CNT; i++) {
		struct emac_clk *clk = &adpt->clk[i];

		if (clk->enabled) {
			clk_disable_unprepare(clk->clk);
			clk->enabled = false;
		}
	}
}

static int msm_emac_pinctrl_init(struct emac_adapter *adpt, struct device *dev)
{
	adpt->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(adpt->pinctrl)) {
		emac_dbg(adpt, probe, "error:%ld Failed to get pin ctrl\n",
			 PTR_ERR(adpt->pinctrl));
		return PTR_ERR(adpt->pinctrl);
	}
	adpt->mdio_pins_active = pinctrl_lookup_state(adpt->pinctrl,
				EMAC_PINCTRL_STATE_MDIO_ACTIVE);
	if (IS_ERR_OR_NULL(adpt->mdio_pins_active)) {
		emac_dbg(adpt, probe, "error:%ld Failed to lookup mdio pinctrl active state\n",
			 PTR_ERR(adpt->mdio_pins_active));
		return PTR_ERR(adpt->mdio_pins_active);
	}

	adpt->mdio_pins_sleep = pinctrl_lookup_state(adpt->pinctrl,
				EMAC_PINCTRL_STATE_MDIO_SLEEP);
	if (IS_ERR_OR_NULL(adpt->mdio_pins_sleep)) {
		emac_dbg(adpt, probe, "error:%ld Failed to lookup mdio pinctrl sleep state\n",
			 PTR_ERR(adpt->mdio_pins_sleep));
		return PTR_ERR(adpt->mdio_pins_sleep);
	}

	adpt->ephy_pins_active = pinctrl_lookup_state(adpt->pinctrl,
				EMAC_PINCTRL_STATE_EPHY_ACTIVE);
	if (IS_ERR_OR_NULL(adpt->ephy_pins_active)) {
		emac_dbg(adpt, probe, "error:%ld Failed to lookup ephy pinctrl active state\n",
			 PTR_ERR(adpt->ephy_pins_active));
		return PTR_ERR(adpt->ephy_pins_active);
	}

	adpt->ephy_pins_sleep = pinctrl_lookup_state(adpt->pinctrl,
				EMAC_PINCTRL_STATE_EPHY_SLEEP);
	if (IS_ERR_OR_NULL(adpt->ephy_pins_sleep)) {
		emac_dbg(adpt, probe, "error:%ld Failed to lookup ephy pinctrl sleep state\n",
			 PTR_ERR(adpt->ephy_pins_sleep));
		return PTR_ERR(adpt->ephy_pins_sleep);
	}

	return 0;
}

/* Get the resources */
static int emac_get_resources(struct platform_device *pdev,
			      struct emac_adapter *adpt)
{
	int retval = 0;
	u8 i;
	struct resource *res;
	struct net_device *netdev = adpt->netdev;
	struct device_node *node = pdev->dev.of_node;
	static const char * const res_name[] = {"emac", "emac_csr",
						"emac_1588"};
	const void *maddr;
	const struct of_device_id *id;

	if (!node)
		return -ENODEV;

	/* get id */
	retval = of_property_read_u32(node, "cell-index", &pdev->id);
	if (retval)
		return retval;

	/* get board id */
	id = of_match_node(emac_dt_match, node);
	if (id == NULL) {
		emac_err(adpt, "can't find emac_dt_match node\n");
		return -ENODEV;
	}
	adpt->phy.board_id = (enum emac_phy_map_type)id->data;

	/* get time stamp enable flag */
	adpt->tstamp_en = of_property_read_bool(node, "qcom,emac-tstamp-en");

	retval = msm_emac_pinctrl_init(adpt, &pdev->dev);
	if (!retval) {
		adpt->gpio_on = msm_emac_request_pinctrl_on;
		adpt->gpio_off = msm_emac_request_pinctrl_off;
	} else {
		for (i = 0; adpt->phy.uses_gpios && i < EMAC_GPIO_CNT; i++) {
			retval = of_get_named_gpio(node, emac_gpio_name[i], 0);
			if (retval < 0)
				return retval;

			adpt->gpio[i] = retval;
		}
		adpt->gpio_on = msm_emac_request_gpio_on;
		adpt->gpio_off = msm_emac_request_gpio_off;
	}

	/* get mac address */
	maddr = of_get_mac_address(node);
	if (maddr)
		memcpy(adpt->hw.mac_perm_addr, maddr, netdev->addr_len);

	/* get irqs */
	for (i = 0; i < EMAC_IRQ_CNT; i++) {
		retval = platform_get_irq_byname(pdev,
						 emac_irq_cmn_tbl[i].name);
		adpt->irq[i].irq = (retval > 0) ? retval : 0;
	}

	retval = emac_get_clk(pdev, adpt);
	if (retval)
		return retval;

	/* get register addresses */
	retval = 0;
	for (i = 0; i < NUM_EMAC_REG_BASES; i++) {
		/* 1588 is required only if tstamp is enabled */
		if ((i == EMAC_1588) && !adpt->tstamp_en)
			continue;

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   res_name[i]);
		if (!res) {
			emac_err(adpt, "can't get %s resource\n", res_name[i]);
			retval = -ENOMEM;
			break;
		}

		adpt->hw.reg_addr[i] = ioremap(res->start, resource_size(res));
		if (!adpt->hw.reg_addr[i]) {
			emac_err(adpt, "can't remap %s\n", res_name[i]);
			retval = -ENOMEM;
			break;
		}
	}

	if (retval) {
		while (--i >= 0)
			if (adpt->hw.reg_addr[i])
				iounmap(adpt->hw.reg_addr[i]);
		goto err_reg_res;
	}

	netdev->base_addr = (unsigned long)adpt->hw.reg_addr[EMAC];
	return 0;

err_reg_res:
	for (i = 0; i < EMAC_CLK_CNT; i++) {
		if (adpt->clk[i].clk)
			clk_put(adpt->clk[i].clk);
	}

	return retval;
}

/* Release resources */
static void emac_release_resources(struct emac_adapter *adpt)
{
	u8 i;

	if (ACPI_HANDLE(adpt->dev))
		return;

	for (i = 0; i < NUM_EMAC_REG_BASES; i++) {
		if (adpt->hw.reg_addr[i])
			iounmap(adpt->hw.reg_addr[i]);
	}

	for (i = 0; i < EMAC_CLK_CNT; i++) {
		if (adpt->clk[i].clk)
			clk_put(adpt->clk[i].clk);
	}
}

/* Get the regulator */
static int emac_get_regulator(struct platform_device *pdev,
			      struct emac_adapter *adpt)
{
	struct regulator *vreg;
	u8 i;
	int len = 0;
	u32 tmp[EMAC_VREG_CNT];

	for (i = 0; i < EMAC_VREG_CNT; i++) {
		vreg = devm_regulator_get(&pdev->dev, emac_regulator_name[i]);

		if (IS_ERR(vreg)) {
			emac_err(adpt, "error:%ld unable to get emac %s\n",
				 PTR_ERR(vreg), emac_regulator_name[i]);
			return PTR_ERR(vreg);
		}
		adpt->vreg[i].vreg = vreg;
	}

	if (of_get_property(pdev->dev.of_node,
			    "qcom,vdd-voltage-level", &len)) {
		if (len == sizeof(tmp)) {
			of_property_read_u32_array(pdev->dev.of_node,
						   "qcom,vdd-voltage-level",
						   tmp, len/sizeof(*tmp));

			for (i = 0; i < EMAC_VREG_CNT; i++)
				adpt->vreg[i].voltage_uv = tmp[i];
		} else {
			emac_err(adpt, "unable to read voltage values for emac LDOs\n");
			return -EINVAL;
		}
	} else {
		emac_err(adpt, "unable to read qcom,vdd-voltage-level emac dt property\n");
		return -EINVAL;
	}
	return 0;
}

/* Set the Voltage */
static int emac_set_voltage(struct emac_adapter *adpt, enum emac_vreg_id id,
			    int min_uV, int max_uV)
{
	int retval = regulator_set_voltage(adpt->vreg[id].vreg, min_uV, max_uV);
	if (retval)
		emac_err(adpt,
			 "error:%d set voltage for %s\n",
			 retval, emac_regulator_name[id]);
	return retval;
}

/* Enable the emac core, internal/external phy regulator */
static int emac_enable_regulator(struct emac_adapter *adpt, u8 start, u8 end)
{
	int retval = 0;
	u8 i;

	for (i = start; i <= end; i++) {
		if (adpt->vreg[i].voltage_uv) {
			retval = emac_set_voltage(adpt, i,
						  adpt->vreg[i].voltage_uv,
						  adpt->vreg[i].voltage_uv);
			if (retval)
				goto err;
		}

		retval = regulator_enable(adpt->vreg[i].vreg);
		if (retval) {
			emac_err(adpt, "error:%d enable regulator %s\n",
				 retval, emac_regulator_name[EMAC_VREG3]);
			goto err;
		} else {
			adpt->vreg[i].enabled = true;
		}
	}
err:
	return retval;
}

/* Disable the emac core, internal/external phy regulator */
static void emac_disable_regulator(struct emac_adapter *adpt, u8 start, u8 end)
{
	u8 i;

	for (i = start; i <= end; i++) {
		struct emac_regulator *vreg = &adpt->vreg[i];

		if (vreg->enabled) {
			regulator_disable(vreg->vreg);
			vreg->enabled = false;
		}

		if (adpt->vreg[i].voltage_uv) {
			emac_set_voltage(adpt, i,
					 0, adpt->vreg[i].voltage_uv);
		}
	}
}

/* LDO init */
static int msm_emac_ldo_init(struct platform_device *pdev,
			     struct emac_adapter *adpt)
{
	int retval = 0;

	retval = emac_get_regulator(pdev, adpt);
	if (retval)
		return retval;

	retval =  emac_enable_regulator(adpt, EMAC_VREG1, EMAC_VREG5);
	if (retval)
		return retval;
	return 0;
}

static int emac_pm_suspend(struct device *device, bool wol_enable)
{
	struct platform_device *pdev = to_platform_device(device);
	struct net_device *netdev = dev_get_drvdata(&pdev->dev);
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;
	struct emac_phy *phy = &adpt->phy;
	u32 wufc = adpt->wol;
	int retval = 0;

	/* Check link state. Don't suspend if link is up */
	if (netif_carrier_ok(adpt->netdev))
		return -EPERM;

	/* cannot suspend if WOL interrupt is not enabled */
	if (!adpt->irq[EMAC_WOL_IRQ].irq)
		return -EPERM;

	if (netif_running(netdev)) {
		/* ensure no task is running and no reset is in progress */
		while (TEST_N_SET_FLAG(adpt, ADPT_STATE_RESETTING))
			msleep(20);	/* Reset might take few 10s of ms */

		emac_down(adpt, 0);

		CLR_FLAG(adpt, ADPT_STATE_RESETTING);
	}

	emac_hw_config_pow_save(hw, adpt->phy.link_speed, !!wufc,
				!!(wufc & EMAC_WOL_MAGIC));

	/* Enable EPHY Link UP interrupt */
	if (!phy->link_up) {
		retval = emac_phy_write(adpt, phy->addr, MII_INT_ENABLE,
					LINK_SUCCESS_INTERRUPT |
					LINK_SUCCESS_BX);
		if (retval)
			return retval;

		if (wol_enable)
			emac_wol_gpio_irq(adpt, true);
	}

	adpt->gpio_off(adpt, true, false);
	emac_disable_clks(adpt);
	emac_disable_regulator(adpt, EMAC_VREG1, EMAC_VREG2);
	return 0;
}

static int emac_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	struct net_device *netdev = dev_get_drvdata(&pdev->dev);
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_phy *phy = &adpt->phy;
	struct emac_hw	*hw  = &adpt->hw;
	int retval = 0;

	adpt->gpio_on(adpt, true, false);
	emac_enable_regulator(adpt, EMAC_VREG1, EMAC_VREG2);
	emac_init_clks(adpt);
	emac_enable_clks(adpt);
	emac_hw_reset_mac(hw);
	emac_phy_reset_external(adpt);

	/* Disable EPHY Link UP interrupt */
	retval = emac_phy_write(adpt, phy->addr, MII_INT_ENABLE, 0);
	if (retval)
		goto error;


	if (netif_running(netdev)) {
		retval = emac_up(adpt);
		if (retval)
			goto error;
	}
	return 0;
error:
	return retval;
}

#ifdef CONFIG_PM_RUNTIME
static int emac_pm_runtime_suspend(struct device *device)
{
	return emac_pm_suspend(device, true);
}

static int emac_pm_runtime_resume(struct device *device)
{
	return emac_pm_resume(device);
}

static int emac_pm_runtime_idle(struct device *device)
{
	return 0;
}
#else
#define emac_pm_runtime_suspend NULL
#define emac_pm_runtime_resume	NULL
#define emac_pm_runtime_idle	NULL
#endif /* CONFIG_PM_RUNTIME */

#ifdef CONFIG_PM_SLEEP
static int emac_pm_sys_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	struct net_device *netdev = dev_get_drvdata(&pdev->dev);
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_phy *phy = &adpt->phy;
	bool link_up = false;

	/* Check link state. Don't suspend if link is up */
	if (netif_carrier_ok(adpt->netdev))
		return -EPERM;

	phy->link_speed = EMAC_LINK_SPEED_10_HALF;
	phy->link_up = link_up;

	/* Disable EPHY WOL interrupt*/
	emac_wol_gpio_irq(adpt, false);

	if (!pm_runtime_enabled(device) || !pm_runtime_suspended(device)) {
		emac_pm_suspend(device, false);
		/* Synchronize runtime-pm and system-pm states:
		 * at this point we are already suspended. However, the
		 * runtime-PM framework still thinks that we are active.
		 * The three calls below let the runtime-PM know that we are
		 * suspended already without re-invoking the suspend callback
		 */
		pm_runtime_disable(netdev->dev.parent);
		pm_runtime_set_suspended(netdev->dev.parent);
		pm_runtime_enable(netdev->dev.parent);
	}

	netif_device_detach(netdev);
	return 0;
}

static int emac_pm_sys_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	struct net_device *netdev = dev_get_drvdata(&pdev->dev);
	struct emac_adapter *adpt = netdev_priv(netdev);

	netif_device_attach(netdev);

	if (!pm_runtime_enabled(device) || !pm_runtime_suspended(device)) {
		/* if runtime PM callback was not invoked (when both runtime-pm
		 * and systme-pm are in transition concurrently)
		*/
		emac_pm_resume(device);
		pm_runtime_mark_last_busy(netdev->dev.parent);
		pm_request_autosuspend(netdev->dev.parent);
	}
	/* Enable EPHY WOL interrupt*/
	emac_wol_gpio_irq(adpt, true);
	return 0;
}
#endif

/* Probe function */
static int emac_probe(struct platform_device *pdev)
{
	struct net_device *netdev;
	struct emac_adapter *adpt;
	struct emac_phy *phy;
	struct emac_hw *hw;
	int retval;
	u8 i;
	u32 hw_ver;

	netdev = alloc_etherdev(sizeof(struct emac_adapter));
	if (netdev == NULL) {
		dev_err(&pdev->dev, "etherdev alloc failed\n");
		retval = -ENOMEM;
		goto err_alloc_netdev;
	}

	dev_set_drvdata(&pdev->dev, netdev);
	SET_NETDEV_DEV(netdev, &pdev->dev);

	adpt = netdev_priv(netdev);
	adpt->netdev = netdev;
	phy = &adpt->phy;
	hw = &adpt->hw;
	adpt->msg_enable = netif_msg_init(msm_emac_msglvl, EMAC_MSG_DEFAULT);

	adpt->dma_mask = DMA_BIT_MASK(32);
	pdev->dev.dma_mask = &adpt->dma_mask;
	pdev->dev.dma_parms = &adpt->dma_parms;
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);

	dma_set_max_seg_size(&pdev->dev, 65536);
	dma_set_seg_boundary(&pdev->dev, 0xffffffff);

	for (i = 0; i < EMAC_IRQ_CNT; i++) {
		adpt->irq[i].idx  = i;
		adpt->irq[i].mask = emac_irq_cmn_tbl[i].init_mask;
	}
	adpt->irq[0].mask |= (msm_emac_intr_ext ? IMR_EXTENDED_MASK :
			      IMR_NORMAL_MASK);

	if (ACPI_HANDLE(adpt->dev))
		retval = emac_acpi_get_resources(pdev, adpt);
	else
		retval = emac_get_resources(pdev, adpt);
	if (retval)
		goto err_res;

	retval = msm_emac_ldo_init(pdev, adpt);
	if (retval)
		goto err_ldo_init;

	/* initialize clocks */
	retval = emac_init_clks(adpt);
	if (retval)
		goto err_clk_init;

	hw_ver = emac_reg_r32(hw, EMAC, EMAC_CORE_HW_VERSION);

	netdev->watchdog_timeo = EMAC_WATCHDOG_TIME;
	netdev->irq = adpt->irq[0].irq;

	if (adpt->tstamp_en)
		adpt->rrdesc_size = EMAC_TS_RRDESC_SIZE;
	else
		adpt->rrdesc_size = EMAC_RRDESC_SIZE;

	adpt->tpdesc_size = EMAC_TPDESC_SIZE;
	adpt->rfdesc_size = EMAC_RFDESC_SIZE;

	if (adpt->tstamp_en)
		SET_FLAG(hw, HW_PTP_CAP);

	/* init netdev */
	netdev->netdev_ops = &emac_netdev_ops;

	emac_set_ethtool_ops(netdev);

	/* init adapter */
	emac_init_adapter(adpt);

	/* init phy */
	retval = emac_phy_config(pdev, adpt);
	if (retval)
		goto err_init_phy;

	/* enable clocks */
	retval = emac_enable_clks(adpt);
	if (retval)
		goto err_clk_en;

	/* Configure MDIO lines */
	retval = adpt->gpio_on(adpt, true, true);
	if (retval)
		goto err_init_mdio_gpio;

	/* init external phy */
	retval = emac_phy_init_external(adpt);
	if (retval)
		goto err_init_ephy;

	/* reset mac */
	emac_hw_reset_mac(hw);

	/* setup link to put it in a known good starting state */
	retval = emac_phy_setup_link(adpt, phy->autoneg_advertised, true,
				     !phy->disable_fc_autoneg);
	if (retval)
		goto err_phy_link;

	/* set mac address */
	memcpy(hw->mac_addr, hw->mac_perm_addr, netdev->addr_len);
	memcpy(netdev->dev_addr, hw->mac_addr, netdev->addr_len);
	emac_hw_set_mac_addr(hw, hw->mac_addr);

	/* disable emac core and phy regulator */
	emac_disable_regulator(adpt, EMAC_VREG1, EMAC_VREG2);
	emac_disable_clks(adpt);

	/* set hw features */
	netdev->features = NETIF_F_SG | NETIF_F_HW_CSUM | NETIF_F_RXCSUM |
			NETIF_F_TSO | NETIF_F_TSO6 | NETIF_F_HW_VLAN_CTAG_RX |
			NETIF_F_HW_VLAN_CTAG_TX;
	netdev->hw_features = netdev->features;

	netdev->vlan_features |= NETIF_F_SG | NETIF_F_HW_CSUM |
				 NETIF_F_TSO | NETIF_F_TSO6;

	setup_timer(&adpt->emac_timer, &emac_timer_routine,
		    (unsigned long)adpt);
	INIT_WORK(&adpt->emac_task, emac_task_routine);

	/* Initialize queues */
	emac_init_rtx_queues(pdev, adpt);

	for (i = 0; i < adpt->num_rxques; i++)
		netif_napi_add(netdev, &adpt->rx_queue[i].napi,
			       emac_napi_rtx, 64);

	spin_lock_init(&adpt->hwtxtstamp_lock);
	spin_lock_init(&adpt->wol_irq_lock);
	skb_queue_head_init(&adpt->hwtxtstamp_pending_queue);
	skb_queue_head_init(&adpt->hwtxtstamp_ready_queue);
	INIT_WORK(&adpt->hwtxtstamp_task, emac_hwtxtstamp_task_routine);
	wakeup_source_init(&adpt->link_wlock, dev_name(&pdev->dev));

	SET_FLAG(hw, HW_VLANSTRIP_EN);
	SET_FLAG(adpt, ADPT_STATE_DOWN);
	strlcpy(netdev->name, "eth%d", sizeof(netdev->name));

	adpt->runtime_enable = 0;
	pm_runtime_set_autosuspend_delay(&pdev->dev, EMAC_TRY_LINK_TIMEOUT);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	/* if  !CONFIG_PM_RUNTIME then enable all the resources here and mange
	 * resources from system suspend/resume callbacks
	 */
	if (!pm_runtime_enabled(&pdev->dev))
		emac_pm_resume(&pdev->dev);

	retval = register_netdev(netdev);
	if (retval) {
		emac_err(adpt, "register netdevice failed\n");
		goto err_register_netdev;
	}

	/* carrier off reporting is important to ethtool even BEFORE open */
	netif_carrier_off(netdev);

	if (TEST_FLAG(hw, HW_PTP_CAP)) {
		pm_runtime_get_sync(&pdev->dev);
		emac_ptp_init(adpt->netdev);
		pm_runtime_mark_last_busy(&pdev->dev);
		pm_runtime_put_autosuspend(&pdev->dev);
	}

	pr_info("%s - version %s\n", emac_drv_description, emac_drv_version);
	emac_dbg(adpt, probe, "EMAC HW ID %d.%d\n", hw->devid, hw->revid);
	emac_dbg(adpt, probe, "EMAC HW version %d.%d.%d\n",
		 (hw_ver & MAJOR_BMSK) >> MAJOR_SHFT,
		 (hw_ver & MINOR_BMSK) >> MINOR_SHFT,
		 (hw_ver & STEP_BMSK) >> STEP_SHFT);
	return 0;

err_register_netdev:
err_phy_link:
err_init_ephy:
	adpt->gpio_off(adpt, true, true);
err_init_mdio_gpio:
err_clk_en:
err_init_phy:
err_clk_init:
	emac_disable_clks(adpt);
err_ldo_init:
	emac_disable_regulator(adpt, EMAC_VREG1, EMAC_VREG5);
	emac_release_resources(adpt);
err_res:
	free_netdev(netdev);
err_alloc_netdev:
	return retval;
}

static int emac_remove(struct platform_device *pdev)
{
	struct net_device *netdev = dev_get_drvdata(&pdev->dev);
	struct emac_adapter *adpt = netdev_priv(netdev);
	struct emac_hw *hw = &adpt->hw;

	pr_info("exiting %s\n", emac_drv_name);

	/* Disable EPHY WOL interrupt in suspend */
	emac_wol_gpio_irq(adpt, false);
	unregister_netdev(netdev);
	wakeup_source_trash(&adpt->link_wlock);
	if (TEST_FLAG(hw, HW_PTP_CAP))
		emac_ptp_remove(netdev);

	adpt->gpio_off(adpt, true, true);
	emac_disable_clks(adpt);
	emac_disable_regulator(adpt, EMAC_VREG1, EMAC_VREG5);

	emac_release_resources(adpt);
	free_netdev(netdev);
	dev_set_drvdata(&pdev->dev, NULL);
	return 0;
}

static const struct dev_pm_ops emac_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		emac_pm_sys_suspend,
		emac_pm_sys_resume
	)
	SET_RUNTIME_PM_OPS(
		emac_pm_runtime_suspend,
		emac_pm_runtime_resume,
		emac_pm_runtime_idle
	)
};

static struct of_device_id emac_dt_match[] = {
	{
		.compatible = "qcom,emac",
		.data = (void *)EMAC_PHY_MAP_DEFAULT,
	},
	{
		.compatible = "qcom,mdm9607-emac",
		.data = (void *)EMAC_PHY_MAP_MDM9607,
	},
	{}
};

static struct platform_driver emac_platform_driver = {
	.probe   = emac_probe,
	.remove  = emac_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name	= "msm_emac",
		.pm = &emac_pm_ops,
		.of_match_table = emac_dt_match,
		.acpi_match_table = ACPI_PTR(emac_acpi_match),
	},
};

static int __init emac_init_module(void)
{
	return platform_driver_register(&emac_platform_driver);
}

static void __exit emac_exit_module(void)
{
	platform_driver_unregister(&emac_platform_driver);
}

module_init(emac_init_module);
module_exit(emac_exit_module);

MODULE_LICENSE("GPL");
