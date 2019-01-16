#ifndef __MODEM_CCIF_H__
#define __MODEM_CCIF_H__

#include <linux/wakelock.h>
#include <linux/dmapool.h>
#include <linux/atomic.h>
#include <mach/ccci_config.h>
#include <mach/mt_ccci_common.h>

#include "ccci_ringbuf.h"
#include "ccci_core.h"

#define QUEUE_NUM   8

struct ccif_sram_layout
{
    struct ccci_header dl_header;
    struct ccci_header up_header;
    struct modem_runtime runtime_data;
};

struct md_ccif_queue {    
    DIRECTION dir;
    unsigned char index;
    unsigned char rx_on_going;
    unsigned char debug_id;
    int budget;
    unsigned int ccif_ch;
    struct ccci_modem *modem;
    struct ccci_port *napi_port;
    struct ccci_ringbuf *ringbuf;
    spinlock_t rx_lock; // lock for the counter, only for rx
    spinlock_t tx_lock; // lock for the counter, only for Tx
    wait_queue_head_t req_wq; // only for Tx
    struct work_struct qwork;
    struct workqueue_struct *worker;
};

struct md_ccif_ctrl{
    struct md_ccif_queue txq[QUEUE_NUM];
    struct md_ccif_queue rxq[QUEUE_NUM];
    unsigned int total_smem_size;
    atomic_t reset_on_going;
    volatile unsigned long channel_id; // CCIF channel
    unsigned int ccif_irq_id;
    unsigned int md_wdt_irq_id;
    unsigned int sram_size;
    struct ccif_sram_layout * ccif_sram_layout;    
    struct wake_lock trm_wake_lock;
    char   wakelock_name[32];
    struct work_struct ccif_sram_work;
    struct tasklet_struct ccif_irq_task;
    struct timer_list bus_timeout_timer;
    void __iomem * ccif_ap_base;
    void __iomem * ccif_md_base;
    void __iomem * md_rgu_base;
    void __iomem * md_boot_slave_Vector;
    void __iomem * md_boot_slave_Key;
    void __iomem * md_boot_slave_En;
    
    struct md_hw_info * hw_info;
};


#endif //__MODEM_CCIF_H__
