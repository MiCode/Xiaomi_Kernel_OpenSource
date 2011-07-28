/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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


#include <linux/qcota.h>
#include "qce.h"
#include "qce_ota.h"

enum qce_ota_oper_enum {
	QCE_OTA_F8_OPER   = 0,
	QCE_OTA_MPKT_F8_OPER = 1,
	QCE_OTA_F9_OPER  = 2,
	QCE_OTA_OPER_LAST
};

struct ota_dev_control;

struct ota_async_req {
	struct list_head list;
	struct completion complete;
	int err;
	enum qce_ota_oper_enum op;
	union {
		struct qce_f9_req f9_req;
		struct qce_f8_req f8_req;
		struct qce_f8_multi_pkt_req f8_mp_req;
	} req;

	struct ota_dev_control  *podev;
};

/*
 * Register ourselves as a misc device to be able to access the ota
 * from userspace.
 */


#define QCOTA_DEV	"qcota"


struct ota_dev_control {

	/* misc device */
	struct miscdevice miscdevice;

	/* qce handle */
	void *qce;

	/* platform device */
	struct platform_device *pdev;

	unsigned magic;

	struct list_head ready_commands;
	struct ota_async_req *active_command;
	spinlock_t lock;
	struct tasklet_struct done_tasklet;
};

#define OTA_MAGIC 0x4f544143

static long qcota_ioctl(struct file *file,
			  unsigned cmd, unsigned long arg);
static int qcota_open(struct inode *inode, struct file *file);
static int qcota_release(struct inode *inode, struct file *file);
static int start_req(struct ota_dev_control *podev);

static const struct file_operations qcota_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = qcota_ioctl,
	.open = qcota_open,
	.release = qcota_release,
};

static struct ota_dev_control qcota_dev[] = {
	{
		.miscdevice = {
			.minor = MISC_DYNAMIC_MINOR,
			.name = "qcota0",
			.fops = &qcota_fops,
		},
		.magic = OTA_MAGIC,
	},
	{
		.miscdevice = {
			.minor = MISC_DYNAMIC_MINOR,
			.name = "qcota1",
			.fops = &qcota_fops,
		},
		.magic = OTA_MAGIC,
	},
	{
		.miscdevice = {
			.minor = MISC_DYNAMIC_MINOR,
			.name = "qcota2",
			.fops = &qcota_fops,
		},
		.magic = OTA_MAGIC,
	}
};

#define MAX_OTA_DEVICE ARRAY_SIZE(qcota_dev)

#define DEBUG_MAX_FNAME  16
#define DEBUG_MAX_RW_BUF 1024

struct qcota_stat {
	u32 f8_req;
	u32 f8_mp_req;
	u32 f9_req;
	u32 f8_op_success;
	u32 f8_op_fail;
	u32 f8_mp_op_success;
	u32 f8_mp_op_fail;
	u32 f9_op_success;
	u32 f9_op_fail;
};
static struct qcota_stat _qcota_stat[MAX_OTA_DEVICE];
static struct dentry *_debug_dent;
static char _debug_read_buf[DEBUG_MAX_RW_BUF];
static int _debug_qcota[MAX_OTA_DEVICE];

static struct ota_dev_control *qcota_minor_to_control(unsigned n)
{
	int i;

	for (i = 0; i < MAX_OTA_DEVICE; i++) {
		if (qcota_dev[i].miscdevice.minor == n)
			return &qcota_dev[i];
	}
	return NULL;
}

static int qcota_open(struct inode *inode, struct file *file)
{
	struct ota_dev_control *podev;

	podev = qcota_minor_to_control(MINOR(inode->i_rdev));
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

static void req_done(unsigned long data)
{
	struct ota_dev_control *podev = (struct ota_dev_control *)data;
	struct ota_async_req *areq;
	unsigned long flags;
	struct ota_async_req *new_req = NULL;
	int ret = 0;

	spin_lock_irqsave(&podev->lock, flags);
	areq = podev->active_command;
	podev->active_command = NULL;

again:
	if (!list_empty(&podev->ready_commands)) {
		new_req = container_of(podev->ready_commands.next,
						struct ota_async_req, list);
		list_del(&new_req->list);
		podev->active_command = new_req;
		new_req->err = 0;
		ret = start_req(podev);
	}

	spin_unlock_irqrestore(&podev->lock, flags);

	if (areq)
		complete(&areq->complete);

	if (new_req && ret) {
		complete(&new_req->complete);
		spin_lock_irqsave(&podev->lock, flags);
		podev->active_command = NULL;
		areq = NULL;
		ret = 0;
		new_req = NULL;
		goto again;
	}

	return;
}

static void f9_cb(void *cookie, unsigned char *icv, unsigned char *iv,
	int ret)
{
	struct ota_async_req *areq = (struct ota_async_req *) cookie;
	struct ota_dev_control *podev;
	struct qcota_stat *pstat;

	podev = areq->podev;
	pstat = &_qcota_stat[podev->pdev->id];
	areq->req.f9_req.mac_i  = (uint32_t) icv;

	if (ret)
		areq->err = -ENXIO;
	else
		areq->err = 0;

	tasklet_schedule(&podev->done_tasklet);
};

static void f8_cb(void *cookie, unsigned char *icv, unsigned char *iv,
	int ret)
{
	struct ota_async_req *areq = (struct ota_async_req *) cookie;
	struct ota_dev_control *podev;
	struct qcota_stat *pstat;

	podev = areq->podev;
	pstat = &_qcota_stat[podev->pdev->id];

	if (ret)
		areq->err = -ENXIO;
	else
		areq->err = 0;

	tasklet_schedule(&podev->done_tasklet);
};

static int start_req(struct ota_dev_control *podev)
{
	struct ota_async_req *areq;
	struct qce_f9_req *pf9;
	struct qce_f8_multi_pkt_req *p_mp_f8;
	struct qce_f8_req *pf8;
	int ret = 0;

	/* start the command on the podev->active_command */
	areq = podev->active_command;
	areq->podev = podev;

	switch (areq->op) {
	case QCE_OTA_F8_OPER:
		pf8 = &areq->req.f8_req;
		ret = qce_f8_req(podev->qce, pf8, areq, f8_cb);
		break;
	case QCE_OTA_MPKT_F8_OPER:
		p_mp_f8 = &areq->req.f8_mp_req;
		ret = qce_f8_multi_pkt_req(podev->qce, p_mp_f8, areq, f8_cb);
		break;

	case QCE_OTA_F9_OPER:
		pf9 = &areq->req.f9_req;
		ret =  qce_f9_req(podev->qce, pf9, areq, f9_cb);
		break;

	default:
		ret = -ENOTSUPP;
		break;
	};
	areq->err = ret;
	return ret;
};

static int submit_req(struct ota_async_req *areq, struct ota_dev_control *podev)
{
	unsigned long flags;
	int ret = 0;
	struct qcota_stat *pstat;

	areq->err = 0;
	spin_lock_irqsave(&podev->lock, flags);
	if (podev->active_command == NULL) {
		podev->active_command = areq;
		ret = start_req(podev);
	} else {
		list_add_tail(&areq->list, &podev->ready_commands);
	}

	if (ret != 0)
		podev->active_command = NULL;
	spin_unlock_irqrestore(&podev->lock, flags);

	if (ret == 0)
		wait_for_completion(&areq->complete);

	pstat = &_qcota_stat[podev->pdev->id];
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
	default:
		if (areq->err)
			pstat->f9_op_fail++;
		else
			pstat->f9_op_success++;
		break;
	};

	return areq->err;
};

static long qcota_ioctl(struct file *file,
			  unsigned cmd, unsigned long arg)
{
	int err = 0;
	struct ota_dev_control *podev;
	uint8_t *user_src;
	uint8_t *user_dst;
	uint8_t *k_buf = NULL;
	struct ota_async_req areq;
	uint32_t total;
	struct qcota_stat *pstat;

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

	pstat = &_qcota_stat[podev->pdev->id];

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

		total = areq.req.f8_mp_req.num_pkt *
				areq.req.f8_mp_req.qce_f8_req.data_len;

		user_src = areq.req.f8_mp_req.qce_f8_req.data_in;
		if (!access_ok(VERIFY_READ, (void __user *)
				user_src, total))
			return -EFAULT;

		user_dst = areq.req.f8_mp_req.qce_f8_req.data_out;
		if (!access_ok(VERIFY_WRITE, (void __user *)
				user_dst, total))
			return -EFAULT;

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

	if (pdev->id >= MAX_OTA_DEVICE) {
		pr_err("%s: device id %d  exceeds allowed %d\n",
			__func__, pdev->id, MAX_OTA_DEVICE);
		return -ENOENT;
	}

	podev = &qcota_dev[pdev->id];

	INIT_LIST_HEAD(&podev->ready_commands);
	podev->active_command = NULL;
	spin_lock_init(&podev->lock);
	tasklet_init(&podev->done_tasklet, req_done, (unsigned long)podev);

	/* open qce */
	handle = qce_open(pdev, &rc);
	if (handle == NULL) {
		pr_err("%s: device id %d, can not open qce\n",
			__func__, pdev->id);
		platform_set_drvdata(pdev, NULL);
		return rc;
	}
	if (qce_hw_support(handle, &ce_support) < 0 ||
					ce_support.ota == false) {
		pr_err("%s: device id %d, qce does not support ota capability\n",
			__func__, pdev->id);
		rc = -ENODEV;
		goto err;
	}
	podev->qce = handle;
	podev->pdev = pdev;
	platform_set_drvdata(pdev, podev);

	rc = misc_register(&podev->miscdevice);
	if (rc < 0)
		goto err;

	return 0;
err:
	if (handle)
		qce_close(handle);
	platform_set_drvdata(pdev, NULL);
	podev->qce = NULL;
	podev->pdev = NULL;
	return rc;
};

static int qcota_remove(struct platform_device *pdev)
{
	struct ota_dev_control *podev;

	podev = platform_get_drvdata(pdev);
	if (!podev)
		return 0;
	if (podev->qce)
		qce_close(podev->qce);

	if (podev->miscdevice.minor != MISC_DYNAMIC_MINOR)
		misc_deregister(&podev->miscdevice);
	tasklet_kill(&podev->done_tasklet);
	return 0;
};

static struct platform_driver qcota_plat_driver = {
	.probe = qcota_probe,
	.remove = qcota_remove,
	.driver = {
		.name = "qcota",
		.owner = THIS_MODULE,
	},
};

static int _disp_stats(int id)
{
	struct qcota_stat *pstat;
	int len = 0;

	pstat = &_qcota_stat[id];
	len = snprintf(_debug_read_buf, DEBUG_MAX_RW_BUF - 1,
			"\nQualcomm OTA crypto accelerator %d Statistics:\n",
				id + 1);

	len += snprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F8 request             : %d\n",
					pstat->f8_req);
	len += snprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F8 operation success   : %d\n",
					pstat->f8_op_success);
	len += snprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F8 operation fail      : %d\n",
					pstat->f8_op_fail);

	len += snprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F8 MP request          : %d\n",
					pstat->f8_mp_req);
	len += snprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F8 MP operation success: %d\n",
					pstat->f8_mp_op_success);
	len += snprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F8 MP operation fail   : %d\n",
					pstat->f8_mp_op_fail);

	len += snprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F9 request             : %d\n",
					pstat->f9_req);
	len += snprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F9 operation success   : %d\n",
					pstat->f9_op_success);
	len += snprintf(_debug_read_buf + len, DEBUG_MAX_RW_BUF - len - 1,
			"   F9 operation fail      : %d\n",
					pstat->f9_op_fail);

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
	int qcota = *((int *) file->private_data);
	int len;

	len = _disp_stats(qcota);

	rc = simple_read_from_buffer((void __user *) buf, len,
			ppos, (void *) _debug_read_buf, len);

	return rc;
}

static ssize_t _debug_stats_write(struct file *file, const char __user *buf,
			size_t count, loff_t *ppos)
{

	int qcota = *((int *) file->private_data);

	memset((char *)&_qcota_stat[qcota], 0, sizeof(struct qcota_stat));
	return count;
};

static const struct file_operations _debug_stats_ops = {
	.open =         _debug_stats_open,
	.read =         _debug_stats_read,
	.write =        _debug_stats_write,
};

static int _qcota_debug_init(void)
{
	int rc;
	char name[DEBUG_MAX_FNAME];
	int i;
	struct dentry *dent;

	_debug_dent = debugfs_create_dir("qcota", NULL);
	if (IS_ERR(_debug_dent)) {
		pr_err("qcota debugfs_create_dir fail, error %ld\n",
				PTR_ERR(_debug_dent));
		return PTR_ERR(_debug_dent);
	}

	for (i = 0; i < MAX_OTA_DEVICE; i++) {
		snprintf(name, DEBUG_MAX_FNAME-1, "stats-%d", i+1);
		_debug_qcota[i] = i;
		dent = debugfs_create_file(name, 0644, _debug_dent,
				&_debug_qcota[i], &_debug_stats_ops);
		if (dent == NULL) {
			pr_err("qcota debugfs_create_file fail, error %ld\n",
					PTR_ERR(dent));
			rc = PTR_ERR(dent);
			goto err;
		}
	}
	return 0;
err:
	debugfs_remove_recursive(_debug_dent);
	return rc;
}

static int __init qcota_init(void)
{
	int rc;

	rc = _qcota_debug_init();
	if (rc)
		return rc;
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
MODULE_VERSION("1.01");

module_init(qcota_init);
module_exit(qcota_exit);
