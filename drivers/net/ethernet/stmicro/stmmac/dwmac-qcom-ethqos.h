/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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
#ifndef	_DWMAC_QCOM_ETHQOS_H
#define	_DWMAC_QCOM_ETHQOS_H

#include <linux/ipc_logging.h>
#include <linux/msm-bus.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/qmp.h>
#include <linux/mailbox_controller.h>

#include <linux/inetdevice.h>
#include <linux/inet.h>

#include <net/addrconf.h>
#include <net/ipv6.h>
#include <net/inet_common.h>

#include <linux/uaccess.h>

extern void *ipc_stmmac_log_ctxt;
extern void *ipc_stmmac_log_ctxt_low;

#define QCOM_ETH_QOS_MAC_ADDR_LEN 6
#define QCOM_ETH_QOS_MAC_ADDR_STR_LEN 18

#define IPCLOG_STATE_PAGES 50
#define MAX_QMP_MSG_SIZE 96
#define IPC_RATELIMIT_BURST 1
#define __FILENAME__ (strrchr(__FILE__, '/') ? \
		strrchr(__FILE__, '/') + 1 : __FILE__)

#define DRV_NAME "qcom-ethqos"
#define ETHQOSDBG(fmt, args...) \
do {\
	pr_debug(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
	if (ipc_stmmac_log_ctxt) { \
		ipc_log_string(ipc_stmmac_log_ctxt, \
		"%s: %s[%u]:[stmmac] DEBUG:" fmt, __FILENAME__,\
		__func__, __LINE__, ## args); \
	} \
} while (0)
#define ETHQOSERR(fmt, args...) \
do {\
	pr_err(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
	if (ipc_stmmac_log_ctxt) { \
		ipc_log_string(ipc_stmmac_log_ctxt, \
		"%s: %s[%u]:[stmmac] ERROR:" fmt, __FILENAME__,\
		__func__, __LINE__, ## args); \
	} \
} while (0)
#define ETHQOSINFO(fmt, args...) \
do {\
	pr_info(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
	if (ipc_stmmac_log_ctxt) { \
		ipc_log_string(ipc_stmmac_log_ctxt, \
		"%s: %s[%u]:[stmmac] INFO:" fmt, __FILENAME__,\
		__func__, __LINE__, ## args); \
	} \
} while (0)

#define IPC_LOW(fmt, args...) \
do {\
	if (ipc_stmmac_log_ctxt_low) { \
		ipc_log_string(ipc_stmmac_log_ctxt_low, \
		"%s: %s[%u]:[stmmac] DEBUG:" fmt, __FILENAME__, \
		__func__, __LINE__, ## args); \
	} \
} while (0)

/* Printing one error message in 5 seconds if multiple error messages
 * are coming back to back.
 */
#define pr_err_ratelimited_ipc(fmt, ...) \
	printk_ratelimited_ipc(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#define printk_ratelimited_ipc(fmt, ...) \
({ \
	static DEFINE_RATELIMIT_STATE(_rs, DEFAULT_RATELIMIT_INTERVAL, \
				       IPC_RATELIMIT_BURST); \
	if (__ratelimit(&_rs)) \
		printk(fmt, ##__VA_ARGS__); \
})
#define IPCERR_RL(fmt, args...) \
	pr_err_ratelimited_ipc(DRV_NAME " %s:%d " fmt, __func__,\
	__LINE__, ## args)

#define RGMII_IO_MACRO_CONFIG		0x0
#define SDCC_HC_REG_DLL_CONFIG		0x4
#define SDCC_HC_REG_DDR_CONFIG		0xC
#define SDCC_HC_REG_DLL_CONFIG2		0x10
#define SDC4_STATUS			0x14
#define SDCC_USR_CTL			0x18
#define RGMII_IO_MACRO_CONFIG2		0x1C

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

#define RGMII_IO_MACRO_DEBUG1		0x20
#define EMAC_SYSTEM_LOW_POWER_DEBUG	0x28

/* RGMII_IO_MACRO_CONFIG fields */
#define RGMII_CONFIG_FUNC_CLK_EN		BIT(30)
#define RGMII_CONFIG_POS_NEG_DATA_SEL		BIT(23)
#define RGMII_CONFIG_GPIO_CFG_RX_INT		GENMASK(21, 20)
#define RGMII_CONFIG_GPIO_CFG_TX_INT		GENMASK(19, 17)
#define RGMII_CONFIG_MAX_SPD_PRG_9		GENMASK(16, 8)
#define RGMII_CONFIG_MAX_SPD_PRG_2		GENMASK(7, 6)
#define RGMII_CONFIG_INTF_SEL			GENMASK(5, 4)
#define RGMII_CONFIG_BYPASS_TX_ID_EN		BIT(3)
#define RGMII_CONFIG_LOOPBACK_EN		BIT(2)
#define RGMII_CONFIG_PROG_SWAP			BIT(1)
#define RGMII_CONFIG_DDR_MODE			BIT(0)

/* SDCC_HC_REG_DLL_CONFIG fields */
#define SDCC_DLL_CONFIG_DLL_RST			BIT(30)
#define SDCC_DLL_CONFIG_PDN			BIT(29)
#define SDCC_DLL_CONFIG_MCLK_FREQ		GENMASK(26, 24)
#define SDCC_DLL_CONFIG_CDR_SELEXT		GENMASK(23, 20)
#define SDCC_DLL_CONFIG_CDR_EXT_EN		BIT(19)
#define SDCC_DLL_CONFIG_CK_OUT_EN		BIT(18)
#define SDCC_DLL_CONFIG_CDR_EN			BIT(17)
#define SDCC_DLL_CONFIG_DLL_EN			BIT(16)
#define SDCC_DLL_MCLK_GATING_EN			BIT(5)
#define SDCC_DLL_CDR_FINE_PHASE			GENMASK(3, 2)

/* SDCC_HC_REG_DDR_CONFIG fields */
#define SDCC_DDR_CONFIG_PRG_DLY_EN		BIT(31)
#define SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY	GENMASK(26, 21)
#define SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_CODE	GENMASK(29, 27)
#define SDCC_DDR_CONFIG_EXT_PRG_RCLK_DLY_EN	BIT(30)
#define SDCC_DDR_CONFIG_PRG_RCLK_DLY		GENMASK(8, 0)

/* SDCC_HC_REG_DLL_CONFIG2 fields */
#define SDCC_DLL_CONFIG2_DLL_CLOCK_DIS		BIT(21)
#define SDCC_DLL_CONFIG2_MCLK_FREQ_CALC		GENMASK(17, 10)
#define SDCC_DLL_CONFIG2_DDR_TRAFFIC_INIT_SEL	GENMASK(3, 2)
#define SDCC_DLL_CONFIG2_DDR_TRAFFIC_INIT_SW	BIT(1)
#define SDCC_DLL_CONFIG2_DDR_CAL_EN		BIT(0)

/* SDC4_STATUS bits */
#define SDC4_STATUS_DLL_LOCK			BIT(7)

/* RGMII_IO_MACRO_CONFIG2 fields */
#define RGMII_CONFIG2_RSVD_CONFIG15		GENMASK(31, 17)
#define RGMII_CONFIG2_RGMII_CLK_SEL_CFG		BIT(16)
#define RGMII_CONFIG2_TX_TO_RX_LOOPBACK_EN	BIT(13)
#define RGMII_CONFIG2_CLK_DIVIDE_SEL		BIT(12)
#define RGMII_CONFIG2_RX_PROG_SWAP		BIT(7)
#define RGMII_CONFIG2_DATA_DIVIDE_CLK_SEL	BIT(6)
#define RGMII_CONFIG2_TX_CLK_PHASE_SHIFT_EN	BIT(5)

#define EMAC_I0_EMAC_CORE_HW_VERSION_RGOFFADDR 0x00000070

#define EMAC_HW_NONE 0
#define EMAC_HW_v2_0_0 0x20000000
#define EMAC_HW_v2_1_0 0x20010000
#define EMAC_HW_v2_1_1 0x20010001
#define EMAC_HW_v2_1_2 0x20010002
#define EMAC_HW_v2_2_0 0x20020000
#define EMAC_HW_v2_3_0 0x20030000
#define EMAC_HW_v2_3_1 0x20030001
#define EMAC_HW_v2_3_2 0x20030002
#define EMAC_HW_vMAX 9

#define MII_BUSY 0x00000001
#define MII_WRITE 0x00000002

/* GMAC4 defines */
#define MII_GMAC4_GOC_SHIFT		2
#define MII_GMAC4_WRITE			BIT(MII_GMAC4_GOC_SHIFT)
#define MII_GMAC4_READ			(3 << MII_GMAC4_GOC_SHIFT)

#define MII_BUSY 0x00000001
#define MII_WRITE 0x00000002

#define DWC_ETH_QOS_PHY_INTR_STATUS     0x0013

#define LINK_UP 1
#define LINK_DOWN 0

#define LINK_DOWN_STATE 0x800
#define LINK_UP_STATE 0x400

#define ATH8031_PHY_ID 0x004dd074
#define ATH8035_PHY_ID 0x004dd072
#define QCA8337_PHY_ID 0x004dd036
#define ATH8030_PHY_ID 0x004dd076
#define DWC_ETH_QOS_MICREL_PHY_INTCS 0x1b
#define DWC_ETH_QOS_MICREL_PHY_CTL 0x1f
#define DWC_ETH_QOS_BASIC_STATUS     0x0001
#define LINK_STATE_MASK 0x4
#define AUTONEG_STATE_MASK 0x20
#define MICREL_LINK_UP_INTR_STATUS BIT(0)
#define PHY_WOL 0x1

#define VOTE_IDX_0MBPS 0
#define VOTE_IDX_10MBPS 1
#define VOTE_IDX_100MBPS 2
#define VOTE_IDX_1000MBPS 3

//Mac config
#define MAC_CONFIGURATION 0x0
#define MAC_LM BIT(12)

#define TLMM_BASE_ADDRESS (tlmm_central_base_addr)

#define TLMM_RGMII_HDRV_PULL_CTL1_ADDRESS_OFFSET\
	(((ethqos->emac_ver == EMAC_HW_v2_3_2) ? 0xA7000\
	 : (ethqos->emac_ver == EMAC_HW_v2_0_0) ? 0xA5000\
	 : (ethqos->emac_ver == EMAC_HW_v2_2_0) ? 0xA5000\
	 : 0))

#define TLMM_RGMII_HDRV_PULL_CTL1_ADDRESS\
	(((unsigned long *)\
		(TLMM_BASE_ADDRESS + TLMM_RGMII_HDRV_PULL_CTL1_ADDRESS_OFFSET)))

#define TLMM_RGMII_HDRV_PULL_CTL1_RGWR(data)\
	iowrite32(data,	(void __iomem *)TLMM_RGMII_HDRV_PULL_CTL1_ADDRESS)

#define TLMM_RGMII_HDRV_PULL_CTL1_RGRD(data)\
	((data) = ioread32((void __iomem *)TLMM_RGMII_HDRV_PULL_CTL1_ADDRESS))

#define TLMM_RGMII_HDRV_PULL_CTL1_HDRV_MASK (unsigned long)(0x7)

#define TLMM_RGMII_HDRV_PULL_CTL1_CK_TX_HDRV_WR_MASK_15\
	(unsigned long)(0xFFFC7FFF)
#define TLMM_RGMII_HDRV_PULL_CTL1_TX_3_HDRV_WR_MASK_12\
	(unsigned long)(0xFFFF8FFF)
#define TLMM_RGMII_HDRV_PULL_CTL1_TX_2_HDRV_WR_MASK_9\
	(unsigned long)(0xFFFFF1FF)
#define TLMM_RGMII_HDRV_PULL_CTL1_TX_1_HDRV_WR_MASK_6\
	(unsigned long)(0xFFFFFE3F)
#define TLMM_RGMII_HDRV_PULL_CTL1_TX_0_HDRV_WR_MASK_3\
	(unsigned long)(0xFFFFFFC7)
#define TLMM_RGMII_HDRV_PULL_CTL1_CTL_TX_HDRV_WR_MASK_0\
	(unsigned long)(0xFFFFFFF8)

#define TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_2MA (unsigned long)(0x0)
#define TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_4MA (unsigned long)(0x1)
#define TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_6MA (unsigned long)(0x2)
#define TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_8MA (unsigned long)(0x3)
#define TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_10MA (unsigned long)(0x4)
#define TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_12MA (unsigned long)(0x5)
#define TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_14MA (unsigned long)(0x6)
#define TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_16MA (unsigned long)(0x7)

#define TLMM_RGMII_HDRV_PULL_CTL1_TX_HDRV_WR(clk, data, ctl) do {\
		unsigned long v;\
		unsigned long drv = data;\
		TLMM_RGMII_HDRV_PULL_CTL1_RGRD(v);\
		v = (v & (TLMM_RGMII_HDRV_PULL_CTL1_CK_TX_HDRV_WR_MASK_15))\
		 | (((clk) & (TLMM_RGMII_HDRV_PULL_CTL1_HDRV_MASK)) << 15);\
		v = (v & (TLMM_RGMII_HDRV_PULL_CTL1_TX_3_HDRV_WR_MASK_12))\
		 | (((drv) & (TLMM_RGMII_HDRV_PULL_CTL1_HDRV_MASK)) << 12);\
		v = (v & (TLMM_RGMII_HDRV_PULL_CTL1_TX_2_HDRV_WR_MASK_9))\
		 | (((drv) & (TLMM_RGMII_HDRV_PULL_CTL1_HDRV_MASK)) << 9);\
		v = (v & (TLMM_RGMII_HDRV_PULL_CTL1_TX_1_HDRV_WR_MASK_6))\
		 | (((drv) & (TLMM_RGMII_HDRV_PULL_CTL1_HDRV_MASK)) << 6);\
		v = (v & (TLMM_RGMII_HDRV_PULL_CTL1_TX_0_HDRV_WR_MASK_3))\
		 | (((drv) & (TLMM_RGMII_HDRV_PULL_CTL1_HDRV_MASK)) << 3);\
		v = (v & (TLMM_RGMII_HDRV_PULL_CTL1_CTL_TX_HDRV_WR_MASK_0))\
		 | (((ctl) & (TLMM_RGMII_HDRV_PULL_CTL1_HDRV_MASK)) << 0);\
		TLMM_RGMII_HDRV_PULL_CTL1_RGWR(v);\
} while (0)

#define TLMM_RGMII_RX_HV_MODE_CTL_ADDRESS_OFFSET \
	(((ethqos->emac_ver == EMAC_HW_v2_3_2) ? 0xA7004\
	  : (ethqos->emac_ver == EMAC_HW_v2_0_0) ? 0xA5004\
	  : (ethqos->emac_ver == EMAC_HW_v2_2_0) ? 0xA5004\
	  : 0))

#define TLMM_RGMII_RX_HV_MODE_CTL_ADDRESS\
	((unsigned long *)\
	 (TLMM_BASE_ADDRESS + TLMM_RGMII_RX_HV_MODE_CTL_ADDRESS_OFFSET))\

#define TLMM_RGMII_RX_HV_MODE_CTL_RGWR(data)\
	(iowrite32(data, (void __iomem *)TLMM_RGMII_RX_HV_MODE_CTL_ADDRESS))

#define TLMM_RGMII_RX_HV_MODE_CTL_RGRD(data)\
	((data) = ioread32((void __iomem *)TLMM_RGMII_RX_HV_MODE_CTL_ADDRESS))
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

enum loopback_mode {
	DISABLE_LOOPBACK = 0,
	ENABLE_IO_MACRO_LOOPBACK,
	ENABLE_MAC_LOOPBACK,
	ENABLE_PHY_LOOPBACK
};

enum phy_power_mode {
	DISABLE_PHY_IMMEDIATELY = 1,
	ENABLE_PHY_IMMEDIATELY,
	DISABLE_PHY_AT_SUSPEND_ONLY,
	DISABLE_PHY_SUSPEND_ENABLE_RESUME,
	DISABLE_PHY_ON_OFF,
};

enum current_phy_state {
	PHY_IS_ON = 0,
	PHY_IS_OFF,
};

#define RGMII_IO_BASE_ADDRESS ethqos->rgmii_base

#define RGMII_IO_MACRO_CONFIG_RGOFFADDR_OFFSET (0x00000000)

#define RGMII_IO_MACRO_CONFIG_RGWR(data)\
	writel_relaxed(data, RGMII_IO_MACRO_CONFIG_RGOFFADDR)

#define RGMII_IO_MACRO_CONFIG_RGOFFADDR \
	(RGMII_IO_BASE_ADDRESS + RGMII_IO_MACRO_CONFIG_RGOFFADDR_OFFSET)

#define RX_CONTEXT_DESC_RDES3_OWN_MLF_WR(ptr, data)\
	SET_BITS(0x1f, 0x1f, ptr, data)

#define RGMII_IO_MACRO_CONFIG_RGRD(data)\
	((data) = (readl_relaxed((RGMII_IO_MACRO_CONFIG_RGOFFADDR))))

#define RGMII_GPIO_CFG_TX_INT_MASK (unsigned long)(0x3)

#define RGMII_GPIO_CFG_TX_INT_WR_MASK (unsigned long)(0xfff9ffff)

#define RGMII_GPIO_CFG_TX_INT_UDFWR(data) do {\
	unsigned long v;\
	RGMII_IO_MACRO_CONFIG_RGRD(v);\
	v = ((v & RGMII_GPIO_CFG_TX_INT_WR_MASK) | \
	((data & RGMII_GPIO_CFG_TX_INT_MASK) << 17));\
	RGMII_IO_MACRO_CONFIG_RGWR(v);\
} while (0)

#define RGMII_GPIO_CFG_RX_INT_MASK (unsigned long)(0x3)

#define RGMII_GPIO_CFG_RX_INT_WR_MASK (unsigned long)(0xffe7ffff)

#define RGMII_GPIO_CFG_RX_INT_UDFWR(data) do {\
	unsigned long v;\
	RGMII_IO_MACRO_CONFIG_RGRD(v);\
	v = ((v & RGMII_GPIO_CFG_RX_INT_WR_MASK) | \
	((data & RGMII_GPIO_CFG_RX_INT_MASK) << 19));\
	RGMII_IO_MACRO_CONFIG_RGWR(v);\
} while (0)

enum CV2X_MODE {
	CV2X_MODE_DISABLE = 0x0,
	CV2X_MODE_MDM,
	CV2X_MODE_AP
};

struct ethqos_vlan_info {
	u16 vlan_id;
	u32 vlan_offset;
	u32 rx_queue;
	bool available;
};

struct ethqos_emac_por {
	unsigned int offset;
	unsigned int value;
};

struct ethqos_emac_driver_data {
	struct ethqos_emac_por *por;
	unsigned int num_por;
};

struct ethqos_io_macro {
	bool rx_prog_swap;
	bool rx_dll_bypass;
};

struct qcom_ethqos {
	struct platform_device *pdev;
	void __iomem *rgmii_base;
	void __iomem *ioaddr;

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

	struct ethqos_emac_por *por;
	unsigned int num_por;
	unsigned int emac_ver;

	struct regulator *gdsc_emac;
	struct regulator *reg_rgmii;
	struct regulator *reg_emac_phy;
	struct regulator *reg_rgmii_io_pads;

	u32 pps_class_a_irq;
	u32 pps_class_b_irq;

	struct pinctrl_state *emac_pps_0;

	/* avb_class_a dev node variables*/
	dev_t avb_class_a_dev_t;
	struct cdev *avb_class_a_cdev;
	struct class *avb_class_a_class;

	/* avb_class_b dev node variables*/
	dev_t avb_class_b_dev_t;
	struct cdev *avb_class_b_cdev;
	struct class *avb_class_b_class;

	dev_t emac_dev_t;
	struct cdev *emac_cdev;
	struct class *emac_class;

	unsigned long avb_class_a_intr_cnt;
	unsigned long avb_class_b_intr_cnt;
	struct dentry *debugfs_dir;

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

	/* QMP message for disabling ctile power collapse while XO shutdown */
	struct mbox_chan *qmp_mbox_chan;
	struct mbox_client *qmp_mbox_client;
	struct work_struct qmp_mailbox_work;
	/* early ethernet parameters */
	struct work_struct early_eth;
	struct delayed_work ipv4_addr_assign_wq;
	struct delayed_work ipv6_addr_assign_wq;
	bool early_eth_enabled;

	int disable_ctile_pc;

	u32 emac_mem_base;
	u32 emac_mem_size;

	bool ipa_enabled;
	/* Key Performance Indicators */
	bool print_kpi;
	unsigned int emac_phy_off_suspend;
	int loopback_speed;
	enum loopback_mode current_loopback;
	enum phy_power_mode current_phy_mode;
	enum current_phy_state phy_state;
	/*Backup variable for phy loopback*/
	int backup_duplex;
	int backup_speed;
	u32 bmcr_backup;
	/*Backup variable for suspend resume*/
	int backup_suspend_speed;
	u32 backup_bmcr;
	unsigned backup_autoneg:1;

	/* IO Macro parameters */
	struct ethqos_io_macro io_macro;

	/* QMI over ethernet parameter */
	u32 qoe_mode;
	struct ethqos_vlan_info qoe_vlan;
	u32 cv2x_mode;
	struct ethqos_vlan_info cv2x_vlan;
	unsigned char cv2x_dev_addr[ETH_ALEN];
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
void qcom_ethqos_request_phy_wol(struct plat_stmmacenet_data *plat);
bool qcom_ethqos_is_phy_link_up(struct qcom_ethqos *ethqos);
void *qcom_ethqos_get_priv(struct qcom_ethqos *ethqos);

int ppsout_config(struct stmmac_priv *priv, struct pps_cfg *eth_pps_cfg);
int ethqos_phy_power_on(struct qcom_ethqos *ethqos);
void  ethqos_phy_power_off(struct qcom_ethqos *ethqos);
void ethqos_reset_phy_enable_interrupt(struct qcom_ethqos *ethqos);

u16 dwmac_qcom_select_queue(
	struct net_device *dev,
	struct sk_buff *skb,
	void *accel_priv,
	select_queue_fallback_t fallback);

#define QTAG_VLAN_ETH_TYPE_OFFSET 16
#define QTAG_UCP_FIELD_OFFSET 14
#define QTAG_ETH_TYPE_OFFSET 12
#define PTP_UDP_EV_PORT 0x013F
#define PTP_UDP_GEN_PORT 0x0140

#define IPA_DMA_TX_CH 0
#define IPA_DMA_RX_CH 0

#define CV2X_TAG_TX_CHANNEL 3
#define QMI_TAG_TX_CHANNEL 2

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

int dwmac_qcom_program_avb_algorithm(
	struct stmmac_priv *priv, struct ifr_data_struct *req);
unsigned int dwmac_qcom_get_plat_tx_coal_frames(
	struct sk_buff *skb);

unsigned int dwmac_qcom_get_eth_type(unsigned char *buf);
#endif
