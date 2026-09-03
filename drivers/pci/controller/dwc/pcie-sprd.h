/* SPDX-License-Identifier: GPL-2.0 */
/*
 * PCIe host controller driver for Unisoc SoCs
 *
 * Copyright (C) 2020 Unisoc Inc.
 * http://www.unisoc.com
 */

#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/regmap.h>
#include <linux/pcie-rc-sprd.h>
#include <linux/spinlock_types.h>

#include "pcie-designware.h"

#define LTSSM_STATE_L0			0x11
#define LTSSM_STATE_L0S			0x12
#define LTSSM_STATE_L1_IDLE		0x14
#define LTSSM_STATE_L2_IDLE		0x15

#define SPRD_PCI_EXP_CAP		0x70

/*
 * WORKAROUND: set CX_FLT_MASK_UR_POIS bit to fix pam ipa pcie access error
 * problem.
 */
#define PCIE_SYMBOL_TIMER_FILTER_1_OFF	0x71c
#define CX_FLT_MASK_UR_POIS		(0x1 << 17)

/*
 * TODO: The following register only for roc1 PCIe gen2 (pcie0),
 * but it's a common PCIe capability.
 * Different PCIe host controller has different offset address.
 */
#define SPRD_PCIE_GEN2_L1SS_CAP		0x154
#define  PCIE_T_POWER_ON_SCALE_MASK	(0x3 << 16)
#define  PCIE_T_POWER_ON_SCALE		(0x1 << 16)
#define  PCIE_T_POWER_ON_VALUE_MASK	(0x1f << 19)
#define  PCIE_T_POWER_ON_VALUE		(0xf << 19)

/*
 * Shadow registers (e.g. BAR mask registers) can be accessed
 * through this register.
 * If you write 0 to (Base address + 0x8000 + 0x10), then BAR0
 * can be set to 0.
 */
#define PCIE_DBI_CS2 0x8000

/* Although this is synopsys defined register, only spreadtrum pcie need it. */
#define PCIE_SS_REG_BASE		0xE00
#define PE0_GEN_CTRL_3			0x58
#define  LTSSM_EN			(0x1 << 0)
#define  L1_AUXCLK_EN			(0x1 << 8)
#define APB_CLKFREQ_TIMEOUT		0x4
#define  BUSERR_EN			(0x1 << 12)
#define  APB_CLKFREQ			0x19a

#define SPRD_PCIE_PE0_PM_CTRL			0xe60
#define SPRD_PCIE_APP_CLK_PM_EN			(0x1 << 21)

#define SPRD_PCIE_PE0_PM_STS			0xe64
#define SPRD_PCIE_PM_CURRENT_STATE_MASK		(0x7 << 8)
#define SPRD_PCIE_L0s				(0x1 << 8)
#define SPRD_PCIE_L1				(0x2 << 8)
#define SPRD_PCIE_L2				(0x3 << 8)
#define SPRD_PCIE_L3				(0x4 << 8)

#define SPRD_PCIE_PE0_TX_MSG_REG		0xe80
#define SPRD_PCIE_PME_TURN_OFF_REQ		(0x1 << 19)

#define SPRD_PCIE_DBI_ATU_OFFSET	0x18000

#define ENTER_L2_MAX_RETRIES	10

#define PCI_DEVICE_ID_SPRD_RC	0xabcd
#define PCI_DEVICE_ID_MARLIN3	0x2355
#define PCI_VENDOR_ID_MARLIN3	0x1db3

#define PCI_BAR_NUM	6
#define PCI_BAR_EN	1

#define SPRD_PCIE_RST_CTRL			0xe4c
#define SPRD_SOFT_WAKE				(0x1 << 4)

struct sprd_pcie {
	const char *label;
	struct dw_pcie *pci;
	struct clk *pcie_eb;
	struct mutex sprd_pcie_mutex;
	spinlock_t lock;

#ifdef CONFIG_SPRD_IPA_INTC
	/* These irq lines are connected to ipa level2 interrupt controller */
	u32 interrupt_line;
	u32 pme_irq;
#endif

	/* These irq lines are connected to GIC */
	u32 aer_irq;

	/* this irq cames from EIC to GIC */
	int wakeup_irq;
	u32 wake_down_irq_cnt;
	u32 wake_down_cnt;
	struct gpio_desc *gpiod_wakeup;

	int perst_irq;
	struct gpio_desc *gpiod_perst;

	/* Save sysnopsys-specific PCIe configuration registers  */
	u32 save_msi_ctrls[MAX_MSI_CTRLS][3];

	/* keep track of pcie rc state */
	unsigned int is_powered:1;
	unsigned int is_suspended:1;
	struct regulator *vpower;
	unsigned int is_wakedown:1;

	/* when pci enter suspend cannot reinit the pci */
	unsigned int reinit_disable:1;

	/* When the EP is powered on late, we need set it */
	unsigned int defer_init:1;
	int retries;
#ifdef CONFIG_PM_SLEEP
	struct notifier_block	pm_notify;
#endif

	struct sprd_pcie_register_event *event_reg;

	wait_queue_head_t action_wait;
	struct task_struct *action_thread;
	struct list_head action_list;

	struct timer_list timer;

	size_t label_len; /* pcie controller device length + 10 */
	char wakeup_label[0];
};

struct sprd_pcie_of_data {
	enum dw_pcie_device_mode mode;
};

int sprd_pcie_syscon_setting(struct platform_device *pdev, char *evn);
int sprd_pcie_syscon_cache_setting(struct platform_device *pdev, char *evn, bool enable);
void sprd_pcie_clear_unhandled_msi(struct dw_pcie *pci);
void sprd_pcie_save_dwc_reg(struct dw_pcie *pci);
void sprd_pcie_restore_dwc_reg(struct dw_pcie *pci);
int sprd_pcie_enter_pcipm_l2(struct dw_pcie *pci);
int sprd_pcie_check_vendor_id(struct dw_pcie *pci);
void sprd_pcie_check_atu_viewport(struct dw_pcie *pci);
void sprd_pcie_ltssm_enable(struct dw_pcie *pci, bool enable);
#ifdef CONFIG_PM_SLEEP
void sprd_pcie_register_pm_notifier(struct sprd_pcie *ctrl);
void sprd_pcie_unregister_pm_notifier(struct sprd_pcie *ctrl);
#else
static inline void sprd_pcie_register_pm_notifier(struct sprd_pcie *ctrl) { }
static inline void sprd_pcie_unregister_pm_notifier(struct sprd_pcie *ctrl) { }
#endif
