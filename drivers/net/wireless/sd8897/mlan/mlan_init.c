/** @file mlan_init.c
 *
 *  @brief This file contains the initialization for FW
 *  and HW.
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
#ifdef STA_SUPPORT
#include "mlan_join.h"
#endif
#include "mlan_util.h"
#include "mlan_fw.h"
#include "mlan_main.h"
#include "mlan_init.h"
#include "mlan_wmm.h"
#include "mlan_11n.h"
#include "mlan_11ac.h"
#include "mlan_11h.h"
#include "mlan_meas.h"
#include "mlan_sdio.h"

/********************************************************
        Global Variables
********************************************************/

/********************************************************
        Local Functions
********************************************************/

/**
 *  @brief This function adds a BSS priority table
 *
 *  @param priv		A pointer to mlan_private structure
 *
 *  @return		MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
static mlan_status
wlan_add_bsspriotbl(pmlan_private priv)
{
	pmlan_adapter pmadapter = priv->adapter;
	mlan_bssprio_node *pbssprio = MNULL;
	mlan_status status = MLAN_STATUS_SUCCESS;

	ENTER();

	if ((status =
	     pmadapter->callbacks.moal_malloc(pmadapter->pmoal_handle,
					      sizeof(mlan_bssprio_node),
					      MLAN_MEM_DEF,
					      (t_u8 **) & pbssprio))) {
		PRINTM(MERROR, "Failed to allocate bsspriotbl\n");
		LEAVE();
		return status;
	}

	pbssprio->priv = priv;

	util_init_list((pmlan_linked_list) pbssprio);

	if (!pmadapter->bssprio_tbl[priv->bss_priority].bssprio_cur)
		pmadapter->bssprio_tbl[priv->bss_priority].bssprio_cur
			= pbssprio;

	util_enqueue_list_tail(pmadapter->pmoal_handle,
			       &pmadapter->bssprio_tbl[priv->
						       bss_priority].bssprio_head,
			       (pmlan_linked_list) pbssprio,
			       pmadapter->callbacks.moal_spin_lock,
			       pmadapter->callbacks.moal_spin_unlock);

	LEAVE();
	return status;
}

/**
 *  @brief This function deletes the BSS priority table
 *
 *  @param priv		A pointer to mlan_private structure
 *
 *  @return		N/A
 */
static t_void
wlan_delete_bsspriotbl(pmlan_private priv)
{
	int i;
	pmlan_adapter pmadapter = priv->adapter;
	mlan_bssprio_node *pbssprio_node = MNULL, *ptmp_node = MNULL, **ppcur =
		MNULL;
	pmlan_list_head phead;

	ENTER();

	for (i = 0; i < pmadapter->priv_num; ++i) {
		phead = &pmadapter->bssprio_tbl[i].bssprio_head;
		ppcur = &pmadapter->bssprio_tbl[i].bssprio_cur;
		PRINTM(MINFO,
		       "Delete BSS priority table, index = %d, i = %d, phead = %p, pcur = %p\n",
		       priv->bss_index, i, phead, *ppcur);
		if (*ppcur) {
			pbssprio_node =
				(mlan_bssprio_node *) util_peek_list(pmadapter->
								     pmoal_handle,
								     phead,
								     pmadapter->
								     callbacks.
								     moal_spin_lock,
								     pmadapter->
								     callbacks.
								     moal_spin_unlock);
			while (pbssprio_node &&
			       ((pmlan_list_head) pbssprio_node != phead)) {
				ptmp_node = pbssprio_node->pnext;
				if (pbssprio_node->priv == priv) {
					PRINTM(MINFO,
					       "Delete node, pnode = %p, pnext = %p\n",
					       pbssprio_node, ptmp_node);
					util_unlink_list(pmadapter->
							 pmoal_handle, phead,
							 (pmlan_linked_list)
							 pbssprio_node,
							 pmadapter->callbacks.
							 moal_spin_lock,
							 pmadapter->callbacks.
							 moal_spin_unlock);
					pmadapter->callbacks.
						moal_mfree(pmadapter->
							   pmoal_handle,
							   (t_u8 *)
							   pbssprio_node);
				}
				pbssprio_node = ptmp_node;
			}
			*ppcur = (mlan_bssprio_node *) phead;
		}
	}

	LEAVE();
}

/********************************************************
        Global Functions
********************************************************/

/**
 *  @brief This function allocates buffer for the members of adapter
 *  		structure like command buffer and BSSID list.
 *
 *  @param pmadapter A pointer to mlan_adapter structure
 *
 *  @return        MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
wlan_allocate_adapter(pmlan_adapter pmadapter)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
#ifdef STA_SUPPORT
	t_u32 buf_size;
	BSSDescriptor_t *ptemp_scan_table = MNULL;
#endif

	ENTER();

#ifdef STA_SUPPORT
	/* Allocate buffer to store the BSSID list */
	buf_size = sizeof(BSSDescriptor_t) * MRVDRV_MAX_BSSID_LIST;
	if (pmadapter->callbacks.moal_vmalloc &&
	    pmadapter->callbacks.moal_vfree)
		ret = pmadapter->callbacks.moal_vmalloc(pmadapter->pmoal_handle,
							buf_size,
							(t_u8 **) &
							ptemp_scan_table);
	else
		ret = pmadapter->callbacks.moal_malloc(pmadapter->pmoal_handle,
						       buf_size, MLAN_MEM_DEF,
						       (t_u8 **) &
						       ptemp_scan_table);
	if (ret != MLAN_STATUS_SUCCESS || !ptemp_scan_table) {
		PRINTM(MERROR, "Failed to allocate scan table\n");
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}
	pmadapter->pscan_table = ptemp_scan_table;
	ret = pmadapter->callbacks.moal_malloc(pmadapter->pmoal_handle,
					       DEFAULT_SCAN_BEACON_BUFFER,
					       MLAN_MEM_DEF,
					       (t_u8 **) & pmadapter->bcn_buf);
	if (ret != MLAN_STATUS_SUCCESS || !pmadapter->bcn_buf) {
		PRINTM(MERROR, "Failed to allocate bcn buf\n");
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}
	pmadapter->bcn_buf_size = DEFAULT_SCAN_BEACON_BUFFER;
#endif

	/* Allocate command buffer */
	ret = wlan_alloc_cmd_buffer(pmadapter);
	if (ret != MLAN_STATUS_SUCCESS) {
		PRINTM(MERROR, "Failed to allocate command buffer\n");
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}

	ret = pmadapter->callbacks.moal_malloc(pmadapter->pmoal_handle,
					       MAX_MP_REGS + DMA_ALIGNMENT,
					       MLAN_MEM_DEF | MLAN_MEM_DMA,
					       (t_u8 **) & pmadapter->
					       mp_regs_buf);
	if (ret != MLAN_STATUS_SUCCESS || !pmadapter->mp_regs_buf) {
		PRINTM(MERROR, "Failed to allocate mp_regs_buf\n");
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}
	pmadapter->mp_regs =
		(t_u8 *) ALIGN_ADDR(pmadapter->mp_regs_buf, DMA_ALIGNMENT);

#if defined(SDIO_MULTI_PORT_TX_AGGR) || defined(SDIO_MULTI_PORT_RX_AGGR)
	ret = wlan_alloc_sdio_mpa_buffers(pmadapter,
					  SDIO_MP_TX_AGGR_DEF_BUF_SIZE,
					  SDIO_MP_RX_AGGR_DEF_BUF_SIZE);
	if (ret != MLAN_STATUS_SUCCESS) {
		PRINTM(MERROR, "Failed to allocate sdio mp-a buffers\n");
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}
#endif

	pmadapter->psleep_cfm =
		wlan_alloc_mlan_buffer(pmadapter,
				       sizeof(opt_sleep_confirm_buffer), 0,
				       MOAL_MALLOC_BUFFER);

	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function initializes the private structure
 *  		and sets default values to the members of mlan_private.
 *
 *  @param priv    A pointer to mlan_private structure
 *
 *  @return        MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
wlan_init_priv(pmlan_private priv)
{
	t_u32 i;
	pmlan_adapter pmadapter = priv->adapter;
	mlan_status ret = MLAN_STATUS_SUCCESS;

	ENTER();

	priv->media_connected = MFALSE;
	memset(pmadapter, priv->curr_addr, 0xff, MLAN_MAC_ADDR_LENGTH);

#ifdef STA_SUPPORT
	priv->pkt_tx_ctrl = 0;
	priv->bss_mode = MLAN_BSS_MODE_INFRA;
	priv->data_rate = 0;	/* Initially indicate the rate as auto */
	priv->is_data_rate_auto = MTRUE;
	priv->bcn_avg_factor = DEFAULT_BCN_AVG_FACTOR;
	priv->data_avg_factor = DEFAULT_DATA_AVG_FACTOR;

	priv->sec_info.wep_status = Wlan802_11WEPDisabled;
	priv->sec_info.authentication_mode = MLAN_AUTH_MODE_AUTO;
	priv->sec_info.encryption_mode = MLAN_ENCRYPTION_MODE_NONE;
	for (i = 0; i < sizeof(priv->wep_key) / sizeof(priv->wep_key[0]); i++)
		memset(pmadapter, &priv->wep_key[i], 0, sizeof(mrvl_wep_key_t));
	priv->wep_key_curr_index = 0;
	priv->ewpa_query = MFALSE;
	priv->adhoc_aes_enabled = MFALSE;
	priv->curr_pkt_filter =
		HostCmd_ACT_MAC_STATIC_DYNAMIC_BW_ENABLE |
		HostCmd_ACT_MAC_RX_ON | HostCmd_ACT_MAC_TX_ON |
		HostCmd_ACT_MAC_ETHERNETII_ENABLE;

	priv->beacon_period = MLAN_BEACON_INTERVAL;
	priv->pattempted_bss_desc = MNULL;
	memset(pmadapter, &priv->curr_bss_params, 0,
	       sizeof(priv->curr_bss_params));
	priv->listen_interval = MLAN_DEFAULT_LISTEN_INTERVAL;

	memset(pmadapter, &priv->assoc_rsp_buf, 0, sizeof(priv->assoc_rsp_buf));
	priv->assoc_rsp_size = 0;

	wlan_11d_priv_init(priv);
	wlan_11h_priv_init(priv);
#ifdef UAP_SUPPORT
	priv->uap_bss_started = MFALSE;
	memset(pmadapter, &priv->uap_state_chan_cb, 0,
	       sizeof(priv->uap_state_chan_cb));
#endif
#if defined(UAP_SUPPORT)
	priv->num_drop_pkts = 0;
#endif
#if defined(STA_SUPPORT)
	priv->adhoc_state_prev = ADHOC_IDLE;
	memset(pmadapter, &priv->adhoc_last_start_ssid, 0,
	       sizeof(priv->adhoc_last_start_ssid));
#endif
	priv->adhoc_channel = DEFAULT_AD_HOC_CHANNEL;
	priv->atim_window = 0;
	priv->adhoc_state = ADHOC_IDLE;
	priv->tx_power_level = 0;
	priv->max_tx_power_level = 0;
	priv->min_tx_power_level = 0;
	priv->tx_rate = 0;
	priv->rxpd_rate_info = 0;
	/* refer to V15 CMD_TX_RATE_QUERY */
	priv->rxpd_vhtinfo = 0;
	priv->rxpd_rate = 0;
	priv->rate_bitmap = 0;
	priv->data_rssi_last = 0;
	priv->data_rssi_avg = 0;
	priv->data_nf_avg = 0;
	priv->data_nf_last = 0;
	priv->bcn_rssi_last = 0;
	priv->bcn_rssi_avg = 0;
	priv->bcn_nf_avg = 0;
	priv->bcn_nf_last = 0;

	priv->sec_info.ewpa_enabled = MFALSE;
	priv->sec_info.wpa_enabled = MFALSE;
	priv->sec_info.wpa2_enabled = MFALSE;
	memset(pmadapter, &priv->wpa_ie, 0, sizeof(priv->wpa_ie));
	memset(pmadapter, &priv->aes_key, 0, sizeof(priv->aes_key));
	priv->wpa_ie_len = 0;
	priv->wpa_is_gtk_set = MFALSE;
#if defined(STA_SUPPORT)
	priv->pmfcfg.mfpc = 0;
	priv->pmfcfg.mfpr = 0;
#endif
	priv->sec_info.wapi_enabled = MFALSE;
	priv->wapi_ie_len = 0;
	priv->sec_info.wapi_key_on = MFALSE;

	memset(pmadapter, &priv->wps, 0, sizeof(priv->wps));
	memset(pmadapter, &priv->gen_ie_buf, 0, sizeof(priv->gen_ie_buf));
	priv->gen_ie_buf_len = 0;
#endif /* STA_SUPPORT */

	priv->tx_bf_cap = 0;
	priv->wmm_required = MTRUE;
	priv->wmm_enabled = MFALSE;
	priv->wmm_qosinfo = 0;
#ifdef STA_SUPPORT
	priv->pcurr_bcn_buf = MNULL;
	priv->curr_bcn_size = 0;
	memset(pmadapter, &priv->ext_cap, 0, sizeof(priv->ext_cap));
	SET_EXTCAP_OPERMODENTF(priv->ext_cap);
#endif /* STA_SUPPORT */

	for (i = 0; i < MAX_NUM_TID; i++)
		priv->addba_reject[i] = ADDBA_RSP_STATUS_ACCEPT;
	priv->max_amsdu = 0;

	if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_STA) {
		priv->port_ctrl_mode = MTRUE;
	} else {
		priv->port_ctrl_mode = MFALSE;
	}
	priv->port_open = MFALSE;

	ret = wlan_add_bsspriotbl(priv);

	LEAVE();
	return ret;
}

/**
 *  @brief This function initializes the adapter structure
 *  		and sets default values to the members of adapter.
 *
 *  @param pmadapter	A pointer to mlan_adapter structure
 *
 *  @return		N/A
 */
t_void
wlan_init_adapter(pmlan_adapter pmadapter)
{
	opt_sleep_confirm_buffer *sleep_cfm_buf = MNULL;
	ENTER();

	sleep_cfm_buf =
		(opt_sleep_confirm_buffer *) (pmadapter->psleep_cfm->pbuf +
					      pmadapter->psleep_cfm->
					      data_offset);

#ifdef MFG_CMD_SUPPORT
	if (pmadapter->init_para.mfg_mode == MLAN_INIT_PARA_DISABLED) {
		pmadapter->mfg_mode = MFALSE;
	} else {
		pmadapter->mfg_mode = pmadapter->init_para.mfg_mode;
	}
#endif

	pmadapter->int_mode = pmadapter->init_para.int_mode;
	pmadapter->gpio_pin = pmadapter->init_para.gpio_pin;

#if defined(STA_SUPPORT)
	pmadapter->pwarm_reset_ioctl_req = MNULL;
#endif
	pmadapter->cmd_sent = MFALSE;
	pmadapter->data_sent = MTRUE;
	pmadapter->mp_rd_bitmap = 0;
	pmadapter->mp_wr_bitmap = 0;
	pmadapter->curr_rd_port = 0;
	pmadapter->curr_wr_port = 0;
	pmadapter->mp_data_port_mask = DATA_PORT_MASK;

#ifdef SDIO_MULTI_PORT_TX_AGGR
	pmadapter->mpa_tx.buf_len = 0;
	pmadapter->mpa_tx.pkt_cnt = 0;
	pmadapter->mpa_tx.start_port = 0;

	if (!pmadapter->init_para.mpa_tx_cfg) {
		pmadapter->mpa_tx.enabled = MFALSE;
	} else if (pmadapter->init_para.mpa_tx_cfg == MLAN_INIT_PARA_DISABLED) {
		pmadapter->mpa_tx.enabled = MFALSE;
	} else {
		pmadapter->mpa_tx.enabled = MTRUE;
	}
	pmadapter->mpa_tx.pkt_aggr_limit = SDIO_MP_AGGR_DEF_PKT_LIMIT;
#endif /* SDIO_MULTI_PORT_TX_AGGR */

#ifdef SDIO_MULTI_PORT_RX_AGGR
	pmadapter->mpa_rx.buf_len = 0;
	pmadapter->mpa_rx.pkt_cnt = 0;
	pmadapter->mpa_rx.start_port = 0;

	if (!pmadapter->init_para.mpa_rx_cfg) {
		pmadapter->mpa_rx.enabled = MFALSE;
	} else if (pmadapter->init_para.mpa_rx_cfg == MLAN_INIT_PARA_DISABLED) {
		pmadapter->mpa_rx.enabled = MFALSE;
	} else {
		pmadapter->mpa_rx.enabled = MTRUE;
	}
	pmadapter->mpa_rx.pkt_aggr_limit = SDIO_MP_AGGR_DEF_PKT_LIMIT;
#endif /* SDIO_MULTI_PORT_RX_AGGR */

	pmadapter->cmd_resp_received = MFALSE;
	pmadapter->event_received = MFALSE;
	pmadapter->data_received = MFALSE;

	pmadapter->cmd_timer_is_set = MFALSE;

	/* PnP and power profile */
	pmadapter->surprise_removed = MFALSE;

	/* Status variables */
	pmadapter->hw_status = WlanHardwareStatusInitializing;

	if (!pmadapter->init_para.ps_mode) {
		pmadapter->ps_mode = DEFAULT_PS_MODE;
	} else if (pmadapter->init_para.ps_mode == MLAN_INIT_PARA_DISABLED) {
		pmadapter->ps_mode = Wlan802_11PowerModeCAM;
	} else {
		pmadapter->ps_mode = Wlan802_11PowerModePSP;
	}
	pmadapter->ps_state = PS_STATE_AWAKE;
	pmadapter->need_to_wakeup = MFALSE;

#ifdef STA_SUPPORT
	pmadapter->scan_block = MFALSE;
	/* Scan type */
	pmadapter->scan_type = MLAN_SCAN_TYPE_ACTIVE;
	/* Scan mode */
	pmadapter->scan_mode = HostCmd_BSS_MODE_ANY;
	/* Scan time */
	pmadapter->specific_scan_time = MRVDRV_SPECIFIC_SCAN_CHAN_TIME;
	pmadapter->active_scan_time = MRVDRV_ACTIVE_SCAN_CHAN_TIME;
	pmadapter->passive_scan_time = MRVDRV_PASSIVE_SCAN_CHAN_TIME;

	pmadapter->num_in_scan_table = 0;
	memset(pmadapter, pmadapter->pscan_table, 0,
	       (sizeof(BSSDescriptor_t) * MRVDRV_MAX_BSSID_LIST));
	pmadapter->ext_scan = 0;
	pmadapter->scan_probes = DEFAULT_PROBES;

	memset(pmadapter, pmadapter->bcn_buf, 0, pmadapter->bcn_buf_size);
	pmadapter->pbcn_buf_end = pmadapter->bcn_buf;

	pmadapter->radio_on = RADIO_ON;
	pmadapter->multiple_dtim = MRVDRV_DEFAULT_MULTIPLE_DTIM;

	pmadapter->local_listen_interval = 0;	/* default value in firmware
						   will be used */
#endif /* STA_SUPPORT */

	pmadapter->is_deep_sleep = MFALSE;
	pmadapter->idle_time = DEEP_SLEEP_IDLE_TIME;
	if (!pmadapter->init_para.auto_ds) {
		pmadapter->init_auto_ds = DEFAULT_AUTO_DS_MODE;
	} else if (pmadapter->init_para.auto_ds == MLAN_INIT_PARA_DISABLED) {
		pmadapter->init_auto_ds = MFALSE;
	} else {
		pmadapter->init_auto_ds = MTRUE;
	}

	pmadapter->delay_null_pkt = MFALSE;
	pmadapter->delay_to_ps = DELAY_TO_PS_DEFAULT;
	pmadapter->enhanced_ps_mode = PS_MODE_AUTO;

	pmadapter->gen_null_pkt = MFALSE;	/* Disable NULL Pkt
						   generation-default */
	pmadapter->pps_uapsd_mode = MFALSE;	/* Disable pps/uapsd mode
						   -default */

	pmadapter->pm_wakeup_card_req = MFALSE;

	pmadapter->pm_wakeup_fw_try = MFALSE;

	if (!pmadapter->init_para.max_tx_buf)
		pmadapter->max_tx_buf_size = MLAN_TX_DATA_BUF_SIZE_2K;
	else
		pmadapter->max_tx_buf_size =
			(t_u16) pmadapter->init_para.max_tx_buf;
	pmadapter->tx_buf_size = MLAN_TX_DATA_BUF_SIZE_2K;
	pmadapter->curr_tx_buf_size = MLAN_TX_DATA_BUF_SIZE_2K;

	pmadapter->is_hs_configured = MFALSE;
	pmadapter->hs_cfg.conditions = HOST_SLEEP_DEF_COND;
	pmadapter->hs_cfg.gpio = HOST_SLEEP_DEF_GPIO;
	pmadapter->hs_cfg.gap = HOST_SLEEP_DEF_GAP;
	pmadapter->hs_activated = MFALSE;
	pmadapter->min_wake_holdoff = HOST_SLEEP_DEF_WAKE_HOLDOFF;

	memset(pmadapter, pmadapter->event_body, 0,
	       sizeof(pmadapter->event_body));
	pmadapter->hw_dot_11n_dev_cap = 0;
	pmadapter->hw_dev_mcs_support = 0;
	pmadapter->usr_dot_11n_dev_cap_bg = 0;
	pmadapter->usr_dot_11n_dev_cap_a = 0;
	pmadapter->usr_dev_mcs_support = 0;
#ifdef STA_SUPPORT
	pmadapter->chan_bandwidth = 0;
	pmadapter->adhoc_11n_enabled = MFALSE;
#endif /* STA_SUPPORT */

	pmadapter->hw_dot_11ac_dev_cap = 0;
	pmadapter->hw_dot_11ac_mcs_support = 0;
	pmadapter->usr_dot_11ac_dev_cap_bg = 0;
	pmadapter->usr_dot_11ac_dev_cap_a = 0;
	pmadapter->usr_dot_11ac_mcs_support = 0;

	/* Initialize 802.11d */
	wlan_11d_init(pmadapter);

	wlan_11h_init(pmadapter);

	wlan_wmm_init(pmadapter);
	pmadapter->bypass_pkt_count = 0;
	if (pmadapter->psleep_cfm) {
		pmadapter->psleep_cfm->buf_type = MLAN_BUF_TYPE_CMD;
		pmadapter->psleep_cfm->data_len = sizeof(OPT_Confirm_Sleep);
		memset(pmadapter, &sleep_cfm_buf->ps_cfm_sleep, 0,
		       sizeof(OPT_Confirm_Sleep));
		sleep_cfm_buf->ps_cfm_sleep.command =
			wlan_cpu_to_le16(HostCmd_CMD_802_11_PS_MODE_ENH);
		sleep_cfm_buf->ps_cfm_sleep.size =
			wlan_cpu_to_le16(sizeof(OPT_Confirm_Sleep));
		sleep_cfm_buf->ps_cfm_sleep.result = 0;
		sleep_cfm_buf->ps_cfm_sleep.action =
			wlan_cpu_to_le16(SLEEP_CONFIRM);
		sleep_cfm_buf->ps_cfm_sleep.sleep_cfm.resp_ctrl =
			wlan_cpu_to_le16(RESP_NEEDED);
	}
	memset(pmadapter, &pmadapter->sleep_params, 0,
	       sizeof(pmadapter->sleep_params));
	memset(pmadapter, &pmadapter->sleep_period, 0,
	       sizeof(pmadapter->sleep_period));
	pmadapter->tx_lock_flag = MFALSE;
	pmadapter->null_pkt_interval = 0;
	pmadapter->fw_bands = 0;
	pmadapter->config_bands = 0;
	pmadapter->adhoc_start_band = 0;
	pmadapter->pscan_channels = MNULL;
	pmadapter->fw_release_number = 0;
	pmadapter->fw_cap_info = 0;
	memset(pmadapter, &pmadapter->upld_buf, 0, sizeof(pmadapter->upld_buf));
	pmadapter->upld_len = 0;
	pmadapter->event_cause = 0;
	pmadapter->pmlan_buffer_event = MNULL;
	memset(pmadapter, &pmadapter->region_channel, 0,
	       sizeof(pmadapter->region_channel));
	pmadapter->region_code = 0;
	memcpy(pmadapter, pmadapter->country_code, MRVDRV_DEFAULT_COUNTRY_CODE,
	       COUNTRY_CODE_LEN);
	pmadapter->bcn_miss_time_out = DEFAULT_BCN_MISS_TIMEOUT;
	pmadapter->adhoc_awake_period = 0;
#ifdef STA_SUPPORT
	memset(pmadapter, &pmadapter->arp_filter, 0,
	       sizeof(pmadapter->arp_filter));
	pmadapter->arp_filter_size = 0;
#endif /* STA_SUPPORT */

	LEAVE();
	return;
}

/**
 *  @brief This function intializes the lock variables and
 *  the list heads.
 *
 *  @param pmadapter  A pointer to a mlan_adapter structure
 *
 *  @return           MLAN_STATUS_SUCCESS -- on success,
 *                    otherwise MLAN_STATUS_FAILURE
 *
 */
mlan_status
wlan_init_lock_list(IN pmlan_adapter pmadapter)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	pmlan_private priv = MNULL;
	pmlan_callbacks pcb = &pmadapter->callbacks;
	t_s32 i = 0;
	t_u32 j = 0;

	ENTER();

	if (pcb->moal_init_lock(pmadapter->pmoal_handle, &pmadapter->pmlan_lock)
	    != MLAN_STATUS_SUCCESS) {
		ret = MLAN_STATUS_FAILURE;
		goto error;
	}
	if (pcb->moal_init_lock(pmadapter->pmoal_handle, &pmadapter->pint_lock)
	    != MLAN_STATUS_SUCCESS) {
		ret = MLAN_STATUS_FAILURE;
		goto error;
	}
	if (pcb->
	    moal_init_lock(pmadapter->pmoal_handle, &pmadapter->pmain_proc_lock)
	    != MLAN_STATUS_SUCCESS) {
		ret = MLAN_STATUS_FAILURE;
		goto error;
	}
	if (pcb->
	    moal_init_lock(pmadapter->pmoal_handle, &pmadapter->pmlan_cmd_lock)
	    != MLAN_STATUS_SUCCESS) {
		ret = MLAN_STATUS_FAILURE;
		goto error;
	}
	for (i = 0; i < pmadapter->priv_num; i++) {
		if (pmadapter->priv[i]) {
			priv = pmadapter->priv[i];
			if (pcb->
			    moal_init_lock(pmadapter->pmoal_handle,
					   &priv->rx_pkt_lock)
			    != MLAN_STATUS_SUCCESS) {
				ret = MLAN_STATUS_FAILURE;
				goto error;
			}
			if (pcb->
			    moal_init_lock(pmadapter->pmoal_handle,
					   &priv->wmm.ra_list_spinlock)
			    != MLAN_STATUS_SUCCESS) {
				ret = MLAN_STATUS_FAILURE;
				goto error;
			}
#ifdef STA_SUPPORT
			if (pcb->
			    moal_init_lock(pmadapter->pmoal_handle,
					   &priv->curr_bcn_buf_lock)
			    != MLAN_STATUS_SUCCESS) {
				ret = MLAN_STATUS_FAILURE;
				goto error;
			}
#endif
		}
	}

	/* Initialize cmd_free_q */
	util_init_list_head((t_void *) pmadapter->pmoal_handle,
			    &pmadapter->cmd_free_q, MTRUE,
			    pmadapter->callbacks.moal_init_lock);
	/* Initialize cmd_pending_q */
	util_init_list_head((t_void *) pmadapter->pmoal_handle,
			    &pmadapter->cmd_pending_q, MTRUE,
			    pmadapter->callbacks.moal_init_lock);
	/* Initialize scan_pending_q */
	util_init_list_head((t_void *) pmadapter->pmoal_handle,
			    &pmadapter->scan_pending_q, MTRUE,
			    pmadapter->callbacks.moal_init_lock);

	for (i = 0; i < pmadapter->priv_num; ++i) {
		util_init_list_head((t_void *) pmadapter->pmoal_handle,
				    &pmadapter->bssprio_tbl[i].bssprio_head,
				    MTRUE, pmadapter->callbacks.moal_init_lock);
		pmadapter->bssprio_tbl[i].bssprio_cur = MNULL;
	}

	for (i = 0; i < pmadapter->priv_num; i++) {
		if (pmadapter->priv[i]) {
			priv = pmadapter->priv[i];
			for (j = 0; j < MAX_NUM_TID; ++j) {
				util_init_list_head((t_void *) pmadapter->
						    pmoal_handle,
						    &priv->wmm.tid_tbl_ptr[j].
						    ra_list, MTRUE,
						    priv->adapter->callbacks.
						    moal_init_lock);
			}
			util_init_list_head((t_void *) pmadapter->pmoal_handle,
					    &priv->tx_ba_stream_tbl_ptr, MTRUE,
					    pmadapter->callbacks.
					    moal_init_lock);
			util_init_list_head((t_void *) pmadapter->pmoal_handle,
					    &priv->rx_reorder_tbl_ptr, MTRUE,
					    pmadapter->callbacks.
					    moal_init_lock);
			util_scalar_init((t_void *) pmadapter->pmoal_handle,
					 &priv->wmm.tx_pkts_queued, 0,
					 priv->wmm.ra_list_spinlock,
					 pmadapter->callbacks.moal_init_lock);
			util_scalar_init((t_void *) pmadapter->pmoal_handle,
					 &priv->wmm.highest_queued_prio,
					 HIGH_PRIO_TID,
					 priv->wmm.ra_list_spinlock,
					 pmadapter->callbacks.moal_init_lock);
			util_init_list_head((t_void *) pmadapter->pmoal_handle,
					    &priv->sta_list, MTRUE,
					    pmadapter->callbacks.
					    moal_init_lock);
			/* Initialize bypass_txq */
			util_init_list_head((t_void *) pmadapter->pmoal_handle,
					    &priv->bypass_txq, MTRUE,
					    pmadapter->callbacks.
					    moal_init_lock);
		}
	}

error:
	LEAVE();
	return ret;
}

/**
 *  @brief This function releases the lock variables
 *
 *  @param pmadapter  A pointer to a mlan_adapter structure
 *
 *  @return           None
 *
 */
t_void
wlan_free_lock_list(IN pmlan_adapter pmadapter)
{
	pmlan_private priv = MNULL;
	pmlan_callbacks pcb = &pmadapter->callbacks;
	t_s32 i = 0;
	t_s32 j = 0;

	ENTER();

	if (pmadapter->pmlan_lock)
		pcb->moal_free_lock(pmadapter->pmoal_handle,
				    pmadapter->pmlan_lock);
	if (pmadapter->pint_lock)
		pcb->moal_free_lock(pmadapter->pmoal_handle,
				    pmadapter->pint_lock);
	if (pmadapter->pmain_proc_lock)
		pcb->moal_free_lock(pmadapter->pmoal_handle,
				    pmadapter->pmain_proc_lock);
	if (pmadapter->pmlan_cmd_lock)
		pcb->moal_free_lock(pmadapter->pmoal_handle,
				    pmadapter->pmlan_cmd_lock);

	for (i = 0; i < pmadapter->priv_num; i++) {
		if (pmadapter->priv[i]) {
			priv = pmadapter->priv[i];
			if (priv->rx_pkt_lock)
				pcb->moal_free_lock(pmadapter->pmoal_handle,
						    priv->rx_pkt_lock);
			if (priv->wmm.ra_list_spinlock)
				pcb->moal_free_lock(pmadapter->pmoal_handle,
						    priv->wmm.ra_list_spinlock);
#ifdef STA_SUPPORT
			if (priv->curr_bcn_buf_lock)
				pcb->moal_free_lock(pmadapter->pmoal_handle,
						    priv->curr_bcn_buf_lock);
#endif
		}
	}

	/* Free lists */
	util_free_list_head((t_void *) pmadapter->pmoal_handle,
			    &pmadapter->cmd_free_q,
			    pmadapter->callbacks.moal_free_lock);

	util_free_list_head((t_void *) pmadapter->pmoal_handle,
			    &pmadapter->cmd_pending_q,
			    pmadapter->callbacks.moal_free_lock);

	util_free_list_head((t_void *) pmadapter->pmoal_handle,
			    &pmadapter->scan_pending_q,
			    pmadapter->callbacks.moal_free_lock);

	for (i = 0; i < pmadapter->priv_num; i++)
		util_free_list_head((t_void *) pmadapter->pmoal_handle,
				    &pmadapter->bssprio_tbl[i].bssprio_head,
				    pcb->moal_free_lock);

	for (i = 0; i < pmadapter->priv_num; i++) {
		if (pmadapter->priv[i]) {
			priv = pmadapter->priv[i];
			util_free_list_head((t_void *) pmadapter->pmoal_handle,
					    &priv->sta_list,
					    priv->adapter->callbacks.
					    moal_free_lock);
			util_free_list_head((t_void *) pmadapter->pmoal_handle,
					    &priv->bypass_txq,
					    pmadapter->callbacks.
					    moal_free_lock);

			for (j = 0; j < MAX_NUM_TID; ++j)
				util_free_list_head((t_void *) priv->adapter->
						    pmoal_handle,
						    &priv->wmm.tid_tbl_ptr[j].
						    ra_list,
						    priv->adapter->callbacks.
						    moal_free_lock);
			util_free_list_head((t_void *) priv->adapter->
					    pmoal_handle,
					    &priv->tx_ba_stream_tbl_ptr,
					    priv->adapter->callbacks.
					    moal_free_lock);
			util_free_list_head((t_void *) priv->adapter->
					    pmoal_handle,
					    &priv->rx_reorder_tbl_ptr,
					    priv->adapter->callbacks.
					    moal_free_lock);
			util_scalar_free((t_void *) priv->adapter->pmoal_handle,
					 &priv->wmm.tx_pkts_queued,
					 priv->adapter->callbacks.
					 moal_free_lock);
			util_scalar_free((t_void *) priv->adapter->pmoal_handle,
					 &priv->wmm.highest_queued_prio,
					 priv->adapter->callbacks.
					 moal_free_lock);
		}
	}

	LEAVE();
	return;
}

/**
 *  @brief This function intializes the timers
 *
 *  @param pmadapter  A pointer to a mlan_adapter structure
 *
 *  @return           MLAN_STATUS_SUCCESS -- on success,
 *                    otherwise MLAN_STATUS_FAILURE
 *
 */
mlan_status
wlan_init_timer(IN pmlan_adapter pmadapter)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	pmlan_callbacks pcb = &pmadapter->callbacks;
	ENTER();

	if (pcb->
	    moal_init_timer(pmadapter->pmoal_handle,
			    &pmadapter->pmlan_cmd_timer, wlan_cmd_timeout_func,
			    pmadapter)
	    != MLAN_STATUS_SUCCESS) {
		ret = MLAN_STATUS_FAILURE;
		goto error;
	}
error:
	LEAVE();
	return ret;
}

/**
 *  @brief This function releases the timers
 *
 *  @param pmadapter  A pointer to a mlan_adapter structure
 *
 *  @return           None
 *
 */
t_void
wlan_free_timer(IN pmlan_adapter pmadapter)
{
	pmlan_callbacks pcb = &pmadapter->callbacks;
	ENTER();

	if (pmadapter->pmlan_cmd_timer)
		pcb->moal_free_timer(pmadapter->pmoal_handle,
				     pmadapter->pmlan_cmd_timer);

	LEAVE();
	return;
}

/**
 *  @brief  This function initializes firmware
 *
 *  @param pmadapter		A pointer to mlan_adapter
 *
 *  @return		MLAN_STATUS_SUCCESS, MLAN_STATUS_PENDING or MLAN_STATUS_FAILURE
 */
mlan_status
wlan_init_fw(IN pmlan_adapter pmadapter)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	pmlan_private priv = MNULL;
	t_u8 i = 0;

	ENTER();

	/* Initialize adapter structure */
	wlan_init_adapter(pmadapter);

	for (i = 0; i < pmadapter->priv_num; i++) {
		if (pmadapter->priv[i]) {
			priv = pmadapter->priv[i];

			/* Initialize private structure */
			if ((ret = wlan_init_priv(priv))) {
				ret = MLAN_STATUS_FAILURE;
				goto done;
			}
		}
	}
#ifdef MFG_CMD_SUPPORT
	if (pmadapter->mfg_mode != MTRUE) {
#endif
		/* Issue firmware initialize commands for first BSS, for other
		   interfaces it will be called after getting the last init
		   command response of previous interface */
		priv = wlan_get_priv(pmadapter, MLAN_BSS_ROLE_ANY);
		ret = priv->ops.init_cmd(priv, MTRUE);
		if (ret == MLAN_STATUS_FAILURE)
			goto done;
#ifdef MFG_CMD_SUPPORT
	}
#endif

	if (util_peek_list(pmadapter->pmoal_handle, &pmadapter->cmd_pending_q,
			   pmadapter->callbacks.moal_spin_lock,
			   pmadapter->callbacks.moal_spin_unlock)) {
		/* Send the first command in queue and return */
		if (mlan_main_process(pmadapter) == MLAN_STATUS_FAILURE)
			ret = MLAN_STATUS_FAILURE;
		else
			ret = MLAN_STATUS_PENDING;
	} else {
		pmadapter->hw_status = WlanHardwareStatusReady;
	}
done:
	LEAVE();
	return ret;
}

/**
 *  @brief This function frees the structure of adapter
 *
 *  @param pmadapter      A pointer to mlan_adapter structure
 *
 *  @return             N/A
 */
t_void
wlan_free_adapter(pmlan_adapter pmadapter)
{
	mlan_callbacks *pcb = (mlan_callbacks *) & pmadapter->callbacks;
	ENTER();

	if (!pmadapter) {
		PRINTM(MERROR, "The adapter is NULL\n");
		LEAVE();
		return;
	}

	wlan_cancel_all_pending_cmd(pmadapter);
	/* Free command buffer */
	PRINTM(MINFO, "Free Command buffer\n");
	wlan_free_cmd_buffer(pmadapter);

	if (pmadapter->cmd_timer_is_set) {
		/* Cancel command timeout timer */
		pcb->moal_stop_timer(pmadapter->pmoal_handle,
				     pmadapter->pmlan_cmd_timer);
		pmadapter->cmd_timer_is_set = MFALSE;
	}
#ifdef STA_SUPPORT
	PRINTM(MINFO, "Free ScanTable\n");
	if (pmadapter->pscan_table) {
		if (pcb->moal_vmalloc && pcb->moal_vfree) {
			pcb->moal_vfree(pmadapter->pmoal_handle,
					(t_u8 *) pmadapter->pscan_table);
		} else
			pcb->moal_mfree(pmadapter->pmoal_handle,
					(t_u8 *) pmadapter->pscan_table);
		pmadapter->pscan_table = MNULL;
	}
	if (pmadapter->bcn_buf) {
		pcb->moal_mfree(pmadapter->pmoal_handle,
				(t_u8 *) pmadapter->bcn_buf);
		pmadapter->bcn_buf = MNULL;
	}
#endif

	wlan_11h_cleanup(pmadapter);

	if (pmadapter->mp_regs_buf) {
		pcb->moal_mfree(pmadapter->pmoal_handle,
				(t_u8 *) pmadapter->mp_regs_buf);
		pmadapter->mp_regs_buf = MNULL;
		pmadapter->mp_regs = MNULL;
	}
#if defined(SDIO_MULTI_PORT_TX_AGGR) || defined(SDIO_MULTI_PORT_RX_AGGR)
	wlan_free_sdio_mpa_buffers(pmadapter);
#endif
	wlan_free_mlan_buffer(pmadapter, pmadapter->psleep_cfm);
	pmadapter->psleep_cfm = MNULL;

	LEAVE();
	return;
}

/**
 *  @brief This function frees the structure of priv
 *
 *  @param pmpriv  A pointer to mlan_private structure
 *
 *  @return        N/A
 */
t_void
wlan_free_priv(mlan_private * pmpriv)
{
	ENTER();
	wlan_clean_txrx(pmpriv);
	wlan_delete_bsspriotbl(pmpriv);

#ifdef STA_SUPPORT
	wlan_free_curr_bcn(pmpriv);
#endif /* STA_SUPPORT */

	wlan_delete_station_list(pmpriv);
	LEAVE();
}

/**
 *  @brief The cmdresp handler calls this function for init_fw_complete callback
 *
 *  @param pmadapter	A pointer to mlan_adapter structure
 *
 *  @return		MLAN_STATUS_SUCCESS
 *                      	The firmware initialization callback succeeded.
 */
mlan_status
wlan_init_fw_complete(IN pmlan_adapter pmadapter)
{
	mlan_status status = MLAN_STATUS_SUCCESS;
	mlan_status ret = MLAN_STATUS_SUCCESS;
	pmlan_callbacks pcb = &pmadapter->callbacks;

	ENTER();

	/* Check if hardware is ready */
	if (pmadapter->hw_status != WlanHardwareStatusReady)
		status = MLAN_STATUS_FAILURE;

	/* Invoke callback */
	ret = pcb->moal_init_fw_complete(pmadapter->pmoal_handle, status);
	LEAVE();
	return ret;
}

/**
 *  @brief The cmdresp handler calls this function for shutdown_fw_complete callback
 *
 *  @param pmadapter	A pointer to mlan_adapter structure
 *
 *  @return		MLAN_STATUS_SUCCESS
 *                      	The firmware shutdown callback succeeded.
 */
mlan_status
wlan_shutdown_fw_complete(IN pmlan_adapter pmadapter)
{
	pmlan_callbacks pcb = &pmadapter->callbacks;
	mlan_status status = MLAN_STATUS_SUCCESS;
	mlan_status ret = MLAN_STATUS_SUCCESS;

	ENTER();
	pmadapter->hw_status = WlanHardwareStatusNotReady;
	/* Invoke callback */
	ret = pcb->moal_shutdown_fw_complete(pmadapter->pmoal_handle, status);
	LEAVE();
	return ret;
}
