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


#define IPA_MAX_MSG_LEN 4096

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

static struct dentry *dent;
static struct dentry *dfile_gen_reg;
static struct dentry *dfile_ep_reg;
static struct dentry *dfile_hdr;
static struct dentry *dfile_ip4_rt;
static struct dentry *dfile_ip6_rt;
static struct dentry *dfile_ip4_flt;
static struct dentry *dfile_ip6_flt;
static struct dentry *dfile_stats;
static char dbg_buff[IPA_MAX_MSG_LEN];
static s8 ep_reg_idx;

static ssize_t ipa_read_gen_reg(struct file *file, char __user *ubuf,
		size_t count, loff_t *ppos)
{
	int nbytes;

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

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
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
				"IPA_ENDP_INIT_ROUTE_%u=0x%x\n",
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_NAT_n_OFST_v2(i)),
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_HDR_n_OFST_v2(i)),
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_MODE_n_OFST_v2(i)),
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_AGGR_n_OFST_v2(i)),
				i, ipa_read_reg(ipa_ctx->mmio,
					IPA_ENDP_INIT_ROUTE_n_OFST_v2(i)));
		}

		*ppos = pos;
		ret = simple_read_from_buffer(ubuf, count, ppos, dbg_buff,
					      nbytes);
		if (ret < 0)
			return ret;

		size += ret;
		ubuf += nbytes;
		count -= nbytes;
	}

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

	if (attrib->attrib_mask & IPA_FLT_TOS) {
		nbytes = scnprintf(buff + cnt, sz - cnt, "tos:%d ",
				attrib->u.v4.tos);
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

	nbytes = scnprintf(dbg_buff, IPA_MAX_MSG_LEN,
			"sw_tx=%u\n"
			"hw_tx=%u\n"
			"rx=%u\n",
			ipa_ctx->stats.tx_sw_pkts,
			ipa_ctx->stats.tx_hw_pkts,
			ipa_ctx->stats.rx_pkts);
	cnt += nbytes;

	for (i = 0; i < MAX_NUM_EXCP; i++) {
		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
				"rx_excp[%u]=%u\n", i,
				ipa_ctx->stats.rx_excp_pkts[i]);
		cnt += nbytes;
	}

	for (i = 0; i < IPA_BRIDGE_TYPE_MAX; i++) {
		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
				"bridged_pkt[%u][dl]=%u\n"
				"bridged_pkt[%u][ul]=%u\n",
				i,
				ipa_ctx->stats.bridged_pkts[i][0],
				i,
				ipa_ctx->stats.bridged_pkts[i][1]);
		cnt += nbytes;
	}

	for (i = 0; i < MAX_NUM_IMM_CMD; i++) {
		nbytes = scnprintf(dbg_buff + cnt, IPA_MAX_MSG_LEN - cnt,
				"IC[%u]=%u\n", i,
				ipa_ctx->stats.imm_cmds[i]);
		cnt += nbytes;
	}

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, cnt);
}


const struct file_operations ipa_gen_reg_ops = {
	.read = ipa_read_gen_reg,
};

const struct file_operations ipa_ep_reg_ops = {
	.read = ipa_read_ep_reg,
	.write = ipa_write_ep_reg,
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

void ipa_debugfs_init(void)
{
	const mode_t read_only_mode = S_IRUSR | S_IRGRP | S_IROTH;
	const mode_t read_write_mode = S_IRUSR | S_IRGRP | S_IROTH |
			S_IWUSR | S_IWGRP | S_IWOTH;

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

