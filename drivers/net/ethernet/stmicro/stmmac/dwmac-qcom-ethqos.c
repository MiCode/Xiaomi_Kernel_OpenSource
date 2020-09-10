// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2018-19, Linaro Limited
// Copyright (c) 2020, The Linux Foundation. All rights reserved.

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/phy.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mii.h>
#include <linux/of_mdio.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/debugfs.h>
#include <asm/dma-iommu.h>
#include <linux/iommu.h>
#include <linux/micrel_phy.h>
#include <linux/tcp.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/rtnetlink.h>
#include <linux/if_vlan.h>

#include "stmmac.h"
#include "stmmac_platform.h"
#include "dwmac-qcom-ethqos.h"
#include "stmmac_ptp.h"
#include "dwmac-qcom-ipa-offload.h"

#define PHY_LOOPBACK_1000 0x4140
#define PHY_LOOPBACK_100 0x6100
#define PHY_LOOPBACK_10 0x4100

static void __iomem *tlmm_central_base_addr;
static void ethqos_rgmii_io_macro_loopback(struct qcom_ethqos *ethqos,
					   int mode);
static int phy_digital_loopback_config(
	struct qcom_ethqos *ethqos, int speed, int config);

bool phy_intr_en;

static struct ethqos_emac_por emac_por[] = {
	{ .offset = RGMII_IO_MACRO_CONFIG,	.value = 0x0 },
	{ .offset = SDCC_HC_REG_DLL_CONFIG,	.value = 0x0 },
	{ .offset = SDCC_HC_REG_DDR_CONFIG,	.value = 0x0 },
	{ .offset = SDCC_HC_REG_DLL_CONFIG2,	.value = 0x0 },
	{ .offset = SDCC_USR_CTL,		.value = 0x0 },
	{ .offset = RGMII_IO_MACRO_CONFIG2,	.value = 0x0},
};

static struct ethqos_emac_driver_data emac_por_data = {
	.por = emac_por,
	.num_por = ARRAY_SIZE(emac_por),
};

struct qcom_ethqos *pethqos;

struct stmmac_emb_smmu_cb_ctx stmmac_emb_smmu_ctx = {0};
static unsigned char dev_addr[ETH_ALEN] = {
	0, 0x55, 0x7b, 0xb5, 0x7d, 0xf7};
static unsigned char config_dev_addr[ETH_ALEN] = {0};

void *ipc_stmmac_log_ctxt;
void *ipc_stmmac_log_ctxt_low;
int stmmac_enable_ipc_low;
#define MAX_PROC_SIZE 1024
char tmp_buff[MAX_PROC_SIZE];
static struct qmp_pkt pkt;
static char qmp_buf[MAX_QMP_MSG_SIZE + 1] = {0};
static struct ip_params pparams;

static void qcom_ethqos_read_iomacro_por_values(struct qcom_ethqos *ethqos)
{
	int i;

	ethqos->por = emac_por_data.por;
	ethqos->num_por = emac_por_data.num_por;

	/* Read to POR values and enable clk */
	for (i = 0; i < ethqos->num_por; i++)
		ethqos->por[i].value =
			readl_relaxed(
				ethqos->rgmii_base +
				ethqos->por[i].offset);
}

unsigned int dwmac_qcom_get_eth_type(unsigned char *buf)
{
	return
		((((u16)buf[QTAG_ETH_TYPE_OFFSET] << 8) |
		  buf[QTAG_ETH_TYPE_OFFSET + 1]) == ETH_P_8021Q) ?
		(((u16)buf[QTAG_VLAN_ETH_TYPE_OFFSET] << 8) |
		 buf[QTAG_VLAN_ETH_TYPE_OFFSET + 1]) :
		 (((u16)buf[QTAG_ETH_TYPE_OFFSET] << 8) |
		  buf[QTAG_ETH_TYPE_OFFSET + 1]);
}

static inline unsigned int dwmac_qcom_get_vlan_ucp(unsigned char  *buf)
{
	return
		(((u16)buf[QTAG_UCP_FIELD_OFFSET] << 8)
		 | buf[QTAG_UCP_FIELD_OFFSET + 1]);
}

u16 dwmac_qcom_select_queue(
	struct net_device *dev,
	struct sk_buff *skb,
	void *accel_priv,
	select_queue_fallback_t fallback)
{
	u16 txqueue_select = ALL_OTHER_TRAFFIC_TX_CHANNEL;
	unsigned int eth_type, priority, vlan_id;
	bool ipa_enabled = pethqos->ipa_enabled;

	/* Retrieve ETH type */
	eth_type = dwmac_qcom_get_eth_type(skb->data);

	if (pethqos->cv2x_mode == CV2X_MODE_AP) {
		if (skb_vlan_tag_present(skb)) {
			vlan_id = skb_vlan_tag_get_id(skb);
			if (vlan_id == pethqos->cv2x_vlan.vlan_id)
				txqueue_select = CV2X_TAG_TX_CHANNEL;
			else if (vlan_id == pethqos->qoe_vlan.vlan_id)
				txqueue_select = QMI_TAG_TX_CHANNEL;
			else
				txqueue_select =
				ALL_OTHER_TX_TRAFFIC_IPA_DISABLED;
		} else {
			txqueue_select = ALL_OTHER_TX_TRAFFIC_IPA_DISABLED;
		}
	} else if (pethqos->cv2x_mode == CV2X_MODE_MDM) {
		txqueue_select = ALL_OTHER_TRAFFIC_TX_CHANNEL;
	} else if (eth_type == ETH_P_TSN) {
		/* Read VLAN priority field from skb->data */
		priority = dwmac_qcom_get_vlan_ucp(skb->data);

		priority >>= VLAN_TAG_UCP_SHIFT;
		if (priority == CLASS_A_TRAFFIC_UCP) {
			txqueue_select = CLASS_A_TRAFFIC_TX_CHANNEL;
		} else if (priority == CLASS_B_TRAFFIC_UCP) {
			txqueue_select = CLASS_B_TRAFFIC_TX_CHANNEL;
		} else {
			if (ipa_enabled)
				txqueue_select = ALL_OTHER_TRAFFIC_TX_CHANNEL;
			else
				txqueue_select =
				ALL_OTHER_TX_TRAFFIC_IPA_DISABLED;
		}
	} else {
		/* VLAN tagged IP packet or any other non vlan packets (PTP)*/
		if (ipa_enabled)
			txqueue_select = ALL_OTHER_TRAFFIC_TX_CHANNEL;
		else
			txqueue_select = ALL_OTHER_TX_TRAFFIC_IPA_DISABLED;
	}

	ETHQOSDBG("tx_queue %d\n", txqueue_select);
	return txqueue_select;
}

int dwmac_qcom_program_avb_algorithm(
	struct stmmac_priv *priv, struct ifr_data_struct *req)
{
	struct dwmac_qcom_avb_algorithm l_avb_struct, *u_avb_struct =
		(struct dwmac_qcom_avb_algorithm *)req->ptr;
	struct dwmac_qcom_avb_algorithm_params *avb_params;
	int ret = 0;

	ETHQOSDBG("\n");

	if (copy_from_user(&l_avb_struct, (void __user *)u_avb_struct,
			   sizeof(struct dwmac_qcom_avb_algorithm))) {
		ETHQOSERR("Failed to fetch AVB Struct\n");
		return -EFAULT;
	}

	if (priv->speed == SPEED_1000)
		avb_params = &l_avb_struct.speed1000params;
	else
		avb_params = &l_avb_struct.speed100params;

	/* Application uses 1 for CLASS A traffic and
	 * 2 for CLASS B traffic
	 * Configure right channel accordingly
	 */
	if (l_avb_struct.qinx == 1)
		l_avb_struct.qinx = CLASS_A_TRAFFIC_TX_CHANNEL;
	else if (l_avb_struct.qinx == 2)
		l_avb_struct.qinx = CLASS_B_TRAFFIC_TX_CHANNEL;

	priv->plat->tx_queues_cfg[l_avb_struct.qinx].mode_to_use =
		MTL_QUEUE_AVB;
	priv->plat->tx_queues_cfg[l_avb_struct.qinx].send_slope =
		avb_params->send_slope,
	priv->plat->tx_queues_cfg[l_avb_struct.qinx].idle_slope =
		avb_params->idle_slope,
	priv->plat->tx_queues_cfg[l_avb_struct.qinx].high_credit =
		avb_params->hi_credit,
	priv->plat->tx_queues_cfg[l_avb_struct.qinx].low_credit =
		avb_params->low_credit,

	priv->hw->mac->config_cbs(
	   priv->hw, priv->plat->tx_queues_cfg[l_avb_struct.qinx].send_slope,
	   priv->plat->tx_queues_cfg[l_avb_struct.qinx].idle_slope,
	   priv->plat->tx_queues_cfg[l_avb_struct.qinx].high_credit,
	   priv->plat->tx_queues_cfg[l_avb_struct.qinx].low_credit,
	   l_avb_struct.qinx);

	ETHQOSDBG("\n");
	return ret;
}

unsigned int dwmac_qcom_get_plat_tx_coal_frames(
	struct sk_buff *skb)
{
	bool is_udp;
	unsigned int eth_type;

	eth_type = dwmac_qcom_get_eth_type(skb->data);

#ifdef CONFIG_PTPSUPPORT_OBJ
	if (eth_type == ETH_P_1588)
		return PTP_INT_MOD;
#endif

	if (eth_type == ETH_P_TSN)
		return AVB_INT_MOD;
	if (eth_type == ETH_P_IP || eth_type == ETH_P_IPV6) {
#ifdef CONFIG_PTPSUPPORT_OBJ
		is_udp = (((eth_type == ETH_P_IP) &&
				   (ip_hdr(skb)->protocol ==
					IPPROTO_UDP)) ||
				  ((eth_type == ETH_P_IPV6) &&
				   (ipv6_hdr(skb)->nexthdr ==
					IPPROTO_UDP)));

		if (is_udp && ((udp_hdr(skb)->dest ==
			htons(PTP_UDP_EV_PORT)) ||
			(udp_hdr(skb)->dest ==
			  htons(PTP_UDP_GEN_PORT))))
			return PTP_INT_MOD;
#endif
		return IP_PKT_INT_MOD;
	}
	return DEFAULT_INT_MOD;
}

int ethqos_handle_prv_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct stmmac_priv *pdata = netdev_priv(dev);
	struct ifr_data_struct req;
	struct pps_cfg eth_pps_cfg;
	int ret = 0;

	if (copy_from_user(&req, ifr->ifr_ifru.ifru_data,
			   sizeof(struct ifr_data_struct)))
		return -EFAULT;

	switch (req.cmd) {
	case ETHQOS_CONFIG_PPSOUT_CMD:
		if (copy_from_user(&eth_pps_cfg, (void __user *)req.ptr,
				   sizeof(struct pps_cfg)))
			return -EFAULT;

		if (eth_pps_cfg.ppsout_ch < 0 ||
		    eth_pps_cfg.ppsout_ch >= pdata->dma_cap.pps_out_num)
			ret = -EOPNOTSUPP;
		else if ((eth_pps_cfg.ppsout_align == 1) &&
			 ((eth_pps_cfg.ppsout_ch != DWC_ETH_QOS_PPS_CH_0) &&
			 (eth_pps_cfg.ppsout_ch != DWC_ETH_QOS_PPS_CH_3)))
			ret = -EOPNOTSUPP;
		else
			ret = ppsout_config(pdata, &eth_pps_cfg);
		break;
	case ETHQOS_AVB_ALGORITHM:
		ret = dwmac_qcom_program_avb_algorithm(pdata, &req);
		break;
	default:
		break;
	}
	return ret;
}

static int __init set_early_ethernet_ipv4(char *ipv4_addr_in)
{
	int ret = 1;

	pparams.is_valid_ipv4_addr = false;

	if (!ipv4_addr_in)
		return ret;

	strlcpy(pparams.ipv4_addr_str,
		ipv4_addr_in, sizeof(pparams.ipv4_addr_str));
	ETHQOSDBG("Early ethernet IPv4 addr: %s\n", pparams.ipv4_addr_str);

	ret = in4_pton(pparams.ipv4_addr_str, -1,
		       (u8 *)&pparams.ipv4_addr.s_addr, -1, NULL);
	if (ret != 1 || pparams.ipv4_addr.s_addr == 0) {
		ETHQOSERR("Invalid ipv4 address programmed: %s\n",
			  ipv4_addr_in);
		return ret;
	}

	pparams.is_valid_ipv4_addr = true;
	return ret;
}

__setup("eipv4=", set_early_ethernet_ipv4);

static int __init set_early_ethernet_ipv6(char *ipv6_addr_in)
{
	int ret = 1;

	pparams.is_valid_ipv6_addr = false;

	if (!ipv6_addr_in)
		return ret;

	strlcpy(pparams.ipv6_addr_str,
		ipv6_addr_in, sizeof(pparams.ipv6_addr_str));
	ETHQOSDBG("Early ethernet IPv6 addr: %s\n", pparams.ipv6_addr_str);

	ret = in6_pton(pparams.ipv6_addr_str, -1,
		       (u8 *)&pparams.ipv6_addr.ifr6_addr.s6_addr32, -1, NULL);
	if (ret != 1 || !pparams.ipv6_addr.ifr6_addr.s6_addr32)  {
		ETHQOSERR("Invalid ipv6 address programmed: %s\n",
			  ipv6_addr_in);
		return ret;
	}

	pparams.is_valid_ipv6_addr = true;
	return ret;
}

__setup("eipv6=", set_early_ethernet_ipv6);

static int __init set_early_ethernet_mac(char *mac_addr)
{
	int ret = 1;
	bool valid_mac = false;

	pparams.is_valid_mac_addr = false;
	if (!mac_addr)
		return ret;

	valid_mac = mac_pton(mac_addr, pparams.mac_addr);
	if (!valid_mac)
		goto fail;

	valid_mac = is_valid_ether_addr(pparams.mac_addr);
	if (!valid_mac)
		goto fail;

	pparams.is_valid_mac_addr = true;
	return ret;

fail:
	ETHQOSERR("Invalid Mac address programmed: %s\n", mac_addr);
	return ret;
}

__setup("ermac=", set_early_ethernet_mac);

static int qcom_ethqos_add_ipaddr(struct ip_params *ip_info,
				  struct net_device *dev)
{
	int res = 0;
	struct ifreq ir;
	struct sockaddr_in *sin = (void *)&ir.ifr_ifru.ifru_addr;
	struct net *net = dev_net(dev);

	if (!net || !net->genl_sock || !net->genl_sock->sk_socket) {
		ETHQOSERR("Sock is null, unable to assign ipv4 address\n");
		return res;
	}
	/*For valid Ipv4 address*/
	memset(&ir, 0, sizeof(ir));
	memcpy(&sin->sin_addr.s_addr, &ip_info->ipv4_addr,
	       sizeof(sin->sin_addr.s_addr));

	strlcpy(ir.ifr_ifrn.ifrn_name,
		dev->name, sizeof(ir.ifr_ifrn.ifrn_name));
	sin->sin_family = AF_INET;
	sin->sin_port = 0;

	res = inet_ioctl(net->genl_sock->sk_socket,
			 SIOCSIFADDR, (unsigned long)(void *)&ir);
		if (res) {
			ETHQOSERR("can't setup IPv4 address!: %d\r\n", res);
		} else {
			ETHQOSINFO("Assigned IPv4 address: %s\r\n",
				   ip_info->ipv4_addr_str);
#ifdef CONFIG_MSM_BOOT_TIME_MARKER
place_marker("M - Etherent Assigned IPv4 address");
#endif
		}
	return res;
}

static int qcom_ethqos_add_ipv6addr(struct ip_params *ip_info,
				    struct net_device *dev)
{
	int ret = -EFAULT;
	struct in6_ifreq ir6;
	char *prefix;
	struct net *net = dev_net(dev);
	/*For valid IPv6 address*/

	if (!net || !net->genl_sock || !net->genl_sock->sk_socket)
		ETHQOSERR("Sock is null, unable to assign ipv6 address\n");

	if (!net->ipv6.devconf_dflt) {
		ETHQOSERR("ipv6.devconf_dflt is null, schedule wq\n");
		schedule_delayed_work(&pethqos->ipv6_addr_assign_wq,
				      msecs_to_jiffies(1000));
		return ret;
	}
	memset(&ir6, 0, sizeof(ir6));
	memcpy(&ir6, &ip_info->ipv6_addr, sizeof(struct in6_ifreq));
	ir6.ifr6_ifindex = dev->ifindex;

	prefix = strnchr(ip_info->ipv6_addr_str,
			 strlen(ip_info->ipv6_addr_str), '/');

	if (!prefix) {
		ir6.ifr6_prefixlen = 0;
	} else {
		kstrtoul(prefix + 1, 0, (unsigned long *)&ir6.ifr6_prefixlen);
		if (ir6.ifr6_prefixlen > 128)
			ir6.ifr6_prefixlen = 0;
	}
	ret = inet6_ioctl(net->genl_sock->sk_socket,
			  SIOCSIFADDR, (unsigned long)(void *)&ir6);
		if (ret) {
			ETHQOSDBG("Can't setup IPv6 address!\r\n");
		} else {
			ETHQOSDBG("Assigned IPv6 address: %s\r\n",
				  ip_info->ipv6_addr_str);
#ifdef CONFIG_MSM_BOOT_TIME_MARKER
		place_marker("M - Ethernet Assigned IPv6 address");
#endif
		}
	return ret;
}

static int rgmii_readl(struct qcom_ethqos *ethqos, unsigned int offset)
{
	return readl(ethqos->rgmii_base + offset);
}

static void rgmii_writel(struct qcom_ethqos *ethqos,
			 int value, unsigned int offset)
{
	writel(value, ethqos->rgmii_base + offset);
}

static void rgmii_updatel(struct qcom_ethqos *ethqos,
			  int mask, int val, unsigned int offset)
{
	unsigned int temp;

	temp =  rgmii_readl(ethqos, offset);
	temp = (temp & ~(mask)) | val;
	rgmii_writel(ethqos, temp, offset);
}

static void rgmii_dump(struct qcom_ethqos *ethqos)
{
	dev_dbg(&ethqos->pdev->dev, "Rgmii register dump\n");
	dev_dbg(&ethqos->pdev->dev, "RGMII_IO_MACRO_CONFIG: %x\n",
		rgmii_readl(ethqos, RGMII_IO_MACRO_CONFIG));
	dev_dbg(&ethqos->pdev->dev, "SDCC_HC_REG_DLL_CONFIG: %x\n",
		rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG));
	dev_dbg(&ethqos->pdev->dev, "SDCC_HC_REG_DDR_CONFIG: %x\n",
		rgmii_readl(ethqos, SDCC_HC_REG_DDR_CONFIG));
	dev_dbg(&ethqos->pdev->dev, "SDCC_HC_REG_DLL_CONFIG2: %x\n",
		rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG2));
	dev_dbg(&ethqos->pdev->dev, "SDC4_STATUS: %x\n",
		rgmii_readl(ethqos, SDC4_STATUS));
	dev_dbg(&ethqos->pdev->dev, "SDCC_USR_CTL: %x\n",
		rgmii_readl(ethqos, SDCC_USR_CTL));
	dev_dbg(&ethqos->pdev->dev, "RGMII_IO_MACRO_CONFIG2: %x\n",
		rgmii_readl(ethqos, RGMII_IO_MACRO_CONFIG2));
	dev_dbg(&ethqos->pdev->dev, "RGMII_IO_MACRO_DEBUG1: %x\n",
		rgmii_readl(ethqos, RGMII_IO_MACRO_DEBUG1));
	dev_dbg(&ethqos->pdev->dev, "EMAC_SYSTEM_LOW_POWER_DEBUG: %x\n",
		rgmii_readl(ethqos, EMAC_SYSTEM_LOW_POWER_DEBUG));
}

/* Clock rates */
#define RGMII_1000_NOM_CLK_FREQ			(250 * 1000 * 1000UL)
#define RGMII_ID_MODE_100_LOW_SVS_CLK_FREQ	 (50 * 1000 * 1000UL)
#define RGMII_ID_MODE_10_LOW_SVS_CLK_FREQ	  (5 * 1000 * 1000UL)

static void
ethqos_update_rgmii_clk_and_bus_cfg(struct qcom_ethqos *ethqos,
				    unsigned int speed)
{
	int ret;

	switch (speed) {
	case SPEED_1000:
		ethqos->rgmii_clk_rate =  RGMII_1000_NOM_CLK_FREQ;
		break;

	case SPEED_100:
		ethqos->rgmii_clk_rate =  RGMII_ID_MODE_100_LOW_SVS_CLK_FREQ;
		break;

	case SPEED_10:
		ethqos->rgmii_clk_rate =  RGMII_ID_MODE_10_LOW_SVS_CLK_FREQ;
		break;
	}

	switch (speed) {
	case SPEED_1000:
		ethqos->vote_idx = VOTE_IDX_1000MBPS;
		break;
	case SPEED_100:
		ethqos->vote_idx = VOTE_IDX_100MBPS;
		break;
	case SPEED_10:
		ethqos->vote_idx = VOTE_IDX_10MBPS;
		break;
	case 0:
		ethqos->vote_idx = VOTE_IDX_0MBPS;
		ethqos->rgmii_clk_rate = 0;
		break;
	}

	if (ethqos->bus_hdl) {
		ret = msm_bus_scale_client_update_request(ethqos->bus_hdl,
							  ethqos->vote_idx);
		WARN_ON(ret);
	}

	clk_set_rate(ethqos->rgmii_clk, ethqos->rgmii_clk_rate);
}

static int qcom_ethqos_qmp_mailbox_init(struct qcom_ethqos *ethqos)
{
	ethqos->qmp_mbox_client = devm_kzalloc(
	&ethqos->pdev->dev, sizeof(*ethqos->qmp_mbox_client), GFP_KERNEL);

	if (IS_ERR(ethqos->qmp_mbox_client)) {
		ETHQOSERR("qmp alloc client failed\n");
		return -EINVAL;
	}

	ethqos->qmp_mbox_client->dev = &ethqos->pdev->dev;
	ethqos->qmp_mbox_client->tx_block = true;
	ethqos->qmp_mbox_client->tx_tout = 1000;
	ethqos->qmp_mbox_client->knows_txdone = false;

	ethqos->qmp_mbox_chan = mbox_request_channel(ethqos->qmp_mbox_client,
						     0);

	if (IS_ERR(ethqos->qmp_mbox_chan)) {
		ETHQOSERR("qmp request channel failed\n");
		return -EINVAL;
	}

	return 0;
}

static int qcom_ethqos_qmp_mailbox_send_message(struct qcom_ethqos *ethqos)
{
	int ret = 0;

	memset(&qmp_buf[0], 0, MAX_QMP_MSG_SIZE + 1);

	snprintf(qmp_buf, MAX_QMP_MSG_SIZE, "{class:ctile, pc:0}");

	pkt.size = ((size_t)strlen(qmp_buf) + 0x3) & ~0x3;
	pkt.data = qmp_buf;

	ret = mbox_send_message(ethqos->qmp_mbox_chan, (void *)&pkt);

	ETHQOSDBG("qmp mbox_send_message ret = %d\n", ret);

	if (ret < 0) {
		ETHQOSERR("Disabling c-tile power collapse failed\n");
		return ret;
	}

	ETHQOSDBG("Disabling c-tile power collapse succeded");

	return 0;
}

/**
 *  DWC_ETH_QOS_qmp_mailbox_work - Scheduled from probe
 *  @work: work_struct
 */
static void qcom_ethqos_qmp_mailbox_work(struct work_struct *work)
{
	struct qcom_ethqos *ethqos =
		container_of(work, struct qcom_ethqos, qmp_mailbox_work);

	ETHQOSDBG("Enter\n");

	/* Send QMP message to disable c-tile power collapse */
	qcom_ethqos_qmp_mailbox_send_message(ethqos);

	ETHQOSDBG("Exit\n");
}

static void ethqos_set_func_clk_en(struct qcom_ethqos *ethqos)
{
	rgmii_updatel(ethqos, RGMII_CONFIG_FUNC_CLK_EN,
		      RGMII_CONFIG_FUNC_CLK_EN, RGMII_IO_MACRO_CONFIG);
}

static int ethqos_dll_configure(struct qcom_ethqos *ethqos)
{
	unsigned int val;
	int retry = 1000;

	/* Set CDR_EN */
	if (ethqos->emac_ver == EMAC_HW_v2_3_2 ||
	    ethqos->emac_ver == EMAC_HW_v2_1_2)
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CDR_EN,
			      0, SDCC_HC_REG_DLL_CONFIG);
	else
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CDR_EN,
			      SDCC_DLL_CONFIG_CDR_EN, SDCC_HC_REG_DLL_CONFIG);

	/* Set CDR_EXT_EN */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CDR_EXT_EN,
		      SDCC_DLL_CONFIG_CDR_EXT_EN, SDCC_HC_REG_DLL_CONFIG);

	/* Clear CK_OUT_EN */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CK_OUT_EN,
		      0, SDCC_HC_REG_DLL_CONFIG);

	if (!ethqos->io_macro.rx_dll_bypass)
		/* Set DLL_EN */
		rgmii_updatel(ethqos, SDCC_DLL_CONFIG_DLL_EN,
			      SDCC_DLL_CONFIG_DLL_EN, SDCC_HC_REG_DLL_CONFIG);

	if (ethqos->emac_ver != EMAC_HW_v2_3_2 &&
	    ethqos->emac_ver != EMAC_HW_v2_1_2) {
		rgmii_updatel(ethqos, SDCC_DLL_MCLK_GATING_EN,
			      0, SDCC_HC_REG_DLL_CONFIG);

		rgmii_updatel(ethqos, SDCC_DLL_CDR_FINE_PHASE,
			      0, SDCC_HC_REG_DLL_CONFIG);
	}
	/* Wait for CK_OUT_EN clear */
	do {
		val = rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG);
		val &= SDCC_DLL_CONFIG_CK_OUT_EN;
		if (!val)
			break;
		mdelay(1);
		retry--;
	} while (retry > 0);
	if (!retry)
		dev_err(&ethqos->pdev->dev, "Clear CK_OUT_EN timedout\n");

	/* Set CK_OUT_EN */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CK_OUT_EN,
		      SDCC_DLL_CONFIG_CK_OUT_EN, SDCC_HC_REG_DLL_CONFIG);

	/* Wait for CK_OUT_EN set */
	retry = 1000;
	do {
		val = rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG);
		val &= SDCC_DLL_CONFIG_CK_OUT_EN;
		if (val)
			break;
		mdelay(1);
		retry--;
	} while (retry > 0);
	if (!retry)
		dev_err(&ethqos->pdev->dev, "Set CK_OUT_EN timedout\n");

	/* Set DDR_CAL_EN */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_DDR_CAL_EN,
		      SDCC_DLL_CONFIG2_DDR_CAL_EN, SDCC_HC_REG_DLL_CONFIG2);

	if (ethqos->emac_ver != EMAC_HW_v2_3_2 &&
	    ethqos->emac_ver != EMAC_HW_v2_1_2) {
		if (!ethqos->io_macro.rx_dll_bypass)
			rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_DLL_CLOCK_DIS,
				      0, SDCC_HC_REG_DLL_CONFIG2);

		rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_MCLK_FREQ_CALC,
			      0x1A << 10, SDCC_HC_REG_DLL_CONFIG2);

		rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_DDR_TRAFFIC_INIT_SEL,
			      BIT(2), SDCC_HC_REG_DLL_CONFIG2);

		rgmii_updatel(ethqos, SDCC_DLL_CONFIG2_DDR_TRAFFIC_INIT_SW,
			      SDCC_DLL_CONFIG2_DDR_TRAFFIC_INIT_SW,
			      SDCC_HC_REG_DLL_CONFIG2);
	}

	return 0;
}

static int ethqos_rgmii_macro_init(struct qcom_ethqos *ethqos)
{
	/* Disable loopback mode */
	rgmii_updatel(ethqos, RGMII_CONFIG2_TX_TO_RX_LOOPBACK_EN,
		      0, RGMII_IO_MACRO_CONFIG2);

	/* Select RGMII, write 0 to interface select */
	rgmii_updatel(ethqos, RGMII_CONFIG_INTF_SEL,
		      0, RGMII_IO_MACRO_CONFIG);

	switch (ethqos->speed) {
	case SPEED_1000:
		rgmii_updatel(ethqos, RGMII_CONFIG_DDR_MODE,
			      RGMII_CONFIG_DDR_MODE, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_BYPASS_TX_ID_EN,
			      0, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_POS_NEG_DATA_SEL,
			      RGMII_CONFIG_POS_NEG_DATA_SEL,
			      RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_PROG_SWAP,
			      RGMII_CONFIG_PROG_SWAP, RGMII_IO_MACRO_CONFIG);
		if (ethqos->emac_ver != EMAC_HW_v2_1_2)
			rgmii_updatel(ethqos, RGMII_CONFIG2_DATA_DIVIDE_CLK_SEL,
				      0, RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG2_RSVD_CONFIG15,
			      0, RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
			      RGMII_CONFIG2_RX_PROG_SWAP,
			      RGMII_IO_MACRO_CONFIG2);

		/* Set PRG_RCLK_DLY to 57 for 1.8 ns delay */
		if (ethqos->emac_ver == EMAC_HW_v2_3_2) {
			rgmii_updatel(ethqos, SDCC_DDR_CONFIG_PRG_RCLK_DLY,
				      69, SDCC_HC_REG_DDR_CONFIG);
		} else if (ethqos->emac_ver == EMAC_HW_v2_1_2) {
			rgmii_updatel(ethqos, SDCC_DDR_CONFIG_PRG_RCLK_DLY,
				      52, SDCC_HC_REG_DDR_CONFIG);
		} else {
			if (!ethqos->io_macro.rx_dll_bypass)
				rgmii_updatel(ethqos,
					      SDCC_DDR_CONFIG_PRG_RCLK_DLY,
					      57, SDCC_HC_REG_DDR_CONFIG);
		}

		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_PRG_DLY_EN,
			      SDCC_DDR_CONFIG_PRG_DLY_EN,
			      SDCC_HC_REG_DDR_CONFIG);
		if (ethqos->emac_ver == EMAC_HW_v2_3_2 ||
		    ethqos->emac_ver == EMAC_HW_v2_1_2)
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      0, RGMII_IO_MACRO_CONFIG);
		else
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_IO_MACRO_CONFIG);
		break;

	case SPEED_100:
		rgmii_updatel(ethqos, RGMII_CONFIG_DDR_MODE,
			      RGMII_CONFIG_DDR_MODE, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_BYPASS_TX_ID_EN,
			      RGMII_CONFIG_BYPASS_TX_ID_EN,
			      RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_POS_NEG_DATA_SEL,
			      0, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_PROG_SWAP,
			      0, RGMII_IO_MACRO_CONFIG);
		if (ethqos->emac_ver != EMAC_HW_v2_1_2)
			rgmii_updatel(ethqos, RGMII_CONFIG2_DATA_DIVIDE_CLK_SEL,
				      0, RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
			      RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG_MAX_SPD_PRG_2,
			      BIT(6), RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG2_RSVD_CONFIG15,
			      0, RGMII_IO_MACRO_CONFIG2);
		if (ethqos->io_macro.rx_prog_swap)
			rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
				      RGMII_CONFIG2_RX_PROG_SWAP,
				      RGMII_IO_MACRO_CONFIG2);
		else
			rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
				      0, RGMII_IO_MACRO_CONFIG2);
		/* Write 0x5 to PRG_RCLK_DLY_CODE */
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_CODE,
			      (BIT(29) | BIT(27)), SDCC_HC_REG_DDR_CONFIG);
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY,
			      SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY,
			      SDCC_HC_REG_DDR_CONFIG);
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_EN,
			      SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_EN,
			      SDCC_HC_REG_DDR_CONFIG);
		if (ethqos->emac_ver == EMAC_HW_v2_3_2 ||
		    ethqos->emac_ver == EMAC_HW_v2_1_2)
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      0, RGMII_IO_MACRO_CONFIG);
		else
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_IO_MACRO_CONFIG);
		break;

	case SPEED_10:
		rgmii_updatel(ethqos, RGMII_CONFIG_DDR_MODE,
			      RGMII_CONFIG_DDR_MODE, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_BYPASS_TX_ID_EN,
			      RGMII_CONFIG_BYPASS_TX_ID_EN,
			      RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_POS_NEG_DATA_SEL,
			      0, RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG_PROG_SWAP,
			      0, RGMII_IO_MACRO_CONFIG);
		if (ethqos->emac_ver != EMAC_HW_v2_1_2)
			rgmii_updatel(ethqos, RGMII_CONFIG2_DATA_DIVIDE_CLK_SEL,
				      0, RGMII_IO_MACRO_CONFIG2);
		if (ethqos->emac_ver == EMAC_HW_v2_3_2 ||
		    ethqos->emac_ver == EMAC_HW_v2_1_2)
			rgmii_updatel(ethqos,
				      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
				      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
				      RGMII_IO_MACRO_CONFIG2);
		else
			rgmii_updatel(ethqos,
				      RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN,
				      0, RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG_MAX_SPD_PRG_9,
			      BIT(12) | GENMASK(9, 8),
			      RGMII_IO_MACRO_CONFIG);
		rgmii_updatel(ethqos, RGMII_CONFIG2_RSVD_CONFIG15,
			      0, RGMII_IO_MACRO_CONFIG2);
		if (ethqos->io_macro.rx_prog_swap)
			rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
				      RGMII_CONFIG2_RX_PROG_SWAP,
				      RGMII_IO_MACRO_CONFIG2);
		else
			rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
				      0, RGMII_IO_MACRO_CONFIG2);
		/* Write 0x5 to PRG_RCLK_DLY_CODE */
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_CODE,
			      (BIT(29) | BIT(27)), SDCC_HC_REG_DDR_CONFIG);
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY,
			      SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY,
			      SDCC_HC_REG_DDR_CONFIG);
		rgmii_updatel(ethqos, SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_EN,
			      SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_EN,
			      SDCC_HC_REG_DDR_CONFIG);
		if (ethqos->emac_ver == EMAC_HW_v2_3_2 ||
		    ethqos->emac_ver == EMAC_HW_v2_1_2)
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      0, RGMII_IO_MACRO_CONFIG);
		else
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_IO_MACRO_CONFIG);
		break;
	default:
		dev_err(&ethqos->pdev->dev,
			"Invalid speed %d\n", ethqos->speed);
		return -EINVAL;
	}

	return 0;
}

static int ethqos_configure(struct qcom_ethqos *ethqos)
{
	volatile unsigned int dll_lock;
	unsigned int i, retry = 1000;

	/* Reset to POR values and enable clk */
	for (i = 0; i < ethqos->num_por; i++)
		rgmii_writel(ethqos, ethqos->por[i].value,
			     ethqos->por[i].offset);
	ethqos_set_func_clk_en(ethqos);

	/* Initialize the DLL first */

	/* Set DLL_RST */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_DLL_RST,
		      SDCC_DLL_CONFIG_DLL_RST, SDCC_HC_REG_DLL_CONFIG);

	/* Set PDN */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_PDN,
		      SDCC_DLL_CONFIG_PDN, SDCC_HC_REG_DLL_CONFIG);

	/* Clear DLL_RST */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_DLL_RST, 0,
		      SDCC_HC_REG_DLL_CONFIG);

	/* Clear PDN */
	rgmii_updatel(ethqos, SDCC_DLL_CONFIG_PDN, 0,
		      SDCC_HC_REG_DLL_CONFIG);

	if (ethqos->speed != SPEED_100 && ethqos->speed != SPEED_10) {
		if (ethqos->io_macro.rx_dll_bypass) {
			/* Set DLL_CLOCK_DISABLE */
			rgmii_updatel(ethqos,
				      SDCC_DLL_CONFIG2_DLL_CLOCK_DIS,
				      SDCC_DLL_CONFIG2_DLL_CLOCK_DIS,
				      SDCC_HC_REG_DLL_CONFIG2);

			/* Clear DLL_EN */
			rgmii_updatel(ethqos, SDCC_DLL_CONFIG_DLL_EN,
				      0, SDCC_HC_REG_DLL_CONFIG);

			/* Set PDN */
			rgmii_updatel(ethqos,
				      SDCC_DLL_CONFIG_PDN,
				      SDCC_DLL_CONFIG_PDN,
				      SDCC_HC_REG_DLL_CONFIG);

			/* Set USR_CTL bit 30 */
			rgmii_updatel(ethqos, BIT(30), BIT(30), SDCC_USR_CTL);
		} else {
			/* Set DLL_EN */
			rgmii_updatel(ethqos,
				      SDCC_DLL_CONFIG_DLL_EN,
				      SDCC_DLL_CONFIG_DLL_EN,
				      SDCC_HC_REG_DLL_CONFIG);

			/* Set CK_OUT_EN */
			rgmii_updatel(ethqos, SDCC_DLL_CONFIG_CK_OUT_EN,
				      SDCC_DLL_CONFIG_CK_OUT_EN,
				      SDCC_HC_REG_DLL_CONFIG);

			/* Set USR_CTL bit 26 with mask of 3 bits */
			rgmii_updatel(ethqos, GENMASK(26, 24), BIT(26),
				      SDCC_USR_CTL);

			/* wait for DLL LOCK */
			do {
				mdelay(1);
				dll_lock = rgmii_readl(ethqos, SDC4_STATUS);
				if (dll_lock & SDC4_STATUS_DLL_LOCK)
					break;
				retry--;
			} while (retry > 0);
			if (!retry)
				dev_err(&ethqos->pdev->dev,
					"Timeout while waiting for DLL lock\n");
		}

		ethqos_dll_configure(ethqos);
	}

	ethqos_rgmii_macro_init(ethqos);

	return 0;
}

static void ethqos_fix_mac_speed(void *priv, unsigned int speed)
{
	struct qcom_ethqos *ethqos = priv;

	ethqos->speed = speed;
	ethqos_update_rgmii_clk_and_bus_cfg(ethqos, speed);
	ethqos_configure(ethqos);
}

static int ethqos_mdio_read(struct stmmac_priv  *priv, int phyaddr, int phyreg)
{
	unsigned int mii_address = priv->hw->mii.addr;
	unsigned int mii_data = priv->hw->mii.data;
	u32 v;
	int data;
	u32 value = MII_BUSY;
	struct qcom_ethqos *ethqos = priv->plat->bsp_priv;

	if (ethqos->phy_state == PHY_IS_OFF) {
		ETHQOSINFO("Phy is in off state reading is not possible\n");
		return -EOPNOTSUPP;
	}

	value |= (phyaddr << priv->hw->mii.addr_shift)
		& priv->hw->mii.addr_mask;
	value |= (phyreg << priv->hw->mii.reg_shift) & priv->hw->mii.reg_mask;
	value |= (priv->clk_csr << priv->hw->mii.clk_csr_shift)
		& priv->hw->mii.clk_csr_mask;
	if (priv->plat->has_gmac4)
		value |= MII_GMAC4_READ;

	if (readl_poll_timeout(priv->ioaddr + mii_address, v, !(v & MII_BUSY),
			       100, 10000))
		return -EBUSY;

	writel_relaxed(value, priv->ioaddr + mii_address);

	if (readl_poll_timeout(priv->ioaddr + mii_address, v, !(v & MII_BUSY),
			       100, 10000))
		return -EBUSY;

	/* Read the data from the MII data register */
	data = (int)readl_relaxed(priv->ioaddr + mii_data);

	return data;
}

static int ethqos_phy_intr_config(struct qcom_ethqos *ethqos)
{
	int ret = 0;

	ethqos->phy_intr = platform_get_irq_byname(ethqos->pdev, "phy-intr");

	if (ethqos->phy_intr < 0) {
		if (ethqos->phy_intr != -EPROBE_DEFER) {
			dev_err(&ethqos->pdev->dev,
				"PHY IRQ configuration information not found\n");
		}
		ret = 1;
	}

	return ret;
}

static void ethqos_handle_phy_interrupt(struct qcom_ethqos *ethqos)
{
	int phy_intr_status = 0;
	struct platform_device *pdev = ethqos->pdev;

	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);
	int micrel_intr_status = 0;

	if ((dev->phydev->phy_id & dev->phydev->drv->phy_id_mask)
		== MICREL_PHY_ID) {
		phy_intr_status = ethqos_mdio_read(
			priv, priv->plat->phy_addr, DWC_ETH_QOS_BASIC_STATUS);
		ETHQOSDBG(
			"Basic Status Reg (%#x) = %#x\n",
			DWC_ETH_QOS_BASIC_STATUS, phy_intr_status);
		micrel_intr_status = ethqos_mdio_read(
			priv, priv->plat->phy_addr,
			DWC_ETH_QOS_MICREL_PHY_INTCS);
		ETHQOSDBG(
			"MICREL PHY Intr EN Reg (%#x) = %#x\n",
			DWC_ETH_QOS_MICREL_PHY_INTCS, micrel_intr_status);

		/* Call ack interrupt to clear the WOL status fields */
		if (dev->phydev->drv->ack_interrupt)
			dev->phydev->drv->ack_interrupt(dev->phydev);

		/* Interrupt received for link state change */
		if (phy_intr_status & LINK_STATE_MASK) {
			if (micrel_intr_status & MICREL_LINK_UP_INTR_STATUS)
				ETHQOSDBG("Intr for link UP state\n");
			phy_mac_interrupt(dev->phydev, LINK_UP);
		} else if (!(phy_intr_status & LINK_STATE_MASK)) {
			ETHQOSDBG("Intr for link DOWN state\n");
			phy_mac_interrupt(dev->phydev, LINK_DOWN);
		} else if (!(phy_intr_status & AUTONEG_STATE_MASK)) {
			ETHQOSDBG("Intr for link down with auto-neg err\n");
		} else if (phy_intr_status & PHY_WOL) {
			ETHQOSDBG("Interrupt received for WoL packet\n");
		}
	} else {
		phy_intr_status =
		 ethqos_mdio_read(
		    priv, priv->plat->phy_addr, DWC_ETH_QOS_PHY_INTR_STATUS);

		if (dev->phydev && (phy_intr_status & LINK_UP_STATE))
			phy_mac_interrupt(dev->phydev, LINK_UP);
		else if (dev->phydev && (phy_intr_status & LINK_DOWN_STATE))
			phy_mac_interrupt(dev->phydev, LINK_DOWN);
	}
}

static void ethqos_defer_phy_isr_work(struct work_struct *work)
{
	struct qcom_ethqos *ethqos =
		container_of(work, struct qcom_ethqos, emac_phy_work);

	if (ethqos->clks_suspended)
		wait_for_completion(&ethqos->clk_enable_done);

	ethqos_handle_phy_interrupt(ethqos);
}

static irqreturn_t ETHQOS_PHY_ISR(int irq, void *dev_data)
{
	struct qcom_ethqos *ethqos = (struct qcom_ethqos *)dev_data;

	pm_wakeup_event(&ethqos->pdev->dev, 5000);

	queue_work(system_wq, &ethqos->emac_phy_work);

	return IRQ_HANDLED;
}

int ethqos_phy_intr_enable(struct stmmac_priv *priv)
{
	int ret = 0;
	struct qcom_ethqos *ethqos = priv->plat->bsp_priv;

	if (ethqos_phy_intr_config(ethqos)) {
		ret = 1;
		return ret;
	}

	INIT_WORK(&ethqos->emac_phy_work, ethqos_defer_phy_isr_work);
	init_completion(&ethqos->clk_enable_done);

	ret = request_irq(ethqos->phy_intr, ETHQOS_PHY_ISR,
			  IRQF_SHARED, "stmmac", ethqos);
	if (ret) {
		ETHQOSERR("Unable to register PHY IRQ %d\n",
			  ethqos->phy_intr);
		return ret;
	}
	phy_intr_en = true;
	return ret;
}

static void ethqos_pps_irq_config(struct qcom_ethqos *ethqos)
{
	ethqos->pps_class_a_irq =
	platform_get_irq_byname(ethqos->pdev, "ptp_pps_irq_0");
	if (ethqos->pps_class_a_irq < 0) {
		if (ethqos->pps_class_a_irq != -EPROBE_DEFER)
			ETHQOSERR("class_a_irq config info not found\n");
	}
	ethqos->pps_class_b_irq =
	platform_get_irq_byname(ethqos->pdev, "ptp_pps_irq_1");
	if (ethqos->pps_class_b_irq < 0) {
		if (ethqos->pps_class_b_irq != -EPROBE_DEFER)
			ETHQOSERR("class_b_irq config info not found\n");
	}
}

static const struct of_device_id qcom_ethqos_match[] = {
	{ .compatible = "qcom,sdxprairie-ethqos",},
	{ .compatible = "qcom,emac-smmu-embedded", },
	{ .compatible = "qcom,stmmac-ethqos", },
	{}
};

static ssize_t read_phy_reg_dump(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	struct qcom_ethqos *ethqos = file->private_data;
	unsigned int len = 0, buf_len = 2000;
	char *buf;
	ssize_t ret_cnt;
	int phydata = 0;
	int i = 0;

	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	if (!ethqos || !dev->phydev) {
		ETHQOSERR("NULL Pointer\n");
		return -EINVAL;
	}
	if (ethqos->phy_state == PHY_IS_OFF) {
		ETHQOSINFO("Phy is in off state phy dump is not possible\n");
		return -EOPNOTSUPP;
	}

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, buf_len - len,
					 "\n************* PHY Reg dump *************\n");

	for (i = 0; i < 32; i++) {
		phydata = ethqos_mdio_read(priv, priv->plat->phy_addr, i);
		len += scnprintf(buf + len, buf_len - len,
					 "MII Register (%#x) = %#x\n",
					 i, phydata);
	}

	if (len > buf_len) {
		ETHQOSERR("(len > buf_len) buffer not sufficient\n");
		len = buf_len;
	}

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static ssize_t read_rgmii_reg_dump(struct file *file,
				   char __user *user_buf, size_t count,
				   loff_t *ppos)
{
	struct qcom_ethqos *ethqos = file->private_data;
	unsigned int len = 0, buf_len = 2000;
	char *buf;
	ssize_t ret_cnt;
	int rgmii_data = 0;
	struct platform_device *pdev = ethqos->pdev;

	struct net_device *dev = platform_get_drvdata(pdev);

	if (!ethqos || !dev->phydev) {
		ETHQOSERR("NULL Pointer\n");
		return -EINVAL;
	}

	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += scnprintf(buf + len, buf_len - len,
					 "\n************* RGMII Reg dump *************\n");
	rgmii_data = rgmii_readl(ethqos, RGMII_IO_MACRO_CONFIG);
	len += scnprintf(buf + len, buf_len - len,
					 "RGMII_IO_MACRO_CONFIG Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG);
	len += scnprintf(buf + len, buf_len - len,
					 "SDCC_HC_REG_DLL_CONFIG Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, SDCC_HC_REG_DDR_CONFIG);
	len += scnprintf(buf + len, buf_len - len,
					 "SDCC_HC_REG_DDR_CONFIG Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, SDCC_HC_REG_DLL_CONFIG2);
	len += scnprintf(buf + len, buf_len - len,
					 "SDCC_HC_REG_DLL_CONFIG2 Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, SDC4_STATUS);
	len += scnprintf(buf + len, buf_len - len,
					 "SDC4_STATUS Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, SDCC_USR_CTL);
	len += scnprintf(buf + len, buf_len - len,
					 "SDCC_USR_CTL Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, RGMII_IO_MACRO_CONFIG2);
	len += scnprintf(buf + len, buf_len - len,
					 "RGMII_IO_MACRO_CONFIG2 Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, RGMII_IO_MACRO_DEBUG1);
	len += scnprintf(buf + len, buf_len - len,
					 "RGMII_IO_MACRO_DEBUG1 Register = %#x\n",
					 rgmii_data);
	rgmii_data = rgmii_readl(ethqos, EMAC_SYSTEM_LOW_POWER_DEBUG);
	len += scnprintf(buf + len, buf_len - len,
					 "EMAC_SYSTEM_LOW_POWER_DEBUG Register = %#x\n",
					 rgmii_data);

	if (len > buf_len) {
		ETHQOSERR("(len > buf_len) buffer not sufficient\n");
		len = buf_len;
	}

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static ssize_t read_phy_off(struct file *file,
			    char __user *user_buf,
			    size_t count, loff_t *ppos)
{
	unsigned int len = 0, buf_len = 2000;
	char *buf;
	ssize_t ret_cnt;
	struct qcom_ethqos *ethqos = file->private_data;

	if (!ethqos) {
		ETHQOSERR("NULL Pointer\n");
		return -EINVAL;
	}
	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (ethqos->current_phy_mode == DISABLE_PHY_IMMEDIATELY)
		len += scnprintf(buf + len, buf_len - len,
				"Disable phy immediately enabled\n");
	else if (ethqos->current_phy_mode == ENABLE_PHY_IMMEDIATELY)
		len += scnprintf(buf + len, buf_len - len,
				 "Enable phy immediately enabled\n");
	else if (ethqos->current_phy_mode == DISABLE_PHY_AT_SUSPEND_ONLY) {
		len += scnprintf(buf + len, buf_len - len,
				 "Disable Phy at suspend\n");
		len += scnprintf(buf + len, buf_len - len,
				 " & do not enable at resume enabled\n");
	} else if (ethqos->current_phy_mode ==
		 DISABLE_PHY_SUSPEND_ENABLE_RESUME) {
		len += scnprintf(buf + len, buf_len - len,
				 "Disable Phy at suspend\n");
		len += scnprintf(buf + len, buf_len - len,
				 " & enable at resume enabled\n");
	} else if (ethqos->current_phy_mode == DISABLE_PHY_ON_OFF)
		len += scnprintf(buf + len, buf_len - len,
				 "Disable phy on/off disabled\n");
	else
		len += scnprintf(buf + len, buf_len - len,
					"Invalid Phy State\n");

	if (len > buf_len) {
		ETHQOSERR("(len > buf_len) buffer not sufficient\n");
		len = buf_len;
	}

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static ssize_t phy_off_config(
	struct file *file, const char __user *user_buffer,
	size_t count, loff_t *position)
{
	char *in_buf;
	int buf_len = 2000;
	unsigned long ret;
	int config = 0;
	struct qcom_ethqos *ethqos = file->private_data;

	in_buf = kzalloc(buf_len, GFP_KERNEL);
	if (!in_buf)
		return -ENOMEM;

	ret = copy_from_user(in_buf, user_buffer, buf_len);
	if (ret) {
		ETHQOSERR("unable to copy from user\n");
		return -EFAULT;
	}

	ret = sscanf(in_buf, "%d", &config);
	if (ret != 1) {
		ETHQOSERR("Error in reading option from user");
		return -EINVAL;
	}
	if (config > DISABLE_PHY_ON_OFF || config < DISABLE_PHY_IMMEDIATELY) {
		ETHQOSERR("Invalid option =%d", config);
		return -EINVAL;
	}
	if (config == ethqos->current_phy_mode) {
		ETHQOSERR("No effect as duplicate config");
		return -EPERM;
	}
	if (config == DISABLE_PHY_IMMEDIATELY) {
		ethqos->current_phy_mode = DISABLE_PHY_IMMEDIATELY;
	//make phy off
		if (ethqos->current_loopback == ENABLE_PHY_LOOPBACK) {
			/* If Phy loopback is enabled
			 *  Disabled It before phy off
			 */
			phy_digital_loopback_config(ethqos,
						    ethqos->loopback_speed, 0);
			ETHQOSDBG("Disable phy Loopback");
			ethqos->current_loopback = ENABLE_PHY_LOOPBACK;
		}
		ethqos_phy_power_off(ethqos);
	} else if (config == ENABLE_PHY_IMMEDIATELY) {
		ethqos->current_phy_mode = ENABLE_PHY_IMMEDIATELY;
		//make phy on
		ethqos_phy_power_on(ethqos);
		ethqos_reset_phy_enable_interrupt(ethqos);
		if (ethqos->current_loopback == ENABLE_PHY_LOOPBACK) {
			/*If Phy loopback is enabled , enabled It again*/
			phy_digital_loopback_config(ethqos,
						    ethqos->loopback_speed, 1);
			ETHQOSDBG("Enabling Phy loopback again");
		}
	} else if (config == DISABLE_PHY_AT_SUSPEND_ONLY) {
		ethqos->current_phy_mode = DISABLE_PHY_AT_SUSPEND_ONLY;
	} else if (config == DISABLE_PHY_SUSPEND_ENABLE_RESUME) {
		ethqos->current_phy_mode = DISABLE_PHY_SUSPEND_ENABLE_RESUME;
	} else if (config == DISABLE_PHY_ON_OFF) {
		ethqos->current_phy_mode = DISABLE_PHY_ON_OFF;
	} else {
		ETHQOSERR("Invalid option\n");
		return -EINVAL;
	}
	kfree(in_buf);
	return count;
}

static void ethqos_rgmii_io_macro_loopback(struct qcom_ethqos *ethqos, int mode)
{
	/* Set loopback mode */
	if (mode == 1) {
		rgmii_updatel(ethqos, RGMII_CONFIG2_TX_TO_RX_LOOPBACK_EN,
			      RGMII_CONFIG2_TX_TO_RX_LOOPBACK_EN,
			      RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
			      0, RGMII_IO_MACRO_CONFIG2);
	} else {
		rgmii_updatel(ethqos, RGMII_CONFIG2_TX_TO_RX_LOOPBACK_EN,
			      0, RGMII_IO_MACRO_CONFIG2);
		rgmii_updatel(ethqos, RGMII_CONFIG2_RX_PROG_SWAP,
			      RGMII_CONFIG2_RX_PROG_SWAP,
			      RGMII_IO_MACRO_CONFIG2);
	}
}

static void ethqos_mac_loopback(struct qcom_ethqos *ethqos, int mode)
{
	u32 read_value = (u32)readl_relaxed(ethqos->ioaddr + MAC_CONFIGURATION);
	/* Set loopback mode */
	if (mode == 1)
		read_value |= MAC_LM;
	else
		read_value &= ~MAC_LM;
	writel_relaxed(read_value, ethqos->ioaddr + MAC_CONFIGURATION);
}

static int phy_digital_loopback_config(
	struct qcom_ethqos *ethqos, int speed, int config)
{
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);
	int phydata = 0;

	if (config == 1) {
		ETHQOSINFO("Request for phy digital loopback enable\n");
		switch (speed) {
		case SPEED_1000:
			phydata = PHY_LOOPBACK_1000;
			break;
		case SPEED_100:
			phydata = PHY_LOOPBACK_100;
			break;
		case SPEED_10:
			phydata = PHY_LOOPBACK_10;
			break;
		default:
			ETHQOSERR("Invalid link speed\n");
			break;
		}
	} else if (config == 0) {
		ETHQOSINFO("Request for phy digital loopback disable\n");
		if (ethqos->bmcr_backup)
			phydata = ethqos->bmcr_backup;
		else
			phydata = 0x1140;
	} else {
		ETHQOSERR("Invalid option\n");
		return -EINVAL;
	}
	if (phydata != 0) {
		if (priv->phydev) {
			phy_write(priv->phydev, MII_BMCR, phydata);
			ETHQOSINFO("write done for phy loopback\n");
		} else {
			ETHQOSINFO("Phy dev is NULL\n");
		}
	}
	return 0;
}

static void print_loopback_detail(enum loopback_mode loopback)
{
	switch (loopback) {
	case DISABLE_LOOPBACK:
		ETHQOSINFO("Loopback is disabled\n");
		break;
	case ENABLE_IO_MACRO_LOOPBACK:
		ETHQOSINFO("Loopback is Enabled as IO MACRO LOOPBACK\n");
		break;
	case ENABLE_MAC_LOOPBACK:
		ETHQOSINFO("Loopback is Enabled as MAC LOOPBACK\n");
		break;
	case ENABLE_PHY_LOOPBACK:
		ETHQOSINFO("Loopback is Enabled as PHY LOOPBACK\n");
		break;
	default:
		ETHQOSINFO("Invalid Loopback=%d\n", loopback);
		break;
	}
}

static void setup_config_registers(struct qcom_ethqos *ethqos,
				   int speed, int duplex, int mode)
{
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);
	u32 ctrl = 0;

	ETHQOSDBG("Speed=%d,dupex=%d,mode=%d\n", speed, duplex, mode);

	if (mode > DISABLE_LOOPBACK && !qcom_ethqos_is_phy_link_up(ethqos)) {
		/*If Link is Down & need to enable Loopback*/
		ETHQOSDBG("Enable Lower Up Flag & disable phy dev\n");
		ETHQOSDBG("IRQ so that Rx/Tx can happen beforeee Link down\n");
		netif_carrier_on(dev);
		/*Disable phy interrupt by Link/Down by cable plug in/out*/
		disable_irq(ethqos->phy_intr);
	} else if (mode > DISABLE_LOOPBACK &&
			qcom_ethqos_is_phy_link_up(ethqos)) {
		ETHQOSDBG("Only disable phy irqqq Lin is UP\n");
		/*Since link is up no need to set Lower UP flag*/
		/*Disable phy interrupt by Link/Down by cable plug in/out*/
		disable_irq(ethqos->phy_intr);
	} else if (mode == DISABLE_LOOPBACK &&
		!qcom_ethqos_is_phy_link_up(ethqos)) {
		ETHQOSDBG("Disable Lower Up as Link is down\n");
		netif_carrier_off(dev);
		enable_irq(ethqos->phy_intr);
	}
	ETHQOSDBG("Old ctrl=%d  dupex full\n", ctrl);
	ctrl = readl_relaxed(priv->ioaddr + MAC_CTRL_REG);
		ETHQOSDBG("Old ctrl=0x%x with mask with flow control\n", ctrl);

	ctrl |= priv->hw->link.duplex;
	priv->dev->phydev->duplex = duplex;
	ctrl &= ~priv->hw->link.speed_mask;
	switch (speed) {
	case SPEED_1000:
		ctrl |= priv->hw->link.speed1000;
		break;
	case SPEED_100:
		ctrl |= priv->hw->link.speed100;
		break;
	case SPEED_10:
		ctrl |= priv->hw->link.speed10;
		break;
	default:
		speed = SPEED_UNKNOWN;
		ETHQOSDBG("unkwon speed\n");
		break;
	}
	writel_relaxed(ctrl, priv->ioaddr + MAC_CTRL_REG);
	ETHQOSDBG("New ctrl=%x priv hw speeed =%d\n", ctrl,
		  priv->hw->link.speed1000);
	priv->dev->phydev->speed = speed;
	priv->speed  = speed;

	if (mode > DISABLE_LOOPBACK && !qcom_ethqos_is_phy_link_up(ethqos)) {
		/*If Link is Down & need to enable Loopback*/
		ETHQOSDBG("Link is down . manual ipa setting up\n");
		if (priv->tx_queue[IPA_DMA_TX_CH].skip_sw)
			ethqos_ipa_offload_event_handler(priv,
							 EV_PHY_LINK_UP);
	} else if (mode == DISABLE_LOOPBACK &&
			  !qcom_ethqos_is_phy_link_up(ethqos)) {
		ETHQOSDBG("Disable request since link was down disable ipa\n");
		if (priv->tx_queue[IPA_DMA_TX_CH].skip_sw)
			ethqos_ipa_offload_event_handler(priv,
							 EV_PHY_LINK_DOWN);
	}

	if (priv->dev->phydev->speed != SPEED_UNKNOWN)
		ethqos_fix_mac_speed(ethqos, speed);

	if (mode > DISABLE_LOOPBACK) {
		if (mode == ENABLE_MAC_LOOPBACK ||
		    mode == ENABLE_IO_MACRO_LOOPBACK)
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_CONFIG_LOOPBACK_EN,
				      RGMII_IO_MACRO_CONFIG);
	} else if (mode == DISABLE_LOOPBACK) {
		if (ethqos->emac_ver == EMAC_HW_v2_3_2 ||
		    ethqos->emac_ver == EMAC_HW_v2_1_2)
			rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
				      0, RGMII_IO_MACRO_CONFIG);
	}
	ETHQOSERR("End\n");
}

static ssize_t loopback_handling_config(
	struct file *file, const char __user *user_buffer,
	size_t count, loff_t *position)
{
	char *in_buf;
	int buf_len = 2000;
	unsigned long ret;
	int config = 0;
	struct qcom_ethqos *ethqos = file->private_data;
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);
	int speed = 0;

	in_buf = kzalloc(buf_len, GFP_KERNEL);
	if (!in_buf)
		return -ENOMEM;

	ret = copy_from_user(in_buf, user_buffer, buf_len);
	if (ret) {
		ETHQOSERR("unable to copy from user\n");
		return -EFAULT;
	}

	ret = sscanf(in_buf, "%d %d", &config,  &speed);
	if (config > DISABLE_LOOPBACK && ret != 2) {
		ETHQOSERR("Speed is also needed while enabling loopback\n");
		return -EINVAL;
	}
	if (config < DISABLE_LOOPBACK || config > ENABLE_PHY_LOOPBACK) {
		ETHQOSERR("Invalid config =%d\n", config);
		return -EINVAL;
	}
	if ((config == ENABLE_PHY_LOOPBACK  || ethqos->current_loopback ==
			ENABLE_PHY_LOOPBACK) &&
			ethqos->current_phy_mode == DISABLE_PHY_IMMEDIATELY) {
		ETHQOSERR("Can't enabled/disable ");
		ETHQOSERR("phy loopback when phy is off\n");
		return -EPERM;
	}

	/*Argument validation*/
	if (config == DISABLE_LOOPBACK || config == ENABLE_IO_MACRO_LOOPBACK ||
	    config == ENABLE_MAC_LOOPBACK || config == ENABLE_PHY_LOOPBACK) {
		if (speed != SPEED_1000 && speed != SPEED_100 &&
		    speed != SPEED_10)
			return -EINVAL;
	} else {
		return -EINVAL;
	}

	if (config == ethqos->current_loopback) {
		switch (config) {
		case DISABLE_LOOPBACK:
			ETHQOSINFO("Loopback is already disabled\n");
			break;
		case ENABLE_IO_MACRO_LOOPBACK:
			ETHQOSINFO("Loopback is already Enabled as ");
			ETHQOSINFO("IO MACRO LOOPBACK\n");
			break;
		case ENABLE_MAC_LOOPBACK:
			ETHQOSINFO("Loopback is already Enabled as ");
			ETHQOSINFO("MAC LOOPBACK\n");
			break;
		case ENABLE_PHY_LOOPBACK:
			ETHQOSINFO("Loopback is already Enabled as ");
			ETHQOSINFO("PHY LOOPBACK\n");
			break;
		}
		return -EINVAL;
	}
	/*If request to enable loopback & some other loopback already enabled*/
	if (config != DISABLE_LOOPBACK &&
	    ethqos->current_loopback > DISABLE_LOOPBACK) {
		ETHQOSINFO("Loopback is already enabled\n");
		print_loopback_detail(ethqos->current_loopback);
		return -EINVAL;
	}
	ETHQOSINFO("enable loopback = %d with link speed = %d backup now\n",
		   config, speed);

	/*Backup speed & duplex before Enabling Loopback */
	if (ethqos->current_loopback == DISABLE_LOOPBACK &&
	    config > DISABLE_LOOPBACK) {
		/*Backup old speed & duplex*/
		ethqos->backup_speed = priv->speed;
		ethqos->backup_duplex = priv->dev->phydev->duplex;
	}
	/*Backup BMCR before Enabling Phy LoopbackLoopback */
	if (ethqos->current_loopback == DISABLE_LOOPBACK &&
	    config == ENABLE_PHY_LOOPBACK)
		ethqos->bmcr_backup = ethqos_mdio_read(priv,
						       priv->plat->phy_addr,
						       MII_BMCR);

	if (config == DISABLE_LOOPBACK)
		setup_config_registers(ethqos, ethqos->backup_speed,
				       ethqos->backup_duplex, 0);
	else
		setup_config_registers(ethqos, speed, DUPLEX_FULL, config);

	switch (config) {
	case DISABLE_LOOPBACK:
		ETHQOSINFO("Request to Disable Loopback\n");
		if (ethqos->current_loopback == ENABLE_IO_MACRO_LOOPBACK)
			ethqos_rgmii_io_macro_loopback(ethqos, 0);
		else if (ethqos->current_loopback == ENABLE_MAC_LOOPBACK)
			ethqos_mac_loopback(ethqos, 0);
		else if (ethqos->current_loopback == ENABLE_PHY_LOOPBACK)
			phy_digital_loopback_config(ethqos,
						    ethqos->backup_speed, 0);
		break;
	case ENABLE_IO_MACRO_LOOPBACK:
		ETHQOSINFO("Request to Enable IO MACRO LOOPBACK\n");
		ethqos_rgmii_io_macro_loopback(ethqos, 1);
		break;
	case ENABLE_MAC_LOOPBACK:
		ETHQOSINFO("Request to Enable MAC LOOPBACK\n");
		ethqos_mac_loopback(ethqos, 1);
		break;
	case ENABLE_PHY_LOOPBACK:
		ETHQOSINFO("Request to Enable PHY LOOPBACK\n");
		ethqos->loopback_speed = speed;
		phy_digital_loopback_config(ethqos, speed, 1);
		break;
	default:
		ETHQOSINFO("Invalid Loopback=%d\n", config);
		break;
	}

	ethqos->current_loopback = config;
	kfree(in_buf);
	return count;
}

static ssize_t read_loopback_config(struct file *file,
				    char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	unsigned int len = 0, buf_len = 2000;
	struct qcom_ethqos *ethqos = file->private_data;
	char *buf;
	ssize_t ret_cnt;

	if (!ethqos) {
		ETHQOSERR("NULL Pointer\n");
		return -EINVAL;
	}
	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (ethqos->current_loopback == DISABLE_LOOPBACK)
		len += scnprintf(buf + len, buf_len - len,
				 "Loopback is Disabled\n");
	else if (ethqos->current_loopback == ENABLE_IO_MACRO_LOOPBACK)
		len += scnprintf(buf + len, buf_len - len,
				 "Current Loopback is IO MACRO LOOPBACK\n");
	else if (ethqos->current_loopback == ENABLE_MAC_LOOPBACK)
		len += scnprintf(buf + len, buf_len - len,
				 "Current Loopback is MAC LOOPBACK\n");
	else if (ethqos->current_loopback == ENABLE_PHY_LOOPBACK)
		len += scnprintf(buf + len, buf_len - len,
				 "Current Loopback is PHY LOOPBACK\n");
	else
		len += scnprintf(buf + len, buf_len - len,
				 "Invalid LOOPBACK Config\n");
	if (len > buf_len)
		len = buf_len;

	ret_cnt = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret_cnt;
}

static const struct file_operations fops_phy_reg_dump = {
	.read = read_phy_reg_dump,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_rgmii_reg_dump = {
	.read = read_rgmii_reg_dump,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t write_ipc_stmmac_log_ctxt_low(struct file *file,
					     const char __user *buf,
					     size_t count, loff_t *data)
{
	int tmp = 0;

	if (count > MAX_PROC_SIZE)
		count = MAX_PROC_SIZE;
	if (copy_from_user(tmp_buff, buf, count))
		return -EFAULT;
	if (sscanf(tmp_buff, "%du", &tmp) < 0) {
		pr_err("sscanf failed\n");
	} else {
		if (tmp) {
			if (!ipc_stmmac_log_ctxt_low) {
				ipc_stmmac_log_ctxt_low =
				ipc_log_context_create(IPCLOG_STATE_PAGES,
						       "stmmac_low", 0);
			}
			if (!ipc_stmmac_log_ctxt_low) {
				pr_err("failed to create ipc stmmac low context\n");
				return -EFAULT;
			}
		} else {
			if (ipc_stmmac_log_ctxt_low) {
				ipc_log_context_destroy(
							ipc_stmmac_log_ctxt_low
						       );
		}
			ipc_stmmac_log_ctxt_low = NULL;
		}
	}

	stmmac_enable_ipc_low = tmp;
	return count;
}

static const struct file_operations fops_phy_off = {
	.read = read_phy_off,
	.write = phy_off_config,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_loopback_config = {
	.read = read_loopback_config,
	.write = loopback_handling_config,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_ipc_stmmac_log_low = {
	.write = write_ipc_stmmac_log_ctxt_low,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static int ethqos_create_debugfs(struct qcom_ethqos        *ethqos)
{
	static struct dentry *phy_reg_dump;
	static struct dentry *rgmii_reg_dump;
	static struct dentry *ipc_stmmac_log_low;
	static struct dentry *phy_off;
	static struct dentry *loopback_enable_mode;

	if (!ethqos) {
		ETHQOSERR("Null Param %s\n", __func__);
		return -ENOMEM;
	}

	ethqos->debugfs_dir = debugfs_create_dir("eth", NULL);

	if (!ethqos->debugfs_dir || IS_ERR(ethqos->debugfs_dir)) {
		ETHQOSERR("Can't create debugfs dir\n");
		return -ENOMEM;
	}

	phy_reg_dump = debugfs_create_file("phy_reg_dump", 0400,
					   ethqos->debugfs_dir, ethqos,
					   &fops_phy_reg_dump);
	if (!phy_reg_dump || IS_ERR(phy_reg_dump)) {
		ETHQOSERR("Can't create phy_dump %d\n", (int)phy_reg_dump);
		goto fail;
	}

	rgmii_reg_dump = debugfs_create_file("rgmii_reg_dump", 0400,
					     ethqos->debugfs_dir, ethqos,
					     &fops_rgmii_reg_dump);
	if (!rgmii_reg_dump || IS_ERR(rgmii_reg_dump)) {
		ETHQOSERR("Can't create rgmii_dump %d\n", (int)rgmii_reg_dump);
		goto fail;
	}

	ipc_stmmac_log_low = debugfs_create_file("ipc_stmmac_log_low", 0220,
						 ethqos->debugfs_dir, ethqos,
						 &fops_ipc_stmmac_log_low);
	if (!ipc_stmmac_log_low || IS_ERR(ipc_stmmac_log_low)) {
		ETHQOSERR("Cannot create debugfs ipc_stmmac_log_low %x\n",
			  ipc_stmmac_log_low);
		goto fail;
	}
	phy_off = debugfs_create_file("phy_off", 0400,
				      ethqos->debugfs_dir, ethqos,
				      &fops_phy_off);
	if (!phy_off || IS_ERR(phy_off)) {
		ETHQOSERR("Can't create phy_off %x\n", phy_off);
		goto fail;
	}

	loopback_enable_mode = debugfs_create_file("loopback_enable_mode", 0400,
						   ethqos->debugfs_dir, ethqos,
						   &fops_loopback_config);
	if (!loopback_enable_mode || IS_ERR(loopback_enable_mode)) {
		ETHQOSERR("Can't create loopback_enable_mode %d\n",
			  (int)loopback_enable_mode);
		goto fail;
	}
	return 0;

fail:
	debugfs_remove_recursive(ethqos->debugfs_dir);
	return -ENOMEM;
}

static void ethqos_emac_mem_base(struct qcom_ethqos *ethqos)
{
	struct resource *resource = NULL;
	int ret = 0;

	resource = platform_get_resource(ethqos->pdev, IORESOURCE_MEM, 0);
	if (!resource) {
		ETHQOSERR("get emac-base resource failed\n");
		ret = -ENODEV;
		return;
	}
	ethqos->emac_mem_base = resource->start;
	ethqos->emac_mem_size = resource_size(resource);
}

static void stmmac_emb_smmu_exit(void)
{
	if (stmmac_emb_smmu_ctx.valid) {
		if (stmmac_emb_smmu_ctx.smmu_pdev)
			arm_iommu_detach_device
			(&stmmac_emb_smmu_ctx.smmu_pdev->dev);
		if (stmmac_emb_smmu_ctx.mapping)
			arm_iommu_release_mapping(stmmac_emb_smmu_ctx.mapping);
		stmmac_emb_smmu_ctx.valid = false;
		stmmac_emb_smmu_ctx.mapping = NULL;
		stmmac_emb_smmu_ctx.pdev_master = NULL;
		stmmac_emb_smmu_ctx.smmu_pdev = NULL;
	}
}

static int stmmac_emb_smmu_cb_probe(struct platform_device *pdev)
{
	int result;
	u32 iova_ap_mapping[2];
	struct device *dev = &pdev->dev;
	int atomic_ctx = 1;
	int fast = 1;
	int bypass = 1;
	struct iommu_domain_geometry geometry = {0};

	ETHQOSDBG("EMAC EMB SMMU CB probe: smmu pdev=%p\n", pdev);

	result = of_property_read_u32_array(dev->of_node, "qcom,iova-mapping",
					    iova_ap_mapping, 2);
	if (result) {
		ETHQOSERR("Failed to read EMB start/size iova addresses\n");
		return result;
	}
	stmmac_emb_smmu_ctx.va_start = iova_ap_mapping[0];
	stmmac_emb_smmu_ctx.va_size = iova_ap_mapping[1];
	stmmac_emb_smmu_ctx.va_end = stmmac_emb_smmu_ctx.va_start +
				   stmmac_emb_smmu_ctx.va_size;

	geometry.aperture_start = stmmac_emb_smmu_ctx.va_start;
	geometry.aperture_end =
	stmmac_emb_smmu_ctx.va_start + stmmac_emb_smmu_ctx.va_size;

	stmmac_emb_smmu_ctx.smmu_pdev = pdev;

	if (dma_set_mask(dev, DMA_BIT_MASK(32)) ||
	    dma_set_coherent_mask(dev, DMA_BIT_MASK(32))) {
		ETHQOSERR("DMA set 32bit mask failed\n");
		return -EOPNOTSUPP;
	}

	stmmac_emb_smmu_ctx.mapping = arm_iommu_create_mapping
	(dev->bus, stmmac_emb_smmu_ctx.va_start, stmmac_emb_smmu_ctx.va_size);
	if (IS_ERR_OR_NULL(stmmac_emb_smmu_ctx.mapping)) {
		ETHQOSDBG("Fail to create mapping\n");
		/* assume this failure is because iommu driver is not ready */
		return -EPROBE_DEFER;
	}
	ETHQOSDBG("Successfully Created SMMU mapping\n");
	stmmac_emb_smmu_ctx.valid = true;

	if (of_property_read_bool(dev->of_node, "qcom,smmu-s1-bypass")) {
		if (iommu_domain_set_attr(stmmac_emb_smmu_ctx.mapping->domain,
					  DOMAIN_ATTR_S1_BYPASS,
					  &bypass)) {
			ETHQOSERR("Couldn't set SMMU S1 bypass\n");
			result = -EIO;
			goto err_smmu_probe;
		}
		ETHQOSDBG("SMMU S1 BYPASS set\n");
	} else {
		if (iommu_domain_set_attr(stmmac_emb_smmu_ctx.mapping->domain,
					  DOMAIN_ATTR_ATOMIC,
					  &atomic_ctx)) {
			ETHQOSERR("Couldn't set SMMU domain as atomic\n");
			result = -EIO;
			goto err_smmu_probe;
		}
		ETHQOSDBG("SMMU atomic set\n");
		if (iommu_domain_set_attr(stmmac_emb_smmu_ctx.mapping->domain,
					  DOMAIN_ATTR_FAST,
					  &fast)) {
			ETHQOSERR("Couldn't set FAST SMMU\n");
			result = -EIO;
			goto err_smmu_probe;
		}
		ETHQOSDBG("SMMU fast map set\n");
		if (of_property_read_bool(dev->of_node,
					  "qcom,smmu-geometry")) {
			if (iommu_domain_set_attr
			    (stmmac_emb_smmu_ctx.mapping->domain,
			     DOMAIN_ATTR_GEOMETRY,
			     &geometry)) {
				ETHQOSERR("Couldn't set DOMAIN_ATTR_GEOMETRY");
				result = -EIO;
				goto err_smmu_probe;
			}
			ETHQOSDBG("SMMU DOMAIN_ATTR_GEOMETRY set\n");
		}

	}

	result = arm_iommu_attach_device(&stmmac_emb_smmu_ctx.smmu_pdev->dev,
					 stmmac_emb_smmu_ctx.mapping);
	if (result) {
		ETHQOSERR("couldn't attach to IOMMU ret=%d\n", result);
		goto err_smmu_probe;
	}

	stmmac_emb_smmu_ctx.iommu_domain =
		iommu_get_domain_for_dev(&stmmac_emb_smmu_ctx.smmu_pdev->dev);

	ETHQOSDBG("Successfully attached to IOMMU\n");
	if (stmmac_emb_smmu_ctx.pdev_master)
		goto smmu_probe_done;

err_smmu_probe:
	if (stmmac_emb_smmu_ctx.mapping)
		arm_iommu_release_mapping(stmmac_emb_smmu_ctx.mapping);
	stmmac_emb_smmu_ctx.valid = false;

smmu_probe_done:
	stmmac_emb_smmu_ctx.ret = result;
	return result;
}

static int ethqos_update_rgmii_tx_drv_strength(struct qcom_ethqos *ethqos)
{
	int ret = 0;
	struct resource *resource = NULL;
	unsigned long tlmm_central_base = 0;
	unsigned long tlmm_central_size = 0;
	unsigned long reg_rgmii_io_pads_voltage = 0;

	resource =
	 platform_get_resource_byname(
	    ethqos->pdev, IORESOURCE_MEM, "tlmm-central-base");

	if (!resource) {
		ETHQOSERR("Resource tlmm-central-base not found\n");
		goto err_out;
	}

	tlmm_central_base = resource->start;
	tlmm_central_size = resource_size(resource);
	ETHQOSDBG("tlmm_central_base = 0x%x, size = 0x%x\n",
		  tlmm_central_base, tlmm_central_size);

	tlmm_central_base_addr = ioremap(
	   tlmm_central_base, tlmm_central_size);
	if (!tlmm_central_base_addr) {
		ETHQOSERR("cannot map dwc_tlmm_central reg memory, aborting\n");
		ret = -EIO;
		goto err_out;
	}

	ETHQOSDBG("dwc_tlmm_central = %#lx\n", tlmm_central_base_addr);

	if (ethqos->emac_ver != EMAC_HW_v2_1_2) {
		reg_rgmii_io_pads_voltage =
		regulator_get_voltage(ethqos->reg_rgmii_io_pads);
	}

	ETHQOSINFO("IOMACRO pads voltage: %u uV\n", reg_rgmii_io_pads_voltage);

	switch (reg_rgmii_io_pads_voltage) {
	case 1500000:
	case 1800000: {
		switch (ethqos->emac_ver) {
		case EMAC_HW_v2_0_0:
		case EMAC_HW_v2_2_0:
		case EMAC_HW_v2_3_2: {
				TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_WR(
				   TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_16MA,
				   TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_16MA,
				   TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_16MA);
				TLMM_RGMII_RX_HV_MODE_CTL_RGWR(0x0);
		}
		break;
		default:
		break;
		}
	}
	break;
	default:
	break;
	}

err_out:
	if (tlmm_central_base_addr)
		iounmap(tlmm_central_base_addr);

	return ret;
}

static void qcom_ethqos_phy_suspend_clks(struct qcom_ethqos *ethqos)
{
	struct stmmac_priv *priv = qcom_ethqos_get_priv(ethqos);

	ETHQOSINFO("Enter\n");

	if (phy_intr_en)
		reinit_completion(&ethqos->clk_enable_done);

	ethqos->clks_suspended = 1;

	ethqos_update_rgmii_clk_and_bus_cfg(ethqos, 0);

	if (priv->plat->stmmac_clk)
		clk_disable_unprepare(priv->plat->stmmac_clk);

	if (priv->plat->pclk)
		clk_disable_unprepare(priv->plat->pclk);

	if (priv->plat->clk_ptp_ref)
		clk_disable_unprepare(priv->plat->clk_ptp_ref);

	if (ethqos->rgmii_clk)
		clk_disable_unprepare(ethqos->rgmii_clk);

	ETHQOSINFO("Exit\n");
}

inline void *qcom_ethqos_get_priv(struct qcom_ethqos *ethqos)
{
	struct platform_device *pdev = ethqos->pdev;
	struct net_device *dev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(dev);

	return priv;
}

inline bool qcom_ethqos_is_phy_link_up(struct qcom_ethqos *ethqos)
{
	/* PHY driver initializes phydev->link=1.
	 * So, phydev->link is 1 even on bootup with no PHY connected.
	 * phydev->link is valid only after adjust_link is called once.
	 */
	struct stmmac_priv *priv = qcom_ethqos_get_priv(ethqos);

	if (priv->plat->mac2mac_en) {
		return true;
	} else {
		return ((priv->oldlink != -1) &&
			(priv->dev->phydev &&
			priv->dev->phydev->link));
	}
}

static void qcom_ethqos_phy_resume_clks(struct qcom_ethqos *ethqos)
{
	struct stmmac_priv *priv = qcom_ethqos_get_priv(ethqos);

	ETHQOSINFO("Enter\n");

	if (priv->plat->stmmac_clk)
		clk_prepare_enable(priv->plat->stmmac_clk);

	if (priv->plat->pclk)
		clk_prepare_enable(priv->plat->pclk);

	if (priv->plat->clk_ptp_ref)
		clk_prepare_enable(priv->plat->clk_ptp_ref);

	if (ethqos->rgmii_clk)
		clk_prepare_enable(ethqos->rgmii_clk);

	if (qcom_ethqos_is_phy_link_up(ethqos))
		ethqos_update_rgmii_clk_and_bus_cfg(ethqos, ethqos->speed);
	else
		ethqos_update_rgmii_clk_and_bus_cfg(ethqos, SPEED_10);

	ethqos->clks_suspended = 0;

	if (phy_intr_en)
		complete_all(&ethqos->clk_enable_done);

	ETHQOSINFO("Exit\n");
}

static void qcom_ethqos_bringup_iface(struct work_struct *work)
{
	struct platform_device *pdev = NULL;
	struct net_device *ndev = NULL;
	struct qcom_ethqos *ethqos =
		container_of(work, struct qcom_ethqos, early_eth);

	ETHQOSINFO("entry\n");

	if (!ethqos)
		return;

	pdev = ethqos->pdev;
	if (!pdev)
		return;

	ndev = platform_get_drvdata(pdev);

	if (!ndev || netif_running(ndev))
		return;

	rtnl_lock();

	if (dev_change_flags(ndev, ndev->flags | IFF_UP) < 0)
		ETHQOSINFO("ERROR\n");

	rtnl_unlock();

	ETHQOSINFO("exit\n");
}

void qcom_ethqos_request_phy_wol(struct plat_stmmacenet_data *plat)
{
	struct qcom_ethqos *ethqos = plat->bsp_priv;
	struct stmmac_priv *priv = qcom_ethqos_get_priv(ethqos);

	ethqos->phy_wol_supported = 0;
	ethqos->phy_wol_wolopts = 0;
	/* Check if phydev is valid*/
	/* Check and enable Wake-on-LAN functionality in PHY*/

	if (priv->phydev) {
		struct ethtool_wolinfo wol = {.cmd = ETHTOOL_GWOL};

		wol.supported = 0;
		wol.wolopts = 0;
		ETHQOSDBG("phydev addr: %x\n", priv->phydev);
		phy_ethtool_get_wol(priv->phydev, &wol);
		ethqos->phy_wol_supported = wol.supported;
		ETHQOSDBG("Get WoL[0x%x] in %s\n", wol.supported,
			  priv->phydev->drv->name);

	/* Try to enable supported Wake-on-LAN features in PHY*/
		if (wol.supported) {
			device_set_wakeup_capable(&ethqos->pdev->dev, 1);

			wol.cmd = ETHTOOL_SWOL;
			wol.wolopts = wol.supported;

			if (!phy_ethtool_set_wol(priv->phydev, &wol)) {
				ethqos->phy_wol_wolopts = wol.wolopts;

				enable_irq_wake(ethqos->phy_intr);
				device_set_wakeup_enable(&ethqos->pdev->dev, 1);

				ETHQOSDBG("Enabled WoL[0x%x] in %s\n",
					  wol.wolopts,
					  priv->phydev->drv->name);
			} else {
				ETHQOSINFO("Disabled WoL[0x%x] in %s\n",
					   wol.wolopts,
					   priv->phydev->drv->name);
			}
		}
	}
}

static void ethqos_is_ipv4_NW_stack_ready(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct qcom_ethqos *ethqos;
	struct platform_device *pdev = NULL;
	struct net_device *ndev = NULL;
	int ret;

	ETHQOSDBG("Enter\n");
	dwork = container_of(work, struct delayed_work, work);
	ethqos = container_of(dwork, struct qcom_ethqos, ipv4_addr_assign_wq);

	if (!ethqos)
		return;

	pdev = ethqos->pdev;

	if (!pdev)
		return;

	ndev = platform_get_drvdata(pdev);

	ret = qcom_ethqos_add_ipaddr(&pparams, ndev);
	if (ret)
		return;

	cancel_delayed_work_sync(&ethqos->ipv4_addr_assign_wq);
	flush_delayed_work(&ethqos->ipv4_addr_assign_wq);
}

static void ethqos_is_ipv6_NW_stack_ready(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct qcom_ethqos *ethqos;
	struct platform_device *pdev = NULL;
	struct net_device *ndev = NULL;
	int ret;

	ETHQOSDBG("Enter\n");
	dwork = container_of(work, struct delayed_work, work);
	ethqos = container_of(dwork, struct qcom_ethqos, ipv6_addr_assign_wq);

	if (!ethqos)
		return;

	pdev = ethqos->pdev;

	if (!pdev)
		return;

	ndev = platform_get_drvdata(pdev);

	ret = qcom_ethqos_add_ipv6addr(&pparams, ndev);
	if (ret)
		return;

	cancel_delayed_work_sync(&ethqos->ipv6_addr_assign_wq);
	flush_delayed_work(&ethqos->ipv6_addr_assign_wq);
}

static void ethqos_set_early_eth_param(
				struct stmmac_priv *priv,
				struct qcom_ethqos *ethqos)
{
	int ret = 0;

	if (priv->plat && priv->plat->mdio_bus_data)
		priv->plat->mdio_bus_data->phy_mask =
		 priv->plat->mdio_bus_data->phy_mask | DUPLEX_FULL | SPEED_100;

	priv->plat->max_speed = SPEED_100;

	if (pparams.is_valid_ipv4_addr) {
		INIT_DELAYED_WORK(&ethqos->ipv4_addr_assign_wq,
				  ethqos_is_ipv4_NW_stack_ready);
		ret = qcom_ethqos_add_ipaddr(&pparams, priv->dev);
		if (ret)
			schedule_delayed_work(&ethqos->ipv4_addr_assign_wq,
					      msecs_to_jiffies(1000));
	}

	if (pparams.is_valid_ipv6_addr) {
		INIT_DELAYED_WORK(&ethqos->ipv6_addr_assign_wq,
				  ethqos_is_ipv6_NW_stack_ready);
		ret = qcom_ethqos_add_ipv6addr(&pparams, priv->dev);
		if (ret)
			schedule_delayed_work(&ethqos->ipv6_addr_assign_wq,
					      msecs_to_jiffies(1000));
	}
	return;
}

bool qcom_ethqos_ipa_enabled(void)
{
#ifdef CONFIG_ETH_IPA_OFFLOAD
	return pethqos->ipa_enabled;
#endif
	return false;
}

static ssize_t ethqos_read_dev_emac(struct file *filp, char __user *buf,
				    size_t count, loff_t *f_pos)
{
	unsigned int len = 0;
	char *temp_buf;
	ssize_t ret_cnt = 0;

	ret_cnt = simple_read_from_buffer(buf, count, f_pos, temp_buf, len);
	return ret_cnt;
}

static ssize_t ethqos_write_dev_emac(struct file *file,
				     const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	unsigned char in_buf[300] = {0};
	unsigned long ret;
	struct qcom_ethqos *ethqos = pethqos;
	struct stmmac_priv *priv = qcom_ethqos_get_priv(pethqos);
	struct vlan_filter_info vlan_filter_info;
	char mac_str[30] = {0};
	char vlan_str[30] = {0};
	char *prefix = NULL;
	u32 err;
	unsigned int number;

	if (sizeof(in_buf) < count) {
		ETHQOSERR("emac string is too long - count=%u\n", count);
		return -EFAULT;
	}
	memset(in_buf, 0,  sizeof(in_buf));
	ret = copy_from_user(in_buf, user_buf, count);

	if (ret)
		return -EFAULT;

	strlcpy(vlan_str, in_buf, sizeof(vlan_str));

	ETHQOSINFO("emac string is %s\n", vlan_str);

	if (strnstr(vlan_str, "QOE", sizeof(vlan_str))) {
		ethqos->qoe_vlan.available = true;
		vlan_filter_info.vlan_id = ethqos->qoe_vlan.vlan_id;
		vlan_filter_info.rx_queue = ethqos->qoe_vlan.rx_queue;
		vlan_filter_info.vlan_offset = ethqos->qoe_vlan.vlan_offset;
		priv->hw->mac->set_vlan(&vlan_filter_info, priv->ioaddr);
	}

	if (strnstr(vlan_str, "qvlanid=", sizeof(vlan_str))) {
		prefix = strnchr(vlan_str,
				 strlen(vlan_str), '=');
		ETHQOSINFO("vlanid data written is %s\n", prefix + 1);
		if (prefix) {
			err = kstrtouint(prefix + 1, 0, &number);
			if (!err)
				ethqos->qoe_vlan.vlan_id = number;
		}
	}

	if (strnstr(vlan_str, "Cv2X", strlen(vlan_str))) {
		ETHQOSDBG("Cv2X supported mode is %u\n", ethqos->cv2x_mode);
		ethqos->cv2x_vlan.available = true;
		vlan_filter_info.vlan_id = ethqos->cv2x_vlan.vlan_id;
		vlan_filter_info.rx_queue = ethqos->cv2x_vlan.rx_queue;
		vlan_filter_info.vlan_offset = ethqos->cv2x_vlan.vlan_offset;
		priv->hw->mac->set_vlan(&vlan_filter_info, priv->ioaddr);
	}

	if (strnstr(vlan_str, "cvlanid=", strlen(vlan_str))) {
		prefix = strnchr(vlan_str, strlen(vlan_str), '=');
		ETHQOSDBG("Cv2X vlanid data written is %s\n", prefix + 1);
		if (prefix) {
			err = kstrtouint(prefix + 1, 0, &number);
			if (!err)
				ethqos->cv2x_vlan.vlan_id = number;
		}
	}

	if (strnstr(in_buf, "cmac_id=", strlen(in_buf))) {
		prefix = strnchr(in_buf, strlen(in_buf), '=');
		if (prefix) {
			memcpy(mac_str, (char *)prefix + 1, 30);

			if (!mac_pton(mac_str, config_dev_addr)) {
				ETHQOSERR("Invalid mac addr in /dev/emac\n");
				return count;
			}

			if (!is_valid_ether_addr(config_dev_addr)) {
				ETHQOSERR("Invalid/Multcast mac addr found\n");
				return count;
			}

			ether_addr_copy(dev_addr, config_dev_addr);
			memcpy(ethqos->cv2x_dev_addr, dev_addr, ETH_ALEN);
		}
	}

	return count;
}

static void ethqos_get_qoe_dt(struct qcom_ethqos *ethqos,
			      struct device_node *np)
{
	int res;

	res = of_property_read_u32(np, "qcom,qoe_mode", &ethqos->qoe_mode);
	if (res) {
		ETHQOSDBG("qoe_mode not in dtsi\n");
		ethqos->qoe_mode = 0;
	}

	if (ethqos->qoe_mode) {
		res = of_property_read_u32(np, "qcom,qoe-queue",
					   &ethqos->qoe_vlan.rx_queue);
		if (res) {
			ETHQOSERR("qoe-queue not in dtsi for qoe_mode %u\n",
				  ethqos->qoe_mode);
			ethqos->qoe_vlan.rx_queue = QMI_TAG_TX_CHANNEL;
		}

		res = of_property_read_u32(np, "qcom,qoe-vlan-offset",
					   &ethqos->qoe_vlan.vlan_offset);
		if (res) {
			ETHQOSERR("qoe-vlan-offset not in dtsi\n");
			ethqos->qoe_vlan.vlan_offset = 0;
		}
	}
}

static const struct file_operations emac_fops = {
	.owner = THIS_MODULE,
	.read = ethqos_read_dev_emac,
	.write = ethqos_write_dev_emac,
};

static int ethqos_create_emac_device_node(dev_t *emac_dev_t,
					  struct cdev **emac_cdev,
					  struct class **emac_class,
					  char *emac_dev_node_name)
{
	int ret;

	ret = alloc_chrdev_region(emac_dev_t, 0, 1,
				  emac_dev_node_name);
	if (ret) {
		ETHQOSERR("alloc_chrdev_region error for node %s\n",
			  emac_dev_node_name);
		goto alloc_chrdev1_region_fail;
	}

	*emac_cdev = cdev_alloc();
	if (!*emac_cdev) {
		ret = -ENOMEM;
		ETHQOSERR("failed to alloc cdev\n");
		goto fail_alloc_cdev;
	}
	cdev_init(*emac_cdev, &emac_fops);

	ret = cdev_add(*emac_cdev, *emac_dev_t, 1);
	if (ret < 0) {
		ETHQOSERR(":cdev_add err=%d\n", -ret);
		goto cdev1_add_fail;
	}

	*emac_class = class_create(THIS_MODULE, emac_dev_node_name);
	if (!*emac_class) {
		ret = -ENODEV;
		ETHQOSERR("failed to create class\n");
		goto fail_create_class;
	}

	if (!device_create(*emac_class, NULL,
			   *emac_dev_t, NULL, emac_dev_node_name)) {
		ret = -EINVAL;
		ETHQOSERR("failed to create device_create\n");
		goto fail_create_device;
	}

	return 0;

fail_create_device:
	class_destroy(*emac_class);
fail_create_class:
	cdev_del(*emac_cdev);
cdev1_add_fail:
fail_alloc_cdev:
	unregister_chrdev_region(*emac_dev_t, 1);
alloc_chrdev1_region_fail:
		return ret;
}

static void ethqos_get_cv2x_dt(struct qcom_ethqos *ethqos,
			       struct device_node *np)
{
	int res;

	res = of_property_read_u32(np, "qcom,cv2x_mode", &ethqos->cv2x_mode);
	if (res) {
		ETHQOSDBG("cv2x_mode not in dtsi\n");
		ethqos->cv2x_mode = CV2X_MODE_DISABLE;
	}

	if (ethqos->cv2x_mode != CV2X_MODE_DISABLE) {
		res = of_property_read_u32(np, "qcom,cv2x-queue",
					   &ethqos->cv2x_vlan.rx_queue);
		if (res) {
			ETHQOSERR("cv2x-queue not in dtsi for cv2x_mode %u\n",
				  ethqos->cv2x_mode);
			ethqos->cv2x_vlan.rx_queue = CV2X_TAG_TX_CHANNEL;
		}

		res = of_property_read_u32(np, "qcom,cv2x-vlan-offset",
					   &ethqos->cv2x_vlan.vlan_offset);
		if (res) {
			ETHQOSERR("cv2x-vlan-offset not in dtsi\n");
			ethqos->cv2x_vlan.vlan_offset = 1;
		}
	}
}

static int qcom_ethqos_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *io_macro_node = NULL;
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;
	struct qcom_ethqos *ethqos;
	struct resource *res;
	struct net_device *ndev;
	struct stmmac_priv *priv;
	int ret, i;

	if (of_device_is_compatible(pdev->dev.of_node,
				    "qcom,emac-smmu-embedded"))
		return stmmac_emb_smmu_cb_probe(pdev);

#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	place_marker("M - Ethernet probe start");
#endif

	ipc_stmmac_log_ctxt = ipc_log_context_create(IPCLOG_STATE_PAGES,
						     "emac", 0);
	if (!ipc_stmmac_log_ctxt)
		ETHQOSERR("Error creating logging context for emac\n");
	else
		ETHQOSDBG("IPC logging has been enabled for emac\n");
	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	ethqos = devm_kzalloc(&pdev->dev, sizeof(*ethqos), GFP_KERNEL);
	if (!ethqos) {
		ret = -ENOMEM;
		goto err_mem;
	}

	ethqos->pdev = pdev;

	ethqos_init_reqgulators(ethqos);
	ethqos_init_gpio(ethqos);

	ethqos_get_qoe_dt(ethqos, np);
	ethqos_get_cv2x_dt(ethqos, np);

	plat_dat = stmmac_probe_config_dt(pdev, &stmmac_res.mac);
	if (IS_ERR(plat_dat)) {
		dev_err(&pdev->dev, "dt configuration failed\n");
		return PTR_ERR(plat_dat);
	}

	if (ethqos->cv2x_mode == CV2X_MODE_MDM ||
	    ethqos->cv2x_mode == CV2X_MODE_AP) {
		for (i = 0; i < plat_dat->rx_queues_to_use; i++) {
			if (plat_dat->rx_queues_cfg[i].pkt_route ==
			    PACKET_AVCPQ)
				plat_dat->rx_queues_cfg[i].pkt_route = 0;
		}
	}

	if (plat_dat->tx_sched_algorithm == MTL_TX_ALGORITHM_WFQ ||
	    plat_dat->tx_sched_algorithm == MTL_TX_ALGORITHM_DWRR) {
		ETHQOSERR("WFO and DWRR TX Algorithm is not supported\n");
		ETHQOSDBG("Set TX Algorithm to default WRR\n");
		plat_dat->tx_sched_algorithm = MTL_TX_ALGORITHM_WRR;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rgmii");
	ethqos->rgmii_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ethqos->rgmii_base)) {
		dev_err(&pdev->dev, "Can't get rgmii base\n");
		ret = PTR_ERR(ethqos->rgmii_base);
		goto err_mem;
	}

	ethqos->rgmii_clk = devm_clk_get(&pdev->dev, "rgmii");
	if (!ethqos->rgmii_clk) {
		ret = -ENOMEM;
		goto err_mem;
	}

	ret = clk_prepare_enable(ethqos->rgmii_clk);
	if (ret)
		goto err_mem;

	/*Initialize Early ethernet to false*/
	ethqos->early_eth_enabled = false;

	/*Check for valid mac, ip address to enable Early eth*/
	if (pparams.is_valid_mac_addr &&
	    (pparams.is_valid_ipv4_addr || pparams.is_valid_ipv6_addr)) {
		/* For 1000BASE-T mode, auto-negotiation is required and
		 * always used to establish a link.
		 * Configure phy and MAC in 100Mbps mode with autoneg
		 * disable as link up takes more time with autoneg
		 * enabled.
		 */
		ethqos->early_eth_enabled = 1;
		ETHQOSINFO("Early ethernet is enabled\n");
	}

	ethqos->speed = SPEED_10;
	ethqos_update_rgmii_clk_and_bus_cfg(ethqos, SPEED_10);
	ethqos_set_func_clk_en(ethqos);
	if (ethqos->emac_ver == EMAC_HW_v2_0_0)
		ethqos->disable_ctile_pc = 1;

	plat_dat->bsp_priv = ethqos;
	plat_dat->fix_mac_speed = ethqos_fix_mac_speed;
	plat_dat->tx_select_queue = dwmac_qcom_select_queue;
	plat_dat->get_plat_tx_coal_frames =  dwmac_qcom_get_plat_tx_coal_frames;
	plat_dat->has_gmac4 = 1;
	plat_dat->pmt = 1;
	plat_dat->tso_en = of_property_read_bool(np, "snps,tso");
	plat_dat->early_eth = ethqos->early_eth_enabled;

	/* Get rgmii interface speed for mac2c from device tree */
	if (of_property_read_u32(np, "mac2mac-rgmii-speed",
				 &plat_dat->mac2mac_rgmii_speed))
		plat_dat->mac2mac_rgmii_speed = -1;
	else
		ETHQOSINFO("mac2mac rgmii speed = %d\n",
			   plat_dat->mac2mac_rgmii_speed);

	if (of_property_read_bool(pdev->dev.of_node, "qcom,arm-smmu")) {
		stmmac_emb_smmu_ctx.pdev_master = pdev;
		ret = of_platform_populate(pdev->dev.of_node,
					   qcom_ethqos_match, NULL, &pdev->dev);
		if (ret)
			ETHQOSERR("Failed to populate EMAC platform\n");
		if (stmmac_emb_smmu_ctx.ret) {
			ETHQOSERR("smmu probe failed\n");
			of_platform_depopulate(&pdev->dev);
			ret = stmmac_emb_smmu_ctx.ret;
			stmmac_emb_smmu_ctx.ret = 0;
		}
	}

	if (of_property_read_bool(pdev->dev.of_node,
				  "emac-core-version")) {
		/* Read emac core version value from dtsi */
		ret = of_property_read_u32(pdev->dev.of_node,
					   "emac-core-version",
					   &ethqos->emac_ver);
		if (ret) {
			ETHQOSDBG(":resource emac-hw-ver! not in dtsi\n");
			ethqos->emac_ver = EMAC_HW_NONE;
			WARN_ON(1);
		}
	} else {
		ethqos->emac_ver =
		rgmii_readl(ethqos, EMAC_I0_EMAC_CORE_HW_VERSION_RGOFFADDR);
	}
	ETHQOSDBG(": emac_core_version = %d\n", ethqos->emac_ver);

	if (of_property_read_bool(pdev->dev.of_node,
				  "emac-phy-off-suspend")) {
		/* Read emac core version value from dtsi */
		ret = of_property_read_u32(pdev->dev.of_node,
					   "emac-phy-off-suspend",
					   &ethqos->current_phy_mode);
		if (ret) {
			ETHQOSDBG(":resource emac-phy-off-suspend! ");
			ETHQOSDBG("not in dtsi\n");
			ethqos->current_phy_mode = 0;
		}
	}
	ETHQOSINFO("emac-phy-off-suspend = %d\n",
		   ethqos->current_phy_mode);

	io_macro_node = of_find_node_by_name(pdev->dev.of_node,
					     "io-macro-info");

	if (ethqos->emac_ver == EMAC_HW_v2_3_2 ||
	    ethqos->emac_ver == EMAC_HW_v2_1_2) {
		ethqos->io_macro.rx_prog_swap = 1;
	} else if (!io_macro_node) {
		ethqos->io_macro.rx_prog_swap = 0;
	} else {
		if (of_property_read_bool(io_macro_node, "rx-prog-swap"))
			ethqos->io_macro.rx_prog_swap = 1;
	}

	if (io_macro_node) {
		if (of_property_read_bool(io_macro_node, "rx-dll-bypass"))
			ethqos->io_macro.rx_dll_bypass = 1;
	}

	ethqos->ioaddr = (&stmmac_res)->addr;
	ethqos_update_rgmii_tx_drv_strength(ethqos);

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	if (ret)
		goto err_clk;

	rgmii_dump(ethqos);

	if (ethqos->emac_ver == EMAC_HW_v2_3_2) {
		ethqos_pps_irq_config(ethqos);
		create_pps_interrupt_device_node(&ethqos->avb_class_a_dev_t,
						 &ethqos->avb_class_a_cdev,
						 &ethqos->avb_class_a_class,
						 AVB_CLASS_A_POLL_DEV_NODE);

		create_pps_interrupt_device_node(&ethqos->avb_class_b_dev_t,
						 &ethqos->avb_class_b_cdev,
						 &ethqos->avb_class_b_class,
						 AVB_CLASS_B_POLL_DEV_NODE);
	}

	if (ethqos->disable_ctile_pc && !qcom_ethqos_qmp_mailbox_init(ethqos)) {
		INIT_WORK(&ethqos->qmp_mailbox_work,
			  qcom_ethqos_qmp_mailbox_work);
		queue_work(system_wq, &ethqos->qmp_mailbox_work);
	}
	ethqos_emac_mem_base(ethqos);
	pethqos = ethqos;
	ethqos_create_debugfs(ethqos);

	qcom_ethqos_read_iomacro_por_values(ethqos);

	ndev = dev_get_drvdata(&ethqos->pdev->dev);
	priv = netdev_priv(ndev);

	if (pparams.is_valid_mac_addr) {
		ether_addr_copy(dev_addr, pparams.mac_addr);
		memcpy(priv->dev->dev_addr, dev_addr, ETH_ALEN);
	}

	if (ethqos->early_eth_enabled) {
		/* Initialize work*/
		INIT_WORK(&ethqos->early_eth,
			  qcom_ethqos_bringup_iface);
		/* Queue the work*/
		queue_work(system_wq, &ethqos->early_eth);
		/*Set early eth parameters*/
		ethqos_set_early_eth_param(priv, ethqos);
	}

	if (ethqos->cv2x_mode)
		for (i = 0; i < plat_dat->rx_queues_to_use; i++)
			priv->rx_queue[i].en_fep = true;

	if (ethqos->qoe_mode || ethqos->cv2x_mode) {
		ethqos_create_emac_device_node(&ethqos->emac_dev_t,
					       &ethqos->emac_cdev,
					       &ethqos->emac_class,
					       "emac");
	}
#ifdef CONFIG_ETH_IPA_OFFLOAD
	ethqos->ipa_enabled = true;
	priv->rx_queue[IPA_DMA_RX_CH].skip_sw = true;
	priv->tx_queue[IPA_DMA_TX_CH].skip_sw = true;
	ethqos_ipa_offload_event_handler(ethqos, EV_PROBE_INIT);
	priv->hw->mac->map_mtl_to_dma(priv->hw, 0, 1); //change
#endif

#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	place_marker("M - Ethernet probe end");
#endif
	return ret;

err_clk:
	clk_disable_unprepare(ethqos->rgmii_clk);

err_mem:
	stmmac_remove_config_dt(pdev, plat_dat);

	return ret;
}

static int qcom_ethqos_remove(struct platform_device *pdev)
{
	struct qcom_ethqos *ethqos;
	int ret;

	ethqos = get_stmmac_bsp_priv(&pdev->dev);
	if (!ethqos)
		return -ENODEV;

	ret = stmmac_pltfr_remove(pdev);
	clk_disable_unprepare(ethqos->rgmii_clk);

	if (phy_intr_en)
		free_irq(ethqos->phy_intr, ethqos);

	if (phy_intr_en)
		cancel_work_sync(&ethqos->emac_phy_work);

	stmmac_emb_smmu_exit();
	ethqos_disable_regulators(ethqos);

	return ret;
}

static int qcom_ethqos_suspend(struct device *dev)
{
	struct qcom_ethqos *ethqos;
	struct net_device *ndev = NULL;
	int ret;
	int allow_suspend = 0;
	struct stmmac_priv *priv;
	struct plat_stmmacenet_data *plat;

	if (of_device_is_compatible(dev->of_node, "qcom,emac-smmu-embedded")) {
		ETHQOSDBG("smmu return\n");
		return 0;
	}

	ETHQOSINFO("Ethernet Suspend Enter\n");
#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	update_marker("M - Ethernet Suspend start");
#endif

	ethqos = get_stmmac_bsp_priv(dev);
	if (!ethqos)
		return -ENODEV;

	ndev = dev_get_drvdata(dev);
	priv = netdev_priv(ndev);
	plat = priv->plat;

	ethqos_ipa_offload_event_handler(&allow_suspend, EV_DPM_SUSPEND);
	if (!allow_suspend) {
		enable_irq_wake(ndev->irq);
		ETHQOSDBG("Suspend Exit enable IRQ\n");
		return 0;
	}
	if (!ndev || !netif_running(ndev))
		return -EINVAL;
	if (ethqos->current_phy_mode == DISABLE_PHY_AT_SUSPEND_ONLY ||
	    ethqos->current_phy_mode == DISABLE_PHY_SUSPEND_ENABLE_RESUME) {
		/*Backup phy related data*/
		if (priv->phydev->autoneg == AUTONEG_DISABLE) {
			ethqos->backup_autoneg = priv->phydev->autoneg;
			ethqos->backup_bmcr = ethqos_mdio_read(priv,
							       plat->phy_addr,
							       MII_BMCR);
		} else {
			ethqos->backup_autoneg = AUTONEG_ENABLE;
		}
	}
	ret = stmmac_suspend(dev);
	qcom_ethqos_phy_suspend_clks(ethqos);
	if (ethqos->current_phy_mode == DISABLE_PHY_AT_SUSPEND_ONLY ||
	    ethqos->current_phy_mode == DISABLE_PHY_SUSPEND_ENABLE_RESUME) {
		ETHQOSINFO("disable phy at suspend\n");
		ethqos_phy_power_off(ethqos);
	}

#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	update_marker("M - Ethernet Suspend End");
#endif
	ETHQOSINFO("Ethernet Suspend End ret = %d\n", ret);

	return ret;
}

static int qcom_ethqos_resume(struct device *dev)
{
	struct net_device *ndev = NULL;
	struct qcom_ethqos *ethqos;
	int ret;
	struct stmmac_priv *priv;

	if (of_device_is_compatible(dev->of_node, "qcom,emac-smmu-embedded"))
		return 0;

	ETHQOSINFO("Ethernet Resume Enter\n");
#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	update_marker("M - Ethernet Resume start");
#endif

	ethqos = get_stmmac_bsp_priv(dev);

	if (!ethqos)
		return -ENODEV;

	ndev = dev_get_drvdata(dev);
	priv = netdev_priv(ndev);

	if (!ndev || !netif_running(ndev)) {
		ETHQOSERR(" Resume not possible\n");
		return -EINVAL;
	}

	if (!ethqos->clks_suspended) {
		disable_irq_wake(ndev->irq);
		ETHQOSDBG("Resume Exit disable IRQ\n");
		return 0;
	}

	if (ethqos->current_phy_mode == DISABLE_PHY_SUSPEND_ENABLE_RESUME) {
		ETHQOSINFO("enable phy at resume\n");
		ethqos_phy_power_on(ethqos);
	}
	qcom_ethqos_phy_resume_clks(ethqos);

	if (ethqos->current_phy_mode == DISABLE_PHY_SUSPEND_ENABLE_RESUME) {
		ETHQOSINFO("reset phy after clock\n");
		ethqos_reset_phy_enable_interrupt(ethqos);
	if (ethqos->backup_autoneg == AUTONEG_DISABLE) {
		priv->phydev->autoneg = ethqos->backup_autoneg;
		if (priv->phydev)
			phy_write(priv->phydev, MII_BMCR, ethqos->backup_bmcr);
		} else {
			ETHQOSINFO("Phy dev is NULL\n");
		}
	}

	if (ethqos->current_phy_mode == DISABLE_PHY_AT_SUSPEND_ONLY) {
		/* Temp Enable LOOPBACK_EN.
		 * TX clock needed for reset As Phy is off
		 */
		rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
			      RGMII_CONFIG_LOOPBACK_EN,
			      RGMII_IO_MACRO_CONFIG);
		ETHQOSINFO("Loopback EN Enabled\n");
	}
	ret = stmmac_resume(dev);
	if (ethqos->current_phy_mode == DISABLE_PHY_AT_SUSPEND_ONLY) {
		//Disable  LOOPBACK_EN
		rgmii_updatel(ethqos, RGMII_CONFIG_LOOPBACK_EN,
			      0, RGMII_IO_MACRO_CONFIG);
		ETHQOSINFO("Loopback EN Disabled\n");
	}
	ethqos_ipa_offload_event_handler(NULL, EV_DPM_RESUME);

#ifdef CONFIG_MSM_BOOT_TIME_MARKER
	update_marker("M - Ethernet Resume End");
#endif
	ETHQOSINFO("Ethernet Resume End ret = %d\n", ret);

	return ret;
}

MODULE_DEVICE_TABLE(of, qcom_ethqos_match);

static const struct dev_pm_ops qcom_ethqos_pm_ops = {
	.suspend = qcom_ethqos_suspend,
	.resume = qcom_ethqos_resume,
};

static struct platform_driver qcom_ethqos_driver = {
	.probe  = qcom_ethqos_probe,
	.remove = qcom_ethqos_remove,
	.driver = {
		.name           = DRV_NAME,
		.pm = &qcom_ethqos_pm_ops,
		.of_match_table = of_match_ptr(qcom_ethqos_match),
	},
};

static int __init qcom_ethqos_init_module(void)
{
	int ret = 0;

	ETHQOSDBG("Enter\n");

	ret = platform_driver_register(&qcom_ethqos_driver);
	if (ret < 0) {
		ETHQOSERR("qcom-ethqos: Driver registration failed");
		return ret;
	}

	ETHQOSDBG("Exit\n");

	return ret;
}

static void __exit qcom_ethqos_exit_module(void)
{
	ETHQOSDBG("Enter\n");

	platform_driver_unregister(&qcom_ethqos_driver);

	if (!ipc_stmmac_log_ctxt)
		ipc_log_context_destroy(ipc_stmmac_log_ctxt);

	if (!ipc_stmmac_log_ctxt_low)
		ipc_log_context_destroy(ipc_stmmac_log_ctxt_low);

	ipc_stmmac_log_ctxt = NULL;
	ipc_stmmac_log_ctxt_low = NULL;
	ETHQOSDBG("Exit\n");
}

/*!
 * \brief Macro to register the driver registration function.
 *
 * \details A module always begin with either the init_module or the function
 * you specify with module_init call. This is the entry function for modules;
 * it tells the kernel what functionality the module provides and sets up the
 * kernel to run the module's functions when they're needed. Once it does this,
 * entry function returns and the module does nothing until the kernel wants
 * to do something with the code that the module provides.
 */

module_init(qcom_ethqos_init_module)

/*!
 * \brief Macro to register the driver un-registration function.
 *
 * \details All modules end by calling either cleanup_module or the function
 * you specify with the module_exit call. This is the exit function for modules;
 * it undoes whatever entry function did. It unregisters the functionality
 * that the entry function registered.
 */

module_exit(qcom_ethqos_exit_module)

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. ETHQOS driver");
MODULE_LICENSE("GPL v2");
