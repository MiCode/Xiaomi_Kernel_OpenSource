/*************************************************************************
 * -----------------------------------------------------------------------
 * Copyright (c) 2013-2015, 2017, The Linux Foundation. All rights reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * -----------------------------------------------------------------------

 * DESCRIPTION
 * Main file for eMBMs Tunneling Module in kernel.
 *************************************************************************
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <net/ip.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/etherdevice.h>

#include <linux/inetdevice.h>
#include <linux/netfilter.h>
#include <net/arp.h>
#include <net/neighbour.h>

#include <linux/skbuff.h>
#include <linux/list.h>
#include <linux/in.h>
#include <net/netfilter/nf_conntrack.h>
#include <linux/miscdevice.h>
#include "embms_kernel.h"

struct embms_info_internal embms_conf;

/* Global structures used for tunneling. These include
 * iphdr and udphdr which are appended to skbs for
 * tunneling, net_device and tunnleing related
 * structs and params
 */

unsigned char hdr_buff[sizeof(struct iphdr) + sizeof(struct udphdr)];
struct iphdr *iph_global;
struct udphdr *udph_global;
struct net_device *dev_global;

static struct tmgi_to_clnt_info tmgi_to_clnt_map_tbl;

/* handle_multicast_stream - packet forwarding
 * function for multicast stream
 * Main use case is for EMBMS Over Softap feature
 */

static int handle_multicast_stream(struct sk_buff *skb)
{
	struct iphdr *iph;
	struct udphdr *udph;
	unsigned char *tmp_ptr = NULL;
	struct sk_buff *skb_new = NULL;
	struct sk_buff *skb_cpy = NULL;
	struct clnt_info *temp_client = NULL;
	struct tmgi_to_clnt_info *temp_tmgi = NULL;
	struct list_head *tmgi_entry_ptr, *prev_tmgi_entry_ptr;
	struct list_head *clnt_ptr, *prev_clnt_ptr;
	int hdr_size = sizeof(*udph) + sizeof(*iph) + ETH_HLEN;

	/* only IP packets */
	if (htons(ETH_P_IP) != skb->protocol) {
		embms_error("Not an IP packet\n");
		return 0;
	}

	if (embms_conf.embms_tunneling_status == TUNNELING_OFF) {
		embms_debug("Tunneling Disabled. Can't process packets\n");
		return 0;
	}

	if (unlikely(memcmp(skb->dev->name, embms_conf.embms_iface,
			    strlen(embms_conf.embms_iface)) != 0)) {
		embms_error("Packet received on %s iface. NOT an EMBMS Iface\n",
			    skb->dev->name);
		return 0;
	}

	/* Check if dst ip of packet is same as multicast ip of any tmgi*/

	iph = (struct iphdr *)skb->data;
	udph = (struct udphdr *)(skb->data + sizeof(struct iphdr));

	spin_lock_bh(&embms_conf.lock);

	list_for_each_safe(tmgi_entry_ptr, prev_tmgi_entry_ptr,
			   &tmgi_to_clnt_map_tbl.tmgi_list_ptr) {
		temp_tmgi = list_entry(tmgi_entry_ptr,
				       struct tmgi_to_clnt_info,
				       tmgi_list_ptr);

		if ((temp_tmgi->tmgi_multicast_addr == iph->daddr) &&
		    (temp_tmgi->tmgi_port == udph->dest))
			break;
	}

	if (tmgi_entry_ptr == &tmgi_to_clnt_map_tbl.tmgi_list_ptr) {
		embms_error("handle_multicast_stream:");
		embms_error("could not find matchin tmgi entry\n");
		spin_unlock_bh(&embms_conf.lock);
		return 0;
	}

	/* Found a matching tmgi entry. Realloc headroom to
	 * accommodate new Ethernet, IP and UDP header
	 */

	skb_new = skb_realloc_headroom(skb, hdr_size);
	if (unlikely(!skb_new)) {
		embms_error("Can't allocate headroom\n");
		spin_unlock_bh(&embms_conf.lock);
		return 0;
	}

	/* push skb->data and copy IP and UDP headers*/

	tmp_ptr = skb_push(skb_new,
			   sizeof(struct udphdr) + sizeof(struct iphdr));

	iph = (struct iphdr *)tmp_ptr;
	udph = (struct udphdr *)(tmp_ptr + sizeof(struct iphdr));

	memcpy(tmp_ptr, hdr_buff, hdr_size - ETH_HLEN);
	udph->len = htons(skb_new->len - sizeof(struct iphdr));
	iph->tot_len = htons(skb_new->len);

	list_for_each_safe(clnt_ptr, prev_clnt_ptr,
			   &temp_tmgi->client_list_head) {
		temp_client = list_entry(clnt_ptr,
					 struct clnt_info,
					 client_list_ptr);

		/* Make a copy of skb_new with new IP and UDP header.
		 * We can't use skb_new or its clone here since we need to
		 * constantly change dst ip and dst port which is not possible
		 * for shared memory as is the case with skb_new.
		 */

		skb_cpy = skb_copy(skb_new, GFP_ATOMIC);
		if (unlikely(!skb_cpy)) {
			embms_error("Can't copy skb\n");
			kfree_skb(skb_new);
			return 0;
		}

		iph = (struct iphdr *)skb_cpy->data;
		udph = (struct udphdr *)(skb_cpy->data + sizeof(struct iphdr));

		iph->id = htons(atomic_inc_return(&embms_conf.ip_ident));

		/* Calculate checksum for new IP and UDP header*/

		udph->dest = temp_client->port;
		skb_cpy->csum = csum_partial((char *)udph,
					     ntohs(udph->len),
					     skb_cpy->csum);

		iph->daddr = temp_client->addr;
		ip_send_check(iph);

		udph->check = 0;
		udph->check = csum_tcpudp_magic(iph->saddr, iph->daddr,
						ntohs(udph->len),
						IPPROTO_UDP,
						skb_cpy->csum);

		if (udph->check == 0)
			udph->check = CSUM_MANGLED_0;

		if (unlikely(!dev_global)) {
			embms_error("Global device NULL\n");
			kfree_skb(skb_cpy);
			kfree_skb(skb_new);
			return 0;
		}

		/* update device info and add MAC header*/

		skb_cpy->dev = dev_global;

		skb_cpy->dev->header_ops->create(skb_cpy, skb_cpy->dev,
						ETH_P_IP, temp_client->dmac,
						NULL, skb_cpy->len);
		dev_queue_xmit(skb_cpy);
	}

	spin_unlock_bh(&embms_conf.lock);
	kfree_skb(skb_new);
	return 1;
}

static int check_embms_device(atomic_t *use_count)
{
	int ret;

	if (atomic_inc_return(use_count) == 1) {
		ret = 0;
	} else {
		atomic_dec(use_count);
		ret = -EBUSY;
	}
	return ret;
}

static int embms_device_open(struct inode *inode, struct file *file)
{
	/*Check if the device is busy*/
	if (check_embms_device(&embms_conf.device_under_use)) {
		embms_error("embms_tm_open : EMBMS device busy\n");
		return -EBUSY;
	}

	try_module_get(THIS_MODULE);
	return SUCCESS;
}

static int embms_device_release(struct inode *inode, struct file *file)
{
	/* Reduce device use count before leaving*/
	embms_debug("Releasing EMBMS device..\n");
	atomic_dec(&embms_conf.device_under_use);
	embms_conf.embms_tunneling_status = TUNNELING_OFF;
	module_put(THIS_MODULE);
	return SUCCESS;
}

static struct tmgi_to_clnt_info *check_for_tmgi_entry(u32 addr,
						      u16 port)
{
	struct list_head *tmgi_ptr, *prev_tmgi_ptr;
	struct tmgi_to_clnt_info *temp_tmgi = NULL;

	embms_debug("check_for_tmgi_entry: mcast addr :%pI4, port %u\n",
		    &addr, ntohs(port));

	list_for_each_safe(tmgi_ptr,
			   prev_tmgi_ptr,
			   &tmgi_to_clnt_map_tbl.tmgi_list_ptr) {
		temp_tmgi = list_entry(tmgi_ptr,
				       struct tmgi_to_clnt_info,
				       tmgi_list_ptr);

		if ((temp_tmgi->tmgi_multicast_addr == addr) &&
		    (temp_tmgi->tmgi_port == port)) {
			embms_debug("check_for_tmgi_entry:TMGI entry found\n");
			return temp_tmgi;
		}
	}
	return NULL;
}

static struct clnt_info *chk_clnt_entry(struct tmgi_to_clnt_info *tmgi,
					struct tmgi_to_clnt_info_update *clnt)
{
	struct list_head *clnt_ptr, *prev_clnt_ptr;
	struct clnt_info *temp_client = NULL;

	embms_debug("check_for_client_entry: clnt addr :%pI4, port %u\n",
		    &clnt->client_addr, ntohs(clnt->client_port));

	list_for_each_safe(clnt_ptr,
			   prev_clnt_ptr,
			   &tmgi->client_list_head) {
		temp_client = list_entry(clnt_ptr,
					 struct clnt_info,
					 client_list_ptr);
		if ((temp_client->addr == clnt->client_addr) &&
		    (temp_client->port == clnt->client_port)) {
			embms_debug("Clnt entry present\n");
			return temp_client;
		}
	}
	return NULL;
}

static int add_new_tmgi_entry(struct tmgi_to_clnt_info_update *info_update,
			      struct clnt_info *clnt)
{
	struct tmgi_to_clnt_info *new_tmgi = NULL;

	embms_debug("add_new_tmgi_entry:Enter\n");

	new_tmgi = kzalloc(sizeof(*new_tmgi),
			   GFP_ATOMIC);
	if (!new_tmgi) {
		embms_error("add_new_tmgi_entry: mem alloc failed\n");
		return -ENOMEM;
	}

	memset(new_tmgi, 0, sizeof(struct tmgi_to_clnt_info));

	new_tmgi->tmgi_multicast_addr = info_update->multicast_addr;
	new_tmgi->tmgi_port = info_update->multicast_port;

	embms_debug("add_new_tmgi_entry:");
	embms_debug("New tmgi multicast addr :%pI4 , port %u\n",
		    &info_update->multicast_addr,
		    ntohs(info_update->multicast_port));

	embms_debug("add_new_tmgi_entry:Adding client entry\n");

	spin_lock_bh(&embms_conf.lock);

	INIT_LIST_HEAD(&new_tmgi->client_list_head);
	list_add(&clnt->client_list_ptr,
		 &new_tmgi->client_list_head);
	new_tmgi->no_of_clients++;

	/* Once above steps are done successfully,
	 * we add tmgi entry to our local table
	 */

	list_add(&new_tmgi->tmgi_list_ptr,
		 &tmgi_to_clnt_map_tbl.tmgi_list_ptr);
	embms_conf.no_of_tmgi_sessions++;

	spin_unlock_bh(&embms_conf.lock);

	return SUCCESS;
}

static void print_tmgi_to_client_table(void)
{
	int i, j;
	struct clnt_info *temp_client = NULL;
	struct tmgi_to_clnt_info *temp_tmgi = NULL;
	struct list_head *tmgi_entry_ptr, *prev_tmgi_entry_ptr;
	struct list_head *clnt_ptr, *prev_clnt_ptr;

	embms_debug("====================================================\n");
	embms_debug("Printing TMGI to Client Table :\n");
	embms_debug("No of Active TMGIs : %d\n",
		    embms_conf.no_of_tmgi_sessions);
	embms_debug("====================================================\n\n");

	if (embms_conf.no_of_tmgi_sessions > 0) {
		i = 1;
		list_for_each_safe(tmgi_entry_ptr, prev_tmgi_entry_ptr,
				   &tmgi_to_clnt_map_tbl.tmgi_list_ptr) {
			temp_tmgi = list_entry(tmgi_entry_ptr,
					       struct tmgi_to_clnt_info,
					       tmgi_list_ptr);

			embms_debug("TMGI entry %d :\n", i);
			embms_debug("TMGI multicast addr : %pI4 , port %u\n\n",
				    &temp_tmgi->tmgi_multicast_addr,
				    ntohs(temp_tmgi->tmgi_port));
			embms_debug("No of clients : %d\n",
				    temp_tmgi->no_of_clients);
			j = 1;

			list_for_each_safe(clnt_ptr, prev_clnt_ptr,
					   &temp_tmgi->client_list_head) {
				temp_client = list_entry(clnt_ptr,
							 struct clnt_info,
							 client_list_ptr);
				embms_debug("Client entry %d :\n", j);
				embms_debug("client addr : %pI4 , port %u\n\n",
					    &temp_client->addr,
					    ntohs(temp_client->port));
				j++;
			}
			i++;
			embms_debug("===========================================\n\n");
		}
	} else {
		embms_debug("No TMGI entries to Display\n");
	}
	embms_debug("==================================================================\n\n");
}

/**
 * delete_tmgi_entry_from_table() - deletes tmgi from global tmgi-client table
 * @buffer:	Buffer containing TMGI info for deletion.
 *
 * This function completely removes the TMGI from
 * global TMGI-client table, along with the client list
 * so that no packets for this TMGI are processed
 *
 * Return: Success on deleting TMGI entry, error otherwise.
 */

int delete_tmgi_entry_from_table(char *buffer)
{
	struct tmgi_to_clnt_info_update *info_update;
	struct clnt_info *temp_client = NULL;
	struct tmgi_to_clnt_info *temp_tmgi = NULL;
	struct list_head *clnt_ptr, *prev_clnt_ptr;

	embms_debug("delete_tmgi_entry_from_table: Enter\n");

	info_update = (struct tmgi_to_clnt_info_update *)buffer;

	if (!info_update) {
		embms_error("delete_tmgi_entry_from_table:");
		embms_error("NULL arguments passed\n");
		return -EBADPARAM;
	}

	/* This function is used to delete a specific TMGI entry
	 * when that particular TMGI goes down
	 * Search for the TMGI entry in our local table
	 */
	if (embms_conf.no_of_tmgi_sessions == 0) {
		embms_error("TMGI count 0. Nothing to delete\n");
		return SUCCESS;
	}

	temp_tmgi = check_for_tmgi_entry(info_update->multicast_addr,
					 info_update->multicast_port);

	if (!temp_tmgi) {
		/* TMGI entry was not found in our local table*/
		embms_error("delete_client_entry_from_table :");
		embms_error("Desired TMGI entry not found\n");
		return -EBADPARAM;
	}

	spin_lock_bh(&embms_conf.lock);

	/* We need to free memory allocated to client entries
	 * for a particular TMGI entry
	 */

	list_for_each_safe(clnt_ptr, prev_clnt_ptr,
			   &temp_tmgi->client_list_head) {
		temp_client = list_entry(clnt_ptr,
					 struct clnt_info,
					 client_list_ptr);
		embms_debug("delete_tmgi_entry_from_table :");
		embms_debug("Client addr to delete :%pI4 , port %u\n",
			    &temp_client->addr, ntohs(temp_client->port));
		list_del(&temp_client->client_list_ptr);
		temp_tmgi->no_of_clients--;
		kfree(temp_client);
	}

	/* Free memory allocated to tmgi entry*/

	list_del(&temp_tmgi->tmgi_list_ptr);
	kfree(temp_tmgi);
	embms_conf.no_of_tmgi_sessions--;

	spin_unlock_bh(&embms_conf.lock);

	embms_debug("delete_tmgi_entry_from_table : TMGI Entry deleted.\n");

	return SUCCESS;
}

/**
 * delete_client_entry_from_all_tmgi() - deletes client from all tmgi lists
 * @buffer:	Buffer containing client info for deletion.
 *
 * This function completely removes a client from
 * all TMGIs in global TMGI-client table. Also delets TMGI
 * entries if no more clients are there
 *
 * Return: Success on deleting client entry, error otherwise.
 */
int delete_client_entry_from_all_tmgi(char *buffer)
{
	struct tmgi_to_clnt_info_update *info_update;
	struct clnt_info *temp_client = NULL;
	struct tmgi_to_clnt_info *tmgi = NULL;
	struct list_head *tmgi_entry_ptr, *prev_tmgi_entry_ptr;

	/* We use this function when we want to delete any
	 * client entry from all TMGI entries. This scenario
	 * happens when any client disconnects and hence
	 * we need to clean all realted client entries
	 * in our mapping table
	 */

	embms_debug("del_clnt_from_all_tmgi: Enter\n");

	info_update = (struct tmgi_to_clnt_info_update *)buffer;

	if (!info_update) {
		embms_error("del_clnt_from_all_tmgi:");
		embms_error("NULL arguments passed\n");
		return -EBADPARAM;
	}

	/* We start checking from first TMGI entry and if client
	 * entry is found in client entries of any TMGI, we clean
	 * up that client entry from that TMGI entry
	 */
	if (embms_conf.no_of_tmgi_sessions == 0)
		return SUCCESS;

	list_for_each_safe(tmgi_entry_ptr, prev_tmgi_entry_ptr,
			   &tmgi_to_clnt_map_tbl.tmgi_list_ptr) {
		tmgi = list_entry(tmgi_entry_ptr,
				  struct tmgi_to_clnt_info,
				  tmgi_list_ptr);

		temp_client = chk_clnt_entry(tmgi, info_update);
		if (!temp_client)
			continue;

		spin_lock_bh(&embms_conf.lock);

		list_del(&temp_client->client_list_ptr);
		tmgi->no_of_clients--;
		kfree(temp_client);

		spin_unlock_bh(&embms_conf.lock);

		temp_client = NULL;

		if (tmgi->no_of_clients == 0) {
			/* Deleted clnt was the only clnt for
			 * that TMGI we need to delete TMGI
			 * entry from table
			 */
			embms_debug("del_clnt_from_all_tmgi:");
			embms_debug("Deleted client was ");
			embms_debug("last client for tmgi\n");
			embms_debug("del_clnt_from_all_tmgi:");
			embms_debug("Delting tmgi as it has ");
			embms_debug("zero clients.TMGI IP ");
			embms_debug(":%pI4 , port %u\n",
				    &tmgi->tmgi_multicast_addr,
				    ntohs(tmgi->tmgi_port));

			spin_lock_bh(&embms_conf.lock);

			list_del(&tmgi->tmgi_list_ptr);
			embms_conf.no_of_tmgi_sessions--;
			kfree(tmgi);

			spin_unlock_bh(&embms_conf.lock);

			embms_debug("del_clnt_from_all_tmgi:");
			embms_debug("TMGI entry deleted\n");
		}
	}

	embms_debug("del_clnt_from_all_tmgi Successful\n");
	return SUCCESS;
}

/**
 * add_client_entry_to_table() - add client entry to specified TMGI
 * @buffer:	Buffer containing client info for addition.
 *
 * This function adds a client to the specified TMGI in
 * the global TMGI-client table. If TMGI entry is not
 * present, it adds a new TMGI entry and adds client
 * entry to it.
 *
 * Return: Success on adding client entry, error otherwise.
 */
int add_client_entry_to_table(char *buffer)
{
	int ret;
	struct tmgi_to_clnt_info_update *info_update;
	struct clnt_info *new_client = NULL;
	struct tmgi_to_clnt_info *tmgi = NULL;
	struct neighbour *neigh_entry;

	embms_debug("add_client_entry_to_table: Enter\n");

	info_update = (struct tmgi_to_clnt_info_update *)buffer;

	if (!info_update) {
		embms_error("add_client_entry_to_table:");
		embms_error("NULL arguments passed\n");
		return -EBADPARAM;
	}

	new_client = kzalloc(sizeof(*new_client), GFP_ATOMIC);
	if (!new_client) {
		embms_error("add_client_entry_to_table:");
		embms_error("Cannot allocate memory\n");
		return -ENOMEM;
	}

	new_client->addr = info_update->client_addr;
	new_client->port = info_update->client_port;

	neigh_entry = __ipv4_neigh_lookup(dev_global,
					  (u32)(new_client->addr));
	if (!neigh_entry) {
		embms_error("add_client_entry_to_table :");
		embms_error("Can't find neighbour entry\n");
		kfree(new_client);
		return -EBADPARAM;
	}

	ether_addr_copy(new_client->dmac, neigh_entry->ha);

	embms_debug("DMAC of client : %pM\n", new_client->dmac);

	embms_debug("add_client_entry_to_table:");
	embms_debug("New client addr :%pI4 , port %u\n",
		    &info_update->client_addr,
		    ntohs(info_update->client_port));

	if (embms_conf.no_of_tmgi_sessions == 0) {
		/* TMGI Client mapping table is empty.
		 * First client entry is being added
		 */

		embms_debug("tmgi_to_clnt_map_tbl is empty\n");

		ret = add_new_tmgi_entry(info_update, new_client);
		if (ret != SUCCESS) {
			kfree(new_client);
			new_client = NULL;
		}

		goto exit_add;
	}

	/* In this case, table already has some entries
	 * and we need to search for the specific tmgi entry
	 * for which client entry is to be added
	 */

	tmgi = check_for_tmgi_entry(info_update->multicast_addr,
				    info_update->multicast_port);
	if (tmgi) {
		if (chk_clnt_entry(tmgi, info_update)) {
			kfree(new_client);
			return -ENOEFFECT;
		}

		/* Adding client to the client list
		 * for the specified TMGI
		 */

		spin_lock_bh(&embms_conf.lock);

		list_add(&new_client->client_list_ptr,
			 &tmgi->client_list_head);
			tmgi->no_of_clients++;

		spin_unlock_bh(&embms_conf.lock);

		ret = SUCCESS;
	} else {
		/* TMGI specified in the message was not found in
		 * mapping table.Hence, we need to add a new entry
		 * for this TMGI and add the specified client to the client
		 * list
		 */

		embms_debug("TMGI entry not present. Adding tmgi entry\n");

		ret = add_new_tmgi_entry(info_update, new_client);
		if (ret != SUCCESS) {
			kfree(new_client);
			new_client = NULL;
		}
	}

exit_add:
	return ret;
}

/**
 * delete_client_entry_from_table() - delete client entry from specified TMGI
 * @buffer:	Buffer containing client info for deletion.
 *
 * This function deletes a client from the specified TMGI in
 * the global TMGI-client table. If this was the last client
 * entry, it also deletes the TMGI entry.
 *
 * Return: Success on deleting client entry, error otherwise.
 */
int delete_client_entry_from_table(char *buffer)
{
	struct tmgi_to_clnt_info_update *info_update;
	struct clnt_info *temp_client = NULL;
	struct tmgi_to_clnt_info *temp_tmgi = NULL;

	embms_debug("delete_client_entry_from_table: Enter\n");

	info_update = (struct tmgi_to_clnt_info_update *)buffer;

	if (!info_update) {
		embms_error("delete_client_entry_from_table:");
		embms_error("NULL arguments passed\n");
		return -EBADPARAM;
	}

	/* Search for the TMGI entry*/
	if (embms_conf.no_of_tmgi_sessions == 0)
		return SUCCESS;

	temp_tmgi = check_for_tmgi_entry(info_update->multicast_addr,
					 info_update->multicast_port);

	if (!temp_tmgi) {
		embms_error("delete_client_entry_from_table:TMGI not found\n");
		return -EBADPARAM;
	}
	/* Delete client entry for a specific tmgi*/

	embms_debug("delete_client_entry_from_table:clnt addr :%pI4,port %u\n",
		    &info_update->client_addr,
		    ntohs(info_update->client_port));

	temp_client = chk_clnt_entry(temp_tmgi, info_update);

	if (!temp_client) {
		/* Specified client entry was not found in client list
		 * of specified TMGI
		 */
		embms_error("delete_client_entry_from_table:Clnt not found\n");
		return -EBADPARAM;
	}

	spin_lock_bh(&embms_conf.lock);

	list_del(&temp_client->client_list_ptr);
	temp_tmgi->no_of_clients--;

	spin_unlock_bh(&embms_conf.lock);

	kfree(temp_client);
	temp_client = NULL;

	embms_debug("delete_client_entry_from_table:Client entry deleted\n");

	if (temp_tmgi->no_of_clients == 0) {
		/* If deleted client was the only client for that TMGI
		 * we need to delete TMGI entry from table
		 */
		embms_debug("delete_client_entry_from_table:");
		embms_debug("Deleted client was the last client for tmgi\n");
		embms_debug("delete_client_entry_from_table:");
		embms_debug("Deleting tmgi since it has zero clients\n");

		spin_lock_bh(&embms_conf.lock);

		list_del(&temp_tmgi->tmgi_list_ptr);
		embms_conf.no_of_tmgi_sessions--;
		kfree(temp_tmgi);

		spin_unlock_bh(&embms_conf.lock);

		embms_debug("delete_client_entry_from_table: TMGI deleted\n");
	}

	if (embms_conf.no_of_tmgi_sessions == 0)
		embms_conf.embms_tunneling_status = TUNNELING_OFF;

	return SUCCESS;
}

/**
 * embms_device_ioctl() - handle IOCTL calls to device
 * @file:	File descriptor of file opened from userspace process
 * @ioctl_num:	IOCTL to use
 * @ioctl_param:	IOCTL parameters/arguments
 *
 * This function is called whenever a process tries to do
 * an ioctl on our device file. As per the IOCTL number,
 * it calls various functions to manipulate global
 * TMGI-client table
 *
 * Return: Success if functoin call returns SUCCESS, error otherwise.
 */

long embms_device_ioctl(struct file *file, unsigned int ioctl_num,
			unsigned long ioctl_param)
{
	int ret;
	char buffer[BUF_LEN];
	struct in_device *iface_dev;
	struct in_ifaddr *iface_info;
	struct tmgi_to_clnt_info_update *info_update;
	char __user *argp = (char __user *)ioctl_param;

	memset(buffer, 0, BUF_LEN);

	/* Switch according to the ioctl called*/
	switch (ioctl_num) {
	case ADD_EMBMS_TUNNEL:
		if (copy_from_user(buffer, argp,
				   sizeof(struct tmgi_to_clnt_info_update)))
			return -EFAULT;

		ret = add_client_entry_to_table(buffer);
		print_tmgi_to_client_table();
		break;

	case DEL_EMBMS_TUNNEL:
		if (copy_from_user(buffer, argp,
				   sizeof(struct tmgi_to_clnt_info_update)))
			return -EFAULT;

		ret = delete_client_entry_from_table(buffer);
		print_tmgi_to_client_table();
		break;

	case TMGI_DEACTIVATE:
		if (copy_from_user(buffer, argp,
				   sizeof(struct tmgi_to_clnt_info_update)))
			return -EFAULT;

		ret = delete_tmgi_entry_from_table(buffer);
		print_tmgi_to_client_table();
		break;

	case CLIENT_DEACTIVATE:
		if (copy_from_user(buffer, argp,
				   sizeof(struct tmgi_to_clnt_info_update)))
			return -EFAULT;

		ret = delete_client_entry_from_all_tmgi(buffer);
		print_tmgi_to_client_table();
		break;

	case GET_EMBMS_TUNNELING_STATUS:
		/* This ioctl is both input (ioctl_param) and
		 * output (the return value of this function)
		 */
		embms_debug("Sending tunneling status : %d\n",
			    embms_conf.embms_tunneling_status);
		ret = embms_conf.embms_tunneling_status;
		break;

	case START_EMBMS_TUNNEL:

		if (copy_from_user(buffer, argp,
				   sizeof(struct tmgi_to_clnt_info_update)))
			return -EFAULT;

		info_update = (struct tmgi_to_clnt_info_update *)buffer;
		embms_conf.embms_data_port = info_update->data_port;
		udph_global->source = embms_conf.embms_data_port;

		memset(embms_conf.embms_iface, 0, EMBMS_MAX_IFACE_NAME);
		memcpy(embms_conf.embms_iface, info_update->iface_name,
		       EMBMS_MAX_IFACE_NAME);

		embms_conf.embms_tunneling_status = TUNNELING_ON;
		embms_debug("Starting Tunneling. Embms_data_port  = %d\n",
			    ntohs(embms_conf.embms_data_port));
		embms_debug("Embms Data Iface = %s\n", embms_conf.embms_iface);
		ret = SUCCESS;

		/*Initialise dev_global to bridge device*/
		dev_global = __dev_get_by_name(&init_net, BRIDGE_IFACE);
		if (!dev_global) {
			embms_error("Error in getting device info\n");
			ret = FAILURE;
		} else {
			iface_dev = (struct in_device *)dev_global->ip_ptr;
			iface_info = iface_dev->ifa_list;
			while (iface_info) {
				if (memcmp(iface_info->ifa_label,
					   BRIDGE_IFACE,
					   strlen(BRIDGE_IFACE)) == 0)
					break;

				iface_info = iface_info->ifa_next;
			}
			if (iface_info) {
				embms_debug("IP address of %s iface is %pI4\n",
					    BRIDGE_IFACE,
					    &iface_info->ifa_address);
				/*Populate source addr for header*/
				iph_global->saddr = iface_info->ifa_address;
				ret = SUCCESS;
			} else {
				embms_debug("Could not find iface address\n");
				ret = FAILURE;
			}
		}

		break;

	case STOP_EMBMS_TUNNEL:

		embms_conf.embms_tunneling_status = TUNNELING_OFF;
		embms_debug("Stopped Tunneling..\n");
		ret = SUCCESS;
		break;
	}

	return ret;
}

/* Module Declarations
 * This structure will hold the functions to be called
 * when a process does something to the device we
 * created. Since a pointer to this structure is kept in
 * the devices table, it can't be local to
 * init_module. NULL is for unimplemented functions.
 */
static const struct file_operations embms_device_fops = {
	.owner = THIS_MODULE,
	.open = embms_device_open,
	.release = embms_device_release,
	.read = NULL,
	.write = NULL,
	.unlocked_ioctl = embms_device_ioctl,
};

static int embms_ioctl_init(void)
{
	int ret;
	struct device *dev;

	ret = alloc_chrdev_region(&device, 0, dev_num, EMBMS_DEVICE_NAME);
	if (ret) {
		embms_error("device_alloc err\n");
		goto dev_alloc_err;
	}

	embms_class = class_create(THIS_MODULE, EMBMS_DEVICE_NAME);
	if (IS_ERR(embms_class)) {
		embms_error("class_create err\n");
		goto class_err;
	}

	dev = device_create(embms_class, NULL, device,
			    &embms_conf, EMBMS_DEVICE_NAME);
	if (IS_ERR(dev)) {
		embms_error("device_create err\n");
		goto device_err;
	}

	cdev_init(&embms_device, &embms_device_fops);
	ret = cdev_add(&embms_device, device, dev_num);
	if (ret) {
		embms_error("cdev_add err\n");
		goto cdev_add_err;
	}

	embms_debug("ioctl init OK!!\n");
	return 0;

cdev_add_err:
	device_destroy(embms_class, device);
device_err:
	class_destroy(embms_class);
class_err:
	unregister_chrdev_region(device, dev_num);
dev_alloc_err:
	return -ENODEV;
}

static void embms_ioctl_deinit(void)
{
	cdev_del(&embms_device);
	device_destroy(embms_class, device);
	class_destroy(embms_class);
	unregister_chrdev_region(device, dev_num);
}

/*Initialize the module - Register the misc device*/
static int __init start_embms(void)
{
	int ret = 0;

	iph_global = (struct iphdr *)hdr_buff;
	udph_global = (struct udphdr *)(hdr_buff + sizeof(struct iphdr));

	embms_conf.embms_tunneling_status = TUNNELING_OFF;
	embms_conf.no_of_tmgi_sessions = 0;
	embms_conf.embms_data_port = 0;
	atomic_set(&embms_conf.device_under_use, 0);
	atomic_set(&embms_conf.ip_ident, 0);
	spin_lock_init(&embms_conf.lock);

	embms_debug("Registering embms device\n");

	ret = embms_ioctl_init();
	if (ret) {
		embms_error("embms device failed to register");
		goto fail_init;
	}

	INIT_LIST_HEAD(&tmgi_to_clnt_map_tbl.tmgi_list_ptr);

	memset(hdr_buff, 0, sizeof(struct udphdr) + sizeof(struct iphdr));
	udph_global->check = UDP_CHECKSUM;
	iph_global->version = IP_VERSION;
	iph_global->ihl = IP_IHL;
	iph_global->tos = IP_TOS;
	iph_global->frag_off = IP_FRAG_OFFSET;
	iph_global->ttl = IP_TTL;
	iph_global->protocol = IPPROTO_UDP;

	dev_global = NULL;

	if (!embms_tm_multicast_recv)
		RCU_INIT_POINTER(embms_tm_multicast_recv,
				 handle_multicast_stream);

	return ret;

fail_init:
	embms_ioctl_deinit();
	return ret;
}

/*Cleanup - unregister the appropriate file from proc*/

static void __exit stop_embms(void)
{
	embms_ioctl_deinit();

	if (rcu_dereference(embms_tm_multicast_recv))
		RCU_INIT_POINTER(embms_tm_multicast_recv, NULL);

	embms_debug("unregister_chrdev done\n");
}

module_init(start_embms);
module_exit(stop_embms);
MODULE_LICENSE("GPL v2");
