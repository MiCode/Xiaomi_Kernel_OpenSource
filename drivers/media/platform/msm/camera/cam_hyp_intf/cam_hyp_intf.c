/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
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

#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/msm_ion.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/dma-buf.h>
#include <media/cam_hyp_intf.h>
#include <microvisor/microvisor.h>
#include <microvisor/resource_manager.h>
#include "cam_debug_util.h"

static const char CAM_HYP_INTF_DRIVER_NAME[] = "cam-hyp-intf";
static const int MAX_WAIT_TIMEOUT_MS = 100;

/**
 * struct cam_hyp_intf_device
 *
 * @open_cnt: Number of open instances
 * @cdev: Represents chracter device
 * @pdev: Rpresents platform device for this driver
 * @cam_hyp_intf_class: entry in /sys/class
 * @ionc: ion client
 * @tx_pipe_kcap: transmit pipe handle
 * @rx_pipe_kcap: receiver pipe handle
 */

struct cam_hyp_intf_device {
	atomic_t open_cnt;
	struct cdev cdev;
	struct platform_device *pdev;
	struct class *cam_hyp_intf_class;
	dev_t cam_hyp_intf_devno;
	struct ion_client *ionc;
	okl4_kcap_t tx_pipe_kcap;
	okl4_kcap_t rx_pipe_kcap;
	struct mutex hyp_intf_lock;
};

static okl4_error_t okl4_pipe_control_locked(okl4_kcap_t kcap, uint8_t control)
{
	okl4_pipe_control_t x = 0;

	okl4_pipe_control_setdoop(&x, true);
	okl4_pipe_control_setoperation(&x, control);
	return _okl4_sys_pipe_control(kcap, x);
}
static int cam_hyp_intf_open(struct inode *inode, struct file *filp)
{
	int rc = 0;
	okl4_error_t okl4_err;

	struct cam_hyp_intf_device *cam_hyp_intf_dev =
		container_of(inode->i_cdev, struct cam_hyp_intf_device, cdev);

	mutex_lock(&cam_hyp_intf_dev->hyp_intf_lock);

	if (atomic_inc_return(&cam_hyp_intf_dev->open_cnt) > 1) {
		atomic_dec(&cam_hyp_intf_dev->open_cnt);
		CAM_ERR(CAM_HYP, "%d instances of driver opened",
			atomic_read(&cam_hyp_intf_dev->open_cnt));
		rc = -EBUSY;
		goto err_busy;
	}

	filp->private_data = cam_hyp_intf_dev;

	okl4_err = okl4_pipe_control_locked(cam_hyp_intf_dev->rx_pipe_kcap,
			OKL4_PIPE_CONTROL_OP_SET_RX_READY);
	if (okl4_err != OKL4_OK) {
		CAM_ERR(CAM_HYP, "RX pipe is not ready (okl4_err %u)",
			okl4_err);
		rc = -EIO;
		goto fail_rx_ready;
	}

	okl4_err = okl4_pipe_control_locked(cam_hyp_intf_dev->tx_pipe_kcap,
			OKL4_PIPE_CONTROL_OP_SET_TX_READY);
	if (okl4_err != OKL4_OK) {
		CAM_ERR(CAM_HYP, "TX pipe is not ready (okl4_err %u)",
			okl4_err);
		rc = -EIO;
		goto fail_tx_ready;
	}
	mutex_unlock(&cam_hyp_intf_dev->hyp_intf_lock);
	return 0;

fail_tx_ready:
	okl4_pipe_control_locked(cam_hyp_intf_dev->rx_pipe_kcap,
		OKL4_PIPE_CONTROL_OP_RESET);
fail_rx_ready:
	atomic_dec(&cam_hyp_intf_dev->open_cnt);
err_busy:
	mutex_unlock(&cam_hyp_intf_dev->hyp_intf_lock);
	return rc;
}

static int cam_hyp_intf_release(struct inode *inode, struct file *filp)
{
	struct cam_hyp_intf_device *cam_hyp_intf_dev = filp->private_data;

	mutex_lock(&cam_hyp_intf_dev->hyp_intf_lock);
	okl4_pipe_control_locked(cam_hyp_intf_dev->rx_pipe_kcap,
		OKL4_PIPE_CONTROL_OP_RESET);
	okl4_pipe_control_locked(cam_hyp_intf_dev->tx_pipe_kcap,
		OKL4_PIPE_CONTROL_OP_RESET);
	atomic_dec(&cam_hyp_intf_dev->open_cnt);

	mutex_unlock(&cam_hyp_intf_dev->hyp_intf_lock);
	return 0;
}

static int cam_hyp_intf_prepare_res_mgr_sglist_locked(
	struct cam_hyp_intf_device *dev, uint32_t fd,
	struct res_mgr_msg **msg, size_t *size)
{
	struct sg_table *sg_table;
	struct scatterlist *sg;
	struct dma_buf *dma_buf = NULL;
	struct dma_buf_attachment *dup_attach = NULL;
	int i = 0, ret = 0;

	dma_buf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		CAM_ERR(CAM_HYP, "dma_buf_get() for fd %d failed", fd);
		return -ENOMEM;
	}

	dup_attach = dma_buf_attach(dma_buf, &dev->pdev->dev);
	if (IS_ERR_OR_NULL(dup_attach)) {
		CAM_ERR(CAM_HYP, "dma_buf_attach() for fd %d failed", fd);
		ret = -ENOMEM;
		goto err_put;
	}

	sg_table = dma_buf_map_attachment(dup_attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(sg_table)) {
		ret = PTR_ERR(sg_table);
		CAM_ERR(CAM_HYP,
			"dma_buf_map_attachment for ion_fd %d failed ret %d",
			fd, ret);
		ret = -ENOMEM;
		goto err_detach;
	}

	*size = offsetof(struct res_mgr_msg, securecam.sglist.regions) +
		(sizeof(struct res_mgr_region) * sg_table->nents);
	*msg = kzalloc(*size, GFP_KERNEL);
	if (*msg == NULL)
		return -ENOMEM;

	sg = sg_table->sgl;
	(*msg)->securecam.sglist.region_count =  sg_table->nents;
	CAM_DBG(CAM_HYP, "SG entries(%d, %d)",
		(*msg)->securecam.sglist.region_count, sg_table->nents);
	for (i = 0; i < sg_table->nents; ++i) {
		if (!sg)
			return -EINVAL;

		CAM_DBG(CAM_HYP, "SG entry(%d): phy_addr (%llx) length (%x)",
			i, sg_phys(sg), sg->length);
		(*msg)->securecam.sglist.regions[i].address_ipa =
			sg_phys(sg);
		(*msg)->securecam.sglist.regions[i].size =
			sg->length;
		sg = sg_next(sg);
	}

	ret = 0;
	dma_buf_unmap_attachment(dup_attach, sg_table, DMA_BIDIRECTIONAL);
err_detach:
	dma_buf_detach(dma_buf, dup_attach);
err_put:
	dma_buf_put(dma_buf);

	return ret;
}

static int cam_hyp_intf_get_mem_handle(
	struct cam_hyp_intf_device *cam_hyp_intf_dev,
	int fd, uint32_t *handle)
{
	okl4_error_t ret;
	struct _okl4_sys_pipe_recv_return recv;
	struct res_mgr_msg *msg = NULL;
	int i, rc = 0;
	size_t size = 0;
	uint8_t *tmp_recv_msg_addr = NULL;

	CAM_DBG(CAM_HYP, "secure handle for camrea 0x%X", fd);

	if (fd < 0) {
		CAM_ERR(CAM_HYP, "Invalid ion handle");
		rc = -EINVAL;
		goto err;
	}

	rc = cam_hyp_intf_prepare_res_mgr_sglist_locked(cam_hyp_intf_dev, fd,
		&msg, &size);
	if (rc) {
		CAM_ERR(CAM_HYP, "SG list craetion failed");
		rc = -ENOMEM;
		goto err;
	}
	msg->msg_id = RES_MGR_SECURECAM_GET_HANDLE;

	CAM_DBG(CAM_HYP, "Tx msg sg_region count %d",
		msg->securecam.sglist.region_count);
	CAM_DBG(CAM_HYP, "Tx msg id [0x%X]", msg->msg_id);
	for (i = 0; i < msg->securecam.sglist.region_count; ++i) {
		CAM_DBG(CAM_HYP, "sg region[%d] phy_addr(0x%llX) length(0x%X)",
			i,
			msg->securecam.sglist.regions[i].address_ipa,
			msg->securecam.sglist.regions[i].size);
	}

	CAM_DBG(CAM_HYP, "Sending Tx message");
	i = 0;
	do {
		ret = _okl4_sys_pipe_send(cam_hyp_intf_dev->tx_pipe_kcap,
		   size, (uint8_t *)msg);
		if (ret == OKL4_OK)
			break;
		msleep_interruptible(20);
	} while ((MAX_WAIT_TIMEOUT_MS - i > 0) && (i += 5));

	if (ret != OKL4_OK) {
		CAM_ERR(CAM_HYP, "Timed out on sending message(ret %u)",
			ret);
		CAM_DBG(CAM_HYP, "Dumping Tx message");
		tmp_recv_msg_addr = (uint8_t *)msg;
		for (i = 0; i < size; i++) {
			CAM_DBG(CAM_HYP, "0x%lX => 0x%X",
				(unsigned long)(tmp_recv_msg_addr+i),
				*(tmp_recv_msg_addr+i));
		}
		rc = -ETIMEDOUT;
		goto err;
	}

	memset(msg, 0, size);
	recv.error = !OKL4_OK;
	i = 0;

	CAM_DBG(CAM_HYP, "Polling for Rx message");
	do {
		recv = _okl4_sys_pipe_recv(cam_hyp_intf_dev->rx_pipe_kcap,
			size, (uint8_t *)msg);
		if (recv.error == OKL4_OK)
			break;
		msleep_interruptible(20);
	} while ((MAX_WAIT_TIMEOUT_MS - i > 0) && (i += 5));


	CAM_DBG(CAM_HYP, "Received Handle (0x%X, fd 0x%X)",
		msg->securecam.handle, fd);
	CAM_DBG(CAM_HYP, "Received Sec Msg id 0x%X", msg->msg_id);
	CAM_DBG(CAM_HYP, "Received Sec Msg size(ret.size) %u",
		(unsigned int)recv.size);
	CAM_DBG(CAM_HYP, "Received Sec Msg errotType(ret.type 0x%X)",
		recv.error);

	if (recv.error != OKL4_OK) {
		CAM_ERR(CAM_HYP, "Timed out to recv message(ret %u)",
			recv.error);
		CAM_DBG(CAM_HYP, "Dumping RX message");
		tmp_recv_msg_addr = (uint8_t *)msg;
		for (i = 0; i < recv.size; i++) {
			CAM_DBG(CAM_HYP, "0x%lX => 0x%X",
				(unsigned long)(tmp_recv_msg_addr+i),
				*(tmp_recv_msg_addr+i));
		}
		rc = -ETIMEDOUT;
		goto err;
	}

	*handle = msg->securecam.handle;
	rc = 0;
err:
	kfree(msg);
	msg = NULL;
	return rc;

}

static long cam_hyp_intf_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	long rc = 0;
	unsigned int dir;
	struct cam_hyp_intf_device *cam_hyp_intf_dev = NULL;

	struct umd_arg {
		struct cam_hyp_intf_hyp_handle_type hyp_handle;
	} data;

	if (!(_IOC_TYPE(cmd) == MSM_CAM_HYP_INTF_IOCTL_MAGIC &&
		_IOC_NR(cmd) <= MSM_CAM_HYP_INTF_IOCTL_MAX)) {
		CAM_ERR(CAM_HYP, "Invalid command %d", cmd);
		rc = -EINVAL;
		goto err;
	}

	switch (cmd) {
	case MSM_CAM_HYP_INTF_IOCTL_GET_HYP_HANDLE:
	dir = _IOC_DIR(cmd);
	if (dir & _IOC_WRITE) {
		if (copy_from_user(&data,
			(void __user *)arg, _IOC_SIZE(cmd))) {
			rc = -EFAULT;
			CAM_ERR(CAM_HYP, "copy_from_user failed");
			goto err;
		}
	} else {
		rc = -EFAULT;
		CAM_ERR(CAM_HYP, "Invalid direction command");
		goto err;
	}

	cam_hyp_intf_dev = filp->private_data;
	mutex_lock(&cam_hyp_intf_dev->hyp_intf_lock);

	rc = cam_hyp_intf_get_mem_handle(cam_hyp_intf_dev,
		data.hyp_handle.fd, &(data.hyp_handle.handle));

	if (rc < 0) {
		CAM_ERR(CAM_HYP, "Failed in hyp calls(rc %ld)", rc);
		mutex_unlock(&cam_hyp_intf_dev->hyp_intf_lock);
		goto err;
	}
	if (copy_to_user((void __user *)arg, &data,
		_IOC_SIZE(cmd))) {
		CAM_ERR(CAM_HYP, "Failed to copy to user");
		rc = -EFAULT;
		mutex_unlock(&cam_hyp_intf_dev->hyp_intf_lock);
		goto err;
	}
	mutex_unlock(&cam_hyp_intf_dev->hyp_intf_lock);
	break;

	default:
		CAM_ERR(CAM_HYP, "Invalid command %d", cmd);
		rc = -EINVAL;
	}

err:
	return rc;
}

#ifdef CONFIG_COMPAT
static long cam_hyp_intf_compat_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	return cam_hyp_intf_ioctl(filp, cmd, arg);
}
#endif

static const struct file_operations cam_hyp_intf_fops = {
	.owner          = THIS_MODULE,
	.open           = cam_hyp_intf_open,
	.release        = cam_hyp_intf_release,
	.unlocked_ioctl = cam_hyp_intf_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = cam_hyp_intf_compat_ioctl,
#endif
};

static int cam_hyp_intf_probe(struct platform_device *pdev)
{
	int rc;
	struct device *dev;
	struct cam_hyp_intf_device *cam_hyp_intf_device_p;
	u32 reg[2];

	cam_hyp_intf_device_p = kzalloc(
		sizeof(*cam_hyp_intf_device_p), GFP_KERNEL);

	if (!cam_hyp_intf_device_p)
		return -ENOMEM;

	atomic_set(&cam_hyp_intf_device_p->open_cnt, 0);

	cam_hyp_intf_device_p->pdev = pdev;

	rc = alloc_chrdev_region(
		&cam_hyp_intf_device_p->cam_hyp_intf_devno, 0,
		1, CAM_HYP_INTF_DRIVER_NAME);
	if (rc < 0) {
		CAM_ERR(CAM_HYP, "failed to allocate chrdev with %d", rc);
		goto err_char_dev_fail;
	}

	cam_hyp_intf_device_p->cam_hyp_intf_class =
		class_create(THIS_MODULE, CAM_HYP_INTF_DRIVER_NAME);
	if (IS_ERR_OR_NULL(cam_hyp_intf_device_p->cam_hyp_intf_class)) {
		rc = PTR_ERR(cam_hyp_intf_device_p->cam_hyp_intf_class);
		CAM_ERR(CAM_HYP, "create device class failed with %d", rc);
		goto err_class_fail;
	}

	dev = device_create(cam_hyp_intf_device_p->cam_hyp_intf_class,
		NULL, cam_hyp_intf_device_p->cam_hyp_intf_devno, NULL,
		"%s%d", CAM_HYP_INTF_DRIVER_NAME, pdev->id);

	if (IS_ERR_OR_NULL(dev)) {
		CAM_ERR(CAM_HYP, "error creating device");
		rc = -ENODEV;
		goto err_dev_fail;
	}

	cdev_init(&cam_hyp_intf_device_p->cdev, &cam_hyp_intf_fops);
	cam_hyp_intf_device_p->cdev.owner = THIS_MODULE;
	cam_hyp_intf_device_p->cdev.ops =
		(const struct file_operations *) &cam_hyp_intf_fops;
	rc = cdev_add(&cam_hyp_intf_device_p->cdev,
			cam_hyp_intf_device_p->cam_hyp_intf_devno, 1);
	if (rc < 0) {
		CAM_ERR(CAM_HYP, "error adding cdev");
		rc = -ENODEV;
		goto err_cdev_add_fail;
	}

	if (of_property_read_u32_array(pdev->dev.of_node,
		"reg", reg, 2)) {
		CAM_ERR(CAM_HYP, "unable to read pipe handles");
		rc = -ENODEV;
		goto err_fail_pipe_kcaps;
	}

	CAM_DBG(CAM_HYP, "tx pipe %d, rx pipe %d\n", reg[0], reg[1]);
	cam_hyp_intf_device_p->tx_pipe_kcap = reg[0];
	cam_hyp_intf_device_p->rx_pipe_kcap = reg[1];

	mutex_init(&cam_hyp_intf_device_p->hyp_intf_lock);
	platform_set_drvdata(pdev, cam_hyp_intf_device_p);

	return 0;

err_fail_pipe_kcaps:
err_cdev_add_fail:
	device_destroy(cam_hyp_intf_device_p->cam_hyp_intf_class,
		cam_hyp_intf_device_p->cam_hyp_intf_devno);

err_dev_fail:
	class_destroy(cam_hyp_intf_device_p->cam_hyp_intf_class);

err_class_fail:
	unregister_chrdev_region(cam_hyp_intf_device_p->cam_hyp_intf_devno, 1);

err_char_dev_fail:
	kfree(cam_hyp_intf_device_p);
	return rc;

}

static int cam_hyp_intf_remove(struct platform_device *pdev)
{
	struct cam_hyp_intf_device *cam_hyp_intf_device_p =
		platform_get_drvdata(pdev);

	if (cam_hyp_intf_device_p) {
		cdev_del(&cam_hyp_intf_device_p->cdev);
		device_destroy(cam_hyp_intf_device_p->cam_hyp_intf_class,
				cam_hyp_intf_device_p->cam_hyp_intf_devno);
		class_destroy(cam_hyp_intf_device_p->cam_hyp_intf_class);
		unregister_chrdev_region(
			cam_hyp_intf_device_p->cam_hyp_intf_devno, 1);
		kfree(cam_hyp_intf_device_p);
	}
	return 0;
}

MODULE_DEVICE_TABLE(of, msm_cam_hyp_intf_dt_match);

static const struct of_device_id msm_cam_hyp_intf_dt_match[] = {
	{
		.compatible = "qcom,resource-manager-scbuf",
	},
	{}
};

static struct platform_driver cam_hyp_intf_driver = {
	.probe  = cam_hyp_intf_probe,
	.remove = cam_hyp_intf_remove,
	.driver = {
		.name  = "msm_cam_hyp_intf",
		.owner = THIS_MODULE,
		.of_match_table = msm_cam_hyp_intf_dt_match,
	},
};

static int __init cam_hyp_intf_init_module(void)
{
	return platform_driver_register(&cam_hyp_intf_driver);
}

static void __exit cam_hyp_intf_exit_module(void)
{
	platform_driver_unregister(&cam_hyp_intf_driver);
}

module_init(cam_hyp_intf_init_module);
module_exit(cam_hyp_intf_exit_module);
MODULE_DESCRIPTION("MSM Camera Hypervisor Interface");
MODULE_LICENSE("GPL v2");
