/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

extern void *ipc_emac_log_ctxt;

#define IPCLOG_STATE_PAGES 50
#define MAX_QMP_MSG_SIZE 96
#define __FILENAME__ (strrchr(__FILE__, '/') ? \
		strrchr(__FILE__, '/') + 1 : __FILE__)

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

#define ETHQOS_CONFIG_PPSOUT_CMD 44
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
#define EMAC_HW_v2_2_0 0x20020000
#define EMAC_HW_v2_3_2 0x20030002

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
#define MICREL_PHY_ID PHY_ID_KSZ9031
#define DWC_ETH_QOS_MICREL_PHY_INTCS 0x1b
#define DWC_ETH_QOS_MICREL_PHY_CTL 0x1f
#define DWC_ETH_QOS_BASIC_STATUS     0x0001
#define LINK_STATE_MASK 0x4
#define AUTONEG_STATE_MASK 0x20
#define MICREL_LINK_UP_INTR_STATUS BIT(0)

#define VOTE_IDX_0MBPS 0
#define VOTE_IDX_10MBPS 1
#define VOTE_IDX_100MBPS 2
#define VOTE_IDX_1000MBPS 3

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

	unsigned long avb_class_a_intr_cnt;
	unsigned long avb_class_b_intr_cnt;
	struct dentry *debugfs_dir;

	int oldlink;
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

	int always_on_phy;
	/* QMP message for disabling ctile power collapse while XO shutdown */
	struct mbox_chan *qmp_mbox_chan;
	struct mbox_client *qmp_mbox_client;
	struct work_struct qmp_mailbox_work;
	int disable_ctile_pc;
};

struct pps_cfg {
	unsigned int ptpclk_freq;
	unsigned int ppsout_freq;
	unsigned int ppsout_ch;
	unsigned int ppsout_duty;
	unsigned int ppsout_start;
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

int ethqos_init_reqgulators(struct qcom_ethqos *ethqos);
void ethqos_disable_regulators(struct qcom_ethqos *ethqos);
int ethqos_init_gpio(struct qcom_ethqos *ethqos);
void ethqos_free_gpios(struct qcom_ethqos *ethqos);
int create_pps_interrupt_device_node(dev_t *pps_dev_t,
				     struct cdev **pps_cdev,
				     struct class **pps_class,
				     char *pps_dev_node_name);
void qcom_ethqos_request_phy_wol(struct plat_stmmacenet_data *plat);

#endif
