/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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

#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/ion.h>
#include <linux/msm_ion.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-core.h>

#include "msm_fd_dev.h"
#include "msm_fd_hw.h"
#include "msm_fd_regs.h"

#define MSM_FD_DRV_NAME "msm_fd"

#define MSM_FD_WORD_SIZE_BYTES 4

/* Face detection thresholds definitions */
#define MSM_FD_DEF_THRESHOLD 5
#define MSM_FD_MAX_THRESHOLD_VALUE 9

/* Face angle lookup table */
#define MSM_FD_DEF_ANGLE_IDX 2
static int msm_fd_angle[] = {45, 135, 359};

/* Face direction lookup table */
#define MSM_FD_DEF_DIR_IDX 0
static int msm_fd_dir[] = {0, 90, 270, 180};

/* Minimum face size lookup table */
#define MSM_FD_DEF_MIN_SIZE_IDX 0
static int msm_fd_min_size[] = {20, 25, 32, 40};

/* Face detection size lookup table */
static struct msm_fd_size fd_size[] = {
	{
		.width    = 320,
		.height   = 240,
		.reg_val  = MSM_FD_IMAGE_SIZE_QVGA,
		.work_size = (13120 * MSM_FD_WORD_SIZE_BYTES),
	},
	{
		.width    = 427,
		.height   = 240,
		.reg_val  = MSM_FD_IMAGE_SIZE_WQVGA,
		.work_size = (17744 * MSM_FD_WORD_SIZE_BYTES),
	},
	{
		.width     = 640,
		.height    = 480,
		.reg_val   = MSM_FD_IMAGE_SIZE_VGA,
		.work_size = (52624 * MSM_FD_WORD_SIZE_BYTES),
	},
	{
		.width     = 854,
		.height    = 480,
		.reg_val   = MSM_FD_IMAGE_SIZE_WVGA,
		.work_size = (70560 * MSM_FD_WORD_SIZE_BYTES),
	},
};

/*
 * msm_fd_ctx_from_fh - Get fd context from v4l2 fh.
 * @fh: Pointer to v4l2 fh.
 */
static inline struct fd_ctx *msm_fd_ctx_from_fh(struct v4l2_fh *fh)
{
	return container_of(fh, struct fd_ctx, fh);
}

/*
 * msm_fd_get_format_index - Get format index from v4l2 format.
 * @f: Pointer to v4l2 format struct.
 */
static int msm_fd_get_format_index(struct v4l2_format *f)
{
	int index;

	for (index = 0; index < ARRAY_SIZE(fd_size); index++) {
		if (f->fmt.pix.width <= fd_size[index].width &&
		    f->fmt.pix.height <= fd_size[index].height)
			return index;
	}
	return index - 1;
}

/*
 * msm_fd_get_idx_from_value - Get array index from value.
 * @value: Value for which index is needed.
 * @array: Array in which index is searched for.
 * @array_size: Array size.
 */
static int msm_fd_get_idx_from_value(int value, int *array, int array_size)
{
	int index;
	int i;

	index = 0;
	for (i = 1; i < array_size; i++) {
		if (value == array[i]) {
			index = i;
			break;
		}
		if (abs(value - array[i]) < abs(value - array[index]))
			index = i;
	}
	return index;
}

/*
 * msm_fd_fill_format_from_index - Fill v4l2 format struct from size index.
 * @f: Pointer of v4l2 struct which will be filled.
 * @index: Size index (Format will be filled based on this index).
 */
static int msm_fd_fill_format_from_index(struct v4l2_format *f, int index)
{
	f->fmt.pix.width = fd_size[index].width;
	f->fmt.pix.height = fd_size[index].height;
	f->fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
	if (f->fmt.pix.bytesperline < f->fmt.pix.width)
		f->fmt.pix.bytesperline = f->fmt.pix.width;

	f->fmt.pix.bytesperline = ALIGN(f->fmt.pix.bytesperline, 16);
	f->fmt.pix.sizeimage = f->fmt.pix.bytesperline * f->fmt.pix.height;
	f->fmt.pix.field = V4L2_FIELD_NONE;

	return 0;
}

/*
 * msm_fd_fill_format_from_ctx - Fill v4l2 format struct from fd context.
 * @f: Pointer of v4l2 struct which will be filled.
 * @c: Pointer to fd context.
 */
static int msm_fd_fill_format_from_ctx(struct v4l2_format *f, struct fd_ctx *c)
{
	if (c->format.size == NULL)
		return -EINVAL;

	f->fmt.pix.width = c->format.size->width;
	f->fmt.pix.height = c->format.size->height;
	f->fmt.pix.pixelformat = c->format.pixelformat;
	f->fmt.pix.bytesperline = c->format.bytesperline;
	f->fmt.pix.sizeimage = c->format.sizeimage;
	f->fmt.pix.field = V4L2_FIELD_NONE;

	return 0;
}

/*
 * msm_fd_queue_setup - vb2_ops queue_setup callback.
 * @q: Pointer to vb2 queue struct.
 * @fmt: Pointer to v4l2 format struct (NULL is valid argument).
 * @num_buffers: Pointer of number of buffers requested.
 * @num_planes: Pointer to number of planes requested.
 * @sizes: Array containing sizes of planes.
 * @alloc_ctxs: Array of allocated contexts for each plane.
 */
static int msm_fd_queue_setup(struct vb2_queue *q,
	const struct v4l2_format *fmt,
	unsigned int *num_buffers, unsigned int *num_planes,
	unsigned int sizes[], void *alloc_ctxs[])
{
	struct fd_ctx *ctx = vb2_get_drv_priv(q);

	*num_planes = 1;

	if (fmt == NULL)
		sizes[0] = ctx->format.sizeimage;
	else
		sizes[0] = fmt->fmt.pix.sizeimage;

	alloc_ctxs[0] = &ctx->mem_pool;

	return 0;
}

/*
 * msm_fd_buf_init - vb2_ops buf_init callback.
 * @vb: Pointer to vb2 buffer struct.
 */
int msm_fd_buf_init(struct vb2_buffer *vb)
{
	struct msm_fd_buffer *fd_buffer =
		(struct msm_fd_buffer *)vb;

	INIT_LIST_HEAD(&fd_buffer->list);
	atomic_set(&fd_buffer->active, 0);

	return 0;
}

/*
 * msm_fd_buf_queue - vb2_ops buf_queue callback.
 * @vb: Pointer to vb2 buffer struct.
 */
static void msm_fd_buf_queue(struct vb2_buffer *vb)
{
	struct fd_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct msm_fd_buffer *fd_buffer =
		(struct msm_fd_buffer *)vb;

	fd_buffer->format = ctx->format;
	fd_buffer->settings = ctx->settings;
	fd_buffer->work_addr = ctx->work_buf.addr;
	msm_fd_hw_add_buffer(ctx->fd_device, fd_buffer);

	if (vb->vb2_queue->streaming)
		msm_fd_hw_schedule_and_start(ctx->fd_device);
}

/*
 * msm_fd_start_streaming - vb2_ops start_streaming callback.
 * @q: Pointer to vb2 queue struct.
 * @count: Number of buffer queued before stream on call.
 */
static int msm_fd_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct fd_ctx *ctx = vb2_get_drv_priv(q);
	int ret;

	if (ctx->work_buf.fd == -1) {
		dev_err(ctx->fd_device->dev, "Missing working buffer\n");
		return -EINVAL;
	}

	ret = msm_fd_hw_get(ctx->fd_device, ctx->settings.speed);
	if (ret < 0) {
		dev_err(ctx->fd_device->dev, "Can not acquire fd hw\n");
		goto out;
	}

	ret = msm_fd_hw_schedule_and_start(ctx->fd_device);
	if (ret < 0)
		dev_err(ctx->fd_device->dev, "Can not start fd hw\n");

out:
	return ret;
}

/*
 * msm_fd_stop_streaming - vb2_ops stop_streaming callback.
 * @q: Pointer to vb2 queue struct.
 */
static void msm_fd_stop_streaming(struct vb2_queue *q)
{
	struct fd_ctx *ctx = vb2_get_drv_priv(q);

	mutex_lock(&ctx->fd_device->recovery_lock);
	msm_fd_hw_remove_buffers_from_queue(ctx->fd_device, q);
	msm_fd_hw_put(ctx->fd_device);
	mutex_unlock(&ctx->fd_device->recovery_lock);
}

/* Videobuf2 queue callbacks. */
static struct vb2_ops msm_fd_vb2_q_ops = {
	.queue_setup     = msm_fd_queue_setup,
	.buf_init        = msm_fd_buf_init,
	.buf_queue       = msm_fd_buf_queue,
	.start_streaming = msm_fd_start_streaming,
	.stop_streaming  = msm_fd_stop_streaming,
};

/*
 * msm_fd_get_userptr - Map and get buffer handler for user pointer buffer.
 * @alloc_ctx: Contexts allocated in buf_setup.
 * @vaddr: Virtual addr passed from userpsace (in our case ion fd)
 * @size: Size of the buffer
 * @write: True if buffer will be used for writing the data.
 */
static void *msm_fd_get_userptr(void *alloc_ctx,
	unsigned long vaddr, unsigned long size, int write)
{
	struct msm_fd_mem_pool *pool = alloc_ctx;
	struct msm_fd_buf_handle *buf;
	int ret;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	ret = msm_fd_hw_map_buffer(pool, vaddr, buf);
	if (ret < 0 || buf->size < size)
		goto error;

	return buf;
error:
	kzfree(buf);
	return ERR_PTR(-ENOMEM);
}

/*
 * msm_fd_put_userptr - Unmap and free buffer handler.
 * @buf_priv: Buffer handler allocated get_userptr callback.
 */
static void msm_fd_put_userptr(void *buf_priv)
{
	if (IS_ERR_OR_NULL(buf_priv))
		return;

	msm_fd_hw_unmap_buffer(buf_priv);

	kzfree(buf_priv);
}

/* Videobuf2 memory callbacks. */
static struct vb2_mem_ops msm_fd_vb2_mem_ops = {
	.get_userptr = msm_fd_get_userptr,
	.put_userptr = msm_fd_put_userptr,
};

/*
 * msm_fd_vbif_error_handler - FD VBIF Error handler
 * @handle: FD Device handle
 * @error: CPP-VBIF Error code
 */
static int msm_fd_vbif_error_handler(void *handle, uint32_t error)
{
	struct fd_ctx *ctx;
	struct msm_fd_device *fd;
	struct msm_fd_buffer *active_buf;
	int ret;

	if (handle == NULL) {
		dev_err(fd->dev, "FD Ctx is null, Cannot recover\n");
		return 0;
	}
	ctx = (struct fd_ctx *)handle;
	fd = (struct msm_fd_device *)ctx->fd_device;

	if (error == CPP_VBIF_ERROR_HANG) {
		mutex_lock(&fd->recovery_lock);
		dev_err(fd->dev, "Handling FD VBIF Hang\n");
		if (fd->state != MSM_FD_DEVICE_RUNNING) {
			dev_err(fd->dev, "FD is not FD_DEVICE_RUNNING, %d\n",
				fd->state);
			mutex_unlock(&fd->recovery_lock);
			return 0;
		}
		fd->recovery_mode = 1;

		/* Halt and reset */
		msm_fd_hw_put(fd);
		msm_fd_hw_get(fd, ctx->settings.speed);

		/* Get active buffer */
		active_buf = msm_fd_hw_get_active_buffer(fd);

		if (active_buf == NULL) {
			dev_dbg(fd->dev, "no active buffer, return\n");
			fd->recovery_mode = 0;
			mutex_unlock(&fd->recovery_lock);
			return 0;
		}

		dev_dbg(fd->dev, "Active Buffer present.. Start re-schedule\n");

		/* Queue the buffer again */
		msm_fd_hw_add_buffer(fd, active_buf);

		/* Schedule and restart */
		ret = msm_fd_hw_schedule_next_buffer(fd);
		if (ret) {
			dev_err(fd->dev, "Cannot reschedule buffer, recovery failed\n");
			fd->recovery_mode = 0;
			mutex_unlock(&fd->recovery_lock);
			return ret;
		}
		dev_dbg(fd->dev, "Restarted FD after VBIF HAng\n");
		mutex_unlock(&fd->recovery_lock);
	}
	return 0;
}

/*
 * msm_fd_open - Fd device open method.
 * @file: Pointer to file struct.
 */
static int msm_fd_open(struct file *file)
{
	struct msm_fd_device *device = video_drvdata(file);
	struct video_device *video = video_devdata(file);
	struct fd_ctx *ctx;
	int ret;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->fd_device = device;

	/* Initialize work buffer handler */
	ctx->work_buf.pool = NULL;
	ctx->work_buf.fd = -1;

	/* Set ctx defaults */
	ctx->settings.speed = ctx->fd_device->clk_rates_num - 1;
	ctx->settings.angle_index = MSM_FD_DEF_ANGLE_IDX;
	ctx->settings.direction_index = MSM_FD_DEF_DIR_IDX;
	ctx->settings.min_size_index = MSM_FD_DEF_MIN_SIZE_IDX;
	ctx->settings.threshold = MSM_FD_DEF_THRESHOLD;

	atomic_set(&ctx->subscribed_for_event, 0);

	v4l2_fh_init(&ctx->fh, video);

	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	ctx->vb2_q.drv_priv = ctx;
	ctx->vb2_q.mem_ops = &msm_fd_vb2_mem_ops;
	ctx->vb2_q.ops = &msm_fd_vb2_q_ops;
	ctx->vb2_q.buf_struct_size = sizeof(struct msm_fd_buffer);
	ctx->vb2_q.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	ctx->vb2_q.io_modes = VB2_USERPTR;
	ctx->vb2_q.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	mutex_init(&ctx->lock);
	ret = vb2_queue_init(&ctx->vb2_q);
	if (ret < 0) {
		dev_err(device->dev, "Error queue init\n");
		goto error_vb2_queue_init;
	}

	ctx->mem_pool.fd_device = ctx->fd_device;
	ctx->stats = vmalloc(sizeof(*ctx->stats) * MSM_FD_MAX_RESULT_BUFS);
	if (!ctx->stats) {
		dev_err(device->dev, "No memory for face statistics\n");
		ret = -ENOMEM;
		goto error_stats_vmalloc;
	}

	ret = cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_FD,
			CAM_AHB_SVS_VOTE);
	if (ret < 0) {
		pr_err("%s: failed to vote for AHB\n", __func__);
		goto error_ahb_config;
	}

	/* Register with CPP VBIF error handler */
	msm_cpp_vbif_register_error_handler((void *)ctx,
		VBIF_CLIENT_FD, msm_fd_vbif_error_handler);

	return 0;

error_ahb_config:
	vfree(ctx->stats);
error_stats_vmalloc:
	vb2_queue_release(&ctx->vb2_q);
error_vb2_queue_init:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);
	return ret;
}

/*
 * msm_fd_release - Fd device release method.
 * @file: Pointer to file struct.
 */
static int msm_fd_release(struct file *file)
{
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(file->private_data);

	/* Un-register with CPP VBIF error handler */
	msm_cpp_vbif_register_error_handler((void *)ctx,
		VBIF_CLIENT_FD, NULL);

	mutex_lock(&ctx->lock);
	vb2_queue_release(&ctx->vb2_q);
	mutex_unlock(&ctx->lock);

	vfree(ctx->stats);

	if (ctx->work_buf.fd != -1)
		msm_fd_hw_unmap_buffer(&ctx->work_buf);

	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);

	kfree(ctx);

	if (cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_FD,
		CAM_AHB_SUSPEND_VOTE) < 0)
		pr_err("%s: failed to remove vote for AHB\n", __func__);

	return 0;
}

/*
 * msm_fd_poll - Fd device pool method.
 * @file: Pointer to file struct.
 * @wait: Pointer to pool table struct.
 */
static unsigned int msm_fd_poll(struct file *file,
	struct poll_table_struct *wait)
{
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(file->private_data);
	unsigned int ret;

	mutex_lock(&ctx->lock);
	ret = vb2_poll(&ctx->vb2_q, file, wait);
	mutex_unlock(&ctx->lock);

	if (atomic_read(&ctx->subscribed_for_event)) {
		poll_wait(file, &ctx->fh.wait, wait);
		if (v4l2_event_pending(&ctx->fh))
			ret |= POLLPRI;
	}

	return ret;
}

/*
 * msm_fd_private_ioctl - V4l2 private ioctl handler.
 * @file: Pointer to file struct.
 * @fd: V4l2 device file handle.
 * @valid_prio: Priority ioctl valid flag.
 * @cmd: Ioctl command.
 * @arg: Ioctl argument.
 */
static long msm_fd_private_ioctl(struct file *file, void *fh,
	bool valid_prio, unsigned int cmd, void *arg)
{
	struct msm_fd_result *req_result = arg;
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(fh);
	struct msm_fd_stats *stats;
	int stats_idx;
	int ret = 0;
	int i;

	switch (cmd) {
	case VIDIOC_MSM_FD_GET_RESULT:
		if (req_result->frame_id == 0) {
			dev_err(ctx->fd_device->dev, "Invalid frame id\n");
			return -EINVAL;
		}

		stats_idx = req_result->frame_id % MSM_FD_MAX_RESULT_BUFS;
		stats = &ctx->stats[stats_idx];
		if (req_result->frame_id != atomic_read(&stats->frame_id)) {
			dev_err(ctx->fd_device->dev, "Stats not available\n");
			return -EINVAL;
		}

		if (req_result->face_cnt > stats->face_cnt)
			req_result->face_cnt = stats->face_cnt;

		for (i = 0; i < req_result->face_cnt; i++) {
			ret = copy_to_user((void __user *)
					&req_result->face_data[i],
					&stats->face_data[i],
					sizeof(struct msm_fd_face_data));
			if (ret) {
				dev_err(ctx->fd_device->dev, "Copy to user\n");
				return -EFAULT;
			}
		}

		if (req_result->frame_id != atomic_read(&stats->frame_id)) {
			dev_err(ctx->fd_device->dev, "Erroneous buffer\n");
			return -EINVAL;
		}
		break;
	default:
		dev_err(ctx->fd_device->dev, "Wrong ioctl type %x\n", cmd);
		ret = -ENOTTY;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
/*
 * msm_fd_compat_ioctl32 - Compat ioctl handler function.
 * @file: Pointer to file struct.
 * @cmd: Ioctl command.
 * @arg: Ioctl argument.
 */
static long msm_fd_compat_ioctl32(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	long ret;

	switch (cmd) {
	case VIDIOC_MSM_FD_GET_RESULT32:
	{
		struct msm_fd_result32 result32;
		struct msm_fd_result result;

		if (copy_from_user(&result32, (void __user *)arg,
				sizeof(result32)))
			return -EFAULT;

		result.frame_id  = result32.frame_id;
		result.face_cnt  = result32.face_cnt;
		result.face_data = compat_ptr(result32.face_data);

		ret = msm_fd_private_ioctl(file, file->private_data,
			0, VIDIOC_MSM_FD_GET_RESULT, (void *)&result);

		result32.frame_id = result.frame_id;
		result32.face_cnt = result.face_cnt;

		if (copy_to_user((void __user *)arg, &result32,
				sizeof(result32)))
			return -EFAULT;

		break;
	}
	default:
		ret = -ENOIOCTLCMD;
		break;

	}

	return ret;
}
#endif

/* Fd device file operations callbacks */
static const struct v4l2_file_operations fd_fops = {
	.owner          = THIS_MODULE,
	.open           = msm_fd_open,
	.release        = msm_fd_release,
	.poll           = msm_fd_poll,
	.unlocked_ioctl = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = msm_fd_compat_ioctl32,
#endif
};

/*
 * msm_fd_querycap - V4l2 ioctl query capability handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @cap: Pointer to v4l2_capability struct need to be filled.
 */
static int msm_fd_querycap(struct file *file,
	void *fh, struct v4l2_capability *cap)
{
	cap->bus_info[0] = 0;
	strlcpy(cap->driver, MSM_FD_DRV_NAME, sizeof(cap->driver));
	strlcpy(cap->card, MSM_FD_DRV_NAME, sizeof(cap->card));
	cap->capabilities = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT;

	return 0;
}

/*
 * msm_fd_enum_fmt_vid_out - V4l2 ioctl enumerate format handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @f: Pointer to v4l2_fmtdesc struct need to be filled.
 */
static int msm_fd_enum_fmt_vid_out(struct file *file,
	void *fh, struct v4l2_fmtdesc *f)
{
	if (f->index > 0)
		return -EINVAL;

	f->pixelformat = V4L2_PIX_FMT_GREY;
	strlcpy(f->description, "8 Greyscale",
		sizeof(f->description));

	return 0;
}

/*
 * msm_fd_g_fmt - V4l2 ioctl get format handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @f: Pointer to v4l2_format struct need to be filled.
 */
static int msm_fd_g_fmt(struct file *file, void *fh, struct v4l2_format *f)
{
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(fh);

	return msm_fd_fill_format_from_ctx(f, ctx);
}

/*
 * msm_fd_try_fmt_vid_out - V4l2 ioctl try format handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @f: Pointer to v4l2_format struct.
 */
static int msm_fd_try_fmt_vid_out(struct file *file,
	void *fh, struct v4l2_format *f)
{
	int index;

	index = msm_fd_get_format_index(f);

	return msm_fd_fill_format_from_index(f, index);
}

/*
 * msm_fd_s_fmt_vid_out - V4l2 ioctl set format handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @f: Pointer to v4l2_format struct.
 */
static int msm_fd_s_fmt_vid_out(struct file *file,
	void *fh, struct v4l2_format *f)
{
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(fh);
	int index;

	index = msm_fd_get_format_index(f);

	msm_fd_fill_format_from_index(f, index);

	ctx->format.size = &fd_size[index];
	ctx->format.pixelformat = f->fmt.pix.pixelformat;
	ctx->format.bytesperline = f->fmt.pix.bytesperline;
	ctx->format.sizeimage = f->fmt.pix.sizeimage;

	/* Initialize crop */
	ctx->format.crop.top = 0;
	ctx->format.crop.left = 0;
	ctx->format.crop.width = fd_size[index].width;
	ctx->format.crop.height = fd_size[index].height;

	return 0;
}

/*
 * msm_fd_reqbufs - V4l2 ioctl request buffers handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @req: Pointer to v4l2_requestbuffer struct.
 */
static int msm_fd_reqbufs(struct file *file,
	void *fh, struct v4l2_requestbuffers *req)
{
	int ret;
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(fh);

	mutex_lock(&ctx->lock);
	ret = vb2_reqbufs(&ctx->vb2_q, req);
	mutex_unlock(&ctx->lock);
	return ret;
}

/*
 * msm_fd_qbuf - V4l2 ioctl queue buffer handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @pb: Pointer to v4l2_buffer struct.
 */
static int msm_fd_qbuf(struct file *file, void *fh,
	struct v4l2_buffer *pb)
{
	int ret;
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(fh);

	mutex_lock(&ctx->lock);
	ret = vb2_qbuf(&ctx->vb2_q, pb);
	mutex_unlock(&ctx->lock);
	return ret;

}

/*
 * msm_fd_dqbuf - V4l2 ioctl dequeue buffer handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @pb: Pointer to v4l2_buffer struct.
 */
static int msm_fd_dqbuf(struct file *file,
	void *fh, struct v4l2_buffer *pb)
{
	int ret;
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(fh);

	mutex_lock(&ctx->lock);
	ret = vb2_dqbuf(&ctx->vb2_q, pb, file->f_flags & O_NONBLOCK);
	mutex_unlock(&ctx->lock);
	return ret;
}

/*
 * msm_fd_streamon - V4l2 ioctl stream on handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @buf_type: V4l2 buffer type.
 */
static int msm_fd_streamon(struct file *file,
	void *fh, enum v4l2_buf_type buf_type)
{
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(fh);
	int ret;

	mutex_lock(&ctx->lock);
	ret = vb2_streamon(&ctx->vb2_q, buf_type);
	mutex_unlock(&ctx->lock);
	if (ret < 0)
		dev_err(ctx->fd_device->dev, "Stream on fails\n");

	return ret;
}

/*
 * msm_fd_streamoff - V4l2 ioctl stream off handler.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @buf_type: V4l2 buffer type.
 */
static int msm_fd_streamoff(struct file *file,
	void *fh, enum v4l2_buf_type buf_type)
{
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(fh);
	int ret;

	mutex_lock(&ctx->lock);
	ret = vb2_streamoff(&ctx->vb2_q, buf_type);
	mutex_unlock(&ctx->lock);
	if (ret < 0)
		dev_err(ctx->fd_device->dev, "Stream off fails\n");

	return ret;
}

/*
 * msm_fd_subscribe_event - V4l2 ioctl subscribe for event handler.
 * @fh: V4l2 File handle.
 * @sub: Pointer to v4l2_event_subscription containing event information.
 */
static int msm_fd_subscribe_event(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(fh);
	int ret;

	if (sub->type != MSM_EVENT_FD)
		return -EINVAL;

	ret = v4l2_event_subscribe(fh, sub, MSM_FD_MAX_RESULT_BUFS, NULL);
	if (!ret)
		atomic_set(&ctx->subscribed_for_event, 1);

	return ret;
}

/*
 * msm_fd_unsubscribe_event - V4l2 ioctl unsubscribe from event handler.
 * @fh: V4l2 File handle.
 * @sub: Pointer to v4l2_event_subscription containing event information.
 */
static int msm_fd_unsubscribe_event(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(fh);
	int ret;

	ret = v4l2_event_unsubscribe(fh, sub);
	if (!ret)
		atomic_set(&ctx->subscribed_for_event, 0);

	return ret;
}

/*
 * msm_fd_guery_ctrl - V4l2 ioctl query control.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @sub: Pointer to v4l2_queryctrl struct info need to be filled based on id.
 */
static int msm_fd_guery_ctrl(struct file *file, void *fh,
	struct v4l2_queryctrl *a)
{
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(fh);

	switch (a->id) {
	case V4L2_CID_FD_SPEED:
		a->type = V4L2_CTRL_TYPE_INTEGER;
		a->default_value = ctx->fd_device->clk_rates_num;
		a->minimum = 0;
		a->maximum = ctx->fd_device->clk_rates_num;
		a->step = 1;
		strlcpy(a->name, "msm fd face speed idx",
			sizeof(a->name));
		break;
	case V4L2_CID_FD_FACE_ANGLE:
		a->type = V4L2_CTRL_TYPE_INTEGER;
		a->default_value =  msm_fd_angle[MSM_FD_DEF_ANGLE_IDX];
		a->minimum = msm_fd_angle[0];
		a->maximum = msm_fd_angle[ARRAY_SIZE(msm_fd_angle) - 1];
		a->step = 1;
		strlcpy(a->name, "msm fd face angle ctrl",
			sizeof(a->name));
		break;
	case V4L2_CID_FD_FACE_DIRECTION:
		a->type = V4L2_CTRL_TYPE_INTEGER;
		a->default_value = msm_fd_dir[MSM_FD_DEF_DIR_IDX];
		a->minimum = msm_fd_dir[0];
		a->maximum = msm_fd_dir[ARRAY_SIZE(msm_fd_dir) - 1];
		a->step = 1;
		strlcpy(a->name, "msm fd face direction ctrl",
			sizeof(a->name));
		break;
	case V4L2_CID_FD_MIN_FACE_SIZE:
		a->type = V4L2_CTRL_TYPE_INTEGER;
		a->default_value = msm_fd_min_size[MSM_FD_DEF_MIN_SIZE_IDX];
		a->minimum = msm_fd_min_size[0];
		a->maximum = msm_fd_min_size[ARRAY_SIZE(msm_fd_min_size) - 1];
		a->step = 1;
		strlcpy(a->name, "msm fd minimum face size (pixels)",
			sizeof(a->name));
		break;
	case V4L2_CID_FD_DETECTION_THRESHOLD:
		a->type = V4L2_CTRL_TYPE_INTEGER;
		a->default_value = MSM_FD_DEF_THRESHOLD;
		a->minimum = 0;
		a->maximum = MSM_FD_MAX_THRESHOLD_VALUE;
		a->step = 1;
		strlcpy(a->name, "msm fd detection threshold",
			sizeof(a->name));
		break;
	case V4L2_CID_FD_WORK_MEMORY_SIZE:
		a->type = V4L2_CTRL_TYPE_INTEGER;
		a->default_value = fd_size[0].work_size;
		a->minimum = fd_size[(ARRAY_SIZE(fd_size) - 1)].work_size;
		a->maximum = fd_size[0].work_size;
		a->step = 1;
		strlcpy(a->name, "msm fd working memory size",
			sizeof(a->name));
		break;
	case V4L2_CID_FD_WORK_MEMORY_FD:
		a->type = V4L2_CTRL_TYPE_INTEGER;
		a->default_value = -1;
		a->minimum = 0;
		a->maximum = INT_MAX;
		a->step = 1;
		strlcpy(a->name, "msm fd ion fd of working memory",
			sizeof(a->name));
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * msm_fd_g_ctrl - V4l2 ioctl get control.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @sub: Pointer to v4l2_queryctrl struct need to be filled.
 */
static int msm_fd_g_ctrl(struct file *file, void *fh, struct v4l2_control *a)
{
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(fh);

	switch (a->id) {
	case V4L2_CID_FD_SPEED:
		a->value = ctx->settings.speed;
		break;
	case V4L2_CID_FD_FACE_ANGLE:
		a->value = msm_fd_angle[ctx->settings.angle_index];
		break;
	case V4L2_CID_FD_FACE_DIRECTION:
		a->value = msm_fd_dir[ctx->settings.direction_index];
		break;
	case V4L2_CID_FD_MIN_FACE_SIZE:
		a->value = msm_fd_min_size[ctx->settings.min_size_index];
		break;
	case V4L2_CID_FD_DETECTION_THRESHOLD:
		a->value = ctx->settings.threshold;
		break;
	case V4L2_CID_FD_WORK_MEMORY_SIZE:
		if (!ctx->format.size)
			return -EINVAL;

		a->value = ctx->format.size->work_size;
		break;
	case V4L2_CID_FD_WORK_MEMORY_FD:
		if (ctx->work_buf.fd == -1)
			return -EINVAL;

		a->value = ctx->work_buf.fd;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * msm_fd_s_ctrl - V4l2 ioctl set control.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @sub: Pointer to v4l2_queryctrl struct need to be set.
 */
static int msm_fd_s_ctrl(struct file *file, void *fh, struct v4l2_control *a)
{
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(fh);
	int idx;
	int ret;

	switch (a->id) {
	case V4L2_CID_FD_SPEED:
		if (a->value > ctx->fd_device->clk_rates_num - 1)
			a->value = ctx->fd_device->clk_rates_num - 1;
		else if (a->value < 0)
			a->value = 0;

		ctx->settings.speed = a->value;
		break;
	case V4L2_CID_FD_FACE_ANGLE:
		idx = msm_fd_get_idx_from_value(a->value, msm_fd_angle,
			ARRAY_SIZE(msm_fd_angle));

		ctx->settings.angle_index = idx;
		a->value = msm_fd_angle[ctx->settings.angle_index];
		break;
	case V4L2_CID_FD_FACE_DIRECTION:
		idx = msm_fd_get_idx_from_value(a->value, msm_fd_dir,
			ARRAY_SIZE(msm_fd_dir));

		ctx->settings.direction_index = idx;
		a->value = msm_fd_dir[ctx->settings.direction_index];
		break;
	case V4L2_CID_FD_MIN_FACE_SIZE:
		idx = msm_fd_get_idx_from_value(a->value, msm_fd_min_size,
			ARRAY_SIZE(msm_fd_min_size));

		ctx->settings.min_size_index = idx;
		a->value = msm_fd_min_size[ctx->settings.min_size_index];
		break;
	case V4L2_CID_FD_DETECTION_THRESHOLD:
		if (a->value > MSM_FD_MAX_THRESHOLD_VALUE)
			a->value = MSM_FD_MAX_THRESHOLD_VALUE;
		else if (a->value < 0)
			a->value = 0;

		ctx->settings.threshold = a->value;
		break;
	case V4L2_CID_FD_WORK_MEMORY_SIZE:
		if (!ctx->format.size)
			return -EINVAL;

		if (a->value < ctx->format.size->work_size)
			a->value = ctx->format.size->work_size;
		break;
	case V4L2_CID_FD_WORK_MEMORY_FD:
		mutex_lock(&ctx->fd_device->recovery_lock);
		if (ctx->work_buf.fd != -1)
			msm_fd_hw_unmap_buffer(&ctx->work_buf);
		if (a->value >= 0) {
			ret = msm_fd_hw_map_buffer(&ctx->mem_pool,
				a->value, &ctx->work_buf);
			if (ret < 0) {
				mutex_unlock(&ctx->fd_device->recovery_lock);
				return ret;
			}
		}
		mutex_unlock(&ctx->fd_device->recovery_lock);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * msm_fd_cropcap - V4l2 ioctl crop capabilities.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @sub: Pointer to v4l2_cropcap struct need to be set.
 */
static int msm_fd_cropcap(struct file *file, void *fh, struct v4l2_cropcap *a)
{
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(fh);

	if (!ctx->format.size) {
		dev_err(ctx->fd_device->dev, "Cropcap fails format missing\n");
		return -EINVAL;
	}

	a->bounds.top = 0;
	a->bounds.left = 0;
	a->bounds.width = ctx->format.size->width;
	a->bounds.height =  ctx->format.size->height;

	a->defrect = ctx->format.crop;

	a->pixelaspect.numerator = 1;
	a->pixelaspect.denominator = 1;

	return 0;
}

/*
 * msm_fd_g_crop - V4l2 ioctl get crop.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @sub: Pointer to v4l2_crop struct need to be set.
 */
static int msm_fd_g_crop(struct file *file, void *fh, struct v4l2_crop *crop)
{
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(fh);

	if (!ctx->format.size) {
		dev_err(ctx->fd_device->dev, "Get crop, format missing!\n");
		return -EINVAL;
	}

	crop->c = ctx->format.crop;

	return 0;
}

/*
 * msm_fd_s_crop - V4l2 ioctl set crop.
 * @file: Pointer to file struct.
 * @fh: V4l2 File handle.
 * @sub: Pointer to v4l2_crop struct need to be set.
 */
static int msm_fd_s_crop(struct file *file, void *fh,
	const struct v4l2_crop *crop)
{
	struct fd_ctx *ctx = msm_fd_ctx_from_fh(fh);
	int min_face_size;

	if (!ctx->format.size) {
		dev_err(ctx->fd_device->dev, "Get crop, format missing!\n");
		return -EINVAL;
	}

	/* First check that crop is valid */
	min_face_size = msm_fd_min_size[ctx->settings.min_size_index];

	if (crop->c.width < min_face_size || crop->c.height < min_face_size)
		return -EINVAL;

	if (crop->c.width + crop->c.left > ctx->format.size->width)
		return -EINVAL;

	if (crop->c.height + crop->c.top > ctx->format.size->height)
		return -EINVAL;

	ctx->format.crop = crop->c;

	return 0;
}

/* V4l2 ioctl handlers */
static const struct v4l2_ioctl_ops fd_ioctl_ops = {
	.vidioc_querycap          = msm_fd_querycap,
	.vidioc_enum_fmt_vid_out  = msm_fd_enum_fmt_vid_out,
	.vidioc_g_fmt_vid_out     = msm_fd_g_fmt,
	.vidioc_try_fmt_vid_out   = msm_fd_try_fmt_vid_out,
	.vidioc_s_fmt_vid_out     = msm_fd_s_fmt_vid_out,
	.vidioc_reqbufs           = msm_fd_reqbufs,
	.vidioc_qbuf              = msm_fd_qbuf,
	.vidioc_dqbuf             = msm_fd_dqbuf,
	.vidioc_streamon          = msm_fd_streamon,
	.vidioc_streamoff         = msm_fd_streamoff,
	.vidioc_queryctrl         = msm_fd_guery_ctrl,
	.vidioc_s_ctrl            = msm_fd_s_ctrl,
	.vidioc_g_ctrl            = msm_fd_g_ctrl,
	.vidioc_cropcap           = msm_fd_cropcap,
	.vidioc_g_crop            = msm_fd_g_crop,
	.vidioc_s_crop            = msm_fd_s_crop,
	.vidioc_subscribe_event   = msm_fd_subscribe_event,
	.vidioc_unsubscribe_event = msm_fd_unsubscribe_event,
	.vidioc_default           = msm_fd_private_ioctl,
};

/*
 * msm_fd_fill_results - Read and fill face detection result.
 * @fd: Pointer to fd device.
 * @face: Pointer of face data which information need to be stored.
 * @idx: Face number index need to be filled.
 */
static void msm_fd_fill_results(struct msm_fd_device *fd,
	struct msm_fd_face_data *face, int idx)
{
	int half_face_size;

	msm_fd_hw_get_result_angle_pose(fd, idx, &face->angle, &face->pose);

	msm_fd_hw_get_result_conf_size(fd, idx, &face->confidence,
		&face->face.width);
	face->face.height = face->face.width;

	face->face.left = msm_fd_hw_get_result_x(fd, idx);
	face->face.top = msm_fd_hw_get_result_y(fd, idx);

	half_face_size = (face->face.width >> 1);
	if (face->face.left > half_face_size)
		face->face.left -= half_face_size;
	else
		face->face.left = 0;

	half_face_size = (face->face.height >> 1);
	if (face->face.top > half_face_size)
		face->face.top -= half_face_size;
	else
		face->face.top = 0;
}

/*
 * msm_fd_wq_handler - Fd device workqueue handler.
 * @work: Pointer to work struct.
 *
 * This function is bottom half of fd irq what it does:
 *
 * - Stop the fd engine.
 * - Getter fd result and store in stats buffer.
 * - If available schedule next buffer for processing.
 * - Sent event to v4l2.
 * - Release buffer from v4l2 queue.
 */
static void msm_fd_wq_handler(struct work_struct *work)
{
	struct msm_fd_buffer *active_buf;
	struct msm_fd_stats *stats;
	struct msm_fd_event *fd_event;
	struct msm_fd_device *fd;
	struct fd_ctx *ctx;
	struct v4l2_event event;
	int i;

	fd = container_of(work, struct msm_fd_device, work);

	active_buf = msm_fd_hw_get_active_buffer(fd);
	if (!active_buf) {
		/* This should never happen, something completely wrong */
		dev_err(fd->dev, "Oops no active buffer empty queue\n");
		return;
	}
	ctx = vb2_get_drv_priv(active_buf->vb.vb2_queue);

	/* Increment sequence number, 0 means sequence is not valid */
	ctx->sequence++;
	if (unlikely(!ctx->sequence))
		ctx->sequence = 1;

	/* Fill face detection statistics */
	stats = &ctx->stats[ctx->sequence % MSM_FD_MAX_RESULT_BUFS];

	/* First mark stats as invalid */
	atomic_set(&stats->frame_id, 0);

	stats->face_cnt = msm_fd_hw_get_face_count(fd);
	for (i = 0; i < stats->face_cnt; i++)
		msm_fd_fill_results(fd, &stats->face_data[i], i);

	/* Stats are ready, set correct frame id */
	atomic_set(&stats->frame_id, ctx->sequence);

	/* If Recovery mode is on, we got IRQ after recovery, reset it */
	if (fd->recovery_mode) {
		fd->recovery_mode = 0;
		dev_dbg(fd->dev, "Got IRQ after Recovery\n");
	}

	/* We have the data from fd hw, we can start next processing */
	msm_fd_hw_schedule_next_buffer(fd);

	/* Return buffer to vb queue */
	active_buf->vb.v4l2_buf.sequence = ctx->fh.sequence;
	vb2_buffer_done(&active_buf->vb, VB2_BUF_STATE_DONE);

	/* Sent event */
	memset(&event, 0x00, sizeof(event));
	event.type = MSM_EVENT_FD;
	fd_event = (struct msm_fd_event *)event.u.data;
	fd_event->face_cnt = stats->face_cnt;
	fd_event->buf_index = active_buf->vb.v4l2_buf.index;
	fd_event->frame_id = ctx->sequence;
	v4l2_event_queue_fh(&ctx->fh, &event);

	/* Release buffer from the device */
	msm_fd_hw_buffer_done(fd, active_buf);
}

/*
 * fd_probe - Fd device probe method.
 * @pdev: Pointer fd platform device.
 */
static int fd_probe(struct platform_device *pdev)
{
	struct msm_fd_device *fd;
	int ret;

	/* Face detection device struct */
	fd = kzalloc(sizeof(struct msm_fd_device), GFP_KERNEL);
	if (!fd)
		return -ENOMEM;

	mutex_init(&fd->lock);
	spin_lock_init(&fd->slock);
	mutex_init(&fd->recovery_lock);
	init_completion(&fd->hw_halt_completion);
	INIT_LIST_HEAD(&fd->buf_queue);
	fd->pdev = pdev;
	fd->dev = &pdev->dev;

	/* Get resources */
	ret = msm_fd_hw_get_mem_resources(pdev, fd);
	if (ret < 0) {
		dev_err(&pdev->dev, "Fail get resources\n");
		ret = -ENODEV;
		goto error_mem_resources;
	}

	ret = msm_camera_get_regulator_info(pdev, &fd->vdd_info,
		&fd->num_reg);
	if (ret < 0) {
		dev_err(&pdev->dev, "Fail to get regulators\n");
		goto error_get_regulator;
	}
	ret = msm_camera_get_clk_info_and_rates(pdev, &fd->clk_info,
		&fd->clk, &fd->clk_rates, &fd->clk_rates_num, &fd->clk_num);
	if (ret < 0) {
		dev_err(&pdev->dev, "Fail to get clocks\n");
		goto error_get_clocks;
	}

	ret = msm_camera_register_bus_client(pdev, CAM_BUS_CLIENT_FD);
	if (ret < 0) {
		dev_err(&pdev->dev, "Fail to get bus\n");
		goto error_get_bus;
	}

	/* Get face detect hw before read engine revision */
	ret = msm_fd_hw_get(fd, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "Fail to get hw\n");
		goto error_hw_get_request_irq;
	}
	fd->hw_revision = msm_fd_hw_get_revision(fd);

	msm_fd_hw_put(fd);

	ret = msm_fd_hw_request_irq(pdev, fd, msm_fd_wq_handler);
	if (ret < 0) {
		dev_err(&pdev->dev, "Fail request irq\n");
		goto error_hw_get_request_irq;
	}

	/* v4l2 device */
	ret = v4l2_device_register(&pdev->dev, &fd->v4l2_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register v4l2 device\n");
		ret = -ENOENT;
		goto error_v4l2_register;
	}

	fd->video.fops  = &fd_fops;
	fd->video.ioctl_ops = &fd_ioctl_ops;
	fd->video.minor = -1;
	fd->video.release  = video_device_release;
	fd->video.v4l2_dev = &fd->v4l2_dev;
	fd->video.vfl_dir = VFL_DIR_TX;
	fd->video.vfl_type = VFL_TYPE_GRABBER;
	strlcpy(fd->video.name, MSM_FD_DRV_NAME, sizeof(fd->video.name));

	ret = video_register_device(&fd->video, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		v4l2_err(&fd->v4l2_dev, "Failed to register video device\n");
		goto error_video_register;
	}

	video_set_drvdata(&fd->video, fd);

	platform_set_drvdata(pdev, fd);

	return 0;

error_video_register:
	v4l2_device_unregister(&fd->v4l2_dev);
error_v4l2_register:
	msm_fd_hw_release_irq(fd);
error_hw_get_request_irq:
	msm_camera_unregister_bus_client(CAM_BUS_CLIENT_FD);
error_get_bus:
	msm_camera_put_clk_info_and_rates(pdev, &fd->clk_info,
		&fd->clk, &fd->clk_rates, fd->clk_rates_num, fd->clk_num);
error_get_clocks:
	msm_camera_put_regulators(pdev, &fd->vdd_info, fd->num_reg);
error_get_regulator:
	msm_fd_hw_release_mem_resources(fd);
error_mem_resources:
	kfree(fd);
	return ret;
}

/*
 * fd_device_remove - Fd device remove method.
 * @pdev: Pointer fd platform device.
 */
static int fd_device_remove(struct platform_device *pdev)
{
	struct msm_fd_device *fd;

	fd = platform_get_drvdata(pdev);
	if (fd == NULL) {
		dev_err(&pdev->dev, "Can not get fd drvdata\n");
		return 0;
	}
	video_unregister_device(&fd->video);
	v4l2_device_unregister(&fd->v4l2_dev);
	msm_fd_hw_release_irq(fd);
	msm_camera_unregister_bus_client(CAM_BUS_CLIENT_FD);
	msm_camera_put_clk_info_and_rates(pdev, &fd->clk_info,
		&fd->clk, &fd->clk_rates, fd->clk_rates_num, fd->clk_num);
	msm_camera_put_regulators(pdev, &fd->vdd_info, fd->num_reg);
	msm_fd_hw_release_mem_resources(fd);
	kfree(fd);

	return 0;
}

/* Device tree match struct */
static const struct of_device_id msm_fd_dt_match[] = {
	{.compatible = "qcom,face-detection"},
	{}
};

/* Fd platform driver definition */
static struct platform_driver fd_driver = {
	.probe = fd_probe,
	.remove = fd_device_remove,
	.driver = {
		.name = MSM_FD_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_fd_dt_match,
	},
};

static int __init msm_fd_init_module(void)
{
	return platform_driver_register(&fd_driver);
}

static void __exit msm_fd_exit_module(void)
{
	platform_driver_unregister(&fd_driver);
}

module_init(msm_fd_init_module);
module_exit(msm_fd_exit_module);
MODULE_DESCRIPTION("MSM FD driver");
MODULE_LICENSE("GPL v2");
