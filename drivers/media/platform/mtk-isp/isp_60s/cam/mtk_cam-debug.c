// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#ifdef CONFIG_DEBUG_FS

#include "mtk_cam.h"
#include "mtk_cam-meta.h"
#include "mtk_cam-debug.h"

#define CAMSYS_DUMP_SATATE_INIT		0
#define CAMSYS_DUMP_SATATE_READY	1

/* Dump single region to the buffer */
static void mtk_cam_dump_buf_content(struct device *dev, void *buf, void *src,
				     int offset, int size, char *buf_name)
{
	void *dest;

	if (size <= 0) {
		dev_dbg(dev, "%s has no dump, size: %d", buf_name, size);
		return;
	}

	dest = buf + offset;

	dev_dbg(dev, "%s: dump:(%p --> %p), offset:%d, size: %d",
		__func__, src, dest, offset, size);
	memcpy(dest, src, size);
}

/* When calling this function, you must hold dump_buf_lock */
static void mtk_cam_debug_dump_buf_reinit(struct mtk_cam_debug_fs *debug_fs)
{
	debug_fs->dump_count = 0;
	debug_fs->dump_buf_head_idx = 0;
	debug_fs->dump_buf_tail_idx = 0;
}

/* When calling this function, you must hold dump_buf_lock */
static void *mtk_cam_debug_next_dump_buf(struct mtk_cam_debug_fs *debug_fs)
{
	struct device *dev = debug_fs->cam_dev->dev;
	void *buf;

	if (debug_fs->dump_buf_entry_num <= 0) {
		dev_dbg(dev, "%s: no buffers:%d", __func__,
			debug_fs->dump_buf_entry_num);
		return NULL;
	}

	if (debug_fs->dump_count) {
		debug_fs->dump_buf_head_idx++;

		if (debug_fs->dump_buf_head_idx == debug_fs->dump_buf_entry_num)
			debug_fs->dump_buf_head_idx = 0;

		/* overwrite the last buffer */
		if (debug_fs->dump_buf_head_idx == debug_fs->dump_buf_tail_idx)
			debug_fs->dump_buf_tail_idx++;

		if (debug_fs->dump_buf_tail_idx == debug_fs->dump_buf_entry_num)
			debug_fs->dump_buf_tail_idx = 0;
	}

	debug_fs->dump_count++;
	if (debug_fs->dump_count > debug_fs->dump_buf_entry_num)
		debug_fs->dump_count = debug_fs->dump_buf_entry_num;

	buf = debug_fs->dump_buf[debug_fs->dump_buf_head_idx];

	dev_dbg(dev, "%s: buf(%p), head(%d), tail(%d), entry_num(%d)", __func__,
		buf, debug_fs->dump_buf_head_idx, debug_fs->dump_buf_tail_idx,
		debug_fs->dump_buf_entry_num);

	return buf;
}

static void
mtk_cam_debug_dump_all_conent(struct mtk_cam_debug_fs *debug_fs, void *dump_buf,
			      struct mtk_cam_dump_param *param)
{
	struct mtk_cam_dump_header *header;
	struct device *dev = debug_fs->cam_dev->dev;

	header = (struct mtk_cam_dump_header *)dump_buf;
	strncpy(header->desc, param->desc, MTK_CAM_DEBUG_DUMP_DESC_SIZE - 1);
	header->request_fd = param->request_fd;
	header->buffer_state = param->buffer_state;
	header->timestamp = param->timestamp;
	header->sequence = param->sequence;
	header->header_size = sizeof(*header);
	header->payload_offset = header->header_size;
	dev_dbg(dev, "req:%d, ts:%lu, seq:%d, header sz:%d, payload_offset%d",
		header->request_fd, header->timestamp, header->sequence,
		header->header_size, header->payload_offset);

	/* meta file information */
	header->meta_version_major = MTK_CAM_META_VERSION_MAJOR;
	header->meta_version_minor = MTK_CAM_META_VERSION_MINOR;
	dev_dbg(dev, "major ver:%d, minor ver:%d\n",
		header->meta_version_major, header->meta_version_minor);

	/* CQ dump */
	header->cq_dump_buf_offset = header->payload_offset;
	header->cq_size = param->cq_size;
	header->cq_iova = param->cq_iova;
	header->cq_desc_size = param->cq_desc_size;

	dev_dbg(dev, "CQ offset:%d, sz:%d, iova:%d, desc_sz:%d\n",
		header->cq_dump_buf_offset, header->cq_size, header->cq_iova,
		header->cq_desc_size);

	/* meta in */
	header->meta_in_dump_buf_offset = header->cq_dump_buf_offset +
					       header->cq_size;
	header->meta_in_dump_buf_size = param->meta_in_dump_buf_size;
	header->meta_in_iova = param->meta_in_iova;

	dev_dbg(dev, "Meta-in offset:%d, sz:%d, iova:%d\n",
		header->meta_in_dump_buf_offset, header->meta_in_dump_buf_size,
		header->meta_in_iova);

	/* meta out 0 */
	header->meta_out_0_dump_buf_offset = header->meta_in_dump_buf_offset +
		header->meta_in_dump_buf_size;
	header->meta_out_0_dump_buf_size = param->meta_out_0_dump_buf_size;
	header->meta_out_0_iova = param->meta_out_0_iova;
	dev_dbg(dev, "Meta-out 0 offset:%d, sz:%d, iova:%d\n",
		header->meta_out_0_dump_buf_offset,
		header->meta_out_0_dump_buf_size, header->meta_out_0_iova);

	/* meta out 1 */
	header->meta_out_1_dump_buf_offset =
		header->meta_out_0_dump_buf_offset +
		header->meta_out_0_dump_buf_size;
	header->meta_out_1_dump_buf_size = param->meta_out_1_dump_buf_size;
	header->meta_out_1_iova = param->meta_out_1_iova;
	dev_dbg(dev, "Meta-out 1 offset:%d, sz:%d, iova:%d\n",
		header->meta_out_1_dump_buf_offset,
		header->meta_out_1_dump_buf_size, header->meta_out_1_iova);

	/* meta out 2 */
	header->meta_out_2_dump_buf_offset =
		header->meta_out_1_dump_buf_offset +
		header->meta_out_1_dump_buf_size;
	header->meta_out_2_dump_buf_size = param->meta_out_2_dump_buf_size;
	header->meta_out_2_iova = param->meta_out_2_iova;

	header->payload_size = debug_fs->dump_buf_entry_size -
			       header->header_size;

	dev_dbg(dev, "Meta-out 2 offset:%d, sz:%d, iova:%d\n",
		header->meta_out_2_dump_buf_offset,
		header->meta_out_2_dump_buf_size, header->meta_out_2_iova);

	mtk_cam_dump_buf_content(dev, dump_buf, param->cq_cpu_addr,
				 header->cq_dump_buf_offset,
				 header->cq_size, "CQ");

	mtk_cam_dump_buf_content(dev, dump_buf, param->meta_in_cpu_addr,
				 header->meta_in_dump_buf_offset,
				 header->meta_in_dump_buf_size,
				 "Meta-in-0");

	mtk_cam_dump_buf_content(dev, dump_buf, param->meta_out_1_cpu_addr,
				 header->meta_out_1_dump_buf_offset,
				 header->meta_out_1_dump_buf_size,
				 "Meta-out-1");

	mtk_cam_dump_buf_content(dev, dump_buf, param->meta_out_2_cpu_addr,
				 header->meta_out_2_dump_buf_offset,
				 header->meta_out_2_dump_buf_size,
				 "Meta-out-2");
}

/**
 * Dump to a ring buffer, the dump opertion is allowed
 * unless the user open and read the ring buffre.
 */
static int mtk_cam_debug_dump(struct mtk_cam_debug_fs *debug_fs,
			      struct mtk_cam_dump_param *param)
{
	struct device *dev = debug_fs->cam_dev->dev;
	void *dump_buf;

	mutex_lock(&debug_fs->dump_buf_lock);

	dev_dbg(dev, "%s: seq:%d, dump_state:%d, max sz:%d\n",
		__func__, param->sequence, atomic_read(&debug_fs->dump_state),
		debug_fs->dump_buf_entry_size);

	/**
	 * The ring buffer dumping is rejected once the file is opened.
	 * In open() of the ring buffer dumping file, we change the dump_state
	 * to CAMSYS_DUMP_SATATE_READY.
	 */
	if (atomic_read(&debug_fs->dump_state) != CAMSYS_DUMP_SATATE_INIT) {
		dev_dbg(dev, "%s dump failed since the buffer's read by user\n",
			__func__);
		mutex_unlock(&debug_fs->dump_buf_lock);

		return -EINVAL;
	}

	dump_buf = mtk_cam_debug_next_dump_buf(debug_fs);
	if (!dump_buf) {
		dev_dbg(dev, "%s: can't get any dump buffer. size(%d), num(%d)\n",
			__func__, debug_fs->dump_buf_entry_size, debug_fs->dump_buf_entry_num);

		mutex_unlock(&debug_fs->dump_buf_lock);

		return -EINVAL;
	}

	mtk_cam_debug_dump_all_conent(debug_fs, dump_buf, param);

	dev_dbg(dev, "%s: dbg dump(%p) is ready to read\n", __func__, dump_buf);

	mutex_unlock(&debug_fs->dump_buf_lock);

	return 0;
}

static int mtk_cam_debug_exp_dump(struct mtk_cam_debug_fs *debug_fs,
				  struct mtk_cam_dump_param *param)
{
	struct device *dev = debug_fs->cam_dev->dev;
	void *dump_buf = debug_fs->exp_dump_buf;

	if (!dump_buf)
		dev_dbg(dev, "%s: no exception dump buffer. size(%d)\n",
			__func__, debug_fs->dump_buf_entry_size);

	dev_dbg(dev, "%s: seq:%d, exp dump_state:%d, buf:%p, max sz:%d\n",
		__func__, param->sequence, atomic_read(&debug_fs->exp_dump_state),
		dump_buf, debug_fs->dump_buf_entry_size);

	mutex_lock(&debug_fs->exp_dump_buf_lock);
	if (atomic_read(&debug_fs->exp_dump_state) != CAMSYS_DUMP_SATATE_INIT) {
		dev_dbg(dev, "%s dump failed since the buffer's read by user\n",
			__func__);
		mutex_unlock(&debug_fs->exp_dump_buf_lock);

		return -EINVAL;
	}

	mtk_cam_debug_dump_all_conent(debug_fs, dump_buf, param);

	/*
	 * The dump buffer can't be written until the user open, read and close
	 * the exp dump file.
	 */
	atomic_set(&debug_fs->exp_dump_state, CAMSYS_DUMP_SATATE_READY);

	dev_dbg(dev, "%s dbg dump is ready to read\n",
		__func__);

	mutex_unlock(&debug_fs->exp_dump_buf_lock);
	wake_up(&debug_fs->exp_dump_waitq);

	return 0;
}

static int mtk_cam_debug_dump_open(struct inode *inode, struct file *file)
{
	struct device *dev;
	struct mtk_cam_device *cam_dev;
	struct mtk_cam_debug_fs *debug_fs;

	if (inode->i_private)
		file->private_data = inode->i_private;

	dev = file->private_data;
	cam_dev = (struct mtk_cam_device *)dev_get_drvdata(dev);
	debug_fs = cam_dev->debug_fs;

	mutex_lock(&debug_fs->dump_buf_lock);

	dev_dbg(dev, "%s: cnt(%d), head(%d), tail(%d), entry_sz(%d), entry_num(%d)\n",
		__func__,
		debug_fs->dump_count, debug_fs->dump_buf_head_idx,
		debug_fs->dump_buf_tail_idx, debug_fs->dump_buf_entry_size,
		debug_fs->dump_buf_entry_num);

	if (debug_fs->reader_count > 0) {
		dev_dbg(dev,
			"%s can't open since we only allow 1 reader now\n",
			__func__);
		mutex_unlock(&debug_fs->dump_buf_lock);
		return -EINVAL;
	}

	debug_fs->reader_count++;

	/**
	 * Mark the ring buffer so that the driver can't write it
	 * until the user closes the file.
	 */
	atomic_set(&debug_fs->dump_state, CAMSYS_DUMP_SATATE_READY);
	mutex_unlock(&debug_fs->dump_buf_lock);

	return 0;
}

static int mtk_cam_debug_exp_open(struct inode *inode, struct file *file)
{
	struct device *dev;
	struct mtk_cam_device *cam_dev;
	struct mtk_cam_debug_fs *debug_fs;

	if (inode->i_private)
		file->private_data = inode->i_private;

	dev = file->private_data;
	cam_dev = (struct mtk_cam_device *)dev_get_drvdata(dev);
	debug_fs = cam_dev->debug_fs;

	mutex_lock(&debug_fs->exp_dump_buf_lock);

	if (debug_fs->exp_reader_count > 0) {
		dev_dbg(dev,
			"%s can't open since we only allow 1 reader now\n",
			__func__);
		mutex_unlock(&debug_fs->dump_buf_lock);
		return -EINVAL;
	}

	debug_fs->exp_reader_count++;
	mutex_unlock(&debug_fs->exp_dump_buf_lock);

	return 0;
}

static int mtk_cam_debug_has_exp_dump(struct mtk_cam_debug_fs *debug_fs)
{
	return (atomic_read(&debug_fs->exp_dump_state) == CAMSYS_DUMP_SATATE_READY);
}

static ssize_t mtk_cam_debug_dump_read(struct file *file, char __user *user_buf,
				       size_t count, loff_t *ppos)
{
	struct device *dev = file->private_data;
	struct mtk_cam_device *cam_dev;
	struct mtk_cam_debug_fs *debug_fs;
	void *dump_buf;
	int read_buf_idx;
	int debugfs_buf_idx;
	int pos;
	int internal_pos;
	int ret;

	if (!dev) {
		pr_debug("%s: dev can't be null\n", __func__);
		return -EINVAL;
	}

	cam_dev = (struct mtk_cam_device *)dev_get_drvdata(dev);
	if (!cam_dev) {
		dev_dbg(dev, "%s: cam_dev can't be null\n", __func__);
		return -EINVAL;
	}

	debug_fs = cam_dev->debug_fs;
	if (!debug_fs) {
		dev_dbg(dev, "%s: debug_fs can't be null\n", __func__);
		return -EINVAL;
	}

	dev_dbg(dev, "%s: read buf request: %d bytes\n", __func__, count);

	mutex_lock(&debug_fs->dump_buf_lock);

	if (!debug_fs->dump_count ||
	    debug_fs->dump_count > debug_fs->dump_buf_entry_num) {
		mutex_unlock(&debug_fs->dump_buf_lock);
		return 0;
	}

	pos = *ppos;
	if (pos >= debug_fs->dump_buf_entry_size * debug_fs->dump_count) {
		mutex_unlock(&debug_fs->dump_buf_lock);
		return 0;
	}

	read_buf_idx = pos / debug_fs->dump_buf_entry_size;

	debugfs_buf_idx = read_buf_idx + debug_fs->dump_buf_tail_idx;
	if (debugfs_buf_idx >= debug_fs->dump_buf_entry_num)
		debugfs_buf_idx = debugfs_buf_idx - debug_fs->dump_buf_entry_num;

	dump_buf = debug_fs->dump_buf[debugfs_buf_idx];
	internal_pos = pos - read_buf_idx * debug_fs->dump_buf_entry_size;

	if (internal_pos < 0) {
		mutex_unlock(&debug_fs->dump_buf_lock);
		return -EINVAL;
	}

	if (count > debug_fs->dump_buf_entry_size - internal_pos)
		count = debug_fs->dump_buf_entry_size - internal_pos;

	dev_dbg(dev, "%s: copy: buf: %d, start: %d, cnt: %d, src:%p, dest:(%p)\n",
		__func__, debugfs_buf_idx, internal_pos, count,
		dump_buf + internal_pos, user_buf);

	ret = copy_to_user(user_buf, dump_buf + internal_pos, count);
	if (ret == count)
		return -EFAULT;

	count -= ret;
	*ppos = pos + count;

	mutex_unlock(&debug_fs->dump_buf_lock);

	return count;
}

static ssize_t mtk_cam_debug_exp_read(struct file *file, char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	struct device *dev = file->private_data;
	struct mtk_cam_device *cam_dev;
	struct mtk_cam_debug_fs *debug_fs;

	if (!dev)
		pr_debug("%s: dev can't be null\n", __func__);

	cam_dev = (struct mtk_cam_device *)dev_get_drvdata(dev);
	if (!cam_dev)
		dev_dbg(dev, "%s: cam_dev can't be null\n", __func__);

	debug_fs = cam_dev->debug_fs;
	if (!debug_fs)
		dev_dbg(dev, "%s: debug_fs can't be null\n", __func__);

	if (!debug_fs->exp_dump_buf)
		dev_dbg(dev, "%s: dump buf can't be null\n", __func__);

	/* Waitning for the dump recording coming */
	wait_event(debug_fs->exp_dump_waitq, mtk_cam_debug_has_exp_dump(debug_fs));

	dev_dbg(dev, "%s: read buf request: %d bytes\n", __func__, count);

	return simple_read_from_buffer(user_buf, count, ppos,
				       debug_fs->exp_dump_buf,
				       debug_fs->dump_buf_entry_size);
}

static int mtk_cam_debug_dump_release(struct inode *inode, struct file *file)
{
	struct device *dev;
	struct mtk_cam_device *cam_dev;
	struct mtk_cam_debug_fs *debug_fs;

	dev = file->private_data;
	if (!dev) {
		pr_debug("%s: dev is NULL", __func__);
		return 0;
	}

	cam_dev = (struct mtk_cam_device *)dev_get_drvdata(dev);
	if (!cam_dev) {
		dev_dbg(dev, "%s: cam_dev is NULL", __func__);
		return 0;
	}

	debug_fs = cam_dev->debug_fs;

	mutex_lock(&debug_fs->dump_buf_lock);

	/* Let the dump buffer can be written again */
	atomic_set(&debug_fs->dump_state, CAMSYS_DUMP_SATATE_INIT);
	debug_fs->reader_count--;

	dev_dbg(dev, "%s dump_state: %d\n", __func__,
		atomic_read(&debug_fs->dump_state));

	if (debug_fs->reader_count < 0) {
		dev_dbg(dev, "%s reader_count can't be %s\n", __func__,
			debug_fs->reader_count);
		debug_fs->reader_count = 0;
	}

	mutex_unlock(&debug_fs->dump_buf_lock);

	return 0;
}

static int mtk_cam_debug_exp_release(struct inode *inode, struct file *file)
{
	struct device *dev;
	struct mtk_cam_device *cam_dev;
	struct mtk_cam_debug_fs *debug_fs;

	dev = file->private_data;
	if (!dev) {
		pr_debug("%s: dev is NULL", __func__);
		return 0;
	}

	cam_dev = (struct mtk_cam_device *)dev_get_drvdata(dev);
	if (!cam_dev) {
		dev_dbg(dev, "%s: cam_dev is NULL", __func__);
		return 0;
	}

	debug_fs = cam_dev->debug_fs;

	mutex_lock(&debug_fs->exp_dump_buf_lock);

	memset(debug_fs->exp_dump_buf, 0, debug_fs->dump_buf_entry_size);

	/* Let the dump buffer can be written again */
	atomic_set(&debug_fs->exp_dump_state, CAMSYS_DUMP_SATATE_INIT);
	debug_fs->exp_reader_count--;

	dev_dbg(dev, "%s dump_state: %d\n", __func__,
		atomic_read(&debug_fs->exp_dump_state));

	if (debug_fs->exp_reader_count < 0) {
		dev_dbg(dev, "%s reader_count can't be %s\n", __func__,
			debug_fs->exp_reader_count);
		debug_fs->exp_reader_count = 0;
	}

	mutex_unlock(&debug_fs->exp_dump_buf_lock);

	return 0;
}

static const struct file_operations mtkcam_debug_dump_fops = {
	.open = mtk_cam_debug_dump_open,
	.read = mtk_cam_debug_dump_read,
	.release = mtk_cam_debug_dump_release,
};

static const struct file_operations mtkcam_debug_exp_dump_fops = {
	.open = mtk_cam_debug_exp_open,
	.read = mtk_cam_debug_exp_read,
	.release = mtk_cam_debug_exp_release,
};

static int mtk_cam_debug_realloc(struct mtk_cam_debug_fs *debug_fs,
				 int num_of_bufs)
{
	int num_of_allocated_buffers = 0;
	int i;
	struct device *dev = debug_fs->cam_dev->dev;

	mutex_lock(&debug_fs->exp_dump_buf_lock);

	/* Release the previous buffers */
	for (i = 0; i < debug_fs->dump_buf_entry_num; i++) {
		dev_dbg(dev, "%s: free dump buf(%d): %p\n", __func__, i, debug_fs->dump_buf[i]);
		kfree(debug_fs->dump_buf[i]);
		debug_fs->dump_buf[i] = NULL;
	}

	if (num_of_bufs > MTK_CAM_DEBUG_DUMP_MAX_BUF)
		debug_fs->dump_buf_entry_num = MTK_CAM_DEBUG_DUMP_MAX_BUF;
	else
		debug_fs->dump_buf_entry_num = num_of_bufs;

	for (i = 0; i < debug_fs->dump_buf_entry_num; i++) {
		void *dump_buf;

		dump_buf = kzalloc(debug_fs->dump_buf_entry_size, GFP_KERNEL);
		if (!dump_buf)
			break;

		debug_fs->dump_buf[i] = dump_buf;
		num_of_allocated_buffers++;

		dev_dbg(dev, "%s: alloc dump buf(%d): %p\n", __func__, i, debug_fs->dump_buf[i]);
	}

	debug_fs->dump_buf_entry_num = num_of_allocated_buffers;
	mtk_cam_debug_dump_buf_reinit(debug_fs);

	dev_dbg(dev, "%s: cnt(%d), head(%d), tail(%d), entry_sz(%d), entry_num(%d)\n",
		__func__,
		debug_fs->dump_count, debug_fs->dump_buf_head_idx,
		debug_fs->dump_buf_tail_idx, debug_fs->dump_buf_entry_size,
		debug_fs->dump_buf_entry_num);

	mutex_unlock(&debug_fs->exp_dump_buf_lock);

	return 0;
}

static int mtk_cam_debug_init(struct mtk_cam_debug_fs *debug_fs,
			      struct mtk_cam_device *cam_dev,
			      int content_size, int num_of_bufs)
{
	struct dentry *d;
	void *exp_dump_buf;
	int dump_mem_size;

	debug_fs->cam_dev = cam_dev;
	d = debugfs_create_file("mtk_cam_dbg_dump", 0444, NULL, cam_dev->dev,
				&mtkcam_debug_dump_fops);
	if (!d) {
		dev_dbg(cam_dev->dev, "Can't create debug fs\n");
		return -ENOMEM;
	}

	debug_fs->dump_entry = d;

	d = debugfs_create_file("mtk_cam_exp_dump", 0444, NULL, cam_dev->dev,
				&mtkcam_debug_exp_dump_fops);
	if (!d) {
		dev_dbg(cam_dev->dev, "Can't create debug fs\n");
		return -ENOMEM;
	}

	debug_fs->exp_dump_entry = d;

	dump_mem_size = content_size + sizeof(struct mtk_cam_dump_header);
	exp_dump_buf = kzalloc(dump_mem_size, GFP_KERNEL);
	if (!exp_dump_buf) {
		dev_dbg(cam_dev->dev, "Can't create debug buffer\n");
		return -ENOMEM;
	}

	debug_fs->dump_buf_entry_size = dump_mem_size;
	debug_fs->exp_dump_buf = exp_dump_buf;
	atomic_set(&debug_fs->dump_state, CAMSYS_DUMP_SATATE_INIT);
	atomic_set(&debug_fs->exp_dump_state, CAMSYS_DUMP_SATATE_INIT);
	debug_fs->reader_count = 0;
	debug_fs->exp_reader_count = 0;
	mutex_init(&debug_fs->dump_buf_lock);
	mutex_init(&debug_fs->exp_dump_buf_lock);
	init_waitqueue_head(&debug_fs->exp_dump_waitq);

	return 0;
}

static void mtk_cam_debug_deinit(struct mtk_cam_debug_fs *debug_fs)
{
	int i;

	if (debug_fs && debug_fs->exp_dump_buf) {
		kfree(debug_fs->exp_dump_buf);
		dev_dbg(debug_fs->cam_dev->dev, "Free dump buffer\n");
	}

	for (i = 0; i < debug_fs->dump_buf_entry_num; i++) {
		kfree(debug_fs->dump_buf[i]);
		debug_fs->dump_buf[i] = NULL;
	}

	debugfs_remove(debug_fs->dump_entry);
	debugfs_remove(debug_fs->exp_dump_entry);
}

static struct mtk_cam_debug_ops debug_ops = {
	.init = mtk_cam_debug_init,
	.deinit = mtk_cam_debug_deinit,
	.dump = mtk_cam_debug_dump,
	.realloc = mtk_cam_debug_realloc,
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
