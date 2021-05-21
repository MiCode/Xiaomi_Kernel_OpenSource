/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */
#ifndef	_DWMAC_QCOM_ETHQOS_H
#define	_DWMAC_QCOM_ETHQOS_H

//#include <linux/msm-bus.h>
#include <linux/ipc_logging.h>

extern void *ipc_emac_log_ctxt;

#define IPCLOG_STATE_PAGES 50
#define __FILENAME__ (strrchr(__FILE__, '/') ? \
		strrchr(__FILE__, '/') + 1 : __FILE__)

#include <linux/inetdevice.h>
#include <linux/inet.h>

#include <net/addrconf.h>
#include <net/ipv6.h>
#include <net/inet_common.h>

#include <linux/uaccess.h>

#define QCOM_ETH_QOS_MAC_ADDR_LEN 6
#define QCOM_ETH_QOS_MAC_ADDR_STR_LEN 18

#define DRV_NAME "qcom-ethqos"
#define ETHQOSDBG(fmt, args...) \
	pr_debug(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)
#define ETHQOSERR(fmt, args...) \
do {\
	pr_err(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
	if (ipc_emac_log_ctxt) { \
		ipc_log_string(ipc_emac_log_ctxt, \
		"%s: %s[%u]:[emac] ERROR:" fmt, __FILENAME__,\
		__func__, __LINE__, ## args); \
	} \
} while (0)
#define ETHQOSINFO(fmt, args...) \
	pr_info(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)
#define RGMII_IO_MACRO_CONFIG		0x0
#define SDCC_HC_REG_DLL_CONFIG		0x4
#define SDCC_HC_REG_DDR_CONFIG		0xC
#define SDCC_HC_REG_DLL_CONFIG2		0x10
#define SDC4_STATUS			0x14
#define SDCC_USR_CTL			0x18
#define RGMII_IO_MACRO_CONFIG2		0x1C
#define EMAC_HW_NONE 0
#define EMAC_HW_v2_1_1 0x20010001
#define EMAC_HW_v2_1_2 0x20010002
#define EMAC_HW_v2_3_0 0x20030000
#define EMAC_HW_v2_3_1 0x20030001
#define EMAC_HW_vMAX 9

#define ETHQOS_CONFIG_PPSOUT_CMD 44
#define ETHQOS_AVB_ALGORITHM 27

#define MAC_PPS_CONTROL			0x00000b70
#define PPS_MAXIDX(x)			((((x) + 1) * 8) - 1)
#define PPS_MINIDX(x)			((x) * 8)
#define MCGRENX(x)			BIT(PPS_MAXIDX(x))
#define PPSEN0				BIT(4)
#define MAC_PPSX_TARGET_TIME_SEC(x)	(0x00000b80 + ((x) * 0x10))
#define MAC_PPSX_TARGET_TIME_NSEC(x)	(0x00000b84 + ((x) * 0x10))
#define TRGTBUSY0			BIT(31)
#define TTSL0				GENMASK(30, 0)
#define MAC_PPSX_INTERVAL(x)		(0x00000b88 + ((x) * 0x10))
#define MAC_PPSX_WIDTH(x)		(0x00000b8c + ((x) * 0x10))

#define PPS_START_DELAY 100000000
#define ONE_NS 1000000000
#define PPS_ADJUST_NS 32

#define DWC_ETH_QOS_PPS_CH_0 0
#define DWC_ETH_QOS_PPS_CH_1 1
#define DWC_ETH_QOS_PPS_CH_2 2
#define DWC_ETH_QOS_PPS_CH_3 3

#define AVB_CLASS_A_POLL_DEV_NODE "avb_class_a_intr"

#define AVB_CLASS_B_POLL_DEV_NODE "avb_class_b_intr"

#define AVB_CLASS_A_CHANNEL_NUM 2
#define AVB_CLASS_B_CHANNEL_NUM 3

#define VOTE_IDX_0MBPS 0
#define VOTE_IDX_10MBPS 1
#define VOTE_IDX_100MBPS 2
#define VOTE_IDX_1000MBPS 3

static inline u32 PPSCMDX(u32 x, u32 val)
{
	return (GENMASK(PPS_MINIDX(x) + 3, PPS_MINIDX(x)) &
	((val) << PPS_MINIDX(x)));
}

static inline u32 TRGTMODSELX(u32 x, u32 val)
{
	return (GENMASK(PPS_MAXIDX(x) - 1, PPS_MAXIDX(x) - 2) &
	((val) << (PPS_MAXIDX(x) - 2)));
}

static inline u32 PPSX_MASK(u32 x)
{
	return GENMASK(PPS_MAXIDX(x), PPS_MINIDX(x));
}

enum IO_MACRO_PHY_MODE {
		RGMII_MODE,
		RMII_MODE,
		MII_MODE
};

struct ethqos_emac_por {
	unsigned int offset;
	unsigned int value;
};

static const struct ethqos_emac_por emac_v2_3_0_por[] = {
	{ .offset = RGMII_IO_MACRO_CONFIG,	.value = 0x00C01343 },
	{ .offset = SDCC_HC_REG_DLL_CONFIG,	.value = 0x2004642C },
	{ .offset = SDCC_HC_REG_DDR_CONFIG,	.value = 0x00000000 },
	{ .offset = SDCC_HC_REG_DLL_CONFIG2,	.value = 0x00200000 },
	{ .offset = SDCC_USR_CTL,		.value = 0x00010800 },
	{ .offset = RGMII_IO_MACRO_CONFIG2,	.value = 0x00002060 },
};

static const struct ethqos_emac_por emac_v2_3_2_por[] = {
	{ .offset = RGMII_IO_MACRO_CONFIG,	.value = 0x00C01343 },
	{ .offset = SDCC_HC_REG_DLL_CONFIG,	.value = 0x2004642C },
	{ .offset = SDCC_HC_REG_DDR_CONFIG,	.value = 0x80040800 },
	{ .offset = SDCC_HC_REG_DLL_CONFIG2,	.value = 0x00200000 },
	{ .offset = SDCC_USR_CTL,		.value = 0x00010800 },
	{ .offset = RGMII_IO_MACRO_CONFIG2,	.value = 0x00002060 },
};

struct qcom_ethqos {
	struct platform_device *pdev;
	void __iomem *rgmii_base;

	struct msm_bus_scale_pdata *bus_scale_vec;
	u32 bus_hdl;
	unsigned int rgmii_clk_rate;
	struct clk *rgmii_clk;
	unsigned int speed;
	unsigned int vote_idx;

	int gpio_phy_intr_redirect;
	u32 phy_intr;
	/* Work struct for handling phy interrupt */
	struct work_struct emac_phy_work;

	const struct ethqos_emac_por *por;
	unsigned int num_por;
	unsigned int emac_ver;

	struct regulator *gdsc_emac;
	struct regulator *reg_rgmii;
	struct regulator *reg_emac_phy;
	struct regulator *reg_rgmii_io_pads;

	int pps_class_a_irq;
	int pps_class_b_irq;

	struct pinctrl_state *emac_pps_0;

	/* avb_class_a dev node variables*/
	dev_t avb_class_a_dev_t;
	struct cdev *avb_class_a_cdev;
	struct class *avb_class_a_class;

	/* avb_class_b dev node variables*/
	dev_t avb_class_b_dev_t;
	struct cdev *avb_class_b_cdev;
	struct class *avb_class_b_class;

	unsigned long avb_class_a_intr_cnt;
	unsigned long avb_class_b_intr_cnt;

	/* saving state for Wake-on-LAN */
	int wolopts;
	/* state of enabled wol options in PHY*/
	u32 phy_wol_wolopts;
	/* state of supported wol options in PHY*/
	u32 phy_wol_supported;
	/* Boolean to check if clock is suspended*/
	int clks_suspended;
	/* Structure which holds done and wait members */
	struct completion clk_enable_done;
	/* early ethernet parameters */
	struct work_struct early_eth;
	struct delayed_work ipv4_addr_assign_wq;
	struct delayed_work ipv6_addr_assign_wq;
	bool early_eth_enabled;
	/* Key Performance Indicators */
	bool print_kpi;

	struct dentry *debugfs_dir;
};

struct pps_cfg {
	unsigned int ptpclk_freq;
	unsigned int ppsout_freq;
	unsigned int ppsout_ch;
	unsigned int ppsout_duty;
	unsigned int ppsout_start;
	unsigned int ppsout_align;
	unsigned int ppsout_align_ns;
};

struct ifr_data_struct {
	unsigned int flags;
	unsigned int qinx; /* dma channel no to be configured */
	unsigned int cmd;
	unsigned int context_setup;
	unsigned int connected_speed;
	unsigned int rwk_filter_values[8];
	unsigned int rwk_filter_length;
	int command_error;
	int test_done;
	void *ptr;
};

struct pps_info {
	int channel_no;
};

struct ip_params {
	unsigned char mac_addr[QCOM_ETH_QOS_MAC_ADDR_LEN];
	bool is_valid_mac_addr;
	char link_speed[32];
	bool is_valid_link_speed;
	char ipv4_addr_str[32];
	struct in_addr ipv4_addr;
	bool is_valid_ipv4_addr;
	char ipv6_addr_str[48];
	struct in6_ifreq ipv6_addr;
	bool is_valid_ipv6_addr;
};

int ethqos_init_reqgulators(struct qcom_ethqos *ethqos);
void ethqos_disable_regulators(struct qcom_ethqos *ethqos);
int ethqos_init_gpio(struct qcom_ethqos *ethqos);
void ethqos_free_gpios(struct qcom_ethqos *ethqos);
int create_pps_interrupt_device_node(dev_t *pps_dev_t,
				     struct cdev **pps_cdev,
				     struct class **pps_class,
				     char *pps_dev_node_name);
int ethqos_remove_pps_dev(struct qcom_ethqos *ethqos);
bool qcom_ethqos_is_phy_link_up(struct qcom_ethqos *ethqos);
void *qcom_ethqos_get_priv(struct qcom_ethqos *ethqos);

int ppsout_config(struct stmmac_priv *priv, struct pps_cfg *eth_pps_cfg);

u16 dwmac_qcom_select_queue(struct net_device *dev,
			    struct sk_buff *skb,
			    struct net_device *sb_dev);

#define QTAG_VLAN_ETH_TYPE_OFFSET 16
#define QTAG_UCP_FIELD_OFFSET 14
#define QTAG_ETH_TYPE_OFFSET 12
#define PTP_UDP_EV_PORT 0x013F
#define PTP_UDP_GEN_PORT 0x0140

#define IPA_DMA_TX_CH 0
#define IPA_DMA_RX_CH 0

#define VLAN_TAG_UCP_SHIFT 13
#define CLASS_A_TRAFFIC_UCP 3
#define CLASS_A_TRAFFIC_TX_CHANNEL 3

#define CLASS_B_TRAFFIC_UCP 2
#define CLASS_B_TRAFFIC_TX_CHANNEL 2

#define NON_TAGGED_IP_TRAFFIC_TX_CHANNEL 1
#define ALL_OTHER_TRAFFIC_TX_CHANNEL 1
#define ALL_OTHER_TX_TRAFFIC_IPA_DISABLED 0

#define DEFAULT_INT_MOD 1
#define AVB_INT_MOD 8
#define IP_PKT_INT_MOD 32
#define PTP_INT_MOD 1

#define PPS_19_2_FREQ 19200000

enum dwmac_qcom_queue_operating_mode {
	DWMAC_QCOM_QDISABLED = 0X0,
	DWMAC_QCOM_QAVB,
	DWMAC_QCOM_QDCB,
	DWMAC_QCOM_QGENERIC
};

struct dwmac_qcom_avb_algorithm_params {
	unsigned int idle_slope;
	unsigned int send_slope;
	unsigned int hi_credit;
	unsigned int low_credit;
};

struct dwmac_qcom_avb_algorithm {
	unsigned int qinx;
	unsigned int algorithm;
	unsigned int cc;
	struct dwmac_qcom_avb_algorithm_params speed100params;
	struct dwmac_qcom_avb_algorithm_params speed1000params;
	enum dwmac_qcom_queue_operating_mode op_mode;
};

void dwmac_qcom_program_avb_algorithm(struct stmmac_priv *priv,
				      struct ifr_data_struct *req);
unsigned int dwmac_qcom_get_plat_tx_coal_frames(struct sk_buff *skb);
int ethqos_init_pps(void *priv);
#endif
