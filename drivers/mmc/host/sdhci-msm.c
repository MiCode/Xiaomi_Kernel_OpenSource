// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 *
 * drivers/mmc/host/sdhci-msm.c - Qualcomm Technologies, Inc. MSM SDHCI Platform
 * driver source file
 */

#include <linux/module.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio_func.h>
#include <linux/gfp.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/dma-mapping.h>
#include <linux/iopoll.h>
#include <linux/pinctrl/consumer.h>
#include <linux/iopoll.h>
#include <linux/msm-bus.h>
#include <linux/pm_runtime.h>
#include <trace/events/mmc.h>
#include <linux/clk/qcom.h>

#include "sdhci-msm.h"
#include "sdhci-pltfm.h"
#include "cqhci.h"
#include "cqhci-crypto-qti.h"

#define QOS_REMOVE_DELAY_MS	10
#define CORE_POWER		0x0
#define CORE_SW_RST		(1 << 7)

#define SDHCI_VER_100		0x2B

#define CORE_VERSION_STEP_MASK		0x0000FFFF
#define CORE_VERSION_MINOR_MASK		0x0FFF0000
#define CORE_VERSION_MINOR_SHIFT	16
#define CORE_VERSION_MAJOR_MASK		0xF0000000
#define CORE_VERSION_MAJOR_SHIFT	28
#define CORE_VERSION_TARGET_MASK	0x000000FF
#define SDHCI_MSM_VER_420               0x49

#define SWITCHABLE_SIGNALLING_VOL	(1 << 29)

#define CORE_VERSION_MAJOR_MASK		0xF0000000
#define CORE_VERSION_MAJOR_SHIFT	28

#define CORE_HC_MODE		0x78
#define HC_MODE_EN		0x1
#define FF_CLK_SW_RST_DIS	(1 << 13)

#define CORE_PWRCTL_BUS_OFF	0x01
#define CORE_PWRCTL_BUS_ON	(1 << 1)
#define CORE_PWRCTL_IO_LOW	(1 << 2)
#define CORE_PWRCTL_IO_HIGH	(1 << 3)

#define CORE_PWRCTL_BUS_SUCCESS	0x01
#define CORE_PWRCTL_BUS_FAIL	(1 << 1)
#define CORE_PWRCTL_IO_SUCCESS	(1 << 2)
#define CORE_PWRCTL_IO_FAIL	(1 << 3)

#define INT_MASK		0xF
#define MAX_PHASES		16

#define CORE_CMD_DAT_TRACK_SEL	(1 << 0)
#define CORE_DLL_EN		(1 << 16)
#define CORE_CDR_EN		(1 << 17)
#define CORE_CK_OUT_EN		(1 << 18)
#define CORE_CDR_EXT_EN		(1 << 19)
#define CORE_DLL_PDN		(1 << 29)
#define CORE_DLL_RST		(1 << 30)

#define CORE_DLL_LOCK		(1 << 7)
#define CORE_DDR_DLL_LOCK	(1 << 11)

#define CORE_CLK_PWRSAVE		(1 << 1)
#define CORE_VNDR_SPEC_ADMA_ERR_SIZE_EN	(1 << 7)
#define CORE_HC_MCLK_SEL_DFLT		(2 << 8)
#define CORE_HC_MCLK_SEL_HS400		(3 << 8)
#define CORE_HC_MCLK_SEL_MASK		(3 << 8)
#define CORE_HC_AUTO_CMD21_EN		(1 << 6)
#define CORE_IO_PAD_PWR_SWITCH_EN	(1 << 15)
#define CORE_IO_PAD_PWR_SWITCH	(1 << 16)
#define CORE_HC_SELECT_IN_EN	(1 << 18)
#define CORE_HC_SELECT_IN_HS400	(6 << 19)
#define CORE_HC_SELECT_IN_MASK	(7 << 19)
#define CORE_VENDOR_SPEC_POR_VAL	0xA1C

#define HC_SW_RST_WAIT_IDLE_DIS	(1 << 20)
#define HC_SW_RST_REQ (1 << 21)
#define CORE_ONE_MID_EN     (1 << 25)

#define CORE_8_BIT_SUPPORT		(1 << 18)
#define CORE_3_3V_SUPPORT		(1 << 24)
#define CORE_3_0V_SUPPORT		(1 << 25)
#define CORE_1_8V_SUPPORT		(1 << 26)
#define CORE_SYS_BUS_SUPPORT_64_BIT	BIT(28)

#define CORE_CSR_CDC_CTLR_CFG0		0x130
#define CORE_SW_TRIG_FULL_CALIB		(1 << 16)
#define CORE_HW_AUTOCAL_ENA		(1 << 17)

#define CORE_CSR_CDC_CTLR_CFG1		0x134
#define CORE_CSR_CDC_CAL_TIMER_CFG0	0x138
#define CORE_TIMER_ENA			(1 << 16)

#define CORE_CSR_CDC_CAL_TIMER_CFG1	0x13C
#define CORE_CSR_CDC_REFCOUNT_CFG	0x140
#define CORE_CSR_CDC_COARSE_CAL_CFG	0x144
#define CORE_CDC_OFFSET_CFG		0x14C
#define CORE_CSR_CDC_DELAY_CFG		0x150
#define CORE_CDC_SLAVE_DDA_CFG		0x160
#define CORE_CSR_CDC_STATUS0		0x164
#define CORE_CALIBRATION_DONE		(1 << 0)

#define CORE_CDC_ERROR_CODE_MASK	0x7000000

#define CQ_CMD_DBG_RAM	                0x110
#define CQ_CMD_DBG_RAM_WA               0x150
#define CQ_CMD_DBG_RAM_OL               0x154

#define CORE_CSR_CDC_GEN_CFG		0x178
#define CORE_CDC_SWITCH_BYPASS_OFF	(1 << 0)
#define CORE_CDC_SWITCH_RC_EN		(1 << 1)

#define CORE_CDC_T4_DLY_SEL		(1 << 0)
#define CORE_CMDIN_RCLK_EN		(1 << 1)
#define CORE_START_CDC_TRAFFIC		(1 << 6)

#define CORE_PWRSAVE_DLL	(1 << 3)
#define CORE_FIFO_ALT_EN	(1 << 10)
#define CORE_CMDEN_HS400_INPUT_MASK_CNT (1 << 13)

#define CORE_DDR_CAL_EN		(1 << 0)
#define CORE_FLL_CYCLE_CNT	(1 << 18)
#define CORE_DLL_CLOCK_DISABLE	(1 << 21)
#define DDR_CONFIG_POR_VAL	0x80040873

#define DLL_USR_CTL_POR_VAL		0x10800
#define ENABLE_DLL_LOCK_STATUS		(1 << 26)
#define FINE_TUNE_MODE_EN		(1 << 27)
#define BIAS_OK_SIGNAL			(1 << 29)
#define DLL_CONFIG_3_POR_VAL		0x10

/* 512 descriptors */
#define SDHCI_MSM_MAX_SEGMENTS  (1 << 9)

#define CORE_FREQ_100MHZ	(100 * 1000 * 1000)
#define TCXO_FREQ		19200000

#define INVALID_TUNING_PHASE	-1
#define sdhci_is_valid_gpio_wakeup_int(_h) ((_h)->pdata->sdiowakeup_irq >= 0)
#define sdhci_is_valid_gpio_testbus_trigger_int(_h) \
	((_h)->pdata->testbus_trigger_irq >= 0)

#define NUM_TUNING_PHASES		16
#define MAX_DRV_TYPES_SUPPORTED_HS200	4
#define MSM_AUTOSUSPEND_DELAY_MS 100

#define RCLK_TOGGLE 0x2

struct sdhci_msm_offset {
	u32 CORE_MCI_DATA_CNT;
	u32 CORE_MCI_STATUS;
	u32 CORE_MCI_FIFO_CNT;
	u32 CORE_MCI_VERSION;
	u32 CORE_GENERICS;
	u32 CORE_TESTBUS_CONFIG;
	u32 CORE_TESTBUS_SEL2_BIT;
	u32 CORE_TESTBUS_ENA;
	u32 CORE_TESTBUS_SEL2;
	u32 CORE_PWRCTL_STATUS;
	u32 CORE_PWRCTL_MASK;
	u32 CORE_PWRCTL_CLEAR;
	u32 CORE_PWRCTL_CTL;
	u32 CORE_SDCC_DEBUG_REG;
	u32 CORE_DLL_CONFIG;
	u32 CORE_DLL_STATUS;
	u32 CORE_VENDOR_SPEC;
	u32 CORE_VENDOR_SPEC_ADMA_ERR_ADDR0;
	u32 CORE_VENDOR_SPEC_ADMA_ERR_ADDR1;
	u32 CORE_VENDOR_SPEC_FUNC2;
	u32 CORE_VENDOR_SPEC_CAPABILITIES0;
	u32 CORE_DDR_200_CFG;
	u32 CORE_VENDOR_SPEC3;
	u32 CORE_DLL_CONFIG_2;
	u32 CORE_DLL_CONFIG_3;
	u32 CORE_DDR_CONFIG;
	u32 CORE_DDR_CONFIG_OLD; /* Applcable to sddcc minor ver < 0x49 only */
	u32 CORE_DLL_USR_CTL; /* Present on SDCC5.1 onwards */
};

struct sdhci_msm_offset sdhci_msm_offset_mci_removed = {
	.CORE_MCI_DATA_CNT = 0x35C,
	.CORE_MCI_STATUS = 0x324,
	.CORE_MCI_FIFO_CNT = 0x308,
	.CORE_MCI_VERSION = 0x318,
	.CORE_GENERICS = 0x320,
	.CORE_TESTBUS_CONFIG = 0x32C,
	.CORE_TESTBUS_SEL2_BIT = 3,
	.CORE_TESTBUS_ENA = (1 << 31),
	.CORE_TESTBUS_SEL2 = (1 << 3),
	.CORE_PWRCTL_STATUS = 0x240,
	.CORE_PWRCTL_MASK = 0x244,
	.CORE_PWRCTL_CLEAR = 0x248,
	.CORE_PWRCTL_CTL = 0x24C,
	.CORE_SDCC_DEBUG_REG = 0x358,
	.CORE_DLL_CONFIG = 0x200,
	.CORE_DLL_STATUS = 0x208,
	.CORE_VENDOR_SPEC = 0x20C,
	.CORE_VENDOR_SPEC_ADMA_ERR_ADDR0 = 0x214,
	.CORE_VENDOR_SPEC_ADMA_ERR_ADDR1 = 0x218,
	.CORE_VENDOR_SPEC_FUNC2 = 0x210,
	.CORE_VENDOR_SPEC_CAPABILITIES0 = 0x21C,
	.CORE_DDR_200_CFG = 0x224,
	.CORE_VENDOR_SPEC3 = 0x250,
	.CORE_DLL_CONFIG_2 = 0x254,
	.CORE_DLL_CONFIG_3 = 0x258,
	.CORE_DDR_CONFIG = 0x25C,
	.CORE_DLL_USR_CTL = 0x388,
};

struct sdhci_msm_offset sdhci_msm_offset_mci_present = {
	.CORE_MCI_DATA_CNT = 0x30,
	.CORE_MCI_STATUS = 0x34,
	.CORE_MCI_FIFO_CNT = 0x44,
	.CORE_MCI_VERSION = 0x050,
	.CORE_GENERICS = 0x70,
	.CORE_TESTBUS_CONFIG = 0x0CC,
	.CORE_TESTBUS_SEL2_BIT = 4,
	.CORE_TESTBUS_ENA = (1 << 3),
	.CORE_TESTBUS_SEL2 = (1 << 4),
	.CORE_PWRCTL_STATUS = 0xDC,
	.CORE_PWRCTL_MASK = 0xE0,
	.CORE_PWRCTL_CLEAR = 0xE4,
	.CORE_PWRCTL_CTL = 0xE8,
	.CORE_SDCC_DEBUG_REG = 0x124,
	.CORE_DLL_CONFIG = 0x100,
	.CORE_DLL_STATUS = 0x108,
	.CORE_VENDOR_SPEC = 0x10C,
	.CORE_VENDOR_SPEC_ADMA_ERR_ADDR0 = 0x114,
	.CORE_VENDOR_SPEC_ADMA_ERR_ADDR1 = 0x118,
	.CORE_VENDOR_SPEC_FUNC2 = 0x110,
	.CORE_VENDOR_SPEC_CAPABILITIES0 = 0x11C,
	.CORE_DDR_200_CFG = 0x184,
	.CORE_VENDOR_SPEC3 = 0x1B0,
	.CORE_DLL_CONFIG_2 = 0x1B4,
	.CORE_DLL_CONFIG_3 = 0x1B8,
	.CORE_DDR_CONFIG_OLD = 0x1B8, /* Applicable to sdcc minor ver < 0x49 */
	.CORE_DDR_CONFIG = 0x1BC,
};

u8 sdhci_msm_readb_relaxed(struct sdhci_host *host, u32 offset)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	void __iomem *base_addr;

	if (msm_host->mci_removed)
		base_addr = host->ioaddr;
	else
		base_addr = msm_host->core_mem;

	return readb_relaxed(base_addr + offset);
}

u32 sdhci_msm_readl_relaxed(struct sdhci_host *host, u32 offset)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	void __iomem *base_addr;

	if (msm_host->mci_removed)
		base_addr = host->ioaddr;
	else
		base_addr = msm_host->core_mem;

	return readl_relaxed(base_addr + offset);
}

void sdhci_msm_writeb_relaxed(u8 val, struct sdhci_host *host, u32 offset)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	void __iomem *base_addr;

	if (msm_host->mci_removed)
		base_addr = host->ioaddr;
	else
		base_addr = msm_host->core_mem;

	writeb_relaxed(val, base_addr + offset);
}

void sdhci_msm_writel_relaxed(u32 val, struct sdhci_host *host, u32 offset)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	void __iomem *base_addr;

	if (msm_host->mci_removed)
		base_addr = host->ioaddr;
	else
		base_addr = msm_host->core_mem;

	writel_relaxed(val, base_addr + offset);
}

/* Timeout value to avoid infinite waiting for pwr_irq */
#define MSM_PWR_IRQ_TIMEOUT_MS 5000

static const u32 tuning_block_64[] = {
	0x00FF0FFF, 0xCCC3CCFF, 0xFFCC3CC3, 0xEFFEFFFE,
	0xDDFFDFFF, 0xFBFFFBFF, 0xFF7FFFBF, 0xEFBDF777,
	0xF0FFF0FF, 0x3CCCFC0F, 0xCFCC33CC, 0xEEFFEFFF,
	0xFDFFFDFF, 0xFFBFFFDF, 0xFFF7FFBB, 0xDE7B7FF7
};

static const u32 tuning_block_128[] = {
	0xFF00FFFF, 0x0000FFFF, 0xCCCCFFFF, 0xCCCC33CC,
	0xCC3333CC, 0xFFFFCCCC, 0xFFFFEEFF, 0xFFEEEEFF,
	0xFFDDFFFF, 0xDDDDFFFF, 0xBBFFFFFF, 0xBBFFFFFF,
	0xFFFFFFBB, 0xFFFFFF77, 0x77FF7777, 0xFFEEDDBB,
	0x00FFFFFF, 0x00FFFFFF, 0xCCFFFF00, 0xCC33CCCC,
	0x3333CCCC, 0xFFCCCCCC, 0xFFEEFFFF, 0xEEEEFFFF,
	0xDDFFFFFF, 0xDDFFFFFF, 0xFFFFFFDD, 0xFFFFFFBB,
	0xFFFFBBBB, 0xFFFF77FF, 0xFF7777FF, 0xEEDDBB77
};

/* global to hold each slot instance for debug */
static struct sdhci_msm_host *sdhci_slot[2];

static int disable_slots;
static int bus_mode;
/* root can write, others read */
module_param(disable_slots, int, 0644);
module_param(bus_mode, int, 0444);
MODULE_PARM_DESC(bus_mode, "Set this parameter during bootup to set the desired bus speed mode for eMMC only.");

enum vdd_io_level {
	/* set vdd_io_data->low_vol_level */
	VDD_IO_LOW,
	/* set vdd_io_data->high_vol_level */
	VDD_IO_HIGH,
	/*
	 * set whatever there in voltage_level (third argument) of
	 * sdhci_msm_set_vdd_io_vol() function.
	 */
	VDD_IO_SET_LEVEL,
};

enum dll_init_context {
	DLL_INIT_NORMAL = 0,
	DLL_INIT_FROM_CX_COLLAPSE_EXIT,
};

static unsigned int sdhci_msm_get_sup_clk_rate(struct sdhci_host *host,
						u32 req_clk);

/* MSM platform specific tuning */
static inline int msm_dll_poll_ck_out_en(struct sdhci_host *host,
						u8 poll)
{
	int rc = 0;
	u32 wait_cnt = 50;
	u8 ck_out_en = 0;
	struct mmc_host *mmc = host->mmc;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;

	/* poll for CK_OUT_EN bit.  max. poll time = 50us */
	ck_out_en = !!(readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG) & CORE_CK_OUT_EN);

	while (ck_out_en != poll) {
		if (--wait_cnt == 0) {
			pr_err("%s: %s: CK_OUT_EN bit is not %d\n",
				mmc_hostname(mmc), __func__, poll);
			rc = -ETIMEDOUT;
			goto out;
		}
		udelay(1);

		ck_out_en = !!(readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG) & CORE_CK_OUT_EN);
	}
out:
	return rc;
}

/*
 * Enable CDR to track changes of DAT lines and adjust sampling
 * point according to voltage/temperature variations
 */
static int msm_enable_cdr_cm_sdc4_dll(struct sdhci_host *host)
{
	int rc = 0;
	u32 config;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;

	config = readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG);
	config |= CORE_CDR_EN;
	config &= ~(CORE_CDR_EXT_EN | CORE_CK_OUT_EN);
	writel_relaxed(config, host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG);

	rc = msm_dll_poll_ck_out_en(host, 0);
	if (rc)
		goto err;

	writel_relaxed((readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG) | CORE_CK_OUT_EN),
		host->ioaddr + msm_host_offset->CORE_DLL_CONFIG);

	rc = msm_dll_poll_ck_out_en(host, 1);
	if (rc)
		goto err;
	goto out;
err:
	pr_err("%s: %s: failed\n", mmc_hostname(host->mmc), __func__);
out:
	return rc;
}

static ssize_t store_auto_cmd21(struct device *dev, struct device_attribute
				*attr, const char *buf, size_t count)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	u32 tmp;
	unsigned long flags;

	if (!kstrtou32(buf, 0, &tmp)) {
		spin_lock_irqsave(&host->lock, flags);
		msm_host->en_auto_cmd21 = !!tmp;
		spin_unlock_irqrestore(&host->lock, flags);
	}
	return count;
}

static ssize_t show_auto_cmd21(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	return snprintf(buf, PAGE_SIZE, "%d\n", msm_host->en_auto_cmd21);
}

/* MSM auto-tuning handler */
static int sdhci_msm_config_auto_tuning_cmd(struct sdhci_host *host,
					    bool enable,
					    u32 type)
{
	int rc = 0;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;
	u32 val = 0;

	if (!msm_host->en_auto_cmd21)
		return 0;

	if (type == MMC_SEND_TUNING_BLOCK_HS200)
		val = CORE_HC_AUTO_CMD21_EN;
	else
		return 0;

	if (enable) {
		rc = msm_enable_cdr_cm_sdc4_dll(host);
		writel_relaxed(readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC) | val,
			host->ioaddr + msm_host_offset->CORE_VENDOR_SPEC);
	} else {
		writel_relaxed(readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC) & ~val,
			host->ioaddr + msm_host_offset->CORE_VENDOR_SPEC);
	}
	return rc;
}

static int msm_config_cm_dll_phase(struct sdhci_host *host, u8 phase)
{
	int rc = 0;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;
	u8 grey_coded_phase_table[] = {0x0, 0x1, 0x3, 0x2, 0x6, 0x7, 0x5, 0x4,
					0xC, 0xD, 0xF, 0xE, 0xA, 0xB, 0x9,
					0x8};
	unsigned long flags;
	u32 config;
	struct mmc_host *mmc = host->mmc;

	pr_debug("%s: Enter %s\n", mmc_hostname(mmc), __func__);
	spin_lock_irqsave(&host->lock, flags);

	config = readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG);
	config &= ~(CORE_CDR_EN | CORE_CK_OUT_EN);
	config |= (CORE_CDR_EXT_EN | CORE_DLL_EN);
	writel_relaxed(config, host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG);

	/* Wait until CK_OUT_EN bit of DLL_CONFIG register becomes '0' */
	rc = msm_dll_poll_ck_out_en(host, 0);
	if (rc)
		goto err_out;

	/*
	 * Write the selected DLL clock output phase (0 ... 15)
	 * to CDR_SELEXT bit field of DLL_CONFIG register.
	 */
	writel_relaxed(((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG)
			& ~(0xF << 20))
			| (grey_coded_phase_table[phase] << 20)),
			host->ioaddr + msm_host_offset->CORE_DLL_CONFIG);

	/* Set CK_OUT_EN bit of DLL_CONFIG register to 1. */
	writel_relaxed((readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG) | CORE_CK_OUT_EN),
		host->ioaddr + msm_host_offset->CORE_DLL_CONFIG);

	/* Wait until CK_OUT_EN bit of DLL_CONFIG register becomes '1' */
	rc = msm_dll_poll_ck_out_en(host, 1);
	if (rc)
		goto err_out;

	config = readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG);
	config |= CORE_CDR_EN;
	config &= ~CORE_CDR_EXT_EN;
	writel_relaxed(config, host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG);
	goto out;

err_out:
	pr_err("%s: %s: Failed to set DLL phase: %d\n",
		mmc_hostname(mmc), __func__, phase);
out:
	spin_unlock_irqrestore(&host->lock, flags);
	pr_debug("%s: Exit %s\n", mmc_hostname(mmc), __func__);
	return rc;
}

/*
 * Find out the greatest range of consecuitive selected
 * DLL clock output phases that can be used as sampling
 * setting for SD3.0 UHS-I card read operation (in SDR104
 * timing mode) or for eMMC4.5 card read operation (in
 * HS400/HS200 timing mode).
 * Select the 3/4 of the range and configure the DLL with the
 * selected DLL clock output phase.
 */

static int msm_find_most_appropriate_phase(struct sdhci_host *host,
				u8 *phase_table, u8 total_phases)
{
	int ret;
	u8 ranges[MAX_PHASES][MAX_PHASES] = { {0}, {0} };
	u8 phases_per_row[MAX_PHASES] = {0};
	int row_index = 0, col_index = 0, selected_row_index = 0, curr_max = 0;
	int i, cnt, phase_0_raw_index = 0, phase_15_raw_index = 0;
	bool phase_0_found = false, phase_15_found = false;
	struct mmc_host *mmc = host->mmc;

	pr_debug("%s: Enter %s\n", mmc_hostname(mmc), __func__);
	if (!total_phases || (total_phases > MAX_PHASES)) {
		pr_err("%s: %s: invalid argument: total_phases=%d\n",
			mmc_hostname(mmc), __func__, total_phases);
		return -EINVAL;
	}

	for (cnt = 0; cnt < total_phases; cnt++) {
		ranges[row_index][col_index] = phase_table[cnt];
		phases_per_row[row_index] += 1;
		col_index++;

		if ((cnt + 1) == total_phases) {
			continue;
		/* check if next phase in phase_table is consecutive or not */
		} else if ((phase_table[cnt] + 1) != phase_table[cnt + 1]) {
			row_index++;
			col_index = 0;
		}
	}

	if (row_index >= MAX_PHASES)
		return -EINVAL;

	/* Check if phase-0 is present in first valid window? */
	if (!ranges[0][0]) {
		phase_0_found = true;
		phase_0_raw_index = 0;
		/* Check if cycle exist between 2 valid windows */
		for (cnt = 1; cnt <= row_index; cnt++) {
			if (phases_per_row[cnt]) {
				for (i = 0; i < phases_per_row[cnt]; i++) {
					if (ranges[cnt][i] == 15) {
						phase_15_found = true;
						phase_15_raw_index = cnt;
						break;
					}
				}
			}
		}
	}

	/* If 2 valid windows form cycle then merge them as single window */
	if (phase_0_found && phase_15_found) {
		/* number of phases in raw where phase 0 is present */
		u8 phases_0 = phases_per_row[phase_0_raw_index];
		/* number of phases in raw where phase 15 is present */
		u8 phases_15 = phases_per_row[phase_15_raw_index];

		if (phases_0 + phases_15 >= MAX_PHASES)
			/*
			 * If there are more than 1 phase windows then total
			 * number of phases in both the windows should not be
			 * more than or equal to MAX_PHASES.
			 */
			return -EINVAL;

		/* Merge 2 cyclic windows */
		i = phases_15;
		for (cnt = 0; cnt < phases_0; cnt++) {
			ranges[phase_15_raw_index][i] =
				ranges[phase_0_raw_index][cnt];
			if (++i >= MAX_PHASES)
				break;
		}

		phases_per_row[phase_0_raw_index] = 0;
		phases_per_row[phase_15_raw_index] = phases_15 + phases_0;
	}

	for (cnt = 0; cnt <= row_index; cnt++) {
		if (phases_per_row[cnt] > curr_max) {
			curr_max = phases_per_row[cnt];
			selected_row_index = cnt;
		}
	}

	i = ((curr_max * 3) / 4);
	if (i)
		i--;

	ret = (int)ranges[selected_row_index][i];

	if (ret >= MAX_PHASES) {
		ret = -EINVAL;
		pr_err("%s: %s: invalid phase selected=%d\n",
			mmc_hostname(mmc), __func__, ret);
	}

	pr_debug("%s: Exit %s\n", mmc_hostname(mmc), __func__);
	return ret;
}

static inline void msm_cm_dll_set_freq(struct sdhci_host *host)
{
	u32 mclk_freq = 0;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;

	/* Program the MCLK value to MCLK_FREQ bit field */
	if (host->clock <= 112000000)
		mclk_freq = 0;
	else if (host->clock <= 125000000)
		mclk_freq = 1;
	else if (host->clock <= 137000000)
		mclk_freq = 2;
	else if (host->clock <= 150000000)
		mclk_freq = 3;
	else if (host->clock <= 162000000)
		mclk_freq = 4;
	else if (host->clock <= 175000000)
		mclk_freq = 5;
	else if (host->clock <= 187000000)
		mclk_freq = 6;
	else if (host->clock <= 208000000)
		mclk_freq = 7;

	writel_relaxed(((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG)
			& ~(7 << 24)) | (mclk_freq << 24)),
			host->ioaddr + msm_host_offset->CORE_DLL_CONFIG);
}

/* Initialize the DLL (Programmable Delay Line ) */
static int msm_init_cm_dll(struct sdhci_host *host,
				enum dll_init_context init_context)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;
	struct mmc_host *mmc = host->mmc;
	int rc = 0;
	unsigned long flags;
	u32 wait_cnt;
	bool prev_pwrsave, curr_pwrsave;

	pr_debug("%s: Enter %s\n", mmc_hostname(mmc), __func__);
	spin_lock_irqsave(&host->lock, flags);
	prev_pwrsave = !!(readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_VENDOR_SPEC) & CORE_CLK_PWRSAVE);
	curr_pwrsave = prev_pwrsave;
	/*
	 * Make sure that clock is always enabled when DLL
	 * tuning is in progress. Keeping PWRSAVE ON may
	 * turn off the clock. So let's disable the PWRSAVE
	 * here and re-enable it once tuning is completed.
	 */
	if (prev_pwrsave) {
		writel_relaxed((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC)
			& ~CORE_CLK_PWRSAVE), host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC);
		curr_pwrsave = false;
	}

	if (msm_host->use_updated_dll_reset) {
		/* Disable the DLL clock */
		writel_relaxed((readl_relaxed(host->ioaddr +
				msm_host_offset->CORE_DLL_CONFIG)
				& ~CORE_CK_OUT_EN), host->ioaddr +
				msm_host_offset->CORE_DLL_CONFIG);

		writel_relaxed((readl_relaxed(host->ioaddr +
				msm_host_offset->CORE_DLL_CONFIG_2)
				| CORE_DLL_CLOCK_DISABLE), host->ioaddr +
				msm_host_offset->CORE_DLL_CONFIG_2);
	}

	/* Write 1 to DLL_RST bit of DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG) | CORE_DLL_RST),
		host->ioaddr + msm_host_offset->CORE_DLL_CONFIG);

	/* Write 1 to DLL_PDN bit of DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG) | CORE_DLL_PDN),
		host->ioaddr + msm_host_offset->CORE_DLL_CONFIG);

	if (msm_host->use_updated_dll_reset) {
		u32 mclk_freq = 0;
		u32 actual_clk = sdhci_msm_get_sup_clk_rate(host, host->clock);

		/*
		 * Only configure the mclk_freq in normal DLL init
		 * context. If the DLL init is coming from
		 * CX Collapse Exit context, the host->clock may be zero.
		 * The DLL_CONFIG_2 register has already been restored to
		 * proper value prior to getting here.
		 */
		if (init_context == DLL_INIT_NORMAL) {
			switch (actual_clk) {
			case 208000000:
			case 202000000:
			case 201500000:
			case 200000000:
				mclk_freq = 42;
				break;
			case 192000000:
				mclk_freq = 40;
				break;
			default:
				mclk_freq = (u32)((actual_clk / TCXO_FREQ) * 4);
				pr_info_once("%s: %s: Non standard clk freq =%u\n",
				mmc_hostname(mmc), __func__, actual_clk);
			}

			if ((readl_relaxed(host->ioaddr +
				msm_host_offset->CORE_DLL_CONFIG_2)
				& CORE_FLL_CYCLE_CNT))
				mclk_freq *= 2;

			writel_relaxed(((readl_relaxed(host->ioaddr +
			   msm_host_offset->CORE_DLL_CONFIG_2)
			   & ~(0xFF << 10)) | (mclk_freq << 10)),
			   host->ioaddr + msm_host_offset->CORE_DLL_CONFIG_2);
		}
		/* wait for 5us before enabling DLL clock */
		udelay(5);
	}

	/* Write 0 to DLL_RST bit of DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG) & ~CORE_DLL_RST),
			host->ioaddr + msm_host_offset->CORE_DLL_CONFIG);

	/* Write 0 to DLL_PDN bit of DLL_CONFIG register */
	writel_relaxed((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG) & ~CORE_DLL_PDN),
			host->ioaddr + msm_host_offset->CORE_DLL_CONFIG);

	if (msm_host->use_updated_dll_reset) {
		/* Enable the DLL clock */
		writel_relaxed((readl_relaxed(host->ioaddr +
				msm_host_offset->CORE_DLL_CONFIG_2)
				& ~CORE_DLL_CLOCK_DISABLE), host->ioaddr +
				msm_host_offset->CORE_DLL_CONFIG_2);
	}

	/* Configure Tassadar DLL (Only applicable for 7FF projects) */
	if (msm_host->use_7nm_dll) {
		if (msm_host->dll_hsr) {
			writel_relaxed(msm_host->dll_hsr->dll_usr_ctl,
					host->ioaddr +
					msm_host_offset->CORE_DLL_USR_CTL);
			writel_relaxed(msm_host->dll_hsr->dll_config_3,
					host->ioaddr +
					msm_host_offset->CORE_DLL_CONFIG_3);
		} else {
			writel_relaxed(DLL_USR_CTL_POR_VAL | FINE_TUNE_MODE_EN |
					ENABLE_DLL_LOCK_STATUS | BIAS_OK_SIGNAL,
					host->ioaddr +
					msm_host_offset->CORE_DLL_USR_CTL);

			writel_relaxed(DLL_CONFIG_3_POR_VAL, host->ioaddr +
				msm_host_offset->CORE_DLL_CONFIG_3);
		}
	}

	/*
	 * Update the lower two bytes of DLL_CONFIG only with HSR values.
	 * Since these are the static settings.
	 */
	if (msm_host->dll_hsr) {
		writel_relaxed((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG) |
			(msm_host->dll_hsr->dll_config & 0xffff)),
			host->ioaddr + msm_host_offset->CORE_DLL_CONFIG);
	}

	/* Set DLL_EN bit to 1. */
	writel_relaxed((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG) | CORE_DLL_EN),
			host->ioaddr + msm_host_offset->CORE_DLL_CONFIG);

	/* Set CK_OUT_EN bit to 1. */
	writel_relaxed((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG)
			| CORE_CK_OUT_EN), host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG);

	/* For hs400es mode, no need to wait for core dll lock */
	if (msm_host->mmc->card
			&& !(msm_host->enhanced_strobe &&
				mmc_card_strobe(msm_host->mmc->card))) {
		wait_cnt = 50;
		/* Wait until DLL_LOCK bit of DLL_STATUS register becomes '1' */
		while (!(readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DLL_STATUS) & CORE_DLL_LOCK)) {
			/* max. wait for 50us sec for LOCK bit to be set */
			if (--wait_cnt == 0) {
				pr_err("%s: %s: DLL failed to LOCK\n",
					mmc_hostname(mmc), __func__);
				rc = -ETIMEDOUT;
				goto out;
			}
			/* wait for 1us before polling again */
			udelay(1);
		}
	}

out:
	/* Restore the correct PWRSAVE state */
	if (prev_pwrsave ^ curr_pwrsave) {
		u32 reg = readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC);

		if (prev_pwrsave)
			reg |= CORE_CLK_PWRSAVE;
		else
			reg &= ~CORE_CLK_PWRSAVE;

		writel_relaxed(reg, host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC);
	}

	spin_unlock_irqrestore(&host->lock, flags);
	pr_debug("%s: Exit %s\n", mmc_hostname(mmc), __func__);
	return rc;
}

static int sdhci_msm_cdclp533_calibration(struct sdhci_host *host)
{
	u32 calib_done;
	int ret = 0;
	int cdc_err = 0;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;

	pr_debug("%s: Enter %s\n", mmc_hostname(host->mmc), __func__);

	/* Write 0 to CDC_T4_DLY_SEL field in VENDOR_SPEC_DDR200_CFG */
	writel_relaxed((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DDR_200_CFG)
			& ~CORE_CDC_T4_DLY_SEL),
			host->ioaddr + msm_host_offset->CORE_DDR_200_CFG);

	/* Write 0 to CDC_SWITCH_BYPASS_OFF field in CORE_CSR_CDC_GEN_CFG */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_CSR_CDC_GEN_CFG)
			& ~CORE_CDC_SWITCH_BYPASS_OFF),
			host->ioaddr + CORE_CSR_CDC_GEN_CFG);

	/* Write 1 to CDC_SWITCH_RC_EN field in CORE_CSR_CDC_GEN_CFG */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_CSR_CDC_GEN_CFG)
			| CORE_CDC_SWITCH_RC_EN),
			host->ioaddr + CORE_CSR_CDC_GEN_CFG);

	/* Write 0 to START_CDC_TRAFFIC field in CORE_DDR200_CFG */
	writel_relaxed((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DDR_200_CFG)
			& ~CORE_START_CDC_TRAFFIC),
			host->ioaddr + msm_host_offset->CORE_DDR_200_CFG);

	/*
	 * Perform CDC Register Initialization Sequence
	 *
	 * CORE_CSR_CDC_CTLR_CFG0	0x11800EC
	 * CORE_CSR_CDC_CTLR_CFG1	0x3011111
	 * CORE_CSR_CDC_CAL_TIMER_CFG0	0x1201000
	 * CORE_CSR_CDC_CAL_TIMER_CFG1	0x4
	 * CORE_CSR_CDC_REFCOUNT_CFG	0xCB732020
	 * CORE_CSR_CDC_COARSE_CAL_CFG	0xB19
	 * CORE_CSR_CDC_DELAY_CFG	0x3AC
	 * CORE_CDC_OFFSET_CFG		0x0
	 * CORE_CDC_SLAVE_DDA_CFG	0x16334
	 */

	writel_relaxed(0x11800EC, host->ioaddr + CORE_CSR_CDC_CTLR_CFG0);
	writel_relaxed(0x3011111, host->ioaddr + CORE_CSR_CDC_CTLR_CFG1);
	writel_relaxed(0x1201000, host->ioaddr + CORE_CSR_CDC_CAL_TIMER_CFG0);
	writel_relaxed(0x4, host->ioaddr + CORE_CSR_CDC_CAL_TIMER_CFG1);
	writel_relaxed(0xCB732020, host->ioaddr + CORE_CSR_CDC_REFCOUNT_CFG);
	writel_relaxed(0xB19, host->ioaddr + CORE_CSR_CDC_COARSE_CAL_CFG);
	writel_relaxed(0x4E2, host->ioaddr + CORE_CSR_CDC_DELAY_CFG);
	writel_relaxed(0x0, host->ioaddr + CORE_CDC_OFFSET_CFG);
	writel_relaxed(0x16334, host->ioaddr + CORE_CDC_SLAVE_DDA_CFG);

	/* CDC HW Calibration */

	/* Write 1 to SW_TRIG_FULL_CALIB field in CORE_CSR_CDC_CTLR_CFG0 */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_CSR_CDC_CTLR_CFG0)
			| CORE_SW_TRIG_FULL_CALIB),
			host->ioaddr + CORE_CSR_CDC_CTLR_CFG0);

	/* Write 0 to SW_TRIG_FULL_CALIB field in CORE_CSR_CDC_CTLR_CFG0 */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_CSR_CDC_CTLR_CFG0)
			& ~CORE_SW_TRIG_FULL_CALIB),
			host->ioaddr + CORE_CSR_CDC_CTLR_CFG0);

	/* Write 1 to HW_AUTOCAL_ENA field in CORE_CSR_CDC_CTLR_CFG0 */
	writel_relaxed((readl_relaxed(host->ioaddr + CORE_CSR_CDC_CTLR_CFG0)
			| CORE_HW_AUTOCAL_ENA),
			host->ioaddr + CORE_CSR_CDC_CTLR_CFG0);

	/* Write 1 to TIMER_ENA field in CORE_CSR_CDC_CAL_TIMER_CFG0 */
	writel_relaxed((readl_relaxed(host->ioaddr +
			CORE_CSR_CDC_CAL_TIMER_CFG0) | CORE_TIMER_ENA),
			host->ioaddr + CORE_CSR_CDC_CAL_TIMER_CFG0);

	/*
	 * SDHC has core_mem and hc_mem device memory and these memory
	 * addresses do not fall within 1KB region. Hence, any update to
	 * core_mem address space would require an mb() to ensure this gets
	 * completed before its next update to registers within hc_mem.
	 */
	mb();

	/* Poll on CALIBRATION_DONE field in CORE_CSR_CDC_STATUS0 to be 1 */
	ret = readl_poll_timeout(host->ioaddr + CORE_CSR_CDC_STATUS0,
		 calib_done, (calib_done & CORE_CALIBRATION_DONE), 1, 50);

	if (ret == -ETIMEDOUT) {
		pr_err("%s: %s: CDC Calibration was not completed\n",
				mmc_hostname(host->mmc), __func__);
		goto out;
	}

	/* Verify CDC_ERROR_CODE field in CORE_CSR_CDC_STATUS0 is 0 */
	cdc_err = readl_relaxed(host->ioaddr + CORE_CSR_CDC_STATUS0)
			& CORE_CDC_ERROR_CODE_MASK;
	if (cdc_err) {
		pr_err("%s: %s: CDC Error Code %d\n",
			mmc_hostname(host->mmc), __func__, cdc_err);
		ret = -EINVAL;
		goto out;
	}

	/* Write 1 to START_CDC_TRAFFIC field in CORE_DDR200_CFG */
	writel_relaxed((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DDR_200_CFG)
			| CORE_START_CDC_TRAFFIC),
			host->ioaddr + msm_host_offset->CORE_DDR_200_CFG);
out:
	pr_debug("%s: Exit %s, ret:%d\n", mmc_hostname(host->mmc),
			__func__, ret);
	return ret;
}

static int sdhci_msm_cm_dll_sdc4_calibration(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;
	u32 dll_status;
	int ret = 0;

	pr_debug("%s: Enter %s\n", mmc_hostname(host->mmc), __func__);

	/*
	 * Reprogramming the value in case it might have been modified by
	 * bootloaders.
	 */
	if (msm_host->dll_hsr && msm_host->dll_hsr->ddr_config) {
		writel_relaxed(msm_host->dll_hsr->ddr_config, host->ioaddr +
			msm_host_offset->CORE_DDR_CONFIG);
	} else if (msm_host->rclk_delay_fix) {
		writel_relaxed(DDR_CONFIG_POR_VAL, host->ioaddr +
			msm_host_offset->CORE_DDR_CONFIG);
	} else {
		writel_relaxed(DDR_CONFIG_POR_VAL, host->ioaddr +
			msm_host_offset->CORE_DDR_CONFIG_OLD);
	}

	if (msm_host->enhanced_strobe && mmc_card_strobe(msm_host->mmc->card))
		writel_relaxed((readl_relaxed(host->ioaddr +
				msm_host_offset->CORE_DDR_200_CFG)
				| CORE_CMDIN_RCLK_EN), host->ioaddr +
				msm_host_offset->CORE_DDR_200_CFG);

	/* Write 1 to DDR_CAL_EN field in CORE_DLL_CONFIG_2 */
	writel_relaxed((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG_2)
			| CORE_DDR_CAL_EN),
			host->ioaddr + msm_host_offset->CORE_DLL_CONFIG_2);

	/* Poll on DDR_DLL_LOCK bit in CORE_DLL_STATUS to be set */
	ret = readl_poll_timeout(host->ioaddr +
		 msm_host_offset->CORE_DLL_STATUS,
		 dll_status, (dll_status & CORE_DDR_DLL_LOCK), 10, 1000);

	if (ret == -ETIMEDOUT) {
		pr_err("%s: %s: CM_DLL_SDC4 Calibration was not completed\n",
				mmc_hostname(host->mmc), __func__);
		goto out;
	}

	/*
	 * set CORE_PWRSAVE_DLL bit in CORE_VENDOR_SPEC3.
	 * when MCLK is gated OFF, it is not gated for less than 0.5us
	 * and MCLK must be switched on for at-least 1us before DATA
	 * starts coming. Controllers with 14lpp tech DLL cannot
	 * guarantee above requirement. So PWRSAVE_DLL should not be
	 * turned on for host controllers using this DLL.
	 */
	if (!msm_host->use_14lpp_dll)
		writel_relaxed((readl_relaxed(host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC3)
				| CORE_PWRSAVE_DLL), host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC3);
	/*
	 * SDHC has core_mem and hc_mem device memory and these memory
	 * addresses do not fall within 1KB region. Hence, any update to
	 * core_mem address space would require an mb() to ensure this
	 * gets completed before its next update to registers within
	 * hc_mem.
	 */
	mb(); /* Register operation sync */
out:
	pr_debug("%s: Exit %s, ret:%d\n", mmc_hostname(host->mmc),
			__func__, ret);
	return ret;
}

static int sdhci_msm_enhanced_strobe(struct sdhci_host *host)
{
	int ret = 0;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct mmc_host *mmc = host->mmc;

	pr_debug("%s: Enter %s\n", mmc_hostname(host->mmc), __func__);

	if (!msm_host->enhanced_strobe || !mmc_card_strobe(mmc->card)) {
		pr_debug("%s: host/card does not support hs400 enhanced strobe\n",
				mmc_hostname(mmc));
		return -EINVAL;
	}

	if (msm_host->calibration_done ||
		!(mmc->ios.timing == MMC_TIMING_MMC_HS400)) {
		return 0;
	}

	/*
	 * Reset the tuning block.
	 */
	ret = msm_init_cm_dll(host, DLL_INIT_NORMAL);
	if (ret)
		goto out;

	ret = sdhci_msm_cm_dll_sdc4_calibration(host);
out:
	if (!ret)
		msm_host->calibration_done = true;
	pr_debug("%s: Exit %s, ret:%d\n", mmc_hostname(host->mmc),
			__func__, ret);
	return ret;
}

static int sdhci_msm_hs400_dll_calibration(struct sdhci_host *host)
{
	int ret = 0;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;

	pr_debug("%s: Enter %s\n", mmc_hostname(host->mmc), __func__);

	/*
	 * Retuning in HS400 (DDR mode) will fail, just reset the
	 * tuning block and restore the saved tuning phase.
	 */
	ret = msm_init_cm_dll(host, DLL_INIT_NORMAL);
	if (ret)
		goto out;

	/* Set the selected phase in delay line hw block */
	ret = msm_config_cm_dll_phase(host, msm_host->saved_tuning_phase);
	if (ret)
		goto out;

	/* Write 1 to CMD_DAT_TRACK_SEL field in DLL_CONFIG */
	writel_relaxed((readl_relaxed(host->ioaddr +
				msm_host_offset->CORE_DLL_CONFIG)
				| CORE_CMD_DAT_TRACK_SEL), host->ioaddr +
				msm_host_offset->CORE_DLL_CONFIG);

	if (msm_host->use_cdclp533)
		/* Calibrate CDCLP533 DLL HW */
		ret = sdhci_msm_cdclp533_calibration(host);
	else
		/* Calibrate CM_DLL_SDC4 HW */
		ret = sdhci_msm_cm_dll_sdc4_calibration(host);
out:
	pr_debug("%s: Exit %s, ret:%d\n", mmc_hostname(host->mmc),
			__func__, ret);
	return ret;
}

static void sdhci_msm_set_mmc_drv_type(struct sdhci_host *host, u32 opcode,
		u8 drv_type)
{
	struct mmc_command cmd = {0};
	struct mmc_request mrq = {NULL};
	struct mmc_host *mmc = host->mmc;
	u8 val = ((drv_type << 4) | 2);

	cmd.opcode = MMC_SWITCH;
	cmd.arg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
		(EXT_CSD_HS_TIMING << 16) |
		(val << 8) |
		EXT_CSD_CMD_SET_NORMAL;
	cmd.flags = MMC_CMD_AC | MMC_RSP_R1B;
	/* 1 sec */
	cmd.busy_timeout = 1000 * 1000;

	memset(cmd.resp, 0, sizeof(cmd.resp));
	cmd.retries = 3;

	mrq.cmd = &cmd;
	cmd.data = NULL;

	mmc_wait_for_req(mmc, &mrq);
	pr_debug("%s: %s: set card drive type to %d\n",
			mmc_hostname(mmc), __func__,
			drv_type);
}

#define MAX_TESTBUS 127
#define IPCAT_MINOR_MASK(val) ((val & 0x0fff0000) >> 0x10)
#define TB_CONF_MASK 0x7f
#define TB_TRIG_CONF 0xff80ffff
#define TB_WRITE_STATUS BIT(8)

/*
 * This function needs to be used when getting mask and
 * match pattern either from cmdline or sysfs
 */
void sdhci_msm_mm_dbg_configure(struct sdhci_host *host, u32 mask,
			u32 match, u32 bit_shift, u32 testbus)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct platform_device *pdev = msm_host->pdev;
	u32 val;
	u32 enable_dbg_feature = 0;
	int ret = 0;

	if (testbus > MAX_TESTBUS) {
		dev_err(&pdev->dev, "%s: testbus should be less than 128.\n",
						__func__);
		return;
	}

	/* Enable debug mode */
	writel_relaxed(ENABLE_DBG,
			host->ioaddr + SDCC_TESTBUS_CONFIG);
	writel_relaxed(DUMMY,
			host->ioaddr + SDCC_DEBUG_EN_DIS_REG);
	writel_relaxed((readl_relaxed(host->ioaddr +
			SDCC_TESTBUS_CONFIG) | TESTBUS_EN),
			host->ioaddr + SDCC_TESTBUS_CONFIG);

	/* Enable particular feature */
	enable_dbg_feature |= MM_TRIGGER_DISABLE;
	writel_relaxed((readl_relaxed(host->ioaddr +
			SDCC_DEBUG_FEATURE_CFG_REG) | enable_dbg_feature),
			host->ioaddr + SDCC_DEBUG_FEATURE_CFG_REG);

	/* Configure Mask & Match pattern*/
	writel_relaxed((mask << bit_shift),
			host->ioaddr + SDCC_DEBUG_MASK_PATTERN_REG);
	writel_relaxed((match << bit_shift),
			host->ioaddr + SDCC_DEBUG_MATCH_PATTERN_REG);

	/* Configure test bus for above mm */
	writel_relaxed((testbus & TB_CONF_MASK), host->ioaddr +
			SDCC_DEBUG_MM_TB_CFG_REG);
	/* Initiate conf shifting */
	writel_relaxed(BIT(8),
			host->ioaddr + SDCC_DEBUG_MM_TB_CFG_REG);

	/* Wait for test bus to be configured */
	ret = readl_poll_timeout(host->ioaddr + SDCC_DEBUG_MM_TB_CFG_REG,
			val, !(val & TB_WRITE_STATUS), 50, 1000);
	if (ret == -ETIMEDOUT)
		pr_err("%s: Unable to set mask & match\n",
				mmc_hostname(host->mmc));

	/* Direct test bus to GPIO */
	writel_relaxed(((readl_relaxed(host->ioaddr +
				SDCC_TESTBUS_CONFIG) & TB_TRIG_CONF)
				| (testbus << 16)), host->ioaddr +
				SDCC_TESTBUS_CONFIG);

	/* Read back to ensure write went through */
	readl_relaxed(host->ioaddr + SDCC_DEBUG_FEATURE_CFG_REG);
}

/* Dummy func for Mask and Match show */
static ssize_t show_mask_and_match(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sdhci_host *host = dev_get_drvdata(dev);

	if (!host)
		return -EINVAL;

	pr_info("%s: M&M show func\n", mmc_hostname(host->mmc));

	return 0;
}

static ssize_t store_mask_and_match(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	unsigned long value;
	char *token;
	int i = 0;
	u32 mask = 0, match = 0, bit_shift = 0, testbus = 0;

	char *temp = (char *)buf;

	if (!host)
		return -EINVAL;

	while ((token = strsep(&temp, " "))) {
		kstrtoul(token, 0, &value);
		if (i == 0)
			mask = value;
		else if (i == 1)
			match = value;
		else if (i == 2)
			bit_shift = value;
		else if (i == 3) {
			testbus = value;
			break;
		}
		i++;
	}

	pr_info("%s: M&M parameter passed are: %d %d %d %d\n",
		mmc_hostname(host->mmc), mask, match, bit_shift, testbus);
	pm_runtime_get_sync(dev);
	sdhci_msm_mm_dbg_configure(host, mask, match, bit_shift, testbus);
	pm_runtime_put_sync(dev);

	pr_debug("%s: M&M debug enabled.\n", mmc_hostname(host->mmc));
	return count;
}

/* Enter sdcc debug mode */
void sdhci_msm_enter_dbg_mode(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct platform_device *pdev = msm_host->pdev;
	u32 enable_dbg_feature = 0;

	if (msm_host->minor < 2 || msm_host->debug_mode_enabled)
		return;
	if (!(host->quirks2 & SDHCI_QUIRK2_USE_DBG_FEATURE))
		return;

	/* Enable debug mode */
	writel_relaxed(ENABLE_DBG,
			host->ioaddr + SDCC_TESTBUS_CONFIG);
	writel_relaxed(DUMMY,
			host->ioaddr + SDCC_DEBUG_EN_DIS_REG);
	writel_relaxed((readl_relaxed(host->ioaddr +
			SDCC_TESTBUS_CONFIG) | TESTBUS_EN),
			host->ioaddr + SDCC_TESTBUS_CONFIG);

	if (msm_host->minor >= 2)
		enable_dbg_feature |= FSM_HISTORY |
			AUTO_RECOVERY_DISABLE |
			MM_TRIGGER_DISABLE |
			IIB_EN;

	/* Enable particular feature */
	writel_relaxed((readl_relaxed(host->ioaddr +
			SDCC_DEBUG_FEATURE_CFG_REG) | enable_dbg_feature),
			host->ioaddr + SDCC_DEBUG_FEATURE_CFG_REG);

	/* Read back to ensure write went through */
	readl_relaxed(host->ioaddr +
			SDCC_DEBUG_FEATURE_CFG_REG);
	msm_host->debug_mode_enabled = true;

	dev_info(&pdev->dev, "Debug feature enabled 0x%08x\n",
			readl_relaxed(host->ioaddr +
			SDCC_DEBUG_FEATURE_CFG_REG));
}

/* Exit sdcc debug mode */
void sdhci_msm_exit_dbg_mode(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct platform_device *pdev = msm_host->pdev;

	if (msm_host->minor  < 2 || !msm_host->debug_mode_enabled)
		return;
	if (!(host->quirks2 & SDHCI_QUIRK2_USE_DBG_FEATURE))
		return;

	/* Exit debug mode */
	writel_relaxed(DISABLE_DBG,
			host->ioaddr + SDCC_TESTBUS_CONFIG);
	writel_relaxed(DUMMY,
			host->ioaddr + SDCC_DEBUG_EN_DIS_REG);

	msm_host->debug_mode_enabled = false;

	dev_dbg(&pdev->dev, "Debug feature disabled 0x%08x\n",
			readl_relaxed(host->ioaddr +
			SDCC_DEBUG_FEATURE_CFG_REG));
}

int sdhci_msm_execute_tuning(struct sdhci_host *host, u32 opcode)
{
	unsigned long flags;
	int tuning_seq_cnt = 10;
	u8 phase, *data_buf, tuned_phases[NUM_TUNING_PHASES], tuned_phase_cnt;
	const u32 *tuning_block_pattern = tuning_block_64;
	int size = sizeof(tuning_block_64); /* Tuning pattern size in bytes */
	int rc;
	struct mmc_host *mmc = host->mmc;
	struct mmc_ios	ios = host->mmc->ios;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	u8 drv_type = 0;
	bool drv_type_changed = false;
	struct mmc_card *card = host->mmc->card;
	int sts_retry;
	u8 last_good_phase = 0;

	/*
	 * Tuning is required for SDR104, HS200 and HS400 cards and
	 * if clock frequency is greater than 100MHz in these modes.
	 */
	if (host->clock <= CORE_FREQ_100MHZ ||
		!((ios.timing == MMC_TIMING_MMC_HS400) ||
		(ios.timing == MMC_TIMING_MMC_HS200) ||
		(ios.timing == MMC_TIMING_UHS_SDR104)))
		return 0;

	/*
	 * Clear tuning_done flag before tuning to ensure proper
	 * HS400 settings.
	 */
	msm_host->tuning_done = 0;

	/*
	 * Don't allow re-tuning for CRC errors observed for any commands
	 * that are sent during tuning sequence itself.
	 */
	if (msm_host->tuning_in_progress)
		return 0;
	sdhci_msm_exit_dbg_mode(host);
	msm_host->tuning_in_progress = true;
	pr_debug("%s: Enter %s\n", mmc_hostname(mmc), __func__);

	/* CDC/SDC4 DLL HW calibration is only required for HS400 mode*/
	if (msm_host->tuning_done && !msm_host->calibration_done &&
		(mmc->ios.timing == MMC_TIMING_MMC_HS400)) {
		rc = sdhci_msm_hs400_dll_calibration(host);
		spin_lock_irqsave(&host->lock, flags);
		if (!rc)
			msm_host->calibration_done = true;
		spin_unlock_irqrestore(&host->lock, flags);
		goto out;
	}

	spin_lock_irqsave(&host->lock, flags);

	if ((opcode == MMC_SEND_TUNING_BLOCK_HS200) &&
		(mmc->ios.bus_width == MMC_BUS_WIDTH_8)) {
		tuning_block_pattern = tuning_block_128;
		size = sizeof(tuning_block_128);
	}
	spin_unlock_irqrestore(&host->lock, flags);

	data_buf = kmalloc(size, GFP_KERNEL);
	if (!data_buf) {
		rc = -ENOMEM;
		goto out;
	}

retry:
	tuned_phase_cnt = 0;

	/* first of all reset the tuning block */
	rc = msm_init_cm_dll(host, DLL_INIT_NORMAL);
	if (rc)
		goto kfree;

	phase = 0;
	do {
		struct mmc_command cmd = {0};
		struct mmc_data data = {0};
		struct mmc_request mrq = {
			.cmd = &cmd,
			.data = &data
		};
		struct scatterlist sg;
		struct mmc_command sts_cmd = {0};

		/* set the phase in delay line hw block */
		rc = msm_config_cm_dll_phase(host, phase);
		if (rc)
			goto kfree;

		cmd.opcode = opcode;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

		data.blksz = size;
		data.blocks = 1;
		data.flags = MMC_DATA_READ;
		data.timeout_ns = 1000 * 1000 * 1000; /* 1 sec */

		data.sg = &sg;
		data.sg_len = 1;
		sg_init_one(&sg, data_buf, size);
		memset(data_buf, 0, size);
		mmc_wait_for_req(mmc, &mrq);

		if (card && (cmd.error || data.error)) {
			/*
			 * Set the dll to last known good phase while sending
			 * status command to ensure that status command won't
			 * fail due to bad phase.
			 */
			if (tuned_phase_cnt)
				last_good_phase =
					tuned_phases[tuned_phase_cnt-1];
			else if (msm_host->saved_tuning_phase !=
					INVALID_TUNING_PHASE)
				last_good_phase = msm_host->saved_tuning_phase;

			rc = msm_config_cm_dll_phase(host, last_good_phase);
			if (rc)
				goto kfree;

			sts_cmd.opcode = MMC_SEND_STATUS;
			sts_cmd.arg = card->rca << 16;
			sts_cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
			sts_retry = 5;
			while (sts_retry) {
				mmc_wait_for_cmd(mmc, &sts_cmd, 0);

				if (sts_cmd.error ||
				   (R1_CURRENT_STATE(sts_cmd.resp[0])
				   != R1_STATE_TRAN)) {
					sts_retry--;
					/*
					 * wait for at least 146 MCLK cycles for
					 * the card to move to TRANS state. As
					 * the MCLK would be min 200MHz for
					 * tuning, we need max 0.73us delay. To
					 * be on safer side 1ms delay is given.
					 */
					usleep_range(1000, 1200);
					pr_debug("%s: phase %d sts cmd err %d resp 0x%x\n",
						mmc_hostname(mmc), phase,
						sts_cmd.error, sts_cmd.resp[0]);
					continue;
				}
				break;
			}
		}

		if (!cmd.error && !data.error &&
			!memcmp(data_buf, tuning_block_pattern, size)) {
			/* tuning is successful at this tuning point */
			tuned_phases[tuned_phase_cnt++] = phase;
			pr_debug("%s: %s: found *** good *** phase = %d\n",
				mmc_hostname(mmc), __func__, phase);
		} else {
			/* Ignore crc errors occurred during tuning */
			if (cmd.error)
				mmc->err_stats[MMC_ERR_CMD_CRC]--;
			else if (data.error)
				mmc->err_stats[MMC_ERR_DAT_CRC]--;
			pr_debug("%s: %s: found ## bad ## phase = %d\n",
				mmc_hostname(mmc), __func__, phase);
		}
	} while (++phase < 16);

	if ((tuned_phase_cnt == NUM_TUNING_PHASES) &&
			card && mmc_card_mmc(card)) {
		/*
		 * If all phases pass then its a problem. So change the card's
		 * drive type to a different value, if supported and repeat
		 * tuning until at least one phase fails. Then set the original
		 * drive type back.
		 *
		 * If all the phases still pass after trying all possible
		 * drive types, then one of those 16 phases will be picked.
		 * This is no different from what was going on before the
		 * modification to change drive type and retune.
		 */
		pr_debug("%s: tuned phases count: %d\n", mmc_hostname(mmc),
				tuned_phase_cnt);

		/* set drive type to other value . default setting is 0x0 */
		while (++drv_type <= MAX_DRV_TYPES_SUPPORTED_HS200) {
			pr_debug("%s: trying different drive strength (%d)\n",
				mmc_hostname(mmc), drv_type);
			if (card->ext_csd.raw_driver_strength &
					(1 << drv_type)) {
				sdhci_msm_set_mmc_drv_type(host, opcode,
						drv_type);
				if (!drv_type_changed)
					drv_type_changed = true;
				goto retry;
			}
		}
	}

	/* reset drive type to default (50 ohm) if changed */
	if (drv_type_changed)
		sdhci_msm_set_mmc_drv_type(host, opcode, 0);

	if (tuned_phase_cnt) {
		if (tuned_phase_cnt == ARRAY_SIZE(tuned_phases)) {
			/*
			 * All phases valid is _almost_ as bad as no phases
			 * valid.  Probably all phases are not really reliable
			 * but we didn't detect where the unreliable place is.
			 * That means we'll essentially be guessing and hoping
			 * we get a good phase.  Better to try a few times.
			 */
			dev_dbg(mmc_dev(mmc), "%s: All phases valid; try again\n",
				mmc_hostname(mmc));
			if (--tuning_seq_cnt) {
				tuned_phase_cnt = 0;
				goto retry;
			}
		}

		rc = msm_find_most_appropriate_phase(host, tuned_phases,
							tuned_phase_cnt);
		if (rc < 0)
			goto kfree;
		phase = (u8)rc;

		/*
		 * Finally set the selected phase in delay
		 * line hw block.
		 */
		rc = msm_config_cm_dll_phase(host, phase);
		if (rc)
			goto kfree;
		msm_host->saved_tuning_phase = phase;
		pr_debug("%s: %s: finally setting the tuning phase to %d\n",
				mmc_hostname(mmc), __func__, phase);
	} else {
		if (--tuning_seq_cnt)
			goto retry;
		/* tuning failed */
		pr_err("%s: %s: no tuning point found\n",
			mmc_hostname(mmc), __func__);
		rc = -EIO;
	}

kfree:
	kfree(data_buf);
out:
	sdhci_msm_enter_dbg_mode(host);
	spin_lock_irqsave(&host->lock, flags);
	if (!rc)
		msm_host->tuning_done = true;
	spin_unlock_irqrestore(&host->lock, flags);
	msm_host->tuning_in_progress = false;
	pr_debug("%s: Exit %s, err(%d)\n", mmc_hostname(mmc), __func__, rc);
	return rc;
}

static int sdhci_msm_setup_gpio(struct sdhci_msm_pltfm_data *pdata, bool enable)
{
	struct sdhci_msm_gpio_data *curr;
	int i, ret = 0;

	curr = pdata->pin_data->gpio_data;
	for (i = 0; i < curr->size; i++) {
		if (!gpio_is_valid(curr->gpio[i].no)) {
			ret = -EINVAL;
			pr_err("%s: Invalid gpio = %d\n", __func__,
					curr->gpio[i].no);
			goto free_gpios;
		}
		if (enable) {
			ret = gpio_request(curr->gpio[i].no,
						curr->gpio[i].name);
			if (ret) {
				pr_err("%s: gpio_request(%d, %s) failed %d\n",
					__func__, curr->gpio[i].no,
					curr->gpio[i].name, ret);
				goto free_gpios;
			}
			curr->gpio[i].is_enabled = true;
		} else {
			gpio_free(curr->gpio[i].no);
			curr->gpio[i].is_enabled = false;
		}
	}
	return ret;

free_gpios:
	for (i--; i >= 0; i--) {
		gpio_free(curr->gpio[i].no);
		curr->gpio[i].is_enabled = false;
	}
	return ret;
}

static int sdhci_msm_config_pinctrl_drv_type(struct sdhci_msm_pltfm_data *pdata,
		unsigned int clock)
{
	int ret = 0;

	if (clock > 150000000) {
		if (pdata->pctrl_data->pins_drv_type_200MHz)
			ret = pinctrl_select_state(pdata->pctrl_data->pctrl,
				pdata->pctrl_data->pins_drv_type_200MHz);
	} else if (clock > 75000000) {
		if (pdata->pctrl_data->pins_drv_type_100MHz)
			ret = pinctrl_select_state(pdata->pctrl_data->pctrl,
				pdata->pctrl_data->pins_drv_type_100MHz);
	} else if (clock > 400000) {
		if (pdata->pctrl_data->pins_drv_type_50MHz)
			ret = pinctrl_select_state(pdata->pctrl_data->pctrl,
				pdata->pctrl_data->pins_drv_type_50MHz);
	} else {
		if (pdata->pctrl_data->pins_drv_type_400KHz)
			ret = pinctrl_select_state(pdata->pctrl_data->pctrl,
				pdata->pctrl_data->pins_drv_type_400KHz);
	}

	return ret;
}

static int sdhci_msm_setup_pinctrl(struct sdhci_msm_pltfm_data *pdata,
		bool enable)
{
	int ret = 0;

	if (enable)
		ret = pinctrl_select_state(pdata->pctrl_data->pctrl,
			pdata->pctrl_data->pins_active);
	else
		ret = pinctrl_select_state(pdata->pctrl_data->pctrl,
			pdata->pctrl_data->pins_sleep);

	if (ret < 0)
		pr_err("%s state for pinctrl failed with %d\n",
			enable ? "Enabling" : "Disabling", ret);

	return ret;
}

static int sdhci_msm_setup_pins(struct sdhci_msm_pltfm_data *pdata, bool enable)
{
	int ret = 0;

	if  (pdata->pin_cfg_sts == enable) {
		return 0;
	} else if (pdata->pctrl_data) {
		ret = sdhci_msm_setup_pinctrl(pdata, enable);
		goto out;
	} else if (!pdata->pin_data) {
		return 0;
	}

	if (pdata->pin_data->is_gpio)
		ret = sdhci_msm_setup_gpio(pdata, enable);
out:
	if (!ret)
		pdata->pin_cfg_sts = enable;

	return ret;
}

static int sdhci_msm_dt_get_array(struct device *dev, const char *prop_name,
				 u32 **out, int *len, u32 size)
{
	int ret = 0;
	struct device_node *np = dev->of_node;
	size_t sz;
	u32 *arr = NULL;

	if (!of_get_property(np, prop_name, len)) {
		ret = -EINVAL;
		goto out;
	}
	sz = *len = *len / sizeof(*arr);
	if (sz <= 0 || (size > 0 && (sz > size))) {
		dev_err(dev, "%s invalid size\n", prop_name);
		ret = -EINVAL;
		goto out;
	}

	arr = devm_kzalloc(dev, sz * sizeof(*arr), GFP_KERNEL);
	if (!arr) {
		ret = -ENOMEM;
		goto out;
	}

	ret = of_property_read_u32_array(np, prop_name, arr, sz);
	if (ret < 0) {
		dev_err(dev, "%s failed reading array %d\n", prop_name, ret);
		goto out;
	}
	*out = arr;
out:
	if (ret)
		*len = 0;
	return ret;
}

#define MAX_PROP_SIZE 32
static int sdhci_msm_dt_parse_vreg_info(struct device *dev,
		struct sdhci_msm_reg_data **vreg_data, const char *vreg_name)
{
	int len, ret = 0;
	const __be32 *prop;
	char prop_name[MAX_PROP_SIZE];
	struct sdhci_msm_reg_data *vreg;
	struct device_node *np = dev->of_node;

	snprintf(prop_name, MAX_PROP_SIZE, "%s-supply", vreg_name);
	if (!of_parse_phandle(np, prop_name, 0)) {
		dev_info(dev, "No vreg data found for %s\n", vreg_name);
		return ret;
	}

	vreg = devm_kzalloc(dev, sizeof(*vreg), GFP_KERNEL);
	if (!vreg) {
		ret = -ENOMEM;
		return ret;
	}

	vreg->name = vreg_name;

	snprintf(prop_name, MAX_PROP_SIZE,
			"qcom,%s-always-on", vreg_name);
	if (of_get_property(np, prop_name, NULL))
		vreg->is_always_on = true;

	snprintf(prop_name, MAX_PROP_SIZE,
			"qcom,%s-lpm-sup", vreg_name);
	if (of_get_property(np, prop_name, NULL))
		vreg->lpm_sup = true;

	snprintf(prop_name, MAX_PROP_SIZE,
			"qcom,%s-voltage-level", vreg_name);
	prop = of_get_property(np, prop_name, &len);
	if (!prop || (len != (2 * sizeof(__be32)))) {
		dev_warn(dev, "%s %s property\n",
			prop ? "invalid format" : "no", prop_name);
	} else {
		vreg->low_vol_level = be32_to_cpup(&prop[0]);
		vreg->high_vol_level = be32_to_cpup(&prop[1]);
	}

	snprintf(prop_name, MAX_PROP_SIZE,
			"qcom,%s-current-level", vreg_name);
	prop = of_get_property(np, prop_name, &len);
	if (!prop || (len != (2 * sizeof(__be32)))) {
		dev_warn(dev, "%s %s property\n",
			prop ? "invalid format" : "no", prop_name);
	} else {
		vreg->lpm_uA = be32_to_cpup(&prop[0]);
		vreg->hpm_uA = be32_to_cpup(&prop[1]);
	}

	*vreg_data = vreg;
	dev_dbg(dev, "%s: %s %s vol=[%d %d]uV, curr=[%d %d]uA\n",
		vreg->name, vreg->is_always_on ? "always_on," : "",
		vreg->lpm_sup ? "lpm_sup," : "", vreg->low_vol_level,
		vreg->high_vol_level, vreg->lpm_uA, vreg->hpm_uA);

	return ret;
}

static int sdhci_msm_parse_pinctrl_info(struct device *dev,
		struct sdhci_msm_pltfm_data *pdata)
{
	struct sdhci_pinctrl_data *pctrl_data;
	struct pinctrl *pctrl;
	int ret = 0;

	/* Try to obtain pinctrl handle */
	pctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pctrl)) {
		ret = PTR_ERR(pctrl);
		goto out;
	}
	pctrl_data = devm_kzalloc(dev, sizeof(*pctrl_data), GFP_KERNEL);
	if (!pctrl_data) {
		ret = -ENOMEM;
		goto out;
	}
	pctrl_data->pctrl = pctrl;
	/* Look-up and keep the states handy to be used later */
	pctrl_data->pins_active = pinctrl_lookup_state(
			pctrl_data->pctrl, "active");
	if (IS_ERR(pctrl_data->pins_active)) {
		ret = PTR_ERR(pctrl_data->pins_active);
		dev_err(dev, "Could not get active pinstates, err:%d\n", ret);
		goto out;
	}
	pctrl_data->pins_sleep = pinctrl_lookup_state(
			pctrl_data->pctrl, "sleep");
	if (IS_ERR(pctrl_data->pins_sleep)) {
		ret = PTR_ERR(pctrl_data->pins_sleep);
		dev_err(dev, "Could not get sleep pinstates, err:%d\n", ret);
		goto out;
	}

	pctrl_data->pins_drv_type_400KHz = pinctrl_lookup_state(
			pctrl_data->pctrl, "ds_400KHz");
	if (IS_ERR(pctrl_data->pins_drv_type_400KHz)) {
		dev_dbg(dev, "Could not get 400K pinstates, err:%d\n", ret);
		pctrl_data->pins_drv_type_400KHz = NULL;
	}

	pctrl_data->pins_drv_type_50MHz = pinctrl_lookup_state(
			pctrl_data->pctrl, "ds_50MHz");
	if (IS_ERR(pctrl_data->pins_drv_type_50MHz)) {
		dev_dbg(dev, "Could not get 50M pinstates, err:%d\n", ret);
		pctrl_data->pins_drv_type_50MHz = NULL;
	}

	pctrl_data->pins_drv_type_100MHz = pinctrl_lookup_state(
			pctrl_data->pctrl, "ds_100MHz");
	if (IS_ERR(pctrl_data->pins_drv_type_100MHz)) {
		dev_dbg(dev, "Could not get 100M pinstates, err:%d\n", ret);
		pctrl_data->pins_drv_type_100MHz = NULL;
	}

	pctrl_data->pins_drv_type_200MHz = pinctrl_lookup_state(
			pctrl_data->pctrl, "ds_200MHz");
	if (IS_ERR(pctrl_data->pins_drv_type_200MHz)) {
		dev_dbg(dev, "Could not get 200M pinstates, err:%d\n", ret);
		pctrl_data->pins_drv_type_200MHz = NULL;
	}

	pdata->pctrl_data = pctrl_data;
out:
	return ret;
}

#define GPIO_NAME_MAX_LEN 32
static int sdhci_msm_dt_parse_gpio_info(struct device *dev,
		struct sdhci_msm_pltfm_data *pdata)
{
	int ret = 0, cnt, i;
	struct sdhci_msm_pin_data *pin_data;
	struct device_node *np = dev->of_node;

	ret = sdhci_msm_parse_pinctrl_info(dev, pdata);
	if (!ret) {
		goto out;
	} else if (ret == -EPROBE_DEFER) {
		dev_err(dev, "Pinctrl framework not registered, err:%d\n", ret);
		goto out;
	} else {
		dev_err(dev, "Parsing Pinctrl failed with %d, falling back on GPIO lib\n",
			ret);
		ret = 0;
	}
	pin_data = devm_kzalloc(dev, sizeof(*pin_data), GFP_KERNEL);
	if (!pin_data) {
		ret = -ENOMEM;
		goto out;
	}

	cnt = of_gpio_count(np);
	if (cnt > 0) {
		pin_data->gpio_data = devm_kzalloc(dev,
				sizeof(struct sdhci_msm_gpio_data), GFP_KERNEL);
		if (!pin_data->gpio_data) {
			ret = -ENOMEM;
			goto out;
		}
		pin_data->gpio_data->size = cnt;
		pin_data->gpio_data->gpio = devm_kzalloc(dev, cnt *
				sizeof(struct sdhci_msm_gpio), GFP_KERNEL);

		if (!pin_data->gpio_data->gpio) {
			ret = -ENOMEM;
			goto out;
		}

		for (i = 0; i < cnt; i++) {
			const char *name = NULL;
			char result[GPIO_NAME_MAX_LEN];

			pin_data->gpio_data->gpio[i].no = of_get_gpio(np, i);
			of_property_read_string_index(np,
					"qcom,gpio-names", i, &name);

			snprintf(result, GPIO_NAME_MAX_LEN, "%s-%s",
					dev_name(dev), name ? name : "?");
			pin_data->gpio_data->gpio[i].name = result;
			dev_dbg(dev, "%s: gpio[%s] = %d\n", __func__,
					pin_data->gpio_data->gpio[i].name,
					pin_data->gpio_data->gpio[i].no);
		}
	}
	pdata->pin_data = pin_data;
out:
	if (ret)
		dev_err(dev, "%s failed with err %d\n", __func__, ret);
	return ret;
}

#ifdef CONFIG_SMP
static inline void parse_affine_irq(struct sdhci_msm_pltfm_data *pdata)
{
	pdata->pm_qos_data.irq_req_type = PM_QOS_REQ_AFFINE_IRQ;
}
#else
static inline void parse_affine_irq(struct sdhci_msm_pltfm_data *pdata) { }
#endif

static int sdhci_msm_pm_qos_parse_irq(struct device *dev,
		struct sdhci_msm_pltfm_data *pdata)
{
	struct device_node *np = dev->of_node;
	const char *str;
	u32 cpu;
	int ret = 0;
	int i;

	pdata->pm_qos_data.irq_valid = false;
	pdata->pm_qos_data.irq_req_type = PM_QOS_REQ_AFFINE_CORES;
	if (!of_property_read_string(np, "qcom,pm-qos-irq-type", &str) &&
		!strcmp(str, "affine_irq")) {
		parse_affine_irq(pdata);
	}

	/* must specify cpu for "affine_cores" type */
	if (pdata->pm_qos_data.irq_req_type == PM_QOS_REQ_AFFINE_CORES) {
		pdata->pm_qos_data.irq_cpu = -1;
		ret = of_property_read_u32(np, "qcom,pm-qos-irq-cpu", &cpu);
		if (ret) {
			dev_err(dev, "%s: error %d reading irq cpu\n", __func__,
				ret);
			goto out;
		}
		if (cpu < 0 || cpu >= num_possible_cpus()) {
			dev_err(dev, "%s: invalid irq cpu %d (NR_CPUS=%d)\n",
				__func__, cpu, num_possible_cpus());
			ret = -EINVAL;
			goto out;
		}
		pdata->pm_qos_data.irq_cpu = cpu;
	}

	if (of_property_count_u32_elems(np, "qcom,pm-qos-irq-latency") !=
		SDHCI_POWER_POLICY_NUM) {
		dev_err(dev, "%s: could not read %d values for 'qcom,pm-qos-irq-latency'\n",
			__func__, SDHCI_POWER_POLICY_NUM);
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < SDHCI_POWER_POLICY_NUM; i++)
		of_property_read_u32_index(np, "qcom,pm-qos-irq-latency", i,
			&pdata->pm_qos_data.irq_latency.latency[i]);

	pdata->pm_qos_data.irq_valid = true;
out:
	return ret;
}

static int sdhci_msm_pm_qos_parse_cpu_groups(struct device *dev,
		struct sdhci_msm_pltfm_data *pdata)
{
	struct device_node *np = dev->of_node;
	u32 mask;
	int nr_groups;
	int ret;
	int i;

	/* Read cpu group mapping */
	nr_groups = of_property_count_u32_elems(np, "qcom,pm-qos-cpu-groups");
	if (nr_groups <= 0) {
		ret = -EINVAL;
		goto out;
	}
	pdata->pm_qos_data.cpu_group_map.nr_groups = nr_groups;
	pdata->pm_qos_data.cpu_group_map.mask =
		kcalloc(nr_groups, sizeof(cpumask_t), GFP_KERNEL);
	if (!pdata->pm_qos_data.cpu_group_map.mask) {
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < nr_groups; i++) {
		of_property_read_u32_index(np, "qcom,pm-qos-cpu-groups",
			i, &mask);

		pdata->pm_qos_data.cpu_group_map.mask[i].bits[0] = mask;
		if (!cpumask_subset(&pdata->pm_qos_data.cpu_group_map.mask[i],
			cpu_possible_mask)) {
			dev_err(dev, "%s: invalid mask 0x%x of cpu group #%d\n",
				__func__, mask, i);
			ret = -EINVAL;
			goto free_res;
		}
	}
	return 0;

free_res:
	kfree(pdata->pm_qos_data.cpu_group_map.mask);
out:
	return ret;
}

static int sdhci_msm_pm_qos_parse_latency(struct device *dev, const char *name,
		int nr_groups, struct sdhci_msm_pm_qos_latency **latency)
{
	struct device_node *np = dev->of_node;
	struct sdhci_msm_pm_qos_latency *values;
	int ret;
	int i;
	int group;
	int cfg;

	ret = of_property_count_u32_elems(np, name);
	if (ret > 0 && ret != SDHCI_POWER_POLICY_NUM * nr_groups) {
		dev_err(dev, "%s: invalid number of values for property %s: expected=%d actual=%d\n",
			__func__, name,	SDHCI_POWER_POLICY_NUM * nr_groups,
			ret);
		return -EINVAL;
	} else if (ret < 0) {
		return ret;
	}

	values = kcalloc(nr_groups, sizeof(struct sdhci_msm_pm_qos_latency),
			GFP_KERNEL);
	if (!values)
		return -ENOMEM;

	for (i = 0; i < SDHCI_POWER_POLICY_NUM * nr_groups; i++) {
		group = i / SDHCI_POWER_POLICY_NUM;
		cfg = i % SDHCI_POWER_POLICY_NUM;
		of_property_read_u32_index(np, name, i,
				&(values[group].latency[cfg]));
	}

	*latency = values;
	return 0;
}

static void sdhci_msm_pm_qos_parse(struct device *dev,
				struct sdhci_msm_pltfm_data *pdata)
{
	if (sdhci_msm_pm_qos_parse_irq(dev, pdata))
		dev_notice(dev, "%s: PM QoS voting for IRQ will be disabled\n",
			__func__);

	if (!sdhci_msm_pm_qos_parse_cpu_groups(dev, pdata)) {
		pdata->pm_qos_data.cmdq_valid =
			!sdhci_msm_pm_qos_parse_latency(dev,
				"qcom,pm-qos-cmdq-latency-us",
				pdata->pm_qos_data.cpu_group_map.nr_groups,
				&pdata->pm_qos_data.cmdq_latency);
		pdata->pm_qos_data.legacy_valid =
			!sdhci_msm_pm_qos_parse_latency(dev,
				"qcom,pm-qos-legacy-latency-us",
				pdata->pm_qos_data.cpu_group_map.nr_groups,
				&pdata->pm_qos_data.latency);
		if (!pdata->pm_qos_data.cmdq_valid &&
			!pdata->pm_qos_data.legacy_valid) {
			/* clean-up previously allocated arrays */
			kfree(pdata->pm_qos_data.latency);
			kfree(pdata->pm_qos_data.cmdq_latency);
			dev_err(dev, "%s: invalid PM QoS latency values. Voting for cpu group will be disabled\n",
				__func__);
		}
	} else {
		dev_notice(dev, "%s: PM QoS voting for cpu group will be disabled\n",
			__func__);
	}
}

static int sdhci_msm_dt_parse_hsr_info(struct device *dev,
		struct sdhci_msm_host *msm_host)

{
	u32 *dll_hsr_table = NULL;
	int dll_hsr_table_len, dll_hsr_reg_count;
	int ret = 0;

	if (sdhci_msm_dt_get_array(dev, "qcom,dll-hsr-list",
			&dll_hsr_table, &dll_hsr_table_len, 0))
		goto skip_hsr;

	dll_hsr_reg_count = sizeof(struct sdhci_msm_dll_hsr) / sizeof(u32);
	if (dll_hsr_table_len != dll_hsr_reg_count) {
		dev_err(dev, "Number of HSR entries are not matching\n");
		ret = -EINVAL;
	} else {
		msm_host->dll_hsr = (struct sdhci_msm_dll_hsr *)dll_hsr_table;
	}

skip_hsr:
	if (!msm_host->dll_hsr)
		dev_info(dev, "Failed to get dll hsr settings from dt\n");
	return ret;
}

static int sdhci_msm_parse_regulator_info(struct device *dev,
					struct sdhci_msm_pltfm_data *pdata)
{
	int ret = 0;

	pdata->vreg_data = devm_kzalloc(dev, sizeof(struct
						    sdhci_msm_slot_reg_data),
					GFP_KERNEL);
	if (!pdata->vreg_data) {
		dev_err(dev, "failed to allocate memory for vreg data\n");
		goto out;
	}

	if (sdhci_msm_dt_parse_vreg_info(dev, &pdata->vreg_data->vdd_data,
					 "vdd")) {
		dev_err(dev, "failed parsing vdd data\n");
		goto out;
	}
	if (sdhci_msm_dt_parse_vreg_info(dev,
					 &pdata->vreg_data->vdd_io_data,
					 "vdd-io")) {
		dev_err(dev, "failed parsing vdd-io data\n");
		goto out;
	}

	if (sdhci_msm_dt_parse_vreg_info(dev,
					 &pdata->vreg_data->vdd_io_bias_data,
					 "vdd-io-bias")) {
		dev_err(dev, "No vdd-io-bias regulator data\n");
	}

	return ret;
out:
	return -EINVAL;
}

int sdhci_msm_parse_reset_data(struct device *dev,
			struct sdhci_msm_host *msm_host)
{
	int ret = 0;

	msm_host->core_reset = devm_reset_control_get(dev,
					"core_reset");
	if (IS_ERR(msm_host->core_reset)) {
		ret = PTR_ERR(msm_host->core_reset);
		dev_err(dev, "core_reset unavailable,err = %d\n",
				ret);
		msm_host->core_reset = NULL;
	}

	return ret;
}

/* Parse platform data */
static
struct sdhci_msm_pltfm_data *sdhci_msm_populate_pdata(struct device *dev,
						struct sdhci_msm_host *msm_host)
{
	struct sdhci_msm_pltfm_data *pdata = NULL;
	struct device_node *np = dev->of_node;
	u32 bus_width = 0;
	int len, i;
	int clk_table_len;
	u32 *clk_table = NULL;
	int ice_clk_table_len;
	u32 *ice_clk_table = NULL;
	const char *lower_bus_speed = NULL;
	enum of_gpio_flags flags = OF_GPIO_ACTIVE_LOW;
	int bus_clk_table_len;
	u32 *bus_clk_table = NULL;
	int ret = 0;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		goto out;

	pdata->status_gpio = of_get_named_gpio_flags(np, "cd-gpios", 0, &flags);
	if (gpio_is_valid(pdata->status_gpio) && !(flags & OF_GPIO_ACTIVE_LOW))
		pdata->caps2 |= MMC_CAP2_CD_ACTIVE_HIGH;

	of_property_read_u32(np, "qcom,bus-width", &bus_width);
	if (bus_width == 8)
		pdata->mmc_bus_width = MMC_CAP_8_BIT_DATA;
	else if (bus_width == 4)
		pdata->mmc_bus_width = MMC_CAP_4_BIT_DATA;
	else {
		dev_notice(dev, "invalid bus-width, default to 1-bit mode\n");
		pdata->mmc_bus_width = 0;
	}

	if (sdhci_msm_dt_get_array(dev, "qcom,clk-rates",
			&clk_table, &clk_table_len, 0)) {
		dev_err(dev, "failed parsing supported clock rates\n");
		goto out;
	}
	if (!clk_table || !clk_table_len) {
		dev_err(dev, "Invalid clock table\n");
		goto out;
	}
	pdata->sup_clk_table = clk_table;
	pdata->sup_clk_cnt = clk_table_len;

	if (!sdhci_msm_dt_get_array(dev, "qcom,bus-aggr-clk-rates",
			&bus_clk_table, &bus_clk_table_len, 0)) {
		if (bus_clk_table && bus_clk_table_len) {
			pdata->bus_clk_table = bus_clk_table;
			pdata->bus_clk_cnt = bus_clk_table_len;
		}
	}

	if (!sdhci_msm_dt_get_array(dev, "qcom,ice-clk-rates",
			&ice_clk_table, &ice_clk_table_len, 0)) {
		if (ice_clk_table && ice_clk_table_len) {
			if (ice_clk_table_len != 2) {
				dev_err(dev, "Need max and min frequencies\n");
				goto out;
			}
			pdata->sup_ice_clk_table = ice_clk_table;
			pdata->sup_ice_clk_cnt = ice_clk_table_len;
			pdata->ice_clk_max = pdata->sup_ice_clk_table[0];
			pdata->ice_clk_min = pdata->sup_ice_clk_table[1];
			dev_dbg(dev, "ICE clock rates (Hz): max: %u min: %u\n",
				pdata->ice_clk_max, pdata->ice_clk_min);
		}
	}

	if (sdhci_msm_dt_get_array(dev, "qcom,devfreq,freq-table",
			&msm_host->mmc->clk_scaling.pltfm_freq_table,
			&msm_host->mmc->clk_scaling.pltfm_freq_table_sz, 0))
		pr_debug("%s: no clock scaling frequencies were supplied\n",
							dev_name(dev));
	else if (!msm_host->mmc->clk_scaling.pltfm_freq_table ||
			!msm_host->mmc->clk_scaling.pltfm_freq_table_sz)
		dev_err(dev, "bad dts clock scaling frequencies\n");

	/*
	 * Few hosts can support DDR52 mode at the same lower
	 * system voltage corner as high-speed mode. In such cases,
	 * it is always better to put it in DDR mode which will
	 * improve the performance without any power impact.
	 */
	if (!of_property_read_string(np, "qcom,scaling-lower-bus-speed-mode",
			&lower_bus_speed)) {
		if (!strcmp(lower_bus_speed, "DDR52"))
			msm_host->mmc->clk_scaling.lower_bus_speed_mode |=
					MMC_SCALING_LOWER_DDR52_MODE;
	}

	if (sdhci_msm_parse_regulator_info(dev, pdata))
		goto out;

	if (sdhci_msm_dt_parse_gpio_info(dev, pdata)) {
		dev_err(dev, "failed parsing gpio data\n");
		goto out;
	}

	len = of_property_count_strings(np, "qcom,bus-speed-mode");

	for (i = 0; i < len; i++) {
		const char *name = NULL;

		of_property_read_string_index(np,
			"qcom,bus-speed-mode", i, &name);
		if (!name)
			continue;

		if (!strcmp(name, "HS400_1p8v"))
			pdata->caps2 |= MMC_CAP2_HS400_1_8V;
		else if (!strcmp(name, "HS400_1p2v"))
			pdata->caps2 |= MMC_CAP2_HS400_1_2V;
		else if (!strcmp(name, "HS200_1p8v"))
			pdata->caps2 |= MMC_CAP2_HS200_1_8V_SDR;
		else if (!strcmp(name, "HS200_1p2v"))
			pdata->caps2 |= MMC_CAP2_HS200_1_2V_SDR;
		else if (!strcmp(name, "DDR_1p8v"))
			pdata->caps |= MMC_CAP_1_8V_DDR
						| MMC_CAP_UHS_DDR50;
		else if (!strcmp(name, "DDR_1p2v"))
			pdata->caps |= MMC_CAP_1_2V_DDR
						| MMC_CAP_UHS_DDR50;
	}

	if (of_get_property(np, "qcom,nonremovable", NULL))
		pdata->nonremovable = true;

	if (of_get_property(np, "qcom,nonhotplug", NULL))
		pdata->nonhotplug = true;

	pdata->largeaddressbus =
		of_property_read_bool(np, "qcom,large-address-bus");

	sdhci_msm_pm_qos_parse(dev, pdata);

	if (of_get_property(np, "qcom,core_3_0v_support", NULL))
		msm_host->core_3_0v_support = true;

	msm_host->regs_restore.is_supported =
		of_property_read_bool(np, "qcom,restore-after-cx-collapse");

	if (sdhci_msm_dt_parse_hsr_info(dev, msm_host))
		goto out;
	ret = sdhci_msm_parse_reset_data(dev, msm_host);
	if (ret)
		dev_err(dev, "Reset data parsing error\n");

	return pdata;
out:
	return NULL;
}

/* Returns required bandwidth in Bytes per Sec */
static unsigned int sdhci_get_bw_required(struct sdhci_host *host,
					struct mmc_ios *ios)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	unsigned int bw;

	bw = msm_host->clk_rate;
	/*
	 * For DDR mode, SDCC controller clock will be at
	 * the double rate than the actual clock that goes to card.
	 */
	if (ios->bus_width == MMC_BUS_WIDTH_4)
		bw /= 2;
	else if (ios->bus_width == MMC_BUS_WIDTH_1)
		bw /= 8;

	return bw;
}

static int sdhci_msm_bus_get_vote_for_bw(struct sdhci_msm_host *host,
					   unsigned int bw)
{
	unsigned int *table = host->pdata->voting_data->bw_vecs;
	unsigned int size = host->pdata->voting_data->bw_vecs_size;
	int i;

	if (host->msm_bus_vote.is_max_bw_needed && bw)
		return host->msm_bus_vote.max_bw_vote;

	for (i = 0; i < size; i++) {
		if (bw <= table[i])
			break;
	}

	if (i && (i == size))
		i--;

	return i;
}

/*
 * This function must be called with host lock acquired.
 * Caller of this function should also ensure that msm bus client
 * handle is not null.
 */
static inline int sdhci_msm_bus_set_vote(struct sdhci_msm_host *msm_host,
					     int vote,
					     unsigned long *flags)
{
	struct sdhci_host *host =  platform_get_drvdata(msm_host->pdev);
	int rc = 0;

	WARN_ON(!flags);

	if (vote != msm_host->msm_bus_vote.curr_vote) {
		spin_unlock_irqrestore(&host->lock, *flags);
		rc = msm_bus_scale_client_update_request(
				msm_host->msm_bus_vote.client_handle, vote);
		spin_lock_irqsave(&host->lock, *flags);
		if (rc) {
			pr_err("%s: msm_bus_scale_client_update_request() failed: bus_client_handle=0x%x, vote=%d, err=%d\n",
				mmc_hostname(host->mmc),
				msm_host->msm_bus_vote.client_handle, vote, rc);
			goto out;
		}
		msm_host->msm_bus_vote.curr_vote = vote;
	}
out:
	return rc;
}

/*****************************************************************************\
 *                                                                           *
 * MSM Command Queue Engine (CQE)                                            *
 *                                                                           *
\*****************************************************************************/

static u32 sdhci_msm_cqe_irq(struct sdhci_host *host, u32 intmask)
{
	int cmd_error = 0;
	int data_error = 0;
	u32 mask;

	if (!sdhci_cqe_irq(host, intmask, &cmd_error, &data_error))
		return intmask;

	cqhci_irq(host->mmc, intmask, cmd_error, data_error);

	/*
	 * Clear selected interrupts in err case.
	 * as earlier driver skipped
	 */
	if (data_error || cmd_error) {
		mask = intmask & host->cqe_ier;
		sdhci_writel(host, mask, SDHCI_INT_STATUS);
	}
	return 0;
}

void sdhci_msm_cqe_enable(struct mmc_host *mmc)
{
	struct sdhci_host *host = mmc_priv(mmc);

	if (host->flags & SDHCI_USE_64_BIT_DMA)
		host->desc_sz = 12;

	sdhci_cqe_enable(mmc);
	/* Set maximum timeout as per qti spec */
	sdhci_writeb(host, 0xF, SDHCI_TIMEOUT_CONTROL);
}

void sdhci_msm_cqe_disable(struct mmc_host *mmc, bool recovery)
{
	struct sdhci_host *host = mmc_priv(mmc);
	unsigned long flags;
	u32 ctrl;

	if (host->flags & SDHCI_USE_64_BIT_DMA)
		host->desc_sz = 16;

	spin_lock_irqsave(&host->lock, flags);

	ctrl = sdhci_readl(host, SDHCI_INT_ENABLE);
	ctrl |= SDHCI_INT_RESPONSE;
	sdhci_writel(host,  ctrl, SDHCI_INT_ENABLE);
	sdhci_writel(host, SDHCI_INT_RESPONSE, SDHCI_INT_STATUS);

	spin_unlock_irqrestore(&host->lock, flags);

	sdhci_cqe_disable(mmc, recovery);
}

void sdhci_msm_cqe_sdhci_dumpregs(struct mmc_host *mmc)
{
	struct sdhci_host *host = mmc_priv(mmc);

	sdhci_dumpregs(host);
}

static const struct cqhci_host_ops sdhci_msm_cqhci_ops = {
	.enable		= sdhci_msm_cqe_enable,
	.disable	= sdhci_msm_cqe_disable,
	.dumpregs		= sdhci_msm_cqe_sdhci_dumpregs,
};

#ifdef CONFIG_MMC_CQHCI
static int sdhci_msm_cqe_add_host(struct sdhci_host *host,
				struct platform_device *pdev)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct cqhci_host *cq_host;
	bool dma64;
	int ret;

	ret = sdhci_setup_host(host);
	if (ret)
		return ret;

	cq_host = cqhci_pltfm_init(pdev);
	if (IS_ERR(cq_host)) {
		ret = PTR_ERR(cq_host);
		dev_err(&pdev->dev, "cqhci-pltfm init: failed: %d\n", ret);
		goto cleanup;
	}

	msm_host->mmc->caps2 |= MMC_CAP2_CQE;
	cq_host->ops = &sdhci_msm_cqhci_ops;
	msm_host->cq_host = cq_host;

	dma64 = host->flags & SDHCI_USE_64_BIT_DMA;
	/*
	 * Set the vendor specific ops needed for ICE.
	 * Default implementation if the ops are not set.
	 */
#ifdef CONFIG_MMC_CQHCI_CRYPTO_QTI
	cqhci_crypto_qti_set_vops(cq_host);
#endif

	ret = cqhci_init(cq_host, host->mmc, dma64);
	if (ret) {
		dev_err(&pdev->dev, "%s: CQE init: failed (%d)\n",
					mmc_hostname(host->mmc),
					ret);
		goto cleanup;
	}

	ret = __sdhci_add_host(host);
	if (ret)
		goto cleanup;

	dev_info(&pdev->dev, "%s: CQE init: success\n",
					mmc_hostname(host->mmc));
	return ret;

cleanup:
	sdhci_cleanup_host(host);
	return ret;
}
#else
static void sdhci_msm_cqe_add_host(struct sdhci_host *host,
				struct platform_device *pdev)
{
	dev_warn(&pdev->dev, "CQE config not enabled, defaulting to sdhci\n");
	return sdhci_add_host(host);
}
#endif /* CONFIG_MMC_CQHCI */

/*
 * This function cancels any scheduled delayed work and sets the bus
 * vote based on bw (bandwidth) argument.
 */
static void sdhci_msm_bus_get_and_set_vote(struct sdhci_host *host,
						unsigned int bw)
{
	int vote;
	unsigned long flags;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	spin_lock_irqsave(&host->lock, flags);
	vote = sdhci_msm_bus_get_vote_for_bw(msm_host, bw);
	sdhci_msm_bus_set_vote(msm_host, vote, &flags);
	spin_unlock_irqrestore(&host->lock, flags);
}

static int sdhci_msm_bus_register(struct sdhci_msm_host *host,
				struct platform_device *pdev)
{
	int rc = 0;
	struct msm_bus_scale_pdata *bus_pdata;

	struct sdhci_msm_bus_voting_data *data;
	struct device *dev = &pdev->dev;

	data = devm_kzalloc(dev,
		sizeof(struct sdhci_msm_bus_voting_data), GFP_KERNEL);
	if (!data) {
		rc = -ENOMEM;
		goto out;
	}
	data->bus_pdata = msm_bus_cl_get_pdata(pdev);
	if (data->bus_pdata) {
		rc = sdhci_msm_dt_get_array(dev, "qcom,bus-bw-vectors-bps",
				&data->bw_vecs, &data->bw_vecs_size, 0);
		if (rc) {
			dev_err(&pdev->dev,
				"%s: Failed to get bus-bw-vectors-bps\n",
				__func__);
			goto out;
		}
		host->pdata->voting_data = data;
	}
	if (host->pdata->voting_data &&
		host->pdata->voting_data->bus_pdata &&
		host->pdata->voting_data->bw_vecs &&
		host->pdata->voting_data->bw_vecs_size) {

		bus_pdata = host->pdata->voting_data->bus_pdata;
		host->msm_bus_vote.client_handle =
				msm_bus_scale_register_client(bus_pdata);
		if (!host->msm_bus_vote.client_handle) {
			dev_err(&pdev->dev, "msm_bus_scale_register_client()\n");
			rc = -EFAULT;
			goto out;
		}
		/* cache the vote index for minimum and maximum bandwidth */
		host->msm_bus_vote.min_bw_vote =
				sdhci_msm_bus_get_vote_for_bw(host, 0);
		host->msm_bus_vote.max_bw_vote =
				sdhci_msm_bus_get_vote_for_bw(host, UINT_MAX);
	} else {
		devm_kfree(dev, data);
	}

out:
	return rc;
}

static void sdhci_msm_bus_unregister(struct sdhci_msm_host *host)
{
	if (host->msm_bus_vote.client_handle)
		msm_bus_scale_unregister_client(
			host->msm_bus_vote.client_handle);
}

static void sdhci_msm_bus_voting(struct sdhci_host *host, u32 enable)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct mmc_ios *ios = &host->mmc->ios;
	unsigned int bw;

	if (!msm_host->msm_bus_vote.client_handle)
		return;

	if (enable) {
		bw = sdhci_get_bw_required(host, ios);
		sdhci_msm_bus_get_and_set_vote(host, bw);
	} else {
		sdhci_msm_bus_get_and_set_vote(host, 0);
	}
}

/* Regulator utility functions */
static int sdhci_msm_vreg_init_reg(struct device *dev,
					struct sdhci_msm_reg_data *vreg)
{
	int ret = 0;

	/* check if regulator is already initialized? */
	if (vreg->reg)
		goto out;

	/* Get the regulator handle */
	vreg->reg = devm_regulator_get(dev, vreg->name);
	if (IS_ERR(vreg->reg)) {
		ret = PTR_ERR(vreg->reg);
		pr_err("%s: devm_regulator_get(%s) failed. ret=%d\n",
			__func__, vreg->name, ret);
		goto out;
	}

	if (regulator_count_voltages(vreg->reg) > 0) {
		vreg->set_voltage_sup = true;
		/* sanity check */
		if (!vreg->high_vol_level || !vreg->hpm_uA) {
			pr_err("%s: %s invalid constraints specified\n",
			       __func__, vreg->name);
			ret = -EINVAL;
		}
	}

out:
	return ret;
}

static void sdhci_msm_vreg_deinit_reg(struct sdhci_msm_reg_data *vreg)
{
	if (vreg->reg)
		devm_regulator_put(vreg->reg);
}

static int sdhci_msm_vreg_set_optimum_mode(struct sdhci_msm_reg_data
						  *vreg, int uA_load)
{
	int ret = 0;

	/*
	 * regulators that do not support regulator_set_voltage also
	 * do not support regulator_set_optimum_mode
	 */
	if (vreg->set_voltage_sup) {
		ret = regulator_set_load(vreg->reg, uA_load);
		if (ret < 0)
			pr_err("%s: regulator_set_load(reg=%s,uA_load=%d) failed. ret=%d\n",
			       __func__, vreg->name, uA_load, ret);
		else
			/*
			 * regulator_set_load() can return non zero
			 * value even for success case.
			 */
			ret = 0;
	}
	return ret;
}

static int sdhci_msm_vreg_set_voltage(struct sdhci_msm_reg_data *vreg,
					int min_uV, int max_uV)
{
	int ret = 0;

	if (vreg->set_voltage_sup) {
		ret = regulator_set_voltage(vreg->reg, min_uV, max_uV);
		if (ret) {
			pr_err("%s: regulator_set_voltage(%s)failed. min_uV=%d,max_uV=%d,ret=%d\n",
			       __func__, vreg->name, min_uV, max_uV, ret);
		}
	}

	return ret;
}

static int sdhci_msm_vreg_enable(struct sdhci_msm_reg_data *vreg)
{
	int ret = 0;

	/* Put regulator in HPM (high power mode) */
	ret = sdhci_msm_vreg_set_optimum_mode(vreg, vreg->hpm_uA);
	if (ret < 0)
		return ret;

	if (!vreg->is_enabled) {
		/* Set voltage level */
		ret = sdhci_msm_vreg_set_voltage(vreg, vreg->low_vol_level,
						vreg->high_vol_level);
		if (ret)
			return ret;
	}
	ret = regulator_enable(vreg->reg);
	if (ret) {
		pr_err("%s: regulator_enable(%s) failed. ret=%d\n",
				__func__, vreg->name, ret);
		return ret;
	}
	vreg->is_enabled = true;
	return ret;
}

static int sdhci_msm_vreg_disable(struct sdhci_msm_reg_data *vreg)
{
	int ret = 0;

	/* Never disable regulator marked as always_on */
	if (vreg->is_enabled && !vreg->is_always_on) {
		ret = regulator_disable(vreg->reg);
		if (ret) {
			pr_err("%s: regulator_disable(%s) failed. ret=%d\n",
				__func__, vreg->name, ret);
			goto out;
		}
		vreg->is_enabled = false;

		ret = sdhci_msm_vreg_set_optimum_mode(vreg, 0);
		if (ret < 0)
			goto out;

		/* Set min. voltage level to 0 */
		ret = sdhci_msm_vreg_set_voltage(vreg, 0, vreg->high_vol_level);
		if (ret)
			goto out;
	} else if (vreg->is_enabled && vreg->is_always_on) {
		if (vreg->lpm_sup) {
			/* Put always_on regulator in LPM (low power mode) */
			ret = sdhci_msm_vreg_set_optimum_mode(vreg,
							      vreg->lpm_uA);
			if (ret < 0)
				goto out;
		}
	}
out:
	return ret;
}

static int sdhci_msm_setup_vreg(struct sdhci_msm_pltfm_data *pdata,
			bool enable, bool is_init)
{
	int ret = 0, i;
	struct sdhci_msm_slot_reg_data *curr_slot;
	struct sdhci_msm_reg_data *vreg_table[3];

	curr_slot = pdata->vreg_data;
	if (!curr_slot) {
		pr_debug("%s: vreg info unavailable,assuming the slot is powered by always on domain\n",
			 __func__);
		goto out;
	}

	vreg_table[0] = curr_slot->vdd_data;
	vreg_table[1] = curr_slot->vdd_io_data;
	vreg_table[2] = curr_slot->vdd_io_bias_data;

	for (i = 0; i < ARRAY_SIZE(vreg_table); i++) {
		if (vreg_table[i]) {
			if (enable)
				ret = sdhci_msm_vreg_enable(vreg_table[i]);
			else
				ret = sdhci_msm_vreg_disable(vreg_table[i]);
			if (ret)
				goto out;
		}
	}
out:
	return ret;
}

/* This init function should be called only once for each SDHC slot */
static int sdhci_msm_vreg_init(struct device *dev,
				struct sdhci_msm_pltfm_data *pdata,
				bool is_init)
{
	int ret = 0;
	struct sdhci_msm_slot_reg_data *curr_slot;
	struct sdhci_msm_reg_data *curr_vdd_reg, *curr_vdd_io_reg,
				  *curr_vdd_io_bias_reg;

	curr_slot = pdata->vreg_data;
	if (!curr_slot)
		goto out;

	curr_vdd_reg = curr_slot->vdd_data;
	curr_vdd_io_reg = curr_slot->vdd_io_data;
	curr_vdd_io_bias_reg = curr_slot->vdd_io_bias_data;

	if (!is_init)
		/* Deregister all regulators from regulator framework */
		goto vdd_io_bias_reg_deinit;

	/*
	 * Get the regulator handle from voltage regulator framework
	 * and then try to set the voltage level for the regulator
	 */
	if (curr_vdd_reg) {
		ret = sdhci_msm_vreg_init_reg(dev, curr_vdd_reg);
		if (ret)
			goto out;
	}
	if (curr_vdd_io_reg) {
		ret = sdhci_msm_vreg_init_reg(dev, curr_vdd_io_reg);
		if (ret)
			goto vdd_reg_deinit;
	}
	if (curr_vdd_io_bias_reg) {
		ret = sdhci_msm_vreg_init_reg(dev, curr_vdd_io_bias_reg);
		if (ret)
			goto vdd_io_reg_deinit;
	}

	if (ret)
		dev_err(dev, "vreg reset failed (%d)\n", ret);
	goto out;

vdd_io_bias_reg_deinit:
	if (curr_vdd_io_bias_reg)
		sdhci_msm_vreg_deinit_reg(curr_vdd_io_bias_reg);
vdd_io_reg_deinit:
	if (curr_vdd_io_reg)
		sdhci_msm_vreg_deinit_reg(curr_vdd_io_reg);
vdd_reg_deinit:
	if (curr_vdd_reg)
		sdhci_msm_vreg_deinit_reg(curr_vdd_reg);
out:
	return ret;
}


static int sdhci_msm_set_vdd_io_vol(struct sdhci_msm_pltfm_data *pdata,
			enum vdd_io_level level,
			unsigned int voltage_level)
{
	int ret = 0;
	int set_level;
	struct sdhci_msm_reg_data *vdd_io_reg;

	if (!pdata->vreg_data)
		return ret;

	vdd_io_reg = pdata->vreg_data->vdd_io_data;
	if (vdd_io_reg && vdd_io_reg->is_enabled) {
		switch (level) {
		case VDD_IO_LOW:
			set_level = vdd_io_reg->low_vol_level;
			break;
		case VDD_IO_HIGH:
			set_level = vdd_io_reg->high_vol_level;
			break;
		case VDD_IO_SET_LEVEL:
			set_level = voltage_level;
			break;
		default:
			pr_err("%s: invalid argument level = %d\n",
					__func__, level);
			ret = -EINVAL;
			return ret;
		}
		ret = sdhci_msm_vreg_set_voltage(vdd_io_reg, set_level,
				set_level);
	}
	return ret;
}

/*
 * Acquire spin-lock host->lock before calling this function
 */
static void sdhci_msm_cfg_sdiowakeup_gpio_irq(struct sdhci_host *host,
					      bool enable)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	if (enable && !msm_host->is_sdiowakeup_enabled)
		enable_irq(msm_host->pdata->sdiowakeup_irq);
	else if (!enable && msm_host->is_sdiowakeup_enabled)
		disable_irq_nosync(msm_host->pdata->sdiowakeup_irq);
	else
		dev_warn(&msm_host->pdev->dev, "%s: wakeup to config: %d curr: %d\n",
			__func__, enable, msm_host->is_sdiowakeup_enabled);
	msm_host->is_sdiowakeup_enabled = enable;
}

static irqreturn_t sdhci_msm_testbus_trigger_irq(int irq, void *data)
{
	struct sdhci_host *host = (struct sdhci_host *)data;

	pr_info("%s: match happened against mask\n",
				mmc_hostname(host->mmc));

	return IRQ_HANDLED;
}

static irqreturn_t sdhci_msm_sdiowakeup_irq(int irq, void *data)
{
	struct sdhci_host *host = (struct sdhci_host *)data;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	unsigned long flags;

	pr_debug("%s: irq (%d) received\n", __func__, irq);

	spin_lock_irqsave(&host->lock, flags);
	sdhci_msm_cfg_sdiowakeup_gpio_irq(host, false);
	spin_unlock_irqrestore(&host->lock, flags);
	msm_host->sdio_pending_processing = true;

	return IRQ_HANDLED;
}

void sdhci_msm_dump_pwr_ctrl_regs(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;
	unsigned int irq_flags = 0;
	struct irq_desc *pwr_irq_desc = irq_to_desc(msm_host->pwr_irq);

	if (pwr_irq_desc)
		irq_flags = ACCESS_PRIVATE(pwr_irq_desc->irq_data.common,
				state_use_accessors);

	pr_err("%s: PWRCTL_STATUS: 0x%08x | PWRCTL_MASK: 0x%08x | PWRCTL_CTL: 0x%08x, pwr isr state=0x%x\n",
		mmc_hostname(host->mmc),
		sdhci_msm_readl_relaxed(host,
			msm_host_offset->CORE_PWRCTL_STATUS),
		sdhci_msm_readl_relaxed(host,
			msm_host_offset->CORE_PWRCTL_MASK),
		sdhci_msm_readl_relaxed(host,
			msm_host_offset->CORE_PWRCTL_CTL), irq_flags);

	mmc_log_string(host->mmc,
		"Sts: 0x%08x | Mask: 0x%08x | Ctrl: 0x%08x, pwr isr state=0x%x\n",
		sdhci_msm_readb_relaxed(host,
			msm_host_offset->CORE_PWRCTL_STATUS),
		sdhci_msm_readb_relaxed(host,
			msm_host_offset->CORE_PWRCTL_MASK),
		sdhci_msm_readb_relaxed(host,
			msm_host_offset->CORE_PWRCTL_CTL), irq_flags);
}

static int sdhci_msm_clear_pwrctl_status(struct sdhci_host *host, u8 value)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset = msm_host->offset;
	int ret = 0, retry = 10;

	/*
	 * There is a rare HW scenario where the first clear pulse could be
	 * lost when actual reset and clear/read of status register is
	 * happening at a time. Hence, retry for at least 10 times to make
	 * sure status register is cleared. Otherwise, this will result in
	 * a spurious power IRQ resulting in system instability.
	 */
	do {
		if (retry == 0) {
			pr_err("%s: Timedout clearing (0x%x) pwrctl status register\n",
				mmc_hostname(host->mmc), value);
			sdhci_msm_dump_pwr_ctrl_regs(host);
			WARN_ON(1);
			ret = -EBUSY;
			break;
		}

		/*
		 * Clear the PWRCTL_STATUS interrupt bits by writing to the
		 * corresponding bits in the PWRCTL_CLEAR register.
		 */
		sdhci_msm_writeb_relaxed(value, host,
				msm_host_offset->CORE_PWRCTL_CLEAR);
		/*
		 * SDHC has core_mem and hc_mem device memory and these memory
		 * addresses do not fall within 1KB region. Hence, any update to
		 * core_mem address space would require an mb() to ensure this
		 * gets completed before its next update to registers within
		 * hc_mem.
		 */
		mb();
		retry--;
		udelay(10);
	} while (value & sdhci_msm_readb_relaxed(host,
				msm_host_offset->CORE_PWRCTL_STATUS));

	return ret;
}

static irqreturn_t sdhci_msm_pwr_irq(int irq, void *data)
{
	struct sdhci_host *host = (struct sdhci_host *)data;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;
	u8 irq_status = 0;
	u8 irq_ack = 0;
	int ret = 0;
	int pwr_state = 0, io_level = 0;
	unsigned long flags;
	struct sdhci_msm_reg_data *vreg_io_bias =
			msm_host->pdata->vreg_data->vdd_io_bias_data;

	irq_status = sdhci_msm_readb_relaxed(host,
		msm_host_offset->CORE_PWRCTL_STATUS);

	pr_debug("%s: Received IRQ(%d), status=0x%x\n",
		mmc_hostname(msm_host->mmc), irq, irq_status);

	sdhci_msm_clear_pwrctl_status(host, irq_status);

	/* Handle BUS ON/OFF*/
	if (irq_status & CORE_PWRCTL_BUS_ON) {
		ret = sdhci_msm_setup_vreg(msm_host->pdata, true, false);
		if (!ret) {
			ret = sdhci_msm_setup_pins(msm_host->pdata, true);
			ret |= sdhci_msm_set_vdd_io_vol(msm_host->pdata,
					VDD_IO_HIGH, 0);
		}
		if (ret)
			irq_ack |= CORE_PWRCTL_BUS_FAIL;
		else
			irq_ack |= CORE_PWRCTL_BUS_SUCCESS;

		pwr_state = REQ_BUS_ON;
		io_level = REQ_IO_HIGH;
	}
	if (irq_status & CORE_PWRCTL_BUS_OFF) {
		if (msm_host->pltfm_init_done)
			ret = sdhci_msm_setup_vreg(msm_host->pdata,
					false, false);
		if (!ret) {
			ret = sdhci_msm_setup_pins(msm_host->pdata, false);
			ret |= sdhci_msm_set_vdd_io_vol(msm_host->pdata,
					VDD_IO_LOW, 0);
		}
		if (ret)
			irq_ack |= CORE_PWRCTL_BUS_FAIL;
		else
			irq_ack |= CORE_PWRCTL_BUS_SUCCESS;

		pwr_state = REQ_BUS_OFF;
		io_level = REQ_IO_LOW;
	}
	/* Handle IO LOW/HIGH */
	if (irq_status & CORE_PWRCTL_IO_LOW) {
		/* Switch voltage Low */
		ret = sdhci_msm_set_vdd_io_vol(msm_host->pdata, VDD_IO_LOW, 0);
		if (!ret && vreg_io_bias)
			ret = sdhci_msm_vreg_disable(vreg_io_bias);
		if (ret)
			irq_ack |= CORE_PWRCTL_IO_FAIL;
		else
			irq_ack |= CORE_PWRCTL_IO_SUCCESS;

		io_level = REQ_IO_LOW;
	}
	if (irq_status & CORE_PWRCTL_IO_HIGH) {
		/* Switch voltage High */
		ret = sdhci_msm_set_vdd_io_vol(msm_host->pdata, VDD_IO_HIGH, 0);
		if (ret)
			irq_ack |= CORE_PWRCTL_IO_FAIL;
		else
			irq_ack |= CORE_PWRCTL_IO_SUCCESS;

		io_level = REQ_IO_HIGH;
	}

	/* ACK status to the core */
	sdhci_msm_writeb_relaxed(irq_ack, host,
			msm_host_offset->CORE_PWRCTL_CTL);
	/*
	 * SDHC has core_mem and hc_mem device memory and these memory
	 * addresses do not fall within 1KB region. Hence, any update to
	 * core_mem address space would require an mb() to ensure this gets
	 * completed before its next update to registers within hc_mem.
	 */
	mb();
	if ((io_level & REQ_IO_HIGH) &&
			(msm_host->caps_0 & CORE_3_0V_SUPPORT) &&
			!msm_host->core_3_0v_support)
		writel_relaxed((readl_relaxed(host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC) &
				~CORE_IO_PAD_PWR_SWITCH), host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC);
	else if ((io_level & REQ_IO_LOW) ||
			(msm_host->caps_0 & CORE_1_8V_SUPPORT))
		writel_relaxed((readl_relaxed(host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC) |
				CORE_IO_PAD_PWR_SWITCH), host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC);
	/*
	 * SDHC has core_mem and hc_mem device memory and these memory
	 * addresses do not fall within 1KB region. Hence, any update to
	 * core_mem address space would require an mb() to ensure this gets
	 * completed before its next update to registers within hc_mem.
	 */
	mb();

	pr_debug("%s: Handled IRQ(%d), ret=%d, ack=0x%x\n",
		mmc_hostname(msm_host->mmc), irq, ret, irq_ack);
	spin_lock_irqsave(&host->lock, flags);
	if (pwr_state)
		msm_host->curr_pwr_state = pwr_state;
	if (io_level)
		msm_host->curr_io_level = io_level;
	complete(&msm_host->pwr_irq_completion);
	spin_unlock_irqrestore(&host->lock, flags);

	return IRQ_HANDLED;
}

static ssize_t
show_polling(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	int poll;
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	poll = !!(host->mmc->caps & MMC_CAP_NEEDS_POLL);
	spin_unlock_irqrestore(&host->lock, flags);

	return snprintf(buf, PAGE_SIZE, "%d\n", poll);
}

static ssize_t
store_polling(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	int value;
	unsigned long flags;

	if (!kstrtou32(buf, 0, &value)) {
		spin_lock_irqsave(&host->lock, flags);
		if (value) {
			host->mmc->caps |= MMC_CAP_NEEDS_POLL;
			mmc_detect_change(host->mmc, 0);
		} else {
			host->mmc->caps &= ~MMC_CAP_NEEDS_POLL;
		}
		spin_unlock_irqrestore(&host->lock, flags);
	}
	return count;
}

static ssize_t
show_sdhci_max_bus_bw(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	return snprintf(buf, PAGE_SIZE, "%u\n",
			msm_host->msm_bus_vote.is_max_bw_needed);
}

static ssize_t
store_sdhci_max_bus_bw(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	uint32_t value;
	unsigned long flags;

	if (!kstrtou32(buf, 0, &value)) {
		spin_lock_irqsave(&host->lock, flags);
		msm_host->msm_bus_vote.is_max_bw_needed = !!value;
		spin_unlock_irqrestore(&host->lock, flags);
	}
	return count;
}

static void sdhci_msm_check_power_status(struct sdhci_host *host, u32 req_type)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;
	unsigned long flags;
	bool done = false;
	u32 io_sig_sts = SWITCHABLE_SIGNALLING_VOL;

	spin_lock_irqsave(&host->lock, flags);
	pr_debug("%s: %s: request %d curr_pwr_state %x curr_io_level %x\n",
			mmc_hostname(host->mmc), __func__, req_type,
			msm_host->curr_pwr_state, msm_host->curr_io_level);
	if (!msm_host->mci_removed)
		io_sig_sts = sdhci_msm_readl_relaxed(host,
				msm_host_offset->CORE_GENERICS);

	/*
	 * The IRQ for request type IO High/Low will be generated when -
	 * 1. SWITCHABLE_SIGNALLING_VOL is enabled in HW.
	 * 2. If 1 is true and when there is a state change in 1.8V enable
	 * bit (bit 3) of SDHCI_HOST_CONTROL2 register. The reset state of
	 * that bit is 0 which indicates 3.3V IO voltage. So, when MMC core
	 * layer tries to set it to 3.3V before card detection happens, the
	 * IRQ doesn't get triggered as there is no state change in this bit.
	 * The driver already handles this case by changing the IO voltage
	 * level to high as part of controller power up sequence. Hence, check
	 * for host->pwr to handle a case where IO voltage high request is
	 * issued even before controller power up.
	 */
	if (req_type & (REQ_IO_HIGH | REQ_IO_LOW)) {
		if (!(io_sig_sts & SWITCHABLE_SIGNALLING_VOL) ||
				((req_type & REQ_IO_HIGH) && !host->pwr)) {
			pr_debug("%s: do not wait for power IRQ that never comes\n",
					mmc_hostname(host->mmc));
			spin_unlock_irqrestore(&host->lock, flags);
			return;
		}
	}

	if ((req_type & msm_host->curr_pwr_state) ||
			(req_type & msm_host->curr_io_level))
		done = true;
	spin_unlock_irqrestore(&host->lock, flags);

	/*
	 * This is needed here to handle a case where IRQ gets
	 * triggered even before this function is called so that
	 * x->done counter of completion gets reset. Otherwise,
	 * next call to wait_for_completion returns immediately
	 * without actually waiting for the IRQ to be handled.
	 */
	if (done)
		init_completion(&msm_host->pwr_irq_completion);
	else if (!wait_for_completion_timeout(&msm_host->pwr_irq_completion,
				msecs_to_jiffies(MSM_PWR_IRQ_TIMEOUT_MS))) {
		__WARN_printf("%s: request(%d) timed out waiting for pwr_irq\n",
					mmc_hostname(host->mmc), req_type);
		mmc_log_string(host->mmc,
			"request(%d) timed out waiting for pwr_irq\n",
			req_type);
		sdhci_msm_dump_pwr_ctrl_regs(host);
	}
	pr_debug("%s: %s: request %d done\n", mmc_hostname(host->mmc),
			__func__, req_type);
}

static void sdhci_msm_toggle_cdr(struct sdhci_host *host, bool enable)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;
	u32 config = readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG);

	if (enable) {
		config |= CORE_CDR_EN;
		config &= ~CORE_CDR_EXT_EN;
		writel_relaxed(config, host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG);
	} else {
		config &= ~CORE_CDR_EN;
		config |= CORE_CDR_EXT_EN;
		writel_relaxed(config, host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG);
	}
}

static unsigned int sdhci_msm_max_segs(void)
{
	return SDHCI_MSM_MAX_SEGMENTS;
}

static unsigned int sdhci_msm_get_min_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	return msm_host->pdata->sup_clk_table[0];
}

static unsigned int sdhci_msm_get_max_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int max_clk_index = msm_host->pdata->sup_clk_cnt;

	return msm_host->pdata->sup_clk_table[max_clk_index - 1];
}

static unsigned int sdhci_msm_get_sup_clk_rate(struct sdhci_host *host,
						u32 req_clk)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	unsigned int sel_clk = -1;
	unsigned char cnt;

	if (req_clk < sdhci_msm_get_min_clock(host)) {
		sel_clk = sdhci_msm_get_min_clock(host);
		return sel_clk;
	}

	for (cnt = 0; cnt < msm_host->pdata->sup_clk_cnt; cnt++) {
		if (msm_host->pdata->sup_clk_table[cnt] > req_clk) {
			break;
		} else if (msm_host->pdata->sup_clk_table[cnt] == req_clk) {
			sel_clk = msm_host->pdata->sup_clk_table[cnt];
			break;
		}
		sel_clk = msm_host->pdata->sup_clk_table[cnt];
	}
	return sel_clk;
}

static long sdhci_msm_get_bus_aggr_clk_rate(struct sdhci_host *host,
						u32 apps_clk)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	long sel_clk = -1;
	unsigned char cnt;

	if (msm_host->pdata->bus_clk_cnt != msm_host->pdata->sup_clk_cnt) {
		pr_err("%s: %s: mismatch between bus_clk_cnt(%u) and apps_clk_cnt(%u)\n",
				mmc_hostname(host->mmc), __func__,
				(unsigned int)msm_host->pdata->bus_clk_cnt,
				(unsigned int)msm_host->pdata->sup_clk_cnt);
		return msm_host->pdata->bus_clk_table[0];
	}
	if (apps_clk == sdhci_msm_get_min_clock(host)) {
		sel_clk = msm_host->pdata->bus_clk_table[0];
		return sel_clk;
	}

	for (cnt = 0; cnt < msm_host->pdata->bus_clk_cnt; cnt++) {
		if (msm_host->pdata->sup_clk_table[cnt] > apps_clk)
			break;
		sel_clk = msm_host->pdata->bus_clk_table[cnt];
	}
	return sel_clk;
}

static void sdhci_msm_registers_save(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;

	if (!msm_host->regs_restore.is_supported &&
			!msm_host->reg_store)
		return;

	msm_host->regs_restore.vendor_func = readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_VENDOR_SPEC);
	msm_host->regs_restore.vendor_pwrctl_mask =
		readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_PWRCTL_MASK);
	msm_host->regs_restore.vendor_func2 =
		readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_VENDOR_SPEC_FUNC2);
	msm_host->regs_restore.vendor_func3 =
		readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_VENDOR_SPEC3);
	msm_host->regs_restore.hc_2c_2e =
		sdhci_readl(host, SDHCI_CLOCK_CONTROL);
	msm_host->regs_restore.hc_3c_3e =
		sdhci_readl(host, SDHCI_AUTO_CMD_STATUS);
	msm_host->regs_restore.vendor_pwrctl_ctl =
		readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_PWRCTL_CTL);
	msm_host->regs_restore.hc_38_3a =
		sdhci_readl(host, SDHCI_SIGNAL_ENABLE);
	msm_host->regs_restore.hc_34_36 =
		sdhci_readl(host, SDHCI_INT_ENABLE);
	msm_host->regs_restore.hc_28_2a =
		sdhci_readl(host, SDHCI_HOST_CONTROL);
	msm_host->regs_restore.vendor_caps_0 =
		readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_VENDOR_SPEC_CAPABILITIES0);
	msm_host->regs_restore.hc_caps_1 =
		sdhci_readl(host, SDHCI_CAPABILITIES_1);
	msm_host->regs_restore.testbus_config = readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_TESTBUS_CONFIG);
	msm_host->regs_restore.dll_config = readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG);
	msm_host->regs_restore.dll_config2 = readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG_2);
	msm_host->regs_restore.dll_config = readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG);
	msm_host->regs_restore.dll_config2 = readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG_2);
	msm_host->regs_restore.dll_config3 = readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_DLL_CONFIG_3);
	msm_host->regs_restore.dll_usr_ctl = readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_DLL_USR_CTL);

	msm_host->regs_restore.is_valid = true;

	pr_debug("%s: %s: registers saved. PWRCTL_MASK = 0x%x\n",
		mmc_hostname(host->mmc), __func__,
		readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_PWRCTL_MASK));
}

static void sdhci_msm_registers_restore(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	u8 irq_status;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;
	struct mmc_ios ios = host->mmc->ios;

	if ((!msm_host->regs_restore.is_supported ||
		!msm_host->regs_restore.is_valid) &&
		!msm_host->reg_store)
		return;

	writel_relaxed(0, host->ioaddr + msm_host_offset->CORE_PWRCTL_MASK);
	writel_relaxed(msm_host->regs_restore.vendor_func, host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC);
	writel_relaxed(msm_host->regs_restore.vendor_func2,
			host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC_FUNC2);
	writel_relaxed(msm_host->regs_restore.vendor_func3,
			host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC3);
	sdhci_writel(host, msm_host->regs_restore.hc_2c_2e,
			SDHCI_CLOCK_CONTROL);
	sdhci_writel(host, msm_host->regs_restore.hc_3c_3e,
			SDHCI_AUTO_CMD_STATUS);
	sdhci_writel(host, msm_host->regs_restore.hc_38_3a,
			SDHCI_SIGNAL_ENABLE);
	sdhci_writel(host, msm_host->regs_restore.hc_34_36,
			SDHCI_INT_ENABLE);
	sdhci_writel(host, msm_host->regs_restore.hc_28_2a,
			SDHCI_HOST_CONTROL);
	writel_relaxed(msm_host->regs_restore.vendor_caps_0,
			host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC_CAPABILITIES0);
	sdhci_writel(host, msm_host->regs_restore.hc_caps_1,
			SDHCI_CAPABILITIES_1);
	writel_relaxed(msm_host->regs_restore.testbus_config, host->ioaddr +
			msm_host_offset->CORE_TESTBUS_CONFIG);
	msm_host->regs_restore.is_valid = false;

	/*
	 * Clear the PWRCTL_STATUS register.
	 * There is a rare HW scenario where the first clear pulse could be
	 * lost when actual reset and clear/read of status register is
	 * happening at a time. Hence, retry for at least 10 times to make
	 * sure status register is cleared. Otherwise, this will result in
	 * a spurious power IRQ resulting in system instability.
	 */
	irq_status = sdhci_msm_readb_relaxed(host,
				msm_host_offset->CORE_PWRCTL_STATUS);

	sdhci_msm_clear_pwrctl_status(host, irq_status);

	writel_relaxed(msm_host->regs_restore.vendor_pwrctl_ctl,
			host->ioaddr + msm_host_offset->CORE_PWRCTL_CTL);
	writel_relaxed(msm_host->regs_restore.vendor_pwrctl_mask,
			host->ioaddr + msm_host_offset->CORE_PWRCTL_MASK);

	if (((ios.timing == MMC_TIMING_MMC_HS400) ||
			(ios.timing == MMC_TIMING_MMC_HS200) ||
			(ios.timing == MMC_TIMING_UHS_SDR104))
			&& (ios.clock > CORE_FREQ_100MHZ)) {
		writel_relaxed(msm_host->regs_restore.dll_config2,
			host->ioaddr + msm_host_offset->CORE_DLL_CONFIG_2);
		writel_relaxed(msm_host->regs_restore.dll_config3,
			host->ioaddr + msm_host_offset->CORE_DLL_CONFIG_3);
		writel_relaxed(msm_host->regs_restore.dll_usr_ctl,
			host->ioaddr + msm_host_offset->CORE_DLL_USR_CTL);
		writel_relaxed(msm_host->regs_restore.dll_config &
			~(CORE_DLL_RST | CORE_DLL_PDN),
			host->ioaddr + msm_host_offset->CORE_DLL_CONFIG);

		msm_init_cm_dll(host, DLL_INIT_FROM_CX_COLLAPSE_EXIT);
		msm_config_cm_dll_phase(host, msm_host->saved_tuning_phase);
	}

	pr_debug("%s: %s: registers restored. PWRCTL_MASK = 0x%x\n",
		mmc_hostname(host->mmc), __func__,
		readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_PWRCTL_MASK));
}

static int sdhci_msm_enable_controller_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int rc = 0;

	if (atomic_read(&msm_host->controller_clock))
		return 0;

	sdhci_msm_bus_voting(host, 1);

	if (!IS_ERR(msm_host->pclk)) {
		rc = clk_prepare_enable(msm_host->pclk);
		if (rc) {
			pr_err("%s: %s: failed to enable the pclk with error %d\n",
			       mmc_hostname(host->mmc), __func__, rc);
			goto remove_vote;
		}
	}

	if (!IS_ERR(msm_host->bus_aggr_clk)) {
		rc = clk_prepare_enable(msm_host->bus_aggr_clk);
		if (rc) {
			pr_err("%s: %s: failed to enable the bus aggr clk with error %d\n",
			       mmc_hostname(host->mmc), __func__, rc);
			goto disable_pclk;
		}
	}

	rc = clk_prepare_enable(msm_host->clk);
	if (rc) {
		pr_err("%s: %s: failed to enable the host-clk with error %d\n",
		       mmc_hostname(host->mmc), __func__, rc);
		goto disable_bus_aggr_clk;
	}

	if (!IS_ERR(msm_host->ice_clk)) {
		rc = clk_prepare_enable(msm_host->ice_clk);
		if (rc) {
			pr_err("%s: %s: failed to enable the ice-clk with error %d\n",
				mmc_hostname(host->mmc), __func__, rc);
			goto disable_host_clk;
		}
	}
	atomic_set(&msm_host->controller_clock, 1);
	pr_debug("%s: %s: enabled controller clock\n",
			mmc_hostname(host->mmc), __func__);
	sdhci_msm_registers_restore(host);
	goto out;

disable_host_clk:
	if (!IS_ERR(msm_host->clk))
		clk_disable_unprepare(msm_host->clk);
disable_bus_aggr_clk:
	if (!IS_ERR(msm_host->bus_aggr_clk))
		clk_disable_unprepare(msm_host->bus_aggr_clk);
disable_pclk:
	if (!IS_ERR(msm_host->pclk))
		clk_disable_unprepare(msm_host->pclk);
remove_vote:
	sdhci_msm_bus_voting(host, 0);
out:
	return rc;
}

static void sdhci_msm_disable_controller_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	if (atomic_read(&msm_host->controller_clock)) {
		sdhci_msm_registers_save(host);
		if (!IS_ERR(msm_host->clk))
			clk_disable_unprepare(msm_host->clk);
		if (!IS_ERR(msm_host->bus_aggr_clk))
			clk_disable_unprepare(msm_host->bus_aggr_clk);
		if (!IS_ERR(msm_host->pclk))
			clk_disable_unprepare(msm_host->pclk);
		if (!IS_ERR(msm_host->ice_clk))
			clk_disable_unprepare(msm_host->ice_clk);
		sdhci_msm_bus_voting(host, 0);
		atomic_set(&msm_host->controller_clock, 0);
		pr_debug("%s: %s: disabled controller clock\n",
			mmc_hostname(host->mmc), __func__);
	}
}

static int sdhci_msm_prepare_clocks(struct sdhci_host *host, bool enable)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int rc = 0;

	if (enable && !atomic_read(&msm_host->clks_on)) {
		pr_debug("%s: request to enable clocks\n",
				mmc_hostname(host->mmc));

		/*
		 * The bus-width or the clock rate might have changed
		 * after controller clocks are enbaled, update bus vote
		 * in such case.
		 */
		if (atomic_read(&msm_host->controller_clock))
			sdhci_msm_bus_voting(host, 1);

		rc = sdhci_msm_enable_controller_clock(host);
		if (rc)
			goto remove_vote;

		if (!IS_ERR_OR_NULL(msm_host->bus_clk)) {
			rc = clk_prepare_enable(msm_host->bus_clk);
			if (rc) {
				pr_err("%s: %s: failed to enable the bus-clock with error %d\n",
					mmc_hostname(host->mmc), __func__, rc);
				goto disable_controller_clk;
			}
		}
		if (!IS_ERR(msm_host->ff_clk)) {
			rc = clk_prepare_enable(msm_host->ff_clk);
			if (rc) {
				pr_err("%s: %s: failed to enable the ff_clk with error %d\n",
					mmc_hostname(host->mmc), __func__, rc);
				goto disable_bus_clk;
			}
		}
		if (!IS_ERR(msm_host->sleep_clk)) {
			rc = clk_prepare_enable(msm_host->sleep_clk);
			if (rc) {
				pr_err("%s: %s: failed to enable the sleep_clk with error %d\n",
					mmc_hostname(host->mmc), __func__, rc);
				goto disable_ff_clk;
			}
		}
		/*
		 * SDHC has core_mem and hc_mem device memory and these memory
		 * addresses do not fall within 1KB region. Hence, any update to
		 * core_mem address space would require an mb() to ensure this
		 * gets completed before its next update to registers within
		 * hc_mem.
		 */
		mb();

	} else if (!enable && atomic_read(&msm_host->clks_on)) {
		sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);
		/*
		 * SDHC has core_mem and hc_mem device memory and these memory
		 * addresses do not fall within 1KB region. Hence, any update
		 * to core_mem address space would require an mb() to ensure
		 * this gets completed before its next update to registers
		 * within hc_mem.
		 */
		mb();
		/*
		 * During 1.8V signal switching the clock source must
		 * still be ON as it requires accessing SDHC
		 * registers (SDHCi host control2 register bit 3 must
		 * be written and polled after stopping the SDCLK).
		 */
		if (host->mmc->card_clock_off)
			return 0;
		pr_debug("%s: request to disable clocks\n",
				mmc_hostname(host->mmc));
		if (!IS_ERR_OR_NULL(msm_host->sleep_clk))
			clk_disable_unprepare(msm_host->sleep_clk);
		if (!IS_ERR_OR_NULL(msm_host->ff_clk))
			clk_disable_unprepare(msm_host->ff_clk);
		if (!IS_ERR_OR_NULL(msm_host->bus_clk))
			clk_disable_unprepare(msm_host->bus_clk);
		sdhci_msm_disable_controller_clock(host);
	}
	atomic_set(&msm_host->clks_on, enable);
	goto out;
disable_ff_clk:
	if (!IS_ERR_OR_NULL(msm_host->ff_clk))
		clk_disable_unprepare(msm_host->ff_clk);
disable_bus_clk:
	if (!IS_ERR_OR_NULL(msm_host->bus_clk))
		clk_disable_unprepare(msm_host->bus_clk);
disable_controller_clk:
	if (!IS_ERR_OR_NULL(msm_host->clk))
		clk_disable_unprepare(msm_host->clk);
	if (!IS_ERR_OR_NULL(msm_host->bus_aggr_clk))
		clk_disable_unprepare(msm_host->bus_aggr_clk);
	if (!IS_ERR(msm_host->ice_clk))
		clk_disable_unprepare(msm_host->ice_clk);
	if (!IS_ERR_OR_NULL(msm_host->pclk))
		clk_disable_unprepare(msm_host->pclk);
	atomic_set(&msm_host->controller_clock, 0);
remove_vote:
		sdhci_msm_bus_voting(host, 0);
out:
	return rc;
}

/*
 * After MCLK ugating, toggle the FIFO write clock to get
 * the FIFO pointers and flags to valid state.
 */
static void sdhci_msm_toggle_fifo_write_clk(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;
	struct mmc_card *card = host->mmc->card;

	if (msm_host->tuning_done ||
			(card && mmc_card_strobe(card) &&
			msm_host->enhanced_strobe)) {
		/*
		 * set HC_REG_DLL_CONFIG_3[1] to select MCLK as
		 * DLL input clock
		 */
		writel_relaxed(((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG_3))
			| RCLK_TOGGLE), host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG_3);
		/* ensure above write as toggling same bit quickly */
		wmb();
		udelay(2);
		/*
		 * clear HC_REG_DLL_CONFIG_3[1] to select RCLK as
		 * DLL input clock
		 */
		writel_relaxed(((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG_3))
			& ~RCLK_TOGGLE), host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG_3);
	}
}

static void sdhci_msm_set_clock(struct sdhci_host *host, unsigned int clock)
{
	int rc;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;
	struct mmc_card *card = host->mmc->card;
	struct mmc_ios	curr_ios = host->mmc->ios;
	u32 sup_clock, ddr_clock, dll_lock;
	long bus_clk_rate;
	bool curr_pwrsave;

	if (!clock) {
		/*
		 * disable pwrsave to ensure clock is not auto-gated until
		 * the rate is >400KHz (initialization complete).
		 */
		writel_relaxed(readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC) &
			~CORE_CLK_PWRSAVE, host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC);
		sdhci_msm_prepare_clocks(host, false);
		host->clock = clock;
		goto out;
	}

	rc = sdhci_msm_prepare_clocks(host, true);
	if (rc)
		goto out;

	curr_pwrsave = !!(readl_relaxed(host->ioaddr +
	msm_host_offset->CORE_VENDOR_SPEC) & CORE_CLK_PWRSAVE);
	if ((clock > 400000) && !curr_pwrsave)
		writel_relaxed(readl_relaxed(host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC)
				| CORE_CLK_PWRSAVE, host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC);
	/*
	 * Disable pwrsave for a newly added card if doesn't allow clock
	 * gating.
	 */
	else if (curr_pwrsave)
		writel_relaxed(readl_relaxed(host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC)
				& ~CORE_CLK_PWRSAVE, host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC);

	sup_clock = sdhci_msm_get_sup_clk_rate(host, clock);
	if ((curr_ios.timing == MMC_TIMING_UHS_DDR50) ||
		(curr_ios.timing == MMC_TIMING_MMC_DDR52) ||
		(curr_ios.timing == MMC_TIMING_MMC_HS400)) {
		/*
		 * The SDHC requires internal clock frequency to be double the
		 * actual clock that will be set for DDR mode. The controller
		 * uses the faster clock(100/400MHz) for some of its parts and
		 * send the actual required clock (50/200MHz) to the card.
		 */
		ddr_clock = clock * 2;
		sup_clock = sdhci_msm_get_sup_clk_rate(host,
				ddr_clock);
	}

	/*
	 * In general all timing modes are controlled via UHS mode select in
	 * Host Control2 register. eMMC specific HS200/HS400 doesn't have
	 * their respective modes defined here, hence we use these values.
	 *
	 * HS200 - SDR104 (Since they both are equivalent in functionality)
	 * HS400 - This involves multiple configurations
	 *		Initially SDR104 - when tuning is required as HS200
	 *		Then when switching to DDR @ 400MHz (HS400) we use
	 *		the vendor specific HC_SELECT_IN to control the mode.
	 *
	 * In addition to controlling the modes we also need to select the
	 * correct input clock for DLL depending on the mode.
	 *
	 * HS400 - divided clock (free running MCLK/2)
	 * All other modes - default (free running MCLK)
	 */
	if (curr_ios.timing == MMC_TIMING_MMC_HS400) {
		/* Select the divided clock (free running MCLK/2) */
		writel_relaxed(((readl_relaxed(host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC)
				& ~CORE_HC_MCLK_SEL_MASK)
				| CORE_HC_MCLK_SEL_HS400), host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC);
		/*
		 * Select HS400 mode using the HC_SELECT_IN from VENDOR SPEC
		 * register
		 */
		if ((msm_host->tuning_done ||
				(card && mmc_card_strobe(card) &&
				 msm_host->enhanced_strobe)) &&
				!msm_host->calibration_done) {
			/*
			 * Write 0x6 to HC_SELECT_IN and 1 to HC_SELECT_IN_EN
			 * field in VENDOR_SPEC_FUNC
			 */
			writel_relaxed((readl_relaxed(host->ioaddr +
					msm_host_offset->CORE_VENDOR_SPEC)
					| CORE_HC_SELECT_IN_HS400
					| CORE_HC_SELECT_IN_EN), host->ioaddr +
					msm_host_offset->CORE_VENDOR_SPEC);
		}

		sdhci_msm_toggle_fifo_write_clk(host);

		if (!host->mmc->ios.old_rate && !msm_host->use_cdclp533) {
			/*
			 * Poll on DLL_LOCK and DDR_DLL_LOCK bits in
			 * CORE_DLL_STATUS to be set.  This should get set
			 * with in 15 us at 200 MHz.
			 * No need to check for DLL lock for HS400es mode
			 */
			if (card && mmc_card_strobe(card) &&
						msm_host->enhanced_strobe) {
				rc = readl_poll_timeout(host->ioaddr +
					msm_host_offset->CORE_DLL_STATUS,
					dll_lock, (dll_lock &
					CORE_DDR_DLL_LOCK), 10, 1000);
			} else {
				rc = readl_poll_timeout(host->ioaddr +
					msm_host_offset->CORE_DLL_STATUS,
					dll_lock, (dll_lock & (CORE_DLL_LOCK |
					CORE_DDR_DLL_LOCK)), 10, 1000);
			}
			if (rc == -ETIMEDOUT)
				pr_err("%s: Unable to get DLL_LOCK/DDR_DLL_LOCK, dll_status: 0x%08x\n",
						mmc_hostname(host->mmc),
						dll_lock);
		}
	} else {
		if (!msm_host->use_cdclp533)
			/* set CORE_PWRSAVE_DLL bit in CORE_VENDOR_SPEC3 */
			writel_relaxed((readl_relaxed(host->ioaddr +
					msm_host_offset->CORE_VENDOR_SPEC3)
					& ~CORE_PWRSAVE_DLL), host->ioaddr +
					msm_host_offset->CORE_VENDOR_SPEC3);

		/* Select the default clock (free running MCLK) */
		writel_relaxed(((readl_relaxed(host->ioaddr +
					msm_host_offset->CORE_VENDOR_SPEC)
					& ~CORE_HC_MCLK_SEL_MASK)
					| CORE_HC_MCLK_SEL_DFLT), host->ioaddr +
					msm_host_offset->CORE_VENDOR_SPEC);

		/*
		 * Disable HC_SELECT_IN to be able to use the UHS mode select
		 * configuration from Host Control2 register for all other
		 * modes.
		 *
		 * Write 0 to HC_SELECT_IN and HC_SELECT_IN_EN field
		 * in VENDOR_SPEC_FUNC
		 */
		writel_relaxed((readl_relaxed(host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC)
				& ~CORE_HC_SELECT_IN_EN
				& ~CORE_HC_SELECT_IN_MASK), host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC);
	}
	/*
	 * SDHC has core_mem and hc_mem device memory and these memory
	 * addresses do not fall within 1KB region. Hence, any update to
	 * core_mem address space would require an mb() to ensure this gets
	 * completed before its next update to registers within hc_mem.
	 */
	mb();

	if (sup_clock != msm_host->clk_rate) {
		pr_debug("%s: %s: setting clk rate to %u\n",
				mmc_hostname(host->mmc), __func__, sup_clock);
		rc = clk_set_rate(msm_host->clk, sup_clock);
		if (rc) {
			pr_err("%s: %s: Failed to set rate %u for host-clk : %d\n",
					mmc_hostname(host->mmc), __func__,
					sup_clock, rc);
			goto out;
		}
		msm_host->clk_rate = sup_clock;
		host->clock = clock;

		if (!IS_ERR(msm_host->bus_aggr_clk) &&
				msm_host->pdata->bus_clk_cnt) {
			bus_clk_rate = sdhci_msm_get_bus_aggr_clk_rate(host,
					sup_clock);
			if (bus_clk_rate >= 0) {
				rc = clk_set_rate(msm_host->bus_aggr_clk,
						bus_clk_rate);
				if (rc) {
					pr_err("%s: %s: Failed to set rate %ld for bus-aggr-clk : %d\n",
						mmc_hostname(host->mmc),
						__func__, bus_clk_rate, rc);
					goto out;
				}
			} else {
				pr_err("%s: %s: Unsupported apps clk rate %u for bus-aggr-clk, err: %ld\n",
					mmc_hostname(host->mmc), __func__,
					sup_clock, bus_clk_rate);
			}
		}

		/* Configure pinctrl drive type according to
		 * current clock rate
		 */
		rc = sdhci_msm_config_pinctrl_drv_type(msm_host->pdata, clock);
		if (rc)
			pr_err("%s: %s: Failed to set pinctrl drive type for clock rate %u (%d)\n",
					mmc_hostname(host->mmc), __func__,
					clock, rc);

		/*
		 * Update the bus vote in case of frequency change due to
		 * clock scaling.
		 */
		sdhci_msm_bus_voting(host, 1);
	}
out:
	sdhci_set_clock(host, clock);
}

static void sdhci_msm_set_uhs_signaling(struct sdhci_host *host,
					unsigned int uhs)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;
	u16 ctrl_2;

	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	/* Select Bus Speed Mode for host */
	ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;
	if ((uhs == MMC_TIMING_MMC_HS400) ||
		(uhs == MMC_TIMING_MMC_HS200) ||
		(uhs == MMC_TIMING_UHS_SDR104))
		ctrl_2 |= SDHCI_CTRL_UHS_SDR104;
	else if (uhs == MMC_TIMING_UHS_SDR12)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR12;
	else if (uhs == MMC_TIMING_UHS_SDR25)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR25;
	else if (uhs == MMC_TIMING_UHS_SDR50)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR50;
	else if ((uhs == MMC_TIMING_UHS_DDR50) ||
		 (uhs == MMC_TIMING_MMC_DDR52))
		ctrl_2 |= SDHCI_CTRL_UHS_DDR50;
	/*
	 * When clock frquency is less than 100MHz, the feedback clock must be
	 * provided and DLL must not be used so that tuning can be skipped. To
	 * provide feedback clock, the mode selection can be any value less
	 * than 3'b011 in bits [2:0] of HOST CONTROL2 register.
	 */
	if (host->clock <= CORE_FREQ_100MHZ) {
		if ((uhs == MMC_TIMING_MMC_HS400) ||
		    (uhs == MMC_TIMING_MMC_HS200) ||
		    (uhs == MMC_TIMING_UHS_SDR104))
			ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;

		/*
		 * Make sure DLL is disabled when not required
		 *
		 * Write 1 to DLL_RST bit of DLL_CONFIG register
		 */
		writel_relaxed((readl_relaxed(host->ioaddr +
				msm_host_offset->CORE_DLL_CONFIG)
				| CORE_DLL_RST), host->ioaddr +
				msm_host_offset->CORE_DLL_CONFIG);

		/* Write 1 to DLL_PDN bit of DLL_CONFIG register */
		writel_relaxed((readl_relaxed(host->ioaddr +
				msm_host_offset->CORE_DLL_CONFIG)
				| CORE_DLL_PDN), host->ioaddr +
				msm_host_offset->CORE_DLL_CONFIG);
		/*
		 * SDHC has core_mem and hc_mem device memory and these memory
		 * addresses do not fall within 1KB region. Hence, any update to
		 * core_mem address space would require an mb() to ensure this
		 * gets completed before its next update to registers within
		 * hc_mem.
		 */
		mb();

		/*
		 * The DLL needs to be restored and CDCLP533 recalibrated
		 * when the clock frequency is set back to 400MHz.
		 */
		msm_host->calibration_done = false;
	}

	pr_debug("%s: %s-clock:%u uhs mode:%u ctrl_2:0x%x\n",
		mmc_hostname(host->mmc), __func__, host->clock, uhs, ctrl_2);
	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);

}

#define MAX_TEST_BUS 60

static void sdhci_msm_cache_debug_data(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct sdhci_msm_debug_data *cached_data = &msm_host->cached_data;

	memcpy(&cached_data->copy_mmc, msm_host->mmc,
		sizeof(struct mmc_host));
	if (msm_host->mmc->card)
		memcpy(&cached_data->copy_card, msm_host->mmc->card,
			sizeof(struct mmc_card));
	memcpy(&cached_data->copy_host, host,
		sizeof(struct sdhci_host));
}

#define MAX_TEST_BUS 60
#define DRV_NAME "cqhci-host"
static void sdhci_msm_cqe_dump_debug_ram(struct sdhci_host *host)
{
	int i = 0;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;
	struct cqhci_host *cq_host;
	u32 version;
	u16 minor;
	int offset;

	if (msm_host->cq_host)
		cq_host = msm_host->cq_host;
	else
		return;

	version = sdhci_msm_readl_relaxed(host,
		msm_host_offset->CORE_MCI_VERSION);
	minor = version & CORE_VERSION_TARGET_MASK;
	/* registers offset changed starting from 4.2.0 */
	offset = minor >= SDHCI_MSM_VER_420 ? 0 : 0x48;

	if (cq_host->offset_changed)
		offset += CQE_V5_VENDOR_CFG;
	pr_err("---- Debug RAM dump ----\n");
	pr_err(DRV_NAME ": Debug RAM wrap-around: 0x%08x | Debug RAM overlap: 0x%08x\n",
	       cqhci_readl(cq_host, CQ_CMD_DBG_RAM_WA + offset),
	       cqhci_readl(cq_host, CQ_CMD_DBG_RAM_OL + offset));

	while (i < 16) {
		pr_err(DRV_NAME ": Debug RAM dump [%d]: 0x%08x\n", i,
		       cqhci_readl(cq_host, CQ_CMD_DBG_RAM + offset + (4 * i)));
		i++;
	}
	pr_err("-------------------------\n");
}

#define DUMP_FSM readl_relaxed(host->ioaddr + SDCC_DEBUG_FSM_TRACE_RD_REG)
#define MAX_FSM 16

void sdhci_msm_dump_fsm_history(struct sdhci_host *host)
{
	u32 sel_fsm;

	pr_err("----------- FSM REGISTER DUMP -----------\n");
	/* select fsm to dump */
	for (sel_fsm = 0; sel_fsm <= MAX_FSM; sel_fsm++) {
		writel_relaxed(sel_fsm, host->ioaddr +
				SDCC_DEBUG_FSM_TRACE_CFG_REG);
		pr_err(": selected fsm is 0x%08x\n",
				readl_relaxed(host->ioaddr +
				SDCC_DEBUG_FSM_TRACE_CFG_REG));
		/* dump selected fsm history */
		pr_err("0x%08x 0x%08x 0x%08x 0x%08x\n",
				readl_relaxed(host->ioaddr +
					SDCC_DEBUG_FSM_TRACE_RD_REG),
				readl_relaxed(host->ioaddr +
					SDCC_DEBUG_FSM_TRACE_RD_REG),
				readl_relaxed(host->ioaddr +
					SDCC_DEBUG_FSM_TRACE_RD_REG),
				readl_relaxed(host->ioaddr +
					SDCC_DEBUG_FSM_TRACE_RD_REG));
		pr_err("0x%08x 0x%08x 0x%08x\n",
				readl_relaxed(host->ioaddr +
					SDCC_DEBUG_FSM_TRACE_RD_REG),
				readl_relaxed(host->ioaddr +
					SDCC_DEBUG_FSM_TRACE_RD_REG),
				readl_relaxed(host->ioaddr +
					SDCC_DEBUG_FSM_TRACE_RD_REG));
	}
	/* Flush all fsm history */
	writel_relaxed(DUMMY, host->ioaddr +
			SDCC_DEBUG_FSM_TRACE_FIFO_FLUSH_REG);

	/* Exit from Auto recovery disable to move FSMs to idle state */
	writel_relaxed(DUMMY, host->ioaddr +
			SDCC_DEBUG_ERROR_STATE_EXIT_REG);

}

void sdhci_msm_dump_desc_history(struct sdhci_host *host)
{
	pr_err("----------- DESC HISTORY DUMP -----------\n");
	pr_err("Current Desc Addr: 0x%08x | Info: 0x%08x\n",
			readl_relaxed(host->ioaddr + SDCC_CURR_DESC_ADDR),
			readl_relaxed(host->ioaddr + SDCC_CURR_DESC_INFO));
	pr_err("Processed Desc1 Addr: 0x%08x | Info: 0x%08x\n",
			readl_relaxed(host->ioaddr + SDCC_PROC_DESC0_ADDR),
			readl_relaxed(host->ioaddr + SDCC_PROC_DESC0_INFO));
	pr_err("Processed Desc2 Addr: 0x%08x | Info: 0x%08x\n",
			readl_relaxed(host->ioaddr + SDCC_PROC_DESC1_ADDR),
			readl_relaxed(host->ioaddr + SDCC_PROC_DESC1_INFO));
}

void sdhci_msm_dump_iib(struct sdhci_host *host)
{
	u32 iter;

	pr_err("----------- IIB HISTORY DUMP -----------\n");
	for (iter = 0; iter < 8; iter++)
		pr_err("0x%08x\n", readl_relaxed(host->ioaddr +
			SDCC_DEBUG_IIB_REG + (iter * 4)));
}

void sdhci_msm_dump_vendor_regs(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;
	int tbsel, tbsel2;
	int i, index = 0;
	u32 test_bus_val = 0;
	u32 debug_reg[MAX_TEST_BUS] = {0};

	sdhci_msm_cache_debug_data(host);
	pr_info("----------- VENDOR REGISTER DUMP -----------\n");
	if (msm_host->cq_host)
		sdhci_msm_cqe_dump_debug_ram(host);

	mmc_log_string(host->mmc, "Data cnt: 0x%08x | Fifo cnt: 0x%08x\n",
		sdhci_msm_readl_relaxed(host,
			msm_host_offset->CORE_MCI_DATA_CNT),
		sdhci_msm_readl_relaxed(host,
			msm_host_offset->CORE_MCI_FIFO_CNT));
	pr_info("Data cnt: 0x%08x | Fifo cnt: 0x%08x | Int sts: 0x%08x\n",
		sdhci_msm_readl_relaxed(host,
			msm_host_offset->CORE_MCI_DATA_CNT),
		sdhci_msm_readl_relaxed(host,
			msm_host_offset->CORE_MCI_FIFO_CNT),
		sdhci_msm_readl_relaxed(host,
			msm_host_offset->CORE_MCI_STATUS));
	pr_info("DLL sts: 0x%08x | DLL cfg:  0x%08x | DLL cfg2: 0x%08x\n",
		readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DLL_STATUS),
		readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG),
		sdhci_msm_readl_relaxed(host,
			msm_host_offset->CORE_DLL_CONFIG_2));
	pr_info("DLL cfg3: 0x%08x | DLL usr ctl:  0x%08x | DDR cfg: 0x%08x\n",
		readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DLL_CONFIG_3),
		readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_DLL_USR_CTL),
		sdhci_msm_readl_relaxed(host,
			msm_host_offset->CORE_DDR_CONFIG));
	pr_info("SDCC ver: 0x%08x | Vndr adma err : addr0: 0x%08x addr1: 0x%08x\n",
		readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_MCI_VERSION),
		readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC_ADMA_ERR_ADDR0),
		readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC_ADMA_ERR_ADDR1));
	pr_info("Vndr func: 0x%08x | Vndr func2 : 0x%08x Vndr func3: 0x%08x\n",
		readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC),
		readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC_FUNC2),
		readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC3));

	if (msm_host->debug_mode_enabled) {
		sdhci_msm_dump_fsm_history(host);
		sdhci_msm_dump_desc_history(host);
	}
	/* Debug feature enable not must for iib */
	sdhci_msm_dump_iib(host);

	/*
	 * tbsel indicates [2:0] bits and tbsel2 indicates [7:4] bits
	 * of CORE_TESTBUS_CONFIG register.
	 *
	 * To select test bus 0 to 7 use tbsel and to select any test bus
	 * above 7 use (tbsel2 | tbsel) to get the test bus number. For eg,
	 * to select test bus 14, write 0x1E to CORE_TESTBUS_CONFIG register
	 * i.e., tbsel2[7:4] = 0001, tbsel[2:0] = 110.
	 */
	for (tbsel2 = 0; tbsel2 < 7; tbsel2++) {
		for (tbsel = 0; tbsel < 8; tbsel++) {
			if (index >= MAX_TEST_BUS)
				break;
			test_bus_val =
			(tbsel2 << msm_host_offset->CORE_TESTBUS_SEL2_BIT) |
				tbsel | msm_host_offset->CORE_TESTBUS_ENA;
			sdhci_msm_writel_relaxed(test_bus_val, host,
				msm_host_offset->CORE_TESTBUS_CONFIG);
			debug_reg[index++] = sdhci_msm_readl_relaxed(host,
				msm_host_offset->CORE_SDCC_DEBUG_REG);
		}
	}
	for (i = 0; i < MAX_TEST_BUS; i = i + 4)
		pr_info(" Test bus[%d to %d]: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				i, i + 3, debug_reg[i], debug_reg[i+1],
				debug_reg[i+2], debug_reg[i+3]);
}

static void sdhci_msm_reset(struct sdhci_host *host, u8 mask)
{
	sdhci_reset(host, mask);
	if ((host->mmc->caps2 & MMC_CAP2_CQE) && (mask & SDHCI_RESET_ALL))
		cqhci_suspend(host->mmc);
}

/*
 * sdhci_msm_enhanced_strobe_mask :-
 * Before running CMDQ transfers in HS400 Enhanced Strobe mode,
 * SW should write 3 to
 * HC_VENDOR_SPECIFIC_FUNC3.CMDEN_HS400_INPUT_MASK_CNT register.
 * The default reset value of this register is 2.
 */
static void sdhci_msm_enhanced_strobe_mask(struct sdhci_host *host, bool set)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;

	if (!msm_host->enhanced_strobe ||
			!mmc_card_strobe(msm_host->mmc->card)) {
		pr_debug("%s: host/card does not support hs400 enhanced strobe\n",
				mmc_hostname(host->mmc));
		return;
	}

	if (set) {
		writel_relaxed((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC3)
			| CORE_CMDEN_HS400_INPUT_MASK_CNT),
			host->ioaddr + msm_host_offset->CORE_VENDOR_SPEC3);
	} else {
		writel_relaxed((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC3)
			& ~CORE_CMDEN_HS400_INPUT_MASK_CNT),
			host->ioaddr + msm_host_offset->CORE_VENDOR_SPEC3);
	}
}

static void sdhci_msm_clear_set_dumpregs(struct sdhci_host *host, bool set)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;

	if (set) {
		sdhci_msm_writel_relaxed(msm_host_offset->CORE_TESTBUS_ENA,
			host, msm_host_offset->CORE_TESTBUS_CONFIG);
	} else {
		u32 value;

		value = sdhci_msm_readl_relaxed(host,
			msm_host_offset->CORE_TESTBUS_CONFIG);
		value &= ~(msm_host_offset->CORE_TESTBUS_ENA);
		sdhci_msm_writel_relaxed(value, host,
			msm_host_offset->CORE_TESTBUS_CONFIG);
	}
}

void sdhci_msm_reset_workaround(struct sdhci_host *host, u32 enable)
{
	u32 vendor_func2;
	unsigned long timeout;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;

	vendor_func2 = readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_VENDOR_SPEC_FUNC2);

	if (enable) {
		writel_relaxed(vendor_func2 | HC_SW_RST_REQ, host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC_FUNC2);
		timeout = 10000;
		while (readl_relaxed(host->ioaddr +
		msm_host_offset->CORE_VENDOR_SPEC_FUNC2) & HC_SW_RST_REQ) {
			if (timeout == 0) {
				pr_info("%s: Applying wait idle disable workaround\n",
					mmc_hostname(host->mmc));
				/*
				 * Apply the reset workaround to not wait for
				 * pending data transfers on AXI before
				 * resetting the controller. This could be
				 * risky if the transfers were stuck on the
				 * AXI bus.
				 */
				vendor_func2 = readl_relaxed(host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC_FUNC2);
				writel_relaxed(vendor_func2 |
				HC_SW_RST_WAIT_IDLE_DIS, host->ioaddr +
				msm_host_offset->CORE_VENDOR_SPEC_FUNC2);
				host->reset_wa_t = ktime_get();
				return;
			}
			timeout--;
			udelay(10);
		}
		pr_info("%s: waiting for SW_RST_REQ is successful\n",
				mmc_hostname(host->mmc));
	} else {
		writel_relaxed(vendor_func2 & ~HC_SW_RST_WAIT_IDLE_DIS,
			host->ioaddr + msm_host_offset->CORE_VENDOR_SPEC_FUNC2);
	}
}

static void sdhci_msm_pm_qos_irq_unvote_work(struct work_struct *work)
{
	struct sdhci_msm_pm_qos_irq *pm_qos_irq =
		container_of(work, struct sdhci_msm_pm_qos_irq,
			     unvote_work.work);

	if (atomic_read(&pm_qos_irq->counter))
		return;

	pm_qos_irq->latency = PM_QOS_DEFAULT_VALUE;
	pm_qos_update_request(&pm_qos_irq->req, pm_qos_irq->latency);
}

void sdhci_msm_pm_qos_irq_vote(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct sdhci_msm_pm_qos_latency *latency =
		&msm_host->pdata->pm_qos_data.irq_latency;
	int counter;

	if (!msm_host->pm_qos_irq.enabled)
		return;
	if (host->power_policy > SDHCI_POWER_SAVE_MODE)
		return;

	counter = atomic_inc_return(&msm_host->pm_qos_irq.counter);
	/* Make sure to update the voting in case power policy has changed */
	if (msm_host->pm_qos_irq.latency == latency->latency[host->power_policy]
		&& counter > 1)
		return;

	cancel_delayed_work_sync(&msm_host->pm_qos_irq.unvote_work);
	msm_host->pm_qos_irq.latency = latency->latency[host->power_policy];
	pm_qos_update_request(&msm_host->pm_qos_irq.req,
				msm_host->pm_qos_irq.latency);
}

void sdhci_msm_pm_qos_irq_unvote(struct sdhci_host *host, bool async)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int counter;

	if (!msm_host->pm_qos_irq.enabled)
		return;

	if (atomic_read(&msm_host->pm_qos_irq.counter)) {
		counter = atomic_dec_return(&msm_host->pm_qos_irq.counter);
	} else {
		WARN(1, "attempt to decrement pm_qos_irq.counter when it's 0");
		return;
	}

	if (counter)
		return;

	if (async) {
		queue_delayed_work(msm_host->pm_qos_wq,
				&msm_host->pm_qos_irq.unvote_work,
				msecs_to_jiffies(QOS_REMOVE_DELAY_MS));
		return;
	}

	msm_host->pm_qos_irq.latency = PM_QOS_DEFAULT_VALUE;
	pm_qos_update_request(&msm_host->pm_qos_irq.req,
			msm_host->pm_qos_irq.latency);
}

static ssize_t
sdhci_msm_pm_qos_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct sdhci_msm_pm_qos_irq *irq = &msm_host->pm_qos_irq;

	return snprintf(buf, PAGE_SIZE,
		"IRQ PM QoS: enabled=%d, counter=%d, latency=%d\n",
		irq->enabled, atomic_read(&irq->counter), irq->latency);
}

static ssize_t
sdhci_msm_pm_qos_irq_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	return snprintf(buf, PAGE_SIZE, "%u\n", msm_host->pm_qos_irq.enabled);
}

static ssize_t
sdhci_msm_pm_qos_irq_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	uint32_t value;
	bool enable;
	int ret;

	ret = kstrtou32(buf, 0, &value);
	if (ret)
		goto out;
	enable = !!value;

	if (enable == msm_host->pm_qos_irq.enabled)
		goto out;

	msm_host->pm_qos_irq.enabled = enable;
	if (!enable) {
		cancel_delayed_work_sync(&msm_host->pm_qos_irq.unvote_work);
		atomic_set(&msm_host->pm_qos_irq.counter, 0);
		msm_host->pm_qos_irq.latency = PM_QOS_DEFAULT_VALUE;
		pm_qos_update_request(&msm_host->pm_qos_irq.req,
				msm_host->pm_qos_irq.latency);
	}

out:
	return count;
}

#ifdef CONFIG_SMP
static inline void set_affine_irq(struct sdhci_msm_host *msm_host,
				struct sdhci_host *host)
{
	msm_host->pm_qos_irq.req.irq = host->irq;
}
#else
static inline void set_affine_irq(struct sdhci_msm_host *msm_host,
				struct sdhci_host *host) { }
#endif

static bool sdhci_msm_pm_qos_wq_init(struct sdhci_msm_host *msm_host)
{
	char *wq = NULL;
	bool ret = true;

	wq = kasprintf(GFP_KERNEL, "sdhci_msm_pm_qos/%s",
			dev_name(&msm_host->pdev->dev));
	if (!wq)
		return false;
	/*
	 * Create a work queue with flag WQ_MEM_RECLAIM set for
	 * pm_qos_unvote work. Because mmc thread is created with
	 * flag PF_MEMALLOC set, kernel will check for work queue
	 * flag WQ_MEM_RECLAIM when flush the work queue. If work
	 * queue flag WQ_MEM_RECLAIM is not set, kernel warning
	 * will be triggered.
	 */
	msm_host->pm_qos_wq = create_workqueue(wq);
	if (!msm_host->pm_qos_wq) {
		ret = false;
		dev_err(&msm_host->pdev->dev,
				"failed to create pm qos unvote work queue\n");
	}
	kfree(wq);
	return ret;
}

void sdhci_msm_pm_qos_irq_init(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct sdhci_msm_pm_qos_latency *irq_latency;
	int ret;

	if (!msm_host->pdata->pm_qos_data.irq_valid)
		return;

	/* Initialize only once as this gets called per partition */
	if (msm_host->pm_qos_irq.enabled)
		return;

	atomic_set(&msm_host->pm_qos_irq.counter, 0);
	msm_host->pm_qos_irq.req.type =
			msm_host->pdata->pm_qos_data.irq_req_type;
	if ((msm_host->pm_qos_irq.req.type != PM_QOS_REQ_AFFINE_CORES) &&
		(msm_host->pm_qos_irq.req.type != PM_QOS_REQ_ALL_CORES))
		set_affine_irq(msm_host, host);
	else
		cpumask_copy(&msm_host->pm_qos_irq.req.cpus_affine,
			cpumask_of(msm_host->pdata->pm_qos_data.irq_cpu));

	sdhci_msm_pm_qos_wq_init(msm_host);

	INIT_DELAYED_WORK(&msm_host->pm_qos_irq.unvote_work,
		sdhci_msm_pm_qos_irq_unvote_work);
	/* For initialization phase, set the performance latency */
	irq_latency = &msm_host->pdata->pm_qos_data.irq_latency;
	msm_host->pm_qos_irq.latency =
		irq_latency->latency[SDHCI_PERFORMANCE_MODE];
	pm_qos_add_request(&msm_host->pm_qos_irq.req, PM_QOS_CPU_DMA_LATENCY,
			msm_host->pm_qos_irq.latency);
	msm_host->pm_qos_irq.enabled = true;

	/* sysfs */
	msm_host->pm_qos_irq.enable_attr.show =
		sdhci_msm_pm_qos_irq_enable_show;
	msm_host->pm_qos_irq.enable_attr.store =
		sdhci_msm_pm_qos_irq_enable_store;
	sysfs_attr_init(&msm_host->pm_qos_irq.enable_attr.attr);
	msm_host->pm_qos_irq.enable_attr.attr.name = "pm_qos_irq_enable";
	msm_host->pm_qos_irq.enable_attr.attr.mode = 0644;
	ret = device_create_file(&msm_host->pdev->dev,
		&msm_host->pm_qos_irq.enable_attr);
	if (ret)
		pr_err("%s: fail to create pm_qos_irq_enable (%d)\n",
			__func__, ret);

	msm_host->pm_qos_irq.status_attr.show = sdhci_msm_pm_qos_irq_show;
	msm_host->pm_qos_irq.status_attr.store = NULL;
	sysfs_attr_init(&msm_host->pm_qos_irq.status_attr.attr);
	msm_host->pm_qos_irq.status_attr.attr.name = "pm_qos_irq_status";
	msm_host->pm_qos_irq.status_attr.attr.mode = 0444;
	ret = device_create_file(&msm_host->pdev->dev,
			&msm_host->pm_qos_irq.status_attr);
	if (ret)
		pr_err("%s: fail to create pm_qos_irq_status (%d)\n",
			__func__, ret);
}

static ssize_t sdhci_msm_pm_qos_group_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct sdhci_msm_pm_qos_group *group;
	int i;
	int nr_groups = msm_host->pdata->pm_qos_data.cpu_group_map.nr_groups;
	int offset = 0;

	for (i = 0; i < nr_groups; i++) {
		group = &msm_host->pm_qos[i];
		offset += snprintf(&buf[offset], PAGE_SIZE,
			"Group #%d (mask=0x%lx) PM QoS: enabled=%d, counter=%d, latency=%d\n",
			i, group->req.cpus_affine.bits[0],
			msm_host->pm_qos_group_enable,
			atomic_read(&group->counter),
			group->latency);
	}

	return offset;
}

static ssize_t sdhci_msm_pm_qos_group_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	return snprintf(buf, PAGE_SIZE, "%s\n",
		msm_host->pm_qos_group_enable ? "enabled" : "disabled");
}

static ssize_t sdhci_msm_pm_qos_group_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int nr_groups = msm_host->pdata->pm_qos_data.cpu_group_map.nr_groups;
	uint32_t value;
	bool enable;
	int ret;
	int i;

	ret = kstrtou32(buf, 0, &value);
	if (ret)
		goto out;
	enable = !!value;

	if (enable == msm_host->pm_qos_group_enable)
		goto out;

	msm_host->pm_qos_group_enable = enable;
	if (!enable) {
		for (i = 0; i < nr_groups; i++) {
			cancel_delayed_work_sync(
				&msm_host->pm_qos[i].unvote_work);
			atomic_set(&msm_host->pm_qos[i].counter, 0);
			msm_host->pm_qos[i].latency = PM_QOS_DEFAULT_VALUE;
			pm_qos_update_request(&msm_host->pm_qos[i].req,
				msm_host->pm_qos[i].latency);
		}
	}

out:
	return count;
}

static int sdhci_msm_get_cpu_group(struct sdhci_msm_host *msm_host, int cpu)
{
	int i;
	struct sdhci_msm_cpu_group_map *map =
			&msm_host->pdata->pm_qos_data.cpu_group_map;

	if (cpu < 0)
		goto not_found;

	for (i = 0; i < map->nr_groups; i++)
		if (cpumask_test_cpu(cpu, &map->mask[i]))
			return i;

not_found:
	return -EINVAL;
}

void sdhci_msm_pm_qos_cpu_vote(struct sdhci_host *host,
		struct sdhci_msm_pm_qos_latency *latency, int cpu)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int group = sdhci_msm_get_cpu_group(msm_host, cpu);
	struct sdhci_msm_pm_qos_group *pm_qos_group;
	int counter;

	if (!msm_host->pm_qos_group_enable || group < 0)
		return;

	pm_qos_group = &msm_host->pm_qos[group];
	counter = atomic_inc_return(&pm_qos_group->counter);

	/* Make sure to update the voting in case power policy has changed */
	if (pm_qos_group->latency == latency->latency[host->power_policy]
		&& counter > 1)
		return;

	cancel_delayed_work_sync(&pm_qos_group->unvote_work);

	pm_qos_group->latency = latency->latency[host->power_policy];
	pm_qos_update_request(&pm_qos_group->req, pm_qos_group->latency);
}

static void sdhci_msm_pm_qos_cpu_unvote_work(struct work_struct *work)
{
	struct sdhci_msm_pm_qos_group *group =
		container_of(work, struct sdhci_msm_pm_qos_group,
			     unvote_work.work);

	if (atomic_read(&group->counter))
		return;

	group->latency = PM_QOS_DEFAULT_VALUE;
	pm_qos_update_request(&group->req, group->latency);
}

bool sdhci_msm_pm_qos_cpu_unvote(struct sdhci_host *host, int cpu, bool async)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int group = sdhci_msm_get_cpu_group(msm_host, cpu);

	if (!msm_host->pm_qos_group_enable || group < 0 ||
		atomic_dec_return(&msm_host->pm_qos[group].counter))
		return false;

	if (async) {
		queue_delayed_work(msm_host->pm_qos_wq,
				&msm_host->pm_qos[group].unvote_work,
				msecs_to_jiffies(QOS_REMOVE_DELAY_MS));
		return true;
	}

	msm_host->pm_qos[group].latency = PM_QOS_DEFAULT_VALUE;
	pm_qos_update_request(&msm_host->pm_qos[group].req,
				msm_host->pm_qos[group].latency);
	return true;
}

void sdhci_msm_pm_qos_cpu_init(struct sdhci_host *host,
		struct sdhci_msm_pm_qos_latency *latency)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int nr_groups = msm_host->pdata->pm_qos_data.cpu_group_map.nr_groups;
	struct sdhci_msm_pm_qos_group *group;
	int i;
	int ret;

	if (msm_host->pm_qos_group_enable)
		return;

	msm_host->pm_qos = kcalloc(nr_groups, sizeof(*msm_host->pm_qos),
			GFP_KERNEL);
	if (!msm_host->pm_qos)
		return;

	for (i = 0; i < nr_groups; i++) {
		group = &msm_host->pm_qos[i];
		INIT_DELAYED_WORK(&group->unvote_work,
			sdhci_msm_pm_qos_cpu_unvote_work);
		atomic_set(&group->counter, 0);
		group->req.type = PM_QOS_REQ_AFFINE_CORES;
		cpumask_copy(&group->req.cpus_affine,
			&msm_host->pdata->pm_qos_data.cpu_group_map.mask[i]);
		/* We set default latency here for all pm_qos cpu groups. */
		group->latency = PM_QOS_DEFAULT_VALUE;
		pm_qos_add_request(&group->req, PM_QOS_CPU_DMA_LATENCY,
			group->latency);
		pr_info("%s (): voted for group #%d (mask=0x%lx) latency=%d\n",
			__func__, i,
			group->req.cpus_affine.bits[0],
			group->latency);
	}
	msm_host->pm_qos_prev_cpu = -1;
	msm_host->pm_qos_group_enable = true;

	/* sysfs */
	msm_host->pm_qos_group_status_attr.show = sdhci_msm_pm_qos_group_show;
	msm_host->pm_qos_group_status_attr.store = NULL;
	sysfs_attr_init(&msm_host->pm_qos_group_status_attr.attr);
	msm_host->pm_qos_group_status_attr.attr.name =
			"pm_qos_cpu_groups_status";
	msm_host->pm_qos_group_status_attr.attr.mode = 0444;
	ret = device_create_file(&msm_host->pdev->dev,
			&msm_host->pm_qos_group_status_attr);
	if (ret)
		dev_err(&msm_host->pdev->dev, "%s: fail to create pm_qos_group_status_attr (%d)\n",
			__func__, ret);
	msm_host->pm_qos_group_enable_attr.show =
			sdhci_msm_pm_qos_group_enable_show;
	msm_host->pm_qos_group_enable_attr.store =
			sdhci_msm_pm_qos_group_enable_store;
	sysfs_attr_init(&msm_host->pm_qos_group_enable_attr.attr);
	msm_host->pm_qos_group_enable_attr.attr.name =
			"pm_qos_cpu_groups_enable";
	msm_host->pm_qos_group_enable_attr.attr.mode = 0444;
	ret = device_create_file(&msm_host->pdev->dev,
			&msm_host->pm_qos_group_enable_attr);
	if (ret)
		dev_err(&msm_host->pdev->dev, "%s: fail to create pm_qos_group_enable_attr (%d)\n",
			__func__, ret);
}

static void sdhci_msm_pre_req(struct sdhci_host *host,
		struct mmc_request *mmc_req)
{
	int cpu;
	int group;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int prev_group = sdhci_msm_get_cpu_group(msm_host,
			msm_host->pm_qos_prev_cpu);

	sdhci_msm_pm_qos_irq_vote(host);

	cpu = get_cpu();
	put_cpu();
	group = sdhci_msm_get_cpu_group(msm_host, cpu);
	if (group < 0)
		return;

	if (group != prev_group && prev_group >= 0) {
		sdhci_msm_pm_qos_cpu_unvote(host,
				msm_host->pm_qos_prev_cpu, false);
		prev_group = -1; /* make sure to vote for new group */
	}

	if (prev_group < 0) {
		sdhci_msm_pm_qos_cpu_vote(host,
				msm_host->pdata->pm_qos_data.latency, cpu);
		msm_host->pm_qos_prev_cpu = cpu;
	}
}

static void sdhci_msm_post_req(struct sdhci_host *host,
				struct mmc_request *mmc_req)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	sdhci_msm_pm_qos_irq_unvote(host, false);

	if (sdhci_msm_pm_qos_cpu_unvote(host, msm_host->pm_qos_prev_cpu, false))
		msm_host->pm_qos_prev_cpu = -1;
}

static void sdhci_msm_init(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;

	sdhci_msm_pm_qos_irq_init(host);

	if (msm_host->pdata->pm_qos_data.legacy_valid)
		sdhci_msm_pm_qos_cpu_init(host,
				msm_host->pdata->pm_qos_data.latency);
}

static unsigned int sdhci_msm_get_current_limit(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct sdhci_msm_slot_reg_data *curr_slot = msm_host->pdata->vreg_data;
	u32 max_curr = 0;

	if (curr_slot && curr_slot->vdd_data)
		max_curr = curr_slot->vdd_data->hpm_uA;

	return max_curr;
}

static int sdhci_msm_notify_load(struct sdhci_host *host, enum mmc_load state)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int ret = 0;
	u32 clk_rate = 0;

	if (!IS_ERR(msm_host->ice_clk)) {
		clk_rate = (state == MMC_LOAD_LOW) ?
			msm_host->pdata->ice_clk_min :
			msm_host->pdata->ice_clk_max;
		if (msm_host->ice_clk_rate == clk_rate)
			return 0;
		pr_debug("%s: changing ICE clk rate to %u\n",
				mmc_hostname(host->mmc), clk_rate);
		ret = clk_set_rate(msm_host->ice_clk, clk_rate);
		if (ret) {
			pr_err("%s: ICE_CLK rate set failed (%d) for %u\n",
				mmc_hostname(host->mmc), ret, clk_rate);
			return ret;
		}
		msm_host->ice_clk_rate = clk_rate;
	}
	return 0;
}

static void sdhci_msm_hw_reset(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct platform_device *pdev = msm_host->pdev;
	int ret = -ENOTSUPP;

	if (!msm_host->core_reset) {
		dev_err(&pdev->dev, "%s: failed, err = %d\n", __func__,
				ret);
		return;
	}

	msm_host->reg_store = true;
	sdhci_msm_exit_dbg_mode(host);
	sdhci_msm_registers_save(host);
	if (host->mmc->caps2 & MMC_CAP2_CQE) {
		host->mmc->cqe_ops->cqe_disable(host->mmc);
		host->mmc->cqe_enabled = false;
	}

	ret = reset_control_assert(msm_host->core_reset);
	if (ret) {
		dev_err(&pdev->dev, "%s: core_reset assert failed, err = %d\n",
				__func__, ret);
		goto out;
	}

	/*
	 * The hardware requirement for delay between assert/deassert
	 * is at least 3-4 sleep clock (32.7KHz) cycles, which comes to
	 * ~125us (4/32768). To be on the safe side add 200us delay.
	 */
	usleep_range(200, 210);

	ret = reset_control_deassert(msm_host->core_reset);
	if (ret)
		dev_err(&pdev->dev, "%s: core_reset deassert failed, err = %d\n",
				__func__, ret);

	sdhci_msm_registers_restore(host);
	msm_host->reg_store = false;
out:
	return;
}

static struct sdhci_ops sdhci_msm_ops = {
	.set_uhs_signaling = sdhci_msm_set_uhs_signaling,
	.check_power_status = sdhci_msm_check_power_status,
	.platform_execute_tuning = sdhci_msm_execute_tuning,
	.enhanced_strobe = sdhci_msm_enhanced_strobe,
	.toggle_cdr = sdhci_msm_toggle_cdr,
	.get_max_segments = sdhci_msm_max_segs,
	.set_clock = sdhci_msm_set_clock,
	.get_min_clock = sdhci_msm_get_min_clock,
	.get_max_clock = sdhci_msm_get_max_clock,
	.dump_vendor_regs = sdhci_msm_dump_vendor_regs,
	.config_auto_tuning_cmd = sdhci_msm_config_auto_tuning_cmd,
	.enable_controller_clock = sdhci_msm_enable_controller_clock,
	.set_bus_width = sdhci_set_bus_width,
	.reset = sdhci_msm_reset,
	.clear_set_dumpregs = sdhci_msm_clear_set_dumpregs,
	.enhanced_strobe_mask = sdhci_msm_enhanced_strobe_mask,
	.reset_workaround = sdhci_msm_reset_workaround,
	.init = sdhci_msm_init,
	.pre_req = sdhci_msm_pre_req,
	.post_req = sdhci_msm_post_req,
	.get_current_limit = sdhci_msm_get_current_limit,
	.notify_load = sdhci_msm_notify_load,
	.irq = sdhci_msm_cqe_irq,
	.enter_dbg_mode = sdhci_msm_enter_dbg_mode,
	.exit_dbg_mode = sdhci_msm_exit_dbg_mode,
	.hw_reset = sdhci_msm_hw_reset,
};

static void sdhci_set_default_hw_caps(struct sdhci_msm_host *msm_host,
		struct sdhci_host *host)
{
	u32 version, caps = 0;
	u16 minor;
	u8 major;
	u32 val;
	const struct sdhci_msm_offset *msm_host_offset =
					msm_host->offset;

	version = sdhci_msm_readl_relaxed(host,
		msm_host_offset->CORE_MCI_VERSION);
	major = (version & CORE_VERSION_MAJOR_MASK) >>
			CORE_VERSION_MAJOR_SHIFT;
	minor = version & CORE_VERSION_TARGET_MASK;

	caps = readl_relaxed(host->ioaddr + SDHCI_CAPABILITIES);

	/*
	 * Starting with SDCC 5 controller (core major version = 1)
	 * controller won't advertise 3.0v, 1.8v and 8-bit features
	 * except for some targets.
	 */
	if (major >= 1 && minor != 0x11 && minor != 0x12) {
		struct sdhci_msm_reg_data *vdd_io_reg;
		/*
		 * Enable 1.8V support capability on controllers that
		 * support dual voltage
		 */
		vdd_io_reg = msm_host->pdata->vreg_data->vdd_io_data;
		if (vdd_io_reg && (vdd_io_reg->high_vol_level > 2700000))
			caps |= CORE_3_0V_SUPPORT;
		if (vdd_io_reg && (vdd_io_reg->low_vol_level < 1950000))
			caps |= CORE_1_8V_SUPPORT;
		if (msm_host->pdata->mmc_bus_width == MMC_CAP_8_BIT_DATA)
			caps |= CORE_8_BIT_SUPPORT;
	}

	/*
	 * Enable one MID mode for SDCC5 (major 1) on 8916/8939 (minor 0x2e) and
	 * on 8992 (minor 0x3e) as a workaround to reset for data stuck issue.
	 */
	if (major == 1 && (minor == 0x2e || minor == 0x3e)) {
		host->quirks2 |= SDHCI_QUIRK2_USE_RESET_WORKAROUND;
		val = readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC_FUNC2);
		writel_relaxed((val | CORE_ONE_MID_EN),
			host->ioaddr + msm_host_offset->CORE_VENDOR_SPEC_FUNC2);
	}
	/*
	 * SDCC 5 controller with major version 1, minor version 0x34 and later
	 * with HS 400 mode support will use CM DLL instead of CDC LP 533 DLL.
	 */
	if ((major == 1) && (minor < 0x34))
		msm_host->use_cdclp533 = true;

	/*
	 * SDCC 5 controller with major version 1, minor version 0x42 and later
	 * will require additional steps when resetting DLL.
	 * It also supports HS400 enhanced strobe mode.
	 */
	if ((major == 1) && (minor >= 0x42)) {
		msm_host->use_updated_dll_reset = true;
		msm_host->enhanced_strobe = true;
		msm_host->pdata->caps2 |= MMC_CAP2_HS400_ES;
	}

	/*
	 * SDCC 5 controller with major version 1 and minor version 0x42,
	 * 0x46 and 0x49 currently uses 14lpp tech DLL whose internal
	 * gating cannot guarantee MCLK timing requirement i.e.
	 * when MCLK is gated OFF, it is not gated for less than 0.5us
	 * and MCLK must be switched on for at-least 1us before DATA
	 * starts coming.
	 */
	if ((major == 1) && ((minor == 0x42) || (minor == 0x46) ||
				(minor == 0x49) || (minor >= 0x6b)))
		msm_host->use_14lpp_dll = true;

	/* Fake 3.0V support for SDIO devices which requires such voltage */
	if (msm_host->core_3_0v_support) {
		caps |= CORE_3_0V_SUPPORT;
			writel_relaxed((readl_relaxed(host->ioaddr +
			SDHCI_CAPABILITIES) | caps), host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC_CAPABILITIES0);
	}

	if ((major == 1) && (minor >= 0x49))
		msm_host->rclk_delay_fix = true;
	/*
	 * Mask 64-bit support for controller with 32-bit address bus so that
	 * smaller descriptor size will be used and improve memory consumption.
	 */
	if (!msm_host->pdata->largeaddressbus)
		caps &= ~CORE_SYS_BUS_SUPPORT_64_BIT;

	writel_relaxed(caps, host->ioaddr +
		msm_host_offset->CORE_VENDOR_SPEC_CAPABILITIES0);
	/* keep track of the value in SDHCI_CAPABILITIES */
	msm_host->caps_0 = caps;

	if ((major == 1) && (minor >= 0x6b)) {
		host->cdr_support = true;
	}

	/* 7FF projects with 7nm DLL */
	if ((major == 1) && ((minor == 0x6e) || (minor == 0x71) ||
				(minor == 0x72)))
		msm_host->use_7nm_dll = true;
}

static bool sdhci_msm_is_bootdevice(struct device *dev)
{
	if (strnstr(saved_command_line, "androidboot.bootdevice=",
		    strlen(saved_command_line))) {
		char search_string[50];

		snprintf(search_string, ARRAY_SIZE(search_string),
			"androidboot.bootdevice=%s", dev_name(dev));
		if (strnstr(saved_command_line, search_string,
		    strlen(saved_command_line)))
			return true;
		else
			return false;
	}

	/*
	 * "androidboot.bootdevice=" argument is not present then
	 * return true as we don't know the boot device anyways.
	 */
	return true;
}

static int sdhci_msm_setup_ice_clk(struct sdhci_msm_host *msm_host,
						struct platform_device *pdev)
{
	int ret = 0;

	/* Setup SDC ICE clock */
	msm_host->ice_clk = devm_clk_get(&pdev->dev, "ice_core_clk");
	if (!IS_ERR(msm_host->ice_clk)) {
		/* ICE core has only one clock frequency for now */
		ret = clk_set_rate(msm_host->ice_clk,
				msm_host->pdata->ice_clk_max);
		if (ret) {
			dev_err(&pdev->dev, "ICE_CLK rate set failed (%d) for %u\n",
				ret,
				msm_host->pdata->ice_clk_max);
			return ret;
		}
		ret = clk_prepare_enable(msm_host->ice_clk);
		if (ret)
			return ret;
		msm_host->ice_clk_rate =
			msm_host->pdata->ice_clk_max;
	}

	return ret;
}

/*
 * Changes the bus speed mode for eMMC only as per the
 * kernel command line parameter passed in the boot image.
 * If not set remains in HS400ES mode.
 */
static void sdhci_msm_select_bus_mode(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	enum select_bus_mode {SELECT_HS400ES, SELECT_HS400,
				SELECT_HS200, SELECT_DDR52,
				SELECT_HS};

	if (bus_mode) {
		if (bus_mode == SELECT_HS400) {
			msm_host->enhanced_strobe = false;
			msm_host->pdata->caps2 &= ~(MMC_CAP2_HS400_ES);
		} else if (bus_mode == SELECT_HS200) {
			msm_host->pdata->caps2 &= ~(MMC_CAP2_HS400_ES);
			msm_host->pdata->caps2 &= ~(MMC_CAP2_HS400);
		} else if (bus_mode == SELECT_DDR52) {
			host->quirks2 |= SDHCI_QUIRK2_BROKEN_HS200;
			msm_host->pdata->caps2 &= ~(MMC_CAP2_HS400_ES);
			msm_host->pdata->caps2 &= ~(MMC_CAP2_HS400);
			msm_host->pdata->caps2 &= ~(MMC_CAP2_HS200);
		} else if (bus_mode == SELECT_HS) {
			host->quirks2 |= SDHCI_QUIRK2_BROKEN_HS200;
			msm_host->pdata->caps2 &= ~(MMC_CAP2_HS400_ES);
			msm_host->pdata->caps2 &= ~(MMC_CAP2_HS400);
			msm_host->pdata->caps2 &= ~(MMC_CAP2_HS200);
			msm_host->pdata->caps &= ~(MMC_CAP_3_3V_DDR |
					MMC_CAP_1_8V_DDR | MMC_CAP_1_2V_DDR);
			msm_host->mmc->clk_scaling.lower_bus_speed_mode &=
				~(MMC_SCALING_LOWER_DDR52_MODE);
		}
		pr_info("%s: %s: bus_mode=%d set using kernel command line\n",
			mmc_hostname(host->mmc), __func__, bus_mode);
	}
}

static int sdhci_msm_probe(struct platform_device *pdev)
{
	const struct sdhci_msm_offset *msm_host_offset;
	struct sdhci_host *host;
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_msm_host *msm_host;
	struct resource *core_memres = NULL;
	struct device_node *node = pdev->dev.of_node;
	int ret = 0, dead = 0;
	u16 host_version;
	u32 irq_status, irq_ctl;
	struct resource *tlmm_memres = NULL;
	void __iomem *tlmm_mem;
	unsigned long flags;
	bool force_probe;

	pr_debug("%s: Enter %s\n", dev_name(&pdev->dev), __func__);
	msm_host = devm_kzalloc(&pdev->dev, sizeof(struct sdhci_msm_host),
				GFP_KERNEL);
	if (!msm_host) {
		ret = -ENOMEM;
		goto out;
	}

	if (of_find_compatible_node(NULL, NULL, "qcom,sdhci-msm-v5")) {
		msm_host->mci_removed = true;
		msm_host->offset = &sdhci_msm_offset_mci_removed;
	} else {
		msm_host->mci_removed = false;
		msm_host->offset = &sdhci_msm_offset_mci_present;
	}
	msm_host_offset = msm_host->offset;
	msm_host->sdhci_msm_pdata.ops = &sdhci_msm_ops;
	host = sdhci_pltfm_init(pdev, &msm_host->sdhci_msm_pdata, 0);
	if (IS_ERR(host)) {
		ret = PTR_ERR(host);
		goto out_host_free;
	}

	pltfm_host = sdhci_priv(host);
	pltfm_host->priv = msm_host;
	msm_host->mmc = host->mmc;
	msm_host->pdev = pdev;

	/* Extract platform data */
	if (pdev->dev.of_node) {
		ret = of_alias_get_id(pdev->dev.of_node, "sdhc");
		if (ret <= 0) {
			dev_err(&pdev->dev, "Failed to get slot index %d\n",
				ret);
			goto pltfm_free;
		}

		/* Read property to determine if the probe is forced */
		force_probe = of_find_property(pdev->dev.of_node,
			"qcom,force-sdhc1-probe", NULL);

		/* skip the probe if eMMC isn't a boot device */
		if ((ret == 1) && !sdhci_msm_is_bootdevice(&pdev->dev)
		    && !force_probe) {
			/* Turn off MEMCORE_ON for core_clk */
			msm_host->clk = devm_clk_get(&pdev->dev, "core_clk");
			if (!IS_ERR(msm_host->clk)) {
				ret = clk_set_flags(msm_host->clk,
						CLKFLAG_NORETAIN_MEM);
				if (ret)
					dev_err(&pdev->dev,
					"Core clk set NORETAIN_MEM failed: %d\n",
					ret);
			}
			ret = -ENODEV;
			goto pltfm_free;
		}

		if (disable_slots & (1 << (ret - 1))) {
			dev_info(&pdev->dev, "%s: Slot %d disabled\n", __func__,
				ret);
			ret = -ENODEV;
			goto pltfm_free;
		}

		if (ret <= 2)
			sdhci_slot[ret-1] = msm_host;

		msm_host->pdata = sdhci_msm_populate_pdata(&pdev->dev,
							   msm_host);
		if (!msm_host->pdata) {
			dev_err(&pdev->dev, "DT parsing error\n");
			goto pltfm_free;
		}
	} else {
		dev_err(&pdev->dev, "No device tree node\n");
		goto pltfm_free;
	}

	/* Setup Clocks */

	/* Setup SDCC bus voter clock. */
	msm_host->bus_clk = devm_clk_get(&pdev->dev, "bus_clk");
	if (!IS_ERR_OR_NULL(msm_host->bus_clk)) {
		/* Vote for max. clk rate for max. performance */
		ret = clk_set_rate(msm_host->bus_clk, INT_MAX);
		if (ret)
			goto pltfm_free;
		ret = clk_prepare_enable(msm_host->bus_clk);
		if (ret)
			goto pltfm_free;
	}

	/* Setup main peripheral bus clock */
	msm_host->pclk = devm_clk_get(&pdev->dev, "iface_clk");
	if (!IS_ERR(msm_host->pclk)) {
		ret = clk_prepare_enable(msm_host->pclk);
		if (ret) {
			dev_err(&pdev->dev, "Iface clk not enabled (%d)\n"
					, ret);
			goto bus_clk_disable;
		}
	} else {
		ret = PTR_ERR(msm_host->pclk);
		dev_err(&pdev->dev, "Iface clk get failed (%d)\n", ret);
	}
	atomic_set(&msm_host->controller_clock, 1);

	/* Setup SDC ufs bus aggr clock */
	msm_host->bus_aggr_clk = devm_clk_get(&pdev->dev, "bus_aggr_clk");
	if (!IS_ERR(msm_host->bus_aggr_clk)) {
		ret = clk_prepare_enable(msm_host->bus_aggr_clk);
		if (ret) {
			dev_err(&pdev->dev, "Bus aggregate clk not enabled\n");
			goto pclk_disable;
		}
	}

	ret = sdhci_msm_setup_ice_clk(msm_host, pdev);
	if (ret)
		goto pclk_disable;

	/* Setup SDC MMC clock */
	msm_host->clk = devm_clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(msm_host->clk)) {
		ret = PTR_ERR(msm_host->clk);
		dev_err(&pdev->dev, "Core clk get failed (%d)\n", ret);
		goto bus_aggr_clk_disable;
	}

	/* Set to the minimum supported clock frequency */
	ret = clk_set_rate(msm_host->clk, sdhci_msm_get_min_clock(host));
	if (ret) {
		dev_err(&pdev->dev, "MClk rate set failed (%d)\n", ret);
		goto bus_aggr_clk_disable;
	}
	ret = clk_prepare_enable(msm_host->clk);
	if (ret) {
		dev_err(&pdev->dev, "Core clk not enabled (%d)\n", ret);
		goto bus_aggr_clk_disable;
	}
	ret = clk_set_flags(msm_host->clk, CLKFLAG_NORETAIN_MEM);
	if (ret)
		dev_err(&pdev->dev, "Core clk set NORETAIN_MEM failed: %d\n",
			ret);

	msm_host->clk_rate = sdhci_msm_get_min_clock(host);
	atomic_set(&msm_host->clks_on, 1);

	/* Setup CDC calibration fixed feedback clock */
	msm_host->ff_clk = devm_clk_get(&pdev->dev, "cal_clk");
	if (!IS_ERR(msm_host->ff_clk)) {
		ret = clk_prepare_enable(msm_host->ff_clk);
		if (ret)
			goto clk_disable;
	}

	/* Setup CDC calibration sleep clock */
	msm_host->sleep_clk = devm_clk_get(&pdev->dev, "sleep_clk");
	if (!IS_ERR(msm_host->sleep_clk)) {
		ret = clk_prepare_enable(msm_host->sleep_clk);
		if (ret)
			goto ff_clk_disable;
	}

	msm_host->saved_tuning_phase = INVALID_TUNING_PHASE;

	ret = sdhci_msm_bus_register(msm_host, pdev);
	if (ret)
		goto sleep_clk_disable;

	sdhci_msm_bus_voting(host, 1);

	/* Setup regulators */
	ret = sdhci_msm_vreg_init(&pdev->dev, msm_host->pdata, true);
	if (ret) {
		dev_err(&pdev->dev, "Regulator setup failed (%d)\n", ret);
		goto bus_unregister;
	}

	/* Reset the core and Enable SDHC mode */
	core_memres = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "core_mem");
	if (!msm_host->mci_removed) {
		if (!core_memres) {
			dev_err(&pdev->dev, "Failed to get iomem resource\n");
			goto vreg_deinit;
		}
		msm_host->core_mem = devm_ioremap(&pdev->dev,
			core_memres->start, resource_size(core_memres));

		if (!msm_host->core_mem) {
			dev_err(&pdev->dev, "Failed to remap registers\n");
			ret = -ENOMEM;
			goto vreg_deinit;
		}
	}

	tlmm_memres = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "tlmm_mem");
	if (tlmm_memres) {
		tlmm_mem = devm_ioremap(&pdev->dev, tlmm_memres->start,
						resource_size(tlmm_memres));

		if (!tlmm_mem) {
			dev_err(&pdev->dev, "Failed to remap tlmm registers\n");
			ret = -ENOMEM;
			goto vreg_deinit;
		}
		writel_relaxed(readl_relaxed(tlmm_mem) | 0x2, tlmm_mem);
	}

	/*
	 * Reset the vendor spec register to power on reset state.
	 */
	writel_relaxed(CORE_VENDOR_SPEC_POR_VAL,
	host->ioaddr + msm_host_offset->CORE_VENDOR_SPEC);

	/* This enable ADMA error interrupt in case of length mismatch */
	writel_relaxed((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC) |
			CORE_VNDR_SPEC_ADMA_ERR_SIZE_EN),
			host->ioaddr + msm_host_offset->CORE_VENDOR_SPEC);

	/*
	 * Ensure SDHCI FIFO is enabled by disabling alternative FIFO
	 */
	writel_relaxed((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC3) &
			~CORE_FIFO_ALT_EN), host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC3);

	if (!msm_host->mci_removed) {
		/* Set HC_MODE_EN bit in HC_MODE register */
		writel_relaxed(HC_MODE_EN, (msm_host->core_mem + CORE_HC_MODE));

		/* Set FF_CLK_SW_RST_DIS bit in HC_MODE register */
		writel_relaxed(readl_relaxed(msm_host->core_mem +
				CORE_HC_MODE) | FF_CLK_SW_RST_DIS,
				msm_host->core_mem + CORE_HC_MODE);
	}
	sdhci_set_default_hw_caps(msm_host, host);

	sdhci_msm_select_bus_mode(host);
	/*
	 * Set the PAD_PWR_SWITCH_EN bit so that the PAD_PWR_SWITCH bit can
	 * be used as required later on.
	 */
	writel_relaxed((readl_relaxed(host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC) |
			CORE_IO_PAD_PWR_SWITCH_EN), host->ioaddr +
			msm_host_offset->CORE_VENDOR_SPEC);
	/*
	 * CORE_SW_RST above may trigger power irq if previous status of PWRCTL
	 * was either BUS_ON or IO_HIGH_V. So before we enable the power irq
	 * interrupt in GIC (by registering the interrupt handler), we need to
	 * ensure that any pending power irq interrupt status is acknowledged
	 * otherwise power irq interrupt handler would be fired prematurely.
	 */
	irq_status = sdhci_msm_readl_relaxed(host,
		msm_host_offset->CORE_PWRCTL_STATUS);
	sdhci_msm_writel_relaxed(irq_status, host,
		msm_host_offset->CORE_PWRCTL_CLEAR);
	irq_ctl = sdhci_msm_readl_relaxed(host,
		msm_host_offset->CORE_PWRCTL_CTL);

	if (irq_status & (CORE_PWRCTL_BUS_ON | CORE_PWRCTL_BUS_OFF))
		irq_ctl |= CORE_PWRCTL_BUS_SUCCESS;
	if (irq_status & (CORE_PWRCTL_IO_HIGH | CORE_PWRCTL_IO_LOW))
		irq_ctl |= CORE_PWRCTL_IO_SUCCESS;
	sdhci_msm_writel_relaxed(irq_ctl, host,
		msm_host_offset->CORE_PWRCTL_CTL);

	/*
	 * Ensure that above writes are propogated before interrupt enablement
	 * in GIC.
	 */
	mb();

	/*
	 * Following are the deviations from SDHC spec v3.0 -
	 * 1. Card detection is handled using separate GPIO.
	 * 2. Bus power control is handled by interacting with PMIC.
	 */
	host->quirks |= SDHCI_QUIRK_BROKEN_CARD_DETECTION;
	host->quirks |= SDHCI_QUIRK_SINGLE_POWER_WRITE;
	host->quirks |= SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN;
	host->quirks |= SDHCI_QUIRK_MULTIBLOCK_READ_ACMD12;
	host->quirks |= SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC;
	host->quirks2 |= SDHCI_QUIRK2_ALWAYS_USE_BASE_CLOCK;
	host->quirks2 |= SDHCI_QUIRK2_IGNORE_DATATOUT_FOR_R1BCMD;
	host->quirks2 |= SDHCI_QUIRK2_BROKEN_PRESET_VALUE;
	host->quirks2 |= SDHCI_QUIRK2_USE_RESERVED_MAX_TIMEOUT;
	host->quirks2 |= SDHCI_QUIRK2_NON_STANDARD_TUNING;
	host->quirks2 |= SDHCI_QUIRK2_USE_PIO_FOR_EMMC_TUNING;

	if (host->quirks2 & SDHCI_QUIRK2_ALWAYS_USE_BASE_CLOCK)
		host->quirks2 |= SDHCI_QUIRK2_DIVIDE_TOUT_BY_4;

	msm_host->minor = IPCAT_MINOR_MASK(readl_relaxed(host->ioaddr +
				SDCC_IP_CATALOG));

	host_version = readw_relaxed((host->ioaddr + SDHCI_HOST_VERSION));
	dev_dbg(&pdev->dev, "Host Version: 0x%x Vendor Version 0x%x\n",
		host_version, ((host_version & SDHCI_VENDOR_VER_MASK) >>
		  SDHCI_VENDOR_VER_SHIFT));
	if (((host_version & SDHCI_VENDOR_VER_MASK) >>
		SDHCI_VENDOR_VER_SHIFT) == SDHCI_VER_100) {
		/*
		 * Add 40us delay in interrupt handler when
		 * operating at initialization frequency(400KHz).
		 */
		host->quirks2 |= SDHCI_QUIRK2_SLOW_INT_CLR;
		/*
		 * Set Software Reset for DAT line in Software
		 * Reset Register (Bit 2).
		 */
		host->quirks2 |= SDHCI_QUIRK2_RDWR_TX_ACTIVE_EOT;
	}

	host->quirks2 |= SDHCI_QUIRK2_IGN_DATA_END_BIT_ERROR;

	/* Setup PWRCTL irq */
	msm_host->pwr_irq = platform_get_irq_byname(pdev, "pwr_irq");
	if (msm_host->pwr_irq < 0) {
		dev_err(&pdev->dev, "Failed to get pwr_irq by name (%d)\n",
				msm_host->pwr_irq);
		goto vreg_deinit;
	}
	ret = devm_request_threaded_irq(&pdev->dev, msm_host->pwr_irq, NULL,
					sdhci_msm_pwr_irq, IRQF_ONESHOT,
					dev_name(&pdev->dev), host);
	if (ret) {
		dev_err(&pdev->dev, "Request threaded irq(%d) failed (%d)\n",
				msm_host->pwr_irq, ret);
		goto vreg_deinit;
	}

	/* Enable pwr irq interrupts */
	sdhci_msm_writel_relaxed(INT_MASK, host,
		msm_host_offset->CORE_PWRCTL_MASK);

	/* Set host capabilities */
	msm_host->mmc->caps |= msm_host->pdata->mmc_bus_width;
	msm_host->mmc->caps |= msm_host->pdata->caps;
	msm_host->mmc->caps |= MMC_CAP_AGGRESSIVE_PM;
	msm_host->mmc->caps |= MMC_CAP_WAIT_WHILE_BUSY;
	msm_host->mmc->caps2 |= msm_host->pdata->caps2;
	msm_host->mmc->caps2 |= MMC_CAP2_BOOTPART_NOACC;
	msm_host->mmc->caps2 |= MMC_CAP2_HS400_POST_TUNING;
	msm_host->mmc->caps2 |= MMC_CAP2_CLK_SCALE;
	msm_host->mmc->caps2 |= MMC_CAP2_SANITIZE;
	msm_host->mmc->caps2 |= MMC_CAP2_MAX_DISCARD_SIZE;
	msm_host->mmc->caps2 |= MMC_CAP2_SLEEP_AWAKE;
	msm_host->mmc->pm_caps |= MMC_PM_KEEP_POWER | MMC_PM_WAKE_SDIO_IRQ;
	if (msm_host->core_reset)
		msm_host->mmc->caps |= MMC_CAP_HW_RESET;

	if (msm_host->pdata->nonremovable)
		msm_host->mmc->caps |= MMC_CAP_NONREMOVABLE;

	if (msm_host->pdata->nonhotplug)
		msm_host->mmc->caps2 |= MMC_CAP2_NONHOTPLUG;

	init_completion(&msm_host->pwr_irq_completion);

	if (gpio_is_valid(msm_host->pdata->status_gpio)) {
		/*
		 * Set up the card detect GPIO in active configuration before
		 * configuring it as an IRQ. Otherwise, it can be in some
		 * weird/inconsistent state resulting in flood of interrupts.
		 */
		sdhci_msm_setup_pins(msm_host->pdata, true);

		/*
		 * This delay is needed for stabilizing the card detect GPIO
		 * line after changing the pull configs.
		 */
		usleep_range(10000, 10500);
		ret = mmc_gpio_request_cd(msm_host->mmc,
				msm_host->pdata->status_gpio, 0);
		if (ret) {
			dev_err(&pdev->dev, "%s: Failed to request card detection IRQ %d\n",
					__func__, ret);
			goto vreg_deinit;
		}
	}

	if ((sdhci_readl(host, SDHCI_CAPABILITIES) & SDHCI_CAN_64BIT) &&
		(dma_supported(mmc_dev(host->mmc), DMA_BIT_MASK(64)))) {
		host->dma_mask = DMA_BIT_MASK(64);
		mmc_dev(host->mmc)->dma_mask = &host->dma_mask;
		mmc_dev(host->mmc)->coherent_dma_mask  = host->dma_mask;
	} else if (dma_supported(mmc_dev(host->mmc), DMA_BIT_MASK(32))) {
		host->dma_mask = DMA_BIT_MASK(32);
		mmc_dev(host->mmc)->dma_mask = &host->dma_mask;
		mmc_dev(host->mmc)->coherent_dma_mask  = host->dma_mask;
	} else {
		dev_err(&pdev->dev, "%s: Failed to set dma mask\n", __func__);
	}

	msm_host->pdata->sdiowakeup_irq = platform_get_irq_byname(pdev,
							  "sdiowakeup_irq");
	if (sdhci_is_valid_gpio_wakeup_int(msm_host)) {
		dev_info(&pdev->dev, "%s: sdiowakeup_irq = %d\n", __func__,
				msm_host->pdata->sdiowakeup_irq);
		msm_host->is_sdiowakeup_enabled = true;
		ret = request_irq(msm_host->pdata->sdiowakeup_irq,
				  sdhci_msm_sdiowakeup_irq,
				  IRQF_SHARED | IRQF_TRIGGER_HIGH,
				  "sdhci-msm sdiowakeup", host);
		if (ret) {
			dev_err(&pdev->dev, "%s: request sdiowakeup IRQ %d: failed: %d\n",
				__func__, msm_host->pdata->sdiowakeup_irq, ret);
			msm_host->pdata->sdiowakeup_irq = -1;
			msm_host->is_sdiowakeup_enabled = false;
			goto vreg_deinit;
		} else {
			spin_lock_irqsave(&host->lock, flags);
			sdhci_msm_cfg_sdiowakeup_gpio_irq(host, false);
			msm_host->sdio_pending_processing = false;
			spin_unlock_irqrestore(&host->lock, flags);
		}
	}

	msm_host->pdata->testbus_trigger_irq = platform_get_irq_byname(pdev,
							  "tb_trig_irq");
	if (sdhci_is_valid_gpio_testbus_trigger_int(msm_host)) {
		dev_info(&pdev->dev, "%s: testbus_trigger_irq = %d\n", __func__,
				msm_host->pdata->testbus_trigger_irq);
		ret = request_irq(msm_host->pdata->testbus_trigger_irq,
				  sdhci_msm_testbus_trigger_irq,
				  IRQF_SHARED | IRQF_TRIGGER_RISING,
				  "sdhci-msm tb_trig", host);
		if (ret) {
			dev_err(&pdev->dev, "%s: request tb_trig IRQ %d: failed: %d\n",
				__func__, msm_host->pdata->testbus_trigger_irq,
				ret);
		}
	}

	if (of_device_is_compatible(node, "qcom,sdhci-msm-cqe")) {
		dev_dbg(&pdev->dev, "node with qcom,sdhci-msm-cqe\n");
		ret = sdhci_msm_cqe_add_host(host, pdev);
	} else {
		ret = sdhci_add_host(host);
	}
	if (ret) {
		dev_err(&pdev->dev, "Add host failed (%d)\n", ret);
		goto vreg_deinit;
	}

	/*
	 * To avoid polling and to avoid this R1b command conversion
	 * to R1 command if the requested busy timeout > host's max
	 * busy timeout in case of sanitize, erase or any R1b command
	 */
	host->mmc->max_busy_timeout = 0;

	msm_host->pltfm_init_done = true;

	msm_host->mmc->caps |= MMC_CAP_WAIT_WHILE_BUSY | MMC_CAP_NEED_RSP_BUSY;

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, MSM_AUTOSUSPEND_DELAY_MS);
	pm_runtime_use_autosuspend(&pdev->dev);

	msm_host->msm_bus_vote.max_bus_bw.show = show_sdhci_max_bus_bw;
	msm_host->msm_bus_vote.max_bus_bw.store = store_sdhci_max_bus_bw;
	sysfs_attr_init(&msm_host->msm_bus_vote.max_bus_bw.attr);
	msm_host->msm_bus_vote.max_bus_bw.attr.name = "max_bus_bw";
	msm_host->msm_bus_vote.max_bus_bw.attr.mode = 0644;
	ret = device_create_file(&pdev->dev,
			&msm_host->msm_bus_vote.max_bus_bw);
	if (ret)
		goto remove_host;

	if (!gpio_is_valid(msm_host->pdata->status_gpio)) {
		msm_host->polling.show = show_polling;
		msm_host->polling.store = store_polling;
		sysfs_attr_init(&msm_host->polling.attr);
		msm_host->polling.attr.name = "polling";
		msm_host->polling.attr.mode = 0644;
		ret = device_create_file(&pdev->dev, &msm_host->polling);
		if (ret)
			goto remove_max_bus_bw_file;
	}

	msm_host->auto_cmd21_attr.show = show_auto_cmd21;
	msm_host->auto_cmd21_attr.store = store_auto_cmd21;
	sysfs_attr_init(&msm_host->auto_cmd21_attr.attr);
	msm_host->auto_cmd21_attr.attr.name = "enable_auto_cmd21";
	msm_host->auto_cmd21_attr.attr.mode = 0644;
	ret = device_create_file(&pdev->dev, &msm_host->auto_cmd21_attr);
	if (ret) {
		pr_err("%s: %s: failed creating auto-cmd21 attr: %d\n",
		       mmc_hostname(host->mmc), __func__, ret);
		device_remove_file(&pdev->dev, &msm_host->auto_cmd21_attr);
	}

	if (msm_host->minor >= 2) {
		msm_host->mask_and_match.show = show_mask_and_match;
		msm_host->mask_and_match.store = store_mask_and_match;
		sysfs_attr_init(&msm_host->mask_and_match.attr);
		msm_host->mask_and_match.attr.name = "mask_and_match";
		msm_host->mask_and_match.attr.mode = 0644;
		ret = device_create_file(&pdev->dev,
					&msm_host->mask_and_match);
		if (ret) {
			pr_err("%s: %s: failed creating M&M attr: %d\n",
					mmc_hostname(host->mmc), __func__, ret);
		}
	}

	if (sdhci_msm_is_bootdevice(&pdev->dev))
		mmc_flush_detect_work(host->mmc);
	/* Successful initialization */
	goto out;

remove_max_bus_bw_file:
	device_remove_file(&pdev->dev, &msm_host->msm_bus_vote.max_bus_bw);
remove_host:
	dead = (readl_relaxed(host->ioaddr + SDHCI_INT_STATUS) == 0xffffffff);
	pm_runtime_disable(&pdev->dev);
	sdhci_remove_host(host, dead);
vreg_deinit:
	sdhci_msm_vreg_init(&pdev->dev, msm_host->pdata, false);
bus_unregister:
	sdhci_msm_bus_voting(host, 0);
	sdhci_msm_bus_unregister(msm_host);
sleep_clk_disable:
	if (!IS_ERR(msm_host->sleep_clk))
		clk_disable_unprepare(msm_host->sleep_clk);
ff_clk_disable:
	if (!IS_ERR(msm_host->ff_clk))
		clk_disable_unprepare(msm_host->ff_clk);
clk_disable:
	if (!IS_ERR(msm_host->clk))
		clk_disable_unprepare(msm_host->clk);
bus_aggr_clk_disable:
	if (!IS_ERR(msm_host->bus_aggr_clk))
		clk_disable_unprepare(msm_host->bus_aggr_clk);
pclk_disable:
	if (!IS_ERR(msm_host->pclk))
		clk_disable_unprepare(msm_host->pclk);
bus_clk_disable:
	if (!IS_ERR_OR_NULL(msm_host->bus_clk))
		clk_disable_unprepare(msm_host->bus_clk);
pltfm_free:
	sdhci_pltfm_free(pdev);
out_host_free:
	devm_kfree(&pdev->dev, msm_host);
out:
	pr_debug("%s: Exit %s\n", dev_name(&pdev->dev), __func__);
	return ret;
}

static int sdhci_msm_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	struct sdhci_msm_pltfm_data *pdata = msm_host->pdata;
	int nr_groups = msm_host->pdata->pm_qos_data.cpu_group_map.nr_groups;
	int i;
	int dead = (readl_relaxed(host->ioaddr + SDHCI_INT_STATUS) ==
			0xffffffff);

	pr_debug("%s: %s Enter\n", dev_name(&pdev->dev), __func__);
	if (!gpio_is_valid(msm_host->pdata->status_gpio))
		device_remove_file(&pdev->dev, &msm_host->polling);

	device_remove_file(&pdev->dev, &msm_host->auto_cmd21_attr);
	device_remove_file(&pdev->dev, &msm_host->msm_bus_vote.max_bus_bw);
	pm_runtime_disable(&pdev->dev);

	if (msm_host->pm_qos_group_enable) {
		struct sdhci_msm_pm_qos_group *group;

		for (i = 0; i < nr_groups; i++)
			cancel_delayed_work_sync(
					&msm_host->pm_qos[i].unvote_work);

		device_remove_file(&msm_host->pdev->dev,
			&msm_host->pm_qos_group_enable_attr);
		device_remove_file(&msm_host->pdev->dev,
			&msm_host->pm_qos_group_status_attr);

		for (i = 0; i < nr_groups; i++) {
			group = &msm_host->pm_qos[i];
			pm_qos_remove_request(&group->req);
		}
	}

	if (msm_host->pm_qos_irq.enabled) {
		cancel_delayed_work_sync(&msm_host->pm_qos_irq.unvote_work);
		device_remove_file(&pdev->dev,
				&msm_host->pm_qos_irq.enable_attr);
		device_remove_file(&pdev->dev,
				&msm_host->pm_qos_irq.status_attr);
		pm_qos_remove_request(&msm_host->pm_qos_irq.req);
	}

	if (msm_host->pm_qos_wq)
		destroy_workqueue(msm_host->pm_qos_wq);

	sdhci_remove_host(host, dead);

	sdhci_msm_vreg_init(&pdev->dev, msm_host->pdata, false);

	sdhci_msm_setup_pins(pdata, true);
	sdhci_msm_setup_pins(pdata, false);

	sdhci_msm_bus_voting(host, 0);
	if (msm_host->msm_bus_vote.client_handle)
		sdhci_msm_bus_unregister(msm_host);

	sdhci_pltfm_free(pdev);

	return 0;
}

#ifdef CONFIG_PM
static int sdhci_msm_cfg_sdio_wakeup(struct sdhci_host *host, bool enable)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	unsigned long flags;
	int ret = 0;

	if (!(host->mmc->card && mmc_card_sdio(host->mmc->card) &&
	      sdhci_is_valid_gpio_wakeup_int(msm_host) &&
	      mmc_card_wake_sdio_irq(host->mmc))) {
		msm_host->sdio_pending_processing = false;
		return 1;
	}

	spin_lock_irqsave(&host->lock, flags);
	if (enable) {
		/* configure DAT1 gpio if applicable */
		if (sdhci_is_valid_gpio_wakeup_int(msm_host)) {
			msm_host->sdio_pending_processing = false;
			ret = enable_irq_wake(msm_host->pdata->sdiowakeup_irq);
			if (!ret)
				sdhci_msm_cfg_sdiowakeup_gpio_irq(host, true);
			goto out;
		} else {
			pr_err("%s: sdiowakeup_irq(%d) invalid\n",
					mmc_hostname(host->mmc), enable);
		}
	} else {
		if (sdhci_is_valid_gpio_wakeup_int(msm_host)) {
			ret = disable_irq_wake(msm_host->pdata->sdiowakeup_irq);
			sdhci_msm_cfg_sdiowakeup_gpio_irq(host, false);
			msm_host->sdio_pending_processing = false;
		} else {
			pr_err("%s: sdiowakeup_irq(%d)invalid\n",
					mmc_hostname(host->mmc), enable);

		}
	}
out:
	if (ret)
		pr_err("%s: %s: %sable wakeup: failed: %d gpio: %d\n",
		       mmc_hostname(host->mmc), __func__, enable ? "en" : "dis",
		       ret, msm_host->pdata->sdiowakeup_irq);
	spin_unlock_irqrestore(&host->lock, flags);
	return ret;
}


static int sdhci_msm_runtime_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	ktime_t start = ktime_get();

	if (host->mmc->card && mmc_card_sdio(host->mmc->card))
		goto defer_disable_host_irq;

	sdhci_cfg_irq(host, false, true);

defer_disable_host_irq:
	disable_irq(msm_host->pwr_irq);

	if (host->mmc->card && !mmc_host_may_gate_card(host->mmc->card))
		goto skip_clk_gating;

	sdhci_msm_disable_controller_clock(host);

skip_clk_gating:
	trace_sdhci_msm_runtime_suspend(mmc_hostname(host->mmc), 0,
			ktime_to_us(ktime_sub(ktime_get(), start)));
	return 0;
}

static int sdhci_msm_runtime_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int ret;
	ktime_t start = ktime_get();

	if (host->mmc->card && !mmc_host_may_gate_card(host->mmc->card))
		goto skip_clk_ungating;

	ret = sdhci_msm_enable_controller_clock(host);
	if (ret) {
		pr_err("%s: Failed to enable reqd clocks\n",
			mmc_hostname(host->mmc));
	}

skip_clk_ungating:

	if (host->mmc->ios.timing == MMC_TIMING_MMC_HS400)
		sdhci_msm_toggle_fifo_write_clk(host);

	if (host->mmc->card && mmc_card_sdio(host->mmc->card))
		goto defer_enable_host_irq;

	sdhci_cfg_irq(host, true, true);

defer_enable_host_irq:
	enable_irq(msm_host->pwr_irq);

	trace_sdhci_msm_runtime_resume(mmc_hostname(host->mmc), 0,
			ktime_to_us(ktime_sub(ktime_get(), start)));
	return 0;
}

static int sdhci_msm_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	int ret = 0;
	int sdio_cfg = 0;
	ktime_t start = ktime_get();

	if (pm_runtime_suspended(dev)) {
		pr_debug("%s: %s: already runtime suspended\n",
		mmc_hostname(host->mmc), __func__);
		goto out;
	}
	ret = sdhci_msm_runtime_suspend(dev);
out:
	if (host->mmc->card && mmc_card_sdio(host->mmc->card)) {
		sdio_cfg = sdhci_msm_cfg_sdio_wakeup(host, true);
		if (sdio_cfg)
			sdhci_cfg_irq(host, false, true);
	}

	trace_sdhci_msm_suspend(mmc_hostname(host->mmc), ret,
			ktime_to_us(ktime_sub(ktime_get(), start)));
	return ret;
}

static int sdhci_msm_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	int ret = 0;
	int sdio_cfg = 0;
	ktime_t start = ktime_get();

	if (pm_runtime_suspended(dev)) {
		pr_debug("%s: %s: runtime suspended, defer system resume\n",
		mmc_hostname(host->mmc), __func__);
		goto out;
	}

	ret = sdhci_msm_runtime_resume(dev);
out:
	if (host->mmc->card && mmc_card_sdio(host->mmc->card)) {
		sdio_cfg = sdhci_msm_cfg_sdio_wakeup(host, false);
		if (sdio_cfg)
			sdhci_cfg_irq(host, true, true);
	}

	trace_sdhci_msm_resume(mmc_hostname(host->mmc), ret,
			ktime_to_us(ktime_sub(ktime_get(), start)));
	return ret;
}

static int sdhci_msm_suspend_noirq(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_msm_host *msm_host = pltfm_host->priv;
	int ret = 0;

	/*
	 * ksdioirqd may be running, hence retry
	 * suspend in case the clocks are ON
	 */
	if (atomic_read(&msm_host->clks_on)) {
		pr_warn("%s: %s: clock ON after suspend, aborting suspend\n",
			mmc_hostname(host->mmc), __func__);
		ret = -EAGAIN;
	}

	if (host->mmc->card && mmc_card_sdio(host->mmc->card))
		if (msm_host->sdio_pending_processing)
			ret = -EBUSY;

	return ret;
}

static const struct dev_pm_ops sdhci_msm_pmops = {
	SET_LATE_SYSTEM_SLEEP_PM_OPS(sdhci_msm_suspend, sdhci_msm_resume)
	SET_RUNTIME_PM_OPS(sdhci_msm_runtime_suspend, sdhci_msm_runtime_resume,
			   NULL)
	.suspend_noirq = sdhci_msm_suspend_noirq,
};

#define SDHCI_MSM_PMOPS (&sdhci_msm_pmops)

#else
#define SDHCI_MSM_PMOPS NULL
#endif
static const struct of_device_id sdhci_msm_dt_match[] = {
	{.compatible = "qcom,sdhci-msm"},
	{.compatible = "qcom,sdhci-msm-v5"},
	{.compatible = "qcom,sdhci-msm-cqe"},
	{},
};
MODULE_DEVICE_TABLE(of, sdhci_msm_dt_match);

static struct platform_driver sdhci_msm_driver = {
	.probe		= sdhci_msm_probe,
	.remove		= sdhci_msm_remove,
	.driver		= {
		.name	= "sdhci_msm",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = sdhci_msm_dt_match,
		.pm	= SDHCI_MSM_PMOPS,
	},
};

module_platform_driver(sdhci_msm_driver);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. SDHCI driver");
MODULE_LICENSE("GPL v2");
