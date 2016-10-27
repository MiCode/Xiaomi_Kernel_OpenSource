/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "icnss: " fmt

#include <asm/dma-iommu.h>
#include <linux/clk.h>
#include <linux/iommu.h>
#include <linux/export.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/qmi_encdec.h>
#include <linux/ipc_logging.h>
#include <linux/thread_info.h>
#include <linux/uaccess.h>
#include <linux/qpnp/qpnp-adc.h>
#include <soc/qcom/memory_dump.h>
#include <soc/qcom/icnss.h>
#include <soc/qcom/msm_qmi_interface.h>
#include <soc/qcom/secure_buffer.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/service-locator.h>
#include <soc/qcom/service-notifier.h>
#include <soc/qcom/socinfo.h>

#include "wlan_firmware_service_v01.h"

#ifdef CONFIG_ICNSS_DEBUG
unsigned long qmi_timeout = 3000;
module_param(qmi_timeout, ulong, 0600);

#define WLFW_TIMEOUT_MS			qmi_timeout
#else
#define WLFW_TIMEOUT_MS			3000
#endif
#define WLFW_SERVICE_INS_ID_V01		0
#define WLFW_CLIENT_ID			0x4b4e454c
#define MAX_PROP_SIZE			32
#define NUM_LOG_PAGES			10
#define NUM_REG_LOG_PAGES		4
#define ICNSS_MAGIC			0x5abc5abc

/*
 * Registers: MPM2_PSHOLD
 * Base Address: 0x10AC000
 */
#define MPM_WCSSAON_CONFIG_OFFSET				0x18
#define MPM_WCSSAON_CONFIG_ARES_N				BIT(0)
#define MPM_WCSSAON_CONFIG_WLAN_DISABLE				BIT(1)
#define MPM_WCSSAON_CONFIG_MSM_CLAMP_EN_OVRD			BIT(6)
#define MPM_WCSSAON_CONFIG_MSM_CLAMP_EN_OVRD_VAL		BIT(7)
#define MPM_WCSSAON_CONFIG_FORCE_ACTIVE				BIT(14)
#define MPM_WCSSAON_CONFIG_FORCE_XO_ENABLE			BIT(19)
#define MPM_WCSSAON_CONFIG_DISCONNECT_CLR			BIT(21)
#define MPM_WCSSAON_CONFIG_M2W_CLAMP_EN				BIT(22)

/*
 * Registers: WCSS_SR_SHADOW_REGISTERS
 * Base Address: 0x18820000
 */
#define SR_WCSSAON_SR_LSB_OFFSET				0x22070
#define SR_WCSSAON_SR_LSB_RETENTION_STATUS			BIT(20)

#define SR_PMM_SR_MSB						0x2206C
#define SR_PMM_SR_MSB_AHB_CLOCK_MASK				GENMASK(26, 22)
#define SR_PMM_SR_MSB_XO_CLOCK_MASK				GENMASK(31, 27)

/*
 * Registers: WCSS_HM_A_WCSS_CLK_CTL_WCSS_CC_REG
 * Base Address: 0x189D0000
 */
#define WCSS_WLAN1_GDSCR_OFFSET					0x1D3004
#define WCSS_WLAN1_GDSCR_SW_COLLAPSE				BIT(0)
#define WCSS_WLAN1_GDSCR_HW_CONTROL				BIT(1)
#define WCSS_WLAN1_GDSCR_PWR_ON					BIT(31)

#define WCSS_RFACTRL_GDSCR_OFFSET				0x1D60C8
#define WCSS_RFACTRL_GDSCR_SW_COLLAPSE				BIT(0)
#define WCSS_RFACTRL_GDSCR_HW_CONTROL				BIT(1)
#define WCSS_RFACTRL_GDSCR_PWR_ON				BIT(31)

#define WCSS_CLK_CTL_WCSS_CSS_GDSCR_OFFSET			0x1D1004
#define WCSS_CLK_CTL_WCSS_CSS_GDSCR_SW_COLLAPSE			BIT(0)
#define WCSS_CLK_CTL_WCSS_CSS_GDSCR_HW_CONTROL			BIT(1)
#define WCSS_CLK_CTL_WCSS_CSS_GDSCR_PWR_ON			BIT(31)

#define WCSS_CLK_CTL_NOC_CMD_RCGR_OFFSET			0x1D1030
#define WCSS_CLK_CTL_NOC_CMD_RCGR_UPDATE			BIT(0)

#define WCSS_CLK_CTL_NOC_CFG_RCGR_OFFSET			0x1D1034
#define WCSS_CLK_CTL_NOC_CFG_RCGR_SRC_SEL			GENMASK(10, 8)

#define WCSS_CLK_CTL_REF_CMD_RCGR_OFFSET			0x1D602C
#define WCSS_CLK_CTL_REF_CMD_RCGR_UPDATE			BIT(0)

#define WCSS_CLK_CTL_REF_CFG_RCGR_OFFSET			0x1D6030
#define WCSS_CLK_CTL_REF_CFG_RCGR_SRC_SEL			GENMASK(10, 8)

/*
 * Registers: WCSS_HM_A_WIFI_APB_3_A_WCMN_MAC_WCMN_REG
 * Base Address: 0x18AF0000
 */
#define WCMN_PMM_WLAN1_CFG_REG1_OFFSET				0x2F0804
#define WCMN_PMM_WLAN1_CFG_REG1_RFIF_ADC_PORDN_N		BIT(9)
#define WCMN_PMM_WLAN1_CFG_REG1_ADC_DIGITAL_CLAMP		BIT(10)

/*
 * Registers: WCSS_HM_A_PMM_PMM
 * Base Address: 0x18880000
 */
#define PMM_COMMON_IDLEREQ_CSR_OFFSET				0x80120
#define PMM_COMMON_IDLEREQ_CSR_SW_WNOC_IDLEREQ_SET		BIT(16)
#define PMM_COMMON_IDLEREQ_CSR_WNOC_IDLEACK			BIT(26)
#define PMM_COMMON_IDLEREQ_CSR_WNOC_IDLE			BIT(27)

#define PMM_RFACTRL_IDLEREQ_CSR_OFFSET				0x80164
#define PMM_RFACTRL_IDLEREQ_CSR_SW_RFACTRL_IDLEREQ_SET		BIT(16)
#define PMM_RFACTRL_IDLEREQ_CSR_RFACTRL_IDLETACK		BIT(26)

#define PMM_WSI_CMD_OFFSET					0x800E0
#define PMM_WSI_CMD_USE_WLAN1_WSI				BIT(0)
#define PMM_WSI_CMD_SW_USE_PMM_WSI				BIT(2)
#define PMM_WSI_CMD_SW_BUS_SYNC					BIT(3)
#define PMM_WSI_CMD_SW_RF_RESET					BIT(4)
#define PMM_WSI_CMD_SW_REG_READ					BIT(5)
#define PMM_WSI_CMD_SW_XO_DIS					BIT(8)
#define PMM_WSI_CMD_SW_FORCE_IDLE				BIT(9)
#define PMM_WSI_CMD_PMM_WSI_SM					GENMASK(24, 16)
#define PMM_WSI_CMD_RF_CMD_IP					BIT(31)

#define PMM_REG_RW_ADDR_OFFSET					0x800F0
#define PMM_REG_RW_ADDR_SW_REG_RW_ADDR				GENMASK(15, 0)

#define PMM_REG_READ_DATA_OFFSET				0x800F8

#define PMM_RF_VAULT_REG_ADDR_OFFSET				0x800FC
#define PMM_RF_VAULT_REG_ADDR_RF_VAULT_REG_ADDR			GENMASK(15, 0)

#define PMM_RF_VAULT_REG_DATA_OFFSET				0x80100
#define PMM_RF_VAULT_REG_DATA_RF_VAULT_REG_DATA			GENMASK(31, 0)

#define PMM_XO_DIS_ADDR_OFFSET					0x800E8
#define PMM_XO_DIS_ADDR_XO_DIS_ADDR				GENMASK(15, 0)

#define PMM_XO_DIS_DATA_OFFSET					0x800EC
#define PMM_XO_DIS_DATA_XO_DIS_DATA				GENMASK(31, 0)

#define PMM_RF_RESET_ADDR_OFFSET				0x80104
#define PMM_RF_RESET_ADDR_RF_RESET_ADDR				GENMASK(15, 0)

#define PMM_RF_RESET_DATA_OFFSET				0x80108
#define PMM_RF_RESET_DATA_RF_RESET_DATA				GENMASK(31, 0)

#define ICNSS_HW_REG_RETRY					10

#define WCSS_HM_A_PMM_HW_VERSION_V10				0x40000000
#define WCSS_HM_A_PMM_HW_VERSION_V20				0x40010000
#define WCSS_HM_A_PMM_HW_VERSION_Q10				0x40010001

#define ICNSS_SERVICE_LOCATION_CLIENT_NAME			"ICNSS-WLAN"
#define ICNSS_WLAN_SERVICE_NAME					"wlan/fw"

#define ICNSS_THRESHOLD_HIGH		3600000
#define ICNSS_THRESHOLD_LOW		3450000
#define ICNSS_THRESHOLD_GUARD		20000

#define icnss_ipc_log_string(_x...) do {				\
	if (icnss_ipc_log_context)					\
		ipc_log_string(icnss_ipc_log_context, _x);		\
	} while (0)

#ifdef CONFIG_ICNSS_DEBUG
#define icnss_ipc_log_long_string(_x...) do {				\
	if (icnss_ipc_log_long_context)					\
		ipc_log_string(icnss_ipc_log_long_context, _x);		\
	} while (0)
#else
#define icnss_ipc_log_long_string(_x...)
#endif

#define icnss_pr_err(_fmt, ...) do {					\
		pr_err(_fmt, ##__VA_ARGS__);				\
		icnss_ipc_log_string("ERR: " pr_fmt(_fmt),		\
				     ##__VA_ARGS__);			\
	} while (0)

#define icnss_pr_warn(_fmt, ...) do {					\
		pr_warn(_fmt, ##__VA_ARGS__);				\
		icnss_ipc_log_string("WRN: " pr_fmt(_fmt),		\
				     ##__VA_ARGS__);			\
	} while (0)

#define icnss_pr_info(_fmt, ...) do {					\
		pr_info(_fmt, ##__VA_ARGS__);				\
		icnss_ipc_log_string("INF: " pr_fmt(_fmt),		\
				     ##__VA_ARGS__);			\
	} while (0)

#define icnss_pr_dbg(_fmt, ...) do {					\
		pr_debug(_fmt, ##__VA_ARGS__);				\
		icnss_ipc_log_string("DBG: " pr_fmt(_fmt),		\
				     ##__VA_ARGS__);			\
	} while (0)

#define icnss_reg_dbg(_fmt, ...) do {				\
		pr_debug(_fmt, ##__VA_ARGS__);				\
		icnss_ipc_log_long_string("REG: " pr_fmt(_fmt),		\
				     ##__VA_ARGS__);			\
	} while (0)

#ifdef CONFIG_ICNSS_DEBUG
#define ICNSS_ASSERT(_condition) do {					\
		if (!(_condition)) {					\
			icnss_pr_err("ASSERT at line %d\n",		\
				     __LINE__);				\
			BUG_ON(1);					\
		}							\
	} while (0)
#else
#define ICNSS_ASSERT(_condition) do {					\
		if (!(_condition)) {					\
			icnss_pr_err("ASSERT at line %d\n",		\
				     __LINE__);				\
			WARN_ON(1);					\
		}							\
	} while (0)
#endif

enum icnss_debug_quirks {
	HW_ALWAYS_ON,
	HW_DEBUG_ENABLE,
	SKIP_QMI,
	HW_ONLY_TOP_LEVEL_RESET,
	RECOVERY_DISABLE,
	SSR_ONLY,
	PDR_ONLY,
	VBATT_DISABLE,
};

#define ICNSS_QUIRKS_DEFAULT		BIT(VBATT_DISABLE)

unsigned long quirks = ICNSS_QUIRKS_DEFAULT;
module_param(quirks, ulong, 0600);

void *icnss_ipc_log_context;

#ifdef CONFIG_ICNSS_DEBUG
void *icnss_ipc_log_long_context;
#endif

#define ICNSS_EVENT_PENDING			2989

#define ICNSS_EVENT_SYNC			BIT(0)
#define ICNSS_EVENT_UNINTERRUPTIBLE		BIT(1)
#define ICNSS_EVENT_SYNC_UNINTERRUPTIBLE	(ICNSS_EVENT_UNINTERRUPTIBLE | \
						 ICNSS_EVENT_SYNC)

enum icnss_driver_event_type {
	ICNSS_DRIVER_EVENT_SERVER_ARRIVE,
	ICNSS_DRIVER_EVENT_SERVER_EXIT,
	ICNSS_DRIVER_EVENT_FW_READY_IND,
	ICNSS_DRIVER_EVENT_REGISTER_DRIVER,
	ICNSS_DRIVER_EVENT_UNREGISTER_DRIVER,
	ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
	ICNSS_DRIVER_EVENT_MAX,
};

struct icnss_event_pd_service_down_data {
	bool crashed;
};

struct icnss_driver_event {
	struct list_head list;
	enum icnss_driver_event_type type;
	bool sync;
	struct completion complete;
	int ret;
	void *data;
};

enum icnss_driver_state {
	ICNSS_WLFW_QMI_CONNECTED,
	ICNSS_POWER_ON,
	ICNSS_FW_READY,
	ICNSS_DRIVER_PROBED,
	ICNSS_FW_TEST_MODE,
	ICNSS_PM_SUSPEND,
	ICNSS_PM_SUSPEND_NOIRQ,
	ICNSS_SSR_ENABLED,
	ICNSS_PDR_ENABLED,
	ICNSS_PD_RESTART,
	ICNSS_MSA0_ASSIGNED,
	ICNSS_WLFW_EXISTS,
};

struct ce_irq_list {
	int irq;
	irqreturn_t (*handler)(int, void *);
};

struct icnss_vreg_info {
	struct regulator *reg;
	const char *name;
	u32 min_v;
	u32 max_v;
	u32 load_ua;
	unsigned long settle_delay;
	bool required;
};

struct icnss_clk_info {
	struct clk *handle;
	const char *name;
	u32 freq;
	bool required;
};

static struct icnss_vreg_info icnss_vreg_info[] = {
	{NULL, "vdd-0.8-cx-mx", 800000, 800000, 0, 0, true},
	{NULL, "vdd-1.8-xo", 1800000, 1800000, 0, 0, false},
	{NULL, "vdd-1.3-rfa", 1304000, 1304000, 0, 0, false},
	{NULL, "vdd-3.3-ch0", 3312000, 3312000, 0, 0, false},
};

#define ICNSS_VREG_INFO_SIZE		ARRAY_SIZE(icnss_vreg_info)

static struct icnss_clk_info icnss_clk_info[] = {
	{NULL, "cxo_ref_clk_pin", 0, false},
};

#define ICNSS_CLK_INFO_SIZE		ARRAY_SIZE(icnss_clk_info)

struct icnss_stats {
	struct {
		uint32_t posted;
		uint32_t processed;
	} events[ICNSS_DRIVER_EVENT_MAX];

	struct {
		uint32_t request;
		uint32_t free;
		uint32_t enable;
		uint32_t disable;
	} ce_irqs[ICNSS_MAX_IRQ_REGISTRATIONS];

	uint32_t pm_suspend;
	uint32_t pm_suspend_err;
	uint32_t pm_resume;
	uint32_t pm_resume_err;
	uint32_t pm_suspend_noirq;
	uint32_t pm_suspend_noirq_err;
	uint32_t pm_resume_noirq;
	uint32_t pm_resume_noirq_err;
	uint32_t pm_stay_awake;
	uint32_t pm_relax;

	uint32_t ind_register_req;
	uint32_t ind_register_resp;
	uint32_t ind_register_err;
	uint32_t msa_info_req;
	uint32_t msa_info_resp;
	uint32_t msa_info_err;
	uint32_t msa_ready_req;
	uint32_t msa_ready_resp;
	uint32_t msa_ready_err;
	uint32_t msa_ready_ind;
	uint32_t cap_req;
	uint32_t cap_resp;
	uint32_t cap_err;
	uint32_t pin_connect_result;
	uint32_t cfg_req;
	uint32_t cfg_resp;
	uint32_t cfg_req_err;
	uint32_t mode_req;
	uint32_t mode_resp;
	uint32_t mode_req_err;
	uint32_t ini_req;
	uint32_t ini_resp;
	uint32_t ini_req_err;
	uint32_t vbatt_req;
	uint32_t vbatt_resp;
	uint32_t vbatt_req_err;
};

static struct icnss_priv {
	uint32_t magic;
	struct platform_device *pdev;
	struct icnss_driver_ops *ops;
	struct ce_irq_list ce_irq_list[ICNSS_MAX_IRQ_REGISTRATIONS];
	struct icnss_vreg_info vreg_info[ICNSS_VREG_INFO_SIZE];
	struct icnss_clk_info clk_info[ICNSS_CLK_INFO_SIZE];
	u32 ce_irqs[ICNSS_MAX_IRQ_REGISTRATIONS];
	phys_addr_t mem_base_pa;
	void __iomem *mem_base_va;
	phys_addr_t mpm_config_pa;
	void __iomem *mpm_config_va;
	struct dma_iommu_mapping *smmu_mapping;
	dma_addr_t smmu_iova_start;
	size_t smmu_iova_len;
	dma_addr_t smmu_iova_ipa_start;
	size_t smmu_iova_ipa_len;
	struct qmi_handle *wlfw_clnt;
	struct list_head event_list;
	spinlock_t event_lock;
	struct work_struct event_work;
	struct work_struct qmi_recv_msg_work;
	struct workqueue_struct *event_wq;
	phys_addr_t msa_pa;
	uint32_t msa_mem_size;
	void *msa_va;
	unsigned long state;
	struct wlfw_rf_chip_info_s_v01 chip_info;
	struct wlfw_rf_board_info_s_v01 board_info;
	struct wlfw_soc_info_s_v01 soc_info;
	struct wlfw_fw_version_info_s_v01 fw_version_info;
	u32 pwr_pin_result;
	u32 phy_io_pin_result;
	u32 rf_pin_result;
	struct icnss_mem_region_info
		icnss_mem_region[QMI_WLFW_MAX_NUM_MEMORY_REGIONS_V01];
	struct dentry *root_dentry;
	spinlock_t on_off_lock;
	struct icnss_stats stats;
	struct work_struct service_notifier_work;
	void **service_notifier;
	struct notifier_block service_notifier_nb;
	int total_domains;
	struct notifier_block get_service_nb;
	void *modem_notify_handler;
	struct notifier_block modem_ssr_nb;
	uint32_t diag_reg_read_addr;
	uint32_t diag_reg_read_mem_type;
	uint32_t diag_reg_read_len;
	uint8_t *diag_reg_read_buf;
	struct qpnp_adc_tm_btm_param vph_monitor_params;
	struct qpnp_adc_tm_chip *adc_tm_dev;
	struct qpnp_vadc_chip *vadc_dev;
	uint64_t vph_pwr;
	atomic_t pm_count;
} *penv;

static void icnss_hw_write_reg(void *base, u32 offset, u32 val)
{
	writel_relaxed(val, base + offset);
	wmb(); /* Ensure data is written to hardware register */
}

static u32 icnss_hw_read_reg(void *base, u32 offset)
{
	u32 rdata = readl_relaxed(base + offset);

	icnss_reg_dbg(" READ: offset: 0x%06x 0x%08x\n", offset, rdata);

	return rdata;
}

static void icnss_hw_write_reg_field(void *base, u32 offset, u32 mask, u32 val)
{
	u32 shift = find_first_bit((void *)&mask, 32);
	u32 rdata = readl_relaxed(base + offset);

	val = (rdata & ~mask) | (val << shift);

	icnss_reg_dbg("WRITE: offset: 0x%06x 0x%08x -> 0x%08x\n",
		     offset, rdata, val);

	icnss_hw_write_reg(base, offset, val);
}

static int icnss_hw_poll_reg_field(void *base, u32 offset, u32 mask, u32 val,
				    unsigned long usecs, int retry)
{
	u32 shift;
	u32 rdata;
	int r = 0;

	shift = find_first_bit((void *)&mask, 32);

	val = val << shift;

	rdata  = readl_relaxed(base + offset);

	icnss_reg_dbg(" POLL: offset: 0x%06x 0x%08x == 0x%08x & 0x%08x\n",
		     offset, val, rdata, mask);

	while ((rdata & mask) != val) {
		if (retry != 0 && r >= retry) {
			icnss_pr_err("POLL FAILED: offset: 0x%06x 0x%08x == 0x%08x & 0x%08x\n",
				     offset, val, rdata, mask);

			return -EIO;
		}

		r++;
		udelay(usecs);
		rdata = readl_relaxed(base + offset);

		if (retry)
			icnss_reg_dbg(" POLL: offset: 0x%06x 0x%08x == 0x%08x & 0x%08x\n",
					 offset, val, rdata, mask);

	}

	return 0;
}

static void icnss_pm_stay_awake(struct icnss_priv *priv)
{
	if (atomic_inc_return(&priv->pm_count) != 1)
		return;

	icnss_pr_dbg("PM stay awake, state: 0x%lx, count: %d\n", priv->state,
		     atomic_read(&priv->pm_count));

	pm_stay_awake(&priv->pdev->dev);

	priv->stats.pm_stay_awake++;
}

static void icnss_pm_relax(struct icnss_priv *priv)
{
	int r = atomic_dec_return(&priv->pm_count);

	WARN_ON(r < 0);

	if (r != 0)
		return;

	icnss_pr_dbg("PM relax, state: 0x%lx, count: %d\n", priv->state,
		     atomic_read(&priv->pm_count));

	pm_relax(&priv->pdev->dev);
	priv->stats.pm_relax++;
}

static char *icnss_driver_event_to_str(enum icnss_driver_event_type type)
{
	switch (type) {
	case ICNSS_DRIVER_EVENT_SERVER_ARRIVE:
		return "SERVER_ARRIVE";
	case ICNSS_DRIVER_EVENT_SERVER_EXIT:
		return "SERVER_EXIT";
	case ICNSS_DRIVER_EVENT_FW_READY_IND:
		return "FW_READY";
	case ICNSS_DRIVER_EVENT_REGISTER_DRIVER:
		return "REGISTER_DRIVER";
	case ICNSS_DRIVER_EVENT_UNREGISTER_DRIVER:
		return "UNREGISTER_DRIVER";
	case ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN:
		return "PD_SERVICE_DOWN";
	case ICNSS_DRIVER_EVENT_MAX:
		return "EVENT_MAX";
	}

	return "UNKNOWN";
};

static int icnss_driver_event_post(enum icnss_driver_event_type type,
				   u32 flags, void *data)
{
	struct icnss_driver_event *event;
	unsigned long irq_flags;
	int gfp = GFP_KERNEL;
	int ret = 0;

	icnss_pr_dbg("Posting event: %s(%d), %s, flags: 0x%x, state: 0x%lx\n",
		     icnss_driver_event_to_str(type), type, current->comm,
		     flags, penv->state);

	if (type >= ICNSS_DRIVER_EVENT_MAX) {
		icnss_pr_err("Invalid Event type: %d, can't post", type);
		return -EINVAL;
	}

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	event = kzalloc(sizeof(*event), gfp);
	if (event == NULL)
		return -ENOMEM;

	icnss_pm_stay_awake(penv);

	event->type = type;
	event->data = data;
	init_completion(&event->complete);
	event->ret = ICNSS_EVENT_PENDING;
	event->sync = !!(flags & ICNSS_EVENT_SYNC);

	spin_lock_irqsave(&penv->event_lock, irq_flags);
	list_add_tail(&event->list, &penv->event_list);
	spin_unlock_irqrestore(&penv->event_lock, irq_flags);

	penv->stats.events[type].posted++;
	queue_work(penv->event_wq, &penv->event_work);

	if (!(flags & ICNSS_EVENT_SYNC))
		goto out;

	if (flags & ICNSS_EVENT_UNINTERRUPTIBLE)
		wait_for_completion(&event->complete);
	else
		ret = wait_for_completion_interruptible(&event->complete);

	icnss_pr_dbg("Completed event: %s(%d), state: 0x%lx, ret: %d/%d\n",
		     icnss_driver_event_to_str(type), type, penv->state, ret,
		     event->ret);

	spin_lock_irqsave(&penv->event_lock, irq_flags);
	if (ret == -ERESTARTSYS && event->ret == ICNSS_EVENT_PENDING) {
		event->sync = false;
		spin_unlock_irqrestore(&penv->event_lock, irq_flags);
		ret = -EINTR;
		goto out;
	}
	spin_unlock_irqrestore(&penv->event_lock, irq_flags);

	ret = event->ret;
	kfree(event);

out:
	icnss_pm_relax(penv);
	return ret;
}

static int wlfw_vbatt_send_sync_msg(struct icnss_priv *priv,
				    uint64_t voltage_uv)
{
	int ret;
	struct wlfw_vbatt_req_msg_v01 req;
	struct wlfw_vbatt_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!priv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Sending Vbatt message, state: 0x%lx\n",
		     penv->state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.voltage_uv = voltage_uv;

	req_desc.max_msg_len = WLFW_VBATT_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_VBATT_REQ_V01;
	req_desc.ei_array = wlfw_vbatt_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_VBATT_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_VBATT_RESP_V01;
	resp_desc.ei_array = wlfw_vbatt_resp_msg_v01_ei;

	priv->stats.vbatt_req++;

	ret = qmi_send_req_wait(priv->wlfw_clnt, &req_desc, &req, sizeof(req),
			&resp_desc, &resp, sizeof(resp), WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send vbatt req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI vbatt request rejected, result:%d error:%d\n",
			resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}
	priv->stats.vbatt_resp++;

out:
	priv->stats.vbatt_req_err++;
	return ret;
}

static int icnss_get_phone_power(struct icnss_priv *priv, uint64_t *result_uv)
{
	int ret = 0;
	struct qpnp_vadc_result adc_result;

	if (!priv->vadc_dev) {
		icnss_pr_err("VADC dev doesn't exists\n");
		ret = -EINVAL;
		goto out;
	}

	ret = qpnp_vadc_read(penv->vadc_dev, VADC_VPH_PWR, &adc_result);
	if (ret) {
		icnss_pr_err("Error reading ADC channel %d, ret = %d\n",
			     VADC_VPH_PWR, ret);
		goto out;
	}

	icnss_pr_dbg("Phone power read phy=%lld meas=0x%llx\n",
		       adc_result.physical, adc_result.measurement);

	*result_uv = adc_result.physical;
out:
	return ret;
}

static void icnss_vph_notify(enum qpnp_tm_state state, void *ctx)
{
	struct icnss_priv *priv = ctx;
	uint64_t vph_pwr = 0;
	uint64_t vph_pwr_prev;
	int ret = 0;
	bool update = true;

	if (!priv) {
		icnss_pr_err("Priv pointer is NULL\n");
		return;
	}

	vph_pwr_prev = priv->vph_pwr;

	ret = icnss_get_phone_power(priv, &vph_pwr);
	if (ret)
		return;

	if (vph_pwr < ICNSS_THRESHOLD_LOW) {
		if (vph_pwr_prev < ICNSS_THRESHOLD_LOW)
			update = false;
		priv->vph_monitor_params.state_request =
			ADC_TM_HIGH_THR_ENABLE;
		priv->vph_monitor_params.high_thr = ICNSS_THRESHOLD_LOW +
			ICNSS_THRESHOLD_GUARD;
		priv->vph_monitor_params.low_thr = 0;
	} else if (vph_pwr > ICNSS_THRESHOLD_HIGH) {
		if (vph_pwr_prev > ICNSS_THRESHOLD_HIGH)
			update = false;
		priv->vph_monitor_params.state_request =
			ADC_TM_LOW_THR_ENABLE;
		priv->vph_monitor_params.low_thr = ICNSS_THRESHOLD_HIGH -
			ICNSS_THRESHOLD_GUARD;
		priv->vph_monitor_params.high_thr = 0;
	} else {
		if (vph_pwr_prev > ICNSS_THRESHOLD_LOW &&
		    vph_pwr_prev < ICNSS_THRESHOLD_HIGH)
			update = false;
		priv->vph_monitor_params.state_request =
			ADC_TM_HIGH_LOW_THR_ENABLE;
		priv->vph_monitor_params.low_thr = ICNSS_THRESHOLD_LOW;
		priv->vph_monitor_params.high_thr = ICNSS_THRESHOLD_HIGH;
	}

	priv->vph_pwr = vph_pwr;

	if (update)
		wlfw_vbatt_send_sync_msg(priv, vph_pwr);

	icnss_pr_dbg("set low threshold to %d, high threshold to %d\n",
		       priv->vph_monitor_params.low_thr,
		       priv->vph_monitor_params.high_thr);
	ret = qpnp_adc_tm_channel_measure(priv->adc_tm_dev,
					  &priv->vph_monitor_params);
	if (ret)
		icnss_pr_err("TM channel setup failed %d\n", ret);
}

static int icnss_setup_vph_monitor(struct icnss_priv *priv)
{
	int ret = 0;

	if (!priv->adc_tm_dev) {
		icnss_pr_err("ADC TM handler is NULL\n");
		ret = -EINVAL;
		goto out;
	}

	priv->vph_monitor_params.low_thr = ICNSS_THRESHOLD_LOW;
	priv->vph_monitor_params.high_thr = ICNSS_THRESHOLD_HIGH;
	priv->vph_monitor_params.state_request = ADC_TM_HIGH_LOW_THR_ENABLE;
	priv->vph_monitor_params.channel = VADC_VPH_PWR;
	priv->vph_monitor_params.btm_ctx = priv;
	priv->vph_monitor_params.timer_interval = ADC_MEAS1_INTERVAL_1S;
	priv->vph_monitor_params.threshold_notification = &icnss_vph_notify;
	icnss_pr_dbg("Set low threshold to %d, high threshold to %d\n",
		       priv->vph_monitor_params.low_thr,
		       priv->vph_monitor_params.high_thr);

	ret = qpnp_adc_tm_channel_measure(priv->adc_tm_dev,
					  &priv->vph_monitor_params);
	if (ret)
		icnss_pr_err("TM channel setup failed %d\n", ret);
out:
	return ret;
}

static int icnss_init_vph_monitor(struct icnss_priv *priv)
{
	int ret = 0;

	if (test_bit(VBATT_DISABLE, &quirks))
		goto out;

	ret = icnss_get_phone_power(priv, &priv->vph_pwr);
	if (ret)
		goto out;

	wlfw_vbatt_send_sync_msg(priv, priv->vph_pwr);

	ret = icnss_setup_vph_monitor(priv);
	if (ret)
		goto out;
out:
	return ret;
}


static int icnss_qmi_pin_connect_result_ind(void *msg, unsigned int msg_len)
{
	struct msg_desc ind_desc;
	struct wlfw_pin_connect_result_ind_msg_v01 ind_msg;
	int ret = 0;

	if (!penv || !penv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	ind_desc.msg_id = QMI_WLFW_PIN_CONNECT_RESULT_IND_V01;
	ind_desc.max_msg_len = WLFW_PIN_CONNECT_RESULT_IND_MSG_V01_MAX_MSG_LEN;
	ind_desc.ei_array = wlfw_pin_connect_result_ind_msg_v01_ei;

	ret = qmi_kernel_decode(&ind_desc, &ind_msg, msg, msg_len);
	if (ret < 0) {
		icnss_pr_err("Failed to decode message: %d, msg_len: %u\n",
			     ret, msg_len);
		goto out;
	}

	/* store pin result locally */
	if (ind_msg.pwr_pin_result_valid)
		penv->pwr_pin_result = ind_msg.pwr_pin_result;
	if (ind_msg.phy_io_pin_result_valid)
		penv->phy_io_pin_result = ind_msg.phy_io_pin_result;
	if (ind_msg.rf_pin_result_valid)
		penv->rf_pin_result = ind_msg.rf_pin_result;

	icnss_pr_dbg("Pin connect Result: pwr_pin: 0x%x phy_io_pin: 0x%x rf_io_pin: 0x%x\n",
		     ind_msg.pwr_pin_result, ind_msg.phy_io_pin_result,
		     ind_msg.rf_pin_result);

	penv->stats.pin_connect_result++;
out:
	return ret;
}

static int icnss_vreg_on(struct icnss_priv *priv)
{
	int ret = 0;
	struct icnss_vreg_info *vreg_info;
	int i;

	for (i = 0; i < ICNSS_VREG_INFO_SIZE; i++) {
		vreg_info = &priv->vreg_info[i];

		if (!vreg_info->reg)
			continue;

		icnss_pr_dbg("Regulator %s being enabled\n", vreg_info->name);

		ret = regulator_set_voltage(vreg_info->reg, vreg_info->min_v,
					    vreg_info->max_v);

		if (ret) {
			icnss_pr_err("Regulator %s, can't set voltage: min_v: %u, max_v: %u, ret: %d\n",
				     vreg_info->name, vreg_info->min_v,
				     vreg_info->max_v, ret);
			break;
		}

		if (vreg_info->load_ua) {
			ret = regulator_set_load(vreg_info->reg,
						 vreg_info->load_ua);

			if (ret < 0) {
				icnss_pr_err("Regulator %s, can't set load: %u, ret: %d\n",
					     vreg_info->name,
					     vreg_info->load_ua, ret);
				break;
			}
		}

		ret = regulator_enable(vreg_info->reg);
		if (ret) {
			icnss_pr_err("Regulator %s, can't enable: %d\n",
				     vreg_info->name, ret);
			break;
		}

		if (vreg_info->settle_delay)
			udelay(vreg_info->settle_delay);
	}

	if (!ret)
		return 0;

	for (; i >= 0; i--) {
		vreg_info = &priv->vreg_info[i];

		if (!vreg_info->reg)
			continue;

		regulator_disable(vreg_info->reg);

		regulator_set_load(vreg_info->reg, 0);

		regulator_set_voltage(vreg_info->reg, 0, vreg_info->max_v);
	}

	return ret;
}

static int icnss_vreg_off(struct icnss_priv *priv)
{
	int ret = 0;
	struct icnss_vreg_info *vreg_info;
	int i;

	for (i = ICNSS_VREG_INFO_SIZE - 1; i >= 0; i--) {
		vreg_info = &priv->vreg_info[i];

		if (!vreg_info->reg)
			continue;

		icnss_pr_dbg("Regulator %s being disabled\n", vreg_info->name);

		ret = regulator_disable(vreg_info->reg);
		if (ret)
			icnss_pr_err("Regulator %s, can't disable: %d\n",
				     vreg_info->name, ret);

		ret = regulator_set_load(vreg_info->reg, 0);
		if (ret < 0)
			icnss_pr_err("Regulator %s, can't set load: %d\n",
				     vreg_info->name, ret);

		ret = regulator_set_voltage(vreg_info->reg, 0,
					    vreg_info->max_v);

		if (ret)
			icnss_pr_err("Regulator %s, can't set voltage: %d\n",
				     vreg_info->name, ret);
	}

	return ret;
}

static int icnss_clk_init(struct icnss_priv *priv)
{
	struct icnss_clk_info *clk_info;
	int i;
	int ret = 0;

	for (i = 0; i < ICNSS_CLK_INFO_SIZE; i++) {
		clk_info = &priv->clk_info[i];

		if (!clk_info->handle)
			continue;

		icnss_pr_dbg("Clock %s being enabled\n", clk_info->name);

		if (clk_info->freq) {
			ret = clk_set_rate(clk_info->handle, clk_info->freq);

			if (ret) {
				icnss_pr_err("Clock %s, can't set frequency: %u, ret: %d\n",
					     clk_info->name, clk_info->freq,
					     ret);
				break;
			}
		}

		ret = clk_prepare_enable(clk_info->handle);

		if (ret) {
			icnss_pr_err("Clock %s, can't enable: %d\n",
				     clk_info->name, ret);
			break;
		}
	}

	if (ret == 0)
		return 0;

	for (; i >= 0; i--) {
		clk_info = &priv->clk_info[i];

		if (!clk_info->handle)
			continue;

		clk_disable_unprepare(clk_info->handle);
	}

	return ret;
}

static int icnss_clk_deinit(struct icnss_priv *priv)
{
	struct icnss_clk_info *clk_info;
	int i;

	for (i = 0; i < ICNSS_CLK_INFO_SIZE; i++) {
		clk_info = &priv->clk_info[i];

		if (!clk_info->handle)
			continue;

		icnss_pr_dbg("Clock %s being disabled\n", clk_info->name);

		clk_disable_unprepare(clk_info->handle);
	}

	return 0;
}

static void icnss_hw_top_level_release_reset(struct icnss_priv *priv)
{
	icnss_pr_dbg("RESET: HW Release reset: state: 0x%lx\n", priv->state);

	icnss_hw_write_reg_field(priv->mpm_config_va, MPM_WCSSAON_CONFIG_OFFSET,
				 MPM_WCSSAON_CONFIG_ARES_N, 1);

	icnss_hw_write_reg_field(priv->mpm_config_va, MPM_WCSSAON_CONFIG_OFFSET,
				 MPM_WCSSAON_CONFIG_WLAN_DISABLE, 0x0);

	icnss_hw_poll_reg_field(priv->mpm_config_va,
				MPM_WCSSAON_CONFIG_OFFSET,
				MPM_WCSSAON_CONFIG_ARES_N, 1, 10,
				ICNSS_HW_REG_RETRY);
}

static void icnss_hw_top_level_reset(struct icnss_priv *priv)
{
	icnss_pr_dbg("RESET: HW top level reset: state: 0x%lx\n", priv->state);

	icnss_hw_write_reg_field(priv->mpm_config_va,
				 MPM_WCSSAON_CONFIG_OFFSET,
				 MPM_WCSSAON_CONFIG_ARES_N, 0);

	icnss_hw_poll_reg_field(priv->mpm_config_va,
				MPM_WCSSAON_CONFIG_OFFSET,
				MPM_WCSSAON_CONFIG_ARES_N, 0, 10,
				ICNSS_HW_REG_RETRY);
}

static void icnss_hw_io_reset(struct icnss_priv *priv, bool on)
{
	u32 hw_version = priv->soc_info.soc_id;

	if (on && !test_bit(ICNSS_FW_READY, &priv->state))
		return;

	icnss_pr_dbg("HW io reset: %s, SoC: 0x%x, state: 0x%lx\n",
		     on ? "ON" : "OFF", priv->soc_info.soc_id, priv->state);

	if (hw_version == WCSS_HM_A_PMM_HW_VERSION_V10 ||
	    hw_version == WCSS_HM_A_PMM_HW_VERSION_V20) {
		icnss_hw_write_reg_field(priv->mpm_config_va,
				MPM_WCSSAON_CONFIG_OFFSET,
				MPM_WCSSAON_CONFIG_MSM_CLAMP_EN_OVRD_VAL, 0);
		icnss_hw_write_reg_field(priv->mpm_config_va,
				MPM_WCSSAON_CONFIG_OFFSET,
				MPM_WCSSAON_CONFIG_MSM_CLAMP_EN_OVRD, on);
	} else if (hw_version == WCSS_HM_A_PMM_HW_VERSION_Q10) {
		icnss_hw_write_reg_field(priv->mpm_config_va,
				MPM_WCSSAON_CONFIG_OFFSET,
				MPM_WCSSAON_CONFIG_M2W_CLAMP_EN,
				on);
	}
}

static int icnss_hw_reset_wlan_ss_power_down(struct icnss_priv *priv)
{
	u32 rdata;

	icnss_pr_dbg("RESET: WLAN SS power down, state: 0x%lx\n", priv->state);

	rdata = icnss_hw_read_reg(priv->mem_base_va, WCSS_WLAN1_GDSCR_OFFSET);

	if ((rdata & WCSS_WLAN1_GDSCR_PWR_ON) == 0)
		return 0;

	icnss_hw_write_reg_field(priv->mem_base_va, WCSS_WLAN1_GDSCR_OFFSET,
				 WCSS_WLAN1_GDSCR_HW_CONTROL, 0);

	icnss_hw_write_reg_field(priv->mem_base_va, WCSS_WLAN1_GDSCR_OFFSET,
				 WCSS_WLAN1_GDSCR_SW_COLLAPSE, 1);

	icnss_hw_poll_reg_field(priv->mem_base_va, WCSS_WLAN1_GDSCR_OFFSET,
				WCSS_WLAN1_GDSCR_PWR_ON, 0, 10,
				ICNSS_HW_REG_RETRY);

	icnss_hw_write_reg_field(priv->mem_base_va,
				 WCMN_PMM_WLAN1_CFG_REG1_OFFSET,
				 WCMN_PMM_WLAN1_CFG_REG1_ADC_DIGITAL_CLAMP, 1);

	icnss_hw_write_reg_field(priv->mem_base_va,
				 WCMN_PMM_WLAN1_CFG_REG1_OFFSET,
				 WCMN_PMM_WLAN1_CFG_REG1_RFIF_ADC_PORDN_N, 0);

	return 0;
}

static int icnss_hw_reset_common_ss_power_down(struct icnss_priv *priv)
{
	u32 rdata;

	icnss_pr_dbg("RESET: Common SS power down, state: 0x%lx\n",
		     priv->state);

	rdata = icnss_hw_read_reg(priv->mem_base_va,
				  WCSS_CLK_CTL_WCSS_CSS_GDSCR_OFFSET);

	if ((rdata & WCSS_CLK_CTL_WCSS_CSS_GDSCR_PWR_ON) == 0)
		return 0;

	icnss_hw_write_reg_field(priv->mem_base_va,
				 PMM_COMMON_IDLEREQ_CSR_OFFSET,
				 PMM_COMMON_IDLEREQ_CSR_SW_WNOC_IDLEREQ_SET,
				 1);

	icnss_hw_poll_reg_field(priv->mem_base_va,
				PMM_COMMON_IDLEREQ_CSR_OFFSET,
				PMM_COMMON_IDLEREQ_CSR_WNOC_IDLEACK,
				1, 20, ICNSS_HW_REG_RETRY);

	icnss_hw_poll_reg_field(priv->mem_base_va,
				PMM_COMMON_IDLEREQ_CSR_OFFSET,
				PMM_COMMON_IDLEREQ_CSR_WNOC_IDLE,
				1, 10, ICNSS_HW_REG_RETRY);

	icnss_hw_write_reg_field(priv->mem_base_va,
				 WCSS_CLK_CTL_WCSS_CSS_GDSCR_OFFSET,
				 WCSS_CLK_CTL_WCSS_CSS_GDSCR_HW_CONTROL, 0);

	icnss_hw_write_reg_field(priv->mem_base_va,
				 WCSS_CLK_CTL_WCSS_CSS_GDSCR_OFFSET,
				 WCSS_CLK_CTL_WCSS_CSS_GDSCR_SW_COLLAPSE, 1);

	icnss_hw_poll_reg_field(priv->mem_base_va,
				WCSS_CLK_CTL_WCSS_CSS_GDSCR_OFFSET,
				WCSS_CLK_CTL_WCSS_CSS_GDSCR_PWR_ON, 0, 10,
				ICNSS_HW_REG_RETRY);

	return 0;

}

static int icnss_hw_reset_wlan_rfactrl_power_down(struct icnss_priv *priv)
{
	u32 rdata;

	icnss_pr_dbg("RESET: RFACTRL power down, state: 0x%lx\n", priv->state);

	rdata = icnss_hw_read_reg(priv->mem_base_va, WCSS_RFACTRL_GDSCR_OFFSET);

	if ((rdata & WCSS_RFACTRL_GDSCR_PWR_ON) == 0)
		return 0;

	icnss_hw_write_reg_field(priv->mem_base_va,
				 PMM_RFACTRL_IDLEREQ_CSR_OFFSET,
				 PMM_RFACTRL_IDLEREQ_CSR_SW_RFACTRL_IDLEREQ_SET,
				 1);

	icnss_hw_poll_reg_field(priv->mem_base_va,
				PMM_RFACTRL_IDLEREQ_CSR_OFFSET,
				PMM_RFACTRL_IDLEREQ_CSR_RFACTRL_IDLETACK,
				1, 10, ICNSS_HW_REG_RETRY);

	icnss_hw_write_reg_field(priv->mem_base_va, WCSS_RFACTRL_GDSCR_OFFSET,
				 WCSS_RFACTRL_GDSCR_HW_CONTROL, 0);

	icnss_hw_write_reg_field(priv->mem_base_va, WCSS_RFACTRL_GDSCR_OFFSET,
				 WCSS_RFACTRL_GDSCR_SW_COLLAPSE, 1);

	return 0;
}

static void icnss_hw_wsi_cmd_error_recovery(struct icnss_priv *priv)
{
	icnss_pr_dbg("RESET: WSI CMD Error recovery, state: 0x%lx\n",
		     priv->state);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_FORCE_IDLE, 1);

	icnss_hw_poll_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				PMM_WSI_CMD_PMM_WSI_SM, 1, 100, 0);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_FORCE_IDLE, 0);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_BUS_SYNC, 1);

	icnss_hw_poll_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				PMM_WSI_CMD_RF_CMD_IP, 0, 100, 0);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_BUS_SYNC, 0);
}

static u32 icnss_hw_rf_register_read_command(struct icnss_priv *priv, u32 addr)
{
	u32 rdata = 0;
	int ret;
	int i;

	icnss_pr_dbg("RF register read command, addr: 0x%04x, state: 0x%lx\n",
		     addr, priv->state);

	for (i = 0; i < ICNSS_HW_REG_RETRY; i++) {
		icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
					 PMM_WSI_CMD_USE_WLAN1_WSI, 1);

		icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
					 PMM_WSI_CMD_SW_USE_PMM_WSI, 1);

		icnss_hw_write_reg_field(priv->mem_base_va,
					 PMM_REG_RW_ADDR_OFFSET,
					 PMM_REG_RW_ADDR_SW_REG_RW_ADDR,
					 addr & 0xFFFF);

		icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
					 PMM_WSI_CMD_SW_REG_READ, 1);

		ret = icnss_hw_poll_reg_field(priv->mem_base_va,
					      PMM_WSI_CMD_OFFSET,
					      PMM_WSI_CMD_RF_CMD_IP, 0, 10,
					      ICNSS_HW_REG_RETRY);
		if (ret == 0)
			break;

		icnss_hw_wsi_cmd_error_recovery(priv);
	}


	rdata = icnss_hw_read_reg(priv->mem_base_va, PMM_REG_READ_DATA_OFFSET);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_USE_PMM_WSI, 0);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_REG_READ, 0);

	icnss_pr_dbg("RF register read command, data: 0x%08x, state: 0x%lx\n",
		     rdata, priv->state);

	return rdata;
}

static int icnss_hw_reset_rf_reset_cmd(struct icnss_priv *priv)
{
	u32 rdata;
	int ret;

	icnss_pr_dbg("RESET: RF reset command, state: 0x%lx\n", priv->state);

	rdata = icnss_hw_rf_register_read_command(priv, 0x5080);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_USE_WLAN1_WSI, 1);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_USE_PMM_WSI, 1);

	icnss_hw_write_reg_field(priv->mem_base_va,
				 PMM_RF_VAULT_REG_ADDR_OFFSET,
				 PMM_RF_VAULT_REG_ADDR_RF_VAULT_REG_ADDR,
				 0x5082);

	icnss_hw_write_reg_field(priv->mem_base_va,
				 PMM_RF_VAULT_REG_DATA_OFFSET,
				 PMM_RF_VAULT_REG_DATA_RF_VAULT_REG_DATA,
				 0x12AB8FAD);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_RF_RESET_ADDR_OFFSET,
				 PMM_RF_RESET_ADDR_RF_RESET_ADDR, 0x5080);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_RF_RESET_DATA_OFFSET,
				 PMM_RF_RESET_DATA_RF_RESET_DATA,
				 rdata & 0xBFFF);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_RF_RESET, 1);

	ret = icnss_hw_poll_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				      PMM_WSI_CMD_RF_CMD_IP, 0, 10,
				      ICNSS_HW_REG_RETRY);

	if (ret) {
		icnss_pr_err("RESET: RF reset command failed, state: 0x%lx\n",
			     priv->state);
		return ret;
	}

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_USE_PMM_WSI, 0);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_RF_RESET, 0);

	return 0;
}

static int icnss_hw_reset_switch_to_cxo(struct icnss_priv *priv)
{
	icnss_pr_dbg("RESET: Switch to CXO, state: 0x%lx\n", priv->state);

	icnss_hw_write_reg_field(priv->mem_base_va,
				 WCSS_CLK_CTL_NOC_CFG_RCGR_OFFSET,
				 WCSS_CLK_CTL_NOC_CFG_RCGR_SRC_SEL, 0);

	icnss_hw_write_reg_field(priv->mem_base_va,
				 WCSS_CLK_CTL_NOC_CMD_RCGR_OFFSET,
				 WCSS_CLK_CTL_NOC_CMD_RCGR_UPDATE, 1);

	icnss_hw_write_reg_field(priv->mem_base_va,
				 WCSS_CLK_CTL_REF_CFG_RCGR_OFFSET,
				 WCSS_CLK_CTL_REF_CFG_RCGR_SRC_SEL, 0);

	icnss_hw_write_reg_field(priv->mem_base_va,
				 WCSS_CLK_CTL_REF_CMD_RCGR_OFFSET,
				 WCSS_CLK_CTL_REF_CMD_RCGR_UPDATE, 1);

	return 0;
}

static int icnss_hw_reset_xo_disable_cmd(struct icnss_priv *priv)
{
	int ret;

	icnss_pr_dbg("RESET: XO disable command, state: 0x%lx\n", priv->state);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_USE_WLAN1_WSI, 1);
	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_USE_PMM_WSI, 1);

	icnss_hw_write_reg_field(priv->mem_base_va,
				 PMM_RF_VAULT_REG_ADDR_OFFSET,
				 PMM_RF_VAULT_REG_ADDR_RF_VAULT_REG_ADDR,
				 0x5082);

	icnss_hw_write_reg_field(priv->mem_base_va,
				 PMM_RF_VAULT_REG_DATA_OFFSET,
				 PMM_RF_VAULT_REG_DATA_RF_VAULT_REG_DATA,
				 0x12AB8FAD);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_XO_DIS_ADDR_OFFSET,
				 PMM_XO_DIS_ADDR_XO_DIS_ADDR, 0x5081);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_XO_DIS_DATA_OFFSET,
				 PMM_XO_DIS_DATA_XO_DIS_DATA, 1);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_XO_DIS, 1);

	ret = icnss_hw_poll_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				      PMM_WSI_CMD_RF_CMD_IP, 0, 10,
				      ICNSS_HW_REG_RETRY);
	if (ret) {
		icnss_pr_err("RESET: XO disable command failed, state: 0x%lx\n",
			     priv->state);
		return ret;
	}

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_USE_PMM_WSI, 0);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_XO_DIS, 0);

	return 0;
}

static int icnss_hw_reset(struct icnss_priv *priv)
{
	u32 rdata;
	u32 rdata1;
	int i;
	int ret = 0;

	if (test_bit(HW_ONLY_TOP_LEVEL_RESET, &quirks))
		goto top_level_reset;

	icnss_pr_dbg("RESET: START, state: 0x%lx\n", priv->state);

	icnss_hw_write_reg_field(priv->mpm_config_va, MPM_WCSSAON_CONFIG_OFFSET,
				 MPM_WCSSAON_CONFIG_FORCE_ACTIVE, 1);

	icnss_hw_poll_reg_field(priv->mem_base_va, SR_WCSSAON_SR_LSB_OFFSET,
				SR_WCSSAON_SR_LSB_RETENTION_STATUS, 1, 200,
				ICNSS_HW_REG_RETRY);

	for (i = 0; i < ICNSS_HW_REG_RETRY; i++) {
		rdata = icnss_hw_read_reg(priv->mem_base_va, SR_PMM_SR_MSB);
		udelay(10);
		rdata1 = icnss_hw_read_reg(priv->mem_base_va, SR_PMM_SR_MSB);

		icnss_pr_dbg("RESET: XO: 0x%05lx/0x%05lx, AHB: 0x%05lx/0x%05lx\n",
			     rdata & SR_PMM_SR_MSB_XO_CLOCK_MASK,
			     rdata1 & SR_PMM_SR_MSB_XO_CLOCK_MASK,
			     rdata & SR_PMM_SR_MSB_AHB_CLOCK_MASK,
			     rdata1 & SR_PMM_SR_MSB_AHB_CLOCK_MASK);

		if ((rdata & SR_PMM_SR_MSB_AHB_CLOCK_MASK) !=
		    (rdata1 & SR_PMM_SR_MSB_AHB_CLOCK_MASK) &&
		    (rdata & SR_PMM_SR_MSB_XO_CLOCK_MASK) !=
		    (rdata1 & SR_PMM_SR_MSB_XO_CLOCK_MASK))
			break;

		icnss_hw_write_reg_field(priv->mpm_config_va,
					 MPM_WCSSAON_CONFIG_OFFSET,
					 MPM_WCSSAON_CONFIG_FORCE_XO_ENABLE,
					 0x1);
		usleep_range(2000, 3000);
	}

	if (i >= ICNSS_HW_REG_RETRY)
		goto top_level_reset;

	icnss_hw_write_reg_field(priv->mpm_config_va, MPM_WCSSAON_CONFIG_OFFSET,
				 MPM_WCSSAON_CONFIG_DISCONNECT_CLR, 0x1);

	usleep_range(200, 300);

	icnss_hw_reset_wlan_ss_power_down(priv);

	icnss_hw_reset_common_ss_power_down(priv);

	icnss_hw_reset_wlan_rfactrl_power_down(priv);

	ret = icnss_hw_reset_rf_reset_cmd(priv);
	if (ret)
		goto top_level_reset;

	icnss_hw_reset_switch_to_cxo(priv);

	ret = icnss_hw_reset_xo_disable_cmd(priv);
	if (ret)
		goto top_level_reset;

	icnss_hw_write_reg_field(priv->mpm_config_va, MPM_WCSSAON_CONFIG_OFFSET,
				 MPM_WCSSAON_CONFIG_FORCE_ACTIVE, 0);

	icnss_hw_write_reg_field(priv->mpm_config_va, MPM_WCSSAON_CONFIG_OFFSET,
				 MPM_WCSSAON_CONFIG_DISCONNECT_CLR, 0);

	icnss_hw_write_reg_field(priv->mpm_config_va, MPM_WCSSAON_CONFIG_OFFSET,
				 MPM_WCSSAON_CONFIG_WLAN_DISABLE, 1);

	icnss_hw_poll_reg_field(priv->mem_base_va, SR_WCSSAON_SR_LSB_OFFSET,
				BIT(26), 1, 200, ICNSS_HW_REG_RETRY);

top_level_reset:
	icnss_hw_top_level_reset(priv);

	icnss_pr_dbg("RESET: DONE, state: 0x%lx\n", priv->state);

	return 0;
}

static int icnss_hw_power_on(struct icnss_priv *priv)
{
	int ret = 0;
	unsigned long flags;

	icnss_pr_dbg("HW Power on: state: 0x%lx\n", priv->state);

	spin_lock_irqsave(&priv->on_off_lock, flags);
	if (test_bit(ICNSS_POWER_ON, &priv->state)) {
		spin_unlock_irqrestore(&priv->on_off_lock, flags);
		return ret;
	}
	set_bit(ICNSS_POWER_ON, &priv->state);
	spin_unlock_irqrestore(&priv->on_off_lock, flags);

	ret = icnss_vreg_on(priv);
	if (ret)
		goto out;

	ret = icnss_clk_init(priv);
	if (ret)
		goto out;

	icnss_hw_top_level_release_reset(priv);

	icnss_hw_io_reset(penv, 1);

	return ret;
out:
	clear_bit(ICNSS_POWER_ON, &priv->state);
	return ret;
}

static int icnss_hw_power_off(struct icnss_priv *priv)
{
	int ret = 0;
	unsigned long flags;

	if (test_bit(HW_ALWAYS_ON, &quirks))
		return 0;

	icnss_pr_dbg("HW Power off: 0x%lx\n", priv->state);

	spin_lock_irqsave(&priv->on_off_lock, flags);
	if (!test_bit(ICNSS_POWER_ON, &priv->state)) {
		spin_unlock_irqrestore(&priv->on_off_lock, flags);
		return ret;
	}
	clear_bit(ICNSS_POWER_ON, &priv->state);
	spin_unlock_irqrestore(&priv->on_off_lock, flags);

	icnss_hw_io_reset(penv, 0);

	icnss_hw_reset(priv);

	icnss_clk_deinit(priv);

	ret = icnss_vreg_off(priv);
	if (ret)
		goto out;

	return ret;
out:
	set_bit(ICNSS_POWER_ON, &priv->state);
	return ret;
}

int icnss_power_on(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %p, data %p\n",
			     dev, priv);
		return -EINVAL;
	}

	icnss_pr_dbg("Power On: 0x%lx\n", priv->state);

	return icnss_hw_power_on(priv);
}
EXPORT_SYMBOL(icnss_power_on);

int icnss_power_off(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %p, data %p\n",
			     dev, priv);
		return -EINVAL;
	}

	icnss_pr_dbg("Power Off: 0x%lx\n", priv->state);

	return icnss_hw_power_off(priv);
}
EXPORT_SYMBOL(icnss_power_off);

static int icnss_map_msa_permissions(struct icnss_priv *priv, u32 index)
{
	int ret = 0;
	phys_addr_t addr;
	u32 size;
	u32 source_vmlist[1] = {VMID_HLOS};
	int dest_vmids[3] = {VMID_MSS_MSA, VMID_WLAN, 0};
	int dest_perms[3] = {PERM_READ|PERM_WRITE,
			     PERM_READ|PERM_WRITE,
			     PERM_READ|PERM_WRITE};
	int source_nelems = sizeof(source_vmlist)/sizeof(u32);
	int dest_nelems = 0;

	addr = priv->icnss_mem_region[index].reg_addr;
	size = priv->icnss_mem_region[index].size;

	if (!priv->icnss_mem_region[index].secure_flag) {
		dest_vmids[2] = VMID_WLAN_CE;
		dest_nelems = 3;
	} else {
		dest_vmids[2] = 0;
		dest_nelems = 2;
	}
	ret = hyp_assign_phys(addr, size, source_vmlist, source_nelems,
			      dest_vmids, dest_perms, dest_nelems);
	if (ret) {
		icnss_pr_err("Region %u hyp_assign_phys failed IPA=%pa size=%u err=%d\n",
			     index, &addr, size, ret);
		goto out;
	}
	icnss_pr_dbg("Hypervisor map for region %u: source=%x, dest_nelems=%d, dest[0]=%x, dest[1]=%x, dest[2]=%x\n",
		     index, source_vmlist[0], dest_nelems,
		     dest_vmids[0], dest_vmids[1], dest_vmids[2]);
out:
	return ret;

}

static int icnss_unmap_msa_permissions(struct icnss_priv *priv, u32 index)
{
	int ret = 0;
	phys_addr_t addr;
	u32 size;
	u32 dest_vmids[1] = {VMID_HLOS};
	int source_vmlist[3] = {VMID_MSS_MSA, VMID_WLAN, 0};
	int dest_perms[1] = {PERM_READ|PERM_WRITE};
	int source_nelems = 0;
	int dest_nelems = sizeof(dest_vmids)/sizeof(u32);

	addr = priv->icnss_mem_region[index].reg_addr;
	size = priv->icnss_mem_region[index].size;
	if (!priv->icnss_mem_region[index].secure_flag) {
		source_vmlist[2] = VMID_WLAN_CE;
		source_nelems = 3;
	} else {
		source_vmlist[2] = 0;
		source_nelems = 2;
	}

	ret = hyp_assign_phys(addr, size, source_vmlist, source_nelems,
			      dest_vmids, dest_perms, dest_nelems);
	if (ret) {
		icnss_pr_err("Region %u hyp_assign_phys failed IPA=%pa size=%u err=%d\n",
			     index, &addr, size, ret);
		goto out;
	}
	icnss_pr_dbg("hypervisor unmap for region %u, source_nelems=%d, source[0]=%x, source[1]=%x, source[2]=%x, dest=%x\n",
		     index, source_nelems,
		     source_vmlist[0], source_vmlist[1], source_vmlist[2],
		     dest_vmids[0]);
out:
	return ret;
}

static int icnss_setup_msa_permissions(struct icnss_priv *priv)
{
	int ret;

	if (test_bit(ICNSS_MSA0_ASSIGNED, &priv->state))
		return 0;

	ret = icnss_map_msa_permissions(priv, 0);
	if (ret)
		return ret;

	ret = icnss_map_msa_permissions(priv, 1);
	if (ret)
		goto err_map_msa;

	set_bit(ICNSS_MSA0_ASSIGNED, &priv->state);

	return ret;

err_map_msa:
	icnss_unmap_msa_permissions(priv, 0);
	return ret;
}

static void icnss_remove_msa_permissions(struct icnss_priv *priv)
{
	if (!test_bit(ICNSS_MSA0_ASSIGNED, &priv->state))
		return;

	icnss_unmap_msa_permissions(priv, 0);
	icnss_unmap_msa_permissions(priv, 1);

	clear_bit(ICNSS_MSA0_ASSIGNED, &priv->state);
}

static int wlfw_msa_mem_info_send_sync_msg(void)
{
	int ret;
	int i;
	struct wlfw_msa_info_req_msg_v01 req;
	struct wlfw_msa_info_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt)
		return -ENODEV;

	icnss_pr_dbg("Sending MSA mem info, state: 0x%lx\n", penv->state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.msa_addr = penv->msa_pa;
	req.size = penv->msa_mem_size;

	req_desc.max_msg_len = WLFW_MSA_INFO_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_MSA_INFO_REQ_V01;
	req_desc.ei_array = wlfw_msa_info_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_MSA_INFO_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_MSA_INFO_RESP_V01;
	resp_desc.ei_array = wlfw_msa_info_resp_msg_v01_ei;

	penv->stats.msa_info_req++;

	ret = qmi_send_req_wait(penv->wlfw_clnt, &req_desc, &req, sizeof(req),
			&resp_desc, &resp, sizeof(resp), WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send MSA Mem info req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI MSA Mem info request rejected, result:%d error:%d\n",
			resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}

	icnss_pr_dbg("Receive mem_region_info_len: %d\n",
		     resp.mem_region_info_len);

	if (resp.mem_region_info_len > 2) {
		icnss_pr_err("Invalid memory region length received: %d\n",
			     resp.mem_region_info_len);
		ret = -EINVAL;
		goto out;
	}

	penv->stats.msa_info_resp++;
	for (i = 0; i < resp.mem_region_info_len; i++) {
		penv->icnss_mem_region[i].reg_addr =
			resp.mem_region_info[i].region_addr;
		penv->icnss_mem_region[i].size =
			resp.mem_region_info[i].size;
		penv->icnss_mem_region[i].secure_flag =
			resp.mem_region_info[i].secure_flag;
		icnss_pr_dbg("Memory Region: %d Addr: 0x%llx Size: 0x%x Flag: 0x%08x\n",
			 i, penv->icnss_mem_region[i].reg_addr,
			 penv->icnss_mem_region[i].size,
			 penv->icnss_mem_region[i].secure_flag);
	}

	return 0;

out:
	penv->stats.msa_info_err++;
	ICNSS_ASSERT(false);
	return ret;
}

static int wlfw_msa_ready_send_sync_msg(void)
{
	int ret;
	struct wlfw_msa_ready_req_msg_v01 req;
	struct wlfw_msa_ready_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt)
		return -ENODEV;

	icnss_pr_dbg("Sending MSA ready request message, state: 0x%lx\n",
		     penv->state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req_desc.max_msg_len = WLFW_MSA_READY_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_MSA_READY_REQ_V01;
	req_desc.ei_array = wlfw_msa_ready_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_MSA_READY_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_MSA_READY_RESP_V01;
	resp_desc.ei_array = wlfw_msa_ready_resp_msg_v01_ei;

	penv->stats.msa_ready_req++;
	ret = qmi_send_req_wait(penv->wlfw_clnt, &req_desc, &req, sizeof(req),
			&resp_desc, &resp, sizeof(resp), WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send MSA ready req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI MSA ready request rejected: result:%d error:%d\n",
			resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}
	penv->stats.msa_ready_resp++;

	return 0;

out:
	penv->stats.msa_ready_err++;
	ICNSS_ASSERT(false);
	return ret;
}

static int wlfw_ind_register_send_sync_msg(void)
{
	int ret;
	struct wlfw_ind_register_req_msg_v01 req;
	struct wlfw_ind_register_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt)
		return -ENODEV;

	icnss_pr_dbg("Sending indication register message, state: 0x%lx\n",
		     penv->state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.client_id_valid = 1;
	req.client_id = WLFW_CLIENT_ID;
	req.fw_ready_enable_valid = 1;
	req.fw_ready_enable = 1;
	req.msa_ready_enable_valid = 1;
	req.msa_ready_enable = 1;
	req.pin_connect_result_enable_valid = 1;
	req.pin_connect_result_enable = 1;

	req_desc.max_msg_len = WLFW_IND_REGISTER_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_IND_REGISTER_REQ_V01;
	req_desc.ei_array = wlfw_ind_register_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_IND_REGISTER_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_IND_REGISTER_RESP_V01;
	resp_desc.ei_array = wlfw_ind_register_resp_msg_v01_ei;

	penv->stats.ind_register_req++;

	ret = qmi_send_req_wait(penv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send indication register req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI indication register request rejected, resut:%d error:%d\n",
		       resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}
	penv->stats.ind_register_resp++;

	return 0;

out:
	penv->stats.ind_register_err++;
	ICNSS_ASSERT(false);
	return ret;
}

static int wlfw_cap_send_sync_msg(void)
{
	int ret;
	struct wlfw_cap_req_msg_v01 req;
	struct wlfw_cap_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt)
		return -ENODEV;

	icnss_pr_dbg("Sending capability message, state: 0x%lx\n", penv->state);

	memset(&resp, 0, sizeof(resp));

	req_desc.max_msg_len = WLFW_CAP_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_CAP_REQ_V01;
	req_desc.ei_array = wlfw_cap_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_CAP_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_CAP_RESP_V01;
	resp_desc.ei_array = wlfw_cap_resp_msg_v01_ei;

	penv->stats.cap_req++;
	ret = qmi_send_req_wait(penv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send capability req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI capability request rejected, result:%d error:%d\n",
		       resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}

	penv->stats.cap_resp++;
	/* store cap locally */
	if (resp.chip_info_valid)
		penv->chip_info = resp.chip_info;
	if (resp.board_info_valid)
		penv->board_info = resp.board_info;
	else
		penv->board_info.board_id = 0xFF;
	if (resp.soc_info_valid)
		penv->soc_info = resp.soc_info;
	if (resp.fw_version_info_valid)
		penv->fw_version_info = resp.fw_version_info;

	icnss_pr_dbg("Capability, chip_id: 0x%x, chip_family: 0x%x, board_id: 0x%x, soc_id: 0x%x, fw_version: 0x%x, fw_build_timestamp: %s",
		     penv->chip_info.chip_id, penv->chip_info.chip_family,
		     penv->board_info.board_id, penv->soc_info.soc_id,
		     penv->fw_version_info.fw_version,
		     penv->fw_version_info.fw_build_timestamp);

	return 0;

out:
	penv->stats.cap_err++;
	ICNSS_ASSERT(false);
	return ret;
}

static int wlfw_wlan_mode_send_sync_msg(enum wlfw_driver_mode_enum_v01 mode)
{
	int ret;
	struct wlfw_wlan_mode_req_msg_v01 req;
	struct wlfw_wlan_mode_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt)
		return -ENODEV;

	/* During recovery do not send mode request for WLAN OFF as
	 * FW not able to process it.
	 */
	if (test_bit(ICNSS_PD_RESTART, &penv->state) &&
	    mode == QMI_WLFW_OFF_V01)
		return 0;

	icnss_pr_dbg("Sending Mode request, state: 0x%lx, mode: %d\n",
		     penv->state, mode);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.mode = mode;
	req.hw_debug_valid = 1;
	req.hw_debug = !!test_bit(HW_DEBUG_ENABLE, &quirks);

	req_desc.max_msg_len = WLFW_WLAN_MODE_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_WLAN_MODE_REQ_V01;
	req_desc.ei_array = wlfw_wlan_mode_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_WLAN_MODE_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_WLAN_MODE_RESP_V01;
	resp_desc.ei_array = wlfw_wlan_mode_resp_msg_v01_ei;

	penv->stats.mode_req++;
	ret = qmi_send_req_wait(penv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send mode req failed, mode: %d ret: %d\n",
			     mode, ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI mode request rejected, mode:%d result:%d error:%d\n",
			     mode, resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}
	penv->stats.mode_resp++;

	return 0;

out:
	penv->stats.mode_req_err++;
	ICNSS_ASSERT(false);
	return ret;
}

static int wlfw_wlan_cfg_send_sync_msg(struct wlfw_wlan_cfg_req_msg_v01 *data)
{
	int ret;
	struct wlfw_wlan_cfg_req_msg_v01 req;
	struct wlfw_wlan_cfg_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt)
		return -ENODEV;

	icnss_pr_dbg("Sending config request, state: 0x%lx\n", penv->state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	memcpy(&req, data, sizeof(req));

	req_desc.max_msg_len = WLFW_WLAN_CFG_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_WLAN_CFG_REQ_V01;
	req_desc.ei_array = wlfw_wlan_cfg_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_WLAN_CFG_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_WLAN_CFG_RESP_V01;
	resp_desc.ei_array = wlfw_wlan_cfg_resp_msg_v01_ei;

	penv->stats.cfg_req++;
	ret = qmi_send_req_wait(penv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send config req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI config request rejected, result:%d error:%d\n",
		       resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}
	penv->stats.cfg_resp++;

	return 0;

out:
	penv->stats.cfg_req_err++;
	ICNSS_ASSERT(false);
	return ret;
}

static int wlfw_ini_send_sync_msg(bool enable_fw_log)
{
	int ret;
	struct wlfw_ini_req_msg_v01 req;
	struct wlfw_ini_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt)
		return -ENODEV;

	icnss_pr_dbg("Sending ini sync request, state: 0x%lx, fw_log: %d\n",
		     penv->state, enable_fw_log);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.enablefwlog_valid = 1;
	req.enablefwlog = enable_fw_log;

	req_desc.max_msg_len = WLFW_INI_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_INI_REQ_V01;
	req_desc.ei_array = wlfw_ini_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_INI_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_INI_RESP_V01;
	resp_desc.ei_array = wlfw_ini_resp_msg_v01_ei;

	penv->stats.ini_req++;

	ret = qmi_send_req_wait(penv->wlfw_clnt, &req_desc, &req, sizeof(req),
			&resp_desc, &resp, sizeof(resp), WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send INI req failed fw_log: %d, ret: %d\n",
			     enable_fw_log, ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI INI request rejected, fw_log:%d result:%d error:%d\n",
			     enable_fw_log, resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}
	penv->stats.ini_resp++;

	return 0;

out:
	penv->stats.ini_req_err++;
	ICNSS_ASSERT(false);
	return ret;
}

static int wlfw_athdiag_read_send_sync_msg(struct icnss_priv *priv,
					   uint32_t offset, uint32_t mem_type,
					   uint32_t data_len, uint8_t *data)
{
	int ret;
	struct wlfw_athdiag_read_req_msg_v01 req;
	struct wlfw_athdiag_read_resp_msg_v01 *resp = NULL;
	struct msg_desc req_desc, resp_desc;

	if (!priv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Diag read: state 0x%lx, offset %x, mem_type %x, data_len %u\n",
		     priv->state, offset, mem_type, data_len);

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		ret = -ENOMEM;
		goto out;
	}
	memset(&req, 0, sizeof(req));

	req.offset = offset;
	req.mem_type = mem_type;
	req.data_len = data_len;

	req_desc.max_msg_len = WLFW_ATHDIAG_READ_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_ATHDIAG_READ_REQ_V01;
	req_desc.ei_array = wlfw_athdiag_read_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_ATHDIAG_READ_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_ATHDIAG_READ_RESP_V01;
	resp_desc.ei_array = wlfw_athdiag_read_resp_msg_v01_ei;

	ret = qmi_send_req_wait(penv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, resp, sizeof(*resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("send athdiag read req failed %d\n", ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI athdiag read request rejected, result:%d error:%d\n",
			     resp->resp.result, resp->resp.error);
		ret = resp->resp.result;
		goto out;
	}

	if (!resp->data_valid || resp->data_len <= data_len) {
		icnss_pr_err("Athdiag read data is invalid, data_valid = %u, data_len = %u\n",
			     resp->data_valid, resp->data_len);
		ret = -EINVAL;
		goto out;
	}

	memcpy(data, resp->data, resp->data_len);

out:
	kfree(resp);
	return ret;
}

static int wlfw_athdiag_write_send_sync_msg(struct icnss_priv *priv,
					    uint32_t offset, uint32_t mem_type,
					    uint32_t data_len, uint8_t *data)
{
	int ret;
	struct wlfw_athdiag_write_req_msg_v01 *req = NULL;
	struct wlfw_athdiag_write_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!priv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Diag write: state 0x%lx, offset %x, mem_type %x, data_len %u, data %p\n",
		     priv->state, offset, mem_type, data_len, data);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req) {
		ret = -ENOMEM;
		goto out;
	}
	memset(&resp, 0, sizeof(resp));

	req->offset = offset;
	req->mem_type = mem_type;
	req->data_len = data_len;
	memcpy(req->data, data, data_len);

	req_desc.max_msg_len = WLFW_ATHDIAG_WRITE_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_ATHDIAG_WRITE_REQ_V01;
	req_desc.ei_array = wlfw_athdiag_write_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_ATHDIAG_WRITE_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_ATHDIAG_WRITE_RESP_V01;
	resp_desc.ei_array = wlfw_athdiag_write_resp_msg_v01_ei;

	ret = qmi_send_req_wait(penv->wlfw_clnt, &req_desc, req, sizeof(*req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("send athdiag write req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI athdiag write request rejected, result:%d error:%d\n",
			     resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		goto out;
	}
out:
	kfree(req);
	return ret;
}

static void icnss_qmi_wlfw_clnt_notify_work(struct work_struct *work)
{
	int ret;

	if (!penv || !penv->wlfw_clnt)
		return;

	icnss_pr_dbg("Receiving Event in work queue context\n");

	do {
	} while ((ret = qmi_recv_msg(penv->wlfw_clnt)) == 0);

	if (ret != -ENOMSG)
		icnss_pr_err("Error receiving message: %d\n", ret);

	icnss_pr_dbg("Receiving Event completed\n");
}

static void icnss_qmi_wlfw_clnt_notify(struct qmi_handle *handle,
			     enum qmi_event_type event, void *notify_priv)
{
	icnss_pr_dbg("QMI client notify: %d\n", event);

	if (!penv || !penv->wlfw_clnt)
		return;

	switch (event) {
	case QMI_RECV_MSG:
		schedule_work(&penv->qmi_recv_msg_work);
		break;
	default:
		icnss_pr_dbg("Unknown Event:  %d\n", event);
		break;
	}
}

static void icnss_qmi_wlfw_clnt_ind(struct qmi_handle *handle,
			  unsigned int msg_id, void *msg,
			  unsigned int msg_len, void *ind_cb_priv)
{
	if (!penv)
		return;

	icnss_pr_dbg("Received Ind 0x%x, msg_len: %d\n", msg_id, msg_len);

	switch (msg_id) {
	case QMI_WLFW_FW_READY_IND_V01:
		icnss_driver_event_post(ICNSS_DRIVER_EVENT_FW_READY_IND,
					0, NULL);
		break;
	case QMI_WLFW_MSA_READY_IND_V01:
		icnss_pr_dbg("Received MSA Ready Indication msg_id 0x%x\n",
			     msg_id);
		penv->stats.msa_ready_ind++;
		break;
	case QMI_WLFW_PIN_CONNECT_RESULT_IND_V01:
		icnss_pr_dbg("Received Pin Connect Test Result msg_id 0x%x\n",
			     msg_id);
		icnss_qmi_pin_connect_result_ind(msg, msg_len);
		break;
	default:
		icnss_pr_err("Invalid msg_id 0x%x\n", msg_id);
		break;
	}
}

static int icnss_driver_event_server_arrive(void *data)
{
	int ret = 0;

	if (!penv)
		return -ENODEV;

	set_bit(ICNSS_WLFW_EXISTS, &penv->state);

	penv->wlfw_clnt = qmi_handle_create(icnss_qmi_wlfw_clnt_notify, penv);
	if (!penv->wlfw_clnt) {
		icnss_pr_err("QMI client handle create failed\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = qmi_connect_to_service(penv->wlfw_clnt, WLFW_SERVICE_ID_V01,
				     WLFW_SERVICE_VERS_V01,
				     WLFW_SERVICE_INS_ID_V01);
	if (ret < 0) {
		icnss_pr_err("QMI WLAN Service not found : %d\n", ret);
		goto fail;
	}

	ret = qmi_register_ind_cb(penv->wlfw_clnt,
				  icnss_qmi_wlfw_clnt_ind, penv);
	if (ret < 0) {
		icnss_pr_err("Failed to register indication callback: %d\n",
			     ret);
		goto fail;
	}

	set_bit(ICNSS_WLFW_QMI_CONNECTED, &penv->state);

	icnss_pr_info("QMI Server Connected: state: 0x%lx\n", penv->state);

	ret = icnss_hw_power_on(penv);
	if (ret)
		goto fail;

	ret = wlfw_ind_register_send_sync_msg();
	if (ret < 0)
		goto err_power_on;

	if (!penv->msa_va) {
		icnss_pr_err("Invalid MSA address\n");
		ret = -EINVAL;
		goto err_power_on;
	}

	ret = wlfw_msa_mem_info_send_sync_msg();
	if (ret < 0)
		goto err_power_on;

	ret = icnss_setup_msa_permissions(penv);
	if (ret < 0)
		goto err_power_on;

	ret = wlfw_msa_ready_send_sync_msg();
	if (ret < 0)
		goto err_setup_msa;

	ret = wlfw_cap_send_sync_msg();
	if (ret < 0)
		goto err_setup_msa;

	icnss_init_vph_monitor(penv);

	return ret;

err_setup_msa:
	icnss_remove_msa_permissions(penv);
err_power_on:
	icnss_hw_power_off(penv);
fail:
	qmi_handle_destroy(penv->wlfw_clnt);
	penv->wlfw_clnt = NULL;
out:
	ICNSS_ASSERT(0);
	return ret;
}

static int icnss_driver_event_server_exit(void *data)
{
	if (!penv || !penv->wlfw_clnt)
		return -ENODEV;

	icnss_pr_info("QMI Service Disconnected: 0x%lx\n", penv->state);

	if (!test_bit(VBATT_DISABLE, &quirks) && penv->adc_tm_dev)
		qpnp_adc_tm_disable_chan_meas(penv->adc_tm_dev,
					      &penv->vph_monitor_params);

	qmi_handle_destroy(penv->wlfw_clnt);

	clear_bit(ICNSS_WLFW_QMI_CONNECTED, &penv->state);
	penv->wlfw_clnt = NULL;

	return 0;
}

static int icnss_call_driver_probe(struct icnss_priv *priv)
{
	int ret;

	if (!priv->ops || !priv->ops->probe)
		return 0;

	icnss_pr_dbg("Calling driver probe state: 0x%lx\n", priv->state);

	icnss_hw_power_on(priv);

	ret = priv->ops->probe(&priv->pdev->dev);
	if (ret < 0) {
		icnss_pr_err("Driver probe failed: %d, state: 0x%lx\n",
			     ret, priv->state);
		goto out;
	}

	set_bit(ICNSS_DRIVER_PROBED, &priv->state);

	return 0;

out:
	icnss_hw_power_off(priv);
	penv->ops = NULL;
	return ret;
}

static int icnss_call_driver_reinit(struct icnss_priv *priv)
{
	int ret = 0;

	if (!priv->ops || !priv->ops->reinit)
		goto out;

	if (!test_bit(ICNSS_DRIVER_PROBED, &penv->state))
		goto out;

	icnss_pr_dbg("Calling driver reinit state: 0x%lx\n", priv->state);

	icnss_hw_power_on(priv);

	ret = priv->ops->reinit(&priv->pdev->dev);
	if (ret < 0) {
		icnss_pr_err("Driver reinit failed: %d, state: 0x%lx\n",
			     ret, priv->state);
		ICNSS_ASSERT(false);
		goto out_power_off;
	}

out:
	clear_bit(ICNSS_PD_RESTART, &priv->state);

	icnss_pm_relax(priv);

	return 0;

out_power_off:
	icnss_hw_power_off(priv);

	clear_bit(ICNSS_PD_RESTART, &priv->state);

	icnss_pm_relax(priv);
	return ret;
}


static int icnss_driver_event_fw_ready_ind(void *data)
{
	int ret = 0;

	if (!penv)
		return -ENODEV;

	set_bit(ICNSS_FW_READY, &penv->state);

	icnss_pr_info("WLAN FW is ready: 0x%lx\n", penv->state);

	icnss_hw_power_off(penv);

	if (!penv->pdev) {
		icnss_pr_err("Device is not ready\n");
		ret = -ENODEV;
		goto out;
	}

	if (test_bit(ICNSS_PD_RESTART, &penv->state))
		ret = icnss_call_driver_reinit(penv);
	else
		ret = icnss_call_driver_probe(penv);

out:
	return ret;
}

static int icnss_driver_event_register_driver(void *data)
{
	int ret = 0;

	if (penv->ops)
		return -EEXIST;

	penv->ops = data;

	if (test_bit(SKIP_QMI, &quirks))
		set_bit(ICNSS_FW_READY, &penv->state);

	if (!test_bit(ICNSS_FW_READY, &penv->state)) {
		icnss_pr_dbg("FW is not ready yet, state: 0x%lx\n",
			     penv->state);
		goto out;
	}

	ret = icnss_hw_power_on(penv);
	if (ret)
		goto out;

	ret = penv->ops->probe(&penv->pdev->dev);

	if (ret) {
		icnss_pr_err("Driver probe failed: %d, state: 0x%lx\n",
			     ret, penv->state);
		goto power_off;
	}

	set_bit(ICNSS_DRIVER_PROBED, &penv->state);

	return 0;

power_off:
	icnss_hw_power_off(penv);
	penv->ops = NULL;
out:
	return ret;
}

static int icnss_driver_event_unregister_driver(void *data)
{
	if (!test_bit(ICNSS_DRIVER_PROBED, &penv->state)) {
		penv->ops = NULL;
		goto out;
	}

	if (penv->ops)
		penv->ops->remove(&penv->pdev->dev);

	clear_bit(ICNSS_DRIVER_PROBED, &penv->state);

	penv->ops = NULL;

	icnss_hw_power_off(penv);

out:
	return 0;
}

static int icnss_call_driver_remove(struct icnss_priv *priv)
{
	icnss_pr_dbg("Calling driver remove state: 0x%lx\n", priv->state);

	clear_bit(ICNSS_FW_READY, &priv->state);

	if (!test_bit(ICNSS_DRIVER_PROBED, &penv->state))
		return 0;

	if (!priv->ops || !priv->ops->remove)
		return 0;

	penv->ops->remove(&priv->pdev->dev);

	clear_bit(ICNSS_DRIVER_PROBED, &priv->state);

	return 0;
}

static int icnss_call_driver_shutdown(struct icnss_priv *priv)
{
	icnss_pr_dbg("Calling driver shutdown state: 0x%lx\n", priv->state);

	set_bit(ICNSS_PD_RESTART, &priv->state);
	clear_bit(ICNSS_FW_READY, &priv->state);

	icnss_pm_stay_awake(priv);

	if (!test_bit(ICNSS_DRIVER_PROBED, &penv->state))
		return 0;

	if (!priv->ops || !priv->ops->shutdown)
		return 0;

	priv->ops->shutdown(&priv->pdev->dev);

	return 0;
}

static int icnss_driver_event_pd_service_down(struct icnss_priv *priv,
					      void *data)
{
	int ret = 0;
	struct icnss_event_pd_service_down_data *event_data = data;

	if (!test_bit(ICNSS_WLFW_EXISTS, &priv->state))
		return 0;

	if (test_bit(ICNSS_PD_RESTART, &priv->state)) {
		icnss_pr_err("PD Down while recovery inprogress, crashed: %d, state: 0x%lx\n",
			     event_data->crashed, priv->state);
		ICNSS_ASSERT(0);
		goto out;
	}

	if (event_data->crashed)
		icnss_call_driver_shutdown(priv);
	else
		icnss_call_driver_remove(priv);

out:
	icnss_remove_msa_permissions(priv);

	ret = icnss_hw_power_off(priv);

	kfree(data);

	return ret;
}

static void icnss_driver_event_work(struct work_struct *work)
{
	struct icnss_driver_event *event;
	unsigned long flags;
	int ret;

	icnss_pm_stay_awake(penv);

	spin_lock_irqsave(&penv->event_lock, flags);

	while (!list_empty(&penv->event_list)) {
		event = list_first_entry(&penv->event_list,
					 struct icnss_driver_event, list);
		list_del(&event->list);
		spin_unlock_irqrestore(&penv->event_lock, flags);

		icnss_pr_dbg("Processing event: %s%s(%d), state: 0x%lx\n",
			     icnss_driver_event_to_str(event->type),
			     event->sync ? "-sync" : "", event->type,
			     penv->state);

		switch (event->type) {
		case ICNSS_DRIVER_EVENT_SERVER_ARRIVE:
			ret = icnss_driver_event_server_arrive(event->data);
			break;
		case ICNSS_DRIVER_EVENT_SERVER_EXIT:
			ret = icnss_driver_event_server_exit(event->data);
			break;
		case ICNSS_DRIVER_EVENT_FW_READY_IND:
			ret = icnss_driver_event_fw_ready_ind(event->data);
			break;
		case ICNSS_DRIVER_EVENT_REGISTER_DRIVER:
			ret = icnss_driver_event_register_driver(event->data);
			break;
		case ICNSS_DRIVER_EVENT_UNREGISTER_DRIVER:
			ret = icnss_driver_event_unregister_driver(event->data);
			break;
		case ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN:
			ret = icnss_driver_event_pd_service_down(penv,
								 event->data);
			break;
		default:
			icnss_pr_err("Invalid Event type: %d", event->type);
			kfree(event);
			continue;
		}

		penv->stats.events[event->type].processed++;

		icnss_pr_dbg("Event Processed: %s%s(%d), ret: %d, state: 0x%lx\n",
			     icnss_driver_event_to_str(event->type),
			     event->sync ? "-sync" : "", event->type, ret,
			     penv->state);

		spin_lock_irqsave(&penv->event_lock, flags);
		if (event->sync) {
			event->ret = ret;
			complete(&event->complete);
			continue;
		}
		spin_unlock_irqrestore(&penv->event_lock, flags);

		kfree(event);

		spin_lock_irqsave(&penv->event_lock, flags);
	}
	spin_unlock_irqrestore(&penv->event_lock, flags);

	icnss_pm_relax(penv);
}

static int icnss_qmi_wlfw_clnt_svc_event_notify(struct notifier_block *this,
					       unsigned long code,
					       void *_cmd)
{
	int ret = 0;

	if (!penv)
		return -ENODEV;

	icnss_pr_dbg("Event Notify: code: %ld", code);

	switch (code) {
	case QMI_SERVER_ARRIVE:
		ret = icnss_driver_event_post(ICNSS_DRIVER_EVENT_SERVER_ARRIVE,
					      0, NULL);
		break;

	case QMI_SERVER_EXIT:
		ret = icnss_driver_event_post(ICNSS_DRIVER_EVENT_SERVER_EXIT,
					      0, NULL);
		break;
	default:
		icnss_pr_dbg("Invalid code: %ld", code);
		break;
	}
	return ret;
}

static struct notifier_block wlfw_clnt_nb = {
	.notifier_call = icnss_qmi_wlfw_clnt_svc_event_notify,
};

static int icnss_modem_notifier_nb(struct notifier_block *nb,
				  unsigned long code,
				  void *data)
{
	struct icnss_event_pd_service_down_data *event_data;
	struct notif_data *notif = data;
	struct icnss_priv *priv = container_of(nb, struct icnss_priv,
					       modem_ssr_nb);

	icnss_pr_dbg("Modem-Notify: event %lu\n", code);

	if (code != SUBSYS_BEFORE_SHUTDOWN)
		return NOTIFY_OK;

	icnss_pr_info("Modem went down, state: %lx\n", priv->state);

	event_data = kzalloc(sizeof(*data), GFP_KERNEL);

	if (event_data == NULL)
		return notifier_from_errno(-ENOMEM);

	event_data->crashed = notif->crashed;

	icnss_driver_event_post(ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
				ICNSS_EVENT_SYNC, event_data);

	return NOTIFY_OK;
}

static int icnss_modem_ssr_register_notifier(struct icnss_priv *priv)
{
	int ret = 0;

	priv->modem_ssr_nb.notifier_call = icnss_modem_notifier_nb;

	priv->modem_notify_handler =
		subsys_notif_register_notifier("modem", &priv->modem_ssr_nb);

	if (IS_ERR(priv->modem_notify_handler)) {
		ret = PTR_ERR(priv->modem_notify_handler);
		icnss_pr_err("Modem register notifier failed: %d\n", ret);
	}

	set_bit(ICNSS_SSR_ENABLED, &priv->state);

	return ret;
}

static int icnss_modem_ssr_unregister_notifier(struct icnss_priv *priv)
{
	if (!test_and_clear_bit(ICNSS_SSR_ENABLED, &priv->state))
		return 0;

	subsys_notif_unregister_notifier(priv->modem_notify_handler,
					 &priv->modem_ssr_nb);
	priv->modem_notify_handler = NULL;

	return 0;
}

static int icnss_pdr_unregister_notifier(struct icnss_priv *priv)
{
	int i;

	if (!test_and_clear_bit(ICNSS_PDR_ENABLED, &priv->state))
		return 0;

	for (i = 0; i < priv->total_domains; i++)
		service_notif_unregister_notifier(priv->service_notifier[i],
						  &priv->service_notifier_nb);

	kfree(priv->service_notifier);

	priv->service_notifier = NULL;

	return 0;
}

static int icnss_service_notifier_notify(struct notifier_block *nb,
					 unsigned long notification, void *data)
{
	struct icnss_priv *priv = container_of(nb, struct icnss_priv,
					       service_notifier_nb);
	enum pd_subsys_state *state = data;
	struct icnss_event_pd_service_down_data *event_data;

	switch (notification) {
	case SERVREG_NOTIF_SERVICE_STATE_DOWN_V01:
		icnss_pr_info("Service down, data: 0x%p, state: 0x%lx\n", data,
			      priv->state);
		event_data = kzalloc(sizeof(*data), GFP_KERNEL);

		if (event_data == NULL)
			return notifier_from_errno(-ENOMEM);

		if (state == NULL || *state != SHUTDOWN)
			event_data->crashed = true;

		icnss_driver_event_post(ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
					ICNSS_EVENT_SYNC, event_data);
		break;
	case SERVREG_NOTIF_SERVICE_STATE_UP_V01:
		icnss_pr_dbg("Service up, state: 0x%lx\n", priv->state);
		break;
	default:
		icnss_pr_dbg("Service state Unknown, notification: 0x%lx, state: 0x%lx\n",
			     notification, priv->state);
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static int icnss_get_service_location_notify(struct notifier_block *nb,
					     unsigned long opcode, void *data)
{
	struct icnss_priv *priv = container_of(nb, struct icnss_priv,
					       get_service_nb);
	struct pd_qmi_client_data *pd = data;
	int curr_state;
	int ret;
	int i;
	void **handle;

	icnss_pr_dbg("Get service notify opcode: %lu, state: 0x%lx\n", opcode,
		     priv->state);

	if (opcode != LOCATOR_UP)
		return NOTIFY_DONE;

	if (pd->total_domains == 0) {
		icnss_pr_err("Did not find any domains\n");
		ret = -ENOENT;
		goto out;
	}

	handle = kcalloc(pd->total_domains, sizeof(void *), GFP_KERNEL);

	if (!handle) {
		ret = -ENOMEM;
		goto out;
	}

	priv->service_notifier_nb.notifier_call = icnss_service_notifier_notify;

	for (i = 0; i < pd->total_domains; i++) {
		icnss_pr_dbg("%d: domain_name: %s, instance_id: %d\n", i,
			     pd->domain_list[i].name,
			     pd->domain_list[i].instance_id);

		handle[i] =
			service_notif_register_notifier(pd->domain_list[i].name,
				pd->domain_list[i].instance_id,
				&priv->service_notifier_nb, &curr_state);

		if (IS_ERR(handle[i])) {
			icnss_pr_err("%d: Unable to register notifier for %s(0x%x)\n",
				     i, pd->domain_list->name,
				     pd->domain_list->instance_id);
			ret = PTR_ERR(handle[i]);
			goto free_handle;
		}
	}

	priv->service_notifier = handle;
	priv->total_domains = pd->total_domains;

	set_bit(ICNSS_PDR_ENABLED, &priv->state);

	icnss_modem_ssr_unregister_notifier(priv);

	icnss_pr_dbg("PD restart enabled, state: 0x%lx\n", priv->state);

	return NOTIFY_OK;

free_handle:
	for (i = 0; i < pd->total_domains; i++) {
		if (handle[i])
			service_notif_unregister_notifier(handle[i],
					&priv->service_notifier_nb);
	}
	kfree(handle);

out:
	icnss_pr_err("PD restart not enabled: %d, state: 0x%lx\n", ret,
		     priv->state);

	return NOTIFY_OK;
}


static int icnss_pd_restart_enable(struct icnss_priv *priv)
{
	int ret;

	if (test_bit(SSR_ONLY, &quirks)) {
		icnss_pr_dbg("PDR disabled through module parameter\n");
		return 0;
	}

	icnss_pr_dbg("Get service location, state: 0x%lx\n", priv->state);

	priv->get_service_nb.notifier_call = icnss_get_service_location_notify;
	ret = get_service_location(ICNSS_SERVICE_LOCATION_CLIENT_NAME,
				   ICNSS_WLAN_SERVICE_NAME,
				   &priv->get_service_nb);
	if (ret) {
		icnss_pr_err("Get service location failed: %d\n", ret);
		goto out;
	}

	return 0;
out:
	icnss_pr_err("PD restart not enabled: %d\n", ret);
	return ret;

}


static int icnss_enable_recovery(struct icnss_priv *priv)
{
	int ret;

	if (test_bit(RECOVERY_DISABLE, &quirks)) {
		icnss_pr_dbg("Recovery disabled through module parameter\n");
		return 0;
	}

	if (test_bit(PDR_ONLY, &quirks)) {
		icnss_pr_dbg("SSR disabled through module parameter\n");
		goto enable_pdr;
	}

	icnss_modem_ssr_register_notifier(priv);
	if (test_bit(SSR_ONLY, &quirks)) {
		icnss_pr_dbg("PDR disabled through module parameter\n");
		return 0;
	}

enable_pdr:
	ret = icnss_pd_restart_enable(priv);

	if (ret)
		return ret;

	return 0;
}

int icnss_register_driver(struct icnss_driver_ops *ops)
{
	int ret = 0;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Registering driver, state: 0x%lx\n", penv->state);

	if (penv->ops) {
		icnss_pr_err("Driver already registered\n");
		ret = -EEXIST;
		goto out;
	}

	if (!ops->probe || !ops->remove) {
		ret = -EINVAL;
		goto out;
	}

	ret = icnss_driver_event_post(ICNSS_DRIVER_EVENT_REGISTER_DRIVER,
				      ICNSS_EVENT_SYNC, ops);

	if (ret == -EINTR)
		ret = 0;

out:
	return ret;
}
EXPORT_SYMBOL(icnss_register_driver);

int icnss_unregister_driver(struct icnss_driver_ops *ops)
{
	int ret;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Unregistering driver, state: 0x%lx\n", penv->state);

	if (!penv->ops) {
		icnss_pr_err("Driver not registered\n");
		ret = -ENOENT;
		goto out;
	}

	ret = icnss_driver_event_post(ICNSS_DRIVER_EVENT_UNREGISTER_DRIVER,
				      ICNSS_EVENT_SYNC_UNINTERRUPTIBLE, NULL);
out:
	return ret;
}
EXPORT_SYMBOL(icnss_unregister_driver);

int icnss_ce_request_irq(unsigned int ce_id,
	irqreturn_t (*handler)(int, void *),
		unsigned long flags, const char *name, void *ctx)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("CE request IRQ: %d, state: 0x%lx\n", ce_id, penv->state);

	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		icnss_pr_err("Invalid CE ID, ce_id: %d\n", ce_id);
		ret = -EINVAL;
		goto out;
	}
	irq = penv->ce_irqs[ce_id];
	irq_entry = &penv->ce_irq_list[ce_id];

	if (irq_entry->handler || irq_entry->irq) {
		icnss_pr_err("IRQ already requested: %d, ce_id: %d\n",
			     irq, ce_id);
		ret = -EEXIST;
		goto out;
	}

	ret = request_irq(irq, handler, flags, name, ctx);
	if (ret) {
		icnss_pr_err("IRQ request failed: %d, ce_id: %d, ret: %d\n",
			     irq, ce_id, ret);
		goto out;
	}
	irq_entry->irq = irq;
	irq_entry->handler = handler;

	icnss_pr_dbg("IRQ requested: %d, ce_id: %d\n", irq, ce_id);

	penv->stats.ce_irqs[ce_id].request++;
out:
	return ret;
}
EXPORT_SYMBOL(icnss_ce_request_irq);

int icnss_ce_free_irq(unsigned int ce_id, void *ctx)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;

	if (!penv || !penv->pdev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("CE free IRQ: %d, state: 0x%lx\n", ce_id, penv->state);

	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		icnss_pr_err("Invalid CE ID to free, ce_id: %d\n", ce_id);
		ret = -EINVAL;
		goto out;
	}

	irq = penv->ce_irqs[ce_id];
	irq_entry = &penv->ce_irq_list[ce_id];
	if (!irq_entry->handler || !irq_entry->irq) {
		icnss_pr_err("IRQ not requested: %d, ce_id: %d\n", irq, ce_id);
		ret = -EEXIST;
		goto out;
	}
	free_irq(irq, ctx);
	irq_entry->irq = 0;
	irq_entry->handler = NULL;

	penv->stats.ce_irqs[ce_id].free++;
out:
	return ret;
}
EXPORT_SYMBOL(icnss_ce_free_irq);

void icnss_enable_irq(unsigned int ce_id)
{
	unsigned int irq;

	if (!penv || !penv->pdev) {
		icnss_pr_err("Platform driver not initialized\n");
		return;
	}

	icnss_pr_dbg("Enable IRQ: ce_id: %d, state: 0x%lx\n", ce_id,
		     penv->state);

	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		icnss_pr_err("Invalid CE ID to enable IRQ, ce_id: %d\n", ce_id);
		return;
	}

	penv->stats.ce_irqs[ce_id].enable++;

	irq = penv->ce_irqs[ce_id];
	enable_irq(irq);
}
EXPORT_SYMBOL(icnss_enable_irq);

void icnss_disable_irq(unsigned int ce_id)
{
	unsigned int irq;

	if (!penv || !penv->pdev) {
		icnss_pr_err("Platform driver not initialized\n");
		return;
	}

	icnss_pr_dbg("Disable IRQ: ce_id: %d, state: 0x%lx\n", ce_id,
		     penv->state);

	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS) {
		icnss_pr_err("Invalid CE ID to disable IRQ, ce_id: %d\n",
			     ce_id);
		return;
	}

	irq = penv->ce_irqs[ce_id];
	disable_irq(irq);

	penv->stats.ce_irqs[ce_id].disable++;
}
EXPORT_SYMBOL(icnss_disable_irq);

int icnss_get_soc_info(struct icnss_soc_info *info)
{
	if (!penv) {
		icnss_pr_err("Platform driver not initialized\n");
		return -EINVAL;
	}

	info->v_addr = penv->mem_base_va;
	info->p_addr = penv->mem_base_pa;
	info->chip_id = penv->chip_info.chip_id;
	info->chip_family = penv->chip_info.chip_family;
	info->board_id = penv->board_info.board_id;
	info->soc_id = penv->soc_info.soc_id;
	info->fw_version = penv->fw_version_info.fw_version;
	strlcpy(info->fw_build_timestamp,
		penv->fw_version_info.fw_build_timestamp,
		QMI_WLFW_MAX_TIMESTAMP_LEN_V01 + 1);

	return 0;
}
EXPORT_SYMBOL(icnss_get_soc_info);

int icnss_set_fw_debug_mode(bool enable_fw_log)
{
	int ret;

	icnss_pr_dbg("%s FW debug mode",
		     enable_fw_log ? "Enalbing" : "Disabling");

	ret = wlfw_ini_send_sync_msg(enable_fw_log);
	if (ret)
		icnss_pr_err("Fail to send ini, ret = %d, fw_log: %d\n", ret,
		       enable_fw_log);

	return ret;
}
EXPORT_SYMBOL(icnss_set_fw_debug_mode);

int icnss_athdiag_read(struct device *dev, uint32_t offset,
		       uint32_t mem_type, uint32_t data_len,
		       uint8_t *output)
{
	int ret = 0;
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for diag read: dev %p, data %p, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	if (!output || data_len == 0
	    || data_len > QMI_WLFW_MAX_DATA_SIZE_V01) {
		icnss_pr_err("Invalid parameters for diag read: output %p, data_len %u\n",
			     output, data_len);
		ret = -EINVAL;
		goto out;
	}

	if (!test_bit(ICNSS_FW_READY, &priv->state) ||
	    !test_bit(ICNSS_POWER_ON, &priv->state)) {
		icnss_pr_err("Invalid state for diag read: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	ret = wlfw_athdiag_read_send_sync_msg(priv, offset, mem_type,
					      data_len, output);
out:
	return ret;
}
EXPORT_SYMBOL(icnss_athdiag_read);

int icnss_athdiag_write(struct device *dev, uint32_t offset,
			uint32_t mem_type, uint32_t data_len,
			uint8_t *input)
{
	int ret = 0;
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for diag write: dev %p, data %p, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	if (!input || data_len == 0
	    || data_len > QMI_WLFW_MAX_DATA_SIZE_V01) {
		icnss_pr_err("Invalid parameters for diag write: input %p, data_len %u\n",
			     input, data_len);
		ret = -EINVAL;
		goto out;
	}

	if (!test_bit(ICNSS_FW_READY, &priv->state) ||
	    !test_bit(ICNSS_POWER_ON, &priv->state)) {
		icnss_pr_err("Invalid state for diag write: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	ret = wlfw_athdiag_write_send_sync_msg(priv, offset, mem_type,
					       data_len, input);
out:
	return ret;
}
EXPORT_SYMBOL(icnss_athdiag_write);

int icnss_wlan_enable(struct icnss_wlan_enable_cfg *config,
		      enum icnss_driver_mode mode,
		      const char *host_version)
{
	struct wlfw_wlan_cfg_req_msg_v01 req;
	u32 i;
	int ret;

	icnss_pr_dbg("Mode: %d, config: %p, host_version: %s\n",
		     mode, config, host_version);

	memset(&req, 0, sizeof(req));

	if (mode == ICNSS_WALTEST || mode == ICNSS_CCPM)
		goto skip;

	if (!config || !host_version) {
		icnss_pr_err("Invalid cfg pointer, config: %p, host_version: %p\n",
			     config, host_version);
		ret = -EINVAL;
		goto out;
	}

	req.host_version_valid = 1;
	strlcpy(req.host_version, host_version,
		QMI_WLFW_MAX_STR_LEN_V01 + 1);

	req.tgt_cfg_valid = 1;
	if (config->num_ce_tgt_cfg > QMI_WLFW_MAX_NUM_CE_V01)
		req.tgt_cfg_len = QMI_WLFW_MAX_NUM_CE_V01;
	else
		req.tgt_cfg_len = config->num_ce_tgt_cfg;
	for (i = 0; i < req.tgt_cfg_len; i++) {
		req.tgt_cfg[i].pipe_num = config->ce_tgt_cfg[i].pipe_num;
		req.tgt_cfg[i].pipe_dir = config->ce_tgt_cfg[i].pipe_dir;
		req.tgt_cfg[i].nentries = config->ce_tgt_cfg[i].nentries;
		req.tgt_cfg[i].nbytes_max = config->ce_tgt_cfg[i].nbytes_max;
		req.tgt_cfg[i].flags = config->ce_tgt_cfg[i].flags;
	}

	req.svc_cfg_valid = 1;
	if (config->num_ce_svc_pipe_cfg > QMI_WLFW_MAX_NUM_SVC_V01)
		req.svc_cfg_len = QMI_WLFW_MAX_NUM_SVC_V01;
	else
		req.svc_cfg_len = config->num_ce_svc_pipe_cfg;
	for (i = 0; i < req.svc_cfg_len; i++) {
		req.svc_cfg[i].service_id = config->ce_svc_cfg[i].service_id;
		req.svc_cfg[i].pipe_dir = config->ce_svc_cfg[i].pipe_dir;
		req.svc_cfg[i].pipe_num = config->ce_svc_cfg[i].pipe_num;
	}

	req.shadow_reg_valid = 1;
	if (config->num_shadow_reg_cfg >
	    QMI_WLFW_MAX_NUM_SHADOW_REG_V01)
		req.shadow_reg_len = QMI_WLFW_MAX_NUM_SHADOW_REG_V01;
	else
		req.shadow_reg_len = config->num_shadow_reg_cfg;

	memcpy(req.shadow_reg, config->shadow_reg_cfg,
	       sizeof(struct wlfw_shadow_reg_cfg_s_v01) * req.shadow_reg_len);

	ret = wlfw_wlan_cfg_send_sync_msg(&req);
	if (ret)
		goto out;
skip:
	ret = wlfw_wlan_mode_send_sync_msg(mode);
out:
	if (test_bit(SKIP_QMI, &quirks))
		ret = 0;

	return ret;
}
EXPORT_SYMBOL(icnss_wlan_enable);

int icnss_wlan_disable(enum icnss_driver_mode mode)
{
	return wlfw_wlan_mode_send_sync_msg(QMI_WLFW_OFF_V01);
}
EXPORT_SYMBOL(icnss_wlan_disable);

bool icnss_is_qmi_disable(void)
{
	return test_bit(SKIP_QMI, &quirks) ? true : false;
}
EXPORT_SYMBOL(icnss_is_qmi_disable);

int icnss_get_ce_id(int irq)
{
	int i;

	if (!penv || !penv->pdev)
		return -ENODEV;

	for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++) {
		if (penv->ce_irqs[i] == irq)
			return i;
	}

	icnss_pr_err("No matching CE id for irq %d\n", irq);

	return -EINVAL;
}
EXPORT_SYMBOL(icnss_get_ce_id);

int icnss_get_irq(int ce_id)
{
	int irq;

	if (!penv || !penv->pdev)
		return -ENODEV;

	if (ce_id >= ICNSS_MAX_IRQ_REGISTRATIONS)
		return -EINVAL;

	irq = penv->ce_irqs[ce_id];

	return irq;
}
EXPORT_SYMBOL(icnss_get_irq);

struct dma_iommu_mapping *icnss_smmu_get_mapping(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %p, data %p\n",
			     dev, priv);
		return NULL;
	}

	return priv->smmu_mapping;
}
EXPORT_SYMBOL(icnss_smmu_get_mapping);

int icnss_smmu_map(struct device *dev,
		   phys_addr_t paddr, uint32_t *iova_addr, size_t size)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	unsigned long iova;
	size_t len;
	int ret = 0;

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %p, data %p\n",
			     dev, priv);
		return -EINVAL;
	}

	if (!iova_addr) {
		icnss_pr_err("iova_addr is NULL, paddr %pa, size %zu\n",
			     &paddr, size);
		return -EINVAL;
	}

	len = roundup(size + paddr - rounddown(paddr, PAGE_SIZE), PAGE_SIZE);
	iova = roundup(penv->smmu_iova_ipa_start, PAGE_SIZE);

	if (iova >= priv->smmu_iova_ipa_start + priv->smmu_iova_ipa_len) {
		icnss_pr_err("No IOVA space to map, iova %lx, smmu_iova_ipa_start %pad, smmu_iova_ipa_len %zu\n",
			     iova,
			     &priv->smmu_iova_ipa_start,
			     priv->smmu_iova_ipa_len);
		return -ENOMEM;
	}

	ret = iommu_map(priv->smmu_mapping->domain, iova,
			rounddown(paddr, PAGE_SIZE), len,
			IOMMU_READ | IOMMU_WRITE);
	if (ret) {
		icnss_pr_err("PA to IOVA mapping failed, ret %d\n", ret);
		return ret;
	}

	priv->smmu_iova_ipa_start = iova + len;
	*iova_addr = (uint32_t)(iova + paddr - rounddown(paddr, PAGE_SIZE));

	return 0;
}
EXPORT_SYMBOL(icnss_smmu_map);

unsigned int icnss_socinfo_get_serial_number(struct device *dev)
{
	return socinfo_get_serial_number();
}
EXPORT_SYMBOL(icnss_socinfo_get_serial_number);

static int icnss_smmu_init(struct icnss_priv *priv)
{
	struct dma_iommu_mapping *mapping;
	int atomic_ctx = 1;
	int s1_bypass = 1;
	int ret = 0;

	icnss_pr_dbg("Initializing SMMU\n");

	mapping = arm_iommu_create_mapping(&platform_bus_type,
					   priv->smmu_iova_start,
					   priv->smmu_iova_len);
	if (IS_ERR(mapping)) {
		icnss_pr_err("Create mapping failed, err = %d\n", ret);
		ret = PTR_ERR(mapping);
		goto map_fail;
	}

	ret = iommu_domain_set_attr(mapping->domain,
				    DOMAIN_ATTR_ATOMIC,
				    &atomic_ctx);
	if (ret < 0) {
		icnss_pr_err("Set atomic_ctx attribute failed, err = %d\n",
			     ret);
		goto set_attr_fail;
	}

	ret = iommu_domain_set_attr(mapping->domain,
				    DOMAIN_ATTR_S1_BYPASS,
				    &s1_bypass);
	if (ret < 0) {
		icnss_pr_err("Set s1_bypass attribute failed, err = %d\n", ret);
		goto set_attr_fail;
	}

	ret = arm_iommu_attach_device(&priv->pdev->dev, mapping);
	if (ret < 0) {
		icnss_pr_err("Attach device failed, err = %d\n", ret);
		goto attach_fail;
	}

	priv->smmu_mapping = mapping;

	return ret;

attach_fail:
set_attr_fail:
	arm_iommu_release_mapping(mapping);
map_fail:
	return ret;
}

static void icnss_smmu_deinit(struct icnss_priv *priv)
{
	if (!priv->smmu_mapping)
		return;

	arm_iommu_detach_device(&priv->pdev->dev);
	arm_iommu_release_mapping(priv->smmu_mapping);

	priv->smmu_mapping = NULL;
}

static int icnss_get_vreg_info(struct device *dev,
			       struct icnss_vreg_info *vreg_info)
{
	int ret = 0;
	char prop_name[MAX_PROP_SIZE];
	struct regulator *reg;
	const __be32 *prop;
	int len = 0;
	int i;

	reg = devm_regulator_get_optional(dev, vreg_info->name);

	if (PTR_ERR(reg) == -EPROBE_DEFER) {
		icnss_pr_err("EPROBE_DEFER for regulator: %s\n",
			     vreg_info->name);
		ret = PTR_ERR(reg);
		goto out;
	}

	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);

		if (vreg_info->required) {

			icnss_pr_err("Regulator %s doesn't exist: %d\n",
				     vreg_info->name, ret);
			goto out;
		} else {
			icnss_pr_dbg("Optional regulator %s doesn't exist: %d\n",
				     vreg_info->name, ret);
			goto done;
		}
	}

	vreg_info->reg = reg;

	snprintf(prop_name, MAX_PROP_SIZE,
		 "qcom,%s-config", vreg_info->name);

	prop = of_get_property(dev->of_node, prop_name, &len);

	icnss_pr_dbg("Got regulator config, prop: %s, len: %d\n",
		     prop_name, len);

	if (!prop || len < (2 * sizeof(__be32))) {
		icnss_pr_dbg("Property %s %s\n", prop_name,
			     prop ? "invalid format" : "doesn't exist");
		goto done;
	}

	for (i = 0; (i * sizeof(__be32)) < len; i++) {
		switch (i) {
		case 0:
			vreg_info->min_v = be32_to_cpup(&prop[0]);
			break;
		case 1:
			vreg_info->max_v = be32_to_cpup(&prop[1]);
			break;
		case 2:
			vreg_info->load_ua = be32_to_cpup(&prop[2]);
			break;
		case 3:
			vreg_info->settle_delay = be32_to_cpup(&prop[3]);
			break;
		default:
			icnss_pr_dbg("Property %s, ignoring value at %d\n",
				     prop_name, i);
			break;
		}
	}

done:
	icnss_pr_dbg("Regulator: %s, min_v: %u, max_v: %u, load: %u, delay: %lu\n",
		     vreg_info->name, vreg_info->min_v, vreg_info->max_v,
		     vreg_info->load_ua, vreg_info->settle_delay);

	return 0;

out:
	return ret;
}

static int icnss_get_clk_info(struct device *dev,
			      struct icnss_clk_info *clk_info)
{
	struct clk *handle;
	int ret = 0;

	handle = devm_clk_get(dev, clk_info->name);

	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		if (clk_info->required) {
			icnss_pr_err("Clock %s isn't available: %d\n",
				     clk_info->name, ret);
			goto out;
		} else {
			icnss_pr_dbg("Ignoring clock %s: %d\n", clk_info->name,
				     ret);
			ret = 0;
			goto out;
		}
	}

	icnss_pr_dbg("Clock: %s, freq: %u\n", clk_info->name, clk_info->freq);

	clk_info->handle = handle;
out:
	return ret;
}

static int icnss_test_mode_show(struct seq_file *s, void *data)
{
	struct icnss_priv *priv = s->private;

	seq_puts(s, "0 : Test mode disable\n");
	seq_puts(s, "1 : WLAN Firmware test\n");
	seq_puts(s, "2 : CCPM test\n");

	seq_puts(s, "\n");

	if (!test_bit(ICNSS_FW_READY, &priv->state)) {
		seq_puts(s, "Firmware is not ready yet!, wait for FW READY\n");
		goto out;
	}

	if (test_bit(ICNSS_DRIVER_PROBED, &priv->state)) {
		seq_puts(s, "Machine mode is running, can't run test mode!\n");
		goto out;
	}

	if (test_bit(ICNSS_FW_TEST_MODE, &priv->state)) {
		seq_puts(s, "Test mode is running!\n");
		goto out;
	}

	seq_puts(s, "Test can be run, Have fun!\n");

out:
	seq_puts(s, "\n");
	return 0;
}

static int icnss_test_mode_fw_test_off(struct icnss_priv *priv)
{
	int ret;

	if (!test_bit(ICNSS_FW_READY, &priv->state)) {
		icnss_pr_err("Firmware is not ready yet!, wait for FW READY: state: 0x%lx\n",
			     priv->state);
		ret = -ENODEV;
		goto out;
	}

	if (test_bit(ICNSS_DRIVER_PROBED, &priv->state)) {
		icnss_pr_err("Machine mode is running, can't run test mode: state: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	if (!test_bit(ICNSS_FW_TEST_MODE, &priv->state)) {
		icnss_pr_err("Test mode not started, state: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	icnss_wlan_disable(ICNSS_OFF);

	ret = icnss_hw_power_off(priv);

	clear_bit(ICNSS_FW_TEST_MODE, &priv->state);

out:
	return ret;
}
static int icnss_test_mode_fw_test(struct icnss_priv *priv,
				   enum icnss_driver_mode mode)
{
	int ret;

	if (!test_bit(ICNSS_FW_READY, &priv->state)) {
		icnss_pr_err("Firmware is not ready yet!, wait for FW READY, state: 0x%lx\n",
			     priv->state);
		ret = -ENODEV;
		goto out;
	}

	if (test_bit(ICNSS_DRIVER_PROBED, &priv->state)) {
		icnss_pr_err("Machine mode is running, can't run test mode, state: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(ICNSS_FW_TEST_MODE, &priv->state)) {
		icnss_pr_err("Test mode already started, state: 0x%lx\n",
			     priv->state);
		ret = -EBUSY;
		goto out;
	}

	ret = icnss_hw_power_on(priv);
	if (ret)
		goto out;

	set_bit(ICNSS_FW_TEST_MODE, &priv->state);

	ret = icnss_wlan_enable(NULL, mode, NULL);
	if (ret)
		goto power_off;

	return 0;

power_off:
	icnss_hw_power_off(priv);
	clear_bit(ICNSS_FW_TEST_MODE, &priv->state);

out:
	return ret;
}

static ssize_t icnss_test_mode_write(struct file *fp, const char __user *buf,
				    size_t count, loff_t *off)
{
	struct icnss_priv *priv =
		((struct seq_file *)fp->private_data)->private;
	int ret;
	u32 val;

	ret = kstrtou32_from_user(buf, count, 0, &val);
	if (ret)
		return ret;

	switch (val) {
	case 0:
		ret = icnss_test_mode_fw_test_off(priv);
		break;
	case 1:
		ret = icnss_test_mode_fw_test(priv, ICNSS_WALTEST);
		break;
	case 2:
		ret = icnss_test_mode_fw_test(priv, ICNSS_CCPM);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (ret)
		return ret;

	if (ret == 0)
		memset(&priv->stats, 0, sizeof(priv->stats));

	return count;
}

static int icnss_test_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, icnss_test_mode_show, inode->i_private);
}

static const struct file_operations icnss_test_mode_fops = {
	.read		= seq_read,
	.write		= icnss_test_mode_write,
	.release	= single_release,
	.open		= icnss_test_mode_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static ssize_t icnss_stats_write(struct file *fp, const char __user *buf,
				    size_t count, loff_t *off)
{
	struct icnss_priv *priv =
		((struct seq_file *)fp->private_data)->private;
	int ret;
	u32 val;

	ret = kstrtou32_from_user(buf, count, 0, &val);
	if (ret)
		return ret;

	if (ret == 0)
		memset(&priv->stats, 0, sizeof(priv->stats));

	return count;
}

static int icnss_stats_show_state(struct seq_file *s, struct icnss_priv *priv)
{
	enum icnss_driver_state i;
	int skip = 0;
	unsigned long state;

	seq_printf(s, "\nState: 0x%lx(", priv->state);
	for (i = 0, state = priv->state; state != 0; state >>= 1, i++) {

		if (!(state & 0x1))
			continue;

		if (skip++)
			seq_puts(s, " | ");

		switch (i) {
		case ICNSS_WLFW_QMI_CONNECTED:
			seq_puts(s, "QMI CONN");
			continue;
		case ICNSS_POWER_ON:
			seq_puts(s, "POWER ON");
			continue;
		case ICNSS_FW_READY:
			seq_puts(s, "FW READY");
			continue;
		case ICNSS_DRIVER_PROBED:
			seq_puts(s, "DRIVER PROBED");
			continue;
		case ICNSS_FW_TEST_MODE:
			seq_puts(s, "FW TEST MODE");
			continue;
		case ICNSS_PM_SUSPEND:
			seq_puts(s, "PM SUSPEND");
			continue;
		case ICNSS_PM_SUSPEND_NOIRQ:
			seq_puts(s, "PM SUSPEND NOIRQ");
			continue;
		case ICNSS_SSR_ENABLED:
			seq_puts(s, "SSR ENABLED");
			continue;
		case ICNSS_PDR_ENABLED:
			seq_puts(s, "PDR ENABLED");
			continue;
		case ICNSS_PD_RESTART:
			seq_puts(s, "PD RESTART");
			continue;
		case ICNSS_MSA0_ASSIGNED:
			seq_puts(s, "MSA0 ASSIGNED");
			continue;
		case ICNSS_WLFW_EXISTS:
			seq_puts(s, "WLAN FW EXISTS");
			continue;
		}

		seq_printf(s, "UNKNOWN-%d", i);
	}
	seq_puts(s, ")\n");

	return 0;
}

static int icnss_stats_show_capability(struct seq_file *s,
				       struct icnss_priv *priv)
{
	if (test_bit(ICNSS_FW_READY, &priv->state)) {
		seq_puts(s, "\n<---------------- FW Capability ----------------->\n");
		seq_printf(s, "Chip ID: 0x%x\n", priv->chip_info.chip_id);
		seq_printf(s, "Chip family: 0x%x\n",
			  priv->chip_info.chip_family);
		seq_printf(s, "Board ID: 0x%x\n", priv->board_info.board_id);
		seq_printf(s, "SOC Info: 0x%x\n", priv->soc_info.soc_id);
		seq_printf(s, "Firmware Version: 0x%x\n",
			   priv->fw_version_info.fw_version);
		seq_printf(s, "Firmware Build Timestamp: %s\n",
			   priv->fw_version_info.fw_build_timestamp);
	}

	return 0;
}

static int icnss_stats_show_events(struct seq_file *s, struct icnss_priv *priv)
{
	int i;

	seq_puts(s, "\n<----------------- Events stats ------------------->\n");
	seq_printf(s, "%24s %16s %16s\n", "Events", "Posted", "Processed");
	for (i = 0; i < ICNSS_DRIVER_EVENT_MAX; i++)
		seq_printf(s, "%24s %16u %16u\n",
			   icnss_driver_event_to_str(i),
			   priv->stats.events[i].posted,
			   priv->stats.events[i].processed);

	return 0;
}

static int icnss_stats_show_irqs(struct seq_file *s, struct icnss_priv *priv)
{
	int i;

	seq_puts(s, "\n<------------------ IRQ stats ------------------->\n");
	seq_printf(s, "%4s %4s %8s %8s %8s %8s\n", "CE_ID", "IRQ", "Request",
		   "Free", "Enable", "Disable");
	for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++)
		seq_printf(s, "%4d: %4u %8u %8u %8u %8u\n", i,
			   priv->ce_irqs[i], priv->stats.ce_irqs[i].request,
			   priv->stats.ce_irqs[i].free,
			   priv->stats.ce_irqs[i].enable,
			   priv->stats.ce_irqs[i].disable);

	return 0;
}

static int icnss_stats_show(struct seq_file *s, void *data)
{
#define ICNSS_STATS_DUMP(_s, _priv, _x) \
	seq_printf(_s, "%24s: %u\n", #_x, _priv->stats._x)

	struct icnss_priv *priv = s->private;

	ICNSS_STATS_DUMP(s, priv, ind_register_req);
	ICNSS_STATS_DUMP(s, priv, ind_register_resp);
	ICNSS_STATS_DUMP(s, priv, ind_register_err);
	ICNSS_STATS_DUMP(s, priv, msa_info_req);
	ICNSS_STATS_DUMP(s, priv, msa_info_resp);
	ICNSS_STATS_DUMP(s, priv, msa_info_err);
	ICNSS_STATS_DUMP(s, priv, msa_ready_req);
	ICNSS_STATS_DUMP(s, priv, msa_ready_resp);
	ICNSS_STATS_DUMP(s, priv, msa_ready_err);
	ICNSS_STATS_DUMP(s, priv, msa_ready_ind);
	ICNSS_STATS_DUMP(s, priv, cap_req);
	ICNSS_STATS_DUMP(s, priv, cap_resp);
	ICNSS_STATS_DUMP(s, priv, cap_err);
	ICNSS_STATS_DUMP(s, priv, pin_connect_result);
	ICNSS_STATS_DUMP(s, priv, cfg_req);
	ICNSS_STATS_DUMP(s, priv, cfg_resp);
	ICNSS_STATS_DUMP(s, priv, cfg_req_err);
	ICNSS_STATS_DUMP(s, priv, mode_req);
	ICNSS_STATS_DUMP(s, priv, mode_resp);
	ICNSS_STATS_DUMP(s, priv, mode_req_err);
	ICNSS_STATS_DUMP(s, priv, ini_req);
	ICNSS_STATS_DUMP(s, priv, ini_resp);
	ICNSS_STATS_DUMP(s, priv, ini_req_err);
	ICNSS_STATS_DUMP(s, priv, vbatt_req);
	ICNSS_STATS_DUMP(s, priv, vbatt_resp);
	ICNSS_STATS_DUMP(s, priv, vbatt_req_err);

	seq_puts(s, "\n<------------------ PM stats ------------------->\n");
	ICNSS_STATS_DUMP(s, priv, pm_suspend);
	ICNSS_STATS_DUMP(s, priv, pm_suspend_err);
	ICNSS_STATS_DUMP(s, priv, pm_resume);
	ICNSS_STATS_DUMP(s, priv, pm_resume_err);
	ICNSS_STATS_DUMP(s, priv, pm_suspend_noirq);
	ICNSS_STATS_DUMP(s, priv, pm_suspend_noirq_err);
	ICNSS_STATS_DUMP(s, priv, pm_resume_noirq);
	ICNSS_STATS_DUMP(s, priv, pm_resume_noirq_err);
	ICNSS_STATS_DUMP(s, priv, pm_stay_awake);
	ICNSS_STATS_DUMP(s, priv, pm_relax);

	icnss_stats_show_irqs(s, priv);

	icnss_stats_show_capability(s, priv);

	icnss_stats_show_events(s, priv);

	icnss_stats_show_state(s, priv);

	return 0;
#undef ICNSS_STATS_DUMP
}

static int icnss_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, icnss_stats_show, inode->i_private);
}

static const struct file_operations icnss_stats_fops = {
	.read		= seq_read,
	.write		= icnss_stats_write,
	.release	= single_release,
	.open		= icnss_stats_open,
	.owner		= THIS_MODULE,
	.llseek		= seq_lseek,
};

static int icnss_regwrite_show(struct seq_file *s, void *data)
{
	struct icnss_priv *priv = s->private;

	seq_puts(s, "\nUsage: echo <mem_type> <offset> <reg_val> > <debugfs>/icnss/reg_write\n");

	if (!test_bit(ICNSS_FW_READY, &priv->state))
		seq_puts(s, "Firmware is not ready yet!, wait for FW READY\n");

	return 0;
}

static ssize_t icnss_regwrite_write(struct file *fp,
				    const char __user *user_buf,
				    size_t count, loff_t *off)
{
	struct icnss_priv *priv =
		((struct seq_file *)fp->private_data)->private;
	char buf[64];
	char *sptr, *token;
	unsigned int len = 0;
	uint32_t reg_offset, mem_type, reg_val;
	const char *delim = " ";
	int ret = 0;

	if (!test_bit(ICNSS_FW_READY, &priv->state) ||
	    !test_bit(ICNSS_POWER_ON, &priv->state))
		return -EINVAL;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &mem_type))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_offset))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_val))
		return -EINVAL;

	ret = wlfw_athdiag_write_send_sync_msg(priv, reg_offset, mem_type,
					       sizeof(uint32_t),
					       (uint8_t *)&reg_val);
	if (ret)
		return ret;

	return count;
}

static int icnss_regwrite_open(struct inode *inode, struct file *file)
{
	return single_open(file, icnss_regwrite_show, inode->i_private);
}

static const struct file_operations icnss_regwrite_fops = {
	.read		= seq_read,
	.write          = icnss_regwrite_write,
	.open           = icnss_regwrite_open,
	.owner          = THIS_MODULE,
	.llseek		= seq_lseek,
};

static int icnss_regread_show(struct seq_file *s, void *data)
{
	struct icnss_priv *priv = s->private;

	if (!priv->diag_reg_read_buf) {
		seq_puts(s, "Usage: echo <mem_type> <offset> <data_len> > <debugfs>/icnss/reg_read\n");

		if (!test_bit(ICNSS_FW_READY, &priv->state))
			seq_puts(s, "Firmware is not ready yet!, wait for FW READY\n");

		return 0;
	}

	seq_printf(s, "REGREAD: Addr 0x%x Type 0x%x Length 0x%x\n",
		   priv->diag_reg_read_addr, priv->diag_reg_read_mem_type,
		   priv->diag_reg_read_len);

	seq_hex_dump(s, "", DUMP_PREFIX_OFFSET, 32, 4, priv->diag_reg_read_buf,
		     priv->diag_reg_read_len, false);

	priv->diag_reg_read_len = 0;
	kfree(priv->diag_reg_read_buf);
	priv->diag_reg_read_buf = NULL;

	return 0;
}

static ssize_t icnss_regread_write(struct file *fp, const char __user *user_buf,
				size_t count, loff_t *off)
{
	struct icnss_priv *priv =
		((struct seq_file *)fp->private_data)->private;
	char buf[64];
	char *sptr, *token;
	unsigned int len = 0;
	uint32_t reg_offset, mem_type;
	uint32_t data_len = 0;
	uint8_t *reg_buf = NULL;
	const char *delim = " ";
	int ret = 0;

	if (!test_bit(ICNSS_FW_READY, &priv->state) ||
	    !test_bit(ICNSS_POWER_ON, &priv->state))
		return -EINVAL;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &mem_type))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (!sptr)
		return -EINVAL;

	if (kstrtou32(token, 0, &reg_offset))
		return -EINVAL;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;

	if (kstrtou32(token, 0, &data_len))
		return -EINVAL;

	if (data_len == 0 ||
	    data_len > QMI_WLFW_MAX_DATA_SIZE_V01)
		return -EINVAL;

	reg_buf = kzalloc(data_len, GFP_KERNEL);
	if (!reg_buf)
		return -ENOMEM;

	ret = wlfw_athdiag_read_send_sync_msg(priv, reg_offset,
					      mem_type, data_len,
					      reg_buf);
	if (ret) {
		kfree(reg_buf);
		return ret;
	}

	priv->diag_reg_read_addr = reg_offset;
	priv->diag_reg_read_mem_type = mem_type;
	priv->diag_reg_read_len = data_len;
	priv->diag_reg_read_buf = reg_buf;

	return count;
}

static int icnss_regread_open(struct inode *inode, struct file *file)
{
	return single_open(file, icnss_regread_show, inode->i_private);
}

static const struct file_operations icnss_regread_fops = {
	.read           = seq_read,
	.write          = icnss_regread_write,
	.open           = icnss_regread_open,
	.owner          = THIS_MODULE,
	.llseek         = seq_lseek,
};

static int icnss_debugfs_create(struct icnss_priv *priv)
{
	int ret = 0;
	struct dentry *root_dentry;

	root_dentry = debugfs_create_dir("icnss", 0);

	if (IS_ERR(root_dentry)) {
		ret = PTR_ERR(root_dentry);
		icnss_pr_err("Unable to create debugfs %d\n", ret);
		goto out;
	}

	priv->root_dentry = root_dentry;

	debugfs_create_file("test_mode", 0644, root_dentry, priv,
			    &icnss_test_mode_fops);

	debugfs_create_file("stats", 0644, root_dentry, priv,
			    &icnss_stats_fops);
	debugfs_create_file("reg_read", 0600, root_dentry, priv,
			    &icnss_regread_fops);
	debugfs_create_file("reg_write", 0644, root_dentry, priv,
			    &icnss_regwrite_fops);

out:
	return ret;
}

static void icnss_debugfs_destroy(struct icnss_priv *priv)
{
	debugfs_remove_recursive(priv->root_dentry);
}

static int icnss_get_vbatt_info(struct icnss_priv *priv)
{
	struct qpnp_adc_tm_chip *adc_tm_dev = NULL;
	struct qpnp_vadc_chip *vadc_dev = NULL;
	int ret = 0;

	if (test_bit(VBATT_DISABLE, &quirks)) {
		icnss_pr_dbg("VBATT feature is disabled\n");
		return ret;
	}

	adc_tm_dev = qpnp_get_adc_tm(&priv->pdev->dev, "icnss");
	if (PTR_ERR(adc_tm_dev) == -EPROBE_DEFER) {
		icnss_pr_err("adc_tm_dev probe defer\n");
		return -EPROBE_DEFER;
	}

	if (IS_ERR(adc_tm_dev)) {
		ret = PTR_ERR(adc_tm_dev);
		icnss_pr_err("Not able to get ADC dev, VBATT monitoring is disabled: %d\n",
			     ret);
		return ret;
	}

	vadc_dev = qpnp_get_vadc(&priv->pdev->dev, "icnss");
	if (PTR_ERR(vadc_dev) == -EPROBE_DEFER) {
		icnss_pr_err("vadc_dev probe defer\n");
		return -EPROBE_DEFER;
	}

	if (IS_ERR(vadc_dev)) {
		ret = PTR_ERR(vadc_dev);
		icnss_pr_err("Not able to get VADC dev, VBATT monitoring is disabled: %d\n",
			     ret);
		return ret;
	}

	priv->adc_tm_dev = adc_tm_dev;
	priv->vadc_dev = vadc_dev;

	return 0;
}

static int icnss_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	int i;
	struct device *dev = &pdev->dev;
	struct icnss_priv *priv;

	if (penv) {
		icnss_pr_err("Driver is already initialized\n");
		return -EEXIST;
	}

	icnss_pr_dbg("Platform driver probe\n");

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->magic = ICNSS_MAGIC;
	dev_set_drvdata(dev, priv);

	priv->pdev = pdev;

	ret = icnss_get_vbatt_info(priv);
	if (ret == -EPROBE_DEFER)
		goto out;

	memcpy(priv->vreg_info, icnss_vreg_info, sizeof(icnss_vreg_info));
	for (i = 0; i < ICNSS_VREG_INFO_SIZE; i++) {
		ret = icnss_get_vreg_info(dev, &priv->vreg_info[i]);

		if (ret)
			goto out;
	}

	memcpy(priv->clk_info, icnss_clk_info, sizeof(icnss_clk_info));
	for (i = 0; i < ICNSS_CLK_INFO_SIZE; i++) {
		ret = icnss_get_clk_info(dev, &priv->clk_info[i]);
		if (ret)
			goto out;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "membase");
	if (!res) {
		icnss_pr_err("Memory base not found in DT\n");
		ret = -EINVAL;
		goto out;
	}

	priv->mem_base_pa = res->start;
	priv->mem_base_va = devm_ioremap(dev, priv->mem_base_pa,
					 resource_size(res));
	if (!priv->mem_base_va) {
		icnss_pr_err("Memory base ioremap failed: phy addr: %pa\n",
			     &priv->mem_base_pa);
		ret = -EINVAL;
		goto out;
	}
	icnss_pr_dbg("MEM_BASE pa: %pa, va: 0x%p\n", &priv->mem_base_pa,
		     priv->mem_base_va);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "mpm_config");
	if (!res) {
		icnss_pr_err("MPM Config not found\n");
		ret = -EINVAL;
		goto out;
	}
	priv->mpm_config_pa = res->start;
	priv->mpm_config_va = devm_ioremap(dev, priv->mpm_config_pa,
					   resource_size(res));
	if (!priv->mpm_config_va) {
		icnss_pr_err("MPM Config ioremap failed, phy addr: %pa\n",
			     &priv->mpm_config_pa);
		ret = -EINVAL;
		goto out;
	}

	icnss_pr_dbg("MPM_CONFIG pa: %pa, va: 0x%p\n", &priv->mpm_config_pa,
		     priv->mpm_config_va);

	for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++) {
		res = platform_get_resource(priv->pdev, IORESOURCE_IRQ, i);
		if (!res) {
			icnss_pr_err("Fail to get IRQ-%d\n", i);
			ret = -ENODEV;
			goto out;
		} else {
			priv->ce_irqs[i] = res->start;
		}
	}

	ret = of_property_read_u32(dev->of_node, "qcom,wlan-msa-memory",
				   &priv->msa_mem_size);

	if (ret || priv->msa_mem_size == 0) {
		icnss_pr_err("Fail to get MSA Memory Size: %u, ret: %d\n",
			     priv->msa_mem_size, ret);
		goto out;
	}

	priv->msa_va = dmam_alloc_coherent(&pdev->dev, priv->msa_mem_size,
					   &priv->msa_pa, GFP_KERNEL);
	if (!priv->msa_va) {
		icnss_pr_err("DMA alloc failed for MSA\n");
		ret = -ENOMEM;
		goto out;
	}
	icnss_pr_dbg("MSA pa: %pa, MSA va: 0x%p\n", &priv->msa_pa,
		     priv->msa_va);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "smmu_iova_base");
	if (!res) {
		icnss_pr_err("SMMU IOVA base not found\n");
	} else {
		priv->smmu_iova_start = res->start;
		priv->smmu_iova_len = resource_size(res);
		icnss_pr_dbg("SMMU IOVA start: %pa, len: %zu\n",
			     &priv->smmu_iova_start, priv->smmu_iova_len);

		res = platform_get_resource_byname(pdev,
						   IORESOURCE_MEM,
						   "smmu_iova_ipa");
		if (!res) {
			icnss_pr_err("SMMU IOVA IPA not found\n");
		} else {
			priv->smmu_iova_ipa_start = res->start;
			priv->smmu_iova_ipa_len = resource_size(res);
			icnss_pr_dbg("SMMU IOVA IPA start: %pa, len: %zu\n",
				     &priv->smmu_iova_ipa_start,
				     priv->smmu_iova_ipa_len);
		}

		ret = icnss_smmu_init(priv);
		if (ret < 0) {
			icnss_pr_err("SMMU init failed, err = %d, start: %pad, len: %zx\n",
				     ret, &priv->smmu_iova_start,
				     priv->smmu_iova_len);
			goto out;
		}
	}

	spin_lock_init(&priv->event_lock);
	spin_lock_init(&priv->on_off_lock);

	priv->event_wq = alloc_workqueue("icnss_driver_event", WQ_UNBOUND, 1);
	if (!priv->event_wq) {
		icnss_pr_err("Workqueue creation failed\n");
		ret = -EFAULT;
		goto out_smmu_deinit;
	}

	INIT_WORK(&priv->event_work, icnss_driver_event_work);
	INIT_WORK(&priv->qmi_recv_msg_work, icnss_qmi_wlfw_clnt_notify_work);
	INIT_LIST_HEAD(&priv->event_list);

	ret = qmi_svc_event_notifier_register(WLFW_SERVICE_ID_V01,
					      WLFW_SERVICE_VERS_V01,
					      WLFW_SERVICE_INS_ID_V01,
					      &wlfw_clnt_nb);
	if (ret < 0) {
		icnss_pr_err("Notifier register failed: %d\n", ret);
		goto out_destroy_wq;
	}

	icnss_enable_recovery(priv);

	icnss_debugfs_create(priv);

	penv = priv;

	icnss_pr_info("Platform driver probed successfully\n");

	return 0;

out_destroy_wq:
	destroy_workqueue(priv->event_wq);
out_smmu_deinit:
	icnss_smmu_deinit(priv);
out:
	dev_set_drvdata(dev, NULL);

	return ret;
}

static int icnss_remove(struct platform_device *pdev)
{
	icnss_pr_info("Removing driver: state: 0x%lx\n", penv->state);

	icnss_debugfs_destroy(penv);

	icnss_modem_ssr_unregister_notifier(penv);

	icnss_pdr_unregister_notifier(penv);

	qmi_svc_event_notifier_unregister(WLFW_SERVICE_ID_V01,
					  WLFW_SERVICE_VERS_V01,
					  WLFW_SERVICE_INS_ID_V01,
					  &wlfw_clnt_nb);
	if (penv->event_wq)
		destroy_workqueue(penv->event_wq);

	icnss_hw_power_off(penv);

	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int icnss_pm_suspend(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for pm suspend: dev %p, data %p, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	icnss_pr_dbg("PM Suspend, state: 0x%lx\n", priv->state);

	if (!priv->ops || !priv->ops->pm_suspend ||
	    !test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto out;

	ret = priv->ops->pm_suspend(dev);

out:
	if (ret == 0) {
		priv->stats.pm_suspend++;
		set_bit(ICNSS_PM_SUSPEND, &priv->state);
	} else {
		priv->stats.pm_suspend_err++;
	}
	return ret;
}

static int icnss_pm_resume(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for pm resume: dev %p, data %p, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	icnss_pr_dbg("PM resume, state: 0x%lx\n", priv->state);

	if (!priv->ops || !priv->ops->pm_resume ||
	    !test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto out;

	ret = priv->ops->pm_resume(dev);

out:
	if (ret == 0) {
		priv->stats.pm_resume++;
		clear_bit(ICNSS_PM_SUSPEND, &priv->state);
	} else {
		priv->stats.pm_resume_err++;
	}
	return ret;
}

static int icnss_pm_suspend_noirq(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for pm suspend_noirq: dev %p, data %p, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	icnss_pr_dbg("PM suspend_noirq, state: 0x%lx\n", priv->state);

	if (!priv->ops || !priv->ops->suspend_noirq ||
	    !test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto out;

	ret = priv->ops->suspend_noirq(dev);

out:
	if (ret == 0) {
		priv->stats.pm_suspend_noirq++;
		set_bit(ICNSS_PM_SUSPEND_NOIRQ, &priv->state);
	} else {
		priv->stats.pm_suspend_noirq_err++;
	}
	return ret;
}

static int icnss_pm_resume_noirq(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);
	int ret = 0;

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata for pm resume_noirq: dev %p, data %p, magic 0x%x\n",
			     dev, priv, priv->magic);
		return -EINVAL;
	}

	icnss_pr_dbg("PM resume_noirq, state: 0x%lx\n", priv->state);

	if (!priv->ops || !priv->ops->resume_noirq ||
	    !test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto out;

	ret = priv->ops->resume_noirq(dev);

out:
	if (ret == 0) {
		priv->stats.pm_resume_noirq++;
		clear_bit(ICNSS_PM_SUSPEND_NOIRQ, &priv->state);
	} else {
		priv->stats.pm_resume_noirq_err++;
	}
	return ret;
}
#endif

static const struct dev_pm_ops icnss_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(icnss_pm_suspend,
				icnss_pm_resume)
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(icnss_pm_suspend_noirq,
				      icnss_pm_resume_noirq)
};

static const struct of_device_id icnss_dt_match[] = {
	{.compatible = "qcom,icnss"},
	{}
};

MODULE_DEVICE_TABLE(of, icnss_dt_match);

static struct platform_driver icnss_driver = {
	.probe  = icnss_probe,
	.remove = icnss_remove,
	.driver = {
		.name = "icnss",
		.pm = &icnss_pm_ops,
		.owner = THIS_MODULE,
		.of_match_table = icnss_dt_match,
	},
};

#ifdef CONFIG_ICNSS_DEBUG
static void __init icnss_ipc_log_long_context_init(void)
{
	icnss_ipc_log_long_context = ipc_log_context_create(NUM_REG_LOG_PAGES,
							   "icnss_long", 0);
	if (!icnss_ipc_log_long_context)
		icnss_pr_err("Unable to create register log context\n");
}

static void __exit icnss_ipc_log_long_context_destroy(void)
{
	ipc_log_context_destroy(icnss_ipc_log_long_context);
	icnss_ipc_log_long_context = NULL;
}
#else

static void __init icnss_ipc_log_long_context_init(void) { }
static void __exit icnss_ipc_log_long_context_destroy(void) { }
#endif

static int __init icnss_initialize(void)
{
	icnss_ipc_log_context = ipc_log_context_create(NUM_LOG_PAGES,
						       "icnss", 0);
	if (!icnss_ipc_log_context)
		icnss_pr_err("Unable to create log context\n");

	icnss_ipc_log_long_context_init();

	return platform_driver_register(&icnss_driver);
}

static void __exit icnss_exit(void)
{
	platform_driver_unregister(&icnss_driver);
	ipc_log_context_destroy(icnss_ipc_log_context);
	icnss_ipc_log_context = NULL;

	icnss_ipc_log_long_context_destroy();
}


module_init(icnss_initialize);
module_exit(icnss_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DEVICE "iCNSS CORE platform driver");
