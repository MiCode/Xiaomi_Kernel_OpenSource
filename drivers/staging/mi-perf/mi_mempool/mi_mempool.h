#ifndef _MI_MEMPOOL_MODULE_H_
#define _MI_MEMPOOL_MODULE_H_

#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/kobject.h>

/* Default refill mark for every page pool. */
#define MI_DYNAMIC_POOL_LOW_MARK_PERCENT 40UL
/* The minimum interval to awake the refill_worker. */
#define MI_DYNAMIC_POOL_REFILL_MIN_INTERVAL_MS 300
/*
 * The minimum interval for refill_worker to check the watermark
 * before memory allocation request.
 */
#define MI_DYNAMIC_POOL_REFILL_MIN_CHECK_INTERVAL_MS 10
/* Nice value of the refill_worker. */
#define MI_DYNAMIC_POOL_REFILL_WORKER_NICE_VAL 10
/* Default threshold of the system to use the reserved memory. */
#define MI_DYNAMIC_POOL_DEFAULT_DIRECT_RELCAIM_THRESHOLD_MB 11UL
/* Default limit for every process to use the reserved memory. */
#define MI_DYNAMIC_POOL_OOM_DEFAULT_LOW_PERCENT 10
/* Max retries for first refill. */
#define MI_DYNAMIC_POOL_MAX_FIRST_REFILL_RETRIES 16

#define BYTES_PER_MB (1024 * 1024)

#define MI_MEMPOOL_ATTR_RO(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0440,	\
		_name##_show, NULL)

#define MI_MEMPOOL_ATTR_RW(_name)	\
	struct kobj_attribute kobj_attr_##_name =	\
		__ATTR(_name, 0660,	\
		_name##_show, _name##_store)

#define HIGH_ORDER_GFP  ((GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN \
				| __GFP_NORETRY) & ~__GFP_RECLAIM)
#define LOW_ORDER_GFP (GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN)

struct oom_low_mark_policy {
	int order;
	int process_id; /* -1 indicates all processes in whitelist. */
	int oom_low_mark_percent;
};

struct proc_node {
	struct rb_node rb_node;
	pid_t pid;
	int process_id;
};

static gfp_t order_flags[] = {LOW_ORDER_GFP, HIGH_ORDER_GFP, HIGH_ORDER_GFP,
	HIGH_ORDER_GFP};
static const unsigned int orders[] = {0, 1, 4, 5}; /* Orders for mi_mempool to reserve. */
static const unsigned int high_mark_mb[] = {32, 1, 1, 1};/* Reserved upper limit */
static unsigned int order_map[MAX_ORDER + 1];
static const unsigned int refill_limited_order[] = {4, 5};
#define NUM_ORDERS ARRAY_SIZE(orders)
#define NUM_LIMITED_ORDER ARRAY_SIZE(refill_limited_order)
#define INVALID_ADJ -2000
/* The comm of processes in  whitelist. */
static char *comm_whitelist[] = {"composer", "surfaceflinger",
		"com.miui.home", "com.android.systemui", "system_server",
		".globallauncher", NULL};
#define WHITELIST_LEN (ARRAY_SIZE(comm_whitelist) - 1)
static int comm_whitelist_sizes[WHITELIST_LEN];
/* The oom_adj_score of processes in  whitelist. */
static int adj_whitelist[] = {-1000, -1000,  INVALID_ADJ, INVALID_ADJ, -900,
			INVALID_ADJ};
/* Process use limit policy  */
static struct oom_low_mark_policy oom_low_mark_policys[] = {{1, -1,
	MI_DYNAMIC_POOL_LOW_MARK_PERCENT}, {0, 4, 60},
        {4, -1, 100}, {5, -1, 100}};

enum mi_mempool_config {
	NEED_PROCESS_USE_COUNT = 1,
	DEBUG_LOG = 2,
	DEBUG_WHITELIST_CHECK = 4,
};

enum process_use_count {
	PROCESS_RECLAIM_SUCCESS,
	PROCESS_RECLAIM_FAILED,
	PROCESS_OOM_SUCCESS,
	PROCESS_OOM_FAILED,
	NR_PROCESS_COUNT_STATE
};

struct mi_dynamic_page_pool {
	/* Used for protecting page list. */
	spinlock_t lock;
	gfp_t gfp_mask;
	int order;
	struct list_head list;
	atomic_t count;
	atomic_t reclaim_use_count;
	atomic_t oom_use_count;
	int low_mark;
	int high_mark;
	ktime_t last_check_watermark_time;
	int oom_low_mark[WHITELIST_LEN];
	bool first_filled;
	int first_fill_retries;
	atomic_t process_use_counts[WHITELIST_LEN][NR_PROCESS_COUNT_STATE];
};

struct mi_mempool {
	struct mi_dynamic_page_pool **pools;
	struct task_struct *refill_worker;
	atomic_long_t last_refill_time;
	int last_refill_count;
	int history_refill_count;
	unsigned long direct_relcaim_pages_threshold;
	bool has_lowmark_pool;
	bool refill_check_wakeup;
	/* Used for protecting refill_worker_running. */
	spinlock_t refill_wakeup_lock;
	/*
	 * Represent the status of refill_worker,
	 * which helps to tell how the refill_worker are
	 * awakened.
	 */
	bool refill_worker_running;
	/*
	 * Configuration for mi_mempool to output more
	 * detail info:
	 * 1 - enable statistics for per process in whitelist
	 * 2 - enable debug log
	 * 4 - enable statistics for whitelist check
	 */
	int config;
	struct kobject *kobj;
	atomic_t total_page_num;
	atomic_t oom_use_failure_count[MAX_ORDER];

	atomic_t whitelist_check_num;
	atomic_t proc_node_alloc_failure;
	int proc_tree_size;
};

#endif
