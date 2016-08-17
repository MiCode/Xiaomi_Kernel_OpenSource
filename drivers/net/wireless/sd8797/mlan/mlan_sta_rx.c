/** @file mlan_sta_rx.c
 *
 *  @brief This file contains the handling of RX in MLAN
 *  module.
 *
 *  Copyright (C) 2008-2011, Marvell International Ltd.
 *
 *  This software file (the "File") is distributed by Marvell International
 *  Ltd. under the terms of the GNU General Public License Version 2, June 1991
 *  (the "License").  You may use, redistribute and/or modify this File in
 *  accordance with the terms and conditions of the License, a copy of which
 *  is available by writing to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or on the
 *  worldwide web at http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
 *
 *  THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE
 *  ARE EXPRESSLY DISCLAIMED.  The License provides additional details about
 *  this warranty disclaimer.
 */

/********************************************************
Change log:
    10/27/2008: initial version
********************************************************/

#include "mlan.h"
#include "mlan_join.h"
#include "mlan_util.h"
#include "mlan_fw.h"
#include "mlan_main.h"
#include "mlan_11n_aggr.h"
#include "mlan_11n_rxreorder.h"

/********************************************************
		Local Variables
********************************************************/

/** Ethernet II header */
typedef struct
{
    /** Ethernet II header destination address */
	t_u8 dest_addr[MLAN_MAC_ADDR_LENGTH];
    /** Ethernet II header source address */
	t_u8 src_addr[MLAN_MAC_ADDR_LENGTH];
    /** Ethernet II header length */
	t_u16 ethertype;

} EthII_Hdr_t;

/********************************************************
		Global Variables
********************************************************/

/********************************************************
		Local Functions
********************************************************/

/********************************************************
		Global functions
********************************************************/

/**
 *  @brief This function processes received packet and forwards it
 *  		to kernel/upper layer
 *
 *  @param pmadapter A pointer to mlan_adapter
 *  @param pmbuf     A pointer to mlan_buffer which includes the received packet
 *
 *  @return 	   MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
wlan_process_rx_packet(pmlan_adapter pmadapter, pmlan_buffer pmbuf)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	pmlan_private priv = pmadapter->priv[pmbuf->bss_index];
	RxPacketHdr_t *prx_pkt;
	RxPD *prx_pd;
	int hdr_chop;
	EthII_Hdr_t *peth_hdr;
	t_u8 rfc1042_eth_hdr[MLAN_MAC_ADDR_LENGTH] =
		{ 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };
	t_u8 snap_oui_802_h[MLAN_MAC_ADDR_LENGTH] =
		{ 0xaa, 0xaa, 0x03, 0x00, 0x00, 0xf8 };
	t_u8 appletalk_aarp_type[2] = { 0x80, 0xf3 };
	t_u8 ipx_snap_type[2] = { 0x81, 0x37 };

	ENTER();

	prx_pd = (RxPD *) (pmbuf->pbuf + pmbuf->data_offset);
	prx_pkt = (RxPacketHdr_t *) ((t_u8 *) prx_pd + prx_pd->rx_pkt_offset);

/** Small debug type */
#define DBG_TYPE_SMALL  2
/** Size of debugging structure */
#define SIZE_OF_DBG_STRUCT 4
	if (prx_pd->rx_pkt_type == PKT_TYPE_DEBUG) {
		t_u8 dbgType;
		dbgType = *(t_u8 *) & prx_pkt->eth803_hdr;
		if (dbgType == DBG_TYPE_SMALL) {
			PRINTM(MFW_D, "\n");
			DBG_HEXDUMP(MFW_D, "FWDBG",
				    (char *)((t_u8 *) & prx_pkt->eth803_hdr +
					     SIZE_OF_DBG_STRUCT),
				    prx_pd->rx_pkt_length);
			PRINTM(MFW_D, "FWDBG::\n");
		}
		goto done;
	}

	PRINTM(MINFO,
	       "RX Data: data_len - prx_pd->rx_pkt_offset = %d - %d = %d\n",
	       pmbuf->data_len, prx_pd->rx_pkt_offset,
	       pmbuf->data_len - prx_pd->rx_pkt_offset);

	HEXDUMP("RX Data: Dest", prx_pkt->eth803_hdr.dest_addr,
		sizeof(prx_pkt->eth803_hdr.dest_addr));
	HEXDUMP("RX Data: Src", prx_pkt->eth803_hdr.src_addr,
		sizeof(prx_pkt->eth803_hdr.src_addr));

	if ((memcmp(pmadapter, &prx_pkt->rfc1042_hdr,
		    snap_oui_802_h, sizeof(snap_oui_802_h)) == 0) ||
	    ((memcmp(pmadapter, &prx_pkt->rfc1042_hdr,
		     rfc1042_eth_hdr, sizeof(rfc1042_eth_hdr)) == 0) &&
	     memcmp(pmadapter, &prx_pkt->rfc1042_hdr.snap_type,
		    appletalk_aarp_type, sizeof(appletalk_aarp_type)) &&
	     memcmp(pmadapter, &prx_pkt->rfc1042_hdr.snap_type,
		    ipx_snap_type, sizeof(ipx_snap_type)))) {
		/*
		 *  Replace the 803 header and rfc1042 header (llc/snap) with an
		 *    EthernetII header, keep the src/dst and snap_type (ethertype).
		 *  The firmware only passes up SNAP frames converting
		 *    all RX Data from 802.11 to 802.2/LLC/SNAP frames.
		 *  To create the Ethernet II, just move the src, dst address right
		 *    before the snap_type.
		 */
		peth_hdr = (EthII_Hdr_t *)
			((t_u8 *) & prx_pkt->eth803_hdr
			 + sizeof(prx_pkt->eth803_hdr) +
			 sizeof(prx_pkt->rfc1042_hdr)
			 - sizeof(prx_pkt->eth803_hdr.dest_addr)
			 - sizeof(prx_pkt->eth803_hdr.src_addr)
			 - sizeof(prx_pkt->rfc1042_hdr.snap_type));

		memcpy(pmadapter, peth_hdr->src_addr,
		       prx_pkt->eth803_hdr.src_addr,
		       sizeof(peth_hdr->src_addr));
		memcpy(pmadapter, peth_hdr->dest_addr,
		       prx_pkt->eth803_hdr.dest_addr,
		       sizeof(peth_hdr->dest_addr));

		/* Chop off the RxPD + the excess memory from the
		   802.2/llc/snap header that was removed. */
		hdr_chop = (t_u32) ((t_ptr) peth_hdr - (t_ptr) prx_pd);
	} else {
		HEXDUMP("RX Data: LLC/SNAP",
			(t_u8 *) & prx_pkt->rfc1042_hdr,
			sizeof(prx_pkt->rfc1042_hdr));

		/* Chop off the RxPD */
		hdr_chop =
			(t_u32) ((t_ptr) & prx_pkt->eth803_hdr -
				 (t_ptr) prx_pd);
	}

	/* Chop off the leading header bytes so the it points to the start of
	   either the reconstructed EthII frame or the 802.2/llc/snap frame */
	pmbuf->data_len -= hdr_chop;
	pmbuf->data_offset += hdr_chop;
	pmbuf->pparent = MNULL;

	DBG_HEXDUMP(MDAT_D, "RxPD", (t_u8 *) prx_pd,
		    MIN(sizeof(RxPD), MAX_DATA_DUMP_LEN));
	DBG_HEXDUMP(MDAT_D, "Rx Payload",
		    ((t_u8 *) prx_pd + prx_pd->rx_pkt_offset),
		    MIN(prx_pd->rx_pkt_length, MAX_DATA_DUMP_LEN));

	priv->rxpd_rate = prx_pd->rx_rate;
	priv->rxpd_htinfo = prx_pd->ht_info;

	pmadapter->callbacks.moal_get_system_time(pmadapter->pmoal_handle,
						  &pmbuf->out_ts_sec,
						  &pmbuf->out_ts_usec);
	PRINTM_NETINTF(MDATA, priv);
	PRINTM(MDATA, "%lu.%06lu : Data => kernel seq_num=%d tid=%d\n",
	       pmbuf->out_ts_sec, pmbuf->out_ts_usec, prx_pd->seq_num,
	       prx_pd->priority);

	ret = pmadapter->callbacks.moal_recv_packet(pmadapter->pmoal_handle,
						    pmbuf);
	if (ret == MLAN_STATUS_FAILURE) {
		pmbuf->status_code = MLAN_ERROR_PKT_INVALID;
		PRINTM(MERROR,
		       "STA Rx Error: moal_recv_packet returned error\n");
	}
done:
	if (ret != MLAN_STATUS_PENDING) {
		wlan_free_mlan_buffer(pmadapter, pmbuf);
	}
	LEAVE();

	return ret;
}

/**
 *   @brief This function processes the received buffer
 *
 *   @param adapter A pointer to mlan_adapter
 *   @param pmbuf     A pointer to the received buffer
 *
 *   @return        MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
wlan_ops_sta_process_rx_packet(IN t_void * adapter, IN pmlan_buffer pmbuf)
{
	pmlan_adapter pmadapter = (pmlan_adapter) adapter;
	mlan_status ret = MLAN_STATUS_SUCCESS;
	RxPD *prx_pd;
	RxPacketHdr_t *prx_pkt;
	pmlan_private priv = pmadapter->priv[pmbuf->bss_index];
	t_u8 ta[MLAN_MAC_ADDR_LENGTH];
	t_u16 rx_pkt_type = 0;
	wlan_mgmt_pkt *pmgmt_pkt_hdr = MNULL;
	ENTER();

	prx_pd = (RxPD *) (pmbuf->pbuf + pmbuf->data_offset);
	/* Endian conversion */
	endian_convert_RxPD(prx_pd);
	rx_pkt_type = prx_pd->rx_pkt_type;
	prx_pkt = (RxPacketHdr_t *) ((t_u8 *) prx_pd + prx_pd->rx_pkt_offset);

	if ((prx_pd->rx_pkt_offset + prx_pd->rx_pkt_length) >
	    (t_u16) pmbuf->data_len) {
		PRINTM(MERROR,
		       "Wrong rx packet: len=%d,rx_pkt_offset=%d,"
		       " rx_pkt_length=%d\n", pmbuf->data_len,
		       prx_pd->rx_pkt_offset, prx_pd->rx_pkt_length);
		pmbuf->status_code = MLAN_ERROR_PKT_SIZE_INVALID;
		ret = MLAN_STATUS_FAILURE;
		wlan_free_mlan_buffer(pmadapter, pmbuf);
		goto done;
	}
	pmbuf->data_len = prx_pd->rx_pkt_offset + prx_pd->rx_pkt_length;

	if (pmadapter->priv[pmbuf->bss_index]->mgmt_frame_passthru_mask &&
	    prx_pd->rx_pkt_type == PKT_TYPE_MGMT_FRAME) {
		/* Check if this is mgmt packet and needs to forwarded to app
		   as an event */
		pmgmt_pkt_hdr =
			(wlan_mgmt_pkt *) ((t_u8 *) prx_pd +
					   prx_pd->rx_pkt_offset);
		pmgmt_pkt_hdr->frm_len =
			wlan_le16_to_cpu(pmgmt_pkt_hdr->frm_len);

		if ((pmgmt_pkt_hdr->wlan_header.frm_ctl
		     & IEEE80211_FC_MGMT_FRAME_TYPE_MASK) == 0)
			wlan_process_802dot11_mgmt_pkt(pmadapter->
						       priv[pmbuf->bss_index],
						       (t_u8 *) &
						       pmgmt_pkt_hdr->
						       wlan_header,
						       pmgmt_pkt_hdr->frm_len +
						       sizeof(wlan_mgmt_pkt)
						       -
						       sizeof(pmgmt_pkt_hdr->
							      frm_len));
		wlan_free_mlan_buffer(pmadapter, pmbuf);
		goto done;
	}

	/*
	 * If the packet is not an unicast packet then send the packet
	 * directly to os. Don't pass thru rx reordering
	 */
	if (!IS_11N_ENABLED(priv) ||
	    memcmp(priv->adapter, priv->curr_addr,
		   prx_pkt->eth803_hdr.dest_addr, MLAN_MAC_ADDR_LENGTH)) {
		wlan_process_rx_packet(pmadapter, pmbuf);
		goto done;
	}

	if (queuing_ra_based(priv)) {
		memcpy(pmadapter, ta, prx_pkt->eth803_hdr.src_addr,
		       MLAN_MAC_ADDR_LENGTH);
	} else {
		if ((rx_pkt_type != PKT_TYPE_BAR) &&
		    (prx_pd->priority < MAX_NUM_TID))
			priv->rx_seq[prx_pd->priority] = prx_pd->seq_num;
		memcpy(pmadapter, ta,
		       priv->curr_bss_params.bss_descriptor.mac_address,
		       MLAN_MAC_ADDR_LENGTH);
	}

	/* Reorder and send to OS */
	if ((ret = mlan_11n_rxreorder_pkt(priv, prx_pd->seq_num,
					  prx_pd->priority, ta,
					  (t_u8) prx_pd->rx_pkt_type,
					  (void *)pmbuf)) ||
	    (rx_pkt_type == PKT_TYPE_BAR)
		) {
		wlan_free_mlan_buffer(pmadapter, pmbuf);
	}

done:

	LEAVE();
	return (ret);
}
