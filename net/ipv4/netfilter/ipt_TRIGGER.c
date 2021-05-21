// SPDX-License-Identifier: GPL-2.0-only

#ifndef _IPT_PORTTRIGGER_target
#define _IPT_PORTTRIGGER_target
/* net/ipv4/netfilter/ipt_TRIGGER.c
 *	Porttrigger kernel side implementation.
 *
 * (C) Copyright 2011, Ubicom, Inc.
 *
 * This file is part of the Ubicom32 Linux Kernel Port.
 *
 * The Ubicom32 Linux Kernel Port is free software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Ubicom32 Linux Kernel Port is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Ubicom32 Linux Kernel Port.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Ubicom32 implementation derived from Cameo's implementation(with many thanks):
 *
 */
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/module.h>
#include <net/protocol.h>
#include <net/checksum.h>
#include <net/ip.h>
#include <linux/tcp.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_nat.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ipt_TRIGGER.h>
#include <linux/atomic.h>

#define PORTTRIGGER_DEBUG

#if !defined(PORTTRIGGER_DEBUG)
#define DEBUGP(type, args...)
#else
static const char * const modes[] = {"MODE_TRIGGER_DNAT", "MODE_TRIGGER_FORWARD_IN",
				"MODE_TRIGGER_FORWARD_OUT"};
#define DEBUGP(args...) pr_debug(args)
#endif

/* TODO: Add magic value checks to data structure.
 */
struct ipt_porttrigger {
	struct list_head list;
	struct timer_list timeout;
	unsigned int src_ip;
	unsigned int dst_ip;
	unsigned short trigger_proto;	/* Protocol: TCP or UDP */
	unsigned short forward_proto;	/* Protocol: TCP or UDP */
	struct ipt_mport trigger_ports;
	struct ipt_mport forward_ports;
};

/* TODO: It might be better to use a hash table for performance in
 * heavy traffic.
 */
static LIST_HEAD(trigger_list);
static DEFINE_SPINLOCK(porttrigger_lock);

/* porttrigger_pte_debug_print()
 */
static void porttrigger_pte_debug_print(const struct ipt_porttrigger *pte, const char *s)
{
#if defined(PORTTRIGGER_DEBUG)
	DEBUGP("%p: %s - trigger_proto[%d], forward_proto[%d], src_ip[%pI4], dst_ip[%pI4]\n",
	       pte, s,
	       pte->trigger_proto, pte->forward_proto,
	       &pte->src_ip, &pte->dst_ip);
#endif
}

/* porttrigger_free()
 *	Free the object.
 */
static void porttrigger_free(struct ipt_porttrigger *pte)
{
	porttrigger_pte_debug_print(pte, "free");
	kfree(pte);
}

/* trigger_refresh_timer()
 *	Refresh the timer for this object.
 */
static bool trigger_refresh_timer(struct ipt_porttrigger *pte, unsigned long extra_jiffies)
{
	lockdep_assert_held(&porttrigger_lock);

	if (extra_jiffies == 0)
		extra_jiffies = TRIGGER_TIMEOUT * HZ;
	if (del_timer(&pte->timeout)) {
		pte->timeout.expires = jiffies + extra_jiffies;
		add_timer(&pte->timeout);
		return true;
	}
	return false;
}

/* porttrigger_timer_timeout()
 *	The timer has gone off, self-destruct.
 */
static void porttrigger_timer_timeout(struct timer_list *t)
{
	struct ipt_porttrigger *pte = from_timer(pte, t, timeout);

	/* The race with list deletion is solved by ensuring
	 * that either this code or the list deletion code
	 * but not both will remove the oject.
	 */
	spin_lock_bh(&porttrigger_lock);
	porttrigger_pte_debug_print(pte, "timeout");
	list_del(&pte->list);
	spin_unlock_bh(&porttrigger_lock);
	porttrigger_free(pte);
}

/* porttrigger_ports_match()
 *	Common check port match function for eggress and ingress packets.
 */
static inline bool porttrigger_ports_match(const struct ipt_mport *minfo, u_int16_t port)
{
	unsigned int i, m;
	unsigned short pflags = minfo->pflags;

	for (i = 0, m = 1; i < IPT_MULTI_PORTS; i++, m <<= 1) {
		unsigned short s, e;
		/* Short list termination check is indicated by a range entry (pflags set)
		 * and a start value of the last valid port value (which clearly cannot be a range).
		 * Abort checking the rest of the list.
		 */
		if ((pflags & m)  && minfo->ports[i] == 65535)
			break;

		/* Get the start and end ports from the array.
		 */
		s = minfo->ports[i];
		e = minfo->ports[i];
		DEBUGP("Start port: %d End port: %d\n", s, e);
		if (unlikely(pflags & m)) {
			/* If the pflags is set, this means the next element in the array is the
			 * end port of the range instead of a start.
			 */
			e = minfo->ports[++i];

			/* Shift the mask to left for the next pflags check
			 */
			m <<= 1;
		}

		if ((htons(port) >= s) && (htons(port) <= e))
			return true;
	}
	DEBUGP("ports = %d don't match\n", htons(port));
	return false;
}

/* porttrigger_packet_in_match()
 *	Ingress packet check match entry function.
 */
static inline bool porttrigger_packet_in_match(const struct ipt_porttrigger *pte,
					       const unsigned short proto,
					       const unsigned short dport,
					       const unsigned int src_ip)
{
	DEBUGP("src_ip = %pI4, pte->src_ip = %pI4 proto = %d, dport = %d in match\n",
	       &src_ip, &pte->src_ip, proto, htons(dport));

	/* Check LAND attack. If the ingress packet's src ip is equal to the pte's src ip
	 * (which is the LAN PC's ip), ignore the packet.
	 */
	if (src_ip == pte->src_ip) {
		DEBUGP("LAND attack: ingress packet's src ip is equal to LAN src ip\n");
		return false;
	}

	/* The protocol values can be TCP, UDP or both (0). Check whether we have a
	 * valid protocol or not.
	 */
	if (pte->forward_proto && pte->forward_proto != proto) {
		DEBUGP("Invalid forward_proto: %d\n", pte->forward_proto);
		return false;
	}

	if (!porttrigger_ports_match(&pte->forward_ports, dport))
		return false;

	DEBUGP("pte->forward_proto = %d, dport: %d\n",  pte->forward_proto, htons(dport));
	return true;
}

/* porttrigger_packet_out_match()
 *	Egress packet check match entry function.
 */
static inline bool porttrigger_packet_out_match(const unsigned short trigger_proto,
						const unsigned short protocol,
						const struct ipt_mport *trigger_ports,
						unsigned short dport)
{
	DEBUGP("protocol = %d, dport = %d out match\n", protocol, htons(dport));

	if (trigger_proto && trigger_proto != protocol) {
		DEBUGP("Invalid trigger_proto: %d\n", trigger_proto);
		return false;
	}

	if (!porttrigger_ports_match(trigger_ports, dport))
		return false;

	DEBUGP("trigger_proto = %d, dport: %d\n", trigger_proto, htons(dport));

	return true;
}

/* porttrigger_nat()
 *	Ingress packet on PRE_ROUTING hook, find match, update conntrack to allow
 */
static unsigned int porttrigger_nat(struct sk_buff *skb, const struct xt_action_param *par)
{
	struct ipt_porttrigger *pte;
	unsigned short dst_port = 0;
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct;
	enum ip_conntrack_dir dir;
	unsigned int src_addr;
	unsigned short protocol;

	DEBUGP("%s", __func__);
	if (xt_hooknum(par) != NF_INET_PRE_ROUTING)
		return XT_CONTINUE;

	ct = nf_ct_get(skb, &ctinfo);
	if (!ct) {
		DEBUGP("ingress packet conntrack not found\n");
		return XT_CONTINUE;
	}

	dir = CTINFO2DIR(ctinfo);
	src_addr = ct->tuplehash[dir].tuple.src.u3.ip;
	dst_port = ct->tuplehash[dir].tuple.dst.u.all;
	protocol = ct->tuplehash[dir].tuple.dst.protonum;
	DEBUGP("%s: src_addr: %pI4 dst_port = %d protocol = %d\n",
	       __func__, &src_addr, htons(dst_port), protocol);

	spin_lock_bh(&porttrigger_lock);
	list_for_each_entry(pte, &trigger_list, list) {
		struct nf_nat_range2 newrange;

		if (!porttrigger_packet_in_match(pte, protocol, dst_port, ntohl(src_addr)))
			continue;

		newrange.flags = NF_NAT_RANGE_MAP_IPS;
		newrange.min_addr.ip = pte->src_ip;
		newrange.max_addr.ip = pte->src_ip;

		spin_unlock_bh(&porttrigger_lock);

		DEBUGP("DNAT: src_addr: %pI4 dst_port: %d protocol: %d\n",
		       &src_addr, htons(dst_port), protocol);
		DEBUGP("DNAT: LAN src IP %pI4\n", &pte->src_ip);
		return nf_nat_setup_info(ct, &newrange, NF_NAT_MANIP_DST);
	}
	spin_unlock_bh(&porttrigger_lock);
	return XT_CONTINUE;
}

/* porttrigger_forward()
 *	Ingress and Egress packet forwarding hook
 */
static unsigned int porttrigger_forward(struct sk_buff *skb, const struct xt_action_param *par)
{
	const struct ipt_porttrigger_info *info = par->targinfo;
	struct ipt_porttrigger *pte;
	struct ipt_porttrigger *pte2;
	unsigned short dst_port = 0, dst_tcp_port = 0, dst_udp_port = 0;
	unsigned short dst_dccp_port = 0, dst_sctp_port = 0;
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct;
	enum ip_conntrack_dir dir;
	unsigned int src_addr;
	unsigned short protocol;

	if (xt_hooknum(par) != NF_INET_POST_ROUTING)
		return XT_CONTINUE;

	ct = nf_ct_get(skb, &ctinfo);
	if (!ct) {
		DEBUGP("%s: conntrack not found", __func__);
		return XT_CONTINUE;
	}

	dir = CTINFO2DIR(ctinfo);
	src_addr = ct->tuplehash[dir].tuple.src.u3.ip;
	dst_port = ct->tuplehash[dir].tuple.dst.u.all;
	protocol = ct->tuplehash[dir].tuple.dst.protonum;
	DEBUGP("%s: src_addr: %pI4 dst_port = %d protocol = %d\n", __func__,
	       &src_addr, dst_port, protocol);

	/* Ingress packet, refresh the timer if we find an entry.
	 */
	if (info->mode == MODE_TRIGGER_FORWARD_IN) {
		DEBUGP("porttrigger_forward_in\n");
		spin_lock_bh(&porttrigger_lock);
		list_for_each_entry(pte, &trigger_list, list) {
			/* Compare the ingress packet with the existing
			 * entries looking for a match.
			 */
			if (!porttrigger_packet_in_match(pte, protocol, dst_port, ntohl(src_addr)))
				continue;

			/* Refresh the timer, if we fail, break
			 * out and forward fail as though we never
			 * found the entry.
			 */
			if (!trigger_refresh_timer(pte, info->timer * HZ))
				break;

			/* The entry is found and refreshed, but the entry values
			 * can be changed by another thread in the MODE_TRIGGER_FORWARD_OUT case,
			 * so print them inside the lock.
			 */
			porttrigger_pte_debug_print(pte, "refresh");
			spin_unlock_bh(&porttrigger_lock);
			DEBUGP("FORWARD_IN_ACCEPT: src_addr: %pI4 dst_port: %d protocol: %d\n",
			       &src_addr, dst_port, protocol);
			return NF_ACCEPT;
		}
		spin_unlock_bh(&porttrigger_lock);
		DEBUGP("FORWARD_IN_FAIL\n");
		return XT_CONTINUE;
	}
	/* Egress packet, create a new rule in our list.  If we don't have a rule for this port
	 * do not create any rule.
	 */
	DEBUGP("porttrigger_forward_out: Check for the match\n");
	if (!porttrigger_packet_out_match(info->trigger_proto, protocol,
					  &info->trigger_ports, dst_port))
		return XT_CONTINUE;

	/* Allocate a new entry
	 */
	DEBUGP("Create a new entry\n");
	pte = kzalloc(sizeof(*pte), GFP_ATOMIC | __GFP_NOWARN);
	if (!pte) {
		DEBUGP("kernel malloc fail\n");
		return XT_CONTINUE;
	}

	INIT_LIST_HEAD(&pte->list);

	//pte->src_ip = ntohl(src_addr);
	pte->src_ip = src_addr;
	pte->trigger_proto = protocol;
	pte->forward_proto = info->forward_proto;
	memcpy(&pte->trigger_ports, &info->trigger_ports, sizeof(struct ipt_mport));
	memcpy(&pte->forward_ports, &info->forward_ports, sizeof(struct ipt_mport));
	DEBUGP("porttrigger_forward_out new entry: src_addr: %pI4 trigger_proto = %d forward_proto = %d \n",
	       &pte->src_ip, pte->trigger_proto, pte->forward_proto);

	/* We have created the new pte; however, it might not be unique.
	 * Search the list for a matching entry.  If found, throw away
	 * the new entry and refresh the old.  If not found, atomically
	 * insert the new entry on the list.
	 */
	spin_lock_bh(&porttrigger_lock);
	list_for_each_entry(pte2, &trigger_list, list) {
		if (!porttrigger_packet_out_match(pte2->trigger_proto, protocol,
						  &pte2->trigger_ports, dst_port))
			continue;

		/* If we can not refresh this entry, insert our new
		 * entry as this one is timed out and will be removed
		 * from the list shortly.
		 */
		if (!trigger_refresh_timer(pte2, info->timer * HZ))
			break;

		/* We have an entry in the list with this dst_port (trigger port) and protocol,
		 * but the src ip may be different than this new one,
		 * so refresh the src_ip of the found pte.
		 */
		//pte2->src_ip = ntohl(src_addr);
		pte2->src_ip = src_addr;

		/* It is not good to use debug message inside the lock, but we are changing the
		 * value, and we have to print it here by paying the price under debugs on.
		 */
		porttrigger_pte_debug_print(pte2, "refresh");

		/* Found and refreshed an existing entry.  Its values
		 * do not change so print the values outside of the lock.
		 *
		 * Free up the new entry.
		 */
		spin_unlock_bh(&porttrigger_lock);
		porttrigger_free(pte);
		return NF_ACCEPT;
	}

	/* Initialize the self-destruct timer.
	 */
	DEBUGP("Init timer");
	timer_setup(&pte->timeout, porttrigger_timer_timeout, 0);

	/* Add the new entry to the list.
	 */
	pte->timeout.expires = jiffies + (TRIGGER_TIMEOUT * HZ);
	add_timer(&pte->timeout);
	list_add(&pte->list, &trigger_list);
	porttrigger_pte_debug_print(pte, "ADD");
	spin_unlock_bh(&porttrigger_lock);
	return NF_ACCEPT;
}

/* porttrigger_target()
 *	One of the iptables hooks has a packet for us to analyze, do so.
 */
static unsigned int porttrigger_target(struct sk_buff *skb, const struct xt_action_param *par)
{
	const struct ipt_porttrigger_info *info = par->targinfo;
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct;
	enum ip_conntrack_dir dir;
	unsigned int src_addr;
	unsigned int dst_addr;
	unsigned short protocol;

	/* Check if we have enough data in the skb.
	 */
	if (skb->len < ip_hdrlen(skb)) {
		DEBUGP("skb is too short for IP header\n");
		return XT_CONTINUE;
	}

	ct = nf_ct_get(skb, &ctinfo);
	if (!ct) {
		DEBUGP("%s: conntrack not found\n", __func__);
		return XT_CONTINUE;
	}

	dir = CTINFO2DIR(ctinfo);
	src_addr = ct->tuplehash[dir].tuple.src.u3.ip;
	dst_addr = ct->tuplehash[dir].tuple.dst.u3.ip;
	protocol = ct->tuplehash[dir].tuple.dst.protonum;

	DEBUGP("%s: src_addr = %pI4 dst_addr = %pI4 protocol = %d\n",
	       __func__, &src_addr, &dst_addr, protocol);

	/* We can not perform porttriggering on anything but UDP and TCP.
	 */
	if (protocol != IPPROTO_TCP && protocol != IPPROTO_UDP)
		return XT_CONTINUE;

	/* Check for LAND attack and ignore.
	 */
	if (dst_addr == src_addr)
		return XT_CONTINUE;

	/* Check that we have valid source and destination addresses.
	 */
	if (dst_addr == (__be32)0 || src_addr == (__be32)0)
		return XT_CONTINUE;

	DEBUGP("info->mode is %d\n", info->mode);
	DEBUGP("%s: mode = %s\n", __func__, modes[info->mode]);

	/* TODO: why have mode at all since par->hooknum provides this information?
	 */
	switch (info->mode) {
	case MODE_TRIGGER_DNAT:
		return porttrigger_nat(skb, par);
	case MODE_TRIGGER_FORWARD_OUT:
	case MODE_TRIGGER_FORWARD_IN:
		return porttrigger_forward(skb, par);
	}
	return XT_CONTINUE;
}

/* porttrigger_check()
 *	check info set by iptables
 */
static int porttrigger_check(const struct xt_tgchk_param *par)
{
	const struct ipt_porttrigger_info *info = par->targinfo;
	struct list_head *cur, *tmp;

	if (info->mode == MODE_TRIGGER_DNAT && (strcmp(par->table, "nat") != 0)) {
		DEBUGP("%s: bad table '%s'.\n", __func__, par->table);
		return -EINVAL;
	}

	if (par->hook_mask & ~((1 << NF_INET_PRE_ROUTING) | (1 << NF_INET_POST_ROUTING))) {
		DEBUGP("%s: bad hooks %x.\n", __func__, par->hook_mask);
		return -EINVAL;
	}

	if (info->forward_proto != IPPROTO_TCP && info->forward_proto != IPPROTO_UDP &&
	    info->forward_proto != 0) {
		DEBUGP("%s: bad trigger proto '%d'.\n", __func__, info->forward_proto);
		return -EINVAL;
	}

	/* If the last entry in the port list is a start and it expects an end port entry,
	 * porttrigger_ports_match() function will derefence beyond the end of the array.
	 * Make sure we are not handed a bad port list.
	 */
	if ((info->forward_ports.pflags & (1 << IPT_MULTI_PORTS)) &&
	    info->forward_ports.ports[IPT_MULTI_PORTS - 1] != 65535) {
		DEBUGP("%s: bad forward_ports list\n", __func__);
		return -EINVAL;
	}

	if ((info->trigger_ports.pflags & (1 << IPT_MULTI_PORTS)) &&
	    info->trigger_ports.ports[IPT_MULTI_PORTS - 1] != 65535) {
		DEBUGP("%s: bad trigger_ports list\n", __func__);
		return -EINVAL;
	}

	/* Remove all entries from the trigger list.
	 */
drain:
	spin_lock_bh(&porttrigger_lock);
	list_for_each_safe(cur, tmp, &trigger_list) {
		struct ipt_porttrigger *pte = (void *)cur;

		/* If the timeout is in process, it will tear
		 * us down.  Since it is waiting on the spinlock
		 * we have to give up the spinlock to give the
		 * timeout on another CPU a chance to run.
		 */
		if (!del_timer(&pte->timeout)) {
			spin_unlock_bh(&porttrigger_lock);
			goto drain;
		}

		DEBUGP("%p: removing from list\n", pte);
		list_del(&pte->list);
		spin_unlock_bh(&porttrigger_lock);
		porttrigger_free(pte);
		goto drain;
	}
	spin_unlock_bh(&porttrigger_lock);
	return 0;
}

static struct xt_target trigger = {
	.name		= "TRIGGER",
	.family		= NFPROTO_IPV4,
	.target		= porttrigger_target,
	.checkentry	= porttrigger_check,
	.targetsize	= sizeof(struct ipt_porttrigger_info),
	.hooks		= ((1 << NF_INET_PRE_ROUTING) | (1 << NF_INET_POST_ROUTING)),
	.me		= THIS_MODULE,
};

static int __init init(void)
{
	return xt_register_target(&trigger);
}

static void __exit fini(void)
{
	xt_unregister_target(&trigger);
}

module_init(init);
module_exit(fini);

MODULE_LICENSE("GPL v2");

#endif /* _IPT_PORTTRIGGER_target */

