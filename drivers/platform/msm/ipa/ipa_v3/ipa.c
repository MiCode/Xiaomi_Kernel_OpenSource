/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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
#include <linux/of_gpio.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/netdevice.h>
#include <linux/delay.h>
#include <linux/msm_gsi.h>
#include <linux/qcom_iommu.h>
#include <linux/time.h>
#include <linux/hashtable.h>
#include <linux/hash.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/smem.h>
#include <soc/qcom/scm.h>

#ifdef CONFIG_ARM64

/* Outer caches unsupported on ARM64 platforms */
#define outer_flush_range(x, y)
#define __cpuc_flush_dcache_area __flush_dcache_area

#endif

#define IPA_SUBSYSTEM_NAME "ipa_fws"
#include "ipa_i.h"
#include "../ipa_rm_i.h"
#include "ipahal/ipahal.h"
#include "ipahal/ipahal_fltrt.h"

#define CREATE_TRACE_POINTS
#include "ipa_trace.h"

#define IPA_GPIO_IN_QUERY_CLK_IDX 0
#define IPA_GPIO_OUT_CLK_RSP_CMPLT_IDX 0
#define IPA_GPIO_OUT_CLK_VOTE_IDX 1

#define IPA_SUMMING_THRESHOLD (0x10)
#define IPA_PIPE_MEM_START_OFST (0x0)
#define IPA_PIPE_MEM_SIZE (0x0)
#define IPA_MOBILE_AP_MODE(x) (x == IPA_MODE_MOBILE_AP_ETH || \
			       x == IPA_MODE_MOBILE_AP_WAN || \
			       x == IPA_MODE_MOBILE_AP_WLAN)
#define IPA_CNOC_CLK_RATE (75 * 1000 * 1000UL)
#define IPA_A5_MUX_HEADER_LENGTH (8)

#define IPA_AGGR_MAX_STR_LENGTH (10)

#define CLEANUP_TAG_PROCESS_TIMEOUT 500

#define IPA_AGGR_STR_IN_BYTES(str) \
	(strnlen((str), IPA_AGGR_MAX_STR_LENGTH - 1) + 1)

#define IPA_TRANSPORT_PROD_TIMEOUT_MSEC 100

#define IPA3_ACTIVE_CLIENTS_TABLE_BUF_SIZE 2048

#define IPA3_ACTIVE_CLIENT_LOG_TYPE_EP 0
#define IPA3_ACTIVE_CLIENT_LOG_TYPE_SIMPLE 1
#define IPA3_ACTIVE_CLIENT_LOG_TYPE_RESOURCE 2
#define IPA3_ACTIVE_CLIENT_LOG_TYPE_SPECIAL 3

#define IPA_SMEM_SIZE (8 * 1024)

/* round addresses for closes page per SMMU requirements */
#define IPA_SMMU_ROUND_TO_PAGE(iova, pa, size, iova_p, pa_p, size_p) \
	do { \
		(iova_p) = rounddown((iova), PAGE_SIZE); \
		(pa_p) = rounddown((pa), PAGE_SIZE); \
		(size_p) = roundup((size) + (pa) - (pa_p), PAGE_SIZE); \
	} while (0)


/* The relative location in /lib/firmware where the FWs will reside */
#define IPA_FWS_PATH "ipa/ipa_fws.elf"

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
#endif

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

static void ipa3_sps_release_resource(struct work_struct *work);
static DECLARE_DELAYED_WORK(ipa3_sps_release_resource_work,
	ipa3_sps_release_resource);
static void ipa_gsi_notify_cb(struct gsi_per_notify *notify);

static void ipa_gsi_request_resource(struct work_struct *work);
static DECLARE_WORK(ipa_gsi_request_resource_work,
	ipa_gsi_request_resource);

static void ipa_gsi_release_resource(struct work_struct *work);
static DECLARE_DELAYED_WORK(ipa_gsi_release_resource_work,
	ipa_gsi_release_resource);

static struct ipa3_plat_drv_res ipa3_res = {0, };
struct msm_bus_scale_pdata *ipa3_bus_scale_table;

static struct clk *ipa3_clk;

struct ipa3_context *ipa3_ctx;
static struct device *master_dev;
struct platform_device *ipa3_pdev;
static struct {
	bool present;
	bool arm_smmu;
	bool fast_map;
	bool s1_bypass;
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

	return cnt;
}

int ipa3_active_clients_log_print_table(char *buf, int size)
{
	int i;
	struct ipa3_active_client_htable_entry *iterator;
	int cnt = 0;

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
			ipa3_ctx->ipa3_active_clients.cnt);

	return cnt;
}

static int ipa3_active_clients_panic_notifier(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	ipa3_active_clients_lock();
	ipa3_active_clients_log_print_table(active_clients_table_buf,
			IPA3_ACTIVE_CLIENTS_TABLE_BUF_SIZE);
	IPAERR("%s", active_clients_table_buf);
	ipa3_active_clients_unlock();

	return NOTIFY_DONE;
}

static struct notifier_block ipa3_active_clients_panic_blk = {
	.notifier_call  = ipa3_active_clients_panic_notifier,
};

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

static int ipa3_active_clients_log_init(void)
{
	int i;

	ipa3_ctx->ipa3_active_clients_logging.log_buffer[0] = kzalloc(
			IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES *
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
	ipa3_active_clients_lock();
	ipa3_ctx->ipa3_active_clients_logging.log_head = 0;
	ipa3_ctx->ipa3_active_clients_logging.log_tail =
			IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES - 1;
	ipa3_active_clients_unlock();
}

static void ipa3_active_clients_log_destroy(void)
{
	ipa3_ctx->ipa3_active_clients_logging.log_rdy = 0;
	kfree(ipa3_ctx->ipa3_active_clients_logging.log_buffer[0]);
	ipa3_ctx->ipa3_active_clients_logging.log_head = 0;
	ipa3_ctx->ipa3_active_clients_logging.log_tail =
			IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES - 1;
}

enum ipa_smmu_cb_type {
	IPA_SMMU_CB_AP,
	IPA_SMMU_CB_WLAN,
	IPA_SMMU_CB_UC,
	IPA_SMMU_CB_MAX

};

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


struct device *ipa3_get_dma_dev(void)
{
	return ipa3_ctx->pdev;
}

/**
 * ipa3_get_smmu_ctx()- Return the wlan smmu context
 *
 * Return value: pointer to smmu context address
 */
struct ipa_smmu_cb_ctx *ipa3_get_smmu_ctx(void)
{
	return &smmu_cb[IPA_SMMU_CB_AP];
}

/**
 * ipa3_get_wlan_smmu_ctx()- Return the wlan smmu context
 *
 * Return value: pointer to smmu context address
 */
struct ipa_smmu_cb_ctx *ipa3_get_wlan_smmu_ctx(void)
{
	return &smmu_cb[IPA_SMMU_CB_WLAN];
}

/**
 * ipa3_get_uc_smmu_ctx()- Return the uc smmu context
 *
 * Return value: pointer to smmu context address
 */
struct ipa_smmu_cb_ctx *ipa3_get_uc_smmu_ctx(void)
{
	return &smmu_cb[IPA_SMMU_CB_UC];
}

static int ipa3_open(struct inode *inode, struct file *filp)
{
	struct ipa3_context *ctx = NULL;

	IPADBG_LOW("ENTER\n");
	ctx = container_of(inode->i_cdev, struct ipa3_context, cdev);
	filp->private_data = ctx;

	return 0;
}

/**
* ipa3_flow_control() - Enable/Disable flow control on a particular client.
* Return codes:
* None
*/
void ipa3_flow_control(enum ipa_client_type ipa_client,
		bool enable, uint32_t qmap_id)
{
	struct ipa_ep_cfg_ctrl ep_ctrl = {0};
	int ep_idx;
	struct ipa3_ep_context *ep;

	/* Check if tethered flow control is needed or not.*/
	if (!ipa3_ctx->tethered_flow_control) {
		IPADBG("Apps flow control is not needed\n");
		return;
	}

	/* Check if ep is valid. */
	ep_idx = ipa3_get_ep_mapping(ipa_client);
	if (ep_idx == -1) {
		IPADBG("Invalid IPA client\n");
		return;
	}

	ep = &ipa3_ctx->ep[ep_idx];
	if (!ep->valid || (ep->client != IPA_CLIENT_USB_PROD)) {
		IPADBG("EP not valid/Not applicable for client.\n");
		return;
	}

	spin_lock(&ipa3_ctx->disconnect_lock);
	/* Check if the QMAP_ID matches. */
	if (ep->cfg.meta.qmap_id != qmap_id) {
		IPADBG("Flow control ind not for same flow: %u %u\n",
			ep->cfg.meta.qmap_id, qmap_id);
		spin_unlock(&ipa3_ctx->disconnect_lock);
		return;
	}
	if (!ep->disconnect_in_progress) {
		if (enable) {
			IPADBG("Enabling Flow\n");
			ep_ctrl.ipa_ep_delay = false;
			IPA_STATS_INC_CNT(ipa3_ctx->stats.flow_enable);
		} else {
			IPADBG("Disabling Flow\n");
			ep_ctrl.ipa_ep_delay = true;
			IPA_STATS_INC_CNT(ipa3_ctx->stats.flow_disable);
		}
		ep_ctrl.ipa_ep_suspend = false;
		ipa3_cfg_ep_ctrl(ep_idx, &ep_ctrl);
	} else {
		IPADBG("EP disconnect is in progress\n");
	}
	spin_unlock(&ipa3_ctx->disconnect_lock);
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
		IPAERR("Wrong type given. buff %p type %d\n", buff, type);
		return;
	}

	kfree(buff);
}

static int ipa3_send_wan_msg(unsigned long usr_param, uint8_t msg_type)
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
	retval = ipa3_send_msg(&msg_meta, wan_msg, ipa3_wan_msg_free_cb);
	if (retval) {
		IPAERR("ipa3_send_msg failed: %d\n", retval);
		kfree(wan_msg);
		return retval;
	}

	return 0;
}


static long ipa3_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
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

	if (!ipa3_is_ready()) {
		IPAERR("IPA not ready, waiting for init completion\n");
		wait_for_completion(&ipa3_ctx->init_completion_obj);
	}

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

		if (ipa3_allocate_nat_device(&nat_mem)) {
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
		if (ipa3_nat_init_cmd(&nat_init)) {
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
			IPAERR("current %d pre %d\n",
				((struct ipa_ioc_nat_dma_cmd *)param)->entries,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_nat_dma_cmd((struct ipa_ioc_nat_dma_cmd *)param)) {
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
		if (ipa3_nat_del_cmd(&nat_del)) {
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
			IPAERR("current %d pre %d\n",
				((struct ipa_ioc_add_hdr *)param)->num_hdrs,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_add_hdr((struct ipa_ioc_add_hdr *)param)) {
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
			IPAERR("current %d pre %d\n",
				((struct ipa_ioc_del_hdr *)param)->num_hdls,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_del_hdr((struct ipa_ioc_del_hdr *)param)) {
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
			IPAERR("current %d pre %d\n",
				((struct ipa_ioc_add_rt_rule *)param)->
				num_rules,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_add_rt_rule((struct ipa_ioc_add_rt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	case IPA_IOC_ADD_RT_RULE_AFTER:
		if (copy_from_user(header, (u8 *)arg,
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
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_add_rt_rule_after *)param)->
			num_rules != pre_entry)) {
			IPAERR("current %d pre %d\n",
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
			IPAERR("current %d pre %d\n",
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
			IPAERR("current %d pre %d\n",
				((struct ipa_ioc_del_rt_rule *)param)->num_hdls,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_del_rt_rule((struct ipa_ioc_del_rt_rule *)param)) {
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
			IPAERR("current %d pre %d\n",
				((struct ipa_ioc_add_flt_rule *)param)->
				num_rules,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_add_flt_rule((struct ipa_ioc_add_flt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_ADD_FLT_RULE_AFTER:
		if (copy_from_user(header, (u8 *)arg,
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
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		/* add check in case user-space module compromised */
		if (unlikely(((struct ipa_ioc_add_flt_rule_after *)param)->
			num_rules != pre_entry)) {
			IPAERR("current %d pre %d\n",
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
			IPAERR("current %d pre %d\n",
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
			IPAERR("current %d pre %d\n",
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
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_COMMIT_HDR:
		retval = ipa3_commit_hdr();
		break;
	case IPA_IOC_RESET_HDR:
		retval = ipa3_reset_hdr();
		break;
	case IPA_IOC_COMMIT_RT:
		retval = ipa3_commit_rt(arg);
		break;
	case IPA_IOC_RESET_RT:
		retval = ipa3_reset_rt(arg);
		break;
	case IPA_IOC_COMMIT_FLT:
		retval = ipa3_commit_flt(arg);
		break;
	case IPA_IOC_RESET_FLT:
		retval = ipa3_reset_flt(arg);
		break;
	case IPA_IOC_GET_RT_TBL:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_get_rt_tbl))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_get_rt_tbl((struct ipa_ioc_get_rt_tbl *)header)) {
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
		retval = ipa3_put_rt_tbl(arg);
		break;
	case IPA_IOC_GET_HDR:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_get_hdr))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_get_hdr((struct ipa_ioc_get_hdr *)header)) {
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
		retval = ipa3_put_hdr(arg);
		break;
	case IPA_IOC_SET_FLT:
		retval = ipa3_cfg_filter(arg);
		break;
	case IPA_IOC_COPY_HDR:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_copy_hdr))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_copy_hdr((struct ipa_ioc_copy_hdr *)header)) {
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
		if (ipa3_query_intf((struct ipa_ioc_query_intf *)header)) {
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
			IPAERR("current %d pre %d\n",
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
			IPAERR("current %d pre %d\n",
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
			IPAERR("current %d pre %d\n",
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
			IPAERR("current %d pre %d\n",
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
			if (ipahal_flt_generate_equation(flt_eq.ip,
				&flt_eq.attrib, &flt_eq.eq_attrib)) {
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
			retval = ipa3_get_ep_mapping(arg);
			break;
		}
	case IPA_IOC_QUERY_RT_TBL_INDEX:
		if (copy_from_user(header, (u8 *)arg,
				sizeof(struct ipa_ioc_get_rt_tbl_indx))) {
			retval = -EFAULT;
			break;
		}
		if (ipa3_query_rt_index(
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
		if (ipa3_write_qmap_id((struct ipa_ioc_write_qmapid *)header)) {
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
		retval = ipa3_send_wan_msg(arg, WAN_UPSTREAM_ROUTE_ADD);
		if (retval) {
			IPAERR("ipa3_send_wan_msg failed: %d\n", retval);
			break;
		}
		break;
	case IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_DEL:
		retval = ipa3_send_wan_msg(arg, WAN_UPSTREAM_ROUTE_DEL);
		if (retval) {
			IPAERR("ipa3_send_wan_msg failed: %d\n", retval);
			break;
		}
		break;
	case IPA_IOC_NOTIFY_WAN_EMBMS_CONNECTED:
		retval = ipa3_send_wan_msg(arg, WAN_EMBMS_CONNECT);
		if (retval) {
			IPAERR("ipa3_send_wan_msg failed: %d\n", retval);
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
			IPAERR("current %d pre %d\n",
				((struct ipa_ioc_add_hdr_proc_ctx *)
				param)->num_proc_ctxs, pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_add_hdr_proc_ctx(
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
			IPAERR("current %d pre %d\n",
				((struct ipa_ioc_del_hdr_proc_ctx *)param)->
				num_hdls,
				pre_entry);
			retval = -EFAULT;
			break;
		}
		if (ipa3_del_hdr_proc_ctx(
			(struct ipa_ioc_del_hdr_proc_ctx *)param)) {
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
		memcpy(param, &ipa3_ctx->ipa_hw_type, pyld_sz);
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
	if (!hdr) {
		IPAERR("fail to alloc exception hdr\n");
		return -ENOMEM;
	}
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
	desc.opcode = ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_DMA_SHARED_MEM);
	desc.pyld = cmd_pyld->data;
	desc.len = cmd_pyld->len;
	desc.type = IPA_IMM_CMD_DESC;

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

	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		if (IPA_CLIENT_IS_Q6_CONS(client_idx)) {
			ep_idx = ipa3_get_ep_mapping(client_idx);
			if (ep_idx == -1)
				continue;

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

			ipahal_write_reg_n_fields(
				IPA_ENDP_INIT_CTRL_n,
				ep_idx, &ep_suspend);
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

	/* Up to filtering pipes we have filtering tables */
	desc = kcalloc(ipa3_ctx->ep_flt_num, sizeof(struct ipa3_desc),
		GFP_KERNEL);
	if (!desc) {
		IPAERR("failed to allocate memory\n");
		return -ENOMEM;
	}

	cmd_pyld = kcalloc(ipa3_ctx->ep_flt_num,
		sizeof(struct ipahal_imm_cmd_pyld *), GFP_KERNEL);
	if (!cmd_pyld) {
		IPAERR("failed to allocate memory\n");
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
		0, &mem);
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
			desc[num_cmds].opcode = ipahal_imm_cmd_get_opcode(
				IPA_IMM_CMD_DMA_SHARED_MEM);
			desc[num_cmds].pyld = cmd_pyld[num_cmds]->data;
			desc[num_cmds].len = cmd_pyld[num_cmds]->len;
			desc[num_cmds].type = IPA_IMM_CMD_DESC;
			num_cmds++;
		}

		flt_idx++;
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
		lcl_hdr_sz, lcl_hdr_sz, &mem);
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
	desc->opcode =
		ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_DMA_SHARED_MEM);
	desc->pyld = cmd_pyld->data;
	desc->len = cmd_pyld->len;
	desc->type = IPA_IMM_CMD_DESC;

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
	int retval;
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

	/* Flush rules cache */
	desc = kzalloc(sizeof(struct ipa3_desc), GFP_KERNEL);
	if (!desc) {
		IPAERR("failed to allocate memory\n");
		return -ENOMEM;
	}

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
	desc->opcode =
		ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_REGISTER_WRITE);
	desc->pyld = cmd_pyld->data;
	desc->len = cmd_pyld->len;
	desc->type = IPA_IMM_CMD_DESC;

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
	struct ipahal_reg_valmask valmask;

	desc = kcalloc(ipa3_ctx->ipa_num_pipes, sizeof(struct ipa3_desc),
			GFP_KERNEL);
	if (!desc) {
		IPAERR("failed to allocate memory\n");
		return -ENOMEM;
	}

	/* Set the exception path to AP */
	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		ep_idx = ipa3_get_ep_mapping(client_idx);
		if (ep_idx == -1)
			continue;

		if (ipa3_ctx->ep[ep_idx].valid &&
			ipa3_ctx->ep[ep_idx].skip_ep_cfg) {
			BUG_ON(num_descs >= ipa3_ctx->ipa_num_pipes);

			reg_write.skip_pipeline_clear = false;
			reg_write.pipeline_clear_options =
				IPAHAL_HPS_CLEAR;
			reg_write.offset =
				ipahal_get_reg_ofst(IPA_ENDP_STATUS_n);
			ipahal_get_status_ep_valmask(
				ipa3_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS),
				&valmask);
			reg_write.value = valmask.val;
			reg_write.value_mask = valmask.mask;
			cmd_pyld = ipahal_construct_imm_cmd(
				IPA_IMM_CMD_REGISTER_WRITE, &reg_write, false);
			if (!cmd_pyld) {
				IPAERR("fail construct register_write cmd\n");
				BUG();
			}

			desc[num_descs].opcode = ipahal_imm_cmd_get_opcode(
				IPA_IMM_CMD_REGISTER_WRITE);
			desc[num_descs].type = IPA_IMM_CMD_DESC;
			desc[num_descs].callback = ipa3_destroy_imm;
			desc[num_descs].user1 = cmd_pyld;
			desc[num_descs].pyld = cmd_pyld->data;
			desc[num_descs].len = cmd_pyld->len;
			num_descs++;
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

	ipa3_q6_pipe_delay(true);
	ipa3_q6_avoid_holb();
	if (ipa3_q6_clean_q6_tables()) {
		IPAERR("Failed to clean Q6 tables\n");
		BUG();
	}
	if (ipa3_q6_set_ex_path_to_apps()) {
		IPAERR("Failed to redirect exceptions to APPS\n");
		BUG();
	}
	/* Remove delay from Q6 PRODs to avoid pending descriptors
	  * on pipe reset procedure
	  */
	ipa3_q6_pipe_delay(false);

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

	IPADBG_LOW("ENTER\n");

	if (!ipa3_ctx->uc_ctx.uc_loaded) {
		IPAERR("uC is not loaded. Skipping\n");
		return;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	/* Handle the issue where SUSPEND was removed for some reason */
	ipa3_q6_avoid_holb();

	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++)
		if (IPA_CLIENT_IS_Q6_PROD(client_idx)) {
			if (ipa3_uc_is_gsi_channel_empty(client_idx)) {
				IPAERR("fail to validate Q6 ch emptiness %d\n",
					client_idx);
				BUG();
				return;
			}
		}

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	IPADBG_LOW("Exit with success\n");
}

static inline void ipa3_sram_set_canary(u32 *sram_mmio, int offset)
{
	/* Set 4 bytes of CANARY before the offset */
	sram_mmio[(offset - 4) / 4] = IPA_MEM_CANARY_VAL;
}

/**
 * _ipa_init_sram_v3_0() - Initialize IPA local SRAM.
 *
 * Return codes: 0 for success, negative value for failure
 */
int _ipa_init_sram_v3_0(void)
{
	u32 *ipa_sram_mmio;
	unsigned long phys_addr;

	phys_addr = ipa3_ctx->ipa_wrapper_base +
		ipa3_ctx->ctrl->ipa_reg_base_ofst +
		ipahal_get_reg_n_ofst(IPA_SRAM_DIRECT_ACCESS_n,
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
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(modem_ofst) - 4);
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(modem_ofst));
	ipa3_sram_set_canary(ipa_sram_mmio, IPA_MEM_PART(end_ofst));

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
	struct ipa3_desc desc = { 0 };
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
	desc.opcode = ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_HDR_INIT_LOCAL);
	desc.type = IPA_IMM_CMD_DESC;
	desc.pyld = cmd_pyld->data;
	desc.len = cmd_pyld->len;
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
	memset(&desc, 0, sizeof(desc));

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
		return -EFAULT;
	}
	desc.opcode = ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_DMA_SHARED_MEM);
	desc.pyld = cmd_pyld->data;
	desc.len = cmd_pyld->len;
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa3_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		ipahal_destroy_imm_cmd(cmd_pyld);
		dma_free_coherent(ipa3_ctx->pdev,
			mem.size,
			mem.base,
			mem.phys_base);
		return -EFAULT;
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
	struct ipa3_desc desc = { 0 };
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
		&mem);
	if (rc) {
		IPAERR("fail generate empty v4 rt img\n");
		return rc;
	}

	v4_cmd.hash_rules_addr = mem.phys_base;
	v4_cmd.hash_rules_size = mem.size;
	v4_cmd.hash_local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v4_rt_hash_ofst);
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

	desc.opcode =
		ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_IP_V4_ROUTING_INIT);
	desc.type = IPA_IMM_CMD_DESC;
	desc.pyld = cmd_pyld->data;
	desc.len = cmd_pyld->len;
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
	struct ipa3_desc desc = { 0 };
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
		&mem);
	if (rc) {
		IPAERR("fail generate empty v6 rt img\n");
		return rc;
	}

	v6_cmd.hash_rules_addr = mem.phys_base;
	v6_cmd.hash_rules_size = mem.size;
	v6_cmd.hash_local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v6_rt_hash_ofst);
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

	desc.opcode =
		ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_IP_V6_ROUTING_INIT);
	desc.type = IPA_IMM_CMD_DESC;
	desc.pyld = cmd_pyld->data;
	desc.len = cmd_pyld->len;
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
	struct ipa3_desc desc = { 0 };
	struct ipa_mem_buffer mem;
	struct ipahal_imm_cmd_ip_v4_filter_init v4_cmd;
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	int rc;

	rc = ipahal_flt_generate_empty_img(ipa3_ctx->ep_flt_num,
		IPA_MEM_PART(v4_flt_hash_size),
		IPA_MEM_PART(v4_flt_nhash_size), ipa3_ctx->ep_flt_bitmap,
		&mem);
	if (rc) {
		IPAERR("fail generate empty v4 flt img\n");
		return rc;
	}

	v4_cmd.hash_rules_addr = mem.phys_base;
	v4_cmd.hash_rules_size = mem.size;
	v4_cmd.hash_local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v4_flt_hash_ofst);
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

	desc.opcode = ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_IP_V4_FILTER_INIT);
	desc.type = IPA_IMM_CMD_DESC;
	desc.pyld = cmd_pyld->data;
	desc.len = cmd_pyld->len;
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
	struct ipa3_desc desc = { 0 };
	struct ipa_mem_buffer mem;
	struct ipahal_imm_cmd_ip_v6_filter_init v6_cmd;
	struct ipahal_imm_cmd_pyld *cmd_pyld;
	int rc;

	rc = ipahal_flt_generate_empty_img(ipa3_ctx->ep_flt_num,
		IPA_MEM_PART(v6_flt_hash_size),
		IPA_MEM_PART(v6_flt_nhash_size), ipa3_ctx->ep_flt_bitmap,
		&mem);
	if (rc) {
		IPAERR("fail generate empty v6 flt img\n");
		return rc;
	}

	v6_cmd.hash_rules_addr = mem.phys_base;
	v6_cmd.hash_rules_size = mem.size;
	v6_cmd.hash_local_addr = ipa3_ctx->smem_restricted_bytes +
		IPA_MEM_PART(v6_flt_hash_ofst);
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

	desc.opcode = ipahal_imm_cmd_get_opcode(IPA_IMM_CMD_IP_V6_FILTER_INIT);
	desc.type = IPA_IMM_CMD_DESC;
	desc.pyld = cmd_pyld->data;
	desc.len = cmd_pyld->len;
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
			goto fail_cmd;
		}
	}

	/* CMD OUT (AP->IPA) */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_APPS_CMD_PROD;
	sys_in.desc_fifo_sz = IPA_SYS_DESC_FIFO_SZ;
	sys_in.ipa_ep_cfg.mode.mode = IPA_DMA;
	sys_in.ipa_ep_cfg.mode.dst = IPA_CLIENT_APPS_LAN_CONS;
	if (ipa3_setup_sys_pipe(&sys_in, &ipa3_ctx->clnt_hdl_cmd)) {
		IPAERR(":setup sys pipe failed.\n");
		result = -EPERM;
		goto fail_cmd;
	}
	IPADBG("Apps to IPA cmd pipe is connected\n");

	ipa3_ctx->ctrl->ipa_init_sram();
	IPADBG("SRAM initialized\n");

	ipa3_ctx->ctrl->ipa_init_hdr();
	IPADBG("HDR initialized\n");

	ipa3_ctx->ctrl->ipa_init_rt4();
	IPADBG("V4 RT initialized\n");

	ipa3_ctx->ctrl->ipa_init_rt6();
	IPADBG("V6 RT initialized\n");

	ipa3_ctx->ctrl->ipa_init_flt4();
	IPADBG("V4 FLT initialized\n");

	ipa3_ctx->ctrl->ipa_init_flt6();
	IPADBG("V6 FLT initialized\n");

	if (ipa3_setup_flt_hash_tuple()) {
		IPAERR(":fail to configure flt hash tuple\n");
		result = -EPERM;
		goto fail_schedule_delayed_work;
	}
	IPADBG("flt hash tuple is configured\n");

	if (ipa3_setup_rt_hash_tuple()) {
		IPAERR(":fail to configure rt hash tuple\n");
		result = -EPERM;
		goto fail_schedule_delayed_work;
	}
	IPADBG("rt hash tuple is configured\n");

	if (ipa3_setup_exception_path()) {
		IPAERR(":fail to setup excp path\n");
		result = -EPERM;
		goto fail_schedule_delayed_work;
	}
	IPADBG("Exception path was successfully set");

	if (ipa3_setup_dflt_rt_tables()) {
		IPAERR(":fail to setup dflt routes\n");
		result = -EPERM;
		goto fail_schedule_delayed_work;
	}
	IPADBG("default routing was set\n");

	/* LAN IN (IPA->A5) */
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
	sys_in.ipa_ep_cfg.cfg.cs_offload_en = IPA_ENABLE_CS_OFFLOAD_DL;

	/**
	 * ipa_lan_rx_cb() intended to notify the source EP about packet
	 * being received on the LAN_CONS via calling the source EP call-back.
	 * There could be a race condition with calling this call-back. Other
	 * thread may nullify it - e.g. on EP disconnect.
	 * This lock intended to protect the access to the source EP call-back
	 */
	spin_lock_init(&ipa3_ctx->disconnect_lock);
	if (ipa3_setup_sys_pipe(&sys_in, &ipa3_ctx->clnt_hdl_data_in)) {
		IPAERR(":setup sys pipe failed.\n");
		result = -EPERM;
		goto fail_schedule_delayed_work;
	}

	/* LAN-WAN OUT (AP->IPA) */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_APPS_LAN_WAN_PROD;
	sys_in.desc_fifo_sz = IPA_SYS_TX_DATA_DESC_FIFO_SZ;
	sys_in.ipa_ep_cfg.mode.mode = IPA_BASIC;
	if (ipa3_setup_sys_pipe(&sys_in, &ipa3_ctx->clnt_hdl_data_out)) {
		IPAERR(":setup sys pipe failed.\n");
		result = -EPERM;
		goto fail_data_out;
	}

	return 0;

fail_data_out:
	ipa3_teardown_sys_pipe(ipa3_ctx->clnt_hdl_data_in);
fail_schedule_delayed_work:
	if (ipa3_ctx->dflt_v6_rt_rule_hdl)
		__ipa3_del_rt_rule(ipa3_ctx->dflt_v6_rt_rule_hdl);
	if (ipa3_ctx->dflt_v4_rt_rule_hdl)
		__ipa3_del_rt_rule(ipa3_ctx->dflt_v4_rt_rule_hdl);
	if (ipa3_ctx->excp_hdr_hdl)
		__ipa3_del_hdr(ipa3_ctx->excp_hdr_hdl);
	ipa3_teardown_sys_pipe(ipa3_ctx->clnt_hdl_cmd);
fail_cmd:
	return result;
}

static void ipa3_teardown_apps_pipes(void)
{
	ipa3_teardown_sys_pipe(ipa3_ctx->clnt_hdl_data_out);
	ipa3_teardown_sys_pipe(ipa3_ctx->clnt_hdl_data_in);
	__ipa3_del_rt_rule(ipa3_ctx->dflt_v6_rt_rule_hdl);
	__ipa3_del_rt_rule(ipa3_ctx->dflt_v4_rt_rule_hdl);
	__ipa3_del_hdr(ipa3_ctx->excp_hdr_hdl);
	ipa3_teardown_sys_pipe(ipa3_ctx->clnt_hdl_cmd);
}

#ifdef CONFIG_COMPAT
long compat_ipa3_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
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
		if (copy_from_user((u8 *)&nat_mem32, (u8 *)arg,
			sizeof(struct ipa3_ioc_nat_alloc_mem32))) {
			retval = -EFAULT;
			goto ret;
		}
		memcpy(nat_mem.dev_name, nat_mem32.dev_name,
				IPA_RESOURCE_NAME_MAX);
		nat_mem.size = (size_t)nat_mem32.size;
		nat_mem.offset = (off_t)nat_mem32.offset;

		/* null terminate the string */
		nat_mem.dev_name[IPA_RESOURCE_NAME_MAX - 1] = '\0';

		if (ipa3_allocate_nat_device(&nat_mem)) {
			retval = -EFAULT;
			goto ret;
		}
		nat_mem32.offset = (compat_off_t)nat_mem.offset;
		if (copy_to_user((u8 *)arg, (u8 *)&nat_mem32,
			sizeof(struct ipa3_ioc_nat_alloc_mem32))) {
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
	IPADBG_LOW("enabling gcc_ipa_clk\n");
	if (ipa3_clk) {
		clk_prepare(ipa3_clk);
		clk_enable(ipa3_clk);
		IPADBG_LOW("curr_ipa_clk_rate=%d", ipa3_ctx->curr_ipa_clk_rate);
		clk_set_rate(ipa3_clk, ipa3_ctx->curr_ipa_clk_rate);
		ipa3_uc_notify_clk_state(true);
	} else {
		WARN_ON(1);
	}

	ipa3_suspend_apps_pipes(false);
}

static unsigned int ipa3_get_bus_vote(void)
{
	unsigned int idx = 1;

	if (ipa3_ctx->curr_ipa_clk_rate == ipa3_ctx->ctrl->ipa_clk_rate_svs) {
		idx = 1;
	} else if (ipa3_ctx->curr_ipa_clk_rate ==
			ipa3_ctx->ctrl->ipa_clk_rate_nominal) {
		if (ipa3_ctx->ctrl->msm_bus_data_ptr->num_usecases <= 2)
			idx = 1;
		else
			idx = 2;
	} else if (ipa3_ctx->curr_ipa_clk_rate ==
			ipa3_ctx->ctrl->ipa_clk_rate_turbo) {
		idx = ipa3_ctx->ctrl->msm_bus_data_ptr->num_usecases - 1;
	} else {
		WARN_ON(1);
	}

	IPADBG("curr %d idx %d\n", ipa3_ctx->curr_ipa_clk_rate, idx);

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
	IPADBG("enabling IPA clocks and bus voting\n");

	ipa3_ctx->ctrl->ipa3_enable_clks();

	if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_VIRTUAL)
		if (msm_bus_scale_client_update_request(ipa3_ctx->ipa_bus_hdl,
		    ipa3_get_bus_vote()))
			WARN_ON(1);
}


/**
 * _ipa_disable_clks_v3_0() - Disable IPA clocks.
 */
void _ipa_disable_clks_v3_0(void)
{
	IPADBG_LOW("disabling gcc_ipa_clk\n");
	ipa3_suspend_apps_pipes(true);
	ipa3_uc_notify_clk_state(false);
	if (ipa3_clk)
		clk_disable_unprepare(ipa3_clk);
	else
		WARN_ON(1);
}

/**
* ipa3_disable_clks() - Turn off IPA clocks
*
* Return codes:
* None
*/
void ipa3_disable_clks(void)
{
	IPADBG("disabling IPA clocks and bus voting\n");

	ipa3_ctx->ctrl->ipa3_disable_clks();

	if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_VIRTUAL)
		if (msm_bus_scale_client_update_request(ipa3_ctx->ipa_bus_hdl,
		    0))
			WARN_ON(1);
}

/**
 * ipa3_start_tag_process() - Send TAG packet and wait for it to come back
 *
 * This function is called prior to clock gating when active client counter
 * is 1. TAG process ensures that there are no packets inside IPA HW that
 * were not submitted to peer's BAM. During TAG process all aggregation frames
 * are (force) closed.
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
void ipa3_active_clients_log_mod(struct ipa_active_client_logging_info *id,
		bool inc, bool int_ctx)
{
	char temp_str[IPA3_ACTIVE_CLIENTS_LOG_LINE_LEN];
	unsigned long long t;
	unsigned long nanosec_rem;
	struct ipa3_active_client_htable_entry *hentry;
	struct ipa3_active_client_htable_entry *hfound;
	u32 hkey;
	char str_to_hash[IPA3_ACTIVE_CLIENTS_LOG_NAME_LEN];

	hfound = NULL;
	memset(str_to_hash, 0, IPA3_ACTIVE_CLIENTS_LOG_NAME_LEN);
	strlcpy(str_to_hash, id->id_string, IPA3_ACTIVE_CLIENTS_LOG_NAME_LEN);
	hkey = arch_fast_hash(str_to_hash, IPA3_ACTIVE_CLIENTS_LOG_NAME_LEN,
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
			IPAERR("failed allocating active clients hash entry");
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
}

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
	ipa3_active_clients_lock();
	ipa3_active_clients_log_inc(id, false);
	ipa3_ctx->ipa3_active_clients.cnt++;
	if (ipa3_ctx->ipa3_active_clients.cnt == 1)
		ipa3_enable_clks();
	IPADBG_LOW("active clients = %d\n", ipa3_ctx->ipa3_active_clients.cnt);
	ipa3_active_clients_unlock();
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
	int res = 0;
	unsigned long flags;

	if (ipa3_active_clients_trylock(&flags) == 0)
		return -EPERM;

	if (ipa3_ctx->ipa3_active_clients.cnt == 0) {
		res = -EPERM;
		goto bail;
	}
	ipa3_active_clients_log_inc(id, true);
	ipa3_ctx->ipa3_active_clients.cnt++;
	IPADBG_LOW("active clients = %d\n", ipa3_ctx->ipa3_active_clients.cnt);
bail:
	ipa3_active_clients_trylock_unlock(&flags);

	return res;
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
	struct ipa_active_client_logging_info log_info;

	ipa3_active_clients_lock();
	ipa3_active_clients_log_dec(id, false);
	ipa3_ctx->ipa3_active_clients.cnt--;
	IPADBG_LOW("active clients = %d\n", ipa3_ctx->ipa3_active_clients.cnt);
	if (ipa3_ctx->ipa3_active_clients.cnt == 0) {
		if (ipa3_ctx->tag_process_before_gating) {
			ipa3_ctx->tag_process_before_gating = false;
			/*
			 * When TAG process ends, active clients will be
			 * decreased
			 */
			IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info,
					"TAG_PROCESS");
			ipa3_active_clients_log_inc(&log_info, false);
			ipa3_ctx->ipa3_active_clients.cnt = 1;
			queue_work(ipa3_ctx->power_mgmt_wq, &ipa3_tag_work);
		} else {
			ipa3_disable_clks();
		}
	}
	ipa3_active_clients_unlock();
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

int ipa3_set_required_perf_profile(enum ipa_voltage_level floor_voltage,
				  u32 bandwidth_mbps)
{
	enum ipa_voltage_level needed_voltage;
	u32 clk_rate;

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
		else
			needed_voltage = IPA_VOLTAGE_SVS;
	} else {
		IPADBG_LOW("Clock scaling is disabled\n");
		needed_voltage = IPA_VOLTAGE_NOMINAL;
	}

	needed_voltage = max(needed_voltage, floor_voltage);
	switch (needed_voltage) {
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

	ipa3_active_clients_lock();
	ipa3_ctx->curr_ipa_clk_rate = clk_rate;
	IPADBG_LOW("setting clock rate to %u\n", ipa3_ctx->curr_ipa_clk_rate);
	if (ipa3_ctx->ipa3_active_clients.cnt > 0) {
		clk_set_rate(ipa3_clk, ipa3_ctx->curr_ipa_clk_rate);
		if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_VIRTUAL)
			if (msm_bus_scale_client_update_request(
			    ipa3_ctx->ipa_bus_hdl, ipa3_get_bus_vote()))
				WARN_ON(1);
	} else {
		IPADBG_LOW("clocks are gated, not setting rate\n");
	}
	ipa3_active_clients_unlock();
	IPADBG_LOW("Done\n");
	return 0;
}

static void ipa3_sps_process_irq_schedule_rel(void)
{
	queue_delayed_work(ipa3_ctx->transport_power_mgmt_wq,
		&ipa3_sps_release_resource_work,
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

	IPADBG("interrupt=%d, interrupt_data=%u\n",
		interrupt, suspend_data);
	memset(&holb_cfg, 0, sizeof(holb_cfg));
	holb_cfg.tmr_val = 0;

	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if ((suspend_data & bmsk) && (ipa3_ctx->ep[i].valid)) {
			if (IPA_CLIENT_IS_APPS_CONS(ipa3_ctx->ep[i].client)) {
				/*
				 * pipe will be unsuspended as part of
				 * enabling IPA clocks
				 */
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
					ipa3_sps_process_irq_schedule_rel();
				}
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
					if (res) {
						IPAERR("holb en fail, stall\n");
						BUG();
					}
				}
			}
		}
		bmsk = bmsk << 1;
	}
}

/**
* ipa3_restore_suspend_handler() - restores the original suspend IRQ handler
* as it was registered in the IPA init sequence.
* Return codes:
* 0: success
* -EPERM: failed to remove current handler or failed to add original handler
* */
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

static void ipa3_sps_release_resource(struct work_struct *work)
{
	mutex_lock(&ipa3_ctx->transport_pm.transport_pm_mutex);
	/* check whether still need to decrease client usage */
	if (atomic_read(&ipa3_ctx->transport_pm.dec_clients)) {
		if (atomic_read(&ipa3_ctx->transport_pm.eot_activity)) {
			IPADBG("EOT pending Re-scheduling\n");
			ipa3_sps_process_irq_schedule_rel();
		} else {
			atomic_set(&ipa3_ctx->transport_pm.dec_clients, 0);
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL("SPS_RESOURCE");
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
			master_dev);
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
	free_irq(ipa3_res.ipa_irq, master_dev);
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

	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if (!ipa_is_ep_support_flt(i))
			continue;

		flt_tbl = &ipa3_ctx->flt_tbl[i][IPA_IP_v4];
		idr_destroy(&flt_tbl->rule_ids);
		flt_tbl = &ipa3_ctx->flt_tbl[i][IPA_IP_v6];
		idr_destroy(&flt_tbl->rule_ids);
	}
}

static void ipa3_freeze_clock_vote_and_notify_modem(void)
{
	int res;
	u32 ipa_clk_state;
	struct ipa_active_client_logging_info log_info;

	if (ipa3_ctx->smp2p_info.res_sent)
		return;

	IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info, "FREEZE_VOTE");
	res = ipa3_inc_client_enable_clks_no_block(&log_info);
	if (res)
		ipa_clk_state = 0;
	else
		ipa_clk_state = 1;

	if (ipa3_ctx->smp2p_info.out_base_id) {
		gpio_set_value(ipa3_ctx->smp2p_info.out_base_id +
			IPA_GPIO_OUT_CLK_VOTE_IDX, ipa_clk_state);
		gpio_set_value(ipa3_ctx->smp2p_info.out_base_id +
			IPA_GPIO_OUT_CLK_RSP_CMPLT_IDX, 1);
		ipa3_ctx->smp2p_info.res_sent = true;
	} else {
		IPAERR("smp2p out gpio not assigned\n");
	}

	IPADBG("IPA clocks are %s\n", ipa_clk_state ? "ON" : "OFF");
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

static int ipa3_gsi_pre_fw_load_init(void)
{
	int result;

	result = gsi_configure_regs(ipa3_res.transport_mem_base,
		ipa3_res.transport_mem_size,
		ipa3_res.ipa_mem_base);
	if (result) {
		IPAERR("Failed to configure GSI registers\n");
		return -EINVAL;
	}

	return 0;
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
	default:
		IPAERR("No GSI version for ipa type %d\n", ipa_hw_type);
		WARN_ON(1);
		gsi_ver = GSI_VER_ERR;
	}

	IPADBG("GSI version %d\n", gsi_ver);

	return gsi_ver;
}

/**
 * ipa3_post_init() - Initialize the IPA Driver (Part II).
 * This part contains all initialization which requires interaction with
 * IPA HW (via SPS BAM or GSI).
 *
 * @resource_p:	contain platform specific values from DST file
 * @pdev:	The platform device structure representing the IPA driver
 *
 * Function initialization process:
 * - Register BAM/SPS or GSI
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
	struct sps_bam_props bam_props = { 0 };
	struct gsi_per_props gsi_props;
	struct ipa3_uc_hdlrs uc_hdlrs = { 0 };

	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI) {
		memset(&gsi_props, 0, sizeof(gsi_props));
		gsi_props.ver = ipa3_get_gsi_ver(resource_p->ipa_hw_type);
		gsi_props.ee = resource_p->ee;
		gsi_props.intr = GSI_INTR_IRQ;
		gsi_props.irq = resource_p->transport_irq;
		gsi_props.phys_addr = resource_p->transport_mem_base;
		gsi_props.size = resource_p->transport_mem_size;
		gsi_props.notify_cb = ipa_gsi_notify_cb;
		gsi_props.req_clk_cb = NULL;
		gsi_props.rel_clk_cb = NULL;

		result = gsi_register_device(&gsi_props,
			&ipa3_ctx->gsi_dev_hdl);
		if (result != GSI_STATUS_SUCCESS) {
			IPAERR(":gsi register error - %d\n", result);
			result = -ENODEV;
			goto fail_register_device;
		}
		IPADBG("IPA gsi is registered\n");
	} else {
		/* register IPA with SPS driver */
		bam_props.phys_addr = resource_p->transport_mem_base;
		bam_props.virt_size = resource_p->transport_mem_size;
		bam_props.irq = resource_p->transport_irq;
		bam_props.num_pipes = ipa3_ctx->ipa_num_pipes;
		bam_props.summing_threshold = IPA_SUMMING_THRESHOLD;
		bam_props.event_threshold = IPA_EVENT_THRESHOLD;
		bam_props.options |= SPS_BAM_NO_LOCAL_CLK_GATING;
		if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_VIRTUAL)
			bam_props.options |= SPS_BAM_OPT_IRQ_WAKEUP;
		if (ipa3_ctx->ipa_bam_remote_mode == true)
			bam_props.manage |= SPS_BAM_MGR_DEVICE_REMOTE;
		if (!ipa3_ctx->smmu_s1_bypass)
			bam_props.options |= SPS_BAM_SMMU_EN;
		bam_props.ee = resource_p->ee;
		bam_props.ipc_loglevel = 3;

		result = sps_register_bam_device(&bam_props,
			&ipa3_ctx->bam_handle);
		if (result) {
			IPAERR(":bam register error - %d\n", result);
			result = -EPROBE_DEFER;
			goto fail_register_device;
		}
		IPADBG("IPA BAM is registered\n");
	}

	/* setup the AP-IPA pipes */
	if (ipa3_setup_apps_pipes()) {
		IPAERR(":failed to setup IPA-Apps pipes\n");
		result = -ENODEV;
		goto fail_setup_apps_pipes;
	}
	IPADBG("IPA System2Bam pipes were connected\n");

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

	ipa3_debugfs_init();

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

	result = ipa3_ntn_init();
	if (result)
		IPAERR(":ntn init failed (%d)\n", -result);
	else
		IPADBG(":ntn init ok\n");

	ipa3_register_panic_hdlr();

	ipa3_ctx->q6_proxy_clk_vote_valid = true;

	mutex_lock(&ipa3_ctx->lock);
	ipa3_ctx->ipa_initialization_complete = true;
	mutex_unlock(&ipa3_ctx->lock);

	ipa3_trigger_ipa_ready_cbs();
	complete_all(&ipa3_ctx->init_completion_obj);
	pr_info("IPA driver initialization was successful.\n");

	return 0;

fail_teth_bridge_driver_init:
	ipa3_teardown_apps_pipes();
fail_setup_apps_pipes:
	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI)
		gsi_deregister_device(ipa3_ctx->gsi_dev_hdl, false);
	else
		sps_deregister_bam_device(ipa3_ctx->bam_handle);
fail_register_device:
	ipa_rm_delete_resource(IPA_RM_RESOURCE_APPS_CONS);
	ipa_rm_exit();
	cdev_del(&ipa3_ctx->cdev);
	device_destroy(ipa3_ctx->class, ipa3_ctx->dev_num);
	unregister_chrdev_region(ipa3_ctx->dev_num, 1);
	if (ipa3_ctx->pipe_mem_pool)
		gen_pool_destroy(ipa3_ctx->pipe_mem_pool);
	ipa3_destroy_flt_tbl_idrs();
	idr_destroy(&ipa3_ctx->ipa_idr);
	kmem_cache_destroy(ipa3_ctx->rx_pkt_wrapper_cache);
	kmem_cache_destroy(ipa3_ctx->tx_pkt_wrapper_cache);
	kmem_cache_destroy(ipa3_ctx->rt_tbl_cache);
	kmem_cache_destroy(ipa3_ctx->hdr_proc_ctx_offset_cache);
	kmem_cache_destroy(ipa3_ctx->hdr_proc_ctx_cache);
	kmem_cache_destroy(ipa3_ctx->hdr_offset_cache);
	kmem_cache_destroy(ipa3_ctx->hdr_cache);
	kmem_cache_destroy(ipa3_ctx->rt_rule_cache);
	kmem_cache_destroy(ipa3_ctx->flt_rule_cache);
	destroy_workqueue(ipa3_ctx->transport_power_mgmt_wq);
	destroy_workqueue(ipa3_ctx->power_mgmt_wq);
	iounmap(ipa3_ctx->mmio);
	ipa3_disable_clks();
	msm_bus_scale_unregister_client(ipa3_ctx->ipa_bus_hdl);
	if (ipa3_bus_scale_table) {
		msm_bus_cl_clear_pdata(ipa3_bus_scale_table);
		ipa3_bus_scale_table = NULL;
	}
	kfree(ipa3_ctx->ctrl);
	kfree(ipa3_ctx);
	ipa3_ctx = NULL;
	return result;
}

static int ipa3_trigger_fw_loading_mdms(void)
{
	int result;
	const struct firmware *fw;

	IPADBG("FW loading process initiated\n");

	result = request_firmware(&fw, IPA_FWS_PATH, ipa3_ctx->dev);
	if (result < 0) {
		IPAERR("request_firmware failed, error %d\n", result);
		return result;
	}
	if (fw == NULL) {
		IPAERR("Firmware is NULL!\n");
		return -EINVAL;
	}

	IPADBG("FWs are available for loading\n");

	result = ipa3_load_fws(fw);
	if (result) {
		IPAERR("IPA FWs loading has failed\n");
		release_firmware(fw);
		return result;
	}

	result = gsi_enable_fw(ipa3_res.transport_mem_base,
				ipa3_res.transport_mem_size);
	if (result) {
		IPAERR("Failed to enable GSI FW\n");
		release_firmware(fw);
		return result;
	}

	release_firmware(fw);

	IPADBG("FW loading process is complete\n");
	return 0;
}

static int ipa3_trigger_fw_loading_msms(void)
{
	void *subsystem_get_retval = NULL;

	IPADBG("FW loading process initiated\n");

	subsystem_get_retval = subsystem_get(IPA_SUBSYSTEM_NAME);
	if (IS_ERR_OR_NULL(subsystem_get_retval)) {
		IPAERR("Unable to trigger PIL process for FW loading\n");
		return -EINVAL;
	}

	IPADBG("FW loading process is complete\n");
	return 0;
}

static ssize_t ipa3_write(struct file *file, const char __user *buf,
			  size_t count, loff_t *ppos)
{
	unsigned long missing;
	int result = -EINVAL;

	char dbg_buff[16] = { 0 };

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, buf, count);

	if (missing) {
		IPAERR("Unable to copy data from user\n");
		return -EFAULT;
	}

	/* Prevent consequent calls from trying to load the FW again. */
	if (ipa3_is_ready())
		return count;

	/*
	 * We will trigger the process only if we're in GSI mode, otherwise,
	 * we just ignore the write.
	 */
	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI) {
		IPA_ACTIVE_CLIENTS_INC_SIMPLE();

		if (ipa3_is_msm_device())
			result = ipa3_trigger_fw_loading_msms();
		else
			result = ipa3_trigger_fw_loading_mdms();
		/* No IPAv3.x chipsets that don't support FW loading */

		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

		if (result) {
			IPAERR("FW loading process has failed\n");
			BUG();
		} else
			ipa3_post_init(&ipa3_res, ipa3_ctx->dev);
	}
	return count;
}

static int ipa3_tz_unlock_reg(struct ipa3_context *ipa3_ctx)
{
	int i, size, ret, resp;
	struct tz_smmu_ipa_protect_region_iovec_s *ipa_tz_unlock_vec;
	struct tz_smmu_ipa_protect_region_s cmd_buf;

	if (ipa3_ctx && ipa3_ctx->ipa_tz_unlock_reg_num > 0) {
		size = ipa3_ctx->ipa_tz_unlock_reg_num *
			sizeof(struct tz_smmu_ipa_protect_region_iovec_s);
		ipa_tz_unlock_vec = kzalloc(PAGE_ALIGN(size), GFP_KERNEL);
		if (ipa_tz_unlock_vec == NULL)
			return -ENOMEM;

		for (i = 0; i < ipa3_ctx->ipa_tz_unlock_reg_num; i++) {
			ipa_tz_unlock_vec[i].input_addr =
				ipa3_ctx->ipa_tz_unlock_reg[i].reg_addr ^
				(ipa3_ctx->ipa_tz_unlock_reg[i].reg_addr &
				0xFFF);
			ipa_tz_unlock_vec[i].output_addr =
				ipa3_ctx->ipa_tz_unlock_reg[i].reg_addr ^
				(ipa3_ctx->ipa_tz_unlock_reg[i].reg_addr &
				0xFFF);
			ipa_tz_unlock_vec[i].size =
				ipa3_ctx->ipa_tz_unlock_reg[i].size;
			ipa_tz_unlock_vec[i].attr = IPA_TZ_UNLOCK_ATTRIBUTE;
		}

		/* pass physical address of command buffer */
		cmd_buf.iovec_buf = virt_to_phys((void *)ipa_tz_unlock_vec);
		cmd_buf.size_bytes = size;

		/* flush cache to DDR */
		__cpuc_flush_dcache_area((void *)ipa_tz_unlock_vec, size);
		outer_flush_range(cmd_buf.iovec_buf, cmd_buf.iovec_buf + size);

		ret = scm_call(SCM_SVC_MP, TZ_MEM_PROTECT_REGION_ID, &cmd_buf,
				sizeof(cmd_buf), &resp, sizeof(resp));
		if (ret) {
			IPAERR("scm call SCM_SVC_MP failed: %d\n", ret);
			kfree(ipa_tz_unlock_vec);
			return -EFAULT;
		}
		kfree(ipa_tz_unlock_vec);
	}
	return 0;
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
* - Allocate memory for the driver context data struct
* - Initializing the ipa3_ctx with:
*    1)parsed values from the dts file
*    2)parameters passed to the module initialization
*    3)read HW values(such as core memory size)
* - Map IPA core registers to CPU memory
* - Restart IPA core(HW reset)
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
* - Initialize the filter block by committing IPV4 and IPV6 default rules
* - Create empty routing table in system memory(no committing)
* - Initialize pipes memory pool with ipa3_pipe_mem_init for supported platforms
* - Create a char-device for IPA
* - Initialize IPA RM (resource manager)
* - Configure GSI registers (in GSI case)
*/
static int ipa3_pre_init(const struct ipa3_plat_drv_res *resource_p,
		struct device *ipa_dev)
{
	int result = 0;
	int i;
	struct ipa3_flt_tbl *flt_tbl;
	struct ipa3_rt_tbl_set *rset;
	struct ipa_active_client_logging_info log_info;

	IPADBG("IPA Driver initialization started\n");

	ipa3_ctx = kzalloc(sizeof(*ipa3_ctx), GFP_KERNEL);
	if (!ipa3_ctx) {
		IPAERR(":kzalloc err.\n");
		result = -ENOMEM;
		goto fail_mem_ctx;
	}

	ipa3_ctx->logbuf = ipc_log_context_create(IPA_IPC_LOG_PAGES, "ipa", 0);
	if (ipa3_ctx->logbuf == NULL) {
		IPAERR("failed to get logbuf\n");
		result = -ENOMEM;
		goto fail_logbuf;
	}

	ipa3_ctx->pdev = ipa_dev;
	ipa3_ctx->uc_pdev = ipa_dev;
	ipa3_ctx->smmu_present = smmu_info.present;
	if (!ipa3_ctx->smmu_present)
		ipa3_ctx->smmu_s1_bypass = true;
	else
		ipa3_ctx->smmu_s1_bypass = smmu_info.s1_bypass;
	ipa3_ctx->ipa_wrapper_base = resource_p->ipa_mem_base;
	ipa3_ctx->ipa_wrapper_size = resource_p->ipa_mem_size;
	ipa3_ctx->ipa_hw_type = resource_p->ipa_hw_type;
	ipa3_ctx->ipa3_hw_mode = resource_p->ipa3_hw_mode;
	ipa3_ctx->use_ipa_teth_bridge = resource_p->use_ipa_teth_bridge;
	ipa3_ctx->ipa_bam_remote_mode = resource_p->ipa_bam_remote_mode;
	ipa3_ctx->modem_cfg_emb_pipe_flt = resource_p->modem_cfg_emb_pipe_flt;
	ipa3_ctx->ipa_wdi2 = resource_p->ipa_wdi2;
	ipa3_ctx->use_64_bit_dma_mask = resource_p->use_64_bit_dma_mask;
	ipa3_ctx->wan_rx_ring_size = resource_p->wan_rx_ring_size;
	ipa3_ctx->lan_rx_ring_size = resource_p->lan_rx_ring_size;
	ipa3_ctx->skip_uc_pipe_reset = resource_p->skip_uc_pipe_reset;
	ipa3_ctx->tethered_flow_control = resource_p->tethered_flow_control;
	ipa3_ctx->transport_prototype = resource_p->transport_prototype;
	ipa3_ctx->ee = resource_p->ee;
	ipa3_ctx->apply_rg10_wa = resource_p->apply_rg10_wa;
	ipa3_ctx->gsi_ch20_wa = resource_p->gsi_ch20_wa;
	ipa3_ctx->ipa3_active_clients_logging.log_rdy = false;
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
	}

	/* unlock registers for uc */
	ipa3_tz_unlock_reg(ipa3_ctx);

	/* default aggregation parameters */
	ipa3_ctx->aggregation_type = IPA_MBIM_16;
	ipa3_ctx->aggregation_byte_limit = 1;
	ipa3_ctx->aggregation_time_limit = 0;

	ipa3_ctx->ctrl = kzalloc(sizeof(*ipa3_ctx->ctrl), GFP_KERNEL);
	if (!ipa3_ctx->ctrl) {
		IPAERR("memory allocation error for ctrl\n");
		result = -ENOMEM;
		goto fail_mem_ctrl;
	}
	result = ipa3_controller_static_bind(ipa3_ctx->ctrl,
			ipa3_ctx->ipa_hw_type);
	if (result) {
		IPAERR("fail to static bind IPA ctrl.\n");
		result = -EFAULT;
		goto fail_bind;
	}

	result = ipa3_init_mem_partition(master_dev->of_node);
	if (result) {
		IPAERR(":ipa3_init_mem_partition failed!\n");
		result = -ENODEV;
		goto fail_init_mem_partition;
	}

	if (ipa3_bus_scale_table) {
		IPADBG("Use bus scaling info from device tree\n");
		ipa3_ctx->ctrl->msm_bus_data_ptr = ipa3_bus_scale_table;
	}

	if (ipa3_ctx->ipa3_hw_mode != IPA_HW_MODE_VIRTUAL) {
		/* get BUS handle */
		ipa3_ctx->ipa_bus_hdl =
			msm_bus_scale_register_client(
				ipa3_ctx->ctrl->msm_bus_data_ptr);
		if (!ipa3_ctx->ipa_bus_hdl) {
			IPAERR("fail to register with bus mgr!\n");
			result = -ENODEV;
			goto fail_bus_reg;
		}
	} else {
		IPADBG("Skipping bus scaling registration on Virtual plat\n");
	}

	/* get IPA clocks */
	result = ipa3_get_clks(master_dev);
	if (result)
		goto fail_clk;

	/* init active_clients_log after getting ipa-clk */
	if (ipa3_active_clients_log_init())
		goto fail_init_active_client;

	/* Enable ipa3_ctx->enable_clock_scaling */
	ipa3_ctx->enable_clock_scaling = 1;
	ipa3_ctx->curr_ipa_clk_rate = ipa3_ctx->ctrl->ipa_clk_rate_turbo;

	/* enable IPA clocks explicitly to allow the initialization */
	ipa3_enable_clks();

	/* setup IPA register access */
	IPADBG("Mapping 0x%x\n", resource_p->ipa_mem_base +
		ipa3_ctx->ctrl->ipa_reg_base_ofst);
	ipa3_ctx->mmio = ioremap(resource_p->ipa_mem_base +
			ipa3_ctx->ctrl->ipa_reg_base_ofst,
			resource_p->ipa_mem_size);
	if (!ipa3_ctx->mmio) {
		IPAERR(":ipa-base ioremap err.\n");
		result = -EFAULT;
		goto fail_remap;
	}

	if (ipahal_init(ipa3_ctx->ipa_hw_type, ipa3_ctx->mmio,
		ipa3_ctx->pdev)) {
		IPAERR("fail to init ipahal\n");
		result = -EFAULT;
		goto fail_ipahal;
	}

	result = ipa3_init_hw();
	if (result) {
		IPAERR(":error initializing HW.\n");
		result = -ENODEV;
		goto fail_init_hw;
	}
	IPADBG("IPA HW initialization sequence completed");

	ipa3_ctx->ipa_num_pipes = ipa3_get_num_pipes();
	if (ipa3_ctx->ipa_num_pipes > IPA3_MAX_NUM_PIPES) {
		IPAERR("IPA has more pipes then supported! has %d, max %d\n",
			ipa3_ctx->ipa_num_pipes, IPA3_MAX_NUM_PIPES);
		result = -ENODEV;
		goto fail_init_hw;
	}

	ipa_init_ep_flt_bitmap();
	IPADBG("EP with flt support bitmap 0x%x (%u pipes)\n",
		ipa3_ctx->ep_flt_bitmap, ipa3_ctx->ep_flt_num);

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

	mutex_init(&ipa3_ctx->ipa3_active_clients.mutex);
	spin_lock_init(&ipa3_ctx->ipa3_active_clients.spinlock);
	IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info, "PROXY_CLK_VOTE");
	ipa3_active_clients_log_inc(&log_info, false);
	ipa3_ctx->ipa3_active_clients.cnt = 1;

	/* Assign resource limitation to each group */
	ipa3_set_resorce_groups_min_max_limits();

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

	/* Initialize the SPS PM lock. */
	mutex_init(&ipa3_ctx->transport_pm.transport_pm_mutex);
	spin_lock_init(&ipa3_ctx->transport_pm.lock);
	ipa3_ctx->transport_pm.res_granted = false;
	ipa3_ctx->transport_pm.res_rel_in_prog = false;

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

	/* Setup DMA pool */
	ipa3_ctx->dma_pool = dma_pool_create("ipa_tx", ipa3_ctx->pdev,
		IPA_NUM_DESC_PER_SW_TX * sizeof(struct sps_iovec),
		0, 0);
	if (!ipa3_ctx->dma_pool) {
		IPAERR("cannot alloc DMA pool.\n");
		result = -ENOMEM;
		goto fail_dma_pool;
	}

	/* init the various list heads */
	INIT_LIST_HEAD(&ipa3_ctx->hdr_tbl.head_hdr_entry_list);
	for (i = 0; i < IPA_HDR_BIN_MAX; i++) {
		INIT_LIST_HEAD(&ipa3_ctx->hdr_tbl.head_offset_list[i]);
		INIT_LIST_HEAD(&ipa3_ctx->hdr_tbl.head_free_offset_list[i]);
	}
	INIT_LIST_HEAD(&ipa3_ctx->hdr_proc_ctx_tbl.head_proc_ctx_entry_list);
	for (i = 0; i < IPA_HDR_PROC_CTX_BIN_MAX; i++) {
		INIT_LIST_HEAD(&ipa3_ctx->hdr_proc_ctx_tbl.head_offset_list[i]);
		INIT_LIST_HEAD(&ipa3_ctx->
				hdr_proc_ctx_tbl.head_free_offset_list[i]);
	}
	INIT_LIST_HEAD(&ipa3_ctx->rt_tbl_set[IPA_IP_v4].head_rt_tbl_list);
	INIT_LIST_HEAD(&ipa3_ctx->rt_tbl_set[IPA_IP_v6].head_rt_tbl_list);
	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if (!ipa_is_ep_support_flt(i))
			continue;

		flt_tbl = &ipa3_ctx->flt_tbl[i][IPA_IP_v4];
		INIT_LIST_HEAD(&flt_tbl->head_flt_rule_list);
		flt_tbl->in_sys[IPA_RULE_HASHABLE] =
			!ipa3_ctx->ip4_flt_tbl_hash_lcl;
		flt_tbl->in_sys[IPA_RULE_NON_HASHABLE] =
			!ipa3_ctx->ip4_flt_tbl_nhash_lcl;
		idr_init(&flt_tbl->rule_ids);

		flt_tbl = &ipa3_ctx->flt_tbl[i][IPA_IP_v6];
		INIT_LIST_HEAD(&flt_tbl->head_flt_rule_list);
		flt_tbl->in_sys[IPA_RULE_HASHABLE] =
			!ipa3_ctx->ip6_flt_tbl_hash_lcl;
		flt_tbl->in_sys[IPA_RULE_NON_HASHABLE] =
			!ipa3_ctx->ip6_flt_tbl_nhash_lcl;
		idr_init(&flt_tbl->rule_ids);
	}

	rset = &ipa3_ctx->reap_rt_tbl_set[IPA_IP_v4];
	INIT_LIST_HEAD(&rset->head_rt_tbl_list);
	rset = &ipa3_ctx->reap_rt_tbl_set[IPA_IP_v6];
	INIT_LIST_HEAD(&rset->head_rt_tbl_list);

	INIT_LIST_HEAD(&ipa3_ctx->intf_list);
	INIT_LIST_HEAD(&ipa3_ctx->msg_list);
	INIT_LIST_HEAD(&ipa3_ctx->pull_msg_list);
	init_waitqueue_head(&ipa3_ctx->msg_waitq);
	mutex_init(&ipa3_ctx->msg_lock);

	mutex_init(&ipa3_ctx->lock);
	mutex_init(&ipa3_ctx->nat_mem.lock);

	idr_init(&ipa3_ctx->ipa_idr);
	spin_lock_init(&ipa3_ctx->idr_lock);

	/* wlan related member */
	memset(&ipa3_ctx->wc_memb, 0, sizeof(ipa3_ctx->wc_memb));
	spin_lock_init(&ipa3_ctx->wc_memb.wlan_spinlock);
	spin_lock_init(&ipa3_ctx->wc_memb.ipa_tx_mul_spinlock);
	INIT_LIST_HEAD(&ipa3_ctx->wc_memb.wlan_comm_desc_list);

	/* setup the IPA pipe mem pool */
	if (resource_p->ipa_pipe_mem_size)
		ipa3_pipe_mem_init(resource_p->ipa_pipe_mem_start_ofst,
				resource_p->ipa_pipe_mem_size);

	ipa3_ctx->class = class_create(THIS_MODULE, DRV_NAME);

	result = alloc_chrdev_region(&ipa3_ctx->dev_num, 0, 1, DRV_NAME);
	if (result) {
		IPAERR("alloc_chrdev_region err.\n");
		result = -ENODEV;
		goto fail_alloc_chrdev_region;
	}

	ipa3_ctx->dev = device_create(ipa3_ctx->class, NULL, ipa3_ctx->dev_num,
			ipa3_ctx, DRV_NAME);
	if (IS_ERR(ipa3_ctx->dev)) {
		IPAERR(":device_create err.\n");
		result = -ENODEV;
		goto fail_device_create;
	}

	cdev_init(&ipa3_ctx->cdev, &ipa3_drv_fops);
	ipa3_ctx->cdev.owner = THIS_MODULE;
	ipa3_ctx->cdev.ops = &ipa3_drv_fops;  /* from LDD3 */

	result = cdev_add(&ipa3_ctx->cdev, ipa3_ctx->dev_num, 1);
	if (result) {
		IPAERR(":cdev_add err=%d\n", -result);
		result = -ENODEV;
		goto fail_cdev_add;
	}
	IPADBG("ipa cdev added successful. major:%d minor:%d\n",
			MAJOR(ipa3_ctx->dev_num),
			MINOR(ipa3_ctx->dev_num));

	if (ipa3_create_nat_device()) {
		IPAERR("unable to create nat device\n");
		result = -ENODEV;
		goto fail_nat_dev_add;
	}

	/* Create a wakeup source. */
	wakeup_source_init(&ipa3_ctx->w_lock, "IPA_WS");
	spin_lock_init(&ipa3_ctx->wakelock_ref_cnt.spinlock);

	/* Initialize IPA RM (resource manager) */
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

	if (!ipa3_ctx->apply_rg10_wa) {
		result = ipa3_init_interrupts();
		if (result) {
			IPAERR("ipa initialization of interrupts failed\n");
			result = -ENODEV;
			goto fail_ipa_init_interrupts;
		}
	} else {
		IPADBG("Initialization of ipa interrupts skipped\n");
	}

	INIT_LIST_HEAD(&ipa3_ctx->ipa_ready_cb_list);

	init_completion(&ipa3_ctx->init_completion_obj);
	init_completion(&ipa3_ctx->uc_loaded_completion_obj);

	/*
	 * For GSI, we can't register the GSI driver yet, as it expects
	 * the GSI FW to be up and running before the registration.
	 */
	if (ipa3_ctx->transport_prototype == IPA_TRANSPORT_TYPE_GSI) {
		/*
		 * For IPA3.0, the GSI configuration is done by the GSI driver.
		 * For IPA3.1 (and on), the GSI configuration is done by TZ.
		 */
		if (ipa3_ctx->ipa_hw_type == IPA_HW_v3_0) {
			result = ipa3_gsi_pre_fw_load_init();
			if (result) {
				IPAERR("gsi pre FW loading config failed\n");
				result = -ENODEV;
				goto fail_ipa_init_interrupts;
			}
		}
	}
	/* For BAM (No other mode), we can just carry on with initialization */
	else
		return ipa3_post_init(resource_p, ipa_dev);

	return 0;

fail_ipa_init_interrupts:
	ipa_rm_delete_resource(IPA_RM_RESOURCE_APPS_CONS);
fail_create_apps_resource:
	ipa_rm_exit();
fail_ipa_rm_init:
fail_nat_dev_add:
	cdev_del(&ipa3_ctx->cdev);
fail_cdev_add:
	device_destroy(ipa3_ctx->class, ipa3_ctx->dev_num);
fail_device_create:
	unregister_chrdev_region(ipa3_ctx->dev_num, 1);
fail_alloc_chrdev_region:
	if (ipa3_ctx->pipe_mem_pool)
		gen_pool_destroy(ipa3_ctx->pipe_mem_pool);
	ipa3_destroy_flt_tbl_idrs();
	idr_destroy(&ipa3_ctx->ipa_idr);
fail_dma_pool:
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
	ipahal_destroy();
fail_ipahal:
	iounmap(ipa3_ctx->mmio);
fail_remap:
	ipa3_disable_clks();
	ipa3_active_clients_log_destroy();
fail_init_active_client:
fail_clk:
	msm_bus_scale_unregister_client(ipa3_ctx->ipa_bus_hdl);
fail_bus_reg:
fail_init_mem_partition:
fail_bind:
	kfree(ipa3_ctx->ctrl);
fail_mem_ctrl:
	kfree(ipa3_ctx->ipa_tz_unlock_reg);
fail_tz_unlock_reg:
	ipc_log_context_destroy(ipa3_ctx->logbuf);
fail_logbuf:
	kfree(ipa3_ctx);
	ipa3_ctx = NULL;
fail_mem_ctx:
	return result;
}

static int get_ipa_dts_configuration(struct platform_device *pdev,
		struct ipa3_plat_drv_res *ipa_drv_res)
{
	int i, result, pos;
	struct resource *resource;
	u32 *ipa_tz_unlock_reg;
	int elem_num;

	/* initialize ipa3_res */
	ipa_drv_res->ipa_pipe_mem_start_ofst = IPA_PIPE_MEM_START_OFST;
	ipa_drv_res->ipa_pipe_mem_size = IPA_PIPE_MEM_SIZE;
	ipa_drv_res->ipa_hw_type = 0;
	ipa_drv_res->ipa3_hw_mode = 0;
	ipa_drv_res->ipa_bam_remote_mode = false;
	ipa_drv_res->modem_cfg_emb_pipe_flt = false;
	ipa_drv_res->ipa_wdi2 = false;
	ipa_drv_res->use_64_bit_dma_mask = false;
	ipa_drv_res->wan_rx_ring_size = IPA_GENERIC_RX_POOL_SZ;
	ipa_drv_res->lan_rx_ring_size = IPA_GENERIC_RX_POOL_SZ;
	ipa_drv_res->apply_rg10_wa = false;
	ipa_drv_res->gsi_ch20_wa = false;
	ipa_drv_res->ipa_tz_unlock_reg_num = 0;
	ipa_drv_res->ipa_tz_unlock_reg = NULL;

	/* Get IPA HW Version */
	result = of_property_read_u32(pdev->dev.of_node, "qcom,ipa-hw-ver",
					&ipa_drv_res->ipa_hw_type);
	if ((result) || (ipa_drv_res->ipa_hw_type == 0)) {
		IPAERR(":get resource failed for ipa-hw-ver!\n");
		return -ENODEV;
	}
	IPADBG(": ipa_hw_type = %d", ipa_drv_res->ipa_hw_type);

	if (ipa_drv_res->ipa_hw_type < IPA_HW_v3_0) {
		IPAERR(":IPA version below 3.0 not supported!\n");
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

	ipa_drv_res->ipa_wdi2 =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,ipa-wdi2");
	IPADBG(": WDI-2.0 = %s\n",
			ipa_drv_res->ipa_wdi2
			? "True" : "False");

	ipa_drv_res->use_64_bit_dma_mask =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,use-64-bit-dma-mask");
	IPADBG(": use_64_bit_dma_mask = %s\n",
			ipa_drv_res->use_64_bit_dma_mask
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

	if (of_property_read_bool(pdev->dev.of_node,
		"qcom,use-gsi"))
		ipa_drv_res->transport_prototype = IPA_TRANSPORT_TYPE_GSI;
	else
		ipa_drv_res->transport_prototype = IPA_TRANSPORT_TYPE_SPS;

	IPADBG(": transport type = %s\n",
		ipa_drv_res->transport_prototype == IPA_TRANSPORT_TYPE_SPS
		? "SPS" : "GSI");

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

	if (ipa_drv_res->transport_prototype == IPA_TRANSPORT_TYPE_SPS) {
		/* Get IPA BAM address */
		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
				"bam-base");
		if (!resource) {
			IPAERR(":get resource failed for bam-base!\n");
			return -ENODEV;
		}
		ipa_drv_res->transport_mem_base = resource->start;
		ipa_drv_res->transport_mem_size = resource_size(resource);
		IPADBG(": bam-base = 0x%x, size = 0x%x\n",
				ipa_drv_res->transport_mem_base,
				ipa_drv_res->transport_mem_size);

		/* Get IPA BAM IRQ number */
		resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
				"bam-irq");
		if (!resource) {
			IPAERR(":get resource failed for bam-irq!\n");
			return -ENODEV;
		}
		ipa_drv_res->transport_irq = resource->start;
		IPADBG(": bam-irq = %d\n", ipa_drv_res->transport_irq);
	} else {
		/* Get IPA GSI address */
		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
				"gsi-base");
		if (!resource) {
			IPAERR(":get resource failed for gsi-base!\n");
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
			IPAERR(":get resource failed for gsi-irq!\n");
			return -ENODEV;
		}
		ipa_drv_res->transport_irq = resource->start;
		IPADBG(": gsi-irq = %d\n", ipa_drv_res->transport_irq);
	}

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

	result = of_property_read_u32(pdev->dev.of_node, "qcom,ee",
			&ipa_drv_res->ee);
	if (result)
		ipa_drv_res->ee = 0;

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
		ipa_drv_res->apply_rg10_wa
		? "Needed" : "Not needed");

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
			IPADBG("tz unlock reg %d: addr 0x%pa size %d\n", i,
				&ipa_drv_res->ipa_tz_unlock_reg[i].reg_addr,
				ipa_drv_res->ipa_tz_unlock_reg[i].size);
		}
		kfree(ipa_tz_unlock_reg);
	}
	return 0;
}

static int ipa_smmu_wlan_cb_probe(struct device *dev)
{
	struct ipa_smmu_cb_ctx *cb = ipa3_get_wlan_smmu_ctx();
	int atomic_ctx = 1;
	int fast = 1;
	int bypass = 1;
	int ret;
	u32 add_map_size;
	const u32 *add_map;
	int i;

	IPADBG("sub pdev=%p\n", dev);

	cb->dev = dev;
	cb->iommu = iommu_domain_alloc(msm_iommu_get_bus(dev));
	if (!cb->iommu) {
		IPAERR("could not alloc iommu domain\n");
		/* assume this failure is because iommu driver is not ready */
		return -EPROBE_DEFER;
	}
	cb->valid = true;

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
			IPAERR("couldn't disable coherent HTW\n");
			cb->valid = false;
			return -EIO;
		}
		IPADBG("SMMU ATTR ATOMIC\n");

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
			IPADBG("mapping 0x%lx to 0x%pa size %d\n",
				iova_p, &pa_p, size_p);
			ipa3_iommu_map(cb->iommu,
				iova_p, pa_p, size_p,
				IOMMU_READ | IOMMU_WRITE | IOMMU_DEVICE);
		}
	}
	return 0;
}

static int ipa_smmu_uc_cb_probe(struct device *dev)
{
	struct ipa_smmu_cb_ctx *cb = ipa3_get_uc_smmu_ctx();
	int atomic_ctx = 1;
	int bypass = 1;
	int fast = 1;
	int ret;
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
	ipa3_ctx->uc_pdev = dev;

	return 0;
}

static int ipa_smmu_ap_cb_probe(struct device *dev)
{
	struct ipa_smmu_cb_ctx *cb = ipa3_get_smmu_ctx();
	int result;
	int atomic_ctx = 1;
	int fast = 1;
	int bypass = 1;
	u32 iova_ap_mapping[2];
	u32 add_map_size;
	const u32 *add_map;
	void *smem_addr;
	int i;

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
			IPADBG("mapping 0x%lx to 0x%pa size %d\n",
				iova_p, &pa_p, size_p);
			ipa3_iommu_map(cb->mapping->domain,
				iova_p, pa_p, size_p,
				IOMMU_READ | IOMMU_WRITE | IOMMU_DEVICE);
		}
	}

	/* map SMEM memory for IPA table accesses */
	smem_addr = smem_alloc(SMEM_IPA_FILTER_TABLE, IPA_SMEM_SIZE,
		SMEM_MODEM, 0);
	if (smem_addr) {
		phys_addr_t iova = smem_virt_to_phys(smem_addr);
		phys_addr_t pa = iova;
		unsigned long iova_p;
		phys_addr_t pa_p;
		u32 size_p;

		IPA_SMMU_ROUND_TO_PAGE(iova, pa, IPA_SMEM_SIZE,
			iova_p, pa_p, size_p);
		IPADBG("mapping 0x%lx to 0x%pa size %d\n",
			iova_p, &pa_p, size_p);
		ipa3_iommu_map(cb->mapping->domain,
			iova_p, pa_p, size_p,
			IOMMU_READ | IOMMU_WRITE | IOMMU_DEVICE);
	}


	smmu_info.present = true;

	if (!ipa3_bus_scale_table)
		ipa3_bus_scale_table = msm_bus_cl_get_pdata(ipa3_pdev);

	/* Proceed to real initialization */
	result = ipa3_pre_init(&ipa3_res, dev);
	if (result) {
		IPAERR("ipa_init failed\n");
		arm_iommu_detach_device(cb->dev);
		arm_iommu_release_mapping(cb->mapping);
		cb->valid = false;
		return result;
	}

	return result;
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

	IPADBG("node->name=%s\n", node->name);
	if (strcmp("qcom,smp2pgpio_map_ipa_1_out", node->name) == 0) {
		res = of_get_gpio(node, 0);
		if (res < 0) {
			IPADBG("of_get_gpio returned %d\n", res);
			return res;
		}

		ipa3_ctx->smp2p_info.out_base_id = res;
		IPADBG("smp2p out_base_id=%d\n",
			ipa3_ctx->smp2p_info.out_base_id);
	} else if (strcmp("qcom,smp2pgpio_map_ipa_1_in", node->name) == 0) {
		int irq;

		res = of_get_gpio(node, 0);
		if (res < 0) {
			IPADBG("of_get_gpio returned %d\n", res);
			return res;
		}

		ipa3_ctx->smp2p_info.in_base_id = res;
		IPADBG("smp2p in_base_id=%d\n",
			ipa3_ctx->smp2p_info.in_base_id);

		/* register for modem clk query */
		irq = gpio_to_irq(ipa3_ctx->smp2p_info.in_base_id +
			IPA_GPIO_IN_QUERY_CLK_IDX);
		if (irq < 0) {
			IPAERR("gpio_to_irq failed %d\n", irq);
			return -ENODEV;
		}
		IPADBG("smp2p irq#=%d\n", irq);
		res = request_irq(irq,
			(irq_handler_t)ipa3_smp2p_modem_clk_query_isr,
			IRQF_TRIGGER_RISING, "ipa_smp2p_clk_vote", dev);
		if (res) {
			IPAERR("fail to register smp2p irq=%d\n", irq);
			return -ENODEV;
		}
		res = enable_irq_wake(ipa3_ctx->smp2p_info.in_base_id +
			IPA_GPIO_IN_QUERY_CLK_IDX);
		if (res)
			IPAERR("failed to enable irq wake\n");
	}

	return 0;
}

int ipa3_plat_drv_probe(struct platform_device *pdev_p,
	struct ipa_api_controller *api_ctrl, struct of_device_id *pdrv_match)
{
	int result;
	struct device *dev = &pdev_p->dev;

	IPADBG("IPA driver probing started\n");
	IPADBG("dev->of_node->name = %s\n", dev->of_node->name);

	if (of_device_is_compatible(dev->of_node, "qcom,ipa-smmu-ap-cb"))
		return ipa_smmu_ap_cb_probe(dev);

	if (of_device_is_compatible(dev->of_node, "qcom,ipa-smmu-wlan-cb"))
		return ipa_smmu_wlan_cb_probe(dev);

	if (of_device_is_compatible(dev->of_node, "qcom,ipa-smmu-uc-cb"))
		return ipa_smmu_uc_cb_probe(dev);

	if (of_device_is_compatible(dev->of_node,
	    "qcom,smp2pgpio-map-ipa-1-in"))
		return ipa3_smp2p_probe(dev);

	if (of_device_is_compatible(dev->of_node,
	    "qcom,smp2pgpio-map-ipa-1-out"))
		return ipa3_smp2p_probe(dev);

	master_dev = dev;
	if (!ipa3_pdev)
		ipa3_pdev = pdev_p;

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

	result = of_platform_populate(pdev_p->dev.of_node,
		pdrv_match, NULL, &pdev_p->dev);
	if (result) {
		IPAERR("failed to populate platform\n");
		return result;
	}

	if (of_property_read_bool(pdev_p->dev.of_node, "qcom,arm-smmu")) {
		if (of_property_read_bool(pdev_p->dev.of_node,
		    "qcom,smmu-s1-bypass"))
			smmu_info.s1_bypass = true;
		if (of_property_read_bool(pdev_p->dev.of_node,
			"qcom,smmu-fast-map"))
			smmu_info.fast_map = true;
		if (of_property_read_bool(pdev_p->dev.of_node,
			"qcom,use-64-bit-dma-mask"))
			smmu_info.use_64_bit_dma_mask = true;
		smmu_info.arm_smmu = true;
		pr_info("IPA smmu_info.s1_bypass=%d smmu_info.fast_map=%d\n",
			smmu_info.s1_bypass, smmu_info.fast_map);
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

		if (!ipa3_bus_scale_table)
			ipa3_bus_scale_table = msm_bus_cl_get_pdata(pdev_p);
		/* Proceed to real initialization */
		result = ipa3_pre_init(&ipa3_res, dev);
		if (result) {
			IPAERR("ipa3_init failed\n");
			return result;
		}
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

	/* release SPS IPA resource without waiting for inactivity timer */
	atomic_set(&ipa3_ctx->transport_pm.eot_activity, 0);
	ipa3_sps_release_resource(NULL);
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

static void ipa_gsi_request_resource(struct work_struct *work)
{
	unsigned long flags;
	int ret;

	/* request IPA clocks */
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	/* mark transport resource as granted */
	spin_lock_irqsave(&ipa3_ctx->transport_pm.lock, flags);
	ipa3_ctx->transport_pm.res_granted = true;

	IPADBG("IPA is ON, calling gsi driver\n");
	ret = gsi_complete_clk_grant(ipa3_ctx->gsi_dev_hdl);
	if (ret != GSI_STATUS_SUCCESS)
		IPAERR("gsi_complete_clk_grant failed %d\n", ret);

	spin_unlock_irqrestore(&ipa3_ctx->transport_pm.lock, flags);
}

void ipa_gsi_req_res_cb(void *user_data, bool *granted)
{
	unsigned long flags;
	struct ipa_active_client_logging_info log_info;

	spin_lock_irqsave(&ipa3_ctx->transport_pm.lock, flags);

	/* make sure no release will happen */
	cancel_delayed_work(&ipa_gsi_release_resource_work);
	ipa3_ctx->transport_pm.res_rel_in_prog = false;

	if (ipa3_ctx->transport_pm.res_granted) {
		*granted = true;
	} else {
		IPA_ACTIVE_CLIENTS_PREP_SPECIAL(log_info, "GSI_RESOURCE");
		if (ipa3_inc_client_enable_clks_no_block(&log_info) == 0) {
			ipa3_ctx->transport_pm.res_granted = true;
			*granted = true;
		} else {
			queue_work(ipa3_ctx->transport_power_mgmt_wq,
				   &ipa_gsi_request_resource_work);
			*granted = false;
		}
	}
	spin_unlock_irqrestore(&ipa3_ctx->transport_pm.lock, flags);
}

static void ipa_gsi_release_resource(struct work_struct *work)
{
	unsigned long flags;
	bool dec_clients = false;

	spin_lock_irqsave(&ipa3_ctx->transport_pm.lock, flags);
	/* check whether still need to decrease client usage */
	if (ipa3_ctx->transport_pm.res_rel_in_prog) {
		dec_clients = true;
		ipa3_ctx->transport_pm.res_rel_in_prog = false;
		ipa3_ctx->transport_pm.res_granted = false;
	}
	spin_unlock_irqrestore(&ipa3_ctx->transport_pm.lock, flags);
	if (dec_clients)
		IPA_ACTIVE_CLIENTS_DEC_SPECIAL("GSI_RESOURCE");
}

int ipa_gsi_rel_res_cb(void *user_data)
{
	unsigned long flags;

	spin_lock_irqsave(&ipa3_ctx->transport_pm.lock, flags);

	ipa3_ctx->transport_pm.res_rel_in_prog = true;
	queue_delayed_work(ipa3_ctx->transport_power_mgmt_wq,
			   &ipa_gsi_release_resource_work,
			   msecs_to_jiffies(IPA_TRANSPORT_PROD_TIMEOUT_MSEC));

	spin_unlock_irqrestore(&ipa3_ctx->transport_pm.lock, flags);
	return 0;
}

static void ipa_gsi_notify_cb(struct gsi_per_notify *notify)
{
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
	struct ipa_smmu_cb_ctx *ap_cb = ipa3_get_smmu_ctx();
	struct ipa_smmu_cb_ctx *uc_cb = ipa3_get_uc_smmu_ctx();

	IPADBG("domain =0x%p iova 0x%lx\n", domain, iova);
	IPADBG("paddr =0x%pa size 0x%x\n", &paddr, (u32)size);

	/* make sure no overlapping */
	if (domain == ipa3_get_smmu_domain()) {
		if (iova >= ap_cb->va_start && iova < ap_cb->va_end) {
			IPAERR("iommu AP overlap addr 0x%lx\n", iova);
			ipa_assert();
			return -EFAULT;
		}
	} else if (domain == ipa3_get_wlan_smmu_domain()) {
		/* wlan is one time map */
	} else if (domain == ipa3_get_uc_smmu_domain()) {
		if (iova >= uc_cb->va_start && iova < uc_cb->va_end) {
			IPAERR("iommu uC overlap addr 0x%lx\n", iova);
			ipa_assert();
			return -EFAULT;
		}
	} else {
		IPAERR("Unexpected domain 0x%p\n", domain);
		ipa_assert();
		return -EFAULT;
	}

	return iommu_map(domain, iova, paddr, size, prot);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA HW device driver");
