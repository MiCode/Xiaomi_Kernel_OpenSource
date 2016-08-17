/*
 * File: l2mux.h
 *
 * MHI L2MUX kernel definitions
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef LINUX_L2MUX_H
#define LINUX_L2MUX_H

#include <linux/types.h>
#include <linux/socket.h>

#include <net/sock.h>


/* Official L3 protocol IDs */
#define MHI_L3_PHONET		0x00
#define MHI_L3_FILE		0x01
#define MHI_L3_AUDIO		0x02
#define MHI_L3_SECURITY		0x03
#define MHI_L3_TEST		0x04
#define MHI_L3_TEST_PRIO	0x05
#define MHI_L3_XFILE		0x06
#define MHI_L3_MHDP_DL		0x07
#define MHI_L3_MHDP_UL		0x08
#define MHI_L3_AUX_HOST		0x09
#define MHI_L3_THERMAL		0xC1
#define MHI_L3_HIGH_PRIO_TEST	0xFD
#define MHI_L3_MED_PRIO_TEST	0xFE
#define MHI_L3_LOW_PRIO_TEST	0xFF

/* 256 possible protocols */
#define MHI_L3_NPROTO		256

/* Special value for ANY */
#define MHI_L3_ANY		0xFFFF

typedef int (l2mux_skb_fn)(struct sk_buff *skb, struct net_device *dev);

struct l2muxhdr {
	__u8	l3_len[3];
	__u8	l3_prot;
} __packed;

#define L2MUX_HDR_SIZE  (sizeof(struct l2muxhdr))


static inline struct l2muxhdr *l2mux_hdr(struct sk_buff *skb)
{
	return (struct l2muxhdr *)skb_mac_header(skb);
}

static inline void l2mux_set_proto(struct l2muxhdr *hdr, int proto)
{
	hdr->l3_prot = proto;
}

static inline int l2mux_get_proto(struct l2muxhdr *hdr)
{
	return hdr->l3_prot;
}

static inline void l2mux_set_length(struct l2muxhdr *hdr, unsigned len)
{
	hdr->l3_len[0] = (len) & 0xFF;
	hdr->l3_len[1] = (len >>  8) & 0xFF;
	hdr->l3_len[2] = (len >> 16) & 0xFF;
}

static inline unsigned l2mux_get_length(struct l2muxhdr *hdr)
{
	return (((unsigned)hdr->l3_len[2]) << 16) |
		(((unsigned)hdr->l3_len[1]) << 8) |
			((unsigned)hdr->l3_len[0]);
}

extern int l2mux_netif_rx_register(int l3, l2mux_skb_fn *rx_fn);
extern int l2mux_netif_rx_unregister(int l3);

extern int l2mux_netif_tx_register(int pt, l2mux_skb_fn *rx_fn);
extern int l2mux_netif_tx_unregister(int pt);

extern int l2mux_skb_rx(struct sk_buff *skb, struct net_device *dev);
extern int l2mux_skb_tx(struct sk_buff *skb, struct net_device *dev);


#endif /* LINUX_L2MUX_H */
