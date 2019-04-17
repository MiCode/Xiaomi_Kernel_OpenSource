/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#ifndef __ICNSS_PRIVATE_H__
#define __ICNSS_PRIVATE_H__

#include <linux/adc-tm-clients.h>
#include <linux/iio/consumer.h>
#include <linux/kobject.h>

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
			BUG();						\
		}							\
	} while (0)
#else
#define ICNSS_ASSERT(_condition) do { } while (0)
#endif

#define icnss_fatal_err(_fmt, ...)					\
	icnss_pr_err("fatal: "_fmt, ##__VA_ARGS__)

enum icnss_debug_quirks {
	HW_ALWAYS_ON,
	HW_DEBUG_ENABLE,
	SKIP_QMI,
	HW_ONLY_TOP_LEVEL_RESET,
	RECOVERY_DISABLE,
	SSR_ONLY,
	PDR_ONLY,
	FW_REJUVENATE_ENABLE,
};

extern uint64_t dynamic_feature_mask;
extern void *icnss_ipc_log_context;
extern void *icnss_ipc_log_long_context;
extern unsigned long quirks;

enum icnss_driver_event_type {
	ICNSS_DRIVER_EVENT_SERVER_ARRIVE,
	ICNSS_DRIVER_EVENT_SERVER_EXIT,
	ICNSS_DRIVER_EVENT_FW_READY_IND,
	ICNSS_DRIVER_EVENT_REGISTER_DRIVER,
	ICNSS_DRIVER_EVENT_UNREGISTER_DRIVER,
	ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
	ICNSS_DRIVER_EVENT_FW_EARLY_CRASH_IND,
	ICNSS_DRIVER_EVENT_MAX,
};

struct icnss_event_server_arrive_data {
	unsigned int node;
	unsigned int port;
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
	ICNSS_WLFW_CONNECTED,
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
	ICNSS_REJUVENATE,
	ICNSS_MODE_ON,
	ICNSS_BLOCK_SHUTDOWN,
	ICNSS_PDR,
	ICNSS_CLK_UP,
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
	u32 rejuvenate_ind;
	uint32_t rejuvenate_ack_req;
	uint32_t rejuvenate_ack_resp;
	uint32_t rejuvenate_ack_err;
	uint32_t vbatt_req;
	uint32_t vbatt_resp;
	uint32_t vbatt_req_err;
};

#define WLFW_MAX_TIMESTAMP_LEN 32
#define WLFW_MAX_BUILD_ID_LEN 128
#define WLFW_MAX_NUM_MEMORY_REGIONS 2
#define WLFW_FUNCTION_NAME_LEN 129
#define WLFW_MAX_DATA_SIZE 6144
#define WLFW_MAX_STR_LEN 16
#define WLFW_MAX_NUM_CE 12
#define WLFW_MAX_NUM_SVC 24
#define WLFW_MAX_NUM_SHADOW_REG 24

struct service_notifier_context {
	void *handle;
	uint32_t instance_id;
	char name[QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1];
};

struct wlfw_rf_chip_info {
	uint32_t chip_id;
	uint32_t chip_family;
};

struct wlfw_rf_board_info {
	uint32_t board_id;
};

struct wlfw_fw_version_info {
	uint32_t fw_version;
	char fw_build_timestamp[WLFW_MAX_TIMESTAMP_LEN + 1];
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

struct icnss_priv {
	uint32_t magic;
	struct platform_device *pdev;
	struct icnss_driver_ops *ops;
	struct ce_irq_list ce_irq_list[ICNSS_MAX_IRQ_REGISTRATIONS];
	struct icnss_vreg_info *vreg_info;
	struct icnss_clk_info *clk_info;
	u32 ce_irqs[ICNSS_MAX_IRQ_REGISTRATIONS];
	phys_addr_t mem_base_pa;
	void __iomem *mem_base_va;
	struct dma_iommu_mapping *smmu_mapping;
	dma_addr_t smmu_iova_start;
	size_t smmu_iova_len;
	dma_addr_t smmu_iova_ipa_start;
	size_t smmu_iova_ipa_len;
	struct qmi_handle qmi;
	struct list_head event_list;
	spinlock_t event_lock;
	struct work_struct event_work;
	struct work_struct fw_recv_msg_work;
	struct workqueue_struct *event_wq;
	phys_addr_t msa_pa;
	uint32_t msa_mem_size;
	void *msa_va;
	unsigned long state;
	struct wlfw_rf_chip_info chip_info;
	uint32_t board_id;
	uint32_t soc_id;
	struct wlfw_fw_version_info fw_version_info;
	char fw_build_id[WLFW_MAX_BUILD_ID_LEN + 1];
	u32 pwr_pin_result;
	u32 phy_io_pin_result;
	u32 rf_pin_result;
	uint32_t nr_mem_region;
	struct icnss_mem_region_info
		mem_region[WLFW_MAX_NUM_MEMORY_REGIONS];
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
	atomic_t pm_count;
	struct ramdump_device *msa0_dump_dev;
	bool bypass_s1_smmu;
	bool force_err_fatal;
	bool allow_recursive_recovery;
	bool early_crash_ind;
	u8 cause_for_rejuvenation;
	u8 requesting_sub_system;
	u16 line_number;
	struct mutex dev_lock;
	bool is_hyp_disabled;
	uint32_t fw_error_fatal_irq;
	uint32_t fw_early_crash_irq;
	struct completion unblock_shutdown;
	struct adc_tm_param vph_monitor_params;
	struct adc_tm_chip *adc_tm_dev;
	struct iio_channel *channel;
	uint64_t vph_pwr;
	bool vbatt_supported;
	char function_name[WLFW_FUNCTION_NAME_LEN + 1];
	struct kobject *icnss_kobject;
	atomic_t is_shutdown;
	bool is_ssr;
	bool clk_monitor_enable;
	void *ext_modem_notify_handler;
	struct notifier_block ext_modem_ssr_nb;
	struct completion clk_complete;
};

int icnss_call_driver_uevent(struct icnss_priv *priv,
				    enum icnss_uevent uevent, void *data);
int icnss_driver_event_post(enum icnss_driver_event_type type,
				   u32 flags, void *data);
#endif

