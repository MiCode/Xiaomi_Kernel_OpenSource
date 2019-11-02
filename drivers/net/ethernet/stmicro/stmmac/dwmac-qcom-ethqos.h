/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

extern void *ipc_emac_log_ctxt;

#define IPCLOG_STATE_PAGES 50
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

	unsigned int rgmii_clk_rate;
	struct clk *rgmii_clk;
	unsigned int speed;

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
#endif
