/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_PCIE_H
#define __ARCH_ARM_MACH_MSM_PCIE_H

#include <linux/clk.h>
#include <linux/compiler.h>
#include <linux/ipc_logging.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>
#include <linux/pm_wakeup.h>
#include <mach/msm_pcie.h>

#define MSM_PCIE_MAX_VREG 3
#define MSM_PCIE_MAX_CLK 6
#define MSM_PCIE_MAX_PIPE_CLK 1

#define MAX_RC_NUM 2

#ifdef CONFIG_ARM_LPAE
#define PCIE_UPPER_ADDR(addr) ((u32)((addr) >> 32))
#else
#define PCIE_UPPER_ADDR(addr) (0x0)
#endif
#define PCIE_LOWER_ADDR(addr) ((u32)((addr) & 0xffffffff))

#define PCIE_MSI_NR_IRQS 256

#define PCIE_LOG_PAGES (50)

#define PCIE_DBG(dev, fmt, arg...) do {			 \
	if ((dev) && (dev)->ipc_log_long)   \
		ipc_log_string((dev)->ipc_log_long, \
			"DBG1:%s: " fmt, __func__, arg); \
	if ((dev) && (dev)->ipc_log)   \
		ipc_log_string((dev)->ipc_log, "%s: " fmt, __func__, arg); \
	if (msm_pcie_get_debug_mask())   \
		pr_alert("%s: " fmt, __func__, arg);              \
	} while (0)

#define PCIE_DBG2(dev, fmt, arg...) do {			 \
	if ((dev) && (dev)->ipc_log)   \
		ipc_log_string((dev)->ipc_log, "DBG2:%s: " fmt, __func__, arg);\
	if (msm_pcie_get_debug_mask())   \
		pr_alert("%s: " fmt, __func__, arg);              \
	} while (0)

#define PCIE_DBG3(dev, fmt, arg...) do {			 \
	if ((dev) && (dev)->ipc_log)   \
		ipc_log_string((dev)->ipc_log, "DBG3:%s: " fmt, __func__, arg);\
	if (msm_pcie_get_debug_mask())   \
		pr_alert("%s: " fmt, __func__, arg);              \
	} while (0)

#define PCIE_INFO(dev, fmt, arg...) do {			 \
	if ((dev) && (dev)->ipc_log_long)   \
		ipc_log_string((dev)->ipc_log_long, \
			"INFO:%s: " fmt, __func__, arg); \
	if ((dev) && (dev)->ipc_log)   \
		ipc_log_string((dev)->ipc_log, "%s: " fmt, __func__, arg); \
	pr_info("%s: " fmt, __func__, arg);  \
	} while (0)

#define PCIE_ERR(dev, fmt, arg...) do {			 \
	if ((dev) && (dev)->ipc_log_long)   \
		ipc_log_string((dev)->ipc_log_long, \
			"ERR:%s: " fmt, __func__, arg); \
	if ((dev) && (dev)->ipc_log)   \
		ipc_log_string((dev)->ipc_log, "%s: " fmt, __func__, arg); \
	pr_err("%s: " fmt, __func__, arg);  \
	} while (0)

#define PCIE_BUS_PRIV_DATA(pdev) \
	(((struct pci_sys_data *)pdev->bus->sysdata)->private_data)

/* PM control options */
#define PM_IRQ                   0x1
#define PM_CLK                   0x2
#define PM_GPIO                  0x4
#define PM_VREG                  0x8
#define PM_PIPE_CLK              0x10
#define PM_ALL (PM_IRQ | PM_CLK | PM_GPIO | PM_VREG | PM_PIPE_CLK)

#define PCIE_CONF_SPACE_DW		      1024
#define PCIE_CLEAR			      0xDEADBEEF
#define PCIE_LINK_DOWN                        0xFFFFFFFF

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
	MSM_PCIE_INT_WAKE,
	MSM_PCIE_MAX_IRQ,
};

/* gpios */
enum msm_pcie_gpio {
	MSM_PCIE_GPIO_PERST,
	MSM_PCIE_GPIO_WAKE,
	MSM_PCIE_GPIO_CLKREQ,
	MSM_PCIE_MAX_GPIO
};

enum msm_pcie_link_status {
	MSM_PCIE_LINK_DEINIT,
	MSM_PCIE_LINK_ENABLED,
	MSM_PCIE_LINK_DISABLED
};

/* gpio info structure */
struct msm_pcie_gpio_info_t {
	char      *name;
	uint32_t   num;
	bool       out;
	uint32_t   on;
	uint32_t   init;
};

/* voltage regulator info structrue */
struct msm_pcie_vreg_info_t {
	struct regulator  *hdl;
	char              *name;
	uint32_t           max_v;
	uint32_t           min_v;
	uint32_t           opt_mode;
	bool               required;
};

/* clock info structure */
struct msm_pcie_clk_info_t {
	struct clk  *hdl;
	char        *name;
	u32         freq;
	bool        required;
};

/* resource info structure */
struct msm_pcie_res_info_t {
	char            *name;
	struct resource *resource;
	void __iomem    *base;
};

/* irq info structrue */
struct msm_pcie_irq_info_t {
	char              *name;
	uint32_t          num;
};

/* msm pcie device structure */
struct msm_pcie_dev_t {
	struct platform_device       *pdev;
	struct pci_dev *dev;
	struct regulator *gdsc;
	struct msm_pcie_vreg_info_t  vreg[MSM_PCIE_MAX_VREG];
	struct msm_pcie_gpio_info_t  gpio[MSM_PCIE_MAX_GPIO];
	struct msm_pcie_clk_info_t   clk[MSM_PCIE_MAX_CLK];
	struct msm_pcie_clk_info_t   pipeclk[MSM_PCIE_MAX_PIPE_CLK];
	struct msm_pcie_res_info_t   res[MSM_PCIE_MAX_RES];
	struct msm_pcie_irq_info_t   irq[MSM_PCIE_MAX_IRQ];

	void __iomem                 *parf;
	void __iomem                 *phy;
	void __iomem                 *elbi;
	void __iomem                 *dm_core;
	void __iomem                 *conf;
	void __iomem                 *bars;

	uint32_t                      axi_bar_start;
	uint32_t                      axi_bar_end;

	struct resource               *dev_mem_res;
	struct resource               *dev_io_res;

	uint32_t                      wake_n;
	uint32_t                      vreg_n;
	uint32_t                      gpio_n;
	uint32_t                      parf_deemph;
	uint32_t                      parf_swing;

	bool                         cfg_access;
	spinlock_t                   cfg_lock;
	unsigned long                irqsave_flags;
	struct mutex                 setup_lock;

	struct irq_domain            *irq_domain;
	DECLARE_BITMAP(msi_irq_in_use, PCIE_MSI_NR_IRQS);
	uint32_t                     msi_gicm_addr;
	uint32_t                     msi_gicm_base;
	bool                         use_msi;

	enum msm_pcie_link_status    link_status;
	bool                         user_suspend;
	struct pci_saved_state	     *saved_state;

	struct wakeup_source	     ws;
	struct msm_bus_scale_pdata   *bus_scale_table;
	uint32_t                     bus_client;

	bool                         l1ss_supported;
	bool                         aux_clk_sync;
	uint32_t                     n_fts;
	bool                         ext_ref_clk;
	uint32_t                     ep_latency;
	bool                         ep_wakeirq;

	uint32_t                     rc_idx;
	bool                         enumerated;
	struct work_struct	     handle_wake_work;
	struct mutex                 recovery_lock;
	spinlock_t                   linkdown_lock;
	spinlock_t                   wakeup_lock;
	ulong                        linkdown_counter;
	bool                         suspending;
	ulong                        wake_counter;
	ulong                        req_exit_l1_counter;
	u32			     ep_shadow[PCIE_CONF_SPACE_DW];
	u32                          rc_shadow[PCIE_CONF_SPACE_DW];
	bool                         shadow_en;
	struct msm_pcie_register_event *event_reg;
	bool                         power_on;
	void                         *ipc_log;
	void                         *ipc_log_long;
};

extern int msm_pcie_enumerate(u32 rc_idx);
extern int msm_pcie_enable(struct msm_pcie_dev_t *dev, u32 options);
extern void msm_pcie_disable(struct msm_pcie_dev_t *dev, u32 options);
extern void msm_pcie_cfg_recover(struct msm_pcie_dev_t *dev, bool rc);
extern void msm_pcie_config_msi_controller(struct msm_pcie_dev_t *dev);
extern int32_t msm_pcie_irq_init(struct msm_pcie_dev_t *dev);
extern void msm_pcie_irq_deinit(struct msm_pcie_dev_t *dev);
extern int msm_pcie_get_debug_mask(void);
extern bool msm_pcie_confirm_linkup(struct msm_pcie_dev_t *dev,
			bool check_sw_stts, bool check_ep);

extern void pcie_phy_init(struct msm_pcie_dev_t *dev);
extern bool pcie_phy_is_ready(struct msm_pcie_dev_t *dev);

#endif
