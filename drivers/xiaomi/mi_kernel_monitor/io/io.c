// SPDX-License-Identifier: GPL-2.0+
/*
 * io trace.
 *
 * Copyright (C) 2024 Xiaomi Ltd.
 */

#include <linux/blk_types.h>
#include <linux/workqueue.h>
#include <linux/bio.h>
#include <linux/list.h>
#include <linux/build_bug.h>
#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/tracepoint.h>
#include <trace/events/block.h>
#include "internal.h"

#include "kernel_monitor.h"

#define BIO_OEM_PAYLOAD 15

/*
 * NOTE: io_trace_d_find_any_alias is copied from fs/dcache.c:d_find_any_alias
 * because it is not exported for ogki.
 */
static struct dentry * __d_find_any_alias(struct inode *inode)
{
	struct dentry *alias;
	if (hlist_empty(&inode->i_dentry))
		return NULL;
	alias = hlist_entry(inode->i_dentry.first, struct dentry, d_u.d_alias);
	lockref_get(&alias->d_lockref);
	return alias;
}

static struct dentry *io_trace_d_find_any_alias(struct inode *inode)
{
	struct dentry *de;
	spin_lock(&inode->i_lock);
	de = __d_find_any_alias(inode);
	spin_unlock(&inode->i_lock);
	return de;
}

/* NOTE: reference to struct iomap_dio in fs/iomap/direct-io.c */
struct iomap_dio {
	struct kiocb *iocb;
};

#define DEFAULT_IO_LATENCY_THRESHOLD 50000000 // 50ms
#define DEFAULT_NR_RECORD_MAX 1024

static int64_t io_latency_threshold = DEFAULT_IO_LATENCY_THRESHOLD;
static unsigned int nr_record_max = DEFAULT_NR_RECORD_MAX;
static struct kmem_cache *payload_slab;
static struct proc_dir_entry *io_dir;
static spinlock_t lists_lock;
static bool lists_ready;
static struct list_head issued_list;
static unsigned int nr_records;
static unsigned int max_records;
static struct workqueue_struct *io_trace_wq;
static DEFINE_MUTEX(switch_mutex);
/* this is not a very precise switch, it is ok to check it without a lock */
static bool enabled;

struct xiaomi_io_trace {
	struct work_struct pending_work;
	struct list_head list;
	struct inode *inode;
	sector_t sector;
	time64_t issue_ts;
	time64_t complete_ts;
	pid_t pid;
	blk_opf_t opf;
	dev_t devid;
	unsigned int nr_sector;
	int error;
};

struct xiaomi_payload {
	struct xiaomi_io_trace io_trace;
};

#define XIAOMI_IO_TRACE_PTR_OF(bio_or_req, member) \
	&((struct xiaomi_payload *)(bio_or_req)->member)->io_trace

/* #define IO_TRACE_DEBUG */
#ifdef IO_TRACE_DEBUG
#define show_xiaomi_io_trace(io_trace) do { \
	char rwbs[RWBS_LEN] = {0}; \
	blk_fill_rwbs(rwbs, (io_trace)->opf); \
	pr_err("%s:%d (%d,%d) pid:%d ino:%lu opf:%#x rwbs:%s sector:%#llx " \
	       "nr_sector:%u issue:%llu complete:%llu error:%d\n", __func__, __LINE__, \
	       MAJOR((io_trace)->devid), MINOR((io_trace)->devid), (io_trace)->pid, \
	       (io_trace)->inode ? (io_trace)->inode->i_ino : 0, (io_trace)->opf, rwbs, \
	       (io_trace)->sector, (io_trace)->nr_sector, (io_trace)->issue_ts, \
	       (io_trace)->complete_ts, (io_trace)->error); \
} while (0)
#else
#define show_xiaomi_io_trace(io_trace) do {} while (0)
#endif

#define BIO_IS_XIAOMI_IO_TRACE_ENABLED(bio) bio_flagged(bio, BIO_OEM_PAYLOAD)

bool is_critical_ux_task(struct task_struct* task);

static inline bool xiaomi_is_read_op(blk_opf_t opf)
{
	return ((opf & REQ_OP_MASK) == REQ_OP_READ);
}

static void pending_handler(struct work_struct *work)
{
	struct xiaomi_io_trace *io_trace = container_of(work,
							struct xiaomi_io_trace,
							pending_work);
	struct xiaomi_payload *payload = container_of(io_trace,
						      struct xiaomi_payload,
						      io_trace);
	struct inode *inode = io_trace->inode;
	char *buf = NULL, *path = NULL;
	struct dentry *dentry;
	unsigned long flags;
	bool need_record = xiaomi_is_read_op(io_trace->opf) &&
			   io_trace->complete_ts - io_trace->issue_ts >
						io_latency_threshold;

	if (inode) {
		/*
		 * get file path only if trace event is enabled and latency
		 * is larger than threshold
		 */
		if (trace_io_read_delay_enabled() && need_record) {
			buf = __getname();
			if (buf) {
				dentry = io_trace_d_find_any_alias(inode);
				if (dentry) {
					path = dentry_path_raw(dentry, buf, PATH_MAX);
					if (IS_ERR(path))
						path = NULL;
					dput(dentry);
				}
			}
		}
	}

	/* TODO do statistics, use iostat for now */
	show_xiaomi_io_trace(io_trace);

	if (need_record)
		trace_io_read_delay(io_trace->devid, io_trace->pid, io_trace->opf,
				    io_trace->inode, path, io_trace->sector,
				    io_trace->nr_sector, io_trace->issue_ts,
				    io_trace->complete_ts, io_trace->error);

	if (inode)
		iput(inode);

	if (buf)
		__putname(buf);

	spin_lock_irqsave(&lists_lock, flags);
	list_del(&io_trace->list);
	nr_records--;
	spin_unlock_irqrestore(&lists_lock, flags);

	kmem_cache_free(payload_slab, payload);
}

static struct inode *xiaomi_io_trace_get_inode(struct bio *bio)
{
	struct folio *folio;
	struct address_space *mapping;

	if (!bio_has_data(bio))
		return NULL;

	/*
	 * Scenario 1: DIO + heap buffer (anonymous pages)
	 * Scenario 2: DIO + file-backed mmap (get TARGET file inode)
	 * Scenario 3: DIO + stack buffer
	 *
	 * Non-cloned DIO path - get TARGET file inode from iomap_dio
	 * via bi_private. Must check before folio_mapping, because
	 * folio_mapping would return source file inode for mmap case.
	 */
	if (bio->bi_private &&
	    bio_flagged(bio, BIO_PAGE_PINNED) &&
	    !bio_flagged(bio, BIO_CLONED) &&
	    !(bio->bi_opf & REQ_SWAP)) {
		struct iomap_dio *dio =
			(struct iomap_dio *)bio->bi_private;
		if (dio && dio->iocb && dio->iocb->ki_filp)
			return file_inode(dio->iocb->ki_filp);
	}

	folio = bio_first_folio_all(bio);
	mapping = folio ? folio_mapping(folio) : NULL;

	/*
	 * Scenario 4: Buffered read - folio from page cache, TARGET inode.
	 * Scenario 5: Buffered write - folio from page cache, TARGET inode.
	 * Scenario 6: DIO + mmap + dm clone - SOURCE file inode (fallback).
	 */
	if (mapping && mapping->host)
		return mapping->host;

	/*
	 * Scenario 7: Swap writeback - swap block device inode.
	 */
	if ((bio->bi_opf & REQ_SWAP) &&
	    bio->bi_bdev && bio->bi_bdev->bd_mapping)
		return bio->bi_bdev->bd_mapping->host;

	/*
	 * Scenario 8: DIO + heap + dm clone - cannot obtain inode.
	 */
	return NULL;
}

static inline struct xiaomi_io_trace *xiaomi_io_trace_alloc(struct bio *bio)
{
	struct xiaomi_payload *payload;
	struct xiaomi_io_trace *io_trace;
	struct inode *inode = NULL;
	unsigned long flags;

#ifdef IO_TRACE_DEBUG
	BUG_ON(BIO_IS_XIAOMI_IO_TRACE_ENABLED(bio));
#endif
	if (BIO_IS_XIAOMI_IO_TRACE_ENABLED(bio))
		return NULL;

	if (nr_records >= nr_record_max) {
		pr_warn_ratelimited("too many records already\n");
		return NULL;
	}

	if (bio_has_data(bio))
		inode = xiaomi_io_trace_get_inode(bio);

	if (in_atomic() || irqs_disabled()) {
		payload = kmem_cache_alloc(payload_slab, GFP_ATOMIC);
	} else {
		payload = kmem_cache_alloc(payload_slab, GFP_NOIO);
	}

	if (!payload) {
		pr_warn_ratelimited("%s: cannot alloc payload, %u existed\n",
				   __func__, nr_records);
		return NULL;
	}

	io_trace = &payload->io_trace;

	spin_lock_irqsave(&lists_lock, flags);
	/* check-lists_ready and list-add should be under the same lock */
	if (unlikely(!lists_ready) || nr_records >= nr_record_max) {
		kmem_cache_free(payload_slab, payload);
		spin_unlock_irqrestore(&lists_lock, flags);
		return NULL;
	}
	nr_records++;
	if (nr_records > max_records)
		max_records = nr_records;
	list_add_tail(&io_trace->list, &issued_list);
	spin_unlock_irqrestore(&lists_lock, flags);

	io_trace->pid = current->pid;
	io_trace->inode = inode ? igrab(inode) : NULL;
	io_trace->opf = bio->bi_opf;
	io_trace->sector = bio->bi_iter.bi_sector;
	io_trace->nr_sector = bio_sectors(bio);
	io_trace->devid = disk_devt(bio->bi_bdev->bd_disk);
	io_trace->issue_ts = ktime_get_ns();
	io_trace->complete_ts = 0;
	INIT_WORK(&io_trace->pending_work, pending_handler);
	show_xiaomi_io_trace(io_trace);

	/*
	 * NOTE: erofs iostat has already used android_oem_data1 to save
	 * bio latency. Fortunately, monitored EROFSes are all mounted
	 * on dm-X, all bios are cloned from device mapper to lower
	 * device. So we can use android_oem_data1 directly. BUT this
	 * is only a temporary solution.
	 */
	bio->android_oem_data1 = (unsigned long)io_trace;
	bio_set_flag(bio, BIO_OEM_PAYLOAD);
	return io_trace;
}

static void block_rq_issue_handler(void *unused, struct request *rq)
{
	struct xiaomi_io_trace *io_trace;
	struct bio *bio;

	if (likely(!enabled) || !is_critical_ux_task(current))
		return;

	bio = rq->bio;
	if (!bio || BIO_IS_XIAOMI_IO_TRACE_ENABLED(bio)) {
#ifdef IO_TRACE_DEBUG
		BUG_ON((bio && bio->android_oem_data1) || rq->android_oem_data1);
#endif
		return;
	}

	io_trace = xiaomi_io_trace_alloc(bio);
	if (!io_trace)
		return;

	rq->android_oem_data1 = bio->android_oem_data1;
	show_xiaomi_io_trace(io_trace);
}

static void block_rq_complete_handler(void *unused, struct request *rq,
				      blk_status_t error, unsigned int nr_bytes)
{
	struct bio *bio = rq->bio;
	struct xiaomi_io_trace *io_trace;

	if (!bio || !BIO_IS_XIAOMI_IO_TRACE_ENABLED(bio))
		return;

	if (!rq->android_oem_data1)
		return;

	io_trace = XIAOMI_IO_TRACE_PTR_OF(rq, android_oem_data1);
	io_trace->complete_ts = ktime_get_ns();
	io_trace->error = (__force int)error;
	bio->android_oem_data1 = 0;
	rq->android_oem_data1 = 0;
	bio_clear_flag(bio, BIO_OEM_PAYLOAD);
	queue_work(io_trace_wq, &io_trace->pending_work);
}

static int switch_hooks(bool on);

static ssize_t enable_proc_write(struct file *file, const char __user *buf,
				 size_t size, loff_t *ppos)
{
	bool on;
	int ret;

	ret = kstrtobool_from_user(buf, size, &on);
	if (ret < 0)
		return ret;

	if (enabled == on)
		return size;

	ret = switch_hooks(on);
	if (ret < 0)
		return ret;

	return size;
}

static ssize_t enable_proc_read(struct file *file, char __user *buf,
				size_t size, loff_t *ppos)
{
	char val[2];

	val[0] = enabled ? '1' : '0';
	val[1] = '\n';
	return simple_read_from_buffer(buf, size, ppos, val, sizeof(val));
}

static const struct proc_ops enable_proc_ops = {
	.proc_write	= enable_proc_write,
	.proc_read	= enable_proc_read,
};

static int issued_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "pending records: %u\n", nr_records);
	seq_printf(m, "max records: %u\n", max_records);
	return 0;
}

static int issued_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, issued_proc_show, NULL);
}

static const struct proc_ops issued_proc_ops = {
	.proc_open = issued_proc_open,
	.proc_read = seq_read,
	.proc_release = single_release,
};

static ssize_t nr_record_max_proc_write(struct file *file, const char __user *buf,
					size_t size, loff_t *ppos)
{
	unsigned int val;
	int ret;

	ret = kstrtouint_from_user(buf, size, 0, &val);
	if (ret < 0)
		return ret;

	nr_record_max = val;
	return size;
}

static ssize_t nr_record_max_proc_read(struct file *file, char __user *buf,
				       size_t size, loff_t *ppos)
{
	char val[8];
	int len;

	len = scnprintf(val, sizeof(val), "%u\n", nr_record_max);
	return simple_read_from_buffer(buf, size, ppos, val, len);
}

static const struct proc_ops nr_record_max_proc_ops = {
	.proc_write	= nr_record_max_proc_write,
	.proc_read	= nr_record_max_proc_read,
};

static ssize_t io_latency_threshold_proc_write(struct file *file,
			const char __user *buf, size_t size, loff_t *ppos)
{
	char kbuf[16] = { 0 };
	long long val;
	int ret;

	if (size > sizeof(kbuf) - 1)
		return -EINVAL;

	if (copy_from_user(kbuf, buf, size))
		return -EFAULT;

	ret = kstrtoll(strstrip(kbuf), 0, &val);
	if (ret < 0)
		return ret;

	if (val < 0 || val > S64_MAX / 1000)
		return -EINVAL;

	io_latency_threshold = val * 1000;
	return size;
}

static ssize_t io_latency_threshold_proc_read(struct file *file, char __user *buf,
				size_t size, loff_t *ppos)
{
	char val[16];
	int len;

	len = scnprintf(val, sizeof(val), "%lld\n",
			io_latency_threshold / 1000);
	return simple_read_from_buffer(buf, size, ppos, val, len);
}

static const struct proc_ops io_latency_threshold_proc_ops = {
	.proc_write	= io_latency_threshold_proc_write,
	.proc_read	= io_latency_threshold_proc_read,
};

static int create_procfs_nodes(void)
{
	io_dir = xiaomi_proc_mkdir(PROC_TRACE_IO, NULL);
	if (!io_dir) {
		pr_err("%s: cannot mkdir in procfs\n", __func__);
		return -ENOMEM;
	}

	if (!proc_create("enable", S_IRUGO | S_IWUGO, io_dir, &enable_proc_ops) ||
	    !proc_create("nr_record_max", S_IRUGO | S_IWUGO, io_dir,
			 &nr_record_max_proc_ops) ||
	    !proc_create("io_latency_threshold_us", S_IRUGO | S_IWUGO, io_dir,
			 &io_latency_threshold_proc_ops) ||
	    !proc_create("issued", S_IRUGO, io_dir, &issued_proc_ops)) {
		pr_err("%s: cannot create entry in procfs\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static void destroy_procfs_nodes(void)
{
	if (!io_dir)
		return;

	remove_proc_entry("enable", io_dir);
	remove_proc_entry("nr_record_max", io_dir);
	remove_proc_entry("io_latency_threshold_us", io_dir);
	remove_proc_entry("issued", io_dir);
	remove_proc_subtree(PROC_TRACE_IO, NULL);
	io_dir = NULL;
}

static int switch_hooks(bool on)
{
	int ret, orig_ret = 0;

	/* lock to avoid concurrent write */
	mutex_lock(&switch_mutex);

	if (on) {
		ret = register_trace_block_rq_issue(block_rq_issue_handler, NULL);
		if (ret < 0) {
			pr_err("%s:%d failed to register hook\n", __func__, __LINE__);
			orig_ret = ret;
			goto turn_off;
		}

		ret = register_trace_block_rq_complete(block_rq_complete_handler, NULL);
		if (ret < 0) {
			pr_err("%s:%d failed to register hook\n", __func__, __LINE__);
			orig_ret = ret;
			goto turn_off;
		}

		pr_info("%s:%d register hook and set enabled = true\n", __func__, __LINE__);
		enabled = true;
	} else {
turn_off:
		enabled = false;
		ret = unregister_trace_block_rq_issue(block_rq_issue_handler, NULL);
		if (ret < 0)
			pr_err("%s:%d failed to unregister hook\n", __func__, __LINE__);

		ret = unregister_trace_block_rq_complete(block_rq_complete_handler, NULL);
		if (ret < 0)
			pr_err("io_trace: failed to unregister hook\n");

		pr_info("io_trace: unregister hook and set enabled = false\n");
	}

	mutex_unlock(&switch_mutex);
	return orig_ret ? orig_ret : ret;
}

int xiaomi_io_trace_init(void)
{
	unsigned long flags;
	int ret;

	BUILD_BUG_ON(BIO_OEM_PAYLOAD < BIO_FLAG_LAST ||
		     BIO_OEM_PAYLOAD >= 8 * sizeof_field(struct bio, bi_flags));

	spin_lock_init(&lists_lock);
	INIT_LIST_HEAD(&issued_list);

	io_trace_wq = alloc_workqueue("xiaomi_io_trace", WQ_UNBOUND, 0);
	if (!io_trace_wq) {
		pr_err("%s: cannot alloc workqueue\n", __func__);
		return -ENOMEM;
	}

	payload_slab = kmem_cache_create("xiaomi_payload",
					 sizeof(struct xiaomi_payload), 0,
					 SLAB_TEMPORARY, NULL);
	if (!payload_slab) {
		pr_err("%s: cannot alloc kmem cache in advance\n", __func__);
		ret = -ENOMEM;
		goto out_destroy_wq;
	}

	ret = create_procfs_nodes();
	if (ret < 0)
		goto out_remove_proc;

	spin_lock_irqsave(&lists_lock, flags);
	lists_ready = true;
	spin_unlock_irqrestore(&lists_lock, flags);

	return 0;

out_remove_proc:
	destroy_procfs_nodes();
	kmem_cache_destroy(payload_slab);
out_destroy_wq:
	destroy_workqueue(io_trace_wq);
	return ret;
}

void xiaomi_io_trace_exit(void)
{
	unsigned long flags;

	spin_lock_irqsave(&lists_lock, flags);
	lists_ready = false;
	spin_unlock_irqrestore(&lists_lock, flags);

	switch_hooks(false);

	/* Ensure all in-flight tracepoint callbacks have completed */
	tracepoint_synchronize_unregister();

	/* Wait for all queued pending_handlers to finish self-cleanup */
	flush_workqueue(io_trace_wq);
	destroy_workqueue(io_trace_wq);

	/*
	 * At this point, no new works can be queued and all queued works have
	 * completed. Remaining entries in issued_list are IOs that were issued
	 * but never completed (work was never queued), clean them up manually.
	 */
	if (payload_slab) {
		struct xiaomi_io_trace *io_trace, *next;
		struct xiaomi_payload *payload;

		list_for_each_entry_safe(io_trace, next, &issued_list, list) {
			list_del(&io_trace->list);
			if (io_trace->inode)
				iput(io_trace->inode);
			payload = container_of(io_trace, struct xiaomi_payload, io_trace);
			kmem_cache_free(payload_slab, payload);
		}
		kmem_cache_destroy(payload_slab);
	}

	destroy_procfs_nodes();
}
