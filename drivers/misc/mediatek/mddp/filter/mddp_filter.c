/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>
#include <linux/kernel_stat.h>
#include <linux/proc_fs.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <linux/netfilter_bridge.h>
#include <asm/byteorder.h>
#include <net/xfrm.h>

#include "mddp_track.h"
#include "mddp_f_desc.h"
#include "mddp_f_proto.h"
#include "mddp_f_tuple.h"
#include "mddp_f_dev.h"
#include "mddp_f_hw.c"

#include "mddp_ctrl.h"

#define MAX_IFACE_NUM 32
#define DYN_IFACE_OFFSET 16
#define IPC_HDR_IS_V4(_ip_hdr) \
	(0x40 == (*((unsigned char *)(_ip_hdr)) & 0xf0))
#define IPC_HDR_IS_V6(_ip_hdr) \
	(0x60 == (*((unsigned char *)(_ip_hdr)) & 0xf0))
#define IPC_NE_GET_2B(_buf) \
	((((u16)*((u8 *)(_buf) + 0)) << 8) | \
	  (((u16)*((u8 *)(_buf) + 1)) << 0))

int mddp_f_max_nat = MD_DIRECT_TETHERING_RULE_NUM;
int mddp_f_max_router = MD_DIRECT_TETHERING_RULE_NUM;

struct kmem_cache *mddp_f_nat_tuple_cache;
struct kmem_cache *mddp_f_router_tuple_cache;

static u32 mddp_f_jhash_initval __read_mostly;

int mddp_f_contentfilter;

struct interface ifaces[MAX_IFACE_NUM];

spinlock_t mddp_f_lock;
#ifdef CONFIG_PREEMPT_RT_FULL
raw_spinlock_t mddp_f_tuple_lock;
#else
spinlock_t mddp_f_tuple_lock;
#endif

#ifdef MDDP_F_NO_KERNEL_SUPPORT
struct mddp_f_track_table_list_t mddp_f_track[MDDP_F_MAX_TRACK_NUM];
struct mddp_f_track_table_list_t mddp_f_table_buffer;
unsigned int buffer_cnt;
//------------------------------------------------------------------------------
// Function prototype.
//------------------------------------------------------------------------------
static uint32_t mddp_nfhook_prerouting
(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);
static uint32_t mddp_nfhook_postrouting
(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);

//------------------------------------------------------------------------------
// Registed callback function.
//------------------------------------------------------------------------------
static struct nf_hook_ops mddp_nf_ops[] __read_mostly = {
	{
		.hook           = mddp_nfhook_prerouting,
		.pf             = NFPROTO_IPV4,
		.hooknum        = NF_INET_PRE_ROUTING,
		.priority       = NF_IP_PRI_FIRST + 1,
	},
	{
		.hook           = mddp_nfhook_postrouting,
		.pf             = NFPROTO_IPV4,
		.hooknum        = NF_INET_POST_ROUTING,
		.priority       = NF_IP_PRI_LAST,
	},
	{
		.hook           = mddp_nfhook_prerouting,
		.pf             = NFPROTO_IPV6,
		.hooknum        = NF_INET_PRE_ROUTING,
		.priority       = NF_IP6_PRI_FIRST + 1,
	},
	{
		.hook           = mddp_nfhook_postrouting,
		.pf             = NFPROTO_IPV6,
		.hooknum        = NF_INET_POST_ROUTING,
		.priority       = NF_IP6_PRI_LAST,
	},
	{
		.hook           = mddp_nfhook_prerouting,
		.pf             = PF_BRIDGE,
		.hooknum        = NF_BR_PRE_ROUTING,
		.priority       = NF_BR_PRI_FIRST + 1,
	},
	{
		.hook           = mddp_nfhook_postrouting,
		.pf             = PF_BRIDGE,
		.hooknum        = NF_BR_POST_ROUTING,
		.priority       = NF_IP_PRI_LAST,
	 },
};

//------------------------------------------------------------------------------
// Private functions.
//------------------------------------------------------------------------------
void dummy_destructor_track_table(struct sk_buff *skb)
{
	(void)skb;
}

static void mddp_f_init_table_buffer(void)
{
	int i;
	struct mddp_f_track_table_t *track_table;
	struct mddp_f_track_table_t *next_track_table = NULL;

	TRACK_TABLE_INIT_LOCK(mddp_f_table_buffer);
	mddp_f_table_buffer.table = NULL;

	for (i = 0; i < MDDP_F_TABLE_BUFFER_NUM; i++) {
		track_table = kmalloc(sizeof(struct mddp_f_track_table_t),
								GFP_ATOMIC);
		if (!track_table) {

			pr_notice("%s: kmalloc failed!\n", __func__);
			continue;
		}

		memset(track_table, 0, sizeof(struct mddp_f_track_table_t));
		track_table->next_track_table = next_track_table;
		next_track_table = track_table;

		buffer_cnt++;
	}
	mddp_f_table_buffer.table = track_table;

	pr_notice("%s: Total table buffer num[%d], table head[%p]!\n",
				__func__, buffer_cnt,
				mddp_f_table_buffer.table);
}

static struct mddp_f_track_table_t *mddp_f_get_table_buffer(void)
{
	unsigned long flags;
	struct mddp_f_track_table_t *track_table = NULL;

	TRACK_TABLE_LOCK(mddp_f_table_buffer, flags);

	if (mddp_f_table_buffer.table) {
		track_table = mddp_f_table_buffer.table;
		mddp_f_table_buffer.table =
				mddp_f_table_buffer.table->next_track_table;

		buffer_cnt--;
	}

	TRACK_TABLE_UNLOCK(mddp_f_table_buffer, flags);

	return track_table;
}

static void mddp_f_put_table_buffer(struct mddp_f_track_table_t *track_table)
{
	unsigned long flags;

	TRACK_TABLE_LOCK(mddp_f_table_buffer, flags);

	track_table->next_track_table = mddp_f_table_buffer.table;
	mddp_f_table_buffer.table = track_table;
	buffer_cnt++;

	TRACK_TABLE_UNLOCK(mddp_f_table_buffer, flags);
}

static inline struct mddp_f_cb *_insert_track_table(
	struct sk_buff *skb,
	struct mddp_f_track_table_t *track_table)
{

	track_table->ref_count = 0;
	track_table->tracked_address = (void *)skb;
	track_table->jiffies = jiffies;
	track_table->next_track_table = NULL;

	return &track_table->cb;
}

static inline void _remove_track_table(
	unsigned int index,
	bool is_first_track_table,
	struct mddp_f_track_table_t *prev_track_table,
	struct mddp_f_track_table_t *curr_track_table)
{
	if (is_first_track_table == true)
		mddp_f_track[index].table = curr_track_table->next_track_table;
	else
		prev_track_table->next_track_table =
					curr_track_table->next_track_table;

	mddp_f_put_table_buffer(curr_track_table);

}

static void mddp_f_init_track_table(void)
{
	int i;

	for (i = 0; i < MDDP_F_MAX_TRACK_NUM; i++) {
		TRACK_TABLE_INIT_LOCK(mddp_f_track[i]);
		mddp_f_track[i].table = NULL;
	}
}

static void dest_track_table(void)
{
	del_all_track_table();
}

static struct mddp_f_cb *add_track_table(
	struct sk_buff *skb,
	struct mddp_f_desc *desc)
{
	struct mddp_f_cb *cb = NULL;
	unsigned int hash;
	unsigned long flags;
	unsigned int list_cnt = 0;
	bool is_first_track_table = true;
	struct mddp_f_track_table_t *track_table = NULL;
	struct mddp_f_track_table_t *curr_track_table = NULL;
	struct mddp_f_track_table_t *prev_track_table = NULL;
	struct mddp_f_track_table_t *tmp_track_table = NULL;

	if (!skb || !desc) {
		pr_notice("%s: Null input, skb[%p], desc[%p].\n",
					__func__, skb, desc);
		goto out;
	}

	if ((desc->flag & DESC_FLAG_IPV4) || (desc->flag & DESC_FLAG_IPV6)) {
		/* Get hash value */
		hash = jhash_1word(((unsigned long)skb & 0xFFFFFFFF),
				  mddp_f_jhash_initval) % MDDP_F_MAX_TRACK_NUM;
	} else {
		pr_notice("%s: Not a IPv4/v6 packet, flag[%x], skb[%p].\n",
					__func__, desc->flag, skb);
		goto out;
	}

	track_table = mddp_f_get_table_buffer();
	if (track_table)
		memset(track_table, 0, sizeof(struct mddp_f_track_table_t));
	else
		pr_notice("%s: Table buffer is used up, cannot get table buffer!.\n",
					__func__);


	pr_debug("%s: 1. Add track table, skb[%p], hash[%d], track_table[%p], remain buffer[%d].\n",
				__func__, skb, hash, track_table, buffer_cnt);

	TRACK_TABLE_LOCK(mddp_f_track[hash], flags);
	curr_track_table = mddp_f_track[hash].table;

	if (!curr_track_table) {
		mddp_f_track[hash].table = track_table;
	} else {
		while (curr_track_table) {
			if (list_cnt >= MDDP_F_MAX_TRACK_TABLE_LIST) {
				pr_notice("%s: Reach max hash list! Don't add track table[%p], skb[%p], hash[%d], remain buffer[%d].\n",
						__func__, track_table,
						skb, hash, buffer_cnt+1);

				if (track_table)
					mddp_f_put_table_buffer(track_table);

				TRACK_TABLE_UNLOCK(mddp_f_track[hash], flags);
				goto out;
			}

			tmp_track_table = curr_track_table->next_track_table;

			if (curr_track_table->ref_count == 0) {
				/* Remove timeout track table */
				if (time_after(jiffies,
					 (curr_track_table->jiffies
					   + msecs_to_jiffies(10)))) {
					pr_debug("%s: Remove timeout track table[%p], skb[%p], hash[%d], is_first_track_table[%d], remain buffer[%d].\n",
						__func__, curr_track_table,
						skb, hash,
						is_first_track_table,
						buffer_cnt+1);

					_remove_track_table(hash,
							is_first_track_table,
							prev_track_table,
							curr_track_table);
					curr_track_table = tmp_track_table;
					continue;
				}

				/* Remove track table of duplicated skb */
				if (unlikely(curr_track_table->tracked_address
						== (void *)skb)) {
					pr_debug("%s: Remove track table[%p] of duplicated skb[%p], hash[%d], is_first_track_table[%d], remain buffer[%d].\n",
						__func__, curr_track_table,
						skb, hash,
						is_first_track_table,
						buffer_cnt+1);

					_remove_track_table(hash,
							is_first_track_table,
							prev_track_table,
							curr_track_table);
					curr_track_table = tmp_track_table;
					continue;
				}
			}

			is_first_track_table = false;
			prev_track_table = curr_track_table;
			curr_track_table = tmp_track_table;
			list_cnt++;
		}

		if (is_first_track_table == true)
			mddp_f_track[hash].table = track_table;
		else
			prev_track_table->next_track_table = track_table;
	}

	/* insert current skb into track table */
	if (track_table)
		cb = _insert_track_table(skb, track_table);

	TRACK_TABLE_UNLOCK(mddp_f_track[hash], flags);

	pr_debug("%s: 2. Add track table, skb[%p], hash[%d], is_first_track_table[%d], track_table[%p].\n",
				__func__, skb, hash,
				is_first_track_table,
				track_table);

out:
	return cb;
}

static struct mddp_f_cb *search_and_hold_track_table(
	struct sk_buff *skb,
	struct mddp_f_track_table_t **curr_track_table)
{
	unsigned int hash;
	unsigned long flags;
	struct mddp_f_cb *cb = NULL;
	bool is_first_track_table = true;
	struct mddp_f_track_table_t *prev_track_table = NULL;

	if (likely(skb->destructor == dummy_destructor_track_table)) {
		pr_notice("%s: Dummy destructor track table, skb[%p].\n",
					__func__, skb);
		return cb;
	}

	hash = jhash_1word(((unsigned long)skb & 0xFFFFFFFF),
				mddp_f_jhash_initval) % MDDP_F_MAX_TRACK_NUM;

	pr_debug("%s: 1. Search track table, skb[%p], hash[%d].\n",
				__func__, skb, hash);

	TRACK_TABLE_LOCK(mddp_f_track[hash], flags);
	*curr_track_table = mddp_f_track[hash].table;
	while (*curr_track_table) {
		if (((*curr_track_table)->tracked_address == (void *)skb)
			&& ((*curr_track_table)->ref_count == 0)) {
			pr_debug("%s: 2. Search track table, skb[%p], hash[%d], is_first_track_table[%d], track_table[%p].\n",
						__func__, skb, hash,
						is_first_track_table,
						*curr_track_table);

			/* Pop the track table from hash list */
			if (is_first_track_table == true)
				mddp_f_track[hash].table =
					(*curr_track_table)->next_track_table;
			else
				prev_track_table->next_track_table =
					(*curr_track_table)->next_track_table;

			(*curr_track_table)->ref_count++;
			cb = &((*curr_track_table)->cb);

			break;
		}

		is_first_track_table = false;
		prev_track_table = *curr_track_table;
		*curr_track_table = (*curr_track_table)->next_track_table;
	}
	TRACK_TABLE_UNLOCK(mddp_f_track[hash], flags);

	return cb;
}

static void put_track_table(
	struct mddp_f_track_table_t *curr_track_table)
{
	curr_track_table->ref_count--;
	if (curr_track_table->ref_count == 0) {
		pr_debug("%s: Remove track table track_table[%p], remain buffer[%d].\n",
				__func__, curr_track_table, buffer_cnt+1);
		mddp_f_put_table_buffer(curr_track_table);
	} else {
		WARN_ON(1);
		pr_notice("%s: Invalid ref_count of track table[%p], ref_cnt[%d].\n",
					__func__, curr_track_table,
					curr_track_table->ref_count);
	}
}

void del_all_track_table(void)
{
	int i;
	unsigned long flags;
	struct mddp_f_track_table_t *curr_track_table = NULL;
	struct mddp_f_track_table_t *tmp_track_table = NULL;

	pr_notice("%s: Delete all track table!\n", __func__);

	/* Free table buffer on hash table */
	for (i = 0; i < MDDP_F_MAX_TRACK_NUM; i++) {
		TRACK_TABLE_LOCK(mddp_f_track[i], flags);
		curr_track_table = mddp_f_track[i].table;

		while (curr_track_table) {
			tmp_track_table = curr_track_table->next_track_table;
			kfree(curr_track_table);
			curr_track_table = tmp_track_table;
		}
		mddp_f_track[i].table = NULL;
		TRACK_TABLE_UNLOCK(mddp_f_track[i], flags);
	}

	/* Free unused table buffer */
	TRACK_TABLE_LOCK(mddp_f_table_buffer, flags);
	curr_track_table = mddp_f_table_buffer.table;

	while (curr_track_table) {
		tmp_track_table = curr_track_table->next_track_table;
		kfree(curr_track_table);
		curr_track_table = tmp_track_table;
	}
	TRACK_TABLE_UNLOCK(mddp_f_table_buffer, flags);
}

#endif

#ifdef MDDP_F_NETFILTER
#define MDDP_F_DEVICE_DYNAMIC_REGISTERING_PROC_ENTRY "driver/mddp_f_dynamic_registered_device"
#define MDDP_F_DEVICE_REGISTERING_HASH_SIZE (32)
static spinlock_t device_registering_lock;
static struct list_head *device_registering_hash;
struct device_registering {
	struct list_head list;
	struct net_device *dev;
};
static int mddp_f_dynamic_register_proc_show(
	struct seq_file *m,
	void *v)
{
	unsigned long flags;
	struct device_registering *found_device;
	int i;

	spin_lock_irqsave(&device_registering_lock, flags);
	for (i = 0; i < MDDP_F_DEVICE_REGISTERING_HASH_SIZE; i++) {
		list_for_each_entry(found_device, &device_registering_hash[i],
							list) {
			seq_printf(m, "%s\n", found_device->dev->name);
		}
	}
	spin_unlock_irqrestore(&device_registering_lock, flags);
	return 0;
}

static int mddp_f_dynamic_register_proc_open(
	struct inode *inode,
	struct file *file)
{
	return single_open(file, mddp_f_dynamic_register_proc_show,
						NULL);
}

static const struct file_operations mddp_f_dynamic_register_proc_fops = {
	.owner = THIS_MODULE,
	.open = mddp_f_dynamic_register_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static inline unsigned int mddp_f_get_hash_by_device(struct net_device *dev)
{
	unsigned int ret;

	ret = jhash_1word((uintptr_t) dev, 0)
				% MDDP_F_DEVICE_REGISTERING_HASH_SIZE;

	return ret;
}

static inline void mddp_f_release_device(struct device_registering *device)
{
	list_del(&device->list);
	dev_put(device->dev);
	device->dev = NULL;
	kfree(device);
}
#endif

static int mddp_f_notifier_init(void)
{
	/* TODO: Peter DEBUG */
	int ret = 0;

#ifdef MDDP_F_NETFILTER
	int i;

	spin_lock_init(&device_registering_lock);
	device_registering_hash =
			vmalloc(sizeof(struct list_head)
					* MDDP_F_DEVICE_REGISTERING_HASH_SIZE);

	for (i = 0; i < MDDP_F_DEVICE_REGISTERING_HASH_SIZE; i++)
		INIT_LIST_HEAD(&device_registering_hash[i]);

	proc_create(MDDP_F_DEVICE_DYNAMIC_REGISTERING_PROC_ENTRY, 0000, NULL,
			&mddp_f_dynamic_register_proc_fops);
#endif

	return ret;
}

static void mddp_f_notifier_dest(void)
{
#ifdef MDDP_F_NETFILTER
	unsigned long flags;
	struct device_registering *found_device;
	int i;
#endif

#ifdef MDDP_F_NETFILTER
	remove_proc_entry(MDDP_F_DEVICE_DYNAMIC_REGISTERING_PROC_ENTRY, NULL);
	/* release all registered devices */
	spin_lock_irqsave(&device_registering_lock, flags);
	for (i = 0; i < MDDP_F_DEVICE_REGISTERING_HASH_SIZE; i++) {
		list_for_each_entry(found_device, &device_registering_hash[i],
							list) {
			mddp_f_release_device(found_device);
		}
	}
	spin_unlock_irqrestore(&device_registering_lock, flags);
	vfree(device_registering_hash);
#endif
}

static void mddp_f_init_jhash(void)
{
	get_random_bytes(&mddp_f_jhash_initval, sizeof(mddp_f_jhash_initval));
}

static bool mddp_f_tag_packet(
	bool is_uplink,
	struct sk_buff *skb,
	struct net_device *out,
	struct mddp_f_cb *cb,
	unsigned char protocol)
{
	struct mddp_f_tag_packet_t *skb_tag;
	unsigned char in_netif_id;
	unsigned char out_netif_id;
	struct sk_buff *fake_skb;
	unsigned char mddp_md_version = mddp_get_md_version();
	int ret = 0;

	if (is_uplink == true) { /* uplink*/
		if (mddp_md_version == 0) {
			in_netif_id = mddp_f_dev_name_to_id(cb->dev->name);
			out_netif_id = mddp_f_dev_name_to_id(out->name);

			skb_tag = (struct mddp_f_tag_packet_t *)skb->head;
			skb_tag->guard_pattern = MDDP_TAG_PATTERN;
			skb_tag->version = __MDDP_VERSION__;
			skb_tag->v1.in_netif_id = in_netif_id;
			skb_tag->v1.out_netif_id = out_netif_id;
			skb_tag->v1.port = cb->sport;

			pr_debug("%s: Add MDDP UL tag, guard_pattern[%x], version[%d], in_netif_id[%d], out_netif_id[%d], port[%x], skb[%p].\n",
					__func__, skb_tag->guard_pattern,
					skb_tag->version,
					skb_tag->v1.in_netif_id,
					skb_tag->v1.out_netif_id,
					skb_tag->v1.port, skb);
		} else if (mddp_md_version == 2) {
			skb_tag = (struct mddp_f_tag_packet_t *)skb->head;
			skb_tag->guard_pattern = MDDP_TAG_PATTERN;
			skb_tag->version = __MDDP_VERSION__;
			skb_tag->v2.tag_info = MDDP_RULE_TAG_NORMAL_PACKET;
			skb_tag->v2.lan_netif_id =
				mddp_f_dev_name_to_netif_id(cb->dev->name);
			skb_tag->v2.port = cb->sport;
			skb_tag->v2.ip = cb->src[0];  /* Don't care IPv6 IP */

			pr_debug("%s: Add MDDP UL tag, guard_pattern[%x], version[%d], tag_info[%d], lan_netif_id[%x], port[%x], ip[%x], skb[%p].\n",
					__func__, skb_tag->guard_pattern,
					skb_tag->version, skb_tag->v2.tag_info,
					skb_tag->v2.lan_netif_id,
					skb_tag->v2.port, skb_tag->v2.ip,
					skb);
		} else {
			WARN_ON(1);
			pr_notice("%s: Unsupported MD MDDP version[%d], AP MDDP version[%d].\n",
				__func__, mddp_md_version, __MDDP_VERSION__);
			ret = -EBADF;
		}
	} else { /* downlink */
		if (mddp_md_version == 0) {
			in_netif_id = mddp_f_dev_name_to_id(cb->dev->name);
			out_netif_id = mddp_f_dev_name_to_id(out->name);

			skb_tag = (struct mddp_f_tag_packet_t *)skb->head;
			skb_tag->guard_pattern = MDDP_TAG_PATTERN;
			skb_tag->version = __MDDP_VERSION__;
			skb_tag->v1.in_netif_id = in_netif_id;
			skb_tag->v1.out_netif_id = out_netif_id;
			skb_tag->v1.port = cb->dport;

			pr_debug("%s: Add MDDP DL tag, guard_pattern[%x], version[%d], in_netif_id[%d], out_netif_id[%d], port[%x], skb[%p].\n",
				__func__, skb_tag->guard_pattern,
				skb_tag->version, skb_tag->v1.in_netif_id,
				skb_tag->v1.out_netif_id, skb_tag->v1.port,
				skb);
		} else if (mddp_md_version == 2) {
			if (mddp_f_is_support_lan_dev(cb->dev->name) == true) {
				pr_notice("%s: Both in and out devices are lan devices. Do not tag the packet! out_device[%s], in_device[%s].\n",
					__func__, out->name, cb->dev->name);
				return -EFAULT;
			}

			fake_skb = skb_copy(skb, GFP_ATOMIC);
			fake_skb->dev = cb->dev;
			skb_tag = (struct mddp_f_tag_packet_t *)fake_skb->head;
			skb_tag->guard_pattern = MDDP_TAG_PATTERN;
			skb_tag->version = __MDDP_VERSION__;
			skb_tag->v2.tag_info = MDDP_RULE_TAG_FAKE_DL_NAT_PACKET;
			skb_tag->v2.lan_netif_id =
					mddp_f_dev_name_to_netif_id(out->name);
			skb_tag->v2.port = cb->dport;
			skb_tag->v2.ip = cb->dst[0];  /* Don't care IPv6 IP */

			dev_queue_xmit(fake_skb);

			pr_debug("%s: Add MDDP DL tag, guard_pattern[%x], version[%d], tag_info[%d], lan_netif_id[%x], port[%x], ip[%x], skb[%p], fake_skb[%p].\n",
					__func__, skb_tag->guard_pattern,
					skb_tag->version, skb_tag->v2.tag_info,
					skb_tag->v2.lan_netif_id,
					skb_tag->v2.port, skb_tag->v2.ip,
					skb, fake_skb);
		} else {
			WARN_ON(1);
			pr_notice("%s: Unsupported MD MDDP version[%d], AP MDDP version[%d].\n",
				__func__, mddp_md_version, __MDDP_VERSION__);
			ret = -EBADF;
		}
	}

	return ret;
}

static inline void _mddp_f_in_tail(
	u_int8_t iface,
	struct mddp_f_desc *desc,
	struct mddp_f_cb *cb,
	struct sk_buff *skb)
{
	struct ip4header *ip;
	struct ip6header *ip6;
	struct tcpheader *tcp;
	struct udpheader *udp;

	cb = add_track_table(skb, desc);
	if (!cb) {
		pr_debug("%s: Add track table failed, skb[%p], desc[%p]!\n",
				__func__, skb, desc);
		return;
	}

	cb->flag = desc->flag;

	if (desc->flag & DESC_FLAG_TRACK_NAT) {

		cb->dev = skb->dev;

		ip = (struct ip4header *) (skb->data + desc->l3_off);

		cb->src[0] = ip->ip_src;
		cb->dst[0] = ip->ip_dst;
		switch (ip->ip_p) {
		case IPPROTO_TCP:
			tcp = (struct tcpheader *) (skb->data + desc->l4_off);

			cb->sport = tcp->th_sport;
			cb->dport = tcp->th_dport;
			break;
		case IPPROTO_UDP:
			udp = (struct udpheader *) (skb->data + desc->l4_off);

			cb->sport = udp->uh_sport;
			cb->dport = udp->uh_dport;
			break;
		default:
			pr_notice("IPv4 BUG, %s: Should not reach here, skb[%p], protocol[%d].\n",
						__func__, skb, ip->ip_p);
			break;
		}
		cb->proto = ip->ip_p;
		cb->v4_ip_id = ip->ip_id;

		return;
	} else if (desc->flag & DESC_FLAG_TRACK_ROUTER) {
		/* Now only support IPv6 (20130104) */
		if (desc->flag & DESC_FLAG_IPV6) {
			ip6 = (struct ip6header *) (skb->data + desc->l3_off);

			cb->dev = skb->dev;
		} else {
			pr_notice("%s: Invalid router flag[%x], skb[%p]!\n",
					__func__, desc->flag, skb);

			return;
		}

		ipv6_addr_copy((struct in6_addr *)(&cb->src), &ip6->saddr);
		ipv6_addr_copy((struct in6_addr *)(&cb->dst), &ip6->daddr);
		switch (ip6->nexthdr) {
		case IPPROTO_TCP:
			tcp = (struct tcpheader *) (skb->data + desc->l4_off);

			cb->sport = tcp->th_sport;
			cb->dport = tcp->th_dport;
			break;
		case IPPROTO_UDP:
			udp = (struct udpheader *) (skb->data + desc->l4_off);

			cb->sport = udp->uh_sport;
			cb->dport = udp->uh_dport;
			break;
		default:
			pr_notice("IPv6 BUG, %s: Should not reach here, skb[%p], protocol[%d].\n",
						__func__, skb, ip6->nexthdr);
			break;
		}
		cb->proto = ip6->nexthdr;

		return;
	}
	pr_debug("%s: Invalid flag[%x], skb[%p]!\n",
				__func__, desc->flag, skb);
}

static inline void _mddp_f_in_nat(
	unsigned char *offset2,
	struct mddp_f_desc *desc,
	struct sk_buff *skb,
	int iface)
{
	/* IPv4 */
	struct tuple t4;
	struct ip4header *ip;
	/* IPv6 */
	struct router_tuple t6;
	struct ip6header *ip6;
	__be16 ip6_frag_off;

	void *l4_header;


	if (likely(IPC_HDR_IS_V4(offset2))) {	/* support NAT and ROUTE */
		desc->flag |= DESC_FLAG_IPV4;

		ip = (struct ip4header *) offset2;

		pr_debug("%s: IPv4 pre-routing, skb[%p], ip_id[%x], checksum[%x], protocol[%d], in_dev[%s].\n",
				__func__, skb, ip->ip_id,
				ip->ip_sum, ip->ip_p, skb->dev->name);

		/* ip fragmentation? */
		if (ip->ip_off & 0xff3f) {
			desc->flag |= DESC_FLAG_IPFRAG;
			return;
		}

		desc->l3_len = ip->ip_hl << 2;
		desc->l4_off = desc->l3_off + desc->l3_len;
		l4_header = skb->data + desc->l4_off;

		t4.nat.src = ip->ip_src;
		t4.nat.dst = ip->ip_dst;
		t4.nat.proto = ip->ip_p;
#ifdef MDDP_F_NETFILTER
		t4.dev_in = skb->dev;
#endif

		switch (ip->ip_p) {
		case IPPROTO_TCP:
			mddp_f_ip4_tcp(desc, skb, &t4, &ifaces[iface],
						ip, l4_header);
			return;
		case IPPROTO_UDP:
			mddp_f_ip4_udp(desc, skb, &t4, &ifaces[iface],
						ip, l4_header);
			return;
		default:
			desc->flag |= DESC_FLAG_UNKNOWN_PROTOCOL;
			return;
		}
	} else if (IPC_HDR_IS_V6(offset2)) {	/* only support ROUTE */
		desc->flag |= DESC_FLAG_IPV6;

		ip6 = (struct ip6header *) offset2;

		pr_debug("%s: IPv6 pre-routing, skb[%p], protocol[%d].\n",
				__func__, skb, ip6->nexthdr);

		t6.proto = ip6->nexthdr;
		desc->l3_len =
			ipv6_skip_exthdr(skb,
				desc->l3_off + sizeof(struct ip6header),
				&t6.proto, &ip6_frag_off) - desc->l3_off;
		desc->l4_off = desc->l3_off + desc->l3_len;
		l4_header = skb->data + desc->l4_off;

		/* ip fragmentation? */
		if (unlikely(ip6_frag_off > 0)) {
			desc->flag |= DESC_FLAG_IPFRAG;
			return;
		}

		t6.dev_src = skb->dev;
		ipv6_addr_copy((struct in6_addr *)(&(t6.saddr)), &(ip6->saddr));
		ipv6_addr_copy((struct in6_addr *)(&(t6.daddr)), &(ip6->daddr));

		switch (t6.proto) {
		case IPPROTO_TCP:
			mddp_f_ip6_tcp_lan(desc, skb, &t6, &ifaces[iface],
							ip6, l4_header);
			return;
		case IPPROTO_UDP:
			mddp_f_ip6_udp_lan(desc, skb, &t6, &ifaces[iface],
							ip6, l4_header);
			return;
		default:
			{
				desc->flag |= DESC_FLAG_UNKNOWN_PROTOCOL;
				return;
			}
		}
	} else {
		memset(desc, 0, sizeof(*desc));	/* avoid compiler warning */
		desc->flag |= DESC_FLAG_UNKNOWN_ETYPE;
		return;
	}
}

static inline int mddp_f_in_internal(int iface, struct sk_buff *skb)
{
	struct mddp_f_cb *cb;
	struct mddp_f_desc desc;

	pm_reset_traffic();

	cb = (struct mddp_f_cb *) (&skb->cb[48]);
	/* reset cb flag ?? */
	/* cb->flag = 0; */

	/* HW */
	desc.flag = 0;
	desc.l3_off = 0;


	skb_set_network_header(skb, desc.l3_off);

	_mddp_f_in_nat(skb->data, &desc, skb, iface);
	if (desc.flag & DESC_FLAG_NOMEM) {
		pr_notice("%s: No memory, flag[%x], skb[%p].\n",
				__func__, desc.flag, skb);

		return 2;
	}
	if (desc.flag & (DESC_FLAG_UNKNOWN_ETYPE |
			DESC_FLAG_UNKNOWN_PROTOCOL | DESC_FLAG_IPFRAG)) {
		/* un-handled packet, so pass it to kernel stack */
		pr_debug("%s: Un-handled packet, pass it to kernel stack, flag[%x], skb[%p].\n",
					__func__, desc.flag, skb);
		return 0;
	}

	_mddp_f_in_tail(iface, &desc, cb, skb);

	return 0;		/* original path */
}

int mddp_f_in_nf(int iface, struct sk_buff *skb)
{
	int ret;
	/* JQ: Remove lock */
	/* unsigned long flag; */

	/* MDDP_F_LOCK(&mddp_f_lock, flag); */
	ret = mddp_f_in_internal(iface, skb);
	/* MDDP_F_UNLOCK(&mddp_f_lock, flag); */
	return ret;
}

#ifdef MDDP_F_NETFILTER
void mddp_f_out_nf_ipv4(
	int iface,
	struct sk_buff *skb,
	struct net_device *out,
	unsigned char *offset2,
	struct mddp_f_cb *cb,
	struct mddp_f_track_table_t *curr_track_table)
{
	struct nf_conn *nat_ip_conntrack;
	enum ip_conntrack_info ctinfo;
	struct nat_tuple *t;
	struct tcpheader *tcp;
	struct udpheader *udp;
	unsigned char tcp_state;
	unsigned char ext_offset;
	bool is_uplink;
	int not_match = 0;
	unsigned char mddp_md_version = mddp_get_md_version();
	int ret;

	struct ip4header *ip = (struct ip4header *) offset2;

	pr_debug("%s: IPv4 add rule, skb[%p], cb->proto[%d], ip->ip_p[%d], offset2[%p], ip_id[%x], checksum[%x].\n",
				__func__, skb, cb->proto, ip->ip_p,
				offset2, ip->ip_id, ip->ip_sum);

	if (cb->proto != ip->ip_p) {
		pr_info("%s: IPv4 protocol mismatch, cb->proto[%d], ip->ip_p[%d], skb[%p] is filtered out.\n",
					__func__, cb->proto, ip->ip_p, skb);
		goto out;
	}

	offset2 += (ip->ip_hl << 2);

	switch (ip->ip_p) {
	case IPPROTO_TCP:
		tcp = (struct tcpheader *) offset2;
		nat_ip_conntrack = (struct nf_conn *)skb->nfct;
		tcp_state = nat_ip_conntrack->proto.tcp.state;
		ext_offset = nat_ip_conntrack->ext->offset[NF_CT_EXT_HELPER];
		ctinfo = skb->nfctinfo;

		if (!nat_ip_conntrack) {
			pr_notice("%s: Null ip conntrack, skb[%p] is filtered out.\n",
					__func__, skb);
			goto out;
		}

		if (nat_ip_conntrack->ext && ext_offset) { /* helper */
			pr_info("%s: skb[%p] is filtered out, ext[%p], ext_offset[%d].\n",
					__func__, skb,
					nat_ip_conntrack->ext, ext_offset);
			goto out;
		}

		if (mddp_f_contentfilter
				&& (tcp->th_dport == htons(80)
				|| tcp->th_sport == htons(80))
				&& nat_ip_conntrack->mark != 0x80000000) {
			pr_notice("%s: Invalid parameter, contentfilter[%d], dport[%x], sport[%x], mark[%x], skb[%p] is filtered out,.\n",
					__func__, mddp_f_contentfilter,
					tcp->th_dport,
					tcp->th_sport,
					nat_ip_conntrack->mark, skb);
			goto out;
		}

		if (tcp_state >= TCP_CONNTRACK_FIN_WAIT
				&& tcp_state <=	TCP_CONNTRACK_CLOSE) {
			pr_notice("%s: Invalid TCP state[%d], skb[%p] is filtered out.\n",
						__func__, tcp_state, skb);
			goto out;
		} else if (tcp_state !=	TCP_CONNTRACK_ESTABLISHED) {
			pr_notice("%s: TCP state[%d] is not in ESTABLISHED state, skb[%p] is filtered out.\n",
						__func__, tcp_state, skb);
			goto out;
		}

		if (mddp_f_is_support_wan_dev(out->name) == true) {
			is_uplink = true;

			not_match += (ip->ip_dst != cb->dst[0]) ? 1 : 0;
			not_match += (ip->ip_p != cb->proto) ? 1 : 0;
			not_match += (ip->ip_id != cb->v4_ip_id) ? 1 : 0;
			not_match += (tcp->th_dport != cb->dport) ? 1 : 0;
			if (not_match) {
				pr_debug("%s: IPv4 TCP UL tag not_match[%d], ip_dst[%x], ip_p[%d], ip_id[%x], dport[%x], cb_dst[%x], cb_proto[%d], cb_ip_id[%x], cb_dport[%x].\n",
					__func__, not_match, ip->ip_dst,
					ip->ip_p, ip->ip_id,
					tcp->th_dport, cb->dst[0],
					cb->proto, cb->v4_ip_id,
					cb->dport);

				goto out;
			}
		} else {
			is_uplink = false;

			if (mddp_md_version == 0) {
				not_match += (ip->ip_src != cb->src[0]) ? 1 : 0;
				not_match += (ip->ip_p != cb->proto) ? 1 : 0;
				not_match +=
					(ip->ip_id != cb->v4_ip_id) ? 1 : 0;
				not_match +=
					(tcp->th_sport != cb->sport) ? 1 : 0;
				if (not_match) {
					pr_debug("%s: IPv4 TCP DL tag not_match[%d], ip_src[%x], ip_p[%d], ip_id[%x], sport[%x], cb_src[%x], cb_proto[%d], cb_ip_id[%x], cb_sport[%x].\n",
						__func__, not_match, ip->ip_src,
						ip->ip_p, ip->ip_id,
						tcp->th_sport, cb->src[0],
						cb->proto, cb->v4_ip_id,
						cb->sport);

					goto out;
				}
			} else {
				/* Do not tag TCP DL packet */
				pr_debug("%s: Do not tag IPv4 TCP DL.\n",
						__func__);

				goto out;
			}
		}

		/* Tag this packet for MD tracking */
		if (skb_headroom(skb) > sizeof(struct mddp_f_tag_packet_t)) {
			ret = mddp_f_tag_packet(is_uplink,
					skb, out, cb, ip->ip_p);
			if (ret == 0)
				pr_debug("%s: Add IPv4 TCP MDDP tag, is_uplink[%d], skb[%p], ip_id[%x], ip_checksum[%x].\n",
						__func__, is_uplink, skb,
						ip->ip_id, ip->ip_sum);

			if (mddp_md_version == 0)
				goto out;

			/* Save tuple to avoid tag many packets */
			t = kmem_cache_alloc(mddp_f_nat_tuple_cache,
							GFP_ATOMIC);
			t->src_ip = cb->src[0];
			t->dst_ip = cb->dst[0];
			t->nat_src_ip = ip->ip_src;
			t->nat_dst_ip = ip->ip_dst;
			t->src.tcp.port = cb->sport;
			t->dst.tcp.port = cb->dport;
			t->nat_src.tcp.port = tcp->th_sport;
			t->nat_dst.tcp.port = tcp->th_dport;
			t->proto = ip->ip_p;
			t->dev_src = cb->dev;
			t->dev_dst = out;

			mddp_f_add_nat_tuple(t);
		} else {
			pr_notice("%s: Headroom of skb[%p] is not enough to add MDDP tag, headroom[%d].\n",
				__func__, skb, skb_headroom(skb));
		}
		break;
	case IPPROTO_UDP:
		udp = (struct udpheader *) offset2;
		nat_ip_conntrack = (struct nf_conn *)skb->nfct;
		ext_offset = nat_ip_conntrack->ext->offset[NF_CT_EXT_HELPER];
		ctinfo = skb->nfctinfo;

		if (!nat_ip_conntrack) {
			pr_notice("%s: Null ip conntrack, skb[%p] is filtered out.\n",
					__func__, skb);
			goto out;
		}

		if (nat_ip_conntrack->ext && ext_offset) { /* helper */
			pr_notice("%s: skb[%p] is filtered out, ext[%p], ext_offset[%d].\n",
					__func__, skb,
					nat_ip_conntrack->ext, ext_offset);
			goto out;
		}

		if (mddp_f_is_support_wan_dev(out->name) == true) {
			is_uplink = true;

			not_match += (ip->ip_dst != cb->dst[0]) ? 1 : 0;
			not_match += (ip->ip_p != cb->proto) ? 1 : 0;
			not_match += (ip->ip_id != cb->v4_ip_id) ? 1 : 0;
			not_match += (udp->uh_dport != cb->dport) ? 1 : 0;
			if (not_match) {
				pr_debug("%s: IPv4 UDP UL tag not_match[%d], ip_dst[%x], ip_p[%d], ip_id[%x], dport[%x], cb_dst[%x], cb_proto[%d], cb_ip_id[%x], cb_dport[%x].\n",
					__func__, not_match, ip->ip_dst,
					ip->ip_p, ip->ip_id,
					udp->uh_dport, cb->dst[0],
					cb->proto, cb->v4_ip_id,
					cb->dport);

				goto out;
			}
		} else {
			is_uplink = false;

			not_match += (ip->ip_src != cb->src[0]) ? 1 : 0;
			not_match += (ip->ip_p != cb->proto) ? 1 : 0;
			not_match += (ip->ip_id != cb->v4_ip_id) ? 1 : 0;
			not_match += (udp->uh_sport != cb->sport) ? 1 : 0;
			if (not_match) {
				pr_debug("%s: IPv4 UDP DL tag not_match[%d], ip_src[%x], ip_p[%d], ip_id[%x], sport[%x], cb_src[%x], cb_proto[%d], cb_ip_id[%x], cb_sport[%x].\n",
					__func__, not_match, ip->ip_src,
					ip->ip_p, ip->ip_id,
					udp->uh_sport, cb->src[0],
					cb->proto, cb->v4_ip_id,
					cb->sport);

				goto out;
			}
		}

		if (cb->sport != 67 && cb->sport != 68) {
			/* Don't fastpath dhcp packet */

			/* Tag this packet for MD tracking */
			if (skb_headroom(skb)
					> sizeof(struct mddp_f_tag_packet_t)) {
				ret = mddp_f_tag_packet(is_uplink, skb,
							out, cb, ip->ip_p);
				if (ret == 0)
					pr_debug("%s: Add IPv4 UDP MDDP tag, is_uplink[%d], skb[%p], ip_id[%x], ip_checksum[%x].\n",
							__func__, is_uplink,
							skb, ip->ip_id,
							ip->ip_sum);

				if (mddp_md_version == 0)
					goto out;

				/* Save tuple to avoid tag many packets */
				t = kmem_cache_alloc(mddp_f_nat_tuple_cache,
								GFP_ATOMIC);
				t->src_ip = cb->src[0];
				t->dst_ip = cb->dst[0];
				t->nat_src_ip = ip->ip_src;
				t->nat_dst_ip = ip->ip_dst;
				t->src.udp.port = cb->sport;
				t->dst.udp.port = cb->dport;
				t->nat_src.udp.port = udp->uh_sport;
				t->nat_dst.udp.port = udp->uh_dport;
				t->proto = ip->ip_p;
				t->dev_src = cb->dev;
				t->dev_dst = out;

				mddp_f_add_nat_tuple(t);
			} else {
				pr_notice("%s: Headroom of skb[%p] is not enough to add MDDP tag, headroom[%d].\n",
					__func__, skb, skb_headroom(skb));
			}
		} else {
			pr_debug("%s: Don't track DHCP packet, s_port[%d], skb[%p] is filtered out.\n",
					__func__, cb->sport, skb);
		}
		break;
	default:
		pr_debug("%s: Not TCP/UDP packet, protocal[%d], skb[%p] is filtered out.\n",
					__func__, ip->ip_p, skb);
		break;
	}

out:
#ifdef MDDP_F_NO_KERNEL_SUPPORT
	put_track_table(curr_track_table);
#endif
}

void mddp_f_out_nf_ipv6(
	int iface,
	struct sk_buff *skb,
	struct net_device *out,
	struct mddp_f_cb *cb,
	int l3_off,
	struct mddp_f_track_table_t *curr_track_table)
{
	struct ip6header *ip6 = (struct ip6header *) (skb->data + l3_off);
	unsigned char nexthdr;
	__be16 frag_off;
	int l4_off = 0;
	struct nf_conn *nat_ip_conntrack;
	enum ip_conntrack_info ctinfo;
	struct router_tuple *t;
	struct tcpheader *tcp;
	struct udpheader *udp;
	unsigned char tcp_state;
	unsigned char ext_offset;
	bool is_uplink;
	int not_match = 0;
	unsigned char mddp_md_version = mddp_get_md_version();
	int ret;

	nexthdr = ip6->nexthdr;
	l4_off = ipv6_skip_exthdr(skb, l3_off + sizeof(struct ip6header),
							&nexthdr, &frag_off);
	switch (nexthdr) {
	case IPPROTO_TCP:
		tcp = (struct tcpheader *) (skb->data + l4_off);
		nat_ip_conntrack = (struct nf_conn *)skb->nfct;
		tcp_state = nat_ip_conntrack->proto.tcp.state;
		ext_offset = nat_ip_conntrack->ext->offset[NF_CT_EXT_HELPER];
		ctinfo = skb->nfctinfo;

		if (!nat_ip_conntrack) {
			pr_notice("%s: Null ip conntrack, skb[%p] is filtered out.\n",
					__func__, skb);
			goto out;
		}

		if (nat_ip_conntrack->ext && ext_offset) { /* helper */
			pr_info("%s: skb[%p] is filtered out, ext[%p], ext_offset[%d].\n",
					__func__, skb,
					nat_ip_conntrack->ext, ext_offset);
			goto out;
		}

		if (mddp_f_contentfilter
				&& (tcp->th_dport == htons(80)
				|| tcp->th_sport == htons(80))
				&& nat_ip_conntrack->mark != 0x80000000) {
			pr_notice("%s: Invalid parameter, contentfilter[%d], dport[%x], sport[%x], mark[%x], skb[%p] is filtered out,.\n",
				__func__, mddp_f_contentfilter, tcp->th_dport,
				tcp->th_sport, nat_ip_conntrack->mark, skb);
			goto out;
		}
		if (tcp_state >= TCP_CONNTRACK_FIN_WAIT
				&& tcp_state <=	TCP_CONNTRACK_CLOSE) {
			pr_notice("%s: Invalid TCP state[%d], skb[%p] is filtered out.\n",
						__func__, tcp_state, skb);
			goto out;
		} else if (tcp_state !=	TCP_CONNTRACK_ESTABLISHED) {
			pr_notice("%s: TCP state[%d] is not in ESTABLISHED state, skb[%p] is filtered out.\n",
						__func__, tcp_state, skb);
			goto out;
		}
#ifndef MDDP_F_NETFILTER
		if (cb->iface == iface) {
			pr_notice("BUG %s,%d: in_iface[%p] and out_iface[%p] are same.\n",
					__func__, __LINE__, cb->iface, iface);
			goto out;
		}
#else
		if (cb->dev == out) {
			pr_notice("BUG %s,%d: in_dev[%p] name[%s] and out_dev[%p] name [%s] are same.\n",
					__func__, __LINE__, cb->dev,
					cb->dev->name, out, out->name);
			goto out;
		}
#endif

		if (mddp_f_is_support_wan_dev(out->name) == true) {
			is_uplink = true;

			not_match +=
				(!ipv6_addr_equal(&ip6->saddr,
				(struct in6_addr *)(&cb->src))) ? 1 : 0;
			not_match +=
				(!ipv6_addr_equal(&ip6->daddr,
				(struct in6_addr *)(&cb->dst))) ? 1 : 0;
			not_match += (ip6->nexthdr != cb->proto) ? 1 : 0;
			not_match += (tcp->th_sport != cb->sport) ? 1 : 0;
			not_match += (tcp->th_dport != cb->dport) ? 1 : 0;
			if (not_match) {
				pr_debug("%s: IPv6 TCP UL tag not_match[%d], ip_p[%d], sport[%x], dport[%x], cb_proto[%d], cb_sport[%x], cb_dport[%x].\n",
					__func__, not_match, ip6->nexthdr,
					tcp->th_sport, tcp->th_dport, cb->proto,
					cb->sport, cb->dport);

				goto out;
			}
		} else {
			is_uplink = false;

			if (mddp_md_version == 0) {
				not_match +=
					(!ipv6_addr_equal(&ip6->saddr,
					(struct in6_addr *)(&cb->src))) ? 1 : 0;
				not_match +=
					(!ipv6_addr_equal(&ip6->daddr,
					(struct in6_addr *)(&cb->dst))) ? 1 : 0;
				not_match +=
					(ip6->nexthdr != cb->proto) ? 1 : 0;
				not_match +=
					(tcp->th_sport != cb->sport) ? 1 : 0;
				not_match +=
					(tcp->th_dport != cb->dport) ? 1 : 0;
				if (not_match) {
					pr_debug("%s: IPv6 TCP DL tag not_match[%d], ip_p[%d], sport[%x], dport[%x], cb_proto[%d], cb_sport[%x], cb_dport[%x].\n",
						__func__, not_match,
						ip6->nexthdr, tcp->th_sport,
						tcp->th_dport, cb->proto,
						cb->sport, cb->dport);

					goto out;
				}
			} else {
				/* Do not tag TCP DL packet */
				pr_debug("%s: Do not tag IPv6 TCP DL.\n",
						__func__);

				goto out;
			}
		}


		/* Tag this packet for MD tracking */
		if (skb_headroom(skb) > sizeof(struct mddp_f_tag_packet_t)) {
			ret = mddp_f_tag_packet(is_uplink,
					skb, out, cb, nexthdr);
			if (ret == 0)
				pr_debug("%s: Add IPv6 TCP MDDP tag, is_uplink[%d], skb[%p], tcp_checksum[%x].\n",
						__func__, is_uplink, skb,
						tcp->th_sum);

			if (mddp_md_version == 0)
				goto out;

			/* Save tuple to avoid tag many packets */
			t = kmem_cache_alloc(mddp_f_router_tuple_cache,
							GFP_ATOMIC);
#ifndef MDDP_F_NETFILTER
			t->iface_src = cb->iface;
			t->iface_dst = iface;
#else
			t->dev_src = cb->dev;
			t->dev_dst = out;
#endif
			ipv6_addr_copy(&t->saddr, &ip6->saddr);
			ipv6_addr_copy(&t->daddr, &ip6->daddr);
			t->in.tcp.port = tcp->th_sport;
			t->out.tcp.port = tcp->th_dport;
			t->proto = nexthdr;

			mddp_f_add_router_tuple_tcpudp(t);
		} else {
			pr_notice("%s: Headroom of skb[%p] is not enough to add MDDP tag, headroom[%d].\n",
					__func__, skb, skb_headroom(skb));
		}
		break;
	case IPPROTO_UDP:
		udp = (struct udpheader *)(skb->data + l4_off);
		nat_ip_conntrack = (struct nf_conn *)skb->nfct;
		ext_offset = nat_ip_conntrack->ext->offset[NF_CT_EXT_HELPER];
		ctinfo = skb->nfctinfo;

		if (!nat_ip_conntrack) {
			pr_notice("%s: Null ip conntrack, skb[%p] is filtered out.\n",
				__func__, skb);
			goto out;
		}

		if (nat_ip_conntrack->ext && ext_offset) { /* helper */
			pr_notice("%s: skb[%p] is filtered out, ext[%p], ext_offset[%d].\n",
				__func__, skb,
				nat_ip_conntrack->ext, ext_offset);
			goto out;
		}

#ifndef MDDP_F_NETFILTER
		if (cb->iface == iface)	{
			pr_notice("BUG %s,%d: in_iface[%p] and out_iface[%p] are same.\n",
					__func__, __LINE__,
					cb->iface, iface);
			goto out;
		}
#else
		if (cb->dev == out)	{
			pr_notice("BUG %s,%d: in_dev[%p] name[%s] and out_dev[%p] name [%s] are same.\n",
					__func__, __LINE__, cb->dev,
					cb->dev->name, out, out->name);
			goto out;
		}
#endif

		if (mddp_f_is_support_wan_dev(out->name) == true) {
			is_uplink = true;

			not_match +=
				(!ipv6_addr_equal(&ip6->saddr,
					(struct in6_addr *)(&cb->src))) ? 1 : 0;
			not_match +=
				(!ipv6_addr_equal(&ip6->daddr,
					(struct in6_addr *)(&cb->dst))) ? 1 : 0;
			not_match += (ip6->nexthdr != cb->proto) ? 1 : 0;
			not_match += (udp->uh_sport != cb->sport) ? 1 : 0;
			not_match += (udp->uh_dport != cb->dport) ? 1 : 0;
			if (not_match) {
				pr_debug("%s: IPv6 UDP UL tag not_match[%d], ip_p[%d], sport[%x], dport[%x], cb_proto[%d], cb_sport[%x], cb_dport[%x].\n",
					__func__, not_match, ip6->nexthdr,
					udp->uh_sport, udp->uh_dport, cb->proto,
					cb->sport, cb->dport);

				goto out;
			}
		} else {
			is_uplink = false;

			not_match +=
				(!ipv6_addr_equal(&ip6->saddr,
					(struct in6_addr *)(&cb->src))) ? 1 : 0;
			not_match +=
				(!ipv6_addr_equal(&ip6->daddr,
					(struct in6_addr *)(&cb->dst))) ? 1 : 0;
			not_match += (ip6->nexthdr != cb->proto) ? 1 : 0;
			not_match += (udp->uh_sport != cb->sport) ? 1 : 0;
			not_match += (udp->uh_dport != cb->dport) ? 1 : 0;
			if (not_match) {
				pr_debug("%s: IPv6 UDP DL tag not_match[%d], ip_p[%d], sport[%x], dport[%x], cb_proto[%d], cb_sport[%x], cb_dport[%x].\n",
					__func__, not_match, ip6->nexthdr,
					udp->uh_sport, udp->uh_dport, cb->proto,
					cb->sport, cb->dport);

				goto out;
			}
		}

		if (udp->uh_sport != 67 && udp->uh_sport != 68) {
			/* Don't fastpath dhcp packet */

			/* Tag this packet for MD tracking */
			if (skb_headroom(skb)
					> sizeof(struct mddp_f_tag_packet_t)) {
				ret = mddp_f_tag_packet(is_uplink, skb,
							out, cb, nexthdr);
				if (ret == 0)
					pr_debug("%s: Add IPv6 UDP MDDP tag, is_uplink[%d], skb[%p], udp_checksum[%x].\n",
							__func__, is_uplink,
							skb, udp->uh_check);

				if (mddp_md_version == 0)
					goto out;

				/* Save tuple to avoid tag many packets */
				t = kmem_cache_alloc(mddp_f_router_tuple_cache,
								GFP_ATOMIC);
#ifndef MDDP_F_NETFILTER
				t->iface_src = cb->iface;
				t->iface_dst = iface;
#else
				t->dev_src = cb->dev;
				t->dev_dst = out;
#endif
				ipv6_addr_copy(&t->saddr, &ip6->saddr);
				ipv6_addr_copy(&t->daddr, &ip6->daddr);
				t->in.udp.port = udp->uh_sport;
				t->out.udp.port = udp->uh_dport;
				t->proto = nexthdr;

				mddp_f_add_router_tuple_tcpudp(t);
			} else {
				pr_notice("%s: Headroom of skb[%p] is not enough to add MDDP tag, headroom[%d].\n",
					__func__, skb, skb_headroom(skb));
			}
		} else {
			pr_debug("%s: Don't track DHCP packet, s_port[%d], skb[%p] is filtered out.\n",
					__func__, cb->sport, skb);
		}
		break;
	default:
		pr_debug("%s: Not TCP/UDP packet, protocal[%d], skb[%p] is filtered out.\n",
					__func__, nexthdr, skb);
		break;
	}

out:
#ifdef MDDP_F_NO_KERNEL_SUPPORT
	put_track_table(curr_track_table);
#endif
}

void mddp_f_out_nf(int iface, struct sk_buff *skb, struct net_device *out)
{
	unsigned char *offset2 = skb->data;
	struct mddp_f_cb *cb;
	struct mddp_f_track_table_t *curr_track_table;

	pr_debug("%s: post-routing, add rule, skb[%p].\n",
				__func__, skb);

	pm_reset_traffic();

	cb = search_and_hold_track_table(skb, &curr_track_table);
	if (!cb) {
		pr_debug("%s: Cannot find cb, skb[%p].\n",
					__func__, skb);
		return;
	}

	if (cb->dev == out) {
		pr_info("%s: in_dev[%p] name[%s] and out_dev[%p] name[%s] are same, don't track skb[%p].\n",
			__func__, cb->dev, cb->dev->name, out, out->name, skb);
		goto out;
	}

	if (cb->dev == NULL || out == NULL) {
		pr_info("%s: Each of in_dev[%p] or out_dev[%p] is NULL, don't track skb[%p].\n",
					__func__, cb->dev, out, skb);
		goto out;
	}

	if (cb->flag & DESC_FLAG_TRACK_NAT) {
		if (IPC_HDR_IS_V4(offset2)) {
			mddp_f_out_nf_ipv4(iface, skb, out, offset2, cb,
							curr_track_table);
			return;

		} else {
			pr_notice("%s: Wrong IPv4 version[%d], offset[%p], flag[%x], skb[%p].\n",
					__func__, IPC_HDR_IS_V4(offset2),
					offset2, cb->flag, skb);
		}


	} else if (cb->flag & DESC_FLAG_TRACK_ROUTER) {
		int l3_off = 0;

		if (cb->flag & DESC_FLAG_IPV6) {
			mddp_f_out_nf_ipv6(iface, skb, out, cb, l3_off,
							curr_track_table);
			return;

		} else {
			pr_notice("%s: Invalid IPv6 flag[%x], skb[%p].\n",
						__func__, cb->flag, skb);
		}

	} else {
		pr_debug("%s: No need to track, skb[%p], cb->flag[%x].\n",
					__func__, skb, cb->flag);
	}

out:
#ifdef MDDP_F_NO_KERNEL_SUPPORT
	put_track_table(curr_track_table);
#endif
}
//EXPORT_SYMBOL(mddp_f_out_nf);
module_param(mddp_f_contentfilter, int, 0000);

#endif
//EXPORT_SYMBOL(mddp_f_in_nf);

static uint32_t mddp_nfhook_prerouting
(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
	struct mddp_app_t      *mdu;
	enum mddp_state_e       curr_state;

	// TODO: support MDDP-WH type
	mdu = mddp_get_app_inst(MDDP_APP_TYPE_USB);
	curr_state = mddp_get_state(mdu);

	if (curr_state != MDDP_STATE_ACTIVATED) {
		//pr_debug("%s: Wrong state, curr_state(%d)!\n",
		//		__func__, curr_state);
		return NF_ACCEPT;
	}

	if (unlikely(!state->in || !skb->dev || !skb_mac_header_was_set(skb))) {
		pr_debug("%s: Invalid param, in(%p), dev(%p), mac(%d)!\n",
			__func__, state->in, skb->dev,
			skb_mac_header_was_set(skb));
		return NF_ACCEPT;
	}

	if ((state->in->priv_flags & IFF_EBRIDGE) ||
			(state->in->flags & IFF_LOOPBACK)) {
		pr_debug("%s: Invalid flag, priv_flags(%x), flags(%x)!\n",
			__func__, state->in->priv_flags, state->in->flags);
		return NF_ACCEPT;
	}

	if (!mddp_f_is_support_dev(state->in->name)) {
		pr_debug("%s: Unsupport device, state->in->name(%s)!\n",
			__func__, state->in->name);
		return NF_ACCEPT;
	}

	mddp_f_in_nf(0, skb);
	return NF_ACCEPT;
}

static uint32_t mddp_nfhook_postrouting
(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
	struct mddp_app_t      *mdu;
	enum mddp_state_e       curr_state;

	mdu = mddp_get_app_inst(MDDP_APP_TYPE_USB);
	curr_state = mddp_get_state(mdu);

	if (curr_state != MDDP_STATE_ACTIVATED) {
		//pr_debug("%s: Wrong state, curr_state(%d).\n",
		//		__func__, curr_state);
		return NF_ACCEPT;
	}

	if (unlikely(!state->out || !skb->dev ||
				(skb_headroom(skb) < ETH_HLEN))) {
		pr_debug("%s: Invalid parameter, out(%p), dev(%p), headroom(%d)\n",
			__func__, state->out, skb->dev, skb_headroom(skb));
		goto out;
	}

	if ((state->out->priv_flags & IFF_EBRIDGE) ||
			(state->out->flags & IFF_LOOPBACK)) {
		pr_debug("%s: Invalid flag, priv_flags(%x), flags(%x).\n",
			__func__, state->out->priv_flags, state->out->flags);
		return NF_ACCEPT;
	}

	if (!mddp_f_is_support_dev(state->out->name)) {
		pr_debug("%s: Unsuport device,state->out->name(%s).\n",
			__func__, state->out->name);
		return NF_ACCEPT;
	}

	mddp_f_out_nf(0, skb, state->out);

out:
	return NF_ACCEPT;
}

//------------------------------------------------------------------------------
// Public functions.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Kernel functions.
//------------------------------------------------------------------------------
int32_t mddp_filter_init(void)
{
	int ret = 0;

	ret = nf_register_hooks(mddp_nf_ops, ARRAY_SIZE(mddp_nf_ops));
	if (ret < 0) {
		pr_notice("%s: Cannot register hooks.\n", __func__);
		return ret;
	}

	memset(ifaces, 0, sizeof(ifaces));

	MDDP_F_INIT_LOCK(&mddp_f_lock);
	MDDP_F_TUPLE_INIT_LOCK(&mddp_f_tuple_lock);

#ifdef MDDP_F_NO_KERNEL_SUPPORT
	mddp_f_init_table_buffer();
	mddp_f_init_track_table();
	mddp_f_init_jhash();
#endif

	mddp_f_init_nat_tuple();
	mddp_f_init_router_tuple();

	mddp_f_nat_tuple_cache =
		kmem_cache_create("mddp_f_nat_tuple",
					sizeof(struct nat_tuple), 0,
					SLAB_HWCACHE_ALIGN, NULL);
	mddp_f_router_tuple_cache =
		kmem_cache_create("mddp_f_router_tuple",
					sizeof(struct router_tuple), 0,
					SLAB_HWCACHE_ALIGN, NULL);

	ret = mddp_f_notifier_init();
	if (ret < 0) {
		pr_notice("%s: mddp_f_notifier_init failed with ret = %d\n",
				__func__, ret);
		return ret;
	}

	return 0;
}

void mddp_filter_uninit(void)
{
	nf_unregister_hooks(mddp_nf_ops, ARRAY_SIZE(mddp_nf_ops));
	mddp_f_notifier_dest();
#ifdef MDDP_F_NO_KERNEL_SUPPORT
	dest_track_table();
#endif
}

MODULE_LICENSE("GPL");
