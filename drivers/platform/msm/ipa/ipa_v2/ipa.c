/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
#include <linux/qcom_iommu.h>
#include <linux/time.h>
#include <linux/hashtable.h>
#include <linux/hash.h>
#include "ipa_i.h"
#include "../ipa_rm_i.h"

#define CREATE_TRACE_POINTS
#include "ipa_trace.h"

#define IPA_SUMMING_THRESHOLD (0x10)
#define IPA_PIPE_MEM_START_OFST (0x0)
#define IPA_PIPE_MEM_SIZE (0x0)
#define IPA_MOBILE_AP_MODE(x) (x == IPA_MODE_MOBILE_AP_ETH || \
			       x == IPA_MODE_MOBILE_AP_WAN || \
			       x == IPA_MODE_MOBILE_AP_WLAN)
#define IPA_CNOC_CLK_RATE (75 * 1000 * 1000UL)
#define IPA_A5_MUX_HEADER_LENGTH (8)
#define IPA_ROUTING_RULE_BYTE_SIZE (4)
#define IPA_BAM_CNFG_BITS_VALv1_1 (0x7FFFE004)
#define IPA_BAM_CNFG_BITS_VALv2_0 (0xFFFFE004)
#define IPA_STATUS_CLEAR_OFST (0x3f28)
#define IPA_STATUS_CLEAR_SIZE (32)

#define IPA_AGGR_MAX_STR_LENGTH (10)

#define CLEANUP_TAG_PROCESS_TIMEOUT 150

#define IPA2_ACTIVE_CLIENTS_TABLE_BUF_SIZE 2048

#define IPA2_ACTIVE_CLIENT_LOG_TYPE_EP 0
#define IPA2_ACTIVE_CLIENT_LOG_TYPE_SIMPLE 1
#define IPA2_ACTIVE_CLIENT_LOG_TYPE_RESOURCE 2
#define IPA2_ACTIVE_CLIENT_LOG_TYPE_SPECIAL 3

#define MAX_POLLING_ITERATION 40
#define MIN_POLLING_ITERATION 1
#define ONE_MSEC 1

#define IPA_AGGR_STR_IN_BYTES(str) \
	(strnlen((str), IPA_AGGR_MAX_STR_LENGTH - 1) + 1)

#define IPA_SPS_PROD_TIMEOUT_MSEC 100

#ifdef CONFIG_COMPAT
#define IPA_IOC_ADD_HDR32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_ADD_HDR, \
					compat_uptr_t)
#define IPA_IOC_DEL_HDR32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_DEL_HDR, \
					compat_uptr_t)
#define IPA_IOC_ADD_RT_RULE32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_ADD_RT_RULE, \
					compat_uptr_t)
#define IPA_IOC_DEL_RT_RULE32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_DEL_RT_RULE, \
					compat_uptr_t)
#define IPA_IOC_ADD_FLT_RULE32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_ADD_FLT_RULE, \
					compat_uptr_t)
#define IPA_IOC_DEL_FLT_RULE32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_DEL_FLT_RULE, \
					compat_uptr_t)
#define IPA_IOC_GET_RT_TBL32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GET_RT_TBL, \
				compat_uptr_t)
#define IPA_IOC_COPY_HDR32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_COPY_HDR, \
				compat_uptr_t)
#define IPA_IOC_QUERY_INTF32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_QUERY_INTF, \
				compat_uptr_t)
#define IPA_IOC_QUERY_INTF_TX_PROPS32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_QUERY_INTF_TX_PROPS, \
				compat_uptr_t)
#define IPA_IOC_QUERY_INTF_RX_PROPS32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_QUERY_INTF_RX_PROPS, \
					compat_uptr_t)
#define IPA_IOC_QUERY_INTF_EXT_PROPS32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_QUERY_INTF_EXT_PROPS, \
					compat_uptr_t)
#define IPA_IOC_GET_HDR32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GET_HDR, \
				compat_uptr_t)
#define IPA_IOC_ALLOC_NAT_MEM32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_ALLOC_NAT_MEM, \
				compat_uptr_t)
#define IPA_IOC_V4_INIT_NAT32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_V4_INIT_NAT, \
				compat_uptr_t)
#define IPA_IOC_NAT_DMA32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_NAT_DMA, \
				compat_uptr_t)
#define IPA_IOC_V4_DEL_NAT32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_V4_DEL_NAT, \
				compat_uptr_t)
#define IPA_IOC_GET_NAT_OFFSET32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GET_NAT_OFFSET, \
				compat_uptr_t)
#define IPA_IOC_PULL_MSG32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_PULL_MSG, \
				compat_uptr_t)
#define IPA_IOC_RM_ADD_DEPENDENCY32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_RM_ADD_DEPENDENCY, \
				compat_uptr_t)
#define IPA_IOC_RM_DEL_DEPENDENCY32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_RM_DEL_DEPENDENCY, \
				compat_uptr_t)
#define IPA_IOC_GENERATE_FLT_EQ32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_GENERATE_FLT_EQ, \
				compat_uptr_t)
#define IPA_IOC_QUERY_RT_TBL_INDEX32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_QUERY_RT_TBL_INDEX, \
				compat_uptr_t)
#define IPA_IOC_WRITE_QMAPID32  _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_WRITE_QMAPID, \
				compat_uptr_t)
#define IPA_IOC_MDFY_FLT_RULE32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_MDFY_FLT_RULE, \
				compat_uptr_t)
#define IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_ADD32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_NOTIFY_WAN_UPSTREAM_ROUTE_ADD, \
				compat_uptr_t)
#define IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_DEL32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_NOTIFY_WAN_UPSTREAM_ROUTE_DEL, \
				compat_uptr_t)
#define IPA_IOC_NOTIFY_WAN_EMBMS_CONNECTED32 _IOWR(IPA_IOC_MAGIC, \
					IPA_IOCTL_NOTIFY_WAN_EMBMS_CONNECTED, \
					compat_uptr_t)
#define IPA_IOC_ADD_HDR_PROC_CTX32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_ADD_HDR_PROC_CTX, \
				compat_uptr_t)
#define IPA_IOC_DEL_HDR_PROC_CTX32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_DEL_HDR_PROC_CTX, \
				compat_uptr_t)
#define IPA_IOC_MDFY_RT_RULE32 _IOWR(IPA_IOC_MAGIC, \
				IPA_IOCTL_MDFY_RT_RULE, \
				compat_uptr_t)

/**
 * struct ipa_ioc_nat_alloc_mem32 - nat table memory allocation
 * properties
 * @dev_name: input parameter, the name of table
 * @size: input parameter, size of table in bytes
 * @offset: output parameter, offset into page in case of system memory
 */
struct ipa_ioc_nat_alloc_mem32 {
	char dev_name[IPA_RESOURCE_NAME_MAX];
	compat_size_t size;
	compat_off_t offset;
};
#endif

static void ipa_start_tag_process(struct work_struct *work);
static DECLARE_WORK(ipa_tag_work, ipa_start_tag_process);

static void ipa_sps_release_resource(struct work_struct *work);
static DECLARE_DELAYED_WORK(ipa_sps_release_resource_work,
	ipa_sps_release_resource);

static struct ipa_plat_drv_res ipa_res = {0, };

struct msm_bus_scale_pdata *bus_scale_table;

static struct clk *ipa_clk_src;
static struct clk *ipa_clk;
static struct clk *smmu_clk;
static struct clk *sys_noc_ipa_axi_clk;
static struct clk *ipa_cnoc_clk;
static struct clk *ipa_inactivity_clk;

struct ipa_context *ipa_ctx;
static struct device *master_dev;
struct platform_device *ipa_pdev;
static struct {
	bool present;
	bool arm_smmu;
	bool disable_htw;
	bool fast_map;
	bool s1_bypass;
	u32 ipa_base;
	u32 ipa_size;
} smmu_info;

static char *active_clients_table_buf;

int ipa2_active_clients_log_print_buffer(char *buf, int size)
{
	int i;
	int nbytes;
	int cnt = 0;
	int start_idx;
	int end_idx;

	start_idx = (ipa_ctx->ipa2_active_clients_logging.log_tail + 1) %
			IPA2_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES;
	end_idx = ipa_ctx->ipa2_active_clients_logging.log_head;
	for (i = start_idx; i != end_idx;
		i = (i + 1) % IPA2_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES) {
		nbytes = scnprintf(buf + cnt, size - cnt, "%s\n",
				ipa_ctx->ipa2_active_clients_logging
				.log_buffer[i]);
		cnt += nbytes;
	}

	return cnt;
}

int ipa2_active_clients_log_print_table(char *buf, int size)
{
	int i;
	struct ipa2_active_client_htable_entry *iterator;
	int cnt = 0;

	cnt = scnprintf(buf, size, "\n---- Active Clients Table ----\n");
	hash_for_each(ipa_ctx->ipa2_active_clients_logging.htable, i,
			iterator, list) {
		switch (iterator->type) {
		case IPA2_ACTIVE_CLIENT_LOG_TYPE_EP:
			cnt += scnprintf(buf + cnt, size - cnt,
					"%-40s %-3d ENDPOINT\n",
					iterator->id_string, iterator->count);
			break;
		case IPA2_ACTIVE_CLIENT_LOG_TYPE_SIMPLE:
			cnt += scnprintf(buf + cnt, size - cnt,
					"%-40s %-3d SIMPLE\n",
					iterator->id_string, iterator->count);
			break;
		case IPA2_ACTIVE_CLIENT_LOG_TYPE_RESOURCE:
			cnt += scnprintf(buf + cnt, size - cnt,
					"%-40s %-3d RESOURCE\n",
					iterator->id_string, iterator->count);
			break;
		case IPA2_ACTIVE_CLIENT_LOG_TYPE_SPECIAL:
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
			ipa_ctx->ipa_active_clients.cnt);

	return cnt;
}

static int ipa2_active_clients_panic_notifier(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	ipa_active_clients_lock();
	ipa2_active_clients_log_print_table(active_clients_table_buf,
			IPA2_ACTIVE_CLIENTS_TABLE_BUF_SIZE);
	IPAERR("%s", active_clients_table_buf);
	ipa_active_clients_unlock();

	return NOTIFY_DONE;
}

static struct notifier_block ipa2_active_clients_panic_blk = {
	.notifier_call  = ipa2_active_clients_panic_notifier,
};

static int ipa2_active_clients_log_insert(const char *string)
{
	int head;
	int tail;

	head = ipa_ctx->ipa2_active_clients_logging.log_head;
	tail = ipa_ctx->ipa2_active_clients_logging.log_tail;

	if (!ipa_ctx->ipa2_active_clients_logging.log_rdy)
		return -EPERM;
	memset(ipa_ctx->ipa2_active_clients_logging.log_buffer[head], '_',
			IPA2_ACTIVE_CLIENTS_LOG_LINE_LEN);
	strlcpy(ipa_ctx->ipa2_active_clients_logging.log_buffer[head], string,
			(size_t)IPA2_ACTIVE_CLIENTS_LOG_LINE_LEN);
	head = (head + 1) % IPA2_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES;
	if (tail == head)
		tail = (tail + 1) % IPA2_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES;

	ipa_ctx->ipa2_active_clients_logging.log_tail = tail;
	ipa_ctx->ipa2_active_clients_logging.log_head = head;

	return 0;
}

static int ipa2_active_clients_log_init(void)
{
	int i;

	ipa_ctx->ipa2_active_clients_logging.log_buffer[0] = kzalloc(
			IPA2_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES *
			sizeof(char[IPA2_ACTIVE_CLIENTS_LOG_LINE_LEN]),
			GFP_KERNEL);
	active_clients_table_buf = kzalloc(sizeof(
			char[IPA2_ACTIVE_CLIENTS_TABLE_BUF_SIZE]), GFP_KERNEL);
	if (ipa_ctx->ipa2_active_clients_logging.log_buffer == NULL) {
		IPAERR("Active Clients Logging memory allocation failed");
		goto bail;
	}
	for (i = 0; i < IPA2_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES; i++) {
		ipa_ctx->ipa2_active_clients_logging.log_buffer[i] =
			ipa_ctx->ipa2_active_clients_logging.log_buffer[0] +
			(IPA2_ACTIVE_CLIENTS_LOG_LINE_LEN * i);
	}
	ipa_ctx->ipa2_active_clients_logging.log_head = 0;
	ipa_ctx->ipa2_active_clients_logging.log_tail =
			IPA2_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES - 1;
	hash_init(ipa_ctx->ipa2_active_clients_logging.htable);
	atomic_notifier_chain_register(&panic_notifier_list,
			&ipa2_active_clients_panic_blk);
	ipa_ctx->ipa2_active_clients_logging.log_rdy = 1;

	return 0;

bail:
	return -ENOMEM;
}

void ipa2_active_clients_log_clear(void)
{
	ipa_active_clients_lock();
	ipa_ctx->ipa2_active_clients_logging.log_head = 0;
	ipa_ctx->ipa2_active_clients_logging.log_tail =
			IPA2_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES - 1;
	ipa_active_clients_unlock();
}

static void ipa2_active_clients_log_destroy(void)
{
	ipa_ctx->ipa2_active_clients_logging.log_rdy = 0;
	kfree(ipa_ctx->ipa2_active_clients_logging.log_buffer[0]);
	ipa_ctx->ipa2_active_clients_logging.log_head = 0;
	ipa_ctx->ipa2_active_clients_logging.log_tail =
			IPA2_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES - 1;
}

enum ipa_smmu_cb_type {
	IPA_SMMU_CB_AP,
	IPA_SMMU_CB_WLAN,
	IPA_SMMU_CB_UC,
	IPA_SMMU_CB_MAX

};

static struct ipa_smmu_cb_ctx smmu_cb[IPA_SMMU_CB_MAX];

struct iommu_domain *ipa2_get_smmu_domain(void)
{
	if (smmu_cb[IPA_SMMU_CB_AP].valid)
		return smmu_cb[IPA_SMMU_CB_AP].mapping->domain;

	IPAERR("CB not valid\n");

	return NULL;
}

struct iommu_domain *ipa2_get_uc_smmu_domain(void)
{
	if (smmu_cb[IPA_SMMU_CB_UC].valid)
		return smmu_cb[IPA_SMMU_CB_UC].mapping->domain;

	IPAERR("CB not valid\n");

	return NULL;
}

struct iommu_domain *ipa2_get_wlan_smmu_domain(void)
{
	if (smmu_cb[IPA_SMMU_CB_WLAN].valid)
		return smmu_cb[IPA_SMMU_CB_WLAN].iommu;

	IPAERR("CB not valid\n");

	return NULL;
}

struct device *ipa2_get_dma_dev(void)
{
	return ipa_ctx->pdev;
}

/**
 * ipa2_get_smmu_ctx()- Return the smmu context
 *
 * Return value: pointer to smmu context address
 */
struct ipa_smmu_cb_ctx *ipa2_get_smmu_ctx(void)
{
	return &smmu_cb[IPA_SMMU_CB_AP];
}


/**
 * ipa2_get_wlan_smmu_ctx()- Return the wlan smmu context
 *
 * Return value: pointer to smmu context address
 */
struct ipa_smmu_cb_ctx *ipa2_get_wlan_smmu_ctx(void)
{
	return &smmu_cb[IPA_SMMU_CB_WLAN];
}

/**
 * ipa2_get_uc_smmu_ctx()- Return the uc smmu context
 *
 * Return value: pointer to smmu context address
 */
struct ipa_smmu_cb_ctx *ipa2_get_uc_smmu_ctx(void)
{
	return &smmu_cb[IPA_SMMU_CB_UC];
}

static int ipa_open(struct inode *inode, struct file *filp)
{
	struct ipa_context *ctx = NULL;

	IPADBG("ENTER\n");
	ctx = container_of(inode->i_cdev, struct ipa_context, cdev);
	filp->private_data = ctx;

	return 0;
}

/**
* ipa_flow_control() - Enable/Disable flow control on a particular client.
* Return codes:
* None
*/
void ipa_flow_control(enum ipa_client_type ipa_client,
		bool enable, uint32_t qmap_id)
{
	struct ipa_ep_cfg_ctrl ep_ctrl = {0};
	int ep_idx;
	struct ipa_ep_context *ep;

	/* Check if tethered flow control is needed or not.*/
	if (!ipa_ctx->tethered_flow_control) {
		IPADBG("Apps flow control is not needed\n");
		return;
	}

	/* Check if ep is valid. */
	ep_idx = ipa2_get_ep_mapping(ipa_client);
	if (ep_idx == -1) {
		IPADBG("Invalid IPA client\n");
		return;
	}

	ep = &ipa_ctx->ep[ep_idx];
	if (!ep->valid || (ep->client != IPA_CLIENT_USB_PROD)) {
		IPADBG("EP not valid/Not applicable for client.\n");
		return;
	}

	spin_lock(&ipa_ctx->disconnect_lock);
	/* Check if the QMAP_ID matches. */
	if (ep->cfg.meta.qmap_id != qmap_id) {
		IPADBG("Flow control ind not for same flow: %u %u\n",
			ep->cfg.meta.qmap_id, qmap_id);
		spin_unlock(&ipa_ctx->disconnect_lock);
		return;
	}
	if (!ep->disconnect_in_progress) {
		if (enable) {
			IPADBG("Enabling Flow\n");
			ep_ctrl.ipa_ep_delay = false;
			IPA_STATS_INC_CNT(ipa_ctx->stats.flow_enable);
		} else {
			IPADBG("Disabling Flow\n");
			ep_ctrl.ipa_ep_delay = true;
			IPA_STATS_INC_CNT(ipa_ctx->stats.flow_disable);
		}
		ep_ctrl.ipa_ep_suspend = false;
		ipa2_cfg_ep_ctrl(ep_idx, &ep_ctrl);
	} else {
		IPADBG("EP disconnect is in progress\n");
	}
	spin_unlock(&ipa_ctx->disconnect_lock);
}

static void ipa_wan_msg_free_cb(void *buff, u32 len, u32 type)
{
	if (!buff) {
		IPAERR("Null buffer\n");
		return;
	}

	if (type != WAN_UPSTREAM_ROUTE_ADD &&
	    type != WAN_UPSTREAM_ROUTE_DEL &&
	    type != WAN_EMBMS_CONNECT) {
		IPAERR("Wrong type given. buff %p type %d\n", buff, type);
		return;
	}

	kfree(buff);
}

static int ipa_send_wan_msg(unsigned long usr_param, uint8_t msg_type)
{
	int retval;
	struct ipa_wan_msg *wan_msg;
	struct ipa_msg_meta msg_meta;

	wan_msg = kzalloc(sizeof(struct ipa_wan_msg), GFP_KERNEL);
	if (!wan_msg) {
		IPAERR("no memory\n");
		return -ENOMEM;
	}

	if (copy_from_user((u8 *)wan_msg, (u8 *)usr_param,
		sizeof(struct ipa_wan_msg))) {
		kfree(wan_msg);
		return -EFAULT;
	}

	memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
	msg_meta.msg_type = msg_type;
	msg_meta.msg_len = sizeof(struct ipa_wan_msg);
	retval = ipa2_send_msg(&msg_meta, wan_msg, ipa_wan_msg_free_cb);
	if (retval) {
		IPAERR("ipa2_send_msg failed: %d\n", retval);
		kfree(wan_msg);
		return retval;
	}

	return 0;
}


static long ipa_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	u32 pyld_sz;
	u8 header[128] = { 0 };
	u8 *param = NULL;
	struct ipa_ioc_nat_alloc_mem nat_mem;
	struct ipa_ioc_v4_nat_init nat_init;
	struct ipa_ioc_v4_nat_del nat_del;
	struct ipa_ioc_rm_dependency rm_depend;
	size_t sz;
	int pre_entry;

	IPADBG("cmd=%x nr=%d\n", cmd, _IOC_NR(cmd));

	if (_IOC_TYPE(cmd) != IPA_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) >= IPA_IOCTL_MAX)
		return -ENOTTY;

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	switch (cmd) {
	case IPA_IOC_ALLOC_NAT_MEM:
		if (copy_from_user((u8 *)&nat_mem, (u8 *)arg,
					sizeof(struct ipa_ioc_nat_alloc_mem))) {
			retval = -EFAULT;
			break;
		}
		/* null terminate the string */
		nat_mem.dev_name[IPA_RESOURCE_NAME_MAX - 1] = '\0';

		if (ipa2_allocate_nat_device(&nat_mem)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, (u8 *)&nat_mem,
					sizeof(struct ipa_ioc_nat_alloc_mem))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_V4_INIT_NAT:
		if (copy_from_user((u8 *)&nat_init, (u8 *)arg,
					sizeof(struct ipa_ioc_v4_nat_init))) {
			retval = -EFAULT;
			break;
		}
		if (ipa2_nat_init_cmd(&nat_init)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_NAT_DMA:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_nat_dma_cmd))) {
			retval = -EFAULT;
			break;
		}
		pre_entry =
			((struct ipa_ioc_nat_dma_cmd *)header)->entries;
		pyld_sz =
		   sizeof(struct ipa_ioc_nat_dma_cmd) +
		   pre_entry * sizeof(struct ipa_ioc_nat_dma_one);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}

		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_nat_dma_cmd *)param)->entries
			!= pre_entry)) {
			IPAERR(" prevent memory corruption(%d not match %d)\n",
				((struct ipa_ioc_nat_dma_cmd *)param)->entries,
				pre_entry);
			retval = -EINVAL;
			break;
		}
		if (ipa2_nat_dma_cmd((struct ipa_ioc_nat_dma_cmd *)param)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_V4_DEL_NAT:
		if (copy_from_user((u8 *)&nat_del, (u8 *)arg,
					sizeof(struct ipa_ioc_v4_nat_del))) {
			retval = -EFAULT;
			break;
		}
		if (ipa2_nat_del_cmd(&nat_del)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_ADD_HDR:
		if (copy_from_user(header, (u8 *)arg,
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
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_add_hdr *)param)->num_hdrs
			!= pre_entry)) {
			IPAERR(" prevent memory corruption(%d not match %d)\n",
				((struct ipa_ioc_add_hdr *)param)->num_hdrs,
				pre_entry);
			retval = -EINVAL;
			break;
		}
		if (ipa2_add_hdr((struct ipa_ioc_add_hdr *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_DEL_HDR:
		if (copy_from_user(header, (u8 *)arg,
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
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_del_hdr *)param)->num_hdls
			!= pre_entry)) {
			IPAERR(" prevent memory corruption(%d not match %d)\n",
				((struct ipa_ioc_del_hdr *)param)->num_hdls,
				pre_entry);
			retval = -EINVAL;
			break;
		}
		if (ipa2_del_hdr_by_user((struct ipa_ioc_del_hdr *)param,
			true)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_ADD_RT_RULE:
		if (copy_from_user(header, (u8 *)arg,
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
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_add_rt_rule *)param)->num_rules
			!= pre_entry)) {
			IPAERR(" prevent memory corruption(%d not match %d)\n",
				((struct ipa_ioc_add_rt_rule *)param)->
				num_rules,
				pre_entry);
			retval = -EINVAL;
			break;
		}
		if (ipa2_add_rt_rule((struct ipa_ioc_add_rt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_MDFY_RT_RULE:
		if (copy_from_user(header, (u8 *)arg,
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
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_mdfy_rt_rule *)param)->num_rules
			!= pre_entry)) {
			IPAERR(" prevent memory corruption(%d not match %d)\n",
				((struct ipa_ioc_mdfy_rt_rule *)param)->
				num_rules,
				pre_entry);
			retval = -EINVAL;
			break;
		}
		if (ipa2_mdfy_rt_rule((struct ipa_ioc_mdfy_rt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_DEL_RT_RULE:
		if (copy_from_user(header, (u8 *)arg,
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
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_del_rt_rule *)param)->num_hdls
			!= pre_entry)) {
			IPAERR(" prevent memory corruption(%d not match %d)\n",
				((struct ipa_ioc_del_rt_rule *)param)->num_hdls,
				pre_entry);
			retval = -EINVAL;
			break;
		}
		if (ipa2_del_rt_rule((struct ipa_ioc_del_rt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_ADD_FLT_RULE:
		if (copy_from_user(header, (u8 *)arg,
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
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_add_flt_rule *)param)->num_rules
			!= pre_entry)) {
			IPAERR(" prevent memory corruption(%d not match %d)\n",
				((struct ipa_ioc_add_flt_rule *)param)->
				num_rules,
				pre_entry);
			retval = -EINVAL;
			break;
		}
		if (ipa2_add_flt_rule((struct ipa_ioc_add_flt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_DEL_FLT_RULE:
		if (copy_from_user(header, (u8 *)arg,
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
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_del_flt_rule *)param)->num_hdls
			!= pre_entry)) {
			IPAERR(" prevent memory corruption(%d not match %d)\n",
				((struct ipa_ioc_del_flt_rule *)param)->
				num_hdls,
				pre_entry);
			retval = -EINVAL;
			break;
		}
		if (ipa2_del_flt_rule((struct ipa_ioc_del_flt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_MDFY_FLT_RULE:
		if (copy_from_user(header, (u8 *)arg,
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
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_mdfy_flt_rule *)param)->num_rules
			!= pre_entry)) {
			IPAERR(" prevent memory corruption(%d not match %d)\n",
				((struct ipa_ioc_mdfy_flt_rule *)param)->
				num_rules,
				pre_entry);
			retval = -EINVAL;
			break;
		}
		if (ipa2_mdfy_flt_rule((struct ipa_ioc_mdfy_flt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_COMMIT_HDR:
		retval = ipa2_commit_hdr();
		break;
	case IPA_IOC_RESET_HDR:
		retval = ipa2_reset_hdr();
		break;
	case IPA_IOC_COMMIT_RT:
		retval = ipa2_commit_rt(arg);
		break;
	case IPA_IOC_RESET_RT:
		retval = ipa2_reset_rt(arg);
		break;
	case IPA_IOC_COMMIT_FLT:
		retval = ipa2_commit_flt(arg);
		break;
	case IPA_IOC_RESET_FLT:
		retval = ipa2_reset_flt(arg);
		break;
	case IPA_IOC_GET_RT_TBL:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_get_rt_tbl))) {
			retval = -EFAULT;
			break;
		}
		if (ipa2_get_rt_tbl((struct ipa_ioc_get_rt_tbl *)header)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, header,
					sizeof(struct ipa_ioc_get_rt_tbl))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_PUT_RT_TBL:
		retval = ipa2_put_rt_tbl(arg);
		break;
	case IPA_IOC_GET_HDR:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_get_hdr))) {
			retval = -EFAULT;
			break;
		}
		if (ipa2_get_hdr((struct ipa_ioc_get_hdr *)header)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, header,
					sizeof(struct ipa_ioc_get_hdr))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_PUT_HDR:
		retval = ipa2_put_hdr(arg);
		break;
	case IPA_IOC_SET_FLT:
		retval = ipa_cfg_filter(arg);
		break;
	case IPA_IOC_COPY_HDR:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_copy_hdr))) {
			retval = -EFAULT;
			break;
		}
		if (ipa2_copy_hdr((struct ipa_ioc_copy_hdr *)header)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, header,
					sizeof(struct ipa_ioc_copy_hdr))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_QUERY_INTF:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_query_intf))) {
			retval = -EFAULT;
			break;
		}
		if (ipa_query_intf((struct ipa_ioc_query_intf *)header)) {
			retval = -1;
			break;
		}
		if (copy_to_user((u8 *)arg, header,
					sizeof(struct ipa_ioc_query_intf))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_QUERY_INTF_TX_PROPS:
		sz = sizeof(struct ipa_ioc_query_intf_tx_props);
		if (copy_from_user(header, (u8 *)arg, sz)) {
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
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_query_intf_tx_props *)
			param)->num_tx_props
			!= pre_entry)) {
			IPAERR(" prevent memory corruption(%d not match %d)\n",
				((struct ipa_ioc_query_intf_tx_props *)
				param)->num_tx_props, pre_entry);
			retval = -EINVAL;
			break;
		}
		if (ipa_query_intf_tx_props(
				(struct ipa_ioc_query_intf_tx_props *)param)) {
			retval = -1;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_QUERY_INTF_RX_PROPS:
		sz = sizeof(struct ipa_ioc_query_intf_rx_props);
		if (copy_from_user(header, (u8 *)arg, sz)) {
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
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_query_intf_rx_props *)
			param)->num_rx_props != pre_entry)) {
			IPAERR(" prevent memory corruption(%d not match %d)\n",
				((struct ipa_ioc_query_intf_rx_props *)
				param)->num_rx_props, pre_entry);
			retval = -EINVAL;
			break;
		}
		if (ipa_query_intf_rx_props(
				(struct ipa_ioc_query_intf_rx_props *)param)) {
			retval = -1;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_QUERY_INTF_EXT_PROPS:
		sz = sizeof(struct ipa_ioc_query_intf_ext_props);
		if (copy_from_user(header, (u8 *)arg, sz)) {
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
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_query_intf_ext_props *)
			param)->num_ext_props != pre_entry)) {
			IPAERR(" prevent memory corruption(%d not match %d)\n",
				((struct ipa_ioc_query_intf_ext_props *)
				param)->num_ext_props, pre_entry);
			retval = -EINVAL;
			break;
		}
		if (ipa_query_intf_ext_props(
				(struct ipa_ioc_query_intf_ext_props *)param)) {
			retval = -1;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_PULL_MSG:
		if (copy_from_user(header, (u8 *)arg,
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
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_msg_meta *)param)->msg_len
			!= pre_entry)) {
			IPAERR(" prevent memory corruption(%d not match %d)\n",
				((struct ipa_msg_meta *)param)->msg_len,
				pre_entry);
			retval = -EINVAL;
			break;
		}
		if (ipa_pull_msg((struct ipa_msg_meta *)param,
				 (char *)param + sizeof(struct ipa_msg_meta),
				 ((struct ipa_msg_meta *)param)->msg_len) !=
		       ((struct ipa_msg_meta *)param)->msg_len) {
			retval = -1;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_RM_ADD_DEPENDENCY:
		if (copy_from_user((u8 *)&rm_depend, (u8 *)arg,
				sizeof(struct ipa_ioc_rm_dependency))) {
			retval = -EFAULT;
			break;
		}
		retval = ipa_rm_add_dependency_from_ioctl(
			rm_depend.resource_name, rm_depend.depends_on_name);
		break;
	case IPA_IOC_RM_DEL_DEPENDENCY:
		if (copy_from_user((u8 *)&rm_depend, (u8 *)arg,
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

			if (copy_from_user(&flt_eq, (u8 *)arg,
				sizeof(struct ipa_ioc_generate_flt_eq))) {
				retval = -EFAULT;
				break;
			}
			if (ipa_generate_flt_eq(flt_eq.ip, &flt_eq.attrib,
						&flt_eq.eq_attrib)) {
				retval = -EFAULT;
				break;
			}
			if (copy_to_user((u8 *)arg, &flt_eq,
				sizeof(struct ipa_ioc_generate_flt_eq))) {
				retval = -EFAULT;
				break;
			}
			break;
		}
	case IPA_IOC_QUERY_EP_MAPPING:
		{
			retval = ipa2_get_ep_mapping(arg);
			break;
		}
	case IPA_IOC_QUERY_RT_TBL_INDEX:
		if (copy_from_user(header, (u8 *)arg,
				sizeof(struct ipa_ioc_get_rt_tbl_indx))) {
			retval = -EFAULT;
			break;
		}
		if (ipa2_query_rt_index(
			 (struct ipa_ioc_get_rt_tbl_indx *)header)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, header,
				sizeof(struct ipa_ioc_get_rt_tbl_indx))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_WRITE_QMAPID:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_write_qmapid))) {
			retval = -EFAULT;
			break;
		}
		if (ipa2_write_qmap_id((struct ipa_ioc_write_qmapid *)header)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, header,
					sizeof(struct ipa_ioc_write_qmapid))) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_ADD:
		retval = ipa_send_wan_msg(arg, WAN_UPSTREAM_ROUTE_ADD);
		if (retval) {
			IPAERR("ipa_send_wan_msg failed: %d\n", retval);
			break;
		}
		break;
	case IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_DEL:
		retval = ipa_send_wan_msg(arg, WAN_UPSTREAM_ROUTE_DEL);
		if (retval) {
			IPAERR("ipa_send_wan_msg failed: %d\n", retval);
			break;
		}
		break;
	case IPA_IOC_NOTIFY_WAN_EMBMS_CONNECTED:
		retval = ipa_send_wan_msg(arg, WAN_EMBMS_CONNECT);
		if (retval) {
			IPAERR("ipa_send_wan_msg failed: %d\n", retval);
			break;
		}
		break;
	case IPA_IOC_ADD_HDR_PROC_CTX:
		if (copy_from_user(header, (u8 *)arg,
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
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_add_hdr_proc_ctx *)
			param)->num_proc_ctxs != pre_entry)) {
			IPAERR(" prevent memory corruption(%d not match %d)\n",
				((struct ipa_ioc_add_hdr_proc_ctx *)
				param)->num_proc_ctxs, pre_entry);
			retval = -EINVAL;
			break;
		}
		if (ipa2_add_hdr_proc_ctx(
			(struct ipa_ioc_add_hdr_proc_ctx *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_DEL_HDR_PROC_CTX:
		if (copy_from_user(header, (u8 *)arg,
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
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_del_hdr_proc_ctx *)
			param)->num_hdls != pre_entry)) {
			IPAERR(" prevent memory corruption( %d not match %d)\n",
				((struct ipa_ioc_del_hdr_proc_ctx *)param)->
				num_hdls,
				pre_entry);
			retval = -EINVAL;
			break;
		}
		if (ipa2_del_hdr_proc_ctx_by_user(
			(struct ipa_ioc_del_hdr_proc_ctx *)param, true)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
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
		memcpy(param, &ipa_ctx->ipa_hw_type, pyld_sz);
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	default:        /* redundant, as cmd was checked against MAXNR */
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return -ENOTTY;
	}
	kfree(param);

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return retval;
}

/**
* ipa_setup_dflt_rt_tables() - Setup default routing tables
*
* Return codes:
* 0: success
* -ENOMEM: failed to allocate memory
* -EPERM: failed to add the tables
*/
int ipa_setup_dflt_rt_tables(void)
{
	struct ipa_ioc_add_rt_rule *rt_rule;
	struct ipa_rt_rule_add *rt_rule_entry;

	rt_rule =
	   kzalloc(sizeof(struct ipa_ioc_add_rt_rule) + 1 *
			   sizeof(struct ipa_rt_rule_add), GFP_KERNEL);
	if (!rt_rule) {
		IPAERR("fail to alloc mem\n");
		return -ENOMEM;
	}
	/* setup a default v4 route to point to Apps */
	rt_rule->num_rules = 1;
	rt_rule->commit = 1;
	rt_rule->ip = IPA_IP_v4;
	strlcpy(rt_rule->rt_tbl_name, IPA_DFLT_RT_TBL_NAME,
			IPA_RESOURCE_NAME_MAX);

	rt_rule_entry = &rt_rule->rules[0];
	rt_rule_entry->at_rear = 1;
	rt_rule_entry->rule.dst = IPA_CLIENT_APPS_LAN_CONS;
	rt_rule_entry->rule.hdr_hdl = ipa_ctx->excp_hdr_hdl;

	if (ipa2_add_rt_rule(rt_rule)) {
		IPAERR("fail to add dflt v4 rule\n");
		kfree(rt_rule);
		return -EPERM;
	}
	IPADBG("dflt v4 rt rule hdl=%x\n", rt_rule_entry->rt_rule_hdl);
	ipa_ctx->dflt_v4_rt_rule_hdl = rt_rule_entry->rt_rule_hdl;

	/* setup a default v6 route to point to A5 */
	rt_rule->ip = IPA_IP_v6;
	if (ipa2_add_rt_rule(rt_rule)) {
		IPAERR("fail to add dflt v6 rule\n");
		kfree(rt_rule);
		return -EPERM;
	}
	IPADBG("dflt v6 rt rule hdl=%x\n", rt_rule_entry->rt_rule_hdl);
	ipa_ctx->dflt_v6_rt_rule_hdl = rt_rule_entry->rt_rule_hdl;

	/*
	 * because these tables are the very first to be added, they will both
	 * have the same index (0) which is essential for programming the
	 * "route" end-point config
	 */

	kfree(rt_rule);

	return 0;
}

static int ipa_setup_exception_path(void)
{
	struct ipa_ioc_add_hdr *hdr;
	struct ipa_hdr_add *hdr_entry;
	struct ipa_route route = { 0 };
	int ret;

	/* install the basic exception header */
	hdr = kzalloc(sizeof(struct ipa_ioc_add_hdr) + 1 *
		      sizeof(struct ipa_hdr_add), GFP_KERNEL);
	if (!hdr) {
		IPAERR("fail to alloc exception hdr\n");
		return -ENOMEM;
	}
	hdr->num_hdrs = 1;
	hdr->commit = 1;
	hdr_entry = &hdr->hdr[0];

	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_1) {
		strlcpy(hdr_entry->name, IPA_A5_MUX_HDR_NAME,
				IPA_RESOURCE_NAME_MAX);
		/* set template for the A5_MUX hdr in header addition block */
		hdr_entry->hdr_len = IPA_A5_MUX_HEADER_LENGTH;
	} else if (ipa_ctx->ipa_hw_type >= IPA_HW_v2_0) {
		strlcpy(hdr_entry->name, IPA_LAN_RX_HDR_NAME,
				IPA_RESOURCE_NAME_MAX);
		hdr_entry->hdr_len = IPA_LAN_RX_HEADER_LENGTH;
	} else {
		WARN_ON(1);
	}

	if (ipa2_add_hdr(hdr)) {
		IPAERR("fail to add exception hdr\n");
		ret = -EPERM;
		goto bail;
	}

	if (hdr_entry->status) {
		IPAERR("fail to add exception hdr\n");
		ret = -EPERM;
		goto bail;
	}

	ipa_ctx->excp_hdr_hdl = hdr_entry->hdr_hdl;

	/* set the route register to pass exception packets to Apps */
	route.route_def_pipe = ipa2_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS);
	route.route_frag_def_pipe = ipa2_get_ep_mapping(
		IPA_CLIENT_APPS_LAN_CONS);
	route.route_def_hdr_table = !ipa_ctx->hdr_tbl_lcl;

	if (ipa_cfg_route(&route)) {
		IPAERR("fail to add exception hdr\n");
		ret = -EPERM;
		goto bail;
	}

	ret = 0;
bail:
	kfree(hdr);
	return ret;
}

static int ipa_init_smem_region(int memory_region_size,
				int memory_region_offset)
{
	struct ipa_hw_imm_cmd_dma_shared_mem *cmd = NULL;
	struct ipa_desc desc;
	struct ipa_mem_buffer mem;
	int rc;

	if (memory_region_size == 0)
		return 0;

	memset(&desc, 0, sizeof(desc));
	memset(&mem, 0, sizeof(mem));

	mem.size = memory_region_size;
	mem.base = dma_alloc_coherent(ipa_ctx->pdev, mem.size,
		&mem.phys_base, GFP_KERNEL);
	if (!mem.base) {
		IPAERR("failed to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	memset(mem.base, 0, mem.size);

	cmd = kzalloc(sizeof(*cmd),
		GFP_KERNEL);
	if (cmd == NULL) {
		IPAERR("Failed to alloc immediate command object\n");
		rc = -ENOMEM;
		goto fail_send_cmd;
	}

	cmd->size = mem.size;
	cmd->system_addr = mem.phys_base;
	cmd->local_addr = ipa_ctx->smem_restricted_bytes +
		memory_region_offset;
	desc.opcode = IPA_DMA_SHARED_MEM;
	desc.pyld = cmd;
	desc.len = sizeof(*cmd);
	desc.type = IPA_IMM_CMD_DESC;

	rc = ipa_send_cmd(1, &desc);
	if (rc) {
		IPAERR("failed to send immediate command (error %d)\n", rc);
		rc = -EFAULT;
	}

	kfree(cmd);
fail_send_cmd:
	dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base,
		mem.phys_base);

	return rc;
}

/**
* ipa_init_q6_smem() - Initialize Q6 general memory and
*                      header memory regions in IPA.
*
* Return codes:
* 0: success
* -ENOMEM: failed to allocate dma memory
* -EFAULT: failed to send IPA command to initialize the memory
*/
int ipa_init_q6_smem(void)
{
	int rc;

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	if (ipa_ctx->ipa_hw_type == IPA_HW_v2_0)
		rc = ipa_init_smem_region(IPA_MEM_PART(modem_size) -
			IPA_MEM_RAM_MODEM_NETWORK_STATS_SIZE,
			IPA_MEM_PART(modem_ofst));
	else
		rc = ipa_init_smem_region(IPA_MEM_PART(modem_size),
			IPA_MEM_PART(modem_ofst));

	if (rc) {
		IPAERR("failed to initialize Modem RAM memory\n");
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return rc;
	}

	rc = ipa_init_smem_region(IPA_MEM_PART(modem_hdr_size),
		IPA_MEM_PART(modem_hdr_ofst));
	if (rc) {
		IPAERR("failed to initialize Modem HDRs RAM memory\n");
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return rc;
	}

	rc = ipa_init_smem_region(IPA_MEM_PART(modem_hdr_proc_ctx_size),
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst));
	if (rc) {
		IPAERR("failed to initialize Modem proc ctx RAM memory\n");
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return rc;
	}

	rc = ipa_init_smem_region(IPA_MEM_PART(modem_comp_decomp_size),
		IPA_MEM_PART(modem_comp_decomp_ofst));
	if (rc) {
		IPAERR("failed to initialize Modem Comp/Decomp RAM memory\n");
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
		return rc;
	}

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return rc;
}

static void ipa_free_buffer(void *user1, int user2)
{
	kfree(user1);
}

int ipa_q6_pipe_delay(bool zip_pipes)
{
	u32 reg_val = 0;
	int client_idx;
	int ep_idx;

	/* For ZIP pipes, processing is done in AFTER_SHUTDOWN callback. */
	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		/* Skip the processing for non Q6 pipes. */
		if (!IPA_CLIENT_IS_Q6_PROD(client_idx))
			continue;
		/* Skip the processing for NON-ZIP pipes. */
		else if (zip_pipes && IPA_CLIENT_IS_Q6_NON_ZIP_PROD(client_idx))
			continue;
		/* Skip the processing for ZIP pipes. */
		else if (!zip_pipes && IPA_CLIENT_IS_Q6_ZIP_PROD(client_idx))
			continue;

		ep_idx = ipa2_get_ep_mapping(client_idx);
		if (ep_idx == -1)
			continue;

		IPA_SETFIELD_IN_REG(reg_val, 1,
			IPA_ENDP_INIT_CTRL_N_ENDP_DELAY_SHFT,
			IPA_ENDP_INIT_CTRL_N_ENDP_DELAY_BMSK);

		ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_CTRL_N_OFST(ep_idx), reg_val);
	}

	return 0;
}

int ipa_q6_monitor_holb_mitigation(bool enable)
{
	int ep_idx;
	int client_idx;

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		if (IPA_CLIENT_IS_Q6_NON_ZIP_CONS(client_idx)) {
			ep_idx = ipa2_get_ep_mapping(client_idx);
			if (ep_idx == -1)
				continue;
			/* Send a command to Uc to enable/disable
			 * holb monitoring.
			 */
			ipa_uc_monitor_holb(client_idx, enable);
		}
	}
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return 0;
}

static int ipa_q6_avoid_holb(bool zip_pipes)
{
	u32 reg_val;
	int ep_idx;
	int client_idx;
	struct ipa_ep_cfg_ctrl avoid_holb;

	memset(&avoid_holb, 0, sizeof(avoid_holb));
	avoid_holb.ipa_ep_suspend = true;

	/* For ZIP pipes, processing is done in AFTER_SHUTDOWN callback. */
	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		/* Skip the processing for non Q6 pipes. */
		if (!IPA_CLIENT_IS_Q6_CONS(client_idx))
			continue;
		/* Skip the processing for NON-ZIP pipes. */
		else if (zip_pipes && IPA_CLIENT_IS_Q6_NON_ZIP_CONS(client_idx))
			continue;
		/* Skip the processing for ZIP pipes. */
		else if (!zip_pipes && IPA_CLIENT_IS_Q6_ZIP_CONS(client_idx))
			continue;

		ep_idx = ipa2_get_ep_mapping(client_idx);
		if (ep_idx == -1)
			continue;

		/*
		 * ipa2_cfg_ep_holb is not used here because we are
		 * setting HOLB on Q6 pipes, and from APPS perspective
		 * they are not valid, therefore, the above function
		 * will fail.
		 */
		reg_val = 0;
		IPA_SETFIELD_IN_REG(reg_val, 0,
			IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_TIMER_SHFT,
			IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_TIMER_BMSK);

		ipa_write_reg(ipa_ctx->mmio,
		IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_OFST_v2_0(ep_idx),
			reg_val);

		reg_val = 0;
		IPA_SETFIELD_IN_REG(reg_val, 1,
			IPA_ENDP_INIT_HOL_BLOCK_EN_N_EN_SHFT,
			IPA_ENDP_INIT_HOL_BLOCK_EN_N_EN_BMSK);

		ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HOL_BLOCK_EN_N_OFST_v2_0(ep_idx),
			reg_val);

		ipa2_cfg_ep_ctrl(ep_idx, &avoid_holb);
	}

	return 0;
}

static u32 ipa_get_max_flt_rt_cmds(u32 num_pipes)
{
	u32 max_cmds = 0;

	/* As many filter tables as there are pipes, x2 for IPv4 and IPv6 */
	max_cmds += num_pipes * 2;

	/* For each of the Modem routing tables */
	max_cmds += (IPA_MEM_PART(v4_modem_rt_index_hi) -
		     IPA_MEM_PART(v4_modem_rt_index_lo) + 1);

	max_cmds += (IPA_MEM_PART(v6_modem_rt_index_hi) -
		     IPA_MEM_PART(v6_modem_rt_index_lo) + 1);

	return max_cmds;
}

static int ipa_q6_clean_q6_tables(void)
{
	struct ipa_desc *desc;
	struct ipa_hw_imm_cmd_dma_shared_mem *cmd = NULL;
	int pipe_idx;
	int num_cmds = 0;
	int index;
	int retval;
	struct ipa_mem_buffer mem = { 0 };
	u32 *entry;
	u32 max_cmds = ipa_get_max_flt_rt_cmds(ipa_ctx->ipa_num_pipes);

	mem.base = dma_alloc_coherent(ipa_ctx->pdev, 4, &mem.phys_base,
		GFP_ATOMIC);
	if (!mem.base) {
		IPAERR("failed to alloc DMA buff of size 4\n");
		return -ENOMEM;
	}

	mem.size = 4;
	entry = mem.base;
	*entry = ipa_ctx->empty_rt_tbl_mem.phys_base;

	desc = kcalloc(max_cmds, sizeof(struct ipa_desc), GFP_KERNEL);
	if (!desc) {
		IPAERR("failed to allocate memory\n");
		retval = -ENOMEM;
		goto bail_dma;
	}

	cmd = kcalloc(max_cmds, sizeof(struct ipa_hw_imm_cmd_dma_shared_mem),
		GFP_KERNEL);
	if (!cmd) {
		IPAERR("failed to allocate memory\n");
		retval = -ENOMEM;
		goto bail_desc;
	}

	/*
	 * Iterating over all the pipes which are either invalid but connected
	 * or connected but not configured by AP.
	 */
	for (pipe_idx = 0; pipe_idx < ipa_ctx->ipa_num_pipes; pipe_idx++) {
		if (!ipa_ctx->ep[pipe_idx].valid ||
		    ipa_ctx->ep[pipe_idx].skip_ep_cfg) {
			/*
			 * Need to point v4 and v6 fltr tables to an empty
			 * table
			 */
			cmd[num_cmds].size = mem.size;
			cmd[num_cmds].system_addr = mem.phys_base;
			cmd[num_cmds].local_addr =
				ipa_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v4_flt_ofst) + 8 + pipe_idx * 4;

			desc[num_cmds].opcode = IPA_DMA_SHARED_MEM;
			desc[num_cmds].pyld = &cmd[num_cmds];
			desc[num_cmds].len = sizeof(*cmd);
			desc[num_cmds].type = IPA_IMM_CMD_DESC;
			num_cmds++;

			cmd[num_cmds].size = mem.size;
			cmd[num_cmds].system_addr =  mem.phys_base;
			cmd[num_cmds].local_addr =
				ipa_ctx->smem_restricted_bytes +
				IPA_MEM_PART(v6_flt_ofst) + 8 + pipe_idx * 4;

			desc[num_cmds].opcode = IPA_DMA_SHARED_MEM;
			desc[num_cmds].pyld = &cmd[num_cmds];
			desc[num_cmds].len = sizeof(*cmd);
			desc[num_cmds].type = IPA_IMM_CMD_DESC;
			num_cmds++;
		}
	}

	/* Need to point v4/v6 modem routing tables to an empty table */
	for (index = IPA_MEM_PART(v4_modem_rt_index_lo);
		 index <= IPA_MEM_PART(v4_modem_rt_index_hi);
		 index++) {
		cmd[num_cmds].size = mem.size;
		cmd[num_cmds].system_addr =  mem.phys_base;
		cmd[num_cmds].local_addr = ipa_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v4_rt_ofst) + index * 4;

		desc[num_cmds].opcode = IPA_DMA_SHARED_MEM;
		desc[num_cmds].pyld = &cmd[num_cmds];
		desc[num_cmds].len = sizeof(*cmd);
		desc[num_cmds].type = IPA_IMM_CMD_DESC;
		num_cmds++;
	}

	for (index = IPA_MEM_PART(v6_modem_rt_index_lo);
		 index <= IPA_MEM_PART(v6_modem_rt_index_hi);
		 index++) {
		cmd[num_cmds].size = mem.size;
		cmd[num_cmds].system_addr =  mem.phys_base;
		cmd[num_cmds].local_addr = ipa_ctx->smem_restricted_bytes +
			IPA_MEM_PART(v6_rt_ofst) + index * 4;

		desc[num_cmds].opcode = IPA_DMA_SHARED_MEM;
		desc[num_cmds].pyld = &cmd[num_cmds];
		desc[num_cmds].len = sizeof(*cmd);
		desc[num_cmds].type = IPA_IMM_CMD_DESC;
		num_cmds++;
	}

	retval = ipa_send_cmd(num_cmds, desc);
	if (retval) {
		IPAERR("failed to send immediate command (error %d)\n", retval);
		retval = -EFAULT;
	}

	kfree(cmd);

bail_desc:
	kfree(desc);

bail_dma:
	dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base, mem.phys_base);

	return retval;
}

static void ipa_q6_disable_agg_reg(struct ipa_register_write *reg_write,
				   int ep_idx)
{
	reg_write->skip_pipeline_clear = 0;

	reg_write->offset = IPA_ENDP_INIT_AGGR_N_OFST_v2_0(ep_idx);
	reg_write->value =
		(1 & IPA_ENDP_INIT_AGGR_n_AGGR_FORCE_CLOSE_BMSK) <<
		IPA_ENDP_INIT_AGGR_n_AGGR_FORCE_CLOSE_SHFT;
	reg_write->value_mask =
		IPA_ENDP_INIT_AGGR_n_AGGR_FORCE_CLOSE_BMSK <<
		IPA_ENDP_INIT_AGGR_n_AGGR_FORCE_CLOSE_SHFT;

	reg_write->value |=
		((0 & IPA_ENDP_INIT_AGGR_N_AGGR_EN_BMSK) <<
		IPA_ENDP_INIT_AGGR_N_AGGR_EN_SHFT);
	reg_write->value_mask |=
		((IPA_ENDP_INIT_AGGR_N_AGGR_EN_BMSK <<
		IPA_ENDP_INIT_AGGR_N_AGGR_EN_SHFT));
}

static int ipa_q6_set_ex_path_dis_agg(void)
{
	int ep_idx;
	int client_idx;
	struct ipa_desc *desc;
	int num_descs = 0;
	int index;
	struct ipa_register_write *reg_write;
	int retval;

	desc = kcalloc(ipa_ctx->ipa_num_pipes, sizeof(struct ipa_desc),
			GFP_KERNEL);
	if (!desc) {
		IPAERR("failed to allocate memory\n");
		return -ENOMEM;
	}

	/* Set the exception path to AP */
	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		ep_idx = ipa2_get_ep_mapping(client_idx);
		if (ep_idx == -1)
			continue;

		if (ipa_ctx->ep[ep_idx].valid &&
			ipa_ctx->ep[ep_idx].skip_ep_cfg) {
			BUG_ON(num_descs >= ipa_ctx->ipa_num_pipes);
			reg_write = kzalloc(sizeof(*reg_write), GFP_KERNEL);

			if (!reg_write) {
				IPAERR("failed to allocate memory\n");
				BUG();
			}
			reg_write->skip_pipeline_clear = 0;
			reg_write->offset = IPA_ENDP_STATUS_n_OFST(ep_idx);
			reg_write->value =
				(ipa2_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS) &
				IPA_ENDP_STATUS_n_STATUS_ENDP_BMSK) <<
				IPA_ENDP_STATUS_n_STATUS_ENDP_SHFT;
			reg_write->value_mask =
				IPA_ENDP_STATUS_n_STATUS_ENDP_BMSK <<
				IPA_ENDP_STATUS_n_STATUS_ENDP_SHFT;

			desc[num_descs].opcode = IPA_REGISTER_WRITE;
			desc[num_descs].pyld = reg_write;
			desc[num_descs].len = sizeof(*reg_write);
			desc[num_descs].type = IPA_IMM_CMD_DESC;
			desc[num_descs].callback = ipa_free_buffer;
			desc[num_descs].user1 = reg_write;
			num_descs++;
		}
	}

	/* Disable AGGR on IPA->Q6 pipes */
	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		ep_idx = ipa2_get_ep_mapping(client_idx);
		if (ep_idx == -1)
			continue;
		if (IPA_CLIENT_IS_Q6_NON_ZIP_CONS(client_idx) ||
			IPA_CLIENT_IS_Q6_ZIP_CONS(client_idx)) {
			reg_write = kzalloc(sizeof(*reg_write), GFP_KERNEL);

			if (!reg_write) {
				IPAERR("failed to allocate memory\n");
				BUG();
			}

			ipa_q6_disable_agg_reg(reg_write, ep_idx);

			desc[num_descs].opcode = IPA_REGISTER_WRITE;
			desc[num_descs].pyld = reg_write;
			desc[num_descs].len = sizeof(*reg_write);
			desc[num_descs].type = IPA_IMM_CMD_DESC;
			desc[num_descs].callback = ipa_free_buffer;
			desc[num_descs].user1 = reg_write;
			num_descs++;
		}
	}

	/* Will wait 150msecs for IPA tag process completion */
	retval = ipa_tag_process(desc, num_descs,
				 msecs_to_jiffies(CLEANUP_TAG_PROCESS_TIMEOUT));
	if (retval) {
		IPAERR("TAG process failed! (error %d)\n", retval);
		/* For timeout error ipa_free_buffer cb will free user1 */
		if (retval != -ETIME) {
			for (index = 0; index < num_descs; index++)
				kfree(desc[index].user1);
			retval = -EINVAL;
		}
	}

	kfree(desc);

	return retval;
}

/**
* ipa_q6_pre_shutdown_cleanup() - A cleanup for all Q6 related configuration
*                    in IPA HW before modem shutdown. This is performed in
*                    case of SSR.
*
* Return codes:
* 0: success
* This is a mandatory procedure, in case one of the steps fails, the
* AP needs to restart.
*/
int ipa_q6_pre_shutdown_cleanup(void)
{
	/* If uC has notified the APPS upon a ZIP engine error,
	 * APPS need to assert (This is a non recoverable error).
	 */
	if (ipa_ctx->uc_ctx.uc_zip_error)
		BUG();

	IPA_ACTIVE_CLIENTS_INC_SPECIAL("Q6");

	/*
	 * Do not delay Q6 pipes here. This may result in IPA reading a
	 * DMA_TASK with lock bit set and then Q6 pipe delay is set. In this
	 * situation IPA will be remain locked as the DMA_TASK with unlock
	 * bit will not be read by IPA as pipe delay is enabled. IPA uC will
	 * wait for pipe to be empty before issuing a BAM pipe reset.
	 */

	if (ipa_q6_monitor_holb_mitigation(false)) {
		IPAERR("Failed to disable HOLB monitroing on Q6 pipes\n");
		BUG();
	}

	if (ipa_q6_avoid_holb(false)) {
		IPAERR("Failed to set HOLB on Q6 pipes\n");
		BUG();
	}
	if (ipa_q6_clean_q6_tables()) {
		IPAERR("Failed to clean Q6 tables\n");
		BUG();
	}
	if (ipa_q6_set_ex_path_dis_agg()) {
		IPAERR("Failed to disable aggregation on Q6 pipes\n");
		BUG();
	}

	ipa_ctx->q6_proxy_clk_vote_valid = true;
	return 0;
}

/**
* ipa_q6_post_shutdown_cleanup() - A cleanup for the Q6 pipes
*                    in IPA HW after modem shutdown. This is performed
*                    in case of SSR.
*
* Return codes:
* 0: success
* This is a mandatory procedure, in case one of the steps fails, the
* AP needs to restart.
*/
int ipa_q6_post_shutdown_cleanup(void)
{
	int client_idx;
	int res;

	/*
	 * Do not delay Q6 pipes here. This may result in IPA reading a
	 * DMA_TASK with lock bit set and then Q6 pipe delay is set. In this
	 * situation IPA will be remain locked as the DMA_TASK with unlock
	 * bit will not be read by IPA as pipe delay is enabled. IPA uC will
	 * wait for pipe to be empty before issuing a BAM pipe reset.
	 */

	if (ipa_q6_avoid_holb(true)) {
		IPAERR("Failed to set HOLB on Q6 ZIP pipes\n");
		BUG();
	}

	if (!ipa_ctx->uc_ctx.uc_loaded) {
		IPAERR("uC is not loaded, won't reset Q6 pipes\n");
		return 0;
	}

	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++)
		if (IPA_CLIENT_IS_Q6_NON_ZIP_CONS(client_idx) ||
			IPA_CLIENT_IS_Q6_ZIP_CONS(client_idx) ||
			IPA_CLIENT_IS_Q6_NON_ZIP_PROD(client_idx) ||
			IPA_CLIENT_IS_Q6_ZIP_PROD(client_idx)) {
			res = ipa_uc_reset_pipe(client_idx);
			if (res)
				BUG();
		}
	return 0;
}

int _ipa_init_sram_v2(void)
{
	u32 *ipa_sram_mmio;
	unsigned long phys_addr;
	struct ipa_hw_imm_cmd_dma_shared_mem *cmd = NULL;
	struct ipa_desc desc = {0};
	struct ipa_mem_buffer mem;
	int rc = 0;

	phys_addr = ipa_ctx->ipa_wrapper_base +
		ipa_ctx->ctrl->ipa_reg_base_ofst +
		IPA_SRAM_DIRECT_ACCESS_N_OFST_v2_0(
			ipa_ctx->smem_restricted_bytes / 4);

	ipa_sram_mmio = ioremap(phys_addr,
			ipa_ctx->smem_sz - ipa_ctx->smem_restricted_bytes);
	if (!ipa_sram_mmio) {
		IPAERR("fail to ioremap IPA SRAM\n");
		return -ENOMEM;
	}

#define IPA_SRAM_SET(ofst, val) (ipa_sram_mmio[(ofst - 4) / 4] = val)

	IPA_SRAM_SET(IPA_MEM_PART(v6_flt_ofst) - 4, IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(v6_flt_ofst), IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(v4_rt_ofst) - 4, IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(v4_rt_ofst), IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(v6_rt_ofst), IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(modem_hdr_ofst), IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(modem_ofst), IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(apps_v4_flt_ofst), IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(uc_info_ofst), IPA_MEM_CANARY_VAL);

	iounmap(ipa_sram_mmio);

	mem.size = IPA_STATUS_CLEAR_SIZE;
	mem.base = dma_alloc_coherent(ipa_ctx->pdev, mem.size, &mem.phys_base,
			GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}
	memset(mem.base, 0, mem.size);

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL) {
		IPAERR("Failed to alloc immediate command object\n");
		rc = -ENOMEM;
		goto fail_send_cmd;
	}

	cmd->size = mem.size;
	cmd->system_addr = mem.phys_base;
	cmd->local_addr = IPA_STATUS_CLEAR_OFST;
	desc.opcode = IPA_DMA_SHARED_MEM;
	desc.pyld = (void *)cmd;
	desc.len = sizeof(struct ipa_hw_imm_cmd_dma_shared_mem);
	desc.type = IPA_IMM_CMD_DESC;

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	kfree(cmd);
fail_send_cmd:
	dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return rc;
}

int _ipa_init_sram_v2_5(void)
{
	u32 *ipa_sram_mmio;
	unsigned long phys_addr;

	phys_addr = ipa_ctx->ipa_wrapper_base +
			ipa_ctx->ctrl->ipa_reg_base_ofst +
			IPA_SRAM_SW_FIRST_v2_5;

	ipa_sram_mmio = ioremap(phys_addr,
		ipa_ctx->smem_sz - ipa_ctx->smem_restricted_bytes);
	if (!ipa_sram_mmio) {
		IPAERR("fail to ioremap IPA SRAM\n");
		return -ENOMEM;
	}

#define IPA_SRAM_SET(ofst, val) (ipa_sram_mmio[(ofst - 4) / 4] = val)

	IPA_SRAM_SET(IPA_MEM_PART(v4_flt_ofst) - 4, IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(v4_flt_ofst), IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(v6_flt_ofst) - 4, IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(v6_flt_ofst), IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(v4_rt_ofst) - 4, IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(v4_rt_ofst), IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(v6_rt_ofst), IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(modem_hdr_ofst), IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(modem_hdr_proc_ctx_ofst) - 4,
							IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(modem_hdr_proc_ctx_ofst), IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(modem_ofst), IPA_MEM_CANARY_VAL);
	IPA_SRAM_SET(IPA_MEM_PART(end_ofst), IPA_MEM_CANARY_VAL);

	iounmap(ipa_sram_mmio);

	return 0;
}

static inline void ipa_sram_set_canary(u32 *sram_mmio, int offset)
{
	/* Set 4 bytes of CANARY before the offset */
	sram_mmio[(offset - 4) / 4] = IPA_MEM_CANARY_VAL;
}

int _ipa_init_sram_v2_6L(void)
{
	u32 *ipa_sram_mmio;
	unsigned long phys_addr;

	phys_addr = ipa_ctx->ipa_wrapper_base +
		ipa_ctx->ctrl->ipa_reg_base_ofst +
		IPA_SRAM_SW_FIRST_v2_5;

	ipa_sram_mmio = ioremap(phys_addr,
		ipa_ctx->smem_sz - ipa_ctx->smem_restricted_bytes);
	if (!ipa_sram_mmio) {
		IPAERR("fail to ioremap IPA SRAM\n");
		return -ENOMEM;
	}

	/* Consult with ipa_ram_mmap.h on the location of the CANARY values */
	ipa_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_flt_ofst) - 4);
	ipa_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_flt_ofst));
	ipa_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_flt_ofst) - 4);
	ipa_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_flt_ofst));
	ipa_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_rt_ofst) - 4);
	ipa_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v4_rt_ofst));
	ipa_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(v6_rt_ofst));
	ipa_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(modem_hdr_ofst));
	ipa_sram_set_canary(ipa_sram_mmio,
			    IPA_MEM_PART(modem_comp_decomp_ofst) - 4);
	ipa_sram_set_canary(ipa_sram_mmio,
			    IPA_MEM_PART(modem_comp_decomp_ofst));
	ipa_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(modem_ofst));
	ipa_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(end_ofst));

	iounmap(ipa_sram_mmio);

	return 0;
}

int _ipa_init_hdr_v2(void)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer mem;
	struct ipa_hdr_init_local *cmd = NULL;
	int rc = 0;

	mem.size = IPA_MEM_PART(modem_hdr_size) + IPA_MEM_PART(apps_hdr_size);
	mem.base = dma_alloc_coherent(ipa_ctx->pdev, mem.size, &mem.phys_base,
			GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}
	memset(mem.base, 0, mem.size);

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL) {
		IPAERR("Failed to alloc header init command object\n");
		rc = -ENOMEM;
		goto fail_send_cmd;
	}

	cmd->hdr_table_src_addr = mem.phys_base;
	cmd->size_hdr_table = mem.size;
	cmd->hdr_table_dst_addr = ipa_ctx->smem_restricted_bytes +
		IPA_MEM_PART(modem_hdr_ofst);

	desc.opcode = IPA_HDR_INIT_LOCAL;
	desc.pyld = (void *)cmd;
	desc.len = sizeof(struct ipa_hdr_init_local);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	kfree(cmd);
fail_send_cmd:
	dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return rc;
}

int _ipa_init_hdr_v2_5(void)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer mem;
	struct ipa_hdr_init_local *cmd = NULL;
	struct ipa_hw_imm_cmd_dma_shared_mem *dma_cmd = NULL;

	mem.size = IPA_MEM_PART(modem_hdr_size) + IPA_MEM_PART(apps_hdr_size);
	mem.base = dma_alloc_coherent(ipa_ctx->pdev, mem.size, &mem.phys_base,
		GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}
	memset(mem.base, 0, mem.size);

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (cmd == NULL) {
		IPAERR("Failed to alloc header init command object\n");
		dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base,
			mem.phys_base);
		return -ENOMEM;
	}

	cmd->hdr_table_src_addr = mem.phys_base;
	cmd->size_hdr_table = mem.size;
	cmd->hdr_table_dst_addr = ipa_ctx->smem_restricted_bytes +
		IPA_MEM_PART(modem_hdr_ofst);

	desc.opcode = IPA_HDR_INIT_LOCAL;
	desc.pyld = (void *)cmd;
	desc.len = sizeof(struct ipa_hdr_init_local);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		kfree(cmd);
		dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base,
			mem.phys_base);
		return -EFAULT;
	}

	kfree(cmd);
	dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base, mem.phys_base);

	mem.size = IPA_MEM_PART(modem_hdr_proc_ctx_size) +
		IPA_MEM_PART(apps_hdr_proc_ctx_size);
	mem.base = dma_alloc_coherent(ipa_ctx->pdev, mem.size, &mem.phys_base,
		GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}
	memset(mem.base, 0, mem.size);
	memset(&desc, 0, sizeof(desc));

	dma_cmd = kzalloc(sizeof(*dma_cmd), GFP_KERNEL);
	if (dma_cmd == NULL) {
		IPAERR("Failed to alloc immediate command object\n");
		dma_free_coherent(ipa_ctx->pdev,
			mem.size,
			mem.base,
			mem.phys_base);
		return -ENOMEM;
	}

	dma_cmd->system_addr = mem.phys_base;
	dma_cmd->local_addr = ipa_ctx->smem_restricted_bytes +
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst);
	dma_cmd->size = mem.size;
	desc.opcode = IPA_DMA_SHARED_MEM;
	desc.pyld = (void *)dma_cmd;
	desc.len = sizeof(struct ipa_hw_imm_cmd_dma_shared_mem);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		kfree(dma_cmd);
		dma_free_coherent(ipa_ctx->pdev,
			mem.size,
			mem.base,
			mem.phys_base);
		return -EFAULT;
	}

	ipa_write_reg(ipa_ctx->mmio,
		IPA_LOCAL_PKT_PROC_CNTXT_BASE_OFST,
		dma_cmd->local_addr);

	kfree(dma_cmd);
	dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base, mem.phys_base);

	return 0;
}

int _ipa_init_hdr_v2_6L(void)
{
	/* Same implementation as IPAv2 */
	return _ipa_init_hdr_v2();
}

int _ipa_init_rt4_v2(void)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer mem;
	struct ipa_ip_v4_routing_init *v4_cmd = NULL;
	u32 *entry;
	int i;
	int rc = 0;

	for (i = IPA_MEM_PART(v4_modem_rt_index_lo);
		i <= IPA_MEM_PART(v4_modem_rt_index_hi);
		i++)
		ipa_ctx->rt_idx_bitmap[IPA_IP_v4] |= (1 << i);
	IPADBG("v4 rt bitmap 0x%lx\n", ipa_ctx->rt_idx_bitmap[IPA_IP_v4]);

	mem.size = IPA_MEM_PART(v4_rt_size);
	mem.base = dma_alloc_coherent(ipa_ctx->pdev, mem.size, &mem.phys_base,
			GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	entry = mem.base;
	for (i = 0; i < IPA_MEM_PART(v4_num_index); i++) {
		*entry = ipa_ctx->empty_rt_tbl_mem.phys_base;
		entry++;
	}

	v4_cmd = kzalloc(sizeof(*v4_cmd), GFP_KERNEL);
	if (v4_cmd == NULL) {
		IPAERR("Failed to alloc v4 routing init command object\n");
		rc = -ENOMEM;
		goto fail_send_cmd;
	}

	desc.opcode = IPA_IP_V4_ROUTING_INIT;
	v4_cmd->ipv4_rules_addr = mem.phys_base;
	v4_cmd->size_ipv4_rules = mem.size;
	v4_cmd->ipv4_addr = ipa_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v4_rt_ofst);
	IPADBG("putting Routing IPv4 rules to phys 0x%x",
				v4_cmd->ipv4_addr);

	desc.pyld = (void *)v4_cmd;
	desc.len = sizeof(struct ipa_ip_v4_routing_init);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	kfree(v4_cmd);
fail_send_cmd:
	dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return rc;
}

int _ipa_init_rt6_v2(void)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer mem;
	struct ipa_ip_v6_routing_init *v6_cmd = NULL;
	u32 *entry;
	int i;
	int rc = 0;

	for (i = IPA_MEM_PART(v6_modem_rt_index_lo);
		i <= IPA_MEM_PART(v6_modem_rt_index_hi);
		i++)
		ipa_ctx->rt_idx_bitmap[IPA_IP_v6] |= (1 << i);
	IPADBG("v6 rt bitmap 0x%lx\n", ipa_ctx->rt_idx_bitmap[IPA_IP_v6]);

	mem.size = IPA_MEM_PART(v6_rt_size);
	mem.base = dma_alloc_coherent(ipa_ctx->pdev, mem.size, &mem.phys_base,
			GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	entry = mem.base;
	for (i = 0; i < IPA_MEM_PART(v6_num_index); i++) {
		*entry = ipa_ctx->empty_rt_tbl_mem.phys_base;
		entry++;
	}

	v6_cmd = kzalloc(sizeof(*v6_cmd), GFP_KERNEL);
	if (v6_cmd == NULL) {
		IPAERR("Failed to alloc v6 routing init command object\n");
		rc = -ENOMEM;
		goto fail_send_cmd;
	}

	desc.opcode = IPA_IP_V6_ROUTING_INIT;
	v6_cmd->ipv6_rules_addr = mem.phys_base;
	v6_cmd->size_ipv6_rules = mem.size;
	v6_cmd->ipv6_addr = ipa_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v6_rt_ofst);
	IPADBG("putting Routing IPv6 rules to phys 0x%x",
				v6_cmd->ipv6_addr);

	desc.pyld = (void *)v6_cmd;
	desc.len = sizeof(struct ipa_ip_v6_routing_init);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	kfree(v6_cmd);
fail_send_cmd:
	dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return rc;
}

int _ipa_init_flt4_v2(void)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer mem;
	struct ipa_ip_v4_filter_init *v4_cmd = NULL;
	u32 *entry;
	int i;
	int rc = 0;

	mem.size = IPA_MEM_PART(v4_flt_size);
	mem.base = dma_alloc_coherent(ipa_ctx->pdev, mem.size, &mem.phys_base,
			GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	entry = mem.base;

	*entry = ((0xFFFFF << 1) | 0x1);
	entry++;

	for (i = 0; i <= ipa_ctx->ipa_num_pipes; i++) {
		*entry = ipa_ctx->empty_rt_tbl_mem.phys_base;
		entry++;
	}

	v4_cmd = kzalloc(sizeof(*v4_cmd), GFP_KERNEL);
	if (v4_cmd == NULL) {
		IPAERR("Failed to alloc v4 fliter init command object\n");
		rc = -ENOMEM;
		goto fail_send_cmd;
	}

	desc.opcode = IPA_IP_V4_FILTER_INIT;
	v4_cmd->ipv4_rules_addr = mem.phys_base;
	v4_cmd->size_ipv4_rules = mem.size;
	v4_cmd->ipv4_addr = ipa_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v4_flt_ofst);
	IPADBG("putting Filtering IPv4 rules to phys 0x%x",
				v4_cmd->ipv4_addr);

	desc.pyld = (void *)v4_cmd;
	desc.len = sizeof(struct ipa_ip_v4_filter_init);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	kfree(v4_cmd);
fail_send_cmd:
	dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return rc;
}

int _ipa_init_flt6_v2(void)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer mem;
	struct ipa_ip_v6_filter_init *v6_cmd = NULL;
	u32 *entry;
	int i;
	int rc = 0;

	mem.size = IPA_MEM_PART(v6_flt_size);
	mem.base = dma_alloc_coherent(ipa_ctx->pdev, mem.size, &mem.phys_base,
			GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	entry = mem.base;

	*entry = (0xFFFFF << 1) | 0x1;
	entry++;

	for (i = 0; i <= ipa_ctx->ipa_num_pipes; i++) {
		*entry = ipa_ctx->empty_rt_tbl_mem.phys_base;
		entry++;
	}

	v6_cmd = kzalloc(sizeof(*v6_cmd), GFP_KERNEL);
	if (v6_cmd == NULL) {
		IPAERR("Failed to alloc v6 fliter init command object\n");
		rc = -ENOMEM;
		goto fail_send_cmd;
	}

	desc.opcode = IPA_IP_V6_FILTER_INIT;
	v6_cmd->ipv6_rules_addr = mem.phys_base;
	v6_cmd->size_ipv6_rules = mem.size;
	v6_cmd->ipv6_addr = ipa_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v6_flt_ofst);
	IPADBG("putting Filtering IPv6 rules to phys 0x%x",
				v6_cmd->ipv6_addr);

	desc.pyld = (void *)v6_cmd;
	desc.len = sizeof(struct ipa_ip_v6_filter_init);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	kfree(v6_cmd);
fail_send_cmd:
	dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return rc;
}

static int ipa_setup_apps_pipes(void)
{
	struct ipa_sys_connect_params sys_in;
	int result = 0;

	/* CMD OUT (A5->IPA) */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_APPS_CMD_PROD;
	sys_in.desc_fifo_sz = IPA_SYS_DESC_FIFO_SZ;
	sys_in.ipa_ep_cfg.mode.mode = IPA_DMA;
	sys_in.ipa_ep_cfg.mode.dst = IPA_CLIENT_APPS_LAN_CONS;
	sys_in.skip_ep_cfg = true;
	if (ipa2_setup_sys_pipe(&sys_in, &ipa_ctx->clnt_hdl_cmd)) {
		IPAERR(":setup sys pipe failed.\n");
		result = -EPERM;
		goto fail_cmd;
	}
	IPADBG("Apps to IPA cmd pipe is connected\n");

	ipa_ctx->ctrl->ipa_init_sram();
	IPADBG("SRAM initialized\n");

	ipa_ctx->ctrl->ipa_init_hdr();
	IPADBG("HDR initialized\n");

	ipa_ctx->ctrl->ipa_init_rt4();
	IPADBG("V4 RT initialized\n");

	ipa_ctx->ctrl->ipa_init_rt6();
	IPADBG("V6 RT initialized\n");

	ipa_ctx->ctrl->ipa_init_flt4();
	IPADBG("V4 FLT initialized\n");

	ipa_ctx->ctrl->ipa_init_flt6();
	IPADBG("V6 FLT initialized\n");

	if (ipa_setup_exception_path()) {
		IPAERR(":fail to setup excp path\n");
		result = -EPERM;
		goto fail_schedule_delayed_work;
	}
	IPADBG("Exception path was successfully set");

	if (ipa_setup_dflt_rt_tables()) {
		IPAERR(":fail to setup dflt routes\n");
		result = -EPERM;
		goto fail_schedule_delayed_work;
	}
	IPADBG("default routing was set\n");

	/* LAN IN (IPA->A5) */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_APPS_LAN_CONS;
	sys_in.desc_fifo_sz = IPA_SYS_DESC_FIFO_SZ;
	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_1) {
		sys_in.ipa_ep_cfg.hdr.hdr_a5_mux = 1;
		sys_in.ipa_ep_cfg.hdr.hdr_len = IPA_A5_MUX_HEADER_LENGTH;
	} else if (ipa_ctx->ipa_hw_type >= IPA_HW_v2_0) {
		sys_in.notify = ipa_lan_rx_cb;
		sys_in.priv = NULL;
		sys_in.ipa_ep_cfg.hdr.hdr_len = IPA_LAN_RX_HEADER_LENGTH;
		sys_in.ipa_ep_cfg.hdr_ext.hdr_little_endian = false;
		sys_in.ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad_valid = true;
		sys_in.ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad = IPA_HDR_PAD;
		sys_in.ipa_ep_cfg.hdr_ext.hdr_payload_len_inc_padding = false;
		sys_in.ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad_offset = 0;
		sys_in.ipa_ep_cfg.hdr_ext.hdr_pad_to_alignment = 2;
		sys_in.ipa_ep_cfg.cfg.cs_offload_en = IPA_ENABLE_CS_OFFLOAD_DL;
	} else {
		WARN_ON(1);
	}

	/**
	 * ipa_lan_rx_cb() intended to notify the source EP about packet
	 * being received on the LAN_CONS via calling the source EP call-back.
	 * There could be a race condition with calling this call-back. Other
	 * thread may nullify it - e.g. on EP disconnect.
	 * This lock intended to protect the access to the source EP call-back
	 */
	spin_lock_init(&ipa_ctx->disconnect_lock);
	if (ipa2_setup_sys_pipe(&sys_in, &ipa_ctx->clnt_hdl_data_in)) {
		IPAERR(":setup sys pipe failed.\n");
		result = -EPERM;
		goto fail_schedule_delayed_work;
	}

	/* LAN-WAN OUT (A5->IPA) */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_APPS_LAN_WAN_PROD;
	sys_in.desc_fifo_sz = IPA_SYS_TX_DATA_DESC_FIFO_SZ;
	sys_in.ipa_ep_cfg.mode.mode = IPA_BASIC;
	if (ipa2_setup_sys_pipe(&sys_in, &ipa_ctx->clnt_hdl_data_out)) {
		IPAERR(":setup sys pipe failed.\n");
		result = -EPERM;
		goto fail_data_out;
	}

	return 0;

fail_data_out:
	ipa2_teardown_sys_pipe(ipa_ctx->clnt_hdl_data_in);
fail_schedule_delayed_work:
	if (ipa_ctx->dflt_v6_rt_rule_hdl)
		__ipa_del_rt_rule(ipa_ctx->dflt_v6_rt_rule_hdl);
	if (ipa_ctx->dflt_v4_rt_rule_hdl)
		__ipa_del_rt_rule(ipa_ctx->dflt_v4_rt_rule_hdl);
	if (ipa_ctx->excp_hdr_hdl)
		__ipa_del_hdr(ipa_ctx->excp_hdr_hdl, false);
	ipa2_teardown_sys_pipe(ipa_ctx->clnt_hdl_cmd);
fail_cmd:
	return result;
}

static void ipa_teardown_apps_pipes(void)
{
	ipa2_teardown_sys_pipe(ipa_ctx->clnt_hdl_data_out);
	ipa2_teardown_sys_pipe(ipa_ctx->clnt_hdl_data_in);
	__ipa_del_rt_rule(ipa_ctx->dflt_v6_rt_rule_hdl);
	__ipa_del_rt_rule(ipa_ctx->dflt_v4_rt_rule_hdl);
	__ipa_del_hdr(ipa_ctx->excp_hdr_hdl, false);
	ipa2_teardown_sys_pipe(ipa_ctx->clnt_hdl_cmd);
}

#ifdef CONFIG_COMPAT
long compat_ipa_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	struct ipa_ioc_nat_alloc_mem32 nat_mem32;
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
		if (copy_from_user((u8 *)&nat_mem32, (u8 *)arg,
			sizeof(struct ipa_ioc_nat_alloc_mem32))) {
			retval = -EFAULT;
			goto ret;
		}
		memcpy(nat_mem.dev_name, nat_mem32.dev_name,
				IPA_RESOURCE_NAME_MAX);
		nat_mem.size = (size_t)nat_mem32.size;
		nat_mem.offset = (off_t)nat_mem32.offset;

		/* null terminate the string */
		nat_mem.dev_name[IPA_RESOURCE_NAME_MAX - 1] = '\0';

		if (ipa2_allocate_nat_device(&nat_mem)) {
			retval = -EFAULT;
			goto ret;
		}
		nat_mem32.offset = (compat_off_t)nat_mem.offset;
		if (copy_to_user((u8 *)arg, (u8 *)&nat_mem32,
			sizeof(struct ipa_ioc_nat_alloc_mem32))) {
			retval = -EFAULT;
		}
ret:
		return retval;
	case IPA_IOC_V4_INIT_NAT32:
		cmd = IPA_IOC_V4_INIT_NAT;
		break;
	case IPA_IOC_NAT_DMA32:
		cmd = IPA_IOC_NAT_DMA;
		break;
	case IPA_IOC_V4_DEL_NAT32:
		cmd = IPA_IOC_V4_DEL_NAT;
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
	return ipa_ioctl(file, cmd, (unsigned long) compat_ptr(arg));
}
#endif

static const struct file_operations ipa_drv_fops = {
	.owner = THIS_MODULE,
	.open = ipa_open,
	.read = ipa_read,
	.unlocked_ioctl = ipa_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_ipa_ioctl,
#endif
};

static int ipa_get_clks(struct device *dev)
{
	ipa_clk = clk_get(dev, "core_clk");
	if (IS_ERR(ipa_clk)) {
		if (ipa_clk != ERR_PTR(-EPROBE_DEFER))
			IPAERR("fail to get ipa clk\n");
		return PTR_ERR(ipa_clk);
	}

	if (smmu_info.present && smmu_info.arm_smmu) {
		smmu_clk = clk_get(dev, "smmu_clk");
		if (IS_ERR(smmu_clk)) {
			if (smmu_clk != ERR_PTR(-EPROBE_DEFER))
				IPAERR("fail to get smmu clk\n");
			return PTR_ERR(smmu_clk);
		}

		if (clk_get_rate(smmu_clk) == 0) {
			long rate = clk_round_rate(smmu_clk, 1000);

			clk_set_rate(smmu_clk, rate);
		}
	}

	if (ipa_ctx->ipa_hw_type < IPA_HW_v2_0) {
		ipa_cnoc_clk = clk_get(dev, "iface_clk");
		if (IS_ERR(ipa_cnoc_clk)) {
			ipa_cnoc_clk = NULL;
			IPAERR("fail to get cnoc clk\n");
			return -ENODEV;
		}

		ipa_clk_src = clk_get(dev, "core_src_clk");
		if (IS_ERR(ipa_clk_src)) {
			ipa_clk_src = NULL;
			IPAERR("fail to get ipa clk src\n");
			return -ENODEV;
		}

		sys_noc_ipa_axi_clk = clk_get(dev, "bus_clk");
		if (IS_ERR(sys_noc_ipa_axi_clk)) {
			sys_noc_ipa_axi_clk = NULL;
			IPAERR("fail to get sys_noc_ipa_axi clk\n");
			return -ENODEV;
		}

		ipa_inactivity_clk = clk_get(dev, "inactivity_clk");
		if (IS_ERR(ipa_inactivity_clk)) {
			ipa_inactivity_clk = NULL;
			IPAERR("fail to get inactivity clk\n");
			return -ENODEV;
		}
	}

	return 0;
}

void _ipa_enable_clks_v2_0(void)
{
	IPADBG("enabling gcc_ipa_clk\n");
	if (ipa_clk) {
		clk_prepare(ipa_clk);
		clk_enable(ipa_clk);
		IPADBG("curr_ipa_clk_rate=%d", ipa_ctx->curr_ipa_clk_rate);
		clk_set_rate(ipa_clk, ipa_ctx->curr_ipa_clk_rate);
		ipa_uc_notify_clk_state(true);
	} else {
		WARN_ON(1);
	}

	if (smmu_clk)
		clk_prepare_enable(smmu_clk);
	/* Enable the BAM IRQ. */
	ipa_sps_irq_control_all(true);
	ipa_suspend_apps_pipes(false);
}

void _ipa_enable_clks_v1_1(void)
{

	if (ipa_cnoc_clk) {
		clk_prepare(ipa_cnoc_clk);
		clk_enable(ipa_cnoc_clk);
		clk_set_rate(ipa_cnoc_clk, IPA_CNOC_CLK_RATE);
	} else {
		WARN_ON(1);
	}

	if (ipa_clk_src)
		clk_set_rate(ipa_clk_src,
				ipa_ctx->curr_ipa_clk_rate);
	else
		WARN_ON(1);

	if (ipa_clk)
		clk_prepare(ipa_clk);
	else
		WARN_ON(1);

	if (sys_noc_ipa_axi_clk)
		clk_prepare(sys_noc_ipa_axi_clk);
	else
		WARN_ON(1);

	if (ipa_inactivity_clk)
		clk_prepare(ipa_inactivity_clk);
	else
		WARN_ON(1);

	if (ipa_clk)
		clk_enable(ipa_clk);
	else
		WARN_ON(1);

	if (sys_noc_ipa_axi_clk)
		clk_enable(sys_noc_ipa_axi_clk);
	else
		WARN_ON(1);

	if (ipa_inactivity_clk)
		clk_enable(ipa_inactivity_clk);
	else
		WARN_ON(1);

}

static unsigned int ipa_get_bus_vote(void)
{
	unsigned int idx = 1;

	if (ipa_ctx->curr_ipa_clk_rate == ipa_ctx->ctrl->ipa_clk_rate_svs) {
		idx = 1;
	} else if (ipa_ctx->curr_ipa_clk_rate ==
			ipa_ctx->ctrl->ipa_clk_rate_nominal) {
		if (ipa_ctx->ctrl->msm_bus_data_ptr->num_usecases <= 2)
			idx = 1;
		else
			idx = 2;
	} else if (ipa_ctx->curr_ipa_clk_rate ==
			ipa_ctx->ctrl->ipa_clk_rate_turbo) {
		idx = ipa_ctx->ctrl->msm_bus_data_ptr->num_usecases - 1;
	} else {
		WARN_ON(1);
	}

	IPADBG("curr %d idx %d\n", ipa_ctx->curr_ipa_clk_rate, idx);

	return idx;
}

/**
* ipa_enable_clks() - Turn on IPA clocks
*
* Return codes:
* None
*/
void ipa_enable_clks(void)
{
	IPADBG("enabling IPA clocks and bus voting\n");

	ipa_ctx->ctrl->ipa_enable_clks();

	if (ipa_ctx->ipa_hw_mode != IPA_HW_MODE_VIRTUAL)
		if (msm_bus_scale_client_update_request(ipa_ctx->ipa_bus_hdl,
		    ipa_get_bus_vote()))
			WARN_ON(1);
}

void _ipa_disable_clks_v1_1(void)
{

	if (ipa_inactivity_clk)
		clk_disable_unprepare(ipa_inactivity_clk);
	else
		WARN_ON(1);

	if (sys_noc_ipa_axi_clk)
		clk_disable_unprepare(sys_noc_ipa_axi_clk);
	else
		WARN_ON(1);

	if (ipa_clk)
		clk_disable_unprepare(ipa_clk);
	else
		WARN_ON(1);

	if (ipa_cnoc_clk)
		clk_disable_unprepare(ipa_cnoc_clk);
	else
		WARN_ON(1);

}

void _ipa_disable_clks_v2_0(void)
{
	IPADBG("disabling gcc_ipa_clk\n");
	ipa_suspend_apps_pipes(true);
	ipa_sps_irq_control_all(false);
	ipa_uc_notify_clk_state(false);
	if (ipa_clk)
		clk_disable_unprepare(ipa_clk);
	else
		WARN_ON(1);

	if (smmu_clk)
		clk_disable_unprepare(smmu_clk);
}

/**
* ipa_disable_clks() - Turn off IPA clocks
*
* Return codes:
* None
*/
void ipa_disable_clks(void)
{
	IPADBG("disabling IPA clocks and bus voting\n");

	ipa_ctx->ctrl->ipa_disable_clks();

	if (ipa_ctx->ipa_hw_mode != IPA_HW_MODE_VIRTUAL)
		if (msm_bus_scale_client_update_request(ipa_ctx->ipa_bus_hdl,
		    0))
			WARN_ON(1);
}

/**
 * ipa_start_tag_process() - Send TAG packet and wait for it to come back
 *
 * This function is called prior to clock gating when active client counter
 * is 1. TAG process ensures that there are no packets inside IPA HW that
 * were not submitted to peer's BAM. During TAG process all aggregation frames
 * are (force) closed.
 *
 * Return codes:
 * None
 */
static void ipa_start_tag_process(struct work_struct *work)
{
	int res;

	IPADBG("starting TAG process\n");
	/* close aggregation frames on all pipes */
	res = ipa_tag_aggr_force_close(-1);
	if (res)
		IPAERR("ipa_tag_aggr_force_close failed %d\n", res);

	IPA_ACTIVE_CLIENTS_DEC_SPECIAL("TAG_PROCESS");

	IPADBG("TAG process done\n");
}

/**
* ipa2_active_clients_log_mod() - Log a modification in the active clients
* reference count
*
* This method logs any modification in the active clients reference count:
* It logs the modification in the circular history buffer
* It logs the modification in the hash table - looking for an entry,
* creating one if needed and deleting one if needed.
*
* @id: ipa2_active client logging info struct to hold the log information
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
void ipa2_active_clients_log_mod(struct ipa_active_client_logging_info *id,
		bool inc, bool int_ctx)
{
	char temp_str[IPA2_ACTIVE_CLIENTS_LOG_LINE_LEN];
	unsigned long long t;
	unsigned long nanosec_rem;
	struct ipa2_active_client_htable_entry *hentry;
	struct ipa2_active_client_htable_entry *hfound;
	u32 hkey;
	char str_to_hash[IPA2_ACTIVE_CLIENTS_LOG_NAME_LEN];

	hfound = NULL;
	memset(str_to_hash, 0, IPA2_ACTIVE_CLIENTS_LOG_NAME_LEN);
	strlcpy(str_to_hash, id->id_string, IPA2_ACTIVE_CLIENTS_LOG_NAME_LEN);
	hkey = arch_fast_hash(str_to_hash, IPA2_ACTIVE_CLIENTS_LOG_NAME_LEN,
			0);
	hash_for_each_possible(ipa_ctx->ipa2_active_clients_logging.htable,
			hentry, list, hkey) {
		if (!strcmp(hentry->id_string, id->id_string)) {
			hentry->count = hentry->count + (inc ? 1 : -1);
			hfound = hentry;
		}
	}
	if (hfound == NULL) {
		hentry = NULL;
		hentry = kzalloc(sizeof(
				struct ipa2_active_client_htable_entry),
				int_ctx ? GFP_ATOMIC : GFP_KERNEL);
		if (hentry == NULL) {
			IPAERR("failed allocating active clients hash entry");
			return;
		}
		hentry->type = id->type;
		strlcpy(hentry->id_string, id->id_string,
				IPA2_ACTIVE_CLIENTS_LOG_NAME_LEN);
		INIT_HLIST_NODE(&hentry->list);
		hentry->count = inc ? 1 : -1;
		hash_add(ipa_ctx->ipa2_active_clients_logging.htable,
				&hentry->list, hkey);
	} else if (hfound->count == 0) {
		hash_del(&hfound->list);
		kfree(hfound);
	}

	if (id->type != SIMPLE) {
		t = local_clock();
		nanosec_rem = do_div(t, 1000000000) / 1000;
		snprintf(temp_str, IPA2_ACTIVE_CLIENTS_LOG_LINE_LEN,
				inc ? "[%5lu.%06lu] ^ %s, %s: %d" :
						"[%5lu.%06lu] v %s, %s: %d",
				(unsigned long)t, nanosec_rem,
				id->id_string, id->file, id->line);
		ipa2_active_clients_log_insert(temp_str);
	}
}

void ipa2_active_clients_log_dec(struct ipa_active_client_logging_info *id,
		bool int_ctx)
{
	ipa2_active_clients_log_mod(id, false, int_ctx);
}

void ipa2_active_clients_log_inc(struct ipa_active_client_logging_info *id,
		bool int_ctx)
{
	ipa2_active_clients_log_mod(id, true, int_ctx);
}

/**
* ipa_inc_client_enable_clks() - Increase active clients counter, and
* enable ipa clocks if necessary
*
* Please do not use this API, use the wrapper macros instead (ipa_i.h)
* IPA2_ACTIVE_CLIENTS_INC_XXXX();
*
* Return codes:
* None
*/
void ipa2_inc_client_enable_clks(struct ipa_active_client_logging_info *id)
{
	ipa_active_clients_lock();
	ipa2_active_clients_log_inc(id, false);
	ipa_ctx->ipa_active_clients.cnt++;
	if (ipa_ctx->ipa_active_clients.cnt == 1)
		ipa_enable_clks();
	IPADBG("active clients = %d\n", ipa_ctx->ipa_active_clients.cnt);
	ipa_active_clients_unlock();
}

/**
* ipa_inc_client_enable_clks_no_block() - Only increment the number of active
* clients if no asynchronous actions should be done. Asynchronous actions are
* locking a mutex and waking up IPA HW.
*
* Please do not use this API, use the wrapper macros instead (ipa_i.h)
*
*
* Return codes: 0 for success
*		-EPERM if an asynchronous action should have been done
*/
int ipa2_inc_client_enable_clks_no_block(struct ipa_active_client_logging_info
		*id)
{
	int res = 0;
	unsigned long flags;

	if (ipa_active_clients_trylock(&flags) == 0)
		return -EPERM;

	if (ipa_ctx->ipa_active_clients.cnt == 0) {
		res = -EPERM;
		goto bail;
	}

	ipa2_active_clients_log_inc(id, true);

	ipa_ctx->ipa_active_clients.cnt++;
	IPADBG("active clients = %d\n", ipa_ctx->ipa_active_clients.cnt);
bail:
	ipa_active_clients_trylock_unlock(&flags);

	return res;
}

/**
 * ipa_dec_client_disable_clks() - Decrease active clients counter
 *
 * In case that there are no active clients this function also starts
 * TAG process. When TAG progress ends ipa clocks will be gated.
 * start_tag_process_again flag is set during this function to signal TAG
 * process to start again as there was another client that may send data to ipa
 *
 * Please do not use this API, use the wrapper macros instead (ipa_i.h)
 * IPA2_ACTIVE_CLIENTS_DEC_XXXX();
 *
 * Return codes:
 * None
 */
void ipa2_dec_client_disable_clks(struct ipa_active_client_logging_info *id)
{
	struct ipa_active_client_logging_info log_info;

	ipa_active_clients_lock();
	ipa2_active_clients_log_dec(id, false);
	ipa_ctx->ipa_active_clients.cnt--;
	IPADBG("active clients = %d\n", ipa_ctx->ipa_active_clients.cnt);
	if (ipa_ctx->ipa_active_clients.cnt == 0) {
		if (ipa_ctx->tag_process_before_gating) {
			IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info,
					"TAG_PROCESS");
			ipa2_active_clients_log_inc(&log_info, false);
			ipa_ctx->tag_process_before_gating = false;
			/*
			 * When TAG process ends, active clients will be
			 * decreased
			 */
			ipa_ctx->ipa_active_clients.cnt = 1;
			queue_work(ipa_ctx->power_mgmt_wq, &ipa_tag_work);
		} else {
			ipa_disable_clks();
		}
	}
	ipa_active_clients_unlock();
}

/**
* ipa_inc_acquire_wakelock() - Increase active clients counter, and
* acquire wakelock if necessary
*
* Return codes:
* None
*/
void ipa_inc_acquire_wakelock(enum ipa_wakelock_ref_client ref_client)
{
	unsigned long flags;

	if (ref_client >= IPA_WAKELOCK_REF_CLIENT_MAX)
		return;
	spin_lock_irqsave(&ipa_ctx->wakelock_ref_cnt.spinlock, flags);
	if (ipa_ctx->wakelock_ref_cnt.cnt & (1 << ref_client))
		IPADBG("client enum %d mask already set. ref cnt = %d\n",
		ref_client, ipa_ctx->wakelock_ref_cnt.cnt);
	ipa_ctx->wakelock_ref_cnt.cnt |= (1 << ref_client);
	if (ipa_ctx->wakelock_ref_cnt.cnt)
		__pm_stay_awake(&ipa_ctx->w_lock);
	IPADBG("active wakelock ref cnt = %d client enum %d\n",
		ipa_ctx->wakelock_ref_cnt.cnt, ref_client);
	spin_unlock_irqrestore(&ipa_ctx->wakelock_ref_cnt.spinlock, flags);
}

/**
 * ipa_dec_release_wakelock() - Decrease active clients counter
 *
 * In case if the ref count is 0, release the wakelock.
 *
 * Return codes:
 * None
 */
void ipa_dec_release_wakelock(enum ipa_wakelock_ref_client ref_client)
{
	unsigned long flags;

	if (ref_client >= IPA_WAKELOCK_REF_CLIENT_MAX)
		return;
	spin_lock_irqsave(&ipa_ctx->wakelock_ref_cnt.spinlock, flags);
	ipa_ctx->wakelock_ref_cnt.cnt &= ~(1 << ref_client);
	IPADBG("active wakelock ref cnt = %d client enum %d\n",
		ipa_ctx->wakelock_ref_cnt.cnt, ref_client);
	if (ipa_ctx->wakelock_ref_cnt.cnt == 0)
		__pm_relax(&ipa_ctx->w_lock);
	spin_unlock_irqrestore(&ipa_ctx->wakelock_ref_cnt.spinlock, flags);
}

static int ipa_setup_bam_cfg(const struct ipa_plat_drv_res *res)
{
	void *ipa_bam_mmio;
	int reg_val;
	int retval = 0;

	ipa_bam_mmio = ioremap(res->ipa_mem_base + IPA_BAM_REG_BASE_OFST,
			IPA_BAM_REMAP_SIZE);
	if (!ipa_bam_mmio)
		return -ENOMEM;
	switch (ipa_ctx->ipa_hw_type) {
	case IPA_HW_v1_1:
		reg_val = IPA_BAM_CNFG_BITS_VALv1_1;
		break;
	case IPA_HW_v2_0:
	case IPA_HW_v2_5:
	case IPA_HW_v2_6L:
		reg_val = IPA_BAM_CNFG_BITS_VALv2_0;
		break;
	default:
		retval = -EPERM;
		goto fail;
	}
	if (ipa_ctx->ipa_hw_type < IPA_HW_v2_5)
		ipa_write_reg(ipa_bam_mmio, IPA_BAM_CNFG_BITS_OFST, reg_val);
fail:
	iounmap(ipa_bam_mmio);

	return retval;
}

int ipa2_set_required_perf_profile(enum ipa_voltage_level floor_voltage,
				  u32 bandwidth_mbps)
{
	enum ipa_voltage_level needed_voltage;
	u32 clk_rate;

	IPADBG("floor_voltage=%d, bandwidth_mbps=%u",
					floor_voltage, bandwidth_mbps);

	if (floor_voltage < IPA_VOLTAGE_UNSPECIFIED ||
		floor_voltage >= IPA_VOLTAGE_MAX) {
		IPAERR("bad voltage\n");
		return -EINVAL;
	}

	if (ipa_ctx->enable_clock_scaling) {
		IPADBG("Clock scaling is enabled\n");
		if (bandwidth_mbps >=
			ipa_ctx->ctrl->clock_scaling_bw_threshold_turbo)
			needed_voltage = IPA_VOLTAGE_TURBO;
		else if (bandwidth_mbps >=
			ipa_ctx->ctrl->clock_scaling_bw_threshold_nominal)
			needed_voltage = IPA_VOLTAGE_NOMINAL;
		else
			needed_voltage = IPA_VOLTAGE_SVS;
	} else {
		IPADBG("Clock scaling is disabled\n");
		needed_voltage = IPA_VOLTAGE_NOMINAL;
	}

	needed_voltage = max(needed_voltage, floor_voltage);
	switch (needed_voltage) {
	case IPA_VOLTAGE_SVS:
		clk_rate = ipa_ctx->ctrl->ipa_clk_rate_svs;
		break;
	case IPA_VOLTAGE_NOMINAL:
		clk_rate = ipa_ctx->ctrl->ipa_clk_rate_nominal;
		break;
	case IPA_VOLTAGE_TURBO:
		clk_rate = ipa_ctx->ctrl->ipa_clk_rate_turbo;
		break;
	default:
		IPAERR("bad voltage\n");
		WARN_ON(1);
		return -EFAULT;
	}

	if (clk_rate == ipa_ctx->curr_ipa_clk_rate) {
		IPADBG("Same voltage\n");
		return 0;
	}

	ipa_active_clients_lock();
	ipa_ctx->curr_ipa_clk_rate = clk_rate;
	IPADBG("setting clock rate to %u\n", ipa_ctx->curr_ipa_clk_rate);
	if (ipa_ctx->ipa_active_clients.cnt > 0) {
		clk_set_rate(ipa_clk, ipa_ctx->curr_ipa_clk_rate);
		if (ipa_ctx->ipa_hw_mode != IPA_HW_MODE_VIRTUAL)
			if (msm_bus_scale_client_update_request(
			    ipa_ctx->ipa_bus_hdl, ipa_get_bus_vote()))
				WARN_ON(1);
	} else {
		IPADBG("clocks are gated, not setting rate\n");
	}
	ipa_active_clients_unlock();
	IPADBG("Done\n");
	return 0;
}

static int ipa_init_flt_block(void)
{
	int result = 0;

	/*
	 * SW workaround for Improper Filter Behavior when neither Global nor
	 * Pipe Rules are present => configure dummy global filter rule
	 * always which results in a miss
	 */
	struct ipa_ioc_add_flt_rule *rules;
	struct ipa_flt_rule_add *rule;
	struct ipa_ioc_get_rt_tbl rt_lookup;
	enum ipa_ip_type ip;

	if (ipa_ctx->ipa_hw_type >= IPA_HW_v1_1) {
		size_t sz = sizeof(struct ipa_ioc_add_flt_rule) +
		   sizeof(struct ipa_flt_rule_add);

		rules = kmalloc(sz, GFP_KERNEL);
		if (rules == NULL) {
			IPAERR("fail to alloc mem for dummy filter rule\n");
			return -ENOMEM;
		}

		IPADBG("Adding global rules for IPv4 and IPv6");
		for (ip = IPA_IP_v4; ip < IPA_IP_MAX; ip++) {
			memset(&rt_lookup, 0,
					sizeof(struct ipa_ioc_get_rt_tbl));
			rt_lookup.ip = ip;
			strlcpy(rt_lookup.name, IPA_DFLT_RT_TBL_NAME,
					IPA_RESOURCE_NAME_MAX);
			ipa2_get_rt_tbl(&rt_lookup);
			ipa2_put_rt_tbl(rt_lookup.hdl);

			memset(rules, 0, sz);
			rule = &rules->rules[0];
			rules->commit = 1;
			rules->ip = ip;
			rules->global = 1;
			rules->num_rules = 1;
			rule->at_rear = 1;
			if (ip == IPA_IP_v4) {
				rule->rule.attrib.attrib_mask =
					IPA_FLT_PROTOCOL | IPA_FLT_DST_ADDR;
				rule->rule.attrib.u.v4.protocol =
				   IPA_INVALID_L4_PROTOCOL;
				rule->rule.attrib.u.v4.dst_addr_mask = ~0;
				rule->rule.attrib.u.v4.dst_addr = ~0;
			} else if (ip == IPA_IP_v6) {
				rule->rule.attrib.attrib_mask =
					IPA_FLT_NEXT_HDR | IPA_FLT_DST_ADDR;
				rule->rule.attrib.u.v6.next_hdr =
					IPA_INVALID_L4_PROTOCOL;
				rule->rule.attrib.u.v6.dst_addr_mask[0] = ~0;
				rule->rule.attrib.u.v6.dst_addr_mask[1] = ~0;
				rule->rule.attrib.u.v6.dst_addr_mask[2] = ~0;
				rule->rule.attrib.u.v6.dst_addr_mask[3] = ~0;
				rule->rule.attrib.u.v6.dst_addr[0] = ~0;
				rule->rule.attrib.u.v6.dst_addr[1] = ~0;
				rule->rule.attrib.u.v6.dst_addr[2] = ~0;
				rule->rule.attrib.u.v6.dst_addr[3] = ~0;
			} else {
				result = -EINVAL;
				WARN_ON(1);
				break;
			}
			rule->rule.action = IPA_PASS_TO_ROUTING;
			rule->rule.rt_tbl_hdl = rt_lookup.hdl;
			rule->rule.retain_hdr = true;

			if (ipa2_add_flt_rule(rules) ||
			rules->rules[0].status) {

				result = -EINVAL;
				WARN_ON(1);
				break;
			}
		}
		kfree(rules);
	}
	return result;
}

static void ipa_sps_process_irq_schedule_rel(void)
{
	queue_delayed_work(ipa_ctx->sps_power_mgmt_wq,
		&ipa_sps_release_resource_work,
		msecs_to_jiffies(IPA_SPS_PROD_TIMEOUT_MSEC));
}

/**
* ipa_suspend_handler() - Handles the suspend interrupt:
* wakes up the suspended peripheral by requesting its consumer
* @interrupt:		Interrupt type
* @private_data:	The client's private data
* @interrupt_data:	Interrupt specific information data
*/
void ipa_suspend_handler(enum ipa_irq_type interrupt,
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

	IPADBG("interrupt=%d, interrupt_data=%u\n", interrupt, suspend_data);
	memset(&holb_cfg, 0, sizeof(holb_cfg));
	holb_cfg.tmr_val = 0;

	for (i = 0; i < ipa_ctx->ipa_num_pipes; i++) {
		if ((suspend_data & bmsk) && (ipa_ctx->ep[i].valid)) {
			if (IPA_CLIENT_IS_APPS_CONS(ipa_ctx->ep[i].client)) {
				/*
				 * pipe will be unsuspended as part of
				 * enabling IPA clocks
				 */
				if (!atomic_read(
					&ipa_ctx->sps_pm.dec_clients)
					) {
					IPA_ACTIVE_CLIENTS_INC_EP(
							ipa_ctx->ep[i].client);
					IPADBG("Pipes un-suspended.\n");
					IPADBG("Enter poll mode.\n");
					atomic_set(
						&ipa_ctx->sps_pm.dec_clients,
						1);
					ipa_sps_process_irq_schedule_rel();
				}
			} else {
				resource = ipa2_get_rm_resource_from_ep(i);
				res = ipa_rm_request_resource_with_timer(
					resource);
				if (res == -EPERM &&
				    IPA_CLIENT_IS_CONS(
					ipa_ctx->ep[i].client)) {
					holb_cfg.en = 1;
					res = ipa2_cfg_ep_holb_by_client(
					   ipa_ctx->ep[i].client, &holb_cfg);
					if (res) {
						IPAERR("holb en fail\n");
						IPAERR("IPAHW stall\n");
						BUG();
					}
				}
			}
		}
			bmsk = bmsk << 1;
	}
}

/**
* ipa2_restore_suspend_handler() - restores the original suspend IRQ handler
* as it was registered in the IPA init sequence.
* Return codes:
* 0: success
* -EPERM: failed to remove current handler or failed to add original handler
* */
int ipa2_restore_suspend_handler(void)
{
	int result = 0;

	result  = ipa2_remove_interrupt_handler(IPA_TX_SUSPEND_IRQ);
	if (result) {
		IPAERR("remove handler for suspend interrupt failed\n");
		return -EPERM;
	}

	result = ipa2_add_interrupt_handler(IPA_TX_SUSPEND_IRQ,
			ipa_suspend_handler, true, NULL);
	if (result) {
		IPAERR("register handler for suspend interrupt failed\n");
		result = -EPERM;
	}

	return result;
}

static int apps_cons_release_resource(void)
{
	return 0;
}

static int apps_cons_request_resource(void)
{
	return 0;
}

static void ipa_sps_release_resource(struct work_struct *work)
{
	mutex_lock(&ipa_ctx->sps_pm.sps_pm_lock);
	/* check whether still need to decrease client usage */
	if (atomic_read(&ipa_ctx->sps_pm.dec_clients)) {
		if (atomic_read(&ipa_ctx->sps_pm.eot_activity)) {
			IPADBG("EOT pending Re-scheduling\n");
			ipa_sps_process_irq_schedule_rel();
		} else {
			atomic_set(&ipa_ctx->sps_pm.dec_clients, 0);
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL("SPS_RESOURCE");
		}
	}
	atomic_set(&ipa_ctx->sps_pm.eot_activity, 0);
	mutex_unlock(&ipa_ctx->sps_pm.sps_pm_lock);
}

int ipa_create_apps_resource(void)
{
	struct ipa_rm_create_params apps_cons_create_params;
	struct ipa_rm_perf_profile profile;
	int result = 0;

	memset(&apps_cons_create_params, 0,
				sizeof(apps_cons_create_params));
	apps_cons_create_params.name = IPA_RM_RESOURCE_APPS_CONS;
	apps_cons_create_params.request_resource = apps_cons_request_resource;
	apps_cons_create_params.release_resource = apps_cons_release_resource;
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
* ipa_init() - Initialize the IPA Driver
* @resource_p:	contain platform specific values from DST file
* @pdev:	The platform device structure representing the IPA driver
*
* Function initialization process:
* - Allocate memory for the driver context data struct
* - Initializing the ipa_ctx with:
*    1)parsed values from the dts file
*    2)parameters passed to the module initialization
*    3)read HW values(such as core memory size)
* - Map IPA core registers to CPU memory
* - Restart IPA core(HW reset)
* - Register IPA BAM to SPS driver and get a BAM handler
* - Set configuration for IPA BAM via BAM_CNFG_BITS
* - Initialize the look-aside caches(kmem_cache/slab) for filter,
*   routing and IPA-tree
* - Create memory pool with 4 objects for DMA operations(each object
*   is 512Bytes long), this object will be use for tx(A5->IPA)
* - Initialize lists head(routing,filter,hdr,system pipes)
* - Initialize mutexes (for ipa_ctx and NAT memory mutexes)
* - Initialize spinlocks (for list related to A5<->IPA pipes)
* - Initialize 2 single-threaded work-queue named "ipa rx wq" and "ipa tx wq"
* - Initialize Red-Black-Tree(s) for handles of header,routing rule,
*   routing table ,filtering rule
* - Setup all A5<->IPA pipes by calling to ipa_setup_a5_pipes
* - Preparing the descriptors for System pipes
* - Initialize the filter block by committing IPV4 and IPV6 default rules
* - Create empty routing table in system memory(no committing)
* - Initialize pipes memory pool with ipa_pipe_mem_init for supported platforms
* - Create a char-device for IPA
* - Initialize IPA RM (resource manager)
*/
static int ipa_init(const struct ipa_plat_drv_res *resource_p,
		struct device *ipa_dev)
{
	int result = 0;
	int i;
	struct sps_bam_props bam_props = { 0 };
	struct ipa_flt_tbl *flt_tbl;
	struct ipa_rt_tbl_set *rset;
	struct ipa_active_client_logging_info log_info;

	IPADBG("IPA Driver initialization started\n");

	/*
	 * since structure alignment is implementation dependent, add test to
	 * avoid different and incompatible data layouts
	 */
	BUILD_BUG_ON(sizeof(struct ipa_hw_pkt_status) != IPA_PKT_STATUS_SIZE);

	ipa_ctx = kzalloc(sizeof(*ipa_ctx), GFP_KERNEL);
	if (!ipa_ctx) {
		IPAERR(":kzalloc err.\n");
		result = -ENOMEM;
		goto fail_mem_ctx;
	}

	ipa_ctx->pdev = ipa_dev;
	ipa_ctx->uc_pdev = ipa_dev;
	ipa_ctx->smmu_present = smmu_info.present;
	if (!ipa_ctx->smmu_present)
		ipa_ctx->smmu_s1_bypass = true;
	else
		ipa_ctx->smmu_s1_bypass = smmu_info.s1_bypass;
	ipa_ctx->ipa_wrapper_base = resource_p->ipa_mem_base;
	ipa_ctx->ipa_wrapper_size = resource_p->ipa_mem_size;
	ipa_ctx->ipa_hw_type = resource_p->ipa_hw_type;
	ipa_ctx->ipa_hw_mode = resource_p->ipa_hw_mode;
	ipa_ctx->use_ipa_teth_bridge = resource_p->use_ipa_teth_bridge;
	ipa_ctx->ipa_bam_remote_mode = resource_p->ipa_bam_remote_mode;
	ipa_ctx->modem_cfg_emb_pipe_flt = resource_p->modem_cfg_emb_pipe_flt;
	ipa_ctx->wan_rx_ring_size = resource_p->wan_rx_ring_size;
	ipa_ctx->lan_rx_ring_size = resource_p->lan_rx_ring_size;
	ipa_ctx->skip_uc_pipe_reset = resource_p->skip_uc_pipe_reset;
	ipa_ctx->use_dma_zone = resource_p->use_dma_zone;
	ipa_ctx->tethered_flow_control = resource_p->tethered_flow_control;

	/* Setting up IPA RX Polling Timeout Seconds */
	ipa_rx_timeout_min_max_calc(&ipa_ctx->ipa_rx_min_timeout_usec,
		&ipa_ctx->ipa_rx_max_timeout_usec,
		resource_p->ipa_rx_polling_sleep_msec);

	/* Setting up ipa polling iteration */
	if ((resource_p->ipa_polling_iteration >= MIN_POLLING_ITERATION)
		&& (resource_p->ipa_polling_iteration <= MAX_POLLING_ITERATION))
		ipa_ctx->ipa_polling_iteration =
			resource_p->ipa_polling_iteration;
	else
		ipa_ctx->ipa_polling_iteration = MAX_POLLING_ITERATION;

	/* default aggregation parameters */
	ipa_ctx->aggregation_type = IPA_MBIM_16;
	ipa_ctx->aggregation_byte_limit = 1;
	ipa_ctx->aggregation_time_limit = 0;
	ipa_ctx->ipa2_active_clients_logging.log_rdy = false;

	ipa_ctx->ctrl = kzalloc(sizeof(*ipa_ctx->ctrl), GFP_KERNEL);
	if (!ipa_ctx->ctrl) {
		IPAERR("memory allocation error for ctrl\n");
		result = -ENOMEM;
		goto fail_mem_ctrl;
	}
	result = ipa_controller_static_bind(ipa_ctx->ctrl,
			ipa_ctx->ipa_hw_type);
	if (result) {
		IPAERR("fail to static bind IPA ctrl.\n");
		result = -EFAULT;
		goto fail_bind;
	}

	IPADBG("hdr_lcl=%u ip4_rt=%u ip6_rt=%u ip4_flt=%u ip6_flt=%u\n",
	       ipa_ctx->hdr_tbl_lcl, ipa_ctx->ip4_rt_tbl_lcl,
	       ipa_ctx->ip6_rt_tbl_lcl, ipa_ctx->ip4_flt_tbl_lcl,
	       ipa_ctx->ip6_flt_tbl_lcl);

	if (bus_scale_table) {
		IPADBG("Use bus scaling info from device tree\n");
		ipa_ctx->ctrl->msm_bus_data_ptr = bus_scale_table;
	}

	if (ipa_ctx->ipa_hw_mode != IPA_HW_MODE_VIRTUAL) {
		/* get BUS handle */
		ipa_ctx->ipa_bus_hdl =
			msm_bus_scale_register_client(
				ipa_ctx->ctrl->msm_bus_data_ptr);
		if (!ipa_ctx->ipa_bus_hdl) {
			IPAERR("fail to register with bus mgr!\n");
			result = -ENODEV;
			goto fail_bus_reg;
		}
	} else {
		IPADBG("Skipping bus scaling registration on Virtual plat\n");
	}

	if (ipa2_active_clients_log_init())
		goto fail_init_active_client;

	/* get IPA clocks */
	result = ipa_get_clks(master_dev);
	if (result)
		goto fail_clk;

	/* Enable ipa_ctx->enable_clock_scaling */
	ipa_ctx->enable_clock_scaling = 1;
	ipa_ctx->curr_ipa_clk_rate = ipa_ctx->ctrl->ipa_clk_rate_turbo;

	/* enable IPA clocks explicitly to allow the initialization */
	ipa_enable_clks();

	/* setup IPA register access */
	ipa_ctx->mmio = ioremap(resource_p->ipa_mem_base +
			ipa_ctx->ctrl->ipa_reg_base_ofst,
			resource_p->ipa_mem_size);
	if (!ipa_ctx->mmio) {
		IPAERR(":ipa-base ioremap err.\n");
		result = -EFAULT;
		goto fail_remap;
	}

	result = ipa_init_hw();
	if (result) {
		IPAERR(":error initializing HW.\n");
		result = -ENODEV;
		goto fail_init_hw;
	}
	IPADBG("IPA HW initialization sequence completed");

	ipa_ctx->ipa_num_pipes = ipa_get_num_pipes();
	ipa_ctx->ctrl->ipa_sram_read_settings();
	IPADBG("SRAM, size: 0x%x, restricted bytes: 0x%x\n",
		ipa_ctx->smem_sz, ipa_ctx->smem_restricted_bytes);

	if (ipa_ctx->smem_reqd_sz >
		ipa_ctx->smem_sz - ipa_ctx->smem_restricted_bytes) {
		IPAERR("SW expect more core memory, needed %d, avail %d\n",
			ipa_ctx->smem_reqd_sz, ipa_ctx->smem_sz -
			ipa_ctx->smem_restricted_bytes);
		result = -ENOMEM;
		goto fail_init_hw;
	}

	mutex_init(&ipa_ctx->ipa_active_clients.mutex);
	spin_lock_init(&ipa_ctx->ipa_active_clients.spinlock);
	IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info, "PROXY_CLK_VOTE");
	ipa2_active_clients_log_inc(&log_info, false);
	ipa_ctx->ipa_active_clients.cnt = 1;

	/* Create workqueues for power management */
	ipa_ctx->power_mgmt_wq =
		create_singlethread_workqueue("ipa_power_mgmt");
	if (!ipa_ctx->power_mgmt_wq) {
		IPAERR("failed to create power mgmt wq\n");
		result = -ENOMEM;
		goto fail_init_hw;
	}

	ipa_ctx->sps_power_mgmt_wq =
		create_singlethread_workqueue("sps_ipa_power_mgmt");
	if (!ipa_ctx->sps_power_mgmt_wq) {
		IPAERR("failed to create sps power mgmt wq\n");
		result = -ENOMEM;
		goto fail_create_sps_wq;
	}

	/* register IPA with SPS driver */
	bam_props.phys_addr = resource_p->bam_mem_base;
	bam_props.virt_size = resource_p->bam_mem_size;
	bam_props.irq = resource_p->bam_irq;
	bam_props.num_pipes = ipa_ctx->ipa_num_pipes;
	bam_props.summing_threshold = IPA_SUMMING_THRESHOLD;
	bam_props.event_threshold = IPA_EVENT_THRESHOLD;
	bam_props.options |= SPS_BAM_NO_LOCAL_CLK_GATING;
	if (ipa_ctx->ipa_hw_mode != IPA_HW_MODE_VIRTUAL)
		bam_props.options |= SPS_BAM_OPT_IRQ_WAKEUP;
	if (ipa_ctx->ipa_bam_remote_mode == true)
		bam_props.manage |= SPS_BAM_MGR_DEVICE_REMOTE;
	if (!ipa_ctx->smmu_s1_bypass)
		bam_props.options |= SPS_BAM_SMMU_EN;
	bam_props.options |= SPS_BAM_CACHED_WP;
	bam_props.ee = resource_p->ee;
	bam_props.ipc_loglevel = 3;

	result = sps_register_bam_device(&bam_props, &ipa_ctx->bam_handle);
	if (result) {
		IPAERR(":bam register err.\n");
		result = -EPROBE_DEFER;
		goto fail_register_bam_device;
	}
	IPADBG("IPA BAM is registered\n");

	if (ipa_setup_bam_cfg(resource_p)) {
		IPAERR(":bam cfg err.\n");
		result = -ENODEV;
		goto fail_flt_rule_cache;
	}

	/* init the lookaside cache */
	ipa_ctx->flt_rule_cache = kmem_cache_create("IPA_FLT",
			sizeof(struct ipa_flt_entry), 0, 0, NULL);
	if (!ipa_ctx->flt_rule_cache) {
		IPAERR(":ipa flt cache create failed\n");
		result = -ENOMEM;
		goto fail_flt_rule_cache;
	}
	ipa_ctx->rt_rule_cache = kmem_cache_create("IPA_RT",
			sizeof(struct ipa_rt_entry), 0, 0, NULL);
	if (!ipa_ctx->rt_rule_cache) {
		IPAERR(":ipa rt cache create failed\n");
		result = -ENOMEM;
		goto fail_rt_rule_cache;
	}
	ipa_ctx->hdr_cache = kmem_cache_create("IPA_HDR",
			sizeof(struct ipa_hdr_entry), 0, 0, NULL);
	if (!ipa_ctx->hdr_cache) {
		IPAERR(":ipa hdr cache create failed\n");
		result = -ENOMEM;
		goto fail_hdr_cache;
	}
	ipa_ctx->hdr_offset_cache =
	   kmem_cache_create("IPA_HDR_OFFSET",
			   sizeof(struct ipa_hdr_offset_entry), 0, 0, NULL);
	if (!ipa_ctx->hdr_offset_cache) {
		IPAERR(":ipa hdr off cache create failed\n");
		result = -ENOMEM;
		goto fail_hdr_offset_cache;
	}
	ipa_ctx->hdr_proc_ctx_cache = kmem_cache_create("IPA_HDR_PROC_CTX",
		sizeof(struct ipa_hdr_proc_ctx_entry), 0, 0, NULL);
	if (!ipa_ctx->hdr_proc_ctx_cache) {
		IPAERR(":ipa hdr proc ctx cache create failed\n");
		result = -ENOMEM;
		goto fail_hdr_proc_ctx_cache;
	}
	ipa_ctx->hdr_proc_ctx_offset_cache =
		kmem_cache_create("IPA_HDR_PROC_CTX_OFFSET",
		sizeof(struct ipa_hdr_proc_ctx_offset_entry), 0, 0, NULL);
	if (!ipa_ctx->hdr_proc_ctx_offset_cache) {
		IPAERR(":ipa hdr proc ctx off cache create failed\n");
		result = -ENOMEM;
		goto fail_hdr_proc_ctx_offset_cache;
	}
	ipa_ctx->rt_tbl_cache = kmem_cache_create("IPA_RT_TBL",
			sizeof(struct ipa_rt_tbl), 0, 0, NULL);
	if (!ipa_ctx->rt_tbl_cache) {
		IPAERR(":ipa rt tbl cache create failed\n");
		result = -ENOMEM;
		goto fail_rt_tbl_cache;
	}
	ipa_ctx->tx_pkt_wrapper_cache =
	   kmem_cache_create("IPA_TX_PKT_WRAPPER",
			   sizeof(struct ipa_tx_pkt_wrapper), 0, 0, NULL);
	if (!ipa_ctx->tx_pkt_wrapper_cache) {
		IPAERR(":ipa tx pkt wrapper cache create failed\n");
		result = -ENOMEM;
		goto fail_tx_pkt_wrapper_cache;
	}
	ipa_ctx->rx_pkt_wrapper_cache =
	   kmem_cache_create("IPA_RX_PKT_WRAPPER",
			   sizeof(struct ipa_rx_pkt_wrapper), 0, 0, NULL);
	if (!ipa_ctx->rx_pkt_wrapper_cache) {
		IPAERR(":ipa rx pkt wrapper cache create failed\n");
		result = -ENOMEM;
		goto fail_rx_pkt_wrapper_cache;
	}

	/* Setup DMA pool */
	ipa_ctx->dma_pool = dma_pool_create("ipa_tx", ipa_ctx->pdev,
		IPA_NUM_DESC_PER_SW_TX * sizeof(struct sps_iovec),
		0, 0);
	if (!ipa_ctx->dma_pool) {
		IPAERR("cannot alloc DMA pool.\n");
		result = -ENOMEM;
		goto fail_dma_pool;
	}

	ipa_ctx->glob_flt_tbl[IPA_IP_v4].in_sys = !ipa_ctx->ip4_flt_tbl_lcl;
	ipa_ctx->glob_flt_tbl[IPA_IP_v6].in_sys = !ipa_ctx->ip6_flt_tbl_lcl;

	/* init the various list heads */
	INIT_LIST_HEAD(&ipa_ctx->glob_flt_tbl[IPA_IP_v4].head_flt_rule_list);
	INIT_LIST_HEAD(&ipa_ctx->glob_flt_tbl[IPA_IP_v6].head_flt_rule_list);
	INIT_LIST_HEAD(&ipa_ctx->hdr_tbl.head_hdr_entry_list);
	for (i = 0; i < IPA_HDR_BIN_MAX; i++) {
		INIT_LIST_HEAD(&ipa_ctx->hdr_tbl.head_offset_list[i]);
		INIT_LIST_HEAD(&ipa_ctx->hdr_tbl.head_free_offset_list[i]);
	}
	INIT_LIST_HEAD(&ipa_ctx->hdr_proc_ctx_tbl.head_proc_ctx_entry_list);
	for (i = 0; i < IPA_HDR_PROC_CTX_BIN_MAX; i++) {
		INIT_LIST_HEAD(&ipa_ctx->hdr_proc_ctx_tbl.head_offset_list[i]);
		INIT_LIST_HEAD(&ipa_ctx->
				hdr_proc_ctx_tbl.head_free_offset_list[i]);
	}
	INIT_LIST_HEAD(&ipa_ctx->rt_tbl_set[IPA_IP_v4].head_rt_tbl_list);
	INIT_LIST_HEAD(&ipa_ctx->rt_tbl_set[IPA_IP_v6].head_rt_tbl_list);
	for (i = 0; i < ipa_ctx->ipa_num_pipes; i++) {
		flt_tbl = &ipa_ctx->flt_tbl[i][IPA_IP_v4];
		INIT_LIST_HEAD(&flt_tbl->head_flt_rule_list);
		flt_tbl->in_sys = !ipa_ctx->ip4_flt_tbl_lcl;

		flt_tbl = &ipa_ctx->flt_tbl[i][IPA_IP_v6];
		INIT_LIST_HEAD(&flt_tbl->head_flt_rule_list);
		flt_tbl->in_sys = !ipa_ctx->ip6_flt_tbl_lcl;
	}

	rset = &ipa_ctx->reap_rt_tbl_set[IPA_IP_v4];
	INIT_LIST_HEAD(&rset->head_rt_tbl_list);
	rset = &ipa_ctx->reap_rt_tbl_set[IPA_IP_v6];
	INIT_LIST_HEAD(&rset->head_rt_tbl_list);

	INIT_LIST_HEAD(&ipa_ctx->intf_list);
	INIT_LIST_HEAD(&ipa_ctx->msg_list);
	INIT_LIST_HEAD(&ipa_ctx->pull_msg_list);
	init_waitqueue_head(&ipa_ctx->msg_waitq);
	mutex_init(&ipa_ctx->msg_lock);

	mutex_init(&ipa_ctx->lock);
	mutex_init(&ipa_ctx->nat_mem.lock);

	idr_init(&ipa_ctx->ipa_idr);
	spin_lock_init(&ipa_ctx->idr_lock);

	/* wlan related member */
	memset(&ipa_ctx->wc_memb, 0, sizeof(ipa_ctx->wc_memb));
	spin_lock_init(&ipa_ctx->wc_memb.wlan_spinlock);
	spin_lock_init(&ipa_ctx->wc_memb.ipa_tx_mul_spinlock);
	INIT_LIST_HEAD(&ipa_ctx->wc_memb.wlan_comm_desc_list);
	/*
	 * setup an empty routing table in system memory, this will be used
	 * to delete a routing table cleanly and safely
	 */
	ipa_ctx->empty_rt_tbl_mem.size = IPA_ROUTING_RULE_BYTE_SIZE;

	ipa_ctx->empty_rt_tbl_mem.base =
		dma_alloc_coherent(ipa_ctx->pdev,
				ipa_ctx->empty_rt_tbl_mem.size,
				    &ipa_ctx->empty_rt_tbl_mem.phys_base,
				    GFP_KERNEL);
	if (!ipa_ctx->empty_rt_tbl_mem.base) {
		IPAERR("DMA buff alloc fail %d bytes for empty routing tbl\n",
				ipa_ctx->empty_rt_tbl_mem.size);
		result = -ENOMEM;
		goto fail_apps_pipes;
	}
	memset(ipa_ctx->empty_rt_tbl_mem.base, 0,
			ipa_ctx->empty_rt_tbl_mem.size);
	IPADBG("empty routing table was allocated in system memory");

	/* setup the A5-IPA pipes */
	if (ipa_setup_apps_pipes()) {
		IPAERR(":failed to setup IPA-Apps pipes.\n");
		result = -ENODEV;
		goto fail_empty_rt_tbl;
	}
	IPADBG("IPA System2Bam pipes were connected\n");

	if (ipa_init_flt_block()) {
		IPAERR("fail to setup dummy filter rules\n");
		result = -ENODEV;
		goto fail_empty_rt_tbl;
	}
	IPADBG("filter block was set with dummy filter rules");

	/* setup the IPA pipe mem pool */
	if (resource_p->ipa_pipe_mem_size)
		ipa_pipe_mem_init(resource_p->ipa_pipe_mem_start_ofst,
				resource_p->ipa_pipe_mem_size);

	ipa_ctx->class = class_create(THIS_MODULE, DRV_NAME);

	result = alloc_chrdev_region(&ipa_ctx->dev_num, 0, 1, DRV_NAME);
	if (result) {
		IPAERR("alloc_chrdev_region err.\n");
		result = -ENODEV;
		goto fail_alloc_chrdev_region;
	}

	ipa_ctx->dev = device_create(ipa_ctx->class, NULL, ipa_ctx->dev_num,
			ipa_ctx, DRV_NAME);
	if (IS_ERR(ipa_ctx->dev)) {
		IPAERR(":device_create err.\n");
		result = -ENODEV;
		goto fail_device_create;
	}

	cdev_init(&ipa_ctx->cdev, &ipa_drv_fops);
	ipa_ctx->cdev.owner = THIS_MODULE;
	ipa_ctx->cdev.ops = &ipa_drv_fops;  /* from LDD3 */

	result = cdev_add(&ipa_ctx->cdev, ipa_ctx->dev_num, 1);
	if (result) {
		IPAERR(":cdev_add err=%d\n", -result);
		result = -ENODEV;
		goto fail_cdev_add;
	}
	IPADBG("ipa cdev added successful. major:%d minor:%d\n",
			MAJOR(ipa_ctx->dev_num),
			MINOR(ipa_ctx->dev_num));

	if (create_nat_device()) {
		IPAERR("unable to create nat device\n");
		result = -ENODEV;
		goto fail_nat_dev_add;
	}



	/* Create a wakeup source. */
	wakeup_source_init(&ipa_ctx->w_lock, "IPA_WS");
	spin_lock_init(&ipa_ctx->wakelock_ref_cnt.spinlock);

	/* Initialize the SPS PM lock. */
	mutex_init(&ipa_ctx->sps_pm.sps_pm_lock);

	/* Initialize IPA RM (resource manager) */
	result = ipa_rm_initialize();
	if (result) {
		IPAERR("RM initialization failed (%d)\n", -result);
		result = -ENODEV;
		goto fail_ipa_rm_init;
	}
	IPADBG("IPA resource manager initialized");

	result = ipa_create_apps_resource();
	if (result) {
		IPAERR("Failed to create APPS_CONS resource\n");
		result = -ENODEV;
		goto fail_create_apps_resource;
	}

	/*register IPA IRQ handler*/
	result = ipa_interrupts_init(resource_p->ipa_irq, 0,
			master_dev);
	if (result) {
		IPAERR("ipa interrupts initialization failed\n");
		result = -ENODEV;
		goto fail_ipa_interrupts_init;
	}

	/*add handler for suspend interrupt*/
	result = ipa_add_interrupt_handler(IPA_TX_SUSPEND_IRQ,
			ipa_suspend_handler, false, NULL);
	if (result) {
		IPAERR("register handler for suspend interrupt failed\n");
		result = -ENODEV;
		goto fail_add_interrupt_handler;
	}

	if (ipa_ctx->use_ipa_teth_bridge) {
		/* Initialize the tethering bridge driver */
		result = teth_bridge_driver_init();
		if (result) {
			IPAERR(":teth_bridge init failed (%d)\n", -result);
			result = -ENODEV;
			goto fail_add_interrupt_handler;
		}
		IPADBG("teth_bridge initialized");
	}

	ipa_debugfs_init();

	result = ipa_uc_interface_init();
	if (result)
		IPAERR(":ipa Uc interface init failed (%d)\n", -result);
	else
		IPADBG(":ipa Uc interface init ok\n");

	result = ipa_wdi_init();
	if (result)
		IPAERR(":wdi init failed (%d)\n", -result);
	else
		IPADBG(":wdi init ok\n");

	result = ipa_ntn_init();
	if (result)
		IPAERR(":ntn init failed (%d)\n", -result);
	else
		IPADBG(":ntn init ok\n");

	ipa_ctx->q6_proxy_clk_vote_valid = true;

	ipa_register_panic_hdlr();

	pr_info("IPA driver initialization was successful.\n");

	return 0;

fail_add_interrupt_handler:
	free_irq(resource_p->ipa_irq, master_dev);
fail_ipa_interrupts_init:
	ipa_rm_delete_resource(IPA_RM_RESOURCE_APPS_CONS);
fail_create_apps_resource:
	ipa_rm_exit();
fail_ipa_rm_init:
fail_nat_dev_add:
	cdev_del(&ipa_ctx->cdev);
fail_cdev_add:
	device_destroy(ipa_ctx->class, ipa_ctx->dev_num);
fail_device_create:
	unregister_chrdev_region(ipa_ctx->dev_num, 1);
fail_alloc_chrdev_region:
	if (ipa_ctx->pipe_mem_pool)
		gen_pool_destroy(ipa_ctx->pipe_mem_pool);
fail_empty_rt_tbl:
	ipa_teardown_apps_pipes();
	dma_free_coherent(ipa_ctx->pdev,
			  ipa_ctx->empty_rt_tbl_mem.size,
			  ipa_ctx->empty_rt_tbl_mem.base,
			  ipa_ctx->empty_rt_tbl_mem.phys_base);
fail_apps_pipes:
	idr_destroy(&ipa_ctx->ipa_idr);
fail_dma_pool:
	kmem_cache_destroy(ipa_ctx->rx_pkt_wrapper_cache);
fail_rx_pkt_wrapper_cache:
	kmem_cache_destroy(ipa_ctx->tx_pkt_wrapper_cache);
fail_tx_pkt_wrapper_cache:
	kmem_cache_destroy(ipa_ctx->rt_tbl_cache);
fail_rt_tbl_cache:
	kmem_cache_destroy(ipa_ctx->hdr_proc_ctx_offset_cache);
fail_hdr_proc_ctx_offset_cache:
	kmem_cache_destroy(ipa_ctx->hdr_proc_ctx_cache);
fail_hdr_proc_ctx_cache:
	kmem_cache_destroy(ipa_ctx->hdr_offset_cache);
fail_hdr_offset_cache:
	kmem_cache_destroy(ipa_ctx->hdr_cache);
fail_hdr_cache:
	kmem_cache_destroy(ipa_ctx->rt_rule_cache);
fail_rt_rule_cache:
	kmem_cache_destroy(ipa_ctx->flt_rule_cache);
fail_flt_rule_cache:
	sps_deregister_bam_device(ipa_ctx->bam_handle);
fail_register_bam_device:
	destroy_workqueue(ipa_ctx->sps_power_mgmt_wq);
fail_create_sps_wq:
	destroy_workqueue(ipa_ctx->power_mgmt_wq);
fail_init_hw:
	iounmap(ipa_ctx->mmio);
fail_remap:
	ipa_disable_clks();
fail_clk:
	ipa2_active_clients_log_destroy();
fail_init_active_client:
	msm_bus_scale_unregister_client(ipa_ctx->ipa_bus_hdl);
fail_bus_reg:
	if (bus_scale_table) {
		msm_bus_cl_clear_pdata(bus_scale_table);
		bus_scale_table = NULL;
	}
fail_bind:
	kfree(ipa_ctx->ctrl);
fail_mem_ctrl:
	kfree(ipa_ctx);
	ipa_ctx = NULL;
fail_mem_ctx:
	return result;
}

static int get_ipa_dts_configuration(struct platform_device *pdev,
		struct ipa_plat_drv_res *ipa_drv_res)
{
	int result;
	struct resource *resource;

	/* initialize ipa_res */
	ipa_drv_res->ipa_pipe_mem_start_ofst = IPA_PIPE_MEM_START_OFST;
	ipa_drv_res->ipa_pipe_mem_size = IPA_PIPE_MEM_SIZE;
	ipa_drv_res->ipa_hw_type = 0;
	ipa_drv_res->ipa_hw_mode = 0;
	ipa_drv_res->ipa_bam_remote_mode = false;
	ipa_drv_res->modem_cfg_emb_pipe_flt = false;
	ipa_drv_res->wan_rx_ring_size = IPA_GENERIC_RX_POOL_SZ;
	ipa_drv_res->lan_rx_ring_size = IPA_GENERIC_RX_POOL_SZ;

	smmu_info.disable_htw = of_property_read_bool(pdev->dev.of_node,
			"qcom,smmu-disable-htw");

	/* Get IPA HW Version */
	result = of_property_read_u32(pdev->dev.of_node, "qcom,ipa-hw-ver",
					&ipa_drv_res->ipa_hw_type);
	if ((result) || (ipa_drv_res->ipa_hw_type == 0)) {
		IPAERR(":get resource failed for ipa-hw-ver!\n");
		return -ENODEV;
	}
	IPADBG(": ipa_hw_type = %d", ipa_drv_res->ipa_hw_type);

	/* Get IPA HW mode */
	result = of_property_read_u32(pdev->dev.of_node, "qcom,ipa-hw-mode",
			&ipa_drv_res->ipa_hw_mode);
	if (result)
		IPADBG("using default (IPA_MODE_NORMAL) for ipa-hw-mode\n");
	else
		IPADBG(": found ipa_drv_res->ipa_hw_mode = %d",
				ipa_drv_res->ipa_hw_mode);

	/* Get IPA WAN / LAN RX  pool sizes */
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
	IPADBG(": using TBDr = %s",
		ipa_drv_res->use_ipa_teth_bridge
		? "True" : "False");

	ipa_drv_res->ipa_bam_remote_mode =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,ipa-bam-remote-mode");
	IPADBG(": ipa bam remote mode = %s\n",
			ipa_drv_res->ipa_bam_remote_mode
			? "True" : "False");

	ipa_drv_res->modem_cfg_emb_pipe_flt =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,modem-cfg-emb-pipe-flt");
	IPADBG(": modem configure embedded pipe filtering = %s\n",
			ipa_drv_res->modem_cfg_emb_pipe_flt
			? "True" : "False");

	ipa_drv_res->skip_uc_pipe_reset =
		of_property_read_bool(pdev->dev.of_node,
		"qcom,skip-uc-pipe-reset");
	IPADBG(": skip uC pipe reset = %s\n",
		ipa_drv_res->skip_uc_pipe_reset
		? "True" : "False");

	ipa_drv_res->use_dma_zone =
		of_property_read_bool(pdev->dev.of_node,
		"qcom,use-dma-zone");
	IPADBG(": use dma zone = %s\n",
		ipa_drv_res->use_dma_zone
		? "True" : "False");

	ipa_drv_res->tethered_flow_control =
		of_property_read_bool(pdev->dev.of_node,
		"qcom,tethered-flow-control");
	IPADBG(": Use apps based flow control = %s\n",
		ipa_drv_res->tethered_flow_control
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

	/* Get IPA BAM address */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"bam-base");
	if (!resource) {
		IPAERR(":get resource failed for bam-base!\n");
		return -ENODEV;
	}
	ipa_drv_res->bam_mem_base = resource->start;
	ipa_drv_res->bam_mem_size = resource_size(resource);
	IPADBG(": bam-base = 0x%x, size = 0x%x\n",
			ipa_drv_res->bam_mem_base,
			ipa_drv_res->bam_mem_size);

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
		IPAERR(":get resource failed for ipa-irq!\n");
		return -ENODEV;
	}
	ipa_drv_res->ipa_irq = resource->start;
	IPADBG(":ipa-irq = %d\n", ipa_drv_res->ipa_irq);

	/* Get IPA BAM IRQ number */
	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"bam-irq");
	if (!resource) {
		IPAERR(":get resource failed for bam-irq!\n");
		return -ENODEV;
	}
	ipa_drv_res->bam_irq = resource->start;
	IPADBG(":ibam-irq = %d\n", ipa_drv_res->bam_irq);

	result = of_property_read_u32(pdev->dev.of_node, "qcom,ee",
			&ipa_drv_res->ee);
	if (result)
		ipa_drv_res->ee = 0;

	/* Get IPA RX Polling Timeout Seconds */
	result = of_property_read_u32(pdev->dev.of_node,
				"qcom,rx-polling-sleep-ms",
				&ipa_drv_res->ipa_rx_polling_sleep_msec);

	if (result) {
		ipa_drv_res->ipa_rx_polling_sleep_msec = ONE_MSEC;
		IPADBG("using default polling timeout of 1MSec\n");
	} else {
		IPADBG(": found ipa_drv_res->ipa_rx_polling_sleep_sec = %d",
			ipa_drv_res->ipa_rx_polling_sleep_msec);
	}

	/* Get IPA Polling Iteration */
	result = of_property_read_u32(pdev->dev.of_node,
				"qcom,ipa-polling-iteration",
				&ipa_drv_res->ipa_polling_iteration);
	if (result) {
		ipa_drv_res->ipa_polling_iteration = MAX_POLLING_ITERATION;
		IPADBG("using default polling iteration\n");
	} else {
		IPADBG(": found ipa_drv_res->ipa_polling_iteration = %d",
			ipa_drv_res->ipa_polling_iteration);
	}

	return 0;
}

static int ipa_smmu_wlan_cb_probe(struct device *dev)
{
	struct ipa_smmu_cb_ctx *cb = ipa2_get_wlan_smmu_ctx();
	int disable_htw = 1;
	int atomic_ctx = 1;
	int fast = 1;
	int bypass = 1;
	int ret;

	IPADBG("sub pdev=%p\n", dev);

	cb->dev = dev;
	cb->iommu = iommu_domain_alloc(msm_iommu_get_bus(dev));
	if (!cb->iommu) {
		IPAERR("could not alloc iommu domain\n");
		/* assume this failure is because iommu driver is not ready */
		return -EPROBE_DEFER;
	}
	cb->valid = true;

	if (smmu_info.disable_htw) {
		ret = iommu_domain_set_attr(cb->iommu,
			DOMAIN_ATTR_COHERENT_HTW_DISABLE,
			&disable_htw);
		if (ret) {
			IPAERR("couldn't disable coherent HTW\n");
			cb->valid = false;
			return -EIO;
		}
	}

	if (smmu_info.s1_bypass) {
		if (iommu_domain_set_attr(cb->iommu,
			DOMAIN_ATTR_S1_BYPASS,
			&bypass)) {
			IPAERR("couldn't set bypass\n");
			cb->valid = false;
			return -EIO;
		}
		IPADBG("SMMU S1 BYPASS\n");
	} else {
		if (iommu_domain_set_attr(cb->iommu,
			DOMAIN_ATTR_ATOMIC,
			&atomic_ctx)) {
			IPAERR("couldn't set domain as atomic\n");
			cb->valid = false;
			return -EIO;
		}
		IPADBG("SMMU atomic set\n");
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

	ret = iommu_attach_device(cb->iommu, dev);
	if (ret) {
		IPAERR("could not attach device ret=%d\n", ret);
		cb->valid = false;
		return ret;
	}

	if (!smmu_info.s1_bypass) {
		IPAERR("map IPA region to WLAN_CB IOMMU\n");
		ret = ipa_iommu_map(cb->iommu,
			rounddown(smmu_info.ipa_base, PAGE_SIZE),
			rounddown(smmu_info.ipa_base, PAGE_SIZE),
			roundup(smmu_info.ipa_size, PAGE_SIZE),
			IOMMU_READ | IOMMU_WRITE | IOMMU_DEVICE);
		if (ret) {
			IPAERR("map IPA to WLAN_CB IOMMU failed ret=%d\n",
				ret);
			arm_iommu_detach_device(cb->dev);
			cb->valid = false;
			return ret;
		}
	}

	return 0;
}

static int ipa_smmu_uc_cb_probe(struct device *dev)
{
	struct ipa_smmu_cb_ctx *cb = ipa2_get_uc_smmu_ctx();
	int disable_htw = 1;
	int atomic_ctx = 1;
	int ret;
	int fast = 1;
	int bypass = 1;
	u32 iova_ap_mapping[2];

	IPADBG("UC CB PROBE sub pdev=%p\n", dev);

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

	if (dma_set_mask(dev, DMA_BIT_MASK(32)) ||
		    dma_set_coherent_mask(dev, DMA_BIT_MASK(32))) {
		IPAERR("DMA set mask failed\n");
		return -EOPNOTSUPP;
	}

	IPADBG("UC CB PROBE=%p create IOMMU mapping\n", dev);

	cb->dev = dev;
	cb->mapping = arm_iommu_create_mapping(msm_iommu_get_bus(dev),
				cb->va_start, cb->va_size);
	if (IS_ERR_OR_NULL(cb->mapping)) {
		IPADBG("Fail to create mapping\n");
		/* assume this failure is because iommu driver is not ready */
		return -EPROBE_DEFER;
	}
	IPADBG("SMMU mapping created\n");
	cb->valid = true;

	IPADBG("UC CB PROBE sub pdev=%p disable htw\n", dev);
	if (smmu_info.disable_htw) {
		if (iommu_domain_set_attr(cb->mapping->domain,
				DOMAIN_ATTR_COHERENT_HTW_DISABLE,
				 &disable_htw)) {
			IPAERR("couldn't disable coherent HTW\n");
			arm_iommu_release_mapping(cb->mapping);
			cb->valid = false;
			return -EIO;
		}
	}

	IPADBG("UC CB PROBE sub pdev=%p set attribute\n", dev);
	if (smmu_info.s1_bypass) {
		if (iommu_domain_set_attr(cb->mapping->domain,
			DOMAIN_ATTR_S1_BYPASS,
			&bypass)) {
			IPAERR("couldn't set bypass\n");
			arm_iommu_release_mapping(cb->mapping);
			cb->valid = false;
			return -EIO;
		}
		IPADBG("SMMU S1 BYPASS\n");
	} else {
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

	IPADBG("UC CB PROBE sub pdev=%p attaching IOMMU device\n", dev);
	ret = arm_iommu_attach_device(cb->dev, cb->mapping);
	if (ret) {
		IPAERR("could not attach device ret=%d\n", ret);
		arm_iommu_release_mapping(cb->mapping);
		cb->valid = false;
		return ret;
	}

	cb->next_addr = cb->va_end;
	ipa_ctx->uc_pdev = dev;

	IPADBG("UC CB PROBE pdev=%p attached\n", dev);
	return 0;
}

static int ipa_smmu_ap_cb_probe(struct device *dev)
{
	struct ipa_smmu_cb_ctx *cb = ipa2_get_smmu_ctx();
	int result;
	int disable_htw = 1;
	int atomic_ctx = 1;
	int fast = 1;
	int bypass = 1;
	u32 iova_ap_mapping[2];

	IPADBG("AP CB probe: sub pdev=%p\n", dev);

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

	if (dma_set_mask(dev, DMA_BIT_MASK(32)) ||
		    dma_set_coherent_mask(dev, DMA_BIT_MASK(32))) {
		IPAERR("DMA set mask failed\n");
		return -EOPNOTSUPP;
	}

	cb->dev = dev;
	cb->mapping = arm_iommu_create_mapping(msm_iommu_get_bus(dev),
					       cb->va_start,
					       cb->va_size);
	if (IS_ERR_OR_NULL(cb->mapping)) {
		IPADBG("Fail to create mapping\n");
		/* assume this failure is because iommu driver is not ready */
		return -EPROBE_DEFER;
	}
	IPADBG("SMMU mapping created\n");
	cb->valid = true;

	if (smmu_info.disable_htw) {
		if (iommu_domain_set_attr(cb->mapping->domain,
				DOMAIN_ATTR_COHERENT_HTW_DISABLE,
				 &disable_htw)) {
			IPAERR("couldn't disable coherent HTW\n");
			arm_iommu_release_mapping(cb->mapping);
			cb->valid = false;
			return -EIO;
		}
		IPADBG("SMMU disable HTW\n");
	}

	if (smmu_info.s1_bypass) {
		if (iommu_domain_set_attr(cb->mapping->domain,
			DOMAIN_ATTR_S1_BYPASS,
			&bypass)) {
			IPAERR("couldn't set bypass\n");
			arm_iommu_release_mapping(cb->mapping);
			cb->valid = false;
			return -EIO;
		}
		IPADBG("SMMU S1 BYPASS\n");
	} else {
		if (iommu_domain_set_attr(cb->mapping->domain,
			DOMAIN_ATTR_ATOMIC,
			&atomic_ctx)) {
			IPAERR("couldn't set domain as atomic\n");
			arm_iommu_release_mapping(cb->mapping);
			cb->valid = false;
			return -EIO;
		}
		IPADBG("SMMU atomic set\n");

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

	result = arm_iommu_attach_device(cb->dev, cb->mapping);
	if (result) {
		IPAERR("couldn't attach to IOMMU ret=%d\n", result);
		cb->valid = false;
		return result;
	}

	if (!smmu_info.s1_bypass) {
		IPAERR("map IPA region to AP_CB IOMMU\n");
		result = ipa_iommu_map(cb->mapping->domain,
				rounddown(smmu_info.ipa_base, PAGE_SIZE),
				rounddown(smmu_info.ipa_base, PAGE_SIZE),
				roundup(smmu_info.ipa_size, PAGE_SIZE),
				IOMMU_READ | IOMMU_WRITE | IOMMU_DEVICE);
		if (result) {
			IPAERR("map IPA region to AP_CB IOMMU failed ret=%d\n",
				result);
			arm_iommu_release_mapping(cb->mapping);
			cb->valid = false;
			return result;
		}
	}

	smmu_info.present = true;

	if (!bus_scale_table)
		bus_scale_table = msm_bus_cl_get_pdata(ipa_pdev);

	/* Proceed to real initialization */
	result = ipa_init(&ipa_res, dev);
	if (result) {
		IPAERR("ipa_init failed\n");
		arm_iommu_detach_device(cb->dev);
		arm_iommu_release_mapping(cb->mapping);
		cb->valid = false;
		return result;
	}

	return result;
}

int ipa_plat_drv_probe(struct platform_device *pdev_p,
	struct ipa_api_controller *api_ctrl, struct of_device_id *pdrv_match)
{
	int result;
	struct device *dev = &pdev_p->dev;

	IPADBG("IPA driver probing started\n");

	if (of_device_is_compatible(dev->of_node, "qcom,ipa-smmu-ap-cb"))
		return ipa_smmu_ap_cb_probe(dev);

	if (of_device_is_compatible(dev->of_node, "qcom,ipa-smmu-wlan-cb"))
		return ipa_smmu_wlan_cb_probe(dev);

	if (of_device_is_compatible(dev->of_node, "qcom,ipa-smmu-uc-cb"))
		return ipa_smmu_uc_cb_probe(dev);

	master_dev = dev;
	if (!ipa_pdev)
		ipa_pdev = pdev_p;

	result = get_ipa_dts_configuration(pdev_p, &ipa_res);
	if (result) {
		IPAERR("IPA dts parsing failed\n");
		return result;
	}

	result = ipa2_bind_api_controller(ipa_res.ipa_hw_type, api_ctrl);
	if (result) {
		IPAERR("IPA API binding failed\n");
		return result;
	}

	if (of_property_read_bool(pdev_p->dev.of_node, "qcom,arm-smmu")) {
		if (of_property_read_bool(pdev_p->dev.of_node,
		    "qcom,smmu-s1-bypass"))
			smmu_info.s1_bypass = true;
		if (of_property_read_bool(pdev_p->dev.of_node,
		    "qcom,smmu-fast-map"))
			smmu_info.fast_map = true;
		smmu_info.arm_smmu = true;
		pr_info("IPA smmu_info.s1_bypass=%d smmu_info.fast_map=%d\n",
			smmu_info.s1_bypass, smmu_info.fast_map);
		result = of_platform_populate(pdev_p->dev.of_node,
				pdrv_match, NULL, &pdev_p->dev);
	} else if (of_property_read_bool(pdev_p->dev.of_node,
				"qcom,msm-smmu")) {
		IPAERR("Legacy IOMMU not supported\n");
		result = -EOPNOTSUPP;
	} else {
		if (dma_set_mask(&pdev_p->dev, DMA_BIT_MASK(32)) ||
			    dma_set_coherent_mask(&pdev_p->dev,
			    DMA_BIT_MASK(32))) {
			IPAERR("DMA set mask failed\n");
			return -EOPNOTSUPP;
		}

		if (!bus_scale_table)
			bus_scale_table = msm_bus_cl_get_pdata(pdev_p);

		/* Proceed to real initialization */
		result = ipa_init(&ipa_res, dev);
		if (result) {
			IPAERR("ipa_init failed\n");
			return result;
		}
	}

	return result;
}

/**
 * ipa2_ap_suspend() - suspend callback for runtime_pm
 * @dev: pointer to device
 *
 * This callback will be invoked by the runtime_pm framework when an AP suspend
 * operation is invoked, usually by pressing a suspend button.
 *
 * Returns -EAGAIN to runtime_pm framework in case IPA is in use by AP.
 * This will postpone the suspend operation until IPA is no longer used by AP.
*/
int ipa2_ap_suspend(struct device *dev)
{
	int i;

	IPADBG("Enter...\n");

	/* In case there is a tx/rx handler in polling mode fail to suspend */
	for (i = 0; i < ipa_ctx->ipa_num_pipes; i++) {
		if (ipa_ctx->ep[i].sys &&
			atomic_read(&ipa_ctx->ep[i].sys->curr_polling_state)) {
			IPAERR("EP %d is in polling state, do not suspend\n",
				i);
			return -EAGAIN;
		}
	}

	/* release SPS IPA resource without waiting for inactivity timer */
	atomic_set(&ipa_ctx->sps_pm.eot_activity, 0);
	ipa_sps_release_resource(NULL);
	IPADBG("Exit\n");

	return 0;
}

/**
* ipa2_ap_resume() - resume callback for runtime_pm
* @dev: pointer to device
*
* This callback will be invoked by the runtime_pm framework when an AP resume
* operation is invoked.
*
* Always returns 0 since resume should always succeed.
*/
int ipa2_ap_resume(struct device *dev)
{
	return 0;
}

struct ipa_context *ipa_get_ctx(void)
{
	return ipa_ctx;
}

int ipa_iommu_map(struct iommu_domain *domain,
	unsigned long iova, phys_addr_t paddr, size_t size, int prot)
{
	struct ipa_smmu_cb_ctx *ap_cb = ipa2_get_smmu_ctx();
	struct ipa_smmu_cb_ctx *uc_cb = ipa2_get_uc_smmu_ctx();

	IPADBG("domain =0x%p iova 0x%lx\n", domain, iova);
	IPADBG("paddr =0x%pa size 0x%x\n", &paddr, (u32)size);

	/* make sure no overlapping */
	if (domain == ipa2_get_smmu_domain()) {
		if (iova >= ap_cb->va_start && iova < ap_cb->va_end) {
			IPAERR("iommu AP overlap addr 0x%lx\n", iova);
			BUG();
			return -EFAULT;
		}
	} else if (domain == ipa2_get_wlan_smmu_domain()) {
		/* wlan is one time map */
	} else if (domain == ipa2_get_uc_smmu_domain()) {
		if (iova >= uc_cb->va_start && iova < uc_cb->va_end) {
			IPAERR("iommu uC overlap addr 0x%lx\n", iova);
			BUG();
			return -EFAULT;
		}
	} else {
		IPAERR("Unexpected domain 0x%p\n", domain);
		BUG();
		return -EFAULT;
	}

	return iommu_map(domain, iova, paddr, size, prot);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA HW device driver");

