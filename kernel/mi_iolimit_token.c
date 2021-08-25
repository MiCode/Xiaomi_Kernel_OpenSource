#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/semaphore.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched/signal.h>
#include <linux/jiffies.h>
#include <linux/mi_iolimit_common.h>
#include <linux/mi_iolimit_token.h>


/*
 * module init is default module for io
 * can register bandwidth-control function by register api from here
 *
 * Token algorithm used in io speed limit, as follows:
 * Divide one second into the time interval that the producer provides
 * the token and the number of tokens.
 * such as one second = TOKEN_BUCKET_CAPACITY * PUSH_TOKEN_INTERVAL
 *
 * The capacity of a token is equal to the bandwidth divided
 * by the number of tokens.
 * such as The capacity of bg token = BG_RATE_LIMIT_BANDWIDTH / TOKEN_BUCKET_CAPACITY
 *
 * About consumers
 * Producer increases token capacity
 * When the capacity of a token is full, the number of tokens is reduced by one
 *
 * About producers:
 * Add a token every cycle which is PUSH_TOKEN_INTERVAL above.
 */
#define BG_RATE_LIMIT_BANDWIDTH		(3 * 1024 * 1024)	/* unit byte */
#define NATIVE_RATE_LIMIT_BANDWIDTH	(20 * 1024 * 1024)
#define FG_RATE_LIMIT_BANDWIDTH		(30 * 1024 * 1024)

#define TOKEN_BUCKET_CAPACITY	20	/* number of tokens per bucket */
#define PUSH_TOKEN_INTERVAL	50	/* interval of time sending token, unit ms */

#define BWC_MODE_RO 0440
#define BWC_MODE_RW 0660

#define BWC_ATTR(_name, _mode, _show, _store) \
	struct kobj_attribute kobj_attr_##_name \
		= __ATTR(_name, _mode, _show, _store)

static bwc_manager_t bwc_manager;
static bwc_manager_t *pg_bwc_manager;
static bandwidth_control_t *pg_default_bwc;
static bandwidth_control_t *pg_native_bwc;
static bandwidth_control_t *pg_fg_bwc;

inline void io_wakeup_process(void)
{
	wake_up_all(&pg_default_bwc->token_throttle.token_waitqueue);
}

/* about iomonitor thread */
static inline void io_monitor_thread_wakeup(void)
{
	if (NULL != pg_bwc_manager->io_moniter.task) {
		up(&pg_bwc_manager->io_moniter.wait);
	}
}

static inline void io_monitor_thread_wait(long timeout)
{
	if (timeout > 0)
		down_timeout(&pg_bwc_manager->io_moniter.wait, msecs_to_jiffies(timeout));
	else
		down(&pg_bwc_manager->io_moniter.wait);
}

static int iomonitor_thread(void *unused)
{
	int index;
	struct task_struct *task = current;

	task->flags |= PF_MEMALLOC;
	while (true) {
		if (pg_bwc_manager->io_moniter.io_pressure_warning) {
			pg_bwc_manager->io_moniter.log_out = true;
			spin_lock(&pg_bwc_manager->io_moniter.io_lock);
			if (jiffies_to_msecs(elapsed_jiffies(pg_bwc_manager->io_moniter.last_out_log_time)) >
					IO_PRESSURE_THREAD_LOG_INTERVAL) {
				io_monitor_info_t *p_io_monitor_info;
				for (index = 0; index < IO_PRESSURE_WARNING_PROCESS_NUMS; index++) {
					p_io_monitor_info = &pg_bwc_manager->io_moniter.io_monitor_info[index];
					if (p_io_monitor_info->pid)
						pr_info("iomonitor[%s] name=%s pid=%d group=%d adj=%d io_count=%lu io_time=%lu stime=%llu wtime=%llu rbw=%llu wbw=%llu fsize=%lld\n",
							IO_WARNING == p_io_monitor_info->type ? "W" : "L",
							p_io_monitor_info->comm, p_io_monitor_info->pid,
							p_io_monitor_info->group, p_io_monitor_info->adj,
							p_io_monitor_info->io_count, p_io_monitor_info->limit_time,
							p_io_monitor_info->stime, p_io_monitor_info->wtime,
							p_io_monitor_info->rbw, p_io_monitor_info->wbw,
							p_io_monitor_info->fsize);
				}
				pg_bwc_manager->io_moniter.last_out_log_time = jiffies;
			}
			memset(pg_bwc_manager->io_moniter.io_monitor_info, 0,
				sizeof(pg_bwc_manager->io_moniter.io_monitor_info));
			pg_bwc_manager->io_moniter.log_out = false;
			spin_unlock(&pg_bwc_manager->io_moniter.io_lock);
		}

		io_monitor_thread_wait(THREAD_WAIT_FOREVER);
		if (kthread_should_stop())
			goto out;

		/* maybe tune bw for each scene in here afterwards*/
	}
out:
	task->flags &= ~PF_MEMALLOC;
	pr_info("iolimit: iomonitor thread exit.\n");
	return 0;
}

static inline void io_monitor_thread_start(void)
{
	sema_init(&pg_bwc_manager->io_moniter.wait, 0);
	spin_lock_init(&pg_bwc_manager->io_moniter.io_lock);
	pg_bwc_manager->io_moniter.task = kthread_run(iomonitor_thread, NULL, "iomonitor");
	if (IS_ERR(pg_bwc_manager->io_moniter.task)) {
		pr_err("io_monitor: failed to start thread.\n");
	}
}

static inline void io_monitor_thread_stop(void)
{
	if (NULL != pg_bwc_manager->io_moniter.task) {
		up(&pg_bwc_manager->io_moniter.wait);
		kthread_stop(pg_bwc_manager->io_moniter.task);
		pg_bwc_manager->io_moniter.task = NULL;
	}
}

/* about stats */
static inline void io_bandwidth_rw_stats(bandwidth_control_t *p_bwc,
		enum io_type type)
{
	spin_lock(&p_bwc->bwc_lock);
	if (p_bwc->stats.enable) {
		switch (type) {
		case NORMAL_READ:
			p_bwc->stats.nr_count++;
			break;
		case NORMAL_WRITE:
			p_bwc->stats.nw_count++;
			break;
		case DIRECT_READ:
			p_bwc->stats.dr_count++;
			break;
		case DIRECT_WRITE:
			p_bwc->stats.dw_count++;
			break;
		default:
			break;
		}
	}
	spin_unlock(&p_bwc->bwc_lock);
}

/* caller lock */
static inline void io_bandwidth_limit_stats(bandwidth_control_t *p_bwc,
		enum io_type type)
{
	if (p_bwc->stats.enable) {
		switch (type) {
		case NORMAL_READ:
			p_bwc->stats.nr_limit_count++;
			break;
		case NORMAL_WRITE:
			p_bwc->stats.nw_limit_count++;
			break;
		case DIRECT_READ:
			p_bwc->stats.dr_limit_count++;
			break;
		case DIRECT_WRITE:
			p_bwc->stats.dw_limit_count++;
			break;
		default:
			break;
		}
	}
}

static inline bool is_bwcm_init_ok(void)
{
	if (NULL == pg_bwc_manager || !pg_bwc_manager->bwcm_init_ok)
		return false;

	return true;
}

static inline bool is_bwc_init_ok(bandwidth_control_t *p_bwc)
{
	if (!p_bwc->bwc_init_ok || !p_bwc->limit_switch)
		return false;

	return true;
}

/* about debug */
static inline void debug_process_info(struct kiocb *iocb, struct inode *inode,
		pgoff_t page_index, enum io_type type)
{
	short oom_score_adj;
	struct file *file = NULL;

	if (pg_bwc_manager->bwcm_debug) {
		if (NULL != iocb)
			file = iocb->ki_filp;
		task_lock(current);
		oom_score_adj = current->signal->oom_score_adj;
		task_unlock(current);
		pr_info("iolimit(%s %d) g=%d rw=%d adj=%d index=%lu fname=%s fsize=%lld\n",
			current->comm, current->pid, task_type(current), type,
			oom_score_adj, page_index, NULL != file ?
			(NULL != file->f_path.dentry ? file->f_path.dentry->d_name.name : NULL) :
			NULL, NULL != inode ? i_size_read(inode) : 0);
	}
}

/* caller lock */
static inline void debug_consume_info(bandwidth_control_t *p_bwc)
{
	if (p_bwc->debug)
		pr_info("iolimit_token (%s %d) %s consume remains=%d token_ok=%d current_capacity=%lu last_token_dispatch_time %lu\n",
			current->comm, current->pid, p_bwc->name,
			p_bwc->token_throttle.token_remains, p_bwc->token_throttle.token_ok,
			p_bwc->token_throttle.token_current_capacity,
			p_bwc->token_throttle.last_token_dispatch_time);
}

/* caller lock */
static inline void debug_product_info(bandwidth_control_t *p_bwc)
{
	if (p_bwc->debug) {
		unsigned int throttle_interval_time;

		throttle_interval_time = jiffies_to_msecs(elapsed_jiffies(p_bwc->token_throttle.last_token_dispatch_time));
		pr_info("iolimit_token %s product remains=%d token_ok=%d last_token_dispatch_time %lu interval_time %d",
			p_bwc->name, p_bwc->token_throttle.token_remains, p_bwc->token_throttle.token_ok,
			p_bwc->token_throttle.last_token_dispatch_time, throttle_interval_time);
	}
}

/* about filter */
static inline bool is_need_limit(struct kiocb *iocb, struct inode *inode,
		pgoff_t page_index, enum io_type type)
{
	struct file *filp;
	struct page *page;
	struct address_space *mapping;

	if (fatal_signal_pending(current))
		return false;

	/* no limiting kernel task */
	if (unlikely(current->flags & PF_KTHREAD))
		return false;

	/* limiting regular file only */
	if (NULL != inode && !S_ISREG(inode->i_mode))
		return false;

	if (NULL != iocb && NORMAL_READ == type) {
		filp = iocb->ki_filp;
		mapping = filp->f_mapping;

		if (iocb->ki_flags & IOCB_NOWAIT)
			return false;

		page = find_get_page(mapping, page_index);
		if (page) {
			if (PageUptodate(page)) {
				put_page(page);
				return false;
			}
			put_page(page);
		}
	}

	return true;
}

static inline bandwidth_control_t *get_bwc_by_type(char *type_name)
{
	int index;
	bandwidth_control_t *p_bwc;

	p_bwc = NULL;
	if (NULL == pg_bwc_manager)
		return NULL;

	if (!pg_bwc_manager->bwcm_init_ok)
		return NULL;

	spin_lock(&pg_bwc_manager->bwcm_lock);
	for (index = 0; index < BWC_NUMS; index++) {
		if (pg_bwc_manager->bwc_online & (1 << index)) {
			p_bwc = &pg_bwc_manager->bwc[index];
			if (p_bwc->bwc_init_ok &&
					0 == strncmp((char *)p_bwc->name, type_name, strlen(type_name)))
				break;
			p_bwc = NULL;
		}
	}
	spin_unlock(&pg_bwc_manager->bwcm_lock);
	return p_bwc;
}

static inline bandwidth_control_t *get_current_bwc(void)
{
	int task_group;
	bandwidth_control_t *p_bwc;

	if (!pg_bwc_manager->bwcm_switch)
		return NULL;

	/* for saving time not to call get_bwc_by_type
	 * but using direct value
	 */
	task_group = task_type(current);
	if (BG == task_group) {
		p_bwc = pg_default_bwc;
		if (NULL != p_bwc && p_bwc->bwc_init_ok) {
				task_lock(current);
				if (current->signal->oom_score_adj > p_bwc->adj_limit_level) {
					task_unlock(current);
					return p_bwc;
				}
				task_unlock(current);
		}
	} else if (NATIVE == task_group) {
		p_bwc = pg_native_bwc;
		if (NULL != p_bwc && (sysctl_mi_iolimit & IO_TOKEN_NATIVE_LIMIT_MASK) &&
				native_need_limit())
			return p_bwc;
	} else if (FG == task_group) {
		p_bwc = pg_fg_bwc;
		if (NULL != p_bwc && (sysctl_mi_iolimit & IO_TOKEN_FG_LIMIT_MASK) &&
				p_bwc->bwc_init_ok) {
			task_lock(current);
			if (current->signal->oom_score_adj >= p_bwc->adj_limit_level &&
					pg_bwc_manager->last_stime > LIMIT_STIME_MIN) {
				task_unlock(current);
				return p_bwc;
			}
			task_unlock(current);
		}
	}

	return NULL;
}

/* input parameter check by caller*/
static inline void storage_io_monitor_info(struct inode *inode, u64 stime, u64 wtime,
		enum io_monitor_type type, unsigned int limit_time)
{
	int index;

	if (current->ioac.last_ioac.rbw_char > IO_PRESSURE_WARNING_BW_LEVEL ||
			current->ioac.last_ioac.wbw_char > IO_PRESSURE_WARNING_BW_LEVEL ||
			IO_LIMIT == type) {
		io_monitor_info_t *p_io_monitor_info = NULL;
		spin_lock(&pg_bwc_manager->io_moniter.io_lock);
		for (index = 0; index < IO_PRESSURE_WARNING_PROCESS_NUMS; index++) {
			p_io_monitor_info = &pg_bwc_manager->io_moniter.io_monitor_info[index];
			if (p_io_monitor_info->pid) {
				if (current->pid == p_io_monitor_info->pid) {
					break;
				}
			} else {
				p_io_monitor_info->pid = current->pid;
				memcpy((char *)p_io_monitor_info->comm,
					(char *)current->comm, sizeof(p_io_monitor_info->comm));
				p_io_monitor_info->rbw = current->ioac.last_ioac.rbw_char;
				p_io_monitor_info->wbw = current->ioac.last_ioac.wbw_char;
				p_io_monitor_info->group = task_type(current);
				p_io_monitor_info->stime = stime;
				p_io_monitor_info->wtime = wtime;
				task_lock(current);
				p_io_monitor_info->adj = current->signal->oom_score_adj;
				task_unlock(current);
				p_io_monitor_info->type = type;
				p_io_monitor_info->fsize = i_size_read(inode);
				break;
			}
		}

		if (index < IO_PRESSURE_WARNING_PROCESS_NUMS) {
			if (IO_LIMIT == type) {
				p_io_monitor_info->type = type;
				p_io_monitor_info->limit_time += limit_time;
			}
			p_io_monitor_info->io_count++;
		}
		spin_unlock(&pg_bwc_manager->io_moniter.io_lock);

		if (index > IO_PRESSURE_WARNING_PROCESS_NUMS >> 1) {
			pg_bwc_manager->io_moniter.io_pressure_warning = true;
			mb();
			io_monitor_thread_wakeup();
		}
	}
}

static inline void io_moniter(struct inode *inode, enum io_monitor_type type,
		unsigned int limit_time)
{
	u64 stime = 0;
	u64 wtime = 0;
	struct block_device *s_bdev;

	/* stop storage warning info to reduce lock competition
	 * when thread out log */
	if (pg_bwc_manager->io_moniter.log_out)
		return;

	if (NULL == inode)
		return;

	if (NULL == inode->i_sb)
		return;

	s_bdev = inode->i_sb->s_bdev;
	if (NULL != s_bdev) {
		stime = atomic64_read(bdev_get_stime(s_bdev));
		wtime = atomic64_read(bdev_get_wtime(s_bdev));
		pg_bwc_manager->last_stime = stime;
		pg_bwc_manager->last_wtime = wtime;
	}

	/* storage bandwidth information */
	storage_io_monitor_info(inode, stime, wtime, type, limit_time);
	if (stime > IO_PRESSURE_WARNING_WAKEUP_THREAD_LEVEL) {
		pg_bwc_manager->io_moniter.io_pressure_warning = true;
		mb();
		io_monitor_thread_wakeup();
	}
}

inline void mi_io_bwc(struct kiocb *iocb, struct inode *inode, pgoff_t page_index,
		size_t count, enum io_type type)
{
	unsigned long wait_token_timeout;
	unsigned int throttle_interval_time;
	bandwidth_control_t *p_bwc;

	/* switch */
	if (!(sysctl_mi_iolimit & IO_TOKEN_LIMIT_MASK))
		return;

	if (!is_bwcm_init_ok())
		return;

	io_moniter(inode, IO_WARNING, 0);
	debug_process_info(iocb, inode, page_index, type);
	if (!is_need_limit(iocb, inode, page_index, type))
		return;

	p_bwc = get_current_bwc();
	if (NULL == p_bwc)
		return;

	if (!is_bwc_init_ok(p_bwc))
		return;

	io_bandwidth_rw_stats(p_bwc, type);
	spin_lock(&p_bwc->bwc_lock);
	debug_consume_info(p_bwc);
	/* push token
	 * everyone who came in can be an wakener
	 */
	throttle_interval_time = jiffies_to_msecs(elapsed_jiffies(p_bwc->token_throttle.last_token_dispatch_time));
	if (0 != p_bwc->token_throttle.last_token_dispatch_time &&
			throttle_interval_time >= p_bwc->token_throttle.push_token_interval) {
		if (p_bwc->token_throttle.token_remains < p_bwc->token_throttle.token_bucket_capacity) {
			p_bwc->token_throttle.token_remains++;
			p_bwc->token_throttle.token_ok = true;
			debug_product_info(p_bwc);
			p_bwc->token_throttle.last_token_dispatch_time = jiffies;
			spin_unlock(&p_bwc->bwc_lock);
			/* wake up consumer */
			wake_up_all(&p_bwc->token_throttle.token_waitqueue);
			spin_lock(&p_bwc->bwc_lock);
		}
	}

	/* wait token */
	if (0 == p_bwc->token_throttle.token_remains) {
		unsigned long limit_start_time;
		wait_token_timeout = msecs_to_jiffies(p_bwc->token_throttle.push_token_interval);
		spin_unlock(&p_bwc->bwc_lock);
		limit_start_time = jiffies;
		wait_event_interruptible_timeout(p_bwc->token_throttle.token_waitqueue,
			p_bwc->token_throttle.token_ok, wait_token_timeout);
		io_moniter(inode, IO_LIMIT, jiffies_to_msecs(elapsed_jiffies(limit_start_time)));
		spin_lock(&p_bwc->bwc_lock);
		io_bandwidth_limit_stats(p_bwc, type);
	}

	/* token consume */
	p_bwc->token_throttle.token_current_capacity += (unsigned long)count;
	if (p_bwc->token_throttle.token_current_capacity > p_bwc->token_throttle.capacity_per_token) {
		if (0 != p_bwc->token_throttle.token_remains) {
			p_bwc->token_throttle.token_remains--;
			/* start time of issuing token */
			if (0 == p_bwc->token_throttle.last_token_dispatch_time)
				p_bwc->token_throttle.last_token_dispatch_time = jiffies;
		}

		if (0 == p_bwc->token_throttle.token_remains)
			p_bwc->token_throttle.token_ok = false;
		p_bwc->token_throttle.token_current_capacity = 0;
	}
	spin_unlock(&p_bwc->bwc_lock);
}

static void bandwidth_control_start(bandwidth_control_t *p_bwc)
{
	if (!p_bwc->bwc_init_ok)
		return;

	spin_lock(&p_bwc->bwc_lock);
	p_bwc->limit_switch = true;
	spin_unlock(&p_bwc->bwc_lock);
}

static void bandwidth_control_stop(bandwidth_control_t *p_bwc)
{
	p_bwc->limit_switch = false;
	mb();
	wake_up_all(&p_bwc->token_throttle.token_waitqueue);
}

/* stats */
static ssize_t stats_data_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	int index;
	int ret = 0;
	bandwidth_control_t *p_bwc;
	io_monitor_info_t *p_io_monitor_info;

	p_bwc = &pg_bwc_manager->bwc[pg_bwc_manager->bwc_index];
	if (!p_bwc->bwc_init_ok)
		return -EAGAIN;

	spin_lock(&p_bwc->bwc_lock);
	if (p_bwc->stats.enable)
		ret = snprintf(buf, PAGE_SIZE, "nr %llu\nnw %llu\nnrl %llu\nnwl %llu\ndr %llu\ndw %llu\ndrl %llu\ndwl %llu\n",
				p_bwc->stats.nr_count,
				p_bwc->stats.nw_count,
				p_bwc->stats.nr_limit_count,
				p_bwc->stats.nw_limit_count,
				p_bwc->stats.dr_count,
				p_bwc->stats.dw_count,
				p_bwc->stats.dr_limit_count,
				p_bwc->stats.dw_limit_count);
	spin_unlock(&p_bwc->bwc_lock);

	ret += snprintf(buf + strlen(buf), PAGE_SIZE, "\nlimit info:\n");
	spin_lock(&pg_bwc_manager->io_moniter.io_lock);
	for (index = 0; index < IO_PRESSURE_WARNING_PROCESS_NUMS; index++) {
		p_io_monitor_info = &pg_bwc_manager->io_moniter.io_monitor_info[index];
		if (p_io_monitor_info->pid)
			ret += snprintf(buf + strlen(buf), PAGE_SIZE, "iomonitor[%s] name=%s pid=%d group=%d adj=%d io_count=%lu limit_time=%lu stime=%llu wtime=%llu rbw=%llu wbw=%llu fsize=%lld\n",
				IO_WARNING == p_io_monitor_info->type ? "W" : "L",
				p_io_monitor_info->comm, p_io_monitor_info->pid,
				p_io_monitor_info->group, p_io_monitor_info->adj,
				p_io_monitor_info->io_count, p_io_monitor_info->limit_time,
				p_io_monitor_info->stime, p_io_monitor_info->wtime,
				p_io_monitor_info->rbw, p_io_monitor_info->wbw,
				p_io_monitor_info->fsize);
	}
	spin_unlock(&pg_bwc_manager->io_moniter.io_lock);

	return ret;
}

static ssize_t stats_enable_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	bandwidth_control_t *p_bwc;

	p_bwc = &pg_bwc_manager->bwc[pg_bwc_manager->bwc_index];

	return snprintf(buf, PAGE_SIZE, "%u\n", p_bwc->stats.enable);
}

static ssize_t stats_enable_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t len)
{
	int ret;
	int enable;
	bandwidth_control_t *p_bwc;

	p_bwc = &pg_bwc_manager->bwc[pg_bwc_manager->bwc_index];

	ret = kstrtoint(buf, 0, &enable);
	if (ret)
		return ret;

	if (enable)
		p_bwc->stats.enable = true;
	else
		p_bwc->stats.enable = false;

	return len;
}

static BWC_ATTR(stats_enable, BWC_MODE_RW, stats_enable_show, stats_enable_store);
static BWC_ATTR(stats_data, BWC_MODE_RO, stats_data_show, NULL);

static struct attribute *bwc_stats_attrs[] = {
	&kobj_attr_stats_enable.attr,
	&kobj_attr_stats_data.attr,
	NULL,
};

static struct attribute_group bwc_stats_attr_group = {
	.attrs = bwc_stats_attrs,
};

/* bwc attribute */
static ssize_t enable_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	bandwidth_control_t *p_bwc;

	p_bwc = &pg_bwc_manager->bwc[pg_bwc_manager->bwc_index];

	return snprintf(buf, PAGE_SIZE, "%u\n", p_bwc->limit_switch);
}

static ssize_t enable_store(struct kobject *kobj, struct kobj_attribute *attr,
			const char *buf, size_t len)
{
	int ret;
	int enable;
	bandwidth_control_t *p_bwc;

	p_bwc = &pg_bwc_manager->bwc[pg_bwc_manager->bwc_index];

	ret = kstrtoint(buf, 0, &enable);
	if (ret)
		return ret;

	if (enable)
		bandwidth_control_start(p_bwc);
	else
		bandwidth_control_stop(p_bwc);

	return len;
}

static ssize_t config_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	int ret;
	bandwidth_control_t *p_bwc;

	p_bwc = &pg_bwc_manager->bwc[pg_bwc_manager->bwc_index];
	if (!p_bwc->bwc_init_ok)
		return -EAGAIN;

	spin_lock(&p_bwc->bwc_lock);
	ret = snprintf(buf, PAGE_SIZE, "type %s\nbwc_init %d\nswitch %d\ncapacity_per_token %lu\ntoken_bucket_capacity %d\npush_token_interval %d\nadj %d\n",
			p_bwc->name,
			p_bwc->bwc_init_ok,
			p_bwc->limit_switch,
			p_bwc->token_throttle.capacity_per_token,
			p_bwc->token_throttle.token_bucket_capacity,
			p_bwc->token_throttle.push_token_interval,
			p_bwc->adj_limit_level);
	spin_unlock(&p_bwc->bwc_lock);

	return ret;
}

static ssize_t config_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t len)
{
	bandwidth_control_t *p_bwc;

	p_bwc = &pg_bwc_manager->bwc[pg_bwc_manager->bwc_index];
	if (!p_bwc->bwc_init_ok)
		return -EAGAIN;

	spin_lock(&p_bwc->bwc_lock);
	if (sscanf(buf, "%lu %d %d %d\n", &p_bwc->token_throttle.capacity_per_token,
			&p_bwc->token_throttle.token_bucket_capacity,
			&p_bwc->token_throttle.push_token_interval,
			&p_bwc->adj_limit_level) != 1) {
		spin_unlock(&p_bwc->bwc_lock);
		return -EINVAL;
	}
	spin_unlock(&p_bwc->bwc_lock);

	return len;
}

static ssize_t debug_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	bandwidth_control_t *p_bwc;

	p_bwc = &pg_bwc_manager->bwc[pg_bwc_manager->bwc_index];

	return snprintf(buf, PAGE_SIZE, "%u\n", p_bwc->debug);
}

static ssize_t debug_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t len)
{
	int ret;
	int enable;
	bandwidth_control_t *p_bwc;

	p_bwc = &pg_bwc_manager->bwc[pg_bwc_manager->bwc_index];

	ret = kstrtoint(buf, 0, &enable);
	if (ret)
		return ret;

	if (enable)
		p_bwc->debug = true;
	else
		p_bwc->debug = false;

	return len;
}

static ssize_t bwcm_debug_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "debug %u\nswitch %u\ninit %u",
		pg_bwc_manager->bwcm_debug,
		pg_bwc_manager->bwcm_switch,
		pg_bwc_manager->bwcm_init_ok);
}

static ssize_t bwcm_debug_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t len)
{
	int ret;
	int enable;

	ret = kstrtoint(buf, 0, &enable);
	if (ret)
		return ret;

	if (enable)
		pg_bwc_manager->bwcm_debug = true;
	else
		pg_bwc_manager->bwcm_debug = false;

	return len;
}

static ssize_t bwc_index_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", pg_bwc_manager->bwc_index);
}

static ssize_t bwc_index_store(struct kobject *kobj, struct kobj_attribute *attr,
		const char *buf, size_t len)
{
	int ret;
	int bwc_index;

	ret = kstrtoint(buf, 0, &bwc_index);
	if (ret)
		return ret;

	if (bwc_index <= BWC_MAX_INDEX) {
		pg_bwc_manager->bwc_index = bwc_index;
		pg_bwc_manager->bwcm_switch = true;
	}

	if (bwc_index == 0xff)
		pg_bwc_manager->bwcm_switch = false;

	return len;
}

static BWC_ATTR(enable, BWC_MODE_RW, enable_show, enable_store);
static BWC_ATTR(config, BWC_MODE_RW, config_show, config_store);
static BWC_ATTR(debug, BWC_MODE_RW, debug_show, debug_store);
static BWC_ATTR(bwcm_debug, BWC_MODE_RW, bwcm_debug_show, bwcm_debug_store);
static BWC_ATTR(bwc_index, BWC_MODE_RW, bwc_index_show, bwc_index_store);

static struct attribute *bwc_attrs[] = {
	&kobj_attr_enable.attr,
	&kobj_attr_config.attr,
	&kobj_attr_debug.attr,
	&kobj_attr_bwcm_debug.attr,
	&kobj_attr_bwc_index.attr,
	NULL,
};

static struct attribute_group bwc_attr_group = {
	.attrs = bwc_attrs,
};

static struct kobject *sysfs_create(char *p_name,
		struct kobject *parent, struct attribute_group *grp)
{
	int err;
	struct kobject *kobj = NULL;

	kobj = kobject_create_and_add(p_name, parent);
	if (!kobj) {
		pr_err("%s limit: failed to create sysfs node.\n", p_name);
		return NULL;
	}

	err = sysfs_create_group(kobj, grp);
	if (err) {
		pr_err("%s limit: failed to create sysfs attrs.\n", p_name);
		kobject_put(kobj);
		return NULL;
	}

	return kobj;
}

static void sysfs_destory(struct kobject *p_kobj)
{
	if (!p_kobj)
		return;

	kobject_put(p_kobj);
}

static inline void  __bandwidth_control_init(bandwidth_control_t *p_bwc,
		char *name, unsigned long bandwidth)
{
	p_bwc->limit_switch = true;
	p_bwc->adj_limit_level = 700;
	p_bwc->token_throttle.token_ok = true;
	p_bwc->token_throttle.push_token_interval = PUSH_TOKEN_INTERVAL;
	p_bwc->token_throttle.token_bucket_capacity = TOKEN_BUCKET_CAPACITY;
	p_bwc->token_throttle.token_remains = TOKEN_BUCKET_CAPACITY;
	p_bwc->token_throttle.capacity_per_token = bandwidth / TOKEN_BUCKET_CAPACITY;
	memcpy((char *)(p_bwc->name), name, min(strlen(name), sizeof(p_bwc->name)));

	spin_lock_init(&p_bwc->bwc_lock);
	init_waitqueue_head(&p_bwc->token_throttle.token_waitqueue);
	mb();
	p_bwc->bwc_init_ok = true;
}

static inline void __bandwidth_control_exit(bandwidth_control_t *p_bwc)
{
	if (NULL != p_bwc) {
		p_bwc->limit_switch = false;

		mb();
		wake_up_all(&p_bwc->token_throttle.token_waitqueue);
	}
}

static inline bandwidth_control_t *get_bwc(void)
{
	int index;
	bandwidth_control_t *p_bwc;

	if (NULL == pg_bwc_manager) {
		pr_info("bwcm null\n");
		return NULL;
	}

	if (!pg_bwc_manager->bwcm_lock_init_ok) {
		pr_info("bwcm lock is no ready");
		return NULL;
	}

	p_bwc = NULL;
	spin_lock(&pg_bwc_manager->bwcm_lock);
	for (index = 0; index < BWC_NUMS; index++) {
		if (!(pg_bwc_manager->bwc_online & (1 << index))) {
			p_bwc = &pg_bwc_manager->bwc[index];
			p_bwc->index = index;
			pg_bwc_manager->bwc_online |= 1 << index;
			break;
		}
	}
	spin_unlock(&pg_bwc_manager->bwcm_lock);

	return p_bwc;
}

static inline void release_bwc(bandwidth_control_t *p_bwc)
{
	if (NULL != p_bwc) {
		spin_lock(&pg_bwc_manager->bwcm_lock);
		pg_bwc_manager->bwc_online &= ~(1 << p_bwc->index);
		spin_unlock(&pg_bwc_manager->bwcm_lock);
	}
}

inline bandwidth_control_t *register_bwc(char *name, unsigned long bandwidth)
{
	bandwidth_control_t *p_bwc;

	p_bwc = get_bwc();
	if (NULL == p_bwc) {
		pr_err("Fail to register bwc\n");
		return NULL;
	}

	__bandwidth_control_init(p_bwc, name, bandwidth);

	return p_bwc;
}

inline void unregister_bwc(bandwidth_control_t *p_bwc)
{
	release_bwc(p_bwc);
	__bandwidth_control_exit(p_bwc);
}

static int __init bandwidth_control_init(void)
{
	/* default register bg native and fg */
	pg_bwc_manager = &bwc_manager;
	pg_bwc_manager->bwcm_switch = true;
	spin_lock_init(&pg_bwc_manager->bwcm_lock);
	mb();
	pg_bwc_manager->bwcm_lock_init_ok = true;

	mb();
	pg_default_bwc = register_bwc("bg", BG_RATE_LIMIT_BANDWIDTH);
	if (IS_ERR(pg_default_bwc))
		return PTR_ERR(pg_default_bwc);

	pg_native_bwc = register_bwc("native", NATIVE_RATE_LIMIT_BANDWIDTH);
	pg_fg_bwc = register_bwc("fg", FG_RATE_LIMIT_BANDWIDTH);
	if (NULL != pg_fg_bwc)
		pg_fg_bwc->adj_limit_level = 300;

	pg_bwc_manager->p_kobj = sysfs_create("iolimit", kernel_kobj, &bwc_attr_group);
	if (NULL != pg_bwc_manager->p_kobj)
		pg_bwc_manager->p_stats_kobj = sysfs_create("stats", pg_bwc_manager->p_kobj,
				&bwc_stats_attr_group);

	io_monitor_thread_start();

	mb();
	pg_bwc_manager->bwcm_init_ok = true;

	return 0;
}

static void __exit bandwidth_control_exit(void)
{
	if (NULL != pg_bwc_manager) {
		sysfs_destory(pg_bwc_manager->p_stats_kobj);
		pg_bwc_manager->p_stats_kobj = NULL;

		sysfs_destory(pg_bwc_manager->p_kobj);
		pg_bwc_manager->p_kobj = NULL;

		io_monitor_thread_stop();
	}

	unregister_bwc(pg_default_bwc);
	unregister_bwc(pg_native_bwc);
	unregister_bwc(pg_fg_bwc);
}


subsys_initcall(bandwidth_control_init);

module_exit(bandwidth_control_exit);
MODULE_AUTHOR("zhangcang<zhangcang@xaiomi.com>");
MODULE_DESCRIPTION("Token Bucket Rate Limit");
MODULE_LICENSE("GPL");
