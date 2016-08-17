/** @file mlan_sta_event.c
 *
 *  @brief This file contains MLAN event handling.
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
    10/13/2008: initial version
********************************************************/

#include "mlan.h"
#include "mlan_join.h"
#include "mlan_util.h"
#include "mlan_fw.h"
#include "mlan_main.h"
#include "mlan_wmm.h"
#include "mlan_11n.h"
#include "mlan_11h.h"

/********************************************************
        Global Variables
********************************************************/

/********************************************************
        Local Functions
********************************************************/

/**
 *  @brief This function handles link lost, deauth and
 *  		disassoc events.
 *
 *  @param pmpriv  A pointer to mlan_private structure
 *  @return        N/A
 */
static t_void
wlan_handle_disconnect_event(pmlan_private pmpriv)
{
	ENTER();

	if (pmpriv->media_connected == MTRUE) {
		wlan_reset_connect_state(pmpriv, MTRUE);
	}

	LEAVE();
}

/********************************************************
        Global Functions
********************************************************/
/**
 *  @brief This function handles disconnect event, reports disconnect
 *  		to upper layer, cleans tx/rx packets,
 *  		resets link state etc.
 *
 *  @param priv            A pointer to mlan_private structure
 *  @param drv_disconnect  Flag indicating the driver should disconnect
 *                         and flush pending packets.
 *
 *  @return        N/A
 */
t_void
wlan_reset_connect_state(pmlan_private priv, t_u8 drv_disconnect)
{
	mlan_adapter *pmadapter = priv->adapter;
	mlan_status ret = MLAN_STATUS_SUCCESS;
	state_11d_t enable;

	ENTER();

	PRINTM(MINFO, "Handles disconnect event.\n");

	if (drv_disconnect) {
		priv->media_connected = MFALSE;
		wlan_11h_check_update_radar_det_state(priv);
	}

	if (priv->port_ctrl_mode == MTRUE) {
		/* Close the port on Disconnect */
		PRINTM(MINFO, "DISC: port_status = CLOSED\n");
		priv->port_open = MFALSE;
	}
	pmadapter->scan_block = MFALSE;

	/* Reset SNR/NF/RSSI values */
	priv->data_rssi_last = 0;
	priv->data_nf_last = 0;
	priv->data_rssi_avg = 0;
	priv->data_nf_avg = 0;
	priv->bcn_rssi_last = 0;
	priv->bcn_nf_last = 0;
	priv->bcn_rssi_avg = 0;
	priv->bcn_nf_avg = 0;
	priv->rxpd_rate = 0;
	priv->rxpd_htinfo = 0;
	priv->max_amsdu = 0;

	priv->sec_info.ewpa_enabled = MFALSE;
	priv->sec_info.wpa_enabled = MFALSE;
	priv->sec_info.wpa2_enabled = MFALSE;
	priv->wpa_ie_len = 0;

	priv->sec_info.wapi_enabled = MFALSE;
	priv->wapi_ie_len = 0;
	priv->sec_info.wapi_key_on = MFALSE;

	priv->wps.session_enable = MFALSE;
	memset(priv->adapter, (t_u8 *) & priv->wps.wps_ie, 0x00,
	       sizeof(priv->wps.wps_ie));

	priv->sec_info.encryption_mode = MLAN_ENCRYPTION_MODE_NONE;

	/* Enable auto data rate */
	priv->is_data_rate_auto = MTRUE;
	priv->data_rate = 0;

	if (priv->bss_mode == MLAN_BSS_MODE_IBSS) {
		priv->adhoc_state = ADHOC_IDLE;
		priv->adhoc_is_link_sensed = MFALSE;
		priv->intf_state_11h.adhoc_auto_sel_chan = MTRUE;
	}

	if (drv_disconnect) {
		/* Free Tx and Rx packets, report disconnect to upper layer */
		wlan_clean_txrx(priv);

		/* Need to erase the current SSID and BSSID info */
		memset(pmadapter,
		       &priv->curr_bss_params, 0x00,
		       sizeof(priv->curr_bss_params));
	}
	pmadapter->tx_lock_flag = MFALSE;
	pmadapter->pps_uapsd_mode = MFALSE;
	pmadapter->delay_null_pkt = MFALSE;

	if ((wlan_11d_is_enabled(priv)) &&
	    (priv->state_11d.user_enable_11d == DISABLE_11D)) {

		priv->state_11d.enable_11d = DISABLE_11D;
		enable = DISABLE_11D;

		/* Send cmd to FW to enable/disable 11D function */
		ret = wlan_prepare_cmd(priv,
				       HostCmd_CMD_802_11_SNMP_MIB,
				       HostCmd_ACT_GEN_SET,
				       Dot11D_i, MNULL, &enable);
		if (ret)
			PRINTM(MERROR, "11D: Failed to enable 11D\n");
	}
	if (pmadapter->num_cmd_timeout && pmadapter->curr_cmd &&
	    (pmadapter->cmd_timer_is_set == MFALSE)) {
		LEAVE();
		return;
	}

	wlan_recv_event(priv, MLAN_EVENT_ID_FW_DISCONNECTED, MNULL);

	LEAVE();
}

/**
 *  @brief This function sends the OBSS scan parameters to the application
 *
 *  @param pmpriv     A pointer to mlan_private structure
 *
 *  @return           N/A
 */
t_void
wlan_2040_coex_event(pmlan_private pmpriv)
{
	t_u8 event_buf[100];
	mlan_event *pevent = (mlan_event *) event_buf;
	t_u8 ele_len;

	ENTER();

	if (pmpriv->curr_bss_params.bss_descriptor.poverlap_bss_scan_param &&
	    pmpriv->curr_bss_params.bss_descriptor.poverlap_bss_scan_param->
	    ieee_hdr.element_id == OVERLAPBSSSCANPARAM) {
		ele_len =
			pmpriv->curr_bss_params.bss_descriptor.
			poverlap_bss_scan_param->ieee_hdr.len;
		pevent->bss_index = pmpriv->bss_index;
		pevent->event_id = MLAN_EVENT_ID_DRV_OBSS_SCAN_PARAM;
		/* Copy OBSS scan parameters */
		memcpy(pmpriv->adapter, (t_u8 *) pevent->event_buf,
		       (t_u8 *) & pmpriv->curr_bss_params.bss_descriptor.
		       poverlap_bss_scan_param->obss_scan_param, ele_len);
		pevent->event_len = ele_len;
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_DRV_OBSS_SCAN_PARAM,
				pevent);
	}

	LEAVE();
}

/**
 *  @brief This function handles events generated by firmware
 *
 *  @param priv	A pointer to mlan_private structure
 *
 *  @return		MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
wlan_ops_sta_process_event(IN t_void * priv)
{
	pmlan_private pmpriv = (pmlan_private) priv;
	pmlan_adapter pmadapter = pmpriv->adapter;
	mlan_status ret = MLAN_STATUS_SUCCESS;
	t_u32 eventcause = pmadapter->event_cause;
	t_u8 event_buf[100];
	t_u8 *evt_buf = MNULL;
	pmlan_buffer pmbuf = pmadapter->pmlan_buffer_event;
	t_u16 reason_code;
	pmlan_callbacks pcb = &pmadapter->callbacks;
	mlan_event *pevent = (mlan_event *) event_buf;

	ENTER();

	/* Event length check */
	if ((pmbuf->data_len - sizeof(eventcause)) > MAX_EVENT_SIZE) {
		pmbuf->status_code = MLAN_ERROR_PKT_SIZE_INVALID;
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}

	if (eventcause != EVENT_PS_SLEEP && eventcause != EVENT_PS_AWAKE &&
	    pmbuf->data_len > sizeof(eventcause))
		DBG_HEXDUMP(MEVT_D, "EVENT", pmbuf->pbuf + pmbuf->data_offset,
			    pmbuf->data_len);

	switch (eventcause) {
	case EVENT_DUMMY_HOST_WAKEUP_SIGNAL:
		PRINTM(MERROR,
		       "Invalid EVENT: DUMMY_HOST_WAKEUP_SIGNAL, ignoring it\n");
		break;
	case EVENT_LINK_SENSED:
		PRINTM(MEVENT, "EVENT: LINK_SENSED\n");
		pmpriv->adhoc_is_link_sensed = MTRUE;
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_ADHOC_LINK_SENSED,
				MNULL);
		break;

	case EVENT_DEAUTHENTICATED:
		reason_code =
			*(t_u16 *) (pmbuf->pbuf + pmbuf->data_offset +
				    sizeof(eventcause));
		PRINTM(MMSG, "wlan: EVENT: Deauthenticated (reason 0x%x)\n",
		       reason_code);
		pmadapter->dbg.num_event_deauth++;
		wlan_handle_disconnect_event(pmpriv);

		break;

	case EVENT_DISASSOCIATED:
		reason_code =
			*(t_u16 *) (pmbuf->pbuf + pmbuf->data_offset +
				    sizeof(eventcause));
		PRINTM(MMSG, "wlan: EVENT: Disassociated (reason 0x%x)\n",
		       reason_code);
		pmadapter->dbg.num_event_disassoc++;
		wlan_handle_disconnect_event(pmpriv);
		break;

	case EVENT_LINK_LOST:
		reason_code =
			*(t_u16 *) (pmbuf->pbuf + pmbuf->data_offset +
				    sizeof(eventcause));
		PRINTM(MMSG, "wlan: EVENT: Link lost (reason 0x%x)\n",
		       reason_code);
		pmadapter->dbg.num_event_link_lost++;
		wlan_handle_disconnect_event(pmpriv);
		break;

	case EVENT_PS_SLEEP:
		PRINTM(MINFO, "EVENT: SLEEP\n");
		PRINTM(MEVENT, "_");

		/* Handle unexpected PS SLEEP event */
		if (pmadapter->ps_state == PS_STATE_SLEEP_CFM)
			break;
		pmadapter->ps_state = PS_STATE_PRE_SLEEP;

		wlan_check_ps_cond(pmadapter);
		break;

	case EVENT_PS_AWAKE:
		PRINTM(MINFO, "EVENT: AWAKE \n");
		PRINTM(MEVENT, "|");
		if (!pmadapter->pps_uapsd_mode &&
		    pmpriv->media_connected &&
		    (pmpriv->port_open || !pmpriv->port_ctrl_mode) &&
		    pmadapter->sleep_period.period) {
			pmadapter->pps_uapsd_mode = MTRUE;
			PRINTM(MEVENT, "PPS/UAPSD mode activated\n");
		}
		/* Handle unexpected PS AWAKE event */
		if (pmadapter->ps_state == PS_STATE_SLEEP_CFM)
			break;
		pmadapter->tx_lock_flag = MFALSE;
		if (pmadapter->pps_uapsd_mode && pmadapter->gen_null_pkt) {
			if (MTRUE == wlan_check_last_packet_indication(pmpriv)) {
				if (!pmadapter->data_sent) {
					if (wlan_send_null_packet(pmpriv,
								  MRVDRV_TxPD_POWER_MGMT_NULL_PACKET
								  |
								  MRVDRV_TxPD_POWER_MGMT_LAST_PACKET)
					    == MLAN_STATUS_SUCCESS) {
						LEAVE();
						return MLAN_STATUS_SUCCESS;
					}
				}
			}
		}
		pmadapter->ps_state = PS_STATE_AWAKE;
		pmadapter->pm_wakeup_card_req = MFALSE;
		pmadapter->pm_wakeup_fw_try = MFALSE;
		break;

	case EVENT_HS_ACT_REQ:
		PRINTM(MEVENT, "EVENT: HS_ACT_REQ\n");
		ret = wlan_prepare_cmd(priv,
				       HostCmd_CMD_802_11_HS_CFG_ENH,
				       0, 0, MNULL, MNULL);
		break;

	case EVENT_MIC_ERR_UNICAST:
		PRINTM(MEVENT, "EVENT: UNICAST MIC ERROR\n");
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_MIC_ERR_UNI, MNULL);
		break;

	case EVENT_MIC_ERR_MULTICAST:
		PRINTM(MEVENT, "EVENT: MULTICAST MIC ERROR\n");
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_MIC_ERR_MUL, MNULL);
		break;
	case EVENT_MIB_CHANGED:
	case EVENT_INIT_DONE:
		break;

	case EVENT_ADHOC_BCN_LOST:
		PRINTM(MEVENT, "EVENT: ADHOC_BCN_LOST\n");
		pmpriv->adhoc_is_link_sensed = MFALSE;
		wlan_clean_txrx(pmpriv);
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_ADHOC_LINK_LOST,
				MNULL);
		break;

	case EVENT_FW_DEBUG_INFO:
		/* Allocate memory for event buffer */
		ret = pcb->moal_malloc(pmadapter->pmoal_handle,
				       MAX_EVENT_SIZE, MLAN_MEM_DEF, &evt_buf);
		if ((ret == MLAN_STATUS_SUCCESS) && evt_buf) {
			pevent = (pmlan_event) evt_buf;
			pevent->bss_index = pmpriv->bss_index;
			PRINTM(MEVENT, "EVENT: FW Debug Info\n");
			pevent->event_id = MLAN_EVENT_ID_FW_DEBUG_INFO;
			pevent->event_len =
				pmbuf->data_len - sizeof(eventcause);
			memcpy(pmadapter, (t_u8 *) pevent->event_buf,
			       pmbuf->pbuf + pmbuf->data_offset +
			       sizeof(eventcause), pevent->event_len);
			wlan_recv_event(pmpriv, pevent->event_id, pevent);
			pcb->moal_mfree(pmadapter->pmoal_handle, evt_buf);
		}
		break;

	case EVENT_BG_SCAN_REPORT:
		PRINTM(MEVENT, "EVENT: BGS_REPORT\n");
		pmadapter->bgscan_reported = MTRUE;
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_BG_SCAN, MNULL);
		break;
	case EVENT_BG_SCAN_STOPPED:
		PRINTM(MEVENT, "EVENT: BGS_STOPPED\n");
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_BG_SCAN_STOPPED,
				MNULL);
		break;

	case EVENT_PORT_RELEASE:
		PRINTM(MEVENT, "EVENT: PORT RELEASE\n");
		/* Open the port for e-supp mode */
		if (pmpriv->port_ctrl_mode == MTRUE) {
			PRINTM(MINFO, "PORT_REL: port_status = OPEN\n");
			pmpriv->port_open = MTRUE;
		}
		pmadapter->scan_block = MFALSE;
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_PORT_RELEASE, MNULL);
		break;

	case EVENT_STOP_TX:
		PRINTM(MEVENT, "EVENT: Stop Tx (%#x)\n", eventcause);
		wlan_11h_tx_disable(pmpriv);	// this fn will send event up
						// to MOAL
		break;
	case EVENT_START_TX:
		PRINTM(MEVENT, "EVENT: Start Tx (%#x)\n", eventcause);
		wlan_11h_tx_enable(pmpriv);	// this fn will send event up
						// to MOAL
		break;
	case EVENT_CHANNEL_SWITCH:
		PRINTM(MEVENT, "EVENT: Channel Switch (%#x)\n", eventcause);
		/* To be handled for 'chanswann' private command */
		break;
	case EVENT_CHANNEL_SWITCH_ANN:
		PRINTM(MEVENT, "EVENT: Channel Switch Announcement\n");
		/* Here, pass up event first, as handling will send deauth */
		wlan_recv_event(pmpriv,
				MLAN_EVENT_ID_FW_CHANNEL_SWITCH_ANN, MNULL);
		wlan_11h_handle_event_chanswann(pmpriv);
		break;
	case EVENT_RADAR_DETECTED:
		PRINTM(MEVENT, "EVENT: Radar Detected\n");

		/* Send as passthru first, this event can cause other events */
		memset(pmadapter, pevent, 0x00, sizeof(event_buf));
		pevent->bss_index = pmpriv->bss_index;
		pevent->event_id = MLAN_EVENT_ID_DRV_PASSTHRU;
		pevent->event_len = pmbuf->data_len;
		memcpy(pmadapter, (t_u8 *) pevent->event_buf,
		       pmbuf->pbuf + pmbuf->data_offset, pevent->event_len);
		wlan_recv_event(pmpriv, pevent->event_id, pevent);

		if (pmadapter->state_rdh.stage == RDH_OFF) {
			pmadapter->state_rdh.stage = RDH_CHK_INTFS;
			wlan_11h_radar_detected_handling(pmadapter);
		} else {
			PRINTM(MEVENT, "Ignore Event Radar Detected - handling"
			       " already in progress.\n");
		}

		break;

	case EVENT_CHANNEL_REPORT_RDY:
		PRINTM(MEVENT, "EVENT: Channel Report Ready\n");
		/* Allocate memory for event buffer */
		ret = pcb->moal_malloc(pmadapter->pmoal_handle,
				       MAX_EVENT_SIZE, MLAN_MEM_DEF, &evt_buf);
		if ((ret == MLAN_STATUS_SUCCESS) && evt_buf) {
			memset(pmadapter, evt_buf, 0x00, MAX_EVENT_SIZE);
			/* Setup event buffer */
			pevent = (pmlan_event) evt_buf;
			pevent->bss_index = pmpriv->bss_index;
			pevent->event_id = MLAN_EVENT_ID_FW_CHANNEL_REPORT_RDY;
			pevent->event_len =
				pmbuf->data_len - sizeof(eventcause);
			/* Copy event data */
			memcpy(pmadapter, (t_u8 *) pevent->event_buf,
			       pmbuf->pbuf + pmbuf->data_offset +
			       sizeof(eventcause), pevent->event_len);
			/* Handle / pass event data */
			ret = wlan_11h_handle_event_chanrpt_ready(pmpriv,
								  pevent);

			/* Also send this event as passthru */
			pevent->event_id = MLAN_EVENT_ID_DRV_PASSTHRU;
			pevent->event_len = pmbuf->data_len;
			memcpy(pmadapter, (t_u8 *) pevent->event_buf,
			       pmbuf->pbuf + pmbuf->data_offset,
			       pevent->event_len);
			wlan_recv_event(pmpriv, pevent->event_id, pevent);
			/* Now done with buffer */
			pcb->moal_mfree(pmadapter->pmoal_handle, evt_buf);
		}
		/* Send up this Event to unblock MOAL waitqueue */
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_DRV_MEAS_REPORT, MNULL);
		break;
	case EVENT_EXT_SCAN_REPORT:
		PRINTM(MEVENT, "EVENT: EXT_SCAN Report (%#x)\n", eventcause);
		if (pmadapter->pext_scan_ioctl_req)
			ret = wlan_handle_event_ext_scan_report(priv, pmbuf);
		break;
	case EVENT_MEAS_REPORT_RDY:
		PRINTM(MEVENT, "EVENT: Measurement Report Ready (%#x)\n",
		       eventcause);
		ret = wlan_prepare_cmd(priv, HostCmd_CMD_MEASUREMENT_REPORT,
				       HostCmd_ACT_GEN_SET, 0, 0, MNULL);
		break;
	case EVENT_WMM_STATUS_CHANGE:
		if (pmbuf && pmbuf->data_len
		    > sizeof(eventcause) + sizeof(MrvlIEtypesHeader_t)) {
			PRINTM(MEVENT, "EVENT: WMM status changed: %d\n",
			       pmbuf->data_len);

			evt_buf = (pmbuf->pbuf
				   + pmbuf->data_offset + sizeof(eventcause));

			wlan_ret_wmm_get_status(pmpriv,
						evt_buf,
						pmbuf->data_len -
						sizeof(eventcause));
		} else {
			PRINTM(MEVENT, "EVENT: WMM status changed\n");
			ret = wlan_cmd_wmm_status_change(pmpriv);
		}
		break;

	case EVENT_RSSI_LOW:
		PRINTM(MEVENT, "EVENT: Beacon RSSI_LOW\n");
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_BCN_RSSI_LOW, MNULL);
		break;
	case EVENT_SNR_LOW:
		PRINTM(MEVENT, "EVENT: Beacon SNR_LOW\n");
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_BCN_SNR_LOW, MNULL);
		break;
	case EVENT_MAX_FAIL:
		PRINTM(MEVENT, "EVENT: MAX_FAIL\n");
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_MAX_FAIL, MNULL);
		break;
	case EVENT_RSSI_HIGH:
		PRINTM(MEVENT, "EVENT: Beacon RSSI_HIGH\n");
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_BCN_RSSI_HIGH, MNULL);
		break;
	case EVENT_SNR_HIGH:
		PRINTM(MEVENT, "EVENT: Beacon SNR_HIGH\n");
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_BCN_SNR_HIGH, MNULL);
		break;
	case EVENT_DATA_RSSI_LOW:
		PRINTM(MEVENT, "EVENT: Data RSSI_LOW\n");
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_DATA_RSSI_LOW, MNULL);
		break;
	case EVENT_DATA_SNR_LOW:
		PRINTM(MEVENT, "EVENT: Data SNR_LOW\n");
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_DATA_SNR_LOW, MNULL);
		break;
	case EVENT_DATA_RSSI_HIGH:
		PRINTM(MEVENT, "EVENT: Data RSSI_HIGH\n");
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_DATA_RSSI_HIGH, MNULL);
		break;
	case EVENT_DATA_SNR_HIGH:
		PRINTM(MEVENT, "EVENT: Data SNR_HIGH\n");
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_DATA_SNR_HIGH, MNULL);
		break;
	case EVENT_LINK_QUALITY:
		PRINTM(MEVENT, "EVENT: Link Quality\n");
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_LINK_QUALITY, MNULL);
		break;
	case EVENT_PRE_BEACON_LOST:
		PRINTM(MEVENT, "EVENT: Pre-Beacon Lost\n");
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_PRE_BCN_LOST, MNULL);
		break;
	case EVENT_IBSS_COALESCED:
		PRINTM(MEVENT, "EVENT: IBSS_COALESCED\n");
		ret = wlan_prepare_cmd(pmpriv,
				       HostCmd_CMD_802_11_IBSS_COALESCING_STATUS,
				       HostCmd_ACT_GEN_GET, 0, MNULL, MNULL);
		break;
	case EVENT_ADDBA:
		PRINTM(MEVENT, "EVENT: ADDBA Request\n");
		if (pmpriv->media_connected == MTRUE)
			ret = wlan_prepare_cmd(pmpriv,
					       HostCmd_CMD_11N_ADDBA_RSP,
					       HostCmd_ACT_GEN_SET, 0, MNULL,
					       pmadapter->event_body);
		else
			PRINTM(MERROR,
			       "Ignore ADDBA Request event in disconnected state\n");
		break;
	case EVENT_DELBA:
		PRINTM(MEVENT, "EVENT: DELBA Request\n");
		if (pmpriv->media_connected == MTRUE)
			wlan_11n_delete_bastream(pmpriv, pmadapter->event_body);
		else
			PRINTM(MERROR,
			       "Ignore DELBA Request event in disconnected state\n");
		break;
	case EVENT_BA_STREAM_TIMEOUT:
		PRINTM(MEVENT, "EVENT:  BA Stream timeout\n");
		if (pmpriv->media_connected == MTRUE)
			wlan_11n_ba_stream_timeout(pmpriv,
						   (HostCmd_DS_11N_BATIMEOUT *)
						   pmadapter->event_body);
		else
			PRINTM(MERROR,
			       "Ignore BA Stream timeout event in disconnected state\n");
		break;
	case EVENT_RXBA_SYNC:
		PRINTM(MEVENT, "EVENT:  RXBA_SYNC\n");
		wlan_11n_rxba_sync_event(pmpriv, pmadapter->event_body,
					 pmbuf->data_len - sizeof(eventcause));
		break;
	case EVENT_AMSDU_AGGR_CTRL:
		PRINTM(MEVENT, "EVENT:  AMSDU_AGGR_CTRL %d\n",
		       *(t_u16 *) pmadapter->event_body);
		pmadapter->tx_buf_size =
			MIN(pmadapter->curr_tx_buf_size,
			    wlan_le16_to_cpu(*(t_u16 *) pmadapter->event_body));
		PRINTM(MEVENT, "tx_buf_size %d\n", pmadapter->tx_buf_size);
		break;

	case EVENT_WEP_ICV_ERR:
		PRINTM(MEVENT, "EVENT: WEP ICV error\n");
		pevent->bss_index = pmpriv->bss_index;
		pevent->event_id = MLAN_EVENT_ID_FW_WEP_ICV_ERR;
		pevent->event_len = sizeof(Event_WEP_ICV_ERR);
		memcpy(pmadapter, (t_u8 *) pevent->event_buf,
		       pmadapter->event_body, pevent->event_len);
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_WEP_ICV_ERR, pevent);
		break;

	case EVENT_BW_CHANGE:
		PRINTM(MEVENT, "EVENT: BW Change\n");
		pevent->bss_index = pmpriv->bss_index;
		pevent->event_id = MLAN_EVENT_ID_FW_BW_CHANGED;
		pevent->event_len = sizeof(t_u8);
		/* Copy event body from the event buffer */
		memcpy(pmadapter, (t_u8 *) pevent->event_buf,
		       pmadapter->event_body, pevent->event_len);
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_BW_CHANGED, pevent);
		break;

#ifdef WIFI_DIRECT_SUPPORT
	case EVENT_WIFIDIRECT_GENERIC_EVENT:
		PRINTM(MEVENT, "EVENT: WIFIDIRECT event %d\n", eventcause);
		/* Allocate memory for event buffer */
		ret = pcb->moal_malloc(pmadapter->pmoal_handle, MAX_EVENT_SIZE,
				       MLAN_MEM_DEF, &evt_buf);
		if ((ret == MLAN_STATUS_SUCCESS) && evt_buf) {
			pevent = (pmlan_event) evt_buf;
			pevent->bss_index = pmpriv->bss_index;
			pevent->event_id = MLAN_EVENT_ID_DRV_PASSTHRU;
			pevent->event_len = pmbuf->data_len;
			memcpy(pmadapter, (t_u8 *) pevent->event_buf,
			       pmbuf->pbuf + pmbuf->data_offset,
			       pevent->event_len);
			wlan_recv_event(pmpriv, pevent->event_id, pevent);
			pcb->moal_mfree(pmadapter->pmoal_handle, evt_buf);
		}
		break;
	case EVENT_WIFIDIRECT_SERVICE_DISCOVERY:
		PRINTM(MEVENT, "EVENT: WIFIDIRECT service discovery event %d\n",
		       eventcause);
		/* Allocate large memory for service discovery */
		if (pmbuf->data_len < MAX_EVENT_SIZE)
			ret = pcb->moal_malloc(pmadapter->pmoal_handle,
					       MAX_EVENT_SIZE, MLAN_MEM_DEF,
					       &evt_buf);
		else
			ret = pcb->moal_malloc(pmadapter->pmoal_handle,
					       MAX_EVENT_SIZE * 2, MLAN_MEM_DEF,
					       &evt_buf);
		if ((ret == MLAN_STATUS_SUCCESS) && evt_buf) {
			pevent = (pmlan_event) evt_buf;
			pevent->bss_index = pmpriv->bss_index;
			pevent->event_id = MLAN_EVENT_ID_DRV_PASSTHRU;
			pevent->event_len = pmbuf->data_len;
			memcpy(pmadapter, (t_u8 *) pevent->event_buf,
			       pmbuf->pbuf + pmbuf->data_offset,
			       pevent->event_len);
			wlan_recv_event(pmpriv, pevent->event_id, pevent);
			pcb->moal_mfree(pmadapter->pmoal_handle, evt_buf);
		}
		break;
	case EVENT_REMAIN_ON_CHANNEL_EXPIRED:
		PRINTM(MEVENT, "EVENT: REMAIN_ON_CHANNEL_EXPIRED reason=%d\n",
		       *(t_u16 *) pmadapter->event_body);
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_REMAIN_ON_CHAN_EXPIRED,
				MNULL);
		break;
#endif /* WIFI_DIRECT_SUPPORT */

	case EVENT_SAD_REPORT:
		{
			t_u8 *pEvtDat =
				pmbuf->pbuf + pmbuf->data_offset +
				sizeof(t_u32);
			PRINTM(MEVENT,
			       "EVENT: Antenna Diversity %d  (%d, %d, %d, %d)\n",
			       eventcause, pEvtDat[0] + 1, pEvtDat[1] + 1,
			       pEvtDat[2], pEvtDat[3]);
		}
		break;

	default:
		PRINTM(MEVENT, "EVENT: unknown event id: %#x\n", eventcause);
		wlan_recv_event(pmpriv, MLAN_EVENT_ID_FW_UNKNOWN, MNULL);
		break;
	}

	LEAVE();
	return ret;
}
