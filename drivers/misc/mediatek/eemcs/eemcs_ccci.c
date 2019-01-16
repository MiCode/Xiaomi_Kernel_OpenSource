#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include "eemcs_ccci.h"
#include "eemcs_debug.h"
#include "eemcs_boot.h"
#include "eemcs_state.h"
#include "eemcs_boot.h"
#include "eemcs_statistics.h"

#ifdef _EEMCS_CCCI_LB_UT
#include "eccmni.h"
#include "eemcs_expt_ut.h"
#endif

struct wake_lock eemcs_wake_lock;
/*used to ensure sdio_fun1_read, sdio_fun1_write*/
struct wake_lock sdio_wake_lock;
static KAL_UINT8 eemcs_wakelock_name[32];
static spinlock_t swint_cb_lock;
static ccci_tx_waitq_t ccci_tx_waitq[TXQ_NUM];
static EEMCS_CCCI_SWINT_CALLBACK ccci_swint_cb;
static EEMCS_CCCI_WDT_CALLBACK ccci_WDT_cb = NULL;
static KAL_UINT32 ccci_ch_to_port_mapping[CH_NUM_MAX];
static ccci_port_cfg ccci_port_info[CCCI_PORT_NUM_MAX] = {
    /* CCCI Character Devices */
    {{CH_CTRL_RX,   CH_CTRL_TX, NULL, NULL}, HIF_SDIO, TX_Q_0, RX_Q_0, 0, 0, BLK_4K, 4, BLK_4K, 4, EX_T_USER, PRI_RT,EXPORT_CCCI_H|TX_PRVLG1|TX_PRVLG2},
    {{CH_SYS_RX,    CH_SYS_TX,  NULL, NULL}, HIF_SDIO, TX_Q_0, RX_Q_0, 0, 0, BLK_4K, 1, BLK_4K, 1, EX_T_USER, PRI_RT,EXPORT_CCCI_H},
    {{CH_AUD_RX,    CH_AUD_TX,  NULL, NULL}, HIF_SDIO, TX_Q_0, RX_Q_0, 0, 0, BLK_4K, 8, BLK_4K, 8, EX_T_USER, PRI_RT, EXPORT_CCCI_H},
    
    {{CH_META_RX,   CH_META_TX, NULL, NULL}, HIF_SDIO, TX_Q_1, RX_Q_1, 0, 0, BLK_16K, 4, BLK_16K, 4, EX_T_USER, PRI_NR, TX_PRVLG2},
    {{CH_MUX_RX,    CH_MUX_TX,  NULL, NULL}, HIF_SDIO, TX_Q_1, RX_Q_1, 0, 0, BLK_4K, 16, BLK_2K, 16, EX_T_USER, PRI_NR, 0},

    {{CH_FS_RX,     CH_FS_TX,   NULL, NULL}, HIF_SDIO, TX_Q_1, RX_Q_1, 0, 0, BLK_4K, 4, BLK_4K, 8, EX_T_USER, PRI_NR, EXPORT_CCCI_H|TX_PRVLG1|TX_PRVLG2},
    {{CH_PMIC_RX,   CH_PMIC_TX, NULL, NULL}, HIF_SDIO, TX_Q_1, RX_Q_1, 0, 0, BLK_4K, 1, BLK_2K, 1, EX_T_USER, PRI_NR, EXPORT_CCCI_H},
    {{CH_UEM_RX,    CH_UEM_TX,  NULL, NULL}, HIF_SDIO, TX_Q_1, RX_Q_1, 0, 0, BLK_4K, 1, BLK_1K, 1, EX_T_USER, PRI_NR, EXPORT_CCCI_H},

    {{CH_RPC_RX,    CH_RPC_TX,  NULL, NULL}, HIF_SDIO, TX_Q_1, RX_Q_1, 0, 0, BLK_4K, 2, BLK_4K, 2, EX_T_USER, PRI_NR, TX_PRVLG1},
    {{CH_IPC_RX,    CH_IPC_TX,  NULL, NULL}, HIF_SDIO, TX_Q_1, RX_Q_1, 0, 0, BLK_4K, 2, BLK_4K, 2, EX_T_USER, PRI_NR, 0},
    {{CH_AGPS_RX,   CH_AGPS_TX, NULL, NULL}, HIF_SDIO, TX_Q_1, RX_Q_1, 0, 0, BLK_4K, 2, BLK_4K, 2, EX_T_USER, PRI_NR, 0},
    {{CH_MLOG_RX,   CH_MLOG_TX, NULL, NULL}, HIF_SDIO, TX_Q_1, RX_Q_2, 500, 300, BLK_4K, 4, BLK_4K, 4, EX_T_USER, PRI_RT, TX_PRVLG2},

    {{CH_IMSV_DL,   CH_IMSV_UL, NULL, NULL}, HIF_SDIO, TX_Q_0, RX_Q_0, 0, 0, BLK_4K, 8, BLK_4K, 8, EX_T_USER, PRI_RT, 0},
    {{CH_IMSC_DL,   CH_IMSC_UL, NULL, NULL}, HIF_SDIO, TX_Q_0, RX_Q_0, 0, 0, BLK_4K, 8, BLK_4K, 8, EX_T_USER, PRI_RT, 0},
    {{CH_IMSA_DL,   CH_IMSA_UL, NULL, NULL}, HIF_SDIO, TX_Q_0, RX_Q_0, 0, 0, BLK_4K, 8, BLK_4K, 8, EX_T_USER, PRI_RT, 0},
    {{CH_IMSDC_DL,  CH_IMSDC_UL,NULL, NULL}, HIF_SDIO, TX_Q_0, RX_Q_0, 0, 0, BLK_4K, 8, BLK_4K, 8, EX_T_USER, PRI_RT, 0},
        
    {{CH_DUMMY,     CH_DUMMY   ,NULL, NULL}, HIF_SDIO, TX_Q_0, RX_Q_0, 0, 0, BLK_4K, 8, BLK_4K, 8, EX_T_USER, PRI_RT, 0}, /*CCCI_PORT_MUX_REPORT*/
    {{CH_DUMMY,     CH_DUMMY   ,NULL, NULL}, HIF_SDIO, TX_Q_0, RX_Q_0, 0, 0, BLK_4K, 8, BLK_4K, 8, EX_T_USER, PRI_RT, 0}, /*CCCI_PORT_IOCTL*/
    {{CH_DUMMY,     CH_DUMMY   ,NULL, NULL}, HIF_SDIO, TX_Q_0, RX_Q_0, 0, 0, BLK_4K, 8, BLK_4K, 8, EX_T_USER, PRI_RT, 0}, /*CCCI_PORT_RILD*/
        
    {{CH_IT_RX,   CH_IT_TX, NULL, NULL}, HIF_SDIO, TX_Q_0, RX_Q_0, 0, 0, BLK_4K, 4, BLK_4K, 4, EX_T_USER, PRI_RT,EXPORT_CCCI_H|TX_PRVLG1|TX_PRVLG2},    
    /* CCCI Network Interface */
    {{CH_NET1_RX,   CH_NET1_TX, NULL, NULL}, HIF_SDIO, TX_Q_2, RX_Q_3, 0, 0, BLK_2K, 32, BLK_2K, 32, EX_T_KERN, PRI_NR,0},
    {{CH_NET2_RX,   CH_NET2_TX, NULL, NULL}, HIF_SDIO, TX_Q_3, RX_Q_3, 0, 0, BLK_2K, 4, BLK_2K, 4, EX_T_KERN, PRI_NR,0},
    {{CH_NET3_RX,   CH_NET3_TX, NULL, NULL}, HIF_SDIO, TX_Q_4, RX_Q_3, 0, 0, BLK_2K, 4, BLK_2K, 4, EX_T_KERN, PRI_NR,0},

};
KAL_UINT32 ccci_ch_to_port(KAL_UINT32 ccci_ch_num){
    KAL_UINT32 port_index;
    KAL_ASSERT(ccci_ch_num < CH_NUM_MAX);
    port_index = ccci_ch_to_port_mapping[ccci_ch_num];
    KAL_ASSERT(port_index < CCCI_PORT_NUM_MAX);
    return port_index;
}
KAL_UINT32 ccci_get_port_cflag(KAL_UINT32 ccci_port_index){
    KAL_ASSERT(ccci_port_index < CCCI_PORT_NUM_MAX);
    return ccci_port_info[ccci_port_index].flag;
}    

void ccci_set_port_type(KAL_UINT32 ccci_port_index, KAL_UINT32 new_flag){
    DEBUG_LOG_FUNCTION_ENTRY;
    KAL_ASSERT(ccci_port_index < CCCI_PORT_NUM_MAX);
    ccci_port_info[ccci_port_index].flag = new_flag;
    DEBUG_LOG_FUNCTION_LEAVE;
    return;
}    

KAL_UINT32 ccci_get_port_type(KAL_UINT32 ccci_port_index){
    KAL_ASSERT(ccci_port_index < CCCI_PORT_NUM_MAX);
    return ccci_port_info[ccci_port_index].export_type;
}    

ccci_port_cfg* ccci_get_port_info(KAL_UINT32 ccci_port_index){
    DEBUG_LOG_FUNCTION_ENTRY;
    KAL_ASSERT(ccci_port_index < CCCI_PORT_NUM_MAX);
    DEBUG_LOG_FUNCTION_LEAVE;
    return &ccci_port_info[ccci_port_index];
}

void eemcs_ccci_turn_off_dlq_by_port(KAL_UINT32 ccci_port_index){
    KAL_ASSERT(ccci_port_index < CCCI_PORT_NUM_MAX);
    DBGLOG(CCCI,DBG, "CCCI port (%d) turn off dlq(%d)", ccci_port_index, SDIO_RXQ(ccci_port_info[ccci_port_index].rxq_id));
    hif_turn_off_dl_q(SDIO_RXQ(ccci_port_info[ccci_port_index].rxq_id));
    return;
}   

void eemcs_ccci_turn_on_dlq_by_port(KAL_UINT32 ccci_port_index){
    KAL_ASSERT(ccci_port_index < CCCI_PORT_NUM_MAX);
    DBGLOG(CCCI,DBG, "CCCI port (%d) turn on dlq(%d)", ccci_port_index, SDIO_RXQ(ccci_port_info[ccci_port_index].rxq_id));
    hif_turn_on_dl_q(SDIO_RXQ(ccci_port_info[ccci_port_index].rxq_id));
    return;
} 

void eemcs_ccci_release_rx_skb(KAL_UINT32 port_idx, KAL_UINT32 cnt, struct sk_buff *skb) {
	if (ccci_port_info[port_idx].rx_flow_ctrl_limit) {
		mtlte_df_DL_release_buff(SDIO_RXQ(ccci_port_info[port_idx].rxq_id), cnt, skb);
	}
}

KAL_UINT32 eemcs_ccci_register_callback(CCCI_CHANNEL_T chn, EEMCS_CCCI_CALLBACK func_ptr , KAL_UINT32 private_data) {	
    KAL_UINT32   port_id = 0;

    DEBUG_LOG_FUNCTION_ENTRY;
	
    port_id = ccci_ch_to_port(chn);
    ccci_port_info[port_id].ch.rx_cb = func_ptr;
    DBGLOG(CCCI, DBG, "PORT%d(ch%d) register rx callback", port_id, chn);
	
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}


/*
 * @brief Register software interrupt.
 * @param
 *     func_ptr [in] Callback function pointer.
 * @return A non-negative value of callback ID indicates success.
 *         Otherwise, KAL_FAIL is returned.
 */
KAL_UINT32 eemcs_ccci_register_swint_callback(EEMCS_CCCI_SWINT_CALLBACK func_ptr)
{
    DEBUG_LOG_FUNCTION_ENTRY;

    spin_lock(&swint_cb_lock);
    ccci_swint_cb = func_ptr;
    spin_unlock(&swint_cb_lock);
    DBGLOG(CCCI, DBG, "ccci register swint callback");

    DEBUG_LOG_FUNCTION_LEAVE;

    return KAL_SUCCESS;
}

/*
 * @brief Unregister software interrupt.
 * @param
 *     id [int] The callback ID returned from eemcs_ccci_register_callback()
 * @return KAL_SUCCESS is returned always.
 */
KAL_UINT32 eemcs_ccci_unregister_swint_callback(KAL_UINT32 id)
{
    DEBUG_LOG_FUNCTION_ENTRY;

    KAL_ASSERT(id >= 0);
    spin_lock(&swint_cb_lock);
    ccci_swint_cb = NULL;
    spin_unlock(&swint_cb_lock);
    DBGLOG(CCCI, DBG, "CCCI unregister swint callback");

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_UINT32 eemcs_ccci_unregister_callback(CCCI_CHANNEL_T chn) {
    KAL_UINT32   port_id = 0;
    
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(CCCI,DBG, "CCCI channel (%d) UNregister callback", chn);

    port_id = ccci_ch_to_port(chn);
    ccci_port_info[port_id].ch.rx_cb = NULL;

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief Register WDT callback.
 * @param
 *     func_ptr [in] Callback function pointer.
 * @return KAL_SUCESS for success.
 *         Otherwise, KAL_FAIL is returned.
 */
KAL_UINT32 eemcs_ccci_register_WDT_callback(EEMCS_CCCI_WDT_CALLBACK func_ptr)
{
    DEBUG_LOG_FUNCTION_ENTRY;

    
    DBGLOG(CCCI, DBG, "CCCI register WDT callback");
    ccci_WDT_cb = func_ptr;
    DEBUG_LOG_FUNCTION_LEAVE;

    return KAL_SUCCESS;
}

/*
 * @brief Unregister WDT callback.
 * @param
 *     id [int] The callback ID returned from eemcs_ccci_register_callback()
 * @return KAL_SUCCESS is returned always.
 */
KAL_UINT32 eemcs_ccci_unregister_WDT_callback(void)
{
    DEBUG_LOG_FUNCTION_ENTRY;
   
    DBGLOG(CCCI, DBG, "CCCI unregister WDT callback");
    ccci_WDT_cb = NULL;
    
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_UINT32 eemcs_ccci_UL_write_room_alloc(CCCI_CHANNEL_T chn)
{
    KAL_UINT32   port_id = 0;
    KAL_UINT32   tx_queue_idx=0;
    KAL_UINT32   ret = 0;
#ifdef __EEMCS_EXPT_SUPPORT__
    EEMCS_EXCEPTION_STATE mode = EEMCS_EX_INVALID;
#endif
    
    port_id = ccci_ch_to_port(chn);
    
#ifdef __EEMCS_EXPT_SUPPORT__
    /*we just return 1 to bypass this check.
      exception check will be taken when writing SKB to swq */
    if(is_exception_mode(&mode))
    {
        return 1;
    }
#endif        

    tx_queue_idx = ccci_port_info[port_id].txq_id;

    // for poll
    if(atomic_read(&ccci_port_info[port_id].reserve_space) >= 1){
        DBGLOG(CCCI, TRA, "ccci_write_room_alloc: port%d write space has reserved",port_id);
        return 1;
    }
    
    ret = hif_ul_swq_space(tx_queue_idx) - atomic_read(&ccci_tx_waitq[tx_queue_idx].reserve_space);

    if (ret > 0){
        atomic_inc(&ccci_port_info[port_id].reserve_space);
        atomic_inc(&ccci_tx_waitq[tx_queue_idx].reserve_space);
    } else {
    	DBGLOG(CCCI, INF, "ccci_write_room_alloc: port%d, tx_qlen=%d, resv=%d, ret=%d", \
			port_id, mtlte_df_UL_pkt_in_swq(tx_queue_idx), atomic_read(&ccci_tx_waitq[tx_queue_idx].reserve_space), ret);
    }

    DBGLOG(CCCI, TRA, "ccci_write_room_alloc: txq=%d, size=%d",tx_queue_idx, ret);
    return ret;
}

KAL_UINT32 eemcs_ccci_UL_write_room_release(CCCI_CHANNEL_T chn){
    KAL_UINT32   port_id = 0;
    KAL_UINT32   tx_queue_idx = 0;

    port_id = ccci_ch_to_port(chn);
    tx_queue_idx = ccci_port_info[port_id].txq_id;
    atomic_dec(&ccci_port_info[port_id].reserve_space);
    atomic_dec(&ccci_tx_waitq[tx_queue_idx].reserve_space);
    return 0;
}


KAL_UINT32 eemcs_ccci_UL_write_wait(CCCI_CHANNEL_T chn)
{
    KAL_UINT32   port_id = 0;
    KAL_UINT32   tx_queue_idx=0;
    KAL_UINT32   ret = 0;
    
    port_id = ccci_ch_to_port(chn);
    tx_queue_idx = ccci_port_info[port_id].txq_id;

    DBGLOG(CCCI, INF, "ccci_write_wait: port%d wait, tx_qlen=%d, resv=%d", port_id, \
		mtlte_df_UL_pkt_in_swq(tx_queue_idx), atomic_read(&ccci_tx_waitq[tx_queue_idx].reserve_space));
	
    ret = wait_event_interruptible_exclusive(ccci_tx_waitq[tx_queue_idx].tx_waitq, (hif_ul_swq_space(tx_queue_idx) - atomic_read(&ccci_tx_waitq[tx_queue_idx].reserve_space) )> 0);

    DBGLOG(CCCI, INF, "ccci_write_wait: port%d wakeup, tx_qlen=%d, resv=%d, ret=%d", port_id, \
		mtlte_df_UL_pkt_in_swq(tx_queue_idx), atomic_read(&ccci_tx_waitq[tx_queue_idx].reserve_space), ret);
    return ret;
}

void eemcs_ccci_reset(void)
{
    KAL_UINT32 port_id = 0;
    KAL_UINT32 txq = 0;

    for(port_id = 0; port_id < CCCI_PORT_NUM_MAX; port_id++){
        if ((ccci_port_info[port_id].ch.tx != CH_DUMMY) && (ccci_port_info[port_id].ch.rx != CH_DUMMY)) {
		txq = ccci_port_info[port_id].txq_id;
		atomic_set(&ccci_tx_waitq[txq].reserve_space, 0);
		atomic_set(&ccci_port_info[port_id].reserve_space, 0);
        }
    }
    return;
}


KAL_INT32 eemcs_ccci_UL_write_skb_to_swq(CCCI_CHANNEL_T chn, struct sk_buff *skb)
{
    //KAL_DBGPRINT(KAL, DBG_INFO,("====> %s, chn: %d\n", KAL_FUNC_NAME, chn)) ;		
    //return	mtlte_df_UL_write_skb_to_swq(df_ul_ccci_ch_to_q[chn], skb) ;
    CCCI_BUFF_T *pccci_h = NULL;
    KAL_UINT32 tx_queue_idx;
    KAL_INT32 ret = KAL_SUCCESS;
#ifdef __EEMCS_EXPT_SUPPORT__
    ccci_expt_port_cfg *expt_port_info;
    EEMCS_EXCEPTION_STATE mode = EEMCS_EX_INVALID;
#endif
	DEBUG_LOG_FUNCTION_ENTRY;

    if(NULL != skb){
        pccci_h = (CCCI_BUFF_T *)skb->data;
    	DBGLOG(CCCI, DBG, "[TX]CCCI_H: 0x%x, 0x%x, 0x%x, 0x%x",\
            pccci_h->data[0], pccci_h->data[1], pccci_h->channel, pccci_h->reserved);
        KAL_ASSERT(pccci_h->channel == chn);
        KAL_ASSERT(pccci_h->channel < CH_NUM_MAX || pccci_h->channel == CCCI_FORCE_RESET_MODEM_CHANNEL); 
    }else{
        DBGLOG(CCCI, WAR, "CH%d write NULL skb to kick DF process!", chn);
    }
	if(pccci_h->channel == CCCI_FORCE_RESET_MODEM_CHANNEL){
	    tx_queue_idx = 0;
		hif_ul_write_swq(tx_queue_idx, skb);
    }else{
#ifdef __EEMCS_EXPT_SUPPORT__
        if(is_exception_mode(&mode))
        {
            if(is_valid_exception_tx_channel(chn))
            {
                expt_port_info = get_expt_port_info(ccci_ch_to_port(chn));
                /* set exception TX Q*/
                tx_queue_idx = expt_port_info->expt_txq_id;
                DBGLOG(CCCI, DBG, "[EXPT] ccci_UL_write_skb_to_swq write skb to DF: ch=%d, txq=%d", chn, tx_queue_idx);
                hif_ul_write_swq(tx_queue_idx, skb);
        	    atomic_dec(&ccci_port_info[ccci_ch_to_port(chn)].reserve_space);
        	    atomic_dec(&ccci_tx_waitq[tx_queue_idx].reserve_space);
                eemcs_update_statistics(0, ccci_ch_to_port(chn), TX, NORMAL);
            }
            else
            {
                DBGLOG(CCCI, WAR, "[EXPT] Invalid exception channel(%d)!", chn);
                /*
                 * if KAL_FAIL is returned, skb is freed at device layer
                 * we don't have to free it here
                 */
                 //eemcs_ex_ccci_tx_drop(ccci_ch_to_port(chn));
                 ret = KAL_FAIL;
            }
        }
        else
#endif
        {
    	    tx_queue_idx = ccci_port_info[ccci_ch_to_port(chn)].txq_id;
    		hif_ul_write_swq(tx_queue_idx, skb);
    	    atomic_dec(&ccci_port_info[ccci_ch_to_port(chn)].reserve_space);
    	    atomic_dec(&ccci_tx_waitq[tx_queue_idx].reserve_space);
            eemcs_update_statistics(0, ccci_ch_to_port(chn), TX, NORMAL);
        }
	}
	
	DEBUG_LOG_FUNCTION_LEAVE;
	return ret;
}

KAL_UINT32 eemcs_ccci_boot_UL_write_room_check(void)
{
    KAL_UINT32   boot_port_id = 0;
    KAL_UINT32   tx_queue_idx=0;
    KAL_UINT32   ret = 0;

    boot_port_id = ccci_ch_to_port(CH_CTRL_TX);
    tx_queue_idx = ccci_port_info[boot_port_id].txq_id;
    ret = hif_ul_swq_space(tx_queue_idx);

    return ret;
}

KAL_INT32 eemcs_ccci_boot_UL_write_skb_to_swq(struct sk_buff *skb)
{
    XBOOT_CMD *p_xcmd = NULL;
    KAL_UINT32 tx_queue_idx = 0;
    KAL_INT32 ret = KAL_SUCCESS;
#ifdef __EEMCS_EXPT_SUPPORT__
    ccci_expt_port_cfg *expt_port_info;
    EEMCS_EXCEPTION_STATE mode = EEMCS_EX_INVALID;
#endif
    DEBUG_LOG_FUNCTION_ENTRY;

    // Use boot state, not EEMCS state
    //if (check_device_state() < EEMCS_MOLY_HS_P1) {
    if (eemcs_boot_get_state() < MD_ROM_BOOT_READY) {

        if (NULL != skb){
            p_xcmd = (XBOOT_CMD *)skb->data;
            if (p_xcmd->magic == (unsigned int)MAGIC_MD_CMD_ACK) {
                KAL_ASSERT(p_xcmd->msg_id < CMDID_MAX);
                DBGLOG(CCCI, DBG, "XBOOT_CMD: [TX]0x%X, 0x%X, 0x%X, 0x%X",
                    p_xcmd->magic, p_xcmd->msg_id, p_xcmd->status, p_xcmd->reserved[0]);
            } else {
                KAL_ASSERT(skb->len > 0);
                DBGLOG(CCCI, DBG, "XBOOT_BIN: get %dByte bin to md", skb->len);
            }
        } else {
            DBGLOG(CCCI, WAR, "CH_CTRL_TX write NULL skb to kick DF process!");
        }
#ifdef __EEMCS_EXPT_SUPPORT__
        if(is_exception_mode(&mode))
        {
            if(is_valid_exception_tx_channel(CH_CTRL_TX))
            {
                expt_port_info = get_expt_port_info(ccci_ch_to_port(CH_CTRL_TX));
                /* set exception TX Q*/
                tx_queue_idx = expt_port_info->expt_txq_id;
                DBGLOG(CCCI, DBG, "[EXPT]boot_write_skb_to_swq: ch=%d, txq=%d", CH_CTRL_TX, tx_queue_idx);
                hif_ul_write_swq(tx_queue_idx, skb);
            }
            else
            {
                DBGLOG(CCCI, ERR, "[EXPT]Invalid exception channel(%d)", CH_CTRL_TX);
                /*
                 * if KAL_FAIL is returned, skb is freed at device layer
                 * we don't have to free it here
                 */
                 //eemcs_ex_ccci_tx_drop(ccci_ch_to_port(chn));
                 ret = KAL_FAIL;
            }
        }
        else
#endif 
        {
            tx_queue_idx = ccci_port_info[ccci_ch_to_port(CH_CTRL_TX)].txq_id;
            hif_ul_write_swq(tx_queue_idx, skb);
        }
    } else {
        eemcs_ccci_UL_write_skb_to_swq(CH_CTRL_TX, skb);
    }

    DEBUG_LOG_FUNCTION_LEAVE;
    return ret;
}

/*
 * @brief Check if a incomming buffer is a xBoot command.
 * @param
 *     skb [in] The incomming socket buffer.
 * @return If a incomming buffer is a xBoot command, true is returned.
 *         Otherwise, false is returned.
 */
int is_xboot_command(struct sk_buff *skb)
{
    int result = false;
    XBOOT_CMD *p_xcmd = NULL;

    DEBUG_LOG_FUNCTION_ENTRY;
    if (check_device_state() >= EEMCS_MOLY_HS_P1) {
        DBGLOG(CCCI, DBG, "Current EEMCS state is %d. Should not received a xBoot command !!", check_device_state());
    } else {
        p_xcmd = (XBOOT_CMD *)skb->data;
        if (p_xcmd->magic == (KAL_UINT32)MAGIC_MD_CMD)
            result = true;
        else
            DBGLOG(CCCI, DBG, "This is not a xBoot command (0x%X)(0x%X)(0x%X)",
                p_xcmd->magic, p_xcmd->msg_id, p_xcmd->status);
    }
    DEBUG_LOG_FUNCTION_LEAVE;
    return result;
}

int ccci_df_to_ccci_callback(unsigned int rxq_no)
{
    int ret, hc_ret;
    bool is_xcmd = false;
    struct sk_buff * skb = NULL;
    CCCI_BUFF_T *ccci_h  = NULL;
    XBOOT_CMD *p_xcmd = NULL;
    KAL_UINT32   port_id = CCCI_PORT_CTRL;
    static KAL_UINT32 rx_err_cnt[CCCI_PORT_NUM_MAX] = {0};
#ifdef __EEMCS_EXPT_SUPPORT__
    EEMCS_EXCEPTION_STATE mode = EEMCS_EX_INVALID;
#endif

    
    DEBUG_LOG_FUNCTION_ENTRY;
    /* Step 1. read skb from swq */
    skb = hif_dl_read_swq(rxq_no);
    if(skb == NULL) {
    	DBGLOG(CCCI, DBG, "ccci_df_to_ccci_callback read NULL skb on %d", rxq_no);
    	if(is_exception_mode(&mode))
    		return KAL_FAIL;
    	else
    		KAL_ASSERT(NULL != skb);
    }

    /* Step 2. call handle complete */
    hc_ret = hif_dl_pkt_handle_complete(rxq_no);
    KAL_ASSERT(0 == hc_ret);

    wake_lock_timeout(&eemcs_wake_lock, HZ/2); // Using 0.5s wake lock

	/* Step 3. buffer type */
    if (rxq_no == RXQ_Q0) {
        //is_xcmd = is_xboot_command(skb);
        p_xcmd = (XBOOT_CMD *)skb->data;
	    if (p_xcmd->magic == (KAL_UINT32)MAGIC_MD_CMD) {
			if (check_device_state() >= EEMCS_MOLY_HS_P1) {
        		DBGLOG(CCCI, ERR, "can't recv xBoot cmd when EEMCS state=%d", check_device_state());
    		} else {
    			is_xcmd = true;
	    	}
	    }
    }

    if (is_xcmd) {
        /* Step 4. callback to xBoot */
    	CDEV_LOG(port_id, CCCI, INF, "XBOOT_CMD: 0x%08X, 0x%08X, 0x%08X, 0x%08X",\
        	p_xcmd->magic, p_xcmd->msg_id, p_xcmd->status, p_xcmd->reserved[0]);
    	ret = ccci_port_info[port_id].ch.rx_cb(skb, 0);
    } else {
    	ccci_h = (CCCI_BUFF_T *)skb->data;
    	port_id = ccci_ch_to_port(ccci_h->channel);		
    	CDEV_LOG(port_id, CCCI, INF, "CCCI_H: 0x%08X, 0x%08X, 0x%08X, 0x%08X",\
        	ccci_h->data[0],ccci_h->data[1],ccci_h->channel, ccci_h->reserved);
        /* Step 4. callback to CCCI device */       
        if(NULL != ccci_port_info[port_id].ch.rx_cb){
            #ifdef __EEMCS_EXPT_SUPPORT__
            if(is_exception_mode(&mode))
            {
                if(!is_valid_exception_port(port_id, true))
                {
                    ret = KAL_FAIL;
                    dev_kfree_skb(skb);
                    eemcs_ccci_release_rx_skb(port_id, 1, skb);
                    eemcs_expt_ccci_rx_drop(port_id);
                    DBGLOG(CCCI, ERR, "PKT DROP when PORT%d(rxq=%d) at md exception", \
						port_id, rxq_no);
                    goto _end;
                } else {
                    ret = ccci_port_info[port_id].ch.rx_cb(skb, 0);
                }
            }
            else
            #endif      
            {
                ret = ccci_port_info[port_id].ch.rx_cb(skb, 0);
            }
            rx_err_cnt[port_id] = 0;
        } else { 
            ret = KAL_FAIL;
            dev_kfree_skb(skb);
            eemcs_ccci_release_rx_skb(port_id, 1, skb);
            if (rx_err_cnt[port_id]%20 == 0) {
            	DBGLOG(CCCI, ERR, "PKT DROP when PORT%d rx callback(ch=%d) not registered", \
					port_id, ccci_h->channel);
            }
            rx_err_cnt[port_id]++;
            eemcs_update_statistics(0, port_id, RX, DROP);
			
        }
        eemcs_update_statistics(0, port_id, RX, NORMAL);
    }
    
_end:    
    DEBUG_LOG_FUNCTION_LEAVE;
    return ret;
}


int ccci_df_to_ccci_tx_callback(unsigned int txq_no)
{
    int space=hif_ul_swq_space(txq_no) - atomic_read(&ccci_tx_waitq[txq_no].reserve_space);
    
    DEBUG_LOG_FUNCTION_ENTRY;

    wake_up_nr(&ccci_tx_waitq[txq_no].tx_waitq, space);
    DBGLOG(CCCI, TRA, "ccci_tx_callback: txq_no=%d, wake task num=%d", txq_no, space);    

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief Software interrupt callback function to upper layer.
 * @param
 *     swint_status [in] The software interrupt status.
 * @return KAL_SUCCESS is returned always.
 */
int ccci_df_to_ccci_swint_callback(unsigned int swint_status)
{

    spin_lock(&swint_cb_lock);
    if(ccci_swint_cb == NULL)
    {
        DBGLOG(CCCI, ERR, "ccci_swint_cb is NULL");
        spin_unlock(&swint_cb_lock);
        return KAL_FAIL;
    }else{
        ccci_swint_cb(swint_status);
        spin_unlock(&swint_cb_lock);
        return KAL_SUCCESS;
    }   
}

/*
 * @brief Software interrupt callback function to upper layer.
 * @param
 *     swint_status [in] The software interrupt status.
 * @return KAL_SUCCESS is returned always.
 */
int ccci_df_to_ccci_WDT_callback(unsigned int WDT)
{
    if(ccci_WDT_cb == NULL){
        DBGLOG(CCCI, DBG, "ccci_df_to_ccci_WDT_callback: ccci_WDT_cb is NULL");
        return KAL_FAIL;
    }
    ccci_WDT_cb();

    return KAL_SUCCESS;
}

KAL_INT32 eemcs_ccci_mod_init(void)
{
    KAL_UINT32 i, ret;

    // 4 <1> register ccci to HIF
    for(i = 0; i<RXQ_NUM; i++)
    {
        ret = hif_reg_rx_cb(i, ccci_df_to_ccci_callback, 0);
        /* this API only return KAL_SUCCESS*/
        KAL_ASSERT(ret == KAL_SUCCESS);
    }
    for(i = 0; i<TXQ_NUM; i++)
    {
        ret = hif_reg_tx_cb(i, ccci_df_to_ccci_tx_callback, 0);
        init_waitqueue_head(&ccci_tx_waitq[i].tx_waitq);
        atomic_set(&ccci_tx_waitq[i].reserve_space, 0);
        /* this API only return KAL_SUCCESS*/
        KAL_ASSERT(ret == KAL_SUCCESS);
    }
    // 4 <2> register ccci swint to HIF
    spin_lock_init(&swint_cb_lock);
    ccci_swint_cb = NULL;
    ret = hif_reg_swint_cb(ccci_df_to_ccci_swint_callback);
    KAL_ASSERT(ret == KAL_SUCCESS);

    ret = hif_reg_WDT_cb(ccci_df_to_ccci_WDT_callback);
    KAL_ASSERT(ret == KAL_SUCCESS);
    
    // 4 <3> initialize ccci_ch_to_port_mapping
    for(i = 0; i<CH_NUM_MAX; i++){
        ccci_ch_to_port_mapping[i] = 0xff;
    }
    for(i = 0; i<CCCI_PORT_NUM_MAX; i++){
        if ((ccci_port_info[i].ch.tx != CH_DUMMY) && (ccci_port_info[i].ch.rx != CH_DUMMY)) {
	    ccci_ch_to_port_mapping[ccci_port_info[i].ch.tx] = i;
	    ccci_ch_to_port_mapping[ccci_port_info[i].ch.rx] = i;
	    if (ccci_port_info[i].rx_flow_ctrl_limit) {
		mtlte_df_Init_DL_flow_ctrl(SDIO_RXQ(ccci_port_info[i].rxq_id), false, \
			ccci_port_info[i].rx_flow_ctrl_limit, ccci_port_info[i].rx_flow_ctrl_thresh);
	    }
        }
    }
 
    // please fix the way of hardcode rx skb size
    mtlte_df_DL_set_skb_alloc_size_depth(0, DEV_MAX_PKT_SIZE, 256);
    mtlte_df_DL_set_skb_alloc_size_depth(1, DEV_MAX_PKT_SIZE, 256);
    mtlte_df_DL_set_skb_alloc_size_depth(2, DEV_MAX_PKT_SIZE, 512);
    mtlte_df_DL_set_skb_alloc_size_depth(3, 1600, 512);
    
    snprintf(eemcs_wakelock_name, sizeof(eemcs_wakelock_name), "eemcs_wakelock");
    wake_lock_init(&eemcs_wake_lock, WAKE_LOCK_SUSPEND, eemcs_wakelock_name);    
    wake_lock_init(&sdio_wake_lock, WAKE_LOCK_SUSPEND, "sdio_wakelock");    
    
#ifdef _EEMCS_CCCI_LB_UT
    ccci_ut_init_probe();
#endif
   
    return KAL_SUCCESS;
}

void eemcs_ccci_exit(void)
{
    KAL_UINT32 i;

    hif_unreg_swint_cb();
    for(i = 0; i<RXQ_NUM; i++)
    {
        hif_unreg_rx_cb(i);
    }

    wake_lock_destroy(&eemcs_wake_lock);
	wake_lock_destroy(&sdio_wake_lock);
    
#ifdef _EEMCS_CCCI_LB_UT
    ccci_ut_exit();
#endif
    return;
}

#if defined(_EEMCS_CCCI_LB_UT)
//#if defined(_EEMCS_EXCEPTION_UT)
//extern EEMCS_EXPT_UT_SET g_expt_ut_inst;
//#endif
extern KAL_UINT32 ccci_is_net_ch(KAL_UINT32 cccich);
void ccci_ul_lb_channel(struct sk_buff *skb){
        CCCI_BUFF_T *pccci_h;
        KAL_UINT32 port_id, tx_ch, rx_ch;

        pccci_h = (CCCI_BUFF_T *)skb->data;
        port_id = ccci_ch_to_port(pccci_h->channel);
        tx_ch = pccci_h->channel;
        rx_ch =  ccci_port_info[port_id].ch.rx;
        pccci_h->channel = rx_ch;

        DBGLOG(CCCI,DBG, "[CCCI_UT]=========PORT(%d) tx_ch(%d) LB to rx_ch(%d)",\
            port_id, tx_ch, rx_ch);

        if(KAL_SUCCESS == ccci_is_net_ch(tx_ch))
        {
            eccmni_swap(skb);
        }
}

KAL_UINT32 ccci_ul_lb_queue(struct sk_buff *skb)
{
    CCCI_BUFF_T *pccci_h;
    KAL_UINT32 port_id;
    KAL_UINT32 rx_qno;
    ccci_port_cfg *ccci_port_info;
#ifdef _EEMCS_EXCEPTION_UT
    ccci_expt_port_cfg *ccci_expt_port_info;
#endif

    pccci_h = (CCCI_BUFF_T *)skb->data;
    port_id = ccci_ch_to_port(pccci_h->channel);
    
    DBGLOG(CCCI,DBG, "[CCCI_UT]=========tx_ch(%d) is mapped to PORT(%d)", pccci_h->channel, port_id);
#ifdef _EEMCS_EXCEPTION_UT
    if(is_exception_mode(NULL))
    {
        ccci_expt_port_info = get_expt_port_info(port_id);
        rx_qno = SDIO_RXQ(ccci_expt_port_info->expt_rxq_id);
    }
    else
#endif
    {
        ccci_port_info = ccci_get_port_info(port_id);
        rx_qno = SDIO_RXQ(ccci_port_info->rxq_id);
    }
    DBGLOG(CCCI,DBG, "[CCCI_UT]=========Loopback Rxqno(%d)", rx_qno);
    return rx_qno;
}

struct sk_buff_head ccci_ut_queue[RXQ_NUM];

KAL_UINT32 ccci_is_net_ch(KAL_UINT32 cccich)
{
    KAL_UINT32 ret = KAL_FAIL;
    switch(cccich)
    {
        case CH_NET1_RX:
        case CH_NET1_TX:
        case CH_NET2_RX:
        case CH_NET2_TX:
        case CH_NET3_RX:
        case CH_NET3_TX:
            ret = KAL_SUCCESS;
            break;
        default:
            ret = KAL_FAIL;
            break;
    }
    return ret;
}

/* UL APIs */
int ccci_ut_UL_write_skb_to_swq(MTLTE_DF_TX_QUEUE_TYPE qno , struct sk_buff *skb)
{
#ifdef _EEMCS_EXCEPTION_UT 
    EEMCS_EXPT_UT_SET *eemcs_expt_inst = eemcs_expt_get_inst(); 
#endif
	DEBUG_LOG_FUNCTION_ENTRY;
    
    if(NULL != skb)
    {
        DBGLOG(CCCI,DBG, "[CCCI_UT] queue(%d) Tx",qno);
#ifdef _EEMCS_CCCI_LB_UT
#if defined(__EEMCS_XBOOT_SUPPORT__) && defined(_EEMCS_BOOT_UT)
        if (!eemcs_device_ready()) {
            ccci_boot_write_desc_to_q(skb);
            return KAL_SUCCESS;
        } else

#endif // __EEMCS_XBOOT_SUPPORT__ && _EEMCS_BOOT_UT
        {      
#ifdef _EEMCS_EXCEPTION_UT 
            if(eemcs_expt_inst->enable_ccci_lb == 1)
#endif /*_EEMCS_EXCEPTION_UT*/
            {
                struct sk_buff *new_skb;
                KAL_UINT32 lb_rx_qno;
                new_skb = dev_alloc_skb(skb->len);
                if(new_skb == NULL){
                    DBGLOG(CCCI,ERR,"[CCCI_UT] ccci_ut_UL_write_skb_to_swq dev_alloc_skb fail sz(%d).", skb->len);
                    dev_kfree_skb(skb);
                    DEBUG_LOG_FUNCTION_LEAVE;
            	    return KAL_SUCCESS;
                }        
                memcpy(skb_put(new_skb, skb->len), skb->data, skb->len);
                ccci_ul_lb_channel(new_skb);

#ifdef _EEMCS_EXCEPTION_UT 
                /*Get Rx loopback Queue number*/
                lb_rx_qno = ccci_ul_lb_queue(new_skb);
#else
                lb_rx_qno = qno;
#endif /*_EEMCS_EXCEPTION_UT*/

                skb_queue_tail(&ccci_ut_queue[lb_rx_qno], new_skb);
                ccci_df_to_ccci_callback(lb_rx_qno);
            }
        }
#else
        DBGLOG(CCCI,DBG, "[CCCI_UT]========= DROP!!!");
#endif
        dev_kfree_skb(skb);    
    }else{
        DBGLOG(CCCI,DBG, "[CCCI_UT] fake kick DF !!!");
    }
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS;
}

int ccci_ut_UL_swq_space(MTLTE_DF_TX_QUEUE_TYPE qno){
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(CCCI,DBG, "[CCCI_UT] queue(%d) Query Tx SZ 99",qno);
    DEBUG_LOG_FUNCTION_LEAVE;
    return 99;
}

int ccci_ut_register_swint_callback(MTLTE_DF_TO_DEV_CALLBACK func_ptr)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS;
}

int ccci_ut_unregister_swint_callback(void)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS;
}

int ccci_ut_register_WDT_callback(MTLTE_DF_TO_DEV_CALLBACK func_ptr)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS;
}

int ccci_ut_unregister_WDT_callback(void)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS;
}

/* DL APIs */
struct sk_buff * ccci_ut_DL_read_skb_from_swq(MTLTE_DF_RX_QUEUE_TYPE qno){
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return skb_dequeue(&ccci_ut_queue[qno]);
}

int ccci_ut_DL_pkt_handle_complete(MTLTE_DF_RX_QUEUE_TYPE qno)
{
    /* please wei-de comment what is this function used for */
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(CCCI,DBG, "[CCCI_UT] queue(%d) handle complete",qno);
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS;
}

int ccci_ut_register_rx_callback(MTLTE_DF_RX_QUEUE_TYPE qno, MTLTE_DF_TO_DEV_CALLBACK func_ptr , unsigned int private_data)
{
	DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(CCCI,DBG, "[CCCI_UT] queue (%d) register cb",qno);
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS;
}
void ccci_ut_unregister_rx_callback(MTLTE_DF_RX_QUEUE_TYPE qno)
{
	DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(CCCI,DBG, "[CCCI_UT] queue(%d) Register Callback Function",qno);
    DEBUG_LOG_FUNCTION_LEAVE;
}

int ccci_ut_register_tx_callback(MTLTE_DF_TX_QUEUE_TYPE qno, MTLTE_DF_TO_DEV_CALLBACK func_ptr , unsigned int private_data)
{
	DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(CCCI,DBG, "[CCCI_UT] queue (%d) register tx cb",qno);
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS;
}
void ccci_ut_unregister_tx_callback(MTLTE_DF_TX_QUEUE_TYPE qno)
{
	DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(CCCI,DBG, "[CCCI_UT] queue(%d) Register TX Callback Function",qno);
    DEBUG_LOG_FUNCTION_LEAVE;
}

void ccci_ut_clean_txq_count(void)
{
	DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
}

void ccci_ut_init_probe(void)
{
    KAL_UINT32 i;
    for(i = 0; i<RXQ_NUM; i++)
    {
        skb_queue_head_init(&ccci_ut_queue[i]);
    }
}
void ccci_ut_exit(void)
{
    KAL_UINT32 i;
    for(i = 0; i<RXQ_NUM; i++)
    {
        skb_queue_purge(&ccci_ut_queue[i]);
    }
}
void ccci_ut_expt_q_num_init(KAL_UINT32 nonstop_q, KAL_UINT32 except_q)
{
#ifdef _EEMCS_EXCEPTION_UT    
    EEMCS_EXPT_UT_SET *eemcs_expt_inst = eemcs_expt_get_inst();
    DEBUG_LOG_FUNCTION_ENTRY;

    DBGLOG(EXPT,TRA, "[CCCI_EXPT_UT] exption q num init");
    eemcs_expt_inst->nonstop_q = nonstop_q;
    eemcs_expt_inst->expt_txq = (0xFFFF0000 & except_q) >> 16;
    eemcs_expt_inst->expt_rxq = 0x0000FFFF & except_q;    
    DBGLOG(EXPT,TRA, "[CCCI_EXPT_UT] nonstop_q(%x) expt_txq(%x) expt_rxq(%x)", eemcs_expt_inst->nonstop_q, eemcs_expt_inst->expt_txq, eemcs_expt_inst->expt_rxq);
    DEBUG_LOG_FUNCTION_LEAVE;
#else
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
#endif    
}
int ccci_ut_register_expt_callback(EEMCS_CCCI_EX_IND func_ptr)
{
#ifdef _EEMCS_EXCEPTION_UT    
    EEMCS_EXPT_UT_SET *eemcs_expt_inst = eemcs_expt_get_inst();
	DEBUG_LOG_FUNCTION_ENTRY;

    DBGLOG(EXPT,TRA, "[CCCI_EXPT_UT] register_expt_callback");
    eemcs_expt_inst->ccci_expt_cb = func_ptr;
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS;
#else
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS;
#endif
}
void ccci_ut_turnoff_DL_port(MTLTE_DF_RX_QUEUE_TYPE qno)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(CCCI,DBG, "[CCCI_UT] queue(%d) is turned off",qno);
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS;
}
void ccci_ut_turnon_DL_port(MTLTE_DF_RX_QUEUE_TYPE qno)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(CCCI,DBG, "[CCCI_UT] queue(%d) is turned on",qno);
    DEBUG_LOG_FUNCTION_LEAVE;
	return KAL_SUCCESS;
}

#endif /*_EEMCS_CCCI_LB_UT*/
