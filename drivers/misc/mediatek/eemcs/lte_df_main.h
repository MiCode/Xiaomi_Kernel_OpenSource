#ifndef __LTE_DF_MAIN_H__ 
#define __LTE_DF_MAIN_H__

#include <linux/skbuff.h>
#include "eemcs_kal.h"


#define DISPATCH_AFTER_ALL_SKB_DONE  1  // Don't turn on these flag at same time

#define USE_QUE_WORK_DISPATCH_RX     0  // Don't turn on these flag at same time
#define USE_MULTI_QUE_DISPATCH       0

#define IMMEDIATE_RELOAD_DL_SKB      1  //move from makefile to solve compile error

#define FORMAL_DL_FLOW_CONTROL       1 
#define FORMAL_DL_FLOW_CONTROL_TEST  0 

#define BUFFER_POOL_FOR_EACH_QUE     1 
#define EXPT_HELP_WAKELOCK_FOR_UART  1


//#define DEV_MAX_PKT_SIZE		(65536) 
//#define DEV_MAX_PKT_SIZE		(4096)
 
/* Change PKT SIZE to 3584Byte due to AP skb allocate way & page size issue  */
#define DEV_MAX_PKT_SIZE		(3584)

/* TX Q Threshold */
#define MT_LTE_TX_SWQ_Q0_TH			(32)

#define MT_LTE_TX_SWQ_Q1_TH			(32)

#define MT_LTE_TX_SWQ_Q2_TH			(512) // Enalrge to 512 due to network usage

#define MT_LTE_TX_SWQ_Q3_TH			(512) // Enalrge to 512 due to network usage

#define MT_LTE_TX_SWQ_Q4_TH			(512) // Enalrge to 512 due to network usage

#define MT_LTE_TX_SWQ_Q5_TH			(32)

#define MT_LTE_TX_SWQ_Q6_TH			(32)


/* RX Q Threshold */
#define MT_LTE_RX_SWQ_Q0_TH			(128)

#define MT_LTE_RX_SWQ_Q1_TH			(256)

#define MT_LTE_RX_SWQ_Q2_TH			(256)

#define MT_LTE_RX_SWQ_Q3_TH			(256)

/* DL Buffer pool Threshold */
#define MT_LTE_DL_BUFF_POOL_TH			(512)

typedef enum{

/* TX PORT 1 */	
	TX_PORT1_Q_MIN = 0 ,
	TXQ_Q0 = 0 ,
	TXQ_Q1 = 1 ,
	TXQ_Q2 = 2 ,
	TXQ_Q3 = 3 ,
	TXQ_Q4 = 4 ,	
	TXQ_Q5 = 5 ,
	TXQ_Q6 = 6 ,
	TX_PORT1_Q_MAX = 6 ,
	
	TXQ_NUM ,

} MTLTE_DF_TX_QUEUE_TYPE ;

#define TXQ_PORT1_Q_NUM		(1 + TX_PORT1_Q_MAX - TX_PORT1_Q_MIN) // 7

typedef enum{
	RXQ_Q0 = 0,
	RXQ_Q1 = 1,
	RXQ_Q2 = 2 ,	
	RXQ_Q3 = 3 ,	
	RXQ_NUM ,
} MTLTE_DF_RX_QUEUE_TYPE ;

typedef int (*MTLTE_DF_TO_DEV_CALLBACK)(unsigned int data);

typedef int (*MTLTE_DF_TO_HIF_CALLBACK)(unsigned int data);

struct mtlte_df_to_hif_callback {	
	MTLTE_DF_TO_HIF_CALLBACK callback_func ;
	unsigned int private_data ;
} ;

struct mtlte_df_to_dev_callback {	
	MTLTE_DF_TO_DEV_CALLBACK callback_func ;
	unsigned int private_data ;
} ;

typedef struct {
  struct work_struct rxq_dispatch_work;
  MTLTE_DF_RX_QUEUE_TYPE    rxq_num;
} rxq_dispatch_work_t;


struct mtlte_df_core {
	
	struct mtlte_df_to_dev_callback cb_handle[RXQ_NUM] ;
    struct mtlte_df_to_dev_callback tx_cb_handle[TXQ_NUM] ;
    MTLTE_DF_TO_DEV_CALLBACK cb_sw_int;
    MTLTE_DF_TO_DEV_CALLBACK cb_wd_timeout;
	
	/* For Down Link path */	
#if BUFFER_POOL_FOR_EACH_QUE
	struct sk_buff_head dl_buffer_pool_queue[RXQ_NUM];
    KAL_UINT32 df_skb_alloc_size[RXQ_NUM];
    KAL_UINT32 df_buffer_pool_depth[RXQ_NUM];
#else
	struct sk_buff_head dl_buffer_pool_queue;
#endif
	struct workqueue_struct *dl_reload_work_queue;
	struct work_struct dl_reload_work;
	
	struct sk_buff_head dl_recv_wait_queue[RXQ_NUM];
	volatile int dl_pkt_in_use[RXQ_NUM];

#if USE_MULTI_QUE_DISPATCH
    struct workqueue_struct *rxq_dispatch_work_queue[RXQ_NUM];
    rxq_dispatch_work_t rxq_dispatch_work_param[RXQ_NUM];
#else
    struct workqueue_struct *dl_dispatch_work_queue;
	struct work_struct dl_dispatch_work;
#endif

	/* For Up Link path */	
	struct sk_buff_head ul_xmit_wait_queue[TXQ_NUM];

	struct sk_buff_head ul_xmit_free_queue;
	
	struct mtlte_df_to_hif_callback kick_hif_process ;

    /* For dl_pkt_in_use */    
    KAL_MUTEX   dl_pkt_lock ;

#if FORMAL_DL_FLOW_CONTROL
    /* for Down Link flow control */
    bool fl_ctrl_enable[RXQ_NUM];
    bool fl_ctrl_free_skb[RXQ_NUM];
    KAL_UINT32 fl_ctrl_limit[RXQ_NUM];
    KAL_UINT32 fl_ctrl_threshold[RXQ_NUM];
    bool fl_ctrl_full[RXQ_NUM];
    atomic_t fl_ctrl_counter[RXQ_NUM];
    KAL_UINT32 fl_ctrl_record[RXQ_NUM];
#endif    
    
} ;


int mtlte_df_init(void) ;

int mtlte_df_deinit(void) ;

int mtlte_df_probe(void) ;

//int mtlte_df_remove(void) ;

int mtlte_df_remove_phase1(void) ;
int mtlte_df_remove_phase2(void) ;





/******************************************************
*
*					API for HIF Layer 
*
******************************************************/
/******************************************************
*                   UL data traffic APIs    
******************************************************/
int mtlte_df_UL_pkt_in_swq(MTLTE_DF_TX_QUEUE_TYPE qno) ;

void mtlte_df_UL_callback(MTLTE_DF_TX_QUEUE_TYPE qno) ;
  
unsigned int mtlte_df_UL_deswq_buf(MTLTE_DF_TX_QUEUE_TYPE qno , void *buf_ptr) ;

/******************************************************
*					DL data traffic APIs	
******************************************************/
#if BUFFER_POOL_FOR_EACH_QUE
int mtlte_df_DL_pkt_in_buff_pool(MTLTE_DF_TX_QUEUE_TYPE qno);
#else
int mtlte_df_DL_pkt_in_buff_pool(void);
#endif

void mtlte_df_DL_try_reload_swq(void);

int mtlte_df_DL_pkt_in_swq(MTLTE_DF_RX_QUEUE_TYPE qno);

int mtlte_df_DL_enswq_buf(MTLTE_DF_RX_QUEUE_TYPE qno ,  void *buf, unsigned int len) ;

void mtlte_df_UL_swq_drop_skb(MTLTE_DF_TX_QUEUE_TYPE qno);

int mtlte_df_register_df_to_hif_callback(void *func_ptr , unsigned int data) ;

/******************************************************
*					Software Interrupt related APIs	
******************************************************/
int mtlte_df_swint_handle(unsigned int swint_status);

#if BUFFER_POOL_FOR_EACH_QUE
void  mtlte_df_DL_prepare_skb_for_swq_short(unsigned int target_num, MTLTE_DF_TX_QUEUE_TYPE qno);
#else
void  mtlte_df_DL_prepare_skb_for_swq_short(unsigned int target_num);
#endif

/******************************************************
*
*					API for Upper (Device) Layer 
*
******************************************************/

/******************************************************
*					UL data traffic APIs	
******************************************************/
int mtlte_df_UL_write_skb_to_swq(MTLTE_DF_TX_QUEUE_TYPE qno , struct sk_buff *skb) ;

int mtlte_df_UL_swq_space(MTLTE_DF_TX_QUEUE_TYPE qno) ;

int mtlte_df_register_tx_callback(MTLTE_DF_TX_QUEUE_TYPE qno, MTLTE_DF_TO_DEV_CALLBACK func_ptr , unsigned int private_data) ;

void mtlte_df_unregister_tx_callback(MTLTE_DF_TX_QUEUE_TYPE qno) ;

void mtlte_df_UL_SWQ_threshold_set(MTLTE_DF_TX_QUEUE_TYPE qno, unsigned int threshold) ;


/******************************************************
*					DL data traffic APIs	
******************************************************/
struct sk_buff * mtlte_df_DL_read_skb_from_swq(MTLTE_DF_RX_QUEUE_TYPE qno) ;

int mtlte_df_register_rx_callback(MTLTE_DF_RX_QUEUE_TYPE qno, MTLTE_DF_TO_DEV_CALLBACK func_ptr , unsigned int private_data) ;

void mtlte_df_unregister_rx_callback(MTLTE_DF_RX_QUEUE_TYPE qno) ;

int mtlte_df_DL_pkt_handle_complete(MTLTE_DF_RX_QUEUE_TYPE qno);

#if USE_QUE_WORK_DISPATCH_RX
  #if USE_MULTI_QUE_DISPATCH
void mtlte_df_DL_try_dispatch_rxque(MTLTE_DF_RX_QUEUE_TYPE qno);
  #else
void mtlte_df_DL_try_dispatch_rx(void);
  #endif
#endif  

#if DISPATCH_AFTER_ALL_SKB_DONE
void mtlte_df_DL_rx_callback(MTLTE_DF_RX_QUEUE_TYPE qno);
#endif

#if BUFFER_POOL_FOR_EACH_QUE
void mtlte_df_DL_set_skb_alloc_size_depth(MTLTE_DF_TX_QUEUE_TYPE qno, unsigned int alloc_size, unsigned int pool_depth);
#endif

/******************************************************
*					Software Interrupt related APIs	
******************************************************/
int mtlte_df_register_swint_callback(MTLTE_DF_TO_DEV_CALLBACK func_ptr);
int mtlte_df_unregister_swint_callback(void);



/******************************************************
*					Other APIs	
******************************************************/
 /* NOTICE : Only can be called at end of xboot !!!! */
int mtlte_hif_clean_txq_count(void);
int sdio_xboot_mb_wr(KAL_UINT32 *pBuffer, KAL_UINT32 len);
int sdio_xboot_mb_rd(KAL_UINT32 *pBuffer, KAL_UINT32 len);



/******************************************************
*			Device Watchdog Timeout related APIs	
******************************************************/
int mtlte_df_register_WDT_callback(MTLTE_DF_TO_DEV_CALLBACK func_ptr);
int mtlte_df_unregister_WDT_callback(void);
int mtlte_df_WDT_handle(int wd_handle_data);
void mtlte_enable_WDT_flow(void);


/******************************************************
*			Downlink flow control APIs	
******************************************************/
int mtlte_manual_turnoff_DL_port(MTLTE_DF_RX_QUEUE_TYPE qno);
int mtlte_manual_turnon_DL_port(MTLTE_DF_RX_QUEUE_TYPE qno);

#if FORMAL_DL_FLOW_CONTROL
void mtlte_df_Init_DL_flow_ctrl(MTLTE_DF_RX_QUEUE_TYPE qno, bool free_skb, unsigned int limit, unsigned int threshold);
void mtlte_df_DL_release_buff (MTLTE_DF_RX_QUEUE_TYPE qno, unsigned int buff_amount, struct sk_buff *skb);

 /* Below is only for internal usage */
bool mtlte_df_DL_check_fl_ctrl_enable(MTLTE_DF_RX_QUEUE_TYPE qno);
bool mtlte_df_DL_check_fl_ctrl_full(MTLTE_DF_RX_QUEUE_TYPE qno);
int mtlte_df_DL_read_fl_ctrl_record(MTLTE_DF_RX_QUEUE_TYPE qno);
#endif
#if FORMAL_DL_FLOW_CONTROL_TEST
void mtlte_df_DL_fl_ctrl_print_status(MTLTE_DF_RX_QUEUE_TYPE qno);
#endif

/******************************************************
*			Assertion Dump Flow Related	
******************************************************/

#define H2D_INT_except_ack         (0x1 << 18)
#define H2D_INT_except_clearQ_ack  (0x1 << 19)
#define H2D_INT_except_wakelock_ack  (0x1 << 20)

#define D2H_INT_except_init        (0x1 << 18)
#define D2H_INT_except_init_done   (0x1 << 19)
#define D2H_INT_except_clearQ_done (0x1 << 20)
#define D2H_INT_except_allQ_reset  (0x1 << 21)

#define D2H_INT_xboot_D2HMB               (1 << 16)
#define H2D_INT_xboot_H2DMB               (1 << 17)

#define D2H_INT_PLL_START               (1 << 22)
#define D2H_INT_PLL_END                 (1 << 23)

#define D2H_INT_except_wakelock         (1 << 24)


#define EX_INIT          (0)
#define EX_DHL_DL_RDY    (1)
#define EX_INIT_DONE     (2)


typedef void (*EEMCS_CCCI_EX_IND)(KAL_UINT32 msgid);

struct mtlte_expt_priv {
    
    EEMCS_CCCI_EX_IND  cb_ccci_expt_int;

    kal_uint32 non_stop_dlq[RXQ_NUM];
    
    kal_uint32 except_mode_ulq[TXQ_NUM];
    kal_uint32 except_mode_dlq[RXQ_NUM];
};


int mtlte_check_excetion_int(unsigned int swint_status);

int mtlte_expt_reset_inform_hif(void);

void mtlte_expt_q_num_init(KAL_UINT32 dhldl_q, KAL_UINT32 except_q);

int mtlte_expt_register_callback(EEMCS_CCCI_EX_IND cb);

int mtlte_expt_check_expt_q_num(KAL_UINT32 is_DL, KAL_UINT32 q_num);

int mtlte_expt_init(void);

int mtlte_expt_probe(void) ;

int mtlte_expt_remove(void) ;

int mtlte_expt_deinit(void);


#endif
