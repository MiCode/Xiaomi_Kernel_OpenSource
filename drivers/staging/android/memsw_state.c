#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/kthread.h>

#include "memsw_state.h"

#define memsw_freeram      (global_page_state(NR_FREE_PAGES) - totalreserve_pages)
#define memsw_filepage     (global_page_state(NR_FILE_PAGES) - total_swapcache_pages)

#define MEMSW_FREEMEM_KB   ((memsw_freeram + memsw_filepage) << (PAGE_SHIFT - 10))
#define MEMSW_FREESWAP_KB  (get_nr_swap_pages() << (PAGE_SHIFT - 10))
#define MEMSW_EXTRA_KB     (16 << (PAGE_SHIFT - 10))

static int memsw_dev_open(struct inode *inode, struct file *file)
{
	struct memsw_dev *memsw_dev;
	struct memsw_reader *reader;
	int ret, i = MINOR(inode->i_rdev);


	memsw_dev = memsw_dev_get_w_check(i);
	if (memsw_dev == NULL)
		return -ENODEV;


	if (!(file->f_mode & FMODE_READ))
		ret = -EBADF;

	ret = nonseekable_open(inode, file);
	if (ret)
		return ret;

	reader = kmalloc(sizeof(struct memsw_reader), GFP_KERNEL);
	if (!reader)
		return -ENOMEM;

	reader->dev = memsw_dev;
	reader->version = 1;
	reader->auth_rights = in_egroup_p(inode->i_gid) || capable(CAP_SYS_ADMIN);
	INIT_LIST_HEAD(&reader->list);

	mutex_lock(&memsw_dev->mutex);
	list_add_tail(&reader->list, &memsw_dev->reader_list);
	mutex_unlock(&memsw_dev->mutex);

	file->private_data = reader;

	return 0;
}

static inline void memsw_dev_update_data(struct memsw_dev *memsw_dev)
{
	mutex_lock(&memsw_dev->mutex);
	(memsw_dev->memsw_data).current_freeram = MEMSW_FREEMEM_KB;
	(memsw_dev->memsw_data).current_freeswap = MEMSW_FREESWAP_KB;
	mutex_unlock(&memsw_dev->mutex);
}

static inline int memsw_dev_event_pending(struct memsw_dev *memsw_dev)
{
	int pending = 0;

	mutex_lock(&memsw_dev->mutex);
	if ((memsw_dev->memsw_data).mem_threshold >= MEMSW_FREEMEM_KB) {
		(memsw_dev->memsw_data).low_mem_triggered = 1;
		pending = 1;
	} else {
		(memsw_dev->memsw_data).low_mem_triggered = 0;
	}
	if (((memsw_dev->memsw_data).swap_threshold >= MEMSW_FREESWAP_KB) && (total_swap_pages != 0)) {
		(memsw_dev->memsw_data).low_swap_triggered = 1;
		pending = 1;
	} else {
		(memsw_dev->memsw_data).low_swap_triggered = 0;
	}
	mutex_unlock(&memsw_dev->mutex);

	return pending;
}

static ssize_t memsw_dev_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	ssize_t ret = 0;
	struct memsw_reader *reader = file->private_data;
	struct memsw_dev *memsw_dev = reader->dev;

	if (!(reader->auth_rights))
		return -EPERM;

	if (!memsw_dev_event_pending(memsw_dev) && (file->f_flags & O_NONBLOCK))
		return -EAGAIN;

	wait_event_interruptible(memsw_dev->wq, memsw_dev_event_pending(memsw_dev));

	memsw_dev_update_data(memsw_dev);
	if (copy_to_user(buf, &memsw_dev->memsw_data, sizeof(struct memsw_state_data)))
		return -EFAULT;

	ret = sizeof(struct memsw_state_data);

	return ret;
}

static unsigned int memsw_dev_poll(struct file *file, poll_table *wait)
{
	struct memsw_reader *reader = file->private_data;
	struct memsw_dev *memsw_dev = reader->dev;

	poll_wait(file, &memsw_dev->wq, wait);
	return memsw_dev_event_pending(memsw_dev) ? (POLLIN | POLLRDNORM) : 0;
}

static long memsw_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -EINVAL;
	struct memsw_reader *reader = file->private_data;
	struct memsw_dev *memsw_dev = reader->dev;
	void __user *argp = (void __user *) arg;

	if (!(reader->auth_rights))
		return -EPERM;

	mutex_lock(&memsw_dev->mutex);

	switch (cmd) {
	case GET_MEM_THRESHOLD:
		ret = copy_to_user(argp, &(memsw_dev->memsw_data.mem_threshold), sizeof(unsigned long));
		break;
	case SET_MEM_THRESHOLD:
		ret = copy_from_user(&(memsw_dev->memsw_data.mem_threshold), argp, sizeof(unsigned long));
		if (ret == 0) {
			if ((memsw_dev->memsw_data).mem_threshold <= MEMSW_FREEMEM_KB) {
				(memsw_dev->memsw_data).low_mem_triggered = 1;
				wake_up_interruptible(&memsw_dev->wq);
			}
		}
		break;
	case GET_SWAP_THRESHOLD:
		ret = copy_to_user(argp, &(memsw_dev->memsw_data.swap_threshold), sizeof(unsigned long));
		break;
	case SET_SWAP_THRESHOLD:
		ret = copy_from_user(&(memsw_dev->memsw_data.swap_threshold), argp, sizeof(unsigned long));
		if (ret == 0) {
			if ((memsw_dev->memsw_data).swap_threshold <= MEMSW_FREESWAP_KB) {
				(memsw_dev->memsw_data).low_swap_triggered = 1;
				wake_up_interruptible(&memsw_dev->wq);
			}
		}
		break;
	case GET_MEMSW_DEV_VERSION:
		ret = copy_to_user(argp, &(reader->version), sizeof(uint8_t));
		break;
	case SET_MEMSW_DEV_VERSION:
		ret = copy_from_user(&(reader->version), argp, sizeof(uint8_t));
		break;
	}

	mutex_unlock(&memsw_dev->mutex);

	return ret;
}

static int memsw_dev_release(struct inode *inode, struct file *file)
{
	struct memsw_reader *reader = file->private_data;
	struct memsw_dev *memsw_dev = reader->dev;

	mutex_lock(&memsw_dev->mutex);
	list_del(&reader->list);
	mutex_unlock(&memsw_dev->mutex);
	kfree(reader);

	return 0;
}

static const struct file_operations memsw_dev_fops = {
	.owner = THIS_MODULE,
	.open = memsw_dev_open,
	.read = memsw_dev_read,
	.poll = memsw_dev_poll,
	.unlocked_ioctl = memsw_dev_ioctl,
	.compat_ioctl = memsw_dev_ioctl,
	.release = memsw_dev_release,
	.llseek	= no_llseek,
};

static struct memsw_dev memsw_state_dev = {
	.memsw_data = { 0 },
	.misc_dev = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "xiaomi_memsw_state",
		.fops = &memsw_dev_fops,
		.parent = NULL,
	},
	.wq = __WAIT_QUEUE_HEAD_INITIALIZER(memsw_state_dev.wq),
	.reader_list = LIST_HEAD_INIT(memsw_state_dev.reader_list),
	.mutex = __MUTEX_INITIALIZER(memsw_state_dev.mutex),
};

struct memsw_dev *memsw_dev_get_wo_check(void)
{
	return &memsw_state_dev;
}

struct memsw_dev *memsw_dev_get_w_check(int minor)
{
	if (memsw_state_dev.misc_dev.minor == minor)
		return &memsw_state_dev;
	else
		return NULL;
}

uint8_t memsw_is_below_limit(uint8_t flag)
{
	unsigned long cur_free, threshold;
	struct memsw_dev *memsw_dev = memsw_dev_get_wo_check();

	cur_free = flag ? MEMSW_FREESWAP_KB : MEMSW_FREEMEM_KB;
	threshold = flag ? memsw_dev->memsw_data.swap_threshold : memsw_dev->memsw_data.mem_threshold;

	if (cur_free <= threshold)
		return 1;
	else
		return 0;
}

uint8_t memsw_is_above_limit(uint8_t flag)
{
	unsigned long cur_free, threshold;
	struct memsw_dev *memsw_dev = memsw_dev_get_wo_check();

	cur_free = flag ? MEMSW_FREESWAP_KB : MEMSW_FREEMEM_KB;
	threshold = flag ? memsw_dev->memsw_data.swap_threshold : memsw_dev->memsw_data.mem_threshold;

	if (cur_free > (threshold + MEMSW_EXTRA_KB))
		return 1;
	else
		return 0;
}

static inline int memsw_dev_check_pending(void)
{
	struct memsw_dev *memsw_dev = memsw_dev_get_wo_check();

	if (memsw_dev_event_pending(memsw_dev))
		wake_up_interruptible(&memsw_dev->wq);

	return 0;
}

static int mem_stat_chk_shrink(struct shrinker *s, struct shrink_control *sc)
{
	struct memsw_dev *memsw_dev = memsw_dev_get_wo_check();

	if ((((memsw_dev->memsw_data).low_mem_triggered == 0) && memsw_is_below_limit(0)) ||
		(((memsw_dev->memsw_data).low_swap_triggered == 0) && memsw_is_below_limit(1)))
		memsw_dev_check_pending();

	return 0;
}

static struct shrinker mem_stat_chk_shrinker = {
	.shrink = mem_stat_chk_shrink,
	.seeks = DEFAULT_SEEKS * 16
};

static struct task_struct *kmemswchkd_ktp;
static int __init kmemswchkd_init(void)
{
	int ret = 0;
	struct memsw_dev *memsw_dev = memsw_dev_get_wo_check();

	register_shrinker(&mem_stat_chk_shrinker);
	ret = misc_register(&memsw_dev->misc_dev);
	if (ret < 0) {
		printk(KERN_WARNING "misc register device 'memsw_state' failed: %d", ret);
		goto err_out;
	}

	if (kmemswchkd_ktp)
		return -EPERM;
err_out:
	return ret;
}

static void __init kmemswchkd_exit(void)
{
	struct memsw_dev *memsw_dev = memsw_dev_get_wo_check();
	misc_deregister(&memsw_dev->misc_dev);

	if (kmemswchkd_ktp != NULL) {
		kthread_stop(kmemswchkd_ktp);
		kmemswchkd_ktp = NULL;
	}
}

module_init(kmemswchkd_init);
module_exit(kmemswchkd_exit);

MODULE_LICENSE("GPL");
