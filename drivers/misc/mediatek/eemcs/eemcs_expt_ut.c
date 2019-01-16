#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/kfifo.h>
#include <linux/semaphore.h>

#include "eemcs_ccci.h"
#include "eemcs_debug.h"
#include "eemcs_boot.h"
#include "eemcs_state.h"
#include "eemcs_boot.h"
#include "eemcs_expt_ut.h"

#include "lte_df_main.h"

/*
 * Exception UT How To
 *
 * 1. Enable _EEMCS_EXCEPTION_UT in driver then build eemcs.ko
 * 2. "insmod eemcs.ko" on target
 * 3. launch emcs_vad on target
 * 4. use emcs_vacmd to configure test configuration of VA
 * 5. launch "emcs_vacmd start" to start VA
 * 6. while VA is testing, launch
 *        "echo init > /sys/class/eemcs/eemcs/expt_mode"
 *    could change drive state to EEMCS_EXCEPTION state and EEMCS_EX_INIT mode
 * 7. launch
 *        "echo dhl_ready > /sys/clas/eemcs/eemcs/expt_mode"
 *    to change driver state to EEMCS_EXCEPTION state and EEMCS_EX_DHL_DL_RDY mode
 * 8. launch
 *        "echo init_done > /sys/class/eemcs/eemcs/expt_mode"
 *    to change driver state to EEMCS_EXCEPTION state and EEMCS_EX_INIT_DONE mode
 */


#ifdef _EEMCS_EXCEPTION_UT
EEMCS_EXPT_UT_SET g_expt_ut_inst; 


//static kal_bool is_exception_mode(KAL_UINT32 *mode);

#if 0
void exception_ul_lb_channel(struct sk_buff *skb, KAL_UINT32 *rx_qno)
{
    CCCI_BUFF_T *pccci_h;
    KAL_UINT32 port_id, tx_ch, rx_ch, rx_q;

    pccci_h = (CCCI_BUFF_T *)skb->data;
    port_id = ccci_ch_to_port(pccci_h->channel);
    tx_ch = pccci_h->channel;
    rx_ch = ccci_get_rx_ch(port_id);
    pccci_h->channel = rx_ch;
    if (is_exception_mode(NULL))
        rx_q = SDIO_RXQ(ccci_get_expt_rxq(port_id));
    else
        rx_q = SDIO_RXQ(ccci_get_rxq(port_id));
    if (rx_qno != NULL)
        *rx_qno = rx_q;

    DBGLOG(EXPT, DBG, "[EXPT_UT] === PORT(%d) tx_ch(%d) LB to rx_ch(%d) in Rx_Q(%d) ===",
        port_id, tx_ch, rx_ch, rx_q);
}
#endif
EEMCS_EXPT_UT_SET* eemcs_expt_get_inst(void)
{
    return &g_expt_ut_inst;
}

void eemcs_expt_ut_trigger(unsigned int msg_id)
{
//    KAL_UINT32 i = 0;
#if 0
    KAL_ASSERT(msg_id >= EX_INIT && msg_id <= EX_INIT_DONE);
    if (down_interruptible(&g_expt_ut_inst.lb_sem)) {
        DBGLOG(EXPT, WAR, "[EXPT] eemcs_expt_ut_trigger() is interruptibled !!");
    } else {
        g_expt_ut_inst.expt_id = msg_id;
        up(&g_expt_ut_inst.lb_sem);
        /* callback to CCCI layer */
        if (g_expt_ut_inst.ccci_expt_cb != NULL)
            g_expt_ut_inst.ccci_expt_cb(msg_id);
    }
#else
    DBGLOG(EXPT, DBG, "[EXPT UT] eemcs_expt_ut_trigger(), msg_id(%d) !!", msg_id);
    g_expt_ut_inst.expt_id = msg_id;
    DBGLOG(EXPT, DBG, "[EXPT UT] dump g_expt_ut_inst, msg_id(%d) ccci_expt_cb(%x)!!", g_expt_ut_inst.expt_id, g_expt_ut_inst.ccci_expt_cb);
    /* callback to CCCI layer */
    if (g_expt_ut_inst.ccci_expt_cb != NULL)
        g_expt_ut_inst.ccci_expt_cb(msg_id);
    else
        DBGLOG(EXPT, DBG, "[EXPT UT] No exception callback function registered!! ");
#endif
}

/*static KAL_UINT32 get_exception_mode(void)
{
#if 0    
    KAL_UINT32 id = -1;
    if (down_interruptible(&g_expt_ut_inst.lb_sem)) {
        DBGLOG(EXPT, WAR, "[EXPT] get_exception_mode() is interruptibled !!");
        return -1;
    }
    id = g_expt_ut_inst.expt_id;
    up(&g_expt_ut_inst.lb_sem);
    return id;
#else
    KAL_UINT32 id = -1;
    id = g_expt_ut_inst.expt_id;
    return id;
#endif
}
*/
#if 0
static kal_bool is_exception_mode(KAL_UINT32 *mode)
{
    kal_bool ret = false;
    KAL_UINT32 cur_mode = -1;

    /* not in exception mode */
    if ((cur_mode = get_exception_mode()) == -1) {
        if (mode != NULL)
            *mode = -1;
    /* in exception mode */
    } else {
        if (mode != NULL)
            *mode = cur_mode;
        ret = true;
    }
    return ret;
}

kal_bool is_valid_txq(MTLTE_DF_TX_QUEUE_TYPE qno, struct sk_buff *skb)
{
    kal_bool valid = true;
    KAL_UINT32 mode = -1;
    CCCI_BUFF_T *ccci_h = NULL;
    ccci_h = (CCCI_BUFF_T*)skb->data;

    do {
        /* not in exception mode */
        if (!is_exception_mode(&mode))
            break;
        /* all pass */
        if (ccci_h->channel == CH_MLOG_TX)
            break;
        /* valid Tx Q in exception mode */
        if (mode == EX_INIT_DONE) {
            if (test_bit(qno, (unsigned long *)&g_expt_ut_inst.expt_txq))
                break;
        }
        valid = false;
    } while(0);

    return valid;
}

kal_bool is_valid_rxq(MTLTE_DF_RX_QUEUE_TYPE qno, struct sk_buff *skb)
{
    kal_bool valid = true;
    KAL_UINT32 mode = -1;
    CCCI_BUFF_T *ccci_h = NULL;
    ccci_h = (CCCI_BUFF_T*)skb->data;

    do {
        /* not in exception mode */
        if (!is_exception_mode(&mode))
            break;
        /* all pass */
        if (ccci_h->channel == CH_MLOG_RX)
            break;
        /* valid Tx Q in exception mode */
        if (mode == EX_INIT_DONE) {
            if (test_bit(qno, (unsigned long *)&g_expt_ut_inst.expt_rxq))
                break;
        }
        valid = false;
    } while(0);

    return valid;
}
#endif
/* UL APIs */
#if 0
int exception_ut_UL_write_skb_to_swq(MTLTE_DF_TX_QUEUE_TYPE qno , struct sk_buff *skb)
{
    KAL_INT32 ret = KAL_SUCCESS;
    kal_bool go = false;
    KAL_UINT32 mode = -1;
    DEBUG_LOG_FUNCTION_ENTRY;

    if (NULL != skb) {
//#if defined(_EEMCS_XBOOT_SUPPORT) && defined(_EEMCS_BOOT_UT)
//        if (!eemcs_device_ready()) {
//            ccci_boot_write_desc_to_q(CH_CTRL_TX, skb);
//            return KAL_SUCCESS;
//        } else
//#endif // _EEMCS_XBOOT_SUPPORT && _EEMCS_BOOT_UT
        {
            if (is_exception_mode(&mode)) {
                go = is_valid_txq(qno, skb);
            } else {
                go = true;
            }

            if (go) {
                skb_queue_tail(&g_expt_ut_inst.tx_queue[qno], skb);
                DBGLOG(EXPT, DBG, "[EXPT_UT] Add a skb(0x%p) to Tx queue(%d)", skb, qno);
                /* Loopback to Rx queue or not */
                if (g_expt_ut_inst.enable_rx_lb) {
_retry:
                    // Add skb to UT Rx Q only not in exception mode
                    if (kfifo_in_spinlocked(&g_expt_ut_inst.lb_qno_fifo, &qno, sizeof(KAL_UINT32), &g_expt_ut_inst.kfifo_lock) != sizeof(KAL_UINT32)) {
                        //DBGLOG(EXPT, WAR, "[EXPT_UT] kfifo_in() ERROR, retry !!");
                        goto _retry;
                    } else {
                        atomic_set(&g_expt_ut_inst.lb_thread_cond, 1);
                        wake_up(&g_expt_ut_inst.lb_thread_wq);
                    }
                }
            } else {
                /*
                 * CCCI layer doesn't handle KAL_FAIL, so we free skb here
                 */
                dev_kfree_skb(skb);
                DBGLOG(EXPT, WAR, "[EXPT] exception_ut_UL_write_skb_to_swq(%d) PKT DROPPED !!", qno);
                ret = KAL_FAIL;
            }
//            up(&g_expt_ut_inst.lb_sem);
        }
    } else {
        DBGLOG(EXPT, DBG, "[EXPT_UT] fake kick DF !!!");
    }
    DEBUG_LOG_FUNCTION_LEAVE;
    return ret;
}

int exception_ut_UL_swq_space(MTLTE_DF_TX_QUEUE_TYPE qno)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(EXPT, DBG, "[EXPT_UT] queue(%d) Query Tx SZ 99",qno);
    DEBUG_LOG_FUNCTION_LEAVE;
    return 99;
}

int exception_ut_UL_pkt_in_swq(MTLTE_DF_TX_QUEUE_TYPE qno)
{
    int no = 0;
    DEBUG_LOG_FUNCTION_ENTRY;

    no = skb_queue_len(&g_expt_ut_inst.tx_queue[qno]);

    DEBUG_LOG_FUNCTION_LEAVE;
    return no;
}

struct sk_buff * exception_ut_UL_read_skb_from_swq(MTLTE_DF_TX_QUEUE_TYPE qno)
{
    struct sk_buff *skb = NULL;
    DEBUG_LOG_FUNCTION_ENTRY;

    skb = skb_dequeue(&g_expt_ut_inst.tx_queue[qno]);

    DEBUG_LOG_FUNCTION_LEAVE;
    return skb;
}

/* DL APIs */
int exception_ut_DL_pkt_in_swq(MTLTE_DF_RX_QUEUE_TYPE qno)
{
    unsigned int no = 0;
    DEBUG_LOG_FUNCTION_ENTRY;

    no = skb_queue_len(&g_expt_ut_inst.rx_queue[qno]);

    DEBUG_LOG_FUNCTION_LEAVE;
    return no;
}

struct sk_buff * exception_ut_DL_read_skb_from_swq(MTLTE_DF_RX_QUEUE_TYPE qno)
{
    struct sk_buff *skb = NULL;
    DEBUG_LOG_FUNCTION_ENTRY;

    skb = skb_dequeue(&g_expt_ut_inst.rx_queue[qno]);

    DEBUG_LOG_FUNCTION_LEAVE;
    return skb;
}

int exception_ut_DL_pkt_handle_complete(MTLTE_DF_RX_QUEUE_TYPE qno)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

int exception_ut_register_rx_callback(MTLTE_DF_RX_QUEUE_TYPE qno, MTLTE_DF_TO_DEV_CALLBACK func_ptr , unsigned int private_data)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    KAL_ASSERT(qno >= CCCI_RXQ_TO_DF(RX_Q_MIN) && qno < CCCI_RXQ_TO_DF(RX_Q_MAX));
    DBGLOG(EXPT, DBG, "[EXPT_UT] Register Q(%d) Rx callback", qno);
    g_expt_ut_inst.ccci_rx_cb[qno] = func_ptr;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

void exception_ut_unregister_rx_callback(MTLTE_DF_RX_QUEUE_TYPE qno)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DBGLOG(EXPT, DBG, "[EXPT_UT] Unregister Q(%d) callback function", qno);
    DEBUG_LOG_FUNCTION_LEAVE;
}

int exception_ut_register_swint_callback(MTLTE_DF_TO_DEV_CALLBACK func_ptr)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

int exception_ut_unregister_swint_callback()
{
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

int exception_ut_exception_init(KAL_UINT32 nonstop_q, KAL_UINT32 except_q)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    g_expt_ut_inst.nonstop_q = nonstop_q;
    g_expt_ut_inst.expt_txq = (0xFFFF0000 & except_q) >> 16;
    g_expt_ut_inst.expt_rxq = 0x0000FFFF & except_q;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

int exception_ut_register_expt_callback(EEMCS_CCCI_EX_IND func_ptr)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    g_expt_ut_inst.ccci_expt_cb = func_ptr;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

static int eemcs_expt_ut_lb(void *none)
{
    struct sk_buff *skb = NULL;
    KAL_UINT32 tx_qno = 0, rx_qno = 0;
    KAL_UINT32 pkts = 0;
    DEBUG_LOG_FUNCTION_ENTRY;

    while (1) {
        wait_event_interruptible(
            g_expt_ut_inst.lb_thread_wq,
            (kthread_should_stop() || atomic_read(&g_expt_ut_inst.lb_thread_cond)));
        atomic_set(&g_expt_ut_inst.lb_thread_cond, 0);
        DBGLOG(EXPT, DBG, "[EXPT_UT] eemcs_expt_ut_lb() is awaked !!");
        if (kthread_should_stop()) {
            break ;
        }

        pkts = kfifo_len(&g_expt_ut_inst.lb_qno_fifo)/sizeof(KAL_UINT32);
        DBGLOG(EXPT, DBG, "[EXPT_UT] %d packets in kfifo", pkts);
        while (pkts) {
            if (kfifo_out_spinlocked(&g_expt_ut_inst.lb_qno_fifo, &tx_qno, sizeof(KAL_UINT32), &g_expt_ut_inst.kfifo_lock) != sizeof(KAL_UINT32)) {
                DBGLOG(EXPT, ERR, "[EXPT_UT] Failed to get tx_qno from kfifo !!");
                continue;
            }
            /* get from UL queue */
            skb = skb_dequeue(&g_expt_ut_inst.tx_queue[tx_qno]);

            if (skb != NULL) {
                if (is_valid_txq(tx_qno, skb)) {
                    // modify skb content for loopback
                    exception_ul_lb_channel(skb, &rx_qno);
                    if (is_valid_rxq(rx_qno, skb)) {
                        skb_queue_tail(&g_expt_ut_inst.rx_queue[rx_qno], skb);
                        if (g_expt_ut_inst.enable_ccci_lb && g_expt_ut_inst.ccci_rx_cb[rx_qno] != NULL)
                            g_expt_ut_inst.ccci_rx_cb[rx_qno](rx_qno);
                        else
                            DBGLOG(EXPT, WAR, "[EXPT_UT] eemcs_expt_ut_lb() CCCI CB is disabled !!");
                    } else {
                        dev_kfree_skb(skb);
                        DBGLOG(EXPT, DBG, "[EXPT_UT] eemcs_expt_ut_lb() PKT DROPPED !! Rx_Q(%d)", rx_qno);
                        atomic_inc(&g_expt_ut_inst.rx_queue_drp[rx_qno]);
                    }
                } else {
                    dev_kfree_skb(skb);
                    DBGLOG(EXPT, DBG, "[EXPT_UT] eemcs_expt_ut_lb() PKT DROPPED !! Tx_Q(%d)", tx_qno);
                    atomic_inc(&g_expt_ut_inst.tx_queue_drp[tx_qno]);
                }
            } else {
                DBGLOG(EXPT, ERR, "[EXPT_UT] Failed to de-queue form UL !!");
            }
            pkts--;
        }
    }
    DEBUG_LOG_FUNCTION_LEAVE;
    return 0;
}
#endif
/*void eemcs_expt_enable_ut_rx_lb(KAL_UINT32 enable)
{
    if (enable > 0)
        g_expt_ut_inst.enable_rx_lb = 1;
    else
        g_expt_ut_inst.enable_rx_lb = 0;
}

ssize_t eemcs_expt_sysfs_show_ut_rx_lb(char *buf)
{
    return snprintf(buf, PAGE_SIZE, "[EXPT] Exception UT Rx LB = %d\n", g_expt_ut_inst.enable_rx_lb);
}
*/
void eemcs_expt_enable_ut_ccci_lb(KAL_UINT32 enable)
{
    if (enable > 0)
        g_expt_ut_inst.enable_ccci_lb = 1;
    else
        g_expt_ut_inst.enable_ccci_lb = 0;
}

ssize_t eemcs_expt_sysfs_show_ut_ccci_lb(char *buf)
{
    return snprintf(buf, PAGE_SIZE, "[EXPT] Exception UT CCCI LB = %d\n", g_expt_ut_inst.enable_ccci_lb);
}

/*ssize_t eemcs_expt_ut_show_statistics(char *buf)
{
    KAL_UINT32 i = 0;
    ssize_t pos = 0;

    DEBUG_LOG_FUNCTION_ENTRY;
    pos += snprintf(buf + pos, PAGE_SIZE, "\n=== Exception UT Statistics ===\n");
    pos += snprintf(buf + pos, PAGE_SIZE, "%8s %12s\n", "Queue", "PKT_DROP");
    for (i = 0; i < SDIO_TX_Q_NUM; i++) {
        pos += snprintf(buf + pos, PAGE_SIZE, "%4s_Q_%d %12d\n",
            "Tx", i,
            atomic_read(&g_expt_ut_inst.tx_queue_drp[i]));
    }
    for (i = 0; i < SDIO_RX_Q_NUM; i++) {
        pos += snprintf(buf + pos, PAGE_SIZE, "%4s_Q_%d %12d\n",
            "Rx", i, 
            atomic_read(&g_expt_ut_inst.rx_queue_drp[i]));
    }
    DEBUG_LOG_FUNCTION_ENTRY;
    return pos;
}*/

#endif // _EEMCS_EXCEPTION_UT

KAL_INT32 eemcs_expt_ut_init()
{
#ifdef _EEMCS_EXCEPTION_UT
    //KAL_UINT32 i;
    DEBUG_LOG_FUNCTION_ENTRY;

    /* initialization */
    spin_lock_init(&g_expt_ut_inst.kfifo_lock);
    spin_lock_init(&g_expt_ut_inst.lb_lock);
    sema_init(&g_expt_ut_inst.lb_sem, 1);
    //init_waitqueue_head(&g_expt_ut_inst.lb_thread_wq);
    //atomic_set(&g_expt_ut_inst.lb_thread_cond, 0);

    /*for (i = 0; i < SDIO_TX_Q_NUM; i++) {
        skb_queue_head_init(&g_expt_ut_inst.tx_queue[i]);
        atomic_set(&g_expt_ut_inst.tx_queue_drp[i], 0);
    }
    for (i = 0; i < SDIO_RX_Q_NUM; i++) {
        skb_queue_head_init(&g_expt_ut_inst.rx_queue[i]);
        atomic_set(&g_expt_ut_inst.rx_queue_drp[i], 0);
        g_expt_ut_inst.ccci_rx_cb[i] = NULL;
    }
    if (kfifo_alloc(&g_expt_ut_inst.lb_qno_fifo, 1000 * sizeof(KAL_UINT32), GFP_KERNEL) != 0) {
        DBGLOG(EXPT, ERR, "[EXPT_UT] Failed to alloc kfifo !!");
        goto _error;
    }*/
    
    /* default value */
    //g_expt_ut_inst.lb_thread = NULL;
    //g_expt_ut_inst.enable_rx_lb = 1;
    g_expt_ut_inst.enable_ccci_lb = 1;
    g_expt_ut_inst.ccci_expt_cb = NULL;
    g_expt_ut_inst.nonstop_q = 0;
    g_expt_ut_inst.expt_rxq = 0;
    g_expt_ut_inst.expt_txq = 0;
    g_expt_ut_inst.expt_id = -1;

    //g_expt_ut_inst.lb_thread = kthread_run(eemcs_expt_ut_lb, NULL, "expt_ut_lb_kthread");
    //if (IS_ERR(g_expt_ut_inst.lb_thread)) {
    //    DBGLOG(EXPT, ERR, "[EXPT_UT] eemcs_expt_ut_init - kthread_run fail");
    //}

    return KAL_SUCCESS;
//_error:
//    DEBUG_LOG_FUNCTION_LEAVE;
//    return KAL_FAIL;

#else // _EEMCS_EXCEPTION_UT
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
#endif // _EEMCS_EXCEPTION_UT
}

void eemcs_expt_ut_exit()
{
#if 0 //_EEMCS_EXCEPTION_UT
    KAL_UINT32 i;
    DEBUG_LOG_FUNCTION_ENTRY;

    for (i = 0; i < TX_Q_NUM; i++) {
        skb_queue_purge(&g_expt_ut_inst.tx_queue[i]);
    }
    for (i = 0; i < RX_Q_NUM; i++) {
        skb_queue_purge(&g_expt_ut_inst.rx_queue[i]);
    }
    if (!IS_ERR(g_expt_ut_inst.lb_thread)) {
        kthread_stop(g_expt_ut_inst.lb_thread);
    }
    kfifo_free(&g_expt_ut_inst.lb_qno_fifo);
    DEBUG_LOG_FUNCTION_LEAVE;

#else // _EEMCS_EXCEPTION_UT
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
#endif // _EEMCS_EXCEPTION_UT
}

