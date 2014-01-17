/*
 * IP Packet Parser Module.
 *
 * Copyright (C) 1999-2014, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: dhd_ip.c 436748 2013-11-15 03:12:22Z $
 */
#include <typedefs.h>
#include <osl.h>

#include <proto/ethernet.h>
#include <proto/vlan.h>
#include <proto/802.3.h>
#include <proto/bcmip.h>
#include <bcmendian.h>

#include <dhd_dbg.h>

#include <dhd_ip.h>

#ifdef DHDTCPACK_SUPPRESS
#include <dhd_bus.h>
#include <proto/bcmtcp.h>
#endif /* DHDTCPACK_SUPPRESS */

/* special values */
/* 802.3 llc/snap header */
static const uint8 llc_snap_hdr[SNAP_HDR_LEN] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};

pkt_frag_t pkt_frag_info(osl_t *osh, void *p)
{
	uint8 *frame;
	int length;
	uint8 *pt;			/* Pointer to type field */
	uint16 ethertype;
	struct ipv4_hdr *iph;		/* IP frame pointer */
	int ipl;			/* IP frame length */
	uint16 iph_frag;

	ASSERT(osh && p);

	frame = PKTDATA(osh, p);
	length = PKTLEN(osh, p);

	/* Process Ethernet II or SNAP-encapsulated 802.3 frames */
	if (length < ETHER_HDR_LEN) {
		DHD_INFO(("%s: short eth frame (%d)\n", __FUNCTION__, length));
		return DHD_PKT_FRAG_NONE;
	} else if (ntoh16(*(uint16 *)(frame + ETHER_TYPE_OFFSET)) >= ETHER_TYPE_MIN) {
		/* Frame is Ethernet II */
		pt = frame + ETHER_TYPE_OFFSET;
	} else if (length >= ETHER_HDR_LEN + SNAP_HDR_LEN + ETHER_TYPE_LEN &&
	           !bcmp(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN)) {
		pt = frame + ETHER_HDR_LEN + SNAP_HDR_LEN;
	} else {
		DHD_INFO(("%s: non-SNAP 802.3 frame\n", __FUNCTION__));
		return DHD_PKT_FRAG_NONE;
	}

	ethertype = ntoh16(*(uint16 *)pt);

	/* Skip VLAN tag, if any */
	if (ethertype == ETHER_TYPE_8021Q) {
		pt += VLAN_TAG_LEN;

		if (pt + ETHER_TYPE_LEN > frame + length) {
			DHD_INFO(("%s: short VLAN frame (%d)\n", __FUNCTION__, length));
			return DHD_PKT_FRAG_NONE;
		}

		ethertype = ntoh16(*(uint16 *)pt);
	}

	if (ethertype != ETHER_TYPE_IP) {
		DHD_INFO(("%s: non-IP frame (ethertype 0x%x, length %d)\n",
			__FUNCTION__, ethertype, length));
		return DHD_PKT_FRAG_NONE;
	}

	iph = (struct ipv4_hdr *)(pt + ETHER_TYPE_LEN);
	ipl = (uint)(length - (pt + ETHER_TYPE_LEN - frame));

	/* We support IPv4 only */
	if ((ipl < IPV4_OPTIONS_OFFSET) || (IP_VER(iph) != IP_VER_4)) {
		DHD_INFO(("%s: short frame (%d) or non-IPv4\n", __FUNCTION__, ipl));
		return DHD_PKT_FRAG_NONE;
	}

	iph_frag = ntoh16(iph->frag);

	if (iph_frag & IPV4_FRAG_DONT) {
		return DHD_PKT_FRAG_NONE;
	} else if ((iph_frag & IPV4_FRAG_MORE) == 0) {
		return DHD_PKT_FRAG_LAST;
	} else {
		return (iph_frag & IPV4_FRAG_OFFSET_MASK)? DHD_PKT_FRAG_CONT : DHD_PKT_FRAG_FIRST;
	}
}

#ifdef DHDTCPACK_SUPPRESS
void dhd_tcpack_suppress_set(dhd_pub_t *dhdp, bool on)
{
	if (dhdp->tcpack_sup_enabled != on) {
		DHD_ERROR(("%s %d: %d -> %d\n", __FUNCTION__, __LINE__,
			dhdp->tcpack_sup_enabled, on));
		dhd_os_tcpacklock(dhdp);
		dhdp->tcpack_sup_enabled = on;
		dhdp->tcp_ack_info_cnt = 0;
		bzero(dhdp->tcp_ack_info_tbl, sizeof(struct tcp_ack_info) * MAXTCPSTREAMS);
		dhd_os_tcpackunlock(dhdp);
	} else
		DHD_ERROR(("%s %d: already %d\n", __FUNCTION__, __LINE__, on));

	return;
}

void
dhd_tcpack_info_tbl_clean(dhd_pub_t *dhdp)
{
	if (!dhdp->tcpack_sup_enabled)
		goto exit;

	dhd_os_tcpacklock(dhdp);
	dhdp->tcp_ack_info_cnt = 0;
	bzero(&dhdp->tcp_ack_info_tbl, sizeof(struct tcp_ack_info) * MAXTCPSTREAMS);
	dhd_os_tcpackunlock(dhdp);

exit:
	return;
}

inline int dhd_tcpack_check_xmit(dhd_pub_t *dhdp, void *pkt)
{
	uint8 i;
	tcp_ack_info_t *tcp_ack_info = NULL;
	int tbl_cnt;
	uint pushed_len;
	int ret = BCME_OK;
	void *pdata;
	uint32 pktlen;

	if (!dhdp->tcpack_sup_enabled)
		return ret;

	pdata = PKTDATA(dhdp->osh, pkt);

	/* Length of BDC(+WLFC) headers pushed */
	pushed_len = BDC_HEADER_LEN + (((struct bdc_header *)pdata)->dataOffset * 4);
	pktlen = PKTLEN(dhdp->osh, pkt) - pushed_len;

	if (pktlen < TCPACKSZMIN || pktlen > TCPACKSZMAX) {
		DHD_TRACE(("%s %d: Too short or long length %d to be TCP ACK\n",
			__FUNCTION__, __LINE__, pktlen));
		return ret;
	}

	dhd_os_tcpacklock(dhdp);
	tbl_cnt = dhdp->tcp_ack_info_cnt;
	for (i = 0; i < tbl_cnt; i++) {
		tcp_ack_info = &dhdp->tcp_ack_info_tbl[i];
		if (tcp_ack_info->pkt_in_q == pkt) {
			DHD_TRACE(("%s %d: pkt %p sent out. idx %d, tbl_cnt %d\n",
				__FUNCTION__, __LINE__, pkt, i, tbl_cnt));
			/* This pkt is being transmitted so remove the tcp_ack_info of it. */
			if (i < tbl_cnt - 1) {
				bcopy(&dhdp->tcp_ack_info_tbl[tbl_cnt - 1],
					&dhdp->tcp_ack_info_tbl[i], sizeof(struct tcp_ack_info));
			}
			bzero(&dhdp->tcp_ack_info_tbl[tbl_cnt - 1], sizeof(struct tcp_ack_info));
			if (--dhdp->tcp_ack_info_cnt < 0) {
				DHD_ERROR(("%s %d: ERROR!!! tcp_ack_info_cnt %d\n",
					__FUNCTION__, __LINE__, dhdp->tcp_ack_info_cnt));
				ret = BCME_ERROR;
			}
			break;
		}
	}
	dhd_os_tcpackunlock(dhdp);
	return ret;
}

bool
dhd_tcpack_suppress(dhd_pub_t *dhdp, void *pkt)
{
	uint8 *new_ether_hdr;	/* Ethernet header of the new packet */
	uint16 new_ether_type;	/* Ethernet type of the new packet */
	uint8 *new_ip_hdr;		/* IP header of the new packet */
	uint8 *new_tcp_hdr;		/* TCP header of the new packet */
	uint32 new_ip_hdr_len;	/* IP header length of the new packet */
	uint32 cur_framelen;
#if defined(DHD_DEBUG)
	uint32 new_tcp_seq_num;		/* TCP sequence number of the new packet */
#endif
	uint32 new_tcp_ack_num;		/* TCP acknowledge number of the new packet */
	uint16 new_ip_total_len;	/* Total length of IP packet for the new packet */
	uint32 new_tcp_hdr_len;		/* TCP header length of the new packet */
	int i;
	bool ret = FALSE;

	if (!dhdp->tcpack_sup_enabled)
		goto exit;

	new_ether_hdr = PKTDATA(dhdp->osh, pkt);
	cur_framelen = PKTLEN(dhdp->osh, pkt);

	if (cur_framelen < TCPACKSZMIN || cur_framelen > TCPACKSZMAX) {
		DHD_TRACE(("%s %d: Too short or long length %d to be TCP ACK\n",
			__FUNCTION__, __LINE__, cur_framelen));
		goto exit;
	}

	new_ether_type = new_ether_hdr[12] << 8 | new_ether_hdr[13];

	if (new_ether_type != ETHER_TYPE_IP) {
		DHD_TRACE(("%s %d: Not a IP packet 0x%x\n",
			__FUNCTION__, __LINE__, new_ether_type));
		goto exit;
	}

	DHD_TRACE(("%s %d: IP pkt! 0x%x\n", __FUNCTION__, __LINE__, new_ether_type));

	new_ip_hdr = new_ether_hdr + ETHER_HDR_LEN;
	cur_framelen -= ETHER_HDR_LEN;

	ASSERT(cur_framelen >= IPV4_MIN_HEADER_LEN);

	new_ip_hdr_len = IPV4_HLEN(new_ip_hdr);
	if (IP_VER(new_ip_hdr) != IP_VER_4 || IPV4_PROT(new_ip_hdr) != IP_PROT_TCP) {
		DHD_TRACE(("%s %d: Not IPv4 nor TCP! ip ver %d, prot %d\n",
			__FUNCTION__, __LINE__, IP_VER(new_ip_hdr), IPV4_PROT(new_ip_hdr)));
		goto exit;
	}

	new_tcp_hdr = new_ip_hdr + new_ip_hdr_len;
	cur_framelen -= new_ip_hdr_len;

	ASSERT(cur_framelen >= TCP_MIN_HEADER_LEN);

	DHD_TRACE(("%s %d: TCP pkt!\n", __FUNCTION__, __LINE__));

	/* is it an ack ? Allow only ACK flag, not to suppress others. */
	if (new_tcp_hdr[TCP_FLAGS_OFFSET] != TCP_FLAG_ACK) {
		DHD_TRACE(("%s %d: Do not touch TCP flag 0x%x\n",
			__FUNCTION__, __LINE__, new_tcp_hdr[TCP_FLAGS_OFFSET]));
		goto exit;
	}

	new_ip_total_len = ntoh16_ua(&new_ip_hdr[IPV4_PKTLEN_OFFSET]);
	new_tcp_hdr_len = 4 * TCP_HDRLEN(new_tcp_hdr[TCP_HLEN_OFFSET]);

	/* This packet has TCP data, so just send */
	if (new_ip_total_len > new_ip_hdr_len + new_tcp_hdr_len) {
		DHD_TRACE(("%s %d: Do nothing for TCP DATA\n", __FUNCTION__, __LINE__));
		goto exit;
	}

	ASSERT(new_ip_total_len == new_ip_hdr_len + new_tcp_hdr_len);

	new_tcp_ack_num = ntoh32_ua(&new_tcp_hdr[TCP_ACK_NUM_OFFSET]);
#if defined(DHD_DEBUG)
	new_tcp_seq_num = ntoh32_ua(&new_tcp_hdr[TCP_SEQ_NUM_OFFSET]);
	DHD_TRACE(("%s %d: TCP ACK seq %u ack %u\n", __FUNCTION__, __LINE__,
		new_tcp_seq_num, new_tcp_ack_num));
#endif

	DHD_TRACE(("%s %d: TCP ACK with zero DATA length"
		" IP addr "IPv4_ADDR_STR" "IPv4_ADDR_STR" TCP port %d %d\n",
		__FUNCTION__, __LINE__,
		IPv4_ADDR_TO_STR(ntoh32_ua(&new_ip_hdr[IPV4_SRC_IP_OFFSET])),
		IPv4_ADDR_TO_STR(ntoh32_ua(&new_ip_hdr[IPV4_DEST_IP_OFFSET])),
		ntoh16_ua(&new_tcp_hdr[TCP_SRC_PORT_OFFSET]),
		ntoh16_ua(&new_tcp_hdr[TCP_DEST_PORT_OFFSET])));

	/* Look for tcp_ack_info that has the same ip src/dst addrs and tcp src/dst ports */
	dhd_os_tcpacklock(dhdp);
	for (i = 0; i < dhdp->tcp_ack_info_cnt; i++) {
		void *oldpkt;	/* TCPACK packet that is already in txq or DelayQ */
		uint8 *old_ether_hdr, *old_ip_hdr, *old_tcp_hdr;
		uint32 old_ip_hdr_len, old_tcp_hdr_len;
		uint32 old_tcpack_num;	/* TCP ACK number of old TCPACK packet in Q */

		if ((oldpkt = dhdp->tcp_ack_info_tbl[i].pkt_in_q) == NULL) {
			DHD_ERROR(("%s %d: Unexpected error!! cur idx %d, ttl cnt %d\n",
				__FUNCTION__, __LINE__, i, dhdp->tcp_ack_info_cnt));
			break;
		}

		if (PKTDATA(dhdp->osh, oldpkt) == NULL) {
			DHD_ERROR(("%s %d: oldpkt data NULL!! cur idx %d, ttl cnt %d\n",
				__FUNCTION__, __LINE__, i, dhdp->tcp_ack_info_cnt));
			break;
		}

		old_ether_hdr = dhdp->tcp_ack_info_tbl[i].pkt_ether_hdr;
		old_ip_hdr = old_ether_hdr + ETHER_HDR_LEN;
		old_ip_hdr_len = IPV4_HLEN(old_ip_hdr);
		old_tcp_hdr = old_ip_hdr + old_ip_hdr_len;
		old_tcp_hdr_len = 4 * TCP_HDRLEN(old_tcp_hdr[TCP_HLEN_OFFSET]);

		DHD_TRACE(("%s %d: oldpkt %p[%d], IP addr "IPv4_ADDR_STR" "IPv4_ADDR_STR
			" TCP port %d %d\n", __FUNCTION__, __LINE__, oldpkt, i,
			IPv4_ADDR_TO_STR(ntoh32_ua(&old_ip_hdr[IPV4_SRC_IP_OFFSET])),
			IPv4_ADDR_TO_STR(ntoh32_ua(&old_ip_hdr[IPV4_DEST_IP_OFFSET])),
			ntoh16_ua(&old_tcp_hdr[TCP_SRC_PORT_OFFSET]),
			ntoh16_ua(&old_tcp_hdr[TCP_DEST_PORT_OFFSET])));

		/* If either of IP address or TCP port number does not match, skip. */
		if (memcmp(&new_ip_hdr[IPV4_SRC_IP_OFFSET],
			&old_ip_hdr[IPV4_SRC_IP_OFFSET], IPV4_ADDR_LEN * 2) ||
			memcmp(&new_tcp_hdr[TCP_SRC_PORT_OFFSET],
			&old_tcp_hdr[TCP_SRC_PORT_OFFSET], TCP_PORT_LEN * 2))
			continue;

		old_tcpack_num = ntoh32_ua(&old_tcp_hdr[TCP_ACK_NUM_OFFSET]);
		if (new_tcp_ack_num > old_tcpack_num) {
			/* New packet has higher TCP ACK number, so it replaces the old packet */
			if (new_ip_hdr_len == old_ip_hdr_len &&
				new_tcp_hdr_len == old_tcp_hdr_len) {
				ASSERT(memcmp(new_ether_hdr, old_ether_hdr, ETHER_HDR_LEN) == 0);
				bcopy(new_ip_hdr, old_ip_hdr, new_ip_total_len);
				PKTFREE(dhdp->osh, pkt, FALSE);
				DHD_TRACE(("%s %d: TCP ACK replace %u -> %u\n",
					__FUNCTION__, __LINE__, old_tcpack_num, new_tcp_ack_num));
				ret = TRUE;
			} else
				DHD_TRACE(("%s %d: lenth mismatch %d != %d || %d != %d\n",
					__FUNCTION__, __LINE__, new_ip_hdr_len, old_ip_hdr_len,
					new_tcp_hdr_len, old_tcp_hdr_len));
		} else {
			DHD_TRACE(("%s %d: ACK number reverse old %u(0x%p) new %u(0x%p)\n",
				__FUNCTION__, __LINE__, old_tcpack_num, oldpkt,
				new_tcp_ack_num, pkt));
#ifdef TCPACK_TEST
			if (new_ip_hdr_len == old_ip_hdr_len &&
				new_tcp_hdr_len == old_tcp_hdr_len) {
				PKTFREE(dhdp->osh, pkt, FALSE);
				ret = TRUE;
			}
#endif
		}
		dhd_os_tcpackunlock(dhdp);
		goto exit;
	}

	if (i == dhdp->tcp_ack_info_cnt && i < MAXTCPSTREAMS) {
		/* No TCPACK packet with the same IP addr and TCP port is found
		 * in tcp_ack_info_tbl. So add this packet to the table.
		 */
		DHD_TRACE(("%s %d: Add pkt 0x%p(ether_hdr 0x%p) to tbl[%d]\n",
			__FUNCTION__, __LINE__, pkt, new_ether_hdr, dhdp->tcp_ack_info_cnt));

		dhdp->tcp_ack_info_tbl[dhdp->tcp_ack_info_cnt].pkt_in_q = pkt;
		dhdp->tcp_ack_info_tbl[dhdp->tcp_ack_info_cnt].pkt_ether_hdr = new_ether_hdr;
		dhdp->tcp_ack_info_cnt++;
	} else {
		ASSERT(i == dhdp->tcp_ack_info_cnt);
		DHD_TRACE(("%s %d: No empty tcp ack info tbl\n",
			__FUNCTION__, __LINE__));
	}
	dhd_os_tcpackunlock(dhdp);

exit:
	return ret;
}
#endif /* DHDTCPACK_SUPPRESS */
