/*
 * IOPS based IO scheduler. Based on CFQ.
 *  Copyright (C) 2003 Jens Axboe <axboe@kernel.dk>
 *  Shaohua Li <shli@kernel.org>
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/jiffies.h>
#include <linux/rbtree.h>
#include <linux/ioprio.h>
#include <linux/blktrace_api.h>
#include "blk.h"

#define VIOS_SCALE_SHIFT 10
#define VIOS_SCALE (1 << VIOS_SCALE_SHIFT)

#define VIOS_READ_SCALE (1)
#define VIOS_WRITE_SCALE (1)
#define VIOS_SYNC_SCALE (2)
#define VIOS_ASYNC_SCALE (5)

#define VIOS_PRIO_SCALE (5)

struct fiops_rb_root {
        struct rb_root rb;
        struct rb_node *left;
        unsigned count;

        u64 min_vios;
};
#define FIOPS_RB_ROOT        (struct fiops_rb_root) { .rb = RB_ROOT}

enum wl_prio_t {
        IDLE_WORKLOAD = 0,
        BE_WORKLOAD = 1,
        RT_WORKLOAD = 2,
        FIOPS_PRIO_NR,
};

struct fiops_data {
        struct request_queue *queue;

        struct fiops_rb_root service_tree[FIOPS_PRIO_NR];

        unsigned int busy_queues;
        unsigned int in_flight[2];

        struct work_struct unplug_work;

        unsigned int read_scale;
        unsigned int write_scale;
        unsigned int sync_scale;
        unsigned int async_scale;
};

struct fiops_ioc {
        struct io_cq icq;

        unsigned int flags;
        struct fiops_data *fiopsd;
        struct rb_node rb_node;
        u64 vios; /* key in service_tree */
        struct fiops_rb_root *service_tree;

        unsigned int in_flight;

        struct rb_root sort_list;
        struct list_head fifo;

        pid_t pid;
        unsigned short ioprio;
        enum wl_prio_t wl_type;
};

#define ioc_service_tree(ioc) (&((ioc)->fiopsd->service_tree[(ioc)->wl_type]))
#define RQ_CIC(rq)                icq_to_cic((rq)->elv.icq)

enum ioc_state_flags {
        FIOPS_IOC_FLAG_on_rr = 0,        /* on round-robin busy list */
        FIOPS_IOC_FLAG_prio_changed,        /* task priority has changed */
};

#define FIOPS_IOC_FNS(name)                                                \
static inline void fiops_mark_ioc_##name(struct fiops_ioc *ioc)        \
{                                                                        \
        ioc->flags |= (1 << FIOPS_IOC_FLAG_##name);                        \
}                                                                        \
static inline void fiops_clear_ioc_##name(struct fiops_ioc *ioc)        \
{                                                                        \
        ioc->flags &= ~(1 << FIOPS_IOC_FLAG_##name);                        \
}                                                                        \
static inline int fiops_ioc_##name(const struct fiops_ioc *ioc)        \
{                                                                        \
        return ((ioc)->flags & (1 << FIOPS_IOC_FLAG_##name)) != 0;        \
}

FIOPS_IOC_FNS(on_rr);
FIOPS_IOC_FNS(prio_changed);
#undef FIOPS_IOC_FNS

#define fiops_log_ioc(fiopsd, ioc, fmt, args...)        \
        blk_add_trace_msg((fiopsd)->queue, "ioc%d " fmt, (ioc)->pid, ##args)
#define fiops_log(fiopsd, fmt, args...)        \
        blk_add_trace_msg((fiopsd)->queue, "fiops " fmt, ##args)

enum wl_prio_t fiops_wl_type(short prio_class)
{
        if (prio_class == IOPRIO_CLASS_RT)
                return RT_WORKLOAD;
        if (prio_class == IOPRIO_CLASS_BE)
                return BE_WORKLOAD;
        return IDLE_WORKLOAD;
}

static inline struct fiops_ioc *icq_to_cic(struct io_cq *icq)
{
        /* cic->icq is the first member, %NULL will convert to %NULL */
        return container_of(icq, struct fiops_ioc, icq);
}

static inline struct fiops_ioc *fiops_cic_lookup(struct fiops_data *fiopsd,
                                               struct io_context *ioc)
{
        if (ioc)
                return icq_to_cic(ioc_lookup_icq(ioc, fiopsd->queue));
        return NULL;
}

/*
 * The below is leftmost cache rbtree addon
 */
static struct fiops_ioc *fiops_rb_first(struct fiops_rb_root *root)
{
        /* Service tree is empty */
        if (!root->count)
                return NULL;

        if (!root->left)
                root->left = rb_first(&root->rb);

        if (root->left)
                return rb_entry(root->left, struct fiops_ioc, rb_node);

        return NULL;
}

static void rb_erase_init(struct rb_node *n, struct rb_root *root)
{
        rb_erase(n, root);
        RB_CLEAR_NODE(n);
}

static void fiops_rb_erase(struct rb_node *n, struct fiops_rb_root *root)
{
        if (root->left == n)
                root->left = NULL;
        rb_erase_init(n, &root->rb);
        --root->count;
}

static inline u64 max_vios(u64 min_vios, u64 vios)
{
        s64 delta = (s64)(vios - min_vios);
        if (delta > 0)
                min_vios = vios;

        return min_vios;
}

static void fiops_update_min_vios(struct fiops_rb_root *service_tree)
{
        struct fiops_ioc *ioc;

        ioc = fiops_rb_first(service_tree);
        if (!ioc)
                return;
        service_tree->min_vios = max_vios(service_tree->min_vios, ioc->vios);
}

/*
 * The fiopsd->service_trees holds all pending fiops_ioc's that have
 * requests waiting to be processed. It is sorted in the order that
 * we will service the queues.
 */
static void fiops_service_tree_add(struct fiops_data *fiopsd,
        struct fiops_ioc *ioc)
{
        struct rb_node **p, *parent;
        struct fiops_ioc *__ioc;
        struct fiops_rb_root *service_tree = ioc_service_tree(ioc);
        u64 vios;
        int left;

        /* New added IOC */
        if (RB_EMPTY_NODE(&ioc->rb_node)) {
                if (ioc->in_flight > 0)
                        vios = ioc->vios;
                else
                        vios = max_vios(service_tree->min_vios, ioc->vios);
        } else {
                vios = ioc->vios;
                /* ioc->service_tree might not equal to service_tree */
                fiops_rb_erase(&ioc->rb_node, ioc->service_tree);
                ioc->service_tree = NULL;
        }

        fiops_log_ioc(fiopsd, ioc, "service tree add, vios %lld", vios);

        left = 1;
        parent = NULL;
        ioc->service_tree = service_tree;
        p = &service_tree->rb.rb_node;
        while (*p) {
                struct rb_node **n;

                parent = *p;
                __ioc = rb_entry(parent, struct fiops_ioc, rb_node);

                /*
                 * sort by key, that represents service time.
                 */
                if (vios <  __ioc->vios)
                        n = &(*p)->rb_left;
                else {
                        n = &(*p)->rb_right;
                        left = 0;
                }

                p = n;
        }

        if (left)
                service_tree->left = &ioc->rb_node;

        ioc->vios = vios;
        rb_link_node(&ioc->rb_node, parent, p);
        rb_insert_color(&ioc->rb_node, &service_tree->rb);
        service_tree->count++;

        fiops_update_min_vios(service_tree);
}

/*
 * Update ioc's position in the service tree.
 */
static void fiops_resort_rr_list(struct fiops_data *fiopsd,
        struct fiops_ioc *ioc)
{
        /*
         * Resorting requires the ioc to be on the RR list already.
         */
        if (fiops_ioc_on_rr(ioc))
                fiops_service_tree_add(fiopsd, ioc);
}

/*
 * add to busy list of queues for service, trying to be fair in ordering
 * the pending list according to last request service
 */
static void fiops_add_ioc_rr(struct fiops_data *fiopsd, struct fiops_ioc *ioc)
{
        BUG_ON(fiops_ioc_on_rr(ioc));
        fiops_mark_ioc_on_rr(ioc);

        fiopsd->busy_queues++;

        fiops_resort_rr_list(fiopsd, ioc);
}

/*
 * Called when the ioc no longer has requests pending, remove it from
 * the service tree.
 */
static void fiops_del_ioc_rr(struct fiops_data *fiopsd, struct fiops_ioc *ioc)
{
        BUG_ON(!fiops_ioc_on_rr(ioc));
        fiops_clear_ioc_on_rr(ioc);

        if (!RB_EMPTY_NODE(&ioc->rb_node)) {
                fiops_rb_erase(&ioc->rb_node, ioc->service_tree);
                ioc->service_tree = NULL;
        }

        BUG_ON(!fiopsd->busy_queues);
        fiopsd->busy_queues--;
}

/*
 * rb tree support functions
 */
static void fiops_del_rq_rb(struct request *rq)
{
        struct fiops_ioc *ioc = RQ_CIC(rq);

        elv_rb_del(&ioc->sort_list, rq);
}

static void fiops_add_rq_rb(struct request *rq)
{
        struct fiops_ioc *ioc = RQ_CIC(rq);
        struct fiops_data *fiopsd = ioc->fiopsd;

        elv_rb_add(&ioc->sort_list, rq);

        if (!fiops_ioc_on_rr(ioc))
                fiops_add_ioc_rr(fiopsd, ioc);
}

static void fiops_reposition_rq_rb(struct fiops_ioc *ioc, struct request *rq)
{
        elv_rb_del(&ioc->sort_list, rq);
        fiops_add_rq_rb(rq);
}

static void fiops_remove_request(struct request *rq)
{
        list_del_init(&rq->queuelist);
        fiops_del_rq_rb(rq);
}

static u64 fiops_scaled_vios(struct fiops_data *fiopsd,
        struct fiops_ioc *ioc, struct request *rq)
{
        int vios = VIOS_SCALE;

        if (rq_data_dir(rq) == WRITE)
                vios = vios * fiopsd->write_scale / fiopsd->read_scale;

        if (!rq_is_sync(rq))
                vios = vios * fiopsd->async_scale / fiopsd->sync_scale;

        vios +=  vios * (ioc->ioprio - IOPRIO_NORM) / VIOS_PRIO_SCALE;

        return vios;
}

/* return vios dispatched */
static u64 fiops_dispatch_request(struct fiops_data *fiopsd,
        struct fiops_ioc *ioc)
{
        struct request *rq;
        struct request_queue *q = fiopsd->queue;

        rq = rq_entry_fifo(ioc->fifo.next);

        fiops_remove_request(rq);
        elv_dispatch_add_tail(q, rq);

        fiopsd->in_flight[rq_is_sync(rq)]++;
        ioc->in_flight++;

        return fiops_scaled_vios(fiopsd, ioc, rq);
}

static int fiops_forced_dispatch(struct fiops_data *fiopsd)
{
        struct fiops_ioc *ioc;
        int dispatched = 0;
        int i;

        for (i = RT_WORKLOAD; i >= IDLE_WORKLOAD; i--) {
                while (!RB_EMPTY_ROOT(&fiopsd->service_tree[i].rb)) {
                        ioc = fiops_rb_first(&fiopsd->service_tree[i]);

                        while (!list_empty(&ioc->fifo)) {
                                fiops_dispatch_request(fiopsd, ioc);
                                dispatched++;
                        }
                        if (fiops_ioc_on_rr(ioc))
                                fiops_del_ioc_rr(fiopsd, ioc);
                }
        }
        return dispatched;
}

static struct fiops_ioc *fiops_select_ioc(struct fiops_data *fiopsd)
{
        struct fiops_ioc *ioc;
        struct fiops_rb_root *service_tree = NULL;
        int i;
        struct request *rq;

        for (i = RT_WORKLOAD; i >= IDLE_WORKLOAD; i--) {
                if (!RB_EMPTY_ROOT(&fiopsd->service_tree[i].rb)) {
                        service_tree = &fiopsd->service_tree[i];
                        break;
                }
        }

        if (!service_tree)
                return NULL;

        ioc = fiops_rb_first(service_tree);

        rq = rq_entry_fifo(ioc->fifo.next);
        /*
         * we are the only async task and sync requests are in flight, delay a
         * moment. If there are other tasks coming, sync tasks have no chance
         * to be starved, don't delay
         */
        if (!rq_is_sync(rq) && fiopsd->in_flight[1] != 0 &&
                        service_tree->count == 1) {
                fiops_log_ioc(fiopsd, ioc,
                                "postpone async, in_flight async %d sync %d",
                                fiopsd->in_flight[0], fiopsd->in_flight[1]);
                return NULL;
        }

        return ioc;
}

static void fiops_charge_vios(struct fiops_data *fiopsd,
        struct fiops_ioc *ioc, u64 vios)
{
        struct fiops_rb_root *service_tree = ioc->service_tree;
        ioc->vios += vios;

        fiops_log_ioc(fiopsd, ioc, "charge vios %lld, new vios %lld", vios, ioc->vios);

        if (RB_EMPTY_ROOT(&ioc->sort_list))
                fiops_del_ioc_rr(fiopsd, ioc);
        else
                fiops_resort_rr_list(fiopsd, ioc);

        fiops_update_min_vios(service_tree);
}

static int fiops_dispatch_requests(struct request_queue *q, int force)
{
        struct fiops_data *fiopsd = q->elevator->elevator_data;
        struct fiops_ioc *ioc;
        u64 vios;

        if (unlikely(force))
                return fiops_forced_dispatch(fiopsd);

        ioc = fiops_select_ioc(fiopsd);
        if (!ioc)
                return 0;

        vios = fiops_dispatch_request(fiopsd, ioc);

        fiops_charge_vios(fiopsd, ioc, vios);
        return 1;
}

static void fiops_init_prio_data(struct fiops_ioc *cic)
{
        struct task_struct *tsk = current;
        struct io_context *ioc = cic->icq.ioc;
        int ioprio_class;

        if (!fiops_ioc_prio_changed(cic))
                return;

        ioprio_class = IOPRIO_PRIO_CLASS(ioc->ioprio);
        switch (ioprio_class) {
        default:
                printk(KERN_ERR "fiops: bad prio %x\n", ioprio_class);
        case IOPRIO_CLASS_NONE:
                /*
                 * no prio set, inherit CPU scheduling settings
                 */
                cic->ioprio = task_nice_ioprio(tsk);
                cic->wl_type = fiops_wl_type(task_nice_ioclass(tsk));
                break;
        case IOPRIO_CLASS_RT:
                cic->ioprio = task_ioprio(ioc);
                cic->wl_type = fiops_wl_type(IOPRIO_CLASS_RT);
                break;
        case IOPRIO_CLASS_BE:
                cic->ioprio = task_ioprio(ioc);
                cic->wl_type = fiops_wl_type(IOPRIO_CLASS_BE);
                break;
        case IOPRIO_CLASS_IDLE:
                cic->wl_type = fiops_wl_type(IOPRIO_CLASS_IDLE);
                cic->ioprio = 7;
                break;
        }

        fiops_clear_ioc_prio_changed(cic);
}

static void fiops_insert_request(struct request_queue *q, struct request *rq)
{
        struct fiops_ioc *ioc = RQ_CIC(rq);

        fiops_init_prio_data(ioc);

        list_add_tail(&rq->queuelist, &ioc->fifo);

        fiops_add_rq_rb(rq);
}

/*
 * scheduler run of queue, if there are requests pending and no one in the
 * driver that will restart queueing
 */
static inline void fiops_schedule_dispatch(struct fiops_data *fiopsd)
{
        if (fiopsd->busy_queues)
                kblockd_schedule_work(fiopsd->queue, &fiopsd->unplug_work);
}

static void fiops_completed_request(struct request_queue *q, struct request *rq)
{
        struct fiops_data *fiopsd = q->elevator->elevator_data;
        struct fiops_ioc *ioc = RQ_CIC(rq);

        fiopsd->in_flight[rq_is_sync(rq)]--;
        ioc->in_flight--;

        fiops_log_ioc(fiopsd, ioc, "in_flight %d, busy queues %d",
                ioc->in_flight, fiopsd->busy_queues);

        if (fiopsd->in_flight[0] + fiopsd->in_flight[1] == 0)
                fiops_schedule_dispatch(fiopsd);
}

static struct request *
fiops_find_rq_fmerge(struct fiops_data *fiopsd, struct bio *bio)
{
        struct task_struct *tsk = current;
        struct fiops_ioc *cic;

        cic = fiops_cic_lookup(fiopsd, tsk->io_context);

        if (cic) {
                sector_t sector = bio->bi_sector + bio_sectors(bio);

                return elv_rb_find(&cic->sort_list, sector);
        }

        return NULL;
}

static int fiops_merge(struct request_queue *q, struct request **req,
                     struct bio *bio)
{
        struct fiops_data *fiopsd = q->elevator->elevator_data;
        struct request *__rq;

        __rq = fiops_find_rq_fmerge(fiopsd, bio);
        if (__rq && elv_rq_merge_ok(__rq, bio)) {
                *req = __rq;
                return ELEVATOR_FRONT_MERGE;
        }

        return ELEVATOR_NO_MERGE;
}

static void fiops_merged_request(struct request_queue *q, struct request *req,
                               int type)
{
        if (type == ELEVATOR_FRONT_MERGE) {
                struct fiops_ioc *ioc = RQ_CIC(req);

                fiops_reposition_rq_rb(ioc, req);
        }
}

static void
fiops_merged_requests(struct request_queue *q, struct request *rq,
                    struct request *next)
{
        struct fiops_ioc *ioc = RQ_CIC(rq);
        struct fiops_data *fiopsd = q->elevator->elevator_data;

        fiops_remove_request(next);

        ioc = RQ_CIC(next);
        /*
         * all requests of this task are merged to other tasks, delete it
         * from the service tree.
         */
        if (fiops_ioc_on_rr(ioc) && RB_EMPTY_ROOT(&ioc->sort_list))
                fiops_del_ioc_rr(fiopsd, ioc);
}

static int fiops_allow_merge(struct request_queue *q, struct request *rq,
                           struct bio *bio)
{
        struct fiops_data *fiopsd = q->elevator->elevator_data;
        struct fiops_ioc *cic;

        /*
         * Lookup the ioc that this bio will be queued with. Allow
         * merge only if rq is queued there.
         */
        cic = fiops_cic_lookup(fiopsd, current->io_context);

        return cic == RQ_CIC(rq);
}

static void fiops_exit_queue(struct elevator_queue *e)
{
        struct fiops_data *fiopsd = e->elevator_data;

        cancel_work_sync(&fiopsd->unplug_work);

        kfree(fiopsd);
}

static void fiops_kick_queue(struct work_struct *work)
{
        struct fiops_data *fiopsd =
                container_of(work, struct fiops_data, unplug_work);
        struct request_queue *q = fiopsd->queue;

        spin_lock_irq(q->queue_lock);
        __blk_run_queue(q);
        spin_unlock_irq(q->queue_lock);
}

static void *fiops_init_queue(struct request_queue *q)
{
        struct fiops_data *fiopsd;
        int i;

        fiopsd = kzalloc_node(sizeof(*fiopsd), GFP_KERNEL, q->node);
        if (!fiopsd)
                return NULL;

        fiopsd->queue = q;

        for (i = IDLE_WORKLOAD; i <= RT_WORKLOAD; i++)
                fiopsd->service_tree[i] = FIOPS_RB_ROOT;

        INIT_WORK(&fiopsd->unplug_work, fiops_kick_queue);

        fiopsd->read_scale = VIOS_READ_SCALE;
        fiopsd->write_scale = VIOS_WRITE_SCALE;
        fiopsd->sync_scale = VIOS_SYNC_SCALE;
        fiopsd->async_scale = VIOS_ASYNC_SCALE;

        return fiopsd;
}

static void fiops_init_icq(struct io_cq *icq)
{
        struct fiops_data *fiopsd = icq->q->elevator->elevator_data;
        struct fiops_ioc *ioc = icq_to_cic(icq);

        RB_CLEAR_NODE(&ioc->rb_node);
        INIT_LIST_HEAD(&ioc->fifo);
        ioc->sort_list = RB_ROOT;

        ioc->fiopsd = fiopsd;

        ioc->pid = current->pid;
        fiops_mark_ioc_prio_changed(ioc);
}

/*
 * sysfs parts below -->
 */
static ssize_t
fiops_var_show(unsigned int var, char *page)
{
        return sprintf(page, "%d\n", var);
}

static ssize_t
fiops_var_store(unsigned int *var, const char *page, size_t count)
{
        char *p = (char *) page;

        *var = simple_strtoul(p, &p, 10);
        return count;
}

#define SHOW_FUNCTION(__FUNC, __VAR)                                        \
static ssize_t __FUNC(struct elevator_queue *e, char *page)                \
{                                                                        \
        struct fiops_data *fiopsd = e->elevator_data;                        \
        return fiops_var_show(__VAR, (page));                                \
}
SHOW_FUNCTION(fiops_read_scale_show, fiopsd->read_scale);
SHOW_FUNCTION(fiops_write_scale_show, fiopsd->write_scale);
SHOW_FUNCTION(fiops_sync_scale_show, fiopsd->sync_scale);
SHOW_FUNCTION(fiops_async_scale_show, fiopsd->async_scale);
#undef SHOW_FUNCTION

#define STORE_FUNCTION(__FUNC, __PTR, MIN, MAX)                                \
static ssize_t __FUNC(struct elevator_queue *e, const char *page, size_t count)        \
{                                                                        \
        struct fiops_data *fiopsd = e->elevator_data;                        \
        unsigned int __data;                                                \
        int ret = fiops_var_store(&__data, (page), count);                \
        if (__data < (MIN))                                                \
                __data = (MIN);                                                \
        else if (__data > (MAX))                                        \
                __data = (MAX);                                                \
        *(__PTR) = __data;                                                \
        return ret;                                                        \
}
STORE_FUNCTION(fiops_read_scale_store, &fiopsd->read_scale, 1, 100);
STORE_FUNCTION(fiops_write_scale_store, &fiopsd->write_scale, 1, 100);
STORE_FUNCTION(fiops_sync_scale_store, &fiopsd->sync_scale, 1, 100);
STORE_FUNCTION(fiops_async_scale_store, &fiopsd->async_scale, 1, 100);
#undef STORE_FUNCTION

#define FIOPS_ATTR(name) \
        __ATTR(name, S_IRUGO|S_IWUSR, fiops_##name##_show, fiops_##name##_store)

static struct elv_fs_entry fiops_attrs[] = {
        FIOPS_ATTR(read_scale),
        FIOPS_ATTR(write_scale),
        FIOPS_ATTR(sync_scale),
        FIOPS_ATTR(async_scale),
        __ATTR_NULL
};

static struct elevator_type iosched_fiops = {
        .ops = {
                .elevator_merge_fn =                fiops_merge,
                .elevator_merged_fn =                fiops_merged_request,
                .elevator_merge_req_fn =        fiops_merged_requests,
                .elevator_allow_merge_fn =        fiops_allow_merge,
                .elevator_dispatch_fn =                fiops_dispatch_requests,
                .elevator_add_req_fn =                fiops_insert_request,
                .elevator_completed_req_fn =        fiops_completed_request,
                .elevator_former_req_fn =        elv_rb_former_request,
                .elevator_latter_req_fn =        elv_rb_latter_request,
                .elevator_init_icq_fn =                fiops_init_icq,
                .elevator_init_fn =                fiops_init_queue,
                .elevator_exit_fn =                fiops_exit_queue,
        },
        .icq_size        =        sizeof(struct fiops_ioc),
        .icq_align        =        __alignof__(struct fiops_ioc),
        .elevator_attrs =        fiops_attrs,
        .elevator_name =        "fiops",
        .elevator_owner =        THIS_MODULE,
};

static int __init fiops_init(void)
{
        return elv_register(&iosched_fiops);
}

static void __exit fiops_exit(void)
{
        elv_unregister(&iosched_fiops);
}

module_init(fiops_init);
module_exit(fiops_exit);

MODULE_AUTHOR("Jens Axboe, Shaohua Li <shli@kernel.org>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("IOPS based IO scheduler");
