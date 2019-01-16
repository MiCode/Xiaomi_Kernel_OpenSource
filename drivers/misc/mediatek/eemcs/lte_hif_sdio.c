#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include "eemcs_kal.h"
#include "lte_dev_test.h"
#include "lte_df_main.h"
#include "lte_hif_sdio.h"
#include <linux/cpumask.h>
#include <linux/kthread.h>


extern void lte_sdio_trigger_wakedevice(void);
extern void lte_sdio_turnoff_wakedevice(void);

extern struct mtlte_dev *lte_dev_p ;
extern unsigned int LTE_WD_timeout_indicator;

#if INTEGRATION_DEBUG
//struct timespec lte_time_before_t, lte_time_now_t;
KAL_UINT32 lte_jiffies_before, lte_jiffies_now, lte_time_diff_sec;
KAL_UINT32 rxq_wait_big_buf_time = 0;
KAL_UINT32 rxq_workq_buf_exe_time = 0;
KAL_UINT32 rxq_workq_get_skb_time = 0;
KAL_UINT32 rxq_workq_callback_user_time = 0;

KAL_UINT32 eemcs_sdio_throughput_log = 1;
KAL_UINT32 log_sdio_ul_now = 0;
KAL_UINT32 log_sdio_dl_now = 0;
KAL_UINT32 log_sdio_ul_history = 1;
KAL_UINT32 log_sdio_dl_history = 1;
KAL_UINT32 log_sdio_buf_pool = 0;
KAL_UINT32 log_sdio_ul_txqcnt= 0;

KAL_UINT32 log_sdio_print_now = 0;

KAL_UINT32 sdio_rxq_skb_log[RXQ_NUM];
KAL_UINT32 sdio_txq_skb_log[TXQ_NUM];
#endif

static hif_sdio_handle hif_sdio_handler ;

//#define TCP_OUT_OF_ORDER_SOLUTION     //alloc_workqueue with WQ_NON_REETANET
#if defined (TCP_OUT_OF_ORDER_SOLUTION)
static KAL_INT32 lte_online_cpu;
#endif


//----------------------------------------------------//
#if TEST_DRV
static KAL_UINT32 abnormal_int_count;
//KAL_UINT32 abnormal_int_count;
static KAL_UINT32 stop_when_abnormal = 1;

KAL_UINT32 test_rx_tail_change = 1;
KAL_UINT32 test_rx_pkt_cnt_q0 = MT_LTE_RXQ0_MAX_PKT_REPORT_NUM;
KAL_UINT32 test_rx_pkt_cnt_q1 = MT_LTE_RXQ1_MAX_PKT_REPORT_NUM;
KAL_UINT32 test_rx_pkt_cnt_q2 = MT_LTE_RXQ2_MAX_PKT_REPORT_NUM;
KAL_UINT32 test_rx_pkt_cnt_q3 = MT_LTE_RXQ3_MAX_PKT_REPORT_NUM;

#define test_rx_tail_len (4+4+4+2+2+2+2+2*(test_rx_pkt_cnt_q0+test_rx_pkt_cnt_q1+test_rx_pkt_cnt_q2+test_rx_pkt_cnt_q3)+4+4)

KAL_UINT32 ABNORMAL_INT_LIMIT = 50;
#endif

#if 0
static KAL_UINT32 tx_port1_process_priority[TXQ_PORT1_Q_NUM] = {
        TXQ_Q0,
        TXQ_Q1,
		TXQ_Q2,
		TXQ_Q3,
		TXQ_Q4,
		TXQ_Q5,
		TXQ_Q6,
} ;

static KAL_UINT32 rx_port_process_priority[RXQ_NUM] = {
		RXQ_Q0,
		RXQ_Q1,
		RXQ_Q2,
		RXQ_Q3,
} ;
#endif

static sdio_tx_queue_info tx_queue_info[TXQ_NUM] = {
    {TXQ_Q0,	SDIO_IP_WTDR1},
    {TXQ_Q1,	SDIO_IP_WTDR1},
    {TXQ_Q2,	SDIO_IP_WTDR1},
    {TXQ_Q3,	SDIO_IP_WTDR1},
    {TXQ_Q4,	SDIO_IP_WTDR1},
    {TXQ_Q5,	SDIO_IP_WTDR1},
    {TXQ_Q6,	SDIO_IP_WTDR1},
};


static sdio_rx_queue_info rx_queue_info[RXQ_NUM] = {
	{RXQ_Q0,	SDIO_IP_WRDR0,	MT_LTE_RX_Q0_PKT_CNT,	MT_LTE_RX_SWQ_Q0_TH},
	{RXQ_Q1, 	SDIO_IP_WRDR1,	MT_LTE_RX_Q1_PKT_CNT,	MT_LTE_RX_SWQ_Q1_TH},
	{RXQ_Q2,	SDIO_IP_WRDR2,	MT_LTE_RX_Q2_PKT_CNT,	MT_LTE_RX_SWQ_Q2_TH},
	{RXQ_Q3,	SDIO_IP_WRDR3,	MT_LTE_RX_Q3_PKT_CNT,	MT_LTE_RX_SWQ_Q3_TH},
};

inline KAL_UINT32 mtlte_hif_sdio_tx_chk_pkt_num(KAL_UINT32 txqno);
inline KAL_UINT32 mtlte_hif_sdio_rx_chk_pkt_num(KAL_UINT32 rxqno);
static SDIO_TXRXWORK_RESP mtlte_hif_sdio_tx(KAL_UINT32 txqno);
static SDIO_TXRXWORK_RESP mtlte_hif_sdio_rx(KAL_UINT32 rxqno);

/* real time queue priority */
#if 0
static sdio_proc_queue_handle proc_normal_queue_handle[] = {
	{TXQ_Q0,		1,	mtlte_hif_sdio_tx_chk_pkt_num, 		mtlte_hif_sdio_tx},	// Priority 1 , TX , for Real time application send packet
	{RXQ_Q0,		0,	mtlte_hif_sdio_rx_chk_pkt_num,		mtlte_hif_sdio_rx},	// Priority 2 , RX , for Real time application receive packet
	{TXQ_Q1,		1,	mtlte_hif_sdio_tx_chk_pkt_num, 		mtlte_hif_sdio_tx},	// Priority 3 , TX , for control-Mail Box and control-stream send packet	
	{RXQ_Q1,		0,	mtlte_hif_sdio_rx_chk_pkt_num,		mtlte_hif_sdio_rx},	// Priority 4 , RX , for Control path receive packet
	{TXQ_Q2,		1,	mtlte_hif_sdio_tx_chk_pkt_num, 		mtlte_hif_sdio_tx},	// Priority 5 , TX , for NET Interface 0 send packet
	{TXQ_Q3,		1,	mtlte_hif_sdio_tx_chk_pkt_num, 		mtlte_hif_sdio_tx},	// Priority 6 , TX , for NET Interface 1 send packet
	{TXQ_Q4,		1,	mtlte_hif_sdio_tx_chk_pkt_num, 		mtlte_hif_sdio_tx},	// Priority 7 , TX , for NET Interface 2 send packet
	{RXQ_Q3,		0,	mtlte_hif_sdio_rx_chk_pkt_num,		mtlte_hif_sdio_rx},	// Priority 8 , RX , for NET Interface receive packet
	{RXQ_Q2,		0,	mtlte_hif_sdio_rx_chk_pkt_num,		mtlte_hif_sdio_rx},	// Priority 9 , RX , for LOG receive packet
};

static KAL_UINT32 proc_normal_queue_num = sizeof(proc_normal_queue_handle)/sizeof(sdio_proc_queue_handle) ;
#else
static sdio_proc_queue_handle proc_realtime_queue_handle[] = {
	{TXQ_Q0,		1,	mtlte_hif_sdio_tx_chk_pkt_num, 		mtlte_hif_sdio_tx},	// Priority 1 , TX , for Real time application send packet
	{RXQ_Q0,		0,	mtlte_hif_sdio_rx_chk_pkt_num,		mtlte_hif_sdio_rx},	// Priority 2 , RX , for Real time application receive packet
};

static KAL_UINT32 proc_realtime_queue_num = sizeof(proc_realtime_queue_handle)/sizeof(sdio_proc_queue_handle) ;

static sdio_proc_queue_handle proc_normal_queue_handle[] = {
	{TXQ_Q1,		1,	mtlte_hif_sdio_tx_chk_pkt_num, 		mtlte_hif_sdio_tx},	// Priority 3 , TX , for control-Mail Box and control-stream send packet	
	{RXQ_Q1,		0,	mtlte_hif_sdio_rx_chk_pkt_num,		mtlte_hif_sdio_rx},	// Priority 4 , RX , for Control path receive packet
	{TXQ_Q2,		1,	mtlte_hif_sdio_tx_chk_pkt_num, 		mtlte_hif_sdio_tx},	// Priority 5 , TX , for NET Interface 0 send packet
	{TXQ_Q3,		1,	mtlte_hif_sdio_tx_chk_pkt_num, 		mtlte_hif_sdio_tx},	// Priority 6 , TX , for NET Interface 1 send packet
	{TXQ_Q4,		1,	mtlte_hif_sdio_tx_chk_pkt_num, 		mtlte_hif_sdio_tx},	// Priority 7 , TX , for NET Interface 2 send packet
	{RXQ_Q2,		0,	mtlte_hif_sdio_rx_chk_pkt_num,		mtlte_hif_sdio_rx},	// Priority 8 , RX , for NET Interface receive packet
	{RXQ_Q3,		0,	mtlte_hif_sdio_rx_chk_pkt_num,		mtlte_hif_sdio_rx},	// Priority 9 , RX , for LOG receive packet

    {TXQ_Q5,		1,	mtlte_hif_sdio_tx_chk_pkt_num, 		mtlte_hif_sdio_tx},	// Priority ?? , TX , new Tx queue
    {TXQ_Q6,		1,	mtlte_hif_sdio_tx_chk_pkt_num, 		mtlte_hif_sdio_tx},	// Priority ?? , TX , new Tx queue
};

static KAL_UINT32 proc_normal_queue_num = sizeof(proc_normal_queue_handle)/sizeof(sdio_proc_queue_handle) ;
#endif


#if TEST_DRV
static volatile KAL_UINT32 fw_own_back_enable = 0 ;

void mtlte_hif_sdio_enable_fw_own_back(KAL_UINT32 enable)
{
	fw_own_back_enable = enable ;
    KAL_SLEEP_MSEC(1) ;

    #if USING_WAKE_MD_EINT
    lte_sdio_turnoff_wakedevice();
    #endif
    //KAL_DBGPRINT(KAL, DBG_ERROR,("[DEBUG] fw_own_back_enable = %d , enable = %d \r\n", fw_own_back_enable, enable)) ;
}

athif_test_param_t attest_setting_param;

#endif


void mtlte_hif_sdio_txrx_proc_enable(KAL_UINT32 enable)
{
	hif_sdio_handler.txrx_enable = enable ;
	return ; 

}

void mtlte_hif_sdio_clear_fw_own(void)
{
	hif_sdio_handler.fw_own = 0 ;
	return ; 

}

KAL_UINT32 mtlte_hif_sdio_check_fw_own(void)
{
	return hif_sdio_handler.fw_own ;
}

static inline KAL_INT32 mtlte_hif_sdio_give_fw_own(void)
{
	KAL_INT32 ret ; 
	KAL_UINT32 value ;

    KAL_DBGPRINT(KAL, DBG_LOUD,("[INT] mtlte_hif_sdio_give_fw_own <==========>\r\n")) ;

#if TEST_DRV
	if (fw_own_back_enable == 0){
        //KAL_DBGPRINT(KAL, DBG_ERROR,("[DEBUG] Always FW own, Do not return own back !! \r\n")) ;
		return KAL_SUCCESS ;
	}
#endif

#if USING_WAKE_MD_EINT
    lte_sdio_turnoff_wakedevice();
    KAL_DBGPRINT(KAL, DBG_LOUD,("[SDIO Sleep] Give Own to FW, turn off AP_wake_MD EINT <==========>\r\n")) ;
#endif

	value = W_FW_OWN_REQ_SET ; 
	if ((ret = sdio_func1_wr(SDIO_IP_WHLPCR, &value, 4)) != KAL_SUCCESS){
		return ret ; 
	}
	
	hif_sdio_handler.fw_own = 1 ;
    KAL_DBGPRINT(KAL, DBG_ERROR,(" [SDIO][Sleep] Succes give own to MD. \r\n")) ;
	return KAL_SUCCESS ;

}

static inline KAL_INT32 mtlte_hif_sdio_get_driver_own(void)
{

	KAL_INT32 ret ; 
	KAL_UINT32 value ;
	KAL_UINT32 cnt = 500 ;

	KAL_DBGPRINT(KAL, DBG_LOUD,("[INT] mtlte_hif_sdio_get_driver_own <==========>\r\n")) ;
	
	// already get the driver own
	if (hif_sdio_handler.fw_own == 0){
		return KAL_SUCCESS ;
	}
	
    if ((ret = sdio_func1_rd(SDIO_IP_WHLPCR, &value, 4)) != KAL_SUCCESS){
			return ret ; 
	}
	if (value & W_DRV_OWN_STATUS){  // Read will get the DRV own status
			hif_sdio_handler.fw_own = 0 ;

        #if USING_WAKE_MD_EINT
        lte_sdio_turnoff_wakedevice();
        KAL_DBGPRINT(KAL, DBG_ERROR,(" [SDIO][Sleep] Get Own back, turn off AP_wake_MD EINT <==========>\r\n")) ;
        #endif 

            KAL_DBGPRINT(KAL, DBG_ERROR,(" [SDIO][Sleep] Succes get own back to AP at first time read. \r\n")) ;
			return KAL_SUCCESS ;
	}

    
	value = W_FW_OWN_REQ_CLR ; 
	if ((ret = sdio_func1_wr(SDIO_IP_WHLPCR, &value, 4)) != KAL_SUCCESS){
		return ret ; 
	}

	while(cnt--){
		if ((ret = sdio_func1_rd(SDIO_IP_WHLPCR, &value, 4)) != KAL_SUCCESS){
			return ret ; 
		}
		if (value & W_DRV_OWN_STATUS){  // Read will get the DRV own status
			hif_sdio_handler.fw_own = 0 ;

            #if USING_WAKE_MD_EINT
            lte_sdio_turnoff_wakedevice();
            KAL_DBGPRINT(KAL, DBG_ERROR,(" [SDIO][Sleep] Get Own back, turn off AP_wake_MD EINT <==========>\r\n")) ;
            #endif 

            KAL_DBGPRINT(KAL, DBG_ERROR,(" [SDIO][Sleep] Succes get own back to AP after write request own back bit. \r\n")) ;
			break ;
		}

		if (LTE_WD_timeout_indicator == 1)
			break;

		KAL_SLEEP_USEC(1000) ;
	}
			
	return (hif_sdio_handler.fw_own == 0)? KAL_SUCCESS : KAL_FAIL ;

}


#if USING_WAKE_MD_EINT
void mtlte_hif_sdio_wake_MD_up_EINT(void)
{
    KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO][Sleep] trigger AP_wake_MD EINT <==========>\r\n")) ;
    lte_sdio_trigger_wakedevice();

    if (mt_get_gpio_mode(GPIO_LTE_WK_MD_PIN) != GPIO_MODE_00)
	KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO][Sleep] GPIO MODE Wrong: %d\r\n", mt_get_gpio_mode(GPIO_LTE_WK_MD_PIN))) ;
}
#endif


static KAL_INT32 mtlte_hif_sdio_mask_interrupt(void)
{
	KAL_UINT32 value ;
	KAL_INT32 ret ;

	KAL_DBGPRINT(KAL, DBG_LOUD,("[INT] mtlte_hif_sdio_mask_interrupt <==========>\r\n")) ;

	value = 0 ; 
	if ((ret = sdio_func1_wr(SDIO_IP_WHIER, &value, 4)) != KAL_SUCCESS){
		return ret ; 
	}

	return KAL_SUCCESS ;

}

static KAL_INT32 mtlte_hif_sdio_unmask_interrupt(void)
{
	KAL_UINT32 value ;
	KAL_INT32 ret ;

	KAL_DBGPRINT(KAL, DBG_LOUD,("[INT] mtlte_hif_sdio_unmask_interrupt <==========>\r\n")) ;

	value = WHISR_INT_MASK ; 
	if ((ret = sdio_func1_wr(SDIO_IP_WHIER, &value, 4)) != KAL_SUCCESS){
		return ret ; 
	}

	return KAL_SUCCESS ;

}

static inline KAL_INT32 mtlte_hif_sdio_disable_interrupt(void)
{
	KAL_UINT32 value ;
	KAL_INT32 ret ;

	KAL_DBGPRINT(KAL, DBG_LOUD,("[INT] mtlte_hif_sdio_disable_interrupt <==========>\r\n")) ;

	value = W_INT_EN_CLR ; 
	if ((ret = sdio_func1_wr(SDIO_IP_WHLPCR, &value, 4)) != KAL_SUCCESS){
		return ret ; 
	}

	return KAL_SUCCESS ;

}

static inline KAL_INT32 mtlte_hif_sdio_enable_interrupt(void)
{
	KAL_UINT32 value ;
	KAL_INT32 ret ;

	KAL_DBGPRINT(KAL, DBG_LOUD,("[INT] mtlte_hif_sdio_enable_interrupt <==========>\r\n")) ;

	value = W_INT_EN_SET ; 
	if ((ret = sdio_func1_wr(SDIO_IP_WHLPCR, &value, 4)) != KAL_SUCCESS){
		return ret ; 
	}

	return KAL_SUCCESS ;

}


#if TEST_DRV
static inline void mtlte_hif_test_drv_whisr_handle(sdio_whisr_enhance *p_enh_wxhisr)
{
	sdio_whisr *pwhisr;
    KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;
    pwhisr = &(p_enh_wxhisr->whisr) ;

    if(attest_setting_param.testing_dlq_pkt_fifo == 1){
        p_enh_wxhisr->rx0_num = 0;
        p_enh_wxhisr->rx1_num = 0;
        p_enh_wxhisr->rx2_num = 0;
        p_enh_wxhisr->rx3_num = 0;
        KAL_DBGPRINT(KAL, DBG_LOUD,("[TEST] change all rx num to 0\n")) ;
    }
    KAL_DBGPRINT(KAL, DBG_INFO,("The Rx PKT CNT update to : %d, %d, %d, %d \r\n", \
        p_enh_wxhisr->rx0_num, p_enh_wxhisr->rx1_num, p_enh_wxhisr->rx2_num, p_enh_wxhisr->rx3_num));
    
    if(attest_setting_param.testing_fifo_max == 1){
        if(p_enh_wxhisr->rx0_num > attest_setting_param.fifo_max[0]){
            attest_setting_param.test_result = KAL_FAIL;
            KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Receive %d pkt len at rx0 when set max = %d \r\n", \
                p_enh_wxhisr->rx0_num, attest_setting_param.fifo_max[0])) ;
        }
        if(p_enh_wxhisr->rx1_num > attest_setting_param.fifo_max[1]){
            attest_setting_param.test_result = KAL_FAIL;
            KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Receive %d pkt len at rx1 when set max = %d \r\n", \
                p_enh_wxhisr->rx1_num, attest_setting_param.fifo_max[1])) ;
        }
        if(p_enh_wxhisr->rx2_num > attest_setting_param.fifo_max[2]){
            attest_setting_param.test_result = KAL_FAIL;
            KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Receive %d pkt len at rx2 when set max = %d \r\n", \
                p_enh_wxhisr->rx2_num, attest_setting_param.fifo_max[2])) ;
        }
        if(p_enh_wxhisr->rx3_num > attest_setting_param.fifo_max[3]){
            attest_setting_param.test_result = KAL_FAIL;
            KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Receive %d pkt len at rx3 when set max = %d \r\n", \
                p_enh_wxhisr->rx3_num, attest_setting_param.fifo_max[3])) ;
        }
    }

    // 1. update the Tx Pkt Count
    if(attest_setting_param.testing_ulq_count == 1){ // only record it for test
        attest_setting_param.int_indicator = 1;

	    attest_setting_param.received_ulq_count[0] += p_enh_wxhisr->whtsr0.u.bits.tq0_cnt ;
        attest_setting_param.received_ulq_count[1] += p_enh_wxhisr->whtsr0.u.bits.tq1_cnt ; 
        attest_setting_param.received_ulq_count[2] += p_enh_wxhisr->whtsr0.u.bits.tq2_cnt ; 
        attest_setting_param.received_ulq_count[3] += p_enh_wxhisr->whtsr0.u.bits.tq3_cnt ; 
	    attest_setting_param.received_ulq_count[4] += p_enh_wxhisr->whtsr1.u.bits.tq4_cnt ; 
	    attest_setting_param.received_ulq_count[5] += p_enh_wxhisr->whtsr1.u.bits.tq5_cnt ; 
	    attest_setting_param.received_ulq_count[6] += p_enh_wxhisr->whtsr1.u.bits.tq6_cnt ; 
	    attest_setting_param.received_ulq_count[7] += p_enh_wxhisr->whtsr1.u.bits.tq7_cnt ; 

        KAL_DBGPRINT(KAL, DBG_INFO,("The Tx PKT CNT update to : %d, %d, %d, %d, %d, %d, %d, %d\r\n",  \
	    attest_setting_param.received_ulq_count[0],attest_setting_param.received_ulq_count[1],        \
	    attest_setting_param.received_ulq_count[2],attest_setting_param.received_ulq_count[3],        \
	    attest_setting_param.received_ulq_count[4],attest_setting_param.received_ulq_count[5],        \
	    attest_setting_param.received_ulq_count[6],attest_setting_param.received_ulq_count[7])) ;
    }
    else{
        
	    hif_sdio_handler.tx_pkt_cnt[0] += p_enh_wxhisr->whtsr0.u.bits.tq0_cnt ; 
	    hif_sdio_handler.tx_pkt_cnt[1] += p_enh_wxhisr->whtsr0.u.bits.tq1_cnt ; 
	    hif_sdio_handler.tx_pkt_cnt[2] += p_enh_wxhisr->whtsr0.u.bits.tq2_cnt ; 
	    hif_sdio_handler.tx_pkt_cnt[3] += p_enh_wxhisr->whtsr0.u.bits.tq3_cnt ; 
	    hif_sdio_handler.tx_pkt_cnt[4] += p_enh_wxhisr->whtsr1.u.bits.tq4_cnt ;
	    hif_sdio_handler.tx_pkt_cnt[5] += p_enh_wxhisr->whtsr1.u.bits.tq5_cnt ; 
	    hif_sdio_handler.tx_pkt_cnt[6] += p_enh_wxhisr->whtsr1.u.bits.tq6_cnt ; 
	    //hif_sdio_handler.tx_pkt_cnt[7] += p_enh_wxhisr->whtsr1.u.bits.tq7_cnt ; 

        KAL_DBGPRINT(KAL, DBG_INFO,("The Tx PKT CNT update to : %d, %d, %d, %d, %d, %d, %d \r\n",
	    hif_sdio_handler.tx_pkt_cnt[0],hif_sdio_handler.tx_pkt_cnt[1],hif_sdio_handler.tx_pkt_cnt[2],hif_sdio_handler.tx_pkt_cnt[3],  \
	    hif_sdio_handler.tx_pkt_cnt[4],hif_sdio_handler.tx_pkt_cnt[5],hif_sdio_handler.tx_pkt_cnt[6] /*,hif_sdio_handler.tx_pkt_cnt[7]*/)) ;	
    }

    
	// 1.5. Ack the interrupt status (Because the interrupt is turn off during ISR executing,
	//                                             and some sw_int flow control will sent same int again after we ack the original one.
	//                                             So ack operation must be executed before sw int handle.
	if (pwhisr->u.bits.fw_ownback_int || pwhisr->u.bits.d2h_sw_int){
		if(sdio_func1_wr(SDIO_IP_WHISR, &(pwhisr->u.asUINT32),4)){
			KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Ack the WHISR interrupt sdio_func1_rd fail\r\n")) ;		
		}
	}	
	
	// 2. handle the abnormal case
	if (pwhisr->u.bits.abnormal_int){
		if(sdio_func1_rd(SDIO_IP_WASR, hif_sdio_handler.sdio_wasr_cache,sizeof(sdio_wasr))){
			KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Get the abnormal interrupt sdio_func1_rd fail\r\n")) ;		
		}
		// TODO_HANDLE_THE_ABNORMAL_CASES: 	
		KAL_RAWPRINT(("[ERR] Get the abnormal interrupt status : 0x%08x\r\n", hif_sdio_handler.sdio_wasr_cache->u.asUINT32)) ;
        attest_setting_param.abnormal_status = hif_sdio_handler.sdio_wasr_cache->u.asUINT32;
        sdio_func1_wr(SDIO_IP_WASR, hif_sdio_handler.sdio_wasr_cache,sizeof(sdio_wasr));
        //if(attest_setting_param.abnormal_status == 1){
        //    p_enh_wxhisr->rx0_num = 0;
        //    p_enh_wxhisr->rx1_num = 0;
        //    p_enh_wxhisr->rx2_num = 0;
        //    p_enh_wxhisr->rx3_num = 0; 
        //}
        //KAL_DBGPRINT(KAL, DBG_LOUD,("[TEST] save the abnormal status & ack \n")) ;
        if(stop_when_abnormal == 1){
            abnormal_int_count++;
            if(abnormal_int_count > ABNORMAL_INT_LIMIT){
                while(1);
            }
        }
	}

    // 3. handle the sw interrupt case	
	// TODO_HANDLE_THE_SWINT_CASES:
	if(0 != pwhisr->u.bits.d2h_sw_int){
        
        if((unsigned int)pwhisr->u.asUINT32 & D2H_INT_PLL_START){
            KAL_DBGPRINT(KAL, DBG_ERROR,("Device PLL change start!! \n")) ;

            mtlte_hif_sdio_txrx_proc_enable(0);
            mtlte_hif_sdio_enable_fw_own_back(1);
            mtlte_hif_sdio_give_fw_own();

            KAL_SLEEP_MSEC(1) ;
            if(KAL_SUCCESS != mtlte_hif_sdio_get_driver_own()){
                KAL_DBGPRINT(KAL, DBG_ERROR,("Device PLL change flow fail !!! \n")) ;
                while(1);
            }else{
                KAL_DBGPRINT(KAL, DBG_ERROR,("Device PLL change flow success !!! \n")) ;
            }

            mtlte_hif_sdio_enable_fw_own_back(0);
            mtlte_hif_sdio_txrx_proc_enable(1);
            
        }
        if((unsigned int)pwhisr->u.asUINT32 & D2H_INT_PLL_END){
            KAL_DBGPRINT(KAL, DBG_ERROR,("Device PLL change end!! \n")) ;
        }
	    mtlte_df_swint_handle((unsigned int)pwhisr->u.asUINT32 );
	}

    KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;

}

KAL_INT32 mtlte_hif_test_drv_hw_setting(void){
	KAL_UINT32 value ;
	KAL_INT32 ret ;
	KAL_UINT16 blocksize ;

	KAL_RAWPRINT(("mtlte_hif_test_hw_setting=============>\r\n")) ;	
	
	blocksize = MT_LTE_SDIO_BLOCK_SIZE ;
	if ((ret = sdio_property_set(HDRV_SDBUS_FUNCTION_BLOCK_LENGTH, (unsigned char *)&blocksize, sizeof(KAL_INT16))) != KAL_SUCCESS){
		return ret ; 
	}
	
	value = DB_WR_BUSY_EN | DB_RD_BUSY_EN | SDIO_INT_CTL ; 
	if ((ret = sdio_func1_wr(SDIO_IP_WSDIOCSR, &value, 4)) != KAL_SUCCESS){
		return ret ; 
	}
	
	if ((ret = mtlte_hif_sdio_get_driver_own()) != KAL_SUCCESS){
		KAL_DBGPRINT(KAL, DBG_ERROR,("[ERROR] %s : mtlte_hif_sdio_get_driver_own fail !!!\r\n",KAL_FUNC_NAME)) ;	
		return ret ; 
	}
	
	value = RX_ENHANC_MODE | W_INT_CLR_CTRL;  // WHISR W'1 Clear 
	if ((ret = sdio_func1_wr(SDIO_IP_WHCR, &value, 4)) != KAL_SUCCESS){
		return ret ; 
	}
	
	KAL_RAWPRINT(("mtlte_hif_test_hw_setting<=============\r\n")) ;	

	return KAL_SUCCESS ;
}

KAL_INT32 mtlte_hif_test_drv_whisr_tail_handle(sdio_whisr_enhance* enh_whisr_cache)
{
    if(attest_setting_param.testing_dlq_int == 1){

        
        enh_whisr_cache->rx0_num = 0;
        enh_whisr_cache->rx1_num = 0;
        enh_whisr_cache->rx2_num = 0;
        enh_whisr_cache->rx3_num = 0;
    }
    return 0;
}

static void transform_whisr_rx_tail( sdio_whisr_enhance *tail_ptr)
{
    sdio_whisr_enhance orig_buf;
    void *orig_scan_ptr;
    void *tail_scan_ptr;
    KAL_UINT32 len_of_first_part;


    KAL_COPY_MEM(&orig_buf, tail_ptr, sizeof(sdio_whisr_enhance));
    len_of_first_part = sizeof(sdio_whisr) + sizeof(sdio_wtsr0) + sizeof(sdio_wtsr1) + (sizeof(KAL_UINT16)*RXQ_NUM);
    
    orig_scan_ptr = (void *)&orig_buf + len_of_first_part;
    tail_scan_ptr = (void *)tail_ptr + len_of_first_part;
    KAL_ZERO_MEM(tail_scan_ptr, sizeof(sdio_whisr_enhance)- len_of_first_part);

    KAL_COPY_MEM(tail_scan_ptr, orig_scan_ptr, sizeof(KAL_UINT16)*test_rx_pkt_cnt_q0 );
    orig_scan_ptr += sizeof(KAL_UINT16)*test_rx_pkt_cnt_q0;
    tail_scan_ptr += sizeof(KAL_UINT16)*MT_LTE_RX_Q0_PKT_CNT;

    KAL_COPY_MEM(tail_scan_ptr, orig_scan_ptr, sizeof(KAL_UINT16)*test_rx_pkt_cnt_q1 );
    orig_scan_ptr += sizeof(KAL_UINT16)*test_rx_pkt_cnt_q1;
    tail_scan_ptr += sizeof(KAL_UINT16)*MT_LTE_RX_Q1_PKT_CNT;

    KAL_COPY_MEM(tail_scan_ptr, orig_scan_ptr, sizeof(KAL_UINT16)*test_rx_pkt_cnt_q2 );
    orig_scan_ptr += sizeof(KAL_UINT16)*test_rx_pkt_cnt_q2;
    tail_scan_ptr += sizeof(KAL_UINT16)*MT_LTE_RX_Q2_PKT_CNT;

    KAL_COPY_MEM(tail_scan_ptr, orig_scan_ptr, sizeof(KAL_UINT16)*test_rx_pkt_cnt_q3 );
    orig_scan_ptr += sizeof(KAL_UINT16)*test_rx_pkt_cnt_q3;
    tail_scan_ptr += sizeof(KAL_UINT16)*MT_LTE_RX_Q3_PKT_CNT;

    // for last 2 mailbox value.
    KAL_COPY_MEM(tail_scan_ptr, orig_scan_ptr, sizeof(KAL_UINT32)*2 );
    
}


#endif


KAL_INT32 mtlte_hif_clean_txq_count(void)
{
    KAL_INT32 i;

    for (i = 0; i < TXQ_NUM; i++) {
        hif_sdio_handler.tx_pkt_cnt[i] = 0 ;
    }

    return 0;
}

int sdio_xboot_mb_wr(KAL_UINT32 *pBuffer, KAL_UINT32 len) 								
{
    int ret = KAL_FAIL;
    KAL_UINT32 H2D_sw_int = H2D_INT_xboot_H2DMB;  

    if (len == 4) {													
        ret = sdio_func1_wr(SDIO_IP_H2DSM0R, pBuffer, 4);

        //add SW int for inform
        if (ret == KAL_SUCCESS) {
            ret = sdio_func1_wr(SDIO_IP_WSICR, &H2D_sw_int, 4);
        }
    } else if (len == 8) {	
        ret = sdio_func1_wr(SDIO_IP_H2DSM0R, pBuffer, 4);

        if (ret == KAL_SUCCESS) {
            ret = sdio_func1_wr(SDIO_IP_H2DSM1R, (KAL_UINT32 *)((KAL_UINT32)pBuffer+4), 4);		
        }

        //add SW int for inform
        if (ret == KAL_SUCCESS) {
            ret = sdio_func1_wr(SDIO_IP_WSICR, &H2D_sw_int, 4);
        }
    }

    return ret;
}

int sdio_xboot_mb_rd(KAL_UINT32 *pBuffer, KAL_UINT32 len)								
{																		
    int ret = KAL_FAIL;

    if (len == 4) {													
        ret = sdio_func1_rd(SDIO_IP_D2HRM0R, pBuffer, 4);	
    } else if (len == 8) {	
        ret = sdio_func1_rd(SDIO_IP_D2HRM0R, pBuffer, 4);

        if (ret == KAL_SUCCESS) {
            ret = sdio_func1_rd(SDIO_IP_D2HRM1R, (KAL_UINT32 *)((KAL_UINT32)pBuffer+4), 4);		
        }
    }

    return ret;																
}


KAL_INT32 mtlte_hif_sdio_hw_setting(void)
{
#if TEST_DRV

	return mtlte_hif_test_drv_hw_setting() ;	

#else

	KAL_UINT32 value ;
	KAL_INT32 ret ;
	KAL_UINT16 blocksize ;

	blocksize = MT_LTE_SDIO_BLOCK_SIZE ;
	if ((ret = sdio_property_set(HDRV_SDBUS_FUNCTION_BLOCK_LENGTH, (KAL_UINT8 *)(&blocksize), sizeof(KAL_INT16))) != KAL_SUCCESS){
		return ret ; 
	}
	
	value = DB_WR_BUSY_EN | DB_RD_BUSY_EN | SDIO_INT_CTL ; 
	if ((ret = sdio_func1_wr(SDIO_IP_WSDIOCSR, &value, 4)) != KAL_SUCCESS){
		return ret ; 
	}
	
	if ((ret = mtlte_hif_sdio_get_driver_own()) != KAL_SUCCESS){
		KAL_DBGPRINT(KAL, DBG_ERROR,("[ERROR] %s : mtlte_hif_sdio_get_driver_own fail !!!\r\n",KAL_FUNC_NAME)) ;	
		return ret ; 
	}
	
	value = RX_ENHANC_MODE | W_INT_CLR_CTRL;  // WHISR W'1 Clear 
	if ((ret = sdio_func1_wr(SDIO_IP_WHCR, &value, 4)) != KAL_SUCCESS){
		return ret ; 
	}

#endif	
	return KAL_SUCCESS ;
}

KAL_INT32 mtlte_hif_sdio_wait_FW_ready()
{
	KAL_UINT32 cnt = 50 ;
	KAL_UINT32 value ;

	while (cnt--){
		if (sdio_func1_rd(SDIO_IP_WCIR, &value, 4) != KAL_SUCCESS){
			return KAL_FAIL ; 
		}	
		
		if (value & W_FUNC_RDY){
			return KAL_SUCCESS ;
		}
		KAL_SLEEP_MSEC(100) ;
	}
	
	return KAL_FAIL ;
}

void mtlte_hif_sdio_dump(void)
{
	KAL_UINT32 i ;
	KAL_UINT8 rd_value ;
	KAL_UINT32 rd_32_value ;
	KAL_INT32 ret = 0;
	
	KAL_RAWPRINT(("[WHISR_DUMP] ---------    Start    --------\r\n")) ;

	ret = sdio_func1_rd(SDIO_IP_WHLPCR, &rd_32_value, sizeof(KAL_UINT32)) ;
	if(ret){
		KAL_RAWPRINT(("[ERR] function 1 read fail : addr : 0x%x , err_ret: 0x%x\n", SDIO_IP_WHLPCR, ret)) ;	        
	}else{
		KAL_RAWPRINT(("[WHISR_DUMP] FN1 addr :0x%x, value:0x%08x\r\n",SDIO_IP_WHLPCR, rd_32_value)) ;
	}

	mtlte_hif_sdio_disable_interrupt() ;
	
	mtlte_hif_sdio_get_driver_own() ;
	
	KAL_RAWPRINT(("The Current Tx PKT CNT : %d, %d, %d, %d, %d, %d, %d \r\n",
	hif_sdio_handler.tx_pkt_cnt[0],hif_sdio_handler.tx_pkt_cnt[1],hif_sdio_handler.tx_pkt_cnt[2],hif_sdio_handler.tx_pkt_cnt[3],  \
	hif_sdio_handler.tx_pkt_cnt[4],hif_sdio_handler.tx_pkt_cnt[5],hif_sdio_handler.tx_pkt_cnt[6])) ;		

	KAL_RAWPRINT(("[WHISR_DUMP] rx0 number : %d, rx1 number : %d, rx2 number : %d, rx3 number : %d\r\n", 
	hif_sdio_handler.enh_whisr_cache->rx0_num,hif_sdio_handler.enh_whisr_cache->rx1_num,hif_sdio_handler.enh_whisr_cache->rx2_num,hif_sdio_handler.enh_whisr_cache->rx3_num)) ;
	
	KAL_RAWPRINT(("\r\n[WHISR_DUMP] rx0 pkt len : \r\n")) ;
	for (i=0; i<MT_LTE_RX_Q0_PKT_CNT ; i++){
		KAL_RAWPRINT(("	pkt[%d] = %d", i , hif_sdio_handler.enh_whisr_cache->rx0_pkt_len[i])) ;
	}
	KAL_RAWPRINT(("\r\n[WHISR_DUMP] rx1 pkt len : \r\n")) ;
	for (i=0; i<MT_LTE_RX_Q1_PKT_CNT ; i++){
		KAL_RAWPRINT(("	pkt[%d] = %d", i , hif_sdio_handler.enh_whisr_cache->rx1_pkt_len[i])) ;
	}
	KAL_RAWPRINT(("\r\n[WHISR_DUMP] rx2 pkt len : \r\n")) ;
	for (i=0; i<MT_LTE_RX_Q2_PKT_CNT ; i++){
		KAL_RAWPRINT(("	pkt[%d] = %d", i , hif_sdio_handler.enh_whisr_cache->rx2_pkt_len[i])) ;
	}
	KAL_RAWPRINT(("\r\n[WHISR_DUMP] rx3 pkt len : \r\n")) ;
	for (i=0; i<MT_LTE_RX_Q3_PKT_CNT ; i++){
		KAL_RAWPRINT(("	pkt[%d] = %d", i , hif_sdio_handler.enh_whisr_cache->rx3_pkt_len[i])) ;
	}
	KAL_RAWPRINT(("\r\n")) ;
	
	KAL_RAWPRINT(("[WHISR_DUMP] d2hrm0r :	0x%08x\r\n", hif_sdio_handler.enh_whisr_cache->d2hrm0r)) ;
	KAL_RAWPRINT(("[WHISR_DUMP] d2hrm1r :	0x%08x\r\n", hif_sdio_handler.enh_whisr_cache->d2hrm1r)) ;

	KAL_RAWPRINT(("[WHISR_DUMP] wasr :	0x%08x\r\n", hif_sdio_handler.sdio_wasr_cache->u.asUINT32)) ;
	KAL_RAWPRINT(("[WHISR_DUMP] FW_OWN :	%d\r\n", hif_sdio_handler.fw_own)) ;

	for (i=SDIO_FN0_CCCR_CSRR ; i<=SDIO_FN0_CCCR_IEXTR ; i++){
		ret = sdio_func0_rd(i, &rd_value, sizeof(KAL_UINT8)) ;
		if(ret){
			KAL_RAWPRINT(("[ERR] function 0 read fail : addr : 0x%x , err_ret: 0x%x\n", i, ret)) ;	        
		}else{
			KAL_RAWPRINT(("[WHISR_DUMP] FN0 addr :0x%x, value:0x%02x\r\n",i, rd_value)) ;
		}
	}

	for (i=SDIO_FN1_FBR_CSAR ; i<=SDIO_FN1_FBR_F1BSR+2 ; i++){
		ret = sdio_func0_rd(i, &rd_value, sizeof(KAL_UINT8)) ;
		if(ret){
			KAL_RAWPRINT(("[ERR] function 0 read fail : addr : 0x%x , err_ret: 0x%x\n", i, ret)) ;	        
		}else{
			KAL_RAWPRINT(("[WHISR_DUMP] FN0 addr :0x%x, value:0x%02x\r\n",i, rd_value)) ;
		}
	}

	for (i=SDIO_IP_WCIR ; i<=SDIO_IP_WASR ; i+=4){
		ret = sdio_func1_rd(i, &rd_32_value, sizeof(KAL_UINT32)) ;
		if(ret){
			KAL_RAWPRINT(("[ERR] function 1 read fail : addr : 0x%x , err_ret: 0x%x\n", i, ret)) ;	        
		}else{
			KAL_RAWPRINT(("[WHISR_DUMP] FN1 addr :0x%x, value:0x%08x\r\n",i, rd_32_value)) ;
		}
	}
	
	mtlte_hif_sdio_enable_interrupt(); 
	
	KAL_RAWPRINT(("[WHISR_DUMP] ---------     End     --------\r\n")) ;	
}

KAL_INT32 mtlte_hif_register_hif_to_sys_sleep_callback(void *func_ptr , KAL_UINT32 data)
{
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	
	hif_sdio_handler.sys_callback.sleep_callback_func = func_ptr ;
	hif_sdio_handler.sys_callback.sleep_private_data = data ;
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;

	return KAL_SUCCESS ; 
}

KAL_INT32 mtlte_hif_register_hif_to_sys_wake_callback(void *func_ptr , KAL_UINT32 data)
{
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	
	hif_sdio_handler.sys_callback.callback_func = func_ptr ;
	hif_sdio_handler.sys_callback.private_data = data ;
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;

	return KAL_SUCCESS ; 
}

KAL_INT32 mtlte_hif_sdio_tx_kick_process(KAL_UINT32 data)
{
	KAL_DBGPRINT(KAL, DBG_INFO,("[TX] mtlte_hif_sdio_tx_kick_process\r\n")) ;

	if (hif_sdio_handler.sys_callback.callback_func){
		return hif_sdio_handler.sys_callback.callback_func(hif_sdio_handler.sys_callback.private_data) ;
	}else{
		return KAL_FAIL ;
	}

}

KAL_INT32 mtlte_hif_sdio_rx_sleep_process(void)
{
	KAL_DBGPRINT(KAL, DBG_INFO,("[TX] mtlte_hif_sdio_rx_sleep_process\r\n")) ;

	if (hif_sdio_handler.sys_callback.sleep_callback_func){
		return hif_sdio_handler.sys_callback.sleep_callback_func(hif_sdio_handler.sys_callback.sleep_private_data) ;
	}else{
		return KAL_FAIL ;
	}

}


static inline void mtlte_hif_sdio_enhance_whisr_handle(sdio_whisr_enhance *p_enh_wxhisr)
{
#if TEST_DRV
	mtlte_hif_test_drv_whisr_handle(p_enh_wxhisr) ;;
#else
	sdio_whisr *pwhisr = &(p_enh_wxhisr->whisr) ;
	// 1. update the Tx Pkt Count
	
	hif_sdio_handler.tx_pkt_cnt[0] += p_enh_wxhisr->whtsr0.u.bits.tq0_cnt ; 
	hif_sdio_handler.tx_pkt_cnt[1] += p_enh_wxhisr->whtsr0.u.bits.tq1_cnt ; 
	hif_sdio_handler.tx_pkt_cnt[2] += p_enh_wxhisr->whtsr0.u.bits.tq2_cnt ; 
	hif_sdio_handler.tx_pkt_cnt[3] += p_enh_wxhisr->whtsr0.u.bits.tq3_cnt ;  
	hif_sdio_handler.tx_pkt_cnt[4] += p_enh_wxhisr->whtsr1.u.bits.tq4_cnt ; 
	hif_sdio_handler.tx_pkt_cnt[5] += p_enh_wxhisr->whtsr1.u.bits.tq5_cnt ; 
	hif_sdio_handler.tx_pkt_cnt[6] += p_enh_wxhisr->whtsr1.u.bits.tq6_cnt ; 
	//hif_sdio_handler.tx_pkt_cnt[7] += p_enh_wxhisr->whtsr1.u.bits.tq7_cnt ; 

	KAL_DBGPRINT(KAL, DBG_INFO,("The Tx PKT CNT update to : %d, %d, %d, %d, %d, %d, %d,  \r\n",
	    hif_sdio_handler.tx_pkt_cnt[0],hif_sdio_handler.tx_pkt_cnt[1],hif_sdio_handler.tx_pkt_cnt[2],hif_sdio_handler.tx_pkt_cnt[3],  \
	    hif_sdio_handler.tx_pkt_cnt[4],hif_sdio_handler.tx_pkt_cnt[5],hif_sdio_handler.tx_pkt_cnt[6] /*,hif_sdio_handler.tx_pkt_cnt[7]*/)) ;		
	
    // 2. Ack the interrupt status (Because the interrupt is turn off during ISR executing,
	//                                             and some sw_int flow control will sent same int again after we ack the original one.
	//                                             So ack operation must be executed before sw int handle.
	if (pwhisr->u.bits.fw_ownback_int || pwhisr->u.bits.d2h_sw_int){
		if(sdio_func1_wr(SDIO_IP_WHISR, &(pwhisr->u.asUINT32),4)){
			KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Ack the WHISR interrupt sdio_func1_rd fail\r\n")) ;		
		}
	}

    // 3. handle the abnormal case
	if (pwhisr->u.bits.abnormal_int){
		if(sdio_func1_rd(SDIO_IP_WASR, hif_sdio_handler.sdio_wasr_cache, sizeof(sdio_wasr))){
			KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Get the abnormal interrupt sdio_func1_rd fail\r\n")) ;		
		}
	
		KAL_RAWPRINT(("[ERR] Get the abnormal interrupt status : 0x%08x\r\n", hif_sdio_handler.sdio_wasr_cache->u.asUINT32)) ;
        KAL_ASSERT(0);
	}

	// 4. handle the sw interrupt case	
	if(0 != pwhisr->u.bits.d2h_sw_int){
	    mtlte_df_swint_handle((unsigned int)pwhisr->u.asUINT32 );
	}

#endif
}

static inline void mtlte_hif_sdio_read_whisr_enhance(void)
{

#if TEST_DRV

    /* directly copy the enhance mode whisr to our SW cache */
    if(1 == test_rx_tail_change){
        
        // for rx tail length change test
        if(sdio_func1_rd(SDIO_IP_WHISR, hif_sdio_handler.enh_whisr_cache, test_rx_tail_len)){
		    KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR]mtlte_hif_sdio_read_whisr_enhance => sdio_func1_rd SDIO_IP_WHISR fail\r\n")) ;
		    return ;
        }
        
        transform_whisr_rx_tail(hif_sdio_handler.enh_whisr_cache);
        
    }else{
        // normal case
        if(sdio_func1_rd(SDIO_IP_WHISR, hif_sdio_handler.enh_whisr_cache,MT_LTE_RX_TAILOR_LENGTH)){
		    KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR]mtlte_hif_sdio_read_whisr_enhance => sdio_func1_rd SDIO_IP_WHISR fail\r\n")) ;
		    return ;
        }
    }
    

    mtlte_hif_test_drv_whisr_tail_handle(hif_sdio_handler.enh_whisr_cache);
#else

   if(sdio_func1_rd(SDIO_IP_WHISR, hif_sdio_handler.enh_whisr_cache,MT_LTE_RX_TAILOR_LENGTH)){
		KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR]mtlte_hif_sdio_read_whisr_enhance => sdio_func1_rd SDIO_IP_WHISR fail\r\n")) ;
		return ;
   }
   
#endif

	mtlte_hif_sdio_enhance_whisr_handle(hif_sdio_handler.enh_whisr_cache) ;
}

inline KAL_UINT32 mtlte_hif_sdio_tx_chk_pkt_num(KAL_UINT32 txqno){
	return mtlte_df_UL_pkt_in_swq(txqno) ;
}

inline KAL_UINT32 mtlte_hif_sdio_rx_chk_pkt_num(KAL_UINT32 rxqno){
	switch(rxqno){
	case RXQ_Q0:
		return hif_sdio_handler.enh_whisr_cache->rx0_num ;
	case RXQ_Q1:
		return hif_sdio_handler.enh_whisr_cache->rx1_num ;	
	case RXQ_Q2:
		return hif_sdio_handler.enh_whisr_cache->rx2_num ;
	case RXQ_Q3:
		return hif_sdio_handler.enh_whisr_cache->rx3_num ;
	default :
		return 0 ;
	}
}

// LTE Uplink, from host to device 
static SDIO_TXRXWORK_RESP mtlte_hif_sdio_tx(KAL_UINT32 txqno)
{
	KAL_UINT32 pkt_in_q, try_to_send_pkt_num ;
	SDIO_TXRXWORK_RESP ret_resp = 0 ;
	sdio_tx_sdu_header * tx_header_temp ;
	KAL_UINT32 tx_pkt_len ;

	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s, txqno: %d\n",KAL_FUNC_NAME, txqno)) ;

/********************************************************
*                      Process Tx Port 0
*********************************************************/	
	/* Clean the data buffer offset */
	hif_sdio_handler.data_buf_offset = 0 ; 

	pkt_in_q = mtlte_df_UL_pkt_in_swq(txqno);		

	KAL_DBGPRINT(KAL, DBG_INFO,("tx pkt in swq %d is %d\n",txqno, pkt_in_q)) ;
#if 1
	if ( (pkt_in_q == 0) || (hif_sdio_handler.tx_expt_stop[txqno] == 1) ){
			try_to_send_pkt_num = 0 ;
	}else if (pkt_in_q > hif_sdio_handler.tx_pkt_cnt[txqno]){
			KAL_DBGPRINT(KAL, DBG_INFO,("[TX] try_to_send_pkt_num is not complete\r\n")) ;
		try_to_send_pkt_num = hif_sdio_handler.tx_pkt_cnt[txqno] ;
			ret_resp |= WORK_RESP_TX_NO_RESOURCE ;
			ret_resp |= WORK_RESP_TX_NOT_COMPLETE ;
		}else{
			try_to_send_pkt_num = pkt_in_q ;
		}
#else
		if (pkt_in_q == 0){
			try_to_send_pkt_num = 0 ;
		}else{
			try_to_send_pkt_num = pkt_in_q ;
		}
#endif
		
	while(try_to_send_pkt_num > 0){

            if (hif_sdio_handler.data_buf_offset > MT_LTE_SDIO_DATA_BUFF_TX_LIMIT){
				KAL_DBGPRINT(KAL, DBG_INFO,("[TX] send data over buff tx limited\r\n")) ;
				/* Stop , goto send tx data */
				//break ;
				ret_resp |= WORK_RESP_TX_NOT_COMPLETE ;
			goto TX_PORT_SEND ;  /*strick priority, high priority queue will be sent until it's empty*/
			}
            
			// 1. fill the Tx data from DF pkt , so we pre off set Tx header length 
		    tx_pkt_len = mtlte_df_UL_deswq_buf(txqno, hif_sdio_handler.data_buf+MT_LTE_TX_HEADER_LENGTH+hif_sdio_handler.data_buf_offset) ;
#if !defined(FORMAL_RELEASE)
			KAL_ASSERT((tx_pkt_len>0) && (tx_pkt_len<=MT_LTE_TRX_MAX_PKT_SIZE)) ;
#endif			

			// 2. fill the Tx header  
			tx_header_temp = (sdio_tx_sdu_header *)(hif_sdio_handler.data_buf+hif_sdio_handler.data_buf_offset) ;
		    tx_header_temp->u.bits.tx_type = txqno ;
		    //tx_header_temp->u.bits.length = tx_pkt_len;
		    // Add the tx header length
		    tx_header_temp->u.bits.length = tx_pkt_len+MT_LTE_TX_HEADER_LENGTH;
		
			// 3. update the data_buf_offset
			hif_sdio_handler.data_buf_offset += (MT_LTE_TX_HEADER_LENGTH + KAL_ALIGN_TO_DWORD(tx_pkt_len)) ;

			// 4. update the packet count			
		    try_to_send_pkt_num-- ;
		    hif_sdio_handler.tx_pkt_cnt[txqno]-- ;			

            #if INTEGRATION_DEBUG
            if(eemcs_sdio_throughput_log == 1){
                sdio_txq_skb_log[txqno]++;
            }
            #endif
	}

				

	TX_PORT_SEND :
	
    
	if (hif_sdio_handler.data_buf_offset){  // we have Tx data fill in the buffer 
		KAL_DBGPRINT(KAL, DBG_TRACE,("[TX] TXQ %d send data, data_buf_offset is %d\r\n", txqno, hif_sdio_handler.data_buf_offset)) ;
        //KAL_DBGPRINT(KAL, DBG_ERROR,("[TEST] MT_LTE_SDIO_DATA_BUFF_LEN = %d, MT_LTE_SDIO_DATA_BUFF_TX_LIMIT = %d\r\n", MT_LTE_SDIO_DATA_BUFF_LEN, MT_LTE_SDIO_DATA_BUFF_TX_LIMIT)) ;
        //KAL_DBGPRINT(KAL, DBG_ERROR,("[TEST] MT_LTE_SDIO_DATA_BUFF_LEN_UNALIGN = %d \r\n", MT_LTE_SDIO_DATA_BUFF_LEN_UNALIGN)) ;

        // 5. fill padding 0 tail if necessary	
		if (hif_sdio_handler.data_buf_offset != MT_LTE_SDIO_DATA_BUFF_LEN){
			KAL_ZERO_MEM((hif_sdio_handler.data_buf+hif_sdio_handler.data_buf_offset),MT_LTE_TX_ZERO_PADDING_LEN) ;
		}
#if 0
		//KAL_SLEEP_MSEC(10) ;
		ut_dump_tx_sdu( 1 , hif_sdio_handler.data_buf) ;
#else
        
        KAL_ASSERT(ALIGN_TO_BLOCK_SIZE(hif_sdio_handler.data_buf_offset)<=MT_LTE_SDIO_DATA_BUFF_LEN) ; 
		if(sdio_func1_wr(tx_queue_info[txqno].port_address, hif_sdio_handler.data_buf, ALIGN_TO_BLOCK_SIZE(hif_sdio_handler.data_buf_offset))){
			ret_resp |= WORK_RESP_TX_DATA_ERROR ;
			KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] mtlte_hif_sdio_tx => sdio_func1_wr 0x%08x fail\r\n",tx_queue_info[txqno].port_address)) ;
		}
        mtlte_df_UL_callback(txqno); // for wake up tx blocking I/O
        /* debug*/
        //KAL_UINT32 print_index ;
        //if(txqno == 1){
            //KAL_DBGPRINT(KAL, DBG_WARN,("first block : \r\n ")) ;
            //for(print_index=0; print_index<256; print_index++)
            //    KAL_DBGPRINT(KAL, DBG_WARN,("%x ", (*(KAL_UINT8 *)(hif_sdio_handler.data_buf+print_index)))) ;
            //KAL_DBGPRINT(KAL, DBG_WARN,("last block : \r\n ")) ;
            //for(print_index=ALIGN_TO_BLOCK_SIZE(hif_sdio_handler.data_buf_offset)-256; print_index<ALIGN_TO_BLOCK_SIZE(hif_sdio_handler.data_buf_offset); print_index++)
            //    KAL_DBGPRINT(KAL, DBG_WARN,("%x ", (*(KAL_UINT8 *)(hif_sdio_handler.data_buf+print_index)))) ;
        //}
        
        //KAL_DBGPRINT(KAL, DBG_WARN,("\r\n")) ;
#endif		
		ret_resp |= WORK_RESP_TX_SENT_PKT ;
	}

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	
	return ret_resp ;

}

// LTE Downlink, from device to host
static SDIO_TXRXWORK_RESP mtlte_hif_sdio_rx(KAL_UINT32 rxqno)
{
	SDIO_TXRXWORK_RESP ret_resp = 0 ;
	KAL_UINT32 j ;
	KAL_UINT32 total_len ;
	sdio_whisr_enhance *tailor_cache = NULL;
	KAL_UINT16	*fw_rx_pkt_num ;
	KAL_UINT16	*fw_rx_pkt_len ;
	KAL_UINT32 wait_start = 0, cur_time = 0;

#if THREAD_FOR_MEMCPY_SKB
	KAL_UINT32 rxbuf_num_thistime;
	MTLTE_HIF_RX_BUF_USAGE buf_usage_now;
#endif    

	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;
	// 1.  prepare the corresponding rx queue number and packet length pointer 
		total_len = 0 ;
        
	switch(rxqno){
			case RXQ_Q0:				
			{
			fw_rx_pkt_num = &(hif_sdio_handler.enh_whisr_cache->rx0_num) ;
			fw_rx_pkt_len = hif_sdio_handler.enh_whisr_cache->rx0_pkt_len ;
				break ;
			}
			case RXQ_Q1:
			{
			fw_rx_pkt_num = &(hif_sdio_handler.enh_whisr_cache->rx1_num) ;
			fw_rx_pkt_len = hif_sdio_handler.enh_whisr_cache->rx1_pkt_len ;
				break ;
			}
			case RXQ_Q2:
			{
			fw_rx_pkt_num = &(hif_sdio_handler.enh_whisr_cache->rx2_num) ;
			fw_rx_pkt_len = hif_sdio_handler.enh_whisr_cache->rx2_pkt_len ;		
			break;
				}
		    case RXQ_Q3:
				{
			fw_rx_pkt_num = &(hif_sdio_handler.enh_whisr_cache->rx3_num) ;
			fw_rx_pkt_len = hif_sdio_handler.enh_whisr_cache->rx3_pkt_len ;		
			break;
				}	
		default :
		{
			KAL_ASSERT(0) ;
			break;
				}
				}

			{

        // 1.5 Check whether this que is no needs to transfer in exception mode.        
        if ( hif_sdio_handler.rx_expt_stop[rxqno] == 1 ){
			KAL_DBGPRINT(KAL, DBG_LOUD,("[RX] RXQ%d is stop because no need in exception mode \r\n", rxqno)) ;								
			goto RX_UPDATE_WHISR ;
		}  

#if FORMAL_DL_FLOW_CONTROL
        // 1.8 Check whether this que is no needs to transfer due to DL flow control.        
        if ( hif_sdio_handler.rx_fl_ctrl_stop[rxqno] == 1 ){
			KAL_DBGPRINT(KAL, DBG_LOUD,("[RX] RXQ%d is stop due to manual flow control \r\n", rxqno)) ;								
			goto RX_UPDATE_WHISR ;
		} 
#endif

        // 1.9 Check whether this que is no needs to transfer due to manual stop.        
        if ( hif_sdio_handler.rx_manual_stop[rxqno] == 1 ){
			KAL_DBGPRINT(KAL, DBG_LOUD,("[RX] RXQ%d is stop by manual \r\n", rxqno)) ;								
			goto RX_UPDATE_WHISR ;
		}  
     

		// 2. Check if there is any rx packet from FW
		if (*fw_rx_pkt_num == 0){
			KAL_DBGPRINT(KAL, DBG_LOUD,("[RX] RXQ%d RX PKT NUM is 0\r\n", rxqno)) ;								
			goto RX_UPDATE_WHISR ;
		}
        
#if THREAD_FOR_MEMCPY_SKB
 #if !ALLOCATE_SKB_IN_QWORK
  #if BUFFER_POOL_FOR_EACH_QUE
        if (mtlte_df_DL_pkt_in_buff_pool(rxqno) < rx_queue_info[rxqno].max_rx_pktcnt){
            mtlte_df_DL_try_reload_swq() ; // try to reload it 
        }
  #else
        if (mtlte_df_DL_pkt_in_buff_pool() < rx_queue_info[rxqno].max_rx_pktcnt){
            mtlte_df_DL_try_reload_swq() ; // try to reload it 
        }
  #endif      
 #endif
#else
        // 3. Check the DL pkt buffer pool is enough or not to receive the rx packet
  #if BUFFER_POOL_FOR_EACH_QUE
        if (mtlte_df_DL_pkt_in_buff_pool(rxqno) < rx_queue_info[rxqno].max_rx_pktcnt){
            #if IMMEDIATE_RELOAD_DL_SKB
                while (mtlte_df_DL_pkt_in_buff_pool(rxqno) < rx_queue_info[rxqno].max_rx_pktcnt){
                    mtlte_df_DL_prepare_skb_for_swq_short(rx_queue_info[rxqno].max_rx_pktcnt, rxqno);
                    if (mtlte_df_DL_pkt_in_buff_pool(rxqno) < rx_queue_info[rxqno].max_rx_pktcnt){
                        KAL_SLEEP_USEC(1000);
                    }
                }
            #else
                KAL_DBGPRINT(KAL, DBG_INFO,("[RX] RXQ%d mtlte_df_DL_pkt_in_buff_pool is unenough\r\n", rxqno)) ;
                ret_resp |= WORK_RESP_RX_NOT_COMPLETE ; 
                mtlte_df_DL_try_reload_swq() ; // try to reload it 
                goto RX_UPDATE_WHISR ;
            #endif            
        } 
  #else
        if (mtlte_df_DL_pkt_in_buff_pool() < rx_queue_info[rxqno].max_rx_pktcnt){
            #if IMMEDIATE_RELOAD_DL_SKB
                while (mtlte_df_DL_pkt_in_buff_pool() < rx_queue_info[rxqno].max_rx_pktcnt){
                    mtlte_df_DL_prepare_skb_for_swq_short(rx_queue_info[rxqno].max_rx_pktcnt);
                    if (mtlte_df_DL_pkt_in_buff_pool() < rx_queue_info[rxqno].max_rx_pktcnt){
                        KAL_SLEEP_USEC(1000);
                    }
                }
            #else
                KAL_DBGPRINT(KAL, DBG_INFO,("[RX] RXQ%d mtlte_df_DL_pkt_in_buff_pool is unenough\r\n", rxqno)) ;
                ret_resp |= WORK_RESP_RX_NOT_COMPLETE ; 
                mtlte_df_DL_try_reload_swq() ; // try to reload it 
                goto RX_UPDATE_WHISR ;
            #endif            
        } 
  #endif
#endif

		// 4. Check the SWQ is over thereshold or not
		if ( mtlte_df_DL_pkt_in_swq(rxqno) > (rx_queue_info[rxqno].max_rx_swq_inuse)){
			KAL_DBGPRINT(KAL, DBG_INFO,("[RX] RXQ%d mtlte_df_DL_pkt_in_swq is over threshold\r\n", rxqno)) ;
					ret_resp |= WORK_RESP_RX_NOT_COMPLETE ; 
			goto RX_UPDATE_WHISR ;
				}

		// 5. calculate the total size 
		total_len = 0 ;
		for (j=0 ; j< *(fw_rx_pkt_num) ; j++){
			total_len += (KAL_ALIGN_TO_DWORD(fw_rx_pkt_len[j])+MT_LTE_RX_LEGACY_SDU_TAIL) ;
		}
        
#if TEST_DRV
        if(1 == test_rx_tail_change){
            total_len += (MT_LTE_RX_TAILOR_PRESPACE + test_rx_tail_len) ;
        }else{
            total_len += (MT_LTE_RX_TAILOR_PRESPACE + MT_LTE_RX_TAILOR_LENGTH) ;
        }
#else
        total_len += (MT_LTE_RX_TAILOR_PRESPACE + MT_LTE_RX_TAILOR_LENGTH) ;
#endif


#if !defined(FORMAL_RELEASE)
                if(ALIGN_TO_BLOCK_SIZE(total_len) > MT_LTE_SDIO_DATA_BUFF_LEN){
                    KAL_DBGPRINT(KAL, DBG_ERROR,("[RX] total_len = %d , MT_LTE_SDIO_DATA_BUFF_LEN = %d \r\n", total_len, MT_LTE_SDIO_DATA_BUFF_LEN)) ;
                }
				KAL_ASSERT(ALIGN_TO_BLOCK_SIZE(total_len)<=MT_LTE_SDIO_DATA_BUFF_LEN) ;
#endif

#if THREAD_FOR_MEMCPY_SKB

        // 5.5  Find available RX_BUF 
        rxbuf_num_thistime = hif_sdio_handler.rx_buf_order_recv;
        KAL_MUTEXLOCK(&hif_sdio_handler.rx_buf_lock[rxbuf_num_thistime]);
        buf_usage_now = hif_sdio_handler.rx_buf_usage[rxbuf_num_thistime];
        KAL_MUTEXUNLOCK(&hif_sdio_handler.rx_buf_lock[rxbuf_num_thistime]);

 		wait_start = jiffies;
#if USE_EVENT_TO_WAKE_BUFFER
        if(buf_usage_now != NOT_BE_USED){
            wait_event_interruptible( lte_dev_p->sdio_thread_wq, (kthread_should_stop() || (hif_sdio_handler.wake_evt_buff[rxbuf_num_thistime]==1)));
            if (kthread_should_stop()) {
                goto RX_THREAD_STOP;
            }else{
                KAL_MUTEXLOCK(&hif_sdio_handler.rx_buf_lock[rxbuf_num_thistime]);
                buf_usage_now = hif_sdio_handler.rx_buf_usage[rxbuf_num_thistime];
                KAL_MUTEXUNLOCK(&hif_sdio_handler.rx_buf_lock[rxbuf_num_thistime]);
            }
        }
#endif

        while(buf_usage_now != NOT_BE_USED){
            //KAL_SLEEP_USEC(30);
            KAL_SLEEP_MSEC(5);

            KAL_MUTEXLOCK(&hif_sdio_handler.rx_buf_lock[rxbuf_num_thistime]);
            buf_usage_now = hif_sdio_handler.rx_buf_usage[rxbuf_num_thistime];
            KAL_MUTEXUNLOCK(&hif_sdio_handler.rx_buf_lock[rxbuf_num_thistime]);
        }


        if(buf_usage_now == NOT_BE_USED){
            hif_sdio_handler.rx_buf_order_recv++;
            
            if(hif_sdio_handler.rx_buf_order_recv >= RX_BUF_NUM){
                hif_sdio_handler.rx_buf_order_recv = 0;
            }
        } 
#if USE_EVENT_TO_WAKE_BUFFER 
        else{
            KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] kthread is waked up by wake_evt_buff[%d], but buf_usage_now != NOT_BE_USED \r\n", rxbuf_num_thistime)) ;
            KAL_ASSERT(0);
        }
#endif
        cur_time = jiffies;
        if (cur_time > wait_start) {
			rxq_wait_big_buf_time = cur_time - wait_start;
			if (rxq_wait_big_buf_time > 4) { // > 40ms
				KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO perf][WARN] DLQ%d rxq_wait_big_buf_time=%dms, rxbuf_num_thistime=%d\n", rxqno, 10*rxq_wait_big_buf_time, rxbuf_num_thistime)) ;			
			}
        }

        #if INTEGRATION_DEBUG
        if(eemcs_sdio_throughput_log == 1){
            sdio_rxq_skb_log[rxqno]+= *(fw_rx_pkt_num);
        }
        #endif

        // 6. start read data from FW 
		if(sdio_func1_rd(rx_queue_info[rxqno].port_address, hif_sdio_handler.rx_data_buf[rxbuf_num_thistime], ALIGN_TO_BLOCK_SIZE(total_len))){
			ret_resp |= WORK_RESP_RX_DATA_ERROR ;
			KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] mtlte_hif_sdio_rx => sdio_func1_rd SDIO_IP_WRDR0 fail\r\n")) ;
		}
        KAL_COPY_MEM(hif_sdio_handler.whisr_for_rxbuf[rxbuf_num_thistime], hif_sdio_handler.enh_whisr_cache, sizeof(sdio_whisr_enhance)) ;

        // 7. get the tailor 
        total_len = 0;
        for (j=0 ; j< *(fw_rx_pkt_num) ; j++){
            total_len +=KAL_ALIGN_TO_DWORD(fw_rx_pkt_len[j])+MT_LTE_RX_LEGACY_SDU_TAIL ; 
        }
		tailor_cache = (sdio_whisr_enhance *)(hif_sdio_handler.rx_data_buf[rxbuf_num_thistime] + total_len + MT_LTE_RX_TAILOR_PRESPACE) ;
        // for rx tail length change test
        if(1 == test_rx_tail_change){
            transform_whisr_rx_tail(tailor_cache);
        }

	// 8. set the WORK_RESP_RX_RECV_PKT flag, because we get some data from FW
	ret_resp |= WORK_RESP_RX_RECV_PKT ;	

        // 9. kick the pop to the upper layer thread
#if USE_EVENT_TO_WAKE_BUFFER         
        KAL_MUTEXLOCK(&hif_sdio_handler.wake_evt_lock[rxbuf_num_thistime]);
        hif_sdio_handler.wake_evt_buff[rxbuf_num_thistime]= 0;
        KAL_MUTEXUNLOCK(&hif_sdio_handler.wake_evt_lock[rxbuf_num_thistime]);
#endif            
        KAL_MUTEXLOCK(&hif_sdio_handler.rx_buf_lock[rxbuf_num_thistime]);
        hif_sdio_handler.rx_buf_usage[rxbuf_num_thistime] = (MTLTE_HIF_RX_BUF_USAGE)rxqno;
        KAL_MUTEXUNLOCK(&hif_sdio_handler.rx_buf_lock[rxbuf_num_thistime]);
        
#if defined (TCP_OUT_OF_ORDER_SOLUTION) 
		KAL_INT32 cpu;
		if (num_online_cpus() == 1) {	
			lte_online_cpu = 0;
		} else if (!cpu_online(lte_online_cpu) || (lte_online_cpu == 0)) {
			if (unlikely(lte_online_cpu == 0)) {			
				KAL_DBGPRINT(KAL, DBG_ERROR, ("[Warning]queue_work_on_cpu0 when online_cpus=%d\n", num_possible_cpus())) ;
			}
        	for (cpu=(lte_online_cpu+1); cpu<num_possible_cpus(); cpu++) {
				if (cpu_online(cpu)) {
					lte_online_cpu = cpu;
					break;
				}
        	}
			if (cpu == num_possible_cpus()) {
				for (cpu=lte_online_cpu; cpu>=0; cpu--) {
					if (cpu_online(cpu)) {
						lte_online_cpu = cpu;
						break;
					}
				}
			}
		}
		
		queue_work_on(lte_online_cpu, hif_sdio_handler.rx_memcpy_work_queue, &hif_sdio_handler.rx_memcpy_work); 		
		KAL_DBGPRINT(KAL, DBG_INFO, ("mtlte_hif_sdio_rx: queue_work_on online_cpu=%d, cpus=%d\n", lte_online_cpu, num_possible_cpus())) ;

#else
		queue_work(hif_sdio_handler.rx_memcpy_work_queue, &hif_sdio_handler.rx_memcpy_work);

#endif
		
#else
					
		// 6. start read data from FW 
		if(sdio_func1_rd(rx_queue_info[rxqno].port_address, hif_sdio_handler.data_buf, ALIGN_TO_BLOCK_SIZE(total_len))){
					ret_resp |= WORK_RESP_RX_DATA_ERROR ;
			KAL_DBGPRINT(KAL, DBG_ERROR, ("[ERR] mtlte_hif_sdio_rx => sdio_func1_rd SDIO_IP_WRDR0 fail\r\n")) ;
		}

		// 7. pop to the upper layer				
		total_len = 0 ; // reuse the total_len to scan the buffer 
		
		for (j=0 ; j< *(fw_rx_pkt_num) ; j++){
#if !defined(FORMAL_RELEASE)
			KAL_ASSERT(mtlte_df_DL_enswq_buf(rxqno, (hif_sdio_handler.data_buf + total_len), fw_rx_pkt_len[j]) == KAL_SUCCESS) ;
#else					
			mtlte_df_DL_enswq_buf(rxqno, (hif_sdio_handler.data_buf + total_len), fw_rx_pkt_len[j]) ;
#endif
			total_len +=KAL_ALIGN_TO_DWORD(fw_rx_pkt_len[j])+MT_LTE_RX_LEGACY_SDU_TAIL ; 									
		}
        
#if USE_QUE_WORK_DISPATCH_RX
    #if USE_MULTI_QUE_DISPATCH
        mtlte_df_DL_try_dispatch_rxque(rxqno);
    #else
        mtlte_df_DL_try_dispatch_rx();
    #endif
#endif
		KAL_DBGPRINT(KAL, DBG_TRACE,("[RX] RXQ%d receive total align length: %d , total packets: %d\r\n", rxqno, total_len, j)) ;


		// 8. get the tailor 
			tailor_cache = (sdio_whisr_enhance *)(hif_sdio_handler.data_buf + total_len + MT_LTE_RX_TAILOR_PRESPACE) ;
#if TEST_DRV
            // for rx tail length change test
            if(1 == test_rx_tail_change){
                transform_whisr_rx_tail(tailor_cache);
            }
#endif
		// 9. set the WORK_RESP_RX_RECV_PKT flag, because we get some data from FW
		ret_resp |= WORK_RESP_RX_RECV_PKT ;		
#endif
	}

RX_UPDATE_WHISR:					
	// 10. Update the tailor 
	if (tailor_cache != NULL){
		KAL_DBGPRINT(KAL, DBG_TRACE,("[RX] update the whisr cache by the RX tailor \n")) ;	
		/* copy the enhance mode whisr from tailor to our SW cache */
#if 1		
		KAL_COPY_MEM(hif_sdio_handler.enh_whisr_cache, tailor_cache, sizeof(sdio_whisr_enhance)) ;
		mtlte_hif_sdio_enhance_whisr_handle(hif_sdio_handler.enh_whisr_cache) ;
#endif		
	}

#if USE_EVENT_TO_WAKE_BUFFER
RX_THREAD_STOP:
#endif
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return ret_resp ;
}


KAL_INT32 mtlte_hif_expt_mode_init(void)
{
    KAL_UINT32 i;
    KAL_UINT32 h2d_ack_int;
    KAL_UINT32 cnt = 500 ;
	KAL_UINT32 init_done_int ;
    
    mtlte_hif_sdio_disable_interrupt();
    mtlte_hif_sdio_mask_interrupt();

    mtlte_hif_sdio_get_driver_own();
    mtlte_hif_sdio_enable_fw_own_back(0);
    
    mtlte_hif_sdio_txrx_proc_enable(0);

    // [SDIO]Clean related variable , drop UL skb
    for(i=0; i<TXQ_NUM; i++)
    {
         mtlte_df_UL_swq_drop_skb(i);
    }

    KAL_ZERO_MEM(hif_sdio_handler.enh_whisr_cache, sizeof(sdio_whisr_enhance)) ;
    KAL_ZERO_MEM(hif_sdio_handler.sdio_wasr_cache, sizeof(sdio_wasr)) ;

    for(i=0; i<TXQ_NUM; i++)
    {
         hif_sdio_handler.tx_pkt_cnt[i] = 0;
         hif_sdio_handler.tx_expt_stop[i] = 1;
    }

    for(i=0; i<RXQ_NUM; i++)
    {
         hif_sdio_handler.rx_expt_stop[i] = 1;
    }
   
    // ack interrupt
    h2d_ack_int = H2D_INT_except_ack;
    sdio_func1_wr(SDIO_IP_WSICR, &h2d_ack_int, 4);
    
    // Polling device Exception Init Done
	while (cnt--){
		if (sdio_func1_rd(SDIO_IP_WHISR, &init_done_int, 4) != KAL_SUCCESS){
			return KAL_FAIL ; 
		}	
		
		if (init_done_int & D2H_INT_except_init_done){
            
            init_done_int = D2H_INT_except_init_done;
            sdio_func1_wr(SDIO_IP_WHISR, &init_done_int, 4) ;

            mtlte_hif_sdio_txrx_proc_enable(1);
            
			return KAL_SUCCESS ;
		}
		KAL_SLEEP_MSEC(10) ;
	}
	
	return KAL_FAIL ;

}


KAL_INT32 mtlte_hif_expt_restart_que(KAL_UINT32 is_DL, KAL_UINT32 q_num)
{
	KAL_UINT32 value ;

	KAL_DBGPRINT(KAL, DBG_LOUD,("[INT] mtlte_hif_sdio_expt_restart_que <==========>\r\n")) ;

    if(is_DL)
    {
        if (sdio_func1_rd(SDIO_IP_WHIER, &value, 4) != KAL_SUCCESS){ return KAL_FAIL ; }
        value |= (0x1 << (q_num+1));
        if (sdio_func1_wr(SDIO_IP_WHIER, &value, 4) != KAL_SUCCESS){ return KAL_FAIL ; }

        hif_sdio_handler.rx_expt_stop[q_num] = 0;

    }
    else
    {
        hif_sdio_handler.tx_pkt_cnt[q_num] = 0;
        hif_sdio_handler.tx_expt_stop[q_num] = 0;
    }

	return KAL_SUCCESS ;

}


KAL_INT32 mtlte_hif_expt_unmask_swint(void)
{
	KAL_UINT32 value ;

	KAL_DBGPRINT(KAL, DBG_LOUD,("[INT] mtlte_hif_expt_unmask_swint <==========>\r\n")) ;

    if (sdio_func1_rd(SDIO_IP_WHIER, &value, 4) != KAL_SUCCESS){ return KAL_FAIL ; }
    value |= 0xFFFFFF00;
    if (sdio_func1_wr(SDIO_IP_WHIER, &value, 4) != KAL_SUCCESS){ return KAL_FAIL ; }

	return KAL_SUCCESS ;

}



KAL_INT32 mtlte_hif_expt_reset_allQ(void)
{
	KAL_UINT32 h2d_ack_int;
    KAL_UINT32 reset_done_int ;
    KAL_UINT32 cnt = 500 ;
    KAL_UINT32 i ;
    KAL_UINT32 value;

	// ack interrupt
    h2d_ack_int = H2D_INT_except_clearQ_ack;
    sdio_func1_wr(SDIO_IP_WSICR, &h2d_ack_int, 4);

        // Polling device Exception Init Done
	while (cnt--){
		if (sdio_func1_rd(SDIO_IP_WHISR, &reset_done_int, 4) != KAL_SUCCESS){
			return KAL_FAIL ; 
		}	
		
		if (reset_done_int & D2H_INT_except_allQ_reset){
            sdio_func1_wr(SDIO_IP_WHISR, &reset_done_int, 4) ;

            mtlte_expt_reset_inform_hif();

            for(i=0; i<TXQ_NUM; i++)
            {
                hif_sdio_handler.tx_pkt_cnt[i] = 0;
                if(mtlte_expt_check_expt_q_num(0, i))
                {
                    hif_sdio_handler.tx_expt_stop[i] = 0;
                }
            }

            for(i=0; i<RXQ_NUM; i++)
            {
                if(mtlte_expt_check_expt_q_num(1, i))
                {
                    if (sdio_func1_rd(SDIO_IP_WHIER, &value, 4) != KAL_SUCCESS){ return KAL_FAIL ; }
                    value |= (0x1 << (i+1));
                    if (sdio_func1_wr(SDIO_IP_WHIER, &value, 4) != KAL_SUCCESS){ return KAL_FAIL ; }
                    
                    hif_sdio_handler.rx_expt_stop[i] = 0;
                }
            }

            hif_sdio_handler.expt_reset_allQ = 0;

            KAL_DBGPRINT(KAL, DBG_ERROR,("[exception] Reset que completed, Exception que can work now... \r\n")) ;
            
			return KAL_SUCCESS ;
		}
		KAL_SLEEP_MSEC(10) ;
	}

	return KAL_FAIL;

}



KAL_INT32 mtlte_hif_expt_enable_interrupt(void)
{
    return mtlte_hif_sdio_enable_interrupt();
}


KAL_INT32 mtlte_hif_expt_set_reset_allQ_bit(void)
{
    hif_sdio_handler.expt_reset_allQ = 1;
    return KAL_SUCCESS ;
}

#if FORMAL_DL_FLOW_CONTROL_TEST
void mtlte_hif_print_fl_ctrl(void)
{
    KAL_INT32 fl_qno;
    for(fl_qno=0; fl_qno<RXQ_NUM; fl_qno++){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[TEST] rx_fl_ctrl_stop at DLQ%d = %d \n", fl_qno, hif_sdio_handler.rx_fl_ctrl_stop[fl_qno])) ;
    }
}
#endif


#define MINI_ROUND_TO_CHECK_WHISR	10
KAL_INT32 mtlte_hif_sdio_process()
{
	SDIO_TXRXWORK_RESP tx_work_resp ;
	SDIO_TXRXWORK_RESP rx_work_resp ;
    SDIO_TXRXWORK_RESP previous_tx_work_resp ;
	SDIO_TXRXWORK_RESP previous_rx_work_resp ;
	KAL_UINT32 ret_resp ;
	KAL_UINT32 do_again ;
	KAL_UINT32 not_to_get_tail_whisr = 0 ;
	/* realtime queue index */
	KAL_UINT32 rr_index ;	
	KAL_UINT32 rr_recheck = 0 ;
	/* normal queue index */
	KAL_UINT32 nm_index = 0 ;
    KAL_UINT32 time_of_no_resource = 0 ;

    //KAL_UINT32 diff_jif = 0;
    KAL_UINT32 proc_time = 0, proc_start = 0;
    
	KAL_UINT32 log_retry_cnt = 0;
	


#if INTEGRATION_DEBUG
    KAL_UINT32 log_qno = 0 ;
    KAL_UINT32 log_total_ul = 0 ;
    KAL_UINT32 log_total_dl = 0 ;
#endif

#if FORMAL_DL_FLOW_CONTROL
    KAL_UINT32 fl_qno = 0 ;
    KAL_UINT32 fl_int_mask = 0 ;
#endif

	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;
        proc_start = jiffies;

#if !USING_WAKE_MD_EINT
	// 1. check thw owner ship 	
		if (mtlte_hif_sdio_get_driver_own() == KAL_FAIL){
			KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] Get Drv Own Fail!!!\n")) ;
			goto END_OF_TXRX_PROCESS ; 
		}
#endif	

	mtlte_hif_sdio_read_whisr_enhance() ;

	// 3. try to reload the DL SW buffer 
	// mtlte_df_DL_try_reload_swq() ; 

    previous_tx_work_resp = 0 ;
	previous_rx_work_resp = 0 ;

	do_again = 1 ; 
	// 3. start Tx/Rx processing  
	while(do_again && (hif_sdio_handler.txrx_enable)) {

#if FORMAL_DL_FLOW_CONTROL
    for(fl_qno=0; fl_qno<RXQ_NUM; fl_qno++){
        
        if(true == mtlte_df_DL_check_fl_ctrl_enable(fl_qno)){
            // flow control is enable, check que status of flow control
            
            if( true == mtlte_df_DL_check_fl_ctrl_full(fl_qno) ){
                if(0 == hif_sdio_handler.rx_fl_ctrl_stop[fl_qno]){
                    // DL skb is up to limit, but rx que not stop yet. So stop the rx que.
                    
                    if (sdio_func1_rd(SDIO_IP_WHIER, &fl_int_mask, 4) != KAL_SUCCESS){ return KAL_FAIL ; }
                    fl_int_mask &= ~(0x1 << (fl_qno+1));
                    if (sdio_func1_wr(SDIO_IP_WHIER, &fl_int_mask, 4) != KAL_SUCCESS){ return KAL_FAIL ; }
        
                    hif_sdio_handler.rx_fl_ctrl_stop[fl_qno] = 1;

                    hif_sdio_handler.rx_fl_ctrl_stop_count[fl_qno]++;
                    if(100 > hif_sdio_handler.rx_fl_ctrl_stop_count[fl_qno]){
                        KAL_DBGPRINT(KAL, DBG_ERROR,("[EEMCS][SDIO][WARN] FLOW control start at DLQ%d !!!\n", fl_qno)) ;
                    }
                }
            }
            else{
                if(1 == hif_sdio_handler.rx_fl_ctrl_stop[fl_qno]){
                    // DL skb is down to threshold, but rx que not enable yet. So enable the rx que.
                    
                    if (sdio_func1_rd(SDIO_IP_WHIER, &fl_int_mask, 4) != KAL_SUCCESS){ return KAL_FAIL ; }
                    fl_int_mask |= (0x1 << (fl_qno+1));
                    if (sdio_func1_wr(SDIO_IP_WHIER, &fl_int_mask, 4) != KAL_SUCCESS){ return KAL_FAIL ; }
                    
                    hif_sdio_handler.rx_fl_ctrl_stop[fl_qno] = 0;
                    if(100 > hif_sdio_handler.rx_fl_ctrl_stop_count[fl_qno]){
                        KAL_DBGPRINT(KAL, DBG_ERROR,("[EEMCS][SDIO][WARN] FLOW control end at DLQ%d !!!\n", fl_qno)) ;
                    }
                }
            }
        }
    }
#endif

		do_again = 0 ;
		rx_work_resp = 0 ;
		tx_work_resp = 0 ;		
		nm_index = 0 ;

		while (nm_index<proc_normal_queue_num){	
			rr_recheck = 0 ;
        
			/* we Always check that if the Real Time Queues have packets to handle before handle normal queue. If YES, handle each queue by round robin*/
			for (rr_index = 0 ; rr_index < proc_realtime_queue_num ; rr_index++){
				if (proc_realtime_queue_handle[rr_index].pkt_num_check(proc_realtime_queue_handle[rr_index].qno)>0){
					rr_recheck=1 ;
					if (proc_realtime_queue_handle[rr_index].is_tx){
						ret_resp = mtlte_hif_sdio_tx(proc_realtime_queue_handle[rr_index].qno) ;
						not_to_get_tail_whisr = (ret_resp==WORK_RESP_TX_SENT_PKT)? not_to_get_tail_whisr+1 : not_to_get_tail_whisr ;
						tx_work_resp |= ret_resp ;
					}else{
						ret_resp = mtlte_hif_sdio_rx(proc_realtime_queue_handle[rr_index].qno) ;
						not_to_get_tail_whisr = (ret_resp==WORK_RESP_RX_RECV_PKT)? 0 : not_to_get_tail_whisr ;
						rx_work_resp |= ret_resp ;						
					}
				}
			}
			/* Recheck the Real Time Queues before start to handle the normal queue packets*/
            // TODO: wedo : this will make deadlock when que 0 has no resource
			//if (rr_recheck){
			//	KAL_DBGPRINT(KAL, DBG_LOUD,("[DOAGAIN] RealTime Packet resent again!!\n")) ;
			//	continue ;
			//}

			/* Check that if the normal Queues have packets to handle. If YES, handle each queue by round robin*/
			if (proc_normal_queue_handle[nm_index].pkt_num_check(proc_normal_queue_handle[nm_index].qno)>0){
				if (proc_normal_queue_handle[nm_index].is_tx){
					ret_resp = mtlte_hif_sdio_tx(proc_normal_queue_handle[nm_index].qno) ;
					not_to_get_tail_whisr = (ret_resp==WORK_RESP_TX_SENT_PKT)? not_to_get_tail_whisr+1 : not_to_get_tail_whisr ;
					tx_work_resp |= ret_resp ;
				}else{
					ret_resp = mtlte_hif_sdio_rx(proc_normal_queue_handle[nm_index].qno) ;
					not_to_get_tail_whisr = (ret_resp==WORK_RESP_RX_RECV_PKT)? 0 : not_to_get_tail_whisr ;
					rx_work_resp |= ret_resp ;
				}
			}
	
			/* update the normal queue round robin index */
			nm_index++ ;
		
		}

		// 3. If we don't do any action in RX path for many times, and the TX is out of resource, we should update the whisr again 
		if (tx_work_resp & WORK_RESP_TX_NO_RESOURCE || not_to_get_tail_whisr>MINI_ROUND_TO_CHECK_WHISR){	
				/* no Rx and Tx need to get pkt cnt */
				mtlte_hif_sdio_read_whisr_enhance() ;
			not_to_get_tail_whisr = 0 ; 
		}

		if ((rx_work_resp & WORK_RESP_RX_DATA_ERROR) || (tx_work_resp & WORK_RESP_TX_DATA_ERROR)){
			KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] mtlte_hif_sdio_rx WORK_RESP_RX_DATA_ERROR\n")) ;
			break ;			
		}
		
		/* there are still rx packets have to be sent */
#if FORMAL_DL_FLOW_CONTROL
        else if ( (hif_sdio_handler.enh_whisr_cache->rx0_num>0 && (!hif_sdio_handler.rx_fl_ctrl_stop[0])) || 
			      (hif_sdio_handler.enh_whisr_cache->rx1_num>0 && (!hif_sdio_handler.rx_fl_ctrl_stop[1])) || 
			      (hif_sdio_handler.enh_whisr_cache->rx2_num>0 && (!hif_sdio_handler.rx_fl_ctrl_stop[2])) || 
			      (hif_sdio_handler.enh_whisr_cache->rx3_num>0 && (!hif_sdio_handler.rx_fl_ctrl_stop[3])) )
#else
        else if (hif_sdio_handler.enh_whisr_cache->rx0_num>0 || 
			     hif_sdio_handler.enh_whisr_cache->rx1_num>0 || 
			     hif_sdio_handler.enh_whisr_cache->rx2_num>0 || 
			     hif_sdio_handler.enh_whisr_cache->rx3_num>0 )
#endif
		{
			KAL_DBGPRINT(KAL, DBG_INFO,("[DOAGAIN] mtlte_hif_sdio_process tailor pkt exists!!\n")) ;			
			/* Still Rx packets in FW */	
			do_again = 1 ; 
			
		}else if ((rx_work_resp & WORK_RESP_RX_NOT_COMPLETE) || (tx_work_resp & WORK_RESP_TX_NOT_COMPLETE)){
			KAL_DBGPRINT(KAL, DBG_INFO,("[RETRY] mtlte_hif_sdio_process NOT_COMPLETE, tx:%x, rx:%x, do_again = 1\n", tx_work_resp,rx_work_resp)) ;				
	
			/* Tx or Rx has incomplete packet to handle */	
			do_again = 1 ; 

            /* Sleep for a while for resource ready */	
            if((rx_work_resp == previous_rx_work_resp) || (tx_work_resp == previous_tx_work_resp)){
                
                if(time_of_no_resource > 2){
                    previous_rx_work_resp = 0;
                    previous_tx_work_resp = 0;
                    time_of_no_resource = 0;

                    if (log_retry_cnt%20000 == 0)
                    {
			KAL_RAWPRINT(("[Busy] Sleep for resource ready, cnt=%d\r\n", log_retry_cnt)) ;
                    }
                    log_retry_cnt++;

                    mtlte_hif_sdio_enable_interrupt();
                    KAL_SLEEP_USEC(50) ;
                }else{
                    previous_rx_work_resp = rx_work_resp;
                    previous_tx_work_resp = tx_work_resp;
                    time_of_no_resource++;
                }
            }else{
                previous_rx_work_resp = 0;
                previous_tx_work_resp = 0;
                time_of_no_resource = 0;
            }
            
		}

#if INTEGRATION_DEBUG
        if(eemcs_sdio_throughput_log == 1){
            //jiffies_to_timespec(jiffies , &lte_time_now_t);
            lte_jiffies_now = jiffies;
            lte_time_diff_sec = (lte_jiffies_now - lte_jiffies_before) / HZ;

            if(lte_time_diff_sec >= 10){
                log_sdio_print_now = 1;
            }else if(lte_jiffies_now < lte_jiffies_before){
                log_sdio_print_now = 1;
                lte_time_diff_sec = 0;
            }else{
                log_sdio_print_now = 0;
            }
            
            if( log_sdio_print_now == 1){

                KAL_DBGPRINT(KAL, DBG_ERROR,(" ===== SDIO perf log ===== \n")) ;
                
                if(log_sdio_ul_now == 1){
                    for(log_qno=0; log_qno<TXQ_NUM; log_qno++){
                        KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO perf] pkt in UL SWQ%d now = %d \n", log_qno, mtlte_df_UL_pkt_in_swq(log_qno))) ;
                    }
                }
                if(log_sdio_dl_now == 1){
                    for(log_qno=0; log_qno<RXQ_NUM; log_qno++){
                        KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO perf] pkt in DL SWQ%d now = %d \n", log_qno, mtlte_df_DL_pkt_in_swq(log_qno))) ;
                    }
                }
                if(log_sdio_ul_history == 1){
                    log_total_ul = 0;
                    for(log_qno=0; log_qno<TXQ_NUM; log_qno++){
                        KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO perf] pkt of ULQ%d in past %d sec = %d, now TxQcnt=%d \n", log_qno, lte_time_diff_sec, sdio_txq_skb_log[log_qno], hif_sdio_handler.tx_pkt_cnt[log_qno])) ;
                        log_total_ul += sdio_txq_skb_log[log_qno];
                    }
                    KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO perf] Total ULQ packet in past %d sec = %d \n", lte_time_diff_sec, log_total_ul)) ;
                }
                if(log_sdio_dl_history == 1){
                    log_total_dl = 0;
                    for(log_qno=0; log_qno<RXQ_NUM; log_qno++){
                        KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO perf] pkt of DLQ%d in past %d sec = %d \n", log_qno, lte_time_diff_sec, sdio_rxq_skb_log[log_qno])) ;
                        log_total_dl += sdio_rxq_skb_log[log_qno];
                    }
                    KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO perf] Total DLQ packet in past %d sec = %d \n", lte_time_diff_sec, log_total_dl)) ;
                }

                if(log_sdio_buf_pool == 1){
                    #if BUFFER_POOL_FOR_EACH_QUE
                        for(log_qno=0; log_qno<RXQ_NUM; log_qno++){
                            KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO perf] empty skb in pool of RxQ%d now = %d \n", log_qno, mtlte_df_DL_pkt_in_buff_pool(log_qno))) ;
                        }
                    #else
                        KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO perf] empty skb in pool now = %d \n", mtlte_df_DL_pkt_in_buff_pool())) ;
                    #endif
                }
                
                KAL_DBGPRINT(KAL, DBG_ERROR,(" ===== ===== ===== ===== \n")) ;

                //lte_time_before_t = lte_time_now_t;
                lte_jiffies_before = lte_jiffies_now;
                for(log_qno=0; log_qno<TXQ_NUM; log_qno++){
                    sdio_txq_skb_log[log_qno] = 0;
                }
                for(log_qno=0; log_qno<RXQ_NUM; log_qno++){
                    sdio_rxq_skb_log[log_qno] = 0;
                }
                
            }
                    
        }
#endif

	}

    // 3.5  exception handle
    if(hif_sdio_handler.expt_reset_allQ)
    {
        mtlte_hif_expt_reset_allQ();
    }

#if USING_WAKE_MD_EINT
    // move to thread to avoid race condition
#else
	// 4. own back to FW   
	mtlte_hif_sdio_give_fw_own() ;

END_OF_TXRX_PROCESS :

#endif


	// 5. enable the SDIO interrupt   
	mtlte_hif_sdio_enable_interrupt() ;

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	if (jiffies >= proc_start) {
		proc_time = jiffies - proc_start;
		if (proc_time > 100) {
			KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO perf][Busy] <==== %s, proc_time= %dms\n",KAL_FUNC_NAME, 10*proc_time)) ;
		}
	}
	return KAL_SUCCESS;
}

KAL_INT32 mtlte_hif_enable_interrupt_at_probe(void)
{
    // Enable the FW to Host SDIO interrupt 
	mtlte_hif_sdio_enable_interrupt() ;
	mtlte_hif_sdio_unmask_interrupt() ;
	return KAL_SUCCESS;
}


static void mtlte_hif_DL_skb_enqueue_work(struct work_struct *work)
{	
    KAL_INT32 j = 0;
    MTLTE_HIF_RX_BUF_USAGE buf_usage_now;
    KAL_UINT16	*fw_rx_pkt_num ;
	KAL_UINT16	*fw_rx_pkt_len ;
    KAL_UINT32 total_len ;
    KAL_UINT16 total_pkt_num ;
    KAL_INT32 rxbuf_thistime = 0xFF ;
    KAL_INT32 rxque_num_thistime = 0xFF ;
    KAL_UINT32 rxbuf_start ;
    KAL_UINT32 rxbuf_end ;
    KAL_UINT32 continue_check;
    KAL_UINT32 start_time = 0, cur_time = 0, monitor_time = 0;
    //KAL_UINT32 diff_time = 0; 
    
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	

    rxbuf_start = rxbuf_end = hif_sdio_handler.rx_buf_order_dispatch;
    continue_check = 1;

    //for(i=0; i<RX_BUF_NUM; i++){
    
    while(continue_check){
        
        rxbuf_thistime = 0xFF ;
        rxque_num_thistime = 0xFF ;
        
        KAL_MUTEXLOCK(&hif_sdio_handler.rx_buf_lock[rxbuf_start]);
        buf_usage_now = hif_sdio_handler.rx_buf_usage[rxbuf_start];
        KAL_MUTEXUNLOCK(&hif_sdio_handler.rx_buf_lock[rxbuf_start]);

        if(buf_usage_now != NOT_BE_USED){
            rxque_num_thistime = buf_usage_now;
            rxbuf_thistime = rxbuf_start;
            //break;
        }else{
            continue_check = 0;
        }
  

        if(0xFF != rxque_num_thistime){
			start_time = jiffies;
            KAL_MUTEXLOCK(&hif_sdio_handler.all_buf_full);
            
            switch(rxque_num_thistime){
    			case RXQ_Q0:				
    			{
    			    fw_rx_pkt_num = &(hif_sdio_handler.whisr_for_rxbuf[rxbuf_thistime]->rx0_num) ;
    			    fw_rx_pkt_len = hif_sdio_handler.whisr_for_rxbuf[rxbuf_thistime]->rx0_pkt_len ;
    				break ;
    			}
    			case RXQ_Q1:
    			{
    			    fw_rx_pkt_num = &(hif_sdio_handler.whisr_for_rxbuf[rxbuf_thistime]->rx1_num) ;
    			    fw_rx_pkt_len = hif_sdio_handler.whisr_for_rxbuf[rxbuf_thistime]->rx1_pkt_len ;
    				break ;
    			}
    			case RXQ_Q2:
    			{
    			    fw_rx_pkt_num = &(hif_sdio_handler.whisr_for_rxbuf[rxbuf_thistime]->rx2_num) ;
    			    fw_rx_pkt_len = hif_sdio_handler.whisr_for_rxbuf[rxbuf_thistime]->rx2_pkt_len ;		
    			    break;
    		    }
    		    case RXQ_Q3:
    			{
    			    fw_rx_pkt_num = &(hif_sdio_handler.whisr_for_rxbuf[rxbuf_thistime]->rx3_num) ;
    			    fw_rx_pkt_len = hif_sdio_handler.whisr_for_rxbuf[rxbuf_thistime]->rx3_pkt_len ;		
    			    break;
    			}
                
    		    default :
    		    {
    			    KAL_ASSERT(0) ;
    			    break;
    			}
    		}

#if ALLOCATE_SKB_IN_QWORK
  #if BUFFER_POOL_FOR_EACH_QUE
            if (mtlte_df_DL_pkt_in_buff_pool(rxque_num_thistime) < rx_queue_info[rxque_num_thistime].max_rx_pktcnt){
			    mtlte_df_DL_try_reload_swq() ; // try to reload it 
			}
  #else
		    if (mtlte_df_DL_pkt_in_buff_pool() < rx_queue_info[rxque_num_thistime].max_rx_pktcnt){
			    mtlte_df_DL_try_reload_swq() ; // try to reload it 
			}	
  #endif          
#endif
  #if BUFFER_POOL_FOR_EACH_QUE
            monitor_time = jiffies;
            while (mtlte_df_DL_pkt_in_buff_pool(rxque_num_thistime) < rx_queue_info[rxque_num_thistime].max_rx_pktcnt){
                mtlte_df_DL_prepare_skb_for_swq_short(rx_queue_info[rxque_num_thistime].max_rx_pktcnt, rxque_num_thistime);
                if (mtlte_df_DL_pkt_in_buff_pool(rxque_num_thistime) < rx_queue_info[rxque_num_thistime].max_rx_pktcnt){
                    KAL_SLEEP_USEC(1000);
                }
            }
  #else
			monitor_time = jiffies;			
            while (mtlte_df_DL_pkt_in_buff_pool() < rx_queue_info[rxque_num_thistime].max_rx_pktcnt){
                mtlte_df_DL_prepare_skb_for_swq_short(rx_queue_info[rxque_num_thistime].max_rx_pktcnt);
                if (mtlte_df_DL_pkt_in_buff_pool() < rx_queue_info[rxque_num_thistime].max_rx_pktcnt){
                    KAL_SLEEP_USEC(1000);
                }
            }
  #endif
			cur_time = jiffies;
			if (cur_time >= monitor_time) {
				rxq_workq_get_skb_time = cur_time - monitor_time;
				if (rxq_workq_get_skb_time> 50) { //>500ms
					KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO perf][WARN] DLQ%d workq_immediate_reload_skb_time= %dms,rxbuf_thistime=%d \n", 
							rxque_num_thistime, 10*rxq_workq_get_skb_time, rxbuf_thistime));
				}
			}

            total_len = 0 ; 
            total_pkt_num = *(fw_rx_pkt_num);
    		
    		for (j=0 ; j<total_pkt_num ; j++){
    			#if !defined(FORMAL_RELEASE)
    			KAL_ASSERT(mtlte_df_DL_enswq_buf(rxque_num_thistime, (hif_sdio_handler.rx_data_buf[rxbuf_thistime] + total_len), fw_rx_pkt_len[j]) == KAL_SUCCESS) ;
    			#else					
    			mtlte_df_DL_enswq_buf(rxque_num_thistime, (hif_sdio_handler.rx_data_buf[rxbuf_thistime] + total_len), fw_rx_pkt_len[j]) ;
    			#endif						
    			total_len +=KAL_ALIGN_TO_DWORD(fw_rx_pkt_len[j])+MT_LTE_RX_LEGACY_SDU_TAIL ; 
                
#if !DISPATCH_AFTER_ALL_SKB_DONE
                mtlte_df_DL_rx_callback(rxque_num_thistime);
#endif
    		}
            
            #if USE_QUE_WORK_DISPATCH_RX
              #if USE_MULTI_QUE_DISPATCH
                mtlte_df_DL_try_dispatch_rxque(rxque_num_thistime);
              #else
                mtlte_df_DL_try_dispatch_rx();
              #endif
            #endif

            // restore the usage mark
            KAL_MUTEXLOCK(&hif_sdio_handler.rx_buf_lock[rxbuf_thistime]);
            hif_sdio_handler.rx_buf_usage[rxbuf_thistime] = NOT_BE_USED;
            KAL_MUTEXUNLOCK(&hif_sdio_handler.rx_buf_lock[rxbuf_thistime]);

            KAL_MUTEXUNLOCK(&hif_sdio_handler.all_buf_full);

#if USE_EVENT_TO_WAKE_BUFFER  
            KAL_MUTEXLOCK(&hif_sdio_handler.wake_evt_lock[rxbuf_thistime]);
            hif_sdio_handler.wake_evt_buff[rxbuf_thistime]= 1;
            KAL_MUTEXUNLOCK(&hif_sdio_handler.wake_evt_lock[rxbuf_thistime]);
            wake_up_all(&(lte_dev_p->sdio_thread_wq));
#endif

#if DISPATCH_AFTER_ALL_SKB_DONE
            monitor_time = jiffies;
            for (j=0 ; j<total_pkt_num ; j++){
                mtlte_df_DL_rx_callback(rxque_num_thistime);
            }
			cur_time = jiffies;
			if (cur_time >= monitor_time) {
				rxq_workq_callback_user_time = cur_time - monitor_time;
				if (rxq_workq_callback_user_time > 50) { //>500ms
					KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO perf][WARN] DLQ%d workq_callback_to_user_time=%dms, total_pkt_num=%d ,rxbuf_thistime=%d\n", 
								rxque_num_thistime, 10*rxq_workq_callback_user_time, total_pkt_num, rxbuf_thistime));
				}
			}
#endif

            rxbuf_start++;
            if(rxbuf_start >= RX_BUF_NUM){
                rxbuf_start = 0;
            }

            hif_sdio_handler.rx_buf_order_dispatch = rxbuf_start;

			cur_time = jiffies;
			if (cur_time >= start_time) {
				rxq_workq_buf_exe_time = cur_time - start_time;
				if (rxq_workq_buf_exe_time > 50) { //>500ms
					KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO perf][WARN] DLQ%d rxq_workq_total_time = %dms, total_pkt_num=%d, rxbuf_thistime=%d \n", 
							rxque_num_thistime, 10*rxq_workq_buf_exe_time, total_pkt_num, rxbuf_thistime));
				}
			}
        }
    }

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return ;

}


// temp for DL flow control
int mtlte_manual_turnoff_DL_port(MTLTE_DF_RX_QUEUE_TYPE qno)
{
    KAL_UINT32 i = qno;
    //KAL_UINT32 value;

    if(mtlte_df_DL_check_fl_ctrl_enable(qno))
    {
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s] WARN : DL port %d is using formal flow control !!! ",KAL_FUNC_NAME, qno)) ;
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s] WARN : Do not use temp API again !!! ",KAL_FUNC_NAME)) ;
        return KAL_FAIL;
    }
    
    if(0 == hif_sdio_handler.rx_manual_stop[i])
    {
        // Not to access MD SDIO due to sleep mode is on
        //if (sdio_func1_rd(SDIO_IP_WHIER, &value, 4) != KAL_SUCCESS){ return KAL_FAIL ; }
        //value &= ~(0x1 << (i+1));
        //if (sdio_func1_wr(SDIO_IP_WHIER, &value, 4) != KAL_SUCCESS){ return KAL_FAIL ; }
        
        hif_sdio_handler.rx_manual_stop[i] = 1;

        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s] INFO : DL port %d is turn off !!! ",KAL_FUNC_NAME, qno)) ;
        return KAL_SUCCESS;
    }
    else
    {
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s] WARN : DL port %d is already turn off !!! ",KAL_FUNC_NAME, qno)) ;
        return KAL_FAIL;
    }
}

int mtlte_manual_turnon_DL_port(MTLTE_DF_RX_QUEUE_TYPE qno)
{
    KAL_UINT32 i = qno;
    //KAL_UINT32 value;

    if(mtlte_df_DL_check_fl_ctrl_enable(qno))
    {
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s] WARN : DL port %d is using formal flow control !!! ",KAL_FUNC_NAME, qno)) ;
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s] WARN : Do not use temp API again !!! ",KAL_FUNC_NAME)) ;
        return KAL_FAIL;
    }
    
    if(1 == hif_sdio_handler.rx_manual_stop[i])
    {
        // Not to access MD SDIO due to sleep mode is on
        //if (sdio_func1_rd(SDIO_IP_WHIER, &value, 4) != KAL_SUCCESS){ return KAL_FAIL ; }
        //value |= (0x1 << (i+1));
        //if (sdio_func1_wr(SDIO_IP_WHIER, &value, 4) != KAL_SUCCESS){ return KAL_FAIL ; }
        
        hif_sdio_handler.rx_manual_stop[i] = 0;

        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s] INFO : DL port %d is turn on !!! ",KAL_FUNC_NAME, qno)) ;
        return KAL_SUCCESS;
    }
    else
    {
        KAL_DBGPRINT(KAL, DBG_ERROR,("[%s] WARN : DL port %d is already turn on !!! ",KAL_FUNC_NAME, qno)) ;
        return KAL_FAIL;
    }
}


extern void lte_sdio_enable_wd_eirq(void);
extern void lte_sdio_disable_wd_eirq(void);
extern void lte_sdio_request_wd_eirq(void *irq_handler, void *data);
extern void lte_sdio_disable_eirq(void);
int hif_irq_data = 0;

int mtlte_hif_WDT_handle(int hif_handle_data)
{
    LTE_WD_timeout_indicator = 1;
    lte_sdio_disable_wd_eirq();
    lte_sdio_disable_eirq();
    mtlte_hif_sdio_txrx_proc_enable(0);
    
    return mtlte_df_WDT_handle(0);
}

void mtlte_enable_WDT_flow(void)
{
    lte_sdio_enable_wd_eirq();
}

KAL_INT32 mtlte_hif_sdio_probe()
{
	KAL_INT32 i = 0 ;
	KAL_INT32 ret = KAL_SUCCESS;

	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;
	
	for (i=0 ; i<TXQ_NUM; i++){
		hif_sdio_handler.tx_pkt_cnt[i] = 0 ;
	}
	hif_sdio_handler.enh_whisr_cache->rx0_num = 0 ;
	hif_sdio_handler.enh_whisr_cache->rx1_num = 0 ;
	hif_sdio_handler.enh_whisr_cache->rx2_num = 0 ;
	hif_sdio_handler.enh_whisr_cache->rx3_num = 0 ;

	/* After power reset or device reset, the default fw_own is 1 */			
	hif_sdio_handler.fw_own = 1 ;
	mtlte_hif_sdio_enable_fw_own_back(0);
    
    /* setting rx multi buf related register */
    for (i=0 ; i<RX_BUF_NUM; i++){
        hif_sdio_handler.rx_buf_usage[i] = NOT_BE_USED;
    }

	/* register the call back frm df to hif*/
	mtlte_df_register_df_to_hif_callback(mtlte_hif_sdio_tx_kick_process, 0) ;
	
			
	/* Setting Host Side HW register's */	
	if ((ret = mtlte_hif_sdio_hw_setting()) != KAL_SUCCESS){
		KAL_DBGPRINT(KAL, DBG_ERROR,("[PROBE] XXXXXX mt_lte_sdio_probe -mtlte_hif_sdio_hw_setting fail \n")); 
	}

#if TEST_DRV
    attest_setting_param.testing_ulq_count = 0;
    attest_setting_param.testing_dlq_int = 0;
    attest_setting_param.testing_dlq_pkt_fifo = 0;
    attest_setting_param.testing_fifo_max = 0;
    attest_setting_param.testing_error_case = 0;
#endif

#if THREAD_FOR_MEMCPY_SKB
    hif_sdio_handler.rx_buf_order_recv = 0;
    hif_sdio_handler.rx_buf_order_dispatch = 0;
#endif

    //register wd timeout int but not enable it.
    lte_sdio_disable_wd_eirq();
    lte_sdio_request_wd_eirq(mtlte_hif_WDT_handle, (void *)hif_irq_data);
    lte_sdio_disable_wd_eirq();
	
#if INTEGRATION_DEBUG
    //jiffies_to_timespec(jiffies , &lte_time_before_t);
    lte_jiffies_before = jiffies;
        
    for (i=0 ; i<TXQ_NUM; i++){
        sdio_txq_skb_log[i] = 0;
    }

    for (i=0 ; i<RXQ_NUM; i++){
        sdio_rxq_skb_log[i] = 0;
    }
#endif

    hif_sdio_handler.expt_reset_allQ = 0 ;

    for (i=0 ; i<TXQ_NUM; i++){
		hif_sdio_handler.tx_expt_stop[i] = 0 ;
	}

    for (i=0 ; i<RXQ_NUM; i++){
		hif_sdio_handler.rx_expt_stop[i] = 0 ;
	}

#if FORMAL_DL_FLOW_CONTROL
    for (i=0 ; i<RXQ_NUM; i++){
        hif_sdio_handler.rx_fl_ctrl_stop[i] = 0 ;
        hif_sdio_handler.rx_fl_ctrl_stop_count[i] = 0 ;
	}
#endif

#if USE_EVENT_TO_WAKE_BUFFER
    for (i=0 ; i<RX_BUF_NUM; i++){
        hif_sdio_handler.wake_evt_buff[i] = 0;
    }
#endif

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	
	return ret ; 
}

KAL_INT32 mtlte_hif_sdio_remove_phase1()
{
	KAL_INT32 ret = KAL_SUCCESS;

	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;
	
    lte_sdio_disable_wd_eirq();
	
    mtlte_hif_sdio_clear_fw_own();

    mtlte_hif_sdio_enable_fw_own_back(0);

#if THREAD_FOR_MEMCPY_SKB
    flush_workqueue(hif_sdio_handler.rx_memcpy_work_queue);
#endif

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	
	return ret ; 
}


KAL_INT32 mtlte_hif_sdio_remove_phase2()
{
	KAL_INT32 i = 0 ;
	KAL_INT32 ret = KAL_SUCCESS;

	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;
	
#if THREAD_FOR_MEMCPY_SKB    
    for (i=0 ; i<RX_BUF_NUM; i++){
        hif_sdio_handler.rx_buf_usage[i] = NOT_BE_USED;
    }
    hif_sdio_handler.rx_buf_order_recv = 0;
    hif_sdio_handler.rx_buf_order_dispatch = 0;
#endif

	for (i=0 ; i<TXQ_NUM; i++){
		hif_sdio_handler.tx_pkt_cnt[i] = 0 ;
	}
	hif_sdio_handler.enh_whisr_cache->rx0_num = 0 ;
	hif_sdio_handler.enh_whisr_cache->rx1_num = 0 ;
	hif_sdio_handler.enh_whisr_cache->rx2_num = 0 ;
	hif_sdio_handler.enh_whisr_cache->rx3_num = 0 ;

	KAL_DESTROYMUTEX(&hif_sdio_handler.pwsv_lock) ;

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	
	return ret ; 
}

KAL_INT32 mtlte_hif_sdio_init()
{	
	KAL_INT32 ret = KAL_SUCCESS;
    KAL_INT32 i = 0 ;

	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;

#ifdef USER_BUILD_KERNEL
    mtlte_hal_register_MSDC_ERR_callback(mtlte_hif_WDT_handle);
#endif
	
	if ((ret = KAL_ALLOCATE_PHYSICAL_DMA_MEM_NEW(hif_sdio_handler.data_buf, MT_LTE_SDIO_DATA_BUFF_LEN))){
		goto BUF_MEM_FAIL ;
	}
	KAL_ZERO_MEM(hif_sdio_handler.data_buf, MT_LTE_SDIO_DATA_BUFF_LEN) ;

	if ((ret = KAL_ALLOCATE_PHYSICAL_DMA_MEM(hif_sdio_handler.sdio_wasr_cache, sizeof(sdio_wasr)))){
		goto WASR_MEM_FAIL ;
	}
	KAL_ZERO_MEM(hif_sdio_handler.sdio_wasr_cache, sizeof(sdio_wasr)) ;

	if ((ret = KAL_ALLOCATE_PHYSICAL_DMA_MEM(hif_sdio_handler.enh_whisr_cache,  sizeof(sdio_whisr_enhance)))){
		goto WHISR_MEM_FAIL ;
	}
	KAL_ZERO_MEM(hif_sdio_handler.enh_whisr_cache, sizeof(sdio_whisr_enhance)) ;
	
    for(i=0; i<RX_BUF_NUM; i++){
        if ((ret = KAL_ALLOCATE_PHYSICAL_DMA_MEM(hif_sdio_handler.rx_data_buf[i], MT_LTE_SDIO_DATA_BUFF_LEN))){
    		goto RX_DATA_BUF_MEM_FAIL ;
    	}
    	KAL_ZERO_MEM(hif_sdio_handler.rx_data_buf[i], MT_LTE_SDIO_DATA_BUFF_LEN) ;
    }

    for(i=0; i<RX_BUF_NUM; i++){
        if ((ret = KAL_ALLOCATE_PHYSICAL_DMA_MEM(hif_sdio_handler.whisr_for_rxbuf[i],  sizeof(sdio_whisr_enhance)))){
		    goto WHISR_RXBUF_MEM_FAIL ;
	    }
	    KAL_ZERO_MEM(hif_sdio_handler.whisr_for_rxbuf[i], sizeof(sdio_whisr_enhance)) ;
    }

	hif_sdio_handler.txrx_enable = 0 ;

    hif_sdio_handler.expt_reset_allQ = 0 ;

    for (i=0 ; i<TXQ_NUM; i++){
		hif_sdio_handler.tx_expt_stop[i] = 0 ;
	}

    for (i=0 ; i<RXQ_NUM; i++){
		hif_sdio_handler.rx_expt_stop[i] = 0 ;
	}

    // temp for dl flow control
    for (i=0 ; i<RXQ_NUM; i++){
		hif_sdio_handler.rx_manual_stop[i] = 0 ;
	}

    /* Change mutex init from probe to init to avoid race condition after remove */
    KAL_AQUIREMUTEX(&hif_sdio_handler.pwsv_lock) ;

#if THREAD_FOR_MEMCPY_SKB    
    for (i=0 ; i<RX_BUF_NUM; i++){
        KAL_AQUIREMUTEX(&hif_sdio_handler.rx_buf_lock[i]) ;
    }
    KAL_AQUIREMUTEX(&hif_sdio_handler.all_buf_full) ;
#endif

#if USE_EVENT_TO_WAKE_BUFFER
    for (i=0 ; i<RX_BUF_NUM; i++){
        KAL_AQUIREMUTEX(&hif_sdio_handler.wake_evt_lock[i]) ;
    }
#endif
    

#if THREAD_FOR_MEMCPY_SKB

#if defined (TCP_OUT_OF_ORDER_SOLUTION)
    if (num_possible_cpus() > 2)
        lte_online_cpu = 2; //skb_enqueue_work default bound on CPU2
    else if (num_possible_cpus() == 2)
        lte_online_cpu = 1; //skb_enqueue_work default bound on CPU1
    else if (num_possible_cpus() == 1)
        lte_online_cpu = 0; //skb_enqueue_work default bound on CPU0
    else
        lte_online_cpu = 0; //skb_enqueue_work default bound on CPU0
    KAL_DBGPRINT(KAL, DBG_ERROR, ("num_possible_cpus=%d, lte_cpu=%d\n", \
        num_possible_cpus(), lte_online_cpu)) ;

    hif_sdio_handler.rx_memcpy_work_queue = alloc_workqueue("lte_sdio_memcpy_work", \
        WQ_MEM_RECLAIM | WQ_NON_REENTRANT, 1); 

#else
    hif_sdio_handler.rx_memcpy_work_queue = create_singlethread_workqueue("lte_sdio_memcpy_work");

#endif

    if (!hif_sdio_handler.rx_memcpy_work_queue) {
        KAL_DBGPRINT(KAL, DBG_ERROR, ("[ERR] create mtlte_sdio_memcpy_work fail!!\n")) ;
        return -ENOMEM;
    } else {
    	KAL_DBGPRINT(KAL, DBG_ERROR, ("create mtlte_sdio_memcpy_work ok\n")) ;
    }
    INIT_WORK(&hif_sdio_handler.rx_memcpy_work, mtlte_hif_DL_skb_enqueue_work);
#endif

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;

	return KAL_SUCCESS ;

WHISR_RXBUF_MEM_FAIL:
    for(i=0; i<RX_BUF_NUM; i++){
        KAL_FREE_PHYSICAL_MEM(hif_sdio_handler.rx_data_buf[i]) ;
    }
RX_DATA_BUF_MEM_FAIL:
	KAL_FREE_PHYSICAL_MEM(hif_sdio_handler.enh_whisr_cache) ;
WHISR_MEM_FAIL:
	KAL_FREE_PHYSICAL_MEM(hif_sdio_handler.sdio_wasr_cache) ;
WASR_MEM_FAIL:
	KAL_FREE_PHYSICAL_MEM(hif_sdio_handler.data_buf) ;
BUF_MEM_FAIL:
	return ret ;

}

KAL_INT32 mtlte_hif_sdio_deinit()
{
	KAL_INT32 ret = KAL_SUCCESS;
    KAL_INT32 i = 0;

    KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;

#if THREAD_FOR_MEMCPY_SKB
        destroy_workqueue(hif_sdio_handler.rx_memcpy_work_queue);

        for (i=0 ; i<RX_BUF_NUM; i++){
            KAL_DESTROYMUTEX(&hif_sdio_handler.rx_buf_lock[i]) ;
        }
        KAL_DESTROYMUTEX(&hif_sdio_handler.all_buf_full) ;
#endif

#if USE_EVENT_TO_WAKE_BUFFER
    for (i=0 ; i<RX_BUF_NUM; i++){
            KAL_DESTROYMUTEX(&hif_sdio_handler.wake_evt_lock[i]) ;
        }
#endif
    KAL_DESTROYMUTEX(&hif_sdio_handler.pwsv_lock) ;
	
	KAL_FREE_PHYSICAL_MEM(hif_sdio_handler.data_buf) ;

	KAL_FREE_PHYSICAL_MEM(hif_sdio_handler.sdio_wasr_cache) ;

	KAL_FREE_PHYSICAL_MEM(hif_sdio_handler.enh_whisr_cache) ;
	
    for(i=0; i<RX_BUF_NUM; i++){
        KAL_FREE_PHYSICAL_MEM(hif_sdio_handler.rx_data_buf[i]) ;
    }

    for(i=0; i<RX_BUF_NUM; i++){
        KAL_FREE_PHYSICAL_MEM(hif_sdio_handler.whisr_for_rxbuf[i]) ;
    }
	
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	
	return ret ;
}


KAL_INT32 mtlte_hif_sdio_give_fw_own_in_main(void)
{
    return mtlte_hif_sdio_give_fw_own();
}

KAL_INT32 mtlte_hif_sdio_get_driver_own_in_main(void)
{
    return mtlte_hif_sdio_get_driver_own();
}



/*************************************************************
*
*				FOR AUTO TEST   
*
*************************************************************/

#if TEST_DRV

#define HIF_MAX_ULQ_NUM		7
#define HIF_MAX_DLQ_NUM		4

KAL_INT32 at_mtlte_hif_sdio_give_fw_own(void)
{
    return mtlte_hif_sdio_give_fw_own();
}

KAL_INT32 at_mtlte_hif_sdio_get_driver_own(void)
{
    return mtlte_hif_sdio_get_driver_own();
}

KAL_INT32 at_mtlte_hif_set_test_param(athif_test_param_t set_param, athif_test_set_item_e set_item)
{
    KAL_INT32 i;
    
    switch (set_item){
        
        case set_testing_ulq_count:
            attest_setting_param.testing_ulq_count = set_param.testing_ulq_count;
            break;
            
        case set_testing_dlq_int:
            attest_setting_param.testing_dlq_int= set_param.testing_dlq_int;
            break;
            
        case set_testing_dlq_pkt_fifo:
            attest_setting_param.testing_dlq_pkt_fifo= set_param.testing_dlq_pkt_fifo;
            break;
            
        case set_testing_fifo_max:
            attest_setting_param.testing_fifo_max= set_param.testing_fifo_max;
            break;
            
        case set_received_ulq_count:
            for(i=0; i<8; i++){
                attest_setting_param.received_ulq_count[i] = set_param.received_ulq_count[i];
            }
            break;
            
        case set_int_indicator:
            attest_setting_param.int_indicator = set_param.int_indicator;
            break;

        case set_fifo_max:
            for(i=0; i<HIF_MAX_DLQ_NUM; i++){
                attest_setting_param.fifo_max[i] = set_param.fifo_max[i];
            }
            break;

        case set_test_result:
            attest_setting_param.test_result= set_param.test_result;
            break;

        case set_abnormal_status:
            attest_setting_param.abnormal_status = set_param.abnormal_status;
            break;
            
        case set_all:
            attest_setting_param = set_param;
            break;
            
        default :
            break;
    }  
    return 0;
}


athif_test_param_t at_mtlte_hif_get_test_param(void)
{
    return attest_setting_param;
}


KAL_INT32 at_mtlte_hif_sdio_clear_tx_count(void)
{
    KAL_INT32 i;
    for (i=0 ; i<TXQ_NUM; i++){
	    hif_sdio_handler.tx_pkt_cnt[i] = 0 ;
    }

    return 0;
}

KAL_INT32 at_mtlte_hif_sdio_get_tx_count(KAL_UINT32 *tx_count)
{
    KAL_INT32 i;
    for(i=0; i<TXQ_NUM; i++){
        *(tx_count+i) = hif_sdio_handler.tx_pkt_cnt[i];
    }

    return 0;
}

KAL_INT32 at_mtlte_hif_sdio_reset_abnormal_enable(KAL_INT32 abnormal_limit)
{
    abnormal_int_count = 0;
    ABNORMAL_INT_LIMIT = abnormal_limit;
    stop_when_abnormal = 1;
    return 0;
}

KAL_INT32 at_mtlte_hif_sdio_reset_abnormal_disable(void)
{
    stop_when_abnormal = 0;
    return 0;
}

void at_transform_whisr_rx_tail( sdio_whisr_enhance *tail_ptr)
{
    transform_whisr_rx_tail( tail_ptr );
}


#endif


