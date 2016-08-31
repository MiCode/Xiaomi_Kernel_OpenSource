/*
 * ahci-tegra.c - AHCI SATA support for TEGRA AHCI device
 *
 * Copyright (c) 2011-2013, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * libata documentation is available via 'make {ps|pdf}docs',
 * as Documentation/DocBook/libata.*
 *
 * AHCI hardware documentation:
 * http://www.intel.com/technology/serialata/pdf/rev1_0.pdf
 * http://www.intel.com/technology/serialata/pdf/rev1_1.pdf
 *
 */

#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/dmi.h>
#include <linux/gpio.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <linux/libata.h>
#include <linux/regulator/machine.h>
#include <linux/pm_runtime.h>
#include "ahci.h"

#include <linux/clk.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-powergate.h>
#include <linux/platform_data/tegra_ahci.h>

#include "../../arch/arm/mach-tegra/iomap.h"

#define DRV_NAME	"tegra-sata"
#define DRV_VERSION	"1.0"

#define ENABLE_AHCI_DBG_PRINT			0
#if ENABLE_AHCI_DBG_PRINT
#define AHCI_DBG_PRINT(fmt, arg...)  printk(KERN_ERR fmt, ## arg)
#else
#define AHCI_DBG_PRINT(fmt, arg...) do {} while (0)
#endif

/* number of AHCI ports */
#define TEGRA_AHCI_NUM_PORTS			1

/* idle timeout for PM in msec */
#define TEGRA_AHCI_MIN_IDLE_TIME		1000
#define TEGRA_AHCI_DEFAULT_IDLE_TIME		2000

#ifdef CONFIG_TEGRA_SATA_IDLE_POWERGATE
static u32 tegra_ahci_idle_time = TEGRA_AHCI_DEFAULT_IDLE_TIME;
#endif

#define PORT_BASE	(TEGRA_SATA_BAR5_BASE + 0x100)

/* Bit 0 (EN_FPCI) to allow FPCI accesses to SATA */
#define SATA_CONFIGURATION_0_OFFSET		0x180
#define EN_FPCI					(1 << 0)
#define CLK_OVERRIDE				(1 << 31)

#define SATA_INTR_MASK_0_OFFSET			0x188
#define IP_INT_MASK				(1 << 16)

/* Need to write 0x00400200 to 0x70020094 */
#define SATA_FPCI_BAR5_0_OFFSET			0x094
#define PRI_ICTLR_CPU_IER_SET_0_OFFSET		0x024
#define CPU_IER_SATA_CTL			(1 << 23)

#define AHCI_BAR5_CONFIG_LOCATION		0x24
#define TEGRA_SATA_BAR5_INIT_PROGRAM		0xFFFFFFFF
#define TEGRA_SATA_BAR5_FINAL_PROGRAM		0x40020000

#define FUSE_SATA_CALIB_OFFSET			0x224
#define FUSE_SATA_CALIB_MASK			0x3

#define T_SATA0_CFG_PHY_REG			0x120
#define T_SATA0_CFG_PHY_SQUELCH_MASK		(1 << 24)
#define PHY_USE_7BIT_ALIGN_DET_FOR_SPD_MASK	(1 << 11)

#define T_SATA0_CFG_POWER_GATE			0x4ac
#define POWER_GATE_SSTS_RESTORED_MASK		(1 << 23)
#define POWER_GATE_SSTS_RESTORED_YES		(1 << 23)
#define POWER_GATE_SSTS_RESTORED_NO		(0 << 23)

#define T_SATA0_DBG0_OFFSET			0x550

#define T_SATA0_INDEX_OFFSET			0x680
#define SATA0_NONE_SELECTED			0
#define SATA0_CH1_SELECTED			(1 << 0)

#define T_SATA0_CHX_PHY_CTRL1_GEN1_OFFSET	0x690
#define SATA0_CHX_PHY_CTRL1_GEN1_TX_AMP_SHIFT	0
#define SATA0_CHX_PHY_CTRL1_GEN1_TX_AMP_MASK	(0xff << 0)
#define SATA0_CHX_PHY_CTRL1_GEN1_TX_PEAK_SHIFT	8
#define SATA0_CHX_PHY_CTRL1_GEN1_TX_PEAK_MASK	(0xff << 8)

#define T_SATA0_CHX_PHY_CTRL1_GEN2_OFFSET	0x694
#define SATA0_CHX_PHY_CTRL1_GEN2_TX_AMP_SHIFT	0
#define SATA0_CHX_PHY_CTRL1_GEN2_TX_AMP_MASK	(0xff << 0)
#define SATA0_CHX_PHY_CTRL1_GEN2_TX_PEAK_SHIFT	12
#define SATA0_CHX_PHY_CTRL1_GEN2_TX_PEAK_MASK	(0xff << 12)
#define SATA0_CHX_PHY_CTRL1_GEN2_RX_EQ_SHIFT	24
#define SATA0_CHX_PHY_CTRL1_GEN2_RX_EQ_MASK	(0xf << 24)

/* AHCI config space defines */
#define TEGRA_PRIVATE_AHCI_CC_BKDR		0x4a4
#define TEGRA_PRIVATE_AHCI_CC_BKDR_OVERRIDE	0x54c
#define TEGRA_PRIVATE_AHCI_CC_BKDR_OVERRIDE_EN	(1 << 12)
#define TEGRA_PRIVATE_AHCI_CC_BKDR_PGM		0x01060100

/* AHCI HBA_CAP */
#define TEGRA_PRIVATE_AHCI_CAP_BKDR		0xa0
#define T_SATA0_AHCI_HBA_CAP_BKDR		0x300
#define AHCI_HBA_PLL_CTRL_0			0xa8

#define CLAMP_TXCLK_ON_SLUMBER			(1 << 13)
#define CLAMP_TXCLK_ON_DEVSLP			(1 << 24)
#define NO_CLAMP_SHUT_DOWN			(1 << 3)

#define TEGRA_SATA_IO_SPACE_OFFSET		4
#define TEGRA_SATA_ENABLE_IO_SPACE		(1 << 0)
#define TEGRA_SATA_ENABLE_MEM_SPACE		(1 << 1)
#define TEGRA_SATA_ENABLE_BUS_MASTER		(1 << 2)
#define TEGRA_SATA_ENABLE_SERR			(1 << 8)
#define TEGRA_SATA_CORE_CLOCK_FREQ_HZ		(105*1000*1000)
#define TEGRA_SATA_OOB_CLOCK_FREQ_HZ		(272*1000*1000)

#define APB_PMC_SATA_PWRGT_0_REG		0x1ac
#define APBDEV_PMC_PWRGATE_TOGGLE_0		0x30
#define APBDEV_PMC_PWRGATE_STATUS_0		0x38
#define APBDEV_PMC_REMOVE_CLAMPING_CMD_0	0x34

#define CLK_RST_SATA_PLL_CFG0_REG		0x490
#define CLK_RST_SATA_PLL_CFG1_REG		0x494
#define CLK_RST_CONTROLLER_RST_DEVICES_V_0	0x358
#define CLK_RST_CONTROLLER_RST_DEVICES_W_0	0x35c
#define CLK_RST_CONTROLLER_RST_DEV_W_CLR_0	0x43c
#define CLK_RST_CONTROLLER_RST_DEV_V_CLR_0	0x434
#define CLK_RST_CONTROLLER_CLK_ENB_V_CLR_0	0x444
#define CLK_RST_CONTROLLER_CLK_ENB_V_SET_0	0x440
#define CLK_RST_CONTROLLER_CLK_ENB_V_0		0x360


#define CLR_CLK_ENB_SATA_OOB			(1 << 27)
#define CLR_CLK_ENB_SATA			(1 << 28)

#define CLK_RST_CONTROLLER_PLLE_MISC_0		0x0ec
#define CLK_RST_CONTROLLER_PLLE_MISC_0_VALUE	0x00070300
#define CLK_RST_CONTROLLER_PLLE_BASE_0		0xe8
#define PLLE_ENABLE				(1 << 30)
#define CLK_RST_CONTROLLER_PLLE_AUX_0		0x48c
#define CLK_RST_CONTROLLER_PLLE_AUX_0_MASK	(1 << 1)

#define CLR_SATACOLD_RST			(1 << 1)
#define SWR_SATACOLD_RST			(1 << 1)
#define SWR_SATA_RST				(1 << 28)
#define SWR_SATA_OOB_RST			(1 << 27)
#define DEVSLP_OVERRIDE				(1 << 17)
#define SDS_SUPPORT				(1 << 13)
#define DESO_SUPPORT				(1 << 15)
#define SATA_AUX_PAD_PLL_CNTL_1_REG		0x1100
#define SATA_AUX_MISC_CNTL_1_REG		0x1108
#define SATA_AUX_SPARE_CFG0_0			0x1118

/* for APB_PMC_SATA_PWRGT_0_REG */
#define PG_INFO_MASK				(1 << 6)
#define PG_INFO_ON				(1 << 6)
#define PG_INFO_OFF				(0 << 6)
#define PLLE_IDDQ_SWCTL_MASK			(1 << 4)
#define PADPHY_IDDQ_OVERRIDE_VALUE_MASK		(1 << 3)
#define PADPHY_IDDQ_OVERRIDE_VALUE_ON		(1 << 3)
#define PADPHY_IDDQ_OVERRIDE_VALUE_OFF		(0 << 3)
#define PADPHY_IDDQ_SWCTL_MASK			(1 << 2)
#define PADPHY_IDDQ_SWCTL_ON			(1 << 2)
#define PADPHY_IDDQ_SWCTL_OFF			(0 << 2)
#define PADPLL_IDDQ_OVERRIDE_VALUE_MASK		(1 << 1)
#define PADPLL_IDDQ_OVERRIDE_VALUE_ON		(1 << 1)
#define PADPLL_IDDQ_OVERRIDE_VALUE_OFF		(0 << 1)
#define PADPLL_IDDQ_SWCTL_MASK			(1 << 0)
#define PADPLL_IDDQ_SWCTL_ON			(1 << 0)
#define PADPLL_IDDQ_SWCTL_OFF			(0 << 0)

#define START					(1 < 8)
#define PARTID_VALUE				0x8

#define SAX_MASK				(1 << 8)

/* for CLK_RST_SATA_PLL_CFG0_REG */
#define PADPLL_RESET_OVERRIDE_VALUE_MASK	(1 << 1)
#define PADPLL_RESET_OVERRIDE_VALUE_ON		(1 << 1)
#define PADPLL_RESET_OVERRIDE_VALUE_OFF		(0 << 1)
#define PADPLL_RESET_SWCTL_MASK			(1 << 0)
#define PADPLL_RESET_SWCTL_ON			(1 << 0)
#define PADPLL_RESET_SWCTL_OFF			(0 << 0)
#define PLLE_IDDQ_SWCTL_ON			(1 << 4)
#define PLLE_IDDQ_SWCTL_OFF			(0 << 4)
#define PLLE_SATA_SEQ_ENABLE			(1 << 24)
#define PLLE_SATA_SEQ_START_STATE		(1 << 25)
#define SATA_SEQ_PADPLL_PD_INPUT_VALUE		(1 << 5)
#define SATA_SEQ_LANE_PD_INPUT_VALUE		(1 << 6)
#define SATA_SEQ_RESET_INPUT_VALUE		(1 << 7)

/* for CLK_RST_SATA_PLL_CFG1_REG */
#define IDDQ2LANE_SLUMBER_DLY_MASK		(0xffL << 16)
#define IDDQ2LANE_SLUMBER_DLY_SHIFT		16
#define IDDQ2LANE_SLUMBER_DLY_3MS		(3 << 16)
#define IDDQ2LANE_IDDQ_DLY_SHIFT		0
#define IDDQ2LANE_IDDQ_DLY_MASK			(0xffL << 0)

/* for SATA_AUX_PAD_PLL_CNTL_1_REG */
#define REFCLK_SEL_MASK				(3 << 11)
#define REFCLK_SEL_INT_CML			(0 << 11)
#define LOCKDET_FIELD				(1 << 6)

/* for SATA_AUX_MISC_CNTL_1_REG */
#define NVA2SATA_OOB_ON_POR_MASK		(1 << 7)
#define NVA2SATA_OOB_ON_POR_YES			(1 << 7)
#define NVA2SATA_OOB_ON_POR_NO			(0 << 7)
#define L0_RX_IDLE_T_SAX_SHIFT			5
#define L0_RX_IDLE_T_SAX_MASK			(3 << 5)
#define L0_RX_IDLE_T_NPG_SHIFT			3
#define L0_RX_IDLE_T_NPG_MASK			(3 << 3)
#define L0_RX_IDLE_T_MUX_MASK			(1 << 2)
#define L0_RX_IDLE_T_MUX_FROM_APB_MISC		(1 << 2)
#define L0_RX_IDLE_T_MUX_FROM_SATA		(0 << 2)

#define SSTAT_IPM_STATE_MASK			0xF00
#define SSTAT_IPM_SLUMBER_STATE			0x600
#define XUSB_PADCTL_USB3_PAD_MUX_0              0x134
#define FORCE_SATA_PAD_IDDQ_DISABLE_MASK0	(1 << 6)
#define XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0		0x138
#define XUSB_PADCTL_PLL1_MODE			(1 << 24)
#define XUSB_PADCTL_ELPG_PROGRAM_0		0x01c
#define AUX_ELPG_CLAMP_EN			(1 << 24)
#define AUX_ELPG_CLAMP_EN_EARLY			(1 << 25)
#define AUX_ELPG_VCORE_DOWN			(1 << 26)
#define XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL_1_0	0x148
#define XUSB_PADCTL_IOPHY_MISC_IDDQ_OVRD	(1 << 1)
#define XUSB_PADCTL_IOPHY_MISC_IDDQ		(1 << 0)

#define SATA_AXI_BAR5_START_0			0x54
#define SATA_AXI_BAR5_SZ_0			0x14
#define SATA_AXI_BAR5_START_VALUE		0x70020
#define AXI_BAR5_SIZE_VALUE			0x00008
#define FPCI_BAR5_0_START_VALUE			0x0010000
#define FPCI_BAR5_0_FINAL_VALUE			0x40020100
#define FPCI_BAR5_0_ACCESS_TYPE			(1 << 0)

#define XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL_1_0	0x148
#define IDDQ_OVRD_MASK				(1 << 1)
#define IDDQ_MASK				(1 << 0)

/* for XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0 */
#define PLL_PWR_OVRD_MASK			(1 << 3)
#define PLL_RST_MASK				(1 << 1)
#define PLL_IDDQ_MASK				(1 << 0)

/* for CLK_RST_CONTROLLER_PLLE_MISC_0 */
#define T124_PLLE_IDDQ_SWCTL_MASK		(1 << 14)
#define PLLE_IDDQ_OVERRIDE_VALUE_MASK		(1 << 13)

/* Spread Settings */
#define SATA0_CHX_PHY_CTRL11_0			0x6D0
#define SATA0_CHX_PHY_CTRL2_0			0x69c
#define GEN2_RX_EQ				(0x2800 << 16)
#define CDR_CNTL_GEN1				0x23

#define CLK_RST_CONTROLLER_PLLE_SS_CNTL_0	0x68
#define PLLE_SSCCENTER				(1 << 14)
#define PLLE_SSCINVERT				(1 << 15)
#define PLLE_SSCMAX				(0x25)
#define PLLE_SSCINCINTRV			(0x20 << 24)
#define PLLE_SSCINC				(1 << 16)
#define PLLE_BYPASS_SS				(1 << 10)
#define PLLE_SSCBYP				(1 << 12)
#define PLLE_INTERP_RESET			(1 << 11)

#define SATA_AUX_RX_STAT_INT_0			0x110c
#define SATA_RX_STAT_INT_DISABLE		(1 << 2)

#define T_SATA0_NVOOB				0x114
#define T_SATA0_NVOOB_SQUELCH_FILTER_MODE_SHIFT	24
#define T_SATA0_NVOOB_SQUELCH_FILTER_MODE_MASK	(3 << 24)
#define T_SATA0_NVOOB_SQUELCH_FILTER_LENGTH_SHIFT	26
#define T_SATA0_NVOOB_SQUELCH_FILTER_LENGTH_MASK	(3 << 26)

#define PXSSTS_DEVICE_DETECTED			(1 << 0)

#ifdef CONFIG_TEGRA_SATA_IDLE_POWERGATE

/* create a work for handling the async transfers */
static void tegra_ahci_work_handler(struct work_struct *work);
static DECLARE_WORK(tegra_ahci_work, tegra_ahci_work_handler);
static struct workqueue_struct *tegra_ahci_work_q;
#endif

enum {
	AHCI_PCI_BAR = 5,
};

enum port_idle_status {
	PORT_IS_NOT_IDLE,
	PORT_IS_IDLE,
	PORT_IS_IDLE_NOT_SLUMBER,
	PORT_IS_SLUMBER,
};

enum sata_state {
	SATA_ON,
	SATA_OFF,
	SATA_GOING_ON,
	SATA_GOING_OFF,
	SATA_ABORT_OFF,
};

enum clk_gate_state {
	CLK_OFF,
	CLK_ON,
};

char *sata_power_rails[] = {
	"avdd_sata",
	"vdd_sata",
	"hvdd_sata",
	"avdd_sata_pll",
	"vddio_pex_sata"
};

#define NUM_SATA_POWER_RAILS	ARRAY_SIZE(sata_power_rails)

struct tegra_qc_list {
	struct list_head list;
	struct ata_queued_cmd *qc;
};

/*
 *  tegra_ahci_host_priv is the extension of ahci_host_priv
 *  with extra fields: idle_timer, pg_save, pg_state, etc.
 */
struct tegra_ahci_host_priv {
	struct ahci_host_priv	ahci_host_priv;
	struct regulator	*power_rails[NUM_SATA_POWER_RAILS];
	void __iomem		*bars_table[6];
	struct ata_host		*host;
	struct timer_list	idle_timer;
	struct device		*dev;
	void			*pg_save;
	enum sata_state		pg_state;
	struct list_head	qc_list;
	struct clk		*clk_sata;
	struct clk		*clk_sata_oob;
	struct clk		*clk_pllp;
	struct clk		*clk_cml1;
	enum clk_gate_state	clk_state;
};

static int tegra_ahci_init_one(struct platform_device *pdev);
static int tegra_ahci_remove_one(struct platform_device *pdev);
static void tegra_ahci_set_clk_rst_cnt_rst_dev(void);
static void tegra_ahci_clr_clk_rst_cnt_rst_dev(void);
static void tegra_ahci_pad_config(void);
static void tegra_ahci_put_sata_in_iddq(void);
static void tegra_ahci_iddqlane_config(void);

#ifdef CONFIG_PM
static bool tegra_ahci_power_un_gate(struct ata_host *host);
static bool tegra_ahci_power_gate(struct ata_host *host);
static void tegra_ahci_abort_power_gate(struct ata_host *host);
static int tegra_ahci_controller_suspend(struct platform_device *pdev);
static int tegra_ahci_controller_resume(struct platform_device *pdev);
static int tegra_ahci_suspend(struct platform_device *pdev, pm_message_t mesg);
static int tegra_ahci_resume(struct platform_device *pdev);
static enum port_idle_status tegra_ahci_is_port_idle(struct ata_port *ap);
static bool tegra_ahci_are_all_ports_idle(struct ata_host *host);
#ifdef CONFIG_TEGRA_SATA_IDLE_POWERGATE
static bool tegra_ahci_pad_resume(struct ata_host *host);
static bool tegra_ahci_pad_suspend(struct ata_host *host);
static void tegra_ahci_abort_pad_suspend(struct ata_host *host);
static unsigned int tegra_ahci_qc_issue(struct ata_queued_cmd *qc);
static int tegra_ahci_hardreset(struct ata_link *link, unsigned int *class,
				unsigned long deadline);
static int tegra_ahci_runtime_suspend(struct device *dev);
static int tegra_ahci_runtime_resume(struct device *dev);
static void tegra_ahci_idle_timer(unsigned long arg);
static int tegra_ahci_queue_one_qc(struct tegra_ahci_host_priv *tegra_hpriv,
				   struct ata_queued_cmd *qc);
static void tegra_ahci_dequeue_qcs(struct tegra_ahci_host_priv *tegra_hpriv);
#endif
#else
#define tegra_ahci_controller_suspend	NULL
#define tegra_ahci_controller_resume	NULL
#define tegra_ahci_suspend		NULL
#define tegra_ahci_resume		NULL
#endif

static struct scsi_host_template ahci_sht = {
	AHCI_SHT("tegra-sata"),
};

static struct ata_port_operations tegra_ahci_ops = {
	.inherits	= &ahci_ops,
#ifdef CONFIG_PM
#ifdef CONFIG_TEGRA_SATA_IDLE_POWERGATE
	.qc_issue	= tegra_ahci_qc_issue,
	.hardreset	= tegra_ahci_hardreset,
#endif
#endif
};

static const struct ata_port_info ahci_port_info = {
	.flags		= AHCI_FLAG_COMMON,
	.pio_mask	= 0x1f, /* pio0-4 */
	.udma_mask	= ATA_UDMA6,
	.port_ops	= &tegra_ahci_ops,
};

#ifdef CONFIG_TEGRA_SATA_IDLE_POWERGATE
static const struct dev_pm_ops tegra_ahci_dev_rt_ops = {
	.runtime_suspend = tegra_ahci_runtime_suspend,
	.runtime_resume = tegra_ahci_runtime_resume,
};
#endif

static struct platform_driver tegra_platform_ahci_driver = {
	.probe		= tegra_ahci_init_one,
	.remove		= tegra_ahci_remove_one,
#ifdef CONFIG_PM
	.suspend	= tegra_ahci_suspend,
	.resume		= tegra_ahci_resume,
#endif
	.driver = {
		.name = DRV_NAME,
	}
};

struct tegra_ahci_host_priv *g_tegra_hpriv;

static inline u32 port_readl(u32 offset)
{
	u32 val;

	val = readl(IO_ADDRESS(PORT_BASE + offset));
	AHCI_DBG_PRINT("[0x%x] => 0x%08x\n", PORT_BASE + offset, val);
	return val;
}

static inline void port_writel(u32 val, u32 offset)
{
	AHCI_DBG_PRINT("[0x%x] => 0x%08x\n", PORT_BASE + offset, val);
	writel(val, IO_ADDRESS(PORT_BASE + offset));
}

static inline u32 bar5_readl(u32 offset)
{
	u32 val;
	val = readl(IO_ADDRESS(TEGRA_SATA_BAR5_BASE + offset));
	AHCI_DBG_PRINT("[0x%x] => 0x%08x\n", TEGRA_SATA_BAR5_BASE+offset,
			val);
	return val;
}

static inline void bar5_writel(u32 val, u32 offset)
{
	AHCI_DBG_PRINT("[0x%x] <= 0x%08x\n", TEGRA_SATA_BAR5_BASE+offset,
			val);
	writel(val, IO_ADDRESS(TEGRA_SATA_BAR5_BASE + offset));
}


static inline u32 xusb_readl(u32 offset)
{
	u32 val;
	val = readl(IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE + offset));
	AHCI_DBG_PRINT("[0x%x] => 0x%08x\n", TEGRA_XUSB_PADCTL_BASE+offset,
			val);
	return val;
}

static inline void xusb_writel(u32 val, u32 offset)
{
	AHCI_DBG_PRINT("[0x%x] <= 0x%08x\n", TEGRA_XUSB_PADCTL_BASE+offset,
			val);
	writel(val, IO_ADDRESS(TEGRA_XUSB_PADCTL_BASE + offset));
}


static inline u32 pmc_readl(u32 offset)
{
	u32 val;
	val = readl(IO_ADDRESS(TEGRA_PMC_BASE + offset));
	AHCI_DBG_PRINT("[0x%x] => 0x%08x\n", TEGRA_PMC_BASE+offset, val);
	return val;
}

static inline void pmc_writel(u32 val, u32 offset)
{
	AHCI_DBG_PRINT("[0x%x] <= 0x%08x\n", TEGRA_PMC_BASE+offset, val);
	writel(val, IO_ADDRESS(TEGRA_PMC_BASE + offset));
}

static inline u32 clk_readl(u32 offset)
{
	u32 val;

	val = readl(IO_ADDRESS(TEGRA_CLK_RESET_BASE + offset));
	AHCI_DBG_PRINT("[0x%x] => 0x%08x\n", TEGRA_CLK_RESET_BASE+offset, val);
	return val;
}

static inline void clk_writel(u32 val, u32 offset)
{
	AHCI_DBG_PRINT("[0x%x] <= 0x%08x\n", TEGRA_CLK_RESET_BASE+offset, val);
	writel(val, IO_ADDRESS(TEGRA_CLK_RESET_BASE + offset));
}

static inline u32 misc_readl(u32 offset)
{
	u32 val;

	val = readl(IO_ADDRESS(TEGRA_APB_MISC_BASE + offset));
	AHCI_DBG_PRINT("[0x%x] => 0x%08x\n", TEGRA_APB_MISC_BASE+offset, val);
	return val;
}

static inline void misc_writel(u32 val, u32 offset)
{
	AHCI_DBG_PRINT("[0x%x] <= 0x%08x\n", TEGRA_APB_MISC_BASE+offset, val);
	writel(val, IO_ADDRESS(TEGRA_APB_MISC_BASE + offset));
}

static inline u32 sata_readl(u32 offset)
{
	u32 val;

	val = readl(IO_ADDRESS(TEGRA_SATA_BASE + offset));
	AHCI_DBG_PRINT("[0x%x] => 0x%08x\n", TEGRA_SATA_BASE+offset, val);
	return val;
}

static inline void sata_writel(u32 val, u32 offset)
{
	AHCI_DBG_PRINT("[0x%x] <= 0x%08x\n", TEGRA_SATA_BASE+offset, val);
	writel(val, IO_ADDRESS(TEGRA_SATA_BASE + offset));
}

static inline u32 scfg_readl(u32 offset)
{
	u32 val;

	val = readl(IO_ADDRESS(TEGRA_SATA_CONFIG_BASE + offset));
	AHCI_DBG_PRINT("[0x%x] => 0x%08x\n", TEGRA_SATA_CONFIG_BASE+offset,
					     val);
	return val;
}

static inline void scfg_writel(u32 val, u32 offset)
{
	AHCI_DBG_PRINT("[0x%x] <= 0x%08x\n", TEGRA_SATA_CONFIG_BASE+offset,
					     val);
	writel(val, IO_ADDRESS(TEGRA_SATA_CONFIG_BASE + offset));
}

static inline u32 pictlr_readl(u32 offset)
{
	u32 val;

	val = readl(IO_ADDRESS(TEGRA_PRIMARY_ICTLR_BASE + offset));
	AHCI_DBG_PRINT("[0x%x] => 0x%08x\n", TEGRA_PRIMARY_ICTLR_BASE+offset,
					     val);
	return val;
}

static inline void pictlr_writel(u32 val, u32 offset)
{
	AHCI_DBG_PRINT("[0x%x] <= 0x%08x\n", TEGRA_PRIMARY_ICTLR_BASE+offset,
					     val);
	writel(val, IO_ADDRESS(TEGRA_PRIMARY_ICTLR_BASE + offset));
}

static inline u32 fuse_readl(u32 offset)
{
	u32 val;

	val = readl(IO_ADDRESS(TEGRA_FUSE_BASE + offset));
	AHCI_DBG_PRINT("[0x%x] => 0x%08x\n", TEGRA_FUSE_BASE+offset, val);

	return val;
}

/* Sata Pad Cntrl Values */
struct sata_pad_cntrl {
	u8 gen1_tx_amp;
	u8 gen1_tx_peak;
	u8 gen2_tx_amp;
	u8 gen2_tx_peak;
};

static const struct sata_pad_cntrl sata_calib_pad_val[] = {
	{	/* SATA_CALIB[1:0]  = 00 */
		0x18,
		0x04,
		0x18,
		0x0a
	},
	{	/* SATA_CALIB[1:0]  = 01 */
		0x0e,
		0x04,
		0x14,
		0x0a
	},
	{	/* SATA_CALIB[1:0]  = 10 */
		0x0e,
		0x07,
		0x1a,
		0x0e
	},
	{	/* SATA_CALIB[1:0]  = 11 */
		0x14,
		0x0e,
		0x1a,
		0x0e
	}
};

#ifdef CONFIG_TEGRA_SATA_IDLE_POWERGATE
static void tegra_ahci_work_handler(struct work_struct *work)
{
	tegra_ahci_runtime_resume(g_tegra_hpriv->dev);
}
#endif

static void tegra_ahci_set_pad_cntrl_regs(
			struct tegra_ahci_platform_data *ahci_pdata)
{
	int	calib_val;
	int	val;
	int	i;

	calib_val = fuse_readl(FUSE_SATA_CALIB_OFFSET) & FUSE_SATA_CALIB_MASK;

	val = clk_readl(CLK_RST_CONTROLLER_PLLE_SS_CNTL_0);
	val &= ~(PLLE_SSCCENTER | PLLE_SSCINVERT);
	val |= (PLLE_SSCMAX | PLLE_SSCINCINTRV | PLLE_SSCINC);
	clk_writel(val, CLK_RST_CONTROLLER_PLLE_SS_CNTL_0);

	val = clk_readl(CLK_RST_CONTROLLER_PLLE_SS_CNTL_0);
	val &= ~(PLLE_BYPASS_SS | PLLE_SSCBYP);
	clk_writel(val, CLK_RST_CONTROLLER_PLLE_SS_CNTL_0);

	udelay(2);

	val = clk_readl(CLK_RST_CONTROLLER_PLLE_SS_CNTL_0);
	val &= ~(PLLE_INTERP_RESET);
	clk_writel(val, CLK_RST_CONTROLLER_PLLE_SS_CNTL_0);

	for (i = 0; i < TEGRA_AHCI_NUM_PORTS; ++i) {
		scfg_writel((1 << i), T_SATA0_INDEX_OFFSET);

		val = scfg_readl(T_SATA0_CHX_PHY_CTRL1_GEN1_OFFSET);
		val &= ~SATA0_CHX_PHY_CTRL1_GEN1_TX_AMP_MASK;
		val |= (sata_calib_pad_val[calib_val].gen1_tx_amp <<
			SATA0_CHX_PHY_CTRL1_GEN1_TX_AMP_SHIFT);
		scfg_writel(val, T_SATA0_CHX_PHY_CTRL1_GEN1_OFFSET);

		val = scfg_readl(T_SATA0_CHX_PHY_CTRL1_GEN1_OFFSET);
		val &= ~SATA0_CHX_PHY_CTRL1_GEN1_TX_PEAK_MASK;
		val |= (sata_calib_pad_val[calib_val].gen1_tx_peak <<
			SATA0_CHX_PHY_CTRL1_GEN1_TX_PEAK_SHIFT);
		scfg_writel(val, T_SATA0_CHX_PHY_CTRL1_GEN1_OFFSET);

		val = scfg_readl(T_SATA0_CHX_PHY_CTRL1_GEN2_OFFSET);
		val &= ~SATA0_CHX_PHY_CTRL1_GEN2_TX_AMP_MASK;
		val |= (sata_calib_pad_val[calib_val].gen2_tx_amp <<
			SATA0_CHX_PHY_CTRL1_GEN2_TX_AMP_SHIFT);
		scfg_writel(val, T_SATA0_CHX_PHY_CTRL1_GEN2_OFFSET);

		val = scfg_readl(T_SATA0_CHX_PHY_CTRL1_GEN2_OFFSET);
		val &= ~SATA0_CHX_PHY_CTRL1_GEN2_TX_PEAK_MASK;
		val |= (sata_calib_pad_val[calib_val].gen2_tx_peak <<
			SATA0_CHX_PHY_CTRL1_GEN2_TX_PEAK_SHIFT);
		scfg_writel(val, T_SATA0_CHX_PHY_CTRL1_GEN2_OFFSET);

		val = GEN2_RX_EQ;
		scfg_writel(val, SATA0_CHX_PHY_CTRL11_0);

		val = CDR_CNTL_GEN1;
		scfg_writel(val, SATA0_CHX_PHY_CTRL2_0);
	}
	scfg_writel(SATA0_NONE_SELECTED, T_SATA0_INDEX_OFFSET);
}

int tegra_ahci_get_rails(struct regulator *regulators[])
{
	struct regulator *reg;
	int i;
	int ret = 0;

	for (i = 0; i < NUM_SATA_POWER_RAILS; ++i) {
		reg = regulator_get(g_tegra_hpriv->dev, sata_power_rails[i]);
		if (IS_ERR_OR_NULL(reg)) {
			pr_err("%s: can't get regulator %s\n",
				__func__, sata_power_rails[i]);
			WARN_ON(1);
			ret = PTR_ERR(reg);
			goto exit;
		}
		regulators[i] = reg;
	}
exit:
	return ret;
}

void tegra_ahci_put_rails(struct regulator *regulators[])
{
	int i;

	for (i = 0; i < NUM_SATA_POWER_RAILS; ++i)
		regulator_put(regulators[i]);
}

int tegra_ahci_power_on_rails(struct regulator *regulators[])
{
	struct regulator *reg;
	int i;
	int ret = 0;

	for (i = 0; i < NUM_SATA_POWER_RAILS; ++i) {
		reg = regulators[i];
		ret = regulator_enable(reg);
		if (ret) {
			pr_err("%s: can't enable regulator[%d]\n",
				__func__, i);
			WARN_ON(1);
			goto exit;
		}
	}

exit:
	return ret;
}

int tegra_ahci_power_off_rails(struct regulator *regulators[])
{
	struct regulator *reg;
	int i;
	int ret = 0;

	for (i = 0; i < NUM_SATA_POWER_RAILS; ++i) {
		reg = regulators[i];
		if (!IS_ERR_OR_NULL(reg)) {
			ret = regulator_disable(reg);
			if (ret) {
				pr_err("%s: can't disable regulator[%d]\n",
					__func__, i);
				WARN_ON(1);
				goto exit;
			}
		}
	}

exit:
	return ret;
}

static void tegra_first_level_clk_gate(void)
{
	if (g_tegra_hpriv->clk_state == CLK_OFF)
		return;

	clk_disable_unprepare(g_tegra_hpriv->clk_sata);
	clk_disable_unprepare(g_tegra_hpriv->clk_sata_oob);
	clk_disable_unprepare(g_tegra_hpriv->clk_cml1);
	g_tegra_hpriv->clk_state = CLK_OFF;
}

static int tegra_first_level_clk_ungate(void)
{
	int ret = 0;

	if (g_tegra_hpriv->clk_state == CLK_ON) {
		ret = -1;
		return ret;
	}

	if (clk_prepare_enable(g_tegra_hpriv->clk_sata)) {
		pr_err("%s: unable to enable SATA clock\n", __func__);
		ret = -ENODEV;
		return ret;
	}
	if (clk_prepare_enable(g_tegra_hpriv->clk_sata_oob)) {
		pr_err("%s: unable to enable SATA_OOB clock\n", __func__);
		clk_disable_unprepare(g_tegra_hpriv->clk_sata);
		ret = -ENODEV;
		return ret;
	}
	if (clk_prepare_enable(g_tegra_hpriv->clk_cml1)) {
		pr_err("%s: unable to enable cml1 clock\n", __func__);
		clk_disable_unprepare(g_tegra_hpriv->clk_sata);
		clk_disable_unprepare(g_tegra_hpriv->clk_sata_oob);
		ret = -ENODEV;
		return ret;
	}
	g_tegra_hpriv->clk_state = CLK_ON;

	return ret;
}

static int tegra_ahci_controller_init(struct tegra_ahci_host_priv *tegra_hpriv,
		int lp0)
{
	int err;
	struct clk *clk_sata = NULL;
	struct clk *clk_sata_oob = NULL;
	struct clk *clk_sata_cold = NULL;
	struct clk *clk_pllp = NULL;
	struct clk *clk_cml1 = NULL;
	u32 val;
	u32 timeout;

	struct tegra_ahci_platform_data *ahci_pdata =
					tegra_hpriv->dev->platform_data;

	if (!lp0) {
		err = tegra_ahci_get_rails(tegra_hpriv->power_rails);
		if (err) {
			pr_err("%s: fails to get rails (%d)\n", __func__, err);
			goto exit;
		}

	}
	err = tegra_ahci_power_on_rails(tegra_hpriv->power_rails);
	if (err) {
		pr_err("%s: fails to power on rails (%d)\n", __func__, err);
		goto exit;
	}

	/* pll_p is the parent of tegra_sata and tegra_sata_oob */
	clk_pllp = clk_get_sys(NULL, "pll_p");
	if (IS_ERR_OR_NULL(clk_pllp)) {
		pr_err("%s: unable to get PLL_P clock\n", __func__);
		err = PTR_ERR(clk_pllp);
		goto exit;
	}

	clk_sata = clk_get_sys("tegra_sata", NULL);
	if (IS_ERR_OR_NULL(clk_sata)) {
		pr_err("%s: unable to get SATA clock\n", __func__);
		err = PTR_ERR(clk_sata);
		goto exit;
	}

	clk_sata_oob = clk_get_sys("tegra_sata_oob", NULL);
	if (IS_ERR_OR_NULL(clk_sata_oob)) {
		pr_err("%s: unable to get SATA OOB clock\n", __func__);
		err = PTR_ERR(clk_sata_oob);
		goto exit;
	}

	clk_sata_cold = clk_get_sys("tegra_sata_cold", NULL);
	if (IS_ERR_OR_NULL(clk_sata_cold)) {
		pr_err("%s: unable to get SATA COLD clock\n", __func__);
		err = PTR_ERR(clk_sata_cold);
		goto exit;
	}

	tegra_hpriv->clk_sata = clk_sata;
	tegra_hpriv->clk_sata_oob = clk_sata_oob;
	tegra_hpriv->clk_pllp = clk_pllp;

	tegra_periph_reset_assert(clk_sata);
	tegra_periph_reset_assert(clk_sata_oob);
	tegra_periph_reset_assert(clk_sata_cold);
	udelay(10);

	/* need to establish both clocks divisors before setting clk sources */
	clk_set_rate(clk_sata, clk_get_rate(clk_sata)/10);
	clk_set_rate(clk_sata_oob, clk_get_rate(clk_sata_oob)/10);

	/* set SATA clk and SATA_OOB clk source */
	clk_set_parent(clk_sata, clk_pllp);
	clk_set_parent(clk_sata_oob, clk_pllp);

	/* Configure SATA clocks */
	/* Core clock runs at 108MHz */
	if (clk_set_rate(clk_sata, TEGRA_SATA_CORE_CLOCK_FREQ_HZ)) {
		err = -ENODEV;
		goto exit;
	}
	/* OOB clock runs at 216MHz */
	if (clk_set_rate(clk_sata_oob, TEGRA_SATA_OOB_CLOCK_FREQ_HZ)) {
		err = -ENODEV;
		goto exit;
	}

	if (clk_prepare_enable(clk_sata)) {
		pr_err("%s: unable to enable SATA clock\n", __func__);
		err = -ENODEV;
		goto exit;
	}

	if (clk_prepare_enable(clk_sata_oob)) {
		pr_err("%s: unable to enable SATA OOB clock\n", __func__);
		err = -ENODEV;
		goto exit;
	}

	tegra_periph_reset_deassert(clk_sata);
	tegra_periph_reset_deassert(clk_sata_oob);
	tegra_periph_reset_deassert(clk_sata_cold);

	tegra_ahci_clr_clk_rst_cnt_rst_dev();

	if (ahci_pdata->pexp_gpio) {
		if (gpio_is_valid(ahci_pdata->pexp_gpio)) {
			val = gpio_request(ahci_pdata->pexp_gpio, "ahci-tegra");
			if (val) {
				pr_err("failed to allocate Port expander gpio\n");
				err = -ENODEV;
				goto exit;
			}
			gpio_direction_output(ahci_pdata->pexp_gpio, 1);
		}
	}

	val = 0x100;
	pmc_writel(val, APBDEV_PMC_REMOVE_CLAMPING_CMD_0);

	/**** Init the SATA PAD PLL ****/
	/* SATA_PADPLL_IDDQ_SWCTL=1 and SATA_PADPLL_IDDQ_OVERRIDE_VALUE=1 */
	val = pmc_readl(APB_PMC_SATA_PWRGT_0_REG);
	val |= (PADPLL_IDDQ_SWCTL_ON | PADPLL_IDDQ_OVERRIDE_VALUE_ON);
	pmc_writel(val, APB_PMC_SATA_PWRGT_0_REG);

	/* SATA_PADPLL_RESET_OVERRIDE_VALUE=1 and SATA_PADPLL_RESET_SWCTL=1 */
	val = clk_readl(CLK_RST_SATA_PLL_CFG0_REG);
	val |= (PADPLL_RESET_OVERRIDE_VALUE_ON | PADPLL_RESET_SWCTL_ON);
	clk_writel(val, CLK_RST_SATA_PLL_CFG0_REG);

	/* SATA_PADPHY_IDDQ_OVERRIDE_VALUE and SATA_PADPHY_IDDQ_SWCTL = 1 */
	val = pmc_readl(APB_PMC_SATA_PWRGT_0_REG);
	val |= (PADPHY_IDDQ_OVERRIDE_VALUE_ON | PADPHY_IDDQ_SWCTL_ON |
				PLLE_IDDQ_SWCTL_ON | PLLE_SATA_SEQ_ENABLE);
	pmc_writel(val, APB_PMC_SATA_PWRGT_0_REG);

	/* Get SATA pad PLL out of IDDQ mode */
	val = pmc_readl(APB_PMC_SATA_PWRGT_0_REG);
	val &= ~PADPLL_IDDQ_OVERRIDE_VALUE_MASK;
	pmc_writel(val, APB_PMC_SATA_PWRGT_0_REG);
	udelay(3);

	/* select internal CML ref clk
	 * select PLLE as input to IO phy */
	val = misc_readl(SATA_AUX_PAD_PLL_CNTL_1_REG);
	val &= ~REFCLK_SEL_MASK;
	val |= REFCLK_SEL_INT_CML;
	misc_writel(val, SATA_AUX_PAD_PLL_CNTL_1_REG);

	/* wait for SATA_PADPLL_IDDQ2LANE_SLUMBER_DLY = 3 microseconds. */
	val = clk_readl(CLK_RST_SATA_PLL_CFG1_REG);
	val &= ~IDDQ2LANE_SLUMBER_DLY_MASK;
	val |= IDDQ2LANE_SLUMBER_DLY_3MS;
	clk_writel(val, CLK_RST_SATA_PLL_CFG1_REG);
	udelay(3);

	/* de-assert IDDQ mode signal going to PHY */
	val = pmc_readl(APB_PMC_SATA_PWRGT_0_REG);
	val &= ~PADPHY_IDDQ_OVERRIDE_VALUE_MASK;
	pmc_writel(val, APB_PMC_SATA_PWRGT_0_REG);
#if defined(CONFIG_TEGRA_SILICON_PLATFORM)
	err = tegra_unpowergate_partition_with_clk_on(TEGRA_POWERGATE_SATA);
	if (err) {
		pr_err("%s: ** failed to turn-on SATA (0x%x) **\n",
				__func__, err);
		goto exit;
	}
#endif

	/*
	 * place SATA Pad PLL out of reset by writing
	 * SATA_PADPLL_RST_OVERRIDE_VALUE = 0
	 */
	val = clk_readl(CLK_RST_SATA_PLL_CFG0_REG);
	val &= ~PADPLL_RESET_OVERRIDE_VALUE_MASK;
	clk_writel(val, CLK_RST_SATA_PLL_CFG0_REG);

	/*
	 * Wait for SATA_AUX_PAD_PLL_CNTL_1_0_LOCKDET to turn 1 with a timeout
	 * of 15 us.
	 */
	timeout = 15;
	while (timeout--) {
		udelay(1);
		val = xusb_readl(XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0);
		if (val & LOCKDET_FIELD)
			break;
	}
	if (timeout == 0)
		pr_err("%s: AUX_PAD_PLL_CNTL_1 (0x%x) is not locked in 15us.\n",
			__func__, val);

	tegra_ahci_pad_config();

	clk_cml1 = clk_get_sys(NULL, "cml1");
	if (IS_ERR_OR_NULL(clk_cml1)) {
		pr_err("%s: unable to get cml1 clock Errone is %d\n",
					__func__, (int) PTR_ERR(clk_cml1));
		err = PTR_ERR(clk_cml1);
		goto exit;
	}
	tegra_hpriv->clk_cml1 = clk_cml1;
	if (clk_prepare_enable(clk_cml1)) {
		pr_err("%s: unable to enable cml1 clock\n", __func__);
		err = -ENODEV;
		goto exit;
	}

	g_tegra_hpriv->clk_state = CLK_ON;

	val = clk_readl(CLK_RST_SATA_PLL_CFG0_REG);
	val &= ~PADPLL_RESET_SWCTL_MASK;
	clk_writel(val, CLK_RST_SATA_PLL_CFG0_REG);

	/* PLLE Programing for SATA */

	val = clk_readl(CLK_RST_CONTROLLER_PLLE_AUX_0);
	val |= CLK_RST_CONTROLLER_PLLE_AUX_0_MASK;
	clk_writel(val, CLK_RST_CONTROLLER_PLLE_AUX_0);

	/* bring SATA IOPHY out of IDDQ */
	val = xusb_readl(XUSB_PADCTL_USB3_PAD_MUX_0);
	val |= FORCE_SATA_PAD_IDDQ_DISABLE_MASK0;
	xusb_writel(val, XUSB_PADCTL_USB3_PAD_MUX_0);

	val = xusb_readl(XUSB_PADCTL_ELPG_PROGRAM_0);
	val &= ~(AUX_ELPG_CLAMP_EN | AUX_ELPG_CLAMP_EN_EARLY |
		AUX_ELPG_VCORE_DOWN);
	xusb_writel(val, XUSB_PADCTL_ELPG_PROGRAM_0);

	val = xusb_readl(XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0);
	val = val | XUSB_PADCTL_PLL1_MODE;
	xusb_writel(val, XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0);

	/* clear NVA2SATA_OOB_ON_POR in SATA_AUX_MISC_CNTL_1_REG */
	val = misc_readl(SATA_AUX_MISC_CNTL_1_REG);
	val &= ~NVA2SATA_OOB_ON_POR_MASK;
	misc_writel(val, SATA_AUX_MISC_CNTL_1_REG);

	val = sata_readl(SATA_CONFIGURATION_0_OFFSET);
	val |= EN_FPCI;
	sata_writel(val, SATA_CONFIGURATION_0_OFFSET);

	val = sata_readl(SATA_CONFIGURATION_0_OFFSET);
	val |= CLK_OVERRIDE;
	sata_writel(val, SATA_CONFIGURATION_0_OFFSET);

	/* program sata pad control based on the fuse */
	tegra_ahci_set_pad_cntrl_regs(ahci_pdata);

	/*
	 * clear bit T_SATA0_CFG_PHY_0_USE_7BIT_ALIGN_DET_FOR_SPD of
	 * T_SATA0_CFG_PHY_0
	 */
	val = scfg_readl(T_SATA0_CFG_PHY_REG);
	val &= ~PHY_USE_7BIT_ALIGN_DET_FOR_SPD_MASK;
	val |= T_SATA0_CFG_PHY_SQUELCH_MASK;
	scfg_writel(val, T_SATA0_CFG_PHY_REG);

	val = scfg_readl(T_SATA0_NVOOB);
	val |= (1 << T_SATA0_NVOOB_SQUELCH_FILTER_MODE_SHIFT);
	val |= (3 << T_SATA0_NVOOB_SQUELCH_FILTER_LENGTH_SHIFT);
	scfg_writel(val, T_SATA0_NVOOB);

	/*
	 * WAR: Before enabling SATA PLL shutdown, lockdet needs to be ignored.
	 *      To ignore lockdet, T_SATA0_DBG0_OFFSET register bit 10 needs to
	 *      be 1, and bit 8 needs to be 0.
	 */
	val = scfg_readl(T_SATA0_DBG0_OFFSET);
	val |= (1 << 10);
	val &= ~(1 << 8);
	scfg_writel(val, T_SATA0_DBG0_OFFSET);

	/* program class code and programming interface for AHCI */
	val = scfg_readl(TEGRA_PRIVATE_AHCI_CC_BKDR_OVERRIDE);
	val |= TEGRA_PRIVATE_AHCI_CC_BKDR_OVERRIDE_EN;
	scfg_writel(val, TEGRA_PRIVATE_AHCI_CC_BKDR_OVERRIDE);
	scfg_writel(TEGRA_PRIVATE_AHCI_CC_BKDR_PGM, TEGRA_PRIVATE_AHCI_CC_BKDR);
	val &= ~TEGRA_PRIVATE_AHCI_CC_BKDR_OVERRIDE_EN;
	scfg_writel(val, TEGRA_PRIVATE_AHCI_CC_BKDR_OVERRIDE);

	/* Program config space registers: */

	/* Enable BUS_MASTER+MEM+IO space, and SERR */
	val = scfg_readl(TEGRA_SATA_IO_SPACE_OFFSET);
	val |= TEGRA_SATA_ENABLE_IO_SPACE | TEGRA_SATA_ENABLE_MEM_SPACE |
	       TEGRA_SATA_ENABLE_BUS_MASTER | TEGRA_SATA_ENABLE_SERR;
	scfg_writel(val, TEGRA_SATA_IO_SPACE_OFFSET);

	/* program bar5 space, by first writing 1's to bar5 register */
	scfg_writel(TEGRA_SATA_BAR5_INIT_PROGRAM, AHCI_BAR5_CONFIG_LOCATION);
	/* flush */
	val = scfg_readl(AHCI_BAR5_CONFIG_LOCATION);

	/* then, write the BAR5_FINAL_PROGRAM address */
	scfg_writel(TEGRA_SATA_BAR5_FINAL_PROGRAM, AHCI_BAR5_CONFIG_LOCATION);
	/* flush */
	scfg_readl(AHCI_BAR5_CONFIG_LOCATION);

	sata_writel((FPCI_BAR5_0_FINAL_VALUE >> 8),
			SATA_FPCI_BAR5_0_OFFSET);

	val = scfg_readl(T_SATA0_AHCI_HBA_CAP_BKDR);
	val |= (HOST_CAP_ALPM | HOST_CAP_SSC | HOST_CAP_PART);
	scfg_writel(val, T_SATA0_AHCI_HBA_CAP_BKDR);

	/* Second Level Clock Gating*/
	val = bar5_readl(AHCI_HBA_PLL_CTRL_0);
	val |= (CLAMP_TXCLK_ON_SLUMBER | CLAMP_TXCLK_ON_DEVSLP);
	val &= ~NO_CLAMP_SHUT_DOWN;
	bar5_writel(val, AHCI_HBA_PLL_CTRL_0);

	/* enable Interrupt channel */
	val = pictlr_readl(PRI_ICTLR_CPU_IER_SET_0_OFFSET);
	val |= CPU_IER_SATA_CTL;
	pictlr_writel(val, PRI_ICTLR_CPU_IER_SET_0_OFFSET);

	/* set IP_INT_MASK */
	val = sata_readl(SATA_INTR_MASK_0_OFFSET);
	val |= IP_INT_MASK;
	sata_writel(val, SATA_INTR_MASK_0_OFFSET);

exit:
	if (!IS_ERR_OR_NULL(clk_pllp))
		clk_put(clk_pllp);
	if (!IS_ERR_OR_NULL(clk_sata))
		clk_put(clk_sata);
	if (!IS_ERR_OR_NULL(clk_sata_oob))
		clk_put(clk_sata_oob);
	if (!IS_ERR_OR_NULL(clk_sata_cold))
		clk_put(clk_sata_cold);
	if (!IS_ERR_OR_NULL(clk_cml1))
		clk_put(clk_cml1);

	if (err) {
		/* turn off all SATA power rails; ignore returned status */
		tegra_ahci_power_off_rails(tegra_hpriv->power_rails);
		/* return regulators to system */
		tegra_ahci_put_rails(tegra_hpriv->power_rails);
	}
	return err;
}

static void tegra_ahci_save_initial_config(struct platform_device *pdev,
					   struct ahci_host_priv *hpriv)
{
	ahci_save_initial_config(&pdev->dev, hpriv, 0, 0);
}

static void tegra_ahci_controller_remove(struct platform_device *pdev)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	struct tegra_ahci_host_priv *tegra_hpriv;
	int status;

	tegra_hpriv = (struct tegra_ahci_host_priv *)host->private_data;

#ifdef CONFIG_PM
	/* call tegra_ahci_controller_suspend() to power-down the SATA */
	status = tegra_ahci_controller_suspend(pdev);
	if (status)
		dev_err(host->dev, "remove: error suspend SATA (0x%x)\n",
				   status);
#else
	/* power off the sata */
	status = tegra_powergate_partition_with_clk_off(TEGRA_POWERGATE_SATA);
	if (status)
		dev_err(host->dev, "remove: error turn-off SATA (0x%x)\n",
				   status);
	tegra_ahci_power_off_rails(tegra_hpriv->power_rails);
#endif

	/* return system resources */
	tegra_ahci_put_rails(tegra_hpriv->power_rails);
}

#ifdef CONFIG_PM
static int tegra_ahci_controller_suspend(struct platform_device *pdev)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	struct tegra_ahci_host_priv *tegra_hpriv;
	struct tegra_ahci_platform_data *ahci_pdata;
	unsigned long flags;

	tegra_hpriv = (struct tegra_ahci_host_priv *)host->private_data;
	ahci_pdata = tegra_hpriv->dev->platform_data;

	/* stop the idle timer */
	if (timer_pending(&tegra_hpriv->idle_timer))
		del_timer_sync(&tegra_hpriv->idle_timer);

	spin_lock_irqsave(&host->lock, flags);
	if (tegra_hpriv->pg_state == SATA_OFF)
		dev_dbg(host->dev, "suspend: SATA already power gated\n");
	else {
		bool pg_ok;

		dev_dbg(host->dev, "suspend: power gating SATA...\n");
		pg_ok = tegra_ahci_power_gate(host);
		if (pg_ok) {
			tegra_hpriv->pg_state = SATA_OFF;
			dev_dbg(host->dev, "suspend: SATA is power gated\n");
		} else {
			tegra_ahci_abort_power_gate(host);
			spin_unlock_irqrestore(&host->lock, flags);
			return -EBUSY;
		}
	}

	if (ahci_pdata->pexp_gpio)
		gpio_free(ahci_pdata->pexp_gpio);
	spin_unlock_irqrestore(&host->lock, flags);

	tegra_first_level_clk_gate();

	return tegra_ahci_power_off_rails(tegra_hpriv->power_rails);
}

static int tegra_ahci_controller_resume(struct platform_device *pdev)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	struct tegra_ahci_host_priv *tegra_hpriv;
	unsigned long flags;
	int err;

	tegra_hpriv = (struct tegra_ahci_host_priv *)host->private_data;

	err = tegra_ahci_power_on_rails(tegra_hpriv->power_rails);
	if (err) {
		pr_err("%s: fails to power on rails (%d)\n", __func__, err);
		return err;
	}

	err = tegra_first_level_clk_ungate();
	if (err < 0) {
		pr_err("%s: flcg ungate failed\n", __func__);
		return err;
	}

	spin_lock_irqsave(&host->lock, flags);
	if (tegra_hpriv->pg_state == SATA_ON) {
		dev_dbg(host->dev, "resume: SATA already powered on\n");
	} else {
		dev_dbg(host->dev, "resume: powering on SATA...\n");
		tegra_ahci_power_un_gate(host);
		tegra_hpriv->pg_state = SATA_ON;
	}
	spin_unlock_irqrestore(&host->lock, flags);
	tegra_first_level_clk_gate();

	return 0;
}

static int tegra_ahci_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	void __iomem *mmio = host->iomap[AHCI_PCI_BAR];
	u32 ctl;
	int rc;

	dev_dbg(host->dev, "** entering %s: **\n", __func__);
	if (mesg.event & PM_EVENT_SLEEP) {
		/*
		 * AHCI spec rev1.1 section 8.3.3:
		 * Software must disable interrupts prior to requesting a
		 * transition of the HBA to D3 state.
		 */
		ctl = readl(mmio + HOST_CTL);
		ctl &= ~HOST_IRQ_EN;
		writel(ctl, mmio + HOST_CTL);
		readl(mmio + HOST_CTL); /* flush */
	}

	rc = ata_host_suspend(host, mesg);
	if (rc)
		return rc;

	return tegra_ahci_controller_suspend(pdev);
}

static int tegra_ahci_resume(struct platform_device *pdev)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	int rc;
	u32 val;

	dev_dbg(host->dev, "** entering %s: **\n", __func__);
	rc = tegra_ahci_controller_resume(pdev);
	if (rc != 0)
		return rc;

	rc = tegra_ahci_controller_init(g_tegra_hpriv, 1);
	if (rc != 0) {
		dev_err(host->dev, "TEGRA SATA init failed in resume\n");
		return rc;
	}

	if (pdev->dev.power.power_state.event == PM_EVENT_SUSPEND) {
		rc = ahci_reset_controller(host);
		if (rc)
			return rc;

		val = misc_readl(SATA_AUX_RX_STAT_INT_0);
		if (val && SATA_RX_STAT_INT_DISABLE) {
			val &= ~SATA_RX_STAT_INT_DISABLE;
			misc_writel(val, SATA_AUX_RX_STAT_INT_0);
		}

		ahci_init_controller(host);
	}

	ata_host_resume(host);

	return 0;
}

#ifdef CONFIG_TEGRA_SATA_IDLE_POWERGATE
static int tegra_ahci_runtime_suspend(struct device *dev)
{
	struct ata_host *host;
	struct tegra_ahci_host_priv *tegra_hpriv;
	bool pg_ok;
	unsigned long flags;
	int err = 0;

	host = dev_get_drvdata(dev);
	tegra_hpriv = (struct tegra_ahci_host_priv *)host->private_data;

	spin_lock_irqsave(&host->lock, flags);

	switch (tegra_hpriv->pg_state) {
	case SATA_OFF:
		dev_dbg(dev, "** rt-suspend: already power gated **\n");
		break;

	case SATA_ABORT_OFF:
		dev_dbg(dev, "** rt-suspend: abort suspend **\n");
		tegra_hpriv->pg_state = SATA_ON;
		tegra_ahci_dequeue_qcs(tegra_hpriv);
		err = -EBUSY;
		break;

	case SATA_ON:
	case SATA_GOING_OFF:
		if (tegra_ahci_are_all_ports_idle(host)) {
			/* if all ports are in idle, do power-gate */
			dev_dbg(dev, "** rt-suspend: power-down sata (%u) **\n",
					tegra_hpriv->pg_state);
#ifdef TEGRA_AHCI_CONTEXT_RESTORE
			pg_ok = tegra_ahci_power_gate(host);
#else
			pg_ok = tegra_ahci_pad_suspend(host);
#endif
			dev_dbg(dev, "** rt-suspend: done **\n");
			if (pg_ok) {
				tegra_hpriv->pg_state = SATA_OFF;
			} else {
				dev_err(dev, "** rt-suspend: abort pg **\n");
#ifdef TEGRA_AHCI_CONTEXT_RESTORE
				tegra_ahci_abort_power_gate(host);
#else
				tegra_ahci_abort_pad_suspend(host);
#endif
				tegra_hpriv->pg_state = SATA_ON;
				err = -EBUSY;
			}
		} else {
			dev_dbg(dev, "** rt-suspend: port not idle (%u) **\n",
					tegra_hpriv->pg_state);
			err = -EBUSY;
		}
		break;

	case SATA_GOING_ON:
	default:
		dev_err(dev, "** rt-suspend: bad state (%u) **\n",
			tegra_hpriv->pg_state);
		WARN_ON(1);
		err = -EBUSY;
		break;

	}

	spin_unlock_irqrestore(&host->lock, flags);

	return err;
}

static int tegra_ahci_runtime_resume(struct device *dev)
{
	struct ata_host *host;
	struct tegra_ahci_host_priv *tegra_hpriv;
	int err = 0;

	host = dev_get_drvdata(dev);
	tegra_hpriv = (struct tegra_ahci_host_priv *)host->private_data;

	if (tegra_hpriv->pg_state == SATA_ON) {
		dev_dbg(dev, "** rt-resume: already power ungated **\n");
		goto exit;
	}

	if ((tegra_hpriv->pg_state == SATA_OFF) ||
	    (tegra_hpriv->pg_state == SATA_GOING_ON)) {
		dev_dbg(dev, "** rt-resume: power-up sata (%u) **\n",
				tegra_hpriv->pg_state);
#ifdef TEGRA_AHCI_CONTEXT_RESTORE
		tegra_ahci_power_un_gate(host);
#else
		tegra_ahci_pad_resume(host);
#endif
		dev_dbg(dev, "** rt-resume: done **\n");
		tegra_hpriv->pg_state = SATA_ON;

		/* now qc_issue all qcs in the qc_list */
		tegra_ahci_dequeue_qcs(tegra_hpriv);

	} else {
		dev_err(dev, "** rt-resume: bad state (%u) **\n",
				tegra_hpriv->pg_state);
		WARN_ON(1);
		err = -EBUSY;
	}


exit:

	return err;
}

#endif

#ifdef TEGRA_AHCI_CONTEXT_RESTORE
static u16 pg_save_bar5_registers[] = {
	0x018,	/* T_AHCI_HBA_CCC_PORTS */
	0x004,	/* T_AHCI_HBA_GHC */
	0x014,	/* T_AHCI_HBA_CCC_CTL - OP (optional) */
	0x01C,	/* T_AHCI_HBA_EM_LOC */
	0x020	/* T_AHCI_HBA_EM_CTL - OP */
};

static u16 pg_save_bar5_port_registers[] = {
	0x100,	/* T_AHCI_PORT_PXCLB */
	0x104,	/* T_AHCI_PORT_PXCLBU */
	0x108,	/* T_AHCI_PORT_PXFB */
	0x10C,	/* T_AHCI_PORT_PXFBU */
	0x114,	/* T_AHCI_PORT_PXIE */
	0x118,	/* T_AHCI_PORT_PXCMD */
	0x12C,	/* T_AHCI_PORT_PXSCTL */
	0x144	/* T_AHCI_PORT_PXDEVSLP */
};

/*
 * pg_save_bar5_bkdr_registers:
 *    These registers in BAR5 are read only.
 * To restore back those register values, write the saved value
 *    to the registers specified in pg_restore_bar5_bkdr_registers[].
 *    These pg_restore_bar5_bkdr_registers[] are in SATA_CONFIG space.
 */
static u16 pg_save_bar5_bkdr_registers[] = {
	/* Save and restore via bkdr writes */
	0x000,	/* T_AHCI_HBA_CAP */
	0x00C,	/* T_AHCI_HBA_PI */
	0x024	/* T_AHCI_HBA_CAP2 */
};

static u16 pg_restore_bar5_bkdr_registers[] = {
	/* Save and restore via bkdr writes */
	0x300,	/* BKDR of T_AHCI_HBA_CAP */
	0x33c,	/* BKDR of T_AHCI_HBA_PI */
	0x330	/* BKDR of T_AHCI_HBA_CAP2 */
};

/* These registers are saved for each port */
static u16 pg_save_bar5_bkdr_port_registers[] = {
	0x120,	/* NV_PROJ__SATA0_CHX_AHCI_PORT_PXTFD  */
	0x124,	/* NV_PROJ__SATA0_CHX_AHCI_PORT_PXSIG */
	0x128	/* NV_PROJ__SATA0_CHX_AHCI_PORT_PXSSTS */
};

static u16 pg_restore_bar5_bkdr_port_registers[] = {
	/* Save and restore via bkdr writes */
	0x790,	/* BKDR of NV_PROJ__SATA0_CHX_AHCI_PORT_PXTFD  */
	0x794,	/* BKDR of NV_PROJ__SATA0_CHX_AHCI_PORT_PXSIG */
	0x798	/* BKDR of NV_PROJ__SATA0_CHX_AHCI_PORT_PXSSTS */
};

static u16 pg_save_config_registers[] = {
	0x004,	/* T_SATA0_CFG_1 */
	0x00C,	/* T_SATA0_CFG_3 */
	0x024,	/* T_SATA0_CFG_9 */
	0x028,	/* T_SATA0_CFG_10 */
	0x030,	/* T_SATA0_CFG_12 */
	0x034,	/* T_SATA0_CFG_13 */
	0x038,	/* T_SATA0_CFG_14 */
	0x03C,	/* T_SATA0_CFG_15 */
	0x040,	/* T_SATA0_CFG_16 */
	0x044,	/* T_SATA0_CFG_17 */
	0x048,	/* T_SATA0_CFG_18 */
	0x0B0,	/* T_SATA0_MSI_CTRL */
	0x0B4,	/* T_SATA0_MSI_ADDR1 */
	0x0B8,	/* T_SATA0_MSI_ADDR2 */
	0x0BC,	/* T_SATA0_MSI_DATA */
	0x0C0,	/* T_SATA0_MSI_QUEUE */
	0x0EC,	/* T_SATA0_MSI_MAP */
	0x124,	/* T_SATA0_CFG_PHY_POWER */
	0x128,	/* T_SATA0_CFG_PHY_POWER_1 */
	0x12C,	/* T_SATA0_CFG_PHY_1 */
	0x174,	/* T_SATA0_CFG_LINK_0 */
	0x178,	/* T_SATA0_CFG_LINK_1 */
	0x1D0,	/* MCP_SATA0_CFG_TRANS_0 */
	0x238,	/* T_SATA0_ALPM_CTRL */
	0x30C,	/* T_SATA0_AHCI_HBA_CYA_0 */
	0x320,	/* T_SATA0_AHCI_HBA_SPARE_1 */
	0x324,	/* T_SATA0_AHCI_HBA_SPARE_2 */
	0x328,	/* T_SATA0_AHCI_HBA_DYN_CLK_CLAMP */
	0x32C,	/* T_SATA0_AHCI_CFG_ERR_CTRL */
	0x338,	/* T_SATA0_AHCI_HBA_CYA_1 */
	0x340,	/* T_SATA0_AHCI_HBA_PRE_STAGING_CONTROL */
	0x430,	/* T_SATA0_CFG_FPCI_0 */
	0x494,	/* T_SATA0_CFG_ESATA_CTRL */
	0x4A0,	/* T_SATA0_CYA1 */
	0x4B0,	/* T_SATA0_CFG_GLUE */
	0x534,	/* T_SATA0_PHY_CTRL */
	0x540,	/* T_SATA0_CTRL */
	0x550,	/* T_SATA0_DBG0 */
	0x554	/* T_SATA0_LOW_POWER_COUNT */
};

static u16 pg_save_config_port_registers[] = {
	/* Save and restore per port */
	/* need to have port selected */
	0x530,	/* T_SATA0_CHXCFG1 */
	0x684,	/* T_SATA0_CHX_MISC */
	0x700,	/* T_SATA0_CHXCFG3 */
	0x704,	/* T_SATA0_CHXCFG4_CHX */
	0x690,	/* T_SATA0_CHX_PHY_CTRL1_GEN1 */
	0x694,	/* T_SATA0_CHX_PHY_CTRL1_GEN2 */
	0x698,	/* T_SATA0_CHX_PHY_CTRL1_GEN3 */
	0x69C,	/* T_SATA0_CHX_PHY_CTRL_2 */
	0x6B0,	/* T_SATA0_CHX_PHY_CTRL_3 */
	0x6B4,	/* T_SATA0_CHX_PHY_CTRL_4 */
	0x6B8,	/* T_SATA0_CHX_PHY_CTRL_5 */
	0x6BC,	/* T_SATA0_CHX_PHY_CTRL_6 */
	0x714,	/* T_SATA0_PRBS_CHX - OP */
	0x750,	/* T_SATA0_CHX_LINK0 */
	0x7F0	/* T_SATA0_CHX_GLUE */
};

static u16 pg_save_ipfs_registers[] = {
	0x094,	/* SATA_FPCI_BAR5_0 */
	0x0C0,	/* SATA_MSI_BAR_SZ_0 */
	0x0C4,	/* SATA_MSI_AXI_BAR_ST_0 */
	0x0C8,	/* SATA_MSI_FPCI_BAR_ST_0 */
	0x140,	/* SATA_MSI_EN_VEC0_0 */
	0x144,	/* SATA_MSI_EN_VEC1_0 */
	0x148,	/* SATA_MSI_EN_VEC2_0 */
	0x14C,	/* SATA_MSI_EN_VEC3_0 */
	0x150,	/* SATA_MSI_EN_VEC4_0 */
	0x154,	/* SATA_MSI_EN_VEC5_0 */
	0x158,	/* SATA_MSI_EN_VEC6_0 */
	0x15C,	/* SATA_MSI_EN_VEC7_0 */
	0x180,	/* SATA_CONFIGURATION_0 */
	0x184,	/* SATA_FPCI_ERROR_MASKS_0 */
	0x188,	/* SATA_INTR_MASK_0 */
	0x1A0,	/* SATA_CFG_REVID_0 */
	0x198,	/* SATA_IPFS_INTR_ENABLE_0 */
	0x1BC,	/* SATA_CLKGATE_HYSTERSIS_0 */
	0x1DC	/* SATA_SATA_MCCIF_FIFOCTRL_0 */
};

static void tegra_ahci_save_regs(u32 **save_addr,
				 u32 reg_base,
				 u16 reg_array[],
				 u32 regs)
{
	u32 i;
	u32 *dest = *save_addr;
	void __iomem *base = IO_ADDRESS(reg_base);

	for (i = 0; i < regs; ++i, ++dest) {
		*dest = readl(base + (u32)reg_array[i]);
		AHCI_DBG_PRINT("save: [0x%x]=0x%08x\n",
			       (reg_base+(u32)reg_array[i]), *dest);
	}
	*save_addr = dest;
}

static void tegra_ahci_restore_regs(void **save_addr,
				    u32 reg_base,
				    u16 reg_array[],
				    u32 regs)
{
	u32 i;
	u32 *src = *save_addr;
	void __iomem *base = IO_ADDRESS(reg_base);

	for (i = 0; i < regs; ++i, ++src) {
		writel(*src, base + (u32)reg_array[i]);
		AHCI_DBG_PRINT("restore: [0x%x]=0x%08x\n",
				(reg_base+(u32)reg_array[i]), *src);
	}
	*save_addr = src;
}

static void tegra_ahci_pg_save_registers(struct ata_host *host)
{
	struct tegra_ahci_host_priv *tegra_hpriv;
	u32 *pg_save;
	u32 regs;
	int i;

	tegra_hpriv = (struct tegra_ahci_host_priv *)host->private_data;
	pg_save = tegra_hpriv->pg_save;

	/*
	 * Driver should save/restore the registers in the order of
	 * IPFS, CFG, Ext CFG, BAR5.
	 */

	/* save IPFS registers */
	regs = ARRAY_SIZE(pg_save_ipfs_registers);
	tegra_ahci_save_regs(&pg_save, TEGRA_SATA_BASE,
			     pg_save_ipfs_registers, regs);
	/* after the call, pg_save should point to the next address to save */

	/* save CONFIG registers */
	regs = ARRAY_SIZE(pg_save_config_registers);
	tegra_ahci_save_regs(&pg_save, TEGRA_SATA_CONFIG_BASE,
			     pg_save_config_registers, regs);

	/* save CONFIG per port registers */
	for (i = 0; i < TEGRA_AHCI_NUM_PORTS; ++i) {
		scfg_writel((1 << i), T_SATA0_INDEX_OFFSET);
		regs = ARRAY_SIZE(pg_save_config_port_registers);
		tegra_ahci_save_regs(&pg_save, TEGRA_SATA_CONFIG_BASE,
				     pg_save_config_port_registers, regs);
	}
	scfg_writel(SATA0_NONE_SELECTED, T_SATA0_INDEX_OFFSET);

	/* save BAR5 registers */
	regs = ARRAY_SIZE(pg_save_bar5_registers);
	tegra_ahci_save_regs(&pg_save, TEGRA_SATA_BAR5_BASE,
			     pg_save_bar5_registers, regs);

	/* save BAR5 port_registers */
	regs = ARRAY_SIZE(pg_save_bar5_port_registers);
	for (i = 0; i < TEGRA_AHCI_NUM_PORTS; ++i)
		tegra_ahci_save_regs(&pg_save, TEGRA_SATA_BAR5_BASE + (0x80*i),
				     pg_save_bar5_port_registers, regs);

	/* save bkdr registers */
	regs = ARRAY_SIZE(pg_save_bar5_bkdr_registers);
	tegra_ahci_save_regs(&pg_save, TEGRA_SATA_BAR5_BASE,
			     pg_save_bar5_bkdr_registers, regs);

	/* and save bkdr per_port registers */
	for (i = 0; i < TEGRA_AHCI_NUM_PORTS; ++i) {
		scfg_writel((1 << i), T_SATA0_INDEX_OFFSET);
		regs = ARRAY_SIZE(pg_save_bar5_bkdr_port_registers);
		tegra_ahci_save_regs(&pg_save, TEGRA_SATA_BAR5_BASE + (0x80*i),
				     pg_save_bar5_bkdr_port_registers,
				     regs);
	}
	scfg_writel(SATA0_NONE_SELECTED, T_SATA0_INDEX_OFFSET);
}

static void tegra_ahci_pg_restore_registers(struct ata_host *host)
{
	struct tegra_ahci_host_priv *tegra_hpriv;
	void *pg_save;
	u32 regs, val;
	int i;

	tegra_hpriv = (struct tegra_ahci_host_priv *)host->private_data;
	pg_save = tegra_hpriv->pg_save;

	/*
	 * Driver should restore the registers in the order of
	 * IPFS, CFG, Ext CFG, BAR5.
	 */

	/* restore IPFS registers */
	regs = ARRAY_SIZE(pg_save_ipfs_registers);
	tegra_ahci_restore_regs(&pg_save, TEGRA_SATA_BASE,
				pg_save_ipfs_registers, regs);
	/* after the call, pg_save should point to the next addr to restore */

	/* restore CONFIG registers */
	regs = ARRAY_SIZE(pg_save_config_registers);
	tegra_ahci_restore_regs(&pg_save, TEGRA_SATA_CONFIG_BASE,
				pg_save_config_registers, regs);

	/* restore CONFIG per port registers */
	for (i = 0; i < TEGRA_AHCI_NUM_PORTS; ++i) {
		scfg_writel((1 << i), T_SATA0_INDEX_OFFSET);
		regs = ARRAY_SIZE(pg_save_config_port_registers);
		tegra_ahci_restore_regs(&pg_save, TEGRA_SATA_CONFIG_BASE,
					pg_save_config_port_registers,
					regs);
	}
	scfg_writel(SATA0_NONE_SELECTED, T_SATA0_INDEX_OFFSET);

	/* restore BAR5 registers */
	regs = ARRAY_SIZE(pg_save_bar5_registers);
	tegra_ahci_restore_regs(&pg_save, TEGRA_SATA_BAR5_BASE,
				pg_save_bar5_registers, regs);

	/* restore BAR5 port_registers */
	regs = ARRAY_SIZE(pg_save_bar5_port_registers);
	for (i = 0; i < TEGRA_AHCI_NUM_PORTS; ++i)
		tegra_ahci_restore_regs(&pg_save, TEGRA_SATA_BAR5_BASE+(0x80*i),
					pg_save_bar5_port_registers, regs);

	/* restore bkdr registers */
	regs = ARRAY_SIZE(pg_restore_bar5_bkdr_registers);
	tegra_ahci_restore_regs(&pg_save, TEGRA_SATA_CONFIG_BASE,
			     pg_restore_bar5_bkdr_registers, regs);

	/* and restore BAR5 bkdr per_port registers */
	for (i = 0; i < TEGRA_AHCI_NUM_PORTS; ++i) {
		scfg_writel((1 << i), T_SATA0_INDEX_OFFSET);
		regs = ARRAY_SIZE(pg_restore_bar5_bkdr_port_registers);
		tegra_ahci_restore_regs(&pg_save, TEGRA_SATA_CONFIG_BASE,
					pg_restore_bar5_bkdr_port_registers,
					regs);
	}
	scfg_writel(SATA0_NONE_SELECTED, T_SATA0_INDEX_OFFSET);

	/* program class code and programming interface for AHCI */
	val = scfg_readl(TEGRA_PRIVATE_AHCI_CC_BKDR_OVERRIDE);
	val |= TEGRA_PRIVATE_AHCI_CC_BKDR_OVERRIDE_EN;
	scfg_writel(val, TEGRA_PRIVATE_AHCI_CC_BKDR_OVERRIDE);
	scfg_writel(TEGRA_PRIVATE_AHCI_CC_BKDR_PGM, TEGRA_PRIVATE_AHCI_CC_BKDR);
	val &= ~TEGRA_PRIVATE_AHCI_CC_BKDR_OVERRIDE_EN;
	scfg_writel(val, TEGRA_PRIVATE_AHCI_CC_BKDR_OVERRIDE);
}
#endif
static u32 tegra_ahci_port_error(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);
	u32 err_status;

	err_status = readl(port_mmio + PORT_IRQ_STAT);
	/* excludes PhyRdy and Connect Change status */
	err_status &= (PORT_IRQ_ERROR & (~(PORT_IRQ_PHYRDY|PORT_IRQ_CONNECT)));
	return err_status;
}

static bool tegra_ahci_check_errors(struct ata_host *host)
{	int i;
	struct ata_port *ap;
	u32 err;

	for (i = 0; i < host->n_ports; i++) {
		ap = host->ports[i];
		err = tegra_ahci_port_error(ap);
		if (err) {
			dev_err(host->dev,
				"pg-chk-err = 0x%08x on port %d\n", err, i);
			return true;
		}
	}
	return false;
}

void tegra_ahci_iddqlane_config(void)
{
	u32 val;
	u32 dat;

	/* wait for SATA_PADPLL_IDDQ2LANE_SLUMBER_DLY = 3 microseconds. */
	val = clk_readl(CLK_RST_SATA_PLL_CFG1_REG);
	val &= ~IDDQ2LANE_SLUMBER_DLY_MASK;
	val |= IDDQ2LANE_SLUMBER_DLY_3MS;
	clk_writel(val, CLK_RST_SATA_PLL_CFG1_REG);

	/* get sata phy and pll out of iddq: */
	val = pmc_readl(APB_PMC_SATA_PWRGT_0_REG);
	val &= ~(PADPLL_IDDQ_OVERRIDE_VALUE_MASK | PADPLL_IDDQ_SWCTL_MASK);
	pmc_writel(val, APB_PMC_SATA_PWRGT_0_REG);

	/* wait for delay of IDDQ2LAND_SLUMBER_DLY */
	val = clk_readl(CLK_RST_SATA_PLL_CFG1_REG);
	dat = (val & IDDQ2LANE_SLUMBER_DLY_MASK) >> IDDQ2LANE_SLUMBER_DLY_SHIFT;
	udelay(dat);
	val = pmc_readl(APB_PMC_SATA_PWRGT_0_REG);
	val &= ~(PADPHY_IDDQ_OVERRIDE_VALUE_MASK | PADPHY_IDDQ_SWCTL_MASK);
	pmc_writel(val, APB_PMC_SATA_PWRGT_0_REG);

}
void tegra_ahci_put_sata_in_iddq()
{
	u32 val;
	u32 dat;

	/*
	 * Hw wake up is not needed:
	 * Driver/RM shall place the SATA PHY and SATA PADPLL in IDDQ.
	 * SATA_PADPLL_RESET_SWCTL =1
	 * SATA_PADPLL_RESET_OVERRIDE_VALUE=1
	 * SATA_PADPHY_IDDQ_SWCTL=1
	 * SATA_PADPHY_IDDQ_OVERRIDE_VALUE=1
	 */

	val = clk_readl(CLK_RST_SATA_PLL_CFG0_REG);
	val &= ~(PADPLL_RESET_SWCTL_MASK | PADPLL_RESET_OVERRIDE_VALUE_MASK);
	val |= (PADPLL_RESET_SWCTL_ON | PADPLL_RESET_OVERRIDE_VALUE_ON);
	clk_writel(val, CLK_RST_SATA_PLL_CFG0_REG);

	val = pmc_readl(APB_PMC_SATA_PWRGT_0_REG);
	val &= ~(PADPHY_IDDQ_OVERRIDE_VALUE_MASK | PADPHY_IDDQ_SWCTL_MASK);
	val |= (PADPHY_IDDQ_SWCTL_ON | PADPHY_IDDQ_OVERRIDE_VALUE_ON);
	pmc_writel(val, APB_PMC_SATA_PWRGT_0_REG);

	/* Wait for time specified in SATA_LANE_IDDQ2_PADPLL_IDDQ */
	val = clk_readl(CLK_RST_SATA_PLL_CFG1_REG);
	dat = (val & IDDQ2LANE_IDDQ_DLY_MASK) >> IDDQ2LANE_IDDQ_DLY_SHIFT;
	udelay(dat);

	/* SATA_PADPLL_IDDQ_SWCTL=1 & SATA_PADPLL_IDDQ_OVERRIDE_VALUE=1 */
	val = pmc_readl(APB_PMC_SATA_PWRGT_0_REG);
	val &= ~(PADPLL_IDDQ_OVERRIDE_VALUE_MASK | PADPLL_IDDQ_SWCTL_MASK);
	val |= (PADPLL_IDDQ_SWCTL_ON | PADPLL_IDDQ_OVERRIDE_VALUE_ON);
	pmc_writel(val, APB_PMC_SATA_PWRGT_0_REG);

	val = xusb_readl(XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0);
	val |= (PLL_PWR_OVRD_MASK | PLL_IDDQ_MASK | PLL_RST_MASK);
	xusb_writel(val, XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0);

}
void tegra_ahci_clr_clk_rst_cnt_rst_dev(void)
{
	u32 val;

	val = clk_readl(CLK_RST_CONTROLLER_RST_DEV_V_CLR_0);
	val |= (SWR_SATA_OOB_RST | SWR_SATA_RST);
	clk_writel(val, CLK_RST_CONTROLLER_RST_DEV_V_CLR_0);

	val = clk_readl(CLK_RST_CONTROLLER_RST_DEV_W_CLR_0);
	val |= SWR_SATACOLD_RST;
	clk_writel(val, CLK_RST_CONTROLLER_RST_DEV_W_CLR_0);

}
void tegra_ahci_set_clk_rst_cnt_rst_dev(void)
{

	u32 val;

	val = clk_readl(CLK_RST_CONTROLLER_RST_DEV_V_CLR_0);
	val &= ~(SWR_SATA_OOB_RST | SWR_SATA_RST);
	clk_writel(val, CLK_RST_CONTROLLER_RST_DEV_V_CLR_0);

	val = clk_readl(CLK_RST_CONTROLLER_RST_DEV_W_CLR_0);
	val &= ~SWR_SATACOLD_RST;
	clk_writel(val, CLK_RST_CONTROLLER_RST_DEV_W_CLR_0);

}

static void tegra_ahci_pad_config(void)
{
	u32 val;

	/* clear SW control of SATA PADPLL, SATA PHY and PLLE */

	/* for SATA PHY IDDQ */
	val = xusb_readl(XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL_1_0);
	val &= ~(IDDQ_OVRD_MASK | IDDQ_MASK);
	xusb_writel(val, XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL_1_0);

	/* for SATA PADPLL IDDQ */
	val = xusb_readl(XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0);
	val &= ~(PLL_PWR_OVRD_MASK | PLL_IDDQ_MASK | PLL_RST_MASK);
	xusb_writel(val, XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0);

	/* PLLE related stuff*/

	val = clk_readl(CLK_RST_CONTROLLER_PLLE_MISC_0);
	val &= ~(T124_PLLE_IDDQ_SWCTL_MASK | PLLE_IDDQ_OVERRIDE_VALUE_MASK);
	clk_writel(val, CLK_RST_CONTROLLER_PLLE_MISC_0);

	clk_writel(CLK_RST_CONTROLLER_PLLE_MISC_0_VALUE,
			CLK_RST_CONTROLLER_PLLE_MISC_0);

	val = clk_readl(CLK_RST_CONTROLLER_PLLE_BASE_0);
	val |= PLLE_ENABLE;
	clk_writel(val, CLK_RST_CONTROLLER_PLLE_BASE_0);

}

static void tegra_ahci_abort_power_gate(struct ata_host *host)
{
	u32 val;
	val = pmc_readl(APB_PMC_SATA_PWRGT_0_REG);
	val &= ~PG_INFO_MASK;
	pmc_writel(val, APB_PMC_SATA_PWRGT_0_REG);
}

static bool tegra_ahci_power_gate(struct ata_host *host)
{
	u32 val;
	u32 dat;
	struct tegra_ahci_host_priv *tegra_hpriv;
	int status;

	tegra_hpriv = (struct tegra_ahci_host_priv *)host->private_data;

	val = pmc_readl(APB_PMC_SATA_PWRGT_0_REG);
	val &= ~PG_INFO_MASK;
	val |= PG_INFO_ON;
	pmc_writel(val, APB_PMC_SATA_PWRGT_0_REG);

#ifdef TEGRA_AHCI_CONTEXT_RESTORE
	tegra_ahci_pg_save_registers(host);
#endif
	val = xusb_readl(XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL_1_0);
	val |= XUSB_PADCTL_IOPHY_MISC_IDDQ |
		XUSB_PADCTL_IOPHY_MISC_IDDQ_OVRD;
	xusb_writel(val, XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL_1_0);

	/*
	 * Read SATA_AUX_MISC_CNTL_1_0 register L0_RX_IDLE_T_SAX field and
	 * write that value into same register L0_RX_IDLE_T_NPG field.
	 * And write 1 to L0_RX_IDLE_T_MUX field.
	 */
	val = misc_readl(SATA_AUX_MISC_CNTL_1_REG);
	dat = val;
	dat &= L0_RX_IDLE_T_SAX_MASK;
	dat >>= L0_RX_IDLE_T_SAX_SHIFT;
	dat <<= L0_RX_IDLE_T_NPG_SHIFT;
	val &= ~L0_RX_IDLE_T_NPG_MASK;
	val |= dat;
	val |= L0_RX_IDLE_T_MUX_FROM_APB_MISC;
	val |= DEVSLP_OVERRIDE;
	misc_writel(val, SATA_AUX_MISC_CNTL_1_REG);

	tegra_ahci_set_clk_rst_cnt_rst_dev();

	/* abort PG if there are errors occurred */
	if (tegra_ahci_check_errors(host)) {
		dev_err(host->dev, "** pg: errors; abort power gating **\n");
		return false;
	}
	/* make sure all ports have no outstanding commands and are idle. */
	if (!tegra_ahci_are_all_ports_idle(host)) {
		dev_err(host->dev, "** pg: cmds; abort power gating **\n");
		return false;
	}
	tegra_ahci_put_sata_in_iddq();

	val = pmc_readl(APBDEV_PMC_PWRGATE_TOGGLE_0);
	val |= PARTID_VALUE;
	val |= START;
	pmc_writel(val, APBDEV_PMC_PWRGATE_TOGGLE_0);

	val = pmc_readl(APBDEV_PMC_PWRGATE_STATUS_0);
	val &= ~SAX_MASK;
	pmc_writel(val, APBDEV_PMC_PWRGATE_STATUS_0);

	/* power off the sata */
	status = tegra_powergate_partition_with_clk_off(TEGRA_POWERGATE_SATA);
	if (status) {
		dev_err(host->dev, "** failed to turn-off SATA (0x%x) **\n",
				   status);
		return false;
	}

	return true;
}

static bool tegra_ahci_power_un_gate(struct ata_host *host)
{
	u32 val;
	u32 timeout;
	struct tegra_ahci_host_priv *tegra_hpriv;
	int status;

	tegra_hpriv = (struct tegra_ahci_host_priv *)host->private_data;

	tegra_ahci_iddqlane_config();

	val = pmc_readl(APBDEV_PMC_PWRGATE_TOGGLE_0);
	val |= PARTID_VALUE;
	val |= START;
	pmc_writel(val, APBDEV_PMC_PWRGATE_TOGGLE_0);

	val = pmc_readl(APBDEV_PMC_PWRGATE_STATUS_0);
	val |= SAX_MASK;
	pmc_writel(val, APBDEV_PMC_PWRGATE_STATUS_0);

	status = tegra_unpowergate_partition_with_clk_on(TEGRA_POWERGATE_SATA);
	if (status) {
		dev_err(host->dev, "** failed to turn-on SATA (0x%x) **\n",
				   status);
		return false;
	}

	/* deasset PADPLL and wait until it locks. */
	val = clk_readl(CLK_RST_SATA_PLL_CFG0_REG);
	val &= ~PADPLL_RESET_OVERRIDE_VALUE_MASK;
	clk_writel(val, CLK_RST_SATA_PLL_CFG0_REG);

	tegra_ahci_clr_clk_rst_cnt_rst_dev();

#ifdef TEGRA_AHCI_CONTEXT_RESTORE
	/* restore registers */
	tegra_ahci_pg_restore_registers(host);
#endif
	tegra_ahci_pad_config();

	/*
	 * Wait for SATA_AUX_PAD_PLL_CNTL_1_0_LOCKDET to turn 1 with a timeout
	 * of 15 us.
	 */
	timeout = 15;
	while (timeout--) {
		udelay(1);
		val = xusb_readl(XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0);
		if (val & LOCKDET_FIELD)
			break;
	}
	if (timeout == 0)
		pr_err("%s: SATA_PAD_PLL is not locked in 15us.\n", __func__);

	/*
	 * During the restoration of the registers, the driver would now need to
	 * restore the register T_SATA0_CFG_POWER_GATE_SSTS_RESTORED after the
	 * ssts_det, ssts_spd are restored. This register is used to tell the
	 * controller whether a drive existed earlier or not and move the PHY
	 * state machines into either HR_slumber or not.
	 */
	val = scfg_readl(T_SATA0_CFG_POWER_GATE);
	val &= ~POWER_GATE_SSTS_RESTORED_MASK;
	val |= POWER_GATE_SSTS_RESTORED_YES;
	scfg_writel(val, T_SATA0_CFG_POWER_GATE);


	/*
	 * Driver needs to switch the rx_idle_t driven source back to from
	 * Sata controller after SAX is power-ungated.
	 */
	val = misc_readl(SATA_AUX_MISC_CNTL_1_REG);
	val &= ~DEVSLP_OVERRIDE;
	val &= ~L0_RX_IDLE_T_MUX_MASK;
	val |= L0_RX_IDLE_T_MUX_FROM_SATA;
	misc_writel(val, SATA_AUX_MISC_CNTL_1_REG);

	/*
	 * Driver can start to use main SATA interrupt instead of the
	 * rx_stat_t interrupt.
	 */
	val = pictlr_readl(PRI_ICTLR_CPU_IER_SET_0_OFFSET);
	val |= CPU_IER_SATA_CTL;
	pictlr_writel(val, PRI_ICTLR_CPU_IER_SET_0_OFFSET);

	/* Set the bits in the CAR to allow HW based low power sequencing. */
	val = clk_readl(CLK_RST_SATA_PLL_CFG0_REG);
	val &= ~PADPLL_RESET_SWCTL_MASK;
	clk_writel(val, CLK_RST_SATA_PLL_CFG0_REG);

	/*
	 * power un-gating process is complete by clearing
	 * APBDEV_PMC_SATA_PWRGT_0.Pmc2sata_pg_info = 0
	 */
	val = pmc_readl(APB_PMC_SATA_PWRGT_0_REG);
	val &= ~PG_INFO_MASK;
	pmc_writel(val, APB_PMC_SATA_PWRGT_0_REG);

	return true;
}

static enum port_idle_status tegra_ahci_is_port_idle(struct ata_port *ap)
{
	void __iomem *port_mmio = ahci_port_base(ap);

	if (readl(port_mmio + PORT_CMD_ISSUE) ||
	    readl(port_mmio + PORT_SCR_ACT))
		return PORT_IS_NOT_IDLE;
	return PORT_IS_IDLE;
}

/* check if all supported ports are idle (no outstanding commands) */
static bool tegra_ahci_are_all_ports_idle(struct ata_host *host)
{	int i;
	struct ata_port *ap;

	for (i = 0; i < host->n_ports; i++) {
		ap = host->ports[i];
		if (ap && (tegra_ahci_is_port_idle(ap) == PORT_IS_NOT_IDLE))
			return false;
	}
	return true;
}

#ifdef CONFIG_TEGRA_SATA_IDLE_POWERGATE
static void tegra_ahci_abort_pad_suspend(struct ata_host *host)
{
	/*No implementation*/
}

static bool tegra_ahci_pad_suspend(struct ata_host *host)
{
	u32 val;
	struct tegra_ahci_host_priv *tegra_hpriv;

	tegra_hpriv = (struct tegra_ahci_host_priv *)host->private_data;

	val = xusb_readl(XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL_1_0);
	val |= XUSB_PADCTL_IOPHY_MISC_IDDQ |
		XUSB_PADCTL_IOPHY_MISC_IDDQ_OVRD;
	xusb_writel(val, XUSB_PADCTL_IOPHY_MISC_PAD_S0_CTL_1_0);

	/* abort PG if there are errors occurred */
	if (tegra_ahci_check_errors(host)) {
		dev_err(host->dev, "** pg: errors; abort power gating **\n");
		return false;
	}
	/* make sure all ports have no outstanding commands and are idle. */
	if (!tegra_ahci_are_all_ports_idle(host)) {
		dev_err(host->dev, "** pg: cmds; abort power gating **\n");
		return false;
	}
	tegra_ahci_put_sata_in_iddq();

	val = clk_readl(CLK_RST_SATA_PLL_CFG0_REG);
	val |= (SATA_SEQ_PADPLL_PD_INPUT_VALUE |
		SATA_SEQ_LANE_PD_INPUT_VALUE | SATA_SEQ_RESET_INPUT_VALUE);
	clk_writel(val, CLK_RST_SATA_PLL_CFG0_REG);

	tegra_first_level_clk_gate();

	return true;
}

static bool tegra_ahci_pad_resume(struct ata_host *host)
{
	u32 val;
	u32 timeout;
	struct tegra_ahci_host_priv *tegra_hpriv;

	tegra_hpriv = (struct tegra_ahci_host_priv *)host->private_data;

	val = clk_readl(CLK_RST_SATA_PLL_CFG0_REG);
	val &= ~(SATA_SEQ_PADPLL_PD_INPUT_VALUE |
		SATA_SEQ_LANE_PD_INPUT_VALUE | SATA_SEQ_RESET_INPUT_VALUE);
	clk_writel(val, CLK_RST_SATA_PLL_CFG0_REG);

	if (tegra_first_level_clk_ungate() < 0) {
		pr_err("%s: flcg ungate failed\n", __func__);
		return false;
	}

	tegra_ahci_iddqlane_config();

	/* deasset PADPLL and wait until it locks. */
	val = clk_readl(CLK_RST_SATA_PLL_CFG0_REG);
	val &= ~PADPLL_RESET_OVERRIDE_VALUE_MASK;
	clk_writel(val, CLK_RST_SATA_PLL_CFG0_REG);

	tegra_ahci_pad_config();

	/*
	 * Wait for SATA_AUX_PAD_PLL_CNTL_1_0_LOCKDET to turn 1 with a timeout
	 * of 15 us.
	 */
	timeout = 15;
	while (timeout--) {
		udelay(1);
		val = xusb_readl(XUSB_PADCTL_IOPHY_PLL_S0_CTL1_0);
		if (val & LOCKDET_FIELD)
			break;
	}
	if (timeout == 0)
		pr_err("%s: SATA_PAD_PLL is not locked in 15us.\n", __func__);

	/*
	 * Driver can start to use main SATA interrupt instead of the
	 * rx_stat_t interrupt.
	 */
	val = pictlr_readl(PRI_ICTLR_CPU_IER_SET_0_OFFSET);
	val |= CPU_IER_SATA_CTL;
	pictlr_writel(val, PRI_ICTLR_CPU_IER_SET_0_OFFSET);

	/* Set the bits in the CAR to allow HW based low power sequencing. */
	val = clk_readl(CLK_RST_SATA_PLL_CFG0_REG);
	val &= ~PADPLL_RESET_SWCTL_MASK;
	clk_writel(val, CLK_RST_SATA_PLL_CFG0_REG);

	return true;
}

static void tegra_ahci_to_add_idle_timer(struct ata_host *host)
{
	struct tegra_ahci_host_priv *tegra_hpriv;

	tegra_hpriv = (struct tegra_ahci_host_priv *)host->private_data;

	/* note: the routine is called from interrupt context */
	spin_lock(&host->lock);
	/* start idle-timer if all ports have no outstanding commands */
	if (tegra_ahci_are_all_ports_idle(host)) {
		/* adjust tegra_ahci_idle_time to minimum if it is too small */
		tegra_ahci_idle_time = max((u32)TEGRA_AHCI_MIN_IDLE_TIME,
					   tegra_ahci_idle_time);
		tegra_hpriv->idle_timer.expires =
			ata_deadline(jiffies, tegra_ahci_idle_time);
		mod_timer(&tegra_hpriv->idle_timer,
			  tegra_hpriv->idle_timer.expires);
	}
	spin_unlock(&host->lock);
}

static void tegra_ahci_idle_timer(unsigned long arg)
{
	struct ata_host *host = (void *)arg;
	struct tegra_ahci_host_priv *tegra_hpriv;
	unsigned long flags;

	tegra_hpriv = (struct tegra_ahci_host_priv *)host->private_data;

	spin_lock_irqsave(&host->lock, flags);
	if (tegra_hpriv->pg_state == SATA_ON)
		tegra_hpriv->pg_state = SATA_GOING_OFF;
	else {
		dev_err(host->dev, "idle_timer: bad state (%u)\n",
				tegra_hpriv->pg_state);
		WARN_ON(1);
		spin_unlock_irqrestore(&host->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&host->lock, flags);
	tegra_ahci_runtime_suspend(tegra_hpriv->dev);
}

static int tegra_ahci_queue_one_qc(struct tegra_ahci_host_priv *tegra_hpriv,
				   struct ata_queued_cmd *qc)
{
	struct tegra_qc_list *qc_list;

	qc_list = kmalloc(sizeof(struct tegra_qc_list), GFP_ATOMIC);
	if (!qc_list) {
		dev_err(tegra_hpriv->dev, "failed to alloc qc_list\n");
		return AC_ERR_SYSTEM;
	}
	qc_list->qc = qc;
	list_add_tail(&(qc_list->list), &(tegra_hpriv->qc_list));
	dev_dbg(tegra_hpriv->dev, "queuing qc=%x\n", (unsigned int)qc);
	return 0;
}

static void tegra_ahci_dequeue_qcs(struct tegra_ahci_host_priv *tegra_hpriv)
{
	struct list_head *list, *next;
	struct tegra_qc_list *qc_list;
	struct ata_queued_cmd *qc;

	/* now qc_issue all qcs in the qc_list */
	list_for_each_safe(list, next, &tegra_hpriv->qc_list) {
		qc_list = list_entry(list, struct tegra_qc_list, list);
		qc = qc_list->qc;
		dev_dbg(tegra_hpriv->dev, "dequeue qc=%x\n", (unsigned int)qc);
		ahci_ops.qc_issue(qc);
		list_del(list);
		kfree(qc_list);
	}
}

static unsigned int tegra_ahci_qc_issue(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct ata_host *host = ap->host;
	struct tegra_ahci_host_priv *tegra_hpriv = host->private_data;

	/* stop the idle timer */
	if (timer_pending(&tegra_hpriv->idle_timer))
		del_timer_sync(&tegra_hpriv->idle_timer);

	/* note: host->lock is locked */
	switch (tegra_hpriv->pg_state) {
	case SATA_ON:
		/* normal case, issue the qc */
		return ahci_ops.qc_issue(qc);
	case SATA_GOING_OFF:
	case SATA_ABORT_OFF:
		/* SATA is going OFF, let's abort the suspend */
		dev_dbg(host->dev, "** qc_issue: going OFF **\n");
		tegra_hpriv->pg_state = SATA_ABORT_OFF;
		return tegra_ahci_queue_one_qc(tegra_hpriv, qc);
	case SATA_OFF:
		dev_dbg(host->dev, "** qc_issue: request power-up sata **\n");
		queue_work(tegra_ahci_work_q, &tegra_ahci_work);
		tegra_hpriv->pg_state = SATA_GOING_ON;
		/* continue with the following code to queue the qc */
	case SATA_GOING_ON:
		return tegra_ahci_queue_one_qc(tegra_hpriv, qc);
	default:
		dev_err(host->dev, "** qc_issue: bad state (%u) **\n",
					tegra_hpriv->pg_state);
		WARN_ON(1);
		return AC_ERR_SYSTEM;
	}
}

static int tegra_ahci_hardreset(struct ata_link *link, unsigned int *class,
				unsigned long deadline)
{
	struct ata_port *ap = link->ap;
	struct ata_host *host = ap->host;
	struct tegra_ahci_host_priv *tegra_hpriv = host->private_data;
	unsigned long flags;
	int rc;

	if (tegra_hpriv->pg_state == SATA_OFF) {
		dev_dbg(host->dev, "** hreset: request power-up sata **\n");
		spin_lock_irqsave(&host->lock, flags);
		rc = tegra_ahci_runtime_resume(tegra_hpriv->dev);
		spin_unlock_irqrestore(&host->lock, flags);
		/* rc == 0 means the request has been run successfully */
		if (rc) {
			dev_err(host->dev, "** hreset: rt_get()=%d **\n", rc);
			WARN_ON(1);
			return AC_ERR_SYSTEM;
		}
		tegra_hpriv->pg_state = SATA_ON;
	}

	return ahci_ops.hardreset(link, class, deadline);
}

static irqreturn_t tegra_ahci_interrupt(int irq, void *dev_instance)
{
	irqreturn_t irq_retval;
	u32 val;

	val = misc_readl(SATA_AUX_RX_STAT_INT_0);
	if (!(val && SATA_RX_STAT_INT_DISABLE)) {
		val |= SATA_RX_STAT_INT_DISABLE;
		misc_writel(val, SATA_AUX_RX_STAT_INT_0);
	}

	irq_retval = ahci_interrupt(irq, dev_instance);
	if (irq_retval == IRQ_NONE)
		return IRQ_NONE;

#ifdef CONFIG_PM
	tegra_ahci_to_add_idle_timer((struct ata_host *)dev_instance);
#endif

	return irq_retval;
}
#endif
#endif

static int tegra_ahci_remove_one(struct platform_device *pdev)
{
	struct ata_host *host = dev_get_drvdata(&pdev->dev);
	struct ahci_host_priv *hpriv;

	BUG_ON(host == NULL);
	BUG_ON(host->iomap[AHCI_PCI_BAR] == NULL);
	hpriv = host->private_data;

	tegra_ahci_controller_remove(pdev);

	devm_iounmap(&pdev->dev, host->iomap[AHCI_PCI_BAR]);
	ata_host_detach(host);

#ifdef TEGRA_AHCI_CONTEXT_RESTORE
	/* Free PG save/restore area */
	devm_kfree(&pdev->dev, ((struct tegra_ahci_host_priv *)hpriv)->pg_save);

#endif
#ifdef CONFIG_TEGRA_SATA_IDLE_POWERGATE
	cancel_work_sync(&tegra_ahci_work);
	if (tegra_ahci_work_q)
		destroy_workqueue(tegra_ahci_work_q);
#endif

	devm_kfree(&pdev->dev, hpriv);

	return 0;
}

static int tegra_ahci_init_one(struct platform_device *pdev)
{
	struct ata_port_info pi = ahci_port_info;
	const struct ata_port_info *ppi[] = { &pi, NULL };
	struct device *dev = &pdev->dev;
	struct ahci_host_priv *hpriv = NULL;
	struct tegra_ahci_host_priv *tegra_hpriv;
	struct ata_host *host = NULL;
	int n_ports, i, rc = 0;
	struct resource *res, *irq_res;
	void __iomem *mmio;

#if defined(TEGRA_AHCI_CONTEXT_RESTORE)
	u32 save_size;
#endif
	irq_handler_t irq_handler = ahci_interrupt;

	VPRINTK("ENTER\n");

	WARN_ON((int)ATA_MAX_QUEUE > AHCI_MAX_CMDS);

	ata_print_version_once(&pdev->dev, DRV_VERSION);

	/* Simple resource validation */
	if (pdev->num_resources != 3) {
		dev_err(dev, "invalid number of resources\n");
		dev_err(dev, "not enough SATA resources\n");
		return -EINVAL;
	}

	/* acquire bar resources */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL)
		return -EINVAL;

	/* acquire IRQ resource */
	irq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (irq_res == NULL)
		return -EINVAL;
	if (irq_res->start <= 0)
		return -EINVAL;

	/* allocate sizeof tegra_ahci_host_priv, which contains extra fields */
	hpriv = devm_kzalloc(dev, sizeof(struct tegra_ahci_host_priv),
			     GFP_KERNEL);
	if (!hpriv) {
		rc = -ENOMEM;
		goto fail;
	}

#ifdef CONFIG_TEGRA_SATA_IDLE_POWERGATE
	tegra_ahci_work_q = alloc_workqueue("tegra_ahci_work_q",
					WQ_HIGHPRI | WQ_UNBOUND, 16);
	if (!tegra_ahci_work_q) {
		dev_err(dev, "TEGRA WORKQUEUE SATA init failed\n");
		goto fail;
	}
#endif

	hpriv->flags |= (unsigned long)pi.private_data;
	tegra_hpriv = (struct tegra_ahci_host_priv *)hpriv;
	tegra_hpriv->dev = dev;
	g_tegra_hpriv = tegra_hpriv;

	/* Call tegra init routine */
	rc = tegra_ahci_controller_init(tegra_hpriv, 0);
	if (rc != 0) {
		dev_err(dev, "TEGRA SATA init failed\n");
		goto fail;
	}

	/*
	 * We reserve a table of 6 BARs in tegra_hpriv to store BARs.
	 * Save the mapped AHCI_PCI_BAR address to the table.
	 */
	mmio = devm_ioremap(dev, res->start, (res->end-res->start+1));
	tegra_hpriv->bars_table[AHCI_PCI_BAR] = mmio;
	hpriv->mmio = mmio;

	/* save initial config */
	tegra_ahci_save_initial_config(pdev, hpriv);
	dev_dbg(dev, "past save init config\n");

	/* prepare host */
	if (hpriv->cap & HOST_CAP_NCQ) {
		pi.flags |= ATA_FLAG_NCQ;
		pi.flags |= ATA_FLAG_FPDMA_AA;
	}

	/*
	 * CAP.NP sometimes indicate the index of the last enabled
	 * port, at other times, that of the last possible port, so
	 * determining the maximum port number requires looking at
	 * both CAP.NP and port_map.
	 */
	n_ports = max(ahci_nr_ports(hpriv->cap), fls(hpriv->port_map));
	host = ata_host_alloc_pinfo(dev, ppi, n_ports);
	if (!host) {
		rc = -ENOMEM;
		goto fail;
	}
	host->private_data = hpriv;
	tegra_hpriv->host = host;
	host->iomap = tegra_hpriv->bars_table;

	if (!(hpriv->cap & HOST_CAP_SSS))
		host->flags |= ATA_HOST_PARALLEL_SCAN;
	else
		pr_info("ahci: SSS flag set, parallel bus scan disabled\n");

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];

		/* set initial link pm policy */
		ap->target_lpm_policy = ATA_LPM_UNKNOWN;

		/* disabled/not-implemented port */
		if (!(hpriv->port_map & (1 << i)))
			ap->ops = &ata_dummy_port_ops;
		else
			ap->target_lpm_policy = ATA_LPM_MIN_POWER;
	}

	rc = ahci_reset_controller(host);
	if (rc) {
		dev_err(dev, "Reset controller failed! (rc=%d)\n", rc);
		goto fail;
	}

	ahci_init_controller(host);
	ahci_print_info(host, "TEGRA-SATA");
	dev_dbg(dev, "controller init okay\n");

#if defined(TEGRA_AHCI_CONTEXT_RESTORE)
	/* Setup PG save/restore area: */

	/* calculate the size */
	save_size = ARRAY_SIZE(pg_save_ipfs_registers) +
			    ARRAY_SIZE(pg_save_config_registers) +
			    ARRAY_SIZE(pg_save_bar5_registers) +
			    ARRAY_SIZE(pg_save_bar5_bkdr_registers);

	/* and add save port_registers for all the ports */
	save_size += TEGRA_AHCI_NUM_PORTS *
		     (ARRAY_SIZE(pg_save_config_port_registers) +
		      ARRAY_SIZE(pg_save_bar5_port_registers) +
		      ARRAY_SIZE(pg_save_bar5_bkdr_port_registers));

	/*
	 * save_size is number of registers times number of bytes per
	 * register to get total save size.
	 */
	save_size *= sizeof(u32);
	tegra_hpriv->pg_save = devm_kzalloc(dev, save_size, GFP_KERNEL);
	if (!tegra_hpriv->pg_save) {
		rc = -ENOMEM;
		goto fail;
	}
#endif
#ifdef CONFIG_PM
#ifdef CONFIG_TEGRA_SATA_IDLE_POWERGATE
	tegra_hpriv->pg_state = SATA_ON;

	/* setup sata idle timer */
	init_timer_deferrable(&tegra_hpriv->idle_timer);
	tegra_hpriv->idle_timer.function = tegra_ahci_idle_timer;
	tegra_hpriv->idle_timer.data = (unsigned long)host;

	INIT_LIST_HEAD(&tegra_hpriv->qc_list);

	/* use our own irq handler */
	irq_handler = tegra_ahci_interrupt;
#endif

#endif

	rc = ata_host_activate(host, irq_res->start, irq_handler, 0, &ahci_sht);
	if (rc == 0)
		return 0;

fail:
	if (host) {
		if (host->iomap[AHCI_PCI_BAR])
			devm_iounmap(dev, host->iomap[AHCI_PCI_BAR]);
		ata_host_detach(host);
	}
	if (hpriv)
		devm_kfree(dev, hpriv);

#ifdef CONFIG_TEGRA_SATA_IDLE_POWERGATE
	if (tegra_ahci_work_q)
		destroy_workqueue(tegra_ahci_work_q);
#endif

	return rc;
}

static int __init ahci_init(void)
{
	return platform_driver_register(&tegra_platform_ahci_driver);
}

static void __exit ahci_exit(void)
{
	platform_driver_unregister(&tegra_platform_ahci_driver);
}

#ifdef	CONFIG_DEBUG_FS

#include <linux/debugfs.h>
#include <linux/seq_file.h>

static void dbg_ahci_dump_regs(struct seq_file *s, u32 *ptr, u32 base, u32 regs)
{
#define REGS_PER_LINE	4

	u32 i, j;
	u32 lines = regs / REGS_PER_LINE;

	for (i = 0; i < lines; i++) {
		seq_printf(s, "0x%08x: ", base+(i*16));
		for (j = 0; j < REGS_PER_LINE; ++j) {
			seq_printf(s, "0x%08x ", readl(ptr));
			++ptr;
		}
		seq_puts(s, "\n");
	}
#undef REGS_PER_LINE
}

static int dbg_ahci_dump_show(struct seq_file *s, void *unused)
{
	u32 base;
	u32 *ptr;
	u32 i;

#ifdef CONFIG_TEGRA_SATA_IDLE_POWERGATE
	tegra_ahci_runtime_resume(g_tegra_hpriv->dev);
#endif

	base = TEGRA_SATA_CONFIG_BASE;
	ptr = (u32 *)IO_TO_VIRT(base);
	seq_puts(s, "SATA CONFIG Registers:\n");
	seq_puts(s, "----------------------\n");
	dbg_ahci_dump_regs(s, ptr, base, 0x200);

	base = TEGRA_SATA_BAR5_BASE;
	ptr = (u32 *)IO_TO_VIRT(base);
	seq_puts(s, "\nAHCI HBA Registers:\n");
	seq_puts(s, "-------------------\n");
	dbg_ahci_dump_regs(s, ptr, base, 64);

	for (i = 0; i < TEGRA_AHCI_NUM_PORTS; ++i) {
		base = TEGRA_SATA_BAR5_BASE + 0x100 + (0x80*i);
		ptr = (u32 *)IO_TO_VIRT(base);
		seq_printf(s, "\nPort %u Registers:\n", i);
		seq_puts(s, "---------------\n");
		dbg_ahci_dump_regs(s, ptr, base, 20);
	}

#ifdef	CONFIG_TEGRA_SATA_IDLE_POWERGATE
	/* adjust tegra_ahci_idle_time to minimum if it is too small */
	tegra_ahci_idle_time = max((u32)TEGRA_AHCI_MIN_IDLE_TIME,
				   tegra_ahci_idle_time);
	seq_printf(s, "\nIdle Timeout = %u milli-seconds.\n",
		      tegra_ahci_idle_time);
#endif

	if (tegra_powergate_is_powered(TEGRA_POWERGATE_SATA))
		seq_puts(s, "\n=== SATA controller is powered on ===\n\n");
	else
		seq_puts(s, "\n=== SATA controller is powered off ===\n\n");
#ifdef CONFIG_TEGRA_SATA_IDLE_POWERGATE
	tegra_ahci_runtime_suspend(g_tegra_hpriv->dev);
#endif


	return 0;
}

static int dbg_ahci_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_ahci_dump_show, &inode->i_private);
}

static const struct file_operations debug_fops = {
	.open		= dbg_ahci_dump_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init tegra_ahci_dump_debuginit(void)
{
	(void) debugfs_create_file("tegra_ahci", S_IRUGO,
				   NULL, NULL, &debug_fops);
#ifdef	CONFIG_TEGRA_SATA_IDLE_POWERGATE
	(void) debugfs_create_u32("tegra_ahci_idle_ms", S_IRUGO | S_IXUGO
					| S_IWUSR | S_IWGRP,
				   NULL, &tegra_ahci_idle_time);
#endif
	return 0;
}
late_initcall(tegra_ahci_dump_debuginit);
#endif

MODULE_AUTHOR("NVIDIA");
MODULE_DESCRIPTION("Tegra AHCI SATA low-level driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRV_VERSION);

module_init(ahci_init);
module_exit(ahci_exit);
