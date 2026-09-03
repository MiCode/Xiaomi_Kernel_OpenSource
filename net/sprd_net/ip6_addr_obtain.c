// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2016 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <uapi/linux/in6.h>
#include <linux/string.h>
#include <net/ipv6.h>
#include <net/ndisc.h>
#include <net/addrconf.h>
#include <linux/proc_fs.h>
#include <linux/netfilter_ipv6.h>

#define IP_SFT(ip, x) (((ip) >> (x)) & 0xFF)
#define MAX_IFACE_NUM 14

struct v6_iface_stat {
	u64 v6_iface_id_fail;
};

struct ipv6_iface_id {
	bool initialized;
	struct in6_addr v6_iface_id;
};

static struct ipv6_iface_id gl_ipv6_iface_id[MAX_IFACE_NUM];

static const char * const iface_name[] = {"seth_lte", "sipa_eth", NULL};

static ssize_t sn_v6_iface_id_write(struct file *filp,
				    const char __user *buffer,
				    size_t count, loff_t *ppos)
{
	int i = 0;
	char *tok;
	long tmp;
	int len;
	char v6_addr_info[128] = {0};
	char *p_v6_addr_info = &v6_addr_info[0];
	char *p_v6_addr;
	long index = 0;

	if (*ppos != 0)
		return -EINVAL;

	len = simple_write_to_buffer(v6_addr_info, sizeof(v6_addr_info) - 1,
				     ppos, buffer, count);
	if (len < 0)
		return len;

	v6_addr_info[len] = '\0';

	tok = strsep(&p_v6_addr_info, " ");
	if (tok)
		if (kstrtol(tok, 10, &index))
			return -EINVAL;

	if (index < 0 || index >= MAX_IFACE_NUM) {
		pr_err("index %ld is illegal\n", index);
		return -EINVAL;
	}

	tok = strsep(&p_v6_addr_info, " ");
	if (tok)
		p_v6_addr = tok;

	while ((tok = strsep(&p_v6_addr, ":"))) {
		if (!*tok) {
			pr_debug("continue here\n");
			continue;
		}
		if (kstrtol(tok, 16, &tmp))
			return -EINVAL;
		gl_ipv6_iface_id[index].v6_iface_id.in6_u.u6_addr8[i++] =
				IP_SFT(tmp, 8);
		gl_ipv6_iface_id[index].v6_iface_id.in6_u.u6_addr8[i++] =
				IP_SFT(tmp, 0);
		gl_ipv6_iface_id[index].initialized = true;
	}

	return count;
}

static int sn_v6_iface_id_show(struct seq_file *s, void *v)
{
	int i;

	for (i = 0; i < MAX_IFACE_NUM; i++) {
		if (gl_ipv6_iface_id[i].initialized) {
			seq_puts(s, "ipv6 interface id:\n");
			seq_printf(s, "%pI6\n", &gl_ipv6_iface_id[i].v6_iface_id);
		} else {
			seq_printf(s, "ipv6 interface %d id not obtained yet\n", i);
		}
	}
	return 0;
}

static int sn_v6_iface_id_open(struct inode *inode, struct file *file)
{
	return single_open(file, sn_v6_iface_id_show, NULL);
}

static const struct proc_ops v6_iface_id_fops = {
	.proc_open	 = sn_v6_iface_id_open,
	.proc_read	 = seq_read,
	.proc_lseek	 = seq_lseek,
	.proc_release	 = single_release,
	.proc_write	 = sn_v6_iface_id_write,
};

static int sn_v6_iface_init_proc(struct net *net)
{
	if (!proc_create("v6_iface_id", 0644, net->proc_net,
			 &v6_iface_id_fops)) {
		pr_err("proc create v6_iface_id fail!\n");
		goto err_v6_iface;
	}

	return 0;
err_v6_iface:
	remove_proc_entry("v6_iface_stat", net->proc_net);
	return -ENOMEM;
}

static int sn_ip6_parse_ra_options(struct sk_buff *skb)
{
	int err, i;
	int optlen;
	u32 addr_flags = 0;
	struct ra_msg *ra_msg = (struct ra_msg *)skb_transport_header(skb);
	__u8 *opt = (__u8 *)(ra_msg + 1);
	struct nd_opt_hdr *nd_opt = (struct nd_opt_hdr *)opt;
	struct prefix_info *pinfo;
	struct inet6_dev *in6_dev;
	struct net_device *dev = skb->dev;
	struct net *net = dev_net(dev);
	struct in6_addr v6_iface_id;
	long index = 0;

	in6_dev = in6_dev_get(dev);
	if (!in6_dev) {
		pr_warn("device %s not configured\n", dev->name);
		return NF_ACCEPT;
	}

	optlen = (skb_tail_pointer(skb) - skb_transport_header(skb)) -
		  sizeof(struct ra_msg);

	pr_debug("skb %p, optlen %d\n", skb, optlen);

	if (!(ipv6_addr_type(&ipv6_hdr(skb)->saddr) & IPV6_ADDR_LINKLOCAL)) {
		pr_warn("RA: source address is not link-local\n");
		goto put;
	}

	if (!nd_opt || optlen < 0) {
		pr_warn("RA: packet too short\n");
		goto put;
	}

	while (optlen) {
		int l;

		l = nd_opt->nd_opt_len << 3;
		if (optlen < sizeof(struct nd_opt_hdr))
			goto put;

		if (optlen < l || l == 0)
			goto put;

		pr_debug("skb %p, nd_opt_type %d\n", skb, nd_opt->nd_opt_type);
		switch (nd_opt->nd_opt_type) {
		case ND_OPT_PREFIX_INFO:
			for (i = 0; iface_name[i]; i++) {
				if (!strncmp(dev->name, iface_name[i], strlen(iface_name[i])) &&
				    !kstrtol(&dev->name[strlen(iface_name[i])], 10, &index))
					break;

				if (!iface_name[i + 1])
					goto put;
			}

			pinfo = (struct prefix_info *)nd_opt;
			pr_debug("skb %p RA: %pI6, %d\n", skb, &pinfo->prefix, pinfo->prefix_len);

			if (gl_ipv6_iface_id[index].initialized) {
				memcpy(&gl_ipv6_iface_id[index].v6_iface_id, &pinfo->prefix, 8);

				pr_debug("skb %p - index %ld global ipv6: %pI6, %pI6\n", skb, index,
					 &pinfo->prefix, &gl_ipv6_iface_id[index].v6_iface_id);

				v6_iface_id = gl_ipv6_iface_id[index].v6_iface_id;
				err = addrconf_prefix_rcv_add_addr(net, dev, pinfo, in6_dev,
								   &v6_iface_id,
								   ipv6_addr_type(&pinfo->prefix),
								   addr_flags, false,
								   false, ntohl(pinfo->valid),
								   ntohl(pinfo->prefered));

				pr_debug("skb %p, add addr err %d\n", skb, err);
				goto put;
			}
			break;
		default:
			break;
		}
		optlen -= l;
		nd_opt = ((void *)nd_opt) + l;
	}

put:
	in6_dev_put(in6_dev);
	return NF_ACCEPT;
}

static unsigned int nf_sn_ra_process(void *priv,
				     struct sk_buff *skb,
				     const struct nf_hook_state *state)
{
	int ret = NF_ACCEPT;
	__be16 frag_off;
	int offset;
	struct ipv6hdr *hdr = ipv6_hdr(skb);
	u8 nexthdr = hdr->nexthdr;

	if (ipv6_ext_hdr(nexthdr)) {
		offset = ipv6_skip_exthdr(skb, sizeof(*hdr), &nexthdr, &frag_off);
		if (offset < 0)
			return ret;
	} else {
		offset = sizeof(struct ipv6hdr);
	}

	if (nexthdr == IPPROTO_ICMPV6) {
		struct icmp6hdr *icmp6;

		if (!pskb_may_pull(skb, (skb_network_header(skb) +
					 offset + 1 - skb->data)))
			return ret;

		icmp6 = (struct icmp6hdr *)(skb_network_header(skb) + offset);

		pr_debug("skb %p, icmp6 type %d, icmp6 %20ph\n", skb,
			 icmp6->icmp6_type, (char *)icmp6);
		switch (icmp6->icmp6_type) {
		case NDISC_ROUTER_ADVERTISEMENT:
			pr_debug("skb %p is a RA\n", skb);
			ret = sn_ip6_parse_ra_options(skb);
			break;
		default:
			break;
		}
	}

	return ret;
}

static struct nf_hook_ops nf_sn_ip6_ra_ops[] __read_mostly = {
	{
		.hook		= nf_sn_ra_process,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_PRE_ROUTING,
		.priority	= NF_IP6_PRI_MANGLE - 1,
	},
};

static int __net_init sn_ip6_nf_register(struct net *net)
{
	return nf_register_net_hooks(net, nf_sn_ip6_ra_ops,
				     ARRAY_SIZE(nf_sn_ip6_ra_ops));
}

static void __net_init sn_ip6_nf_unregister(struct net *net)
{
	nf_unregister_net_hooks(net, nf_sn_ip6_ra_ops, ARRAY_SIZE(nf_sn_ip6_ra_ops));
}

static struct pernet_operations ip6_net_ops = {
	.init = sn_ip6_nf_register,
	.exit = sn_ip6_nf_unregister,
};

static int __init sn_ip6_hooks_init(void)
{
	int err;

	pr_debug("Registering netfilter hooks\n");

	err = register_pernet_subsys(&ip6_net_ops);
	if (err)
		pr_err("nf_register_hooks: err %d\n", err);

	if (sn_v6_iface_init_proc(&init_net))
		return -ENOMEM;

	return 0;
}

static void sn_ip6_hooks_exit(void)
{
	pr_debug("Unregistering netfilter hooks\n");

	unregister_pernet_subsys(&ip6_net_ops);
}

module_init(sn_ip6_hooks_init)
module_exit(sn_ip6_hooks_exit);

MODULE_AUTHOR("Wade.Shu <wade.shu@unisoc.com>");
MODULE_DESCRIPTION("SPRD NET in AP");
MODULE_LICENSE("GPL");
