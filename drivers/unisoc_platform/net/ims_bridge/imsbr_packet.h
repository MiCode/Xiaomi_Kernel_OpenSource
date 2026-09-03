/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _IMSBR_PACKET_H
#define _IMSBR_PACKET_H

#define IMSBR_PACKET_VER	0

#define IMSBR_SKB_MARK_SIP		(0x10000000)
#define IMSBR_SKB_MARK_IKE		(0x20000000)
#define IMSBR_SKB_MARK_VOICE	(0x04000000)
#define IMSBR_SKB_MARK_VIDEO	(0x08000000)

struct imsbr_packet {
	__u8	version;
	__u8	resv1;
	__u16	resv2;
	__u16	reasm_tlen;
	__u16	frag_off;
	/**
	 * Current packet length can be caculated by
	 * blk.length - sizeof(imsbr_packet)
	 */
	char	packet[0];
};

#ifdef CONFIG_SPRD_IMS_BRIDGE_TEST

struct call_p_function{
	struct sk_buff *(*pkt2skb) (void *pkt, int pktlen);
	void (*dumpcap) (struct sk_buff *skb);
	void (*frag_send) (struct sk_buff *skb);
};

void call_packet_function(struct call_p_function *cpf);

#endif

#define IMSBR_PACKET_MAXSZ \
	(IMSBR_DATA_BLKSZ - sizeof(struct imsbr_packet))

#define INIT_IMSBR_PACKET(p, reasm_tl) do { \
	typeof(p) _p = (p); \
	_p->version = IMSBR_PACKET_VER; \
	_p->resv1 = 0; \
	_p->resv2 = 0; \
	_p->reasm_tlen = reasm_tl; \
	_p->frag_off = 0; \
} while (0)

extern uint imsbr_frag_size;

void imsbr_packet_relay2cp(struct sk_buff *skb);

void imsbr_process_packet(struct imsbr_sipc *sipc, struct sblock *blk,
			  bool freeit);
void imsbr_dumpcap(struct sk_buff *skb);
#endif
