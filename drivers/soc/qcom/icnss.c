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
#include <soc/qcom/memory_dump.h>
#include <soc/qcom/icnss.h>
#include <soc/qcom/msm_qmi_interface.h>
#include <soc/qcom/secure_buffer.h>

#include "wlan_firmware_service_v01.h"

#define ICNSS_PANIC			1
#define WLFW_TIMEOUT_MS			3000
#define WLFW_SERVICE_INS_ID_V01		0
#define SMMU_CLOCK_NAME			"smmu_aggre2_noc_clk"
#define MAX_PROP_SIZE			32
#define MAX_VOLTAGE_LEVEL		2
#define VREG_ON				1
#define VREG_OFF			0
#define MPM2_MPM_WCSSAON_CONFIG_OFFSET	0x18
#define NUM_LOG_PAGES			4

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
	HW_ALWAY_ON,
	HW_DEBUG_ENABLE,
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
	u32 nominal_min;
	u32 max_voltage;
	bool state;
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

static struct icnss_data {
	struct platform_device *pdev;
	struct icnss_driver_ops *ops;
	struct ce_irq_list ce_irq_list[ICNSS_MAX_IRQ_REGISTRATIONS];
	struct icnss_vreg_info vreg_info;
	u32 ce_irqs[ICNSS_MAX_IRQ_REGISTRATIONS];
	phys_addr_t mem_base_pa;
	void __iomem *mem_base_va;
	phys_addr_t mpm_config_pa;
	void __iomem *mpm_config_va;
	struct dma_iommu_mapping *smmu_mapping;
	dma_addr_t smmu_iova_start;
	size_t smmu_iova_len;
	struct clk *smmu_clk;
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
	bool skip_qmi;
	struct dentry *root_dentry;
	spinlock_t on_off_lock;
	struct icnss_stats stats;
} *penv;

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

static int icnss_vreg_on(struct icnss_vreg_info *vreg_info)
{
	int ret = 0;

	if (!vreg_info->reg) {
		icnss_pr_err("regulator is not initialized\n");
		return -ENOENT;
	}

	if (!vreg_info->max_voltage || !vreg_info->nominal_min) {
		icnss_pr_err("%s invalid constraints specified\n",
			     vreg_info->name);
		return -EINVAL;
	}

	ret = regulator_set_voltage(vreg_info->reg,
			vreg_info->nominal_min, vreg_info->max_voltage);
	if (ret < 0) {
		icnss_pr_err("regulator_set_voltage failed for (%s). min_uV=%d,max_uV=%d,ret=%d\n",
			     vreg_info->name, vreg_info->nominal_min,
			     vreg_info->max_voltage, ret);
		return ret;
	}

	ret = regulator_enable(vreg_info->reg);
	if (ret < 0) {
		icnss_pr_err("Fail to enable regulator (%s) ret=%d\n",
			     vreg_info->name, ret);
	}

	return ret;
}

static int icnss_vreg_off(struct icnss_vreg_info *vreg_info)
{
	int ret = 0;
	int min_uV = 0;

	if (!vreg_info->reg) {
		icnss_pr_err("Regulator is not initialized\n");
		return -ENOENT;
	}

	ret = regulator_disable(vreg_info->reg);
	if (ret < 0) {
		icnss_pr_err("Fail to disable regulator (%s) ret=%d\n",
			vreg_info->name, ret);
		return ret;
	}

	ret = regulator_set_voltage(vreg_info->reg,
				    min_uV, vreg_info->max_voltage);
	if (ret < 0) {
		icnss_pr_err("regulator_set_voltage failed for (%s). min_uV=%d,max_uV=%d,ret=%d\n",
			vreg_info->name, min_uV,
			vreg_info->max_voltage, ret);
	}
	return ret;
}

static int icnss_vreg_set(bool state)
{
	int ret = 0;
	struct icnss_vreg_info *vreg_info = &penv->vreg_info;

	if (vreg_info->state == state) {
		icnss_pr_dbg("Already %s state is %s\n", vreg_info->name,
			state ? "enabled" : "disabled");
		return ret;
	}

	if (state)
		ret = icnss_vreg_on(vreg_info);
	else
		ret = icnss_vreg_off(vreg_info);

	if (ret < 0)
		goto out;
	else
		ret = 0;

	icnss_pr_dbg("Regulator %s is now %s\n", vreg_info->name,
			state ? "enabled" : "disabled");

	vreg_info->state = state;
out:
	return ret;
}

static void icnss_hw_release_reset(struct icnss_data *pdata)
{
	uint32_t rdata = 0;

	icnss_pr_dbg("HW Release reset: state: 0x%lx\n", pdata->state);

	if (penv->mpm_config_va) {
		writel_relaxed(0x1,
			       penv->mpm_config_va +
			       MPM2_MPM_WCSSAON_CONFIG_OFFSET);
		while (rdata != 0x1)
			rdata = readl_relaxed(penv->mpm_config_va +
					      MPM2_MPM_WCSSAON_CONFIG_OFFSET);
	}
}

static void icnss_hw_reset(struct icnss_data *pdata)
{
	uint32_t rdata = 0;

	icnss_pr_dbg("HW reset: state: 0x%lx\n", pdata->state);

	if (penv->mpm_config_va) {
		writel_relaxed(0x0,
			       penv->mpm_config_va +
			       MPM2_MPM_WCSSAON_CONFIG_OFFSET);
		while (rdata != 0x0)
			rdata = readl_relaxed(penv->mpm_config_va +
					      MPM2_MPM_WCSSAON_CONFIG_OFFSET);
	}
}

static int icnss_hw_power_on(struct icnss_data *pdata)
{
	int ret = 0;
	unsigned long flags;

	icnss_pr_dbg("Power on: state: 0x%lx\n", pdata->state);

	spin_lock_irqsave(&pdata->on_off_lock, flags);
	if (test_bit(ICNSS_POWER_ON, &pdata->state)) {
		spin_unlock_irqrestore(&pdata->on_off_lock, flags);
		return ret;
	}
	set_bit(ICNSS_POWER_ON, &pdata->state);
	spin_unlock_irqrestore(&pdata->on_off_lock, flags);

	ret = icnss_vreg_set(VREG_ON);
	if (ret)
		goto out;

	icnss_hw_release_reset(pdata);

	return ret;
out:
	clear_bit(ICNSS_POWER_ON, &pdata->state);
	return ret;
}

static int icnss_hw_power_off(struct icnss_data *pdata)
{
	int ret = 0;
	unsigned long flags;

	icnss_pr_dbg("Power off: 0x%lx\n", pdata->state);

	spin_lock_irqsave(&pdata->on_off_lock, flags);
	if (!test_bit(ICNSS_POWER_ON, &pdata->state)) {
		spin_unlock_irqrestore(&pdata->on_off_lock, flags);
		return ret;
	}
	clear_bit(ICNSS_POWER_ON, &pdata->state);
	spin_unlock_irqrestore(&pdata->on_off_lock, flags);

	icnss_hw_reset(pdata);

	ret = icnss_vreg_set(VREG_OFF);
	if (ret)
		goto out;

	return ret;
out:
	set_bit(ICNSS_POWER_ON, &pdata->state);
	return ret;
}

int icnss_power_on(struct device *dev)
{
	struct icnss_data *priv = dev_get_drvdata(dev);

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
	struct icnss_data *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %p, data %p\n",
			     dev, priv);
		return -EINVAL;
	}

	return icnss_hw_power_off(priv);
}
EXPORT_SYMBOL(icnss_power_off);

int icnss_map_msa_permissions(struct icnss_data *priv, u32 index)
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

int icnss_unmap_msa_permissions(struct icnss_data *priv, u32 index)
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

static int icnss_setup_msa_permissions(struct icnss_data *priv)
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

static void icnss_remove_msa_permissions(struct icnss_data *priv)
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

	if (!penv->pdev) {
		icnss_pr_err("Device is not ready\n");
		ret = -ENODEV;
		goto out;
	}

	/*
	 * WAR required after FW ready without which CCPM init fails in firmware
	 * when WLAN enable is sent to firmware
	 */
	icnss_hw_reset(penv);
	usleep_range(100, 102);
	icnss_hw_release_reset(penv);

	if (!penv->ops || !penv->ops->probe)
		goto out;

	ret = penv->ops->probe(&penv->pdev->dev);
	if (ret < 0) {
		icnss_pr_err("Driver probe failed: %d\n", ret);
		goto out;
	}

	set_bit(ICNSS_DRIVER_PROBED, &penv->state);

	return 0;
out:
	icnss_hw_power_off(penv);
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

	if (penv->skip_qmi)
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
	if (penv->skip_qmi)
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

static struct clk *icnss_clock_init(struct device *dev, const char *cname)
{
	struct clk *c;
	long rate;

	if (of_property_match_string(dev->of_node, "clock-names", cname) < 0) {
		icnss_pr_err("Clock %s not found!", cname);
		return NULL;
	}

	c = devm_clk_get(dev, cname);
	if (IS_ERR(c)) {
		icnss_pr_err("Couldn't get clock %s!", cname);
		return NULL;
	}

	if (clk_get_rate(c) == 0) {
		rate = clk_round_rate(c, 1000);
		clk_set_rate(c, rate);
	}

	return c;
}

static int icnss_clock_enable(struct clk *c)
{
	int ret = 0;

	ret = clk_prepare_enable(c);

	if (ret < 0)
		icnss_pr_err("Couldn't enable clock: %d!\n", ret);
	return ret;
}

static void icnss_clock_disable(struct clk *c)
{
	clk_disable_unprepare(c);
}

static int icnss_smmu_init(struct device *dev)
{
	struct dma_iommu_mapping *mapping;
	int disable_htw = 1;
	int atomic_ctx = 1;
	int s1_bypass = 1;
	int ret = 0;

	icnss_pr_dbg("Initializing SMMU\n");

	mapping = arm_iommu_create_mapping(&platform_bus_type,
					   penv->smmu_iova_start,
					   penv->smmu_iova_len);
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

	ret = arm_iommu_attach_device(dev, mapping);
	if (ret < 0) {
		icnss_pr_err("Attach device failed, err = %d\n", ret);
		goto attach_fail;
	}

	penv->smmu_mapping = mapping;

	return ret;

attach_fail:
set_attr_fail:
	arm_iommu_release_mapping(mapping);
map_fail:
	return ret;
}

static void icnss_smmu_remove(struct device *dev)
{
	arm_iommu_detach_device(dev);
	arm_iommu_release_mapping(penv->smmu_mapping);

	penv->smmu_mapping = NULL;
}

static int icnss_dt_parse_vreg_info(struct device *dev,
				struct icnss_vreg_info *vreg_info,
				const char *vreg_name)
{
	int ret = 0;
	u32 voltage_levels[MAX_VOLTAGE_LEVEL];
	char prop_name[MAX_PROP_SIZE];
	struct device_node *np = dev->of_node;

	snprintf(prop_name, MAX_PROP_SIZE, "%s-supply", vreg_name);
	if (!of_parse_phandle(np, prop_name, 0)) {
		icnss_pr_err("No vreg data found for %s\n", vreg_name);
		ret = -EINVAL;
		return ret;
	}

	vreg_info->name = vreg_name;

	snprintf(prop_name, MAX_PROP_SIZE,
		"qcom,%s-voltage-level", vreg_name);
	ret = of_property_read_u32_array(np, prop_name, voltage_levels,
					ARRAY_SIZE(voltage_levels));
	if (ret) {
		icnss_pr_err("Error reading %s property\n", prop_name);
		return ret;
	}

	vreg_info->nominal_min = voltage_levels[0];
	vreg_info->max_voltage = voltage_levels[1];

	return ret;
}

static int icnss_get_resources(struct device *dev)
{
	int ret = 0;
	struct icnss_vreg_info *vreg_info;

	vreg_info = &penv->vreg_info;
	if (vreg_info->reg) {
		icnss_pr_err("%s regulator is already initialized\n",
		       vreg_info->name);
		return ret;
	}

	vreg_info->reg = devm_regulator_get(dev, vreg_info->name);
	if (IS_ERR(vreg_info->reg)) {
		ret = PTR_ERR(vreg_info->reg);
		if (ret == -EPROBE_DEFER) {
			icnss_pr_err("%s probe deferred!\n", vreg_info->name);
		} else {
			icnss_pr_err("Get %s failed!\n", vreg_info->name);
		}
	}
	return ret;
}

static int icnss_release_resources(void)
{
	int ret = 0;
	struct icnss_vreg_info *vreg_info = &penv->vreg_info;

	if (!vreg_info->reg) {
		icnss_pr_err("Regulator is not initialized\n");
		return -ENOENT;
	}

	devm_regulator_put(vreg_info->reg);
	return ret;
}

static int icnss_test_mode_show(struct seq_file *s, void *data)
{
	struct icnss_data *priv = s->private;

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

static int icnss_test_mode_fw_test_off(struct icnss_data *priv)
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
static int icnss_test_mode_fw_test(struct icnss_data *priv,
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
	struct icnss_data *priv =
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
	struct icnss_data *priv =
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

static int icnss_stats_show_state(struct seq_file *s, struct icnss_data *priv)
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
				       struct icnss_data *priv)
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

static int icnss_stats_show_events(struct seq_file *s, struct icnss_data *priv)
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

static int icnss_stats_show_irqs(struct seq_file *s, struct icnss_data *priv)
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

	struct icnss_data *priv = s->private;

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

static int icnss_debugfs_create(struct icnss_data *priv)
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

static void icnss_debugfs_destroy(struct icnss_data *priv)
{
	debugfs_remove_recursive(priv->root_dentry);
}

static int icnss_probe(struct platform_device *pdev)
{
	int ret = 0;
	u32 smmu_iova_address[2];
	struct resource *res;
	int i;
	struct device *dev = &pdev->dev;

	if (penv) {
		icnss_pr_err("penv is already initialized\n");
		return -EEXIST;
	}

	penv = devm_kzalloc(&pdev->dev, sizeof(*penv), GFP_KERNEL);
	if (!penv)
		return -ENOMEM;

	dev_set_drvdata(dev, penv);

	penv->pdev = pdev;

	ret = icnss_dt_parse_vreg_info(dev, &penv->vreg_info, "vdd-io");
	if (ret < 0) {
		icnss_pr_err("Failed to parse vdd io data: %d\n", ret);
		goto out;
	}

	ret = icnss_get_resources(dev);
	if (ret < 0) {
		icnss_pr_err("Regulator setup failed (%d)\n", ret);
		goto out;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "membase");
	if (!res) {
		icnss_pr_err("Memory base not found\n");
		ret = -EINVAL;
		goto release_regulator;
	}
	penv->mem_base_pa = res->start;
	penv->mem_base_va = ioremap(penv->mem_base_pa, resource_size(res));
	if (!penv->mem_base_va) {
		icnss_pr_err("mem_base ioremap failed\n");
		ret = -EINVAL;
		goto release_regulator;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "mpm_config");
	if (!res) {
		icnss_pr_err("mpm_config not found\n");
		ret = -EINVAL;
		goto unmap_mem_base;
	}
	penv->mpm_config_pa = res->start;
	penv->mpm_config_va = ioremap(penv->mpm_config_pa, resource_size(res));
	if (!penv->mpm_config_va) {
		icnss_pr_err("mpm_config ioremap failed, phy addr: %pa\n",
			     &penv->mpm_config_pa);
		ret = -EINVAL;
		goto unmap_mem_base;
	}
	icnss_pr_dbg("mpm_config_pa: %pa, mpm_config_va: %p\n",
		     &penv->mpm_config_pa, penv->mpm_config_va);

	for (i = 0; i < ICNSS_MAX_IRQ_REGISTRATIONS; i++) {
		res = platform_get_resource(pdev, IORESOURCE_IRQ, i);
		if (!res) {
			icnss_pr_err("Fail to get IRQ-%d\n", i);
			ret = -ENODEV;
			goto unmap_mpm_config;
		} else {
			penv->ce_irqs[i] = res->start;
		}
	}

	if (of_property_read_u32(dev->of_node, "qcom,wlan-msa-memory",
				 &penv->msa_mem_size) == 0) {
		if (penv->msa_mem_size) {
			penv->msa_va = dma_alloc_coherent(&pdev->dev,
							  penv->msa_mem_size,
							  &penv->msa_pa,
							  GFP_KERNEL);
			if (!penv->msa_va) {
				icnss_pr_err("DMA alloc failed for MSA\n");
				ret = -EINVAL;
				goto unmap_mpm_config;
			}

			icnss_pr_dbg("MSA va: %p, MSA pa: %pa\n", penv->msa_va,
				     &penv->msa_pa);
		}
	} else {
		icnss_pr_err("Fail to get MSA Memory Size\n");
		ret = -ENODEV;
		goto unmap_mpm_config;
	}

	if (of_property_read_u32_array(pdev->dev.of_node,
				       "qcom,wlan-smmu-iova-address",
				       smmu_iova_address, 2) == 0) {
		penv->smmu_iova_start = smmu_iova_address[0];
		penv->smmu_iova_len = smmu_iova_address[1];

		ret = icnss_smmu_init(&pdev->dev);
		if (ret < 0) {
			icnss_pr_err("SMMU init failed, err = %d, start: %pad, len: %zx\n",
				     ret, &penv->smmu_iova_start,
				     penv->smmu_iova_len);
			goto err_smmu_init;
		}

		penv->smmu_clk = icnss_clock_init(&pdev->dev, SMMU_CLOCK_NAME);
		if (penv->smmu_clk) {
			ret = icnss_clock_enable(penv->smmu_clk);
			if (ret < 0)
				goto err_smmu_clock_enable;
		}
	}

	penv->skip_qmi = of_property_read_bool(dev->of_node,
					       "qcom,skip-qmi");

	spin_lock_init(&penv->event_lock);
	spin_lock_init(&penv->on_off_lock);

	penv->event_wq = alloc_workqueue("icnss_driver_event", 0, 0);
	if (!penv->event_wq) {
		icnss_pr_err("Workqueue creation failed\n");
		ret = -EFAULT;
		goto err_smmu_clock_enable;
	}

	INIT_WORK(&penv->event_work, icnss_driver_event_work);
	INIT_WORK(&penv->qmi_recv_msg_work, icnss_qmi_wlfw_clnt_notify_work);
	INIT_LIST_HEAD(&penv->event_list);

	ret = qmi_svc_event_notifier_register(WLFW_SERVICE_ID_V01,
					      WLFW_SERVICE_VERS_V01,
					      WLFW_SERVICE_INS_ID_V01,
					      &wlfw_clnt_nb);
	if (ret < 0) {
		icnss_pr_err("Notifier register failed: %d\n", ret);
		goto err_qmi;
	}

	icnss_debugfs_create(penv);

	icnss_pr_info("Platform driver probed successfully\n");

	return ret;

err_qmi:
	if (penv->event_wq)
		destroy_workqueue(penv->event_wq);
err_smmu_clock_enable:
	if (penv->smmu_mapping)
		icnss_smmu_remove(&pdev->dev);
err_smmu_init:
	if (penv->msa_va)
		dma_free_coherent(&pdev->dev, penv->msa_mem_size,
				  penv->msa_va, penv->msa_pa);
unmap_mpm_config:
	if (penv->mpm_config_va)
		iounmap(penv->mpm_config_va);
unmap_mem_base:
	if (penv->mem_base_va)
		iounmap(penv->mem_base_va);
release_regulator:
	icnss_release_resources();
out:
	dev_set_drvdata(dev, NULL);
	devm_kfree(&pdev->dev, penv);
	penv = NULL;
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

	if (penv->smmu_mapping) {
		if (penv->smmu_clk)
			icnss_clock_disable(penv->smmu_clk);
		icnss_smmu_remove(&pdev->dev);
	}

	if (penv->msa_va)
		dma_free_coherent(&pdev->dev, penv->msa_mem_size,
				  penv->msa_va, penv->msa_pa);
	if (penv->mpm_config_va)
		iounmap(penv->mpm_config_va);
	if (penv->mem_base_va)
		iounmap(penv->mem_base_va);

	icnss_hw_power_off(penv);

	icnss_release_resources();

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
