
#define pr_fmt(fmt)  "ktrace : " fmt


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/ktrace.h>



#ifdef KTRACE_QUEUE_NOLOCK

static inline int KTRACE_QUEUE_TRYLOCK(spinlock_t *lock, unsigned long flags)
{
	return 1;
}
static inline void KTRACE_QUEUE_LOCK(spinlock_t *lock, unsigned long flags) {}
static inline void KTRACE_QUEUE_UNLOCK(spinlock_t *lock, unsigned long flags) {}

#else

#define KTRACE_QUEUE_TRYLOCK(lock, flags) \
	spin_trylock_irqsave(lock, flags)

#define KTRACE_QUEUE_LOCK(lock, flags)  \
	spin_lock_irqsave(lock, flags)

#define KTRACE_QUEUE_UNLOCK(lock, flags) \
	spin_unlock_irqrestore(lock, flags)

#endif

void *ktrace_q_start(struct seq_file *m, loff_t *pos)
{
	struct ktrace_queue *q = m->private;
	unsigned long flags;
	void *ret = (void *)	1;
	int enable_debug = q->enable_debug;

	KTRACE_QUEUE_LOCK(&q->lock, flags);

	if (enable_debug) {
		seq_printf(m, "-> queue: tail %d, head %d, max %d\n",
				q->tail, q->head, q->max);
	}

	q->read_cnt = 0;

	if (QUEUE_EMPTY(q->head, q->tail)) {
		ret = NULL;
	}

	KTRACE_QUEUE_UNLOCK(&q->lock, flags);

	return ret;
}

void *ktrace_q_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct ktrace_queue *q = m->private;
	unsigned long flags;
	void *ret = (void *)	1;

	KTRACE_QUEUE_LOCK(&q->lock, flags);


	q->read_cnt += 1;

	if ((q->read_cnt >= q->items_per_read) ||
			QUEUE_EMPTY(q->head, q->tail)) {
		ret = NULL;
	}

	KTRACE_QUEUE_UNLOCK(&q->lock, flags);

	return ret;

}

void ktrace_q_stop(struct seq_file *m, void *v)
{
	struct ktrace_queue *q = m->private;
	int enable_debug = q->enable_debug;

	if (enable_debug) {
		seq_printf(m, "   queue: tail %d, head %d, max %d\n",
				q->tail, q->head, q->max);
	}
}

int ktrace_q_show(struct seq_file *m, void *v)
{
	struct ktrace_queue *q = m->private;
	unsigned long flags;
	int enable_debug = q->enable_debug;

	KTRACE_QUEUE_LOCK(&q->lock, flags);

	if (QUEUE_EMPTY(q->head, q->tail)) {
		goto out;
	}

	q->show_entry(m, q->buf + q->tail * q->entry_size, enable_debug);

	q->tail = QUEUE_NEXT(q->tail, q->max);

out:
	KTRACE_QUEUE_UNLOCK(&q->lock, flags);

	return 0;
}

int ktrace_q_single_show(struct seq_file *m, void *v)
{
	struct ktrace_queue *q = m->private;
	unsigned long flags;
	int ret;
	int enable_debug = q->enable_debug;
	int tail;

	KTRACE_QUEUE_LOCK(&q->lock, flags);

	if (enable_debug) {
		seq_printf(m, "-> queue: tail %d, head %d, max %d\n",
				q->tail, q->head, q->max);
	}

	tail = q->tail;
	while (!QUEUE_EMPTY(q->head, tail)) {

		ret = q->show_entry(m, q->buf + tail * q->entry_size, q->enable_debug);
		tail = QUEUE_NEXT(tail, q->max);

		if (ret)
			break;
	}

	if (!ret)
		q->tail = tail;

	if (enable_debug) {
		seq_printf(m, "   queue: tail %d, head %d, max %d\n",
				q->tail, q->head, q->max);
	}

	KTRACE_QUEUE_UNLOCK(&q->lock, flags);

	return 0;
}

void ktrace_tryadd_queue(struct ktrace_queue *q, void *entry)
{
	unsigned long flags;
	void *dst;

	if (!KTRACE_QUEUE_TRYLOCK(&q->lock, flags))
		return;

	dst = q->buf + q->head * q->entry_size;
	memcpy(dst, entry, q->entry_size);

	if (QUEUE_FULL(q->tail, q->head, q->max))
		q->tail = QUEUE_NEXT(q->tail, q->max);

	q->head = QUEUE_NEXT(q->head, q->max);

	KTRACE_QUEUE_UNLOCK(&q->lock, flags);

}

void ktrace_add_queue_multi(struct ktrace_queue *q, void *entry, int num)
{
	unsigned long flags;
	void *dst;
	int i;

	KTRACE_QUEUE_LOCK(&q->lock, flags);

	for (i = 0; i < num; i++) {
		dst = q->buf + q->head * q->entry_size;
		memcpy(dst, entry + i * q->entry_size, q->entry_size);

		if (QUEUE_FULL(q->tail, q->head, q->max))
			q->tail = QUEUE_NEXT(q->tail, q->max);

		q->head = QUEUE_NEXT(q->head, q->max);
	}

	KTRACE_QUEUE_UNLOCK(&q->lock, flags);
}

void ktrace_reset_queue(struct ktrace_queue *q)
{
	unsigned long flags;

	KTRACE_QUEUE_LOCK(&q->lock, flags);

	q->head = 0;
	q->tail = 0;

	KTRACE_QUEUE_UNLOCK(&q->lock, flags);
}

void __init ktrace_init_queue(struct ktrace_queue *q, struct dentry *dir,
		void *priv, void *buf, int entry_size, int max, int items_per_read,
		int(*show_entry)(struct seq_file *, void *, bool))
{
	spin_lock_init(&q->lock);

	q->priv = priv;
	q->buf = buf;
	q->entry_size = entry_size;

	q->head = 0;
	q->tail = 0;
	q->max = max;
	q->read_cnt = 0;
	q->items_per_read = items_per_read;
	q->show_entry = show_entry;

	if (dir) {
		debugfs_create_bool("queue_debug",
				S_IFREG | S_IRUGO | S_IWUSR,
				dir,
				&q->enable_debug);
		debugfs_create_u32("items_per_read",
				S_IFREG | S_IRUGO | S_IWUSR,
				dir,
				&q->items_per_read);
	}

	return;
}
