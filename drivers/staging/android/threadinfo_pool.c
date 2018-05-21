#define pr_fmt(fmt)  "threadinfo_pool : " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/vmstat.h>
#include <linux/freezer.h>
#include <linux/threadinfo_pool.h>

#define MP_NAME_LEN 12
enum {
	NR_ALLOC_FROM_POOL = 0,
	NR_ALLOC_FROM_BUDDY,
	NR_STAT,
};

static const char *stat_str[NR_STAT] = {
	[NR_ALLOC_FROM_POOL] = "alloc from pool",
	[NR_ALLOC_FROM_BUDDY] = "alloc from buddy",
};

struct mpool {
	atomic_t init;
	unsigned int cur_nr;
	unsigned int max_nr;
	unsigned int low_wm_nr;
	unsigned int stat[NR_STAT];

	struct list_head list;
	spinlock_t lock; /*mp lru lock */
	u64 freeze_time;
	u64 freeze_duration;
	struct dentry *dir;
	struct task_struct *tsk;
	wait_queue_head_t waitqueue;
} _pool = {
	.init = ATOMIC_INIT(0),
	.max_nr = 40,
};

static inline bool __mp_is_not_full(void)
{
	int ret;
	spin_lock(&_pool.lock);
	ret = (_pool.cur_nr < _pool.max_nr);
	spin_unlock(&_pool.lock);
	return ret;
}

static inline bool __pool_thread_running(void)
{
	return _pool.tsk && (_pool.tsk->state == TASK_RUNNING);
}

static struct page *mp_alloc_pages(void)
{
	struct page *page = NULL;
	int below_wm = 0;

	if (!atomic_read(&_pool.init))
		return page;

	spin_lock(&_pool.lock);
	if (_pool.cur_nr > 0) {
		if (!list_empty(&_pool.list)) {
			page = list_first_entry(&_pool.list, struct page, lru);
			list_del(&page->lru);
			if (--_pool.cur_nr < _pool.low_wm_nr)
				below_wm = 1;
			_pool.stat[NR_ALLOC_FROM_POOL]++;
		}
	}
	spin_unlock(&_pool.lock);

	if (below_wm && !__pool_thread_running())
		wake_up(&_pool.waitqueue);

	return page;
}

static void mp_free_pages(void *addr)
{
	struct page *page = virt_to_page(addr);
	int below_wm = 0;

	spin_lock(&_pool.lock);
	list_add(&page->lru, &_pool.list);
	if (++_pool.cur_nr < _pool.low_wm_nr)
		below_wm = 1;
	spin_unlock(&_pool.lock);

	if (below_wm && !__pool_thread_running())
		wake_up(&_pool.waitqueue);
}

static struct page *mp_alloc_pages_ti(void)
{
	return alloc_kmem_pages_node(NUMA_NO_NODE, THREADINFO_GFP,
			THREAD_SIZE_ORDER);
}

static void mp_free_pages_ti(void *addr)
{
	/* free to buddy system */
	kasan_alloc_pages(virt_to_page(addr), THREAD_SIZE_ORDER);
	free_kmem_pages((unsigned long)addr, THREAD_SIZE_ORDER);
}

struct page *threadinfo_pool_alloc_pages(void)
{
	struct page *page = NULL;

	page = mp_alloc_pages();
	if (!page) {
		page = mp_alloc_pages_ti();
		_pool.stat[NR_ALLOC_FROM_BUDDY]++;
	}

	return page;
}

void threadinfo_pool_free_pages(void *addr)
{
	if (atomic_read(&_pool.init) && __mp_is_not_full())
		mp_free_pages(addr);
	else
		mp_free_pages_ti(addr);
}

static int mp_thread(void *data)
{
	struct sched_param param = { .sched_priority = 0 };
	sched_setscheduler(_pool.tsk, SCHED_NORMAL, &param);

	while (true) {
		wait_event_freezable(_pool.waitqueue, __mp_is_not_full() && time_after64(get_jiffies_64(), _pool.freeze_time));
		while (__mp_is_not_full()) {
			struct page *page = mp_alloc_pages_ti();
			if (!page) {
				_pool.freeze_time = get_jiffies_64() + _pool.freeze_duration;
				break;
			}

			spin_lock(&_pool.lock);
			list_add_tail(&page->lru, &_pool.list);
			_pool.cur_nr++;
			spin_unlock(&_pool.lock);
		}
	}

	return 0;
}

static int mp_freeze_duration_read(void *data, u64 *val)
{
	struct mpool *mp = (struct mpool *)data;

	*val = mp->freeze_duration;
	return 0;
}

static int mp_freeze_duration_write(void *data, u64 val)
{
	struct mpool *mp = (struct mpool *)data;

	if (val > 5*HZ)
		val = 5*HZ;
	spin_lock(&mp->lock);
	mp->freeze_duration = val;
	spin_unlock(&mp->lock);
	return 0;
}

static int mp_maxnr_read(void *data, u64 *val)
{
	struct mpool *mp = (struct mpool *)data;

	*val = mp->max_nr;
	return 0;
}

static int mp_maxnr_write(void *data, u64 val)
{
	struct mpool *mp = (struct mpool *)data;
	unsigned int old_nr, new_nr;

	spin_lock(&mp->lock);
	old_nr = mp->max_nr;
	new_nr = (unsigned int)val;
	mp->max_nr = new_nr;
	mp->low_wm_nr = mp->max_nr * 3 / 4;
	spin_unlock(&mp->lock);

	if (old_nr < new_nr)
		wake_up(&_pool.waitqueue);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(pool_max_nr, mp_maxnr_read,
		mp_maxnr_write, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(pool_freeze_duration_nr, mp_freeze_duration_read,
		mp_freeze_duration_write, "%llu\n");

static int mp_stats_show(struct seq_file *s, void *v)
{
	struct mpool *mp = s->private;
	unsigned int max_nr, low_wm_nr, cur_nr;
	unsigned int stat[NR_STAT];
	int i;

	spin_lock(&mp->lock);
	max_nr = mp->max_nr;
	low_wm_nr = mp->low_wm_nr;
	cur_nr = mp->cur_nr;
	memcpy(stat, mp->stat, NR_STAT * sizeof(unsigned int));
	spin_unlock(&mp->lock);

	seq_puts(s, "\n");

	seq_printf(s, "max %d, low_wm %d, cur %d\n",
			max_nr,
			low_wm_nr,
			cur_nr);

	seq_puts(s, "----\n");

	for (i = 0; i < NR_STAT; i++) {
		seq_printf(s, "  %-25s: %d\n",
				stat_str[i],
				stat[i]);
	}

	seq_puts(s, "----\n");
	seq_printf(s, "  pool hit rate %d%%\n",
			100 * stat[NR_ALLOC_FROM_POOL] /
			(stat[NR_ALLOC_FROM_POOL] + stat[NR_ALLOC_FROM_BUDDY]));

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

static int __init mp_init(void)
{
	if (!debugfs_initialized()) {
		pr_warn("debugfs not available, stat dir not created\n");
		return -ENOENT;
	}

	spin_lock_init(&_pool.lock);
	INIT_LIST_HEAD(&_pool.list);
	_pool.cur_nr = 0;
	_pool.low_wm_nr = _pool.max_nr * 3 / 4;
	_pool.freeze_time = get_jiffies_64();
	_pool.freeze_duration = HZ;
	memset(_pool.stat, 0, NR_STAT * sizeof(unsigned int));

	_pool.dir = debugfs_create_dir("threadinfo_pool", NULL);
	if (!_pool.dir)
		pr_err("fail to create debugfs dir\n");
	else {
		debugfs_create_file("pool_max_nr",
				S_IRUGO | S_IWUSR, _pool.dir,
				&_pool, &pool_max_nr);
		debugfs_create_file("pool_freeze_duration_nr",
				S_IRUGO | S_IWUSR, _pool.dir,
				&_pool, &pool_freeze_duration_nr);
		debugfs_create_file("stat", S_IFREG | S_IRUGO,
				_pool.dir, &_pool, &mp_stat_ops);
	}

	init_waitqueue_head(&_pool.waitqueue);
	_pool.tsk = kthread_run(mp_thread, NULL, "threadinfo_pool");
	if (IS_ERR(_pool.tsk)) {
		pr_err("%s: creating thread for threadinfo pool failed\n", __func__);
		return PTR_ERR_OR_ZERO(_pool.tsk);
	}

	pr_info("threadinfo pool init OK\n");
	atomic_set(&_pool.init, 1);

	return 0;
}

fs_initcall(mp_init);

MODULE_LICENSE("Dual BSD/GPL");
