/*
 * Support for Medifield PNW Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
 *
 * Copyright (c) 2010 Silicon Hive www.siliconhive.com.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/module.h>
#include <linux/pm_runtime.h>

#include <media/v4l2-ioctl.h>
#include <media/videobuf-vmalloc.h>

#include "atomisp_cmd.h"
#include "atomisp_common.h"
#include "atomisp_fops.h"
#include "atomisp_internal.h"
#include "atomisp_ioctl.h"
#include "atomisp_compat.h"
#include "atomisp_subdev.h"
#include "atomisp-regs.h"
#include "hmm/hmm.h"

#include "hrt/hive_isp_css_mm_hrt.h"

#ifndef CSS21
#include "host/mmu_local.h"
#else
#include "type_support.h"
#endif
#include "device_access/device_access.h"
#include "memory_access/memory_access.h"

#include "atomisp_acc.h"
#include "atomisp_fw.h"

#define ISP_LEFT_PAD			128	/* equal to 2*NWAY */

/*
 * input image data, and current frame resolution for test
 */
#define	ISP_PARAM_MMAP_OFFSET	0xfffff000

#define MAGIC_CHECK(is, should)	\
	if (unlikely((is) != (should))) { \
		printk(KERN_ERR "magic mismatch: %x (expected %x)\n", \
			is, should); \
		BUG(); \
	}

/*
 * Videobuf ops
 */
static int atomisp_buf_setup(struct videobuf_queue *vq, unsigned int *count,
			     unsigned int *size)
{
	struct atomisp_video_pipe *pipe = vq->priv_data;

	*size = pipe->pix.sizeimage;

	return 0;
}

static int atomisp_buf_prepare(struct videobuf_queue *vq,
			       struct videobuf_buffer *vb,
			       enum v4l2_field field)
{
	struct atomisp_video_pipe *pipe = vq->priv_data;

	vb->size = pipe->pix.sizeimage;
	vb->width = pipe->pix.width;
	vb->height = pipe->pix.height;
	vb->field = field;
	vb->state = VIDEOBUF_PREPARED;

	return 0;
}

int atomisp_q_video_buffers_to_css(struct atomisp_sub_device *asd,
			     struct atomisp_video_pipe *pipe,
			     enum atomisp_input_stream_id stream_id,
			     enum atomisp_css_buffer_type css_buf_type,
			     enum atomisp_css_pipe_id css_pipe_id)
{
	struct videobuf_vmalloc_memory *vm_mem;
	unsigned long irqflags;
	int err;

	while (pipe->buffers_in_css < ATOMISP_CSS_Q_DEPTH) {
		struct videobuf_buffer *vb;

		spin_lock_irqsave(&pipe->irq_lock, irqflags);
		if (list_empty(&pipe->activeq)) {
			spin_unlock_irqrestore(&pipe->irq_lock, irqflags);
			return -EINVAL;
		}
		vb = list_entry(pipe->activeq.next,
				struct videobuf_buffer, queue);
		list_del_init(&vb->queue);
		vb->state = VIDEOBUF_ACTIVE;
		spin_unlock_irqrestore(&pipe->irq_lock, irqflags);

		vm_mem = vb->priv;

		err = atomisp_q_video_buffer_to_css(asd, vm_mem, stream_id,
						css_buf_type, css_pipe_id);
		if (err) {
			spin_lock_irqsave(&pipe->irq_lock, irqflags);
			list_add_tail(&vb->queue, &pipe->activeq);
			vb->state = VIDEOBUF_QUEUED;
			spin_unlock_irqrestore(&pipe->irq_lock, irqflags);
			dev_err(asd->isp->dev, "%s, css q fails: %d\n",
					__func__, err);
			return -EINVAL;
		}
		pipe->buffers_in_css++;
	}
	return 0;
}

int atomisp_q_s3a_buffers_to_css(struct atomisp_sub_device *asd,
				enum atomisp_input_stream_id stream_id,
				enum atomisp_css_pipe_id css_pipe_id)
{
	struct atomisp_s3a_buf *s3a_buf;

	if (list_empty(&asd->s3a_stats)) {
		WARN(1, "%s: No s3a buffers available!\n", __func__);
		return -EINVAL;
	}

	while (asd->s3a_bufs_in_css[css_pipe_id] < ATOMISP_CSS_Q_DEPTH) {
		s3a_buf = list_entry(asd->s3a_stats.next,
				struct atomisp_s3a_buf, list);
		list_move_tail(&s3a_buf->list, &asd->s3a_stats);

		if (atomisp_q_s3a_buffer_to_css(asd, s3a_buf,
						stream_id, css_pipe_id))
			return -EINVAL;

		asd->s3a_bufs_in_css[css_pipe_id]++;
	}

	return 0;
}

int atomisp_q_dis_buffers_to_css(struct atomisp_sub_device *asd,
				enum atomisp_input_stream_id stream_id,
				enum atomisp_css_pipe_id css_pipe_id)
{
	if (list_empty(&asd->dis_stats)) {
		WARN(1, "%s: No dis buffers available!\n", __func__);
		return -EINVAL;
	}

	while (asd->dis_bufs_in_css < ATOMISP_CSS_Q_DEPTH) {
		struct atomisp_dis_buf *dis_buf =
			list_entry(asd->dis_stats.next,
				   struct atomisp_dis_buf, list);
		list_move_tail(&dis_buf->list, &asd->dis_stats);

		if (atomisp_q_dis_buffer_to_css(asd, dis_buf,
						stream_id, css_pipe_id))
			return -EINVAL;

		asd->dis_bufs_in_css++;
	}

	return 0;
}

static int atomisp_get_css_buf_type(struct atomisp_sub_device *asd,
				    enum atomisp_css_pipe_id pipe_id,
				    uint16_t source_pad)
{
	if (pipe_id == CSS_PIPE_ID_COPY ||
	    source_pad == ATOMISP_SUBDEV_PAD_SOURCE_CAPTURE ||
	    source_pad == ATOMISP_SUBDEV_PAD_SOURCE_VIDEO ||
	    (source_pad == ATOMISP_SUBDEV_PAD_SOURCE_PREVIEW &&
	     asd->run_mode->val != ATOMISP_RUN_MODE_VIDEO))
		return CSS_BUFFER_TYPE_OUTPUT_FRAME;
	else
		return CSS_BUFFER_TYPE_VF_OUTPUT_FRAME;
}

/* queue all available buffers to css */
int atomisp_qbuffers_to_css(struct atomisp_sub_device *asd)
{
	enum atomisp_css_buffer_type buf_type;
	enum atomisp_css_pipe_id css_capture_pipe_id = CSS_PIPE_ID_NUM;
	enum atomisp_css_pipe_id css_preview_pipe_id = CSS_PIPE_ID_NUM;
	enum atomisp_css_pipe_id css_video_pipe_id = CSS_PIPE_ID_NUM;
	enum atomisp_input_stream_id input_stream_id;
	struct atomisp_video_pipe *capture_pipe = NULL;
	struct atomisp_video_pipe *vf_pipe = NULL;
	struct atomisp_video_pipe *preview_pipe = NULL;
	struct atomisp_video_pipe *video_pipe = NULL;
	bool raw_mode = atomisp_is_mbuscode_raw(
			    asd->fmt[asd->capture_pad].fmt.code);

	if (asd->vfpp->val == ATOMISP_VFPP_DISABLE_SCALER) {
		video_pipe = &asd->video_out_video_capture;
		css_video_pipe_id = CSS_PIPE_ID_VIDEO;
	} else if (asd->vfpp->val == ATOMISP_VFPP_DISABLE_LOWLAT) {
		preview_pipe = &asd->video_out_capture;
		css_preview_pipe_id = CSS_PIPE_ID_CAPTURE;
	} else if (asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO) {
		if (asd->continuous_mode->val) {
			capture_pipe = &asd->video_out_capture;
			vf_pipe = &asd->video_out_vf;
			css_capture_pipe_id = CSS_PIPE_ID_CAPTURE;
		}
		video_pipe = &asd->video_out_video_capture;
		preview_pipe = &asd->video_out_preview;
		css_video_pipe_id = CSS_PIPE_ID_VIDEO;
		css_preview_pipe_id = CSS_PIPE_ID_VIDEO;
	} else if (asd->continuous_mode->val) {
		capture_pipe = &asd->video_out_capture;
		vf_pipe = &asd->video_out_vf;
		preview_pipe = &asd->video_out_preview;

		css_preview_pipe_id = CSS_PIPE_ID_PREVIEW;
		css_capture_pipe_id = CSS_PIPE_ID_CAPTURE;
	} else if (asd->run_mode->val == ATOMISP_RUN_MODE_PREVIEW) {
		preview_pipe = &asd->video_out_preview;
		css_preview_pipe_id = CSS_PIPE_ID_PREVIEW;
	} else {
		/* ATOMISP_RUN_MODE_STILL_CAPTURE */
		capture_pipe = &asd->video_out_capture;
		if (!raw_mode)
			vf_pipe = &asd->video_out_vf;
		css_capture_pipe_id = CSS_PIPE_ID_CAPTURE;
	}

#ifdef ISP2401_NEW_INPUT_SYSTEM
	if (asd->isp->inputs[asd->input_curr].type == SOC_CAMERA || raw_mode) {
		css_capture_pipe_id = CSS_PIPE_ID_COPY;
		css_preview_pipe_id = CSS_PIPE_ID_COPY;
	}
#endif

	if (capture_pipe) {
		buf_type = atomisp_get_css_buf_type(
			asd, css_capture_pipe_id,
			atomisp_subdev_source_pad(&capture_pipe->vdev));
#ifdef CSS20
		if (asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO &&
			asd->stream_env[ATOMISP_INPUT_STREAM_VIDEO].stream)
			input_stream_id = ATOMISP_INPUT_STREAM_VIDEO;
		else
#endif
			input_stream_id = ATOMISP_INPUT_STREAM_GENERAL;
		atomisp_q_video_buffers_to_css(asd, capture_pipe,
					       input_stream_id,
					       buf_type, css_capture_pipe_id);
	}

	if (vf_pipe) {
		buf_type = atomisp_get_css_buf_type(
			asd, css_capture_pipe_id,
			atomisp_subdev_source_pad(&vf_pipe->vdev));
#ifdef CSS20
		if (asd->stream_env[ATOMISP_INPUT_STREAM_POSTVIEW].stream)
			input_stream_id = ATOMISP_INPUT_STREAM_POSTVIEW;
		else
#endif
			input_stream_id = ATOMISP_INPUT_STREAM_GENERAL;
		atomisp_q_video_buffers_to_css(asd, vf_pipe,
					       input_stream_id,
					       buf_type, css_capture_pipe_id);
	}

	if (preview_pipe) {
		buf_type = atomisp_get_css_buf_type(
			asd, css_preview_pipe_id,
			atomisp_subdev_source_pad(&preview_pipe->vdev));
#ifdef CSS20
		if (asd->stream_env[ATOMISP_INPUT_STREAM_PREVIEW].stream)
			input_stream_id = ATOMISP_INPUT_STREAM_PREVIEW;
		else
#endif
			input_stream_id = ATOMISP_INPUT_STREAM_GENERAL;
		atomisp_q_video_buffers_to_css(asd, preview_pipe,
					       input_stream_id,
					       buf_type, css_preview_pipe_id);
	}

	if (video_pipe) {
		buf_type = atomisp_get_css_buf_type(
			asd, css_video_pipe_id,
			atomisp_subdev_source_pad(&video_pipe->vdev));
#ifdef CSS20
		if (asd->stream_env[ATOMISP_INPUT_STREAM_VIDEO].stream)
			input_stream_id = ATOMISP_INPUT_STREAM_VIDEO;
		else
#endif
			input_stream_id = ATOMISP_INPUT_STREAM_GENERAL;
		atomisp_q_video_buffers_to_css(asd, video_pipe,
					       input_stream_id,
					       buf_type, css_video_pipe_id);
	}


	if (asd->params.curr_grid_info.s3a_grid.enable) {
		if (css_capture_pipe_id < CSS_PIPE_ID_NUM)
			atomisp_q_s3a_buffers_to_css(asd,
					ATOMISP_INPUT_STREAM_GENERAL,
					css_capture_pipe_id);
		if (css_preview_pipe_id < CSS_PIPE_ID_NUM)
			atomisp_q_s3a_buffers_to_css(asd,
					ATOMISP_INPUT_STREAM_GENERAL,
					css_preview_pipe_id);
		if (css_video_pipe_id < CSS_PIPE_ID_NUM)
			atomisp_q_s3a_buffers_to_css(asd,
					ATOMISP_INPUT_STREAM_GENERAL,
					css_video_pipe_id);
	}

	if (asd->params.curr_grid_info.dvs_grid.enable)
		atomisp_q_dis_buffers_to_css(asd,
					ATOMISP_INPUT_STREAM_GENERAL,
					css_video_pipe_id);

	return 0;
}

static void atomisp_buf_queue(struct videobuf_queue *vq,
			      struct videobuf_buffer *vb)
{
	struct atomisp_video_pipe *pipe = vq->priv_data;

	list_add_tail(&vb->queue, &pipe->activeq);
	vb->state = VIDEOBUF_QUEUED;
}

static void atomisp_buf_release(struct videobuf_queue *vq,
				struct videobuf_buffer *vb)
{
	vb->state = VIDEOBUF_NEEDS_INIT;
	atomisp_videobuf_free_buf(vb);
}

static int atomisp_buf_setup_output(struct videobuf_queue *vq,
				    unsigned int *count, unsigned int *size)
{
	struct atomisp_video_pipe *pipe = vq->priv_data;

	*size = pipe->pix.sizeimage;

	return 0;
}

static int atomisp_buf_prepare_output(struct videobuf_queue *vq,
				      struct videobuf_buffer *vb,
				      enum v4l2_field field)
{
	struct atomisp_video_pipe *pipe = vq->priv_data;

	vb->size = pipe->pix.sizeimage;
	vb->width = pipe->pix.width;
	vb->height = pipe->pix.height;
	vb->field = field;
	vb->state = VIDEOBUF_PREPARED;

	return 0;
}

static void atomisp_buf_queue_output(struct videobuf_queue *vq,
				     struct videobuf_buffer *vb)
{
	struct atomisp_video_pipe *pipe = vq->priv_data;

	list_add_tail(&vb->queue, &pipe->activeq_out);
	vb->state = VIDEOBUF_QUEUED;
}

static void atomisp_buf_release_output(struct videobuf_queue *vq,
				       struct videobuf_buffer *vb)
{
	videobuf_vmalloc_free(vb);
	vb->state = VIDEOBUF_NEEDS_INIT;
}

static struct videobuf_queue_ops videobuf_qops = {
	.buf_setup	= atomisp_buf_setup,
	.buf_prepare	= atomisp_buf_prepare,
	.buf_queue	= atomisp_buf_queue,
	.buf_release	= atomisp_buf_release,
};

static struct videobuf_queue_ops videobuf_qops_output = {
	.buf_setup	= atomisp_buf_setup_output,
	.buf_prepare	= atomisp_buf_prepare_output,
	.buf_queue	= atomisp_buf_queue_output,
	.buf_release	= atomisp_buf_release_output,
};

static int atomisp_init_pipe(struct atomisp_video_pipe *pipe)
{
	/* init locks */
	spin_lock_init(&pipe->irq_lock);

	videobuf_queue_vmalloc_init(&pipe->capq, &videobuf_qops, NULL,
				    &pipe->irq_lock,
				    V4L2_BUF_TYPE_VIDEO_CAPTURE,
				    V4L2_FIELD_NONE,
				    sizeof(struct atomisp_buffer), pipe,
				    NULL);	/* ext_lock: NULL */

	videobuf_queue_vmalloc_init(&pipe->outq, &videobuf_qops_output, NULL,
				    &pipe->irq_lock,
				    V4L2_BUF_TYPE_VIDEO_OUTPUT,
				    V4L2_FIELD_NONE,
				    sizeof(struct atomisp_buffer), pipe,
				    NULL);	/* ext_lock: NULL */

	INIT_LIST_HEAD(&pipe->activeq);
	INIT_LIST_HEAD(&pipe->activeq_out);

	return 0;
}

static void atomisp_dev_init_struct(struct atomisp_device *isp)
{
	unsigned int i;

	isp->sw_contex.file_input = 0;
	isp->need_gfx_throttle = true;
	isp->isp_fatal_error = false;
	isp->mipi_frame_size = 0;

	for (i = 0; i < isp->input_cnt; i++)
		isp->inputs[i].asd = NULL;
	/*
	 * For Merrifield, frequency is scalable.
	 * After boot-up, the default frequency is 200MHz.
	 * For Medfield/Clovertrail, all running at 320MHz
	 */
	if (IS_ISP24XX(isp))
		isp->sw_contex.running_freq = ISP_FREQ_200MHZ;
	else
		isp->sw_contex.running_freq = ISP_FREQ_320MHZ;
}

static void atomisp_subdev_init_struct(struct atomisp_sub_device *asd)
{
	v4l2_ctrl_s_ctrl(asd->run_mode, ATOMISP_RUN_MODE_STILL_CAPTURE);
	asd->params.color_effect = V4L2_COLORFX_NONE;
	asd->params.bad_pixel_en = 1;
	asd->params.gdc_cac_en = 0;
	asd->params.video_dis_en = 0;
	asd->params.sc_en = 0;
	asd->params.fpn_en = 0;
	asd->params.xnr_en = 0;
	asd->params.false_color = 0;
	asd->params.online_process = 1;
	asd->params.yuv_ds_en = 0;
#ifdef CSS20
	asd->params.dvs_6axis = NULL;
#endif
	asd->params.offline_parm.num_captures = 1;
	asd->params.offline_parm.skip_frames = 0;
	asd->params.offline_parm.offset = 0;
	asd->delayed_init = ATOMISP_DELAYED_INIT_NOT_QUEUED;
	/* Add for channel */
	asd->input_curr = 0;

	asd->mipi_frame_size = 0;

	atomisp_css_init_struct(asd);
}
/*
 * file operation functions
 */
unsigned int atomisp_subdev_users(struct atomisp_sub_device *asd)
{
	return asd->video_out_preview.users +
	       asd->video_out_vf.users +
	       asd->video_out_capture.users +
	       asd->video_out_video_capture.users +
	       asd->video_in.users;
}

unsigned int atomisp_dev_users(struct atomisp_device *isp)
{
	unsigned int i, sum;
	for (i = 0, sum = 0; i < isp->num_of_streams; i++)
		sum += atomisp_subdev_users(&isp->asd[i]);

	return sum;
}

static int atomisp_open(struct file *file)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);
	struct atomisp_sub_device *asd = pipe->asd;
	int ret;

	dev_dbg(isp->dev, "open device %s\n", vdev->name);

	mutex_lock(&isp->mutex);

	if (!isp->input_cnt) {
		dev_err(isp->dev, "no camera attached\n");
		ret = -EINVAL;
		goto error;
	}

	if (pipe->users)
		goto done;

	ret = atomisp_init_pipe(pipe);
	if (ret)
		goto error;


	if (atomisp_dev_users(isp)) {
		dev_err(isp->dev, "skip init isp in open\n");
		goto init_subdev;
	}

	/* runtime power management, turn on ISP */
	ret = pm_runtime_get_sync(vdev->v4l2_dev->dev);
	if (ret < 0) {
		dev_err(isp->dev, "Failed to power on device\n");
		goto error;
	}

	if (dypool_enable) {
		ret = hmm_pool_register(dypool_pgnr, HMM_POOL_TYPE_DYNAMIC);
		if (ret)
			dev_err(isp->dev, "Failed to register dynamic memory pool.\n");
	}

	/* Init ISP */
	if (atomisp_css_init(isp)) {
		ret = -EINVAL;
		goto error;
	}

	/* Initialize the CSS debug trace verbosity level. To change
	 * the verbosity level, change the definition of this macro
	 * up in the file
	 */
	atomisp_css_debug_set_dtrace_level(CSS_DTRACE_VERBOSITY_LEVEL);

	atomisp_dev_init_struct(isp);

	ret = v4l2_subdev_call(isp->flash, core, s_power, 1);
	if (ret < 0 && ret != -ENODEV && ret != -ENOIOCTLCMD) {
		dev_err(isp->dev, "Failed to power-on flash\n");
		goto error;
	}

init_subdev: /* For CTP CSS1.5, below init depends on css global init */
	if (atomisp_subdev_users(asd))
		goto done;

	atomisp_subdev_init_struct(asd);

done:
	pipe->users++;
	mutex_unlock(&isp->mutex);
	return 0;

error:
	hmm_pool_unregister(HMM_POOL_TYPE_DYNAMIC);
	pm_runtime_put(vdev->v4l2_dev->dev);
	mutex_unlock(&isp->mutex);
	return ret;
}

static int atomisp_release(struct file *file)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);
	struct atomisp_sub_device *asd = pipe->asd;
	struct v4l2_requestbuffers req;
	struct v4l2_subdev_fh fh;
	int ret = 0;

	v4l2_fh_init(&fh.vfh, vdev);

	req.count = 0;
	if (isp == NULL)
		return -EBADF;

	mutex_lock(&isp->streamoff_mutex);
	mutex_lock(&isp->mutex);

	dev_dbg(isp->dev, "release device %s\n", vdev->name);

	pipe->users--;

	if (pipe->capq.streaming &&
	    __atomisp_streamoff(file, NULL, V4L2_BUF_TYPE_VIDEO_CAPTURE)) {
		dev_err(isp->dev,
			"atomisp_streamoff failed on release, driver bug");
		goto done;
	}

	if (pipe->users)
		goto done;

	if (__atomisp_reqbufs(file, NULL, &req)) {
		dev_err(isp->dev,
			"atomisp_reqbufs failed on release, driver bug");
		goto done;
	}

	if (pipe->outq.bufs[0]) {
		mutex_lock(&pipe->outq.vb_lock);
		videobuf_queue_cancel(&pipe->outq);
		mutex_unlock(&pipe->outq.vb_lock);
	}

	/*
	 * A little trick here:
	 * file injection input resolution is recorded in the sink pad,
	 * therefore can not be cleared when releaseing one device node.
	 * The sink pad setting can only be cleared when all device nodes
	 * get released.
	 */
	if (!isp->sw_contex.file_input && asd->fmt_auto->val) {
		struct v4l2_mbus_framefmt isp_sink_fmt = { 0 };
		atomisp_subdev_set_ffmt(
			&asd->subdev, &fh,
			V4L2_SUBDEV_FORMAT_ACTIVE, ATOMISP_SUBDEV_PAD_SINK,
			&isp_sink_fmt);
	}

	if (atomisp_subdev_users(asd))
		goto done;

	/* clear the sink pad for file input */
	if (isp->sw_contex.file_input && asd->fmt_auto->val) {
		struct v4l2_mbus_framefmt isp_sink_fmt = { 0 };
		atomisp_subdev_set_ffmt(&asd->subdev, &fh,
					V4L2_SUBDEV_FORMAT_ACTIVE,
					ATOMISP_SUBDEV_PAD_SINK, &isp_sink_fmt);
	}

	atomisp_free_3a_dis_buffers(asd);
	atomisp_free_internal_buffers(asd);
	ret = v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
				       core, s_power, 0);
	if (ret)
		dev_warn(isp->dev, "Failed to power-off sensor\n");

	/* clear the asd field to show this camera is not used */
	isp->inputs[asd->input_curr].asd = NULL;

	if (atomisp_dev_users(isp))
		goto done;

	del_timer_sync(&isp->wdt);
	atomisp_acc_release(isp);
	atomisp_free_all_shading_tables(isp);
#ifdef CSS20
	atomisp_destroy_pipes_stream_force(asd);
#endif
	atomisp_css_uninit(isp);
#ifndef CSS20
	hrt_isp_css_mm_clear();
#else /* CSS20 */
	hmm_cleanup_mmu_l2();
#endif /* CSS20 */
	hmm_pool_unregister(HMM_POOL_TYPE_DYNAMIC);

	ret = v4l2_subdev_call(isp->flash, core, s_power, 0);
	if (ret < 0 && ret != -ENODEV && ret != -ENOIOCTLCMD)
		dev_warn(isp->dev, "Failed to power-off flash\n");

	if (pm_runtime_put_sync(vdev->v4l2_dev->dev) < 0)
		dev_err(isp->dev, "Failed to power off device\n");

done:
	mutex_unlock(&isp->mutex);
	mutex_unlock(&isp->streamoff_mutex);

	return 0;
}

/*
 * Memory help functions for image frame and private parameters
 */
static int do_isp_mm_remap(struct atomisp_device *isp,
			   struct vm_area_struct *vma,
			   ia_css_ptr isp_virt, u32 host_virt, u32 pgnr)
{
	u32 pfn;

	while (pgnr) {
		pfn = hmm_virt_to_phys(isp_virt) >> PAGE_SHIFT;
		if (remap_pfn_range(vma, host_virt, pfn,
				    PAGE_SIZE, PAGE_SHARED)) {
			dev_err(isp->dev, "remap_pfn_range err.\n");
			return -EAGAIN;
		}

		isp_virt += PAGE_SIZE;
		host_virt += PAGE_SIZE;
		pgnr--;
	}

	return 0;
}

static int frame_mmap(struct atomisp_device *isp,
	const struct atomisp_css_frame *frame, struct vm_area_struct *vma)
{
	ia_css_ptr isp_virt;
	u32 host_virt;
	u32 pgnr;

	if (!frame) {
		dev_err(isp->dev, "%s: NULL frame pointer.\n", __func__);
		return -EINVAL;
	}

	host_virt = vma->vm_start;
	isp_virt = frame->data;
	atomisp_get_frame_pgnr(isp, frame, &pgnr);

	if (do_isp_mm_remap(isp, vma, isp_virt, host_virt, pgnr))
		return -EAGAIN;

	return 0;
}

int atomisp_videobuf_mmap_mapper(struct videobuf_queue *q,
	struct vm_area_struct *vma)
{
	u32 offset = vma->vm_pgoff << PAGE_SHIFT;
	int ret = -EINVAL, i;
	struct atomisp_device *isp =
		((struct atomisp_video_pipe *)(q->priv_data))->isp;
	struct videobuf_vmalloc_memory *vm_mem;
	struct videobuf_mapping *map;

	MAGIC_CHECK(q->int_ops->magic, MAGIC_QTYPE_OPS);
	if (!(vma->vm_flags & VM_WRITE) || !(vma->vm_flags & VM_SHARED)) {
		dev_err(isp->dev, "map appl bug: PROT_WRITE and MAP_SHARED "
				  "are required\n");
		return -EINVAL;
	}

	mutex_lock(&q->vb_lock);
	for (i = 0; i < VIDEO_MAX_FRAME; i++) {
		struct videobuf_buffer *buf = q->bufs[i];
		if (buf == NULL)
			continue;

		map = kzalloc(sizeof(struct videobuf_mapping), GFP_KERNEL);
		if (map == NULL) {
			mutex_unlock(&q->vb_lock);
			return -ENOMEM;
		}

		buf->map = map;
		map->q = q;

		buf->baddr = vma->vm_start;

		if (buf && buf->memory == V4L2_MEMORY_MMAP &&
		    buf->boff == offset) {
			vm_mem = buf->priv;
			ret = frame_mmap(isp, vm_mem->vaddr, vma);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
			vma->vm_flags |= VM_IO|VM_DONTEXPAND|VM_DONTDUMP;
#else
			vma->vm_flags |= VM_DONTEXPAND | VM_RESERVED;
#endif
			break;
		}
	}
	mutex_unlock(&q->vb_lock);

	return ret;
}

/* The input frame contains left and right padding that need to be removed.
 * There is always ISP_LEFT_PAD padding on the left side.
 * There is also padding on the right (padded_width - width).
 */
static int remove_pad_from_frame(struct atomisp_device *isp,
		struct atomisp_css_frame *in_frame, __u32 width, __u32 height)
{
	unsigned int i;
	unsigned short *buffer;
	int ret = 0;
	ia_css_ptr load = in_frame->data;
	ia_css_ptr store = load;

	buffer = kmalloc(width*sizeof(load), GFP_KERNEL);
	if (!buffer) {
		dev_err(isp->dev, "out of memory.\n");
		return -ENOMEM;
	}

//#define ISP_LEFT_PAD			128	/* equal to 2*NWAY */
	load += ISP_LEFT_PAD;
	for (i = 0; i < height; i++) {
		ret = hrt_isp_css_mm_load(load, buffer, width*sizeof(load));
		if (ret < 0)
			goto remove_pad_error;

		ret = hrt_isp_css_mm_store(store, buffer, width*sizeof(store));
		if (ret < 0)
			goto remove_pad_error;

		load  += in_frame->info.padded_width;
		store += width;
	}

remove_pad_error:
	kfree(buffer);
	return ret;
}

static int atomisp_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);
	struct atomisp_sub_device *asd = pipe->asd;
	struct atomisp_css_frame *raw_virt_addr;
	u32 start = vma->vm_start;
	u32 end = vma->vm_end;
	u32 size = end - start;
	u32 origin_size, new_size;
	int ret;

	if (!(vma->vm_flags & (VM_WRITE | VM_READ)))
		return -EACCES;

	if (!(vma->vm_flags & VM_SHARED))
		return -EINVAL;

	mutex_lock(&isp->mutex);
	new_size = pipe->pix.width * pipe->pix.height * 2;

	/* mmap for ISP offline raw data */
	if (atomisp_subdev_source_pad(vdev)
	    == ATOMISP_SUBDEV_PAD_SOURCE_CAPTURE &&
	    vma->vm_pgoff == (ISP_PARAM_MMAP_OFFSET >> PAGE_SHIFT)) {
		if (asd->params.online_process != 0) {
			ret = -EINVAL;
			goto error;
		}
		raw_virt_addr = asd->raw_output_frame;
		if (raw_virt_addr == NULL) {
			dev_err(isp->dev, "Failed to request RAW frame\n");
			ret = -EINVAL;
			goto error;
		}

		ret = remove_pad_from_frame(isp, raw_virt_addr,
				      pipe->pix.width, pipe->pix.height);
		if (ret < 0) {
			dev_err(isp->dev, "remove pad failed.\n");
			goto error;
		}
		origin_size = raw_virt_addr->data_bytes;
		raw_virt_addr->data_bytes = new_size;

		if (size != PAGE_ALIGN(new_size)) {
			dev_err(isp->dev, "incorrect size for mmap ISP"
				 " Raw Frame\n");
			ret = -EINVAL;
			goto error;
		}

		if (frame_mmap(isp, raw_virt_addr, vma)) {
			dev_err(isp->dev, "frame_mmap failed.\n");
			raw_virt_addr->data_bytes = origin_size;
			ret = -EAGAIN;
			goto error;
		}
		raw_virt_addr->data_bytes = origin_size;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
		vma->vm_flags |= VM_IO|VM_DONTEXPAND|VM_DONTDUMP;
#else
		vma->vm_flags |= VM_RESERVED;
#endif
		mutex_unlock(&isp->mutex);
		return 0;
	}

	/*
	 * mmap for normal frames
	 */
	if (size != pipe->pix.sizeimage) {
		dev_err(isp->dev, "incorrect size for mmap ISP frames\n");
		ret = -EINVAL;
		goto error;
	}
	mutex_unlock(&isp->mutex);

	return atomisp_videobuf_mmap_mapper(&pipe->capq, vma);

error:
	mutex_unlock(&isp->mutex);

	return ret;
}

static int atomisp_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);

	return videobuf_mmap_mapper(&pipe->outq, vma);
}

static unsigned int atomisp_poll(struct file *file,
				 struct poll_table_struct *pt)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);

	mutex_lock(&isp->mutex);
	if (pipe->capq.streaming != 1) {
		mutex_unlock(&isp->mutex);
		return POLLERR;
	}
	mutex_unlock(&isp->mutex);

	return videobuf_poll_stream(file, &pipe->capq, pt);
}

const struct v4l2_file_operations atomisp_fops = {
	.owner = THIS_MODULE,
	.open = atomisp_open,
	.release = atomisp_release,
	.mmap = atomisp_mmap,
	.ioctl = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = atomisp_compat_ioctl32,
#endif
	.poll = atomisp_poll,
};

const struct v4l2_file_operations atomisp_file_fops = {
	.owner = THIS_MODULE,
	.open = atomisp_open,
	.release = atomisp_release,
	.mmap = atomisp_file_mmap,
	.ioctl = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = atomisp_compat_ioctl32,
#endif
	.poll = atomisp_poll,
};

