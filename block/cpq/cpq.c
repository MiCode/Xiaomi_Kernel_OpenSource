// SPDX-License-Identifier: GPL-2.0
/*
 *  Command Priority Queueing i/o scheduler
 */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/compiler.h>
#include <linux/rbtree.h>
#include <linux/sbitmap.h>
#include <linux/cgroup.h>

#include <trace/events/block.h>

#include "../elevator.h"
#include "../blk.h"
#include "../blk-mq.h"
#include "../blk-mq-debugfs.h"
#include "../blk-mq-sched.h"

static const int read_expire = HZ / 2;  /* max time before a read is submitted. */
static const int write_expire = 5 * HZ; /* ditto for writes, these limits are SOFT! */
/*
 * Time after which to dispatch lower priority requests even if higher
 * priority requests are pending.
 */
static const int prio_aging_expire = 10 * HZ;
static const int writes_starved = 2;    /* max times reads can starve a write */
static const int fifo_batch = 16;       /* # of sequential requests treated as one
				     by the above parameters. For throughput. */

static const unsigned long cpq_fore_timeout = 4000 ; //for 40 times 4KB read (89*40 is almost equal 4000)
static const unsigned long cpq_back_timeout = 1000 ;

//data from ufs4.0 + sm8650
static const unsigned int ufs_timecost_table[2][8] = {
   /*4   8    16   32   64   128  256  512*/
	{89, 107, 118, 124, 151, 167, 197, 291},//read
	{50, 54,  56,  61,  65,  86,  136, 168} //write
};


static u64 cpq_slice_idle = NSEC_PER_SEC / 1000;
static int cpq_io_threshold = 8;// which is 8 requests has inserted in scheduler but not completed in the whole system

enum cpq_data_dir {
	CPQ_READ		= READ,
	CPQ_WRITE	= WRITE,
};

enum { CPQ_DIR_COUNT = 2 };

enum cpq_prio {
	CPQ_RT_PRIO	= 0,
	CPQ_BE_PRIO	= 1,
	CPQ_IDLE_PRIO	= 2,
	CPQ_PRIO_MAX	= 2,
};

enum { CPQ_PRIO_COUNT = 3 };

enum cpq_group {
	FORE_GROUND = 0,
	BACK_GROUND = 1,
	GROUND_NUM = 2,
};
enum cpq_group_expiration {
	GROUP_NO_MORE_REQUEST,
	GROUP_TIMEOUT,
	GROUP_PREEMPTED,
};

/*
 * I/O statistics per I/O priority. It is fine if these counters overflow.
 * What matters is that these counters are at least as wide as
 * log2(max_outstanding_requests).
 */
struct io_stats_per_prio {
	uint32_t inserted;
	uint32_t merged;
	uint32_t dispatched;
	uint32_t completed;
};

/*
 * CPQ scheduler data per I/O priority (enum cpq_prio). Requests are
 * present on both sort_list[] and fifo_list[].
 */
struct cpq_per_prio {
	struct list_head dispatch;
	struct rb_root sort_list[CPQ_DIR_COUNT];
	struct list_head fifo_list[CPQ_DIR_COUNT];
	/* Next request in FIFO order. Read, write or both are NULL. */
	struct request *next_rq[CPQ_DIR_COUNT];
	struct io_stats_per_prio stats;
};

struct cpq_per_group{
	struct cpq_per_prio per_prio[CPQ_PRIO_COUNT];
	unsigned long time_cost;
	int time_out;
	enum cpq_group_expiration last_expire_reason;
};

struct cpq_data {
	/*
	 * run time data
	 */

	struct cpq_per_group per_group[GROUND_NUM];
	enum cpq_group in_service_group;

	/* Data direction of latest dispatched request. */
	enum cpq_data_dir last_dir;
	unsigned int batching;		/* number of sequential requests made */
	unsigned int starved;		/* times reads have starved writes */

	/*
	 * settings that change how the i/o scheduler behaves
	 */
	int fifo_expire[CPQ_DIR_COUNT];
	int fifo_batch;
	int writes_starved;
	int front_merges;
	u32 async_depth;
	int prio_aging_expire;

	struct hrtimer idle_slice_timer;
	u64 cpq_slice_idle;
	int cpq_log;
	bool wait_request;
	bool timer_flag;
	int rq_in_driver[2];// 0 is asysnc, 1 is sync
	int io_threshold;

	spinlock_t lock;
	spinlock_t zone_lock;
};

/* Maps an I/O priority class to a CPQ scheduler priority. */
static const enum cpq_prio ioprio_class_to_prio[] = {
	[IOPRIO_CLASS_NONE]	= CPQ_BE_PRIO,
	[IOPRIO_CLASS_RT]	= CPQ_RT_PRIO,
	[IOPRIO_CLASS_BE]	= CPQ_BE_PRIO,
	[IOPRIO_CLASS_IDLE]	= CPQ_IDLE_PRIO,
};

static inline struct rb_root *
cpq_rb_root(struct cpq_per_prio *per_prio, struct request *rq)
{
	return &per_prio->sort_list[rq_data_dir(rq)];
}

/*
 * Returns the I/O priority class (IOPRIO_CLASS_*) that has been assigned to a
 * request.
 */
static u8 cpq_rq_ioclass(struct request *rq)
{
	return IOPRIO_PRIO_CLASS(req_get_ioprio(rq));
}

/*
 * get the request before `rq' in sector-sorted order
 */
static inline struct request *
cpq_earlier_request(struct request *rq)
{
	struct rb_node *node = rb_prev(&rq->rb_node);

	if (node)
		return rb_entry_rq(node);

	return NULL;
}

/*
 * get the request after `rq' in sector-sorted order
 */
static inline struct request *
cpq_latter_request(struct request *rq)
{
	struct rb_node *node = rb_next(&rq->rb_node);

	if (node)
		return rb_entry_rq(node);

	return NULL;
}

static void
cpq_add_rq_rb(struct cpq_per_prio *per_prio, struct request *rq)
{
	struct rb_root *root = cpq_rb_root(per_prio, rq);

	elv_rb_add(root, rq);
}

static inline void
cpq_del_rq_rb(struct cpq_per_prio *per_prio, struct request *rq)
{
	const enum cpq_data_dir data_dir = rq_data_dir(rq);

	if (per_prio->next_rq[data_dir] == rq)
		per_prio->next_rq[data_dir] = cpq_latter_request(rq);

	elv_rb_del(cpq_rb_root(per_prio, rq), rq);
}

/*
 * remove rq from rbtree and fifo.
 */
static void cpq_remove_request(struct request_queue *q,
				    struct cpq_per_prio *per_prio,
				    struct request *rq)
{
	list_del_init(&rq->queuelist);

	/*
	 * We might not be on the rbtree, if we are doing an insert merge
	 */
	if (!RB_EMPTY_NODE(&rq->rb_node))
		cpq_del_rq_rb(per_prio, rq);

	elv_rqhash_del(q, rq);
	if (q->last_merge == rq)
		q->last_merge = NULL;
}

static void cpq_request_merged(struct request_queue *q, struct request *req,
			      enum elv_merge type)
{
	struct cpq_data *cpqd = q->elevator->elevator_data;
	const u8 ioprio_class = cpq_rq_ioclass(req);
	const enum cpq_prio prio = ioprio_class_to_prio[ioprio_class];
	struct cpq_per_prio *per_prio;

	if(req->elv.priv[1]){
		per_prio = &cpqd->per_group[BACK_GROUND].per_prio[prio];
	}else{
		per_prio = &cpqd->per_group[FORE_GROUND].per_prio[prio];
	}

	/*
	 * if the merge was a front merge, we need to reposition request
	 */
	if (type == ELEVATOR_FRONT_MERGE) {
		elv_rb_del(cpq_rb_root(per_prio, req), req);
		cpq_add_rq_rb(per_prio, req);
	}
}

/*
 * Callback function that is invoked after @next has been merged into @req.
 */
static void cpq_merged_requests(struct request_queue *q, struct request *req,
			       struct request *next)
{
	struct cpq_data *cpqd = q->elevator->elevator_data;
	const u8 ioprio_class = cpq_rq_ioclass(next);
	const enum cpq_prio prio = ioprio_class_to_prio[ioprio_class];
	struct cpq_per_prio *per_prio;

	lockdep_assert_held(&cpqd->lock);

	if(next->elv.priv[1]){
		per_prio = &cpqd->per_group[BACK_GROUND].per_prio[prio];
	}else{
		per_prio = &cpqd->per_group[FORE_GROUND].per_prio[prio];
	}

	per_prio->stats.merged++;

	/*
	 * if next expires before rq, assign its expire time to rq
	 * and move into next position (next will be deleted) in fifo
	 */
	if ((req->elv.priv[1] == next->elv.priv[1]) && (!list_empty(&req->queuelist) && !list_empty(&next->queuelist))) {
		if (time_before((unsigned long)next->fifo_time,
				(unsigned long)req->fifo_time)) {
			list_move(&req->queuelist, &next->queuelist);
			req->fifo_time = next->fifo_time;
		}
	}

	/*
	 * kill knowledge of next, this one is a goner
	 */
	cpq_remove_request(q, per_prio, next);
}

/*
 * move an entry to dispatch queue
 */
static void
cpq_move_request(struct cpq_data *cpqd, struct cpq_per_prio *per_prio,
		      struct request *rq)
{
	const enum cpq_data_dir data_dir = rq_data_dir(rq);

	per_prio->next_rq[data_dir] = cpq_latter_request(rq);

	/*
	 * take it off the sort and fifo list
	 */
	cpq_remove_request(rq->q, per_prio, rq);
}

/* Number of requests queued for a given priority level. */
static u32 cpq_queued(struct cpq_data *cpqd, struct cpq_per_group *cpq_group, enum cpq_prio prio)
{
	const struct io_stats_per_prio *stats = &cpq_group->per_prio[prio].stats;

	lockdep_assert_held(&cpqd->lock);

	return stats->inserted - stats->completed;
}

/*
 * cpq_check_fifo returns 0 if there are no expired requests on the fifo,
 * 1 otherwise. Requires !list_empty(&cpqd->fifo_list[data_dir])
 */
static inline int cpq_check_fifo(struct cpq_per_prio *per_prio,
				      enum cpq_data_dir data_dir)
{
	struct request *rq = rq_entry_fifo(per_prio->fifo_list[data_dir].next);

	/*
	 * rq is expired!
	 */
	if (time_after_eq(jiffies, (unsigned long)rq->fifo_time))
		return 1;

	return 0;
}

/*
 * Check if rq has a sequential request preceding it.
 */
static bool cpq_is_seq_write(struct cpq_data *cpqd, struct request *rq)
{
	struct request *prev = cpq_earlier_request(rq);

	if (!prev)
		return false;

	return blk_rq_pos(prev) + blk_rq_sectors(prev) == blk_rq_pos(rq);
}

/*
 * Skip all write requests that are sequential from @rq, even if we cross
 * a zone boundary.
 */
static struct request *cpq_skip_seq_writes(struct cpq_data *cpqd,
						struct request *rq)
{
	sector_t pos = blk_rq_pos(rq);
	sector_t skipped_sectors = 0;

	while (rq) {
		if (blk_rq_pos(rq) != pos + skipped_sectors)
			break;
		skipped_sectors += blk_rq_sectors(rq);
		rq = cpq_latter_request(rq);
	}

	return rq;
}

/*
 * For the specified data direction, return the next request to
 * dispatch using arrival ordered lists.
 */
static struct request *
cpq_fifo_request(struct cpq_data *cpqd, struct cpq_per_prio *per_prio,
		      enum cpq_data_dir data_dir)
{
	struct request *rq;
	unsigned long flags;

	if (list_empty(&per_prio->fifo_list[data_dir]))
		return NULL;

	rq = rq_entry_fifo(per_prio->fifo_list[data_dir].next);
	if (data_dir == CPQ_READ || !blk_queue_is_zoned(rq->q))
		return rq;

	/*
	 * Look for a write request that can be dispatched, that is one with
	 * an unlocked target zone. For some HDDs, breaking a sequential
	 * write stream can lead to lower throughput, so make sure to preserve
	 * sequential write streams, even if that stream crosses into the next
	 * zones and these zones are unlocked.
	 */
	spin_lock_irqsave(&cpqd->zone_lock, flags);
	list_for_each_entry(rq, &per_prio->fifo_list[CPQ_WRITE], queuelist) {
		if (blk_req_can_dispatch_to_zone(rq) &&
		    (blk_queue_nonrot(rq->q) ||
		     !cpq_is_seq_write(cpqd, rq)))
			goto out;
	}
	rq = NULL;
out:
	spin_unlock_irqrestore(&cpqd->zone_lock, flags);

	return rq;
}

/*
 * For the specified data direction, return the next request to
 * dispatch using sector position sorted lists.
 */
static struct request *
cpq_next_request(struct cpq_data *cpqd, struct cpq_per_prio *per_prio,
		      enum cpq_data_dir data_dir)
{
	struct request *rq;
	unsigned long flags;

	rq = per_prio->next_rq[data_dir];
	if (!rq)
		return NULL;

	if (data_dir == CPQ_READ || !blk_queue_is_zoned(rq->q))
		return rq;

	/*
	 * Look for a write request that can be dispatched, that is one with
	 * an unlocked target zone. For some HDDs, breaking a sequential
	 * write stream can lead to lower throughput, so make sure to preserve
	 * sequential write streams, even if that stream crosses into the next
	 * zones and these zones are unlocked.
	 */
	spin_lock_irqsave(&cpqd->zone_lock, flags);
	while (rq) {
		if (blk_req_can_dispatch_to_zone(rq))
			break;
		if (blk_queue_nonrot(rq->q))
			rq = cpq_latter_request(rq);
		else
			rq = cpq_skip_seq_writes(cpqd, rq);
	}
	spin_unlock_irqrestore(&cpqd->zone_lock, flags);

	return rq;
}

/*
 * Returns true if and only if @rq started after @latest_start where
 * @latest_start is in jiffies.
 */
static bool started_after(struct cpq_data *cpqd, struct request *rq,
			  unsigned long latest_start)
{
	unsigned long start_time = (unsigned long)rq->fifo_time;

	start_time -= cpqd->fifo_expire[rq_data_dir(rq)];

	return time_after(start_time, latest_start);
}

static void cpq_update_group_cost(const struct request * rq, struct cpq_per_group *cpq_group)
{
	enum cpq_data_dir data_dir = rq_data_dir(rq);
	unsigned int data_len = blk_rq_bytes(rq);
	if(data_len <= 4096){
		cpq_group->time_cost += ufs_timecost_table[data_dir][0]; //0k~4k
	}else if(data_len > 4096 && data_len <= 8192){
		cpq_group->time_cost += ufs_timecost_table[data_dir][1]; //4k~8k
	}else if(data_len > 8192 && data_len <= 16384){
		cpq_group->time_cost += ufs_timecost_table[data_dir][2]; //8k~16k
	}else if(data_len > 16384 && data_len <= 32768){
		cpq_group->time_cost += ufs_timecost_table[data_dir][3]; //16k~32k
	}else if(data_len > 32768 && data_len <= 65536){
		cpq_group->time_cost += ufs_timecost_table[data_dir][4]; //32k~64k
	}else if(data_len > 65536 && data_len <= 131072){
		cpq_group->time_cost += ufs_timecost_table[data_dir][5]; //64k~128k
	}else if(data_len > 131072 && data_len <= 262144){
		cpq_group->time_cost += ufs_timecost_table[data_dir][6]; //128k~256k
	}else if(data_len > 262144 && data_len <= 524288){
		cpq_group->time_cost += ufs_timecost_table[data_dir][7]; //256K~512k
	}else{
		cpq_group->time_cost += ufs_timecost_table[data_dir][7]; //512k~
	}
}

/*
 * cpq_dispatch_requests selects the best request according to
 * read/write expire, fifo_batch, etc and with a start time <= @latest_start.
 */
static struct request *__cpq_dispatch_request(struct cpq_data *cpqd,
					     struct cpq_per_prio *per_prio,
					     unsigned long latest_start)
{
	struct request *rq, *next_rq;
	enum cpq_data_dir data_dir;
	enum cpq_prio prio;
	u8 ioprio_class;
	struct cpq_per_group *cpq_group;

	lockdep_assert_held(&cpqd->lock);

	if (!list_empty(&per_prio->dispatch)) {
		rq = list_first_entry(&per_prio->dispatch, struct request,
				      queuelist);
		if (started_after(cpqd, rq, latest_start))
			return NULL;
		list_del_init(&rq->queuelist);
		goto done;
	}

	/*
	 * batches are currently reads XOR writes
	 */
	rq = cpq_next_request(cpqd, per_prio, cpqd->last_dir);
	if (rq && cpqd->batching < cpqd->fifo_batch)
		/* we have a next request are still entitled to batch */
		goto dispatch_request;

	/*
	 * at this point we are not running a batch. select the appropriate
	 * data direction (read / write)
	 */

	if (!list_empty(&per_prio->fifo_list[CPQ_READ])) {
		BUG_ON(RB_EMPTY_ROOT(&per_prio->sort_list[CPQ_READ]));

		if (cpq_fifo_request(cpqd, per_prio, CPQ_WRITE) &&
		    (cpqd->starved++ >= cpqd->writes_starved))
			goto dispatch_writes;

		data_dir = CPQ_READ;

		goto dispatch_find_request;
	}

	/*
	 * there are either no reads or writes have been starved
	 */

	if (!list_empty(&per_prio->fifo_list[CPQ_WRITE])) {
dispatch_writes:
		BUG_ON(RB_EMPTY_ROOT(&per_prio->sort_list[CPQ_WRITE]));

		cpqd->starved = 0;

		data_dir = CPQ_WRITE;

		goto dispatch_find_request;
	}

	return NULL;

dispatch_find_request:
	/*
	 * we are not running a batch, find best request for selected data_dir
	 */
	next_rq = cpq_next_request(cpqd, per_prio, data_dir);
	if (cpq_check_fifo(per_prio, data_dir) || !next_rq) {
		/*
		 * A deadline has expired, the last request was in the other
		 * direction, or we have run out of higher-sectored requests.
		 * Start again from the request with the earliest expiry time.
		 */
		rq = cpq_fifo_request(cpqd, per_prio, data_dir);
	} else {
		/*
		 * The last req was the same dir and we have a next request in
		 * sort order. No expired requests so continue on from here.
		 */
		rq = next_rq;
	}

	/*
	 * For a zoned block device, if we only have writes queued and none of
	 * them can be dispatched, rq will be NULL.
	 */
	if (!rq)
		return NULL;

	cpqd->last_dir = data_dir;
	cpqd->batching = 0;

dispatch_request:
	if (started_after(cpqd, rq, latest_start))
		return NULL;

	/*
	 * rq is the selected appropriate request.
	 */
	cpqd->batching++;
	cpq_move_request(cpqd, per_prio, rq);
done:
	ioprio_class = cpq_rq_ioclass(rq);
	prio = ioprio_class_to_prio[ioprio_class];
	if(rq->elv.priv[1]){
		cpq_group = &cpqd->per_group[BACK_GROUND];
	}else{
		cpq_group = &cpqd->per_group[FORE_GROUND];
	}

	cpq_update_group_cost(rq, cpq_group);
	cpq_group->per_prio[prio].stats.dispatched++;
	/*
	 * If the request needs its target zone locked, do it.
	 */
	blk_req_zone_write_lock(rq);
	rq->rq_flags |= RQF_STARTED;
	return rq;
}

/*
 * Check whether there are any requests with priority other than CPQ_RT_PRIO
 * that were inserted more than prio_aging_expire jiffies ago.
 */
static struct request *cpq_dispatch_prio_aged_requests(struct cpq_data *cpqd,
							  struct cpq_per_group *cpq_group,
						      unsigned long now)
{
	struct request *rq;
	enum cpq_prio prio;
	int prio_cnt;

	lockdep_assert_held(&cpqd->lock);

	prio_cnt = !!cpq_queued(cpqd, cpq_group, CPQ_RT_PRIO) + !!cpq_queued(cpqd, cpq_group, CPQ_BE_PRIO) +
		   !!cpq_queued(cpqd, cpq_group, CPQ_IDLE_PRIO);
	if (prio_cnt < 2)
		return NULL;

	for (prio = CPQ_BE_PRIO; prio <= CPQ_PRIO_MAX; prio++) {
		rq = __cpq_dispatch_request(cpqd, &cpq_group->per_prio[prio],
					   now - cpqd->prio_aging_expire);
		if (rq)
			return rq;
	}

	return NULL;
}

static bool cpq_no_work_for_prio(struct cpq_per_prio *per_prio){
	return list_empty_careful(&per_prio->dispatch) &&
		list_empty_careful(&per_prio->fifo_list[CPQ_READ]) &&
		list_empty_careful(&per_prio->fifo_list[CPQ_WRITE]);
}

static enum hrtimer_restart cpq_idle_slice_timer(struct hrtimer *timer)
{
	struct cpq_data *cpqd = container_of(timer, struct cpq_data,
					     idle_slice_timer);
	unsigned long flags;

	spin_lock_irqsave(&cpqd->lock, flags);
	
	cpqd->wait_request = 0;

	if(cpqd->cpq_log){
		pr_err("cpq:%s %d  pid: %d\n", __func__, __LINE__, current->pid);
	}
	spin_unlock_irqrestore(&cpqd->lock, flags);

	return HRTIMER_NORESTART;
}

static void cpq_arm_slice_timer(struct cpq_data *cpqd)
{
	u64 sl;

	sl = cpqd->cpq_slice_idle;

	cpqd->wait_request = 1;

	if(cpqd->cpq_log){
		pr_err("cpq:%s %d hrtimer_start  pid: %d sl : %llu\n", __func__, __LINE__, current->pid, sl);
	}

	hrtimer_start(&cpqd->idle_slice_timer, ns_to_ktime(sl),
		      HRTIMER_MODE_REL);
}

static bool cpq_group_no_more_request(struct cpq_per_group *cpq_group)
{
	enum cpq_prio prio;

	for (prio = 0; prio <= CPQ_PRIO_MAX; prio++)
		if (!cpq_no_work_for_prio(&cpq_group->per_prio[prio]))
			return false;

	return true;
}


static struct cpq_per_group *cpq_select_group(struct cpq_data *cpqd)
{
	struct cpq_per_group *cpq_group;
	struct cpq_per_group *cpq_off_service_group;
	int off_service_group;
	// first select will start at 0, which is FORE_GROUND

	cpq_group = &cpqd->per_group[cpqd->in_service_group];
 
	off_service_group = !cpqd->in_service_group;
	cpq_off_service_group = &cpqd->per_group[off_service_group];

	if(cpq_group->time_cost >= cpq_group->time_out){
		if(cpq_group_no_more_request(cpq_off_service_group)){
			goto keep_group;
		}
		cpq_group->last_expire_reason = GROUP_TIMEOUT;
		cpq_group->time_cost = 0;
		cpqd->in_service_group = !cpqd->in_service_group;
		return &cpqd->per_group[cpqd->in_service_group];
	}

	if(cpq_group_no_more_request(cpq_group)){
		if(!cpqd->in_service_group && cpqd->wait_request){
			goto keep_group;
		}
		if(!cpqd->in_service_group && !cpqd->wait_request && cpqd->timer_flag && (cpqd->rq_in_driver[0] + cpqd->rq_in_driver[1]) >= cpqd->io_threshold){
			//start timer only if there is a lot I/O pressure in the whole system
			cpq_arm_slice_timer(cpqd);
			cpqd->timer_flag = 0;
			goto keep_group;
		}
		cpq_group->last_expire_reason = GROUP_NO_MORE_REQUEST;
		cpqd->in_service_group = !cpqd->in_service_group;
		return &cpqd->per_group[cpqd->in_service_group];
	}

	if(cpqd->wait_request && cpqd->in_service_group == FORE_GROUND){
		hrtimer_try_to_cancel(&cpqd->idle_slice_timer);
		cpqd->wait_request = 0;
	}

	if(cpqd->in_service_group == BACK_GROUND){
		if(!cpq_group_no_more_request(&cpqd->per_group[FORE_GROUND]) && 
		cpqd->per_group[FORE_GROUND].last_expire_reason == GROUP_NO_MORE_REQUEST){
			cpq_group->last_expire_reason = GROUP_PREEMPTED;
			cpqd->in_service_group = !cpqd->in_service_group;
			return &cpqd->per_group[cpqd->in_service_group];
		}
	}
keep_group:

	return &cpqd->per_group[cpqd->in_service_group];
}

static struct request *cpq_fb_dispatch_request(struct cpq_data *cpqd, struct cpq_per_group *cpq_group)
{
	const unsigned long now = jiffies;
	struct request *rq;
	enum cpq_prio prio;

	rq = cpq_dispatch_prio_aged_requests(cpqd, cpq_group, now);
	if (rq)
		return rq;

	/*
	 * Next, dispatch requests in priority order. Ignore lower priority
	 * requests if any higher priority requests are pending.
	 */
	for (prio = 0; prio <= CPQ_PRIO_MAX; prio++) {
		rq = __cpq_dispatch_request(cpqd, &cpq_group->per_prio[prio], now);
		if (rq || cpq_queued(cpqd, cpq_group, prio))
			return rq;
	}
	return rq;
}

/*
 * Called from blk_mq_run_hw_queue() -> __blk_mq_sched_dispatch_requests().
 *
 * One confusing aspect here is that we get called for a specific
 * hardware queue, but we may return a request that is for a
 * different hardware queue. This is because mq-deadline has shared
 * state for all hardware queues, in terms of sorting, FIFOs, etc.
 */
static struct request *cpq_dispatch_request(struct blk_mq_hw_ctx *hctx)
{
	struct cpq_data *cpqd = hctx->queue->elevator->elevator_data;
	struct cpq_per_group *cpq_group;
	struct request *rq;

	spin_lock_irq(&cpqd->lock);

	cpq_group = cpq_select_group(cpqd);
	rq = cpq_fb_dispatch_request(cpqd, cpq_group);

	if(rq && !rq->elv.priv[1]){
		cpqd->timer_flag = 1;
	}

	spin_unlock_irq(&cpqd->lock);

	return rq;
}

/*
 * Called by __blk_mq_alloc_request(). The shallow_depth value set by this
 * function is used by __blk_mq_get_tag().
 */
static void cpq_limit_depth(blk_opf_t opf, struct blk_mq_alloc_data *data)
{
	struct cpq_data *cpqd = data->q->elevator->elevator_data;

	/* Do not throttle synchronous reads. */
	if (op_is_sync(opf) && !op_is_write(opf))
		return;

	/*
	 * Throttle asynchronous requests and writes such that these requests
	 * do not block the allocation of synchronous requests.
	 */
	data->shallow_depth = cpqd->async_depth;
}

/* Called by blk_mq_update_nr_requests(). */
static void cpq_depth_updated(struct blk_mq_hw_ctx *hctx)
{
	struct request_queue *q = hctx->queue;
	struct cpq_data *cpqd = q->elevator->elevator_data;
	struct blk_mq_tags *tags = hctx->sched_tags;

	cpqd->async_depth = max(1UL, 3 * q->nr_requests / 4);

	sbitmap_queue_min_shallow_depth(&tags->bitmap_tags, cpqd->async_depth);
}

/* Called by blk_mq_init_hctx() and blk_mq_init_sched(). */
static int cpq_init_hctx(struct blk_mq_hw_ctx *hctx, unsigned int hctx_idx)
{
	cpq_depth_updated(hctx);
	return 0;
}

static void cpq_exit_sched(struct elevator_queue *e)
{
	struct cpq_data *cpqd = e->elevator_data;
	enum cpq_prio prio;
	enum cpq_group group;

	for (group = 0; group < GROUND_NUM; group++){
		for (prio = 0; prio <= CPQ_PRIO_MAX; prio++) {
			struct cpq_per_prio *per_prio = &cpqd->per_group[group].per_prio[prio];
			const struct io_stats_per_prio *stats = &per_prio->stats;
			uint32_t queued;

			WARN_ON_ONCE(!list_empty(&per_prio->fifo_list[CPQ_READ]));
			WARN_ON_ONCE(!list_empty(&per_prio->fifo_list[CPQ_WRITE]));

			spin_lock_irq(&cpqd->lock);
			queued = cpq_queued(cpqd, &cpqd->per_group[group], prio);
			spin_unlock_irq(&cpqd->lock);

			WARN_ONCE(queued != 0,
				"statistics for group %s priority %d: i %u m %u d %u c %u\n",
				group ? "back" : "fore",
				prio, stats->inserted, stats->merged,
				stats->dispatched, stats->completed);
		}
	}

	hrtimer_cancel(&cpqd->idle_slice_timer);

	kfree(cpqd);
}

/*
 * initialize elevator private data (cpq_data).
 */
static int cpq_init_sched(struct request_queue *q, struct elevator_type *e)
{
	struct cpq_data *cpqd;
	struct elevator_queue *eq;
	enum cpq_prio prio;
	enum cpq_group group;
	int ret = -ENOMEM;

	eq = elevator_alloc(q, e);
	if (!eq)
		return ret;

	cpqd = kzalloc_node(sizeof(*cpqd), GFP_KERNEL, q->node);
	if (!cpqd)
		goto put_eq;

	eq->elevator_data = cpqd;

	for (group = 0; group < GROUND_NUM; group++){
		for (prio = 0; prio <= CPQ_PRIO_MAX; prio++) {
			struct cpq_per_prio *per_prio = &cpqd->per_group[group].per_prio[prio];

			INIT_LIST_HEAD(&per_prio->dispatch);
			INIT_LIST_HEAD(&per_prio->fifo_list[CPQ_READ]);
			INIT_LIST_HEAD(&per_prio->fifo_list[CPQ_WRITE]);
			per_prio->sort_list[CPQ_READ] = RB_ROOT;
			per_prio->sort_list[CPQ_WRITE] = RB_ROOT;
		}
	}

	hrtimer_init(&cpqd->idle_slice_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	cpqd->idle_slice_timer.function = cpq_idle_slice_timer;

	cpqd->fifo_expire[CPQ_READ] = read_expire;
	cpqd->fifo_expire[CPQ_WRITE] = write_expire;
	cpqd->writes_starved = writes_starved;
	cpqd->front_merges = 1;
	cpqd->last_dir = CPQ_WRITE;
	cpqd->fifo_batch = fifo_batch;
	cpqd->prio_aging_expire = prio_aging_expire;
	cpqd->per_group[FORE_GROUND].time_out = cpq_fore_timeout;
	cpqd->per_group[BACK_GROUND].time_out = cpq_back_timeout;
	cpqd->cpq_slice_idle = cpq_slice_idle;
	cpqd->io_threshold = cpq_io_threshold;

	spin_lock_init(&cpqd->lock);
	spin_lock_init(&cpqd->zone_lock);

	/* We dispatch from request queue wide instead of hw queue */
	blk_queue_flag_set(QUEUE_FLAG_SQ_SCHED, q);

	q->elevator = eq;
	return 0;

put_eq:
	kobject_put(&eq->kobj);
	return ret;
}

/*
 * Try to merge @bio into an existing request. If @bio has been merged into
 * an existing request, store the pointer to that request into *@rq.
 */
static int cpq_request_merge(struct request_queue *q, struct request **rq,
			    struct bio *bio)
{
	struct cpq_data *cpqd = q->elevator->elevator_data;
	const u8 ioprio_class = IOPRIO_PRIO_CLASS(bio->bi_ioprio);
	const enum cpq_prio prio = ioprio_class_to_prio[ioprio_class];
	sector_t sector = bio_end_sector(bio);
	struct request *__rq;

	if (!cpqd->front_merges)
		return ELEVATOR_NO_MERGE;

	__rq = elv_rb_find(&cpqd->per_group[FORE_GROUND].per_prio[prio].sort_list[bio_data_dir(bio)], sector);
	if (__rq) {
		BUG_ON(sector != blk_rq_pos(__rq));

		if (elv_bio_merge_ok(__rq, bio)) {
			*rq = __rq;
			if (blk_discard_mergable(__rq))
				return ELEVATOR_DISCARD_MERGE;
			return ELEVATOR_FRONT_MERGE;
		}
	}

	__rq = elv_rb_find(&cpqd->per_group[BACK_GROUND].per_prio[prio].sort_list[bio_data_dir(bio)], sector);
	if (__rq) {
		BUG_ON(sector != blk_rq_pos(__rq));

		if (elv_bio_merge_ok(__rq, bio)) {
			*rq = __rq;
			if (blk_discard_mergable(__rq))
				return ELEVATOR_DISCARD_MERGE;
			return ELEVATOR_FRONT_MERGE;
		}
	}

	return ELEVATOR_NO_MERGE;
}

/*
 * Attempt to merge a bio into an existing request. This function is called
 * before @bio is associated with a request.
 */
static bool cpq_bio_merge(struct request_queue *q, struct bio *bio,
		unsigned int nr_segs)
{
	struct cpq_data *cpqd = q->elevator->elevator_data;
	struct request *free = NULL;
	bool ret;

	spin_lock_irq(&cpqd->lock);
	ret = blk_mq_sched_try_merge(q, bio, nr_segs, &free);
	spin_unlock_irq(&cpqd->lock);

	if (free)
		blk_mq_free_request(free);

	return ret;
}

/*
 * add rq to rbtree and fifo
 */
static void cpq_insert_request(struct blk_mq_hw_ctx *hctx, struct request *rq,
			      blk_insert_t flags)
{
	struct request_queue *q = hctx->queue;
	struct cpq_data *cpqd = q->elevator->elevator_data;
	const enum cpq_data_dir data_dir = rq_data_dir(rq);
	u16 ioprio = req_get_ioprio(rq);
	u8 ioprio_class = IOPRIO_PRIO_CLASS(ioprio);
	struct cpq_per_prio *per_prio;
	enum cpq_prio prio;
	LIST_HEAD(free);

	spin_lock_irq(&cpqd->lock);
	/*
	 * This may be a requeue of a write request that has locked its
	 * target zone. If it is the case, this releases the zone lock.
	 */
	blk_req_zone_write_unlock(rq);

	prio = ioprio_class_to_prio[ioprio_class];

	cpqd->rq_in_driver[rq_is_sync(rq)]++;

	if(!strncmp(task_css(current, cpuset_cgrp_id)->cgroup->kn->name, "background", 11)){
		per_prio = &cpqd->per_group[BACK_GROUND].per_prio[prio];
		//background rq->elv.priv[1] is non empty
		rq->elv.priv[1] = (void *)(uintptr_t)1;
		if (!rq->elv.priv[0]) {
			per_prio->stats.inserted++;
			rq->elv.priv[0] = (void *)(uintptr_t)1;
		}
	}else{
		per_prio = &cpqd->per_group[FORE_GROUND].per_prio[prio];
		rq->elv.priv[1] = NULL;
		if (!rq->elv.priv[0]) {
			per_prio->stats.inserted++;
			rq->elv.priv[0] = (void *)(uintptr_t)1;
		}
	}

	if (blk_mq_sched_try_insert_merge(q, rq, &free)) {
		spin_unlock_irq(&cpqd->lock);
		blk_mq_free_requests(&free);
		return;
	}

	trace_block_rq_insert(rq);

	if (flags & BLK_MQ_INSERT_AT_HEAD) {
		list_add(&rq->queuelist, &per_prio->dispatch);
		rq->fifo_time = jiffies;
	} else {
		cpq_add_rq_rb(per_prio, rq);

		if (rq_mergeable(rq)) {
			elv_rqhash_add(q, rq);
			if (!q->last_merge)
				q->last_merge = rq;
		}

		/*
		 * set expire time and add to fifo list
		 */
		rq->fifo_time = jiffies + cpqd->fifo_expire[data_dir];
		list_add_tail(&rq->queuelist, &per_prio->fifo_list[data_dir]);
	}
	spin_unlock_irq(&cpqd->lock);
}

/*
 * Called from blk_mq_sched_insert_request() or blk_mq_sched_insert_requests().
 */
static void cpq_insert_requests(struct blk_mq_hw_ctx *hctx,
			       struct list_head *list, blk_insert_t flags)
{
	while (!list_empty(list)) {
		struct request *rq;

		rq = list_first_entry(list, struct request, queuelist);
		list_del_init(&rq->queuelist);
		cpq_insert_request(hctx, rq, flags);
	}
}

/* Callback from inside blk_mq_rq_ctx_init(). */
static void cpq_prepare_request(struct request *rq)
{
	rq->elv.priv[0] = NULL;
	rq->elv.priv[1] = NULL;
}

static bool cpq_has_write_work(struct blk_mq_hw_ctx *hctx)
{
	struct cpq_data *cpqd = hctx->queue->elevator->elevator_data;
	enum cpq_prio p;
	enum cpq_group group;

	for(group =0; group < GROUND_NUM; group++){
		for (p = 0; p <= CPQ_PRIO_MAX; p++)
			if (!list_empty_careful(&cpqd->per_group[group].per_prio[p].fifo_list[CPQ_WRITE]))
				return true;
	}

	return false;
}

/*
 * Callback from inside blk_mq_free_request().
 *
 * For zoned block devices, write unlock the target zone of
 * completed write requests. Do this while holding the zone lock
 * spinlock so that the zone is never unlocked while cpq_fifo_request()
 * or cpq_next_request() are executing. This function is called for
 * all requests, whether or not these requests complete successfully.
 *
 * For a zoned block device, __cpq_dispatch_request() may have stopped
 * dispatching requests if all the queued requests are write requests directed
 * at zones that are already locked due to on-going write requests. To ensure
 * write request dispatch progress in this case, mark the queue as needing a
 * restart to ensure that the queue is run again after completion of the
 * request and zones being unlocked.
 */
static void cpq_finish_request(struct request *rq)
{
	struct request_queue *q = rq->q;
	struct cpq_data *cpqd = q->elevator->elevator_data;
	const u8 ioprio_class = cpq_rq_ioclass(rq);
	const enum cpq_prio prio = ioprio_class_to_prio[ioprio_class];
	struct cpq_per_prio *per_prio;
	struct cpq_per_group *cpq_group;
	unsigned long flags;
	enum cpq_group current_group;

	/*
	 * The block layer core may call cpq_finish_request() without having
	 * called cpq_insert_requests(). Skip requests that bypassed I/O
	 * scheduling. See also blk_mq_request_bypass_insert().
	 */
	if (!rq->elv.priv[0])
		return;

	spin_lock_irqsave(&cpqd->lock, flags);

	if(!rq->elv.priv[1]){
		current_group = FORE_GROUND;
	}else{
		current_group = BACK_GROUND;
	}

	cpq_group = &cpqd->per_group[current_group];

	per_prio = &cpq_group->per_prio[prio];

	per_prio->stats.completed++;
	cpqd->rq_in_driver[rq_is_sync(rq)]--;

	spin_unlock_irqrestore(&cpqd->lock, flags);

	if (blk_queue_is_zoned(q)) {
		unsigned long zflags;
		spin_lock_irqsave(&cpqd->zone_lock, zflags);
		blk_req_zone_write_unlock(rq);
		spin_unlock_irqrestore(&cpqd->zone_lock, zflags);

		if (cpq_has_write_work(rq->mq_hctx))
			blk_mq_sched_mark_restart_hctx(rq->mq_hctx);
	}
}

static bool cpq_has_work_for_prio(struct cpq_per_prio *per_prio)
{
	return !list_empty_careful(&per_prio->dispatch) ||
		!list_empty_careful(&per_prio->fifo_list[CPQ_READ]) ||
		!list_empty_careful(&per_prio->fifo_list[CPQ_WRITE]);
}

static bool cpq_has_work(struct blk_mq_hw_ctx *hctx)
{
	struct cpq_data *cpqd = hctx->queue->elevator->elevator_data;
	enum cpq_prio prio;
	enum cpq_group group;

	for(group = 0; group < GROUND_NUM; group++){
		for (prio = 0; prio <= CPQ_PRIO_MAX; prio++)
			if (cpq_has_work_for_prio(&cpqd->per_group[group].per_prio[prio]))
				return true;
	}

	return false;
}

/*
 * sysfs parts below
 */
#define SHOW_INT(__FUNC, __VAR)						\
static ssize_t __FUNC(struct elevator_queue *e, char *page)		\
{									\
	struct cpq_data *cpqd = e->elevator_data;			\
									\
	return sysfs_emit(page, "%d\n", __VAR);				\
}
#define SHOW_JIFFIES(__FUNC, __VAR) SHOW_INT(__FUNC, jiffies_to_msecs(__VAR))
SHOW_JIFFIES(cpq_read_expire_show, cpqd->fifo_expire[CPQ_READ]);
SHOW_JIFFIES(cpq_write_expire_show, cpqd->fifo_expire[CPQ_WRITE]);
SHOW_JIFFIES(cpq_prio_aging_expire_show, cpqd->prio_aging_expire);
SHOW_INT(cpq_fore_timeout_show, cpqd->per_group[FORE_GROUND].time_out);
SHOW_INT(cpq_back_timeout_show, cpqd->per_group[BACK_GROUND].time_out);
SHOW_INT(cpq_writes_starved_show, cpqd->writes_starved);
SHOW_INT(cpq_front_merges_show, cpqd->front_merges);
SHOW_INT(cpq_async_depth_show, cpqd->async_depth);
SHOW_INT(cpq_fifo_batch_show, cpqd->fifo_batch);
SHOW_INT(cpq_cpq_log_show, cpqd->cpq_log);
SHOW_INT(cpq_io_threshold_show, cpqd->io_threshold);
#undef SHOW_INT
#undef SHOW_JIFFIES

#define STORE_FUNCTION(__FUNC, __PTR, MIN, MAX, __CONV)			\
static ssize_t __FUNC(struct elevator_queue *e, const char *page, size_t count)	\
{									\
	struct cpq_data *cpqd = e->elevator_data;			\
	int __data, __ret;						\
									\
	__ret = kstrtoint(page, 0, &__data);				\
	if (__ret < 0)							\
		return __ret;						\
	if (__data < (MIN))						\
		__data = (MIN);						\
	else if (__data > (MAX))					\
		__data = (MAX);						\
	*(__PTR) = __CONV(__data);					\
	return count;							\
}
#define STORE_INT(__FUNC, __PTR, MIN, MAX)				\
	STORE_FUNCTION(__FUNC, __PTR, MIN, MAX, )
#define STORE_JIFFIES(__FUNC, __PTR, MIN, MAX)				\
	STORE_FUNCTION(__FUNC, __PTR, MIN, MAX, msecs_to_jiffies)
STORE_JIFFIES(cpq_read_expire_store, &cpqd->fifo_expire[CPQ_READ], 0, INT_MAX);
STORE_JIFFIES(cpq_write_expire_store, &cpqd->fifo_expire[CPQ_WRITE], 0, INT_MAX);
STORE_JIFFIES(cpq_prio_aging_expire_store, &cpqd->prio_aging_expire, 0, INT_MAX);
STORE_INT(cpq_fore_timeout_store, &cpqd->per_group[FORE_GROUND].time_out, 0, INT_MAX);
STORE_INT(cpq_back_timeout_store, &cpqd->per_group[BACK_GROUND].time_out, 0, INT_MAX);
STORE_INT(cpq_writes_starved_store, &cpqd->writes_starved, INT_MIN, INT_MAX);
STORE_INT(cpq_front_merges_store, &cpqd->front_merges, 0, 1);
STORE_INT(cpq_async_depth_store, &cpqd->async_depth, 1, INT_MAX);
STORE_INT(cpq_fifo_batch_store, &cpqd->fifo_batch, 0, INT_MAX);
STORE_INT(cpq_cpq_log_store, &cpqd->cpq_log, 0, INT_MAX);
STORE_INT(cpq_io_threshold_store, &cpqd->io_threshold, 0, INT_MAX);
#undef STORE_FUNCTION
#undef STORE_INT
#undef STORE_JIFFIES

static ssize_t cpq_slice_idle_store(struct elevator_queue *e, const char *page, size_t count)	\
{									\
	struct cpq_data *cpqd = e->elevator_data;			\
	int __data, __ret;						\
									\
	__ret = kstrtoint(page, 0, &__data);				\
	if (__ret < 0){							\
		return __ret;}						\
	if (__data < 0){					\
		__data = 0;}				\
	else if (__data > INT_MAX){		\
		__data = INT_MAX;}					\
	cpqd->cpq_slice_idle = (u64)__data * NSEC_PER_MSEC;					\
	return count;							\
}

static ssize_t cpq_slice_idle_show(struct elevator_queue *e, char *page)	\
{									\
	struct cpq_data *cpqd = e->elevator_data;			\
	u64 __data = cpqd->cpq_slice_idle;		\
	__data = div_u64(__data, NSEC_PER_MSEC);			\
	return sprintf(page, "%llu\n", __data);							\
}

#define CPQ_ATTR(name) \
	__ATTR(name, 0644, cpq_##name##_show, cpq_##name##_store)

static struct elv_fs_entry cpq_attrs[] = {
	CPQ_ATTR(read_expire),
	CPQ_ATTR(write_expire),
	CPQ_ATTR(writes_starved),
	CPQ_ATTR(front_merges),
	CPQ_ATTR(async_depth),
	CPQ_ATTR(fifo_batch),
	CPQ_ATTR(prio_aging_expire),
	CPQ_ATTR(fore_timeout),
	CPQ_ATTR(back_timeout),
	CPQ_ATTR(cpq_log),
	CPQ_ATTR(slice_idle),
	CPQ_ATTR(io_threshold),
	__ATTR_NULL
};

static struct elevator_type iosched_cpq_mq = {
	.ops = {
		.depth_updated		= cpq_depth_updated,
		.limit_depth		= cpq_limit_depth,
		.insert_requests	= cpq_insert_requests,
		.dispatch_request	= cpq_dispatch_request,
		.prepare_request	= cpq_prepare_request,
		.finish_request		= cpq_finish_request,
		.next_request		= elv_rb_latter_request,
		.former_request		= elv_rb_former_request,
		.bio_merge		= cpq_bio_merge,
		.request_merge		= cpq_request_merge,
		.requests_merged	= cpq_merged_requests,
		.request_merged		= cpq_request_merged,
		.has_work		= cpq_has_work,
		.init_sched		= cpq_init_sched,
		.exit_sched		= cpq_exit_sched,
		.init_hctx		= cpq_init_hctx,
	},

	.elevator_attrs = cpq_attrs,
	.elevator_name = "cpq",
	.elevator_alias = "cpq",
	.elevator_features = ELEVATOR_F_ZBD_SEQ_WRITE,
	.elevator_owner = THIS_MODULE,
};
MODULE_ALIAS("cpq-iosched");

static int __init cpq_init(void)
{
	return elv_register(&iosched_cpq_mq);
}

static void __exit cpq_exit(void)
{
	elv_unregister(&iosched_cpq_mq);
}

module_init(cpq_init);
module_exit(cpq_exit);

MODULE_AUTHOR("wangshuai12@xiaomi.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("COQ IO scheduler");

