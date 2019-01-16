#ifndef __MODEM_UT_H__
#define __MODEM_UT_H__

#include <linux/wakelock.h>
#include <linux/dmapool.h>

#define LOOP_BACK // loop back, or gen Rx data
#ifdef LOOP_BACK
#ifdef CCCI_STATISTIC
#define STATISTIC // statistic data gathering for performance debug
#endif
//#define NO_RX_Q_LOCK // check comment in md_ut_rxq_process
#define NO_RX_Q_LOCK_IN_TX // check comment in md_ut_txq_process
#endif

#define MD_UT_MAJOR 169
#define MAX_QUEUE_LENGTH 32
#define QUEUE_BUDGET 16

typedef enum {
	HIF_EX_INIT = 0, // interrupt
	HIF_EX_ACK, // AP->MD
	HIF_EX_INIT_DONE, // polling
	HIF_EX_CLEARQ_DONE, //interrupt
	HIF_EX_CLEARQ_ACK, // AP->MD
	HIF_EX_ALLQ_RESET, // polling
}HIF_EX_STAGE;

struct md_ut_queue {
	DIRECTION dir;
	unsigned char index;
	struct ccci_modem *modem;

	struct list_head req_list;
	spinlock_t req_lock;
	int budget;
	int length;
	wait_queue_head_t req_wq;
	// now only for Tx, Rx won't wait, just drop
	int length_th;
#ifdef STATISTIC
	unsigned int process_count; // write or dispatch operation
	unsigned int not_complet_count; // have to wait due to list full
	unsigned int data_count; // packet size accumulation
#endif
};

struct md_ut_ctrl{
	struct md_ut_queue txq[3];
	struct md_ut_queue rxq[3];
	wait_queue_head_t sched_thread_wq;
	unsigned char sched_thread_kick;
	atomic_t reset_on_going;
	struct wake_lock trm_wake_lock;

	struct task_struct *sched_thread;
	struct timer_list rx_gen_timer;
#ifdef STATISTIC
	struct timer_list statistic_timer;
#endif
};

#define QUEUE_LEN(a) (sizeof(a)/sizeof(struct md_ut_queue))

static void inline md_ut_queue_struct_init(struct md_ut_queue *queue, struct ccci_modem *md,
	DIRECTION dir, unsigned char index, 
	int th, int bg)
{
	queue->dir = OUT;
	queue->index = index;
	queue->modem = md;

	INIT_LIST_HEAD(&queue->req_list);
	init_waitqueue_head(&queue->req_wq);
	spin_lock_init(&queue->req_lock);
	queue->length_th = th;
	queue->budget = bg;
	queue->length = 0;
#ifdef STATISTIC
	queue->process_count = 0;
	queue->not_complet_count = 0;
#endif
}

#endif //__MODEM_UT_H__