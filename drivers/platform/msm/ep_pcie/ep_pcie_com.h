/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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

#ifndef __EP_PCIE_COM_H
#define __EP_PCIE_COM_H

#include <linux/io.h>
#include <linux/clk.h>
#include <linux/compiler.h>
#include <linux/ipc_logging.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/msm_ep_pcie.h>

#define PCIE20_PARF_SYS_CTRL           0x00
#define PCIE20_PARF_DB_CTRL            0x10
#define PCIE20_PARF_PM_CTRL            0x20
#define PCIE20_PARF_PM_STTS            0x24
#define PCIE20_PARF_PHY_CTRL           0x40
#define PCIE20_PARF_PHY_REFCLK         0x4C
#define PCIE20_PARF_CONFIG_BITS        0x50
#define PCIE20_PARF_TEST_BUS           0xE4
#define PCIE20_PARF_MHI_BASE_ADDR_LOWER 0x178
#define PCIE20_PARF_MHI_BASE_ADDR_UPPER 0x17c
#define PCIE20_PARF_MSI_GEN             0x188
#define PCIE20_PARF_DEBUG_INT_EN        0x190
#define PCIE20_PARF_MHI_IPA_DBS                0x198
#define PCIE20_PARF_MHI_IPA_CDB_TARGET_LOWER   0x19C
#define PCIE20_PARF_MHI_IPA_EDB_TARGET_LOWER   0x1A0
#define PCIE20_PARF_AXI_MSTR_RD_HALT_NO_WRITES 0x1A4
#define PCIE20_PARF_AXI_MSTR_WR_ADDR_HALT      0x1A8
#define PCIE20_PARF_Q2A_FLUSH          0x1AC
#define PCIE20_PARF_LTSSM              0x1B0
#define PCIE20_PARF_CFG_BITS           0x210
#define PCIE20_PARF_LTR_MSI_EXIT_L1SS  0x214
#define PCIE20_PARF_INT_ALL_STATUS     0x224
#define PCIE20_PARF_INT_ALL_CLEAR      0x228
#define PCIE20_PARF_INT_ALL_MASK       0x22C
#define PCIE20_PARF_SLV_ADDR_MSB_CTRL  0x2C0
#define PCIE20_PARF_DBI_BASE_ADDR      0x350
#define PCIE20_PARF_DBI_BASE_ADDR_HI   0x354
#define PCIE20_PARF_SLV_ADDR_SPACE_SIZE        0x358
#define PCIE20_PARF_SLV_ADDR_SPACE_SIZE_HI     0x35C
#define PCIE20_PARF_DEVICE_TYPE        0x1000

#define PCIE20_ELBI_VERSION            0x00
#define PCIE20_ELBI_SYS_CTRL           0x04
#define PCIE20_ELBI_SYS_STTS	       0x08
#define PCIE20_ELBI_CS2_ENABLE         0xA4

#define PCIE20_DEVICE_ID_VENDOR_ID     0x00
#define PCIE20_COMMAND_STATUS          0x04
#define PCIE20_CLASS_CODE_REVISION_ID  0x08
#define PCIE20_BIST_HDR_TYPE           0x0C
#define PCIE20_BAR0                    0x10
#define PCIE20_SUBSYSTEM               0x2c
#define PCIE20_CAP_ID_NXT_PTR          0x40
#define PCIE20_CON_STATUS              0x44
#define PCIE20_MSI_CAP_ID_NEXT_CTRL    0x50
#define PCIE20_MSI_LOWER               0x54
#define PCIE20_MSI_UPPER               0x58
#define PCIE20_MSI_DATA                0x5C
#define PCIE20_MSI_MASK                0x60
#define PCIE20_DEVICE_CAPABILITIES     0x74
#define PCIE20_MASK_EP_L1_ACCPT_LATENCY 0xE00
#define PCIE20_MASK_EP_L0S_ACCPT_LATENCY 0x1C0
#define PCIE20_LINK_CAPABILITIES       0x7C
#define PCIE20_MASK_CLOCK_POWER_MAN    0x40000
#define PCIE20_MASK_L1_EXIT_LATENCY    0x38000
#define PCIE20_MASK_L0S_EXIT_LATENCY   0x7000
#define PCIE20_CAP_LINKCTRLSTATUS      0x80
#define PCIE20_DEVICE_CONTROL2_STATUS2 0x98
#define PCIE20_LINK_CONTROL2_LINK_STATUS2 0xA0
#define PCIE20_L1SUB_CAPABILITY        0x154
#define PCIE20_L1SUB_CONTROL1          0x158
#define PCIE20_ACK_F_ASPM_CTRL_REG     0x70C
#define PCIE20_MASK_ACK_N_FTS          0xff00
#define PCIE20_MISC_CONTROL_1          0x8BC

#define PCIE20_PLR_IATU_VIEWPORT       0x900
#define PCIE20_PLR_IATU_CTRL1          0x904
#define PCIE20_PLR_IATU_CTRL2          0x908
#define PCIE20_PLR_IATU_LBAR           0x90C
#define PCIE20_PLR_IATU_UBAR           0x910
#define PCIE20_PLR_IATU_LAR            0x914
#define PCIE20_PLR_IATU_LTAR           0x918
#define PCIE20_PLR_IATU_UTAR           0x91c

#define PCIE20_MHICFG                  0x110
#define PCIE20_BHI_EXECENV             0x228
#define PCIE20_MHIVER                  0x108
#define PCIE20_MHICTRL                 0x138
#define PCIE20_MHISTATUS               0x148
#define PCIE20_BHI_VERSION_LOWER	0x200
#define PCIE20_BHI_VERSION_UPPER	0x204
#define PCIE20_BHI_INTVEC		0x220

#define PCIE20_AUX_CLK_FREQ_REG        0xB40

#define PERST_TIMEOUT_US_MIN	              1000
#define PERST_TIMEOUT_US_MAX	              1000
#define PERST_CHECK_MAX_COUNT		      30000
#define LINK_UP_TIMEOUT_US_MIN	              1000
#define LINK_UP_TIMEOUT_US_MAX	              1000
#define LINK_UP_CHECK_MAX_COUNT		      30000
#define BME_TIMEOUT_US_MIN	              1000
#define BME_TIMEOUT_US_MAX	              1000
#define BME_CHECK_MAX_COUNT		      30000
#define PHY_STABILIZATION_DELAY_US_MIN	      1000
#define PHY_STABILIZATION_DELAY_US_MAX	      1000
#define REFCLK_STABILIZATION_DELAY_US_MIN     1000
#define REFCLK_STABILIZATION_DELAY_US_MAX     1000
#define PHY_READY_TIMEOUT_COUNT               30000
#define MSI_EXIT_L1SS_WAIT	              10
#define MSI_EXIT_L1SS_WAIT_MAX_COUNT          100
#define XMLH_LINK_UP                          0x400
#define PARF_XMLH_LINK_UP                     0x40000000

#define MAX_PROP_SIZE 32
#define MAX_MSG_LEN 80
#define MAX_NAME_LEN 80
#define MAX_IATU_ENTRY_NUM 2

#define EP_PCIE_LOG_PAGES 50
#define EP_PCIE_MAX_VREG 2
#define EP_PCIE_MAX_CLK 6
#define EP_PCIE_MAX_PIPE_CLK 1

#define EP_PCIE_ERROR -30655
#define EP_PCIE_LINK_DOWN 0xFFFFFFFF

#define EP_PCIE_OATU_INDEX_MSI 1
#define EP_PCIE_OATU_INDEX_CTRL 2
#define EP_PCIE_OATU_INDEX_DATA 3

#define EP_PCIE_OATU_UPPER 0x100

#define EP_PCIE_GEN_DBG(x...) do { \
	if (ep_pcie_get_debug_mask()) \
		pr_alert(x); \
	else \
		pr_debug(x); \
	} while (0)

#define EP_PCIE_DBG(dev, fmt, arg...) do {			 \
	if ((dev)->ipc_log_ful)   \
		ipc_log_string((dev)->ipc_log_ful, "%s: " fmt, __func__, arg); \
	if (ep_pcie_get_debug_mask())   \
		pr_alert("%s: " fmt, __func__, arg);		  \
	} while (0)

#define EP_PCIE_DBG2(dev, fmt, arg...) do {			 \
	if ((dev)->ipc_log_sel)   \
		ipc_log_string((dev)->ipc_log_sel, \
			"DBG1:%s: " fmt, __func__, arg); \
	if ((dev)->ipc_log_ful)   \
		ipc_log_string((dev)->ipc_log_ful, \
			"DBG2:%s: " fmt, __func__, arg); \
	if (ep_pcie_get_debug_mask())   \
		pr_alert("%s: " fmt, __func__, arg); \
	} while (0)

#define EP_PCIE_DBG_FS(fmt, arg...) pr_alert("%s: " fmt, __func__, arg)

#define EP_PCIE_DUMP(dev, fmt, arg...) do {			\
	if ((dev)->ipc_log_dump) \
		ipc_log_string((dev)->ipc_log_dump, \
			"DUMP:%s: " fmt, __func__, arg); \
	if (ep_pcie_get_debug_mask())   \
		pr_alert("%s: " fmt, __func__, arg); \
	} while (0)

#define EP_PCIE_INFO(dev, fmt, arg...) do {			 \
	if ((dev)->ipc_log_sel)   \
		ipc_log_string((dev)->ipc_log_sel, \
			"INFO:%s: " fmt, __func__, arg); \
	if ((dev)->ipc_log_ful)   \
		ipc_log_string((dev)->ipc_log_ful, "%s: " fmt, __func__, arg); \
	pr_info("%s: " fmt, __func__, arg);  \
	} while (0)

#define EP_PCIE_ERR(dev, fmt, arg...) do {			 \
	if ((dev)->ipc_log_sel)   \
		ipc_log_string((dev)->ipc_log_sel, \
			"ERR:%s: " fmt, __func__, arg); \
	if ((dev)->ipc_log_ful)   \
		ipc_log_string((dev)->ipc_log_ful, "%s: " fmt, __func__, arg); \
	pr_err("%s: " fmt, __func__, arg);  \
	} while (0)

enum ep_pcie_res {
	EP_PCIE_RES_PARF,
	EP_PCIE_RES_PHY,
	EP_PCIE_RES_MMIO,
	EP_PCIE_RES_MSI,
	EP_PCIE_RES_DM_CORE,
	EP_PCIE_RES_ELBI,
	EP_PCIE_MAX_RES,
};

enum ep_pcie_irq {
	EP_PCIE_INT_PM_TURNOFF,
	EP_PCIE_INT_DSTATE_CHANGE,
	EP_PCIE_INT_L1SUB_TIMEOUT,
	EP_PCIE_INT_LINK_UP,
	EP_PCIE_INT_LINK_DOWN,
	EP_PCIE_INT_BRIDGE_FLUSH_N,
	EP_PCIE_INT_BME,
	EP_PCIE_INT_GLOBAL,
	EP_PCIE_MAX_IRQ,
};

enum ep_pcie_gpio {
	EP_PCIE_GPIO_PERST,
	EP_PCIE_GPIO_WAKE,
	EP_PCIE_GPIO_CLKREQ,
	EP_PCIE_GPIO_MDM2AP,
	EP_PCIE_MAX_GPIO,
};

struct ep_pcie_gpio_info_t {
	char  *name;
	u32   num;
	bool  out;
	u32   on;
	u32   init;
};

struct ep_pcie_vreg_info_t {
	struct regulator  *hdl;
	char              *name;
	u32           max_v;
	u32           min_v;
	u32           opt_mode;
	bool          required;
};

struct ep_pcie_clk_info_t {
	struct clk  *hdl;
	char        *name;
	u32         freq;
	bool        required;
};

struct ep_pcie_res_info_t {
	char            *name;
	struct resource *resource;
	void __iomem    *base;
};

struct ep_pcie_irq_info_t {
	char         *name;
	u32          num;
};

/* phy info structure */
struct ep_pcie_phy_info_t {
	u32	offset;
	u32	val;
	u32	delay;
	u32	direction;
};

/* pcie endpoint device structure */
struct ep_pcie_dev_t {
	struct platform_device       *pdev;
	struct regulator             *gdsc;
	struct ep_pcie_vreg_info_t   vreg[EP_PCIE_MAX_VREG];
	struct ep_pcie_gpio_info_t   gpio[EP_PCIE_MAX_GPIO];
	struct ep_pcie_clk_info_t    clk[EP_PCIE_MAX_CLK];
	struct ep_pcie_clk_info_t    pipeclk[EP_PCIE_MAX_PIPE_CLK];
	struct ep_pcie_irq_info_t    irq[EP_PCIE_MAX_IRQ];
	struct ep_pcie_res_info_t    res[EP_PCIE_MAX_RES];

	void __iomem                 *parf;
	void __iomem                 *phy;
	void __iomem                 *mmio;
	void __iomem                 *msi;
	void __iomem                 *dm_core;
	void __iomem                 *elbi;

	struct msm_bus_scale_pdata   *bus_scale_table;
	u32                          bus_client;
	u32                          link_speed;
	bool                         active_config;
	bool                         aggregated_irq;
	bool                         mhi_a7_irq;
	u32                          dbi_base_reg;
	u32                          slv_space_reg;
	u32                          phy_status_reg;
	u32                          phy_init_len;
	struct ep_pcie_phy_info_t    *phy_init;
	bool                         perst_enum;

	u32                          rev;
	u32                          phy_rev;
	void                         *ipc_log_sel;
	void                         *ipc_log_ful;
	void                         *ipc_log_dump;
	struct mutex                 setup_mtx;
	struct mutex                 ext_mtx;
	spinlock_t                   ext_lock;
	unsigned long                ext_save_flags;

	spinlock_t                   isr_lock;
	unsigned long                isr_save_flags;
	ulong                        linkdown_counter;
	ulong                        linkup_counter;
	ulong                        bme_counter;
	ulong                        pm_to_counter;
	ulong                        d0_counter;
	ulong                        d3_counter;
	ulong                        perst_ast_counter;
	ulong                        perst_deast_counter;
	ulong                        wake_counter;
	ulong                        msi_counter;
	ulong                        global_irq_counter;

	bool                         dump_conf;
	bool                         config_mmio_init;
	bool                         enumerated;
	enum ep_pcie_link_status     link_status;
	bool                         perst_deast;
	bool                         power_on;
	bool                         suspending;
	bool                         l23_ready;
	bool                         l1ss_enabled;
	struct ep_pcie_msi_config    msi_cfg;
	bool                         no_notify;
	bool                         client_ready;

	struct ep_pcie_register_event *event_reg;
	struct work_struct	     handle_perst_work;
	struct work_struct           handle_bme_work;
	struct work_struct           handle_d3cold_work;
};

extern struct ep_pcie_dev_t ep_pcie_dev;
extern struct ep_pcie_hw hw_drv;

static inline void ep_pcie_write_mask(void __iomem *addr,
				u32 clear_mask, u32 set_mask)
{
	u32 val;

	val = (readl_relaxed(addr) & ~clear_mask) | set_mask;
	writel_relaxed(val, addr);
	/* ensure register write goes through before next regiser operation */
	wmb();
}

static inline void ep_pcie_write_reg(void __iomem *base, u32 offset, u32 value)
{
	writel_relaxed(value, base + offset);
	/* ensure register write goes through before next regiser operation */
	wmb();
}

static inline void ep_pcie_write_reg_field(void __iomem *base, u32 offset,
	const u32 mask, u32 val)
{
	u32 shift = find_first_bit((void *)&mask, 32);
	u32 tmp = readl_relaxed(base + offset);

	tmp &= ~mask; /* clear written bits */
	val = tmp | (val << shift);
	writel_relaxed(val, base + offset);
	/* ensure register write goes through before next regiser operation */
	wmb();
}

extern int ep_pcie_core_register_event(struct ep_pcie_register_event *reg);
extern int ep_pcie_get_debug_mask(void);
extern void ep_pcie_phy_init(struct ep_pcie_dev_t *dev);
extern bool ep_pcie_phy_is_ready(struct ep_pcie_dev_t *dev);
extern void ep_pcie_reg_dump(struct ep_pcie_dev_t *dev, u32 sel, bool linkdown);
extern void ep_pcie_debugfs_init(struct ep_pcie_dev_t *ep_dev);
extern void ep_pcie_debugfs_exit(void);

#endif
