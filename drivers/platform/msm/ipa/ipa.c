/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/rbtree.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/netdevice.h>
#include <linux/delay.h>
#include "ipa_i.h"
#include "ipa_rm_i.h"

#define IPA_SUMMING_THRESHOLD (0x10)
#define IPA_PIPE_MEM_START_OFST (0x0)
#define IPA_PIPE_MEM_SIZE (0x0)
#define IPA_MOBILE_AP_MODE(x) (x == IPA_MODE_MOBILE_AP_ETH || \
			       x == IPA_MODE_MOBILE_AP_WAN || \
			       x == IPA_MODE_MOBILE_AP_WLAN)
#define IPA_CNOC_CLK_RATE (75 * 1000 * 1000UL)
#define IPA_A5_MUX_HEADER_LENGTH (8)
#define IPA_DMA_POOL_SIZE (512)
#define IPA_DMA_POOL_ALIGNMENT (4)
#define IPA_DMA_POOL_BOUNDARY (1024)
#define IPA_ROUTING_RULE_BYTE_SIZE (4)
#define IPA_BAM_CNFG_BITS_VALv1_1 (0x7FFFE004)
#define IPA_BAM_CNFG_BITS_VALv2_0 (0xFFFFE004)
#define IPA_STATUS_CLEAR_OFST (0x3f28)
#define IPA_STATUS_CLEAR_SIZE (32)

#define IPA_AGGR_MAX_STR_LENGTH (10)

#define CLEANUP_TAG_PROCESS_TIMEOUT 20

#define IPA_AGGR_STR_IN_BYTES(str) \
	(strnlen((str), IPA_AGGR_MAX_STR_LENGTH - 1) + 1)

#define IPA_Q6_CLEANUP_FLT_RT_MAX_CMDS \
	(IPA_NUM_PIPES*2 + \
	(IPA_v2_V4_MODEM_RT_INDEX_HI - IPA_v2_V4_MODEM_RT_INDEX_LO + 1) + \
	(IPA_v2_V6_MODEM_RT_INDEX_HI - IPA_v2_V6_MODEM_RT_INDEX_LO + 1)) \

/* To be on the safe side */
#define IPA_Q6_CLEANUP_EXP_AGGR_MAX_CMDS \
	(IPA_NUM_PIPES*2) \

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

static struct ipa_plat_drv_res ipa_res = {0, };
static struct of_device_id ipa_plat_drv_match[] = {
	{
		.compatible = "qcom,ipa",
	},

	{
	}
};

static struct clk *ipa_clk_src;
static struct clk *ipa_clk;
static struct clk *sys_noc_ipa_axi_clk;
static struct clk *ipa_cnoc_clk;
static struct clk *ipa_inactivity_clk;

struct ipa_context *ipa_ctx;

static int ipa_open(struct inode *inode, struct file *filp)
{
	struct ipa_context *ctx = NULL;

	IPADBG("ENTER\n");
	ctx = container_of(inode->i_cdev, struct ipa_context, cdev);
	filp->private_data = ctx;

	return 0;
}

static void ipa_wan_msg_free_cb(void *buff, u32 len, u32 type)
{
	if (!buff || (type != WAN_UPSTREAM_ROUTE_ADD
			&& type != WAN_UPSTREAM_ROUTE_DEL)) {
		IPAERR("Null buffer or wrong type give. buff %p type %d\n",
			buff, type);
		return;
	}

	kfree(buff);
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
	struct ipa_wan_msg *wan_msg;
	struct ipa_msg_meta msg_meta;
	size_t sz;

	IPADBG("cmd=%x nr=%d\n", cmd, _IOC_NR(cmd));

	if (_IOC_TYPE(cmd) != IPA_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) >= IPA_IOCTL_MAX)
		return -ENOTTY;

	ipa_inc_client_enable_clks();

	switch (cmd) {
	case IPA_IOC_ALLOC_NAT_MEM:
		if (copy_from_user((u8 *)&nat_mem, (u8 *)arg,
					sizeof(struct ipa_ioc_nat_alloc_mem))) {
			retval = -EFAULT;
			break;
		}
		/* null terminate the string */
		nat_mem.dev_name[IPA_RESOURCE_NAME_MAX - 1] = '\0';

		if (allocate_nat_device(&nat_mem)) {
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
		if (ipa_nat_init_cmd(&nat_init)) {
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

		pyld_sz =
		   sizeof(struct ipa_ioc_nat_dma_cmd) +
		   ((struct ipa_ioc_nat_dma_cmd *)header)->entries *
		   sizeof(struct ipa_ioc_nat_dma_one);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}

		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}

		if (ipa_nat_dma_cmd((struct ipa_ioc_nat_dma_cmd *)param)) {
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
		if (ipa_nat_del_cmd(&nat_del)) {
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
		pyld_sz =
		   sizeof(struct ipa_ioc_add_hdr) +
		   ((struct ipa_ioc_add_hdr *)header)->num_hdrs *
		   sizeof(struct ipa_hdr_add);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa_add_hdr((struct ipa_ioc_add_hdr *)param)) {
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
		pyld_sz =
		   sizeof(struct ipa_ioc_del_hdr) +
		   ((struct ipa_ioc_del_hdr *)header)->num_hdls *
		   sizeof(struct ipa_hdr_del);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa_del_hdr((struct ipa_ioc_del_hdr *)param)) {
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
		pyld_sz =
		   sizeof(struct ipa_ioc_add_rt_rule) +
		   ((struct ipa_ioc_add_rt_rule *)header)->num_rules *
		   sizeof(struct ipa_rt_rule_add);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa_add_rt_rule((struct ipa_ioc_add_rt_rule *)param)) {
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
		pyld_sz =
		   sizeof(struct ipa_ioc_del_rt_rule) +
		   ((struct ipa_ioc_del_rt_rule *)header)->num_hdls *
		   sizeof(struct ipa_rt_rule_del);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa_del_rt_rule((struct ipa_ioc_del_rt_rule *)param)) {
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
		pyld_sz =
		   sizeof(struct ipa_ioc_add_flt_rule) +
		   ((struct ipa_ioc_add_flt_rule *)header)->num_rules *
		   sizeof(struct ipa_flt_rule_add);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa_add_flt_rule((struct ipa_ioc_add_flt_rule *)param)) {
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
		pyld_sz =
		   sizeof(struct ipa_ioc_del_flt_rule) +
		   ((struct ipa_ioc_del_flt_rule *)header)->num_hdls *
		   sizeof(struct ipa_flt_rule_del);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa_del_flt_rule((struct ipa_ioc_del_flt_rule *)param)) {
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
		pyld_sz =
		   sizeof(struct ipa_ioc_mdfy_flt_rule) +
		   ((struct ipa_ioc_mdfy_flt_rule *)header)->num_rules *
		   sizeof(struct ipa_flt_rule_mdfy);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (ipa_mdfy_flt_rule((struct ipa_ioc_mdfy_flt_rule *)param)) {
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case IPA_IOC_COMMIT_HDR:
		retval = ipa_commit_hdr();
		break;
	case IPA_IOC_RESET_HDR:
		retval = ipa_reset_hdr();
		break;
	case IPA_IOC_COMMIT_RT:
		retval = ipa_commit_rt(arg);
		break;
	case IPA_IOC_RESET_RT:
		retval = ipa_reset_rt(arg);
		break;
	case IPA_IOC_COMMIT_FLT:
		retval = ipa_commit_flt(arg);
		break;
	case IPA_IOC_RESET_FLT:
		retval = ipa_reset_flt(arg);
		break;
	case IPA_IOC_GET_RT_TBL:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_get_rt_tbl))) {
			retval = -EFAULT;
			break;
		}
		if (ipa_get_rt_tbl((struct ipa_ioc_get_rt_tbl *)header)) {
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
		retval = ipa_put_rt_tbl(arg);
		break;
	case IPA_IOC_GET_HDR:
		if (copy_from_user(header, (u8 *)arg,
					sizeof(struct ipa_ioc_get_hdr))) {
			retval = -EFAULT;
			break;
		}
		if (ipa_get_hdr((struct ipa_ioc_get_hdr *)header)) {
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
		retval = ipa_put_hdr(arg);
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
		if (ipa_copy_hdr((struct ipa_ioc_copy_hdr *)header)) {
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

		pyld_sz = sz + ((struct ipa_ioc_query_intf_tx_props *)
				header)->num_tx_props *
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

		pyld_sz = sz + ((struct ipa_ioc_query_intf_rx_props *)
				header)->num_rx_props *
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

		pyld_sz = sz + ((struct ipa_ioc_query_intf_ext_props *)
				header)->num_ext_props *
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
		pyld_sz = sizeof(struct ipa_msg_meta) +
		   ((struct ipa_msg_meta *)header)->msg_len;
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
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
		retval = ipa_rm_add_dependency(rm_depend.resource_name,
						rm_depend.depends_on_name);
		break;
	case IPA_IOC_RM_DEL_DEPENDENCY:
		if (copy_from_user((u8 *)&rm_depend, (u8 *)arg,
				sizeof(struct ipa_ioc_rm_dependency))) {
			retval = -EFAULT;
			break;
		}
		retval = ipa_rm_delete_dependency(rm_depend.resource_name,
						rm_depend.depends_on_name);
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
			retval = ipa_get_ep_mapping(arg);
			break;
		}
	case IPA_IOC_QUERY_RT_TBL_INDEX:
		if (copy_from_user(header, (u8 *)arg,
				sizeof(struct ipa_ioc_get_rt_tbl_indx))) {
			retval = -EFAULT;
			break;
		}
		if (ipa_query_rt_index(
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
		if (ipa_write_qmap_id((struct ipa_ioc_write_qmapid *)header)) {
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
		wan_msg = kzalloc(sizeof(struct ipa_wan_msg), GFP_KERNEL);
		if (!wan_msg) {
			IPAERR("no memory\n");
			retval = -ENOMEM;
			break;
		}

		if (copy_from_user((u8 *)wan_msg, (u8 *)arg,
			sizeof(struct ipa_wan_msg))) {
				kfree(wan_msg);
				retval = -EFAULT;
				break;
		}

		memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
		msg_meta.msg_type = WAN_UPSTREAM_ROUTE_ADD;
		msg_meta.msg_len = sizeof(struct ipa_wan_msg);
		retval = ipa_send_msg(&msg_meta, wan_msg, ipa_wan_msg_free_cb);
		if (retval) {
			IPAERR("ipa_send_msg failed: %d\n", retval);
			kfree(wan_msg);
			break;
		}
		break;
	case IPA_IOC_NOTIFY_WAN_UPSTREAM_ROUTE_DEL:
		wan_msg = kzalloc(sizeof(struct ipa_wan_msg), GFP_KERNEL);
		if (!wan_msg) {
			IPAERR("no memory\n");
			retval = -ENOMEM;
			break;
		}

		if (copy_from_user((u8 *)wan_msg, (u8 *)arg,
			sizeof(struct ipa_wan_msg))) {
				kfree(wan_msg);
				retval = -EFAULT;
				break;
		}

		memset(&msg_meta, 0, sizeof(struct ipa_msg_meta));
		msg_meta.msg_type = WAN_UPSTREAM_ROUTE_DEL;
		msg_meta.msg_len = sizeof(struct ipa_wan_msg);
		retval = ipa_send_msg(&msg_meta, wan_msg, ipa_wan_msg_free_cb);
		if (retval) {
			IPAERR("ipa_send_msg failed: %d\n", retval);
			kfree(wan_msg);
			break;
		}
		break;
	default:        /* redundant, as cmd was checked against MAXNR */
		ipa_dec_client_disable_clks();
		return -ENOTTY;
	}
	kfree(param);

	ipa_dec_client_disable_clks();

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

	if (ipa_add_rt_rule(rt_rule)) {
		IPAERR("fail to add dflt v4 rule\n");
		kfree(rt_rule);
		return -EPERM;
	}
	IPADBG("dflt v4 rt rule hdl=%x\n", rt_rule_entry->rt_rule_hdl);
	ipa_ctx->dflt_v4_rt_rule_hdl = rt_rule_entry->rt_rule_hdl;

	/* setup a default v6 route to point to A5 */
	rt_rule->ip = IPA_IP_v6;
	if (ipa_add_rt_rule(rt_rule)) {
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
	} else if (ipa_ctx->ipa_hw_type == IPA_HW_v2_0) {
		strlcpy(hdr_entry->name, IPA_LAN_RX_HDR_NAME,
				IPA_RESOURCE_NAME_MAX);
		hdr_entry->hdr_len = IPA_LAN_RX_HEADER_LENGTH;
	} else {
		WARN_ON(1);
	}

	if (ipa_add_hdr(hdr)) {
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
	route.route_def_pipe = ipa_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS);
	route.route_frag_def_pipe = ipa_get_ep_mapping(
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
	struct ipa_hw_imm_cmd_dma_shared_mem cmd;
	struct ipa_desc desc;
	struct ipa_mem_buffer mem;
	int rc;

	memset(&desc, 0, sizeof(desc));
	memset(&cmd, 0, sizeof(cmd));
	memset(&mem, 0, sizeof(mem));

	mem.size = memory_region_size;
	mem.base = dma_alloc_coherent(ipa_ctx->pdev, mem.size,
		&mem.phys_base, GFP_KERNEL);
	if (!mem.base) {
		IPAERR("failed to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	memset(mem.base, 0, mem.size);
	cmd.size = mem.size;
	cmd.system_addr = mem.phys_base;
	cmd.local_addr = ipa_ctx->smem_restricted_bytes +
		memory_region_offset;
	desc.opcode = IPA_DMA_SHARED_MEM;
	desc.pyld = &cmd;
	desc.len = sizeof(cmd);
	desc.type = IPA_IMM_CMD_DESC;

	rc = ipa_send_cmd(1, &desc);
	if (rc) {
		IPAERR("failed to send immediate command (error %d)\n", rc);
		rc = -EFAULT;
	}

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

	rc = ipa_init_smem_region(IPA_v2_RAM_MODEM_SIZE,
				  IPA_v2_RAM_MODEM_OFST);

	if (rc) {
		IPAERR("failed to initialize Modem RAM memory\n");
		return rc;
	}

	rc = ipa_init_smem_region(IPA_v2_RAM_MODEM_HDR_SIZE,
				  IPA_v2_RAM_MODEM_HDR_OFST);

	if (rc) {
		IPAERR("failed to initialize Modem HDRs RAM memory\n");
		return rc;
	}

	return rc;
}

static void ipa_free_buffer(void *user1, int user2)
{
	kfree(user1);
}

static int ipa_q6_pipe_delay(void)
{
	u32 reg_val = 0;
	int client_idx;
	int ep_idx;

	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		if (IPA_CLIENT_IS_Q6_PROD(client_idx)) {
			ep_idx = ipa_get_ep_mapping(client_idx);
			if (ep_idx == -1)
				continue;

			IPA_SETFIELD_IN_REG(reg_val, 1,
				IPA_ENDP_INIT_CTRL_N_ENDP_DELAY_SHFT,
				IPA_ENDP_INIT_CTRL_N_ENDP_DELAY_BMSK);

			ipa_write_reg(ipa_ctx->mmio,
				IPA_ENDP_INIT_CTRL_N_OFST(ep_idx), reg_val);
		}
	}

	return 0;
}

static int ipa_q6_avoid_holb(void)
{
	u32 reg_val;
	int ep_idx;
	int client_idx;
	struct ipa_ep_cfg_ctrl avoid_holb;

	memset(&avoid_holb, 0, sizeof(avoid_holb));
	avoid_holb.ipa_ep_suspend = true;

	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		if (IPA_CLIENT_IS_Q6_CONS(client_idx)) {
			ep_idx = ipa_get_ep_mapping(client_idx);
			if (ep_idx == -1)
				continue;

			/*
			 * ipa_cfg_ep_holb is not used here because we are
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

			ipa_cfg_ep_ctrl(ep_idx, &avoid_holb);
		}
	}

	return 0;
}

static int ipa_q6_clean_q6_tables(void)
{
	struct ipa_desc *desc;
	struct ipa_hw_imm_cmd_dma_shared_mem *cmd = NULL;
	int client_idx;
	int ep_idx;
	int num_cmds = 0;
	int index;
	int retval;
	struct ipa_mem_buffer mem = { 0 };
	u32 *entry;

	mem.base = dma_alloc_coherent(ipa_ctx->pdev, 4, &mem.phys_base,
		GFP_KERNEL);
	if (!mem.base) {
		IPAERR("failed to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	mem.size = 4;
	entry = mem.base;
	*entry = ipa_ctx->empty_rt_tbl_mem.phys_base;

	desc = kzalloc(sizeof(struct ipa_desc) *
		IPA_Q6_CLEANUP_FLT_RT_MAX_CMDS, GFP_KERNEL);
	if (!desc) {
		IPAERR("failed to allocate memory\n");
		retval = -ENOMEM;
		goto bail_dma;
	}

	cmd = kzalloc(sizeof(struct ipa_hw_imm_cmd_dma_shared_mem) *
		IPA_Q6_CLEANUP_FLT_RT_MAX_CMDS, GFP_KERNEL);
	if (!cmd) {
		IPAERR("failed to allocate memory\n");
		retval = -ENOMEM;
		goto bail_desc;
	}

	/*
	 * Iterating over all the pipes which are either invalid but connected
	 * or connected but not configured by AP.
	 */
	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		ep_idx = ipa_get_ep_mapping(client_idx);
		if (ep_idx == -1)
			continue;

		if (!ipa_ctx->ep[ep_idx].valid ||
		    ipa_ctx->ep[ep_idx].skip_ep_cfg) {
			/*
			 * Need to point v4 and v6 fltr tables to an empty
			 * table
			 */
			cmd[num_cmds].size = mem.size;
			cmd[num_cmds].system_addr = mem.phys_base;
			cmd[num_cmds].local_addr =
				ipa_ctx->smem_restricted_bytes +
				IPA_v2_RAM_V4_FLT_OFST + 8 + ep_idx*4;

			desc[num_cmds].opcode = IPA_DMA_SHARED_MEM;
			desc[num_cmds].pyld = &cmd[num_cmds];
			desc[num_cmds].len = sizeof(*cmd);
			desc[num_cmds].type = IPA_IMM_CMD_DESC;
			num_cmds++;

			cmd[num_cmds].size = mem.size;
			cmd[num_cmds].system_addr =  mem.phys_base;
			cmd[num_cmds].local_addr =
				ipa_ctx->smem_restricted_bytes +
				IPA_v2_RAM_V6_FLT_OFST + 8 + ep_idx*4;

			desc[num_cmds].opcode = IPA_DMA_SHARED_MEM;
			desc[num_cmds].pyld = &cmd[num_cmds];
			desc[num_cmds].len = sizeof(*cmd);
			desc[num_cmds].type = IPA_IMM_CMD_DESC;
			num_cmds++;
		}
	}

	/* Need to point v4/v6 modem routing tables to an empty table */
	for (index = IPA_v2_V4_MODEM_RT_INDEX_LO; index <=
		IPA_v2_V4_MODEM_RT_INDEX_HI; index++) {
		cmd[num_cmds].size = mem.size;
		cmd[num_cmds].system_addr =  mem.phys_base;
		cmd[num_cmds].local_addr = ipa_ctx->smem_restricted_bytes +
			IPA_v2_RAM_V4_RT_OFST + index*4;

		desc[num_cmds].opcode = IPA_DMA_SHARED_MEM;
		desc[num_cmds].pyld = &cmd[num_cmds];
		desc[num_cmds].len = sizeof(*cmd);
		desc[num_cmds].type = IPA_IMM_CMD_DESC;
		num_cmds++;
	}

	for (index = IPA_v2_V6_MODEM_RT_INDEX_LO; index <=
		IPA_v2_V6_MODEM_RT_INDEX_HI; index++) {
		cmd[num_cmds].size = mem.size;
		cmd[num_cmds].system_addr =  mem.phys_base;
		cmd[num_cmds].local_addr = ipa_ctx->smem_restricted_bytes +
			IPA_v2_RAM_V6_RT_OFST + index*4;

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

	desc = kzalloc(sizeof(struct ipa_desc) *
		IPA_Q6_CLEANUP_EXP_AGGR_MAX_CMDS, GFP_KERNEL);
	if (!desc) {
		IPAERR("failed to allocate memory\n");
		return -ENOMEM;
	}

	/* Set the exception path to AP */
	for (client_idx = 0; client_idx < IPA_CLIENT_MAX; client_idx++) {
		ep_idx = ipa_get_ep_mapping(client_idx);
		if (ep_idx == -1)
			continue;

		if (ipa_ctx->ep[ep_idx].valid &&
			ipa_ctx->ep[ep_idx].skip_ep_cfg) {
			reg_write = kzalloc(sizeof(*reg_write), GFP_KERNEL);

			if (!reg_write) {
				IPAERR("failed to allocate memory\n");
				BUG();
			}
			reg_write->skip_pipeline_clear = 0;
			reg_write->offset = IPA_ENDP_STATUS_n_OFST(ep_idx);
			reg_write->value =
				(ipa_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS) &
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
		if (IPA_CLIENT_IS_Q6_CONS(client_idx)) {
			reg_write = kzalloc(sizeof(*reg_write), GFP_KERNEL);

			if (!reg_write) {
				IPAERR("failed to allocate memory\n");
				BUG();
			}

			ipa_q6_disable_agg_reg(reg_write,
					       ipa_get_ep_mapping(client_idx));

			desc[num_descs].opcode = IPA_REGISTER_WRITE;
			desc[num_descs].pyld = reg_write;
			desc[num_descs].len = sizeof(*reg_write);
			desc[num_descs].type = IPA_IMM_CMD_DESC;
			desc[num_descs].callback = ipa_free_buffer;
			desc[num_descs].user1 = reg_write;
			num_descs++;
		}
	}

	/* Will wait 20msecs for IPA tag process completion */
	retval = ipa_tag_process(desc, num_descs,
				 msecs_to_jiffies(CLEANUP_TAG_PROCESS_TIMEOUT));
	if (retval) {
		IPAERR("TAG process failed! (error %d)\n", retval);
		for (index = 0; index < num_descs; index++)
			kfree(desc[index].user1);
		retval = -EINVAL;
	}

	kfree(desc);

	return retval;
}

/**
* ipa_q6_cleanup() - A cleanup for all Q6 related configuration
*                    in IPA HW. This is performed in case of SSR.
*
* Return codes:
* 0: success
* This is a mandatory procedure, in case one of the steps fails, the
* AP needs to restart.
*/
int ipa_q6_cleanup(void)
{
	if (ipa_q6_pipe_delay()) {
		IPAERR("Failed to delay Q6 pipes\n");
		BUG();
	}
	if (ipa_q6_avoid_holb()) {
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

	return 0;
}

static int ipa_init_sram(void)
{
	u32 *ipa_sram_mmio;
	unsigned long phys_addr;
	struct ipa_hw_imm_cmd_dma_shared_mem cmd = {0};
	struct ipa_desc desc = {0};
	struct ipa_mem_buffer mem;
	int rc = 0;

	phys_addr = ipa_ctx->ipa_wrapper_base + IPA_REG_BASE_OFST +
		IPA_SRAM_DIRECT_ACCESS_N_OFST_v2_0(
				ipa_ctx->smem_restricted_bytes / 4);
	ipa_sram_mmio = ioremap(phys_addr,
			ipa_ctx->smem_sz - ipa_ctx->smem_restricted_bytes);
	if (!ipa_sram_mmio) {
		IPAERR("fail to ioremap IPA SRAM\n");
		return -ENOMEM;
	}

#define IPA_SRAM_SET(ofst, val) (ipa_sram_mmio[(ofst - 4) / 4] = val)

	IPA_SRAM_SET(IPA_v2_RAM_V6_FLT_OFST - 4, IPA_CANARY_VAL);
	IPA_SRAM_SET(IPA_v2_RAM_V6_FLT_OFST, IPA_CANARY_VAL);
	IPA_SRAM_SET(IPA_v2_RAM_V4_RT_OFST - 4, IPA_CANARY_VAL);
	IPA_SRAM_SET(IPA_v2_RAM_V4_RT_OFST, IPA_CANARY_VAL);
	IPA_SRAM_SET(IPA_v2_RAM_V6_RT_OFST, IPA_CANARY_VAL);
	IPA_SRAM_SET(IPA_v2_RAM_MODEM_HDR_OFST, IPA_CANARY_VAL);
	IPA_SRAM_SET(IPA_v2_RAM_MODEM_OFST, IPA_CANARY_VAL);
	IPA_SRAM_SET(IPA_v2_RAM_APPS_V4_FLT_OFST, IPA_CANARY_VAL);
	IPA_SRAM_SET(IPA_v2_RAM_END_OFST, IPA_CANARY_VAL);

	iounmap(ipa_sram_mmio);

	mem.size = IPA_STATUS_CLEAR_SIZE;
	mem.base = dma_alloc_coherent(ipa_ctx->pdev, mem.size, &mem.phys_base,
			GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}
	memset(mem.base, 0, mem.size);

	cmd.size = mem.size;
	cmd.system_addr = mem.phys_base;
	cmd.local_addr = IPA_STATUS_CLEAR_OFST;
	desc.opcode = IPA_DMA_SHARED_MEM;
	desc.pyld = &cmd;
	desc.len = sizeof(struct ipa_hw_imm_cmd_dma_shared_mem);
	desc.type = IPA_IMM_CMD_DESC;

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return rc;
}

static int ipa_init_hdr(void)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer mem;
	struct ipa_hdr_init_local cmd;
	int rc = 0;

	mem.size = IPA_v2_RAM_MODEM_HDR_SIZE + IPA_v2_RAM_APPS_HDR_SIZE;
	mem.base = dma_alloc_coherent(ipa_ctx->pdev, mem.size, &mem.phys_base,
			GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}
	memset(mem.base, 0, mem.size);

	cmd.hdr_table_src_addr = mem.phys_base;
	cmd.size_hdr_table = mem.size;
	cmd.hdr_table_dst_addr = ipa_ctx->smem_restricted_bytes +
		IPA_v2_RAM_MODEM_HDR_OFST;

	desc.opcode = IPA_HDR_INIT_LOCAL;
	desc.pyld = &cmd;
	desc.len = sizeof(struct ipa_hdr_init_local);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return rc;
}

static int ipa_init_rt4(void)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer mem;
	struct ipa_ip_v4_routing_init v4_cmd;
	u32 *entry;
	int i;
	int rc = 0;

	for (i = IPA_v2_V4_MODEM_RT_INDEX_LO;
			i <= IPA_v2_V4_MODEM_RT_INDEX_HI; i++)
		ipa_ctx->rt_idx_bitmap[IPA_IP_v4] |= (1 << i);
	IPADBG("v4 rt bitmap 0x%lx\n", ipa_ctx->rt_idx_bitmap[IPA_IP_v4]);

	mem.size = IPA_v2_RAM_V4_RT_SIZE;
	mem.base = dma_alloc_coherent(ipa_ctx->pdev, mem.size, &mem.phys_base,
			GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	entry = mem.base;
	for (i = 0; i < IPA_v2_RAM_V4_NUM_INDEX; i++) {
		*entry = ipa_ctx->empty_rt_tbl_mem.phys_base;
		entry++;
	}

	desc.opcode = IPA_IP_V4_ROUTING_INIT;
	v4_cmd.ipv4_rules_addr = mem.phys_base;
	v4_cmd.size_ipv4_rules = mem.size;
	v4_cmd.ipv4_addr = ipa_ctx->smem_restricted_bytes +
		IPA_v2_RAM_V4_RT_OFST;
	IPADBG("putting Routing IPv4 rules to phys 0x%x",
				v4_cmd.ipv4_addr);

	desc.pyld = &v4_cmd;
	desc.len = sizeof(struct ipa_ip_v4_routing_init);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return rc;
}

static int ipa_init_rt6(void)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer mem;
	struct ipa_ip_v6_routing_init v6_cmd;
	u32 *entry;
	int i;
	int rc = 0;

	for (i = IPA_v2_V6_MODEM_RT_INDEX_LO;
			i <= IPA_v2_V6_MODEM_RT_INDEX_HI; i++)
		ipa_ctx->rt_idx_bitmap[IPA_IP_v6] |= (1 << i);
	IPADBG("v6 rt bitmap 0x%lx\n", ipa_ctx->rt_idx_bitmap[IPA_IP_v6]);

	mem.size = IPA_v2_RAM_V6_RT_SIZE;
	mem.base = dma_alloc_coherent(ipa_ctx->pdev, mem.size, &mem.phys_base,
			GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	entry = mem.base;
	for (i = 0; i < IPA_v2_RAM_V6_NUM_INDEX; i++) {
		*entry = ipa_ctx->empty_rt_tbl_mem.phys_base;
		entry++;
	}

	desc.opcode = IPA_IP_V6_ROUTING_INIT;
	v6_cmd.ipv6_rules_addr = mem.phys_base;
	v6_cmd.size_ipv6_rules = mem.size;
	v6_cmd.ipv6_addr = ipa_ctx->smem_restricted_bytes +
		IPA_v2_RAM_V6_RT_OFST;
	IPADBG("putting Routing IPv6 rules to phys 0x%x",
				v6_cmd.ipv6_addr);

	desc.pyld = &v6_cmd;
	desc.len = sizeof(struct ipa_ip_v6_routing_init);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return rc;
}

static int ipa_init_flt4(void)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer mem;
	struct ipa_ip_v4_filter_init v4_cmd;
	u32 *entry;
	int i;
	int rc = 0;

	mem.size = IPA_v2_RAM_V4_FLT_SIZE;
	mem.base = dma_alloc_coherent(ipa_ctx->pdev, mem.size, &mem.phys_base,
			GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	entry = mem.base;

	*entry = ((0xFFFFF << 1) | 0x1);
	entry++;

	for (i = 0; i <= IPA_NUM_PIPES; i++) {
		*entry = ipa_ctx->empty_rt_tbl_mem.phys_base;
		entry++;
	}

	desc.opcode = IPA_IP_V4_FILTER_INIT;
	v4_cmd.ipv4_rules_addr = mem.phys_base;
	v4_cmd.size_ipv4_rules = mem.size;
	v4_cmd.ipv4_addr = ipa_ctx->smem_restricted_bytes +
		IPA_v2_RAM_V4_FLT_OFST;
	IPADBG("putting Filtering IPv4 rules to phys 0x%x",
				v4_cmd.ipv4_addr);

	desc.pyld = &v4_cmd;
	desc.len = sizeof(struct ipa_ip_v4_filter_init);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

	dma_free_coherent(ipa_ctx->pdev, mem.size, mem.base, mem.phys_base);
	return rc;
}

static int ipa_init_flt6(void)
{
	struct ipa_desc desc = { 0 };
	struct ipa_mem_buffer mem;
	struct ipa_ip_v6_filter_init v6_cmd;
	u32 *entry;
	int i;
	int rc = 0;

	mem.size = IPA_v2_RAM_V6_FLT_SIZE;
	mem.base = dma_alloc_coherent(ipa_ctx->pdev, mem.size, &mem.phys_base,
			GFP_KERNEL);
	if (!mem.base) {
		IPAERR("fail to alloc DMA buff of size %d\n", mem.size);
		return -ENOMEM;
	}

	entry = mem.base;

	*entry = (0xFFFFF << 1) | 0x1;
	entry++;

	for (i = 0; i <= IPA_NUM_PIPES; i++) {
		*entry = ipa_ctx->empty_rt_tbl_mem.phys_base;
		entry++;
	}

	desc.opcode = IPA_IP_V6_FILTER_INIT;
	v6_cmd.ipv6_rules_addr = mem.phys_base;
	v6_cmd.size_ipv6_rules = mem.size;
	v6_cmd.ipv6_addr = ipa_ctx->smem_restricted_bytes +
		IPA_v2_RAM_V6_FLT_OFST;
	IPADBG("putting Filtering IPv6 rules to phys 0x%x",
				v6_cmd.ipv6_addr);

	desc.pyld = &v6_cmd;
	desc.len = sizeof(struct ipa_ip_v6_filter_init);
	desc.type = IPA_IMM_CMD_DESC;
	IPA_DUMP_BUFF(mem.base, mem.phys_base, mem.size);

	if (ipa_send_cmd(1, &desc)) {
		IPAERR("fail to send immediate command\n");
		rc = -EFAULT;
	}

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
	if (ipa_setup_sys_pipe(&sys_in, &ipa_ctx->clnt_hdl_cmd)) {
		IPAERR(":setup sys pipe failed.\n");
		result = -EPERM;
		goto fail_cmd;
	}
	IPADBG("Apps to IPA cmd pipe is connected\n");

	if (ipa_ctx->ipa_hw_type == IPA_HW_v2_0) {
		ipa_init_sram();
		ipa_init_hdr();
		ipa_init_rt4();
		ipa_init_rt6();
		ipa_init_flt4();
		ipa_init_flt6();
	}

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
	} else if (ipa_ctx->ipa_hw_type == IPA_HW_v2_0) {
		sys_in.notify = ipa_lan_rx_cb;
		sys_in.priv = NULL;
		sys_in.ipa_ep_cfg.hdr.hdr_len = IPA_LAN_RX_HEADER_LENGTH;
		sys_in.ipa_ep_cfg.hdr_ext.hdr_little_endian = false;
		sys_in.ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad_valid = true;
		sys_in.ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad = IPA_HDR_PAD;
		sys_in.ipa_ep_cfg.hdr_ext.hdr_payload_len_inc_padding = false;
		sys_in.ipa_ep_cfg.hdr_ext.hdr_total_len_or_pad_offset = 0;
		sys_in.ipa_ep_cfg.hdr_ext.hdr_pad_to_alignment = 2;
	} else {
		WARN_ON(1);
	}
	if (ipa_setup_sys_pipe(&sys_in, &ipa_ctx->clnt_hdl_data_in)) {
		IPAERR(":setup sys pipe failed.\n");
		result = -EPERM;
		goto fail_schedule_delayed_work;
	}

	/* LAN-WAN OUT (A5->IPA) */
	memset(&sys_in, 0, sizeof(struct ipa_sys_connect_params));
	sys_in.client = IPA_CLIENT_APPS_LAN_WAN_PROD;
	sys_in.desc_fifo_sz = IPA_SYS_TX_DATA_DESC_FIFO_SZ;
	sys_in.ipa_ep_cfg.mode.mode = IPA_BASIC;
	if (ipa_setup_sys_pipe(&sys_in, &ipa_ctx->clnt_hdl_data_out)) {
		IPAERR(":setup sys pipe failed.\n");
		result = -EPERM;
		goto fail_data_out;
	}

	return 0;

fail_data_out:
	ipa_teardown_sys_pipe(ipa_ctx->clnt_hdl_data_in);
fail_schedule_delayed_work:
	if (ipa_ctx->dflt_v6_rt_rule_hdl)
		__ipa_del_rt_rule(ipa_ctx->dflt_v6_rt_rule_hdl);
	if (ipa_ctx->dflt_v4_rt_rule_hdl)
		__ipa_del_rt_rule(ipa_ctx->dflt_v4_rt_rule_hdl);
	if (ipa_ctx->excp_hdr_hdl)
		__ipa_del_hdr(ipa_ctx->excp_hdr_hdl);
	ipa_teardown_sys_pipe(ipa_ctx->clnt_hdl_cmd);
fail_cmd:
	return result;
}

static void ipa_teardown_apps_pipes(void)
{
	ipa_teardown_sys_pipe(ipa_ctx->clnt_hdl_data_out);
	ipa_teardown_sys_pipe(ipa_ctx->clnt_hdl_data_in);
	__ipa_del_rt_rule(ipa_ctx->dflt_v6_rt_rule_hdl);
	__ipa_del_rt_rule(ipa_ctx->dflt_v4_rt_rule_hdl);
	__ipa_del_hdr(ipa_ctx->excp_hdr_hdl);
	ipa_teardown_sys_pipe(ipa_ctx->clnt_hdl_cmd);
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

		if (allocate_nat_device(&nat_mem)) {
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
	if (ipa_ctx->ipa_hw_mode != IPA_HW_MODE_NORMAL)
		return 0;

	ipa_clk = clk_get(dev, "core_clk");
	if (IS_ERR(ipa_clk)) {
		ipa_clk = NULL;
		IPAERR("fail to get ipa clk\n");
		return -ENODEV;
	}

	if (ipa_ctx->ipa_hw_type != IPA_HW_v2_0) {
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
	} else {
		WARN_ON(1);
	}
}

void _ipa_enable_clks_v1(void)
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

/**
* ipa_enable_clks() - Turn on IPA clocks
*
* Return codes:
* None
*/
void ipa_enable_clks(void)
{
	IPADBG("enabling IPA clocks and bus voting\n");

	if (ipa_ctx->ipa_hw_mode != IPA_HW_MODE_NORMAL)
		return;

	ipa_ctx->ctrl->ipa_enable_clks();

	if (msm_bus_scale_client_update_request(ipa_ctx->ipa_bus_hdl, 1))
		WARN_ON(1);
}

void _ipa_disable_clks_v1(void)
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
	if (ipa_clk)
		clk_disable_unprepare(ipa_clk);
	else
		WARN_ON(1);
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

	if (ipa_ctx->ipa_hw_mode != IPA_HW_MODE_NORMAL)
			return;

	ipa_ctx->ctrl->ipa_disable_clks();

	if (msm_bus_scale_client_update_request(ipa_ctx->ipa_bus_hdl, 0))
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
	if (res) {
		IPAERR("ipa_tag_aggr_force_close failed %d\n", res);
		return;
	}

	ipa_dec_client_disable_clks();

	IPADBG("TAG process done\n");
	return;
}

/**
* ipa_inc_client_enable_clks() - Increase active clients counter, and
* enable ipa clocks if necessary
*
* Return codes:
* None
*/
void ipa_inc_client_enable_clks(void)
{
	ipa_active_clients_lock();
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
* Return codes: 0 for success
*		-EPERM if an asynchronous action should have been done
*/
int ipa_inc_client_enable_clks_no_block(void)
{
	int res = 0;

	if (ipa_active_clients_trylock() == 0)
		return -EPERM;

	if (ipa_ctx->ipa_active_clients.cnt == 0) {
		res = -EPERM;
		goto bail;
	}

	ipa_ctx->ipa_active_clients.cnt++;
	IPADBG("active clients = %d\n", ipa_ctx->ipa_active_clients.cnt);
bail:
	ipa_active_clients_unlock();

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
 * Return codes:
 * None
 */
void ipa_dec_client_disable_clks(void)
{
	ipa_active_clients_lock();
	ipa_ctx->ipa_active_clients.cnt--;
	IPADBG("active clients = %d\n", ipa_ctx->ipa_active_clients.cnt);
	if (ipa_ctx->ipa_active_clients.cnt == 0) {
		if (ipa_ctx->tag_process_before_gating) {
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
	case IPA_HW_v1_0:
	case IPA_HW_v1_1:
		reg_val = IPA_BAM_CNFG_BITS_VALv1_1;
		break;
	case IPA_HW_v2_0:
		reg_val = IPA_BAM_CNFG_BITS_VALv2_0;
		break;
	default:
		retval = -EPERM;
		goto fail;
	}

	ipa_write_reg(ipa_bam_mmio, IPA_BAM_CNFG_BITS_OFST, reg_val);

fail:
	iounmap(ipa_bam_mmio);

	return retval;
}

int ipa_set_required_perf_profile(enum ipa_voltage_level floor_voltage,
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
		if (bandwidth_mbps > ipa_ctx->ctrl->clock_scaling_bw_threshold)
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
		clk_rate = ipa_ctx->ctrl->ipa_clk_rate_lo;
		break;
	case IPA_VOLTAGE_NOMINAL:
		clk_rate = ipa_ctx->ctrl->ipa_clk_rate_hi;
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
	if (ipa_ctx->ipa_active_clients.cnt > 0)
		clk_set_rate(ipa_clk, ipa_ctx->curr_ipa_clk_rate);
	else
		IPADBG("clocks are gated, not setting rate\n");
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
			ipa_get_rt_tbl(&rt_lookup);
			ipa_put_rt_tbl(rt_lookup.hdl);

			memset(rules, 0, sz);
			rule = &rules->rules[0];
			rules->commit = 1;
			rules->ip = ip;
			rules->global = 1;
			rules->num_rules = 1;
			rule->at_rear = 1;
			if (ip == IPA_IP_v4) {
				rule->rule.attrib.attrib_mask =
					IPA_FLT_PROTOCOL;
				rule->rule.attrib.u.v4.protocol =
				   IPA_INVALID_L4_PROTOCOL;
			} else if (ip == IPA_IP_v6) {
				rule->rule.attrib.attrib_mask =
					IPA_FLT_NEXT_HDR;
				rule->rule.attrib.u.v6.next_hdr =
				   IPA_INVALID_L4_PROTOCOL;
			} else {
				result = -EINVAL;
				WARN_ON(1);
				break;
			}
			rule->rule.action = IPA_PASS_TO_ROUTING;
			rule->rule.rt_tbl_hdl = rt_lookup.hdl;
			rule->rule.retain_hdr = true;

			if (ipa_add_flt_rule(rules) || rules->rules[0].status) {
				result = -EINVAL;
				WARN_ON(1);
				break;
			}
		}
		kfree(rules);
	}
	return result;
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

	IPADBG("interrupt=%d, interrupt_data=%u\n", interrupt, suspend_data);
	for (i = 0; i < IPA_NUM_PIPES; i++) {
		if ((suspend_data & bmsk) && (ipa_ctx->ep[i].valid)) {
			resource = ipa_get_rm_resource_from_ep(i);
			ipa_rm_request_resource_with_timer(resource);
		}
		bmsk = bmsk << 1;
	}
}

static int apps_cons_release_resource(void)
{
	return 0;
}

static int apps_cons_request_resource(void)
{
	return 0;
}

static int ipa_create_apps_resource(void)
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
* @ipa_dev:	The basic device structure representing the IPA driver
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

	IPADBG("IPA Driver initialization started\n");

	ipa_ctx = kzalloc(sizeof(*ipa_ctx), GFP_KERNEL);
	if (!ipa_ctx) {
		IPAERR(":kzalloc err.\n");
		result = -ENOMEM;
		goto fail_mem_ctx;
	}

	ipa_ctx->pdev = ipa_dev;
	ipa_ctx->ipa_wrapper_base = resource_p->ipa_mem_base;
	ipa_ctx->ipa_hw_type = resource_p->ipa_hw_type;
	ipa_ctx->ipa_hw_mode = resource_p->ipa_hw_mode;
	ipa_ctx->use_ipa_teth_bridge = resource_p->use_ipa_teth_bridge;

	/* default aggregation parameters */
	ipa_ctx->aggregation_type = IPA_MBIM_16;
	ipa_ctx->aggregation_byte_limit = 1;
	ipa_ctx->aggregation_time_limit = 0;

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

	/* get BUS handle */
	ipa_ctx->ipa_bus_hdl =
		msm_bus_scale_register_client(ipa_ctx->ctrl->msm_bus_data_ptr);
	if (!ipa_ctx->ipa_bus_hdl) {
		IPAERR("fail to register with bus mgr!\n");
		result = -ENODEV;
		goto fail_bind;
	}

	/* get IPA clocks */
	if (ipa_get_clks(ipa_dev) != 0) {
		IPAERR(":fail to get clk handle's!\n");
		result = -ENODEV;
		goto fail_bind;
	}

	/* Enable ipa_ctx->enable_clock_scaling */
	ipa_ctx->enable_clock_scaling = 1;
	ipa_ctx->curr_ipa_clk_rate = ipa_ctx->ctrl->ipa_clk_rate_hi;

	/* enable IPA clocks explicitly to allow the initialization */
	ipa_enable_clks();

	/* setup IPA register access */
	ipa_ctx->mmio = ioremap(resource_p->ipa_mem_base + IPA_REG_BASE_OFST,
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

	/* register IPA with SPS driver */
	bam_props.phys_addr = resource_p->bam_mem_base;
	bam_props.virt_size = resource_p->bam_mem_size;
	bam_props.irq = resource_p->bam_irq;
	bam_props.num_pipes = IPA_NUM_PIPES;
	bam_props.summing_threshold = IPA_SUMMING_THRESHOLD;
	bam_props.event_threshold = IPA_EVENT_THRESHOLD;
	bam_props.options |= SPS_BAM_NO_LOCAL_CLK_GATING;
	bam_props.ee = resource_p->ee;

	result = sps_register_bam_device(&bam_props, &ipa_ctx->bam_handle);
	if (result) {
		IPAERR(":bam register err.\n");
		result = -EPROBE_DEFER;
		goto fail_init_hw;
	}
	IPADBG("IPA BAM is registered\n");

	if (ipa_setup_bam_cfg(resource_p)) {
		IPAERR(":bam cfg err.\n");
		result = -ENODEV;
		goto fail_flt_rule_cache;
	}

	/* init the lookaside cache */
	ipa_ctx->flt_rule_cache = kmem_cache_create("IPA FLT",
			sizeof(struct ipa_flt_entry), 0, 0, NULL);
	if (!ipa_ctx->flt_rule_cache) {
		IPAERR(":ipa flt cache create failed\n");
		result = -ENOMEM;
		goto fail_flt_rule_cache;
	}
	ipa_ctx->rt_rule_cache = kmem_cache_create("IPA RT",
			sizeof(struct ipa_rt_entry), 0, 0, NULL);
	if (!ipa_ctx->rt_rule_cache) {
		IPAERR(":ipa rt cache create failed\n");
		result = -ENOMEM;
		goto fail_rt_rule_cache;
	}
	ipa_ctx->hdr_cache = kmem_cache_create("IPA HDR",
			sizeof(struct ipa_hdr_entry), 0, 0, NULL);
	if (!ipa_ctx->hdr_cache) {
		IPAERR(":ipa hdr cache create failed\n");
		result = -ENOMEM;
		goto fail_hdr_cache;
	}
	ipa_ctx->hdr_offset_cache =
	   kmem_cache_create("IPA HDR OFFSET",
			   sizeof(struct ipa_hdr_offset_entry), 0, 0, NULL);
	if (!ipa_ctx->hdr_offset_cache) {
		IPAERR(":ipa hdr off cache create failed\n");
		result = -ENOMEM;
		goto fail_hdr_offset_cache;
	}
	ipa_ctx->rt_tbl_cache = kmem_cache_create("IPA RT TBL",
			sizeof(struct ipa_rt_tbl), 0, 0, NULL);
	if (!ipa_ctx->rt_tbl_cache) {
		IPAERR(":ipa rt tbl cache create failed\n");
		result = -ENOMEM;
		goto fail_rt_tbl_cache;
	}
	ipa_ctx->tx_pkt_wrapper_cache =
	   kmem_cache_create("IPA TX PKT WRAPPER",
			   sizeof(struct ipa_tx_pkt_wrapper), 0, 0, NULL);
	if (!ipa_ctx->tx_pkt_wrapper_cache) {
		IPAERR(":ipa tx pkt wrapper cache create failed\n");
		result = -ENOMEM;
		goto fail_tx_pkt_wrapper_cache;
	}
	ipa_ctx->rx_pkt_wrapper_cache =
	   kmem_cache_create("IPA RX PKT WRAPPER",
			   sizeof(struct ipa_rx_pkt_wrapper), 0, 0, NULL);
	if (!ipa_ctx->rx_pkt_wrapper_cache) {
		IPAERR(":ipa rx pkt wrapper cache create failed\n");
		result = -ENOMEM;
		goto fail_rx_pkt_wrapper_cache;
	}
	/*
	 * setup DMA pool 4 byte aligned, don't cross 1k boundaries, nominal
	 * size 512 bytes
	 * This is an issue with IPA HW v1.0 only.
	 */
	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_0) {
		ipa_ctx->dma_pool = dma_pool_create("ipa_1k",
				ipa_ctx->pdev,
				IPA_DMA_POOL_SIZE, IPA_DMA_POOL_ALIGNMENT,
				IPA_DMA_POOL_BOUNDARY);
	} else {
		ipa_ctx->dma_pool = dma_pool_create("ipa_tx", ipa_ctx->pdev,
			IPA_NUM_DESC_PER_SW_TX * sizeof(struct sps_iovec),
			0, 0);
	}
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
	INIT_LIST_HEAD(&ipa_ctx->rt_tbl_set[IPA_IP_v4].head_rt_tbl_list);
	INIT_LIST_HEAD(&ipa_ctx->rt_tbl_set[IPA_IP_v6].head_rt_tbl_list);
	for (i = 0; i < IPA_NUM_PIPES; i++) {
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

	mutex_init(&ipa_ctx->ipa_active_clients.mutex);
	spin_lock_init(&ipa_ctx->ipa_active_clients.spinlock);
	ipa_ctx->ipa_active_clients.cnt = 1;

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
	IPADBG("ipa cdev added successful. major:%d minor:%d",
			MAJOR(ipa_ctx->dev_num),
			MINOR(ipa_ctx->dev_num));

	/* Create workqueue for power management */
	ipa_ctx->power_mgmt_wq =
		create_singlethread_workqueue("ipa_power_mgmt");
	if (!ipa_ctx->power_mgmt_wq) {
		IPAERR("failed to create wq\n");
		result = -ENOMEM;
		goto fail_power_mgmt_wq;
	}

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
			ipa_dev);
	if (result) {
		IPAERR("ipa interrupts initialization failed\n");
		result = -ENODEV;
		goto fail_ipa_interrupts_init;
	}

	/*add handler for suspend interrupt*/
	result = ipa_add_interrupt_handler(IPA_TX_SUSPEND_IRQ,
			ipa_suspend_handler, true, NULL);
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
	result = ipa_wdi_init();
	if (result)
		IPAERR(":wdi init failed (%d)\n", -result);
	else
		IPADBG(":wdi init ok\n");

	ipa_dec_client_disable_clks();

	pr_info("IPA driver initialization was successful.\n");

	return 0;

fail_add_interrupt_handler:
	free_irq(resource_p->ipa_irq, ipa_dev);
fail_ipa_interrupts_init:
	ipa_rm_delete_resource(IPA_RM_RESOURCE_APPS_CONS);
fail_create_apps_resource:
	ipa_rm_exit();
fail_ipa_rm_init:
	destroy_workqueue(ipa_ctx->power_mgmt_wq);
fail_power_mgmt_wq:
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
	/*
	 * DMA pool need to be released only for IPA HW v1.0 only.
	 */
	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_0)
		dma_pool_destroy(ipa_ctx->dma_pool);
fail_dma_pool:
	kmem_cache_destroy(ipa_ctx->rx_pkt_wrapper_cache);
fail_rx_pkt_wrapper_cache:
	kmem_cache_destroy(ipa_ctx->tx_pkt_wrapper_cache);
fail_tx_pkt_wrapper_cache:
	kmem_cache_destroy(ipa_ctx->rt_tbl_cache);
fail_rt_tbl_cache:
	kmem_cache_destroy(ipa_ctx->hdr_offset_cache);
fail_hdr_offset_cache:
	kmem_cache_destroy(ipa_ctx->hdr_cache);
fail_hdr_cache:
	kmem_cache_destroy(ipa_ctx->rt_rule_cache);
fail_rt_rule_cache:
	kmem_cache_destroy(ipa_ctx->flt_rule_cache);
fail_flt_rule_cache:
	sps_deregister_bam_device(ipa_ctx->bam_handle);
fail_init_hw:
	iounmap(ipa_ctx->mmio);
fail_remap:
	ipa_disable_clks();
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

	ipa_drv_res->use_ipa_teth_bridge =
			of_property_read_bool(pdev->dev.of_node,
			"qcom,use-ipa-tethering-bridge");
	IPADBG(": using TBDr = %s",
		ipa_drv_res->use_ipa_teth_bridge
		? "True" : "False");

	/* Get IPA wrapper address */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"ipa-base");
	if (!resource) {
		IPAERR(":get resource failed for ipa-base!\n");
		return -ENODEV;
	} else {
		ipa_drv_res->ipa_mem_base = resource->start;
		ipa_drv_res->ipa_mem_size = resource_size(resource);
		IPADBG(": ipa-base = 0x%x, size = 0x%x\n",
				ipa_drv_res->ipa_mem_base,
				ipa_drv_res->ipa_mem_size);
	}

	/* Get IPA BAM address */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"bam-base");
	if (!resource) {
		IPAERR(":get resource failed for bam-base!\n");
		return -ENODEV;
	} else {
		ipa_drv_res->bam_mem_base = resource->start;
		ipa_drv_res->bam_mem_size = resource_size(resource);
		IPADBG(": bam-base = 0x%x, size = 0x%x\n",
				ipa_drv_res->bam_mem_base,
				ipa_drv_res->bam_mem_size);
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
	} else {
		ipa_drv_res->ipa_irq = resource->start;
		IPADBG(":ipa-irq = %d\n", ipa_drv_res->ipa_irq);
	}

	/* Get IPA BAM IRQ number */
	resource = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			"bam-irq");
	if (!resource) {
		IPAERR(":get resource failed for bam-irq!\n");
		return -ENODEV;
	} else {
		ipa_drv_res->bam_irq = resource->start;
		IPADBG(":ibam-irq = %d\n", ipa_drv_res->bam_irq);
	}

	result = of_property_read_u32(pdev->dev.of_node, "qcom,ee",
			&ipa_drv_res->ee);
	if (result)
		ipa_drv_res->ee = 0;

	return 0;
}

static int ipa_plat_drv_probe(struct platform_device *pdev_p)
{
	int result;

	IPADBG("IPA driver probing started\n");

	 result = get_ipa_dts_configuration(pdev_p, &ipa_res);
	 if (result) {
		IPAERR("IPA dts parsing failed\n");
		return result;
	}

	if (dma_set_mask(&pdev_p->dev, DMA_BIT_MASK(32)) ||
		    dma_set_coherent_mask(&pdev_p->dev, DMA_BIT_MASK(32))) {
		IPAERR("DMA set mask failed\n");
		return -EOPNOTSUPP;
	}

	/* Proceed to real initialization */
	result = ipa_init(&ipa_res, &pdev_p->dev);
	if (result) {
		IPAERR("ipa_init failed\n");
		return result;
	}

	return result;
}

static struct platform_driver ipa_plat_drv = {
	.probe = ipa_plat_drv_probe,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = ipa_plat_drv_match,
	},
};

struct ipa_context *ipa_get_ctx(void)
{
	return ipa_ctx;
}

static int __init ipa_module_init(void)
{
	IPADBG("IPA module init\n");

	/* Register as a platform device driver */
	return platform_driver_register(&ipa_plat_drv);
}
subsys_initcall(ipa_module_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA HW device driver");

