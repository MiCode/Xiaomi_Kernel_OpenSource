// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2016-2021, The Linux Foundation. All rights reserved. */

#include <linux/cma.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/pm_runtime.h>
#include <linux/memblock.h>
#include <linux/completion.h>

#include "main.h"
#include "bus.h"
#include "debug.h"
#include "pci.h"
#include "reg.h"

#define PCI_LINK_UP			1
#define PCI_LINK_DOWN			0

#define SAVE_PCI_CONFIG_SPACE		1
#define RESTORE_PCI_CONFIG_SPACE	0

#define PM_OPTIONS_DEFAULT		0
#define PM_OPTIONS_LINK_DOWN \
	(MSM_PCIE_CONFIG_NO_CFG_RESTORE | MSM_PCIE_CONFIG_LINKDOWN)

#define PCI_BAR_NUM			0

#define PCI_DMA_MASK_32_BIT		32
#define PCI_DMA_MASK_64_BIT		64

#define MHI_NODE_NAME			"qcom,mhi"
#define MHI_MSI_NAME			"MHI"

#define QCA6390_PATH_PREFIX		"qca6390/"
#define QCA6490_PATH_PREFIX		"qca6490/"
#define DEFAULT_M3_FILE_NAME		"m3.bin"
#define DEFAULT_FW_FILE_NAME		"amss.bin"
#define FW_V2_FILE_NAME			"amss20.bin"
#define FW_V2_NUMBER			2

#define WAKE_MSI_NAME			"WAKE"

#define DEV_RDDM_TIMEOUT		5000
#define WAKE_EVENT_TIMEOUT		5000

#ifdef CONFIG_CNSS_EMULATION
#define EMULATION_HW			1
#else
#define EMULATION_HW			0
#endif

static DEFINE_SPINLOCK(pci_link_down_lock);
static DEFINE_SPINLOCK(pci_reg_window_lock);
static DEFINE_SPINLOCK(time_sync_lock);

#define MHI_TIMEOUT_OVERWRITE_MS	(plat_priv->ctrl_params.mhi_timeout)
#define MHI_M2_TIMEOUT_MS		(plat_priv->ctrl_params.mhi_m2_timeout)

#define WLAON_PWR_CTRL_SHUTDOWN_DELAY_MIN_US	1000
#define WLAON_PWR_CTRL_SHUTDOWN_DELAY_MAX_US	2000

#define FORCE_WAKE_DELAY_MIN_US			4000
#define FORCE_WAKE_DELAY_MAX_US			6000
#define FORCE_WAKE_DELAY_TIMEOUT_US		60000

#define POWER_ON_RETRY_MAX_TIMES		3
#define POWER_ON_RETRY_DELAY_MS			200

#define LINK_TRAINING_RETRY_MAX_TIMES		3
#define LINK_TRAINING_RETRY_DELAY_MS		500

#define HANG_DATA_LENGTH		384
#define HST_HANG_DATA_OFFSET		((3 * 1024 * 1024) - HANG_DATA_LENGTH)
#define HSP_HANG_DATA_OFFSET		((2 * 1024 * 1024) - HANG_DATA_LENGTH)

#define MHI_SUSPEND_RETRY_CNT		3

static struct cnss_pci_reg ce_src[] = {
	{ "SRC_RING_BASE_LSB", QCA6390_CE_SRC_RING_BASE_LSB_OFFSET },
	{ "SRC_RING_BASE_MSB", QCA6390_CE_SRC_RING_BASE_MSB_OFFSET },
	{ "SRC_RING_ID", QCA6390_CE_SRC_RING_ID_OFFSET },
	{ "SRC_RING_MISC", QCA6390_CE_SRC_RING_MISC_OFFSET },
	{ "SRC_CTRL", QCA6390_CE_SRC_CTRL_OFFSET },
	{ "SRC_R0_CE_CH_SRC_IS", QCA6390_CE_SRC_R0_CE_CH_SRC_IS_OFFSET },
	{ "SRC_RING_HP", QCA6390_CE_SRC_RING_HP_OFFSET },
	{ "SRC_RING_TP", QCA6390_CE_SRC_RING_TP_OFFSET },
	{ NULL },
};

static struct cnss_pci_reg ce_dst[] = {
	{ "DEST_RING_BASE_LSB", QCA6390_CE_DEST_RING_BASE_LSB_OFFSET },
	{ "DEST_RING_BASE_MSB", QCA6390_CE_DEST_RING_BASE_MSB_OFFSET },
	{ "DEST_RING_ID", QCA6390_CE_DEST_RING_ID_OFFSET },
	{ "DEST_RING_MISC", QCA6390_CE_DEST_RING_MISC_OFFSET },
	{ "DEST_CTRL", QCA6390_CE_DEST_CTRL_OFFSET },
	{ "CE_CH_DST_IS", QCA6390_CE_CH_DST_IS_OFFSET },
	{ "CE_CH_DEST_CTRL2", QCA6390_CE_CH_DEST_CTRL2_OFFSET },
	{ "DEST_RING_HP", QCA6390_CE_DEST_RING_HP_OFFSET },
	{ "DEST_RING_TP", QCA6390_CE_DEST_RING_TP_OFFSET },
	{ "STATUS_RING_BASE_LSB", QCA6390_CE_STATUS_RING_BASE_LSB_OFFSET },
	{ "STATUS_RING_BASE_MSB", QCA6390_CE_STATUS_RING_BASE_MSB_OFFSET },
	{ "STATUS_RING_ID", QCA6390_CE_STATUS_RING_ID_OFFSET },
	{ "STATUS_RING_MISC", QCA6390_CE_STATUS_RING_MISC_OFFSET },
	{ "STATUS_RING_HP", QCA6390_CE_STATUS_RING_HP_OFFSET },
	{ "STATUS_RING_TP", QCA6390_CE_STATUS_RING_TP_OFFSET },
	{ NULL },
};

static struct cnss_pci_reg ce_cmn[] = {
	{ "GXI_ERR_INTS", QCA6390_CE_COMMON_GXI_ERR_INTS },
	{ "GXI_ERR_STATS", QCA6390_CE_COMMON_GXI_ERR_STATS },
	{ "GXI_WDOG_STATUS", QCA6390_CE_COMMON_GXI_WDOG_STATUS },
	{ "TARGET_IE_0", QCA6390_CE_COMMON_TARGET_IE_0 },
	{ "TARGET_IE_1", QCA6390_CE_COMMON_TARGET_IE_1 },
	{ NULL },
};

static struct cnss_pci_reg qdss_csr[] = {
	{ "QDSSCSR_ETRIRQCTRL", QDSS_APB_DEC_CSR_ETRIRQCTRL_OFFSET },
	{ "QDSSCSR_PRESERVEETF", QDSS_APB_DEC_CSR_PRESERVEETF_OFFSET },
	{ "QDSSCSR_PRESERVEETR0", QDSS_APB_DEC_CSR_PRESERVEETR0_OFFSET },
	{ "QDSSCSR_PRESERVEETR1", QDSS_APB_DEC_CSR_PRESERVEETR1_OFFSET },
	{ NULL },
};

static struct cnss_misc_reg wcss_reg_access_seq[] = {
	{0, QCA6390_GCC_DEBUG_CLK_CTL, 0},
	{1, QCA6390_GCC_DEBUG_CLK_CTL, 0x802},
	{0, QCA6390_GCC_DEBUG_CLK_CTL, 0},
	{0, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_PLL_MODE, 0},
	{1, QCA6390_GCC_DEBUG_CLK_CTL, 0x805},
	{0, QCA6390_GCC_DEBUG_CLK_CTL, 0},
	{0, QCA6390_WCSS_WFSS_PMM_WFSS_PMM_R0_PMM_CTRL, 0},
	{0, QCA6390_WCSS_PMM_TOP_PMU_CX_CSR, 0},
	{0, QCA6390_WCSS_PMM_TOP_AON_INT_RAW_STAT, 0},
	{0, QCA6390_WCSS_PMM_TOP_AON_INT_EN, 0},
	{0, QCA6390_WCSS_PMM_TOP_PMU_TESTBUS_STS, 0},
	{1, QCA6390_WCSS_PMM_TOP_PMU_TESTBUS_CTL, 0xD},
	{0, QCA6390_WCSS_PMM_TOP_TESTBUS_STS, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_SAW2_CFG, 0},
	{1, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_SAW2_CFG, 0},
	{1, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_CTL, 0x8},
	{0, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_VALUE, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_SAW2_SPM_STS, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_SAW2_SPM_CTL, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_SAW2_SPM_SLP_SEQ_ENTRY_0, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_SAW2_SPM_SLP_SEQ_ENTRY_9, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_STATUS0, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_STATUS1, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_STATUS2, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_STATUS3, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_STATUS4, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_STATUS5, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_STATUS6, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_ENABLE0, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_ENABLE1, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_ENABLE2, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_ENABLE3, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_ENABLE4, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_ENABLE5, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_ENABLE6, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_PENDING0, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_PENDING1, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_PENDING2, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_PENDING3, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_PENDING4, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_PENDING5, 0},
	{0, QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_L2VIC_INT_PENDING6, 0},
	{1, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_CTL, 0x30040},
	{0, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_CTL, 0},
	{0, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_VALUE, 0},
	{0, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_VALUE, 0},
	{0, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_VALUE, 0},
	{1, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_CTL, 0x30105},
	{0, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_CTL, 0},
	{0, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_VALUE, 0},
	{0, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_VALUE, 0},
	{0, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_VALUE, 0},
	{0, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_VALUE, 0},
	{0, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_VALUE, 0},
	{0, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_VALUE, 0},
	{0, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_VALUE, 0},
	{0, QCA6390_WCSS_Q6SS_PUBCSR_QDSP6SS_TEST_BUS_CTL, 0},
	{0, QCA6390_WCSS_CC_WCSS_UMAC_NOC_CBCR, 0},
	{0, QCA6390_WCSS_CC_WCSS_UMAC_AHB_CBCR, 0},
	{0, QCA6390_WCSS_CC_WCSS_UMAC_GDSCR, 0},
	{0, QCA6390_WCSS_CC_WCSS_WLAN1_GDSCR, 0},
	{0, QCA6390_WCSS_CC_WCSS_WLAN2_GDSCR, 0},
	{0, QCA6390_WCSS_PMM_TOP_PMM_INT_CLR, 0},
	{0, QCA6390_WCSS_PMM_TOP_AON_INT_STICKY_EN, 0},
};

static struct cnss_misc_reg pcie_reg_access_seq[] = {
	{0, QCA6390_PCIE_PCIE_WCSS_STATUS_FOR_DEBUG_LOW_PCIE_LOCAL_REG, 0},
	{0, QCA6390_PCIE_SOC_PCIE_WRAP_INTR_MASK_SOC_PCIE_REG, 0},
	{1, QCA6390_PCIE_SOC_PCIE_WRAP_INTR_MASK_SOC_PCIE_REG, 0x18},
	{0, QCA6390_PCIE_SOC_PCIE_WRAP_INTR_MASK_SOC_PCIE_REG, 0},
	{0, QCA6390_PCIE_SOC_PCIE_WRAP_INTR_MASK_SOC_PCIE_REG, 0},
	{0, QCA6390_PCIE_SOC_PCIE_WRAP_INTR_STATUS_SOC_PCIE_REG, 0},
	{0, QCA6390_PCIE_SOC_COMMIT_REPLAY_SOC_PCIE_REG, 0},
	{0, QCA6390_TLMM_GPIO_IN_OUT57, 0},
	{0, QCA6390_TLMM_GPIO_INTR_CFG57, 0},
	{0, QCA6390_TLMM_GPIO_INTR_STATUS57, 0},
	{0, QCA6390_TLMM_GPIO_IN_OUT59, 0},
	{0, QCA6390_TLMM_GPIO_INTR_CFG59, 0},
	{0, QCA6390_TLMM_GPIO_INTR_STATUS59, 0},
	{0, QCA6390_PCIE_PCIE_PARF_LTSSM, 0},
	{0, QCA6390_PCIE_PCIE_PARF_PM_STTS, 0},
	{0, QCA6390_PCIE_PCIE_PARF_PM_STTS_1, 0},
	{0, QCA6390_PCIE_PCIE_PARF_INT_STATUS, 0},
	{0, QCA6390_PCIE_PCIE_INT_ALL_STATUS, 0},
	{0, QCA6390_PCIE_PCIE_INT_ALL_MASK, 0},
	{0, QCA6390_PCIE_PCIE_PARF_BDF_TO_SID_CFG, 0},
	{0, QCA6390_PCIE_PCIE_PARF_L1SS_SLEEP_NO_MHI_ACCESS_HANDLER_RD_4, 0},
	{0, QCA6390_PCIE_PCIE_PARF_L1SS_SLEEP_NO_MHI_ACCESS_HANDLER_RD_3, 0},
	{0, QCA6390_PCIE_PCIE_PARF_MHI_CLOCK_RESET_CTRL, 0},
	{0, QCA6390_PCIE_PCIE_PARF_MHI_BASE_ADDR_LOWER, 0},
	{0, QCA6390_PCIE_PCIE_PARF_L1SS_SLEEP_MODE_HANDLER_STATUS, 0},
	{0, QCA6390_PCIE_PCIE_PARF_L1SS_SLEEP_MODE_HANDLER_CFG, 0},
	{0, QCA6390_PCIE_PCIE_PARF_DEBUG_CNT_AUX_CLK_IN_L1SUB_L2, 0},
	{0, QCA6390_PCIE_PCIE_PARF_DEBUG_CNT_PM_LINKST_IN_L1SUB, 0},
	{0, QCA6390_PCIE_PCIE_CORE_CONFIG, 0},
	{0, QCA6390_PCIE_PCIE_PARF_L1SS_SLEEP_NO_MHI_ACCESS_HANDLER_RD_4, 0},
	{0, QCA6390_PCIE_PCIE_PARF_DEBUG_CNT_PM_LINKST_IN_L2, 0},
	{0, QCA6390_PCIE_PCIE_PARF_DEBUG_CNT_PM_LINKST_IN_L1, 0},
	{0, QCA6390_PCIE_PCIE_PARF_DEBUG_CNT_AUX_CLK_IN_L1SUB_L1, 0},
	{0, QCA6390_PCIE_PCIE_PARF_DEBUG_CNT_AUX_CLK_IN_L1SUB_L2, 0},
	{0, QCA6390_PCIE_PCIE_LOCAL_REG_WCSSAON_PCIE_SR_STATUS_HIGH, 0},
	{0, QCA6390_PCIE_PCIE_LOCAL_REG_WCSSAON_PCIE_SR_STATUS_LOW, 0},
	{0, QCA6390_PCIE_PCIE_LOCAL_REG_WCSS_STATUS_FOR_DEBUG_HIGH, 0},
	{0, QCA6390_PCIE_PCIE_LOCAL_REG_WCSS_STATUS_FOR_DEBUG_LOW, 0},
	{0, QCA6390_WFSS_PMM_WFSS_PMM_R0_WLAN1_STATUS_REG2, 0},
	{0, QCA6390_WFSS_PMM_WFSS_PMM_R0_WLAN2_STATUS_REG2, 0},
	{0, QCA6390_WFSS_PMM_WFSS_PMM_R0_PMM_WLAN2_CFG_REG1, 0},
	{0, QCA6390_WFSS_PMM_WFSS_PMM_R0_PMM_WLAN1_CFG_REG1, 0},
	{0, QCA6390_WFSS_PMM_WFSS_PMM_R0_WLAN2_APS_STATUS_REG1, 0},
	{0, QCA6390_WFSS_PMM_WFSS_PMM_R0_WLAN1_APS_STATUS_REG1, 0},
	{0, QCA6390_PCIE_PCIE_BHI_EXECENV_REG, 0},
};

static struct cnss_misc_reg wlaon_reg_access_seq[] = {
	{0, QCA6390_WLAON_WCSSAON_PCIE_SR_STATUS_HI_REG, 0},
	{0, QCA6390_WLAON_SOC_POWER_CTRL, 0},
	{0, QCA6390_WLAON_PCIE_PWR_CTRL_REG, 0},
	{0, QCA6390_WLAON_WCSSAON_PCIE_SR_STATUS_LO_REG, 0},
	{0, QCA6390_WLAON_WCSS_TCSR_PMM_SR_STATUS_HI_REG, 0},
	{0, QCA6390_WLAON_WCSS_TCSR_PMM_SR_STATUS_LO_REG, 0},
	{0, QCA6390_WLAON_SOC_POWER_CTRL, 0},
	{0, QCA6390_WLAON_SOC_PWR_WDG_BARK_THRSHD, 0},
	{0, QCA6390_WLAON_SOC_PWR_WDG_BITE_THRSHD, 0},
	{0, QCA6390_WLAON_SW_COLD_RESET, 0},
	{0, QCA6390_WLAON_RFA_MEM_SLP_NRET_N_OVERRIDE, 0},
	{0, QCA6390_WLAON_GDSC_DELAY_SETTING, 0},
	{0, QCA6390_WLAON_GDSC_DELAY_SETTING2, 0},
	{0, QCA6390_WLAON_WL_PWR_STATUS_REG, 0},
	{0, QCA6390_WLAON_WL_AON_DBG_CFG_REG, 0},
	{0, QCA6390_WLAON_GLOBAL_COUNTER_CTRL1, 0},
	{0, QCA6390_WLAON_GLOBAL_COUNTER_CTRL6, 0},
	{0, QCA6390_WLAON_GLOBAL_COUNTER_CTRL7, 0},
	{0, QCA6390_WLAON_GLOBAL_COUNTER_CTRL3, 0},
	{0, QCA6390_WLAON_GLOBAL_COUNTER_CTRL4, 0},
	{0, QCA6390_WLAON_GLOBAL_COUNTER_CTRL5, 0},
	{0, QCA6390_WLAON_GLOBAL_COUNTER_CTRL8, 0},
	{0, QCA6390_WLAON_GLOBAL_COUNTER_CTRL2, 0},
	{0, QCA6390_WLAON_GLOBAL_COUNTER_CTRL9, 0},
	{0, QCA6390_WLAON_RTC_CLK_CAL_CTRL1, 0},
	{0, QCA6390_WLAON_RTC_CLK_CAL_CTRL2, 0},
	{0, QCA6390_WLAON_RTC_CLK_CAL_CTRL3, 0},
	{0, QCA6390_WLAON_RTC_CLK_CAL_CTRL4, 0},
	{0, QCA6390_WLAON_RTC_CLK_CAL_CTRL5, 0},
	{0, QCA6390_WLAON_RTC_CLK_CAL_CTRL6, 0},
	{0, QCA6390_WLAON_RTC_CLK_CAL_CTRL7, 0},
	{0, QCA6390_WLAON_RTC_CLK_CAL_CTRL8, 0},
	{0, QCA6390_WLAON_RTC_CLK_CAL_CTRL9, 0},
	{0, QCA6390_WLAON_WCSSAON_CONFIG_REG, 0},
	{0, QCA6390_WLAON_WLAN_OEM_DEBUG_REG, 0},
	{0, QCA6390_WLAON_WLAN_RAM_DUMP_REG, 0},
	{0, QCA6390_WLAON_QDSS_WCSS_REG, 0},
	{0, QCA6390_WLAON_QDSS_WCSS_ACK, 0},
	{0, QCA6390_WLAON_WL_CLK_CNTL_KDF_REG, 0},
	{0, QCA6390_WLAON_WL_CLK_CNTL_PMU_HFRC_REG, 0},
	{0, QCA6390_WLAON_QFPROM_PWR_CTRL_REG, 0},
	{0, QCA6390_WLAON_DLY_CONFIG, 0},
	{0, QCA6390_WLAON_WLAON_Q6_IRQ_REG, 0},
	{0, QCA6390_WLAON_PCIE_INTF_SW_CFG_REG, 0},
	{0, QCA6390_WLAON_PCIE_INTF_STICKY_SW_CFG_REG, 0},
	{0, QCA6390_WLAON_PCIE_INTF_PHY_SW_CFG_REG, 0},
	{0, QCA6390_WLAON_PCIE_INTF_PHY_NOCSR_SW_CFG_REG, 0},
	{0, QCA6390_WLAON_Q6_COOKIE_BIT, 0},
	{0, QCA6390_WLAON_WARM_SW_ENTRY, 0},
	{0, QCA6390_WLAON_RESET_DBG_SW_ENTRY, 0},
	{0, QCA6390_WLAON_WL_PMUNOC_CFG_REG, 0},
	{0, QCA6390_WLAON_RESET_CAUSE_CFG_REG, 0},
	{0, QCA6390_WLAON_SOC_WCSSAON_WAKEUP_IRQ_7_EN_REG, 0},
	{0, QCA6390_WLAON_DEBUG, 0},
	{0, QCA6390_WLAON_SOC_PARAMETERS, 0},
	{0, QCA6390_WLAON_WLPM_SIGNAL, 0},
	{0, QCA6390_WLAON_SOC_RESET_CAUSE_REG, 0},
	{0, QCA6390_WLAON_WAKEUP_PCIE_SOC_REG, 0},
	{0, QCA6390_WLAON_PBL_STACK_CANARY, 0},
	{0, QCA6390_WLAON_MEM_TOT_NUM_GRP_REG, 0},
	{0, QCA6390_WLAON_MEM_TOT_BANKS_IN_GRP0_REG, 0},
	{0, QCA6390_WLAON_MEM_TOT_BANKS_IN_GRP1_REG, 0},
	{0, QCA6390_WLAON_MEM_TOT_BANKS_IN_GRP2_REG, 0},
	{0, QCA6390_WLAON_MEM_TOT_BANKS_IN_GRP3_REG, 0},
	{0, QCA6390_WLAON_MEM_TOT_SIZE_IN_GRP0_REG, 0},
	{0, QCA6390_WLAON_MEM_TOT_SIZE_IN_GRP1_REG, 0},
	{0, QCA6390_WLAON_MEM_TOT_SIZE_IN_GRP2_REG, 0},
	{0, QCA6390_WLAON_MEM_TOT_SIZE_IN_GRP3_REG, 0},
	{0, QCA6390_WLAON_MEM_SLP_NRET_OVERRIDE_GRP0_REG, 0},
	{0, QCA6390_WLAON_MEM_SLP_NRET_OVERRIDE_GRP1_REG, 0},
	{0, QCA6390_WLAON_MEM_SLP_NRET_OVERRIDE_GRP2_REG, 0},
	{0, QCA6390_WLAON_MEM_SLP_NRET_OVERRIDE_GRP3_REG, 0},
	{0, QCA6390_WLAON_MEM_SLP_RET_OVERRIDE_GRP0_REG, 0},
	{0, QCA6390_WLAON_MEM_SLP_RET_OVERRIDE_GRP1_REG, 0},
	{0, QCA6390_WLAON_MEM_SLP_RET_OVERRIDE_GRP2_REG, 0},
	{0, QCA6390_WLAON_MEM_SLP_RET_OVERRIDE_GRP3_REG, 0},
	{0, QCA6390_WLAON_MEM_CNT_SEL_REG, 0},
	{0, QCA6390_WLAON_MEM_NO_EXTBHS_REG, 0},
	{0, QCA6390_WLAON_MEM_DEBUG_REG, 0},
	{0, QCA6390_WLAON_MEM_DEBUG_BUS_REG, 0},
	{0, QCA6390_WLAON_MEM_REDUN_CFG_REG, 0},
	{0, QCA6390_WLAON_WL_AON_SPARE2, 0},
	{0, QCA6390_WLAON_VSEL_CFG_FOR_WL_RET_DISABLE_REG, 0},
	{0, QCA6390_WLAON_BTFM_WLAN_IPC_STATUS_REG, 0},
	{0, QCA6390_WLAON_MPM_COUNTER_CHICKEN_BITS, 0},
	{0, QCA6390_WLAON_WLPM_CHICKEN_BITS, 0},
	{0, QCA6390_WLAON_PCIE_PHY_PWR_REG, 0},
	{0, QCA6390_WLAON_WL_CLK_CNTL_PMU_LPO2M_REG, 0},
	{0, QCA6390_WLAON_WL_SS_ROOT_CLK_SWITCH_REG, 0},
	{0, QCA6390_WLAON_POWERCTRL_PMU_REG, 0},
	{0, QCA6390_WLAON_POWERCTRL_MEM_REG, 0},
	{0, QCA6390_WLAON_PCIE_PWR_CTRL_REG, 0},
	{0, QCA6390_WLAON_SOC_PWR_PROFILE_REG, 0},
	{0, QCA6390_WLAON_WCSSAON_PCIE_SR_STATUS_HI_REG, 0},
	{0, QCA6390_WLAON_WCSSAON_PCIE_SR_STATUS_LO_REG, 0},
	{0, QCA6390_WLAON_WCSS_TCSR_PMM_SR_STATUS_HI_REG, 0},
	{0, QCA6390_WLAON_WCSS_TCSR_PMM_SR_STATUS_LO_REG, 0},
	{0, QCA6390_WLAON_MEM_SVS_CFG_REG, 0},
	{0, QCA6390_WLAON_CMN_AON_MISC_REG, 0},
	{0, QCA6390_WLAON_INTR_STATUS, 0},
	{0, QCA6390_SYSPM_SYSPM_PWR_STATUS, 0},
	{0, QCA6390_SYSPM_DBG_BTFM_AON_REG, 0},
	{0, QCA6390_SYSPM_DBG_BUS_SEL_REG, 0},
	{0, QCA6390_SYSPM_WCSSAON_SR_STATUS, 0},
	{0, QCA6390_SYSPM_WCSSAON_SR_STATUS, 0},
	{0, QCA6390_SYSPM_WCSSAON_SR_STATUS, 0},
	{0, QCA6390_SYSPM_WCSSAON_SR_STATUS, 0},
	{0, QCA6390_SYSPM_WCSSAON_SR_STATUS, 0},
	{0, QCA6390_SYSPM_WCSSAON_SR_STATUS, 0},
	{0, QCA6390_SYSPM_WCSSAON_SR_STATUS, 0},
	{0, QCA6390_SYSPM_WCSSAON_SR_STATUS, 0},
	{0, QCA6390_SYSPM_WCSSAON_SR_STATUS, 0},
	{0, QCA6390_SYSPM_WCSSAON_SR_STATUS, 0},
};

#define WCSS_REG_SIZE ARRAY_SIZE(wcss_reg_access_seq)
#define PCIE_REG_SIZE ARRAY_SIZE(pcie_reg_access_seq)
#define WLAON_REG_SIZE ARRAY_SIZE(wlaon_reg_access_seq)

#if IS_ENABLED(CONFIG_PCI_MSM)
/**
 * _cnss_pci_get_reg_dump() - Dump PCIe RC registers for debug
 * @pci_priv: driver PCI bus context pointer
 * @buf: destination buffer pointer
 * @len: length of the buffer
 *
 * This function shall call corresponding PCIe root complex driver API
 * to dump PCIe RC registers for debug purpose.
 *
 * Return: 0 for success, negative value for error
 */
static int _cnss_pci_get_reg_dump(struct cnss_pci_data *pci_priv,
				  u8 *buf, u32 len)
{
	return msm_pcie_reg_dump(pci_priv->pci_dev, buf, len);
}
#else
static int _cnss_pci_get_reg_dump(struct cnss_pci_data *pci_priv,
				  u8 *buf, u32 len)
{
	return 0;
}
#endif /* CONFIG_PCI_MSM */

int cnss_pci_check_link_status(struct cnss_pci_data *pci_priv)
{
	u16 device_id;

	if (pci_priv->pci_link_state == PCI_LINK_DOWN) {
		cnss_pr_dbg("%ps: PCIe link is in suspend state\n",
			    (void *)_RET_IP_);
		return -EIO;
	}

	if (pci_priv->pci_link_down_ind) {
		cnss_pr_err("%ps: PCIe link is down\n", (void *)_RET_IP_);
		return -EIO;
	}

	pci_read_config_word(pci_priv->pci_dev, PCI_DEVICE_ID, &device_id);
	if (device_id != pci_priv->device_id)  {
		cnss_fatal_err("%ps: PCI device ID mismatch, link possibly down, current read ID: 0x%x, record ID: 0x%x\n",
			       (void *)_RET_IP_, device_id,
			       pci_priv->device_id);
		return -EIO;
	}

	return 0;
}

static void cnss_pci_select_window(struct cnss_pci_data *pci_priv, u32 offset)
{
	u32 window = (offset >> WINDOW_SHIFT) & WINDOW_VALUE_MASK;

	writel_relaxed(WINDOW_ENABLE_BIT | window,
		       QCA6390_PCIE_REMAP_BAR_CTRL_OFFSET +
		       pci_priv->bar);

	if (window != pci_priv->remap_window) {
		pci_priv->remap_window = window;
		cnss_pr_dbg("Config PCIe remap window register to 0x%x\n",
			    WINDOW_ENABLE_BIT | window);
	}
}

static int cnss_pci_reg_read(struct cnss_pci_data *pci_priv,
			     u32 offset, u32 *val)
{
	int ret;

	if (!in_interrupt() && !irqs_disabled()) {
		ret = cnss_pci_check_link_status(pci_priv);
		if (ret)
			return ret;
	}

	if (pci_priv->pci_dev->device == QCA6174_DEVICE_ID ||
	    offset < MAX_UNWINDOWED_ADDRESS) {
		*val = readl_relaxed(pci_priv->bar + offset);
		return 0;
	}

	spin_lock_bh(&pci_reg_window_lock);
	cnss_pci_select_window(pci_priv, offset);

	*val = readl_relaxed(pci_priv->bar + WINDOW_START +
			     (offset & WINDOW_RANGE_MASK));
	spin_unlock_bh(&pci_reg_window_lock);

	return 0;
}

static int cnss_pci_reg_write(struct cnss_pci_data *pci_priv, u32 offset,
			      u32 val)
{
	int ret;

	if (!in_interrupt() && !irqs_disabled()) {
		ret = cnss_pci_check_link_status(pci_priv);
		if (ret)
			return ret;
	}

	if (pci_priv->pci_dev->device == QCA6174_DEVICE_ID ||
	    offset < MAX_UNWINDOWED_ADDRESS) {
		writel_relaxed(val, pci_priv->bar + offset);
		return 0;
	}

	spin_lock_bh(&pci_reg_window_lock);
	cnss_pci_select_window(pci_priv, offset);

	writel_relaxed(val, pci_priv->bar + WINDOW_START +
		       (offset & WINDOW_RANGE_MASK));
	spin_unlock_bh(&pci_reg_window_lock);

	return 0;
}

static int cnss_pci_force_wake_get(struct cnss_pci_data *pci_priv)
{
	struct device *dev = &pci_priv->pci_dev->dev;
	int ret;

	ret = cnss_pci_force_wake_request_sync(dev,
					       FORCE_WAKE_DELAY_TIMEOUT_US);
	if (ret) {
		if (ret != -EAGAIN)
			cnss_pr_err("Failed to request force wake\n");
		return ret;
	}

	/* If device's M1 state-change event races here, it can be ignored,
	 * as the device is expected to immediately move from M2 to M0
	 * without entering low power state.
	 */
	if (cnss_pci_is_device_awake(dev) != true)
		cnss_pr_warn("MHI not in M0, while reg still accessible\n");

	return 0;
}

static int cnss_pci_force_wake_put(struct cnss_pci_data *pci_priv)
{
	struct device *dev = &pci_priv->pci_dev->dev;
	int ret;

	ret = cnss_pci_force_wake_release(dev);
	if (ret && ret != -EAGAIN)
		cnss_pr_err("Failed to release force wake\n");

	return ret;
}

int cnss_pci_debug_reg_read(struct cnss_pci_data *pci_priv, u32 offset,
			    u32 *val)
{
	int ret = 0;
	bool do_force_wake_put = true;

	ret = cnss_pci_is_device_down(&pci_priv->pci_dev->dev);
	if (ret)
		goto out;

	ret = cnss_pci_pm_runtime_get_sync(pci_priv, RTPM_ID_CNSS);
	if (ret < 0)
		goto runtime_pm_put;

	ret = cnss_pci_force_wake_get(pci_priv);
	if (ret)
		do_force_wake_put = false;

	ret = cnss_pci_reg_read(pci_priv, offset, val);
	if (ret) {
		cnss_pr_err("Failed to read register offset 0x%x, err = %d\n",
			    offset, ret);
		goto force_wake_put;
	}

force_wake_put:
	if (do_force_wake_put)
		cnss_pci_force_wake_put(pci_priv);
runtime_pm_put:
	cnss_pci_pm_runtime_mark_last_busy(pci_priv);
	cnss_pci_pm_runtime_put_autosuspend(pci_priv, RTPM_ID_CNSS);
out:
	return ret;
}

int cnss_pci_debug_reg_write(struct cnss_pci_data *pci_priv, u32 offset,
			     u32 val)
{
	int ret = 0;
	bool do_force_wake_put = true;

	ret = cnss_pci_is_device_down(&pci_priv->pci_dev->dev);
	if (ret)
		goto out;

	ret = cnss_pci_pm_runtime_get_sync(pci_priv, RTPM_ID_CNSS);
	if (ret < 0)
		goto runtime_pm_put;

	ret = cnss_pci_force_wake_get(pci_priv);
	if (ret)
		do_force_wake_put = false;

	ret = cnss_pci_reg_write(pci_priv, offset, val);
	if (ret) {
		cnss_pr_err("Failed to write 0x%x to register offset 0x%x, err = %d\n",
			    val, offset, ret);
		goto force_wake_put;
	}

force_wake_put:
	if (do_force_wake_put)
		cnss_pci_force_wake_put(pci_priv);
runtime_pm_put:
	cnss_pci_pm_runtime_mark_last_busy(pci_priv);
	cnss_pci_pm_runtime_put_autosuspend(pci_priv, RTPM_ID_CNSS);
out:
	return ret;
}

static int cnss_set_pci_config_space(struct cnss_pci_data *pci_priv, bool save)
{
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	bool link_down_or_recovery;

	if (!plat_priv)
		return -ENODEV;

	link_down_or_recovery = pci_priv->pci_link_down_ind ||
		(test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state));

	if (save) {
		if (link_down_or_recovery) {
			pci_priv->saved_state = NULL;
		} else {
			pci_save_state(pci_dev);
			pci_priv->saved_state = pci_store_saved_state(pci_dev);
		}
	} else {
		if (link_down_or_recovery) {
			pci_load_saved_state(pci_dev, pci_priv->default_state);
			pci_restore_state(pci_dev);
		} else if (pci_priv->saved_state) {
			pci_load_and_free_saved_state(pci_dev,
						      &pci_priv->saved_state);
			pci_restore_state(pci_dev);
		}
	}

	return 0;
}

static int cnss_pci_get_link_status(struct cnss_pci_data *pci_priv)
{
	u16 link_status;
	int ret;

	ret = pcie_capability_read_word(pci_priv->pci_dev, PCI_EXP_LNKSTA,
					&link_status);
	if (ret)
		return ret;

	cnss_pr_dbg("Get PCI link status register: %u\n", link_status);

	pci_priv->def_link_speed = link_status & PCI_EXP_LNKSTA_CLS;
	pci_priv->def_link_width =
		(link_status & PCI_EXP_LNKSTA_NLW) >> PCI_EXP_LNKSTA_NLW_SHIFT;

	cnss_pr_dbg("Default PCI link speed is 0x%x, link width is 0x%x\n",
		    pci_priv->def_link_speed, pci_priv->def_link_width);

	return 0;
}

static int cnss_set_pci_link_status(struct cnss_pci_data *pci_priv,
				    enum pci_link_status status)
{
	u16 link_speed, link_width;

	cnss_pr_vdbg("Set PCI link status to: %u\n", status);

	switch (status) {
	case PCI_GEN1:
		link_speed = PCI_EXP_LNKSTA_CLS_2_5GB;
		link_width = PCI_EXP_LNKSTA_NLW_X1 >> PCI_EXP_LNKSTA_NLW_SHIFT;
		break;
	case PCI_GEN2:
		link_speed = PCI_EXP_LNKSTA_CLS_5_0GB;
		link_width = PCI_EXP_LNKSTA_NLW_X1 >> PCI_EXP_LNKSTA_NLW_SHIFT;
		break;
	case PCI_DEF:
		link_speed = pci_priv->def_link_speed;
		link_width = pci_priv->def_link_width;
		if (!link_speed && !link_width) {
			cnss_pr_err("PCI link speed or width is not valid\n");
			return -EINVAL;
		}
		break;
	default:
		cnss_pr_err("Unknown PCI link status config: %u\n", status);
		return -EINVAL;
	}

	return msm_pcie_set_link_bandwidth(pci_priv->pci_dev,
					   link_speed, link_width);
}

static int cnss_set_pci_link(struct cnss_pci_data *pci_priv, bool link_up)
{
	int ret = 0;
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	enum msm_pcie_pm_opt pm_ops;
	int retry = 0;

	cnss_pr_vdbg("%s PCI link\n", link_up ? "Resuming" : "Suspending");

	if (link_up) {
		pm_ops = MSM_PCIE_RESUME;
	} else {
		if (pci_priv->drv_connected_last) {
			cnss_pr_vdbg("Use PCIe DRV suspend\n");
			pm_ops = MSM_PCIE_DRV_SUSPEND;
			if (pci_priv->device_id != QCA6390_DEVICE_ID)
				cnss_set_pci_link_status(pci_priv, PCI_GEN1);
		} else {
			pm_ops = MSM_PCIE_SUSPEND;
		}
	}

retry:
	ret = msm_pcie_pm_control(pm_ops, pci_dev->bus->number, pci_dev,
				  NULL, PM_OPTIONS_DEFAULT);
	if (ret) {
		cnss_pr_err("Failed to %s PCI link with default option, err = %d\n",
			    link_up ? "resume" : "suspend", ret);
		if (link_up && retry++ < LINK_TRAINING_RETRY_MAX_TIMES) {
			cnss_pr_dbg("Retry PCI link training #%d\n", retry);
			if (pci_priv->pci_link_down_ind)
				msleep(LINK_TRAINING_RETRY_DELAY_MS * retry);
			goto retry;
		}
	}

	if (pci_priv->drv_connected_last) {
		if ((link_up && !ret) || (!link_up && ret))
			cnss_set_pci_link_status(pci_priv, PCI_DEF);
	}

	return ret;
}

int cnss_suspend_pci_link(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv)
		return -ENODEV;

	if (pci_priv->pci_link_state == PCI_LINK_DOWN) {
		cnss_pr_info("PCI link is already suspended\n");
		goto out;
	}

	pci_clear_master(pci_priv->pci_dev);

	ret = cnss_set_pci_config_space(pci_priv, SAVE_PCI_CONFIG_SPACE);
	if (ret)
		goto out;

	pci_disable_device(pci_priv->pci_dev);

	if (pci_priv->pci_dev->device != QCA6174_DEVICE_ID) {
		if (pci_set_power_state(pci_priv->pci_dev, PCI_D3hot))
			cnss_pr_err("Failed to set D3Hot, err =  %d\n", ret);
	}

	ret = cnss_set_pci_link(pci_priv, PCI_LINK_DOWN);
	if (ret)
		goto out;

	pci_priv->pci_link_state = PCI_LINK_DOWN;

	return 0;
out:
	return ret;
}

int cnss_resume_pci_link(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv)
		return -ENODEV;

	if (pci_priv->pci_link_state == PCI_LINK_UP) {
		cnss_pr_info("PCI link is already resumed\n");
		goto out;
	}

	ret = cnss_set_pci_link(pci_priv, PCI_LINK_UP);
	if (ret) {
		ret = -EAGAIN;
		goto out;
	}

	pci_priv->pci_link_state = PCI_LINK_UP;

	if (pci_priv->pci_dev->device != QCA6174_DEVICE_ID) {
		ret = pci_set_power_state(pci_priv->pci_dev, PCI_D0);
		if (ret) {
			cnss_pr_err("Failed to set D0, err = %d\n", ret);
			goto out;
		}
	}

	ret = pci_enable_device(pci_priv->pci_dev);
	if (ret) {
		cnss_pr_err("Failed to enable PCI device, err = %d\n", ret);
		goto out;
	}

	ret = cnss_set_pci_config_space(pci_priv, RESTORE_PCI_CONFIG_SPACE);
	if (ret)
		goto out;

	pci_set_master(pci_priv->pci_dev);

	if (pci_priv->pci_link_down_ind)
		pci_priv->pci_link_down_ind = false;

	return 0;
out:
	return ret;
}

int cnss_pci_recover_link_down(struct cnss_pci_data *pci_priv)
{
	int ret;

	switch (pci_priv->device_id) {
	case QCA6390_DEVICE_ID:
	case QCA6490_DEVICE_ID:
		break;
	default:
		return -EOPNOTSUPP;
	}

	/* Always wait here to avoid missing WAKE assert for RDDM
	 * before link recovery
	 */
	msleep(WAKE_EVENT_TIMEOUT);

	ret = cnss_suspend_pci_link(pci_priv);
	if (ret)
		cnss_pr_err("Failed to suspend PCI link, err = %d\n", ret);

	ret = cnss_resume_pci_link(pci_priv);
	if (ret) {
		cnss_pr_err("Failed to resume PCI link, err = %d\n", ret);
		del_timer(&pci_priv->dev_rddm_timer);
		return ret;
	}

	mod_timer(&pci_priv->dev_rddm_timer,
		  jiffies + msecs_to_jiffies(DEV_RDDM_TIMEOUT));

	mhi_debug_reg_dump(pci_priv->mhi_ctrl);

	return 0;
}

int cnss_pci_prevent_l1(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	if (pci_priv->pci_link_state == PCI_LINK_DOWN) {
		cnss_pr_dbg("PCIe link is in suspend state\n");
		return -EIO;
	}

	if (pci_priv->pci_link_down_ind) {
		cnss_pr_err("PCIe link is down\n");
		return -EIO;
	}

	return msm_pcie_prevent_l1(pci_dev);
}
EXPORT_SYMBOL(cnss_pci_prevent_l1);

void cnss_pci_allow_l1(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return;
	}

	if (pci_priv->pci_link_state == PCI_LINK_DOWN) {
		cnss_pr_dbg("PCIe link is in suspend state\n");
		return;
	}

	if (pci_priv->pci_link_down_ind) {
		cnss_pr_err("PCIe link is down\n");
		return;
	}

	msm_pcie_allow_l1(pci_dev);
}
EXPORT_SYMBOL(cnss_pci_allow_l1);

static void cnss_pci_update_link_event(struct cnss_pci_data *pci_priv,
				       enum cnss_bus_event_type type,
				       void *data)
{
	struct cnss_bus_event bus_event;

	bus_event.etype = type;
	bus_event.event_data = data;
	cnss_pci_call_driver_uevent(pci_priv, CNSS_BUS_EVENT, &bus_event);
}

static void cnss_pci_handle_linkdown(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	unsigned long flags;

	if (test_bit(ENABLE_PCI_LINK_DOWN_PANIC,
		     &plat_priv->ctrl_params.quirks))
		panic("cnss: PCI link is down\n");

	spin_lock_irqsave(&pci_link_down_lock, flags);
	if (pci_priv->pci_link_down_ind) {
		cnss_pr_dbg("PCI link down recovery is in progress, ignore\n");
		spin_unlock_irqrestore(&pci_link_down_lock, flags);
		return;
	}
	pci_priv->pci_link_down_ind = true;
	spin_unlock_irqrestore(&pci_link_down_lock, flags);

	reinit_completion(&pci_priv->wake_event);

	if (pci_dev->device == QCA6174_DEVICE_ID)
		disable_irq(pci_dev->irq);

	/* Notify bus related event. Now for all supported chips.
	 * Here PCIe LINK_DOWN notification taken care.
	 * uevent buffer can be extended later, to cover more bus info.
	 */
	cnss_pci_update_link_event(pci_priv, BUS_EVENT_PCI_LINK_DOWN, NULL);

	cnss_fatal_err("PCI link down, schedule recovery\n");
	cnss_schedule_recovery(&pci_dev->dev, CNSS_REASON_LINK_DOWN);
}

int cnss_pci_link_down(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv = NULL;
	int ret;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -EINVAL;
	}

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	if (pci_priv->drv_connected_last &&
	    of_property_read_bool(plat_priv->plat_dev->dev.of_node,
				  "cnss-enable-self-recovery"))
		plat_priv->ctrl_params.quirks |= BIT(LINK_DOWN_SELF_RECOVERY);

	cnss_pr_err("PCI link down is detected by drivers\n");

	ret = msm_pcie_pm_control(MSM_PCIE_HANDLE_LINKDOWN,
				  pci_dev->bus->number, pci_dev, NULL,
				  PM_OPTIONS_DEFAULT);
	if (ret)
		cnss_pci_handle_linkdown(pci_priv);

	return ret;
}
EXPORT_SYMBOL(cnss_pci_link_down);

int cnss_pci_get_reg_dump(struct device *dev, uint8_t *buffer, uint32_t len)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	if (pci_priv->pci_link_state == PCI_LINK_DOWN) {
		cnss_pr_dbg("No PCIe reg dump since PCIe device is suspended(D3)\n");
		return -EACCES;
	}

	cnss_pr_dbg("Start to get PCIe reg dump\n");

	return _cnss_pci_get_reg_dump(pci_priv, buffer, len);
}
EXPORT_SYMBOL(cnss_pci_get_reg_dump);

int cnss_pcie_is_device_down(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	return test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state) |
		pci_priv->pci_link_down_ind;
}

int cnss_pci_is_device_down(struct device *dev)
{
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(to_pci_dev(dev));

	return cnss_pcie_is_device_down(pci_priv);
}
EXPORT_SYMBOL(cnss_pci_is_device_down);

void cnss_pci_lock_reg_window(struct device *dev, unsigned long *flags)
{
	spin_lock_bh(&pci_reg_window_lock);
}
EXPORT_SYMBOL(cnss_pci_lock_reg_window);

void cnss_pci_unlock_reg_window(struct device *dev, unsigned long *flags)
{
	spin_unlock_bh(&pci_reg_window_lock);
}
EXPORT_SYMBOL(cnss_pci_unlock_reg_window);

static char *cnss_mhi_state_to_str(enum cnss_mhi_state mhi_state)
{
	switch (mhi_state) {
	case CNSS_MHI_INIT:
		return "INIT";
	case CNSS_MHI_DEINIT:
		return "DEINIT";
	case CNSS_MHI_POWER_ON:
		return "POWER_ON";
	case CNSS_MHI_POWERING_OFF:
		return "POWERING_OFF";
	case CNSS_MHI_POWER_OFF:
		return "POWER_OFF";
	case CNSS_MHI_FORCE_POWER_OFF:
		return "FORCE_POWER_OFF";
	case CNSS_MHI_SUSPEND:
		return "SUSPEND";
	case CNSS_MHI_RESUME:
		return "RESUME";
	case CNSS_MHI_TRIGGER_RDDM:
		return "TRIGGER_RDDM";
	case CNSS_MHI_RDDM_DONE:
		return "RDDM_DONE";
	default:
		return "UNKNOWN";
	}
};

static int cnss_pci_check_mhi_state_bit(struct cnss_pci_data *pci_priv,
					enum cnss_mhi_state mhi_state)
{
	switch (mhi_state) {
	case CNSS_MHI_INIT:
		if (!test_bit(CNSS_MHI_INIT, &pci_priv->mhi_state))
			return 0;
		break;
	case CNSS_MHI_DEINIT:
	case CNSS_MHI_POWER_ON:
		if (test_bit(CNSS_MHI_INIT, &pci_priv->mhi_state) &&
		    !test_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state))
			return 0;
		break;
	case CNSS_MHI_FORCE_POWER_OFF:
		if (test_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state))
			return 0;
		break;
	case CNSS_MHI_POWER_OFF:
	case CNSS_MHI_SUSPEND:
		if (test_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state) &&
		    !test_bit(CNSS_MHI_SUSPEND, &pci_priv->mhi_state))
			return 0;
		break;
	case CNSS_MHI_RESUME:
		if (test_bit(CNSS_MHI_SUSPEND, &pci_priv->mhi_state))
			return 0;
		break;
	case CNSS_MHI_TRIGGER_RDDM:
		if (test_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state) &&
		    !test_bit(CNSS_MHI_TRIGGER_RDDM, &pci_priv->mhi_state))
			return 0;
		break;
	case CNSS_MHI_RDDM_DONE:
		return 0;
	default:
		cnss_pr_err("Unhandled MHI state: %s(%d)\n",
			    cnss_mhi_state_to_str(mhi_state), mhi_state);
	}

	cnss_pr_err("Cannot set MHI state %s(%d) in current MHI state (0x%lx)\n",
		    cnss_mhi_state_to_str(mhi_state), mhi_state,
		    pci_priv->mhi_state);
	if (mhi_state != CNSS_MHI_TRIGGER_RDDM)
		CNSS_ASSERT(0);

	return -EINVAL;
}

static void cnss_pci_set_mhi_state_bit(struct cnss_pci_data *pci_priv,
				       enum cnss_mhi_state mhi_state)
{
	switch (mhi_state) {
	case CNSS_MHI_INIT:
		set_bit(CNSS_MHI_INIT, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_DEINIT:
		clear_bit(CNSS_MHI_INIT, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_POWER_ON:
		set_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_POWERING_OFF:
		set_bit(CNSS_MHI_POWERING_OFF, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_POWER_OFF:
	case CNSS_MHI_FORCE_POWER_OFF:
		clear_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state);
		clear_bit(CNSS_MHI_POWERING_OFF, &pci_priv->mhi_state);
		clear_bit(CNSS_MHI_TRIGGER_RDDM, &pci_priv->mhi_state);
		clear_bit(CNSS_MHI_RDDM_DONE, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_SUSPEND:
		set_bit(CNSS_MHI_SUSPEND, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_RESUME:
		clear_bit(CNSS_MHI_SUSPEND, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_TRIGGER_RDDM:
		set_bit(CNSS_MHI_TRIGGER_RDDM, &pci_priv->mhi_state);
		break;
	case CNSS_MHI_RDDM_DONE:
		set_bit(CNSS_MHI_RDDM_DONE, &pci_priv->mhi_state);
		break;
	default:
		cnss_pr_err("Unhandled MHI state (%d)\n", mhi_state);
	}
}

static int cnss_pci_set_mhi_state(struct cnss_pci_data *pci_priv,
				  enum cnss_mhi_state mhi_state)
{
	int ret = 0;
	u8 retry = 0;

	if (pci_priv->device_id == QCA6174_DEVICE_ID)
		return 0;

	if (mhi_state < 0) {
		cnss_pr_err("Invalid MHI state (%d)\n", mhi_state);
		return -EINVAL;
	}

	ret = cnss_pci_check_mhi_state_bit(pci_priv, mhi_state);
	if (ret)
		goto out;

	cnss_pr_vdbg("Setting MHI state: %s(%d)\n",
		     cnss_mhi_state_to_str(mhi_state), mhi_state);

	switch (mhi_state) {
	case CNSS_MHI_INIT:
		ret = mhi_prepare_for_power_up(pci_priv->mhi_ctrl);
		break;
	case CNSS_MHI_DEINIT:
		mhi_unprepare_after_power_down(pci_priv->mhi_ctrl);
		ret = 0;
		break;
	case CNSS_MHI_POWER_ON:
		ret = mhi_sync_power_up(pci_priv->mhi_ctrl);
		/* Only set img_pre_alloc when power up succeeds */
		if (!ret && !pci_priv->mhi_ctrl->img_pre_alloc) {
			cnss_pr_dbg("Notify MHI to use already allocated images\n");
			pci_priv->mhi_ctrl->img_pre_alloc = true;
		}
		/* -ETIMEDOUT means MHI power up has succeeded but timed out
		 * for firmware mission mode event, so handle it properly.
		 */
		if (ret == -ETIMEDOUT)
			ret = 0;
		break;
	case CNSS_MHI_POWER_OFF:
		mhi_power_down(pci_priv->mhi_ctrl, true);
		ret = 0;
		break;
	case CNSS_MHI_FORCE_POWER_OFF:
		mhi_power_down(pci_priv->mhi_ctrl, false);
		ret = 0;
		break;
	case CNSS_MHI_SUSPEND:
		mutex_lock(&pci_priv->mhi_ctrl->pm_mutex);
		if (pci_priv->drv_connected_last) {
			ret = mhi_pm_fast_suspend(pci_priv->mhi_ctrl, true);
		} else {
			ret = mhi_pm_suspend(pci_priv->mhi_ctrl);
			/* in some corner case, when cnss try to suspend,
			 * there is still packets pending in mhi layer,
			 * so retry suspend to save roll back effort.
			 */
			while (ret == -EBUSY && retry < MHI_SUSPEND_RETRY_CNT) {
				usleep_range(5000, 6000);
				retry++;
				cnss_pr_err("mhi is busy, retry #%u", retry);
				ret = mhi_pm_suspend(pci_priv->mhi_ctrl);
			}
		}
		mutex_unlock(&pci_priv->mhi_ctrl->pm_mutex);
		break;
	case CNSS_MHI_RESUME:
		mutex_lock(&pci_priv->mhi_ctrl->pm_mutex);
		if (pci_priv->drv_connected_last)
			ret = mhi_pm_fast_resume(pci_priv->mhi_ctrl, true);
		else
			ret = mhi_pm_resume(pci_priv->mhi_ctrl);
		mutex_unlock(&pci_priv->mhi_ctrl->pm_mutex);
		break;
	case CNSS_MHI_TRIGGER_RDDM:
		ret = mhi_force_rddm_mode(pci_priv->mhi_ctrl);
		break;
	case CNSS_MHI_RDDM_DONE:
		break;
	default:
		cnss_pr_err("Unhandled MHI state (%d)\n", mhi_state);
		ret = -EINVAL;
	}

	if (ret)
		goto out;

	cnss_pci_set_mhi_state_bit(pci_priv, mhi_state);

	return 0;

out:
	cnss_pr_err("Failed to set MHI state: %s(%d) ret: %d\n",
		    cnss_mhi_state_to_str(mhi_state), mhi_state, ret);

	if (mhi_state == CNSS_MHI_RESUME && ret != -ETIMEDOUT)
		cnss_force_fw_assert_async(&pci_priv->pci_dev->dev);

	return ret;
}

int cnss_pci_start_mhi(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	plat_priv = pci_priv->plat_priv;
	if (test_bit(FBC_BYPASS, &plat_priv->ctrl_params.quirks))
		return 0;

	if (MHI_TIMEOUT_OVERWRITE_MS)
		pci_priv->mhi_ctrl->timeout_ms = MHI_TIMEOUT_OVERWRITE_MS;
	pci_priv->mhi_ctrl->m2_timeout_ms = MHI_M2_TIMEOUT_MS;

	ret = cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_INIT);
	if (ret)
		goto out;

	ret = cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_POWER_ON);
	if (ret)
		goto out;

	return 0;

out:
	return ret;
}

static void cnss_pci_power_off_mhi(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	if (test_bit(FBC_BYPASS, &plat_priv->ctrl_params.quirks))
		return;

	cnss_pci_set_mhi_state_bit(pci_priv, CNSS_MHI_RESUME);
	cnss_pci_set_mhi_state_bit(pci_priv, CNSS_MHI_POWERING_OFF);

	if (!pci_priv->pci_link_down_ind)
		cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_POWER_OFF);
	else
		cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_FORCE_POWER_OFF);
}

static void cnss_pci_deinit_mhi(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	if (test_bit(FBC_BYPASS, &plat_priv->ctrl_params.quirks))
		return;

	cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_DEINIT);
}

static void cnss_pci_set_wlaon_pwr_ctrl(struct cnss_pci_data *pci_priv,
					bool set_vddd4blow, bool set_shutdown,
					bool do_force_wake)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	int ret;
	u32 val;

	if (!plat_priv->set_wlaon_pwr_ctrl)
		return;

	if (pci_priv->pci_link_state == PCI_LINK_DOWN ||
	    pci_priv->pci_link_down_ind)
		return;

	if (do_force_wake)
		if (cnss_pci_force_wake_get(pci_priv))
			return;

	ret = cnss_pci_reg_read(pci_priv, QCA6390_WLAON_QFPROM_PWR_CTRL_REG,
				&val);
	if (ret) {
		cnss_pr_err("Failed to read register offset 0x%x, err = %d\n",
			    QCA6390_WLAON_QFPROM_PWR_CTRL_REG, ret);
		goto force_wake_put;
	}

	cnss_pr_dbg("Read register offset 0x%x, val = 0x%x\n",
		    QCA6390_WLAON_QFPROM_PWR_CTRL_REG, val);

	if (set_vddd4blow)
		val |= QFPROM_PWR_CTRL_VDD4BLOW_SW_EN_MASK;
	else
		val &= ~QFPROM_PWR_CTRL_VDD4BLOW_SW_EN_MASK;

	if (set_shutdown)
		val |= QFPROM_PWR_CTRL_SHUTDOWN_EN_MASK;
	else
		val &= ~QFPROM_PWR_CTRL_SHUTDOWN_EN_MASK;

	ret = cnss_pci_reg_write(pci_priv, QCA6390_WLAON_QFPROM_PWR_CTRL_REG,
				 val);
	if (ret) {
		cnss_pr_err("Failed to write register offset 0x%x, err = %d\n",
			    QCA6390_WLAON_QFPROM_PWR_CTRL_REG, ret);
		goto force_wake_put;
	}

	cnss_pr_dbg("Write val 0x%x to register offset 0x%x\n", val,
		    QCA6390_WLAON_QFPROM_PWR_CTRL_REG);

	if (set_shutdown)
		usleep_range(WLAON_PWR_CTRL_SHUTDOWN_DELAY_MIN_US,
			     WLAON_PWR_CTRL_SHUTDOWN_DELAY_MAX_US);

force_wake_put:
	if (do_force_wake)
		cnss_pci_force_wake_put(pci_priv);
}

static int cnss_pci_get_device_timestamp(struct cnss_pci_data *pci_priv,
					 u64 *time_us)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	u32 low, high;
	u64 device_ticks;

	if (!plat_priv->device_freq_hz) {
		cnss_pr_err("Device time clock frequency is not valid\n");
		return -EINVAL;
	}

	cnss_pci_reg_read(pci_priv, QCA6390_WLAON_GLOBAL_COUNTER_CTRL3, &low);
	cnss_pci_reg_read(pci_priv, QCA6390_WLAON_GLOBAL_COUNTER_CTRL4, &high);

	device_ticks = (u64)high << 32 | low;
	do_div(device_ticks, plat_priv->device_freq_hz / 100000);
	*time_us = device_ticks * 10;

	return 0;
}

static void cnss_pci_enable_time_sync_counter(struct cnss_pci_data *pci_priv)
{
	cnss_pci_reg_write(pci_priv, QCA6390_WLAON_GLOBAL_COUNTER_CTRL5,
			   QCA6390_TIME_SYNC_ENABLE);
}

static void cnss_pci_clear_time_sync_counter(struct cnss_pci_data *pci_priv)
{
	cnss_pci_reg_write(pci_priv, QCA6390_WLAON_GLOBAL_COUNTER_CTRL5,
			   QCA6390_TIME_SYNC_CLEAR);
}

static int cnss_pci_update_timestamp(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct device *dev = &pci_priv->pci_dev->dev;
	unsigned long flags = 0;
	u64 host_time_us, device_time_us, offset;
	u32 low, high;
	int ret;

	ret = cnss_pci_prevent_l1(dev);
	if (ret)
		goto out;

	ret = cnss_pci_force_wake_get(pci_priv);
	if (ret)
		goto allow_l1;

	spin_lock_irqsave(&time_sync_lock, flags);
	cnss_pci_clear_time_sync_counter(pci_priv);
	cnss_pci_enable_time_sync_counter(pci_priv);
	host_time_us = cnss_get_host_timestamp(plat_priv);
	ret = cnss_pci_get_device_timestamp(pci_priv, &device_time_us);
	cnss_pci_clear_time_sync_counter(pci_priv);
	spin_unlock_irqrestore(&time_sync_lock, flags);
	if (ret)
		goto force_wake_put;

	if (host_time_us < device_time_us) {
		cnss_pr_err("Host time (%llu us) is smaller than device time (%llu us), stop\n",
			    host_time_us, device_time_us);
		ret = -EINVAL;
		goto force_wake_put;
	}

	offset = host_time_us - device_time_us;
	cnss_pr_dbg("Host time = %llu us, device time = %llu us, offset = %llu us\n",
		    host_time_us, device_time_us, offset);

	low = offset & 0xFFFFFFFF;
	high = offset >> 32;

	cnss_pci_reg_write(pci_priv, QCA6390_PCIE_SHADOW_REG_VALUE_34, low);
	cnss_pci_reg_write(pci_priv, QCA6390_PCIE_SHADOW_REG_VALUE_35, high);

	cnss_pci_reg_read(pci_priv, QCA6390_PCIE_SHADOW_REG_VALUE_34, &low);
	cnss_pci_reg_read(pci_priv, QCA6390_PCIE_SHADOW_REG_VALUE_35, &high);

	cnss_pr_dbg("Updated time sync regs [0x%x] = 0x%x, [0x%x] = 0x%x\n",
		    QCA6390_PCIE_SHADOW_REG_VALUE_34, low,
		    QCA6390_PCIE_SHADOW_REG_VALUE_35, high);

force_wake_put:
	cnss_pci_force_wake_put(pci_priv);
allow_l1:
	cnss_pci_allow_l1(dev);
out:
	return ret;
}

static void cnss_pci_time_sync_work_hdlr(struct work_struct *work)
{
	struct cnss_pci_data *pci_priv =
		container_of(work, struct cnss_pci_data, time_sync_work.work);
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	unsigned int time_sync_period_ms =
		plat_priv->ctrl_params.time_sync_period;

	if (cnss_pci_is_device_down(&pci_priv->pci_dev->dev))
		return;

	if (cnss_pci_pm_runtime_get_sync(pci_priv, RTPM_ID_CNSS) < 0)
		goto runtime_pm_put;

	cnss_pci_update_timestamp(pci_priv);
	schedule_delayed_work(&pci_priv->time_sync_work,
			      msecs_to_jiffies(time_sync_period_ms));

runtime_pm_put:
	cnss_pci_pm_runtime_mark_last_busy(pci_priv);
	cnss_pci_pm_runtime_put_autosuspend(pci_priv, RTPM_ID_CNSS);
}

static int cnss_pci_start_time_sync_update(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	switch (pci_priv->device_id) {
	case QCA6390_DEVICE_ID:
	case QCA6490_DEVICE_ID:
		break;
	default:
		return -EOPNOTSUPP;
	}

	if (!plat_priv->device_freq_hz) {
		cnss_pr_dbg("Device time clock frequency is not valid, skip time sync\n");
		return -EINVAL;
	}

	cnss_pci_time_sync_work_hdlr(&pci_priv->time_sync_work.work);

	return 0;
}

static void cnss_pci_stop_time_sync_update(struct cnss_pci_data *pci_priv)
{
	switch (pci_priv->device_id) {
	case QCA6390_DEVICE_ID:
	case QCA6490_DEVICE_ID:
		break;
	default:
		return;
	}

	cancel_delayed_work_sync(&pci_priv->time_sync_work);
}

int cnss_pci_update_qtime_sync_period(struct device *dev,
				      unsigned int qtime_sync_period)
{
	struct cnss_plat_data *plat_priv = dev_get_drvdata(dev);
	struct cnss_pci_data *pci_priv = plat_priv->bus_priv;

	if (!plat_priv || !pci_priv)
		return -ENODEV;

	cnss_pci_stop_time_sync_update(pci_priv);
	plat_priv->ctrl_params.time_sync_period = qtime_sync_period;
	cnss_pci_start_time_sync_update(pci_priv);
	cnss_pr_dbg("WLAN qtime sync period %u\n",
		    plat_priv->ctrl_params.time_sync_period);

	return 0;
}

int cnss_pci_call_driver_probe(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv;

	if (!pci_priv)
		return -ENODEV;

	plat_priv = pci_priv->plat_priv;

	if (test_bit(CNSS_DRIVER_DEBUG, &plat_priv->driver_state)) {
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		cnss_pr_dbg("Skip driver probe\n");
		goto out;
	}

	if (!pci_priv->driver_ops) {
		cnss_pr_err("driver_ops is NULL\n");
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state) &&
	    test_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state)) {
		ret = pci_priv->driver_ops->reinit(pci_priv->pci_dev,
						   pci_priv->pci_device_id);
		if (ret) {
			cnss_pr_err("Failed to reinit host driver, err = %d\n",
				    ret);
			goto out;
		}
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		complete(&plat_priv->recovery_complete);
	} else if (test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state)) {
		ret = pci_priv->driver_ops->probe(pci_priv->pci_dev,
						  pci_priv->pci_device_id);
		if (ret) {
			cnss_pr_err("Failed to probe host driver, err = %d\n",
				    ret);
			goto out;
		}
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		clear_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
		set_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state);
		complete_all(&plat_priv->power_up_complete);
	} else if (test_bit(CNSS_DRIVER_IDLE_RESTART,
			    &plat_priv->driver_state)) {
		ret = pci_priv->driver_ops->idle_restart(pci_priv->pci_dev,
			pci_priv->pci_device_id);
		if (ret) {
			cnss_pr_err("Failed to idle restart host driver, err = %d\n",
				    ret);
			plat_priv->power_up_error = ret;
			complete_all(&plat_priv->power_up_complete);
			goto out;
		}
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		clear_bit(CNSS_DRIVER_IDLE_RESTART, &plat_priv->driver_state);
		complete_all(&plat_priv->power_up_complete);
	} else {
		complete(&plat_priv->power_up_complete);
	}

	cnss_pci_start_time_sync_update(pci_priv);

	return 0;

out:
	return ret;
}

int cnss_pci_call_driver_remove(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv;
	int ret;

	if (!pci_priv)
		return -ENODEV;

	plat_priv = pci_priv->plat_priv;

	cnss_pci_stop_time_sync_update(pci_priv);

	if (test_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state) ||
	    test_bit(CNSS_FW_BOOT_RECOVERY, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_DEBUG, &plat_priv->driver_state)) {
		cnss_pr_dbg("Skip driver remove\n");
		return 0;
	}

	if (!pci_priv->driver_ops) {
		cnss_pr_err("driver_ops is NULL\n");
		return -EINVAL;
	}

	if (test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state) &&
	    test_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state)) {
		pci_priv->driver_ops->shutdown(pci_priv->pci_dev);
	} else if (test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state)) {
		pci_priv->driver_ops->remove(pci_priv->pci_dev);
		clear_bit(CNSS_DRIVER_PROBED, &plat_priv->driver_state);
	} else if (test_bit(CNSS_DRIVER_IDLE_SHUTDOWN,
			    &plat_priv->driver_state)) {
		ret = pci_priv->driver_ops->idle_shutdown(pci_priv->pci_dev);
		if (ret == -EAGAIN) {
			clear_bit(CNSS_DRIVER_IDLE_SHUTDOWN,
				  &plat_priv->driver_state);
			return ret;
		}
	}

	plat_priv->get_info_cb_ctx = NULL;
	plat_priv->get_info_cb = NULL;

	return 0;
}

int cnss_pci_call_driver_modem_status(struct cnss_pci_data *pci_priv,
				      int modem_current_status)
{
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		return -ENODEV;

	driver_ops = pci_priv->driver_ops;
	if (!driver_ops || !driver_ops->modem_status)
		return -EINVAL;

	driver_ops->modem_status(pci_priv->pci_dev, modem_current_status);

	return 0;
}

int cnss_pci_update_status(struct cnss_pci_data *pci_priv,
			   enum cnss_driver_status status)
{
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		return -ENODEV;

	driver_ops = pci_priv->driver_ops;
	if (!driver_ops || !driver_ops->update_status)
		return -EINVAL;

	cnss_pr_dbg("Update driver status: %d\n", status);

	driver_ops->update_status(pci_priv->pci_dev, status);

	return 0;
}

static void cnss_pci_misc_reg_dump(struct cnss_pci_data *pci_priv,
				   struct cnss_misc_reg *misc_reg,
				   u32 misc_reg_size,
				   char *reg_name)
{
	int i;

	if (!misc_reg)
		return;

	if (in_interrupt() || irqs_disabled())
		return;

	if (cnss_pci_check_link_status(pci_priv))
		return;

	if (cnss_pci_force_wake_get(pci_priv))
		return;

	cnss_pr_dbg("start to dump %s registers\n", reg_name);

	for (i = 0; i < misc_reg_size; i++) {
		if (misc_reg[i].wr) {
			if (misc_reg[i].offset ==
			    QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_SAW2_CFG &&
			    i >= 1)
				misc_reg[i].val =
				QCA6390_WCSS_Q6SS_PRIVCSR_QDSP6SS_SAW2_CFG_MSK |
				misc_reg[i - 1].val;
			if (cnss_pci_reg_write(pci_priv,
					       misc_reg[i].offset,
					       misc_reg[i].val))
				goto force_wake_put;
			cnss_pr_vdbg("Write 0x%X to 0x%X\n",
				     misc_reg[i].val,
				     misc_reg[i].offset);

		} else {
			if (cnss_pci_reg_read(pci_priv,
					      misc_reg[i].offset,
					      &misc_reg[i].val))
				goto force_wake_put;
			cnss_pr_vdbg("Read 0x%X from 0x%X\n",
				     misc_reg[i].val,
				     misc_reg[i].offset);
		}
	}

force_wake_put:
	cnss_pci_force_wake_put(pci_priv);
}

static void cnss_pci_dump_misc_reg(struct cnss_pci_data *pci_priv)
{
	if (in_interrupt() || irqs_disabled())
		return;

	if (cnss_pci_check_link_status(pci_priv))
		return;

	mhi_debug_reg_dump(pci_priv->mhi_ctrl);
	cnss_pci_misc_reg_dump(pci_priv, pci_priv->wcss_reg,
			       pci_priv->wcss_reg_size, "wcss");
	cnss_pci_misc_reg_dump(pci_priv, pci_priv->pcie_reg,
			       pci_priv->pcie_reg_size, "pcie");
	cnss_pci_misc_reg_dump(pci_priv, pci_priv->wlaon_reg,
			       pci_priv->wlaon_reg_size, "wlaon");
}

static void cnss_pci_dump_mhi_reg(struct cnss_pci_data *pci_priv)
{
	if (in_interrupt() || irqs_disabled())
		return;

	if (cnss_pci_check_link_status(pci_priv))
		return;

	mhi_debug_reg_dump(pci_priv->mhi_ctrl);
}

static void cnss_pci_dump_shadow_reg(struct cnss_pci_data *pci_priv)
{
	int i, j = 0, array_size = SHADOW_REG_COUNT + SHADOW_REG_INTER_COUNT;
	u32 reg_offset;
	bool do_force_wake_put = true;

	if (in_interrupt() || irqs_disabled())
		return;

	if (cnss_pci_check_link_status(pci_priv))
		return;

	if (!pci_priv->debug_reg) {
		pci_priv->debug_reg = devm_kzalloc(&pci_priv->pci_dev->dev,
						   sizeof(*pci_priv->debug_reg)
						   * array_size, GFP_KERNEL);
		if (!pci_priv->debug_reg)
			return;
	}

	if (cnss_pci_force_wake_get(pci_priv))
		do_force_wake_put = false;

	cnss_pr_dbg("Start to dump shadow registers\n");

	for (i = 0; i < SHADOW_REG_COUNT; i++, j++) {
		reg_offset = QCA6390_PCIE_SHADOW_REG_VALUE_0 + i * 4;
		pci_priv->debug_reg[j].offset = reg_offset;
		if (cnss_pci_reg_read(pci_priv, reg_offset,
				      &pci_priv->debug_reg[j].val))
			goto force_wake_put;
	}

	for (i = 0; i < SHADOW_REG_INTER_COUNT; i++, j++) {
		reg_offset = QCA6390_PCIE_SHADOW_REG_INTER_0 + i * 4;
		pci_priv->debug_reg[j].offset = reg_offset;
		if (cnss_pci_reg_read(pci_priv, reg_offset,
				      &pci_priv->debug_reg[j].val))
			goto force_wake_put;
	}

force_wake_put:
	if (do_force_wake_put)
		cnss_pci_force_wake_put(pci_priv);
}

#ifdef CONFIG_CNSS2_DEBUG
static void cnss_pci_collect_dump(struct cnss_pci_data *pci_priv)
{
	cnss_pci_collect_dump_info(pci_priv, false);
	CNSS_ASSERT(0);
}
#else
static void cnss_pci_collect_dump(struct cnss_pci_data *pci_priv)
{
}
#endif

/**
 * cnss_pci_dump_qca6390_sram_mem - Dump WLAN FW bootloader debug log
 * @pci_priv: PCI device private data structure of cnss platform driver
 *
 * Dump Primary and secondary bootloader debug log data. For SBL check the
 * log struct address and size for validity.
 *
 * Supported only on QCA6390
 *
 * Return: None
 */
static void cnss_pci_dump_qca6390_sram_mem(struct cnss_pci_data *pci_priv)
{
	int i;
	u32 mem_addr, val, pbl_stage, sbl_log_start, sbl_log_size;
	u32 pbl_wlan_boot_cfg, pbl_bootstrap_status;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	if (plat_priv->device_id != QCA6390_DEVICE_ID)
		return;

	if (cnss_pci_check_link_status(pci_priv))
		return;

	cnss_pci_reg_read(pci_priv, QCA6390_TCSR_PBL_LOGGING_REG, &pbl_stage);
	cnss_pci_reg_read(pci_priv, QCA6390_PCIE_BHI_ERRDBG2_REG,
			  &sbl_log_start);
	cnss_pci_reg_read(pci_priv, QCA6390_PCIE_BHI_ERRDBG3_REG,
			  &sbl_log_size);
	cnss_pci_reg_read(pci_priv, QCA6390_PBL_WLAN_BOOT_CFG,
			  &pbl_wlan_boot_cfg);
	cnss_pci_reg_read(pci_priv, QCA6390_PBL_BOOTSTRAP_STATUS,
			  &pbl_bootstrap_status);
	cnss_pr_dbg("TCSR_PBL_LOGGING: 0x%08x PCIE_BHI_ERRDBG: Start: 0x%08x Size:0x%08x\n",
		    pbl_stage, sbl_log_start, sbl_log_size);
	cnss_pr_dbg("PBL_WLAN_BOOT_CFG: 0x%08x PBL_BOOTSTRAP_STATUS: 0x%08x\n",
		    pbl_wlan_boot_cfg, pbl_bootstrap_status);

	cnss_pr_dbg("Dumping PBL log data\n");
	/* cnss_pci_reg_read provides 32bit register values */
	for (i = 0; i < QCA6390_DEBUG_PBL_LOG_SRAM_MAX_SIZE; i += sizeof(val)) {
		mem_addr = QCA6390_DEBUG_PBL_LOG_SRAM_START + i;
		if (cnss_pci_reg_read(pci_priv, mem_addr, &val))
			break;
		cnss_pr_dbg("SRAM[0x%x] = 0x%x\n", mem_addr, val);
	}

	sbl_log_size = (sbl_log_size > QCA6390_DEBUG_SBL_LOG_SRAM_MAX_SIZE ?
			QCA6390_DEBUG_SBL_LOG_SRAM_MAX_SIZE : sbl_log_size);

	if (sbl_log_start < QCA6390_V2_SBL_DATA_START ||
	    sbl_log_start > QCA6390_V2_SBL_DATA_END ||
	    (sbl_log_start + sbl_log_size) > QCA6390_V2_SBL_DATA_END)
		goto out;

	cnss_pr_dbg("Dumping SBL log data\n");
	for (i = 0; i < sbl_log_size; i += sizeof(val)) {
		mem_addr = sbl_log_start + i;
		if (cnss_pci_reg_read(pci_priv, mem_addr, &val))
			break;
		cnss_pr_dbg("SRAM[0x%x] = 0x%x\n", mem_addr, val);
	}
	return;
out:
	cnss_pr_err("Invalid SBL log data\n");
}

/**
 * cnss_pci_dump_bl_sram_mem - Dump WLAN FW bootloader debug log
 * @pci_priv: PCI device private data structure of cnss platform driver
 *
 * Dump Primary and secondary bootloader debug log data. For SBL check the
 * log struct address and size for validity.
 *
 * Supported only on QCA6490
 *
 * Return: None
 */
static void cnss_pci_dump_bl_sram_mem(struct cnss_pci_data *pci_priv)
{
	int i;
	u32 mem_addr, val, pbl_stage, sbl_log_start, sbl_log_size;
	u32 pbl_wlan_boot_cfg, pbl_bootstrap_status;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	if (plat_priv->device_id == QCA6390_DEVICE_ID) {
		cnss_pci_dump_qca6390_sram_mem(pci_priv);
		return;
	} else if (plat_priv->device_id != QCA6490_DEVICE_ID) {
		return;
	}

	if (cnss_pci_check_link_status(pci_priv))
		return;

	cnss_pci_reg_read(pci_priv, QCA6490_TCSR_PBL_LOGGING_REG, &pbl_stage);
	cnss_pci_reg_read(pci_priv, QCA6490_PCIE_BHI_ERRDBG2_REG,
			  &sbl_log_start);
	cnss_pci_reg_read(pci_priv, QCA6490_PCIE_BHI_ERRDBG3_REG,
			  &sbl_log_size);
	cnss_pci_reg_read(pci_priv, QCA6490_PBL_WLAN_BOOT_CFG,
			  &pbl_wlan_boot_cfg);
	cnss_pci_reg_read(pci_priv, QCA6490_PBL_BOOTSTRAP_STATUS,
			  &pbl_bootstrap_status);
	cnss_pr_dbg("TCSR_PBL_LOGGING: 0x%08x PCIE_BHI_ERRDBG: Start: 0x%08x Size:0x%08x",
		    pbl_stage, sbl_log_start, sbl_log_size);
	cnss_pr_dbg("PBL_WLAN_BOOT_CFG: 0x%08x PBL_BOOTSTRAP_STATUS: 0x%08x",
		    pbl_wlan_boot_cfg, pbl_bootstrap_status);

	cnss_pr_dbg("Dumping PBL log data");
	/* cnss_pci_reg_read provides 32bit register values */
	for (i = 0; i < QCA6490_DEBUG_PBL_LOG_SRAM_MAX_SIZE; i += sizeof(val)) {
		mem_addr = QCA6490_DEBUG_PBL_LOG_SRAM_START + i;
		if (cnss_pci_reg_read(pci_priv, mem_addr, &val))
			break;
		cnss_pr_dbg("SRAM[0x%x] = 0x%x\n", mem_addr, val);
	}

	sbl_log_size = (sbl_log_size > QCA6490_DEBUG_SBL_LOG_SRAM_MAX_SIZE ?
			QCA6490_DEBUG_SBL_LOG_SRAM_MAX_SIZE : sbl_log_size);
	if (plat_priv->device_version.major_version == FW_V2_NUMBER) {
		if (sbl_log_start < QCA6490_V2_SBL_DATA_START ||
		    sbl_log_start > QCA6490_V2_SBL_DATA_END ||
		    (sbl_log_start + sbl_log_size) > QCA6490_V2_SBL_DATA_END)
			goto out;
	} else {
		if (sbl_log_start < QCA6490_V1_SBL_DATA_START ||
		    sbl_log_start > QCA6490_V1_SBL_DATA_END ||
		    (sbl_log_start + sbl_log_size) > QCA6490_V1_SBL_DATA_END)
			goto out;
	}

	cnss_pr_dbg("Dumping SBL log data");
	for (i = 0; i < sbl_log_size; i += sizeof(val)) {
		mem_addr = sbl_log_start + i;
		if (cnss_pci_reg_read(pci_priv, mem_addr, &val))
			break;
		cnss_pr_dbg("SRAM[0x%x] = 0x%x\n", mem_addr, val);
	}
	return;
out:
	cnss_pr_err("Invalid SBL log data");
}

static int cnss_qca6174_powerup(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	ret = cnss_power_on_device(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to power on device, err = %d\n", ret);
		goto out;
	}

	ret = cnss_resume_pci_link(pci_priv);
	if (ret) {
		cnss_pr_err("Failed to resume PCI link, err = %d\n", ret);
		goto power_off;
	}

	ret = cnss_pci_call_driver_probe(pci_priv);
	if (ret)
		goto suspend_link;

	return 0;
suspend_link:
	cnss_suspend_pci_link(pci_priv);
power_off:
	cnss_power_off_device(plat_priv);
out:
	return ret;
}

static int cnss_qca6174_shutdown(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	cnss_pci_pm_runtime_resume(pci_priv);

	ret = cnss_pci_call_driver_remove(pci_priv);
	if (ret == -EAGAIN)
		goto out;

	cnss_request_bus_bandwidth(&plat_priv->plat_dev->dev,
				   CNSS_BUS_WIDTH_NONE);
	cnss_pci_set_monitor_wake_intr(pci_priv, false);
	cnss_pci_set_auto_suspended(pci_priv, 0);

	ret = cnss_suspend_pci_link(pci_priv);
	if (ret)
		cnss_pr_err("Failed to suspend PCI link, err = %d\n", ret);

	cnss_power_off_device(plat_priv);

	clear_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state);
	clear_bit(CNSS_DRIVER_IDLE_SHUTDOWN, &plat_priv->driver_state);

out:
	return ret;
}

static void cnss_qca6174_crash_shutdown(struct cnss_pci_data *pci_priv)
{
	if (pci_priv->driver_ops && pci_priv->driver_ops->crash_shutdown)
		pci_priv->driver_ops->crash_shutdown(pci_priv->pci_dev);
}

static int cnss_qca6174_ramdump(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_ramdump_info *ramdump_info;

	ramdump_info = &plat_priv->ramdump_info;
	if (!ramdump_info->ramdump_size)
		return -EINVAL;

	return cnss_do_ramdump(plat_priv);
}

static int cnss_qca6290_powerup(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	unsigned int timeout;
	int retry = 0;

	if (plat_priv->ramdump_info_v2.dump_data_valid) {
		cnss_pci_clear_dump_info(pci_priv);
		cnss_pci_deinit_mhi(pci_priv);
	}

	plat_priv->power_up_error = 0;
retry:
	ret = cnss_power_on_device(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to power on device, err = %d\n", ret);
		goto out;
	}

	ret = cnss_resume_pci_link(pci_priv);
	if (ret) {
		cnss_pr_err("Failed to resume PCI link, err = %d\n", ret);
		if (test_bit(IGNORE_PCI_LINK_FAILURE,
			     &plat_priv->ctrl_params.quirks)) {
			cnss_pr_dbg("Ignore PCI link resume failure\n");
			ret = 0;
			goto out;
		}
		if (ret == -EAGAIN && retry++ < POWER_ON_RETRY_MAX_TIMES) {
			cnss_power_off_device(plat_priv);
			cnss_pr_dbg("Retry to resume PCI link #%d\n", retry);
			msleep(POWER_ON_RETRY_DELAY_MS * retry);
			goto retry;
		}
		/* Assert when it reaches maximum retries */
		CNSS_ASSERT(0);
		goto power_off;
	}

	cnss_pci_set_wlaon_pwr_ctrl(pci_priv, false, false, false);
	timeout = cnss_get_timeout(plat_priv, CNSS_TIMEOUT_QMI);

	ret = cnss_pci_start_mhi(pci_priv);
	if (ret) {
		cnss_fatal_err("Failed to start MHI, err = %d\n", ret);
		if (!test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state) &&
		    !pci_priv->pci_link_down_ind && timeout)
			mod_timer(&plat_priv->fw_boot_timer,
				  jiffies + msecs_to_jiffies(timeout >> 1));
		return 0;
	}

	if (test_bit(USE_CORE_ONLY_FW, &plat_priv->ctrl_params.quirks)) {
		clear_bit(CNSS_FW_BOOT_RECOVERY, &plat_priv->driver_state);
		clear_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state);
		return 0;
	}

	cnss_set_pin_connect_status(plat_priv);

	if (test_bit(QMI_BYPASS, &plat_priv->ctrl_params.quirks)) {
		ret = cnss_pci_call_driver_probe(pci_priv);
		if (ret)
			goto stop_mhi;
	} else if (timeout) {
		if (test_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state))
			timeout = timeout << 1;
		mod_timer(&plat_priv->fw_boot_timer,
			  jiffies + msecs_to_jiffies(timeout << 1));
	}

	return 0;

stop_mhi:
	cnss_pci_set_wlaon_pwr_ctrl(pci_priv, false, true, true);
	cnss_pci_power_off_mhi(pci_priv);
	cnss_suspend_pci_link(pci_priv);
	cnss_pci_deinit_mhi(pci_priv);
power_off:
	cnss_power_off_device(plat_priv);
out:
	return ret;
}

static int cnss_qca6290_shutdown(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	int do_force_wake = true;

	cnss_pci_pm_runtime_resume(pci_priv);

	ret = cnss_pci_call_driver_remove(pci_priv);
	if (ret == -EAGAIN)
		goto out;

	cnss_request_bus_bandwidth(&plat_priv->plat_dev->dev,
				   CNSS_BUS_WIDTH_NONE);
	cnss_pci_set_monitor_wake_intr(pci_priv, false);
	cnss_pci_set_auto_suspended(pci_priv, 0);

	if ((test_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state) ||
	     test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state) ||
	     test_bit(CNSS_DRIVER_IDLE_RESTART, &plat_priv->driver_state) ||
	     test_bit(CNSS_DRIVER_IDLE_SHUTDOWN, &plat_priv->driver_state)) &&
	    test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state)) {
		del_timer(&pci_priv->dev_rddm_timer);
		cnss_pci_collect_dump(pci_priv);
	}

	if (!cnss_is_device_powered_on(plat_priv)) {
		cnss_pr_dbg("Device is already powered off, ignore\n");
		goto skip_power_off;
	}

	if (test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state))
		do_force_wake = false;

	cnss_pci_set_wlaon_pwr_ctrl(pci_priv, false, true, do_force_wake);
	cnss_pci_power_off_mhi(pci_priv);
	ret = cnss_suspend_pci_link(pci_priv);
	if (ret)
		cnss_pr_err("Failed to suspend PCI link, err = %d\n", ret);
	if (!plat_priv->ramdump_info_v2.dump_data_valid)
		cnss_pci_deinit_mhi(pci_priv);

	cnss_power_off_device(plat_priv);

skip_power_off:
	pci_priv->remap_window = 0;

	clear_bit(CNSS_FW_READY, &plat_priv->driver_state);
	clear_bit(CNSS_FW_MEM_READY, &plat_priv->driver_state);
	if (test_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state) ||
	    test_bit(CNSS_DRIVER_IDLE_SHUTDOWN, &plat_priv->driver_state))
		clear_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state);
	clear_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state);
	clear_bit(CNSS_DRIVER_IDLE_SHUTDOWN, &plat_priv->driver_state);

out:
	return ret;
}

static void cnss_qca6290_crash_shutdown(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	cnss_pr_dbg("Crash shutdown with driver_state 0x%lx\n",
		    plat_priv->driver_state);

	cnss_pci_collect_dump_info(pci_priv, true);
}

static int cnss_qca6290_ramdump(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_ramdump_info_v2 *info_v2 = &plat_priv->ramdump_info_v2;
	struct cnss_dump_data *dump_data = &info_v2->dump_data;
	int ret = 0;

	if (!info_v2->dump_data_valid ||
	    dump_data->nentries == 0)
		return 0;

	ret = cnss_do_elf_ramdump(plat_priv);

	cnss_pci_clear_dump_info(pci_priv);
	cnss_pci_deinit_mhi(pci_priv);

	return ret;
}

int cnss_pci_dev_powerup(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	switch (pci_priv->device_id) {
	case QCA6174_DEVICE_ID:
		ret = cnss_qca6174_powerup(pci_priv);
		break;
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCA6490_DEVICE_ID:
		ret = cnss_qca6290_powerup(pci_priv);
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%x\n",
			    pci_priv->device_id);
		ret = -ENODEV;
	}

	return ret;
}

int cnss_pci_dev_shutdown(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	switch (pci_priv->device_id) {
	case QCA6174_DEVICE_ID:
		ret = cnss_qca6174_shutdown(pci_priv);
		break;
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCA6490_DEVICE_ID:
		ret = cnss_qca6290_shutdown(pci_priv);
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%x\n",
			    pci_priv->device_id);
		ret = -ENODEV;
	}

	return ret;
}

int cnss_pci_dev_crash_shutdown(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	switch (pci_priv->device_id) {
	case QCA6174_DEVICE_ID:
		cnss_qca6174_crash_shutdown(pci_priv);
		break;
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCA6490_DEVICE_ID:
		cnss_qca6290_crash_shutdown(pci_priv);
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%x\n",
			    pci_priv->device_id);
		ret = -ENODEV;
	}

	return ret;
}

int cnss_pci_dev_ramdump(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	switch (pci_priv->device_id) {
	case QCA6174_DEVICE_ID:
		ret = cnss_qca6174_ramdump(pci_priv);
		break;
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCA6490_DEVICE_ID:
		ret = cnss_qca6290_ramdump(pci_priv);
		break;
	default:
		cnss_pr_err("Unknown device_id found: 0x%x\n",
			    pci_priv->device_id);
		ret = -ENODEV;
	}

	return ret;
}

int cnss_pci_is_drv_connected(struct device *dev)
{
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(to_pci_dev(dev));

	if (!pci_priv)
		return -ENODEV;

	return pci_priv->drv_connected_last;
}
EXPORT_SYMBOL(cnss_pci_is_drv_connected);

int cnss_wlan_register_driver(struct cnss_wlan_driver *driver_ops)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
	struct cnss_pci_data *pci_priv;
	unsigned int timeout;
	struct cnss_cal_info *cal_info;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return -ENODEV;
	}

	pci_priv = plat_priv->bus_priv;
	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	if (pci_priv->driver_ops) {
		cnss_pr_err("Driver has already registered\n");
		return -EEXIST;
	}

	if (!driver_ops->id_table || !pci_dev_present(driver_ops->id_table)) {
		cnss_pr_err("PCIe device id is %x, not supported by loading driver\n",
			    pci_priv->device_id);
		return -ENODEV;
	}

	if (!test_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state))
		goto register_driver;

	cnss_pr_dbg("Start to wait for calibration to complete\n");

	timeout = cnss_get_timeout(plat_priv, CNSS_TIMEOUT_CALIBRATION);
	ret = wait_for_completion_timeout(&plat_priv->cal_complete,
					  msecs_to_jiffies(timeout));
	if (!ret) {
		cnss_pr_err("Timeout (%ums) waiting for calibration to complete\n",
			    timeout);
		if (!test_bit(CNSS_IN_REBOOT, &plat_priv->driver_state)) {
			cnss_pci_dump_bl_sram_mem(pci_priv);
			CNSS_ASSERT(0);
		}

		cal_info = kzalloc(sizeof(*cal_info), GFP_KERNEL);
		if (!cal_info)
			return -ENOMEM;

		cal_info->cal_status = CNSS_CAL_TIMEOUT;
		cnss_driver_event_post(plat_priv,
				       CNSS_DRIVER_EVENT_COLD_BOOT_CAL_DONE,
				       0, cal_info);
	}

	if (test_bit(CNSS_IN_REBOOT, &plat_priv->driver_state)) {
		cnss_pr_dbg("Reboot or shutdown is in progress, ignore register driver\n");
		return -EINVAL;
	}

register_driver:
	reinit_completion(&plat_priv->power_up_complete);
	ret = cnss_driver_event_post(plat_priv,
				     CNSS_DRIVER_EVENT_REGISTER_DRIVER,
				     CNSS_EVENT_SYNC_UNKILLABLE,
				     driver_ops);

	return ret;
}
EXPORT_SYMBOL(cnss_wlan_register_driver);

void cnss_wlan_unregister_driver(struct cnss_wlan_driver *driver_ops)
{
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
	int ret = 0;
	unsigned int timeout;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL\n");
		return;
	}

	mutex_lock(&plat_priv->driver_ops_lock);

	if (plat_priv->device_id == QCA6174_DEVICE_ID)
		goto skip_wait_power_up;

	timeout = cnss_get_timeout(plat_priv, CNSS_TIMEOUT_WLAN_WATCHDOG);
	ret = wait_for_completion_timeout(&plat_priv->power_up_complete,
					  msecs_to_jiffies(timeout));
	if (!ret) {
		cnss_pr_err("Timeout (%ums) waiting for driver power up to complete\n",
			    timeout);
		CNSS_ASSERT(0);
	}

skip_wait_power_up:
	if (!test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state) &&
	    !test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state))
		goto skip_wait_recovery;

	reinit_completion(&plat_priv->recovery_complete);
	timeout = cnss_get_timeout(plat_priv, CNSS_TIMEOUT_RECOVERY);
	ret = wait_for_completion_timeout(&plat_priv->recovery_complete,
					  msecs_to_jiffies(timeout));
	if (!ret) {
		cnss_pr_err("Timeout (%ums) waiting for recovery to complete\n",
			    timeout);
		CNSS_ASSERT(0);
	}

skip_wait_recovery:
	cnss_driver_event_post(plat_priv,
			       CNSS_DRIVER_EVENT_UNREGISTER_DRIVER,
			       CNSS_EVENT_SYNC_UNKILLABLE, NULL);

	mutex_unlock(&plat_priv->driver_ops_lock);
}
EXPORT_SYMBOL(cnss_wlan_unregister_driver);

int cnss_pci_register_driver_hdlr(struct cnss_pci_data *pci_priv,
				  void *data)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	if (test_bit(CNSS_IN_REBOOT, &plat_priv->driver_state)) {
		cnss_pr_dbg("Reboot or shutdown is in progress, ignore register driver\n");
		return -EINVAL;
	}

	set_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
	pci_priv->driver_ops = data;

	ret = cnss_pci_dev_powerup(pci_priv);
	if (ret) {
		clear_bit(CNSS_DRIVER_LOADING, &plat_priv->driver_state);
		pci_priv->driver_ops = NULL;
	}

	return ret;
}

int cnss_pci_unregister_driver_hdlr(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;

	set_bit(CNSS_DRIVER_UNLOADING, &plat_priv->driver_state);
	cnss_pci_dev_shutdown(pci_priv);
	pci_priv->driver_ops = NULL;

	return 0;
}

static bool cnss_pci_is_drv_supported(struct cnss_pci_data *pci_priv)
{
	struct pci_dev *root_port = pci_find_pcie_root_port(pci_priv->pci_dev);
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct device_node *root_of_node;
	bool drv_supported = false;

	if (!root_port) {
		cnss_pr_err("PCIe DRV is not supported as root port is null\n");
		return drv_supported;
	}

	root_of_node = root_port->dev.of_node;

	if (root_of_node->parent)
		drv_supported = of_property_read_bool(root_of_node->parent,
						      "qcom,drv-supported");

	cnss_pr_dbg("PCIe DRV is %s\n",
		    drv_supported ? "supported" : "not supported");

	if (drv_supported)
		plat_priv->cap.cap_flag |= CNSS_HAS_DRV_SUPPORT;

	return drv_supported;
}

static void cnss_pci_event_cb(struct msm_pcie_notify *notify)
{
	struct pci_dev *pci_dev;
	struct cnss_pci_data *pci_priv;
	struct cnss_plat_data *plat_priv = NULL;
	int ret = 0;

	if (!notify)
		return;

	pci_dev = notify->user;
	if (!pci_dev)
		return;

	pci_priv = cnss_get_pci_priv(pci_dev);
	if (!pci_priv)
		return;

	switch (notify->event) {
	case MSM_PCIE_EVENT_LINK_RECOVER:
		cnss_pr_dbg("PCI link recover callback\n");

		plat_priv = pci_priv->plat_priv;
		if (!plat_priv) {
			cnss_pr_err("plat_priv is NULL\n");
			return;
		}

		plat_priv->ctrl_params.quirks |= BIT(LINK_DOWN_SELF_RECOVERY);

		ret = msm_pcie_pm_control(MSM_PCIE_HANDLE_LINKDOWN,
					  pci_dev->bus->number, pci_dev, NULL,
					  PM_OPTIONS_DEFAULT);
		if (ret)
			cnss_pci_handle_linkdown(pci_priv);
		break;
	case MSM_PCIE_EVENT_LINKDOWN:
		cnss_pr_dbg("PCI link down event callback\n");
		cnss_pci_handle_linkdown(pci_priv);
		break;
	case MSM_PCIE_EVENT_WAKEUP:
		complete(&pci_priv->wake_event);
		if (cnss_pci_get_monitor_wake_intr(pci_priv) &&
		    cnss_pci_get_auto_suspended(pci_priv)) {
			cnss_pci_set_monitor_wake_intr(pci_priv, false);
			cnss_pci_pm_request_resume(pci_priv);
		}
		break;
	case MSM_PCIE_EVENT_DRV_CONNECT:
		cnss_pr_dbg("DRV subsystem is connected\n");
		cnss_pci_set_drv_connected(pci_priv, 1);
		break;
	case MSM_PCIE_EVENT_DRV_DISCONNECT:
		cnss_pr_dbg("DRV subsystem is disconnected\n");
		if (cnss_pci_get_auto_suspended(pci_priv))
			cnss_pci_pm_request_resume(pci_priv);
		cnss_pci_set_drv_connected(pci_priv, 0);
		break;
	default:
		cnss_pr_err("Received invalid PCI event: %d\n", notify->event);
	}
}

static int cnss_reg_pci_event(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct msm_pcie_register_event *pci_event;

	pci_event = &pci_priv->msm_pci_event;
	pci_event->events = MSM_PCIE_EVENT_LINK_RECOVER |
			    MSM_PCIE_EVENT_LINKDOWN |
			    MSM_PCIE_EVENT_WAKEUP;

	if (cnss_pci_is_drv_supported(pci_priv))
		pci_event->events = pci_event->events |
			MSM_PCIE_EVENT_DRV_CONNECT |
			MSM_PCIE_EVENT_DRV_DISCONNECT;

	pci_event->user = pci_priv->pci_dev;
	pci_event->mode = MSM_PCIE_TRIGGER_CALLBACK;
	pci_event->callback = cnss_pci_event_cb;
	pci_event->options = MSM_PCIE_CONFIG_NO_RECOVERY;

	ret = msm_pcie_register_event(pci_event);
	if (ret)
		cnss_pr_err("Failed to register MSM PCI event, err = %d\n",
			    ret);

	return ret;
}

static void cnss_dereg_pci_event(struct cnss_pci_data *pci_priv)
{
	msm_pcie_deregister_event(&pci_priv->msm_pci_event);
}

static int cnss_pci_suspend_driver(struct cnss_pci_data *pci_priv)
{
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	struct cnss_wlan_driver *driver_ops = pci_priv->driver_ops;
	int ret = 0;

	pm_message_t state = { .event = PM_EVENT_SUSPEND };

	if (driver_ops && driver_ops->suspend) {
		ret = driver_ops->suspend(pci_dev, state);
		if (ret) {
			cnss_pr_err("Failed to suspend host driver, err = %d\n",
				    ret);
			ret = -EAGAIN;
		}
	}

	return ret;
}

static int cnss_pci_resume_driver(struct cnss_pci_data *pci_priv)
{
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	struct cnss_wlan_driver *driver_ops = pci_priv->driver_ops;
	int ret = 0;

	if (driver_ops && driver_ops->resume) {
		ret = driver_ops->resume(pci_dev);
		if (ret)
			cnss_pr_err("Failed to resume host driver, err = %d\n",
				    ret);
	}

	return ret;
}

int cnss_pci_suspend_bus(struct cnss_pci_data *pci_priv)
{
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	int ret = 0;

	if (pci_priv->pci_link_state == PCI_LINK_DOWN)
		goto out;

	if (cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_SUSPEND)) {
		ret = -EAGAIN;
		goto out;
	}

	if (pci_priv->drv_connected_last)
		goto skip_disable_pci;

	pci_clear_master(pci_dev);
	cnss_set_pci_config_space(pci_priv, SAVE_PCI_CONFIG_SPACE);
	pci_disable_device(pci_dev);

	ret = pci_set_power_state(pci_dev, PCI_D3hot);
	if (ret)
		cnss_pr_err("Failed to set D3Hot, err = %d\n", ret);

skip_disable_pci:
	if (cnss_set_pci_link(pci_priv, PCI_LINK_DOWN)) {
		ret = -EAGAIN;
		goto resume_mhi;
	}
	pci_priv->pci_link_state = PCI_LINK_DOWN;

	return 0;

resume_mhi:
	if (!pci_is_enabled(pci_dev))
		if (pci_enable_device(pci_dev))
			cnss_pr_err("Failed to enable PCI device\n");
	if (pci_priv->saved_state)
		cnss_set_pci_config_space(pci_priv, RESTORE_PCI_CONFIG_SPACE);
	pci_set_master(pci_dev);
	cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_RESUME);
out:
	return ret;
}

int cnss_pci_resume_bus(struct cnss_pci_data *pci_priv)
{
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	int ret = 0;

	if (pci_priv->pci_link_state == PCI_LINK_UP)
		goto out;

	if (cnss_set_pci_link(pci_priv, PCI_LINK_UP)) {
		cnss_fatal_err("Failed to resume PCI link from suspend\n");
		cnss_pci_link_down(&pci_dev->dev);
		ret = -EAGAIN;
		goto out;
	}

	pci_priv->pci_link_state = PCI_LINK_UP;

	if (pci_priv->drv_connected_last)
		goto skip_enable_pci;

	ret = pci_enable_device(pci_dev);
	if (ret) {
		cnss_pr_err("Failed to enable PCI device, err = %d\n",
			    ret);
		goto out;
	}

	if (pci_priv->saved_state)
		cnss_set_pci_config_space(pci_priv,
					  RESTORE_PCI_CONFIG_SPACE);
	pci_set_master(pci_dev);

skip_enable_pci:
	cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_RESUME);
out:
	return ret;
}

static int cnss_pci_suspend(struct device *dev)
{
	int ret = 0;
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(to_pci_dev(dev));
	struct cnss_plat_data *plat_priv;

	if (!pci_priv)
		goto out;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		goto out;

	if (!cnss_is_device_powered_on(plat_priv))
		goto out;

	if (!test_bit(DISABLE_DRV, &plat_priv->ctrl_params.quirks)) {
		pci_priv->drv_connected_last =
			cnss_pci_get_drv_connected(pci_priv);
		if (!pci_priv->drv_connected_last) {
			cnss_pr_dbg("Firmware does not support non-DRV suspend, reject\n");
			ret = -EAGAIN;
			goto out;
		}
	}

	set_bit(CNSS_IN_SUSPEND_RESUME, &plat_priv->driver_state);

	ret = cnss_pci_suspend_driver(pci_priv);
	if (ret)
		goto clear_flag;

	if (!pci_priv->disable_pc) {
		mutex_lock(&pci_priv->bus_lock);
		ret = cnss_pci_suspend_bus(pci_priv);
		mutex_unlock(&pci_priv->bus_lock);
		if (ret)
			goto resume_driver;
	}

	cnss_pci_set_monitor_wake_intr(pci_priv, false);

	return 0;

resume_driver:
	cnss_pci_resume_driver(pci_priv);
clear_flag:
	pci_priv->drv_connected_last = 0;
	clear_bit(CNSS_IN_SUSPEND_RESUME, &plat_priv->driver_state);
out:
	return ret;
}

static int cnss_pci_resume(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv;

	if (!pci_priv)
		goto out;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		goto out;

	if (pci_priv->pci_link_down_ind)
		goto out;

	if (!cnss_is_device_powered_on(pci_priv->plat_priv))
		goto out;

	if (!pci_priv->disable_pc) {
		mutex_lock(&pci_priv->bus_lock);
		ret = cnss_pci_resume_bus(pci_priv);
		mutex_unlock(&pci_priv->bus_lock);
		if (ret)
			goto out;
	}

	ret = cnss_pci_resume_driver(pci_priv);

	pci_priv->drv_connected_last = 0;
	clear_bit(CNSS_IN_SUSPEND_RESUME, &plat_priv->driver_state);

out:
	return ret;
}

static int cnss_pci_suspend_noirq(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		goto out;

	driver_ops = pci_priv->driver_ops;
	if (driver_ops && driver_ops->suspend_noirq)
		ret = driver_ops->suspend_noirq(pci_dev);

out:
	return ret;
}

static int cnss_pci_resume_noirq(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		goto out;

	driver_ops = pci_priv->driver_ops;
	if (driver_ops && driver_ops->resume_noirq &&
	    !pci_priv->pci_link_down_ind)
		ret = driver_ops->resume_noirq(pci_dev);

out:
	return ret;
}

static int cnss_pci_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv;
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		return -EAGAIN;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		return -EAGAIN;

	if (!cnss_is_device_powered_on(pci_priv->plat_priv))
		return -EAGAIN;

	if (pci_priv->pci_link_down_ind) {
		cnss_pr_dbg("PCI link down recovery is in progress!\n");
		return -EAGAIN;
	}

	if (!test_bit(DISABLE_DRV, &plat_priv->ctrl_params.quirks)) {
		pci_priv->drv_connected_last =
			cnss_pci_get_drv_connected(pci_priv);
		if (!pci_priv->drv_connected_last) {
			cnss_pr_dbg("Firmware does not support non-DRV suspend, reject\n");
			return -EAGAIN;
		}
	}

	cnss_pr_vdbg("Runtime suspend start\n");

	driver_ops = pci_priv->driver_ops;
	if (driver_ops && driver_ops->runtime_ops &&
	    driver_ops->runtime_ops->runtime_suspend)
		ret = driver_ops->runtime_ops->runtime_suspend(pci_dev);
	else
		ret = cnss_auto_suspend(dev);

	if (ret)
		pci_priv->drv_connected_last = 0;

	cnss_pr_vdbg("Runtime suspend status: %d\n", ret);

	return ret;
}

static int cnss_pci_runtime_resume(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_wlan_driver *driver_ops;

	if (!pci_priv)
		return -EAGAIN;

	if (!cnss_is_device_powered_on(pci_priv->plat_priv))
		return -EAGAIN;

	if (pci_priv->pci_link_down_ind) {
		cnss_pr_dbg("PCI link down recovery is in progress!\n");
		return -EAGAIN;
	}

	cnss_pr_vdbg("Runtime resume start\n");

	driver_ops = pci_priv->driver_ops;
	if (driver_ops && driver_ops->runtime_ops &&
	    driver_ops->runtime_ops->runtime_resume)
		ret = driver_ops->runtime_ops->runtime_resume(pci_dev);
	else
		ret = cnss_auto_resume(dev);

	if (!ret)
		pci_priv->drv_connected_last = 0;

	cnss_pr_vdbg("Runtime resume status: %d\n", ret);

	return ret;
}

static int cnss_pci_runtime_idle(struct device *dev)
{
	cnss_pr_vdbg("Runtime idle\n");

	pm_request_autosuspend(dev);

	return -EBUSY;
}

int cnss_wlan_pm_control(struct device *dev, bool vote)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	int ret = 0;

	if (!pci_priv)
		return -ENODEV;

	ret = msm_pcie_pm_control(vote ? MSM_PCIE_DISABLE_PC :
				  MSM_PCIE_ENABLE_PC,
				  pci_dev->bus->number, pci_dev,
				  NULL, PM_OPTIONS_DEFAULT);
	if (ret)
		return ret;

	pci_priv->disable_pc = vote;
	cnss_pr_dbg("%s PCIe power collapse\n", vote ? "disable" : "enable");

	return 0;
}
EXPORT_SYMBOL(cnss_wlan_pm_control);

static void cnss_pci_pm_runtime_get_record(struct cnss_pci_data *pci_priv,
					   enum cnss_rtpm_id id)
{
	if (id >= RTPM_ID_MAX)
		return;

	atomic_inc(&pci_priv->pm_stats.runtime_get);
	atomic_inc(&pci_priv->pm_stats.runtime_get_id[id]);
	pci_priv->pm_stats.runtime_get_timestamp_id[id] =
		cnss_get_host_timestamp(pci_priv->plat_priv);
}

static void cnss_pci_pm_runtime_put_record(struct cnss_pci_data *pci_priv,
					   enum cnss_rtpm_id id)
{
	if (id >= RTPM_ID_MAX)
		return;

	atomic_inc(&pci_priv->pm_stats.runtime_put);
	atomic_inc(&pci_priv->pm_stats.runtime_put_id[id]);
	pci_priv->pm_stats.runtime_put_timestamp_id[id] =
		cnss_get_host_timestamp(pci_priv->plat_priv);
}

void cnss_pci_pm_runtime_show_usage_count(struct cnss_pci_data *pci_priv)
{
	struct device *dev;

	if (!pci_priv)
		return;

	dev = &pci_priv->pci_dev->dev;

	cnss_pr_dbg("Runtime PM usage count: %d\n",
		    atomic_read(&dev->power.usage_count));
}

int cnss_pci_pm_request_resume(struct cnss_pci_data *pci_priv)
{
	struct device *dev;
	enum rpm_status status;

	if (!pci_priv)
		return -ENODEV;

	dev = &pci_priv->pci_dev->dev;

	status = dev->power.runtime_status;
	if (status == RPM_SUSPENDING || status == RPM_SUSPENDED)
		cnss_pr_vdbg("Runtime PM resume is requested by %ps\n",
			     (void *)_RET_IP_);

	return pm_request_resume(dev);
}

int cnss_pci_pm_runtime_resume(struct cnss_pci_data *pci_priv)
{
	struct device *dev;
	enum rpm_status status;

	if (!pci_priv)
		return -ENODEV;

	dev = &pci_priv->pci_dev->dev;

	status = dev->power.runtime_status;
	if (status == RPM_SUSPENDING || status == RPM_SUSPENDED)
		cnss_pr_vdbg("Runtime PM resume is requested by %ps\n",
			     (void *)_RET_IP_);

	return pm_runtime_resume(dev);
}

int cnss_pci_pm_runtime_get(struct cnss_pci_data *pci_priv,
			    enum cnss_rtpm_id id)
{
	struct device *dev;
	enum rpm_status status;

	if (!pci_priv)
		return -ENODEV;

	dev = &pci_priv->pci_dev->dev;

	status = dev->power.runtime_status;
	if (status == RPM_SUSPENDING || status == RPM_SUSPENDED)
		cnss_pr_vdbg("Runtime PM resume is requested by %ps\n",
			     (void *)_RET_IP_);

	cnss_pci_pm_runtime_get_record(pci_priv, id);

	return pm_runtime_get(dev);
}

int cnss_pci_pm_runtime_get_sync(struct cnss_pci_data *pci_priv,
				 enum cnss_rtpm_id id)
{
	struct device *dev;
	enum rpm_status status;

	if (!pci_priv)
		return -ENODEV;

	dev = &pci_priv->pci_dev->dev;

	status = dev->power.runtime_status;
	if (status == RPM_SUSPENDING || status == RPM_SUSPENDED)
		cnss_pr_vdbg("Runtime PM resume is requested by %ps\n",
			     (void *)_RET_IP_);

	cnss_pci_pm_runtime_get_record(pci_priv, id);

	return pm_runtime_get_sync(dev);
}

void cnss_pci_pm_runtime_get_noresume(struct cnss_pci_data *pci_priv,
				      enum cnss_rtpm_id id)
{
	if (!pci_priv)
		return;

	cnss_pci_pm_runtime_get_record(pci_priv, id);
	pm_runtime_get_noresume(&pci_priv->pci_dev->dev);
}

int cnss_pci_pm_runtime_put_autosuspend(struct cnss_pci_data *pci_priv,
					enum cnss_rtpm_id id)
{
	struct device *dev;

	if (!pci_priv)
		return -ENODEV;

	dev = &pci_priv->pci_dev->dev;

	if (atomic_read(&dev->power.usage_count) == 0) {
		cnss_pr_dbg("Ignore excessive runtime PM put operation\n");
		return -EINVAL;
	}

	cnss_pci_pm_runtime_put_record(pci_priv, id);

	return pm_runtime_put_autosuspend(&pci_priv->pci_dev->dev);
}

void cnss_pci_pm_runtime_put_noidle(struct cnss_pci_data *pci_priv,
				    enum cnss_rtpm_id id)
{
	struct device *dev;

	if (!pci_priv)
		return;

	dev = &pci_priv->pci_dev->dev;

	if (atomic_read(&dev->power.usage_count) == 0) {
		cnss_pr_dbg("Ignore excessive runtime PM put operation\n");
		return;
	}

	cnss_pci_pm_runtime_put_record(pci_priv, id);
	pm_runtime_put_noidle(&pci_priv->pci_dev->dev);
}

void cnss_pci_pm_runtime_mark_last_busy(struct cnss_pci_data *pci_priv)
{
	if (!pci_priv)
		return;

	pm_runtime_mark_last_busy(&pci_priv->pci_dev->dev);
}

int cnss_auto_suspend(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv;
	struct cnss_bus_bw_info *bus_bw_info;

	if (!pci_priv)
		return -ENODEV;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		return -ENODEV;

	mutex_lock(&pci_priv->bus_lock);
	ret = cnss_pci_suspend_bus(pci_priv);
	if (ret) {
		mutex_unlock(&pci_priv->bus_lock);
		return ret;
	}

	cnss_pci_set_auto_suspended(pci_priv, 1);
	mutex_unlock(&pci_priv->bus_lock);

	cnss_pci_set_monitor_wake_intr(pci_priv, true);

	bus_bw_info = &plat_priv->bus_bw_info;
	msm_bus_scale_client_update_request(bus_bw_info->bus_client,
					    CNSS_BUS_WIDTH_NONE);

	return 0;
}
EXPORT_SYMBOL(cnss_auto_suspend);

int cnss_auto_resume(struct device *dev)
{
	int ret = 0;
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv;
	struct cnss_bus_bw_info *bus_bw_info;

	if (!pci_priv)
		return -ENODEV;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		return -ENODEV;

	mutex_lock(&pci_priv->bus_lock);
	ret = cnss_pci_resume_bus(pci_priv);
	if (ret) {
		mutex_unlock(&pci_priv->bus_lock);
		return ret;
	}

	cnss_pci_set_auto_suspended(pci_priv, 0);
	mutex_unlock(&pci_priv->bus_lock);

	bus_bw_info = &plat_priv->bus_bw_info;
	msm_bus_scale_client_update_request(bus_bw_info->bus_client,
					    bus_bw_info->current_bw_vote);

	return 0;
}
EXPORT_SYMBOL(cnss_auto_resume);

int cnss_pci_force_wake_request_sync(struct device *dev, int timeout_us)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv;
	struct mhi_controller *mhi_ctrl;

	if (!pci_priv)
		return -ENODEV;

	if (pci_priv->device_id != QCA6390_DEVICE_ID &&
	    pci_priv->device_id != QCA6490_DEVICE_ID)
		return 0;

	mhi_ctrl = pci_priv->mhi_ctrl;
	if (!mhi_ctrl)
		return -EINVAL;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		return -ENODEV;

	if (test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state))
		return -EAGAIN;

	if (timeout_us) {
		/* Busy wait for timeout_us */
		return mhi_device_get_sync_atomic(mhi_ctrl->mhi_dev,
						  timeout_us, false);
	} else {
		/* Sleep wait for mhi_ctrl->timeout_ms */
		return mhi_device_get_sync(mhi_ctrl->mhi_dev, MHI_VOTE_DEVICE);
	}
}
EXPORT_SYMBOL(cnss_pci_force_wake_request_sync);

int cnss_pci_force_wake_request(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv;
	struct mhi_controller *mhi_ctrl;

	if (!pci_priv)
		return -ENODEV;

	if (pci_priv->device_id != QCA6390_DEVICE_ID &&
	    pci_priv->device_id != QCA6490_DEVICE_ID)
		return 0;

	mhi_ctrl = pci_priv->mhi_ctrl;
	if (!mhi_ctrl)
		return -EINVAL;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		return -ENODEV;

	if (test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state))
		return -EAGAIN;

	mhi_device_get(mhi_ctrl->mhi_dev, MHI_VOTE_DEVICE);

	return 0;
}
EXPORT_SYMBOL(cnss_pci_force_wake_request);

int cnss_pci_is_device_awake(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct mhi_controller *mhi_ctrl;

	if (!pci_priv)
		return -ENODEV;

	if (pci_priv->device_id != QCA6390_DEVICE_ID &&
	    pci_priv->device_id != QCA6490_DEVICE_ID)
		return true;

	mhi_ctrl = pci_priv->mhi_ctrl;
	if (!mhi_ctrl)
		return -EINVAL;

	return (mhi_ctrl->dev_state == MHI_STATE_M0);
}
EXPORT_SYMBOL(cnss_pci_is_device_awake);

int cnss_pci_force_wake_release(struct device *dev)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv;
	struct mhi_controller *mhi_ctrl;

	if (!pci_priv)
		return -ENODEV;

	if (pci_priv->device_id != QCA6390_DEVICE_ID &&
	    pci_priv->device_id != QCA6490_DEVICE_ID)
		return 0;

	mhi_ctrl = pci_priv->mhi_ctrl;
	if (!mhi_ctrl)
		return -EINVAL;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		return -ENODEV;

	if (test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state))
		return -EAGAIN;

	mhi_device_put(mhi_ctrl->mhi_dev, MHI_VOTE_DEVICE);

	return 0;
}
EXPORT_SYMBOL(cnss_pci_force_wake_release);

int cnss_pci_qmi_send_get(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv)
		return -ENODEV;

	mutex_lock(&pci_priv->bus_lock);
	if (!cnss_pci_get_auto_suspended(pci_priv))
		goto out;

	cnss_pr_vdbg("Starting to handle get info prepare\n");

	ret = cnss_pci_resume_bus(pci_priv);

out:
	mutex_unlock(&pci_priv->bus_lock);
	return ret;
}

int cnss_pci_qmi_send_put(struct cnss_pci_data *pci_priv)
{
	int ret = 0;

	if (!pci_priv)
		return -ENODEV;

	mutex_lock(&pci_priv->bus_lock);
	if (!cnss_pci_get_auto_suspended(pci_priv))
		goto out;

	cnss_pr_vdbg("Starting to handle get info done\n");

	ret = cnss_pci_suspend_bus(pci_priv);

out:
	mutex_unlock(&pci_priv->bus_lock);
	return ret;
}

int cnss_pci_alloc_fw_mem(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *fw_mem = plat_priv->fw_mem;
	struct device *dev = &pci_priv->pci_dev->dev;
	int i;

	for (i = 0; i < plat_priv->fw_mem_seg_len; i++) {
		if (!fw_mem[i].va && fw_mem[i].size) {
			fw_mem[i].va =
				dma_alloc_attrs(dev, fw_mem[i].size,
						&fw_mem[i].pa, GFP_KERNEL,
						fw_mem[i].attrs);

			if (!fw_mem[i].va) {
				cnss_pr_err("Failed to allocate memory for FW, size: 0x%zx, type: %u\n",
					    fw_mem[i].size, fw_mem[i].type);

				return -ENOMEM;
			}
		}
	}

	return 0;
}

static void cnss_pci_free_fw_mem(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *fw_mem = plat_priv->fw_mem;
	struct device *dev = &pci_priv->pci_dev->dev;
	int i;

	for (i = 0; i < plat_priv->fw_mem_seg_len; i++) {
		if (fw_mem[i].va && fw_mem[i].size) {
			cnss_pr_dbg("Freeing memory for FW, va: 0x%pK, pa: %pa, size: 0x%zx, type: %u\n",
				    fw_mem[i].va, &fw_mem[i].pa,
				    fw_mem[i].size, fw_mem[i].type);
			dma_free_attrs(dev, fw_mem[i].size,
				       fw_mem[i].va, fw_mem[i].pa,
				       fw_mem[i].attrs);
			fw_mem[i].va = NULL;
			fw_mem[i].pa = 0;
			fw_mem[i].size = 0;
			fw_mem[i].type = 0;
		}
	}

	plat_priv->fw_mem_seg_len = 0;
}

int cnss_pci_alloc_qdss_mem(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *qdss_mem = plat_priv->qdss_mem;
	int i, j;

	for (i = 0; i < plat_priv->qdss_mem_seg_len; i++) {
		if (!qdss_mem[i].va && qdss_mem[i].size) {
			qdss_mem[i].va =
				dma_alloc_coherent(&pci_priv->pci_dev->dev,
						   qdss_mem[i].size,
						   &qdss_mem[i].pa,
						   GFP_KERNEL);
			if (!qdss_mem[i].va) {
				cnss_pr_err("Failed to allocate QDSS memory for FW, size: 0x%zx, type: %u, chuck-ID: %d\n",
					    qdss_mem[i].size,
					    qdss_mem[i].type, i);
				break;
			}
		}
	}

	/* Best-effort allocation for QDSS trace */
	if (i < plat_priv->qdss_mem_seg_len) {
		for (j = i; j < plat_priv->qdss_mem_seg_len; j++) {
			qdss_mem[j].type = 0;
			qdss_mem[j].size = 0;
		}
		plat_priv->qdss_mem_seg_len = i;
	}

	return 0;
}

void cnss_pci_free_qdss_mem(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *qdss_mem = plat_priv->qdss_mem;
	int i;

	for (i = 0; i < plat_priv->qdss_mem_seg_len; i++) {
		if (qdss_mem[i].va && qdss_mem[i].size) {
			cnss_pr_dbg("Freeing memory for QDSS: pa: %pa, size: 0x%zx, type: %u\n",
				    &qdss_mem[i].pa, qdss_mem[i].size,
				    qdss_mem[i].type);
			dma_free_coherent(&pci_priv->pci_dev->dev,
					  qdss_mem[i].size, qdss_mem[i].va,
					  qdss_mem[i].pa);
			qdss_mem[i].va = NULL;
			qdss_mem[i].pa = 0;
			qdss_mem[i].size = 0;
			qdss_mem[i].type = 0;
		}
	}
	plat_priv->qdss_mem_seg_len = 0;
}

int cnss_pci_load_m3(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *m3_mem = &plat_priv->m3_mem;
	char filename[MAX_FIRMWARE_NAME_LEN];
	const struct firmware *fw_entry;
	int ret = 0;

	if (!m3_mem->va && !m3_mem->size) {
		cnss_pci_add_fw_prefix_name(pci_priv, filename,
					    DEFAULT_M3_FILE_NAME);

		ret = request_firmware(&fw_entry, filename,
				       &pci_priv->pci_dev->dev);
		if (ret) {
			cnss_pr_err("Failed to load M3 image: %s\n", filename);
			return ret;
		}

		m3_mem->va = dma_alloc_coherent(&pci_priv->pci_dev->dev,
						fw_entry->size, &m3_mem->pa,
						GFP_KERNEL);
		if (!m3_mem->va) {
			cnss_pr_err("Failed to allocate memory for M3, size: 0x%zx\n",
				    fw_entry->size);
			release_firmware(fw_entry);
			return -ENOMEM;
		}

		memcpy(m3_mem->va, fw_entry->data, fw_entry->size);
		m3_mem->size = fw_entry->size;
		release_firmware(fw_entry);
	}

	return 0;
}

static void cnss_pci_free_m3_mem(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *m3_mem = &plat_priv->m3_mem;

	if (m3_mem->va && m3_mem->size) {
		cnss_pr_dbg("Freeing memory for M3, va: 0x%pK, pa: %pa, size: 0x%zx\n",
			    m3_mem->va, &m3_mem->pa, m3_mem->size);
		dma_free_coherent(&pci_priv->pci_dev->dev, m3_mem->size,
				  m3_mem->va, m3_mem->pa);
	}

	m3_mem->va = NULL;
	m3_mem->pa = 0;
	m3_mem->size = 0;
}

void cnss_pci_fw_boot_timeout_hdlr(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv;

	if (!pci_priv)
		return;

	cnss_fatal_err("Timeout waiting for FW ready indication\n");

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		return;

	if (test_bit(CNSS_COLD_BOOT_CAL, &plat_priv->driver_state)) {
		cnss_pr_dbg("Ignore FW ready timeout for calibration mode\n");
		return;
	}

	cnss_schedule_recovery(&pci_priv->pci_dev->dev,
			       CNSS_REASON_TIMEOUT);
}

static int cnss_pci_smmu_fault_handler(struct iommu_domain *domain,
				       struct device *dev, unsigned long iova,
				       int flags, void *handler_token)
{
	struct cnss_pci_data *pci_priv = handler_token;

	cnss_fatal_err("SMMU fault happened with IOVA 0x%lx\n", iova);

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -ENODEV;
	}

	cnss_pci_update_status(pci_priv, CNSS_FW_DOWN);
	cnss_force_fw_assert(&pci_priv->pci_dev->dev);

	/* IOMMU driver requires non-zero return value to print debug info. */
	return -EINVAL;
}

static int cnss_pci_init_smmu(struct cnss_pci_data *pci_priv)
{
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct device_node *of_node;
	struct resource *res;
	const char *iommu_dma_type;
	u32 addr_win[2];
	int ret = 0;

	of_node = of_parse_phandle(pci_dev->dev.of_node, "qcom,iommu-group", 0);
	if (!of_node)
		return ret;

	cnss_pr_dbg("Initializing SMMU\n");

	pci_priv->iommu_domain = iommu_get_domain_for_dev(&pci_dev->dev);
	ret = of_property_read_string(of_node, "qcom,iommu-dma",
				      &iommu_dma_type);
	if (!ret && !strcmp("fastmap", iommu_dma_type)) {
		cnss_pr_dbg("Enabling SMMU S1 stage\n");
		pci_priv->smmu_s1_enable = true;
		iommu_set_fault_handler(pci_priv->iommu_domain,
					cnss_pci_smmu_fault_handler, pci_priv);
	}

	ret = of_property_read_u32_array(of_node,  "qcom,iommu-dma-addr-pool",
					 addr_win, ARRAY_SIZE(addr_win));
	if (ret) {
		cnss_pr_err("Invalid SMMU size window, err = %d\n", ret);
		of_node_put(of_node);
		return ret;
	}

	pci_priv->smmu_iova_start = addr_win[0];
	pci_priv->smmu_iova_len = addr_win[1];
	cnss_pr_dbg("smmu_iova_start: %pa, smmu_iova_len: 0x%zx\n",
		    &pci_priv->smmu_iova_start,
		    pci_priv->smmu_iova_len);

	res = platform_get_resource_byname(plat_priv->plat_dev, IORESOURCE_MEM,
					   "smmu_iova_ipa");
	if (res) {
		pci_priv->smmu_iova_ipa_start = res->start;
		pci_priv->smmu_iova_ipa_current = res->start;
		pci_priv->smmu_iova_ipa_len = resource_size(res);
		cnss_pr_dbg("smmu_iova_ipa_start: %pa, smmu_iova_ipa_len: 0x%zx\n",
			    &pci_priv->smmu_iova_ipa_start,
			    pci_priv->smmu_iova_ipa_len);
	}

	pci_priv->iommu_geometry = of_property_read_bool(of_node,
							 "qcom,iommu-geometry");
	cnss_pr_dbg("iommu_geometry: %d\n", pci_priv->iommu_geometry);

	of_node_put(of_node);

	return 0;
}

static void cnss_pci_deinit_smmu(struct cnss_pci_data *pci_priv)
{
	pci_priv->iommu_domain = NULL;
}

int cnss_pci_get_iova(struct cnss_pci_data *pci_priv, u64 *addr, u64 *size)
{
	if (!pci_priv)
		return -ENODEV;

	if (!pci_priv->smmu_iova_len)
		return -EINVAL;

	*addr = pci_priv->smmu_iova_start;
	*size = pci_priv->smmu_iova_len;

	return 0;
}

int cnss_pci_get_iova_ipa(struct cnss_pci_data *pci_priv, u64 *addr, u64 *size)
{
	if (!pci_priv)
		return -ENODEV;

	if (!pci_priv->smmu_iova_ipa_len)
		return -EINVAL;

	*addr = pci_priv->smmu_iova_ipa_start;
	*size = pci_priv->smmu_iova_ipa_len;

	return 0;
}

struct iommu_domain *cnss_smmu_get_domain(struct device *dev)
{
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(to_pci_dev(dev));

	if (!pci_priv)
		return NULL;

	return pci_priv->iommu_domain;
}
EXPORT_SYMBOL(cnss_smmu_get_domain);

int cnss_smmu_map(struct device *dev,
		  phys_addr_t paddr, uint32_t *iova_addr, size_t size)
{
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(to_pci_dev(dev));
	struct cnss_plat_data *plat_priv;
	unsigned long iova;
	size_t len;
	int ret = 0;
	int flag = IOMMU_READ | IOMMU_WRITE;
	struct pci_dev *root_port;
	struct device_node *root_of_node;
	bool dma_coherent = false;

	if (!pci_priv)
		return -ENODEV;

	if (!iova_addr) {
		cnss_pr_err("iova_addr is NULL, paddr %pa, size %zu\n",
			    &paddr, size);
		return -EINVAL;
	}

	plat_priv = pci_priv->plat_priv;

	len = roundup(size + paddr - rounddown(paddr, PAGE_SIZE), PAGE_SIZE);
	iova = roundup(pci_priv->smmu_iova_ipa_current, PAGE_SIZE);

	if (pci_priv->iommu_geometry &&
	    iova >= pci_priv->smmu_iova_ipa_start +
		    pci_priv->smmu_iova_ipa_len) {
		cnss_pr_err("No IOVA space to map, iova %lx, smmu_iova_ipa_start %pad, smmu_iova_ipa_len %zu\n",
			    iova,
			    &pci_priv->smmu_iova_ipa_start,
			    pci_priv->smmu_iova_ipa_len);
		return -ENOMEM;
	}

	if (!test_bit(DISABLE_IO_COHERENCY,
		      &plat_priv->ctrl_params.quirks)) {
		root_port = pci_find_pcie_root_port(pci_priv->pci_dev);
		if (!root_port) {
			cnss_pr_err("Root port is null, so dma_coherent is disabled\n");
		} else {
			root_of_node = root_port->dev.of_node;
			if (root_of_node && root_of_node->parent) {
				dma_coherent =
				    of_property_read_bool(root_of_node->parent,
							  "dma-coherent");
			cnss_pr_dbg("dma-coherent is %s\n",
					dma_coherent ? "enabled" : "disabled");
			if (dma_coherent)
				flag |= IOMMU_CACHE;
			}
		}
	}

	cnss_pr_dbg("IOMMU map: iova %lx, len %zu\n", iova, len);

	ret = iommu_map(pci_priv->iommu_domain, iova,
			rounddown(paddr, PAGE_SIZE), len, flag);
	if (ret) {
		cnss_pr_err("PA to IOVA mapping failed, ret %d\n", ret);
		return ret;
	}

	pci_priv->smmu_iova_ipa_current = iova + len;
	*iova_addr = (uint32_t)(iova + paddr - rounddown(paddr, PAGE_SIZE));
	cnss_pr_dbg("IOMMU map: iova_addr %lx\n", *iova_addr);

	return 0;
}
EXPORT_SYMBOL(cnss_smmu_map);

int cnss_smmu_unmap(struct device *dev, uint32_t iova_addr, size_t size)
{
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(to_pci_dev(dev));
	unsigned long iova;
	size_t unmapped;
	size_t len;

	if (!pci_priv)
		return -ENODEV;

	iova = rounddown(iova_addr, PAGE_SIZE);
	len = roundup(size + iova_addr - iova, PAGE_SIZE);

	if (iova >= pci_priv->smmu_iova_ipa_start +
		    pci_priv->smmu_iova_ipa_len) {
		cnss_pr_err("Out of IOVA space to unmap, iova %lx, smmu_iova_ipa_start %pad, smmu_iova_ipa_len %zu\n",
			    iova,
			    &pci_priv->smmu_iova_ipa_start,
			    pci_priv->smmu_iova_ipa_len);
		return -ENOMEM;
	}

	cnss_pr_dbg("IOMMU unmap: iova %lx, len %zu\n", iova, len);

	unmapped = iommu_unmap(pci_priv->iommu_domain, iova, len);
	if (unmapped != len) {
		cnss_pr_err("IOMMU unmap failed, unmapped = %zu, requested = %zu\n",
			    unmapped, len);
		return -EINVAL;
	}

	pci_priv->smmu_iova_ipa_current = iova;
	return 0;
}
EXPORT_SYMBOL(cnss_smmu_unmap);

int cnss_get_soc_info(struct device *dev, struct cnss_soc_info *info)
{
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(to_pci_dev(dev));
	struct cnss_plat_data *plat_priv;

	if (!pci_priv)
		return -ENODEV;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		return -ENODEV;

	info->va = pci_priv->bar;
	info->pa = pci_resource_start(pci_priv->pci_dev, PCI_BAR_NUM);
	info->chip_id = plat_priv->chip_info.chip_id;
	info->chip_family = plat_priv->chip_info.chip_family;
	info->board_id = plat_priv->board_info.board_id;
	info->soc_id = plat_priv->soc_info.soc_id;
	info->fw_version = plat_priv->fw_version_info.fw_version;
	strlcpy(info->fw_build_timestamp,
		plat_priv->fw_version_info.fw_build_timestamp,
		sizeof(info->fw_build_timestamp));
	memcpy(&info->device_version, &plat_priv->device_version,
	       sizeof(info->device_version));
	memcpy(&info->dev_mem_info, &plat_priv->dev_mem_info,
	       sizeof(info->dev_mem_info));

	return 0;
}
EXPORT_SYMBOL(cnss_get_soc_info);

static struct cnss_msi_config msi_config = {
	.total_vectors = 32,
	.total_users = 4,
	.users = (struct cnss_msi_user[]) {
		{ .name = "MHI", .num_vectors = 3, .base_vector = 0 },
		{ .name = "CE", .num_vectors = 10, .base_vector = 3 },
		{ .name = "WAKE", .num_vectors = 1, .base_vector = 13 },
		{ .name = "DP", .num_vectors = 18, .base_vector = 14 },
	},
};

static int cnss_pci_get_msi_assignment(struct cnss_pci_data *pci_priv)
{
	pci_priv->msi_config = &msi_config;

	return 0;
}

static int cnss_pci_enable_msi(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	int num_vectors;
	struct cnss_msi_config *msi_config;
	struct msi_desc *msi_desc;

	ret = cnss_pci_get_msi_assignment(pci_priv);
	if (ret) {
		cnss_pr_err("Failed to get MSI assignment, err = %d\n", ret);
		goto out;
	}

	msi_config = pci_priv->msi_config;
	if (!msi_config) {
		cnss_pr_err("msi_config is NULL!\n");
		ret = -EINVAL;
		goto out;
	}

	num_vectors = pci_alloc_irq_vectors(pci_dev,
					    msi_config->total_vectors,
					    msi_config->total_vectors,
					    PCI_IRQ_MSI);
	if (num_vectors != msi_config->total_vectors) {
		cnss_pr_err("Failed to get enough MSI vectors (%d), available vectors = %d",
			    msi_config->total_vectors, num_vectors);
		if (num_vectors >= 0)
			ret = -EINVAL;
		goto reset_msi_config;
	}

	msi_desc = irq_get_msi_desc(pci_dev->irq);
	if (!msi_desc) {
		cnss_pr_err("msi_desc is NULL!\n");
		ret = -EINVAL;
		goto free_msi_vector;
	}

	pci_priv->msi_ep_base_data = msi_desc->msg.data;
	if (!pci_priv->msi_ep_base_data) {
		cnss_pr_err("Got 0 MSI base data!\n");
		CNSS_ASSERT(0);
	}

	cnss_pr_dbg("MSI base data is %d\n", pci_priv->msi_ep_base_data);

	return 0;

free_msi_vector:
	pci_free_irq_vectors(pci_priv->pci_dev);
reset_msi_config:
	pci_priv->msi_config = NULL;
out:
	return ret;
}

static void cnss_pci_disable_msi(struct cnss_pci_data *pci_priv)
{
	pci_free_irq_vectors(pci_priv->pci_dev);
}

int cnss_get_user_msi_assignment(struct device *dev, char *user_name,
				 int *num_vectors, u32 *user_base_data,
				 u32 *base_vector)
{
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(to_pci_dev(dev));
	struct cnss_msi_config *msi_config;
	int idx;

	if (!pci_priv)
		return -ENODEV;

	msi_config = pci_priv->msi_config;
	if (!msi_config) {
		cnss_pr_err("MSI is not supported.\n");
		return -EINVAL;
	}

	for (idx = 0; idx < msi_config->total_users; idx++) {
		if (strcmp(user_name, msi_config->users[idx].name) == 0) {
			*num_vectors = msi_config->users[idx].num_vectors;
			*user_base_data = msi_config->users[idx].base_vector
				+ pci_priv->msi_ep_base_data;
			*base_vector = msi_config->users[idx].base_vector;

			cnss_pr_dbg("Assign MSI to user: %s, num_vectors: %d, user_base_data: %u, base_vector: %u\n",
				    user_name, *num_vectors, *user_base_data,
				    *base_vector);

			return 0;
		}
	}

	cnss_pr_err("Failed to find MSI assignment for %s!\n", user_name);

	return -EINVAL;
}
EXPORT_SYMBOL(cnss_get_user_msi_assignment);

int cnss_get_msi_irq(struct device *dev, unsigned int vector)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	int irq_num;

	irq_num = pci_irq_vector(pci_dev, vector);
	cnss_pr_dbg("Get IRQ number %d for vector index %d\n", irq_num, vector);

	return irq_num;
}
EXPORT_SYMBOL(cnss_get_msi_irq);

void cnss_get_msi_address(struct device *dev, u32 *msi_addr_low,
			  u32 *msi_addr_high)
{
	struct pci_dev *pci_dev = to_pci_dev(dev);
	u16 control;

	pci_read_config_word(pci_dev, pci_dev->msi_cap + PCI_MSI_FLAGS,
			     &control);
	pci_read_config_dword(pci_dev, pci_dev->msi_cap + PCI_MSI_ADDRESS_LO,
			      msi_addr_low);
	/*return msi high addr only when device support 64 BIT MSI */
	if (control & PCI_MSI_FLAGS_64BIT)
		pci_read_config_dword(pci_dev,
				      pci_dev->msi_cap + PCI_MSI_ADDRESS_HI,
				      msi_addr_high);
	else
		*msi_addr_high = 0;
}
EXPORT_SYMBOL(cnss_get_msi_address);

u32 cnss_pci_get_wake_msi(struct cnss_pci_data *pci_priv)
{
	int ret, num_vectors;
	u32 user_base_data, base_vector;

	if (!pci_priv)
		return -ENODEV;

	ret = cnss_get_user_msi_assignment(&pci_priv->pci_dev->dev,
					   WAKE_MSI_NAME, &num_vectors,
					   &user_base_data, &base_vector);
	if (ret) {
		cnss_pr_err("WAKE MSI is not valid\n");
		return 0;
	}

	return user_base_data;
}

static int cnss_pci_enable_bus(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	u16 device_id;
	u32 pci_dma_mask = PCI_DMA_MASK_64_BIT;

	pci_read_config_word(pci_dev, PCI_DEVICE_ID, &device_id);
	if (device_id != pci_priv->pci_device_id->device)  {
		cnss_pr_err("PCI device ID mismatch, config ID: 0x%x, probe ID: 0x%x\n",
			    device_id, pci_priv->pci_device_id->device);
		ret = -EIO;
		goto out;
	}

	ret = pci_assign_resource(pci_dev, PCI_BAR_NUM);
	if (ret) {
		pr_err("Failed to assign PCI resource, err = %d\n", ret);
		goto out;
	}

	ret = pci_enable_device(pci_dev);
	if (ret) {
		cnss_pr_err("Failed to enable PCI device, err = %d\n", ret);
		goto out;
	}

	ret = pci_request_region(pci_dev, PCI_BAR_NUM, "cnss");
	if (ret) {
		cnss_pr_err("Failed to request PCI region, err = %d\n", ret);
		goto disable_device;
	}

	if (device_id == QCA6174_DEVICE_ID)
		pci_dma_mask = PCI_DMA_MASK_32_BIT;

	ret = pci_set_dma_mask(pci_dev, DMA_BIT_MASK(pci_dma_mask));
	if (ret) {
		cnss_pr_err("Failed to set PCI DMA mask (%d), err = %d\n",
			    ret, pci_dma_mask);
		goto release_region;
	}

	ret = pci_set_consistent_dma_mask(pci_dev, DMA_BIT_MASK(pci_dma_mask));
	if (ret) {
		cnss_pr_err("Failed to set PCI consistent DMA mask (%d), err = %d\n",
			    ret, pci_dma_mask);
		goto release_region;
	}

	pci_priv->bar = pci_iomap(pci_dev, PCI_BAR_NUM, 0);
	if (!pci_priv->bar) {
		cnss_pr_err("Failed to do PCI IO map!\n");
		ret = -EIO;
		goto release_region;
	}

	/* Save default config space without BME enabled */
	pci_save_state(pci_dev);
	pci_priv->default_state = pci_store_saved_state(pci_dev);

	pci_set_master(pci_dev);

	return 0;

release_region:
	pci_release_region(pci_dev, PCI_BAR_NUM);
disable_device:
	pci_disable_device(pci_dev);
out:
	return ret;
}

static void cnss_pci_disable_bus(struct cnss_pci_data *pci_priv)
{
	struct pci_dev *pci_dev = pci_priv->pci_dev;

	pci_clear_master(pci_dev);
	pci_load_and_free_saved_state(pci_dev, &pci_priv->saved_state);

	if (pci_priv->bar) {
		pci_iounmap(pci_dev, pci_priv->bar);
		pci_priv->bar = NULL;
	}

	pci_release_region(pci_dev, PCI_BAR_NUM);
	if (pci_is_enabled(pci_dev))
		pci_disable_device(pci_dev);
}

static void cnss_pci_dump_qdss_reg(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	int i, array_size = ARRAY_SIZE(qdss_csr) - 1;
	gfp_t gfp = GFP_KERNEL;
	u32 reg_offset;

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	if (!plat_priv->qdss_reg) {
		plat_priv->qdss_reg = devm_kzalloc(&pci_priv->pci_dev->dev,
						   sizeof(*plat_priv->qdss_reg)
						   * array_size, gfp);
		if (!plat_priv->qdss_reg)
			return;
	}

	for (i = 0; qdss_csr[i].name; i++) {
		reg_offset = QDSS_APB_DEC_CSR_BASE + qdss_csr[i].offset;
		if (cnss_pci_reg_read(pci_priv, reg_offset,
				      &plat_priv->qdss_reg[i]))
			return;
		cnss_pr_dbg("%s[0x%x] = 0x%x\n", qdss_csr[i].name, reg_offset,
			    plat_priv->qdss_reg[i]);
	}
}

static void cnss_pci_dump_ce_reg(struct cnss_pci_data *pci_priv,
				 enum cnss_ce_index ce)
{
	int i;
	u32 ce_base = ce * QCA6390_CE_REG_INTERVAL;
	u32 reg_offset, val;

	switch (ce) {
	case CNSS_CE_09:
	case CNSS_CE_10:
		for (i = 0; ce_src[i].name; i++) {
			reg_offset = QCA6390_CE_SRC_RING_REG_BASE +
				ce_base + ce_src[i].offset;
			if (cnss_pci_reg_read(pci_priv, reg_offset, &val))
				return;
			cnss_pr_dbg("CE_%02d_%s[0x%x] = 0x%x\n",
				    ce, ce_src[i].name, reg_offset, val);
		}

		for (i = 0; ce_dst[i].name; i++) {
			reg_offset = QCA6390_CE_DST_RING_REG_BASE +
				ce_base + ce_dst[i].offset;
			if (cnss_pci_reg_read(pci_priv, reg_offset, &val))
				return;
			cnss_pr_dbg("CE_%02d_%s[0x%x] = 0x%x\n",
				    ce, ce_dst[i].name, reg_offset, val);
		}
		break;
	case CNSS_CE_COMMON:
		for (i = 0; ce_cmn[i].name; i++) {
			reg_offset = QCA6390_CE_COMMON_REG_BASE +
				ce_cmn[i].offset;
			if (cnss_pci_reg_read(pci_priv, reg_offset, &val))
				return;
			cnss_pr_dbg("CE_COMMON_%s[0x%x] = 0x%x\n",
				    ce_cmn[i].name, reg_offset, val);
		}
		break;
	default:
		cnss_pr_err("Unsupported CE[%d] registers dump\n", ce);
	}
}

static void cnss_pci_dump_registers(struct cnss_pci_data *pci_priv)
{
	cnss_pr_dbg("Start to dump debug registers\n");

	if (cnss_pci_check_link_status(pci_priv))
		return;

	mhi_debug_reg_dump(pci_priv->mhi_ctrl);
	cnss_pci_dump_ce_reg(pci_priv, CNSS_CE_COMMON);
	cnss_pci_dump_ce_reg(pci_priv, CNSS_CE_09);
	cnss_pci_dump_ce_reg(pci_priv, CNSS_CE_10);
}

int cnss_pci_force_fw_assert_hdlr(struct cnss_pci_data *pci_priv)
{
	int ret;
	struct cnss_plat_data *plat_priv;

	if (!pci_priv)
		return -ENODEV;

	plat_priv = pci_priv->plat_priv;
	if (!plat_priv)
		return -ENODEV;

	if (!test_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state) ||
	    test_bit(CNSS_MHI_POWERING_OFF, &pci_priv->mhi_state))
		return -EINVAL;

	cnss_auto_resume(&pci_priv->pci_dev->dev);
	/* If link is still down here, directly trigger link down recovery */
	ret = cnss_pci_check_link_status(pci_priv);
	if (ret) {
		cnss_pci_link_down(&pci_priv->pci_dev->dev);
		return 0;
	}

	cnss_pci_dump_misc_reg(pci_priv);
	cnss_pci_dump_shadow_reg(pci_priv);

	ret = cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_TRIGGER_RDDM);
	if (ret) {
		if (!test_bit(CNSS_MHI_POWER_ON, &pci_priv->mhi_state) ||
		    test_bit(CNSS_MHI_POWERING_OFF, &pci_priv->mhi_state)) {
			cnss_pr_dbg("MHI is not powered on, ignore RDDM failure\n");
			return 0;
		}
		cnss_fatal_err("Failed to trigger RDDM, err = %d\n", ret);
		cnss_pci_dump_registers(pci_priv);
		cnss_schedule_recovery(&pci_priv->pci_dev->dev,
				       CNSS_REASON_DEFAULT);
		return ret;
	}

	if (!test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state)) {
		mod_timer(&pci_priv->dev_rddm_timer,
			  jiffies + msecs_to_jiffies(DEV_RDDM_TIMEOUT));
	}

	return 0;
}

static void cnss_pci_add_dump_seg(struct cnss_pci_data *pci_priv,
				  struct cnss_dump_seg *dump_seg,
				  enum cnss_fw_dump_type type, int seg_no,
				  void *va, dma_addr_t dma, size_t size)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct device *dev = &pci_priv->pci_dev->dev;
	phys_addr_t pa;

	dump_seg->address = dma;
	dump_seg->v_address = va;
	dump_seg->size = size;
	dump_seg->type = type;

	cnss_pr_dbg("Seg: %x, va: %pK, dma: %pa, size: 0x%zx\n",
		    seg_no, va, &dma, size);

	if (cnss_va_to_pa(dev, size, va, dma, &pa, DMA_ATTR_FORCE_CONTIGUOUS))
		return;

	cnss_minidump_add_region(plat_priv, type, seg_no, va, pa, size);
}

static void cnss_pci_remove_dump_seg(struct cnss_pci_data *pci_priv,
				     struct cnss_dump_seg *dump_seg,
				     enum cnss_fw_dump_type type, int seg_no,
				     void *va, dma_addr_t dma, size_t size)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct device *dev = &pci_priv->pci_dev->dev;
	phys_addr_t pa;

	cnss_va_to_pa(dev, size, va, dma, &pa, DMA_ATTR_FORCE_CONTIGUOUS);
	cnss_minidump_remove_region(plat_priv, type, seg_no, va, pa, size);
}

int cnss_pci_call_driver_uevent(struct cnss_pci_data *pci_priv,
				enum cnss_driver_status status, void *data)
{
	struct cnss_uevent_data uevent_data;
	struct cnss_wlan_driver *driver_ops;

	driver_ops = pci_priv->driver_ops;
	if (!driver_ops || !driver_ops->update_event) {
		cnss_pr_dbg("Hang event driver ops is NULL\n");
		return -EINVAL;
	}

	cnss_pr_dbg("Calling driver uevent: %d\n", status);

	uevent_data.status = status;
	uevent_data.data = data;

	return driver_ops->update_event(pci_priv->pci_dev, &uevent_data);
}

static void cnss_pci_send_hang_event(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_fw_mem *fw_mem = plat_priv->fw_mem;
	struct cnss_hang_event hang_event = {0};
	void *hang_data_va = NULL;
	u64 offset = 0;
	int i = 0;

	if (!fw_mem || !plat_priv->fw_mem_seg_len)
		return;

	switch (pci_priv->device_id) {
	case QCA6390_DEVICE_ID:
		offset = HST_HANG_DATA_OFFSET;
		break;
	case QCA6490_DEVICE_ID:
		offset = HSP_HANG_DATA_OFFSET;
		break;
	default:
		cnss_pr_err("Skip Hang Event Data as unsupported Device ID received: %d\n",
			    pci_priv->device_id);
		return;
	}

	for (i = 0; i < plat_priv->fw_mem_seg_len; i++) {
		if (fw_mem[i].type == QMI_WLFW_MEM_TYPE_DDR_V01 &&
		    fw_mem[i].va) {
			hang_data_va = fw_mem[i].va + offset;
			hang_event.hang_event_data = kmemdup(hang_data_va,
							     HANG_DATA_LENGTH,
							     GFP_ATOMIC);
			if (!hang_event.hang_event_data) {
				cnss_pr_dbg("Hang data memory alloc failed\n");
				return;
			}
			hang_event.hang_event_data_len = HANG_DATA_LENGTH;
			break;
		}
	}

	cnss_pci_call_driver_uevent(pci_priv, CNSS_HANG_EVENT, &hang_event);

	kfree(hang_event.hang_event_data);
	hang_event.hang_event_data = NULL;
}

void cnss_pci_collect_dump_info(struct cnss_pci_data *pci_priv, bool in_panic)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_dump_data *dump_data =
		&plat_priv->ramdump_info_v2.dump_data;
	struct cnss_dump_seg *dump_seg =
		plat_priv->ramdump_info_v2.dump_data_vaddr;
	struct image_info *fw_image, *rddm_image;
	struct cnss_fw_mem *fw_mem = plat_priv->fw_mem;
	int ret, i, j;

	if (test_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state))
		cnss_pci_send_hang_event(pci_priv);

	if (test_bit(CNSS_MHI_RDDM_DONE, &pci_priv->mhi_state)) {
		cnss_pr_dbg("RAM dump is already collected, skip\n");
		return;
	}

	if (cnss_pci_check_link_status(pci_priv))
		return;

	cnss_pci_dump_misc_reg(pci_priv);
	cnss_pci_dump_shadow_reg(pci_priv);
	cnss_pci_dump_qdss_reg(pci_priv);
	cnss_pci_dump_bl_sram_mem(pci_priv);

	ret = mhi_download_rddm_img(pci_priv->mhi_ctrl, in_panic);
	if (ret) {
		cnss_fatal_err("Failed to download RDDM image, err = %d\n",
			       ret);
		cnss_pci_dump_registers(pci_priv);
		return;
	}

	fw_image = pci_priv->mhi_ctrl->fbc_image;
	rddm_image = pci_priv->mhi_ctrl->rddm_image;
	dump_data->nentries = 0;

	cnss_pr_dbg("Collect FW image dump segment, nentries %d\n",
		    fw_image->entries);

	for (i = 0; i < fw_image->entries; i++) {
		cnss_pci_add_dump_seg(pci_priv, dump_seg, CNSS_FW_IMAGE, i,
				      fw_image->mhi_buf[i].buf,
				      fw_image->mhi_buf[i].dma_addr,
				      fw_image->mhi_buf[i].len);
		dump_seg++;
	}

	dump_data->nentries += fw_image->entries;

	cnss_pr_dbg("Collect RDDM image dump segment, nentries %d\n",
		    rddm_image->entries);

	for (i = 0; i < rddm_image->entries; i++) {
		cnss_pci_add_dump_seg(pci_priv, dump_seg, CNSS_FW_RDDM, i,
				      rddm_image->mhi_buf[i].buf,
				      rddm_image->mhi_buf[i].dma_addr,
				      rddm_image->mhi_buf[i].len);
		dump_seg++;
	}

	dump_data->nentries += rddm_image->entries;

	mhi_dump_sfr(pci_priv->mhi_ctrl);

	cnss_pr_dbg("Collect remote heap dump segment\n");

	for (i = 0, j = 0; i < plat_priv->fw_mem_seg_len; i++) {
		if (fw_mem[i].type == CNSS_MEM_TYPE_DDR) {
			cnss_pci_add_dump_seg(pci_priv, dump_seg,
					      CNSS_FW_REMOTE_HEAP, j,
					      fw_mem[i].va, fw_mem[i].pa,
					      fw_mem[i].size);
			dump_seg++;
			dump_data->nentries++;
			j++;
		}
	}

	if (dump_data->nentries > 0)
		plat_priv->ramdump_info_v2.dump_data_valid = true;

	cnss_pci_set_mhi_state(pci_priv, CNSS_MHI_RDDM_DONE);
	complete(&plat_priv->rddm_complete);
}

void cnss_pci_clear_dump_info(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct cnss_dump_seg *dump_seg =
		plat_priv->ramdump_info_v2.dump_data_vaddr;
	struct image_info *fw_image, *rddm_image;
	struct cnss_fw_mem *fw_mem = plat_priv->fw_mem;
	int i, j;

	fw_image = pci_priv->mhi_ctrl->fbc_image;
	rddm_image = pci_priv->mhi_ctrl->rddm_image;

	for (i = 0; i < fw_image->entries; i++) {
		cnss_pci_remove_dump_seg(pci_priv, dump_seg, CNSS_FW_IMAGE, i,
					 fw_image->mhi_buf[i].buf,
					 fw_image->mhi_buf[i].dma_addr,
					 fw_image->mhi_buf[i].len);
		dump_seg++;
	}

	for (i = 0; i < rddm_image->entries; i++) {
		cnss_pci_remove_dump_seg(pci_priv, dump_seg, CNSS_FW_RDDM, i,
					 rddm_image->mhi_buf[i].buf,
					 rddm_image->mhi_buf[i].dma_addr,
					 rddm_image->mhi_buf[i].len);
		dump_seg++;
	}

	for (i = 0, j = 0; i < plat_priv->fw_mem_seg_len; i++) {
		if (fw_mem[i].type == CNSS_MEM_TYPE_DDR) {
			cnss_pci_remove_dump_seg(pci_priv, dump_seg,
						 CNSS_FW_REMOTE_HEAP, j,
						 fw_mem[i].va, fw_mem[i].pa,
						 fw_mem[i].size);
			dump_seg++;
			j++;
		}
	}

	plat_priv->ramdump_info_v2.dump_data.nentries = 0;
	plat_priv->ramdump_info_v2.dump_data_valid = false;
}

void cnss_pci_device_crashed(struct cnss_pci_data *pci_priv)
{
	if (!pci_priv)
		return;

	cnss_device_crashed(&pci_priv->pci_dev->dev);
}

static int cnss_mhi_pm_runtime_get(struct mhi_controller *mhi_ctrl, void *priv)
{
	struct cnss_pci_data *pci_priv = priv;

	return cnss_pci_pm_runtime_get(pci_priv, RTPM_ID_MHI);
}

static void cnss_mhi_pm_runtime_put_noidle(struct mhi_controller *mhi_ctrl,
					   void *priv)
{
	struct cnss_pci_data *pci_priv = priv;

	cnss_pci_pm_runtime_put_noidle(pci_priv, RTPM_ID_MHI);
}

void cnss_pci_add_fw_prefix_name(struct cnss_pci_data *pci_priv,
				 char *prefix_name, char *name)
{
	struct cnss_plat_data *plat_priv;

	if (!pci_priv)
		return;

	plat_priv = pci_priv->plat_priv;

	if (!plat_priv->use_fw_path_with_prefix) {
		scnprintf(prefix_name, MAX_FIRMWARE_NAME_LEN, "%s", name);
		return;
	}

	switch (pci_priv->device_id) {
	case QCA6390_DEVICE_ID:
		scnprintf(prefix_name, MAX_FIRMWARE_NAME_LEN,
			  QCA6390_PATH_PREFIX "%s", name);
		break;
	case QCA6490_DEVICE_ID:
		scnprintf(prefix_name, MAX_FIRMWARE_NAME_LEN,
			  QCA6490_PATH_PREFIX "%s", name);
		break;
	default:
		scnprintf(prefix_name, MAX_FIRMWARE_NAME_LEN, "%s", name);
		break;
	}

	cnss_pr_dbg("FW name added with prefix: %s\n", prefix_name);
}

static int cnss_pci_update_fw_name(struct cnss_pci_data *pci_priv)
{
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct mhi_controller *mhi_ctrl = pci_priv->mhi_ctrl;

	plat_priv->device_version.family_number = mhi_ctrl->family_number;
	plat_priv->device_version.device_number = mhi_ctrl->device_number;
	plat_priv->device_version.major_version = mhi_ctrl->major_version;
	plat_priv->device_version.minor_version = mhi_ctrl->minor_version;

	cnss_pr_dbg("Get device version info, family number: 0x%x, device number: 0x%x, major version: 0x%x, minor version: 0x%x\n",
		    plat_priv->device_version.family_number,
		    plat_priv->device_version.device_number,
		    plat_priv->device_version.major_version,
		    plat_priv->device_version.minor_version);

	switch (pci_priv->device_id) {
	case QCA6390_DEVICE_ID:
		if (plat_priv->device_version.major_version < FW_V2_NUMBER) {
			cnss_pr_dbg("Device ID:version (0x%lx:%d) is not supported\n",
				    pci_priv->device_id,
				    plat_priv->device_version.major_version);
			return -EINVAL;
		}
		cnss_pci_add_fw_prefix_name(pci_priv, plat_priv->firmware_name,
					    FW_V2_FILE_NAME);
		snprintf(plat_priv->fw_fallback_name, MAX_FIRMWARE_NAME_LEN,
			 FW_V2_FILE_NAME);
		break;
	case QCA6490_DEVICE_ID:
		switch (plat_priv->device_version.major_version) {
		case FW_V2_NUMBER:
			cnss_pci_add_fw_prefix_name(pci_priv,
						    plat_priv->firmware_name,
						    FW_V2_FILE_NAME);
			snprintf(plat_priv->fw_fallback_name,
				 MAX_FIRMWARE_NAME_LEN, FW_V2_FILE_NAME);
			break;
		default:
			cnss_pci_add_fw_prefix_name(pci_priv,
						    plat_priv->firmware_name,
						    DEFAULT_FW_FILE_NAME);
			snprintf(plat_priv->fw_fallback_name,
				 MAX_FIRMWARE_NAME_LEN, DEFAULT_FW_FILE_NAME);
			break;
		}
		break;
	default:
		cnss_pci_add_fw_prefix_name(pci_priv, plat_priv->firmware_name,
					    DEFAULT_FW_FILE_NAME);
		snprintf(plat_priv->fw_fallback_name, MAX_FIRMWARE_NAME_LEN,
			 DEFAULT_FW_FILE_NAME);
		break;
	}

	cnss_pr_dbg("FW name is %s, FW fallback name is %s\n",
		    mhi_ctrl->fw_image, mhi_ctrl->fw_image_fallback);

	return 0;
}

static char *cnss_mhi_notify_status_to_str(enum MHI_CB status)
{
	switch (status) {
	case MHI_CB_IDLE:
		return "IDLE";
	case MHI_CB_EE_RDDM:
		return "RDDM";
	case MHI_CB_SYS_ERROR:
		return "SYS_ERROR";
	case MHI_CB_FATAL_ERROR:
		return "FATAL_ERROR";
	case MHI_CB_EE_MISSION_MODE:
		return "MISSION_MODE";
	case MHI_CB_FW_FALLBACK_IMG:
		return "FW_FALLBACK";
	default:
		return "UNKNOWN";
	}
};

static void cnss_dev_rddm_timeout_hdlr(struct timer_list *t)
{
	struct cnss_pci_data *pci_priv =
		from_timer(pci_priv, t, dev_rddm_timer);
	struct mhi_controller *mhi_ctrl;

	if (!pci_priv)
		return;

	mhi_ctrl = pci_priv->mhi_ctrl;
	cnss_fatal_err("Timeout waiting for RDDM notification\n");

	if (mhi_get_exec_env(mhi_ctrl) == MHI_EE_PBL)
		cnss_pr_err("Unable to collect ramdumps due to abrupt reset\n");

	mhi_debug_reg_dump(mhi_ctrl);

	cnss_schedule_recovery(&pci_priv->pci_dev->dev, CNSS_REASON_TIMEOUT);
}

static int cnss_mhi_link_status(struct mhi_controller *mhi_ctrl, void *priv)
{
	struct cnss_pci_data *pci_priv = priv;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL\n");
		return -EINVAL;
	}

	return cnss_pci_check_link_status(pci_priv);
}

static void cnss_mhi_notify_status(struct mhi_controller *mhi_ctrl, void *priv,
				   enum MHI_CB reason)
{
	struct cnss_pci_data *pci_priv = priv;
	struct cnss_plat_data *plat_priv;
	enum cnss_recovery_reason cnss_reason;

	if (!pci_priv) {
		cnss_pr_err("pci_priv is NULL");
		return;
	}

	plat_priv = pci_priv->plat_priv;

	if (reason != MHI_CB_IDLE)
		cnss_pr_dbg("MHI status cb is called with reason %s(%d)\n",
			    cnss_mhi_notify_status_to_str(reason), reason);

	switch (reason) {
	case MHI_CB_IDLE:
	case MHI_CB_EE_MISSION_MODE:
		return;
	case MHI_CB_FATAL_ERROR:
		cnss_ignore_qmi_failure(true);
		set_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state);
		del_timer(&plat_priv->fw_boot_timer);
		cnss_pci_update_status(pci_priv, CNSS_FW_DOWN);
		cnss_reason = CNSS_REASON_DEFAULT;
		break;
	case MHI_CB_SYS_ERROR:
		cnss_ignore_qmi_failure(true);
		set_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state);
		del_timer(&plat_priv->fw_boot_timer);
		mod_timer(&pci_priv->dev_rddm_timer,
			  jiffies + msecs_to_jiffies(DEV_RDDM_TIMEOUT));
		cnss_pci_update_status(pci_priv, CNSS_FW_DOWN);
		return;
	case MHI_CB_EE_RDDM:
		cnss_ignore_qmi_failure(true);
		set_bit(CNSS_DEV_ERR_NOTIFY, &plat_priv->driver_state);
		del_timer(&plat_priv->fw_boot_timer);
		del_timer(&pci_priv->dev_rddm_timer);
		cnss_pci_update_status(pci_priv, CNSS_FW_DOWN);
		cnss_reason = CNSS_REASON_RDDM;
		break;
	case MHI_CB_FW_FALLBACK_IMG:
		plat_priv->use_fw_path_with_prefix = false;
		cnss_pci_update_fw_name(pci_priv);
		return;
	case MHI_CB_BOOTUP_TIMEOUT:
		if (plat_priv->device_id == QCA6490_DEVICE_ID ||
		    plat_priv->device_id == QCA6390_DEVICE_ID) {
			cnss_pci_dump_bl_sram_mem(pci_priv);
			cnss_pci_dump_mhi_reg(pci_priv);
		}
		cnss_reason = CNSS_REASON_TIMEOUT;
		break;
	default:
		cnss_pr_err("Unsupported MHI status cb reason: %d\n", reason);
		return;
	}

	cnss_schedule_recovery(&pci_priv->pci_dev->dev, cnss_reason);
}

static int cnss_pci_get_mhi_msi(struct cnss_pci_data *pci_priv)
{
	int ret, num_vectors, i;
	u32 user_base_data, base_vector;
	int *irq;

	ret = cnss_get_user_msi_assignment(&pci_priv->pci_dev->dev,
					   MHI_MSI_NAME, &num_vectors,
					   &user_base_data, &base_vector);
	if (ret)
		return ret;

	cnss_pr_dbg("Number of assigned MSI for MHI is %d, base vector is %d\n",
		    num_vectors, base_vector);

	irq = kcalloc(num_vectors, sizeof(int), GFP_KERNEL);
	if (!irq)
		return -ENOMEM;

	for (i = 0; i < num_vectors; i++)
		irq[i] = cnss_get_msi_irq(&pci_priv->pci_dev->dev,
					  base_vector + i);

	pci_priv->mhi_ctrl->irq = irq;
	pci_priv->mhi_ctrl->msi_allocated = num_vectors;

	return 0;
}

static int cnss_mhi_bw_scale(struct mhi_controller *mhi_ctrl,
			     struct mhi_link_info *link_info)
{
	struct cnss_pci_data *pci_priv = mhi_ctrl->priv_data;
	int ret = 0;

	ret = msm_pcie_set_link_bandwidth(pci_priv->pci_dev,
					  link_info->target_link_speed,
					  link_info->target_link_width);

	if (ret)
		return ret;

	pci_priv->def_link_speed = link_info->target_link_speed;
	pci_priv->def_link_width = link_info->target_link_width;

	cnss_pr_dbg("Setting link speed:0x%x, width:0x%x\n",
		    link_info->target_link_speed,
		    link_info->target_link_width);

	return 0;
}

static int cnss_pci_register_mhi(struct cnss_pci_data *pci_priv)
{
	int ret = 0;
	struct cnss_plat_data *plat_priv = pci_priv->plat_priv;
	struct pci_dev *pci_dev = pci_priv->pci_dev;
	struct mhi_controller *mhi_ctrl;

	mhi_ctrl = mhi_alloc_controller(0);
	if (!mhi_ctrl) {
		cnss_pr_err("Invalid MHI controller context\n");
		return -EINVAL;
	}

	pci_priv->mhi_ctrl = mhi_ctrl;

	mhi_ctrl->priv_data = pci_priv;
	mhi_ctrl->dev = &pci_dev->dev;
	mhi_ctrl->of_node = (&plat_priv->plat_dev->dev)->of_node;
	mhi_ctrl->dev_id = pci_priv->device_id;
	mhi_ctrl->domain = pci_domain_nr(pci_dev->bus);
	mhi_ctrl->bus = pci_dev->bus->number;
	mhi_ctrl->slot = PCI_SLOT(pci_dev->devfn);

	mhi_ctrl->fw_image = plat_priv->firmware_name;
	mhi_ctrl->fw_image_fallback = plat_priv->fw_fallback_name;

	mhi_ctrl->regs = pci_priv->bar;
	mhi_ctrl->len = pci_resource_len(pci_priv->pci_dev, PCI_BAR_NUM);
	cnss_pr_dbg("BAR starts at %pa, len-%x\n",
		    &pci_resource_start(pci_priv->pci_dev, PCI_BAR_NUM),
		    mhi_ctrl->len);

	ret = cnss_pci_get_mhi_msi(pci_priv);
	if (ret) {
		cnss_pr_err("Failed to get MSI for MHI, err = %d\n", ret);
		goto free_mhi_ctrl;
	}

	if (pci_priv->smmu_s1_enable) {
		mhi_ctrl->iova_start = pci_priv->smmu_iova_start;
		mhi_ctrl->iova_stop = pci_priv->smmu_iova_start +
					pci_priv->smmu_iova_len;
	} else {
		mhi_ctrl->iova_start = memblock_start_of_DRAM();
		mhi_ctrl->iova_stop = memblock_end_of_DRAM();
	}

	mhi_ctrl->link_status = cnss_mhi_link_status;
	mhi_ctrl->status_cb = cnss_mhi_notify_status;
	mhi_ctrl->runtime_get = cnss_mhi_pm_runtime_get;
	mhi_ctrl->runtime_put = cnss_mhi_pm_runtime_put_noidle;
	mhi_ctrl->bw_scale = cnss_mhi_bw_scale;

	mhi_ctrl->rddm_size = pci_priv->plat_priv->ramdump_info_v2.ramdump_size;
	mhi_ctrl->sbl_size = SZ_512K;
	mhi_ctrl->seg_len = SZ_512K;
	mhi_ctrl->fbc_download = true;
	mhi_ctrl->rddm_supported = true;

	mhi_ctrl->log_buf = ipc_log_context_create(CNSS_IPC_LOG_PAGES,
						   "cnss-mhi", 0);
	if (!mhi_ctrl->log_buf)
		cnss_pr_err("Unable to create CNSS MHI IPC log context\n");

	mhi_ctrl->cntrl_log_buf = ipc_log_context_create(CNSS_IPC_LOG_PAGES,
							 "cnss-mhi-cntrl", 0);
	if (!mhi_ctrl->cntrl_log_buf)
		cnss_pr_err("Unable to create CNSS MHICNTRL IPC log context\n");

	ret = of_register_mhi_controller(mhi_ctrl);
	if (ret) {
		cnss_pr_err("Failed to register to MHI bus, err = %d\n", ret);
		goto destroy_ipc;
	}

	ret = cnss_pci_update_fw_name(pci_priv);
	if (ret)
		goto unreg_mhi;

	return 0;

unreg_mhi:
	mhi_unregister_mhi_controller(mhi_ctrl);
destroy_ipc:
	if (mhi_ctrl->log_buf)
		ipc_log_context_destroy(mhi_ctrl->log_buf);
	if (mhi_ctrl->cntrl_log_buf)
		ipc_log_context_destroy(mhi_ctrl->cntrl_log_buf);
	kfree(mhi_ctrl->irq);
free_mhi_ctrl:
	mhi_free_controller(mhi_ctrl);

	return ret;
}

static void cnss_pci_unregister_mhi(struct cnss_pci_data *pci_priv)
{
	struct mhi_controller *mhi_ctrl = pci_priv->mhi_ctrl;

	mhi_unregister_mhi_controller(mhi_ctrl);
	if (mhi_ctrl->log_buf)
		ipc_log_context_destroy(mhi_ctrl->log_buf);
	if (mhi_ctrl->cntrl_log_buf)
		ipc_log_context_destroy(mhi_ctrl->cntrl_log_buf);
	kfree(mhi_ctrl->irq);
	mhi_free_controller(mhi_ctrl);
}

static void cnss_pci_config_regs(struct cnss_pci_data *pci_priv)
{
	switch (pci_priv->device_id) {
	case QCA6390_DEVICE_ID:
		pci_priv->wcss_reg = wcss_reg_access_seq;
		pci_priv->wcss_reg_size = WCSS_REG_SIZE;
		pci_priv->pcie_reg = pcie_reg_access_seq;
		pci_priv->pcie_reg_size = PCIE_REG_SIZE;
		pci_priv->wlaon_reg = wlaon_reg_access_seq;
		pci_priv->wlaon_reg_size = WLAON_REG_SIZE;

		/* Configure WDOG register with specific value so that we can
		 * know if HW is in the process of WDOG reset recovery or not
		 * when reading the registers.
		 */
		cnss_pci_reg_write
		(pci_priv,
		QCA6390_PCIE_SOC_WDOG_DISC_BAD_DATA_LOW_CFG_SOC_PCIE_REG,
		QCA6390_PCIE_SOC_WDOG_DISC_BAD_DATA_LOW_CFG_SOC_PCIE_REG_VAL);
		break;
	default:
		return;
	}
}

static int cnss_pci_probe(struct pci_dev *pci_dev,
			  const struct pci_device_id *id)
{
	int ret = 0;
	struct cnss_pci_data *pci_priv;
	struct cnss_plat_data *plat_priv = cnss_bus_dev_to_plat_priv(NULL);
	struct device *dev = &pci_dev->dev;

	cnss_pr_dbg("PCI is probing, vendor ID: 0x%x, device ID: 0x%x\n",
		    id->vendor, pci_dev->device);

	pci_priv = devm_kzalloc(dev, sizeof(*pci_priv), GFP_KERNEL);
	if (!pci_priv) {
		ret = -ENOMEM;
		goto out;
	}

	pci_dev->no_d3hot = true;
	pci_priv->pci_link_state = PCI_LINK_UP;
	pci_priv->plat_priv = plat_priv;
	pci_priv->pci_dev = pci_dev;
	pci_priv->pci_device_id = id;
	pci_priv->device_id = pci_dev->device;
	cnss_set_pci_priv(pci_dev, pci_priv);
	plat_priv->device_id = pci_dev->device;
	plat_priv->bus_priv = pci_priv;
	mutex_init(&pci_priv->bus_lock);

	ret = of_reserved_mem_device_init(dev);
	if (ret)
		cnss_pr_err("Failed to init reserved mem device, err = %d\n",
			    ret);
	if (dev->cma_area)
		cnss_pr_dbg("CMA area is %s\n", cma_get_name(dev->cma_area));

	ret = cnss_register_subsys(plat_priv);
	if (ret)
		goto reset_ctx;

	ret = cnss_register_ramdump(plat_priv);
	if (ret)
		goto unregister_subsys;

	ret = cnss_pci_init_smmu(pci_priv);
	if (ret)
		goto unregister_ramdump;

	ret = cnss_reg_pci_event(pci_priv);
	if (ret) {
		cnss_pr_err("Failed to register PCI event, err = %d\n", ret);
		goto deinit_smmu;
	}

	ret = cnss_pci_enable_bus(pci_priv);
	if (ret)
		goto dereg_pci_event;

	switch (pci_dev->device) {
	case QCA6174_DEVICE_ID:
		pci_read_config_word(pci_dev, QCA6174_REV_ID_OFFSET,
				     &pci_priv->revision_id);
		ret = cnss_suspend_pci_link(pci_priv);
		if (ret)
			cnss_pr_err("Failed to suspend PCI link, err = %d\n",
				    ret);
		cnss_power_off_device(plat_priv);
		break;
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCA6490_DEVICE_ID:
		cnss_pci_set_wlaon_pwr_ctrl(pci_priv, false, false, false);
		timer_setup(&pci_priv->dev_rddm_timer,
			    cnss_dev_rddm_timeout_hdlr, 0);
		init_completion(&pci_priv->wake_event);
		INIT_DELAYED_WORK(&pci_priv->time_sync_work,
				  cnss_pci_time_sync_work_hdlr);

		ret = cnss_pci_enable_msi(pci_priv);
		if (ret)
			goto disable_bus;
		ret = cnss_pci_register_mhi(pci_priv);
		if (ret) {
			cnss_pci_disable_msi(pci_priv);
			goto disable_bus;
		}
		cnss_pci_get_link_status(pci_priv);
		cnss_pci_config_regs(pci_priv);
		if (EMULATION_HW)
			break;
		cnss_pci_set_wlaon_pwr_ctrl(pci_priv, false, true, false);
		ret = cnss_suspend_pci_link(pci_priv);
		if (ret)
			cnss_pr_err("Failed to suspend PCI link, err = %d\n",
				    ret);
		cnss_power_off_device(plat_priv);
		break;
	default:
		cnss_pr_err("Unknown PCI device found: 0x%x\n",
			    pci_dev->device);
		ret = -ENODEV;
		goto disable_bus;
	}

	return 0;

disable_bus:
	cnss_pci_disable_bus(pci_priv);
dereg_pci_event:
	cnss_dereg_pci_event(pci_priv);
deinit_smmu:
	cnss_pci_deinit_smmu(pci_priv);
unregister_ramdump:
	cnss_unregister_ramdump(plat_priv);
unregister_subsys:
	cnss_unregister_subsys(plat_priv);
reset_ctx:
	plat_priv->bus_priv = NULL;
out:
	return ret;
}

static void cnss_pci_remove(struct pci_dev *pci_dev)
{
	struct cnss_pci_data *pci_priv = cnss_get_pci_priv(pci_dev);
	struct cnss_plat_data *plat_priv =
		cnss_bus_dev_to_plat_priv(&pci_dev->dev);

	cnss_pci_free_m3_mem(pci_priv);
	cnss_pci_free_fw_mem(pci_priv);
	cnss_pci_free_qdss_mem(pci_priv);

	switch (pci_dev->device) {
	case QCA6290_DEVICE_ID:
	case QCA6390_DEVICE_ID:
	case QCA6490_DEVICE_ID:
		cnss_pci_unregister_mhi(pci_priv);
		cnss_pci_disable_msi(pci_priv);
		complete_all(&pci_priv->wake_event);
		del_timer(&pci_priv->dev_rddm_timer);
		break;
	default:
		break;
	}

	cnss_pci_disable_bus(pci_priv);
	cnss_dereg_pci_event(pci_priv);
	cnss_pci_deinit_smmu(pci_priv);
	if (plat_priv) {
		cnss_unregister_ramdump(plat_priv);
		cnss_unregister_subsys(plat_priv);
		plat_priv->bus_priv = NULL;
	} else {
		cnss_pr_err("Plat_priv is null, Unable to unregister ramdump,subsys\n");
	}

}

static const struct pci_device_id cnss_pci_id_table[] = {
	{ QCA6174_VENDOR_ID, QCA6174_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID },
	{ QCA6290_VENDOR_ID, QCA6290_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID },
	{ QCA6390_VENDOR_ID, QCA6390_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID },
	{ QCA6490_VENDOR_ID, QCA6490_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, cnss_pci_id_table);

static const struct dev_pm_ops cnss_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(cnss_pci_suspend, cnss_pci_resume)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(cnss_pci_suspend_noirq,
				      cnss_pci_resume_noirq)
	SET_RUNTIME_PM_OPS(cnss_pci_runtime_suspend, cnss_pci_runtime_resume,
			   cnss_pci_runtime_idle)
};

struct pci_driver cnss_pci_driver = {
	.name     = "cnss_pci",
	.id_table = cnss_pci_id_table,
	.probe    = cnss_pci_probe,
	.remove   = cnss_pci_remove,
	.driver = {
		.pm = &cnss_pm_ops,
	},
};

int cnss_pci_init(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct device *dev = &plat_priv->plat_dev->dev;
	u32 rc_num;
	int retry = 0;

	ret = of_property_read_u32(dev->of_node, "qcom,wlan-rc-num", &rc_num);
	if (ret) {
		cnss_pr_err("Failed to find PCIe RC number, err = %d\n", ret);
		goto out;
	}

retry:
	ret = msm_pcie_enumerate(rc_num);
	if (ret) {
		cnss_pr_err("Failed to enable PCIe RC%x, err = %d\n",
			    rc_num, ret);
		if (retry++ < LINK_TRAINING_RETRY_MAX_TIMES) {
			cnss_pr_dbg("Retry PCI link training #%d\n", retry);
			goto retry;
		} else {
			goto out;
		}
	}

	ret = pci_register_driver(&cnss_pci_driver);
	if (ret) {
		cnss_pr_err("Failed to register to PCI framework, err = %d\n",
			    ret);
		goto out;
	}

	if (!plat_priv->bus_priv) {
		cnss_pr_err("Failed to probe PCI driver\n");
		ret = -ENODEV;
		goto unreg_pci;
	}

	return 0;

unreg_pci:
	pci_unregister_driver(&cnss_pci_driver);
out:
	return ret;
}

void cnss_pci_deinit(struct cnss_plat_data *plat_priv)
{
	pci_unregister_driver(&cnss_pci_driver);
}
