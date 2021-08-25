/* buffer rate limit based on window
 * author: zhangcang@xiaomi.com
 *
*/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/mount.h>
#include <linux/path.h>
#include <linux/pagevec.h>
#include <linux/uio.h>
#include <linux/mm.h>
#include <linux/vmstat.h>
#include <linux/version.h>
#include <linux/sysctl.h>
#include <linux/cpuset.h>
#include <linux/cgroup.h>
#include <linux/mi_iolimit_common.h>


#define LIMIT_PAGES				32


/* 76800:300MB 12800:50MB 2560:10MB */
static throttle_t throttle_base[THROTTLE_TYPE_NR] = {
	{64, 16, 50, 20, 1000,
	2000, 1000, 10, 0, 0, false, 30000, 50000},
	{100, 30, 10, 5, 200,
	2000, 1000, 10, 0, 0, false, 40000, 80000},
};

unsigned int sysctl_mi_iolimit;
static const int throttle_file_size = 1 * MB;

int mi_iolimit_handler(struct ctl_table *table, int write,
			void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;
	unsigned int *data = (unsigned int *)table->data;

	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);

	if (ret || !write)
		goto done;

	sysctl_mi_iolimit = *data;

done:
	return ret;
}

static inline bool throttle_check(struct kiocb *iocb)
{
	struct file     *file = iocb->ki_filp;
	struct inode    *inode = file_inode(file);
	struct block_device *s_bdev;
	struct backing_dev_info *bdi;
	mi_throttle_t *mi_throttle;

	/* switch */
	if (!(sysctl_mi_iolimit & IO_WINDOW_LIMIT_MASK))
		return false;

	/* No restrictions on direct and sync */
	if (iocb->ki_flags &
		(IOCB_DIRECT | IOCB_HIPRI | IOCB_DSYNC | IOCB_SYNC | IOCB_NOWAIT))
		return false;

	/* no limiting kernel task */
	if (unlikely(current->flags & PF_KTHREAD))
		return false;

	if (NULL == inode)
		return false;

	/* limiting regular file */
	if (!S_ISREG(inode->i_mode))
		return false;

	if (NULL == inode->i_sb)
		return false;

	s_bdev = inode->i_sb->s_bdev;
	if (NULL == s_bdev)
		return false;

	bdi = bdev_get_bdi(s_bdev);
	if (NULL == bdi)
		return false;

	mi_throttle = &bdi->mi_throttle;
	if (!mi_throttle->throttle_init)
		return false;
	return true;
}

void mi_throttle_init(mi_throttle_t *mi_throttle)
{
	memcpy((char *)(mi_throttle->throttle), (char *)throttle_base, sizeof(throttle_base));
	if (0 != strncmp((char *)mi_throttle->throttle, (char *)throttle_base, sizeof(throttle_base))) {
		mi_throttle->throttle_init = false;
		pr_info("mi throttle init fail to copy\n");
		return;
	}
	mi_throttle->throttle_init = true;
	spin_lock_init(&mi_throttle->throttle_lock);
}

static inline bool limited(struct kiocb *iocb, struct iov_iter *iovec,
				enum throttle_type ty)
{
	struct file  *file = iocb->ki_filp;
	struct address_space *mapping = file->f_mapping;
	struct inode *inode = file_inode(file);
	struct block_device *s_bdev = inode->i_sb->s_bdev;
	mi_throttle_t *mi_throttle;
	unsigned long access_pages = 0;
	unsigned long hot_pages = 0;
	bool          is_hit;
	pgoff_t index, end;

	mi_throttle = &bdev_get_bdi(s_bdev)->mi_throttle;

	/* filter by file size */
	if (i_size_read(inode) < throttle_file_size)
		return false;

	if (task_type(current) == TOP)
		return false;

	index = iocb->ki_pos >> PAGE_SHIFT;
	end = (iocb->ki_pos + iov_iter_count(iovec) - 1) >> PAGE_SHIFT;
	access_pages = end - index + 1;
	if (iov_iter_rw(iovec) == READ) {
		hot_pages = filemap_range_nr_page(mapping, index, end);
		is_hit = (access_pages == hot_pages);
		if (is_hit)
			return false;
		access_pages = access_pages - hot_pages;
	}
	current->nr_access += access_pages;

	if (current->nr_access < LIMIT_PAGES)
		return false;

	/* filter bw by window */
	if (iov_iter_rw(iovec) == READ) {
		if (current->ioac.last_ioac.rbw_char > mi_throttle->throttle[ty].window_high) {
			current->read_throttling = true;
			return true;
		}
		if (current->read_throttling &&
				current->ioac.last_ioac.rbw_char > mi_throttle->throttle[ty].window_low)
			return true;
		current->read_throttling = false;
		return false;
	}

	if (iov_iter_rw(iovec) == WRITE) {
		if (current->ioac.last_ioac.wbw_char > mi_throttle->throttle[ty].window_high) {
			current->write_throttling = true;
			return true;
		}
		if (current->write_throttling &&
				current->ioac.last_ioac.wbw_char > mi_throttle->throttle[ty].window_low)
			return true;
		current->write_throttling = false;
		return false;
	}

	return false;
}

static inline enum throttle_type throttle_type(struct kiocb *iocb)
{
	int task_group;
	enum throttle_type throttle_type;
	mi_throttle_t *mi_throttle;
	struct file     *file = iocb->ki_filp;
	struct inode    *inode = file_inode(file);
	struct block_device *s_bdev = inode->i_sb->s_bdev;

	mi_throttle = &bdev_get_bdi(s_bdev)->mi_throttle;
	task_group = task_type(current);
	/* stime limit */
	if (task_group > GL)
		throttle_type = FG_THROTTLE;
	else if (task_group < GL) {
		if (NATIVE == task_group && !native_need_limit())
			throttle_type = NO_THROTTLE;
		else
			throttle_type = BG_THROTTLE;
	} else
		throttle_type = NO_THROTTLE;

	if (atomic64_read(bdev_get_stime(s_bdev)) < LIMIT_STIME_MIN)
		throttle_type = NO_THROTTLE;

	/* filter by cgroup */
	if (task_group > GL &&
		atomic64_read(bdev_get_stime(s_bdev)) < LIMIT_STIME_HIGH)
		throttle_type = NO_THROTTLE;

	return throttle_type;
}

static inline void filepage_throttle(struct kiocb *iocb, enum throttle_type type)
{
	struct file     *file = iocb->ki_filp;
	struct inode    *inode = file_inode(file);
	struct block_device *s_bdev = inode->i_sb->s_bdev;
	mi_throttle_t *mi_throttle;
	unsigned int  timeout;
	u64 io_time;

	mi_throttle = &bdev_get_bdi(s_bdev)->mi_throttle;

	spin_lock(&mi_throttle->throttle_lock);
	io_time = atomic64_read(bdev_get_stime(s_bdev));
	if (mi_throttle->throttle[type].stabling) {
		if (jiffies_to_msecs(elapsed_jiffies(mi_throttle->throttle[type].stable_start_time)) < mi_throttle->throttle[type].stable_time)
			goto stable;
		else
			mi_throttle->throttle[type].stabling = false;
	}

	if (io_time < mi_throttle->throttle[type].iotime_threshold_down &&
			mi_throttle->throttle[type].throttle_time != 0) {
		mi_throttle->throttle[type].throttle_time -= mi_throttle->throttle[type].delay_down_step;
		if (mi_throttle->throttle[type].throttle_time < 0) {
			mi_throttle->throttle[type].throttle_time = 0;
		} else {
			mi_throttle->throttle[type].stable_start_time = jiffies;
			mi_throttle->throttle[type].stable_time = mi_throttle->throttle[type].stable_time_speed_up;
			mi_throttle->throttle[type].stabling = true;
		}
	} else if (io_time >= mi_throttle->throttle[type].iotime_threshold_up &&
			mi_throttle->throttle[type].throttle_time != mi_throttle->throttle[type].delay_max) {
		mi_throttle->throttle[type].throttle_time += mi_throttle->throttle[type].delay_up_step;
		if (mi_throttle->throttle[type].throttle_time > mi_throttle->throttle[type].delay_max) {
			mi_throttle->throttle[type].throttle_time = mi_throttle->throttle[type].delay_max;
		} else {
			mi_throttle->throttle[type].stable_start_time = jiffies;
			mi_throttle->throttle[type].stable_time = mi_throttle->throttle[type].stable_time_slow_down;
			mi_throttle->throttle[type].stabling = true;
		}
	}

stable:
	timeout = mi_throttle->throttle[type].throttle_time;
	spin_unlock(&mi_throttle->throttle_lock);

	if (timeout > 0) {
		__set_current_state(TASK_KILLABLE);
		io_schedule_timeout(msecs_to_jiffies(timeout));
	}
	if (sysctl_mi_iolimit & IO_DEBUG_MASK)
		pr_info("%d:%d name=%s pid=%d nr_access=%d iotime=%llu iolimit=%d mode=%d rt=%d rbw=%llu wt=%d wbw=%llu file_size=%lld file_name=%s\n",
			MAJOR(inode->i_sb->s_dev), MINOR(inode->i_sb->s_dev),
			current->comm, current->pid,
			current->nr_access, io_time, timeout, type,
			current->read_throttling, current->ioac.last_ioac.rbw_char,
			current->write_throttling, current->ioac.last_ioac.wbw_char,
			i_size_read(inode),
			NULL != file->f_path.dentry ? file->f_path.dentry->d_name.name : NULL);
	current->nr_access = 0;
}

inline void mi_throttle(struct kiocb *iocb, struct iov_iter *iodata)
{
	enum throttle_type ty;

	if (!throttle_check(iocb))
		return;

	ty = throttle_type(iocb);
	if (NO_THROTTLE == ty)
		return;

	if (!limited(iocb, iodata, ty)) {
		if (current->nr_access >= LIMIT_PAGES)
			current->nr_access = 0;
		return;
	}

	filepage_throttle(iocb, ty);
}
