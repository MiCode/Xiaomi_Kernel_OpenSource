/** @file mlan_shim.c
 *
 *  @brief This file contains APIs to MOAL module.
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
#include "mlan_wmm.h"
#include "mlan_sdio.h"
#ifdef UAP_SUPPORT
#include "mlan_uap.h"
#endif
#include "mlan_11h.h"

/********************************************************
                Local Variables
********************************************************/

/********************************************************
        Global Variables
********************************************************/
#ifdef STA_SUPPORT
mlan_operations mlan_sta_ops = {
	/* init cmd handler */
	wlan_ops_sta_init_cmd,
	/* ioctl handler */
	wlan_ops_sta_ioctl,
	/* cmd handler */
	wlan_ops_sta_prepare_cmd,
	/* cmdresp handler */
	wlan_ops_sta_process_cmdresp,
	/* rx handler */
	wlan_ops_sta_process_rx_packet,
	/* Event handler */
	wlan_ops_sta_process_event,
	/* txpd handler */
	wlan_ops_sta_process_txpd,
	/* BSS role: STA */
	MLAN_BSS_ROLE_STA,
};
#endif
#ifdef UAP_SUPPORT
mlan_operations mlan_uap_ops = {
	/* init cmd handler */
	wlan_ops_uap_init_cmd,
	/* ioctl handler */
	wlan_ops_uap_ioctl,
	/* cmd handler */
	wlan_ops_uap_prepare_cmd,
	/* cmdresp handler */
	wlan_ops_uap_process_cmdresp,
	/* rx handler */
	wlan_ops_uap_process_rx_packet,
	/* Event handler */
	wlan_ops_uap_process_event,
	/* txpd handler */
	wlan_ops_uap_process_txpd,
	/* BSS role: uAP */
	MLAN_BSS_ROLE_UAP,
};
#endif

/** mlan function table */
mlan_operations *mlan_ops[] = {
#ifdef STA_SUPPORT
	&mlan_sta_ops,
#endif
#ifdef UAP_SUPPORT
	&mlan_uap_ops,
#endif
	MNULL,
};

/** Global moal_assert callback */
t_void(*assert_callback) (IN t_void * pmoal_handle, IN t_u32 cond) = MNULL;
#ifdef DEBUG_LEVEL1
#ifdef DEBUG_LEVEL2
#define DEFAULT_DEBUG_MASK	(0xffffffff)
#else
#define DEFAULT_DEBUG_MASK	(MMSG | MFATAL | MERROR)
#endif

/** Global moal_print callback */
t_void(*print_callback) (IN t_void * pmoal_handle,
			 IN t_u32 level, IN char *pformat, IN ...) = MNULL;

/** Global moal_get_system_time callback */
mlan_status(*get_sys_time_callback) (IN t_void * pmoal_handle,
				     OUT t_u32 * psec,
				     OUT t_u32 * pusec) = MNULL;

/** Global driver debug mit masks */
t_u32 mlan_drvdbg = DEFAULT_DEBUG_MASK;
#endif

/********************************************************
        Local Functions
*******************************************************/

/********************************************************
        Global Functions
********************************************************/

/**
 *  @brief This function registers MOAL to MLAN module.
 *
 *  @param pmdevice        A pointer to a mlan_device structure
 *                         allocated in MOAL
 *  @param ppmlan_adapter  A pointer to a t_void pointer to store
 *                         mlan_adapter structure pointer as the context
 *
 *  @return                MLAN_STATUS_SUCCESS
 *                             The registration succeeded.
 *                         MLAN_STATUS_FAILURE
 *                             The registration failed.
 *
 * mlan_status mlan_register (
 *   IN pmlan_device     pmdevice,
 *   OUT t_void          **ppmlan_adapter
 * );
 *
 * Comments
 *   MOAL constructs mlan_device data structure to pass moal_handle and
 *   mlan_callback table to MLAN. MLAN returns mlan_adapter pointer to
 *   the ppmlan_adapter buffer provided by MOAL.
 * Headers:
 *   declared in mlan_decl.h
 * See Also
 *   mlan_unregister
 */
mlan_status
mlan_register(IN pmlan_device pmdevice, OUT t_void ** ppmlan_adapter)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	pmlan_adapter pmadapter = MNULL;
	pmlan_callbacks pcb = MNULL;
	t_u8 i = 0;
	t_u32 j = 0;

	MASSERT(pmdevice);
	MASSERT(ppmlan_adapter);
	MASSERT(pmdevice->callbacks.moal_print);
#ifdef DEBUG_LEVEL1
	print_callback = pmdevice->callbacks.moal_print;
	get_sys_time_callback = pmdevice->callbacks.moal_get_system_time;
#endif
	assert_callback = pmdevice->callbacks.moal_assert;

	ENTER();

	MASSERT(pmdevice->callbacks.moal_malloc);
	MASSERT(pmdevice->callbacks.moal_memset);
	MASSERT(pmdevice->callbacks.moal_memmove);

	/* Allocate memory for adapter structure */
	if (pmdevice->callbacks.moal_vmalloc && pmdevice->callbacks.moal_vfree)
		ret = pmdevice->callbacks.moal_vmalloc(pmdevice->pmoal_handle,
						       sizeof(mlan_adapter),
						       (t_u8 **) & pmadapter);
	else
		ret = pmdevice->callbacks.moal_malloc(pmdevice->pmoal_handle,
						      sizeof(mlan_adapter),
						      MLAN_MEM_DEF,
						      (t_u8 **) & pmadapter);
	if ((ret != MLAN_STATUS_SUCCESS) || !pmadapter) {
		ret = MLAN_STATUS_FAILURE;
		goto exit_register;
	}

	pmdevice->callbacks.moal_memset(pmdevice->pmoal_handle, pmadapter,
					0, sizeof(mlan_adapter));

	pcb = &pmadapter->callbacks;

	/* Save callback functions */
	pmdevice->callbacks.moal_memmove(pmadapter->pmoal_handle, pcb,
					 &pmdevice->callbacks,
					 sizeof(mlan_callbacks));

	/* Assertion for all callback functions */
	MASSERT(pcb->moal_init_fw_complete);
	MASSERT(pcb->moal_shutdown_fw_complete);
	MASSERT(pcb->moal_send_packet_complete);
	MASSERT(pcb->moal_recv_packet);
	MASSERT(pcb->moal_recv_event);
	MASSERT(pcb->moal_ioctl_complete);

	MASSERT(pcb->moal_write_reg);
	MASSERT(pcb->moal_read_reg);
	MASSERT(pcb->moal_alloc_mlan_buffer);
	MASSERT(pcb->moal_free_mlan_buffer);
	MASSERT(pcb->moal_write_data_sync);
	MASSERT(pcb->moal_read_data_sync);
	MASSERT(pcb->moal_mfree);
	MASSERT(pcb->moal_memcpy);
	MASSERT(pcb->moal_memcmp);
	MASSERT(pcb->moal_get_system_time);
	MASSERT(pcb->moal_init_timer);
	MASSERT(pcb->moal_free_timer);
	MASSERT(pcb->moal_start_timer);
	MASSERT(pcb->moal_stop_timer);
	MASSERT(pcb->moal_init_lock);
	MASSERT(pcb->moal_free_lock);
	MASSERT(pcb->moal_spin_lock);
	MASSERT(pcb->moal_spin_unlock);

	/* Save pmoal_handle */
	pmadapter->pmoal_handle = pmdevice->pmoal_handle;

	pmadapter->feature_control = pmdevice->feature_control;

	if ((pmdevice->int_mode == INT_MODE_GPIO) && (pmdevice->gpio_pin == 0)) {
		PRINTM(MERROR, "SDIO_GPIO_INT_CONFIG: Invalid GPIO Pin\n");
		ret = MLAN_STATUS_FAILURE;
		goto error;
	}
	pmadapter->init_para.int_mode = pmdevice->int_mode;
	pmadapter->init_para.gpio_pin = pmdevice->gpio_pin;
	/* card specific probing has been deferred until now .. */
	if (MLAN_STATUS_SUCCESS != (ret = wlan_sdio_probe(pmadapter))) {
		ret = MLAN_STATUS_FAILURE;
		goto error;
	}
#ifdef DEBUG_LEVEL1
	mlan_drvdbg = pmdevice->drvdbg;
#endif

#ifdef MFG_CMD_SUPPORT
	pmadapter->init_para.mfg_mode = pmdevice->mfg_mode;
#endif
#ifdef SDIO_MULTI_PORT_TX_AGGR
	pmadapter->init_para.mpa_tx_cfg = pmdevice->mpa_tx_cfg;
#endif
#ifdef SDIO_MULTI_PORT_RX_AGGR
	pmadapter->init_para.mpa_rx_cfg = pmdevice->mpa_rx_cfg;
#endif
	pmadapter->init_para.auto_ds = pmdevice->auto_ds;
	pmadapter->init_para.ps_mode = pmdevice->ps_mode;
	if (pmdevice->max_tx_buf == MLAN_TX_DATA_BUF_SIZE_2K ||
	    pmdevice->max_tx_buf == MLAN_TX_DATA_BUF_SIZE_4K ||
	    pmdevice->max_tx_buf == MLAN_TX_DATA_BUF_SIZE_8K)
		pmadapter->init_para.max_tx_buf = pmdevice->max_tx_buf;
#ifdef STA_SUPPORT
	pmadapter->init_para.cfg_11d = pmdevice->cfg_11d;
#else
	pmadapter->init_para.cfg_11d = 0;
#endif
	if (IS_DFS_SUPPORT(pmadapter->feature_control))
		pmadapter->init_para.dfs_master_radar_det_en =
			DFS_MASTER_RADAR_DETECT_EN;
	pmadapter->init_para.dfs_slave_radar_det_en = DFS_SLAVE_RADAR_DETECT_EN;

	pmadapter->priv_num = 0;
	for (i = 0; i < MLAN_MAX_BSS_NUM; i++) {
		pmadapter->priv[i] = MNULL;
		if (pmdevice->bss_attr[i].active == MTRUE) {
			/* For valid bss_attr, allocate memory for private
			   structure */
			if (pcb->moal_vmalloc && pcb->moal_vfree)
				ret = pcb->moal_vmalloc(pmadapter->pmoal_handle,
							sizeof(mlan_private),
							(t_u8 **) & pmadapter->
							priv[i]);
			else
				ret = pcb->moal_malloc(pmadapter->pmoal_handle,
						       sizeof(mlan_private),
						       MLAN_MEM_DEF,
						       (t_u8 **) & pmadapter->
						       priv[i]);
			if (ret != MLAN_STATUS_SUCCESS || !pmadapter->priv[i]) {
				ret = MLAN_STATUS_FAILURE;
				goto error;
			}

			pmadapter->priv_num++;
			memset(pmadapter, pmadapter->priv[i], 0,
			       sizeof(mlan_private));

			pmadapter->priv[i]->adapter = pmadapter;

			/* Save bss_type, frame_type & bss_priority */
			pmadapter->priv[i]->bss_type =
				(t_u8) pmdevice->bss_attr[i].bss_type;
			pmadapter->priv[i]->frame_type =
				(t_u8) pmdevice->bss_attr[i].frame_type;
			pmadapter->priv[i]->bss_priority =
				(t_u8) pmdevice->bss_attr[i].bss_priority;
			if (pmdevice->bss_attr[i].bss_type == MLAN_BSS_TYPE_STA)
				pmadapter->priv[i]->bss_role =
					MLAN_BSS_ROLE_STA;
			else if (pmdevice->bss_attr[i].bss_type ==
				 MLAN_BSS_TYPE_UAP)
				pmadapter->priv[i]->bss_role =
					MLAN_BSS_ROLE_UAP;
#ifdef WIFI_DIRECT_SUPPORT
			else if (pmdevice->bss_attr[i].bss_type ==
				 MLAN_BSS_TYPE_WIFIDIRECT) {
				pmadapter->priv[i]->bss_role =
					MLAN_BSS_ROLE_STA;
				if (pmdevice->bss_attr[i].bss_virtual)
					pmadapter->priv[i]->bss_virtual = MTRUE;
			}
#endif
			/* Save bss_index and bss_num */
			pmadapter->priv[i]->bss_index = i;
			pmadapter->priv[i]->bss_num =
				(t_u8) pmdevice->bss_attr[i].bss_num;

			/* init function table */
			for (j = 0; mlan_ops[j]; j++) {
				if (mlan_ops[j]->bss_role ==
				    GET_BSS_ROLE(pmadapter->priv[i])) {
					memcpy(pmadapter,
					       &pmadapter->priv[i]->ops,
					       mlan_ops[j],
					       sizeof(mlan_operations));
				}
			}
		}
	}

	/* Initialize lock variables */
	if (wlan_init_lock_list(pmadapter) != MLAN_STATUS_SUCCESS) {
		ret = MLAN_STATUS_FAILURE;
		goto error;
	}

	/* Allocate memory for member of adapter structure */
	if (wlan_allocate_adapter(pmadapter)) {
		ret = MLAN_STATUS_FAILURE;
		goto error;
	}

	/* Initialize timers */
	if (wlan_init_timer(pmadapter) != MLAN_STATUS_SUCCESS) {
		ret = MLAN_STATUS_FAILURE;
		goto error;
	}
	/* Return pointer of mlan_adapter to MOAL */
	*ppmlan_adapter = pmadapter;

	goto exit_register;

error:
	PRINTM(MINFO, "Leave mlan_register with error\n");
	/* Free timers */
	wlan_free_timer(pmadapter);
	/* Free adapter structure */
	wlan_free_adapter(pmadapter);
	/* Free lock variables */
	wlan_free_lock_list(pmadapter);
	for (i = 0; i < MLAN_MAX_BSS_NUM; i++) {
		if (pmadapter->priv[i]) {
			if (pcb->moal_vmalloc && pcb->moal_vfree)
				pcb->moal_vfree(pmadapter->pmoal_handle,
						(t_u8 *) pmadapter->priv[i]);
			else
				pcb->moal_mfree(pmadapter->pmoal_handle,
						(t_u8 *) pmadapter->priv[i]);
		}
	}
	if (pcb->moal_vmalloc && pcb->moal_vfree)
		pcb->moal_vfree(pmadapter->pmoal_handle, (t_u8 *) pmadapter);
	else
		pcb->moal_mfree(pmadapter->pmoal_handle, (t_u8 *) pmadapter);

exit_register:
	LEAVE();
	return ret;
}

/**
 *  @brief This function unregisters MOAL from MLAN module.
 *
 *  @param pmlan_adapter   A pointer to a mlan_device structure
 *                         allocated in MOAL
 *
 *  @return                MLAN_STATUS_SUCCESS
 *                             The deregistration succeeded.
 */
mlan_status
mlan_unregister(IN t_void * pmlan_adapter)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_adapter *pmadapter = (mlan_adapter *) pmlan_adapter;
	pmlan_callbacks pcb;
	t_s32 i = 0;

	MASSERT(pmlan_adapter);

	ENTER();

	pcb = &pmadapter->callbacks;

	/* Free adapter structure */
	wlan_free_adapter(pmadapter);

	/* Free timers */
	wlan_free_timer(pmadapter);

	/* Free lock variables */
	wlan_free_lock_list(pmadapter);

	/* Free private structures */
	for (i = 0; i < pmadapter->priv_num; i++) {
		if (pmadapter->priv[i]) {
			if (pcb->moal_vmalloc && pcb->moal_vfree)
				pcb->moal_vfree(pmadapter->pmoal_handle,
						(t_u8 *) pmadapter->priv[i]);
			else
				pcb->moal_mfree(pmadapter->pmoal_handle,
						(t_u8 *) pmadapter->priv[i]);
		}
	}

	/* Free mlan_adapter */
	if (pcb->moal_vmalloc && pcb->moal_vfree)
		pcb->moal_vfree(pmadapter->pmoal_handle, (t_u8 *) pmadapter);
	else
		pcb->moal_mfree(pmadapter->pmoal_handle, (t_u8 *) pmadapter);

	LEAVE();
	return ret;
}

/**
 *  @brief This function downloads the firmware
 *
 *  @param pmlan_adapter   A pointer to a t_void pointer to store
 *                         mlan_adapter structure pointer
 *  @param pmfw            A pointer to firmware image
 *
 *  @return                MLAN_STATUS_SUCCESS
 *                             The firmware download succeeded.
 *                         MLAN_STATUS_FAILURE
 *                             The firmware download failed.
 */
mlan_status
mlan_dnld_fw(IN t_void * pmlan_adapter, IN pmlan_fw_image pmfw)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_adapter *pmadapter = (mlan_adapter *) pmlan_adapter;
	t_u32 poll_num = 1;
	t_u32 winner = 0;

	ENTER();
	MASSERT(pmlan_adapter);

	/* Card specific probing */
	ret = wlan_sdio_probe(pmadapter);
	if (ret == MLAN_STATUS_FAILURE) {
		PRINTM(MERROR, "WLAN SDIO probe failed\n", ret);
		LEAVE();
		return ret;
	}

	/* Check if firmware is already running */
	ret = wlan_check_fw_status(pmadapter, poll_num);
	if (ret == MLAN_STATUS_SUCCESS) {
		PRINTM(MMSG, "WLAN FW already running! Skip FW download\n");
		goto done;
	}
	poll_num = MAX_FIRMWARE_POLL_TRIES;

	/* Check if other interface is downloading */
	ret = wlan_check_winner_status(pmadapter, &winner);
	if (ret == MLAN_STATUS_FAILURE) {
		PRINTM(MFATAL, "WLAN read winner status failed!\n");
		goto done;
	}
	if (winner) {
		PRINTM(MMSG,
		       "WLAN is not the winner (0x%x). Skip FW download\n",
		       winner);
		poll_num = MAX_MULTI_INTERFACE_POLL_TRIES;
		goto poll_fw;
	}

	if (pmfw) {
		/* Download helper/firmware */
		ret = wlan_dnld_fw(pmadapter, pmfw);
		if (ret != MLAN_STATUS_SUCCESS) {
			PRINTM(MERROR, "wlan_dnld_fw fail ret=0x%x\n", ret);
			LEAVE();
			return ret;
		}
	}

poll_fw:
	/* Check if the firmware is downloaded successfully or not */
	ret = wlan_check_fw_status(pmadapter, poll_num);
	if (ret != MLAN_STATUS_SUCCESS) {
		PRINTM(MFATAL, "FW failed to be active in time!\n");
		ret = MLAN_STATUS_FAILURE;
		LEAVE();
		return ret;
	}
done:

	/* re-enable host interrupt for mlan after fw dnld is successful */
	wlan_enable_host_int(pmadapter);

	LEAVE();
	return ret;
}

/**
 *  @brief This function pass init param to MLAN
 *
 *  @param pmlan_adapter  A pointer to a t_void pointer to store
 *                        mlan_adapter structure pointer
 *  @param pparam         A pointer to mlan_init_param structure
 *
 *  @return               MLAN_STATUS_SUCCESS
 *
 */
mlan_status
mlan_set_init_param(IN t_void * pmlan_adapter, IN pmlan_init_param pparam)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_adapter *pmadapter = (mlan_adapter *) pmlan_adapter;

	ENTER();
	MASSERT(pmlan_adapter);

    /** Save cal data in MLAN */
	if ((pparam->pcal_data_buf) && (pparam->cal_data_len > 0)) {
		pmadapter->pcal_data = pparam->pcal_data_buf;
		pmadapter->cal_data_len = pparam->cal_data_len;
	}

	LEAVE();
	return ret;
}

/**
 *  @brief This function initializes the firmware
 *
 *  @param pmlan_adapter   A pointer to a t_void pointer to store
 *                         mlan_adapter structure pointer
 *
 *  @return                MLAN_STATUS_SUCCESS
 *                             The firmware initialization succeeded.
 *                         MLAN_STATUS_PENDING
 *                             The firmware initialization is pending.
 *                         MLAN_STATUS_FAILURE
 *                             The firmware initialization failed.
 */
mlan_status
mlan_init_fw(IN t_void * pmlan_adapter)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_adapter *pmadapter = (mlan_adapter *) pmlan_adapter;

	ENTER();
	MASSERT(pmlan_adapter);

	pmadapter->hw_status = WlanHardwareStatusInitializing;

	/* Initialize firmware, may return PENDING */
	ret = wlan_init_fw(pmadapter);
	PRINTM(MINFO, "wlan_init_fw returned ret=0x%x\n", ret);

	LEAVE();
	return ret;
}

/**
 *  @brief Shutdown firmware
 *
 *  @param pmlan_adapter	A pointer to mlan_adapter structure
 *
 *  @return			MLAN_STATUS_SUCCESS
 *                      		The firmware shutdown call succeeded.
 *				MLAN_STATUS_PENDING
 *      	                	The firmware shutdown call is pending.
 *				MLAN_STATUS_FAILURE
 *      	                	The firmware shutdown call failed.
 */
mlan_status
mlan_shutdown_fw(IN t_void * pmlan_adapter)
{
	mlan_status ret = MLAN_STATUS_PENDING;
	mlan_adapter *pmadapter = (mlan_adapter *) pmlan_adapter;
	pmlan_callbacks pcb;
	t_s32 i = 0;

	ENTER();

	MASSERT(pmlan_adapter);
	/* MLAN already shutdown */
	if (pmadapter->hw_status == WlanHardwareStatusNotReady) {
		LEAVE();
		return MLAN_STATUS_SUCCESS;
	}

	pmadapter->hw_status = WlanHardwareStatusClosing;
	/* Wait for mlan_process to complete */
	if (pmadapter->mlan_processing) {
		PRINTM(MWARN, "MLAN main processing is still running\n");
		LEAVE();
		return ret;
	}

	/* Shut down MLAN */
	PRINTM(MINFO, "Shutdown MLAN...\n");

	/* Cancel all pending commands and complete ioctls */
	wlan_cancel_all_pending_cmd(pmadapter);

	/* Clean up priv structures */
	for (i = 0; i < pmadapter->priv_num; i++) {
		if (pmadapter->priv[i]) {
			wlan_free_priv(pmadapter->priv[i]);
		}
	}

	pcb = &pmadapter->callbacks;

	/* Notify completion */
	ret = wlan_shutdown_fw_complete(pmadapter);

	LEAVE();
	return ret;
}

/**
 *  @brief The main process
 *
 *  @param pmlan_adapter	A pointer to mlan_adapter structure
 *
 *  @return			MLAN_STATUS_SUCCESS or MLAN_STATUS_FAILURE
 */
mlan_status
mlan_main_process(IN t_void * pmlan_adapter)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	mlan_adapter *pmadapter = (mlan_adapter *) pmlan_adapter;
	pmlan_callbacks pcb;

	ENTER();

	MASSERT(pmlan_adapter);

	pcb = &pmadapter->callbacks;

	pcb->moal_spin_lock(pmadapter->pmoal_handle,
			    pmadapter->pmain_proc_lock);

	/* Check if already processing */
	if (pmadapter->mlan_processing) {
		pmadapter->more_task_flag = MTRUE;
		pcb->moal_spin_unlock(pmadapter->pmoal_handle,
				      pmadapter->pmain_proc_lock);
		goto exit_main_proc;
	} else {
		pmadapter->mlan_processing = MTRUE;
		pcb->moal_spin_unlock(pmadapter->pmoal_handle,
				      pmadapter->pmain_proc_lock);
	}
process_start:
	do {
		pcb->moal_spin_lock(pmadapter->pmoal_handle,
				    pmadapter->pmain_proc_lock);
		pmadapter->more_task_flag = MFALSE;
		pcb->moal_spin_unlock(pmadapter->pmoal_handle,
				      pmadapter->pmain_proc_lock);
		/* Is MLAN shutting down or not ready? */
		if ((pmadapter->hw_status == WlanHardwareStatusClosing) ||
		    (pmadapter->hw_status == WlanHardwareStatusNotReady))
			break;

		/* Handle pending SDIO interrupts if any */
		if (pmadapter->sdio_ireg) {
			if (pmadapter->hs_activated == MTRUE)
				wlan_process_hs_config(pmadapter);
			wlan_process_int_status(pmadapter);
		}

		/* Need to wake up the card ? */
		if ((pmadapter->ps_state == PS_STATE_SLEEP) &&
		    (pmadapter->pm_wakeup_card_req &&
		     !pmadapter->pm_wakeup_fw_try) &&
		    (util_peek_list
		     (pmadapter->pmoal_handle, &pmadapter->cmd_pending_q,
		      pcb->moal_spin_lock, pcb->moal_spin_unlock)
		     || !wlan_bypass_tx_list_empty(pmadapter)
		     || !wlan_wmm_lists_empty(pmadapter)
		    )) {
			pmadapter->pm_wakeup_fw_try = MTRUE;
			wlan_pm_wakeup_card(pmadapter);
			continue;
		}
		if (IS_CARD_RX_RCVD(pmadapter)) {
			pmadapter->data_received = MFALSE;
			if (pmadapter->hs_activated == MTRUE) {
				pmadapter->is_hs_configured = MFALSE;
				wlan_host_sleep_activated_event(wlan_get_priv
								(pmadapter,
								 MLAN_BSS_ROLE_ANY),
								MFALSE);
			}
			pmadapter->pm_wakeup_fw_try = MFALSE;
			if (pmadapter->ps_state == PS_STATE_SLEEP)
				pmadapter->ps_state = PS_STATE_AWAKE;
		} else {
			/* We have tried to wakeup the card already */
			if (pmadapter->pm_wakeup_fw_try)
				break;
			if (pmadapter->ps_state != PS_STATE_AWAKE ||
			    (pmadapter->tx_lock_flag == MTRUE))
				break;

			if (pmadapter->scan_processing
			    || pmadapter->data_sent
			    || (wlan_bypass_tx_list_empty(pmadapter) &&
				wlan_wmm_lists_empty(pmadapter))
			    || wlan_11h_radar_detected_tx_blocked(pmadapter)
				) {
				if (pmadapter->cmd_sent || pmadapter->curr_cmd
				    ||
				    (!util_peek_list
				     (pmadapter->pmoal_handle,
				      &pmadapter->cmd_pending_q,
				      pcb->moal_spin_lock,
				      pcb->moal_spin_unlock))) {
					break;
				}
			}
		}

		/* Check for Cmd Resp */
		if (pmadapter->cmd_resp_received) {
			pmadapter->cmd_resp_received = MFALSE;
			wlan_process_cmdresp(pmadapter);

			/* call moal back when init_fw is done */
			if (pmadapter->hw_status == WlanHardwareStatusInitdone) {
				pmadapter->hw_status = WlanHardwareStatusReady;
				wlan_init_fw_complete(pmadapter);
			}
		}

		/* Check for event */
		if (pmadapter->event_received) {
			pmadapter->event_received = MFALSE;
			wlan_process_event(pmadapter);
		}

		/* Check if we need to confirm Sleep Request received
		   previously */
		if (pmadapter->ps_state == PS_STATE_PRE_SLEEP) {
			if (!pmadapter->cmd_sent && !pmadapter->curr_cmd) {
				wlan_check_ps_cond(pmadapter);
			}
		}

		/*
		 * The ps_state may have been changed during processing of
		 * Sleep Request event.
		 */
		if ((pmadapter->ps_state == PS_STATE_SLEEP)
		    || (pmadapter->ps_state == PS_STATE_PRE_SLEEP)
		    || (pmadapter->ps_state == PS_STATE_SLEEP_CFM)
		    || (pmadapter->tx_lock_flag == MTRUE)
			)
			continue;

		if (!pmadapter->cmd_sent && !pmadapter->curr_cmd) {
			if (wlan_exec_next_cmd(pmadapter) ==
			    MLAN_STATUS_FAILURE) {
				ret = MLAN_STATUS_FAILURE;
				break;
			}
		}

		if (!pmadapter->scan_processing
		    && !pmadapter->data_sent &&
		    !wlan_11h_radar_detected_tx_blocked(pmadapter) &&
		    !wlan_bypass_tx_list_empty(pmadapter)) {
			PRINTM(MINFO, "mlan_send_pkt(): deq(bybass_txq)\n");
			wlan_process_bypass_tx(pmadapter);
			if (pmadapter->hs_activated == MTRUE) {
				pmadapter->is_hs_configured = MFALSE;
				wlan_host_sleep_activated_event(wlan_get_priv
								(pmadapter,
								 MLAN_BSS_ROLE_ANY),
								MFALSE);
			}
		}

		if (!pmadapter->scan_processing
		    && !pmadapter->data_sent && !wlan_wmm_lists_empty(pmadapter)
		    && !wlan_11h_radar_detected_tx_blocked(pmadapter)
			) {
			wlan_wmm_process_tx(pmadapter);
			if (pmadapter->hs_activated == MTRUE) {
				pmadapter->is_hs_configured = MFALSE;
				wlan_host_sleep_activated_event(wlan_get_priv
								(pmadapter,
								 MLAN_BSS_ROLE_ANY),
								MFALSE);
			}
		}

#ifdef STA_SUPPORT
		if (pmadapter->delay_null_pkt && !pmadapter->cmd_sent &&
		    !pmadapter->curr_cmd && !IS_COMMAND_PENDING(pmadapter) &&
		    wlan_bypass_tx_list_empty(pmadapter) &&
		    wlan_wmm_lists_empty(pmadapter)) {
			if (wlan_send_null_packet
			    (wlan_get_priv(pmadapter, MLAN_BSS_ROLE_STA),
			     MRVDRV_TxPD_POWER_MGMT_NULL_PACKET |
			     MRVDRV_TxPD_POWER_MGMT_LAST_PACKET)
			    == MLAN_STATUS_SUCCESS) {
				pmadapter->delay_null_pkt = MFALSE;
			}
			break;
		}
#endif

	} while (MTRUE);

	pcb->moal_spin_lock(pmadapter->pmoal_handle,
			    pmadapter->pmain_proc_lock);
	if (pmadapter->more_task_flag == MTRUE) {
		pcb->moal_spin_unlock(pmadapter->pmoal_handle,
				      pmadapter->pmain_proc_lock);
		goto process_start;
	}
	pmadapter->mlan_processing = MFALSE;
	pcb->moal_spin_unlock(pmadapter->pmoal_handle,
			      pmadapter->pmain_proc_lock);

exit_main_proc:
	if (pmadapter->hw_status == WlanHardwareStatusClosing)
		mlan_shutdown_fw(pmadapter);
	LEAVE();
	return ret;
}

/**
 *  @brief Function to send packet
 *
 *  @param pmlan_adapter	A pointer to mlan_adapter structure
 *  @param pmbuf		A pointer to mlan_buffer structure
 *
 *  @return			MLAN_STATUS_PENDING
 */
mlan_status
mlan_send_packet(IN t_void * pmlan_adapter, IN pmlan_buffer pmbuf)
{
	mlan_status ret = MLAN_STATUS_PENDING;
	mlan_adapter *pmadapter = (mlan_adapter *) pmlan_adapter;

	ENTER();
	MASSERT(pmlan_adapter && pmbuf);

	MASSERT(pmbuf->bss_index < pmadapter->priv_num);
	pmbuf->flags = MLAN_BUF_FLAG_MOAL_TX_BUF;

	if (((pmadapter->priv[pmbuf->bss_index]->port_ctrl_mode == MTRUE) &&
	     ((mlan_ntohs(*(t_u16 *) & pmbuf->pbuf[pmbuf->data_offset +
						   MLAN_ETHER_PKT_TYPE_OFFSET])
	       == MLAN_ETHER_PKT_TYPE_EAPOL)
	      ||
	      (mlan_ntohs
	       (*(t_u16 *) & pmbuf->
		pbuf[pmbuf->data_offset + MLAN_ETHER_PKT_TYPE_OFFSET]) ==
	       MLAN_ETHER_PKT_TYPE_WAPI)
	     ))
	    || (pmbuf->buf_type == MLAN_BUF_TYPE_RAW_DATA)

		) {
		PRINTM(MINFO, "mlan_send_pkt(): enq(bybass_txq)\n");
		wlan_add_buf_bypass_txqueue(pmadapter, pmbuf);
	} else {
		/* Transmit the packet */
		wlan_wmm_add_buf_txqueue(pmadapter, pmbuf);
	}

	LEAVE();
	return ret;
}

/**
 *  @brief MLAN ioctl handler
 *
 *  @param adapter	A pointer to mlan_adapter structure
 *  @param pioctl_req	A pointer to ioctl request buffer
 *
 *  @return		MLAN_STATUS_SUCCESS/MLAN_STATUS_PENDING --success, otherwise fail
 */
mlan_status
mlan_ioctl(IN t_void * adapter, IN pmlan_ioctl_req pioctl_req)
{
	mlan_status ret = MLAN_STATUS_SUCCESS;
	pmlan_adapter pmadapter = (pmlan_adapter) adapter;
	pmlan_private pmpriv = MNULL;

	ENTER();

	if (pioctl_req == MNULL) {
		PRINTM(MERROR, "MLAN IOCTL information buffer is NULL\n");
		ret = MLAN_STATUS_FAILURE;
		goto exit;
	}
	if (pioctl_req->action == MLAN_ACT_CANCEL) {
		wlan_cancel_pending_ioctl(pmadapter, pioctl_req);
		ret = MLAN_STATUS_SUCCESS;
		goto exit;
	}
	pmpriv = pmadapter->priv[pioctl_req->bss_index];
	ret = pmpriv->ops.ioctl(adapter, pioctl_req);
exit:
	LEAVE();
	return ret;
}

/**
 *  @brief Packet receive completion callback handler
 *
 *  @param pmlan_adapter	A pointer to mlan_adapter structure
 *  @param pmbuf		A pointer to mlan_buffer structure
 *  @param status		Callback status
 *
 *  @return			MLAN_STATUS_SUCCESS
 */
mlan_status
mlan_recv_packet_complete(IN t_void * pmlan_adapter,
			  IN pmlan_buffer pmbuf, IN mlan_status status)
{
	mlan_adapter *pmadapter = (mlan_adapter *) pmlan_adapter;

	ENTER();
	wlan_recv_packet_complete(pmadapter, pmbuf, status);
	LEAVE();
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief select wmm queue
 *
 *  @param pmlan_adapter	A pointer to mlan_adapter structure
 *  @param bss_num		BSS number
 *  @param tid			TID
 *
 *  @return			wmm queue priority (0 - 3)
 */
t_u8
mlan_select_wmm_queue(IN t_void * pmlan_adapter, IN t_u8 bss_num, IN t_u8 tid)
{
	mlan_adapter *pmadapter = (mlan_adapter *) pmlan_adapter;
	pmlan_private pmpriv = pmadapter->priv[bss_num];
	t_u8 ret;
	ENTER();
	ret = wlan_wmm_select_queue(pmpriv, tid);
	LEAVE();
	return ret;
}

/**
 *  @brief This function gets interrupt status.
 *
 *  @param adapter  A pointer to mlan_adapter structure
 *  @return         N/A
 */
t_void
mlan_interrupt(IN t_void * adapter)
{
	mlan_adapter *pmadapter = (mlan_adapter *) adapter;

	ENTER();
	if (!pmadapter->pps_uapsd_mode && pmadapter->ps_state == PS_STATE_SLEEP) {
		pmadapter->pm_wakeup_fw_try = MFALSE;
		pmadapter->ps_state = PS_STATE_AWAKE;
	}
	wlan_interrupt(pmadapter);
	LEAVE();
}
