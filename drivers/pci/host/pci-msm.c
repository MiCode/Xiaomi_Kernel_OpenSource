/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

/*
 * MSM PCIe controller driver.
 */

#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of_gpio.h>
#include <linux/clk/msm-clk.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/io.h>
#include <linux/msi.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/pm_wakeup.h>
#include <linux/compiler.h>
#include <linux/ipc_logging.h>
#include <linux/msm_pcie.h>


#define QSERDES_COM_PLL_VCOTAIL_EN	0x004
#define QSERDES_COM_IE_TRIM	0x00C
#define QSERDES_COM_IP_TRIM	0x010
#define QSERDES_COM_PLL_CNTRL	0x014
#define QSERDES_COM_PLL_IP_SETI	0x024
#define QSERDES_COM_PLL_CP_SETI	0x034
#define QSERDES_COM_PLL_IP_SETP	0x038
#define QSERDES_COM_PLL_CP_SETP	0x03C
#define QSERDES_COM_SYSCLK_EN_SEL_TXBAND	0x048
#define QSERDES_COM_RESETSM_CNTRL	0x04C
#define QSERDES_COM_RESETSM_CNTRL2	0x050
#define QSERDES_COM_PLLLOCK_CMP1	0x090
#define QSERDES_COM_PLLLOCK_CMP2	0x094
#define QSERDES_COM_PLLLOCK_CMP_EN	0x09C
#define QSERDES_COM_DEC_START1	0x0AC
#define QSERDES_COM_RES_CODE_START_SEG1	0x0E0
#define QSERDES_COM_RES_CODE_CAL_CSR	0x0E8
#define QSERDES_COM_RES_TRIM_CONTROL	0x0F0
#define QSERDES_COM_DIV_FRAC_START1	0x100
#define QSERDES_COM_DIV_FRAC_START2	0x104
#define QSERDES_COM_DIV_FRAC_START3	0x108
#define QSERDES_COM_DEC_START2	0x10C
#define QSERDES_COM_PLL_RXTXEPCLK_EN	0x110
#define QSERDES_COM_PLL_CRCTRL	0x114

#define QSERDES_COM_PLL_VCO_HIGH	0x14C
#define QSERDES_COM_RESET_SM	0x150
#define QSERDES_COM_MUXVAL	0x154
#define QSERDES_COM_CORE_RES_CODE_DN 0x158
#define QSERDES_COM_CORE_RES_CODE_UP 0x15C
#define QSERDES_COM_CORE_VCO_TUNE 0x160
#define QSERDES_COM_CORE_VCO_TAIL 0x164
#define QSERDES_COM_CORE_KVCO_CODE 0x168

#define QSERDES_RX_CDR_CONTROL1	0x400
#define QSERDES_RX_CDR_CONTROL_HALF	0x408
#define QSERDES_RX_RX_EQ_GAIN1_LSB	0x4A8
#define QSERDES_RX_RX_EQ_GAIN1_MSB	0x4AC
#define QSERDES_RX_RX_EQ_GAIN2_LSB	0x4B0
#define QSERDES_RX_RX_EQ_GAIN2_MSB	0x4B4
#define QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2	0x4BC
#define QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1	0x4F0
#define QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL2 0x4F4
#define QSERDES_RX_SIGDET_ENABLES	0x4F8
#define QSERDES_RX_SIGDET_CNTRL	0x500
#define QSERDES_RX_SIGDET_DEGLITCH_CNTRL	0x504

#define QSERDES_RX_PI_CTRL1	0x558
#define QSERDES_RX_PI_CTRL2	0x55C
#define QSERDES_RX_PI_QUAD	0x560
#define QSERDES_RX_IDATA1	0x564
#define QSERDES_RX_IDATA2	0x568
#define QSERDES_RX_AUX_DATA1	0x56C
#define QSERDES_RX_AUX_DATA2	0x570
#define QSERDES_RX_AC_JTAG_OUTP	0x574
#define QSERDES_RX_AC_JTAG_OUTN	0x578
#define QSERDES_RX_RX_SIGDET	0x57C
#define QSERDES_RX_RX_VDCOFF	0x580
#define QSERDES_RX_IDAC_CAL_ON	0x584
#define QSERDES_RX_IDAC_STATUS_I	0x588
#define QSERDES_RX_IDAC_STATUS_Q	0x58C
#define QSERDES_RX_IDAC_STATUS_A	0x590
#define QSERDES_RX_CALST_STATUS_I	0x594
#define QSERDES_RX_CALST_STATUS_Q	0x598
#define QSERDES_RX_CALST_STATUS_A	0x59C
#define QSERDES_RX_EOM_STATUS0	0x5A0
#define QSERDES_RX_EOM_STATUS1	0x5A4
#define QSERDES_RX_EOM_STATUS2	0x5A8
#define QSERDES_RX_EOM_STATUS3	0x5AC
#define QSERDES_RX_EOM_STATUS4	0x5B0
#define QSERDES_RX_EOM_STATUS5	0x5B4
#define QSERDES_RX_EOM_STATUS6	0x5B8
#define QSERDES_RX_EOM_STATUS7	0x5BC
#define QSERDES_RX_EOM_STATUS8	0x5C0
#define QSERDES_RX_EOM_STATUS9	0x5C4
#define QSERDES_RX_RX_ALOG_INTF_OBSV	0x5C8
#define QSERDES_RX_READ_EQCODE	0x5CC
#define QSERDES_RX_READ_OFFSETCODE	0x5D0

#define QSERDES_TX_RCV_DETECT_LVL	0x268

#define QSERDES_TX_BIST_STATUS	0x2B4
#define QSERDES_TX_BIST_ERROR_COUNT1	0x2B8
#define QSERDES_TX_BIST_ERROR_COUNT2	0x2BC
#define QSERDES_TX_TX_ALOG_INTF_OBSV	0x2C0
#define QSERDES_TX_PWM_DEC_STATUS	0x2C4

#define PCIE_PHY_SW_RESET	0x600
#define PCIE_PHY_POWER_DOWN_CONTROL	0x604
#define PCIE_PHY_START	0x608
#define PCIE_PHY_ENDPOINT_REFCLK_DRIVE	0x648
#define PCIE_PHY_RX_IDLE_DTCT_CNTRL 0x64C
#define PCIE_PHY_POWER_STATE_CONFIG1	0x650
#define PCIE_PHY_POWER_STATE_CONFIG2	0x654

#define PCIE_PHY_BIST_CHK_ERR_CNT_L	0x718
#define PCIE_PHY_BIST_CHK_ERR_CNT_H	0x71C
#define PCIE_PHY_BIST_CHK_STATUS	0x720
#define PCIE_PHY_LFPS_RXTERM_IRQ_SOURCE	0x724
#define PCIE_PHY_PCS_STATUS	0x728
#define PCIE_PHY_PCS_STATUS2	0x72C
#define PCIE_PHY_REVISION_ID0	0x730
#define PCIE_PHY_REVISION_ID1	0x734
#define PCIE_PHY_REVISION_ID2	0x738
#define PCIE_PHY_REVISION_ID3	0x73C
#define PCIE_PHY_DEBUG_BUS_0_STATUS	0x740
#define PCIE_PHY_DEBUG_BUS_1_STATUS	0x744
#define PCIE_PHY_DEBUG_BUS_2_STATUS	0x748
#define PCIE_PHY_DEBUG_BUS_3_STATUS	0x74C


#define PCIE20_PARF_SYS_CTRL	     0x00
#define PCIE20_PARF_PM_STTS		0x24
#define PCIE20_PARF_PCS_DEEMPH	   0x34
#define PCIE20_PARF_PCS_SWING	    0x38
#define PCIE20_PARF_PHY_CTRL	     0x40
#define PCIE20_PARF_PHY_REFCLK	   0x4C
#define PCIE20_PARF_CONFIG_BITS	  0x50
#define PCIE20_PARF_DBI_BASE_ADDR	0x168
#define PCIE20_PARF_SLV_ADDR_SPACE_SIZE	0x16C
#define PCIE20_PARF_AXI_MSTR_WR_ADDR_HALT   0x178
#define PCIE20_PARF_LTSSM              0x1B0

#define PCIE20_ELBI_VERSION		0x00
#define PCIE20_ELBI_SYS_CTRL	     0x04
#define PCIE20_ELBI_SYS_STTS		 0x08

#define PCIE20_CAP			   0x70
#define PCIE20_CAP_LINKCTRLSTATUS	(PCIE20_CAP + 0x10)

#define PCIE20_COMMAND_STATUS	    0x04
#define PCIE20_HEADER_TYPE		0x0C
#define PCIE20_BUSNUMBERS		  0x18
#define PCIE20_MEMORY_BASE_LIMIT	 0x20
#define PCIE20_L1SUB_CONTROL1	    0x158
#define PCIE20_EP_L1SUB_CTL1_OFFSET    0x30
#define PCIE20_DEVICE_CONTROL2_STATUS2 0x98

#define PCIE20_ACK_F_ASPM_CTRL_REG     0x70C
#define PCIE20_ACK_N_FTS		   0xff00

#define PCIE20_PLR_IATU_VIEWPORT	 0x900
#define PCIE20_PLR_IATU_CTRL1	    0x904
#define PCIE20_PLR_IATU_CTRL2	    0x908
#define PCIE20_PLR_IATU_LBAR	     0x90C
#define PCIE20_PLR_IATU_UBAR	     0x910
#define PCIE20_PLR_IATU_LAR		0x914
#define PCIE20_PLR_IATU_LTAR	     0x918
#define PCIE20_PLR_IATU_UTAR	     0x91c

#define PCIE20_CTRL1_TYPE_CFG0		0x04
#define PCIE20_CTRL1_TYPE_CFG1		0x05

#define PCIE_VENDOR_ID_RCP		 0x17cb
#define PCIE_DEVICE_ID_RCP		 0x0300

#define RD 0
#define WR 1
#define MSM_PCIE_ERROR -1

#define PERST_PROPAGATION_DELAY_US_MIN	  10000
#define PERST_PROPAGATION_DELAY_US_MAX	  10005
#define REFCLK_STABILIZATION_DELAY_US_MIN     1000
#define REFCLK_STABILIZATION_DELAY_US_MAX     1005
#define LINK_UP_TIMEOUT_US_MIN		    5000
#define LINK_UP_TIMEOUT_US_MAX		    5100
#define LINK_UP_CHECK_MAX_COUNT		   20
#define PHY_STABILIZATION_DELAY_US_MIN	  995
#define PHY_STABILIZATION_DELAY_US_MAX	  1005
#define LINKDOWN_INIT_WAITING_US_MIN    995
#define LINKDOWN_INIT_WAITING_US_MAX    1005
#define LINKDOWN_WAITING_US_MIN	   4900
#define LINKDOWN_WAITING_US_MAX	   5100
#define LINKDOWN_WAITING_COUNT	    200

#define PHY_READY_TIMEOUT_COUNT		   10
#define XMLH_LINK_UP				  0x400
#define MAX_LINK_RETRIES 5
#define MAX_BUS_NUM 3
#define MAX_PROP_SIZE 32
#define MAX_RC_NAME_LEN 15
#define MSM_PCIE_MAX_VREG 3
#define MSM_PCIE_MAX_CLK 7
#define MSM_PCIE_MAX_PIPE_CLK 1
#define MAX_RC_NUM 2
#define MAX_DEVICE_NUM 20
#define PCIE_MSI_NR_IRQS 256
#define PCIE_LOG_PAGES (50)
#define PCIE_CONF_SPACE_DW			1024
#define PCIE_CLEAR				0xDEADBEEF
#define PCIE_LINK_DOWN				0xFFFFFFFF

#define MSM_PCIE_MSI_PHY 0xa0000000
#define PCIE20_MSI_CTRL_ADDR		(0x820)
#define PCIE20_MSI_CTRL_UPPER_ADDR	(0x824)
#define PCIE20_MSI_CTRL_INTR_EN	   (0x828)
#define PCIE20_MSI_CTRL_INTR_MASK	 (0x82C)
#define PCIE20_MSI_CTRL_INTR_STATUS     (0x830)
#define PCIE20_MSI_CTRL_MAX 8

/* PM control options */
#define PM_IRQ			 0x1
#define PM_CLK			 0x2
#define PM_GPIO			0x4
#define PM_VREG			0x8
#define PM_PIPE_CLK		  0x10
#define PM_ALL (PM_IRQ | PM_CLK | PM_GPIO | PM_VREG | PM_PIPE_CLK)

#ifdef CONFIG_PHYS_ADDR_T_64BIT
#define PCIE_UPPER_ADDR(addr) ((u32)((addr) >> 32))
#else
#define PCIE_UPPER_ADDR(addr) (0x0)
#endif
#define PCIE_LOWER_ADDR(addr) ((u32)((addr) & 0xffffffff))

/* Config Space Offsets */
#define BDF_OFFSET(bus, devfn) \
	((bus << 24) | (devfn << 16))

#define PCIE_BUS_PRIV_DATA(pdev) \
	(((struct pci_sys_data *)pdev->bus->sysdata)->private_data)

#define PCIE_GEN_DBG(x...) do { \
	if (msm_pcie_debug_mask) \
		pr_alert(x); \
	} while (0)

#define PCIE_DBG(dev, fmt, arg...) do {			 \
	if ((dev) && (dev)->ipc_log)   \
		ipc_log_string((dev)->ipc_log, "%s: " fmt, __func__, arg); \
	if (msm_pcie_debug_mask)   \
		pr_alert("%s: " fmt, __func__, arg);		  \
	} while (0)

#define PCIE_INFO(dev, fmt, arg...) do {			 \
	if ((dev) && (dev)->ipc_log)   \
		ipc_log_string((dev)->ipc_log, "%s: " fmt, __func__, arg); \
	pr_info("%s: " fmt, __func__, arg);  \
	} while (0)

#define PCIE_ERR(dev, fmt, arg...) do {			 \
	if ((dev) && (dev)->ipc_log)   \
		ipc_log_string((dev)->ipc_log, "%s: " fmt, __func__, arg); \
	pr_err("%s: " fmt, __func__, arg);  \
	} while (0)


enum msm_pcie_res {
	MSM_PCIE_RES_PARF,
	MSM_PCIE_RES_PHY,
	MSM_PCIE_RES_DM_CORE,
	MSM_PCIE_RES_ELBI,
	MSM_PCIE_RES_CONF,
	MSM_PCIE_RES_IO,
	MSM_PCIE_RES_BARS,
	MSM_PCIE_MAX_RES,
};

enum msm_pcie_irq {
	MSM_PCIE_INT_MSI,
	MSM_PCIE_INT_A,
	MSM_PCIE_INT_B,
	MSM_PCIE_INT_C,
	MSM_PCIE_INT_D,
	MSM_PCIE_INT_PLS_PME,
	MSM_PCIE_INT_PME_LEGACY,
	MSM_PCIE_INT_PLS_ERR,
	MSM_PCIE_INT_AER_LEGACY,
	MSM_PCIE_INT_LINK_UP,
	MSM_PCIE_INT_LINK_DOWN,
	MSM_PCIE_INT_BRIDGE_FLUSH_N,
	MSM_PCIE_MAX_IRQ,
};

enum msm_pcie_gpio {
	MSM_PCIE_GPIO_PERST,
	MSM_PCIE_GPIO_WAKE,
	MSM_PCIE_MAX_GPIO
};

enum msm_pcie_link_status {
	MSM_PCIE_LINK_DEINIT,
	MSM_PCIE_LINK_ENABLED,
	MSM_PCIE_LINK_DISABLED
};

/* gpio info structure */
struct msm_pcie_gpio_info_t {
	char	*name;
	uint32_t   num;
	bool	 out;
	uint32_t   on;
	uint32_t   init;
};

/* voltage regulator info structrue */
struct msm_pcie_vreg_info_t {
	struct regulator  *hdl;
	char		  *name;
	uint32_t	     max_v;
	uint32_t	     min_v;
	uint32_t	     opt_mode;
	bool		   required;
};

/* clock info structure */
struct msm_pcie_clk_info_t {
	struct clk  *hdl;
	char	  *name;
	u32	   freq;
	bool	  required;
};

/* resource info structure */
struct msm_pcie_res_info_t {
	char		*name;
	struct resource *resource;
	void __iomem    *base;
};

/* irq info structrue */
struct msm_pcie_irq_info_t {
	char		  *name;
	uint32_t	    num;
};

/* PCIe device info structure */
struct msm_pcie_device_info {
	u32			bdf;
	struct pci_dev		*dev;
	int			domain;
	void __iomem		*conf_base;
	unsigned long		phy_address;
};

/* msm pcie device structure */
struct msm_pcie_dev_t {
	struct platform_device	 *pdev;
	struct pci_dev *dev;
	struct regulator *gdsc;
	struct msm_pcie_vreg_info_t  vreg[MSM_PCIE_MAX_VREG];
	struct msm_pcie_gpio_info_t  gpio[MSM_PCIE_MAX_GPIO];
	struct msm_pcie_clk_info_t   clk[MSM_PCIE_MAX_CLK];
	struct msm_pcie_clk_info_t   pipeclk[MSM_PCIE_MAX_PIPE_CLK];
	struct msm_pcie_res_info_t   res[MSM_PCIE_MAX_RES];
	struct msm_pcie_irq_info_t   irq[MSM_PCIE_MAX_IRQ];

	void __iomem		     *parf;
	void __iomem		     *phy;
	void __iomem		     *elbi;
	void __iomem		     *dm_core;
	void __iomem		     *conf;
	void __iomem		     *bars;

	uint32_t			    axi_bar_start;
	uint32_t			    axi_bar_end;

	struct resource		   *dev_mem_res;
	struct resource		   *dev_io_res;

	uint32_t			    wake_n;
	uint32_t			    vreg_n;
	uint32_t			    gpio_n;
	uint32_t			    parf_deemph;
	uint32_t			    parf_swing;

	bool				 cfg_access;
	spinlock_t			 cfg_lock;
	unsigned long		    irqsave_flags;
	struct mutex		     setup_lock;

	struct irq_domain		*irq_domain;
	DECLARE_BITMAP(msi_irq_in_use, PCIE_MSI_NR_IRQS);
	uint32_t			   msi_gicm_addr;
	uint32_t			   msi_gicm_base;
	bool				 use_msi;

	enum msm_pcie_link_status    link_status;
	bool				 user_suspend;
	bool                         disable_pc;
	struct pci_saved_state	     *saved_state;

	struct wakeup_source	     ws;
	struct msm_bus_scale_pdata   *bus_scale_table;
	uint32_t			   bus_client;

	bool				l0s_supported;
	bool				l1_supported;
	bool				 l1ss_supported;
	bool				 aux_clk_sync;
	uint32_t			   n_fts;
	bool				 ext_ref_clk;
	uint32_t			   ep_latency;
	uint32_t			current_bdf;
	bool				 ep_wakeirq;

	uint32_t			   rc_idx;
	bool				drv_ready;
	bool				 enumerated;
	struct work_struct	     handle_wake_work;
	struct mutex		     recovery_lock;
	spinlock_t                   linkdown_lock;
	spinlock_t                   wakeup_lock;
	ulong				linkdown_counter;
	bool				 suspending;
	ulong				wake_counter;
	u32		ep_shadow[MAX_DEVICE_NUM][PCIE_CONF_SPACE_DW];
	u32				  rc_shadow[PCIE_CONF_SPACE_DW];
	bool				 shadow_en;
	struct msm_pcie_register_event *event_reg;
	bool				 power_on;
	void				 *ipc_log;
	struct msm_pcie_device_info   pcidev_table[MAX_DEVICE_NUM];
};


/* debug mask sys interface */
static int msm_pcie_debug_mask;
module_param_named(debug_mask, msm_pcie_debug_mask,
			    int, S_IRUGO | S_IWUSR | S_IWGRP);

/* Table to track info of PCIe devices */
static struct msm_pcie_device_info
	msm_pcie_dev_tbl[MAX_RC_NUM * MAX_DEVICE_NUM];

/* PCIe driver state */
struct pcie_drv_sta {
	u32 rc_num;
	struct mutex drv_lock;
} pcie_drv;

/* msm pcie device data */
static struct msm_pcie_dev_t msm_pcie_dev[MAX_RC_NUM];

/* regulators */
static struct msm_pcie_vreg_info_t msm_pcie_vreg_info[MSM_PCIE_MAX_VREG] = {
	{NULL, "vreg-3.3", 0, 0, 0, false},
	{NULL, "vreg-1.8", 1800000, 1800000, 1000, true},
	{NULL, "vreg-0.9", 1000000, 1000000, 24000, true}
};

/* GPIOs */
static struct msm_pcie_gpio_info_t msm_pcie_gpio_info[MSM_PCIE_MAX_GPIO] = {
	{"perst-gpio",	0, 1, 0, 0},
	{"wake-gpio",	 0, 0, 0, 0}
};

/* clocks */
static struct msm_pcie_clk_info_t
	msm_pcie_clk_info[MAX_RC_NUM][MSM_PCIE_MAX_CLK] = {
	{
	{NULL, "pcie_0_ref_clk_src", 0, false},
	{NULL, "pcie_0_aux_clk", 1010000, true},
	{NULL, "pcie_0_cfg_ahb_clk", 0, true},
	{NULL, "pcie_0_mstr_axi_clk", 0, true},
	{NULL, "pcie_0_slv_axi_clk", 0, true},
	{NULL, "pcie_0_ldo", 0, true},
	{NULL, "pcie_0_phy_reset", 0, true}
	},
	{
	{NULL, "pcie_1_ref_clk_src", 0, false},
	{NULL, "pcie_1_aux_clk", 1010000, true},
	{NULL, "pcie_1_cfg_ahb_clk", 0, true},
	{NULL, "pcie_1_mstr_axi_clk", 0, true},
	{NULL, "pcie_1_slv_axi_clk", 0, true},
	{NULL, "pcie_1_ldo", 0, true},
	{NULL, "pcie_1_phy_reset", 0, true}
	}
};

/* Pipe Clocks */
static struct msm_pcie_clk_info_t
	msm_pcie_pipe_clk_info[MAX_RC_NUM][MSM_PCIE_MAX_PIPE_CLK] = {
	{
	{NULL, "pcie_0_pipe_clk", 125000000, true},
	},
	{
	{NULL, "pcie_1_pipe_clk", 125000000, true},
	}
};

/* resources */
static const struct msm_pcie_res_info_t msm_pcie_res_info[MSM_PCIE_MAX_RES] = {
	{"parf",	0, 0},
	{"phy",     0, 0},
	{"dm_core",	0, 0},
	{"elbi",	0, 0},
	{"conf",	0, 0},
	{"io",		0, 0},
	{"bars",	0, 0}
};

/* irqs */
static const struct msm_pcie_irq_info_t msm_pcie_irq_info[MSM_PCIE_MAX_IRQ] = {
	{"int_msi",	0},
	{"int_a",	0},
	{"int_b",	0},
	{"int_c",	0},
	{"int_d",	0},
	{"int_pls_pme",		0},
	{"int_pme_legacy",	0},
	{"int_pls_err",		0},
	{"int_aer_legacy",	0},
	{"int_pls_link_up",	0},
	{"int_pls_link_down",	0},
	{"int_bridge_flush_n",	0}
};

static inline void msm_pcie_write_reg(void *base, u32 offset, u32 value)
{
	writel_relaxed(value, base + offset);
	wmb();
}

static inline void msm_pcie_write_reg_field(void *base, u32 offset,
	const u32 mask, u32 val)
{
	u32 shift = find_first_bit((void *)&mask, 32);
	u32 tmp = readl_relaxed(base + offset);

	tmp &= ~mask; /* clear written bits */
	val = tmp | (val << shift);
	writel_relaxed(val, base + offset);
	wmb();
}


static void pcie_phy_dump(struct msm_pcie_dev_t *dev)
{
	PCIE_ERR(dev, "PCIe: RC%d PHY register dump\n", dev->rc_idx);
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_COM_PLL_VCO_HIGH: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_COM_PLL_VCO_HIGH));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_COM_RESET_SM: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_COM_RESET_SM));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_COM_MUXVAL: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_COM_MUXVAL));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_COM_CORE_RES_CODE_DN: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_COM_CORE_RES_CODE_DN));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_COM_CORE_RES_CODE_UP: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_COM_CORE_RES_CODE_UP));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_COM_CORE_VCO_TUNE: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_COM_CORE_VCO_TUNE));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_COM_CORE_VCO_TAIL: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_COM_CORE_VCO_TAIL));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_COM_CORE_KVCO_CODE: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_COM_CORE_KVCO_CODE));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_PI_CTRL1: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_PI_CTRL1));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_PI_CTRL2: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_PI_CTRL2));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_PI_QUAD: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_PI_QUAD));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_IDATA1: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_IDATA1));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_IDATA2: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_IDATA2));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_AUX_DATA1: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_AUX_DATA1));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_AUX_DATA2: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_AUX_DATA2));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_AC_JTAG_OUTP: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_AC_JTAG_OUTP));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_AC_JTAG_OUTN: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_AC_JTAG_OUTN));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_RX_SIGDET: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_RX_SIGDET));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_RX_VDCOFF: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_RX_VDCOFF));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_IDAC_CAL_ON: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_IDAC_CAL_ON));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_IDAC_STATUS_I: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_IDAC_STATUS_I));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_IDAC_STATUS_Q: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_IDAC_STATUS_Q));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_IDAC_STATUS_A: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_IDAC_STATUS_A));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_CALST_STATUS_I: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_CALST_STATUS_I));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_CALST_STATUS_Q: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_CALST_STATUS_Q));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_CALST_STATUS_A: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_CALST_STATUS_A));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_EOM_STATUS0: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_EOM_STATUS0));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_EOM_STATUS1: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_EOM_STATUS1));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_EOM_STATUS2: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_EOM_STATUS2));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_EOM_STATUS3: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_EOM_STATUS3));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_EOM_STATUS4: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_EOM_STATUS4));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_EOM_STATUS5: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_EOM_STATUS5));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_EOM_STATUS6: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_EOM_STATUS6));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_EOM_STATUS7: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_EOM_STATUS7));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_EOM_STATUS8: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_EOM_STATUS8));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_EOM_STATUS9: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_EOM_STATUS9));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_RX_ALOG_INTF_OBSV: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_RX_ALOG_INTF_OBSV));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_READ_EQCODE: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_READ_EQCODE));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_RX_READ_OFFSETCODE: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_RX_READ_OFFSETCODE));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_TX_BIST_STATUS: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_TX_BIST_STATUS));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_TX_BIST_ERROR_COUNT1: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_TX_BIST_ERROR_COUNT1));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_TX_BIST_ERROR_COUNT2: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_TX_BIST_ERROR_COUNT2));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_TX_TX_ALOG_INTF_OBSV: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_TX_TX_ALOG_INTF_OBSV));
	PCIE_ERR(dev, "PCIe: RC%d QSERDES_TX_PWM_DEC_STATUS: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + QSERDES_TX_PWM_DEC_STATUS));
	PCIE_ERR(dev, "PCIe: RC%d PCIE_PHY_BIST_CHK_ERR_CNT_L: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + PCIE_PHY_BIST_CHK_ERR_CNT_L));
	PCIE_ERR(dev, "PCIe: RC%d PCIE_PHY_BIST_CHK_ERR_CNT_H: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + PCIE_PHY_BIST_CHK_ERR_CNT_H));
	PCIE_ERR(dev, "PCIe: RC%d PCIE_PHY_BIST_CHK_STATUS: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + PCIE_PHY_BIST_CHK_STATUS));
	PCIE_ERR(dev, "PCIe: RC%d PCIE_PHY_LFPS_RXTERM_IRQ_SOURCE: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + PCIE_PHY_LFPS_RXTERM_IRQ_SOURCE));
	PCIE_ERR(dev, "PCIe: RC%d PCIE_PHY_PCS_STATUS: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + PCIE_PHY_PCS_STATUS));
	PCIE_ERR(dev, "PCIe: RC%d PCIE_PHY_PCS_STATUS2: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + PCIE_PHY_PCS_STATUS2));
	PCIE_ERR(dev, "PCIe: RC%d PCIE_PHY_REVISION_ID0: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + PCIE_PHY_REVISION_ID0));
	PCIE_ERR(dev, "PCIe: RC%d PCIE_PHY_REVISION_ID1: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + PCIE_PHY_REVISION_ID1));
	PCIE_ERR(dev, "PCIe: RC%d PCIE_PHY_REVISION_ID2: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + PCIE_PHY_REVISION_ID2));
	PCIE_ERR(dev, "PCIe: RC%d PCIE_PHY_REVISION_ID3: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + PCIE_PHY_REVISION_ID3));
	PCIE_ERR(dev, "PCIe: RC%d PCIE_PHY_DEBUG_BUS_0_STATUS: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + PCIE_PHY_DEBUG_BUS_0_STATUS));
	PCIE_ERR(dev, "PCIe: RC%d PCIE_PHY_DEBUG_BUS_1_STATUS: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + PCIE_PHY_DEBUG_BUS_1_STATUS));
	PCIE_ERR(dev, "PCIe: RC%d PCIE_PHY_DEBUG_BUS_2_STATUS: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + PCIE_PHY_DEBUG_BUS_2_STATUS));
	PCIE_ERR(dev, "PCIe: RC%d PCIE_PHY_DEBUG_BUS_3_STATUS: 0x%x\n",
	dev->rc_idx, readl_relaxed(dev->phy + PCIE_PHY_DEBUG_BUS_3_STATUS));
}

static void pcie_phy_init(struct msm_pcie_dev_t *dev)
{

	PCIE_DBG(dev, "RC%d: Initializing 20nm QMP phy - 19.2MHz\n",
		dev->rc_idx);

	msm_pcie_write_reg(dev->phy, PCIE_PHY_POWER_DOWN_CONTROL, 0x03);

	msm_pcie_write_reg(dev->phy, QSERDES_COM_SYSCLK_EN_SEL_TXBAND, 0x08);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_DEC_START1, 0x82);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_DEC_START2, 0x03);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_DIV_FRAC_START1, 0xD5);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_DIV_FRAC_START2, 0xAA);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_DIV_FRAC_START3, 0x4D);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_PLLLOCK_CMP_EN, 0x07);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_PLLLOCK_CMP1, 0x41);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_PLLLOCK_CMP2, 0x03);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_PLL_CRCTRL, 0x7C);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_PLL_CP_SETI, 0x02);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_PLL_IP_SETP, 0x1F);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_PLL_CP_SETP, 0x0F);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_PLL_IP_SETI, 0x01);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_IE_TRIM, 0x0F);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_IP_TRIM, 0x0F);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_PLL_CNTRL, 0x46);

	/* CDR Settings */
	msm_pcie_write_reg(dev->phy, QSERDES_RX_CDR_CONTROL1, 0xF4);
	msm_pcie_write_reg(dev->phy, QSERDES_RX_CDR_CONTROL_HALF, 0x2C);

	msm_pcie_write_reg(dev->phy, QSERDES_COM_PLL_VCOTAIL_EN, 0xE1);

	/* Calibration Settings */
	msm_pcie_write_reg(dev->phy, QSERDES_COM_RESETSM_CNTRL, 0x90);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_RESETSM_CNTRL2, 0x7);

	/* Additional writes */
	msm_pcie_write_reg(dev->phy, QSERDES_COM_RES_CODE_START_SEG1, 0x20);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_RES_CODE_CAL_CSR, 0x77);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_RES_TRIM_CONTROL, 0x15);
	msm_pcie_write_reg(dev->phy, QSERDES_TX_RCV_DETECT_LVL, 0x03);
	msm_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQ_GAIN1_LSB, 0xFF);
	msm_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQ_GAIN1_MSB, 0x1F);
	msm_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQ_GAIN2_LSB, 0xFF);
	msm_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQ_GAIN2_MSB, 0x00);
	msm_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2, 0x1E);
	msm_pcie_write_reg(dev->phy, QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1,
				0x67);
	msm_pcie_write_reg(dev->phy, QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL2, 0x80);
	msm_pcie_write_reg(dev->phy, QSERDES_RX_SIGDET_ENABLES, 0x40);
	msm_pcie_write_reg(dev->phy, QSERDES_RX_SIGDET_CNTRL, 0x50);
	msm_pcie_write_reg(dev->phy, QSERDES_RX_SIGDET_DEGLITCH_CNTRL, 0x0E);
	msm_pcie_write_reg(dev->phy, QSERDES_COM_PLL_RXTXEPCLK_EN, 0x10);
	msm_pcie_write_reg(dev->phy, PCIE_PHY_ENDPOINT_REFCLK_DRIVE, 0x10);
	msm_pcie_write_reg(dev->phy, PCIE_PHY_POWER_STATE_CONFIG1, 0x23);
	msm_pcie_write_reg(dev->phy, PCIE_PHY_POWER_STATE_CONFIG2, 0xCB);
	msm_pcie_write_reg(dev->phy, PCIE_PHY_RX_IDLE_DTCT_CNTRL, 0x4D);

	msm_pcie_write_reg(dev->phy, PCIE_PHY_SW_RESET, 0x00);
	msm_pcie_write_reg(dev->phy, PCIE_PHY_START, 0x03);
}

static bool pcie_phy_is_ready(struct msm_pcie_dev_t *dev)
{
	if (readl_relaxed(dev->phy + PCIE_PHY_PCS_STATUS) & BIT(6))
		return false;
	else
		return true;
}

static bool msm_pcie_confirm_linkup(struct msm_pcie_dev_t *dev,
						bool check_sw_stts,
						bool check_ep)
{
	u32 val;

	if (check_sw_stts && (dev->link_status != MSM_PCIE_LINK_ENABLED)) {
		PCIE_DBG(dev, "PCIe: The link of RC %d is not enabled.\n",
			dev->rc_idx);
		return false;
	}

	if (!(readl_relaxed(dev->dm_core + 0x80) & BIT(29))) {
		PCIE_DBG(dev, "PCIe: The link of RC %d is not up.\n",
			dev->rc_idx);
		return false;
	}

	val = readl_relaxed(dev->dm_core);
	PCIE_DBG(dev, "PCIe: device ID and vender ID of RC %d are 0x%x.\n",
		dev->rc_idx, val);
	if (val == PCIE_LINK_DOWN) {
		PCIE_ERR(dev,
			"PCIe: The link of RC %d is not really up; device ID and vender ID of RC %d are 0x%x.\n",
			dev->rc_idx, dev->rc_idx, val);
		return false;
	}

	if (check_ep) {
		val = readl_relaxed(dev->conf);
		PCIE_DBG(dev,
			"PCIe: device ID and vender ID of EP of RC %d are 0x%x.\n",
			dev->rc_idx, val);
		if (val == PCIE_LINK_DOWN) {
			PCIE_ERR(dev,
				"PCIe: The link of RC %d is not really up; device ID and vender ID of EP of RC %d are 0x%x.\n",
				dev->rc_idx, dev->rc_idx, val);
			return false;
		}
	}

	return true;
}

static void msm_pcie_cfg_recover(struct msm_pcie_dev_t *dev, bool rc)
{
	int i, j;
	u32 val = 0;
	u32 *shadow;
	void *cfg = dev->conf;

	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		if (!rc && !dev->pcidev_table[i].bdf)
			break;
		if (rc) {
			cfg = dev->dm_core;
			shadow = dev->rc_shadow;
		} else {
			shadow = dev->ep_shadow[i];
			PCIE_DBG(dev,
				"PCIe Device: %02x:%02x.%01x\n",
				dev->pcidev_table[i].bdf >> 24,
				dev->pcidev_table[i].bdf >> 19 & 0x1f,
				dev->pcidev_table[i].bdf >> 16 & 0x07);
		}
		for (j = PCIE_CONF_SPACE_DW - 1; j >= 0; j--) {
			val = readl_relaxed(shadow + j);
			if (val != PCIE_CLEAR) {
				PCIE_DBG(dev,
					"PCIe: before recovery:cfg 0x%x:0x%x\n",
					j * 4, readl_relaxed(cfg + j * 4));
				PCIE_DBG(dev,
					"PCIe: shadow_dw[%d]:cfg 0x%x:0x%x\n",
					j, j * 4, val);
				writel_relaxed(val, cfg + j * 4);
				wmb();
				PCIE_DBG(dev,
					"PCIe: after recovery:cfg 0x%x:0x%x\n\n",
					j * 4, readl_relaxed(cfg + j * 4));
			}
		}
		if (rc)
			break;
		cfg += SZ_4K;
	}
}

static void msm_pcie_write_mask(void __iomem *addr,
				uint32_t clear_mask, uint32_t set_mask)
{
	uint32_t val;

	val = (readl_relaxed(addr) & ~clear_mask) | set_mask;
	writel_relaxed(val, addr);
	wmb();  /* ensure data is written to hardware register */
}

static inline int msm_pcie_is_link_up(struct msm_pcie_dev_t *dev)
{
	return readl_relaxed(dev->dm_core +
			PCIE20_CAP_LINKCTRLSTATUS) & BIT(29);
}

/**
 * msm_pcie_iatu_config - configure outbound address translation region
 * @dev: root commpex
 * @nr: region number
 * @type: target transaction type, see PCIE20_CTRL1_TYPE_xxx
 * @host_addr: - region start address on host
 * @host_end: - region end address (low 32 bit) on host,
 *	upper 32 bits are same as for @host_addr
 * @target_addr: - region start address on target
 */
static void msm_pcie_iatu_config(struct msm_pcie_dev_t *dev, int nr, u8 type,
				unsigned long host_addr, u32 host_end,
				unsigned long target_addr)
{
	void __iomem *pcie20 = dev->dm_core;

	if (dev->shadow_en) {
		writel_relaxed(nr, dev->rc_shadow +
				PCIE20_PLR_IATU_VIEWPORT / 4);
		writel_relaxed(type, dev->rc_shadow +
				PCIE20_PLR_IATU_CTRL1 / 4);
		writel_relaxed(lower_32_bits(host_addr), dev->rc_shadow +
				PCIE20_PLR_IATU_LBAR / 4);
		writel_relaxed(upper_32_bits(host_addr), dev->rc_shadow +
				PCIE20_PLR_IATU_UBAR / 4);
		writel_relaxed(host_end, dev->rc_shadow +
				PCIE20_PLR_IATU_LAR / 4);
		writel_relaxed(lower_32_bits(target_addr), dev->rc_shadow +
				PCIE20_PLR_IATU_LTAR / 4);
		writel_relaxed(upper_32_bits(target_addr), dev->rc_shadow +
				PCIE20_PLR_IATU_UTAR / 4);
		writel_relaxed(BIT(31), dev->rc_shadow +
				PCIE20_PLR_IATU_CTRL2 / 4);
	}

	/* select region */
	writel_relaxed(nr, pcie20 + PCIE20_PLR_IATU_VIEWPORT);
	/* ensure that hardware locks it */
	wmb();

	/* switch off region before changing it */
	writel_relaxed(0, pcie20 + PCIE20_PLR_IATU_CTRL2);
	/* and wait till it propagates to the hardware */
	wmb();

	writel_relaxed(type, pcie20 + PCIE20_PLR_IATU_CTRL1);
	writel_relaxed(lower_32_bits(host_addr),
		       pcie20 + PCIE20_PLR_IATU_LBAR);
	writel_relaxed(upper_32_bits(host_addr),
		       pcie20 + PCIE20_PLR_IATU_UBAR);
	writel_relaxed(host_end, pcie20 + PCIE20_PLR_IATU_LAR);
	writel_relaxed(lower_32_bits(target_addr),
		       pcie20 + PCIE20_PLR_IATU_LTAR);
	writel_relaxed(upper_32_bits(target_addr),
		       pcie20 + PCIE20_PLR_IATU_UTAR);
	wmb();
	writel_relaxed(BIT(31), pcie20 + PCIE20_PLR_IATU_CTRL2);

	/* ensure that changes propagated to the hardware */
	wmb();

	if (dev->enumerated) {
		PCIE_DBG(dev, "IATU for Endpoint %02x:%02x.%01x\n",
			dev->pcidev_table[nr].bdf >> 24,
			dev->pcidev_table[nr].bdf >> 19 & 0x1f,
			dev->pcidev_table[nr].bdf >> 16 & 0x07);
		PCIE_DBG(dev, "PCIE20_PLR_IATU_VIEWPORT:0x%x\n",
			readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_VIEWPORT));
		PCIE_DBG(dev, "PCIE20_PLR_IATU_CTRL1:0x%x\n",
			readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_CTRL1));
		PCIE_DBG(dev, "PCIE20_PLR_IATU_LBAR:0x%x\n",
			readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_LBAR));
		PCIE_DBG(dev, "PCIE20_PLR_IATU_UBAR:0x%x\n",
			readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_UBAR));
		PCIE_DBG(dev, "PCIE20_PLR_IATU_LAR:0x%x\n",
			readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_LAR));
		PCIE_DBG(dev, "PCIE20_PLR_IATU_LTAR:0x%x\n",
			readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_LTAR));
		PCIE_DBG(dev, "PCIE20_PLR_IATU_UTAR:0x%x\n",
			readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_UTAR));
		PCIE_DBG(dev, "PCIE20_PLR_IATU_CTRL2:0x%x\n\n",
			readl_relaxed(dev->dm_core + PCIE20_PLR_IATU_CTRL2));
	}
}

/**
 * msm_pcie_cfg_bdf - configure for config access
 * @dev: root commpex
 * @bus: PCI bus number
 * @devfn: PCI dev and function number
 *
 * Remap if required region 0 for config access of proper type
 * (CFG0 for bus 1, CFG1 for other buses)
 * Cache current device bdf for speed-up
 */
static void msm_pcie_cfg_bdf(struct msm_pcie_dev_t *dev, u8 bus, u8 devfn)
{
	struct resource *axi_conf = dev->res[MSM_PCIE_RES_CONF].resource;
	u32 bdf  = BDF_OFFSET(bus, devfn);
	u8 type = bus == 1 ? PCIE20_CTRL1_TYPE_CFG0 : PCIE20_CTRL1_TYPE_CFG1;
	if (dev->current_bdf == bdf)
		return;

	msm_pcie_iatu_config(dev, 0, type,
			axi_conf->start,
			axi_conf->start + SZ_4K - 1,
			bdf);

	dev->current_bdf = bdf;
}

static inline void msm_pcie_save_shadow(struct msm_pcie_dev_t *dev,
					u32 word_offset, u32 wr_val,
					u32 bdf, bool rc)
{
	int i, j;
	u32 max_dev = MAX_RC_NUM * MAX_DEVICE_NUM;

	if (rc) {
		dev->rc_shadow[word_offset / 4] = wr_val;
	} else {
		for (i = 0; i < MAX_DEVICE_NUM; i++) {
			if (!dev->pcidev_table[i].bdf) {
				for (j = 0; j < max_dev; j++)
					if (!msm_pcie_dev_tbl[j].bdf) {
						msm_pcie_dev_tbl[j].bdf = bdf;
						break;
					}
				dev->pcidev_table[i].bdf = bdf;
			}
			if (dev->pcidev_table[i].bdf == bdf) {
				dev->ep_shadow[i][word_offset / 4] = wr_val;
				break;
			}
		}
	}
}

static inline int msm_pcie_oper_conf(struct pci_bus *bus, u32 devfn, int oper,
				     int where, int size, u32 *val)
{
	uint32_t word_offset, byte_offset, mask;
	uint32_t rd_val, wr_val;
	struct msm_pcie_dev_t *dev;
	void __iomem *config_base;
	bool rc = false;
	u32 rc_idx;
	int rv = 0;
	u32 bdf = BDF_OFFSET(bus->number, devfn);
	int i;

	dev = ((struct msm_pcie_dev_t *)
		(((struct pci_sys_data *)bus->sysdata)->private_data));

	if (!dev) {
		pr_err("PCIe: No device found for this bus.\n");
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto out;
	}

	rc_idx = dev->rc_idx;
	rc = (bus->number == 0);

	spin_lock_irqsave(&dev->cfg_lock, dev->irqsave_flags);

	if (!dev->cfg_access) {
		PCIE_DBG(dev, "Access denied for RC%d %d:0x%02x + 0x%04x[%d]\n",
			rc_idx, bus->number, devfn, where, size);
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto unlock;
	}

	if (rc && (devfn != 0)) {
		PCIE_DBG(dev, "RC%d invalid %s - bus %d devfn %d\n", rc_idx,
			 (oper == RD) ? "rd" : "wr", bus->number, devfn);
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto unlock;
	}

	if (dev->link_status != MSM_PCIE_LINK_ENABLED) {
		PCIE_DBG(dev,
			"Access to RC%d %d:0x%02x + 0x%04x[%d] is denied because link is down\n",
			rc_idx, bus->number, devfn, where, size);
		*val = ~0;
		rv = PCIBIOS_DEVICE_NOT_FOUND;
		goto unlock;
	}

	/* check if the link is up for endpoint */
	if (!rc && !msm_pcie_is_link_up(dev)) {
		PCIE_ERR(dev,
			"PCIe: RC%d %s fail, link down - bus %d devfn %d\n",
				rc_idx, (oper == RD) ? "rd" : "wr",
				bus->number, devfn);
			*val = ~0;
			rv = PCIBIOS_DEVICE_NOT_FOUND;
			goto unlock;
	}

	if (!rc && !dev->enumerated)
		msm_pcie_cfg_bdf(dev, bus->number, devfn);

	word_offset = where & ~0x3;
	byte_offset = where & 0x3;
	mask = (~0 >> (8 * (4 - size))) << (8 * byte_offset);

	if (rc || !dev->enumerated) {
		config_base = rc ? dev->dm_core : dev->conf;
	} else {
		for (i = 0; i < MAX_DEVICE_NUM; i++) {
			if (dev->pcidev_table[i].bdf == bdf) {
				config_base = dev->pcidev_table[i].conf_base;
				break;
			}
		}
		if (i == MAX_DEVICE_NUM) {
			*val = ~0;
			rv = PCIBIOS_DEVICE_NOT_FOUND;
			goto unlock;
		}
	}

	rd_val = readl_relaxed(config_base + word_offset);

	if (oper == RD) {
		*val = ((rd_val & mask) >> (8 * byte_offset));
		PCIE_DBG(dev,
			"RC%d %d:0x%02x + 0x%04x[%d] -> 0x%08x; rd 0x%08x\n",
			rc_idx, bus->number, devfn, where, size, *val, rd_val);
	} else {
		wr_val = (rd_val & ~mask) |
				((*val << (8 * byte_offset)) & mask);
		writel_relaxed(wr_val, config_base + word_offset);
		wmb(); /* ensure config data is written to hardware register */

		if (rd_val == PCIE_LINK_DOWN)
			PCIE_ERR(dev,
				"Read of RC%d %d:0x%02x + 0x%04x[%d] is all FFs\n",
				rc_idx, bus->number, devfn, where, size);
		else if (dev->shadow_en)
			msm_pcie_save_shadow(dev, word_offset, wr_val, bdf, rc);

		PCIE_DBG(dev,
			"RC%d %d:0x%02x + 0x%04x[%d] <- 0x%08x; rd 0x%08x val 0x%08x\n",
			rc_idx, bus->number, devfn, where, size,
			wr_val, rd_val, *val);
	}

unlock:
	spin_unlock_irqrestore(&dev->cfg_lock, dev->irqsave_flags);
out:
	return rv;
}

static int msm_pcie_rd_conf(struct pci_bus *bus, u32 devfn, int where,
			    int size, u32 *val)
{
	int ret = msm_pcie_oper_conf(bus, devfn, RD, where, size, val);

	if ((bus->number == 0) && (where == PCI_CLASS_REVISION)) {
		*val = (*val & 0xff) | (PCI_CLASS_BRIDGE_PCI << 16);
		PCIE_GEN_DBG("change class for RC:0x%x\n", *val);
	}

	return ret;
}

static int msm_pcie_wr_conf(struct pci_bus *bus, u32 devfn,
			    int where, int size, u32 val)
{
	return msm_pcie_oper_conf(bus, devfn, WR, where, size, &val);
}

static struct pci_ops msm_pcie_ops = {
	.read = msm_pcie_rd_conf,
	.write = msm_pcie_wr_conf,
};

static int msm_pcie_gpio_init(struct msm_pcie_dev_t *dev)
{
	int rc, i;
	struct msm_pcie_gpio_info_t *info;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = 0; i < dev->gpio_n; i++) {
		info = &dev->gpio[i];

		if (!info->num)
			continue;

		rc = gpio_request(info->num, info->name);
		if (rc) {
			PCIE_ERR(dev, "PCIe: RC%d can't get gpio %s; %d\n",
				dev->rc_idx, info->name, rc);
			break;
		}

		if (info->out)
			rc = gpio_direction_output(info->num, info->init);
		else
			rc = gpio_direction_input(info->num);
		if (rc) {
			PCIE_ERR(dev,
				"PCIe: RC%d can't set direction for GPIO %s:%d\n",
				dev->rc_idx, info->name, rc);
			gpio_free(info->num);
			break;
		}
	}

	if (rc)
		while (i--)
			gpio_free(dev->gpio[i].num);

	return rc;
}

static void msm_pcie_gpio_deinit(struct msm_pcie_dev_t *dev)
{
	int i;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = 0; i < dev->gpio_n; i++)
		gpio_free(dev->gpio[i].num);
}

int msm_pcie_vreg_init(struct msm_pcie_dev_t *dev)
{
	int i, rc = 0;
	struct regulator *vreg;
	struct msm_pcie_vreg_info_t *info;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = 0; i < MSM_PCIE_MAX_VREG; i++) {
		info = &dev->vreg[i];
		vreg = info->hdl;

		if (!vreg)
			continue;

		PCIE_DBG(dev, "RC%d Vreg %s is being enabled\n",
			dev->rc_idx, info->name);
		if (info->max_v) {
			rc = regulator_set_voltage(vreg,
						   info->min_v, info->max_v);
			if (rc) {
				PCIE_ERR(dev,
					"PCIe: RC%d can't set voltage for %s: %d\n",
					dev->rc_idx, info->name, rc);
				break;
			}
		}

		if (info->opt_mode) {
			rc = regulator_set_optimum_mode(vreg, info->opt_mode);
			if (rc < 0) {
				PCIE_ERR(dev,
					"PCIe: RC%d can't set mode for %s: %d\n",
					dev->rc_idx, info->name, rc);
				break;
			}
		}

		rc = regulator_enable(vreg);
		if (rc) {
			PCIE_ERR(dev,
				"PCIe: RC%d can't enable regulator %s: %d\n",
				dev->rc_idx, info->name, rc);
			break;
		}
	}

	if (rc)
		while (i--) {
			struct regulator *hdl = dev->vreg[i].hdl;
			if (hdl)
				regulator_disable(hdl);
		}

	return rc;
}

static void msm_pcie_vreg_deinit(struct msm_pcie_dev_t *dev)
{
	int i;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = MSM_PCIE_MAX_VREG - 1; i >= 0; i--) {
		if (dev->vreg[i].hdl) {
			PCIE_DBG(dev, "Vreg %s is being disabled\n",
				dev->vreg[i].name);
			regulator_disable(dev->vreg[i].hdl);
		}
	}
}

static int msm_pcie_clk_init(struct msm_pcie_dev_t *dev)
{
	int i, rc = 0;
	struct msm_pcie_clk_info_t *info;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	rc = regulator_enable(dev->gdsc);

	if (rc) {
		PCIE_ERR(dev, "PCIe: fail to enable GDSC for RC%d (%s)\n",
			dev->rc_idx, dev->pdev->name);
		return rc;
	}

	if (dev->bus_client) {
		rc = msm_bus_scale_client_update_request(dev->bus_client, 1);
		if (rc) {
			PCIE_ERR(dev,
				"PCIe: fail to set bus bandwidth for RC%d:%d.\n",
				dev->rc_idx, rc);
			return rc;
		} else {
			PCIE_DBG(dev,
				"PCIe: set bus bandwidth for RC%d.\n",
				dev->rc_idx);
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_CLK; i++) {
		info = &dev->clk[i];

		if (!info->hdl)
			continue;

		if (i == MSM_PCIE_MAX_CLK-1)
			clk_reset(info->hdl, CLK_RESET_DEASSERT);

		if (info->freq) {
			rc = clk_set_rate(info->hdl, info->freq);
			if (rc) {
				PCIE_ERR(dev,
					"PCIe: RC%d can't set rate for clk %s: %d.\n",
					dev->rc_idx, info->name, rc);
				break;
			} else {
				PCIE_DBG(dev,
					"PCIe: RC%d set rate for clk %s.\n",
					dev->rc_idx, info->name);
			}
		}

		rc = clk_prepare_enable(info->hdl);

		if (rc)
			PCIE_ERR(dev, "PCIe: RC%d failed to enable clk %s\n",
				dev->rc_idx, info->name);
		else
			PCIE_DBG(dev, "enable clk %s for RC%d.\n",
				info->name, dev->rc_idx);
	}

	if (rc) {
		PCIE_DBG(dev, "RC%d disable clocks for error handling.\n",
			dev->rc_idx);
		while (i--) {
			struct clk *hdl = dev->clk[i].hdl;
			if (hdl)
				clk_disable_unprepare(hdl);
		}

		regulator_disable(dev->gdsc);
	}

	return rc;
}

static void msm_pcie_clk_deinit(struct msm_pcie_dev_t *dev)
{
	int i;
	int rc;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = 0; i < MSM_PCIE_MAX_CLK; i++)
		if (dev->clk[i].hdl)
			clk_disable_unprepare(dev->clk[i].hdl);

	if (dev->bus_client) {
		rc = msm_bus_scale_client_update_request(dev->bus_client, 0);
		if (rc)
			PCIE_ERR(dev,
				"PCIe: fail to relinquish bus bandwidth for RC%d:%d.\n",
				dev->rc_idx, rc);
		else
			PCIE_DBG(dev,
				"PCIe: relinquish bus bandwidth for RC%d.\n",
				dev->rc_idx);
	}

	regulator_disable(dev->gdsc);
}

static int msm_pcie_pipe_clk_init(struct msm_pcie_dev_t *dev)
{
	int i, rc = 0;
	struct msm_pcie_clk_info_t *info;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++) {
		info = &dev->pipeclk[i];

		if (!info->hdl)
			continue;

		clk_reset(info->hdl, CLK_RESET_DEASSERT);

		if (info->freq) {
			rc = clk_set_rate(info->hdl, info->freq);
			if (rc) {
				PCIE_ERR(dev,
					"PCIe: RC%d can't set rate for clk %s: %d.\n",
					dev->rc_idx, info->name, rc);
				break;
			} else {
				PCIE_DBG(dev,
					"PCIe: RC%d set rate for clk %s: %d.\n",
					dev->rc_idx, info->name, rc);
			}
		}

		rc = clk_prepare_enable(info->hdl);

		if (rc)
			PCIE_ERR(dev, "PCIe: RC%d failed to enable clk %s.\n",
				dev->rc_idx, info->name);
		else
			PCIE_DBG(dev, "RC%d enabled pipe clk %s.\n",
				dev->rc_idx, info->name);
	}

	if (rc) {
		PCIE_DBG(dev, "RC%d disable pipe clocks for error handling.\n",
			dev->rc_idx);
		while (i--)
			if (dev->pipeclk[i].hdl)
				clk_disable_unprepare(dev->pipeclk[i].hdl);
	}

	return rc;
}

static void msm_pcie_pipe_clk_deinit(struct msm_pcie_dev_t *dev)
{
	int i;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++)
		if (dev->pipeclk[i].hdl)
			clk_disable_unprepare(
				dev->pipeclk[i].hdl);
}

static void msm_pcie_iatu_config_all_ep(struct msm_pcie_dev_t *dev)
{
	int i;
	u8 type;
	struct msm_pcie_device_info *dev_table = dev->pcidev_table;

	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		if (!dev_table[i].bdf)
			break;

		type = dev_table[i].bdf >> 24 == 0x1 ?
			PCIE20_CTRL1_TYPE_CFG0 : PCIE20_CTRL1_TYPE_CFG1;

		msm_pcie_iatu_config(dev, i, type, dev_table[i].phy_address,
			dev_table[i].phy_address + SZ_4K - 1,
			dev_table[i].bdf);
	}
}

static void msm_pcie_config_controller(struct msm_pcie_dev_t *dev)
{
	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	/*
	 * program and enable address translation region 0 (device config
	 * address space); region type config;
	 * axi config address range to device config address range
	 */
	if (dev->enumerated) {
		msm_pcie_iatu_config_all_ep(dev);
	} else {
		dev->current_bdf = 0; /* to force IATU re-config */
		msm_pcie_cfg_bdf(dev, 1, 0);
	}

	/* configure N_FTS */
	PCIE_DBG(dev, "Original PCIE20_ACK_F_ASPM_CTRL_REG:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_ACK_F_ASPM_CTRL_REG));
	if (!dev->n_fts)
		msm_pcie_write_mask(dev->dm_core + PCIE20_ACK_F_ASPM_CTRL_REG,
					0, BIT(15));
	else
		msm_pcie_write_mask(dev->dm_core + PCIE20_ACK_F_ASPM_CTRL_REG,
					PCIE20_ACK_N_FTS,
					dev->n_fts << 8);

	if (dev->shadow_en) {
		if (!dev->n_fts)
			msm_pcie_write_mask(dev->rc_shadow +
				PCIE20_ACK_F_ASPM_CTRL_REG / 4, 0, BIT(15));
		else
			msm_pcie_write_mask(dev->rc_shadow +
				PCIE20_ACK_F_ASPM_CTRL_REG / 4,
				PCIE20_ACK_N_FTS, dev->n_fts << 8);
	}

	PCIE_DBG(dev, "Updated PCIE20_ACK_F_ASPM_CTRL_REG:0x%x\n",
		readl_relaxed(dev->dm_core + PCIE20_ACK_F_ASPM_CTRL_REG));
}

static void msm_pcie_config_link_state(struct msm_pcie_dev_t *dev)
{
	u32 offset = 0;

	if (dev->rc_idx)
		offset = PCIE20_EP_L1SUB_CTL1_OFFSET;

	/* Enable the AUX Clock and the Core Clk to be synchronous for L1SS*/
	if (!dev->aux_clk_sync && dev->l1ss_supported)
		msm_pcie_write_mask(dev->parf +
				PCIE20_PARF_SYS_CTRL, BIT(3), 0);

	if (dev->l0s_supported) {
		msm_pcie_write_mask(dev->dm_core + PCIE20_CAP_LINKCTRLSTATUS,
					0, BIT(0));
		msm_pcie_write_mask(dev->conf + PCIE20_CAP_LINKCTRLSTATUS,
					0, BIT(0));
		if (dev->shadow_en) {
			msm_pcie_write_mask(dev->rc_shadow +
						PCIE20_CAP_LINKCTRLSTATUS / 4,
						0, BIT(0));
			msm_pcie_write_mask(dev->ep_shadow[0] +
						PCIE20_CAP_LINKCTRLSTATUS / 4,
						0, BIT(0));
		}
		PCIE_DBG(dev, "RC's CAP_LINKCTRLSTATUS:0x%x\n",
			readl_relaxed(dev->dm_core +
			PCIE20_CAP_LINKCTRLSTATUS));
		PCIE_DBG(dev, "EP's CAP_LINKCTRLSTATUS:0x%x\n",
			readl_relaxed(dev->conf + PCIE20_CAP_LINKCTRLSTATUS));
	}

	if (dev->l1_supported) {
		msm_pcie_write_mask(dev->dm_core + PCIE20_CAP_LINKCTRLSTATUS,
					0, BIT(1));
		msm_pcie_write_mask(dev->conf + PCIE20_CAP_LINKCTRLSTATUS,
					0, BIT(1));
		if (dev->shadow_en) {
			msm_pcie_write_mask(dev->rc_shadow +
						PCIE20_CAP_LINKCTRLSTATUS / 4,
						0, BIT(1));
			msm_pcie_write_mask(dev->ep_shadow[0] +
						PCIE20_CAP_LINKCTRLSTATUS / 4,
						0, BIT(1));
		}
		PCIE_DBG(dev, "RC's CAP_LINKCTRLSTATUS:0x%x\n",
			readl_relaxed(dev->dm_core +
			PCIE20_CAP_LINKCTRLSTATUS));
		PCIE_DBG(dev, "EP's CAP_LINKCTRLSTATUS:0x%x\n",
			readl_relaxed(dev->conf + PCIE20_CAP_LINKCTRLSTATUS));
	}

	if (dev->l1ss_supported) {
		msm_pcie_write_mask(dev->dm_core + PCIE20_L1SUB_CONTROL1, 0,
					BIT(3)|BIT(2)|BIT(1)|BIT(0));
		msm_pcie_write_mask(dev->dm_core +
					PCIE20_DEVICE_CONTROL2_STATUS2,
					0, BIT(10));
		msm_pcie_write_mask(dev->conf + PCIE20_L1SUB_CONTROL1 +
					offset, 0,
					BIT(3)|BIT(2)|BIT(1)|BIT(0));
		msm_pcie_write_mask(dev->conf + PCIE20_DEVICE_CONTROL2_STATUS2,
					0, BIT(10));
		if (dev->shadow_en) {
			msm_pcie_write_mask(dev->rc_shadow +
					PCIE20_L1SUB_CONTROL1 / 4, 0,
					BIT(3)|BIT(2)|BIT(1)|BIT(0));
			msm_pcie_write_mask(dev->rc_shadow +
					PCIE20_DEVICE_CONTROL2_STATUS2 / 4,
					0, BIT(10));
			msm_pcie_write_mask(dev->ep_shadow[0] +
					PCIE20_L1SUB_CONTROL1 / 4 +
					PCIE20_EP_L1SUB_CTL1_OFFSET / 4, 0,
					BIT(3)|BIT(2)|BIT(1)|BIT(0));
			msm_pcie_write_mask(dev->ep_shadow[0] +
					PCIE20_DEVICE_CONTROL2_STATUS2 / 4,
					0, BIT(10));
		}
		PCIE_DBG(dev, "RC's L1SUB_CONTROL1:0x%x\n",
			readl_relaxed(dev->dm_core + PCIE20_L1SUB_CONTROL1));
		PCIE_DBG(dev, "RC's DEVICE_CONTROL2_STATUS2:0x%x\n",
			readl_relaxed(dev->dm_core +
			PCIE20_DEVICE_CONTROL2_STATUS2));
		PCIE_DBG(dev, "EP's L1SUB_CONTROL1:0x%x\n",
			readl_relaxed(dev->conf + PCIE20_L1SUB_CONTROL1 +
			offset));
		PCIE_DBG(dev, "EP's DEVICE_CONTROL2_STATUS2:0x%x\n",
			readl_relaxed(dev->conf +
			PCIE20_DEVICE_CONTROL2_STATUS2));
	}
}

void msm_pcie_config_msi_controller(struct msm_pcie_dev_t *dev)
{
	int i;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	/* program MSI controller and enable all interrupts */
	writel_relaxed(MSM_PCIE_MSI_PHY, dev->dm_core + PCIE20_MSI_CTRL_ADDR);
	writel_relaxed(0, dev->dm_core + PCIE20_MSI_CTRL_UPPER_ADDR);

	for (i = 0; i < PCIE20_MSI_CTRL_MAX; i++)
		writel_relaxed(~0, dev->dm_core +
			       PCIE20_MSI_CTRL_INTR_EN + (i * 12));

	/* ensure that hardware is configured before proceeding */
	wmb();
}

static int msm_pcie_get_resources(struct msm_pcie_dev_t *dev,
					struct platform_device *pdev)
{
	int i, len, cnt, ret = 0;
	struct msm_pcie_vreg_info_t *vreg_info;
	struct msm_pcie_gpio_info_t *gpio_info;
	struct msm_pcie_clk_info_t  *clk_info;
	struct resource *res;
	struct msm_pcie_res_info_t *res_info;
	struct msm_pcie_irq_info_t *irq_info;
	char prop_name[MAX_PROP_SIZE];
	const __be32 *prop;
	u32 *clkfreq = NULL;

	cnt = of_property_count_strings((&pdev->dev)->of_node,
			"clock-names");
	if (cnt > 0) {
		clkfreq = kzalloc(cnt * sizeof(*clkfreq),
					GFP_KERNEL);
		if (!clkfreq) {
			PCIE_ERR(dev, "PCIe: memory alloc failed for RC%d\n",
					dev->rc_idx);
			return -ENOMEM;
		}
		ret = of_property_read_u32_array(
			(&pdev->dev)->of_node,
			"max-clock-frequency-hz", clkfreq, cnt);
		if (ret) {
			PCIE_ERR(dev,
				"PCIe: invalid max-clock-frequency-hz property for RC%d:%d\n",
				dev->rc_idx, ret);
			goto out;
		}
	}

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (i = 0; i < MSM_PCIE_MAX_VREG; i++) {
		vreg_info = &dev->vreg[i];
		vreg_info->hdl =
				devm_regulator_get(&pdev->dev, vreg_info->name);

		if (PTR_ERR(vreg_info->hdl) == -EPROBE_DEFER) {
			PCIE_DBG(dev, "EPROBE_DEFER for VReg:%s\n",
				vreg_info->name);
			ret = PTR_ERR(vreg_info->hdl);
			goto out;
		}

		if (IS_ERR(vreg_info->hdl)) {
			if (vreg_info->required) {
				PCIE_DBG(dev, "Vreg %s doesn't exist\n",
					vreg_info->name);
				ret = PTR_ERR(vreg_info->hdl);
				goto out;
			} else {
				PCIE_DBG(dev,
					"Optional Vreg %s doesn't exist\n",
					vreg_info->name);
				vreg_info->hdl = NULL;
			}
		} else {
			dev->vreg_n++;
			snprintf(prop_name, MAX_PROP_SIZE,
				"qcom,%s-voltage-level", vreg_info->name);
			prop = of_get_property((&pdev->dev)->of_node,
						prop_name, &len);
			if (!prop || (len != (3 * sizeof(__be32)))) {
				PCIE_DBG(dev, "%s %s property\n",
					prop ? "invalid format" :
					"no", prop_name);
			} else {
				vreg_info->max_v = be32_to_cpup(&prop[0]);
				vreg_info->min_v = be32_to_cpup(&prop[1]);
				vreg_info->opt_mode =
					be32_to_cpup(&prop[2]);
			}
		}
	}

	dev->gdsc = devm_regulator_get(&pdev->dev, "gdsc-vdd");

	if (IS_ERR(dev->gdsc)) {
		PCIE_ERR(dev, "PCIe: RC%d Failed to get %s GDSC:%ld\n",
			dev->rc_idx, dev->pdev->name, PTR_ERR(dev->gdsc));
		if (PTR_ERR(dev->gdsc) == -EPROBE_DEFER)
			PCIE_DBG(dev, "PCIe: EPROBE_DEFER for %s GDSC\n",
					dev->pdev->name);
		ret = PTR_ERR(dev->gdsc);
		goto out;
	}

	dev->gpio_n = 0;
	for (i = 0; i < MSM_PCIE_MAX_GPIO; i++) {
		gpio_info = &dev->gpio[i];
		ret = of_get_named_gpio((&pdev->dev)->of_node,
					gpio_info->name, 0);
		if (ret >= 0) {
			gpio_info->num = ret;
			ret = 0;
			dev->gpio_n++;
			PCIE_DBG(dev, "GPIO num for %s is %d\n",
				gpio_info->name, gpio_info->num);
		} else {
			goto out;
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_CLK; i++) {
		clk_info = &dev->clk[i];

		clk_info->hdl = devm_clk_get(&pdev->dev, clk_info->name);

		if (IS_ERR(clk_info->hdl)) {
			if (clk_info->required) {
				PCIE_DBG(dev, "Clock %s isn't available:%ld\n",
				clk_info->name, PTR_ERR(clk_info->hdl));
				ret = PTR_ERR(clk_info->hdl);
				goto out;
			} else {
				PCIE_DBG(dev, "Ignoring Clock %s\n",
					clk_info->name);
				clk_info->hdl = NULL;
			}
		} else {
			if (clkfreq != NULL) {
				clk_info->freq = clkfreq[i +
					MSM_PCIE_MAX_PIPE_CLK];
				PCIE_DBG(dev, "Freq of Clock %s is:%d\n",
					clk_info->name, clk_info->freq);
			}
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_PIPE_CLK; i++) {
		clk_info = &dev->pipeclk[i];

		clk_info->hdl = devm_clk_get(&pdev->dev, clk_info->name);

		if (IS_ERR(clk_info->hdl)) {
			if (clk_info->required) {
				PCIE_DBG(dev, "Clock %s isn't available:%ld\n",
				clk_info->name, PTR_ERR(clk_info->hdl));
				ret = PTR_ERR(clk_info->hdl);
				goto out;
			} else {
				PCIE_DBG(dev, "Ignoring Clock %s\n",
					clk_info->name);
				clk_info->hdl = NULL;
			}
		} else {
			if (clkfreq != NULL) {
				clk_info->freq = clkfreq[i];
				PCIE_DBG(dev, "Freq of Clock %s is:%d\n",
					clk_info->name, clk_info->freq);
			}
		}
	}


	dev->bus_scale_table = msm_bus_cl_get_pdata(pdev);
	if (!dev->bus_scale_table) {
		PCIE_DBG(dev, "PCIe: No bus scale table for RC%d (%s)\n",
			dev->rc_idx, dev->pdev->name);
		dev->bus_client = 0;
	} else {
		dev->bus_client =
			msm_bus_scale_register_client(dev->bus_scale_table);
		if (!dev->bus_client) {
			PCIE_ERR(dev,
				"PCIe: Failed to register bus client for RC%d (%s)\n",
				dev->rc_idx, dev->pdev->name);
			msm_bus_cl_clear_pdata(dev->bus_scale_table);
			ret = -ENODEV;
			goto out;
		}
	}

	for (i = 0; i < MSM_PCIE_MAX_RES; i++) {
		res_info = &dev->res[i];

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							   res_info->name);

		if (!res) {
			PCIE_ERR(dev, "PCIe: RC%d can't get %s resource.\n",
				dev->rc_idx, res_info->name);
			ret = -ENOMEM;
			goto out;
		} else
			PCIE_DBG(dev, "start addr for %s is %pa.\n",
				res_info->name,	&res->start);

		res_info->base = devm_ioremap(&pdev->dev,
						res->start, resource_size(res));
		if (!res_info->base) {
			PCIE_ERR(dev, "PCIe: RC%d can't remap %s.\n",
				dev->rc_idx, res_info->name);
			ret = -ENOMEM;
			goto out;
		}
		res_info->resource = res;
	}

	for (i = 0; i < MSM_PCIE_MAX_IRQ; i++) {
		irq_info = &dev->irq[i];

		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
							   irq_info->name);

		if (!res) {
			int j;
			for (j = 0; j < MSM_PCIE_MAX_RES; j++) {
				iounmap(dev->res[j].base);
				dev->res[j].base = NULL;
			}
			PCIE_ERR(dev, "PCIe: RC%d can't find IRQ # for %s.\n",
				dev->rc_idx, irq_info->name);
			ret = -ENODEV;
			goto out;
		} else {
			irq_info->num = res->start;
			PCIE_DBG(dev, "IRQ # for %s is %d.\n", irq_info->name,
					irq_info->num);
		}
	}

	/* All allocations succeeded */

	dev->wake_n = gpio_to_irq(dev->gpio[MSM_PCIE_GPIO_WAKE].num);

	dev->parf = dev->res[MSM_PCIE_RES_PARF].base;
	dev->phy = dev->res[MSM_PCIE_RES_PHY].base;
	dev->elbi = dev->res[MSM_PCIE_RES_ELBI].base;
	dev->dm_core = dev->res[MSM_PCIE_RES_DM_CORE].base;
	dev->conf = dev->res[MSM_PCIE_RES_CONF].base;
	dev->bars = dev->res[MSM_PCIE_RES_BARS].base;
	dev->dev_mem_res = dev->res[MSM_PCIE_RES_BARS].resource;
	dev->dev_io_res = dev->res[MSM_PCIE_RES_IO].resource;
	dev->dev_io_res->flags = IORESOURCE_IO;

out:
	kfree(clkfreq);
	return ret;
}

static void msm_pcie_release_resources(struct msm_pcie_dev_t *dev)
{
	dev->parf = NULL;
	dev->elbi = NULL;
	dev->dm_core = NULL;
	dev->conf = NULL;
	dev->bars = NULL;
	dev->dev_mem_res = NULL;
	dev->dev_io_res = NULL;
}

int msm_pcie_enable(struct msm_pcie_dev_t *dev, u32 options)
{
	int ret = 0;
	uint32_t val;
	long int retries = 0;
	int link_check_count = 0;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	mutex_lock(&dev->setup_lock);

	if (dev->link_status == MSM_PCIE_LINK_ENABLED) {
		PCIE_ERR(dev, "PCIe: the link of RC%d is already enabled\n",
			dev->rc_idx);
		goto out;
	}

	/* assert PCIe reset link to keep EP in reset */

	PCIE_INFO(dev, "PCIe: Assert the reset of endpoint of RC%d.\n",
		dev->rc_idx);
	gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				dev->gpio[MSM_PCIE_GPIO_PERST].on);
	usleep_range(PERST_PROPAGATION_DELAY_US_MIN,
				 PERST_PROPAGATION_DELAY_US_MAX);

	/* enable power */

	if (options & PM_VREG) {
		ret = msm_pcie_vreg_init(dev);
		if (ret)
			goto out;
	}

	/* enable clocks */
	if (options & PM_CLK) {
		ret = msm_pcie_clk_init(dev);
		wmb();
		if (ret)
			goto clk_fail;
	}

	/* enable PCIe clocks and resets */
	msm_pcie_write_mask(dev->parf + PCIE20_PARF_PHY_CTRL, BIT(0), 0);

	/* change DBI base address */
	writel_relaxed(0, dev->parf + PCIE20_PARF_DBI_BASE_ADDR);

	writel_relaxed(0x3656, dev->parf + PCIE20_PARF_SYS_CTRL);

	if (dev->dev_mem_res->end - dev->dev_mem_res->start > SZ_8M)
		writel_relaxed(SZ_16M, dev->parf +
			PCIE20_PARF_SLV_ADDR_SPACE_SIZE);

	if (dev->use_msi) {
		PCIE_DBG(dev, "RC%d: enable WR halt.\n", dev->rc_idx);
		msm_pcie_write_mask(dev->parf +
			PCIE20_PARF_AXI_MSTR_WR_ADDR_HALT, 0, BIT(31));
	}

	/* init PCIe PHY */
	pcie_phy_init(dev);

	if (options & PM_PIPE_CLK) {
		usleep_range(PHY_STABILIZATION_DELAY_US_MIN,
					 PHY_STABILIZATION_DELAY_US_MAX);
		/* Enable the pipe clock */
		ret = msm_pcie_pipe_clk_init(dev);
		wmb();
		if (ret)
			goto link_fail;
	}

	PCIE_DBG(dev, "RC%d: waiting for phy ready...\n", dev->rc_idx);

	do {
		if (pcie_phy_is_ready(dev))
			break;
		retries++;
		usleep_range(REFCLK_STABILIZATION_DELAY_US_MIN,
					 REFCLK_STABILIZATION_DELAY_US_MAX);
	} while (retries < PHY_READY_TIMEOUT_COUNT);

	PCIE_DBG(dev, "RC%d: number of PHY retries:%ld.\n",
		dev->rc_idx, retries);

	if (pcie_phy_is_ready(dev))
		PCIE_INFO(dev, "PCIe RC%d PHY is ready!\n", dev->rc_idx);
	else {
		PCIE_ERR(dev, "PCIe PHY RC%d failed to come up!\n",
			dev->rc_idx);
		ret = -ENODEV;
		pcie_phy_dump(dev);
		goto link_fail;
	}

	if (dev->ep_latency)
		msleep(dev->ep_latency);

	/* de-assert PCIe reset link to bring EP out of reset */

	PCIE_INFO(dev, "PCIe: Release the reset of endpoint of RC%d.\n",
		dev->rc_idx);
	gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				1 - dev->gpio[MSM_PCIE_GPIO_PERST].on);
	usleep_range(PERST_PROPAGATION_DELAY_US_MIN,
				 PERST_PROPAGATION_DELAY_US_MAX);

	/* enable link training */
	msm_pcie_write_mask(dev->parf + PCIE20_PARF_LTSSM, 0, BIT(8));

	PCIE_DBG(dev, "%s", "check if link is up\n");

	/* Wait for up to 100ms for the link to come up */
	do {
		usleep_range(LINK_UP_TIMEOUT_US_MIN, LINK_UP_TIMEOUT_US_MAX);
		val =  readl_relaxed(dev->elbi + PCIE20_ELBI_SYS_STTS);
	} while ((!(val & XMLH_LINK_UP) ||
		!msm_pcie_confirm_linkup(dev, false, false))
		&& (link_check_count++ < LINK_UP_CHECK_MAX_COUNT));

	if ((val & XMLH_LINK_UP) &&
		msm_pcie_confirm_linkup(dev, false, false)) {
		PCIE_DBG(dev, "Link is up after %d checkings\n",
			link_check_count);
		PCIE_INFO(dev, "PCIe RC%d link initialized\n", dev->rc_idx);
	} else {
		PCIE_INFO(dev, "PCIe: Assert the reset of endpoint of RC%d.\n",
			dev->rc_idx);
		gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
			dev->gpio[MSM_PCIE_GPIO_PERST].on);
		PCIE_ERR(dev, "PCIe RC%d link initialization failed\n",
			dev->rc_idx);
		ret = -1;
		goto link_fail;
	}

	msm_pcie_config_controller(dev);

	if (!dev->msi_gicm_addr)
		msm_pcie_config_msi_controller(dev);

	msm_pcie_config_link_state(dev);

	dev->link_status = MSM_PCIE_LINK_ENABLED;
	dev->power_on = true;
	dev->suspending = false;
	goto out;

link_fail:
	msm_pcie_clk_deinit(dev);
clk_fail:
	msm_pcie_vreg_deinit(dev);
	msm_pcie_pipe_clk_deinit(dev);
out:
	mutex_unlock(&dev->setup_lock);

	return ret;
}

void msm_pcie_disable(struct msm_pcie_dev_t *dev, u32 options)
{
	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	mutex_lock(&dev->setup_lock);

	if (!dev->power_on) {
		PCIE_DBG(dev,
			"PCIe: the link of RC%d is already power down.\n",
			dev->rc_idx);
		mutex_unlock(&dev->setup_lock);
		return;
	}

	dev->link_status = MSM_PCIE_LINK_DISABLED;
	dev->power_on = false;

	PCIE_INFO(dev, "PCIe: Assert the reset of endpoint of RC%d.\n",
		dev->rc_idx);

	gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				dev->gpio[MSM_PCIE_GPIO_PERST].on);

	if (options & PM_CLK) {
		msm_pcie_write_mask(dev->parf + PCIE20_PARF_PHY_CTRL, 0,
					BIT(0));
		msm_pcie_clk_deinit(dev);
	}

	if (options & PM_VREG)
		msm_pcie_vreg_deinit(dev);

	if (options & PM_PIPE_CLK)
		msm_pcie_pipe_clk_deinit(dev);

	mutex_unlock(&dev->setup_lock);
}

static int msm_pcie_setup(int nr, struct pci_sys_data *sys)
{
	struct msm_pcie_dev_t *dev =
			(struct msm_pcie_dev_t *)(sys->private_data);

	PCIE_DBG(dev, "bus %d\n", nr);
	/*
	 * specify linux PCI framework to allocate device memory (BARs)
	 * from msm_pcie_dev.dev_mem_res resource.
	 */
	sys->mem_offset = 0;
	sys->io_offset = 0;

	pci_add_resource(&sys->resources, dev->dev_io_res);
	pci_add_resource(&sys->resources, dev->dev_mem_res);
	return 1;
}

static struct pci_bus *msm_pcie_scan_bus(int nr,
						struct pci_sys_data *sys)
{
	struct pci_bus *bus = NULL;
	struct msm_pcie_dev_t *dev =
			(struct msm_pcie_dev_t *)(sys->private_data);

	PCIE_DBG(dev, "bus %d\n", nr);

	bus = pci_scan_root_bus(NULL, sys->busnr, &msm_pcie_ops, sys,
					&sys->resources);

	return bus;
}

static int msm_pcie_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);
	int ret = 0;

	PCIE_DBG(pcie_dev, "rc %s slot %d pin %d\n", pcie_dev->pdev->name,
		slot, pin);

	switch (pin) {
	case 1:
		ret = pcie_dev->irq[MSM_PCIE_INT_A].num;
		break;
	case 2:
		ret = pcie_dev->irq[MSM_PCIE_INT_B].num;
		break;
	case 3:
		ret = pcie_dev->irq[MSM_PCIE_INT_C].num;
		break;
	case 4:
		ret = pcie_dev->irq[MSM_PCIE_INT_D].num;
		break;
	default:
		PCIE_ERR(pcie_dev, "PCIe: RC%d: unsupported pin number.\n",
			pcie_dev->rc_idx);
	}

	return ret;
}


static struct hw_pci msm_pci[MAX_RC_NUM] = {
	{
	.domain = 0,
	.nr_controllers	= 1,
	.swizzle	= pci_common_swizzle,
	.setup		= msm_pcie_setup,
	.scan		= msm_pcie_scan_bus,
	.map_irq	= msm_pcie_map_irq,
	},
	{
	.domain = 1,
	.nr_controllers	= 1,
	.swizzle	= pci_common_swizzle,
	.setup		= msm_pcie_setup,
	.scan		= msm_pcie_scan_bus,
	.map_irq	= msm_pcie_map_irq,
	},
};

static int msm_pcie_config_device_table(struct device *dev, void *pdev)
{
	struct pci_dev *pcidev = to_pci_dev(dev);
	struct msm_pcie_dev_t *pcie_dev = (struct msm_pcie_dev_t *) pdev;
	struct msm_pcie_device_info *dev_table_t = pcie_dev->pcidev_table;
	struct resource *axi_conf = pcie_dev->res[MSM_PCIE_RES_CONF].resource;
	int ret = 0;
	u32 rc_idx = pcie_dev->rc_idx;
	u32 i, index;
	u32 bdf = 0;
	u8 type;
	u32 h_type;
	u32 bme;

	if (!pcidev) {
		PCIE_ERR(pcie_dev,
			"PCIe: Did not find PCI device in list for RC%d.\n",
			pcie_dev->rc_idx);
		return -ENODEV;
	} else {
		PCIE_DBG(pcie_dev,
			"PCI device found: vendor-id:0x%x device-id:0x%x\n",
			pcidev->vendor, pcidev->device);
	}

	if (!pcidev->bus->number)
		return ret;

	bdf = BDF_OFFSET(pcidev->bus->number, pcidev->devfn);
	type = pcidev->bus->number == 1 ?
		PCIE20_CTRL1_TYPE_CFG0 : PCIE20_CTRL1_TYPE_CFG1;

	for (i = 0; i < (MAX_RC_NUM * MAX_DEVICE_NUM); i++) {
		if (msm_pcie_dev_tbl[i].bdf == bdf &&
			!msm_pcie_dev_tbl[i].dev) {
			for (index = 0; index < MAX_DEVICE_NUM; index++) {
				if (dev_table_t[index].bdf == bdf) {
					msm_pcie_dev_tbl[i].dev = pcidev;
					msm_pcie_dev_tbl[i].domain = rc_idx;
					msm_pcie_dev_tbl[i].conf_base =
						pcie_dev->conf + index * SZ_4K;
					msm_pcie_dev_tbl[i].phy_address =
						axi_conf->start + index * SZ_4K;

					dev_table_t[index].dev = pcidev;
					dev_table_t[index].domain = rc_idx;
					dev_table_t[index].conf_base =
						pcie_dev->conf + index * SZ_4K;
					dev_table_t[index].phy_address =
						axi_conf->start + index * SZ_4K;

					msm_pcie_iatu_config(pcie_dev, index,
						type,
						dev_table_t[index].phy_address,
						dev_table_t[index].phy_address
						+ SZ_4K - 1,
						bdf);

					h_type = readl_relaxed(
						dev_table_t[index].conf_base +
						PCIE20_HEADER_TYPE);

					bme = readl_relaxed(
						dev_table_t[index].conf_base +
						PCIE20_COMMAND_STATUS);

					if (h_type & (1 << 16)) {
						pci_write_config_dword(pcidev,
							PCIE20_COMMAND_STATUS,
							bme | 0x06);
					}
					break;
				}
			}
			if (index == MAX_DEVICE_NUM) {
				PCIE_ERR(pcie_dev,
					"RC%d PCI device table is full.\n",
					rc_idx);
				ret = index;
			} else {
				break;
			}
		} else if (msm_pcie_dev_tbl[i].bdf == bdf &&
			pcidev == msm_pcie_dev_tbl[i].dev) {
			break;
		}
	}
	if (i == MAX_RC_NUM * MAX_DEVICE_NUM) {
		PCIE_ERR(pcie_dev,
			"Global PCI device table is full: %d elements.\n",
			i);
		PCIE_ERR(pcie_dev,
			"Bus number is 0x%x\nDevice number is 0x%x\n",
			pcidev->bus->number, pcidev->devfn);
		ret = i;
	}
	return ret;
}


int msm_pcie_enumerate(u32 rc_idx)
{
	int ret = 0, bus_ret = 0;
	struct msm_pcie_dev_t *dev = &msm_pcie_dev[rc_idx];

	PCIE_DBG(dev, "Enumerate RC%d\n", rc_idx);

	if (!dev->drv_ready) {
		PCIE_DBG(dev, "RC%d has not been successfully probed yet\n",
			rc_idx);
		return -EPROBE_DEFER;
	}

	if (!dev->enumerated) {
		ret = msm_pcie_enable(dev, PM_ALL);

		/* kick start ARM PCI configuration framework */
		if (!ret) {
			struct pci_dev *pcidev = NULL;
			bool found = false;
			u32 ids = readl_relaxed(msm_pcie_dev[rc_idx].dm_core);
			u32 vendor_id = ids & 0xffff;
			u32 device_id = (ids & 0xffff0000) >> 16;

			PCIE_DBG(dev, "vendor-id:0x%x device_id:0x%x\n",
					vendor_id, device_id);

			msm_pci[rc_idx].private_data = (void **)&dev;
			pci_common_init(&msm_pci[rc_idx]);
			/* This has to happen only once */
			dev->enumerated = true;

			msm_pcie_write_mask(dev->dm_core +
				PCIE20_COMMAND_STATUS, 0, BIT(2)|BIT(1));

			if (dev->shadow_en) {
				u32 val = readl_relaxed(dev->dm_core +
						PCIE20_COMMAND_STATUS);
				PCIE_DBG(dev, "PCIE20_COMMAND_STATUS:0x%x\n",
					val);
				writel_relaxed(val, dev->rc_shadow +
					PCIE20_COMMAND_STATUS / 4);
			}

			do {
				pcidev = pci_get_device(vendor_id,
					device_id, pcidev);
				if (pcidev && (&msm_pcie_dev[rc_idx] ==
					(struct msm_pcie_dev_t *)
					PCIE_BUS_PRIV_DATA(pcidev))) {
					msm_pcie_dev[rc_idx].dev = pcidev;
					found = true;
					PCIE_DBG(&msm_pcie_dev[rc_idx],
						"PCI device is found for RC%d\n",
						rc_idx);
				}
			} while (!found && pcidev);

			if (!pcidev) {
				PCIE_ERR(dev,
					"PCIe: Did not find PCI device for RC%d.\n",
					dev->rc_idx);
				return -ENODEV;
			}

			bus_ret = bus_for_each_dev(&pci_bus_type, NULL, dev,
					&msm_pcie_config_device_table);

			if (bus_ret) {
				PCIE_ERR(dev,
					"PCIe: Failed to set up device table for RC%d\n",
					dev->rc_idx);
				return -ENODEV;
			}
		} else {
			PCIE_ERR(dev, "PCIe: failed to enable RC%d.\n",
				dev->rc_idx);
		}
	} else {
		PCIE_ERR(dev, "PCIe: RC%d has already been enumerated.\n",
			dev->rc_idx);
	}

	return ret;
}
EXPORT_SYMBOL(msm_pcie_enumerate);

static void msm_pcie_notify_client(struct msm_pcie_dev_t *dev,
					enum msm_pcie_event event)
{
	if (dev->event_reg && dev->event_reg->callback &&
		(dev->event_reg->events & event)) {
		struct msm_pcie_notify *notify = &dev->event_reg->notify;
		notify->event = event;
		notify->user = dev->event_reg->user;
		PCIE_DBG(dev, "PCIe: callback RC%d for event %d\n",
			dev->rc_idx, event);
		dev->event_reg->callback(notify);

		if ((dev->event_reg->options & MSM_PCIE_CONFIG_NO_RECOVERY) &&
				(event == MSM_PCIE_EVENT_LINKDOWN)) {
			dev->user_suspend = true;
			PCIE_DBG(dev,
				"PCIe: Client of RC%d will recover the link later.\n",
				dev->rc_idx);
			return;
		} else {
			PCIE_DBG(dev,
				"PCIe: Client of RC%d does not have registration for event %d\n",
				dev->rc_idx, event);
		}
	}
}

static void handle_wake_func(struct work_struct *work)
{
	int ret;
	struct msm_pcie_dev_t *dev = container_of(work, struct msm_pcie_dev_t,
					handle_wake_work);

	PCIE_DBG(dev, "PCIe: Wake work for RC%d\n", dev->rc_idx);

	mutex_lock(&dev->recovery_lock);

	if (!dev->enumerated) {
		PCIE_DBG(dev,
			"PCIe: Start enumeration for RC%d upon the wake from endpoint.\n",
			dev->rc_idx);

		ret = msm_pcie_enumerate(dev->rc_idx);
		if (ret) {
			PCIE_ERR(dev,
				"PCIe: failed to enable RC%d upon wake request from the device.\n",
				dev->rc_idx);
			goto out;
		}

		if ((dev->link_status == MSM_PCIE_LINK_ENABLED) &&
			dev->event_reg && dev->event_reg->callback &&
			(dev->event_reg->events & MSM_PCIE_EVENT_LINKUP)) {
			struct msm_pcie_notify *notify =
					&dev->event_reg->notify;
			notify->event = MSM_PCIE_EVENT_LINKUP;
			notify->user = dev->event_reg->user;
			PCIE_DBG(dev,
				"PCIe: Linkup callback for RC%d after enumeration is successful in wake IRQ handling\n",
				dev->rc_idx);
			dev->event_reg->callback(notify);
		} else {
			PCIE_DBG(dev,
				"PCIe: Client of RC%d does not have registration for linkup event.\n",
				dev->rc_idx);
		}
		goto out;
	} else {
		PCIE_ERR(dev,
			"PCIe: The enumeration for RC%d has already been done.\n",
			dev->rc_idx);
		goto out;
	}

out:
	mutex_unlock(&dev->recovery_lock);
}

static irqreturn_t handle_wake_irq(int irq, void *data)
{
	struct msm_pcie_dev_t *dev = data;
	unsigned long irqsave_flags;

	spin_lock_irqsave(&dev->wakeup_lock, irqsave_flags);

	dev->wake_counter++;
	PCIE_DBG(dev, "PCIe: No. %ld wake IRQ for RC%d\n",
			dev->wake_counter, dev->rc_idx);

	PCIE_DBG(dev, "PCIe WAKE is asserted by Endpoint of RC%d\n",
		dev->rc_idx);

	if (!dev->enumerated) {
		PCIE_DBG(dev, "Start enumeating RC%d\n", dev->rc_idx);
		schedule_work(&dev->handle_wake_work);
	} else {
		PCIE_DBG(dev, "Wake up RC%d\n", dev->rc_idx);
		__pm_stay_awake(&dev->ws);
		__pm_relax(&dev->ws);
		msm_pcie_notify_client(dev, MSM_PCIE_EVENT_WAKEUP);
	}

	spin_unlock_irqrestore(&dev->wakeup_lock, irqsave_flags);

	return IRQ_HANDLED;
}

static irqreturn_t handle_linkdown_irq(int irq, void *data)
{
	struct msm_pcie_dev_t *dev = data;
	unsigned long irqsave_flags;

	spin_lock_irqsave(&dev->linkdown_lock, irqsave_flags);

	dev->linkdown_counter++;

	PCIE_DBG(dev,
		"PCIe: No. %ld linkdown IRQ for RC%d.\n",
		dev->linkdown_counter, dev->rc_idx);

	if (!dev->enumerated || dev->link_status != MSM_PCIE_LINK_ENABLED) {
		PCIE_DBG(dev,
			"PCIe:Linkdown IRQ for RC%d when the link is not enabled\n",
			dev->rc_idx);
	} else if (dev->suspending) {
		PCIE_DBG(dev,
			"PCIe:the link of RC%d is suspending.\n",
			dev->rc_idx);
	} else {
		dev->link_status = MSM_PCIE_LINK_DISABLED;
		dev->shadow_en = false;
		/* assert PERST */
		gpio_set_value(dev->gpio[MSM_PCIE_GPIO_PERST].num,
				dev->gpio[MSM_PCIE_GPIO_PERST].on);
		PCIE_ERR(dev, "PCIe link is down for RC%d\n", dev->rc_idx);
		msm_pcie_notify_client(dev, MSM_PCIE_EVENT_LINKDOWN);
	}

	spin_unlock_irqrestore(&dev->linkdown_lock, irqsave_flags);

	return IRQ_HANDLED;
}

static irqreturn_t handle_msi_irq(int irq, void *data)
{
	int i, j;
	unsigned long val;
	struct msm_pcie_dev_t *dev = data;
	void __iomem *ctrl_status;

	PCIE_DBG(dev, "irq=%d\n", irq);

	/* check for set bits, clear it by setting that bit
	   and trigger corresponding irq */
	for (i = 0; i < PCIE20_MSI_CTRL_MAX; i++) {
		ctrl_status = dev->dm_core +
				PCIE20_MSI_CTRL_INTR_STATUS + (i * 12);

		val = readl_relaxed(ctrl_status);
		while (val) {
			j = find_first_bit(&val, 32);
			writel_relaxed(BIT(j), ctrl_status);
			/* ensure that interrupt is cleared (acked) */
			wmb();
			generic_handle_irq(
			   irq_find_mapping(dev->irq_domain, (j + (32*i)))
			   );
			val = readl_relaxed(ctrl_status);
		}
	}

	return IRQ_HANDLED;
}

void msm_pcie_destroy_irq(unsigned int irq, struct msm_pcie_dev_t *pcie_dev)
{
	int pos;
	struct msm_pcie_dev_t *dev;

	if (pcie_dev)
		dev = pcie_dev;
	else
		dev = irq_get_chip_data(irq);

	if (dev->msi_gicm_addr) {
		PCIE_DBG(dev, "destroy QGIC based irq %d\n", irq);
		pos = irq - dev->msi_gicm_base;
	} else {
		PCIE_DBG(dev, "destroy default MSI irq %d\n", irq);
		pos = irq - irq_find_mapping(dev->irq_domain, 0);
	}

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	if (!dev->msi_gicm_addr)
		dynamic_irq_cleanup(irq);

	PCIE_DBG(dev, "Before clear_bit pos:%d msi_irq_in_use:%ld\n",
		pos, *dev->msi_irq_in_use);
	clear_bit(pos, dev->msi_irq_in_use);
	PCIE_DBG(dev, "After clear_bit pos:%d msi_irq_in_use:%ld\n",
		pos, *dev->msi_irq_in_use);
}

/* hookup to linux pci msi framework */
void arch_teardown_msi_irq(unsigned int irq)
{
	PCIE_GEN_DBG("irq %d deallocated\n", irq);
	msm_pcie_destroy_irq(irq, NULL);
}

void arch_teardown_msi_irqs(struct pci_dev *dev)
{
	struct msi_desc *entry;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG(pcie_dev, "RC:%d EP: vendor_id:0x%x device_id:0x%x\n",
		pcie_dev->rc_idx, dev->vendor, dev->device);

	pcie_dev->use_msi = false;

	list_for_each_entry(entry, &dev->msi_list, list) {
		int i, nvec;
		if (entry->irq == 0)
			continue;
		nvec = 1 << entry->msi_attrib.multiple;
		for (i = 0; i < nvec; i++)
			msm_pcie_destroy_irq(entry->irq + i, pcie_dev);
	}
}

static void msm_pcie_msi_nop(struct irq_data *d)
{
	return;
}

static struct irq_chip pcie_msi_chip = {
	.name = "msm-pcie-msi",
	.irq_ack = msm_pcie_msi_nop,
	.irq_enable = unmask_msi_irq,
	.irq_disable = mask_msi_irq,
	.irq_mask = mask_msi_irq,
	.irq_unmask = unmask_msi_irq,
};

static int msm_pcie_create_irq(struct msm_pcie_dev_t *dev)
{
	int irq, pos;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

again:
	pos = find_first_zero_bit(dev->msi_irq_in_use, PCIE_MSI_NR_IRQS);

	if (pos >= PCIE_MSI_NR_IRQS)
		return -ENOSPC;

	PCIE_DBG(dev, "pos:%d msi_irq_in_use:%ld\n", pos, *dev->msi_irq_in_use);

	if (test_and_set_bit(pos, dev->msi_irq_in_use))
		goto again;
	else
		PCIE_DBG(dev, "test_and_set_bit is successful pos=%d\n", pos);

	irq = irq_create_mapping(dev->irq_domain, pos);
	if (!irq)
		return -EINVAL;

	return irq;
}

static int arch_setup_msi_irq_default(struct pci_dev *pdev,
		struct msi_desc *desc, int nvec)
{
	int irq;
	struct msi_msg msg;
	struct msm_pcie_dev_t *dev = PCIE_BUS_PRIV_DATA(pdev);

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	irq = msm_pcie_create_irq(dev);

	PCIE_DBG(dev, "IRQ %d is allocated.\n", irq);

	if (irq < 0)
		return irq;

	PCIE_DBG(dev, "irq %d allocated\n", irq);

	irq_set_msi_desc(irq, desc);

	/* write msi vector and data */
	msg.address_hi = 0;
	msg.address_lo = MSM_PCIE_MSI_PHY;
	msg.data = irq - irq_find_mapping(dev->irq_domain, 0);
	write_msi_msg(irq, &msg);

	return 0;
}

static int msm_pcie_create_irq_qgic(struct msm_pcie_dev_t *dev)
{
	int irq, pos;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

again:
	pos = find_first_zero_bit(dev->msi_irq_in_use, PCIE_MSI_NR_IRQS);

	if (pos >= PCIE_MSI_NR_IRQS)
		return -ENOSPC;

	PCIE_DBG(dev, "pos:%d msi_irq_in_use:%ld\n", pos, *dev->msi_irq_in_use);

	if (test_and_set_bit(pos, dev->msi_irq_in_use))
		goto again;
	else
		PCIE_DBG(dev, "test_and_set_bit is successful pos=%d\n", pos);

	irq = dev->msi_gicm_base + pos;
	if (!irq) {
		PCIE_ERR(dev, "PCIe: RC%d failed to create QGIC MSI IRQ.\n",
			dev->rc_idx);
		return -EINVAL;
	}

	return irq;
}

static int arch_setup_msi_irq_qgic(struct pci_dev *pdev,
		struct msi_desc *desc, int nvec)
{
	int irq, index, firstirq = 0;
	struct msi_msg msg;
	struct msm_pcie_dev_t *dev = PCIE_BUS_PRIV_DATA(pdev);

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	for (index = 0; index < nvec; index++) {
		irq = msm_pcie_create_irq_qgic(dev);
		PCIE_DBG(dev, "irq %d is allocated\n", irq);

		if (irq < 0)
			return irq;

		if (index == 0)
			firstirq = irq;

		irq_set_irq_type(irq, IRQ_TYPE_EDGE_RISING);
	}

	/* write msi vector and data */
	irq_set_msi_desc(firstirq, desc);
	msg.address_hi = 0;
	msg.address_lo = dev->msi_gicm_addr;
	msg.data = firstirq;
	write_msi_msg(firstirq, &msg);

	return 0;
}

int arch_setup_msi_irq(struct pci_dev *pdev, struct msi_desc *desc)
{
	struct msm_pcie_dev_t *dev = PCIE_BUS_PRIV_DATA(pdev);

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	if (dev->msi_gicm_addr)
		return arch_setup_msi_irq_qgic(pdev, desc, 1);
	else
		return arch_setup_msi_irq_default(pdev, desc, 1);
}

static int msm_pcie_get_msi_multiple(int nvec)
{
	int msi_multiple = 0;

	while (nvec) {
		nvec = nvec >> 1;
		msi_multiple++;
	}
	PCIE_GEN_DBG("log2 number of MSI multiple:%d\n",
		msi_multiple - 1);

	return msi_multiple - 1;
}

int arch_setup_msi_irqs(struct pci_dev *dev, int nvec, int type)
{
	struct msi_desc *entry;
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG(pcie_dev, "RC%d\n", pcie_dev->rc_idx);

	if (type != PCI_CAP_ID_MSI || nvec > 32)
		return -ENOSPC;

	PCIE_DBG(pcie_dev, "nvec = %d\n", nvec);

	list_for_each_entry(entry, &dev->msi_list, list) {
		entry->msi_attrib.multiple =
				msm_pcie_get_msi_multiple(nvec);

		if (pcie_dev->msi_gicm_addr)
			ret = arch_setup_msi_irq_qgic(dev, entry, nvec);
		else
			ret = arch_setup_msi_irq_default(dev, entry, nvec);

		PCIE_DBG(pcie_dev, "ret from msi_irq: %d\n", ret);

		if (ret < 0)
			return ret;
		if (ret > 0)
			return -ENOSPC;
	}

	pcie_dev->use_msi = true;

	return 0;
}

static int msm_pcie_msi_map(struct irq_domain *domain, unsigned int irq,
	   irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler (irq, &pcie_msi_chip, handle_simple_irq);
	irq_set_chip_data(irq, domain->host_data);
	set_irq_flags(irq, IRQF_VALID);
	return 0;
}

static const struct irq_domain_ops msm_pcie_msi_ops = {
	.map = msm_pcie_msi_map,
};

int32_t msm_pcie_irq_init(struct msm_pcie_dev_t *dev)
{
	int rc;
	int msi_start =  0;
	struct device *pdev = &dev->pdev->dev;

	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	if (dev->rc_idx)
		wakeup_source_init(&dev->ws, "RC1 pcie_wakeup_source");
	else
		wakeup_source_init(&dev->ws, "RC0 pcie_wakeup_source");

	/* register handler for linkdown interrupt */
	rc = devm_request_irq(pdev,
		dev->irq[MSM_PCIE_INT_LINK_DOWN].num, handle_linkdown_irq,
		IRQF_TRIGGER_RISING, dev->irq[MSM_PCIE_INT_LINK_DOWN].name,
		dev);
	if (rc) {
		PCIE_ERR(dev, "PCIe: Unable to request linkdown interrupt:%d\n",
			dev->irq[MSM_PCIE_INT_LINK_DOWN].num);
		return rc;
	}

	/* register handler for physical MSI interrupt line */
	rc = devm_request_irq(pdev,
		dev->irq[MSM_PCIE_INT_MSI].num, handle_msi_irq,
		IRQF_TRIGGER_RISING, dev->irq[MSM_PCIE_INT_MSI].name, dev);
	if (rc) {
		PCIE_ERR(dev, "PCIe: RC%d: Unable to request MSI interrupt\n",
			dev->rc_idx);
		return rc;
	}

	/* register handler for PCIE_WAKE_N interrupt line */
	rc = devm_request_irq(pdev,
			dev->wake_n, handle_wake_irq, IRQF_TRIGGER_FALLING,
			 "msm_pcie_wake", dev);
	if (rc) {
		PCIE_ERR(dev, "PCIe: RC%d: Unable to request wake interrupt\n",
			dev->rc_idx);
		return rc;
	}

	INIT_WORK(&dev->handle_wake_work, handle_wake_func);

	rc = enable_irq_wake(dev->wake_n);
	if (rc) {
		PCIE_ERR(dev, "PCIe: RC%d: Unable to enable wake interrupt\n",
			dev->rc_idx);
		return rc;
	}

	/* Create a virtual domain of interrupts */
	if (!dev->msi_gicm_addr) {
		dev->irq_domain = irq_domain_add_linear(dev->pdev->dev.of_node,
			PCIE_MSI_NR_IRQS, &msm_pcie_msi_ops, dev);

		if (!dev->irq_domain) {
			PCIE_ERR(dev,
				"PCIe: RC%d: Unable to initialize irq domain\n",
				dev->rc_idx);
			disable_irq(dev->wake_n);
			return PTR_ERR(dev->irq_domain);
		}

		msi_start = irq_create_mapping(dev->irq_domain, 0);
	}

	return 0;
}

void msm_pcie_irq_deinit(struct msm_pcie_dev_t *dev)
{
	PCIE_DBG(dev, "RC%d\n", dev->rc_idx);

	wakeup_source_trash(&dev->ws);
	disable_irq(dev->wake_n);
}


static int msm_pcie_probe(struct platform_device *pdev)
{
	int ret = 0;
	int rc_idx = -1;
	int i, j;

	PCIE_GEN_DBG("%s\n", __func__);

	mutex_lock(&pcie_drv.drv_lock);

	ret = of_property_read_u32((&pdev->dev)->of_node,
				"cell-index", &rc_idx);
	if (ret) {
		PCIE_GEN_DBG("Did not find RC index.\n");
		goto out;
	} else {
		if (rc_idx >= MAX_RC_NUM) {
			pr_err(
				"PCIe: Invalid RC Index %d (max supported = %d)\n",
				rc_idx, MAX_RC_NUM);
			goto out;
		}
		pcie_drv.rc_num++;
		PCIE_DBG(&msm_pcie_dev[rc_idx], "PCIe: RC index is %d.\n",
			rc_idx);
	}

	msm_pcie_dev[rc_idx].l0s_supported =
		of_property_read_bool((&pdev->dev)->of_node,
				"qcom,l0s-supported");
	PCIE_DBG(&msm_pcie_dev[rc_idx], "L0s is %s supported.\n",
		msm_pcie_dev[rc_idx].l0s_supported ? "" : "not");
	msm_pcie_dev[rc_idx].l1_supported =
		of_property_read_bool((&pdev->dev)->of_node,
				"qcom,l1-supported");
	PCIE_DBG(&msm_pcie_dev[rc_idx], "L1 is %s supported.\n",
		msm_pcie_dev[rc_idx].l1_supported ? "" : "not");
	msm_pcie_dev[rc_idx].l1ss_supported =
		of_property_read_bool((&pdev->dev)->of_node,
				"qcom,l1ss-supported");
	PCIE_DBG(&msm_pcie_dev[rc_idx], "L1ss is %s supported.\n",
		msm_pcie_dev[rc_idx].l1ss_supported ? "" : "not");
	msm_pcie_dev[rc_idx].aux_clk_sync =
		of_property_read_bool((&pdev->dev)->of_node,
				"qcom,aux-clk-sync");
	PCIE_DBG(&msm_pcie_dev[rc_idx],
		"AUX clock is %s synchronous to Core clock.\n",
		msm_pcie_dev[rc_idx].aux_clk_sync ? "" : "not");

	msm_pcie_dev[rc_idx].ep_wakeirq =
		of_property_read_bool((&pdev->dev)->of_node,
				"qcom,ep-wakeirq");
	PCIE_DBG(&msm_pcie_dev[rc_idx],
		"PCIe: EP of RC%d does %s assert wake when it is up.\n",
		rc_idx, msm_pcie_dev[rc_idx].ep_wakeirq ? "" : "not");

	msm_pcie_dev[rc_idx].n_fts = 0;
	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,n-fts",
				&msm_pcie_dev[rc_idx].n_fts);

	if (ret)
		PCIE_DBG(&msm_pcie_dev[rc_idx],
			"n-fts does not exist. ret=%d\n", ret);
	else
		PCIE_DBG(&msm_pcie_dev[rc_idx], "n-fts: 0x%x.\n",
				msm_pcie_dev[rc_idx].n_fts);

	msm_pcie_dev[rc_idx].ext_ref_clk =
		of_property_read_bool((&pdev->dev)->of_node,
				"qcom,ext-ref-clk");
	PCIE_DBG(&msm_pcie_dev[rc_idx], "ref clk is %s.\n",
		msm_pcie_dev[rc_idx].ext_ref_clk ? "external" : "internal");

	msm_pcie_dev[rc_idx].ep_latency = 0;
	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,ep-latency",
				&msm_pcie_dev[rc_idx].ep_latency);
	if (ret)
		PCIE_DBG(&msm_pcie_dev[rc_idx],
			"RC%d: ep-latency does not exist.\n",
			rc_idx);
	else
		PCIE_DBG(&msm_pcie_dev[rc_idx], "RC%d: ep-latency: 0x%x.\n",
			rc_idx, msm_pcie_dev[rc_idx].ep_latency);

	msm_pcie_dev[rc_idx].msi_gicm_addr = 0;
	msm_pcie_dev[rc_idx].msi_gicm_base = 0;
	ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,msi-gicm-addr",
				&msm_pcie_dev[rc_idx].msi_gicm_addr);

	if (ret) {
		PCIE_DBG(&msm_pcie_dev[rc_idx], "%s",
			"msi-gicm-addr does not exist.\n");
	} else {
		PCIE_DBG(&msm_pcie_dev[rc_idx], "msi-gicm-addr: 0x%x.\n",
				msm_pcie_dev[rc_idx].msi_gicm_addr);

		ret = of_property_read_u32((&pdev->dev)->of_node,
				"qcom,msi-gicm-base",
				&msm_pcie_dev[rc_idx].msi_gicm_base);

		if (ret) {
			PCIE_ERR(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: msi-gicm-base does not exist.\n",
				rc_idx);
			goto decrease_rc_num;
		} else {
			PCIE_DBG(&msm_pcie_dev[rc_idx], "msi-gicm-base: 0x%x\n",
					msm_pcie_dev[rc_idx].msi_gicm_base);
		}
	}

	msm_pcie_dev[rc_idx].rc_idx = rc_idx;
	msm_pcie_dev[rc_idx].pdev = pdev;
	msm_pcie_dev[rc_idx].vreg_n = 0;
	msm_pcie_dev[rc_idx].gpio_n = 0;
	msm_pcie_dev[rc_idx].parf_deemph = 0;
	msm_pcie_dev[rc_idx].parf_swing = 0;
	msm_pcie_dev[rc_idx].link_status = MSM_PCIE_LINK_DEINIT;
	msm_pcie_dev[rc_idx].user_suspend = false;
	msm_pcie_dev[rc_idx].disable_pc = false;
	msm_pcie_dev[rc_idx].saved_state = NULL;
	msm_pcie_dev[rc_idx].enumerated = false;
	msm_pcie_dev[rc_idx].linkdown_counter = 0;
	msm_pcie_dev[rc_idx].suspending = false;
	msm_pcie_dev[rc_idx].wake_counter = 0;
	msm_pcie_dev[rc_idx].power_on = false;
	msm_pcie_dev[rc_idx].use_msi = false;
	memcpy(msm_pcie_dev[rc_idx].vreg, msm_pcie_vreg_info,
				sizeof(msm_pcie_vreg_info));
	memcpy(msm_pcie_dev[rc_idx].gpio, msm_pcie_gpio_info,
				sizeof(msm_pcie_gpio_info));
	memcpy(msm_pcie_dev[rc_idx].clk, msm_pcie_clk_info[rc_idx],
				sizeof(msm_pcie_clk_info));
	memcpy(msm_pcie_dev[rc_idx].pipeclk, msm_pcie_pipe_clk_info[rc_idx],
				sizeof(msm_pcie_pipe_clk_info));
	memcpy(msm_pcie_dev[rc_idx].res, msm_pcie_res_info,
				sizeof(msm_pcie_res_info));
	memcpy(msm_pcie_dev[rc_idx].irq, msm_pcie_irq_info,
				sizeof(msm_pcie_irq_info));
	msm_pcie_dev[rc_idx].shadow_en = true;
	for (i = 0; i < PCIE_CONF_SPACE_DW; i++)
		msm_pcie_dev[rc_idx].rc_shadow[i] = PCIE_CLEAR;
	for (i = 0; i < MAX_DEVICE_NUM; i++)
		for (j = 0; j < PCIE_CONF_SPACE_DW; j++)
			msm_pcie_dev[rc_idx].ep_shadow[i][j] = PCIE_CLEAR;
	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		msm_pcie_dev[rc_idx].pcidev_table[i].bdf = 0;
		msm_pcie_dev[rc_idx].pcidev_table[i].dev = NULL;
		msm_pcie_dev[rc_idx].pcidev_table[i].domain = rc_idx;
		msm_pcie_dev[rc_idx].pcidev_table[i].conf_base = 0;
		msm_pcie_dev[rc_idx].pcidev_table[i].phy_address = 0;
	}

	ret = msm_pcie_get_resources(&msm_pcie_dev[rc_idx],
				msm_pcie_dev[rc_idx].pdev);

	if (ret)
		goto decrease_rc_num;

	ret = msm_pcie_gpio_init(&msm_pcie_dev[rc_idx]);
	if (ret) {
		msm_pcie_release_resources(&msm_pcie_dev[rc_idx]);
		goto decrease_rc_num;
	}

	ret = msm_pcie_irq_init(&msm_pcie_dev[rc_idx]);
	if (ret) {
		msm_pcie_release_resources(&msm_pcie_dev[rc_idx]);
		msm_pcie_gpio_deinit(&msm_pcie_dev[rc_idx]);
		goto decrease_rc_num;
	}

	msm_pcie_dev[rc_idx].drv_ready = true;

	if (msm_pcie_dev[rc_idx].ep_wakeirq) {
		PCIE_DBG(&msm_pcie_dev[rc_idx],
			"PCIe: RC%d will be enumerated upon WAKE signal from Endpoint.\n",
			rc_idx);
		mutex_unlock(&pcie_drv.drv_lock);
		return 0;
	}

	ret = msm_pcie_enumerate(rc_idx);

	if (ret)
		PCIE_ERR(&msm_pcie_dev[rc_idx],
			"PCIe: RC%d is not enabled during bootup; it will be enumerated upon client request.\n",
			rc_idx);
	else
		PCIE_ERR(&msm_pcie_dev[rc_idx], "RC%d is enabled in bootup\n",
			rc_idx);

	PCIE_DBG(&msm_pcie_dev[rc_idx], "PCIE probed %s\n",
		dev_name(&(pdev->dev)));

	mutex_unlock(&pcie_drv.drv_lock);
	return 0;

decrease_rc_num:
	pcie_drv.rc_num--;
out:
	PCIE_ERR(&msm_pcie_dev[rc_idx],
		"PCIe: Driver probe failed for RC%d:%d\n",
		rc_idx, ret);
	mutex_unlock(&pcie_drv.drv_lock);

	return ret;
}

static int msm_pcie_remove(struct platform_device *pdev)
{
	int ret = 0;
	int rc_idx;

	PCIE_GEN_DBG("PCIe:%s.\n", __func__);

	mutex_lock(&pcie_drv.drv_lock);

	ret = of_property_read_u32((&pdev->dev)->of_node,
				"cell-index", &rc_idx);
	if (ret) {
		pr_err("%s: Did not find RC index.\n", __func__);
		goto out;
	} else {
		pcie_drv.rc_num--;
		PCIE_GEN_DBG("%s: RC index is 0x%x.", __func__, rc_idx);
	}

	msm_pcie_irq_deinit(&msm_pcie_dev[rc_idx]);
	msm_pcie_vreg_deinit(&msm_pcie_dev[rc_idx]);
	msm_pcie_clk_deinit(&msm_pcie_dev[rc_idx]);
	msm_pcie_gpio_deinit(&msm_pcie_dev[rc_idx]);
	msm_pcie_release_resources(&msm_pcie_dev[rc_idx]);

out:
	mutex_unlock(&pcie_drv.drv_lock);

	return ret;
}

static struct of_device_id msm_pcie_match[] = {
	{	.compatible = "qcom,pci-msm",
	},
	{}
};

static struct platform_driver msm_pcie_driver = {
	.probe	= msm_pcie_probe,
	.remove	= msm_pcie_remove,
	.driver	= {
		.name		= "pci-msm",
		.owner		= THIS_MODULE,
		.of_match_table	= msm_pcie_match,
	},
};

int __init pcie_init(void)
{
	int ret = 0, i;
	char rc_name[MAX_RC_NAME_LEN];

	pr_alert("pcie:%s.\n", __func__);

	pcie_drv.rc_num = 0;
	mutex_init(&pcie_drv.drv_lock);

	for (i = 0; i < MAX_RC_NUM; i++) {
		snprintf(rc_name, MAX_RC_NAME_LEN, "pcie%d", i);
		msm_pcie_dev[i].ipc_log =
			ipc_log_context_create(PCIE_LOG_PAGES, rc_name, 0);
		if (msm_pcie_dev[i].ipc_log == NULL)
			pr_err("%s: unable to create IPC log context for %s\n",
				__func__, rc_name);
		else
			PCIE_DBG(&msm_pcie_dev[i],
				"PCIe IPC logging is enable for RC%d\n",
				i);
		spin_lock_init(&msm_pcie_dev[i].cfg_lock);
		msm_pcie_dev[i].cfg_access = true;
		mutex_init(&msm_pcie_dev[i].setup_lock);
		mutex_init(&msm_pcie_dev[i].recovery_lock);
		spin_lock_init(&msm_pcie_dev[i].linkdown_lock);
		spin_lock_init(&msm_pcie_dev[i].wakeup_lock);
		msm_pcie_dev[i].drv_ready = false;
	}
	for (i = 0; i < MAX_RC_NUM * MAX_DEVICE_NUM; i++) {
		msm_pcie_dev_tbl[i].bdf = 0;
		msm_pcie_dev_tbl[i].dev = NULL;
		msm_pcie_dev_tbl[i].domain = -1;
		msm_pcie_dev_tbl[i].conf_base = 0;
		msm_pcie_dev_tbl[i].phy_address = 0;
	}

	ret = platform_driver_register(&msm_pcie_driver);

	return ret;
}

static void __exit pcie_exit(void)
{
	PCIE_GEN_DBG("pcie:%s.\n", __func__);

	platform_driver_unregister(&msm_pcie_driver);
}

subsys_initcall_sync(pcie_init);
module_exit(pcie_exit);


/* RC do not represent the right class; set it to PCI_CLASS_BRIDGE_PCI */
static void msm_pcie_fixup_early(struct pci_dev *dev)
{
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);
	PCIE_DBG(pcie_dev, "hdr_type %d\n", dev->hdr_type);
	if (dev->hdr_type == 1)
		dev->class = (dev->class & 0xff) | (PCI_CLASS_BRIDGE_PCI << 8);
}
DECLARE_PCI_FIXUP_EARLY(PCIE_VENDOR_ID_RCP, PCIE_DEVICE_ID_RCP,
			msm_pcie_fixup_early);

/* Suspend the PCIe link */
static int msm_pcie_pm_suspend(struct pci_dev *dev,
			void *user, void *data, u32 options)
{
	int ret = 0;
	u32 val = 0;
	int ret_l23;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	pcie_dev->suspending = true;
	PCIE_DBG(pcie_dev, "RC%d\n", pcie_dev->rc_idx);

	if (!pcie_dev->power_on) {
		PCIE_DBG(pcie_dev,
			"PCIe: power of RC%d has been turned off.\n",
			pcie_dev->rc_idx);
		return ret;
	}

	if (dev && !(options & MSM_PCIE_CONFIG_NO_CFG_RESTORE)
		&& msm_pcie_confirm_linkup(pcie_dev, true, true)) {
		ret = pci_save_state(dev);
		pcie_dev->saved_state =	pci_store_saved_state(dev);
	}
	if (ret) {
		PCIE_ERR(pcie_dev, "PCIe: fail to save state of RC%d:%d.\n",
			pcie_dev->rc_idx, ret);
		pcie_dev->suspending = false;
		return ret;
	}

	spin_lock_irqsave(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);
	pcie_dev->cfg_access = false;
	spin_unlock_irqrestore(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);

	msm_pcie_write_mask(pcie_dev->elbi + PCIE20_ELBI_SYS_CTRL, 0,
				BIT(4));

	PCIE_DBG(pcie_dev, "RC%d: PME_TURNOFF_MSG is sent out\n",
		pcie_dev->rc_idx);

	ret_l23 = readl_poll_timeout((pcie_dev->parf
		+ PCIE20_PARF_PM_STTS), val, (val & BIT(5)), 10000, 100000);

	/* check L23_Ready */
	PCIE_DBG(pcie_dev, "RC%d: PCIE20_PARF_PM_STTS is 0x%x.\n",
		pcie_dev->rc_idx,
		readl_relaxed(pcie_dev->parf + PCIE20_PARF_PM_STTS));
	if (!ret_l23)
		PCIE_DBG(pcie_dev, "RC%d: PM_Enter_L23 is received\n",
			pcie_dev->rc_idx);
	else
		PCIE_DBG(pcie_dev, "RC%d: PM_Enter_L23 is NOT received\n",
			pcie_dev->rc_idx);

		msm_pcie_disable(pcie_dev, PM_PIPE_CLK | PM_CLK | PM_VREG);

	return ret;
}

static void msm_pcie_fixup_suspend(struct pci_dev *dev)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG(pcie_dev, "RC%d\n", pcie_dev->rc_idx);

	if (pcie_dev->link_status != MSM_PCIE_LINK_ENABLED)
		return;

	spin_lock_irqsave(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);
	if (pcie_dev->disable_pc) {
		PCIE_DBG(pcie_dev,
			"RC%d: Skip suspend because of user request\n",
			pcie_dev->rc_idx);
		spin_unlock_irqrestore(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);
		return;
	}
	spin_unlock_irqrestore(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);

	mutex_lock(&pcie_dev->recovery_lock);

	ret = msm_pcie_pm_suspend(dev, NULL, NULL, 0);
	if (ret)
		PCIE_ERR(pcie_dev, "PCIe: RC%d got failure in suspend:%d.\n",
			pcie_dev->rc_idx, ret);

	mutex_unlock(&pcie_dev->recovery_lock);
}
DECLARE_PCI_FIXUP_SUSPEND(PCIE_VENDOR_ID_RCP, PCIE_DEVICE_ID_RCP,
			  msm_pcie_fixup_suspend);

/* Resume the PCIe link */
static int msm_pcie_pm_resume(struct pci_dev *dev,
			void *user, void *data, u32 options)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG(pcie_dev, "RC%d\n", pcie_dev->rc_idx);

	spin_lock_irqsave(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);
	pcie_dev->cfg_access = true;
	spin_unlock_irqrestore(&pcie_dev->cfg_lock,
				pcie_dev->irqsave_flags);

	ret = msm_pcie_enable(pcie_dev, PM_PIPE_CLK | PM_CLK | PM_VREG);
	if (ret) {
		PCIE_ERR(pcie_dev,
			"PCIe: RC%d fail to enable PCIe link in resume.\n",
			pcie_dev->rc_idx);
		return ret;
	} else {
		pcie_dev->suspending = false;
		PCIE_DBG(pcie_dev,
			"dev->bus->number = %d dev->bus->primary = %d\n",
			 dev->bus->number, dev->bus->primary);

		if (!(options & MSM_PCIE_CONFIG_NO_CFG_RESTORE)) {
			pci_load_and_free_saved_state(dev,
					&pcie_dev->saved_state);
			pci_restore_state(dev);
		}
	}

	return ret;
}

void msm_pcie_fixup_resume(struct pci_dev *dev)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG(pcie_dev, "RC%d\n", pcie_dev->rc_idx);

	if ((pcie_dev->link_status != MSM_PCIE_LINK_DISABLED) ||
		pcie_dev->user_suspend)
		return;

	mutex_lock(&pcie_dev->recovery_lock);
	ret = msm_pcie_pm_resume(dev, NULL, NULL, 0);
	if (ret)
		PCIE_ERR(pcie_dev,
			"PCIe: RC%d got failure in fixup resume:%d.\n",
			pcie_dev->rc_idx, ret);

	mutex_unlock(&pcie_dev->recovery_lock);
}
DECLARE_PCI_FIXUP_RESUME(PCIE_VENDOR_ID_RCP, PCIE_DEVICE_ID_RCP,
				 msm_pcie_fixup_resume);

void msm_pcie_fixup_resume_early(struct pci_dev *dev)
{
	int ret;
	struct msm_pcie_dev_t *pcie_dev = PCIE_BUS_PRIV_DATA(dev);

	PCIE_DBG(pcie_dev, "RC%d\n", pcie_dev->rc_idx);

	if ((pcie_dev->link_status != MSM_PCIE_LINK_DISABLED) ||
		pcie_dev->user_suspend)
		return;

	mutex_lock(&pcie_dev->recovery_lock);
	ret = msm_pcie_pm_resume(dev, NULL, NULL, 0);
	if (ret)
		PCIE_ERR(pcie_dev, "PCIe: RC%d got failure in resume:%d.\n",
			pcie_dev->rc_idx, ret);

	mutex_unlock(&pcie_dev->recovery_lock);
}
DECLARE_PCI_FIXUP_RESUME_EARLY(PCIE_VENDOR_ID_RCP, PCIE_DEVICE_ID_RCP,
				 msm_pcie_fixup_resume_early);

int msm_pcie_pm_control(enum msm_pcie_pm_opt pm_opt, u32 busnr, void *user,
			void *data, u32 options)
{
	int ret = 0;
	struct pci_dev *dev;
	u32 rc_idx = 0;

	PCIE_GEN_DBG("PCIe: pm_opt:%d;busnr:%d;options:%d\n",
		pm_opt, busnr, options);

	switch (busnr) {
	case 1:
		if (user) {
			struct msm_pcie_dev_t *pcie_dev
				= PCIE_BUS_PRIV_DATA(((struct pci_dev *)user));

			if (pcie_dev) {
				rc_idx = pcie_dev->rc_idx;
				PCIE_DBG(pcie_dev,
					"PCIe: RC%d: pm_opt:%d;busnr:%d;options:%d\n",
					rc_idx, pm_opt, busnr, options);
			} else {
				pr_err(
					"PCIe: did not find RC for pci endpoint device.\n"
					);
				ret = -ENODEV;
				goto out;
			}
		}
		break;
	default:
		pr_err("PCIe: unsupported bus number.\n");
		ret = PCIBIOS_DEVICE_NOT_FOUND;
		goto out;
	}

	dev = msm_pcie_dev[rc_idx].dev;

	if (!msm_pcie_dev[rc_idx].drv_ready) {
		PCIE_ERR(&msm_pcie_dev[rc_idx],
			"RC%d has not been successfully probed yet\n",
			rc_idx);
		return -EPROBE_DEFER;
	}

	switch (pm_opt) {
	case MSM_PCIE_SUSPEND:
		if (msm_pcie_dev[rc_idx].link_status != MSM_PCIE_LINK_ENABLED)
			PCIE_DBG(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: requested to suspend when link is not enabled:%d.\n",
				rc_idx, msm_pcie_dev[rc_idx].link_status);

		if (!msm_pcie_dev[rc_idx].power_on) {
			PCIE_ERR(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: requested to suspend when link is powered down:%d.\n",
				rc_idx, msm_pcie_dev[rc_idx].link_status);
			break;
		}

		msm_pcie_dev[rc_idx].user_suspend = true;

		mutex_lock(&msm_pcie_dev[rc_idx].recovery_lock);

		ret = msm_pcie_pm_suspend(dev, user, data, options);
		if (ret) {
			PCIE_ERR(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: user failed to suspend the link.\n",
				rc_idx);
			msm_pcie_dev[rc_idx].user_suspend = false;
		}

		mutex_unlock(&msm_pcie_dev[rc_idx].recovery_lock);
		break;
	case MSM_PCIE_RESUME:
		PCIE_DBG(&msm_pcie_dev[rc_idx],
			"User of RC%d requests to resume the link\n", rc_idx);
		if (msm_pcie_dev[rc_idx].link_status !=
					MSM_PCIE_LINK_DISABLED) {
			PCIE_ERR(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: requested to resume when link is not disabled:%d.\n",
				rc_idx, msm_pcie_dev[rc_idx].link_status);
			break;
		}

		mutex_lock(&msm_pcie_dev[rc_idx].recovery_lock);
		ret = msm_pcie_pm_resume(dev, user, data, options);
		if (ret) {
			PCIE_ERR(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: user failed to resume the link.\n",
				rc_idx);
		} else {
			PCIE_DBG(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d: user succeeded to resume the link.\n",
				rc_idx);

			msm_pcie_dev[rc_idx].user_suspend = false;
		}

		mutex_unlock(&msm_pcie_dev[rc_idx].recovery_lock);

		break;
	case MSM_PCIE_DISABLE_PC:
		PCIE_DBG(&msm_pcie_dev[rc_idx],
			"User of RC%d requests to keep the link always alive.\n",
			rc_idx);
		spin_lock_irqsave(&msm_pcie_dev[rc_idx].cfg_lock,
				msm_pcie_dev[rc_idx].irqsave_flags);
		if (msm_pcie_dev[rc_idx].suspending) {
			PCIE_ERR(&msm_pcie_dev[rc_idx],
				"PCIe: RC%d Link has been suspended before request\n",
				rc_idx);
			ret = MSM_PCIE_ERROR;
		} else {
			msm_pcie_dev[rc_idx].disable_pc = true;
		}
		spin_unlock_irqrestore(&msm_pcie_dev[rc_idx].cfg_lock,
				msm_pcie_dev[rc_idx].irqsave_flags);
		break;
	case MSM_PCIE_ENABLE_PC:
		PCIE_DBG(&msm_pcie_dev[rc_idx],
			"User of RC%d cancels the request of alive link.\n",
			rc_idx);
		spin_lock_irqsave(&msm_pcie_dev[rc_idx].cfg_lock,
				msm_pcie_dev[rc_idx].irqsave_flags);
		msm_pcie_dev[rc_idx].disable_pc = false;
		spin_unlock_irqrestore(&msm_pcie_dev[rc_idx].cfg_lock,
				msm_pcie_dev[rc_idx].irqsave_flags);
		break;
	default:
		PCIE_ERR(&msm_pcie_dev[rc_idx],
			"PCIe: RC%d: unsupported pm operation:%d.\n",
			rc_idx, pm_opt);
		ret = -ENODEV;
		goto out;
	}

out:
	return ret;
}
EXPORT_SYMBOL(msm_pcie_pm_control);

int msm_pcie_register_event(struct msm_pcie_register_event *reg)
{
	int ret = 0;
	struct msm_pcie_dev_t *pcie_dev;

	if (!reg) {
		pr_err("PCIe: Event registration is NULL\n");
		return -ENODEV;
	}

	if (!reg->user) {
		pr_err("PCIe: User of event registration is NULL\n");
		return -ENODEV;
	}

	pcie_dev = PCIE_BUS_PRIV_DATA(((struct pci_dev *)reg->user));

	if (pcie_dev) {
		pcie_dev->event_reg = reg;
		PCIE_DBG(pcie_dev,
			"Event 0x%x is registered for RC %d\n", reg->events,
			pcie_dev->rc_idx);
	} else {
		PCIE_ERR(pcie_dev, "%s",
			"PCIe: did not find RC for pci endpoint device.\n");
		ret = -ENODEV;
	}

	return ret;
}
EXPORT_SYMBOL(msm_pcie_register_event);

int msm_pcie_deregister_event(struct msm_pcie_register_event *reg)
{
	int ret = 0;
	struct msm_pcie_dev_t *pcie_dev;

	if (!reg) {
		pr_err("PCIe: Event deregistration is NULL\n");
		return -ENODEV;
	}

	if (!reg->user) {
		pr_err("PCIe: User of event deregistration is NULL\n");
		return -ENODEV;
	}

	pcie_dev = PCIE_BUS_PRIV_DATA(((struct pci_dev *)reg->user));

	if (pcie_dev) {
		pcie_dev->event_reg = NULL;
		PCIE_DBG(pcie_dev, "Event is deregistered for RC %d\n",
				pcie_dev->rc_idx);
	} else {
		PCIE_ERR(pcie_dev, "%s",
			"PCIe: did not find RC for pci endpoint device.\n");
		ret = -ENODEV;
	}

	return ret;
}
EXPORT_SYMBOL(msm_pcie_deregister_event);

int msm_pcie_recover_config(struct pci_dev *dev)
{
	int ret = 0;
	struct msm_pcie_dev_t *pcie_dev;

	if (dev) {
		pcie_dev = PCIE_BUS_PRIV_DATA(dev);
		PCIE_DBG(pcie_dev,
			"Recovery for the link of RC%d\n", pcie_dev->rc_idx);
	} else {
		pr_err("PCIe: the input pci dev is NULL.\n");
		return -ENODEV;
	}

	if (msm_pcie_confirm_linkup(pcie_dev, true, true)) {
		PCIE_DBG(pcie_dev,
			"Recover config space of RC%d and its EP\n",
			pcie_dev->rc_idx);
		pcie_dev->shadow_en = false;
		PCIE_DBG(pcie_dev, "Recover RC%d\n", pcie_dev->rc_idx);
		msm_pcie_cfg_recover(pcie_dev, true);
		PCIE_DBG(pcie_dev, "Recover EP of RC%d\n", pcie_dev->rc_idx);
		msm_pcie_cfg_recover(pcie_dev, false);
		PCIE_DBG(pcie_dev,
			"Refreshing the saved config space in PCI framework for RC%d and its EP\n",
			pcie_dev->rc_idx);
		pci_save_state(pcie_dev->dev);
		pci_save_state(dev);
		pcie_dev->shadow_en = true;
		PCIE_DBG(pcie_dev, "Turn on shadow for RC%d\n",
			pcie_dev->rc_idx);
	} else {
		PCIE_ERR(pcie_dev,
			"PCIe: the link of RC%d is not up yet; can't recover config space.\n",
			pcie_dev->rc_idx);
		ret = -ENODEV;
	}

	return ret;
}
EXPORT_SYMBOL(msm_pcie_recover_config);

int msm_pcie_shadow_control(struct pci_dev *dev, bool enable)
{
	int ret = 0;
	struct msm_pcie_dev_t *pcie_dev;

	if (dev) {
		pcie_dev = PCIE_BUS_PRIV_DATA(dev);
		PCIE_DBG(pcie_dev,
			"Recovery for the link of RC%d\n", pcie_dev->rc_idx);
	} else {
		pr_err("PCIe: the input pci dev is NULL.\n");
		return -ENODEV;
	}

	PCIE_DBG(pcie_dev,
		"The shadowing of RC%d is %s enabled currently.\n",
		pcie_dev->rc_idx, pcie_dev->shadow_en ? "" : "not");

	pcie_dev->shadow_en = enable;

	PCIE_DBG(pcie_dev,
		"Shadowing of RC%d is turned %s upon user's request.\n",
		pcie_dev->rc_idx, enable ? "on" : "off");

	return ret;
}
EXPORT_SYMBOL(msm_pcie_shadow_control);
