/* Copyright (c) 2010-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* Qualcomm Over the Air (OTA) Crypto driver */

#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/dmapool.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/cache.h>


#include <linux/qcota.h>
#include "qce.h"
#include "qce_ota.h"

enum qce_ota_oper_enum {
	QCE_OTA_F8_OPER   = 0,
	QCE_OTA_MPKT_F8_OPER = 1,
	QCE_OTA_F9_OPER  = 2,
	QCE_OTA_VAR_MPKT_F8_OPER = 3,
	QCE_OTA_OPER_LAST
};

struct ota_dev_control;

struct ota_async_req {
	struct list_head rlist;
	struct completion complete;
	int err;
	enum qce_ota_oper_enum op;
	union {
		struct qce_f9_req f9_req;
		struct qce_f8_req f8_req;
		struct qce_f8_multi_pkt_req f8_mp_req;
		struct qce_f8_varible_multi_pkt_req f8_v_mp_req;
	} req;
	unsigned int steps;
	struct ota_qce_dev  *pqce;
};

/*
 * Register ourselves as a misc device to be able to access the ota
 * from userspace.
 */


#define QCOTA_DEV	"qcota"


struct ota_dev_control {

	/* misc device */
	struct miscdevice miscdevice;
	struct list_head ready_commands;
	unsigned magic;
	struct list_head qce_dev;
	spinlock_t lock;
	struct mutex register_lock;
	bool registered;
	uint32_t total_units;
};

struct ota_qce_dev {
	struct list_head qlist;
	/* qce handle */
	void *qce;

	/* platform device */
	struct platform_device *pdev;

	struct ota_async_req *active_command;
	struct tasklet_struct done_tasklet;
	struct ota_dev_control *podev;
	uint32_t unit;
	u64 total_req;
	u64 err_req;
};

#define OTA_MAGIC 0x4f544143

static long qcota_ioctl(struct file *file,
			  unsigned cmd, unsigned long arg);
static int qcota_open(struct inode *inode, struct file *file);
static int qcota_release(struct inode *inode, struct file *file);
static int start_req(struct ota_qce_dev *pqce, struct ota_async_req *areq);
static void f8_cb(void *cookie, unsigned char *icv, unsigned char *iv, int ret);

static const struct file_operations qcota_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = qcota_ioctl,
	.open = qcota_open,
	.release = qcota_release,
};

static struct ota_dev_control qcota_dev = {
	.miscdevice = {
			.minor = MISC_DYNAMIC_MINOR,
			.name = "qcota0",
			.fops = &qcota_fops,
	},
	.magic = OTA_MAGIC,
};

#define DEBUG_MAX_FNAME  16
#define DEBUG_MAX_RW_BUF 1024

struct qcota_stat {
	u64 f8_req;
	u64 f8_mp_req;
	u64 f8_v_mp_req;
	u64 f9_req;
	u64 f8_op_success;
	u64 f8_op_fail;
	u64 f8_mp_op_success;
	u64 f8_mp_op_fail;
	u64 f8_v_mp_op_success;
	u64 f8_v_mp_op_fail;
	u64 f9_op_success;
	u64 f9_op_fail;
};
static struct qcota_stat _qcota_stat;
static struct dentry *_debug_dent;
static char _debug_read_buf[DEBUG_MAX_RW_BUF];
static int _debug_qcota;

static struct ota_dev_control *qcota_control(void)
{

	return &qcota_dev;
}

static int qcota_open(struct inode *inode, struct file *file)
{
	struct ota_dev_control *podev;

	podev = qcota_control();
	if (podev == NULL) {
		pr_err("%s: no such device %d\n", __func__,
				MINOR(inode->i_rdev));
		return -ENOENT;
	}

	file->private_data = podev;

	return 0;
}

static int qcota_release(struct inode *inode, struct file *file)
{
	struct ota_dev_control *podev;

	podev =  file->private_data;

	if (podev != NULL && podev->magic != OTA_MAGIC) {
		pr_err("%s: invalid handle %p\n",
			__func__, podev);
	}

	file->private_data = NULL;

	return 0;
}

static bool  _next_v_mp_req(struct ota_async_req *areq)
{
	unsigned char *p;

	if (areq->err)
		return false;
	if (++areq->steps >= areq->req.f8_v_mp_req.num_pkt)
		return false;

	p = areq->req.f8_v_mp_req.qce_f8_req.data_in;
	p += areq->req.f8_v_mp_req.qce_f8_req.data_len;
	p = (uint8_t *) ALIGN(((uintptr_t)p), L1_CACHE_BYTES);

	areq->req.f8_v_mp_req.qce_f8_req.data_out = p;
	areq->req.f8_v_mp_req.qce_f8_req.data_in = p;
	areq->req.f8_v_mp_req.qce_f8_req.data_len =
		areq->req.f8_v_mp_req.cipher_iov[areq->steps].size;

	areq->req.f8_v_mp_req.qce_f8_req.count_c++;
	return true;
}

static void req_done(unsigned long data)
{
	struct ota_qce_dev *pqce = (struct ota_qce_dev *)data;
	struct ota_dev_control *podev = pqce->podev;
	struct ota_async_req *areq;
	unsigned long flags;
	struct ota_async_req *new_req = NULL;
	int ret = 0;
	bool schedule = true;

	spin_lock_irqsave(&podev->lock, flags);
	areq = pqce->active_command;
	if (unlikely(areq == NULL))
		pr_err("ota_crypto: req_done, no active request\n");
	else if (areq->op == QCE_OTA_VAR_MPKT_F8_OPER) {
		if (_next_v_mp_req(areq)) {
			/* execute next subcommand */
			spin_unlock_irqrestore(&podev->lock, flags);
			ret = start_req(pqce, areq);
			if (unlikely(ret)) {
				areq->err = ret;
				schedule = true;
				spin_lock_irqsave(&podev->lock, flags);
			} else {
				areq = NULL;
				schedule = false;
			}
		} else {
			/* done with this variable mp req */
			schedule = true;
		}
	}
	while (schedule) {
		if (!list_empty(&podev->ready_commands)) {
			new_req = container_of(podev->ready_commands.next,
						struct ota_async_req, rlist);
			if (NULL == new_req) {
				pr_err("ota_crypto: req_done, new_req = NULL");
				return;
			}
			list_del(&new_req->rlist);
			pqce->active_command = new_req;
			spin_unlock_irqrestore(&podev->lock, flags);

			new_req->err = 0;
			/* start a new request */
			ret = start_req(pqce, new_req);
			if (unlikely(new_req && ret)) {
				new_req->err = ret;
				complete(&new_req->complete);
				ret = 0;
				new_req = NULL;
				spin_lock_irqsave(&podev->lock, flags);
			} else {
				schedule = false;
			}
		} else {
			pqce->active_command = NULL;
			spin_unlock_irqrestore(&podev->lock, flags);
			schedule = false;
		};
	}
	if (areq)
		complete(&areq->complete);
	return;
}

static void f9_cb(void *cookie, unsigned char *icv, unsigned char *iv,
	int ret)
{
	struct ota_async_req *areq = (struct ota_async_req *) cookie;
	struct ota_qce_dev *pqce;

	pqce = areq->pqce;
	areq->req.f9_req.mac_i  = *((uint32_t *)icv);

	if (ret) {
		pqce->err_req++;
		areq->err = -ENXIO;
	} else
		areq->err = 0;

	tasklet_schedule(&pqce->done_tasklet);
}

static void f8_cb(void *cookie, unsigned char *icv, unsigned char *iv,
	int ret)
{
	struct ota_async_req *areq = (struct ota_async_req *) cookie;
	struct ota_qce_dev *pqce;

	pqce = areq->pqce;

	if (ret) {
		pqce->err_req++;
		areq->err = -ENXIO;
	} else {
		areq->err = 0;
	}

	tasklet_schedule(&pqce->done_tasklet);
}

static int start_req(struct ota_qce_dev *pqce, struct ota_async_req *areq)
{
	struct qce_f9_req *pf9;
	struct qce_f8_multi_pkt_req *p_mp_f8;
	struct qce_f8_req *pf8;
	int ret = 0;

	/* command should be on the podev->active_command */
	areq->pqce = pqce;

	switch (areq->op) {
	case QCE_OTA_F8_OPER:
		pf8 = &areq->req.f8_req;
		ret = qce_f8_req(pqce->qce, pf8, areq, f8_cb);
		break;
	case QCE_OTA_MPKT_F8_OPER:
		p_mp_f8 = &areq->req.f8_mp_req;
		ret = qce_f8_multi_pkt_req(pqce->qce, p_mp_f8, areq, f8_cb);
		break;

	case QCE_OTA_F9_OPER:
		pf9 = &areq->req.f9_req;
		ret =  qce_f9_req(pqce->qce, pf9, areq, f9_cb);
		break;

	case QCE_OTA_VAR_MPKT_F8_OPER:
		pf8 = &areq->req.f8_v_mp_req.qce_f8_req;
		ret = qce_f8_req(pqce->qce, pf8, areq, f8_cb);
		break;

	default:
		ret = -ENOTSUPP;
		break;
	};
	areq->err = ret;
	pqce->total_req++;
	if (ret)
		pqce->err_req++;
	return ret;
}

static struct ota_qce_dev *schedule_qce(struct ota_dev_control *podev)
{
	/* do this function with spinlock set */
	struct ota_qce_dev *p;

	if (unlikely(list_empty(&podev->qce_dev))) {
		pr_err("%s: no valid qce to schedule\n", __func__);
		return NULL;
	}

	list_for_each_entry(p, &podev->qce_dev, qlist) {
		if (p->active_command == NULL)
			return p;
	}
	return NULL;
}

static int submit_req(struct ota_async_req *areq, struct ota_dev_control *podev)
{
	unsigned long flags;
	int ret = 0;
	struct qcota_stat *pstat;
	struct ota_qce_dev *pqce;

	areq->err = 0;

	spin_lock_irqsave(&podev->lock, flags);
	pqce = schedule_qce(podev);
	if (pqce) {
		pqce->active_command = areq;
		spin_unlock_irqrestore(&podev->lock, flags);

		ret = start_req(pqce, areq);
		if (ret != 0) {
			spin_lock_irqsave(&podev->lock, flags);
			pqce->active_command = NULL;
			spin_unlock_irqrestore(&podev->lock, flags);
		}

	} else {
		list_add_tail(&areq->rlist, &podev->ready_commands);
		spin_unlock_irqrestore(&podev->lock, flags);
	}

	if (ret == 0)
		wait_for_completion(&areq->complete);

	pstat = &_qcota_stat;
	switch (areq->op) {
	case QCE_OTA_F8_OPER:
		if (areq->err)
			pstat->f8_op_fail++;
		else
			pstat->f8_op_success++;
		break;

	case QCE_OTA_MPKT_F8_OPER:

		if (areq->err)
			pstat->f8_mp_op_fail++;
		else
			pstat->f8_mp_op_success++;
		break;

	case QCE_OTA_F9_OPER:
		if (areq->err)
			pstat->f9_op_fail++;
		else
			pstat->f9_op_success++;
		break;
	case QCE_OTA_VAR_MPKT_F8_OPER:
	default:
		if (areq->err)
			pstat->f8_v_mp_op_fail++;
		else
			pstat->f8_v_mp_op_success++;
		break;
	};

	return areq->err;
}

static long qcota_ioctl(struct file *file,
			  unsigned cmd, unsigned long arg)
{
	int err = 0;
	struct ota_dev_control *podev;
	uint8_t *user_src;
	uint8_t *user_dst;
	uint8_t *k_buf = NULL;
	struct ota_async_req areq;
	uint32_t total, temp;
	struct qcota_stat *pstat;
	int i;
	uint8_t *p = NULL;

	podev =  file->private_data;
	if (podev == NULL || podev->magic != OTA_MAGIC) {
		pr_err("%s: invalid handle %p\n",
			__func__, podev);
		return -ENOENT;
	}

	/* Verify user arguments. */
	if (_IOC_TYPE(cmd) != QCOTA_IOC_MAGIC)
		return -ENOTTY;

	init_completion(&areq.complete);

	pstat = &_qcota_stat;

	switch (cmd) {
	case QCOTA_F9_REQ:
		if (!access_ok(VERIFY_WRITE, (void __user *)arg,
			       sizeof(struct qce_f9_req)))
			return -EFAULT;
		if (__copy_from_user(&areq.req.f9_req, (void __user *)arg,
				     sizeof(struct qce_f9_req)))
			return -EFAULT;

		user_src = areq.req.f9_req.message;
		if (!access_ok(VERIFY_READ, (void __user *)user_src,
			       areq.req.f9_req.msize))
			return -EFAULT;

		if (areq.req.f9_req.msize == 0)
			return 0;
		k_buf = kmalloc(areq.req.f9_req.msize, GFP_KERNEL);
		if (k_buf == NULL)
			return -ENOMEM;

		if (__copy_from_user(k_buf, (void __user *)user_src,
				areq.req.f9_req.msize)) {
			kfree(k_buf);
			return -EFAULT;
		}

		areq.req.f9_req.message = k_buf;
		areq.op = QCE_OTA_F9_OPER;

		pstat->f9_req++;
		err = submit_req(&areq, podev);

		areq.req.f9_req.message = user_src;
		if (err == 0 && __copy_to_user((void __user *)arg,
				&areq.req.f9_req, sizeof(struct qce_f9_req))) {
			err = -EFAULT;
		}
		kfree(k_buf);
		break;

	case QCOTA_F8_REQ:
		if (!access_ok(VERIFY_WRITE, (void __user *)arg,
			       sizeof(struct qce_f8_req)))
			return -EFAULT;
		if (__copy_from_user(&areq.req.f8_req, (void __user *)arg,
				     sizeof(struct qce_f8_req)))
			return -EFAULT;
		total = areq.req.f8_req.data_len;
		user_src = areq.req.f8_req.data_in;
		if (user_src != NULL) {
			if (!access_ok(VERIFY_READ, (void __user *)
					user_src, total))
				return -EFAULT;

		};

		user_dst = areq.req.f8_req.data_out;
		if (!access_ok(VERIFY_WRITE, (void __user *)
				user_dst, total))
			return -EFAULT;

		if (!total)
			return 0;
		k_buf = kmalloc(total, GFP_KERNEL);
		if (k_buf == NULL)
			return -ENOMEM;

		/* k_buf returned from kmalloc should be cache line aligned */
		if (user_src && __copy_from_user(k_buf,
				(void __user *)user_src, total)) {
			kfree(k_buf);
			return -EFAULT;
		}

		if (user_src)
			areq.req.f8_req.data_in = k_buf;
		else
			areq.req.f8_req.data_in = NULL;
		areq.req.f8_req.data_out = k_buf;

		areq.op = QCE_OTA_F8_OPER;

		pstat->f8_req++;
		err = submit_req(&areq, podev);

		if (err == 0 && __copy_to_user(user_dst, k_buf, total))
			err = -EFAULT;
		kfree(k_buf);

		break;

	case QCOTA_F8_MPKT_REQ:
		if (!access_ok(VERIFY_WRITE, (void __user *)arg,
			       sizeof(struct qce_f8_multi_pkt_req)))
			return -EFAULT;
		if (__copy_from_user(&areq.req.f8_mp_req, (void __user *)arg,
				     sizeof(struct qce_f8_multi_pkt_req)))
			return -EFAULT;
		temp = areq.req.f8_mp_req.qce_f8_req.data_len;
		if (temp < (uint32_t) areq.req.f8_mp_req.cipher_start +
				 areq.req.f8_mp_req.cipher_size)
			return -EINVAL;
		total = (uint32_t) areq.req.f8_mp_req.num_pkt *
				areq.req.f8_mp_req.qce_f8_req.data_len;

		user_src = areq.req.f8_mp_req.qce_f8_req.data_in;
		if (!access_ok(VERIFY_READ, (void __user *)
				user_src, total))
			return -EFAULT;

		user_dst = areq.req.f8_mp_req.qce_f8_req.data_out;
		if (!access_ok(VERIFY_WRITE, (void __user *)
				user_dst, total))
			return -EFAULT;

		if (!total)
			return 0;
		k_buf = kmalloc(total, GFP_KERNEL);
		if (k_buf == NULL)
			return -ENOMEM;
		/* k_buf returned from kmalloc should be cache line aligned */
		if (__copy_from_user(k_buf, (void __user *)user_src, total)) {
			kfree(k_buf);

			return -EFAULT;
		}

		areq.req.f8_mp_req.qce_f8_req.data_out = k_buf;
		areq.req.f8_mp_req.qce_f8_req.data_in = k_buf;

		areq.op = QCE_OTA_MPKT_F8_OPER;

		pstat->f8_mp_req++;
		err = submit_req(&areq, podev);

		if (err == 0 && __copy_to_user(user_dst, k_buf, total))
			err = -EFAULT;
		kfree(k_buf);
		break;

	case QCOTA_F8_V_MPKT_REQ:
		if (!access_ok(VERIFY_WRITE, (void __user *)arg,
				sizeof(struct qce_f8_varible_multi_pkt_req)))
			return -EFAULT;
		if (__copy_from_user(&areq.req.f8_v_mp_req, (void __user *)arg,
				sizeof(struct qce_f8_varible_multi_pkt_req)))
			return -EFAULT;

		if (areq.req.f8_v_mp_req.num_pkt > MAX_NUM_V_MULTI_PKT)
			return -EINVAL;

		for (i = 0, total = 0; i < areq.req.f8_v_mp_req.num_pkt; i++) {
			if (!access_ok(VERIFY_WRITE, (void __user *)
				areq.req.f8_v_mp_req.cipher_iov[i].addr,
				areq.req.f8_v_mp_req.cipher_iov[i].size))
				return -EFAULT;
			total += areq.req.f8_v_mp_req.cipher_iov[i].size;
			total = ALIGN(total, L1_CACHE_BYTES);
		}

		if (!total)
			return 0;
		k_buf = kmalloc(total, GFP_KERNEL);
		if (k_buf == NULL)
			return -ENOMEM;

		for (i = 0, p = k_buf; i < areq.req.f8_v_mp_req.num_pkt; i++) {
			user_src =  areq.req.f8_v_mp_req.cipher_iov[i].addr;
			if (__copy_from_user(p, (void __user *)user_src,
				areq.req.f8_v_mp_req.cipher_iov[i].size)) {
				kfree(k_buf);
				return -EFAULT;
			}
			p += areq.req.f8_v_mp_req.cipher_iov[i].size;
			p = (uint8_t *) ALIGN(((uintptr_t)p),
							L1_CACHE_BYTES);
		}

		areq.req.f8_v_mp_req.qce_f8_req.data_out = k_buf;
		areq.req.f8_v_mp_req.qce_f8_req.data_in = k_buf;
		areq.req.f8_v_mp_req.qce_f8_req.data_len =
			areq.req.f8_v_mp_req.cipher_iov[0].size;
		areq.steps = 0;
		areq.op = QCE_OTA_VAR_MPKT_F8_OPER;

		pstat->f8_v_mp_req++;
		err = submit_req(&areq, podev);

		if (err != 0) {
			kfree(k_buf);
			return err;
		}

		for (i = 0, p = k_buf; i < areq.req.f8_v_mp_req.num_pkt; i++) {
			user_dst =  areq.req.f8_v_mp_req.cipher_iov[i].addr;
			if (__copy_to_user(user_dst, p,
				areq.req.f8_v_mp_req.cipher_iov[i].size)) {
				kfree(k_buf);
				return -EFAULT;
			}
			p += areq.req.f8_v_mp_req.cipher_iov[i].size;
			p = (uint8_t *) ALIGN(((uintptr_t)p),
							L1_CACHE_BYTES);
		}
		kfree(k_buf);
		break;
	default:
		return -ENOTTY;
	}

	return err;
}

static int qcota_probe(struct platform_device *pdev)
{
	void *handle = NULL;
	int rc = 0;
	struct ota_dev_control *podev;
	struct ce_hw_support ce_support;
	struct ota_qce_dev *pqce;
	unsigned long flags;

	podev = &qcota_dev;
	pqce = kzalloc(sizeof(*pqce), GFP_KERNEL);
	if (!pqce) {
		pr_err("qcota_probe: Memory allocation FAIL\n");
		return -ENOMEM;
	}

	pqce->podev = podev;
	pqce->active_command = NULL;
	tasklet_init(&pqce->done_tasklet, req_done, (unsigned long)pqce);

	/* open qce */
	handle = qce_open(pdev, &rc);
	if (handle == NULL) {
		pr_err("%s: device %s, can not open qce\n",
			__func__, pdev->name);
		goto err;
	}
	if (qce_hw_support(handle, &ce_support) < 0 ||
					ce_support.ota == false) {
		pr_err("%s: device %s, qce does not support ota capability\n",
			__func__, pdev->name);
		rc = -ENODEV;
		goto err;
	}
	pqce->qce = handle;
	pqce->pdev = pdev;
	pqce->total_req = 0;
	pqce->err_req = 0;
	platform_set_drvdata(pdev, pqce);

	mutex_lock(&podev->register_lock);
	rc = 0;
	if (podev->registered == false) {
		rc = misc_register(&podev->miscdevice);
		if (rc == 0) {
			pqce->unit = podev->total_units;
			podev->total_units++;
			podev->registered = true;
		};
	} else {
		pqce->unit = podev->total_units;
		podev->total_units++;
	}
	mutex_unlock(&podev->register_lock);
	if (rc) {
		pr_err("ion: failed to register misc device.\n");
		goto err;
	}

	spin_lock_irqsave(&podev->lock, flags);
	list_add_tail(&pqce->qlist, &podev->qce_dev);
	spin_unlock_irqrestore(&podev->lock, flags);

	return 0;
err:
	if (handle)
		qce_close(handle);

	platform_set_drvdata(pdev, NULL);
	tasklet_kill(&pqce->done_tasklet);
	kfree(pqce);
	return rc;
}

static int qcota_remove(struct platform_device *pdev)
{
	struct ota_dev_control *podev;
	struct ota_qce_dev *pqce;
	unsigned long flags;

	pqce = platform_get_drvdata(pdev);
	if (!pqce)
		return 0;
	if (pqce->qce)
		qce_close(pqce->qce);

	podev = pqce->podev;
	if (!podev)
		goto ret;

	spin_lock_irqsave(&podev->lock, flags);
	list_del(&pqce->qlist);
	spin_unlock_irqrestore(&podev->lock, flags);

	mutex_lock(&podev->register_lock);
	if (--podev->total_units == 0) {
		if (podev->miscdevice.minor != MISC_DYNAMIC_MINOR)
			misc_deregister(&podev->miscdevice);
		podev->registered = false;
	}
	mutex_unlock(&podev->register_lock);
ret:

	tasklet_kill(&pqce->done_tasklet);
	kfree(pqce);
	return 0;
}

static struct of_device_id qcota_match[] = {
	{	.compatible = "qcom,qcota",
	},
	{}
};

static struct platform_driver qcota_plat_driver = {
	.probe = qcota_probe,
	.remove = qcota_remove,
	.driver = {
		.name = "qcota",
		.owner = THIS_MODULE,
		.of_match_table = qcota_match,
	},
};

static int _disp_stats(void)
{
	struct qcota_stat *pstat;
	int len = 0;
	struct ota_dev_control *podev = &qcota_dev;
	unsigned long flags;
	struct ota_qce_dev *p;

	pstat = &_qcota_stat;
	len = scnprintf(_debug_read_buf, DEBUG_MAX_RW_BUF - 1,
			"\nQualcomm OTA crypto accelerator Statistics:\n");

	len += scnprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F8 request                      : %llu\n",
					pstat->f8_req);
	len += scnprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F8 operation success            : %llu\n",
					pstat->f8_op_success);
	len += scnprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F8 operation fail               : %llu\n",
					pstat->f8_op_fail);

	len += scnprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F8 MP request                   : %llu\n",
					pstat->f8_mp_req);
	len += scnprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F8 MP operation success         : %llu\n",
					pstat->f8_mp_op_success);
	len += scnprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F8 MP operation fail            : %llu\n",
					pstat->f8_mp_op_fail);

	len += scnprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F8 Variable MP request          : %llu\n",
					pstat->f8_v_mp_req);
	len += scnprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F8 Variable MP operation success: %llu\n",
					pstat->f8_v_mp_op_success);
	len += scnprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F8 Variable MP operation fail   : %llu\n",
					pstat->f8_v_mp_op_fail);

	len += scnprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F9 request                      : %llu\n",
					pstat->f9_req);
	len += scnprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F9 operation success            : %llu\n",
					pstat->f9_op_success);
	len += scnprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F9 operation fail               : %llu\n",
					pstat->f9_op_fail);

	spin_lock_irqsave(&podev->lock, flags);

	list_for_each_entry(p, &podev->qce_dev, qlist) {
		len += scnprintf(
			_debug_read_buf + len,
			DEBUG_MAX_RW_BUF - len - 1,
			"   Engine %4d Req                 : %llu\n",
			p->unit,
			p->total_req
		);
		len += scnprintf(
			_debug_read_buf + len,
			DEBUG_MAX_RW_BUF - len - 1,
			"   Engine %4d Req Error           : %llu\n",
			p->unit,
			p->err_req
		);
	}

	spin_unlock_irqrestore(&podev->lock, flags);

	return len;
}

static int _debug_stats_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t _debug_stats_read(struct file *file, char __user *buf,
			size_t count, loff_t *ppos)
{
	int rc = -EINVAL;
	int len;

	len = _disp_stats();

	rc = simple_read_from_buffer((void __user *) buf, len,
			ppos, (void *) _debug_read_buf, len);

	return rc;
}

static ssize_t _debug_stats_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{
	struct ota_dev_control *podev = &qcota_dev;
	unsigned long flags;
	struct ota_qce_dev *p;

	memset((char *)&_qcota_stat, 0, sizeof(struct qcota_stat));

	spin_lock_irqsave(&podev->lock, flags);

	list_for_each_entry(p, &podev->qce_dev, qlist) {
		p->total_req = 0;
		p->err_req = 0;
	}

	spin_unlock_irqrestore(&podev->lock, flags);

	return count;
}

static const struct file_operations _debug_stats_ops = {
	.open =         _debug_stats_open,
	.read =         _debug_stats_read,
	.write =        _debug_stats_write,
};

static int _qcota_debug_init(void)
{
	int rc;
	char name[DEBUG_MAX_FNAME];
	struct dentry *dent;

	_debug_dent = debugfs_create_dir("qcota", NULL);
	if (IS_ERR(_debug_dent)) {
		pr_err("qcota debugfs_create_dir fail, error %ld\n",
				PTR_ERR(_debug_dent));
		return PTR_ERR(_debug_dent);
	}

	snprintf(name, DEBUG_MAX_FNAME-1, "stats-0");
	_debug_qcota = 0;
	dent = debugfs_create_file(name, 0644, _debug_dent,
				&_debug_qcota, &_debug_stats_ops);
	if (dent == NULL) {
		pr_err("qcota debugfs_create_file fail, error %ld\n",
					PTR_ERR(dent));
		rc = PTR_ERR(dent);
		goto err;
	}
	return 0;
err:
	debugfs_remove_recursive(_debug_dent);
	return rc;
}

static int __init qcota_init(void)
{
	int rc;
	struct ota_dev_control *podev;

	rc = _qcota_debug_init();
	if (rc)
		return rc;

	podev = &qcota_dev;
	INIT_LIST_HEAD(&podev->ready_commands);
	INIT_LIST_HEAD(&podev->qce_dev);
	spin_lock_init(&podev->lock);
	mutex_init(&podev->register_lock);
	podev->registered = false;
	podev->total_units = 0;

	return platform_driver_register(&qcota_plat_driver);
}
static void __exit qcota_exit(void)
{
	debugfs_remove_recursive(_debug_dent);
	platform_driver_unregister(&qcota_plat_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Rohit Vaswani <rvaswani@codeaurora.org>");
MODULE_DESCRIPTION("Qualcomm Ota Crypto driver");
MODULE_VERSION("1.02");

module_init(qcota_init);
module_exit(qcota_exit);
