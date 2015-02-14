/*
* V(R) I/O Scheduler
*
* Copyright (C) 2007 Aaron Carroll <aaronc@gelato.unsw.edu.au>
*
*
* The algorithm:
*
* The next request is decided based on its distance from the last
* request, with a multiplicative penalty of `rev_penalty' applied
* for reversing the head direction. A rev_penalty of 1 means SSTF
* behaviour. As this variable is increased, the algorithm approaches
* pure SCAN. Setting rev_penalty to 0 forces SCAN.
*
* Async and synch requests are not treated seperately. Instead we
* rely on deadlines to ensure fairness.
*
*/
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/compiler.h>
#include <linux/rbtree.h>

#include <asm/div64.h>

enum vr_data_dir {
        ASYNC,
        SYNC,
};

enum vr_head_dir {
        FORWARD,
        BACKWARD,
};

static const int sync_expire = HZ / 2; /* max time before a sync is submitted. */
static const int async_expire = 5 * HZ; /* ditto for async, these limits are SOFT! */
static const int fifo_batch = 1;
static const int rev_penalty = 10; /* penalty for reversing head direction */

struct vr_data {
struct rb_root sort_list;
struct list_head fifo_list[2];

struct request *next_rq;
struct request *prev_rq;

unsigned int nbatched;
sector_t last_sector; /* head position */
int head_dir;

/* tunables */
int fifo_expire[2];
int fifo_batch;
int rev_penalty;
};

static void vr_move_request(struct vr_data *, struct request *);

static inline struct vr_data *
vr_get_data(struct request_queue *q)
{
        return q->elevator->elevator_data;
}

static void
vr_add_rq_rb(struct vr_data *vd, struct request *rq)
{
        elv_rb_add(&vd->sort_list, rq);

        if (blk_rq_pos(rq) >= vd->last_sector) {
                if (!vd->next_rq || blk_rq_pos(vd->next_rq) > blk_rq_pos(rq))
                        vd->next_rq = rq;
        } else {
                if (!vd->prev_rq || blk_rq_pos(vd->prev_rq) < blk_rq_pos(rq))
                        vd->prev_rq = rq;
        }

        BUG_ON(vd->next_rq && vd->next_rq == vd->prev_rq);
        BUG_ON(vd->next_rq && vd->prev_rq && blk_rq_pos(vd->next_rq) < blk_rq_pos(vd->prev_rq));
}

static void
vr_del_rq_rb(struct vr_data *vd, struct request *rq)
{
        /*
        * We might be deleting our cached next request.
        * If so, find its sucessor.
        */

        if (vd->next_rq == rq)
                vd->next_rq = elv_rb_latter_request(NULL, rq);
        else if (vd->prev_rq == rq)
                vd->prev_rq = elv_rb_former_request(NULL, rq);

        BUG_ON(vd->next_rq && vd->next_rq == vd->prev_rq);
        BUG_ON(vd->next_rq && vd->prev_rq && blk_rq_pos(vd->next_rq) < blk_rq_pos(vd->prev_rq));

        elv_rb_del(&vd->sort_list, rq);
}

/*
 * add rq to rbtree and fifo
 */
static void
vr_add_request(struct request_queue *q, struct request *rq)
{
        struct vr_data *vd = vr_get_data(q);
        const int dir = rq_is_sync(rq);

        vr_add_rq_rb(vd, rq);
        
        if (vd->fifo_expire[dir]) {
                rq_set_fifo_time(rq, jiffies + vd->fifo_expire[dir]);
                list_add_tail(&rq->queuelist, &vd->fifo_list[dir]);
        }
}

/*
 * remove rq from rbtree and fifo.
 */
static void
vr_remove_request(struct request_queue *q, struct request *rq)
{
        struct vr_data *vd = vr_get_data(q);

        rq_fifo_clear(rq);
        vr_del_rq_rb(vd, rq);
}

static int
vr_merge(struct request_queue *q, struct request **rqp, struct bio *bio)
{
        sector_t sector = bio->bi_sector + bio_sectors(bio);
        struct vr_data *vd = vr_get_data(q);
        struct request *rq = elv_rb_find(&vd->sort_list, sector);
        
        if (rq && elv_rq_merge_ok(rq, bio)) {
                *rqp = rq;
                return ELEVATOR_FRONT_MERGE;
        }
        return ELEVATOR_NO_MERGE;
}

static void
vr_merged_request(struct request_queue *q, struct request *req, int type)
{
        struct vr_data *vd = vr_get_data(q);

        /*
         * if the merge was a front merge, we need to reposition request
         */
        if (type == ELEVATOR_FRONT_MERGE) {
                vr_del_rq_rb(vd, req);
                vr_add_rq_rb(vd, req);
        }
}

static void
vr_merged_requests(struct request_queue *q, struct request *rq,
struct request *next)
{
        /*
         * if next expires before rq, assign its expire time to rq
         * and move into next position (next will be deleted) in fifo
         */
        if (!list_empty(&rq->queuelist) && !list_empty(&next->queuelist)) {
                if (time_before(rq_fifo_time(next), rq_fifo_time(rq))) {
                        list_move(&rq->queuelist, &next->queuelist);
                        rq_set_fifo_time(rq, rq_fifo_time(next));
                }
        }

        vr_remove_request(q, next);
}

/*
 * move an entry to dispatch queue
 */
static void
vr_move_request(struct vr_data *vd, struct request *rq)
{
        struct request_queue *q = rq->q;

        if (blk_rq_pos(rq) > vd->last_sector)
                vd->head_dir = FORWARD;
        else
                vd->head_dir = BACKWARD;

        vd->last_sector = blk_rq_pos(rq);
        vd->next_rq = elv_rb_latter_request(NULL, rq);
        vd->prev_rq = elv_rb_former_request(NULL, rq);

        BUG_ON(vd->next_rq && vd->next_rq == vd->prev_rq);

        vr_remove_request(q, rq);
        elv_dispatch_add_tail(q, rq);
        vd->nbatched++;
}

/*
 * get the first expired request in direction ddir
 */
static struct request *
vr_expired_request(struct vr_data *vd, int ddir)
{
        struct request *rq;

        if (list_empty(&vd->fifo_list[ddir]))
                return NULL;

        rq = rq_entry_fifo(vd->fifo_list[ddir].next);
        if (time_after(jiffies, rq_fifo_time(rq)))
                return rq;

        return NULL;
}

/*
 * Returns the oldest expired request
 */
static struct request *
vr_check_fifo(struct vr_data *vd)
{
        struct request *rq_sync = vr_expired_request(vd, SYNC);
        struct request *rq_async = vr_expired_request(vd, ASYNC);

        if (rq_async && rq_sync) {
                if (time_after(rq_fifo_time(rq_async), rq_fifo_time(rq_sync)))
                        return rq_sync;
        }
        else if (rq_sync)
                return rq_sync;

        return rq_async;
}

/*
* Return the request with the lowest penalty
*/
static struct request *
vr_choose_request(struct vr_data *vd)
{
        int penalty = (vd->rev_penalty) ? : INT_MAX;
        struct request *next = vd->next_rq;
        struct request *prev = vd->prev_rq;
        sector_t next_pen, prev_pen;
        
        BUG_ON(prev && prev == next);

        if (!prev)
                return next;
        else if (!next)
                return prev;

/* At this point both prev and next are defined and distinct */

        next_pen = blk_rq_pos(next) - vd->last_sector;
        prev_pen = vd->last_sector - blk_rq_pos(prev);

        if (vd->head_dir == FORWARD)
                next_pen = do_div(next_pen, penalty);
        else
                prev_pen = do_div(prev_pen, penalty);

        if (next_pen <= prev_pen)
                return next;

        return prev;
}

static int
vr_dispatch_requests(struct request_queue *q, int force)
{
        struct vr_data *vd = vr_get_data(q);
        struct request *rq = NULL;

/* Check for and issue expired requests */
        if (vd->nbatched > vd->fifo_batch) {
                vd->nbatched = 0;
                rq = vr_check_fifo(vd);
        }

        if (!rq) {
                rq = vr_choose_request(vd);
                if (!rq)
                        return 0;
        }

        vr_move_request(vd, rq);

        return 1;
}


static void
vr_exit_queue(struct elevator_queue *e)
{
        struct vr_data *vd = e->elevator_data;
        BUG_ON(!RB_EMPTY_ROOT(&vd->sort_list));
        kfree(vd);
}

/*
* initialize elevator private data (vr_data).
*/
static void *vr_init_queue(struct request_queue *q)
{
        struct vr_data *vd;
        
        vd = kmalloc_node(sizeof(*vd), GFP_KERNEL | __GFP_ZERO, q->node);
        if (!vd)
                return NULL;

        INIT_LIST_HEAD(&vd->fifo_list[SYNC]);
        INIT_LIST_HEAD(&vd->fifo_list[ASYNC]);
        vd->sort_list = RB_ROOT;
        vd->fifo_expire[SYNC] = sync_expire;
        vd->fifo_expire[ASYNC] = async_expire;
        vd->fifo_batch = fifo_batch;
        vd->rev_penalty = rev_penalty;
        return vd;
}

/*
 * sysfs parts below
 */

static ssize_t
vr_var_show(int var, char *page)
{
        return sprintf(page, "%d\n", var);
}

static ssize_t
vr_var_store(int *var, const char *page, size_t count)
{
        *var = simple_strtol(page, NULL, 10);
        return count;
}

#define SHOW_FUNCTION(__FUNC, __VAR, __CONV) \
static ssize_t __FUNC(struct elevator_queue *e, char *page) \
{ \
struct vr_data *vd = e->elevator_data; \
int __data = __VAR; \
if (__CONV) \
__data = jiffies_to_msecs(__data); \
return vr_var_show(__data, (page)); \
}
SHOW_FUNCTION(vr_sync_expire_show, vd->fifo_expire[SYNC], 1);
SHOW_FUNCTION(vr_async_expire_show, vd->fifo_expire[ASYNC], 1);
SHOW_FUNCTION(vr_fifo_batch_show, vd->fifo_batch, 0);
SHOW_FUNCTION(vr_rev_penalty_show, vd->rev_penalty, 0);
#undef SHOW_FUNCTION

#define STORE_FUNCTION(__FUNC, __PTR, MIN, MAX, __CONV) \
static ssize_t __FUNC(struct elevator_queue *e, const char *page, size_t count) \
{ \
struct vr_data *vd = e->elevator_data; \
int __data; \
int ret = vr_var_store(&__data, (page), count); \
if (__data < (MIN)) \
__data = (MIN); \
else if (__data > (MAX)) \
__data = (MAX); \
if (__CONV) \
*(__PTR) = msecs_to_jiffies(__data); \
else \
*(__PTR) = __data; \
return ret; \
}
STORE_FUNCTION(vr_sync_expire_store, &vd->fifo_expire[SYNC], 0, INT_MAX, 1);
STORE_FUNCTION(vr_async_expire_store, &vd->fifo_expire[ASYNC], 0, INT_MAX, 1);
STORE_FUNCTION(vr_fifo_batch_store, &vd->fifo_batch, 0, INT_MAX, 0);
STORE_FUNCTION(vr_rev_penalty_store, &vd->rev_penalty, 0, INT_MAX, 0);
#undef STORE_FUNCTION

#define DD_ATTR(name) \
__ATTR(name, S_IRUGO|S_IWUSR, vr_##name##_show, \
vr_##name##_store)

static struct elv_fs_entry vr_attrs[] = {
        DD_ATTR(sync_expire),
        DD_ATTR(async_expire),
        DD_ATTR(fifo_batch),
        DD_ATTR(rev_penalty),
        __ATTR_NULL
};

static struct elevator_type iosched_vr = {
        .ops = {
                .elevator_merge_fn = vr_merge,
                .elevator_merged_fn = vr_merged_request,
                .elevator_merge_req_fn = vr_merged_requests,
                .elevator_dispatch_fn = vr_dispatch_requests,
                .elevator_add_req_fn = vr_add_request,
                .elevator_former_req_fn = elv_rb_former_request,
                .elevator_latter_req_fn = elv_rb_latter_request,
                .elevator_init_fn = vr_init_queue,
                .elevator_exit_fn = vr_exit_queue,
        },

        .elevator_attrs = vr_attrs,
        .elevator_name = "vr",
        .elevator_owner = THIS_MODULE,
};
        
static int __init vr_init(void)
{
        elv_register(&iosched_vr);

        return 0;
}

static void __exit vr_exit(void)
{
        elv_unregister(&iosched_vr);
}

module_init(vr_init);
module_exit(vr_exit);

MODULE_AUTHOR("Aaron Carroll");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("V(R) IO scheduler");
