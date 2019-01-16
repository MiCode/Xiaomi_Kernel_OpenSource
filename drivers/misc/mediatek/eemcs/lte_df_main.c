#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/aee.h>

#include "lte_df_main.h"
#include "eemcs_kal.h"

static struct mtlte_df_core lte_df_core ;

#if TEST_DRV
extern unsigned int testing_rx_big_packet;
#endif

static int df_txq_buff_threshold[TXQ_NUM] ;
static int df_rxq_buff_threshold[RXQ_NUM] ;

static inline int mtlte_df_UL_kick_proccess(void)
{	
	KAL_DBGPRINT(KAL, DBG_INFO,("<====> %s\n",KAL_FUNC_NAME)) ;	
	
	if (lte_df_core.kick_hif_process.callback_func){
		return lte_df_core.kick_hif_process.callback_func(lte_df_core.kick_hif_process.private_data) ;
	}else{
		return KAL_FAIL ;
	}
	
}

int mtlte_df_register_df_to_hif_callback(void *func_ptr , unsigned int data)
{
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	
	lte_df_core.kick_hif_process.callback_func = func_ptr ;
	lte_df_core.kick_hif_process.private_data = data ;
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;

	return KAL_SUCCESS ; 
}

int mtlte_df_UL_swq_space(MTLTE_DF_TX_QUEUE_TYPE qno)
{
	int diff ;
	diff = df_txq_buff_threshold[qno] - lte_df_core.ul_xmit_wait_queue[qno].qlen ;
	return (diff>0)? diff : 0 ;
	
}

int mtlte_df_UL_pkt_in_swq(MTLTE_DF_TX_QUEUE_TYPE qno)
{
	return lte_df_core.ul_xmit_wait_queue[qno].qlen ;
}

void mtlte_df_UL_callback(MTLTE_DF_TX_QUEUE_TYPE qno)
{
	lte_df_core.tx_cb_handle[qno].callback_func(qno);
}

int mtlte_df_UL_write_skb_to_swq(MTLTE_DF_TX_QUEUE_TYPE qno , struct sk_buff *skb)
{
	int ret = KAL_SUCCESS ; 

	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s , qno: %d\r\n",KAL_FUNC_NAME, qno)) ;	
	
	if (skb){
		KAL_ASSERT(skb->len <= DEV_MAX_PKT_SIZE) ;
#if 0		
		if (lte_df_core.ul_xmit_wait_queue[qno].qlen > df_txq_buff_threshold[qno]){
			KAL_DBGPRINT(KAL, DBG_WARN,("qno %d is out of threshold\n",qno)) ;
			ret = -EBUSY ;
		}else{
			skb_queue_tail(&lte_df_core.ul_xmit_wait_queue[qno], skb) ;
		}	
#else
		skb_queue_tail(&lte_df_core.ul_xmit_wait_queue[qno], skb) ;
#endif
	}

	/* kick HIF tasking to handle the UL packets */
	mtlte_df_UL_kick_proccess() ;
	
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return ret ;
} 

unsigned int mtlte_df_UL_deswq_buf(MTLTE_DF_TX_QUEUE_TYPE qno , void *buf_ptr)
{
	unsigned int len = 0;
	struct sk_buff *skb ;
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s, qno: %d\n",KAL_FUNC_NAME, qno)) ;	

	skb = skb_dequeue(&(lte_df_core.ul_xmit_wait_queue[qno])) ; 
	if (skb){
		len = skb->len ;
		memcpy(buf_ptr, skb->data, len) ;				
		dev_kfree_skb(skb); 
	}

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return len ;
}

void mtlte_df_UL_swq_drop_skb(MTLTE_DF_TX_QUEUE_TYPE qno)
{

	struct sk_buff *skb ;
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s, qno: %d\n",KAL_FUNC_NAME, qno)) ;	

    while(mtlte_df_UL_pkt_in_swq(qno))
    {
        skb = skb_dequeue(&(lte_df_core.ul_xmit_wait_queue[qno])) ; 
	    if (skb){				
		    dev_kfree_skb(skb); 
	    }
    }

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;

}


int mtlte_df_DL_pkt_in_swq(MTLTE_DF_RX_QUEUE_TYPE qno)
{
	//return lte_df_core.dl_recv_wait_queue[qno].qlen ;
	return lte_df_core.dl_pkt_in_use[qno] ;
}

#if BUFFER_POOL_FOR_EACH_QUE
int mtlte_df_DL_pkt_in_buff_pool(MTLTE_DF_TX_QUEUE_TYPE qno)
{
	return lte_df_core.dl_buffer_pool_queue[qno].qlen ;
}
#else
int mtlte_df_DL_pkt_in_buff_pool()
{
	return lte_df_core.dl_buffer_pool_queue.qlen ;
}
#endif

/* DL data traffic APIs */
void mtlte_df_DL_try_reload_swq()
{	
	queue_work(lte_df_core.dl_reload_work_queue, &lte_df_core.dl_reload_work);	
}

/* DL data traffic APIs */
#if BUFFER_POOL_FOR_EACH_QUE
void inline mtlte_df_DL_prepare_skb_for_swq(MTLTE_DF_TX_QUEUE_TYPE qno)
{	
	struct sk_buff *skb_tmp ;
	unsigned cnt=0 ;

	while(lte_df_core.dl_buffer_pool_queue[qno].qlen<lte_df_core.df_buffer_pool_depth[qno]){
		skb_tmp = dev_alloc_skb(lte_df_core.df_skb_alloc_size[qno]) ;
		if (skb_tmp){
			cnt++ ;
			skb_queue_tail(&lte_df_core.dl_buffer_pool_queue[qno], skb_tmp) ;
		}else{
			break ;
		}
	}
	KAL_DBGPRINT(KAL, DBG_INFO,("Reload %d skbs for DL buffer pool, the pool len is %d\n",cnt, lte_df_core.dl_buffer_pool_queue[qno].qlen)) ;			
}
#else
void inline mtlte_df_DL_prepare_skb_for_swq(void)
{	
	struct sk_buff *skb_tmp ;
	unsigned cnt=0 ;
	
	while(lte_df_core.dl_buffer_pool_queue.qlen<MT_LTE_DL_BUFF_POOL_TH){
		skb_tmp = dev_alloc_skb(DEV_MAX_PKT_SIZE) ;
		if (skb_tmp){
			cnt++ ;
			skb_queue_tail(&lte_df_core.dl_buffer_pool_queue, skb_tmp) ;
		}else{
			break ;
		}
	}
	KAL_DBGPRINT(KAL, DBG_INFO,("Reload %d skbs for DL buffer pool, the pool len is %d\n",cnt, lte_df_core.dl_buffer_pool_queue.qlen)) ;			
}
#endif


#if USE_MULTI_QUE_DISPATCH

/* DL data traffic APIs */
void mtlte_df_DL_try_dispatch_rxque(MTLTE_DF_RX_QUEUE_TYPE qno)
{	
	queue_work(lte_df_core.rxq_dispatch_work_queue[qno], &lte_df_core.rxq_dispatch_work_param[qno].rxq_dispatch_work);	
}

/* DL data traffic APIs */
void inline mtlte_df_DL_dispatch_rxque_work(struct work_struct *work)
{	
    unsigned int rx_q_num ;
    rxq_dispatch_work_t *rxq_work_now;

    rxq_work_now = container_of(work, rxq_dispatch_work_t, rxq_dispatch_work);
    rx_q_num = rxq_work_now->rxq_num;
    
    KAL_DBGPRINT(KAL, DBG_INFO,("====> %s, rxq%d \n",KAL_FUNC_NAME, rx_q_num)) ;

        
    while(1){
        if( skb_queue_empty(&lte_df_core.dl_recv_wait_queue[rx_q_num]) ){
            break;
        }else{
            lte_df_core.cb_handle[rx_q_num].callback_func(rx_q_num) ;
        }
    }       
    
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
}

#else

/* DL data traffic APIs */
void mtlte_df_DL_try_dispatch_rx(void)
{	
	queue_work(lte_df_core.dl_dispatch_work_queue, &lte_df_core.dl_dispatch_work);	
}

/* DL data traffic APIs */
void inline mtlte_df_DL_dispatch_rx_work(void)
{	
    unsigned int rx_q_num = 0;
    //unsigned int pkt_in_qum = 0;
    
    KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;

    for(rx_q_num=0; rx_q_num<RXQ_NUM; rx_q_num++){ 
        
        while(1){
            //KAL_DBGPRINT(KAL, DBG_ERROR,("pkt in que %d = %d \n",rx_q_num, pkt_in_qum)) ;
            if( skb_queue_empty(&lte_df_core.dl_recv_wait_queue[rx_q_num]) ){
                break;
            }else{
                lte_df_core.cb_handle[rx_q_num].callback_func(rx_q_num) ;
            }
        }       
	}
    
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
}

static void mtlte_df_DL_dispatch_work(struct work_struct *work)
{	
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;		

	mtlte_df_DL_dispatch_rx_work(); 

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return ;

}

#endif

#if BUFFER_POOL_FOR_EACH_QUE
void  mtlte_df_DL_prepare_skb_for_swq_short(unsigned int target_num, MTLTE_DF_TX_QUEUE_TYPE qno)
{	
	struct sk_buff *skb_tmp ;
	unsigned cnt=0 ;
	
	while(lte_df_core.dl_buffer_pool_queue[qno].qlen < target_num){
		skb_tmp = dev_alloc_skb(lte_df_core.df_skb_alloc_size[qno]) ;
		if (skb_tmp){
			cnt++ ;
			skb_queue_tail(&lte_df_core.dl_buffer_pool_queue[qno], skb_tmp) ;
		}else{
			break ;
		}
	}
}
 
#else
void  mtlte_df_DL_prepare_skb_for_swq_short(unsigned int target_num)
{	
	struct sk_buff *skb_tmp ;
	unsigned cnt=0 ;
	
	while(lte_df_core.dl_buffer_pool_queue.qlen < target_num){
		skb_tmp = dev_alloc_skb(DEV_MAX_PKT_SIZE) ;
		if (skb_tmp){
			cnt++ ;
			skb_queue_tail(&lte_df_core.dl_buffer_pool_queue, skb_tmp) ;
		}else{
			break ;
		}
	}
}
 
#endif

#if BUFFER_POOL_FOR_EACH_QUE
void mtlte_df_DL_set_skb_alloc_size_depth(MTLTE_DF_TX_QUEUE_TYPE qno, unsigned int alloc_size, unsigned int pool_depth)
{
    if(alloc_size == 0){
        lte_df_core.df_skb_alloc_size[qno] = DEV_MAX_PKT_SIZE;
    }else if(alloc_size > DEV_MAX_PKT_SIZE){
        lte_df_core.df_skb_alloc_size[qno] = DEV_MAX_PKT_SIZE;
        KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO][WARN] Set skb allocate size to %d of DLQ%d is Invaild, which is bigger than DEV_MAX_PKT_SIZE %d \n", alloc_size, qno, DEV_MAX_PKT_SIZE)) ; 
    }else{
        lte_df_core.df_skb_alloc_size[qno] = alloc_size;
    }
    
    KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO][INFO] Set skb allocate size of DLQ%d = %d \n", qno, lte_df_core.df_skb_alloc_size[qno])) ; 


    if(pool_depth == 0){
        lte_df_core.df_buffer_pool_depth[qno] = MT_LTE_DL_BUFF_POOL_TH;
    }else{
        lte_df_core.df_buffer_pool_depth[qno] = pool_depth;
    }

    KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO][INFO] Set skb pool depth of DLQ%d = %d \n", qno, lte_df_core.df_buffer_pool_depth[qno])) ; 

}
#endif

static void mtlte_df_DL_reload_work(struct work_struct *work)
{	
#if BUFFER_POOL_FOR_EACH_QUE
    KAL_UINT32 qno;

	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;      

    for(qno=0; qno<RXQ_NUM; qno++){
	    mtlte_df_DL_prepare_skb_for_swq(qno);
    }

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
#else
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;		

	mtlte_df_DL_prepare_skb_for_swq(); 

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
#endif    
	return ;

}



int mtlte_df_DL_pkt_handle_complete(MTLTE_DF_RX_QUEUE_TYPE qno)
{
    
	KAL_MUTEXLOCK(&lte_df_core.dl_pkt_lock);
	lte_df_core.dl_pkt_in_use[qno] -- ; 
    KAL_MUTEXUNLOCK(&lte_df_core.dl_pkt_lock); 
    
	KAL_ASSERT(lte_df_core.dl_pkt_in_use[qno]>=0) ;
	return KAL_SUCCESS ;
}

/* DL data traffic APIs */
struct sk_buff * mtlte_df_DL_read_skb_from_swq(MTLTE_DF_RX_QUEUE_TYPE qno)
{	
	struct sk_buff * skb ;
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s , qno: %d\r\n",KAL_FUNC_NAME, qno)) ;		
    
	skb = skb_dequeue(&(lte_df_core.dl_recv_wait_queue[qno])) ; 

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return skb;

}

int mtlte_df_DL_enswq_buf(MTLTE_DF_RX_QUEUE_TYPE qno ,  void *buf, unsigned int len)
{
	int ret = KAL_SUCCESS ; 
	struct sk_buff *skb = NULL; 
	
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s , qno: %d\n",KAL_FUNC_NAME, qno)) ;	

#if BUFFER_POOL_FOR_EACH_QUE
    if(len > lte_df_core.df_skb_alloc_size[qno]){
        KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO][ERR] lte_df_core.df_skb_alloc_size[%d] = %d, packet this time = %d \n", qno, lte_df_core.df_skb_alloc_size[qno], len)) ;
        KAL_DBGPRINT(KAL, DBG_ERROR,("[SDIO][ERR] First 64byte of this error packet = ")) ;
        KAL_DBGPRINT(KAL, DBG_ERROR, ("0x%08x, 0x%08x, 0x%08x, 0x%08x,  ", *(unsigned int *)(buf+0), *(unsigned int *)(buf+4), *(unsigned int *)(buf+8), *(unsigned int *)(buf+12))) ;
        KAL_DBGPRINT(KAL, DBG_ERROR, ("0x%08x, 0x%08x, 0x%08x, 0x%08x,  ", *(unsigned int *)(buf+16), *(unsigned int *)(buf+20), *(unsigned int *)(buf+24), *(unsigned int *)(buf+28))) ;
        KAL_DBGPRINT(KAL, DBG_ERROR, ("0x%08x, 0x%08x, 0x%08x, 0x%08x,  ", *(unsigned int *)(buf+32), *(unsigned int *)(buf+36), *(unsigned int *)(buf+40), *(unsigned int *)(buf+44))) ;
        KAL_DBGPRINT(KAL, DBG_ERROR, ("0x%08x, 0x%08x, 0x%08x, 0x%08x,  ", *(unsigned int *)(buf+48), *(unsigned int *)(buf+52), *(unsigned int *)(buf+56), *(unsigned int *)(buf+60))) ;
    }
    KAL_ASSERT(len <= lte_df_core.df_skb_alloc_size[qno]) ;
#else
	KAL_ASSERT(len <= DEV_MAX_PKT_SIZE) ;
#endif

	if (lte_df_core.cb_handle[qno].callback_func == NULL){
		return KAL_SUCCESS ;
	}

#if BUFFER_POOL_FOR_EACH_QUE
    if ((skb = skb_dequeue(&lte_df_core.dl_buffer_pool_queue[qno])) == NULL ){
		KAL_DBGPRINT(KAL, DBG_WARN,("mtlte_df_DL_enswq_buf skb_dequeue no skb\n")) ;
		return KAL_FAIL ;
	}
#else
	if ((skb = skb_dequeue(&lte_df_core.dl_buffer_pool_queue)) == NULL ){
		KAL_DBGPRINT(KAL, DBG_WARN,("mtlte_df_DL_enswq_buf skb_dequeue no skb\n")) ;
		return KAL_FAIL ;
	}
#endif

	memcpy(skb_put(skb, len), buf, len) ;

    KAL_MUTEXLOCK(&lte_df_core.dl_pkt_lock);    
        
    //NOTICE : try to set in_use number before really enqueue skb, to avoid in_use number <0 assert
    lte_df_core.dl_pkt_in_use[qno] ++ ;

	skb_queue_tail(&lte_df_core.dl_recv_wait_queue[qno], skb) ;

    KAL_MUTEXUNLOCK(&lte_df_core.dl_pkt_lock);

#if FORMAL_DL_FLOW_CONTROL
    if(true == lte_df_core.fl_ctrl_enable[qno]){
        atomic_inc(&lte_df_core.fl_ctrl_counter[qno]);
        if(atomic_read(&lte_df_core.fl_ctrl_counter[qno]) >= lte_df_core.fl_ctrl_limit[qno]){
            lte_df_core.fl_ctrl_full[qno] = true;
        }
        // record the largest counter ever
        if(atomic_read(&lte_df_core.fl_ctrl_counter[qno]) > lte_df_core.fl_ctrl_record[qno]){
            lte_df_core.fl_ctrl_record[qno] = atomic_read(&lte_df_core.fl_ctrl_counter[qno]);
        }
    }
#endif

#if (USE_QUE_WORK_DISPATCH_RX || DISPATCH_AFTER_ALL_SKB_DONE)   
#else
	KAL_DBGPRINT(KAL, DBG_INFO,("RXQ %d callback , and the private data is %d\r\n",qno, lte_df_core.cb_handle[qno].private_data)) ;	
	//lte_df_core.cb_handle[qno].callback_func(lte_df_core.cb_handle[qno].private_data) ;
	lte_df_core.cb_handle[qno].callback_func(qno) ;
#endif		

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return ret ;
}

#if DISPATCH_AFTER_ALL_SKB_DONE
/* DL data traffic APIs */
void mtlte_df_DL_rx_callback(MTLTE_DF_RX_QUEUE_TYPE qno)
{	
    KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	
	lte_df_core.cb_handle[qno].callback_func(qno) ;
    KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
}
#endif

int mtlte_df_register_rx_callback(MTLTE_DF_RX_QUEUE_TYPE qno, MTLTE_DF_TO_DEV_CALLBACK func_ptr , unsigned int private_data) 
{
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	

	KAL_ASSERT(qno < RXQ_NUM) ;

	lte_df_core.cb_handle[qno].callback_func = func_ptr ;
	lte_df_core.cb_handle[qno].private_data= private_data ;
	
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return KAL_SUCCESS ; 
}

void mtlte_df_unregister_rx_callback(MTLTE_DF_RX_QUEUE_TYPE qno) 
{
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	

	KAL_ASSERT(qno < RXQ_NUM) ;

	lte_df_core.cb_handle[qno].callback_func = NULL ;
	lte_df_core.cb_handle[qno].private_data= 0 ;
	
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
}

int mtlte_df_register_tx_callback(MTLTE_DF_TX_QUEUE_TYPE qno, MTLTE_DF_TO_DEV_CALLBACK func_ptr , unsigned int private_data) 
{
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	

	KAL_ASSERT(qno < TXQ_NUM) ;

	lte_df_core.tx_cb_handle[qno].callback_func = func_ptr ;
	lte_df_core.tx_cb_handle[qno].private_data= private_data ;
	
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return KAL_SUCCESS ; 
}

void mtlte_df_unregister_tx_callback(MTLTE_DF_TX_QUEUE_TYPE qno) 
{
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	

	KAL_ASSERT(qno < TXQ_NUM) ;

	lte_df_core.tx_cb_handle[qno].callback_func = NULL ;
	lte_df_core.tx_cb_handle[qno].private_data= 0 ;
	
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
}

int mtlte_df_register_swint_callback(MTLTE_DF_TO_DEV_CALLBACK func_ptr) 
{
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;
    
	lte_df_core.cb_sw_int = func_ptr ;
	
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return KAL_SUCCESS ; 
}

int mtlte_df_unregister_swint_callback(void)
{
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;

    lte_df_core.cb_sw_int = NULL;
    
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
    return KAL_SUCCESS;
}


int mtlte_df_swint_handle(unsigned int swint_status)
{
    mtlte_check_excetion_int(swint_status);
    
    if(swint_status & 0xFF030000){
        if(lte_df_core.cb_sw_int == NULL){
            KAL_DBGPRINT(KAL, DBG_ERROR,("the sw interrupt callback func has no be registed!! \n")) ;
            return KAL_FAIL ; 
        }
        else{
            lte_df_core.cb_sw_int(swint_status);
        }
    }
    return KAL_SUCCESS ; 
}

int mtlte_df_register_WDT_callback(MTLTE_DF_TO_DEV_CALLBACK func_ptr) 
{
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;
    
	lte_df_core.cb_wd_timeout = func_ptr ;
	
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
	return KAL_SUCCESS ; 
}

int mtlte_df_unregister_WDT_callback(void)
{
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;

    lte_df_core.cb_wd_timeout = NULL;
    
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;
    return KAL_SUCCESS;
}


int mtlte_df_WDT_handle(int wd_handle_data)
{
    if(lte_df_core.cb_wd_timeout == NULL){
        KAL_DBGPRINT(KAL, DBG_ERROR,("the Watchdog Timeout callback func has no be registed!! \n")) ;
        return KAL_FAIL ; 
    }
    else{
        KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] the Watchdog Timeout of LTE occur!! \n")) ;
        lte_df_core.cb_wd_timeout(wd_handle_data);
    }
    return KAL_SUCCESS ; 
}


#if FORMAL_DL_FLOW_CONTROL
void mtlte_df_Init_DL_flow_ctrl(MTLTE_DF_RX_QUEUE_TYPE qno, bool free_skb, unsigned int limit, unsigned int threshold)
{
    if(limit)
    {
        lte_df_core.fl_ctrl_enable[qno] = true;
        lte_df_core.fl_ctrl_free_skb[qno] = free_skb;
        lte_df_core.fl_ctrl_limit[qno] = limit;
        lte_df_core.fl_ctrl_threshold[qno] = threshold;
        lte_df_core.fl_ctrl_full[qno] = false;
        atomic_set(&lte_df_core.fl_ctrl_counter[qno], 0);
    }
    else
    {
        lte_df_core.fl_ctrl_enable[qno] = false;
    }
}

void mtlte_df_DL_release_buff (MTLTE_DF_RX_QUEUE_TYPE qno, unsigned int buff_amount, struct sk_buff *skb)
{
    if(true == lte_df_core.fl_ctrl_enable[qno])
    {
        if(true == lte_df_core.fl_ctrl_free_skb[qno])
        {
            buff_amount = 1;
			dev_kfree_skb_any(skb); 
        }

        atomic_sub(buff_amount, &lte_df_core.fl_ctrl_counter[qno]);
        if((true == lte_df_core.fl_ctrl_full[qno]) && (atomic_read(&lte_df_core.fl_ctrl_counter[qno]) <= lte_df_core.fl_ctrl_threshold[qno]))
        {
            lte_df_core.fl_ctrl_full[qno] = false;
            // kick proccess up to re-receive DL packet; 
            mtlte_df_UL_kick_proccess();
        }
    }
    else
    {
        //  red screen - kernel api warning
         char error_srt[64] = {0};
         sprintf(error_srt, "DL_release_buff is called without Init");
         aee_kernel_warning("[EEMCS] Use flow ctrl API without Init", error_srt);
    }
}


bool mtlte_df_DL_check_fl_ctrl_enable(MTLTE_DF_RX_QUEUE_TYPE qno)
{
    return lte_df_core.fl_ctrl_enable[qno];
}

bool mtlte_df_DL_check_fl_ctrl_full(MTLTE_DF_RX_QUEUE_TYPE qno)
{
    return lte_df_core.fl_ctrl_full[qno];
}

int mtlte_df_DL_read_fl_ctrl_record(MTLTE_DF_RX_QUEUE_TYPE qno)
{
    return lte_df_core.fl_ctrl_record[qno];
}
#endif

#if FORMAL_DL_FLOW_CONTROL_TEST
void mtlte_df_DL_fl_ctrl_print_status(MTLTE_DF_RX_QUEUE_TYPE qno)
{
    KAL_DBGPRINT(KAL, DBG_ERROR,("====> status of DLQ%d \n",qno)) ;
    KAL_DBGPRINT(KAL, DBG_ERROR,("      fl_ctrl_enable = %d \n",lte_df_core.fl_ctrl_enable[qno])) ;
    KAL_DBGPRINT(KAL, DBG_ERROR,("      fl_ctrl_free_skb = %d \n",lte_df_core.fl_ctrl_free_skb[qno])) ;
    KAL_DBGPRINT(KAL, DBG_ERROR,("      fl_ctrl_limit = %d \n",lte_df_core.fl_ctrl_limit[qno])) ;
    KAL_DBGPRINT(KAL, DBG_ERROR,("      fl_ctrl_threshold = %d \n",lte_df_core.fl_ctrl_threshold[qno])) ;
    KAL_DBGPRINT(KAL, DBG_ERROR,("      fl_ctrl_full = %d \n",lte_df_core.fl_ctrl_full[qno])) ;
    KAL_DBGPRINT(KAL, DBG_ERROR,("      fl_ctrl_counter = %d \n", atomic_read(&lte_df_core.fl_ctrl_counter[qno]))) ;
    KAL_DBGPRINT(KAL, DBG_ERROR,("      fl_ctrl_record = %d \n",lte_df_core.fl_ctrl_record[qno])) ;
}
#endif


void mtlte_df_buff_threshold_default_setting(void)				
{	
	/* TXQ_Q0 */
	df_txq_buff_threshold[TXQ_Q0] = MT_LTE_TX_SWQ_Q0_TH ;

	/* TXQ_Q1 */
	df_txq_buff_threshold[TXQ_Q1] = MT_LTE_TX_SWQ_Q1_TH ;
	
	/* TXQ_Q2 */
	df_txq_buff_threshold[TXQ_Q2] = MT_LTE_TX_SWQ_Q2_TH ;

	/* TXQ_Q3 */
	df_txq_buff_threshold[TXQ_Q3] = MT_LTE_TX_SWQ_Q3_TH ;

	/* TXQ_Q4 */
	df_txq_buff_threshold[TXQ_Q4] = MT_LTE_TX_SWQ_Q4_TH ;

    /* TXQ_Q5 */
	df_txq_buff_threshold[TXQ_Q5] = MT_LTE_TX_SWQ_Q5_TH ;

    /* TXQ_Q6 */
	df_txq_buff_threshold[TXQ_Q6] = MT_LTE_TX_SWQ_Q6_TH ;

	/* RXQ_Q0 */
	df_rxq_buff_threshold[RXQ_Q0] = MT_LTE_RX_SWQ_Q0_TH;

	/* RXQ_Q1 */
	df_rxq_buff_threshold[RXQ_Q1] = MT_LTE_RX_SWQ_Q1_TH;

	/* RXQ_Q2 */
	df_rxq_buff_threshold[RXQ_Q2] = MT_LTE_RX_SWQ_Q2_TH;

	/* RXQ_Q3 */
	df_rxq_buff_threshold[RXQ_Q3] = MT_LTE_RX_SWQ_Q3_TH;

}


void mtlte_df_UL_SWQ_threshold_set(MTLTE_DF_TX_QUEUE_TYPE qno, unsigned int threshold)
{
    df_txq_buff_threshold[qno] = threshold;
}

int mtlte_df_init(void)
{
#if (FORMAL_DL_FLOW_CONTROL || BUFFER_POOL_FOR_EACH_QUE)
    unsigned int i;
#endif
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	
	
	mtlte_df_buff_threshold_default_setting() ;
    lte_df_core.cb_sw_int = NULL;
    lte_df_core.cb_wd_timeout = NULL;

#if FORMAL_DL_FLOW_CONTROL
    for (i=0; i<RXQ_NUM ; i++){
        lte_df_core.fl_ctrl_enable[i] = false;
        lte_df_core.fl_ctrl_record[i] = 0;
    }
#endif

    lte_df_core.dl_reload_work_queue = create_singlethread_workqueue("df_dl_reload_work");
	if (!lte_df_core.dl_reload_work_queue) {
		KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] create the dafa flow layer DL reload work fail!!\n")) ;
		return -ENOMEM;
	}
	INIT_WORK(&lte_df_core.dl_reload_work, mtlte_df_DL_reload_work);

#if	USE_MULTI_QUE_DISPATCH
    for (i=0; i<RXQ_NUM ; i++){
        sprintf(rxq_work_name[i], "rxq%d_dispatch_work", i);
        
        lte_df_core.rxq_dispatch_work_queue[i] = create_singlethread_workqueue(rxq_work_name[i]);
	    if (!lte_df_core.rxq_dispatch_work_queue[i]) {
		    KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] create the dafa flow layer rxq%d dispatch work fail!!\n", i)) ;
		    return -ENOMEM;
	    }
	    INIT_WORK(&lte_df_core.rxq_dispatch_work_param[i].rxq_dispatch_work, mtlte_df_DL_dispatch_rxque_work);
        lte_df_core.rxq_dispatch_work_param[i].rxq_num = i;
    }
#else
    lte_df_core.dl_dispatch_work_queue = create_singlethread_workqueue("df_dl_dispatch_work");
	if (!lte_df_core.dl_dispatch_work_queue) {
		KAL_DBGPRINT(KAL, DBG_ERROR,("[ERR] create the dafa flow layer DL dispatch work fail!!\n")) ;
		return -ENOMEM;
	}
	INIT_WORK(&lte_df_core.dl_dispatch_work, mtlte_df_DL_dispatch_work);
#endif

#if BUFFER_POOL_FOR_EACH_QUE
    for (i=0; i<RXQ_NUM ; i++){
        lte_df_core.df_skb_alloc_size[i] = DEV_MAX_PKT_SIZE;
        lte_df_core.df_buffer_pool_depth[i] = MT_LTE_DL_BUFF_POOL_TH;
    }
#endif

    KAL_AQUIREMUTEX(&lte_df_core.dl_pkt_lock) ;

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;

	return KAL_SUCCESS ; 
}

int mtlte_df_probe(void)
{
	unsigned int i ;
#if	USE_MULTI_QUE_DISPATCH    
    char rxq_work_name[RXQ_NUM][50];
#endif

	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	

	for (i=0; i<TXQ_NUM ; i++){
		skb_queue_head_init(&lte_df_core.ul_xmit_wait_queue[i]);
	}

	skb_queue_head_init(&lte_df_core.ul_xmit_free_queue);
	
	for (i=0; i<RXQ_NUM ; i++){
		lte_df_core.dl_pkt_in_use[i] = 0 ;
		skb_queue_head_init(&lte_df_core.dl_recv_wait_queue[i]);
	}

#if BUFFER_POOL_FOR_EACH_QUE
    for (i=0; i<RXQ_NUM ; i++){
        skb_queue_head_init(&lte_df_core.dl_buffer_pool_queue[i]);
        mtlte_df_DL_prepare_skb_for_swq(i) ;
        KAL_RAWPRINT(("The DL Buffer Pool packet of RXQ%d is %d\r\n", i, lte_df_core.dl_buffer_pool_queue[i].qlen)) ;
        KAL_RAWPRINT(("The skb size of RXQ%d is %d\r\n", i, lte_df_core.df_skb_alloc_size[i])) ;
    }
#else
    skb_queue_head_init(&lte_df_core.dl_buffer_pool_queue);
	mtlte_df_DL_prepare_skb_for_swq() ;
    KAL_RAWPRINT(("The DL Buffer Pool packet is %d\r\n", lte_df_core.dl_buffer_pool_queue.qlen)) ;
#endif

#if FORMAL_DL_FLOW_CONTROL
    for (i=0; i<RXQ_NUM ; i++){
        lte_df_core.fl_ctrl_full[i] = false;
        atomic_set(&lte_df_core.fl_ctrl_counter[i], 0);
    }
#endif

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;

	return KAL_SUCCESS ; 
}


int mtlte_df_remove_phase1(void)
{	
#if	USE_MULTI_QUE_DISPATCH
	int i ;
#endif

	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	

#if	USE_MULTI_QUE_DISPATCH
    for (i=0; i<RXQ_NUM ; i++){
        flush_workqueue(lte_df_core.rxq_dispatch_work_queue[i]);
    }
#else
    flush_workqueue(lte_df_core.dl_dispatch_work_queue);
#endif

	flush_workqueue(lte_df_core.dl_reload_work_queue);

	KAL_RAWPRINT(("[REMOVE] All queue work is flushed ~~ \r\n")) ;

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;

	return 0 ; 
}


int mtlte_df_remove_phase2(void)
{	
	int i ;
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	
    
	for (i=0; i<TXQ_NUM ; i++){
		skb_queue_purge(&lte_df_core.ul_xmit_wait_queue[i]);
	}

	skb_queue_purge(&lte_df_core.ul_xmit_free_queue);
	
	for (i=0; i<RXQ_NUM ; i++){
		// already performed at probe, no need to do again.
		//lte_df_core.dl_pkt_in_use[i] = 0 ;
		skb_queue_purge(&lte_df_core.dl_recv_wait_queue[i]);		
	}
#if BUFFER_POOL_FOR_EACH_QUE
    for (i=0; i<RXQ_NUM ; i++){
        skb_queue_purge(&lte_df_core.dl_buffer_pool_queue[i]);	
    }
#else
	skb_queue_purge(&lte_df_core.dl_buffer_pool_queue);		
#endif

	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;

	return 0 ; 
}

int mtlte_df_deinit(void)
{
	KAL_DBGPRINT(KAL, DBG_INFO,("====> %s\n",KAL_FUNC_NAME)) ;	

#if	USE_MULTI_QUE_DISPATCH
    for (i=0; i<RXQ_NUM ; i++){
	    destroy_workqueue(lte_df_core.rxq_dispatch_work_queue[i]);
    }
#else
	destroy_workqueue(lte_df_core.dl_dispatch_work_queue);
#endif

    destroy_workqueue(lte_df_core.dl_reload_work_queue);

    KAL_DESTROYMUTEX(&lte_df_core.dl_pkt_lock) ;
	
	KAL_DBGPRINT(KAL, DBG_INFO,("<==== %s\n",KAL_FUNC_NAME)) ;

	return KAL_SUCCESS ; 
}

