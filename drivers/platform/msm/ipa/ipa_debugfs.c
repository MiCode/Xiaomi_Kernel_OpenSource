/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#ifdef CONFIG_DEBUG_FS

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/stringify.h>
#include "ipa_i.h"
#include "ipa_rm_i.h"

#define IPA_MAX_MSG_LEN 4096
#define IPA_DBG_CNTR_ON 127265
#define IPA_DBG_CNTR_OFF 127264

const char *ipa_excp_name[] = {
	__stringify_1(IPA_A5_MUX_HDR_EXCP_RSVD0),
	__stringify_1(IPA_A5_MUX_HDR_EXCP_RSVD1),
	__stringify_1(IPA_A5_MUX_HDR_EXCP_FLAG_IHL),
	__stringify_1(IPA_A5_MUX_HDR_EXCP_FLAG_REPLICATED),
	__stringify_1(IPA_A5_MUX_HDR_EXCP_FLAG_TAG),
	__stringify_1(IPA_A5_MUX_HDR_EXCP_FLAG_SW_FLT),
	__stringify_1(IPA_A5_MUX_HDR_EXCP_FLAG_NAT),
	__stringify_1(IPA_A5_MUX_HDR_EXCP_FLAG_IP),
};

const char *ipa_status_excp_name[] = {
	__stringify_1(IPA_EXCP_DEAGGR),
	__stringify_1(IPA_EXCP_REPLICATION),
	__stringify_1(IPA_EXCP_IP),
	__stringify_1(IPA_EXCP_IHL),
	__stringify_1(IPA_EXCP_FRAG_MISS),
	__stringify_1(IPA_EXCP_SW),
	__stringify_1(IPA_EXCP_NAT),
	__stringify_1(IPA_EXCP_NONE),
};

const char *ipa_event_name[] = {
	__stringify(WLAN_CLIENT_CONNECT),
	__stringify(WLAN_CLIENT_DISCONNECT),
	__stringify(WLAN_CLIENT_POWER_SAVE_MODE),
	__stringify(WLAN_CLIENT_NORMAL_MODE),
	__stringify(SW_ROUTING_ENABLE),
	__stringify(SW_ROUTING_DISABLE),
	__stringify(WLAN_AP_CONNECT),
	__stringify(WLAN_AP_DISCONNECT),
	__stringify(WLAN_STA_CONNECT),
	__stringify(WLAN_STA_DISCONNECT),
	__stringify(WLAN_CLIENT_CONNECT_EX),
	__stringify(WLAN_SWITCH_TO_SCC),
	__stringify(WLAN_SWITCH_TO_MCC),
	__stringify(WLAN_WDI_ENABLE),
	__stringify(WLAN_WDI_DISABLE),
	__stringify(WAN_UPSTREAM_ROUTE_ADD),
	__stringify(WAN_UPSTREAM_ROUTE_DEL),
	__stringify(WAN_EMBMS_CONNECT),
	__stringify(ECM_CONNECT),
	__stringify(ECM_DISCONNECT),
	__stringify(IPA_TETHERING_STATS_UPDATE_STATS),
	__stringify(IPA_TETHERING_STATS_UPDATE_NETWORK_STATS),
};

const char *ipa_hdr_l2_type_name[] = {
	__stringify(IPA_HDR_L2_NONE),
	__stringify(IPA_HDR_L2_ETHERNET_II),
	__stringify(IPA_HDR_L2_802_3),
};

const char *ipa_hdr_proc_type_name[] = {
	__stringify(IPA_HDR_PROC_NONE),
	__stringify(IPA_HDR_PROC_ETHII_TO_ETHII),
	__stringify(IPA_HDR_PROC_ETHII_TO_802_3),
	__stringify(IPA_HDR_PROC_802_3_TO_ETHII),
	__stringify(IPA_HDR_PROC_802_3_TO_802_3),
};

static struct dentry *dent;
static struct dentry *dfile_gen_reg;
static struct dentry *dfile_ep_reg;
static struct dentry *dfile_keep_awake;
static struct dentry *dfile_ep_holb;
static struct dentry *dfile_hdr;
static struct dentry *dfile_proc_ctx;
static struct dentry *dfile_ip4_rt;
static struct dentry *dfile_ip6_rt;
static struct dentry *dfile_ip4_flt;
static struct dentry *dfile_ip6_flt;
static struct dentry *dfile_stats;
static struct dentry *dfile_wstats;
static struct dentry *dfile_wdi_stats;
static struct dentry *dfile_dbg_cnt;
static struct dentry *dfile_msg;
static struct dentry *dfile_ip4_nat;
static struct dentry *dfile_rm_stats;
static char dbg_buff[IPA_MAX_MSG_LEN];
static s8 ep_reg_idx;

int _ipa_read_gen_reg_v1_1(char *buff, int max_len)
{
	return scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
			"IPA_VERSION=0x%x\n"
			"IPA_COMP_HW_VERSION=0x%x\n"
			"IPA_ROUTE=0x%x\n"
			"IPA_FILTER=0x%x\n"
			"IPA_SHARED_MEM_SIZE=0x%x\n",
			ipa_read_reg(ipa_ctx->mmio, IPA_VERSION_OFST),
			ipa_read_reg(ipa_ctx->mmio, IPA_COMP_HW_VERSION_OFST),
			ipa_read_reg(ipa_ctx->mmio, IPA_ROUTE_OFST_v1_1),
			ipa_read_reg(ipa_ctx->mmio, IPA_FILTER_OFST_v1_1),
			ipa_read_reg(ipa_ctx->mmio,
					IPA_SHARED_MEM_SIZE_OFST_v1_1));
}

int _ipa_read_gen_reg_v2_0(char *buff, int max_len)
{
	return scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
			"IPA_VERSION=0x%x\n"
			"IPA_COMP_HW_VERSION=0x%x\n"
			"IPA_ROUTE=0x%x\n"
			"IPA_FILTER=0x%x\n"
			"IPA_SHARED_MEM_RESTRICTED=0x%x\n"
			"IPA_SHARED_MEM_SIZE=0x%x\n",
			ipa_read_reg(ipa_ctx->mmio, IPA_VERSION_OFST),
			ipa_read_reg(ipa_ctx->mmio, IPA_COMP_HW_VERSION_OFST),
			ipa_read_reg(ipa_ctx->mmio, IPA_ROUTE_OFST_v1_1),
			ipa_read_reg(ipa_ctx->mmio, IPA_FILTER_OFST_v1_1),
			ipa_read_reg_field(ipa_ctx->mmio,
				IPA_SHARED_MEM_SIZE_OFST_v2_0,
				IPA_SHARED_MEM_SIZE_SHARED_MEM_BADDR_BMSK_v2_0,
				IPA_SHARED_MEM_SIZE_SHARED_MEM_BADDR_SHFT_v2_0),
			ipa_read_reg_field(ipa_ctx->mmio,
				IPA_SHARED_MEM_SIZE_OFST_v2_0,
				IPA_SHARED_MEM_SIZE_SHARED_MEM_SIZE_BMSK_v2_0,
				IPA_SHARED_MEM_SIZE_SHARED_MEM_SIZE_SHFT_v2_0));
}

static ssize_t ipa_read_gen_reg(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes;

	ipa_inc_client_enable_clks();
	nbytes = ipa_ctx->ctrl->ipa_read_gen_reg(dbg_buff, IPA_MAX_MSG_LEN);
	ipa_dec_client_disable_clks();

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa_write_ep_holb(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct ipa_ep_cfg_holb holb;
	u32 en;
	u32 tmr_val;
	u32 ep_idx;
	unsigned long missing;
	char *sptr, *token;

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, buf, count);
	if (missing)
		return -EFAULT;

	dbg_buff[count] = '\0';

	sptr = dbg_buff;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &ep_idx))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &en))
		return -EINVAL;

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &tmr_val))
		return -EINVAL;

	holb.en = en;
	holb.tmr_val = tmr_val;

	ipa_cfg_ep_holb(ep_idx, &holb);

	return count;
}

static ssize_t ipa_write_ep_reg(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	unsigned long missing;
	s8 option = 0;

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, buf, count);
	if (missing)
		return -EFAULT;

	dbg_buff[count] = '\0';
	if (kstrtos8(dbg_buff, 0, &option))
		return -EFAULT;

	if (option >= IPA_NUM_PIPES) {
		IPAERR("bad pipe specified %u\n", option);
		return count;
	}

	ep_reg_idx = option;

	return count;
}

int _ipa_read_ep_reg_v1_1(char *buf, int max_len, int pipe)
{
	return scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
			"IPA_ENDP_INIT_NAT_%u=0x%x\n"
			"IPA_ENDP_INIT_HDR_%u=0x%x\n"
			"IPA_ENDP_INIT_MODE_%u=0x%x\n"
			"IPA_ENDP_INIT_AGGR_%u=0x%x\n"
			"IPA_ENDP_INIT_ROUTE_%u=0x%x\n"
			"IPA_ENDP_INIT_CTRL_%u=0x%x\n"
			"IPA_ENDP_INIT_HOL_EN_%u=0x%x\n"
			"IPA_ENDP_INIT_HOL_TIMER_%u=0x%x\n",
			pipe, ipa_read_reg(ipa_ctx->mmio,
				IPA_ENDP_INIT_NAT_N_OFST_v1_1(pipe)),
			pipe, ipa_read_reg(ipa_ctx->mmio,
				IPA_ENDP_INIT_HDR_N_OFST_v1_1(pipe)),
			pipe, ipa_read_reg(ipa_ctx->mmio,
				IPA_ENDP_INIT_MODE_N_OFST_v1_1(pipe)),
			pipe, ipa_read_reg(ipa_ctx->mmio,
				IPA_ENDP_INIT_AGGR_N_OFST_v1_1(pipe)),
			pipe, ipa_read_reg(ipa_ctx->mmio,
				IPA_ENDP_INIT_ROUTE_N_OFST_v1_1(pipe)),
			pipe, ipa_read_reg(ipa_ctx->mmio,
				IPA_ENDP_INIT_CTRL_N_OFST(pipe)),
			pipe, ipa_read_reg(ipa_ctx->mmio,
				IPA_ENDP_INIT_HOL_BLOCK_EN_N_OFST_v1_1(pipe)),
			pipe, ipa_read_reg(ipa_ctx->mmio,
				IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_OFST_v1_1(pipe))
				);
}

int _ipa_read_ep_reg_v2_0(char *buf, int max_len, int pipe)
{
	return scnprintf(
		dbg_buff, IPA_MAX_MSG_LEN,
		"IPA_ENDP_INIT_NAT_%u=0x%x\n"
		"IPA_ENDP_INIT_HDR_%u=0x%x\n"
		"IPA_ENDP_INIT_HDR_EXT_%u=0x%x\n"
		"IPA_ENDP_INIT_MODE_%u=0x%x\n"
		"IPA_ENDP_INIT_AGGR_%u=0x%x\n"
		"IPA_ENDP_INIT_ROUTE_%u=0x%x\n"
		"IPA_ENDP_INIT_CTRL_%u=0x%x\n"
		"IPA_ENDP_INIT_HOL_EN_%u=0x%x\n"
		"IPA_ENDP_INIT_HOL_TIMER_%u=0x%x\n"
		"IPA_ENDP_INIT_DEAGGR_%u=0x%x\n"
		"IPA_ENDP_INIT_CFG_%u=0x%x\n",
		pipe, ipa_read_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_NAT_N_OFST_v2_0(pipe)),
		pipe, ipa_read_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HDR_N_OFST_v2_0(pipe)),
		pipe, ipa_read_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HDR_EXT_n_OFST_v2_0(pipe)),
		pipe, ipa_read_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_MODE_N_OFST_v2_0(pipe)),
		pipe, ipa_read_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_AGGR_N_OFST_v2_0(pipe)),
		pipe, ipa_read_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_ROUTE_N_OFST_v2_0(pipe)),
		pipe, ipa_read_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_CTRL_N_OFST(pipe)),
		pipe, ipa_read_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HOL_BLOCK_EN_N_OFST_v2_0(pipe)),
		pipe, ipa_read_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_OFST_v2_0(pipe)),
		pipe, ipa_read_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_DEAGGR_n_OFST_v2_0(pipe)),
		pipe, ipa_read_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_CFG_n_OFST(pipe)));
}

static ssize_t ipa_read_ep_reg(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes;
	int i;
	int start_idx;
	int end_idx;
	int size = 0;
	int ret;
	loff_t pos;

	/* negative ep_reg_idx means all registers */
	if (ep_reg_idx < 0) {
		start_idx = 0;
		end_idx = IPA_NUM_PIPES;
	} else {
		start_idx = ep_reg_idx;
		end_idx = start_idx + 1;
	}
	pos = *ppos;
	ipa_inc_client_enable_clks();
	for (i = start_idx; i < end_idx; i++) {

		nbytes = ipa_ctx->ctrl->ipa_read_ep_reg(dbg_buff,
				IPA_MAX_MSG_LEN, i);

		*ppos = pos;
		ret = simple_read_from_buffer(ubuf, count, ppos, dbg_buff,
					      nbytes);
		if (ret < 0) {
			ipa_dec_client_disable_clks();
			return ret;
		}

		size += ret;
		ubuf += nbytes;
		count -= nbytes;
	}
	ipa_dec_client_disable_clks();

	*ppos = pos + size;
	return size;
}

static ssize_t ipa_write_keep_awake(struct file *file, const char __user *buf,
	size_t count, loff_t *ppos)
{
	unsigned long missing;
	s8 option = 0;

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, buf, count);
	if (missing)
		return -EFAULT;

	dbg_buff[count] = '\0';
	if (kstrtos8(dbg_buff, 0, &option))
		return -EFAULT;

	if (option == 1)
		ipa_inc_client_enable_clks();
	else if (option == 0)
		ipa_dec_client_disable_clks();
	else
		return -EFAULT;

	return count;
}

static ssize_t ipa_read_keep_awake(struct file *file, char __user *ubuf,
	size_t count, loff_t *ppos)
{
	int nbytes;

	ipa_active_clients_lock();
	if (ipa_ctx->ipa_active_clients.cnt)
		nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
				"IPA APPS power state is ON\n");
	else
		nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
				"IPA APPS power state is OFF\n");
	ipa_active_clients_unlock();

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa_read_hdr(struct file *file, char __user *ubuf, size_t count,
		loff_t *ppos)
{
	int nbytes = 0;
	int i = 0;
	struct ipa_hdr_entry *entry;

	mutex_lock(&ipa_ctx->lock);

	if (ipa_ctx->hdr_tbl_lcl)
		pr_err("Table resides on local memory\n");
	else
		pr_err("Table resides on system (ddr) memory\n");

	list_for_each_entry(entry, &ipa_ctx->hdr_tbl.head_hdr_entry_list,
			link) {
		nbytes = scnprintf(
			dbg_buff,
			IPA_MAX_MSG_LEN,
			"name:%s len=%d ref=%d partial=%d type=%s ",
			entry->name,
			entry->hdr_len,
			entry->ref_cnt,
			entry->is_partial,
			ipa_hdr_l2_type_name[entry->type]);

		if (entry->is_hdr_proc_ctx) {
			nbytes += scnprintf(
				dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"phys_base=0x%pa ",
				&entry->phys_base);
		} else {
			nbytes += scnprintf(
				dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"ofst=%u ",
				entry->offset_entry->offset >> 2);
		}
		for (i = 0; i < entry->hdr_len; i++) {
			scnprintf(dbg_buff + nbytes + i * 2,
				  IPA_MAX_MSG_LEN - nbytes - i * 2,
				  "%02x", entry->hdr[i]);
		}
		scnprintf(dbg_buff + nbytes + entry->hdr_len * 2,
			  IPA_MAX_MSG_LEN - nbytes - entry->hdr_len * 2,
			  "\n");
		pr_err("%s", dbg_buff);
	}
	mutex_unlock(&ipa_ctx->lock);

	return 0;
}

static int ipa_attrib_dump(struct ipa_rule_attrib *attrib,
		enum ipa_ip_type ip)
{
	uint32_t addr[4];
	uint32_t mask[4];
	int i;

	if (attrib->attrib_mask & IPA_FLT_TOS_MASKED)
		pr_err("tos_value:%d ", attrib->tos_value);

	if (attrib->attrib_mask & IPA_FLT_TOS_MASKED)
		pr_err("tos_mask:%d ", attrib->tos_mask);

	if (attrib->attrib_mask & IPA_FLT_PROTOCOL)
		pr_err("protocol:%d ", attrib->u.v4.protocol);

	if (attrib->attrib_mask & IPA_FLT_SRC_ADDR) {
		if (ip == IPA_IP_v4) {
			addr[0] = htonl(attrib->u.v4.src_addr);
			mask[0] = htonl(attrib->u.v4.src_addr_mask);
			pr_err(
					"src_addr:%pI4 src_addr_mask:%pI4 ",
					addr + 0, mask + 0);
		} else if (ip == IPA_IP_v6) {
			for (i = 0; i < 4; i++) {
				addr[i] = htonl(attrib->u.v6.src_addr[i]);
				mask[i] = htonl(attrib->u.v6.src_addr_mask[i]);
			}
			pr_err(
					   "src_addr:%pI6 src_addr_mask:%pI6 ",
					   addr + 0, mask + 0);
		} else {
			WARN_ON(1);
		}
	}
	if (attrib->attrib_mask & IPA_FLT_DST_ADDR) {
		if (ip == IPA_IP_v4) {
			addr[0] = htonl(attrib->u.v4.dst_addr);
			mask[0] = htonl(attrib->u.v4.dst_addr_mask);
			pr_err(
					   "dst_addr:%pI4 dst_addr_mask:%pI4 ",
					   addr + 0, mask + 0);
		} else if (ip == IPA_IP_v6) {
			for (i = 0; i < 4; i++) {
				addr[i] = htonl(attrib->u.v6.dst_addr[i]);
				mask[i] = htonl(attrib->u.v6.dst_addr_mask[i]);
			}
			pr_err(
					   "dst_addr:%pI6 dst_addr_mask:%pI6 ",
					   addr + 0, mask + 0);
		} else {
			WARN_ON(1);
		}
	}
	if (attrib->attrib_mask & IPA_FLT_SRC_PORT_RANGE) {
		pr_err("src_port_range:%u %u ",
				   attrib->src_port_lo,
			     attrib->src_port_hi);
	}
	if (attrib->attrib_mask & IPA_FLT_DST_PORT_RANGE) {
		pr_err("dst_port_range:%u %u ",
				   attrib->dst_port_lo,
			     attrib->dst_port_hi);
	}
	if (attrib->attrib_mask & IPA_FLT_TYPE)
		pr_err("type:%d ", attrib->type);

	if (attrib->attrib_mask & IPA_FLT_CODE)
		pr_err("code:%d ", attrib->code);

	if (attrib->attrib_mask & IPA_FLT_SPI)
		pr_err("spi:%x ", attrib->spi);

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT)
		pr_err("src_port:%u ", attrib->src_port);

	if (attrib->attrib_mask & IPA_FLT_DST_PORT)
		pr_err("dst_port:%u ", attrib->dst_port);

	if (attrib->attrib_mask & IPA_FLT_TC)
		pr_err("tc:%d ", attrib->u.v6.tc);

	if (attrib->attrib_mask & IPA_FLT_FLOW_LABEL)
		pr_err("flow_label:%x ", attrib->u.v6.flow_label);

	if (attrib->attrib_mask & IPA_FLT_NEXT_HDR)
		pr_err("next_hdr:%d ", attrib->u.v6.next_hdr);

	if (attrib->attrib_mask & IPA_FLT_META_DATA) {
		pr_err(
				   "metadata:%x metadata_mask:%x",
				   attrib->meta_data, attrib->meta_data_mask);
	}

	if (attrib->attrib_mask & IPA_FLT_FRAGMENT)
		pr_err("frg ");

	if ((attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_ETHER_II) ||
		(attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_802_3)) {
		pr_err("src_mac_addr:%pM ", attrib->src_mac_addr);
	}

	if ((attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_ETHER_II) ||
		(attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_802_3)) {
		pr_err("dst_mac_addr:%pM ", attrib->dst_mac_addr);
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_ETHER_TYPE)
		pr_err("ether_type:%x ", attrib->ether_type);

	pr_err("\n");
	return 0;
}

static int ipa_attrib_dump_eq(struct ipa_ipfltri_rule_eq *attrib)
{
	uint8_t addr[16];
	uint8_t mask[16];
	int i;
	int j;

	if (attrib->tos_eq_present)
		pr_err("tos_value:%d ", attrib->tos_eq);

	if (attrib->protocol_eq_present)
		pr_err("protocol:%d ", attrib->protocol_eq);

	for (i = 0; i < attrib->num_ihl_offset_range_16; i++) {
		pr_err(
			   "(ihl_ofst_range16: ofst:%u lo:%u hi:%u) ",
			   attrib->ihl_offset_range_16[i].offset,
			   attrib->ihl_offset_range_16[i].range_low,
			   attrib->ihl_offset_range_16[i].range_high);
	}

	for (i = 0; i < attrib->num_offset_meq_32; i++) {
		pr_err(
			   "(ofst_meq32: ofst:%u mask:0x%x val:0x%x) ",
			   attrib->offset_meq_32[i].offset,
			   attrib->offset_meq_32[i].mask,
			   attrib->offset_meq_32[i].value);
	}

	if (attrib->tc_eq_present)
		pr_err("tc:%d ", attrib->tc_eq);

	if (attrib->fl_eq_present)
		pr_err("flow_label:%d ", attrib->fl_eq);

	if (attrib->ihl_offset_eq_16_present) {
		pr_err(
				"(ihl_ofst_eq16:%d val:0x%x) ",
				attrib->ihl_offset_eq_16.offset,
				attrib->ihl_offset_eq_16.value);
	}

	for (i = 0; i < attrib->num_ihl_offset_meq_32; i++) {
		pr_err(
				"(ihl_ofst_meq32: ofts:%d mask:0x%x val:0x%x) ",
				attrib->ihl_offset_meq_32[i].offset,
				attrib->ihl_offset_meq_32[i].mask,
				attrib->ihl_offset_meq_32[i].value);
	}

	for (i = 0; i < attrib->num_offset_meq_128; i++) {
		for (j = 0; j < 16; j++) {
			addr[j] = attrib->offset_meq_128[i].value[j];
			mask[j] = attrib->offset_meq_128[i].mask[j];
		}
		pr_err(
				"(ofst_meq128: ofst:%d mask:%pI6 val:%pI6) ",
				attrib->offset_meq_128[i].offset,
				mask + 0,
				addr + 0);
	}

	if (attrib->metadata_meq32_present) {
		pr_err(
				"(metadata: ofst:%u mask:0x%x val:0x%x) ",
				attrib->metadata_meq32.offset,
				attrib->metadata_meq32.mask,
				attrib->metadata_meq32.value);
	}

	if (attrib->ipv4_frag_eq_present)
		pr_err("frg ");

	pr_err("\n");
	return 0;
}

static int ipa_open_dbg(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t ipa_read_rt(struct file *file, char __user *ubuf, size_t count,
		loff_t *ppos)
{
	int i = 0;
	struct ipa_rt_tbl *tbl;
	struct ipa_rt_entry *entry;
	struct ipa_rt_tbl_set *set;
	enum ipa_ip_type ip = (enum ipa_ip_type)file->private_data;
	u32 ofst;
	u32 ofst_words;

	set = &ipa_ctx->rt_tbl_set[ip];

	mutex_lock(&ipa_ctx->lock);

	if (ip ==  IPA_IP_v6) {
		if (ipa_ctx->ip6_rt_tbl_lcl)
			pr_err("Table resides on local memory\n");
		else
			pr_err("Table resides on system (ddr) memory\n");
	} else if (ip == IPA_IP_v4) {
		if (ipa_ctx->ip4_rt_tbl_lcl)
			pr_err("Table resides on local memory\n");
		else
			pr_err("Table resides on system (ddr) memory\n");
	}

	list_for_each_entry(tbl, &set->head_rt_tbl_list, link) {
		i = 0;
		list_for_each_entry(entry, &tbl->head_rt_rule_list, link) {
			if (entry->proc_ctx) {
				ofst = entry->proc_ctx->offset_entry->offset;
				ofst_words =
					(ofst +
					ipa_ctx->hdr_proc_ctx_tbl.start_offset)
					>> 5;

				pr_err(
					"tbl_idx:%d tbl_name:%s tbl_ref:%u "
					"rule_idx:%d dst:%d ep:%d S:%u "
					"proc_ctx[32B]:%u attrib_mask:%08x ",
					entry->tbl->idx, entry->tbl->name,
					entry->tbl->ref_cnt, i, entry->rule.dst,
					ipa_get_ep_mapping(entry->rule.dst),
					!ipa_ctx->hdr_tbl_lcl,
					ofst_words,
					entry->rule.attrib.attrib_mask);
			} else {
				if (entry->hdr)
					ofst = entry->hdr->offset_entry->offset;
				else
					ofst = 0;

				pr_err(
					"tbl_idx:%d tbl_name:%s tbl_ref:%u "
					"rule_idx:%d dst:%d ep:%d S:%u "
					"hdr_ofst[words]:%u attrib_mask:%08x ",
					entry->tbl->idx, entry->tbl->name,
					entry->tbl->ref_cnt, i, entry->rule.dst,
					ipa_get_ep_mapping(entry->rule.dst),
					!ipa_ctx->hdr_tbl_lcl,
					ofst >> 2,
					entry->rule.attrib.attrib_mask);
			}

			ipa_attrib_dump(&entry->rule.attrib, ip);
			i++;
		}
	}
	mutex_unlock(&ipa_ctx->lock);

	return 0;
}

static ssize_t ipa_read_proc_ctx(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes = 0;
	struct ipa_hdr_proc_ctx_tbl *tbl;
	struct ipa_hdr_proc_ctx_entry *entry;
	u32 ofst_words;

	tbl = &ipa_ctx->hdr_proc_ctx_tbl;

	mutex_lock(&ipa_ctx->lock);

	if (ipa_ctx->hdr_proc_ctx_tbl_lcl)
		pr_info("Table resides on local memory\n");
	else
		pr_info("Table resides on system(ddr) memory\n");

	list_for_each_entry(entry, &tbl->head_proc_ctx_entry_list, link) {
		ofst_words = (entry->offset_entry->offset +
			ipa_ctx->hdr_proc_ctx_tbl.start_offset)
			>> 5;
		if (entry->hdr->is_hdr_proc_ctx) {
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"id:%u hdr_proc_type:%s proc_ctx[32B]:%u "
				"hdr_phys_base:0x%pa\n",
				entry->id,
				ipa_hdr_proc_type_name[entry->type],
				ofst_words,
				&entry->hdr->phys_base);
		} else {
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"id:%u hdr_proc_type:%s proc_ctx[32B]:%u "
				"hdr[words]:%u\n",
				entry->id,
				ipa_hdr_proc_type_name[entry->type],
				ofst_words,
				entry->hdr->offset_entry->offset >> 2);
		}
	}
	mutex_unlock(&ipa_ctx->lock);

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa_read_flt(struct file *file, char __user *ubuf, size_t count,
		loff_t *ppos)
{
	int i;
	int j;
	struct ipa_flt_tbl *tbl;
	struct ipa_flt_entry *entry;
	enum ipa_ip_type ip = (enum ipa_ip_type)file->private_data;
	struct ipa_rt_tbl *rt_tbl;
	u32 rt_tbl_idx;
	u32 bitmap;
	bool eq;

	tbl = &ipa_ctx->glob_flt_tbl[ip];
	mutex_lock(&ipa_ctx->lock);
	i = 0;
	list_for_each_entry(entry, &tbl->head_flt_rule_list, link) {
		if (entry->rule.eq_attrib_type) {
			rt_tbl_idx = entry->rule.rt_tbl_idx;
			bitmap = entry->rule.eq_attrib.rule_eq_bitmap;
			eq = true;
		} else {
			rt_tbl = ipa_id_find(entry->rule.rt_tbl_hdl);
			if (rt_tbl)
				rt_tbl_idx = rt_tbl->idx;
			else
				rt_tbl_idx = ~0;
			bitmap = entry->rule.attrib.attrib_mask;
			eq = false;
		}
		pr_err(
			   "ep_idx:global rule_idx:%d act:%d rt_tbl_idx:%d "
			   "attrib_mask:%08x to_uc:%d, retain_hdr:%d eq:%d ",
			   i, entry->rule.action, rt_tbl_idx, bitmap,
			   entry->rule.to_uc, entry->rule.retain_hdr, eq);
		if (eq)
			ipa_attrib_dump_eq(
				&entry->rule.eq_attrib);
		else
			ipa_attrib_dump(
				&entry->rule.attrib, ip);
		i++;
	}

	for (j = 0; j < IPA_NUM_PIPES; j++) {
		tbl = &ipa_ctx->flt_tbl[j][ip];
		i = 0;
		list_for_each_entry(entry, &tbl->head_flt_rule_list, link) {
			if (entry->rule.eq_attrib_type) {
				rt_tbl_idx = entry->rule.rt_tbl_idx;
				bitmap = entry->rule.eq_attrib.rule_eq_bitmap;
				eq = true;
			} else {
				rt_tbl = ipa_id_find(entry->rule.rt_tbl_hdl);
				if (rt_tbl)
					rt_tbl_idx = rt_tbl->idx;
				else
					rt_tbl_idx = ~0;
				bitmap = entry->rule.attrib.attrib_mask;
				eq = false;
			}
			pr_err(
				"ep_idx:%d rule_idx:%d act:%d rt_tbl_idx:%d "
				"attrib_mask:%08x to_uc:%d, retain_hdr:%d eq:%d ",
				j, i, entry->rule.action,
				rt_tbl_idx, bitmap, entry->rule.to_uc,
				entry->rule.retain_hdr, eq);
			if (eq)
				ipa_attrib_dump_eq(
					&entry->rule.eq_attrib);
			else
				ipa_attrib_dump(
					&entry->rule.attrib, ip);
			i++;
		}
	}
	mutex_unlock(&ipa_ctx->lock);

	return 0;
}

static ssize_t ipa_read_stats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes;
	int i;
	int cnt = 0;
	uint connect = 0;

	for (i = 0; i < IPA_NUM_PIPES; i++)
		connect |= (ipa_ctx->ep[i].valid << i);

	if (ipa_ctx->ipa_hw_type == IPA_HW_v2_0 ||
		ipa_ctx->ipa_hw_type == IPA_HW_v2_5) {
		nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
			"sw_tx=%u\n"
			"hw_tx=%u\n"
			"tx_compl=%u\n"
			"wan_rx=%u\n"
			"stat_compl=%u\n"
			"lan_aggr_close=%u\n"
			"wan_aggr_close=%u\n"
			"act_clnt=%u\n"
			"con_clnt_bmap=0x%x\n"
			"wan_rx_empty=%u\n"
			"wan_repl_rx_empty=%u\n"
			"lan_rx_empty=%u\n"
			"lan_repl_rx_empty=%u\n",
			ipa_ctx->stats.tx_sw_pkts,
			ipa_ctx->stats.tx_hw_pkts,
			ipa_ctx->stats.tx_pkts_compl,
			ipa_ctx->stats.rx_pkts,
			ipa_ctx->stats.stat_compl,
			ipa_ctx->stats.aggr_close,
			ipa_ctx->stats.wan_aggr_close,
			ipa_ctx->ipa_active_clients.cnt,
			connect,
			ipa_ctx->stats.wan_rx_empty,
			ipa_ctx->stats.wan_repl_rx_empty,
			ipa_ctx->stats.lan_rx_empty,
			ipa_ctx->stats.lan_repl_rx_empty);
		cnt += nbytes;

		for (i = 0; i < MAX_NUM_EXCP; i++) {
			nbytes = scnprintf(dbg_buff + cnt,
				IPA_MAX_MSG_LEN - cnt,
				"lan_rx_excp[%u:%20s]=%u\n", i,
				ipa_status_excp_name[i],
				ipa_ctx->stats.rx_excp_pkts[i]);
			cnt += nbytes;
		}
	} else{
		nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
			"sw_tx=%u\n"
			"hw_tx=%u\n"
			"rx=%u\n"
			"rx_repl_repost=%u\n"
			"rx_q_len=%u\n"
			"act_clnt=%u\n"
			"con_clnt_bmap=0x%x\n",
			ipa_ctx->stats.tx_sw_pkts,
			ipa_ctx->stats.tx_hw_pkts,
			ipa_ctx->stats.rx_pkts,
			ipa_ctx->stats.rx_repl_repost,
			ipa_ctx->stats.rx_q_len,
			ipa_ctx->ipa_active_clients.cnt,
			connect);
	cnt += nbytes;

		for (i = 0; i < MAX_NUM_EXCP; i++) {
			nbytes = scnprintf(dbg_buff + cnt,
				IPA_MAX_MSG_LEN - cnt,
				"rx_excp[%u:%35s]=%u\n", i, ipa_excp_name[i],
				ipa_ctx->stats.rx_excp_pkts[i]);
			cnt += nbytes;
		}
	}

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
}

static ssize_t ipa_read_wstats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{

#define HEAD_FRMT_STR "%25s\n"
#define FRMT_STR "%25s %10u\n"
#define FRMT_STR1 "%25s %10u\n\n"

	int cnt = 0;
	int nbytes;
	int ipa_ep_idx;
	enum ipa_client_type client = IPA_CLIENT_WLAN1_PROD;
	struct ipa_ep_context *ep;

	do {
		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
			HEAD_FRMT_STR, "Client IPA_CLIENT_WLAN1_PROD Stats:");
		cnt += nbytes;

		ipa_ep_idx = ipa_get_ep_mapping(client);
		if (ipa_ep_idx == -1) {
			nbytes = scnprintf(dbg_buff + cnt,
				IPA_MAX_MSG_LEN - cnt, HEAD_FRMT_STR, "Not up");
			cnt += nbytes;
			break;
		}

		ep = &ipa_ctx->ep[ipa_ep_idx];
		if (ep->valid != 1) {
			nbytes = scnprintf(dbg_buff + cnt,
				IPA_MAX_MSG_LEN - cnt, HEAD_FRMT_STR, "Not up");
			cnt += nbytes;
			break;
		}

		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
			FRMT_STR, "Avail Fifo Desc:",
			atomic_read(&ep->avail_fifo_desc));
		cnt += nbytes;

		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
			FRMT_STR, "Rx Pkts Rcvd:", ep->wstats.rx_pkts_rcvd);
		cnt += nbytes;

		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
			FRMT_STR, "Rx Pkts Status Rcvd:",
			ep->wstats.rx_pkts_status_rcvd);
		cnt += nbytes;

		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
			FRMT_STR, "Rx DH Rcvd:", ep->wstats.rx_hd_rcvd);
		cnt += nbytes;

		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
			FRMT_STR, "Rx DH Processed:",
			ep->wstats.rx_hd_processed);
		cnt += nbytes;

		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
			FRMT_STR, "Rx DH Sent Back:", ep->wstats.rx_hd_reply);
		cnt += nbytes;

		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
			FRMT_STR, "Rx Pkt Leak:", ep->wstats.rx_pkt_leak);
		cnt += nbytes;

		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
			FRMT_STR1, "Rx DP Fail:", ep->wstats.rx_dp_fail);
		cnt += nbytes;

	} while (0);

	client = IPA_CLIENT_WLAN1_CONS;
	nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt, HEAD_FRMT_STR,
		"Client IPA_CLIENT_WLAN1_CONS Stats:");
	cnt += nbytes;
	while (1) {
		ipa_ep_idx = ipa_get_ep_mapping(client);
		if (ipa_ep_idx == -1) {
			nbytes = scnprintf(dbg_buff + cnt,
				IPA_MAX_MSG_LEN - cnt, HEAD_FRMT_STR, "Not up");
			cnt += nbytes;
			goto nxt_clnt_cons;
		}

		ep = &ipa_ctx->ep[ipa_ep_idx];
		if (ep->valid != 1) {
			nbytes = scnprintf(dbg_buff + cnt,
				IPA_MAX_MSG_LEN - cnt, HEAD_FRMT_STR, "Not up");
			cnt += nbytes;
			goto nxt_clnt_cons;
		}

		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
			FRMT_STR, "Tx Pkts Received:", ep->wstats.tx_pkts_rcvd);
		cnt += nbytes;

		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
			FRMT_STR, "Tx Pkts Sent:", ep->wstats.tx_pkts_sent);
		cnt += nbytes;

		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
			FRMT_STR1, "Tx Pkts Dropped:",
			ep->wstats.tx_pkts_dropped);
		cnt += nbytes;

nxt_clnt_cons:
			switch (client) {
			case IPA_CLIENT_WLAN1_CONS:
				client = IPA_CLIENT_WLAN2_CONS;
				nbytes = scnprintf(dbg_buff + cnt,
					IPA_MAX_MSG_LEN - cnt, HEAD_FRMT_STR,
					"Client IPA_CLIENT_WLAN2_CONS Stats:");
				cnt += nbytes;
				continue;
			case IPA_CLIENT_WLAN2_CONS:
				client = IPA_CLIENT_WLAN3_CONS;
				nbytes = scnprintf(dbg_buff + cnt,
					IPA_MAX_MSG_LEN - cnt, HEAD_FRMT_STR,
					"Client IPA_CLIENT_WLAN3_CONS Stats:");
				cnt += nbytes;
				continue;
			case IPA_CLIENT_WLAN3_CONS:
				client = IPA_CLIENT_WLAN4_CONS;
				nbytes = scnprintf(dbg_buff + cnt,
					IPA_MAX_MSG_LEN - cnt, HEAD_FRMT_STR,
					"Client IPA_CLIENT_WLAN4_CONS Stats:");
				cnt += nbytes;
				continue;
			case IPA_CLIENT_WLAN4_CONS:
			default:
				break;
			}
		break;
	}

	nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
		"\n"HEAD_FRMT_STR, "All Wlan Consumer pipes stats:");
	cnt += nbytes;

	nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt, FRMT_STR,
		"Tx Comm Buff Allocated:",
		ipa_ctx->wc_memb.wlan_comm_total_cnt);
	cnt += nbytes;

	nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt, FRMT_STR,
		"Tx Comm Buff Avail:", ipa_ctx->wc_memb.wlan_comm_free_cnt);
	cnt += nbytes;

	nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt, FRMT_STR1,
		"Total Tx Pkts Freed:", ipa_ctx->wc_memb.total_tx_pkts_freed);
	cnt += nbytes;

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
}

static ssize_t ipa_read_wdi(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct IpaHwStatsWDIInfoData_t stats;
	int nbytes;
	int cnt = 0;

	if (!ipa_get_wdi_stats(&stats)) {
		nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
			"TX num_pkts_processed=%u\n"
			"TX copy_engine_doorbell_value=%u\n"
			"TX num_db_fired=%u\n"
			"TX ringFull=%u\n"
			"TX ringEmpty=%u\n"
			"TX ringUsageHigh=%u\n"
			"TX ringUsageLow=%u\n"
			"TX bamFifoFull=%u\n"
			"TX bamFifoEmpty=%u\n"
			"TX bamFifoUsageHigh=%u\n"
			"TX bamFifoUsageLow=%u\n"
			"TX num_db=%u\n"
			"TX num_unexpected_db=%u\n"
			"TX num_bam_int_handled=%u\n"
			"TX num_bam_int_in_non_runnning_state=%u\n"
			"TX num_qmb_int_handled=%u\n",
			stats.tx_ch_stats.num_pkts_processed,
			stats.tx_ch_stats.copy_engine_doorbell_value,
			stats.tx_ch_stats.num_db_fired,
			stats.tx_ch_stats.tx_comp_ring_stats.ringFull,
			stats.tx_ch_stats.tx_comp_ring_stats.ringEmpty,
			stats.tx_ch_stats.tx_comp_ring_stats.ringUsageHigh,
			stats.tx_ch_stats.tx_comp_ring_stats.ringUsageLow,
			stats.tx_ch_stats.bam_stats.bamFifoFull,
			stats.tx_ch_stats.bam_stats.bamFifoEmpty,
			stats.tx_ch_stats.bam_stats.bamFifoUsageHigh,
			stats.tx_ch_stats.bam_stats.bamFifoUsageLow,
			stats.tx_ch_stats.num_db,
			stats.tx_ch_stats.num_unexpected_db,
			stats.tx_ch_stats.num_bam_int_handled,
			stats.tx_ch_stats.num_bam_int_in_non_runnning_state,
			stats.tx_ch_stats.num_qmb_int_handled);
		cnt += nbytes;
		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
			"RX max_outstanding_pkts=%u\n"
			"RX num_pkts_processed=%u\n"
			"RX rx_ring_rp_value=%u\n"
			"RX ringFull=%u\n"
			"RX ringEmpty=%u\n"
			"RX ringUsageHigh=%u\n"
			"RX ringUsageLow=%u\n"
			"RX bamFifoFull=%u\n"
			"RX bamFifoEmpty=%u\n"
			"RX bamFifoUsageHigh=%u\n"
			"RX bamFifoUsageLow=%u\n"
			"RX num_bam_int_handled=%u\n"
			"RX num_db=%u\n"
			"RX num_unexpected_db=%u\n"
			"RX reserved1=%u\n"
			"RX reserved2=%u\n",
			stats.rx_ch_stats.max_outstanding_pkts,
			stats.rx_ch_stats.num_pkts_processed,
			stats.rx_ch_stats.rx_ring_rp_value,
			stats.rx_ch_stats.rx_ind_ring_stats.ringFull,
			stats.rx_ch_stats.rx_ind_ring_stats.ringEmpty,
			stats.rx_ch_stats.rx_ind_ring_stats.ringUsageHigh,
			stats.rx_ch_stats.rx_ind_ring_stats.ringUsageLow,
			stats.rx_ch_stats.bam_stats.bamFifoFull,
			stats.rx_ch_stats.bam_stats.bamFifoEmpty,
			stats.rx_ch_stats.bam_stats.bamFifoUsageHigh,
			stats.rx_ch_stats.bam_stats.bamFifoUsageLow,
			stats.rx_ch_stats.num_bam_int_handled,
			stats.rx_ch_stats.num_db,
			stats.rx_ch_stats.num_unexpected_db,
			stats.rx_ch_stats.reserved1,
			stats.rx_ch_stats.reserved2);
		cnt += nbytes;
	} else {
		nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
				"Fail to read WDI stats\n");
		cnt += nbytes;
	}

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
}

void _ipa_write_dbg_cnt_v1_1(int option)
{
	if (option == 1)
		ipa_write_reg(ipa_ctx->mmio, IPA_DEBUG_CNT_CTRL_N_OFST_v1_1(0),
				IPA_DBG_CNTR_ON);
	else
		ipa_write_reg(ipa_ctx->mmio, IPA_DEBUG_CNT_CTRL_N_OFST_v1_1(0),
				IPA_DBG_CNTR_OFF);
}

void _ipa_write_dbg_cnt_v2_0(int option)
{
	if (option == 1)
		ipa_write_reg(ipa_ctx->mmio, IPA_DEBUG_CNT_CTRL_N_OFST_v2_0(0),
				IPA_DBG_CNTR_ON);
	else
		ipa_write_reg(ipa_ctx->mmio, IPA_DEBUG_CNT_CTRL_N_OFST_v2_0(0),
				IPA_DBG_CNTR_OFF);
}

static ssize_t ipa_write_dbg_cnt(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	unsigned long missing;
	u32 option = 0;

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, buf, count);
	if (missing)
		return -EFAULT;

	dbg_buff[count] = '\0';
	if (kstrtou32(dbg_buff, 0, &option))
		return -EFAULT;

	ipa_inc_client_enable_clks();
	ipa_ctx->ctrl->ipa_write_dbg_cnt(option);
	ipa_dec_client_disable_clks();

	return count;
}

int _ipa_read_dbg_cnt_v1_1(char *buf, int max_len)
{
	int regval;

	regval = ipa_read_reg(ipa_ctx->mmio,
			IPA_DEBUG_CNT_REG_N_OFST_v1_1(0));

	return scnprintf(buf, max_len,
			"IPA_DEBUG_CNT_REG_0=0x%x\n", regval);
}

int _ipa_read_dbg_cnt_v2_0(char *buf, int max_len)
{
	int regval;

	regval = ipa_read_reg(ipa_ctx->mmio,
			IPA_DEBUG_CNT_REG_N_OFST_v2_0(0));

	return scnprintf(buf, max_len,
			"IPA_DEBUG_CNT_REG_0=0x%x\n", regval);
}

static ssize_t ipa_read_dbg_cnt(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes;

	ipa_inc_client_enable_clks();
	nbytes = ipa_ctx->ctrl->ipa_read_dbg_cnt(dbg_buff, IPA_MAX_MSG_LEN);
	ipa_dec_client_disable_clks();

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa_read_msg(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes;
	int cnt = 0;
	int i;

	for (i = 0; i < IPA_EVENT_MAX_NUM; i++) {
		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
				"msg[%u:%27s] W:%u R:%u\n", i,
				ipa_event_name[i],
				ipa_ctx->stats.msg_w[i],
				ipa_ctx->stats.msg_r[i]);
		cnt += nbytes;
	}

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
}

static ssize_t ipa_read_nat4(struct file *file,
		char __user *ubuf, size_t count,
		loff_t *ppos) {

#define ENTRY_U32_FIELDS 8
#define NAT_ENTRY_ENABLE 0x8000
#define NAT_ENTRY_RST_FIN_BIT 0x4000
#define BASE_TABLE 0
#define EXPANSION_TABLE 1

	u32 *base_tbl, *indx_tbl;
	u32 tbl_size, *tmp;
	u32 value, i, j, rule_id;
	u16 enable, tbl_entry, flag;

	value = ipa_ctx->nat_mem.public_ip_addr;
	pr_err(
				"Table IP Address:%d.%d.%d.%d\n",
				((value & 0xFF000000) >> 24),
				((value & 0x00FF0000) >> 16),
				((value & 0x0000FF00) >> 8),
				((value & 0x000000FF)));

	pr_err("Table Size:%d\n",
				ipa_ctx->nat_mem.size_base_tables);

	pr_err("Expansion Table Size:%d\n",
				ipa_ctx->nat_mem.size_expansion_tables);

	if (!ipa_ctx->nat_mem.is_sys_mem)
		pr_err("Not supported for local(shared) memory\n");

	/* Print Base tables */
	rule_id = 0;
	for (j = 0; j < 2; j++) {
		if (j == BASE_TABLE) {
			tbl_size = ipa_ctx->nat_mem.size_base_tables;
			base_tbl = (u32 *)ipa_ctx->nat_mem.ipv4_rules_addr;

			pr_err("\nBase Table:\n");
		} else {
			tbl_size = ipa_ctx->nat_mem.size_expansion_tables;
			base_tbl =
			 (u32 *)ipa_ctx->nat_mem.ipv4_expansion_rules_addr;

			pr_err("\nExpansion Base Table:\n");
		}

		if (base_tbl != NULL) {
			for (i = 0; i <= tbl_size; i++, rule_id++) {
				tmp = base_tbl;
				value = tmp[4];
				enable = ((value & 0xFFFF0000) >> 16);

				if (enable & NAT_ENTRY_ENABLE) {
					pr_err("Rule:%d ", rule_id);

					value = *tmp;
					pr_err(
						"Private_IP:%d.%d.%d.%d ",
						((value & 0xFF000000) >> 24),
						((value & 0x00FF0000) >> 16),
						((value & 0x0000FF00) >> 8),
						((value & 0x000000FF)));
					tmp++;

					value = *tmp;
					pr_err(
						"Target_IP:%d.%d.%d.%d ",
						((value & 0xFF000000) >> 24),
						((value & 0x00FF0000) >> 16),
						((value & 0x0000FF00) >> 8),
						((value & 0x000000FF)));
					tmp++;

					value = *tmp;
					pr_err(
						"Next_Index:%d  Public_Port:%d ",
						(value & 0x0000FFFF),
						((value & 0xFFFF0000) >> 16));
					tmp++;

					value = *tmp;
					pr_err(
						"Private_Port:%d  Target_Port:%d ",
						(value & 0x0000FFFF),
						((value & 0xFFFF0000) >> 16));
					tmp++;

					value = *tmp;
					flag = ((value & 0xFFFF0000) >> 16);
					if (flag & NAT_ENTRY_RST_FIN_BIT) {
						pr_err(
								"IP_CKSM_delta:0x%x  Flags:%s ",
							  (value & 0x0000FFFF),
								"Direct_To_A5");
					} else {
						pr_err(
							"IP_CKSM_delta:0x%x  Flags:%s ",
							(value & 0x0000FFFF),
							"Fwd_to_route");
					}
					tmp++;

					value = *tmp;
					pr_err(
						"Time_stamp:0x%x Proto:%d ",
						(value & 0x00FFFFFF),
						((value & 0xFF000000) >> 24));
					tmp++;

					value = *tmp;
					pr_err(
						"Prev_Index:%d  Indx_tbl_entry:%d ",
						(value & 0x0000FFFF),
						((value & 0xFFFF0000) >> 16));
					tmp++;

					value = *tmp;
					pr_err(
						"TCP_UDP_cksum_delta:0x%x\n",
						((value & 0xFFFF0000) >> 16));
				}

				base_tbl += ENTRY_U32_FIELDS;

			}
		}
	}

	/* Print Index tables */
	rule_id = 0;
	for (j = 0; j < 2; j++) {
		if (j == BASE_TABLE) {
			tbl_size = ipa_ctx->nat_mem.size_base_tables;
			indx_tbl = (u32 *)ipa_ctx->nat_mem.index_table_addr;

			pr_err("\nIndex Table:\n");
		} else {
			tbl_size = ipa_ctx->nat_mem.size_expansion_tables;
			indx_tbl =
			 (u32 *)ipa_ctx->nat_mem.index_table_expansion_addr;

			pr_err("\nExpansion Index Table:\n");
		}

		if (indx_tbl != NULL) {
			for (i = 0; i <= tbl_size; i++, rule_id++) {
				tmp = indx_tbl;
				value = *tmp;
				tbl_entry = (value & 0x0000FFFF);

				if (tbl_entry) {
					pr_err("Rule:%d ", rule_id);

					value = *tmp;
					pr_err(
						"Table_Entry:%d  Next_Index:%d\n",
						tbl_entry,
						((value & 0xFFFF0000) >> 16));
				}

				indx_tbl++;
			}
		}
	}

	return 0;
}

static ssize_t ipa_rm_read_stats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int result, nbytes, cnt = 0;
	result = ipa_rm_stat(dbg_buff, IPA_MAX_MSG_LEN);
	if (result < 0) {
		nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
				"Error in printing RM stat %d\n", result);
		cnt += nbytes;
	} else
		cnt += result;
	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
}

const struct file_operations ipa_gen_reg_ops = {
	.read = ipa_read_gen_reg,
};

const struct file_operations ipa_ep_reg_ops = {
	.read = ipa_read_ep_reg,
	.write = ipa_write_ep_reg,
};

const struct file_operations ipa_keep_awake_ops = {
	.read = ipa_read_keep_awake,
	.write = ipa_write_keep_awake,
};

const struct file_operations ipa_ep_holb_ops = {
	.write = ipa_write_ep_holb,
};

const struct file_operations ipa_hdr_ops = {
	.read = ipa_read_hdr,
};

const struct file_operations ipa_rt_ops = {
	.read = ipa_read_rt,
	.open = ipa_open_dbg,
};

const struct file_operations ipa_proc_ctx_ops = {
	.read = ipa_read_proc_ctx,
};

const struct file_operations ipa_flt_ops = {
	.read = ipa_read_flt,
	.open = ipa_open_dbg,
};

const struct file_operations ipa_stats_ops = {
	.read = ipa_read_stats,
};

const struct file_operations ipa_wstats_ops = {
	.read = ipa_read_wstats,
};

const struct file_operations ipa_wdi_ops = {
	.read = ipa_read_wdi,
};

const struct file_operations ipa_msg_ops = {
	.read = ipa_read_msg,
};

const struct file_operations ipa_dbg_cnt_ops = {
	.read = ipa_read_dbg_cnt,
	.write = ipa_write_dbg_cnt,
};

const struct file_operations ipa_nat4_ops = {
	.read = ipa_read_nat4,
};

const struct file_operations ipa_rm_stats = {
	.read = ipa_rm_read_stats,
};

void ipa_debugfs_init(void)
{
	const mode_t read_only_mode = S_IRUSR | S_IRGRP | S_IROTH;
	const mode_t read_write_mode = S_IRUSR | S_IRGRP | S_IROTH |
			S_IWUSR | S_IWGRP;
	const mode_t write_only_mode = S_IWUSR | S_IWGRP;
	struct dentry *file;

	dent = debugfs_create_dir("ipa", 0);
	if (IS_ERR(dent)) {
		IPAERR("fail to create folder in debug_fs.\n");
		return;
	}

	file = debugfs_create_u32("hw_type", read_only_mode,
			dent, &ipa_ctx->ipa_hw_type);
	if (!file) {
		IPAERR("could not create hw_type file\n");
		goto fail;
	}


	dfile_gen_reg = debugfs_create_file("gen_reg", read_only_mode, dent, 0,
			&ipa_gen_reg_ops);
	if (!dfile_gen_reg || IS_ERR(dfile_gen_reg)) {
		IPAERR("fail to create file for debug_fs gen_reg\n");
		goto fail;
	}

	dfile_ep_reg = debugfs_create_file("ep_reg", read_write_mode, dent, 0,
			&ipa_ep_reg_ops);
	if (!dfile_ep_reg || IS_ERR(dfile_ep_reg)) {
		IPAERR("fail to create file for debug_fs ep_reg\n");
		goto fail;
	}

	dfile_keep_awake = debugfs_create_file("keep_awake", read_write_mode,
			dent, 0, &ipa_keep_awake_ops);
	if (!dfile_keep_awake || IS_ERR(dfile_keep_awake)) {
		IPAERR("fail to create file for debug_fs dfile_keep_awake\n");
		goto fail;
	}

	dfile_ep_holb = debugfs_create_file("holb", write_only_mode, dent,
			0, &ipa_ep_holb_ops);
	if (!dfile_ep_holb || IS_ERR(dfile_ep_holb)) {
		IPAERR("fail to create file for debug_fs dfile_ep_hol_en\n");
		goto fail;
	}

	dfile_hdr = debugfs_create_file("hdr", read_only_mode, dent, 0,
			&ipa_hdr_ops);
	if (!dfile_hdr || IS_ERR(dfile_hdr)) {
		IPAERR("fail to create file for debug_fs hdr\n");
		goto fail;
	}

	dfile_proc_ctx = debugfs_create_file("proc_ctx", read_only_mode, dent,
		0, &ipa_proc_ctx_ops);
	if (!dfile_hdr || IS_ERR(dfile_hdr)) {
		IPAERR("fail to create file for debug_fs proc_ctx\n");
		goto fail;
	}

	dfile_ip4_rt = debugfs_create_file("ip4_rt", read_only_mode, dent,
			(void *)IPA_IP_v4, &ipa_rt_ops);
	if (!dfile_ip4_rt || IS_ERR(dfile_ip4_rt)) {
		IPAERR("fail to create file for debug_fs ip4 rt\n");
		goto fail;
	}

	dfile_ip6_rt = debugfs_create_file("ip6_rt", read_only_mode, dent,
			(void *)IPA_IP_v6, &ipa_rt_ops);
	if (!dfile_ip6_rt || IS_ERR(dfile_ip6_rt)) {
		IPAERR("fail to create file for debug_fs ip6:w" " rt\n");
		goto fail;
	}

	dfile_ip4_flt = debugfs_create_file("ip4_flt", read_only_mode, dent,
			(void *)IPA_IP_v4, &ipa_flt_ops);
	if (!dfile_ip4_flt || IS_ERR(dfile_ip4_flt)) {
		IPAERR("fail to create file for debug_fs ip4 flt\n");
		goto fail;
	}

	dfile_ip6_flt = debugfs_create_file("ip6_flt", read_only_mode, dent,
			(void *)IPA_IP_v6, &ipa_flt_ops);
	if (!dfile_ip6_flt || IS_ERR(dfile_ip6_flt)) {
		IPAERR("fail to create file for debug_fs ip6 flt\n");
		goto fail;
	}

	dfile_stats = debugfs_create_file("stats", read_only_mode, dent, 0,
			&ipa_stats_ops);
	if (!dfile_stats || IS_ERR(dfile_stats)) {
		IPAERR("fail to create file for debug_fs stats\n");
		goto fail;
	}

	dfile_wstats = debugfs_create_file("wstats", read_only_mode,
			dent, 0, &ipa_wstats_ops);
	if (!dfile_wstats || IS_ERR(dfile_wstats)) {
		IPAERR("fail to create file for debug_fs wstats\n");
		goto fail;
	}

	dfile_wdi_stats = debugfs_create_file("wdi", read_only_mode, dent, 0,
			&ipa_wdi_ops);
	if (!dfile_wdi_stats || IS_ERR(dfile_wdi_stats)) {
		IPAERR("fail to create file for debug_fs wdi stats\n");
		goto fail;
	}

	dfile_dbg_cnt = debugfs_create_file("dbg_cnt", read_write_mode, dent, 0,
			&ipa_dbg_cnt_ops);
	if (!dfile_dbg_cnt || IS_ERR(dfile_dbg_cnt)) {
		IPAERR("fail to create file for debug_fs dbg_cnt\n");
		goto fail;
	}

	dfile_msg = debugfs_create_file("msg", read_only_mode, dent, 0,
			&ipa_msg_ops);
	if (!dfile_msg || IS_ERR(dfile_msg)) {
		IPAERR("fail to create file for debug_fs msg\n");
		goto fail;
	}

	dfile_ip4_nat = debugfs_create_file("ip4_nat", read_only_mode, dent,
			0, &ipa_nat4_ops);
	if (!dfile_ip4_nat || IS_ERR(dfile_ip4_nat)) {
		IPAERR("fail to create file for debug_fs ip4 nat\n");
		goto fail;
	}

	dfile_rm_stats = debugfs_create_file("rm_stats",
			read_only_mode, dent, 0, &ipa_rm_stats);
	if (!dfile_rm_stats || IS_ERR(dfile_rm_stats)) {
		IPAERR("fail to create file for debug_fs rm_stats\n");
		goto fail;
	}

	file = debugfs_create_u32("enable_clock_scaling", read_write_mode,
		dent, &ipa_ctx->enable_clock_scaling);
	if (!file) {
		IPAERR("could not create enable_clock_scaling file\n");
		goto fail;
	}

	file = debugfs_create_u32("clock_scaling_bw_threshold_mbps",
		read_write_mode, dent,
		&ipa_ctx->ctrl->clock_scaling_bw_threshold);
	if (!file) {
		IPAERR("could not create clock_scaling_bw_threshold_mbps\n");
		goto fail;
	}

	return;

fail:
	debugfs_remove_recursive(dent);
}

void ipa_debugfs_remove(void)
{
	if (IS_ERR(dent)) {
		IPAERR("ipa_debugfs_remove: folder was not created.\n");
		return;
	}
	debugfs_remove_recursive(dent);
}

#else /* !CONFIG_DEBUG_FS */
void ipa_debugfs_init(void) {}
void ipa_debugfs_remove(void) {}
#endif

