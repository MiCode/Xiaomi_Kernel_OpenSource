
#define pr_fmt(fmt)  "rtmm : " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/preempt.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/vmstat.h>
#include <linux/swap.h>
#include <linux/compaction.h>
#include <linux/freezer.h>

#include <linux/ktrace.h>
#include <linux/rtmm.h>

#define MP_NAME_LEN 12

enum {
	NR_THREAD_ALLOC_FROM_BUDDY = 0,
	NR_ALLOC_FROM_POOL,
	NR_ALLOC_FROM_BUDDY,

	NR_FREE_TO_POOL,
	NR_FREE_TO_BUDDY,

	NR_THREAD_ALLOC_FAIL,

	NR_WAKEUP_FROM_ALLOC,
	NR_WAKEUP_FROM_FREE,
	NR_WAKEUP_FROM_SET_MAXNR,

	NR_STAT
};

static const char *stat_str[NR_STAT] = {
	[NR_THREAD_ALLOC_FROM_BUDDY] = "thread alloc",
	[NR_ALLOC_FROM_POOL] = "alloc from pool",
	[NR_ALLOC_FROM_BUDDY] = "alloc from buddy",

	[NR_FREE_TO_POOL] = "free to pool",
	[NR_FREE_TO_BUDDY] = "free to buddy",

	[NR_THREAD_ALLOC_FAIL] = "thread alloc fail",

	[NR_WAKEUP_FROM_ALLOC] = "wakeup from alloc",
	[NR_WAKEUP_FROM_FREE] = "wakeup from free",
	[NR_WAKEUP_FROM_SET_MAXNR] = "wakeup from set maxnr",
};

struct mpool {
	atomic_t init;
	gfp_t gfp_mask;
	int order;

	atomic_t cur_nr;
	atomic_t max_nr;
	atomic_t low_wm_nr;

	atomic_t stat[NR_STAT];

	struct list_head list;
	spinlock_t lock;

	int pool_type;
	struct dentry *dir;
	struct rtmm_pool *pool;

	struct page *(*__alloc_pages)(void);
	void (*__free_pages)(void *addr);
};

struct rtmm_pool {
	struct dentry *dir;

	struct mpool mp[RTMM_POOL_NR];

	struct task_struct *tsk;
	wait_queue_head_t waitqueue;
};

static struct page *mp_alloc_pages_ti(void);
static void mp_free_pages_ti(void *addr);

static struct rtmm_pool __pool = {
	.mp = {
		[RTMM_POOL_THREADINFO] = {
			.init = ATOMIC_INIT(0),
			.max_nr = ATOMIC_INIT(40),
			.__alloc_pages = mp_alloc_pages_ti,
			.__free_pages = mp_free_pages_ti,
		}
	}
};

static inline struct rtmm_pool *get_pool(void)
{
	return &__pool;
}

static inline bool __mp_is_not_full(struct mpool *mp)
{
	return atomic_read(&mp->cur_nr) < atomic_read(&mp->max_nr);
}

static inline bool __mp_is_not_empty(struct mpool *mp)
{
	return atomic_read(&mp->cur_nr) > 0;
}

static inline bool __mp_is_below_wm(struct mpool *mp)
{
	return atomic_read(&mp->cur_nr) < atomic_read(&mp->low_wm_nr);
}

static inline bool __pool_thread_running(struct task_struct *tsk)
{
	return tsk && (tsk->state == TASK_RUNNING);
}

static inline void __mp_wakeup(struct mpool *mp, int type)
{
	wake_up(&mp->pool->waitqueue);

	atomic_inc(&mp->stat[type]);
}

static struct page *mp_alloc_pages(struct mpool *mp)
{
	struct page *page = NULL;

	if (__mp_is_not_empty(mp)) {
		spin_lock(&mp->lock);
		if (!list_empty(&mp->list)) {
			page = list_first_entry(&mp->list, struct page , lru);
			list_del(&page->lru);
		}
		spin_unlock(&mp->lock);
	}

	if (page) {
		atomic_dec(&mp->cur_nr);
		atomic_inc(&mp->stat[NR_ALLOC_FROM_POOL]);
	}

	if (__mp_is_below_wm(mp) && !__pool_thread_running(mp->pool->tsk))
		__mp_wakeup(mp, NR_WAKEUP_FROM_ALLOC);

	return page;
}

static void mp_free_pages(struct mpool *mp, void *addr)
{
	struct page *page = virt_to_page(addr);

	spin_lock(&mp->lock);
	list_add(&page->lru, &mp->list);
	spin_unlock(&mp->lock);

	atomic_inc(&mp->cur_nr);
	atomic_inc(&mp->stat[NR_FREE_TO_POOL]);

	if (__mp_is_below_wm(mp) && !__pool_thread_running(mp->pool->tsk))
		__mp_wakeup(mp, NR_WAKEUP_FROM_FREE);
}

static struct page *mp_alloc_pages_ti(void)
{
	return alloc_kmem_pages_node(NUMA_NO_NODE, THREADINFO_GFP,
			THREAD_SIZE_ORDER);
}

static void mp_free_pages_ti(void *addr)
{

	kasan_alloc_pages(virt_to_page(addr), THREAD_SIZE_ORDER);
	free_kmem_pages((unsigned long)addr, THREAD_SIZE_ORDER);
}


static int __pool_is_not_full(struct rtmm_pool *pool)
{
	int i;
	int not_full = 0;

	for (i = 0; i < RTMM_POOL_NR; i++) {
		if (__mp_is_not_full(&pool->mp[i])) {
			not_full = 1;
			break;
		}
	}

	return not_full;
}

struct page *rtmm_alloc_pages(int pool_type)
{
	struct rtmm_pool *pool = get_pool();
	struct mpool *mp = &pool->mp[pool_type];
	struct page *page = NULL;

	if (1 || ktrace_sched_match_pid(current->pid))
		page = mp_alloc_pages(mp);

	if (!page) {
		page = mp->__alloc_pages();

		atomic_inc(&mp->stat[NR_ALLOC_FROM_BUDDY]);
	}

	return page;
}

void rtmm_free_pages(void *addr, int pool_type)
{
	struct rtmm_pool *pool = get_pool();
	struct mpool *mp = &pool->mp[pool_type];

	if (atomic_read(&mp->init) && __mp_is_not_full(mp)) {
		mp_free_pages(mp, addr);
	} else {
		mp->__free_pages(addr);
		atomic_inc(&mp->stat[NR_FREE_TO_BUDDY]);
	}
}

static int mp_thread(void *data)
{
	struct rtmm_pool *pool = data;
	struct sched_param param = { .sched_priority = 0 };

	sched_setscheduler(pool->tsk, SCHED_NORMAL, &param);

	while (true) {
		int i;

		wait_event_freezable(pool->waitqueue, __pool_is_not_full(pool));

		for (i = 0; i < RTMM_POOL_NR; i++) {
			struct mpool *mp = &pool->mp[i];

			while (__mp_is_not_full(mp)) {
				struct page *page = mp->__alloc_pages();

				if (!page) {
					atomic_inc(&mp->stat[NR_THREAD_ALLOC_FAIL]);
					continue;
				}

				spin_lock(&mp->lock);
				list_add_tail(&page->lru, &mp->list);
				spin_unlock(&mp->lock);

				atomic_inc(&mp->cur_nr);
				atomic_inc(&mp->stat[NR_THREAD_ALLOC_FROM_BUDDY]);
			}
		}
	}

	return 0;
}

static int mp_maxnr_read(void *data, u64 *val)
{
	struct mpool *mp = (struct mpool *)data;

	*val = atomic_read(&mp->max_nr);

	return 0;
}

static int mp_maxnr_write(void *data, u64 val)
{
	struct mpool *mp = (struct mpool *)data;

	int old_nr = atomic_read(&mp->max_nr);
	int new_nr = (unsigned int)val;

	atomic_set(&mp->max_nr, val);

	if (old_nr < new_nr)
		__mp_wakeup(mp, NR_WAKEUP_FROM_SET_MAXNR);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(pool_max_nr, mp_maxnr_read,
	mp_maxnr_write, "%llu\n");

static int mp_stats_show(struct seq_file *s, void *v)
{
	struct mpool *mp = s->private;
	int i;

	seq_puts(s, "\n");

	seq_printf(s, "max %d, low_wm %d, cur %d\n",
			atomic_read(&mp->max_nr),
			atomic_read(&mp->low_wm_nr),
			atomic_read(&mp->cur_nr));

	seq_puts(s, "----\n");

	for (i = 0; i < NR_STAT; i++) {
		seq_printf(s, "  %-25s: %d\n",
				stat_str[i],
				atomic_read(&mp->stat[i]));
	}

	seq_puts(s, "----\n");
	seq_printf(s, "  pool hit rate %d%%\n",
			100 * atomic_read(&mp->stat[NR_ALLOC_FROM_POOL]) /
			(atomic_read(&mp->stat[NR_ALLOC_FROM_POOL]) +
			atomic_read(&mp->stat[NR_ALLOC_FROM_BUDDY])));

	return 0;
}

static int mp_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, mp_stats_show, inode->i_private);
}

static const struct file_operations mp_stat_ops = {
	.open           = mp_stats_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static void __init mp_init(struct rtmm_pool *pool)
{
	int i;

	for (i = 0; i < RTMM_POOL_NR; i++) {
		struct mpool *mp = &pool->mp[i];
		int j;

		mp->pool = pool;
		mp->pool_type = i;

		spin_lock_init(&mp->lock);
		INIT_LIST_HEAD(&mp->list);
		atomic_set(&mp->cur_nr, 0);
		atomic_set(&mp->low_wm_nr, atomic_read(&mp->max_nr) * 3 / 4);

		for (j = 0; j < NR_STAT; j++)
			atomic_set(&mp->stat[j], 0);

		if (pool->dir) {
			char mp_dir_name[MP_NAME_LEN];

			snprintf(mp_dir_name , MP_NAME_LEN - 1, "pool-%d", mp->pool_type);
			mp->dir = debugfs_create_dir(mp_dir_name, pool->dir);

			if (mp->dir) {
				debugfs_create_file("pool_max_nr", S_IRUGO | S_IWUSR, mp->dir, mp,
						&pool_max_nr);

				debugfs_create_file("stat", S_IFREG | S_IRUGO,
						mp->dir, mp, &mp_stat_ops);
			}
		}

		atomic_set(&mp->init, 1);
	}

}

int __init rtmm_pool_init(struct dentry *dir)
{
	struct rtmm_pool *pool = get_pool();

	memset(pool, sizeof(struct rtmm_pool), 0);

	pool->dir = debugfs_create_dir("pool", dir);
	if (!pool->dir) {
		pr_err("fail to create debugfs dir\n");
	}

	init_waitqueue_head(&pool->waitqueue);

	mp_init(pool);
	pool->tsk = kthread_run(mp_thread, pool, "rtmm_pool");
	if (IS_ERR(pool->tsk)) {
		pr_err("%s: creating thread for rtmm pool failed\n",
				__func__);
		return PTR_ERR_OR_ZERO(pool->tsk);
	}

	smp_mb();

	pr_info("rtmm mp init OK\n");

	return 0;
}

