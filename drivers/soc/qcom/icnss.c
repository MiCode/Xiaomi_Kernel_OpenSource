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
#include <linux/msm-bus.h>
#include <soc/qcom/memory_dump.h>
#include <soc/qcom/icnss.h>
#include <soc/qcom/msm_qmi_interface.h>
#include <soc/qcom/secure_buffer.h>

#include "wlan_firmware_service_v01.h"

#define ICNSS_PANIC			1
#define WLFW_TIMEOUT_MS			3000
#define WLFW_SERVICE_INS_ID_V01		0
#define MAX_PROP_SIZE			32
#define NUM_LOG_PAGES			4

/*
 * Registers: MPM2_PSHOLD
 * Base Address: 0x10AC000
 */
#define MPM_WCSSAON_CONFIG_OFFSET				0x18
#define MPM_WCSSAON_CONFIG_ARES_N				BIT(0)
#define MPM_WCSSAON_CONFIG_WLAN_DISABLE				BIT(1)
#define MPM_WCSSAON_CONFIG_FORCE_ACTIVE				BIT(14)
#define MPM_WCSSAON_CONFIG_FORCE_XO_ENABLE			BIT(19)
#define MPM_WCSSAON_CONFIG_DISCONNECT_CLR			BIT(21)

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

#define icnss_ipc_log_string(_x...) do {				\
	if (icnss_ipc_log_context)					\
		ipc_log_string(icnss_ipc_log_context, _x);		\
	} while (0)

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

#ifdef ICNSS_PANIC
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
};

#define ICNSS_QUIRKS_DEFAULT 0

unsigned long quirks = ICNSS_QUIRKS_DEFAULT;
module_param(quirks, ulong, 0600);

void *icnss_ipc_log_context;

enum icnss_driver_event_type {
	ICNSS_DRIVER_EVENT_SERVER_ARRIVE,
	ICNSS_DRIVER_EVENT_SERVER_EXIT,
	ICNSS_DRIVER_EVENT_FW_READY_IND,
	ICNSS_DRIVER_EVENT_REGISTER_DRIVER,
	ICNSS_DRIVER_EVENT_UNREGISTER_DRIVER,
	ICNSS_DRIVER_EVENT_MAX,
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
	ICNSS_SUSPEND,
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
};

static struct icnss_priv {
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
	struct msm_bus_scale_pdata *bus_scale_table;
	uint32_t bus_client;
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
} *penv;

static void icnss_hw_write_reg(void *base, u32 offset, u32 val)
{
	writel_relaxed(val, base + offset);
	wmb(); /* Ensure data is written to hardware register */
}

static u32 icnss_hw_read_reg(void *base, u32 offset)
{
	u32 rdata = readl_relaxed(base + offset);

	icnss_pr_dbg(" READ: offset: 0x%06x 0x%08x\n", offset, rdata);

	return rdata;
}

static void icnss_hw_write_reg_field(void *base, u32 offset, u32 mask, u32 val)
{
	u32 shift = find_first_bit((void *)&mask, 32);
	u32 rdata = readl_relaxed(base + offset);

	val = (rdata & ~mask) | (val << shift);

	icnss_pr_dbg("WRITE: offset: 0x%06x 0x%08x -> 0x%08x\n",
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

	icnss_pr_dbg(" POLL: offset: 0x%06x 0x%08x == 0x%08x & 0x%08x\n",
		     offset, val, rdata, mask);

	while ((rdata & mask) != val) {
		if (retry != 0 && r >= retry) {
			icnss_pr_err(" POLL FAILED: offset: 0x%06x 0x%08x == 0x%08x & 0x%08x\n",
				     offset, val, rdata, mask);

			return -EIO;
		}

		r++;
		udelay(usecs);
		rdata = readl_relaxed(base + offset);

		if (retry)
			icnss_pr_dbg(" POLL: offset: 0x%06x 0x%08x == 0x%08x & 0x%08x\n",
				     offset, val, rdata, mask);

	}

	return 0;
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
	case ICNSS_DRIVER_EVENT_MAX:
		return "EVENT_MAX";
	}

	return "UNKNOWN";
};

static int icnss_driver_event_post(enum icnss_driver_event_type type,
				   bool sync, void *data)
{
	struct icnss_driver_event *event;
	unsigned long flags;
	int gfp = GFP_KERNEL;
	int ret = 0;

	icnss_pr_dbg("Posting event: %s(%d)%s, state: 0x%lx\n",
		     icnss_driver_event_to_str(type), type,
		     sync ? "-sync" : "", penv->state);

	if (type >= ICNSS_DRIVER_EVENT_MAX) {
		icnss_pr_err("Invalid Event type: %d, can't post", type);
		return -EINVAL;
	}

	if (in_interrupt() || irqs_disabled())
		gfp = GFP_ATOMIC;

	event = kzalloc(sizeof(*event), gfp);
	if (event == NULL)
		return -ENOMEM;

	event->type = type;
	event->data = data;
	init_completion(&event->complete);
	event->sync = sync;

	spin_lock_irqsave(&penv->event_lock, flags);
	list_add_tail(&event->list, &penv->event_list);
	spin_unlock_irqrestore(&penv->event_lock, flags);

	penv->stats.events[type].posted++;
	queue_work(penv->event_wq, &penv->event_work);
	if (sync) {
		ret = wait_for_completion_interruptible(&event->complete);
		if (ret == 0)
			ret = event->ret;
		kfree(event);
	}

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
		icnss_pr_err("Failed to decode message: %d, msg_len: %u!\n",
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

int icnss_hw_reset_wlan_ss_power_down(struct icnss_priv *priv)
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

int icnss_hw_reset_common_ss_power_down(struct icnss_priv *priv)
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

int icnss_hw_reset_wlan_rfactrl_power_down(struct icnss_priv *priv)
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

void icnss_hw_wsi_cmd_error_recovery(struct icnss_priv *priv)
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

u32 icnss_hw_rf_register_read_command(struct icnss_priv *priv, u32 addr)
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

int icnss_hw_reset_rf_reset_cmd(struct icnss_priv *priv)
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
		icnss_hw_wsi_cmd_error_recovery(priv);
	}

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_USE_PMM_WSI, 0);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_RF_RESET, 0);

	return 0;
}

int icnss_hw_reset_xo_disable_cmd(struct icnss_priv *priv)
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
		icnss_hw_wsi_cmd_error_recovery(priv);
	}

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_USE_PMM_WSI, 0);

	icnss_hw_write_reg_field(priv->mem_base_va, PMM_WSI_CMD_OFFSET,
				 PMM_WSI_CMD_SW_XO_DIS, 0);

	return 0;
}

int icnss_hw_reset(struct icnss_priv *priv)
{
	u32 rdata;
	u32 rdata1;
	int i;

	if (test_bit(HW_ONLY_TOP_LEVEL_RESET, &quirks))
		goto top_level_reset;

	icnss_pr_dbg("RESET: START, state: 0x%lx\n", priv->state);

	icnss_hw_write_reg_field(priv->mpm_config_va, MPM_WCSSAON_CONFIG_OFFSET,
				 MPM_WCSSAON_CONFIG_FORCE_ACTIVE, 1);

	icnss_hw_poll_reg_field(priv->mem_base_va, SR_WCSSAON_SR_LSB_OFFSET,
				SR_WCSSAON_SR_LSB_RETENTION_STATUS, 1, 10,
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
		ICNSS_ASSERT(false);

	icnss_hw_write_reg_field(priv->mpm_config_va, MPM_WCSSAON_CONFIG_OFFSET,
				 MPM_WCSSAON_CONFIG_DISCONNECT_CLR, 0x1);

	icnss_hw_reset_wlan_ss_power_down(priv);

	icnss_hw_reset_common_ss_power_down(priv);

	icnss_hw_reset_wlan_rfactrl_power_down(priv);

	icnss_hw_reset_rf_reset_cmd(priv);

	icnss_hw_reset_xo_disable_cmd(priv);

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

	icnss_pr_dbg("Power on: state: 0x%lx\n", priv->state);

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

	icnss_pr_dbg("Power off: 0x%lx\n", priv->state);

	spin_lock_irqsave(&priv->on_off_lock, flags);
	if (!test_bit(ICNSS_POWER_ON, &priv->state)) {
		spin_unlock_irqrestore(&priv->on_off_lock, flags);
		return ret;
	}
	clear_bit(ICNSS_POWER_ON, &priv->state);
	spin_unlock_irqrestore(&priv->on_off_lock, flags);

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

	return icnss_hw_power_off(priv);
}
EXPORT_SYMBOL(icnss_power_off);

int icnss_map_msa_permissions(struct icnss_priv *priv, u32 index)
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
		icnss_pr_err("region %u hyp_assign_phys failed IPA=%pa size=%u err=%d\n",
			     index, &addr, size, ret);
		goto out;
	}
	icnss_pr_dbg("hypervisor map for region %u: source=%x, dest_nelems=%d, dest[0]=%x, dest[1]=%x, dest[2]=%x\n",
		     index, source_vmlist[0], dest_nelems,
		     dest_vmids[0], dest_vmids[1], dest_vmids[2]);
out:
	return ret;

}

int icnss_unmap_msa_permissions(struct icnss_priv *priv, u32 index)
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
		icnss_pr_err("region %u hyp_assign_phys failed IPA=%pa size=%u err=%d\n",
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
	int ret = 0;

	ret = icnss_map_msa_permissions(priv, 0);
	if (ret)
		return ret;

	ret = icnss_map_msa_permissions(priv, 1);
	if (ret)
		goto err_map_msa;

	return ret;

err_map_msa:
	icnss_unmap_msa_permissions(priv, 0);
	return ret;
}

static void icnss_remove_msa_permissions(struct icnss_priv *priv)
{
	icnss_unmap_msa_permissions(priv, 0);
	icnss_unmap_msa_permissions(priv, 1);
}

static int wlfw_msa_mem_info_send_sync_msg(void)
{
	int ret = 0;
	int i;
	struct wlfw_msa_info_req_msg_v01 req;
	struct wlfw_msa_info_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

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
		icnss_pr_err("Send req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI request failed %d %d\n",
			resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		penv->stats.msa_info_err++;
		goto out;
	}

	icnss_pr_dbg("Receive mem_region_info_len: %d\n",
		     resp.mem_region_info_len);

	if (resp.mem_region_info_len > 2) {
		icnss_pr_err("Invalid memory region length received%d\n",
			     resp.mem_region_info_len);
		ret = -EINVAL;
		penv->stats.msa_info_err++;
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
		icnss_pr_dbg("Memory Region: %d Addr: 0x%x Size: %d Flag: %d\n",
			 i, (unsigned int)penv->icnss_mem_region[i].reg_addr,
			 penv->icnss_mem_region[i].size,
			 penv->icnss_mem_region[i].secure_flag);
	}

out:
	return ret;
}

static int wlfw_msa_ready_send_sync_msg(void)
{
	int ret;
	struct wlfw_msa_ready_req_msg_v01 req;
	struct wlfw_msa_ready_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

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
		penv->stats.msa_ready_err++;
		icnss_pr_err("Send req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI request failed %d %d\n",
			resp.resp.result, resp.resp.error);
		penv->stats.msa_ready_err++;
		ret = resp.resp.result;
		goto out;
	}
	penv->stats.msa_ready_resp++;
out:
	return ret;
}

static int wlfw_ind_register_send_sync_msg(void)
{
	int ret;
	struct wlfw_ind_register_req_msg_v01 req;
	struct wlfw_ind_register_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Sending indication register message, state: 0x%lx\n",
		     penv->state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

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
	penv->stats.ind_register_resp++;
	if (ret < 0) {
		icnss_pr_err("Send req failed %d\n", ret);
		penv->stats.ind_register_err++;
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI request failed %d %d\n",
		       resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		penv->stats.ind_register_err++;
		goto out;
	}
out:
	return ret;
}

static int wlfw_cap_send_sync_msg(void)
{
	int ret;
	struct wlfw_cap_req_msg_v01 req;
	struct wlfw_cap_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

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
		icnss_pr_err("Send req failed %d\n", ret);
		penv->stats.cap_err++;
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI request failed %d %d\n",
		       resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		penv->stats.cap_err++;
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
out:
	return ret;
}

static int wlfw_wlan_mode_send_sync_msg(enum wlfw_driver_mode_enum_v01 mode)
{
	int ret;
	struct wlfw_wlan_mode_req_msg_v01 req;
	struct wlfw_wlan_mode_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

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
		icnss_pr_err("Send req failed %d\n", ret);
		penv->stats.mode_req_err++;
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI request failed %d %d\n",
		       resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		penv->stats.mode_req_err++;
		goto out;
	}
	penv->stats.mode_resp++;
out:
	return ret;
}

static int wlfw_wlan_cfg_send_sync_msg(struct wlfw_wlan_cfg_req_msg_v01 *data)
{
	int ret;
	struct wlfw_wlan_cfg_req_msg_v01 req;
	struct wlfw_wlan_cfg_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt) {
		return -ENODEV;
		goto out;
	}

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
		icnss_pr_err("Send req failed %d\n", ret);
		penv->stats.cfg_req_err++;
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI request failed %d %d\n",
		       resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		penv->stats.cfg_req_err++;
		goto out;
	}
	penv->stats.cfg_resp++;
out:
	return ret;
}

static int wlfw_ini_send_sync_msg(bool enable_fw_log)
{
	int ret;
	struct wlfw_ini_req_msg_v01 req;
	struct wlfw_ini_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

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
		icnss_pr_err("send req failed %d\n", ret);
		penv->stats.ini_req_err++;
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI request failed %d %d\n",
		       resp.resp.result, resp.resp.error);
		ret = resp.resp.result;
		penv->stats.ini_req_err++;
		goto out;
	}
	penv->stats.ini_resp++;
out:
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
					false, NULL);
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

	penv->wlfw_clnt = qmi_handle_create(icnss_qmi_wlfw_clnt_notify, penv);
	if (!penv->wlfw_clnt) {
		icnss_pr_err("QMI client handle create failed\n");
		ret = -ENOMEM;
		goto out;
	}

	ret = qmi_connect_to_service(penv->wlfw_clnt,
					WLFW_SERVICE_ID_V01,
					WLFW_SERVICE_VERS_V01,
					WLFW_SERVICE_INS_ID_V01);
	if (ret < 0) {
		icnss_pr_err("Server not found : %d\n", ret);
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
	if (ret < 0) {
		icnss_pr_err("Failed to send indication message: %d\n", ret);
		goto err_power_on;
	}

	if (!penv->msa_va) {
		icnss_pr_err("Invalid MSA address\n");
		ret = -EINVAL;
		goto err_power_on;
	}

	ret = wlfw_msa_mem_info_send_sync_msg();
	if (ret < 0) {
		icnss_pr_err("Failed to send MSA info: %d\n", ret);
		goto err_power_on;
	}
	ret = icnss_setup_msa_permissions(penv);
	if (ret < 0) {
		icnss_pr_err("Failed to setup msa permissions: %d\n",
			     ret);
		goto err_power_on;
	}
	ret = wlfw_msa_ready_send_sync_msg();
	if (ret < 0) {
		icnss_pr_err("Failed to send MSA ready : %d\n", ret);
		goto err_setup_msa;
	}

	ret = wlfw_cap_send_sync_msg();
	if (ret < 0) {
		icnss_pr_err("Failed to get capability: %d\n", ret);
		goto err_setup_msa;
	}
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

	qmi_handle_destroy(penv->wlfw_clnt);

	penv->state = 0;
	penv->wlfw_clnt = NULL;

	return 0;
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

	if (!penv->ops || !penv->ops->probe)
		goto out;

	icnss_hw_power_on(penv);

	ret = penv->ops->probe(&penv->pdev->dev);
	if (ret < 0) {
		icnss_pr_err("Driver probe failed: %d\n", ret);
		goto out_power_off;
	}

	set_bit(ICNSS_DRIVER_PROBED, &penv->state);

	return 0;

out_power_off:
	icnss_hw_power_off(penv);
out:
	return ret;
}

static int icnss_driver_event_register_driver(void *data)
{
	int ret = 0;

	if (penv->ops) {
		ret = -EEXIST;
		goto out;
	}

	penv->ops = data;

	if (test_bit(SKIP_QMI, &quirks))
		set_bit(ICNSS_FW_READY, &penv->state);

	if (!test_bit(ICNSS_FW_READY, &penv->state)) {
		icnss_pr_dbg("FW is not ready yet, state: 0x%lx!\n",
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

static void icnss_driver_event_work(struct work_struct *work)
{
	struct icnss_driver_event *event;
	unsigned long flags;
	int ret;

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
		default:
			icnss_pr_err("Invalid Event type: %d", event->type);
			kfree(event);
			continue;
		}

		penv->stats.events[event->type].processed++;

		if (event->sync) {
			event->ret = ret;
			complete(&event->complete);
		} else
			kfree(event);

		spin_lock_irqsave(&penv->event_lock, flags);
	}
	spin_unlock_irqrestore(&penv->event_lock, flags);
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
					      false, NULL);
		break;

	case QMI_SERVER_EXIT:
		ret = icnss_driver_event_post(ICNSS_DRIVER_EVENT_SERVER_EXIT,
					      false, NULL);
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
				      true, ops);

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
				      true, NULL);
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
	if (ret) {
		icnss_pr_err("Failed to send cfg, ret = %d\n", ret);
		goto out;
	}
skip:
	ret = wlfw_wlan_mode_send_sync_msg(mode);
	if (ret)
		icnss_pr_err("Failed to send mode, ret = %d\n", ret);
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
		icnss_pr_err("iova_addr is NULL, paddr %pa, size %zu",
			     &paddr, size);
		return -EINVAL;
	}

	len = roundup(size + paddr - rounddown(paddr, PAGE_SIZE), PAGE_SIZE);
	iova = roundup(penv->smmu_iova_ipa_start, PAGE_SIZE);

	if (iova >= priv->smmu_iova_ipa_start + priv->smmu_iova_ipa_len) {
		icnss_pr_err("No IOVA space to map, iova %lx, smmu_iova_ipa_start %pad, smmu_iova_ipa_len %zu",
			     iova,
			     &priv->smmu_iova_ipa_start,
			     priv->smmu_iova_ipa_len);
		return -ENOMEM;
	}

	ret = iommu_map(priv->smmu_mapping->domain, iova,
			rounddown(paddr, PAGE_SIZE), len,
			IOMMU_READ | IOMMU_WRITE);
	if (ret) {
		icnss_pr_err("PA to IOVA mapping failed, ret %d!", ret);
		return ret;
	}

	priv->smmu_iova_ipa_start = iova + len;
	*iova_addr = (uint32_t)(iova + paddr - rounddown(paddr, PAGE_SIZE));

	return 0;
}
EXPORT_SYMBOL(icnss_smmu_map);

static int icnss_bw_vote(struct icnss_priv *priv, int index)
{
	int ret = 0;

	icnss_pr_dbg("Vote %d for msm_bus, state 0x%lx\n",
		     index, priv->state);
	ret = msm_bus_scale_client_update_request(priv->bus_client, index);
	if (ret)
		icnss_pr_err("Fail to vote %d: ret %d, state 0x%lx!\n",
			     index, ret, priv->state);

	return ret;
}

static int icnss_bw_init(struct icnss_priv *priv)
{
	int ret = 0;

	priv->bus_scale_table = msm_bus_cl_get_pdata(priv->pdev);
	if (!priv->bus_scale_table) {
		icnss_pr_err("Missing entry for msm_bus scale table\n");
		return -EINVAL;
	}

	priv->bus_client = msm_bus_scale_register_client(priv->bus_scale_table);
	if (!priv->bus_client) {
		icnss_pr_err("Fail to register with bus_scale client\n");
		ret = -EINVAL;
		goto out;
	}

	ret = icnss_bw_vote(priv, 1);
	if (ret)
		goto out;

	return 0;

out:
	msm_bus_cl_clear_pdata(priv->bus_scale_table);
	return ret;
}

static void icnss_bw_deinit(struct icnss_priv *priv)
{
	if (!priv)
		return;

	if (priv->bus_client) {
		icnss_bw_vote(priv, 0);
		msm_bus_scale_unregister_client(priv->bus_client);
	}

	if (priv->bus_scale_table)
		msm_bus_cl_clear_pdata(priv->bus_scale_table);
}

static int icnss_smmu_init(struct icnss_priv *priv)
{
	struct dma_iommu_mapping *mapping;
	int disable_htw = 1;
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
				    DOMAIN_ATTR_COHERENT_HTW_DISABLE,
				    &disable_htw);
	if (ret < 0) {
		icnss_pr_err("Set disable_htw attribute failed, err = %d\n",
			     ret);
		goto set_attr_fail;
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

	if (IS_ERR(reg) == -EPROBE_DEFER) {
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
			icnss_pr_dbg("Ignoring clock %s: %d!\n", clk_info->name,
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
	int i;
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
		case ICNSS_SUSPEND:
			seq_puts(s, "DRIVER SUSPENDED");
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

out:
	return ret;
}

static void icnss_debugfs_destroy(struct icnss_priv *priv)
{
	debugfs_remove_recursive(priv->root_dentry);
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

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dev_set_drvdata(dev, priv);

	priv->pdev = pdev;

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
		icnss_pr_dbg("smmu_iova_start: %pa, smmu_iova_len: %zu\n",
			     &priv->smmu_iova_start,
			     priv->smmu_iova_len);

		res = platform_get_resource_byname(pdev,
						   IORESOURCE_MEM,
						   "smmu_iova_ipa");
		if (!res) {
			icnss_pr_err("SMMU IOVA IPA not found\n");
		} else {
			priv->smmu_iova_ipa_start = res->start;
			priv->smmu_iova_ipa_len = resource_size(res);
			icnss_pr_dbg("smmu_iova_ipa_start: %pa, smmu_iova_ipa_len: %zu\n",
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

		ret = icnss_bw_init(priv);
		if (ret)
			goto out_smmu_deinit;
	}

	spin_lock_init(&priv->event_lock);
	spin_lock_init(&priv->on_off_lock);

	priv->event_wq = alloc_workqueue("icnss_driver_event", WQ_UNBOUND, 1);
	if (!priv->event_wq) {
		icnss_pr_err("Workqueue creation failed\n");
		ret = -EFAULT;
		goto out_bw_deinit;
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

	icnss_debugfs_create(priv);

	penv = priv;

	icnss_pr_info("Platform driver probed successfully\n");

	return 0;

out_destroy_wq:
	destroy_workqueue(priv->event_wq);
out_bw_deinit:
	icnss_bw_deinit(priv);
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

	qmi_svc_event_notifier_unregister(WLFW_SERVICE_ID_V01,
					  WLFW_SERVICE_VERS_V01,
					  WLFW_SERVICE_INS_ID_V01,
					  &wlfw_clnt_nb);
	if (penv->event_wq)
		destroy_workqueue(penv->event_wq);

	icnss_bw_deinit(penv);

	icnss_hw_power_off(penv);

	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

static int icnss_suspend(struct platform_device *pdev,
			 pm_message_t state)
{
	int ret = 0;

	if (!penv) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Driver suspending, state: 0x%lx\n",
		     penv->state);

	if (!penv->ops || !penv->ops->suspend ||
	    !test_bit(ICNSS_DRIVER_PROBED, &penv->state))
		goto out;

	ret = penv->ops->suspend(&pdev->dev, state);

out:
	if (ret == 0)
		set_bit(ICNSS_SUSPEND, &penv->state);
	return ret;
}

static int icnss_resume(struct platform_device *pdev)
{
	int ret = 0;

	if (!penv) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_dbg("Driver resuming, state: 0x%lx\n",
		     penv->state);

	if (!penv->ops || !penv->ops->resume ||
	    !test_bit(ICNSS_DRIVER_PROBED, &penv->state))
		goto out;

	ret = penv->ops->resume(&pdev->dev);

out:
	if (ret == 0)
		clear_bit(ICNSS_SUSPEND, &penv->state);
	return ret;
}

static const struct of_device_id icnss_dt_match[] = {
	{.compatible = "qcom,icnss"},
	{}
};

MODULE_DEVICE_TABLE(of, icnss_dt_match);

static struct platform_driver icnss_driver = {
	.probe  = icnss_probe,
	.remove = icnss_remove,
	.suspend = icnss_suspend,
	.resume = icnss_resume,
	.driver = {
		.name = "icnss",
		.owner = THIS_MODULE,
		.of_match_table = icnss_dt_match,
	},
};

static int __init icnss_initialize(void)
{
	icnss_ipc_log_context = ipc_log_context_create(NUM_LOG_PAGES,
						       "icnss", 0);
	if (!icnss_ipc_log_context)
		icnss_pr_err("Unable to create log context\n");

	return platform_driver_register(&icnss_driver);
}

static void __exit icnss_exit(void)
{
	platform_driver_unregister(&icnss_driver);
	ipc_log_context_destroy(icnss_ipc_log_context);
	icnss_ipc_log_context = NULL;
}


module_init(icnss_initialize);
module_exit(icnss_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DEVICE "iCNSS CORE platform driver");
