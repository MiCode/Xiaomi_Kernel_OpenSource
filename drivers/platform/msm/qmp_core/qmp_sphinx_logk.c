/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "qmp_sphinx: %s: " fmt, __func__

#include "qmp_sphinx_logk.h"
#include "qmp_sphinx_ringbuf.h"

#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define MASK_BUFFER_SIZE 256
#define FOUR_MB 4
#define USER_APP_START_UID 10000
#define YEAR_BASE 1900

static struct qmp_sphinx_logk_dev *slogk_dev;

static unsigned int ring_sz = FOUR_MB;

/*
 * default is besteffort, apps do not get blocked
 */
static unsigned int block_apps;


/*
 * When this flag is turned on,
 * kmalloc should be used for ring buf allocation
 * otherwise it is vmalloc.
 * default is to use vmalloc
 * kmalloc has a limit of 4MB
 */
unsigned int kmalloc_flag;

static struct class *cl;

static rwlock_t filter_lock;
static struct qmp_sphinx_source_mask *pmask;
static unsigned int num_sources;

static long qmp_sphinx_logk_reserve_rdblks(
		struct qmp_sphinx_logk_dev *sdev, unsigned long arg);
static long qmp_sphinx_logk_set_mask(unsigned long arg);
static long qmp_sphinx_logk_set_mapping(unsigned long arg);
static long qmp_sphinx_logk_check_filter(unsigned long arg);

void* (*qmp_sphinx_logk_kernel_begin)(char **buf);

void (*qmp_sphinx_logk_kernel_end) (void *blck);

/*
 * the last param is the permission bits *
 * kernel logging is done in three steps:
 * (1)  fetch a block, fill everything except payload.
 * (2)  return payload pointer to the caller.
 * (3)  caller fills its data directly into the payload area.
 * (4)  caller invoked finish_record(), to finish writing.
 */
void *qmp_sphinx_logk_kernel_start_record(char **buf)
{
	struct qmp_sphinx_logk_blk *blk;
	struct timespec now;
	struct tm ts;
	int idx;
	int ret;

	DEFINE_WAIT(write_wait);
	ret = 0;
	idx = 0;
	now = current_kernel_time();
	blk = ringbuf_fetch_wr_block(slogk_dev);
	if (!blk) {
		/*
		 * there is no blk to write
		 * if block_apps == 0; quietly return
		 */
		if (!block_apps) {
			*buf = NULL;
			return NULL;
		}
		/*else wait for the blks to be available*/
		while (1) {
			mutex_lock(&slogk_dev->lock);
			prepare_to_wait(&slogk_dev->writers_wq,
				&write_wait, TASK_INTERRUPTIBLE);
			ret = (slogk_dev->num_write_avail_blks <= 0);
			if (!ret) {
				/* don't have to wait*/
				break;
			}
			mutex_unlock(&slogk_dev->lock);
			if (signal_pending(current)) {
				ret = -EINTR;
				break;
			}
			schedule();
		}

		finish_wait(&slogk_dev->writers_wq, &write_wait);
		if (ret) {
			mutex_unlock(&slogk_dev->lock);
			return NULL;
		}

		idx = slogk_dev->write_idx;
		slogk_dev->write_idx =
			(slogk_dev->write_idx + 1) % slogk_dev->num_tot_blks;
		slogk_dev->num_write_avail_blks--;
		slogk_dev->num_write_in_prog_blks++;
		slogk_dev->num_writers++;

		blk = &slogk_dev->ring[idx];
		/*mark block invalid*/
		blk->status = 0x0;
		mutex_unlock(&slogk_dev->lock);
	}

	blk->version = OBSERVER_VERSION;
	blk->pid = current->tgid;
	blk->tid = current->pid;
	blk->uid = current_uid();
	blk->sec = now.tv_sec;
	blk->nsec = now.tv_nsec;
	strlcpy(blk->appname, current->comm, TASK_COMM_LEN);
	time_to_tm(now.tv_sec, 0, &ts);
	ts.tm_year += YEAR_BASE;
	ts.tm_mon += 1;

	snprintf(blk->ts, TS_SIZE, "%04ld-%02d-%02d %02d:%02d:%02d",
			ts.tm_year, ts.tm_mon, ts.tm_mday,
			ts.tm_hour, ts.tm_min, ts.tm_sec);

	*buf = blk->msg;

	return blk;
}

void qmp_sphinx_logk_kernel_end_record(void *blck)
{
	struct qmp_sphinx_logk_blk *blk = (struct qmp_sphinx_logk_blk *)blck;
	if (blk) {
		blk->len = strlen(blk->msg);
		/*update status at the very end*/
		blk->status |= 0x1;
		ringbuf_finish_writer(slogk_dev);
	}
}

/*
 * get_uid_from_message_for_system_event() - helper function to get the
 * uid of the actual app that is changing the state and updating it
 * accordingly rather than with the system UID = 1000
 * NOTE: Not a very efficient implementation. This does a N*8 character
 * comparisons everytime a message with UID less than 10000 is seen
 */
int get_uid_from_message_for_system_event(const char *buffer)
{
	char asciiuid[6];
	long appuid = 0;
	int aindex = 0;
	char *comparator_string = "app_uid=";
	int ret = 0;

	char *p1 = (char *)buffer;

	while (*p1) {
		char *p1begin = p1;
		char *p2 = (char *)comparator_string;
		aindex = 0;

		while (*p1 && *p2 && *p1 == *p2) {
			p1++;
			p2++;
		}

		if (*p2 == '\0') {
			while (*p1 != ',') {
				asciiuid[aindex++] = *p1;
				p1++;
			}
			asciiuid[aindex] = '\0';

			/*
			 * now get the integer value of this ascii
			 * string number
			 */
			ret = kstrtol(asciiuid, 10, &appuid);
			if (ret != 0) {
				pr_err("failed in the kstrtol function uid:%s\n",
						asciiuid);
				return ret;
			} else {
				return (int)(appuid);
			}
		}

		p1 = p1begin + 1;
	}
	return -EPERM;
}

static int qmp_sphinx_logk_usr_record(const char __user *buf, size_t count)
{
	struct qmp_sphinx_logk_blk *blk;
	struct timespec now;
	struct tm ts;
	int idx, ret;
	int currentuid;
	int parsedcurrentuid;

	DEFINE_WAIT(write_wait);
	idx = ret = 0;
	now = current_kernel_time();
	blk = ringbuf_fetch_wr_block(slogk_dev);
	if (!blk) {
		/*just drop this log*/
		if (!block_apps)
			return 0;
		while (1) {
			mutex_lock(&slogk_dev->lock);
			prepare_to_wait(&slogk_dev->writers_wq,
					&write_wait,
					TASK_INTERRUPTIBLE);
			ret = (slogk_dev->num_write_avail_blks <= 0);
			if (!ret) {
				/*don't have to wait*/
				break;
			}

			mutex_unlock(&slogk_dev->lock);

			if (signal_pending(current)) {
				ret = -EINTR;
				break;
			}
			schedule();
		}

		finish_wait(&slogk_dev->writers_wq, &write_wait);
		if (ret) {
			mutex_unlock(&slogk_dev->lock);
			return -EINTR;
		}
		idx = slogk_dev->write_idx;
		slogk_dev->write_idx =
			(slogk_dev->write_idx + 1) % slogk_dev->num_tot_blks;
		slogk_dev->num_write_avail_blks--;
		slogk_dev->num_write_in_prog_blks++;
		slogk_dev->num_writers++;

		blk = &slogk_dev->ring[idx];
		/*mark block invalid*/
		blk->status = 0x0;
		mutex_unlock(&slogk_dev->lock);
	}

	if (count > BLK_MAX_MSG_SZ-1)
		count = BLK_MAX_MSG_SZ-1;

	if (copy_from_user(blk->msg, buf, count) != 0)
		ret = -EFAULT;

	blk->msg[count] = '\0';
	blk->len = count;
	blk->pid = current->tgid;

	currentuid = current_uid();
	if (currentuid <= USER_APP_START_UID) {
		parsedcurrentuid = get_uid_from_message_for_system_event
								(blk->msg);
		if (parsedcurrentuid != -EPERM)
			blk->uid = parsedcurrentuid;
		else
			blk->uid = currentuid;
	} else
		blk->uid = currentuid;

	blk->tid = current->pid;
	blk->sec = now.tv_sec;
	blk->nsec = now.tv_nsec;

	time_to_tm(now.tv_sec, 0, &ts);
	ts.tm_year += YEAR_BASE;
	ts.tm_mon += 1;
	snprintf(blk->ts, TS_SIZE, "%02ld-%02d-%02d %02d:%02d:%02d",
			ts.tm_year, ts.tm_mon, ts.tm_mday,
			ts.tm_hour, ts.tm_min, ts.tm_sec);

	strlcpy(blk->appname, current->comm, TASK_COMM_LEN);
	/*update status at the very end*/
	blk->status |= 0x1;

	ringbuf_finish_writer(slogk_dev);
	return ret;
}

static void qmp_sphinx_logk_attach(void)
{
	qmp_sphinx_logk_kernel_end = qmp_sphinx_logk_kernel_end_record;
	qmp_sphinx_logk_kernel_begin = qmp_sphinx_logk_kernel_start_record;
}

static void qmp_sphinx_logk_detach(void)
{
	qmp_sphinx_logk_kernel_begin = NULL;
	qmp_sphinx_logk_kernel_end = NULL;
}

static ssize_t
qmp_sphinx_logk_write(struct file *file, const char __user *buf, size_t count,
		loff_t *ppos)
{
	return qmp_sphinx_logk_usr_record(buf, count);
}

static int
qmp_sphinx_logk_open(struct inode *inode, struct file *filp)
{
	int ret;

	/*disallow seeks on this file*/
	ret = nonseekable_open(inode, filp);
	if (ret) {
		pr_err("ret= %d\n", ret);
		return ret;
	}

	slogk_dev->minor = iminor(inode);
	filp->private_data = slogk_dev;

	return 0;
}

static bool qmp_sphinx_logk_get_bit_from_vector(__u8 *pVec, __u32 index)
{
	unsigned int byte_num = index/8;
	unsigned int bit_num = index%8;
	unsigned char byte = pVec[byte_num];

	return !(byte & (1 << bit_num));
}

static long qmp_sphinx_logk_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	struct qmp_sphinx_logk_dev *sdev;
	int ret;

	sdev = (struct qmp_sphinx_logk_dev *) filp->private_data;

	if (cmd == QMP_SPHINX_CMD_RESERVE_RDBLKS) {
		return qmp_sphinx_logk_reserve_rdblks(sdev, arg);
	} else if (cmd == QMP_SPHINX_CMD_RELEASE_RDBLKS) {
		mutex_lock(&sdev->lock);
		sdev->read_idx = (sdev->read_idx + sdev->num_read_in_prog_blks)
			% sdev->num_tot_blks;
		sdev->num_write_avail_blks += sdev->num_read_in_prog_blks;
		ret = sdev->num_read_in_prog_blks;
		sdev->num_read_in_prog_blks = 0;
		/*wake up any waiting writers*/
		mutex_unlock(&sdev->lock);
		if (ret && block_apps)
			wake_up_interruptible(&sdev->writers_wq);
	} else if (cmd == QMP_SPHINX_CMD_GET_RINGSZ) {
		if (copy_to_user((unsigned int *)arg, &sdev->ring_sz,
				sizeof(unsigned int)))
			return -EFAULT;
	} else if (cmd == QMP_SPHINX_CMD_GET_BLKSZ) {
		if (copy_to_user((unsigned int *)arg, &sdev->blk_sz,
				sizeof(unsigned int)))
			return -EFAULT;
	} else if (QMP_SPHINX_CMD_SET_MASK == cmd) {
		return qmp_sphinx_logk_set_mask(arg);
	} else if (QMP_SPHINX_CMD_SET_MAPPING == cmd) {
		return qmp_sphinx_logk_set_mapping(arg);
	} else if (QMP_SPHINX_CMD_CHECK_FILTER == cmd) {
		return qmp_sphinx_logk_check_filter(arg);
	} else {
		pr_err("Invalid Request %X\n", cmd);
		return -ENOIOCTLCMD;
	}
	return 0;
}

static long qmp_sphinx_logk_reserve_rdblks(
		struct qmp_sphinx_logk_dev *sdev, unsigned long arg)
{
	int ret;
	struct read_range rrange;

	DEFINE_WAIT(read_wait);
	mutex_lock(&sdev->lock);
	if (sdev->num_writers > 0 || sdev->num_read_avail_blks <= 0) {
		ret = -EPERM;
		pr_debug("(reserve): blocking, cannot read.\n");
		pr_debug("num_writers=%d num_read_avail_blks=%d\n",
		sdev->num_writers,
				sdev->num_read_avail_blks);
		mutex_unlock(&sdev->lock);
		/*
		 * unlock the device
		 * wait on a wait queue
		 * after wait, grab the dev lock again
		 */
		while (1) {
			mutex_lock(&sdev->lock);
			prepare_to_wait(&sdev->readers_wq, &read_wait,
					TASK_INTERRUPTIBLE);
			ret = (sdev->num_writers > 0 ||
					sdev->num_read_avail_blks <= 0);
			if (!ret) {
				/*don't have to wait*/
				break;
			}
			mutex_unlock(&sdev->lock);
			if (signal_pending(current)) {
				ret = -EINTR;
				break;
			}
			schedule();
		}

		finish_wait(&sdev->readers_wq, &read_wait);
		if (ret) {
			mutex_unlock(&slogk_dev->lock);
			return -EINTR;
		}
	}

	/*sdev->lock is held at this point*/
	sdev->num_read_in_prog_blks = sdev->num_read_avail_blks;
	sdev->num_read_avail_blks = 0;
	rrange.start_idx = sdev->read_idx;
	rrange.num = sdev->num_read_in_prog_blks;
	mutex_unlock(&sdev->lock);

	if (copy_to_user((unsigned int *)arg, &rrange,
			sizeof(struct read_range)))
		return -EFAULT;

	return 0;
}

static long qmp_sphinx_logk_set_mask(unsigned long arg)
{
	__u8 buffer[256];
	int i;
	unsigned int num_elements;

	if (copy_from_user(&num_elements,
		(unsigned int __user *) arg, sizeof(unsigned int)))
		return -EFAULT;

	if (0 == num_sources)
		return -EINVAL;

	if (num_elements == 0 ||
		MASK_BUFFER_SIZE < DIV_ROUND_UP(num_sources, 8))
		return -EINVAL;

	if (copy_from_user(buffer,
			(__u8 *)arg, DIV_ROUND_UP(num_sources, 8)))
			return -EFAULT;

	write_lock(&filter_lock);
	if (num_elements != num_sources) {
		write_unlock(&filter_lock);
		return -EPERM;
	}

	for (i = 0; i < num_sources; i++) {
		pmask[i].isOn =
				qmp_sphinx_logk_get_bit_from_vector(
					(__u8 *)buffer, i);
	}
	write_unlock(&filter_lock);
	return 0;
}

static long qmp_sphinx_logk_set_mapping(unsigned long arg)
{
	__u32 num_elements;
	__u32 *pbuffer;
	int i;
	struct qmp_sphinx_source_mask *pnewmask;
	if (copy_from_user(&num_elements,
					(__u32 __user *)arg, sizeof(__u32)))
		return -EFAULT;

	if (0 == num_elements)
		return -EFAULT;

	if (NULL != pmask) {
		/*
		 * Mask is getting set again.
		 * qmp_sphinx_core was probably restarted.
		 */
		struct qmp_sphinx_source_mask *ptempmask;
		write_lock(&filter_lock);
		num_sources = 0;
		ptempmask = pmask;
		pmask = NULL;
		write_unlock(&filter_lock);
		kfree(ptempmask);
	}
	pbuffer = kmalloc(sizeof(struct qmp_sphinx_source_mask)
				* num_elements, GFP_KERNEL);
	if (NULL == pbuffer)
		return -ENOMEM;

	/*
	 * Use our new table as scratch space for now.
	 * We copy an ordered list of hash values into our buffer.
	 */
	if (copy_from_user(pbuffer, &((__u32 __user *)arg)[1],
					num_elements*sizeof(unsigned int))) {
		kfree(pbuffer);
		return -EFAULT;
	}
	/*
	 * We arrange the user data into a more usable form.
	 * This is done in-place.
	 */
	pnewmask = (struct qmp_sphinx_source_mask *) pbuffer;
	for (i = num_elements - 1; i >= 0; i--) {
		pnewmask[i].hash = pbuffer[i];
		/* Observer is off by default*/
		pnewmask[i].isOn = 0;
	}
	write_lock(&filter_lock);
	pmask = pnewmask;
	num_sources = num_elements;
	write_unlock(&filter_lock);
	return 0;
}

static long qmp_sphinx_logk_check_filter(unsigned long arg)
{
	int i;
	unsigned int hash = (unsigned int) arg;

	/*
	 * This lock may be a bit long.
	 * If it is a problem, it can be fixed.
	 */
	read_lock(&filter_lock);
	for (i = 0; i < num_sources; i++) {
		if (hash == pmask[i].hash) {
			int result = pmask[i].isOn;
			read_unlock(&filter_lock);
			return result;
		}
	}
	read_unlock(&filter_lock);
	return 0;
}

static int qmp_sphinx_logk_mmap(struct file *filp,
		struct vm_area_struct *vma)
{
	int ret;
	char *vptr;
	unsigned long length, pfn;
	unsigned long start = vma->vm_start;

	length = vma->vm_end - vma->vm_start;

	if (length > (unsigned long) slogk_dev->ring_sz) {
		pr_err("len check failed\n");
		return -EIO;
	}

	vma->vm_flags |= VM_RESERVED | VM_SHARED;
	vptr = (char *) slogk_dev->ring;
	ret = 0;

	if (kmalloc_flag) {
		ret = remap_pfn_range(vma,
				start,
				virt_to_phys((void *)
				((unsigned long)slogk_dev->ring)) >> PAGE_SHIFT,
				length,
				vma->vm_page_prot);
		if (ret != 0) {
			pr_err("remap_pfn_range() fails with ret = %d\n",
				ret);
			return -EAGAIN;
		}
	} else {
		while (length > 0) {
			pfn = vmalloc_to_pfn(vptr);

			ret = remap_pfn_range(vma, start, pfn, PAGE_SIZE,
					vma->vm_page_prot);
			if (ret < 0) {
				pr_err("remap_pfn_range() fails with ret = %d\n",
					ret);
				return ret;
			}
			start += PAGE_SIZE;
			vptr += PAGE_SIZE;
			length -= PAGE_SIZE;
		}
	}

	return 0;
}

static const struct file_operations qmp_sphinx_logk_fops = {
	.write = qmp_sphinx_logk_write,
	.open = qmp_sphinx_logk_open,
	.unlocked_ioctl = qmp_sphinx_logk_ioctl,
	.compat_ioctl = qmp_sphinx_logk_ioctl,
	.mmap = qmp_sphinx_logk_mmap,
};

__init int qmp_sphinx_logk_init(void)
{
	int ret;
	int devno = 0;

	num_sources = 0;
	kmalloc_flag = 0;
	block_apps = 0;
	pmask = NULL;

	if (kmalloc_flag && ring_sz > FOUR_MB) {
		pr_err("kmalloc cannot allocate > 4MB\n");
		return -ENOMEM;
	}

	ring_sz = ring_sz * SZ_1M;
	if (ring_sz <= 0) {
		pr_err("Too small a ring_sz=%d\n", ring_sz);
		return -EINVAL;
	}

	slogk_dev = kmalloc(sizeof(*slogk_dev), GFP_KERNEL);
	if (!slogk_dev) {
		pr_err("kmalloc failure!\n");
		return -ENOMEM;
	}

	slogk_dev->ring_sz = ring_sz;
	slogk_dev->blk_sz = sizeof(struct qmp_sphinx_logk_blk);
	/*intialize ping-pong buffers*/
	ret = ringbuf_init(slogk_dev);
	if (ret < 0) {
		pr_err("Init Failed, ret = %d\n", ret);
		goto pingpong_fail;
	}

	ret = alloc_chrdev_region(&devno, 0, qmp_sphinx_LOGK_NUM_DEVS,
			qmp_sphinx_LOGK_DEV_NAME);
	if (ret < 0) {
		pr_err("alloc_chrdev_region failed with ret = %d\n",
				ret);
		goto register_fail;
	}

	slogk_dev->major = MAJOR(devno);

	pr_debug("logk: major# = %d\n", slogk_dev->major);

	cl = class_create(THIS_MODULE, qmp_sphinx_LOGK_DEV_NAME);
	if (cl == NULL) {
		pr_err("class create failed");
		goto cdev_fail;
	}
	if (device_create(cl, NULL, devno, NULL,
			qmp_sphinx_LOGK_DEV_NAME) == NULL) {
		pr_err("device create failed");
		goto class_destroy_fail;
	}
	cdev_init(&(slogk_dev->cdev), &qmp_sphinx_logk_fops);

	slogk_dev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&(slogk_dev->cdev), MKDEV(slogk_dev->major, 0), 1);
	if (ret) {
		pr_err("cdev_add failed with ret = %d", ret);
		goto class_destroy_fail;
	}

	qmp_sphinx_logk_attach();
	mutex_init(&slogk_dev->lock);
	init_waitqueue_head(&slogk_dev->readers_wq);
	init_waitqueue_head(&slogk_dev->writers_wq);
	rwlock_init(&filter_lock);
	return 0;
class_destroy_fail:
	class_destroy(cl);
cdev_fail:
	unregister_chrdev_region(devno, qmp_sphinx_LOGK_NUM_DEVS);
register_fail:
	ringbuf_cleanup(slogk_dev);
pingpong_fail:
	kfree(slogk_dev);
	return -EPERM;
}

__exit void qmp_sphinx_logk_cleanup(void)
{
	dev_t devno = MKDEV(slogk_dev->major, slogk_dev->minor);
	qmp_sphinx_logk_detach();

	cdev_del(&slogk_dev->cdev);

	unregister_chrdev_region(devno, qmp_sphinx_LOGK_NUM_DEVS);
	ringbuf_cleanup(slogk_dev);
	kfree(slogk_dev);

	if (NULL != pmask) {
		kfree(pmask);
		pmask = NULL;
	}
}

module_init(qmp_sphinx_logk_init);
module_exit(qmp_sphinx_logk_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("qmp_sphinx Observer");

