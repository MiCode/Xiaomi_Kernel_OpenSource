#ifndef __LTE_HIF_SDIO_H__
#define __LTE_HIF_SDIO_H__

#define INTEGRATION_DEBUG              1

#include "eemcs_kal.h"
#include "lte_df_main.h"
#include "lte_main.h"

#define THREAD_FOR_MEMCPY_SKB          1
#define ALLOCATE_SKB_IN_QWORK          1
//#define WAKE_AFTER_BUF_EMPTY            0 
#define USE_EVENT_TO_WAKE_BUFFER       1

#define USING_WAKE_MD_EINT             1

#define MT_LTE_SDIO_VENDOR_ID           (0x037A)

#define MT_LTE_SDIO_DEVICE_ID           (0x6290) // 0x{lte_deviceid}

// Set the WHIER
#define WHISR_INT_MASK					(0xFFFFFFDE)  // maks the Tx done interrupt 

/////////////////////////////////////////////////////////////////////////
/* TX related parameters */
#define MT_LTE_TX_HEADER_LENGTH		(4)

#define MT_LTE_TX_PACKET_ALIGN		(4)

#define MT_LTE_TX_ZERO_PADDING_LEN			(4)


/////////////////////////////////////////////////////////////////////////
/* RX related parameters */
#define MT_LTE_RX_HEADER_LENGTH		(0)


#if EMCS_SDIO_DRVTST
// for rx max pkt test temp use
#define MT_LTE_RX_Q0_PKT_CNT		(64)
#define MT_LTE_RX_Q1_PKT_CNT		(64)
#define MT_LTE_RX_Q2_PKT_CNT		(64)
#define MT_LTE_RX_Q3_PKT_CNT		(64)

#define MT_LTE_RX_TAILOR_LENGTH		768 

#else
// for rx max pkt test temp use
#define MT_LTE_RX_Q0_PKT_CNT		MT_LTE_RXQ0_MAX_PKT_REPORT_NUM
#define MT_LTE_RX_Q1_PKT_CNT		MT_LTE_RXQ1_MAX_PKT_REPORT_NUM
#define MT_LTE_RX_Q2_PKT_CNT		MT_LTE_RXQ2_MAX_PKT_REPORT_NUM
#define MT_LTE_RX_Q3_PKT_CNT		MT_LTE_RXQ3_MAX_PKT_REPORT_NUM

#define MT_LTE_RX_TAILOR_LENGTH		(4+4+4+2+2+2+2+2*(MT_LTE_RX_Q0_PKT_CNT+MT_LTE_RX_Q1_PKT_CNT+MT_LTE_RX_Q2_PKT_CNT+MT_LTE_RX_Q3_PKT_CNT)+4+4) 


#endif


#if (MT_LTE_RX_TAILOR_LENGTH%4)
	#error 
#endif

// since the new SDIO HW is removed the Rx legacy 4-byte redundant zero, we set the parameter to zero
//#define MT_LTE_RX_LEGACY_SDU_TAIL	(4)
#define MT_LTE_RX_LEGACY_SDU_TAIL	(0)

#define MT_LTE_RX_TAILOR_PRESPACE	(4)

/////////////////////////////////////////////////////////////////////////
/* HW related parameters */
#define MT_LTE_SDIO_BLOCK_SIZE		(0x100)//(256)

#define MT_LTE_SDIO_ALIGN_BLOCK_SIZE_A		(MT_LTE_SDIO_BLOCK_SIZE-1)//(0xFF)

#define ALIGN_TO_BLOCK_SIZE(_value)			(((_value) + MT_LTE_SDIO_ALIGN_BLOCK_SIZE_A) & ~MT_LTE_SDIO_ALIGN_BLOCK_SIZE_A)

#define MT_LTE_TRX_MAX_PKT_CNT		(32)

//#define MT_LTE_TRX_MAX_PKT_SIZE		DEV_MAX_PKT_SIZE 
/* Saperate MT_LTE_TRX_MAX_PKT_SIZE for DEV_MAX_PKT_SIZE */
#define MT_LTE_TRX_MAX_PKT_SIZE		(4096)

// MT_LTE_TRX_MAX_PKT_CNT * (pkt_size + (tx header or rx legacy ) + rx tailor)
#define MT_LTE_SDIO_DATA_BUFF_LEN_UNALIGN (MT_LTE_TRX_MAX_PKT_CNT*(MT_LTE_TRX_MAX_PKT_SIZE+MT_LTE_TX_HEADER_LENGTH)+MT_LTE_RX_TAILOR_PRESPACE+MT_LTE_RX_TAILOR_LENGTH)

// For allocate memory easier, use 2^N size of data buff
//#define MT_LTE_SDIO_DATA_BUFF_LEN	ALIGN_TO_BLOCK_SIZE(MT_LTE_SDIO_DATA_BUFF_LEN_UNALIGN + MT_LTE_TRX_MAX_PKT_SIZE)
#define MT_LTE_SDIO_DATA_BUFF_LEN   131072

//#define MT_LTE_SDIO_DATA_BUFF_TX_LIMIT	(MT_LTE_SDIO_DATA_BUFF_LEN-MT_LTE_TRX_MAX_PKT_SIZE-MT_LTE_TX_HEADER_LENGTH)
#define MT_LTE_SDIO_DATA_BUFF_TX_LIMIT	((256*254)-MT_LTE_TRX_MAX_PKT_SIZE-MT_LTE_TX_HEADER_LENGTH)


#define MT_LTE_SDIO_DATA_BUFF_RX_LIMIT 	(MT_LTE_SDIO_DATA_BUFF_LEN-MT_LTE_TRX_MAX_PKT_SIZE-MT_LTE_RX_TAILOR_LENGTH-MT_LTE_RX_LEGACY_SDU_TAIL-MT_LTE_RX_TAILOR_PRESPACE)
/////////////////////////////////////////////////////////////////////////
typedef enum _SDIO_TXRXWORK_RESP {
    
    /* ================= TX part ========================== */

	WORK_RESP_TX_SENT_PKT = 0x1 , 
	WORK_RESP_TX_NO_ACTION = 0x2 , 
	WORK_RESP_TX_NOT_COMPLETE = 0x4 , 
	
	WORK_RESP_TX_NO_RESOURCE = 0x10 , 

	WORK_RESP_TX_DATA_ERROR = 0x100 , 

    /* ================= RX part ===================== */

    WORK_RESP_RX_RECV_PKT = 0x10000, 
	WORK_RESP_RX_NOT_COMPLETE = 0x40000 , 
        WORK_RESP_RX_NO_PHYCICAL_BUF = 0x80000 ,

	WORK_RESP_RX_DATA_ERROR = 0x1000000 , 
	
}SDIO_TXRXWORK_RESP ;

/*
typedef enum _SDIO_RXWORK_RESP {
	WORK_RESP_RX_RECV_PKT = 0x1 , 
	WORK_RESP_RX_NOT_COMPLETE = 0x4 , 
	WORK_RESP_RX_NO_PHYCICAL_BUF = 0x8 , 

	WORK_RESP_RX_DATA_ERROR = 0x100 , 
}SDIO_RXWORK_RESP ;
*/

typedef enum _SDIO_TXRXWORK_STATE {
	WORK_STATE_IDLE = 0x0 , 
	WORK_STATE_RUNNING = 0x1 , 

}SDIO_TXRXWORK_STATE ;


#define  RX_BUF_NUM   2

typedef enum{
	RXQ0_RECEIVED = 0,
	RXQ1_RECEIVED = 1,
	RXQ2_RECEIVED = 2 ,	
	RXQ3_RECEIVED = 3 ,	
	NOT_BE_USED   = 0xFF ,
} MTLTE_HIF_RX_BUF_USAGE ;


typedef int (*MTLTE_HIF_TO_SYS_CALLBACK)(unsigned int data);

struct mtlte_hif_to_sys_callback {	
	MTLTE_HIF_TO_SYS_CALLBACK callback_func ;
	unsigned int private_data ;

    MTLTE_HIF_TO_SYS_CALLBACK sleep_callback_func ;
	unsigned int sleep_private_data ;
} ;

typedef struct SDIO_TX_SDU_HEADER {
    union {
        struct {
            KAL_UINT32	length:12;
          	KAL_UINT32	reserved:17;
            KAL_UINT32	tx_type:3;
        } bits;
        KAL_UINT32         asUINT32;
    } u;
} sdio_tx_sdu_header;

typedef struct SDIO_WASR {
    union {
        struct {
            KAL_UINT32	tx0_overflow:1;
            KAL_UINT32	tx1_overflow:1;
            KAL_UINT32	reserved0:6;
            KAL_UINT32	rx0_underflow:1;
            KAL_UINT32	rx1_underflow:1;
            KAL_UINT32	rx2_underflow:1;
            KAL_UINT32	rx3_underflow:1;
          	KAL_UINT32	reserved1:4;
          	KAL_UINT32	fw_own_invalid_access:1;
            KAL_UINT32	reserved2:15;
        } bits;
        KAL_UINT32         asUINT32;
    } u;
} sdio_wasr ;

typedef struct SDIO_WHISR {
    union {
        struct {
            KAL_UINT32	tx_done_int:1;
            KAL_UINT32	rx0_done_int:1;
            KAL_UINT32	rx1_done_int:1;
            KAL_UINT32	rx2_done_int:1;
            KAL_UINT32	rx3_done_int:1;
            KAL_UINT32	reserved:1;
            KAL_UINT32	abnormal_int:1;
          	KAL_UINT32	fw_ownback_int:1;
            KAL_UINT32	d2h_sw_int:24;
        } bits;
        KAL_UINT32         asUINT32;
    } u;
} sdio_whisr;

typedef struct SDIO_WTSR0 {
    union {
        struct {
            KAL_UINT32	tq0_cnt:8;
          	KAL_UINT32	tq1_cnt:8;
            KAL_UINT32	tq2_cnt:8;
            KAL_UINT32	tq3_cnt:8;            
        } bits;
        KAL_UINT32         asUINT32;
    } u;
} sdio_wtsr0;

typedef struct SDIO_WTSR1 {
    union {
        struct {
            KAL_UINT32	tq4_cnt:8;
          	KAL_UINT32	tq5_cnt:8;
            KAL_UINT32	tq6_cnt:8;
            KAL_UINT32	tq7_cnt:8;            
        } bits;
        KAL_UINT32         asUINT32;
    } u;
} sdio_wtsr1;

typedef struct SDIO_ENHANCE_WHISR {
	sdio_whisr		whisr ;
	sdio_wtsr0		whtsr0 ;
	sdio_wtsr1		whtsr1 ;
	KAL_UINT16  rx0_num ;
	KAL_UINT16  rx1_num ;
	KAL_UINT16  rx2_num ;
	KAL_UINT16  rx3_num ;;
	KAL_UINT16  rx0_pkt_len[MT_LTE_RX_Q0_PKT_CNT] ;
	KAL_UINT16  rx1_pkt_len[MT_LTE_RX_Q1_PKT_CNT] ;
	KAL_UINT16  rx2_pkt_len[MT_LTE_RX_Q2_PKT_CNT] ;
	KAL_UINT16  rx3_pkt_len[MT_LTE_RX_Q3_PKT_CNT] ;
	KAL_UINT32		d2hrm0r ;
	KAL_UINT32		d2hrm1r ;
} sdio_rx_sdu_tailor, sdio_whisr_enhance;

typedef struct SDIO_TX_QUEUE_INFO {
	KAL_UINT32	qno ; 
	KAL_UINT32	port_address ; 
} sdio_tx_queue_info;

typedef struct SDIO_RX_QUEUE_INFO {
	KAL_UINT32	qno ; 
	KAL_UINT32	port_address ; 
	KAL_UINT32	max_rx_pktcnt ;
	KAL_UINT32	max_rx_swq_inuse ;
} sdio_rx_queue_info ;

typedef struct SDIO_PROC_QUEUE_HANDLE {
	KAL_UINT32	qno ;     
	KAL_UINT32	is_tx ;     
	KAL_UINT32	(*pkt_num_check)(KAL_UINT32 qNo) ;
	SDIO_TXRXWORK_RESP		(*pkt_handler)(KAL_UINT32 qNo) ;
} sdio_proc_queue_handle ;

typedef struct HIF_SDIO_HANDLE {
	/* For Tx Handle */
	KAL_UINT32	tx_pkt_cnt[TXQ_NUM] ;
	KAL_UINT32  h2d_sw_int ;
		
	/* For Rx Handle */	

	/* For HW data transfer */
	sdio_whisr_enhance *enh_whisr_cache ;	
	sdio_wasr	*sdio_wasr_cache ;	
	KAL_UINT8	*data_buf ;
	KAL_UINT32	data_buf_offset ;
	
    /* For Rx ping-pong buffer */
    KAL_UINT8	*rx_data_buf[RX_BUF_NUM];
    MTLTE_HIF_RX_BUF_USAGE  rx_buf_usage[RX_BUF_NUM];
    KAL_MUTEX	rx_buf_lock[RX_BUF_NUM];
    sdio_whisr_enhance *whisr_for_rxbuf[RX_BUF_NUM];
    KAL_UINT32	rx_buf_order_recv ;
    KAL_UINT32	rx_buf_order_dispatch ;

    KAL_MUTEX all_buf_full;
    struct workqueue_struct *rx_memcpy_work_queue;
	struct work_struct rx_memcpy_work;
	
	/* For PWSV */
	volatile KAL_UINT32  fw_own ; 
	KAL_MUTEX	pwsv_lock ;

	/* For Drror Status */

	/* For Sys Control */
	struct mtlte_hif_to_sys_callback sys_callback ;
	volatile KAL_UINT32 	txrx_enable ;	
    
    KAL_UINT32 	expt_reset_allQ ;
    KAL_UINT32	tx_expt_stop[TXQ_NUM] ;
    KAL_UINT32	rx_expt_stop[RXQ_NUM] ;

    // temp for dl flow control
    KAL_UINT32	rx_manual_stop[RXQ_NUM] ;

#if FORMAL_DL_FLOW_CONTROL
    // formal DL flow control
    KAL_UINT32	rx_fl_ctrl_stop[RXQ_NUM] ;
    KAL_UINT32	rx_fl_ctrl_stop_count[RXQ_NUM] ;
#endif

#if USE_EVENT_TO_WAKE_BUFFER
    volatile KAL_UINT32 wake_evt_buff[RX_BUF_NUM];
    KAL_MUTEX	wake_evt_lock[RX_BUF_NUM];
#endif
} hif_sdio_handle;





/** 
 * @name: SDIO IP control registers 
 */
#define SDIO_IP_WCIR			(0x0000) 
#define SDIO_IP_WHLPCR			(0x0004) 
#define SDIO_IP_WSDIOCSR		(0x0008) 
#define SDIO_IP_WHCR			(0x000C)
#define SDIO_IP_WHISR			(0x0010)
#define SDIO_IP_WHIER			(0x0014)

#define SDIO_IP_WASR			(0x0020)
#define SDIO_IP_WSICR			(0x0024)
#define SDIO_IP_WTSR0			(0x0028)
#define SDIO_IP_WTSR1			(0x002C)

//#define SDIO_IP_WTDR0			(0x0030)
#define SDIO_IP_WTDR1			(0x0034)

#define SDIO_IP_WRDR0			(0x0050)
#define SDIO_IP_WRDR1			(0x0054)
#define SDIO_IP_WRDR2			(0x0058)
#define SDIO_IP_WRDR3			(0x005C)

#define SDIO_IP_H2DSM0R			(0x0070)
#define SDIO_IP_H2DSM1R			(0x0074)
#define SDIO_IP_D2HRM0R			(0x0078)
#define SDIO_IP_D2HRM1R			(0x007C)
//#define SDIO_IP_D2HRM2R			(0x0080)
#define SDIO_IP_WRPLR			(0x0090)
#define SDIO_IP_WRPLR1			(0x0094)

#define SDIO_IP_WTMDR       	(0x00B0)	
#define SDIO_IP_WTMCR       	(0x00B4)	
#define SDIO_IP_WTMDPCR0    	(0x00B8)	
#define SDIO_IP_WTMDPCR1    	(0x00BC)	
#define SDIO_IP_WPLRCR      	(0x00D4)	
#define SDIO_IP_CLKIOCR     	(0x0100)	
#define SDIO_IP_CMDIOCR     	(0x0104)	
#define SDIO_IP_DAT0IOCR    	(0x0108)	
#define SDIO_IP_DAT1IOCR    	(0x010C)	
#define SDIO_IP_DAT2IOCR    	(0x0110)	
#define SDIO_IP_DAT3IOCR    	(0x0114)	
#define SDIO_IP_CLKDLYCR    	(0x0118)	
#define SDIO_IP_CMDDLYCR    	(0x011C)	
#define SDIO_IP_ODATDLYCR   	(0x0120)	
#define SDIO_IP_IDATDLYCR1  	(0x0124)	
#define SDIO_IP_IDATDLYCR2  	(0x0128)	
#define SDIO_IP_ILCHCR      	(0x012C)	




//SDIO_IP_WHLPCR		
#define W_INT_EN_SET		(0x1)
#define W_INT_EN_CLR		(0x2) 
#define W_FW_OWN_REQ_SET	(0x100)
#define W_DRV_OWN_STATUS	(0x100)
#define W_FW_OWN_REQ_CLR	(0x200)

//WSDIOCSR 
#define DB_WR_BUSY_EN		(0x8)	
#define DB_RD_BUSY_EN		(0x4)
#define SDIO_INT_CTL		(0x2)	

//WHCR
#define RX_ENHANC_MODE		    (0x10000)	
#define MAX_HIF_RX_LEN_NUM	    (0xF0)
#define RPT_OWN_RX_PACKET_LEN   (0x8)
#define RECV_MAIL_BOX_RD_CLR_EN	(0x4)
#define W_INT_CLR_CTRL		    (0x2)

//WCIR
#define POR_INDICATOR		(0x100000) 
#define W_FUNC_RDY			(0x200000) 	
#define REVISION_ID			(0xF0000) 	
#define CHIP_ID				(0xFFFF) 

//WPLRCR
#define RX0_RPT_PKT_LEN     (0x3F)
#define RX_RPT_PKT_LEN(n)   (RX0_RPT_PKT_LEN<<(n*8))

//WTMCR
#define TEST_MODE_SELECT    (0x3)
#define TEST_MODE_STATUS    (0x1<<8)
#define PRBS_INIT_VAL       (0xFF<<16)
#define TEST_MODE_FW_OWN    (0x1<<24)



void mtlte_hif_sdio_clear_fw_own(void) ;

KAL_INT32 mtlte_hif_sdio_give_fw_own_in_main(void);

KAL_INT32 mtlte_hif_sdio_get_driver_own_in_main(void);

KAL_UINT32 mtlte_hif_sdio_check_fw_own(void);

#if USING_WAKE_MD_EINT
void mtlte_hif_sdio_wake_MD_up_EINT(void);
#endif

KAL_INT32 mtlte_hif_sdio_hw_setting(void) ;

KAL_INT32 mtlte_hif_sdio_wait_FW_ready(void) ;

KAL_INT32 mtlte_hif_expt_mode_init(void);

KAL_INT32 mtlte_hif_expt_restart_que(KAL_UINT32 is_DL, KAL_UINT32 q_num);

KAL_INT32 mtlte_hif_expt_unmask_swint(void);

KAL_INT32 mtlte_hif_expt_enable_interrupt(void);

KAL_INT32 mtlte_hif_expt_set_reset_allQ_bit(void);

KAL_INT32 mtlte_hif_sdio_process(void) ;

KAL_INT32 mtlte_hif_enable_interrupt_at_probe(void);

KAL_INT32 mtlte_hif_sdio_probe(void) ;

//KAL_INT32 mtlte_hif_sdio_remove(void) ;

KAL_INT32 mtlte_hif_sdio_remove_phase1(void) ;
KAL_INT32 mtlte_hif_sdio_remove_phase2(void) ;

KAL_INT32 mtlte_hif_sdio_init(void) ;

KAL_INT32 mtlte_hif_sdio_deinit(void) ;

KAL_INT32 mtlte_hif_register_hif_to_sys_wake_callback(void *func_ptr , unsigned int data) ;
KAL_INT32 mtlte_hif_register_hif_to_sys_sleep_callback(void *func_ptr , unsigned int data) ;


void mtlte_hif_sdio_txrx_proc_enable(KAL_UINT32 enable) ;

void mtlte_hif_sdio_dump(void) ;


/*************************************************************
*
*				FOR AUTO TEST   
*
*************************************************************/

#if TEST_DRV
void mtlte_hif_sdio_enable_fw_own_back(KAL_UINT32 enable) ;
#endif


#if TEST_DRV
typedef struct _athif_test_param{
    KAL_INT16   testing_ulq_count;
    KAL_INT16   testing_dlq_int;
    KAL_INT16   testing_dlq_pkt_fifo;
    KAL_INT16   testing_fifo_max;
    
    KAL_INT16   received_ulq_count[8];
    KAL_INT16   int_indicator;
    KAL_INT16   fifo_max[4];

    KAL_INT16   test_result;
    KAL_INT32   abnormal_status;
    KAL_INT16   testing_error_case;
    
}athif_test_param_t;


typedef enum _ATHIF_TEST_SET_ITEM {
	set_testing_ulq_count = 0x0,
    set_testing_dlq_int,
    set_testing_dlq_pkt_fifo,
    set_testing_fifo_max,
    
    set_received_ulq_count,
    set_int_indicator,
    set_fifo_max,

    set_test_result,
    set_abnormal_status,

    set_all = 0xFF,
    
}athif_test_set_item_e ;


KAL_INT32 at_mtlte_hif_sdio_give_fw_own(void);


KAL_INT32 at_mtlte_hif_sdio_get_driver_own(void);

athif_test_param_t at_mtlte_hif_get_test_param(void);

KAL_INT32 at_mtlte_hif_set_test_param(athif_test_param_t set_param, athif_test_set_item_e set_item);

KAL_INT32 at_mtlte_hif_sdio_clear_tx_count(void);

KAL_INT32 at_mtlte_hif_sdio_get_tx_count(KAL_UINT32 *tx_count);

KAL_INT32 at_mtlte_hif_sdio_reset_abnormal_enable(KAL_INT32 abnormal_limit);

KAL_INT32 at_mtlte_hif_sdio_reset_abnormal_disable(void);


void at_transform_whisr_rx_tail( sdio_whisr_enhance *tail_ptr);

//EXPORT_SYMBOL(at_transform_whisr_rx_tail);


#endif

#endif


