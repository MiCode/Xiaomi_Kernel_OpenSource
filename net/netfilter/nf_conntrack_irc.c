/* IRC extension for IP connection tracking, Version 1.21
 * (C) 2000-2002 by Harald Welte <laforge@gnumonks.org>
 * based on RR's ip_conntrack_ftp.c
 * (C) 2006-2012 Patrick McHardy <kaber@trash.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/skbuff.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/netfilter.h>
#include <linux/slab.h>
#include <linux/list.h>

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <linux/netfilter/nf_conntrack_irc.h>

#define MAX_PORTS 8
static unsigned short ports[MAX_PORTS];
static unsigned int ports_c;
static unsigned int max_dcc_channels = 8;
static unsigned int dcc_timeout __read_mostly = 300;
/* This is slow, but it's simple. --RR */
static char *irc_buffer;
struct irc_client_info {
	char *nickname;
	bool conn_to_server;
	int nickname_len;
	__be32 server_ip;
	__be32 client_ip;
	struct list_head ptr;
	};

static struct irc_client_info client_list;

static unsigned int no_of_clients;
static DEFINE_SPINLOCK(irc_buffer_lock);

unsigned int (*nf_nat_irc_hook)(struct sk_buff *skb,
				enum ip_conntrack_info ctinfo,
				unsigned int protoff,
				unsigned int matchoff,
				unsigned int matchlen,
				struct nf_conntrack_expect *exp) __read_mostly;
EXPORT_SYMBOL_GPL(nf_nat_irc_hook);

MODULE_AUTHOR("Harald Welte <laforge@netfilter.org>");
MODULE_DESCRIPTION("IRC (DCC) connection tracking helper");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ip_conntrack_irc");
MODULE_ALIAS_NFCT_HELPER("irc");

module_param_array(ports, ushort, &ports_c, 0400);
MODULE_PARM_DESC(ports, "port numbers of IRC servers");
module_param(max_dcc_channels, uint, 0400);
MODULE_PARM_DESC(max_dcc_channels, "max number of expected DCC channels per "
				   "IRC session");
module_param(dcc_timeout, uint, 0400);
MODULE_PARM_DESC(dcc_timeout, "timeout on for unestablished DCC channels");

static const char *const dccprotos[] = {
	"SEND ", "CHAT ", "MOVE ", "TSEND ", "SCHAT "
};

#define MINMATCHLEN	5
#define MINLENNICK	1
/* tries to get the ip_addr and port out of a dcc command
 * return value: -1 on failure, 0 on success
 *	data		pointer to first byte of DCC command data
 *	data_end	pointer to last byte of dcc command data
 *	ip		returns parsed ip of dcc command
 *	port		returns parsed port of dcc command
 *	ad_beg_p	returns pointer to first byte of addr data
 *	ad_end_p	returns pointer to last byte of addr data
 */
static struct irc_client_info *search_client_by_ip
(
	struct nf_conntrack_tuple *tuple
)
{
	struct irc_client_info *temp, *ret = NULL;
	struct list_head *obj_ptr, *prev_obj_ptr;

	list_for_each_safe(obj_ptr, prev_obj_ptr, &client_list.ptr) {
		temp = list_entry(obj_ptr, struct irc_client_info, ptr);
		if ((temp->client_ip == tuple->src.u3.ip) &&
		    (temp->server_ip == tuple->dst.u3.ip))
			ret = temp;
	}
	return ret;
}

static int parse_dcc(char *data, const char *data_end, __be32 *ip,
		     u_int16_t *port, char **ad_beg_p, char **ad_end_p)
{
	char *tmp;

	/* at least 12: "AAAAAAAA P\1\n" */
	while (*data++ != ' ')
		if (data > data_end - 12)
			return -1;

	/* Make sure we have a newline character within the packet boundaries
	 * because simple_strtoul parses until the first invalid character. */
	for (tmp = data; tmp <= data_end; tmp++)
		if (*tmp == '\n')
			break;
	if (tmp > data_end || *tmp != '\n')
		return -1;

	*ad_beg_p = data;
	*ip = cpu_to_be32(simple_strtoul(data, &data, 10));

	/* skip blanks between ip and port */
	while (*data == ' ') {
		if (data >= data_end)
			return -1;
		data++;
	}

	*port = simple_strtoul(data, &data, 10);
	*ad_end_p = data;

	return 0;
}

static bool mangle_ip(struct nf_conn *ct,
		      int dir, char *nick_start)
{
	char *nick_end;
	struct nf_conntrack_tuple *tuple;
	struct irc_client_info *temp;
	struct list_head *obj_ptr, *prev_obj_ptr;

	tuple = &ct->tuplehash[dir].tuple;
	nick_end = nick_start;
	while (*nick_end != ' ')
		nick_end++;
	list_for_each_safe(obj_ptr, prev_obj_ptr,
			   &client_list.ptr) {
		temp = list_entry(obj_ptr,
				  struct irc_client_info, ptr);
		/*If it is an internal client,
		 *do not mangle the DCC Server IP
		 */
		if ((temp->server_ip == tuple->dst.u3.ip) &&
		    (temp->nickname_len == (nick_end - nick_start))) {
			if (memcmp(nick_start, temp->nickname,
				   temp->nickname_len) == 0)
				return false;
		}
	}
	return true;
}

static int handle_nickname(struct nf_conn *ct,
			   int dir, char *nick_start)
{
	char *nick_end;
	struct nf_conntrack_tuple *tuple;
	struct irc_client_info *temp;
	int i, j;
	bool add_entry = true;

	nick_end = nick_start;
	i = 0;
	while (*nick_end != '\n') {
		nick_end++;
		i++;
	}
	tuple = &ct->tuplehash[dir].tuple;
	/*Check if the entry is already
	 * present for that client
	 */
	temp = search_client_by_ip(tuple);
	if (temp) {
		add_entry = false;
		/*Update nickname if the client is not already
		 * connected to the server.If the client is
		 * connected, wait for server to confirm
		 * if nickname is valid
		 */
		if (!temp->conn_to_server) {
			kfree(temp->nickname);
			temp->nickname =
				kmalloc(i, GFP_ATOMIC);
			if (temp->nickname) {
				temp->nickname_len = i;
				memcpy(temp->nickname,
				       nick_start, temp->nickname_len);
			} else {
				list_del(&temp->ptr);
				no_of_clients--;
				kfree(temp);
			}
		}
	}
	/*Add client entry if not already present*/
	if (add_entry) {
		j = sizeof(struct irc_client_info);
		temp = kmalloc(j, GFP_ATOMIC);
		if (temp) {
			no_of_clients++;
			tuple = &ct->tuplehash[dir].tuple;
			temp->nickname_len = i;
			temp->nickname =
				kmalloc(temp->nickname_len, GFP_ATOMIC);
			if (!temp->nickname) {
				kfree(temp);
				return NF_DROP;
			}
			memcpy(temp->nickname, nick_start,
			       temp->nickname_len);
			memcpy(&temp->client_ip,
			       &tuple->src.u3.ip, sizeof(__be32));
			memcpy(&temp->server_ip,
			       &tuple->dst.u3.ip, sizeof(__be32));
			temp->conn_to_server = false;
			list_add(&temp->ptr,
				 &client_list.ptr);
		} else {
			return NF_DROP;
		}
	}
	return NF_ACCEPT;
}
static int help(struct sk_buff *skb, unsigned int protoff,
		struct nf_conn *ct, enum ip_conntrack_info ctinfo)
{
	unsigned int dataoff;
	const struct iphdr *iph;
	const struct tcphdr *th;
	struct tcphdr _tcph;
	const char *data_limit;
	char *data, *ib_ptr, *for_print, *nick_end;
	int dir = CTINFO2DIR(ctinfo);
	struct nf_conntrack_expect *exp;
	struct nf_conntrack_tuple *tuple;
	__be32 dcc_ip;
	u_int16_t dcc_port;
	__be16 port;
	int i, ret = NF_ACCEPT;
	char *addr_beg_p, *addr_end_p;
	typeof(nf_nat_irc_hook) nf_nat_irc;
	struct irc_client_info *temp;
	bool mangle = true;

	/* Until there's been traffic both ways, don't look in packets. */
	if (ctinfo != IP_CT_ESTABLISHED && ctinfo != IP_CT_ESTABLISHED_REPLY)
		return NF_ACCEPT;

	/* Not a full tcp header? */
	th = skb_header_pointer(skb, protoff, sizeof(_tcph), &_tcph);
	if (th == NULL)
		return NF_ACCEPT;

	/* No data? */
	dataoff = protoff + th->doff*4;
	if (dataoff >= skb->len)
		return NF_ACCEPT;

	spin_lock_bh(&irc_buffer_lock);
	ib_ptr = skb_header_pointer(skb, dataoff, skb->len - dataoff,
				    irc_buffer);
	BUG_ON(ib_ptr == NULL);

	data = ib_ptr;
	data_limit = ib_ptr + skb->len - dataoff;

	/* If packet is coming from IRC server
	 * parse the packet for different type of
	 * messages (MOTD,NICK etc) and process
	 * accordingly
	 */
	if (dir == IP_CT_DIR_REPLY) {
		/* strlen("NICK xxxxxx")
		 * 5+strlen("xxxxxx")=1 (minimum length of nickname)
		 */

		while (data < data_limit - 6) {
			if (memcmp(data, " MOTD ", 6)) {
				data++;
				continue;
			}
			/* MOTD message signifies successful
			 * registration with server
			 */
			tuple = &ct->tuplehash[!dir].tuple;
			temp = search_client_by_ip(tuple);
			if (temp && !temp->conn_to_server)
				temp->conn_to_server = true;
			ret = NF_ACCEPT;
			goto out;
		}

		/* strlen("NICK :xxxxxx")
		 * 6+strlen("xxxxxx")=1 (minimum length of nickname)
		 * Parsing the server reply to get nickname
		 * of the client
		 */
		data = ib_ptr;
		data_limit = ib_ptr + skb->len - dataoff;
		while (data < data_limit - (6 + MINLENNICK)) {
			if (memcmp(data, "NICK :", 6)) {
				data++;
				continue;
			}
			data += 6;
			nick_end = data;
			i = 0;
			while ((*nick_end != 0x0d) &&
			       (*(nick_end + 1) != '\n')) {
				nick_end++;
				i++;
			}
			tuple = &ct->tuplehash[!dir].tuple;
			temp = search_client_by_ip(tuple);
			if (temp && temp->nickname) {
				kfree(temp->nickname);
				temp->nickname = kmalloc(i, GFP_ATOMIC);
				if (temp->nickname) {
					temp->nickname_len = i;
					memcpy(temp->nickname, data,
					       temp->nickname_len);
					temp->conn_to_server = true;
				} else {
					list_del(&temp->ptr);
					no_of_clients--;
					kfree(temp);
					ret = NF_ACCEPT;
				}
			}
			/*NICK during registration*/
			ret = NF_ACCEPT;
			goto out;
		}
	}

	else{
		/*Parsing NICK command from client to create an entry
		 * strlen("NICK xxxxxx")
		 * 5+strlen("xxxxxx")=1 (minimum length of nickname)
		 */
		data = ib_ptr;
		data_limit = ib_ptr + skb->len - dataoff;
		while (data < data_limit - (5 + MINLENNICK)) {
			if (memcmp(data, "NICK ", 5)) {
				data++;
				continue;
			}
			data += 5;
			ret = handle_nickname(ct, dir, data);
			goto out;
		}

		data = ib_ptr;
		while (data < data_limit - 6) {
			if (memcmp(data, "QUIT :", 6)) {
				data++;
				continue;
			}
			/* Parsing QUIT to free the list entry
			 */
			tuple = &ct->tuplehash[dir].tuple;
			temp = search_client_by_ip(tuple);
			if (temp) {
				list_del(&temp->ptr);
				no_of_clients--;
				kfree(temp->nickname);
				kfree(temp);
			}
			ret = NF_ACCEPT;
			goto out;
		}
		/* strlen("\1DCC SENT t AAAAAAAA P\1\n")=24
		 * 5+MINMATCHLEN+strlen("t AAAAAAAA P\1\n")=14
		 */
		data = ib_ptr;
		while (data < data_limit - (19 + MINMATCHLEN)) {
			if (memcmp(data, "\1DCC ", 5)) {
				data++;
				continue;
			}
			data += 5;
			/* we have at least (19+MINMATCHLEN)-5
			 *bytes valid data left
			 */
			iph = ip_hdr(skb);
			pr_debug("DCC found in master %pI4:%u %pI4:%u\n",
				 &iph->saddr, ntohs(th->source),
				 &iph->daddr, ntohs(th->dest));

			for (i = 0; i < ARRAY_SIZE(dccprotos); i++) {
				if (memcmp(data, dccprotos[i],
					   strlen(dccprotos[i]))) {
					/* no match */
					continue;
				}
				data += strlen(dccprotos[i]);
				pr_debug("DCC %s detected\n", dccprotos[i]);

				/* we have at least
				 * (19+MINMATCHLEN)-5-dccprotos[i].matchlen
				 *bytes valid data left (== 14/13 bytes)
				 */
				if (parse_dcc(data, data_limit, &dcc_ip,
					      &dcc_port, &addr_beg_p,
					      &addr_end_p)) {
					pr_debug("unable to parse dcc command\n");
					continue;
				}

				pr_debug("DCC bound ip/port: %pI4:%u\n",
					 &dcc_ip, dcc_port);

				/* dcc_ip can be the internal OR
				 *external (NAT'ed) IP
				 */
				tuple = &ct->tuplehash[dir].tuple;
				if (tuple->src.u3.ip != dcc_ip &&
				    tuple->dst.u3.ip != dcc_ip) {
					net_warn_ratelimited("Forged DCC command from %pI4: %pI4:%u\n",
							     &tuple->src.u3.ip,
							     &dcc_ip, dcc_port);
					continue;
				}

				exp = nf_ct_expect_alloc(ct);
				if (!exp) {
					nf_ct_helper_log(skb, ct,
							 "cannot alloc expectation");
					ret = NF_DROP;
					goto out;
				}
				tuple = &ct->tuplehash[!dir].tuple;
				port = htons(dcc_port);
				nf_ct_expect_init(exp,
						  NF_CT_EXPECT_CLASS_DEFAULT,
						  tuple->src.l3num,
						  NULL, &tuple->dst.u3,
						  IPPROTO_TCP, NULL, &port);

				nf_nat_irc = rcu_dereference(nf_nat_irc_hook);

				tuple = &ct->tuplehash[dir].tuple;
				for_print = ib_ptr;
				/* strlen("PRIVMSG xxxx :\1DCC
				 *SENT t AAAAAAAA P\1\n")=26
				 * 8+strlen(xxxx) = 1(min length)+7+
				 *MINMATCHLEN+strlen("t AAAAAAAA P\1\n")=14
				 *Parsing DCC command to get client name and
				 *check whether it is an internal client
				 */
				while (for_print <
				       data_limit - (25 + MINMATCHLEN)) {
					if (memcmp(for_print, "PRIVMSG ", 8)) {
						for_print++;
						continue;
					}
					for_print += 8;
					mangle = mangle_ip(ct,
							   dir, for_print);
					break;
				}
				if (mangle &&
				    nf_nat_irc &&
				    ct->status & IPS_NAT_MASK)
					ret = nf_nat_irc(skb, ctinfo,
							 protoff,
							 addr_beg_p - ib_ptr,
							 addr_end_p
							 - addr_beg_p,
							 exp);

				else if (mangle &&
					 nf_ct_expect_related(exp)
					 != 0) {
					nf_ct_helper_log(skb, ct,
							 "cannot add expectation");
					ret = NF_DROP;
				}
				nf_ct_expect_put(exp);
				goto out;
			}
		}
	}
 out:
	spin_unlock_bh(&irc_buffer_lock);
	return ret;
}

static struct nf_conntrack_helper irc[MAX_PORTS] __read_mostly;
static struct nf_conntrack_expect_policy irc_exp_policy;

static void nf_conntrack_irc_fini(void);

static int __init nf_conntrack_irc_init(void)
{
	int i, ret;

	if (max_dcc_channels < 1) {
		pr_err("max_dcc_channels must not be zero\n");
		return -EINVAL;
	}

	irc_exp_policy.max_expected = max_dcc_channels;
	irc_exp_policy.timeout = dcc_timeout;

	irc_buffer = kmalloc(65536, GFP_KERNEL);
	if (!irc_buffer)
		return -ENOMEM;

	/* If no port given, default to standard irc port */
	if (ports_c == 0)
		ports[ports_c++] = IRC_PORT;

	for (i = 0; i < ports_c; i++) {
		nf_ct_helper_init(&irc[i], AF_INET, IPPROTO_TCP, "irc",
				  IRC_PORT, ports[i], i, &irc_exp_policy,
				  0, 0, help, NULL, THIS_MODULE);
	}

	ret = nf_conntrack_helpers_register(&irc[0], ports_c);
	if (ret) {
		pr_err("failed to register helpers\n");
		kfree(irc_buffer);
		return ret;
	}
	no_of_clients = 0;
	INIT_LIST_HEAD(&client_list.ptr);
	return 0;
}

/* This function is intentionally _NOT_ defined as __exit, because
 * it is needed by the init function */
static void nf_conntrack_irc_fini(void)
{
	nf_conntrack_helpers_unregister(irc, ports_c);
	kfree(irc_buffer);
}

module_init(nf_conntrack_irc_init);
module_exit(nf_conntrack_irc_fini);
