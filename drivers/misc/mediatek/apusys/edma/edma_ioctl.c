/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: JB Tsai <jb.tsai@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>


#include "edma_driver.h"
#include "edma_cmd_hnd.h"
#include "edma_ioctl.h"
#include "edma_queue.h"

long edma_ioctl(struct file *flip, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct edma_user *user = flip->private_data;
	struct edma_device *edma_device = dev_get_drvdata(user->dev);
	struct device *dev = edma_device->dev;

	switch (cmd) {
	case EDMA_IOCTL_ENQUE_EXT_MODE:{
			struct edma_request *req;
			struct edma_ext edma_ext;

			ret = copy_from_user((void *)&edma_ext, (void *)arg,
						sizeof(struct edma_ext));
			if (ret) {
				dev_notice(dev,
					"[ENQUE_EXT_MODE] get params failed, ret=%d\n",
					ret);
				goto out;
			}

			ret = edma_alloc_request(&req);
			if (ret) {
				dev_notice(dev,
					"[ENQUE_EXT_MODE] alloc request failed, ret=%d\n",
					ret);
				goto out;
			}

			edma_setup_ext_mode_request(req, &edma_ext,
							EDMA_PROC_EXT_MODE);
			ret = edma_push_request_to_queue(user, req);
			if (ret) {
				dev_notice(dev,
					"[ENQUE_EXT_MODE] push to user's queue failed, ret=%d\n",
					ret);
				edma_free_request(req);
				goto out;
			}

			edma_ext.cmd_handle = req->handle;

			ret =
			    copy_to_user((void *)arg, (void *)&edma_ext,
					 sizeof(struct edma_ext));
			if (ret) {
				dev_notice(dev,
					"[ENQUE_EXT_MODE] return params failed, ret=%d\n",
					ret);
				edma_free_request(req);
				goto out;
			}

			break;
		}
	case EDMA_IOCTL_ENQUE_NORMAL:{
			struct edma_request *req;
			struct edma_normal edma_normal;

			ret = copy_from_user((void *)&edma_normal, (void *)arg,
						sizeof(struct edma_normal));
			if (ret) {
				dev_notice(dev,
					"[ENQUE_NORMAL] get params failed, ret=%d\n",
					ret);
				goto out;
			}

			ret = edma_alloc_request(&req);
			if (ret) {
				dev_notice(dev,
					"[ENQUE_NORMAL] alloc request failed, ret=%d\n",
					ret);
				goto out;
			}

			edma_setup_normal_request(req, &edma_normal,
							EDMA_PROC_NORMAL);
			ret = edma_push_request_to_queue(user, req);
			if (ret) {
				dev_notice(dev,
					"[ENQUE_NORMAL] push to user's queue failed, ret=%d\n",
					ret);
				edma_free_request(req);
				goto out;
			}

			edma_normal.cmd_handle = req->handle;

			ret =
			    copy_to_user((void *)arg, (void *)&edma_normal,
					 sizeof(struct edma_normal));
			if (ret) {
				dev_notice(dev,
					"[ENQUE_NORMAL] return params failed, ret=%d\n",
					ret);
				edma_free_request(req);
				goto out;
			}

			break;
		}
	case EDMA_IOCTL_ENQUE_FILL:{
			struct edma_request *req;
			struct edma_fill edma_fill;

			ret = copy_from_user((void *)&edma_fill, (void *)arg,
						sizeof(struct edma_fill));
			if (ret) {
				dev_notice(dev,
					"[ENQUE_FILL] get params failed, ret=%d\n",
					ret);
				goto out;
			}

			ret = edma_alloc_request(&req);
			if (ret) {
				dev_notice(dev,
					"[ENQUE_FILL] alloc request failed, ret=%d\n",
					ret);
				goto out;
			}

			edma_setup_fill_request(req, &edma_fill,
							EDMA_PROC_FILL);
			ret = edma_push_request_to_queue(user, req);
			if (ret) {
				dev_notice(dev,
					"[ENQUE_FILL] push to user's queue failed, ret=%d\n",
					ret);
				edma_free_request(req);
				goto out;
			}

			edma_fill.cmd_handle = req->handle;

			ret =
			    copy_to_user((void *)arg, (void *)&edma_fill,
					 sizeof(struct edma_fill));
			if (ret) {
				dev_notice(dev,
					"[ENQUE_FILL] return params failed, ret=%d\n",
					ret);
				edma_free_request(req);
				goto out;
			}

			break;
		}
	case EDMA_IOCTL_ENQUE_NUMERICAL:{
			struct edma_request *req;
			struct edma_numerical edma_numerical;

			ret = copy_from_user((void *)&edma_numerical,
				(void *)arg, sizeof(struct edma_numerical));
			if (ret) {
				dev_notice(dev,
					"[ENQUE_NUMERICAL] get params failed, ret=%d\n",
					ret);
				goto out;
			}

			ret = edma_alloc_request(&req);
			if (ret) {
				dev_notice(dev,
					"[ENQUE_NUMERICAL] alloc request failed, ret=%d\n",
					ret);
				goto out;
			}

			edma_setup_numerical_request(req, &edma_numerical,
							EDMA_PROC_NORMAL);
			ret = edma_push_request_to_queue(user, req);
			if (ret) {
				dev_notice(dev,
					"[ENQUE_NUMERICAL] push to user's queue failed, ret=%d\n",
					ret);
				edma_free_request(req);
				goto out;
			}

			edma_numerical.cmd_handle = req->handle;

			ret =
			    copy_to_user((void *)arg, (void *)&edma_numerical,
					 sizeof(struct edma_numerical));
			if (ret) {
				dev_notice(dev,
					"[ENQUE_NUMERICAL] return params failed, ret=%d\n",
					ret);
				edma_free_request(req);
				goto out;
			}

			break;
		}
	case EDMA_IOCTL_ENQUE_FORMAT:{
			struct edma_request *req;
			struct edma_format edma_format;

			ret = copy_from_user((void *)&edma_format, (void *)arg,
						sizeof(struct edma_format));
			if (ret) {
				dev_notice(dev,
					"[ENQUE_FORMAT] get params failed, ret=%d\n",
					ret);
				goto out;
			}

			ret = edma_alloc_request(&req);
			if (ret) {
				dev_notice(dev,
					"[ENQUE_FORMAT] alloc request failed, ret=%d\n",
					ret);
				goto out;
			}

			edma_setup_format_request(req, &edma_format,
							EDMA_PROC_FORMAT);
			ret = edma_push_request_to_queue(user, req);
			if (ret) {
				dev_notice(dev,
					"[ENQUE_FORMAT] push to user's queue failed, ret=%d\n",
					ret);
				edma_free_request(req);
				goto out;
			}

			edma_format.cmd_handle = req->handle;

			ret =
			    copy_to_user((void *)arg, (void *)&edma_format,
					 sizeof(struct edma_format));
			if (ret) {
				dev_notice(dev,
					"[ENQUE_FORMAT] return params failed, ret=%d\n",
					ret);
				edma_free_request(req);
				goto out;
			}

			break;
		}
	case EDMA_IOCTL_ENQUE_COMPRESS:{
			struct edma_request *req;
			struct edma_compress edma_compress;

			ret = copy_from_user((void *)&edma_compress,
				(void *)arg, sizeof(struct edma_compress));
			if (ret) {
				dev_notice(dev,
					"[ENQUE_COMPRESS] get params failed, ret=%d\n",
					ret);
				goto out;
			}

			ret = edma_alloc_request(&req);
			if (ret) {
				dev_notice(dev,
					"[ENQUE_COMPRESS] alloc request failed, ret=%d\n",
					ret);
				goto out;
			}

			edma_setup_compress_request(req, &edma_compress,
							EDMA_PROC_COMPRESS);
			ret = edma_push_request_to_queue(user, req);
			if (ret) {
				dev_notice(dev,
					"[ENQUE_COMPRESS] push to user's queue failed, ret=%d\n",
					ret);
				edma_free_request(req);
				goto out;
			}

			edma_compress.cmd_handle = req->handle;

			ret =
			    copy_to_user((void *)arg, (void *)&edma_compress,
					 sizeof(struct edma_compress));
			if (ret) {
				dev_notice(dev,
					"[ENQUE_COMPRESS] return params failed, ret=%d\n",
					ret);
				edma_free_request(req);
				goto out;
			}

			break;
		}
	case EDMA_IOCTL_ENQUE_DECOMPRESS:{
			struct edma_request *req;
			struct edma_decompress edma_decompress;

			ret = copy_from_user((void *)&edma_decompress,
				(void *)arg, sizeof(struct edma_decompress));
			if (ret) {
				dev_notice(dev,
					"[ENQUE_DECOMPRESS] get params failed, ret=%d\n",
					ret);
				goto out;
			}

			ret = edma_alloc_request(&req);
			if (ret) {
				dev_notice(dev,
					"[ENQUE_DECOMPRESS] alloc request failed, ret=%d\n",
					ret);
				goto out;
			}

			edma_setup_decompress_request(req, &edma_decompress,
							EDMA_PROC_DECOMPRESS);
			ret = edma_push_request_to_queue(user, req);
			if (ret) {
				dev_notice(dev,
					"[ENQUE_DECOMPRESS] push to user's queue failed, ret=%d\n",
					ret);
				edma_free_request(req);
				goto out;
			}

			edma_decompress.cmd_handle = req->handle;

			ret =
			    copy_to_user((void *)arg, (void *)&edma_decompress,
					 sizeof(struct edma_decompress));
			if (ret) {
				dev_notice(dev,
					"[ENQUE_DECOMPRESS] return params failed, ret=%d\n",
					ret);
				edma_free_request(req);
				goto out;
			}

			break;
		}
	case EDMA_IOCTL_ENQUE_RAW:{
			struct edma_request *req;
			struct edma_raw edma_raw;

			ret = copy_from_user((void *)&edma_raw, (void *)arg,
						sizeof(struct edma_raw));
			if (ret) {
				dev_notice(dev,
					"[ENQUE_DECOMPRESS] get params failed, ret=%d\n",
					ret);
				goto out;
			}

			ret = edma_alloc_request(&req);
			if (ret) {
				dev_notice(dev,
					"[ENQUE_DECOMPRESS] alloc request failed, ret=%d\n",
					ret);
				goto out;
			}

			edma_setup_raw_request(req, &edma_raw, EDMA_PROC_RAW);
			ret = edma_push_request_to_queue(user, req);
			if (ret) {
				dev_notice(dev,
					"[ENQUE_DECOMPRESS] push to user's queue failed, ret=%d\n",
					ret);
				edma_free_request(req);
				goto out;
			}

			edma_raw.cmd_handle = req->handle;

			ret =
			    copy_to_user((void *)arg, (void *)&edma_raw,
					 sizeof(struct edma_raw));
			if (ret) {
				dev_notice(dev,
					"[ENQUE_RAW] return params failed, ret=%d\n",
					ret);
				edma_free_request(req);
				goto out;
			}

			break;
		}
	case EDMA_IOCTL_DEQUE:{
			struct edma_request *req = NULL;
			struct edma_cmd_deque cmd_deque;
			int status = EDMA_REQ_STATUS_RUN;

			ret =
			    copy_from_user((void *)&cmd_deque, (void *)arg,
					   sizeof(struct edma_cmd_deque));
			if (ret) {
				dev_notice(dev,
					"[DEQUE] get params failed, ret=%d\n",
					ret);
				goto out;
			}

			if (cmd_deque.cmd_handle == 0) {
				dev_notice(dev,
					"[DEQUE] get invalid cmd\n");
				ret = -EINVAL;
				goto out;
			}

			status =
			    edma_pop_request_from_queue(cmd_deque.cmd_handle,
								user, &req);

			if (req && status == EDMA_REQ_STATUS_DEQUEUE) {
				cmd_deque.cmd_result = req->cmd_result;
				cmd_deque.cmd_status = req->cmd_status;

				edma_free_request(req);
			}

			ret =
			    copy_to_user((void *)arg, &cmd_deque,
					 sizeof(struct edma_cmd_deque));
			if (ret) {
				dev_notice(dev,
					"[DEQUE] return params failed, ret=%d\n",
					ret);
				goto out;
			}
			break;
		}
	#ifdef DEBUG
	case EDMA_IOCTL_SYNC_NORMAL:{
			struct edma_request *req = NULL;
			struct edma_normal edma_normal;

			ret = copy_from_user((void *)&edma_normal, (void *)arg,
						sizeof(struct edma_normal));
			if (ret) {
				dev_notice(dev,
					"[SYNC_NORMAL] get params failed, ret=%d\n",
					ret);
				goto out;
			}

			ret = edma_alloc_request(&req);
			if (ret) {
				dev_notice(dev,
					"[SYNC_NORMAL] alloc request failed, ret=%d\n",
					ret);
				goto out;
			}

			edma_setup_normal_request(req, &edma_normal,
							EDMA_PROC_NORMAL);
			ret = edma_sync_normal_mode(edma_device, req);
			edma_free_request(req);
			break;
		}
	case EDMA_IOCTL_SYNC_EXT_MODE:{
			struct edma_request *req = NULL;
			struct edma_ext edma_ext;

			ret = copy_from_user((void *)&edma_ext, (void *)arg,
						sizeof(struct edma_ext));
			if (ret) {
				dev_notice(dev,
					"[SYNC_EXT_MODE] get params failed, ret=%d\n",
					ret);
				goto out;
			}

			ret = edma_alloc_request(&req);
			if (ret) {
				dev_notice(dev,
					"[SYNC_EXT_MODE] alloc request failed, ret=%d\n",
					ret);
				goto out;
			}

			edma_setup_ext_mode_request(req, &edma_ext,
							EDMA_PROC_EXT_MODE);
			ret = edma_sync_ext_mode(edma_device, req);
			edma_free_request(req);
			break;
		}
	#endif
	default:
		dev_notice(dev, "ioctl: no such command!\n");
		ret = -EINVAL;
		break;
	}
out:
	if (ret) {
		dev_notice(dev,
			"fail, cmd(%d), pid(%d), (process, pid, tgid)=(%s, %d, %d)\n",
			cmd, user->open_pid, current->comm, current->pid,
			current->tgid);
	}

	return ret;
}


