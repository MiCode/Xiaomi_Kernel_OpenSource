/* Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
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

#include <linux/clk.h>
#include <linux/compat.h>
#include <linux/device.h>
#include <linux/dmapool.h>
#include <linux/fs.h>
#include <linux/genalloc.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/rbtree.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/netdevice.h>
#include <linux/delay.h>
#include <linux/msm_gsi.h>
#include <linux/time.h>
#include <linux/hashtable.h>
#include <linux/jhash.h>
#include <linux/pci.h>
#include <soc/qcom/subsystem_restart.h>
#include <linux/soc/qcom/smem.h>
#include <soc/qcom/scm.h>
#include <asm/cacheflush.h>
#include <linux/soc/qcom/smem_state.h>
#include <linux/of_irq.h>
#include <linux/ctype.h>

#ifdef CONFIG_ARM64

/* Outer caches unsupported on ARM64 platforms */
#define outer_flush_range(x, y)
#define __cpuc_flush_dcache_area __flush_dcache_area

#endif

#define IPA_SUBSYSTEM_NAME "ipa_fws"
#define IPA_UC_SUBSYSTEM_NAME "ipa_uc"

#include "ipa_i.h"
#include "../ipa_rm_i.h"
#include "ipahal/ipahal.h"
#include "ipahal/ipahal_fltrt.h"

#define CREATE_TRACE_POINTS
#include "ipa_trace.h"
#include "ipa_odl.h"

/*
 * The following for adding code (ie. for EMULATION) not found on x86.
 */
#if defined(CONFIG_IPA_EMULATION)
# include "ipa_emulation_stubs.h"
#endif

#ifdef CONFIG_COMPAT
/**
 * struct ipa3_ioc_nat_alloc_mem32 - nat table memory allocation
 * properties
 * @dev_name: input parameter, the name of table
 * @size: input parameter, size of table in bytes
 * @offset: output parameter, offset into page in case of system memory
 */
struct ipa3_ioc_nat_alloc_mem32 {
	char dev_name[IPA_RESOURCE_NAME_MAX];
	compat_size_t size;
	compat_off_t offset;
};

/**
 * struct ipa_ioc_nat_ipv6ct_table_alloc32 - table memory allocation
 * properties
 * @size: input parameter, size of table in bytes
 * @offset: output parameter, offset into page in case of system memory
 */
struct ipa_ioc_nat_ipv6ct_table_alloc32 {
	compat_size_t size;
	compat_off_t offset;
};
#endif /* #ifdef CONFIG_COMPAT */

#define IPA_TZ_UNLOCK_ATTRIBUTE 0x0C0311
#define TZ_MEM_PROTECT_REGION_ID 0x10

struct tz_smmu_ipa_protect_region_iovec_s {
	u64 input_addr;
	u64 output_addr;
	u64 size;
	u32 attr;
} __packed;

struct tz_smmu_ipa_protect_region_s {
	phys_addr_t iovec_buf;
	u32 size_bytes;
} __packed;

static void ipa3_start_tag_process(struct work_struct *work);
static DECLARE_WORK(ipa3_tag_work, ipa3_start_tag_process);

static void ipa3_transport_release_resource(struct work_struct *work);
static DECLARE_DELAYED_WORK(ipa3_transport_release_resource_work,
	ipa3_transport_release_resource);
static void ipa_gsi_notify_cb(struct gsi_per_notify *notify);

static int ipa3_attach_to_smmu(void);
static int ipa3_alloc_pkt_init(void);

static void ipa3_load_ipa_fw(struct work_struct *work);
static DECLARE_WORK(ipa3_fw_loading_work, ipa3_load_ipa_fw);

static void ipa_dec_clients_disable_clks_on_wq(struct work_struct *work);
static DECLARE_WORK(ipa_dec_clients_disable_clks_on_wq_work,
	ipa_dec_clients_disable_clks_on_wq);

static struct ipa3_plat_drv_res ipa3_res = {0, };

static struct clk *ipa3_clk;

struct ipa3_context *ipa3_ctx;

static struct {
	bool present[IPA_SMMU_CB_MAX];
	bool arm_smmu;
	bool fast_map;
	bool s1_bypass_arr[IPA_SMMU_CB_MAX];
	bool use_64_bit_dma_mask;
	u32 ipa_base;
	u32 ipa_size;
} smmu_info;

static char *active_clients_table_buf;

int ipa3_active_clients_log_print_buffer(char *buf, int size)
{
	int i;
	int nbytes;
	int cnt = 0;
	int start_idx;
	int end_idx;
	unsigned long flags;

	spin_lock_irqsave(&ipa3_ctx->ipa3_active_clients_logging.lock, flags);
	start_idx = (ipa3_ctx->ipa3_active_clients_logging.log_tail + 1) %
			IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES;
	end_idx = ipa3_ctx->ipa3_active_clients_logging.log_head;
	for (i = start_idx; i != end_idx;
		i = (i + 1) % IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES) {
		nbytes = scnprintf(buf + cnt, size - cnt, "%s\n",
				ipa3_ctx->ipa3_active_clients_logging
				.log_buffer[i]);
		cnt += nbytes;
	}
	spin_unlock_irqrestore(&ipa3_ctx->ipa3_active_clients_logging.lock,
		flags);

	return cnt;
}

int ipa3_active_clients_log_print_table(char *buf, int size)
{
	int i;
	struct ipa3_active_client_htable_entry *iterator;
	int cnt = 0;
	unsigned long flags;

	spin_lock_irqsave(&ipa3_ctx->ipa3_active_clients_logging.lock, flags);
	cnt = scnprintf(buf, size, "\n---- Active Clients Table ----\n");
	hash_for_each(ipa3_ctx->ipa3_active_clients_logging.htable, i,
			iterator, list) {
		switch (iterator->type) {
		case IPA3_ACTIVE_CLIENT_LOG_TYPE_EP:
			cnt += scnprintf(buf + cnt, size - cnt,
					"%-40s %-3d ENDPOINT\n",
					iterator->id_string, iterator->count);
			break;
		case IPA3_ACTIVE_CLIENT_LOG_TYPE_SIMPLE:
			cnt += scnprintf(buf + cnt, size - cnt,
					"%-40s %-3d SIMPLE\n",
					iterator->id_string, iterator->count);
			break;
		case IPA3_ACTIVE_CLIENT_LOG_TYPE_RESOURCE:
			cnt += scnprintf(buf + cnt, size - cnt,
					"%-40s %-3d RESOURCE\n",
					iterator->id_string, iterator->count);
			break;
		case IPA3_ACTIVE_CLIENT_LOG_TYPE_SPECIAL:
			cnt += scnprintf(buf + cnt, size - cnt,
					"%-40s %-3d SPECIAL\n",
					iterator->id_string, iterator->count);
			break;
		default:
			IPAERR("Trying to print illegal active_clients type");
			break;
		}
	}
	cnt += scnprintf(buf + cnt, size - cnt,
			"\nTotal active clients count: %d\n",
			atomic_read(&ipa3_ctx->ipa3_active_clients.cnt));

	if (ipa3_is_mhip_offload_enabled())
		cnt += ipa_mpm_panic_handler(buf + cnt, size - cnt);

	spin_unlock_irqrestore(&ipa3_ctx->ipa3_active_clients_logging.lock,
		flags);

	return cnt;
}

static int ipa3_clean_modem_rule(void)
{
	struct ipa_install_fltr_rule_req_msg_v01 *req;
	struct ipa_install_fltr_rule_req_ex_msg_v01 *req_ex;
	int val = 0;

	if (ipa3_ctx->ipa_hw_type < IPA_HW_v3_0) {
		req = kzalloc(
			sizeof(struct ipa_install_fltr_rule_req_msg_v01),
			GFP_KERNEL);
		if (!req) {
			IPAERR("mem allocated failed!\n");
			return -ENOMEM;
		}
		req->filter_spec_list_valid = false;
		req->filter_spec_list_len = 0;
		req->source_pipe_index_valid = 0;
		val = ipa3_qmi_filter_request_send(req);
		kfree(req);
	} else {
		req_ex = kzalloc(
			sizeof(struct ipa_install_fltr_rule_req_ex_msg_v01),
			GFP_KERNEL);
		if (!req_ex) {
			IPAERR("mem allocated failed!\n");
			return -ENOMEM;
		}
		req_ex->filter_spec_ex_list_valid = false;
		req_ex->filter_spec_ex_list_len = 0;
		req_ex->source_pipe_index_valid = 0;
		val = ipa3_qmi_filter_request_ex_send(req_ex);
		kfree(req_ex);
	}

	return val;
}

static int ipa3_clean_mhip_dl_rule(void)
{
	struct ipa_remove_offload_connection_req_msg_v01 req;

	memset(&req, 0, sizeof(struct
		ipa_remove_offload_connection_req_msg_v01));

	req.clean_all_rules_valid = true;
	req.clean_all_rules = true;

	if (ipa3_qmi_rmv_offload_request_send(&req)) {
		IPAWANDBG("clean dl rule cache failed\n");
		return -EFAULT;
	}

	return 0;
}

static int ipa3_active_clients_panic_notifier(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	ipa3_active_clients_log_print_table(active_clients_table_buf,
			IPA3_ACTIVE_CLIENTS_TABLE_BUF_SIZE);
	IPAERR("%s\n", active_clients_table_buf);

	return NOTIFY_DONE;
}

static struct notifier_block ipa3_active_clients_panic_blk = {
	.notifier_call  = ipa3_active_clients_panic_notifier,
};

#ifdef CONFIG_IPA_DEBUG
static int ipa3_active_clients_log_insert(const char *string)
{
	int head;
	int tail;

	if (!ipa3_ctx->ipa3_active_clients_logging.log_rdy)
		return -EPERM;

	head = ipa3_ctx->ipa3_active_clients_logging.log_head;
	tail = ipa3_ctx->ipa3_active_clients_logging.log_tail;

	memset(ipa3_ctx->ipa3_active_clients_logging.log_buffer[head], '_',
			IPA3_ACTIVE_CLIENTS_LOG_LINE_LEN);
	strlcpy(ipa3_ctx->ipa3_active_clients_logging.log_buffer[head], string,
			(size_t)IPA3_ACTIVE_CLIENTS_LOG_LINE_LEN);
	head = (head + 1) % IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES;
	if (tail == head)
		tail = (tail + 1) % IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES;

	ipa3_ctx->ipa3_active_clients_logging.log_tail = tail;
	ipa3_ctx->ipa3_active_clients_logging.log_head = head;

	return 0;
}
#endif

static int ipa3_active_clients_log_init(void)
{
	int i;

	spin_lock_init(&ipa3_ctx->ipa3_active_clients_logging.lock);
	ipa3_ctx->ipa3_active_clients_logging.log_buffer[0] = kcalloc(
			IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES,
			sizeof(char[IPA3_ACTIVE_CLIENTS_LOG_LINE_LEN]),
			GFP_KERNEL);
	active_clients_table_buf = kzalloc(sizeof(
			char[IPA3_ACTIVE_CLIENTS_TABLE_BUF_SIZE]), GFP_KERNEL);
	if (ipa3_ctx->ipa3_active_clients_logging.log_buffer == NULL) {
		pr_err("Active Clients Logging memory allocation failed");
		goto bail;
	}
	for (i = 0; i < IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES; i++) {
		ipa3_ctx->ipa3_active_clients_logging.log_buffer[i] =
			ipa3_ctx->ipa3_active_clients_logging.log_buffer[0] +
			(IPA3_ACTIVE_CLIENTS_LOG_LINE_LEN * i);
	}
	ipa3_ctx->ipa3_active_clients_logging.log_head = 0;
	ipa3_ctx->ipa3_active_clients_logging.log_tail =
			IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES - 1;
	hash_init(ipa3_ctx->ipa3_active_clients_logging.htable);
	atomic_notifier_chain_register(&panic_notifier_list,
			&ipa3_active_clients_panic_blk);
	ipa3_ctx->ipa3_active_clients_logging.log_rdy = 1;

	return 0;

bail:
	return -ENOMEM;
}

void ipa3_active_clients_log_clear(void)
{
	unsigned long flags;

	spin_lock_irqsave(&ipa3_ctx->ipa3_active_clients_logging.lock, flags);
	ipa3_ctx->ipa3_active_clients_logging.log_head = 0;
	ipa3_ctx->ipa3_active_clients_logging.log_tail =
			IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES - 1;
	spin_unlock_irqrestore(&ipa3_ctx->ipa3_active_clients_logging.lock,
		flags);
}

static void ipa3_active_clients_log_destroy(void)
{
	unsigned long flags;

	spin_lock_irqsave(&ipa3_ctx->ipa3_active_clients_logging.lock, flags);
	ipa3_ctx->ipa3_active_clients_logging.log_rdy = 0;
	kfree(active_clients_table_buf);
	active_clients_table_buf = NULL;
	kfree(ipa3_ctx->ipa3_active_clients_logging.log_buffer[0]);
	ipa3_ctx->ipa3_active_clients_logging.log_head = 0;
	ipa3_ctx->ipa3_active_clients_logging.log_tail =
			IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES - 1;
	spin_unlock_irqrestore(&ipa3_ctx->ipa3_active_clients_logging.lock,
		flags);
}

static struct ipa_smmu_cb_ctx smmu_cb[IPA_SMMU_CB_MAX];

struct iommu_domain *ipa3_get_smmu_domain(void)
{
	if (smmu_cb[IPA_SMMU_CB_AP].valid)
		return smmu_cb[IPA_SMMU_CB_AP].mapping->domain;

	IPAERR("CB not valid\n");

	return NULL;
}

struct iommu_domain *ipa3_get_uc_smmu_domain(void)
{
	if (smmu_cb[IPA_SMMU_CB_UC].valid)
		return smmu_cb[IPA_SMMU_CB_UC].mapping->domain;

	IPAERR("CB not valid\n");

	return NULL;
}

struct iommu_domain *ipa3_get_wlan_smmu_domain(void)
{
	if (smmu_cb[IPA_SMMU_CB_WLAN].valid)
		return smmu_cb[IPA_SMMU_CB_WLAN].iommu;

	IPAERR("CB not valid\n");

	return NULL;
}

struct iommu_domain *ipa3_get_smmu_domain_by_type(enum ipa_smmu_cb_type cb_type)
{

	if (cb_type == IPA_SMMU_CB_WLAN && smmu_cb[IPA_SMMU_CB_WLAN].valid)
		return smmu_cb[IPA_SMMU_CB_WLAN].iommu;

	if (smmu_cb[cb_type].valid)
		return smmu_cb[cb_type].mapping->domain;

	IPAERR("CB#%d not valid\n", cb_type);

	return NULL;
}

struct device *ipa3_get_dma_dev(void)
{
	return ipa3_ctx->pdev;
}

/**
 * ipa3_get_smmu_ctx()- Return smmu context for the given cb_type
 *
 * Return value: pointer to smmu context address
 */
struct ipa_smmu_cb_ctx *ipa3_get_smmu_ctx(enum ipa_smmu_cb_type cb_type)
{
	return &smmu_cb[cb_type];
}

static int ipa3_open(struct inode *inode, struct file *filp)
{
	IPADBG_LOW("ENTER\n");
	filp->private_data = ipa3_ctx;

	return 0;
}

static void ipa3_wan_msg_free_cb(void *buff, u32 len, u32 type)
{
	if (!buff) {
		IPAERR("Null buffer\n");
		return;
	}

	if (type != WAN_UPSTREAM_ROUTE_ADD &&
	    type != WAN_UPSTREAM_ROUTE_DEL &&
	    type != WAN_EMBMS_CONNECT) {
		IPAERR("Wrong type given. buff %pK type %d\n", buff, type);
		return;
	}

	kfree(buff);
}

static int ipa3_send_wan_msg(unsigned long usr_param,
	uint8_t msg_type, bool is_cache)
{
	int retval;
	struct ipa_wan_msg *wan_msg;
	struct ipa_msg_meta msg_meta;
	struct ipa_wan_msg cache_wan_msg;

	wan_msg = kzalloc(sizeof(*wan_msg), GFP_KERNEL);
	if (!wan_msg)
		return -ENOMEM;

	if (copy_from_user(wan_msg, (const void __user *)usr_param,
		sizeof(struct ipa_wan_msg))) {
		kfree(wan_msg);
		return -EFAULT;
	}

	memcpy(&cache_wan_msg, wan_msg, sizeof(cache_wan_msg));

	memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
	msg_meta.msg_type = msg_type;
	msg_meta.msg_len = sizeof(struct ipa_wan_msg);
	retval = ipa3_send_msg(&msg_meta, wan_msg, ipa3_wan_msg_free_cb);
	if (retval) {
		IPAERR_RL("ipa3_send_msg failed: %d\n", retval);
		kfree(wan_msg);
		return retval;
	}

	if (is_cache) {
		mutex_lock(&ipa3_ctx->ipa_cne_evt_lock);

		/* cache the cne event */
		memcpy(&ipa3_ctx->ipa_cne_evt_req_cache[
			ipa3_ctx->num_ipa_cne_evt_req].wan_msg,
			&cache_wan_msg,
			sizeof(cache_wan_msg));

		memcpy(&ipa3_ctx->ipa_cne_evt_req_cache[
			ipa3_ctx->num_ipa_cne_evt_req].msg_meta,
			&msg_meta,
			sizeof(struct ipa_msg_meta));

		ipa3_ctx->num_ipa_cne_evt_req++;
		ipa3_ctx->num_ipa_cne_evt_req %= IPA_MAX_NUM_REQ_CACHE;
		mutex_unlock(&ipa3_ctx->ipa_cne_evt_lock);
	}

	return 0;
}

static void ipa3_vlan_l2tp_msg_free_cb(void *buff, u32 len, u32 type)
{
	if (!buff) {
		IPAERR("Null buffer\n");
		return;
	}

	switch (type) {
	case ADD_VLAN_IFACE:
	case DEL_VLAN_IFACE:
	case ADD_L2TP_VLAN_MAPPING:
	case DEL_L2TP_VLAN_MAPPING:
	case ADD_BRIDGE_VLAN_MAPPING:
	case DEL_BRIDGE_VLAN_MAPPING:
		break;
	default:
		IPAERR("Wrong type given. buff %pK type %d\n", buff, type);
		return;
	}

	kfree(buff);
}

static int ipa3_send_vlan_l2tp_msg(unsigned long usr_param, uint8_t msg_type)
{
	int retval;
	struct ipa_ioc_vlan_iface_info *vlan_info;
	struct ipa_ioc_l2tp_vlan_mapping_info *mapping_info;
	struct ipa_ioc_bridge_vlan_mapping_info *bridge_vlan_info;
	struct ipa_msg_meta msg_meta;
	void *buff;

	IPADBG("type %d\n", msg_type);

	memset(&msg_meta, 0, sizeof(msg_meta));
	msg_meta.msg_type = msg_type;

	if ((msg_type == ADD_VLAN_IFACE) ||
		(msg_type == DEL_VLAN_IFACE)) {
		vlan_info = kzalloc(sizeof(struct ipa_ioc_vlan_iface_info),
			GFP_KERNEL);
		if (!vlan_info)
			return -ENOMEM;

		if (copy_from_user((u8 *)vlan_info, (void __user *)usr_param,
			sizeof(struct ipa_ioc_vlan_iface_info))) {
			kfree(vlan_info);
			return -EFAULT;
		}

		msg_meta.msg_len = sizeof(struct ipa_ioc_vlan_iface_info);
		buff = vlan_info;
	} else if ((msg_type == ADD_L2TP_VLAN_MAPPING) ||
		(msg_type == DEL_L2TP_VLAN_MAPPING)) {
		mapping_info = kzalloc(sizeof(struct
			ipa_ioc_l2tp_vlan_mapping_info), GFP_KERNEL);
		if (!mapping_info)
			return -ENOMEM;

		if (copy_from_user((u8 *)mapping_info,
			(void __user *)usr_param,
			sizeof(struct ipa_ioc_l2tp_vlan_mapping_info))) {
			kfree(mapping_info);
			return -EFAULT;
		}

		msg_meta.msg_len = sizeof(struct
			ipa_ioc_l2tp_vlan_mapping_info);
		buff = mapping_info;
	} else if ((msg_type == ADD_BRIDGE_VLAN_MAPPING) ||
		(msg_type == DEL_BRIDGE_VLAN_MAPPING)) {
		bridge_vlan_info = kzalloc(
			sizeof(struct ipa_ioc_bridge_vlan_mapping_info),
			GFP_KERNEL);
		if (!bridge_vlan_info)
			return -ENOMEM;

		if (copy_from_user((u8 *)bridge_vlan_info,
			(void __user *)usr_param,
			sizeof(struct ipa_ioc_bridge_vlan_mapping_info))) {
			kfree(bridge_vlan_info);
			IPAERR("copy from user failed\n");
			return -EFAULT;
		}

		msg_meta.msg_len = sizeof(struct
			ipa_ioc_bridge_vlan_mapping_info);
		buff = bridge_vlan_info;
	} else {
		IPAERR("Unexpected event\n");
		return -EFAULT;
	}

	retval = ipa3_send_msg(&msg_meta, buff,
		ipa3_vlan_l2tp_msg_free_cb);
	if (retval) {
		IPAERR("ipa3_send_msg failed: %d, msg_type %d\n",
			retval,
			msg_type);
		kfree(buff);
		return retval;
	}
	IPADBG("exit\n");

	return 0;
}

static void ipa3_gsb_msg_free_cb(void *buff, u32 len, u32 type)
{
	if (!buff) {
		IPAERR("Null buffer\n");
		return;
	}

	switch (type) {
	case IPA_GSB_CONNECT:
	case IPA_GSB_DISCONNECT:
		break;
	default:
		IPAERR("Wrong type given. buff %pK type %d\n", buff, type);
		return;
	}

	kfree(buff);
}

static int ipa3_send_gsb_msg(unsigned long usr_param, uint8_t msg_type)
{
	int retval;
	struct ipa_ioc_gsb_info *gsb_info;
	struct ipa_msg_meta msg_meta;
	void *buff;

	IPADBG("type %d\n", msg_type);

	memset(&msg_meta, 0, sizeof(msg_meta));
	msg_meta.msg_type = msg_type;

	if ((msg_type == IPA_GSB_CONNECT) ||
		(msg_type == IPA_GSB_DISCONNECT)) {
		gsb_info = kzalloc(sizeof(struct ipa_ioc_gsb_info),
			GFP_KERNEL);
		if (!gsb_info) {
			IPAERR("no memory\n");
			return -ENOMEM;
		}

		if (copy_from_user((u8 *)gsb_info, (void __user *)usr_param,
			sizeof(struct ipa_ioc_gsb_info))) {
			kfree(gsb_info);
			return -EFAULT;
		}

		msg_meta.msg_len = sizeof(struct ipa_ioc_gsb_info);
		buff = gsb_info;
	} else {
		IPAERR("Unexpected event\n");
		return -EFAULT;
	}

	retval = ipa3_send_msg(&msg_meta, buff,
		ipa3_gsb_msg_free_cb);
	if (retval) {
		IPAERR("ipa3_send_msg failed: %d, msg_type %d\n",
			retval,
			msg_type);
		kfree(buff);
		return retval;
	}
	IPADBG("exit\n");

	return 0;
}

static long ipa3_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	int i;
	u32 usr_pyld_sz;
	u32 pyld_sz;
	u8 header[128] = { 0 };
	u8 *param = NULL;
	u8 *kptr = NULL;
	unsigned long uptr = 0;
	bool is_vlan_mode;
	struct ipa_ioc_nat_alloc_mem nat_mem;
	struct ipa_ioc_nat_ipv6ct_table_alloc table_alloc;
	struct ipa_ioc_v4_nat_init nat_init;
	struct ipa_ioc_ipv6ct_init ipv6ct_init;
	struct ipa_ioc_v4_nat_del nat_del;
	struct ipa_ioc_nat_ipv6ct_table_del table_del;
	struct ipa_ioc_nat_pdn_entry mdfy_pdn;
	struct ipa_ioc_rm_dependency rm_depend;
	struct ipa_ioc_nat_dma_cmd *table_dma_cmd;
	struct ipa_ioc_get_vlan_mode vlan_mode;
	struct ipa_ioc_wigig_fst_switch fst_switch;
	struct ipa_nat_in_sram_info nat_in_sram_info;
	size_t sz;
	int pre_entry;
	int hdl;

	IPADBG("cmd=%x nr=%d\n", cmd, _IOC_NR(cmd));

	if (_IOC_TYPE(cmd) != IPA_IOC_MAGIC)
		return -ENOTTY;

	if (!ipa3_is_ready()) {
		IPAERR("IPA not ready, waiting for init completion\n");
		wait_for_completion(&ipa3_ctx->init_completion_obj);
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	switch (cmd) {
	case IPA_IOC_ALLOC_NAT_MEM:
		if (copy_from_user(&nat_mem, (const void __user *)arg,
			sizeof(struct ipa_ioc_nat_alloc_mem))) {
			retval = -EFAULT;
			break;
		}
		/* null terminate the string */
		nat_mem.dev_name[IPA_RESOURCE_NAME_MAX - 1] = '\0';

		if (ipa3_allocate_nat_device(&nat_mem)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, &nat_mem,
			sizeof(struct ipa_ioc_nat_alloc_mem))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_ALLOC_NAT_TABLE:
		if (copy_from_user(&table_alloc, (const void __user *)arg,
			sizeof(struct ipa_ioc_nat_ipv6ct_table_alloc))) {
			retval = -EFAULT;
			break;
		}

		if (ipa3_allocate_nat_table(&table_alloc)) {
			retval = -EFAULT;
			break;
		}
		if (table_alloc.offset &&
			copy_to_user((void __user *)arg, &table_alloc, sizeof(
				struct ipa_ioc_nat_ipv6ct_table_alloc))) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_ALLOC_IPV6CT_TABLE:
		if (copy_from_user(&table_alloc, (const void __user *)arg,
			sizeof(struct ipa_ioc_nat_ipv6ct_table_alloc))) {
			retval = -EFAULT;
			break;
		}

		if (ipa3_allocate_ipv6ct_table(&table_alloc)) {
			retval = -EFAULT;
			break;
		}
		if (table_alloc.offset &&
			copy_to_user((void __user *)arg, &table_alloc, sizeof(
				struct ipa_ioc_nat_ipv6ct_table_alloc))) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_V4_INIT_NAT:
		if (copy_from_user(&nat_init, (const void __user *)arg,
			sizeof(struct ipa_ioc_v4_nat_init))) {
			retval = -EFAULT;
			break;
		}

		if (ipa3_nat_init_cmd(&nat_init)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_INIT_IPV6CT_TABLE:
		if (copy_from_user(&ipv6ct_init, (const void __user *)arg,
			sizeof(struct ipa_ioc_ipv6ct_init))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_ipv6ct_init_cmd(&ipv6ct_init)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_TABLE_DMA_CMD:
		table_dma_cmd = (struct ipa_ioc_nat_dma_cmd *)header;
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_nat_dma_cmd))) {
			retval = -EFAULT;
			break;
		}
		pre_entry = table_dma_cmd->entries;
		pyld_sz = sizeof(struct ipa_ioc_nat_dma_cmd) +
			pre_entry * sizeof(struct ipa_ioc_nat_dma_one);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}

		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		table_dma_cmd = (struct ipa_ioc_nat_dma_cmd *)param;

		/* add check in case user-space module compromised */
		if (unlikely(table_dma_cmd->entries != pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				table_dma_cmd->entries, pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_table_dma_cmd(table_dma_cmd)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_V4_DEL_NAT:
		if (copy_from_user(&nat_del, (const void __user *)arg,
			sizeof(struct ipa_ioc_v4_nat_del))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_nat_del_cmd(&nat_del)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_DEL_NAT_TABLE:
		if (copy_from_user(&table_del, (const void __user *)arg,
			sizeof(struct ipa_ioc_nat_ipv6ct_table_del))) {
			retval = -EFAULT;
			break;
		}

		if (ipa3_del_nat_table(&table_del)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_DEL_IPV6CT_TABLE:
		if (copy_from_user(&table_del, (const void __user *)arg,
			sizeof(struct ipa_ioc_nat_ipv6ct_table_del))) {
			retval = -EFAULT;
			break;
		}

		if (ipa3_del_ipv6ct_table(&table_del)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_NAT_MODIFY_PDN:
		if (copy_from_user(&mdfy_pdn, (const void __user *)arg,
			sizeof(struct ipa_ioc_nat_pdn_entry))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_nat_mdfy_pdn(&mdfy_pdn)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_ADD_HDR:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_add_hdr))) {
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_add_hdr *)header)->num_hdrs;
		pyld_sz =
		   sizeof(struct ipa_ioc_add_hdr) +
		   pre_entry * sizeof(struct ipa_hdr_add);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_add_hdr *)param)->num_hdrs
			!= pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				((struct ipa_ioc_add_hdr *)param)->num_hdrs,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_add_hdr_usr((struct ipa_ioc_add_hdr *)param,
			true)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_DEL_HDR:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_del_hdr))) {
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_del_hdr *)header)->num_hdls;
		pyld_sz =
		   sizeof(struct ipa_ioc_del_hdr) +
		   pre_entry * sizeof(struct ipa_hdr_del);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_del_hdr *)param)->num_hdls
			!= pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				((struct ipa_ioc_del_hdr *)param)->num_hdls,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_del_hdr_by_user((struct ipa_ioc_del_hdr *)param,
			true)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_ADD_RT_RULE:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_add_rt_rule))) {
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_add_rt_rule *)header)->num_rules;
		pyld_sz =
		   sizeof(struct ipa_ioc_add_rt_rule) +
		   pre_entry * sizeof(struct ipa_rt_rule_add);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_add_rt_rule *)param)->num_rules
			!= pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				((struct ipa_ioc_add_rt_rule *)param)->
				num_rules,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_add_rt_rule_usr((struct ipa_ioc_add_rt_rule *)param,
				true)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_ADD_RT_RULE_EXT:
		if (copy_from_user(header,
				(const void __user *)arg,
				sizeof(struct ipa_ioc_add_rt_rule_ext))) {
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_add_rt_rule_ext *)header)->num_rules;
		pyld_sz =
		   sizeof(struct ipa_ioc_add_rt_rule_ext) +
		   pre_entry * sizeof(struct ipa_rt_rule_add_ext);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(
			((struct ipa_ioc_add_rt_rule_ext *)param)->num_rules
			!= pre_entry)) {
			IPAERR(" prevent memory corruption(%d not match %d)\n",
				((struct ipa_ioc_add_rt_rule_ext *)param)->
				num_rules,
				pre_entry);
			retval = -EINVAL;
			break;
		}
		if (ipa3_add_rt_rule_ext(
			(struct ipa_ioc_add_rt_rule_ext *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_ADD_RT_RULE_AFTER:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_add_rt_rule_after))) {

			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_add_rt_rule_after *)header)->num_rules;
		pyld_sz =
		   sizeof(struct ipa_ioc_add_rt_rule_after) +
		   pre_entry * sizeof(struct ipa_rt_rule_add);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_add_rt_rule_after *)param)->
			num_rules != pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				((struct ipa_ioc_add_rt_rule_after *)param)->
				num_rules,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_add_rt_rule_after(
			(struct ipa_ioc_add_rt_rule_after *)param)) {

			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_MDFY_RT_RULE:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_mdfy_rt_rule))) {
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_mdfy_rt_rule *)header)->num_rules;
		pyld_sz =
		   sizeof(struct ipa_ioc_mdfy_rt_rule) +
		   pre_entry * sizeof(struct ipa_rt_rule_mdfy);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_mdfy_rt_rule *)param)->num_rules
			!= pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				((struct ipa_ioc_mdfy_rt_rule *)param)->
				num_rules,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_mdfy_rt_rule((struct ipa_ioc_mdfy_rt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_DEL_RT_RULE:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_del_rt_rule))) {
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_del_rt_rule *)header)->num_hdls;
		pyld_sz =
		   sizeof(struct ipa_ioc_del_rt_rule) +
		   pre_entry * sizeof(struct ipa_rt_rule_del);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_del_rt_rule *)param)->num_hdls
			!= pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				((struct ipa_ioc_del_rt_rule *)param)->num_hdls,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_del_rt_rule((struct ipa_ioc_del_rt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_ADD_FLT_RULE:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_add_flt_rule))) {
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_add_flt_rule *)header)->num_rules;
		pyld_sz =
		   sizeof(struct ipa_ioc_add_flt_rule) +
		   pre_entry * sizeof(struct ipa_flt_rule_add);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_add_flt_rule *)param)->num_rules
			!= pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				((struct ipa_ioc_add_flt_rule *)param)->
				num_rules,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_add_flt_rule_usr((struct ipa_ioc_add_flt_rule *)param,
				true)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_ADD_FLT_RULE_AFTER:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_add_flt_rule_after))) {

			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_add_flt_rule_after *)header)->
			num_rules;
		pyld_sz =
		   sizeof(struct ipa_ioc_add_flt_rule_after) +
		   pre_entry * sizeof(struct ipa_flt_rule_add);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_add_flt_rule_after *)param)->
			num_rules != pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				((struct ipa_ioc_add_flt_rule_after *)param)->
				num_rules,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_add_flt_rule_after(
				(struct ipa_ioc_add_flt_rule_after *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_DEL_FLT_RULE:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_del_flt_rule))) {
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_del_flt_rule *)header)->num_hdls;
		pyld_sz =
		   sizeof(struct ipa_ioc_del_flt_rule) +
		   pre_entry * sizeof(struct ipa_flt_rule_del);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_del_flt_rule *)param)->num_hdls
			!= pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				((struct ipa_ioc_del_flt_rule *)param)->
				num_hdls,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_del_flt_rule((struct ipa_ioc_del_flt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_MDFY_FLT_RULE:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_mdfy_flt_rule))) {
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_mdfy_flt_rule *)header)->num_rules;
		pyld_sz =
		   sizeof(struct ipa_ioc_mdfy_flt_rule) +
		   pre_entry * sizeof(struct ipa_flt_rule_mdfy);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_mdfy_flt_rule *)param)->num_rules
			!= pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				((struct ipa_ioc_mdfy_flt_rule *)param)->
				num_rules,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_mdfy_flt_rule((struct ipa_ioc_mdfy_flt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_COMMIT_HDR:
		retval = ipa3_commit_hdr();
		break;
	case IPA_IOC_RESET_HDR:
		retval = ipa3_reset_hdr(false);
		break;
	case IPA_IOC_COMMIT_RT:
		retval = ipa3_commit_rt(arg);
		break;
	case IPA_IOC_RESET_RT:
		retval = ipa3_reset_rt(arg, false);
		break;
	case IPA_IOC_COMMIT_FLT:
		retval = ipa3_commit_flt(arg);
		break;
	case IPA_IOC_RESET_FLT:
		retval = ipa3_reset_flt(arg, false);
		break;
	case IPA_IOC_GET_RT_TBL:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_get_rt_tbl))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_get_rt_tbl((struct ipa_ioc_get_rt_tbl *)header)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, header,
					sizeof(struct ipa_ioc_get_rt_tbl))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_PUT_RT_TBL:
		retval = ipa3_put_rt_tbl(arg);
		break;
	case IPA_IOC_GET_HDR:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_get_hdr))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_get_hdr((struct ipa_ioc_get_hdr *)header)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, header,
			sizeof(struct ipa_ioc_get_hdr))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_PUT_HDR:
		retval = ipa3_put_hdr(arg);
		break;
	case IPA_IOC_SET_FLT:
		retval = ipa3_cfg_filter(arg);
		break;
	case IPA_IOC_COPY_HDR:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_copy_hdr))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_copy_hdr((struct ipa_ioc_copy_hdr *)header)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, header,
			sizeof(struct ipa_ioc_copy_hdr))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_QUERY_INTF:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_query_intf))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_query_intf((struct ipa_ioc_query_intf *)header)) {
			retval = -1;
			break;
		}
		if (copy_to_user((void __user *)arg, header,
			sizeof(struct ipa_ioc_query_intf))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_QUERY_INTF_TX_PROPS:
		sz = sizeof(struct ipa_ioc_query_intf_tx_props);
		if (copy_from_user(header, (const void __user *)arg, sz)) {
			retval = -EFAULT;
			break;
		}

		if (((struct ipa_ioc_query_intf_tx_props *)header)->num_tx_props
			> IPA_NUM_PROPS_MAX) {
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_query_intf_tx_props *)
			header)->num_tx_props;
		pyld_sz = sz + pre_entry *
			sizeof(struct ipa_ioc_tx_intf_prop);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_query_intf_tx_props *)
			param)->num_tx_props
			!= pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				((struct ipa_ioc_query_intf_tx_props *)
				param)->num_tx_props, pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_query_intf_tx_props(
			(struct ipa_ioc_query_intf_tx_props *)param)) {
			retval = -1;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_QUERY_INTF_RX_PROPS:
		sz = sizeof(struct ipa_ioc_query_intf_rx_props);
		if (copy_from_user(header, (const void __user *)arg, sz)) {
			retval = -EFAULT;
			break;
		}

		if (((struct ipa_ioc_query_intf_rx_props *)header)->num_rx_props
			> IPA_NUM_PROPS_MAX) {
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_query_intf_rx_props *)
			header)->num_rx_props;
		pyld_sz = sz + pre_entry *
			sizeof(struct ipa_ioc_rx_intf_prop);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_query_intf_rx_props *)
			param)->num_rx_props != pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				((struct ipa_ioc_query_intf_rx_props *)
				param)->num_rx_props, pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_query_intf_rx_props(
			(struct ipa_ioc_query_intf_rx_props *)param)) {
			retval = -1;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_QUERY_INTF_EXT_PROPS:
		sz = sizeof(struct ipa_ioc_query_intf_ext_props);
		if (copy_from_user(header, (const void __user *)arg, sz)) {
			retval = -EFAULT;
			break;
		}

		if (((struct ipa_ioc_query_intf_ext_props *)
			header)->num_ext_props > IPA_NUM_PROPS_MAX) {
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_query_intf_ext_props *)
			header)->num_ext_props;
		pyld_sz = sz + pre_entry *
			sizeof(struct ipa_ioc_ext_intf_prop);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_query_intf_ext_props *)
			param)->num_ext_props != pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				((struct ipa_ioc_query_intf_ext_props *)
				param)->num_ext_props, pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_query_intf_ext_props(
			(struct ipa_ioc_query_intf_ext_props *)param)) {
			retval = -1;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_PULL_MSG:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_msg_meta))) {
			retval = -EFAULT;
			break;
		}
		pre_entry =
		   ((struct ipa_msg_meta *)header)->msg_len;
		pyld_sz = sizeof(struct ipa_msg_meta) +
		   pre_entry;
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_msg_meta *)param)->msg_len
			!= pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				((struct ipa_msg_meta *)param)->msg_len,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_pull_msg((struct ipa_msg_meta *)param,
			(char *)param + sizeof(struct ipa_msg_meta),
			((struct ipa_msg_meta *)param)->msg_len) !=
			((struct ipa_msg_meta *)param)->msg_len) {
			retval = -1;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_RM_ADD_DEPENDENCY:
		/* deprecate if IPA PM is used */
		if (ipa3_ctx->use_ipa_pm)
			return 0;

		if (copy_from_user(&rm_depend, (const void __user *)arg,
			sizeof(struct ipa_ioc_rm_dependency))) {
			retval = -EFAULT;
			break;
		}
		retval = ipa_rm_add_dependency_from_ioctl(
			rm_depend.resource_name, rm_depend.depends_on_name);
		break;
	case IPA_IOC_RM_DEL_DEPENDENCY:
		/* deprecate if IPA PM is used */
		if (ipa3_ctx->use_ipa_pm)
			return 0;

		if (copy_from_user(&rm_depend, (const void __user *)arg,
			sizeof(struct ipa_ioc_rm_dependency))) {
			retval = -EFAULT;
			break;
		}
		retval = ipa_rm_delete_dependency_from_ioctl(
			rm_depend.resource_name, rm_depend.depends_on_name);
		break;
	case IPA_IOC_GENERATE_FLT_EQ:
		{
			struct ipa_ioc_generate_flt_eq flt_eq;

			if (copy_from_user(&flt_eq, (const void __user *)arg,
				sizeof(struct ipa_ioc_generate_flt_eq))) {
				retval = -EFAULT;
				break;
			}
			if (ipahal_flt_generate_equation(flt_eq.ip,
				&flt_eq.attrib, &flt_eq.eq_attrib)) {
				retval = -EFAULT;
				break;
			}
			if (copy_to_user((void __user *)arg, &flt_eq,
				sizeof(struct ipa_ioc_generate_flt_eq))) {
				retval = -EFAULT;
				break;
			}
			break;
		}
	case IPA_IOC_QUERY_EP_MAPPING:
		{
			retval = ipa3_get_ep_mapping(arg);
			break;
		}
	case IPA_IOC_QUERY_RT_TBL_INDEX:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_get_rt_tbl_indx))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_query_rt_index(
			(struct ipa_ioc_get_rt_tbl_indx *)header)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, header,
			sizeof(struct ipa_ioc_get_rt_tbl_indx))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_WRITE_QMAPID:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_write_qmapid))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_write_qmap_id((struct ipa_ioc_write_qmapid *)header)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, header,
			sizeof(struct ipa_ioc_write_qmapid))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_ADD:
		retval = ipa3_send_wan_msg(arg, WAN_UPSTREAM_ROUTE_ADD, true);
		if (retval) {
			IPAERR("ipa3_send_wan_msg failed: %d\n", retval);
			break;
		}
		break;
	case IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_DEL:
		retval = ipa3_send_wan_msg(arg, WAN_UPSTREAM_ROUTE_DEL, true);
		if (retval) {
			IPAERR("ipa3_send_wan_msg failed: %d\n", retval);
			break;
		}
		break;
	case IPA_IOC_NOTIFY_WAN_EMBMS_CONNECTED:
		retval = ipa3_send_wan_msg(arg, WAN_EMBMS_CONNECT, false);
		if (retval) {
			IPAERR("ipa3_send_wan_msg failed: %d\n", retval);
			break;
		}
		break;
	case IPA_IOC_ADD_HDR_PROC_CTX:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_add_hdr_proc_ctx))) {
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_add_hdr_proc_ctx *)
			header)->num_proc_ctxs;
		pyld_sz =
		   sizeof(struct ipa_ioc_add_hdr_proc_ctx) +
		   pre_entry * sizeof(struct ipa_hdr_proc_ctx_add);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_add_hdr_proc_ctx *)
			param)->num_proc_ctxs != pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				((struct ipa_ioc_add_hdr_proc_ctx *)
				param)->num_proc_ctxs, pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_add_hdr_proc_ctx(
			(struct ipa_ioc_add_hdr_proc_ctx *)param, true)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_DEL_HDR_PROC_CTX:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_del_hdr_proc_ctx))) {
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_del_hdr_proc_ctx *)header)->num_hdls;
		pyld_sz =
		   sizeof(struct ipa_ioc_del_hdr_proc_ctx) +
		   pre_entry * sizeof(struct ipa_hdr_proc_ctx_del);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_del_hdr_proc_ctx *)
			param)->num_hdls != pre_entry)) {
			IPAERR_RL("current %d pre %d\n",
				((struct ipa_ioc_del_hdr_proc_ctx *)param)->
				num_hdls,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_del_hdr_proc_ctx_by_user(
			(struct ipa_ioc_del_hdr_proc_ctx *)param, true)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_GET_HW_VERSION:
		pyld_sz = sizeof(enum ipa_hw_type);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		memcpy(param, &ipa3_ctx->ipa_hw_type, pyld_sz);
		if (copy_to_user((void __user *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_GET_VLAN_MODE:
		if (copy_from_user(&vlan_mode, (const void __user *)arg,
			sizeof(struct ipa_ioc_get_vlan_mode))) {
			retval = -EFAULT;
			break;
		}
		retval = ipa3_is_vlan_mode(
			vlan_mode.iface,
			&is_vlan_mode);
		if (retval)
			break;

		vlan_mode.is_vlan_mode = is_vlan_mode;

		if (copy_to_user((void __user *)arg,
			&vlan_mode,
			sizeof(struct ipa_ioc_get_vlan_mode))) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_ADD_VLAN_IFACE:
		if (ipa3_send_vlan_l2tp_msg(arg, ADD_VLAN_IFACE)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_DEL_VLAN_IFACE:
		if (ipa3_send_vlan_l2tp_msg(arg, DEL_VLAN_IFACE)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_ADD_BRIDGE_VLAN_MAPPING:
		if (ipa3_send_vlan_l2tp_msg(arg, ADD_BRIDGE_VLAN_MAPPING)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_DEL_BRIDGE_VLAN_MAPPING:
		if (ipa3_send_vlan_l2tp_msg(arg, DEL_BRIDGE_VLAN_MAPPING)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_ADD_L2TP_VLAN_MAPPING:
		if (ipa3_send_vlan_l2tp_msg(arg, ADD_L2TP_VLAN_MAPPING)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_DEL_L2TP_VLAN_MAPPING:
		if (ipa3_send_vlan_l2tp_msg(arg, DEL_L2TP_VLAN_MAPPING)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_CLEANUP:
		/*Route and filter rules will also be clean*/
		IPADBG("Got IPA_IOC_CLEANUP\n");
		retval = ipa3_reset_hdr(true);
		memset(&nat_del, 0, sizeof(nat_del));
		nat_del.table_index = 0;
		retval = ipa3_nat_del_cmd(&nat_del);
		if (ipa3_ctx->platform_type == IPA_PLAT_TYPE_APQ)
			retval = ipa3_clean_mhip_dl_rule();
		else
			retval = ipa3_clean_modem_rule();
		ipa3_counter_id_remove_all();
		break;

	case IPA_IOC_QUERY_WLAN_CLIENT:
		IPADBG("Got IPA_IOC_QUERY_WLAN_CLIENT\n");
		retval = ipa3_resend_wlan_msg();
		break;

	case IPA_IOC_GSB_CONNECT:
		IPADBG("Got IPA_IOC_GSB_CONNECT\n");
		if (ipa3_send_gsb_msg(arg, IPA_GSB_CONNECT)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_GSB_DISCONNECT:
		IPADBG("Got IPA_IOC_GSB_DISCONNECT\n");
		if (ipa3_send_gsb_msg(arg, IPA_GSB_DISCONNECT)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_ADD_RT_RULE_V2:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_add_rt_rule_v2))) {
			IPAERR_RL("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_add_rt_rule_v2 *)header)->num_rules;
		if (unlikely(((struct ipa_ioc_add_rt_rule_v2 *)
			header)->rule_add_size >
			sizeof(struct ipa_rt_rule_add_i))) {
			IPAERR_RL("unexpected rule_add_size %d\n",
			((struct ipa_ioc_add_rt_rule_v2 *)
			header)->rule_add_size);
			retval = -EFAULT;
			break;
		};
		/* user payload size */
		usr_pyld_sz = ((struct ipa_ioc_add_rt_rule_v2 *)
			header)->rule_add_size * pre_entry;
		/* actual payload structure size in kernel */
		pyld_sz = sizeof(struct ipa_rt_rule_add_i) * pre_entry;
		uptr = ((struct ipa_ioc_add_rt_rule_v2 *)
			header)->rules;
		if (unlikely(!uptr)) {
			IPAERR_RL("unexpected NULL rules\n");
			retval = -EFAULT;
			break;
		}
		/* alloc param with same payload size as user payload */
		param = kzalloc(usr_pyld_sz, GFP_KERNEL);
		if (!param) {
			IPAERR_RL("kzalloc fails\n");
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)uptr,
			usr_pyld_sz)) {
			IPAERR_RL("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		/* alloc kernel pointer with actual payload size */
		kptr = kzalloc(pyld_sz, GFP_KERNEL);
		if (!kptr) {
			IPAERR_RL("kzalloc fails\n");
			retval = -ENOMEM;
			break;
		}
		for (i = 0; i < pre_entry; i++)
			memcpy(kptr + i * sizeof(struct ipa_rt_rule_add_i),
				(void *)param + i *
				((struct ipa_ioc_add_rt_rule_v2 *)
				header)->rule_add_size,
				((struct ipa_ioc_add_rt_rule_v2 *)
				header)->rule_add_size);
		/* modify the rule pointer to the kernel pointer */
		((struct ipa_ioc_add_rt_rule_v2 *)header)->rules =
			(uintptr_t)kptr;
		if (ipa3_add_rt_rule_usr_v2(
			(struct ipa_ioc_add_rt_rule_v2 *)header, true)) {
			IPAERR_RL("ipa3_add_rt_rule_usr_v2 fails\n");
			retval = -EFAULT;
			break;
		}
		for (i = 0; i < pre_entry; i++)
			memcpy((void *)param + i *
				((struct ipa_ioc_add_rt_rule_v2 *)
				header)->rule_add_size,
				kptr + i * sizeof(struct ipa_rt_rule_add_i),
				((struct ipa_ioc_add_rt_rule_v2 *)
				header)->rule_add_size);
		if (copy_to_user((void __user *)uptr, param,
			usr_pyld_sz)) {
			IPAERR_RL("copy_to_user fails\n");
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_ADD_RT_RULE_EXT_V2:
		if (copy_from_user(header,
				(const void __user *)arg,
				sizeof(struct ipa_ioc_add_rt_rule_ext_v2))) {
			IPAERR_RL("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_add_rt_rule_ext_v2 *)
			header)->num_rules;
		if (unlikely(((struct ipa_ioc_add_rt_rule_ext_v2 *)
			header)->rule_add_ext_size >
			sizeof(struct ipa_rt_rule_add_ext_i))) {
			IPAERR_RL("unexpected rule_add_size %d\n",
			((struct ipa_ioc_add_rt_rule_ext_v2 *)
			header)->rule_add_ext_size);
			retval = -EFAULT;
			break;
		};
		/* user payload size */
		usr_pyld_sz = ((struct ipa_ioc_add_rt_rule_ext_v2 *)
			header)->rule_add_ext_size * pre_entry;
		/* actual payload structure size in kernel */
		pyld_sz = sizeof(struct ipa_rt_rule_add_ext_i)
			* pre_entry;
		uptr = ((struct ipa_ioc_add_rt_rule_ext_v2 *)
			header)->rules;
		if (unlikely(!uptr)) {
			IPAERR_RL("unexpected NULL rules\n");
			retval = -EFAULT;
			break;
		}
		/* alloc param with same payload size as user payload */
		param = kzalloc(usr_pyld_sz, GFP_KERNEL);
		if (!param) {
			IPAERR_RL("kzalloc fails\n");
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)uptr,
			usr_pyld_sz)) {
			IPAERR_RL("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		/* alloc kernel pointer with actual payload size */
		kptr = kzalloc(pyld_sz, GFP_KERNEL);
		if (!kptr) {
			IPAERR_RL("kzalloc fails\n");
			retval = -ENOMEM;
			break;
		}
		for (i = 0; i < pre_entry; i++)
			memcpy(kptr + i *
				sizeof(struct ipa_rt_rule_add_ext_i),
				(void *)param + i *
				((struct ipa_ioc_add_rt_rule_ext_v2 *)
				header)->rule_add_ext_size,
				((struct ipa_ioc_add_rt_rule_ext_v2 *)
				header)->rule_add_ext_size);
		/* modify the rule pointer to the kernel pointer */
		((struct ipa_ioc_add_rt_rule_ext_v2 *)header)->rules =
			(uintptr_t)kptr;
		if (ipa3_add_rt_rule_ext_v2(
			(struct ipa_ioc_add_rt_rule_ext_v2 *)header)) {
			IPAERR_RL("ipa3_add_rt_rule_ext_v2 fails\n");
			retval = -EFAULT;
			break;
		}
		for (i = 0; i < pre_entry; i++)
			memcpy((void *)param + i *
				((struct ipa_ioc_add_rt_rule_ext_v2 *)
				header)->rule_add_ext_size,
				kptr + i *
				sizeof(struct ipa_rt_rule_add_ext_i),
				((struct ipa_ioc_add_rt_rule_ext_v2 *)
				header)->rule_add_ext_size);
		if (copy_to_user((void __user *)uptr, param,
			usr_pyld_sz)) {
			IPAERR_RL("copy_to_user fails\n");
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_ADD_RT_RULE_AFTER_V2:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_add_rt_rule_after_v2))) {
			IPAERR_RL("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_add_rt_rule_after_v2 *)
			header)->num_rules;
		if (unlikely(((struct ipa_ioc_add_rt_rule_after_v2 *)
			header)->rule_add_size >
			sizeof(struct ipa_rt_rule_add_i))) {
			IPAERR_RL("unexpected rule_add_size %d\n",
			((struct ipa_ioc_add_rt_rule_after_v2 *)
			header)->rule_add_size);
			retval = -EFAULT;
			break;
		};
		/* user payload size */
		usr_pyld_sz = ((struct ipa_ioc_add_rt_rule_after_v2 *)
			header)->rule_add_size * pre_entry;
		/* actual payload structure size in kernel */
		pyld_sz = sizeof(struct ipa_rt_rule_add_i)
			* pre_entry;
		uptr = ((struct ipa_ioc_add_rt_rule_after_v2 *)
			header)->rules;
		if (unlikely(!uptr)) {
			IPAERR_RL("unexpected NULL rules\n");
			retval = -EFAULT;
			break;
		}
		/* alloc param with same payload size as user payload */
		param = kzalloc(usr_pyld_sz, GFP_KERNEL);
		if (!param) {
			IPAERR_RL("kzalloc fails\n");
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)uptr,
			usr_pyld_sz)) {
			IPAERR_RL("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		/* alloc kernel pointer with actual payload size */
		kptr = kzalloc(pyld_sz, GFP_KERNEL);
		if (!kptr) {
			IPAERR_RL("kzalloc fails\n");
			retval = -ENOMEM;
			break;
		}
		for (i = 0; i < pre_entry; i++)
			memcpy(kptr + i * sizeof(struct ipa_rt_rule_add_i),
				(void *)param + i *
				((struct ipa_ioc_add_rt_rule_after_v2 *)
				header)->rule_add_size,
				((struct ipa_ioc_add_rt_rule_after_v2 *)
				header)->rule_add_size);
		/* modify the rule pointer to the kernel pointer */
		((struct ipa_ioc_add_rt_rule_after_v2 *)header)->rules =
			(uintptr_t)kptr;
		if (ipa3_add_rt_rule_after_v2(
			(struct ipa_ioc_add_rt_rule_after_v2 *)header)) {
			IPAERR_RL("ipa3_add_rt_rule_after_v2 fails\n");
			retval = -EFAULT;
			break;
		}
		for (i = 0; i < pre_entry; i++)
			memcpy((void *)param + i *
				((struct ipa_ioc_add_rt_rule_after_v2 *)
				header)->rule_add_size,
				kptr + i * sizeof(struct ipa_rt_rule_add_i),
				((struct ipa_ioc_add_rt_rule_after_v2 *)
				header)->rule_add_size);
		if (copy_to_user((void __user *)uptr, param,
			usr_pyld_sz)) {
			IPAERR_RL("copy_to_user fails\n");
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_MDFY_RT_RULE_V2:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_mdfy_rt_rule_v2))) {
			IPAERR_RL("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_mdfy_rt_rule_v2 *)
			header)->num_rules;
		if (unlikely(((struct ipa_ioc_mdfy_rt_rule_v2 *)
			header)->rule_mdfy_size >
			sizeof(struct ipa_rt_rule_mdfy_i))) {
			IPAERR_RL("unexpected rule_add_size %d\n",
			((struct ipa_ioc_mdfy_rt_rule_v2 *)
			header)->rule_mdfy_size);
			retval = -EFAULT;
			break;
		};
		/* user payload size */
		usr_pyld_sz = ((struct ipa_ioc_mdfy_rt_rule_v2 *)
			header)->rule_mdfy_size * pre_entry;
		/* actual payload structure size in kernel */
		pyld_sz = sizeof(struct ipa_rt_rule_mdfy_i)
			* pre_entry;
		uptr = ((struct ipa_ioc_mdfy_rt_rule_v2 *)
			header)->rules;
		if (unlikely(!uptr)) {
			IPAERR_RL("unexpected NULL rules\n");
			retval = -EFAULT;
			break;
		}
		/* alloc param with same payload size as user payload */
		param = kzalloc(usr_pyld_sz, GFP_KERNEL);
		if (!param) {
			IPAERR_RL("kzalloc fails\n");
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)uptr,
			usr_pyld_sz)) {
			IPAERR_RL("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		/* alloc kernel pointer with actual payload size */
		kptr = kzalloc(pyld_sz, GFP_KERNEL);
		if (!kptr) {
			IPAERR_RL("kzalloc fails\n");
			retval = -ENOMEM;
			break;
		}
		for (i = 0; i < pre_entry; i++)
			memcpy(kptr + i * sizeof(struct ipa_rt_rule_mdfy_i),
				(void *)param + i *
				((struct ipa_ioc_mdfy_rt_rule_v2 *)
				header)->rule_mdfy_size,
				((struct ipa_ioc_mdfy_rt_rule_v2 *)
				header)->rule_mdfy_size);
		/* modify the rule pointer to the kernel pointer */
		((struct ipa_ioc_mdfy_rt_rule_v2 *)header)->rules =
			(uintptr_t)kptr;
		if (ipa3_mdfy_rt_rule_v2((struct ipa_ioc_mdfy_rt_rule_v2 *)
			header)) {
			IPAERR_RL("ipa3_mdfy_rt_rule_v2 fails\n");
			retval = -EFAULT;
			break;
		}
		for (i = 0; i < pre_entry; i++)
			memcpy((void *)param + i *
				((struct ipa_ioc_mdfy_rt_rule_v2 *)
				header)->rule_mdfy_size,
				kptr + i * sizeof(struct ipa_rt_rule_mdfy_i),
				((struct ipa_ioc_mdfy_rt_rule_v2 *)
				header)->rule_mdfy_size);
		if (copy_to_user((void __user *)uptr, param,
			usr_pyld_sz)) {
			IPAERR_RL("copy_to_user fails\n");
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_ADD_FLT_RULE_V2:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_add_flt_rule_v2))) {
			IPAERR_RL("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_add_flt_rule_v2 *)header)->num_rules;
		if (unlikely(((struct ipa_ioc_add_flt_rule_v2 *)
			header)->flt_rule_size >
			sizeof(struct ipa_flt_rule_add_i))) {
			IPAERR_RL("unexpected rule_add_size %d\n",
			((struct ipa_ioc_add_flt_rule_v2 *)
			header)->flt_rule_size);
			retval = -EFAULT;
			break;
		};
		/* user payload size */
		usr_pyld_sz = ((struct ipa_ioc_add_flt_rule_v2 *)
			header)->flt_rule_size * pre_entry;
		/* actual payload structure size in kernel */
		pyld_sz = sizeof(struct ipa_flt_rule_add_i)
			* pre_entry;
		uptr = ((struct ipa_ioc_add_flt_rule_v2 *)
			header)->rules;
		if (unlikely(!uptr)) {
			IPAERR_RL("unexpected NULL rules\n");
			retval = -EFAULT;
			break;
		}
		/* alloc param with same payload size as user payload */
		param = kzalloc(usr_pyld_sz, GFP_KERNEL);
		if (!param) {
			IPAERR_RL("kzalloc fails\n");
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)uptr,
			usr_pyld_sz)) {
			IPAERR_RL("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		/* alloc kernel pointer with actual payload size */
		kptr = kzalloc(pyld_sz, GFP_KERNEL);
		if (!kptr) {
			IPAERR_RL("kzalloc fails\n");
			retval = -ENOMEM;
			break;
		}
		for (i = 0; i < pre_entry; i++)
			memcpy(kptr + i * sizeof(struct ipa_flt_rule_add_i),
				(void *)param + i *
				((struct ipa_ioc_add_flt_rule_v2 *)
				header)->flt_rule_size,
				((struct ipa_ioc_add_flt_rule_v2 *)
				header)->flt_rule_size);
		/* modify the rule pointer to the kernel pointer */
		((struct ipa_ioc_add_flt_rule_v2 *)header)->rules =
			(uintptr_t)kptr;
		if (ipa3_add_flt_rule_usr_v2((struct ipa_ioc_add_flt_rule_v2 *)
				header, true)) {
			IPAERR_RL("ipa3_add_flt_rule_usr_v2 fails\n");
			retval = -EFAULT;
			break;
		}
		for (i = 0; i < pre_entry; i++)
			memcpy((void *)param + i *
				((struct ipa_ioc_add_flt_rule_v2 *)
				header)->flt_rule_size,
				kptr + i * sizeof(struct ipa_flt_rule_add_i),
				((struct ipa_ioc_add_flt_rule_v2 *)
				header)->flt_rule_size);
		if (copy_to_user((void __user *)uptr, param,
			usr_pyld_sz)) {
			IPAERR_RL("copy_to_user fails\n");
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_ADD_FLT_RULE_AFTER_V2:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_add_flt_rule_after_v2))) {
			IPAERR_RL("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_add_flt_rule_after_v2 *)
			 header)->num_rules;
		if (unlikely(((struct ipa_ioc_add_flt_rule_after_v2 *)
			header)->flt_rule_size >
			sizeof(struct ipa_flt_rule_add_i))) {
			IPAERR_RL("unexpected rule_add_size %d\n",
			((struct ipa_ioc_add_flt_rule_after_v2 *)
			header)->flt_rule_size);
			retval = -EFAULT;
			break;
		};
		/* user payload size */
		usr_pyld_sz = ((struct ipa_ioc_add_flt_rule_after_v2 *)
			header)->flt_rule_size * pre_entry;
		/* actual payload structure size in kernel */
		pyld_sz = sizeof(struct ipa_flt_rule_add_i)
			* pre_entry;
		uptr = ((struct ipa_ioc_add_flt_rule_after_v2 *)
			header)->rules;
		if (unlikely(!uptr)) {
			IPAERR_RL("unexpected NULL rules\n");
			retval = -EFAULT;
			break;
		}
		/* alloc param with same payload size as user payload */
		param = kzalloc(usr_pyld_sz, GFP_KERNEL);
		if (!param) {
			IPAERR_RL("kzalloc fails\n");
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)uptr,
			usr_pyld_sz)) {
			IPAERR_RL("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		/* alloc kernel pointer with actual payload size */
		kptr = kzalloc(pyld_sz, GFP_KERNEL);
		if (!kptr) {
			IPAERR_RL("kzalloc fails\n");
			retval = -ENOMEM;
			break;
		}
		for (i = 0; i < pre_entry; i++)
			memcpy(kptr + i * sizeof(struct ipa_flt_rule_add_i),
				(void *)param + i *
				((struct ipa_ioc_add_flt_rule_after_v2 *)
				header)->flt_rule_size,
				((struct ipa_ioc_add_flt_rule_after_v2 *)
				header)->flt_rule_size);
		/* modify the rule pointer to the kernel pointer */
		((struct ipa_ioc_add_flt_rule_after_v2 *)header)->rules =
			(uintptr_t)kptr;
		if (ipa3_add_flt_rule_after_v2(
			(struct ipa_ioc_add_flt_rule_after_v2 *)header)) {
			IPAERR_RL("ipa3_add_flt_rule_after_v2 fails\n");
			retval = -EFAULT;
			break;
		}
		for (i = 0; i < pre_entry; i++)
			memcpy((void *)param + i *
				((struct ipa_ioc_add_flt_rule_after_v2 *)
				header)->flt_rule_size,
				kptr + i * sizeof(struct ipa_flt_rule_add_i),
				((struct ipa_ioc_add_flt_rule_after_v2 *)
				header)->flt_rule_size);
		if (copy_to_user((void __user *)uptr, param,
			usr_pyld_sz)) {
			IPAERR_RL("copy_to_user fails\n");
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_MDFY_FLT_RULE_V2:
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_mdfy_flt_rule_v2))) {
			IPAERR_RL("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_mdfy_flt_rule_v2 *)
			 header)->num_rules;
		if (unlikely(((struct ipa_ioc_mdfy_flt_rule_v2 *)
			header)->rule_mdfy_size >
			sizeof(struct ipa_flt_rule_mdfy_i))) {
			IPAERR_RL("unexpected rule_add_size %d\n",
			((struct ipa_ioc_mdfy_flt_rule_v2 *)
			header)->rule_mdfy_size);
			retval = -EFAULT;
			break;
		};
		/* user payload size */
		usr_pyld_sz = ((struct ipa_ioc_mdfy_flt_rule_v2 *)
			header)->rule_mdfy_size * pre_entry;
		/* actual payload structure size in kernel */
		pyld_sz = sizeof(struct ipa_flt_rule_mdfy_i)
			* pre_entry;
		uptr = ((struct ipa_ioc_mdfy_flt_rule_v2 *)
			header)->rules;
		if (unlikely(!uptr)) {
			IPAERR_RL("unexpected NULL rules\n");
			retval = -EFAULT;
			break;
		}
		/* alloc param with same payload size as user payload */
		param = kzalloc(usr_pyld_sz, GFP_KERNEL);
		if (!param) {
			IPAERR_RL("kzalloc fails\n");
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)uptr,
			usr_pyld_sz)) {
			IPAERR_RL("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		/* alloc kernel pointer with actual payload size */
		kptr = kzalloc(pyld_sz, GFP_KERNEL);
		if (!kptr) {
			IPAERR_RL("kzalloc fails\n");
			retval = -ENOMEM;
			break;
		}
		for (i = 0; i < pre_entry; i++)
			memcpy(kptr + i * sizeof(struct ipa_flt_rule_mdfy_i),
				(void *)param + i *
				((struct ipa_ioc_mdfy_flt_rule_v2 *)
				header)->rule_mdfy_size,
				((struct ipa_ioc_mdfy_flt_rule_v2 *)
				header)->rule_mdfy_size);
		/* modify the rule pointer to the kernel pointer */
		((struct ipa_ioc_mdfy_flt_rule_v2 *)header)->rules =
			(uintptr_t)kptr;
		if (ipa3_mdfy_flt_rule_v2
			((struct ipa_ioc_mdfy_flt_rule_v2 *)header)) {
			IPAERR_RL("ipa3_mdfy_flt_rule_v2 fails\n");
			retval = -EFAULT;
			break;
		}
		for (i = 0; i < pre_entry; i++)
			memcpy((void *)param + i *
				((struct ipa_ioc_mdfy_flt_rule_v2 *)
				header)->rule_mdfy_size,
				kptr + i * sizeof(struct ipa_flt_rule_mdfy_i),
				((struct ipa_ioc_mdfy_flt_rule_v2 *)
				header)->rule_mdfy_size);
		if (copy_to_user((void __user *)uptr, param,
			usr_pyld_sz)) {
			IPAERR_RL("copy_to_user fails\n");
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_FNR_COUNTER_ALLOC:
		if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_5) {
			IPAERR("FNR stats not supported on IPA ver %d",
				ipa3_ctx->ipa_hw_type);
			retval = -EFAULT;
			break;
		}
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_flt_rt_counter_alloc))) {
			IPAERR("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		if (((struct ipa_ioc_flt_rt_counter_alloc *)
			header)->hw_counter.num_counters >
			IPA_FLT_RT_HW_COUNTER ||
			((struct ipa_ioc_flt_rt_counter_alloc *)
			header)->sw_counter.num_counters >
			IPA_FLT_RT_SW_COUNTER) {
			IPAERR("failed: wrong sw/hw num_counters\n");
			retval = -EFAULT;
			break;
		}
		if (((struct ipa_ioc_flt_rt_counter_alloc *)
			header)->hw_counter.num_counters == 0 &&
			((struct ipa_ioc_flt_rt_counter_alloc *)
			header)->sw_counter.num_counters == 0) {
			IPAERR("failed: both sw/hw num_counters 0\n");
			retval = -EFAULT;
			break;
		}
		retval = ipa3_alloc_counter_id
			((struct ipa_ioc_flt_rt_counter_alloc *)header);
		if (retval < 0) {
			IPAERR("ipa3_alloc_counter_id failed\n");
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg, header,
			sizeof(struct ipa_ioc_flt_rt_counter_alloc))) {
			IPAERR("copy_to_user fails\n");
			retval = -EFAULT;
			ipa3_counter_remove_hdl(
			((struct ipa_ioc_flt_rt_counter_alloc *)
			header)->hdl);
			break;
		}
		break;

	case IPA_IOC_FNR_COUNTER_DEALLOC:
		if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_5) {
			IPAERR("FNR stats not supported on IPA ver %d",
				ipa3_ctx->ipa_hw_type);
			retval = -EFAULT;
			break;
		}
		hdl = (int)arg;
		if (hdl < 0) {
			IPAERR("IPA_FNR_COUNTER_DEALLOC failed: hdl %d\n",
				hdl);
			retval = -EFAULT;
			break;
		}
		ipa3_counter_remove_hdl(hdl);
		break;

	case IPA_IOC_FNR_COUNTER_QUERY:
		if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_5) {
			IPAERR("FNR stats not supported on IPA ver %d",
				ipa3_ctx->ipa_hw_type);
			retval = -EFAULT;
			break;
		}
		if (copy_from_user(header, (const void __user *)arg,
			sizeof(struct ipa_ioc_flt_rt_query))) {
			IPAERR_RL("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_flt_rt_query *)
			header)->end_id - ((struct ipa_ioc_flt_rt_query *)
			header)->start_id + 1;
		if (pre_entry <= 0 || pre_entry > IPA_MAX_FLT_RT_CNT_INDEX) {
			IPAERR("IPA_IOC_FNR_COUNTER_QUERY failed: num %d\n",
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (((struct ipa_ioc_flt_rt_query *)header)->stats_size
			> sizeof(struct ipa_flt_rt_stats)) {
			IPAERR_RL("unexpected stats_size %d\n",
			((struct ipa_ioc_flt_rt_query *)header)->stats_size);
			retval = -EFAULT;
			break;
		};
		/* user payload size */
		usr_pyld_sz = ((struct ipa_ioc_flt_rt_query *)
			header)->stats_size * pre_entry;
		/* actual payload structure size in kernel */
		pyld_sz = sizeof(struct ipa_flt_rt_stats) * pre_entry;
		uptr = ((struct ipa_ioc_flt_rt_query *)
			header)->stats;
		if (unlikely(!uptr)) {
			IPAERR_RL("unexpected NULL rules\n");
			retval = -EFAULT;
			break;
		}
		/* alloc param with same payload size as user payload */
		param = kzalloc(usr_pyld_sz, GFP_KERNEL);
		if (!param) {
			IPAERR_RL("kzalloc fails\n");
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (const void __user *)uptr,
			usr_pyld_sz)) {
			IPAERR_RL("copy_from_user fails\n");
			retval = -EFAULT;
			break;
		}
		/* alloc kernel pointer with actual payload size */
		kptr = kzalloc(pyld_sz, GFP_KERNEL);
		if (!kptr) {
			IPAERR_RL("kzalloc fails\n");
			retval = -ENOMEM;
			break;
		}
		for (i = 0; i < pre_entry; i++)
			memcpy(kptr + i * sizeof(struct ipa_flt_rt_stats),
				(void *)param + i *
				((struct ipa_ioc_flt_rt_query *)
				header)->stats_size,
				((struct ipa_ioc_flt_rt_query *)
				header)->stats_size);
		/* modify the rule pointer to the kernel pointer */
		((struct ipa_ioc_flt_rt_query *)
			header)->stats = (uintptr_t)kptr;
		retval = ipa_get_flt_rt_stats
			((struct ipa_ioc_flt_rt_query *)header);
		if (retval < 0) {
			IPAERR("ipa_get_flt_rt_stats failed\n");
			retval = -EFAULT;
			break;
		}
		for (i = 0; i < pre_entry; i++)
			memcpy((void *)param + i *
				((struct ipa_ioc_flt_rt_query *)
				header)->stats_size,
				kptr + i * sizeof(struct ipa_flt_rt_stats),
				((struct ipa_ioc_flt_rt_query *)
				header)->stats_size);
		if (copy_to_user((void __user *)uptr, param,
			usr_pyld_sz)) {
			IPAERR_RL("copy_to_user fails\n");
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_WIGIG_FST_SWITCH:
		IPADBG("Got IPA_IOCTL_WIGIG_FST_SWITCH\n");
		if (copy_from_user(&fst_switch, (const void __user *)arg,
			sizeof(struct ipa_ioc_wigig_fst_switch))) {
			retval = -EFAULT;
			break;
		}

		/* null terminate the string */
		fst_switch.netdev_name[IPA_RESOURCE_NAME_MAX - 1] = '\0';

		retval = ipa_wigig_send_msg(WIGIG_FST_SWITCH,
			fst_switch.netdev_name,
			fst_switch.client_mac_addr,
			IPA_CLIENT_MAX,
			fst_switch.to_wigig);
		break;

	case IPA_IOC_GET_NAT_IN_SRAM_INFO:
		if (ipa3_nat_get_sram_info(&nat_in_sram_info)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((void __user *)arg,
			&nat_in_sram_info,
			sizeof(struct ipa_nat_in_sram_info))) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_APP_CLOCK_VOTE:
		retval = ipa3_app_clk_vote(
			(enum ipa_app_clock_vote_type) arg);
		break;

	default:
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return -ENOTTY;
	}
	kfree(kptr);
	kfree(param);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return retval;
}

/**
 * ipa3_setup_dflt_rt_tables() - Setup default routing tables
 *
 * Return codes:
 * 0: success
 * -ENOMEM: failed to allocate memory
 * -EPERM: failed to add the tables
 */
int ipa3_setup_dflt_rt_tables(void)
{
	struct ipa_ioc_add_rt_rule *rt_rule;
	struct ipa_rt_rule_add *rt_rule_entry;

	rt_rule =
		kzalloc(sizeof(struct ipa_ioc_add_rt_rule) + 1 *
			sizeof(struct ipa_rt_rule_add), GFP_KERNEL);
	if (!rt_rule)
		return -ENOMEM;

	/* setup a default v4 route to point to Apps */
	rt_rule->num_rules = 1;
	rt_rule->commit = 1;
	rt_rule->ip = IPA_IP_v4;
	strlcpy(rt_rule->rt_tbl_name, IPA_DFLT_RT_TBL_NAME,
		IPA_RESOURCE_NAME_MAX);

	rt_rule_entry = &rt_rule->rules[0];
	rt_rule_entry->at_rear = 1;
	rt_rule_entry->rule.dst = IPA_CLIENT_APPS_LAN_CONS;
	rt_rule_entry->rule.hdr_hdl = ipa3_ctx->excp_hdr_hdl;
	rt_rule_entry->rule.retain_hdr = 1;

	if (ipa3_add_rt_rule(rt_rule)) {
		IPAERR("fail to add dflt v4 rule\n");
		kfree(rt_rule);
		return -EPERM;
	}
	IPADBG("dflt v4 rt rule hdl=%x\n", rt_rule_entry->rt_rule_hdl);
	ipa3_ctx->dflt_v4_rt_rule_hdl = rt_rule_entry->rt_rule_hdl;

	/* setup a default v6 route to point to A5 */
	rt_rule->ip = IPA_IP_v6;
	if (ipa3_add_rt_rule(rt_rule)) {
		IPAERR("fail to add dflt v6 rule\n");
		kfree(rt_rule);
		return -EPERM;
	}
	IPADBG("dflt v6 rt rule hdl=%x\n", rt_rule_entry->rt_rule_hdl);
	ipa3_ctx->dflt_v6_rt_rule_hdl = rt_rule_entry->rt_rule_hdl;

	/*
	 * because these tables are the very first to be added, they will both
	 * have the same index (0) which is essential for programming the
	 * "route" end-point config
	 */

	kfree(rt_rule);

	return 0;
}

static int ipa3_setup_exception_path(void)
{
	struct ipa_ioc_add_hdr *hdr;
	struct ipa_hdr_add *hdr_entry;
	struct ipahal_reg_route route = { 0 };
	int ret;

	/* install the basic exception header */
	hdr = kzalloc(sizeof(struct ipa_ioc_add_hdr) + 1 *
		      sizeof(struct ipa_hdr_add), GFP_KERNEL);
	if (!hdr)
		return -ENOMEM;

	hdr->num_hdrs = 1;
	hdr->commit = 1;
	hdr_entry = &hdr->hdr[0];

	strlcpy(hdr_entry->name, IPA_LAN_RX_HDR_NAME, IPA_RESOURCE_NAME_MAX);
	hdr_entry->hdr_len = IPA_LAN_RX_HEADER_LENGTH;

	if (ipa3_add_hdr(hdr)) {
		IPAERR("fail to add exception hdr\n");
		ret = -EPERM;
		goto bail;
	}

	if (hdr_entry->status) {
		IPAERR("fail to add exception hdr\n");
		ret = -EPERM;
		goto bail;
	}

	ipa3_ctx->excp_hdr_hdl = hdr_entry->hdr_hdl;

	/* set the route register to pass exception packets to Apps */
	route.route_def_pipe = ipa3_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS);
	route.route_frag_def_pipe = ipa3_get_ep_mapping(
		IPA_CLIENT_APPS_LAN_CONS);
	route.route_def_hdr_table = !ipa3_ctx->hdr_tbl_lcl;
	route.route_def_retain_hdr = 1;

	if (ipa3_cfg_route(&route)) {
		IPAERR("fail to add exception hdr\n");
		ret = -EPERM;
		goto bail;
	}

	ret = 0;
bail:
	kfree(hdr);
	return ret;
}

static int ipa3_init_smem_region(int memory_region_size,
				int memory_region_offset)
{
	struct ipahal_imm_cmd_dma_shared_mem cmd;
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	struct ipa3_desc desc;
	struct ipa_mem_buffer mem;
	int rc;

	if (memory_region_size == 0)
		return 0;

	memset(&desc, 0, sizeof(desc));
	memset(&cmd, 0, sizeof(cmd));
	memset(&mem, 0, sizeof(mem));

	mem.size = memory_region_size;
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev, mem.size,
		&mem.phys_base, GFP_KERNEL);
	if (!mem.base) {
		IPAERR("failed to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	memset(mem.base, 0, mem.size);
	cmd.is_read = false;
	cmd.skip_pipeline_clear = false;
	cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
	cmd.size = mem.size;
	cmd.system_addr = mem.phys_base;
	cmd.local_addr = ipa3_ctx->smem_restricted_bytes +
		memory_region_offset;
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_DMA_SHARED_MEM, &cmd, false);
	if (!cmd_pyld) {
		IPAERR("failed to construct dma_shared_mem imm cmd\n");
		return -ENOMEM;
	}
	ipa3_init_imm_cmd_desc(&desc, cmd_pyld);

	rc = ipa3_send_cmd(1, &desc);
	if (rc) {
		IPAERR("failed to send immediate command (error %d)\n", rc);
		rc = -EFAULT;
	}

	ipahal_destroy_imm_cmd(cmd_pyld);
	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base,
		mem.phys_base);

	return rc;
}

/**
 * ipa3_init_q6_smem() - Initialize Q6 general memory and
 *                      header memory regions in IPA.
 *
 * Return codes:
 * 0: success
 * -ENOMEM: failed to allocate dma memory
 * -EFAULT: failed to send IPA command to initialize the memory
 */
int ipa3_init_q6_smem(void)
{
	int rc;

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	rc = ipa3_init_smem_region(IPA_MEM_PART(modem_size),
		IPA_MEM_PART(modem_ofst));
	if (rc) {
		IPAERR("failed to initialize Modem RAM memory\n");
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return rc;
	}

	rc = ipa3_init_smem_region(IPA_MEM_PART(modem_hdr_size),
		IPA_MEM_PART(modem_hdr_ofst));
	if (rc) {
		IPAERR("failed to initialize Modem HDRs RAM memory\n");
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return rc;
	}

	rc = ipa3_init_smem_region(IPA_MEM_PART(modem_hdr_proc_ctx_size),
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst));
	if (rc) {
		IPAERR("failed to initialize Modem proc ctx RAM memory\n");
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return rc;
	}

	rc = ipa3_init_smem_region(IPA_MEM_PART(modem_comp_decomp_size),
		IPA_MEM_PART(modem_comp_decomp_ofst));
	if (rc) {
		IPAERR("failed to initialize Modem Comp/Decomp RAM memory\n");
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return rc;
	}
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return rc;
}

static void ipa3_destroy_imm(void *user1, int user2)
{
	ipahal_destroy_imm_cmd(user1);
}

static void ipa3_q6_pipe_delay(bool delay)
{
	int client_idx;
	int ep_idx;
	struct ipa_ep_cfg_ctrl ep_ctrl;

	memset(&ep_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
	ep_ctrl.ipa_ep_delay = delay;

	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		if (IPA_CLIENT_IS_Q6_PROD(client_idx)) {
			ep_idx = ipa3_get_ep_mapping(client_idx);
			if (ep_idx == -1)
				continue;

			ipahal_write_reg_n_fields(IPA_ENDP_INIT_CTRL_n,
				ep_idx, &ep_ctrl);
		}
	}
}

static void ipa3_q6_avoid_holb(void)
{
	int ep_idx;
	int client_idx;
	struct ipa_ep_cfg_ctrl ep_suspend;
	struct ipa_ep_cfg_holb ep_holb;

	memset(&ep_suspend, 0, sizeof(ep_suspend));
	memset(&ep_holb, 0, sizeof(ep_holb));

	ep_suspend.ipa_ep_suspend = true;
	ep_holb.tmr_val = 0;
	ep_holb.en = 1;

	if (ipa3_ctx->ipa_hw_type == IPA_HW_v4_2)
		ipa3_cal_ep_holb_scale_base_val(ep_holb.tmr_val, &ep_holb);

	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		if (IPA_CLIENT_IS_Q6_CONS(client_idx)) {
			ep_idx = ipa3_get_ep_mapping(client_idx);
			if (ep_idx == -1)
				continue;

			/* from IPA 4.0 pipe suspend is not supported */
			if (ipa3_ctx->ipa_hw_type < IPA_HW_v4_0)
				ipahal_write_reg_n_fields(
				IPA_ENDP_INIT_CTRL_n,
				ep_idx, &ep_suspend);

			/*
			 * ipa3_cfg_ep_holb is not used here because we are
			 * setting HOLB on Q6 pipes, and from APPS perspective
			 * they are not valid, therefore, the above function
			 * will fail.
			 */
			ipahal_write_reg_n_fields(
				IPA_ENDP_INIT_HOL_BLOCK_TIMER_n,
				ep_idx, &ep_holb);
			ipahal_write_reg_n_fields(
				IPA_ENDP_INIT_HOL_BLOCK_EN_n,
				ep_idx, &ep_holb);

			/* IPA4.5 issue requires HOLB_EN to be written twice */
			if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_5)
				ipahal_write_reg_n_fields(
					IPA_ENDP_INIT_HOL_BLOCK_EN_n,
					ep_idx, &ep_holb);
		}
	}
}

static void ipa3_halt_q6_gsi_channels(bool prod)
{
	int ep_idx;
	int client_idx;
	const struct ipa_gsi_ep_config *gsi_ep_cfg;
	int i;
	int ret;
	int code = 0;

	/* if prod flag is true, then we halt the producer channels also */
	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		if (IPA_CLIENT_IS_Q6_CONS(client_idx)
			|| (IPA_CLIENT_IS_Q6_PROD(client_idx) && prod)) {
			ep_idx = ipa3_get_ep_mapping(client_idx);
			if (ep_idx == -1)
				continue;

			gsi_ep_cfg = ipa3_get_gsi_ep_info(client_idx);
			if (!gsi_ep_cfg) {
				IPAERR("failed to get GSI config\n");
				ipa_assert();
				return;
			}

			ret = gsi_halt_channel_ee(
				gsi_ep_cfg->ipa_gsi_chan_num, gsi_ep_cfg->ee,
				&code);
			for (i = 0; i < IPA_GSI_CHANNEL_STOP_MAX_RETRY &&
				ret == -GSI_STATUS_AGAIN; i++) {
				IPADBG(
				"ch %d ee %d with code %d\n is busy try again",
					gsi_ep_cfg->ipa_gsi_chan_num,
					gsi_ep_cfg->ee,
					code);
				usleep_range(IPA_GSI_CHANNEL_HALT_MIN_SLEEP,
					IPA_GSI_CHANNEL_HALT_MAX_SLEEP);
				ret = gsi_halt_channel_ee(
					gsi_ep_cfg->ipa_gsi_chan_num,
					gsi_ep_cfg->ee, &code);
			}
			if (ret == GSI_STATUS_SUCCESS)
				IPADBG("halted gsi ch %d ee %d with code %d\n",
				gsi_ep_cfg->ipa_gsi_chan_num,
				gsi_ep_cfg->ee,
				code);
			else
				IPAERR("failed to halt ch %d ee %d code %d\n",
				gsi_ep_cfg->ipa_gsi_chan_num,
				gsi_ep_cfg->ee,
				code);
		}
	}
}

static int ipa3_q6_clean_q6_flt_tbls(enum ipa_ip_type ip,
	enum ipa_rule_type rlt)
{
	struct ipa3_desc *desc;
	struct ipahal_imm_cmd_dma_shared_mem cmd = {0};
	struct ipahal_imm_cmd_pyld **cmd_pyld;
	int retval = 0;
	int pipe_idx;
	int flt_idx = 0;
	int num_cmds = 0;
	int index;
	u32 lcl_addr_mem_part;
	u32 lcl_hdr_sz;
	struct ipa_mem_buffer mem;

	IPADBG("Entry\n");

	if ((ip >= IPA_IP_MAX) || (rlt >= IPA_RULE_TYPE_MAX)) {
		IPAERR("Input Err: ip=%d ; rlt=%d\n", ip, rlt);
		return -EINVAL;
	}

	/*
	 * SRAM memory not allocated to hash tables. Cleaning the of hash table
	 * operation not supported.
	 */
	if (rlt == IPA_RULE_HASHABLE && ipa3_ctx->ipa_fltrt_not_hashable) {
		IPADBG("Clean hashable rules not supported\n");
		return retval;
	}

	/* Up to filtering pipes we have filtering tables */
	desc = kcalloc(ipa3_ctx->ep_flt_num, sizeof(struct ipa3_desc),
		GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	cmd_pyld = kcalloc(ipa3_ctx->ep_flt_num,
		sizeof(struct ipahal_imm_cmd_pyld *), GFP_KERNEL);
	if (!cmd_pyld) {
		retval = -ENOMEM;
		goto free_desc;
	}

	if (ip == IPA_IP_v4) {
		if (rlt == IPA_RULE_HASHABLE) {
			lcl_addr_mem_part = IPA_MEM_PART(v4_flt_hash_ofst);
			lcl_hdr_sz = IPA_MEM_PART(v4_flt_hash_size);
		} else {
			lcl_addr_mem_part = IPA_MEM_PART(v4_flt_nhash_ofst);
			lcl_hdr_sz = IPA_MEM_PART(v4_flt_nhash_size);
		}
	} else {
		if (rlt == IPA_RULE_HASHABLE) {
			lcl_addr_mem_part = IPA_MEM_PART(v6_flt_hash_ofst);
			lcl_hdr_sz = IPA_MEM_PART(v6_flt_hash_size);
		} else {
			lcl_addr_mem_part = IPA_MEM_PART(v6_flt_nhash_ofst);
			lcl_hdr_sz = IPA_MEM_PART(v6_flt_nhash_size);
		}
	}

	retval = ipahal_flt_generate_empty_img(1, lcl_hdr_sz, lcl_hdr_sz,
		0, &mem, true);
	if (retval) {
		IPAERR("failed to generate flt single tbl empty img\n");
		goto free_cmd_pyld;
	}

	for (pipe_idx = 0; pipe_idx < ipa3_ctx->ipa_num_pipes; pipe_idx++) {
		if (!ipa_is_ep_support_flt(pipe_idx))
			continue;

		/*
		 * Iterating over all the filtering pipes which are either
		 * invalid but connected or connected but not configured by AP.
		 */
		if (!ipa3_ctx->ep[pipe_idx].valid ||
		    ipa3_ctx->ep[pipe_idx].skip_ep_cfg) {

			if (num_cmds >= ipa3_ctx->ep_flt_num) {
				IPAERR("number of commands is out of range\n");
				retval = -ENOBUFS;
				goto free_empty_img;
			}

			cmd.is_read = false;
			cmd.skip_pipeline_clear = false;
			cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
			cmd.size = mem.size;
			cmd.system_addr = mem.phys_base;
			cmd.local_addr =
				ipa3_ctx->smem_restricted_bytes +
				lcl_addr_mem_part +
				ipahal_get_hw_tbl_hdr_width() +
				flt_idx * ipahal_get_hw_tbl_hdr_width();
			cmd_pyld[num_cmds] = ipahal_construct_imm_cmd(
				IPA_IMM_CMD_DMA_SHARED_MEM, &cmd, false);
			if (!cmd_pyld[num_cmds]) {
				IPAERR("fail construct dma_shared_mem cmd\n");
				retval = -ENOMEM;
				goto free_empty_img;
			}
			ipa3_init_imm_cmd_desc(&desc[num_cmds],
				cmd_pyld[num_cmds]);
			++num_cmds;
		}

		++flt_idx;
	}

	IPADBG("Sending %d descriptors for flt tbl clearing\n", num_cmds);
	retval = ipa3_send_cmd(num_cmds, desc);
	if (retval) {
		IPAERR("failed to send immediate command (err %d)\n", retval);
		retval = -EFAULT;
	}

free_empty_img:
	ipahal_free_dma_mem(&mem);
free_cmd_pyld:
	for (index = 0; index < num_cmds; index++)
		ipahal_destroy_imm_cmd(cmd_pyld[index]);
	kfree(cmd_pyld);
free_desc:
	kfree(desc);
	return retval;
}

static int ipa3_q6_clean_q6_rt_tbls(enum ipa_ip_type ip,
	enum ipa_rule_type rlt)
{
	struct ipa3_desc *desc;
	struct ipahal_imm_cmd_dma_shared_mem cmd = {0};
	struct ipahal_imm_cmd_pyld *cmd_pyld = NULL;
	int retval = 0;
	u32 modem_rt_index_lo;
	u32 modem_rt_index_hi;
	u32 lcl_addr_mem_part;
	u32 lcl_hdr_sz;
	struct ipa_mem_buffer mem;

	IPADBG("Entry\n");

	if ((ip >= IPA_IP_MAX) || (rlt >= IPA_RULE_TYPE_MAX)) {
		IPAERR("Input Err: ip=%d ; rlt=%d\n", ip, rlt);
		return -EINVAL;
	}

	/*
	 * SRAM memory not allocated to hash tables. Cleaning the of hash table
	 * operation not supported.
	 */
	if (rlt == IPA_RULE_HASHABLE && ipa3_ctx->ipa_fltrt_not_hashable) {
		IPADBG("Clean hashable rules not supported\n");
		return retval;
	}

	if (ip == IPA_IP_v4) {
		modem_rt_index_lo = IPA_MEM_PART(v4_modem_rt_index_lo);
		modem_rt_index_hi = IPA_MEM_PART(v4_modem_rt_index_hi);
		if (rlt == IPA_RULE_HASHABLE) {
			lcl_addr_mem_part = IPA_MEM_PART(v4_rt_hash_ofst);
			lcl_hdr_sz =  IPA_MEM_PART(v4_flt_hash_size);
		} else {
			lcl_addr_mem_part = IPA_MEM_PART(v4_rt_nhash_ofst);
			lcl_hdr_sz = IPA_MEM_PART(v4_flt_nhash_size);
		}
	} else {
		modem_rt_index_lo = IPA_MEM_PART(v6_modem_rt_index_lo);
		modem_rt_index_hi = IPA_MEM_PART(v6_modem_rt_index_hi);
		if (rlt == IPA_RULE_HASHABLE) {
			lcl_addr_mem_part = IPA_MEM_PART(v6_rt_hash_ofst);
			lcl_hdr_sz =  IPA_MEM_PART(v6_flt_hash_size);
		} else {
			lcl_addr_mem_part = IPA_MEM_PART(v6_rt_nhash_ofst);
			lcl_hdr_sz = IPA_MEM_PART(v6_flt_nhash_size);
		}
	}

	retval = ipahal_rt_generate_empty_img(
		modem_rt_index_hi - modem_rt_index_lo + 1,
		lcl_hdr_sz, lcl_hdr_sz, &mem, true);
	if (retval) {
		IPAERR("fail generate empty rt img\n");
		return -ENOMEM;
	}

	desc = kzalloc(sizeof(struct ipa3_desc), GFP_KERNEL);
	if (!desc) {
		IPAERR("failed to allocate memory\n");
		goto free_empty_img;
	}

	cmd.is_read = false;
	cmd.skip_pipeline_clear = false;
	cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
	cmd.size = mem.size;
	cmd.system_addr =  mem.phys_base;
	cmd.local_addr = ipa3_ctx->smem_restricted_bytes +
		lcl_addr_mem_part +
		modem_rt_index_lo * ipahal_get_hw_tbl_hdr_width();
	cmd_pyld = ipahal_construct_imm_cmd(
			IPA_IMM_CMD_DMA_SHARED_MEM, &cmd, false);
	if (!cmd_pyld) {
		IPAERR("failed to construct dma_shared_mem imm cmd\n");
		retval = -ENOMEM;
		goto free_desc;
	}
	ipa3_init_imm_cmd_desc(desc, cmd_pyld);

	IPADBG("Sending 1 descriptor for rt tbl clearing\n");
	retval = ipa3_send_cmd(1, desc);
	if (retval) {
		IPAERR("failed to send immediate command (err %d)\n", retval);
		retval = -EFAULT;
	}

	ipahal_destroy_imm_cmd(cmd_pyld);
free_desc:
	kfree(desc);
free_empty_img:
	ipahal_free_dma_mem(&mem);
	return retval;
}

static int ipa3_q6_clean_q6_tables(void)
{
	struct ipa3_desc *desc;
	struct ipahal_imm_cmd_pyld *cmd_pyld = NULL;
	struct ipahal_imm_cmd_register_write reg_write_cmd = {0};
	int retval = 0;
	struct ipahal_reg_fltrt_hash_flush flush;
	struct ipahal_reg_valmask valmask;

	IPADBG("Entry\n");


	if (ipa3_q6_clean_q6_flt_tbls(IPA_IP_v4, IPA_RULE_HASHABLE)) {
		IPAERR("failed to clean q6 flt tbls (v4/hashable)\n");
		return -EFAULT;
	}
	if (ipa3_q6_clean_q6_flt_tbls(IPA_IP_v6, IPA_RULE_HASHABLE)) {
		IPAERR("failed to clean q6 flt tbls (v6/hashable)\n");
		return -EFAULT;
	}
	if (ipa3_q6_clean_q6_flt_tbls(IPA_IP_v4, IPA_RULE_NON_HASHABLE)) {
		IPAERR("failed to clean q6 flt tbls (v4/non-hashable)\n");
		return -EFAULT;
	}
	if (ipa3_q6_clean_q6_flt_tbls(IPA_IP_v6, IPA_RULE_NON_HASHABLE)) {
		IPAERR("failed to clean q6 flt tbls (v6/non-hashable)\n");
		return -EFAULT;
	}

	if (ipa3_q6_clean_q6_rt_tbls(IPA_IP_v4, IPA_RULE_HASHABLE)) {
		IPAERR("failed to clean q6 rt tbls (v4/hashable)\n");
		return -EFAULT;
	}
	if (ipa3_q6_clean_q6_rt_tbls(IPA_IP_v6, IPA_RULE_HASHABLE)) {
		IPAERR("failed to clean q6 rt tbls (v6/hashable)\n");
		return -EFAULT;
	}
	if (ipa3_q6_clean_q6_rt_tbls(IPA_IP_v4, IPA_RULE_NON_HASHABLE)) {
		IPAERR("failed to clean q6 rt tbls (v4/non-hashable)\n");
		return -EFAULT;
	}
	if (ipa3_q6_clean_q6_rt_tbls(IPA_IP_v6, IPA_RULE_NON_HASHABLE)) {
		IPAERR("failed to clean q6 rt tbls (v6/non-hashable)\n");
		return -EFAULT;
	}

	/*
	 * SRAM memory not allocated to hash tables. Cleaning the of hash table
	 * operation not supported.
	 */
	if (ipa3_ctx->ipa_fltrt_not_hashable)
		return retval;
	/* Flush rules cache */
	desc = kzalloc(sizeof(struct ipa3_desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	flush.v4_flt = true;
	flush.v4_rt = true;
	flush.v6_flt = true;
	flush.v6_rt = true;
	ipahal_get_fltrt_hash_flush_valmask(&flush, &valmask);
	reg_write_cmd.skip_pipeline_clear = false;
	reg_write_cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
	reg_write_cmd.offset = ipahal_get_reg_ofst(IPA_FILT_ROUT_HASH_FLUSH);
	reg_write_cmd.value = valmask.val;
	reg_write_cmd.value_mask = valmask.mask;
	cmd_pyld = ipahal_construct_imm_cmd(IPA_IMM_CMD_REGISTER_WRITE,
		&reg_write_cmd, false);
	if (!cmd_pyld) {
		IPAERR("fail construct register_write imm cmd\n");
		retval = -EFAULT;
		goto bail_desc;
	}
	ipa3_init_imm_cmd_desc(desc, cmd_pyld);

	IPADBG("Sending 1 descriptor for tbls flush\n");
	retval = ipa3_send_cmd(1, desc);
	if (retval) {
		IPAERR("failed to send immediate command (err %d)\n", retval);
		retval = -EFAULT;
	}

	ipahal_destroy_imm_cmd(cmd_pyld);

bail_desc:
	kfree(desc);
	IPADBG("Done - retval = %d\n", retval);
	return retval;
}

static int ipa3_q6_set_ex_path_to_apps(void)
{
	int ep_idx;
	int client_idx;
	struct ipa3_desc *desc;
	int num_descs = 0;
	int index;
	struct ipahal_imm_cmd_register_write reg_write;
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	int retval;

	desc = kcalloc(ipa3_ctx->ipa_num_pipes, sizeof(struct ipa3_desc),
			GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	/* Set the exception path to AP */
	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		ep_idx = ipa3_get_ep_mapping(client_idx);
		if (ep_idx == -1 || (ep_idx >= IPA3_MAX_NUM_PIPES))
			continue;

		/* disable statuses for all modem controlled prod pipes */
		if (IPA_CLIENT_IS_Q6_PROD(client_idx) ||
			(ipa3_ctx->ep[ep_idx].valid &&
			ipa3_ctx->ep[ep_idx].skip_ep_cfg) ||
			(ipa3_ctx->ep[ep_idx].client == IPA_CLIENT_APPS_WAN_PROD
			&& ipa3_ctx->modem_cfg_emb_pipe_flt)) {
			ipa_assert_on(num_descs >= ipa3_ctx->ipa_num_pipes);

			ipa3_ctx->ep[ep_idx].status.status_en = false;
			reg_write.skip_pipeline_clear = false;
			reg_write.pipeline_clear_options =
				IPAHAL_HPS_CLEAR;
			reg_write.offset =
				ipahal_get_reg_n_ofst(IPA_ENDP_STATUS_n,
					ep_idx);
			reg_write.value = 0;
			reg_write.value_mask = ~0;
			cmd_pyld = ipahal_construct_imm_cmd(
				IPA_IMM_CMD_REGISTER_WRITE, &reg_write, false);
			if (!cmd_pyld) {
				IPAERR("fail construct register_write cmd\n");
				ipa_assert();
				return -ENOMEM;
			}

			ipa3_init_imm_cmd_desc(&desc[num_descs], cmd_pyld);
			desc[num_descs].callback = ipa3_destroy_imm;
			desc[num_descs].user1 = cmd_pyld;
			++num_descs;
		}
	}

	/* Will wait 500msecs for IPA tag process completion */
	retval = ipa3_tag_process(desc, num_descs,
		msecs_to_jiffies(CLEANUP_TAG_PROCESS_TIMEOUT));
	if (retval) {
		IPAERR("TAG process failed! (error %d)\n", retval);
		/* For timeout error ipa3_destroy_imm cb will destroy user1 */
		if (retval != -ETIME) {
			for (index = 0; index < num_descs; index++)
				if (desc[index].callback)
					desc[index].callback(desc[index].user1,
						desc[index].user2);
			retval = -EINVAL;
		}
	}

	kfree(desc);

	return retval;
}

/*
 * ipa3_update_ssr_state() - updating current SSR state
 * @is_ssr:	[in] Current SSR state
 */

void ipa3_update_ssr_state(bool is_ssr)
{
	if (is_ssr)
		atomic_set(&ipa3_ctx->is_ssr, 1);
	else
		atomic_set(&ipa3_ctx->is_ssr, 0);
}

/**
 * ipa3_q6_pre_shutdown_cleanup() - A cleanup for all Q6 related configuration
 *                    in IPA HW. This is performed in case of SSR.
 *
 * This is a mandatory procedure, in case one of the steps fails, the
 * AP needs to restart.
 */
void ipa3_q6_pre_shutdown_cleanup(void)
{
	IPADBG_LOW("ENTER\n");

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	ipa3_update_ssr_state(true);
	if (!ipa3_ctx->ipa_endp_delay_wa)
		ipa3_q6_pipe_delay(true);
	ipa3_q6_avoid_holb();
	if (ipa3_ctx->ipa_config_is_mhi)
		ipa3_set_reset_client_cons_pipe_sus_holb(true,
		IPA_CLIENT_MHI_CONS);
	if (ipa3_q6_clean_q6_tables()) {
		IPAERR("Failed to clean Q6 tables\n");
		/*
		 * Indicates IPA hardware is stalled, unexpected
		 * hardware state.
		 */
		BUG();
	}
	if (ipa3_q6_set_ex_path_to_apps()) {
		IPAERR("Failed to redirect exceptions to APPS\n");
		/*
		 * Indicates IPA hardware is stalled, unexpected
		 * hardware state.
		 */
		BUG();
	}
	/* Remove delay from Q6 PRODs to avoid pending descriptors
	 * on pipe reset procedure
	 */
	if (!ipa3_ctx->ipa_endp_delay_wa) {
		ipa3_q6_pipe_delay(false);
		ipa3_set_reset_client_prod_pipe_delay(true,
			IPA_CLIENT_USB_PROD);
	} else {
		ipa3_start_stop_client_prod_gsi_chnl(IPA_CLIENT_USB_PROD,
						false);
	}

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	IPADBG_LOW("Exit with success\n");
}

/*
 * ipa3_q6_post_shutdown_cleanup() - As part of this cleanup
 * check if GSI channel related to Q6 producer client is empty.
 *
 * Q6 GSI channel emptiness is needed to garantee no descriptors with invalid
 *  info are injected into IPA RX from IPA_IF, while modem is restarting.
 */
void ipa3_q6_post_shutdown_cleanup(void)
{
	int client_idx;
	int ep_idx;
	bool prod = false;

	IPADBG_LOW("ENTER\n");

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	/* Handle the issue where SUSPEND was removed for some reason */
	ipa3_q6_avoid_holb();

	/* halt both prod and cons channels starting at IPAv4 */
	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_0) {
		prod = true;
		ipa3_halt_q6_gsi_channels(prod);
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		IPADBG("Exit without consumer check\n");
		return;
	}

	ipa3_halt_q6_gsi_channels(prod);

	if (!ipa3_ctx->uc_ctx.uc_loaded) {
		IPAERR("uC is not loaded. Skipping\n");
		return;
	}

	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++)
		if (IPA_CLIENT_IS_Q6_PROD(client_idx)) {
			ep_idx = ipa3_get_ep_mapping(client_idx);
			if (ep_idx == -1)
				continue;

			if (ipa3_uc_is_gsi_channel_empty(client_idx)) {
				IPAERR("fail to validate Q6 ch emptiness %d\n",
					client_idx);
				/*
				 * Indicates GSI hardware is stalled, unexpected
				 * hardware state.
				 * Remove bug for adb reboot issue.
				 */
			}
		}

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	IPADBG_LOW("Exit with success\n");
}

/**
 * ipa3_q6_pre_powerup_cleanup() - A cleanup routine for pheripheral
 * configuration in IPA HW. This is performed in case of SSR.
 *
 * This is a mandatory procedure, in case one of the steps fails, the
 * AP needs to restart.
 */
void ipa3_q6_pre_powerup_cleanup(void)
{
	IPADBG_LOW("ENTER\n");

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	if (ipa3_ctx->ipa_config_is_mhi)
		ipa3_set_reset_client_prod_pipe_delay(true,
			IPA_CLIENT_MHI_PROD);

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	IPADBG_LOW("Exit with success\n");
}

/*
 * ipa3_client_prod_post_shutdown_cleanup () - As part of this function
 * set end point delay client producer pipes and starting corresponding
 * gsi channels
 */

void ipa3_client_prod_post_shutdown_cleanup(void)
{
	IPADBG_LOW("ENTER\n");

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	ipa3_set_reset_client_prod_pipe_delay(true,
				IPA_CLIENT_USB_PROD);
	ipa3_start_stop_client_prod_gsi_chnl(IPA_CLIENT_USB_PROD, true);

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	IPADBG_LOW("Exit with success\n");
}

static inline void ipa3_sram_set_canary(u32 *sram_mmio, int offset)
{
	/* Set 4 bytes of CANARY before the offset */
	sram_mmio[(offset - 4) / 4] = IPA_MEM_CANARY_VAL;
}

/**
 * _ipa_init_sram_v3() - Initialize IPA local SRAM.
 *
 * Return codes: 0 for success, negative value for failure
 */
int _ipa_init_sram_v3(void)
{
	u32 *ipa_sram_mmio;
	unsigned long phys_addr;

	IPADBG(
	    "ipa_wrapper_base(0x%08X) ipa_reg_base_ofst(0x%08X) IPA_SW_AREA_RAM_DIRECT_ACCESS_n(0x%08X) smem_restricted_bytes(0x%08X) smem_sz(0x%08X)\n",
	    ipa3_ctx->ipa_wrapper_base,
	    ipa3_ctx->ctrl->ipa_reg_base_ofst,
	    ipahal_get_reg_n_ofst(
		IPA_SW_AREA_RAM_DIRECT_ACCESS_n,
		ipa3_ctx->smem_restricted_bytes / 4),
	    ipa3_ctx->smem_restricted_bytes,
	    ipa3_ctx->smem_sz);

	phys_addr = ipa3_ctx->ipa_wrapper_base +
		ipa3_ctx->ctrl->ipa_reg_base_ofst +
		ipahal_get_reg_n_ofst(IPA_SW_AREA_RAM_DIRECT_ACCESS_n,
			ipa3_ctx->smem_restricted_bytes / 4);

	ipa_sram_mmio = ioremap(phys_addr, ipa3_ctx->smem_sz);
	if (!ipa_sram_mmio) {
		IPAERR("fail to ioremap IPA SRAM\n");
		return -ENOMEM;
	}

	/* Consult with ipa_i.h on the location of the CANARY values */
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_flt_hash_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_flt_hash_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio,
		IPA_MEM_PART(v4_flt_nhash_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_flt_nhash_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_flt_hash_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_flt_hash_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio,
		IPA_MEM_PART(v6_flt_nhash_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_flt_nhash_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_rt_hash_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_rt_hash_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_rt_nhash_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_rt_nhash_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_rt_hash_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_rt_hash_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_rt_nhash_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_rt_nhash_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(modem_hdr_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(modem_hdr_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio,
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio,
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst));
	if (ipa_get_hw_type() >= IPA_HW_v4_5) {
		ipa3_sram_set_canary(ipa_sram_mmio,
			IPA_MEM_PART(nat_tbl_ofst) - 12);
	}
	if (ipa_get_hw_type() >= IPA_HW_v4_0) {
		if (ipa_get_hw_type() < IPA_HW_v4_5) {
			ipa3_sram_set_canary(ipa_sram_mmio,
				IPA_MEM_PART(pdn_config_ofst) - 4);
			ipa3_sram_set_canary(ipa_sram_mmio,
				IPA_MEM_PART(pdn_config_ofst));
			ipa3_sram_set_canary(ipa_sram_mmio,
				IPA_MEM_PART(stats_quota_q6_ofst) - 4);
			ipa3_sram_set_canary(ipa_sram_mmio,
				IPA_MEM_PART(stats_quota_q6_ofst));
		} else {
			ipa3_sram_set_canary(ipa_sram_mmio,
				IPA_MEM_PART(stats_quota_q6_ofst) - 12);
		}
	}

	if (ipa_get_hw_type() <= IPA_HW_v3_5 ||
		ipa_get_hw_type() >= IPA_HW_v4_5) {
		ipa3_sram_set_canary(ipa_sram_mmio,
			IPA_MEM_PART(modem_ofst) - 4);
		ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(modem_ofst));
	}
	ipa3_sram_set_canary(ipa_sram_mmio,
		(ipa_get_hw_type() >= IPA_HW_v3_5) ?
			IPA_MEM_PART(uc_descriptor_ram_ofst) :
			IPA_MEM_PART(end_ofst));

	iounmap(ipa_sram_mmio);

	return 0;
}

/**
 * _ipa_init_hdr_v3_0() - Initialize IPA header block.
 *
 * Return codes: 0 for success, negative value for failure
 */
int _ipa_init_hdr_v3_0(void)
{
	struct ipa3_desc desc;
	struct ipa_mem_buffer mem;
	struct ipahal_imm_cmd_hdr_init_local cmd = {0};
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	struct ipahal_imm_cmd_dma_shared_mem dma_cmd = { 0 };

	mem.size = IPA_MEM_PART(modem_hdr_size) + IPA_MEM_PART(apps_hdr_size);
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev, mem.size, &mem.phys_base,
		GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}
	memset(mem.base, 0, mem.size);

	cmd.hdr_table_addr = mem.phys_base;
	cmd.size_hdr_table = mem.size;
	cmd.hdr_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(modem_hdr_ofst);
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_HDR_INIT_LOCAL, &cmd, false);
	if (!cmd_pyld) {
		IPAERR("fail to construct hdr_init_local imm cmd\n");
		dma_free_coherent(ipa3_ctx->pdev,
			mem.size, mem.base,
			mem.phys_base);
		return -EFAULT;
	}
	ipa3_init_imm_cmd_desc(&desc, cmd_pyld);
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa3_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		ipahal_destroy_imm_cmd(cmd_pyld);
		dma_free_coherent(ipa3_ctx->pdev,
			mem.size, mem.base,
			mem.phys_base);
		return -EFAULT;
	}

	ipahal_destroy_imm_cmd(cmd_pyld);
	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base, mem.phys_base);

	mem.size = IPA_MEM_PART(modem_hdr_proc_ctx_size) +
		IPA_MEM_PART(apps_hdr_proc_ctx_size);
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev, mem.size, &mem.phys_base,
		GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}
	memset(mem.base, 0, mem.size);

	dma_cmd.is_read = false;
	dma_cmd.skip_pipeline_clear = false;
	dma_cmd.pipeline_clear_options = IPAHAL_HPS_CLEAR;
	dma_cmd.system_addr = mem.phys_base;
	dma_cmd.local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst);
	dma_cmd.size = mem.size;
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_DMA_SHARED_MEM, &dma_cmd, false);
	if (!cmd_pyld) {
		IPAERR("fail to construct dma_shared_mem imm\n");
		dma_free_coherent(ipa3_ctx->pdev,
			mem.size, mem.base,
			mem.phys_base);
		return -ENOMEM;
	}
	ipa3_init_imm_cmd_desc(&desc, cmd_pyld);
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa3_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		ipahal_destroy_imm_cmd(cmd_pyld);
		dma_free_coherent(ipa3_ctx->pdev,
			mem.size,
			mem.base,
			mem.phys_base);
		return -EBUSY;
	}
	ipahal_destroy_imm_cmd(cmd_pyld);

	ipahal_write_reg(IPA_LOCAL_PKT_PROC_CNTXT_BASE, dma_cmd.local_addr);

	dma_free_coherent(ipa3_ctx->pdev, mem.size, mem.base, mem.phys_base);

	return 0;
}

/**
 * _ipa_init_rt4_v3() - Initialize IPA routing block for IPv4.
 *
 * Return codes: 0 for success, negative value for failure
 */
int _ipa_init_rt4_v3(void)
{
	struct ipa3_desc desc;
	struct ipa_mem_buffer mem;
	struct ipahal_imm_cmd_ip_v4_routing_init v4_cmd;
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	int i;
	int rc = 0;

	for (i = IPA_MEM_PART(v4_modem_rt_index_lo);
		i <= IPA_MEM_PART(v4_modem_rt_index_hi);
		i++)
		ipa3_ctx->rt_idx_bitmap[IPA_IP_v4] |= (1 << i);
	IPADBG("v4 rt bitmap 0x%lx\n", ipa3_ctx->rt_idx_bitmap[IPA_IP_v4]);

	rc = ipahal_rt_generate_empty_img(IPA_MEM_PART(v4_rt_num_index),
		IPA_MEM_PART(v4_rt_hash_size), IPA_MEM_PART(v4_rt_nhash_size),
		&mem, false);
	if (rc) {
		IPAERR("fail generate empty v4 rt img\n");
		return rc;
	}

	/*
	 * SRAM memory not allocated to hash tables. Initializing/Sending
	 * command to hash tables(filer/routing) operation not supported.
	 */
	if (ipa3_ctx->ipa_fltrt_not_hashable) {
		v4_cmd.hash_rules_addr = 0;
		v4_cmd.hash_rules_size = 0;
		v4_cmd.hash_local_addr = 0;
	} else {
		v4_cmd.hash_rules_addr = mem.phys_base;
		v4_cmd.hash_rules_size = mem.size;
		v4_cmd.hash_local_addr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v4_rt_hash_ofst);
	}

	v4_cmd.nhash_rules_addr = mem.phys_base;
	v4_cmd.nhash_rules_size = mem.size;
	v4_cmd.nhash_local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v4_rt_nhash_ofst);
	IPADBG("putting hashable routing IPv4 rules to phys 0x%x\n",
				v4_cmd.hash_local_addr);
	IPADBG("putting non-hashable routing IPv4 rules to phys 0x%x\n",
				v4_cmd.nhash_local_addr);
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_IP_V4_ROUTING_INIT, &v4_cmd, false);
	if (!cmd_pyld) {
		IPAERR("fail construct ip_v4_rt_init imm cmd\n");
		rc = -EPERM;
		goto free_mem;
	}

	ipa3_init_imm_cmd_desc(&desc, cmd_pyld);
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa3_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	ipahal_destroy_imm_cmd(cmd_pyld);

free_mem:
	ipahal_free_dma_mem(&mem);
	return rc;
}

/**
 * _ipa_init_rt6_v3() - Initialize IPA routing block for IPv6.
 *
 * Return codes: 0 for success, negative value for failure
 */
int _ipa_init_rt6_v3(void)
{
	struct ipa3_desc desc;
	struct ipa_mem_buffer mem;
	struct ipahal_imm_cmd_ip_v6_routing_init v6_cmd;
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	int i;
	int rc = 0;

	for (i = IPA_MEM_PART(v6_modem_rt_index_lo);
		i <= IPA_MEM_PART(v6_modem_rt_index_hi);
		i++)
		ipa3_ctx->rt_idx_bitmap[IPA_IP_v6] |= (1 << i);
	IPADBG("v6 rt bitmap 0x%lx\n", ipa3_ctx->rt_idx_bitmap[IPA_IP_v6]);

	rc = ipahal_rt_generate_empty_img(IPA_MEM_PART(v6_rt_num_index),
		IPA_MEM_PART(v6_rt_hash_size), IPA_MEM_PART(v6_rt_nhash_size),
		&mem, false);
	if (rc) {
		IPAERR("fail generate empty v6 rt img\n");
		return rc;
	}

	/*
	 * SRAM memory not allocated to hash tables. Initializing/Sending
	 * command to hash tables(filer/routing) operation not supported.
	 */
	if (ipa3_ctx->ipa_fltrt_not_hashable) {
		v6_cmd.hash_rules_addr = 0;
		v6_cmd.hash_rules_size = 0;
		v6_cmd.hash_local_addr = 0;
	} else {
		v6_cmd.hash_rules_addr = mem.phys_base;
		v6_cmd.hash_rules_size = mem.size;
		v6_cmd.hash_local_addr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v6_rt_hash_ofst);
	}

	v6_cmd.nhash_rules_addr = mem.phys_base;
	v6_cmd.nhash_rules_size = mem.size;
	v6_cmd.nhash_local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v6_rt_nhash_ofst);
	IPADBG("putting hashable routing IPv6 rules to phys 0x%x\n",
				v6_cmd.hash_local_addr);
	IPADBG("putting non-hashable routing IPv6 rules to phys 0x%x\n",
				v6_cmd.nhash_local_addr);
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_IP_V6_ROUTING_INIT, &v6_cmd, false);
	if (!cmd_pyld) {
		IPAERR("fail construct ip_v6_rt_init imm cmd\n");
		rc = -EPERM;
		goto free_mem;
	}

	ipa3_init_imm_cmd_desc(&desc, cmd_pyld);
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa3_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	ipahal_destroy_imm_cmd(cmd_pyld);

free_mem:
	ipahal_free_dma_mem(&mem);
	return rc;
}

/**
 * _ipa_init_flt4_v3() - Initialize IPA filtering block for IPv4.
 *
 * Return codes: 0 for success, negative value for failure
 */
int _ipa_init_flt4_v3(void)
{
	struct ipa3_desc desc;
	struct ipa_mem_buffer mem;
	struct ipahal_imm_cmd_ip_v4_filter_init v4_cmd;
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	int rc;

	rc = ipahal_flt_generate_empty_img(ipa3_ctx->ep_flt_num,
		IPA_MEM_PART(v4_flt_hash_size),
		IPA_MEM_PART(v4_flt_nhash_size), ipa3_ctx->ep_flt_bitmap,
		&mem, false);
	if (rc) {
		IPAERR("fail generate empty v4 flt img\n");
		return rc;
	}

	/*
	 * SRAM memory not allocated to hash tables. Initializing/Sending
	 * command to hash tables(filer/routing) operation not supported.
	 */
	if (ipa3_ctx->ipa_fltrt_not_hashable) {
		v4_cmd.hash_rules_addr = 0;
		v4_cmd.hash_rules_size = 0;
		v4_cmd.hash_local_addr = 0;
	} else {
		v4_cmd.hash_rules_addr = mem.phys_base;
		v4_cmd.hash_rules_size = mem.size;
		v4_cmd.hash_local_addr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v4_flt_hash_ofst);
	}

	v4_cmd.nhash_rules_addr = mem.phys_base;
	v4_cmd.nhash_rules_size = mem.size;
	v4_cmd.nhash_local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v4_flt_nhash_ofst);
	IPADBG("putting hashable filtering IPv4 rules to phys 0x%x\n",
				v4_cmd.hash_local_addr);
	IPADBG("putting non-hashable filtering IPv4 rules to phys 0x%x\n",
				v4_cmd.nhash_local_addr);
	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_IP_V4_FILTER_INIT, &v4_cmd, false);
	if (!cmd_pyld) {
		IPAERR("fail construct ip_v4_flt_init imm cmd\n");
		rc = -EPERM;
		goto free_mem;
	}

	ipa3_init_imm_cmd_desc(&desc, cmd_pyld);
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa3_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	ipahal_destroy_imm_cmd(cmd_pyld);

free_mem:
	ipahal_free_dma_mem(&mem);
	return rc;
}

/**
 * _ipa_init_flt6_v3() - Initialize IPA filtering block for IPv6.
 *
 * Return codes: 0 for success, negative value for failure
 */
int _ipa_init_flt6_v3(void)
{
	struct ipa3_desc desc;
	struct ipa_mem_buffer mem;
	struct ipahal_imm_cmd_ip_v6_filter_init v6_cmd;
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	int rc;

	rc = ipahal_flt_generate_empty_img(ipa3_ctx->ep_flt_num,
		IPA_MEM_PART(v6_flt_hash_size),
		IPA_MEM_PART(v6_flt_nhash_size), ipa3_ctx->ep_flt_bitmap,
		&mem, false);
	if (rc) {
		IPAERR("fail generate empty v6 flt img\n");
		return rc;
	}

	/*
	 * SRAM memory not allocated to hash tables. Initializing/Sending
	 * command to hash tables(filer/routing) operation not supported.
	 */
	if (ipa3_ctx->ipa_fltrt_not_hashable) {
		v6_cmd.hash_rules_addr = 0;
		v6_cmd.hash_rules_size = 0;
		v6_cmd.hash_local_addr = 0;
	} else {
		v6_cmd.hash_rules_addr = mem.phys_base;
		v6_cmd.hash_rules_size = mem.size;
		v6_cmd.hash_local_addr = ipa3_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v6_flt_hash_ofst);
	}

	v6_cmd.nhash_rules_addr = mem.phys_base;
	v6_cmd.nhash_rules_size = mem.size;
	v6_cmd.nhash_local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v6_flt_nhash_ofst);
	IPADBG("putting hashable filtering IPv6 rules to phys 0x%x\n",
				v6_cmd.hash_local_addr);
	IPADBG("putting non-hashable filtering IPv6 rules to phys 0x%x\n",
				v6_cmd.nhash_local_addr);

	cmd_pyld = ipahal_construct_imm_cmd(
		IPA_IMM_CMD_IP_V6_FILTER_INIT, &v6_cmd, false);
	if (!cmd_pyld) {
		IPAERR("fail construct ip_v6_flt_init imm cmd\n");
		rc = -EPERM;
		goto free_mem;
	}

	ipa3_init_imm_cmd_desc(&desc, cmd_pyld);
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa3_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	ipahal_destroy_imm_cmd(cmd_pyld);

free_mem:
	ipahal_free_dma_mem(&mem);
	return rc;
}

static int ipa3_setup_flt_hash_tuple(void)
{
	int pipe_idx;
	struct ipahal_reg_hash_tuple tuple;

	memset(&tuple, 0, sizeof(struct ipahal_reg_hash_tuple));

	for (pipe_idx = 0; pipe_idx < ipa3_ctx->ipa_num_pipes ; pipe_idx++) {
		if (!ipa_is_ep_support_flt(pipe_idx))
			continue;

		if (ipa_is_modem_pipe(pipe_idx))
			continue;

		if (ipa3_set_flt_tuple_mask(pipe_idx, &tuple)) {
			IPAERR("failed to setup pipe %d flt tuple\n", pipe_idx);
			return -EFAULT;
		}
	}

	return 0;
}

static int ipa3_setup_rt_hash_tuple(void)
{
	int tbl_idx;
	struct ipahal_reg_hash_tuple tuple;

	memset(&tuple, 0, sizeof(struct ipahal_reg_hash_tuple));

	for (tbl_idx = 0;
		tbl_idx < max(IPA_MEM_PART(v6_rt_num_index),
		IPA_MEM_PART(v4_rt_num_index));
		tbl_idx++) {

		if (tbl_idx >= IPA_MEM_PART(v4_modem_rt_index_lo) &&
			tbl_idx <= IPA_MEM_PART(v4_modem_rt_index_hi))
			continue;

		if (tbl_idx >= IPA_MEM_PART(v6_modem_rt_index_lo) &&
			tbl_idx <= IPA_MEM_PART(v6_modem_rt_index_hi))
			continue;

		if (ipa3_set_rt_tuple_mask(tbl_idx, &tuple)) {
			IPAERR("failed to setup tbl %d rt tuple\n", tbl_idx);
			return -EFAULT;
		}
	}

	return 0;
}

static int ipa3_setup_apps_pipes(void)
{
	struct ipa_sys_connect_params sys_in;
	int result = 0;

	if (ipa3_ctx->gsi_ch20_wa) {
		IPADBG("Allocating GSI physical channel 20\n");
		result = ipa_gsi_ch20_wa();
		if (result) {
			IPAERR("ipa_gsi_ch20_wa failed %d\n", result);
			goto fail_ch20_wa;
		}
	}

	/* allocate the common PROD event ring */
	if (ipa3_alloc_common_event_ring()) {
		IPAERR("ipa3_alloc_common_event_ring failed.\n");
		result = -EPERM;
		goto fail_ch20_wa;
	}

	/* CMD OUT (AP->IPA) */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_APPS_CMD_PROD;
	sys_in.desc_fifo_sz = IPA_SYS_DESC_FIFO_SZ;
	sys_in.ipa_ep_cfg.mode.mode = IPA_DMA;
	sys_in.ipa_ep_cfg.mode.dst = IPA_CLIENT_APPS_LAN_CONS;
	if (ipa3_setup_sys_pipe(&sys_in, &ipa3_ctx->clnt_hdl_cmd)) {
		IPAERR(":setup sys pipe (APPS_CMD_PROD) failed.\n");
		result = -EPERM;
		goto fail_ch20_wa;
	}
	IPADBG("Apps to IPA cmd pipe is connected\n");

	IPADBG("Will initialize SRAM\n");
	ipa3_ctx->ctrl->ipa_init_sram();
	IPADBG("SRAM initialized\n");

	IPADBG("Will initialize HDR\n");
	ipa3_ctx->ctrl->ipa_init_hdr();
	IPADBG("HDR initialized\n");

	IPADBG("Will initialize V4 RT\n");
	ipa3_ctx->ctrl->ipa_init_rt4();
	IPADBG("V4 RT initialized\n");

	IPADBG("Will initialize V6 RT\n");
	ipa3_ctx->ctrl->ipa_init_rt6();
	IPADBG("V6 RT initialized\n");

	IPADBG("Will initialize V4 FLT\n");
	ipa3_ctx->ctrl->ipa_init_flt4();
	IPADBG("V4 FLT initialized\n");

	IPADBG("Will initialize V6 FLT\n");
	ipa3_ctx->ctrl->ipa_init_flt6();
	IPADBG("V6 FLT initialized\n");

	if (!ipa3_ctx->ipa_fltrt_not_hashable) {
		if (ipa3_setup_flt_hash_tuple()) {
			IPAERR(":fail to configure flt hash tuple\n");
			result = -EPERM;
			goto fail_flt_hash_tuple;
		}
		IPADBG("flt hash tuple is configured\n");

		if (ipa3_setup_rt_hash_tuple()) {
			IPAERR(":fail to configure rt hash tuple\n");
			result = -EPERM;
			goto fail_flt_hash_tuple;
		}
		IPADBG("rt hash tuple is configured\n");
	}
	if (ipa3_setup_exception_path()) {
		IPAERR(":fail to setup excp path\n");
		result = -EPERM;
		goto fail_flt_hash_tuple;
	}
	IPADBG("Exception path was successfully set");

	if (ipa3_setup_dflt_rt_tables()) {
		IPAERR(":fail to setup dflt routes\n");
		result = -EPERM;
		goto fail_flt_hash_tuple;
	}
	IPADBG("default routing was set\n");

	/* LAN IN (IPA->AP) */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_APPS_LAN_CONS;
	sys_in.desc_fifo_sz = IPA_SYS_DESC_FIFO_SZ;
	sys_in.notify = ipa3_lan_rx_cb;
	sys_in.priv = NULL;
	sys_in.ipa_ep_cfg.hdr.hdr_len = IPA_LAN_RX_HEADER_LENGTH;
	sys_in.ipa_ep_cfg.hdr_ext.hdr_little_endian = false;
	sys_in.ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad_valid = true;
	sys_in.ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad = IPA_HDR_PAD;
	sys_in.ipa_ep_cfg.hdr_ext.hdr_payload_len_inc_padding = false;
	sys_in.ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad_offset = 0;
	sys_in.ipa_ep_cfg.hdr_ext.hdr_pad_to_alignment = 2;
	sys_in.ipa_ep_cfg.cfg.cs_offload_en = IPA_DISABLE_CS_OFFLOAD;

	/**
	 * ipa_lan_rx_cb() intended to notify the source EP about packet
	 * being received on the LAN_CONS via calling the source EP call-back.
	 * There could be a race condition with calling this call-back. Other
	 * thread may nullify it - e.g. on EP disconnect.
	 * This lock intended to protect the access to the source EP call-back
	 */
	spin_lock_init(&ipa3_ctx->disconnect_lock);
	if (ipa3_setup_sys_pipe(&sys_in, &ipa3_ctx->clnt_hdl_data_in)) {
		IPAERR(":setup sys pipe (LAN_CONS) failed.\n");
		result = -EPERM;
		goto fail_flt_hash_tuple;
	}

	/* LAN OUT (AP->IPA) */
	if (!ipa3_ctx->ipa_config_is_mhi) {
		memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
		sys_in.client = IPA_CLIENT_APPS_LAN_PROD;
		sys_in.desc_fifo_sz = IPA_SYS_TX_DATA_DESC_FIFO_SZ;
		sys_in.ipa_ep_cfg.mode.mode = IPA_BASIC;
		if (ipa3_setup_sys_pipe(&sys_in,
			&ipa3_ctx->clnt_hdl_data_out)) {
			IPAERR(":setup sys pipe (LAN_PROD) failed.\n");
			result = -EPERM;
			goto fail_lan_data_out;
		}
	}

	return 0;

fail_lan_data_out:
	ipa3_teardown_sys_pipe(ipa3_ctx->clnt_hdl_data_in);
fail_flt_hash_tuple:
	if (ipa3_ctx->dflt_v6_rt_rule_hdl)
		__ipa3_del_rt_rule(ipa3_ctx->dflt_v6_rt_rule_hdl);
	if (ipa3_ctx->dflt_v4_rt_rule_hdl)
		__ipa3_del_rt_rule(ipa3_ctx->dflt_v4_rt_rule_hdl);
	if (ipa3_ctx->excp_hdr_hdl)
		__ipa3_del_hdr(ipa3_ctx->excp_hdr_hdl, false);
	ipa3_teardown_sys_pipe(ipa3_ctx->clnt_hdl_cmd);
fail_ch20_wa:
	return result;
}

static void ipa3_teardown_apps_pipes(void)
{
	if (!ipa3_ctx->ipa_config_is_mhi)
		ipa3_teardown_sys_pipe(ipa3_ctx->clnt_hdl_data_out);
	ipa3_teardown_sys_pipe(ipa3_ctx->clnt_hdl_data_in);
	__ipa3_del_rt_rule(ipa3_ctx->dflt_v6_rt_rule_hdl);
	__ipa3_del_rt_rule(ipa3_ctx->dflt_v4_rt_rule_hdl);
	__ipa3_del_hdr(ipa3_ctx->excp_hdr_hdl, false);
	ipa3_teardown_sys_pipe(ipa3_ctx->clnt_hdl_cmd);
}

#ifdef CONFIG_COMPAT

static long compat_ipa3_nat_ipv6ct_alloc_table(unsigned long arg,
	int (alloc_func)(struct ipa_ioc_nat_ipv6ct_table_alloc *))
{
	long retval;
	struct ipa_ioc_nat_ipv6ct_table_alloc32 table_alloc32;
	struct ipa_ioc_nat_ipv6ct_table_alloc table_alloc;

	retval = copy_from_user(&table_alloc32, (const void __user *)arg,
		sizeof(struct ipa_ioc_nat_ipv6ct_table_alloc32));
	if (retval)
		return retval;

	table_alloc.size = (size_t)table_alloc32.size;
	table_alloc.offset = (off_t)table_alloc32.offset;

	retval = alloc_func(&table_alloc);
	if (retval)
		return retval;

	if (table_alloc.offset) {
		table_alloc32.offset = (compat_off_t)table_alloc.offset;
		retval = copy_to_user((void __user *)arg, &table_alloc32,
			sizeof(struct ipa_ioc_nat_ipv6ct_table_alloc32));
	}

	return retval;
}

long compat_ipa3_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long retval = 0;
	struct ipa3_ioc_nat_alloc_mem32 nat_mem32;
	struct ipa_ioc_nat_alloc_mem nat_mem;

	switch (cmd) {
	case IPA_IOC_ADD_HDR32:
		cmd = IPA_IOC_ADD_HDR;
		break;
	case IPA_IOC_DEL_HDR32:
		cmd = IPA_IOC_DEL_HDR;
		break;
	case IPA_IOC_ADD_RT_RULE32:
		cmd = IPA_IOC_ADD_RT_RULE;
		break;
	case IPA_IOC_DEL_RT_RULE32:
		cmd = IPA_IOC_DEL_RT_RULE;
		break;
	case IPA_IOC_ADD_FLT_RULE32:
		cmd = IPA_IOC_ADD_FLT_RULE;
		break;
	case IPA_IOC_DEL_FLT_RULE32:
		cmd = IPA_IOC_DEL_FLT_RULE;
		break;
	case IPA_IOC_GET_RT_TBL32:
		cmd = IPA_IOC_GET_RT_TBL;
		break;
	case IPA_IOC_COPY_HDR32:
		cmd = IPA_IOC_COPY_HDR;
		break;
	case IPA_IOC_QUERY_INTF32:
		cmd = IPA_IOC_QUERY_INTF;
		break;
	case IPA_IOC_QUERY_INTF_TX_PROPS32:
		cmd = IPA_IOC_QUERY_INTF_TX_PROPS;
		break;
	case IPA_IOC_QUERY_INTF_RX_PROPS32:
		cmd = IPA_IOC_QUERY_INTF_RX_PROPS;
		break;
	case IPA_IOC_QUERY_INTF_EXT_PROPS32:
		cmd = IPA_IOC_QUERY_INTF_EXT_PROPS;
		break;
	case IPA_IOC_GET_HDR32:
		cmd = IPA_IOC_GET_HDR;
		break;
	case IPA_IOC_ALLOC_NAT_MEM32:
		retval = copy_from_user(&nat_mem32, (const void __user *)arg,
			sizeof(struct ipa3_ioc_nat_alloc_mem32));
		if (retval)
			return retval;
		memcpy(nat_mem.dev_name, nat_mem32.dev_name,
				IPA_RESOURCE_NAME_MAX);
		nat_mem.size = (size_t)nat_mem32.size;
		nat_mem.offset = (off_t)nat_mem32.offset;

		/* null terminate the string */
		nat_mem.dev_name[IPA_RESOURCE_NAME_MAX - 1] = '\0';

		retval = ipa3_allocate_nat_device(&nat_mem);
		if (retval)
			return retval;
		nat_mem32.offset = (compat_off_t)nat_mem.offset;
		retval = copy_to_user((void __user *)arg, &nat_mem32,
			sizeof(struct ipa3_ioc_nat_alloc_mem32));
		return retval;
	case IPA_IOC_ALLOC_NAT_TABLE32:
		return compat_ipa3_nat_ipv6ct_alloc_table(arg,
			ipa3_allocate_nat_table);
	case IPA_IOC_ALLOC_IPV6CT_TABLE32:
		return compat_ipa3_nat_ipv6ct_alloc_table(arg,
			ipa3_allocate_ipv6ct_table);
	case IPA_IOC_V4_INIT_NAT32:
		cmd = IPA_IOC_V4_INIT_NAT;
		break;
	case IPA_IOC_INIT_IPV6CT_TABLE32:
		cmd = IPA_IOC_INIT_IPV6CT_TABLE;
		break;
	case IPA_IOC_TABLE_DMA_CMD32:
		cmd = IPA_IOC_TABLE_DMA_CMD;
		break;
	case IPA_IOC_V4_DEL_NAT32:
		cmd = IPA_IOC_V4_DEL_NAT;
		break;
	case IPA_IOC_DEL_NAT_TABLE32:
		cmd = IPA_IOC_DEL_NAT_TABLE;
		break;
	case IPA_IOC_DEL_IPV6CT_TABLE32:
		cmd = IPA_IOC_DEL_IPV6CT_TABLE;
		break;
	case IPA_IOC_NAT_MODIFY_PDN32:
		cmd = IPA_IOC_NAT_MODIFY_PDN;
		break;
	case IPA_IOC_GET_NAT_OFFSET32:
		cmd = IPA_IOC_GET_NAT_OFFSET;
		break;
	case IPA_IOC_PULL_MSG32:
		cmd = IPA_IOC_PULL_MSG;
		break;
	case IPA_IOC_RM_ADD_DEPENDENCY32:
		cmd = IPA_IOC_RM_ADD_DEPENDENCY;
		break;
	case IPA_IOC_RM_DEL_DEPENDENCY32:
		cmd = IPA_IOC_RM_DEL_DEPENDENCY;
		break;
	case IPA_IOC_GENERATE_FLT_EQ32:
		cmd = IPA_IOC_GENERATE_FLT_EQ;
		break;
	case IPA_IOC_QUERY_RT_TBL_INDEX32:
		cmd = IPA_IOC_QUERY_RT_TBL_INDEX;
		break;
	case IPA_IOC_WRITE_QMAPID32:
		cmd = IPA_IOC_WRITE_QMAPID;
		break;
	case IPA_IOC_MDFY_FLT_RULE32:
		cmd = IPA_IOC_MDFY_FLT_RULE;
		break;
	case IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_ADD32:
		cmd = IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_ADD;
		break;
	case IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_DEL32:
		cmd = IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_DEL;
		break;
	case IPA_IOC_NOTIFY_WAN_EMBMS_CONNECTED32:
		cmd = IPA_IOC_NOTIFY_WAN_EMBMS_CONNECTED;
		break;
	case IPA_IOC_MDFY_RT_RULE32:
		cmd = IPA_IOC_MDFY_RT_RULE;
		break;
	case IPA_IOC_GET_NAT_IN_SRAM_INFO32:
		cmd = IPA_IOC_GET_NAT_IN_SRAM_INFO;
		break;
	case IPA_IOC_APP_CLOCK_VOTE32:
		cmd = IPA_IOC_APP_CLOCK_VOTE;
		break;
	case IPA_IOC_COMMIT_HDR:
	case IPA_IOC_RESET_HDR:
	case IPA_IOC_COMMIT_RT:
	case IPA_IOC_RESET_RT:
	case IPA_IOC_COMMIT_FLT:
	case IPA_IOC_RESET_FLT:
	case IPA_IOC_DUMP:
	case IPA_IOC_PUT_RT_TBL:
	case IPA_IOC_PUT_HDR:
	case IPA_IOC_SET_FLT:
	case IPA_IOC_QUERY_EP_MAPPING:
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return ipa3_ioctl(file, cmd, (unsigned long) compat_ptr(arg));
}
#endif

static ssize_t ipa3_write(struct file *file, const char __user *buf,
			  size_t count, loff_t *ppos);

static const struct file_operations ipa3_drv_fops = {
	.owner = THIS_MODULE,
	.open = ipa3_open,
	.read = ipa3_read,
	.write = ipa3_write,
	.unlocked_ioctl = ipa3_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_ipa3_ioctl,
#endif
};

static int ipa3_get_clks(struct device *dev)
{
	if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_NORMAL) {
		IPADBG("not supported in this HW mode\n");
		ipa3_clk = NULL;
		return 0;
	}

	if (ipa3_res.use_bw_vote) {
		IPADBG("Vote IPA clock by bw voting via bus scaling driver\n");
		ipa3_clk = NULL;
		return 0;
	}

	ipa3_clk = clk_get(dev, "core_clk");
	if (IS_ERR(ipa3_clk)) {
		if (ipa3_clk != ERR_PTR(-EPROBE_DEFER))
			IPAERR("fail to get ipa clk\n");
		return PTR_ERR(ipa3_clk);
	}
	return 0;
}

/**
 * _ipa_enable_clks_v3_0() - Enable IPA clocks.
 */
void _ipa_enable_clks_v3_0(void)
{
	IPADBG_LOW("curr_ipa_clk_rate=%d", ipa3_ctx->curr_ipa_clk_rate);
	if (ipa3_clk) {
		IPADBG_LOW("enabling gcc_ipa_clk\n");
		clk_prepare(ipa3_clk);
		clk_enable(ipa3_clk);
		clk_set_rate(ipa3_clk, ipa3_ctx->curr_ipa_clk_rate);
	}

	ipa3_uc_notify_clk_state(true);
}

static unsigned int ipa3_get_bus_vote(void)
{
	unsigned int idx = 1;

	if (ipa3_ctx->curr_ipa_clk_rate == ipa3_ctx->ctrl->ipa_clk_rate_svs2) {
		idx = 1;
	} else if (ipa3_ctx->curr_ipa_clk_rate ==
		ipa3_ctx->ctrl->ipa_clk_rate_svs) {
		idx = 2;
	} else if (ipa3_ctx->curr_ipa_clk_rate ==
		ipa3_ctx->ctrl->ipa_clk_rate_nominal) {
		idx = 3;
	} else if (ipa3_ctx->curr_ipa_clk_rate ==
			ipa3_ctx->ctrl->ipa_clk_rate_turbo) {
		idx = ipa3_ctx->ctrl->msm_bus_data_ptr->num_usecases - 1;
	} else {
		WARN(1, "unexpected clock rate");
	}
	IPADBG_LOW("curr %d idx %d\n", ipa3_ctx->curr_ipa_clk_rate, idx);

	return idx;
}

/**
 * ipa3_enable_clks() - Turn on IPA clocks
 *
 * Return codes:
 * None
 */
void ipa3_enable_clks(void)
{
	if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_NORMAL) {
		IPAERR("not supported in this mode\n");
		return;
	}

	IPADBG("enabling IPA clocks and bus voting\n");

	if (msm_bus_scale_client_update_request(ipa3_ctx->ipa_bus_hdl,
	    ipa3_get_bus_vote()))
		WARN(1, "bus scaling failed");
	ipa3_ctx->ctrl->ipa3_enable_clks();
	atomic_set(&ipa3_ctx->ipa_clk_vote, 1);
}


/**
 * _ipa_disable_clks_v3_0() - Disable IPA clocks.
 */
void _ipa_disable_clks_v3_0(void)
{
	ipa3_uc_notify_clk_state(false);
	if (ipa3_clk) {
		IPADBG_LOW("disabling gcc_ipa_clk\n");
		clk_disable_unprepare(ipa3_clk);
	}
}

/**
 * ipa3_disable_clks() - Turn off IPA clocks
 *
 * Return codes:
 * None
 */
void ipa3_disable_clks(void)
{
	if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_NORMAL) {
		IPAERR("not supported in this mode\n");
		return;
	}

	IPADBG("disabling IPA clocks and bus voting\n");

	ipa3_ctx->ctrl->ipa3_disable_clks();

	if (ipa3_ctx->use_ipa_pm)
		ipa_pm_set_clock_index(0);

	if (msm_bus_scale_client_update_request(ipa3_ctx->ipa_bus_hdl, 0))
		WARN(1, "bus scaling failed");
	atomic_set(&ipa3_ctx->ipa_clk_vote, 0);
}

/**
 * ipa3_start_tag_process() - Send TAG packet and wait for it to come back
 *
 * This function is called prior to clock gating when active client counter
 * is 1. TAG process ensures that there are no packets inside IPA HW that
 * were not submitted to the IPA client via the transport. During TAG process
 * all aggregation frames are (force) closed.
 *
 * Return codes:
 * None
 */
static void ipa3_start_tag_process(struct work_struct *work)
{
	int res;

	IPADBG("starting TAG process\n");
	/* close aggregation frames on all pipes */
	res = ipa3_tag_aggr_force_close(-1);
	if (res)
		IPAERR("ipa3_tag_aggr_force_close failed %d\n", res);
	IPA_ACTIVE_CLIENTS_DEC_SPECIAL("TAG_PROCESS");

	IPADBG("TAG process done\n");
}

/**
 * ipa3_active_clients_log_mod() - Log a modification in the active clients
 * reference count
 *
 * This method logs any modification in the active clients reference count:
 * It logs the modification in the circular history buffer
 * It logs the modification in the hash table - looking for an entry,
 * creating one if needed and deleting one if needed.
 *
 * @id: ipa3_active client logging info struct to hold the log information
 * @inc: a boolean variable to indicate whether the modification is an increase
 * or decrease
 * @int_ctx: a boolean variable to indicate whether this call is being made from
 * an interrupt context and therefore should allocate GFP_ATOMIC memory
 *
 * Method process:
 * - Hash the unique identifier string
 * - Find the hash in the table
 *    1)If found, increase or decrease the reference count
 *    2)If not found, allocate a new hash table entry struct and initialize it
 * - Remove and deallocate unneeded data structure
 * - Log the call in the circular history buffer (unless it is a simple call)
 */
#ifdef CONFIG_IPA_DEBUG
static void ipa3_active_clients_log_mod(
		struct ipa_active_client_logging_info *id,
		bool inc, bool int_ctx)
{
	char temp_str[IPA3_ACTIVE_CLIENTS_LOG_LINE_LEN];
	unsigned long long t;
	unsigned long nanosec_rem;
	struct ipa3_active_client_htable_entry *hentry;
	struct ipa3_active_client_htable_entry *hfound;
	u32 hkey;
	char str_to_hash[IPA3_ACTIVE_CLIENTS_LOG_NAME_LEN];
	unsigned long flags;

	spin_lock_irqsave(&ipa3_ctx->ipa3_active_clients_logging.lock, flags);
	int_ctx = true;
	hfound = NULL;
	memset(str_to_hash, 0, IPA3_ACTIVE_CLIENTS_LOG_NAME_LEN);
	strlcpy(str_to_hash, id->id_string, IPA3_ACTIVE_CLIENTS_LOG_NAME_LEN);
	hkey = jhash(str_to_hash, IPA3_ACTIVE_CLIENTS_LOG_NAME_LEN,
			0);
	hash_for_each_possible(ipa3_ctx->ipa3_active_clients_logging.htable,
			hentry, list, hkey) {
		if (!strcmp(hentry->id_string, id->id_string)) {
			hentry->count = hentry->count + (inc ? 1 : -1);
			hfound = hentry;
		}
	}
	if (hfound == NULL) {
		hentry = NULL;
		hentry = kzalloc(sizeof(
				struct ipa3_active_client_htable_entry),
				int_ctx ? GFP_ATOMIC : GFP_KERNEL);
		if (hentry == NULL) {
			spin_unlock_irqrestore(
				&ipa3_ctx->ipa3_active_clients_logging.lock,
				flags);
			return;
		}
		hentry->type = id->type;
		strlcpy(hentry->id_string, id->id_string,
				IPA3_ACTIVE_CLIENTS_LOG_NAME_LEN);
		INIT_HLIST_NODE(&hentry->list);
		hentry->count = inc ? 1 : -1;
		hash_add(ipa3_ctx->ipa3_active_clients_logging.htable,
				&hentry->list, hkey);
	} else if (hfound->count == 0) {
		hash_del(&hfound->list);
		kfree(hfound);
	}

	if (id->type != SIMPLE) {
		t = local_clock();
		nanosec_rem = do_div(t, 1000000000) / 1000;
		snprintf(temp_str, IPA3_ACTIVE_CLIENTS_LOG_LINE_LEN,
				inc ? "[%5lu.%06lu] ^ %s, %s: %d" :
						"[%5lu.%06lu] v %s, %s: %d",
				(unsigned long)t, nanosec_rem,
				id->id_string, id->file, id->line);
		ipa3_active_clients_log_insert(temp_str);
	}
	spin_unlock_irqrestore(&ipa3_ctx->ipa3_active_clients_logging.lock,
		flags);
}
#else
static void ipa3_active_clients_log_mod(
		struct ipa_active_client_logging_info *id,
		bool inc, bool int_ctx)
{
}
#endif

void ipa3_active_clients_log_dec(struct ipa_active_client_logging_info *id,
		bool int_ctx)
{
	ipa3_active_clients_log_mod(id, false, int_ctx);
}

void ipa3_active_clients_log_inc(struct ipa_active_client_logging_info *id,
		bool int_ctx)
{
	ipa3_active_clients_log_mod(id, true, int_ctx);
}

/**
 * ipa3_inc_client_enable_clks() - Increase active clients counter, and
 * enable ipa clocks if necessary
 *
 * Return codes:
 * None
 */
void ipa3_inc_client_enable_clks(struct ipa_active_client_logging_info *id)
{
	int ret;

	ipa3_active_clients_log_inc(id, false);
	ret = atomic_inc_not_zero(&ipa3_ctx->ipa3_active_clients.cnt);
	if (ret) {
		IPADBG_LOW("active clients = %d\n",
			atomic_read(&ipa3_ctx->ipa3_active_clients.cnt));
		return;
	}

	mutex_lock(&ipa3_ctx->ipa3_active_clients.mutex);

	/* somebody might voted to clocks meanwhile */
	ret = atomic_inc_not_zero(&ipa3_ctx->ipa3_active_clients.cnt);
	if (ret) {
		mutex_unlock(&ipa3_ctx->ipa3_active_clients.mutex);
		IPADBG_LOW("active clients = %d\n",
			atomic_read(&ipa3_ctx->ipa3_active_clients.cnt));
		return;
	}

	ipa3_enable_clks();
	IPADBG_LOW("active clients = %d\n",
		atomic_read(&ipa3_ctx->ipa3_active_clients.cnt));
	ipa3_suspend_apps_pipes(false);
	atomic_inc(&ipa3_ctx->ipa3_active_clients.cnt);
	if (!ipa3_uc_state_check() &&
		(ipa3_ctx->ipa_hw_type == IPA_HW_v4_1)) {
		ipa3_read_mailbox_17(IPA_PC_RESTORE_CONTEXT_STATUS_SUCCESS);
		/* assert if intset = 0 */
		if (ipa3_ctx->gsi_chk_intset_value == 0) {
			IPAERR("expected 1, value: 0\n");
			ipa_assert();
		}
	}
	mutex_unlock(&ipa3_ctx->ipa3_active_clients.mutex);
}

/**
 * ipa3_active_clks_status() - update the current msm bus clock vote
 * status
 */
int ipa3_active_clks_status(void)
{
	return atomic_read(&ipa3_ctx->ipa_clk_vote);
}

/**
 * ipa3_inc_client_enable_clks_no_block() - Only increment the number of active
 * clients if no asynchronous actions should be done. Asynchronous actions are
 * locking a mutex and waking up IPA HW.
 *
 * Return codes: 0 for success
 *		-EPERM if an asynchronous action should have been done
 */
int ipa3_inc_client_enable_clks_no_block(struct ipa_active_client_logging_info
		*id)
{
	int ret;

	ret = atomic_inc_not_zero(&ipa3_ctx->ipa3_active_clients.cnt);
	if (ret) {
		ipa3_active_clients_log_inc(id, true);
		IPADBG_LOW("active clients = %d\n",
			atomic_read(&ipa3_ctx->ipa3_active_clients.cnt));
		return 0;
	}

	return -EPERM;
}

static void __ipa3_dec_client_disable_clks(void)
{
	int ret;

	if (!atomic_read(&ipa3_ctx->ipa3_active_clients.cnt)) {
		IPAERR("trying to disable clocks with refcnt is 0\n");
		ipa_assert();
		return;
	}

	ret = atomic_add_unless(&ipa3_ctx->ipa3_active_clients.cnt, -1, 1);
	if (ret)
		goto bail;

	/* seems like this is the only client holding the clocks */
	mutex_lock(&ipa3_ctx->ipa3_active_clients.mutex);
	if (atomic_read(&ipa3_ctx->ipa3_active_clients.cnt) == 1 &&
	    ipa3_ctx->tag_process_before_gating) {
		ipa3_ctx->tag_process_before_gating = false;
		/*
		 * When TAG process ends, active clients will be
		 * decreased
		 */
		queue_work(ipa3_ctx->power_mgmt_wq, &ipa3_tag_work);
		goto unlock_mutex;
	}

	/* a different context might increase the clock reference meanwhile */
	ret = atomic_sub_return(1, &ipa3_ctx->ipa3_active_clients.cnt);
	if (ret > 0)
		goto unlock_mutex;
	ipa3_suspend_apps_pipes(true);
	ipa3_disable_clks();

unlock_mutex:
	mutex_unlock(&ipa3_ctx->ipa3_active_clients.mutex);
bail:
	IPADBG_LOW("active clients = %d\n",
		atomic_read(&ipa3_ctx->ipa3_active_clients.cnt));
}

/**
 * ipa3_dec_client_disable_clks() - Decrease active clients counter
 *
 * In case that there are no active clients this function also starts
 * TAG process. When TAG progress ends ipa clocks will be gated.
 * start_tag_process_again flag is set during this function to signal TAG
 * process to start again as there was another client that may send data to ipa
 *
 * Return codes:
 * None
 */
void ipa3_dec_client_disable_clks(struct ipa_active_client_logging_info *id)
{
	ipa3_active_clients_log_dec(id, false);
	__ipa3_dec_client_disable_clks();
}

static void ipa_dec_clients_disable_clks_on_wq(struct work_struct *work)
{
	__ipa3_dec_client_disable_clks();
}

/**
 * ipa3_dec_client_disable_clks_no_block() - Decrease active clients counter
 * if possible without blocking. If this is the last client then the desrease
 * will happen from work queue context.
 *
 * Return codes:
 * None
 */
void ipa3_dec_client_disable_clks_no_block(
	struct ipa_active_client_logging_info *id)
{
	int ret;

	ipa3_active_clients_log_dec(id, true);
	ret = atomic_add_unless(&ipa3_ctx->ipa3_active_clients.cnt, -1, 1);
	if (ret) {
		IPADBG_LOW("active clients = %d\n",
			atomic_read(&ipa3_ctx->ipa3_active_clients.cnt));
		return;
	}

	/* seems like this is the only client holding the clocks */
	queue_work(ipa3_ctx->power_mgmt_wq,
		&ipa_dec_clients_disable_clks_on_wq_work);
}

/**
 * ipa3_inc_acquire_wakelock() - Increase active clients counter, and
 * acquire wakelock if necessary
 *
 * Return codes:
 * None
 */
void ipa3_inc_acquire_wakelock(void)
{
	unsigned long flags;

	spin_lock_irqsave(&ipa3_ctx->wakelock_ref_cnt.spinlock, flags);
	ipa3_ctx->wakelock_ref_cnt.cnt++;
	if (ipa3_ctx->wakelock_ref_cnt.cnt == 1)
		__pm_stay_awake(&ipa3_ctx->w_lock);
	IPADBG_LOW("active wakelock ref cnt = %d\n",
		ipa3_ctx->wakelock_ref_cnt.cnt);
	spin_unlock_irqrestore(&ipa3_ctx->wakelock_ref_cnt.spinlock, flags);
}

/**
 * ipa3_dec_release_wakelock() - Decrease active clients counter
 *
 * In case if the ref count is 0, release the wakelock.
 *
 * Return codes:
 * None
 */
void ipa3_dec_release_wakelock(void)
{
	unsigned long flags;

	spin_lock_irqsave(&ipa3_ctx->wakelock_ref_cnt.spinlock, flags);
	ipa3_ctx->wakelock_ref_cnt.cnt--;
	IPADBG_LOW("active wakelock ref cnt = %d\n",
		ipa3_ctx->wakelock_ref_cnt.cnt);
	if (ipa3_ctx->wakelock_ref_cnt.cnt == 0)
		__pm_relax(&ipa3_ctx->w_lock);
	spin_unlock_irqrestore(&ipa3_ctx->wakelock_ref_cnt.spinlock, flags);
}

int ipa3_set_clock_plan_from_pm(int idx)
{
	u32 clk_rate;

	IPADBG_LOW("idx = %d\n", idx);

	if (!ipa3_ctx->enable_clock_scaling) {
		ipa3_ctx->ipa3_active_clients.bus_vote_idx = idx;
		return 0;
	}

	if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_NORMAL) {
		IPAERR("not supported in this mode\n");
		return 0;
	}

	if (idx <= 0 || idx >= ipa3_ctx->ctrl->msm_bus_data_ptr->num_usecases) {
		IPAERR("bad voltage\n");
		return -EINVAL;
	}

	if (idx == 1)
		clk_rate = ipa3_ctx->ctrl->ipa_clk_rate_svs2;
	else if (idx == 2)
		clk_rate = ipa3_ctx->ctrl->ipa_clk_rate_svs;
	else if (idx == 3)
		clk_rate = ipa3_ctx->ctrl->ipa_clk_rate_nominal;
	else if (idx == 4)
		clk_rate = ipa3_ctx->ctrl->ipa_clk_rate_turbo;
	else {
		IPAERR("bad voltage\n");
		WARN_ON(1);
		return -EFAULT;
	}

	if (clk_rate == ipa3_ctx->curr_ipa_clk_rate) {
		IPADBG_LOW("Same voltage\n");
		return 0;
	}

	mutex_lock(&ipa3_ctx->ipa3_active_clients.mutex);
	ipa3_ctx->curr_ipa_clk_rate = clk_rate;
	ipa3_ctx->ipa3_active_clients.bus_vote_idx = idx;
	IPADBG_LOW("setting clock rate to %u\n", ipa3_ctx->curr_ipa_clk_rate);
	if (atomic_read(&ipa3_ctx->ipa3_active_clients.cnt) > 0) {
		if (ipa3_clk)
			clk_set_rate(ipa3_clk, ipa3_ctx->curr_ipa_clk_rate);
		if (msm_bus_scale_client_update_request(ipa3_ctx->ipa_bus_hdl,
				ipa3_get_bus_vote()))
			WARN_ON(1);
	} else {
		IPADBG_LOW("clocks are gated, not setting rate\n");
	}
	mutex_unlock(&ipa3_ctx->ipa3_active_clients.mutex);
	IPADBG_LOW("Done\n");

	return 0;
}

int ipa3_set_required_perf_profile(enum ipa_voltage_level floor_voltage,
				  u32 bandwidth_mbps)
{
	enum ipa_voltage_level needed_voltage;
	u32 clk_rate;

	if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_NORMAL) {
		IPAERR("not supported in this mode\n");
		return 0;
	}

	IPADBG_LOW("floor_voltage=%d, bandwidth_mbps=%u",
					floor_voltage, bandwidth_mbps);

	if (floor_voltage < IPA_VOLTAGE_UNSPECIFIED ||
		floor_voltage >= IPA_VOLTAGE_MAX) {
		IPAERR("bad voltage\n");
		return -EINVAL;
	}

	if (ipa3_ctx->enable_clock_scaling) {
		IPADBG_LOW("Clock scaling is enabled\n");
		if (bandwidth_mbps >=
			ipa3_ctx->ctrl->clock_scaling_bw_threshold_turbo)
			needed_voltage = IPA_VOLTAGE_TURBO;
		else if (bandwidth_mbps >=
			ipa3_ctx->ctrl->clock_scaling_bw_threshold_nominal)
			needed_voltage = IPA_VOLTAGE_NOMINAL;
		else if (bandwidth_mbps >=
			ipa3_ctx->ctrl->clock_scaling_bw_threshold_svs)
			needed_voltage = IPA_VOLTAGE_SVS;
		else
			needed_voltage = IPA_VOLTAGE_SVS2;
	} else {
		IPADBG_LOW("Clock scaling is disabled\n");
		needed_voltage = IPA_VOLTAGE_NOMINAL;
	}

	needed_voltage = max(needed_voltage, floor_voltage);
	switch (needed_voltage) {
	case IPA_VOLTAGE_SVS2:
		clk_rate = ipa3_ctx->ctrl->ipa_clk_rate_svs2;
		break;
	case IPA_VOLTAGE_SVS:
		clk_rate = ipa3_ctx->ctrl->ipa_clk_rate_svs;
		break;
	case IPA_VOLTAGE_NOMINAL:
		clk_rate = ipa3_ctx->ctrl->ipa_clk_rate_nominal;
		break;
	case IPA_VOLTAGE_TURBO:
		clk_rate = ipa3_ctx->ctrl->ipa_clk_rate_turbo;
		break;
	default:
		IPAERR("bad voltage\n");
		WARN_ON(1);
		return -EFAULT;
	}

	if (clk_rate == ipa3_ctx->curr_ipa_clk_rate) {
		IPADBG_LOW("Same voltage\n");
		return 0;
	}

	/* Hold the mutex to avoid race conditions with ipa3_enable_clocks() */
	mutex_lock(&ipa3_ctx->ipa3_active_clients.mutex);
	ipa3_ctx->curr_ipa_clk_rate = clk_rate;
	IPADBG_LOW("setting clock rate to %u\n", ipa3_ctx->curr_ipa_clk_rate);
	if (atomic_read(&ipa3_ctx->ipa3_active_clients.cnt) > 0) {
		if (ipa3_clk)
			clk_set_rate(ipa3_clk, ipa3_ctx->curr_ipa_clk_rate);
		if (msm_bus_scale_client_update_request(ipa3_ctx->ipa_bus_hdl,
				ipa3_get_bus_vote()))
			WARN_ON(1);
	} else {
		IPADBG_LOW("clocks are gated, not setting rate\n");
	}
	mutex_unlock(&ipa3_ctx->ipa3_active_clients.mutex);
	IPADBG_LOW("Done\n");

	return 0;
}

static void ipa3_process_irq_schedule_rel(void)
{
	queue_delayed_work(ipa3_ctx->transport_power_mgmt_wq,
		&ipa3_transport_release_resource_work,
		msecs_to_jiffies(IPA_TRANSPORT_PROD_TIMEOUT_MSEC));
}

/**
 * ipa3_suspend_handler() - Handles the suspend interrupt:
 * wakes up the suspended peripheral by requesting its consumer
 * @interrupt:		Interrupt type
 * @private_data:	The client's private data
 * @interrupt_data:	Interrupt specific information data
 */
void ipa3_suspend_handler(enum ipa_irq_type interrupt,
				void *private_data,
				void *interrupt_data)
{
	enum ipa_rm_resource_name resource;
	u32 suspend_data =
		((struct ipa_tx_suspend_irq_data *)interrupt_data)->endpoints;
	u32 bmsk = 1;
	u32 i = 0;
	int res;
	struct ipa_ep_cfg_holb holb_cfg;
	struct mutex *pm_mutex_ptr = &ipa3_ctx->transport_pm.transport_pm_mutex;
	u32 pipe_bitmask = 0;

	IPADBG("interrupt=%d, interrupt_data=%u\n",
		interrupt, suspend_data);
	memset(&holb_cfg, 0, sizeof(holb_cfg));

	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++, bmsk = bmsk << 1) {
		if ((suspend_data & bmsk) && (ipa3_ctx->ep[i].valid)) {
			if (ipa3_ctx->use_ipa_pm) {
				pipe_bitmask |= bmsk;
				continue;
			}
			if (IPA_CLIENT_IS_APPS_CONS(ipa3_ctx->ep[i].client)) {
				/*
				 * pipe will be unsuspended as part of
				 * enabling IPA clocks
				 */
				mutex_lock(pm_mutex_ptr);
				if (!atomic_read(
					&ipa3_ctx->transport_pm.dec_clients)
					) {
					IPA_ACTIVE_CLIENTS_INC_EP(
						ipa3_ctx->ep[i].client);
					IPADBG_LOW("Pipes un-suspended.\n");
					IPADBG_LOW("Enter poll mode.\n");
					atomic_set(
					&ipa3_ctx->transport_pm.dec_clients,
					1);
					/*
					 * acquire wake lock as long as suspend
					 * vote is held
					 */
					ipa3_inc_acquire_wakelock();
					ipa3_process_irq_schedule_rel();
				}
				mutex_unlock(pm_mutex_ptr);
			} else {
				resource = ipa3_get_rm_resource_from_ep(i);
				res =
				ipa_rm_request_resource_with_timer(resource);
				if (res == -EPERM &&
					IPA_CLIENT_IS_CONS(
					   ipa3_ctx->ep[i].client)) {
					holb_cfg.en = 1;
					res = ipa3_cfg_ep_holb_by_client(
					   ipa3_ctx->ep[i].client, &holb_cfg);
					WARN(res, "holb en failed\n");
				}
			}
		}
	}
	if (ipa3_ctx->use_ipa_pm) {
		res = ipa_pm_handle_suspend(pipe_bitmask);
		if (res) {
			IPAERR("ipa_pm_handle_suspend failed %d\n", res);
			return;
		}
	}
}

/**
 * ipa3_restore_suspend_handler() - restores the original suspend IRQ handler
 * as it was registered in the IPA init sequence.
 * Return codes:
 * 0: success
 * -EPERM: failed to remove current handler or failed to add original handler
 */
int ipa3_restore_suspend_handler(void)
{
	int result = 0;

	result  = ipa3_remove_interrupt_handler(IPA_TX_SUSPEND_IRQ);
	if (result) {
		IPAERR("remove handler for suspend interrupt failed\n");
		return -EPERM;
	}

	result = ipa3_add_interrupt_handler(IPA_TX_SUSPEND_IRQ,
			ipa3_suspend_handler, false, NULL);
	if (result) {
		IPAERR("register handler for suspend interrupt failed\n");
		result = -EPERM;
	}

	IPADBG("suspend handler successfully restored\n");

	return result;
}

static int ipa3_apps_cons_release_resource(void)
{
	return 0;
}

static int ipa3_apps_cons_request_resource(void)
{
	return 0;
}

static void ipa3_transport_release_resource(struct work_struct *work)
{
	mutex_lock(&ipa3_ctx->transport_pm.transport_pm_mutex);
	/* check whether still need to decrease client usage */
	if (atomic_read(&ipa3_ctx->transport_pm.dec_clients)) {
		if (atomic_read(&ipa3_ctx->transport_pm.eot_activity)) {
			IPADBG("EOT pending Re-scheduling\n");
			ipa3_process_irq_schedule_rel();
		} else {
			atomic_set(&ipa3_ctx->transport_pm.dec_clients, 0);
			ipa3_dec_release_wakelock();
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL("TRANSPORT_RESOURCE");
		}
	}
	atomic_set(&ipa3_ctx->transport_pm.eot_activity, 0);
	mutex_unlock(&ipa3_ctx->transport_pm.transport_pm_mutex);
}

int ipa3_create_apps_resource(void)
{
	struct ipa_rm_create_params apps_cons_create_params;
	struct ipa_rm_perf_profile profile;
	int result = 0;

	memset(&apps_cons_create_params, 0,
				sizeof(apps_cons_create_params));
	apps_cons_create_params.name = IPA_RM_RESOURCE_APPS_CONS;
	apps_cons_create_params.request_resource =
		ipa3_apps_cons_request_resource;
	apps_cons_create_params.release_resource =
		ipa3_apps_cons_release_resource;
	result = ipa_rm_create_resource(&apps_cons_create_params);
	if (result) {
		IPAERR("ipa_rm_create_resource failed\n");
		return result;
	}

	profile.max_supported_bandwidth_mbps = IPA_APPS_MAX_BW_IN_MBPS;
	ipa_rm_set_perf_profile(IPA_RM_RESOURCE_APPS_CONS, &profile);

	return result;
}

/**
 * ipa3_init_interrupts() - Register to IPA IRQs
 *
 * Return codes: 0 in success, negative in failure
 *
 */
int ipa3_init_interrupts(void)
{
	int result;

	/*register IPA IRQ handler*/
	result = ipa3_interrupts_init(ipa3_res.ipa_irq, 0,
			&ipa3_ctx->master_pdev->dev);
	if (result) {
		IPAERR("ipa interrupts initialization failed\n");
		return -ENODEV;
	}

	/*add handler for suspend interrupt*/
	result = ipa3_add_interrupt_handler(IPA_TX_SUSPEND_IRQ,
			ipa3_suspend_handler, false, NULL);
	if (result) {
		IPAERR("register handler for suspend interrupt failed\n");
		result = -ENODEV;
		goto fail_add_interrupt_handler;
	}

	return 0;

fail_add_interrupt_handler:
	ipa3_interrupts_destroy(ipa3_res.ipa_irq, &ipa3_ctx->master_pdev->dev);
	return result;
}

/**
 * ipa3_destroy_flt_tbl_idrs() - destroy the idr structure for flt tables
 *  The idr strcuture per filtering table is intended for rule id generation
 *  per filtering rule.
 */
static void ipa3_destroy_flt_tbl_idrs(void)
{
	int i;
	struct ipa3_flt_tbl *flt_tbl;

	idr_destroy(&ipa3_ctx->flt_rule_ids[IPA_IP_v4]);
	idr_destroy(&ipa3_ctx->flt_rule_ids[IPA_IP_v6]);

	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if (!ipa_is_ep_support_flt(i))
			continue;

		flt_tbl = &ipa3_ctx->flt_tbl[i][IPA_IP_v4];
		flt_tbl->rule_ids = NULL;
		flt_tbl = &ipa3_ctx->flt_tbl[i][IPA_IP_v6];
		flt_tbl->rule_ids = NULL;
	}
}

static void ipa3_freeze_clock_vote_and_notify_modem(void)
{
	int res;
	struct ipa_active_client_logging_info log_info;

	if (ipa3_ctx->platform_type == IPA_PLAT_TYPE_APQ) {
		IPADBG("Ignore smp2p on APQ platform\n");
		return;
	}

	if (ipa3_ctx->smp2p_info.res_sent)
		return;

	if (IS_ERR(ipa3_ctx->smp2p_info.smem_state)) {
		IPAERR("fail to get smp2p clk resp bit %ld\n",
			PTR_ERR(ipa3_ctx->smp2p_info.smem_state));
		return;
	}

	IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info, "FREEZE_VOTE");
	res = ipa3_inc_client_enable_clks_no_block(&log_info);
	if (res)
		ipa3_ctx->smp2p_info.ipa_clk_on = false;
	else
		ipa3_ctx->smp2p_info.ipa_clk_on = true;

	qcom_smem_state_update_bits(ipa3_ctx->smp2p_info.smem_state,
			IPA_SMP2P_SMEM_STATE_MASK,
			((ipa3_ctx->smp2p_info.ipa_clk_on <<
			IPA_SMP2P_OUT_CLK_VOTE_IDX) |
			(1 << IPA_SMP2P_OUT_CLK_RSP_CMPLT_IDX)));

	ipa3_ctx->smp2p_info.res_sent = true;
	IPADBG("IPA clocks are %s\n",
		ipa3_ctx->smp2p_info.ipa_clk_on ? "ON" : "OFF");
}

void ipa3_reset_freeze_vote(void)
{
	if (ipa3_ctx->smp2p_info.res_sent == false)
		return;

	if (ipa3_ctx->smp2p_info.ipa_clk_on)
		IPA_ACTIVE_CLIENTS_DEC_SPECIAL("FREEZE_VOTE");

	qcom_smem_state_update_bits(ipa3_ctx->smp2p_info.smem_state,
			IPA_SMP2P_SMEM_STATE_MASK,
			((0 <<
			IPA_SMP2P_OUT_CLK_VOTE_IDX) |
			(0 << IPA_SMP2P_OUT_CLK_RSP_CMPLT_IDX)));

	ipa3_ctx->smp2p_info.res_sent = false;
	ipa3_ctx->smp2p_info.ipa_clk_on = false;
}

static int ipa3_panic_notifier(struct notifier_block *this,
	unsigned long event, void *ptr)
{
	int res;

	ipa3_freeze_clock_vote_and_notify_modem();

	IPADBG("Calling uC panic handler\n");
	res = ipa3_uc_panic_notifier(this, event, ptr);
	if (res)
		IPAERR("uC panic handler failed %d\n", res);

	if (atomic_read(&ipa3_ctx->ipa_clk_vote)) {
		ipahal_print_all_regs(false);
		ipa_save_registers();
	}

	return NOTIFY_DONE;
}

static struct notifier_block ipa3_panic_blk = {
	.notifier_call = ipa3_panic_notifier,
	/* IPA panic handler needs to run before modem shuts down */
	.priority = INT_MAX,
};

static void ipa3_register_panic_hdlr(void)
{
	atomic_notifier_chain_register(&panic_notifier_list,
		&ipa3_panic_blk);
}

static void ipa3_trigger_ipa_ready_cbs(void)
{
	struct ipa3_ready_cb_info *info;

	mutex_lock(&ipa3_ctx->lock);

	/* Call all the CBs */
	list_for_each_entry(info, &ipa3_ctx->ipa_ready_cb_list, link)
		if (info->ready_cb)
			info->ready_cb(info->user_data);

	mutex_unlock(&ipa3_ctx->lock);
}

static void ipa3_uc_is_loaded(void)
{
	IPADBG("\n");
	complete_all(&ipa3_ctx->uc_loaded_completion_obj);
}

static enum gsi_ver ipa3_get_gsi_ver(enum ipa_hw_type ipa_hw_type)
{
	enum gsi_ver gsi_ver;

	switch (ipa_hw_type) {
	case IPA_HW_v3_0:
	case IPA_HW_v3_1:
		gsi_ver = GSI_VER_1_0;
		break;
	case IPA_HW_v3_5:
		gsi_ver = GSI_VER_1_2;
		break;
	case IPA_HW_v3_5_1:
		gsi_ver = GSI_VER_1_3;
		break;
	case IPA_HW_v4_0:
	case IPA_HW_v4_1:
		gsi_ver = GSI_VER_2_0;
		break;
	case IPA_HW_v4_2:
		gsi_ver = GSI_VER_2_2;
		break;
	case IPA_HW_v4_5:
		gsi_ver = GSI_VER_2_5;
		break;
	default:
		IPAERR("No GSI version for ipa type %d\n", ipa_hw_type);
		WARN_ON(1);
		gsi_ver = GSI_VER_ERR;
	}

	IPADBG("GSI version %d\n", gsi_ver);

	return gsi_ver;
}

static int ipa3_gsi_pre_fw_load_init(void)
{
	int result;

	result = gsi_configure_regs(
		ipa3_res.ipa_mem_base,
		ipa3_get_gsi_ver(ipa3_res.ipa_hw_type));

	if (result) {
		IPAERR("Failed to configure GSI registers\n");
		return -EINVAL;
	}

	return 0;
}

static int ipa3_alloc_gsi_channel(void)
{
	const struct ipa_gsi_ep_config *gsi_ep_cfg;
	enum ipa_client_type type;
	int code = 0;
	int ret = 0;
	int i;

	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		type = ipa3_get_client_by_pipe(i);
		gsi_ep_cfg = ipa3_get_gsi_ep_info(type);
		IPADBG("for ep %d client is %d\n", i, type);
		if (!gsi_ep_cfg)
			continue;

		ret = gsi_alloc_channel_ee(gsi_ep_cfg->ipa_gsi_chan_num,
					gsi_ep_cfg->ee, &code);
		if (ret == GSI_STATUS_SUCCESS) {
			IPADBG("alloc gsi ch %d ee %d with code %d\n",
					gsi_ep_cfg->ipa_gsi_chan_num,
					gsi_ep_cfg->ee,
					code);
		} else {
			IPAERR("failed to alloc ch %d ee %d code %d\n",
					gsi_ep_cfg->ipa_gsi_chan_num,
					gsi_ep_cfg->ee,
					code);
			return ret;
		}
	}
	return ret;
}
/**
 * ipa3_post_init() - Initialize the IPA Driver (Part II).
 * This part contains all initialization which requires interaction with
 * IPA HW (via GSI).
 *
 * @resource_p:	contain platform specific values from DST file
 * @pdev:	The platform device structure representing the IPA driver
 *
 * Function initialization process:
 * - Initialize endpoints bitmaps
 * - Initialize resource groups min and max values
 * - Initialize filtering lists heads and idr
 * - Initialize interrupts
 * - Register GSI
 * - Setup APPS pipes
 * - Initialize tethering bridge
 * - Initialize IPA debugfs
 * - Initialize IPA uC interface
 * - Initialize WDI interface
 * - Initialize USB interface
 * - Register for panic handler
 * - Trigger IPA ready callbacks (to all subscribers)
 * - Trigger IPA completion object (to all who wait on it)
 */
static int ipa3_post_init(const struct ipa3_plat_drv_res *resource_p,
			  struct device *ipa_dev)
{
	int result;
	struct gsi_per_props gsi_props;
	struct ipa3_uc_hdlrs uc_hdlrs = { 0 };
	struct ipa3_flt_tbl *flt_tbl;
	int i;
	struct idr *idr;

	if (ipa3_ctx == NULL) {
		IPADBG("IPA driver haven't initialized\n");
		return -ENXIO;
	}

	/* Prevent consequent calls from trying to load the FW again. */
	if (ipa3_ctx->ipa_initialization_complete)
		return 0;

	IPADBG("active clients = %d\n",
			atomic_read(&ipa3_ctx->ipa3_active_clients.cnt));
	/* move proxy vote for modem on ipa3_post_init */
	if (ipa3_ctx->ipa_hw_type != IPA_HW_v4_0)
		ipa3_proxy_clk_vote();

	/* The following will retrieve and save the gsi fw version */
	ipa_save_gsi_ver();

	/*
	 * In Virtual and Emulation mode, IPAHAL initialized at
	 * pre_init as there is no SMMU. In normal mode need to wait
	 * until SMMU is attached and thus initialization done here.
	 */
	if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_VIRTUAL &&
	    ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_EMULATION) {
		if (ipahal_init(ipa3_ctx->ipa_hw_type, ipa3_ctx->mmio,
			ipa3_ctx->pdev)) {
			IPAERR("fail to init ipahal\n");
			result = -EFAULT;
			goto fail_ipahal;
		}
	}

	result = ipa3_init_hw();
	if (result) {
		IPAERR(":error initializing HW\n");
		result = -ENODEV;
		goto fail_init_hw;
	}
	IPADBG("IPA HW initialization sequence completed");

	ipa3_ctx->ipa_num_pipes = ipa3_get_num_pipes();
	IPADBG("IPA Pipes num %u\n", ipa3_ctx->ipa_num_pipes);
	if (ipa3_ctx->ipa_num_pipes > IPA3_MAX_NUM_PIPES) {
		IPAERR("IPA has more pipes then supported has %d, max %d\n",
			ipa3_ctx->ipa_num_pipes, IPA3_MAX_NUM_PIPES);
		result = -ENODEV;
		goto fail_init_hw;
	}

	ipa3_ctx->ctrl->ipa_sram_read_settings();
	IPADBG("SRAM, size: 0x%x, restricted bytes: 0x%x\n",
		ipa3_ctx->smem_sz, ipa3_ctx->smem_restricted_bytes);

	IPADBG("hdr_lcl=%u ip4_rt_hash=%u ip4_rt_nonhash=%u\n",
		ipa3_ctx->hdr_tbl_lcl, ipa3_ctx->ip4_rt_tbl_hash_lcl,
		ipa3_ctx->ip4_rt_tbl_nhash_lcl);

	IPADBG("ip6_rt_hash=%u ip6_rt_nonhash=%u\n",
		ipa3_ctx->ip6_rt_tbl_hash_lcl, ipa3_ctx->ip6_rt_tbl_nhash_lcl);

	IPADBG("ip4_flt_hash=%u ip4_flt_nonhash=%u\n",
		ipa3_ctx->ip4_flt_tbl_hash_lcl,
		ipa3_ctx->ip4_flt_tbl_nhash_lcl);

	IPADBG("ip6_flt_hash=%u ip6_flt_nonhash=%u\n",
		ipa3_ctx->ip6_flt_tbl_hash_lcl,
		ipa3_ctx->ip6_flt_tbl_nhash_lcl);

	if (ipa3_ctx->smem_reqd_sz > ipa3_ctx->smem_sz) {
		IPAERR("SW expect more core memory, needed %d, avail %d\n",
			ipa3_ctx->smem_reqd_sz, ipa3_ctx->smem_sz);
		result = -ENOMEM;
		goto fail_init_hw;
	}

	result = ipa3_allocate_dma_task_for_gsi();
	if (result) {
		IPAERR("failed to allocate dma task\n");
		goto fail_dma_task;
	}

	if (ipa3_nat_ipv6ct_init_devices()) {
		IPAERR("unable to init NAT and IPv6CT devices\n");
		result = -ENODEV;
		goto fail_nat_ipv6ct_init_dev;
	}

	result = ipa3_alloc_pkt_init();
	if (result) {
		IPAERR("Failed to alloc pkt_init payload\n");
		result = -ENODEV;
		goto fail_allok_pkt_init;
	}

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v3_5)
		ipa3_enable_dcd();

	/*
	 * indication whether working in MHI config or non MHI config is given
	 * in ipa3_write which is launched before ipa3_post_init. i.e. from
	 * this point it is safe to use ipa3_ep_mapping array and the correct
	 * entry will be returned from ipa3_get_hw_type_index()
	 */
	ipa_init_ep_flt_bitmap();
	IPADBG("EP with flt support bitmap 0x%x (%u pipes)\n",
		ipa3_ctx->ep_flt_bitmap, ipa3_ctx->ep_flt_num);

	/* Assign resource limitation to each group */
	ipa3_set_resorce_groups_min_max_limits();

	idr = &(ipa3_ctx->flt_rule_ids[IPA_IP_v4]);
	idr_init(idr);
	idr = &(ipa3_ctx->flt_rule_ids[IPA_IP_v6]);
	idr_init(idr);

	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if (!ipa_is_ep_support_flt(i))
			continue;

		flt_tbl = &ipa3_ctx->flt_tbl[i][IPA_IP_v4];
		INIT_LIST_HEAD(&flt_tbl->head_flt_rule_list);
		flt_tbl->in_sys[IPA_RULE_HASHABLE] =
			!ipa3_ctx->ip4_flt_tbl_hash_lcl;
		flt_tbl->in_sys[IPA_RULE_NON_HASHABLE] =
			!ipa3_ctx->ip4_flt_tbl_nhash_lcl;
		flt_tbl->rule_ids = &ipa3_ctx->flt_rule_ids[IPA_IP_v4];

		flt_tbl = &ipa3_ctx->flt_tbl[i][IPA_IP_v6];
		INIT_LIST_HEAD(&flt_tbl->head_flt_rule_list);
		flt_tbl->in_sys[IPA_RULE_HASHABLE] =
			!ipa3_ctx->ip6_flt_tbl_hash_lcl;
		flt_tbl->in_sys[IPA_RULE_NON_HASHABLE] =
			!ipa3_ctx->ip6_flt_tbl_nhash_lcl;
		flt_tbl->rule_ids = &ipa3_ctx->flt_rule_ids[IPA_IP_v6];
	}

	result = ipa3_init_interrupts();
	if (result) {
		IPAERR("ipa initialization of interrupts failed\n");
		result = -ENODEV;
		goto fail_init_interrupts;
	}

	/*
	 * Disable prefetch for USB or MHI at IPAv3.5/IPA.3.5.1
	 * This is to allow MBIM to work.
	 */
	if ((ipa3_ctx->ipa_hw_type >= IPA_HW_v3_5
		&& ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) &&
		(!ipa3_ctx->ipa_config_is_mhi))
		ipa3_disable_prefetch(IPA_CLIENT_USB_CONS);

	if ((ipa3_ctx->ipa_hw_type >= IPA_HW_v3_5
		&& ipa3_ctx->ipa_hw_type < IPA_HW_v4_0) &&
		(ipa3_ctx->ipa_config_is_mhi))
		ipa3_disable_prefetch(IPA_CLIENT_MHI_CONS);

	memset(&gsi_props, 0, sizeof(gsi_props));
	gsi_props.ver = ipa3_get_gsi_ver(resource_p->ipa_hw_type);
	gsi_props.ee = resource_p->ee;
	gsi_props.intr = GSI_INTR_IRQ;
	gsi_props.phys_addr = resource_p->transport_mem_base;
	gsi_props.size = resource_p->transport_mem_size;
	if (ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_EMULATION) {
		gsi_props.irq = resource_p->emulator_irq;
		gsi_props.emulator_intcntrlr_client_isr = ipa3_get_isr();
		gsi_props.emulator_intcntrlr_addr =
		    resource_p->emulator_intcntrlr_mem_base;
		gsi_props.emulator_intcntrlr_size =
		    resource_p->emulator_intcntrlr_mem_size;
	} else {
		gsi_props.irq = resource_p->transport_irq;
	}
	gsi_props.notify_cb = ipa_gsi_notify_cb;
	gsi_props.req_clk_cb = NULL;
	gsi_props.rel_clk_cb = NULL;
	gsi_props.clk_status_cb = ipa3_active_clks_status;

	if (ipa3_ctx->ipa_config_is_mhi) {
		gsi_props.mhi_er_id_limits_valid = true;
		gsi_props.mhi_er_id_limits[0] = resource_p->mhi_evid_limits[0];
		gsi_props.mhi_er_id_limits[1] = resource_p->mhi_evid_limits[1];
	}

	result = gsi_register_device(&gsi_props,
		&ipa3_ctx->gsi_dev_hdl);
	if (result != GSI_STATUS_SUCCESS) {
		IPAERR(":gsi register error - %d\n", result);
		result = -ENODEV;
		goto fail_register_device;
	}
	IPADBG("IPA gsi is registered\n");
	/* GSI 2.2 requires to allocate all EE GSI channel
	 * during device bootup.
	 */
	if (ipa3_get_gsi_ver(resource_p->ipa_hw_type) == GSI_VER_2_2) {
		result = ipa3_alloc_gsi_channel();
		if (result) {
			IPAERR("Failed to alloc the GSI channels\n");
			result = -ENODEV;
			goto fail_alloc_gsi_channel;
		}
	}

	/* setup the AP-IPA pipes */
	if (ipa3_setup_apps_pipes()) {
		IPAERR(":failed to setup IPA-Apps pipes\n");
		result = -ENODEV;
		goto fail_setup_apps_pipes;
	}
	IPADBG("IPA GPI pipes were connected\n");

	if (ipa3_ctx->use_ipa_teth_bridge) {
		/* Initialize the tethering bridge driver */
		result = ipa3_teth_bridge_driver_init();
		if (result) {
			IPAERR(":teth_bridge init failed (%d)\n", -result);
			result = -ENODEV;
			goto fail_teth_bridge_driver_init;
		}
		IPADBG("teth_bridge initialized");
	}

	result = ipa3_uc_interface_init();
	if (result)
		IPAERR(":ipa Uc interface init failed (%d)\n", -result);
	else
		IPADBG(":ipa Uc interface init ok\n");

	uc_hdlrs.ipa_uc_loaded_hdlr = ipa3_uc_is_loaded;
	ipa3_uc_register_handlers(IPA_HW_FEATURE_COMMON, &uc_hdlrs);

	result = ipa3_wdi_init();
	if (result)
		IPAERR(":wdi init failed (%d)\n", -result);
	else
		IPADBG(":wdi init ok\n");

	result = ipa3_wigig_init_i();
	if (result)
		IPAERR(":wigig init failed (%d)\n", -result);
	else
		IPADBG(":wigig init ok\n");

	result = ipa3_ntn_init();
	if (result)
		IPAERR(":ntn init failed (%d)\n", -result);
	else
		IPADBG(":ntn init ok\n");

	result = ipa_hw_stats_init();
	if (result)
		IPAERR("fail to init stats %d\n", result);
	else
		IPADBG(":stats init ok\n");

	ipa3_register_panic_hdlr();

	ipa3_debugfs_post_init();

	mutex_lock(&ipa3_ctx->lock);
	ipa3_ctx->ipa_initialization_complete = true;
	mutex_unlock(&ipa3_ctx->lock);

	ipa3_trigger_ipa_ready_cbs();
	complete_all(&ipa3_ctx->init_completion_obj);
	pr_info("IPA driver initialization was successful.\n");

	return 0;

fail_teth_bridge_driver_init:
	ipa3_teardown_apps_pipes();
fail_alloc_gsi_channel:
fail_setup_apps_pipes:
	gsi_deregister_device(ipa3_ctx->gsi_dev_hdl, false);
fail_register_device:
	ipa3_destroy_flt_tbl_idrs();
fail_init_interrupts:
	 ipa3_remove_interrupt_handler(IPA_TX_SUSPEND_IRQ);
	 ipa3_interrupts_destroy(ipa3_res.ipa_irq, &ipa3_ctx->master_pdev->dev);
fail_allok_pkt_init:
	ipa3_nat_ipv6ct_destroy_devices();
fail_nat_ipv6ct_init_dev:
	ipa3_free_dma_task_for_gsi();
fail_dma_task:
fail_init_hw:
	if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_VIRTUAL &&
	    ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_EMULATION)
		ipahal_destroy();
fail_ipahal:
	ipa3_proxy_clk_unvote();

	return result;
}

static int ipa3_manual_load_ipa_fws(void)
{
	int result;
	const struct firmware *fw;
	const char *path = IPA_FWS_PATH;

	if (ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_EMULATION) {
		switch (ipa3_get_emulation_type()) {
		case IPA_HW_v3_5_1:
			path = IPA_FWS_PATH_3_5_1;
			break;
		case IPA_HW_v4_0:
			path = IPA_FWS_PATH_4_0;
			break;
		case IPA_HW_v4_5:
			path = IPA_FWS_PATH_4_5;
			break;
		default:
			break;
		}
	}

	IPADBG("Manual FW loading (%s) process initiated\n", path);

	result = request_firmware(&fw, path, ipa3_ctx->cdev.dev);
	if (result < 0) {
		IPAERR("request_firmware failed, error %d\n", result);
		return result;
	}

	IPADBG("FWs are available for loading\n");

	if (ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_EMULATION) {
		result = emulator_load_fws(fw,
			ipa3_res.transport_mem_base,
			ipa3_res.transport_mem_size,
			ipa3_get_gsi_ver(ipa3_res.ipa_hw_type));
	} else {
		result = ipa3_load_fws(fw, ipa3_res.transport_mem_base,
			ipa3_get_gsi_ver(ipa3_res.ipa_hw_type));
	}

	if (result) {
		IPAERR("Manual IPA FWs loading has failed\n");
		release_firmware(fw);
		return result;
	}

	result = gsi_enable_fw(ipa3_res.transport_mem_base,
				ipa3_res.transport_mem_size,
				ipa3_get_gsi_ver(ipa3_res.ipa_hw_type));
	if (result) {
		IPAERR("Failed to enable GSI FW\n");
		release_firmware(fw);
		return result;
	}

	release_firmware(fw);

	IPADBG("Manual FW loading process is complete\n");

	return 0;
}

static int ipa3_pil_load_ipa_fws(const char *sub_sys)
{
	void *subsystem_get_retval = NULL;

	IPADBG("PIL FW loading process initiated sub_sys=%s\n",
		sub_sys);

	subsystem_get_retval = subsystem_get(sub_sys);
	if (IS_ERR_OR_NULL(subsystem_get_retval)) {
		IPAERR("Unable to PIL load FW for sub_sys=%s\n", sub_sys);
		return -EINVAL;
	}

	IPADBG("PIL FW loading process is complete sub_sys=%s\n", sub_sys);
	return 0;
}

static void ipa3_load_ipa_fw(struct work_struct *work)
{
	int result;

	IPADBG("Entry\n");

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	result = ipa3_attach_to_smmu();
	if (result) {
		IPAERR("IPA attach to smmu failed %d\n", result);
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return;
	}

	if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_EMULATION &&
	    ((ipa3_ctx->platform_type != IPA_PLAT_TYPE_MDM) ||
	    (ipa3_ctx->ipa_hw_type >= IPA_HW_v3_5)))
		result = ipa3_pil_load_ipa_fws(IPA_SUBSYSTEM_NAME);
	else
		result = ipa3_manual_load_ipa_fws();

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	if (result) {
		IPAERR("IPA FW loading process has failed result=%d\n",
			result);
		ipa_assert();
		return;
	}
	pr_info("IPA FW loaded successfully\n");

	result = ipa3_post_init(&ipa3_res, ipa3_ctx->cdev.dev);
	if (result) {
		IPAERR("IPA post init failed %d\n", result);
		return;
	}

	if (ipa3_ctx->platform_type == IPA_PLAT_TYPE_APQ &&
		ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_VIRTUAL &&
		ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_EMULATION) {

		IPADBG("Loading IPA uC via PIL\n");

		/* Unvoting will happen when uC loaded event received. */
		ipa3_proxy_clk_vote();

		result = ipa3_pil_load_ipa_fws(IPA_UC_SUBSYSTEM_NAME);
		if (result) {
			IPAERR("IPA uC loading process has failed result=%d\n",
				result);
			return;
		}
		IPADBG("IPA uC PIL loading succeeded\n");
	}
}

static ssize_t ipa3_write(struct file *file, const char __user *buf,
			  size_t count, loff_t *ppos)
{
	unsigned long missing;

	char dbg_buff[32] = { 0 };

	int i = 0;

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, buf, count);

	if (missing) {
		IPAERR("Unable to copy data from user\n");
		return -EFAULT;
	}

	if (count > 0)
		dbg_buff[count] = '\0';

	IPADBG("user input string %s\n", dbg_buff);


	/*Ignore empty ipa_config file*/
	for (i = 0 ; i < count ; ++i) {
		if (!isspace(dbg_buff[i]))
			break;
	}

	if (i == count) {
		IPADBG("Empty ipa_config file\n");
		return count;
	}

	/* Check MHI configuration on MDM devices */
	if (!ipa3_is_msm_device()) {

		if (strnstr(dbg_buff, "vlan", strlen(dbg_buff))) {
			if (strnstr(dbg_buff, "eth", strlen(dbg_buff)))
				ipa3_ctx->vlan_mode_iface[IPA_VLAN_IF_EMAC] =
				true;
			if (strnstr(dbg_buff, "rndis", strlen(dbg_buff)))
				ipa3_ctx->vlan_mode_iface[IPA_VLAN_IF_RNDIS] =
				true;
			if (strnstr(dbg_buff, "ecm", strlen(dbg_buff)))
				ipa3_ctx->vlan_mode_iface[IPA_VLAN_IF_ECM] =
				true;

			/*
			 * when vlan mode is passed to our dev we expect
			 * another write
			 */
			return count;
		}

		/* trim ending newline character if any */
		if (count && (dbg_buff[count - 1] == '\n'))
			dbg_buff[count - 1] = '\0';

		/*
		 * This logic enforeces MHI mode based on userspace input.
		 * Note that MHI mode could be already determined due
		 *  to previous logic.
		 */
		if (!strcasecmp(dbg_buff, "MHI")) {
			ipa3_ctx->ipa_config_is_mhi = true;
		} else if (strcmp(dbg_buff, "1")) {
			IPAERR("got invalid string %s not loading FW\n",
				dbg_buff);
			return count;
		}
		pr_info("IPA is loading with %sMHI configuration\n",
			ipa3_ctx->ipa_config_is_mhi ? "" : "non ");
	}

	/* Prevent consequent calls from trying to load the FW again. */
	if (ipa3_is_ready())
		return count;

	/* Prevent multiple calls from trying to load the FW again. */
	if (ipa3_ctx->fw_loaded) {
		IPAERR("not load FW again\n");
		return count;
	}

	/* Schedule WQ to load ipa-fws */
	ipa3_ctx->fw_loaded = true;

	queue_work(ipa3_ctx->transport_power_mgmt_wq,
		&ipa3_fw_loading_work);

	IPADBG("Scheduled a work to load IPA FW\n");
	return count;
}

/**
 * ipa3_tz_unlock_reg - Unlocks memory regions so that they become accessible
 *	from AP.
 * @reg_info - Pointer to array of memory regions to unlock
 * @num_regs - Number of elements in the array
 *
 * Converts the input array of regions to a struct that TZ understands and
 * issues an SCM call.
 * Also flushes the memory cache to DDR in order to make sure that TZ sees the
 * correct data structure.
 *
 * Returns: 0 on success, negative on failure
 */
int ipa3_tz_unlock_reg(struct ipa_tz_unlock_reg_info *reg_info, u16 num_regs)
{
	int i, size, ret;
	struct tz_smmu_ipa_protect_region_iovec_s *ipa_tz_unlock_vec;
	struct tz_smmu_ipa_protect_region_s cmd_buf;
	struct scm_desc desc = {0};

	if (reg_info ==  NULL || num_regs == 0) {
		IPAERR("Bad parameters\n");
		return -EFAULT;
	}

	size = num_regs * sizeof(struct tz_smmu_ipa_protect_region_iovec_s);
	ipa_tz_unlock_vec = kzalloc(PAGE_ALIGN(size), GFP_KERNEL);
	if (ipa_tz_unlock_vec == NULL)
		return -ENOMEM;

	for (i = 0; i < num_regs; i++) {
		ipa_tz_unlock_vec[i].input_addr = reg_info[i].reg_addr ^
			(reg_info[i].reg_addr & 0xFFF);
		ipa_tz_unlock_vec[i].output_addr = reg_info[i].reg_addr ^
			(reg_info[i].reg_addr & 0xFFF);
		ipa_tz_unlock_vec[i].size = reg_info[i].size;
		ipa_tz_unlock_vec[i].attr = IPA_TZ_UNLOCK_ATTRIBUTE;
	}

	/* pass physical address of command buffer */
	cmd_buf.iovec_buf = virt_to_phys((void *)ipa_tz_unlock_vec);
	cmd_buf.size_bytes = size;

		desc.args[0] = virt_to_phys((void *)ipa_tz_unlock_vec);
		desc.args[1] = size;
		desc.arginfo = SCM_ARGS(2);
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_MP,
				TZ_MEM_PROTECT_REGION_ID), &desc);

		if (ret) {
			IPAERR("scm call SCM_SVC_MP failed: %d\n", ret);
			kfree(ipa_tz_unlock_vec);
			return -EFAULT;
		}
	kfree(ipa_tz_unlock_vec);
	return 0;
}

static int ipa3_alloc_pkt_init(void)
{
	struct ipa_mem_buffer mem;
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	struct ipahal_imm_cmd_ip_packet_init cmd = {0};
	int i;

	cmd_pyld = ipahal_construct_imm_cmd(IPA_IMM_CMD_IP_PACKET_INIT,
		&cmd, false);
	if (!cmd_pyld) {
		IPAERR("failed to construct IMM cmd\n");
		return -ENOMEM;
	}
	ipa3_ctx->pkt_init_imm_opcode = cmd_pyld->opcode;

	mem.size = cmd_pyld->len * ipa3_ctx->ipa_num_pipes;
	mem.base = dma_alloc_coherent(ipa3_ctx->pdev, mem.size,
		&mem.phys_base, GFP_KERNEL);
	if (!mem.base) {
		IPAERR("failed to alloc DMA buff of size %d\n", mem.size);
		ipahal_destroy_imm_cmd(cmd_pyld);
		return -ENOMEM;
	}
	ipahal_destroy_imm_cmd(cmd_pyld);

	memset(mem.base, 0, mem.size);
	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		cmd.destination_pipe_index = i;
		cmd_pyld = ipahal_construct_imm_cmd(IPA_IMM_CMD_IP_PACKET_INIT,
			&cmd, false);
		if (!cmd_pyld) {
			IPAERR("failed to construct IMM cmd\n");
			dma_free_coherent(ipa3_ctx->pdev,
				mem.size,
				mem.base,
				mem.phys_base);
			return -ENOMEM;
		}
		memcpy(mem.base + i * cmd_pyld->len, cmd_pyld->data,
			cmd_pyld->len);
		ipa3_ctx->pkt_init_imm[i] = mem.phys_base + i * cmd_pyld->len;
		ipahal_destroy_imm_cmd(cmd_pyld);
	}

	return 0;
}

/*
 * SCM call to check if secure dump is allowed.
 *
 * Returns true in secure dump allowed.
 * Return false when secure dump not allowed.
 */
#define TZ_UTIL_GET_SEC_DUMP_STATE  0x10
static bool ipa_is_mem_dump_allowed(void)
{
	struct scm_desc desc = {0};
	int ret = 0;

	desc.args[0] = 0;
	desc.arginfo = 0;

	ret = scm_call2(
		SCM_SIP_FNID(SCM_SVC_UTIL, TZ_UTIL_GET_SEC_DUMP_STATE),
		&desc);
	if (ret) {
		IPAERR("SCM DUMP_STATE call failed\n");
		return false;
	}

	return (desc.ret[0] == 1);
}

/**
 * ipa3_pre_init() - Initialize the IPA Driver.
 * This part contains all initialization which doesn't require IPA HW, such
 * as structure allocations and initializations, register writes, etc.
 *
 * @resource_p:	contain platform specific values from DST file
 * @pdev:	The platform device structure representing the IPA driver
 *
 * Function initialization process:
 * Allocate memory for the driver context data struct
 * Initializing the ipa3_ctx with :
 *    1)parsed values from the dts file
 *    2)parameters passed to the module initialization
 *    3)read HW values(such as core memory size)
 * Map IPA core registers to CPU memory
 * Restart IPA core(HW reset)
 * Initialize the look-aside caches(kmem_cache/slab) for filter,
 *   routing and IPA-tree
 * Create memory pool with 4 objects for DMA operations(each object
 *   is 512Bytes long), this object will be use for tx(A5->IPA)
 * Initialize lists head(routing, hdr, system pipes)
 * Initialize mutexes (for ipa_ctx and NAT memory mutexes)
 * Initialize spinlocks (for list related to A5<->IPA pipes)
 * Initialize 2 single-threaded work-queue named "ipa rx wq" and "ipa tx wq"
 * Initialize Red-Black-Tree(s) for handles of header,routing rule,
 *  routing table ,filtering rule
 * Initialize the filter block by committing IPV4 and IPV6 default rules
 * Create empty routing table in system memory(no committing)
 * Create a char-device for IPA
 * Initialize IPA RM (resource manager)
 * Configure GSI registers (in GSI case)
 */
static int ipa3_pre_init(const struct ipa3_plat_drv_res *resource_p,
		struct platform_device *ipa_pdev)
{
	int result = 0;
	int i, j;
	struct ipa3_rt_tbl_set *rset;
	struct ipa_active_client_logging_info log_info;
	struct cdev *cdev;

	IPADBG("IPA Driver initialization started\n");

	ipa3_ctx = kzalloc(sizeof(*ipa3_ctx), GFP_KERNEL);
	if (!ipa3_ctx) {
		result = -ENOMEM;
		goto fail_mem_ctx;
	}

	ipa3_ctx->logbuf = ipc_log_context_create(IPA_IPC_LOG_PAGES, "ipa", 0);
	if (ipa3_ctx->logbuf == NULL)
		IPADBG("failed to create IPC log, continue...\n");

	/* ipa3_ctx->pdev and ipa3_ctx->uc_pdev will be set in the smmu probes*/
	ipa3_ctx->master_pdev = ipa_pdev;
	for (i = 0; i < IPA_SMMU_CB_MAX; i++)
		ipa3_ctx->s1_bypass_arr[i] = true;

	/* initialize the gsi protocol info for uC debug stats */
	for (i = 0; i < IPA_HW_PROTOCOL_MAX; i++) {
		ipa3_ctx->gsi_info[i].protocol = i;
		/* initialize all to be not started */
		for (j = 0; j < IPA_MAX_CH_STATS_SUPPORTED; j++)
			ipa3_ctx->gsi_info[i].ch_id_info[j].ch_id =
				0xFF;
	}

	ipa3_ctx->ipa_wrapper_base = resource_p->ipa_mem_base;
	ipa3_ctx->ipa_wrapper_size = resource_p->ipa_mem_size;
	ipa3_ctx->ipa_hw_type = resource_p->ipa_hw_type;
	ipa3_ctx->ipa3_hw_mode = resource_p->ipa3_hw_mode;
	ipa3_ctx->platform_type = resource_p->platform_type;
	ipa3_ctx->use_ipa_teth_bridge = resource_p->use_ipa_teth_bridge;
	ipa3_ctx->modem_cfg_emb_pipe_flt = resource_p->modem_cfg_emb_pipe_flt;
	ipa3_ctx->ipa_wdi2 = resource_p->ipa_wdi2;
	ipa3_ctx->ipa_wdi2_over_gsi = resource_p->ipa_wdi2_over_gsi;
	ipa3_ctx->ipa_wdi3_over_gsi = resource_p->ipa_wdi3_over_gsi;
	ipa3_ctx->ipa_fltrt_not_hashable = resource_p->ipa_fltrt_not_hashable;
	ipa3_ctx->use_xbl_boot = resource_p->use_xbl_boot;
	ipa3_ctx->use_64_bit_dma_mask = resource_p->use_64_bit_dma_mask;
	ipa3_ctx->wan_rx_ring_size = resource_p->wan_rx_ring_size;
	ipa3_ctx->lan_rx_ring_size = resource_p->lan_rx_ring_size;
	ipa3_ctx->ipa_wan_skb_page = resource_p->ipa_wan_skb_page;
	ipa3_ctx->skip_uc_pipe_reset = resource_p->skip_uc_pipe_reset;
	ipa3_ctx->tethered_flow_control = resource_p->tethered_flow_control;
	ipa3_ctx->ee = resource_p->ee;
	ipa3_ctx->gsi_ch20_wa = resource_p->gsi_ch20_wa;
	ipa3_ctx->use_ipa_pm = resource_p->use_ipa_pm;
	ipa3_ctx->wdi_over_pcie = resource_p->wdi_over_pcie;
	ipa3_ctx->ipa3_active_clients_logging.log_rdy = false;
	ipa3_ctx->ipa_config_is_mhi = resource_p->ipa_mhi_dynamic_config;
	ipa3_ctx->mhi_evid_limits[0] = resource_p->mhi_evid_limits[0];
	ipa3_ctx->mhi_evid_limits[1] = resource_p->mhi_evid_limits[1];
	ipa3_ctx->uc_mailbox17_chk = 0;
	ipa3_ctx->uc_mailbox17_mismatch = 0;
	ipa3_ctx->entire_ipa_block_size = resource_p->entire_ipa_block_size;
	ipa3_ctx->do_register_collection_on_crash =
	    resource_p->do_register_collection_on_crash;
	ipa3_ctx->do_testbus_collection_on_crash =
	    resource_p->do_testbus_collection_on_crash;
	ipa3_ctx->do_non_tn_collection_on_crash =
	    resource_p->do_non_tn_collection_on_crash;
	ipa3_ctx->ipa_endp_delay_wa = resource_p->ipa_endp_delay_wa;
	ipa3_ctx->secure_debug_check_action =
	    resource_p->secure_debug_check_action;
	ipa3_ctx->ipa_mhi_proxy = resource_p->ipa_mhi_proxy;

	if (ipa3_ctx->secure_debug_check_action == USE_SCM) {
		if (ipa_is_mem_dump_allowed())
			ipa3_ctx->sd_state = SD_ENABLED;
		else
			ipa3_ctx->sd_state = SD_DISABLED;
	} else {
		if (ipa3_ctx->secure_debug_check_action == OVERRIDE_SCM_TRUE)
			ipa3_ctx->sd_state = SD_ENABLED;
		else
			/* secure_debug_check_action == OVERRIDE_SCM_FALSE */
			ipa3_ctx->sd_state = SD_DISABLED;
	}

	if (ipa3_ctx->sd_state == SD_ENABLED) {
		/* secure debug is enabled. */
		IPADBG("secure debug enabled\n");
	} else {
		/* secure debug is disabled. */
		IPADBG("secure debug disabled\n");
		ipa3_ctx->do_testbus_collection_on_crash = false;
	}

	WARN(ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_NORMAL,
		"Non NORMAL IPA HW mode, is this emulation platform ?");

	if (resource_p->ipa_tz_unlock_reg) {
		ipa3_ctx->ipa_tz_unlock_reg_num =
			resource_p->ipa_tz_unlock_reg_num;
		ipa3_ctx->ipa_tz_unlock_reg = kcalloc(
			ipa3_ctx->ipa_tz_unlock_reg_num,
			sizeof(*ipa3_ctx->ipa_tz_unlock_reg),
			GFP_KERNEL);
		if (ipa3_ctx->ipa_tz_unlock_reg == NULL) {
			result = -ENOMEM;
			goto fail_tz_unlock_reg;
		}
		for (i = 0; i < ipa3_ctx->ipa_tz_unlock_reg_num; i++) {
			ipa3_ctx->ipa_tz_unlock_reg[i].reg_addr =
				resource_p->ipa_tz_unlock_reg[i].reg_addr;
			ipa3_ctx->ipa_tz_unlock_reg[i].size =
				resource_p->ipa_tz_unlock_reg[i].size;
		}

		/* unlock registers for uc */
		result = ipa3_tz_unlock_reg(ipa3_ctx->ipa_tz_unlock_reg,
					    ipa3_ctx->ipa_tz_unlock_reg_num);
		if (result)
			IPAERR("Failed to unlock memory region using TZ\n");
	}

	/* default aggregation parameters */
	ipa3_ctx->aggregation_type = IPA_MBIM_16;
	ipa3_ctx->aggregation_byte_limit = 1;
	ipa3_ctx->aggregation_time_limit = 0;

	ipa3_ctx->ctrl = kzalloc(sizeof(*ipa3_ctx->ctrl), GFP_KERNEL);
	if (!ipa3_ctx->ctrl) {
		result = -ENOMEM;
		goto fail_mem_ctrl;
	}
	result = ipa3_controller_static_bind(ipa3_ctx->ctrl,
			ipa3_ctx->ipa_hw_type);
	if (result) {
		IPAERR("fail to static bind IPA ctrl\n");
		result = -EFAULT;
		goto fail_bind;
	}

	result = ipa3_init_mem_partition(ipa3_ctx->ipa_hw_type);
	if (result) {
		IPAERR(":ipa3_init_mem_partition failed\n");
		result = -ENODEV;
		goto fail_init_mem_partition;
	}

	if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_VIRTUAL &&
	    ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_EMULATION) {
		ipa3_ctx->ctrl->msm_bus_data_ptr =
			msm_bus_cl_get_pdata(ipa3_ctx->master_pdev);
		if (ipa3_ctx->ctrl->msm_bus_data_ptr == NULL) {
			IPAERR("failed to get bus scaling\n");
			goto fail_bus_reg;
		}
		IPADBG("Use bus scaling info from device tree #usecases=%d\n",
			ipa3_ctx->ctrl->msm_bus_data_ptr->num_usecases);

		/* get BUS handle */
		ipa3_ctx->ipa_bus_hdl =
			msm_bus_scale_register_client(
				ipa3_ctx->ctrl->msm_bus_data_ptr);
		if (!ipa3_ctx->ipa_bus_hdl) {
			IPAERR("fail to register with bus mgr!\n");
			ipa3_ctx->ctrl->msm_bus_data_ptr = NULL;
			result = -EPROBE_DEFER;
			goto fail_bus_reg;
		}
	}

	/* get IPA clocks */
	result = ipa3_get_clks(&ipa3_ctx->master_pdev->dev);
	if (result)
		goto fail_clk;

	/* init active_clients_log after getting ipa-clk */
	result = ipa3_active_clients_log_init();
	if (result)
		goto fail_init_active_client;

	/* Enable ipa3_ctx->enable_clock_scaling */
	ipa3_ctx->enable_clock_scaling = 1;
	/* vote for svs2 on bootup */
	ipa3_ctx->curr_ipa_clk_rate = ipa3_ctx->ctrl->ipa_clk_rate_svs2;

	/* Enable ipa3_ctx->enable_napi_chain */
	ipa3_ctx->enable_napi_chain = 1;

	/* enable IPA clocks explicitly to allow the initialization */
	ipa3_enable_clks();

	/* setup IPA register access */
	IPADBG("Mapping 0x%x\n", resource_p->ipa_mem_base +
		ipa3_ctx->ctrl->ipa_reg_base_ofst);
	ipa3_ctx->mmio = ioremap(resource_p->ipa_mem_base +
			ipa3_ctx->ctrl->ipa_reg_base_ofst,
			resource_p->ipa_mem_size);
	if (!ipa3_ctx->mmio) {
		IPAERR(":ipa-base ioremap err\n");
		result = -EFAULT;
		goto fail_remap;
	}

	IPADBG(
	    "base(0x%x)+offset(0x%x)=(0x%x) mapped to (%pK) with len (0x%x)\n",
	    resource_p->ipa_mem_base,
	    ipa3_ctx->ctrl->ipa_reg_base_ofst,
	    resource_p->ipa_mem_base + ipa3_ctx->ctrl->ipa_reg_base_ofst,
	    ipa3_ctx->mmio,
	    resource_p->ipa_mem_size);

	/*
	 * Setup access for register collection/dump on crash
	 */
	if (ipa_reg_save_init(IPA_MEM_INIT_VAL) != 0) {
		result = -EFAULT;
		goto fail_gsi_map;
	}

	/*
	 * Since we now know where the transport's registers live,
	 * let's set up access to them.  This is done since subseqent
	 * functions, that deal with the transport, require the
	 * access.
	 */
	if (gsi_map_base(
		ipa3_res.transport_mem_base,
		ipa3_res.transport_mem_size) != 0) {
		IPAERR("Allocation of gsi base failed\n");
		result = -EFAULT;
		goto fail_gsi_map;
	}

	/*
	 * In Virtual and Emulation mode, IPAHAL used to load the
	 * firmwares and there is no SMMU so IPAHAL is initialized
	 * here.
	 */
	if (ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_VIRTUAL ||
	    ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_EMULATION) {
		if (ipahal_init(ipa3_ctx->ipa_hw_type,
				ipa3_ctx->mmio,
				&(ipa3_ctx->master_pdev->dev))) {
			IPAERR("fail to init ipahal\n");
			result = -EFAULT;
			goto fail_ipahal_init;
		}
	}

	mutex_init(&ipa3_ctx->ipa3_active_clients.mutex);

	IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info, "PROXY_CLK_VOTE");
	ipa3_active_clients_log_inc(&log_info, false);
	ipa3_ctx->q6_proxy_clk_vote_valid = true;
	ipa3_ctx->q6_proxy_clk_vote_cnt = 1;

	/*Updating the proxy vote cnt 1 */
	atomic_set(&ipa3_ctx->ipa3_active_clients.cnt, 1);

	/* Create workqueues for power management */
	ipa3_ctx->power_mgmt_wq =
		create_singlethread_workqueue("ipa_power_mgmt");
	if (!ipa3_ctx->power_mgmt_wq) {
		IPAERR("failed to create power mgmt wq\n");
		result = -ENOMEM;
		goto fail_init_hw;
	}

	ipa3_ctx->transport_power_mgmt_wq =
		create_singlethread_workqueue("transport_power_mgmt");
	if (!ipa3_ctx->transport_power_mgmt_wq) {
		IPAERR("failed to create transport power mgmt wq\n");
		result = -ENOMEM;
		goto fail_create_transport_wq;
	}

	mutex_init(&ipa3_ctx->transport_pm.transport_pm_mutex);

	/* init the lookaside cache */
	ipa3_ctx->flt_rule_cache = kmem_cache_create("IPA_FLT",
			sizeof(struct ipa3_flt_entry), 0, 0, NULL);
	if (!ipa3_ctx->flt_rule_cache) {
		IPAERR(":ipa flt cache create failed\n");
		result = -ENOMEM;
		goto fail_flt_rule_cache;
	}
	ipa3_ctx->rt_rule_cache = kmem_cache_create("IPA_RT",
			sizeof(struct ipa3_rt_entry), 0, 0, NULL);
	if (!ipa3_ctx->rt_rule_cache) {
		IPAERR(":ipa rt cache create failed\n");
		result = -ENOMEM;
		goto fail_rt_rule_cache;
	}
	ipa3_ctx->hdr_cache = kmem_cache_create("IPA_HDR",
			sizeof(struct ipa3_hdr_entry), 0, 0, NULL);
	if (!ipa3_ctx->hdr_cache) {
		IPAERR(":ipa hdr cache create failed\n");
		result = -ENOMEM;
		goto fail_hdr_cache;
	}
	ipa3_ctx->hdr_offset_cache =
	   kmem_cache_create("IPA_HDR_OFFSET",
			   sizeof(struct ipa_hdr_offset_entry), 0, 0, NULL);
	if (!ipa3_ctx->hdr_offset_cache) {
		IPAERR(":ipa hdr off cache create failed\n");
		result = -ENOMEM;
		goto fail_hdr_offset_cache;
	}
	ipa3_ctx->hdr_proc_ctx_cache = kmem_cache_create("IPA_HDR_PROC_CTX",
		sizeof(struct ipa3_hdr_proc_ctx_entry), 0, 0, NULL);
	if (!ipa3_ctx->hdr_proc_ctx_cache) {
		IPAERR(":ipa hdr proc ctx cache create failed\n");
		result = -ENOMEM;
		goto fail_hdr_proc_ctx_cache;
	}
	ipa3_ctx->hdr_proc_ctx_offset_cache =
		kmem_cache_create("IPA_HDR_PROC_CTX_OFFSET",
		sizeof(struct ipa3_hdr_proc_ctx_offset_entry), 0, 0, NULL);
	if (!ipa3_ctx->hdr_proc_ctx_offset_cache) {
		IPAERR(":ipa hdr proc ctx off cache create failed\n");
		result = -ENOMEM;
		goto fail_hdr_proc_ctx_offset_cache;
	}
	ipa3_ctx->rt_tbl_cache = kmem_cache_create("IPA_RT_TBL",
			sizeof(struct ipa3_rt_tbl), 0, 0, NULL);
	if (!ipa3_ctx->rt_tbl_cache) {
		IPAERR(":ipa rt tbl cache create failed\n");
		result = -ENOMEM;
		goto fail_rt_tbl_cache;
	}
	ipa3_ctx->tx_pkt_wrapper_cache =
	   kmem_cache_create("IPA_TX_PKT_WRAPPER",
			   sizeof(struct ipa3_tx_pkt_wrapper), 0, 0, NULL);
	if (!ipa3_ctx->tx_pkt_wrapper_cache) {
		IPAERR(":ipa tx pkt wrapper cache create failed\n");
		result = -ENOMEM;
		goto fail_tx_pkt_wrapper_cache;
	}
	ipa3_ctx->rx_pkt_wrapper_cache =
	   kmem_cache_create("IPA_RX_PKT_WRAPPER",
			   sizeof(struct ipa3_rx_pkt_wrapper), 0, 0, NULL);
	if (!ipa3_ctx->rx_pkt_wrapper_cache) {
		IPAERR(":ipa rx pkt wrapper cache create failed\n");
		result = -ENOMEM;
		goto fail_rx_pkt_wrapper_cache;
	}

	/* init the various list heads */
	INIT_LIST_HEAD(&ipa3_ctx->hdr_tbl.head_hdr_entry_list);
	for (i = 0; i < IPA_HDR_BIN_MAX; i++) {
		INIT_LIST_HEAD(&ipa3_ctx->hdr_tbl.head_offset_list[i]);
		INIT_LIST_HEAD(&ipa3_ctx->hdr_tbl.head_free_offset_list[i]);
	}
	INIT_LIST_HEAD(&ipa3_ctx->hdr_proc_ctx_tbl.head_proc_ctx_entry_list);
	for (i = 0; i < IPA_HDR_PROC_CTX_BIN_MAX; i++) {
		INIT_LIST_HEAD(
			&ipa3_ctx->hdr_proc_ctx_tbl.head_offset_list[i]);
		INIT_LIST_HEAD(
			&ipa3_ctx->hdr_proc_ctx_tbl.head_free_offset_list[i]);
	}
	INIT_LIST_HEAD(&ipa3_ctx->rt_tbl_set[IPA_IP_v4].head_rt_tbl_list);
	idr_init(&ipa3_ctx->rt_tbl_set[IPA_IP_v4].rule_ids);
	INIT_LIST_HEAD(&ipa3_ctx->rt_tbl_set[IPA_IP_v6].head_rt_tbl_list);
	idr_init(&ipa3_ctx->rt_tbl_set[IPA_IP_v6].rule_ids);

	rset = &ipa3_ctx->reap_rt_tbl_set[IPA_IP_v4];
	INIT_LIST_HEAD(&rset->head_rt_tbl_list);
	idr_init(&rset->rule_ids);
	rset = &ipa3_ctx->reap_rt_tbl_set[IPA_IP_v6];
	INIT_LIST_HEAD(&rset->head_rt_tbl_list);
	idr_init(&rset->rule_ids);
	idr_init(&ipa3_ctx->flt_rt_counters.hdl);
	spin_lock_init(&ipa3_ctx->flt_rt_counters.hdl_lock);
	memset(&ipa3_ctx->flt_rt_counters.used_hw, 0,
		   sizeof(ipa3_ctx->flt_rt_counters.used_hw));
	memset(&ipa3_ctx->flt_rt_counters.used_sw, 0,
		   sizeof(ipa3_ctx->flt_rt_counters.used_sw));

	INIT_LIST_HEAD(&ipa3_ctx->intf_list);
	INIT_LIST_HEAD(&ipa3_ctx->msg_list);
	INIT_LIST_HEAD(&ipa3_ctx->pull_msg_list);
	init_waitqueue_head(&ipa3_ctx->msg_waitq);
	mutex_init(&ipa3_ctx->msg_lock);

	/* store wlan client-connect-msg-list */
	INIT_LIST_HEAD(&ipa3_ctx->msg_wlan_client_list);
	mutex_init(&ipa3_ctx->msg_wlan_client_lock);

	mutex_init(&ipa3_ctx->lock);
	mutex_init(&ipa3_ctx->q6_proxy_clk_vote_mutex);
	mutex_init(&ipa3_ctx->ipa_cne_evt_lock);

	idr_init(&ipa3_ctx->ipa_idr);
	spin_lock_init(&ipa3_ctx->idr_lock);

	/* wlan related member */
	memset(&ipa3_ctx->wc_memb, 0, sizeof(ipa3_ctx->wc_memb));
	spin_lock_init(&ipa3_ctx->wc_memb.wlan_spinlock);
	spin_lock_init(&ipa3_ctx->wc_memb.ipa_tx_mul_spinlock);
	INIT_LIST_HEAD(&ipa3_ctx->wc_memb.wlan_comm_desc_list);

	ipa3_ctx->cdev.class = class_create(THIS_MODULE, DRV_NAME);

	result = alloc_chrdev_region(&ipa3_ctx->cdev.dev_num, 0, 1, DRV_NAME);
	if (result) {
		IPAERR("alloc_chrdev_region err\n");
		result = -ENODEV;
		goto fail_alloc_chrdev_region;
	}

	ipa3_ctx->cdev.dev = device_create(ipa3_ctx->cdev.class, NULL,
		 ipa3_ctx->cdev.dev_num, ipa3_ctx, DRV_NAME);
	if (IS_ERR(ipa3_ctx->cdev.dev)) {
		IPAERR(":device_create err.\n");
		result = -ENODEV;
		goto fail_device_create;
	}

	ipa3_debugfs_pre_init();

	/* Create a wakeup source. */
	wakeup_source_init(&ipa3_ctx->w_lock, "IPA_WS");
	spin_lock_init(&ipa3_ctx->wakelock_ref_cnt.spinlock);

	/* Initialize Power Management framework */
	if (ipa3_ctx->use_ipa_pm) {
		result = ipa_pm_init(&ipa3_res.pm_init);
		if (result) {
			IPAERR("IPA PM initialization failed (%d)\n", -result);
			result = -ENODEV;
			goto fail_ipa_rm_init;
		}
		IPADBG("IPA resource manager initialized");
	} else {
		result = ipa_rm_initialize();
		if (result) {
			IPAERR("RM initialization failed (%d)\n", -result);
			result = -ENODEV;
			goto fail_ipa_rm_init;
		}
		IPADBG("IPA resource manager initialized");

		result = ipa3_create_apps_resource();
		if (result) {
			IPAERR("Failed to create APPS_CONS resource\n");
			result = -ENODEV;
			goto fail_create_apps_resource;
		}
	}

	INIT_LIST_HEAD(&ipa3_ctx->ipa_ready_cb_list);

	init_completion(&ipa3_ctx->init_completion_obj);
	init_completion(&ipa3_ctx->uc_loaded_completion_obj);

	result = ipa3_dma_setup();
	if (result) {
		IPAERR("Failed to setup IPA DMA\n");
		result = -ENODEV;
		goto fail_ipa_dma_setup;
	}

	result = ipa_eth_init();
	if (result) {
		IPAERR("Failed to initialize Ethernet Offload Subsystem\n");
		result = -ENODEV;
		goto fail_eth_init;
	}

	/*
	 * We can't register the GSI driver yet, as it expects
	 * the GSI FW to be up and running before the registration.
	 *
	 * For IPA3.0 and the emulation system, the GSI configuration
	 * is done by the GSI driver.
	 *
	 * For IPA3.1 (and on), the GSI configuration is done by TZ.
	 */
	if (ipa3_ctx->ipa_hw_type == IPA_HW_v3_0 ||
	    ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_EMULATION) {
		result = ipa3_gsi_pre_fw_load_init();
		if (result) {
			IPAERR("gsi pre FW loading config failed\n");
			result = -ENODEV;
			goto fail_gsi_pre_fw_load_init;
		}
	}

	cdev = &ipa3_ctx->cdev.cdev;
	cdev_init(cdev, &ipa3_drv_fops);
	cdev->owner = THIS_MODULE;
	cdev->ops = &ipa3_drv_fops;  /* from LDD3 */

	result = cdev_add(cdev, ipa3_ctx->cdev.dev_num, 1);
	if (result) {
		IPAERR(":cdev_add err=%d\n", -result);
		result = -ENODEV;
		goto fail_cdev_add;
	}
	IPADBG("ipa cdev added successful. major:%d minor:%d\n",
			MAJOR(ipa3_ctx->cdev.dev_num),
			MINOR(ipa3_ctx->cdev.dev_num));

	if (ipa3_ctx->ipa_hw_type >= IPA_HW_v4_1) {
		result = ipa_odl_init();
		if (result) {
			IPADBG("Error: ODL init fialed\n");
			result = -ENODEV;
			goto fail_cdev_add;
		}
	}

	/*
	 * for IPA 4.0 offline charge is not needed and we need to prevent
	 * power collapse until IPA uC is loaded.
	 */

	/* proxy vote for modem is added in ipa3_post_init() phase */
	if (ipa3_ctx->ipa_hw_type != IPA_HW_v4_0)
		ipa3_proxy_clk_unvote();

	mutex_init(&ipa3_ctx->app_clock_vote.mutex);

	return 0;

fail_cdev_add:
fail_gsi_pre_fw_load_init:
	ipa_eth_exit();
fail_eth_init:
	ipa3_dma_shutdown();
fail_ipa_dma_setup:
	if (ipa3_ctx->use_ipa_pm)
		ipa_pm_destroy();
	else
		ipa_rm_delete_resource(IPA_RM_RESOURCE_APPS_CONS);
fail_create_apps_resource:
	if (!ipa3_ctx->use_ipa_pm)
		ipa_rm_exit();
fail_ipa_rm_init:
	device_destroy(ipa3_ctx->cdev.class, ipa3_ctx->cdev.dev_num);
fail_device_create:
	unregister_chrdev_region(ipa3_ctx->cdev.dev_num, 1);
fail_alloc_chrdev_region:
	idr_destroy(&ipa3_ctx->ipa_idr);
	rset = &ipa3_ctx->reap_rt_tbl_set[IPA_IP_v6];
	idr_destroy(&rset->rule_ids);
	rset = &ipa3_ctx->reap_rt_tbl_set[IPA_IP_v4];
	idr_destroy(&rset->rule_ids);
	idr_destroy(&ipa3_ctx->rt_tbl_set[IPA_IP_v6].rule_ids);
	idr_destroy(&ipa3_ctx->rt_tbl_set[IPA_IP_v4].rule_ids);
	kmem_cache_destroy(ipa3_ctx->rx_pkt_wrapper_cache);
fail_rx_pkt_wrapper_cache:
	kmem_cache_destroy(ipa3_ctx->tx_pkt_wrapper_cache);
fail_tx_pkt_wrapper_cache:
	kmem_cache_destroy(ipa3_ctx->rt_tbl_cache);
fail_rt_tbl_cache:
	kmem_cache_destroy(ipa3_ctx->hdr_proc_ctx_offset_cache);
fail_hdr_proc_ctx_offset_cache:
	kmem_cache_destroy(ipa3_ctx->hdr_proc_ctx_cache);
fail_hdr_proc_ctx_cache:
	kmem_cache_destroy(ipa3_ctx->hdr_offset_cache);
fail_hdr_offset_cache:
	kmem_cache_destroy(ipa3_ctx->hdr_cache);
fail_hdr_cache:
	kmem_cache_destroy(ipa3_ctx->rt_rule_cache);
fail_rt_rule_cache:
	kmem_cache_destroy(ipa3_ctx->flt_rule_cache);
fail_flt_rule_cache:
	destroy_workqueue(ipa3_ctx->transport_power_mgmt_wq);
fail_create_transport_wq:
	destroy_workqueue(ipa3_ctx->power_mgmt_wq);
fail_init_hw:
	if (ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_VIRTUAL ||
	    ipa3_ctx->ipa3_hw_mode == IPA_HW_MODE_EMULATION)
		ipahal_destroy();
fail_ipahal_init:
	gsi_unmap_base();
fail_gsi_map:
	if (ipa3_ctx->reg_collection_base)
		iounmap(ipa3_ctx->reg_collection_base);
	iounmap(ipa3_ctx->mmio);
fail_remap:
	ipa3_disable_clks();
	ipa3_active_clients_log_destroy();
fail_init_active_client:
	if (ipa3_clk)
		clk_put(ipa3_clk);
	ipa3_clk = NULL;
fail_clk:
	if (ipa3_ctx->ipa_bus_hdl)
		msm_bus_scale_unregister_client(ipa3_ctx->ipa_bus_hdl);
fail_bus_reg:
	if (ipa3_ctx->ctrl->msm_bus_data_ptr)
		msm_bus_cl_clear_pdata(ipa3_ctx->ctrl->msm_bus_data_ptr);
fail_init_mem_partition:
fail_bind:
	kfree(ipa3_ctx->ctrl);
fail_mem_ctrl:
	kfree(ipa3_ctx->ipa_tz_unlock_reg);
fail_tz_unlock_reg:
	if (ipa3_ctx->logbuf)
		ipc_log_context_destroy(ipa3_ctx->logbuf);
	kfree(ipa3_ctx);
	ipa3_ctx = NULL;
fail_mem_ctx:
	return result;
}

static int get_ipa_dts_pm_info(struct platform_device *pdev,
	struct ipa3_plat_drv_res *ipa_drv_res)
{
	int result;
	int i, j;

	ipa_drv_res->use_ipa_pm = of_property_read_bool(pdev->dev.of_node,
		"qcom,use-ipa-pm");
	IPADBG("use_ipa_pm=%d\n", ipa_drv_res->use_ipa_pm);
	if (!ipa_drv_res->use_ipa_pm)
		return 0;

	result = of_property_read_u32(pdev->dev.of_node,
		"qcom,msm-bus,num-cases",
		&ipa_drv_res->pm_init.threshold_size);
	/* No vote is ignored */
	ipa_drv_res->pm_init.threshold_size -= 2;
	if (result || ipa_drv_res->pm_init.threshold_size >
		IPA_PM_THRESHOLD_MAX) {
		IPAERR("invalid property qcom,msm-bus,num-cases %d\n",
			ipa_drv_res->pm_init.threshold_size);
		return -EFAULT;
	}

	result = of_property_read_u32_array(pdev->dev.of_node,
		"qcom,throughput-threshold",
		ipa_drv_res->pm_init.default_threshold,
		ipa_drv_res->pm_init.threshold_size);
	if (result) {
		IPAERR("failed to read qcom,throughput-thresholds\n");
		return -EFAULT;
	}

	result = of_property_count_strings(pdev->dev.of_node,
		"qcom,scaling-exceptions");
	if (result < 0) {
		IPADBG("no exception list for ipa pm\n");
		result = 0;
	}

	if (result % (ipa_drv_res->pm_init.threshold_size + 1)) {
		IPAERR("failed to read qcom,scaling-exceptions\n");
		return -EFAULT;
	}

	ipa_drv_res->pm_init.exception_size = result /
		(ipa_drv_res->pm_init.threshold_size + 1);
	if (ipa_drv_res->pm_init.exception_size >=
		IPA_PM_EXCEPTION_MAX) {
		IPAERR("exception list larger then max %d\n",
			ipa_drv_res->pm_init.exception_size);
		return -EFAULT;
	}

	for (i = 0; i < ipa_drv_res->pm_init.exception_size; i++) {
		struct ipa_pm_exception *ex = ipa_drv_res->pm_init.exceptions;

		result = of_property_read_string_index(pdev->dev.of_node,
			"qcom,scaling-exceptions",
			i * (ipa_drv_res->pm_init.threshold_size + 1),
			&ex[i].usecase);
		if (result) {
			IPAERR("failed to read qcom,scaling-exceptions");
			return -EFAULT;
		}

		for (j = 0; j < ipa_drv_res->pm_init.threshold_size; j++) {
			const char *str;

			result = of_property_read_string_index(
				pdev->dev.of_node,
				"qcom,scaling-exceptions",
				i * (ipa_drv_res->pm_init.threshold_size + 1)
				+ j + 1,
				&str);
			if (result) {
				IPAERR("failed to read qcom,scaling-exceptions"
					);
				return -EFAULT;
			}

			if (kstrtou32(str, 0, &ex[i].threshold[j])) {
				IPAERR("error str=%s\n", str);
				return -EFAULT;
			}
		}
	}

	return 0;
}

static int get_ipa_dts_configuration(struct platform_device *pdev,
		struct ipa3_plat_drv_res *ipa_drv_res)
{
	int i, result, pos;
	struct resource *resource;
	u32 *ipa_tz_unlock_reg;
	int elem_num;
	u32 mhi_evid_limits[2];

	/* initialize ipa3_res */
	ipa_drv_res->ipa_pipe_mem_start_ofst = IPA_PIPE_MEM_START_OFST;
	ipa_drv_res->ipa_pipe_mem_size = IPA_PIPE_MEM_SIZE;
	ipa_drv_res->ipa_hw_type = 0;
	ipa_drv_res->ipa3_hw_mode = 0;
	ipa_drv_res->platform_type = 0;
	ipa_drv_res->modem_cfg_emb_pipe_flt = false;
	ipa_drv_res->ipa_wdi2 = false;
	ipa_drv_res->ipa_wan_skb_page = false;
	ipa_drv_res->ipa_wdi2_over_gsi = false;
	ipa_drv_res->ipa_wdi3_over_gsi = false;
	ipa_drv_res->use_xbl_boot = false;
	ipa_drv_res->ipa_mhi_dynamic_config = false;
	ipa_drv_res->use_64_bit_dma_mask = false;
	ipa_drv_res->use_bw_vote = false;
	ipa_drv_res->wan_rx_ring_size = IPA_GENERIC_RX_POOL_SZ;
	ipa_drv_res->lan_rx_ring_size = IPA_GENERIC_RX_POOL_SZ;
	ipa_drv_res->apply_rg10_wa = false;
	ipa_drv_res->gsi_ch20_wa = false;
	ipa_drv_res->ipa_tz_unlock_reg_num = 0;
	ipa_drv_res->ipa_tz_unlock_reg = NULL;
	ipa_drv_res->mhi_evid_limits[0] = IPA_MHI_GSI_EVENT_RING_ID_START;
	ipa_drv_res->mhi_evid_limits[1] = IPA_MHI_GSI_EVENT_RING_ID_END;
	ipa_drv_res->ipa_fltrt_not_hashable = false;
	ipa_drv_res->ipa_endp_delay_wa = false;

	/* Get IPA HW Version */
	result = of_property_read_u32(pdev->dev.of_node, "qcom,ipa-hw-ver",
					&ipa_drv_res->ipa_hw_type);
	if ((result) || (ipa_drv_res->ipa_hw_type == 0)) {
		IPAERR(":get resource failed for ipa-hw-ver\n");
		return -ENODEV;
	}
	IPADBG(": ipa_hw_type = %d", ipa_drv_res->ipa_hw_type);

	if (ipa_drv_res->ipa_hw_type < IPA_HW_v3_0) {
		IPAERR(":IPA version below 3.0 not supported\n");
		return -ENODEV;
	}

	if (ipa_drv_res->ipa_hw_type >= IPA_HW_MAX) {
		IPAERR(":IPA version is greater than the MAX\n");
		return -ENODEV;
	}

	/* Get IPA HW mode */
	result = of_property_read_u32(pdev->dev.of_node, "qcom,ipa-hw-mode",
			&ipa_drv_res->ipa3_hw_mode);
	if (result)
		IPADBG("using default (IPA_MODE_NORMAL) for ipa-hw-mode\n");
	else
		IPADBG(": found ipa_drv_res->ipa3_hw_mode = %d",
				ipa_drv_res->ipa3_hw_mode);

	/* Get Platform Type */
	result = of_property_read_u32(pdev->dev.of_node, "qcom,platform-type",
			&ipa_drv_res->platform_type);
	if (result)
		IPADBG("using default (IPA_PLAT_TYPE_MDM) for platform-type\n");
	else
		IPADBG(": found ipa_drv_res->platform_type = %d",
				ipa_drv_res->platform_type);

	/* Get IPA WAN / LAN RX pool size */
	result = of_property_read_u32(pdev->dev.of_node,
			"qcom,wan-rx-ring-size",
			&ipa_drv_res->wan_rx_ring_size);
	if (result)
		IPADBG("using default for wan-rx-ring-size = %u\n",
				ipa_drv_res->wan_rx_ring_size);
	else
		IPADBG(": found ipa_drv_res->wan-rx-ring-size = %u",
				ipa_drv_res->wan_rx_ring_size);

	result = of_property_read_u32(pdev->dev.of_node,
			"qcom,lan-rx-ring-size",
			&ipa_drv_res->lan_rx_ring_size);
	if (result)
		IPADBG("using default for lan-rx-ring-size = %u\n",
			ipa_drv_res->lan_rx_ring_size);
	else
		IPADBG(": found ipa_drv_res->lan-rx-ring-size = %u",
			ipa_drv_res->lan_rx_ring_size);

	ipa_drv_res->use_ipa_teth_bridge =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,use-ipa-tethering-bridge");
	IPADBG(": using ipa teth bridge = %s",
		ipa_drv_res->use_ipa_teth_bridge
		? "True" : "False");

	ipa_drv_res->ipa_mhi_dynamic_config =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,use-ipa-in-mhi-mode");
	IPADBG(": ipa_mhi_dynamic_config (%s)\n",
		ipa_drv_res->ipa_mhi_dynamic_config
		? "True" : "False");

	ipa_drv_res->modem_cfg_emb_pipe_flt =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,modem-cfg-emb-pipe-flt");
	IPADBG(": modem configure embedded pipe filtering = %s\n",
			ipa_drv_res->modem_cfg_emb_pipe_flt
			? "True" : "False");
	ipa_drv_res->ipa_wdi2_over_gsi =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,ipa-wdi2_over_gsi");
	IPADBG(": WDI-2.0 over gsi= %s\n",
			ipa_drv_res->ipa_wdi2_over_gsi
			? "True" : "False");
	ipa_drv_res->ipa_endp_delay_wa =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,ipa-endp-delay-wa");
	IPADBG(": endppoint delay wa = %s\n",
			ipa_drv_res->ipa_endp_delay_wa
			? "True" : "False");

	ipa_drv_res->ipa_wdi3_over_gsi =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,ipa-wdi3-over-gsi");
	IPADBG(": WDI-3.0 over gsi= %s\n",
			ipa_drv_res->ipa_wdi3_over_gsi
			? "True" : "False");

	ipa_drv_res->ipa_wdi2 =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,ipa-wdi2");
	IPADBG(": WDI-2.0 = %s\n",
			ipa_drv_res->ipa_wdi2
			? "True" : "False");

	ipa_drv_res->ipa_wan_skb_page =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,wan-use-skb-page");
	IPADBG(": Use skb page = %s\n",
			ipa_drv_res->ipa_wan_skb_page
			? "True" : "False");

	ipa_drv_res->ipa_fltrt_not_hashable =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,ipa-fltrt-not-hashable");
	IPADBG(": IPA filter/route rule hashable = %s\n",
			ipa_drv_res->ipa_fltrt_not_hashable
			? "True" : "False");

	ipa_drv_res->use_xbl_boot =
		of_property_read_bool(pdev->dev.of_node,
		"qcom,use-xbl-boot");
	IPADBG("Is xbl loading used ? (%s)\n",
		ipa_drv_res->use_xbl_boot ? "Yes":"No");

	ipa_drv_res->use_64_bit_dma_mask =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,use-64-bit-dma-mask");
	IPADBG(": use_64_bit_dma_mask = %s\n",
			ipa_drv_res->use_64_bit_dma_mask
			? "True" : "False");

	ipa_drv_res->use_bw_vote =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,bandwidth-vote-for-ipa");
	IPADBG(": use_bw_vote = %s\n",
			ipa_drv_res->use_bw_vote
			? "True" : "False");

	ipa_drv_res->skip_uc_pipe_reset =
		of_property_read_bool(pdev->dev.of_node,
		"qcom,skip-uc-pipe-reset");
	IPADBG(": skip uC pipe reset = %s\n",
		ipa_drv_res->skip_uc_pipe_reset
		? "True" : "False");

	ipa_drv_res->tethered_flow_control =
		of_property_read_bool(pdev->dev.of_node,
		"qcom,tethered-flow-control");
	IPADBG(": Use apps based flow control = %s\n",
		ipa_drv_res->tethered_flow_control
		? "True" : "False");

	ipa_drv_res->ipa_mhi_proxy =
		of_property_read_bool(pdev->dev.of_node,
		"qcom,ipa-mhi-proxy");
	IPADBG(": Use mhi proxy = %s\n",
		ipa_drv_res->ipa_mhi_proxy
		? "True" : "False");

	/* Get IPA wrapper address */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"ipa-base");
	if (!resource) {
		IPAERR(":get resource failed for ipa-base!\n");
		return -ENODEV;
	}
	ipa_drv_res->ipa_mem_base = resource->start;
	ipa_drv_res->ipa_mem_size = resource_size(resource);
	IPADBG(": ipa-base = 0x%x, size = 0x%x\n",
			ipa_drv_res->ipa_mem_base,
			ipa_drv_res->ipa_mem_size);

	smmu_info.ipa_base = ipa_drv_res->ipa_mem_base;
	smmu_info.ipa_size = ipa_drv_res->ipa_mem_size;

	/* Get IPA GSI address */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"gsi-base");
	if (!resource) {
		IPAERR(":get resource failed for gsi-base\n");
		return -ENODEV;
	}
	ipa_drv_res->transport_mem_base = resource->start;
	ipa_drv_res->transport_mem_size = resource_size(resource);
	IPADBG(": gsi-base = 0x%x, size = 0x%x\n",
			ipa_drv_res->transport_mem_base,
			ipa_drv_res->transport_mem_size);

	/* Get IPA GSI IRQ number */
	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"gsi-irq");
	if (!resource) {
		IPAERR(":get resource failed for gsi-irq\n");
		return -ENODEV;
	}
	ipa_drv_res->transport_irq = resource->start;
	IPADBG(": gsi-irq = %d\n", ipa_drv_res->transport_irq);

	/* Get IPA pipe mem start ofst */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"ipa-pipe-mem");
	if (!resource) {
		IPADBG(":not using pipe memory - resource nonexisting\n");
	} else {
		ipa_drv_res->ipa_pipe_mem_start_ofst = resource->start;
		ipa_drv_res->ipa_pipe_mem_size = resource_size(resource);
		IPADBG(":using pipe memory - at 0x%x of size 0x%x\n",
				ipa_drv_res->ipa_pipe_mem_start_ofst,
				ipa_drv_res->ipa_pipe_mem_size);
	}

	/* Get IPA IRQ number */
	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"ipa-irq");
	if (!resource) {
		IPAERR(":get resource failed for ipa-irq\n");
		return -ENODEV;
	}
	ipa_drv_res->ipa_irq = resource->start;
	IPADBG(":ipa-irq = %d\n", ipa_drv_res->ipa_irq);

	result = of_property_read_u32(pdev->dev.of_node, "qcom,ee",
			&ipa_drv_res->ee);
	if (result)
		ipa_drv_res->ee = 0;
	IPADBG(":ee = %u\n", ipa_drv_res->ee);

	ipa_drv_res->apply_rg10_wa =
		of_property_read_bool(pdev->dev.of_node,
		"qcom,use-rg10-limitation-mitigation");
	IPADBG(": Use Register Group 10 limitation mitigation = %s\n",
		ipa_drv_res->apply_rg10_wa
		? "True" : "False");

	ipa_drv_res->gsi_ch20_wa =
		of_property_read_bool(pdev->dev.of_node,
		"qcom,do-not-use-ch-gsi-20");
	IPADBG(": GSI CH 20 WA is = %s\n",
		ipa_drv_res->gsi_ch20_wa
		? "Needed" : "Not needed");

	elem_num = of_property_count_elems_of_size(pdev->dev.of_node,
		"qcom,mhi-event-ring-id-limits", sizeof(u32));

	if (elem_num == 2) {
		if (of_property_read_u32_array(pdev->dev.of_node,
			"qcom,mhi-event-ring-id-limits", mhi_evid_limits, 2)) {
			IPAERR("failed to read mhi event ring id limits\n");
			return -EFAULT;
		}
		if (mhi_evid_limits[0] > mhi_evid_limits[1]) {
			IPAERR("mhi event ring id low limit > high limit\n");
			return -EFAULT;
		}
		ipa_drv_res->mhi_evid_limits[0] = mhi_evid_limits[0];
		ipa_drv_res->mhi_evid_limits[1] = mhi_evid_limits[1];
		IPADBG(": mhi-event-ring-id-limits start=%u end=%u\n",
			mhi_evid_limits[0], mhi_evid_limits[1]);
	} else {
		if (elem_num > 0) {
			IPAERR("Invalid mhi event ring id limits number %d\n",
				elem_num);
			return -EINVAL;
		}
		IPADBG("use default mhi evt ring id limits start=%u end=%u\n",
			ipa_drv_res->mhi_evid_limits[0],
			ipa_drv_res->mhi_evid_limits[1]);
	}

	elem_num = of_property_count_elems_of_size(pdev->dev.of_node,
		"qcom,ipa-tz-unlock-reg", sizeof(u32));

	if (elem_num > 0 && elem_num % 2 == 0) {
		ipa_drv_res->ipa_tz_unlock_reg_num = elem_num / 2;

		ipa_tz_unlock_reg = kcalloc(elem_num, sizeof(u32), GFP_KERNEL);
		if (ipa_tz_unlock_reg == NULL)
			return -ENOMEM;

		ipa_drv_res->ipa_tz_unlock_reg = kcalloc(
			ipa_drv_res->ipa_tz_unlock_reg_num,
			sizeof(*ipa_drv_res->ipa_tz_unlock_reg),
			GFP_KERNEL);
		if (ipa_drv_res->ipa_tz_unlock_reg == NULL) {
			kfree(ipa_tz_unlock_reg);
			return -ENOMEM;
		}

		if (of_property_read_u32_array(pdev->dev.of_node,
			"qcom,ipa-tz-unlock-reg", ipa_tz_unlock_reg,
			elem_num)) {
			IPAERR("failed to read register addresses\n");
			kfree(ipa_tz_unlock_reg);
			kfree(ipa_drv_res->ipa_tz_unlock_reg);
			return -EFAULT;
		}

		pos = 0;
		for (i = 0; i < ipa_drv_res->ipa_tz_unlock_reg_num; i++) {
			ipa_drv_res->ipa_tz_unlock_reg[i].reg_addr =
				ipa_tz_unlock_reg[pos++];
			ipa_drv_res->ipa_tz_unlock_reg[i].size =
				ipa_tz_unlock_reg[pos++];
			IPADBG("tz unlock reg %d: addr 0x%pa size %llu\n", i,
				&ipa_drv_res->ipa_tz_unlock_reg[i].reg_addr,
				ipa_drv_res->ipa_tz_unlock_reg[i].size);
		}
		kfree(ipa_tz_unlock_reg);
	}

	/* get IPA PM related information */
	result = get_ipa_dts_pm_info(pdev, ipa_drv_res);
	if (result) {
		IPAERR("failed to get pm info from dts %d\n", result);
		return result;
	}

	ipa_drv_res->wdi_over_pcie =
		of_property_read_bool(pdev->dev.of_node,
		"qcom,wlan-ce-db-over-pcie");
	IPADBG("Is wdi_over_pcie ? (%s)\n",
		ipa_drv_res->wdi_over_pcie ? "Yes":"No");

	/*
	 * If we're on emulator, get its interrupt controller's mem
	 * start and size
	 */
	if (ipa_drv_res->ipa3_hw_mode == IPA_HW_MODE_EMULATION) {
		resource = platform_get_resource_byname(
		    pdev, IORESOURCE_MEM, "intctrl-base");
		if (!resource) {
			IPAERR(":Can't find intctrl-base resource\n");
			return -ENODEV;
		}
		ipa_drv_res->emulator_intcntrlr_mem_base =
		    resource->start;
		ipa_drv_res->emulator_intcntrlr_mem_size =
		    resource_size(resource);
		IPADBG(":using intctrl-base at 0x%x of size 0x%x\n",
		       ipa_drv_res->emulator_intcntrlr_mem_base,
		       ipa_drv_res->emulator_intcntrlr_mem_size);
	}

	result = of_property_read_u32(pdev->dev.of_node,
				      "qcom,entire-ipa-block-size",
				      &ipa_drv_res->entire_ipa_block_size);
	if (result || ipa_drv_res->entire_ipa_block_size == 0)
		ipa_drv_res->entire_ipa_block_size = 0x100000;

	IPADBG(": entire_ipa_block_size = %d\n",
	       ipa_drv_res->entire_ipa_block_size);

	/*
	 * We'll read register-collection-on-crash here, but log it
	 * later below because its value may change based on other
	 * subsequent dtsi reads......
	 */
	ipa_drv_res->do_register_collection_on_crash =
	    of_property_read_bool(pdev->dev.of_node,
				  "qcom,register-collection-on-crash");
	/*
	 * We'll read testbus-collection-on-crash here...
	 */
	ipa_drv_res->do_testbus_collection_on_crash =
	    of_property_read_bool(pdev->dev.of_node,
				  "qcom,testbus-collection-on-crash");
	IPADBG(": doing testbus collection on crash = %s\n",
	       ipa_drv_res->do_testbus_collection_on_crash ? "True":"False");

	if (ipa_drv_res->do_testbus_collection_on_crash)
		ipa_drv_res->do_register_collection_on_crash = true;

	/*
	 * We'll read non-tn-collection-on-crash here...
	 */
	ipa_drv_res->do_non_tn_collection_on_crash =
	    of_property_read_bool(pdev->dev.of_node,
				  "qcom,non-tn-collection-on-crash");
	IPADBG(": doing non-tn collection on crash = %s\n",
	       ipa_drv_res->do_non_tn_collection_on_crash ? "True":"False");

	if (ipa_drv_res->do_non_tn_collection_on_crash)
		ipa_drv_res->do_register_collection_on_crash = true;

	IPADBG(": doing register collection on crash = %s\n",
	       ipa_drv_res->do_register_collection_on_crash ? "True":"False");

	result = of_property_read_u32(
		pdev->dev.of_node,
		"qcom,secure-debug-check-action",
		&ipa_drv_res->secure_debug_check_action);
	if (result ||
		(ipa_drv_res->secure_debug_check_action != 0 &&
		 ipa_drv_res->secure_debug_check_action != 1 &&
		 ipa_drv_res->secure_debug_check_action != 2))
		ipa_drv_res->secure_debug_check_action = USE_SCM;

	IPADBG(": secure-debug-check-action = %d\n",
		   ipa_drv_res->secure_debug_check_action);

	return 0;
}

static int ipa_smmu_wlan_cb_probe(struct device *dev)
{
	struct ipa_smmu_cb_ctx *cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_WLAN);
	int atomic_ctx = 1;
	int fast = 1;
	int bypass = 1;
	int ret;
	u32 add_map_size;
	const u32 *add_map;
	int i;

	IPADBG("sub pdev=%pK\n", dev);

	if (!smmu_info.present[IPA_SMMU_CB_WLAN]) {
		IPAERR("WLAN SMMU is disabled\n");
		return 0;
	}

	cb->dev = dev;
	cb->iommu = iommu_domain_alloc(dev->bus);
	if (!cb->iommu) {
		IPAERR("could not alloc iommu domain\n");
		/* assume this failure is because iommu driver is not ready */
		return -EPROBE_DEFER;
	}

	cb->is_cache_coherent = of_property_read_bool(dev->of_node,
							"dma-coherent");
	cb->valid = true;

	if (of_property_read_bool(dev->of_node, "qcom,smmu-s1-bypass") ||
		ipa3_ctx->ipa_config_is_mhi) {
		smmu_info.s1_bypass_arr[IPA_SMMU_CB_WLAN] = true;
		ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_WLAN] = true;
		cb->is_cache_coherent = false;

		if (iommu_domain_set_attr(cb->iommu,
					DOMAIN_ATTR_S1_BYPASS,
					&bypass)) {
			IPAERR("couldn't set bypass\n");
			cb->valid = false;
			return -EIO;
		}
		IPADBG("WLAN SMMU S1 BYPASS\n");
	} else {
		smmu_info.s1_bypass_arr[IPA_SMMU_CB_WLAN] = false;
		ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_WLAN] = false;

		if (iommu_domain_set_attr(cb->iommu,
					DOMAIN_ATTR_ATOMIC,
					&atomic_ctx)) {
			IPAERR("couldn't disable coherent HTW\n");
			cb->valid = false;
			return -EIO;
		}
		IPADBG(" WLAN SMMU ATTR ATOMIC\n");

		if (smmu_info.fast_map) {
			if (iommu_domain_set_attr(cb->iommu,
						DOMAIN_ATTR_FAST,
						&fast)) {
				IPAERR("couldn't set fast map\n");
				cb->valid = false;
				return -EIO;
			}
			IPADBG("SMMU fast map set\n");
		}
	}

	pr_info("IPA smmu_info.s1_bypass_arr[WLAN]=%d smmu_info.fast_map=%d\n",
		smmu_info.s1_bypass_arr[IPA_SMMU_CB_WLAN], smmu_info.fast_map);

	ret = iommu_attach_device(cb->iommu, dev);
	if (ret) {
		IPAERR("could not attach device ret=%d\n", ret);
		cb->valid = false;
		return ret;
	}
	/* MAP ipa-uc ram */
	add_map = of_get_property(dev->of_node,
		"qcom,additional-mapping", &add_map_size);
	if (add_map) {
		/* mapping size is an array of 3-tuple of u32 */
		if (add_map_size % (3 * sizeof(u32))) {
			IPAERR("wrong additional mapping format\n");
			cb->valid = false;
			return -EFAULT;
		}

		/* iterate of each entry of the additional mapping array */
		for (i = 0; i < add_map_size / sizeof(u32); i += 3) {
			u32 iova = be32_to_cpu(add_map[i]);
			u32 pa = be32_to_cpu(add_map[i + 1]);
			u32 size = be32_to_cpu(add_map[i + 2]);
			unsigned long iova_p;
			phys_addr_t pa_p;
			u32 size_p;

			IPA_SMMU_ROUND_TO_PAGE(iova, pa, size,
				iova_p, pa_p, size_p);
			IPADBG_LOW("mapping 0x%lx to 0x%pa size %d\n",
				iova_p, &pa_p, size_p);
			ipa3_iommu_map(cb->iommu,
				iova_p, pa_p, size_p,
				IOMMU_READ | IOMMU_WRITE | IOMMU_MMIO);
		}
	}
	return 0;
}

static int ipa_smmu_uc_cb_probe(struct device *dev)
{
	struct ipa_smmu_cb_ctx *cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_UC);
	int atomic_ctx = 1;
	int bypass = 1;
	int fast = 1;
	int ret;
	u32 iova_ap_mapping[2];
	/* G_RD_CNTR register */
	u32 a1 = 0x0C220000;
	u32 a2 = 0x4000;
	unsigned long iova_p;
	phys_addr_t pa_p;
	u32 size_p;

	IPADBG("UC CB PROBE sub pdev=%pK\n", dev);

	if (!smmu_info.present[IPA_SMMU_CB_UC]) {
		IPAERR("UC SMMU is disabled\n");
		return 0;
	}

	ret = of_property_read_u32_array(dev->of_node, "qcom,iova-mapping",
			iova_ap_mapping, 2);
	if (ret) {
		IPAERR("Fail to read UC start/size iova addresses\n");
		return ret;
	}
	cb->va_start = iova_ap_mapping[0];
	cb->va_size = iova_ap_mapping[1];
	cb->va_end = cb->va_start + cb->va_size;
	IPADBG("UC va_start=0x%x va_sise=0x%x\n", cb->va_start, cb->va_size);

	if (smmu_info.use_64_bit_dma_mask) {
		if (dma_set_mask(dev, DMA_BIT_MASK(64)) ||
				dma_set_coherent_mask(dev, DMA_BIT_MASK(64))) {
			IPAERR("DMA set 64bit mask failed\n");
			return -EOPNOTSUPP;
		}
	} else {
		if (dma_set_mask(dev, DMA_BIT_MASK(32)) ||
				dma_set_coherent_mask(dev, DMA_BIT_MASK(32))) {
			IPAERR("DMA set 32bit mask failed\n");
			return -EOPNOTSUPP;
		}
	}
	IPADBG("UC CB PROBE=%pK create IOMMU mapping\n", dev);

	cb->dev = dev;
	cb->mapping = arm_iommu_create_mapping(dev->bus,
			cb->va_start, cb->va_size);
	if (IS_ERR_OR_NULL(cb->mapping)) {
		IPADBG("Fail to create mapping\n");
		/* assume this failure is because iommu driver is not ready */
		return -EPROBE_DEFER;
	}
	cb->is_cache_coherent = of_property_read_bool(dev->of_node,
							"dma-coherent");
	IPADBG("SMMU mapping created\n");
	cb->valid = true;

	IPADBG("UC CB PROBE sub pdev=%pK set attribute\n", dev);

	if (of_property_read_bool(dev->of_node, "qcom,smmu-s1-bypass") ||
		ipa3_ctx->ipa_config_is_mhi) {
		smmu_info.s1_bypass_arr[IPA_SMMU_CB_UC] = true;
		ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_UC] = true;
		cb->is_cache_coherent = false;

		if (iommu_domain_set_attr(cb->mapping->domain,
			DOMAIN_ATTR_S1_BYPASS,
			&bypass)) {
			IPAERR("couldn't set bypass\n");
			arm_iommu_release_mapping(cb->mapping);
			cb->valid = false;
			return -EIO;
		}
		IPADBG("UC SMMU S1 BYPASS\n");
	} else {
		smmu_info.s1_bypass_arr[IPA_SMMU_CB_UC] = false;
		ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_UC] = false;

		if (iommu_domain_set_attr(cb->mapping->domain,
			DOMAIN_ATTR_ATOMIC,
			&atomic_ctx)) {
			IPAERR("couldn't set domain as atomic\n");
			arm_iommu_release_mapping(cb->mapping);
			cb->valid = false;
			return -EIO;
		}
		IPADBG("SMMU atomic set\n");

		if (smmu_info.fast_map) {
			if (iommu_domain_set_attr(cb->mapping->domain,
				DOMAIN_ATTR_FAST,
				&fast)) {
				IPAERR("couldn't set fast map\n");
				arm_iommu_release_mapping(cb->mapping);
				cb->valid = false;
				return -EIO;
			}
			IPADBG("SMMU fast map set\n");
		}
	}

	pr_info("IPA smmu_info.s1_bypass_arr[UC]=%d smmu_info.fast_map=%d\n",
		smmu_info.s1_bypass_arr[IPA_SMMU_CB_UC], smmu_info.fast_map);

	IPADBG("UC CB PROBE sub pdev=%pK attaching IOMMU device\n", dev);
	ret = arm_iommu_attach_device(cb->dev, cb->mapping);
	if (ret) {
		IPAERR("could not attach device ret=%d\n", ret);
		arm_iommu_release_mapping(cb->mapping);
		cb->valid = false;
		return ret;
	}

	/* map G_RD_CNTR for uc*/
	IPA_SMMU_ROUND_TO_PAGE(a1, a1, a2,
		iova_p, pa_p, size_p);

	if (ipa3_ctx->ipa_hw_type == IPA_HW_v4_1) {
		IPADBG_LOW("mapping 0x%lx to 0x%pa size %d\n",
			iova_p, &pa_p, size_p);
		ipa3_iommu_map(cb->mapping->domain,
			iova_p, pa_p, size_p,
			IOMMU_READ | IOMMU_WRITE | IOMMU_MMIO);
	}

	cb->next_addr = cb->va_end;
	ipa3_ctx->uc_pdev = dev;

	return 0;
}

static int ipa_smmu_ap_cb_probe(struct device *dev)
{
	struct ipa_smmu_cb_ctx *cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_AP);
	int result;
	int atomic_ctx = 1;
	int fast = 1;
	int bypass = 1;
	u32 iova_ap_mapping[2];
	u32 add_map_size;
	const u32 *add_map;
	void *smem_addr;
	size_t smem_size;
	u32 ipa_smem_size = 0;
	int ret;
	int i;
	unsigned long iova_p;
	phys_addr_t pa_p;
	u32 size_p;
	phys_addr_t iova;
	phys_addr_t pa;

	IPADBG("AP CB probe: sub pdev=%pK\n", dev);

	if (!smmu_info.present[IPA_SMMU_CB_AP]) {
		IPAERR("AP SMMU is disabled");
		return 0;
	}

	result = of_property_read_u32_array(dev->of_node, "qcom,iova-mapping",
		iova_ap_mapping, 2);
	if (result) {
		IPAERR("Fail to read AP start/size iova addresses\n");
		return result;
	}
	cb->va_start = iova_ap_mapping[0];
	cb->va_size = iova_ap_mapping[1];
	cb->va_end = cb->va_start + cb->va_size;
	IPADBG("AP va_start=0x%x va_sise=0x%x\n", cb->va_start, cb->va_size);

	if (smmu_info.use_64_bit_dma_mask) {
		if (dma_set_mask(dev, DMA_BIT_MASK(64)) ||
				dma_set_coherent_mask(dev, DMA_BIT_MASK(64))) {
			IPAERR("DMA set 64bit mask failed\n");
			return -EOPNOTSUPP;
		}
	} else {
		if (dma_set_mask(dev, DMA_BIT_MASK(32)) ||
				dma_set_coherent_mask(dev, DMA_BIT_MASK(32))) {
			IPAERR("DMA set 32bit mask failed\n");
			return -EOPNOTSUPP;
		}
	}

	cb->is_cache_coherent = of_property_read_bool(dev->of_node,
							"dma-coherent");
	cb->dev = dev;
	cb->mapping = arm_iommu_create_mapping(dev->bus,
					cb->va_start, cb->va_size);
	if (IS_ERR_OR_NULL(cb->mapping)) {
		IPADBG("Fail to create mapping\n");
		/* assume this failure is because iommu driver is not ready */
		return -EPROBE_DEFER;
	}
	IPADBG("SMMU mapping created\n");
	cb->valid = true;

	if (of_property_read_bool(dev->of_node,
		"qcom,smmu-s1-bypass") || ipa3_ctx->ipa_config_is_mhi) {
		smmu_info.s1_bypass_arr[IPA_SMMU_CB_AP] = true;
		ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_AP] = true;
		cb->is_cache_coherent = false;

		if (iommu_domain_set_attr(cb->mapping->domain,
				DOMAIN_ATTR_S1_BYPASS,
				&bypass)) {
			IPAERR("couldn't set bypass\n");
			arm_iommu_release_mapping(cb->mapping);
			cb->valid = false;
			return -EIO;
		}
		IPADBG("AP/USB SMMU S1 BYPASS\n");
	} else {
		smmu_info.s1_bypass_arr[IPA_SMMU_CB_AP] = false;
		ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_AP] = false;
		if (iommu_domain_set_attr(cb->mapping->domain,
				DOMAIN_ATTR_ATOMIC,
				&atomic_ctx)) {
			IPAERR("couldn't set domain as atomic\n");
			arm_iommu_release_mapping(cb->mapping);
			cb->valid = false;
			return -EIO;
		}
		IPADBG("AP/USB SMMU atomic set\n");

		if (smmu_info.fast_map) {
			if (iommu_domain_set_attr(cb->mapping->domain,
				DOMAIN_ATTR_FAST,
				&fast)) {
				IPAERR("couldn't set fast map\n");
				arm_iommu_release_mapping(cb->mapping);
				cb->valid = false;
				return -EIO;
			}
			IPADBG("SMMU fast map set\n");
		}
	}

	pr_info("IPA smmu_info.s1_bypass_arr[AP]=%d smmu_info.fast_map=%d\n",
		smmu_info.s1_bypass_arr[IPA_SMMU_CB_AP], smmu_info.fast_map);

	result = arm_iommu_attach_device(cb->dev, cb->mapping);
	if (result) {
		IPAERR("couldn't attach to IOMMU ret=%d\n", result);
		cb->valid = false;
		return result;
	}

	add_map = of_get_property(dev->of_node,
		"qcom,additional-mapping", &add_map_size);
	if (add_map) {
		/* mapping size is an array of 3-tuple of u32 */
		if (add_map_size % (3 * sizeof(u32))) {
			IPAERR("wrong additional mapping format\n");
			cb->valid = false;
			return -EFAULT;
		}

		/* iterate of each entry of the additional mapping array */
		for (i = 0; i < add_map_size / sizeof(u32); i += 3) {
			u32 iova = be32_to_cpu(add_map[i]);
			u32 pa = be32_to_cpu(add_map[i + 1]);
			u32 size = be32_to_cpu(add_map[i + 2]);
			unsigned long iova_p;
			phys_addr_t pa_p;
			u32 size_p;

			IPA_SMMU_ROUND_TO_PAGE(iova, pa, size,
				iova_p, pa_p, size_p);
			IPADBG_LOW("mapping 0x%lx to 0x%pa size %d\n",
				iova_p, &pa_p, size_p);
			ipa3_iommu_map(cb->mapping->domain,
				iova_p, pa_p, size_p,
				IOMMU_READ | IOMMU_WRITE | IOMMU_MMIO);
		}
	}

	ret = of_property_read_u32(dev->of_node, "qcom,ipa-q6-smem-size",
					&ipa_smem_size);
	if (ret) {
		IPADBG("ipa q6 smem size (default) = %u\n", IPA_SMEM_SIZE);
		ipa_smem_size = IPA_SMEM_SIZE;
	} else {
		IPADBG("ipa q6 smem size = %u\n", ipa_smem_size);
	}

	if (ipa3_ctx->platform_type != IPA_PLAT_TYPE_APQ) {
		/* map SMEM memory for IPA table accesses */
		ret = qcom_smem_alloc(SMEM_MODEM,
			SMEM_IPA_FILTER_TABLE,
			ipa_smem_size);

		if (ret < 0 && ret != -EEXIST) {
			IPAERR("unable to allocate smem MODEM entry\n");
			cb->valid = false;
			return -EFAULT;
		}
		smem_addr = qcom_smem_get(SMEM_MODEM,
			SMEM_IPA_FILTER_TABLE,
			&smem_size);
		if (IS_ERR(smem_addr)) {
			IPAERR("unable to acquire smem MODEM entry\n");
			cb->valid = false;
			return -EFAULT;
		}
		if (smem_size != ipa_smem_size)
			IPAERR("unexpected read q6 smem size %zu %u\n",
				smem_size, ipa_smem_size);

		iova = qcom_smem_virt_to_phys(smem_addr);
		pa = iova;

		IPA_SMMU_ROUND_TO_PAGE(iova, pa, ipa_smem_size,
					iova_p, pa_p, size_p);
				IPADBG_LOW("mapping 0x%lx to 0x%pa size %d\n",
					iova_p, &pa_p, size_p);
				ipa3_iommu_map(cb->mapping->domain,
					iova_p, pa_p, size_p,
					IOMMU_READ | IOMMU_WRITE);
	}

	smmu_info.present[IPA_SMMU_CB_AP] = true;
	ipa3_ctx->pdev = dev;
	cb->next_addr = cb->va_end;

	return 0;
}

static int ipa_smmu_cb_probe(struct device *dev, enum ipa_smmu_cb_type cb_type)
{
	switch (cb_type) {
	case IPA_SMMU_CB_AP:
		return ipa_smmu_ap_cb_probe(dev);
	case IPA_SMMU_CB_WLAN:
		return ipa_smmu_wlan_cb_probe(dev);
	case IPA_SMMU_CB_UC:
		return ipa_smmu_uc_cb_probe(dev);
	case IPA_SMMU_CB_MAX:
		IPAERR("Invalid cb_type\n");
	}
	return 0;
}

static int ipa3_attach_to_smmu(void)
{
	struct ipa_smmu_cb_ctx *cb;
	int i, result;

	ipa3_ctx->pdev = &ipa3_ctx->master_pdev->dev;
	ipa3_ctx->uc_pdev = &ipa3_ctx->master_pdev->dev;

	if (smmu_info.arm_smmu) {
		IPADBG("smmu is enabled\n");
		for (i = 0; i < IPA_SMMU_CB_MAX; i++) {
			cb = ipa3_get_smmu_ctx(i);
			result = ipa_smmu_cb_probe(cb->dev, i);
			if (result)
				IPAERR("probe failed for cb %d\n", i);
		}
	} else {
		IPADBG("smmu is disabled\n");
	}
	return 0;
}

static irqreturn_t ipa3_smp2p_modem_clk_query_isr(int irq, void *ctxt)
{
	ipa3_freeze_clock_vote_and_notify_modem();

	return IRQ_HANDLED;
}

static int ipa3_smp2p_probe(struct device *dev)
{
	struct device_node *node = dev->of_node;
	int res;
	int irq = 0;

	if (ipa3_ctx == NULL) {
		IPAERR("ipa3_ctx was not initialized\n");
		return -EPROBE_DEFER;
	}
	IPADBG("node->name=%s\n", node->name);
	if (ipa3_ctx->platform_type == IPA_PLAT_TYPE_APQ) {
		IPADBG("Ignore smp2p on APQ platform\n");
		return 0;
	}

	if (strcmp("qcom,smp2p_map_ipa_1_out", node->name) == 0) {
		if (of_find_property(node, "qcom,smem-states", NULL)) {
			ipa3_ctx->smp2p_info.smem_state =
			qcom_smem_state_get(dev, "ipa-smp2p-out",
			&ipa3_ctx->smp2p_info.smem_bit);
			if (IS_ERR(ipa3_ctx->smp2p_info.smem_state)) {
				IPAERR("fail to get smp2p clk resp bit %ld\n",
				PTR_ERR(ipa3_ctx->smp2p_info.smem_state));
				return PTR_ERR(ipa3_ctx->smp2p_info.smem_state);
			}
			IPADBG("smem_bit=%d\n", ipa3_ctx->smp2p_info.smem_bit);
		}
	} else if (strcmp("qcom,smp2p_map_ipa_1_in", node->name) == 0) {
		res = irq = of_irq_get_byname(node, "ipa-smp2p-in");
		if (res < 0) {
			IPADBG("of_irq_get_byname returned %d\n", irq);
			return res;
		}

		ipa3_ctx->smp2p_info.in_base_id = irq;
		IPADBG("smp2p irq#=%d\n", irq);
		res = devm_request_threaded_irq(dev, irq, NULL,
			(irq_handler_t)ipa3_smp2p_modem_clk_query_isr,
			IRQF_TRIGGER_RISING, "ipa_smp2p_clk_vote", dev);
		if (res) {
			IPAERR("fail to register smp2p irq=%d\n", irq);
			return -ENODEV;
		}
	}
	return 0;
}

int ipa3_plat_drv_probe(struct platform_device *pdev_p,
	struct ipa_api_controller *api_ctrl,
	const struct of_device_id *pdrv_match)
{
	int result;
	struct device *dev = &pdev_p->dev;
	struct ipa_smmu_cb_ctx *cb;

	IPADBG("IPA driver probing started\n");
	IPADBG("dev->of_node->name = %s\n", dev->of_node->name);

	if (of_device_is_compatible(dev->of_node, "qcom,ipa-smmu-ap-cb")) {
		if (ipa3_ctx == NULL) {
			IPAERR("ipa3_ctx was not initialized\n");
			return -EPROBE_DEFER;
		}
		cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_AP);
		cb->dev = dev;
		smmu_info.present[IPA_SMMU_CB_AP] = true;

		return 0;
	}

	if (of_device_is_compatible(dev->of_node, "qcom,ipa-smmu-wlan-cb")) {
		if (ipa3_ctx == NULL) {
			IPAERR("ipa3_ctx was not initialized\n");
			return -EPROBE_DEFER;
		}
		cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_WLAN);
		cb->dev = dev;
		smmu_info.present[IPA_SMMU_CB_WLAN] = true;

		return 0;
	}

	if (of_device_is_compatible(dev->of_node, "qcom,ipa-smmu-uc-cb")) {
		if (ipa3_ctx == NULL) {
			IPAERR("ipa3_ctx was not initialized\n");
			return -EPROBE_DEFER;
		}
		cb =  ipa3_get_smmu_ctx(IPA_SMMU_CB_UC);
		cb->dev = dev;
		smmu_info.present[IPA_SMMU_CB_UC] = true;

		if (ipa3_ctx->use_xbl_boot && (gsi_is_mcs_enabled() == 1)) {
			/* Ensure uC probe is the last. */
			if (!smmu_info.present[IPA_SMMU_CB_AP] ||
				!smmu_info.present[IPA_SMMU_CB_WLAN]) {
				IPAERR("AP or WLAN CB probe not done. Defer");
				return -EPROBE_DEFER;
			}

			pr_info("Using XBL boot load for IPA FW\n");
			ipa3_ctx->fw_loaded = true;

			result = ipa3_attach_to_smmu();
			if (result) {
				IPAERR("IPA attach to smmu failed %d\n",
				result);
				return result;
			}

			result = ipa3_post_init(&ipa3_res, ipa3_ctx->cdev.dev);
			if (result) {
				IPAERR("IPA post init failed %d\n", result);
				return result;
			}
		}

		return 0;
	}

	if (of_device_is_compatible(dev->of_node,
	    "qcom,smp2p-map-ipa-1-out"))
		return ipa3_smp2p_probe(dev);
	if (of_device_is_compatible(dev->of_node,
	    "qcom,smp2p-map-ipa-1-in"))
		return ipa3_smp2p_probe(dev);

	result = get_ipa_dts_configuration(pdev_p, &ipa3_res);
	if (result) {
		IPAERR("IPA dts parsing failed\n");
		return result;
	}

	result = ipa3_bind_api_controller(ipa3_res.ipa_hw_type, api_ctrl);
	if (result) {
		IPAERR("IPA API binding failed\n");
		return result;
	}

	if (of_property_read_bool(pdev_p->dev.of_node, "qcom,arm-smmu")) {
		if (of_property_read_bool(pdev_p->dev.of_node,
			"qcom,smmu-fast-map"))
			smmu_info.fast_map = true;
		if (of_property_read_bool(pdev_p->dev.of_node,
			"qcom,use-64-bit-dma-mask"))
			smmu_info.use_64_bit_dma_mask = true;
		smmu_info.arm_smmu = true;
	} else if (of_property_read_bool(pdev_p->dev.of_node,
				"qcom,msm-smmu")) {
		IPAERR("Legacy IOMMU not supported\n");
		result = -EOPNOTSUPP;
	} else {
		if (of_property_read_bool(pdev_p->dev.of_node,
			"qcom,use-64-bit-dma-mask")) {
			if (dma_set_mask(&pdev_p->dev, DMA_BIT_MASK(64)) ||
			    dma_set_coherent_mask(&pdev_p->dev,
			    DMA_BIT_MASK(64))) {
				IPAERR("DMA set 64bit mask failed\n");
				return -EOPNOTSUPP;
			}
		} else {
			if (dma_set_mask(&pdev_p->dev, DMA_BIT_MASK(32)) ||
			    dma_set_coherent_mask(&pdev_p->dev,
			    DMA_BIT_MASK(32))) {
				IPAERR("DMA set 32bit mask failed\n");
				return -EOPNOTSUPP;
			}
		}
	}

	/* Proceed to real initialization */
	result = ipa3_pre_init(&ipa3_res, pdev_p);
	if (result) {
		IPAERR("ipa3_init failed\n");
		return result;
	}

	result = of_platform_populate(pdev_p->dev.of_node,
		pdrv_match, NULL, &pdev_p->dev);
	if (result) {
		IPAERR("failed to populate platform\n");
		return result;
	}

	return result;
}

/**
 * ipa3_ap_suspend() - suspend callback for runtime_pm
 * @dev: pointer to device
 *
 * This callback will be invoked by the runtime_pm framework when an AP suspend
 * operation is invoked, usually by pressing a suspend button.
 *
 * Returns -EAGAIN to runtime_pm framework in case IPA is in use by AP.
 * This will postpone the suspend operation until IPA is no longer used by AP.
 */
int ipa3_ap_suspend(struct device *dev)
{
	int i;

	IPADBG("Enter...\n");

	/* In case there is a tx/rx handler in polling mode fail to suspend */
	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if (ipa3_ctx->ep[i].sys &&
			atomic_read(&ipa3_ctx->ep[i].sys->curr_polling_state)) {
			IPAERR("EP %d is in polling state, do not suspend\n",
				i);
			return -EAGAIN;
		}
	}

	if (ipa3_ctx->use_ipa_pm) {
		ipa_pm_deactivate_all_deferred();
	} else {
		/*
		 * Release transport IPA resource without waiting
		 * for inactivity timer
		 */
		atomic_set(&ipa3_ctx->transport_pm.eot_activity, 0);
		ipa3_transport_release_resource(NULL);
	}
	IPADBG("Exit\n");

	return 0;
}

/**
 * ipa3_ap_resume() - resume callback for runtime_pm
 * @dev: pointer to device
 *
 * This callback will be invoked by the runtime_pm framework when an AP resume
 * operation is invoked.
 *
 * Always returns 0 since resume should always succeed.
 */
int ipa3_ap_resume(struct device *dev)
{
	return 0;
}

struct ipa3_context *ipa3_get_ctx(void)
{
	return ipa3_ctx;
}

bool ipa3_get_lan_rx_napi(void)
{
	return false;
}

static void ipa_gsi_notify_cb(struct gsi_per_notify *notify)
{
	/*
	 * These values are reported by hardware. Any error indicates
	 * hardware unexpected state.
	 */
	switch (notify->evt_id) {
	case GSI_PER_EVT_GLOB_ERROR:
		IPAERR("Got GSI_PER_EVT_GLOB_ERROR\n");
		IPAERR("Err_desc = 0x%04x\n", notify->data.err_desc);
		break;
	case GSI_PER_EVT_GLOB_GP1:
		IPAERR("Got GSI_PER_EVT_GLOB_GP1\n");
		BUG();
		break;
	case GSI_PER_EVT_GLOB_GP2:
		IPAERR("Got GSI_PER_EVT_GLOB_GP2\n");
		BUG();
		break;
	case GSI_PER_EVT_GLOB_GP3:
		IPAERR("Got GSI_PER_EVT_GLOB_GP3\n");
		BUG();
		break;
	case GSI_PER_EVT_GENERAL_BREAK_POINT:
		IPAERR("Got GSI_PER_EVT_GENERAL_BREAK_POINT\n");
		break;
	case GSI_PER_EVT_GENERAL_BUS_ERROR:
		IPAERR("Got GSI_PER_EVT_GENERAL_BUS_ERROR\n");
		BUG();
		break;
	case GSI_PER_EVT_GENERAL_CMD_FIFO_OVERFLOW:
		IPAERR("Got GSI_PER_EVT_GENERAL_CMD_FIFO_OVERFLOW\n");
		BUG();
		break;
	case GSI_PER_EVT_GENERAL_MCS_STACK_OVERFLOW:
		IPAERR("Got GSI_PER_EVT_GENERAL_MCS_STACK_OVERFLOW\n");
		BUG();
		break;
	default:
		IPAERR("Received unexpected evt: %d\n",
			notify->evt_id);
		BUG();
	}
}

int ipa3_register_ipa_ready_cb(void (*ipa_ready_cb)(void *), void *user_data)
{
	struct ipa3_ready_cb_info *cb_info = NULL;

	/* check ipa3_ctx existed or not */
	if (!ipa3_ctx) {
		IPADBG("IPA driver haven't initialized\n");
		return -ENXIO;
	}
	mutex_lock(&ipa3_ctx->lock);
	if (ipa3_ctx->ipa_initialization_complete) {
		mutex_unlock(&ipa3_ctx->lock);
		IPADBG("IPA driver finished initialization already\n");
		return -EEXIST;
	}

	cb_info = kmalloc(sizeof(struct ipa3_ready_cb_info), GFP_KERNEL);
	if (!cb_info) {
		mutex_unlock(&ipa3_ctx->lock);
		return -ENOMEM;
	}

	cb_info->ready_cb = ipa_ready_cb;
	cb_info->user_data = user_data;

	list_add_tail(&cb_info->link, &ipa3_ctx->ipa_ready_cb_list);
	mutex_unlock(&ipa3_ctx->lock);

	return 0;
}

int ipa3_iommu_map(struct iommu_domain *domain,
	unsigned long iova, phys_addr_t paddr, size_t size, int prot)
{
	struct ipa_smmu_cb_ctx *ap_cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_AP);
	struct ipa_smmu_cb_ctx *uc_cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_UC);
	struct ipa_smmu_cb_ctx *wlan_cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_WLAN);

	IPADBG_LOW("domain =0x%pK iova 0x%lx\n", domain, iova);
	IPADBG_LOW("paddr =0x%pa size 0x%x\n", &paddr, (u32)size);

	/* make sure no overlapping */
	if (domain == ipa3_get_smmu_domain()) {
		if (iova >= ap_cb->va_start && iova < ap_cb->va_end) {
			IPAERR("iommu AP overlap addr 0x%lx\n", iova);
			ipa_assert();
			return -EFAULT;
		}
		if (ap_cb->is_cache_coherent)
			prot |= IOMMU_CACHE;
	} else if (domain == ipa3_get_wlan_smmu_domain()) {
		/* wlan is one time map */
		if (wlan_cb->is_cache_coherent)
			prot |= IOMMU_CACHE;
	} else if (domain == ipa3_get_uc_smmu_domain()) {
		if (iova >= uc_cb->va_start && iova < uc_cb->va_end) {
			IPAERR("iommu uC overlap addr 0x%lx\n", iova);
			ipa_assert();
			return -EFAULT;
		}
		if (uc_cb->is_cache_coherent)
			prot |= IOMMU_CACHE;
	} else {
		IPAERR("Unexpected domain 0x%pK\n", domain);
		ipa_assert();
		return -EFAULT;
	}
	/*
	 * IOMMU_CACHE is needed in prot to make the entries cachable
	 * if cache coherency is enabled in dtsi.
	 */
	return iommu_map(domain, iova, paddr, size, prot);
}

/**
 * ipa3_get_smmu_params()- Return the ipa3 smmu related params.
 */
int ipa3_get_smmu_params(struct ipa_smmu_in_params *in,
	struct ipa_smmu_out_params *out)
{
	bool is_smmu_enable = 0;

	if (out == NULL || in == NULL) {
		IPAERR("bad parms for Client SMMU out params\n");
		return -EINVAL;
	}

	if (!ipa3_ctx) {
		IPAERR("IPA not yet initialized\n");
		return -EINVAL;
	}

	switch (in->smmu_client) {
	case IPA_SMMU_WLAN_CLIENT:
		if (ipa3_ctx->ipa_wdi3_over_gsi)
			is_smmu_enable =
				!(ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_AP] |
				ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_WLAN]);
		else
			is_smmu_enable =
				!(ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_UC] |
				ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_WLAN]);
		break;
	case IPA_SMMU_AP_CLIENT:
		is_smmu_enable =
			!(ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_AP]);
		break;
	default:
		is_smmu_enable = 0;
		IPAERR("Trying to get illegal clients SMMU status");
		return -EINVAL;
	}

	out->smmu_enable = is_smmu_enable;

	return 0;
}

#define MAX_LEN 96

void ipa_pc_qmp_enable(void)
{
	char buf[MAX_LEN] = "{class: bcm, res: ipa_pc, val: 1}";
	struct qmp_pkt pkt;
	int ret = 0;
	struct ipa3_pc_mbox_data *mbox_data = &ipa3_ctx->pc_mbox;

	IPADBG("Enter\n");

	/* prepare the mailbox struct */
	mbox_data->mbox_client.dev = &ipa3_ctx->master_pdev->dev;
	mbox_data->mbox_client.tx_block = true;
	mbox_data->mbox_client.tx_tout = MBOX_TOUT_MS;
	mbox_data->mbox_client.knows_txdone = false;

	mbox_data->mbox = mbox_request_channel(&mbox_data->mbox_client, 0);
	if (IS_ERR(mbox_data->mbox)) {
		ret = PTR_ERR(mbox_data->mbox);
		if (ret != -EPROBE_DEFER)
			IPAERR("mailbox channel request failed, ret=%d\n", ret);

		return;
	}

	/* prepare the QMP packet to send */
	pkt.size = MAX_LEN;
	pkt.data = buf;

	/* send the QMP packet to AOP */
	ret = mbox_send_message(mbox_data->mbox, &pkt);
	if (ret < 0)
		IPAERR("qmp message send failed, ret=%d\n", ret);

	if (mbox_data->mbox) {
		mbox_free_channel(mbox_data->mbox);
		mbox_data->mbox = NULL;
	}
}

/**************************************************************
 *            PCIe Version
 *************************************************************/

int ipa3_pci_drv_probe(
	struct pci_dev            *pci_dev,
	struct ipa_api_controller *api_ctrl,
	const struct of_device_id *pdrv_match)
{
	int result;
	struct ipa3_plat_drv_res *ipa_drv_res;
	u32 bar0_offset;
	u32 mem_start;
	u32 mem_end;
	uint32_t bits;
	uint32_t ipa_start, gsi_start, intctrl_start;
	struct device *dev;
	static struct platform_device platform_dev;

	if (!pci_dev || !api_ctrl || !pdrv_match) {
		IPAERR(
		    "Bad arg: pci_dev (%pK) and/or api_ctrl (%pK) and/or pdrv_match (%pK)\n",
		    pci_dev, api_ctrl, pdrv_match);
		return -EOPNOTSUPP;
	}

	dev = &(pci_dev->dev);

	IPADBG("IPA PCI driver probing started\n");

	/*
	 * Follow PCI driver flow here.
	 * pci_enable_device:  Enables device and assigns resources
	 * pci_request_region:  Makes BAR0 address region usable
	 */
	result = pci_enable_device(pci_dev);
	if (result < 0) {
		IPAERR("pci_enable_device() failed\n");
		return -EOPNOTSUPP;
	}

	result = pci_request_region(pci_dev, 0, "IPA Memory");
	if (result < 0) {
		IPAERR("pci_request_region() failed\n");
		pci_disable_device(pci_dev);
		return -EOPNOTSUPP;
	}

	/*
	 * When in the PCI/emulation environment, &platform_dev is
	 * passed to get_ipa_dts_configuration(), but is unused, since
	 * all usages of it in the function are replaced by CPP
	 * relative to definitions in ipa_emulation_stubs.h.  Passing
	 * &platform_dev makes code validity tools happy.
	 */
	if (get_ipa_dts_configuration(&platform_dev, &ipa3_res) != 0) {
		IPAERR("get_ipa_dts_configuration() failed\n");
		pci_release_region(pci_dev, 0);
		pci_disable_device(pci_dev);
		return -EOPNOTSUPP;
	}

	ipa_drv_res = &ipa3_res;

	result =
		of_property_read_u32(NULL, "emulator-bar0-offset",
				     &bar0_offset);
	if (result) {
		IPAERR(":get resource failed for emulator-bar0-offset!\n");
		pci_release_region(pci_dev, 0);
		pci_disable_device(pci_dev);
		return -ENODEV;
	}
	IPADBG(":using emulator-bar0-offset 0x%08X\n", bar0_offset);

	ipa_start     = ipa_drv_res->ipa_mem_base;
	gsi_start     = ipa_drv_res->transport_mem_base;
	intctrl_start = ipa_drv_res->emulator_intcntrlr_mem_base;

	/*
	 * Where will we be inerrupted at?
	 */
	ipa_drv_res->emulator_irq = pci_dev->irq;
	IPADBG(
	    "EMULATION PCI_INTERRUPT_PIN(%u)\n",
	    ipa_drv_res->emulator_irq);

	/*
	 * Set the ipa_mem_base to the PCI base address of BAR0
	 */
	mem_start = pci_resource_start(pci_dev, 0);
	mem_end   = pci_resource_end(pci_dev, 0);

	IPADBG("PCI START                = 0x%x\n", mem_start);
	IPADBG("PCI END                  = 0x%x\n", mem_end);

	ipa_drv_res->ipa_mem_base = mem_start + bar0_offset;

	smmu_info.ipa_base = ipa_drv_res->ipa_mem_base;
	smmu_info.ipa_size = ipa_drv_res->ipa_mem_size;

	ipa_drv_res->transport_mem_base =
	    ipa_drv_res->ipa_mem_base + (gsi_start - ipa_start);

	ipa_drv_res->emulator_intcntrlr_mem_base =
	    ipa_drv_res->ipa_mem_base + (intctrl_start - ipa_start);

	IPADBG("ipa_mem_base                = 0x%x\n",
	       ipa_drv_res->ipa_mem_base);
	IPADBG("ipa_mem_size                = 0x%x\n",
	       ipa_drv_res->ipa_mem_size);

	IPADBG("transport_mem_base          = 0x%x\n",
	       ipa_drv_res->transport_mem_base);
	IPADBG("transport_mem_size          = 0x%x\n",
	       ipa_drv_res->transport_mem_size);

	IPADBG("emulator_intcntrlr_mem_base = 0x%x\n",
	       ipa_drv_res->emulator_intcntrlr_mem_base);
	IPADBG("emulator_intcntrlr_mem_size = 0x%x\n",
	       ipa_drv_res->emulator_intcntrlr_mem_size);

	result = ipa3_bind_api_controller(ipa_drv_res->ipa_hw_type, api_ctrl);
	if (result != 0) {
		IPAERR("ipa3_bind_api_controller() failed\n");
		pci_release_region(pci_dev, 0);
		pci_disable_device(pci_dev);
		return result;
	}

	bits = (ipa_drv_res->use_64_bit_dma_mask) ? 64 : 32;

	if (dma_set_mask(dev, DMA_BIT_MASK(bits)) != 0) {
		IPAERR("dma_set_mask(%pK, %u) failed\n", dev, bits);
		pci_release_region(pci_dev, 0);
		pci_disable_device(pci_dev);
		return -EOPNOTSUPP;
	}

	if (dma_set_coherent_mask(dev, DMA_BIT_MASK(bits)) != 0) {
		IPAERR("dma_set_coherent_mask(%pK, %u) failed\n", dev, bits);
		pci_release_region(pci_dev, 0);
		pci_disable_device(pci_dev);
		return -EOPNOTSUPP;
	}

	pci_set_master(pci_dev);

	memset(&platform_dev, 0, sizeof(platform_dev));
	platform_dev.dev = *dev;

	/* Proceed to real initialization */
	result = ipa3_pre_init(&ipa3_res, &platform_dev);
	if (result) {
		IPAERR("ipa3_init failed\n");
		pci_clear_master(pci_dev);
		pci_release_region(pci_dev, 0);
		pci_disable_device(pci_dev);
		return result;
	}

	return result;
}

/*
 * The following returns transport register memory location and
 * size...
 */
int ipa3_get_transport_info(
	phys_addr_t *phys_addr_ptr,
	unsigned long *size_ptr)
{
	if (!phys_addr_ptr || !size_ptr) {
		IPAERR("Bad arg: phys_addr_ptr(%pK) and/or size_ptr(%pK)\n",
		       phys_addr_ptr, size_ptr);
		return -EINVAL;
	}

	*phys_addr_ptr = ipa3_res.transport_mem_base;
	*size_ptr      = ipa3_res.transport_mem_size;

	return 0;
}
EXPORT_SYMBOL(ipa3_get_transport_info);

static uint emulation_type = IPA_HW_v4_0;

/*
 * The following returns emulation type...
 */
uint ipa3_get_emulation_type(void)
{
	return emulation_type;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA HW device driver");

/*
 * Module parameter. Invoke as follows:
 *     insmod ipat.ko emulation_type=[13|14|17|...|N]
 * Examples:
 *   insmod ipat.ko emulation_type=13 # for IPA 3.5.1
 *   insmod ipat.ko emulation_type=14 # for IPA 4.0
 *   insmod ipat.ko emulation_type=17 # for IPA 4.5
 *
 * NOTE: The emulation_type values need to come from: enum ipa_hw_type
 *
 */

module_param(emulation_type, uint, 0000);
MODULE_PARM_DESC(
	emulation_type,
	"emulation_type=N N can be 13 for IPA 3.5.1, 14 for IPA 4.0, 17 for IPA 4.5");
