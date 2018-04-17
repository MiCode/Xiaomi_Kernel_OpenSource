/* Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
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
#define pr_fmt(fmt) "CAM-BUFMGR %s:%d " fmt, __func__, __LINE__

#include "msm_generic_buf_mgr.h"

static struct msm_buf_mngr_device *msm_buf_mngr_dev;

struct v4l2_subdev *msm_buf_mngr_get_subdev(void)
{
	return &msm_buf_mngr_dev->subdev.sd;
}

static int32_t msm_buf_mngr_hdl_cont_get_buf(struct msm_buf_mngr_device *dev,
	struct msm_buf_mngr_info *buf_info)
{
	unsigned int i;
	struct msm_buf_mngr_user_buf_cont_info *cbuf, *cont_save;

	list_for_each_entry_safe(cbuf, cont_save, &dev->cont_qhead, entry) {
		if ((cbuf->sessid == buf_info->session_id) &&
		(cbuf->index == buf_info->index) &&
		(cbuf->strid == buf_info->stream_id)) {
			buf_info->user_buf.buf_cnt = cbuf->paddr->buf_cnt;
			if (buf_info->user_buf.buf_cnt >
				MSM_CAMERA_MAX_USER_BUFF_CNT) {
				pr_err("Invalid cnt%d,%d,%d\n",
					cbuf->paddr->buf_cnt,
					buf_info->session_id,
					buf_info->stream_id);
				return -EINVAL;
			}
			for (i = 0 ; i < buf_info->user_buf.buf_cnt; i++) {
				buf_info->user_buf.buf_idx[i] =
					cbuf->paddr->buf_idx[i];
			}
			break;
		}
	}
	return 0;
}

static int32_t msm_buf_mngr_get_buf(struct msm_buf_mngr_device *dev,
	void __user *argp)
{
	unsigned long flags;
	int32_t rc = 0;
	struct msm_buf_mngr_info *buf_info =
		(struct msm_buf_mngr_info *)argp;
	struct msm_get_bufs *new_entry =
		kzalloc(sizeof(struct msm_get_bufs), GFP_KERNEL);

	if (!new_entry) {
		pr_err("%s:No mem\n", __func__);
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&new_entry->entry);
	new_entry->vb2_v4l2_buf = dev->vb2_ops.get_buf(buf_info->session_id,
		buf_info->stream_id);
	if (!new_entry->vb2_v4l2_buf) {
		pr_debug("%s:Get buf is null\n", __func__);
		kfree(new_entry);
		return -EINVAL;
	}
	new_entry->session_id = buf_info->session_id;
	new_entry->stream_id = buf_info->stream_id;
	new_entry->index = new_entry->vb2_v4l2_buf->vb2_buf.index;
	spin_lock_irqsave(&dev->buf_q_spinlock, flags);
	list_add_tail(&new_entry->entry, &dev->buf_qhead);
	spin_unlock_irqrestore(&dev->buf_q_spinlock, flags);
	buf_info->index = new_entry->vb2_v4l2_buf->vb2_buf.index;
	if (buf_info->type == MSM_CAMERA_BUF_MNGR_BUF_USER) {
		mutex_lock(&dev->cont_mutex);
		if (!list_empty(&dev->cont_qhead)) {
			rc = msm_buf_mngr_hdl_cont_get_buf(dev, buf_info);
		} else {
			pr_err("Nothing mapped in user buf for %d,%d\n",
				buf_info->session_id, buf_info->stream_id);
			rc = -EINVAL;
		}
		mutex_unlock(&dev->cont_mutex);
	}
	return rc;
}

static int32_t msm_buf_mngr_get_buf_by_idx(struct msm_buf_mngr_device *dev,
	void *argp)
{
	unsigned long flags;
	int32_t rc = 0;
	struct msm_buf_mngr_info *buf_info =
		(struct msm_buf_mngr_info *)argp;
	struct msm_get_bufs *new_entry =
		kzalloc(sizeof(struct msm_get_bufs), GFP_KERNEL);

	if (!new_entry)
		return -ENOMEM;

	if (!buf_info) {
		kfree(new_entry);
		return -EIO;
	}

	INIT_LIST_HEAD(&new_entry->entry);
	new_entry->vb2_v4l2_buf = dev->vb2_ops.get_buf_by_idx(
		buf_info->session_id, buf_info->stream_id, buf_info->index);
	if (!new_entry->vb2_v4l2_buf) {
		pr_debug("%s:Get buf is null\n", __func__);
		kfree(new_entry);
		return -EINVAL;
	}
	new_entry->session_id = buf_info->session_id;
	new_entry->stream_id = buf_info->stream_id;
	new_entry->index = new_entry->vb2_v4l2_buf->vb2_buf.index;
	spin_lock_irqsave(&dev->buf_q_spinlock, flags);
	list_add_tail(&new_entry->entry, &dev->buf_qhead);
	spin_unlock_irqrestore(&dev->buf_q_spinlock, flags);
	if (buf_info->type == MSM_CAMERA_BUF_MNGR_BUF_USER) {
		mutex_lock(&dev->cont_mutex);
		if (!list_empty(&dev->cont_qhead)) {
			rc = msm_buf_mngr_hdl_cont_get_buf(dev, buf_info);
		} else {
			pr_err("Nothing mapped in user buf for %d,%d\n",
				buf_info->session_id, buf_info->stream_id);
			rc = -EINVAL;
		}
		mutex_unlock(&dev->cont_mutex);
	}
	return rc;
}

static int32_t msm_buf_mngr_buf_done(struct msm_buf_mngr_device *buf_mngr_dev,
	struct msm_buf_mngr_info *buf_info)
{
	unsigned long flags;
	struct msm_get_bufs *bufs, *save;
	int32_t ret = -EINVAL;

	spin_lock_irqsave(&buf_mngr_dev->buf_q_spinlock, flags);
	list_for_each_entry_safe(bufs, save, &buf_mngr_dev->buf_qhead, entry) {
		if ((bufs->session_id == buf_info->session_id) &&
			(bufs->stream_id == buf_info->stream_id) &&
			(bufs->index == buf_info->index)) {
			ret = buf_mngr_dev->vb2_ops.buf_done
					(bufs->vb2_v4l2_buf,
						buf_info->session_id,
						buf_info->stream_id,
						buf_info->frame_id,
						&buf_info->timestamp,
						buf_info->reserved);
			list_del_init(&bufs->entry);
			kfree(bufs);
			break;
		}
	}
	spin_unlock_irqrestore(&buf_mngr_dev->buf_q_spinlock, flags);
	return ret;
}

static int32_t msm_buf_mngr_buf_error(struct msm_buf_mngr_device *buf_mngr_dev,
	struct msm_buf_mngr_info *buf_info)
{
	unsigned long flags;
	struct msm_get_bufs *bufs, *save;
	int32_t ret = -EINVAL;

	spin_lock_irqsave(&buf_mngr_dev->buf_q_spinlock, flags);
	list_for_each_entry_safe(bufs, save, &buf_mngr_dev->buf_qhead, entry) {
		if ((bufs->session_id == buf_info->session_id) &&
			(bufs->stream_id == buf_info->stream_id) &&
			(bufs->index == buf_info->index)) {
			ret = buf_mngr_dev->vb2_ops.buf_error
					(bufs->vb2_v4l2_buf,
						buf_info->session_id,
						buf_info->stream_id,
						buf_info->frame_id,
						&buf_info->timestamp,
						buf_info->reserved);
			list_del_init(&bufs->entry);
			kfree(bufs);
			break;
		}
	}
	spin_unlock_irqrestore(&buf_mngr_dev->buf_q_spinlock, flags);
	return ret;
}

static int32_t msm_buf_mngr_put_buf(struct msm_buf_mngr_device *buf_mngr_dev,
	struct msm_buf_mngr_info *buf_info)
{
	unsigned long flags;
	struct msm_get_bufs *bufs, *save;
	int32_t ret = -EINVAL;

	spin_lock_irqsave(&buf_mngr_dev->buf_q_spinlock, flags);
	list_for_each_entry_safe(bufs, save, &buf_mngr_dev->buf_qhead, entry) {
		if ((bufs->session_id == buf_info->session_id) &&
			(bufs->stream_id == buf_info->stream_id) &&
			(bufs->index == buf_info->index)) {
			ret = buf_mngr_dev->vb2_ops.put_buf(bufs->vb2_v4l2_buf,
				buf_info->session_id, buf_info->stream_id);
			list_del_init(&bufs->entry);
			kfree(bufs);
			break;
		}
	}
	spin_unlock_irqrestore(&buf_mngr_dev->buf_q_spinlock, flags);
	return ret;
}

static int32_t msm_generic_buf_mngr_flush(
	struct msm_buf_mngr_device *buf_mngr_dev,
	struct msm_buf_mngr_info *buf_info)
{
	unsigned long flags;
	struct msm_get_bufs *bufs, *save;
	int32_t ret = -EINVAL;
	struct timeval ts;

	spin_lock_irqsave(&buf_mngr_dev->buf_q_spinlock, flags);
	/*
	 * Sanity check on client buf list, remove buf mgr
	 * queue entries in case any
	 */
	list_for_each_entry_safe(bufs, save, &buf_mngr_dev->buf_qhead, entry) {
		if ((bufs->session_id == buf_info->session_id) &&
			(bufs->stream_id == buf_info->stream_id)) {
			ret = buf_mngr_dev->vb2_ops.buf_done(bufs->vb2_v4l2_buf,
						buf_info->session_id,
						buf_info->stream_id, 0, &ts, 0);
			pr_err("Bufs not flushed: str_id = %d buf_index = %d ret = %d\n",
			buf_info->stream_id, bufs->index,
			ret);
			list_del_init(&bufs->entry);
			kfree(bufs);
		}
	}
	spin_unlock_irqrestore(&buf_mngr_dev->buf_q_spinlock, flags);
	/* Flush the remaining vb2 buffers in stream list */
	ret = buf_mngr_dev->vb2_ops.flush_buf(buf_info->session_id,
			buf_info->stream_id);
	return ret;
}

static int32_t msm_buf_mngr_find_cont_stream(struct msm_buf_mngr_device *dev,
					     uint32_t *cnt, uint32_t *tstream,
					     struct msm_sd_close_ioctl *session)
{
	struct msm_buf_mngr_user_buf_cont_info *cont_bufs, *cont_save;
	int32_t ret = -1;

	list_for_each_entry_safe(cont_bufs,
		cont_save, &dev->cont_qhead, entry) {
		if (cont_bufs->sessid == session->session) {
			*cnt = cont_bufs->cnt;
			*tstream = cont_bufs->strid;
			return 0;
		}
	}
	return ret;
}

static void msm_buf_mngr_contq_listdel(struct msm_buf_mngr_device *dev,
				     uint32_t session, int32_t stream,
				     bool unmap, uint32_t cnt)
{
	struct msm_buf_mngr_user_buf_cont_info *cont_bufs, *cont_save;

	list_for_each_entry_safe(cont_bufs,
		cont_save, &dev->cont_qhead, entry) {
		if ((cont_bufs->sessid == session) &&
		(cont_bufs->strid == stream)) {
			if (cnt == 1 && unmap == 1) {
				ion_unmap_kernel(dev->ion_client,
					cont_bufs->ion_handle);
				ion_free(dev->ion_client,
					cont_bufs->ion_handle);
			}
			list_del_init(&cont_bufs->entry);
			kfree(cont_bufs);
			cnt--;
		}
	}
	if (cnt != 0)
		pr_err("Buffers pending cnt = %d\n", cnt);
}

static void msm_buf_mngr_contq_cleanup(struct msm_buf_mngr_device *dev,
				     struct msm_sd_close_ioctl *session)
{
	int32_t stream = -1, found = -1;
	uint32_t cnt = 0;

	do {
		found = msm_buf_mngr_find_cont_stream(dev, &cnt,
			&stream, session);
		if (found == -1)
			break;
		msm_buf_mngr_contq_listdel(dev, session->session,
			stream, 1, cnt);
	} while (found == 0);
}

static void msm_buf_mngr_sd_shutdown(struct msm_buf_mngr_device *dev,
				     struct msm_sd_close_ioctl *session)
{
	unsigned long flags;
	struct msm_get_bufs *bufs, *save;

	BUG_ON(!dev);
	BUG_ON(!session);

	spin_lock_irqsave(&dev->buf_q_spinlock, flags);
	if (!list_empty(&dev->buf_qhead)) {
		list_for_each_entry_safe(bufs,
			save, &dev->buf_qhead, entry) {
			pr_info("%s: Delete invalid bufs =%pK, session_id=%u, bufs->ses_id=%d, str_id=%d, idx=%d\n",
				__func__, (void *)bufs, session->session,
				bufs->session_id, bufs->stream_id,
				bufs->index);
			if (session->session == bufs->session_id) {
				list_del_init(&bufs->entry);
				kfree(bufs);
			}
		}
	}
	spin_unlock_irqrestore(&dev->buf_q_spinlock, flags);
	mutex_lock(&dev->cont_mutex);
	if (!list_empty(&dev->cont_qhead))
		msm_buf_mngr_contq_cleanup(dev, session);
	mutex_unlock(&dev->cont_mutex);
}

static int msm_buf_mngr_handle_cont_cmd(struct msm_buf_mngr_device *dev,
					struct msm_buf_mngr_main_cont_info
					*cont_cmd)
{
	int rc = 0, i = 0;
	struct ion_handle *ion_handle = NULL;
	struct msm_camera_user_buf_cont_t *iaddr, *temp_addr;
	struct msm_buf_mngr_user_buf_cont_info *new_entry, *bufs, *save;
	size_t size;

	if ((cont_cmd->cmd >= MSM_CAMERA_BUF_MNGR_CONT_MAX) ||
		(cont_cmd->cmd < 0) ||
		(cont_cmd->cnt > VB2_MAX_FRAME) ||
		(cont_cmd->cont_fd < 0)) {
		pr_debug("Invalid arg passed Cmd:%d, cnt:%d, fd:%d\n",
			cont_cmd->cmd, cont_cmd->cnt,
			cont_cmd->cont_fd);
		return -EINVAL;
	}

	mutex_lock(&dev->cont_mutex);

	if (cont_cmd->cmd == MSM_CAMERA_BUF_MNGR_CONT_MAP) {
		if (!list_empty(&dev->cont_qhead)) {
			list_for_each_entry_safe(bufs,
				save, &dev->cont_qhead, entry) {
				if ((bufs->sessid == cont_cmd->session_id) &&
				(bufs->strid == cont_cmd->stream_id)) {
					pr_err("Map exist %d,%d unmap first\n",
						cont_cmd->session_id,
						cont_cmd->stream_id);
					rc = -EINVAL;
					goto end;
				}
			}
		}
		ion_handle = ion_import_dma_buf(dev->ion_client,
				cont_cmd->cont_fd);
		if (IS_ERR_OR_NULL(ion_handle)) {
			pr_err("Failed to create ion handle for fd %d\n",
				cont_cmd->cont_fd);
			rc = -EINVAL;
			goto end;
		}
		if (ion_handle_get_size(dev->ion_client,
			ion_handle, &size) < 0) {
			pr_err("Get ion size failed\n");
			rc = -EINVAL;
			goto free_ion_handle;
		}
		if ((size == 0) || (size <
			(sizeof(struct msm_camera_user_buf_cont_t) *
			cont_cmd->cnt))) {
			pr_err("Invalid or zero size ION buffer %zu\n", size);
			rc = -EINVAL;
			goto free_ion_handle;
		}
		iaddr = ion_map_kernel(dev->ion_client, ion_handle);
		if (IS_ERR_OR_NULL(iaddr)) {
			pr_err("Mapping cont buff failed\n");
			rc = -EINVAL;
			goto free_ion_handle;
		}
		for (i = 0; i < cont_cmd->cnt; i++) {
			temp_addr = iaddr + i;
			if (temp_addr->buf_cnt >
				MSM_CAMERA_MAX_USER_BUFF_CNT) {
				pr_err("%s:Invalid buf_cnt:%d for cont:%d\n",
					__func__, temp_addr->buf_cnt, i);
				rc = -EINVAL;
				goto free_list;
			}
			new_entry = kzalloc(sizeof(
				struct msm_buf_mngr_user_buf_cont_info),
				GFP_KERNEL);
			if (!new_entry) {
				pr_err("%s:No mem\n", __func__);
				rc = -ENOMEM;
				goto free_list;
			}
			INIT_LIST_HEAD(&new_entry->entry);
			new_entry->sessid = cont_cmd->session_id;
			new_entry->strid = cont_cmd->stream_id;
			new_entry->index = i;
			new_entry->main_fd = cont_cmd->cont_fd;
			new_entry->ion_handle = ion_handle;
			new_entry->cnt = cont_cmd->cnt;
			new_entry->paddr = temp_addr;
			list_add_tail(&new_entry->entry, &dev->cont_qhead);
		}
		goto end;
	} else if (cont_cmd->cmd == MSM_CAMERA_BUF_MNGR_CONT_UNMAP) {
		if (!list_empty(&dev->cont_qhead)) {
			msm_buf_mngr_contq_listdel(dev, cont_cmd->session_id,
				cont_cmd->stream_id, 1, cont_cmd->cnt);
		} else {
			pr_err("Nothing mapped for %d,%d\n",
				cont_cmd->session_id, cont_cmd->stream_id);
			rc = -EINVAL;
		}
		goto end;
	}

free_list:
	if (i != 0) {
		if (!list_empty(&dev->cont_qhead)) {
			msm_buf_mngr_contq_listdel(dev, cont_cmd->session_id,
				cont_cmd->stream_id, 0, i);
		}
	}
	ion_unmap_kernel(dev->ion_client, ion_handle);
free_ion_handle:
	ion_free(dev->ion_client, ion_handle);
end:
	mutex_unlock(&dev->cont_mutex);
	return rc;
}

static int msm_generic_buf_mngr_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct msm_buf_mngr_device *buf_mngr_dev = v4l2_get_subdevdata(sd);
	if (!buf_mngr_dev) {
		pr_err("%s buf manager device NULL\n", __func__);
		rc = -ENODEV;
		return rc;
	}
	return rc;
}

static int msm_generic_buf_mngr_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct msm_buf_mngr_device *buf_mngr_dev = v4l2_get_subdevdata(sd);
	if (!buf_mngr_dev) {
		pr_err("%s buf manager device NULL\n", __func__);
		rc = -ENODEV;
		return rc;
	}
	return rc;
}

int msm_cam_buf_mgr_ops(unsigned int cmd, void *argp)
{
	int rc = 0;

	if (!msm_buf_mngr_dev)
		return -ENODEV;
	if (!argp)
		return -EINVAL;

	switch (cmd) {
	case VIDIOC_MSM_BUF_MNGR_GET_BUF:
		rc = msm_buf_mngr_get_buf(msm_buf_mngr_dev, argp);
		break;
	case VIDIOC_MSM_BUF_MNGR_BUF_DONE:
		rc = msm_buf_mngr_buf_done(msm_buf_mngr_dev, argp);
		break;
	case VIDIOC_MSM_BUF_MNGR_BUF_ERROR:
		rc = msm_buf_mngr_buf_error(msm_buf_mngr_dev, argp);
		break;
	case VIDIOC_MSM_BUF_MNGR_PUT_BUF:
		rc = msm_buf_mngr_put_buf(msm_buf_mngr_dev, argp);
		break;
	case VIDIOC_MSM_BUF_MNGR_IOCTL_CMD: {
		struct msm_camera_private_ioctl_arg *k_ioctl = argp;

		switch (k_ioctl->id) {
		case MSM_CAMERA_BUF_MNGR_IOCTL_ID_GET_BUF_BY_IDX: {
			struct msm_buf_mngr_info *tmp = NULL;

			if (!k_ioctl->ioctl_ptr)
				return -EINVAL;
			if (k_ioctl->size != sizeof(struct msm_buf_mngr_info))
				return -EINVAL;

			MSM_CAM_GET_IOCTL_ARG_PTR(&tmp, &k_ioctl->ioctl_ptr,
				sizeof(tmp));
			rc = msm_buf_mngr_get_buf_by_idx(msm_buf_mngr_dev,
				tmp);
			}
			break;
		default:
			pr_debug("unimplemented id %d", k_ioctl->id);
			return -EINVAL;
		}
	break;
	}
	default:
		return -ENOIOCTLCMD;
	}

	return rc;
}

int msm_cam_buf_mgr_register_ops(struct msm_cam_buf_mgr_req_ops *cb_struct)
{
	if (!msm_buf_mngr_dev)
		return -ENODEV;
	if (!cb_struct)
		return -EINVAL;

	cb_struct->msm_cam_buf_mgr_ops = msm_cam_buf_mgr_ops;
	return 0;
}

static long msm_buf_mngr_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int32_t rc = 0;
	struct msm_buf_mngr_device *buf_mngr_dev = v4l2_get_subdevdata(sd);
	void __user *argp = (void __user *)arg;

	if (!buf_mngr_dev) {
		pr_err("%s buf manager device NULL\n", __func__);
		rc = -ENOMEM;
		return rc;
	}
	switch (cmd) {
	case VIDIOC_MSM_BUF_MNGR_IOCTL_CMD: {
		struct msm_camera_private_ioctl_arg k_ioctl, *ptr;

		if (!arg)
			return -EINVAL;
		ptr = arg;
		k_ioctl = *ptr;
		switch (k_ioctl.id) {
		case MSM_CAMERA_BUF_MNGR_IOCTL_ID_GET_BUF_BY_IDX: {

			if (k_ioctl.size != sizeof(struct msm_buf_mngr_info))
				return -EINVAL;
			if (!k_ioctl.ioctl_ptr)
				return -EINVAL;
#ifndef CONFIG_COMPAT
			{
				struct msm_buf_mngr_info buf_info, *tmp = NULL;

				MSM_CAM_GET_IOCTL_ARG_PTR(&tmp,
					&k_ioctl.ioctl_ptr, sizeof(tmp));
				if (copy_from_user(&buf_info, tmp,
					sizeof(struct msm_buf_mngr_info))) {
					return -EFAULT;
				}
				k_ioctl.ioctl_ptr = (uintptr_t)&buf_info;
			}
#endif
			argp = &k_ioctl;
			rc = msm_cam_buf_mgr_ops(cmd, argp);
			}
			break;
		default:
			pr_debug("unimplemented id %d", k_ioctl.id);
			return -EINVAL;
		}
		break;
	}
	case VIDIOC_MSM_BUF_MNGR_GET_BUF:
	case VIDIOC_MSM_BUF_MNGR_BUF_DONE:
	case VIDIOC_MSM_BUF_MNGR_PUT_BUF:
	case VIDIOC_MSM_BUF_MNGR_BUF_ERROR:
		rc = msm_cam_buf_mgr_ops(cmd, argp);
		break;
	case VIDIOC_MSM_BUF_MNGR_INIT:
		rc = msm_generic_buf_mngr_open(sd, NULL);
		break;
	case VIDIOC_MSM_BUF_MNGR_DEINIT:
		rc = msm_generic_buf_mngr_close(sd, NULL);
		break;
	case MSM_SD_NOTIFY_FREEZE:
		break;
	case VIDIOC_MSM_BUF_MNGR_FLUSH:
		rc = msm_generic_buf_mngr_flush(buf_mngr_dev, argp);
		break;
	case MSM_SD_UNNOTIFY_FREEZE:
		break;
	case MSM_SD_SHUTDOWN:
		msm_buf_mngr_sd_shutdown(buf_mngr_dev, argp);
		break;
	case VIDIOC_MSM_BUF_MNGR_CONT_CMD:
		rc = msm_buf_mngr_handle_cont_cmd(buf_mngr_dev, argp);
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return rc;
}

#ifdef CONFIG_COMPAT
static long msm_camera_buf_mgr_fetch_buf_info(
		struct msm_buf_mngr_info32_t *buf_info32,
		struct msm_buf_mngr_info *buf_info, unsigned long arg)
{
	if (!arg || !buf_info32 || !buf_info)
		return -EINVAL;

	if (copy_from_user(buf_info32, (void __user *)arg,
				sizeof(struct msm_buf_mngr_info32_t)))
		return -EFAULT;

	buf_info->session_id = buf_info32->session_id;
	buf_info->stream_id = buf_info32->stream_id;
	buf_info->frame_id = buf_info32->frame_id;
	buf_info->index = buf_info32->index;
	buf_info->timestamp.tv_sec = (long) buf_info32->timestamp.tv_sec;
	buf_info->timestamp.tv_usec = (long) buf_info32->
					timestamp.tv_usec;
	buf_info->reserved = buf_info32->reserved;
	buf_info->type = buf_info32->type;
	return 0;
}

static long msm_camera_buf_mgr_update_buf_info(
		struct msm_buf_mngr_info32_t *buf_info32,
		struct msm_buf_mngr_info *buf_info, unsigned long arg)
{
	if (!arg || !buf_info32 || !buf_info)
		return -EINVAL;

	buf_info32->session_id = buf_info->session_id;
	buf_info32->stream_id = buf_info->stream_id;
	buf_info32->index = buf_info->index;
	buf_info32->timestamp.tv_sec = (int32_t) buf_info->
						timestamp.tv_sec;
	buf_info32->timestamp.tv_usec = (int32_t) buf_info->timestamp.
						tv_usec;
	buf_info32->reserved = buf_info->reserved;
	buf_info32->type = buf_info->type;
	buf_info32->user_buf.buf_cnt = buf_info->user_buf.buf_cnt;
	memcpy(&buf_info32->user_buf.buf_idx,
		&buf_info->user_buf.buf_idx,
		sizeof(buf_info->user_buf.buf_idx));
	if (copy_to_user((void __user *)arg, buf_info32,
			sizeof(struct msm_buf_mngr_info32_t)))
		return -EFAULT;
	return 0;
}
static long msm_camera_buf_mgr_internal_compat_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);
	long rc = 0;
	struct msm_camera_private_ioctl_arg k_ioctl;
	void __user *tmp_compat_ioctl_ptr = NULL;

	rc = msm_copy_camera_private_ioctl_args(arg,
		&k_ioctl, &tmp_compat_ioctl_ptr);
	if (rc < 0) {
		pr_err("Subdev cmd %d failed\n", cmd);
		return rc;
	}

	switch (k_ioctl.id) {
	case MSM_CAMERA_BUF_MNGR_IOCTL_ID_GET_BUF_BY_IDX: {
		struct msm_buf_mngr_info32_t buf_info32;
		struct msm_buf_mngr_info buf_info;

		if (k_ioctl.size != sizeof(struct msm_buf_mngr_info32_t)) {
			pr_err("Invalid size for id %d with size %d",
				k_ioctl.id, k_ioctl.size);
			return -EINVAL;
		}
		if (!tmp_compat_ioctl_ptr) {
			pr_err("Invalid ptr for id %d", k_ioctl.id);
			return -EINVAL;
		}
		k_ioctl.ioctl_ptr = (__u64)&buf_info;
		k_ioctl.size = sizeof(struct msm_buf_mngr_info);
		rc = msm_camera_buf_mgr_fetch_buf_info(&buf_info32, &buf_info,
			(unsigned long)tmp_compat_ioctl_ptr);
		if (rc < 0) {
			pr_err("Fetch buf info failed for cmd=%d", cmd);
			return rc;
		}
		rc = v4l2_subdev_call(sd, core, ioctl, cmd, &k_ioctl);
		if (rc < 0) {
			pr_err("Subdev cmd %d failed for id %d", cmd,
				k_ioctl.id);
			return rc;
		}
		}
		break;
	default:
		pr_debug("unimplemented id %d", k_ioctl.id);
		return -EINVAL;
	}

	return 0;
}

static long msm_bmgr_subdev_fops_compat_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);
	int32_t rc = 0;

	/* Convert 32 bit IOCTL ID's to 64 bit IOCTL ID's
	 * except VIDIOC_MSM_CPP_CFG32, which needs special
	 * processing
	 */
	switch (cmd) {
	case VIDIOC_MSM_BUF_MNGR_GET_BUF32:
		cmd = VIDIOC_MSM_BUF_MNGR_GET_BUF;
		break;
	case VIDIOC_MSM_BUF_MNGR_BUF_DONE32:
		cmd = VIDIOC_MSM_BUF_MNGR_BUF_DONE;
		break;
	case VIDIOC_MSM_BUF_MNGR_BUF_ERROR32:
		cmd = VIDIOC_MSM_BUF_MNGR_BUF_ERROR;
		break;
	case VIDIOC_MSM_BUF_MNGR_PUT_BUF32:
		cmd = VIDIOC_MSM_BUF_MNGR_PUT_BUF;
		break;
	case VIDIOC_MSM_BUF_MNGR_CONT_CMD:
		break;
	case VIDIOC_MSM_BUF_MNGR_FLUSH32:
		cmd = VIDIOC_MSM_BUF_MNGR_FLUSH;
		break;
	case VIDIOC_MSM_BUF_MNGR_IOCTL_CMD:
		break;
	default:
		pr_debug("unsupported compat type\n");
		return -ENOIOCTLCMD;
	}

	switch (cmd) {
	case VIDIOC_MSM_BUF_MNGR_GET_BUF:
	case VIDIOC_MSM_BUF_MNGR_BUF_DONE:
	case VIDIOC_MSM_BUF_MNGR_BUF_ERROR:
	case VIDIOC_MSM_BUF_MNGR_FLUSH:
	case VIDIOC_MSM_BUF_MNGR_PUT_BUF: {
		struct msm_buf_mngr_info32_t buf_info32;
		struct msm_buf_mngr_info buf_info;

		rc = msm_camera_buf_mgr_fetch_buf_info(&buf_info32, &buf_info,
			arg);
		if (rc < 0) {
			pr_err("Fetch buf info failed for cmd=%d\n", cmd);
			return rc;
		}
		rc = v4l2_subdev_call(sd, core, ioctl, cmd, &buf_info);
		if (rc < 0) {
			pr_debug("Subdev cmd %d fail\n", cmd);
			return rc;
		}
		rc = msm_camera_buf_mgr_update_buf_info(&buf_info32, &buf_info,
			arg);
		if (rc < 0) {
			pr_err("Update buf info failed for cmd=%d\n", cmd);
			return rc;
		}
		break;
	}
	case VIDIOC_MSM_BUF_MNGR_IOCTL_CMD: {
		rc = msm_camera_buf_mgr_internal_compat_ioctl(file, cmd, arg);
		if (rc < 0) {
			pr_debug("Subdev cmd %d fail\n", cmd);
			return rc;
		}
		}
		break;
	case VIDIOC_MSM_BUF_MNGR_CONT_CMD: {
		struct msm_buf_mngr_main_cont_info cont_cmd;

		if (copy_from_user(&cont_cmd, (void __user *)arg,
			sizeof(struct msm_buf_mngr_main_cont_info)))
			return -EFAULT;
		rc = v4l2_subdev_call(sd, core, ioctl, cmd, &cont_cmd);
		if (rc < 0) {
			pr_debug("Subdev cmd %d fail\n", cmd);
			return rc;
		}
		}
		break;
	default:
		pr_debug("unsupported compat type\n");
		return -ENOIOCTLCMD;
		break;
	}
	return 0;
}
#endif

static struct v4l2_subdev_core_ops msm_buf_mngr_subdev_core_ops = {
	.ioctl = msm_buf_mngr_subdev_ioctl,
};

static const struct v4l2_subdev_internal_ops
	msm_generic_buf_mngr_subdev_internal_ops = {
	.open  = msm_generic_buf_mngr_open,
	.close = msm_generic_buf_mngr_close,
};

static const struct v4l2_subdev_ops msm_buf_mngr_subdev_ops = {
	.core = &msm_buf_mngr_subdev_core_ops,
};

static const struct of_device_id msm_buf_mngr_dt_match[] = {
	{.compatible = "qcom,msm_buf_mngr"},
	{}
};

static struct v4l2_file_operations msm_buf_v4l2_subdev_fops;

static long msm_bmgr_subdev_do_ioctl(
		struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);

	return v4l2_subdev_call(sd, core, ioctl, cmd, arg);
}


static long msm_buf_subdev_fops_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_bmgr_subdev_do_ioctl);
}

static int32_t __init msm_buf_mngr_init(void)
{
	int32_t rc = 0;
	msm_buf_mngr_dev = kzalloc(sizeof(*msm_buf_mngr_dev),
		GFP_KERNEL);
	if (WARN_ON(!msm_buf_mngr_dev)) {
		pr_err("%s: not enough memory", __func__);
		return -ENOMEM;
	}
	/* Sub-dev */
	v4l2_subdev_init(&msm_buf_mngr_dev->subdev.sd,
		&msm_buf_mngr_subdev_ops);
	msm_cam_copy_v4l2_subdev_fops(&msm_buf_v4l2_subdev_fops);
	msm_buf_v4l2_subdev_fops.unlocked_ioctl = msm_buf_subdev_fops_ioctl;
#ifdef CONFIG_COMPAT
	msm_buf_v4l2_subdev_fops.compat_ioctl32 =
			msm_bmgr_subdev_fops_compat_ioctl;
#endif
	snprintf(msm_buf_mngr_dev->subdev.sd.name,
		ARRAY_SIZE(msm_buf_mngr_dev->subdev.sd.name), "msm_buf_mngr");
	msm_buf_mngr_dev->subdev.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	v4l2_set_subdevdata(&msm_buf_mngr_dev->subdev.sd, msm_buf_mngr_dev);

	media_entity_init(&msm_buf_mngr_dev->subdev.sd.entity, 0, NULL, 0);
	msm_buf_mngr_dev->subdev.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	msm_buf_mngr_dev->subdev.sd.entity.group_id =
		MSM_CAMERA_SUBDEV_BUF_MNGR;
	msm_buf_mngr_dev->subdev.sd.internal_ops =
		&msm_generic_buf_mngr_subdev_internal_ops;
	msm_buf_mngr_dev->subdev.close_seq = MSM_SD_CLOSE_4TH_CATEGORY;
	rc = msm_sd_register(&msm_buf_mngr_dev->subdev);
	if (rc != 0) {
		pr_err("%s: msm_sd_register error = %d\n", __func__, rc);
		goto end;
	}

	msm_buf_mngr_dev->subdev.sd.devnode->fops = &msm_buf_v4l2_subdev_fops;

	v4l2_subdev_notify(&msm_buf_mngr_dev->subdev.sd, MSM_SD_NOTIFY_REQ_CB,
		&msm_buf_mngr_dev->vb2_ops);

	INIT_LIST_HEAD(&msm_buf_mngr_dev->buf_qhead);
	spin_lock_init(&msm_buf_mngr_dev->buf_q_spinlock);

	mutex_init(&msm_buf_mngr_dev->cont_mutex);
	INIT_LIST_HEAD(&msm_buf_mngr_dev->cont_qhead);
	msm_buf_mngr_dev->ion_client =
		msm_ion_client_create("msm_cam_generic_buf_mgr");
	if (!msm_buf_mngr_dev->ion_client) {
		pr_err("%s: Failed to create ion client\n", __func__);
		rc = -EBADFD;
	}

end:
	return rc;
}

static void __exit msm_buf_mngr_exit(void)
{
	msm_sd_unregister(&msm_buf_mngr_dev->subdev);
	mutex_destroy(&msm_buf_mngr_dev->cont_mutex);
	kfree(msm_buf_mngr_dev);
}

module_init(msm_buf_mngr_init);
module_exit(msm_buf_mngr_exit);
MODULE_DESCRIPTION("MSM Buffer Manager");
MODULE_LICENSE("GPL v2");
