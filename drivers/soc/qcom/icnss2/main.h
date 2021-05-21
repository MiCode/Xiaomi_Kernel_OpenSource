/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __MAIN_H__
#define __MAIN_H__

#include <linux/adc-tm-clients.h>
#include <linux/iio/consumer.h>
#include <linux/irqreturn.h>
#include <asm/dma-iommu.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/ipc_logging.h>
#include <dt-bindings/iio/qcom,spmi-vadc.h>
#include <soc/qcom/icnss2.h>
#include <soc/qcom/service-locator.h>
#include <soc/qcom/service-notifier.h>
#include "wlan_firmware_service_v01.h"

#define WCN6750_DEVICE_ID 0x6750
#define ADRASTEA_DEVICE_ID 0xabcd
#define QMI_WLFW_MAX_NUM_MEM_SEG 32
#define THERMAL_NAME_LENGTH 20
#define ICNSS_SMEM_VALUE_MASK 0xFFFFFFFF
#define ICNSS_SMEM_SEQ_NO_POS 16
#define ICNSS_PCI_EP_WAKE_OFFSET 4

extern uint64_t dynamic_feature_mask;

enum icnss_bdf_type {
	ICNSS_BDF_BIN,
	ICNSS_BDF_ELF,
	ICNSS_BDF_REGDB = 4,
	ICNSS_BDF_DUMMY = 255,
};

struct icnss_control_params {
	unsigned long quirks;
	unsigned int qmi_timeout;
	unsigned int bdf_type;
};

enum icnss_driver_event_type {
	ICNSS_DRIVER_EVENT_SERVER_ARRIVE,
	ICNSS_DRIVER_EVENT_SERVER_EXIT,
	ICNSS_DRIVER_EVENT_FW_READY_IND,
	ICNSS_DRIVER_EVENT_REGISTER_DRIVER,
	ICNSS_DRIVER_EVENT_UNREGISTER_DRIVER,
	ICNSS_DRIVER_EVENT_PD_SERVICE_DOWN,
	ICNSS_DRIVER_EVENT_FW_EARLY_CRASH_IND,
	ICNSS_DRIVER_EVENT_IDLE_SHUTDOWN,
	ICNSS_DRIVER_EVENT_IDLE_RESTART,
	ICNSS_DRIVER_EVENT_FW_INIT_DONE_IND,
	ICNSS_DRIVER_EVENT_QDSS_TRACE_REQ_MEM,
	ICNSS_DRIVER_EVENT_QDSS_TRACE_SAVE,
	ICNSS_DRIVER_EVENT_QDSS_TRACE_FREE,
	ICNSS_DRIVER_EVENT_M3_DUMP_UPLOAD_REQ,
	ICNSS_DRIVER_EVENT_QDSS_TRACE_REQ_DATA,
	ICNSS_DRIVER_EVENT_MAX,
};

enum icnss_soc_wake_event_type {
	ICNSS_SOC_WAKE_REQUEST_EVENT,
	ICNSS_SOC_WAKE_RELEASE_EVENT,
	ICNSS_SOC_WAKE_EVENT_MAX,
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

struct icnss_soc_wake_event {
	struct list_head list;
	enum icnss_soc_wake_event_type type;
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
	ICNSS_WLFW_EXISTS,
	ICNSS_SHUTDOWN_DONE,
	ICNSS_HOST_TRIGGERED_PDR,
	ICNSS_FW_DOWN,
	ICNSS_DRIVER_UNLOADING,
	ICNSS_REJUVENATE,
	ICNSS_MODE_ON,
	ICNSS_BLOCK_SHUTDOWN,
	ICNSS_PDR,
	ICNSS_DEL_SERVER,
	ICNSS_COLD_BOOT_CAL,
};

struct ce_irq_list {
	int irq;
	irqreturn_t (*handler)(int irq, void *priv);
};

struct icnss_vreg_cfg {
	const char *name;
	u32 min_uv;
	u32 max_uv;
	u32 load_ua;
	u32 delay_us;
	u32 need_unvote;
	bool required;
};

struct icnss_vreg_info {
	struct list_head list;
	struct regulator *reg;
	struct icnss_vreg_cfg cfg;
	u32 enabled;
};

struct icnss_cpr_info {
	resource_size_t tcs_cmd_base_addr;
	resource_size_t tcs_cmd_data_addr;
	void __iomem *tcs_cmd_base_addr_io;
	void __iomem *tcs_cmd_data_addr_io;
	u32 cpr_pmic_addr;
	u32 voltage;
};

enum icnss_vreg_type {
	ICNSS_VREG_PRIM,
};
struct icnss_clk_cfg {
	const char *name;
	u32 freq;
	u32 required;
};

struct icnss_clk_info {
	struct list_head list;
	struct clk *clk;
	struct icnss_clk_cfg cfg;
	u32 enabled;
};

struct icnss_fw_mem {
	size_t size;
	void *va;
	phys_addr_t pa;
	u8 valid;
	u32 type;
	unsigned long attrs;
};

enum icnss_smp2p_msg_id {
	ICNSS_POWER_SAVE_ENTER = 1,
	ICNSS_POWER_SAVE_EXIT,
	ICNSS_TRIGGER_SSR,
	ICNSS_PCI_EP_POWER_SAVE_ENTER = 6,
	ICNSS_PCI_EP_POWER_SAVE_EXIT,
};

struct icnss_stats {
	struct {
		uint32_t posted;
		uint32_t processed;
	} events[ICNSS_DRIVER_EVENT_MAX];

	struct {
		u32 posted;
		u32 processed;
	} soc_wake_events[ICNSS_SOC_WAKE_EVENT_MAX];

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
	uint32_t device_info_req;
	uint32_t device_info_resp;
	uint32_t device_info_err;
	u32 exit_power_save_req;
	u32 exit_power_save_resp;
	u32 exit_power_save_err;
	u32 enter_power_save_req;
	u32 enter_power_save_resp;
	u32 enter_power_save_err;
	u32 soc_wake_req;
	u32 soc_wake_resp;
	u32 soc_wake_err;
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
#define WLFW_MAX_HANG_EVENT_DATA_SIZE 400

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

struct icnss_mem_region_info {
	uint64_t reg_addr;
	uint32_t size;
	uint8_t secure_flag;
};

struct icnss_msi_user {
	char *name;
	int num_vectors;
	u32 base_vector;
};

struct icnss_msi_config {
	int total_vectors;
	int total_users;
	struct icnss_msi_user *users;
};

struct icnss_thermal_cdev {
	struct list_head tcdev_list;
	int tcdev_id;
	unsigned long curr_thermal_state;
	unsigned long max_thermal_state;
	struct device_node *dev_node;
	struct thermal_cooling_device *tcdev;
};

struct smp2p_out_info {
	unsigned short seq;
	unsigned int smem_bit;
	struct qcom_smem_state *smem_state;
};

struct icnss_priv {
	uint32_t magic;
	struct platform_device *pdev;
	struct icnss_driver_ops *ops;
	struct ce_irq_list ce_irq_list[ICNSS_MAX_IRQ_REGISTRATIONS];
	struct list_head vreg_list;
	struct list_head clk_list;
	struct icnss_cpr_info cpr_info;
	unsigned long device_id;
	struct icnss_msi_config *msi_config;
	u32 msi_base_data;
	struct icnss_control_params ctrl_params;
	u8 cal_done;
	u32 ce_irqs[ICNSS_MAX_IRQ_REGISTRATIONS];
	u32 srng_irqs[IWCN_MAX_IRQ_REGISTRATIONS];
	phys_addr_t mem_base_pa;
	void __iomem *mem_base_va;
	u32 mem_base_size;
	phys_addr_t mhi_state_info_pa;
	void __iomem *mhi_state_info_va;
	u32 mhi_state_info_size;
	struct iommu_domain *iommu_domain;
	dma_addr_t smmu_iova_start;
	size_t smmu_iova_len;
	dma_addr_t smmu_iova_ipa_start;
	dma_addr_t smmu_iova_ipa_current;
	size_t smmu_iova_ipa_len;
	struct qmi_handle qmi;
	struct list_head event_list;
	struct list_head soc_wake_msg_list;
	spinlock_t event_lock;
	spinlock_t soc_wake_msg_lock;
	struct work_struct event_work;
	struct work_struct fw_recv_msg_work;
	struct work_struct soc_wake_msg_work;
	struct workqueue_struct *event_wq;
	struct workqueue_struct *soc_wake_wq;
	phys_addr_t msa_pa;
	phys_addr_t msi_addr_pa;
	dma_addr_t msi_addr_iova;
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
	struct ramdump_device *m3_dump_dev_seg1;
	struct ramdump_device *m3_dump_dev_seg2;
	struct ramdump_device *m3_dump_dev_seg3;
	struct ramdump_device *m3_dump_dev_seg4;
	struct ramdump_device *m3_dump_dev_seg5;
	bool force_err_fatal;
	bool allow_recursive_recovery;
	bool early_crash_ind;
	u8 cause_for_rejuvenation;
	u8 requesting_sub_system;
	u16 line_number;
	struct mutex dev_lock;
	uint32_t fw_error_fatal_irq;
	uint32_t fw_early_crash_irq;
	struct smp2p_out_info smp2p_info;
	struct completion unblock_shutdown;
	struct adc_tm_param vph_monitor_params;
	struct adc_tm_chip *adc_tm_dev;
	struct iio_channel *channel;
	uint64_t vph_pwr;
	bool vbatt_supported;
	char function_name[WLFW_FUNCTION_NAME_LEN + 1];
	bool is_ssr;
	bool smmu_s1_enable;
	struct kobject *icnss_kobject;
	atomic_t is_shutdown;
	u32 qdss_mem_seg_len;
	struct icnss_fw_mem qdss_mem[QMI_WLFW_MAX_NUM_MEM_SEG];
	void *get_info_cb_ctx;
	int (*get_info_cb)(void *ctx, void *event, int event_len);
	atomic_t soc_wake_ref_count;
	phys_addr_t hang_event_data_pa;
	void __iomem *hang_event_data_va;
	uint16_t hang_event_data_len;
	void *hang_event_data;
	struct list_head icnss_tcdev_list;
	struct mutex tcdev_lock;
	u32 hw_trc_override;
};

struct icnss_reg_info {
	uint32_t mem_type;
	uint32_t reg_offset;
	uint32_t data_len;
};

char *icnss_driver_event_to_str(enum icnss_driver_event_type type);
int icnss_call_driver_uevent(struct icnss_priv *priv,
				    enum icnss_uevent uevent, void *data);
int icnss_driver_event_post(struct icnss_priv *priv,
			    enum icnss_driver_event_type type,
			    u32 flags, void *data);
void icnss_allow_recursive_recovery(struct device *dev);
void icnss_disallow_recursive_recovery(struct device *dev);
char *icnss_soc_wake_event_to_str(enum icnss_soc_wake_event_type type);
int icnss_soc_wake_event_post(struct icnss_priv *priv,
			      enum icnss_soc_wake_event_type type,
			      u32 flags, void *data);
int icnss_get_iova(struct icnss_priv *priv, u64 *addr, u64 *size);
int icnss_get_iova_ipa(struct icnss_priv *priv, u64 *addr, u64 *size);
int icnss_get_cpr_info(struct icnss_priv *priv);
int icnss_update_cpr_info(struct icnss_priv *priv);
#endif

