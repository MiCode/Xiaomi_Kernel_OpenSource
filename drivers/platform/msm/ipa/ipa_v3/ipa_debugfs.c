/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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
#include "../ipa_rm_i.h"

#define IPA_MAX_MSG_LEN 4096
#define IPA_DBG_MAX_RULE_IN_TBL 128
#define IPA_DBG_ACTIVE_CLIENT_BUF_SIZE ((IPA3_ACTIVE_CLIENTS_LOG_LINE_LEN \
	* IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES) + IPA_MAX_MSG_LEN)

#define IPA_DUMP_STATUS_FIELD(f) \
	pr_err(#f "=0x%x\n", status->f)

const char *ipa3_excp_name[] = {
	__stringify_1(IPA_A5_MUX_HDR_EXCP_RSVD0),
	__stringify_1(IPA_A5_MUX_HDR_EXCP_RSVD1),
	__stringify_1(IPA_A5_MUX_HDR_EXCP_FLAG_IHL),
	__stringify_1(IPA_A5_MUX_HDR_EXCP_FLAG_REPLICATED),
	__stringify_1(IPA_A5_MUX_HDR_EXCP_FLAG_TAG),
	__stringify_1(IPA_A5_MUX_HDR_EXCP_FLAG_SW_FLT),
	__stringify_1(IPA_A5_MUX_HDR_EXCP_FLAG_NAT),
	__stringify_1(IPA_A5_MUX_HDR_EXCP_FLAG_IP),
};

const char *ipa3_event_name[] = {
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
	__stringify(WAN_XLAT_CONNECT),
	__stringify(ECM_CONNECT),
	__stringify(ECM_DISCONNECT),
	__stringify(IPA_TETHERING_STATS_UPDATE_STATS),
	__stringify(IPA_TETHERING_STATS_UPDATE_NETWORK_STATS),
	__stringify(IPA_QUOTA_REACH),
	__stringify(IPA_SSR_BEFORE_SHUTDOWN),
	__stringify(IPA_SSR_AFTER_POWERUP),
};

const char *ipa3_hdr_l2_type_name[] = {
	__stringify(IPA_HDR_L2_NONE),
	__stringify(IPA_HDR_L2_ETHERNET_II),
	__stringify(IPA_HDR_L2_802_3),
};

const char *ipa3_hdr_proc_type_name[] = {
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
static struct dentry *dfile_ip4_rt_hw;
static struct dentry *dfile_ip6_rt;
static struct dentry *dfile_ip6_rt_hw;
static struct dentry *dfile_ip4_flt;
static struct dentry *dfile_ip4_flt_hw;
static struct dentry *dfile_ip6_flt;
static struct dentry *dfile_ip6_flt_hw;
static struct dentry *dfile_stats;
static struct dentry *dfile_wstats;
static struct dentry *dfile_wdi_stats;
static struct dentry *dfile_ntn_stats;
static struct dentry *dfile_dbg_cnt;
static struct dentry *dfile_msg;
static struct dentry *dfile_ip4_nat;
static struct dentry *dfile_rm_stats;
static struct dentry *dfile_status_stats;
static struct dentry *dfile_active_clients;
static char dbg_buff[IPA_MAX_MSG_LEN];
static char *active_clients_buf;

static s8 ep_reg_idx;
static void *ipa_ipc_low_buff;


static ssize_t ipa3_read_gen_reg(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes;
	struct ipahal_reg_shared_mem_size smem_sz;

	memset(&smem_sz, 0, sizeof(smem_sz));

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	ipahal_read_reg_fields(IPA_SHARED_MEM_SIZE, &smem_sz);
	nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
			"IPA_VERSION=0x%x\n"
			"IPA_COMP_HW_VERSION=0x%x\n"
			"IPA_ROUTE=0x%x\n"
			"IPA_SHARED_MEM_RESTRICTED=0x%x\n"
			"IPA_SHARED_MEM_SIZE=0x%x\n",
			ipahal_read_reg(IPA_VERSION),
			ipahal_read_reg(IPA_COMP_HW_VERSION),
			ipahal_read_reg(IPA_ROUTE),
			smem_sz.shared_mem_baddr,
			smem_sz.shared_mem_sz);

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa3_write_ep_holb(struct file *file,
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

	ipa3_cfg_ep_holb(ep_idx, &holb);

	return count;
}

static ssize_t ipa3_write_ep_reg(struct file *file, const char __user *buf,
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

	if (option >= ipa3_ctx->ipa_num_pipes) {
		IPAERR("bad pipe specified %u\n", option);
		return count;
	}

	ep_reg_idx = option;

	return count;
}

/**
 * _ipa_read_ep_reg_v3_0() - Reads and prints endpoint configuration registers
 *
 * Returns the number of characters printed
 */
int _ipa_read_ep_reg_v3_0(char *buf, int max_len, int pipe)
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
		pipe, ipahal_read_reg_n(IPA_ENDP_INIT_NAT_n, pipe),
		pipe, ipahal_read_reg_n(IPA_ENDP_INIT_HDR_n, pipe),
		pipe, ipahal_read_reg_n(IPA_ENDP_INIT_HDR_EXT_n, pipe),
		pipe, ipahal_read_reg_n(IPA_ENDP_INIT_MODE_n, pipe),
		pipe, ipahal_read_reg_n(IPA_ENDP_INIT_AGGR_n, pipe),
		pipe, ipahal_read_reg_n(IPA_ENDP_INIT_ROUTE_n, pipe),
		pipe, ipahal_read_reg_n(IPA_ENDP_INIT_CTRL_n, pipe),
		pipe, ipahal_read_reg_n(IPA_ENDP_INIT_HOL_BLOCK_EN_n, pipe),
		pipe, ipahal_read_reg_n(IPA_ENDP_INIT_HOL_BLOCK_TIMER_n, pipe),
		pipe, ipahal_read_reg_n(IPA_ENDP_INIT_DEAGGR_n, pipe),
		pipe, ipahal_read_reg_n(IPA_ENDP_INIT_CFG_n, pipe));
}

static ssize_t ipa3_read_ep_reg(struct file *file, char __user *ubuf,
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
		end_idx = ipa3_ctx->ipa_num_pipes;
	} else {
		start_idx = ep_reg_idx;
		end_idx = start_idx + 1;
	}
	pos = *ppos;
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	for (i = start_idx; i < end_idx; i++) {

		nbytes = ipa3_ctx->ctrl->ipa3_read_ep_reg(dbg_buff,
				IPA_MAX_MSG_LEN, i);

		*ppos = pos;
		ret = simple_read_from_buffer(ubuf, count, ppos, dbg_buff,
					      nbytes);
		if (ret < 0) {
			IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
			return ret;
		}

		size += ret;
		ubuf += nbytes;
		count -= nbytes;
	}
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	*ppos = pos + size;
	return size;
}

static ssize_t ipa3_write_keep_awake(struct file *file, const char __user *buf,
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
		IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	else if (option == 0)
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	else
		return -EFAULT;

	return count;
}

static ssize_t ipa3_read_keep_awake(struct file *file, char __user *ubuf,
	size_t count, loff_t *ppos)
{
	int nbytes;

	ipa3_active_clients_lock();
	if (ipa3_ctx->ipa3_active_clients.cnt)
		nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
				"IPA APPS power state is ON\n");
	else
		nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
				"IPA APPS power state is OFF\n");
	ipa3_active_clients_unlock();

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa3_read_hdr(struct file *file, char __user *ubuf, size_t count,
		loff_t *ppos)
{
	int nbytes = 0;
	int i = 0;
	struct ipa3_hdr_entry *entry;

	mutex_lock(&ipa3_ctx->lock);

	if (ipa3_ctx->hdr_tbl_lcl)
		pr_err("Table resides on local memory\n");
	else
		pr_err("Table resides on system (ddr) memory\n");

	list_for_each_entry(entry, &ipa3_ctx->hdr_tbl.head_hdr_entry_list,
			link) {
		nbytes = scnprintf(
			dbg_buff,
			IPA_MAX_MSG_LEN,
			"name:%s len=%d ref=%d partial=%d type=%s ",
			entry->name,
			entry->hdr_len,
			entry->ref_cnt,
			entry->is_partial,
			ipa3_hdr_l2_type_name[entry->type]);

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
	mutex_unlock(&ipa3_ctx->lock);

	return 0;
}

static int ipa3_attrib_dump(struct ipa_rule_attrib *attrib,
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
				   "metadata:%x metadata_mask:%x ",
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

static int ipa3_attrib_dump_eq(struct ipa_ipfltri_rule_eq *attrib)
{
	uint8_t addr[16];
	uint8_t mask[16];
	int i;
	int j;

	if (attrib->tos_eq_present)
		pr_err("tos_value:%d ", attrib->tos_eq);

	if (attrib->protocol_eq_present)
		pr_err("protocol:%d ", attrib->protocol_eq);

	if (attrib->tc_eq_present)
		pr_err("tc:%d ", attrib->tc_eq);

	for (i = 0; i < attrib->num_offset_meq_128; i++) {
		for (j = 0; j < 16; j++) {
			addr[j] = attrib->offset_meq_128[i].value[j];
			mask[j] = attrib->offset_meq_128[i].mask[j];
		}
		pr_err(
			"(ofst_meq128: ofst:%d mask:%pI6 val:%pI6) ",
			attrib->offset_meq_128[i].offset,
			mask, addr);
	}

	for (i = 0; i < attrib->num_offset_meq_32; i++)
		pr_err(
			   "(ofst_meq32: ofst:%u mask:0x%x val:0x%x) ",
			   attrib->offset_meq_32[i].offset,
			   attrib->offset_meq_32[i].mask,
			   attrib->offset_meq_32[i].value);

	for (i = 0; i < attrib->num_ihl_offset_meq_32; i++)
		pr_err(
			"(ihl_ofst_meq32: ofts:%d mask:0x%x val:0x%x) ",
			attrib->ihl_offset_meq_32[i].offset,
			attrib->ihl_offset_meq_32[i].mask,
			attrib->ihl_offset_meq_32[i].value);

	if (attrib->metadata_meq32_present)
		pr_err(
			"(metadata: ofst:%u mask:0x%x val:0x%x) ",
			attrib->metadata_meq32.offset,
			attrib->metadata_meq32.mask,
			attrib->metadata_meq32.value);

	for (i = 0; i < attrib->num_ihl_offset_range_16; i++)
		pr_err(
			   "(ihl_ofst_range16: ofst:%u lo:%u hi:%u) ",
			   attrib->ihl_offset_range_16[i].offset,
			   attrib->ihl_offset_range_16[i].range_low,
			   attrib->ihl_offset_range_16[i].range_high);

	if (attrib->ihl_offset_eq_32_present)
		pr_err(
			"(ihl_ofst_eq32:%d val:0x%x) ",
			attrib->ihl_offset_eq_32.offset,
			attrib->ihl_offset_eq_32.value);

	if (attrib->ihl_offset_eq_16_present)
		pr_err(
			"(ihl_ofst_eq16:%d val:0x%x) ",
			attrib->ihl_offset_eq_16.offset,
			attrib->ihl_offset_eq_16.value);

	if (attrib->fl_eq_present)
		pr_err("flow_label:%d ", attrib->fl_eq);

	if (attrib->ipv4_frag_eq_present)
		pr_err("frag ");

	pr_err("\n");
	return 0;
}

static int ipa3_open_dbg(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t ipa3_read_rt(struct file *file, char __user *ubuf, size_t count,
		loff_t *ppos)
{
	int i = 0;
	struct ipa3_rt_tbl *tbl;
	struct ipa3_rt_entry *entry;
	struct ipa3_rt_tbl_set *set;
	enum ipa_ip_type ip = (enum ipa_ip_type)file->private_data;
	u32 ofst;
	u32 ofst_words;

	set = &ipa3_ctx->rt_tbl_set[ip];

	mutex_lock(&ipa3_ctx->lock);

	if (ip ==  IPA_IP_v6) {
		if (ipa3_ctx->ip6_rt_tbl_hash_lcl)
			pr_err("Hashable table resides on local memory\n");
		else
			pr_err("Hashable table resides on system (ddr) memory\n");
		if (ipa3_ctx->ip6_rt_tbl_nhash_lcl)
			pr_err("Non-Hashable table resides on local memory\n");
		else
			pr_err("Non-Hashable table resides on system (ddr) memory\n");
	} else if (ip == IPA_IP_v4) {
		if (ipa3_ctx->ip4_rt_tbl_hash_lcl)
			pr_err("Hashable table resides on local memory\n");
		else
			pr_err("Hashable table resides on system (ddr) memory\n");
		if (ipa3_ctx->ip4_rt_tbl_nhash_lcl)
			pr_err("Non-Hashable table resides on local memory\n");
		else
			pr_err("Non-Hashable table resides on system (ddr) memory\n");
	}

	list_for_each_entry(tbl, &set->head_rt_tbl_list, link) {
		i = 0;
		list_for_each_entry(entry, &tbl->head_rt_rule_list, link) {
			if (entry->proc_ctx) {
				ofst = entry->proc_ctx->offset_entry->offset;
				ofst_words =
					(ofst +
					ipa3_ctx->hdr_proc_ctx_tbl.start_offset)
					>> 5;

				pr_err("tbl_idx:%d tbl_name:%s tbl_ref:%u ",
					entry->tbl->idx, entry->tbl->name,
					entry->tbl->ref_cnt);
				pr_err("rule_idx:%d dst:%d ep:%d S:%u ",
					i, entry->rule.dst,
					ipa3_get_ep_mapping(entry->rule.dst),
					!ipa3_ctx->hdr_proc_ctx_tbl_lcl);
				pr_err("proc_ctx[32B]:%u attrib_mask:%08x ",
					ofst_words,
					entry->rule.attrib.attrib_mask);
				pr_err("rule_id:%u max_prio:%u prio:%u ",
					entry->rule_id, entry->rule.max_prio,
					entry->prio);
				pr_err("hashable:%u retain_hdr:%u ",
					entry->rule.hashable,
					entry->rule.retain_hdr);
			} else {
				if (entry->hdr)
					ofst = entry->hdr->offset_entry->offset;
				else
					ofst = 0;

				pr_err("tbl_idx:%d tbl_name:%s tbl_ref:%u ",
					entry->tbl->idx, entry->tbl->name,
					entry->tbl->ref_cnt);
				pr_err("rule_idx:%d dst:%d ep:%d S:%u ",
					i, entry->rule.dst,
					ipa3_get_ep_mapping(entry->rule.dst),
					!ipa3_ctx->hdr_tbl_lcl);
				pr_err("hdr_ofst[words]:%u attrib_mask:%08x ",
					ofst >> 2,
					entry->rule.attrib.attrib_mask);
				pr_err("rule_id:%u max_prio:%u prio:%u ",
					entry->rule_id, entry->rule.max_prio,
					entry->prio);
				pr_err("hashable:%u retain_hdr:%u ",
					entry->rule.hashable,
					entry->rule.retain_hdr);
			}

			ipa3_attrib_dump(&entry->rule.attrib, ip);
			i++;
		}
	}
	mutex_unlock(&ipa3_ctx->lock);

	return 0;
}

static ssize_t ipa3_read_rt_hw(struct file *file, char __user *ubuf,
	size_t count, loff_t *ppos)
{
	enum ipa_ip_type ip = (enum ipa_ip_type)file->private_data;
	int tbls_num;
	int rules_num;
	int tbl;
	int rl;
	int res = 0;
	struct ipahal_rt_rule_entry *rules = NULL;

	switch (ip) {
	case IPA_IP_v4:
		tbls_num = IPA_MEM_PART(v4_rt_num_index);
		break;
	case IPA_IP_v6:
		tbls_num = IPA_MEM_PART(v6_rt_num_index);
		break;
	default:
		IPAERR("ip type error %d\n", ip);
		return -EINVAL;
	};

	IPADBG("Tring to parse %d H/W routing tables - IP=%d\n", tbls_num, ip);

	rules = kzalloc(sizeof(*rules) * IPA_DBG_MAX_RULE_IN_TBL, GFP_KERNEL);
	if (!rules) {
		IPAERR("failed to allocate mem for tbl rules\n");
		return -ENOMEM;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	mutex_lock(&ipa3_ctx->lock);

	for (tbl = 0 ; tbl < tbls_num ; tbl++) {
		pr_err("=== Routing Table %d = Hashable Rules ===\n", tbl);
		rules_num = IPA_DBG_MAX_RULE_IN_TBL;
		res = ipa3_rt_read_tbl_from_hw(tbl, ip, true, rules,
			&rules_num);
		if (res) {
			pr_err("ERROR - Check the logs\n");
			IPAERR("failed reading tbl from hw\n");
			goto bail;
		}
		if (!rules_num)
			pr_err("-->No rules. Empty tbl or modem system table\n");

		for (rl = 0 ; rl < rules_num ; rl++) {
			pr_err("rule_idx:%d dst ep:%d L:%u ",
				rl, rules[rl].dst_pipe_idx, rules[rl].hdr_lcl);

			if (rules[rl].hdr_type == IPAHAL_RT_RULE_HDR_PROC_CTX)
				pr_err("proc_ctx:%u attrib_mask:%08x ",
					rules[rl].hdr_ofst,
					rules[rl].eq_attrib.rule_eq_bitmap);
			else
				pr_err("hdr_ofst:%u attrib_mask:%08x ",
					rules[rl].hdr_ofst,
					rules[rl].eq_attrib.rule_eq_bitmap);

			pr_err("rule_id:%u prio:%u retain_hdr:%u ",
				rules[rl].id, rules[rl].priority,
				rules[rl].retain_hdr);
			ipa3_attrib_dump_eq(&rules[rl].eq_attrib);
		}

		pr_err("=== Routing Table %d = Non-Hashable Rules ===\n", tbl);
		rules_num = IPA_DBG_MAX_RULE_IN_TBL;
		res = ipa3_rt_read_tbl_from_hw(tbl, ip, false, rules,
			&rules_num);
		if (res) {
			pr_err("ERROR - Check the logs\n");
			IPAERR("failed reading tbl from hw\n");
			goto bail;
		}
		if (!rules_num)
			pr_err("-->No rules. Empty tbl or modem system table\n");

		for (rl = 0 ; rl < rules_num ; rl++) {
			pr_err("rule_idx:%d dst ep:%d L:%u ",
				rl, rules[rl].dst_pipe_idx, rules[rl].hdr_lcl);

			if (rules[rl].hdr_type == IPAHAL_RT_RULE_HDR_PROC_CTX)
				pr_err("proc_ctx:%u attrib_mask:%08x ",
					rules[rl].hdr_ofst,
					rules[rl].eq_attrib.rule_eq_bitmap);
			else
				pr_err("hdr_ofst:%u attrib_mask:%08x ",
					rules[rl].hdr_ofst,
					rules[rl].eq_attrib.rule_eq_bitmap);

			pr_err("rule_id:%u prio:%u retain_hdr:%u\n",
				rules[rl].id, rules[rl].priority,
				rules[rl].retain_hdr);
			ipa3_attrib_dump_eq(&rules[rl].eq_attrib);
		}
		pr_err("\n");
	}

bail:
	mutex_unlock(&ipa3_ctx->lock);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	kfree(rules);
	return res;
}

static ssize_t ipa3_read_proc_ctx(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes = 0;
	struct ipa3_hdr_proc_ctx_tbl *tbl;
	struct ipa3_hdr_proc_ctx_entry *entry;
	u32 ofst_words;

	tbl = &ipa3_ctx->hdr_proc_ctx_tbl;

	mutex_lock(&ipa3_ctx->lock);

	if (ipa3_ctx->hdr_proc_ctx_tbl_lcl)
		pr_info("Table resides on local memory\n");
	else
		pr_info("Table resides on system(ddr) memory\n");

	list_for_each_entry(entry, &tbl->head_proc_ctx_entry_list, link) {
		ofst_words = (entry->offset_entry->offset +
			ipa3_ctx->hdr_proc_ctx_tbl.start_offset)
			>> 5;
		if (entry->hdr->is_hdr_proc_ctx) {
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"id:%u hdr_proc_type:%s proc_ctx[32B]:%u ",
				entry->id,
				ipa3_hdr_proc_type_name[entry->type],
				ofst_words);
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"hdr_phys_base:0x%pa\n",
				&entry->hdr->phys_base);
		} else {
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"id:%u hdr_proc_type:%s proc_ctx[32B]:%u ",
				entry->id,
				ipa3_hdr_proc_type_name[entry->type],
				ofst_words);
			nbytes += scnprintf(dbg_buff + nbytes,
				IPA_MAX_MSG_LEN - nbytes,
				"hdr[words]:%u\n",
				entry->hdr->offset_entry->offset >> 2);
		}
	}
	mutex_unlock(&ipa3_ctx->lock);

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa3_read_flt(struct file *file, char __user *ubuf, size_t count,
		loff_t *ppos)
{
	int i;
	int j;
	struct ipa3_flt_tbl *tbl;
	struct ipa3_flt_entry *entry;
	enum ipa_ip_type ip = (enum ipa_ip_type)file->private_data;
	struct ipa3_rt_tbl *rt_tbl;
	u32 rt_tbl_idx;
	u32 bitmap;
	bool eq;

	mutex_lock(&ipa3_ctx->lock);

	for (j = 0; j < ipa3_ctx->ipa_num_pipes; j++) {
		if (!ipa_is_ep_support_flt(j))
			continue;
		tbl = &ipa3_ctx->flt_tbl[j][ip];
		i = 0;
		list_for_each_entry(entry, &tbl->head_flt_rule_list, link) {
			if (entry->rule.eq_attrib_type) {
				rt_tbl_idx = entry->rule.rt_tbl_idx;
				bitmap = entry->rule.eq_attrib.rule_eq_bitmap;
				eq = true;
			} else {
				rt_tbl = ipa3_id_find(entry->rule.rt_tbl_hdl);
				if (rt_tbl == NULL ||
					rt_tbl->cookie != IPA_RT_TBL_COOKIE)
					rt_tbl_idx =  ~0;
				else
					rt_tbl_idx = rt_tbl->idx;
				bitmap = entry->rule.attrib.attrib_mask;
				eq = false;
			}
			pr_err("ep_idx:%d rule_idx:%d act:%d rt_tbl_idx:%d ",
				j, i, entry->rule.action, rt_tbl_idx);
			pr_err("attrib_mask:%08x retain_hdr:%d eq:%d ",
				bitmap, entry->rule.retain_hdr, eq);
			pr_err("hashable:%u rule_id:%u max_prio:%u prio:%u ",
				entry->rule.hashable, entry->rule_id,
				entry->rule.max_prio, entry->prio);
			if (eq)
				ipa3_attrib_dump_eq(
					&entry->rule.eq_attrib);
			else
				ipa3_attrib_dump(
					&entry->rule.attrib, ip);
			i++;
		}
	}
	mutex_unlock(&ipa3_ctx->lock);

	return 0;
}

static ssize_t ipa3_read_flt_hw(struct file *file, char __user *ubuf,
	size_t count, loff_t *ppos)
{
	int pipe;
	int rl;
	int rules_num;
	struct ipahal_flt_rule_entry *rules;
	enum ipa_ip_type ip = (enum ipa_ip_type)file->private_data;
	u32 rt_tbl_idx;
	u32 bitmap;
	int res = 0;

	IPADBG("Tring to parse %d H/W filtering tables - IP=%d\n",
		ipa3_ctx->ep_flt_num, ip);

	rules = kzalloc(sizeof(*rules) * IPA_DBG_MAX_RULE_IN_TBL, GFP_KERNEL);
	if (!rules) {
		IPAERR("failed to allocate mem for tbl rules\n");
		return -ENOMEM;
	}

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	mutex_lock(&ipa3_ctx->lock);
	for (pipe = 0; pipe < ipa3_ctx->ipa_num_pipes; pipe++) {
		if (!ipa_is_ep_support_flt(pipe))
			continue;
		pr_err("=== Filtering Table ep:%d = Hashable Rules ===\n",
			pipe);
		rules_num = IPA_DBG_MAX_RULE_IN_TBL;
		res = ipa3_flt_read_tbl_from_hw(pipe, ip, true, rules,
			&rules_num);
		if (res) {
			pr_err("ERROR - Check the logs\n");
			IPAERR("failed reading tbl from hw\n");
			goto bail;
		}
		if (!rules_num)
			pr_err("-->No rules. Empty tbl or modem sys table\n");

		for (rl = 0; rl < rules_num; rl++) {
			rt_tbl_idx = rules[rl].rule.rt_tbl_idx;
			bitmap = rules[rl].rule.eq_attrib.rule_eq_bitmap;
			pr_err("ep_idx:%d rule_idx:%d act:%d rt_tbl_idx:%d ",
				pipe, rl, rules[rl].rule.action, rt_tbl_idx);
			pr_err("attrib_mask:%08x retain_hdr:%d ",
				bitmap, rules[rl].rule.retain_hdr);
			pr_err("rule_id:%u prio:%u ",
				rules[rl].id, rules[rl].priority);
			ipa3_attrib_dump_eq(&rules[rl].rule.eq_attrib);
		}

		pr_err("=== Filtering Table ep:%d = Non-Hashable Rules ===\n",
			pipe);
		rules_num = IPA_DBG_MAX_RULE_IN_TBL;
		res = ipa3_flt_read_tbl_from_hw(pipe, ip, false, rules,
			&rules_num);
		if (res) {
			pr_err("ERROR - Check the logs\n");
			IPAERR("failed reading tbl from hw\n");
			goto bail;
		}
		if (!rules_num)
			pr_err("-->No rules. Empty tbl or modem sys table\n");
		for (rl = 0; rl < rules_num; rl++) {
			rt_tbl_idx = rules[rl].rule.rt_tbl_idx;
			bitmap = rules[rl].rule.eq_attrib.rule_eq_bitmap;
			pr_err("ep_idx:%d rule_idx:%d act:%d rt_tbl_idx:%d ",
				pipe, rl, rules[rl].rule.action, rt_tbl_idx);
			pr_err("attrib_mask:%08x retain_hdr:%d ",
				bitmap, rules[rl].rule.retain_hdr);
			pr_err("rule_id:%u  prio:%u ",
				rules[rl].id, rules[rl].priority);
			ipa3_attrib_dump_eq(&rules[rl].rule.eq_attrib);
		}
		pr_err("\n");
	}

bail:
	mutex_unlock(&ipa3_ctx->lock);
	kfree(rules);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
	return res;
}

static ssize_t ipa3_read_stats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes;
	int i;
	int cnt = 0;
	uint connect = 0;

	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++)
		connect |= (ipa3_ctx->ep[i].valid << i);

	nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
		"sw_tx=%u\n"
		"hw_tx=%u\n"
		"tx_non_linear=%u\n"
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
		"lan_repl_rx_empty=%u\n"
		"flow_enable=%u\n"
		"flow_disable=%u\n",
		ipa3_ctx->stats.tx_sw_pkts,
		ipa3_ctx->stats.tx_hw_pkts,
		ipa3_ctx->stats.tx_non_linear,
		ipa3_ctx->stats.tx_pkts_compl,
		ipa3_ctx->stats.rx_pkts,
		ipa3_ctx->stats.stat_compl,
		ipa3_ctx->stats.aggr_close,
		ipa3_ctx->stats.wan_aggr_close,
		ipa3_ctx->ipa3_active_clients.cnt,
		connect,
		ipa3_ctx->stats.wan_rx_empty,
		ipa3_ctx->stats.wan_repl_rx_empty,
		ipa3_ctx->stats.lan_rx_empty,
		ipa3_ctx->stats.lan_repl_rx_empty,
		ipa3_ctx->stats.flow_enable,
		ipa3_ctx->stats.flow_disable);
	cnt += nbytes;

	for (i = 0; i < IPAHAL_PKT_STATUS_EXCEPTION_MAX; i++) {
		nbytes = scnprintf(dbg_buff + cnt,
			IPA_MAX_MSG_LEN - cnt,
			"lan_rx_excp[%u:%20s]=%u\n", i,
			ipahal_pkt_status_exception_str(i),
			ipa3_ctx->stats.rx_excp_pkts[i]);
		cnt += nbytes;
	}

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
}

static ssize_t ipa3_read_wstats(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{

#define HEAD_FRMT_STR "%25s\n"
#define FRMT_STR "%25s %10u\n"
#define FRMT_STR1 "%25s %10u\n\n"

	int cnt = 0;
	int nbytes;
	int ipa_ep_idx;
	enum ipa_client_type client = IPA_CLIENT_WLAN1_PROD;
	struct ipa3_ep_context *ep;

	do {
		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
			HEAD_FRMT_STR, "Client IPA_CLIENT_WLAN1_PROD Stats:");
		cnt += nbytes;

		ipa_ep_idx = ipa3_get_ep_mapping(client);
		if (ipa_ep_idx == -1) {
			nbytes = scnprintf(dbg_buff + cnt,
				IPA_MAX_MSG_LEN - cnt, HEAD_FRMT_STR, "Not up");
			cnt += nbytes;
			break;
		}

		ep = &ipa3_ctx->ep[ipa_ep_idx];
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
		ipa_ep_idx = ipa3_get_ep_mapping(client);
		if (ipa_ep_idx == -1) {
			nbytes = scnprintf(dbg_buff + cnt,
				IPA_MAX_MSG_LEN - cnt, HEAD_FRMT_STR, "Not up");
			cnt += nbytes;
			goto nxt_clnt_cons;
		}

		ep = &ipa3_ctx->ep[ipa_ep_idx];
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
		ipa3_ctx->wc_memb.wlan_comm_total_cnt);
	cnt += nbytes;

	nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt, FRMT_STR,
		"Tx Comm Buff Avail:", ipa3_ctx->wc_memb.wlan_comm_free_cnt);
	cnt += nbytes;

	nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt, FRMT_STR1,
		"Total Tx Pkts Freed:", ipa3_ctx->wc_memb.total_tx_pkts_freed);
	cnt += nbytes;

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
}

static ssize_t ipa3_read_ntn(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
#define TX_STATS(y) \
	ipa3_ctx->uc_ntn_ctx.ntn_uc_stats_mmio->tx_ch_stats[0].y
#define RX_STATS(y) \
	ipa3_ctx->uc_ntn_ctx.ntn_uc_stats_mmio->rx_ch_stats[0].y

	struct Ipa3HwStatsNTNInfoData_t stats;
	int nbytes;
	int cnt = 0;

	if (!ipa3_get_ntn_stats(&stats)) {
		nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
			"TX num_pkts_processed=%u\n"
			"TX tail_ptr_val=%u\n"
			"TX num_db_fired=%u\n"
			"TX ringFull=%u\n"
			"TX ringEmpty=%u\n"
			"TX ringUsageHigh=%u\n"
			"TX ringUsageLow=%u\n"
			"TX RingUtilCount=%u\n"
			"TX bamFifoFull=%u\n"
			"TX bamFifoEmpty=%u\n"
			"TX bamFifoUsageHigh=%u\n"
			"TX bamFifoUsageLow=%u\n"
			"TX bamUtilCount=%u\n"
			"TX num_db=%u\n"
			"TX num_unexpected_db=%u\n"
			"TX num_bam_int_handled=%u\n"
			"TX num_bam_int_in_non_running_state=%u\n"
			"TX num_qmb_int_handled=%u\n"
			"TX num_bam_int_handled_while_wait_for_bam=%u\n"
			"TX num_bam_int_handled_while_not_in_bam=%u\n",
			TX_STATS(num_pkts_processed),
			TX_STATS(tail_ptr_val),
			TX_STATS(num_db_fired),
			TX_STATS(tx_comp_ring_stats.ringFull),
			TX_STATS(tx_comp_ring_stats.ringEmpty),
			TX_STATS(tx_comp_ring_stats.ringUsageHigh),
			TX_STATS(tx_comp_ring_stats.ringUsageLow),
			TX_STATS(tx_comp_ring_stats.RingUtilCount),
			TX_STATS(bam_stats.bamFifoFull),
			TX_STATS(bam_stats.bamFifoEmpty),
			TX_STATS(bam_stats.bamFifoUsageHigh),
			TX_STATS(bam_stats.bamFifoUsageLow),
			TX_STATS(bam_stats.bamUtilCount),
			TX_STATS(num_db),
			TX_STATS(num_unexpected_db),
			TX_STATS(num_bam_int_handled),
			TX_STATS(num_bam_int_in_non_running_state),
			TX_STATS(num_qmb_int_handled),
			TX_STATS(num_bam_int_handled_while_wait_for_bam),
			TX_STATS(num_bam_int_handled_while_not_in_bam));
		cnt += nbytes;
		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
			"RX max_outstanding_pkts=%u\n"
			"RX num_pkts_processed=%u\n"
			"RX rx_ring_rp_value=%u\n"
			"RX ringFull=%u\n"
			"RX ringEmpty=%u\n"
			"RX ringUsageHigh=%u\n"
			"RX ringUsageLow=%u\n"
			"RX RingUtilCount=%u\n"
			"RX bamFifoFull=%u\n"
			"RX bamFifoEmpty=%u\n"
			"RX bamFifoUsageHigh=%u\n"
			"RX bamFifoUsageLow=%u\n"
			"RX bamUtilCount=%u\n"
			"RX num_bam_int_handled=%u\n"
			"RX num_db=%u\n"
			"RX num_unexpected_db=%u\n"
			"RX num_pkts_in_dis_uninit_state=%u\n"
			"num_ic_inj_vdev_change=%u\n"
			"num_ic_inj_fw_desc_change=%u\n",
			RX_STATS(max_outstanding_pkts),
			RX_STATS(num_pkts_processed),
			RX_STATS(rx_ring_rp_value),
			RX_STATS(rx_ind_ring_stats.ringFull),
			RX_STATS(rx_ind_ring_stats.ringEmpty),
			RX_STATS(rx_ind_ring_stats.ringUsageHigh),
			RX_STATS(rx_ind_ring_stats.ringUsageLow),
			RX_STATS(rx_ind_ring_stats.RingUtilCount),
			RX_STATS(bam_stats.bamFifoFull),
			RX_STATS(bam_stats.bamFifoEmpty),
			RX_STATS(bam_stats.bamFifoUsageHigh),
			RX_STATS(bam_stats.bamFifoUsageLow),
			RX_STATS(bam_stats.bamUtilCount),
			RX_STATS(num_bam_int_handled),
			RX_STATS(num_db),
			RX_STATS(num_unexpected_db),
			RX_STATS(num_pkts_in_dis_uninit_state),
			RX_STATS(num_bam_int_handled_while_not_in_bam),
			RX_STATS(num_bam_int_handled_while_in_bam_state));
		cnt += nbytes;
	} else {
		nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
				"Fail to read NTN stats\n");
		cnt += nbytes;
	}

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
}

static ssize_t ipa3_read_wdi(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct IpaHwStatsWDIInfoData_t stats;
	int nbytes;
	int cnt = 0;

	if (!ipa3_get_wdi_stats(&stats)) {
		nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
			"TX num_pkts_processed=%u\n"
			"TX copy_engine_doorbell_value=%u\n"
			"TX num_db_fired=%u\n"
			"TX ringFull=%u\n"
			"TX ringEmpty=%u\n"
			"TX ringUsageHigh=%u\n"
			"TX ringUsageLow=%u\n"
			"TX RingUtilCount=%u\n"
			"TX bamFifoFull=%u\n"
			"TX bamFifoEmpty=%u\n"
			"TX bamFifoUsageHigh=%u\n"
			"TX bamFifoUsageLow=%u\n"
			"TX bamUtilCount=%u\n"
			"TX num_db=%u\n"
			"TX num_unexpected_db=%u\n"
			"TX num_bam_int_handled=%u\n"
			"TX num_bam_int_in_non_runnning_state=%u\n"
			"TX num_qmb_int_handled=%u\n"
			"TX num_bam_int_handled_while_wait_for_bam=%u\n",
			stats.tx_ch_stats.num_pkts_processed,
			stats.tx_ch_stats.copy_engine_doorbell_value,
			stats.tx_ch_stats.num_db_fired,
			stats.tx_ch_stats.tx_comp_ring_stats.ringFull,
			stats.tx_ch_stats.tx_comp_ring_stats.ringEmpty,
			stats.tx_ch_stats.tx_comp_ring_stats.ringUsageHigh,
			stats.tx_ch_stats.tx_comp_ring_stats.ringUsageLow,
			stats.tx_ch_stats.tx_comp_ring_stats.RingUtilCount,
			stats.tx_ch_stats.bam_stats.bamFifoFull,
			stats.tx_ch_stats.bam_stats.bamFifoEmpty,
			stats.tx_ch_stats.bam_stats.bamFifoUsageHigh,
			stats.tx_ch_stats.bam_stats.bamFifoUsageLow,
			stats.tx_ch_stats.bam_stats.bamUtilCount,
			stats.tx_ch_stats.num_db,
			stats.tx_ch_stats.num_unexpected_db,
			stats.tx_ch_stats.num_bam_int_handled,
			stats.tx_ch_stats.num_bam_int_in_non_runnning_state,
			stats.tx_ch_stats.num_qmb_int_handled,
			stats.tx_ch_stats.
				num_bam_int_handled_while_wait_for_bam);
		cnt += nbytes;
		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
			"RX max_outstanding_pkts=%u\n"
			"RX num_pkts_processed=%u\n"
			"RX rx_ring_rp_value=%u\n"
			"RX ringFull=%u\n"
			"RX ringEmpty=%u\n"
			"RX ringUsageHigh=%u\n"
			"RX ringUsageLow=%u\n"
			"RX RingUtilCount=%u\n"
			"RX bamFifoFull=%u\n"
			"RX bamFifoEmpty=%u\n"
			"RX bamFifoUsageHigh=%u\n"
			"RX bamFifoUsageLow=%u\n"
			"RX bamUtilCount=%u\n"
			"RX num_bam_int_handled=%u\n"
			"RX num_db=%u\n"
			"RX num_unexpected_db=%u\n"
			"RX num_pkts_in_dis_uninit_state=%u\n"
			"RX num_ic_inj_vdev_change=%u\n"
			"RX num_ic_inj_fw_desc_change=%u\n"
			"RX num_qmb_int_handled=%u\n"
			"RX reserved1=%u\n"
			"RX reserved2=%u\n",
			stats.rx_ch_stats.max_outstanding_pkts,
			stats.rx_ch_stats.num_pkts_processed,
			stats.rx_ch_stats.rx_ring_rp_value,
			stats.rx_ch_stats.rx_ind_ring_stats.ringFull,
			stats.rx_ch_stats.rx_ind_ring_stats.ringEmpty,
			stats.rx_ch_stats.rx_ind_ring_stats.ringUsageHigh,
			stats.rx_ch_stats.rx_ind_ring_stats.ringUsageLow,
			stats.rx_ch_stats.rx_ind_ring_stats.RingUtilCount,
			stats.rx_ch_stats.bam_stats.bamFifoFull,
			stats.rx_ch_stats.bam_stats.bamFifoEmpty,
			stats.rx_ch_stats.bam_stats.bamFifoUsageHigh,
			stats.rx_ch_stats.bam_stats.bamFifoUsageLow,
			stats.rx_ch_stats.bam_stats.bamUtilCount,
			stats.rx_ch_stats.num_bam_int_handled,
			stats.rx_ch_stats.num_db,
			stats.rx_ch_stats.num_unexpected_db,
			stats.rx_ch_stats.num_pkts_in_dis_uninit_state,
			stats.rx_ch_stats.num_ic_inj_vdev_change,
			stats.rx_ch_stats.num_ic_inj_fw_desc_change,
			stats.rx_ch_stats.num_qmb_int_handled,
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

static ssize_t ipa3_write_dbg_cnt(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	unsigned long missing;
	u32 option = 0;
	struct ipahal_reg_debug_cnt_ctrl dbg_cnt_ctrl;

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, buf, count);
	if (missing)
		return -EFAULT;

	dbg_buff[count] = '\0';
	if (kstrtou32(dbg_buff, 0, &option))
		return -EFAULT;

	memset(&dbg_cnt_ctrl, 0, sizeof(dbg_cnt_ctrl));
	dbg_cnt_ctrl.type = DBG_CNT_TYPE_GENERAL;
	dbg_cnt_ctrl.product = true;
	dbg_cnt_ctrl.src_pipe = 0xff;
	dbg_cnt_ctrl.rule_idx_pipe_rule = false;
	dbg_cnt_ctrl.rule_idx = 0;
	if (option == 1)
		dbg_cnt_ctrl.en = true;
	else
		dbg_cnt_ctrl.en = false;

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	ipahal_write_reg_n_fields(IPA_DEBUG_CNT_CTRL_n, 0, &dbg_cnt_ctrl);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return count;
}

static ssize_t ipa3_read_dbg_cnt(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes;
	u32 regval;

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	regval =
		ipahal_read_reg_n(IPA_DEBUG_CNT_REG_n, 0);
	nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
			"IPA_DEBUG_CNT_REG_0=0x%x\n", regval);
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa3_read_msg(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes;
	int cnt = 0;
	int i;

	for (i = 0; i < IPA_EVENT_MAX_NUM; i++) {
		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
				"msg[%u:%27s] W:%u R:%u\n", i,
				ipa3_event_name[i],
				ipa3_ctx->stats.msg_w[i],
				ipa3_ctx->stats.msg_r[i]);
		cnt += nbytes;
	}

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
}

static ssize_t ipa3_read_nat4(struct file *file,
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
	u32 no_entrys = 0;

	mutex_lock(&ipa3_ctx->nat_mem.lock);
	value = ipa3_ctx->nat_mem.public_ip_addr;
	pr_err(
				"Table IP Address:%d.%d.%d.%d\n",
				((value & 0xFF000000) >> 24),
				((value & 0x00FF0000) >> 16),
				((value & 0x0000FF00) >> 8),
				((value & 0x000000FF)));

	pr_err("Table Size:%d\n",
				ipa3_ctx->nat_mem.size_base_tables);

	if (!ipa3_ctx->nat_mem.size_expansion_tables)
		pr_err("Expansion Table Size:%d\n",
				ipa3_ctx->nat_mem.size_expansion_tables);
	else
		pr_err("Expansion Table Size:%d\n",
				ipa3_ctx->nat_mem.size_expansion_tables-1);

	if (!ipa3_ctx->nat_mem.is_sys_mem)
		pr_err("Not supported for local(shared) memory\n");

	/* Print Base tables */
	rule_id = 0;
	for (j = 0; j < 2; j++) {
		if (j == BASE_TABLE) {
			tbl_size = ipa3_ctx->nat_mem.size_base_tables;
			base_tbl = (u32 *)ipa3_ctx->nat_mem.ipv4_rules_addr;

			pr_err("\nBase Table:\n");
		} else {
			if (!ipa3_ctx->nat_mem.size_expansion_tables)
				continue;
			tbl_size = ipa3_ctx->nat_mem.size_expansion_tables-1;
			base_tbl =
			 (u32 *)ipa3_ctx->nat_mem.ipv4_expansion_rules_addr;

			pr_err("\nExpansion Base Table:\n");
		}

		if (base_tbl != NULL) {
			for (i = 0; i <= tbl_size; i++, rule_id++) {
				tmp = base_tbl;
				value = tmp[4];
				enable = ((value & 0xFFFF0000) >> 16);

				if (enable & NAT_ENTRY_ENABLE) {
					no_entrys++;
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
			tbl_size = ipa3_ctx->nat_mem.size_base_tables;
			indx_tbl = (u32 *)ipa3_ctx->nat_mem.index_table_addr;

			pr_err("\nIndex Table:\n");
		} else {
			if (!ipa3_ctx->nat_mem.size_expansion_tables)
				continue;
			tbl_size = ipa3_ctx->nat_mem.size_expansion_tables-1;
			indx_tbl =
			 (u32 *)ipa3_ctx->nat_mem.index_table_expansion_addr;

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
	pr_err("Current No. Nat Entries: %d\n", no_entrys);
	mutex_unlock(&ipa3_ctx->nat_mem.lock);

	return 0;
}

static ssize_t ipa3_rm_read_stats(struct file *file, char __user *ubuf,
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

static void ipa_dump_status(struct ipahal_pkt_status *status)
{
	IPA_DUMP_STATUS_FIELD(status_opcode);
	IPA_DUMP_STATUS_FIELD(exception);
	IPA_DUMP_STATUS_FIELD(status_mask);
	IPA_DUMP_STATUS_FIELD(pkt_len);
	IPA_DUMP_STATUS_FIELD(endp_src_idx);
	IPA_DUMP_STATUS_FIELD(endp_dest_idx);
	IPA_DUMP_STATUS_FIELD(metadata);
	IPA_DUMP_STATUS_FIELD(flt_local);
	IPA_DUMP_STATUS_FIELD(flt_hash);
	IPA_DUMP_STATUS_FIELD(flt_global);
	IPA_DUMP_STATUS_FIELD(flt_ret_hdr);
	IPA_DUMP_STATUS_FIELD(flt_miss);
	IPA_DUMP_STATUS_FIELD(flt_rule_id);
	IPA_DUMP_STATUS_FIELD(rt_local);
	IPA_DUMP_STATUS_FIELD(rt_hash);
	IPA_DUMP_STATUS_FIELD(ucp);
	IPA_DUMP_STATUS_FIELD(rt_tbl_idx);
	IPA_DUMP_STATUS_FIELD(rt_miss);
	IPA_DUMP_STATUS_FIELD(rt_rule_id);
	IPA_DUMP_STATUS_FIELD(nat_hit);
	IPA_DUMP_STATUS_FIELD(nat_entry_idx);
	IPA_DUMP_STATUS_FIELD(nat_type);
	pr_err("tag = 0x%llx\n", (u64)status->tag_info & 0xFFFFFFFFFFFF);
	IPA_DUMP_STATUS_FIELD(seq_num);
	IPA_DUMP_STATUS_FIELD(time_of_day_ctr);
	IPA_DUMP_STATUS_FIELD(hdr_local);
	IPA_DUMP_STATUS_FIELD(hdr_offset);
	IPA_DUMP_STATUS_FIELD(frag_hit);
	IPA_DUMP_STATUS_FIELD(frag_rule);
}

static ssize_t ipa_status_stats_read(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	struct ipa3_status_stats *stats;
	int i, j;

	stats = kzalloc(sizeof(*stats), GFP_KERNEL);
	if (!stats)
		return -EFAULT;

	for (i = 0; i < ipa3_ctx->ipa_num_pipes; i++) {
		if (!ipa3_ctx->ep[i].sys || !ipa3_ctx->ep[i].sys->status_stat)
			continue;

		memcpy(stats, ipa3_ctx->ep[i].sys->status_stat, sizeof(*stats));
		pr_err("Statuses for pipe %d\n", i);
		for (j = 0; j < IPA_MAX_STATUS_STAT_NUM; j++) {
			pr_err("curr=%d\n", stats->curr);
			ipa_dump_status(&stats->status[stats->curr]);
			pr_err("\n\n\n");
			stats->curr = (stats->curr + 1) %
				IPA_MAX_STATUS_STAT_NUM;
		}
	}

	kfree(stats);
	return 0;
}

static ssize_t ipa3_print_active_clients_log(struct file *file,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int cnt;
	int table_size;

	if (active_clients_buf == NULL) {
		IPAERR("Active Clients buffer is not allocated");
		return 0;
	}
	memset(active_clients_buf, 0, IPA_DBG_ACTIVE_CLIENT_BUF_SIZE);
	ipa3_active_clients_lock();
	cnt = ipa3_active_clients_log_print_buffer(active_clients_buf,
			IPA_DBG_ACTIVE_CLIENT_BUF_SIZE - IPA_MAX_MSG_LEN);
	table_size = ipa3_active_clients_log_print_table(active_clients_buf
			+ cnt, IPA_MAX_MSG_LEN);
	ipa3_active_clients_unlock();

	return simple_read_from_buffer(ubuf, count, ppos,
			active_clients_buf, cnt + table_size);
}

static ssize_t ipa3_clear_active_clients_log(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	unsigned long missing;
		s8 option = 0;

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, ubuf, count);
	if (missing)
		return -EFAULT;

	dbg_buff[count] = '\0';
	if (kstrtos8(dbg_buff, 0, &option))
		return -EFAULT;

	ipa3_active_clients_log_clear();

	return count;
}

static ssize_t ipa3_enable_ipc_low(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	unsigned long missing;
	s8 option = 0;

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, ubuf, count);
	if (missing)
		return -EFAULT;

	dbg_buff[count] = '\0';
	if (kstrtos8(dbg_buff, 0, &option))
		return -EFAULT;

	mutex_lock(&ipa3_ctx->lock);
	if (option) {
		if (!ipa_ipc_low_buff) {
			ipa_ipc_low_buff =
				ipc_log_context_create(IPA_IPC_LOG_PAGES,
					"ipa_low", 0);
		}
			if (ipa_ipc_low_buff == NULL)
				IPAERR("failed to get logbuf_low\n");
		ipa3_ctx->logbuf_low = ipa_ipc_low_buff;
	} else {
		ipa3_ctx->logbuf_low = NULL;
	}
	mutex_unlock(&ipa3_ctx->lock);

	return count;
}

const struct file_operations ipa3_gen_reg_ops = {
	.read = ipa3_read_gen_reg,
};

const struct file_operations ipa3_ep_reg_ops = {
	.read = ipa3_read_ep_reg,
	.write = ipa3_write_ep_reg,
};

const struct file_operations ipa3_keep_awake_ops = {
	.read = ipa3_read_keep_awake,
	.write = ipa3_write_keep_awake,
};

const struct file_operations ipa3_ep_holb_ops = {
	.write = ipa3_write_ep_holb,
};

const struct file_operations ipa3_hdr_ops = {
	.read = ipa3_read_hdr,
};

const struct file_operations ipa3_rt_ops = {
	.read = ipa3_read_rt,
	.open = ipa3_open_dbg,
};

const struct file_operations ipa3_rt_hw_ops = {
	.read = ipa3_read_rt_hw,
	.open = ipa3_open_dbg,
};

const struct file_operations ipa3_proc_ctx_ops = {
	.read = ipa3_read_proc_ctx,
};

const struct file_operations ipa3_flt_ops = {
	.read = ipa3_read_flt,
	.open = ipa3_open_dbg,
};

const struct file_operations ipa3_flt_hw_ops = {
	.read = ipa3_read_flt_hw,
	.open = ipa3_open_dbg,
};

const struct file_operations ipa3_stats_ops = {
	.read = ipa3_read_stats,
};

const struct file_operations ipa3_wstats_ops = {
	.read = ipa3_read_wstats,
};

const struct file_operations ipa3_wdi_ops = {
	.read = ipa3_read_wdi,
};

const struct file_operations ipa3_ntn_ops = {
	.read = ipa3_read_ntn,
};

const struct file_operations ipa3_msg_ops = {
	.read = ipa3_read_msg,
};

const struct file_operations ipa3_dbg_cnt_ops = {
	.read = ipa3_read_dbg_cnt,
	.write = ipa3_write_dbg_cnt,
};

const struct file_operations ipa3_status_stats_ops = {
	.read = ipa_status_stats_read,
};

const struct file_operations ipa3_nat4_ops = {
	.read = ipa3_read_nat4,
};

const struct file_operations ipa3_rm_stats = {
	.read = ipa3_rm_read_stats,
};

const struct file_operations ipa3_active_clients = {
	.read = ipa3_print_active_clients_log,
	.write = ipa3_clear_active_clients_log,
};

const struct file_operations ipa3_ipc_low_ops = {
	.write = ipa3_enable_ipc_low,
};

void ipa3_debugfs_init(void)
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
			dent, &ipa3_ctx->ipa_hw_type);
	if (!file) {
		IPAERR("could not create hw_type file\n");
		goto fail;
	}


	dfile_gen_reg = debugfs_create_file("gen_reg", read_only_mode, dent, 0,
			&ipa3_gen_reg_ops);
	if (!dfile_gen_reg || IS_ERR(dfile_gen_reg)) {
		IPAERR("fail to create file for debug_fs gen_reg\n");
		goto fail;
	}

	dfile_active_clients = debugfs_create_file("active_clients",
			read_write_mode, dent, 0, &ipa3_active_clients);
	if (!dfile_active_clients || IS_ERR(dfile_active_clients)) {
		IPAERR("fail to create file for debug_fs active_clients\n");
		goto fail;
	}

	active_clients_buf = NULL;
	active_clients_buf = kzalloc(IPA_DBG_ACTIVE_CLIENT_BUF_SIZE,
			GFP_KERNEL);
	if (active_clients_buf == NULL)
		IPAERR("fail to allocate active clients memory buffer");

	dfile_ep_reg = debugfs_create_file("ep_reg", read_write_mode, dent, 0,
			&ipa3_ep_reg_ops);
	if (!dfile_ep_reg || IS_ERR(dfile_ep_reg)) {
		IPAERR("fail to create file for debug_fs ep_reg\n");
		goto fail;
	}

	dfile_keep_awake = debugfs_create_file("keep_awake", read_write_mode,
			dent, 0, &ipa3_keep_awake_ops);
	if (!dfile_keep_awake || IS_ERR(dfile_keep_awake)) {
		IPAERR("fail to create file for debug_fs dfile_keep_awake\n");
		goto fail;
	}

	dfile_ep_holb = debugfs_create_file("holb", write_only_mode, dent,
			0, &ipa3_ep_holb_ops);
	if (!dfile_ep_holb || IS_ERR(dfile_ep_holb)) {
		IPAERR("fail to create file for debug_fs dfile_ep_hol_en\n");
		goto fail;
	}

	dfile_hdr = debugfs_create_file("hdr", read_only_mode, dent, 0,
			&ipa3_hdr_ops);
	if (!dfile_hdr || IS_ERR(dfile_hdr)) {
		IPAERR("fail to create file for debug_fs hdr\n");
		goto fail;
	}

	dfile_proc_ctx = debugfs_create_file("proc_ctx", read_only_mode, dent,
		0, &ipa3_proc_ctx_ops);
	if (!dfile_hdr || IS_ERR(dfile_hdr)) {
		IPAERR("fail to create file for debug_fs proc_ctx\n");
		goto fail;
	}

	dfile_ip4_rt = debugfs_create_file("ip4_rt", read_only_mode, dent,
			(void *)IPA_IP_v4, &ipa3_rt_ops);
	if (!dfile_ip4_rt || IS_ERR(dfile_ip4_rt)) {
		IPAERR("fail to create file for debug_fs ip4 rt\n");
		goto fail;
	}

	dfile_ip4_rt_hw = debugfs_create_file("ip4_rt_hw", read_only_mode, dent,
		(void *)IPA_IP_v4, &ipa3_rt_hw_ops);
	if (!dfile_ip4_rt_hw || IS_ERR(dfile_ip4_rt_hw)) {
		IPAERR("fail to create file for debug_fs ip4 rt hw\n");
		goto fail;
	}

	dfile_ip6_rt = debugfs_create_file("ip6_rt", read_only_mode, dent,
			(void *)IPA_IP_v6, &ipa3_rt_ops);
	if (!dfile_ip6_rt || IS_ERR(dfile_ip6_rt)) {
		IPAERR("fail to create file for debug_fs ip6:w" " rt\n");
		goto fail;
	}

	dfile_ip6_rt_hw = debugfs_create_file("ip6_rt_hw", read_only_mode, dent,
		(void *)IPA_IP_v6, &ipa3_rt_hw_ops);
	if (!dfile_ip6_rt_hw || IS_ERR(dfile_ip6_rt_hw)) {
		IPAERR("fail to create file for debug_fs ip6 rt hw\n");
		goto fail;
	}

	dfile_ip4_flt = debugfs_create_file("ip4_flt", read_only_mode, dent,
			(void *)IPA_IP_v4, &ipa3_flt_ops);
	if (!dfile_ip4_flt || IS_ERR(dfile_ip4_flt)) {
		IPAERR("fail to create file for debug_fs ip4 flt\n");
		goto fail;
	}

	dfile_ip4_flt_hw = debugfs_create_file("ip4_flt_hw", read_only_mode,
			dent, (void *)IPA_IP_v4, &ipa3_flt_hw_ops);
	if (!dfile_ip4_flt_hw || IS_ERR(dfile_ip4_flt_hw)) {
		IPAERR("fail to create file for debug_fs ip4 flt\n");
		goto fail;
	}

	dfile_ip6_flt = debugfs_create_file("ip6_flt", read_only_mode, dent,
			(void *)IPA_IP_v6, &ipa3_flt_ops);
	if (!dfile_ip6_flt || IS_ERR(dfile_ip6_flt)) {
		IPAERR("fail to create file for debug_fs ip6 flt\n");
		goto fail;
	}

	dfile_ip6_flt_hw = debugfs_create_file("ip6_flt_hw", read_only_mode,
			dent, (void *)IPA_IP_v6, &ipa3_flt_hw_ops);
	if (!dfile_ip6_flt_hw || IS_ERR(dfile_ip6_flt_hw)) {
		IPAERR("fail to create file for debug_fs ip6 flt\n");
		goto fail;
	}

	dfile_stats = debugfs_create_file("stats", read_only_mode, dent, 0,
			&ipa3_stats_ops);
	if (!dfile_stats || IS_ERR(dfile_stats)) {
		IPAERR("fail to create file for debug_fs stats\n");
		goto fail;
	}

	dfile_wstats = debugfs_create_file("wstats", read_only_mode,
			dent, 0, &ipa3_wstats_ops);
	if (!dfile_wstats || IS_ERR(dfile_wstats)) {
		IPAERR("fail to create file for debug_fs wstats\n");
		goto fail;
	}

	dfile_wdi_stats = debugfs_create_file("wdi", read_only_mode, dent, 0,
			&ipa3_wdi_ops);
	if (!dfile_wdi_stats || IS_ERR(dfile_wdi_stats)) {
		IPAERR("fail to create file for debug_fs wdi stats\n");
		goto fail;
	}

	dfile_ntn_stats = debugfs_create_file("ntn", read_only_mode, dent, 0,
			&ipa3_ntn_ops);
	if (!dfile_ntn_stats || IS_ERR(dfile_ntn_stats)) {
		IPAERR("fail to create file for debug_fs ntn stats\n");
		goto fail;
	}

	dfile_dbg_cnt = debugfs_create_file("dbg_cnt", read_write_mode, dent, 0,
			&ipa3_dbg_cnt_ops);
	if (!dfile_dbg_cnt || IS_ERR(dfile_dbg_cnt)) {
		IPAERR("fail to create file for debug_fs dbg_cnt\n");
		goto fail;
	}

	dfile_msg = debugfs_create_file("msg", read_only_mode, dent, 0,
			&ipa3_msg_ops);
	if (!dfile_msg || IS_ERR(dfile_msg)) {
		IPAERR("fail to create file for debug_fs msg\n");
		goto fail;
	}

	dfile_ip4_nat = debugfs_create_file("ip4_nat", read_only_mode, dent,
			0, &ipa3_nat4_ops);
	if (!dfile_ip4_nat || IS_ERR(dfile_ip4_nat)) {
		IPAERR("fail to create file for debug_fs ip4 nat\n");
		goto fail;
	}

	dfile_rm_stats = debugfs_create_file("rm_stats",
			read_only_mode, dent, 0, &ipa3_rm_stats);
	if (!dfile_rm_stats || IS_ERR(dfile_rm_stats)) {
		IPAERR("fail to create file for debug_fs rm_stats\n");
		goto fail;
	}

	dfile_status_stats = debugfs_create_file("status_stats",
			read_only_mode, dent, 0, &ipa3_status_stats_ops);
	if (!dfile_status_stats || IS_ERR(dfile_status_stats)) {
		IPAERR("fail to create file for debug_fs status_stats\n");
		goto fail;
	}

	file = debugfs_create_u32("enable_clock_scaling", read_write_mode,
		dent, &ipa3_ctx->enable_clock_scaling);
	if (!file) {
		IPAERR("could not create enable_clock_scaling file\n");
		goto fail;
	}

	file = debugfs_create_u32("clock_scaling_bw_threshold_nominal_mbps",
		read_write_mode, dent,
		&ipa3_ctx->ctrl->clock_scaling_bw_threshold_nominal);
	if (!file) {
		IPAERR("could not create bw_threshold_nominal_mbps\n");
		goto fail;
	}

	file = debugfs_create_u32("clock_scaling_bw_threshold_turbo_mbps",
		read_write_mode, dent,
		&ipa3_ctx->ctrl->clock_scaling_bw_threshold_turbo);
	if (!file) {
		IPAERR("could not create bw_threshold_turbo_mbps\n");
		goto fail;
	}

	file = debugfs_create_file("enable_low_prio_print", write_only_mode,
		dent, 0, &ipa3_ipc_low_ops);
	if (!file) {
		IPAERR("could not create enable_low_prio_print file\n");
		goto fail;
	}

	return;

fail:
	debugfs_remove_recursive(dent);
}

void ipa3_debugfs_remove(void)
{
	if (IS_ERR(dent)) {
		IPAERR("ipa3_debugfs_remove: folder was not created.\n");
		return;
	}
	if (active_clients_buf != NULL) {
		kfree(active_clients_buf);
		active_clients_buf = NULL;
	}
	debugfs_remove_recursive(dent);
}

struct dentry *ipa_debugfs_get_root(void)
{
	return dent;
}
EXPORT_SYMBOL(ipa_debugfs_get_root);

#else /* !CONFIG_DEBUG_FS */
void ipa3_debugfs_init(void) {}
void ipa3_debugfs_remove(void) {}
#endif
