/* netfilter.c: look after the filters for various protocols.
 * Heavily influenced by the old firewall.c by David Bonn and Alan Cox.
 *
 * Thanks to Rob `CmdrTaco' Malda for not influencing this code in any
 * way.
 *
 * Rusty Russell (C)2000 -- This code is GPL.
 * Patrick McHardy (c) 2006-2012
 */
#include <linux/kernel.h>
#include <linux/netfilter.h>
#include <net/protocol.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <net/net_namespace.h>
#include <net/sock.h>

#include "nf_internals.h"

#ifdef CONFIG_MTK_NET_LOGGING
#include <linux/timer.h>
#include <linux/string.h>
#include <net/netfilter/nf_log.h>
#endif

static DEFINE_MUTEX(afinfo_mutex);

#ifdef CONFIG_MTK_NET_LOGGING
#define IPTABLES_DROP_PACKET_STATICS 10

/*Trace 500 packets*/
#define IPTABLES_DROP_PACKET_NUM  500
#define S_SIZE (1024 - (sizeof(unsigned int) + 1))

struct drop_packet_info_t {
	unsigned int packet_num;
	unsigned int len;
	u_int8_t pf;
	unsigned int hook;
	long drop_time;
	char *iface;
	char *table;
	int family;
	int already_print;
};

struct drop_packets_statics_t {
	long cnt;
	struct timer_list print_drop_packets_timer;
	unsigned long print_stamp;
	unsigned int print_len;
	struct drop_packet_info_t drop_packets[IPTABLES_DROP_PACKET_NUM];
};

struct drop_packets_statics_t iptables_drop_packets;
struct sbuff {
	unsigned int    count;
	char        buf[S_SIZE + 1];
};

static __printf(2, 3) int sb_add(struct sbuff *m, const char *f, ...)
{
	va_list args;
	int len;

	if (likely(m->count < S_SIZE)) {
			va_start(args, f);
			len = vsnprintf(m->buf + m->count, S_SIZE - m->count, f, args);
			va_end(args);
			if (likely(m->count + len < S_SIZE)) {
									m->count += len;
									return 0;
									}
		}
	m->count = S_SIZE;
	return -1;
}

static void iptables_drop_packet_monitor(unsigned long data)
{
	int limit = 0;
	int i = 0;
	int size = 0;
	int num  = iptables_drop_packets.cnt % 500;
	struct sbuff m;

	m.count = 0;
	memset(m.buf, 0, sizeof(m.buf));
	if (num > 10)
			limit = 10;
	else
			limit = num;
	if ((iptables_drop_packets.print_len + limit) > num)
			size = num;
	else
		size = iptables_drop_packets.print_len + limit;
	for (i = iptables_drop_packets.print_len; i < size; i++) {
		if (iptables_drop_packets.drop_packets[i].already_print == 0) {
			sb_add(&m, "[mtk_net][drop](%ld,%d),",
			       iptables_drop_packets.cnt,
			       iptables_drop_packets.drop_packets[i].packet_num);
			switch (iptables_drop_packets.drop_packets[i].pf) {
			case NFPROTO_IPV4:
				sb_add(&m, "[IPV4],");
				break;
			case NFPROTO_IPV6:
				sb_add(&m, "[IPV6],");
				break;
			case NFPROTO_ARP:
				sb_add(&m, "[ARP],");
				break;
			default:
				sb_add(&m, "[UNSPEC],");
				break;
			}
			switch (iptables_drop_packets.drop_packets[i].hook) {
			case NF_INET_PRE_ROUTING:
				sb_add(&m, "[PRE_ROUTING],");
				break;
			case NF_INET_LOCAL_IN:
				sb_add(&m, "[LOCAL_IN],");
				break;
			case NF_INET_FORWARD:
				sb_add(&m, "[FORWARD],");
				break;
			case NF_INET_LOCAL_OUT:
				sb_add(&m, "[LOCAL_OUT],");
				break;
			case NF_INET_POST_ROUTING:
				sb_add(&m, "[POST_ROUTING],");
				break;
			default:
				sb_add(&m, "[Unspec chain],");
				break;
			}

			sb_add(&m, "[%pS],[%s],[%ld]", iptables_drop_packets.drop_packets[i].table,
			       iptables_drop_packets.drop_packets[i].iface,
			       iptables_drop_packets.drop_packets[i].drop_time);
			pr_info("%s\n", m.buf);
			m.count = 0;
			memset(m.buf, 0, sizeof(m.buf));
			iptables_drop_packets.drop_packets[i].already_print = 1;
		}
	}
iptables_drop_packets.print_len = (iptables_drop_packets.print_len + limit)%500;
}
#endif

const struct nf_afinfo __rcu *nf_afinfo[NFPROTO_NUMPROTO] __read_mostly;
EXPORT_SYMBOL(nf_afinfo);
const struct nf_ipv6_ops __rcu *nf_ipv6_ops __read_mostly;
EXPORT_SYMBOL_GPL(nf_ipv6_ops);

int nf_register_afinfo(const struct nf_afinfo *afinfo)
{
	mutex_lock(&afinfo_mutex);
	RCU_INIT_POINTER(nf_afinfo[afinfo->family], afinfo);
	mutex_unlock(&afinfo_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(nf_register_afinfo);

void nf_unregister_afinfo(const struct nf_afinfo *afinfo)
{
	mutex_lock(&afinfo_mutex);
	RCU_INIT_POINTER(nf_afinfo[afinfo->family], NULL);
	mutex_unlock(&afinfo_mutex);
	synchronize_rcu();
}
EXPORT_SYMBOL_GPL(nf_unregister_afinfo);

struct list_head nf_hooks[NFPROTO_NUMPROTO][NF_MAX_HOOKS] __read_mostly;
EXPORT_SYMBOL(nf_hooks);

#ifdef HAVE_JUMP_LABEL
struct static_key nf_hooks_needed[NFPROTO_NUMPROTO][NF_MAX_HOOKS];
EXPORT_SYMBOL(nf_hooks_needed);
#endif

static DEFINE_MUTEX(nf_hook_mutex);

int nf_register_hook(struct nf_hook_ops *reg)
{
	struct nf_hook_ops *elem;

	mutex_lock(&nf_hook_mutex);
	list_for_each_entry(elem, &nf_hooks[reg->pf][reg->hooknum], list) {
		if (reg->priority < elem->priority)
			break;
	}
	list_add_rcu(&reg->list, elem->list.prev);
	mutex_unlock(&nf_hook_mutex);
#ifdef HAVE_JUMP_LABEL
	static_key_slow_inc(&nf_hooks_needed[reg->pf][reg->hooknum]);
#endif
	return 0;
}
EXPORT_SYMBOL(nf_register_hook);

void nf_unregister_hook(struct nf_hook_ops *reg)
{
	mutex_lock(&nf_hook_mutex);
	list_del_rcu(&reg->list);
	mutex_unlock(&nf_hook_mutex);
#ifdef HAVE_JUMP_LABEL
	static_key_slow_dec(&nf_hooks_needed[reg->pf][reg->hooknum]);
#endif
	synchronize_net();
	nf_queue_nf_hook_drop(reg);
}
EXPORT_SYMBOL(nf_unregister_hook);

int nf_register_hooks(struct nf_hook_ops *reg, unsigned int n)
{
	unsigned int i;
	int err = 0;

	for (i = 0; i < n; i++) {
		err = nf_register_hook(&reg[i]);
		if (err)
			goto err;
	}
	return err;

err:
	if (i > 0)
		nf_unregister_hooks(reg, i);
	return err;
}
EXPORT_SYMBOL(nf_register_hooks);

void nf_unregister_hooks(struct nf_hook_ops *reg, unsigned int n)
{
	while (n-- > 0)
		nf_unregister_hook(&reg[n]);
}
EXPORT_SYMBOL(nf_unregister_hooks);

unsigned int nf_iterate(struct list_head *head,
			struct sk_buff *skb,
			unsigned int hook,
			const struct net_device *indev,
			const struct net_device *outdev,
			struct nf_hook_ops **elemp,
			int (*okfn)(struct sk_buff *),
			int hook_thresh)
{
	unsigned int verdict;

	/*
	 * The caller must not block between calls to this
	 * function because of risk of continuing from deleted element.
	 */
	list_for_each_entry_continue_rcu((*elemp), head, list) {
		if (hook_thresh > (*elemp)->priority)
			continue;

		/* Optimization: we don't need to hold module
		   reference here, since function can't sleep. --RR */
repeat:
		verdict = (*elemp)->hook(*elemp, skb, indev, outdev, okfn);
		if (verdict != NF_ACCEPT) {
#ifdef CONFIG_NETFILTER_DEBUG
			if (unlikely((verdict & NF_VERDICT_MASK)
							> NF_MAX_VERDICT)) {
				NFDEBUG("Evil return from %p(%u).\n",
					(*elemp)->hook, hook);
				continue;
			}
#endif
			if (verdict != NF_REPEAT)
				return verdict;
			goto repeat;
		}
	}
	return NF_ACCEPT;
}


/* Returns 1 if okfn() needs to be executed by the caller,
 * -EPERM for NF_DROP, 0 otherwise. */
int nf_hook_slow(u_int8_t pf, unsigned int hook, struct sk_buff *skb,
		 struct net_device *indev,
		 struct net_device *outdev,
		 int (*okfn)(struct sk_buff *),
		 int hook_thresh)
{
	struct nf_hook_ops *elem;
	unsigned int verdict;
	int ret = 0;
#ifdef CONFIG_MTK_NET_LOGGING
	char *table = NULL;
	unsigned int num;
#endif
	/* We may already have this, but read-locks nest anyway */
	rcu_read_lock();

	elem = list_entry_rcu(&nf_hooks[pf][hook], struct nf_hook_ops, list);
next_hook:
	verdict = nf_iterate(&nf_hooks[pf][hook], skb, hook, indev,
			     outdev, &elem, okfn, hook_thresh);
	if (verdict == NF_ACCEPT || verdict == NF_STOP) {
		ret = 1;
	} else if ((verdict & NF_VERDICT_MASK) == NF_DROP) {
#ifdef CONFIG_MTK_NET_LOGGING
		/*because skb free  need copy some info to ...*/
	iptables_drop_packets.cnt++;
	if (iptables_drop_packets.cnt > 100000)
			iptables_drop_packets.cnt = 1;
	table = (char *)((struct nf_hook_ops *)elem)->hook;
	num = iptables_drop_packets.cnt % 500;
	iptables_drop_packets.drop_packets[num].drop_time = jiffies;
	iptables_drop_packets.drop_packets[num].len = skb->len;
	iptables_drop_packets.drop_packets[num].hook = hook;
	iptables_drop_packets.drop_packets[num].pf = pf;
	iptables_drop_packets.drop_packets[num].iface = skb->dev->name;
	iptables_drop_packets.drop_packets[num].table = table;
	iptables_drop_packets.drop_packets[num].packet_num = num + 1;
	iptables_drop_packets.drop_packets[num].already_print = 0;
	iptables_drop_packets.cnt++;
	if ((jiffies - iptables_drop_packets.print_stamp)/HZ > IPTABLES_DROP_PACKET_STATICS) {
		iptables_drop_packets.print_stamp = jiffies;
		mod_timer(&iptables_drop_packets.print_drop_packets_timer, jiffies);
	}
#endif

		kfree_skb(skb);
		ret = NF_DROP_GETERR(verdict);
		if (ret == 0)
			ret = -EPERM;
	} else if ((verdict & NF_VERDICT_MASK) == NF_QUEUE) {
		int err = nf_queue(skb, elem, pf, hook, indev, outdev, okfn,
						verdict >> NF_VERDICT_QBITS);
		if (err < 0) {
			if (err == -ECANCELED)
				goto next_hook;
			if (err == -ESRCH &&
			   (verdict & NF_VERDICT_FLAG_QUEUE_BYPASS))
				goto next_hook;
			kfree_skb(skb);
		}
	}
	rcu_read_unlock();
	return ret;
}
EXPORT_SYMBOL(nf_hook_slow);


int skb_make_writable(struct sk_buff *skb, unsigned int writable_len)
{
	if (writable_len > skb->len)
		return 0;

	/* Not exclusive use of packet?  Must copy. */
	if (!skb_cloned(skb)) {
		if (writable_len <= skb_headlen(skb))
			return 1;
	} else if (skb_clone_writable(skb, writable_len))
		return 1;

	if (writable_len <= skb_headlen(skb))
		writable_len = 0;
	else
		writable_len -= skb_headlen(skb);

	return !!__pskb_pull_tail(skb, writable_len);
}
EXPORT_SYMBOL(skb_make_writable);

#if IS_ENABLED(CONFIG_NF_CONNTRACK)
/* This does not belong here, but locally generated errors need it if connection
   tracking in use: without this, connection may not be in hash table, and hence
   manufactured ICMP or RST packets will not be associated with it. */
void (*ip_ct_attach)(struct sk_buff *, const struct sk_buff *)
		__rcu __read_mostly;
EXPORT_SYMBOL(ip_ct_attach);

void nf_ct_attach(struct sk_buff *new, const struct sk_buff *skb)
{
	void (*attach)(struct sk_buff *, const struct sk_buff *);

	if (skb->nfct) {
		rcu_read_lock();
		attach = rcu_dereference(ip_ct_attach);
		if (attach)
			attach(new, skb);
		rcu_read_unlock();
	}
}
EXPORT_SYMBOL(nf_ct_attach);

void (*nf_ct_destroy)(struct nf_conntrack *) __rcu __read_mostly;
EXPORT_SYMBOL(nf_ct_destroy);

void nf_conntrack_destroy(struct nf_conntrack *nfct)
{
	void (*destroy)(struct nf_conntrack *);

	rcu_read_lock();
	destroy = rcu_dereference(nf_ct_destroy);
	BUG_ON(destroy == NULL);
	destroy(nfct);
	rcu_read_unlock();
}
EXPORT_SYMBOL(nf_conntrack_destroy);

struct nfq_ct_hook __rcu *nfq_ct_hook __read_mostly;
EXPORT_SYMBOL_GPL(nfq_ct_hook);

struct nfq_ct_nat_hook __rcu *nfq_ct_nat_hook __read_mostly;
EXPORT_SYMBOL_GPL(nfq_ct_nat_hook);

#endif /* CONFIG_NF_CONNTRACK */

#ifdef CONFIG_NF_NAT_NEEDED
void (*nf_nat_decode_session_hook)(struct sk_buff *, struct flowi *);
EXPORT_SYMBOL(nf_nat_decode_session_hook);
#endif

static int __net_init netfilter_net_init(struct net *net)
{
#ifdef CONFIG_PROC_FS
	net->nf.proc_netfilter = proc_net_mkdir(net, "netfilter",
						net->proc_net);
	if (!net->nf.proc_netfilter) {
		if (!net_eq(net, &init_net))
			pr_err("cannot create netfilter proc entry");

		return -ENOMEM;
	}
#endif
#ifdef CONFIG_MTK_NET_LOGGING
	init_timer(&iptables_drop_packets.print_drop_packets_timer);
	iptables_drop_packets.print_drop_packets_timer.function = iptables_drop_packet_monitor;
#endif
	return 0;
}

static void __net_exit netfilter_net_exit(struct net *net)
{
	remove_proc_entry("netfilter", net->proc_net);
}

static struct pernet_operations netfilter_net_ops = {
	.init = netfilter_net_init,
	.exit = netfilter_net_exit,
};

int __init netfilter_init(void)
{
	int i, h, ret;

	for (i = 0; i < ARRAY_SIZE(nf_hooks); i++) {
		for (h = 0; h < NF_MAX_HOOKS; h++)
			INIT_LIST_HEAD(&nf_hooks[i][h]);
	}

	ret = register_pernet_subsys(&netfilter_net_ops);
	if (ret < 0)
		goto err;

	ret = netfilter_log_init();
	if (ret < 0)
		goto err_pernet;

	return 0;
err_pernet:
	unregister_pernet_subsys(&netfilter_net_ops);
err:
	return ret;
}
