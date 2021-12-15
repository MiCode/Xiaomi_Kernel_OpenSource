// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#ifdef CONFIG_DEBUG_FS

#include <linux/freezer.h>
#include <media/v4l2-event.h>
#include "mtk_cam.h"
#include "mtk_cam-raw.h"
#include "mtk_cam-regs.h"
#include "mtk_cam-debug.h"
#include "mtk_camera-v4l2-controls.h"
#include "mtk_camera-videodev2.h"
#include <soc/mediatek/smi.h>
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#endif
#define CAMSYS_DUMP_SATATE_INIT		0
#define CAMSYS_DUMP_SATATE_READY	1

#define CTRL_BLOCK_STATE_EMPTY		0
#define CTRL_BLOCK_STATE_WRITE		1
#define CTRL_BLOCK_STATE_READY		2
#define CTRL_BLOCK_STATE_READ		3

int mtk_cam_debug_init_dump_param(struct mtk_cam_ctx *ctx,
				   struct mtk_cam_dump_param *param,
				   struct mtk_cam_request_stream_data *stream_data,
				   char *desc)
{
	struct mtk_cam_device *cam = ctx->cam;
	struct mtk_cam_buffer *buf;
	struct mtk_cam_video_device *node;
	int request_fd = -1;

	memset(param, 0, sizeof(*param));
	param->stream_id = ctx->stream_id;
	param->sequence = stream_data->frame_seq_no;
	param->timestamp = stream_data->timestamp;

	if (stream_data->working_buf)
		param->cq_cpu_addr = stream_data->working_buf->buffer.va;
	else {
		dev_info(cam->dev,
			"%s:ctx(%d):req(%d):stream_data->working_buf is null\n",
			__func__, ctx->stream_id, param->sequence);
		return -EINVAL;
	}

	buf = mtk_cam_s_data_get_vbuf(stream_data, MTK_RAW_META_IN);
	if (buf) {
		request_fd = buf->vbb.request_fd;
		node = mtk_cam_vbq_to_vdev(buf->vbb.vb2_buf.vb2_queue);
		param->meta_in_cpu_addr = vb2_plane_vaddr(&buf->vbb.vb2_buf, 0);
		param->meta_in_dump_buf_size =
			node->active_fmt.fmt.meta.buffersize;
		param->meta_in_iova = buf->daddr;
		dev_dbg(cam->dev,
			"%s:ctx(%d):req(%d):MTK_RAW_META_IN(%s) found: %d\n",
			__func__, ctx->stream_id, param->sequence,
			node->desc.name, param->meta_in_dump_buf_size);
	}

	param->cq_size = stream_data->working_buf->buffer.size;
	param->cq_iova = stream_data->working_buf->buffer.iova;
	param->cq_desc_offset = stream_data->working_buf->cq_desc_offset;
	param->cq_desc_size = stream_data->working_buf->cq_desc_size;
	param->sub_cq_desc_size = stream_data->working_buf->sub_cq_desc_size;
	param->sub_cq_desc_offset = stream_data->working_buf->sub_cq_desc_offset;

	param->request_fd = request_fd;
	param->desc = desc;

	/* add mtkcam_ipi_frame_param to dump */
	param->frame_params = &stream_data->frame_params;
	param->frame_param_size = sizeof(stream_data->frame_params);
	dev_dbg(cam->dev, "%s:ctx(%d):req(%d), frame_param size(%d)\n",
		__func__, ctx->stream_id, param->sequence,
		param->frame_param_size);

	/* add mtkcam_ipi_config_param to dump */
	param->config_params = &ctx->config_params;
	param->config_param_size = sizeof(ctx->config_params);
	dev_dbg(cam->dev, "%s:ctx(%d):req(%d), cofig_param size(%d)\n",
		__func__, ctx->stream_id, param->sequence,
		param->config_param_size);

	return 0;
}

/* Dump single region to the buffer */
static void mtk_cam_dump_buf_content(struct device *dev, void *buf, void *src,
				     int offset, int size, char *buf_name)
{
	void *dest;

	if (!src || size <= 0)
		return;

	dest = buf + offset;
	dev_dbg(dev, "%s: dump:%s(%p --> %p), offset(%d), size(%d)",
		__func__, buf_name, src, dest, offset, size);
	memcpy(dest, src, size);
}

static void
mtk_cam_debug_dump_all_content(struct mtk_cam_debug_fs *debug_fs,
			       void *dump_buf,
			       struct mtk_cam_dump_param *param)
{
	struct mtk_cam_dump_header *header;
	struct device *dev = debug_fs->cam->dev;

	header = (struct mtk_cam_dump_header *)dump_buf;
	strncpy(header->desc, param->desc, MTK_CAM_DEBUG_DUMP_DESC_SIZE - 1);
	header->request_fd = param->request_fd;
	header->stream_id = param->stream_id;
	header->timestamp = param->timestamp;
	header->sequence = param->sequence;
	header->header_size = sizeof(*header);
	header->payload_offset = header->header_size;
	header->payload_size = debug_fs->buf_size -
			       header->header_size;
	dev_dbg(dev,
		"%s:ctx(%d):req_fd(%d),ts(%lu),req(%d),header_sz(%d),payload_offset(%d),payload_sz(%d)",
		__func__, header->stream_id, header->request_fd,
		header->timestamp, header->sequence, header->header_size,
		header->payload_offset, header->payload_size);

	/* meta file information */
	header->meta_version_major = mtk_cam_get_meta_version(true);
	header->meta_version_minor = mtk_cam_get_meta_version(false);

	/* CQ dump */
	header->cq_dump_buf_offset = header->payload_offset;
	header->cq_size = param->cq_size;
	header->cq_iova = param->cq_iova;
	header->cq_desc_offset = param->cq_desc_offset;
	header->cq_desc_size = param->cq_desc_size;
	header->sub_cq_desc_offset = param->sub_cq_desc_offset;
	header->sub_cq_desc_size = param->sub_cq_desc_size;

	/* meta in */
	header->meta_in_dump_buf_offset = header->cq_dump_buf_offset +
		header->cq_size;
	header->meta_in_dump_buf_size = param->meta_in_dump_buf_size;
	header->meta_in_iova = param->meta_in_iova;

	/* meta out 0 */
	header->meta_out_0_dump_buf_offset = header->meta_in_dump_buf_offset +
		header->meta_in_dump_buf_size;
	header->meta_out_0_dump_buf_size = param->meta_out_0_dump_buf_size;
	header->meta_out_0_iova = param->meta_out_0_iova;

	/* meta out 1 */
	header->meta_out_1_dump_buf_offset =
		header->meta_out_0_dump_buf_offset +
		header->meta_out_0_dump_buf_size;
	header->meta_out_1_dump_buf_size = param->meta_out_1_dump_buf_size;
	header->meta_out_1_iova = param->meta_out_1_iova;

	/* meta out 2 */
	header->meta_out_2_dump_buf_offset =
		header->meta_out_1_dump_buf_offset +
		header->meta_out_1_dump_buf_size;
	header->meta_out_2_dump_buf_size = param->meta_out_2_dump_buf_size;
	header->meta_out_2_iova = param->meta_out_2_iova;

	/* ipi frame param */
	header->frame_dump_offset =
		header->meta_out_2_dump_buf_offset +
		header->meta_out_2_dump_buf_size;
	header->frame_dump_size = param->frame_param_size;

	/* ipi config param */
	header->config_dump_offset =
		header->frame_dump_offset +
		header->frame_dump_size;
	header->config_dump_size = param->config_param_size;
	header->used_stream_num = 1;

	mtk_cam_dump_buf_content(dev, dump_buf, param->cq_cpu_addr,
				 header->cq_dump_buf_offset,
				 header->cq_size, "CQ");

	mtk_cam_dump_buf_content(dev, dump_buf, param->meta_in_cpu_addr,
				 header->meta_in_dump_buf_offset,
				 header->meta_in_dump_buf_size,
				 "Meta-in");

	mtk_cam_dump_buf_content(dev, dump_buf, param->meta_out_0_cpu_addr,
				 header->meta_out_0_dump_buf_offset,
				 header->meta_out_0_dump_buf_size,
				 "Meta-out-0");

	mtk_cam_dump_buf_content(dev, dump_buf, param->meta_out_1_cpu_addr,
				 header->meta_out_1_dump_buf_offset,
				 header->meta_out_1_dump_buf_size,
				 "Meta-out-1");

	mtk_cam_dump_buf_content(dev, dump_buf, param->meta_out_2_cpu_addr,
				 header->meta_out_2_dump_buf_offset,
				 header->meta_out_2_dump_buf_size,
				 "Meta-out-2");

	mtk_cam_dump_buf_content(dev, dump_buf, param->frame_params,
				 header->frame_dump_offset,
				 header->frame_dump_size,
				 "Ipi-frame-params");

	mtk_cam_dump_buf_content(dev, dump_buf, param->config_params,
				 header->config_dump_offset,
				 header->config_dump_size,
				 "Ipi-config-params");
}

static struct mtk_cam_dump_ctrl_block*
mtk_cam_dump_ctrl_block_next(struct mtk_cam_debug_fs *debug_fs,
			     struct mtk_cam_dump_buf_ctrl *ctrl)
{
	struct device *dev = debug_fs->cam->dev;
	struct mtk_cam_dump_ctrl_block *ctrl_block;
	int head, tail;

	head = ctrl->head;
	tail = ctrl->tail;

	if (ctrl->num <= 0) {
		dev_info(dev, "%s:pipe(%d):no buffer allocated:%d\n", __func__,
			 ctrl->pipe_id, ctrl->num);
		return NULL;
	}

	if (ctrl->count) {
		head++;
		if (head == ctrl->num)
			head = 0;
		/* User is still reading the buffer*/
		if (atomic_read(&ctrl->blocks[head].state) == CTRL_BLOCK_STATE_READ)
			return NULL;
		/* overwrite the last buffer */
		if (head == tail)
			tail++;
		if (tail == ctrl->num)
			tail = 0;
	}

	ctrl->tail = tail;
	ctrl->head = head;
	ctrl->count++;
	if (ctrl->count > ctrl->num)
		ctrl->count = ctrl->num;

	ctrl_block = &ctrl->blocks[ctrl->head];
	dev_dbg(dev,
		"%s:pipe(%d):ctrl_block(%p),buf(%p),head(%d),tail(%d),cnt(%d),num(%d)\n",
		__func__, ctrl->pipe_id, ctrl_block, ctrl_block->buf, ctrl->head,
		ctrl->tail, ctrl->count, ctrl->num);

	return ctrl_block;
}

static int mtk_cam_debug_dump(struct mtk_cam_debug_fs *debug_fs,
			      struct mtk_cam_dump_param *param)
{
	struct device *dev = debug_fs->cam->dev;
	struct mtk_cam_dump_buf_ctrl *ctrl = &debug_fs->ctrl[param->stream_id];
	struct mtk_cam_dump_ctrl_block *ctrl_block;

	mutex_lock(&ctrl->ctrl_lock);
	ctrl_block = mtk_cam_dump_ctrl_block_next(debug_fs, ctrl);
	if (!ctrl_block || !ctrl_block->buf) {
		dev_info(dev, "%s:pipe(%d):req(%d):no free buffer, drop\n",
			 __func__, param->stream_id, param->sequence);
		mutex_unlock(&ctrl->ctrl_lock);

		return -EINVAL;
	}
	mutex_unlock(&ctrl->ctrl_lock);

	atomic_set(&ctrl_block->state, CTRL_BLOCK_STATE_WRITE);
	mtk_cam_debug_dump_all_content(debug_fs, ctrl_block->buf, param);
	atomic_set(&ctrl_block->state, CTRL_BLOCK_STATE_READY);

	dev_dbg(dev, "%s:pipe(%d):req(%d):dump(%p) is ready to read, sz(%d)\n",
		__func__, param->stream_id, param->sequence, ctrl_block->buf,
		debug_fs->buf_size);

	return 0;
}

static int mtk_cam_debug_exp_dump(struct mtk_cam_debug_fs *debug_fs,
				  struct mtk_cam_dump_param *param)
{
	struct device *dev = debug_fs->cam->dev;
	void *dump_buf = debug_fs->exp_dump_buf;

	if (!dump_buf) {
		dev_info(dev, "%s:pipe(%d):req(%d):no dump buffer. sz(%d)\n",
			 __func__, param->stream_id, param->sequence,
			 debug_fs->buf_size);
		return -EINVAL;
	}

	mutex_lock(&debug_fs->exp_dump_buf_lock);
	if (atomic_read(&debug_fs->exp_dump_state) != CAMSYS_DUMP_SATATE_INIT) {
		dev_info(dev, "%s:pipe(%d):req(%d):dump can't be written, state(%d), buf(%p), sz(%d)\n",
			 __func__, param->stream_id, param->sequence,
			 atomic_read(&debug_fs->exp_dump_state), dump_buf,
			 debug_fs->buf_size);
		mutex_unlock(&debug_fs->exp_dump_buf_lock);

		return -EINVAL;
	}

	memset(dump_buf, 0, debug_fs->buf_size);
	mtk_cam_debug_dump_all_content(debug_fs, dump_buf, param);
	atomic_set(&debug_fs->exp_dump_state, CAMSYS_DUMP_SATATE_READY);
	mutex_unlock(&debug_fs->exp_dump_buf_lock);

	dev_dbg(dev, "%s:pipe(%d):req(%d):dump is ready to read\n",
		__func__, param->stream_id, param->sequence);

	return 0;
}

static int mtk_cam_debug_find_dump_ctrl(struct mtk_cam_dump_buf_ctrl *ctrl,
					unsigned long seq)
{
	int i = ctrl->head;
	struct mtk_cam_dump_ctrl_block *ctrl_block;
	struct mtk_cam_dump_header *header;

	if (!ctrl->count)
		return -EINVAL;

	while (1) {
		ctrl_block = &ctrl->blocks[i];
		header = (struct mtk_cam_dump_header *)ctrl_block->buf;
		if (header->sequence == seq)
			return i;

		if (i == ctrl->tail)
			break;

		if (--i < 0)
			i = ctrl->num - 1;
	}

	return -EINVAL;
}

static int mtk_cam_debug_dump_ctrl_set(struct mtk_cam_dump_buf_ctrl *ctrl,
				       unsigned long seq, bool start)
{
	struct mtk_cam_dump_ctrl_block *ctrl_block;
	int ctrl_block_idx;

	dev_info(ctrl->debug_fs->cam->dev,
		 "%s:pipe_id(%d):to read req(%ld), start/end(%d)\n", __func__,
		 ctrl->pipe_id, seq, start);
	mutex_lock(&ctrl->ctrl_lock);
	ctrl_block_idx = mtk_cam_debug_find_dump_ctrl(ctrl, seq);
	if (ctrl_block_idx < 0) {
		dev_info(ctrl->debug_fs->cam->dev,
			 "%s:pipe_id(%d): to read req(%ld) dump, start/end(%d), not found\n",
			 __func__, ctrl->pipe_id, seq, start);
		goto FAIL_RELEASE_LOCK;
	}
	ctrl_block = &ctrl->blocks[ctrl_block_idx];
	if (start) {
		if (atomic_read(&ctrl_block->state) != CTRL_BLOCK_STATE_READY) {
			dev_info(ctrl->debug_fs->cam->dev,
				 "%s:pipe_id(%d):to get req(%ld) dump failed, state(%d)\n",
				__func__, ctrl->pipe_id, seq,
				atomic_read(&ctrl_block->state));
			goto FAIL_RELEASE_LOCK;
		}
		atomic_set(&ctrl_block->state, CTRL_BLOCK_STATE_READ);
		ctrl->cur_read = ctrl_block_idx;
	} else {
		if (atomic_read(&ctrl_block->state) != CTRL_BLOCK_STATE_READ) {
			dev_info(ctrl->debug_fs->cam->dev,
				 "%s:pipe_id(%d):to release req(%ld) dump failed, state(%d)\n",
				 __func__, ctrl->pipe_id, seq,
				 atomic_read(&ctrl_block->state));
			goto FAIL_RELEASE_LOCK;
		}
		ctrl->cur_read = -1;
		atomic_set(&ctrl_block->state, CTRL_BLOCK_STATE_READY);
	}
	mutex_unlock(&ctrl->ctrl_lock);
	return 0;

FAIL_RELEASE_LOCK:
	ctrl->cur_read = -1;
	mutex_unlock(&ctrl->ctrl_lock);
	return -EINVAL;
}

static void mtk_cam_dump_ctrl_init(struct mtk_cam_debug_fs *debug_fs,
				   struct mtk_cam_dump_buf_ctrl *ctrl, int num)
{
	ctrl->num = num;
	ctrl->head = 0;
	ctrl->tail = 0;
	ctrl->count = 0;
	ctrl->cur_read = -1;
}

static void mtk_cam_dump_ctrl_reinit(struct mtk_cam_dump_buf_ctrl *ctrl)
{
	int i;

	mutex_lock(&ctrl->ctrl_lock);

	ctrl->head = 0;
	ctrl->tail = 0;
	ctrl->count = 0;
	ctrl->cur_read = -1;
	for (i = 0; i < ctrl->num; i++)
		atomic_set(&ctrl->blocks[i].state, CTRL_BLOCK_STATE_EMPTY);

	mutex_unlock(&ctrl->ctrl_lock);
}

static int mtk_cam_dump_buf_realloc(struct mtk_cam_dump_buf_ctrl *ctrl,
				    int num_of_bufs)
{
	int num_of_alloc = 0;
	int i;
	struct mtk_cam_dump_ctrl_block *ctrl_block;
	struct mtk_cam_debug_fs *debug_fs = ctrl->debug_fs;
	struct device *dev = debug_fs->cam->dev;
	void *dump_buf;

	mutex_lock(&ctrl->ctrl_lock);
	/* Release the previous buffers */
	for (i = 0; i < ctrl->num; i++) {
		dev_dbg(dev, "%s:pipe(%d):free dump buf(%d):%p\n", __func__,
			ctrl->pipe_id, i, ctrl->blocks[i].buf);
		vfree(ctrl->blocks[i].buf);
		ctrl->blocks[i].buf = NULL;
	}

	if (num_of_bufs > MTK_CAM_DEBUG_DUMP_MAX_BUF)
		ctrl->num = MTK_CAM_DEBUG_DUMP_MAX_BUF;
	else
		ctrl->num = num_of_bufs;

	for (i = 0; i < ctrl->num; i++) {
		dump_buf = vzalloc(debug_fs->buf_size);
		if (!dump_buf)
			break;

		ctrl_block = &ctrl->blocks[i];
		atomic_set(&ctrl_block->state, CTRL_BLOCK_STATE_EMPTY);
		ctrl_block->buf = dump_buf;
		num_of_alloc++;
		dev_dbg(dev, "%s:pipe(%d):alloc dump buf(%d):%p\n", __func__,
			ctrl->pipe_id, i, ctrl->blocks[i].buf);
	}

	mtk_cam_dump_ctrl_init(debug_fs, ctrl, num_of_alloc);
	dev_info(dev, "%s:pipe(%d):cnt(%d), head(%d), tail(%d), entry_sz(%d), entry_num(%d)\n",
		 __func__, ctrl->pipe_id, ctrl->count, ctrl->head,
		 ctrl->tail, debug_fs->buf_size, ctrl->num);
	mutex_unlock(&ctrl->ctrl_lock);

	return 0;
}

static int dbg_ctrl_open(struct inode *inode, struct file *file)
{
	struct mtk_cam_debug_fs *debug_fs;
	struct mtk_cam_dump_buf_ctrl *ctrl;

	if (inode->i_private)
		file->private_data = inode->i_private;

	ctrl = file->private_data;
	debug_fs = ctrl->debug_fs;

	dev_dbg(debug_fs->cam->dev,
		"%s:pipe(%d):cnt(%d), head(%d), tail(%d), entry_sz(%d), entry_num(%d)\n",
		__func__, ctrl->pipe_id, ctrl->count, ctrl->head, ctrl->tail,
		debug_fs->buf_size, ctrl->num);

	return 0;
}

static ssize_t dbg_ctrl_write(struct file *file, const char __user *data,
			      size_t count, loff_t *ppos)
{
	struct mtk_cam_debug_fs *debug_fs;
	struct mtk_cam_dump_buf_ctrl *ctrl;
	char tmp[16];
	char *parse_str = tmp;
	char *cmd_str;
	char *param_str_0;
	char *param_str_1;
	unsigned long seq = 0;
	int ret = -EINVAL;

	ctrl = file->private_data;
	debug_fs = ctrl->debug_fs;
	if (count >= 15) {
		dev_info(ctrl->debug_fs->cam->dev,
			 "%s:pipe(%d):Invalid cmd sz(%d)\n", __func__,
			 ctrl->pipe_id, count);
		goto FAIL;
	}

	memset(tmp, 0, 16);
	if (copy_from_user(tmp, data, count)) {
		dev_info(ctrl->debug_fs->cam->dev,
			 "%s:pipe(%d):copy_from_user failed, data(%p), sz(%d)\n",
			 __func__, ctrl->pipe_id, data, count);
		ret = -EFAULT;
		goto FAIL;
	}
	dev_dbg(ctrl->debug_fs->cam->dev, "%s:pipe(%d):received cmd(%s)\n",
		__func__, ctrl->pipe_id, tmp);
	cmd_str = strsep(&parse_str, ":");
	param_str_0 = strsep(&parse_str, ":");

	if (cmd_str[0] == 'r') {
		param_str_1 = strsep(&parse_str, ":");
		if (kstrtoul(param_str_1, 10, &seq)) {
			ret = -EFAULT;
			dev_dbg(ctrl->debug_fs->cam->dev, "kstrtoul failed:%s\n",
				param_str_1);
			goto FAIL;
		}

		if (param_str_0[0] == 's') {
			ret = mtk_cam_debug_dump_ctrl_set(ctrl, seq, true);
			if (ret < 0)
				goto FAIL;
		} else if (param_str_0[0] == 'e') {
			ret = mtk_cam_debug_dump_ctrl_set(ctrl, seq, false);
			if (ret < 0)
				goto FAIL;
		} else {
			ret = -EFAULT;
			goto FAIL;
		}
	} else if (cmd_str[0] == 'd') {
		if (param_str_0[0] == 's') {
			mtk_cam_dump_buf_realloc(ctrl, MTK_CAM_DEBUG_DUMP_MAX_BUF);
			debug_fs->force_dump = MTK_CAM_REQ_DUMP_FORCE;
		} else if (param_str_0[0] == 'r') {
			mtk_cam_dump_ctrl_reinit(ctrl);
			debug_fs->force_dump = MTK_CAM_REQ_DUMP_FORCE;
		} else if (param_str_0[0] == 'e') {
			debug_fs->force_dump = 0;
			mtk_cam_dump_buf_realloc(ctrl, 0);
		} else {
			ret = -EFAULT;
			goto FAIL;
		}
	}

	return count;
FAIL:
	return ret;
}

static int dbg_data_open(struct inode *inode, struct file *file)
{
	struct mtk_cam_debug_fs *debug_fs;
	struct mtk_cam_dump_buf_ctrl *ctrl;

	if (inode->i_private)
		file->private_data = inode->i_private;

	ctrl = file->private_data;
	debug_fs = ctrl->debug_fs;

	dev_dbg(debug_fs->cam->dev,
		"%s:pipe(%d):cnt(%d), head(%d), tail(%d), entry_sz(%d), entry_num(%d)\n",
		__func__, ctrl->pipe_id, ctrl->count, ctrl->head, ctrl->tail,
		debug_fs->buf_size, ctrl->num);

	return 0;
}

static ssize_t dbg_data_read(struct file *file, char __user *user_buf,
			     size_t count, loff_t *ppos)
{
	struct mtk_cam_dump_buf_ctrl *ctrl = file->private_data;
	struct mtk_cam_debug_fs *debug_fs = ctrl->debug_fs;
	struct mtk_cam_dump_ctrl_block *ctrl_block;

	int cur_read = 0;
	size_t read_count;

	cur_read = ctrl->cur_read;
	if (cur_read < 0) {
		dev_dbg(debug_fs->cam->dev,
			"%s:pipe(%d):user requested seq not found! cur_read(%d)\n",
			__func__, ctrl->pipe_id, cur_read);
		return 0;
	}

	ctrl_block = &ctrl->blocks[cur_read];
	if (atomic_read(&ctrl_block->state) != CTRL_BLOCK_STATE_READ)
		return 0;

	dev_dbg(debug_fs->cam->dev,
		"%s:pipe(%d):read buf request: %d bytes\n", __func__,
		ctrl->pipe_id, count);
	read_count = simple_read_from_buffer(user_buf, count, ppos,
					     ctrl_block->buf,
					     debug_fs->buf_size);

	return read_count;
}

static int mtk_cam_debug_has_exp_dump(struct mtk_cam_debug_fs *debug_fs)
{
	return (atomic_read(&debug_fs->exp_dump_state) == CAMSYS_DUMP_SATATE_READY);
}

static int exp_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;

	return 0;
}

static ssize_t exp_read(struct file *file, char __user *user_buf,
			size_t count, loff_t *ppos)
{
	struct device *dev = file->private_data;
	struct mtk_cam_device *cam;
	struct mtk_cam_debug_fs *debug_fs;
	size_t read_count;

	if (!dev)
		pr_debug("%s: dev can't be null\n", __func__);

	cam = (struct mtk_cam_device *)dev_get_drvdata(dev);
	if (!cam)
		dev_dbg(dev, "%s: cam can't be null\n", __func__);

	debug_fs = cam->debug_fs;
	if (!debug_fs)
		dev_dbg(dev, "%s: debug_fs can't be null\n", __func__);

	if (!debug_fs->exp_dump_buf)
		dev_dbg(dev, "%s: dump buf can't be null\n", __func__);

	/* If no dump, return 0 byte read directly */
	if (!mtk_cam_debug_has_exp_dump(debug_fs))
		return 0;

	dev_dbg(dev, "%s: read buf request: %d bytes\n", __func__, count);
	mutex_lock(&debug_fs->exp_dump_buf_lock);

	read_count = simple_read_from_buffer(user_buf, count, ppos,
					     debug_fs->exp_dump_buf,
					     debug_fs->buf_size);

	mutex_unlock(&debug_fs->exp_dump_buf_lock);

	return read_count;
}

static int exp_release(struct inode *inode, struct file *file)
{
	struct device *dev;
	struct mtk_cam_device *cam;
	struct mtk_cam_debug_fs *debug_fs;

	dev = file->private_data;
	if (!dev) {
		pr_debug("%s: dev is NULL", __func__);
		return 0;
	}

	cam = (struct mtk_cam_device *)dev_get_drvdata(dev);
	if (!cam) {
		dev_dbg(dev, "%s: cam is NULL", __func__);
		return 0;
	}

	debug_fs = cam->debug_fs;
	dev_dbg(dev, "%s dump_state: %d\n", __func__,
		atomic_read(&debug_fs->exp_dump_state));

	return 0;
}

static const struct file_operations dbg_ctrl_fops = {
	.open = dbg_ctrl_open,
	.write = dbg_ctrl_write,
};

static const struct file_operations dbg_data_fops = {
	.open = dbg_data_open,
	.read = dbg_data_read,
	.llseek = default_llseek,
};

static const struct file_operations exp_fops = {
	.open = exp_open,
	.read = exp_read,
	.release = exp_release,
};

static int mtk_cam_exp_reinit(struct mtk_cam_debug_fs *debug_fs)
{
	/* Let the exception dump buffer can be written again */
	mutex_lock(&debug_fs->exp_dump_buf_lock);
	atomic_set(&debug_fs->exp_dump_state, CAMSYS_DUMP_SATATE_INIT);
	mutex_unlock(&debug_fs->exp_dump_buf_lock);
	return 0;
}

static int mtk_cam_debug_reinit(struct mtk_cam_debug_fs *debug_fs,
				int streaming_id)
{
	struct mtk_cam_dump_buf_ctrl *ctrl;

	if (!debug_fs->force_dump)
		return 0;

	ctrl = &debug_fs->ctrl[streaming_id];
	mtk_cam_dump_ctrl_reinit(ctrl);

	return 0;
}

static int mtk_cam_debug_init(struct mtk_cam_debug_fs *debug_fs,
			      struct mtk_cam_device *cam, int content_size)
{
	struct mtk_cam_dump_buf_ctrl *ctrl;
	void *exp_dump_buf;
	int dump_mem_size, i;

	dump_mem_size = content_size + sizeof(struct mtk_cam_dump_header);
	debug_fs->cam = cam;
	debug_fs->buf_size = dump_mem_size;

	/* Exception dump initialization*/
	exp_dump_buf = kzalloc(dump_mem_size, GFP_KERNEL);
	if (!exp_dump_buf)
		return -ENOMEM;

	debug_fs->exp_dump_buf = exp_dump_buf;
	atomic_set(&debug_fs->exp_dump_state, CAMSYS_DUMP_SATATE_INIT);
	mutex_init(&debug_fs->exp_dump_buf_lock);

	debug_fs->exp_dump_entry = debugfs_create_file("mtk_cam_exp_dump",
						       0444, NULL, cam->dev,
						       &exp_fops);
	if (!debug_fs->exp_dump_entry) {
		dev_info(cam->dev, "Can't create debug fs\n");
		return -ENOMEM;
	}

	debug_fs->dbg_entry = debugfs_create_dir("mtk_cam_dbg", NULL);
	for (i = 0; i < cam->max_stream_num; i++) {
		char name[4];

		ctrl = &debug_fs->ctrl[i];
		ctrl->pipe_id = i;
		ctrl->debug_fs = debug_fs;
		atomic_set(&ctrl->dump_state, CAMSYS_DUMP_SATATE_INIT);
		mutex_init(&ctrl->ctrl_lock);

		snprintf(name, 4, "%d", i);
		ctrl->dir_entry = debugfs_create_dir(name, debug_fs->dbg_entry);
		if (!ctrl->dir_entry) {
			dev_info(cam->dev,
				 "Can't create dir for pipe:%d\n", i);
			return -ENOMEM;
		}

		ctrl->ctrl_entry = debugfs_create_file("ctrl", 0664,
						       ctrl->dir_entry, ctrl,
						       &dbg_ctrl_fops);
		if (!ctrl->ctrl_entry) {
			dev_info(cam->dev,
				 "Can't create ctrl file for pipe:%d\n", i);
			return -ENOMEM;
		}

		ctrl->data_entry = debugfs_create_file("data", 0444,
						       ctrl->dir_entry, ctrl,
						       &dbg_data_fops);
		if (!ctrl->data_entry) {
			dev_info(cam->dev,
				 "Can't create data file for pipe:%d\n", i);
			return -ENOMEM;
		}
	}

	return 0;
}

static void mtk_cam_debug_deinit(struct mtk_cam_debug_fs *debug_fs)
{
	struct mtk_cam_dump_buf_ctrl *ctrl;
	int i;

	if (!debug_fs)
		return;

	for (i = 0; i < debug_fs->cam->max_stream_num; i++) {
		ctrl = &debug_fs->ctrl[i];
		debugfs_remove(ctrl->ctrl_entry);
		debugfs_remove(ctrl->data_entry);
		debugfs_remove(ctrl->dir_entry);
	}

	debugfs_remove(debug_fs->dbg_entry);
	kfree(debug_fs->exp_dump_buf);
	dev_dbg(debug_fs->cam->dev, "Free exception dump buffer\n");
	debugfs_remove(debug_fs->exp_dump_entry);
}

static void mtk_cam_debug_dump_work(struct work_struct *work)
{
	struct mtk_cam_req_dbg_work *dbg_work = to_mtk_cam_req_dbg_work(work);
	struct mtk_cam_request_stream_data *s_data = dbg_work->s_data;
	struct mtk_cam_ctx *ctx = mtk_cam_s_data_get_ctx(s_data);
	struct mtk_cam_dump_param dump_param;
	struct v4l2_event event = {
		.type = V4L2_EVENT_REQUEST_DUMPED,
	};
	int ret = 0;

	ret = mtk_cam_debug_init_dump_param(ctx, &dump_param, s_data,
				      dbg_work->desc);
	if (ret < 0) {
		atomic_set(&dbg_work->state, MTK_CAM_REQ_DBGWORK_S_FINISHED);
		return;
	}

	ctx->cam->debug_fs->ops->dump(ctx->cam->debug_fs, &dump_param);
	atomic_set(&dbg_work->state, MTK_CAM_REQ_DBGWORK_S_FINISHED);

	event.u.frame_sync.frame_sequence = dump_param.sequence;
	v4l2_event_queue(ctx->pipe->subdev.devnode, &event);
}

static void mtk_cam_exception_work(struct work_struct *work)
{
	struct mtk_cam_req_dbg_work *dbg_work = to_mtk_cam_req_dbg_work(work);
	struct mtk_cam_request_stream_data *s_data = dbg_work->s_data;
	struct mtk_cam_request *req = mtk_cam_s_data_get_req(s_data);
	struct mtk_cam_ctx *ctx = mtk_cam_s_data_get_ctx(s_data);
	struct mtk_cam_dump_param dump_param;
	char warn_desc[48];
	char title_desc[48];
	int ret = 0;

	if (s_data == NULL)
		return;

	if (atomic_read(&s_data->dbg_exception_work.state) == MTK_CAM_REQ_DBGWORK_S_CANCEL) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):used_raw(0x%x):exception dump canceled\n",
			 __func__, ctx->stream_id, ctx->used_raw_dev);
		return;
	}

	ret = mtk_cam_debug_init_dump_param(ctx, &dump_param, s_data,
				      dbg_work->desc);

	if (ret < 0) {
		atomic_set(&dbg_work->state, MTK_CAM_REQ_DBGWORK_S_FINISHED);
		return;
	}

	ctx->cam->debug_fs->ops->exp_dump(ctx->cam->debug_fs, &dump_param);
	snprintf(title_desc, 48, "Camsys:%s", dbg_work->desc);
	snprintf(warn_desc, 48, "%s:ctx(%d):req(%d):%s",
		 req->req.debug_str, ctx->stream_id, s_data->frame_seq_no,
		 dbg_work->desc);
	dev_info(ctx->cam->dev, "%s:camsys dump, %s\n",
		 __func__, warn_desc);

	if (dbg_work->smi_dump)
		mtk_smi_dbg_hang_detect("camsys");

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT, title_desc,
			       warn_desc);
#else
	WARN_ON(1);
#endif

	atomic_set(&dbg_work->state, MTK_CAM_REQ_DBGWORK_S_FINISHED);
}

static bool
mtk_cam_exceptoin_is_job_done(struct mtk_cam_request_stream_data *s_data,
			      bool *streamoff)
{
	struct mtk_cam_ctx *ctx = mtk_cam_s_data_get_ctx(s_data);
	int state;

	spin_lock(&ctx->streaming_lock);
	if (!ctx->streaming) {
		spin_unlock(&ctx->streaming_lock);
		*streamoff = true;
		return true;
	}
	spin_unlock(&ctx->streaming_lock);

	state = atomic_read(&s_data->dbg_exception_work.state);
	if (state == MTK_CAM_REQ_DBGWORK_S_FINISHED ||
	    state == MTK_CAM_REQ_DBGWORK_S_INIT ||
	    state == MTK_CAM_REQ_DBGWORK_S_CANCEL) {
		*streamoff = false;
		return true;
	}

	return false;
}

static void mtk_cam_exceptoin_detect_work(struct work_struct *work)
{
	struct mtk_cam_req_dbg_work *dbg_work = to_mtk_cam_req_dbg_work(work);
	struct mtk_cam_request_stream_data *s_data = dbg_work->s_data;
	struct mtk_cam_request *req = mtk_cam_s_data_get_req(s_data);
	struct mtk_cam_ctx *ctx = mtk_cam_s_data_get_ctx(s_data);
	int ret;
	bool streamoff;

	ret = wait_event_freezable_timeout(ctx->cam->debug_exception_waitq,
					   mtk_cam_exceptoin_is_job_done(s_data, &streamoff),
					   msecs_to_jiffies(1000 / 30 * 8));
	if (ret) {
		if (!streamoff) {
			dev_info(ctx->cam->dev,
				"%s:ctx(%d):%s:req(%d):skip dump since job done\n",
				__func__, ctx->stream_id, req->req.debug_str,
				s_data->frame_seq_no);
			atomic_set(&dbg_work->state, MTK_CAM_REQ_DBGWORK_S_FINISHED);
		} else {
			/**
			 * Workaround for abnormal request release after
			 * streaming off now, we can't touch the request
			 * any more.
			 */
			dev_info(ctx->cam->dev,
				 "%s: skip dump work for stream off ctx:%d\n",
				 __func__, ctx->stream_id);
		}
		return;
	}

	if (atomic_read(&s_data->dbg_exception_work.state) == MTK_CAM_REQ_DBGWORK_S_CANCEL) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):used_raw(0x%x):exception dump canceled\n",
			 __func__, ctx->stream_id, ctx->used_raw_dev);
		return;
	}

	if (ctx->seninf) {
		ret = mtk_cam_seninf_dump(ctx->seninf);
		dev_info(ctx->cam->dev,
			"%s:ctx(%d):used_raw(0x%x):mtk_cam_seninf_dump() ret=%d\n",
			__func__, ctx->stream_id, ctx->used_raw_dev, ret);
	} else {
		dev_info(ctx->cam->dev, "%s: cannot find ctx->seninf\n",
			 __func__);
	}

	mtk_cam_exception_work(work);
}

int mtk_cam_req_dump(struct mtk_cam_request_stream_data *s_data,
		     unsigned int dump_flag, char *desc, bool smi_dump)
{
	struct mtk_cam_ctx *ctx = mtk_cam_s_data_get_ctx(s_data);
	struct mtk_cam_req_dbg_work *dbg_work;
	void (*work_func)(struct work_struct *work);
	struct workqueue_struct *wq;

	if (!ctx->cam->debug_fs)
		return false;

	switch (dump_flag) {
	case MTK_CAM_REQ_DUMP_FORCE:
		if (!ctx->cam->debug_fs->force_dump ||
		    !ctx->cam->debug_fs->ctrl[ctx->stream_id].num)
			return false;

		dbg_work = &s_data->dbg_work;
		work_func = mtk_cam_debug_dump_work;
		wq = ctx->cam->debug_wq;
		break;
	case MTK_CAM_REQ_DUMP_DEQUEUE_FAILED:
		dbg_work =  &s_data->dbg_exception_work;
		work_func = mtk_cam_exception_work;
		wq = ctx->cam->debug_exception_wq;
		break;
	case MTK_CAM_REQ_DUMP_CHK_DEQUEUE_FAILED:
		dbg_work =  &s_data->dbg_exception_work;
		work_func = mtk_cam_exceptoin_detect_work;
		wq = ctx->cam->debug_exception_wq;
		break;
	default:
		dev_dbg(ctx->cam->dev,
			"%s:seq(%d) dump skipped, unknown dump type (%d)\n",
			__func__, s_data->frame_seq_no,
			dump_flag);
		return false;
	}

	if (atomic_read(&dbg_work->state) != MTK_CAM_REQ_DBGWORK_S_INIT)
		return false;

	INIT_WORK(&dbg_work->work, work_func);
	dbg_work->s_data = s_data;
	dbg_work->dump_flags = dump_flag;
	dbg_work->smi_dump = smi_dump;
	atomic_set(&dbg_work->state, MTK_CAM_REQ_DBGWORK_S_PREPARED);
	snprintf(dbg_work->desc, MTK_CAM_DEBUG_DUMP_DESC_SIZE - 1, desc);
	if (!queue_work(wq, &dbg_work->work)) {
		dev_dbg(ctx->cam->dev,
			"%s: seq(%d) failed, debug work is already in queue\n",
			__func__, s_data->frame_seq_no);
		return false;
	}

	return true;
}

void
mtk_cam_debug_detect_dequeue_failed(struct mtk_cam_request_stream_data *s_data,
				    const unsigned int frame_no_update_limit,
				    struct mtk_camsys_irq_info *irq_info,
				    struct mtk_raw_device *raw_dev)
{
#define NO_P1_DONE_DEBUG_START 3

	struct mtk_cam_ctx *ctx;
	struct mtk_cam_request *req;

	if (irq_info->fbc_cnt == 0)
		return;
	/**
	 * If the requset is already dequeued (for example, the p1 done and sof
	 * interrupt come almost together), skip the check.
	 */
	if (!s_data)
		return;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	req = mtk_cam_s_data_get_req(s_data);

	if (ctx->composed_frame_seq_no < ctx->dequeued_frame_seq_no)
		return;

	if (s_data->state.estate == E_STATE_CQ ||
	    s_data->state.estate == E_STATE_OUTER ||
	    s_data->state.estate == E_STATE_INNER ||
	    s_data->state.estate == E_STATE_OUTER_HW_DELAY ||
	    s_data->state.estate == E_STATE_INNER_HW_DELAY) {
		s_data->no_frame_done_cnt++;
		if (s_data->no_frame_done_cnt > 1)
			dev_info(ctx->cam->dev,
			 "%s:SOF[ctx:%d-#%d] no p1 done for %d sofs, FBC_CNT %d dump req(%d) state(%d) ts(%lu)\n",
			 req->req.debug_str, ctx->stream_id,
			 ctx->dequeued_frame_seq_no,
			 s_data->no_frame_done_cnt, irq_info->fbc_cnt,
			 s_data->frame_seq_no, s_data->state.estate, irq_info->ts_ns / 1000);
	}
	if (s_data->no_frame_done_cnt >= NO_P1_DONE_DEBUG_START) {
		dev_info(raw_dev->dev,
			 "INT_EN %x\n",
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_INT_EN));

		dev_info(raw_dev->dev,
			 "REQ RAW/2/3 DMA/2:%08x/%08x/%08x/%08x/%08x\n",
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD_REQ_STAT),
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD2_REQ_STAT),
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD3_REQ_STAT),
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD5_REQ_STAT),
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD6_REQ_STAT));
		dev_info(raw_dev->dev,
			 "RDY RAW/2/3 DMA/2:%08x/%08x/%08x/%08x/%08x\n",
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD_RDY_STAT),
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD2_RDY_STAT),
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD3_RDY_STAT),
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD5_RDY_STAT),
			 readl_relaxed(raw_dev->base + REG_CTL_RAW_MOD6_RDY_STAT));
		dev_info(raw_dev->dev,
			 "REQ YUV/2/3/4 WDMA:%08x/%08x/%08x/%08x/%08x\n",
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD_REQ_STAT),
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD2_REQ_STAT),
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD3_REQ_STAT),
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD4_REQ_STAT),
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD5_REQ_STAT));
		dev_info(raw_dev->dev,
			 "RDY YUV/2/3/4 WDMA:%08x/%08x/%08x/%08x/%08x\n",
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD_RDY_STAT),
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD2_RDY_STAT),
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD3_RDY_STAT),
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD4_RDY_STAT),
			 readl_relaxed(raw_dev->yuv_base + REG_CTL_RAW_MOD5_RDY_STAT));
	}

	if (s_data->no_frame_done_cnt > frame_no_update_limit &&
		s_data->dbg_work.dump_flags == 0) {
		dev_info(ctx->cam->dev,
			 "%s:SOF[ctx:%d-#%d] no p1 done for %d sofs, FBC_CNT %d dump req(%d) state(%d) ts(%lu)\n",
			 req->req.debug_str, ctx->stream_id,
			 ctx->dequeued_frame_seq_no,
			 s_data->no_frame_done_cnt, irq_info->fbc_cnt,
			 s_data->frame_seq_no,
			 s_data->state.estate, irq_info->ts_ns / 1000);
		mtk_cam_req_dump(s_data, MTK_CAM_REQ_DUMP_DEQUEUE_FAILED,
				 "No P1 done", false);
	} else if (s_data->no_frame_done_cnt > frame_no_update_limit &&
		s_data->dbg_work.dump_flags != 0)
		dev_info(ctx->cam->dev,
			 "%s:SOF[ctx:%d-#%d] no p1 done for %d sofs, s_data->dbg_work.dump_flags(%d) state(%d) ts(%lu)\n",
			 req->req.debug_str, ctx->stream_id,
			 ctx->dequeued_frame_seq_no,
			 s_data->no_frame_done_cnt, s_data->dbg_work.dump_flags,
			 s_data->state.estate, irq_info->ts_ns / 1000);
}

void mtk_cam_debug_wakeup(struct wait_queue_head *wq_head)
{
	wake_up(wq_head);
}

static void mtk_cam_req_seninf_dump_work(struct work_struct *work)
{
	struct mtk_cam_seninf_dump_work *seninf_dump_work;
	struct v4l2_subdev *seninf;

	seninf_dump_work = to_mtk_cam_seninf_dump_work(work);
	seninf = seninf_dump_work->seninf;
	if (!seninf)
		pr_info("%s: filaed, seninf can't be NULL\n", __func__);
	else
		mtk_cam_seninf_dump(seninf);

	kfree(seninf_dump_work);
}

void
mtk_cam_debug_seninf_dump(struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_ctx *ctx;
	struct mtk_cam_seninf_dump_work *dump_work;

	ctx = mtk_cam_s_data_get_ctx(s_data);
	if (!ctx) {
		pr_info("%s: failed, ctx can't be NULL\n", __func__);
		return;
	}

	spin_lock(&ctx->streaming_lock);
	if (!ctx->streaming) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):s_data(%d) drop dump due to stream off\n",
			 __func__, ctx->stream_id, s_data->frame_seq_no);
		spin_unlock(&ctx->streaming_lock);
		return;
	}
	spin_unlock(&ctx->streaming_lock);

	if (atomic_read(&s_data->seninf_dump_state) != MTK_CAM_REQ_DBGWORK_S_INIT) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):s_data(%d) drop duplicated dump\n",
			 __func__, ctx->stream_id, s_data->frame_seq_no);

		return;
	}

	dump_work = kmalloc(sizeof(*dump_work), GFP_ATOMIC);
	if (!dump_work) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):s_data(%d) can't trigger seninf, work alloc failed\n",
			 __func__, ctx->stream_id, s_data->frame_seq_no);

		return;
	}

	dump_work->seninf = ctx->seninf;
	INIT_WORK(&dump_work->work, mtk_cam_req_seninf_dump_work);
	if (!queue_work(ctx->frame_done_wq, &dump_work->work))
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):s_data(%d) work was already on a queue\n",
			 __func__, ctx->stream_id, s_data->frame_seq_no);
	else
		atomic_set(&s_data->seninf_dump_state, MTK_CAM_REQ_DBGWORK_S_FINISHED);

}

void mtk_cam_req_dump_work_init(struct mtk_cam_request_stream_data *s_data)
{
	atomic_set(&s_data->seninf_dump_state, MTK_CAM_REQ_DBGWORK_S_INIT);
}

void mtk_cam_req_dbg_works_clean(struct mtk_cam_request_stream_data *s_data)
{
	struct mtk_cam_ctx *ctx = mtk_cam_s_data_get_ctx(s_data);
	char *dbg_str = mtk_cam_s_data_get_dbg_str(s_data);
	int state;
	u64 start, cost;

	/* clean seninf dump work */
	atomic_set(&s_data->seninf_dump_state, MTK_CAM_REQ_DBGWORK_S_FINISHED);

	/* clean execption dump work */
	state = atomic_read(&s_data->dbg_exception_work.state);
	if (state != MTK_CAM_REQ_DBGWORK_S_INIT &&
	    state != MTK_CAM_REQ_DBGWORK_S_FINISHED) {
		atomic_set(&s_data->dbg_exception_work.state, MTK_CAM_REQ_DBGWORK_S_CANCEL);
		mtk_cam_debug_wakeup(&ctx->cam->debug_exception_waitq);
		start = ktime_get_boottime_ns();
		cancel_work_sync(&s_data->dbg_exception_work.work);
		cost = ktime_get_boottime_ns() - start;

		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):%s:seq(%d): cancel dbg_exception_work(%d), wait: %llu ns\n",
			 __func__, ctx->stream_id, dbg_str,
			 s_data->frame_seq_no, state, cost);
		atomic_set(&s_data->dbg_exception_work.state, MTK_CAM_REQ_DBGWORK_S_FINISHED);
	} else {
		mtk_cam_debug_wakeup(&ctx->cam->debug_exception_waitq);
		atomic_set(&s_data->dbg_exception_work.state, MTK_CAM_REQ_DBGWORK_S_FINISHED);
	}

	/* clean debug dump work */
	state = atomic_read(&s_data->dbg_work.state);
	if (state != MTK_CAM_REQ_DBGWORK_S_INIT &&
	    state != MTK_CAM_REQ_DBGWORK_S_FINISHED) {
		start = ktime_get_boottime_ns();
		cancel_work_sync(&s_data->dbg_work.work);
		cost = ktime_get_boottime_ns() - start;
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):%s:seq(%d): cancel dbg_work(%d), wait: %llu ns\n",
			  __func__, ctx->stream_id, dbg_str,
			 s_data->frame_seq_no, state, cost);
		atomic_set(&s_data->dbg_work.state, MTK_CAM_REQ_DBGWORK_S_FINISHED);
	}

}

static struct mtk_cam_debug_ops debug_ops = {
	.init = mtk_cam_debug_init,
	.reinit = mtk_cam_debug_reinit,
	.deinit = mtk_cam_debug_deinit,
	.dump = mtk_cam_debug_dump,
	.exp_reinit = mtk_cam_exp_reinit,
	.exp_dump = mtk_cam_debug_exp_dump,
};

static struct mtk_cam_debug_fs debug_fs = {
	.ops = &debug_ops,
};

struct mtk_cam_debug_fs *mtk_cam_get_debugfs(void)
{
	return &debug_fs;
}

#endif /* CONFIG_DEBUG_FS */
