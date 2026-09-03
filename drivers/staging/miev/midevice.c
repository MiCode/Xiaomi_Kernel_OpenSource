// MIUI ADD: MiSight_LogEnhance

#define pr_fmt(fmt) "miev: " fmt

#include "midevice.h"
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include <linux/version.h>

static int miev_open(struct inode *inode, struct file *filp);
static int miev_release(struct inode *inode, struct file *filp);
static long miev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static ssize_t miev_read(struct file *filp, char __user *ubuf, size_t size,
			 loff_t *offset);
static ssize_t miev_write(struct file *filp, const char __user *ubuf,
			  size_t size, loff_t *offset);

/* Kfifo buffer size */
#define FIFO_SIZE (64 * 1024)
// new workqueue for irq context
#define WORK_MAX_CNT  10
struct miev_work_struct {
	bool   in_use;
	bool   in_work;
	int   size;
	int   offset;
	char  *msg;
	struct work_struct wk;
};
struct workqueue_struct *miev_workqueue = NULL;
struct miev_work_struct miev_works[WORK_MAX_CNT];
static DEFINE_RAW_SPINLOCK(miev_spinlock);
static int bInited = 0;
static int nwork_index = 0;

static int miev_open(struct inode *inode, struct file *filp)
{
	try_module_get(THIS_MODULE);
	return 0;
}

static int miev_release(struct inode *inode, struct file *filp)
{
	module_put(THIS_MODULE);
	wake_up_interruptible(&miev_dev->wait_queue);
	return 0;
}

int write_buf_inner(char *kbuf, int size)
{
	int ret = 0;
	int tmp_in_size = 0;
	int max_cnt = (BUF_MAX_SIZE / sizeof(int)) * 2;

	mutex_lock(&miev_dev->lock);
	size = size < BUF_MAX_SIZE ? size : BUF_MAX_SIZE;
	while (kfifo_avail(&miev_dev->fifo) < size + sizeof(int)) {
		if (kfifo_out(&miev_dev->fifo, &tmp_in_size, sizeof(int))) {
			if((tmp_in_size <= 0 || tmp_in_size > BUF_MAX_SIZE) ||
				!kfifo_out(&miev_dev->fifo, miev_dev->cbuf, tmp_in_size)) {
				pr_err("write_buf_in:kfifo_out failed, reset fifo");
				kfifo_reset(&miev_dev->fifo);
			}
		}
		max_cnt--;
		if (max_cnt < 0) {
			pr_err("write_buf_in:try max cnt, not find enough space");
			mutex_unlock(&miev_dev->lock);
			return ret;
		}
	}
	if (kfifo_in(&miev_dev->fifo, &size, sizeof(int))) {
		ret = kfifo_in(&miev_dev->fifo, kbuf, size);
		if (!ret) {
			pr_err("write_buf_in:kfifo_in write inner buf failed, reset fifo");
			kfifo_reset(&miev_dev->fifo);
		}
	}
	mutex_unlock(&miev_dev->lock);
	wake_up_interruptible(&miev_dev->wait_queue);

	return ret;
}

char** miev_get_work_msg(void)
{
	int i;
	struct miev_work_struct *p_wk = NULL;
	unsigned long flags;

	if (!bInited) {
		pr_err("miev_get_work:miev not init");
		return NULL;
        }

	raw_spin_lock_irqsave(&miev_spinlock, flags);
	p_wk = &miev_works[nwork_index];
	if (p_wk->in_use == false && p_wk->in_work == false) {
		p_wk->in_use = true;
		nwork_index++;
		if (nwork_index >= WORK_MAX_CNT) {
			nwork_index = 0;
        	}
	} else {
		p_wk = NULL;
		for (i = 0; i < WORK_MAX_CNT; i++) {
			if (miev_works[i].in_use || miev_works[i].in_work) {
				continue;
			}
			p_wk = &miev_works[i];
			p_wk->in_use = true;
			break;
		}
	}
	raw_spin_unlock_irqrestore(&miev_spinlock, flags);
	if (p_wk == NULL || p_wk->msg == NULL) {
		pr_err("miev_get_work:invalid, wk=%p msg=%p", p_wk, (p_wk ? p_wk->msg : NULL));
		return NULL;
	}
	memset(p_wk->msg, 0, BUF_MAX_SIZE);
	return &(p_wk->msg);
}

bool check_work(struct miev_work_struct *wk)
{
	return (wk >= miev_works && wk < miev_works + WORK_MAX_CNT);
}

void miev_release_work_msg(char **kbuf)
{
	struct miev_work_struct *p_wk = NULL;

	if (kbuf == NULL || *kbuf == NULL) {
		pr_err("release kbuf input invalid");
		return;
	}
	p_wk = container_of(kbuf, struct miev_work_struct, msg);
	if (!check_work(p_wk)) {
		pr_err("released wk=%p msg=%p addr=%p invalid, not in [%p,%p]", p_wk, *kbuf, kbuf,
			miev_works, miev_works + WORK_MAX_CNT);
		return;
	}
	p_wk->in_use = false;
}

int write_kbuf(char **kbuf, int offset, int size)
{
	struct miev_work_struct *p_wk = NULL;

	if (size > BUF_MAX_SIZE) {
		pr_err("buf over size");
		return -1;
	}
	if (kbuf == NULL || *kbuf == NULL) {
		pr_err("write_buf:kbuf input invalid");
		return -1;
	}
	p_wk = container_of(kbuf, struct miev_work_struct, msg);
	if (!check_work(p_wk)) {
		pr_err("write_buf:wk=%p msg=%p addr=%p invalid, not in [%p,%p]", p_wk, *kbuf, kbuf, miev_works, miev_works + WORK_MAX_CNT);
		return -1;
	}

	p_wk->size = size;
	p_wk->offset = offset;
        if (!in_interrupt()) {
		return write_buf_inner((*kbuf) + offset, size);
        }
	p_wk->in_work = true;
	queue_work(miev_workqueue, &(p_wk->wk));

	return 0;
}

static void do_miev_work(struct work_struct *work)
{
	struct miev_work_struct *p_wk = container_of(work, struct miev_work_struct, wk);
	write_buf_inner(p_wk->msg + p_wk->offset, p_wk->size);
	p_wk->in_work = false;
}

static long miev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	if (_IOC_TYPE(cmd) != MIEV_IOC_MAGIC)
		return -EINVAL;
	if (_IOC_NR(cmd) > MIEV_IOC_MAXNR)
		return -EINVAL;

	if (!access_ok((void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	switch (cmd) {
	case MIEV_IOC_WRITE:
		pr_debug("MIEV_IOC_WRITE");
		break;
	case MIEV_IOC_READ:
		pr_debug("MIEV_IOC_READ");
		break;
	case MIEV_IOC_NONE:
		pr_debug("MIEV_IOC_NONE");
		break;
	default:
		return -ENOTTY;
	}
	return ret;
}

static __poll_t miev_poll(struct file *filp, poll_table *wait)
{
	__poll_t mask = 0;
	poll_wait(filp, &miev_dev->wait_queue, wait);
	mutex_lock(&miev_dev->lock);
	if (!kfifo_is_empty(&miev_dev->fifo)) {
		mask |= POLLIN | POLLRDNORM;
	}
	mutex_unlock(&miev_dev->lock);
	return mask;
}

static ssize_t miev_read(struct file *filp, char __user *ubuf, size_t size,
			 loff_t *offset)
{
	int ret = 0;
	int fifo_len = 0;
	int tmp_out_size = 0;

	mutex_lock(&miev_dev->lock);
	if (!kfifo_is_empty(&miev_dev->fifo)) {
		fifo_len = kfifo_len(&miev_dev->fifo);
		size = size < BUF_MAX_SIZE ? size : BUF_MAX_SIZE;
		size = size < fifo_len ? size : fifo_len;
		if (kfifo_out(&miev_dev->fifo, &tmp_out_size, sizeof(int))) {
			if ((tmp_out_size <= 0 || tmp_out_size > BUF_MAX_SIZE) ||
				kfifo_to_user(&miev_dev->fifo, ubuf, tmp_out_size, &ret)) {
				kfifo_reset(&miev_dev->fifo);
				mutex_unlock(&miev_dev->lock);
				pr_err("copy_to_user failed");
				return -EFAULT;
			}
		}
	}
	mutex_unlock(&miev_dev->lock);
	return ret;
}

static ssize_t miev_write(struct file *filp, const char __user *ubuf,
			  size_t size, loff_t *offset)
{
	int ret = 0;
	int tmp_in_size = 0;
	int max_cnt = (BUF_MAX_SIZE / sizeof(int)) * 2;

	mutex_lock(&miev_dev->lock);
	size = size < BUF_MAX_SIZE ? size : BUF_MAX_SIZE;
	while (kfifo_avail(&miev_dev->fifo) < size + sizeof(int)) {
		if (kfifo_out(&miev_dev->fifo, &tmp_in_size, sizeof(int))) {
			if((tmp_in_size <= 0 || tmp_in_size > BUF_MAX_SIZE) ||
				!kfifo_out(&miev_dev->fifo, miev_dev->cbuf, tmp_in_size)) {
				pr_err("kfifo_out failed");
				kfifo_reset(&miev_dev->fifo);
			}
		}
		max_cnt--;
		if (max_cnt < 0) {
			pr_err("try max cnt, not find enough space");
			mutex_unlock(&miev_dev->lock);
			return -EFAULT;
		}
	}
	if (copy_from_user(miev_dev->cbuf, (char __user *)ubuf, size)) {
		pr_err("copy miev_buffer from user failed");
		mutex_unlock(&miev_dev->lock);
		return -EFAULT;
	}
	if (kfifo_in(&miev_dev->fifo, &size, sizeof(int))) {
		ret = kfifo_in(&miev_dev->fifo, miev_dev->cbuf, size);
		if (!ret) {
			pr_err("write buf failed,reset fifo");
			kfifo_reset(&miev_dev->fifo);
		}
	}
	mutex_unlock(&miev_dev->lock);
	wake_up_interruptible(&miev_dev->wait_queue);

	return size;
}

static struct file_operations miev_fops = {
	.owner = THIS_MODULE,
	.open = miev_open,
	.release = miev_release,
	.read = miev_read,
	.write = miev_write,
	.unlocked_ioctl = miev_ioctl,
	.poll = miev_poll,
};

static int __init miev_init_module(void)
{
	int i;
	int ret = 0;
	struct device *my_device;

	if (bInited) {
		pr_err("already init");
		return 0;
	}
	nwork_index = 0;

	miev_dev = kmalloc(sizeof(struct miev_device), GFP_KERNEL);
	if (!miev_dev) {
		pr_err("Fail to create miev_device");
		ret = -ENOMEM;
		goto err;
	}
	miev_dev->dev_no = MKDEV(DEV_MAJOR, DEV_MINOR);
	ret = register_chrdev_region(miev_dev->dev_no, 1, DEV_NAME);
	if (ret < 0) {
		ret = alloc_chrdev_region(&miev_dev->dev_no, 0, 1, DEV_NAME);
		if (ret < 0) {
			ret = -ENODEV;
			pr_err("Fail to register_chrdev_region");
			goto err_register;
		}
	}
	cdev_init(&miev_dev->chrdev, &miev_fops);
	miev_dev->chrdev.owner = THIS_MODULE;
	ret = cdev_add(&miev_dev->chrdev, miev_dev->dev_no, 1);
	if (ret < 0) {
		pr_err("Fail to add miev_dev");
		goto err_cdev;
	} else {
		if (kfifo_alloc(&miev_dev->fifo, FIFO_SIZE, GFP_KERNEL)) {
			pr_err("kfifo_alloc create failed");
			ret = -ENOMEM;
			goto err_kfifo;
		}
	}
	#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
		miev_dev->my_class = class_create(DEV_NAME);
	#else
		miev_dev->my_class = class_create(THIS_MODULE, DEV_NAME);
	#endif
	if (IS_ERR(miev_dev->my_class)) {
		pr_err("class create failed!");
		ret = PTR_ERR(miev_dev->my_class);
		goto err_class;
	}

	my_device = device_create(miev_dev->my_class, NULL, miev_dev->dev_no,
				  NULL, DEV_NAME);
	if (IS_ERR(my_device)) {
		pr_err("device create failed!");
		ret = PTR_ERR(my_device);
		goto err_device;
	}

	mutex_init(&miev_dev->lock);
	init_waitqueue_head(&miev_dev->wait_queue);

	// add miev workqueue
	miev_workqueue = create_singlethread_workqueue("miev_workqueue");
	if (miev_workqueue == NULL) {
		ret = -EINVAL;
		pr_err("create workqueue failed");
		goto err_queue_in;
	}
	for (i = 0; i < WORK_MAX_CNT; i++) {
		miev_works[i].in_use = false;
		miev_works[i].in_work = false;
		miev_works[i].size = 0;
		miev_works[i].offset = 0;
		miev_works[i].msg = kmalloc(BUF_MAX_SIZE, GFP_KERNEL);
		pr_debug("i=%d,p_wk=%p,msg=%p", i, &miev_works[i], miev_works[i].msg);
		if (miev_works[i].msg == NULL) {
			goto err_mem_alloc;
		}
		INIT_WORK(&miev_works[i].wk, do_miev_work);
	}
	// end workqueue

	bInited = 1;
	return 0;
err_mem_alloc:
	for (i = 0; i < WORK_MAX_CNT; i++) {
		if (miev_works[i].msg != NULL) {
			kfree(miev_works[i].msg);
		}
	}
	destroy_workqueue(miev_workqueue);
	miev_workqueue = NULL;
err_queue_in:
	device_destroy(miev_dev->my_class, miev_dev->dev_no);
err_device:
	class_destroy(miev_dev->my_class);
err_class:
	kfifo_free(&miev_dev->fifo);
err_kfifo:
	cdev_del(&miev_dev->chrdev);
err_cdev:
	unregister_chrdev_region(miev_dev->dev_no, 1);
err_register:
	kfree(miev_dev);
err:
	return ret;
}

static void __exit miev_exit_module(void)
{
	int i;
	bInited = 0;

	device_destroy(miev_dev->my_class, miev_dev->dev_no);
	class_destroy(miev_dev->my_class);
	kfifo_free(&miev_dev->fifo);
	cdev_del(&miev_dev->chrdev);
	unregister_chrdev_region(miev_dev->dev_no, 1);
	kfree(miev_dev);

	for (i = 0; i < WORK_MAX_CNT; i++) {
		if (miev_works[i].msg != NULL) {
			int wait_cnt = 10;
			while (wait_cnt > 0 && (miev_works[i].in_use || miev_works[i].in_work)) {
				wait_cnt--;
				msleep(100);
			}
			kfree(miev_works[i].msg);
		}
	}

	flush_workqueue(miev_workqueue);
	destroy_workqueue(miev_workqueue);
	miev_workqueue = NULL;
}

module_init(miev_init_module);
module_exit(miev_exit_module);

MODULE_AUTHOR("huangqibo <huangqibo@xiaomi.com>");
MODULE_DESCRIPTION("exception log transfer.");
MODULE_LICENSE("GPL");
// END MiSight_LogEnhance
