/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "sprd-gnss-dump: " fmt

#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/vmalloc.h>

#define GNSS_RING_R			0
#define GNSS_RING_W			1
#define GNSS_RX_RING_SIZE		(1024*1024)
#define GNSS_SUCCESS			0
#define GNSS_ERR_RING_FULL		1
#define GNSS_ERR_MALLOC_FAIL		2
#define GNSS_ERR_BAD_PARAM		3
#define GNSS_ERR_SDIO_ERR		4
#define GNSS_ERR_TIMEOUT		5
#define GNSS_ERR_NO_FILE		6
#define FALSE				false
#define TRUE				true

#define GNSS_RING_REMAIN(rp, wp, size) \
	((u_long)(wp) >= (u_long)(rp) ? \
	((size)-(u_long)(wp)+(u_long)(rp)) : \
	((u_long)(rp)-(u_long)(wp)))

struct gnss_ring_t {
	unsigned long size;
	char *pbuff;
	char *rp;
	char *wp;
	char *end;
	bool reset_rp;
	struct mutex *plock;
	int (*memcpy_rd)(char *dest, char *src, size_t cnt);
	int (*memcpy_wr)(char *dest, char *src, size_t cnt);
};

struct gnss_device {
	struct gnss_ring_t	*ring_dev;
	struct miscdevice	wcn_gnss_dump_device;
	wait_queue_head_t	rxwait;
};

static struct gnss_ring_t *gnss_rx_ring;
static struct gnss_device *gnss_dev;

static unsigned long gnss_ring_remain(struct gnss_ring_t *pring)
{
	return (unsigned long)GNSS_RING_REMAIN(pring->rp,
						   pring->wp, pring->size);
}
unsigned long gnss_ring_free_space(void)
{
	return gnss_ring_remain(gnss_dev->ring_dev);
}

static unsigned long gnss_ring_content_len(struct gnss_ring_t *pring)
{
	return pring->size - gnss_ring_remain(pring);
}

static char *gnss_ring_start(struct gnss_ring_t *pring)
{
	return pring->pbuff;
}

static char *gnss_ring_end(struct gnss_ring_t *pring)
{
	return pring->end;
}

void gnss_ring_reset(void)
{
	gnss_dev->ring_dev->wp = gnss_dev->ring_dev->pbuff;
	gnss_dev->ring_dev->rp = gnss_dev->ring_dev->wp;
}

static bool gnss_ring_over_loop(struct gnss_ring_t *pring,
				u_long len,
				int rw)
{
	if (rw == GNSS_RING_R)
		return (u_long)pring->rp + len > (u_long)gnss_ring_end(pring);
	else
		return (u_long)pring->wp + len > (u_long)gnss_ring_end(pring);
}

static void gnss_ring_destroy(struct gnss_ring_t *pring)
{
	if (pring) {
		if (pring->pbuff) {
			pr_debug("%s free pbuff\n", __func__);
			vfree(pring->pbuff);
			pring->pbuff = NULL;
		}

		if (pring->plock) {
			pr_debug("%s free plock\n", __func__);
			mutex_destroy(pring->plock);
			vfree(pring->plock);
			pring->plock = NULL;
		}
		pr_debug("%s free pring\n", __func__);
		vfree(pring);
		pring = NULL;
	}
}

static struct gnss_ring_t *gnss_ring_init(unsigned long size,
					  int (*rd)(char*, char*, size_t),
					  int (*wr)(char*, char*, size_t))
{
	struct gnss_ring_t *pring = NULL;

	if (!rd || !wr) {
		pr_err("Ring must assign callback\n");
		return NULL;
	}

	do {
		pring = vmalloc(sizeof(struct gnss_ring_t));
		if (!pring) {
			pr_err("Ring malloc Failed\n");
			break;
		}
		pring->pbuff = vmalloc(size);
		if (!pring->pbuff) {
			pr_err("Ring buff malloc Failed\n");
			break;
		}
		pring->plock = vmalloc(sizeof(struct mutex));
		if (!pring->plock) {
			pr_err("Ring lock malloc Failed\n");
			break;
		}
		mutex_init(pring->plock);
		memset(pring->pbuff, 0, size);
		pring->size = size;
		pring->rp = pring->pbuff;
		pring->wp = pring->pbuff;
		pring->end = (char *)((u_long)pring->pbuff + (pring->size - 1));
		pring->reset_rp = false;
		pring->memcpy_rd = rd;
		pring->memcpy_wr = wr;
		return pring;
	} while (0);
	gnss_ring_destroy(pring);

	return NULL;
}

static int gnss_ring_read(struct gnss_ring_t *pring, char *buf, int len)
{
	int len1, len2 = 0;
	int cont_len = 0;
	int read_len = 0;
	char *pstart = NULL;
	char *pend = NULL;

	if (!buf || !pring || !len) {
		pr_err("%s: Param Error!,buf=%p,pring=%p,len=%d\n",
		       __func__, buf, pring, len);
		return -GNSS_ERR_BAD_PARAM;
	}
	mutex_lock(pring->plock);
	pr_debug("reset_rp[%d] rp[%p] wp[%p]\n",
		 pring->reset_rp, pring->rp, pring->wp);
	if (pring->reset_rp == true) {
		pring->rp = pring->wp;
		pring->reset_rp = false;
	}
	cont_len = gnss_ring_content_len(pring);
	read_len = cont_len >= len ? len : cont_len;
	pstart = gnss_ring_start(pring);
	pend = gnss_ring_end(pring);
	pr_debug("len=%d, buf=%p, start=%p, end=%p, rp=%p\n",
		 read_len, buf, pstart, pend, pring->rp);

	if ((read_len == 0) || (cont_len == 0)) {
		pr_debug("read_len=0 or Ring empty\n");
		mutex_unlock(pring->plock);
		return 0;
	}

	if (gnss_ring_over_loop(pring, read_len, GNSS_RING_R)) {
		pr_debug("Ring loopover\n");
		len1 = pend - pring->rp + 1;
		len2 = read_len - len1;
		pring->memcpy_rd(buf, pring->rp, len1);
		pring->memcpy_rd((buf + len1), pstart, len2);
		pring->rp = (char *)((u_long)pstart + len2);
	} else {
		pring->memcpy_rd(buf, pring->rp, read_len);
		pring->rp += read_len;
	}
	pr_debug("Ring did read len[%d]\n", read_len);
	mutex_unlock(pring->plock);

	return read_len;
}

static int gnss_ring_write(struct gnss_ring_t *pring, char *buf, int len)
{
	int len1, len2 = 0;
	char *pstart = NULL;
	char *pend = NULL;
	bool check_rp = false;

	if (!pring || !buf || !len) {
		pr_err("%s: Param Error!,buf=%p,pring=%p,len=%d\n",
		       __func__, buf, pring, len);
		return -GNSS_ERR_BAD_PARAM;
	}
	pstart = gnss_ring_start(pring);
	pend = gnss_ring_end(pring);
	pr_debug("start=%p, end=%p, buf=%p, len=%d, wp=%p, rst_rp[%d]\n",
		 pstart, pend, buf, len, pring->wp, pring->reset_rp);

	if (gnss_ring_over_loop(pring, len, GNSS_RING_W)) {
		pr_debug("Ring overloop\n");
		len1 = pend - pring->wp + 1;
		len2 = len - len1;
		pring->memcpy_wr(pring->wp, buf, len1);
		pring->memcpy_wr(pstart, (buf + len1), len2);
		if (pring->wp < pring->rp)
			pring->reset_rp = true;
		else
			check_rp = true;
		pring->wp = (char *)((u_long)pstart + len2);
	} else {
		pring->memcpy_wr(pring->wp, buf, len);
		if (pring->wp < pring->rp)
			check_rp = true;
		pring->wp += len;
	}
	if (check_rp && pring->wp > pring->rp)
		pring->reset_rp = true;
	pr_debug("Ring Wrote len[%d]\n", len);

	return len;
}

int gnss_dump_write(char *buf, int len)
{
	ssize_t count = 0;

	count = gnss_ring_write(gnss_dev->ring_dev, (char *)buf, len);
	if (count > 0)
		wake_up_interruptible(&gnss_dev->rxwait);

	return count;
}

static int gnss_memcpy_rd(char *dest, char *src, size_t count)
{
	return copy_to_user(dest, src, count);
}

static int gnss_memcpy_wr(char *dest, char *src, size_t count)
{
	memcpy(dest, src, count);
	return 0;
}

static int gnss_device_init(void)
{
	gnss_dev = kzalloc(sizeof(*gnss_dev), GFP_KERNEL);
	if (!gnss_dev) {
		pr_err("alloc gnss device error\n");
		return -ENOMEM;
	}
	gnss_dev->ring_dev = gnss_rx_ring;
	init_waitqueue_head(&gnss_dev->rxwait);
	return 0;
}

static int gnss_device_destroy(void)
{
	kfree(gnss_dev);
	gnss_dev = NULL;

	return 0;
}

static int wcn_gnss_dump_open(struct inode *inode, struct file *filp)
{
	pr_debug("%s entry\n", __func__);

	return 0;
}

static int wcn_gnss_dump_release(struct inode *inode, struct file *filp)
{
	pr_debug("%s entry\n", __func__);

	return 0;
}

static ssize_t wcn_gnss_dump_read(struct file *filp,
				char __user *buf,
				size_t count,
				loff_t *pos)
{
	ssize_t len = 0;

	len = gnss_ring_read(gnss_rx_ring, (char *)buf, count);

	return len;
}

static const struct file_operations wcn_gnss_dump_fops = {
	.owner = THIS_MODULE,
	.read = wcn_gnss_dump_read,
	//.write = wcn_gnss_dump_write,
	.open = wcn_gnss_dump_open,
	.release = wcn_gnss_dump_release,
};

static struct miscdevice wcn_gnss_dump_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "wcn_gnss_dump",
	.fops = &wcn_gnss_dump_fops,
};

int wcn_gnss_dump_init(void)
{
	int ret;

	gnss_rx_ring = gnss_ring_init(GNSS_RX_RING_SIZE,
				      gnss_memcpy_rd, gnss_memcpy_wr);
	if (!gnss_rx_ring) {
		pr_err("Ring malloc error\n");
		return -GNSS_ERR_MALLOC_FAIL;
	}

	do {
		ret = gnss_device_init();
		if (ret != 0) {
			gnss_ring_destroy(gnss_rx_ring);
			break;
		}

		ret = misc_register(&wcn_gnss_dump_device);
		if (ret != 0) {
			gnss_ring_destroy(gnss_rx_ring);
			gnss_device_destroy();
			break;
		}
		gnss_dev->wcn_gnss_dump_device = wcn_gnss_dump_device;
	} while (0);

	if (ret != 0)
		pr_err("misc register error\n");

	return ret;
}

void wcn_gnss_dump_exit(void)
{
	gnss_ring_destroy(gnss_rx_ring);
	gnss_device_destroy();
	misc_deregister(&wcn_gnss_dump_device);
}
