#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/module.h>

#include <mach/ccci_config.h>
#include <mach/mt_ccci_common.h>

#include "ccci_bm.h"
#ifdef CCCI_STATISTIC
#define CREATE_TRACE_POINTS
#include "ccci_events.h"
#endif

// extra for network, check cldma modem's ring buffer size, network should not cusume CCCI's skb pool, but it do cosume req pool
struct ccci_request req_pool[BM_POOL_SIZE+2000];
int req_pool_cnt = BM_POOL_SIZE+2000;
spinlock_t req_pool_lock;
wait_queue_head_t req_pool_wq;

struct ccci_skb_queue skb_pool_4K;
struct ccci_skb_queue skb_pool_1_5K;
struct ccci_skb_queue skb_pool_16;

struct workqueue_struct *pool_reload_work_queue;

#ifdef CCCI_STATISTIC
struct timer_list ccci_bm_stat_timer;

void ccci_bm_stat_timer_func(unsigned long data)
{
	trace_ccci_bm(skb_pool_4K.skb_list.qlen, skb_pool_1_5K.skb_list.qlen, skb_pool_16.skb_list.qlen);
	mod_timer(&ccci_bm_stat_timer, jiffies+HZ/2);
}
#endif

static struct sk_buff *ccci_skb_dequeue(struct ccci_skb_queue *queue)
{
	unsigned long flags;
	struct sk_buff *result;

	spin_lock_irqsave(&queue->skb_list.lock, flags);
	result = __skb_dequeue(&queue->skb_list);
	if(queue->skb_list.qlen < queue->max_len/RELOAD_TH)
		queue_work(pool_reload_work_queue, &queue->reload_work);
	spin_unlock_irqrestore(&queue->skb_list.lock, flags);
	return result;
}

static void ccci_skb_enqueue(struct ccci_skb_queue *queue, struct sk_buff *newsk)
{
	unsigned long flags;

	spin_lock_irqsave(&queue->skb_list.lock, flags);
	if(queue->skb_list.qlen < queue->max_len) {
		__skb_queue_tail(&queue->skb_list, newsk);
	} else {
		dev_kfree_skb_any(newsk);
	}
	spin_unlock_irqrestore(&queue->skb_list.lock, flags);
}

static inline struct sk_buff *__alloc_skb_from_pool(int size)
{
	struct sk_buff *skb = NULL;
	if(size > SKB_1_5K) {
		skb = ccci_skb_dequeue(&skb_pool_4K);
	} else if(size > SKB_16) {
		skb = ccci_skb_dequeue(&skb_pool_1_5K);
	} else if(size > 0) {
		skb = ccci_skb_dequeue(&skb_pool_16);
	}
	return skb;
}

static inline struct sk_buff *__alloc_skb_from_kernel(int size)
{
	struct sk_buff *skb = NULL;
	if(size > SKB_1_5K) {
		skb = dev_alloc_skb(SKB_4K);
	} else if(size > SKB_16) {
		skb = dev_alloc_skb(SKB_1_5K);
	} else if(size > 0) {
		skb = dev_alloc_skb(SKB_16);
	}
	if(!skb)
		CCCI_ERR_MSG(-1, BM, "%ps alloc skb from kernel fail, size=%d\n", __builtin_return_address(0), size);
	return skb;
}

// may return NULL, caller should check, network should always use blocking as we do not want it consume our own pool
struct sk_buff *ccci_alloc_skb(int size, char blocking)
{
	int count = 0;
	struct sk_buff *skb = NULL;

	if(size>SKB_4K || size<0)
		goto err_exit;
	skb = blocking?NULL:__alloc_skb_from_kernel(size);
	
	if(!skb) {
slow_retry:
		skb = __alloc_skb_from_pool(size);
	}
	if(unlikely(!skb)) {
		if(blocking) {
			CCCI_INF_MSG(-1, BM, "skb pool is empty! size=%d (%d)\n", size, count++);
			msleep(100);
			goto slow_retry;
		} else {
fast_retry:
			skb = __alloc_skb_from_kernel(size);
			if(!skb && count++<20)
				goto fast_retry;
		}
	}
err_exit:
	if(unlikely(!skb))
		CCCI_ERR_MSG(-1, BM, "%ps alloc skb fail, size=%d\n", __builtin_return_address(0), size);
	else
		CCCI_DBG_MSG(-1, BM, "%ps alloc skb %p, size=%d\n", __builtin_return_address(0), skb, size);
	return skb;
}
EXPORT_SYMBOL(ccci_alloc_skb);

void ccci_free_skb(struct sk_buff *skb, DATA_POLICY policy)
{
	CCCI_DBG_MSG(-1, BM, "%ps free skb %p, policy=%d, len=%d\n", __builtin_return_address(0), 
		skb, policy, skb_size(skb));
	switch(policy) {
		case RECYCLE:	
			// 1. reset sk_buff (take __alloc_skb as ref.)
			skb->data = skb->head;
			skb->len = 0;
			skb_reset_tail_pointer(skb);
			// 2. enqueue
			if(skb_size(skb) < SKB_1_5K) {
				ccci_skb_enqueue(&skb_pool_16, skb);
			} else if (skb_size(skb) < SKB_4K) {
				ccci_skb_enqueue(&skb_pool_1_5K, skb);
			} else {
				ccci_skb_enqueue(&skb_pool_4K, skb);
			}
			break;
		case FREE:
			dev_kfree_skb_any(skb);
			break;
		case NOOP:
		default:
			break;
	};
}
EXPORT_SYMBOL(ccci_free_skb);

static void __4K_reload_work(struct work_struct *work)
{
	struct sk_buff *skb;

	CCCI_DBG_MSG(-1, BM, "refill 4KB skb pool\n");
	while(skb_pool_4K.skb_list.qlen < SKB_POOL_SIZE_4K) {
		skb = dev_alloc_skb(SKB_4K);
		if(!skb)
			skb = __dev_alloc_skb(SKB_4K, GFP_KERNEL);
		if(skb)
			skb_queue_tail(&skb_pool_4K.skb_list, skb);
		else
			CCCI_ERR_MSG(-1, BM, "fail to reload 4KB pool\n");
	}
}

static void __1_5K_reload_work(struct work_struct *work)
{
	struct sk_buff *skb;
	
	CCCI_DBG_MSG(-1, BM, "refill 1.5KB skb pool\n");
	while(skb_pool_1_5K.skb_list.qlen < SKB_POOL_SIZE_1_5K) {
		skb = dev_alloc_skb(SKB_1_5K);
		if(!skb)
			skb = __dev_alloc_skb(SKB_1_5K, GFP_KERNEL);
		if(skb)
			skb_queue_tail(&skb_pool_1_5K.skb_list, skb);
		else
			CCCI_ERR_MSG(-1, BM, "fail to reload 1.5KB pool\n");
		
	}
}

static void __16_reload_work(struct work_struct *work)
{
	struct sk_buff *skb;
	
	CCCI_DBG_MSG(-1, BM, "refill 16B skb pool\n");
	while(skb_pool_16.skb_list.qlen < SKB_POOL_SIZE_16) {
		skb = dev_alloc_skb(SKB_16);
		if(!skb)
			skb = __dev_alloc_skb(SKB_16, GFP_KERNEL);
		if(skb)
			skb_queue_tail(&skb_pool_16.skb_list, skb);
		else
			CCCI_ERR_MSG(-1, BM, "fail to reload 16B pool\n");
	}
}

/*
 * a write operation may block at 3 stages:
 * 1. ccci_alloc_req
 * 2. wait until the queue has available slot (threshold check)
 * 3. wait until the SDIO transfer is complete --> abandoned, see the reason below.
 * the 1st one is decided by @blk1. and the 2nd and 3rd are decided by @blk2, wating on @wq.
 * NULL is returned if no available skb, even when you set blk1=1.
 *
 * we removed the wait_queue_head_t in ccci_request, so user can NOT wait for certain request to
 * be completed. this is because request will be recycled and its state will be reset, so if a request
 * is completed and then used again, the poor guy who is waiting for it may never see the state 
 * transition (FLYING->IDLE/COMPLETE->FLYING) and wait forever.
 */
struct ccci_request *ccci_alloc_req(DIRECTION dir, int size, char blk1, char blk2)
{
	int i;
	struct ccci_request *req = NULL;
	unsigned long flags;

retry:
	spin_lock_irqsave(&req_pool_lock, flags);
	for(i=0; i<sizeof(req_pool)/sizeof(struct ccci_request); i++) {
		if(req_pool[i].state == IDLE) {
			// important checking when reqeust is passed cross-layer, make sure this request is no longer in any list
			if(req_pool[i].entry.next == LIST_POISON1 && req_pool[i].entry.prev == LIST_POISON2) {
				req = &req_pool[i];
				CCCI_DBG_MSG(-1, BM, "%ps alloc req=%p, i=%d size=%d\n", __builtin_return_address(0), req, i, size);
				req->state = FLYING;
				break;
			} else {
				// should not happen
				CCCI_ERR_MSG(-1, BM, "idle but in list i=%d, from %ps\n", i, __builtin_return_address(0));
				list_del(&req_pool[i].entry);
			}
		}
	}
	if(req) {
		req->dir = dir;
		req_pool_cnt--;
		CCCI_DBG_MSG(-1, BM, "pool count-=%d\n", req_pool_cnt);
	}
	spin_unlock_irqrestore(&req_pool_lock, flags);
	if(req) {
		if(size>0) {
			req->skb = ccci_alloc_skb(size, blk1);
			req->policy = RECYCLE;
			if(req->skb)
				CCCI_DBG_MSG(-1, BM, "alloc ok, req=%p skb=%p, len=%d\n", req, req->skb, skb_size(req->skb));
		} else {
			req->skb = NULL;
			req->policy = NOOP;
		}
		req->blocking = blk2;
	} else {
		if(blk1) {
			wait_event_interruptible(req_pool_wq, (req_pool_cnt>0));
			goto retry;
		}
		CCCI_INF_MSG(-1, BM, "fail to alloc req for %ps, no retry\n", __builtin_return_address(0));
	}
	if(unlikely(size>0 && !req->skb)) {
		CCCI_ERR_MSG(-1, BM, "fail to alloc skb for %ps, size=%d\n", __builtin_return_address(0), size);
		req->policy = NOOP;
		ccci_free_req(req);
		req = NULL;
	}
	return req;
}
EXPORT_SYMBOL(ccci_alloc_req);

void ccci_free_req(struct ccci_request *req)
{
	unsigned long flags;

	CCCI_DBG_MSG(-1, BM, "%ps free req=%p, policy=%d, skb=%p\n", __builtin_return_address(0),
		req, req->policy, req->skb);
	if(req->skb)
		ccci_free_skb(req->skb, req->policy);
	spin_lock_irqsave(&req_pool_lock, flags);
	// 1. reset the request
	req->state = IDLE;
	req->skb = NULL;
	/*
	 * do NOT reset req->entry here, always maitain it by list API (list_del).
	 * for Tx requests, they are never in any queue, so no extra effort when delete them.
	 * but for Rx request, we must make sure list_del is called once before we free them.
	 */
	if(req->entry.next != LIST_POISON1 || req->entry.prev != LIST_POISON2) {
		CCCI_ERR_MSG(-1, BM, "req %p entry not deleted yet, from %ps\n", req, __builtin_return_address(0));
		list_del(&req->entry);
	}
	// 2. wake up pending allocation
	req_pool_cnt++;
	CCCI_DBG_MSG(-1, BM, "pool count+=%d\n", req_pool_cnt);
	spin_unlock_irqrestore(&req_pool_lock, flags);
	wake_up_all(&req_pool_wq);

}
EXPORT_SYMBOL(ccci_free_req);

void ccci_mem_dump(int md_id, void *start_addr, int len)
{
	unsigned int *curr_p = (unsigned int *)start_addr;
	unsigned char *curr_ch_p;
	int _16_fix_num = len/16;
	int tail_num = len%16;
	char buf[16];
	int i,j;

	if(NULL == curr_p) {
		printk("[CCCI%d-DUMP]NULL point to dump!\n",md_id+1);
		return;
	}
	if(0 == len){
		printk("[CCCI%d-DUMP]Not need to dump\n",md_id+1);
		return;
	}

	printk("[CCCI%d-DUMP]Base: %p\n", md_id+1,start_addr);
	// Fix section
	for(i=0; i<_16_fix_num; i++){
		printk("[CCCI%d-DUMP]%03X: %08X %08X %08X %08X\n", 
				md_id+1,i*16, *curr_p, *(curr_p+1), *(curr_p+2), *(curr_p+3) );
		curr_p+=4;
	}

	// Tail section
	if(tail_num > 0){
		curr_ch_p = (unsigned char*)curr_p;
		for(j=0; j<tail_num; j++){
			buf[j] = *curr_ch_p;
			curr_ch_p++;
		}
		for(; j<16; j++)
			buf[j] = 0;
		curr_p = (unsigned int*)buf;
		printk("[CCCI%d-DUMP]%03X: %08X %08X %08X %08X\n", 
				md_id+1,i*16, *curr_p, *(curr_p+1), *(curr_p+2), *(curr_p+3) );
	}
}
EXPORT_SYMBOL(ccci_mem_dump);

void ccci_cmpt_mem_dump(int md_id, void *start_addr, int len)
{
	unsigned int *curr_p = (unsigned int *)start_addr;
	unsigned char *curr_ch_p;
	int _32_fix_num = len/32;
	int tail_num = len%32;
	char buf[32];
	int i,j;

	if(NULL == curr_p) {
		printk(KERN_DEBUG "[CCCI%d-DUMP]NULL point to dump!\n", md_id+1);
		return;
	}
	if(0 == len){
		printk(KERN_DEBUG "[CCCI%d-DUMP]Not need to dump\n", md_id+1);
		return;
	}

	printk(KERN_DEBUG "[CCCI%d-DUMP]Base: %p\n", md_id+1, start_addr);
	// Fix section
	for(i=0; i<_32_fix_num; i++){
		printk(KERN_DEBUG "[CCCI%d-DUMP]%03X: %X %X %X %X %X %X %X %X\n", 
				md_id+1, i*32, 
				*curr_p, *(curr_p+1), *(curr_p+2), *(curr_p+3),
				*(curr_p+4), *(curr_p+5), *(curr_p+6), *(curr_p+7));
		curr_p+=8;
	}

	// Tail section
	if(tail_num > 0){
		curr_ch_p = (unsigned char*)curr_p;
		for(j=0; j<tail_num; j++){
			buf[j] = *curr_ch_p;
			curr_ch_p++;
		}
		for(; j<32; j++)
			buf[j] = 0;
		curr_p = (unsigned int*)buf;
		printk(KERN_DEBUG "[CCCI%d-DUMP]%03X: %X %X %X %X %X %X %X %X\n", 
				md_id+1, i*32, 
				*curr_p, *(curr_p+1), *(curr_p+2), *(curr_p+3),
				*(curr_p+4), *(curr_p+5), *(curr_p+6), *(curr_p+7));
	}
}
EXPORT_SYMBOL(ccci_cmpt_mem_dump);

void ccci_dump_req(struct ccci_request *req)
{
	/*
	int i, len;
	if(req && req->skb) {
		len = req->skb->len;
		printk("[ccci dump(%d)]", len);
		for(i=0; i<len && len<32; i++) {
			printk("%02X ", *(req->skb->data+i));
		}
		printk("\n");
	}
	*/
	ccci_mem_dump(-1,req->skb->data, req->skb->len>32?32:req->skb->len);
}
EXPORT_SYMBOL(ccci_dump_req);

int ccci_subsys_bm_init(void)
{
	int i;
	struct sk_buff *skb = NULL;

	CCCI_INF_MSG(-1, BM, "MTU=%d/%d, pool size %d/%d/%d/%d\n", CCCI_MTU, CCMNI_MTU, 
		SKB_POOL_SIZE_4K, SKB_POOL_SIZE_1_5K, SKB_POOL_SIZE_16, req_pool_cnt);
	// init ccci_request
	for(i=0; i<sizeof(req_pool)/sizeof(struct ccci_request); i++) {
		ccci_request_struct_init(&req_pool[i]);
	}
	// init skb pool
	skb_queue_head_init(&skb_pool_4K.skb_list);
	skb_pool_4K.max_len = SKB_POOL_SIZE_4K;
	for(i=0; i<SKB_POOL_SIZE_4K; i++) {
		skb = dev_alloc_skb(SKB_4K);
		skb_queue_tail(&skb_pool_4K.skb_list, skb);
	}
	skb_queue_head_init(&skb_pool_1_5K.skb_list);
	skb_pool_1_5K.max_len = SKB_POOL_SIZE_1_5K;
	skb = NULL;
	for(i=0; i<SKB_POOL_SIZE_1_5K; i++) {
		skb = dev_alloc_skb(SKB_1_5K);
		skb_queue_tail(&skb_pool_1_5K.skb_list, skb);
	}
	skb_queue_head_init(&skb_pool_16.skb_list);
	skb_pool_16.max_len = SKB_POOL_SIZE_16;
	skb = NULL;
	for(i=0; i<SKB_POOL_SIZE_16; i++) {
		skb = dev_alloc_skb(SKB_16);
		skb_queue_tail(&skb_pool_16.skb_list, skb);
	}
	// init pool reload work
	pool_reload_work_queue = alloc_workqueue("pool_reload_work", WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	INIT_WORK(&skb_pool_4K.reload_work, __4K_reload_work);
	INIT_WORK(&skb_pool_1_5K.reload_work, __1_5K_reload_work);
	INIT_WORK(&skb_pool_16.reload_work, __16_reload_work);
	
#ifdef CCCI_STATISTIC
	init_timer(&ccci_bm_stat_timer);
	ccci_bm_stat_timer.function = ccci_bm_stat_timer_func;
	mod_timer(&ccci_bm_stat_timer, jiffies+10*HZ);
#endif
	spin_lock_init(&req_pool_lock);
	init_waitqueue_head(&req_pool_wq);
	return 0;
}

