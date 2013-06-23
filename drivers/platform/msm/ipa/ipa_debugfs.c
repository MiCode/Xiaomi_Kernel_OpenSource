/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/stringify.h>
#include "ipa_i.h"
#include "ipa_rm_i.h"

#define IPA_MAX_MSG_LEN 4096
#define IPA_DBG_CNTR_ON 127265
#define IPA_DBG_CNTR_OFF 127264

const char *ipa_client_name[] = {
	__stringify(IPA_CLIENT_HSIC1_PROD),
	__stringify(IPA_CLIENT_HSIC2_PROD),
	__stringify(IPA_CLIENT_HSIC3_PROD),
	__stringify(IPA_CLIENT_HSIC4_PROD),
	__stringify(IPA_CLIENT_HSIC5_PROD),
	__stringify(IPA_CLIENT_USB_PROD),
	__stringify(IPA_CLIENT_A5_WLAN_AMPDU_PROD),
	__stringify(IPA_CLIENT_A2_EMBEDDED_PROD),
	__stringify(IPA_CLIENT_A2_TETHERED_PROD),
	__stringify(IPA_CLIENT_A5_LAN_WAN_PROD),
	__stringify(IPA_CLIENT_A5_CMD_PROD),
	__stringify(IPA_CLIENT_Q6_LAN_PROD),
	__stringify(IPA_CLIENT_HSIC1_CONS),
	__stringify(IPA_CLIENT_HSIC2_CONS),
	__stringify(IPA_CLIENT_HSIC3_CONS),
	__stringify(IPA_CLIENT_HSIC4_CONS),
	__stringify(IPA_CLIENT_HSIC5_CONS),
	__stringify(IPA_CLIENT_USB_CONS),
	__stringify(IPA_CLIENT_A2_EMBEDDED_CONS),
	__stringify(IPA_CLIENT_A2_TETHERED_CONS),
	__stringify(IPA_CLIENT_A5_LAN_WAN_CONS),
	__stringify(IPA_CLIENT_Q6_LAN_CONS),
	__stringify(IPA_CLIENT_MAX),
};

const char *ipa_ic_name[] = {
	__stringify_1(IPA_IP_CMD_INVALID),
	__stringify_1(IPA_DECIPH_INIT),
	__stringify_1(IPA_PPP_FRM_INIT),
	__stringify_1(IPA_IP_V4_FILTER_INIT),
	__stringify_1(IPA_IP_V6_FILTER_INIT),
	__stringify_1(IPA_IP_V4_NAT_INIT),
	__stringify_1(IPA_IP_V6_NAT_INIT),
	__stringify_1(IPA_IP_V4_ROUTING_INIT),
	__stringify_1(IPA_IP_V6_ROUTING_INIT),
	__stringify_1(IPA_HDR_INIT_LOCAL),
	__stringify_1(IPA_HDR_INIT_SYSTEM),
	__stringify_1(IPA_DECIPH_SETUP),
	__stringify_1(IPA_INSERT_NAT_RULE),
	__stringify_1(IPA_DELETE_NAT_RULE),
	__stringify_1(IPA_NAT_DMA),
	__stringify_1(IPA_IP_PACKET_TAG),
	__stringify_1(IPA_IP_PACKET_INIT),
};

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
};

static struct dentry *dent;
static struct dentry *dfile_gen_reg;
static struct dentry *dfile_ep_reg;
static struct dentry *dfile_ep_holb;
static struct dentry *dfile_hdr;
static struct dentry *dfile_ip4_rt;
static struct dentry *dfile_ip6_rt;
static struct dentry *dfile_ip4_flt;
static struct dentry *dfile_ip6_flt;
static struct dentry *dfile_stats;
static struct dentry *dfile_dbg_cnt;
static struct dentry *dfile_msg;
static struct dentry *dfile_ip4_nat;
static struct dentry *dfile_rm_stats;
static char dbg_buff[IPA_MAX_MSG_LEN];
static s8 ep_reg_idx;

static ssize_t ipa_read_gen_reg(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes;

	ipa_inc_client_enable_clks();
	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_0)
		nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
			"IPA_VERSION=0x%x\n"
			"IPA_COMP_HW_VERSION=0x%x\n"
			"IPA_ROUTE=0x%x\n"
			"IPA_FILTER=0x%x\n"
			"IPA_SHARED_MEM_SIZE=0x%x\n"
			"IPA_HEAD_OF_LINE_BLOCK_EN=0x%x\n",
			ipa_read_reg(ipa_ctx->mmio, IPA_VERSION_OFST),
			ipa_read_reg(ipa_ctx->mmio, IPA_COMP_HW_VERSION_OFST),
			ipa_read_reg(ipa_ctx->mmio, IPA_ROUTE_OFST_v1),
			ipa_read_reg(ipa_ctx->mmio, IPA_FILTER_OFST_v1),
			ipa_read_reg(ipa_ctx->mmio,
				IPA_SHARED_MEM_SIZE_OFST_v1),
			ipa_read_reg(ipa_ctx->mmio,
				IPA_HEAD_OF_LINE_BLOCK_EN_OFST)
				);
	 else
		nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
			"IPA_VERSION=0x%x\n"
			"IPA_COMP_HW_VERSION=0x%x\n"
			"IPA_ROUTE=0x%x\n"
			"IPA_FILTER=0x%x\n"
			"IPA_SHARED_MEM_SIZE=0x%x\n",
			ipa_read_reg(ipa_ctx->mmio, IPA_VERSION_OFST),
			ipa_read_reg(ipa_ctx->mmio, IPA_COMP_HW_VERSION_OFST),
			ipa_read_reg(ipa_ctx->mmio, IPA_ROUTE_OFST_v2),
			ipa_read_reg(ipa_ctx->mmio, IPA_FILTER_OFST_v2),
			ipa_read_reg(ipa_ctx->mmio, IPA_SHARED_MEM_SIZE_OFST_v2)
				);
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

		if (ipa_ctx->ipa_hw_type == IPA_HW_v1_0) {
			nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
				"IPA_ENDP_INIT_NAT_%u=0x%x\n"
				"IPA_ENDP_INIT_HDR_%u=0x%x\n"
				"IPA_ENDP_INIT_MODE_%u=0x%x\n"
				"IPA_ENDP_INIT_AGGR_%u=0x%x\n"
				"IPA_ENDP_INIT_ROUTE_%u=0x%x\n",
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_NAT_n_OFST_v1(i)),
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_HDR_n_OFST_v1(i)),
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_MODE_n_OFST_v1(i)),
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_AGGR_n_OFST_v1(i)),
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_ROUTE_n_OFST_v1(i)));
		} else {
			nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
				"IPA_ENDP_INIT_NAT_%u=0x%x\n"
				"IPA_ENDP_INIT_HDR_%u=0x%x\n"
				"IPA_ENDP_INIT_MODE_%u=0x%x\n"
				"IPA_ENDP_INIT_AGGR_%u=0x%x\n"
				"IPA_ENDP_INIT_ROUTE_%u=0x%x\n"
				"IPA_ENDP_INIT_CTRL_%u=0x%x\n"
				"IPA_ENDP_INIT_HOL_EN_%u=0x%x\n"
				"IPA_ENDP_INIT_HOL_TIMER_%u=0x%x\n",
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_NAT_n_OFST_v2(i)),
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_HDR_n_OFST_v2(i)),
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_MODE_n_OFST_v2(i)),
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_AGGR_n_OFST_v2(i)),
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_ROUTE_n_OFST_v2(i)),
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_CTRL_n_OFST(i)),
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_HOL_BLOCK_EN_n_OFST(i)),
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_HOL_BLOCK_TIMER_n_OFST(i))
					);
		}

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

static ssize_t ipa_read_hdr(struct file *file, char __user *ubuf, size_t count,
		loff_t *ppos)
{
	int nbytes = 0;
	int cnt = 0;
	int i = 0;
	struct ipa_hdr_entry *entry;

	mutex_lock(&ipa_ctx->lock);
	list_for_each_entry(entry, &ipa_ctx->hdr_tbl.head_hdr_entry_list,
			link) {
		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
				   "name:%s len=%d ref=%d partial=%d lcl=%d ofst=%u ",
				   entry->name,
				   entry->hdr_len, entry->ref_cnt,
				   entry->is_partial,
				   ipa_ctx->hdr_tbl_lcl,
				   entry->offset_entry->offset >> 2);
		for (i = 0; i < entry->hdr_len; i++) {
			scnprintf(dbg_buff + cnt + nbytes + i * 2,
				  IPA_MAX_MSG_LEN - cnt - nbytes - i * 2,
				  "%02x", entry->hdr[i]);
		}
		scnprintf(dbg_buff + cnt + nbytes + entry->hdr_len * 2,
			  IPA_MAX_MSG_LEN - cnt - nbytes - entry->hdr_len * 2,
			  "\n");
		cnt += nbytes + entry->hdr_len * 2 + 1;
	}
	mutex_unlock(&ipa_ctx->lock);

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
}

static int ipa_attrib_dump(char *buff, size_t sz,
		struct ipa_rule_attrib *attrib, enum ipa_ip_type ip)
{
	int nbytes = 0;
	int cnt = 0;
	uint32_t addr[4];
	uint32_t mask[4];
	int i;


	if (attrib->attrib_mask & IPA_FLT_TOS_MASKED) {
		nbytes = scnprintf(buff + cnt, sz - cnt, "tos_value:%d ",
				attrib->tos_value);
		cnt += nbytes;
	}
	if (attrib->attrib_mask & IPA_FLT_TOS_MASKED) {
		nbytes = scnprintf(buff + cnt, sz - cnt, "tos_mask:%d ",
				attrib->tos_mask);
		cnt += nbytes;
	}

	if (attrib->attrib_mask & IPA_FLT_PROTOCOL) {
		nbytes = scnprintf(buff + cnt, sz - cnt, "protocol:%d ",
				attrib->u.v4.protocol);
		cnt += nbytes;
	}
	if (attrib->attrib_mask & IPA_FLT_SRC_ADDR) {
		if (ip == IPA_IP_v4) {
			addr[0] = htonl(attrib->u.v4.src_addr);
			mask[0] = htonl(attrib->u.v4.src_addr_mask);
			nbytes = scnprintf(buff + cnt, sz - cnt,
					"src_addr:%pI4 src_addr_mask:%pI4 ",
					addr + 0, mask + 0);
			cnt += nbytes;
		} else if (ip == IPA_IP_v6) {
			for (i = 0; i < 4; i++) {
				addr[i] = htonl(attrib->u.v6.src_addr[i]);
				mask[i] = htonl(attrib->u.v6.src_addr_mask[i]);
			}
			nbytes =
			   scnprintf(buff + cnt, sz - cnt,
					   "src_addr:%pI6 src_addr_mask:%pI6 ",
					   addr + 0, mask + 0);
			cnt += nbytes;
		} else {
			WARN_ON(1);
		}
	}
	if (attrib->attrib_mask & IPA_FLT_DST_ADDR) {
		if (ip == IPA_IP_v4) {
			addr[0] = htonl(attrib->u.v4.dst_addr);
			mask[0] = htonl(attrib->u.v4.dst_addr_mask);
			nbytes =
			   scnprintf(buff + cnt, sz - cnt,
					   "dst_addr:%pI4 dst_addr_mask:%pI4 ",
					   addr + 0, mask + 0);
			cnt += nbytes;
		} else if (ip == IPA_IP_v6) {
			for (i = 0; i < 4; i++) {
				addr[i] = htonl(attrib->u.v6.dst_addr[i]);
				mask[i] = htonl(attrib->u.v6.dst_addr_mask[i]);
			}
			nbytes =
			   scnprintf(buff + cnt, sz - cnt,
					   "dst_addr:%pI6 dst_addr_mask:%pI6 ",
					   addr + 0, mask + 0);
			cnt += nbytes;
		} else {
			WARN_ON(1);
		}
	}
	if (attrib->attrib_mask & IPA_FLT_SRC_PORT_RANGE) {
		nbytes =
		   scnprintf(buff + cnt, sz - cnt, "src_port_range:%u %u ",
				   attrib->src_port_lo,
			     attrib->src_port_hi);
		cnt += nbytes;
	}
	if (attrib->attrib_mask & IPA_FLT_DST_PORT_RANGE) {
		nbytes =
		   scnprintf(buff + cnt, sz - cnt, "dst_port_range:%u %u ",
				   attrib->dst_port_lo,
			     attrib->dst_port_hi);
		cnt += nbytes;
	}
	if (attrib->attrib_mask & IPA_FLT_TYPE) {
		nbytes = scnprintf(buff + cnt, sz - cnt, "type:%d ",
				attrib->type);
		cnt += nbytes;
	}
	if (attrib->attrib_mask & IPA_FLT_CODE) {
		nbytes = scnprintf(buff + cnt, sz - cnt, "code:%d ",
				attrib->code);
		cnt += nbytes;
	}
	if (attrib->attrib_mask & IPA_FLT_SPI) {
		nbytes = scnprintf(buff + cnt, sz - cnt, "spi:%x ",
				attrib->spi);
		cnt += nbytes;
	}
	if (attrib->attrib_mask & IPA_FLT_SRC_PORT) {
		nbytes = scnprintf(buff + cnt, sz - cnt, "src_port:%u ",
				attrib->src_port);
		cnt += nbytes;
	}
	if (attrib->attrib_mask & IPA_FLT_DST_PORT) {
		nbytes = scnprintf(buff + cnt, sz - cnt, "dst_port:%u ",
				attrib->dst_port);
		cnt += nbytes;
	}
	if (attrib->attrib_mask & IPA_FLT_TC) {
		nbytes = scnprintf(buff + cnt, sz - cnt, "tc:%d ",
				attrib->u.v6.tc);
		cnt += nbytes;
	}
	if (attrib->attrib_mask & IPA_FLT_FLOW_LABEL) {
		nbytes = scnprintf(buff + cnt, sz - cnt, "flow_label:%x ",
				attrib->u.v6.flow_label);
		cnt += nbytes;
	}
	if (attrib->attrib_mask & IPA_FLT_NEXT_HDR) {
		nbytes = scnprintf(buff + cnt, sz - cnt, "next_hdr:%d ",
				attrib->u.v6.next_hdr);
		cnt += nbytes;
	}
	if (attrib->attrib_mask & IPA_FLT_META_DATA) {
		nbytes =
		   scnprintf(buff + cnt, sz - cnt,
				   "metadata:%x metadata_mask:%x",
				   attrib->meta_data, attrib->meta_data_mask);
		cnt += nbytes;
	}
	if (attrib->attrib_mask & IPA_FLT_FRAGMENT) {
		nbytes = scnprintf(buff + cnt, sz - cnt, "frg ");
		cnt += nbytes;
	}
	nbytes = scnprintf(buff + cnt, sz - cnt, "\n");
	cnt += nbytes;

	return cnt;
}

static int ipa_open_dbg(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t ipa_read_rt(struct file *file, char __user *ubuf, size_t count,
		loff_t *ppos)
{
	int nbytes = 0;
	int cnt = 0;
	int i = 0;
	struct ipa_rt_tbl *tbl;
	struct ipa_rt_entry *entry;
	struct ipa_rt_tbl_set *set;
	enum ipa_ip_type ip = (enum ipa_ip_type)file->private_data;
	u32 hdr_ofst;

	set = &ipa_ctx->rt_tbl_set[ip];

	mutex_lock(&ipa_ctx->lock);
	if (ip ==  IPA_IP_v6) {
		if (ipa_ctx->ip6_rt_tbl_lcl) {
			nbytes = scnprintf(dbg_buff + cnt,
				IPA_MAX_MSG_LEN - cnt,
				"Table Resides on local memory\n");
			cnt += nbytes;
		} else {
			nbytes = scnprintf(dbg_buff + cnt,
				IPA_MAX_MSG_LEN - cnt,
				"Table Resides on system(ddr) memory\n");
			cnt += nbytes;
		}
	} else if (ip == IPA_IP_v4) {
		if (ipa_ctx->ip4_rt_tbl_lcl) {
			nbytes = scnprintf(dbg_buff + cnt,
				IPA_MAX_MSG_LEN - cnt,
				"Table Resides on local memory\n");
			cnt += nbytes;
		} else {
			nbytes = scnprintf(dbg_buff + cnt,
				IPA_MAX_MSG_LEN - cnt,
				"Table Resides on system(ddr) memory\n");
			cnt += nbytes;
		}
	}

	list_for_each_entry(tbl, &set->head_rt_tbl_list, link) {
		i = 0;
		list_for_each_entry(entry, &tbl->head_rt_rule_list, link) {
			if (entry->hdr)
				hdr_ofst = entry->hdr->offset_entry->offset;
			else
				hdr_ofst = 0;
			nbytes = scnprintf(dbg_buff + cnt,
					IPA_MAX_MSG_LEN - cnt,
					"tbl_idx:%d tbl_name:%s tbl_ref:%u rule_idx:%d dst:%d name:%s ep:%d S:%u hdr_ofst[words]:%u attrib_mask:%08x ",
					entry->tbl->idx, entry->tbl->name,
					entry->tbl->ref_cnt, i, entry->rule.dst,
					ipa_client_name[entry->rule.dst],
					ipa_get_ep_mapping(ipa_ctx->mode,
						entry->rule.dst),
					   !ipa_ctx->hdr_tbl_lcl,
					   hdr_ofst >> 2,
					   entry->rule.attrib.attrib_mask);
			cnt += nbytes;
			cnt += ipa_attrib_dump(dbg_buff + cnt,
					   IPA_MAX_MSG_LEN - cnt,
					   &entry->rule.attrib,
					   ip);
			i++;
		}
	}
	mutex_unlock(&ipa_ctx->lock);

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
}

static ssize_t ipa_read_flt(struct file *file, char __user *ubuf, size_t count,
		loff_t *ppos)
{
	int nbytes = 0;
	int cnt = 0;
	int i;
	int j;
	int k;
	struct ipa_flt_tbl *tbl;
	struct ipa_flt_entry *entry;
	enum ipa_ip_type ip = (enum ipa_ip_type)file->private_data;
	struct ipa_rt_tbl *rt_tbl;
	u32 rt_tbl_idx;

	tbl = &ipa_ctx->glob_flt_tbl[ip];
	mutex_lock(&ipa_ctx->lock);
	i = 0;
	list_for_each_entry(entry, &tbl->head_flt_rule_list, link) {
		rt_tbl = (struct ipa_rt_tbl *)entry->rule.rt_tbl_hdl;
		if (rt_tbl == NULL)
			rt_tbl_idx = ~0;
		else
			rt_tbl_idx = rt_tbl->idx;
		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
				   "ep_idx:global rule_idx:%d act:%d rt_tbl_idx:%d attrib_mask:%08x ",
				   i, entry->rule.action, rt_tbl_idx,
				   entry->rule.attrib.attrib_mask);
		cnt += nbytes;
		cnt += ipa_attrib_dump(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
				&entry->rule.attrib, ip);
		i++;
	}

	for (j = 0; j < IPA_NUM_PIPES; j++) {
		tbl = &ipa_ctx->flt_tbl[j][ip];
		i = 0;
		list_for_each_entry(entry, &tbl->head_flt_rule_list, link) {
			rt_tbl = (struct ipa_rt_tbl *)entry->rule.rt_tbl_hdl;
			if (rt_tbl == NULL)
				rt_tbl_idx = ~0;
			else
				rt_tbl_idx = rt_tbl->idx;
			k = ipa_get_client_mapping(ipa_ctx->mode, j);
			nbytes = scnprintf(dbg_buff + cnt,
					IPA_MAX_MSG_LEN - cnt,
					"ep_idx:%d name:%s rule_idx:%d act:%d rt_tbl_idx:%d attrib_mask:%08x ",
					j, ipa_client_name[k], i,
					entry->rule.action, rt_tbl_idx,
					entry->rule.attrib.attrib_mask);
			cnt += nbytes;
			cnt +=
			   ipa_attrib_dump(dbg_buff + cnt,
					   IPA_MAX_MSG_LEN - cnt,
					   &entry->rule.attrib,
					   ip);
			i++;
		}
	}
	mutex_unlock(&ipa_ctx->lock);

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
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

	nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
			"sw_tx=%u\n"
			"hw_tx=%u\n"
			"rx=%u\n"
			"rx_repl_repost=%u\n"
			"x_intr_repost=%u\n"
			"x_intr_repost_tx=%u\n"
			"rx_q_len=%u\n"
			"act_clnt=%u\n"
			"con_clnt_bmap=0x%x\n"
			"a2_power_on_reqs_in=%u\n"
			"a2_power_on_reqs_out=%u\n"
			"a2_power_off_reqs_in=%u\n"
			"a2_power_off_reqs_out=%u\n"
			"a2_power_modem_acks=%u\n"
			"a2_power_apps_acks=%u\n",
			ipa_ctx->stats.tx_sw_pkts,
			ipa_ctx->stats.tx_hw_pkts,
			ipa_ctx->stats.rx_pkts,
			ipa_ctx->stats.rx_repl_repost,
			ipa_ctx->stats.x_intr_repost,
			ipa_ctx->stats.x_intr_repost_tx,
			ipa_ctx->stats.rx_q_len,
			ipa_ctx->ipa_active_clients,
			connect,
			ipa_ctx->stats.a2_power_on_reqs_in,
			ipa_ctx->stats.a2_power_on_reqs_out,
			ipa_ctx->stats.a2_power_off_reqs_in,
			ipa_ctx->stats.a2_power_off_reqs_out,
			ipa_ctx->stats.a2_power_modem_acks,
			ipa_ctx->stats.a2_power_apps_acks);
	cnt += nbytes;

	for (i = 0; i < MAX_NUM_EXCP; i++) {
		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
				"rx_excp[%u:%35s]=%u\n", i, ipa_excp_name[i],
				ipa_ctx->stats.rx_excp_pkts[i]);
		cnt += nbytes;
	}

	for (i = 0; i < IPA_BRIDGE_TYPE_MAX; i++) {
		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
				"brg_pkt[%u:%s][dl]=%u\n"
				"brg_pkt[%u:%s][ul]=%u\n",
				i, (i == 0) ? "teth" : "embd",
				ipa_ctx->stats.bridged_pkts[i][0],
				i, (i == 0) ? "teth" : "embd",
				ipa_ctx->stats.bridged_pkts[i][1]);
		cnt += nbytes;
	}

	for (i = 0; i < MAX_NUM_IMM_CMD; i++) {
		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
				"IC[%2u:%22s]=%u\n", i, ipa_ic_name[i],
				ipa_ctx->stats.imm_cmds[i]);
		cnt += nbytes;
	}

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
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
	if (option == 1)
		ipa_write_reg(ipa_ctx->mmio, IPA_DEBUG_CNT_CTRL_n_OFST(0),
				IPA_DBG_CNTR_ON);
	else
		ipa_write_reg(ipa_ctx->mmio, IPA_DEBUG_CNT_CTRL_n_OFST(0),
				IPA_DBG_CNTR_OFF);
	ipa_dec_client_disable_clks();

	return count;
}

static ssize_t ipa_read_dbg_cnt(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes;

	ipa_inc_client_enable_clks();
	nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
			"IPA_DEBUG_CNT_REG_0=0x%x\n",
			ipa_read_reg(ipa_ctx->mmio,
			IPA_DEBUG_CNT_REG_n_OFST(0)));
	ipa_dec_client_disable_clks();

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t ipa_read_msg(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes;
	int cnt = 0;
	int i;

	for (i = 0; i < IPA_EVENT_MAX; i++) {
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
	int nbytes, cnt;

	cnt = 0;
	value = ipa_ctx->nat_mem.public_ip_addr;
	nbytes = scnprintf(dbg_buff + cnt,
				IPA_MAX_MSG_LEN,
				"Table IP Address:%d.%d.%d.%d\n",
				((value & 0xFF000000) >> 24),
				((value & 0x00FF0000) >> 16),
				((value & 0x0000FF00) >> 8),
				((value & 0x000000FF)));
	cnt += nbytes;

	nbytes = scnprintf(dbg_buff + cnt,
				IPA_MAX_MSG_LEN,
				"Table Size:%d\n",
				ipa_ctx->nat_mem.size_base_tables);
	cnt += nbytes;

	nbytes = scnprintf(dbg_buff + cnt,
				IPA_MAX_MSG_LEN,
				"Expansion Table Size:%d\n",
				ipa_ctx->nat_mem.size_expansion_tables);
	cnt += nbytes;

	if (!ipa_ctx->nat_mem.is_sys_mem) {
		nbytes = scnprintf(dbg_buff + cnt,
					IPA_MAX_MSG_LEN,
					"Not supported for local(shared) memory\n");
		cnt += nbytes;

		return simple_read_from_buffer(ubuf, count,
						ppos, dbg_buff, cnt);
	}


	/* Print Base tables */
	rule_id = 0;
	for (j = 0; j < 2; j++) {
		if (j == BASE_TABLE) {
			tbl_size = ipa_ctx->nat_mem.size_base_tables;
			base_tbl = (u32 *)ipa_ctx->nat_mem.ipv4_rules_addr;

			nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN,
					"\nBase Table:\n");
			cnt += nbytes;
		} else {
			tbl_size = ipa_ctx->nat_mem.size_expansion_tables;
			base_tbl =
			 (u32 *)ipa_ctx->nat_mem.ipv4_expansion_rules_addr;

			nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN,
					"\nExpansion Base Table:\n");
			cnt += nbytes;
		}

		if (base_tbl != NULL) {
			for (i = 0; i <= tbl_size; i++, rule_id++) {
				tmp = base_tbl;
				value = tmp[4];
				enable = ((value & 0xFFFF0000) >> 16);

				if (enable & NAT_ENTRY_ENABLE) {
					nbytes = scnprintf(dbg_buff + cnt,
								IPA_MAX_MSG_LEN,
								"Rule:%d ",
								rule_id);
					cnt += nbytes;

					value = *tmp;
					nbytes = scnprintf(dbg_buff + cnt,
						IPA_MAX_MSG_LEN,
						"Private_IP:%d.%d.%d.%d ",
						((value & 0xFF000000) >> 24),
						((value & 0x00FF0000) >> 16),
						((value & 0x0000FF00) >> 8),
						((value & 0x000000FF)));
					cnt += nbytes;
					tmp++;

					value = *tmp;
					nbytes = scnprintf(dbg_buff + cnt,
						IPA_MAX_MSG_LEN,
						"Target_IP:%d.%d.%d.%d ",
						((value & 0xFF000000) >> 24),
						((value & 0x00FF0000) >> 16),
						((value & 0x0000FF00) >> 8),
						((value & 0x000000FF)));
					cnt += nbytes;
					tmp++;

					value = *tmp;
					nbytes = scnprintf(dbg_buff + cnt,
						IPA_MAX_MSG_LEN,
						"Next_Index:%d  Public_Port:%d ",
						(value & 0x0000FFFF),
						((value & 0xFFFF0000) >> 16));
					cnt += nbytes;
					tmp++;

					value = *tmp;
					nbytes = scnprintf(dbg_buff + cnt,
						IPA_MAX_MSG_LEN,
						"Private_Port:%d  Target_Port:%d ",
						(value & 0x0000FFFF),
						((value & 0xFFFF0000) >> 16));
					cnt += nbytes;
					tmp++;

					value = *tmp;
					nbytes = scnprintf(dbg_buff + cnt,
						IPA_MAX_MSG_LEN,
						"IP-CKSM-delta:0x%x  ",
						(value & 0x0000FFFF));
					cnt += nbytes;

					flag = ((value & 0xFFFF0000) >> 16);
					if (flag & NAT_ENTRY_RST_FIN_BIT) {
						nbytes =
						 scnprintf(dbg_buff + cnt,
							  IPA_MAX_MSG_LEN,
								"IP_CKSM_delta:0x%x  Flags:%s ",
							  (value & 0x0000FFFF),
								"Direct_To_A5");
						cnt += nbytes;
					} else {
						nbytes =
						 scnprintf(dbg_buff + cnt,
							IPA_MAX_MSG_LEN,
							"IP_CKSM_delta:0x%x  Flags:%s ",
							(value & 0x0000FFFF),
							"Fwd_to_route");
						cnt += nbytes;
					}
					tmp++;

					value = *tmp;
					nbytes = scnprintf(dbg_buff + cnt,
						IPA_MAX_MSG_LEN,
						"Time_stamp:0x%x Proto:%d ",
						(value & 0x00FFFFFF),
						((value & 0xFF000000) >> 27));
					cnt += nbytes;
					tmp++;

					value = *tmp;
					nbytes = scnprintf(dbg_buff + cnt,
						IPA_MAX_MSG_LEN,
						"Prev_Index:%d  Indx_tbl_entry:%d ",
						(value & 0x0000FFFF),
						((value & 0xFFFF0000) >> 16));
					cnt += nbytes;
					tmp++;

					value = *tmp;
					nbytes = scnprintf(dbg_buff + cnt,
						IPA_MAX_MSG_LEN,
						"TCP_UDP_cksum_delta:0x%x\n",
						((value & 0xFFFF0000) >> 16));
					cnt += nbytes;
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

			nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN,
					"\nIndex Table:\n");
			cnt += nbytes;
		} else {
			tbl_size = ipa_ctx->nat_mem.size_expansion_tables;
			indx_tbl =
			 (u32 *)ipa_ctx->nat_mem.index_table_expansion_addr;

			nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN,
					"\nExpansion Index Table:\n");
			cnt += nbytes;
		}

		if (indx_tbl != NULL) {
			for (i = 0; i <= tbl_size; i++, rule_id++) {
				tmp = indx_tbl;
				value = *tmp;
				tbl_entry = (value & 0x0000FFFF);

				if (tbl_entry) {
					nbytes = scnprintf(dbg_buff + cnt,
								IPA_MAX_MSG_LEN,
								"Rule:%d ",
								rule_id);
					cnt += nbytes;

					value = *tmp;
					nbytes = scnprintf(dbg_buff + cnt,
						IPA_MAX_MSG_LEN,
						"Table_Entry:%d  Next_Index:%d\n",
						tbl_entry,
						((value & 0xFFFF0000) >> 16));
					cnt += nbytes;
				}

				indx_tbl++;
			}
		}
	}

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
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

const struct file_operations ipa_flt_ops = {
	.read = ipa_read_flt,
	.open = ipa_open_dbg,
};

const struct file_operations ipa_stats_ops = {
	.read = ipa_read_stats,
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
			S_IWUSR | S_IWGRP | S_IWOTH;
	const mode_t write_only_mode = S_IWUSR | S_IWGRP | S_IWOTH;

	dent = debugfs_create_dir("ipa", 0);
	if (IS_ERR(dent)) {
		IPAERR("fail to create folder in debug_fs.\n");
		return;
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

	dfile_rm_stats = debugfs_create_file("rm_stats", read_only_mode,
		dent, 0,
		&ipa_rm_stats);
	if (!dfile_rm_stats || IS_ERR(dfile_rm_stats)) {
		IPAERR("fail to create file for debug_fs rm_stats\n");
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

