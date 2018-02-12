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

#define pr_fmt(fmt) "icnss: " fmt

#include <asm/dma-iommu.h>
#include <linux/of_address.h>
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
#include <linux/etherdevice.h>
#include <linux/of_gpio.h>
#include <soc/qcom/memory_dump.h>
#include <soc/qcom/icnss.h>
#include <soc/qcom/msm_qmi_interface.h>
#include <soc/qcom/secure_buffer.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/service-locator.h>
#include <soc/qcom/service-notifier.h>
#include <soc/qcom/socinfo.h>
#include <soc/qcom/ramdump.h>

#include "wlan_firmware_service_v01.h"

#ifdef CONFIG_ICNSS_DEBUG
unsigned long qmi_timeout = 10000;
module_param(qmi_timeout, ulong, 0600);

#define WLFW_TIMEOUT_MS			qmi_timeout
#else
#define WLFW_TIMEOUT_MS			10000
#endif
#define WLFW_SERVICE_INS_ID_V01		0
#define WLFW_CLIENT_ID			0x4b4e454c
#define MAX_PROP_SIZE			32
#define NUM_LOG_PAGES			10
#define NUM_LOG_LONG_PAGES		4
#define ICNSS_MAGIC			0x5abc5abc

#define ICNSS_SERVICE_LOCATION_CLIENT_NAME			"ICNSS-WLAN"
#define ICNSS_WLAN_SERVICE_NAME					"wlan/fw"

#define ICNSS_THRESHOLD_HIGH		3600000
#define ICNSS_THRESHOLD_LOW		3450000
#define ICNSS_THRESHOLD_GUARD		20000

#define ICNSS_MAX_PROBE_CNT		2

#define icnss_ipc_log_string(_x...) do {				\
	if (icnss_ipc_log_context)					\
		ipc_log_string(icnss_ipc_log_context, _x);		\
	} while (0)

#define icnss_ipc_log_long_string(_x...) do {				\
	if (icnss_ipc_log_long_context)					\
		ipc_log_string(icnss_ipc_log_long_context, _x);		\
	} while (0)

#define icnss_pr_err(_fmt, ...) do {					\
	printk("%s" pr_fmt(_fmt), KERN_ERR, ##__VA_ARGS__);		\
	icnss_ipc_log_string("%s" pr_fmt(_fmt), "",			\
			     ##__VA_ARGS__);				\
	} while (0)

#define icnss_pr_warn(_fmt, ...) do {					\
	printk("%s" pr_fmt(_fmt), KERN_WARNING, ##__VA_ARGS__);		\
	icnss_ipc_log_string("%s" pr_fmt(_fmt), "",			\
			     ##__VA_ARGS__);				\
	} while (0)

#define icnss_pr_info(_fmt, ...) do {					\
	printk("%s" pr_fmt(_fmt), KERN_INFO, ##__VA_ARGS__);		\
	icnss_ipc_log_string("%s" pr_fmt(_fmt), "",			\
			     ##__VA_ARGS__);				\
	} while (0)

#if defined(CONFIG_DYNAMIC_DEBUG)
#define icnss_pr_dbg(_fmt, ...) do {					\
	pr_debug(_fmt, ##__VA_ARGS__);					\
	icnss_ipc_log_string(pr_fmt(_fmt), ##__VA_ARGS__);		\
	} while (0)

#define icnss_pr_vdbg(_fmt, ...) do {					\
	pr_debug(_fmt, ##__VA_ARGS__);					\
	icnss_ipc_log_long_string(pr_fmt(_fmt), ##__VA_ARGS__);		\
	} while (0)
#elif defined(DEBUG)
#define icnss_pr_dbg(_fmt, ...) do {					\
	printk("%s" pr_fmt(_fmt), KERN_DEBUG, ##__VA_ARGS__);		\
	icnss_ipc_log_string("%s" pr_fmt(_fmt), "",			\
			     ##__VA_ARGS__);				\
	} while (0)

#define icnss_pr_vdbg(_fmt, ...) do {					\
	printk("%s" pr_fmt(_fmt), KERN_DEBUG, ##__VA_ARGS__);		\
	icnss_ipc_log_long_string("%s" pr_fmt(_fmt), "",		\
				  ##__VA_ARGS__);			\
	} while (0)
#else
#define icnss_pr_dbg(_fmt, ...) do {					\
	no_printk("%s" pr_fmt(_fmt), KERN_DEBUG, ##__VA_ARGS__);	\
	icnss_ipc_log_string("%s" pr_fmt(_fmt), "",			\
		     ##__VA_ARGS__);					\
	} while (0)

#define icnss_pr_vdbg(_fmt, ...) do {					\
	no_printk("%s" pr_fmt(_fmt), KERN_DEBUG, ##__VA_ARGS__);	\
	icnss_ipc_log_long_string("%s" pr_fmt(_fmt), "",		\
				  ##__VA_ARGS__);			\
	} while (0)
#endif

#ifdef CONFIG_ICNSS_DEBUG
#define ICNSS_ASSERT(_condition) do {					\
		if (!(_condition)) {					\
			icnss_pr_err("ASSERT at line %d\n", __LINE__);	\
			BUG_ON(1);					\
		}							\
	} while (0)

bool ignore_qmi_timeout;
#define ICNSS_QMI_ASSERT() ICNSS_ASSERT(ignore_qmi_timeout)
#else
#define ICNSS_ASSERT(_condition) do { } while (0)
#define ICNSS_QMI_ASSERT() do { } while (0)
#endif

#define QMI_ERR_PLAT_CCPM_CLK_INIT_FAILED 0x77

enum icnss_debug_quirks {
	HW_ALWAYS_ON,
	HW_DEBUG_ENABLE,
	SKIP_QMI,
	HW_ONLY_TOP_LEVEL_RESET,
	RECOVERY_DISABLE,
	SSR_ONLY,
	PDR_ONLY,
	VBATT_DISABLE,
	FW_REJUVENATE_ENABLE,
};

#define ICNSS_QUIRKS_DEFAULT		(BIT(VBATT_DISABLE) | \
					 BIT(FW_REJUVENATE_ENABLE))

unsigned long quirks = ICNSS_QUIRKS_DEFAULT;
module_param(quirks, ulong, 0600);

uint64_t dynamic_feature_mask = QMI_WLFW_FW_REJUVENATE_V01;
module_param(dynamic_feature_mask, ullong, 0600);

void *icnss_ipc_log_context;
void *icnss_ipc_log_long_context;

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

enum icnss_msa_perm {
	ICNSS_MSA_PERM_HLOS_ALL = 0,
	ICNSS_MSA_PERM_WLAN_HW_RW = 1,
	ICNSS_MSA_PERM_MAX,
};

#define ICNSS_MAX_VMIDS     4

struct icnss_mem_region_info {
	uint64_t reg_addr;
	uint32_t size;
	uint8_t secure_flag;
	enum icnss_msa_perm perm;
};

struct icnss_msa_perm_list_t {
	int vmids[ICNSS_MAX_VMIDS];
	int perms[ICNSS_MAX_VMIDS];
	int nelems;
};

struct icnss_msa_perm_list_t msa_perm_secure_list[ICNSS_MSA_PERM_MAX] = {
	[ICNSS_MSA_PERM_HLOS_ALL] = {
		.vmids = {VMID_HLOS},
		.perms = {PERM_READ | PERM_WRITE | PERM_EXEC},
		.nelems = 1,
	},

	[ICNSS_MSA_PERM_WLAN_HW_RW] = {
		.vmids = {VMID_MSS_MSA, VMID_WLAN},
		.perms = {PERM_READ | PERM_WRITE,
			PERM_READ | PERM_WRITE},
		.nelems = 2,
	},

};

struct icnss_msa_perm_list_t msa_perm_list[ICNSS_MSA_PERM_MAX] = {
	[ICNSS_MSA_PERM_HLOS_ALL] = {
		.vmids = {VMID_HLOS},
		.perms = {PERM_READ | PERM_WRITE | PERM_EXEC},
		.nelems = 1,
	},

	[ICNSS_MSA_PERM_WLAN_HW_RW] = {
		.vmids = {VMID_MSS_MSA, VMID_WLAN, VMID_WLAN_CE},
		.perms = {PERM_READ | PERM_WRITE,
			PERM_READ | PERM_WRITE,
			PERM_READ | PERM_WRITE},
		.nelems = 3,
	},

};

struct icnss_event_pd_service_down_data {
	bool crashed;
	bool fw_rejuvenate;
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
	ICNSS_SSR_REGISTERED,
	ICNSS_PDR_REGISTERED,
	ICNSS_PD_RESTART,
	ICNSS_MSA0_ASSIGNED,
	ICNSS_WLFW_EXISTS,
	ICNSS_SHUTDOWN_DONE,
	ICNSS_HOST_TRIGGERED_PDR,
	ICNSS_FW_DOWN,
	ICNSS_DRIVER_UNLOADING,
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
	{NULL, "vdd-0.8-cx-mx", 800000, 800000, 0, 0, false},
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

	struct {
		uint32_t pdr_fw_crash;
		uint32_t pdr_host_error;
		uint32_t root_pd_crash;
		uint32_t root_pd_shutdown;
	} recovery;

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
	u32 rejuvenate_ind;
	uint32_t rejuvenate_ack_req;
	uint32_t rejuvenate_ack_resp;
	uint32_t rejuvenate_ack_err;
};

enum icnss_pdr_cause_index {
	ICNSS_FW_CRASH,
	ICNSS_ROOT_PD_CRASH,
	ICNSS_ROOT_PD_SHUTDOWN,
	ICNSS_HOST_ERROR,
};

static const char * const icnss_pdr_cause[] = {
	[ICNSS_FW_CRASH] = "FW crash",
	[ICNSS_ROOT_PD_CRASH] = "Root PD crashed",
	[ICNSS_ROOT_PD_SHUTDOWN] = "Root PD shutdown",
	[ICNSS_HOST_ERROR] = "Host error",
};

struct service_notifier_context {
	void *handle;
	uint32_t instance_id;
	char name[QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1];
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
	char fw_build_id[QMI_WLFW_MAX_BUILD_ID_LEN_V01 + 1];
	u32 pwr_pin_result;
	u32 phy_io_pin_result;
	u32 rf_pin_result;
	uint32_t nr_mem_region;
	struct icnss_mem_region_info
		mem_region[QMI_WLFW_MAX_NUM_MEMORY_REGIONS_V01];
	struct dentry *root_dentry;
	spinlock_t on_off_lock;
	struct icnss_stats stats;
	struct work_struct service_notifier_work;
	struct service_notifier_context *service_notifier;
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
	struct ramdump_device *msa0_dump_dev;
	bool bypass_s1_smmu;
	bool force_err_fatal;
	u8 cause_for_rejuvenation;
	u8 requesting_sub_system;
	u16 line_number;
	char function_name[QMI_WLFW_FUNCTION_NAME_LEN_V01 + 1];
	struct mutex dev_lock;
} *penv;

#ifdef CONFIG_ICNSS_DEBUG
static void icnss_ignore_qmi_timeout(bool ignore)
{
	ignore_qmi_timeout = ignore;
}
#else
static void icnss_ignore_qmi_timeout(bool ignore) { }
#endif

static int icnss_assign_msa_perm(struct icnss_mem_region_info
				 *mem_region, enum icnss_msa_perm new_perm)
{
	int ret = 0;
	phys_addr_t addr;
	u32 size;
	u32 i = 0;
	u32 source_vmids[ICNSS_MAX_VMIDS] = {0};
	u32 source_nelems;
	u32 dest_vmids[ICNSS_MAX_VMIDS] = {0};
	u32 dest_perms[ICNSS_MAX_VMIDS] = {0};
	u32 dest_nelems;
	enum icnss_msa_perm cur_perm = mem_region->perm;
	struct icnss_msa_perm_list_t *new_perm_list, *old_perm_list;

	addr = mem_region->reg_addr;
	size = mem_region->size;

	if (mem_region->secure_flag) {
		new_perm_list = &msa_perm_secure_list[new_perm];
		old_perm_list = &msa_perm_secure_list[cur_perm];
	} else {
		new_perm_list = &msa_perm_list[new_perm];
		old_perm_list = &msa_perm_list[cur_perm];
	}

	source_nelems = old_perm_list->nelems;
	dest_nelems = new_perm_list->nelems;

	for (i = 0; i < source_nelems; ++i)
		source_vmids[i] = old_perm_list->vmids[i];

	for (i = 0; i < dest_nelems; ++i) {
		dest_vmids[i] = new_perm_list->vmids[i];
		dest_perms[i] = new_perm_list->perms[i];
	}

	ret = hyp_assign_phys(addr, size, source_vmids, source_nelems,
			      dest_vmids, dest_perms, dest_nelems);
	if (ret) {
		icnss_pr_err("Hyperviser map failed for PA=%pa size=%u err=%d\n",
			     &addr, size, ret);
		goto out;
	}

	icnss_pr_dbg("Hypervisor map for source_nelems=%d, source[0]=%x, source[1]=%x, source[2]=%x,"
		     "source[3]=%x, dest_nelems=%d, dest[0]=%x, dest[1]=%x, dest[2]=%x, dest[3]=%x\n",
		     source_nelems, source_vmids[0], source_vmids[1],
		     source_vmids[2], source_vmids[3], dest_nelems,
		     dest_vmids[0], dest_vmids[1], dest_vmids[2],
		     dest_vmids[3]);
out:
	return ret;
}

static int icnss_assign_msa_perm_all(struct icnss_priv *priv,
				     enum icnss_msa_perm new_perm)
{
	int ret;
	int i;
	enum icnss_msa_perm old_perm;

	if (priv->nr_mem_region > QMI_WLFW_MAX_NUM_MEMORY_REGIONS_V01) {
		icnss_pr_err("Invalid memory region len %d\n",
			     priv->nr_mem_region);
		return -EINVAL;
	}

	for (i = 0; i < priv->nr_mem_region; i++) {
		old_perm = priv->mem_region[i].perm;
		ret = icnss_assign_msa_perm(&priv->mem_region[i], new_perm);
		if (ret)
			goto err_unmap;
		priv->mem_region[i].perm = new_perm;
	}
	return 0;

err_unmap:
	for (i--; i >= 0; i--) {
		icnss_assign_msa_perm(&priv->mem_region[i], old_perm);
	}
	return ret;
}

static void icnss_pm_stay_awake(struct icnss_priv *priv)
{
	if (atomic_inc_return(&priv->pm_count) != 1)
		return;

	icnss_pr_vdbg("PM stay awake, state: 0x%lx, count: %d\n", priv->state,
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

	icnss_pr_vdbg("PM relax, state: 0x%lx, count: %d\n", priv->state,
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
		ret = -resp.resp.result;
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

	memset(&ind_msg, 0, sizeof(ind_msg));

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

		icnss_pr_vdbg("Regulator %s being enabled\n", vreg_info->name);

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

		icnss_pr_vdbg("Regulator %s being disabled\n", vreg_info->name);

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

		icnss_pr_vdbg("Clock %s being enabled\n", clk_info->name);

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

		icnss_pr_vdbg("Clock %s being disabled\n", clk_info->name);

		clk_disable_unprepare(clk_info->handle);
	}

	return 0;
}

static int icnss_hw_power_on(struct icnss_priv *priv)
{
	int ret = 0;

	icnss_pr_dbg("HW Power on: state: 0x%lx\n", priv->state);

	spin_lock(&priv->on_off_lock);
	if (test_bit(ICNSS_POWER_ON, &priv->state)) {
		spin_unlock(&priv->on_off_lock);
		return ret;
	}
	set_bit(ICNSS_POWER_ON, &priv->state);
	spin_unlock(&priv->on_off_lock);

	ret = icnss_vreg_on(priv);
	if (ret)
		goto out;

	ret = icnss_clk_init(priv);
	if (ret)
		goto vreg_off;

	return ret;

vreg_off:
	icnss_vreg_off(priv);
out:
	clear_bit(ICNSS_POWER_ON, &priv->state);
	return ret;
}

static int icnss_hw_power_off(struct icnss_priv *priv)
{
	int ret = 0;

	if (test_bit(HW_ALWAYS_ON, &quirks))
		return 0;

	if (test_bit(ICNSS_FW_DOWN, &priv->state))
		return 0;

	icnss_pr_dbg("HW Power off: 0x%lx\n", priv->state);

	spin_lock(&priv->on_off_lock);
	if (!test_bit(ICNSS_POWER_ON, &priv->state)) {
		spin_unlock(&priv->on_off_lock);
		return ret;
	}
	clear_bit(ICNSS_POWER_ON, &priv->state);
	spin_unlock(&priv->on_off_lock);

	icnss_clk_deinit(priv);

	ret = icnss_vreg_off(priv);

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

bool icnss_is_fw_ready(void)
{
	if (!penv)
		return false;
	else
		return test_bit(ICNSS_FW_READY, &penv->state);
}
EXPORT_SYMBOL(icnss_is_fw_ready);

bool icnss_is_fw_down(void)
{
	if (!penv)
		return false;
	else
		return test_bit(ICNSS_FW_DOWN, &penv->state);
}
EXPORT_SYMBOL(icnss_is_fw_down);


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

static irqreturn_t fw_error_fatal_handler(int irq, void *ctx)
{
	struct icnss_priv *priv = ctx;

	if (priv)
		priv->force_err_fatal = true;

	icnss_pr_err("Received force error fatal request from FW\n");

	return IRQ_HANDLED;
}

static void icnss_register_force_error_fatal(struct icnss_priv *priv)
{
	int gpio, irq, ret;

	if (!of_find_property(priv->pdev->dev.of_node,
				"qcom,gpio-force-fatal-error", NULL)) {
		icnss_pr_dbg("Error fatal smp2p handler not registered\n");
		return;
	}
	gpio = of_get_named_gpio(priv->pdev->dev.of_node,
				"qcom,gpio-force-fatal-error", 0);
	if (!gpio_is_valid(gpio)) {
		icnss_pr_err("Invalid GPIO for error fatal smp2p %d\n", gpio);
		return;
	}
	irq = gpio_to_irq(gpio);
	if (irq < 0) {
		icnss_pr_err("Invalid IRQ for error fatal smp2p %u\n", irq);
		return;
	}
	ret = request_irq(irq, fw_error_fatal_handler,
			IRQF_TRIGGER_RISING, "wlanfw-err", priv);
	if (ret < 0) {
		icnss_pr_err("Unable to regiser for error fatal IRQ handler %d",
				irq);
		return;
	}
	icnss_pr_dbg("FW force error fatal handler registered\n");
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
		ret = -resp.resp.result;
		goto out;
	}

	icnss_pr_dbg("Receive mem_region_info_len: %d\n",
		     resp.mem_region_info_len);

	if (resp.mem_region_info_len > QMI_WLFW_MAX_NUM_MEMORY_REGIONS_V01) {
		icnss_pr_err("Invalid memory region length received: %d\n",
			     resp.mem_region_info_len);
		ret = -EINVAL;
		goto out;
	}

	penv->stats.msa_info_resp++;
	penv->nr_mem_region = resp.mem_region_info_len;
	for (i = 0; i < resp.mem_region_info_len; i++) {
		penv->mem_region[i].reg_addr =
			resp.mem_region_info[i].region_addr;
		penv->mem_region[i].size =
			resp.mem_region_info[i].size;
		penv->mem_region[i].secure_flag =
			resp.mem_region_info[i].secure_flag;
		icnss_pr_dbg("Memory Region: %d Addr: 0x%llx Size: 0x%x Flag: 0x%08x\n",
			     i, penv->mem_region[i].reg_addr,
			     penv->mem_region[i].size,
			     penv->mem_region[i].secure_flag);
	}

	return 0;

out:
	penv->stats.msa_info_err++;
	ICNSS_QMI_ASSERT();
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
		ret = -resp.resp.result;
		goto out;
	}
	penv->stats.msa_ready_resp++;

	return 0;

out:
	penv->stats.msa_ready_err++;
	ICNSS_QMI_ASSERT();
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
	if (test_bit(FW_REJUVENATE_ENABLE, &quirks)) {
		req.rejuvenate_enable_valid = 1;
		req.rejuvenate_enable = 1;
	}

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
		ret = -resp.resp.result;
		goto out;
	}
	penv->stats.ind_register_resp++;

	return 0;

out:
	penv->stats.ind_register_err++;
	ICNSS_QMI_ASSERT();
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
		ret = -resp.resp.result;
		if (resp.resp.error == QMI_ERR_PLAT_CCPM_CLK_INIT_FAILED)
			icnss_pr_err("RF card Not present");
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
	if (resp.fw_build_id_valid)
		strlcpy(penv->fw_build_id, resp.fw_build_id,
			QMI_WLFW_MAX_BUILD_ID_LEN_V01 + 1);

	icnss_pr_dbg("Capability, chip_id: 0x%x, chip_family: 0x%x, board_id: 0x%x, soc_id: 0x%x, fw_version: 0x%x, fw_build_timestamp: %s, fw_build_id: %s",
		     penv->chip_info.chip_id, penv->chip_info.chip_family,
		     penv->board_info.board_id, penv->soc_info.soc_id,
		     penv->fw_version_info.fw_version,
		     penv->fw_version_info.fw_build_timestamp,
		     penv->fw_build_id);

	return 0;

out:
	penv->stats.cap_err++;
	ICNSS_QMI_ASSERT();
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
		ret = -resp.resp.result;
		goto out;
	}
	penv->stats.mode_resp++;

	return 0;

out:
	penv->stats.mode_req_err++;
	ICNSS_QMI_ASSERT();
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
		ret = -resp.resp.result;
		goto out;
	}
	penv->stats.cfg_resp++;

	return 0;

out:
	penv->stats.cfg_req_err++;
	ICNSS_QMI_ASSERT();
	return ret;
}

static int wlfw_ini_send_sync_msg(uint8_t fw_log_mode)
{
	int ret;
	struct wlfw_ini_req_msg_v01 req;
	struct wlfw_ini_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!penv || !penv->wlfw_clnt)
		return -ENODEV;

	icnss_pr_dbg("Sending ini sync request, state: 0x%lx, fw_log_mode: %d\n",
		     penv->state, fw_log_mode);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.enablefwlog_valid = 1;
	req.enablefwlog = fw_log_mode;

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
		icnss_pr_err("Send INI req failed fw_log_mode: %d, ret: %d\n",
			     fw_log_mode, ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI INI request rejected, fw_log_mode:%d result:%d error:%d\n",
			     fw_log_mode, resp.resp.result, resp.resp.error);
		ret = -resp.resp.result;
		goto out;
	}
	penv->stats.ini_resp++;

	return 0;

out:
	penv->stats.ini_req_err++;
	ICNSS_QMI_ASSERT();
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
		ret = -resp->resp.result;
		goto out;
	}

	if (!resp->data_valid || resp->data_len < data_len) {
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
		ret = -resp.resp.result;
		goto out;
	}
out:
	kfree(req);
	return ret;
}

static int icnss_decode_rejuvenate_ind(void *msg, unsigned int msg_len)
{
	struct msg_desc ind_desc;
	struct wlfw_rejuvenate_ind_msg_v01 ind_msg;
	int ret = 0;

	if (!penv || !penv->wlfw_clnt) {
		ret = -ENODEV;
		goto out;
	}

	memset(&ind_msg, 0, sizeof(ind_msg));

	ind_desc.msg_id = QMI_WLFW_REJUVENATE_IND_V01;
	ind_desc.max_msg_len = WLFW_REJUVENATE_IND_MSG_V01_MAX_MSG_LEN;
	ind_desc.ei_array = wlfw_rejuvenate_ind_msg_v01_ei;

	ret = qmi_kernel_decode(&ind_desc, &ind_msg, msg, msg_len);
	if (ret < 0) {
		icnss_pr_err("Failed to decode rejuvenate ind message: ret %d, msg_len %u\n",
			     ret, msg_len);
		goto out;
	}

	if (ind_msg.cause_for_rejuvenation_valid)
		penv->cause_for_rejuvenation = ind_msg.cause_for_rejuvenation;
	else
		penv->cause_for_rejuvenation = 0;
	if (ind_msg.requesting_sub_system_valid)
		penv->requesting_sub_system = ind_msg.requesting_sub_system;
	else
		penv->requesting_sub_system = 0;
	if (ind_msg.line_number_valid)
		penv->line_number = ind_msg.line_number;
	else
		penv->line_number = 0;
	if (ind_msg.function_name_valid)
		memcpy(penv->function_name, ind_msg.function_name,
		       QMI_WLFW_FUNCTION_NAME_LEN_V01 + 1);
	else
		memset(penv->function_name, 0,
		       QMI_WLFW_FUNCTION_NAME_LEN_V01 + 1);

	icnss_pr_info("Cause for rejuvenation: 0x%x, requesting sub-system: 0x%x, line number: %u, function name: %s\n",
		      penv->cause_for_rejuvenation,
		      penv->requesting_sub_system,
		      penv->line_number,
		      penv->function_name);

	penv->stats.rejuvenate_ind++;
out:
	return ret;
}

static int wlfw_rejuvenate_ack_send_sync_msg(struct icnss_priv *priv)
{
	int ret;
	struct wlfw_rejuvenate_ack_req_msg_v01 req;
	struct wlfw_rejuvenate_ack_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	icnss_pr_dbg("Sending rejuvenate ack request, state: 0x%lx\n",
		     priv->state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req_desc.max_msg_len = WLFW_REJUVENATE_ACK_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_REJUVENATE_ACK_REQ_V01;
	req_desc.ei_array = wlfw_rejuvenate_ack_req_msg_v01_ei;

	resp_desc.max_msg_len = WLFW_REJUVENATE_ACK_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_REJUVENATE_ACK_RESP_V01;
	resp_desc.ei_array = wlfw_rejuvenate_ack_resp_msg_v01_ei;

	priv->stats.rejuvenate_ack_req++;
	ret = qmi_send_req_wait(priv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send rejuvenate ack req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI rejuvenate ack request rejected, result:%d error %d\n",
			     resp.resp.result, resp.resp.error);
		ret = -resp.resp.result;
		goto out;
	}
	priv->stats.rejuvenate_ack_resp++;
	return 0;

out:
	priv->stats.rejuvenate_ack_err++;
	ICNSS_QMI_ASSERT();
	return ret;
}

static int wlfw_dynamic_feature_mask_send_sync_msg(struct icnss_priv *priv,
					   uint64_t dynamic_feature_mask)
{
	int ret;
	struct wlfw_dynamic_feature_mask_req_msg_v01 req;
	struct wlfw_dynamic_feature_mask_resp_msg_v01 resp;
	struct msg_desc req_desc, resp_desc;

	if (!test_bit(ICNSS_WLFW_QMI_CONNECTED, &priv->state)) {
		icnss_pr_err("Invalid state for dynamic feature: 0x%lx\n",
			     priv->state);
		return -EINVAL;
	}

	if (!test_bit(FW_REJUVENATE_ENABLE, &quirks)) {
		icnss_pr_dbg("FW rejuvenate is disabled from quirks\n");
		return 0;
	}

	icnss_pr_dbg("Sending dynamic feature mask request, val 0x%llx, state: 0x%lx\n",
		     dynamic_feature_mask, priv->state);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.mask_valid = 1;
	req.mask = dynamic_feature_mask;

	req_desc.max_msg_len =
		WLFW_DYNAMIC_FEATURE_MASK_REQ_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = QMI_WLFW_DYNAMIC_FEATURE_MASK_REQ_V01;
	req_desc.ei_array = wlfw_dynamic_feature_mask_req_msg_v01_ei;

	resp_desc.max_msg_len =
		WLFW_DYNAMIC_FEATURE_MASK_RESP_MSG_V01_MAX_MSG_LEN;
	resp_desc.msg_id = QMI_WLFW_DYNAMIC_FEATURE_MASK_RESP_V01;
	resp_desc.ei_array = wlfw_dynamic_feature_mask_resp_msg_v01_ei;

	ret = qmi_send_req_wait(priv->wlfw_clnt, &req_desc, &req, sizeof(req),
				&resp_desc, &resp, sizeof(resp),
				WLFW_TIMEOUT_MS);
	if (ret < 0) {
		icnss_pr_err("Send dynamic feature mask req failed %d\n", ret);
		goto out;
	}

	if (resp.resp.result != QMI_RESULT_SUCCESS_V01) {
		icnss_pr_err("QMI dynamic feature mask request rejected, result:%d error %d\n",
			     resp.resp.result, resp.resp.error);
		ret = -resp.resp.result;
		goto out;
	}

	icnss_pr_dbg("prev_mask_valid %u, prev_mask 0x%llx, curr_maks_valid %u, curr_mask 0x%llx\n",
		     resp.prev_mask_valid, resp.prev_mask,
		     resp.curr_mask_valid, resp.curr_mask);

	return 0;

out:
	return ret;
}

static void icnss_qmi_wlfw_clnt_notify_work(struct work_struct *work)
{
	int ret;

	if (!penv || !penv->wlfw_clnt)
		return;

	icnss_pr_vdbg("Receiving Event in work queue context\n");

	do {
	} while ((ret = qmi_recv_msg(penv->wlfw_clnt)) == 0);

	if (ret != -ENOMSG)
		icnss_pr_err("Error receiving message: %d\n", ret);

	icnss_pr_vdbg("Receiving Event completed\n");
}

static void icnss_qmi_wlfw_clnt_notify(struct qmi_handle *handle,
			     enum qmi_event_type event, void *notify_priv)
{
	icnss_pr_vdbg("QMI client notify: %d\n", event);

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

static int icnss_call_driver_uevent(struct icnss_priv *priv,
				    enum icnss_uevent uevent, void *data)
{
	struct icnss_uevent_data uevent_data;

	if (!priv->ops || !priv->ops->uevent)
		return 0;

	icnss_pr_dbg("Calling driver uevent state: 0x%lx, uevent: %d\n",
		     priv->state, uevent);

	uevent_data.uevent = uevent;
	uevent_data.data = data;

	return priv->ops->uevent(&priv->pdev->dev, &uevent_data);
}

static void icnss_qmi_wlfw_clnt_ind(struct qmi_handle *handle,
			  unsigned int msg_id, void *msg,
			  unsigned int msg_len, void *ind_cb_priv)
{
	struct icnss_event_pd_service_down_data *event_data;
	struct icnss_uevent_fw_down_data fw_down_data;

	if (!penv)
		return;

	icnss_pr_dbg("Received Ind 0x%x, msg_len: %d\n", msg_id, msg_len);

	if (test_bit(ICNSS_FW_DOWN, &penv->state)) {
		icnss_pr_dbg("FW down, ignoring 0x%x, state: 0x%lx\n",
				msg_id, penv->state);
		return;
	}

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
	case QMI_WLFW_REJUVENATE_IND_V01:
		icnss_pr_dbg("Received Rejuvenate Indication msg_id 0x%x, state: 0x%lx\n",
			     msg_id, penv->state);

		icnss_ignore_qmi_timeout(true);
		icnss_decode_rejuvenate_ind(msg, msg_len);
		event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
		if (event_data == NULL)
			return;
		event_data->crashed = true;
		event_data->fw_rejuvenate = true;
		fw_down_data.crashed = true;
		icnss_call_driver_uevent(penv, ICNSS_UEVENT_FW_DOWN,
					 &fw_down_data);
		icnss_driver_event_post(ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
					0, event_data);
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
	clear_bit(ICNSS_FW_DOWN, &penv->state);

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

	if (!test_bit(ICNSS_MSA0_ASSIGNED, &penv->state)) {
		ret = icnss_assign_msa_perm_all(penv, ICNSS_MSA_PERM_WLAN_HW_RW);
		if (ret < 0)
			goto err_power_on;
		set_bit(ICNSS_MSA0_ASSIGNED, &penv->state);
	}

	ret = wlfw_msa_ready_send_sync_msg();
	if (ret < 0)
		goto err_setup_msa;

	ret = wlfw_cap_send_sync_msg();
	if (ret < 0)
		goto err_setup_msa;

	wlfw_dynamic_feature_mask_send_sync_msg(penv,
						dynamic_feature_mask);

	icnss_init_vph_monitor(penv);

	icnss_register_force_error_fatal(penv);

	return ret;

err_setup_msa:
	icnss_assign_msa_perm_all(penv, ICNSS_MSA_PERM_HLOS_ALL);
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
	int ret = 0;
	int probe_cnt = 0;

	if (!priv->ops || !priv->ops->probe)
		return 0;

	if (test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		return -EINVAL;

	icnss_pr_dbg("Calling driver probe state: 0x%lx\n", priv->state);

	icnss_hw_power_on(priv);

	while (probe_cnt < ICNSS_MAX_PROBE_CNT) {
		ret = priv->ops->probe(&priv->pdev->dev);
		probe_cnt++;
		if (ret != -EPROBE_DEFER)
			break;
	}
	if (ret < 0) {
		icnss_pr_err("Driver probe failed: %d, state: 0x%lx, probe_cnt: %d\n",
			     ret, priv->state, probe_cnt);
		goto out;
	}

	set_bit(ICNSS_DRIVER_PROBED, &priv->state);

	return 0;

out:
	icnss_hw_power_off(priv);
	return ret;
}

static int icnss_call_driver_shutdown(struct icnss_priv *priv)
{
	if (!test_bit(ICNSS_DRIVER_PROBED, &penv->state))
		goto out;

	if (!priv->ops || !priv->ops->shutdown)
		goto out;

	if (test_bit(ICNSS_SHUTDOWN_DONE, &penv->state))
		goto out;

	icnss_pr_dbg("Calling driver shutdown state: 0x%lx\n", priv->state);

	priv->ops->shutdown(&priv->pdev->dev);
	set_bit(ICNSS_SHUTDOWN_DONE, &penv->state);

out:
	return 0;
}

static int icnss_pd_restart_complete(struct icnss_priv *priv)
{
	int ret;

	icnss_pm_relax(priv);

	icnss_call_driver_shutdown(priv);

	clear_bit(ICNSS_PD_RESTART, &priv->state);

	if (!priv->ops || !priv->ops->reinit)
		goto out;

	if (test_bit(ICNSS_FW_DOWN, &priv->state)) {
		icnss_pr_err("FW is in bad state, state: 0x%lx\n",
			     priv->state);
		goto out;
	}

	if (!test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		goto call_probe;

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
	clear_bit(ICNSS_SHUTDOWN_DONE, &penv->state);
	return 0;

call_probe:
	return icnss_call_driver_probe(priv);

out_power_off:
	icnss_hw_power_off(priv);

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
		ret = icnss_pd_restart_complete(penv);
	else
		ret = icnss_call_driver_probe(penv);

out:
	return ret;
}

static int icnss_driver_event_register_driver(void *data)
{
	int ret = 0;
	int probe_cnt = 0;

	if (penv->ops)
		return -EEXIST;

	penv->ops = data;

	if (test_bit(SKIP_QMI, &quirks))
		set_bit(ICNSS_FW_READY, &penv->state);

	if (test_bit(ICNSS_FW_DOWN, &penv->state)) {
		icnss_pr_err("FW is in bad state, state: 0x%lx\n",
			     penv->state);
		return -ENODEV;
	}

	if (!test_bit(ICNSS_FW_READY, &penv->state)) {
		icnss_pr_dbg("FW is not ready yet, state: 0x%lx\n",
			     penv->state);
		goto out;
	}

	ret = icnss_hw_power_on(penv);
	if (ret)
		goto out;

	while (probe_cnt < ICNSS_MAX_PROBE_CNT) {
		ret = penv->ops->probe(&penv->pdev->dev);
		probe_cnt++;
		if (ret != -EPROBE_DEFER)
			break;
	}
	if (ret) {
		icnss_pr_err("Driver probe failed: %d, state: 0x%lx, probe_cnt: %d\n",
			     ret, penv->state, probe_cnt);
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

	set_bit(ICNSS_DRIVER_UNLOADING, &penv->state);
	if (penv->ops)
		penv->ops->remove(&penv->pdev->dev);

	clear_bit(ICNSS_DRIVER_UNLOADING, &penv->state);
	clear_bit(ICNSS_DRIVER_PROBED, &penv->state);

	penv->ops = NULL;

	icnss_hw_power_off(penv);

out:
	return 0;
}

static int icnss_fw_crashed(struct icnss_priv *priv,
			    struct icnss_event_pd_service_down_data *event_data)
{
	icnss_pr_dbg("FW crashed, state: 0x%lx\n", priv->state);

	set_bit(ICNSS_PD_RESTART, &priv->state);
	clear_bit(ICNSS_FW_READY, &priv->state);

	icnss_pm_stay_awake(priv);

	if (test_bit(ICNSS_DRIVER_PROBED, &priv->state))
		icnss_call_driver_uevent(priv, ICNSS_UEVENT_FW_CRASHED, NULL);

	if (event_data->fw_rejuvenate)
		wlfw_rejuvenate_ack_send_sync_msg(priv);

	return 0;
}

static int icnss_driver_event_pd_service_down(struct icnss_priv *priv,
					      void *data)
{
	int ret = 0;
	struct icnss_event_pd_service_down_data *event_data = data;

	if (!test_bit(ICNSS_WLFW_EXISTS, &priv->state))
		goto out;

	if (test_bit(ICNSS_PD_RESTART, &priv->state) && event_data->crashed) {
		icnss_pr_err("PD Down while recovery inprogress, crashed: %d, state: 0x%lx\n",
			     event_data->crashed, priv->state);
		ICNSS_ASSERT(0);
		goto out;
	}

	if (priv->force_err_fatal)
		ICNSS_ASSERT(0);

	icnss_fw_crashed(priv, event_data);

out:
	kfree(data);

	icnss_ignore_qmi_timeout(false);

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

static int icnss_msa0_ramdump(struct icnss_priv *priv)
{
	struct ramdump_segment segment;

	memset(&segment, 0, sizeof(segment));
	segment.v_address = priv->msa_va;
	segment.size = priv->msa_mem_size;
	return do_ramdump(priv->msa0_dump_dev, &segment, 1);
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
	struct icnss_uevent_fw_down_data fw_down_data;
	int ret = 0;

	icnss_pr_vdbg("Modem-Notify: event %lu\n", code);

	if (code == SUBSYS_AFTER_SHUTDOWN &&
	    notif->crashed == CRASH_STATUS_ERR_FATAL) {
		ret = icnss_assign_msa_perm_all(priv,
						ICNSS_MSA_PERM_HLOS_ALL);
		if (!ret) {
			icnss_pr_info("Collecting msa0 segment dump\n");
			icnss_msa0_ramdump(priv);
			icnss_assign_msa_perm_all(priv,
						  ICNSS_MSA_PERM_WLAN_HW_RW);
		} else {
			icnss_pr_err("Not able to Collect msa0 segment dump"
				     "Apps permissions not assigned %d\n", ret);
		}
		return NOTIFY_OK;
	}

	if (code != SUBSYS_BEFORE_SHUTDOWN)
		return NOTIFY_OK;

	if (test_bit(ICNSS_PDR_REGISTERED, &priv->state)) {
		set_bit(ICNSS_FW_DOWN, &priv->state);
		icnss_ignore_qmi_timeout(true);

		fw_down_data.crashed = !!notif->crashed;
		if (test_bit(ICNSS_FW_READY, &priv->state) &&
		    !test_bit(ICNSS_DRIVER_UNLOADING, &priv->state))
			icnss_call_driver_uevent(priv,
						 ICNSS_UEVENT_FW_DOWN,
						 &fw_down_data);
		return NOTIFY_OK;
	}

	icnss_pr_info("Modem went down, state: 0x%lx, crashed: %d\n",
		      priv->state, notif->crashed);

	set_bit(ICNSS_FW_DOWN, &priv->state);

	if (notif->crashed)
		priv->stats.recovery.root_pd_crash++;
	else
		priv->stats.recovery.root_pd_shutdown++;

	icnss_ignore_qmi_timeout(true);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);

	if (event_data == NULL)
		return notifier_from_errno(-ENOMEM);

	event_data->crashed = notif->crashed;

	fw_down_data.crashed = !!notif->crashed;
	icnss_call_driver_uevent(priv, ICNSS_UEVENT_FW_DOWN, &fw_down_data);

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

	set_bit(ICNSS_SSR_REGISTERED, &priv->state);

	return ret;
}

static int icnss_modem_ssr_unregister_notifier(struct icnss_priv *priv)
{
	if (!test_and_clear_bit(ICNSS_SSR_REGISTERED, &priv->state))
		return 0;

	subsys_notif_unregister_notifier(priv->modem_notify_handler,
					 &priv->modem_ssr_nb);
	priv->modem_notify_handler = NULL;

	return 0;
}

static int icnss_pdr_unregister_notifier(struct icnss_priv *priv)
{
	int i;

	if (!test_and_clear_bit(ICNSS_PDR_REGISTERED, &priv->state))
		return 0;

	for (i = 0; i < priv->total_domains; i++)
		service_notif_unregister_notifier(
				priv->service_notifier[i].handle,
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
	struct icnss_uevent_fw_down_data fw_down_data;
	enum icnss_pdr_cause_index cause = ICNSS_ROOT_PD_CRASH;

	icnss_pr_dbg("PD service notification: 0x%lx state: 0x%lx\n",
		     notification, priv->state);

	if (notification != SERVREG_NOTIF_SERVICE_STATE_DOWN_V01)
		goto done;

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);

	if (event_data == NULL)
		return notifier_from_errno(-ENOMEM);

	event_data->crashed = true;

	if (state == NULL) {
		priv->stats.recovery.root_pd_crash++;
		goto event_post;
	}

	switch (*state) {
	case ROOT_PD_WDOG_BITE:
		priv->stats.recovery.root_pd_crash++;
		break;
	case ROOT_PD_SHUTDOWN:
		cause = ICNSS_ROOT_PD_SHUTDOWN;
		priv->stats.recovery.root_pd_shutdown++;
		event_data->crashed = false;
		break;
	case USER_PD_STATE_CHANGE:
		if (test_bit(ICNSS_HOST_TRIGGERED_PDR, &priv->state)) {
			cause = ICNSS_HOST_ERROR;
			priv->stats.recovery.pdr_host_error++;
		} else {
			cause = ICNSS_FW_CRASH;
			priv->stats.recovery.pdr_fw_crash++;
		}
		break;
	default:
		priv->stats.recovery.root_pd_crash++;
		break;
	}

	icnss_pr_info("PD service down, pd_state: %d, state: 0x%lx: cause: %s\n",
		      *state, priv->state, icnss_pdr_cause[cause]);
event_post:
	if (!test_bit(ICNSS_FW_DOWN, &priv->state)) {
		set_bit(ICNSS_FW_DOWN, &priv->state);
		icnss_ignore_qmi_timeout(true);

		fw_down_data.crashed = event_data->crashed;
		if (test_bit(ICNSS_FW_READY, &priv->state) &&
		    !test_bit(ICNSS_DRIVER_UNLOADING, &priv->state))
			icnss_call_driver_uevent(priv,
						 ICNSS_UEVENT_FW_DOWN,
						 &fw_down_data);
	}

	clear_bit(ICNSS_HOST_TRIGGERED_PDR, &priv->state);
	icnss_driver_event_post(ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
				ICNSS_EVENT_SYNC, event_data);
done:
	if (notification == SERVREG_NOTIF_SERVICE_STATE_UP_V01)
		clear_bit(ICNSS_FW_DOWN, &priv->state);
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
	struct service_notifier_context *notifier;

	icnss_pr_dbg("Get service notify opcode: %lu, state: 0x%lx\n", opcode,
		     priv->state);

	if (opcode != LOCATOR_UP)
		return NOTIFY_DONE;

	if (pd->total_domains == 0) {
		icnss_pr_err("Did not find any domains\n");
		ret = -ENOENT;
		goto out;
	}

	notifier = kcalloc(pd->total_domains,
				sizeof(struct service_notifier_context),
				GFP_KERNEL);
	if (!notifier) {
		ret = -ENOMEM;
		goto out;
	}

	priv->service_notifier_nb.notifier_call = icnss_service_notifier_notify;

	for (i = 0; i < pd->total_domains; i++) {
		icnss_pr_dbg("%d: domain_name: %s, instance_id: %d\n", i,
			     pd->domain_list[i].name,
			     pd->domain_list[i].instance_id);

		notifier[i].handle =
			service_notif_register_notifier(pd->domain_list[i].name,
				pd->domain_list[i].instance_id,
				&priv->service_notifier_nb, &curr_state);
		notifier[i].instance_id = pd->domain_list[i].instance_id;
		strlcpy(notifier[i].name, pd->domain_list[i].name,
			QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1);

		if (IS_ERR(notifier[i].handle)) {
			icnss_pr_err("%d: Unable to register notifier for %s(0x%x)\n",
				     i, pd->domain_list->name,
				     pd->domain_list->instance_id);
			ret = PTR_ERR(notifier[i].handle);
			goto free_handle;
		}
	}

	priv->service_notifier = notifier;
	priv->total_domains = pd->total_domains;

	set_bit(ICNSS_PDR_REGISTERED, &priv->state);

	icnss_pr_dbg("PD notification registration happened, state: 0x%lx\n",
		     priv->state);

	return NOTIFY_OK;

free_handle:
	for (i = 0; i < pd->total_domains; i++) {
		if (notifier[i].handle)
			service_notif_unregister_notifier(notifier[i].handle,
					&priv->service_notifier_nb);
	}
	kfree(notifier);

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
	icnss_pr_err("Failed to enable PD restart: %d\n", ret);
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

	priv->msa0_dump_dev = create_ramdump_device("wcss_msa0",
						    &priv->pdev->dev);
	if (!priv->msa0_dump_dev)
		return -ENOMEM;

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

int __icnss_register_driver(struct icnss_driver_ops *ops,
			    struct module *owner, const char *mod_name)
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
				      0, ops);

	if (ret == -EINTR)
		ret = 0;

out:
	return ret;
}
EXPORT_SYMBOL(__icnss_register_driver);

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

int icnss_ce_request_irq(struct device *dev, unsigned int ce_id,
	irqreturn_t (*handler)(int, void *),
		unsigned long flags, const char *name, void *ctx)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;

	if (!penv || !penv->pdev || !dev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_vdbg("CE request IRQ: %d, state: 0x%lx\n", ce_id, penv->state);

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

	icnss_pr_vdbg("IRQ requested: %d, ce_id: %d\n", irq, ce_id);

	penv->stats.ce_irqs[ce_id].request++;
out:
	return ret;
}
EXPORT_SYMBOL(icnss_ce_request_irq);

int icnss_ce_free_irq(struct device *dev, unsigned int ce_id, void *ctx)
{
	int ret = 0;
	unsigned int irq;
	struct ce_irq_list *irq_entry;

	if (!penv || !penv->pdev || !dev) {
		ret = -ENODEV;
		goto out;
	}

	icnss_pr_vdbg("CE free IRQ: %d, state: 0x%lx\n", ce_id, penv->state);

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

void icnss_enable_irq(struct device *dev, unsigned int ce_id)
{
	unsigned int irq;

	if (!penv || !penv->pdev || !dev) {
		icnss_pr_err("Platform driver not initialized\n");
		return;
	}

	icnss_pr_vdbg("Enable IRQ: ce_id: %d, state: 0x%lx\n", ce_id,
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

void icnss_disable_irq(struct device *dev, unsigned int ce_id)
{
	unsigned int irq;

	if (!penv || !penv->pdev || !dev) {
		icnss_pr_err("Platform driver not initialized\n");
		return;
	}

	icnss_pr_vdbg("Disable IRQ: ce_id: %d, state: 0x%lx\n", ce_id,
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

int icnss_get_soc_info(struct device *dev, struct icnss_soc_info *info)
{
	if (!penv || !dev) {
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

int icnss_set_fw_log_mode(struct device *dev, uint8_t fw_log_mode)
{
	int ret;

	if (!dev)
		return -ENODEV;

	icnss_pr_dbg("FW log mode: %u\n", fw_log_mode);

	ret = wlfw_ini_send_sync_msg(fw_log_mode);
	if (ret)
		icnss_pr_err("Fail to send ini, ret = %d, fw_log_mode: %u\n",
			     ret, fw_log_mode);
	return ret;
}
EXPORT_SYMBOL(icnss_set_fw_log_mode);

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

int icnss_wlan_enable(struct device *dev, struct icnss_wlan_enable_cfg *config,
		      enum icnss_driver_mode mode,
		      const char *host_version)
{
	struct wlfw_wlan_cfg_req_msg_v01 req;
	u32 i;
	int ret;

	if (!dev)
		return -ENODEV;

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

int icnss_wlan_disable(struct device *dev, enum icnss_driver_mode mode)
{
	if (!dev)
		return -ENODEV;

	return wlfw_wlan_mode_send_sync_msg(QMI_WLFW_OFF_V01);
}
EXPORT_SYMBOL(icnss_wlan_disable);

bool icnss_is_qmi_disable(struct device *dev)
{
	return test_bit(SKIP_QMI, &quirks) ? true : false;
}
EXPORT_SYMBOL(icnss_is_qmi_disable);

int icnss_get_ce_id(struct device *dev, int irq)
{
	int i;

	if (!penv || !penv->pdev || !dev)
		return -ENODEV;

	for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++) {
		if (penv->ce_irqs[i] == irq)
			return i;
	}

	icnss_pr_err("No matching CE id for irq %d\n", irq);

	return -EINVAL;
}
EXPORT_SYMBOL(icnss_get_ce_id);

int icnss_get_irq(struct device *dev, int ce_id)
{
	int irq;

	if (!penv || !penv->pdev || !dev)
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

int icnss_trigger_recovery(struct device *dev)
{
	int ret = 0;
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (priv->magic != ICNSS_MAGIC) {
		icnss_pr_err("Invalid drvdata: magic 0x%x\n", priv->magic);
		ret = -EINVAL;
		goto out;
	}

	if (test_bit(ICNSS_PD_RESTART, &priv->state)) {
		icnss_pr_err("PD recovery already in progress: state: 0x%lx\n",
			     priv->state);
		ret = -EPERM;
		goto out;
	}

	if (!test_bit(ICNSS_PDR_REGISTERED, &priv->state)) {
		icnss_pr_err("PD restart not enabled to trigger recovery: state: 0x%lx\n",
			     priv->state);
		ret = -EOPNOTSUPP;
		goto out;
	}

	if (!priv->service_notifier || !priv->service_notifier[0].handle) {
		icnss_pr_err("Invalid handle during recovery, state: 0x%lx\n",
			     priv->state);
		ret = -EINVAL;
		goto out;
	}

	WARN_ON(1);
	icnss_pr_warn("Initiate PD restart at WLAN FW, state: 0x%lx\n",
		      priv->state);

	/*
	 * Initiate PDR, required only for the first instance
	 */
	ret = service_notif_pd_restart(priv->service_notifier[0].name,
		priv->service_notifier[0].instance_id);

	if (!ret)
		set_bit(ICNSS_HOST_TRIGGERED_PDR, &priv->state);

out:
	return ret;
}
EXPORT_SYMBOL(icnss_trigger_recovery);


static int icnss_smmu_init(struct icnss_priv *priv)
{
	struct dma_iommu_mapping *mapping;
	int atomic_ctx = 1;
	int s1_bypass = 1;
	int fast = 1;
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

	if (priv->bypass_s1_smmu) {
		ret = iommu_domain_set_attr(mapping->domain,
					    DOMAIN_ATTR_S1_BYPASS,
					    &s1_bypass);
		if (ret < 0) {
			icnss_pr_err("Set s1_bypass attribute failed, err = %d\n",
				     ret);
			goto set_attr_fail;
		}
		icnss_pr_dbg("SMMU S1 BYPASS\n");
	} else {
		ret = iommu_domain_set_attr(mapping->domain,
					    DOMAIN_ATTR_ATOMIC,
					    &atomic_ctx);
		if (ret < 0) {
			icnss_pr_err("Set atomic_ctx attribute failed, err = %d\n",
				     ret);
			goto set_attr_fail;
		}
		icnss_pr_dbg("SMMU ATTR ATOMIC\n");

		ret = iommu_domain_set_attr(mapping->domain,
					    DOMAIN_ATTR_FAST,
					    &fast);
		if (ret < 0) {
			icnss_pr_err("Set fast map attribute failed, err = %d\n",
				     ret);
			goto set_attr_fail;
		}
		icnss_pr_dbg("SMMU FAST map set\n");
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

static int icnss_fw_debug_show(struct seq_file *s, void *data)
{
	struct icnss_priv *priv = s->private;

	seq_puts(s, "\nUsage: echo <CMD> <VAL> > <DEBUGFS>/icnss/fw_debug\n");

	seq_puts(s, "\nCMD: test_mode\n");
	seq_puts(s, "  VAL: 0 (Test mode disable)\n");
	seq_puts(s, "  VAL: 1 (WLAN FW test)\n");
	seq_puts(s, "  VAL: 2 (CCPM test)\n");
	seq_puts(s, "  VAL: 3 (Trigger Recovery)\n");

	seq_puts(s, "\nCMD: dynamic_feature_mask\n");
	seq_puts(s, "  VAL: (64 bit feature mask)\n");

	if (!test_bit(ICNSS_FW_READY, &priv->state)) {
		seq_puts(s, "Firmware is not ready yet, can't run test_mode!\n");
		goto out;
	}

	if (test_bit(ICNSS_DRIVER_PROBED, &priv->state)) {
		seq_puts(s, "Machine mode is running, can't run test_mode!\n");
		goto out;
	}

	if (test_bit(ICNSS_FW_TEST_MODE, &priv->state)) {
		seq_puts(s, "test_mode is running, can't run test_mode!\n");
		goto out;
	}

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

	icnss_wlan_disable(&priv->pdev->dev, ICNSS_OFF);

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

	ret = icnss_wlan_enable(&priv->pdev->dev, NULL, mode, NULL);
	if (ret)
		goto power_off;

	return 0;

power_off:
	icnss_hw_power_off(priv);
	clear_bit(ICNSS_FW_TEST_MODE, &priv->state);

out:
	return ret;
}

static ssize_t icnss_fw_debug_write(struct file *fp,
				    const char __user *user_buf,
				    size_t count, loff_t *off)
{
	struct icnss_priv *priv =
		((struct seq_file *)fp->private_data)->private;
	char buf[64];
	char *sptr, *token;
	unsigned int len = 0;
	char *cmd;
	uint64_t val;
	const char *delim = " ";
	int ret = 0;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EINVAL;

	buf[len] = '\0';
	sptr = buf;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;
	if (!sptr)
		return -EINVAL;
	cmd = token;

	token = strsep(&sptr, delim);
	if (!token)
		return -EINVAL;
	if (kstrtou64(token, 0, &val))
		return -EINVAL;

	if (strcmp(cmd, "test_mode") == 0) {
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
		case 3:
			ret = icnss_trigger_recovery(&priv->pdev->dev);
			break;
		default:
			return -EINVAL;
		}
	} else if (strcmp(cmd, "dynamic_feature_mask") == 0) {
		ret = wlfw_dynamic_feature_mask_send_sync_msg(priv, val);
	} else {
		return -EINVAL;
	}

	if (ret)
		return ret;

	return count;
}

static int icnss_fw_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, icnss_fw_debug_show, inode->i_private);
}

static const struct file_operations icnss_fw_debug_fops = {
	.read		= seq_read,
	.write		= icnss_fw_debug_write,
	.release	= single_release,
	.open		= icnss_fw_debug_open,
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
		case ICNSS_SSR_REGISTERED:
			seq_puts(s, "SSR REGISTERED");
			continue;
		case ICNSS_PDR_REGISTERED:
			seq_puts(s, "PDR REGISTERED");
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
		case ICNSS_SHUTDOWN_DONE:
			seq_puts(s, "SHUTDOWN DONE");
			continue;
		case ICNSS_HOST_TRIGGERED_PDR:
			seq_puts(s, "HOST TRIGGERED PDR");
			continue;
		case ICNSS_FW_DOWN:
			seq_puts(s, "FW DOWN");
			continue;
		case ICNSS_DRIVER_UNLOADING:
			seq_puts(s, "DRIVER UNLOADING");
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
		seq_printf(s, "Firmware Build ID: %s\n",
			   priv->fw_build_id);
	}

	return 0;
}

static int icnss_stats_show_rejuvenate_info(struct seq_file *s,
					    struct icnss_priv *priv)
{
	if (priv->stats.rejuvenate_ind)  {
		seq_puts(s, "\n<---------------- Rejuvenate Info ----------------->\n");
		seq_printf(s, "Number of Rejuvenations: %u\n",
			   priv->stats.rejuvenate_ind);
		seq_printf(s, "Cause for Rejuvenation: 0x%x\n",
			   priv->cause_for_rejuvenation);
		seq_printf(s, "Requesting Sub-System: 0x%x\n",
			   priv->requesting_sub_system);
		seq_printf(s, "Line Number: %u\n",
			   priv->line_number);
		seq_printf(s, "Function Name: %s\n",
			   priv->function_name);
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
	ICNSS_STATS_DUMP(s, priv, rejuvenate_ind);
	ICNSS_STATS_DUMP(s, priv, rejuvenate_ack_req);
	ICNSS_STATS_DUMP(s, priv, rejuvenate_ack_resp);
	ICNSS_STATS_DUMP(s, priv, rejuvenate_ack_err);
	ICNSS_STATS_DUMP(s, priv, recovery.pdr_fw_crash);
	ICNSS_STATS_DUMP(s, priv, recovery.pdr_host_error);
	ICNSS_STATS_DUMP(s, priv, recovery.root_pd_crash);
	ICNSS_STATS_DUMP(s, priv, recovery.root_pd_shutdown);

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

	icnss_stats_show_rejuvenate_info(s, priv);

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

	mutex_lock(&priv->dev_lock);
	if (!priv->diag_reg_read_buf) {
		seq_puts(s, "Usage: echo <mem_type> <offset> <data_len> > <debugfs>/icnss/reg_read\n");

		if (!test_bit(ICNSS_FW_READY, &priv->state))
			seq_puts(s, "Firmware is not ready yet!, wait for FW READY\n");

		mutex_unlock(&priv->dev_lock);
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
	mutex_unlock(&priv->dev_lock);

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

	mutex_lock(&priv->dev_lock);
	kfree(priv->diag_reg_read_buf);
	priv->diag_reg_read_buf = NULL;

	reg_buf = kzalloc(data_len, GFP_KERNEL);
	if (!reg_buf) {
		mutex_unlock(&priv->dev_lock);
		return -ENOMEM;
	}

	ret = wlfw_athdiag_read_send_sync_msg(priv, reg_offset,
					      mem_type, data_len,
					      reg_buf);
	if (ret) {
		kfree(reg_buf);
		mutex_unlock(&priv->dev_lock);
		return ret;
	}

	priv->diag_reg_read_addr = reg_offset;
	priv->diag_reg_read_mem_type = mem_type;
	priv->diag_reg_read_len = data_len;
	priv->diag_reg_read_buf = reg_buf;
	mutex_unlock(&priv->dev_lock);

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

#ifdef CONFIG_ICNSS_DEBUG
static int icnss_debugfs_create(struct icnss_priv *priv)
{
	int ret = 0;
	struct dentry *root_dentry;

	root_dentry = debugfs_create_dir("icnss", NULL);

	if (IS_ERR(root_dentry)) {
		ret = PTR_ERR(root_dentry);
		icnss_pr_err("Unable to create debugfs %d\n", ret);
		goto out;
	}

	priv->root_dentry = root_dentry;

	debugfs_create_file("fw_debug", 0600, root_dentry, priv,
			    &icnss_fw_debug_fops);

	debugfs_create_file("stats", 0600, root_dentry, priv,
			    &icnss_stats_fops);
	debugfs_create_file("reg_read", 0600, root_dentry, priv,
			    &icnss_regread_fops);
	debugfs_create_file("reg_write", 0600, root_dentry, priv,
			    &icnss_regwrite_fops);

out:
	return ret;
}
#else
static int icnss_debugfs_create(struct icnss_priv *priv)
{
	int ret = 0;
	struct dentry *root_dentry;

	root_dentry = debugfs_create_dir("icnss", NULL);

	if (IS_ERR(root_dentry)) {
		ret = PTR_ERR(root_dentry);
		icnss_pr_err("Unable to create debugfs %d\n", ret);
		return ret;
	}

	priv->root_dentry = root_dentry;

	debugfs_create_file("stats", 0600, root_dentry, priv,
			    &icnss_stats_fops);
	return 0;
}
#endif

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
	const __be32 *addrp;
	u64 prop_size = 0;
	struct device_node *np;

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

	if (of_property_read_bool(pdev->dev.of_node, "qcom,smmu-s1-bypass"))
		priv->bypass_s1_smmu = true;

	icnss_pr_dbg("SMMU S1 BYPASS = %d\n", priv->bypass_s1_smmu);

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

	np = of_parse_phandle(dev->of_node,
			      "qcom,wlan-msa-fixed-region", 0);
	if (np) {
		addrp = of_get_address(np, 0, &prop_size, NULL);
		if (!addrp) {
			icnss_pr_err("Failed to get assigned-addresses or property\n");
			ret = -EINVAL;
			goto out;
		}

		priv->msa_pa = of_translate_address(np, addrp);
		if (priv->msa_pa == OF_BAD_ADDR) {
			icnss_pr_err("Failed to translate MSA PA from device-tree\n");
			ret = -EINVAL;
			goto out;
		}

		priv->msa_va = memremap(priv->msa_pa,
					(unsigned long)prop_size, MEMREMAP_WT);
		if (!priv->msa_va) {
			icnss_pr_err("MSA PA ioremap failed: phy addr: %pa\n",
				     &priv->msa_pa);
			ret = -EINVAL;
			goto out;
		}
		priv->msa_mem_size = prop_size;
	} else {
		ret = of_property_read_u32(dev->of_node, "qcom,wlan-msa-memory",
					   &priv->msa_mem_size);
		if (ret || priv->msa_mem_size == 0) {
			icnss_pr_err("Fail to get MSA Memory Size: %u ret: %d\n",
				     priv->msa_mem_size, ret);
			goto out;
		}

		priv->msa_va = dmam_alloc_coherent(&pdev->dev,
				priv->msa_mem_size, &priv->msa_pa, GFP_KERNEL);

		if (!priv->msa_va) {
			icnss_pr_err("DMA alloc failed for MSA\n");
			ret = -ENOMEM;
			goto out;
		}
	}

	icnss_pr_dbg("MSA pa: %pa, MSA va: 0x%p MSA Memory Size: 0x%x\n",
		     &priv->msa_pa, (void *)priv->msa_va, priv->msa_mem_size);

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
	mutex_init(&priv->dev_lock);

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

	ret = device_init_wakeup(&priv->pdev->dev, true);
	if (ret)
		icnss_pr_err("Failed to init platform device wakeup source, err = %d\n",
			     ret);

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

	device_init_wakeup(&penv->pdev->dev, false);

	icnss_debugfs_destroy(penv);

	icnss_modem_ssr_unregister_notifier(penv);

	destroy_ramdump_device(penv->msa0_dump_dev);

	icnss_pdr_unregister_notifier(penv);

	qmi_svc_event_notifier_unregister(WLFW_SERVICE_ID_V01,
					  WLFW_SERVICE_VERS_V01,
					  WLFW_SERVICE_INS_ID_V01,
					  &wlfw_clnt_nb);
	if (penv->event_wq)
		destroy_workqueue(penv->event_wq);

	icnss_hw_power_off(penv);

	icnss_assign_msa_perm_all(penv, ICNSS_MSA_PERM_HLOS_ALL);
	clear_bit(ICNSS_MSA0_ASSIGNED, &penv->state);

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

	icnss_pr_vdbg("PM Suspend, state: 0x%lx\n", priv->state);

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

	icnss_pr_vdbg("PM resume, state: 0x%lx\n", priv->state);

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

	icnss_pr_vdbg("PM suspend_noirq, state: 0x%lx\n", priv->state);

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

	icnss_pr_vdbg("PM resume_noirq, state: 0x%lx\n", priv->state);

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

static int __init icnss_initialize(void)
{
	icnss_ipc_log_context = ipc_log_context_create(NUM_LOG_PAGES,
						       "icnss", 0);
	if (!icnss_ipc_log_context)
		icnss_pr_err("Unable to create log context\n");

	icnss_ipc_log_long_context = ipc_log_context_create(NUM_LOG_LONG_PAGES,
						       "icnss_long", 0);
	if (!icnss_ipc_log_long_context)
		icnss_pr_err("Unable to create log long context\n");

	return platform_driver_register(&icnss_driver);
}

static void __exit icnss_exit(void)
{
	platform_driver_unregister(&icnss_driver);
	ipc_log_context_destroy(icnss_ipc_log_context);
	icnss_ipc_log_context = NULL;
	ipc_log_context_destroy(icnss_ipc_log_long_context);
	icnss_ipc_log_long_context = NULL;
}


module_init(icnss_initialize);
module_exit(icnss_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(DEVICE "iCNSS CORE platform driver");
