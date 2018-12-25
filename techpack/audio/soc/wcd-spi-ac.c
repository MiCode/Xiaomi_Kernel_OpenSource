/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <linux/wcd-spi-ac-params.h>
#include <soc/wcd-spi-ac.h>
#include <soc/qcom/msm_qmi_interface.h>

#include "wcd_spi_ctl_v01.h"

#define WCD_SPI_AC_PFS_ENTRY_MAX_LEN	16
#define WCD_SPI_AC_WRITE_CMD_MIN_SIZE	\
	(sizeof(struct wcd_spi_ac_write_cmd))
#define WCD_SPI_AC_WRITE_CMD_MAX_SIZE		\
	(WCD_SPI_AC_WRITE_CMD_MIN_SIZE +	\
	 (WCD_SPI_AC_MAX_BUFFERS *		\
	  sizeof(struct wcd_spi_ac_buf_data)))

#define WCD_SPI_AC_MUTEX_LOCK(dev, lock)	\
{						\
	dev_dbg(dev, "%s: mutex_lock(%s)\n",	\
		__func__, __stringify_1(lock));	\
	mutex_lock(&lock);			\
}

#define WCD_SPI_AC_MUTEX_UNLOCK(dev, lock)	\
{						\
	dev_dbg(dev, "%s: mutex_unlock(%s)\n",	\
		__func__, __stringify_1(lock));	\
	mutex_unlock(&lock);			\
}

/*
 * All bits of status should be cleared for SPI access
 * to be released.
 */
#define WCD_SPI_AC_STATUS_RELEASE_ACCESS	0x00
#define WCD_SPI_AC_LOCAL_ACCESS	(0)
#define WCD_SPI_AC_REMOTE_ACCESS (0x01)
#define WCD_SPI_AC_NO_ACCESS (0x02)
#define WCD_SPI_CTL_INS_ID 0
#define WCD_SPI_AC_QMI_TIMEOUT_MS 100

struct wcd_spi_ac_priv {

	/* Pointer to device for this driver */
	struct device *dev;

	/* Pointer to parent's device */
	struct device *parent;

	/* char dev related */
	struct class *cls;
	struct device *chardev;
	struct cdev cdev;
	dev_t cdev_num;

	/* proc entry related */
	struct proc_dir_entry *pfs_root;
	struct proc_dir_entry *pfs_status;

	/* service status related */
	u8 svc_offline;
	u8 svc_offline_change;
	wait_queue_head_t svc_poll_wait;
	struct mutex status_lock;
	struct completion online_compl;

	/* state maintenence related */
	u32 state;
	struct mutex state_lock;
	u8 current_access;

	/* qmi related */
	struct qmi_handle *qmi_hdl;
	struct work_struct svc_arr_work;
	struct work_struct svc_exit_work;
	struct notifier_block nb;
	struct mutex msg_lock;
	struct mutex event_lock;
	struct workqueue_struct *qmi_wq;
	struct work_struct recv_msg_work;
};


static void wcd_spi_ac_status_change(struct wcd_spi_ac_priv *ac,
				     u8 online)
{
	WCD_SPI_AC_MUTEX_LOCK(ac->dev, ac->status_lock);
	ac->svc_offline = !online;
	/* Make sure the write is complete */
	wmb();
	xchg(&ac->svc_offline_change, 1);
	wake_up_interruptible(&ac->svc_poll_wait);
	dev_dbg(ac->dev,
		"%s request %u offline %u off_change %u\n",
		__func__, online, ac->svc_offline,
		ac->svc_offline_change);
	WCD_SPI_AC_MUTEX_UNLOCK(ac->dev, ac->status_lock);
}

static int wcd_spi_ac_status_open(struct inode *inode,
				    struct file *file)
{
	struct wcd_spi_ac_priv *ac = PDE_DATA(inode);

	file->private_data = ac;

	return 0;
}

static ssize_t wcd_spi_ac_status_read(struct file *file,
		char __user *buffer,
		size_t count, loff_t *offset)
{
	struct wcd_spi_ac_priv *ac;
	char buf[WCD_SPI_AC_PFS_ENTRY_MAX_LEN];
	int len, ret;
	u8 offline;

	ac = (struct wcd_spi_ac_priv *) file->private_data;
	if (!ac) {
		pr_err("%s: Invalid private data for status\n",
			__func__);
		return -EINVAL;
	}

	WCD_SPI_AC_MUTEX_LOCK(ac->dev, ac->status_lock);
	offline = ac->svc_offline;
	/* Make sure the read is complete */
	rmb();
	dev_dbg(ac->dev, "%s: offline = %sline\n",
		__func__, offline ? "off" : "on");
	len = snprintf(buf, sizeof(buf), "%s\n",
		       offline ? "OFFLINE" : "ONLINE");
	ret = simple_read_from_buffer(buffer, count, offset, buf, len);
	WCD_SPI_AC_MUTEX_UNLOCK(ac->dev, ac->status_lock);

	return ret;
}

static unsigned int wcd_spi_ac_status_poll(struct file *file,
		poll_table *wait)
{
	struct wcd_spi_ac_priv *ac;
	unsigned int ret = 0;

	ac = (struct wcd_spi_ac_priv *) file->private_data;
	if (!ac) {
		pr_err("%s: Invalid private data for status\n",
			__func__);
		return -EINVAL;
	}

	dev_dbg(ac->dev, "%s: Poll wait, svc = %s\n",
		__func__, ac->svc_offline ? "offline" : "online");
	poll_wait(file, &ac->svc_poll_wait, wait);
	dev_dbg(ac->dev, "%s: Woken up Poll wait, svc = %s\n",
		__func__, ac->svc_offline ? "offline" : "online");

	WCD_SPI_AC_MUTEX_LOCK(ac->dev, ac->status_lock);
	if (xchg(&ac->svc_offline_change, 0))
		ret = POLLIN | POLLPRI | POLLRDNORM;
	dev_dbg(ac->dev, "%s: ret (%d) from poll_wait\n",
		__func__, ret);
	WCD_SPI_AC_MUTEX_UNLOCK(ac->dev, ac->status_lock);

	return ret;
}

static const struct file_operations wcd_spi_ac_status_ops = {
	.owner = THIS_MODULE,
	.open = wcd_spi_ac_status_open,
	.read = wcd_spi_ac_status_read,
	.poll = wcd_spi_ac_status_poll,
};

static int wcd_spi_ac_procfs_init(struct wcd_spi_ac_priv *ac)
{
	int ret = 0;

	ac->pfs_root = proc_mkdir(WCD_SPI_AC_PROCFS_DIR_NAME, NULL);
	if (!ac->pfs_root) {
		dev_err(ac->dev, "%s: proc_mkdir failed\n", __func__);
		return -EINVAL;
	}

	ac->pfs_status = proc_create_data(WCD_SPI_AC_PROCFS_STATE_NAME,
					  0444, ac->pfs_root,
					  &wcd_spi_ac_status_ops,
					  ac);
	if (!ac->pfs_status) {
		dev_err(ac->dev, "%s: proc_create_data failed\n",
			__func__);
		ret = -EINVAL;
		goto rmdir_root;
	}

	proc_set_size(ac->pfs_status, WCD_SPI_AC_PFS_ENTRY_MAX_LEN);

	return 0;

rmdir_root:
	proc_remove(ac->pfs_root);
	return ret;

}

static void wcd_spi_ac_procfs_deinit(struct wcd_spi_ac_priv *ac)
{
	proc_remove(ac->pfs_status);
	proc_remove(ac->pfs_root);
}

static int wcd_spi_ac_request_access(struct wcd_spi_ac_priv *ac)
{
	struct wcd_spi_req_access_msg_v01 req;
	struct wcd_spi_req_access_resp_v01 rsp;
	struct msg_desc req_desc, rsp_desc;
	int ret = 0;

	memset(&req, 0, sizeof(req));
	memset(&rsp, 0, sizeof(rsp));

	req.reason_valid = 1;
	req.reason = ac->state  & 0x03;

	req_desc.max_msg_len = WCD_SPI_REQ_ACCESS_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = WCD_SPI_REQ_ACCESS_MSG_V01;
	req_desc.ei_array = wcd_spi_req_access_msg_v01_ei;

	rsp_desc.max_msg_len = WCD_SPI_REQ_ACCESS_RESP_V01_MAX_MSG_LEN;
	rsp_desc.msg_id = WCD_SPI_REQ_ACCESS_RESP_V01;
	rsp_desc.ei_array = wcd_spi_req_access_resp_v01_ei;

	WCD_SPI_AC_MUTEX_LOCK(ac->dev, ac->msg_lock);

	ret = qmi_send_req_wait(ac->qmi_hdl,
				&req_desc, &req, sizeof(req),
				&rsp_desc, &rsp, sizeof(rsp),
				WCD_SPI_AC_QMI_TIMEOUT_MS);
	if (ret) {
		dev_err(ac->dev, "%s: msg send failed %d\n",
			__func__, ret);
		goto done;
	}

	if (rsp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ret = -EIO;
		dev_err(ac->dev, "%s: qmi resp error %d\n",
			__func__, rsp.resp.result);
	}
done:
	dev_dbg(ac->dev, "%s: status %d\n", __func__, ret);
	WCD_SPI_AC_MUTEX_UNLOCK(ac->dev, ac->msg_lock);

	return ret;
}

static int wcd_spi_ac_release_access(struct wcd_spi_ac_priv *ac)
{
	struct wcd_spi_rel_access_msg_v01 req;
	struct wcd_spi_rel_access_resp_v01 rsp;
	struct msg_desc req_desc, rsp_desc;
	int ret = 0;

	memset(&req, 0, sizeof(req));
	memset(&rsp, 0, sizeof(rsp));

	req_desc.max_msg_len = WCD_SPI_REL_ACCESS_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = WCD_SPI_REL_ACCESS_MSG_V01;
	req_desc.ei_array = wcd_spi_rel_access_msg_v01_ei;

	rsp_desc.max_msg_len = WCD_SPI_REL_ACCESS_RESP_V01_MAX_MSG_LEN;
	rsp_desc.msg_id = WCD_SPI_REL_ACCESS_RESP_V01;
	rsp_desc.ei_array = wcd_spi_rel_access_resp_v01_ei;

	WCD_SPI_AC_MUTEX_LOCK(ac->dev, ac->msg_lock);

	ret = qmi_send_req_wait(ac->qmi_hdl,
				&req_desc, &req, sizeof(req),
				&rsp_desc, &rsp, sizeof(rsp),
				WCD_SPI_AC_QMI_TIMEOUT_MS);
	if (ret) {
		dev_err(ac->dev, "%s: msg send failed %d\n",
			__func__, ret);
		goto done;
	}

	if (rsp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ret = -EIO;
		dev_err(ac->dev, "%s: qmi resp error %d\n",
			__func__, rsp.resp.result);
	}
done:
	dev_dbg(ac->dev, "%s: status %d\n", __func__, ret);
	WCD_SPI_AC_MUTEX_UNLOCK(ac->dev, ac->msg_lock);

	return ret;
}

static int wcd_spi_ac_buf_msg(
		struct wcd_spi_ac_priv *ac,
		u8 *data, int data_sz)
{
	struct wcd_spi_ac_buf_data *buf_data;
	struct wcd_spi_buff_msg_v01 req;
	struct wcd_spi_buff_resp_v01 rsp;
	struct msg_desc req_desc, rsp_desc;
	int ret = 0;

	memset(&req, 0, sizeof(req));
	memset(&rsp, 0, sizeof(rsp));

	buf_data = (struct wcd_spi_ac_buf_data *) data;
	memcpy(req.buff_addr_1, buf_data,
	       sizeof(*buf_data));

	if (data_sz - sizeof(*buf_data) != 0) {
		req.buff_addr_2_valid = 1;
		buf_data++;
		memcpy(req.buff_addr_2, buf_data,
		       sizeof(*buf_data));
	}

	req_desc.max_msg_len = WCD_SPI_BUFF_MSG_V01_MAX_MSG_LEN;
	req_desc.msg_id = WCD_SPI_BUFF_MSG_V01;
	req_desc.ei_array = wcd_spi_buff_msg_v01_ei;

	rsp_desc.max_msg_len = WCD_SPI_BUFF_RESP_V01_MAX_MSG_LEN;
	rsp_desc.msg_id = WCD_SPI_BUFF_RESP_V01;
	rsp_desc.ei_array = wcd_spi_buff_resp_v01_ei;

	WCD_SPI_AC_MUTEX_LOCK(ac->dev, ac->msg_lock);
	ret = qmi_send_req_wait(ac->qmi_hdl,
				&req_desc, &req, sizeof(req),
				&rsp_desc, &rsp, sizeof(rsp),
				WCD_SPI_AC_QMI_TIMEOUT_MS);

	if (ret) {
		dev_err(ac->dev, "%s: msg send failed %d\n",
			__func__, ret);
		goto done;
	}

	if (rsp.resp.result != QMI_RESULT_SUCCESS_V01) {
		ret = -EIO;
		dev_err(ac->dev, "%s: qmi resp error %d\n",
			__func__, rsp.resp.result);
	}
done:
	WCD_SPI_AC_MUTEX_UNLOCK(ac->dev, ac->msg_lock);
	return ret;

}

/*
 * wcd_spi_ac_set_sync: Sets the current status of the SPI
 *			bus and requests access if not
 *			already accesible.
 * @ac: pointer to the drivers private data
 * @value: value to be set in the status mask
 */
static int wcd_spi_ac_set_sync(struct wcd_spi_ac_priv *ac,
			       u32 value)
{
	int ret = 0;

	WCD_SPI_AC_MUTEX_LOCK(ac->dev, ac->state_lock);
	ac->state |= value;
	/* any non-zero state indicates us to request SPI access */
	wmb();
	dev_dbg(ac->dev,
		"%s: value 0x%x cur_state = 0x%x cur_access 0x%x\n",
		__func__, value, ac->state, ac->current_access);
	if (ac->current_access == WCD_SPI_AC_REMOTE_ACCESS) {
		if (value & WCD_SPI_AC_SVC_OFFLINE) {
			/*
			 * service exited while holding access.
			 * SPI access is blocked until service
			 * arrives again.
			 */
			ac->current_access = WCD_SPI_AC_NO_ACCESS;
			dev_err(ac->dev,
				"%s: svc exit while holding access\n",
				__func__);
			goto done;
		}

		dev_dbg(ac->dev,
			"%s: requesting access, state = 0x%x\n",
			__func__, ac->state);
		ret = wcd_spi_ac_request_access(ac);
		if (!ret)
			ac->current_access = WCD_SPI_AC_LOCAL_ACCESS;
	} else if (ac->current_access == WCD_SPI_AC_NO_ACCESS) {
		/*
		 * Access is lost due to service offline,
		 * Wait here until service is back online
		 */
		ret = wait_for_completion_timeout(&ac->online_compl,
					msecs_to_jiffies(3000));
		if (!ret) {
			dev_err(ac->dev,
				"%s: svc_online timedout\n", __func__);
			ret = -ETIMEDOUT;
			goto done;
		}

		/*
		 * service is now online, current access is expected
		 * to be local at this time. check to make sure.
		 */
		if (ac->current_access != WCD_SPI_AC_LOCAL_ACCESS) {
			dev_err(ac->dev,
				"%s: wait complete, access 0x%x not local\n",
				__func__, ac->current_access);
			ret = -EIO;
			goto done;
		}

		/* Explicity assign return to 0, as access is local */
		ret = 0;
	}
done:
	WCD_SPI_AC_MUTEX_UNLOCK(ac->dev, ac->state_lock);
	return ret;
}

/*
 * wcd_spi_ac_clear_sync: Clears the current status of the SPI
 *			  bus and releases access if applicable
 * @ac: pointer to the drivers private data
 * @value: value to be cleared in the status mask
 */
static int wcd_spi_ac_clear_sync(struct wcd_spi_ac_priv *ac,
				 u32 value)
{
	int ret = 0;

	WCD_SPI_AC_MUTEX_LOCK(ac->dev, ac->state_lock);
	ac->state &= ~(value);
	/* make sure value is written before read */
	wmb();
	dev_dbg(ac->dev, "%s: current state = 0x%x, current access 0x%x\n",
		__func__, ac->state, ac->current_access);
	/* state should be zero to release SPI access */
	if (!ac->state &&
	    ac->current_access == WCD_SPI_AC_LOCAL_ACCESS) {
		dev_dbg(ac->dev,
			"%s: releasing access, state = 0x%x\n",
			__func__, ac->state);
		ret = wcd_spi_ac_release_access(ac);
		if (!ret)
			ac->current_access = WCD_SPI_AC_REMOTE_ACCESS;
	}
	WCD_SPI_AC_MUTEX_UNLOCK(ac->dev, ac->state_lock);

	return ret;

}

/*
 * wcd_spi_access_ctl: API to request/release the access
 *		       to wcd-spi bus.
 * @dev: handle to the wcd-spi-ac device
 * @request: enum to indicate access request or access release
 * @reason: reason for request/release. Must be one of the
 *	    valid reasons.
 * Returns success if the access handover was sucessful,
 * negative error code otherwise.
 */
int wcd_spi_access_ctl(struct device *dev,
		      enum wcd_spi_acc_req request,
		      u32 reason)
{
	struct wcd_spi_ac_priv *ac;
	int ret = 0;

	if (!dev) {
		pr_err("%s: invalid device\n", __func__);
		return -EINVAL;
	}

	/* only data_transfer and remote_down are valid reasons */
	if (reason != WCD_SPI_AC_DATA_TRANSFER &&
	    reason != WCD_SPI_AC_REMOTE_DOWN) {
		pr_err("%s: Invalid reason 0x%x\n",
			__func__, reason);
		return -EINVAL;
	}

	ac = (struct wcd_spi_ac_priv *) dev_get_drvdata(dev);
	if (!ac) {
		dev_err(dev, "%s: invalid driver data\n", __func__);
		return -EINVAL;
	}

	dev_dbg(dev, "%s: request = 0x%x, reason = 0x%x\n",
		__func__, request, reason);

	switch (request) {
	case WCD_SPI_ACCESS_REQUEST:
		ret = wcd_spi_ac_set_sync(ac, reason);
		if (ret)
			dev_err(dev, "%s: set_sync(0x%x) failed %d\n",
				__func__, reason, ret);
		break;
	case WCD_SPI_ACCESS_RELEASE:
		ret = wcd_spi_ac_clear_sync(ac, reason);
		if (ret)
			dev_err(dev, "%s: clear_sync(0x%x) failed %d\n",
				__func__, reason, ret);
		break;
	default:
		dev_err(dev, "%s: invalid request 0x%x\n",
			__func__, request);
		break;
	}

	return ret;
}
EXPORT_SYMBOL(wcd_spi_access_ctl);

static int wcd_spi_ac_cdev_open(struct inode *inode,
				struct file *file)
{
	struct wcd_spi_ac_priv *ac;
	int ret = 0;

	ac = container_of(inode->i_cdev, struct wcd_spi_ac_priv, cdev);
	if (!ac) {
		pr_err("%s: Invalid private data\n", __func__);
		return -EINVAL;
	}

	WCD_SPI_AC_MUTEX_LOCK(ac->dev, ac->status_lock);
	if (ac->svc_offline) {
		dev_err(ac->dev, "%s: SVC is not online, cannot open driver\n",
			__func__);
		ret = -ENODEV;
		goto done;
	}

	file->private_data = ac;

done:
	WCD_SPI_AC_MUTEX_UNLOCK(ac->dev, ac->status_lock);
	return ret;
}

static ssize_t wcd_spi_ac_cdev_write(struct file *file,
				     const char __user *buf,
				     size_t count,
				     loff_t *ppos)
{
	struct wcd_spi_ac_priv *ac;
	struct wcd_spi_ac_write_cmd *cmd_buf;
	int ret = 0;
	int data_sz;

	ac = (struct wcd_spi_ac_priv *) file->private_data;
	if (!ac) {
		pr_err("%s: Invalid private data\n", __func__);
		return -EINVAL;
	}

	if (count < WCD_SPI_AC_WRITE_CMD_MIN_SIZE ||
	    count > WCD_SPI_AC_WRITE_CMD_MAX_SIZE) {
		dev_err(ac->dev, "%s: Invalid write count %zd\n",
			__func__, count);
		return -EINVAL;
	}

	cmd_buf = kzalloc(count, GFP_KERNEL);
	if (!cmd_buf)
		return -ENOMEM;

	if (get_user(cmd_buf->cmd_type, buf)) {
		dev_err(ac->dev, "%s: get_user failed\n", __func__);
		ret = -EFAULT;
		goto free_cmd_buf;
	}

	dev_dbg(ac->dev, "%s: write cmd type 0x%x\n",
		__func__, cmd_buf->cmd_type);

	switch (cmd_buf->cmd_type) {

	case WCD_SPI_AC_CMD_CONC_BEGIN:
		ret = wcd_spi_ac_set_sync(ac, WCD_SPI_AC_CONCURRENCY);
		if (ret) {
			dev_err(ac->dev, "%s: set_sync(CONC) fail %d\n",
				__func__, ret);
			goto free_cmd_buf;
		}

		break;

	case WCD_SPI_AC_CMD_CONC_END:
		ret = wcd_spi_ac_clear_sync(ac, WCD_SPI_AC_CONCURRENCY);
		if (ret) {
			dev_err(ac->dev, "%s: clear_sync(CONC) fail %d\n",
				__func__, ret);
			goto free_cmd_buf;
		}

		break;

	case WCD_SPI_AC_CMD_BUF_DATA:

		/* Read the buffer details and send to service */
		data_sz = count - sizeof(cmd_buf->cmd_type);

		if (!data_sz ||
		    (data_sz % sizeof(struct wcd_spi_ac_buf_data))) {
			dev_err(ac->dev, "%s: size %d not multiple of %ld\n",
				__func__, data_sz,
				sizeof(struct wcd_spi_ac_buf_data));
			goto free_cmd_buf;
		}

		if (data_sz / sizeof(struct wcd_spi_ac_buf_data) >
		    WCD_SPI_AC_MAX_BUFFERS) {
			dev_err(ac->dev, "%s: invalid size %d\n",
				__func__, data_sz);
			goto free_cmd_buf;
		}

		if (copy_from_user(cmd_buf->payload,
				   buf + sizeof(cmd_buf->cmd_type),
				   data_sz)) {
			dev_err(ac->dev, "%s: copy_from_user failed\n",
				__func__);
			ret = -EFAULT;
			goto free_cmd_buf;
		}

		ret = wcd_spi_ac_buf_msg(ac, cmd_buf->payload, data_sz);
		if (ret) {
			dev_err(ac->dev, "%s: _buf_msg failed %d\n",
				__func__, ret);
			goto free_cmd_buf;
		}

		ret = wcd_spi_ac_clear_sync(ac, WCD_SPI_AC_UNINITIALIZED);
		if (ret) {
			dev_err(ac->dev, "%s: clear_sync 0x%lx failed %d\n",
				__func__, WCD_SPI_AC_UNINITIALIZED, ret);
			goto free_cmd_buf;
		}
		break;
	default:
		dev_err(ac->dev, "%s: Invalid cmd_type 0x%x\n",
			__func__, cmd_buf->cmd_type);
		ret = -EINVAL;
		goto free_cmd_buf;
	}

free_cmd_buf:

	kfree(cmd_buf);
	if (!ret)
		ret = count;

	return ret;
}

static int wcd_spi_ac_cdev_release(struct inode *inode,
				   struct file *file)
{
	struct wcd_spi_ac_priv *ac;
	int ret = 0;

	ac = (struct wcd_spi_ac_priv *) file->private_data;
	if (!ac) {
		pr_err("%s: Invalid private data\n", __func__);
		return -EINVAL;
	}

	ret = wcd_spi_ac_set_sync(ac, WCD_SPI_AC_UNINITIALIZED);
	if (ret)
		dev_err(ac->dev, "%s: set_sync(UNINITIALIZED) failed %d\n",
			__func__, ret);
	return ret;
}

static const struct file_operations wcd_spi_ac_cdev_fops = {
	.owner = THIS_MODULE,
	.open = wcd_spi_ac_cdev_open,
	.write = wcd_spi_ac_cdev_write,
	.release = wcd_spi_ac_cdev_release,
};

static int wcd_spi_ac_reg_chardev(struct wcd_spi_ac_priv *ac)
{
	int ret;

	ret = alloc_chrdev_region(&ac->cdev_num, 0, 1,
				  WCD_SPI_AC_CLIENT_CDEV_NAME);
	if (ret) {
		dev_err(ac->dev, "%s: alloc_chrdev_region failed %d\n",
			__func__, ret);
		return ret;
	}

	ac->cls = class_create(THIS_MODULE, WCD_SPI_AC_CLIENT_CDEV_NAME);
	if (IS_ERR(ac->cls)) {
		ret = PTR_ERR(ac->cls);
		dev_err(ac->dev, "%s: class_create failed %d\n",
			__func__, ret);
		goto unregister_chrdev;
	}

	ac->chardev = device_create(ac->cls, NULL, ac->cdev_num,
				      NULL, WCD_SPI_AC_CLIENT_CDEV_NAME);
	if (IS_ERR(ac->chardev)) {
		ret = PTR_ERR(ac->chardev);
		dev_err(ac->dev, "%s: device_create failed %d\n",
			__func__, ret);
		goto destroy_class;
	}

	cdev_init(&ac->cdev, &wcd_spi_ac_cdev_fops);
	ret = cdev_add(&ac->cdev, ac->cdev_num, 1);
	if (ret) {
		dev_err(ac->dev, "%s: cdev_add failed %d\n",
			__func__, ret);
		goto destroy_device;
	}

	return 0;

destroy_device:
	device_destroy(ac->cls, ac->cdev_num);

destroy_class:
	class_destroy(ac->cls);

unregister_chrdev:
	unregister_chrdev_region(0, 1);
	return ret;
}

static int wcd_spi_ac_unreg_chardev(struct wcd_spi_ac_priv *ac)
{
	cdev_del(&ac->cdev);
	device_destroy(ac->cls, ac->cdev_num);
	class_destroy(ac->cls);
	unregister_chrdev_region(0, 1);

	return 0;
}

static void wcd_spi_ac_recv_msg(struct work_struct *work)
{
	struct wcd_spi_ac_priv *ac;
	int rc = 0;

	ac = container_of(work, struct wcd_spi_ac_priv,
			  recv_msg_work);
	if (!ac) {
		pr_err("%s: Invalid private data\n", __func__);
		return;
	}

	do {
		dev_dbg(ac->dev, "%s: msg received, rc = %d\n",
			__func__, rc);
	} while ((rc = qmi_recv_msg(ac->qmi_hdl)) == 0);

	if (rc != -ENOMSG)
		dev_err(ac->dev, "%s: qmi_recv_msg failed %d\n",
			__func__, rc);
}

static void wcd_spi_ac_clnt_notify(struct qmi_handle *hdl,
		enum qmi_event_type event, void *priv_data)
{
	struct wcd_spi_ac_priv *ac;

	if (!priv_data) {
		pr_err("%s: Invalid private data\n", __func__);
		return;
	}

	ac = (struct wcd_spi_ac_priv *) priv_data;

	switch (event) {
	case QMI_RECV_MSG:
		queue_work(ac->qmi_wq, &ac->recv_msg_work);
		break;
	default:
		break;
	}
}

static void wcd_spi_ac_svc_arrive(struct work_struct *work)
{
	struct wcd_spi_ac_priv *ac;
	int ret;

	ac = container_of(work, struct wcd_spi_ac_priv,
			  svc_arr_work);
	if (!ac) {
		pr_err("%s: Invalid private data\n",
			__func__);
		return;
	}

	WCD_SPI_AC_MUTEX_LOCK(ac->dev, ac->event_lock);
	ac->qmi_hdl = qmi_handle_create(wcd_spi_ac_clnt_notify,
					ac);
	if (!ac->qmi_hdl) {
		dev_err(ac->dev, "%s: qmi_handle_create failed\n",
			__func__);
		goto done;
	}

	ret = qmi_connect_to_service(ac->qmi_hdl,
			WCD_SPI_CTL_SERVICE_ID_V01,
			WCD_SPI_CTL_SERVICE_VERS_V01,
			WCD_SPI_CTL_INS_ID);
	if (ret) {
		dev_err(ac->dev, "%s, cant connect to service, error %d\n",
			__func__, ret);
		qmi_handle_destroy(ac->qmi_hdl);
		ac->qmi_hdl = NULL;
		goto done;
	}

	/* Notify service availability */
	ac->current_access = WCD_SPI_AC_LOCAL_ACCESS;
	complete(&ac->online_compl);
	/*
	 * update the state and clear the WCD_SPI_AC_SVC_OFFLINE
	 * bit to indicate that the service is now online.
	 */
	ret = wcd_spi_ac_clear_sync(ac, WCD_SPI_AC_SVC_OFFLINE);
	if (ret)
		dev_err(ac->dev, "%s: clear_sync(SVC_OFFLINE) failed %d\n",
			__func__, ret);

	/* Mark service as online */
	wcd_spi_ac_status_change(ac, 1);
done:
	WCD_SPI_AC_MUTEX_UNLOCK(ac->dev, ac->event_lock);

}

static void wcd_spi_ac_svc_exit(struct work_struct *work)
{
	struct wcd_spi_ac_priv *ac;
	int ret = 0;

	ac = container_of(work, struct wcd_spi_ac_priv,
			  svc_exit_work);
	if (!ac) {
		pr_err("%s: Invalid private data\n",
			__func__);
		return;
	}

	WCD_SPI_AC_MUTEX_LOCK(ac->dev, ac->event_lock);
	reinit_completion(&ac->online_compl);
	ret = wcd_spi_ac_set_sync(ac, WCD_SPI_AC_SVC_OFFLINE |
				      WCD_SPI_AC_UNINITIALIZED);
	if (ret)
		dev_err(ac->dev, "%s: set_sync(SVC_OFFLINE) failed %d\n",
			__func__, ret);
	qmi_handle_destroy(ac->qmi_hdl);
	ac->qmi_hdl = NULL;
	wcd_spi_ac_status_change(ac, 0);
	WCD_SPI_AC_MUTEX_UNLOCK(ac->dev, ac->event_lock);
}

static int wcd_spi_ac_svc_event(struct notifier_block *this,
				unsigned long event,
				void *data)
{
	struct wcd_spi_ac_priv *ac;

	ac = container_of(this, struct wcd_spi_ac_priv, nb);
	if (!ac) {
		pr_err("%s: Invalid private data\n", __func__);
		return -EINVAL;
	}

	dev_dbg(ac->dev, "%s: event = 0x%lx", __func__, event);

	switch (event) {
	case QMI_SERVER_ARRIVE:
		schedule_work(&ac->svc_arr_work);
		break;
	case QMI_SERVER_EXIT:
		schedule_work(&ac->svc_exit_work);
		break;
	default:
		dev_err(ac->dev, "%s unhandled event %ld\n",
			__func__, event);
		break;
	}

	return 0;
}

static int wcd_spi_ac_probe(struct platform_device *pdev)
{
	struct wcd_spi_ac_priv *ac;
	struct device *parent = pdev->dev.parent;
	int ret = 0;

	ac = devm_kzalloc(&pdev->dev, sizeof(*ac),
			    GFP_KERNEL);
	if (!ac)
		return -ENOMEM;

	ac->dev = &pdev->dev;
	ac->parent = parent;

	ret = wcd_spi_ac_reg_chardev(ac);
	if (ret)
		return ret;

	ret = wcd_spi_ac_procfs_init(ac);
	if (ret)
		goto unreg_chardev;

	mutex_init(&ac->status_lock);
	mutex_init(&ac->state_lock);
	mutex_init(&ac->msg_lock);
	mutex_init(&ac->event_lock);
	init_completion(&ac->online_compl);
	init_waitqueue_head(&ac->svc_poll_wait);
	ac->svc_offline = 1;
	ac->state = (WCD_SPI_AC_SVC_OFFLINE |
		     WCD_SPI_AC_UNINITIALIZED);
	ac->current_access = WCD_SPI_AC_LOCAL_ACCESS;

	ac->nb.notifier_call = wcd_spi_ac_svc_event;
	INIT_WORK(&ac->svc_arr_work, wcd_spi_ac_svc_arrive);
	INIT_WORK(&ac->svc_exit_work, wcd_spi_ac_svc_exit);
	INIT_WORK(&ac->recv_msg_work, wcd_spi_ac_recv_msg);

	ac->qmi_wq = create_singlethread_workqueue("qmi_wq");
	if (!ac->qmi_wq) {
		dev_err(&pdev->dev,
			"%s: create_singlethread_workqueue failed\n",
			__func__);
		goto deinit_procfs;
	}

	dev_set_drvdata(&pdev->dev, ac);

	ret = qmi_svc_event_notifier_register(
			WCD_SPI_CTL_SERVICE_ID_V01,
			WCD_SPI_CTL_SERVICE_VERS_V01,
			WCD_SPI_CTL_INS_ID,
			&ac->nb);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: qmi_svc_event_notifier_register failed %d\n",
			__func__, ret);
		goto destroy_wq;
	}


	return 0;

destroy_wq:
	destroy_workqueue(ac->qmi_wq);
	dev_set_drvdata(&pdev->dev, NULL);
deinit_procfs:
	wcd_spi_ac_procfs_deinit(ac);
	mutex_destroy(&ac->status_lock);
	mutex_destroy(&ac->state_lock);
	mutex_destroy(&ac->msg_lock);
	mutex_destroy(&ac->event_lock);
unreg_chardev:
	wcd_spi_ac_unreg_chardev(ac);
	return ret;
}

static int wcd_spi_ac_remove(struct platform_device *pdev)
{
	struct wcd_spi_ac_priv *ac;

	ac = dev_get_drvdata(&pdev->dev);
	qmi_svc_event_notifier_unregister(
			WCD_SPI_CTL_SERVICE_ID_V01,
			WCD_SPI_CTL_SERVICE_VERS_V01,
			WCD_SPI_CTL_INS_ID,
			&ac->nb);
	if (ac->qmi_wq)
		destroy_workqueue(ac->qmi_wq);
	wcd_spi_ac_unreg_chardev(ac);
	wcd_spi_ac_procfs_deinit(ac);
	mutex_destroy(&ac->status_lock);
	mutex_destroy(&ac->state_lock);
	mutex_destroy(&ac->msg_lock);
	mutex_destroy(&ac->event_lock);

	return 0;
}

static const struct of_device_id wcd_spi_ac_of_match[] = {
	{ .compatible = "qcom,wcd-spi-ac" },
	{ },
};

MODULE_DEVICE_TABLE(of, wcd_spi_ac_of_match);

static struct platform_driver wcd_spi_ac_driver = {
	.driver = {
		.name = "qcom,wcd-spi-ac",
		.of_match_table = wcd_spi_ac_of_match,
		.suppress_bind_attrs = true,
	},
	.probe = wcd_spi_ac_probe,
	.remove = wcd_spi_ac_remove,
};

module_platform_driver(wcd_spi_ac_driver);

MODULE_DESCRIPTION("WCD SPI access control driver");
MODULE_LICENSE("GPL v2");
