#ifndef __EEMCS_EXPT_UT_H__
#define __EEMCS_EXPT_UT_H__

#include <asm/atomic.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/kfifo.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>

#include "eemcs_kal.h"
#include "eemcs_ccci.h"

#include "lte_df_main.h"


#ifdef _EEMCS_EXCEPTION_UT

typedef struct EEMCS_EXPT_UT_st {
    //struct sk_buff_head tx_queue[SDIO_TX_Q_NUM];
    //struct sk_buff_head rx_queue[SDIO_RX_Q_NUM];
    //atomic_t tx_queue_drp[SDIO_TX_Q_NUM];
    //atomic_t rx_queue_drp[SDIO_RX_Q_NUM];
    //struct task_struct *lb_thread;
    //struct kfifo lb_qno_fifo;
    spinlock_t kfifo_lock;
    spinlock_t lb_lock;
    struct semaphore lb_sem;
    //wait_queue_head_t lb_thread_wq;
    //atomic_t lb_thread_cond;
    volatile KAL_UINT32 enable_rx_lb;
    volatile KAL_UINT32 enable_ccci_lb;
    MTLTE_DF_TO_DEV_CALLBACK ccci_rx_cb[SDIO_RX_Q_NUM];
    EEMCS_CCCI_EX_IND ccci_expt_cb;
    volatile KAL_UINT32 expt_id;                        // exception mode id
    KAL_UINT32 nonstop_q;
    KAL_UINT32 expt_rxq;
    KAL_UINT32 expt_txq;
} EEMCS_EXPT_UT_SET;


//#include "eemcs_expt_ut_api.h"

/* For sysfs operation */
EEMCS_EXPT_UT_SET* eemcs_expt_get_inst(void);
void eemcs_expt_ut_trigger(unsigned int msg_id);
//void eemcs_expt_enable_ut_rx_lb(KAL_UINT32 enable);
//ssize_t eemcs_expt_sysfs_show_ut_rx_lb(char *);

void eemcs_expt_enable_ut_ccci_lb(KAL_UINT32 enable);
ssize_t eemcs_expt_sysfs_show_ut_ccci_lb(char *);

ssize_t eemcs_expt_ut_show_statistics(char *);

#endif // _EEMCS_EXCEPTION_UT

KAL_INT32 eemcs_expt_ut_init(void);
void eemcs_expt_ut_exit(void);

#endif // __EEMCS_EXPT_UT_H__
