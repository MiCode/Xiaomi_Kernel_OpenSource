/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _IMSBR_HOOKS_H
#define _IMSBR_HOOKS_H

#include <net/netfilter/nf_conntrack.h>

#define ESP_PORT 4500

static inline void
imsbr_nfct_debug(char *prefix, struct sk_buff *skb,
		 struct nf_conntrack_tuple *nft)
{
	if (nft->src.l3num == AF_INET) {
		pr_debug("%s skb=%p(l=%u m=%x) %pI4 %hu -> %pI4 %hu p=%u\n",
			 prefix, skb, skb->len, skb->mark,
			 &nft->src.u3.ip, ntohs(nft->src.u.all),
			 &nft->dst.u3.ip, ntohs(nft->dst.u.all),
			 nft->dst.protonum);
	} else if (nft->src.l3num == AF_INET6) {
		pr_debug("%s skb=%p(l=%u m=%x) %pI6c %hu -> %pI6c %hu p=%u\n",
			 prefix, skb, skb->len, skb->mark,
			 nft->src.u3.ip6, ntohs(nft->src.u.all),
			 nft->dst.u3.ip6, ntohs(nft->dst.u.all),
			 nft->dst.protonum);
	}
}

int imsbr_parse_nfttuple(struct net *net, struct sk_buff *skb,
			 struct nf_conntrack_tuple *nft);

int imsbr_hooks_init(void);
void imsbr_hooks_exit(void);

struct espheader {
	u32 spi;
	u32 seq;
};

extern struct espheader esphs[];

#endif
