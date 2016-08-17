/** @file mlan_11n.c
 *
 *  @brief This file contains functions for 11n handling.
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
 *
 */

/********************************************************
Change log:
    11/10/2008: initial version
********************************************************/

#include "mlan.h"
#include "mlan_join.h"
#include "mlan_util.h"
#include "mlan_fw.h"
#include "mlan_main.h"
#include "mlan_wmm.h"
#include "mlan_11n.h"

/********************************************************
    Local Variables
********************************************************/

/********************************************************
    Global Variables
********************************************************/

/********************************************************
    Local Functions
********************************************************/

/**
 *
 *  @brief set/get max tx buf size
 *
 *  @param pmadapter	A pointer to mlan_adapter structure
 *  @param pioctl_req	A pointer to ioctl request buffer
 *
 *  @return				MLAN_STATUS_SUCCESS --success, otherwise fail
 */
static mlan_status
wlan_11n_ioctl_max_tx_buf_size(IN pmlan_adapter pmadapter,
			       IN pmlan_ioctl_req pioctl_req)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_11n_cfg *cfg = MNULL;

	ENTER();
	cfg = (mlan_ds_11n_cfg *) pioctl_req->pbuf;
	cfg->param.tx_buf_size = (t_u32) pmadapter->max_tx_buf_size;
	pioctl_req->data_read_written = sizeof(t_u32) + MLAN_SUB_COMMAND_SIZE;

	LEAVE();
	return ret;
}

/**
 *  @brief Set/get htcapinfo configuration
 *
 *  @param pmadapter	A pointer to mlan_adapter structure
 *  @param pioctl_req	A pointer to ioctl request buffer
 *
 *  @return				MLAN_STATUS_SUCCESS --success, otherwise fail
 */
static mlan_status
wlan_11n_ioctl_htusrcfg(IN pmlan_adapter pmadapter,
			IN pmlan_ioctl_req pioctl_req)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_private *pmpriv = pmadapter->priv[pioctl_req->bss_index];
	mlan_ds_11n_cfg *cfg = MNULL;

	ENTER();

	cfg = (mlan_ds_11n_cfg *) pioctl_req->pbuf;

	if (pioctl_req->action == MLAN_ACT_SET) {
		if (((cfg->param.htcap_cfg.htcap & ~IGN_HW_DEV_CAP) &
		     pmpriv->adapter->hw_dot_11n_dev_cap)
		    != (cfg->param.htcap_cfg.htcap & ~IGN_HW_DEV_CAP)) {
			pioctl_req->status_code = MLAN_ERROR_INVALID_PARAMETER;
			ret = MLAN_STATUS_FAILURE;
		} else {
			if (cfg->param.htcap_cfg.misc_cfg == BAND_SELECT_BG) {
				pmadapter->usr_dot_11n_dev_cap_bg =
					cfg->param.htcap_cfg.htcap;
				PRINTM(MINFO,
				       "Set: UsrDot11nCap for 2.4GHz 0x%x\n",
				       pmadapter->usr_dot_11n_dev_cap_bg);
			}
			if (cfg->param.htcap_cfg.misc_cfg == BAND_SELECT_A) {
				pmadapter->usr_dot_11n_dev_cap_a =
					cfg->param.htcap_cfg.htcap;
				PRINTM(MINFO,
				       "Set: UsrDot11nCap for 5GHz 0x%x\n",
				       pmadapter->usr_dot_11n_dev_cap_a);
			}
			if (cfg->param.htcap_cfg.misc_cfg == BAND_SELECT_BOTH) {
				pmadapter->usr_dot_11n_dev_cap_bg =
					cfg->param.htcap_cfg.htcap;
				pmadapter->usr_dot_11n_dev_cap_a =
					cfg->param.htcap_cfg.htcap;
				PRINTM(MINFO,
				       "Set: UsrDot11nCap for 2.4GHz and 5GHz 0x%x\n",
				       cfg->param.htcap_cfg.htcap);
			}
		}
	} else {
		/* Hardware 11N device capability required */
		if (cfg->param.htcap_cfg.hw_cap_req)
			cfg->param.htcap_cfg.htcap =
				pmadapter->hw_dot_11n_dev_cap;
		else {
			if (cfg->param.htcap_cfg.misc_cfg == BAND_SELECT_BG) {
				cfg->param.htcap_cfg.htcap =
					pmadapter->usr_dot_11n_dev_cap_bg;
				PRINTM(MINFO,
				       "Get: UsrDot11nCap for 2.4GHz 0x%x\n",
				       cfg->param.htcap_cfg.htcap);
			}
			if (cfg->param.htcap_cfg.misc_cfg == BAND_SELECT_A) {
				cfg->param.htcap_cfg.htcap =
					pmadapter->usr_dot_11n_dev_cap_a;
				PRINTM(MINFO,
				       "Get: UsrDot11nCap for 5GHz 0x%x\n",
				       cfg->param.htcap_cfg.htcap);
			}
		}
	}

	LEAVE();
	return ret;
}

/**
 *  @brief Enable/Disable AMSDU AGGR CTRL
 *
 *  @param pmadapter	A pointer to mlan_adapter structure
 *  @param pioctl_req	A pointer to ioctl request buffer
 *
 *  @return		MLAN_STATUS_PENDING --success, otherwise fail
 */
static mlan_status
wlan_11n_ioctl_amsdu_aggr_ctrl(IN pmlan_adapter pmadapter,
			       IN pmlan_ioctl_req pioctl_req)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_private *pmpriv = pmadapter->priv[pioctl_req->bss_index];
	mlan_ds_11n_cfg *cfg = MNULL;
	t_u16 cmd_action = 0;

	ENTER();

	cfg = (mlan_ds_11n_cfg *) pioctl_req->pbuf;
	if (pioctl_req->action == MLAN_ACT_SET)
		cmd_action = HostCmd_ACT_GEN_SET;
	else
		cmd_action = HostCmd_ACT_GEN_GET;

	/* Send request to firmware */
	ret = wlan_prepare_cmd(pmpriv,
			       HostCmd_CMD_AMSDU_AGGR_CTRL,
			       cmd_action,
			       0,
			       (t_void *) pioctl_req,
			       (t_void *) & cfg->param.amsdu_aggr_ctrl);
	if (ret == MLAN_STATUS_SUCCESS)
		ret = MLAN_STATUS_PENDING;

	LEAVE();
	return ret;
}

/**
 *  @brief Set/get 11n configuration
 *
 *  @param pmadapter	A pointer to mlan_adapter structure
 *  @param pioctl_req	A pointer to ioctl request buffer
 *
 *  @return		MLAN_STATUS_PENDING --success, otherwise fail
 */
static mlan_status
wlan_11n_ioctl_httxcfg(IN pmlan_adapter pmadapter,
		       IN pmlan_ioctl_req pioctl_req)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_private *pmpriv = pmadapter->priv[pioctl_req->bss_index];
	mlan_ds_11n_cfg *cfg = MNULL;
	t_u16 cmd_action = 0;

	ENTER();

	cfg = (mlan_ds_11n_cfg *) pioctl_req->pbuf;
	if (pioctl_req->action == MLAN_ACT_SET)
		cmd_action = HostCmd_ACT_GEN_SET;
	else
		cmd_action = HostCmd_ACT_GEN_GET;

	/* Send request to firmware */
	ret = wlan_prepare_cmd(pmpriv,
			       HostCmd_CMD_11N_CFG,
			       cmd_action,
			       0,
			       (t_void *) pioctl_req,
			       (t_void *) & cfg->param.tx_cfg);
	if (ret == MLAN_STATUS_SUCCESS)
		ret = MLAN_STATUS_PENDING;

	LEAVE();
	return ret;
}

/**
 *  @brief Set/get TX beamforming capabilities
 *
 *  @param pmadapter    A pointer to mlan_adapter structure
 *  @param pioctl_req   A pointer to ioctl request buffer
 *
 *  @return             MLAN_STATUS_SUCCESS --success
 */
static mlan_status
wlan_11n_ioctl_tx_bf_cap(IN pmlan_adapter pmadapter,
			 IN pmlan_ioctl_req pioctl_req)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_private *pmpriv = pmadapter->priv[pioctl_req->bss_index];
	mlan_ds_11n_cfg *cfg = MNULL;

	ENTER();

	cfg = (mlan_ds_11n_cfg *) pioctl_req->pbuf;
	if (pioctl_req->action == MLAN_ACT_SET)
		pmpriv->tx_bf_cap = cfg->param.tx_bf_cap;
	else
		cfg->param.tx_bf_cap = pmpriv->tx_bf_cap;

	LEAVE();
	return ret;
}

/**
 *  @brief Set/get TX beamforming configurations
 *
 *  @param pmadapter    A pointer to mlan_adapter structure
 *  @param pioctl_req   A pointer to ioctl request buffer
 *
 *  @return             MLAN_STATUS_PENDING --success, otherwise fail
 */
static mlan_status
wlan_11n_ioctl_tx_bf_cfg(IN pmlan_adapter pmadapter,
			 IN pmlan_ioctl_req pioctl_req)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_private *pmpriv = pmadapter->priv[pioctl_req->bss_index];
	mlan_ds_11n_cfg *cfg = MNULL;
	t_u16 cmd_action = 0;

	ENTER();

	cfg = (mlan_ds_11n_cfg *) pioctl_req->pbuf;
	if (pioctl_req->action == MLAN_ACT_SET)
		cmd_action = HostCmd_ACT_GEN_SET;
	else
		cmd_action = HostCmd_ACT_GEN_GET;

	/* Send request to firmware */
	ret = wlan_prepare_cmd(pmpriv,
			       HostCmd_CMD_TX_BF_CFG,
			       cmd_action,
			       0,
			       (t_void *) pioctl_req,
			       (t_void *) & cfg->param.tx_bf);
	if (ret == MLAN_STATUS_SUCCESS)
		ret = MLAN_STATUS_PENDING;

	LEAVE();
	return ret;
}

/**
 *  @brief Set/get HT stream configurations
 *
 *  @param pmadapter    A pointer to mlan_adapter structure
 *  @param pioctl_req   A pointer to ioctl request buffer
 *
 *  @return             MLAN_STATUS_SUCCESS --success, otherwise fail
 */
static mlan_status
wlan_11n_ioctl_stream_cfg(IN pmlan_adapter pmadapter,
			  IN pmlan_ioctl_req pioctl_req)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_ds_11n_cfg *cfg = MNULL;

	ENTER();

	cfg = (mlan_ds_11n_cfg *) pioctl_req->pbuf;
	if (pioctl_req->action == MLAN_ACT_GET) {
		cfg->param.stream_cfg = pmadapter->usr_dev_mcs_support;
	} else if (pioctl_req->action == MLAN_ACT_SET) {
		switch (cfg->param.stream_cfg) {
		case HT_STREAM_MODE_2X2:
			if (pmadapter->hw_dev_mcs_support == HT_STREAM_MODE_1X1) {
				PRINTM(MERROR,
				       "HW does not support this mode\n");
				ret = MLAN_STATUS_FAILURE;
			} else
				pmadapter->usr_dev_mcs_support =
					cfg->param.stream_cfg;
			break;
		case HT_STREAM_MODE_1X1:
			pmadapter->usr_dev_mcs_support = cfg->param.stream_cfg;
			break;
		default:
			PRINTM(MERROR, "Invalid stream mode\n");
			pioctl_req->status_code = MLAN_ERROR_INVALID_PARAMETER;
			ret = MLAN_STATUS_FAILURE;
			break;
		}
	}

	LEAVE();
	return ret;
}

/**
 *  @brief This function will resend addba request to all
 *          the peer in the TxBAStreamTbl
 *
 *  @param priv     A pointer to mlan_private
 *
 *  @return         N/A
 */
static void
wlan_11n_update_addba_request(mlan_private * priv)
{

	TxBAStreamTbl *ptx_tbl;

	ENTER();

	ptx_tbl = (TxBAStreamTbl *) util_peek_list(priv->adapter->pmoal_handle,
						   &priv->tx_ba_stream_tbl_ptr,
						   priv->adapter->callbacks.
						   moal_spin_lock,
						   priv->adapter->callbacks.
						   moal_spin_unlock);
	if (!ptx_tbl) {
		LEAVE();
		return;
	}

	while (ptx_tbl != (TxBAStreamTbl *) & priv->tx_ba_stream_tbl_ptr) {
		wlan_send_addba(priv, ptx_tbl->tid, ptx_tbl->ra);
		ptx_tbl = ptx_tbl->pnext;
	}
	/* Signal MOAL to trigger mlan_main_process */
	wlan_recv_event(priv, MLAN_EVENT_ID_DRV_DEFER_HANDLING, MNULL);
	LEAVE();
	return;
}

/**
 *  @brief Set/get addba parameter
 *
 *  @param pmadapter	A pointer to mlan_adapter structure
 *  @param pioctl_req	A pointer to ioctl request buffer
 *
 *  @return		MLAN_STATUS_SUCCESS --success
 */
static mlan_status
wlan_11n_ioctl_addba_param(IN pmlan_adapter pmadapter,
			   IN pmlan_ioctl_req pioctl_req)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_private *pmpriv = pmadapter->priv[pioctl_req->bss_index];
	mlan_ds_11n_cfg *cfg = MNULL;
	t_u32 timeout;

	ENTER();

	cfg = (mlan_ds_11n_cfg *) pioctl_req->pbuf;
	if (pioctl_req->action == MLAN_ACT_GET) {
		cfg->param.addba_param.timeout = pmpriv->add_ba_param.timeout;
		cfg->param.addba_param.txwinsize =
			pmpriv->add_ba_param.tx_win_size;
		cfg->param.addba_param.rxwinsize =
			pmpriv->add_ba_param.rx_win_size;
		cfg->param.addba_param.txamsdu = pmpriv->add_ba_param.tx_amsdu;
		cfg->param.addba_param.rxamsdu = pmpriv->add_ba_param.rx_amsdu;
	} else {
		timeout = pmpriv->add_ba_param.timeout;
		pmpriv->add_ba_param.timeout = cfg->param.addba_param.timeout;
		pmpriv->add_ba_param.tx_win_size =
			cfg->param.addba_param.txwinsize;
		pmpriv->add_ba_param.rx_win_size =
			cfg->param.addba_param.rxwinsize;
		pmpriv->add_ba_param.tx_amsdu = cfg->param.addba_param.txamsdu;
		pmpriv->add_ba_param.rx_amsdu = cfg->param.addba_param.rxamsdu;
		if (timeout != pmpriv->add_ba_param.timeout) {
			wlan_11n_update_addba_request(pmpriv);
		}
	}

	LEAVE();
	return ret;
}

/**
 *  @brief This function send delba to specific tid
 *
 *  @param priv         A pointer to mlan_priv
 *  @param tid          tid
 *  @return 	        N/A
 */
void
wlan_11n_delba(mlan_private * priv, int tid)
{
	RxReorderTbl *rx_reor_tbl_ptr;

	ENTER();

	rx_reor_tbl_ptr =
		(RxReorderTbl *) util_peek_list(priv->adapter->pmoal_handle,
						&priv->rx_reorder_tbl_ptr,
						priv->adapter->callbacks.
						moal_spin_lock,
						priv->adapter->callbacks.
						moal_spin_unlock);
	if (!rx_reor_tbl_ptr) {
		LEAVE();
		return;
	}

	while (rx_reor_tbl_ptr != (RxReorderTbl *) & priv->rx_reorder_tbl_ptr) {
		if (rx_reor_tbl_ptr->tid == tid) {
			PRINTM(MIOCTL, "Send delba to tid=%d, " MACSTR "\n",
			       tid, MAC2STR(rx_reor_tbl_ptr->ta));
			wlan_send_delba(priv, MNULL, tid, rx_reor_tbl_ptr->ta,
					0);
		}
		rx_reor_tbl_ptr = rx_reor_tbl_ptr->pnext;
	}

	LEAVE();
	return;
}

/**
 *  @brief Set/get addba reject set
 *
 *  @param pmadapter	A pointer to mlan_adapter structure
 *  @param pioctl_req	A pointer to ioctl request buffer
 *
 *  @return		MLAN_STATUS_SUCCESS --success, otherwise fail
 */
static mlan_status
wlan_11n_ioctl_addba_reject(IN pmlan_adapter pmadapter,
			    IN pmlan_ioctl_req pioctl_req)
{
	int i = 0;
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_private *pmpriv = pmadapter->priv[pioctl_req->bss_index];
	mlan_ds_11n_cfg *cfg = MNULL;

	ENTER();

	cfg = (mlan_ds_11n_cfg *) pioctl_req->pbuf;

	if (pioctl_req->action == MLAN_ACT_GET) {
		PRINTM(MINFO, "Get Addba reject\n");
		memcpy(pmadapter, cfg->param.addba_reject, pmpriv->addba_reject,
		       MAX_NUM_TID);
	} else {
		if (pmpriv->media_connected == MTRUE) {
			for (i = 0; i < MAX_NUM_TID; i++) {
				if (cfg->param.addba_reject[i] ==
				    ADDBA_RSP_STATUS_REJECT) {
					PRINTM(MIOCTL,
					       "Receive addba reject: tid=%d\n",
					       i);
					wlan_11n_delba(pmpriv, i);
				}
			}
			wlan_recv_event(pmpriv,
					MLAN_EVENT_ID_DRV_DEFER_HANDLING,
					MNULL);
		}
		for (i = 0; i < MAX_NUM_TID; i++) {
			/* For AMPDU */
			if (cfg->param.addba_reject[i] >
			    ADDBA_RSP_STATUS_REJECT) {
				pioctl_req->status_code =
					MLAN_ERROR_INVALID_PARAMETER;
				ret = MLAN_STATUS_FAILURE;
				break;
			}

			pmpriv->addba_reject[i] = cfg->param.addba_reject[i];
		}
	}

	LEAVE();
	return ret;
}

/**
 *  @brief This function will send DELBA to entries in the priv's
 *          Tx BA stream table
 *
 *  @param priv                 A pointer to mlan_private
 *  @param pioctl_req	        A pointer to ioctl request buffer
 *  @param tid                  TID
 *  @param peer_address         A pointer to peer address
 *  @param last_tx_ba_to_delete A pointer to the last entry in TxBAStreamTbl
 *
 *  @return		MLAN_STATUS_SUCCESS or MLAN_STATUS_PENDING
 */
static mlan_status
wlan_send_delba_to_entry_in_txbastream_tbl(pmlan_private priv,
					   pmlan_ioctl_req pioctl_req, t_u8 tid,
					   t_u8 * peer_address,
					   TxBAStreamTbl * last_tx_ba_to_delete)
{
	pmlan_adapter pmadapter = priv->adapter;
	TxBAStreamTbl *tx_ba_stream_tbl_ptr;
	t_u8 zero_mac[MLAN_MAC_ADDR_LENGTH] = { 0 };
	mlan_status ret = MLAN_STATUS_SUCCESS;

	ENTER();

	tx_ba_stream_tbl_ptr =
		(TxBAStreamTbl *) util_peek_list(pmadapter->pmoal_handle,
						 &priv->tx_ba_stream_tbl_ptr,
						 pmadapter->callbacks.
						 moal_spin_lock,
						 pmadapter->callbacks.
						 moal_spin_unlock);
	if (!tx_ba_stream_tbl_ptr) {
		LEAVE();
		return ret;
	}

	while (tx_ba_stream_tbl_ptr !=
	       (TxBAStreamTbl *) & priv->tx_ba_stream_tbl_ptr) {
		if (tx_ba_stream_tbl_ptr->ba_status == BA_STREAM_SETUP_COMPLETE) {
			if (((tid == DELBA_ALL_TIDS) ||
			     (tid == tx_ba_stream_tbl_ptr->tid)) &&
			    (!memcmp
			     (pmadapter, peer_address, zero_mac,
			      MLAN_MAC_ADDR_LENGTH) ||
			     !memcmp(pmadapter, peer_address,
				     tx_ba_stream_tbl_ptr->ra,
				     MLAN_MAC_ADDR_LENGTH))) {
				if (last_tx_ba_to_delete &&
				    (tx_ba_stream_tbl_ptr ==
				     last_tx_ba_to_delete))
					ret = wlan_send_delba(priv, pioctl_req,
							      tx_ba_stream_tbl_ptr->
							      tid,
							      tx_ba_stream_tbl_ptr->
							      ra, 1);
				else
					ret = wlan_send_delba(priv, MNULL,
							      tx_ba_stream_tbl_ptr->
							      tid,
							      tx_ba_stream_tbl_ptr->
							      ra, 1);
			}
		}
		tx_ba_stream_tbl_ptr = tx_ba_stream_tbl_ptr->pnext;
	}

	LEAVE();
	return ret;
}

/**
 *  @brief This function will send DELBA to entries in the priv's
 *          rx reordering table
 *
 *  @param priv                 A pointer to mlan_private
 *  @param pioctl_req	        A pointer to ioctl request buffer
 *  @param tid                  TID
 *  @param peer_address         A pointer to peer address
 *  @param last_rx_ba_to_delete A pointer to the last entry in RxReorderTbl
 *
 *  @return		MLAN_STATUS_SUCCESS or MLAN_STATUS_PENDING
 */
static mlan_status
wlan_send_delba_to_entry_in_reorder_tbl(pmlan_private priv,
					pmlan_ioctl_req pioctl_req, t_u8 tid,
					t_u8 * peer_address,
					RxReorderTbl * last_rx_ba_to_delete)
{
	pmlan_adapter pmadapter = priv->adapter;
	RxReorderTbl *rx_reor_tbl_ptr;
	t_u8 zero_mac[MLAN_MAC_ADDR_LENGTH] = { 0 };
	mlan_status ret = MLAN_STATUS_SUCCESS;

	ENTER();

	rx_reor_tbl_ptr =
		(RxReorderTbl *) util_peek_list(pmadapter->pmoal_handle,
						&priv->rx_reorder_tbl_ptr,
						pmadapter->callbacks.
						moal_spin_lock,
						pmadapter->callbacks.
						moal_spin_unlock);
	if (!rx_reor_tbl_ptr) {
		LEAVE();
		return ret;
	}

	while (rx_reor_tbl_ptr != (RxReorderTbl *) & priv->rx_reorder_tbl_ptr) {
		if (rx_reor_tbl_ptr->ba_status == BA_STREAM_SETUP_COMPLETE) {
			if (((tid == DELBA_ALL_TIDS) ||
			     (tid == rx_reor_tbl_ptr->tid)) &&
			    (!memcmp
			     (pmadapter, peer_address, zero_mac,
			      MLAN_MAC_ADDR_LENGTH) ||
			     !memcmp(pmadapter, peer_address,
				     rx_reor_tbl_ptr->ta,
				     MLAN_MAC_ADDR_LENGTH))) {
				if (last_rx_ba_to_delete &&
				    (rx_reor_tbl_ptr == last_rx_ba_to_delete))
					ret = wlan_send_delba(priv, pioctl_req,
							      rx_reor_tbl_ptr->
							      tid,
							      rx_reor_tbl_ptr->
							      ta, 0);
				else
					ret = wlan_send_delba(priv, MNULL,
							      rx_reor_tbl_ptr->
							      tid,
							      rx_reor_tbl_ptr->
							      ta, 0);
			}
		}
		rx_reor_tbl_ptr = rx_reor_tbl_ptr->pnext;
	}

	LEAVE();
	return ret;
}

/**
 *  @brief IOCTL to delete BA
 *
 *  @param pmadapter	A pointer to mlan_adapter structure
 *  @param pioctl_req	A pointer to ioctl request buffer
 *
 *  @return		MLAN_STATUS_SUCCESS --success, otherwise fail
 */
static mlan_status
wlan_11n_ioctl_delba(IN pmlan_adapter pmadapter, IN pmlan_ioctl_req pioctl_req)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_private *pmpriv = pmadapter->priv[pioctl_req->bss_index];
	mlan_ds_11n_cfg *cfg = MNULL;
	TxBAStreamTbl *tx_ba_stream_tbl_ptr, *last_tx_ba_to_delete = MNULL;
	RxReorderTbl *rx_reor_tbl_ptr, *last_rx_ba_to_delete = MNULL;
	t_u8 zero_mac[MLAN_MAC_ADDR_LENGTH] = { 0 };
	t_u8 tid, *peer_address;

	ENTER();

	cfg = (mlan_ds_11n_cfg *) pioctl_req->pbuf;
	tid = cfg->param.del_ba.tid;
	peer_address = cfg->param.del_ba.peer_mac_addr;

	PRINTM(MINFO, "DelBA: direction %d, TID %d, peer address " MACSTR "\n",
	       cfg->param.del_ba.direction, tid, MAC2STR(peer_address));

	if (cfg->param.del_ba.direction & DELBA_RX) {
		rx_reor_tbl_ptr =
			(RxReorderTbl *) util_peek_list(pmadapter->pmoal_handle,
							&pmpriv->
							rx_reorder_tbl_ptr,
							pmadapter->callbacks.
							moal_spin_lock,
							pmadapter->callbacks.
							moal_spin_unlock);

		if (rx_reor_tbl_ptr) {
			while (rx_reor_tbl_ptr !=
			       (RxReorderTbl *) & pmpriv->rx_reorder_tbl_ptr) {
				if (rx_reor_tbl_ptr->ba_status ==
				    BA_STREAM_SETUP_COMPLETE) {
					if (((tid == DELBA_ALL_TIDS) ||
					     (tid == rx_reor_tbl_ptr->tid)) &&
					    (!memcmp
					     (pmadapter, peer_address, zero_mac,
					      MLAN_MAC_ADDR_LENGTH) ||
					     !memcmp(pmadapter, peer_address,
						     rx_reor_tbl_ptr->ta,
						     MLAN_MAC_ADDR_LENGTH))) {
						/* Found RX BA to delete */
						last_rx_ba_to_delete =
							rx_reor_tbl_ptr;
					}
				}
				rx_reor_tbl_ptr = rx_reor_tbl_ptr->pnext;
			}
		}
	}

	if ((last_rx_ba_to_delete == MNULL) &&
	    (cfg->param.del_ba.direction & DELBA_TX)) {
		tx_ba_stream_tbl_ptr =
			(TxBAStreamTbl *) util_peek_list(pmadapter->
							 pmoal_handle,
							 &pmpriv->
							 tx_ba_stream_tbl_ptr,
							 pmadapter->callbacks.
							 moal_spin_lock,
							 pmadapter->callbacks.
							 moal_spin_unlock);

		if (tx_ba_stream_tbl_ptr) {
			while (tx_ba_stream_tbl_ptr !=
			       (TxBAStreamTbl *) & pmpriv->
			       tx_ba_stream_tbl_ptr) {
				if (tx_ba_stream_tbl_ptr->ba_status ==
				    BA_STREAM_SETUP_COMPLETE) {
					if (((tid == DELBA_ALL_TIDS) ||
					     (tid == tx_ba_stream_tbl_ptr->tid))
					    &&
					    (!memcmp
					     (pmadapter, peer_address, zero_mac,
					      MLAN_MAC_ADDR_LENGTH) ||
					     !memcmp(pmadapter, peer_address,
						     tx_ba_stream_tbl_ptr->ra,
						     MLAN_MAC_ADDR_LENGTH))) {
						/* Found TX BA to delete */
						last_tx_ba_to_delete =
							tx_ba_stream_tbl_ptr;
					}
				}
				tx_ba_stream_tbl_ptr =
					tx_ba_stream_tbl_ptr->pnext;
			}
		}
	}

	if (cfg->param.del_ba.direction & DELBA_TX) {
		if (last_rx_ba_to_delete)
			ret = wlan_send_delba_to_entry_in_txbastream_tbl(pmpriv,
									 MNULL,
									 tid,
									 peer_address,
									 MNULL);
		else
			ret = wlan_send_delba_to_entry_in_txbastream_tbl(pmpriv,
									 pioctl_req,
									 tid,
									 peer_address,
									 last_tx_ba_to_delete);
	}
	if (last_rx_ba_to_delete) {
		ret = wlan_send_delba_to_entry_in_reorder_tbl(pmpriv,
							      pioctl_req, tid,
							      peer_address,
							      last_rx_ba_to_delete);
	}

	LEAVE();
	return ret;
}

/**
 *  @brief IOCTL to reject addba req
 *
 *  @param pmadapter	A pointer to mlan_adapter structure
 *  @param pioctl_req	A pointer to ioctl request buffer
 *
 *  @return		MLAN_STATUS_SUCCESS --success, otherwise fail
 */
static mlan_status
wlan_11n_ioctl_rejectaddbareq(IN pmlan_adapter pmadapter,
			      IN pmlan_ioctl_req pioctl_req)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_private *pmpriv = pmadapter->priv[pioctl_req->bss_index];
	mlan_ds_11n_cfg *cfg = MNULL;
	t_u16 cmd_action = 0;

	ENTER();

	cfg = (mlan_ds_11n_cfg *) pioctl_req->pbuf;
	if (pioctl_req->action == MLAN_ACT_SET)
		cmd_action = HostCmd_ACT_GEN_SET;
	else
		cmd_action = HostCmd_ACT_GEN_GET;

	/* Send command to firmware */
	ret = wlan_prepare_cmd(pmpriv,
			       HostCmd_CMD_REJECT_ADDBA_REQ,
			       cmd_action,
			       0,
			       (t_void *) pioctl_req,
			       &cfg->param.reject_addba_req);

	if (ret == MLAN_STATUS_SUCCESS)
		ret = MLAN_STATUS_PENDING;

	LEAVE();
	return ret;
}

/**
 *  @brief This function will send DELBA to entries in the priv's
 *          Tx BA stream table
 *
 *  @param priv                 A pointer to mlan_private
 *  @param tid                  TID
 *
 *  @return		 N/A
 */
static void
wlan_send_delba_txbastream_tbl(pmlan_private priv, t_u8 tid)
{
	pmlan_adapter pmadapter = priv->adapter;
	TxBAStreamTbl *tx_ba_stream_tbl_ptr;

	ENTER();

	tx_ba_stream_tbl_ptr =
		(TxBAStreamTbl *) util_peek_list(pmadapter->pmoal_handle,
						 &priv->tx_ba_stream_tbl_ptr,
						 pmadapter->callbacks.
						 moal_spin_lock,
						 pmadapter->callbacks.
						 moal_spin_unlock);
	if (!tx_ba_stream_tbl_ptr) {
		LEAVE();
		return;
	}

	while (tx_ba_stream_tbl_ptr !=
	       (TxBAStreamTbl *) & priv->tx_ba_stream_tbl_ptr) {
		if (tx_ba_stream_tbl_ptr->ba_status == BA_STREAM_SETUP_COMPLETE) {
			if (tid == tx_ba_stream_tbl_ptr->tid) {
				PRINTM(MIOCTL,
				       "Tx:Send delba to tid=%d, " MACSTR "\n",
				       tid, MAC2STR(tx_ba_stream_tbl_ptr->ra));
				wlan_send_delba(priv, MNULL,
						tx_ba_stream_tbl_ptr->tid,
						tx_ba_stream_tbl_ptr->ra, 1);
			}
		}
		tx_ba_stream_tbl_ptr = tx_ba_stream_tbl_ptr->pnext;
	}

	LEAVE();
	return;
}

/**
 *  @brief Set/get aggr_prio_tbl
 *
 *  @param pmadapter	A pointer to mlan_adapter structure
 *  @param pioctl_req	A pointer to ioctl request buffer
 *
 *  @return		MLAN_STATUS_SUCCESS --success, otherwise fail
 */
static mlan_status
wlan_11n_ioctl_aggr_prio_tbl(IN pmlan_adapter pmadapter,
			     IN pmlan_ioctl_req pioctl_req)
{
	int i = 0;
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_private *pmpriv = pmadapter->priv[pioctl_req->bss_index];
	mlan_ds_11n_cfg *cfg = MNULL;

	ENTER();

	cfg = (mlan_ds_11n_cfg *) pioctl_req->pbuf;

	if (pioctl_req->action == MLAN_ACT_GET) {
		for (i = 0; i < MAX_NUM_TID; i++) {
			cfg->param.aggr_prio_tbl.ampdu[i] =
				pmpriv->aggr_prio_tbl[i].ampdu_user;
			cfg->param.aggr_prio_tbl.amsdu[i] =
				pmpriv->aggr_prio_tbl[i].amsdu;
		}
	} else {
		if (pmpriv->media_connected == MTRUE) {
			for (i = 0; i < MAX_NUM_TID; i++) {
				if (cfg->param.aggr_prio_tbl.ampdu[i] ==
				    BA_STREAM_NOT_ALLOWED) {
					PRINTM(MIOCTL,
					       "Receive aggrpriotbl: BA not allowed tid=%d\n",
					       i);
					wlan_send_delba_txbastream_tbl(pmpriv,
								       i);
				}
			}
			wlan_recv_event(pmpriv,
					MLAN_EVENT_ID_DRV_DEFER_HANDLING,
					MNULL);
		}

		for (i = 0; i < MAX_NUM_TID; i++) {
			/* For AMPDU */
			if ((cfg->param.aggr_prio_tbl.ampdu[i] > HIGH_PRIO_TID)
			    && (cfg->param.aggr_prio_tbl.ampdu[i] !=
				BA_STREAM_NOT_ALLOWED)) {
				pioctl_req->status_code =
					MLAN_ERROR_INVALID_PARAMETER;
				ret = MLAN_STATUS_FAILURE;
				break;
			}

			pmpriv->aggr_prio_tbl[i].ampdu_ap =
				pmpriv->aggr_prio_tbl[i].ampdu_user =
				cfg->param.aggr_prio_tbl.ampdu[i];

			/* For AMSDU */
			if ((cfg->param.aggr_prio_tbl.amsdu[i] > HIGH_PRIO_TID
			     && cfg->param.aggr_prio_tbl.amsdu[i] !=
			     BA_STREAM_NOT_ALLOWED)) {
				pioctl_req->status_code =
					MLAN_ERROR_INVALID_PARAMETER;
				ret = MLAN_STATUS_FAILURE;
				break;
			} else {
				pmpriv->aggr_prio_tbl[i].amsdu =
					cfg->param.aggr_prio_tbl.amsdu[i];
			}
		}
	}

	LEAVE();
	return ret;
}

/**
 *  @brief Get supported MCS set
 *
 *  @param pmadapter	A pointer to mlan_adapter structure
 *  @param pioctl_req	A pointer to ioctl request buffer
 *
 *  @return		MLAN_STATUS_SUCCESS --success, otherwise fail
 */
static mlan_status
wlan_11n_ioctl_supported_mcs_set(IN pmlan_adapter pmadapter,
				 IN pmlan_ioctl_req pioctl_req)
{
	mlan_ds_11n_cfg *cfg = MNULL;
	int rx_mcs_supp;
	t_u8 mcs_set[NUM_MCS_FIELD];

	ENTER();

	if (pioctl_req->action == MLAN_ACT_SET) {
		PRINTM(MERROR, "Set operation is not supported\n");
		pioctl_req->status_code = MLAN_ERROR_IOCTL_INVALID;
		LEAVE();
		return MLAN_STATUS_FAILURE;
	}
	rx_mcs_supp = GET_RXMCSSUPP(pmadapter->usr_dev_mcs_support);
	/* Set MCS for 1x1/2x2 */
	memset(pmadapter, (t_u8 *) mcs_set, 0xff, rx_mcs_supp);
	/* Clear all the other values */
	memset(pmadapter, (t_u8 *) & mcs_set[rx_mcs_supp], 0,
	       NUM_MCS_FIELD - rx_mcs_supp);
	/* Set MCS32 with 40MHz support */
	if (ISSUPP_CHANWIDTH40(pmadapter->usr_dot_11n_dev_cap_bg)
	    || ISSUPP_CHANWIDTH40(pmadapter->usr_dot_11n_dev_cap_a)
		)
		SETHT_MCS32(mcs_set);

	cfg = (mlan_ds_11n_cfg *) pioctl_req->pbuf;
	memcpy(pmadapter, cfg->param.supported_mcs_set, mcs_set, NUM_MCS_FIELD);

	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function checks if the given pointer is valid entry of
 *         Tx BA Stream table
 *
 *  @param priv         Pointer to mlan_private
 *  @param ptxtblptr    Pointer to tx ba stream entry
 *
 *  @return             MTRUE or MFALSE
 */
static int
wlan_is_txbastreamptr_valid(mlan_private * priv, TxBAStreamTbl * ptxtblptr)
{
	TxBAStreamTbl *ptx_tbl;

	ENTER();

	ptx_tbl = (TxBAStreamTbl *) util_peek_list(priv->adapter->pmoal_handle,
						   &priv->tx_ba_stream_tbl_ptr,
						   MNULL, MNULL);
	if (!ptx_tbl) {
		LEAVE();
		return MFALSE;
	}

	while (ptx_tbl != (TxBAStreamTbl *) & priv->tx_ba_stream_tbl_ptr) {
		if (ptx_tbl == ptxtblptr) {
			LEAVE();
			return MTRUE;
		}

		ptx_tbl = ptx_tbl->pnext;
	}

	LEAVE();
	return MFALSE;
}

/**
 *  @brief This function will return the pointer to a entry in BA Stream
 *  		table which matches the ba_status requested
 *
 *  @param priv    	    A pointer to mlan_private
 *  @param ba_status	Current status of the BA stream
 *
 *  @return 	        A pointer to first entry matching status in BA stream
 *                      NULL if not found
 */
static TxBAStreamTbl *
wlan_11n_get_txbastream_status(mlan_private * priv, baStatus_e ba_status)
{
	TxBAStreamTbl *ptx_tbl;

	ENTER();

	ptx_tbl = (TxBAStreamTbl *) util_peek_list(priv->adapter->pmoal_handle,
						   &priv->tx_ba_stream_tbl_ptr,
						   priv->adapter->callbacks.
						   moal_spin_lock,
						   priv->adapter->callbacks.
						   moal_spin_unlock);
	if (!ptx_tbl) {
		LEAVE();
		return MNULL;
	}

	while (ptx_tbl != (TxBAStreamTbl *) & priv->tx_ba_stream_tbl_ptr) {
		if (ptx_tbl->ba_status == ba_status) {
			LEAVE();
			return ptx_tbl;
		}

		ptx_tbl = ptx_tbl->pnext;
	}

	LEAVE();
	return MNULL;
}

/********************************************************
    Global Functions
********************************************************/

#ifdef STA_SUPPORT
/**
 *  @brief This function fills the cap info
 *
 *  @param priv         A pointer to mlan_private structure
 *  @param pht_cap      A pointer to MrvlIETypes_HTCap_t structure
 *  @param bands        Band configuration
 *
 *  @return             N/A
 */
static void
wlan_fill_cap_info(mlan_private * priv,
		   MrvlIETypes_HTCap_t * pht_cap, t_u8 bands)
{
	mlan_adapter *pmadapter = priv->adapter;
	t_u32 usr_dot_11n_dev_cap;

	ENTER();

	if (bands & BAND_A)
		usr_dot_11n_dev_cap = pmadapter->usr_dot_11n_dev_cap_a;
	else
		usr_dot_11n_dev_cap = pmadapter->usr_dot_11n_dev_cap_bg;

	if (ISSUPP_CHANWIDTH40(usr_dot_11n_dev_cap))
		SETHT_SUPPCHANWIDTH(pht_cap->ht_cap.ht_cap_info);
	else
		RESETHT_SUPPCHANWIDTH(pht_cap->ht_cap.ht_cap_info);

	if (ISSUPP_GREENFIELD(usr_dot_11n_dev_cap))
		SETHT_GREENFIELD(pht_cap->ht_cap.ht_cap_info);
	else
		RESETHT_GREENFIELD(pht_cap->ht_cap.ht_cap_info);

	if (ISSUPP_SHORTGI20(usr_dot_11n_dev_cap))
		SETHT_SHORTGI20(pht_cap->ht_cap.ht_cap_info);
	else
		RESETHT_SHORTGI20(pht_cap->ht_cap.ht_cap_info);

	if (ISSUPP_SHORTGI40(usr_dot_11n_dev_cap))
		SETHT_SHORTGI40(pht_cap->ht_cap.ht_cap_info);
	else
		RESETHT_SHORTGI40(pht_cap->ht_cap.ht_cap_info);

	if (ISSUPP_RXSTBC(usr_dot_11n_dev_cap))
		SETHT_RXSTBC(pht_cap->ht_cap.ht_cap_info, 1);
	else
		RESETHT_RXSTBC(pht_cap->ht_cap.ht_cap_info);

	if (ISENABLED_40MHZ_INTOLARENT(usr_dot_11n_dev_cap))
		SETHT_40MHZ_INTOLARANT(pht_cap->ht_cap.ht_cap_info);
	else
		RESETHT_40MHZ_INTOLARANT(pht_cap->ht_cap.ht_cap_info);

	/* No user config for LDPC coding capability yet */
	if (ISSUPP_RXLDPC(pmadapter->hw_dot_11n_dev_cap))
		SETHT_LDPCCODINGCAP(pht_cap->ht_cap.ht_cap_info);
	else
		RESETHT_LDPCCODINGCAP(pht_cap->ht_cap.ht_cap_info);

	/* No user config for TX STBC yet */
	if (ISSUPP_TXSTBC(pmadapter->hw_dot_11n_dev_cap))
		SETHT_TXSTBC(pht_cap->ht_cap.ht_cap_info);
	else
		RESETHT_TXSTBC(pht_cap->ht_cap.ht_cap_info);

	/* No user config for Delayed BACK yet */
	if (GET_DELAYEDBACK(pmadapter->hw_dot_11n_dev_cap))
		SETHT_DELAYEDBACK(pht_cap->ht_cap.ht_cap_info);
	else
		RESETHT_DELAYEDBACK(pht_cap->ht_cap.ht_cap_info);

	/* Need change to support 8k AMSDU receive */
	RESETHT_MAXAMSDU(pht_cap->ht_cap.ht_cap_info);

	LEAVE();
}

/**
 *  @brief This function fills the HT cap tlv
 *
 *  @param priv         A pointer to mlan_private structure
 *  @param pht_cap      A pointer to MrvlIETypes_HTCap_t structure
 *  @param bands        Band configuration
 *
 *  @return             N/A
 */
void
wlan_fill_ht_cap_tlv(mlan_private * priv,
		     MrvlIETypes_HTCap_t * pht_cap, t_u8 bands)
{
	mlan_adapter *pmadapter = priv->adapter;
	int rx_mcs_supp;
	t_u32 usr_dot_11n_dev_cap;

	ENTER();

	if (bands & BAND_A)
		usr_dot_11n_dev_cap = pmadapter->usr_dot_11n_dev_cap_a;
	else
		usr_dot_11n_dev_cap = pmadapter->usr_dot_11n_dev_cap_bg;

	/* Fill HT cap info */
	wlan_fill_cap_info(priv, pht_cap, bands);
	pht_cap->ht_cap.ht_cap_info =
		wlan_cpu_to_le16(pht_cap->ht_cap.ht_cap_info);

	/* Set ampdu param */
	SETAMPDU_SIZE(pht_cap->ht_cap.ampdu_param, AMPDU_FACTOR_64K);
	SETAMPDU_SPACING(pht_cap->ht_cap.ampdu_param, 0);

	rx_mcs_supp = GET_RXMCSSUPP(pmadapter->usr_dev_mcs_support);
	/* Clear all the other values to get the minimum mcs set btw STA and AP
	 */
	memset(pmadapter,
	       (t_u8 *) & pht_cap->ht_cap.supported_mcs_set[rx_mcs_supp], 0,
	       NUM_MCS_FIELD - rx_mcs_supp);
	/* Set MCS32 with 40MHz support */
	if (ISSUPP_CHANWIDTH40(usr_dot_11n_dev_cap))
		SETHT_MCS32(pht_cap->ht_cap.supported_mcs_set);

	/* Clear RD responder bit */
	RESETHT_EXTCAP_RDG(pht_cap->ht_cap.ht_ext_cap);
	pht_cap->ht_cap.ht_ext_cap =
		wlan_cpu_to_le16(pht_cap->ht_cap.ht_ext_cap);

	/* Set Tx BF cap */
	pht_cap->ht_cap.tx_bf_cap = wlan_cpu_to_le32(priv->tx_bf_cap);

	LEAVE();
	return;
}
#endif /* STA_SUPPORT */

/**
 *  @brief This function prints the 802.11n device capability
 *
 *  @param pmadapter     A pointer to mlan_adapter structure
 *  @param cap           Capability value
 *
 *  @return        N/A
 */
void
wlan_show_dot11ndevcap(pmlan_adapter pmadapter, t_u32 cap)
{
	ENTER();

	PRINTM(MINFO, "GET_HW_SPEC: Maximum MSDU length = %s octets\n",
	       (ISSUPP_MAXAMSDU(cap) ? "7935" : "3839"));
	PRINTM(MINFO, "GET_HW_SPEC: Beam forming %s\n",
	       (ISSUPP_BEAMFORMING(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: Greenfield preamble %s\n",
	       (ISSUPP_GREENFIELD(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: AMPDU %s\n",
	       (ISSUPP_AMPDU(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: MIMO Power Save %s\n",
	       (ISSUPP_MIMOPS(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: Rx STBC %s\n",
	       (ISSUPP_RXSTBC(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: Tx STBC %s\n",
	       (ISSUPP_TXSTBC(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: Short GI for 40 Mhz %s\n",
	       (ISSUPP_SHORTGI40(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: Short GI for 20 Mhz %s\n",
	       (ISSUPP_SHORTGI20(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: LDPC coded packet receive %s\n",
	       (ISSUPP_RXLDPC(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: Number of Delayed Block Ack streams = %d\n",
	       GET_DELAYEDBACK(cap));
	PRINTM(MINFO,
	       "GET_HW_SPEC: Number of Immediate Block Ack streams = %d\n",
	       GET_IMMEDIATEBACK(cap));
	PRINTM(MINFO, "GET_HW_SPEC: 40 Mhz channel width %s\n",
	       (ISSUPP_CHANWIDTH40(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: 20 Mhz channel width %s\n",
	       (ISSUPP_CHANWIDTH20(cap) ? "supported" : "not supported"));
	PRINTM(MINFO, "GET_HW_SPEC: 10 Mhz channel width %s\n",
	       (ISSUPP_CHANWIDTH10(cap) ? "supported" : "not supported"));

	if (ISSUPP_RXANTENNAA(cap)) {
		PRINTM(MINFO, "GET_HW_SPEC: Presence of Rx antenna A\n");
	}
	if (ISSUPP_RXANTENNAB(cap)) {
		PRINTM(MINFO, "GET_HW_SPEC: Presence of Rx antenna B\n");
	}
	if (ISSUPP_RXANTENNAC(cap)) {
		PRINTM(MINFO, "GET_HW_SPEC: Presence of Rx antenna C\n");
	}
	if (ISSUPP_RXANTENNAD(cap)) {
		PRINTM(MINFO, "GET_HW_SPEC: Presence of Rx antenna D\n");
	}
	if (ISSUPP_TXANTENNAA(cap)) {
		PRINTM(MINFO, "GET_HW_SPEC: Presence of Tx antenna A\n");
	}
	if (ISSUPP_TXANTENNAB(cap)) {
		PRINTM(MINFO, "GET_HW_SPEC: Presence of Tx antenna B\n");
	}
	if (ISSUPP_TXANTENNAC(cap)) {
		PRINTM(MINFO, "GET_HW_SPEC: Presence of Tx antenna C\n");
	}
	if (ISSUPP_TXANTENNAD(cap)) {
		PRINTM(MINFO, "GET_HW_SPEC: Presence of Tx antenna D\n");
	}

	LEAVE();
	return;
}

/**
 *  @brief This function prints the 802.11n device MCS
 *
 *  @param pmadapter A pointer to mlan_adapter structure
 *  @param support   Support value
 *
 *  @return        N/A
 */
void
wlan_show_devmcssupport(pmlan_adapter pmadapter, t_u8 support)
{
	ENTER();

	PRINTM(MINFO, "GET_HW_SPEC: MCSs for %dx%d MIMO\n",
	       GET_RXMCSSUPP(support), GET_TXMCSSUPP(support));

	LEAVE();
	return;
}

/**
 *  @brief This function handles the command response of
 *              delete a block ack request
 *
 *  @param priv    A pointer to mlan_private structure
 *  @param resp    A pointer to HostCmd_DS_COMMAND
 *
 *  @return        MLAN_STATUS_SUCCESS
 */
mlan_status
wlan_ret_11n_delba(mlan_private * priv, HostCmd_DS_COMMAND * resp)
{
	int tid;
	TxBAStreamTbl *ptx_ba_tbl;
	HostCmd_DS_11N_DELBA *pdel_ba =
		(HostCmd_DS_11N_DELBA *) & resp->params.del_ba;

	ENTER();

	pdel_ba->del_ba_param_set = wlan_le16_to_cpu(pdel_ba->del_ba_param_set);
	pdel_ba->reason_code = wlan_le16_to_cpu(pdel_ba->reason_code);

	tid = pdel_ba->del_ba_param_set >> DELBA_TID_POS;
	if (pdel_ba->del_result == BA_RESULT_SUCCESS) {
		mlan_11n_delete_bastream_tbl(priv, tid, pdel_ba->peer_mac_addr,
					     TYPE_DELBA_SENT,
					     INITIATOR_BIT(pdel_ba->
							   del_ba_param_set));

		ptx_ba_tbl = wlan_11n_get_txbastream_status(priv,
							    BA_STREAM_SETUP_INPROGRESS);
		if (ptx_ba_tbl) {
			wlan_send_addba(priv, ptx_ba_tbl->tid, ptx_ba_tbl->ra);
		}
	} else {		/*
				 * In case of failure, recreate the deleted stream in
				 * case we initiated the ADDBA
				 */
		if (INITIATOR_BIT(pdel_ba->del_ba_param_set)) {
			wlan_11n_create_txbastream_tbl(priv,
						       pdel_ba->peer_mac_addr,
						       tid,
						       BA_STREAM_SETUP_INPROGRESS);
			ptx_ba_tbl =
				wlan_11n_get_txbastream_status(priv,
							       BA_STREAM_SETUP_INPROGRESS);
			if (ptx_ba_tbl) {
				mlan_11n_delete_bastream_tbl(priv,
							     ptx_ba_tbl->tid,
							     ptx_ba_tbl->ra,
							     TYPE_DELBA_SENT,
							     MTRUE);
			}
		}
	}

	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function handles the command response of
 *              add a block ack request
 *
 *  @param priv    A pointer to mlan_private structure
 *  @param resp    A pointer to HostCmd_DS_COMMAND
 *
 *  @return        MLAN_STATUS_SUCCESS
 */
mlan_status
wlan_ret_11n_addba_req(mlan_private * priv, HostCmd_DS_COMMAND * resp)
{
	t_u8 tid;
	HostCmd_DS_11N_ADDBA_RSP *padd_ba_rsp =
		(HostCmd_DS_11N_ADDBA_RSP *) & resp->params.add_ba_rsp;
	TxBAStreamTbl *ptx_ba_tbl;
	raListTbl *ra_list = MNULL;

	ENTER();

	padd_ba_rsp->block_ack_param_set =
		wlan_le16_to_cpu(padd_ba_rsp->block_ack_param_set);
	padd_ba_rsp->block_ack_tmo =
		wlan_le16_to_cpu(padd_ba_rsp->block_ack_tmo);
	padd_ba_rsp->ssn = (wlan_le16_to_cpu(padd_ba_rsp->ssn)) & SSN_MASK;
	padd_ba_rsp->status_code = wlan_le16_to_cpu(padd_ba_rsp->status_code);

	tid = (padd_ba_rsp->block_ack_param_set & BLOCKACKPARAM_TID_MASK)
		>> BLOCKACKPARAM_TID_POS;
	if (padd_ba_rsp->status_code == BA_RESULT_SUCCESS) {
		ptx_ba_tbl = wlan_11n_get_txbastream_tbl(priv, tid,
							 padd_ba_rsp->
							 peer_mac_addr);
		if (ptx_ba_tbl) {
			PRINTM(MCMND,
			       "ADDBA REQ: " MACSTR
			       " tid=%d ssn=%d win_size=%d,amsdu=%d\n",
			       MAC2STR(padd_ba_rsp->peer_mac_addr), tid,
			       padd_ba_rsp->ssn,
			       ((padd_ba_rsp->
				 block_ack_param_set &
				 BLOCKACKPARAM_WINSIZE_MASK) >>
				BLOCKACKPARAM_WINSIZE_POS),
			       padd_ba_rsp->
			       block_ack_param_set &
			       BLOCKACKPARAM_AMSDU_SUPP_MASK);
			ptx_ba_tbl->ba_status = BA_STREAM_SETUP_COMPLETE;
			if ((padd_ba_rsp->
			     block_ack_param_set &
			     BLOCKACKPARAM_AMSDU_SUPP_MASK) &&
			    priv->add_ba_param.tx_amsdu &&
			    (priv->aggr_prio_tbl[tid].amsdu !=
			     BA_STREAM_NOT_ALLOWED))
				ptx_ba_tbl->amsdu = MTRUE;
			else
				ptx_ba_tbl->amsdu = MFALSE;
		} else {
			PRINTM(MERROR, "BA stream not created\n");
		}
	} else {
		mlan_11n_delete_bastream_tbl(priv, tid,
					     padd_ba_rsp->peer_mac_addr,
					     TYPE_DELBA_SENT, MTRUE);
		if (padd_ba_rsp->add_rsp_result != BA_RESULT_TIMEOUT) {
#ifdef UAP_SUPPORT
			if (GET_BSS_ROLE(priv) == MLAN_BSS_ROLE_UAP)
				disable_station_ampdu(priv, tid,
						      padd_ba_rsp->
						      peer_mac_addr);
#endif /* UAP_SUPPORT */
			priv->aggr_prio_tbl[tid].ampdu_ap =
				BA_STREAM_NOT_ALLOWED;

		} else {
			/* reset packet threshold */
			ra_list =
				wlan_wmm_get_ralist_node(priv, tid,
							 padd_ba_rsp->
							 peer_mac_addr);
			if (ra_list) {
				ra_list->packet_count = 0;
				ra_list->ba_packet_threshold =
					wlan_get_random_ba_threshold(priv->
								     adapter);
			}
		}
	}

	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function prepares command of reconfigure tx buf
 *
 *  @param priv         A pointer to mlan_private structure
 *  @param cmd          A pointer to HostCmd_DS_COMMAND structure
 *  @param cmd_action   The action: GET or SET
 *  @param pdata_buf    A pointer to data buffer
 *
 *  @return             MLAN_STATUS_SUCCESS
 */
mlan_status
wlan_cmd_recfg_tx_buf(mlan_private * priv,
		      HostCmd_DS_COMMAND * cmd, int cmd_action, void *pdata_buf)
{
	HostCmd_DS_TXBUF_CFG *ptx_buf = &cmd->params.tx_buf;
	t_u16 action = (t_u16) cmd_action;
	t_u16 buf_size = *((t_u16 *) pdata_buf);

	ENTER();
	cmd->command = wlan_cpu_to_le16(HostCmd_CMD_RECONFIGURE_TX_BUFF);
	cmd->size = wlan_cpu_to_le16(sizeof(HostCmd_DS_TXBUF_CFG)
				     + S_DS_GEN);
	ptx_buf->action = wlan_cpu_to_le16(action);
	switch (action) {
	case HostCmd_ACT_GEN_SET:
		PRINTM(MCMND, "set tx_buf = %d\n", buf_size);
		ptx_buf->buff_size = wlan_cpu_to_le16(buf_size);
		break;
	case HostCmd_ACT_GEN_GET:
	default:
		ptx_buf->buff_size = 0;
		break;
	}
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function prepares command of amsdu aggr control
 *
 *  @param priv         A pointer to mlan_private structure
 *  @param cmd          A pointer to HostCmd_DS_COMMAND structure
 *  @param cmd_action   The action: GET or SET
 *  @param pdata_buf    A pointer to data buffer
 *
 *  @return             MLAN_STATUS_SUCCESS
 */
mlan_status
wlan_cmd_amsdu_aggr_ctrl(mlan_private * priv,
			 HostCmd_DS_COMMAND * cmd,
			 int cmd_action, void *pdata_buf)
{
	HostCmd_DS_AMSDU_AGGR_CTRL *pamsdu_ctrl = &cmd->params.amsdu_aggr_ctrl;
	t_u16 action = (t_u16) cmd_action;
	mlan_ds_11n_amsdu_aggr_ctrl *aa_ctrl = (mlan_ds_11n_amsdu_aggr_ctrl *)
		pdata_buf;

	ENTER();

	cmd->command = wlan_cpu_to_le16(HostCmd_CMD_AMSDU_AGGR_CTRL);
	cmd->size = wlan_cpu_to_le16(sizeof(HostCmd_DS_AMSDU_AGGR_CTRL)
				     + S_DS_GEN);
	pamsdu_ctrl->action = wlan_cpu_to_le16(action);
	switch (action) {
	case HostCmd_ACT_GEN_SET:
		pamsdu_ctrl->enable = wlan_cpu_to_le16(aa_ctrl->enable);
		pamsdu_ctrl->curr_buf_size = 0;
		break;
	case HostCmd_ACT_GEN_GET:
	default:
		pamsdu_ctrl->curr_buf_size = 0;
		break;
	}
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function handles the command response of amsdu aggr ctrl
 *
 *  @param pmpriv       A pointer to mlan_private structure
 *  @param resp         A pointer to HostCmd_DS_COMMAND
 *  @param pioctl_buf   A pointer to mlan_ioctl_req structure
 *
 *  @return        MLAN_STATUS_SUCCESS
 */
mlan_status
wlan_ret_amsdu_aggr_ctrl(IN pmlan_private pmpriv,
			 IN HostCmd_DS_COMMAND * resp,
			 IN mlan_ioctl_req * pioctl_buf)
{
	mlan_ds_11n_cfg *cfg = MNULL;
	HostCmd_DS_AMSDU_AGGR_CTRL *amsdu_ctrl = &resp->params.amsdu_aggr_ctrl;

	ENTER();

	if (pioctl_buf) {
		cfg = (mlan_ds_11n_cfg *) pioctl_buf->pbuf;
		cfg->param.amsdu_aggr_ctrl.enable =
			wlan_le16_to_cpu(amsdu_ctrl->enable);
		cfg->param.amsdu_aggr_ctrl.curr_buf_size =
			wlan_le16_to_cpu(amsdu_ctrl->curr_buf_size);
	}
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function prepares 11n cfg command
 *
 *  @param pmpriv    	A pointer to mlan_private structure
 *  @param cmd	   	A pointer to HostCmd_DS_COMMAND structure
 *  @param cmd_action 	the action: GET or SET
 *  @param pdata_buf 	A pointer to data buffer
 *  @return 	   	MLAN_STATUS_SUCCESS
 */
mlan_status
wlan_cmd_11n_cfg(IN pmlan_private pmpriv,
		 IN HostCmd_DS_COMMAND * cmd,
		 IN t_u16 cmd_action, IN t_void * pdata_buf)
{
	HostCmd_DS_11N_CFG *htcfg = &cmd->params.htcfg;
	mlan_ds_11n_tx_cfg *txcfg = (mlan_ds_11n_tx_cfg *) pdata_buf;

	ENTER();
	cmd->command = wlan_cpu_to_le16(HostCmd_CMD_11N_CFG);
	cmd->size = wlan_cpu_to_le16(sizeof(HostCmd_DS_11N_CFG) + S_DS_GEN);
	htcfg->action = wlan_cpu_to_le16(cmd_action);
	htcfg->ht_tx_cap = wlan_cpu_to_le16(txcfg->httxcap);
	htcfg->ht_tx_info = wlan_cpu_to_le16(txcfg->httxinfo);
	htcfg->misc_config = wlan_cpu_to_le16(txcfg->misc_cfg);
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function handles the command response of 11ncfg
 *
 *  @param pmpriv       A pointer to mlan_private structure
 *  @param resp         A pointer to HostCmd_DS_COMMAND
 *  @param pioctl_buf   A pointer to mlan_ioctl_req structure
 *
 *  @return        MLAN_STATUS_SUCCESS
 */
mlan_status
wlan_ret_11n_cfg(IN pmlan_private pmpriv,
		 IN HostCmd_DS_COMMAND * resp, IN mlan_ioctl_req * pioctl_buf)
{
	mlan_ds_11n_cfg *cfg = MNULL;
	HostCmd_DS_11N_CFG *htcfg = &resp->params.htcfg;

	ENTER();
	if (pioctl_buf &&
	    (wlan_le16_to_cpu(htcfg->action) == HostCmd_ACT_GEN_GET)) {
		cfg = (mlan_ds_11n_cfg *) pioctl_buf->pbuf;
		cfg->param.tx_cfg.httxcap = wlan_le16_to_cpu(htcfg->ht_tx_cap);
		cfg->param.tx_cfg.httxinfo =
			wlan_le16_to_cpu(htcfg->ht_tx_info);
		cfg->param.tx_cfg.misc_cfg =
			wlan_le16_to_cpu(htcfg->misc_config);
	}
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function prepares reject addba req command
 *
 *  @param pmpriv    	A pointer to mlan_private structure
 *  @param cmd	   	A pointer to HostCmd_DS_COMMAND structure
 *  @param cmd_action 	the action: GET or SET
 *  @param pdata_buf 	A pointer to data buffer
 *  @return 	   	MLAN_STATUS_SUCCESS
 */
mlan_status
wlan_cmd_reject_addba_req(IN pmlan_private pmpriv,
			  IN HostCmd_DS_COMMAND * cmd,
			  IN t_u16 cmd_action, IN t_void * pdata_buf)
{
	HostCmd_DS_REJECT_ADDBA_REQ *pRejectAddbaReq =
		&cmd->params.rejectaddbareq;
	mlan_ds_reject_addba_req *prejaddbareq =
		(mlan_ds_reject_addba_req *) pdata_buf;

	ENTER();
	cmd->command = wlan_cpu_to_le16(HostCmd_CMD_REJECT_ADDBA_REQ);
	cmd->size =
		wlan_cpu_to_le16(sizeof(HostCmd_DS_REJECT_ADDBA_REQ) +
				 S_DS_GEN);
	pRejectAddbaReq->action = wlan_cpu_to_le16(cmd_action);
	pRejectAddbaReq->conditions =
		wlan_cpu_to_le32(prejaddbareq->conditions);
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function handles the command response of reject addba req
 *
 *  @param pmpriv       A pointer to mlan_private structure
 *  @param resp         A pointer to HostCmd_DS_COMMAND
 *  @param pioctl_buf   A pointer to mlan_ioctl_req structure
 *
 *  @return        MLAN_STATUS_SUCCESS
 */
mlan_status
wlan_ret_reject_addba_req(IN pmlan_private pmpriv,
			  IN HostCmd_DS_COMMAND * resp,
			  IN mlan_ioctl_req * pioctl_buf)
{
	mlan_ds_11n_cfg *cfg = MNULL;
	HostCmd_DS_REJECT_ADDBA_REQ *pRejectAddbaReq =
		&resp->params.rejectaddbareq;

	ENTER();
	if (pioctl_buf &&
	    (wlan_le16_to_cpu(pRejectAddbaReq->action) ==
	     HostCmd_ACT_GEN_GET)) {
		cfg = (mlan_ds_11n_cfg *) pioctl_buf->pbuf;
		cfg->param.reject_addba_req.conditions =
			wlan_le32_to_cpu(pRejectAddbaReq->conditions);
	}
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function prepares TX BF configuration command
 *
 *  @param pmpriv       A pointer to mlan_private structure
 *  @param cmd          A pointer to HostCmd_DS_COMMAND structure
 *  @param cmd_action   The action: GET or SET
 *  @param pdata_buf    A pointer to data buffer
 *  @return             MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
wlan_cmd_tx_bf_cfg(IN pmlan_private pmpriv,
		   IN HostCmd_DS_COMMAND * cmd,
		   IN t_u16 cmd_action, IN t_void * pdata_buf)
{
	pmlan_adapter pmadapter = pmpriv->adapter;
	HostCmd_DS_TX_BF_CFG *txbfcfg = &cmd->params.tx_bf_cfg;
	mlan_ds_11n_tx_bf_cfg *txbf = (mlan_ds_11n_tx_bf_cfg *) pdata_buf;

	ENTER();

	cmd->command = wlan_cpu_to_le16(HostCmd_CMD_TX_BF_CFG);
	cmd->size = wlan_cpu_to_le16(sizeof(HostCmd_DS_TX_BF_CFG) + S_DS_GEN);

	if (txbf->bf_action == SET_GET_BF_PERIODICITY) {
		memcpy(pmadapter, txbfcfg->body.bf_periodicity.peer_mac,
		       txbf->body.bf_periodicity[0].peer_mac,
		       MLAN_MAC_ADDR_LENGTH);
	}
	txbfcfg->action = wlan_cpu_to_le16(txbf->action);
	txbfcfg->bf_action = wlan_cpu_to_le16(txbf->bf_action);
	if (cmd_action == HostCmd_ACT_GEN_SET) {
		switch (txbf->bf_action) {
		case BF_GLOBAL_CONFIGURATION:
			txbfcfg->body.bf_global_cfg.bf_enbl =
				txbf->body.bf_global_cfg.bf_enbl;
			txbfcfg->body.bf_global_cfg.sounding_enbl =
				txbf->body.bf_global_cfg.sounding_enbl;
			txbfcfg->body.bf_global_cfg.fb_type =
				txbf->body.bf_global_cfg.fb_type;
			txbfcfg->body.bf_global_cfg.snr_threshold =
				txbf->body.bf_global_cfg.snr_threshold;
			txbfcfg->body.bf_global_cfg.sounding_interval =
				wlan_cpu_to_le16(txbf->body.bf_global_cfg.
						 sounding_interval);
			txbfcfg->body.bf_global_cfg.bf_mode =
				txbf->body.bf_global_cfg.bf_mode;
			break;
		case TRIGGER_SOUNDING_FOR_PEER:
			memcpy(pmadapter, txbfcfg->body.bf_sound_args.peer_mac,
			       txbf->body.bf_sound[0].peer_mac,
			       MLAN_MAC_ADDR_LENGTH);
			break;
		case SET_GET_BF_PERIODICITY:
			txbfcfg->body.bf_periodicity.interval =
				wlan_cpu_to_le16(txbf->body.bf_periodicity->
						 interval);
			break;
		case TX_BF_FOR_PEER_ENBL:
			memcpy(pmadapter, txbfcfg->body.tx_bf_peer.peer_mac,
			       txbf->body.tx_bf_peer[0].peer_mac,
			       MLAN_MAC_ADDR_LENGTH);
			txbfcfg->body.tx_bf_peer.bf_enbl =
				txbf->body.tx_bf_peer[0].bf_enbl;
			txbfcfg->body.tx_bf_peer.sounding_enbl =
				txbf->body.tx_bf_peer[0].sounding_enbl;
			txbfcfg->body.tx_bf_peer.fb_type =
				txbf->body.tx_bf_peer[0].fb_type;
			break;
		case SET_SNR_THR_PEER:
			memcpy(pmadapter, txbfcfg->body.bf_snr.peer_mac,
			       txbf->body.bf_snr[0].peer_mac,
			       MLAN_MAC_ADDR_LENGTH);
			txbfcfg->body.bf_snr.snr = txbf->body.bf_snr[0].snr;
			break;
		default:
			LEAVE();
			return MLAN_STATUS_FAILURE;
		}
	}

	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief This function handles the command response
 *  of TX BF configuration
 *
 *  @param pmpriv       A pointer to mlan_private structure
 *  @param resp         A pointer to HostCmd_DS_COMMAND
 *  @param pioctl_buf   A pointer to mlan_ioctl_req structure
 *
 *  @return        MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
wlan_ret_tx_bf_cfg(IN pmlan_private pmpriv,
		   IN HostCmd_DS_COMMAND * resp, IN mlan_ioctl_req * pioctl_buf)
{
	pmlan_adapter pmadapter = pmpriv->adapter;
	HostCmd_DS_TX_BF_CFG *txbfcfg = &resp->params.tx_bf_cfg;
	mlan_ds_11n_cfg *cfg_11n = MNULL;
	mlan_ds_11n_tx_bf_cfg *txbf = MNULL;
	bf_peer_args *tx_bf_peer;
	bf_snr_thr_t *bf_snr;
	int i;

	ENTER();

	if (pioctl_buf) {
		cfg_11n = (mlan_ds_11n_cfg *) pioctl_buf->pbuf;
		txbf = (mlan_ds_11n_tx_bf_cfg *) & cfg_11n->param.tx_bf;
		txbf->bf_action = wlan_le16_to_cpu(txbfcfg->bf_action);
		switch (txbf->bf_action) {
		case BF_GLOBAL_CONFIGURATION:
			txbf->body.bf_global_cfg.bf_enbl =
				txbfcfg->body.bf_global_cfg.bf_enbl;
			txbf->body.bf_global_cfg.sounding_enbl =
				txbfcfg->body.bf_global_cfg.sounding_enbl;
			txbf->body.bf_global_cfg.fb_type =
				txbfcfg->body.bf_global_cfg.fb_type;
			txbf->body.bf_global_cfg.snr_threshold =
				txbfcfg->body.bf_global_cfg.snr_threshold;
			txbf->body.bf_global_cfg.sounding_interval =
				wlan_le16_to_cpu(txbfcfg->body.bf_global_cfg.
						 sounding_interval);
			txbf->body.bf_global_cfg.bf_mode =
				txbfcfg->body.bf_global_cfg.bf_mode;
			break;
		case TRIGGER_SOUNDING_FOR_PEER:
			memcpy(pmadapter, txbf->body.bf_sound[0].peer_mac,
			       txbfcfg->body.bf_sound_args.peer_mac,
			       MLAN_MAC_ADDR_LENGTH);
			txbf->body.bf_sound[0].status =
				txbfcfg->body.bf_sound_args.status;
			break;
		case SET_GET_BF_PERIODICITY:
			memcpy(pmadapter, txbf->body.bf_periodicity->peer_mac,
			       txbfcfg->body.bf_periodicity.peer_mac,
			       MLAN_MAC_ADDR_LENGTH);
			txbf->body.bf_periodicity->interval =
				wlan_le16_to_cpu(txbfcfg->body.bf_periodicity.
						 interval);
			break;
		case TX_BF_FOR_PEER_ENBL:
			txbf->no_of_peers = *(t_u8 *) & txbfcfg->body;
			tx_bf_peer =
				(bf_peer_args *) ((t_u8 *) & txbfcfg->body +
						  sizeof(t_u8));
			for (i = 0; i < txbf->no_of_peers; i++) {
				memcpy(pmadapter,
				       txbf->body.tx_bf_peer[i].peer_mac,
				       (t_u8 *) tx_bf_peer->peer_mac,
				       MLAN_MAC_ADDR_LENGTH);
				txbf->body.tx_bf_peer[i].bf_enbl =
					tx_bf_peer->bf_enbl;
				txbf->body.tx_bf_peer[i].sounding_enbl =
					tx_bf_peer->sounding_enbl;
				txbf->body.tx_bf_peer[i].fb_type =
					tx_bf_peer->fb_type;
				tx_bf_peer++;
			}
			break;
		case SET_SNR_THR_PEER:
			txbf->no_of_peers = *(t_u8 *) & txbfcfg->body;
			bf_snr = (bf_snr_thr_t *) ((t_u8 *) & txbfcfg->body +
						   sizeof(t_u8));
			for (i = 0; i < txbf->no_of_peers; i++) {
				memcpy(pmadapter, txbf->body.bf_snr[i].peer_mac,
				       (t_u8 *) bf_snr->peer_mac,
				       MLAN_MAC_ADDR_LENGTH);
				txbf->body.bf_snr[i].snr = bf_snr->snr;
				bf_snr++;
			}
			break;
		default:
			LEAVE();
			return MLAN_STATUS_FAILURE;
		}
	}

	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

#ifdef STA_SUPPORT

/**
 *  @brief This function append the 802_11N tlv
 *
 *  @param pmpriv       A pointer to mlan_private structure
 *  @param pbss_desc    A pointer to BSSDescriptor_t structure
 *  @param ppbuffer     A Pointer to command buffer pointer
 *
 *  @return bytes added to the buffer
 */
int
wlan_cmd_append_11n_tlv(IN mlan_private * pmpriv,
			IN BSSDescriptor_t * pbss_desc, OUT t_u8 ** ppbuffer)
{
	pmlan_adapter pmadapter = pmpriv->adapter;
	MrvlIETypes_HTCap_t *pht_cap;
	MrvlIETypes_HTInfo_t *pht_info;
	MrvlIEtypes_ChanListParamSet_t *pchan_list;
	MrvlIETypes_2040BSSCo_t *p2040_bss_co;
	MrvlIETypes_ExtCap_t *pext_cap;
	t_u32 usr_dot_11n_dev_cap;
	int ret_len = 0;

	ENTER();

	/* Null Checks */
	if (ppbuffer == MNULL) {
		LEAVE();
		return 0;
	}
	if (*ppbuffer == MNULL) {
		LEAVE();
		return 0;
	}

	if (pbss_desc->bss_band & BAND_A)
		usr_dot_11n_dev_cap = pmadapter->usr_dot_11n_dev_cap_a;
	else
		usr_dot_11n_dev_cap = pmadapter->usr_dot_11n_dev_cap_bg;

	if (pbss_desc->pht_cap) {
		pht_cap = (MrvlIETypes_HTCap_t *) * ppbuffer;
		memset(pmadapter, pht_cap, 0, sizeof(MrvlIETypes_HTCap_t));
		pht_cap->header.type = wlan_cpu_to_le16(HT_CAPABILITY);
		pht_cap->header.len = sizeof(HTCap_t);
		memcpy(pmadapter,
		       (t_u8 *) pht_cap + sizeof(MrvlIEtypesHeader_t),
		       (t_u8 *) pbss_desc->pht_cap + sizeof(IEEEtypes_Header_t),
		       pht_cap->header.len);

		pht_cap->ht_cap.ht_cap_info =
			wlan_le16_to_cpu(pht_cap->ht_cap.ht_cap_info);
		pht_cap->ht_cap.ht_ext_cap =
			wlan_le16_to_cpu(pht_cap->ht_cap.ht_ext_cap);
		wlan_fill_ht_cap_tlv(pmpriv, pht_cap, pbss_desc->bss_band);

		HEXDUMP("HT_CAPABILITIES IE", (t_u8 *) pht_cap,
			sizeof(MrvlIETypes_HTCap_t));
		*ppbuffer += sizeof(MrvlIETypes_HTCap_t);
		ret_len += sizeof(MrvlIETypes_HTCap_t);
		pht_cap->header.len = wlan_cpu_to_le16(pht_cap->header.len);
	}

	if (pbss_desc->pht_info) {
		if (pmpriv->bss_mode == MLAN_BSS_MODE_IBSS) {
			pht_info = (MrvlIETypes_HTInfo_t *) * ppbuffer;
			memset(pmadapter, pht_info, 0,
			       sizeof(MrvlIETypes_HTInfo_t));
			pht_info->header.type = wlan_cpu_to_le16(HT_OPERATION);
			pht_info->header.len = sizeof(HTInfo_t);

			memcpy(pmadapter,
			       (t_u8 *) pht_info + sizeof(MrvlIEtypesHeader_t),
			       (t_u8 *) pbss_desc->pht_info +
			       sizeof(IEEEtypes_Header_t),
			       pht_info->header.len);

			if (!ISSUPP_CHANWIDTH40(usr_dot_11n_dev_cap)) {
				RESET_CHANWIDTH40(pht_info->ht_info.field2);
			}

			*ppbuffer += sizeof(MrvlIETypes_HTInfo_t);
			ret_len += sizeof(MrvlIETypes_HTInfo_t);
			pht_info->header.len =
				wlan_cpu_to_le16(pht_info->header.len);
		}

		pchan_list = (MrvlIEtypes_ChanListParamSet_t *) * ppbuffer;
		memset(pmadapter, pchan_list, 0,
		       sizeof(MrvlIEtypes_ChanListParamSet_t));
		pchan_list->header.type = wlan_cpu_to_le16(TLV_TYPE_CHANLIST);
		pchan_list->header.len =
			sizeof(MrvlIEtypes_ChanListParamSet_t) -
			sizeof(MrvlIEtypesHeader_t);
		pchan_list->chan_scan_param[0].chan_number =
			pbss_desc->pht_info->ht_info.pri_chan;
		pchan_list->chan_scan_param[0].radio_type =
			wlan_band_to_radio_type((t_u8) pbss_desc->bss_band);
		if (ISSUPP_CHANWIDTH40(usr_dot_11n_dev_cap) &&
		    ISALLOWED_CHANWIDTH40(pbss_desc->pht_info->ht_info.
					  field2)) {
			SET_SECONDARYCHAN(pchan_list->chan_scan_param[0].
					  radio_type,
					  GET_SECONDARYCHAN(pbss_desc->
							    pht_info->ht_info.
							    field2));
		}

		HEXDUMP("ChanList", (t_u8 *) pchan_list,
			sizeof(MrvlIEtypes_ChanListParamSet_t));
		HEXDUMP("pht_info", (t_u8 *) pbss_desc->pht_info,
			sizeof(MrvlIETypes_HTInfo_t) - 2);
		*ppbuffer += sizeof(MrvlIEtypes_ChanListParamSet_t);
		ret_len += sizeof(MrvlIEtypes_ChanListParamSet_t);
		pchan_list->header.len =
			wlan_cpu_to_le16(pchan_list->header.len);
	}

	if (pbss_desc->pbss_co_2040) {
		p2040_bss_co = (MrvlIETypes_2040BSSCo_t *) * ppbuffer;
		memset(pmadapter, p2040_bss_co, 0,
		       sizeof(MrvlIETypes_2040BSSCo_t));
		p2040_bss_co->header.type = wlan_cpu_to_le16(BSSCO_2040);
		p2040_bss_co->header.len = sizeof(BSSCo2040_t);

		memcpy(pmadapter,
		       (t_u8 *) p2040_bss_co + sizeof(MrvlIEtypesHeader_t),
		       (t_u8 *) pbss_desc->pbss_co_2040 +
		       sizeof(IEEEtypes_Header_t), p2040_bss_co->header.len);

		HEXDUMP("20/40 BSS Coexistence IE", (t_u8 *) p2040_bss_co,
			sizeof(MrvlIETypes_2040BSSCo_t));
		*ppbuffer += sizeof(MrvlIETypes_2040BSSCo_t);
		ret_len += sizeof(MrvlIETypes_2040BSSCo_t);
		p2040_bss_co->header.len =
			wlan_cpu_to_le16(p2040_bss_co->header.len);
	}

	if (pbss_desc->pext_cap) {
		pext_cap = (MrvlIETypes_ExtCap_t *) * ppbuffer;
		memset(pmadapter, pext_cap, 0, sizeof(MrvlIETypes_ExtCap_t));
		pext_cap->header.type = wlan_cpu_to_le16(EXT_CAPABILITY);
		pext_cap->header.len = sizeof(ExtCap_t);

		memcpy(pmadapter,
		       (t_u8 *) pext_cap + sizeof(MrvlIEtypesHeader_t),
		       (t_u8 *) pbss_desc->pext_cap +
		       sizeof(IEEEtypes_Header_t),
		       pbss_desc->pext_cap->ieee_hdr.len);
		if (!ISSUPP_EXTCAP_TDLS(pmpriv->ext_cap))
			RESET_EXTCAP_TDLS(pext_cap->ext_cap);
		if (!ISSUPP_EXTCAP_INTERWORKING(pmpriv->ext_cap))
			RESET_EXTCAP_INTERWORKING(pext_cap->ext_cap);
		HEXDUMP("Extended Capabilities IE", (t_u8 *) pext_cap,
			sizeof(MrvlIETypes_ExtCap_t));
		*ppbuffer += sizeof(MrvlIETypes_ExtCap_t);
		ret_len += sizeof(MrvlIETypes_ExtCap_t);
		pext_cap->header.len = wlan_cpu_to_le16(pext_cap->header.len);
	}
	LEAVE();
	return ret_len;
}

#endif /* STA_SUPPORT */

/**
 *  @brief 11n configuration handler
 *
 *  @param pmadapter	A pointer to mlan_adapter structure
 *  @param pioctl_req	A pointer to ioctl request buffer
 *
 *  @return		MLAN_STATUS_SUCCESS --success, otherwise fail
 */
mlan_status
wlan_11n_cfg_ioctl(IN pmlan_adapter pmadapter, IN pmlan_ioctl_req pioctl_req)
{
	mlan_status status = MLAN_STATUS_SUCCESS;
	mlan_ds_11n_cfg *cfg = MNULL;

	ENTER();

	if (pioctl_req->buf_len < sizeof(mlan_ds_11n_cfg)) {
		PRINTM(MINFO, "MLAN bss IOCTL length is too short.\n");
		pioctl_req->data_read_written = 0;
		pioctl_req->buf_len_needed = sizeof(mlan_ds_11n_cfg);
		pioctl_req->status_code = MLAN_ERROR_INVALID_PARAMETER;
		LEAVE();
		return MLAN_STATUS_RESOURCE;
	}
	cfg = (mlan_ds_11n_cfg *) pioctl_req->pbuf;
	switch (cfg->sub_command) {
	case MLAN_OID_11N_CFG_TX:
		status = wlan_11n_ioctl_httxcfg(pmadapter, pioctl_req);
		break;
	case MLAN_OID_11N_HTCAP_CFG:
		status = wlan_11n_ioctl_htusrcfg(pmadapter, pioctl_req);
		break;
	case MLAN_OID_11N_CFG_AGGR_PRIO_TBL:
		status = wlan_11n_ioctl_aggr_prio_tbl(pmadapter, pioctl_req);
		break;
	case MLAN_OID_11N_CFG_ADDBA_REJECT:
		status = wlan_11n_ioctl_addba_reject(pmadapter, pioctl_req);
		break;
	case MLAN_OID_11N_CFG_ADDBA_PARAM:
		status = wlan_11n_ioctl_addba_param(pmadapter, pioctl_req);
		break;
	case MLAN_OID_11N_CFG_DELBA:
		status = wlan_11n_ioctl_delba(pmadapter, pioctl_req);
		break;
	case MLAN_OID_11N_CFG_REJECT_ADDBA_REQ:
		status = wlan_11n_ioctl_rejectaddbareq(pmadapter, pioctl_req);
		break;
	case MLAN_OID_11N_CFG_MAX_TX_BUF_SIZE:
		status = wlan_11n_ioctl_max_tx_buf_size(pmadapter, pioctl_req);
		break;
	case MLAN_OID_11N_CFG_AMSDU_AGGR_CTRL:
		status = wlan_11n_ioctl_amsdu_aggr_ctrl(pmadapter, pioctl_req);
		break;
	case MLAN_OID_11N_CFG_SUPPORTED_MCS_SET:
		status = wlan_11n_ioctl_supported_mcs_set(pmadapter,
							  pioctl_req);
		break;
	case MLAN_OID_11N_CFG_TX_BF_CAP:
		status = wlan_11n_ioctl_tx_bf_cap(pmadapter, pioctl_req);
		break;
	case MLAN_OID_11N_CFG_TX_BF_CFG:
		status = wlan_11n_ioctl_tx_bf_cfg(pmadapter, pioctl_req);
		break;
	case MLAN_OID_11N_CFG_STREAM_CFG:
		status = wlan_11n_ioctl_stream_cfg(pmadapter, pioctl_req);
		break;
	default:
		pioctl_req->status_code = MLAN_ERROR_IOCTL_INVALID;
		status = MLAN_STATUS_FAILURE;
		break;
	}
	LEAVE();
	return status;
}

/**
 *  @brief This function will delete the given entry in Tx BA Stream table
 *
 *  @param priv    	Pointer to mlan_private
 *  @param ptx_tbl	Pointer to tx ba stream entry to delete
 *
 *  @return 	        N/A
 */
void
wlan_11n_delete_txbastream_tbl_entry(mlan_private * priv,
				     TxBAStreamTbl * ptx_tbl)
{
	pmlan_adapter pmadapter = priv->adapter;

	ENTER();

	pmadapter->callbacks.moal_spin_lock(pmadapter->pmoal_handle,
					    priv->tx_ba_stream_tbl_ptr.plock);

	if (!ptx_tbl || !wlan_is_txbastreamptr_valid(priv, ptx_tbl)) {
		goto exit;
	}

	PRINTM(MINFO, "Delete BA stream table entry: %p\n", ptx_tbl);

	util_unlink_list(pmadapter->pmoal_handle, &priv->tx_ba_stream_tbl_ptr,
			 (pmlan_linked_list) ptx_tbl, MNULL, MNULL);

	pmadapter->callbacks.moal_mfree(pmadapter->pmoal_handle,
					(t_u8 *) ptx_tbl);

exit:
	pmadapter->callbacks.moal_spin_unlock(pmadapter->pmoal_handle,
					      priv->tx_ba_stream_tbl_ptr.plock);
	LEAVE();
}

/**
 *  @brief This function will delete all the entries in Tx BA Stream table
 *
 *  @param priv         A pointer to mlan_private
 *
 *  @return             N/A
 */
void
wlan_11n_deleteall_txbastream_tbl(mlan_private * priv)
{
	int i;
	TxBAStreamTbl *del_tbl_ptr = MNULL;

	ENTER();

	while ((del_tbl_ptr = (TxBAStreamTbl *)
		util_peek_list(priv->adapter->pmoal_handle,
			       &priv->tx_ba_stream_tbl_ptr,
			       priv->adapter->callbacks.moal_spin_lock,
			       priv->adapter->callbacks.moal_spin_unlock))) {
		wlan_11n_delete_txbastream_tbl_entry(priv, del_tbl_ptr);
	}

	util_init_list((pmlan_linked_list) & priv->tx_ba_stream_tbl_ptr);

	for (i = 0; i < MAX_NUM_TID; ++i) {
		priv->aggr_prio_tbl[i].ampdu_ap =
			priv->aggr_prio_tbl[i].ampdu_user;
	}

	LEAVE();
}

/**
 *  @brief This function will return the pointer to an entry in BA Stream
 *  		table which matches the give RA/TID pair
 *
 *  @param priv    A pointer to mlan_private
 *  @param tid	   TID to find in reordering table
 *  @param ra      RA to find in reordering table
 *
 *  @return 	   A pointer to first entry matching RA/TID in BA stream
 *                 NULL if not found
 */
TxBAStreamTbl *
wlan_11n_get_txbastream_tbl(mlan_private * priv, int tid, t_u8 * ra)
{
	TxBAStreamTbl *ptx_tbl;
	pmlan_adapter pmadapter = priv->adapter;

	ENTER();

	ptx_tbl = (TxBAStreamTbl *) util_peek_list(pmadapter->pmoal_handle,
						   &priv->tx_ba_stream_tbl_ptr,
						   pmadapter->callbacks.
						   moal_spin_lock,
						   pmadapter->callbacks.
						   moal_spin_unlock);
	if (!ptx_tbl) {
		LEAVE();
		return MNULL;
	}

	while (ptx_tbl != (TxBAStreamTbl *) & priv->tx_ba_stream_tbl_ptr) {

		PRINTM(MDAT_D, "get_txbastream_tbl TID %d\n", ptx_tbl->tid);
		DBG_HEXDUMP(MDAT_D, "RA", ptx_tbl->ra, MLAN_MAC_ADDR_LENGTH);

		if ((!memcmp(pmadapter, ptx_tbl->ra, ra, MLAN_MAC_ADDR_LENGTH))
		    && (ptx_tbl->tid == tid)) {
			LEAVE();
			return ptx_tbl;
		}

		ptx_tbl = ptx_tbl->pnext;
	}

	LEAVE();
	return MNULL;
}

/**
 *  @brief This function will create a entry in tx ba stream table for the
 *  		given RA/TID.
 *
 *  @param priv      A pointer to mlan_private
 *  @param ra        RA to find in reordering table
 *  @param tid	     TID to find in reordering table
 *  @param ba_status BA stream status to create the stream with
 *
 *  @return 	    N/A
 */
void
wlan_11n_create_txbastream_tbl(mlan_private * priv,
			       t_u8 * ra, int tid, baStatus_e ba_status)
{
	TxBAStreamTbl *newNode = MNULL;
	pmlan_adapter pmadapter = priv->adapter;

	ENTER();

	if (!wlan_11n_get_txbastream_tbl(priv, tid, ra)) {
		PRINTM(MDAT_D, "get_txbastream_tbl TID %d\n", tid);
		DBG_HEXDUMP(MDAT_D, "RA", ra, MLAN_MAC_ADDR_LENGTH);

		pmadapter->callbacks.moal_malloc(pmadapter->pmoal_handle,
						 sizeof(TxBAStreamTbl),
						 MLAN_MEM_DEF,
						 (t_u8 **) & newNode);
		util_init_list((pmlan_linked_list) newNode);

		newNode->tid = tid;
		newNode->ba_status = ba_status;
		memcpy(pmadapter, newNode->ra, ra, MLAN_MAC_ADDR_LENGTH);

		util_enqueue_list_tail(pmadapter->pmoal_handle,
				       &priv->tx_ba_stream_tbl_ptr,
				       (pmlan_linked_list) newNode,
				       pmadapter->callbacks.moal_spin_lock,
				       pmadapter->callbacks.moal_spin_unlock);
	}

	LEAVE();
}

/**
 *  @brief This function will send a block ack to given tid/ra
 *
 *  @param priv     A pointer to mlan_private
 *  @param tid	    TID to send the ADDBA
 *  @param peer_mac MAC address to send the ADDBA
 *
 *  @return 	    MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
int
wlan_send_addba(mlan_private * priv, int tid, t_u8 * peer_mac)
{
	HostCmd_DS_11N_ADDBA_REQ add_ba_req;
	static t_u8 dialog_tok;
	mlan_status ret;

	ENTER();

	PRINTM(MCMND, "Send addba: TID %d\n", tid);
	DBG_HEXDUMP(MCMD_D, "Send addba RA", peer_mac, MLAN_MAC_ADDR_LENGTH);

	add_ba_req.block_ack_param_set =
		(t_u16) ((tid << BLOCKACKPARAM_TID_POS) |
			 (priv->add_ba_param.
			  tx_win_size << BLOCKACKPARAM_WINSIZE_POS) |
			 IMMEDIATE_BLOCK_ACK);
    /** enable AMSDU inside AMPDU */
	if (priv->add_ba_param.tx_amsdu &&
	    (priv->aggr_prio_tbl[tid].amsdu != BA_STREAM_NOT_ALLOWED))
		add_ba_req.block_ack_param_set |= BLOCKACKPARAM_AMSDU_SUPP_MASK;
	add_ba_req.block_ack_tmo = (t_u16) priv->add_ba_param.timeout;

	++dialog_tok;

	if (dialog_tok == 0)
		dialog_tok = 1;

	add_ba_req.dialog_token = dialog_tok;
	memcpy(priv->adapter, &add_ba_req.peer_mac_addr, peer_mac,
	       MLAN_MAC_ADDR_LENGTH);

	/* We don't wait for the response of this command */
	ret = wlan_prepare_cmd(priv, HostCmd_CMD_11N_ADDBA_REQ,
			       0, 0, MNULL, &add_ba_req);

	LEAVE();
	return ret;
}

/**
 *  @brief This function will delete a block ack to given tid/ra
 *
 *  @param priv    		A pointer to mlan_private
 *  @param pioctl_req	A pointer to ioctl request buffer
 *  @param tid	   		TID to send the ADDBA
 *  @param peer_mac 	MAC address to send the ADDBA
 *  @param initiator 	MTRUE if we have initiated ADDBA, MFALSE otherwise
 *
 *  @return 	        MLAN_STATUS_PENDING --success, otherwise fail
 */
int
wlan_send_delba(mlan_private * priv, pmlan_ioctl_req pioctl_req, int tid,
		t_u8 * peer_mac, int initiator)
{
	HostCmd_DS_11N_DELBA delba;
	mlan_status ret;

	ENTER();

	memset(priv->adapter, &delba, 0, sizeof(delba));
	delba.del_ba_param_set = (tid << DELBA_TID_POS);

	if (initiator)
		DELBA_INITIATOR(delba.del_ba_param_set);
	else
		DELBA_RECIPIENT(delba.del_ba_param_set);

	memcpy(priv->adapter, &delba.peer_mac_addr, peer_mac,
	       MLAN_MAC_ADDR_LENGTH);

	ret = wlan_prepare_cmd(priv, HostCmd_CMD_11N_DELBA,
			       HostCmd_ACT_GEN_SET, 0, (t_void *) pioctl_req,
			       (t_void *) & delba);

	if (ret == MLAN_STATUS_SUCCESS)
		ret = MLAN_STATUS_PENDING;

	LEAVE();
	return ret;
}

/**
 *  @brief This function handles the command response of
 *  		delete a block ack request
 *
 *  @param priv		A pointer to mlan_private structure
 *  @param del_ba	A pointer to command response buffer
 *
 *  @return        N/A
 */
void
wlan_11n_delete_bastream(mlan_private * priv, t_u8 * del_ba)
{
	HostCmd_DS_11N_DELBA *pdel_ba = (HostCmd_DS_11N_DELBA *) del_ba;
	int tid;

	ENTER();

	DBG_HEXDUMP(MCMD_D, "Delba:", (t_u8 *) pdel_ba, 20);
	pdel_ba->del_ba_param_set = wlan_le16_to_cpu(pdel_ba->del_ba_param_set);
	pdel_ba->reason_code = wlan_le16_to_cpu(pdel_ba->reason_code);

	tid = pdel_ba->del_ba_param_set >> DELBA_TID_POS;

	mlan_11n_delete_bastream_tbl(priv, tid, pdel_ba->peer_mac_addr,
				     TYPE_DELBA_RECEIVE,
				     INITIATOR_BIT(pdel_ba->del_ba_param_set));

	LEAVE();
}

/**
 *  @brief Get Rx reordering table
 *
 *  @param priv         A pointer to mlan_private structure
 *  @param buf          A pointer to rx_reorder_tbl structure
 *  @return             number of rx reorder table entry
 */
int
wlan_get_rxreorder_tbl(mlan_private * priv, rx_reorder_tbl * buf)
{
	int i;
	rx_reorder_tbl *ptbl = buf;
	RxReorderTbl *rxReorderTblPtr;
	int count = 0;
	ENTER();
	rxReorderTblPtr =
		(RxReorderTbl *) util_peek_list(priv->adapter->pmoal_handle,
						&priv->rx_reorder_tbl_ptr,
						priv->adapter->callbacks.
						moal_spin_lock,
						priv->adapter->callbacks.
						moal_spin_unlock);
	if (!rxReorderTblPtr) {
		LEAVE();
		return count;
	}
	while (rxReorderTblPtr != (RxReorderTbl *) & priv->rx_reorder_tbl_ptr) {
		ptbl->tid = (t_u16) rxReorderTblPtr->tid;
		memcpy(priv->adapter, ptbl->ta, rxReorderTblPtr->ta,
		       MLAN_MAC_ADDR_LENGTH);
		ptbl->start_win = rxReorderTblPtr->start_win;
		ptbl->win_size = rxReorderTblPtr->win_size;
		ptbl->amsdu = rxReorderTblPtr->amsdu;
		for (i = 0; i < rxReorderTblPtr->win_size; ++i) {
			if (rxReorderTblPtr->rx_reorder_ptr[i])
				ptbl->buffer[i] = MTRUE;
			else
				ptbl->buffer[i] = MFALSE;
		}
		rxReorderTblPtr = rxReorderTblPtr->pnext;
		ptbl++;
		count++;
		if (count >= MLAN_MAX_RX_BASTREAM_SUPPORTED)
			break;
	}
	LEAVE();
	return count;
}

/**
 *  @brief Get transmit BA stream table
 *
 *  @param priv         A pointer to mlan_private structure
 *  @param buf          A pointer to tx_ba_stream_tbl structure
 *  @return             number of ba stream table entry
 */
int
wlan_get_txbastream_tbl(mlan_private * priv, tx_ba_stream_tbl * buf)
{
	TxBAStreamTbl *ptxtbl;
	tx_ba_stream_tbl *ptbl = buf;
	int count = 0;

	ENTER();

	ptxtbl = (TxBAStreamTbl *) util_peek_list(priv->adapter->pmoal_handle,
						  &priv->tx_ba_stream_tbl_ptr,
						  priv->adapter->callbacks.
						  moal_spin_lock,
						  priv->adapter->callbacks.
						  moal_spin_unlock);
	if (!ptxtbl) {
		LEAVE();
		return count;
	}

	while (ptxtbl != (TxBAStreamTbl *) & priv->tx_ba_stream_tbl_ptr) {
		ptbl->tid = (t_u16) ptxtbl->tid;
		PRINTM(MINFO, "tid=%d\n", ptbl->tid);
		memcpy(priv->adapter, ptbl->ra, ptxtbl->ra,
		       MLAN_MAC_ADDR_LENGTH);
		ptbl->amsdu = ptxtbl->amsdu;
		ptxtbl = ptxtbl->pnext;
		ptbl++;
		count++;
		if (count >= MLAN_MAX_TX_BASTREAM_SUPPORTED)
			break;
	}

	LEAVE();
	return count;
}

/**
 *  @brief This function cleans up txbastream_tbl for specific station
 *
 *  @param priv    	A pointer to mlan_private
 *  @param ra       RA to find in txbastream_tbl
 *  @return 	   	N/A
 */
void
wlan_11n_cleanup_txbastream_tbl(mlan_private * priv, t_u8 * ra)
{
	TxBAStreamTbl *ptx_tbl = MNULL;
	t_u8 i;
	ENTER();
	for (i = 0; i < MAX_NUM_TID; ++i) {
		ptx_tbl = wlan_11n_get_txbastream_tbl(priv, i, ra);
		if (ptx_tbl) {
			wlan_11n_delete_txbastream_tbl_entry(priv, ptx_tbl);
		}
	}
	LEAVE();
	return;
}
